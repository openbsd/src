/*	$OpenBSD: aparams.c,v 1.1 2008/05/23 07:15:46 ratchov Exp $	*/
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
	fprintf(stderr, "%c", par->sig ? 's' : 'u');
	fprintf(stderr, "%u", par->bits);
	if (par->bps > 1)
		fprintf(stderr, "%s", par->le ? "le" : "be");
	if ((par->bits & (par->bits - 1)) != 0 || par->bits != 8 * par->bps) {
		fprintf(stderr, "/%u", par->bps);
		fprintf(stderr, "%s", par->msb ? "msb" : "lsb");
	}
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
