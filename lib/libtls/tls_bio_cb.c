/* $OpenBSD: tls_bio_cb.c,v 1.14 2016/11/04 15:45:55 jsing Exp $ */
/*
 * Copyright (c) 2016 Tobias Pape <tobias@netshed.de>
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

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include <openssl/bio.h>

#include <tls.h>
#include "tls_internal.h"

static int bio_cb_write(BIO *bio, const char *buf, int num);
static int bio_cb_read(BIO *bio, char *buf, int size);
static int bio_cb_puts(BIO *bio, const char *str);
static long bio_cb_ctrl(BIO *bio, int cmd, long num, void *ptr);
static int bio_cb_new(BIO *bio);
static int bio_cb_free(BIO *bio);

struct bio_cb {
	int (*write_cb)(BIO *bio, const char *buf, int num, void *cb_arg);
	int (*read_cb)(BIO *bio, char *buf, int size, void *cb_arg);
	void *cb_arg;
};

static BIO_METHOD bio_cb_method = {
	.type = BIO_TYPE_MEM,
	.name = "libtls_callbacks",
	.bwrite = bio_cb_write,
	.bread = bio_cb_read,
	.bputs = bio_cb_puts,
	.ctrl = bio_cb_ctrl,
	.create = bio_cb_new,
	.destroy = bio_cb_free,
};

static BIO_METHOD *
bio_s_cb(void)
{
	return (&bio_cb_method);
}

static int
bio_cb_new(BIO *bio)
{
	struct bio_cb *bcb;

	if ((bcb = calloc(1, sizeof(struct bio_cb))) == NULL)
		return (0);

	bio->shutdown = 1;
	bio->init = 1;
	bio->num = -1;
	bio->ptr = bcb;

	return (1);
}

static int
bio_cb_free(BIO *bio)
{
	if (bio == NULL)
		return (0);

	if (bio->shutdown) {
		if ((bio->init) && (bio->ptr != NULL)) {
			free(bio->ptr);
			bio->ptr = NULL;
		}
	}

	return (1);
}

static int
bio_cb_read(BIO *bio, char *buf, int size)
{
	struct bio_cb *bcb = bio->ptr;
	return (bcb->read_cb(bio, buf, size, bcb->cb_arg));
}

static int
bio_cb_write(BIO *bio, const char *buf, int num)
{
	struct bio_cb *bcb = bio->ptr;
	return (bcb->write_cb(bio, buf, num, bcb->cb_arg));
}

static int
bio_cb_puts(BIO *bio, const char *str)
{
	int n;

	n = strlen(str);
	return (bio_cb_write(bio, str, n));
}

static long
bio_cb_ctrl(BIO *bio, int cmd, long num, void *ptr)
{
	long ret = 1;

	switch (cmd) {
	case BIO_CTRL_GET_CLOSE:
		ret = (long)bio->shutdown;
		break;
	case BIO_CTRL_SET_CLOSE:
		bio->shutdown = (int)num;
		break;
	case BIO_CTRL_DUP:
	case BIO_CTRL_FLUSH:
		break;
	case BIO_CTRL_INFO:
	case BIO_CTRL_GET:
	case BIO_CTRL_SET:
	default:
		ret = BIO_ctrl(bio->next_bio, cmd, num, ptr);
	}

	return (ret);
}

static int
tls_bio_write_cb(BIO *bio, const char *buf, int num, void *cb_arg)
{
	struct tls *ctx = cb_arg;
	int rv;

	BIO_clear_retry_flags(bio);
	rv = (ctx->write_cb)(ctx, buf, num, ctx->cb_arg);
	if (rv == TLS_WANT_POLLIN) {
		BIO_set_retry_read(bio);
		rv = -1;
	} else if (rv == TLS_WANT_POLLOUT) {
		BIO_set_retry_write(bio);
		rv = -1;
	}
	return (rv);
}

static int
tls_bio_read_cb(BIO *bio, char *buf, int size, void *cb_arg)
{
	struct tls *ctx = cb_arg;
	int rv;

	BIO_clear_retry_flags(bio);
	rv = (ctx->read_cb)(ctx, buf, size, ctx->cb_arg);
	if (rv == TLS_WANT_POLLIN) {
		BIO_set_retry_read(bio);
		rv = -1;
	} else if (rv == TLS_WANT_POLLOUT) {
		BIO_set_retry_write(bio);
		rv = -1;
	}
	return (rv);
}

static BIO *
tls_get_new_cb_bio(struct tls *ctx)
{
	struct bio_cb *bcb;
	BIO *bio;

	if (ctx->read_cb == NULL || ctx->write_cb == NULL)
		tls_set_errorx(ctx, "no callbacks registered");

	if ((bio = BIO_new(bio_s_cb())) == NULL) {
		tls_set_errorx(ctx, "failed to create callback i/o");
		return (NULL);
	}

	bcb = (struct bio_cb *)bio->ptr;
	bcb->read_cb = tls_bio_read_cb;
	bcb->write_cb = tls_bio_write_cb;
	bcb->cb_arg = ctx;

	return (bio);
}

int
tls_set_cbs(struct tls *ctx, tls_read_cb read_cb, tls_write_cb write_cb,
    void *cb_arg)
{
	int rv = -1;
	BIO *bio;

	ctx->read_cb = read_cb;
	ctx->write_cb = write_cb;
	ctx->cb_arg = cb_arg;

	if ((bio = tls_get_new_cb_bio(ctx)) == NULL) {
		tls_set_errorx(ctx, "failed to create callback i/o");
		goto err;
	}

	SSL_set_bio(ctx->ssl_conn, bio, bio);

	rv = 0;

 err:
	return (rv);
}
