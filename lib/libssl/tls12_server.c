/* $OpenBSD: tls12_server.c,v 1.1 2026/09/22 18:57:11 jsing Exp $ */
/*
 * Copyright (c) 2026 Joel Sing <jsing@openbsd.org>
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

#include <openssl/ssl3.h>

#include "bytestring.h"
#include "ssl_local.h"
#include "ssl_sigalgs.h"
#include "ssl_tlsext.h"
#include "tls12_handshake.h"
#include "tls12_internal.h"

int
tls12_server_init(struct tls12_ctx *ctx)
{
	return 0;
}

int
tls12_server_accept(struct tls12_ctx *ctx)
{
	if (ctx->mode != TLS12_HS_SERVER)
		return TLS12_IO_FAILURE;

	return tls12_handshake_perform(ctx);
}

int
tls12_client_hello_recv(struct tls12_ctx *ctx, CBS *cbs)
{
	return 0;
}

int
tls12_client_certificate_recv(struct tls12_ctx *ctx, CBS *cbs)
{
	return 0;
}

int
tls12_client_certificate_verify_recv(struct tls12_ctx *ctx, CBS *cbs)
{
	return 0;
}

int
tls12_client_key_exchange_recv(struct tls12_ctx *ctx, CBS *cbs)
{
	return 0;
}

int
tls12_client_finished_recv(struct tls12_ctx *ctx, CBS *cbs)
{
	return 0;
}

int
tls12_server_hello_send(struct tls12_ctx *ctx, CBB *cbb)
{
	return 0;
}

int
tls12_server_hello_sent(struct tls12_ctx *ctx)
{
	return 0;
}

int
tls12_server_certificate_send(struct tls12_ctx *ctx, CBB *cbb)
{
	return 0;
}

int
tls12_server_certificate_request_send(struct tls12_ctx *ctx, CBB *cbb)
{
	return 0;
}

int
tls12_server_key_exchange_send(struct tls12_ctx *ctx, CBB *cbb)
{
	return 0;
}

int
tls12_server_hello_done_send(struct tls12_ctx *ctx, CBB *cbb)
{
	return 0;
}

int
tls12_server_finished_send(struct tls12_ctx *ctx, CBB *cbb)
{
	return 0;
}

int
tls12_server_new_session_ticket_send(struct tls12_ctx *ctx, CBB *cbb)
{
	return 0;
}

int
tls12_server_finished_sent(struct tls12_ctx *ctx)
{
	return 0;
}
