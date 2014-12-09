/*	$OpenBSD: grtwo.c,v 1.11 2014/12/09 06:58:29 doug Exp $	*/
/* $NetBSD: grtwo.c,v 1.11 2009/11/22 19:09:15 mbalmer Exp $	 */

/*
 * Copyright (c) 2012, 2014 Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/*
 * Copyright (c) 2004 Christopher SEKIYA
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * <<Id: LICENSE_GC,v 1.1 2001/10/01 23:24:05 cgd Exp>>
 */

/* wscons driver for SGI GR2 family of framebuffers
 *
 * Heavily based on the newport wscons driver.
 */

/*
 * GR2 coordinates start from (0,0) in the lower left corner, to (1279,1023)
 * in the upper right. The low-level drawing routines will take care of
 * converting the traditional ``y goes down'' coordinates to those expected
 * by the hardware.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <machine/autoconf.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/rasops/rasops.h>

#include <sgi/dev/gl.h>
#include <sgi/gio/gioreg.h>
#include <sgi/gio/giovar.h>
#include <sgi/gio/grtworeg.h>
#include <dev/ic/bt458reg.h>
#include <sgi/gio/grtwovar.h>
#include <sgi/localbus/intreg.h>
#include <sgi/localbus/intvar.h>

#include <dev/cons.h>

#define	GRTWO_WIDTH	1280
#define	GRTWO_HEIGHT	1024

struct grtwo_softc {
	struct device		sc_dev;

	struct grtwo_devconfig *sc_dc;

	int			sc_nscreens;
	struct wsscreen_list	sc_wsl;
	const struct wsscreen_descr *sc_scrlist[1];
};

struct grtwo_devconfig {
	struct rasops_info		dc_ri;
	long				dc_defattr;
	struct wsdisplay_charcell	*dc_bs;

	uint32_t			dc_addr;
	bus_space_tag_t			iot;
	bus_space_handle_t		ioh;

	uint32_t			xmapmode;

	uint8_t				boardrev;
	uint8_t				backendrev;
	int				hq2rev;
	int				ge7rev;
	int				vc1rev;
	int				zbuffer;
	int				depth;
	int				monitor;

	struct grtwo_softc		*dc_sc;
	struct wsscreen_descr		dc_wsd;
};

int	grtwo_match(struct device *, void *, void *);
void	grtwo_attach(struct device *, struct device *, void *);

struct cfdriver grtwo_cd = {
	NULL, "grtwo", DV_DULL
};

const struct cfattach grtwo_ca = {
	sizeof(struct grtwo_softc), grtwo_match, grtwo_attach
};

/* accessops */
int	grtwo_ioctl(void *, u_long, caddr_t, int, struct proc *);
paddr_t	grtwo_mmap(void *, off_t, int);
int	grtwo_alloc_screen(void *, const struct wsscreen_descr *, void **,
	    int *, int *, long *);
void	grtwo_free_screen(void *, void *);
int	grtwo_show_screen(void *, void *, int, void (*)(void *, int, int),
	    void *);
int	grtwo_load_font(void *, void *, struct wsdisplay_font *);
int	grtwo_list_font(void *, struct wsdisplay_font *);
void	grtwo_burner(void *, u_int, u_int);

static struct wsdisplay_accessops grtwo_accessops = {
	.ioctl = grtwo_ioctl,
	.mmap = grtwo_mmap,
	.alloc_screen = grtwo_alloc_screen,
	.free_screen = grtwo_free_screen,
	.show_screen = grtwo_show_screen,
	.load_font = grtwo_load_font,
	.list_font = grtwo_list_font,
	.burn_screen = grtwo_burner
};

int	grtwo_cursor(void *, int, int, int);
int	grtwo_putchar(void *, int, int, u_int, long);
int	grtwo_copycols(void *, int, int, int, int);
int	grtwo_erasecols(void *, int, int, int, long);
int	grtwo_copyrows(void *, int, int, int);
int	grtwo_eraserows(void *, int, int, long);

void	grtwo_wait_gfifo(struct grtwo_devconfig *);
static __inline__
void	grtwo_set_color(bus_space_tag_t, bus_space_handle_t, int);
void	grtwo_fillrect(struct grtwo_devconfig *, int, int, int, int, int);
void	grtwo_copyrect(struct grtwo_devconfig *, int, int, int, int, int, int);
int	grtwo_setup_hw(struct grtwo_devconfig *);
static __inline__
int	grtwo_attach_common(struct grtwo_devconfig *, struct gio_attach_args *);
int	grtwo_init_screen(struct grtwo_devconfig *, int);
int	grtwo_putchar_internal(struct rasops_info *, int, int, u_int, int, int,
	    int);

static struct grtwo_devconfig grtwo_console_dc;
/* console backing store, worst cast font selection */
static struct wsdisplay_charcell
	grtwo_console_bs[(GRTWO_WIDTH / 8) * (GRTWO_HEIGHT / 16)];

void
grtwo_wait_gfifo(struct grtwo_devconfig *dc)
{
	int i;

	/*
	 * This loop is for paranoia. Of course there is currently no
	 * known way to whack the FIFO (or reset the board) in case it
	 * gets stuck... but this code is careful to avoid this situation
	 * and it should never happen (famous last words)
	 */
	for (i = 100000; i != 0; i--) {
		if (!int2_is_intr_pending(INT2_L0_FIFO))
			break;
		delay(1);
	}
#ifdef DIAGNOSTIC
	if (i == 0) {
		if (dc != &grtwo_console_dc && dc->dc_sc != NULL)
			printf("%s: FIFO is stuck\n",
			    dc->dc_sc->sc_dev.dv_xname);
	}
#endif
}

static __inline__ void
grtwo_set_color(bus_space_tag_t iot, bus_space_handle_t ioh, int color)
{
	bus_space_write_4(iot, ioh, GR2_FIFO_COLOR, color);
}

/*
 * Rectangle fill with the given background.
 */
void
grtwo_fillrect(struct grtwo_devconfig *dc, int x1, int y1, int x2,
    int y2, int bg)
{
	grtwo_wait_gfifo(dc);
	grtwo_set_color(dc->iot, dc->ioh, bg);

	grtwo_wait_gfifo(dc);
	bus_space_write_4(dc->iot, dc->ioh, GR2_FIFO_RECTI2D, x1);
	bus_space_write_4(dc->iot, dc->ioh, GR2_FIFO_DATA,
	    GRTWO_HEIGHT - 1 - y1);
	bus_space_write_4(dc->iot, dc->ioh, GR2_FIFO_DATA, x2);
	bus_space_write_4(dc->iot, dc->ioh, GR2_FIFO_DATA,
	    GRTWO_HEIGHT - 1 - y2);
}

/*
 * Rectangle copy.
 * Does not handle overlapping copies; this is handled at the wsdisplay
 * emulops level by splitting overlapping copies in smaller, non-overlapping,
 * operations.
 */
void
grtwo_copyrect(struct grtwo_devconfig *dc, int x1, int y1, int x2,
    int y2, int width, int height)
{
	int length = (width + 3) >> 2;
	int lines = 4864 / length;
	int step;

	y1 += height; y2 += height;
	while (height != 0) {
		step = imin(height, lines);
		y1 -= step; y2 -= step;

		grtwo_wait_gfifo(dc);
		bus_space_write_4(dc->iot, dc->ioh, GR2_FIFO_RECTCOPY, length);
		bus_space_write_4(dc->iot, dc->ioh, GR2_FIFO_DATA, lines);
		/* source */
		bus_space_write_4(dc->iot, dc->ioh, GR2_FIFO_DATA, x1);
		bus_space_write_4(dc->iot, dc->ioh, GR2_FIFO_DATA, y1);
		/* span */
		bus_space_write_4(dc->iot, dc->ioh, GR2_FIFO_DATA, width);
		bus_space_write_4(dc->iot, dc->ioh, GR2_FIFO_DATA, step);
		/* dest */
		bus_space_write_4(dc->iot, dc->ioh, GR2_FIFO_DATA, x2);
		bus_space_write_4(dc->iot, dc->ioh, GR2_FIFO_DATA, y2);

		height -= step;
	}
}

int
grtwo_setup_hw(struct grtwo_devconfig *dc)
{
	int i = 0;
	uint8_t rd0, rd1, rd2, rd3;
	uint32_t vc1;

	rd0 = bus_space_read_1(dc->iot, dc->ioh, GR2_REVISION_RD0);
	rd1 = bus_space_read_1(dc->iot, dc->ioh, GR2_REVISION_RD1);
	rd2 = bus_space_read_1(dc->iot, dc->ioh, GR2_REVISION_RD2);
	rd3 = bus_space_read_1(dc->iot, dc->ioh, GR2_REVISION_RD3);

	/* Get various revisions */
	dc->boardrev = ~rd0 & GR2_REVISION_RD0_VERSION_MASK;

	/*
	 * Boards prior to rev 4 have a pretty whacky config scheme.
	 * What is doubly weird is that i have a rev 2 board, but the rev 4
	 * probe routines work just fine.
	 * We'll trust SGI, though, and separate things a bit. It's only
	 * critical for the display depth calculation.
	 */

	if (dc->boardrev < 4) {
		dc->backendrev = ~(rd2 & GR2_REVISION_RD2_BACKEND_REV) >>
		    GR2_REVISION_RD2_BACKEND_SHIFT;
		if (dc->backendrev == 0)
			return ENXIO;
		dc->zbuffer = ~rd1 & GR2_REVISION_RD1_ZBUFFER;
		if ((rd3 & GR2_REVISION_RD3_VMA) != GR2_REVISION_RD3_VMA)
			i++;
		if ((rd3 & GR2_REVISION_RD3_VMB) != GR2_REVISION_RD3_VMB)
			i++;
		if ((rd3 & GR2_REVISION_RD3_VMC) != GR2_REVISION_RD3_VMC)
			i++;
		dc->depth = 8 * i;
		dc->monitor = ((rd2 & 0x03) << 1) | (rd1 & 0x01);
	} else {
		dc->backendrev = ~rd1 & GR2_REVISION4_RD1_BACKEND;
		if (dc->backendrev == 0)
			return ENXIO;
		dc->zbuffer = rd1 & GR2_REVISION4_RD1_ZBUFFER;
		dc->depth = (rd1 & GR2_REVISION4_RD1_24BPP) ? 24 : 8;
		dc->monitor = (rd0 & GR2_REVISION4_RD0_MONITOR_MASK) >>
		    GR2_REVISION4_RD0_MONITOR_SHIFT;
	}

	dc->hq2rev = (bus_space_read_4(dc->iot, dc->ioh, HQ2_VERSION) &
	    HQ2_VERSION_MASK) >> HQ2_VERSION_SHIFT;
	dc->ge7rev = (bus_space_read_4(dc->iot, dc->ioh, GE7_REVISION) &
	    GE7_REVISION_MASK) >> GE7_REVISION_SHIFT;
	/* dc->vc1rev = vc1_read_ireg(dc, 5) & 0x07; */

	vc1 = bus_space_read_4(dc->iot, dc->ioh, VC1_SYSCTL);
	if (vc1 == -1)
		return ENXIO;	/* XXX would need a reset */

	/* Turn on display, DID, disable cursor display */
	bus_space_write_4(dc->iot, dc->ioh, VC1_SYSCTL,
	    VC1_SYSCTL_VC1 | VC1_SYSCTL_DID);

	dc->xmapmode = bus_space_read_4(dc->iot, dc->ioh,
	    XMAP5_BASEALL + XMAP5_MODE);

	/*
	 * Setup Bt457 RAMDACs
	 */
	/* enable all planes */
	bus_space_write_1(dc->iot, dc->ioh, BT457_R + BT457_ADDR, BT_RMR);
	bus_space_write_1(dc->iot, dc->ioh, BT457_R + BT457_CTRL, 0xff);
	bus_space_write_1(dc->iot, dc->ioh, BT457_G + BT457_ADDR, BT_RMR);
	bus_space_write_1(dc->iot, dc->ioh, BT457_G + BT457_CTRL, 0xff);
	bus_space_write_1(dc->iot, dc->ioh, BT457_B + BT457_ADDR, BT_RMR);
	bus_space_write_1(dc->iot, dc->ioh, BT457_B + BT457_CTRL, 0xff);
	/* setup a regular gamma ramp */
	bus_space_write_1(dc->iot, dc->ioh, BT457_R + BT457_ADDR, 0);
	bus_space_write_1(dc->iot, dc->ioh, BT457_G + BT457_ADDR, 0);
	bus_space_write_1(dc->iot, dc->ioh, BT457_B + BT457_ADDR, 0);
	for (i = 0; i < 256; i++) {
		bus_space_write_1(dc->iot, dc->ioh, BT457_R + BT457_CMAPDATA,
		    i);
		bus_space_write_1(dc->iot, dc->ioh, BT457_G + BT457_CMAPDATA,
		    i);
		bus_space_write_1(dc->iot, dc->ioh, BT457_B + BT457_CMAPDATA,
		    i);
	}

	/*
	 * Setup Bt479 RAMDAC
	 */
	grtwo_wait_gfifo(dc);
	bus_space_write_1(dc->iot, dc->ioh, XMAP5_BASEALL + XMAP5_ADDRHI,
	    GR2_CMAP8 >> 8);
	bus_space_write_1(dc->iot, dc->ioh, XMAP5_BASEALL + XMAP5_ADDRLO,
	    GR2_CMAP8 & 0xff);
	bus_space_write_multi_1(dc->iot, dc->ioh, XMAP5_BASEALL + XMAP5_CLUT,
	    rasops_cmap, sizeof(rasops_cmap));

	return 0;
}

/* Attach routines */
int
grtwo_match(struct device *parent, void *vcf, void *aux)
{
	struct gio_attach_args *ga = aux;

	if (ga->ga_product != GIO_PRODUCT_FAKEID_GRTWO)
		return 0;

	return 1;
}

void
grtwo_attach(struct device *parent, struct device *self, void *aux)
{
	struct grtwo_softc *sc = (struct grtwo_softc *)self;
	struct gio_attach_args *ga = aux;
	struct grtwo_devconfig *dc;
	struct wsemuldisplaydev_attach_args waa;
	const char *descr;
	extern struct consdev wsdisplay_cons;

	descr = ga->ga_descr;
	if (descr == NULL || *descr == '\0')
		descr = "GR2";
	printf(": %s", descr);

	if (cn_tab == &wsdisplay_cons &&
	    ga->ga_addr == grtwo_console_dc.dc_addr) {
		waa.console = 1;
		dc = &grtwo_console_dc;
		sc->sc_nscreens = 1;
	} else {
		/*
		 * XXX The driver will not work correctly if we are not the
		 * XXX console device. An initialization is missing - it
		 * XXX seems that everything works, but the colormap is
		 * XXX stuck as black, which makes the device unusable.
		 */
		printf("\n%s: device has not been setup by firmware!\n",
		    self->dv_xname);
		return;
#if 0
		waa.console = 0;
		dc = malloc(sizeof(struct grtwo_devconfig),
		    M_DEVBUF, M_WAITOK | M_CANFAIL | M_ZERO);
		if (dc == NULL)
			goto out;
		if (grtwo_attach_common(dc, ga) != 0) {
			printf("\n%s: not responding\n", self->dv_xname);
			free(dc, M_DEVBUF, 0);
			return;
		}
		if (grtwo_init_screen(dc, M_WAITOK | M_CANFAIL) != 0) {
			free(dc, M_DEVBUF, 0);
			goto out;
		}
#endif
	}
	sc->sc_dc = dc;
	dc->dc_sc = sc;

	printf(", revision %d, monitor sense %d\n", dc->boardrev, dc->monitor);
	printf("%s: %dx%d %d-bit frame buffer\n",
	    self->dv_xname, GRTWO_WIDTH, GRTWO_HEIGHT, dc->depth);

	sc->sc_scrlist[0] = &dc->dc_wsd;
	sc->sc_wsl.nscreens = 1;
	sc->sc_wsl.screens = sc->sc_scrlist;

	waa.scrdata = &sc->sc_wsl;
	waa.accessops = &grtwo_accessops;
	waa.accesscookie = dc;
	waa.defaultscreens = 0;

	config_found(self, &waa, wsemuldisplaydevprint);
	return;

#if 0
out:
	printf("\n%s: failed to allocate memory\n", self->dv_xname);
	return;
#endif
}

int
grtwo_cnprobe(struct gio_attach_args *ga)
{
	return grtwo_match(NULL, NULL, ga);
}

int
grtwo_cnattach(struct gio_attach_args *ga)
{
	struct rasops_info *ri = &grtwo_console_dc.dc_ri;
	struct wsdisplay_charcell *cell;
	long defattr;
	int rc;
	int i;

	rc = grtwo_attach_common(&grtwo_console_dc, ga);
	if (rc != 0)
		return rc;
	grtwo_console_dc.dc_bs = grtwo_console_bs;
	rc = grtwo_init_screen(&grtwo_console_dc, M_NOWAIT);
	if (rc != 0)
		return rc;

	ri->ri_ops.alloc_attr(ri, 0, 0, 0, &defattr);
	cell = grtwo_console_bs;
	for (i = ri->ri_cols * ri->ri_rows; i != 0; i--, cell++)
		cell->attr = defattr;

	wsdisplay_cnattach(&grtwo_console_dc.dc_wsd, ri, 0, 0, defattr);

	return 0;
}

static __inline__ int
grtwo_attach_common(struct grtwo_devconfig *dc, struct gio_attach_args * ga)
{
	dc->dc_addr = ga->ga_addr;
	dc->iot = ga->ga_iot;
	dc->ioh = ga->ga_ioh;

	return grtwo_setup_hw(dc);
}

int
grtwo_init_screen(struct grtwo_devconfig *dc, int malloc_flags)
{
	struct rasops_info *ri = &dc->dc_ri;

	memset(ri, 0, sizeof(struct rasops_info));
	ri->ri_hw = dc;
	ri->ri_flg = RI_CENTER | RI_FULLCLEAR;
	/* for the proper operation of rasops computations, pretend 8bpp */
	ri->ri_depth = 8;
	ri->ri_stride = GRTWO_WIDTH;
	ri->ri_width = GRTWO_WIDTH;
	ri->ri_height = GRTWO_HEIGHT;
	rasops_init(ri, 160, 160);

	/*
	 * Allocate backing store to remember character cells, to
	 * be able to paint an inverted cursor.
	 */
	if (dc->dc_bs == NULL) {
		dc->dc_bs = mallocarray(ri->ri_rows, ri->ri_cols *
		    sizeof(struct wsdisplay_charcell), M_DEVBUF,
		    malloc_flags | M_ZERO);
		if (dc->dc_bs == NULL)
			return ENOMEM;
	}

	ri->ri_ops.cursor = grtwo_cursor;
	ri->ri_ops.copyrows = grtwo_copyrows;
	ri->ri_ops.eraserows = grtwo_eraserows;
	ri->ri_ops.copycols = grtwo_copycols;
	ri->ri_ops.erasecols = grtwo_erasecols;
	ri->ri_ops.putchar = grtwo_putchar;

	strlcpy(dc->dc_wsd.name, "std", sizeof(dc->dc_wsd.name));
	dc->dc_wsd.ncols = ri->ri_cols;
	dc->dc_wsd.nrows = ri->ri_rows;
	dc->dc_wsd.textops = &ri->ri_ops;
	dc->dc_wsd.fontwidth = ri->ri_font->fontwidth;
	dc->dc_wsd.fontheight = ri->ri_font->fontheight;
	dc->dc_wsd.capabilities = ri->ri_caps;

	grtwo_fillrect(dc, 0, 0, GRTWO_WIDTH - 1, GRTWO_HEIGHT - 1,
	    ri->ri_devcmap[WSCOL_BLACK]);

	return 0;
}

/* wsdisplay textops */

int
grtwo_cursor(void *c, int on, int row, int col)
{
	struct rasops_info *ri = c;
	struct grtwo_devconfig *dc = ri->ri_hw;
	struct wsdisplay_charcell *cell;
	int bg, fg, ul;

	cell = dc->dc_bs + row * ri->ri_cols + col;
	ri->ri_ops.unpack_attr(ri, cell->attr, &fg, &bg, &ul);

	if (on) {
		/* redraw the existing character with inverted colors */
		return grtwo_putchar_internal(ri, row, col, cell->uc,
		    ~ri->ri_devcmap[fg], ~ri->ri_devcmap[bg], ul);
	} else {
		/* redraw the existing character with correct colors */
		return grtwo_putchar_internal(ri, row, col, cell->uc,
		    ri->ri_devcmap[fg], ri->ri_devcmap[bg], ul);
	}
}

int
grtwo_putchar(void *c, int row, int col, u_int ch, long attr)
{
	struct rasops_info *ri = c;
	struct grtwo_devconfig *dc = ri->ri_hw;
	struct wsdisplay_charcell *cell;
	int bg, fg, ul;

	/* Update backing store. */
	cell = dc->dc_bs + row * ri->ri_cols + col;
	cell->uc = ch;
	cell->attr = attr;

	ri->ri_ops.unpack_attr(ri, attr, &fg, &bg, &ul);
	return grtwo_putchar_internal(ri, row, col, ch, ri->ri_devcmap[fg],
	    ri->ri_devcmap[bg], ul);
}

int
grtwo_putchar_internal(struct rasops_info *ri, int row, int col, u_int ch,
    int fg, int bg, int ul)
{
	struct grtwo_devconfig *dc = ri->ri_hw;
	struct wsdisplay_font *font = ri->ri_font;
	uint8_t *bitmap;
	uint32_t pattern;
	int x, y;
	int h = font->fontheight;
	int w = font->fontwidth;
	int i;

	/*
	 * The `draw char' operation below writes on top of the existing
	 * background. We need to paint the background first.
	 */
	x = ri->ri_xorigin + col * w;
	y = ri->ri_yorigin + row * h;
	grtwo_fillrect(dc, x, y, x + w - 1, y + h - 1, bg);

	if ((ch == ' ' || ch == 0) && ul == 0)
		return 0;

	/* Set the drawing color */
	grtwo_wait_gfifo(dc);
	grtwo_set_color(dc->iot, dc->ioh, fg);

	/*
	 * This character drawing operation apparently expects a 18 pixel
	 * character cell height. We will perform as many cell fillings as
	 * necessary to draw a complete glyph.
	 */
	bitmap = (uint8_t *)font->data +
	    (ch - font->firstchar + 1) * ri->ri_fontscale;
	y = ri->ri_height - h - y;
	while (h != 0) {
		/* Set drawing coordinates */
		grtwo_wait_gfifo(dc);
		bus_space_write_4(dc->iot, dc->ioh, GR2_FIFO_CMOV2I, x);
		bus_space_write_4(dc->iot, dc->ioh, GR2_FIFO_DATA, y);

		grtwo_wait_gfifo(dc);
		bus_space_write_4(dc->iot, dc->ioh, GR2_FIFO_DRAWCHAR, w);
		bus_space_write_4(dc->iot, dc->ioh, GR2_FIFO_DATA,
		    h > GR2_DRAWCHAR_HEIGHT ? GR2_DRAWCHAR_HEIGHT : h);
		bus_space_write_4(dc->iot, dc->ioh, GR2_FIFO_DATA, 2);
		/* (x,y) offset */
		bus_space_write_4(dc->iot, dc->ioh, GR2_FIFO_DATA, 0);
		bus_space_write_4(dc->iot, dc->ioh, GR2_FIFO_DATA, 0);
		bus_space_write_4(dc->iot, dc->ioh, GR2_FIFO_DATA, 0);
		bus_space_write_4(dc->iot, dc->ioh, GR2_FIFO_DATA, 0);

		grtwo_wait_gfifo(dc);
		if (w <= 8) {
			for (i = 0; i < GR2_DRAWCHAR_HEIGHT; i++) {
				if (h != 0) {
					bitmap -= font->stride;
					if (ul && h == font->fontheight - 1)
						pattern = 0xff << 8;
					else
						pattern = *bitmap << 8;
					h--;
				} else
					pattern = 0;

				bus_space_write_4(dc->iot, dc->ioh,
				    GR2_FIFO_DATA, pattern);
			}
		} else {
			for (i = 0; i < GR2_DRAWCHAR_HEIGHT; i++) {
				if (h != 0) {
					bitmap -= font->stride;
					if (ul && h == font->fontheight - 1)
						pattern = 0xffff;
					else
						pattern = *(uint16_t *)bitmap;
					h--;
				} else
					pattern = 0;

				bus_space_write_4(dc->iot, dc->ioh,
				    GR2_FIFO_DATA, pattern);
			}
		}

		y += GR2_DRAWCHAR_HEIGHT;
	}

	return 0;
}

int
grtwo_copycols(void *c, int row, int src, int dst, int ncol)
{
	struct rasops_info *ri = c;
	struct grtwo_devconfig *dc = ri->ri_hw;
	struct wsdisplay_font *font = ri->ri_font;
	struct wsdisplay_charcell *cell;
	int y = ri->ri_yorigin + row * font->fontheight;
	int delta, chunk;

	/* Copy columns in backing store. */
	cell = dc->dc_bs + row * ri->ri_cols;
	memmove(cell + dst, cell + src, ncol * sizeof(*cell));

	if (src > dst) {
		/* may overlap, copy in non-overlapping blocks */
		delta = src - dst;
		while (ncol > 0) {
			chunk = ncol <= delta ? ncol : delta;
			grtwo_copyrect(dc,
			    ri->ri_xorigin + src * font->fontwidth, y,
			    ri->ri_xorigin + dst * font->fontwidth, y,
			    chunk * font->fontwidth, font->fontheight);
			src += chunk;
			dst += chunk;
			ncol -= chunk;
		}
	} else {
		grtwo_copyrect(dc,
		    ri->ri_xorigin + src * font->fontwidth, y,
		    ri->ri_xorigin + dst * font->fontwidth, y,
		    ncol * font->fontwidth, font->fontheight);
	}

	return 0;
}

int
grtwo_erasecols(void *c, int row, int startcol, int ncol, long attr)
{
	struct rasops_info *ri = c;
	struct grtwo_devconfig *dc = ri->ri_hw;
	struct wsdisplay_font *font = ri->ri_font;
	struct wsdisplay_charcell *cell;
	int y = ri->ri_yorigin + row * font->fontheight;
	int i, bg, fg;

	/* Erase columns in backing store. */
	cell = dc->dc_bs + row * ri->ri_cols + startcol;
	for (i = ncol; i != 0; i--, cell++) {
		cell->uc = 0;
		cell->attr = attr;
	}

	ri->ri_ops.unpack_attr(ri, attr, &fg, &bg, NULL);

	grtwo_fillrect(dc, ri->ri_xorigin + startcol * font->fontwidth, y,
	    ri->ri_xorigin + (startcol + ncol) * font->fontwidth - 1,
	    y + font->fontheight - 1, ri->ri_devcmap[bg]);

	return 0;
}

int
grtwo_copyrows(void *c, int src, int dst, int nrow)
{
	struct rasops_info *ri = c;
	struct grtwo_devconfig *dc = ri->ri_hw;
	struct wsdisplay_font *font = ri->ri_font;
	struct wsdisplay_charcell *cell;
	int delta, chunk;

	/* Copy rows in backing store. */
	cell = dc->dc_bs + dst * ri->ri_cols;
	memmove(cell, dc->dc_bs + src * ri->ri_cols,
	    nrow * ri->ri_cols * sizeof(*cell));

	if (src > dst) {
		/* may overlap, copy in non-overlapping blocks */
		delta = src - dst;
		while (nrow > 0) {
			chunk = nrow <= delta ? nrow : delta;
			grtwo_copyrect(dc, ri->ri_xorigin,
			    ri->ri_yorigin + src * font->fontheight,
			    ri->ri_xorigin,
			    ri->ri_yorigin + dst * font->fontheight,
			    ri->ri_emuwidth, chunk * font->fontheight);
			src += chunk;
			dst += chunk;
			nrow -= chunk;
		}
	} else {
		grtwo_copyrect(dc, ri->ri_xorigin,
		    ri->ri_yorigin + src * font->fontheight, ri->ri_xorigin,
		    ri->ri_yorigin + dst * font->fontheight, ri->ri_emuwidth,
		    nrow * font->fontheight);
	}

	return 0;
}

int
grtwo_eraserows(void *c, int startrow, int nrow, long attr)
{
	struct rasops_info *ri = c;
	struct grtwo_devconfig *dc = ri->ri_hw;
	struct wsdisplay_font *font = ri->ri_font;
	struct wsdisplay_charcell *cell;
	int i, bg, fg;

	/* Erase rows in backing store. */
	cell = dc->dc_bs + startrow * ri->ri_cols;
	for (i = ri->ri_cols; i != 0; i--, cell++) {
		cell->uc = 0;
		cell->attr = attr;
	}
	for (i = 1; i < nrow; i++)
		memmove(dc->dc_bs + (startrow + i) * ri->ri_cols,
		    dc->dc_bs + startrow * ri->ri_cols,
		    ri->ri_cols * sizeof(*cell));

	ri->ri_ops.unpack_attr(ri, attr, &fg, &bg, NULL);

	if (nrow == ri->ri_rows && (ri->ri_flg & RI_FULLCLEAR)) {
		grtwo_fillrect(dc, 0, 0, GRTWO_WIDTH - 1, GRTWO_HEIGHT - 1,
		    ri->ri_devcmap[bg]);
		return 0;
	}

	grtwo_fillrect(dc, ri->ri_xorigin,
	    ri->ri_yorigin + startrow * font->fontheight,
	    ri->ri_xorigin + ri->ri_emuwidth - 1,
	    ri->ri_yorigin + (startrow + nrow) * font->fontheight - 1,
	    ri->ri_devcmap[bg]);

	return 0;
}

/* wsdisplay accessops */

int
grtwo_alloc_screen(void *v, const struct wsscreen_descr * type, void **cookiep,
    int *curxp, int *curyp, long *attrp)
{
	struct grtwo_devconfig *dc = v;
	struct rasops_info *ri = &dc->dc_ri;
	struct grtwo_softc *sc = dc->dc_sc;
	struct wsdisplay_charcell *cell;
	int i;

	if (sc->sc_nscreens > 0)
		return ENOMEM;

	sc->sc_nscreens++;

	*cookiep = ri;
	*curxp = *curyp = 0;
	ri->ri_ops.alloc_attr(ri, 0, 0, 0, &dc->dc_defattr);
	*attrp = dc->dc_defattr;

	cell = dc->dc_bs;
	for (i = ri->ri_cols * ri->ri_rows; i != 0; i--, cell++)
		cell->attr = dc->dc_defattr;

	return 0;
}

void
grtwo_free_screen(void *v, void *cookie)
{
}

int
grtwo_show_screen(void *v, void *cookie, int waitok,
    void (*cb) (void *, int, int), void *cbarg)
{
	return 0;
}

int
grtwo_ioctl(void *v, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct grtwo_devconfig *dc = v;
	struct rasops_info *ri = &dc->dc_ri;
	struct wsdisplay_fbinfo *fb;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(u_int *) data = WSDISPLAY_TYPE_GRTWO;
		break;
	case WSDISPLAYIO_GINFO:
		fb = (struct wsdisplay_fbinfo *)data;
		fb->width = ri->ri_width;
		fb->height = ri->ri_height;
		fb->depth = dc->depth;		/* real depth */
		if (dc->depth > 8)
			fb->cmsize = 0;
		else
			fb->cmsize = 1 << dc->depth;
		break;
	default:
		return -1;
	}

	return 0;
}

paddr_t
grtwo_mmap(void *v, off_t offset, int prot)
{
	/* no directly accessible frame buffer memory */
	return -1;
}

int
grtwo_load_font(void *v, void *emulcookie, struct wsdisplay_font *font)
{
	struct grtwo_devconfig *dc = v;
	struct rasops_info *ri = &dc->dc_ri;

	return rasops_load_font(ri, emulcookie, font);
}

int
grtwo_list_font(void *v, struct wsdisplay_font *font)
{
	struct grtwo_devconfig *dc = v;
	struct rasops_info *ri = &dc->dc_ri;

	return rasops_list_font(ri, font);
}

void
grtwo_burner(void *v, u_int on, u_int flags)
{
	struct grtwo_devconfig *dc = v;

	on = on ? 0xff : 0x00;

	bus_space_write_1(dc->iot, dc->ioh, BT457_R + BT457_ADDR, BT_RMR);
	bus_space_write_1(dc->iot, dc->ioh, BT457_R + BT457_CTRL, on);
	bus_space_write_1(dc->iot, dc->ioh, BT457_G + BT457_ADDR, BT_RMR);
	bus_space_write_1(dc->iot, dc->ioh, BT457_G + BT457_CTRL, on);
	bus_space_write_1(dc->iot, dc->ioh, BT457_B + BT457_ADDR, BT_RMR);
	bus_space_write_1(dc->iot, dc->ioh, BT457_B + BT457_CTRL, on);
}
