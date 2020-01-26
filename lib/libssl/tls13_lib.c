/*	$OpenBSD: tls13_lib.c,v 1.31 2020/01/26 02:45:27 beck Exp $ */
/*
 * Copyright (c) 2018, 2019 Joel Sing <jsing@openbsd.org>
 * Copyright (c) 2019 Bob Beck <beck@openbsd.org>
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

SSL3_ENC_METHOD TLSv1_3_enc_data = {
	.enc = NULL,
	.enc_flags = SSL_ENC_FLAG_SIGALGS|SSL_ENC_FLAG_TLS1_3_CIPHERS,
};

/*
 * RFC 8446 section 4.1.3, magic values which must be set by the
 * server in server random if it is willing to downgrade but supports
 * tls v1.3
 */
uint8_t tls13_downgrade_12[8] = {0x44, 0x4f, 0x57, 0x4e, 0x47, 0x52, 0x44, 0x01};
uint8_t tls13_downgrade_11[8] = {0x44, 0x4f, 0x57, 0x4e, 0x47, 0x52, 0x44, 0x00};

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
		ctx->close_notify_recv = 1;
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

static void
tls13_legacy_handshake_message_recv_cb(void *arg, CBS *cbs)
{
	struct tls13_ctx *ctx = arg;
	SSL *s = ctx->ssl;

	if (s->internal->msg_callback != NULL)
		s->internal->msg_callback(0, TLS1_3_VERSION, SSL3_RT_HANDSHAKE,
		    CBS_data(cbs), CBS_len(cbs), s,
		    s->internal->msg_callback_arg);
}

static void
tls13_legacy_handshake_message_sent_cb(void *arg, CBS *cbs)
{
	struct tls13_ctx *ctx = arg;
	SSL *s = ctx->ssl;

	if (s->internal->msg_callback != NULL)
		s->internal->msg_callback(1, TLS1_3_VERSION, SSL3_RT_HANDSHAKE,
		    CBS_data(cbs), CBS_len(cbs), s,
		    s->internal->msg_callback_arg);
}

static int
tls13_phh_update_local_traffic_secret(struct tls13_ctx *ctx)
{
	struct tls13_secrets *secrets = ctx->hs->secrets;

	if (ctx->mode == TLS13_HS_CLIENT)
		return (tls13_update_client_traffic_secret(secrets) &&
		    tls13_record_layer_set_write_traffic_key(ctx->rl,
			&secrets->client_application_traffic));
	return (tls13_update_server_traffic_secret(secrets) &&
	    tls13_record_layer_set_read_traffic_key(ctx->rl,
	    &secrets->server_application_traffic));
}

static int
tls13_phh_update_peer_traffic_secret(struct tls13_ctx *ctx)
{
	struct tls13_secrets *secrets = ctx->hs->secrets;

	if (ctx->mode == TLS13_HS_CLIENT)
		return (tls13_update_server_traffic_secret(secrets) &&
		    tls13_record_layer_set_read_traffic_key(ctx->rl,
		    &secrets->server_application_traffic));
	return (tls13_update_client_traffic_secret(secrets) &&
	    tls13_record_layer_set_write_traffic_key(ctx->rl,
	    &secrets->client_application_traffic));
}

/*
 * XXX arbitrarily chosen limit of 100 post handshake handshake
 * messages in an hour - to avoid a hostile peer from constantly
 * requesting certificates or key renegotiaitons, etc.
 */
static int
tls13_phh_limit_check(struct tls13_ctx *ctx)
{
	time_t now = time(NULL);

	if (ctx->phh_last_seen > now - TLS13_PHH_LIMIT_TIME) {
		if (ctx->phh_count > TLS13_PHH_LIMIT)
			return 0;
	} else
		ctx->phh_count = 0;
	ctx->phh_count++;
	ctx->phh_last_seen = now;
	return 1;
}

static ssize_t
tls13_key_update_recv(struct tls13_ctx *ctx, CBS *cbs)
{
	ssize_t ret = TLS13_IO_FAILURE;

	if (!CBS_get_u8(cbs, &ctx->key_update_request))
		goto err;
	if (CBS_len(cbs) != 0)
		goto err;

	if (!tls13_phh_update_peer_traffic_secret(ctx))
		goto err;

	if (ctx->key_update_request) {
		CBB cbb;
		CBS cbs; /* XXX */

		free(ctx->hs_msg);
		ctx->hs_msg = tls13_handshake_msg_new();
		if (!tls13_handshake_msg_start(ctx->hs_msg, &cbb, TLS13_MT_KEY_UPDATE))
			goto err;
		if (!CBB_add_u8(&cbb, 0))
			goto err;
		if (!tls13_handshake_msg_finish(ctx->hs_msg))
			goto err;
		tls13_handshake_msg_data(ctx->hs_msg, &cbs);
		ret = tls13_record_layer_phh(ctx->rl, &cbs);

		tls13_handshake_msg_free(ctx->hs_msg);
		ctx->hs_msg = NULL;
	} else
		ret = TLS13_IO_SUCCESS;

	return ret;
 err:
	ctx->key_update_request = 0;
	/* XXX alert */
	return TLS13_IO_FAILURE;
}

static void
tls13_phh_done_cb(void *cb_arg)
{
	struct tls13_ctx *ctx = cb_arg;

	if (ctx->key_update_request) {
		tls13_phh_update_local_traffic_secret(ctx);
		ctx->key_update_request = 0;
	}
}

static ssize_t
tls13_phh_received_cb(void *cb_arg, CBS *cbs)
{
	ssize_t ret = TLS13_IO_FAILURE;
	struct tls13_ctx *ctx = cb_arg;
	CBS phh_cbs;

	if (!tls13_phh_limit_check(ctx))
		return tls13_send_alert(ctx->rl, SSL3_AD_UNEXPECTED_MESSAGE);

	if ((ctx->hs_msg == NULL) &&
	    ((ctx->hs_msg = tls13_handshake_msg_new()) == NULL))
		return TLS13_IO_FAILURE;

	if (!tls13_handshake_msg_set_buffer(ctx->hs_msg, cbs))
		return TLS13_IO_FAILURE;

	if ((ret = tls13_handshake_msg_recv(ctx->hs_msg, ctx->rl))
	    != TLS13_IO_SUCCESS)
		return ret;

	if (!tls13_handshake_msg_content(ctx->hs_msg, &phh_cbs))
		return TLS13_IO_FAILURE;

	switch(tls13_handshake_msg_type(ctx->hs_msg)) {
	case TLS13_MT_KEY_UPDATE:
		ret = tls13_key_update_recv(ctx, &phh_cbs);
		break;
	case TLS13_MT_NEW_SESSION_TICKET:
		/* XXX do nothing for now and ignore this */
		break;
	case TLS13_MT_CERTIFICATE_REQUEST:
		/* XXX add support if we choose to advertise this */
		/* FALLTHROUGH */
	default:
		ret = TLS13_IO_FAILURE; /* XXX send alert */
		break;
	}

	tls13_handshake_msg_free(ctx->hs_msg);
	ctx->hs_msg = NULL;
	return ret;
}

struct tls13_ctx *
tls13_ctx_new(int mode)
{
	struct tls13_ctx *ctx = NULL;

	if ((ctx = calloc(sizeof(struct tls13_ctx), 1)) == NULL)
		goto err;

	ctx->mode = mode;

	if ((ctx->rl = tls13_record_layer_new(tls13_legacy_wire_read_cb,
	    tls13_legacy_wire_write_cb, tls13_alert_received_cb,
	    tls13_phh_received_cb, tls13_phh_done_cb, ctx)) == NULL)
		goto err;

	ctx->handshake_message_sent_cb = tls13_legacy_handshake_message_sent_cb;
	ctx->handshake_message_recv_cb = tls13_legacy_handshake_message_recv_cb;

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

	tls13_error_clear(&ctx->error);
	tls13_record_layer_free(ctx->rl);
	tls13_handshake_msg_free(ctx->hs_msg);

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
	}

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
	if (!ctx->close_notify_sent) {
		ctx->close_notify_sent = 1;
		if ((ret = tls13_send_alert(ctx->rl, SSL_AD_CLOSE_NOTIFY)) < 0)
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

/*
 * Certificate Verify padding - RFC 8446 section 4.4.3.
 */
uint8_t tls13_cert_verify_pad[64] = {
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
};

uint8_t tls13_cert_client_verify_context[] = "TLS 1.3, client CertificateVerify";
uint8_t tls13_cert_server_verify_context[] = "TLS 1.3, server CertificateVerify";

int
tls13_cert_add(CBB *cbb, X509 *cert)
{
	CBB cert_data, cert_exts;
	uint8_t *data;
	int cert_len;

	if ((cert_len = i2d_X509(cert, NULL)) < 0)
		return 0;

	if (!CBB_add_u24_length_prefixed(cbb, &cert_data))
		return 0;
	if (!CBB_add_space(&cert_data, &data, cert_len))
		return 0;
	if (i2d_X509(cert, &data) != cert_len)
		return 0;

	if (!CBB_add_u16_length_prefixed(cbb, &cert_exts))
		return 0;

	if (!CBB_flush(cbb))
		return 0;

	return 1;
}
