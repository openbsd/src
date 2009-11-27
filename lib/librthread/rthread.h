/*	$OpenBSD: rthread.h,v 1.24 2009/11/27 19:43:55 guenther Exp $ */
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
 * Private data structures that back up the typedefs in pthread.h.
 * Since only the thread library cares about their size or arrangement,
 * it should be possible to switch libraries without relinking.
 *
 * Do not reorder _spinlock_lock_t and sem_t variables in the structs.
 * This is due to alignment requirements of certain arches like hppa.
 * The current requirement is 16 bytes.
 */

#include <sys/queue.h>
#include <semaphore.h>

#ifdef __LP64__
#define RTHREAD_STACK_SIZE_DEF (512 * 1024)
#else
#define RTHREAD_STACK_SIZE_DEF (256 * 1024)
#endif

struct stack {
	void *sp;
	void *base;
	void *guard;
	size_t guardsize;
	size_t len;
};

struct sem {
	_spinlock_lock_t lock;
	volatile int waitcount;
	volatile int value;
	int pad;
};

TAILQ_HEAD(pthread_queue, pthread);

struct pthread_mutex {
	struct sem sem;
	int type;
	pthread_t owner;
	int count;
};

struct pthread_mutex_attr {
	int type;
};

struct pthread_cond {
	struct sem sem;
};

struct pthread_cond_attr {
	int shared;
};

struct pthread_rwlock {
	struct sem sem;
	_spinlock_lock_t lock;
	int readers;
	int writer;
};

struct pthread_rwlockattr {
	int dummy;
};

struct pthread_attr {
	void *stack_addr;
	size_t stack_size;
	size_t guard_size;
	int detach_state;
	int contention_scope;
	int sched_policy;
	struct sched_param sched_param;
	int sched_inherit;
	int create_suspended;
};

struct rthread_key {
	int used;
	void (*destructor)(void *);
};

struct rthread_storage {
	int keyid;
	struct rthread_storage *next;
	void *data;
};

struct rthread_cleanup_fn {
	void (*fn)(void *);
	void *arg;
	struct rthread_cleanup_fn *next;
};

struct pthread {
	struct sem donesem;
	pid_t tid;
	unsigned int flags;
	_spinlock_lock_t flags_lock;
	void *retval;
	void *(*fn)(void *);
	void *arg;
	char name[32];
	struct stack *stack;
	LIST_ENTRY(pthread) threads;
	TAILQ_ENTRY(pthread) waiting;
	int sched_policy;
	struct pthread_attr attr;
	struct sched_param sched_param;
	struct rthread_storage *local_storage;
	struct rthread_cleanup_fn *cleanup_fns;
};
#define	THREAD_DONE		0x001
#define	THREAD_DETACHED		0x002
#define THREAD_CANCELLED	0x004
#define THREAD_CANCEL_ENABLE	0x008
#define THREAD_CANCEL_DEFERRED	0x010

extern int _threads_ready;
extern LIST_HEAD(listhead, pthread) _thread_list;
extern struct pthread _initial_thread;
extern _spinlock_lock_t _thread_lock;

void	_spinlock(_spinlock_lock_t *);
void	_spinunlock(_spinlock_lock_t *);
int	_sem_wait(sem_t, int, int);
int	_sem_waitl(sem_t, int, int);
int	_sem_post(sem_t);
int	_sem_wakeup(sem_t);
int	_sem_wakeall(sem_t);

struct stack *_rthread_alloc_stack(pthread_t);
void	_rthread_free_stack(struct stack *);
void	_rthread_tls_destructors(pthread_t);
void	_rthread_debug(int, const char *, ...)
		__attribute__((__format__ (printf, 2, 3)));
void	_rthread_debug_init(void);
#if defined(__ELF__) && defined(PIC)
void	_rthread_dl_lock(int what);
void	_rthread_bind_lock(int);
#endif


void	_thread_dump_info(void);

int	_atomic_lock(register volatile _spinlock_lock_t *);

/* syscalls */
int getthrid(void);
void threxit(pid_t *);
int thrsleep(void *, int, void *);
int thrwakeup(void *, int n);
int sched_yield(void);
int thrsigdivert(sigset_t, siginfo_t *, const struct timespec *);
