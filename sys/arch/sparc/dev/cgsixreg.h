/*	$OpenBSD: cgsixreg.h,v 1.6 2003/06/02 23:27:54 millert Exp $	*/
/*	$NetBSD: cgsixreg.h,v 1.4 1996/02/27 22:09:31 thorpej Exp $ */

/*
 * Copyright (c) 2002 Jason L. Wright (jason@thought.net)
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
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
 *	@(#)cgsixreg.h	8.4 (Berkeley) 1/21/94
 */

/*
 * CG6 display registers.
 *
 * The cg6 is a complicated beastie.  We have been unable to extract any
 * documentation and most of the following are guesses based on a limited
 * amount of reverse engineering.
 *
 * A cg6 is composed of numerous groups of control registers, all with TLAs:
 *	FBC - frame buffer control?
 *	FHC - fbc hardware configuration / control? register (32 bits)
 *	DHC - ???
 *	TEC - transform engine control?
 *	THC - TEC Hardware Configuration
 *	ROM - a 64Kbyte ROM with who knows what in it.
 *	colormap - see below
 *	frame buffer memory (video RAM)
 *	possible other stuff
 *
 * Like the cg3, the cg6 uses a Brooktree Video DAC (see btreg.h).
 *
 * Various revisions of the cgsix have various hardware bugs.  So far,
 * we have only seen rev 1 & 2.
 */

/* offsets */
#define	CGSIX_ROM_OFFSET	0x000000
#define	CGSIX_BT_OFFSET		0x200000
#define	CGSIX_BT_SIZE		(sizeof(u_int32_t) * 4)
#define	CGSIX_DHC_OFFSET	0x240000
#define	CGSIX_ALT_OFFSET	0x280000
#define	CGSIX_FHC_OFFSET	0x300000
#define	CGSIX_FHC_SIZE		(sizeof(u_int32_t) * 1)
#define	CGSIX_THC_OFFSET	0x301000
#define	CGSIX_THC_SIZE		(sizeof(u_int32_t) * 640)
#define	CGSIX_FBC_OFFSET	0x700000
#define	CGSIX_FBC_SIZE		0x1000
#define	CGSIX_TEC_OFFSET	0x701000
#define	CGSIX_TEC_SIZE		(sizeof(u_int32_t) * 3)
#define	CGSIX_VID_OFFSET	0x800000
#define	CGSIX_VID_SIZE		(1024 * 1024)

#define	CG6_FHC			0x0		/* fhc register */

/* bits in FHC register */
#define	FHC_FBID_MASK		0xff000000	/* frame buffer id */
#define	FHC_FBID_SHIFT		24
#define	FHC_REV_MASK		0x00f00000	/* revision */
#define	FHC_REV_SHIFT		20
#define	FHC_FROP_DISABLE	0x00080000	/* disable fast rasterop */
#define	FHC_ROW_DISABLE		0x00040000	/* ??? */
#define	FHC_SRC_DISABLE		0x00020000	/* ??? */
#define	FHC_DST_DISABLE		0x00010000	/* disable dst cache */
#define	FHC_RESET		0x00008000	/* ??? */
#define	FHC_LEBO		0x00002000	/* set little endian order */
#define	FHC_RES_MASK		0x00001800	/* resolution: */
#define	FHC_RES_1024		0x00000000	/*  1024x768 */
#define	FHC_RES_1152		0x00000800	/*  1152x900 */
#define	FHC_RES_1280		0x00001000	/*  1280x1024 */
#define	FHC_RES_1600		0x00001800	/*  1600x1200 */
#define	FHC_CPU_MASK		0x00000600	/* cpu type: */
#define	FHC_CPU_SPARC		0x00000000	/*  sparc */
#define	FHC_CPU_68020		0x00000200	/*  68020 */
#define	FHC_CPU_386		0x00000400	/*  i386 */
#define	FHC_TEST		0x00000100	/* test window */
#define	FHC_TESTX_MASK		0x000000f0	/* test window X */
#define	FHC_TESTX_SHIFT		4
#define	FHC_TESTY_MASK		0x0000000f	/* test window Y */
#define	FHC_TESTY_SHIFT		0

struct cgsix_fbc {
	u_int32_t		fbc_xxx0[1];
	u_int32_t		fbc_mode;	/* mode setting */
	u_int32_t		fbc_clip;	/* ??? */
	u_int32_t		fbc_xxx1[1];
	u_int32_t		fbc_s;		/* global status */
	u_int32_t		fbc_draw;	/* drawing pipeline status */
	u_int32_t		fbc_blit;	/* blitter status */
	u_int32_t		fbc_xxx2[25];
	u_int32_t		fbc_x0;		/* blitter, src llx */
	u_int32_t		fbc_y0;		/* blitter, src lly */
	u_int32_t		fbc_xxx3[2];
	u_int32_t		fbc_x1;		/* blitter, src urx */
	u_int32_t		fbc_y1;		/* blitter, src ury */
	u_int32_t		fbc_xxx4[2];
	u_int32_t		fbc_x2;		/* blitter, dst llx */
	u_int32_t		fbc_y2;		/* blitter, dst lly */
	u_int32_t		fbc_xxx5[2];
	u_int32_t		fbc_x3;		/* blitter, dst urx */
	u_int32_t		fbc_y3;		/* blitter, dst ury */
	u_int32_t		fbc_xxx6[2];
	u_int32_t		fbc_offx;	/* x offset for drawing */
	u_int32_t		fbc_offy;	/* y offset for drawing */
	u_int32_t		fbc_xxx7[6];
	u_int32_t		fbc_clipminx;	/* clip rectangle llx */
	u_int32_t		fbc_clipminy;	/* clip rectangle lly */
	u_int32_t		fbc_xxx8[2];
	u_int32_t		fbc_clipmaxx;	/* clip rectangle urx */
	u_int32_t		fbc_clipmaxy;	/* clip rectangle ury */
	u_int32_t		fbc_xxx9[2];
	u_int32_t		fbc_fg;		/* fg value for rop */
	u_int32_t		fbc_xxx10[1];
	u_int32_t		fbc_alu;	/* operation */
	u_int32_t		fbc_xxx11[509];
	u_int32_t		fbc_arectx;	/* rectangle drawing, x coord */
	u_int32_t		fbc_arecty;	/* rectangle drawing, y coord */
};

#define FBC_MODE_MASK	(						\
	  0x00300000 /* GX_BLIT_ALL */					\
	| 0x00060000 /* GX_MODE_ALL */					\
	| 0x00018000 /* GX_DRAW_ALL */					\
	| 0x00006000 /* GX_BWRITE0_ALL */				\
	| 0x00001800 /* GX_BWRITE1_ALL */				\
	| 0x00000600 /* GX_BREAD_ALL */					\
	| 0x00000180 /* GX_BDISP_ALL */					\
)

#define	FBC_MODE_VAL	(						\
	  0x00200000 /* GX_BLIT_SRC */					\
	| 0x00020000 /* GX_MODE_COLOR8 */				\
	| 0x00008000 /* GX_DRAW_RENDER */				\
	| 0x00002000 /* GX_BWRITE0_ENABLE */				\
	| 0x00001000 /* GX_BWRITE1_DISABLE */				\
	| 0x00000200 /* GX_BREAD_0 */					\
	| 0x00000080 /* GX_BDISP_0 */					\
)

#define	FBC_S_GXINPROGRESS	0x10000000	/* drawing in progress */

#define	FBC_BLIT_UNKNOWN	0x80000000	/* ??? */
#define	FBC_BLIT_GXFULL		0x20000000	/* queue is full */

#define	FBC_DRAW_UNKNOWN	0x80000000	/* ??? */
#define	FBC_DRAW_GXFULL		0x20000000

/* Value for the alu register for screen-to-screen copies */
#define FBC_ALU_COPY    (						\
	  0x80000000 /* GX_PLANE_ONES (ignore planemask register) */	\
	| 0x20000000 /* GX_PIXEL_ONES (ignore pixelmask register) */	\
	| 0x00800000 /* GX_ATTR_SUPP (function unknown) */		\
	| 0x00000000 /* GX_RAST_BOOL (function unknown) */		\
	| 0x00000000 /* GX_PLOT_PLOT (function unknown) */		\
	| 0x08000000 /* GX_PATTERN_ONES (ignore pattern) */		\
	| 0x01000000 /* GX_POLYG_OVERLAP (unsure - handle overlap?) */	\
	| 0x0000cccc /* ALU = src */					\
)

/* Value for the alu register for region fills */
#define FBC_ALU_FILL	(						\
	  0x80000000 /* GX_PLANE_ONES (ignore planemask register) */	\
	| 0x20000000 /* GX_PIXEL_ONES (ignore pixelmask register) */	\
	| 0x00800000 /* GX_ATTR_SUPP (function unknown) */		\
	| 0x00000000 /* GX_RAST_BOOL (function unknown) */		\
	| 0x00000000 /* GX_PLOT_PLOT (function unknown) */		\
	| 0x08000000 /* GX_PATTERN_ONES (ignore pattern) */		\
	| 0x01000000 /* GX_POLYG_OVERLAP (unsure - handle overlap?) */	\
	| 0x0000ff00 /* ALU = fg color */				\
)

/* Value for the alu register for toggling an area */
#define FBC_ALU_FLIP	(						\
	  0x80000000 /* GX_PLANE_ONES (ignore planemask register) */	\
	| 0x20000000 /* GX_PIXEL_ONES (ignore pixelmask register) */	\
	| 0x00800000 /* GX_ATTR_SUPP (function unknown) */		\
	| 0x00000000 /* GX_RAST_BOOL (function unknown) */		\
	| 0x00000000 /* GX_PLOT_PLOT (function unknown) */		\
	| 0x08000000 /* GX_PATTERN_ONES (ignore pattern) */		\
	| 0x01000000 /* GX_POLYG_OVERLAP (unsure - handle overlap?) */	\
	| 0x00005555 /* ALU = ~dst */					\
)

/*
 * The layout of the THC.
 */
struct cgsix_thc {
	u_int32_t	thc_xxx0[512];	/* ??? */
	u_int32_t	thc_hsync1;	/* horizontal sync timing */
	u_int32_t	thc_hsync2;	/* more hsync timing */
	u_int32_t	thc_hsync3;	/* yet more hsync timing */
	u_int32_t	thc_vsync1;	/* vertical sync timing */
	u_int32_t	thc_vsync2;	/* only two of these */
	u_int32_t	thc_refresh;	/* refresh counter */
	u_int32_t	thc_misc;	/* miscellaneous control & status */
	u_int32_t	thc_xxx1[56];	/* ??? */
	u_int32_t	thc_cursxy;	/* cursor x,y position (16 bits each) */
	u_int32_t	thc_cursmask[32]; /* cursor mask bits */
	u_int32_t	thc_cursbits[32]; /* what to show where mask enabled */
};

/* cursor x/y position for 'off' */
#define	THC_CURSOFF		((65536-32) | ((65536-32) << 16))

#define	THC_MISC_REV_M		0x000f0000	/* chip revision */
#define	THC_MISC_REV_S		16
#define	THC_MISC_RESET		0x00001000	/* reset */
#define	THC_MISC_VIDEN		0x00000400	/* video enable */
#define	THC_MISC_SYNC		0x00000200	/* not sure what ... */
#define	THC_MISC_VSYNC		0x00000100	/* ... these really are */
#define	THC_MISC_SYNCEN		0x00000080	/* sync enable */
#define	THC_MISC_CURSRES	0x00000040	/* cursor resolution */
#define	THC_MISC_INTEN		0x00000020	/* v.retrace intr enable */
#define	THC_MISC_INTR		0x00000010	/* intr pending/ack */
#define	THC_MISC_CYCLS		0x0000000f	/* cycles before transfer */

/*
 * Partial description of TEC (needed to get around FHC rev 1 bugs).
 */
struct cgsix_tec_xxx {
	u_int32_t	tec_mv;		/* matrix stuff */
	u_int32_t	tec_clip;	/* clipping stuff */
	u_int32_t	tec_vdc;	/* ??? */
};
