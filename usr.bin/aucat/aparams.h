/*	$OpenBSD: aparams.h,v 1.13 2015/01/16 06:40:05 deraadt Exp $	*/
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

#define NCHAN_MAX	16		/* max channel in a stream */
#define RATE_MIN	4000		/* min sample rate */
#define RATE_MAX	192000		/* max sample rate */
#define BITS_MIN	1		/* min bits per sample */
#define BITS_MAX	32		/* max bits per sample */

/*
 * Maximum size of the encording string (the longest possible
 * encoding is ``s24le3msb'').
 */
#define ENCMAX	10

/*
 * Default bytes per sample for the given bits per sample.
 */
#define APARAMS_BPS(bits) (((bits) <= 8) ? 1 : (((bits) <= 16) ? 2 : 4))

/*
 * Encoding specification.
 */
struct aparams {
	unsigned int bps;		/* bytes per sample */
	unsigned int bits;		/* actually used bits */
	unsigned int le;		/* 1 if little endian, 0 if big endian */
	unsigned int sig;		/* 1 if signed, 0 if unsigned */
	unsigned int msb;		/* 1 if msb justified, 0 if lsb justified */
	unsigned int cmin, cmax;	/* provided/consumed channels */
	unsigned int rate;		/* frames per second */
};

/*
 * Samples are numbers in the interval [-1, 1[, note that 1, the upper
 * boundary is excluded. We represent them as signed fixed point numbers
 * of ADATA_BITS. We also assume that 2^(ADATA_BITS - 1) fits in a int.
 */
#ifndef ADATA_BITS
#define ADATA_BITS			16
#endif
#define ADATA_LE			(BYTE_ORDER == LITTLE_ENDIAN)
#define ADATA_UNIT			(1 << (ADATA_BITS - 1))

#if ADATA_BITS == 16

typedef short adata_t;

#define ADATA_MUL(x,y)		(((int)(x) * (int)(y)) >> (ADATA_BITS - 1))
#define ADATA_MULDIV(x,y,z)	((int)(x) * (int)(y) / (int)(z))

#elif ADATA_BITS == 24

typedef int adata_t;

#if defined(__i386__) && defined(__GNUC__)

static inline int
fp24_mul(int x, int a)
{
	int res;

	asm volatile (
		"imull	%2\n\t"
		"shrdl $23, %%edx, %%eax\n\t"
		: "=a" (res)
		: "a" (x), "r" (a)
		: "%edx"
		);
	return res;
}

static inline int
fp24_muldiv(int x, int a, int b)
{
	int res;

	asm volatile (
		"imull %2\n\t"
		"idivl %3\n\t"
		: "=a" (res)
		: "a" (x), "d" (a), "r" (b)
		);
	return res;
}

#define ADATA_MUL(x,y)		fp24_mul(x, y)
#define ADATA_MULDIV(x,y,z)	fp24_muldiv(x, y, z);

#elif defined(__amd64__) || defined(__sparc64__)

#define ADATA_MUL(x,y)		\
	((int)(((long long)(x) * (long long)(y)) >> (ADATA_BITS - 1)))
#define ADATA_MULDIV(x,y,z)	\
	((int)((long long)(x) * (long long)(y) / (long long)(z)))

#else
#error "no 24-bit code for this architecture"
#endif

#else
#error "only 16-bit and 24-bit precisions are supported"
#endif

#define MIDI_MAXCTL		127
#define MIDI_TO_ADATA(m)	(aparams_ctltovol[m] << (ADATA_BITS - 16))

extern int aparams_ctltovol[128];
extern struct aparams aparams_none;

void aparams_init(struct aparams *, unsigned int, unsigned int, unsigned int);
void aparams_dbg(struct aparams *);
int aparams_eqrate(struct aparams *, struct aparams *);
int aparams_eqenc(struct aparams *, struct aparams *);
int aparams_eq(struct aparams *, struct aparams *);
int aparams_subset(struct aparams *, struct aparams *);
void aparams_grow(struct aparams *, struct aparams *);
unsigned int aparams_bpf(struct aparams *);
int aparams_strtoenc(struct aparams *, char *);
int aparams_enctostr(struct aparams *, char *);
void aparams_copyenc(struct aparams *, struct aparams *);

#endif /* !defined(APARAMS_H) */
