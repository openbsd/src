/*	$OpenBSD: ssl_seclevel.c,v 1.5 2022/06/28 20:54:16 tb Exp $ */
/*
 * Copyright (c) 2020 Theo Buehler <tb@openbsd.org>
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

#include <stddef.h>

#include <openssl/ossl_typ.h>
#include <openssl/ssl.h>
#include <openssl/tls1.h>

#include "ssl_locl.h"

static int
ssl_security_normalize_level(const SSL_CTX *ctx, const SSL *ssl, int *out_level)
{
	int security_level;

	if (ctx != NULL)
		security_level = SSL_CTX_get_security_level(ctx);
	else
		security_level = SSL_get_security_level(ssl);

	if (security_level < 0)
		security_level = 0;
	if (security_level > 5)
		security_level = 5;

	*out_level = security_level;

	return 1;
}

static int
ssl_security_level_to_minimum_bits(int security_level, int *out_minimum_bits)
{
	if (security_level < 0)
		return 0;

	if (security_level == 0)
		*out_minimum_bits = 0;
	else if (security_level == 1)
		*out_minimum_bits = 80;
	else if (security_level == 2)
		*out_minimum_bits = 112;
	else if (security_level == 3)
		*out_minimum_bits = 128;
	else if (security_level == 4)
		*out_minimum_bits = 192;
	else if (security_level >= 5)
		*out_minimum_bits = 256;

	return 1;
}

static int
ssl_security_level_and_minimum_bits(const SSL_CTX *ctx, const SSL *ssl,
    int *out_level, int *out_minimum_bits)
{
	int security_level = 0, minimum_bits = 0;

	if (!ssl_security_normalize_level(ctx, ssl, &security_level))
		return 0;
	if (!ssl_security_level_to_minimum_bits(security_level, &minimum_bits))
		return 0;

	if (out_level != NULL)
		*out_level = security_level;
	if (out_minimum_bits != NULL)
		*out_minimum_bits = minimum_bits;

	return 1;
}

static int
ssl_security_secop_cipher(const SSL_CTX *ctx, const SSL *ssl, int bits,
    void *arg)
{
	const SSL_CIPHER *cipher = arg;
	int security_level, minimum_bits;

	if (!ssl_security_level_and_minimum_bits(ctx, ssl, &security_level,
	    &minimum_bits))
		return 0;

	if (security_level <= 0)
		return 1;

	if (bits < minimum_bits)
		return 0;

	/* No unauthenticated ciphersuites. */
	if (cipher->algorithm_auth & SSL_aNULL)
		return 0;

	if (security_level <= 1)
		return 1;

	if (cipher->algorithm_enc == SSL_RC4)
		return 0;

	if (security_level <= 2)
		return 1;

	/* Security level >= 3 requires a cipher with forward secrecy. */
	if ((cipher->algorithm_mkey & (SSL_kDHE | SSL_kECDHE)) == 0 &&
	    cipher->algorithm_ssl != SSL_TLSV1_3)
		return 0;

	return 1;
}

static int
ssl_security_secop_version(const SSL_CTX *ctx, const SSL *ssl, int version)
{
	int min_version = TLS1_2_VERSION;
	int security_level;

	if (!ssl_security_level_and_minimum_bits(ctx, ssl, &security_level, NULL))
		return 0;

	if (security_level < 4)
		min_version = TLS1_1_VERSION;
	if (security_level < 3)
		min_version = TLS1_VERSION;

	return ssl_tls_version(version) >= min_version;
}

static int
ssl_security_secop_compression(const SSL_CTX *ctx, const SSL *ssl)
{
	return 0;
}

static int
ssl_security_secop_tickets(const SSL_CTX *ctx, const SSL *ssl)
{
	int security_level;

	if (!ssl_security_level_and_minimum_bits(ctx, ssl, &security_level, NULL))
		return 0;

	return security_level < 3;
}

static int
ssl_security_secop_tmp_dh(const SSL_CTX *ctx, const SSL *ssl, int bits)
{
	int security_level, minimum_bits;

	if (!ssl_security_level_and_minimum_bits(ctx, ssl, &security_level,
	    &minimum_bits))
		return 0;

	/* Disallow DHE keys weaker than 1024 bits even at security level 0. */
	if (security_level <= 0 && bits < 80)
		return 0;

	return bits >= minimum_bits;
}

static int
ssl_security_secop_default(const SSL_CTX *ctx, const SSL *ssl, int bits)
{
	int minimum_bits;

	if (!ssl_security_level_and_minimum_bits(ctx, ssl, NULL, &minimum_bits))
		return 0;

	return bits >= minimum_bits;
}

int
ssl_security_default_cb(const SSL *ssl, const SSL_CTX *ctx, int op, int bits,
    int version, void *cipher, void *ex_data)
{
	switch (op) {
	case SSL_SECOP_CIPHER_SUPPORTED:
	case SSL_SECOP_CIPHER_SHARED:
	case SSL_SECOP_CIPHER_CHECK:
		return ssl_security_secop_cipher(ctx, ssl, bits, cipher);
	case SSL_SECOP_VERSION:
		return ssl_security_secop_version(ctx, ssl, version);
	case SSL_SECOP_COMPRESSION:
		return ssl_security_secop_compression(ctx, ssl);
	case SSL_SECOP_TICKET:
		return ssl_security_secop_tickets(ctx, ssl);
	case SSL_SECOP_TMP_DH:
		return ssl_security_secop_tmp_dh(ctx, ssl, bits);
	default:
		return ssl_security_secop_default(ctx, ssl, bits);
	}
}

int
ssl_security_dummy_cb(const SSL *ssl, const SSL_CTX *ctx, int op, int bits,
    int version, void *cipher, void *ex_data)
{
	return 1;
}

int
ssl_ctx_security(const SSL_CTX *ctx, int op, int bits, int nid, void *other)
{
	return ctx->internal->cert->security_cb(NULL, ctx, op, bits, nid, other,
	    ctx->internal->cert->security_ex_data);
}

int
ssl_security(const SSL *ssl, int op, int bits, int nid, void *other)
{
	return ssl->cert->security_cb(ssl, NULL, op, bits, nid, other,
	    ssl->cert->security_ex_data);
}
