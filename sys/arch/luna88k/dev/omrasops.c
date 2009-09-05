/* $OpenBSD: omrasops.c,v 1.6 2009/09/05 14:09:35 miod Exp $ */
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
 * Designed speficically for 'm68k bitorder';
 *	- most significant byte is stored at lower address,
 *	- most significant bit is displayed at left most on screen.
 * Implementation relies on;
 *	- every memory references is done in aligned 32bit chunk,
 *	- font glyphs are stored in 32bit padded.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/rasops/rasops.h>

/* wscons emulator operations */
int	om_cursor(void *, int, int, int);
int	om_putchar(void *, int, int, u_int, long);
int	om_copycols(void *, int, int, int, int);
int	om_copyrows(void *, int, int, int num);
int	om_erasecols(void *, int, int, int, long);
int	om_eraserows(void *, int, int, long);

#define	ALL1BITS	(~0U)
#define	ALL0BITS	(0U)
#define	BLITWIDTH	(32)
#define	ALIGNMASK	(0x1f)
#define	BYTESDONE	(4)

#define	W(p) (*(u_int32_t *)(p))
#define	R(p) (*(u_int32_t *)((caddr_t)(p) + 0x40000))

/*
 * Blit a character at the specified co-ordinates.
 */
int
om_putchar(cookie, row, startcol, uc, attr)
	void *cookie;
	int row, startcol;
	u_int uc;
	long attr;
{
	struct rasops_info *ri = cookie;
	caddr_t p;
	int scanspan, startx, height, width, align, y;
	u_int32_t lmask, rmask, glyph, inverse;
	int i, fg, bg;
	u_int8_t *fb;

	scanspan = ri->ri_stride;
	y = ri->ri_font->fontheight * row;
	startx = ri->ri_font->fontwidth * startcol;
	height = ri->ri_font->fontheight;
	fb = ri->ri_font->data +
	    (uc - ri->ri_font->firstchar) * ri->ri_fontscale;
	ri->ri_ops.unpack_attr(cookie, attr, &fg, &bg, NULL);
	inverse = (bg != 0) ? ALL1BITS : ALL0BITS;

	p = (caddr_t)ri->ri_bits + y * scanspan + ((startx / 32) * 4);
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
		caddr_t q = p;
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
om_erasecols(cookie, row, startcol, ncols, attr)
	void *cookie;
	int row, startcol, ncols;
	long attr;
{
        struct rasops_info *ri = cookie;
        caddr_t p;
        int scanspan, startx, height, width, align, w, y;
        u_int32_t lmask, rmask, fill;

        scanspan = ri->ri_stride;
        y = ri->ri_font->fontheight * row;
        startx = ri->ri_font->fontwidth * startcol;
        height = ri->ri_font->fontheight;
        w = ri->ri_font->fontwidth * ncols;
	fill = (attr != 0) ? ALL1BITS : ALL0BITS;

	p = (caddr_t)ri->ri_bits + y * scanspan + ((startx / 32) * 4);
	align = startx & ALIGNMASK;
	width = w + align;
	lmask = ALL1BITS >> align;
	rmask = ALL1BITS << (-width & ALIGNMASK);
	if (width <= BLITWIDTH) {
		lmask &= rmask;
		fill &= lmask;
		while (height > 0) {
			W(p) = (R(p) & ~lmask) | fill;
			p += scanspan;
			height--;
		}
	}
	else {
		caddr_t q = p;
		while (height > 0) {
			W(p) = (R(p) & ~lmask) | (fill & lmask);
			width -= 2 * BLITWIDTH;
			while (width > 0) {
				p += BYTESDONE;
				W(p) = fill;
				width -= BLITWIDTH;
			}
			p += BYTESDONE;
			W(p) = (fill & rmask) | (R(p) & ~rmask);

			p = (q += scanspan);
			width = w + align;
			height--;
		}
	}

	return 0;
}

int
om_eraserows(cookie, startrow, nrows, attr)
	void *cookie;
	int startrow, nrows;
	long attr;
{
	struct rasops_info *ri = cookie;
	caddr_t p, q;
	int scanspan, starty, height, width, w;
	u_int32_t rmask, fill;

	scanspan = ri->ri_stride;
	starty = ri->ri_font->fontheight * startrow;
	height = ri->ri_font->fontheight * nrows;
	w = ri->ri_emuwidth;
	fill = (attr == 1) ? ALL1BITS : ALL0BITS;

	p = (caddr_t)ri->ri_bits + starty * scanspan;
	width = w;
        rmask = ALL1BITS << (-width & ALIGNMASK);
	q = p;
	while (height > 0) {
		W(p) = fill;				/* always aligned */
		width -= 2 * BLITWIDTH;
		while (width > 0) {
			p += BYTESDONE;
			W(p) = fill;
			width -= BLITWIDTH;
		}
		p += BYTESDONE;
		W(p) = (fill & rmask) | (R(p) & ~rmask);
		p = (q += scanspan);
		width = w;
		height--;
	}

	return 0;
}

int
om_copyrows(cookie, srcrow, dstrow, nrows)
	void *cookie;
	int srcrow, dstrow, nrows;
{
        struct rasops_info *ri = cookie;
        caddr_t p, q;
	int scanspan, offset, srcy, height, width, w;
        u_int32_t rmask;
        
	scanspan = ri->ri_stride;
	height = ri->ri_font->fontheight * nrows;
	offset = (dstrow - srcrow) * scanspan * ri->ri_font->fontheight;
	srcy = ri->ri_font->fontheight * srcrow;
	if (srcrow < dstrow && srcrow + nrows > dstrow) {
		scanspan = -scanspan;
		srcy += height;
	}

	p = (caddr_t)ri->ri_bits + srcy * ri->ri_stride;
	w = ri->ri_emuwidth;
	width = w;
	rmask = ALL1BITS << (-width & ALIGNMASK);
	q = p;
	while (height > 0) {
		W(p + offset) = R(p);			/* always aligned */
		width -= 2 * BLITWIDTH;
		while (width > 0) {
			p += BYTESDONE;
			W(p + offset) = R(p);
			width -= BLITWIDTH;
		}
		p += BYTESDONE;
		W(p + offset) = (R(p) & rmask) | (R(p + offset) & ~rmask);

		p = (q += scanspan);
		width = w;
		height--;
	}

	return 0;
}

int
om_copycols(cookie, startrow, srccol, dstcol, ncols)
	void *cookie;
	int startrow, srccol, dstcol, ncols;
{
	struct rasops_info *ri = cookie;
	caddr_t sp, dp, basep;
	int scanspan, height, width, align, shift, w, y, srcx, dstx;
	u_int32_t lmask, rmask;

	scanspan = ri->ri_stride;
	y = ri->ri_font->fontheight * startrow;
	srcx = ri->ri_font->fontwidth * srccol;
	dstx = ri->ri_font->fontwidth * dstcol;
	height = ri->ri_font->fontheight;
	w = ri->ri_font->fontwidth * ncols;
	basep = (caddr_t)ri->ri_bits + y * scanspan;

	align = shift = srcx & ALIGNMASK;
	width = w + align;
	align = dstx & ALIGNMASK;
	lmask = ALL1BITS >> align;
	rmask = ALL1BITS << (-(w + align) & ALIGNMASK);
	shift = align - shift; 
	sp = basep + (srcx / 32) * 4;
	dp = basep + (dstx / 32) * 4;

	if (shift != 0)
		goto hardluckalignment;

	/* alignments comfortably match */
	if (width <= BLITWIDTH) {
		lmask &= rmask;
		while (height > 0) {
			W(dp) = (R(dp) & ~lmask) | (R(sp) & lmask);
			dp += scanspan;
			sp += scanspan;
			height--;
		}
	}
	/* copy forward (left-to-right) */
	else if (dstcol < srccol || srccol + ncols < dstcol) {
		caddr_t sq = sp, dq = dp;

		w = width;
		while (height > 0) {
			W(dp) = (R(dp) & ~lmask) | (R(sp) & lmask);
			width -= 2 * BLITWIDTH;
			while (width > 0) {
				sp += BYTESDONE;
				dp += BYTESDONE;
				W(dp) = R(sp);
				width -= BLITWIDTH;
			}
			sp += BYTESDONE;
			dp += BYTESDONE;
			W(dp) = (R(sp) & rmask) | (R(dp) & ~rmask);
			sp = (sq += scanspan);
			dp = (dq += scanspan);
			width = w;
			height--;
		}
	}
	/* copy backward (right-to-left) */
	else {
		caddr_t sq, dq;

		sq = (sp += width / 32 * 4);
		dq = (dp += width / 32 * 4);
		w = width;
		while (height > 0) {
			W(dp) = (R(sp) & rmask) | (R(dp) & ~rmask);
			width -= 2 * BLITWIDTH;
			while (width > 0) {
				sp -= BYTESDONE;
				dp -= BYTESDONE;
				W(dp) = R(sp);
				width -= BLITWIDTH;
			}
			sp -= BYTESDONE;
			dp -= BYTESDONE;
			W(dp) = (R(dp) & ~lmask) | (R(sp) & lmask);

			sp = (sq += scanspan);
			dp = (dq += scanspan);
			width = w;
			height--;
		}
	}
	return 0;

    hardluckalignment:
	/* alignments painfully disagree */

	return 0;
}

/*
 * Position|{enable|disable} the cursor at the specified location.
 */
int
om_cursor(cookie, on, row, col)
	void *cookie;
	int on, row, col;
{
	struct rasops_info *ri = cookie;
	caddr_t p;
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

	p = (caddr_t)ri->ri_bits + y * scanspan + ((startx / 32) * 4);
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
		caddr_t q = p;

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
