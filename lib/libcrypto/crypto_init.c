/*
 * Copyright (c) 2018 Bob Beck <beck@openbsd.org>
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

/* OpenSSL style init */

#include <pthread.h>
#include <stdio.h>

#include <openssl/objects.h>
#include <openssl/conf.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include "cryptlib.h"

int OpenSSL_config(const char *);
int OpenSSL_no_config(void);

static pthread_t crypto_init_thread;

static void
OPENSSL_init_crypto_internal(void)
{
	crypto_init_thread = pthread_self();
	OPENSSL_cpuid_setup();
	ERR_load_crypto_strings();
	OpenSSL_add_all_ciphers();
	OpenSSL_add_all_digests();
}

int
OPENSSL_init_crypto(uint64_t opts, const void *settings)
{
	static pthread_once_t once = PTHREAD_ONCE_INIT;

	if (pthread_equal(pthread_self(), crypto_init_thread))
		return 1; /* don't recurse */

	if (pthread_once(&once, OPENSSL_init_crypto_internal) != 0)
		return 0;

	if ((opts & OPENSSL_INIT_NO_LOAD_CONFIG) &&
	    (OpenSSL_no_config() == 0))
		return 0;

	if ((opts & OPENSSL_INIT_LOAD_CONFIG) &&
	    (OpenSSL_config(NULL) == 0))
		return 0;

	return 1;
}
