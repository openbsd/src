/*	$OpenBSD: rboxreg.h,v 1.2 2005/01/24 21:36:39 miod Exp $	*/
/*	$NetBSD: grf_rbreg.h,v 1.4 1994/10/26 07:24:03 cgd Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 * from: Utah $Hdr: grf_rbreg.h 1.9 92/01/21$
 *
 *	@(#)grf_rbreg.h	8.1 (Berkeley) 6/10/93
 */

/*
 * Map of the Renaissance frame buffer controller chip in memory ...
 */

#define rb_waitbusy(regaddr)						\
do {									\
	while (((volatile struct rboxfb *)(regaddr))->wbusy & 0x01)	\
		DELAY(10);						\
} while (0)

#define	RBOX_DUALROP(rop)	((rop) << 4 | (rop))

#define	CM1RED(fb)	((volatile struct rencm  *)((fb)->regkva + 0x6400))
#define	CM1GRN(fb)	((volatile struct rencm  *)((fb)->regkva + 0x6800))
#define	CM1BLU(fb)	((volatile struct rencm  *)((fb)->regkva + 0x6C00))
#define	CM2RED(fb)	((volatile struct rencm  *)((fb)->regkva + 0x7400))
#define	CM2GRN(fb)	((volatile struct rencm  *)((fb)->regkva + 0x7800))
#define	CM2BLU(fb)	((volatile struct rencm  *)((fb)->regkva + 0x7C00))

struct rencm {
	u_int8_t  :8, :8, :8;
	u_int8_t value;
};

struct rboxfb {
	struct diofbreg regs;
	u_int8_t filler2[16359];
	u_int8_t wbusy;			/* window mover is active     0x4047 */
	u_int8_t filler3[0x405b - 0x4048];
	u_int8_t scanbusy;		/* scan converteris active    0x405B */
	u_int8_t filler3b[0x4083 - 0x405c];
	u_int8_t video_enable;   	/* drive vid. refresh bus     0x4083 */
	u_int8_t filler4[3];
	u_int8_t display_enable;	/* enable the display	      0x4087 */
	u_int8_t filler5[8];
	u_int32_t write_enable;		/* write enable register      0x4090 */
	u_int8_t filler6[11];
	u_int8_t wmove;			/* start window mover	      0x409f */
	u_int8_t filler7[3];
	u_int8_t blink;			/* blink register	      0x40a3 */
	u_int8_t filler8[15];
	u_int8_t fold;			/* fold  register	      0x40b3 */
	u_int32_t opwen;		/* overlay plane write enable 0x40b4 */
	u_int8_t filler9[3];
	u_int8_t tmode;			/* Tile mode size	      0x40bb */
	u_int8_t filler9a[3];
	u_int8_t drive;			/* drive register	      0x40bf */
	u_int8_t filler10[3];
	u_int8_t vdrive;		/* vdrive register	      0x40c3 */
	u_int8_t filler10a[0x40cb-0x40c4];
	u_int8_t zconfig;		/* Z-buffer mode	      0x40cb */
	u_int8_t filler11a[2];
	u_int16_t tpatt;		/* Transparency pattern	      0x40ce */
	u_int8_t filler11b[3];
	u_int8_t dmode;			/* dither mode		      0x40d3 */
	u_int8_t filler11c[3];
	u_int8_t en_scan;		/* enable scan board to DTACK 0x40d7 */
	u_int8_t filler11d[0x40ef-0x40d8];
	u_int8_t rep_rule;		/* replacement rule	      0x40ef */
	u_int8_t filler12[2];
	u_int16_t source_x;		/* source x		      0x40f2 */
	u_int8_t filler13[2];
	u_int16_t source_y;		/* source y		      0x40f6 */
	u_int8_t filler14[2];
	u_int16_t dest_x;		/* dest x		      0x40fa */
	u_int8_t filler15[2];
	u_int16_t dest_y;		/* dest y		      0x40fe */
	u_int8_t filler16[2];
	u_int16_t wwidth;		/* window width		      0x4102 */
	u_int8_t filler17[2];
	u_int16_t wheight;		/* window height	      0x4106 */
	u_int8_t filler18[18];
	u_int16_t patt_x;		/* pattern x		      0x411a */
	u_int8_t filler19[2];
	u_int16_t patt_y;		/* pattern y		      0x411e */
	u_int8_t filler20[0x8012 - 0x4120];
	u_int16_t te_status;		/* transform engine status    0x8012 */
	u_int8_t filler21[0x1ffff-0x8014];
};
