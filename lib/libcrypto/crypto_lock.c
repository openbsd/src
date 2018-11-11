/* $OpenBSD: crypto_lock.c,v 1.1 2018/11/11 06:41:28 bcook Exp $ */
/*
 * Copyright (c) 2018 Brent Cook <bcook@openbsd.org>
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

#include <openssl/crypto.h>

static pthread_mutex_t locks[CRYPTO_NUM_LOCKS];

void
crypto_init_locks(void)
{
	int i;

	for (i = 0; i < CRYPTO_NUM_LOCKS; i++)
		pthread_mutex_init(&locks[i], NULL);
}

void
CRYPTO_lock(int mode, int type, const char *file, int line)
{
	if (type < 0 || type >= CRYPTO_NUM_LOCKS)
		return;

	if (mode & CRYPTO_LOCK)
		pthread_mutex_lock(&locks[type]);
	else
		pthread_mutex_unlock(&locks[type]);
}

int
CRYPTO_add_lock(int *pointer, int amount, int type, const char *file,
    int line)
{
	int ret = 0;
	CRYPTO_lock(CRYPTO_LOCK|CRYPTO_WRITE, type, file, line);
	ret = *pointer + amount;
	*pointer = ret;
	CRYPTO_lock(CRYPTO_UNLOCK|CRYPTO_WRITE, type, file, line);
	return (ret);
}
