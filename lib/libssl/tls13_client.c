/* $OpenBSD: tls13_client.c,v 1.1 2019/01/21 13:45:57 jsing Exp $ */
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
#include "tls13_internal.h"

int
tls13_connect(struct tls13_ctx *ctx)
{
	if (ctx->mode != TLS13_HS_CLIENT)
		return TLS13_IO_FAILURE;

	return tls13_handshake_perform(ctx);
}

static int
tls13_client_init(SSL *s)
{
	if (!ssl_supported_version_range(s, &S3I(s)->hs_tls13.min_version,
	    &S3I(s)->hs_tls13.max_version)) {
		SSLerror(s, SSL_R_NO_PROTOCOLS_AVAILABLE);
		return 0;
	}
	s->client_version = s->version = S3I(s)->hs_tls13.max_version;

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

		if (!tls13_client_init(ssl)) {
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
