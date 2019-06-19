/*	$OpenBSD: dsp.h,v 1.7 2018/06/08 06:21:56 ratchov Exp $	*/
/*
 * Copyright (c) 2012 Alexandre Ratchov <alex@caoua.org>
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
#ifndef DSP_H
#define DSP_H

#include <sys/types.h>
#include "defs.h"

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

#define ADATA_MUL(x,y)		(((int)(x) * (int)(y)) >> (ADATA_BITS - 1))
#define ADATA_MULDIV(x,y,z)	((int)(x) * (int)(y) / (int)(z))

typedef short adata_t;

#elif ADATA_BITS == 24

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

typedef int adata_t;

#else
#error "only 16-bit and 24-bit precisions are supported"
#endif

/*
 * Maximum size of the encording string (the longest possible
 * encoding is ``s24le3msb'').
 */
#define ENCMAX	10

/*
 * Default bytes per sample for the given bits per sample.
 */
#define APARAMS_BPS(bits) (((bits) <= 8) ? 1 : (((bits) <= 16) ? 2 : 4))

struct aparams {
	unsigned int bps;		/* bytes per sample */
	unsigned int bits;		/* actually used bits */
	unsigned int le;		/* 1 if little endian, else be */
	unsigned int sig;		/* 1 if signed, 0 if unsigned */
	unsigned int msb;		/* 1 if msb justified, else lsb */
};

struct resamp {
#define RESAMP_NCTX	2
	unsigned int ctx_start;
	adata_t ctx[NCHAN_MAX * RESAMP_NCTX];
	unsigned int iblksz, oblksz;
	int nch;
};

struct conv {
	int bfirst;			/* bytes to skip at startup */
	unsigned int bps;		/* bytes per sample */
	unsigned int shift;		/* shift to get 32bit MSB */
	unsigned int bias;		/* bias of unsigned samples */
	int bnext;			/* to reach the next byte */
	int snext;			/* to reach the next sample */
	int nch;
};

struct cmap {
	int istart;
	int inext;
	int onext;
	int ostart;
	int nch;
};

#define MIDI_TO_ADATA(m)	(aparams_ctltovol[m] << (ADATA_BITS - 16))
extern int aparams_ctltovol[128];

void aparams_init(struct aparams *);
void aparams_log(struct aparams *);
int aparams_strtoenc(struct aparams *, char *);
int aparams_enctostr(struct aparams *, char *);
int aparams_native(struct aparams *);

void resamp_do(struct resamp *, adata_t *, adata_t *, int);
void resamp_init(struct resamp *, unsigned int, unsigned int, int);
void enc_do(struct conv *, unsigned char *, unsigned char *, int);
void enc_sil_do(struct conv *, unsigned char *, int);
void enc_init(struct conv *, struct aparams *, int);
void dec_do(struct conv *, unsigned char *, unsigned char *, int);
void dec_init(struct conv *, struct aparams *, int);
void cmap_add(struct cmap *, void *, void *, int, int);
void cmap_copy(struct cmap *, void *, void *, int, int);
void cmap_init(struct cmap *, int, int, int, int, int, int, int, int);

#endif /* !defined(DSP_H) */
