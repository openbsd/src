/*	$OpenBSD: rthread_sync.c,v 1.28 2012/01/04 17:43:34 mpi Exp $ */
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
 * Mutexes and conditions - synchronization functions.
 */


#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <pthread.h>

#include "rthread.h"

static _spinlock_lock_t static_init_lock = _SPINLOCK_UNLOCKED;

/*
 * mutexen
 */
int
pthread_mutex_init(pthread_mutex_t *mutexp, const pthread_mutexattr_t *attr)
{
	pthread_mutex_t mutex;

	mutex = calloc(1, sizeof(*mutex));
	if (!mutex)
		return (errno);
	mutex->sem.lock = _SPINLOCK_UNLOCKED;
	mutex->sem.value = 1;	/* unlocked */
	if (attr == NULL) {
		mutex->type = PTHREAD_MUTEX_ERRORCHECK;
		mutex->prioceiling = PTHREAD_PRIO_NONE;
	} else {
		mutex->type = (*attr)->ma_type;
		mutex->prioceiling = (*attr)->ma_protocol ==
		    PTHREAD_PRIO_PROTECT ? (*attr)->ma_prioceiling : -1;
	}
	*mutexp = mutex;

	return (0);
}

int
pthread_mutex_destroy(pthread_mutex_t *mutexp)
{

	if ((*mutexp) && (*mutexp)->count) {
#define MSG "pthread_mutex_destroy on mutex with waiters!\n"
		write(2, MSG, sizeof(MSG) - 1);
#undef MSG
		return (EBUSY);
	}
	free((void *)*mutexp);
	*mutexp = NULL;
	return (0);
}

static int
_rthread_mutex_lock(pthread_mutex_t *mutexp, int trywait)
{
	pthread_mutex_t mutex;
	pthread_t thread = pthread_self();
	int ret = 0;

	/*
	 * If the mutex is statically initialized, perform the dynamic
	 * initialization. Note: _thread_mutex_lock() in libc requires
	 * _rthread_mutex_lock() to perform the mutex init when *mutexp
	 * is NULL.
	 */
	if (*mutexp == NULL) {
		_spinlock(&static_init_lock);
		if (*mutexp == NULL)
			ret = pthread_mutex_init(mutexp, NULL);
		_spinunlock(&static_init_lock);
		if (ret != 0)
			return (EINVAL);
	}
	mutex = *mutexp;
	if (mutex->owner == thread) {
		if (mutex->type == PTHREAD_MUTEX_RECURSIVE) {
			mutex->count++;
			return (0);
		}
		if (mutex->type == PTHREAD_MUTEX_ERRORCHECK)
			return (trywait ? EBUSY : EDEADLK);
	}
	if (!_sem_wait((void *)&mutex->sem, trywait))
		return (EBUSY);
	mutex->owner = thread;
	mutex->count = 1;

	return (0);
}

int
pthread_mutex_lock(pthread_mutex_t *p)
{
	return (_rthread_mutex_lock(p, 0));
}

int
pthread_mutex_trylock(pthread_mutex_t *p)
{
	return (_rthread_mutex_lock(p, 1));
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
		_sem_post((void *)&mutex->sem);
	}

	return (0);
}

/*
 * condition variables
 */
int
pthread_cond_init(pthread_cond_t *condp, const pthread_condattr_t *attrp)
{
	pthread_cond_t cond;

	cond = calloc(1, sizeof(*cond));
	if (!cond)
		return (errno);
	cond->sem.lock = _SPINLOCK_UNLOCKED;

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
	int rv;

	if (!*condp)
		if ((error = pthread_cond_init(condp, NULL)))
			return (error);

	_spinlock(&(*condp)->sem.lock);
	pthread_mutex_unlock(mutexp);
	rv = _sem_waitl(&(*condp)->sem, 0, CLOCK_REALTIME, abstime);
	error = pthread_mutex_lock(mutexp);

	return (error ? error : rv ? 0 : ETIMEDOUT);
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

	attr = calloc(1, sizeof(*attr));
	if (!attr)
		return (errno);
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

