/*	$OpenBSD: topcatreg.h,v 1.1 2005/01/14 22:39:26 miod Exp $	*/
/*	$NetBSD: grf_tcreg.h,v 1.6 1994/10/26 07:24:06 cgd Exp $	*/

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
 * from: Utah $Hdr: grf_tcreg.h 1.11 92/01/21$
 *
 *	@(#)grf_tcreg.h	8.1 (Berkeley) 6/10/93
 */

#define tccm_waitbusy(regaddr) \
do { \
	while (((volatile struct tcboxfb *)(regaddr))->cmap_busy & 0x04) \
		DELAY(10); \
} while (0)

#define tc_waitbusy(regaddr,planes) \
do { \
	while (((volatile struct tcboxfb *)(regaddr))->busy & planes) \
		DELAY(10); \
} while (0)

struct tcboxfb {
	u_int8_t :8;
	u_int8_t reset;			/* reset register		0x01 */
	u_int8_t fb_address;		/* frame buffer address 	0x02 */
	u_int8_t interrupt;		/* interrupt register		0x03 */
	u_int8_t :8;
	u_int8_t fbwmsb;		/* frame buffer width MSB	0x05 */
	u_int8_t :8;
	u_int8_t fbwlsb;		/* frame buffer width MSB	0x07 */
	u_int8_t :8;
	u_int8_t fbhmsb;		/* frame buffer height MSB	0x09 */
	u_int8_t :8;
	u_int8_t fbhlsb;		/* frame buffer height MSB	0x0b */
	u_int8_t :8;
	u_int8_t dwmsb;			/* display width MSB		0x0d */
	u_int8_t :8;
	u_int8_t dwlsb;			/* display width MSB		0x0f */
	u_int8_t :8;
	u_int8_t dhmsb;			/* display height MSB		0x11 */
	u_int8_t :8;
	u_int8_t dhlsb;			/* display height MSB		0x13 */
	u_int8_t :8;
	u_int8_t fbid;			/* Scondary frame buffer id	0x15 */
	u_int8_t :8;
	u_int8_t bits;			/* square(0)/double-high(1) 	0x17 */
	u_int8_t f1[0x5b-0x17-1];
	u_int8_t num_planes;		/* number of color planes       0x5b */
	u_int8_t :8;
	u_int8_t fbomsb;		/* frame buffer offset MSB	0x5d */
	u_int8_t :8;
	u_int8_t fbolsb;		/* frame buffer offset LSB	0x5f */
	u_int8_t f2[0x4040-0x5f-1];
	u_int8_t vblank;		/* vertical blanking	      0x4040 */
	u_int8_t :8,:8,:8;
	u_int8_t busy;			/* window move active	      0x4044 */
	u_int8_t :8,:8,:8;
	u_int8_t vtrace_request;	/* vert retrace intr request  0x4048 */
	u_int8_t :8,:8,:8;
	u_int8_t move_request;		/* window move intr request   0x404C */
	u_int8_t f3[0x4080-0x404c-1];
	u_int8_t nblank;		/* display enable planes      0x4080 */
	u_int8_t f4[0x4088-0x4080-1];
	u_int8_t wen;			/* write enable plane 	      0x4088 */
	u_int8_t f5[0x408c-0x4088-1];
	u_int8_t ren;			/* read enable plane          0x408c */
	u_int8_t f6[0x4090-0x408c-1];
	u_int8_t fben;			/* frame buffer write enable  0x4090 */
	u_int8_t f7[0x409c-0x4090-1];
	u_int8_t wmove;			/* start window move 	      0x409c */
	u_int8_t f8[0x40a0-0x409c-1];
	u_int8_t blink;			/* enable blink planes 	      0x40a0 */
	u_int8_t f9[0x40a8-0x40a0-1];
	u_int8_t altframe;		/* enable alternate frame     0x40a8 */
	u_int8_t f10[0x40ac-0x40a8-1];
	u_int8_t curon;			/* cursor control register    0x40ac */
	u_int8_t f11[0x40ea-0x40ac-1];
	u_int8_t prr;			/* pixel replacement rule     0x40ea */
	u_int8_t f12[0x40ef-0x40ea-1];
	u_int8_t wmrr;			/* move replacement rule      0x40ef */
	u_int8_t f13[0x40f2-0x40ef-1];
	u_int16_t source_x;		/* source x pixel # 	      0x40f2 */
	u_int8_t f14[0x40f6-0x40f2-2];
	u_int16_t source_y;		/* source y pixel # 	      0x40f6 */
	u_int8_t f15[0x40fa-0x40f6-2];
	u_int16_t dest_x;		/* dest x pixel # 	      0x40fa */
	u_int8_t f16[0x40fe -0x40fa-2];
	u_int16_t dest_y;		/* dest y pixel # 	      0x40fe */
	u_int8_t f17[0x4102-0x40fe -2];
	u_int16_t wwidth;		/* block mover pixel width    0x4102 */
	u_int8_t f18[0x4106-0x4102-2];
	u_int16_t wheight;		/* block mover pixel height   0x4106 */
  /* Catseye */
	u_int8_t f19[0x4206-0x4106-2];
	u_int16_t rug_cmdstat;		/* RUG Command/Staus	      0x4206 */
	u_int8_t f20[0x4510-0x4206-2];
	u_int16_t vb_select;		/* Vector/BitBlt Select	      0x4510 */
	u_int16_t tcntrl;		/* Three Operand Control      0x4512 */
	u_int16_t acntrl;		/* BitBlt Mode		      0x4514 */
	u_int16_t pncntrl;		/* Plane Control	      0x4516 */
	u_int8_t f21[0x4800-0x4516-2];
	u_int16_t catseye_status;	/* Catseye Status	      0x4800 */
  /* End of Catseye */
	u_int8_t f22[0x6002-0x4800-2];
	u_int16_t cmap_busy;		/* Color Ram busy	      0x6002 */
	u_int8_t f23[0x60b2-0x6002-2];
	u_int16_t rdata;		/* color map red data 	      0x60b2 */
	u_int16_t gdata;		/* color map green data       0x60b4 */
	u_int16_t bdata;		/* color map blue data 	      0x60b6 */
	u_int16_t cindex;		/* color map index 	      0x60b8 */
	u_int16_t plane_mask;		/* plane mask select	      0x60ba */
	u_int8_t f24[0x60f0-0x60ba-2];
	u_int16_t strobe;		/* color map trigger 	      0x60f0 */
};
