/*	$OpenBSD: tls13_lib.c,v 1.1 2019/01/21 09:10:58 jsing Exp $ */
/*
 * Copyright (c) 2018, 2019 Joel Sing <jsing@openbsd.org>
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

#include <limits.h>
#include <stddef.h>

#include <openssl/evp.h>

#include "ssl_locl.h"
#include "tls13_internal.h"

const EVP_AEAD *
tls13_cipher_aead(const SSL_CIPHER *cipher)
{
	if (cipher == NULL)
		return NULL;
	if (cipher->algorithm_ssl != SSL_TLSV1_3)
		return NULL;

	switch (cipher->algorithm_enc) {
	case SSL_AES128GCM:
		return EVP_aead_aes_128_gcm();
	case SSL_AES256GCM:
		return EVP_aead_aes_256_gcm();
	case SSL_CHACHA20POLY1305:
		return EVP_aead_chacha20_poly1305();
	}

	return NULL;
}

const EVP_MD *
tls13_cipher_hash(const SSL_CIPHER *cipher)
{
	if (cipher == NULL)
		return NULL;
	if (cipher->algorithm_ssl != SSL_TLSV1_3)
		return NULL;

	switch (cipher->algorithm2) {
	case SSL_HANDSHAKE_MAC_SHA256:
		return EVP_sha256();
	case SSL_HANDSHAKE_MAC_SHA384:
		return EVP_sha384();
	}

	return NULL;
}

static ssize_t
tls13_legacy_wire_read(SSL *ssl, uint8_t *buf, size_t len)
{
	int n;

	if (ssl->rbio == NULL) {
		SSLerror(ssl, SSL_R_BIO_NOT_SET);
		return TLS13_IO_FAILURE;
	}

	if ((n = BIO_read(ssl->rbio, buf, len)) <= 0) {
		if (BIO_should_read(ssl->rbio))
			return TLS13_IO_WANT_POLLIN;
		if (BIO_should_write(ssl->rbio))
			return TLS13_IO_WANT_POLLOUT;

		return TLS13_IO_FAILURE;
	}

	return n;
}

ssize_t
tls13_legacy_wire_read_cb(void *buf, size_t n, void *arg)
{
	struct tls13_ctx *ctx = arg;

	return tls13_legacy_wire_read(ctx->ssl, buf, n);
}

static ssize_t
tls13_legacy_wire_write(SSL *ssl, const uint8_t *buf, size_t len)
{
	int n;

	if (ssl->wbio == NULL) {
		SSLerror(ssl, SSL_R_BIO_NOT_SET);
		return TLS13_IO_FAILURE;
	}

	if ((n = BIO_write(ssl->wbio, buf, len)) <= 0) {
		if (BIO_should_read(ssl->wbio))
			return TLS13_IO_WANT_POLLIN;
		if (BIO_should_write(ssl->wbio))
			return TLS13_IO_WANT_POLLOUT;

		return TLS13_IO_FAILURE;
	}

	return n;
}

ssize_t
tls13_legacy_wire_write_cb(const void *buf, size_t n, void *arg)
{
	struct tls13_ctx *ctx = arg;

	return tls13_legacy_wire_write(ctx->ssl, buf, n);
}

static int
tls13_legacy_return_code(SSL *ssl, ssize_t ret)
{
	ssl->internal->rwstate = SSL_NOTHING;

	if (ret > INT_MAX) {
		SSLerror(ssl, ERR_R_INTERNAL_ERROR);
		return -1;
	}

	/* A successful read or write. */
	if (ret > 0)
		return ret;

	switch (ret) {
	case TLS13_IO_EOF:
		return 0;

	case TLS13_IO_FAILURE:
		/* XXX - we need to record/map internal errors. */
		if (ERR_peek_error() == 0)
			SSLerror(ssl, ERR_R_INTERNAL_ERROR);
		return -1;

	case TLS13_IO_WANT_POLLIN:
		ssl->internal->rwstate = SSL_READING;
		return -1;

	case TLS13_IO_WANT_POLLOUT:
		ssl->internal->rwstate = SSL_WRITING;
		return -1;
	}

	SSLerror(ssl, ERR_R_INTERNAL_ERROR);
	return -1;
}

int
tls13_legacy_read_bytes(SSL *ssl, int type, unsigned char *buf, int len, int peek)
{
	struct tls13_ctx *ctx = ssl->internal->tls13;
	ssize_t ret;

	if (peek) {
		/* XXX - support peek... */
		SSLerror(ssl, ERR_R_INTERNAL_ERROR);
		return -1;
	}

	if (type != SSL3_RT_APPLICATION_DATA) {
		SSLerror(ssl, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
		return -1;
	}

	ret = tls13_read_application_data(ctx->rl, buf, len);

	return tls13_legacy_return_code(ssl, ret);
}

int
tls13_legacy_write_bytes(SSL *ssl, int type, const void *buf, int len)
{
	struct tls13_ctx *ctx = ssl->internal->tls13;
	ssize_t ret;

	if (type != SSL3_RT_APPLICATION_DATA) {
		SSLerror(ssl, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
		return -1;
	}

	ret = tls13_write_application_data(ctx->rl, buf, len);

	return tls13_legacy_return_code(ssl, ret);
}
