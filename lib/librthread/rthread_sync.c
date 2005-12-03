/*	$OpenBSD: rthread_sync.c,v 1.1 2005/12/03 18:16:19 tedu Exp $ */
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
 * Mutexes, conditions, rwlocks, and semaphores - synchronization functions.
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
#include <errno.h>

#include <pthread.h>

#include "rthread.h"

int thrsleep(long, int, void *);
int thrwakeup(long);

/*
 * Internal implementation of semaphores
 */
int
_sem_wait(sem_t sem, int tryonly, int timo)
{
	int sleep;

	_spinlock(&sem->lock);
again:
	if (sem->value == 0) {
		if (tryonly) {
			_spinunlock(&sem->lock);
			return (0);
		}
		sem->waitcount++;
		sleep = 1;
	} else {
		sem->value--;
		sleep = 0;
	}

	if (sleep) {
		if (thrsleep((long)sem, timo, &sem->lock) == EWOULDBLOCK)
			return (0);
		_spinlock(&sem->lock);
		sem->waitcount--;
		goto again;
	}
	_spinunlock(&sem->lock);
	return (1);
}

int
_sem_wakeup(sem_t sem)
{
	int rv = 0;

	_spinlock(&sem->lock);
	sem->value++;
	if (sem->waitcount) {
		thrwakeup((long)sem);
		rv = 1;
	}
	_spinunlock(&sem->lock);
	return (rv);
}

int
_sem_wakeall(sem_t sem)
{
	int rv;

	_spinlock(&sem->lock);
	rv = sem->waitcount;
	sem->value += rv;
	thrwakeup((long)sem);
	_spinunlock(&sem->lock);

	return (rv);
}

/*
 * mutexen
 */
int
pthread_mutex_init(pthread_mutex_t *mutexp, const pthread_mutexattr_t *attr)
{
	pthread_mutex_t mutex;

	mutex = malloc(sizeof(*mutex));
	if (!mutex)
		return (errno);
	memset((void *)mutex, 0, sizeof(*mutex));
	mutex->sem.value = 1;	/* unlocked */
	mutex->type = attr ? (*attr)->type : PTHREAD_MUTEX_ERRORCHECK;
	*mutexp = mutex;

	return (0);
}

int
pthread_mutex_destroy(pthread_mutex_t *mutexp)
{

	/* check for waiters */
	free((void *)*mutexp);
	*mutexp = NULL;
	return (0);
}

int
rthread_mutex_lock(pthread_mutex_t *mutexp, int trywait)
{
	pthread_mutex_t mutex = *mutexp;
	pthread_t thread = pthread_self();

	if (!mutex) {
		pthread_mutex_init(mutexp, NULL);
		mutex = *mutexp;
	}
	if (mutex->owner == thread) {
		if (mutex->type == PTHREAD_MUTEX_RECURSIVE) {
			mutex->count++;
			return (0);
		}
		if (mutex->type == PTHREAD_MUTEX_ERRORCHECK)
			return (EDEADLK);
	}
	if (!_sem_wait((void *)&mutex->sem, trywait, 0))
		return (EBUSY);
	mutex->owner = thread;
	mutex->count = 1;

	return (0);
}

int
pthread_mutex_lock(pthread_mutex_t *p)
{
	return (rthread_mutex_lock(p, 0));
}

int
pthread_mutex_trylock(pthread_mutex_t *p)
{
	return (rthread_mutex_lock(p, 1));
}

int
pthread_mutex_unlock(pthread_mutex_t *mutexp)
{
	pthread_t thread = pthread_self();
	pthread_mutex_t mutex = *mutexp;

	if (mutex->owner != thread)
		return (EPERM);

	if (--mutex->count == 0) {
		mutex->owner = NULL;
		_sem_wakeup((void *)&mutex->sem);
	}

	return (0);
}

/*
 * mutexen attributes
 */
int
pthread_mutexattr_init(pthread_mutexattr_t *attrp)
{
	pthread_mutexattr_t attr;

	attr = malloc(sizeof(*attr));
	if (!attr)
		return (errno);
	memset(attr, 0, sizeof(*attr));
	attr->type = PTHREAD_MUTEX_ERRORCHECK;
	*attrp = attr;

	return (0);
}

int
pthread_mutexattr_destroy(pthread_mutexattr_t *attrp)
{
	free(*attrp);
	*attrp = NULL;

	return (0);
}

int
pthread_mutexattr_settype(pthread_mutexattr_t *attrp, int type)
{
	(*attrp)->type = type;

	return (0);
}

/*
 * condition variables
 */
int
pthread_cond_init(pthread_cond_t *condp, const pthread_condattr_t *attrp)
{
	pthread_cond_t cond;

	cond = malloc(sizeof(*cond));
	if (!cond)
		return (errno);
	memset(cond, 0, sizeof(*cond));

	*condp = cond;

	return (0);
}

int
pthread_cond_destroy(pthread_cond_t *condp)
{

	free(*condp);
	*condp = NULL;

	return (0);
}

int
pthread_cond_timedwait(pthread_cond_t *condp, pthread_mutex_t *mutexp,
    const struct timespec *abstime)
{
	int error;

	if (!*condp)
		if ((error = pthread_cond_init(condp, NULL)))
			return (error);

	pthread_mutex_unlock(mutexp);
	_sem_wait(&(*condp)->sem, 0, (abstime ?
	    abstime->tv_sec * 100 + abstime->tv_nsec / 10000000 : 0));
	error = pthread_mutex_lock(mutexp);

	return (error);
}

int
pthread_cond_wait(pthread_cond_t *condp, pthread_mutex_t *mutexp)
{
	return (pthread_cond_timedwait(condp, mutexp, NULL));
}

int
pthread_cond_signal(pthread_cond_t *condp)
{
	int error;

	if (!*condp)
		if ((error = pthread_cond_init(condp, NULL)))
			return (error);

	_sem_wakeup(&(*condp)->sem);

	return (0);
}

int
pthread_cond_broadcast(pthread_cond_t *condp)
{
	if (!*condp)
		pthread_cond_init(condp, NULL);

	_sem_wakeall(&(*condp)->sem);

	return (0);
}

/*
 * condition variable attributes
 */
int
pthread_condattr_init(pthread_condattr_t *attrp)
{
	pthread_condattr_t attr;

	attr = malloc(sizeof(*attr));
	if (!attr)
		return (errno);
	memset(attr, 0, sizeof(*attr));
	*attrp = attr;

	return (0);
}

int
pthread_condattr_destroy(pthread_condattr_t *attrp)
{
	free(*attrp);
	*attrp = NULL;

	return (0);
}

/*
 * rwlocks
 */
int
pthread_rwlock_init(pthread_rwlock_t *lockp, const pthread_rwlockattr_t *attrp)
{
	pthread_rwlock_t lock;

	lock = malloc(sizeof(*lock));
	if (!lock)
		return (errno);
	memset(lock, 0, sizeof(*lock));
	*lockp = lock;

	return (0);
}

int
pthread_rwlock_destroy(pthread_rwlock_t *lockp)
{
	free(*lockp);
	*lockp = NULL;

	return (0);
}

int
pthread_rwlock_rdlock(pthread_rwlock_t *lockp)
{
	pthread_rwlock_t lock;

	lock = *lockp;
again:
	_spinlock(&lock->lock);
	if (lock->writer) {
		_spinunlock(&lock->lock);
		_sem_wait(&lock->sem, 0, 0);
		goto again;
	}
	lock->readers++;
	_spinunlock(&lock->lock);

	return (0);
}

int
pthread_rwlock_tryrdlock(pthread_rwlock_t *lockp)
{
	pthread_rwlock_t lock;

	lock = *lockp;

	_spinlock(&lock->lock);
	if (lock->writer) {
		_spinunlock(&lock->lock);
		return (EBUSY);
	}
	lock->readers++;
	_spinunlock(&lock->lock);

	return (0);
}

int
pthread_rwlock_wrlock(pthread_rwlock_t *lockp)
{
	pthread_rwlock_t lock;

	lock = *lockp;

again:
	_spinlock(&lock->lock);
	lock->writer++;
	if (lock->readers) {
		_spinunlock(&lock->lock);
		_sem_wait(&lock->sem, 0, 0);
		goto again;
	}
	lock->readers = -pthread_self()->tid;
	_spinunlock(&lock->lock);

	return (0);
}

int
pthread_rwlock_trywrlock(pthread_rwlock_t *lockp)
{
	pthread_rwlock_t lock;

	lock = *lockp;

	_spinlock(&lock->lock);
	if (lock->readers || lock->writer) {
		_spinunlock(&lock->lock);
		return (EBUSY);
	}
	lock->writer = 1;
	lock->readers = -pthread_self()->tid;
	_spinunlock(&lock->lock);

	return (0);
}

int
pthread_rwlock_unlock(pthread_rwlock_t *lockp)
{
	pthread_rwlock_t lock;

	lock = *lockp;

	_spinlock(&lock->lock);
	if (lock->readers == -pthread_self()->tid) {
		lock->readers = 0;
		lock->writer--;
	} else if (lock->readers > 0) {
		lock->readers--;
	} else {
		_spinunlock(&lock->lock);
		return (EPERM);
	}
	_spinunlock(&lock->lock);
	_sem_wakeall(&lock->sem);

	return (0);
}

/*
 * rwlock attributes
 */
int
pthread_rwlockattr_init(pthread_rwlockattr_t *attrp)
{
	pthread_rwlockattr_t attr;

	attr = malloc(sizeof(*attr));
	if (!attr)
		return (errno);
	memset(attr, 0, sizeof(*attr));
	*attrp = attr;

	return (0);
}

int
pthread_rwlockattr_destory(pthread_rwlockattr_t *attrp)
{
	free(*attrp);
	*attrp = NULL;

	return (0);
}
