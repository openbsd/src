/*	$OpenBSD: wav.h,v 1.7 2010/04/03 17:59:17 ratchov Exp $	*/
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

#include "aparams.h"
#include "pipe.h"

struct wav {
	struct pipe pipe;
#define HDR_AUTO	0	/* guess by looking at the file name */
#define HDR_RAW		1	/* no headers, ie openbsd native ;-) */
#define HDR_WAV		2	/* microsoft riff wave */
	unsigned hdr;		/* HDR_RAW or HDR_WAV */
	struct aparams hpar;	/* parameters to write on the header */
	off_t rbytes;		/* bytes to read, -1 if no limit */
	off_t wbytes;		/* bytes to write, -1 if no limit */
	short *map;		/* mulaw/alaw -> s16 conversion table */
};

extern struct fileops wav_ops;

struct wav *wav_new_in(struct fileops *, char *, unsigned,
    struct aparams *, unsigned, unsigned);
struct wav *wav_new_out(struct fileops *, char *, unsigned,
    struct aparams *, unsigned);
unsigned wav_read(struct file *, unsigned char *, unsigned);
unsigned wav_write(struct file *, unsigned char *, unsigned);
void wav_close(struct file *);
int wav_readhdr(int, struct aparams *, off_t *, short **);
int wav_writehdr(int, struct aparams *);
void wav_conv(unsigned char *, unsigned, short *);

/* legacy */
int legacy_play(char *, char *);

extern short wav_ulawmap[256];
extern short wav_alawmap[256];

#endif /* !defined(WAV_H) */
