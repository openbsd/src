/*	$OpenBSD: rthread.c,v 1.25 2005/12/31 08:51:20 otto Exp $ */
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

#include <sys/param.h>
#include <sys/mman.h>
#include <sys/wait.h>

#include <machine/spinlock.h>

#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <err.h>
#include <errno.h>

#include <pthread.h>

#include "rthread.h"

static int concurrency_level;	/* not used */

int _threads_ready;
struct listhead _thread_list = LIST_HEAD_INITIALIZER(_thread_list);
_spinlock_lock_t _thread_lock = _SPINLOCK_UNLOCKED;
struct pthread _initial_thread;

int rfork_thread(int, void *, void (*)(void *), void *);

/*
 * internal support functions
 */
void
_spinlock(_spinlock_lock_t *lock)
{

	while (_atomic_lock(lock))
		pthread_yield();
}

void
_spinunlock(_spinlock_lock_t *lock)
{

	*lock = _SPINLOCK_UNLOCKED;
}

static pthread_t
_rthread_findself(void)
{
	pthread_t me;
	pid_t tid = getthrid();

	LIST_FOREACH(me, &_thread_list, threads) 
		if (me->tid == tid)
			break;

	return (me);
}


static void
_rthread_start(void *v)
{
	pthread_t thread = v;
	void *retval;

	/* ensure parent returns from rfork, sets up tid */
	_spinlock(&_thread_lock);
	_spinunlock(&_thread_lock);
	retval = thread->fn(thread->arg);
	pthread_exit(retval);
}

static int
_rthread_init(void)
{
	pthread_t thread = &_initial_thread;
	extern int __isthreaded;

	printf("rthread init\n");

	thread->tid = getthrid();
	thread->donesem.lock = _SPINLOCK_UNLOCKED;
	thread->flags |= THREAD_CANCEL_ENABLE|THREAD_CANCEL_DEFERRED;
	strlcpy(thread->name, "Main process", sizeof(thread->name));
	LIST_INSERT_HEAD(&_thread_list, thread, threads);
	_threads_ready = 1;
	__isthreaded = 1;

	return (0);
}

static struct stack *
_rthread_alloc_stack(size_t len, void *base)
{
	struct stack *stack;

	stack = malloc(sizeof(*stack));
	if (!stack)
		return (NULL);
	if (base) {
		stack->base = base;
	} else {
		stack->base = mmap(NULL, len, PROT_READ | PROT_WRITE,
		    MAP_ANON, -1, 0);
		if (stack->base == MAP_FAILED) {
			free(stack);
			return (NULL);
		}
	}
#ifdef MACHINE_STACK_GROWS_UP
	stack->sp = (void *)(((size_t)stack->base + 64) & ~63);
#else
	stack->sp = (void *)(((size_t)stack->base + len - 16) & ~15);
#endif
	stack->len = len;
	return (stack);
}

static void
_rthread_free_stack(struct stack *stack)
{
	munmap(stack->base, stack->len);
	free(stack);
}

/*
 * real pthread functions
 */
pthread_t
pthread_self(void)
{
	pthread_t thread;

	if (!_threads_ready)
		if (_rthread_init())
			return (NULL);

	_spinlock(&_thread_lock);
	thread = _rthread_findself();
	_spinunlock(&_thread_lock);

	return (thread);
}

void
pthread_exit(void *retval)
{
	struct rthread_cleanup_fn *clfn;
	pthread_t thread = pthread_self();

	thread->retval = retval;
	thread->flags |= THREAD_DONE;
	
	_sem_post(&thread->donesem);
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
#if 0
	if (thread->flags & THREAD_DETACHED)
		free(thread);
#endif
	threxit(0);
	for(;;);
}

int
pthread_join(pthread_t thread, void **retval)
{

	_sem_wait(&thread->donesem, 0, 0);
	if (retval)
		*retval = thread->retval;

	return (0);
}

int
pthread_detach(pthread_t thread)
{
	_spinlock(&_thread_lock);
#if 0
	if (thread->flags & THREAD_DONE)
		free(thread);
	else
#endif
		thread->flags |= THREAD_DETACHED;
	_spinunlock(&_thread_lock);
	return (0);
}

int
pthread_create(pthread_t *threadp, const pthread_attr_t *attr,
    void *(*start_routine)(void *), void *arg)
{
	pthread_t thread;
	pid_t tid;
	int rc = 0;

	if (!_threads_ready)
		if ((rc = _rthread_init()))
		    return (rc);

	thread = malloc(sizeof(*thread));
	if (!thread)
		return (errno);
	memset(thread, 0, sizeof(*thread));
	thread->donesem.lock = _SPINLOCK_UNLOCKED;
	thread->fn = start_routine;
	thread->arg = arg;
	if (attr)
		thread->attr = *(*attr);

	_spinlock(&_thread_lock);

	thread->stack = _rthread_alloc_stack(64 * 1024, NULL);
	if (!thread->stack) {
		rc = errno;
		goto fail1;
	}
	LIST_INSERT_HEAD(&_thread_list, thread, threads);

	tid = rfork_thread(RFPROC | RFTHREAD | RFMEM | RFNOWAIT,
	    thread->stack->sp, _rthread_start, thread);
	if (tid == -1) {
		rc = errno;
		goto fail2;
	}
	/* new thread will appear _rthread_start */
	thread->tid = tid;
	thread->flags |= THREAD_CANCEL_ENABLE|THREAD_CANCEL_DEFERRED;
	*threadp = thread;

	/*
	 * Since _rthread_start() aquires the thread lock and due to the way
	 * signal delivery is implemented, this is not a race.
	 */
	if (thread->attr.create_suspended)
		kill(thread->tid, SIGSTOP);

	_spinunlock(&_thread_lock);

	return (0);

fail2:
	_rthread_free_stack(thread->stack);
	LIST_REMOVE(thread, threads);
fail1:
	_spinunlock(&_thread_lock);
	free(thread);

	return (rc);
}

int
pthread_kill(pthread_t thread, int sig)
{
	return (kill(thread->tid, sig));
}

int
pthread_equal(pthread_t t1, pthread_t t2)
{
	return (t1 == t2);
}

int
pthread_cancel(pthread_t thread)
{

	thread->flags |= THREAD_CANCELLED;
	return (0);
}

void
pthread_testcancel(void)
{
	if ((pthread_self()->flags & (THREAD_CANCELLED|THREAD_CANCEL_ENABLE)) ==
	    (THREAD_CANCELLED|THREAD_CANCEL_ENABLE))
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
		self->flags |= THREAD_CANCEL_ENABLE;
		pthread_testcancel();
	} else if (state == PTHREAD_CANCEL_DISABLE) {
		self->flags &= ~THREAD_CANCEL_ENABLE;
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
		self->flags |= THREAD_CANCEL_DEFERRED;
		pthread_testcancel();
	} else if (type == PTHREAD_CANCEL_ASYNCHRONOUS) {
		self->flags &= ~THREAD_CANCEL_DEFERRED;
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

	clfn = malloc(sizeof(*clfn));
	if (!clfn)
		return;
	memset(clfn, 0, sizeof(*clfn));
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

/*
 * the malloc lock
 */
static _spinlock_lock_t malloc_lock = _SPINLOCK_UNLOCKED;

void
_thread_malloc_lock()
{
	_spinlock(&malloc_lock);
}

void
_thread_malloc_unlock()
{
	_spinunlock(&malloc_lock);
}

void
_thread_malloc_init()
{
}
