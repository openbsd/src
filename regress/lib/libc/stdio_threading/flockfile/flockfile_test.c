/*	$OpenBSD: flockfile_test.c,v 1.1 2026/01/19 23:01:00 guenther Exp $	*/
/*
 * Copyright (c) 2026 Philip Guenther <guenther@openbsd.org>
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

#include <stdio.h>
#include <pthread.h>
#include "local.h"

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
int did_test1 = 0;

static void *
test1(void *v)
{
	int r;

	if (ftrylockfile(stdin) == 0)
		errx(1, "Lock on stdin never held");

	if ((r = pthread_mutex_lock(&lock)))
		errc(1, r, "could not lock lock");
	did_test1 = 1;
	if ((r = pthread_cond_signal(&cond)))
		errc(1, r, "could not signal cond");
	if ((r = pthread_mutex_unlock(&lock)))
		errc(1, r, "could not unlock lock");

	flockfile(stdin);
	funlockfile(stdin);
	return NULL;
}

int
main(void)
{
	int fd, r;
	pthread_t thr;

	/*
	 * Make sure flockfile() is handled correctly before the first
	 * thread is created.
	 */
	flockfile(stdin);
	flockfile(stdin);

	if ((r = pthread_create(&thr, NULL, test1, NULL)))
		warnc(r, "could not create thread");

	if ((r = pthread_mutex_lock(&lock)))
		errc(1, r, "could not lock lock");
	while (did_test1 == 0)
		if ((r = pthread_cond_wait(&cond, &lock)))
			errc(1, r, "could not wait on cond");
	if ((r = pthread_mutex_unlock(&lock)))
		errc(1, r, "could not unlock lock");

	funlockfile(stdin);
	funlockfile(stdin);

	if ((r = pthread_join(thr, NULL)))
		warnc(r, "could not join thread");

	exit(0);
}
