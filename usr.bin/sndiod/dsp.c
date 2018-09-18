/*	$OpenBSD: dsp.c,v 1.14 2018/09/18 06:05:45 miko Exp $	*/
/*
 * Copyright (c) 2008-2012 Alexandre Ratchov <alex@caoua.org>
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
#include <string.h>
#include "dsp.h"
#include "utils.h"

int aparams_ctltovol[128] = {
	    0,
	  256,	  266,	  276,	  287,	  299,	  310,	  323,	  335,
	  348,	  362,	  376,	  391,	  406,	  422,	  439,	  456,
	  474,	  493,	  512,	  532,	  553,	  575,	  597,	  621,
	  645,	  670,	  697,	  724,	  753,	  782,	  813,	  845,
	  878,	  912,	  948,	  985,	 1024,	 1064,	 1106,	 1149,
	 1195,	 1241,	 1290,	 1341,	 1393,	 1448,	 1505,	 1564,
	 1625,	 1689,	 1756,	 1825,	 1896,	 1971,	 2048,	 2128,
	 2212,	 2299,	 2389,	 2483,	 2580,	 2682,	 2787,	 2896,
	 3010,	 3128,	 3251,	 3379,	 3511,	 3649,	 3792,	 3941,
	 4096,	 4257,	 4424,	 4598,	 4778,	 4966,	 5161,	 5363,
	 5574,	 5793,	 6020,	 6256,	 6502,	 6757,	 7023,	 7298,
	 7585,	 7883,	 8192,	 8514,	 8848,	 9195,	 9556,	 9931,
	10321,	10726,	11148,	11585,	12040,	12513,	13004,	13515,
	14045,	14596,	15170,	15765,	16384,	17027,	17696,	18390,
	19112,	19863,	20643,	21453,	22295,	23170,	24080,	25025,
	26008,	27029,	28090,	29193,	30339,	31530,	32768
};

/*
 * Generate a string corresponding to the encoding in par,
 * return the length of the resulting string.
 */
int
aparams_enctostr(struct aparams *par, char *ostr)
{
	char *p = ostr;

	*p++ = par->sig ? 's' : 'u';
	if (par->bits > 9)
		*p++ = '0' + par->bits / 10;
	*p++ = '0' + par->bits % 10;
	if (par->bps > 1) {
		*p++ = par->le ? 'l' : 'b';
		*p++ = 'e';
		if (par->bps != APARAMS_BPS(par->bits) ||
		    par->bits < par->bps * 8) {
			*p++ = par->bps + '0';
			if (par->bits < par->bps * 8) {
				*p++ = par->msb ? 'm' : 'l';
				*p++ = 's';
				*p++ = 'b';
			}
		}
	}
	*p++ = '\0';
	return p - ostr - 1;
}

/*
 * Parse an encoding string, examples: s8, u8, s16, s16le, s24be ...
 * set *istr to the char following the encoding. Return the number
 * of bytes consumed.
 */
int
aparams_strtoenc(struct aparams *par, char *istr)
{
	char *p = istr;
	int i, sig, bits, le, bps, msb;

#define IS_SEP(c)			\
	(((c) < 'a' || (c) > 'z') &&	\
	 ((c) < 'A' || (c) > 'Z') &&	\
	 ((c) < '0' || (c) > '9'))

	/*
	 * get signedness
	 */
	if (*p == 's') {
		sig = 1;
	} else if (*p == 'u') {
		sig = 0;
	} else
		return 0;
	p++;

	/*
	 * get number of bits per sample
	 */
	bits = 0;
	for (i = 0; i < 2; i++) {
		if (*p < '0' || *p > '9')
			break;
		bits = (bits * 10) + *p - '0';
		p++;
	}
	if (bits < BITS_MIN || bits > BITS_MAX)
		return 0;
	bps = APARAMS_BPS(bits);
	msb = 1;
	le = ADATA_LE;

	/*
	 * get (optional) endianness
	 */
	if (p[0] == 'l' && p[1] == 'e') {
		le = 1;
		p += 2;
	} else if (p[0] == 'b' && p[1] == 'e') {
		le = 0;
		p += 2;
	} else if (IS_SEP(*p)) {
		goto done;
	} else
		return 0;

	/*
	 * get (optional) number of bytes
	 */
	if (*p >= '0' && *p <= '9') {
		bps = *p - '0';
		if (bps < (bits + 7) / 8 ||
		    bps > (BITS_MAX + 7) / 8)
			return 0;
		p++;

		/*
		 * get (optional) alignment
		 */
		if (p[0] == 'm' && p[1] == 's' && p[2] == 'b') {
			msb = 1;
			p += 3;
		} else if (p[0] == 'l' && p[1] == 's' && p[2] == 'b') {
			msb = 0;
			p += 3;
		} else if (IS_SEP(*p)) {
			goto done;
		} else
			return 0;
	} else if (!IS_SEP(*p))
		return 0;

done:
	par->msb = msb;
	par->sig = sig;
	par->bits = bits;
	par->bps = bps;
	par->le = le;
	return p - istr;
}

/*
 * Initialise parameters structure with the defaults natively supported
 * by the machine.
 */
void
aparams_init(struct aparams *par)
{
	par->bps = sizeof(adata_t);
	par->bits = ADATA_BITS;
	par->le = ADATA_LE;
	par->sig = 1;
	par->msb = 0;
}

/*
 * log the given format/channels/encoding
 */
void
aparams_log(struct aparams *par)
{
	char enc[ENCMAX];

	aparams_enctostr(par, enc);
	log_puts(enc);
}

/*
 * return true if encoding corresponds to what we store in adata_t
 */
int
aparams_native(struct aparams *par)
{
	return par->bps == sizeof(adata_t) && par->bits == ADATA_BITS &&
	    (par->bps == 1 || par->le == ADATA_LE) &&
	    (par->bits == par->bps * 8 || !par->msb);
}

/*
 * resample the given number of frames
 */
void
resamp_do(struct resamp *p, adata_t *in, adata_t *out, int todo)
{
	unsigned int nch;
	adata_t *idata;
	unsigned int oblksz;
	int s, ds, diff;
	adata_t *odata;
	unsigned int iblksz;
	unsigned int c;
	adata_t *ctxbuf, *ctx;
	unsigned int ctx_start;

#ifdef DEBUG
	if (todo % p->iblksz != 0) {
		log_puts("resamp_do: partial blocks not supported\n");
		panic();
	}
#endif

	/*
	 * Partially copy structures into local variables, to avoid
	 * unnecessary indirections; this also allows the compiler to
	 * order local variables more "cache-friendly".
	 */
	idata = in;
	odata = out;
	diff = p->oblksz;
	iblksz = p->iblksz;
	oblksz = p->oblksz;
	ctxbuf = p->ctx;
	ctx_start = p->ctx_start;
	nch = p->nch;

	for (;;) {
		if (diff >= oblksz) {
			if (todo == 0)
				break;
			ctx_start ^= 1;
			ctx = ctxbuf + ctx_start;
			for (c = nch; c > 0; c--) {
				*ctx = *idata++;
				ctx += RESAMP_NCTX;
			}
			diff -= oblksz;
			todo--;
		} else {
			ctx = ctxbuf;
			for (c = nch; c > 0; c--) {
				s = ctx[ctx_start ^ 1];
				ds = ctx[ctx_start] - s;
				ctx += RESAMP_NCTX;
				*odata++ = s + ADATA_MULDIV(ds, diff, oblksz);
			}
			diff += iblksz;
		}
	}

	p->ctx_start = ctx_start;
}

/*
 * initialize resampler with ibufsz/obufsz factor and "nch" channels
 */
void
resamp_init(struct resamp *p, unsigned int iblksz,
    unsigned int oblksz, int nch)
{
	p->iblksz = iblksz;
	p->oblksz = oblksz;
	p->nch = nch;
	p->ctx_start = 0;
	memset(p->ctx, 0, sizeof(p->ctx));
#ifdef DEBUG
	if (log_level >= 3) {
		log_puts("resamp: ");
		log_putu(iblksz);
		log_puts("/");
		log_putu(oblksz);
		log_puts("\n");
	}
#endif
}

/*
 * encode "todo" frames from native to foreign encoding
 */
void
enc_do(struct conv *p, unsigned char *in, unsigned char *out, int todo)
{
	unsigned int f;
	adata_t *idata;
	unsigned int s;
	unsigned int oshift;
	unsigned int obias;
	unsigned int obps;
	unsigned int i;
	unsigned char *odata;
	int obnext;
	int osnext;

#ifdef DEBUG
	if (log_level >= 4) {
		log_puts("enc: copying ");
		log_putu(todo);
		log_puts(" frames\n");
	}
#endif
	/*
	 * Partially copy structures into local variables, to avoid
	 * unnecessary indirections; this also allows the compiler to
	 * order local variables more "cache-friendly".
	 */
	idata = (adata_t *)in;
	odata = out;
	oshift = p->shift;
	obias = p->bias;
	obps = p->bps;
	obnext = p->bnext;
	osnext = p->snext;

	/*
	 * Start conversion.
	 */
	odata += p->bfirst;
	for (f = todo * p->nch; f > 0; f--) {
		/* convert adata to u32 */
		s = (int)*idata++ + ADATA_UNIT;
		s <<= 32 - ADATA_BITS;
		/* convert u32 to uN */
		s >>= oshift;
		/* convert uN to sN */
		s -= obias;
		/* packetize sN */
		for (i = obps; i > 0; i--) {
			*odata = (unsigned char)s;
			s >>= 8;
			odata += obnext;
		}
		odata += osnext;
	}
}

/*
 * store "todo" frames of silence in foreign encoding
 */
void
enc_sil_do(struct conv *p, unsigned char *out, int todo)
{
	unsigned int f;
	unsigned int s;
	unsigned int oshift;
	int obias;
	unsigned int obps;
	unsigned int i;
	unsigned char *odata;
	int obnext;
	int osnext;

#ifdef DEBUG
	if (log_level >= 4) {
		log_puts("enc: silence ");
		log_putu(todo);
		log_puts(" frames\n");
	}
#endif
	/*
	 * Partially copy structures into local variables, to avoid
	 * unnecessary indirections; this also allows the compiler to
	 * order local variables more "cache-friendly".
	 */
	odata = out;
	oshift = p->shift;
	obias = p->bias;
	obps = p->bps;
	obnext = p->bnext;
	osnext = p->snext;

	/*
	 * Start conversion.
	 */
	odata += p->bfirst;
	for (f = todo * p->nch; f > 0; f--) {
		s = ((1U << 31) >> oshift) - obias;
		for (i = obps; i > 0; i--) {
			*odata = (unsigned char)s;
			s >>= 8;
			odata += obnext;
		}
		odata += osnext;
	}
}

/*
 * initialize encoder from native to foreign encoding
 */
void
enc_init(struct conv *p, struct aparams *par, int nch)
{
	p->nch = nch;
	p->bps = par->bps;
	if (par->msb) {
		p->shift = 32 - par->bps * 8;
	} else {
		p->shift = 32 - par->bits;
	}
	if (par->sig) {
		p->bias = (1U << 31) >> p->shift;
	} else {
		p->bias = 0;
	}
	if (!par->le) {
		p->bfirst = par->bps - 1;
		p->bnext = -1;
		p->snext = 2 * par->bps;
	} else {
		p->bfirst = 0;
		p->bnext = 1;
		p->snext = 0;
	}
#ifdef DEBUG
	if (log_level >= 3) {
		log_puts("enc: ");
		aparams_log(par);
		log_puts(", ");
		log_puti(p->nch);
		log_puts(" channels\n");
	}
#endif
}

/*
 * decode "todo" frames from foreign to native encoding
 */
void
dec_do(struct conv *p, unsigned char *in, unsigned char *out, int todo)
{
	unsigned int f;
	unsigned int ibps;
	unsigned int i;
	unsigned int s = 0xdeadbeef;
	unsigned char *idata;
	int ibnext;
	int isnext;
	unsigned int ibias;
	unsigned int ishift;
	adata_t *odata;

#ifdef DEBUG
	if (log_level >= 4) {
		log_puts("dec: copying ");
		log_putu(todo);
		log_puts(" frames\n");
	}
#endif
	/*
	 * Partially copy structures into local variables, to avoid
	 * unnecessary indirections; this also allows the compiler to
	 * order local variables more "cache-friendly".
	 */
	idata = in;
	odata = (adata_t *)out;
	ibps = p->bps;
	ibnext = p->bnext;
	ibias = p->bias;
	ishift = p->shift;
	isnext = p->snext;

	/*
	 * Start conversion.
	 */
	idata += p->bfirst;
	for (f = todo * p->nch; f > 0; f--) {
		for (i = ibps; i > 0; i--) {
			s <<= 8;
			s |= *idata;
			idata += ibnext;
		}
		idata += isnext;
		s += ibias;
		s <<= ishift;
		s >>= 32 - ADATA_BITS;
		*odata++ = s - ADATA_UNIT;
	}
}

/*
 * initialize decoder from foreign to native encoding
 */
void
dec_init(struct conv *p, struct aparams *par, int nch)
{
	p->bps = par->bps;
	p->nch = nch;
	if (par->msb) {
		p->shift = 32 - par->bps * 8;
	} else {
		p->shift = 32 - par->bits;
	}
	if (par->sig) {
		p->bias = (1U << 31) >> p->shift;
	} else {
		p->bias = 0;
	}
	if (par->le) {
		p->bfirst = par->bps - 1;
		p->bnext = -1;
		p->snext = 2 * par->bps;
	} else {
		p->bfirst = 0;
		p->bnext = 1;
		p->snext = 0;
	}
#ifdef DEBUG
	if (log_level >= 3) {
		log_puts("dec: ");
		aparams_log(par);
		log_puts(", ");
		log_puti(p->nch);
		log_puts(" channels\n");
	}
#endif
}

/*
 * mix "todo" input frames on the output with the given volume
 */
void
cmap_add(struct cmap *p, void *in, void *out, int vol, int todo)
{
	adata_t *idata, *odata;
	int i, j, nch, istart, inext, onext, ostart, y, v;

#ifdef DEBUG
	if (log_level >= 4) {
		log_puts("cmap: adding ");
		log_puti(todo);
		log_puts(" frames\n");
	}
#endif
	idata = in;
	odata = out;
	ostart = p->ostart;
	onext = p->onext;
	istart = p->istart;
	inext = p->inext;
	nch = p->nch;
	v = vol;

	/*
	 * map/mix input on the output
	 */
	for (i = todo; i > 0; i--) {
		odata += ostart;
		idata += istart;
		for (j = nch; j > 0; j--) {
			y = *odata + ADATA_MUL(*idata, v);
			if (y >= ADATA_UNIT)
				y = ADATA_UNIT - 1;
			else if (y < -ADATA_UNIT)
				y = -ADATA_UNIT;
			*odata = y;
			idata++;
			odata++;
		}
		odata += onext;
		idata += inext;
	}
}

/*
 * overwrite output with "todo" input frames with the given volume
 */
void
cmap_copy(struct cmap *p, void *in, void *out, int vol, int todo)
{
	adata_t *idata, *odata;
	int i, j, nch, istart, inext, onext, ostart, v;

#ifdef DEBUG
	if (log_level >= 4) {
		log_puts("cmap: copying ");
		log_puti(todo);
		log_puts(" frames\n");
	}
#endif
	idata = in;
	odata = out;
	ostart = p->ostart;
	onext = p->onext;
	istart = p->istart;
	inext = p->inext;
	nch = p->nch;
	v = vol;

	/*
	 * copy to the output buffer
	 */
	for (i = todo; i > 0; i--) {
		idata += istart;
		odata += ostart;
		for (j = nch; j > 0; j--) {
			*odata = ADATA_MUL(*idata, v);
			odata++;
			idata++;
		}
		odata += onext;
		idata += inext;
	}
}

/*
 * initialize channel mapper, to map a subset of input channel range
 * into a subset of the output channel range
 */
void
cmap_init(struct cmap *p,
    int imin, int imax, int isubmin, int isubmax,
    int omin, int omax, int osubmin, int osubmax)
{
	int cmin, cmax;

	cmin = -NCHAN_MAX;
	if (osubmin > cmin)
		cmin = osubmin;
	if (omin > cmin)
		cmin = omin;
	if (isubmin > cmin)
		cmin = isubmin;
	if (imin > cmin)
		cmin = imin;

	cmax = NCHAN_MAX;
	if (osubmax < cmax)
		cmax = osubmax;
	if (omax < cmax)
		cmax = omax;
	if (isubmax < cmax)
		cmax = isubmax;
	if (imax < cmax)
		cmax = imax;

	p->ostart = cmin - omin;
	p->onext = omax - cmax;
	p->istart = cmin - imin;
	p->inext = imax - cmax;
	p->nch = cmax - cmin + 1;
#ifdef DEBUG
	if (log_level >= 3) {
		log_puts("cmap: nch = ");
		log_puti(p->nch);
		log_puts(", ostart = ");
		log_puti(p->ostart);
		log_puts(", onext = ");
		log_puti(p->onext);
		log_puts(", istart = ");
		log_puti(p->istart);
		log_puts(", inext = ");
		log_puti(p->inext);
		log_puts("\n");
	}
#endif
}
