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

#include <sys/socket.h>

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include <ressl.h>
#include "ressl_internal.h"

extern struct ressl_config ressl_config_default;

int
ressl_init(void)
{
	static int ressl_initialised = 0;

	if (ressl_initialised)
		return (0);

	SSL_load_error_strings();
	SSL_library_init();

	ressl_initialised = 1;

	return (0);
}

const char *
ressl_error(struct ressl *ctx)
{
	return ctx->errmsg;
}

int
ressl_set_error(struct ressl *ctx, char *fmt, ...)
{
        va_list ap;
        int rv;

	ctx->err = errno;
	free(ctx->errmsg);
	ctx->errmsg = NULL;

        va_start(ap, fmt);
        rv = vasprintf(&ctx->errmsg, fmt, ap);
        va_end(ap);

	return (rv);
}

struct ressl *
ressl_new(struct ressl_config *config)
{
	struct ressl *ctx;

	if ((ctx = calloc(1, sizeof(*ctx))) == NULL)
		return (NULL);

	if (config == NULL)
		config = &ressl_config_default;

	ctx->config = config;

	ressl_reset(ctx);

	return (ctx);
}

void
ressl_free(struct ressl *ctx)
{
	if (ctx == NULL)
		return;
	ressl_reset(ctx);
	free(ctx);
}

void
ressl_reset(struct ressl *ctx)
{
	/* SSL_free frees the SSL context. */
	if (ctx->ssl_conn != NULL)
		SSL_free(ctx->ssl_conn);
	else
		SSL_CTX_free(ctx->ssl_ctx);

	ctx->ssl_conn = NULL;
	ctx->ssl_ctx = NULL;

	ctx->socket = -1;

	ctx->err = 0;
	free(ctx->errmsg);
	ctx->errmsg = NULL;
}

int
ressl_read(struct ressl *ctx, char *buf, size_t buflen, size_t *outlen)
{
	int ret;

	/* XXX - handle async/non-blocking. */
	ret = SSL_read(ctx->ssl_conn, buf, buflen);
	if (ret <= 0) {
		ret = SSL_get_error(ctx->ssl_conn, ret);
		if (ret == SSL_ERROR_WANT_READ)
			return (-2);
		ressl_set_error(ctx, "read failed: %i", ret);
		return (-1);
	}
	*outlen = (size_t)ret;
	return (0);
}

int
ressl_write(struct ressl *ctx, const char *buf, size_t buflen, size_t *outlen)
{
	int ret;

	/* XXX - handle async/non-blocking. */
	ret = SSL_write(ctx->ssl_conn, buf, buflen);
	if (ret < 0) {
		ressl_set_error(ctx, "write failed %d",
		    SSL_get_error(ctx->ssl_conn, ret));
		return (-1);
	}
	*outlen = (size_t)ret;
	return (0);
}

int
ressl_close(struct ressl *ctx)
{
	/* XXX - handle case where multiple calls are required. */
	if (ctx->ssl_conn != NULL) {
		if (SSL_shutdown(ctx->ssl_conn) == -1) {
			ressl_set_error(ctx, "SSL shutdown failed");
			goto err;
		}
	}

	if (ctx->socket != -1) {
		if (shutdown(ctx->socket, SHUT_RDWR) != 0) {
			ressl_set_error(ctx, "shutdown");
			goto err;
		}
		if (close(ctx->socket) != 0) {
			ressl_set_error(ctx, "close");
			goto err;
		}
		ctx->socket = -1;
	}

	return (0);

err:
	return (-1);
}
