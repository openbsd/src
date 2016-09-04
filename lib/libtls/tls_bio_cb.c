/* $ID$ */
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

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "tls.h"
#include "tls_internal.h"

#include <openssl/bio.h>

static int write_cb(BIO *b, const char *buf, int num);
static int read_cb(BIO *b, char *buf, int size);
static int puts_cb(BIO *b, const char *str);
static long ctrl_cb(BIO *b, int cmd, long num, void *ptr);
static int new_cb(BIO *b);
static int free_cb(BIO *data);

struct bio_cb_st {
	int (*write_cb)(BIO *h, const char *buf, int num, void *cb_arg);
	int (*read_cb)(BIO *h, char *buf, int size, void *cb_arg);
	void *cb_arg;
};

static BIO_METHOD cb_method = {
	.type = BIO_TYPE_MEM,
	.name = "libtls_callbacks",
	.bwrite = write_cb,
	.bread = read_cb,
	.bputs = puts_cb,
	.ctrl = ctrl_cb,
	.create = new_cb,
	.destroy = free_cb
};

static BIO_METHOD *
bio_s_cb(void)
{
	return (&cb_method);
}

static int
bio_set_write_cb(BIO *bi,
    int (*write_cb)(BIO *h, const char *buf, int num, void *cb_arg))
{
	struct bio_cb_st *b;
	b = (struct bio_cb_st *)bi->ptr;
	b->write_cb = write_cb;
	return (0);
}

static int
bio_set_read_cb(BIO *bi,
    int (*read_cb)(BIO *h, char *buf, int size, void *cb_arg))
{
	struct bio_cb_st *b;
	b = (struct bio_cb_st *)bi->ptr;
	b->read_cb = read_cb;
	return (0);
}

static int
bio_set_cb_arg(BIO *bi, void *cb_arg)
{
	struct bio_cb_st *b;
	b = (struct bio_cb_st *)bi->ptr;
	b->cb_arg = cb_arg;
	return (0);
}

static int
new_cb(BIO *bi)
{
	struct bio_cb_st *bcb;

	bcb = calloc(1, sizeof(struct bio_cb_st));
	if (bcb == NULL)
		return (0);

	bi->shutdown = 1;
	bi->init = 1;
	bi->num = -1;
	bi->ptr = (char *)bcb;

	return (1);
}

static int
free_cb(BIO *bi)
{
	if (bi == NULL)
		return (0);

	if (bi->shutdown) {
		if ((bi->init) && (bi->ptr != NULL)) {
			struct bio_cb_st *b;
			b = (struct bio_cb_st *)bi->ptr;
			free(b);
			bi->ptr = NULL;
		}
	}

	return (1);
}

static int
read_cb(BIO *b, char *buf, int size)
{
	struct bio_cb_st *bcb = b->ptr;
	return (bcb->read_cb(b, buf, size, bcb->cb_arg));
}

static int
write_cb(BIO *b, const char *buf, int num)
{
	struct bio_cb_st *bcb = b->ptr;
	return (bcb->write_cb(b, buf, num, bcb->cb_arg));
}

static int
puts_cb(BIO *b, const char *str)
{
	int n;

	n = strlen(str);
	return (write_cb(b, str, n));
}

static long
ctrl_cb(BIO *b, int cmd, long num, void *ptr)
{
	long ret = 1;

	switch (cmd) {
	case BIO_CTRL_GET_CLOSE:
		ret = (long)b->shutdown;
		break;
	case BIO_CTRL_SET_CLOSE:
		b->shutdown = (int)num;
		break;
	case BIO_CTRL_DUP:
		break;
	case BIO_CTRL_INFO:
	case BIO_CTRL_GET:
	case BIO_CTRL_SET:
	default:
		ret = BIO_ctrl(b->next_bio, cmd, num, ptr);
	}

	return (ret);
}

static int
tls_bio_write_cb(BIO *h, const char *buf, int num, void *cb_arg)
{
	struct tls *ctx = cb_arg;
	return (ctx->write_cb)(ctx, buf, num, ctx->cb_arg);
}

static int
tls_bio_read_cb(BIO *h, char *buf, int size, void *cb_arg)
{
	struct tls *ctx = cb_arg;
	return (ctx->read_cb)(ctx, buf, size, ctx->cb_arg);
}

static BIO *
tls_get_new_cb_bio(struct tls *ctx)
{
	BIO *bcb;
	if (ctx->read_cb == NULL || ctx->write_cb == NULL)
		tls_set_errorx(ctx, "no callbacks registered");

	bcb = BIO_new(bio_s_cb());
	if (bcb == NULL) {
		tls_set_errorx(ctx, "failed to create callback i/o");
		return (NULL);
	}

	bio_set_write_cb(bcb, tls_bio_write_cb);
	bio_set_read_cb(bcb, tls_bio_read_cb);
	bio_set_cb_arg(bcb, ctx);

	return (bcb);
}

int
tls_set_cbs(struct tls *ctx, tls_read_cb read_cb, tls_write_cb write_cb,
    void *cb_arg)
{
	int rv = -1;
	BIO *bcb;
	ctx->read_cb = read_cb;
	ctx->write_cb = write_cb;
	ctx->cb_arg = cb_arg;

	bcb = tls_get_new_cb_bio(ctx);
	if (bcb == NULL) {
		tls_set_errorx(ctx, "failed to create callback i/o");
		goto err;
	}

	SSL_set_bio(ctx->ssl_conn, bcb, bcb);

	rv = 0;

 err:
	return (rv);
}
