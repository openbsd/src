/*	$OpenBSD: gzopen.c,v 1.5 2002/12/08 16:07:54 mickey Exp $	*/

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

const char gz_rcsid[] =
    "$OpenBSD: gzopen.c,v 1.5 2002/12/08 16:07:54 mickey Exp $";

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
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
	u_int32_t z_crc;	/* crc32 of uncompressed data */
	char	z_mode;		/* 'w' or 'r' */

} gz_stream;

static const u_char gz_magic[2] = {0x1f, 0x8b}; /* gzip magic header */

static int put_int32(gz_stream *, u_int32_t);
static u_int32_t get_int32(gz_stream *);
static int get_header(gz_stream *);
static int get_byte(gz_stream *);

int
gz_check_header(fd, sb, ofn)
	int fd;
	struct stat *sb;
	const char *ofn;
{
	int f;
	u_char buf[sizeof(gz_magic)];
	off_t off = lseek(fd, 0, SEEK_CUR);

	f = (read(fd, buf, sizeof(buf)) == sizeof(buf) &&
	     !memcmp(buf, gz_magic, sizeof(buf)));

	lseek (fd, off, SEEK_SET);

	return f;
}

void *
gz_open (fd, mode, bits)
	int fd;
	const char *mode;
	int  bits;
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
	s->z_crc = crc32(0L, Z_NULL, 0);
	s->z_mode = mode[0];

	if (s->z_mode == 'w') {
		/* windowBits is passed < 0 to suppress zlib header */
		if (deflateInit2(&(s->z_stream), bits, Z_DEFLATED,
				 -MAX_WBITS, DEF_MEM_LEVEL, 0) != Z_OK) {
			free (s);
			return NULL;
		}
		s->z_stream.next_out = s->z_buf;
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
		u_char buf[10];
		/* Write a very simple .gz header: */
		buf[0] = gz_magic[0];
		buf[1] = gz_magic[1];
		buf[2] = Z_DEFLATED;
		buf[3] = 0 /*flags*/;
		buf[4] = buf[5] = buf[6] = buf[7] = 0 /*time*/;
		buf[8] = 0 /*xflags*/;
		buf[9] = OS_CODE;
		if (write(fd, buf, sizeof(buf)) != sizeof(buf)) {
			gz_close(s);
			s = NULL;
		}
	} else {
		if (get_header(s) != 0) { /* skip the .gz header */
			gz_close (s);
			s = NULL;
		}
	}

	return s;
}

int
gz_close (cookie)
	void *cookie;
{
	gz_stream *s = (gz_stream*)cookie;
	int err = 0;

	if (s == NULL)
		return -1;

	if (s->z_mode == 'w' && (err = gz_flush (s, Z_FINISH)) == Z_OK) {
		if ((err = put_int32 (s, s->z_crc)) == Z_OK)
			err = put_int32 (s, s->z_stream.total_in);
	}

	if (!err && s->z_stream.state != NULL) {
		if (s->z_mode == 'w')
			err = deflateEnd(&s->z_stream);
		else if (s->z_mode == 'r')
			err = inflateEnd(&s->z_stream);
	}

	free(s);

	return err;
}

int
gz_flush (cookie, flush)
    void *cookie;
    int flush;
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

static int
put_int32 (s, x)
	gz_stream *s;
	u_int32_t x;
{
	if (write(s->z_fd, &x, 1) != 1)
		return Z_ERRNO;
	x >>= 8;
	if (write(s->z_fd, &x, 1) != 1)
		return Z_ERRNO;
	x >>= 8;
	if (write(s->z_fd, &x, 1) != 1)
		return Z_ERRNO;
	x >>= 8;
	if (write(s->z_fd, &x, 1) != 1)
		return Z_ERRNO;
	return 0;
}

static int
get_byte(s)
	gz_stream *s;
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
get_int32 (s)
	gz_stream *s;
{
	u_int32_t x;

	x  = ((u_int32_t)(get_byte(s) & 0xff));
	x |= ((u_int32_t)(get_byte(s) & 0xff))<<8;
	x |= ((u_int32_t)(get_byte(s) & 0xff))<<16;
	x |= ((u_int32_t)(get_byte(s) & 0xff))<<24;
	return x;
}

static int
get_header(s)
	gz_stream *s;
{
	int method; /* method byte */
	int flags;  /* flags byte */
	uInt len;
	int c;

	/* Check the gzip magic header */
	for (len = 0; len < 2; len++) {
		c = get_byte(s);
		if (c != gz_magic[len]) {
			errno = EFTYPE;
			return -1;
		}
	}

	method = get_byte(s);
	flags = get_byte(s);
	if (method != Z_DEFLATED || (flags & RESERVED) != 0) {
		errno = EFTYPE;
		return -1;
	}

	/* Discard time, xflags and OS code: */
	for (len = 0; len < 6; len++)
		(void)get_byte(s);

	if ((flags & EXTRA_FIELD) != 0) { /* skip the extra field */
		len  =  (uInt)get_byte(s);
		len += ((uInt)get_byte(s))<<8;
		/* len is garbage if EOF but the loop below will quit anyway */
		while (len-- != 0 && get_byte(s) != EOF)
			;
	}

	if ((flags & ORIG_NAME) != 0) { /* skip the original file name */
		while ((c = get_byte(s)) != 0 && c != EOF) ;
	}

	if ((flags & COMMENT) != 0) {   /* skip the .gz file comment */
		while ((c = get_byte(s)) != 0 && c != EOF) ;
	}

	if ((flags & HEAD_CRC) != 0) {  /* skip the header crc */
		for (len = 0; len < 2; len++) (void)get_byte(s);
	}

	if (s->z_eof) {
		errno = EFTYPE;
		return -1;
	}

	return 0;
}

int
gz_read(cookie, buf, len)
	void *cookie;
	char *buf;
	int len;
{
	gz_stream *s = (gz_stream*)cookie;
	u_char *start = buf; /* starting point for crc computation */

	s->z_stream.next_out = buf;
	s->z_stream.avail_out = len;

	while (s->z_stream.avail_out != 0 && !s->z_eof) {

		if (s->z_stream.avail_in == 0) {

			errno = 0;
			if ((s->z_stream.avail_in =
			     read(s->z_fd, s->z_buf, Z_BUFSIZE)) == 0)
				s->z_eof = 1;
			s->z_stream.next_in = s->z_buf;
		}

		if (inflate(&(s->z_stream), Z_NO_FLUSH) == Z_STREAM_END) {
			/* Check CRC and original size */
			s->z_crc = crc32(s->z_crc, start,
				       (uInt)(s->z_stream.next_out - start));
			start = s->z_stream.next_out;

			if (get_int32(s) != s->z_crc ||
			    get_int32(s) != s->z_stream.total_out) {
			        errno = EIO;
				return -1;
			}
			s->z_eof = 1;
			break;
		}
	}
	s->z_crc = crc32(s->z_crc, start,
			 (uInt)(s->z_stream.next_out - start));

	return (int)(len - s->z_stream.avail_out);
}

int
gz_write(cookie, buf, len)
	void *cookie;
	const char *buf;
	int len;
{
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
}

