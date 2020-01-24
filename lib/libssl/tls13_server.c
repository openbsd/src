/* $OpenBSD: tls13_server.c,v 1.15 2020/01/24 04:47:13 jsing Exp $ */
/*
 * Copyright (c) 2019, 2020 Joel Sing <jsing@openbsd.org>
 * Copyright (c) 2020 Bob Beck <beck@openbsd.org>
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

#include <openssl/curve25519.h>

#include "ssl_locl.h"
#include "ssl_tlsext.h"

#include "tls13_handshake.h"
#include "tls13_internal.h"

static int
tls13_accept(struct tls13_ctx *ctx)
{
	if (ctx->mode != TLS13_HS_SERVER)
		return TLS13_IO_FAILURE;

	return tls13_handshake_perform(ctx);
}

static int
tls13_server_init(struct tls13_ctx *ctx)
{
	SSL *s = ctx->ssl;

	if (!ssl_supported_version_range(s, &ctx->hs->min_version,
	    &ctx->hs->max_version)) {
		SSLerror(s, SSL_R_NO_PROTOCOLS_AVAILABLE);
		return 0;
	}
	s->version = ctx->hs->max_version;

	if (!tls1_transcript_init(s))
		return 0;

	if ((s->session = SSL_SESSION_new()) == NULL)
		return 0;

	arc4random_buf(s->s3->server_random, SSL3_RANDOM_SIZE);

	return 1;
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

	S3I(ssl)->hs.state = SSL_ST_ACCEPT;

	ret = tls13_accept(ctx);
	if (ret == TLS13_IO_USE_LEGACY)
		return ssl->method->internal->ssl_accept(ssl);
	if (ret == TLS13_IO_SUCCESS)
		S3I(ssl)->hs.state = SSL_ST_OK;

	return tls13_legacy_return_code(ssl, ret);
}

int
tls13_use_legacy_server(struct tls13_ctx *ctx)
{
	SSL *s = ctx->ssl;
	CBS cbs;

	s->method = tls_legacy_server_method();
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

static int
tls13_client_hello_is_legacy(CBS *cbs)
{
	CBS extensions_block, extensions, extension_data, versions;
	uint16_t version, max_version = 0;
	uint16_t type;

	CBS_dup(cbs, &extensions_block);

	if (!CBS_get_u16_length_prefixed(&extensions_block, &extensions))
		return 1;

	while (CBS_len(&extensions) > 0) {
		if (!CBS_get_u16(&extensions, &type))
			return 1;
		if (!CBS_get_u16_length_prefixed(&extensions, &extension_data))
			return 1;

		if (type != TLSEXT_TYPE_supported_versions)
			continue;
		if (!CBS_get_u8_length_prefixed(&extension_data, &versions))
			return 1;
		while (CBS_len(&versions) > 0) {
			if (!CBS_get_u16(&versions, &version))
				return 1;
			if (version >= max_version)
				max_version = version;
		}
		if (CBS_len(&extension_data) != 0)
			return 1;
	}

	return (max_version < TLS1_3_VERSION);
}

static int
tls13_client_hello_process(struct tls13_ctx *ctx, CBS *cbs)
{
	CBS cipher_suites, client_random, compression_methods, session_id;
	STACK_OF(SSL_CIPHER) *ciphers = NULL;
	const SSL_CIPHER *cipher;
	uint16_t legacy_version;
	uint8_t compression_method;
	int alert_desc, comp_null;
	SSL *s = ctx->ssl;
	int ret = 0;

	if (!CBS_get_u16(cbs, &legacy_version))
		goto err;
	if (!CBS_get_bytes(cbs, &client_random, SSL3_RANDOM_SIZE))
		goto err;
	if (!CBS_get_u8_length_prefixed(cbs, &session_id))
		goto err;
	if (!CBS_get_u16_length_prefixed(cbs, &cipher_suites))
		goto err;
	if (!CBS_get_u8_length_prefixed(cbs, &compression_methods))
		goto err;

	if (tls13_client_hello_is_legacy(cbs)) {
		if (!CBS_skip(cbs, CBS_len(cbs)))
			goto err;
		return tls13_use_legacy_server(ctx);
	}

	if (!tlsext_server_parse(s, cbs, &alert_desc, SSL_TLSEXT_MSG_CH)) {
		ctx->alert = alert_desc;
		goto err;
	}

	/*
	 * If we got this far we have a supported versions extension that offers
	 * TLS 1.3 or later. This requires the legacy version be set to 0x0303.
	 */
	if (legacy_version != TLS1_2_VERSION) {
		ctx->alert = SSL_AD_PROTOCOL_VERSION;
		goto err;
	}

	/* Store legacy session identifier so we can echo it. */
	if (CBS_len(&session_id) > sizeof(ctx->hs->legacy_session_id)) {
		ctx->alert = SSL_AD_ILLEGAL_PARAMETER;
		goto err;
	}
	if (!CBS_write_bytes(&session_id, ctx->hs->legacy_session_id,
	    sizeof(ctx->hs->legacy_session_id), &ctx->hs->legacy_session_id_len))
		goto err;

	/* Parse cipher suites list and select preferred cipher. */
	if ((ciphers = ssl_bytes_to_cipher_list(s, &cipher_suites)) == NULL) {
		ctx->alert = SSL_AD_ILLEGAL_PARAMETER;
		goto err;
	}
	cipher = ssl3_choose_cipher(s, ciphers, SSL_get_ciphers(s));
	if (cipher == NULL) {
		tls13_set_errorx(ctx, TLS13_ERR_NO_SHARED_CIPHER, 0,
		    "no shared cipher found", NULL);
		ctx->alert = SSL_AD_HANDSHAKE_FAILURE;
		goto err;
	}
	S3I(s)->hs.new_cipher = cipher;

	/* Ensure they advertise the NULL compression method. */
	comp_null = 0;
	while (CBS_len(&compression_methods) > 0) {
		if (!CBS_get_u8(&compression_methods, &compression_method))
			goto err;
		if (compression_method == 0)
			comp_null = 1;
	}
	if (!comp_null) {
		ctx->alert = SSL_AD_ILLEGAL_PARAMETER;
		goto err;
	}

	ret = 1;

 err:
	sk_SSL_CIPHER_free(ciphers);

	return ret;
}

int
tls13_client_hello_recv(struct tls13_ctx *ctx, CBS *cbs)
{
	SSL *s = ctx->ssl;

	if (!tls13_client_hello_process(ctx, cbs))
		goto err;

	/* See if we switched back to the legacy client method. */
	if (s->method->internal->version < TLS1_3_VERSION)
		return 1;

	tls13_record_layer_allow_ccs(ctx->rl, 1);

	return 1;

 err:
	return 0;
}

int
tls13_client_hello_retry_send(struct tls13_ctx *ctx, CBB *cbb)
{
	return 0;
}

int
tls13_server_hello_retry_recv(struct tls13_ctx *ctx, CBS *cbs)
{
	return 0;
}

int
tls13_client_end_of_early_data_send(struct tls13_ctx *ctx, CBB *cbb)
{
	return 0;
}

int
tls13_client_end_of_early_data_recv(struct tls13_ctx *ctx, CBS *cbs)
{
	return 0;
}

int
tls13_client_certificate_send(struct tls13_ctx *ctx, CBB *cbb)
{
	return 0;
}

int
tls13_client_certificate_recv(struct tls13_ctx *ctx, CBS *cbs)
{
	return 0;
}

int
tls13_client_certificate_verify_send(struct tls13_ctx *ctx, CBB *cbb)
{
	return 0;
}

int
tls13_client_certificate_verify_recv(struct tls13_ctx *ctx, CBS *cbs)
{
	return 0;
}

int
tls13_client_finished_recv(struct tls13_ctx *ctx, CBS *cbs)
{
	tls13_record_layer_allow_ccs(ctx->rl, 0);

	return 0;
}

int
tls13_client_key_update_send(struct tls13_ctx *ctx, CBB *cbb)
{
	return 0;
}

int
tls13_client_key_update_recv(struct tls13_ctx *ctx, CBS *cbs)
{
	return 0;
}

static int
tls13_server_hello_build(struct tls13_ctx *ctx, CBB *cbb)
{
	CBB session_id;
	SSL *s = ctx->ssl;
	uint16_t cipher;

	cipher = SSL_CIPHER_get_value(S3I(s)->hs.new_cipher);

	if (!CBB_add_u16(cbb, TLS1_2_VERSION))
		goto err;
	if (!CBB_add_bytes(cbb, s->s3->server_random, SSL3_RANDOM_SIZE))
		goto err;
	if (!CBB_add_u8_length_prefixed(cbb, &session_id))
		goto err;
	if (!CBB_add_bytes(&session_id, ctx->hs->legacy_session_id,
	    ctx->hs->legacy_session_id_len))
		goto err;
	if (!CBB_add_u16(cbb, cipher))
		goto err;
	if (!CBB_add_u8(cbb, 0))
		goto err;
	if (!tlsext_server_build(s, cbb, SSL_TLSEXT_MSG_SH))
		goto err;

	if (!CBB_flush(cbb))
		goto err;

	return 1;
err:
	return 0;
}

int
tls13_server_hello_send(struct tls13_ctx *ctx, CBB *cbb)
{
	if (!tls13_server_hello_build(ctx, cbb))
		return 0;

	return 1;
}

int
tls13_server_hello_sent(struct tls13_ctx *ctx)
{
	struct tls13_secrets *secrets;
	struct tls13_secret context;
	unsigned char buf[EVP_MAX_MD_SIZE];
	uint8_t *shared_key = NULL;
	size_t hash_len;
	SSL *s = ctx->ssl;
	int ret = 0;

	/* XXX - handle other key share types. */
	if (ctx->hs->x25519_peer_public == NULL) {
		/* XXX - alert. */
		goto err;
	}
	if ((shared_key = malloc(X25519_KEY_LENGTH)) == NULL)
		goto err;
	if (!X25519(shared_key, ctx->hs->x25519_private,
	    ctx->hs->x25519_peer_public))
		goto err;

	s->session->cipher = S3I(s)->hs.new_cipher;
	s->session->ssl_version = ctx->hs->server_version;

	if ((ctx->aead = tls13_cipher_aead(S3I(s)->hs.new_cipher)) == NULL)
		goto err;
	if ((ctx->hash = tls13_cipher_hash(S3I(s)->hs.new_cipher)) == NULL)
		goto err;

	if ((secrets = tls13_secrets_create(ctx->hash, 0)) == NULL)
		goto err;
	S3I(ctx->ssl)->hs_tls13.secrets = secrets;

	/* XXX - pass in hash. */
	if (!tls1_transcript_hash_init(s))
		goto err;
	if (!tls1_transcript_hash_value(s, buf, sizeof(buf), &hash_len))
		goto err;
	context.data = buf;
	context.len = hash_len;

	/* Early secrets. */
	if (!tls13_derive_early_secrets(secrets, secrets->zeros.data,
	    secrets->zeros.len, &context))
		goto err;

	/* Handshake secrets. */
	if (!tls13_derive_handshake_secrets(ctx->hs->secrets, shared_key,
	    X25519_KEY_LENGTH, &context))
		goto err;

	tls13_record_layer_set_aead(ctx->rl, ctx->aead);
	tls13_record_layer_set_hash(ctx->rl, ctx->hash);

	if (!tls13_record_layer_set_read_traffic_key(ctx->rl,
	    &secrets->client_handshake_traffic))
		goto err;
	if (!tls13_record_layer_set_write_traffic_key(ctx->rl,
	    &secrets->server_handshake_traffic))
		goto err;

 	ctx->handshake_stage.hs_type |= NEGOTIATED | WITHOUT_CR;
	ret = 1;

 err:
	freezero(shared_key, X25519_KEY_LENGTH);
	return ret;
}

int
tls13_server_hello_retry_send(struct tls13_ctx *ctx, CBB *cbb)
{
	return 0;
}

int
tls13_server_encrypted_extensions_send(struct tls13_ctx *ctx, CBB *cbb)
{
	if (!tlsext_server_build(ctx->ssl, cbb, SSL_TLSEXT_MSG_EE))
		goto err;

	return 1;
 err:
	return 0;
}

int
tls13_server_certificate_send(struct tls13_ctx *ctx, CBB *cbb)
{
	return 0;
}

int
tls13_server_certificate_request_send(struct tls13_ctx *ctx, CBB *cbb)
{
	CBB certificate_request_context;

	if (!CBB_add_u8_length_prefixed(cbb, &certificate_request_context))
		goto err;
	if (!tlsext_server_build(ctx->ssl, cbb, SSL_TLSEXT_MSG_CR))
		goto err;

	if (!CBB_flush(cbb))
		goto err;

	return 1;
 err:
	return 0;
}

int
tls13_server_certificate_verify_send(struct tls13_ctx *ctx, CBB *cbb)
{
	return 0;
}

int
tls13_server_finished_send(struct tls13_ctx *ctx, CBB *cbb)
{
	return 0;
}
