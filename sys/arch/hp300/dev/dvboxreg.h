/*	$OpenBSD: dvboxreg.h,v 1.1 2005/01/14 22:39:25 miod Exp $	*/
/*	$NetBSD: grf_dvreg.h,v 1.5 1994/10/26 07:23:50 cgd Exp $	*/

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
 * from: Utah $Hdr: grf_dvreg.h 1.5 92/01/21$
 *
 *	@(#)grf_dvreg.h	8.1 (Berkeley) 6/10/93
 */

#define db_waitbusy(regaddr)						\
do {									\
	while (((volatile struct dvboxfb *)(regaddr))->wbusy != 0 ||	\
	       ((volatile struct dvboxfb *)(regaddr))->as_busy != 0)	\
		DELAY(10);						\
} while (0)

#define	DVBOX_DUALROP(rop)	((rop) << 4 | (rop))

struct rgb {
	u_int8_t :8, :8, :8;
	u_int8_t red;
	u_int8_t :8, :8, :8;
	u_int8_t green;
	u_int8_t :8, :8, :8;
	u_int8_t blue;
};

struct dvboxfb {
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
	u_int8_t fbid;			/* frame buffer id		0x15 */
	u_int8_t f1[0x47];
	u_int8_t fbomsb;		/* frame buffer offset MSB	0x5d */
	u_int8_t :8;
	u_int8_t fbolsb;		/* frame buffer offset LSB	0x5f */
	u_int8_t f2[16359];
	u_int8_t wbusy;			/* Window move in progress    0x4047 */
	u_int8_t f3[0x405b-0x4047-1];
	u_int8_t as_busy;		/* Scan accessing frame buf.  0x405B */
	u_int8_t f4[0x4090-0x405b-1];
	u_int32_t fbwen;		/* Frame buffer write enable  0x4090 */
	u_int8_t f5[0x409f-0x4090-4];
	u_int8_t wmove;			/* Initiate window move.      0x409F */
	u_int8_t f6[0x40b3-0x409f-1];
	u_int8_t fold;			/* Byte/longword per pixel    0x40B3 */
	u_int8_t f7[0x40b7-0x40b3-1];
	u_int8_t opwen;			/* Overlay plane write enable 0x40B7 */
	u_int8_t f8[0x40bf-0x40b7-1];
	u_int8_t drive;			/* Select FB vs. Overlay.     0x40BF */

	u_int8_t f8a[0x40cb-0x40bf-1];
	u_int8_t zconfig;		/* Z buffer configuration     0x40CB */
	u_int8_t f8b[0x40cf-0x40cb-1];
	u_int8_t alt_rr;		/* Alternate replacement rule 0x40CF */
	u_int8_t f8c[0x40d3-0x40cf-1];
	u_int8_t zrr;			/* Z replacement rule	      0x40D3 */

	u_int8_t f9[0x40d7-0x40d3-1];
	u_int8_t en_scan;		/* Enable scan DTACK.	      0x40D7 */
	u_int8_t f10[0x40ef-0x40d7-1];
	u_int8_t rep_rule;		/* Replacement rule	      0x40EF */
	u_int8_t f11[0x40f2-0x40ef-1];
	u_int16_t source_x;		/* Window source X origin     0x40F2 */
	u_int8_t f12[0x40f6-0x40f2-2];
	u_int16_t source_y;		/* Window source Y origin     0x40F6 */
	u_int8_t f13[0x40fa-0x40f6-2];
	u_int16_t dest_x;		/* Window dest X origin       0x40FA */
	u_int8_t f14[0x40fe -0x40fa-2];
	u_int16_t dest_y;		/* Window dest Y origin       0x40FE */
	u_int8_t f15[0x4102-0x40fe -2];
	u_int16_t wwidth;		/* Window width		      0x4102 */
	u_int8_t f16[0x4106-0x4102-2];
	u_int16_t wheight;		/* Window height	      0x4106 */
	u_int8_t f17[0x6003-0x4106-2];
	u_int8_t cmapbank;		/* Bank select (0 or 1)       0x6003 */
	u_int8_t f18[0x6007-0x6003-1];
	u_int8_t dispen;		/* Display enable	      0x6007 */

	u_int8_t f18a[0x600B-0x6007-1];
	u_int8_t fbvenp;		/* Frame buffer video enable  0x600B */
	u_int8_t f18b[0x6017-0x600B-1];
	u_int8_t fbvens;		/* fbvenp blink counterpart   0x6017 */

	u_int8_t f19[0x6023-0x6017-1];
	u_int8_t vdrive;		/* Video display mode	      0x6023 */
	u_int8_t f20[0x6083-0x6023-1];
	u_int8_t panxh;			/* Pan display in X (high)    0x6083 */
	u_int8_t f21[0x6087-0x6083-1];
	u_int8_t panxl;			/* Pan display in X (low)     0x6087 */
	u_int8_t f22[0x608b-0x6087-1];
	u_int8_t panyh;			/* Pan display in Y (high)    0x608B */
	u_int8_t f23[0x608f-0x608b-1];
	u_int8_t panyl;			/* Pan display in Y (low)     0x608F */
	u_int8_t f24[0x6093-0x608f-1];
	u_int8_t zoom;			/* Zoom factor		      0x6093 */
	u_int8_t f25[0x6097-0x6093-1];
	u_int8_t pz_trig;		/* Pan & zoom trigger	      0x6097 */
	u_int8_t f26[0x609b-0x6097-1];
	u_int8_t ovly0p;		/* Overlay 0 primary map      0x609B */
	u_int8_t f27[0x609f-0x609b-1];
	u_int8_t ovly1p;		/* Overlay 1 primary map      0x609F */
	u_int8_t f28[0x60a3-0x609f-1];
	u_int8_t ovly0s;		/* Overlay 0 secondary map    0x60A3 */
	u_int8_t f29[0x60a7-0x60a3-1];
	u_int8_t ovly1s;		/* Overlay 1 secondary map    0x60A7 */
	u_int8_t f30[0x60ab-0x60a7-1];
	u_int8_t opvenp;		/* Overlay video enable	      0x60AB */
	u_int8_t f31[0x60af-0x60ab-1];
	u_int8_t opvens;		/* Overlay blink enable	      0x60AF */
	u_int8_t f32[0x60b3-0x60af-1];
	u_int8_t fv_trig;		/* Trigger control registers  0x60B3 */
	u_int8_t f33[0x60b7-0x60b3-1];
	u_int8_t cdwidth;		/* Iris cdwidth timing reg.   0x60B7 */
	u_int8_t f34[0x60bb-0x60b7-1];
	u_int8_t chstart;		/* Iris chstart timing reg.   0x60BB */
	u_int8_t f35[0x60bf-0x60bb-1];
	u_int8_t cvwidth;		/* Iris cvwidth timing reg.   0x60BF */
	u_int8_t f36[0x6100-0x60bf-1];
	struct rgb rgb[8];		/* overlay color map */
	u_int8_t f37[0x6403-0x6100-sizeof(struct rgb)*8];
	u_int8_t red0;
	u_int8_t f38[0x6803-0x6403-1];
	u_int8_t green0;
	u_int8_t f39[0x6c03-0x6803-1];
	u_int8_t blue0;
	u_int8_t f40[0x7403-0x6c03-1];
	u_int8_t red1;
	u_int8_t f41[0x7803-0x7403-1];
	u_int8_t green1;
	u_int8_t f42[0x7c03-0x7803-1];
	u_int8_t blue1;
	u_int8_t f43[0x8012-0x7c03-1];
	u_int16_t status1;		/* Master Status register     0x8012 */
	u_int8_t f44[0xC226-0x8012-2];
	u_int16_t trans;		/* Transparency		      0xC226 */
	u_int8_t f45[0xC23E -0xC226-2];
	u_int16_t pstop;		/* Pace value control	      0xc23e */
};
