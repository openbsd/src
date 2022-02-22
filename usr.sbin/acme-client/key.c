/*	$Id: key.c,v 1.6 2022/02/22 13:45:09 tb Exp $ */
/*
 * Copyright (c) 2019 Renaud Allard <renaud@allard.it>
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
#include <openssl/ecdsa.h>
#include <openssl/ec.h>
#include <openssl/obj_mac.h>

#include "key.h"

/*
 * Default number of bits when creating a new RSA key.
 */
#define	KBITS 4096
#define ECCTYPE NID_secp384r1

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
ec_key_create(FILE *f, const char *fname)
{
	EC_KEY		*eckey = NULL;
	EVP_PKEY	*pkey = NULL;

	if ((eckey = EC_KEY_new_by_curve_name(ECCTYPE)) == NULL ) {
		warnx("EC_KEY_new_by_curve_name");
		goto err;
	}

	if (!EC_KEY_generate_key(eckey)) {
		warnx("EC_KEY_generate_key");
		goto err;
	}

	/* set OPENSSL_EC_NAMED_CURVE to be able to load the key */

	EC_KEY_set_asn1_flag(eckey, OPENSSL_EC_NAMED_CURVE);

	/* Serialise the key to the disc in EC format */

	if (!PEM_write_ECPrivateKey(f, eckey, NULL, NULL, 0, NULL, NULL)) {
		warnx("%s: PEM_write_ECPrivateKey", fname);
		goto err;
	}

	/* Convert the EC key into a PKEY structure */

	if ((pkey = EVP_PKEY_new()) == NULL) {
		warnx("EVP_PKEY_new");
		goto err;
	}
	if (!EVP_PKEY_set1_EC_KEY(pkey, eckey)) {
		warnx("EVP_PKEY_assign_EC_KEY");
		goto err;
	}

	goto out;

err:
	EVP_PKEY_free(pkey);
	pkey = NULL;
out:
	EC_KEY_free(eckey);
	return pkey;
}



EVP_PKEY *
key_load(FILE *f, const char *fname)
{
	EVP_PKEY	*pkey;

	pkey = PEM_read_PrivateKey(f, NULL, NULL, NULL);
	if (pkey == NULL) {
		warnx("%s: PEM_read_PrivateKey", fname);
		return NULL;
	}
	if (EVP_PKEY_base_id(pkey) == EVP_PKEY_RSA ||
	    EVP_PKEY_base_id(pkey) == EVP_PKEY_EC)
		return pkey;

	warnx("%s: unsupported key type", fname);
	EVP_PKEY_free(pkey);
	return NULL;
}
