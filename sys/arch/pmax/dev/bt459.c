/*	$NetBSD: bt459.c,v 1.5 1996/10/13 13:13:50 jonathan Exp $	*/

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell and Rick Macklem.
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
 *	from: @(#)sfb.c	8.1 (Berkeley) 6/10/93
 */

/*
 *  devGraphics.c --
 *
 *     	This file contains machine-dependent routines for the graphics device.
 *
 *	Copyright (C) 1989 Digital Equipment Corporation.
 *	Permission to use, copy, modify, and distribute this software and
 *	its documentation for any purpose and without fee is hereby granted,
 *	provided that the above copyright notice appears in all copies.  
 *	Digital Equipment Corporation makes no representations about the
 *	suitability of this software for any purpose.  It is provided "as is"
 *	without express or implied warranty.
 *
 * from: Header: /sprite/src/kernel/dev/ds3100.md/RCS/devGraphics.c,
 *	v 9.2 90/02/13 22:16:24 shirriff Exp  SPRITE (DECWRL)";
 */
/*
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/select.h>

#include <machine/pmioctl.h>

#include <machine/fbio.h>
#include <machine/fbvar.h>

#include <pmax/dev/fbreg.h>

#include <pmax/dev/bt459.h>

/*
 * Forward references.
 */

static void bt459_set_cursor_ram (struct fbinfo *, int, u_char);
static void bt459_select_reg (bt459_regmap_t *, int);
static void bt459_write_reg (bt459_regmap_t *, int, int);
static u_char bt459_read_reg (bt459_regmap_t *, int);

/*
 * Initialization
 */
int
bt459init(fi)
	struct fbinfo *fi;
{
	register bt459_regmap_t *regs = (bt459_regmap_t *)(fi -> fi_vdac);
	u_char foo;

	foo = bt459_read_reg (regs, BT459_REG_ID);
	if (bt459_read_reg(regs, BT459_REG_ID) != 0x4a)
		return (0);

#ifdef FOO /* @@@ DON'T KNOW HOW TO DO THIS @@@ */
	/* Reset the chip */
	*(volatile int *)(fi->fi_base + SFB_OFFSET_RESET) = 0;
	DELAY(2000);	/* ???? check right time on specs! ???? */
#endif /* FOO */

	/* use 4:1 input mux */
	bt459_write_reg(regs, BT459_REG_CMD0, 0x40);

	/* no zooming, no panning */
	bt459_write_reg(regs, BT459_REG_CMD1, 0x00);

	/*
	 * signature test, X-windows cursor, no overlays, SYNC* PLL,
	 * normal RAM select, 7.5 IRE pedestal, do sync
	 */
/*XXX*/ /* FIXME */
#if 0
#ifndef PMAX
	/* whats this for then ?? */
	bt459_write_reg(regs, BT459_REG_CMD2, 0xc2);
#else /* PMAX */
	bt459_write_reg(regs, BT459_REG_CMD2, 0xc0);
#endif /* PMAX */
#else
	bt459_write_reg(regs, BT459_REG_CMD2, 0xc0);
#endif
	/* get all pixel bits */
	bt459_write_reg(regs, BT459_REG_PRM, 0xff);

	/* no blinking */
	bt459_write_reg(regs, BT459_REG_PBM, 0x00);

	/* no overlay */
	bt459_write_reg(regs, BT459_REG_ORM, 0x00);

	/* no overlay blink */
	bt459_write_reg(regs, BT459_REG_OBM, 0x00);

	/* no interleave, no underlay */
	bt459_write_reg(regs, BT459_REG_ILV, 0x00);

	/* normal operation, no signature analysis */
	bt459_write_reg(regs, BT459_REG_TEST, 0x00);

	/*
	 * no blinking, 1bit cross hair, XOR reg&crosshair,
	 * no crosshair on either plane 0 or 1,
	 * regular cursor on both planes.
	 */
	bt459_write_reg(regs, BT459_REG_CCR, 0xc0);

	/* home cursor */
	bt459_write_reg(regs, BT459_REG_CXLO, 0x00);
	bt459_write_reg(regs, BT459_REG_CXHI, 0x00);
	bt459_write_reg(regs, BT459_REG_CYLO, 0x00);
	bt459_write_reg(regs, BT459_REG_CYHI, 0x00);

	/* no crosshair window */
	bt459_write_reg(regs, BT459_REG_WXLO, 0x00);
	bt459_write_reg(regs, BT459_REG_WXHI, 0x00);
	bt459_write_reg(regs, BT459_REG_WYLO, 0x00);
	bt459_write_reg(regs, BT459_REG_WYHI, 0x00);
	bt459_write_reg(regs, BT459_REG_WWLO, 0x00);
	bt459_write_reg(regs, BT459_REG_WWHI, 0x00);
	bt459_write_reg(regs, BT459_REG_WHLO, 0x00);
	bt459_write_reg(regs, BT459_REG_WHHI, 0x00);

	/* Initialize the cursor position... */
	fi -> fi_cursor.width = 64;
	fi -> fi_cursor.height = 64;
	fi -> fi_cursor.x = 0;
	fi -> fi_cursor.y = 0;

	/*
	 * Initialize the color map and the screen.
	 */
	bt459InitColorMap(fi);
	return (1);
}

static u_char	cursor_RGB[6];	/* cursor color 2 & 3 */

/*
 * XXX This assumes 2bits/cursor pixel so that the 1Kbyte cursor RAM
 * defines a 64x64 cursor. If the bt459 does not map the cursor RAM
 * this way, this code is Screwed!
 */
void
bt459LoadCursor(fi, cursor)
	struct fbinfo *fi;
	u_short *cursor;
{
	register int i, j, k, pos;
	register u_short ap, bp, out;

	/*
	 * Fill in the cursor sprite using the A and B planes, as provided
	 * for the pmax.
	 * XXX This will have to change when the X server knows that this
	 * is not a pmax display.
	 */
	pos = 0;
	for (k = 0; k < 16; k++) {
		ap = *cursor;
		bp = *(cursor + 16);
		j = 0;
		while (j < 4) {
			out = 0;
			for (i = 0; i < 4; i++) {
#ifndef CURSOR_EB
				out = (out << 2) | ((ap & 0x1) << 1) |
					(bp & 0x1);
#else
				out = ((out >> 2) & 0x3f) |
					((ap & 0x1) << 7) |
					((bp & 0x1) << 6);
#endif
				ap >>= 1;
				bp >>= 1;
			}
			bt459_set_cursor_ram(fi, pos, out);
			pos++;
			j++;
		}
		while (j < 16) {
			bt459_set_cursor_ram(fi, pos, 0);
			pos++;
			j++;
		}
		cursor++;
	}
	while (pos < 1024) {
		bt459_set_cursor_ram(fi, pos, 0);
		pos++;
	}
}

/*
 * Set a cursor ram value.
 */
static void
bt459_set_cursor_ram(fi, pos, val)
	int pos;
	register u_char val;
	struct fbinfo *fi;
{
	register bt459_regmap_t *regs;
	register int cnt;
	u_char nval;
	regs = (bt459_regmap_t *)(fi -> fi_vdac);

	cnt = 0;
	do {
		bt459_write_reg(regs, BT459_REG_CRAM_BASE + pos, val);
		nval = bt459_read_reg(regs, BT459_REG_CRAM_BASE + pos);
	} while (val != nval && ++cnt < 10);
}

/* Set the cursor color from the saved state. */

void
bt459RestoreCursorColor(fi)
	struct fbinfo *fi;
{
	bt459_regmap_t *regs;
	register int i;

	regs = (bt459_regmap_t *)(fi -> fi_vdac);

	bt459_select_reg(regs, BT459_REG_CCOLOR_1);
	for (i = 0; i < 3; i++) {
		regs->addr_reg = cursor_RGB[i];
		wbflush();
	}
	bt459_select_reg(regs, BT459_REG_CCOLOR_3);
	for (i = 3; i < 6; i++) {
		regs->addr_reg = cursor_RGB[i];
		wbflush();
	}
}

/* Set the cursor color...  */

void
bt459CursorColor(fi, color)
	unsigned int color[];
	struct fbinfo *fi;
{
	register int i;

	for (i = 0; i < 6; i++)
		cursor_RGB[i] = (u_char)(color[i] >> 8);

	bt459RestoreCursorColor(fi);
}

/* Move the hardware cursor to the specified position. */

void
bt459PosCursor(fi, x, y)
	struct fbinfo *fi;
	register int x, y;
{
	bt459_regmap_t *regs;
	struct fbuaccess *fbu = fi->fi_fbu;
	regs = (bt459_regmap_t *)(fi -> fi_vdac);

#ifdef MELLON
	if (y < 0)
	  y = 0;
	else if (y > fi -> fi_type.fb_width - fi -> fi_cursor.width - 1)
	  y = fi -> fi_type.fb_width - fi -> fi_cursor.width - 1;
	if (x < 0)
	  x = 0;
	else if (x > fi -> fi_type.fb_height - fi -> fi_cursor.height - 1)
	  x = fi -> fi_type.fb_height - fi -> fi_cursor.height - 1;
#else /* old-style pmax glass tty */

 	if (y < fbu->scrInfo.min_cur_y || y > fbu->scrInfo.max_cur_y)
		y = fbu->scrInfo.max_cur_y;
	if (x < fbu->scrInfo.min_cur_x || x > fbu->scrInfo.max_cur_x)
		x = fbu->scrInfo.max_cur_x;
#endif


	fi -> fi_cursor.x = x;
	fi -> fi_cursor.y = y;

	fbu->scrInfo.cursor.x = x;		/* keep track of real cursor */
	fbu->scrInfo.cursor.y = y;		/* position, indep. of mouse */

	/* XXX is this a linear function of x-dimension screen size? */
	if (fi->fi_type.fb_boardtype == PMAX_FBTYPE_SFB)
		x += 369;	/* is this correct for rcons on an sfb?? */
	else
		x += 219;	/* correct for a cfb */
	y += 34;

	
	bt459_select_reg(regs, BT459_REG_CXLO);
	regs->addr_reg = x;
	wbflush();
	regs->addr_reg = x >> 8;
	wbflush();
	regs->addr_reg = y;
	wbflush();
	regs->addr_reg = y >> 8;
	wbflush();
}

/* Initialize the colormap to the default state, which is that entry
   zero is black and all other entries are full white. */

void
bt459InitColorMap(fi)
	struct fbinfo *fi;
{
	bt459_regmap_t *regs;
	register int i;
	regs = (bt459_regmap_t *)(fi -> fi_vdac);

	bt459_select_reg(regs, 0);
	((u_char *)(fi -> fi_cmap_bits)) [0] = regs->addr_cmap = 0;
	wbflush();
	((u_char *)(fi -> fi_cmap_bits)) [1] = regs->addr_cmap = 0;
	wbflush();
	((u_char *)(fi -> fi_cmap_bits)) [2] = regs->addr_cmap = 0;
	wbflush();

	for (i = 0; i < 256; i++) {
		((u_char *)(fi -> fi_cmap_bits)) [i * 3]
			= regs->addr_cmap = 0xff;
		wbflush();
		((u_char *)(fi -> fi_cmap_bits)) [i * 3 + 1]
			= regs->addr_cmap = 0xff;
		wbflush();
		((u_char *)(fi -> fi_cmap_bits)) [i * 3 + 2]
			= regs -> addr_cmap = 0xff;
		wbflush();
	}

	for (i = 0; i < 3; i++) {
		cursor_RGB[i] = 0x00;
		cursor_RGB[i + 3] = 0xff;
	}
	bt459RestoreCursorColor(fi);
}

/* Load count entries of the colormap starting at index with the values
   pointed to by bits. */

int
bt459LoadColorMap(fi, bits, index, count)
	struct fbinfo *fi;
	caddr_t bits;
	int index, count;
{
	bt459_regmap_t *regs;
	u_char *cmap_bits;
	u_char *cmap;
	int i;

	if (index > 256 || index < 0 || index + count > 256)
		return EINVAL;

	regs = (bt459_regmap_t *)(fi -> fi_vdac);
	cmap_bits = (u_char *)bits;
	cmap = (u_char *)(fi -> fi_cmap_bits) + index * 3;

	bt459_select_reg(regs, index);

	for (i = 0; i < count; i++) {
		cmap [(i + index) * 3]
			= regs -> addr_cmap = cmap_bits [i * 3];
		cmap [(i + index) * 3 + 1]
			= regs -> addr_cmap = cmap_bits [i * 3 + 1];
		cmap [(i + index) * 3 + 2]
			= regs -> addr_cmap = cmap_bits [i * 3 + 2];
	}
	return 0;
}

/* Copy out count entries of the colormap starting at index into bits. */

int
bt459GetColorMap(fi, bits, index, count)
	struct fbinfo *fi;
	caddr_t bits;
	int index, count;
{
	u_char *cmap_bits;
	u_char *cmap;

	if (index > 256 || index < 0 || index + count > 256)
		return EINVAL;

	cmap_bits = (u_char *)bits;
	cmap = (u_char *)(fi -> fi_cmap_bits) + index * 3;

	bcopy (cmap, cmap_bits, count * 3);
	return 0;
}

/* Enable the video display. */

int
bt459_video_on(fi)
     struct fbinfo *fi;
{
	bt459_regmap_t *regs;
	u_char *cmap_bits;

	if (!fi -> fi_blanked)
		return 0;

	/* XXX The cfb driver did this in the ioctl handler, but
	   Ted Lemon's sfb drifer didn't? */
	bt459RestoreCursorColor(fi);

	regs = (bt459_regmap_t *)(fi -> fi_vdac);
	cmap_bits = (u_char *)fi -> fi_cmap_bits;

	/* restore old color map entry zero */
	bt459_select_reg(regs, 0);
	regs->addr_cmap = cmap_bits [0];
	wbflush();
	regs->addr_cmap = cmap_bits [0];
	wbflush();
	regs->addr_cmap = cmap_bits [0];
	wbflush();

	/* enable normal display */
	bt459_write_reg(regs, BT459_REG_PRM, 0xff);
	bt459_write_reg(regs, BT459_REG_CCR, 0xc0);

	fi -> fi_blanked = 0;
	return 0;
}

/* Disable the video display. */

int
bt459_video_off(fi)
	struct fbinfo *fi;
{
	bt459_regmap_t *regs;
	regs = (bt459_regmap_t *)(fi -> fi_vdac);

	if (fi -> fi_blanked)
		return 0;

	/* set color map entry zero to zero */
	bt459_select_reg(regs, 0);
	regs->addr_cmap = 0;
	wbflush();
	regs->addr_cmap = 0;
	wbflush();
	regs->addr_cmap = 0;
	wbflush();

	/* disable display */
	bt459_write_reg(regs, BT459_REG_PRM, 0);
	bt459_write_reg(regs, BT459_REG_CCR, 0);

	fi -> fi_blanked = 1;
	return 0;
}

/*
 * Generic register access
 */
static void
bt459_select_reg(regs, regno)
	bt459_regmap_t *regs;
{
	regs->addr_lo = regno;
	regs->addr_hi = regno >> 8;
	wbflush();
}

static void
bt459_write_reg(regs, regno, val)
	bt459_regmap_t *regs;
{
	regs->addr_lo = regno;
	regs->addr_hi = regno >> 8;
	wbflush();
	regs->addr_reg = val;
	wbflush();
}

static u_char
bt459_read_reg(regs, regno)
	bt459_regmap_t *regs;
{
	regs->addr_lo = regno;
	regs->addr_hi = regno >> 8;
	wbflush();
	return (regs->addr_reg);
}

