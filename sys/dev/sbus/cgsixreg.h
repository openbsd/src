/*	$OpenBSD: cgsixreg.h,v 1.3 2002/07/30 23:03:30 jason Exp $	*/

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Jason L. Wright
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

union bt_cmap {
	u_int8_t cm_map[256][3];	/* 256 r/b/g entries */
	u_int32_t cm_chip[256 * 3 / 4];	/* the way the chip is loaded */
};

#define	BT_ADDR		0x00		/* map address register */
#define	BT_CMAP		0x04		/* colormap data register */
#define	BT_CTRL		0x08		/* control register */
#define	BT_OMAP		0x0c		/* overlay (cursor) map register */

#define	BT_D4M3(x)	((((x) >> 2) << 1) + ((x) >> 2)) /* (x / 4) * 3 */
#define	BT_D4M4(x)	((x) & ~3)			 /* (x / 4) * 4 */

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

#define	CG6_FBC_MODE		0x004		/* mode setting */
#define	CG6_FBC_CLIP		0x008		/* ??? */
#define	CG6_FBC_S		0x010		/* global status */
#define	CG6_FBC_DRAW		0x014		/* drawing pipeline status */
#define	CG6_FBC_BLIT		0x018		/* blitter status */
#define	CG6_FBC_X0		0x080		/* blitter, src llx */
#define	CG6_FBC_Y0		0x084		/* blitter, src lly */
#define	CG6_FBC_X1		0x090		/* blitter, src urx */
#define	CG6_FBC_Y1		0x094		/* blitter, src ury */
#define	CG6_FBC_X2		0x0a0		/* blitter, dst llx */
#define	CG6_FBC_Y2		0x0a4		/* blitter, dst lly */
#define	CG6_FBC_X3		0x0b0		/* blitter, dst urx */
#define	CG6_FBC_Y3		0x0b4		/* blitter, dst ury */
#define	CG6_FBC_OFFX		0x0c0		/* x offset for drawing */
#define	CG6_FBC_OFFY		0x0c4		/* y offset for drawing */
#define	CG6_FBC_CLIPMINX	0x0e0		/* clip rectangle llx */
#define	CG6_FBC_CLIPMINY	0x0e4		/* clip rectangle lly */
#define	CG6_FBC_CLIPMAXX	0x0f0		/* clip rectangle urx */
#define	CG6_FBC_CLIPMAXY	0x0f4		/* clip rectangle ury */
#define	CG6_FBC_FG		0x100		/* fg value for rop */
#define	CG6_FBC_ALU		0x108		/* operation */
#define	CG6_FBC_ARECTX		0x900		/* rectangle drawing, x coord */
#define	CG6_FBC_ARECTY		0x904		/* rectangle drawing, y coord */

#define	FBC_MODE_VAL	(						\
	  0x00200000 /* GX_BLIT_SRC */					\
	| 0x00020000 /* GX_MODE_COLOR8 */				\
	| 0x00008000 /* GX_DRAW_RENDER */				\
	| 0x00002000 /* GX_BWRITE0_ENABLE */				\
	| 0x00001000 /* GX_BWRITE1_DISABLE */				\
	| 0x00000200 /* GX_BREAD_0 */					\
	| 0x00000080 /* GX_BDISP_0 */					\
)
#define	FBC_MODE_MASK	(						\
	  0x00300000 /* GX_BLIT_ALL */					\
	| 0x00060000 /* GX_MODE_ALL */					\
	| 0x00018000 /* GX_DRAW_ALL */					\
	| 0x00006000 /* GX_BWRITE0_ALL */				\
	| 0x00001800 /* GX_BWRITE1_ALL */				\
	| 0x00000600 /* GX_BREAD_ALL */					\
	| 0x00000180 /* GX_BDISP_ALL */					\
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

#define	CG6_TEC_MV		0x0		/* matrix stuff */
#define	CG6_TEC_CLIP		0x4		/* clipping stuff */
#define	CG6_TEC_VDC		0x8		/* ??? */

#define	CG6_THC_HSYNC1		0x800		/* horizontal sync timing */
#define	CG6_THC_HSYNC2		0x804		/* more hsync timing */
#define	CG6_THC_HSYNC3		0x808		/* yet more hsync timing */
#define	CG6_THC_VSYNC1		0x80c		/* vertical sync timing */
#define	CG6_THC_VSYNC2		0x810		/* only two of these */
#define	CG6_THC_REFRESH		0x814		/* refresh counter */
#define	CG6_THC_MISC		0x818		/* misc control/status */
#define	CG6_THC_CURSXY		0x8fc		/* cursor x/y, 16 bit each */
#define	CG6_THC_CURSMASK	0x900		/* cursor mask bits */
#define	CG6_THC_CURSBITS	0x980		/* cursor bits */

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

struct cgsix_softc {
	struct device sc_dev;
	struct sbusdev sc_sd;
	bus_space_tag_t sc_bustag;
	bus_addr_t sc_paddr;
	bus_space_handle_t sc_bt_regs;
	bus_space_handle_t sc_fhc_regs;
	bus_space_handle_t sc_thc_regs;
	bus_space_handle_t sc_tec_regs;
	bus_space_handle_t sc_vid_regs;
	bus_space_handle_t sc_fbc_regs;
	struct rasops_info sc_rasops;
	int sc_nscreens;
	int sc_width, sc_height, sc_depth, sc_linebytes;
	union bt_cmap sc_cmap;
	void *sc_ih;
	u_int sc_mode;
};

#define	CG6_USER_FBC	0x70000000
#define	CG6_USER_TEC	0x70001000
#define	CG6_USER_BTREGS	0x70002000
#define	CG6_USER_FHC	0x70004000
#define	CG6_USER_THC	0x70005000
#define	CG6_USER_ROM	0x70006000
#define	CG6_USER_RAM	0x70016000
#define	CG6_USER_DHC	0x80000000

#define	THC_READ(sc,r) \
    bus_space_read_4((sc)->sc_bustag, (sc)->sc_thc_regs, (r))
#define	THC_WRITE(sc,r,v) \
    bus_space_write_4((sc)->sc_bustag, (sc)->sc_thc_regs, (r), (v))

#define	TEC_READ(sc,r) \
    bus_space_read_4((sc)->sc_bustag, (sc)->sc_tec_regs, (r))
#define	TEC_WRITE(sc,r,v) \
    bus_space_write_4((sc)->sc_bustag, (sc)->sc_tec_regs, (r), (v))

#define	FHC_READ(sc) \
    bus_space_read_4((sc)->sc_bustag, (sc)->sc_fhc_regs, CG6_FHC)
#define	FHC_WRITE(sc,v) \
    bus_space_write_4((sc)->sc_bustag, (sc)->sc_fhc_regs, CG6_FHC, (v))

#define	FBC_READ(sc,r) \
    bus_space_read_4((sc)->sc_bustag, (sc)->sc_fbc_regs, (r))
#define	FBC_WRITE(sc,r,v) \
    bus_space_write_4((sc)->sc_bustag, (sc)->sc_fbc_regs, (r), (v))

#define	BT_WRITE(sc, reg, val) \
    bus_space_write_4((sc)->sc_bustag, (sc)->sc_bt_regs, (reg), (val))
#define	BT_READ(sc, reg) \
    bus_space_read_4((sc)->sc_bustag, (sc)->sc_bt_regs, (reg))
#define	BT_BARRIER(sc,reg,flags) \
    bus_space_barrier((sc)->sc_bustag, (sc)->sc_bt_regs, (reg), \
	sizeof(u_int32_t), (flags))

#define CG6_BLIT_WAIT(sc)					\
	while ((FBC_READ(sc, CG6_FBC_BLIT) &			\
	    (FBC_BLIT_UNKNOWN|FBC_BLIT_GXFULL)) ==		\
	    (FBC_BLIT_UNKNOWN|FBC_BLIT_GXFULL))
#define CG6_DRAW_WAIT(sc)					\
	while ((FBC_READ(sc, CG6_FBC_DRAW) &			\
	    (FBC_DRAW_UNKNOWN|FBC_DRAW_GXFULL)) ==		\
	    (FBC_DRAW_UNKNOWN|FBC_DRAW_GXFULL))
#define	CG6_DRAIN(sc)						\
	while (FBC_READ(sc, CG6_FBC_S) & FBC_S_GXINPROGRESS)

#define	CG6_CFFLAG_NOACCEL	0x1	/* disable console acceleration */
