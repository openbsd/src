/* $OpenBSD: omrasops.c,v 1.10 2013/11/16 22:45:37 aoyama Exp $ */
/* $NetBSD: omrasops.c,v 1.1 2000/01/05 08:48:56 nisimura Exp $ */

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Tohru Nishimura.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Designed specifically for 'm68k bitorder';
 *	- most significant byte is stored at lower address,
 *	- most significant bit is displayed at left most on screen.
 * Implementation relies on;
 *	- every memory reference is done in aligned 32bit chunks,
 *	- font glyphs are stored in 32bit padded.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/rasops/rasops.h>

#include <luna88k/dev/omrasops.h>

/* wscons emulator operations */
int	om_cursor(void *, int, int, int);
int	om_putchar(void *, int, int, u_int, long);
int	om_copycols(void *, int, int, int, int);
int	om_copyrows(void *, int, int, int num);
int	om_erasecols(void *, int, int, int, long);
int	om_eraserows(void *, int, int, long);

/* internal functions (for 1bpp, in omrasops1.c) */
int	om_windowmove1(struct rasops_info *, u_int16_t, u_int16_t,
		u_int16_t, u_int16_t, u_int16_t, u_int16_t, int16_t,
		int16_t /* ignored */);

#define	ALL1BITS	(~0U)
#define	ALL0BITS	(0U)
#define	BLITWIDTH	(32)
#define	ALIGNMASK	(0x1f)
#define	BYTESDONE	(4)

/*
 * Blit a character at the specified co-ordinates.
 */
int
om_putchar(void *cookie, int row, int startcol, u_int uc, long attr)
{
	struct rasops_info *ri = cookie;
	u_int8_t *p;
	int scanspan, startx, height, width, align, y;
	u_int32_t lmask, rmask, glyph, inverse;
	int i, fg, bg;
	u_int8_t *fb;

	scanspan = ri->ri_stride;
	y = ri->ri_font->fontheight * row;
	startx = ri->ri_font->fontwidth * startcol;
	height = ri->ri_font->fontheight;
	fb = (u_int8_t *)ri->ri_font->data +
	    (uc - ri->ri_font->firstchar) * ri->ri_fontscale;
	ri->ri_ops.unpack_attr(cookie, attr, &fg, &bg, NULL);
	inverse = (bg != 0) ? ALL1BITS : ALL0BITS;

	p = (u_int8_t *)ri->ri_bits + y * scanspan + ((startx / 32) * 4);
	align = startx & ALIGNMASK;
	width = ri->ri_font->fontwidth + align;
	lmask = ALL1BITS >> align;
	rmask = ALL1BITS << (-width & ALIGNMASK);
	if (width <= BLITWIDTH) {
		lmask &= rmask;
		while (height > 0) {
			glyph = 0;
			for (i = ri->ri_font->stride; i != 0; i--)
				glyph = (glyph << 8) | *fb++;
			glyph <<= (4 - ri->ri_font->stride) * NBBY;
			glyph = (glyph >> align) ^ inverse;
			W(p) = (R(p) & ~lmask) | (glyph & lmask);
			p += scanspan;
			height--;
		}
	}
	else {
		u_int8_t *q = p;
		u_int32_t lhalf, rhalf;

		while (height > 0) {
			glyph = 0;
			for (i = ri->ri_font->stride; i != 0; i--)
				glyph = (glyph << 8) | *fb++;
			glyph <<= (4 - ri->ri_font->stride) * NBBY;
			lhalf = (glyph >> align) ^ inverse;
			W(p) = (R(p) & ~lmask) | (lhalf & lmask);
			p += BYTESDONE;
			rhalf = (glyph << (BLITWIDTH - align)) ^ inverse;
			W(p) = (rhalf & rmask) | (R(p) & ~rmask);

			p = (q += scanspan);
			height--;
		}
	}

	return 0;
}

int
om_erasecols(void *cookie, int row, int col, int num, long attr)
{
	struct rasops_info *ri = cookie;
	int fg, bg;
	int snum, scol, srow;

	ri->ri_ops.unpack_attr(cookie, attr, &fg, &bg, NULL);

	snum = num * ri->ri_font->fontwidth;
	scol = col * ri->ri_font->fontwidth  + ri->ri_xorigin;
	srow = row * ri->ri_font->fontheight + ri->ri_yorigin;

	/*
	 * If this is too tricky for the simple raster ops engine,
	 * pass the fun to rasops.
	 */
	if (om_windowmove1(ri, scol, srow, scol, srow, snum,
	    ri->ri_font->fontheight, RR_CLEAR, 0xff ^ bg) != 0)
		rasops_erasecols(cookie, row, col, num, attr);

	return 0;
}

int
om_eraserows(void *cookie, int row, int num, long attr)
{
	struct rasops_info *ri = cookie;
	int fg, bg;
	int srow, snum;
	int rc;

	ri->ri_ops.unpack_attr(cookie, attr, &fg, &bg, NULL);
	bg ^= 0xff;

	if (num == ri->ri_rows && (ri->ri_flg & RI_FULLCLEAR)) {
		rc = om_windowmove1(ri, 0, 0, 0, 0, ri->ri_width, ri->ri_height,
		    RR_CLEAR, bg);
	} else {
		srow = row * ri->ri_font->fontheight + ri->ri_yorigin;
		snum = num * ri->ri_font->fontheight;
		rc = om_windowmove1(ri, ri->ri_xorigin, srow, ri->ri_xorigin,
		    srow, ri->ri_emuwidth, snum, RR_CLEAR, bg);
	}
	if (rc != 0)
		rasops_eraserows(cookie, row, num, attr);

	return 0;
}

int
om_copyrows(void *cookie, int src, int dst, int n)
{
	struct rasops_info *ri = cookie;

	n   *= ri->ri_font->fontheight;
	src *= ri->ri_font->fontheight;
	dst *= ri->ri_font->fontheight;

	om_windowmove1(ri, ri->ri_xorigin, ri->ri_yorigin + src,
		ri->ri_xorigin, ri->ri_yorigin + dst,
		ri->ri_emuwidth, n, RR_COPY, 0xff);

	return 0;
}

int
om_copycols(void *cookie, int row, int src, int dst, int n)
{
	struct rasops_info *ri = cookie;

	n   *= ri->ri_font->fontwidth;
	src *= ri->ri_font->fontwidth;
	dst *= ri->ri_font->fontwidth;
	row *= ri->ri_font->fontheight;

	om_windowmove1(ri, ri->ri_xorigin + src, ri->ri_yorigin + row,
		ri->ri_xorigin + dst, ri->ri_yorigin + row,
		n, ri->ri_font->fontheight, RR_COPY, 0xff);

	return 0;
}

/*
 * Position|{enable|disable} the cursor at the specified location.
 */
int
om_cursor(void *cookie, int on, int row, int col)
{
	struct rasops_info *ri = cookie;
	u_int8_t *p;
	int scanspan, startx, height, width, align, y;
	u_int32_t lmask, rmask, image;

	if (!on) {
		/* make sure it's on */
		if ((ri->ri_flg & RI_CURSOR) == 0)
			return 0;

		row = ri->ri_crow;
		col = ri->ri_ccol;
	} else {
		/* unpaint the old copy. */
		ri->ri_crow = row;
		ri->ri_ccol = col;
	}

	scanspan = ri->ri_stride;
	y = ri->ri_font->fontheight * row;
	startx = ri->ri_font->fontwidth * col;
	height = ri->ri_font->fontheight;

	p = (u_int8_t *)ri->ri_bits + y * scanspan + ((startx / 32) * 4);
	align = startx & ALIGNMASK;
	width = ri->ri_font->fontwidth + align;
	lmask = ALL1BITS >> align;
	rmask = ALL1BITS << (-width & ALIGNMASK);
	if (width <= BLITWIDTH) {
		lmask &= rmask;
		while (height > 0) {
			image = R(p);
			W(p) = (image & ~lmask) | ((image ^ ALL1BITS) & lmask);
			p += scanspan;
			height--;
		}
	}
	else {
		u_int8_t *q = p;

		while (height > 0) {
			image = R(p);
			W(p) = (image & ~lmask) | ((image ^ ALL1BITS) & lmask);
			p += BYTESDONE;
			image = R(p);
			W(p) = ((image ^ ALL1BITS) & rmask) | (image & ~rmask);

			p = (q += scanspan);
			height--;
		}
	}
	ri->ri_flg ^= RI_CURSOR;

	return 0;
}
