/*	$OpenBSD: zipopen.c,v 1.1 2022/10/22 14:41:27 millert Exp $	*/

/*
 * Copyright (c) 2022 Todd C. Miller <Todd.Miller@sudo.ws>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <zlib.h>
#include "compress.h"

#define MINIMUM(a, b)	(((a) < (b)) ? (a) : (b))

/* Signatures for zip file headers we use. */
#define ZIPMAG 0x4b50		/* first two bytes of the zip signature */
#define LOCREM 0x0403		/* remaining two bytes in zip signature */
#define LOCSIG 0x04034b50	/* local file header signature */
#define EXTSIG 0x08074b50	/* extended local header signature */

/* Header sizes. */
#define LOCHDR 30		/* size of local header, including signature */
#define EXTHDR 16		/* size of extended local header, inc sig */

/* General purpose flag bits. */
#define CRPFLG 1		/* flag bit for encrypted entry */
#define EXTFLG 8		/* flag bit for extended local header */

/* Extra field definitions */
#define	EF_ZIP64	0x0001	/* zip64 support */
#define	EF_TIME		0x5455	/* mtime, atime, ctime in UTC ("UT") */
#define EF_IZUNIX	0x5855	/* UNIX extra field ID ("UX") */

#define Z_STORED 0		/* Stored uncompressed in .zip */

struct zip_state {
	z_stream z_stream;	/* libz stream */
	uint8_t  z_buf[Z_BUFSIZE]; /* I/O buffer */
	uint8_t	 z_eof;		/* set if end of input file */
	uint8_t	 z_zip64;	/* 64-bit file sizes */
	uint16_t z_method;	/* Z_DEFLATE or Z_STORED */
	uint16_t z_flags;	/* general purpose flags */
	int	 z_fd;		/* zip file descriptor */
	uint32_t z_time;	/* timestamp (mtime) */
	uint32_t z_crc;		/* crc32 of uncompressed data */
	uint32_t z_ocrc;	/* crc32 of uncompressed data (from header) */
	uint32_t z_hlen;	/* length of the zip header */
	uint64_t z_ulen;	/* uncompressed data length (from header) */
	uint64_t z_total_in;	/* # bytes in */
	uint64_t z_total_out;	/* # bytes out */
};

static int
get_byte(struct zip_state *s)
{
	if (s->z_eof)
		return EOF;

	if (s->z_stream.avail_in == 0) {
		ssize_t nread = read(s->z_fd, s->z_buf, Z_BUFSIZE);
		if (nread <= 0) {
			s->z_eof = 1;
			return EOF;
		}
		s->z_stream.avail_in = nread;
		s->z_stream.next_in = s->z_buf;
	}
	s->z_stream.avail_in--;
	return *s->z_stream.next_in++;
}

static uint16_t
get_uint16(struct zip_state *s)
{
	uint16_t x;

	x  = ((uint16_t)(get_byte(s) & 0xff));
	x |= ((uint16_t)(get_byte(s) & 0xff))<<8;
	return x;
}

static uint32_t
get_uint32(struct zip_state *s)
{
	uint32_t x;

	x  = ((uint32_t)(get_byte(s) & 0xff));
	x |= ((uint32_t)(get_byte(s) & 0xff))<<8;
	x |= ((uint32_t)(get_byte(s) & 0xff))<<16;
	x |= ((uint32_t)(get_byte(s) & 0xff))<<24;
	return x;
}

static uint64_t
get_uint64(struct zip_state *s)
{
	uint64_t x;

	x  = ((uint64_t)(get_byte(s) & 0xff));
	x |= ((uint64_t)(get_byte(s) & 0xff))<<8;
	x |= ((uint64_t)(get_byte(s) & 0xff))<<16;
	x |= ((uint64_t)(get_byte(s) & 0xff))<<24;
	x |= ((uint64_t)(get_byte(s) & 0xff))<<32;
	x |= ((uint64_t)(get_byte(s) & 0xff))<<40;
	x |= ((uint64_t)(get_byte(s) & 0xff))<<48;
	x |= ((uint64_t)(get_byte(s) & 0xff))<<56;
	return x;
}

static int
get_header(struct zip_state *s, char *name, int gotmagic)
{
	int c, got_mtime = 0;
	uint16_t namelen, extlen;
	uint32_t sig;

	/* Check the zip local file header signature. */
	if (!gotmagic) {
		sig = get_uint32(s);
		if (sig != LOCSIG) {
			errno = EFTYPE;
			return -1;
		}
	} else {
		sig = get_uint16(s);
		if (sig != LOCREM) {
			errno = EFTYPE;
			return -1;
		}
	}

	/* Read the local header fields. */
	get_uint16(s);			/* min version */
	s->z_flags = get_uint16(s);	/* general purpose flags */
	s->z_method = get_uint16(s);	/* compression method */
	get_uint32(s);			/* DOS format mtime */
	s->z_ocrc = get_uint32(s);	/* 32-bit CRC */
	get_uint32(s);			/* compressed size */
	s->z_ulen = get_uint32(s);	/* uncompressed size */
	namelen = get_uint16(s);	/* file name length */
	extlen = get_uint16(s);		/* length of extra fields */
	s->z_hlen = LOCHDR;

	/* Encrypted files not supported. */
	if (s->z_flags & CRPFLG) {
		errno = EFTYPE;
		return -1;
	}

	/* Supported compression methods are deflate and store. */
	if (s->z_method != Z_DEFLATED && s->z_method != Z_STORED) {
		errno = EFTYPE;
		return -1;
	}

	/* Store the original file name if present. */
	if (namelen != 0 && name != NULL) {
		const char *ep = name + PATH_MAX - 1;
		for (; namelen > 0; namelen--) {
			if ((c = get_byte(s)) == EOF)
				break;
			s->z_hlen++;
			if (c == '\0')
				break;
			if (name < ep)
				*name++ = c;
		}
		*name = '\0';
	}

	/* Parse extra fields, if any. */
	while (extlen >= 4) {
		uint16_t sig;
		int fieldlen;

		sig = get_uint16(s);
		fieldlen = get_uint16(s);
		s->z_hlen += 4;
		extlen -= 4;

		switch (sig) {
		case EF_ZIP64:
			/* 64-bit file sizes */
			s->z_zip64 = 1;
			if (fieldlen >= 8) {
				s->z_ulen = get_uint64(s);
				s->z_hlen += 8;
				extlen -= 8;
				fieldlen -= 8;
			}
			break;
		case EF_TIME:
			/* UTC timestamps */
			if ((c = get_byte(s)) == EOF)
				break;
			s->z_hlen++;
			extlen--;
			fieldlen--;
			if (c & 1) {
				got_mtime = 1;
				s->z_time = get_uint32(s);
				s->z_hlen += 4;
				extlen -= 4;
				fieldlen -= 4;
			}
			break;
		case EF_IZUNIX:
			/* We prefer EF_TIME if it is present. */
			if (got_mtime)
				break;

			/* skip atime, store mtime. */
			(void)get_uint32(s);
			s->z_time = get_uint32(s);
			s->z_hlen += 8;
			extlen -= 8;
			fieldlen -= 8;
			break;
		default:
			break;
		}

		/* Consume any unparsed bytes in the field. */
		for (; fieldlen > 0; fieldlen--) {
			if (get_byte(s) == EOF)
				break;
			s->z_hlen++;
			extlen--;
		}
	}
	for (; extlen > 0; extlen--) {
		if (get_byte(s) == EOF)
			break;
		s->z_hlen++;
	}

	return 0;
}

void *
zip_ropen(int fd, char *name, int gotmagic)
{
	struct zip_state *s;

	if (fd < 0)
		return NULL;

	if ((s = calloc(1, sizeof(*s))) == NULL)
		return NULL;

	s->z_fd = fd;
	s->z_crc = crc32(0, NULL, 0);

	/* Request a raw inflate, there is no zlib/gzip header present. */
	if (inflateInit2(&s->z_stream, -MAX_WBITS) != Z_OK) {
		free(s);
		return NULL;
	}
	s->z_stream.next_in = s->z_buf;
	s->z_stream.avail_out = sizeof(s->z_buf);

	/* Read the zip header. */
	if (get_header(s, name, gotmagic) != 0) {
		zip_close(s, NULL, NULL, NULL);
		s = NULL;
	}

	return s;
}

static int
zip_store(struct zip_state *s)
{
	int error = Z_OK;
	uLong copy_len;

	if ((int)s->z_stream.avail_in <= 0)
		return s->z_stream.avail_in == 0 ? Z_STREAM_END : Z_DATA_ERROR;

	/* For stored files we rely on z_ulen being set. */
	copy_len = MINIMUM(s->z_stream.avail_out, s->z_stream.avail_in);
	if (copy_len >= s->z_ulen - s->z_total_out) {
		/* Don't copy past the end of the file. */
		copy_len = s->z_ulen - s->z_total_out;
		error = Z_STREAM_END;
	}

	memcpy(s->z_stream.next_out, s->z_stream.next_in, copy_len);
	s->z_stream.next_out += copy_len;
	s->z_stream.avail_out -= copy_len;
	s->z_stream.next_in += copy_len;
	s->z_stream.avail_in -= copy_len;
	s->z_total_in += copy_len;
	s->z_total_out += copy_len;

	return error;
}

int
zip_read(void *cookie, char *buf, int len)
{
	struct zip_state *s = cookie;
	Bytef *ubuf = buf;
	int error = Z_OK;

	s->z_stream.next_out = ubuf;
	s->z_stream.avail_out = len;

	while (error == Z_OK && !s->z_eof && s->z_stream.avail_out != 0) {
		if (s->z_stream.avail_in == 0) {
			ssize_t nread = read(s->z_fd, s->z_buf, Z_BUFSIZE);
			switch (nread) {
			case -1:
				goto bad;
			case 0:
				s->z_eof = 1;
				continue;
			default:
				s->z_stream.avail_in = nread;
				s->z_stream.next_in = s->z_buf;
			}
		}

		if (s->z_method == Z_DEFLATED) {
			/*
			 * Prevent overflow of z_stream.total_{in,out}
			 * which may be 32-bit.
			 */
			uLong prev_total_in = s->z_stream.total_in;
			uLong prev_total_out = s->z_stream.total_out;
			error = inflate(&s->z_stream, Z_NO_FLUSH);
			s->z_total_in += s->z_stream.total_in - prev_total_in;
			s->z_total_out += s->z_stream.total_out - prev_total_out;
		} else {
			/* File stored uncompressed. */
			error = zip_store(s);
		}
	}

	switch (error) {
	case Z_OK:
		s->z_crc = crc32(s->z_crc, ubuf,
		    (uInt)(s->z_stream.next_out - ubuf));
		break;
	case Z_STREAM_END:
		s->z_eof = 1;

		/*
		 * Check CRC and original size.
		 * These may be found in the local header or, if
		 * EXTFLG is set, immediately following the file.
		 */
		s->z_crc = crc32(s->z_crc, ubuf,
		    (uInt)(s->z_stream.next_out - ubuf));

		if (s->z_flags & EXTFLG) {
			/*
			 * Read data descriptor:
			 *  signature 0x08074b50: 4 bytes
			 *  CRC-32: 4 bytes
			 *  compressed size: 4 or 8 bytes
			 *  uncompressed size: 4 or 8 bytes
			 */
			get_uint32(s);
			s->z_ocrc = get_uint32(s);
			if (s->z_zip64) {
				get_uint64(s);
				s->z_ulen = get_uint64(s);
				s->z_hlen += 8;
			} else {
				get_uint32(s);
				s->z_ulen = get_uint32(s);
			}
			s->z_hlen += EXTHDR;
		}
		if (s->z_ulen != s->z_total_out) {
			errno = EIO;
			goto bad;
		}
		if (s->z_ocrc != s->z_crc) {
			errno = EINVAL;
			goto bad;
		}
		break;
	case Z_DATA_ERROR:
		errno = EINVAL;
		goto bad;
	case Z_BUF_ERROR:
		errno = EIO;
		goto bad;
	default:
		goto bad;
	}

	return len - s->z_stream.avail_out;
bad:
	return -1;
}

int
zip_close(void *cookie, struct z_info *info, const char *name, struct stat *sb)
{
	struct zip_state *s = cookie;
	int error = 0;

	if (s == NULL) {
		errno = EINVAL;
		return -1;
	}

	if (info != NULL) {
		info->mtime = s->z_time;
		info->crc = s->z_crc;
		info->hlen = s->z_hlen;
		info->total_in = s->z_total_in;
		info->total_out = s->z_total_out;
	}

	if (s->z_stream.state != NULL) {
		/* inflateEnd() overwrites errno. */
		(void)inflateEnd(&s->z_stream);
	}

	/*
	 * Check for the presence of additional files in the .zip.
	 * Do not remove the original if we cannot extract all the files.
	 */
	s->z_eof = 0;
	if (get_header(s, NULL, 0) == 0) {
		errno = EEXIST;
		error = -1;
	}

	(void)close(s->z_fd);

	free(s);

	return error;
}
