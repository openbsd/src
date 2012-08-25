/*
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

static int compress_file_zlib(int, int);
static int uncompress_file_zlib(int, int);
static size_t compress_buffer_zlib(const char *, size_t, char *, size_t);
static size_t uncompress_buffer_zlib(const char *, size_t, char *, size_t);

struct compress_backend	compress_zlib = {
	compress_file_zlib,
	uncompress_file_zlib,
	compress_buffer_zlib,
	uncompress_buffer_zlib
};


static int
compress_file_zlib(int fdin, int fdout)
{
	gzFile	gzfd;
	char	buf[8192];
	int	r, w;

	if (fdin == -1 || fdout == -1)
		return (0);

	gzfd = gzdopen(fdout, "wb");

	while ((r = read(fdin, buf, sizeof(buf)))) {
		if (r == -1)
			return (0);

		w = gzwrite(gzfd, buf, r);
		if (w == 0 || w != r)
			return (0);
	}
	gzclose(gzfd);

	return (1);
}

static int
uncompress_file_zlib(int fdin, int fdout)
{
	gzFile	 gzfd;
	int	 outfd;
	int	 r, w;
	char	 buf[8192];

	if (fdin == -1 || fdout == -1)
		return (0);

	gzfd = gzdopen(fdin, "r");
	while ((r = gzread(gzfd, buf, sizeof(buf)))) {

		if (r == -1)
			return (0);

		w = write(fdout, buf, r);

		if (w == -1 || w != r)
			return (0);
	}
	gzclose(gzfd);

	return (1);
}

static size_t
compress_buffer_zlib(const char *inbuf, size_t inbuflen, char *outbuf, size_t outbuflen)
{
	uLong	compress_bound;
	int	ret;

	compress_bound = compressBound((uLongf) inbuflen);

	if (compress_bound > outbuflen)
		return (0);

	ret = compress((Bytef *) outbuf, (uLongf *) &outbuflen,
		 (const Bytef *) inbuf, (uLong) inbuflen);

	return (ret == Z_OK ? outbuflen : 0);
}

static size_t
uncompress_buffer_zlib(const char *inbuf, size_t inbuflen, char *outbuf, size_t outbuflen)
{
	int	ret;

	ret = uncompress((Bytef *) outbuf, (uLongf *) &outbuflen,
	    (const Bytef *) inbuf, (uLong) inbuflen);

	return (ret == Z_OK ? outbuflen : 0);
}
