/*	$OpenBSD: aparams.c,v 1.13 2011/04/28 07:20:03 ratchov Exp $	*/
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

#include "aparams.h"
#ifdef DEBUG
#include "dbg.h"
#endif

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
 * Fake parameters for byte-streams
 */
struct aparams aparams_none = { 1, 0, 0, 0, 0, 0, 0, 0 };

#ifdef DEBUG
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
#endif /* DEBUG */

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
		 * get (optional) alignement
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
aparams_init(struct aparams *par, unsigned cmin, unsigned cmax, unsigned rate)
{
	par->bps = sizeof(adata_t);
	par->bits = ADATA_BITS;
	par->le = ADATA_LE;
	par->sig = 1;
	par->msb = 0;
	par->cmin = cmin;
	par->cmax = cmax;
	par->rate = rate;
}

#ifdef DEBUG
/*
 * Print the format/channels/encoding on stderr.
 */
void
aparams_dbg(struct aparams *par)
{
	char enc[ENCMAX];

	aparams_enctostr(par, enc);
	dbg_puts(enc);
	dbg_puts(",");
	dbg_putu(par->cmin);
	dbg_puts(":");
	dbg_putu(par->cmax);
	dbg_puts(",");
	dbg_putu(par->rate);
}
#endif

/*
 * Return true if both encodings are the same.
 */
int
aparams_eqenc(struct aparams *par1, struct aparams *par2)
{
	if (par1->bps != par2->bps ||
	    par1->bits != par2->bits ||
	    par1->sig != par2->sig)
		return 0;
	if ((par1->bits != 8 * par1->bps) && par1->msb != par2->msb)
		return 0;
	if (par1->bps > 1 && par1->le != par2->le)
		return 0;
	return 1;
}

/*
 * Grow channels range and sample rate of ``set'' in order ``subset'' to
 * become an actual subset of it.
 */
void
aparams_grow(struct aparams *set, struct aparams *subset)
{
	if (set->cmin > subset->cmin)
		set->cmin = subset->cmin;
	if (set->cmax < subset->cmax)
		set->cmax = subset->cmax;
	if (set->rate < subset->rate)
		set->rate = subset->rate;
}

/*
 * Return true if rates are the same.
 */
int
aparams_eqrate(struct aparams *p1, struct aparams *p2)
{
	/* XXX: allow 1/9 halftone of difference */
	return p1->rate == p2->rate;
}


/*
 * Return the number of bytes per frame with the given parameters.
 */
unsigned
aparams_bpf(struct aparams *par)
{
	return (par->cmax - par->cmin + 1) * par->bps;
}

void
aparams_copyenc(struct aparams *dst, struct aparams *src)
{
	dst->sig = src->sig;
	dst->le = src->le;
	dst->msb = src->msb;
	dst->bits = src->bits;
	dst->bps = src->bps;
}
