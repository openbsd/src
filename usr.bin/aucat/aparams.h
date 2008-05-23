/*	$OpenBSD: aparams.h,v 1.1 2008/05/23 07:15:46 ratchov Exp $	*/
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
#ifndef APARAMS_H
#define APARAMS_H

#include <sys/param.h>

#define CHAN_MAX	16		/* max number of channels */
#define RATE_MIN	1		/* min sample rate */
#define RATE_MAX	(1 << 30)	/* max sample rate */
#define BITS_MAX	32		/* max bits per sample */

#if BYTE_ORDER ==  LITTLE_ENDIAN
#define NATIVE_LE 1
#elif BYTE_ORDER == BIG_ENDIAN
#define NATIVE_LE 0
#else
/* not defined */
#endif

/*
 * encoding specification
 */
struct aparams {
	unsigned bps;		/* bytes per sample */
	unsigned bits;		/* actually used bits */
	unsigned le;		/* 1 if little endian, 0 if big endian */
	unsigned sig;		/* 1 if signed, 0 if unsigned */
	unsigned msb;		/* 1 if msb justified, 0 if lsb justified */
	unsigned cmin, cmax;	/* provided/consumed channels */
	unsigned rate;		/* frames per second */
};

/*
 * Samples are numbers in the interval [-1, 1[, note that 1, the upper
 * boundary is excluded. We represent them in 16-bit signed fixed point
 * numbers, so that we can do all multiplications and divisions in
 * 32-bit precision without having to deal with overflows.
 */

#define ADATA_SHIFT		(8 * sizeof(short) - 1)
#define ADATA_UNIT		(1 << ADATA_SHIFT)
#define ADATA_MAX		(ADATA_UNIT - 1)
#define ADATA_MUL(x,y)		(((x) * (y)) >> ADATA_SHIFT)

void aparams_init(struct aparams *, unsigned, unsigned, unsigned);
void aparams_print(struct aparams *);
void aparams_print2(struct aparams *, struct aparams *);
int aparams_eq(struct aparams *, struct aparams *);
unsigned aparams_bpf(struct aparams *);

#endif /* !defined(APARAMS_H) */
