/*	$OpenBSD: aparams.c,v 1.2 2008/10/26 08:49:43 ratchov Exp $	*/
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aparams.h"

/*
 * Generate a string corresponding to the encoding in par,
 * return the length of the resulting string
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
 * set *istr to the char following the encoding. Retrun the number
 * of bytes consumed
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
	le = NATIVE_LE;

	/*
	 * get (optionnal) endianness
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
	 * get (optionnal) number of bytes
	 */
	if (*p >= '0' && *p <= '9') {
		bps = *p - '0';
		if (bps < (bits + 7) / 8 ||
		    bps > (BITS_MAX + 7) / 8)
			return 0;
		p++;

		/*
		 * get (optionnal) alignement
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
	par->bps = 2;		/* 2 bytes per sample */
	par->bits = 16;		/* 16 significant bits per sample */
	par->sig = 1;		/* samples are signed */
	par->le = NATIVE_LE;
	par->msb = 1;		/* msb justified */
	par->cmin = cmin;
	par->cmax = cmax;
	par->rate = rate;
}

/*
 * Print the format/channels/encoding on stderr.
 */
void
aparams_print(struct aparams *par)
{	
	char enc[ENCMAX];

	aparams_enctostr(par, enc);
	fprintf(stderr, "%s", enc);
	fprintf(stderr, ",%u:%u", par->cmin, par->cmax);
	fprintf(stderr, ",%uHz", par->rate);
}

void
aparams_print2(struct aparams *par1, struct aparams *par2)
{
	aparams_print(par1);
	fprintf(stderr, " -> ");
	aparams_print(par2);
}

/*
 * Return true if both parameters are the same.
 */
int
aparams_eq(struct aparams *par1, struct aparams *par2)
{
	if (par1->bps != par2->bps ||
	    par1->bits != par2->bits ||
	    par1->sig != par2->sig ||
	    par1->cmin != par2->cmin ||
	    par1->cmax != par2->cmax ||
	    par1->rate != par2->rate)
		return 0;
	if ((par1->bits != 8 * par1->bps) && par1->msb != par2->msb)
		return 0;
	if (par1->bps > 1 && par1->le != par2->le)
		return 0;
	return 1;
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

