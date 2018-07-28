/*	$Id: rsa.c,v 1.7 2018/07/28 15:25:23 tb Exp $ */
/*
 * Copyright (c) 2016 Kristaps Dzonsons <kristaps@bsd.lv>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHORS DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <err.h>
#include <stdlib.h>
#include <unistd.h>

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

#include "rsa.h"

/*
 * Default number of bits when creating a new key.
 */
#define	KBITS 4096

/*
 * Create an RSA key with the default KBITS number of bits.
 */
EVP_PKEY *
rsa_key_create(FILE *f, const char *fname)
{
	EVP_PKEY_CTX	*ctx = NULL;
	EVP_PKEY	*pkey = NULL;

	/* First, create the context and the key. */

	if ((ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL)) == NULL) {
		warnx("EVP_PKEY_CTX_new_id");
		goto err;
	} else if (EVP_PKEY_keygen_init(ctx) <= 0) {
		warnx("EVP_PKEY_keygen_init");
		goto err;
	} else if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, KBITS) <= 0) {
		warnx("EVP_PKEY_set_rsa_keygen_bits");
		goto err;
	} else if (EVP_PKEY_keygen(ctx, &pkey) <= 0) {
		warnx("EVP_PKEY_keygen");
		goto err;
	}

	/* Serialise the key to the disc. */

	if (PEM_write_PrivateKey(f, pkey, NULL, NULL, 0, NULL, NULL))
		goto out;

	warnx("%s: PEM_write_PrivateKey", fname);
err:
	EVP_PKEY_free(pkey);
	pkey = NULL;
out:
	EVP_PKEY_CTX_free(ctx);
	return pkey;
}


EVP_PKEY *
rsa_key_load(FILE *f, const char *fname)
{
	EVP_PKEY	*pkey;

	pkey = PEM_read_PrivateKey(f, NULL, NULL, NULL);
	if (pkey == NULL) {
		warnx("%s: PEM_read_PrivateKey", fname);
		return NULL;
	} else if (EVP_PKEY_type(pkey->type) == EVP_PKEY_RSA)
		return pkey;

	warnx("%s: unsupported key type", fname);
	EVP_PKEY_free(pkey);
	return NULL;
}
