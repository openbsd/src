/*
 * Copyright (c) 2014 Joel Sing <jsing@openbsd.org>
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

#include "ressl_internal.h"

struct ressl *
ressl_server(void)
{
	struct ressl *ctx;

	if ((ctx = ressl_new()) == NULL)
		return (NULL);

	ctx->flags |= RESSL_SERVER;

	return (ctx);
}

struct ressl *
ressl_server_conn(struct ressl *ctx)
{
	struct ressl *conn_ctx;

	if ((conn_ctx = ressl_new()) == NULL)
		return (NULL);

	conn_ctx->flags |= RESSL_SERVER_CONN;

	return (conn_ctx);
}

int
ressl_listen(struct ressl *ctx, const char *host, const char *port, int af)
{
	if ((ctx->flags & RESSL_SERVER) == 0) {
		ressl_set_error(ctx, "not a server context");
		goto err;
	}

err:
	return (1);
}

int
ressl_accept(struct ressl *ctx)
{
	if ((ctx->flags & RESSL_SERVER) == 0) {
		ressl_set_error(ctx, "not a server context");
		goto err;
	}

err:
	return (1);
}

int
ressl_accept_socket(struct ressl *ctx, int socket)
{
	if ((ctx->flags & RESSL_SERVER) == 0) {
		ressl_set_error(ctx, "not a server context");
		goto err;
	}

err:
	return (1);
}
