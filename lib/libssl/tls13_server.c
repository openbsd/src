/* $OpenBSD: tls13_server.c,v 1.8 2020/01/23 02:24:38 jsing Exp $ */
/*
 * Copyright (c) 2019 Joel Sing <jsing@openbsd.org>
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

	if (!tls1_transcript_init(s))
		return 0;

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
	CBS extensions_block, extensions, extension_data;
	uint16_t selected_version = 0;
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
		if (!CBS_get_u16(&extension_data, &selected_version))
			return 1;
		if (CBS_len(&extension_data) != 0)
			return 1;
	}

	return (selected_version < TLS1_3_VERSION);
}

static int
tls13_client_hello_process(struct tls13_ctx *ctx, CBS *cbs)
{
	CBS cipher_suites, client_random, compression_methods, session_id;
	uint16_t legacy_version;
	SSL *s = ctx->ssl;
	int alert;

	if (!CBS_get_u16(cbs, &legacy_version))
		goto err;
	if (!CBS_get_bytes(cbs, &client_random, SSL3_RANDOM_SIZE))
		goto err;
	if (!CBS_get_u8_length_prefixed(cbs, &session_id))
		goto err;
	if (!CBS_get_u8_length_prefixed(cbs, &cipher_suites))
		goto err;
	if (!CBS_get_u8_length_prefixed(cbs, &compression_methods))
		goto err;

	if (tls13_client_hello_is_legacy(cbs)) {
		if (!CBS_skip(cbs, CBS_len(cbs)))
			goto err;
		return tls13_use_legacy_server(ctx);
	}

	if (!tlsext_server_parse(s, cbs, &alert, SSL_TLSEXT_MSG_CH))
		goto err;

	/* XXX - implement. */

 err:
	return 0;
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

int
tls13_server_hello_send(struct tls13_ctx *ctx, CBB *cbb)
{
	ctx->handshake_stage.hs_type |= NEGOTIATED;

	return 0;
}

int
tls13_server_hello_retry_send(struct tls13_ctx *ctx, CBB *cbb)
{
	return 0;
}

int
tls13_server_encrypted_extensions_send(struct tls13_ctx *ctx, CBB *cbb)
{
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
