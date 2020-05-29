/*	$OpenBSD: tls13_legacy.c,v 1.8 2020/05/29 17:47:30 jsing Exp $ */
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

#include "ssl_locl.h"
#include "tls13_internal.h"

SSL3_ENC_METHOD TLSv1_3_enc_data = {
	.enc_flags = SSL_ENC_FLAG_SIGALGS|SSL_ENC_FLAG_TLS1_3_CIPHERS,
};

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

static void
tls13_legacy_error(SSL *ssl)
{
	struct tls13_ctx *ctx = ssl->internal->tls13;
	int reason = SSL_R_UNKNOWN;

	/* If we received a fatal alert we already put an error on the stack. */
	if (S3I(ssl)->fatal_alert != 0)
		return;

	switch (ctx->error.code) {
	case TLS13_ERR_VERIFY_FAILED:
		reason = SSL_R_CERTIFICATE_VERIFY_FAILED;
		break;
	case TLS13_ERR_HRR_FAILED:
		reason = SSL_R_NO_CIPHERS_AVAILABLE;
		break;
	case TLS13_ERR_TRAILING_DATA:
		reason = SSL_R_EXTRA_DATA_IN_MESSAGE;
		break;
	case TLS13_ERR_NO_SHARED_CIPHER:
		reason = SSL_R_NO_SHARED_CIPHER;
		break;
	case TLS13_ERR_NO_CERTIFICATE:
		reason = SSL_R_MISSING_RSA_CERTIFICATE; /* XXX */
		break;
	case TLS13_ERR_NO_PEER_CERTIFICATE:
		reason = SSL_R_PEER_DID_NOT_RETURN_A_CERTIFICATE;
		break;
	}

	/* Something (probably libcrypto) already pushed an error on the stack. */
	if (reason == SSL_R_UNKNOWN && ERR_peek_error() != 0)
		return;

	ERR_put_error(ERR_LIB_SSL, (0xfff), reason, ctx->error.file,
	    ctx->error.line);
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
		tls13_legacy_error(ssl);
		return -1;

	case TLS13_IO_ALERT:
		tls13_legacy_error(ssl);
		return -1;

	case TLS13_IO_WANT_POLLIN:
		BIO_set_retry_read(ssl->rbio);
		ssl->internal->rwstate = SSL_READING;
		return -1;

	case TLS13_IO_WANT_POLLOUT:
		BIO_set_retry_write(ssl->wbio);
		ssl->internal->rwstate = SSL_WRITING;
		return -1;

	case TLS13_IO_WANT_RETRY:
		SSLerror(ssl, ERR_R_INTERNAL_ERROR);
		return -1;
	}

	SSLerror(ssl, ERR_R_INTERNAL_ERROR);
	return -1;
}

int
tls13_legacy_pending(const SSL *ssl)
{
	struct tls13_ctx *ctx = ssl->internal->tls13;
	ssize_t ret;

	if (ctx == NULL)
		return 0;

	ret = tls13_pending_application_data(ctx->rl);
	if (ret < 0 || ret > INT_MAX)
		return 0;

	return ret;
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

	if (type != SSL3_RT_APPLICATION_DATA) {
		SSLerror(ssl, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
		return -1;
	}
	if (len < 0) {
		SSLerror(ssl, SSL_R_BAD_LENGTH); 
		return -1;
	}

	if (peek)
		ret = tls13_peek_application_data(ctx->rl, buf, len);
	else
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
	if (len < 0) {
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

int
tls13_use_legacy_client(struct tls13_ctx *ctx)
{
	SSL *s = ctx->ssl;
	CBS cbs;

	s->method = tls_legacy_client_method();
	s->internal->handshake_func = s->method->internal->ssl_connect;
	s->client_version = s->version = s->method->internal->max_version;

	if (!ssl3_setup_init_buffer(s))
		goto err;
	if (!ssl3_setup_buffers(s))
		goto err;
	if (!ssl_init_wbio_buffer(s, 0))
		goto err;

	if (s->bbio != s->wbio)
		s->wbio = BIO_push(s->bbio, s->wbio);

	/* Stash any unprocessed data from the last record. */
	tls13_record_layer_rbuf(ctx->rl, &cbs);
	if (CBS_len(&cbs) > 0) {
		if (!CBS_write_bytes(&cbs,
		    S3I(s)->rbuf.buf + SSL3_RT_HEADER_LENGTH,
		    S3I(s)->rbuf.len - SSL3_RT_HEADER_LENGTH, NULL))
			goto err;

		S3I(s)->rbuf.offset = SSL3_RT_HEADER_LENGTH;
		S3I(s)->rbuf.left = CBS_len(&cbs);
		S3I(s)->rrec.type = SSL3_RT_HANDSHAKE;
		S3I(s)->rrec.length = CBS_len(&cbs);
		s->internal->rstate = SSL_ST_READ_BODY;
		s->internal->packet = S3I(s)->rbuf.buf;
		s->internal->packet_length = SSL3_RT_HEADER_LENGTH;
		s->internal->mac_packet = 1;
	}

	/* Stash the current handshake message. */
	tls13_handshake_msg_data(ctx->hs_msg, &cbs);
	if (!CBS_write_bytes(&cbs, s->internal->init_buf->data,
	    s->internal->init_buf->length, NULL))
		goto err;

	S3I(s)->tmp.reuse_message = 1;
	S3I(s)->tmp.message_type = tls13_handshake_msg_type(ctx->hs_msg);
	S3I(s)->tmp.message_size = CBS_len(&cbs);

	S3I(s)->hs.state = SSL3_ST_CR_SRVR_HELLO_A;

	return 1;

 err:
	return 0;
}

int
tls13_use_legacy_server(struct tls13_ctx *ctx)
{
	SSL *s = ctx->ssl;
	CBS cbs;

	s->method = tls_legacy_server_method();
	s->internal->handshake_func = s->method->internal->ssl_accept;
	s->client_version = s->version = s->method->internal->max_version;
	s->server = 1;

	if (!ssl3_setup_init_buffer(s))
		goto err;
	if (!ssl3_setup_buffers(s))
		goto err;
	if (!ssl_init_wbio_buffer(s, 0))
		goto err;

	if (s->bbio != s->wbio)
		s->wbio = BIO_push(s->bbio, s->wbio);

	/* Stash any unprocessed data from the last record. */
	tls13_record_layer_rbuf(ctx->rl, &cbs);
	if (CBS_len(&cbs) > 0) {
		if (!CBS_write_bytes(&cbs,
		    S3I(s)->rbuf.buf + SSL3_RT_HEADER_LENGTH,
		    S3I(s)->rbuf.len - SSL3_RT_HEADER_LENGTH, NULL))
			goto err;

		S3I(s)->rbuf.offset = SSL3_RT_HEADER_LENGTH;
		S3I(s)->rbuf.left = CBS_len(&cbs);
		S3I(s)->rrec.type = SSL3_RT_HANDSHAKE;
		S3I(s)->rrec.length = CBS_len(&cbs);
		s->internal->rstate = SSL_ST_READ_BODY;
		s->internal->packet = S3I(s)->rbuf.buf;
		s->internal->packet_length = SSL3_RT_HEADER_LENGTH;
		s->internal->mac_packet = 1;
	}

	/* Stash the current handshake message. */
	tls13_handshake_msg_data(ctx->hs_msg, &cbs);
	if (!CBS_write_bytes(&cbs, s->internal->init_buf->data,
	    s->internal->init_buf->length, NULL))
		goto err;

	S3I(s)->tmp.reuse_message = 1;
	S3I(s)->tmp.message_type = tls13_handshake_msg_type(ctx->hs_msg);
	S3I(s)->tmp.message_size = CBS_len(&cbs);

	S3I(s)->hs.state = SSL3_ST_SR_CLNT_HELLO_A;

	return 1;

 err:
	return 0;
}

int
tls13_legacy_accept(SSL *ssl)
{
	struct tls13_ctx *ctx = ssl->internal->tls13;
	int ret;

	if (ctx == NULL) {
		if ((ctx = tls13_ctx_new(TLS13_HS_SERVER)) == NULL) {
			SSLerror(ssl, ERR_R_INTERNAL_ERROR); /* XXX */
			return -1;
		}
		ssl->internal->tls13 = ctx;
		ctx->ssl = ssl;
		ctx->hs = &S3I(ssl)->hs_tls13;

		if (!tls13_server_init(ctx)) {
			if (ERR_peek_error() == 0)
				SSLerror(ssl, ERR_R_INTERNAL_ERROR); /* XXX */
			return -1;
		}
	}

	ERR_clear_error();
	S3I(ssl)->hs.state = SSL_ST_ACCEPT;

	ret = tls13_server_accept(ctx);
	if (ret == TLS13_IO_USE_LEGACY)
		return ssl->method->internal->ssl_accept(ssl);
	if (ret == TLS13_IO_SUCCESS)
		S3I(ssl)->hs.state = SSL_ST_OK;

	return tls13_legacy_return_code(ssl, ret);
}

int
tls13_legacy_connect(SSL *ssl)
{
	struct tls13_ctx *ctx = ssl->internal->tls13;
	int ret;

#ifdef TLS13_USE_LEGACY_CLIENT_AUTH
	/* XXX drop back to legacy for client auth for now */
	if (ssl->cert->key->privatekey != NULL) {
		ssl->method = tls_legacy_client_method();
		return ssl->method->internal->ssl_connect(ssl);
	}
#endif

	if (ctx == NULL) {
		if ((ctx = tls13_ctx_new(TLS13_HS_CLIENT)) == NULL) {
			SSLerror(ssl, ERR_R_INTERNAL_ERROR); /* XXX */
			return -1;
		}
		ssl->internal->tls13 = ctx;
		ctx->ssl = ssl;
		ctx->hs = &S3I(ssl)->hs_tls13;

		if (!tls13_client_init(ctx)) {
			if (ERR_peek_error() == 0)
				SSLerror(ssl, ERR_R_INTERNAL_ERROR); /* XXX */
			return -1;
		}
	}

	ERR_clear_error();
	S3I(ssl)->hs.state = SSL_ST_CONNECT;

	ret = tls13_client_connect(ctx);
	if (ret == TLS13_IO_USE_LEGACY)
		return ssl->method->internal->ssl_connect(ssl);
	if (ret == TLS13_IO_SUCCESS)
		S3I(ssl)->hs.state = SSL_ST_OK;

	return tls13_legacy_return_code(ssl, ret);
}

int
tls13_legacy_shutdown(SSL *ssl)
{
	struct tls13_ctx *ctx = ssl->internal->tls13;
	uint8_t buf[512]; /* XXX */
	ssize_t ret;

	/*
	 * We need to return 0 when we have sent a close-notify but have not
	 * yet received one. We return 1 only once we have sent and received
	 * close-notify alerts. All other cases return -1 and set internal
	 * state appropriately.
	 */
	if (ctx == NULL || ssl->internal->quiet_shutdown) {
		ssl->internal->shutdown = SSL_SENT_SHUTDOWN | SSL_RECEIVED_SHUTDOWN;
		return 1;
	}

	/* Send close notify. */
	if (!(ssl->internal->shutdown & SSL_SENT_SHUTDOWN)) {
		ssl->internal->shutdown |= SSL_SENT_SHUTDOWN;
		if ((ret = tls13_send_alert(ctx->rl, TLS13_ALERT_CLOSE_NOTIFY)) < 0)
			return tls13_legacy_return_code(ssl, ret);
	}

	/* Ensure close notify has been sent. */
	if ((ret = tls13_record_layer_send_pending(ctx->rl)) != TLS13_IO_SUCCESS)
		return tls13_legacy_return_code(ssl, ret);

	/* Receive close notify. */
	if (!ctx->close_notify_recv) {
		/*
		 * If there is still application data pending then we have no
		 * option but to discard it here. The application should have
		 * continued to call SSL_read() instead of SSL_shutdown().
		 */
		/* XXX - tls13_drain_application_data()? */
		if ((ret = tls13_read_application_data(ctx->rl, buf, sizeof(buf))) > 0)
			ret = TLS13_IO_WANT_POLLIN;
		if (ret != TLS13_IO_EOF)
			return tls13_legacy_return_code(ssl, ret);
	}

	if (ctx->close_notify_recv)
		return 1;

	return 0;
}

int
tls13_legacy_servername_process(struct tls13_ctx *ctx, uint8_t *alert)
{
	int legacy_alert = SSL_AD_UNRECOGNIZED_NAME;
	int ret = SSL_TLSEXT_ERR_NOACK;
	SSL_CTX *ssl_ctx = ctx->ssl->ctx;
	SSL *ssl = ctx->ssl;

	if (ssl_ctx->internal->tlsext_servername_callback == NULL)
		ssl_ctx = ssl->initial_ctx;
	if (ssl_ctx->internal->tlsext_servername_callback == NULL)
		return 1;

	ret = ssl_ctx->internal->tlsext_servername_callback(ssl, &legacy_alert,
	    ssl_ctx->internal->tlsext_servername_arg);

	if (ret == SSL_TLSEXT_ERR_ALERT_FATAL ||
	    ret == SSL_TLSEXT_ERR_ALERT_WARNING) {
		if (legacy_alert >= 0 && legacy_alert <= 255)
			*alert = legacy_alert;
		return 0;
	}

	return 1;
}
