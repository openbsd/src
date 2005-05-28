/*
 * gzip.h
 *
 * Copyright (c) 2001 Dug Song <dugsong@arbor.net>
 * Copyright (c) 2001 Arbor Networks, Inc.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 * 
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *   3. The names of the copyright holders may not be used to endorse or
 *      promote products derived from this software without specific
 *      prior written permission.
 * 
 *   THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 *   INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 *   AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 *   THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 *   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 *   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 *   OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *   OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 *   ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Vendor: gzip.h,v 1.2 2005/04/01 16:47:31 dugsong Exp $
 */

#ifndef GZIP_H
#define GZIP_H

/* RFC 1952 is b0rked! This is from gzip-1.2.4's algorithm.doc... */

/* Magic header */
#define GZIP_MAGIC		"\037\213"

/* Compression methods */
#define GZIP_MSTORED		0
#define GZIP_MCOMPRESS		1
#define GZIP_MPACKED		2
#define GZIP_MLZHED		3
#define GZIP_MDEFLATE		8

/* Flags */
#define GZIP_FTEXT		0x01
#define GZIP_FCONT		0x02	/* never set by gzip-1.2.4 */
#define GZIP_FEXTRA		0x04
#define GZIP_FNAME		0x08
#define GZIP_FCOMMENT		0x10
#define GZIP_FENCRYPT		0x20
#define GZIP_FRESERVED		0xC0

#define GZIP_FENCRYPT_LEN	12

#define GZSIG_ID		"GS"
#define GZSIG_VERSION		1

struct gzsig_data {
	u_char	version;
#ifdef COMMENT_ONLY
	u_char	signature[];
#endif
};

/*
 * Note: all number fields below are in little-endian byte order.
 */

struct gzip_xfield {
	u_short	len;
	struct gzip_subfield {
		u_char	id[2];
		u_short	len;
#ifdef COMMENT_ONLY
		u_char	data[];
#endif
	} subfield;
};

struct gzip_header {
	u_char		magic[2];
	u_char		method;
	u_char		flags;
	u_char		mtime[4];
	u_char		xflags;
	u_char		os;
#if COMMENT_ONLY
	/* Optional fields */
	u_char		part[2];		/* flags & GZIP_FCONT */
	struct gzip_xfield xfield;		/* flags & GZIP_FEXTRA */
	char		filename[];		/* flags & GZIP_FNAME */
	char		comment[];		/* flags & GZIP_FCOMMENT */
	u_char		encrypt_hdr[12];	/* flags & GZIP_FENCRYPT */
#endif
};

struct gzip_trailer {
	u_int32_t	crc32[4];
	u_int32_t	size[4];
};

#endif /* GZIP_H */
