/*	$OpenBSD: rthread_rwlock.c,v 1.1 2011/12/21 23:59:03 guenther Exp $ */
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
 * rwlocks
 */


#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <pthread.h>

#include "rthread.h"


static _spinlock_lock_t rwlock_init_lock = _SPINLOCK_UNLOCKED;

int
pthread_rwlock_init(pthread_rwlock_t *lockp, const pthread_rwlockattr_t *attrp)
{
	pthread_rwlock_t lock;

	lock = calloc(1, sizeof(*lock));
	if (!lock)
		return (errno);
	lock->lock = _SPINLOCK_UNLOCKED;
	lock->sem.lock = _SPINLOCK_UNLOCKED;
	*lockp = lock;

	return (0);
}

int
pthread_rwlock_destroy(pthread_rwlock_t *lockp)
{
	if ((*lockp) && ((*lockp)->readers || (*lockp)->writer)) {
#define MSG "pthread_rwlock_destroy on rwlock with waiters!\n"
		write(2, MSG, sizeof(MSG) - 1);
#undef MSG
		return (EBUSY);
	}
	free(*lockp);
	*lockp = NULL;

	return (0);
}

static int
_rthread_rwlock_ensure_init(pthread_rwlock_t *lockp)
{
	int ret = 0;

	/*
	 * If the rwlock is statically initialized, perform the dynamic
	 * initialization.
	 */
	if (*lockp == NULL)
	{
		_spinlock(&rwlock_init_lock);
		if (*lockp == NULL)
			ret = pthread_rwlock_init(lockp, NULL);
		_spinunlock(&rwlock_init_lock);
	}
	return (ret);
}


int
pthread_rwlock_rdlock(pthread_rwlock_t *lockp)
{
	pthread_rwlock_t lock;
	int error;

	if ((error = _rthread_rwlock_ensure_init(lockp)))
		return (error);

	lock = *lockp;
again:
	_spinlock(&lock->lock);
	if (lock->writer) {
		_spinlock(&lock->sem.lock);
		_spinunlock(&lock->lock);
		_sem_waitl(&lock->sem, 0, 0, NULL);
		goto again;
	}
	lock->readers++;
	_spinunlock(&lock->lock);

	return (0);
}

int
pthread_rwlock_timedrdlock(pthread_rwlock_t *lockp,
    const struct timespec *abstime)
{
	pthread_rwlock_t lock;
	int do_wait = 1;
	int error;

	if ((error = _rthread_rwlock_ensure_init(lockp)))
		return (error);

	lock = *lockp;
	_spinlock(&lock->lock);
	while (lock->writer && do_wait) {
		_spinlock(&lock->sem.lock);
		_spinunlock(&lock->lock);
		do_wait = _sem_waitl(&lock->sem, 0, CLOCK_REALTIME, abstime);
		_spinlock(&lock->lock);
	}
	if (lock->writer) {
		/* do_wait must be 0, so timed out */
		_spinunlock(&lock->lock);
		return (ETIMEDOUT);
	}
	lock->readers++;
	_spinunlock(&lock->lock);

	return (0);
}

int
pthread_rwlock_tryrdlock(pthread_rwlock_t *lockp)
{
	pthread_rwlock_t lock;
	int error;

	if ((error = _rthread_rwlock_ensure_init(lockp)))
		return (error);

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
	int error;

	if ((error = _rthread_rwlock_ensure_init(lockp)))
		return (error);

	lock = *lockp;

	_spinlock(&lock->lock);
	lock->writer++;
	while (lock->readers) {
		_spinlock(&lock->sem.lock);
		_spinunlock(&lock->lock);
		_sem_waitl(&lock->sem, 0, 0, NULL);
		_spinlock(&lock->lock);
	}
	lock->readers = -pthread_self()->tid;
	_spinunlock(&lock->lock);

	return (0);
}

int
pthread_rwlock_timedwrlock(pthread_rwlock_t *lockp,
    const struct timespec *abstime)
{
	pthread_rwlock_t lock;
	int do_wait = 1;
	int error;

	if ((error = _rthread_rwlock_ensure_init(lockp)))
		return (error);

	lock = *lockp;

	_spinlock(&lock->lock);
	lock->writer++;
	while (lock->readers && do_wait) {
		_spinlock(&lock->sem.lock);
		_spinunlock(&lock->lock);
		do_wait = _sem_waitl(&lock->sem, 0, CLOCK_REALTIME, abstime);
		_spinlock(&lock->lock);
	}
	if (lock->readers) {
		/* do_wait must be 0, so timed out */
		lock->writer--;
		_spinunlock(&lock->lock);
		return (ETIMEDOUT);
	}
	lock->readers = -pthread_self()->tid;
	_spinunlock(&lock->lock);

	return (0);
}

int
pthread_rwlock_trywrlock(pthread_rwlock_t *lockp)
{
	pthread_rwlock_t lock;
	int error;

	if ((error = _rthread_rwlock_ensure_init(lockp)))
		return (error);

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
