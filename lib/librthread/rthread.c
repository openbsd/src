/*	$OpenBSD: rthread.c,v 1.81 2015/04/29 06:01:37 guenther Exp $ */
/*
 * Copyright (c) 2004,2005 Ted Unangst <tedu@openbsd.org>
 * All Rights Reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/*
 * The heart of rthreads.  Basic functions like creating and joining
 * threads.
 */

#include <sys/types.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <sys/msg.h>
#if defined(__ELF__)
#include <sys/exec_elf.h>
#pragma weak _DYNAMIC
#endif

#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <poll.h>

#include <pthread.h>

#include "thread_private.h"	/* in libc/include */
#include "rthread.h"
#include "tcb.h"

static int concurrency_level;	/* not used */

struct _spinlock _SPINLOCK_UNLOCKED_ASSIGN = _SPINLOCK_UNLOCKED;

int _threads_ready;
size_t _thread_pagesize;
struct listhead _thread_list = LIST_HEAD_INITIALIZER(_thread_list);
struct _spinlock _thread_lock = _SPINLOCK_UNLOCKED;
static struct pthread_queue _thread_gc_list
    = TAILQ_HEAD_INITIALIZER(_thread_gc_list);
static struct _spinlock _thread_gc_lock = _SPINLOCK_UNLOCKED;
static struct pthread _initial_thread;
static struct thread_control_block _initial_thread_tcb;

struct pthread_attr _rthread_attr_default = {
	.stack_addr			= NULL,
	.stack_size			= RTHREAD_STACK_SIZE_DEF,
/*	.guard_size		set in _rthread_init */
	.detach_state			= PTHREAD_CREATE_JOINABLE,
	.contention_scope		= PTHREAD_SCOPE_SYSTEM,
	.sched_policy			= SCHED_OTHER,
	.sched_param = { .sched_priority = 0 },
	.sched_inherit			= PTHREAD_INHERIT_SCHED,
};

/*
 * internal support functions
 */
void
_spinlock(volatile struct _spinlock *lock)
{
	while (_atomic_lock(&lock->ticket))
		sched_yield();
}

int
_spinlocktry(volatile struct _spinlock *lock)
{
	return 0 == _atomic_lock(&lock->ticket);
}

void
_spinunlock(volatile struct _spinlock *lock)
{
	lock->ticket = _ATOMIC_LOCK_UNLOCKED;
}

/*
 * This sets up the thread base for the initial thread so that it
 * references the errno location provided by libc.  For other threads
 * this is handled by __tfork_thread()
 */
void _rthread_initlib(void) __attribute__((constructor));
void
_rthread_initlib(void)
{
	static int tcb_set;
	struct thread_control_block *tcb;

	if (__predict_false(tcb_set == 0) && __get_tcb() == NULL) {
		tcb_set = 1;

		/* use libc's errno for the main thread */
		tcb = &_initial_thread_tcb;
		TCB_INIT(tcb, &_initial_thread, ___errno());
		TCB_SET(tcb);
	}
}

/*
 * This is invoked by ___start() in crt0.  Eventually, when ld.so handles
 * TCB setup for dynamic executables, this will only be called to handle
 * the TCB setup for static executables and may migrate to libc.  The
 * envp argument is so that it can (someday) use that to find the Auxinfo
 * array and thus the ELF phdr and the PT_TLS info.
 */
void __init_tcb(char **_envp);
void
__init_tcb(__unused char **envp)
{
	_rthread_initlib();
}

int *
__errno(void)
{
	return (TCB_ERRNOPTR());
}

static void
_rthread_start(void *v)
{
	pthread_t thread = v;
	void *retval;

	retval = thread->fn(thread->arg);
	pthread_exit(retval);
}

/* ARGSUSED0 */
static void
sigthr_handler(__unused int sig)
{
	pthread_t self = pthread_self();

	/*
	 * Do nothing unless
	 * 1) pthread_cancel() has been called on this thread,
	 * 2) cancelation is enabled for it, and
	 * 3) we're not already in cancelation processing
	 */
	if ((self->flags & (THREAD_CANCELED|THREAD_CANCEL_ENABLE|THREAD_DYING))
	    != (THREAD_CANCELED|THREAD_CANCEL_ENABLE))
		return;

	/*
	 * If delaying cancels inside complex ops (pthread_cond_wait,
	 * pthread_join, etc), just mark that this has happened to
	 * prevent a race with going to sleep
	 */
	if (self->flags & THREAD_CANCEL_DELAY) {
		self->delayed_cancel = 1;
		return;
	}

	/*
	 * otherwise, if in a cancel point or async cancels are
	 * enabled, then exit
	 */
	if (self->cancel_point || (self->flags & THREAD_CANCEL_DEFERRED) == 0)
		pthread_exit(PTHREAD_CANCELED);
}

int
_rthread_init(void)
{
	pthread_t thread = &_initial_thread;
	struct sigaction sa;

	thread->tid = getthrid();
	thread->donesem.lock = _SPINLOCK_UNLOCKED_ASSIGN;
	thread->flags |= THREAD_CANCEL_ENABLE | THREAD_CANCEL_DEFERRED |
	    THREAD_ORIGINAL | THREAD_INITIAL_STACK;
	thread->flags_lock = _SPINLOCK_UNLOCKED_ASSIGN;
	strlcpy(thread->name, "Main process", sizeof(thread->name));
	LIST_INSERT_HEAD(&_thread_list, thread, threads);
	_rthread_debug_init();

	_thread_pagesize = (size_t)sysconf(_SC_PAGESIZE);
	_rthread_attr_default.guard_size = _thread_pagesize;
	thread->attr = _rthread_attr_default;

	_rthread_initlib();

	_threads_ready = 1;

	_rthread_debug(1, "rthread init\n");

#if defined(__ELF__) && !defined(__vax__)
	if (_DYNAMIC) {
		/*
		 * To avoid recursion problems in ld.so, we need to trigger the
		 * functions once to fully bind them before registering them
		 * for use.
		 */
		_rthread_dl_lock(0);
		_rthread_dl_lock(1);
		_rthread_bind_lock(0);
		_rthread_bind_lock(1);
		sched_yield();
		dlctl(NULL, DL_SETTHREADLCK, _rthread_dl_lock);
		dlctl(NULL, DL_SETBINDLCK, _rthread_bind_lock);
	}
#endif

	/*
	 * Set the handler on the signal used for cancelation and
	 * suspension, and make sure it's unblocked
	 */
	memset(&sa, 0, sizeof(sa));
	sigemptyset(&sa.sa_mask);
	sa.sa_handler = sigthr_handler;
	_thread_sys_sigaction(SIGTHR, &sa, NULL);
	sigaddset(&sa.sa_mask, SIGTHR);
	sigprocmask(SIG_UNBLOCK, &sa.sa_mask, NULL);

	return (0);
}

static void
_rthread_free(pthread_t thread)
{
	/* _initial_thread is static, so don't free it */
	if (thread != &_initial_thread) {
		/*
		 * thread->tid is written to by __threxit in the thread
		 * itself, so it's not safe to touch it here
		 */
		_spinlock(&_thread_gc_lock);
		TAILQ_INSERT_TAIL(&_thread_gc_list, thread, waiting);
		_spinunlock(&_thread_gc_lock);
	}
}

void
_rthread_setflag(pthread_t thread, int flag)
{
	_spinlock(&thread->flags_lock);
	thread->flags |= flag;
	_spinunlock(&thread->flags_lock);
}

void
_rthread_clearflag(pthread_t thread, int flag)
{
	_spinlock(&thread->flags_lock);
	thread->flags &= ~flag;
	_spinunlock(&thread->flags_lock);
}

/*
 * real pthread functions
 */
pthread_t
pthread_self(void)
{
	if (!_threads_ready)
		if (_rthread_init())
			return (NULL);

	return (TCB_THREAD());
}

static void
_rthread_reaper(void)
{
	pthread_t thread;

restart:
	_spinlock(&_thread_gc_lock);
	TAILQ_FOREACH(thread, &_thread_gc_list, waiting) {
		if (thread->tid != 0)
			continue;
		TAILQ_REMOVE(&_thread_gc_list, thread, waiting);
		_spinunlock(&_thread_gc_lock);
		_rthread_debug(3, "rthread reaping %p stack %p\n",
		    (void *)thread, (void *)thread->stack);
		_rthread_free_stack(thread->stack);
		_rtld_free_tls(thread->arg,
		    sizeof(struct thread_control_block), sizeof(void *));
		free(thread);
		goto restart;
	}
	_spinunlock(&_thread_gc_lock);
}

void
pthread_exit(void *retval)
{
	struct rthread_cleanup_fn *clfn;
	pthread_t thread = pthread_self();

	if (thread->flags & THREAD_DYING) {
		/*
		 * Called pthread_exit() from destructor or cancelation
		 * handler: blow up.  XXX write something to stderr?
		 */
		_exit(42);
	}

	_rthread_setflag(thread, THREAD_DYING);

	thread->retval = retval;

	for (clfn = thread->cleanup_fns; clfn; ) {
		struct rthread_cleanup_fn *oclfn = clfn;
		clfn = clfn->next;
		oclfn->fn(oclfn->arg);
		free(oclfn);
	}
	_rthread_tls_destructors(thread);
	_spinlock(&_thread_lock);
	LIST_REMOVE(thread, threads);
	_spinunlock(&_thread_lock);

#ifdef TCB_GET
	thread->arg = TCB_GET();
#else
	thread->arg = __get_tcb();
#endif
	_spinlock(&thread->flags_lock);
	if (thread->flags & THREAD_DETACHED) {
		_spinunlock(&thread->flags_lock);
		_rthread_free(thread);
	} else {
		thread->flags |= THREAD_DONE;
		_spinunlock(&thread->flags_lock);
		_sem_post(&thread->donesem);
	}

	__threxit(&thread->tid);
	for(;;);
}

int
pthread_join(pthread_t thread, void **retval)
{
	int e;
	pthread_t self = pthread_self();

	e = 0;
	_enter_delayed_cancel(self);
	if (thread == NULL)
		e = EINVAL;
	else if (thread == self)
		e = EDEADLK;
	else if (thread->flags & THREAD_DETACHED)
		e = EINVAL;
	else if ((e = _sem_wait(&thread->donesem, 0, NULL,
	    &self->delayed_cancel)) == 0) {
		if (retval)
			*retval = thread->retval;

		/*
		 * We should be the last having a ref to this thread,
		 * but someone stupid or evil might haved detached it;
		 * in that case the thread will clean up itself
		 */
		if ((thread->flags & THREAD_DETACHED) == 0)
			_rthread_free(thread);
	}

	_leave_delayed_cancel(self, e);
	_rthread_reaper();
	return (e);
}

int
pthread_detach(pthread_t thread)
{
	int rc = 0;

	_spinlock(&thread->flags_lock);
	if (thread->flags & THREAD_DETACHED) {
		rc = EINVAL;
		_spinunlock(&thread->flags_lock);
	} else if (thread->flags & THREAD_DONE) {
		_spinunlock(&thread->flags_lock);
		_rthread_free(thread);
	} else {
		thread->flags |= THREAD_DETACHED;
		_spinunlock(&thread->flags_lock);
	}
	_rthread_reaper();
	return (rc);
}

int
pthread_create(pthread_t *threadp, const pthread_attr_t *attr,
    void *(*start_routine)(void *), void *arg)
{
	extern int __isthreaded;
	struct thread_control_block *tcb;
	pthread_t thread;
	struct __tfork param;
	int rc = 0;

	if (!_threads_ready)
		if ((rc = _rthread_init()))
		    return (rc);

	_rthread_reaper();

	thread = calloc(1, sizeof(*thread));
	if (!thread)
		return (errno);
	thread->donesem.lock = _SPINLOCK_UNLOCKED_ASSIGN;
	thread->flags_lock = _SPINLOCK_UNLOCKED_ASSIGN;
	thread->fn = start_routine;
	thread->arg = arg;
	thread->tid = -1;

	thread->attr = attr != NULL ? *(*attr) : _rthread_attr_default;
	if (thread->attr.sched_inherit == PTHREAD_INHERIT_SCHED) {
		pthread_t self = pthread_self();

		thread->attr.sched_policy = self->attr.sched_policy;
		thread->attr.sched_param = self->attr.sched_param;
	}
	if (thread->attr.detach_state == PTHREAD_CREATE_DETACHED)
		thread->flags |= THREAD_DETACHED;
	thread->flags |= THREAD_CANCEL_ENABLE|THREAD_CANCEL_DEFERRED;

	thread->stack = _rthread_alloc_stack(thread);
	if (!thread->stack) {
		rc = errno;
		goto fail1;
	}

	tcb = _rtld_allocate_tls(NULL, sizeof(*tcb), sizeof(void *));
	if (tcb == NULL) {
		rc = errno;
		goto fail2;
	}
	TCB_INIT(tcb, thread, &thread->myerrno);

	param.tf_tcb = tcb;
	param.tf_tid = &thread->tid;
	param.tf_stack = thread->stack->sp;

	_spinlock(&_thread_lock);
	LIST_INSERT_HEAD(&_thread_list, thread, threads);
	_spinunlock(&_thread_lock);

	/* we're going to be multi-threaded real soon now */
	__isthreaded = 1;
	rc = __tfork_thread(&param, sizeof(param), _rthread_start, thread);
	if (rc != -1) {
		/* success */
		*threadp = thread;
		return (0);
	}
		
	rc = errno;

	_spinlock(&_thread_lock);
	LIST_REMOVE(thread, threads);
	_spinunlock(&_thread_lock);
	_rtld_free_tls(tcb, sizeof(*tcb), sizeof(void *));
fail2:
	_rthread_free_stack(thread->stack);
fail1:
	_rthread_free(thread);

	return (rc);
}

int
pthread_kill(pthread_t thread, int sig)
{
	pid_t tid;
	int ret;

	/* killing myself?  do it without locking */
	if (thread == TCB_THREAD())
		return (kill(thread->tid, sig) == 0 ? 0 : errno);

	/* block the other thread from exiting */
	_spinlock(&thread->flags_lock);
	if (thread->flags & THREAD_DYING)
		ret = (thread->flags & THREAD_DETACHED) ? ESRCH : 0;
	else {
		tid = thread->tid;
		if (tid == 0) {
			/* should be impossible without DYING being set */
			ret = ESRCH;
		} else
			ret = kill(tid, sig) == 0 ? 0 : errno;
	}
	_spinunlock(&thread->flags_lock);
	return (ret);
}

int
pthread_equal(pthread_t t1, pthread_t t2)
{
	return (t1 == t2);
}

int
pthread_cancel(pthread_t thread)
{
	pid_t tid;

	_spinlock(&thread->flags_lock);
	tid = thread->tid;
	if ((thread->flags & (THREAD_DYING | THREAD_CANCELED)) == 0 &&
	    tid != 0) {
		thread->flags |= THREAD_CANCELED;

		if (thread->flags & THREAD_CANCEL_ENABLE) {

			/* canceling myself?  release the lock first */
			if (thread == TCB_THREAD()) {
				_spinunlock(&thread->flags_lock);
				kill(tid, SIGTHR);
				return (0);
			}

			kill(tid, SIGTHR);
		}
	}
	_spinunlock(&thread->flags_lock);
	return (0);
}

void
pthread_testcancel(void)
{
	if ((pthread_self()->flags & (THREAD_CANCELED|THREAD_CANCEL_ENABLE)) ==
	    (THREAD_CANCELED|THREAD_CANCEL_ENABLE))
		pthread_exit(PTHREAD_CANCELED);

}

int
pthread_setcancelstate(int state, int *oldstatep)
{
	pthread_t self = pthread_self();
	int oldstate;

	oldstate = self->flags & THREAD_CANCEL_ENABLE ?
	    PTHREAD_CANCEL_ENABLE : PTHREAD_CANCEL_DISABLE;
	if (state == PTHREAD_CANCEL_ENABLE) {
		_rthread_setflag(self, THREAD_CANCEL_ENABLE);
	} else if (state == PTHREAD_CANCEL_DISABLE) {
		_rthread_clearflag(self, THREAD_CANCEL_ENABLE);
	} else {
		return (EINVAL);
	}
	if (oldstatep)
		*oldstatep = oldstate;

	return (0);
}

int
pthread_setcanceltype(int type, int *oldtypep)
{
	pthread_t self = pthread_self();
	int oldtype;

	oldtype = self->flags & THREAD_CANCEL_DEFERRED ?
	    PTHREAD_CANCEL_DEFERRED : PTHREAD_CANCEL_ASYNCHRONOUS;
	if (type == PTHREAD_CANCEL_DEFERRED) {
		_rthread_setflag(self, THREAD_CANCEL_DEFERRED);
	} else if (type == PTHREAD_CANCEL_ASYNCHRONOUS) {
		_rthread_clearflag(self, THREAD_CANCEL_DEFERRED);
	} else {
		return (EINVAL);
	}
	if (oldtypep)
		*oldtypep = oldtype;

	return (0);
}

void
pthread_cleanup_push(void (*fn)(void *), void *arg)
{
	struct rthread_cleanup_fn *clfn;
	pthread_t self = pthread_self();

	clfn = calloc(1, sizeof(*clfn));
	if (!clfn)
		return;
	clfn->fn = fn;
	clfn->arg = arg;
	clfn->next = self->cleanup_fns;
	self->cleanup_fns = clfn;
}

void
pthread_cleanup_pop(int execute)
{
	struct rthread_cleanup_fn *clfn;
	pthread_t self = pthread_self();

	clfn = self->cleanup_fns;
	if (clfn) {
		self->cleanup_fns = clfn->next;
		if (execute)
			clfn->fn(clfn->arg);
		free(clfn);
	}
}

int
pthread_getconcurrency(void)
{
	return (concurrency_level);
}

int
pthread_setconcurrency(int new_level)
{
	if (new_level < 0)
		return (EINVAL);
	concurrency_level = new_level;
	return (0);
}

/*
 * compat debug stuff
 */
void
_thread_dump_info(void)
{
	pthread_t thread;

	_spinlock(&_thread_lock);
	LIST_FOREACH(thread, &_thread_list, threads)
		printf("thread %d flags %d name %s\n",
		    thread->tid, thread->flags, thread->name);
	_spinunlock(&_thread_lock);
}

#if defined(__ELF__)
/*
 * _rthread_dl_lock() provides the locking for dlopen(), dlclose(), and
 * the function called via atexit() to invoke all destructors.  The latter
 * two call shared-object destructors, which may need to call dlclose(),
 * so this lock needs to permit recursive locking.
 * The specific code here was extracted from _rthread_mutex_lock() and
 * pthread_mutex_unlock() and simplified to use the static variables.
 */
void
_rthread_dl_lock(int what)
{
	static struct _spinlock lock = _SPINLOCK_UNLOCKED;
	static pthread_t owner = NULL;
	static struct pthread_queue lockers = TAILQ_HEAD_INITIALIZER(lockers);
	static int count = 0;

	if (what == 0)
	{
		pthread_t self = pthread_self();

		/* lock, possibly recursive */
		_spinlock(&lock);
		if (owner == NULL) {
			owner = self;
		} else if (owner != self) {
			TAILQ_INSERT_TAIL(&lockers, self, waiting);
			while (owner != self) {
				__thrsleep(self, 0 | _USING_TICKETS, NULL,
				    &lock.ticket, NULL);
				_spinlock(&lock);
			}
		}
		count++;
		_spinunlock(&lock);
	}
	else
	{
		/* unlock, possibly recursive */
		if (--count == 0) {
			pthread_t next;

			_spinlock(&lock);
			owner = next = TAILQ_FIRST(&lockers);
			if (next != NULL)
				TAILQ_REMOVE(&lockers, next, waiting);
			_spinunlock(&lock);
			if (next != NULL)
				__thrwakeup(next, 1);
		}
	}
}

void
_rthread_bind_lock(int what)
{
	static struct _spinlock lock = _SPINLOCK_UNLOCKED;

	if (what == 0)
		_spinlock(&lock);
	else
		_spinunlock(&lock);
}
#endif

#ifdef __ELF__
#define CERROR_SYMBOL __cerror
#else
#define CERROR_SYMBOL _cerror
#endif

/*
 * XXX: Bogus type signature, but we only need to be able to emit a
 * reference to it below.
 */
extern void CERROR_SYMBOL(void);

/*
 * All weak references used within libc that are redefined in libpthread
 * MUST be in this table.   This is necessary to force the proper version to
 * be used when linking -static.
 */
static void *__libc_overrides[] __used = {
	&CERROR_SYMBOL,
	&__errno,
	&_thread_arc4_lock,
	&_thread_arc4_unlock,
	&_thread_atexit_lock,
	&_thread_atexit_unlock,
	&_thread_atfork_lock,
	&_thread_atfork_unlock,
	&_thread_malloc_lock,
	&_thread_malloc_unlock,
	&_thread_mutex_destroy,
	&_thread_mutex_lock,
	&_thread_mutex_unlock,
	&_thread_tag_lock,
	&_thread_tag_storage,
	&_thread_tag_unlock,
	&accept,
	&close,
	&closefrom,
	&connect,
	&fcntl,
	&flockfile,
	&fork,
	&fsync,
	&ftrylockfile,
	&funlockfile,
	&msgrcv,
	&msgsnd,
	&msync,
	&nanosleep,
	&open,
	&openat,
	&poll,
	&pread,
	&preadv,
	&pwrite,
	&pwritev,
	&read,
	&readv,
	&recvfrom,
	&recvmsg,
	&select,
	&sendmsg,
	&sendto,
	&sigaction,
	&sigprocmask,
	&sigsuspend,
	&vfork,
	&wait4,
	&write,
	&writev,
};
