/* $OpenBSD: tls13_client.c,v 1.5 2019/02/09 15:26:15 jsing Exp $ */
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

#include "ssl_locl.h"

#include <openssl/curve25519.h>
#include <openssl/ssl3.h>

#include "bytestring.h"
#include "ssl_tlsext.h"
#include "tls13_handshake.h"
#include "tls13_internal.h"

int
tls13_connect(struct tls13_ctx *ctx)
{
	if (ctx->mode != TLS13_HS_CLIENT)
		return TLS13_IO_FAILURE;

	return tls13_handshake_perform(ctx);
}

static int
tls13_client_init(struct tls13_ctx *ctx)
{
	SSL *s = ctx->ssl;

	if (!ssl_supported_version_range(s, &ctx->hs->min_version,
	    &ctx->hs->max_version)) {
		SSLerror(s, SSL_R_NO_PROTOCOLS_AVAILABLE);
		return 0;
	}
	s->client_version = s->version = ctx->hs->max_version;

	if (!ssl_get_new_session(s, 0)) /* XXX */
		return 0;

	if (!tls1_transcript_init(s))
		return 0;

	arc4random_buf(s->s3->client_random, SSL3_RANDOM_SIZE);

	return 1;
}

int
tls13_legacy_connect(SSL *ssl)
{
	struct tls13_ctx *ctx = ssl->internal->tls13;
	int ret;

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

	ret = tls13_connect(ctx);

	return tls13_legacy_return_code(ssl, ret);
}

static int
tls13_client_hello_build(SSL *s, CBB *cbb)
{
	CBB cipher_suites, compression_methods, session_id;
	uint8_t *sid;

	if (!CBB_add_u16(cbb, TLS1_2_VERSION))
		goto err;
	if (!CBB_add_bytes(cbb, s->s3->client_random, SSL3_RANDOM_SIZE))
		goto err;

	/* Either 32-random bytes or zero length... */
	/* XXX - session resumption for TLSv1.2? */
	if (!CBB_add_u8_length_prefixed(cbb, &session_id))
		goto err;
	if (!CBB_add_space(&session_id, &sid, 32))
		goto err;
	arc4random_buf(sid, 32);

	if (!CBB_add_u16_length_prefixed(cbb, &cipher_suites))
		goto err;
	if (!ssl_cipher_list_to_bytes(s, SSL_get_ciphers(s), &cipher_suites)) {
		SSLerror(s, SSL_R_NO_CIPHERS_AVAILABLE);
		goto err;
	}

	if (!CBB_add_u8_length_prefixed(cbb, &compression_methods))
		goto err;
	if (!CBB_add_u8(&compression_methods, 0))
		goto err;

	if (!tlsext_client_build(s, cbb, SSL_TLSEXT_MSG_CH))
		goto err;

	if (!CBB_flush(cbb))
		goto err;

	return 1;

 err:
	return 0;
}

int
tls13_client_hello_send(struct tls13_ctx *ctx)
{
	CBB body;

	if (!tls13_handshake_msg_start(ctx->hs_msg, &body, TLS13_MT_CLIENT_HELLO))
		return 0;
	if (!tls13_client_hello_build(ctx->ssl, &body))
		return 0;
	if (!tls13_handshake_msg_finish(ctx->hs_msg))
		return 0;

	return 1;
}

/*
 * HelloRetryRequest hash - RFC 8446 section 4.1.3.
 */
static const uint8_t tls13_hello_retry_request_hash[] = {
	0xcf, 0x21, 0xad, 0x74, 0xe5, 0x9a, 0x61, 0x11,
	0xbe, 0x1d, 0x8c, 0x02, 0x1e, 0x65, 0xb8, 0x91,
	0xc2, 0xa2, 0x11, 0x16, 0x7a, 0xbb, 0x8c, 0x5e,
	0x07, 0x9e, 0x09, 0xe2, 0xc8, 0xa8, 0x33, 0x9c, 
};

static int
tls13_server_hello_process(struct tls13_ctx *ctx, CBS *cbs)
{
	CBS server_random, session_id;
	uint16_t cipher_suite, legacy_version;
	uint8_t compression_method;
	const SSL_CIPHER *cipher;
	SSL *s = ctx->ssl;
	int alert;

	if (!CBS_get_u16(cbs, &legacy_version))
		goto err;
	if (!CBS_get_bytes(cbs, &server_random, SSL3_RANDOM_SIZE))
		goto err;
	if (!CBS_get_u8_length_prefixed(cbs, &session_id))
		goto err;
	if (!CBS_get_u16(cbs, &cipher_suite))
		goto err;
	if (!CBS_get_u8(cbs, &compression_method))
		goto err;

	if (!tlsext_client_parse(s, cbs, &alert, SSL_TLSEXT_MSG_SH))
		goto err;

	if (CBS_len(cbs) != 0)
		goto err;

	/*
	 * See if a supported versions extension was returned. If it was then
	 * the legacy version must be set to 0x0303 (RFC 8446 section 4.1.3).
	 * Otherwise, fallback to the legacy version, ensuring that it is both
	 * within range and not TLS 1.3 or greater (which must use the
	 * supported version extension.
	 */
	if (ctx->hs->server_version != 0) {
		if (legacy_version != TLS1_2_VERSION) {
			/* XXX - alert. */
			goto err;
		}
	} else {
		if (legacy_version < ctx->hs->min_version ||
		    legacy_version > ctx->hs->max_version ||
		    legacy_version > TLS1_2_VERSION) {
			/* XXX - alert. */
			goto err;
		}
		ctx->hs->server_version = legacy_version;
	}

	/* XXX - session_id must match. */

	/*
	 * Ensure that the cipher suite is one that we offered in the client
	 * hello and that it matches the TLS version selected.
	 */
	cipher = ssl3_get_cipher_by_value(cipher_suite);
	if (cipher == NULL ||
	    sk_SSL_CIPHER_find(ssl_get_ciphers_by_id(s), cipher) < 0) {
		/* XXX - alert. */
		goto err;
	}
	if (ctx->hs->server_version == TLS1_3_VERSION &&
	    cipher->algorithm_ssl != SSL_TLSV1_3) {
		/* XXX - alert. */
		goto err;
	}
	/* XXX - move this to hs_tls13? */
	S3I(s)->hs.new_cipher = cipher;

	if (compression_method != 0) {
		/* XXX - alert. */
		goto err;
	}

	if (CBS_mem_equal(&server_random, tls13_hello_retry_request_hash,
	    sizeof(tls13_hello_retry_request_hash)))
		ctx->handshake_stage.hs_type |= WITH_HRR;

	return 1;

 err:
	/* XXX - send alert. */

	return 0;
}

int
tls13_server_hello_recv(struct tls13_ctx *ctx)
{
	struct tls13_secrets *secrets;
	struct tls13_secret context;
	unsigned char buf[EVP_MAX_MD_SIZE];
	uint8_t *shared_key = NULL;
	size_t hash_len;
	SSL *s = ctx->ssl;
	int ret = 0;
	CBS cbs;

	if (!tls13_handshake_msg_content(ctx->hs_msg, &cbs))
		goto err;

	if (!tls13_server_hello_process(ctx, &cbs))
		goto err;

	if (ctx->hs->server_version < TLS1_3_VERSION) {
		/* XXX - switch back to legacy client. */
		goto err;
	}

	if (ctx->handshake_stage.hs_type & WITH_HRR)
		return 1;

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

	if (!tls13_record_layer_set_traffic_keys(ctx->rl,
	    &secrets->server_handshake_traffic,
	    &secrets->client_handshake_traffic))
		goto err;

	ctx->handshake_stage.hs_type |= NEGOTIATED;
	ret = 1;

 err:
	freezero(shared_key, X25519_KEY_LENGTH);
	return ret;
}

int
tls13_server_encrypted_extensions_recv(struct tls13_ctx *ctx)
{
	int alert;
	CBS cbs;

	if (!tls13_handshake_msg_content(ctx->hs_msg, &cbs))
		goto err;

	if (!tlsext_client_parse(ctx->ssl, &cbs, &alert, SSL_TLSEXT_MSG_EE))
		goto err;

	if (CBS_len(&cbs) != 0)
		goto err;

	return 1;

 err:
	/* XXX - send alert. */

	return 0;
}
