/*	$OpenBSD: cgsix.c,v 1.23 2002/07/26 04:24:44 jason Exp $	*/

/*
 * Copyright (c) 2001 Jason L. Wright (jason@thought.net)
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
 *
 * Effort sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F30602-01-2-0537.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/autoconf.h>
#include <machine/openfirm.h>

#include <dev/sbus/sbusvar.h>
#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wscons_raster.h>
#include <dev/rasops/rasops.h>

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

#define	FBC_MODE_MASK	(						\
	  0x00200000 /* GX_BLIT_SRC */					\
	| 0x00020000 /* GX_MODE_COLOR8 */				\
	| 0x00008000 /* GX_DRAW_RENDER */				\
	| 0x00002000 /* GX_BWRITE0_ENABLE */				\
	| 0x00001000 /* GX_BWRITE1_DISABLE */				\
	| 0x00000200 /* GX_BREAD_0 */					\
	| 0x00000080 /* GX_BDISP_0 */					\
)
#define FBC_MODE_VAL   (						\
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

struct wsscreen_descr cgsix_stdscreen = {
	"std",
	0, 0,	/* will be filled in -- XXX shouldn't, it's global. */
	0,
	0, 0,
	WSSCREEN_REVERSE | WSSCREEN_WSCOLORS
};

const struct wsscreen_descr *cgsix_scrlist[] = {
	&cgsix_stdscreen,
	/* XXX other formats? */
};

struct wsscreen_list cgsix_screenlist = {
	sizeof(cgsix_scrlist) / sizeof(struct wsscreen_descr *), cgsix_scrlist
};

int cgsix_ioctl(void *, u_long, caddr_t, int, struct proc *);
int cgsix_alloc_screen(void *, const struct wsscreen_descr *, void **,
    int *, int *, long *);
void cgsix_free_screen(void *, void *);
int cgsix_show_screen(void *, void *, int, void (*cb)(void *, int, int),
    void *);
paddr_t cgsix_mmap(void *, off_t, int);
int cgsix_is_console(int);
int cg6_bt_getcmap(union bt_cmap *, struct wsdisplay_cmap *);
int cg6_bt_putcmap(union bt_cmap *, struct wsdisplay_cmap *);
void cgsix_loadcmap_immediate(struct cgsix_softc *, u_int, u_int);
void cgsix_loadcmap_deferred(struct cgsix_softc *, u_int, u_int);
void cgsix_setcolor(struct cgsix_softc *, u_int,
    u_int8_t, u_int8_t, u_int8_t);
void cgsix_reset(struct cgsix_softc *);
void cgsix_hardreset(struct cgsix_softc *);
void cgsix_burner(void *, u_int, u_int);
int cgsix_intr(void *);
static int a2int(char *, int);
void cgsix_ras_init(struct cgsix_softc *);
void cgsix_ras_copyrows(void *, int, int, int);
void cgsix_ras_copycols(void *, int, int, int, int);
void cgsix_ras_erasecols(void *, int, int, int, long int);
void cgsix_ras_eraserows(void *, int, int, long int);
void cgsix_ras_do_cursor(struct rasops_info *);

struct wsdisplay_accessops cgsix_accessops = {
	cgsix_ioctl,
	cgsix_mmap,
	cgsix_alloc_screen,
	cgsix_free_screen,
	cgsix_show_screen,
	NULL,	/* load_font */
	NULL,	/* scrollback */
	NULL,	/* getchar */
	cgsix_burner,
};

int	cgsixmatch(struct device *, void *, void *);
void	cgsixattach(struct device *, struct device *, void *);

struct cfattach cgsix_ca = {
	sizeof (struct cgsix_softc), cgsixmatch, cgsixattach
};

struct cfdriver cgsix_cd = {
	NULL, "cgsix", DV_DULL
};

int
cgsixmatch(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	struct cfdata *cf = vcf;
	struct sbus_attach_args *sa = aux;

	return (strcmp(cf->cf_driver->cd_name, sa->sa_name) == 0);
}

void    
cgsixattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct cgsix_softc *sc = (struct cgsix_softc *)self;
	struct sbus_attach_args *sa = aux;
	struct wsemuldisplaydev_attach_args waa;
	int console, i;
	long defattr;

	sc->sc_bustag = sa->sa_bustag;
	sc->sc_paddr = sbus_bus_addr(sa->sa_bustag, sa->sa_slot, sa->sa_offset);

	if (sa->sa_nreg != 1) {
		printf(": expected %d registers, got %d\n", 1, sa->sa_nreg);
		goto fail;
	}

	/*
	 * Map just BT, FHC, FBC, THC, and video RAM.
	 */
	if (sbus_bus_map(sa->sa_bustag, sa->sa_reg[0].sbr_slot,
	    sa->sa_reg[0].sbr_offset + CGSIX_BT_OFFSET,
	    CGSIX_BT_SIZE, 0, 0, &sc->sc_bt_regs) != 0) {
		printf(": cannot map bt registers\n");
		goto fail_bt;
	}

	if (sbus_bus_map(sa->sa_bustag, sa->sa_reg[0].sbr_slot,
	    sa->sa_reg[0].sbr_offset + CGSIX_FHC_OFFSET,
	    CGSIX_FHC_SIZE, 0, 0, &sc->sc_fhc_regs) != 0) {
		printf(": cannot map fhc registers\n");
		goto fail_fhc;
	}

	if (sbus_bus_map(sa->sa_bustag, sa->sa_reg[0].sbr_slot,
	    sa->sa_reg[0].sbr_offset + CGSIX_THC_OFFSET,
	    CGSIX_THC_SIZE, 0, 0, &sc->sc_thc_regs) != 0) {
		printf(": cannot map thc registers\n");
		goto fail_thc;
	}

	if (sbus_bus_map(sa->sa_bustag, sa->sa_reg[0].sbr_slot,
	    sa->sa_reg[0].sbr_offset + CGSIX_VID_OFFSET,
	    CGSIX_VID_SIZE, BUS_SPACE_MAP_LINEAR, 0, &sc->sc_vid_regs) != 0) {
		printf(": cannot map vid registers\n");
		goto fail_vid;
	}

	if (sbus_bus_map(sa->sa_bustag, sa->sa_reg[0].sbr_slot,
	    sa->sa_reg[0].sbr_offset + CGSIX_TEC_OFFSET,
	    CGSIX_TEC_SIZE, 0, 0, &sc->sc_tec_regs) != 0) {
		printf(": cannot map tec registers\n");
		goto fail_tec;
	}

	if (sbus_bus_map(sa->sa_bustag, sa->sa_reg[0].sbr_slot,
	    sa->sa_reg[0].sbr_offset + CGSIX_FBC_OFFSET,
	    CGSIX_FBC_SIZE, 0, 0, &sc->sc_fbc_regs) != 0) {
		printf(": cannot map fbc registers\n");
		goto fail_fbc;
	}

	if ((sc->sc_ih = bus_intr_establish(sa->sa_bustag, sa->sa_pri,
	    IPL_TTY, 0, cgsix_intr, sc)) == NULL) {
		printf(": couldn't establish interrupt, pri %d\n", sa->sa_pri);
		goto fail_intr;
	}

	/* if prom didn't initialize us, do it the hard way */
	if (OF_getproplen(sa->sa_node, "width") != sizeof(u_int32_t))
		cgsix_hardreset(sc);

	console = cgsix_is_console(sa->sa_node);

	cgsix_reset(sc);

	/* grab the current palette */
	BT_WRITE(sc, BT_ADDR, 0);
	for (i = 0; i < 256; i++) {
		sc->sc_cmap.cm_map[i][0] = BT_READ(sc, BT_CMAP) >> 24;
		sc->sc_cmap.cm_map[i][1] = BT_READ(sc, BT_CMAP) >> 24;
		sc->sc_cmap.cm_map[i][2] = BT_READ(sc, BT_CMAP) >> 24;
	}

	cgsix_burner(sc, 1, 0);

	sc->sc_depth = getpropint(sa->sa_node, "depth", 8);
	sc->sc_linebytes = getpropint(sa->sa_node, "linebytes", 1152);
	sc->sc_height = getpropint(sa->sa_node, "height", 900);
	sc->sc_width = getpropint(sa->sa_node, "width", 1152);

	sbus_establish(&sc->sc_sd, self);

	sc->sc_rasops.ri_depth = sc->sc_depth;
	sc->sc_rasops.ri_stride = sc->sc_linebytes;
	sc->sc_rasops.ri_flg = RI_CENTER;
	sc->sc_rasops.ri_bits = (void *)bus_space_vaddr(sc->sc_bustag,
	    sc->sc_vid_regs);
	sc->sc_rasops.ri_width = sc->sc_width;
	sc->sc_rasops.ri_height = sc->sc_height;
	sc->sc_rasops.ri_hw = sc;

	rasops_init(&sc->sc_rasops,
	    a2int(getpropstring(optionsnode, "screen-#rows"), 34),
	    a2int(getpropstring(optionsnode, "screen-#columns"), 80));
	sc->sc_rasops.ri_hw = sc;
	sc->sc_rasops.ri_ops.copyrows = cgsix_ras_copyrows;
	sc->sc_rasops.ri_ops.copycols = cgsix_ras_copycols;
	sc->sc_rasops.ri_ops.eraserows = cgsix_ras_eraserows;
	sc->sc_rasops.ri_ops.erasecols = cgsix_ras_erasecols;
	sc->sc_rasops.ri_do_cursor = cgsix_ras_do_cursor;
#if 0
	cgsix_ras_init(sc);
#endif

	cgsix_stdscreen.nrows = sc->sc_rasops.ri_rows;
	cgsix_stdscreen.ncols = sc->sc_rasops.ri_cols;
	cgsix_stdscreen.textops = &sc->sc_rasops.ri_ops;
	sc->sc_rasops.ri_ops.alloc_attr(&sc->sc_rasops,
	    WSCOL_BLACK, WSCOL_WHITE, WSATTR_WSCOLORS, &defattr);

	printf("\n");

	if (console) {
		int *ccolp, *crowp;

		cgsix_setcolor(sc, WSCOL_BLACK, 0, 0, 0);
		cgsix_setcolor(sc, 255, 0, 0, 0);
		cgsix_setcolor(sc, WSCOL_RED, 255, 0, 0);
		cgsix_setcolor(sc, WSCOL_GREEN, 0, 255, 0);
		cgsix_setcolor(sc, WSCOL_BROWN, 154, 85, 46);
		cgsix_setcolor(sc, WSCOL_BLUE, 0, 0, 255);
		cgsix_setcolor(sc, WSCOL_MAGENTA, 255, 255, 0);
		cgsix_setcolor(sc, WSCOL_CYAN, 0, 255, 255);
		cgsix_setcolor(sc, WSCOL_WHITE, 255, 255, 255);

		if (romgetcursoraddr(&crowp, &ccolp))
			ccolp = crowp = NULL;
		if (ccolp != NULL)
			sc->sc_rasops.ri_ccol = *ccolp;
		if (crowp != NULL)
			sc->sc_rasops.ri_crow = *crowp;

		wsdisplay_cnattach(&cgsix_stdscreen, &sc->sc_rasops,
		    sc->sc_rasops.ri_ccol, sc->sc_rasops.ri_crow, defattr);
	}

	waa.console = console;
	waa.scrdata = &cgsix_screenlist;
	waa.accessops = &cgsix_accessops;
	waa.accesscookie = sc;
	config_found(self, &waa, wsemuldisplaydevprint);

	return;

fail_intr:
	bus_space_unmap(sa->sa_bustag, sc->sc_fbc_regs, CGSIX_FBC_SIZE);
fail_fbc:
	bus_space_unmap(sa->sa_bustag, sc->sc_tec_regs, CGSIX_TEC_SIZE);
fail_tec:
	bus_space_unmap(sa->sa_bustag, sc->sc_vid_regs, CGSIX_VID_SIZE);
fail_vid:
	bus_space_unmap(sa->sa_bustag, sc->sc_thc_regs, CGSIX_THC_SIZE);
fail_thc:
	bus_space_unmap(sa->sa_bustag, sc->sc_fhc_regs, CGSIX_FHC_SIZE);
fail_fhc:
	bus_space_unmap(sa->sa_bustag, sc->sc_bt_regs, CGSIX_BT_SIZE);
fail_bt:
fail:
}

int
cgsix_ioctl(v, cmd, data, flags, p)
	void *v;
	u_long cmd;
	caddr_t data;
	int flags;
	struct proc *p;
{
	struct cgsix_softc *sc = v;
	struct wsdisplay_cmap *cm;
	struct wsdisplay_fbinfo *wdf;
	int error;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_UNKNOWN;
		break;
	case WSDISPLAYIO_GINFO:
		wdf = (void *)data;
		wdf->height = sc->sc_height;
		wdf->width  = sc->sc_width;
		wdf->depth  = sc->sc_depth;
		wdf->cmsize = 256;
		break;
	case WSDISPLAYIO_LINEBYTES:
		*(u_int *)data = sc->sc_linebytes;
		break;

	case WSDISPLAYIO_GETCMAP:
		cm = (struct wsdisplay_cmap *)data;
		error = cg6_bt_getcmap(&sc->sc_cmap, cm);
		if (error)
			return (error);
		break;

	case WSDISPLAYIO_PUTCMAP:
		cm = (struct wsdisplay_cmap *)data;
		error = cg6_bt_putcmap(&sc->sc_cmap, cm);
		if (error)
			return (error);
		cgsix_loadcmap_deferred(sc, cm->index, cm->count);
		break;

	case WSDISPLAYIO_SVIDEO:
	case WSDISPLAYIO_GVIDEO:
	case WSDISPLAYIO_GCURPOS:
	case WSDISPLAYIO_SCURPOS:
	case WSDISPLAYIO_GCURMAX:
	case WSDISPLAYIO_GCURSOR:
	case WSDISPLAYIO_SCURSOR:
	default:
		return -1; /* not supported yet */
        }

	return (0);
}

int
cgsix_alloc_screen(v, type, cookiep, curxp, curyp, attrp)
	void *v;
	const struct wsscreen_descr *type;
	void **cookiep;
	int *curxp, *curyp;
	long *attrp;
{
	struct cgsix_softc *sc = v;

	if (sc->sc_nscreens > 0)
		return (ENOMEM);

	*cookiep = &sc->sc_rasops;
	*curyp = 0;
	*curxp = 0;
	sc->sc_rasops.ri_ops.alloc_attr(&sc->sc_rasops,
	    WSCOL_BLACK, WSCOL_WHITE, WSATTR_WSCOLORS, attrp);
	sc->sc_nscreens++;
	return (0);
}

void
cgsix_free_screen(v, cookie)
	void *v;
	void *cookie;
{
	struct cgsix_softc *sc = v;

	sc->sc_nscreens--;
}

int
cgsix_show_screen(v, cookie, waitok, cb, cbarg)
	void *v;
	void *cookie;
	int waitok;
	void (*cb)(void *, int, int);
	void *cbarg;
{
	return (0);
}

paddr_t
cgsix_mmap(v, off, prot)
	void *v;
	off_t off;
	int prot;
{
	struct cgsix_softc *sc = v;

	if (off & PGOFSET)
		return (-1);

	/* Allow mapping as a dumb framebuffer from offset 0 */
	if (off >= 0 && off < (sc->sc_linebytes * sc->sc_height))
		return (bus_space_mmap(sc->sc_bustag, sc->sc_paddr,
		    off + CGSIX_VID_OFFSET, prot, BUS_SPACE_MAP_LINEAR));

	return (-1);
}

static int
a2int(char *cp, int deflt)
{
	int i = 0;

	if (*cp == '\0')
		return (deflt);
	while (*cp != '\0')
		i = i * 10 + *cp++ - '0';
	return (i);
}

int
cgsix_is_console(node)
	int node;
{
	extern int fbnode;

	return (fbnode == node);
}

int
cg6_bt_getcmap(bcm, rcm)
	union bt_cmap *bcm;
	struct wsdisplay_cmap *rcm;
{
	u_int index = rcm->index, count = rcm->count, i;
	int error;

	if (index >= 256 || index + count > 256)
		return (EINVAL);
	for (i = 0; i < count; i++) {
		if ((error = copyout(&bcm->cm_map[index + i][0],
		    &rcm->red[i], 1)) != 0)
			return (error);
		if ((error = copyout(&bcm->cm_map[index + i][1],
		    &rcm->green[i], 1)) != 0)
			return (error);
		if ((error = copyout(&bcm->cm_map[index + i][2],
		    &rcm->blue[i], 1)) != 0)
			return (error);
	}
	return (0);
}

int
cg6_bt_putcmap(bcm, rcm)
	union bt_cmap *bcm;
	struct wsdisplay_cmap *rcm;
{
	u_int index = rcm->index, count = rcm->count, i;
	int error;

	if (index >= 256 || rcm->count > 256 ||
	    (rcm->index + rcm->count) > 256)
		return (EINVAL);
	for (i = 0; i < count; i++) {
		if ((error = copyin(&rcm->red[i],
		    &bcm->cm_map[index + i][0], 1)) != 0)
			return (error);
		if ((error = copyin(&rcm->green[i],
		    &bcm->cm_map[index + i][1], 1)) != 0)
			return (error);
		if ((error = copyin(&rcm->blue[i],
		    &bcm->cm_map[index + i][2], 1)) != 0)
			return (error);
	}
	return (0);
}

void
cgsix_loadcmap_deferred(sc, start, ncolors)
	struct cgsix_softc *sc;
	u_int start, ncolors;
{
	u_int32_t thcm;

	thcm = THC_READ(sc, CG6_THC_MISC);
	thcm &= ~THC_MISC_RESET;
	thcm |= THC_MISC_INTEN;
	THC_WRITE(sc, CG6_THC_MISC, thcm);
}

void
cgsix_loadcmap_immediate(sc, start, ncolors)
	struct cgsix_softc *sc;
	u_int start, ncolors;
{
	u_int cstart;
	u_int32_t v;
	int count;

	cstart = BT_D4M3(start);
	count = BT_D4M3(start + ncolors - 1) - BT_D4M3(start) + 3;
	BT_WRITE(sc, BT_ADDR, BT_D4M4(start) << 24);
	while (--count >= 0) {
		v = sc->sc_cmap.cm_chip[cstart];
		BT_WRITE(sc, BT_CMAP, v << 0);
		BT_WRITE(sc, BT_CMAP, v << 8);
		BT_WRITE(sc, BT_CMAP, v << 16);
		BT_WRITE(sc, BT_CMAP, v << 24);
		cstart++;
	}
}

void
cgsix_setcolor(sc, index, r, g, b)
	struct cgsix_softc *sc;
	u_int index;
	u_int8_t r, g, b;
{
	union bt_cmap *bcm = &sc->sc_cmap;

	bcm->cm_map[index][0] = r;
	bcm->cm_map[index][1] = g;
	bcm->cm_map[index][2] = b;
	cgsix_loadcmap_immediate(sc, index, 1);
}

void
cgsix_reset(sc)
	struct cgsix_softc *sc;
{
	u_int32_t fhc, rev;

	/* hide the cursor, just in case */
	THC_WRITE(sc, CG6_THC_CURSXY, THC_CURSOFF);

	TEC_WRITE(sc, CG6_TEC_MV, 0);
	TEC_WRITE(sc, CG6_TEC_CLIP, 0);
	TEC_WRITE(sc, CG6_TEC_VDC, 0);

	fhc = FHC_READ(sc);
	rev = (fhc & FHC_REV_MASK) >> FHC_REV_SHIFT;
	/* take core of hardware bugs in old revisions */
	if (rev < 5) {
		/*
		 * Keep current resolution; set cpu to 68020, set test
		 * window (size 1Kx1K), and for rev 1, disable dest cache.
		 */
		fhc &= FHC_RES_MASK;
		fhc |= FHC_CPU_68020 | FHC_TEST |
		    (11 << FHC_TESTX_SHIFT) | (11 << FHC_TESTY_SHIFT);
		if (rev < 2)
			fhc |= FHC_DST_DISABLE;
		FHC_WRITE(sc, fhc);
	}

	/* enable cursor in brooktree DAC */
	BT_WRITE(sc, BT_ADDR, 0x6 << 24);
	BT_WRITE(sc, BT_CTRL, BT_READ(sc, BT_CTRL) | (0x3 << 24));
}

void
cgsix_hardreset(sc)
	struct cgsix_softc *sc;
{
	u_int32_t fhc, rev;

	/* setup brooktree */
	BT_WRITE(sc, BT_ADDR, 0x04 << 24);
	BT_BARRIER(sc, BT_ADDR, BUS_SPACE_BARRIER_WRITE);
	BT_WRITE(sc, BT_CTRL, 0xff << 24);
	BT_BARRIER(sc, BT_CTRL, BUS_SPACE_BARRIER_WRITE);

	BT_WRITE(sc, BT_ADDR, 0x05 << 24);
	BT_BARRIER(sc, BT_ADDR, BUS_SPACE_BARRIER_WRITE);
	BT_WRITE(sc, BT_CTRL, 0x00 << 24);
	BT_BARRIER(sc, BT_CTRL, BUS_SPACE_BARRIER_WRITE);

	BT_WRITE(sc, BT_ADDR, 0x06 << 24);
	BT_BARRIER(sc, BT_ADDR, BUS_SPACE_BARRIER_WRITE);
	BT_WRITE(sc, BT_CTRL, 0x70 << 24);
	BT_BARRIER(sc, BT_CTRL, BUS_SPACE_BARRIER_WRITE);

	BT_WRITE(sc, BT_ADDR, 0x07 << 24);
	BT_BARRIER(sc, BT_ADDR, BUS_SPACE_BARRIER_WRITE);
	BT_WRITE(sc, BT_CTRL, 0x00 << 24);
	BT_BARRIER(sc, BT_CTRL, BUS_SPACE_BARRIER_WRITE);


	/* configure thc */
	THC_WRITE(sc, CG6_THC_MISC, THC_MISC_RESET | THC_MISC_INTR |
	    THC_MISC_CYCLS);
	THC_WRITE(sc, CG6_THC_MISC, THC_MISC_INTR | THC_MISC_CYCLS);

	THC_WRITE(sc, CG6_THC_HSYNC1, 0x10009);
	THC_WRITE(sc, CG6_THC_HSYNC2, 0x570000);
	THC_WRITE(sc, CG6_THC_HSYNC3, 0x15005d);
	THC_WRITE(sc, CG6_THC_VSYNC1, 0x10005);
	THC_WRITE(sc, CG6_THC_VSYNC2, 0x2403a8);
	THC_WRITE(sc, CG6_THC_REFRESH, 0x16b);

	THC_WRITE(sc, CG6_THC_MISC, THC_MISC_RESET | THC_MISC_INTR |
	    THC_MISC_CYCLS);
	THC_WRITE(sc, CG6_THC_MISC, THC_MISC_INTR | THC_MISC_CYCLS);

	/* configure fhc (1152x900) */
	fhc = FHC_READ(sc);
	rev = (fhc & FHC_REV_MASK) >> FHC_REV_SHIFT;

	fhc = FHC_RES_1152 | FHC_CPU_68020 | FHC_TEST;
	if (rev < 1)
		fhc |= FHC_FROP_DISABLE;
	if (rev < 2)
		fhc |= FHC_DST_DISABLE;
	FHC_WRITE(sc, fhc);
}

void
cgsix_burner(vsc, on, flags)
	void *vsc;
	u_int on, flags;
{
	struct cgsix_softc *sc = vsc;
	int s;
	u_int32_t thcm;

	s = splhigh();
	thcm = THC_READ(sc, CG6_THC_MISC);
	if (on)
		thcm |= THC_MISC_VIDEN | THC_MISC_SYNCEN;
	else {
		thcm &= ~THC_MISC_VIDEN;
		if (flags & WSDISPLAY_BURN_VBLANK)
			thcm &= ~THC_MISC_SYNCEN;
	}
	THC_WRITE(sc, CG6_THC_MISC, thcm);
	splx(s);
}

int
cgsix_intr(vsc)
	void *vsc;
{
	struct cgsix_softc *sc = vsc;
	u_int32_t thcm;

	thcm = THC_READ(sc, CG6_THC_MISC);
	if ((thcm & (THC_MISC_INTEN | THC_MISC_INTR)) !=
	    (THC_MISC_INTEN | THC_MISC_INTR)) {
		/* Not expecting an interrupt, it's not for us. */
		return (0);
	}

	/* Acknowledge the interrupt and disable it. */
	thcm &= ~(THC_MISC_RESET | THC_MISC_INTEN);
	thcm |= THC_MISC_INTR;
	THC_WRITE(sc, CG6_THC_MISC, thcm);
	cgsix_loadcmap_immediate(sc, 0, 256);
	return (1);
}

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

void
cgsix_ras_init(sc)
	struct cgsix_softc *sc;
{
	u_int32_t m;

	CG6_DRAIN(sc);
	m = FBC_READ(sc, CG6_FBC_MODE);
	m &= ~FBC_MODE_MASK;
	m |= FBC_MODE_VAL;
	FBC_WRITE(sc, CG6_FBC_MODE, m);
}

void
cgsix_ras_copyrows(cookie, src, dst, n)
	void *cookie;
	int src, dst, n;
{
	struct rasops_info *ri = cookie;
	struct cgsix_softc *sc = ri->ri_hw;

	if (dst == src)
		return;
	if (src < 0) {
		n += src;
		src = 0;
	}
	if (src + n > ri->ri_rows)
		n = ri->ri_rows - src;
	if (dst < 0) {
		n += dst;
		dst = 0;
	}
	if (dst + n > ri->ri_rows)
		n = ri->ri_rows - dst;
	if (n <= 0)
		return;
	n *= ri->ri_font->fontheight;
	src *= ri->ri_font->fontheight;
	dst *= ri->ri_font->fontheight;

	FBC_WRITE(sc, CG6_FBC_CLIP, 0);
	FBC_WRITE(sc, CG6_FBC_S, 0);
	FBC_WRITE(sc, CG6_FBC_OFFX, 0);
	FBC_WRITE(sc, CG6_FBC_OFFY, 0);
	FBC_WRITE(sc, CG6_FBC_CLIPMINX, 0);
	FBC_WRITE(sc, CG6_FBC_CLIPMINY, 0);
	FBC_WRITE(sc, CG6_FBC_CLIPMAXX, ri->ri_width - 1);
	FBC_WRITE(sc, CG6_FBC_CLIPMAXY, ri->ri_height - 1);
	FBC_WRITE(sc, CG6_FBC_ALU, FBC_ALU_COPY);
	FBC_WRITE(sc, CG6_FBC_X0, ri->ri_xorigin);
	FBC_WRITE(sc, CG6_FBC_Y0, ri->ri_yorigin + src);
	FBC_WRITE(sc, CG6_FBC_X1, ri->ri_xorigin + ri->ri_emuwidth - 1);
	FBC_WRITE(sc, CG6_FBC_Y1, ri->ri_yorigin + src + n - 1);
	FBC_WRITE(sc, CG6_FBC_X2, ri->ri_xorigin);
	FBC_WRITE(sc, CG6_FBC_Y2, ri->ri_yorigin + dst);
	FBC_WRITE(sc, CG6_FBC_X3, ri->ri_xorigin + ri->ri_emuwidth - 1);
	FBC_WRITE(sc, CG6_FBC_Y3, ri->ri_yorigin + dst + n - 1);
	CG6_BLIT_WAIT(sc);
	CG6_DRAIN(sc);
}

void
cgsix_ras_copycols(cookie, row, src, dst, n)
	void *cookie;
	int row, src, dst, n;
{
	struct rasops_info *ri = cookie;
	struct cgsix_softc *sc = ri->ri_hw;

	if (dst == src)
		return;
	if ((row < 0) || (row >= ri->ri_rows))
		return;
	if (src < 0) {
		n += src;
		src = 0;
	}
	if (src + n > ri->ri_cols)
		n = ri->ri_cols - src;
	if (dst < 0) {
		n += dst;
		dst = 0;
	}
	if (dst + n > ri->ri_cols)
		n = ri->ri_cols - dst;
	if (n <= 0)
		return;
	n *= ri->ri_font->fontwidth;
	src *= ri->ri_font->fontwidth;
	dst *= ri->ri_font->fontwidth;
	row *= ri->ri_font->fontheight;

	FBC_WRITE(sc, CG6_FBC_CLIP, 0);
	FBC_WRITE(sc, CG6_FBC_S, 0);
	FBC_WRITE(sc, CG6_FBC_OFFX, 0);
	FBC_WRITE(sc, CG6_FBC_OFFY, 0);
	FBC_WRITE(sc, CG6_FBC_CLIPMINX, 0);
	FBC_WRITE(sc, CG6_FBC_CLIPMINY, 0);
	FBC_WRITE(sc, CG6_FBC_CLIPMAXX, ri->ri_width - 1);
	FBC_WRITE(sc, CG6_FBC_CLIPMAXY, ri->ri_height - 1);
	FBC_WRITE(sc, CG6_FBC_ALU, FBC_ALU_COPY);
	FBC_WRITE(sc, CG6_FBC_X0, ri->ri_xorigin + src);
	FBC_WRITE(sc, CG6_FBC_Y0, ri->ri_yorigin + row);
	FBC_WRITE(sc, CG6_FBC_X1, ri->ri_xorigin + src + n - 1);
	FBC_WRITE(sc, CG6_FBC_Y1,
	    ri->ri_yorigin + row + ri->ri_font->fontheight - 1);
	FBC_WRITE(sc, CG6_FBC_X2, ri->ri_xorigin + dst);
	FBC_WRITE(sc, CG6_FBC_Y2, ri->ri_yorigin + row);
	FBC_WRITE(sc, CG6_FBC_X3, ri->ri_xorigin + dst + n - 1);
	FBC_WRITE(sc, CG6_FBC_Y3,
	    ri->ri_yorigin + row + ri->ri_font->fontheight - 1);
	CG6_BLIT_WAIT(sc);
	CG6_DRAIN(sc);
}

void
cgsix_ras_erasecols(cookie, row, col, n, attr)
	void *cookie;
	int row, col, n;
	long int attr;
{
	struct rasops_info *ri = cookie;
	struct cgsix_softc *sc = ri->ri_hw;

	if ((row < 0) || (row >= ri->ri_rows))
		return;
	if (col < 0) {
		n += col;
		col = 0;
	}
	if (col + n > ri->ri_cols)
		n = ri->ri_cols - col;
	if (n <= 0)
		return;
	n *= ri->ri_font->fontwidth;
	col *= ri->ri_font->fontwidth;
	row *= ri->ri_font->fontheight;

	FBC_WRITE(sc, CG6_FBC_CLIP, 0);
	FBC_WRITE(sc, CG6_FBC_S, 0);
	FBC_WRITE(sc, CG6_FBC_OFFX, 0);
	FBC_WRITE(sc, CG6_FBC_OFFY, 0);
	FBC_WRITE(sc, CG6_FBC_CLIPMINX, 0);
	FBC_WRITE(sc, CG6_FBC_CLIPMINY, 0);
	FBC_WRITE(sc, CG6_FBC_CLIPMAXX, ri->ri_width - 1);
	FBC_WRITE(sc, CG6_FBC_CLIPMAXY, ri->ri_height - 1);
	FBC_WRITE(sc, CG6_FBC_ALU, FBC_ALU_FILL);
	FBC_WRITE(sc, CG6_FBC_FG, ri->ri_devcmap[(attr >> 16) & 0xf]);
	FBC_WRITE(sc, CG6_FBC_ARECTY, ri->ri_yorigin + row);
	FBC_WRITE(sc, CG6_FBC_ARECTX, ri->ri_xorigin + col);
	FBC_WRITE(sc, CG6_FBC_ARECTY,
	    ri->ri_yorigin + row + ri->ri_font->fontheight - 1);
	FBC_WRITE(sc, CG6_FBC_ARECTX, ri->ri_xorigin + col + n - 1);
	CG6_DRAW_WAIT(sc);
	CG6_DRAIN(sc);
}

void
cgsix_ras_eraserows(cookie, row, n, attr)
	void *cookie;
	int row, n;
	long int attr;
{
	struct rasops_info *ri = cookie;
	struct cgsix_softc *sc = ri->ri_hw;

	if (row < 0) {
		n += row;
		row = 0;
	}
	if (row + n > ri->ri_rows)
		n = ri->ri_rows - row;
	if (n <= 0)
		return;

	FBC_WRITE(sc, CG6_FBC_CLIP, 0);
	FBC_WRITE(sc, CG6_FBC_S, 0);
	FBC_WRITE(sc, CG6_FBC_OFFX, 0);
	FBC_WRITE(sc, CG6_FBC_OFFY, 0);
	FBC_WRITE(sc, CG6_FBC_CLIPMINX, 0);
	FBC_WRITE(sc, CG6_FBC_CLIPMINY, 0);
	FBC_WRITE(sc, CG6_FBC_CLIPMAXX, ri->ri_width - 1);
	FBC_WRITE(sc, CG6_FBC_CLIPMAXY, ri->ri_height - 1);
	FBC_WRITE(sc, CG6_FBC_ALU, FBC_ALU_FILL);
	FBC_WRITE(sc, CG6_FBC_FG, ri->ri_devcmap[(attr >> 16) & 0xf]);
	if ((n == ri->ri_rows) && (ri->ri_flg & RI_FULLCLEAR)) {
		FBC_WRITE(sc, CG6_FBC_ARECTY, 0);
		FBC_WRITE(sc, CG6_FBC_ARECTX, 0);
		FBC_WRITE(sc, CG6_FBC_ARECTY, ri->ri_height - 1);
		FBC_WRITE(sc, CG6_FBC_ARECTX, ri->ri_width - 1);
	} else {
		row *= ri->ri_font->fontheight;
		FBC_WRITE(sc, CG6_FBC_ARECTY, ri->ri_yorigin + row);
		FBC_WRITE(sc, CG6_FBC_ARECTX, ri->ri_xorigin);
		FBC_WRITE(sc, CG6_FBC_ARECTY,
		    ri->ri_yorigin + row + (n * ri->ri_font->fontheight) - 1);
		FBC_WRITE(sc, CG6_FBC_ARECTX,
		    ri->ri_xorigin + ri->ri_emuwidth - 1);
	}
	CG6_DRAW_WAIT(sc);
	CG6_DRAIN(sc);
}

void
cgsix_ras_do_cursor(ri)
	struct rasops_info *ri;
{
	struct cgsix_softc *sc = ri->ri_hw;
	int row, col;

	row = ri->ri_crow * ri->ri_font->fontheight;
	col = ri->ri_ccol * ri->ri_font->fontwidth;
	FBC_WRITE(sc, CG6_FBC_CLIP, 0);
	FBC_WRITE(sc, CG6_FBC_S, 0);
	FBC_WRITE(sc, CG6_FBC_OFFX, 0);
	FBC_WRITE(sc, CG6_FBC_OFFY, 0);
	FBC_WRITE(sc, CG6_FBC_CLIPMINX, 0);
	FBC_WRITE(sc, CG6_FBC_CLIPMINY, 0);
	FBC_WRITE(sc, CG6_FBC_CLIPMAXX, ri->ri_width - 1);
	FBC_WRITE(sc, CG6_FBC_CLIPMAXY, ri->ri_height - 1);
	FBC_WRITE(sc, CG6_FBC_ALU, FBC_ALU_FLIP);
	FBC_WRITE(sc, CG6_FBC_ARECTY, ri->ri_yorigin + row);
	FBC_WRITE(sc, CG6_FBC_ARECTX, ri->ri_xorigin + col);
	FBC_WRITE(sc, CG6_FBC_ARECTY,
	    ri->ri_yorigin + row + ri->ri_font->fontheight - 1);
	FBC_WRITE(sc, CG6_FBC_ARECTX,
	    ri->ri_xorigin + col + ri->ri_font->fontwidth - 1);
	CG6_DRAW_WAIT(sc);
	CG6_DRAIN(sc);
}
