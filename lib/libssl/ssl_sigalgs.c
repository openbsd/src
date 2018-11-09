/* $OpenBSD: ssl_sigalgs.c,v 1.1 2018/11/09 00:34:55 beck Exp $ */
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

/* This table must be kept in preference order for now */
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
		.md = EVP_sha1,
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

const EVP_MD *
ssl_sigalg_md(uint16_t sigalg)
{
	const struct ssl_sigalg *sap;

	if ((sap = ssl_sigalg_lookup(sigalg)) != NULL)
		return sap->md();

	return NULL;
}

int
ssl_sigalg_pkey_check(uint16_t sigalg, EVP_PKEY *pk)
{
	const struct ssl_sigalg *sap;

	if ((sap = ssl_sigalg_lookup(sigalg)) != NULL)
		return sap->key_type == pk->type;

	return 0;
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
ssl_sigalgs_build(CBB *cbb)
{
	int i;

	for (i = 0; sigalgs[i].value != SIGALG_NONE; i++) {
		if (!CBB_add_u16(cbb, sigalgs[i].value))
			return 0;
	}
	return 1;
}
