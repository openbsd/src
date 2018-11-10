/* $OpenBSD: ssl_sigalgs.c,v 1.4 2018/11/10 01:19:09 beck Exp $ */
/*
 * Copyright (c) 2018, Bob Beck <beck@openbsd.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <string.h>
#include <stdlib.h>

#include <openssl/evp.h>

#include "bytestring.h"
#include "ssl_locl.h"
#include "ssl_sigalgs.h"
#include "tls13_internal.h"

const struct ssl_sigalg sigalgs[] = {
	{
		.value = SIGALG_RSA_PKCS1_SHA512,
		.md = EVP_sha512,
		.key_type = EVP_PKEY_RSA,
		.pkey_idx = SSL_PKEY_RSA_SIGN,
	},
	{
		.value = SIGALG_ECDSA_SECP512R1_SHA512,
		.md = EVP_sha512,
		.key_type = EVP_PKEY_EC,
		.pkey_idx = SSL_PKEY_ECC,
	},
#ifndef OPENSSL_NO_GOST
	{
		.value = SIGALG_GOSTR12_512_STREEBOG_512,
		.md = EVP_streebog512,
		.key_type = EVP_PKEY_GOSTR12_512,
		.pkey_idx = SSL_PKEY_GOST01, /* XXX */
	},
#endif
	{
		.value = SIGALG_RSA_PKCS1_SHA384,
		.md = EVP_sha384,
		.key_type = EVP_PKEY_RSA,
		.pkey_idx = SSL_PKEY_RSA_SIGN,
	},
	{
		.value = SIGALG_ECDSA_SECP384R1_SHA384,
		.md = EVP_sha384,
		.key_type = EVP_PKEY_EC,
		.pkey_idx = SSL_PKEY_ECC,
	},
	{
		.value = SIGALG_RSA_PKCS1_SHA256,
		.md = EVP_sha256,
		.key_type = EVP_PKEY_RSA,
		.pkey_idx = SSL_PKEY_RSA_SIGN,
	},
	{
		.value = SIGALG_ECDSA_SECP256R1_SHA256,
		.md = EVP_sha256,
		.key_type = EVP_PKEY_EC,
		.pkey_idx = SSL_PKEY_ECC,
	},
#ifndef OPENSSL_NO_GOST
	{
		.value = SIGALG_GOSTR12_256_STREEBOG_256,
		.md = EVP_streebog256,
		.key_type = EVP_PKEY_GOSTR12_256,
		.pkey_idx = SSL_PKEY_GOST01, /* XXX */
	},
	{
		.value = SIGALG_GOSTR01_GOST94,
		.md = EVP_gostr341194,
		.key_type = EVP_PKEY_GOSTR01,
		.pkey_idx = SSL_PKEY_GOST01,
	},
#endif
#ifdef LIBRESSL_HAS_TLS1_3
	{
		.value = SIGALG_RSA_PSS_RSAE_SHA256,
		.md = EVP_sha256,
		.key_type = EVP_PKEY_RSA,
		.pkey_idx = SSL_PKEY_RSA_SIGN,
		.flags = SIGALG_FLAG_RSA_PSS,
	},
	{
		.value = SIGALG_RSA_PSS_RSAE_SHA384,
		.md = EVP_sha384,
		.key_type = EVP_PKEY_RSA,
		.pkey_idx = SSL_PKEY_RSA_SIGN,
		.flags = SIGALG_FLAG_RSA_PSS,
	},
	{
		.value = SIGALG_RSA_PSS_RSAE_SHA512,
		.md = EVP_sha512,
		.key_type = EVP_PKEY_RSA,
		.pkey_idx = SSL_PKEY_RSA_SIGN,
		.flags = SIGALG_FLAG_RSA_PSS,
	},
	{
		.value = SIGALG_RSA_PSS_PSS_SHA256,
		.md = EVP_sha256,
		.key_type = EVP_PKEY_RSA,
		.pkey_idx = SSL_PKEY_RSA_SIGN,
		.flags = SIGALG_FLAG_RSA_PSS,
	},
	{
		.value = SIGALG_RSA_PSS_PSS_SHA384,
		.md = EVP_sha384,
		.key_type = EVP_PKEY_RSA,
		.pkey_idx = SSL_PKEY_RSA_SIGN,
		.flags = SIGALG_FLAG_RSA_PSS,
	},
	{
		.value = SIGALG_RSA_PSS_PSS_SHA512,
		.md = EVP_sha512,
		.key_type = EVP_PKEY_RSA,
		.pkey_idx = SSL_PKEY_RSA_SIGN,
		.flags = SIGALG_FLAG_RSA_PSS,
	},
#endif
	{
		.value = SIGALG_RSA_PKCS1_SHA224,
		.md = EVP_sha224,
		.key_type = EVP_PKEY_RSA,
		.pkey_idx = SSL_PKEY_RSA_SIGN,
	},
	{
		.value = SIGALG_ECDSA_SECP224R1_SHA224,
		.md = EVP_sha224,
		.key_type = EVP_PKEY_EC,
		.pkey_idx = SSL_PKEY_ECC,
	},
	{
		.value = SIGALG_RSA_PKCS1_SHA1,
		.key_type = EVP_PKEY_RSA,
		.pkey_idx = SSL_PKEY_RSA_SIGN,
		.md = EVP_md5_sha1,
	},
	{
		.value = SIGALG_ECDSA_SHA1,
		.key_type = EVP_PKEY_EC,
		.md = EVP_sha1,
		.pkey_idx = SSL_PKEY_ECC,
	},
	{
		.value = SIGALG_NONE,
	},
};

/* Sigalgs for tls 1.2, in preference order, */
uint16_t tls12_sigalgs[] = {
	SIGALG_RSA_PKCS1_SHA512,
	SIGALG_ECDSA_SECP512R1_SHA512,
	SIGALG_GOSTR12_512_STREEBOG_512,
	SIGALG_RSA_PKCS1_SHA384,
	SIGALG_ECDSA_SECP384R1_SHA384,
	SIGALG_RSA_PKCS1_SHA256,
	SIGALG_ECDSA_SECP256R1_SHA256,
	SIGALG_GOSTR12_256_STREEBOG_256,
	SIGALG_GOSTR01_GOST94,
	SIGALG_RSA_PKCS1_SHA224,
	SIGALG_ECDSA_SECP224R1_SHA224,
	SIGALG_RSA_PKCS1_SHA1, /* XXX */
	SIGALG_ECDSA_SHA1,     /* XXX */
};
size_t tls12_sigalgs_len = (sizeof(tls12_sigalgs) / sizeof(tls12_sigalgs[0]));

const struct ssl_sigalg *
ssl_sigalg_lookup(uint16_t sigalg)
{
	int i;

	for (i = 0; sigalgs[i].value != SIGALG_NONE; i++) {
		if (sigalgs[i].value == sigalg)
			return &sigalgs[i];
	}

	return NULL;
}

const struct ssl_sigalg *
ssl_sigalg(uint16_t sigalg, uint16_t *values, size_t len)
{
	const struct ssl_sigalg *sap;
	int i;

	for (i = 0; i < len; i++) {
		if (values[i] == sigalg)
			break;
	}
	if (values[i] == sigalg) {
		if ((sap = ssl_sigalg_lookup(sigalg)) != NULL)
			return sap;
	}

	return NULL;
}

uint16_t
ssl_sigalg_value(const EVP_PKEY *pk, const EVP_MD *md)
{
	int i;

	for (i = 0; sigalgs[i].value != SIGALG_NONE; i++) {
		if ((sigalgs[i].key_type == pk->type) &&
		    ((sigalgs[i].md() == md)))
			return sigalgs[i].value;
	}
	return SIGALG_NONE;
}

int
ssl_sigalgs_build(CBB *cbb, uint16_t *values, size_t len)
{
	const struct ssl_sigalg *sap;
	size_t i;

	for (i = 0; sigalgs[i].value != SIGALG_NONE; i++);
	if (len > i)
		return 0;

	/* XXX check for duplicates and other sanity BS? */

	/* Add values in order as long as they are supported. */
	for (i = 0; i < len; i++) {
		if ((sap = ssl_sigalg_lookup(values[i])) != NULL) {
			if (!CBB_add_u16(cbb, values[i]))
				return 0;
		} else
			return 0;
	}
	return 1;
}
