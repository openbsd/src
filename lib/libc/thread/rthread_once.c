/*	$OpenBSD: rthread_once.c,v 1.2 2017/10/28 21:25:24 guenther Exp $ */
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

#include <pthread.h>
#include <stdlib.h>

static void
mutex_unlock(void *arg)
{
	if (pthread_mutex_unlock(arg))
		abort();
}

int
pthread_once(pthread_once_t *once_control, void (*init_routine)(void))
{
	pthread_mutex_lock(&once_control->mutex);
	if (once_control->state == PTHREAD_NEEDS_INIT) {
		pthread_cleanup_push(mutex_unlock, &once_control->mutex);
		init_routine();
		pthread_cleanup_pop(0);
		once_control->state = PTHREAD_DONE_INIT;
	}
	pthread_mutex_unlock(&once_control->mutex);

	return (0);
}
