/*	$OpenBSD: tls13_lib.c,v 1.11 2019/03/17 15:13:23 jsing Exp $ */
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

static void
tls13_alert_received_cb(uint8_t alert_desc, void *arg)
{
	struct tls13_ctx *ctx = arg;
	SSL *s = ctx->ssl;

	if (alert_desc == SSL_AD_CLOSE_NOTIFY) {
		ctx->ssl->internal->shutdown |= SSL_RECEIVED_SHUTDOWN;
		S3I(ctx->ssl)->warn_alert = alert_desc;
		return;
	}

	if (alert_desc == SSL_AD_USER_CANCELLED) {
		/*
		 * We treat this as advisory, since a close_notify alert
		 * SHOULD follow this alert (RFC 8446 section 6.1).
		 */
		return;
	}

	/* All other alerts are treated as fatal in TLSv1.3. */
	S3I(ctx->ssl)->fatal_alert = alert_desc;

	SSLerror(ctx->ssl, SSL_AD_REASON_OFFSET + alert_desc);
	ERR_asprintf_error_data("SSL alert number %d", alert_desc);

	SSL_CTX_remove_session(s->ctx, s->session);
}

struct tls13_ctx *
tls13_ctx_new(int mode)
{
	struct tls13_ctx *ctx = NULL;

	if ((ctx = calloc(sizeof(struct tls13_ctx), 1)) == NULL)
		goto err;

	ctx->mode = mode;

	if ((ctx->rl = tls13_record_layer_new(tls13_legacy_wire_read_cb,
	    tls13_legacy_wire_write_cb, tls13_alert_received_cb, NULL,
	    ctx)) == NULL)
		goto err;

	return ctx;

 err:
	tls13_ctx_free(ctx);

	return NULL;
}

void
tls13_ctx_free(struct tls13_ctx *ctx)
{
	if (ctx == NULL)
		return;

	tls13_record_layer_free(ctx->rl);

	freezero(ctx, sizeof(struct tls13_ctx));
}

static ssize_t
tls13_legacy_wire_read(SSL *ssl, uint8_t *buf, size_t len)
{
	int n;

	if (ssl->rbio == NULL) {
		SSLerror(ssl, SSL_R_BIO_NOT_SET);
		return TLS13_IO_FAILURE;
	}

	ssl->internal->rwstate = SSL_READING;

	if ((n = BIO_read(ssl->rbio, buf, len)) <= 0) {
		if (BIO_should_read(ssl->rbio))
			return TLS13_IO_WANT_POLLIN;
		if (BIO_should_write(ssl->rbio))
			return TLS13_IO_WANT_POLLOUT;
		if (n == 0)
			return TLS13_IO_EOF;

		return TLS13_IO_FAILURE;
	}

	if (n == len)
		ssl->internal->rwstate = SSL_NOTHING;

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

	ssl->internal->rwstate = SSL_WRITING;

	if ((n = BIO_write(ssl->wbio, buf, len)) <= 0) {
		if (BIO_should_read(ssl->wbio))
			return TLS13_IO_WANT_POLLIN;
		if (BIO_should_write(ssl->wbio))
			return TLS13_IO_WANT_POLLOUT;

		return TLS13_IO_FAILURE;
	}

	if (n == len)
		ssl->internal->rwstate = SSL_NOTHING;

	return n;
}

ssize_t
tls13_legacy_wire_write_cb(const void *buf, size_t n, void *arg)
{
	struct tls13_ctx *ctx = arg;

	return tls13_legacy_wire_write(ctx->ssl, buf, n);
}

int
tls13_legacy_return_code(SSL *ssl, ssize_t ret)
{
	if (ret > INT_MAX) {
		SSLerror(ssl, ERR_R_INTERNAL_ERROR);
		return -1;
	}

	/* A successful read, write or other operation. */
	if (ret > 0)
		return ret;

	ssl->internal->rwstate = SSL_NOTHING;

	switch (ret) {
	case TLS13_IO_EOF:
		return 0;

	case TLS13_IO_FAILURE:
		/* XXX - we need to record/map internal errors. */
		if (ERR_peek_error() == 0)
			SSLerror(ssl, ERR_R_INTERNAL_ERROR);
		return -1;

	case TLS13_IO_WANT_POLLIN:
		BIO_set_retry_read(ssl->rbio);
		ssl->internal->rwstate = SSL_READING;
		return -1;

	case TLS13_IO_WANT_POLLOUT:
		BIO_set_retry_write(ssl->wbio);
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

	if (ctx == NULL || !ctx->handshake_completed) {
		if ((ret = ssl->internal->handshake_func(ssl)) <= 0)
			return ret;
		return tls13_legacy_return_code(ssl, TLS13_IO_WANT_POLLIN);
	}

	if (peek) {
		/* XXX - support peek... */
		SSLerror(ssl, ERR_R_INTERNAL_ERROR);
		return -1;
	}

	if (type != SSL3_RT_APPLICATION_DATA) {
		SSLerror(ssl, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
		return -1;
	}
	if (len < 0) {
		SSLerror(ssl, SSL_R_BAD_LENGTH); 
		return -1;
	}

	ret = tls13_read_application_data(ctx->rl, buf, len);
	return tls13_legacy_return_code(ssl, ret);
}

int
tls13_legacy_write_bytes(SSL *ssl, int type, const void *vbuf, int len)
{
	struct tls13_ctx *ctx = ssl->internal->tls13;
	const uint8_t *buf = vbuf;
	size_t n, sent;
	ssize_t ret;

	if (ctx == NULL || !ctx->handshake_completed) {
		if ((ret = ssl->internal->handshake_func(ssl)) <= 0)
			return ret;
		return tls13_legacy_return_code(ssl, TLS13_IO_WANT_POLLOUT);
	}

	if (type != SSL3_RT_APPLICATION_DATA) {
		SSLerror(ssl, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
		return -1;
	}
	if (len <= 0) {
		SSLerror(ssl, SSL_R_BAD_LENGTH); 
		return -1;
	}

	/*
	 * The TLSv1.3 record layer write behaviour is the same as
	 * SSL_MODE_ENABLE_PARTIAL_WRITE.
	 */
	if (ssl->internal->mode & SSL_MODE_ENABLE_PARTIAL_WRITE) {
		ret = tls13_write_application_data(ctx->rl, buf, len);
		return tls13_legacy_return_code(ssl, ret);
	}

	/*
 	 * In the non-SSL_MODE_ENABLE_PARTIAL_WRITE case we have to loop until
	 * we have written out all of the requested data.
	 */
	sent = S3I(ssl)->wnum;
	if (len < sent) {
		SSLerror(ssl, SSL_R_BAD_LENGTH); 
		return -1;
	}
	n = len - sent;
	for (;;) {
		if (n == 0) {
			S3I(ssl)->wnum = 0;
			return sent;
		}
		if ((ret = tls13_write_application_data(ctx->rl,
		    &buf[sent], n)) <= 0) {
			S3I(ssl)->wnum = sent;
			return tls13_legacy_return_code(ssl, ret);
		}
		sent += ret;
		n -= ret;
	}
}
