/*	$OpenBSD: compress_gzip.c,v 1.3 2012/08/30 22:38:22 chl Exp $	*/

/*
 * Copyright (c) 2012 Gilles Chehade <gilles@openbsd.org>
 * Copyright (c) 2012 Charles Longeau <chl@openbsd.org>
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

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <imsg.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <zlib.h>

#include "smtpd.h"
#include "log.h"

#define	GZIP_BUFFER_SIZE	16384

static int compress_file_gzip(FILE *, FILE *);
static int uncompress_file_gzip(FILE *, FILE *);
static size_t compress_buffer_gzip(const char *, size_t, char *, size_t);
static size_t uncompress_buffer_gzip(const char *, size_t, char *, size_t);

struct compress_backend	compress_gzip = {
	compress_file_gzip,
	uncompress_file_gzip,
	compress_buffer_gzip,
	uncompress_buffer_gzip
};

static int
compress_file_gzip(FILE *in, FILE *out)
{
	gzFile	gzf;
	char	ibuf[GZIP_BUFFER_SIZE];
	int	r, w;
	int	ret = 0;

	if (in == NULL || out == NULL)
		return (0);

	gzf = gzdopen(fileno(out), "wb");
	if (gzf == NULL)
		return (0);

	while ((r = fread(ibuf, 1, GZIP_BUFFER_SIZE, in)) != 0) {
		if ((w = gzwrite(gzf, ibuf, r)) != r)
			goto end;
	}
	if (! feof(in))
		goto end;

	ret = 1;

end:
	gzclose(gzf);
	return (ret);
}

static int
uncompress_file_gzip(FILE *in, FILE *out)
{
	gzFile	gzf;
	char	obuf[GZIP_BUFFER_SIZE];
	int	r, w;
	int	ret = 0;

	if (in == NULL || out == NULL)
		return (0);

	gzf = gzdopen(fileno(in), "r");
	if (gzf == NULL)
		return (0);

	while ((r = gzread(gzf, obuf, sizeof(obuf))) > 0) {
		if  ((w = fwrite(obuf, r, 1, out)) != 1)
			goto end;
	}
	if (! gzeof(gzf))
		goto end;

	ret = 1;

end:
	gzclose(gzf);
	return (ret);
}

static size_t
compress_buffer_gzip(const char *in, size_t inlen, char *out, size_t outlen)
{
	z_stream	strm;
	size_t		ret = 0;

	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;

	ret = deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
	    (15+16), 8, Z_DEFAULT_STRATEGY);
	if (ret != Z_OK)
		return 0;

	strm.avail_in = inlen;
	strm.next_in = in;
	strm.avail_out = outlen;
	strm.next_out = out;

	ret = deflate(&strm, Z_FINISH);
	if (ret != Z_STREAM_END)
		goto end;

	ret = strm.total_out;

end:
	(void)deflateEnd(&strm);
	return ret;
}

static size_t
uncompress_buffer_gzip(const char *in, size_t inlen, char *out, size_t outlen)
{
	z_stream	strm;
	size_t		ret = 0;

	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	strm.avail_in = 0;
	strm.next_in = Z_NULL;

	ret = inflateInit2(&strm, (15+16));
	if (ret != Z_OK)
		return ret;

	strm.avail_in = inlen;
	strm.next_in = in;
	strm.avail_out = outlen;
	strm.next_out = out;

	ret = inflate(&strm, Z_FINISH);
	if (ret != Z_STREAM_END)
		goto end;

	ret = strm.total_out;

end:
	(void)inflateEnd(&strm);
	return ret;
}
