/*	$OpenBSD: rthread.h,v 1.1 2005/12/03 18:16:19 tedu Exp $ */
/*
 * Copyright (c) 2004 Ted Unangst <tedu@openbsd.org>
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
 */

struct stack {
	void *sp;
	void *base;
	size_t len;
};

typedef struct semaphore {
	volatile int waitcount;
	volatile int value;
	_spinlock_lock_t lock;
} *sem_t;

struct pthread_mutex {
	struct semaphore sem;
	int type;
	pthread_t owner;
	int count;
};

struct pthread_mutex_attr {
	int type;
};

struct pthread_cond {
	struct semaphore sem;
};

struct pthread_cond_attr {
	int shared;
};

struct pthread_rwlock {
	_spinlock_lock_t lock;
	int readers;
	int writer;
	struct semaphore sem;
};

struct pthread_rwlockattr {
	int dummy;
};

struct pthread_attr {
	size_t stack_size;
	int detach_state;
	int contention_scope;
	int sched_policy;
	struct sched_param sched_param;
	int sched_inherit;
};

struct rthread_key {
	int keyid;
	struct rthread_key *next;
	void (*destructor)(void *);
};

struct rthread_storage {
	int keyid;
	struct rthread_storage *next;
	void *data;
};

struct pthread {
	pid_t tid;
	struct semaphore donesem;
	int flags;
	void *retval;
	void *(*fn)(void *);
	void *arg;
	char name[32];
	struct stack *stack;
	pthread_t next;
	int sched_policy;
	struct sched_param sched_param;
	struct rthread_storage *local_storage;
	int sigpend;
};
#define	THREAD_DONE	0x001
#define	THREAD_DETACHED	0x002

void	_spinlock(_spinlock_lock_t *);
void	_spinunlock(_spinlock_lock_t *);
int	_sem_wait(sem_t, int, int);
int	_sem_wakeup(sem_t);
int	_sem_wakeall(sem_t);

int	_atomic_lock(register volatile _spinlock_lock_t *);
