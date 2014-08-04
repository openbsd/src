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

#include <openssl/ec.h>
#include <openssl/ssl.h>

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
ressl_configure_server(struct ressl *ctx)
{
	EC_KEY *ecdh_key;

	/* XXX - add a configuration option to control versions. */
	if ((ctx->ssl_ctx = SSL_CTX_new(SSLv23_server_method())) == NULL) {
		ressl_set_error(ctx, "ssl context failure");
		goto err;
	}

	if (ressl_configure_keypair(ctx) != 0)
		goto err;

	if (ctx->config->ciphers != NULL) {
		if (SSL_CTX_set_cipher_list(ctx->ssl_ctx,
		    ctx->config->ciphers) != 1) {
			ressl_set_error(ctx, "failed to set ciphers");
			goto err;
		}
	}

	if ((ecdh_key = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1)) == NULL)
		goto err;
	SSL_CTX_set_tmp_ecdh(ctx->ssl_ctx, ecdh_key);
	SSL_CTX_set_options(ctx->ssl_ctx, SSL_OP_SINGLE_ECDH_USE);
	EC_KEY_free(ecdh_key);

	return (0);

err:
	return (-1);
}

int
ressl_listen(struct ressl *ctx, const char *host, const char *port, int af)
{
	if ((ctx->flags & RESSL_SERVER) == 0) {
		ressl_set_error(ctx, "not a server context");
		goto err;
	}

err:
	return (-1);
}

int
ressl_accept(struct ressl *ctx)
{
	if ((ctx->flags & RESSL_SERVER) == 0) {
		ressl_set_error(ctx, "not a server context");
		goto err;
	}

err:
	return (-1);
}

int
ressl_accept_socket(struct ressl *ctx, int socket)
{
	if ((ctx->flags & RESSL_SERVER) == 0) {
		ressl_set_error(ctx, "not a server context");
		goto err;
	}

err:
	return (-1);
}
