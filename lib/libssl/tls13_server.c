/* $OpenBSD: tls13_server.c,v 1.3 2019/11/17 14:25:03 tb Exp $ */
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

	/* XXX implement. */

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
tls13_client_hello_recv(struct tls13_ctx *ctx)
{
	return 0;
}

int
tls13_client_hello_retry_send(struct tls13_ctx *ctx)
{
	return 0;
}

int
tls13_server_hello_retry_recv(struct tls13_ctx *ctx)
{
	return 0;
}

int
tls13_client_hello_retry_recv(struct tls13_ctx *ctx)
{
	return 0;
}


int
tls13_client_end_of_early_data_send(struct tls13_ctx *ctx)
{
	return 0;
}

int
tls13_client_end_of_early_data_recv(struct tls13_ctx *ctx)
{
	return 0;
}

int
tls13_client_certificate_send(struct tls13_ctx *ctx)
{
	return 0;
}

int
tls13_client_certificate_recv(struct tls13_ctx *ctx)
{
	return 0;
}

int
tls13_client_certificate_verify_send(struct tls13_ctx *ctx)
{
	return 0;
}

int
tls13_client_certificate_verify_recv(struct tls13_ctx *ctx)
{
	return 0;
}

int
tls13_client_finished_recv(struct tls13_ctx *ctx)
{
	return 0;
}

int
tls13_client_key_update_send(struct tls13_ctx *ctx)
{
	return 0;
}

int
tls13_client_key_update_recv(struct tls13_ctx *ctx)
{
	return 0;
}

int
tls13_server_hello_send(struct tls13_ctx *ctx)
{
	ctx->handshake_stage.hs_type |= NEGOTIATED;

	return 0;
}

int
tls13_server_hello_retry_send(struct tls13_ctx *ctx)
{
	return 0;
}

int
tls13_server_encrypted_extensions_send(struct tls13_ctx *ctx)
{
	return 0;
}

int
tls13_server_certificate_send(struct tls13_ctx *ctx)
{
	return 0;
}

int
tls13_server_certificate_request_send(struct tls13_ctx *ctx)
{
	return 0;
}

int
tls13_server_certificate_verify_send(struct tls13_ctx *ctx)
{
	return 0;
}

int
tls13_server_finished_send(struct tls13_ctx *ctx)
{
	return 0;
}
