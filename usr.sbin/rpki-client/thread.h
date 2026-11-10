/*	$OpenBSD: thread.h,v 1.1 2026/11/10 11:04:01 claudio Exp $ */

/*
 * Copyright (c) 2026 Claudio Jeker <claudio@openbsd.org>
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

#include <err.h>
#include <pthread.h>
#include <string.h>

static inline void
rw_enter_read(pthread_rwlock_t *lock)
{
	int error;

	if ((error = pthread_rwlock_rdlock(lock)) != 0)
		errx(1, "pthread_rwlock_rdlock: %s", strerror(error));
}

static inline void
rw_enter_write(pthread_rwlock_t *lock)
{
	int error;

	if ((error = pthread_rwlock_wrlock(lock)) != 0)
		errx(1, "pthread_rwlock_wrlock: %s", strerror(error));
}

static inline void
rw_leave(pthread_rwlock_t *lock)
{
	int error;

	if ((error = pthread_rwlock_unlock(lock)) != 0)
		errx(1, "pthread_rwlock_unlock: %s", strerror(error));
}

static inline void
rw_destroy(pthread_rwlock_t *lock)
{
	int error;

	if ((error = pthread_rwlock_destroy(lock)) != 0)
		errx(1, "pthread_rwlock_destroy: %s", strerror(error));
}

static inline void
mtx_enter(pthread_mutex_t *mutex)
{
	int error;

	if ((error = pthread_mutex_lock(mutex)) != 0)
		errx(1, "pthread_mutex_lock: %s", strerror(error));
}

static inline void
mtx_leave(pthread_mutex_t *mutex)
{
	int error;

	if ((error = pthread_mutex_unlock(mutex)) != 0)
		errx(1, "pthread_mutex_unlock: %s", strerror(error));
}

static inline void
mtx_destroy(pthread_mutex_t *mutex)
{
	int error;

	if ((error = pthread_mutex_destroy(mutex)) != 0)
		errx(1, "pthread_mutex_destroy: %s", strerror(error));
}	

static inline void
cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
	int error;

 	if ((error = pthread_cond_wait(cond, mutex)) != 0)
		errx(1, "pthread_cond_wait: %s", strerror(error));
}

static inline void
cond_signal(pthread_cond_t *cond)
{
	int error;

	if ((error = pthread_cond_signal(cond)) != 0)
		errx(1, "pthread_cond_signal: %s", strerror(error));
}

static inline void
cond_signal_all(pthread_cond_t *cond)
{
	int error;

	if ((error = pthread_cond_broadcast(cond)) != 0)
		errx(1, "pthread_cond_broadcast: %s", strerror(error));
}

static inline void
cond_destroy(pthread_cond_t *cond)
{
	int error;

	if ((error = pthread_cond_destroy(cond)) != 0)
		errx(1, "pthread_cond_destroy: %s", strerror(error));
}
