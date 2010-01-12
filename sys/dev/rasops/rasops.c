/*	$OpenBSD: rasops.c,v 1.21 2010/01/12 00:41:03 chl Exp $	*/
/*	$NetBSD: rasops.c,v 1.35 2001/02/02 06:01:01 marcus Exp $	*/

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>

#include <machine/endian.h>

#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wsconsio.h>
#include <dev/wsfont/wsfont.h>
#include <dev/rasops/rasops.h>

#ifndef _KERNEL
#include <errno.h>
#endif

/* ANSI colormap (R,G,B) */

#define	NORMAL_BLACK	0x000000
#define	NORMAL_RED	0x7f0000
#define	NORMAL_GREEN	0x007f00
#define	NORMAL_BROWN	0x7f7f00
#define	NORMAL_BLUE	0x00007f
#define	NORMAL_MAGENTA	0x7f007f
#define	NORMAL_CYAN	0x007f7f
#define	NORMAL_WHITE	0xc7c7c7	/* XXX too dim? */

#define	HILITE_BLACK	0x7f7f7f
#define	HILITE_RED	0xff0000
#define	HILITE_GREEN	0x00ff00
#define	HILITE_BROWN	0xffff00
#define	HILITE_BLUE	0x0000ff
#define	HILITE_MAGENTA	0xff00ff
#define	HILITE_CYAN	0x00ffff
#define	HILITE_WHITE	0xffffff

const u_char rasops_cmap[256 * 3] = {
#define	_C(x)	((x) & 0xff0000) >> 16, ((x) & 0x00ff00) >> 8, ((x) & 0x0000ff)

	_C(NORMAL_BLACK),
	_C(NORMAL_RED),
	_C(NORMAL_GREEN),
	_C(NORMAL_BROWN),
	_C(NORMAL_BLUE),
	_C(NORMAL_MAGENTA),
	_C(NORMAL_CYAN),
	_C(NORMAL_WHITE),

	_C(HILITE_BLACK),
	_C(HILITE_RED),
	_C(HILITE_GREEN),
	_C(HILITE_BROWN),
	_C(HILITE_BLUE),
	_C(HILITE_MAGENTA),
	_C(HILITE_CYAN),
	_C(HILITE_WHITE),

	/*
	 * For the cursor, we need the last 16 colors to be the
	 * opposite of the first 16. Fill the intermediate space with
	 * white completely for simplicity.
	 */
#define _CMWHITE16 \
	_C(HILITE_WHITE), _C(HILITE_WHITE), _C(HILITE_WHITE), _C(HILITE_WHITE), \
	_C(HILITE_WHITE), _C(HILITE_WHITE), _C(HILITE_WHITE), _C(HILITE_WHITE), \
	_C(HILITE_WHITE), _C(HILITE_WHITE), _C(HILITE_WHITE), _C(HILITE_WHITE), \
	_C(HILITE_WHITE), _C(HILITE_WHITE), _C(HILITE_WHITE), _C(HILITE_WHITE),
	_CMWHITE16 _CMWHITE16 _CMWHITE16 _CMWHITE16 _CMWHITE16
	_CMWHITE16 _CMWHITE16 _CMWHITE16 _CMWHITE16 _CMWHITE16
	_CMWHITE16 _CMWHITE16 _CMWHITE16 _CMWHITE16
#undef _CMWHITE16

	_C(~HILITE_WHITE),
	_C(~HILITE_CYAN),
	_C(~HILITE_MAGENTA),
	_C(~HILITE_BLUE),
	_C(~HILITE_BROWN),
	_C(~HILITE_GREEN),
	_C(~HILITE_RED),
	_C(~HILITE_BLACK),

	_C(~NORMAL_WHITE),
	_C(~NORMAL_CYAN),
	_C(~NORMAL_MAGENTA),
	_C(~NORMAL_BLUE),
	_C(~NORMAL_BROWN),
	_C(~NORMAL_GREEN),
	_C(~NORMAL_RED),
	_C(~NORMAL_BLACK),

#undef	_C
};

/* True if color is gray */
const u_char rasops_isgray[16] = {
	1, 0, 0, 0,
	0, 0, 0, 1,
	1, 0, 0, 0,
	0, 0, 0, 1
};

/* Generic functions */
int	rasops_copycols(void *, int, int, int, int);
int	rasops_copyrows(void *, int, int, int);
int	rasops_mapchar(void *, int, u_int *);
int	rasops_cursor(void *, int, int, int);
int	rasops_alloc_cattr(void *, int, int, int, long *);
int	rasops_alloc_mattr(void *, int, int, int, long *);
int	rasops_do_cursor(struct rasops_info *);
void	rasops_init_devcmap(struct rasops_info *);
void	rasops_unpack_attr(void *, long, int *, int *, int *);
#if NRASOPS_BSWAP > 0
static void slow_ovbcopy(void *, void *, size_t);
#endif
#if NRASOPS_ROTATION > 0
void	rasops_copychar(void *, int, int, int, int);
int	rasops_copycols_rotated(void *, int, int, int, int);
int	rasops_copyrows_rotated(void *, int, int, int);
int	rasops_erasecols_rotated(void *, int, int, int, long);
int	rasops_eraserows_rotated(void *, int, int, long);
int	rasops_putchar_rotated(void *, int, int, u_int, long);
void	rasops_rotate_font(int *);

/*
 * List of all rotated fonts
 */
SLIST_HEAD(, rotatedfont) rotatedfonts = SLIST_HEAD_INITIALIZER(rotatedfonts);
struct	rotatedfont {
	SLIST_ENTRY(rotatedfont) rf_next;
	int rf_cookie;
	int rf_rotated;
};
#endif

/*
 * Initialize a 'rasops_info' descriptor.
 */
int
rasops_init(ri, wantrows, wantcols)
	struct rasops_info *ri;
	int wantrows, wantcols;
{

#ifdef _KERNEL
	/* Select a font if the caller doesn't care */
	if (ri->ri_font == NULL) {
		int cookie;

		wsfont_init();

		if (ri->ri_width > 80*12)
			/* High res screen, choose a big font */
			cookie = wsfont_find(NULL, 12, 0, 0);
		else
			/*  lower res, choose a 8 pixel wide font */
			cookie = wsfont_find(NULL, 8, 0, 0);

		if (cookie <= 0)
			cookie = wsfont_find(NULL, 0, 0, 0);

		if (cookie <= 0) {
			printf("rasops_init: font table is empty\n");
			return (-1);
		}

#if NRASOPS_ROTATION > 0
		/*
		 * Pick the rotated version of this font. This will create it
		 * if necessary.
		 */
		if (ri->ri_flg & RI_ROTATE_CW)
			rasops_rotate_font(&cookie);
#endif

		if (wsfont_lock(cookie, &ri->ri_font,
		    WSDISPLAY_FONTORDER_L2R, WSDISPLAY_FONTORDER_L2R) <= 0) {
			printf("rasops_init: couldn't lock font\n");
			return (-1);
		}

		ri->ri_wsfcookie = cookie;
	}
#endif

	/* This should never happen in reality... */
#ifdef DEBUG
	if ((long)ri->ri_bits & 3) {
		printf("rasops_init: bits not aligned on 32-bit boundary\n");
		return (-1);
	}

	if ((int)ri->ri_stride & 3) {
		printf("rasops_init: stride not aligned on 32-bit boundary\n");
		return (-1);
	}
#endif

	if (rasops_reconfig(ri, wantrows, wantcols))
		return (-1);

	rasops_init_devcmap(ri);
	return (0);
}

/*
 * Reconfigure (because parameters have changed in some way).
 */
int
rasops_reconfig(ri, wantrows, wantcols)
	struct rasops_info *ri;
	int wantrows, wantcols;
{
	int l, bpp, s;

	s = splhigh();

	if (ri->ri_font->fontwidth > 32 || ri->ri_font->fontwidth < 4)
		panic("rasops_init: fontwidth assumptions botched!");

	/* Need this to frob the setup below */
	bpp = (ri->ri_depth == 15 ? 16 : ri->ri_depth);

	if ((ri->ri_flg & RI_CFGDONE) != 0)
		ri->ri_bits = ri->ri_origbits;

	/* Don't care if the caller wants a hideously small console */
	if (wantrows < 10)
		wantrows = 10;

	if (wantcols < 20)
		wantcols = 20;

	/* Now constrain what they get */
	ri->ri_emuwidth = ri->ri_font->fontwidth * wantcols;
	ri->ri_emuheight = ri->ri_font->fontheight * wantrows;

	if (ri->ri_emuwidth > ri->ri_width)
		ri->ri_emuwidth = ri->ri_width;

	if (ri->ri_emuheight > ri->ri_height)
		ri->ri_emuheight = ri->ri_height;

	/* Reduce width until aligned on a 32-bit boundary */
	while ((ri->ri_emuwidth * bpp & 31) != 0)
		ri->ri_emuwidth--;

#if NRASOPS_ROTATION > 0
	if (ri->ri_flg & RI_ROTATE_CW) {
		ri->ri_rows = ri->ri_emuwidth / ri->ri_font->fontwidth;
		ri->ri_cols = ri->ri_emuheight / ri->ri_font->fontheight;
	} else
#endif
	{
		ri->ri_cols = ri->ri_emuwidth / ri->ri_font->fontwidth;
		ri->ri_rows = ri->ri_emuheight / ri->ri_font->fontheight;
	}
	ri->ri_emustride = ri->ri_emuwidth * bpp >> 3;
	ri->ri_delta = ri->ri_stride - ri->ri_emustride;
	ri->ri_ccol = 0;
	ri->ri_crow = 0;
	ri->ri_pelbytes = bpp >> 3;

	ri->ri_xscale = (ri->ri_font->fontwidth * bpp) >> 3;
	ri->ri_yscale = ri->ri_font->fontheight * ri->ri_stride;
	ri->ri_fontscale = ri->ri_font->fontheight * ri->ri_font->stride;

#ifdef DEBUG
	if ((ri->ri_delta & 3) != 0)
		panic("rasops_init: ri_delta not aligned on 32-bit boundary");
#endif
	/* Clear the entire display */
	if ((ri->ri_flg & RI_CLEAR) != 0) {
		memset(ri->ri_bits, 0, ri->ri_stride * ri->ri_height);
		ri->ri_flg &= ~RI_CLEARMARGINS;
	}

	/* Now centre our window if needs be */
	ri->ri_origbits = ri->ri_bits;

	if ((ri->ri_flg & RI_CENTER) != 0) {
		ri->ri_bits += (((ri->ri_width * bpp >> 3) -
		    ri->ri_emustride) >> 1) & ~3;
		ri->ri_bits += ((ri->ri_height - ri->ri_emuheight) >> 1) *
		    ri->ri_stride;

		ri->ri_yorigin = (int)(ri->ri_bits - ri->ri_origbits)
		   / ri->ri_stride;
		ri->ri_xorigin = (((int)(ri->ri_bits - ri->ri_origbits)
		   % ri->ri_stride) * 8 / bpp);
	} else
		ri->ri_xorigin = ri->ri_yorigin = 0;

	/* Clear the margins */
	if ((ri->ri_flg & RI_CLEARMARGINS) != 0) {
		memset(ri->ri_origbits, 0, ri->ri_bits - ri->ri_origbits);
		for (l = 0; l < ri->ri_emuheight; l++)
			memset(ri->ri_bits + ri->ri_emustride +
			    l * ri->ri_stride, 0,
			    ri->ri_stride - ri->ri_emustride);
		memset(ri->ri_bits + ri->ri_emuheight * ri->ri_stride, 0,
		    (ri->ri_origbits + ri->ri_height * ri->ri_stride) -
		    (ri->ri_bits + ri->ri_emuheight * ri->ri_stride));
	}

	/*
	 * Fill in defaults for operations set.  XXX this nukes private
	 * routines used by accelerated fb drivers.
	 */
	ri->ri_ops.mapchar = rasops_mapchar;
	ri->ri_ops.copyrows = rasops_copyrows;
	ri->ri_ops.copycols = rasops_copycols;
	ri->ri_ops.erasecols = rasops_erasecols;
	ri->ri_ops.eraserows = rasops_eraserows;
	ri->ri_ops.cursor = rasops_cursor;
	ri->ri_ops.unpack_attr = rasops_unpack_attr;
	ri->ri_do_cursor = rasops_do_cursor;
	ri->ri_updatecursor = NULL;

	if (ri->ri_depth < 8 || (ri->ri_flg & RI_FORCEMONO) != 0) {
		ri->ri_ops.alloc_attr = rasops_alloc_mattr;
		ri->ri_caps = WSSCREEN_UNDERLINE | WSSCREEN_REVERSE;
	} else {
		ri->ri_ops.alloc_attr = rasops_alloc_cattr;
		ri->ri_caps = WSSCREEN_UNDERLINE | WSSCREEN_HILIT |
		    WSSCREEN_WSCOLORS | WSSCREEN_REVERSE;
	}

	switch (ri->ri_depth) {
#if NRASOPS1 > 0
	case 1:
		rasops1_init(ri);
		break;
#endif
#if NRASOPS2 > 0
	case 2:
		rasops2_init(ri);
		break;
#endif
#if NRASOPS4 > 0
	case 4:
		rasops4_init(ri);
		break;
#endif
#if NRASOPS8 > 0
	case 8:
		rasops8_init(ri);
		break;
#endif
#if NRASOPS15 > 0 || NRASOPS16 > 0
	case 15:
	case 16:
		rasops15_init(ri);
		break;
#endif
#if NRASOPS24 > 0
	case 24:
		rasops24_init(ri);
		break;
#endif
#if NRASOPS32 > 0
	case 32:
		rasops32_init(ri);
		break;
#endif
	default:
		ri->ri_flg &= ~RI_CFGDONE;
		splx(s);
		return (-1);
	}

#if NRASOPS_ROTATION > 0
	if (ri->ri_flg & RI_ROTATE_CW) {
		ri->ri_real_ops = ri->ri_ops;
		ri->ri_ops.copycols = rasops_copycols_rotated;
		ri->ri_ops.copyrows = rasops_copyrows_rotated;
		ri->ri_ops.erasecols = rasops_erasecols_rotated;
		ri->ri_ops.eraserows = rasops_eraserows_rotated;
		ri->ri_ops.putchar = rasops_putchar_rotated;
	}
#endif

	ri->ri_flg |= RI_CFGDONE;
	splx(s);
	return (0);
}

/*
 * Map a character.
 */
int
rasops_mapchar(cookie, c, cp)
	void *cookie;
	int c;
	u_int *cp;
{
	struct rasops_info *ri;

	ri = (struct rasops_info *)cookie;

#ifdef DIAGNOSTIC
	if (ri->ri_font == NULL)
		panic("rasops_mapchar: no font selected");
#endif
	if (ri->ri_font->encoding != WSDISPLAY_FONTENC_ISO) {

		if ( (c = wsfont_map_unichar(ri->ri_font, c)) < 0) {

			*cp = ' ';
			return (0);

		}
	}


	if (c < ri->ri_font->firstchar) {
		*cp = ' ';
		return (0);
	}

	if (c - ri->ri_font->firstchar >= ri->ri_font->numchars) {
		*cp = ' ';
		return (0);
	}

	*cp = c;
	return (5);
}

/*
 * Allocate a color attribute.
 */
int
rasops_alloc_cattr(cookie, fg, bg, flg, attr)
	void *cookie;
	int fg, bg, flg;
	long *attr;
{
	int swap;

#ifdef RASOPS_CLIPPING
	fg &= 7;
	bg &= 7;
#endif
	if ((flg & WSATTR_BLINK) != 0)
		return (EINVAL);

	if ((flg & WSATTR_WSCOLORS) == 0) {
		fg = WSCOL_WHITE;
		bg = WSCOL_BLACK;
	}

	if ((flg & WSATTR_REVERSE) != 0) {
		swap = fg;
		fg = bg;
		bg = swap;
	}

	if ((flg & WSATTR_HILIT) != 0)
		fg += 8;

	flg = ((flg & WSATTR_UNDERLINE) ? 1 : 0);

	if (rasops_isgray[fg])
		flg |= 2;

	if (rasops_isgray[bg])
		flg |= 4;

	*attr = (bg << 16) | (fg << 24) | flg;
	return (0);
}

/*
 * Allocate a mono attribute.
 */
int
rasops_alloc_mattr(cookie, fg, bg, flg, attr)
	void *cookie;
	int fg, bg, flg;
	long *attr;
{
	int swap;

	if ((flg & (WSATTR_BLINK | WSATTR_HILIT | WSATTR_WSCOLORS)) != 0)
		return (EINVAL);

	fg = 1;
	bg = 0;

	if ((flg & WSATTR_REVERSE) != 0) {
		swap = fg;
		fg = bg;
		bg = swap;
	}

	*attr = (bg << 16) | (fg << 24) | ((flg & WSATTR_UNDERLINE) ? 7 : 6);
	return (0);
}

/*
 * Copy rows.
 */
int
rasops_copyrows(cookie, src, dst, num)
	void *cookie;
	int src, dst, num;
{
	int32_t *sp, *dp, *srp, *drp;
	struct rasops_info *ri;
	int n8, n1, cnt, delta;

	ri = (struct rasops_info *)cookie;

#ifdef RASOPS_CLIPPING
	if (dst == src)
		return 0;

	if (src < 0) {
		num += src;
		src = 0;
	}

	if ((src + num) > ri->ri_rows)
		num = ri->ri_rows - src;

	if (dst < 0) {
		num += dst;
		dst = 0;
	}

	if ((dst + num) > ri->ri_rows)
		num = ri->ri_rows - dst;

	if (num <= 0)
		return 0;
#endif

	num *= ri->ri_font->fontheight;
	n8 = ri->ri_emustride >> 5;
	n1 = (ri->ri_emustride >> 2) & 7;

	if (dst < src) {
		srp = (int32_t *)(ri->ri_bits + src * ri->ri_yscale);
		drp = (int32_t *)(ri->ri_bits + dst * ri->ri_yscale);
		delta = ri->ri_stride;
	} else {
		src = ri->ri_font->fontheight * src + num - 1;
		dst = ri->ri_font->fontheight * dst + num - 1;
		srp = (int32_t *)(ri->ri_bits + src * ri->ri_stride);
		drp = (int32_t *)(ri->ri_bits + dst * ri->ri_stride);
		delta = -ri->ri_stride;
	}

	while (num--) {
		dp = drp;
		sp = srp;
		DELTA(drp, delta, int32_t *);
		DELTA(srp, delta, int32_t *);

		for (cnt = n8; cnt; cnt--) {
			dp[0] = sp[0];
			dp[1] = sp[1];
			dp[2] = sp[2];
			dp[3] = sp[3];
			dp[4] = sp[4];
			dp[5] = sp[5];
			dp[6] = sp[6];
			dp[7] = sp[7];
			dp += 8;
			sp += 8;
		}

		for (cnt = n1; cnt; cnt--)
			*dp++ = *sp++;
	}

	return 0;
}

/*
 * Copy columns. This is slow, and hard to optimize due to alignment,
 * and the fact that we have to copy both left->right and right->left.
 * We simply cop-out here and use ovbcopy(), since it handles all of
 * these cases anyway.
 */
int
rasops_copycols(cookie, row, src, dst, num)
	void *cookie;
	int row, src, dst, num;
{
	struct rasops_info *ri;
	u_char *sp, *dp;
	int height;

	ri = (struct rasops_info *)cookie;

#ifdef RASOPS_CLIPPING
	if (dst == src)
		return 0;

	/* Catches < 0 case too */
	if ((unsigned)row >= (unsigned)ri->ri_rows)
		return 0;

	if (src < 0) {
		num += src;
		src = 0;
	}

	if ((src + num) > ri->ri_cols)
		num = ri->ri_cols - src;

	if (dst < 0) {
		num += dst;
		dst = 0;
	}

	if ((dst + num) > ri->ri_cols)
		num = ri->ri_cols - dst;

	if (num <= 0)
		return 0;
#endif

	num *= ri->ri_xscale;
	row *= ri->ri_yscale;
	height = ri->ri_font->fontheight;

	sp = ri->ri_bits + row + src * ri->ri_xscale;
	dp = ri->ri_bits + row + dst * ri->ri_xscale;

#if NRASOPS_BSWAP > 0
	if (ri->ri_flg & RI_BSWAP) {
		while (height--) {
			slow_ovbcopy(sp, dp, num);
			dp += ri->ri_stride;
			sp += ri->ri_stride;
		}
	} else
#endif
	{
		while (height--) {
			ovbcopy(sp, dp, num);
			dp += ri->ri_stride;
			sp += ri->ri_stride;
		}
	}

	return 0;
}

/*
 * Turn cursor off/on.
 */
int
rasops_cursor(cookie, on, row, col)
	void *cookie;
	int on, row, col;
{
	struct rasops_info *ri;
	int rc;

	ri = (struct rasops_info *)cookie;

	/* Turn old cursor off */
	if ((ri->ri_flg & RI_CURSOR) != 0) {
#ifdef RASOPS_CLIPPING
		if ((ri->ri_flg & RI_CURSORCLIP) == 0)
#endif
			if ((rc = ri->ri_do_cursor(ri)) != 0)
				return rc;
		ri->ri_flg &= ~RI_CURSOR;
	}

	/* Select new cursor */
#ifdef RASOPS_CLIPPING
	ri->ri_flg &= ~RI_CURSORCLIP;

	if (row < 0 || row >= ri->ri_rows)
		ri->ri_flg |= RI_CURSORCLIP;
	else if (col < 0 || col >= ri->ri_cols)
		ri->ri_flg |= RI_CURSORCLIP;
#endif
	ri->ri_crow = row;
	ri->ri_ccol = col;

	if (ri->ri_updatecursor != NULL)
		ri->ri_updatecursor(ri);

	if (on) {
#ifdef RASOPS_CLIPPING
		if ((ri->ri_flg & RI_CURSORCLIP) == 0)
#endif
			if ((rc = ri->ri_do_cursor(ri)) != 0)
				return rc;
		ri->ri_flg |= RI_CURSOR;
	}

	return 0;
}

/*
 * Make the device colormap
 */
void
rasops_init_devcmap(ri)
	struct rasops_info *ri;
{
	int i;
#if NRASOPS15 > 0 || NRASOPS16 > 0 || NRASOPS24 > 0 || NRASOPS32 > 0
	const u_char *p;
#endif
#if NRASOPS4 > 0 || NRASOPS15 > 0 || NRASOPS16 > 0 || NRASOPS24 > 0 || NRASOPS32 > 0
	int c;
#endif

	if (ri->ri_depth == 1 || (ri->ri_flg & RI_FORCEMONO) != 0) {
		ri->ri_devcmap[0] = 0;
		for (i = 1; i < 16; i++)
			ri->ri_devcmap[i] = 0xffffffff;
		return;
	}

	switch (ri->ri_depth) {
#if NRASOPS2 > 0
	case 2:
		for (i = 1; i < 15; i++)
			ri->ri_devcmap[i] = 0xaaaaaaaa;

		ri->ri_devcmap[0] = 0;
		ri->ri_devcmap[8] = 0x55555555;
		ri->ri_devcmap[15] = 0xffffffff;
		return;
#endif
#if NRASOPS4 > 0
	case 4:
		for (i = 0; i < 16; i++) {
			c = i | (i << 4);
			ri->ri_devcmap[i] = c | (c<<8) | (c<<16) | (c<<24);
		}
		return;
#endif
#if NRASOPS8 > 0
	case 8:
		for (i = 0; i < 16; i++)
			ri->ri_devcmap[i] = i | (i<<8) | (i<<16) | (i<<24);
		return;
#endif
	default:
		break;
	}

#if NRASOPS15 > 0 || NRASOPS16 > 0 || NRASOPS24 > 0 || NRASOPS32 > 0
	p = rasops_cmap;

	for (i = 0; i < 16; i++) {
		if (ri->ri_rnum <= 8)
			c = (*p >> (8 - ri->ri_rnum)) << ri->ri_rpos;
		else
			c = (*p << (ri->ri_rnum - 8)) << ri->ri_rpos;
		p++;

		if (ri->ri_gnum <= 8)
			c |= (*p >> (8 - ri->ri_gnum)) << ri->ri_gpos;
		else
			c |= (*p << (ri->ri_gnum - 8)) << ri->ri_gpos;
		p++;

		if (ri->ri_bnum <= 8)
			c |= (*p >> (8 - ri->ri_bnum)) << ri->ri_bpos;
		else
			c |= (*p << (ri->ri_bnum - 8)) << ri->ri_bpos;
		p++;

		/* Fill the word for generic routines, which want this */
		if (ri->ri_depth == 24)
			c = c | ((c & 0xff) << 24);
		else if (ri->ri_depth <= 16)
			c = c | (c << 16);

		/* 24bpp does bswap on the fly. {32,16,15}bpp do it here. */
#if NRASOPS_BSWAP > 0
		if ((ri->ri_flg & RI_BSWAP) == 0)
			ri->ri_devcmap[i] = c;
		else if (ri->ri_depth == 32)
			ri->ri_devcmap[i] = swap32(c);
		else if (ri->ri_depth == 16 || ri->ri_depth == 15)
			ri->ri_devcmap[i] = swap16(c);
		else
			ri->ri_devcmap[i] = c;
#else
		ri->ri_devcmap[i] = c;
#endif
	}
#endif
}

/*
 * Unpack a rasops attribute
 */
void
rasops_unpack_attr(cookie, attr, fg, bg, underline)
	void *cookie;
	long attr;
	int *fg, *bg, *underline;
{
	*fg = ((u_int)attr >> 24) & 0xf;
	*bg = ((u_int)attr >> 16) & 0xf;
	if (underline != NULL)
		*underline = (u_int)attr & 1;
}

/*
 * Erase rows
 */
int
rasops_eraserows(cookie, row, num, attr)
	void *cookie;
	int row, num;
	long attr;
{
	struct rasops_info *ri;
	int np, nw, cnt, delta;
	int32_t *dp, clr;

	ri = (struct rasops_info *)cookie;

#ifdef RASOPS_CLIPPING
	if (row < 0) {
		num += row;
		row = 0;
	}

	if ((row + num) > ri->ri_rows)
		num = ri->ri_rows - row;

	if (num <= 0)
		return 0;
#endif

	clr = ri->ri_devcmap[(attr >> 16) & 0xf];

	/*
	 * XXX The wsdisplay_emulops interface seems a little deficient in
	 * that there is no way to clear the *entire* screen. We provide a
	 * workaround here: if the entire console area is being cleared, and
	 * the RI_FULLCLEAR flag is set, clear the entire display.
	 */
	if (num == ri->ri_rows && (ri->ri_flg & RI_FULLCLEAR) != 0) {
		np = ri->ri_stride >> 5;
		nw = (ri->ri_stride >> 2) & 7;
		num = ri->ri_height;
		dp = (int32_t *)ri->ri_origbits;
		delta = 0;
	} else {
		np = ri->ri_emustride >> 5;
		nw = (ri->ri_emustride >> 2) & 7;
		num *= ri->ri_font->fontheight;
		dp = (int32_t *)(ri->ri_bits + row * ri->ri_yscale);
		delta = ri->ri_delta;
	}

	while (num--) {
		for (cnt = np; cnt; cnt--) {
			dp[0] = clr;
			dp[1] = clr;
			dp[2] = clr;
			dp[3] = clr;
			dp[4] = clr;
			dp[5] = clr;
			dp[6] = clr;
			dp[7] = clr;
			dp += 8;
		}

		for (cnt = nw; cnt; cnt--) {
			*(int32_t *)dp = clr;
			DELTA(dp, 4, int32_t *);
		}

		DELTA(dp, delta, int32_t *);
	}

	return 0;
}

/*
 * Actually turn the cursor on or off. This does the dirty work for
 * rasops_cursor().
 */
int
rasops_do_cursor(ri)
	struct rasops_info *ri;
{
	int full1, height, cnt, slop1, slop2, row, col;
	u_char *dp, *rp;

#if NRASOPS_ROTATION > 0
	if (ri->ri_flg & RI_ROTATE_CW) {
		/* Rotate rows/columns */
		row = ri->ri_ccol;
		col = ri->ri_rows - ri->ri_crow - 1;
	} else
#endif
	{
		row = ri->ri_crow;
		col = ri->ri_ccol;
	}

	rp = ri->ri_bits + row * ri->ri_yscale + col * ri->ri_xscale;
	height = ri->ri_font->fontheight;
	slop1 = (4 - ((long)rp & 3)) & 3;

	if (slop1 > ri->ri_xscale)
		slop1 = ri->ri_xscale;

	slop2 = (ri->ri_xscale - slop1) & 3;
	full1 = (ri->ri_xscale - slop1 - slop2) >> 2;

	if ((slop1 | slop2) == 0) {
		/* A common case */
		while (height--) {
			dp = rp;
			rp += ri->ri_stride;

			for (cnt = full1; cnt; cnt--) {
				*(int32_t *)dp ^= ~0;
				dp += 4;
			}
		}
	} else {
		/* XXX this is stupid.. use masks instead */
		while (height--) {
			dp = rp;
			rp += ri->ri_stride;

			if (slop1 & 1)
				*dp++ ^= ~0;

			if (slop1 & 2) {
				*(int16_t *)dp ^= ~0;
				dp += 2;
			}

			for (cnt = full1; cnt; cnt--) {
				*(int32_t *)dp ^= ~0;
				dp += 4;
			}

			if (slop2 & 1)
				*dp++ ^= ~0;

			if (slop2 & 2)
				*(int16_t *)dp ^= ~0;
		}
	}

	return 0;
}

/*
 * Erase columns.
 */
int
rasops_erasecols(cookie, row, col, num, attr)
	void *cookie;
	int row, col, num;
	long attr;
{
	int n8, height, cnt, slop1, slop2, clr;
	struct rasops_info *ri;
	int32_t *rp, *dp;

	ri = (struct rasops_info *)cookie;

#ifdef RASOPS_CLIPPING
	if ((unsigned)row >= (unsigned)ri->ri_rows)
		return 0;

	if (col < 0) {
		num += col;
		col = 0;
	}

	if ((col + num) > ri->ri_cols)
		num = ri->ri_cols - col;

	if (num <= 0)
		return 0;
#endif

	num = num * ri->ri_xscale;
	rp = (int32_t *)(ri->ri_bits + row*ri->ri_yscale + col*ri->ri_xscale);
	height = ri->ri_font->fontheight;
	clr = ri->ri_devcmap[(attr >> 16) & 0xf];

	/* Don't bother using the full loop for <= 32 pels */
	if (num <= 32) {
		if (((num | ri->ri_xscale) & 3) == 0) {
			/* Word aligned blt */
			num >>= 2;

			while (height--) {
				dp = rp;
				DELTA(rp, ri->ri_stride, int32_t *);

				for (cnt = num; cnt; cnt--)
					*dp++ = clr;
			}
		} else if (((num | ri->ri_xscale) & 1) == 0) {
			/*
			 * Halfword aligned blt. This is needed so the
			 * 15/16 bit ops can use this function.
			 */
			num >>= 1;

			while (height--) {
				dp = rp;
				DELTA(rp, ri->ri_stride, int32_t *);

				for (cnt = num; cnt; cnt--) {
					*(int16_t *)dp = clr;
					DELTA(dp, 2, int32_t *);
				}
			}
		} else {
			while (height--) {
				dp = rp;
				DELTA(rp, ri->ri_stride, int32_t *);

				for (cnt = num; cnt; cnt--) {
					*(u_char *)dp = clr;
					DELTA(dp, 1, int32_t *);
				}
			}
		}

		return 0;
	}

	slop1 = (4 - ((long)rp & 3)) & 3;
	slop2 = (num - slop1) & 3;
	num -= slop1 + slop2;
	n8 = num >> 5;
	num = (num >> 2) & 7;

	while (height--) {
		dp = rp;
		DELTA(rp, ri->ri_stride, int32_t *);

		/* Align span to 4 bytes */
		if (slop1 & 1) {
			*(u_char *)dp = clr;
			DELTA(dp, 1, int32_t *);
		}

		if (slop1 & 2) {
			*(int16_t *)dp = clr;
			DELTA(dp, 2, int32_t *);
		}

		/* Write 32 bytes per loop */
		for (cnt = n8; cnt; cnt--) {
			dp[0] = clr;
			dp[1] = clr;
			dp[2] = clr;
			dp[3] = clr;
			dp[4] = clr;
			dp[5] = clr;
			dp[6] = clr;
			dp[7] = clr;
			dp += 8;
		}

		/* Write 4 bytes per loop */
		for (cnt = num; cnt; cnt--)
			*dp++ = clr;

		/* Write unaligned trailing slop */
		if (slop2 & 1) {
			*(u_char *)dp = clr;
			DELTA(dp, 1, int32_t *);
		}

		if (slop2 & 2)
			*(int16_t *)dp = clr;
	}

	return 0;
}

#if NRASOPS_ROTATION > 0
/*
 * Quarter clockwise rotation routines (originally intended for the
 * built-in Zaurus C3x00 display in 16bpp).
 */

#include <sys/malloc.h>

void
rasops_rotate_font(int *cookie)
{
	struct rotatedfont *f;
	int ncookie;

	SLIST_FOREACH(f, &rotatedfonts, rf_next) {
		if (f->rf_cookie == *cookie) {
			*cookie = f->rf_rotated;
			return;
		}
	}

	/*
	 * We did not find a rotated version of this font. Ask the wsfont
	 * code to compute one for us.
	 */

	f = malloc(sizeof(struct rotatedfont), M_DEVBUF, M_WAITOK);

	if ((ncookie = wsfont_rotate(*cookie)) == -1)
		return;

	f->rf_cookie = *cookie;
	f->rf_rotated = ncookie;
	SLIST_INSERT_HEAD(&rotatedfonts, f, rf_next);

	*cookie = ncookie;
}

void
rasops_copychar(cookie, srcrow, dstrow, srccol, dstcol)
	void *cookie;
	int srcrow, dstrow, srccol, dstcol;
{
	struct rasops_info *ri;
	u_char *sp, *dp;
	int height;
	int r_srcrow, r_dstrow, r_srccol, r_dstcol;

	ri = (struct rasops_info *)cookie;

	r_srcrow = srccol;
	r_dstrow = dstcol;
	r_srccol = ri->ri_rows - srcrow - 1;
	r_dstcol = ri->ri_rows - dstrow - 1;

	r_srcrow *= ri->ri_yscale;
	r_dstrow *= ri->ri_yscale;
	height = ri->ri_font->fontheight;

	sp = ri->ri_bits + r_srcrow + r_srccol * ri->ri_xscale;
	dp = ri->ri_bits + r_dstrow + r_dstcol * ri->ri_xscale;

#if NRASOPS_BSWAP > 0
	if (ri->ri_flg & RI_BSWAP) {
		while (height--) {
			slow_ovbcopy(sp, dp, ri->ri_xscale);
			dp += ri->ri_stride;
			sp += ri->ri_stride;
		}
	} else
#endif
	{
		while (height--) {
			ovbcopy(sp, dp, ri->ri_xscale);
			dp += ri->ri_stride;
			sp += ri->ri_stride;
		}
	}
}

int
rasops_putchar_rotated(cookie, row, col, uc, attr)
	void *cookie;
	int row, col;
	u_int uc;
	long attr;
{
	struct rasops_info *ri;
	u_char *rp;
	int height;
	int rc;

	ri = (struct rasops_info *)cookie;

	/* Do rotated char sans (side)underline */
	rc = ri->ri_real_ops.putchar(cookie, col, ri->ri_rows - row - 1, uc,
	    attr & ~1);
	if (rc != 0)
		return rc;

	/* Do rotated underline */
	rp = ri->ri_bits + col * ri->ri_yscale + (ri->ri_rows - row - 1) * 
	    ri->ri_xscale;
	height = ri->ri_font->fontheight;

	/* XXX this assumes 16-bit color depth */
	if ((attr & 1) != 0) {
		int16_t c = (int16_t)ri->ri_devcmap[((u_int)attr >> 24) & 0xf];

		while (height--) {
			*(int16_t *)rp = c;
			rp += ri->ri_stride;
		}
	}

	return 0;
}

int
rasops_erasecols_rotated(cookie, row, col, num, attr)
	void *cookie;
	int row, col, num;
	long attr;
{
	struct rasops_info *ri;
	int i;
	int rc;

	ri = (struct rasops_info *)cookie;

	for (i = col; i < col + num; i++) {
		rc = ri->ri_ops.putchar(cookie, row, i, ' ', attr);
		if (rc != 0)
			return rc;
	}

	return 0;
}

/* XXX: these could likely be optimised somewhat. */
int
rasops_copyrows_rotated(cookie, src, dst, num)
	void *cookie;
	int src, dst, num;
{
	struct rasops_info *ri = (struct rasops_info *)cookie;
	int col, roff;

	if (src > dst) {
		for (roff = 0; roff < num; roff++)
			for (col = 0; col < ri->ri_cols; col++)
				rasops_copychar(cookie, src + roff, dst + roff,
				    col, col);
	} else {
		for (roff = num - 1; roff >= 0; roff--)
			for (col = 0; col < ri->ri_cols; col++)
				rasops_copychar(cookie, src + roff, dst + roff,
				    col, col);
	}

	return 0;
}

int
rasops_copycols_rotated(cookie, row, src, dst, num)
	void *cookie;
	int row, src, dst, num;
{
	int coff;

	if (src > dst) {
		for (coff = 0; coff < num; coff++)
			rasops_copychar(cookie, row, row, src + coff,
			    dst + coff);
	} else {
		for (coff = num - 1; coff >= 0; coff--)
			rasops_copychar(cookie, row, row, src + coff,
			    dst + coff);
	}

	return 0;
}

int
rasops_eraserows_rotated(cookie, row, num, attr)
	void *cookie;
	int row, num;
	long attr;
{
	struct rasops_info *ri;
	int col, rn;
	int rc;

	ri = (struct rasops_info *)cookie;

	for (rn = row; rn < row + num; rn++)
		for (col = 0; col < ri->ri_cols; col++) {
			rc = ri->ri_ops.putchar(cookie, rn, col, ' ', attr);
			if (rc != 0)
				return rc;
		}

	return 0;
}
#endif	/* NRASOPS_ROTATION */

#if NRASOPS_BSWAP > 0
/*
 * Strictly byte-only ovbcopy() version, to be used with RI_BSWAP, as the
 * regular ovbcopy() may want to optimize things by doing larger-than-byte
 * reads or write. This may confuse things if src and dst have different
 * alignments.
 */
void
slow_ovbcopy(void *s, void *d, size_t len)
{
	u_int8_t *src = s;
	u_int8_t *dst = d;

	if ((vaddr_t)dst <= (vaddr_t)src) {
		while (len-- != 0)
			*dst++ = *src++;
	} else {
		src += len;
		dst += len;
		if (len != 0)
			while (--len != 0)
				*--dst = *--src;
	}
}
#endif	/* NRASOPS_BSWAP */
