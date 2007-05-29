/*	$OpenBSD: btreg.h,v 1.5 2007/05/29 09:54:05 sobrado Exp $	*/
/*	$NetBSD: btreg.h,v 1.4 1996/02/27 22:09:21 thorpej Exp $ */

/*
 * Copyright (c) 1993
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
 * 3. Neither the name of the University nor the names of its contributors
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
 *	@(#)btreg.h	8.2 (Berkeley) 1/21/94
 */

/*
 * Several Sun color frame buffers use some kind of Brooktree video
 * DAC (e.g., the Bt458, -- in any case, Brooktree make the only
 * decent color frame buffer chips).
 *
 * Color map control on these is a bit funky in a SPARCstation.
 * To update the color map one would normally do byte writes, but
 * the hardware takes longword writes.  Since there are three
 * registers for each color map entry (R, then G, then B), we have
 * to set color 1 with a write to address 0 (setting 0's R/G/B and
 * color 1's R) followed by a second write to address 1 (setting
 * color 1's G/B and color 2's R/G).  Software must therefore keep
 * a copy of the current map.
 *
 * The colormap address register increments automatically, so the
 * above write is done as:
 *
 *	bt->bt_addr = 0;
 *	bt->bt_cmap = R0G0B0R1;
 *	bt->bt_cmap = G1B1R2G2;
 *	...
 *
 * Yow!
 *
 * Bonus complication: on the cg6, only the top 8 bits of each 32 bit
 * register matter, even though the cg3 takes all the bits from all
 * bytes written to it.
 */
struct bt_regs {
	u_int	bt_addr;		/* map address register */
	u_int	bt_cmap;		/* colormap data register */
	u_int	bt_ctrl;		/* control register */
	u_int	bt_omap;		/* overlay (cursor) map register */
};
#define BT_INIT(bt, shift) do { /* whatever this means.. */ \
	(bt)->bt_addr = 0x06 << (shift);	/* command reg */ \
	(bt)->bt_ctrl = 0x73 << (shift);	/* overlay plane */ \
	(bt)->bt_addr = 0x04 << (shift);	/* read mask */ \
	(bt)->bt_ctrl = 0xff << (shift);	/* color planes */ \
} while(0)
#define BT_UNBLANK(bt, x, shift) do { \
	/* restore color 0 (and R of color 1) */ \
	(bt)->bt_addr = 0 << (shift); \
	(bt)->bt_cmap = (x); \
	if ((shift)) { \
		(bt)->bt_cmap = (x) << 8; \
		(bt)->bt_cmap = (x) << 16; \
	/* restore read mask */ \
	BT_INIT((bt), (shift)); \
} while(0)
#define BT_BLANK(bt, shift) do { \
	(bt)->bt_addr = 0x06 << (shift);	/* command reg */ \
	(bt)->bt_ctrl = 0x70 << (shift);	/* overlay plane */ \
	(bt)->bt_addr = 0x04 << (shift);	/* read mask */ \
	(bt)->bt_ctrl = 0x00 << (shift);	/* color planes */ \
	/* Set color 0 to black -- note that this overwrites R of color 1. */\
	(bt)->bt_addr = 0 << (shift); \
	(bt)->bt_cmap = 0 << (shift); \
	/* restore read mask */ \
	BT_INIT((bt), (shift)); \
} while(0)


/*
 * SBus framebuffer control look like this (usually at offset 0x400000).
 */
struct fbcontrol {
	struct	bt_regs fbc_dac;
	u_char	fbc_ctrl;
	u_char	fbc_status;
	u_char	fbc_cursor_start;
	u_char	fbc_cursor_end;
	u_char	fbc_vcontrol[12];	/* 12 bytes of video timing goo */
};
/* fbc_ctrl bits: */
#define FBC_IENAB	0x80		/* Interrupt enable */
#define FBC_VENAB	0x40		/* Video enable */
#define FBC_TIMING	0x20		/* Master timing enable */
#define FBC_CURSOR	0x10		/* Cursor compare enable */
#define FBC_XTALMSK	0x0c		/* Xtal select (0,1,2,test) */
#define FBC_DIVMSK	0x03		/* Divisor (1,2,3,4) */

/* fbc_status bits: */
#define FBS_INTR	0x80		/* Interrupt pending */
#define FBS_MSENSE	0x70		/* Monitor sense mask */
#define		FBS_1024X768	0x10
#define		FBS_1152X900	0x30
#define		FBS_1280X1024	0x40
#define		FBS_1600X1280	0x50
#define FBS_ID_MASK	0x0f		/* ID mask */
#define		FBS_ID_COLOR	0x01
#define		FBS_ID_MONO	0x02
#define		FBS_ID_MONO_ECL	0x03	/* ? */

