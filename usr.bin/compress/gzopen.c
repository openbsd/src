/*	$OpenBSD: gzopen.c,v 1.22 2004/09/06 21:24:11 mickey Exp $	*/

/*
 * Copyright (c) 1997 Michael Shalayeff
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
/* this is partially derived from the zlib's gzio.c file, so the notice: */
/*
  zlib.h -- interface of the 'zlib' general purpose compression library
  version 1.0.4, Jul 24th, 1996.

  Copyright (C) 1995-1996 Jean-loup Gailly and Mark Adler

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  Jean-loup Gailly        Mark Adler
  gzip@prep.ai.mit.edu    madler@alumni.caltech.edu


  The data format used by the zlib library is described by RFCs (Request for
  Comments) 1950 to 1952 in the files ftp://ds.internic.net/rfc/rfc1950.txt
  (zlib format), rfc1951.txt (deflate format) and rfc1952.txt (gzip format).
*/

#ifndef SMALL
const char gz_rcsid[] =
    "$OpenBSD: gzopen.c,v 1.22 2004/09/06 21:24:11 mickey Exp $";
#endif

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <zlib.h>
#include "compress.h"

/* gzip flag byte */
#define ASCII_FLAG   0x01 /* bit 0 set: file probably ascii text */
#define HEAD_CRC     0x02 /* bit 1 set: header CRC present */
#define EXTRA_FIELD  0x04 /* bit 2 set: extra field present */
#define ORIG_NAME    0x08 /* bit 3 set: original file name present */
#define COMMENT      0x10 /* bit 4 set: file comment present */
#define RESERVED     0xE0 /* bits 5..7: reserved */

#define DEF_MEM_LEVEL 8
#define OS_CODE 0x03 /* unix */

typedef
struct gz_stream {
	int	z_fd;		/* .gz file */
	z_stream z_stream;	/* libz stream */
	int	z_eof;		/* set if end of input file */
	u_char	z_buf[Z_BUFSIZE]; /* i/o buffer */
	u_int32_t z_time;	/* timestamp (mtime) */
	u_int32_t z_hlen;	/* length of the gz header */
	u_int32_t z_crc;	/* crc32 of uncompressed data */
	char	z_mode;		/* 'w' or 'r' */

} gz_stream;

static const u_char gz_magic[2] = {0x1f, 0x8b}; /* gzip magic header */

static int put_int32(gz_stream *, u_int32_t);
static u_int32_t get_int32(gz_stream *);
static int get_header(gz_stream *, char *, int);
static int put_header(gz_stream *, char *, u_int32_t, int);
static int get_byte(gz_stream *);

void *
gz_open(int fd, const char *mode, char *name, int bits,
    u_int32_t mtime, int gotmagic)
{
	gz_stream *s;

	if (fd < 0 || !mode)
		return NULL;

	if ((mode[0] != 'r' && mode[0] != 'w') || mode[1] != '\0' ||
	    bits < 0 || bits > Z_BEST_COMPRESSION) {
		errno = EINVAL;
		return NULL;
	}
	if ((s = (gz_stream *)calloc(1, sizeof(gz_stream))) == NULL)
		return NULL;

	s->z_stream.zalloc = (alloc_func)0;
	s->z_stream.zfree = (free_func)0;
	s->z_stream.opaque = (voidpf)0;
	s->z_stream.next_in = Z_NULL;
	s->z_stream.next_out = Z_NULL;
	s->z_stream.avail_in = s->z_stream.avail_out = 0;
	s->z_fd = 0;
	s->z_eof = 0;
	s->z_time = 0;
	s->z_hlen = 0;
	s->z_crc = crc32(0L, Z_NULL, 0);
	s->z_mode = mode[0];

	if (s->z_mode == 'w') {
#ifndef SMALL
		/* windowBits is passed < 0 to suppress zlib header */
		if (deflateInit2(&(s->z_stream), bits, Z_DEFLATED,
				 -MAX_WBITS, DEF_MEM_LEVEL, 0) != Z_OK) {
			free (s);
			return NULL;
		}
		s->z_stream.next_out = s->z_buf;
#else
		return (NULL);
#endif
	} else {
		if (inflateInit2(&(s->z_stream), -MAX_WBITS) != Z_OK) {
			free (s);
			return NULL;
		}
		s->z_stream.next_in = s->z_buf;
	}
	s->z_stream.avail_out = Z_BUFSIZE;

	errno = 0;
	s->z_fd = fd;

	if (s->z_mode == 'w') {
		/* write the .gz header */
		if (put_header(s, name, mtime, bits) != 0) {
			gz_close(s, NULL);
			s = NULL;
		}
	} else {
		/* read the .gz header */
		if (get_header(s, name, gotmagic) != 0) {
			gz_close(s, NULL);
			s = NULL;
		}
	}

	return s;
}

int
gz_close(void *cookie, struct z_info *info)
{
	gz_stream *s = (gz_stream*)cookie;
	int err = 0;

	if (s == NULL)
		return -1;

#ifndef SMALL
	if (s->z_mode == 'w' && (err = gz_flush (s, Z_FINISH)) == Z_OK) {
		if ((err = put_int32 (s, s->z_crc)) == Z_OK) {
			s->z_hlen += sizeof(int32_t);
			if ((err = put_int32 (s, s->z_stream.total_in)) == Z_OK)
				s->z_hlen += sizeof(int32_t);
		}
	}
#endif
	if (!err && s->z_stream.state != NULL) {
		if (s->z_mode == 'w')
#ifndef SMALL
			err = deflateEnd(&s->z_stream);
#else
			err = -1;
#endif
		else if (s->z_mode == 'r')
			err = inflateEnd(&s->z_stream);
	}

	if (info != NULL) {
		info->mtime = s->z_time;
		info->crc = s->z_crc;
		info->hlen = s->z_hlen;
		info->total_in = (off_t)s->z_stream.total_in;
		info->total_out = (off_t)s->z_stream.total_out;
	}

	if (!err)
		err = close(s->z_fd);
	else
		(void)close(s->z_fd);

	free(s);

	return err;
}

#ifndef SMALL
int
gz_flush(void *cookie, int flush)
{
	gz_stream *s = (gz_stream*)cookie;
	size_t len;
	int done = 0;
	int err;

	if (s == NULL || s->z_mode != 'w') {
		errno = EBADF;
		return Z_ERRNO;
	}

	s->z_stream.avail_in = 0; /* should be zero already anyway */

	for (;;) {
		len = Z_BUFSIZE - s->z_stream.avail_out;

		if (len != 0) {
			if (write(s->z_fd, s->z_buf, len) != len)
				return Z_ERRNO;
			s->z_stream.next_out = s->z_buf;
			s->z_stream.avail_out = Z_BUFSIZE;
		}
		if (done)
			break;
		if ((err = deflate(&(s->z_stream), flush)) != Z_OK &&
		    err != Z_STREAM_END)
			return err;

		/* deflate has finished flushing only when it hasn't
		 * used up all the available space in the output buffer
		 */
		done = (s->z_stream.avail_out != 0 || err == Z_STREAM_END);
	}
	return 0;
}
#endif

static int
put_int32(gz_stream *s, u_int32_t x)
{
	u_int32_t y = htole32(x);

	if (write(s->z_fd, &y, sizeof(y)) != sizeof(y))
		return Z_ERRNO;
	return 0;
}

static int
get_byte(gz_stream *s)
{
	if (s->z_eof)
		return EOF;

	if (s->z_stream.avail_in == 0) {
		errno = 0;
		s->z_stream.avail_in = read(s->z_fd, s->z_buf, Z_BUFSIZE);
		if (s->z_stream.avail_in <= 0) {
			s->z_eof = 1;
			return EOF;
		}
		s->z_stream.next_in = s->z_buf;
	}
	s->z_stream.avail_in--;
	return *s->z_stream.next_in++;
}

static u_int32_t
get_int32(gz_stream *s)
{
	u_int32_t x;

	x  = ((u_int32_t)(get_byte(s) & 0xff));
	x |= ((u_int32_t)(get_byte(s) & 0xff))<<8;
	x |= ((u_int32_t)(get_byte(s) & 0xff))<<16;
	x |= ((u_int32_t)(get_byte(s) & 0xff))<<24;
	return x;
}

static int
get_header(gz_stream *s, char *name, int gotmagic)
{
	int method; /* method byte */
	int flags;  /* flags byte */
	char *ep;
	uInt len;
	int c;

	/* Check the gzip magic header */
	if (!gotmagic) {
		for (len = 0; len < 2; len++) {
			c = get_byte(s);
			if (c != gz_magic[len]) {
				errno = EFTYPE;
				return -1;
			}
		}
	}

	method = get_byte(s);
	flags = get_byte(s);
	if (method != Z_DEFLATED || (flags & RESERVED) != 0) {
		errno = EFTYPE;
		return -1;
	}

	/* Stash timestamp (mtime) */
	s->z_time = get_int32(s);

	/* Discard xflags and OS code */
	(void)get_byte(s);
	(void)get_byte(s);

	s->z_hlen = 10; /* magic, method, flags, time, xflags, OS code */
	if ((flags & EXTRA_FIELD) != 0) { /* skip the extra field */
		len  =  (uInt)get_byte(s);
		len += ((uInt)get_byte(s))<<8;
		s->z_hlen += 2;
		/* len is garbage if EOF but the loop below will quit anyway */
		while (len-- != 0 && get_byte(s) != EOF)
			s->z_hlen++;
	}

	if ((flags & ORIG_NAME) != 0) { /* read/save the original file name */
		if ((ep = name) != NULL)
			ep += MAXPATHLEN - 1;
		while ((c = get_byte(s)) != EOF) {
			s->z_hlen++;
			if (c == '\0')
				break;
			if (name < ep)
				*name++ = c;
		}
		if (name != NULL)
			*name = '\0';
	}

	if ((flags & COMMENT) != 0) {   /* skip the .gz file comment */
		while ((c = get_byte(s)) != EOF) {
			s->z_hlen++;
			if (c == '\0')
				break;
		}
	}

	if ((flags & HEAD_CRC) != 0) {  /* skip the header crc */
		(void)get_byte(s);
		(void)get_byte(s);
		s->z_hlen += 2;
	}

	if (s->z_eof) {
		errno = EFTYPE;
		return -1;
	}

	return 0;
}

static int
put_header(gz_stream *s, char *name, u_int32_t mtime, int bits)
{
	struct iovec iov[2];
	u_char buf[10];

	buf[0] = gz_magic[0];
	buf[1] = gz_magic[1];
	buf[2] = Z_DEFLATED;
	buf[3] = name ? ORIG_NAME : 0;
	buf[4] = mtime & 0xff;
	buf[5] = (mtime >> 8) & 0xff;
	buf[6] = (mtime >> 16) & 0xff;
	buf[7] = (mtime >> 24) & 0xff;
	buf[8] = bits == 1 ? 4 : bits == 9 ? 2 : 0;	/* xflags */
	buf[9] = OS_CODE;
	iov[0].iov_base = buf;
	iov[0].iov_len = sizeof(buf);
	s->z_hlen = sizeof(buf);

	if (name != NULL) {
		iov[1].iov_base = name;
		iov[1].iov_len = strlen(name) + 1;
		s->z_hlen += iov[1].iov_len;
	}
	if (writev(s->z_fd, iov, name ? 2 : 1) == -1)
		return (-1);
	return (0);
}

int
gz_read(void *cookie, char *buf, int len)
{
	gz_stream *s = (gz_stream*)cookie;
	u_char *start = buf; /* starting point for crc computation */
	int error = Z_OK;

	s->z_stream.next_out = buf;
	s->z_stream.avail_out = len;

	while (error == Z_OK && !s->z_eof && s->z_stream.avail_out != 0) {

		if (s->z_stream.avail_in == 0) {

			errno = 0;
			if ((s->z_stream.avail_in =
			    read(s->z_fd, s->z_buf, Z_BUFSIZE)) == 0)
				s->z_eof = 1;
			s->z_stream.next_in = s->z_buf;
		}

		error = inflate(&(s->z_stream), Z_NO_FLUSH);
		if (error == Z_STREAM_END) {
			/* Check CRC and original size */
			s->z_crc = crc32(s->z_crc, start,
			    (uInt)(s->z_stream.next_out - start));
			start = s->z_stream.next_out;

			if (get_int32(s) != s->z_crc) {
				errno = EINVAL;
				return -1;
			}
			if (get_int32(s) != (u_int32_t)s->z_stream.total_out) {
				errno = EIO;
				return -1;
			}
			s->z_hlen += 2 * sizeof(int32_t);
			/* Check for the existence of an appended file. */
			if (get_header(s, NULL, 0) != 0) {
				s->z_eof = 1;
				break;
			}
			inflateReset(&(s->z_stream));
			s->z_crc = crc32(0L, Z_NULL, 0);
			error = Z_OK;
		}
	}
	s->z_crc = crc32(s->z_crc, start,
	    (uInt)(s->z_stream.next_out - start));
	len -= s->z_stream.avail_out;

	return (len);
}

int
gz_write(void *cookie, const char *buf, int len)
{
#ifndef SMALL
	gz_stream *s = (gz_stream*)cookie;

	s->z_stream.next_in = (char *)buf;
	s->z_stream.avail_in = len;

	while (s->z_stream.avail_in != 0) {
		if (s->z_stream.avail_out == 0) {
			if (write(s->z_fd, s->z_buf, Z_BUFSIZE) != Z_BUFSIZE)
				break;
			s->z_stream.next_out = s->z_buf;
			s->z_stream.avail_out = Z_BUFSIZE;
		}
		if (deflate(&(s->z_stream), Z_NO_FLUSH) != Z_OK)
			break;
	}
	s->z_crc = crc32(s->z_crc, buf, len);

	return (int)(len - s->z_stream.avail_in);
#endif
}
