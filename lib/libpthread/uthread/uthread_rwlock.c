/*	$OpenBSD: uthread_rwlock.c,v 1.7 2010/04/12 01:54:23 tedu Exp $	*/
/*-
 * Copyright (c) 1998 Alex Nash
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: uthread_rwlock.c,v 1.9 2004/01/08 15:39:12 deischen Exp $
 */

#ifdef _THREAD_SAFE
#include <errno.h>
#include <limits.h>
#include <stdlib.h>

#include <pthread.h>
#include "pthread_private.h"

/* maximum number of times a read lock may be obtained */
#define	MAX_READ_LOCKS		(INT_MAX - 1)

static int init_static (pthread_rwlock_t *rwlock);

static spinlock_t static_init_lock = _SPINLOCK_INITIALIZER;

static int
init_static (pthread_rwlock_t *rwlock)
{
	int ret;

	_SPINLOCK(&static_init_lock);

	if (*rwlock == NULL)
		ret = pthread_rwlock_init(rwlock, NULL);
	else
		ret = 0;

	_SPINUNLOCK(&static_init_lock);

	return (ret);
}

int
pthread_rwlock_destroy (pthread_rwlock_t *rwlock)
{
	int ret;

	if (rwlock == NULL)
		ret = EINVAL;
	else {
		pthread_rwlock_t prwlock;

		prwlock = *rwlock;

		pthread_mutex_destroy(&prwlock->lock);
		pthread_cond_destroy(&prwlock->read_signal);
		pthread_cond_destroy(&prwlock->write_signal);
		free(prwlock);

		*rwlock = NULL;

		ret = 0;
	}
	return (ret);
}

/* ARGSUSED */
int
pthread_rwlock_init (pthread_rwlock_t *rwlock, const pthread_rwlockattr_t *attr)
{
	pthread_rwlock_t prwlock;
	int ret;

	/* allocate rwlock object */
	prwlock = (pthread_rwlock_t)malloc(sizeof(struct pthread_rwlock));

	if (prwlock == NULL)
		return(ENOMEM);

	/* initialize the lock */
	if ((ret = pthread_mutex_init(&prwlock->lock, NULL)) != 0)
		free(prwlock);
	else {
		/* initialize the read condition signal */
		ret = pthread_cond_init(&prwlock->read_signal, NULL);

		if (ret != 0) {
			pthread_mutex_destroy(&prwlock->lock);
			free(prwlock);
		} else {
			/* initialize the write condition signal */
			ret = pthread_cond_init(&prwlock->write_signal, NULL);

			if (ret != 0) {
				pthread_cond_destroy(&prwlock->read_signal);
				pthread_mutex_destroy(&prwlock->lock);
				free(prwlock);
			} else {
				/* success */
				prwlock->state = 0;
				prwlock->blocked_writers = 0;

				*rwlock = prwlock;
			}
		}
	}

	return (ret);
}

static int
rwlock_rdlock_common (pthread_rwlock_t *rwlock, const struct timespec *abstime)
{
	pthread_rwlock_t prwlock;
	struct pthread *curthread;
	int ret;

	if (rwlock == NULL)
		return(EINVAL);

	prwlock = *rwlock;

	/* check for static initialization */
	if (prwlock == NULL) {
		if ((ret = init_static(rwlock)) != 0)
			return(ret);

		prwlock = *rwlock;
	}

	/* grab the monitor lock */
	if ((ret = pthread_mutex_lock(&prwlock->lock)) != 0)
		return(ret);

	/* check lock count */
	if (prwlock->state == MAX_READ_LOCKS) {
		pthread_mutex_unlock(&prwlock->lock);
		return (EAGAIN);
	}

	curthread = _get_curthread();
	if ((curthread->rdlock_count > 0) && (prwlock->state > 0)) {
		/*
		 * To avoid having to track all the rdlocks held by
		 * a thread or all of the threads that hold a rdlock,
		 * we keep a simple count of all the rdlocks held by
		 * a thread.  If a thread holds any rdlocks it is
		 * possible that it is attempting to take a recursive
		 * rdlock.  If there are blocked writers and precedence
		 * is given to them, then that would result in the thread
		 * deadlocking.  So allowing a thread to take the rdlock
		 * when it already has one or more rdlocks avoids the
		 * deadlock.  I hope the reader can follow that logic ;-)
		 */
		;	/* nothing needed */
	} else {
		/* give writers priority over readers */
		while (prwlock->blocked_writers || prwlock->state < 0) {
			if (abstime) {
				ret = pthread_cond_timedwait(&prwlock->read_signal,
					&prwlock->lock, abstime);
			} else {
				ret = pthread_cond_wait(&prwlock->read_signal,
					&prwlock->lock);
			}

			if (ret != 0) {
				/* can't do a whole lot if this fails */
				pthread_mutex_unlock(&prwlock->lock);
				return(ret);
			}
		}
	}

	curthread->rdlock_count++;
	prwlock->state++; /* indicate we are locked for reading */

	/*
	 * Something is really wrong if this call fails.  Returning
	 * error won't do because we've already obtained the read
	 * lock.  Decrementing 'state' is no good because we probably
	 * don't have the monitor lock.
	 */
	pthread_mutex_unlock(&prwlock->lock);

	return (ret);
}

int
pthread_rwlock_rdlock (pthread_rwlock_t *rwlock)
{
	return rwlock_rdlock_common (rwlock, NULL);
}

int
pthread_rwlock_timedrdlock (pthread_rwlock_t *rwlock,
	 const struct timespec *abstime)
{
	return rwlock_rdlock_common(rwlock, abstime);
}

int
pthread_rwlock_tryrdlock (pthread_rwlock_t *rwlock)
{
	pthread_rwlock_t prwlock;
	struct pthread *curthread;
	int ret;

	if (rwlock == NULL)
		return(EINVAL);

	prwlock = *rwlock;

	/* check for static initialization */
	if (prwlock == NULL) {
		if ((ret = init_static(rwlock)) != 0)
			return(ret);

		prwlock = *rwlock;
	}

	/* grab the monitor lock */
	if ((ret = pthread_mutex_lock(&prwlock->lock)) != 0)
		return(ret);

	curthread = _get_curthread();
	if (prwlock->state == MAX_READ_LOCKS)
		ret = EAGAIN; /* too many read locks acquired */
	else if ((curthread->rdlock_count > 0) && (prwlock->state > 0)) {
		/* see comment for pthread_rwlock_rdlock() */
		curthread->rdlock_count++;
		prwlock->state++;
	}
	/* give writers priority over readers */
	else if (prwlock->blocked_writers || prwlock->state < 0)
		ret = EBUSY;
	else {
		prwlock->state++; /* indicate we are locked for reading */
		curthread->rdlock_count++;
	}

	/* see the comment on this in pthread_rwlock_rdlock */
	pthread_mutex_unlock(&prwlock->lock);

	return (ret);
}

int
pthread_rwlock_trywrlock (pthread_rwlock_t *rwlock)
{
	pthread_rwlock_t prwlock;
	int ret;

	if (rwlock == NULL)
		return(EINVAL);

	prwlock = *rwlock;

	/* check for static initialization */
	if (prwlock == NULL) {
		if ((ret = init_static(rwlock)) != 0)
			return(ret);

		prwlock = *rwlock;
	}

	/* grab the monitor lock */
	if ((ret = pthread_mutex_lock(&prwlock->lock)) != 0)
		return(ret);

	if (prwlock->state != 0)
		ret = EBUSY;
	else
		/* indicate we are locked for writing */
		prwlock->state = -1;

	/* see the comment on this in pthread_rwlock_rdlock */
	pthread_mutex_unlock(&prwlock->lock);

	return (ret);
}

int
pthread_rwlock_unlock (pthread_rwlock_t *rwlock)
{
	pthread_rwlock_t prwlock;
	struct pthread *curthread;
	int ret;

	if (rwlock == NULL)
		return(EINVAL);

	prwlock = *rwlock;

	if (prwlock == NULL)
		return(EINVAL);

	/* grab the monitor lock */
	if ((ret = pthread_mutex_lock(&prwlock->lock)) != 0)
		return(ret);

	curthread = _get_curthread();
	if (prwlock->state > 0) {
		curthread->rdlock_count--;
		prwlock->state--;
		if (prwlock->state == 0 && prwlock->blocked_writers)
			ret = pthread_cond_signal(&prwlock->write_signal);
	} else if (prwlock->state < 0) {
		prwlock->state = 0;

		if (prwlock->blocked_writers)
			ret = pthread_cond_signal(&prwlock->write_signal);
		else
			ret = pthread_cond_broadcast(&prwlock->read_signal);
	} else
		ret = EINVAL;

	/* see the comment on this in pthread_rwlock_rdlock */
	pthread_mutex_unlock(&prwlock->lock);

	return (ret);
}

static int
rwlock_wrlock_common (pthread_rwlock_t *rwlock, const struct timespec *abstime)
{
	pthread_rwlock_t prwlock;
	int ret;

	if (rwlock == NULL)
		return(EINVAL);

	prwlock = *rwlock;

	/* check for static initialization */
	if (prwlock == NULL) {
		if ((ret = init_static(rwlock)) != 0)
			return(ret);

		prwlock = *rwlock;
	}

	/* grab the monitor lock */
	if ((ret = pthread_mutex_lock(&prwlock->lock)) != 0)
		return(ret);

	while (prwlock->state != 0) {
		prwlock->blocked_writers++;

		if (abstime != NULL) {
			ret = pthread_cond_timedwait(&prwlock->write_signal,
				&prwlock->lock, abstime);
		} else {
			ret = pthread_cond_wait(&prwlock->write_signal,
				&prwlock->lock);
		}

		if (ret != 0) {
			prwlock->blocked_writers--;
			pthread_mutex_unlock(&prwlock->lock);
			return(ret);
		}

		prwlock->blocked_writers--;
	}

	/* indicate we are locked for writing */
	prwlock->state = -1;

	/* see the comment on this in pthread_rwlock_rdlock */
	pthread_mutex_unlock(&prwlock->lock);

	return (ret);
}

int
pthread_rwlock_wrlock (pthread_rwlock_t *rwlock)
{
	return rwlock_wrlock_common (rwlock, NULL);
}

int
pthread_rwlock_timedwrlock (pthread_rwlock_t *rwlock,
	const struct timespec *abstime)
{
	return rwlock_wrlock_common (rwlock, abstime);
}

#endif /* _THREAD_SAFE */
