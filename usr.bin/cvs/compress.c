/*	$OpenBSD: compress.c,v 1.6 2007/02/22 06:42:09 otto Exp $	*/
/*
 * Copyright (c) 2006 Patrick Latifi <pat@openbsd.org>
 * Copyright (c) 2005 Jean-Francois Brousseau <jfb@openbsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL  DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <zlib.h>

#include "cvs.h"
#include "compress.h"

#define CVS_ZLIB_BUFSIZE	1024

struct cvs_zlib_ctx {
	int		z_level;
	z_stream	z_instrm;
	z_stream	z_destrm;
};

struct zlib_ioctx {
	z_stream *stream;
	int	(*reset)(z_stream *);
	int	(*io)(z_stream *, int);
	int	ioflags;
};

static int cvs_zlib_io(struct zlib_ioctx *, BUF *, u_char *, size_t);

/*
 * cvs_zlib_newctx()
 *
 * Allocate a new ZLIB context structure used for both inflation and deflation
 * of data with compression level <level>, which must be between 0 and 9.  A
 * value of 0 means no compression, and 9 is the highest level of compression.
 */
CVSZCTX *
cvs_zlib_newctx(int level)
{
	CVSZCTX *ctx;

	if (level < 0 || level > 9)
		fatal("invalid compression level %d (must be between 0 and 9)",
		    level);

	ctx = xcalloc(1, sizeof(*ctx));

	ctx->z_level = level;

	ctx->z_instrm.zalloc = Z_NULL;
	ctx->z_instrm.zfree = Z_NULL;
	ctx->z_instrm.opaque = Z_NULL;
	ctx->z_destrm.zalloc = Z_NULL;
	ctx->z_destrm.zfree = Z_NULL;
	ctx->z_destrm.opaque = Z_NULL;

	if (inflateInit(&(ctx->z_instrm)) != Z_OK ||
	    deflateInit(&(ctx->z_destrm), level) != Z_OK)
		fatal("failed to initialize zlib streams");

	return (ctx);
}


/*
 * cvs_zlib_free()
 *
 * Free a ZLIB context previously allocated with cvs_zlib_newctx().
 */
void
cvs_zlib_free(CVSZCTX *ctx)
{
	if (ctx != NULL) {
		(void)inflateEnd(&(ctx->z_instrm));
		(void)deflateEnd(&(ctx->z_destrm));
		xfree(ctx);
	}
}

/*
 * cvs_zlib_inflate()
 *
 * Decompress the first <slen> bytes of <src> using the zlib context <ctx> and
 * store the resulting data in <dst>.
 * Returns the number of bytes inflated on success, or -1 on failure.
 */
int
cvs_zlib_inflate(CVSZCTX *ctx, BUF *dst, u_char *src, size_t slen)
{
	struct zlib_ioctx zio;

	zio.stream = &ctx->z_instrm;
	zio.reset = inflateReset;
	zio.io = inflate;
	zio.ioflags = Z_FINISH;

	return cvs_zlib_io(&zio, dst, src, slen);
}

/*
 * cvs_zlib_deflate()
 *
 * Compress the first <slen> bytes of <src> using the zlib context <ctx> and
 * store the resulting data in <dst>.
 * Returns the number of bytes deflated on success, or -1 on failure.
 */
int
cvs_zlib_deflate(CVSZCTX *ctx, BUF *dst, u_char *src, size_t slen)
{
	struct zlib_ioctx zio;

	zio.stream = &ctx->z_destrm;
	zio.reset = deflateReset;
	zio.io = deflate;
	zio.ioflags = Z_FINISH;

	return cvs_zlib_io(&zio, dst, src, slen);
}

static int
cvs_zlib_io(struct zlib_ioctx *zio, BUF *dst, u_char *src, size_t slen)
{
	int bytes, ret;
	u_char buf[CVS_ZLIB_BUFSIZE];
	z_stream *zstream = zio->stream;

	bytes = 0;
	cvs_buf_empty(dst);
	if ((*zio->reset)(zstream) == Z_STREAM_ERROR)
		fatal("%s error: %s", (zio->reset == inflateReset) ?
		    "inflate" : "deflate", zstream->msg);

	zstream->next_in = src;
	zstream->avail_in = slen;

	do {
		zstream->next_out = buf;
		zstream->avail_out = sizeof(buf);
		ret = (*zio->io)(zstream, zio->ioflags);
		if (ret == Z_MEM_ERROR || ret == Z_STREAM_ERROR ||
		    ret == Z_BUF_ERROR || ret == Z_DATA_ERROR)
			fatal("%s error: %s", (zio->reset == inflateReset) ?
			    "inflate" : "deflate", zstream->msg);

		if (cvs_buf_append(dst, buf,
		    sizeof(buf) - zstream->avail_out) < 0)
			return (-1);
		bytes += sizeof(buf) - zstream->avail_out;
	} while (ret != Z_STREAM_END);

	return (bytes);
}
