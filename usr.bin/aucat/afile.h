/*	$OpenBSD: afile.h,v 1.1 2015/01/21 08:43:55 ratchov Exp $	*/
/*
 * Copyright (c) 2008 Alexandre Ratchov <alex@caoua.org>
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
#ifndef WAV_H
#define WAV_H

#include <sys/types.h>
#include "dsp.h"

struct afile {
	struct aparams par;		/* file params */
#define AFILE_FMT_PCM	0		/* integers (fixed point) */
#define AFILE_FMT_ULAW	1		/* 8-bit mu-law */
#define AFILE_FMT_ALAW	2		/* 8-bit a-law */
#define AFILE_FMT_FLOAT	3		/* IEEE 754 32-bit floats */
	int fmt;			/* one of above */
	int rate;			/* file sample rate */
	int nch;			/* file channel count */
#define AFILE_HDR_AUTO	0		/* guess from file name */
#define AFILE_HDR_RAW	1		/* headerless aka "raw" file */
#define AFILE_HDR_WAV	2		/* microsoft .wav */
#define AFILE_HDR_AIFF	3		/* apple .aiff */
#define AFILE_HDR_AU	4		/* sun/next .au */
	int hdr;			/* header type */
	int fd;				/* file descriptor */
#define AFILE_FREAD	1		/* open for reading */
#define AFILE_FWRITE	2		/* open for writing */
	int flags;			/* bitmap of above */
	off_t curpos;			/* read/write position (bytes) */
	off_t startpos;			/* where payload starts */
	off_t endpos;			/* where payload ends */
	off_t maxpos;			/* max allowed pos (.wav limitation) */
	char *path;			/* file name (debug only) */
};

int afile_open(struct afile *, char *, int, int, struct aparams *, int, int);
size_t afile_read(struct afile *, void *, size_t);
size_t afile_write(struct afile *, void *, size_t);
int afile_seek(struct afile *, off_t);
void afile_close(struct afile *);

#endif /* !defined(WAV_H) */
