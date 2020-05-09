/* $OpenBSD: ssl_sigalgs.c,v 1.21 2020/05/09 16:52:15 beck Exp $ */
/*
 * Copyright (c) 2018-2020 Bob Beck <beck@openbsd.org>
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
	},
	{
		.value = SIGALG_ECDSA_SECP521R1_SHA512,
		.md = EVP_sha512,
		.key_type = EVP_PKEY_EC,
		.curve_nid = NID_secp521r1,
	},
#ifndef OPENSSL_NO_GOST
	{
		.value = SIGALG_GOSTR12_512_STREEBOG_512,
		.md = EVP_streebog512,
		.key_type = EVP_PKEY_GOSTR12_512,
	},
#endif
	{
		.value = SIGALG_RSA_PKCS1_SHA384,
		.md = EVP_sha384,
		.key_type = EVP_PKEY_RSA,
	},
	{
		.value = SIGALG_ECDSA_SECP384R1_SHA384,
		.md = EVP_sha384,
		.key_type = EVP_PKEY_EC,
		.curve_nid = NID_secp384r1,
	},
	{
		.value = SIGALG_RSA_PKCS1_SHA256,
		.md = EVP_sha256,
		.key_type = EVP_PKEY_RSA,
	},
	{
		.value = SIGALG_ECDSA_SECP256R1_SHA256,
		.md = EVP_sha256,
		.key_type = EVP_PKEY_EC,
		.curve_nid = NID_X9_62_prime256v1,
	},
#ifndef OPENSSL_NO_GOST
	{
		.value = SIGALG_GOSTR12_256_STREEBOG_256,
		.md = EVP_streebog256,
		.key_type = EVP_PKEY_GOSTR12_256,
	},
	{
		.value = SIGALG_GOSTR01_GOST94,
		.md = EVP_gostr341194,
		.key_type = EVP_PKEY_GOSTR01,
	},
#endif
	{
		.value = SIGALG_RSA_PSS_RSAE_SHA256,
		.md = EVP_sha256,
		.key_type = EVP_PKEY_RSA,
		.flags = SIGALG_FLAG_RSA_PSS,
	},
	{
		.value = SIGALG_RSA_PSS_RSAE_SHA384,
		.md = EVP_sha384,
		.key_type = EVP_PKEY_RSA,
		.flags = SIGALG_FLAG_RSA_PSS,
	},
	{
		.value = SIGALG_RSA_PSS_RSAE_SHA512,
		.md = EVP_sha512,
		.key_type = EVP_PKEY_RSA,
		.flags = SIGALG_FLAG_RSA_PSS,
	},
	{
		.value = SIGALG_RSA_PSS_PSS_SHA256,
		.md = EVP_sha256,
		.key_type = EVP_PKEY_RSA,
		.flags = SIGALG_FLAG_RSA_PSS,
	},
	{
		.value = SIGALG_RSA_PSS_PSS_SHA384,
		.md = EVP_sha384,
		.key_type = EVP_PKEY_RSA,
		.flags = SIGALG_FLAG_RSA_PSS,
	},
	{
		.value = SIGALG_RSA_PSS_PSS_SHA512,
		.md = EVP_sha512,
		.key_type = EVP_PKEY_RSA,
		.flags = SIGALG_FLAG_RSA_PSS,
	},
	{
		.value = SIGALG_RSA_PKCS1_SHA224,
		.md = EVP_sha224,
		.key_type = EVP_PKEY_RSA,
	},
	{
		.value = SIGALG_ECDSA_SECP224R1_SHA224,
		.md = EVP_sha224,
		.key_type = EVP_PKEY_EC,
	},
	{
		.value = SIGALG_RSA_PKCS1_SHA1,
		.key_type = EVP_PKEY_RSA,
		.md = EVP_sha1,
	},
	{
		.value = SIGALG_ECDSA_SHA1,
		.key_type = EVP_PKEY_EC,
		.md = EVP_sha1,
	},
	{
		.value = SIGALG_RSA_PKCS1_MD5_SHA1,
		.key_type = EVP_PKEY_RSA,
		.md = EVP_md5_sha1,
	},
	{
		.value = SIGALG_NONE,
	},
};

/* Sigalgs for tls 1.3, in preference order, */
uint16_t tls13_sigalgs[] = {
	SIGALG_RSA_PSS_RSAE_SHA512,
	SIGALG_RSA_PKCS1_SHA512,
	SIGALG_ECDSA_SECP521R1_SHA512,
	SIGALG_RSA_PSS_RSAE_SHA384,
	SIGALG_RSA_PKCS1_SHA384,
	SIGALG_ECDSA_SECP384R1_SHA384,
	SIGALG_RSA_PSS_RSAE_SHA256,
	SIGALG_RSA_PKCS1_SHA256,
	SIGALG_ECDSA_SECP256R1_SHA256,
};
size_t tls13_sigalgs_len = (sizeof(tls13_sigalgs) / sizeof(tls13_sigalgs[0]));

/* Sigalgs for tls 1.2, in preference order, */
uint16_t tls12_sigalgs[] = {
	SIGALG_RSA_PSS_RSAE_SHA512,
	SIGALG_RSA_PKCS1_SHA512,
	SIGALG_ECDSA_SECP521R1_SHA512,
	SIGALG_RSA_PSS_RSAE_SHA384,
	SIGALG_RSA_PKCS1_SHA384,
	SIGALG_ECDSA_SECP384R1_SHA384,
	SIGALG_RSA_PSS_RSAE_SHA256,
	SIGALG_RSA_PKCS1_SHA256,
	SIGALG_ECDSA_SECP256R1_SHA256,
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
	int i;

	for (i = 0; i < len; i++) {
		if (values[i] == sigalg)
			return ssl_sigalg_lookup(sigalg);
	}

	return NULL;
}

int
ssl_sigalgs_build(CBB *cbb, uint16_t *values, size_t len)
{
	size_t i;

	for (i = 0; sigalgs[i].value != SIGALG_NONE; i++);
	if (len > i)
		return 0;

	/* XXX check for duplicates and other sanity BS? */

	/* Add values in order as long as they are supported. */
	for (i = 0; i < len; i++) {
		/* Do not allow the legacy value for < 1.2 to be used */
		if (values[i] == SIGALG_RSA_PKCS1_MD5_SHA1)
			return 0;

		if (ssl_sigalg_lookup(values[i]) != NULL) {
			if (!CBB_add_u16(cbb, values[i]))
				return 0;
		} else
			return 0;
	}
	return 1;
}

int
ssl_sigalg_pkey_ok(const struct ssl_sigalg *sigalg, EVP_PKEY *pkey,
    int check_curve)
{
	if (sigalg == NULL || pkey == NULL)
		return 0;
	if (sigalg->key_type != pkey->type)
		return 0;

	if ((sigalg->flags & SIGALG_FLAG_RSA_PSS)) {
		/*
		 * RSA PSS Must have an RSA key that needs to be at
		 * least as big as twice the size of the hash + 2
		 */
		if (pkey->type != EVP_PKEY_RSA ||
		    EVP_PKEY_size(pkey) < (2 * EVP_MD_size(sigalg->md()) + 2))
			return 0;
	}

	if (pkey->type == EVP_PKEY_EC && check_curve) {
		/* Curve must match for EC keys. */
		if (sigalg->curve_nid == 0)
			return 0;
		if (EC_GROUP_get_curve_name(EC_KEY_get0_group
		    (EVP_PKEY_get0_EC_KEY(pkey))) != sigalg->curve_nid) {
			return 0;
		}
	}

	return 1;
}

const struct ssl_sigalg *
ssl_sigalg_select(SSL *s, EVP_PKEY *pkey)
{
	uint16_t *tls_sigalgs = tls12_sigalgs;
	size_t tls_sigalgs_len = tls12_sigalgs_len;
	int check_curve = 0;
	CBS cbs;

	if (TLS1_get_version(s) >= TLS1_3_VERSION) {
		tls_sigalgs = tls13_sigalgs;
		tls_sigalgs_len = tls13_sigalgs_len;
		check_curve = 1;
	}

	/* Pre TLS 1.2 defaults */
	if (!SSL_USE_SIGALGS(s)) {
		switch (pkey->type) {
		case EVP_PKEY_RSA:
			return ssl_sigalg_lookup(SIGALG_RSA_PKCS1_MD5_SHA1);
		case EVP_PKEY_EC:
			return ssl_sigalg_lookup(SIGALG_ECDSA_SHA1);
#ifndef OPENSSL_NO_GOST
		case EVP_PKEY_GOSTR01:
			return ssl_sigalg_lookup(SIGALG_GOSTR01_GOST94);
#endif
		}
		SSLerror(s, SSL_R_UNKNOWN_PKEY_TYPE);
		return (NULL);
	}

	/*
	 * RFC 5246 allows a TLS 1.2 client to send no sigalgs, in
	 * which case the server must use the the default.
	 */
	if (TLS1_get_version(s) < TLS1_3_VERSION &&
	    S3I(s)->hs.sigalgs == NULL) {
		switch (pkey->type) {
		case EVP_PKEY_RSA:
			return ssl_sigalg_lookup(SIGALG_RSA_PKCS1_SHA1);
		case EVP_PKEY_EC:
			return ssl_sigalg_lookup(SIGALG_ECDSA_SHA1);
#ifndef OPENSSL_NO_GOST
		case EVP_PKEY_GOSTR01:
			return ssl_sigalg_lookup(SIGALG_GOSTR01_GOST94);
#endif
		}
		SSLerror(s, SSL_R_UNKNOWN_PKEY_TYPE);
		return (NULL);
	}

	/*
	 * If we get here, we have client or server sent sigalgs, use one.
	 */
	CBS_init(&cbs, S3I(s)->hs.sigalgs, S3I(s)->hs.sigalgs_len);
	while (CBS_len(&cbs) > 0) {
		uint16_t sig_alg;
		const struct ssl_sigalg *sigalg;

		if (!CBS_get_u16(&cbs, &sig_alg))
			return 0;

		if ((sigalg = ssl_sigalg(sig_alg, tls_sigalgs,
		    tls_sigalgs_len)) == NULL)
			continue;

		/* RSA cannot be used without PSS in TLSv1.3. */
		if (TLS1_get_version(s) >= TLS1_3_VERSION &&
		    sigalg->key_type == EVP_PKEY_RSA &&
		    (sigalg->flags & SIGALG_FLAG_RSA_PSS) == 0)
			continue;

		if (ssl_sigalg_pkey_ok(sigalg, pkey, check_curve))
			return sigalg;
	}

	SSLerror(s, SSL_R_UNKNOWN_PKEY_TYPE);
	return NULL;
}
