/*	$OpenBSD */
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

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <pthread.h>

#include "rthread.h"

/*
 * Internal implementation of semaphores
 */
int
_sem_wait(sem_t sem, int tryonly)
{

	_spinlock(&sem->lock);
	return (_sem_waitl(sem, tryonly, 0, NULL));
}

int
_sem_waitl(sem_t sem, int tryonly, clockid_t clock_id,
    const struct timespec *abstime)
{
	int do_sleep;

again:
	if (sem->value == 0) {
		if (tryonly) {
			_spinunlock(&sem->lock);
			return (0);
		}
		sem->waitcount++;
		do_sleep = 1;
	} else {
		sem->value--;
		do_sleep = 0;
	}

	if (do_sleep) {
		if (thrsleep(sem, clock_id, abstime, &sem->lock) == -1 &&
		    errno == EWOULDBLOCK)
			return (0);
		_spinlock(&sem->lock);
		sem->waitcount--;
		goto again;
	}
	_spinunlock(&sem->lock);
	return (1);
}

/* always increment count */
int
_sem_post(sem_t sem)
{
	int rv = 0;

	_spinlock(&sem->lock);
	sem->value++;
	if (sem->waitcount) {
		thrwakeup(sem, 1);
		rv = 1;
	}
	_spinunlock(&sem->lock);
	return (rv);
}

/* only increment count if a waiter */
int
_sem_wakeup(sem_t sem)
{
	int rv = 0;

	_spinlock(&sem->lock);
	if (sem->waitcount) {
		sem->value++;
		thrwakeup(sem, 1);
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
	thrwakeup(sem, 0);
	_spinunlock(&sem->lock);

	return (rv);
}

/*
 * exported semaphores
 */
int
sem_init(sem_t *semp, int pshared, unsigned int value)
{
	sem_t sem;

	if (pshared) {
		errno = EPERM;
		return (-1);
	}

	sem = calloc(1, sizeof(*sem));
	if (!sem)
		return (-1);
	sem->value = value;
	*semp = sem;

	return (0);
}

int
sem_destroy(sem_t *semp)
{
	if (!*semp)
		return (EINVAL);
	if ((*semp)->waitcount) {
#define MSG "sem_destroy on semaphore with waiters!\n"
		write(2, MSG, sizeof(MSG) - 1);
#undef MSG
		return (EBUSY);
	}
	free(*semp);
	*semp = NULL;

	return (0);
}

int
sem_getvalue(sem_t *semp, int *sval)
{
	sem_t sem = *semp;

	_spinlock(&sem->lock);
	*sval = sem->value;
	_spinunlock(&sem->lock);

	return (0);
}

int
sem_post(sem_t *semp)
{
	sem_t sem = *semp;

	_sem_post(sem);

	return (0);
}

int
sem_wait(sem_t *semp)
{
	sem_t sem = *semp;

	_sem_wait(sem, 0);

	return (0);
}

int
sem_trywait(sem_t *semp)
{
	sem_t sem = *semp;
	int rv;

	rv = _sem_wait(sem, 1);

	if (!rv) {
		errno = EAGAIN;
		return (-1);
	}

	return (0);
}

