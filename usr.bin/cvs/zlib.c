/*	$OpenBSD: zlib.c,v 1.1 2005/01/13 18:59:03 jfb Exp $	*/
/*
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

#include <sys/param.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "log.h"
#include "cvs.h"
#include "zlib.h"


#define CVS_ZLIB_BUFSIZE  1024



struct cvs_zlib_ctx {
	int       z_level;
	z_stream  z_instrm;
	z_stream  z_destrm;
};


/*
 * cvs_zlib_newctx()
 *
 * Allocate a new ZLIB context structure used for both inflation and deflation
 * of data with compression level <level>, which must be between 0 and 9.  A
 * value of 0 means no compression, and 9 is the highest level of compression.
 */
CVSZCTX*
cvs_zlib_newctx(int level)
{
	CVSZCTX *ctx;

	if ((level < 0) || (level > 9)) {
		cvs_log(LP_ERR, "invalid compression level %d "
		    "(must be between 0 and 9)", level);
		return (NULL);
	}

	ctx = (CVSZCTX *)malloc(sizeof(*ctx));
	if (ctx == NULL) {
		cvs_log(LP_ERRNO, "failed to allocate zlib context");
		return (NULL);
	}
	memset(ctx, 0, sizeof(*ctx));

	ctx->z_level = level;

	ctx->z_instrm.zalloc = Z_NULL;
	ctx->z_instrm.zfree = Z_NULL;
	ctx->z_instrm.opaque = Z_NULL;
	ctx->z_destrm.zalloc = Z_NULL;
	ctx->z_destrm.zfree = Z_NULL;
	ctx->z_destrm.opaque = Z_NULL;

	if ((inflateInit(&(ctx->z_instrm)) != Z_OK) ||
	    (deflateInit(&(ctx->z_destrm), level) != Z_OK)) {
		cvs_log(LP_ERR, "failed to initialize zlib streams");
		free(ctx);
		return (NULL);
	}

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
		free(ctx);
	}
}

/*
 * cvs_zlib_inflate()
 *
 */
int
cvs_zlib_inflate(CVSZCTX *ctx, BUF *dst, u_char *src, size_t slen)
{
	int bytes, ret;
	u_char buf[CVS_ZLIB_BUFSIZE];

	bytes = 0;
	cvs_buf_empty(dst);
	inflateReset(&(ctx->z_instrm));

	ctx->z_instrm.next_in = src;
	ctx->z_instrm.avail_in = slen;

	do {
		ctx->z_instrm.next_out = buf;
		ctx->z_instrm.avail_out = sizeof(buf);

		ret = inflate(&(ctx->z_instrm), Z_FINISH);
		if ((ret == Z_MEM_ERROR) || (ret == Z_BUF_ERROR) ||
		    (ret == Z_STREAM_ERROR) || (ret == Z_DATA_ERROR)) {
			cvs_log(LP_ERR, "inflate error: %s", ctx->z_instrm.msg);
			return (-1);
		}

		cvs_buf_append(dst, buf, ctx->z_instrm.avail_out);
		bytes += sizeof(buf) - ctx->z_instrm.avail_out;

	} while (ret != Z_STREAM_END);

	cvs_log(LP_WARN, "%u bytes decompressed to %d bytes", slen, bytes);

	return (bytes);
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
	int bytes, ret;
	u_char buf[CVS_ZLIB_BUFSIZE];

	bytes = 0;
	cvs_buf_empty(dst);
	deflateReset(&(ctx->z_destrm));

	ctx->z_destrm.next_in = src;
	ctx->z_destrm.avail_in = slen;

	do {
		ctx->z_destrm.next_out = buf;
		ctx->z_destrm.avail_out = sizeof(buf);
		ret = deflate(&(ctx->z_destrm), Z_FINISH);
		if ((ret == Z_STREAM_ERROR) || (ret == Z_BUF_ERROR)) {
#if 0
		if (ret != Z_OK) {
#endif
			cvs_log(LP_ERR, "deflate error: %s", ctx->z_destrm.msg);
			return (-1);
		}

		if (cvs_buf_append(dst, buf,
		    sizeof(buf) - ctx->z_destrm.avail_out) < 0)
			return (-1);
		bytes += sizeof(buf) - ctx->z_destrm.avail_out;
	} while (ret != Z_STREAM_END);

	cvs_log(LP_WARN, "%u bytes compressed to %d bytes", slen, bytes);

	return (bytes);
}
