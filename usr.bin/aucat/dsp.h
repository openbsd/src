/*	$OpenBSD: dsp.h,v 1.9 2022/03/07 09:04:45 ratchov Exp $	*/
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
#define ADATA_BITS			24
#define ADATA_LE			(BYTE_ORDER == LITTLE_ENDIAN)
#define ADATA_UNIT			(1 << (ADATA_BITS - 1))

#define ADATA_MUL(x,y)		\
	((int)(((long long)(x) * (long long)(y)) >> (ADATA_BITS - 1)))

typedef int adata_t;

/*
 * The FIR is sampled and stored in a table of fixed-point numbers
 * with 23 fractional bits. For convenience, we use the same fixed-point
 * numbers to represent time and to walk through the table.
 */
#define RESAMP_BITS		23
#define RESAMP_UNIT		(1 << RESAMP_BITS)

/*
 * Filter window length (the time unit is RESAMP_UNIT)
 */
#define RESAMP_LENGTH		(8 * RESAMP_UNIT)

/*
 * Time between samples of the FIR (the time unit is RESAMP_UNIT)
 */
#define RESAMP_STEP_BITS	(RESAMP_BITS - 6)
#define RESAMP_STEP		(1 << RESAMP_STEP_BITS)

/*
 * Maximum downsample/upsample ratio we support, must be a power of two.
 * The ratio between the max and the min sample rates is 192kHz / 4kHz = 48,
 * so we can use 64
 */
#define RESAMP_RATIO		64

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
	unsigned int le;		/* 1 if little endian, 0 if big endian */
	unsigned int sig;		/* 1 if signed, 0 if unsigned */
	unsigned int msb;		/* 1 if msb justified, 0 if lsb justified */
};

struct resamp {
#define RESAMP_NCTX	(RESAMP_LENGTH / RESAMP_UNIT * RESAMP_RATIO)
	unsigned int ctx_start;
	adata_t ctx[NCHAN_MAX * RESAMP_NCTX];
	int filt_cutoff, filt_step;
	unsigned int iblksz, oblksz;
	int diff;
	int nch;
};

struct conv {
	int bfirst;			/* bytes to skip at startup */
	unsigned int bps;		/* bytes per sample */
	unsigned int shift;		/* shift to get 32bit MSB */
	unsigned int bias;			/* bias of unsigned samples */
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
extern const int aparams_ctltovol[128];

void aparams_init(struct aparams *);
void aparams_log(struct aparams *);
int aparams_strtoenc(struct aparams *, char *);
int aparams_enctostr(struct aparams *, char *);
int aparams_native(struct aparams *);

void resamp_getcnt(struct resamp *, int *, int *);
void resamp_do(struct resamp *, adata_t *, adata_t *, int, int);
void resamp_init(struct resamp *, unsigned int, unsigned int, int);
void enc_do(struct conv *, unsigned char *, unsigned char *, int);
void enc_sil_do(struct conv *, unsigned char *, int);
void enc_init(struct conv *, struct aparams *, int);
void dec_do(struct conv *, unsigned char *, unsigned char *, int);
void dec_do_float(struct conv *, unsigned char *, unsigned char *, int);
void dec_do_ulaw(struct conv *, unsigned char *, unsigned char *, int, int);
void dec_init(struct conv *, struct aparams *, int);
void cmap_add(struct cmap *, void *, void *, int, int);
void cmap_copy(struct cmap *, void *, void *, int, int);
void cmap_init(struct cmap *, int, int, int, int, int, int, int, int);

#endif /* !defined(DSP_H) */
