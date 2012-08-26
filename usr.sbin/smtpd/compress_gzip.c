/*	$OpenBSD: compress_gzip.c,v 1.1 2012/08/26 13:38:43 gilles Exp $	*/

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

#define	GZIP_BUFFER_SIZE	8192

static int compress_file_gzip(int, int);
static int uncompress_file_gzip(int, int);
static size_t compress_buffer_gzip(const char *, size_t, char *, size_t);
static size_t uncompress_buffer_gzip(const char *, size_t, char *, size_t);

struct compress_backend	compress_gzip = {
	compress_file_gzip,
	uncompress_file_gzip,
	compress_buffer_gzip,
	uncompress_buffer_gzip
};

static int
compress_file_gzip(int fdin, int fdout)
{
	gzFile	gzfd;
	char	buf[GZIP_BUFFER_SIZE];
	int	r, w;
	int	ret = 0;

	if (fdin == -1 || fdout == -1)
		return (0);

	gzfd = gzdopen(fdout, "wb");
	if (gzfd == NULL)
		return (0);

	while ((r = read(fdin, buf, sizeof(buf))) > 0) {
		w = gzwrite(gzfd, buf, r);
		if (w != r)
			goto end;
	}
	if (r == -1)
		goto end;

	ret = 1;

end:
	gzclose(gzfd);
	return (ret);
}

static int
uncompress_file_gzip(int fdin, int fdout)
{
	gzFile	gzfd;
	char	buf[GZIP_BUFFER_SIZE];
	int	r, w;
	int	ret = 0;

	if (fdin == -1 || fdout == -1)
		return (0);
	
	gzfd = gzdopen(fdin, "r");
	if (gzfd == NULL)
		return (0);

	while ((r = gzread(gzfd, buf, sizeof(buf))) > 0) {
		w = write(fdout, buf, r);
		if (w != r)
			goto end;
	}
	if (r == -1)
		goto end;

	ret = 1;

end:
	gzclose(gzfd);
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
