/* $OpenBSD: thread_mutex.c,v 1.1 2007/06/05 18:11:48 kurt Exp $ */

/*
 * Copyright (c) 2007 Kurt Miller <kurt@openbsd.org>
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

#include <sys/types.h>
#include "thread_private.h"
#include "pthread.h"
#include "pthread_private.h"

void
_thread_mutex_lock(void **mutex)
{
	pthread_mutex_t	*pmutex = (pthread_mutex_t *)mutex;

	if (pthread_mutex_lock(pmutex) != 0)
		PANIC("mutex lock failure");
}

void
_thread_mutex_unlock(void **mutex)
{
	pthread_mutex_t	*pmutex = (pthread_mutex_t *)mutex;

	if (pthread_mutex_unlock(pmutex) != 0)
		PANIC("mutex unlock failure");
}

void
_thread_mutex_destroy(void **mutex)
{
	pthread_mutex_t	*pmutex = (pthread_mutex_t *)mutex;

	if (pthread_mutex_destroy(pmutex) != 0)
		PANIC("mutex destroy failure");
}
