/*	$NetBSD: wscons_rinit.c,v 1.1 1996/04/12 02:00:54 cgd Exp $ */

/*
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)rcons_font.c	8.1 (Berkeley) 6/11/93
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/device.h>

#include <dev/rcons/raster.h>
#include <alpha/wscons/wscons_raster.h>

#include <alpha/wscons/wscons_rfont.h>

void
rcons_initfont(rc, fp)
	struct rcons *rc;
	struct raster_font *fp;
{
	static int initfontdone;

	rc->rc_font = fp;

	/* Get distance to top and bottom of font from font origin */
	rc->rc_font_ascent = -(rc->rc_font->chars)['a'].homey;

#if !defined(MSBYTE_FIRST) && !defined(MSBIT_FIRST) /* XXX other cases */
	/* swap byte order on font data.  ick. */
	if (!initfontdone) {
		int ch, i, n, bit;
		u_int32_t *pix, npix;

		for (ch = 0; ch < 256; ch++) {
			if (rc->rc_font->chars[ch].r == 0)
				continue;

			n = rc->rc_font->chars[ch].r->linelongs *
			    rc->rc_font->chars[ch].r->height;
			pix = rc->rc_font->chars[ch].r->pixels;

			for (i = 0; i < n; i++) {
				npix = 0;
				for (bit = 0; bit < 32; bit++)
					if (pix[i] & (1 << bit))
						npix |= (1 << (31 - bit));
				pix[i] = npix;
			}
		}
	}
#endif

	initfontdone = 1;
}

void
rcons_init(rc, mrow, mcol)
	struct rcons *rc;
	int mrow, mcol;
{
	struct raster *rp = rc->rc_sp;
	int i;

	rcons_initfont(rc, &gallant19);

	i = rp->height / rc->rc_font->height;
	rc->rc_maxrow = min(i, mrow);

	i = rp->width / rc->rc_font->width;
	rc->rc_maxcol = min(i, mcol);

	/* Center emulator screen (but align x origin to 32 bits) */
	rc->rc_xorigin =
	    ((rp->width - rc->rc_maxcol * rc->rc_font->width) / 2) & ~0x1f;
	rc->rc_yorigin =
	    (rp->height - rc->rc_maxrow * rc->rc_font->height) / 2;

	/* Raster width used for row copies */
	rc->rc_raswidth = rc->rc_maxcol * rc->rc_font->width;
	if (rc->rc_raswidth & 0x1f) {
		/* Pad to 32 bits */
		i = (rc->rc_raswidth + 0x1f) & ~0x1f;
		/* Make sure width isn't too wide */
		if (rc->rc_xorigin + i <= rp->width)
			rc->rc_raswidth = i;
	}

	rc->rc_ras_blank = RAS_CLEAR;
	rc->rc_bits = 0;

	/* If cursor position given, assume it's there and drawn. */
	if (*rc->rc_crowp != -1 && *rc->rc_ccolp != -1)
		rc->rc_bits |= RC_CURSOR;
}
