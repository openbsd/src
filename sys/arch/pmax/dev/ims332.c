/*	$NetBSD: ims332.c,v 1.4 1996/10/13 13:13:57 jonathan Exp $	*/

/*-
 * Copyright (c) 1992, 1993, 1995
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)ims332.c	8.1 (Berkeley) 6/10/93
 */
/*
 *	Routines for the Inmos IMS-G332 Colour video controller
 * 	Author: Alessandro Forin, Carnegie Mellon University
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/errno.h>

#include <machine/fbio.h>
#include <machine/fbvar.h>

#include <pmax/dev/ims332.h>

static u_int ims332_read_register (struct fbinfo *, int);
static void ims332_write_register (struct fbinfo *, int, unsigned int);

#define	assert_ims332_reset_bit(r)	*r &= ~0x40
#define	deassert_ims332_reset_bit(r)	*r |=  0x40

int
ims332init(fi)
	struct fbinfo *fi;
{
	int i;

	/*
	 * Initialize the screen. (xcfb-specific)
	 */
#ifdef notdef
	register u_int *reset = (u_int *)fi -> fi_base;


	assert_ims332_reset_bit(reset);
	DELAY(1);	/* specs sez 50ns.. */
	deassert_ims332_reset_bit(reset);

	/* CLOCKIN appears to receive a 6.25 Mhz clock --> PLL 12 for
           75Mhz monitor */
	ims332_write_register (fi, IMS332_REG_BOOT,
			       12 | IMS332_BOOT_CLOCK_PLL);

	/* initialize VTG */
	ims332_write_register (fi, IMS332_REG_CSR_A,
			       IMS332_BPP_8 | IMS332_CSR_A_DISABLE_CURSOR);
	DELAY(50);	/* spec does not say */

	/* datapath registers (values taken from prom's settings) */

	ims332_write_register (fi, IMS332_REG_HALF_SYNCH, 0x10);
	ims332_write_register (fi, IMS332_REG_BACK_PORCH, 0x21);
	ims332_write_register (fi, IMS332_REG_DISPLAY, 0x100);
	ims332_write_register (fi, IMS332_REG_SHORT_DIS, 0x5d);
	ims332_write_register (fi, IMS332_REG_BROAD_PULSE, 0x9f);
	ims332_write_register (fi, IMS332_REG_V_SYNC, 0xc);
	ims332_write_register (fi, IMS332_REG_V_PRE_EQUALIZE, 2);
	ims332_write_register (fi, IMS332_REG_V_POST_EQUALIZE, 2);
	ims332_write_register (fi, IMS332_REG_V_BLANK, 0x2a);
	ims332_write_register (fi, IMS332_REG_V_DISPLAY, 0x600);
	ims332_write_register (fi, IMS332_REG_LINE_TIME, 0x146);
	ims332_write_register (fi, IMS332_REG_LINE_START, 0x10);
	ims332_write_register (fi, IMS332_REG_MEM_INIT, 0xa);
	ims332_write_register (fi, IMS332_REG_XFER_DELAY, 0xa);

	ims332_write_register (fi, IMS332_REG_COLOR_MASK, 0xffffff);
#endif	/* notdef */

	/* Zero out the cursor RAM... */
	for (i = 0; i < 512; i++)
		ims332_write_register (fi, IMS332_REG_CURSOR_RAM + i, 0);

	/* Set up the color map... */
	ims332InitColorMap (fi);

	/* Enable display... */
	ims332_write_register (fi, IMS332_REG_CSR_A,
			       IMS332_BPP_8
			       | IMS332_CSR_A_DMA_DISABLE
			       | IMS332_CSR_A_VTG_ENABLE);

	return (1);
}

static u_char	cursor_RGB[6];	/* cursor color 2 & 3 */

static u_int
ims332_read_register(fi, regno)
	struct fbinfo *fi;
	int regno;
{
	register u_char *regs = (u_char *)fi -> fi_vdac;
	unsigned char *rptr;
	register u_int val, v1;

	/* spec sez: */
	rptr = regs + 0x80000 + (regno << 4);
	val = *((volatile u_short *) rptr );
	v1  = *((volatile u_short *) regs );

	return (val & 0xffff) | ((v1 & 0xff00) << 8);
}

static void
ims332_write_register(fi, regno, val)
	struct fbinfo *fi;
	int regno;
	register unsigned int val;
{
	register u_char *regs = (u_char *)fi -> fi_vdac;
	u_char *wptr;

	/* spec sez: */
	wptr = regs + 0xa0000 + (regno << 4);
	*((volatile u_int *)(regs)) = (val >> 8) & 0xff00;
	*((volatile u_short *)(wptr)) = val;
}

void
ims332InitColorMap(fi)
	struct fbinfo *fi;
{
	u_char *cmap;
	int i;

	cmap = (u_char *)(fi -> fi_cmap_bits);

	ims332_write_register (fi, IMS332_REG_LUT_BASE, 0);
	cmap [0] = cmap [1] = cmap [2] = 0;

	for (i = 1; i < 256; i++) {
		ims332_write_register (fi, IMS332_REG_LUT_BASE + i, 0xffffff);
		cmap [i * 3] = cmap [i * 3 + 1] = cmap [i * 3 + 2] = 0xff;
	}

	for (i = 0; i < 3; i++) {
		cursor_RGB[i] = 0x00;
		cursor_RGB[i + 3] = 0xff;
	}
	ims332RestoreCursorColor (fi);
}

/* Load color map entry(s). */

int
ims332LoadColorMap(fi, bits, index, count)
	struct fbinfo *fi;
	caddr_t bits;
	int index, count;
{
	u_char *cmap_bits;
	u_char *cmap;
	int i;

	if (index > 256 || index < 0 || index + count > 256)
		return EINVAL;

	cmap_bits = (u_char *)bits;
	cmap = (u_char *)(fi -> fi_cmap_bits) + index * 3;

	for (i = 0; i < count; i++) {
		ims332_write_register (fi,
				       IMS332_REG_LUT_BASE + i + index,
				       (cmap_bits [i * 3 + 2] << 16) |
				       (cmap_bits [i * 3 + 1] << 8) |
				       (cmap_bits [i * 3]));
		cmap [i * 3] = cmap_bits [i * 3];
		cmap [i * 3 + 1] = cmap_bits [i * 3 + 1];
		cmap [i * 3 + 2] = cmap_bits [i * 3 + 2];
	}
	return 0;
}

/* Get color map entry(s). */

int
ims332GetColorMap(fi, bits, index, count)
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

/*
 * Video on/off
 *
 * It is unfortunate that X11 goes backward with white@0
 * and black@1.  So we must stash away the zero-th entry
 * and fix it while screen is off.
 */

int
ims332_video_off(fi)
	struct fbinfo *fi;
{
	u_int csr;
	u_char *cmap_bits;

	if (fi -> fi_blanked)
		return 0;

	cmap_bits = (u_char *)fi -> fi_cmap_bits;

	ims332_write_register (fi, IMS332_REG_LUT_BASE, 0);

	ims332_write_register (fi, IMS332_REG_COLOR_MASK, 0);

	/* cursor now */
	csr = ims332_read_register (fi, IMS332_REG_CSR_A);
	csr |= IMS332_CSR_A_DISABLE_CURSOR;
	ims332_write_register (fi, IMS332_REG_CSR_A, csr);

	fi -> fi_blanked = 1;
	return 0;
}

int
ims332_video_on (fi)
	struct fbinfo *fi;
{
	u_char *cmap;
	u_int csr;

	if (!fi -> fi_blanked)
		return 0;

	cmap = (u_char *)(fi -> fi_cmap_bits);

	ims332_write_register (fi, IMS332_REG_LUT_BASE,
			       ((unsigned)cmap [0] |
				((unsigned)cmap [1] << 8) |
				((unsigned)cmap [2] << 16)));
	
	ims332_write_register (fi, IMS332_REG_COLOR_MASK, 0xffffffff);

	/* cursor now */
	csr = ims332_read_register (fi, IMS332_REG_CSR_A);
	csr &= ~IMS332_CSR_A_DISABLE_CURSOR;
	ims332_write_register (fi, IMS332_REG_CSR_A, csr);

	fi -> fi_blanked = 0;
	return 0;
}

/*
 * Cursor
 */
void
ims332PosCursor(fi, x, y)
	struct fbinfo *fi;
	int x, y;
{
	if (x < 0)
	  x = 0;
	else if (x > fi -> fi_type.fb_width - fi -> fi_cursor.width - 1)
	  x = fi -> fi_type.fb_width - fi -> fi_cursor.width - 1;
	if (y < 0)
	  y = 0;
	else if (y > fi -> fi_type.fb_height - fi -> fi_cursor.height - 1)
	  y = fi -> fi_type.fb_height - fi -> fi_cursor.height - 1;

	fi -> fi_cursor.x = x;
	fi -> fi_cursor.y = y;

	ims332_write_register (fi, IMS332_REG_CURSOR_LOC,
			       ((x & 0xfff) << 12) | (y & 0xfff));
}

void
ims332RestoreCursorColor (fi)
	struct fbinfo *fi;
{

	/* Bg is color[0], Fg is color[1] */
	ims332_write_register (fi, IMS332_REG_CURSOR_LUT_0,
			       (cursor_RGB[2] << 16) |
			       (cursor_RGB[1] << 8) |
			       (cursor_RGB[0]));
	ims332_write_register (fi, IMS332_REG_CURSOR_LUT_1, 0x7f0000);
	ims332_write_register (fi, IMS332_REG_CURSOR_LUT_2,
			       (cursor_RGB[5] << 16) |
			       (cursor_RGB[4] << 8) |
			       (cursor_RGB[3]));
}

/* Set the color of the cursor. */

void
ims332CursorColor (fi, color)
	struct fbinfo *fi;
	unsigned int color[];
{
	register int i;

	for (i = 0; i < 6; i++)
		cursor_RGB[i] = (u_char)(color[i] >> 8);

	ims332RestoreCursorColor(fi);
}

void
ims332LoadCursor(fi, cursor)
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
		while (j < 2) {
			out = 0;
			for (i = 0; i < 8; i++) {
				out = ((out >> 2) & 0x3fff) |
					((ap & 0x1) << 15) |
					((bp & 0x1) << 14);
				ap >>= 1;
				bp >>= 1;
			}
			ims332_write_register (fi, IMS332_REG_CURSOR_RAM + pos,
					       out);
			pos++;
			j++;
		}
		while (j < 8) {
			ims332_write_register (fi, IMS332_REG_CURSOR_RAM + pos,
					       0);
			pos++;
			j++;
		}
		cursor++;
	}
	while (pos < 512) {
		ims332_write_register (fi, IMS332_REG_CURSOR_RAM + pos, 0);
		pos++;
	}
}

