/*	$OpenBSD: crypto_init.c,v 1.8 2023/05/08 13:53:26 tb Exp $ */
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

#include <openssl/conf.h>
#include <openssl/engine.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/objects.h>

#include "cryptlib.h"
#include "x509_issuer_cache.h"

int OpenSSL_config(const char *);
int OpenSSL_no_config(void);

static pthread_once_t crypto_init_once = PTHREAD_ONCE_INIT;
static pthread_t crypto_init_thread;
static int crypto_init_cleaned_up;

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
	if (crypto_init_cleaned_up) {
		CRYPTOerror(ERR_R_INIT_FAIL);
		return 0;
	}

	if (pthread_equal(pthread_self(), crypto_init_thread))
		return 1; /* don't recurse */

	if (pthread_once(&crypto_init_once, OPENSSL_init_crypto_internal) != 0)
		return 0;

	if ((opts & OPENSSL_INIT_NO_LOAD_CONFIG) &&
	    (OpenSSL_no_config() == 0))
		return 0;

	if ((opts & OPENSSL_INIT_LOAD_CONFIG) &&
	    (OpenSSL_config(NULL) == 0))
		return 0;

	return 1;
}

void
OPENSSL_cleanup(void)
{
	/* This currently calls init... */
	ERR_free_strings();

	CRYPTO_cleanup_all_ex_data();
	ENGINE_cleanup();
	EVP_cleanup();
	x509_issuer_cache_free();

	crypto_init_cleaned_up = 1;
}
