/*	$OpenBSD: gbe.c,v 1.13 2010/12/26 15:41:00 miod Exp $ */

/*
 * Copyright (c) 2007, 2008, 2009 Joel Sing <jsing@openbsd.org>
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
 * Graphics Back End (GBE) Framebuffer for SGI O2.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <uvm/uvm_extern.h>

#include <machine/autoconf.h>
#include <machine/bus.h>

#include <mips64/arcbios.h>
#include <mips64/archtype.h>

#include <sgi/localbus/crimebus.h>
#include <sgi/localbus/macebusvar.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/rasops/rasops.h>

#include "gbereg.h"

/* Amount of memory to allocate for framebuffer. */
#define GBE_FB_SIZE	8 * 1 << 20

/* 
 * Colourmap data.
 */
struct  gbe_cmap {
	u_int8_t cm_red[256];
	u_int8_t cm_green[256];
	u_int8_t cm_blue[256];
};

/*
 * Screen data.
 */
struct gbe_screen {
	struct device *sc;		/* Back pointer. */

	struct rasops_info ri;		/* Screen raster display info. */
	struct rasops_info ri_tile;	/* Raster info for rasops tile. */
	struct gbe_cmap cmap;		/* Display colour map. */

	int fb_size;			/* Size of framebuffer memory. */
	int tm_size;			/* Size of tilemap memory. */

	caddr_t tm;			/* Address of tilemap memory. */
	paddr_t tm_phys;		/* Physical address of tilemap. */
	caddr_t fb;			/* Address of framebuffer memory. */
	paddr_t fb_phys;		/* Physical address of framebuffer. */
	caddr_t ro;			/* Address of rasops tile. */
	paddr_t ro_phys;		/* Physical address of rasops tile. */

	int width;			/* Width in pixels. */
	int height;			/* Height in pixels. */
	int depth;			/* Colour depth in bits. */
	int mode;			/* Display mode. */
	int bufmode;			/* Rendering engine buffer mode. */
	int linebytes;			/* Bytes per line. */
	int ro_curpos;			/* Current position in rasops tile. */
};

/*
 * GBE device data.
 */
struct gbe_softc {
	struct device sc_dev;

	bus_space_tag_t iot;
	bus_space_handle_t ioh;		/* GBE registers. */
	bus_space_handle_t re_ioh;	/* Rendering engine registers. */
	bus_dma_tag_t dmat;

	int rev;			/* Hardware revision. */
	int console;			/* Is this the console? */
	int screens;			/* No of screens allocated. */

	struct gbe_screen *curscr;	/* Current screen. */
};

/*
 * Hardware and device related functions.
 */
void	gbe_init_screen(struct gbe_screen *);
void	gbe_enable(struct gbe_softc *);
void	gbe_disable(struct gbe_softc *);
void	gbe_setup(struct gbe_softc *);
void	gbe_setcolour(struct gbe_softc *, u_int, u_int8_t, u_int8_t, u_int8_t);
void	gbe_wait_re_idle(struct gbe_softc *);

/*
 * Colour map handling for indexed modes.
 */
int	gbe_getcmap(struct gbe_cmap *, struct wsdisplay_cmap *);
int	gbe_putcmap(struct gbe_cmap *, struct wsdisplay_cmap *);
void	gbe_loadcmap(struct gbe_screen *, u_int, u_int);

/* 
 * Interfaces for wscons.
 */
int 	gbe_ioctl(void *, u_long, caddr_t, int, struct proc *);
paddr_t gbe_mmap(void *, off_t, int);
int	gbe_alloc_screen(void *, const struct wsscreen_descr *, void **,
	    int *, int *, long *);
void	gbe_free_screen(void *, void *);
int	gbe_show_screen(void *, void *, int, void (*)(void *, int, int),
	    void *);
void	gbe_burner(void *, u_int, u_int);

/*
 * Hardware acceleration for rasops.
 */
void	gbe_rop(struct gbe_softc *, int, int, int, int, int);
void	gbe_copyrect(struct gbe_softc *, int, int, int, int, int, int, int);
void	gbe_fillrect(struct gbe_softc *, int, int, int, int, int);
int	gbe_do_cursor(struct rasops_info *);
int	gbe_putchar(void *, int, int, u_int, long);
int	gbe_copycols(void *, int, int, int, int);
int	gbe_erasecols(void *, int, int, int, long);
int	gbe_copyrows(void *, int, int, int);
int	gbe_eraserows(void *, int, int, long);

static struct gbe_screen gbe_consdata;
static int gbe_console;

struct wsscreen_descr gbe_stdscreen = {
	"std",		/* Screen name. */
};

struct wsdisplay_accessops gbe_accessops = {
	gbe_ioctl,
	gbe_mmap,
	gbe_alloc_screen,
	gbe_free_screen,
	gbe_show_screen,
	NULL,		/* load_font */
	NULL,		/* scrollback */
	NULL,		/* getchar */
	gbe_burner,
	NULL		/* pollc */
};

const struct wsscreen_descr *gbe_scrlist[] = {
	&gbe_stdscreen
};

struct wsscreen_list gbe_screenlist = {
	sizeof(gbe_scrlist) / sizeof(struct wsscreen_descr *), gbe_scrlist
};

int	gbe_match(struct device *, void *, void *);
void	gbe_attach(struct device *, struct device *, void *);

struct cfattach gbe_ca = {
	sizeof (struct gbe_softc), gbe_match, gbe_attach
};

struct cfdriver gbe_cd = {
	NULL, "gbe", DV_DULL
};

int
gbe_match(struct device *parent, void *cf, void *aux)
{
	struct mainbus_attach_args *maa = aux;

	if (strcmp(maa->maa_name, gbe_cd.cd_name) != 0)
		return 0;

	return 1;
}

void
gbe_attach(struct device *parent, struct device *self, void *aux)
{
	struct gbe_softc *gsc = (void*)self;
	struct gbe_screen *screen;
	struct wsemuldisplaydev_attach_args waa;
	bus_dma_segment_t tm_segs[1];
	bus_dma_segment_t fb_segs[1];
	bus_dma_segment_t ro_segs[1];
	bus_dmamap_t tm_dmamap;
	bus_dmamap_t fb_dmamap;
	bus_dmamap_t ro_dmamap;
	int tm_nsegs;
	int fb_nsegs;
	int ro_nsegs;
	uint32_t val;
	long attr;

	printf(": ");

	/* GBE isn't strictly on the crimebus, but use this for now... */
	gsc->iot = &crimebus_tag;
	gsc->dmat = &mace_bus_dma_tag;
	gsc->console = gbe_console;
	gsc->screens = 0;

	if (gsc->console == 1) {

		/*
		 * We've already been setup via gbe_cnattach().
		 */

		gsc->ioh = PHYS_TO_UNCACHED(GBE_BASE);
		gsc->re_ioh = PHYS_TO_UNCACHED(RE_BASE);

		gsc->rev = bus_space_read_4(gsc->iot, gsc->ioh, GBE_CTRL_STAT)
		    & 0xf;

		gsc->curscr = &gbe_consdata;
		gbe_consdata.sc = (void*)self;

		printf("rev %u, %iMB, %dx%d at %d bits\n", gsc->rev,
		    gbe_consdata.fb_size >> 20, gbe_consdata.width,
		    gbe_consdata.height, gbe_consdata.depth);

		shutdownhook_establish((void(*)(void *))gbe_disable, self);

		waa.console = gsc->console;
		waa.scrdata = &gbe_screenlist;
		waa.accessops = &gbe_accessops;
		waa.accesscookie = &gbe_consdata;
		waa.defaultscreens = 0;
		config_found(self, &waa, wsemuldisplaydevprint);

		return;
	}

	/*
	 * Setup screen data.
	 */
	gsc->curscr = malloc(sizeof(struct gbe_screen), M_DEVBUF, M_NOWAIT);
	if (gsc->curscr == NULL) {
		printf("failed to allocate screen memory!\n");
		return;
	}
	gsc->curscr->sc = (void *)gsc;
	screen = gsc->curscr;

	/* 
	 * Setup bus space mappings.
	 */
	if (bus_space_map(gsc->iot, GBE_BASE - CRIMEBUS_BASE, GBE_REG_SIZE, 
	    BUS_SPACE_MAP_LINEAR, &gsc->ioh)) {
		printf("failed to map framebuffer bus space!\n");
		return;
	}

	if (bus_space_map(gsc->iot, RE_BASE - CRIMEBUS_BASE, RE_REG_SIZE, 
	    BUS_SPACE_MAP_LINEAR, &gsc->re_ioh)) {
		printf("failed to map rendering engine bus space!\n");
		goto fail0;
	}

	/* Determine GBE revision. */
	gsc->rev = bus_space_read_4(gsc->iot, gsc->ioh, GBE_CTRL_STAT) & 0xf;

	/* Determine resolution configured by firmware. */
	val = bus_space_read_4(gsc->iot, gsc->ioh, GBE_VT_HCMAP);
	screen->width = (val >> GBE_VT_HCMAP_ON_SHIFT) & 0xfff;
	val = bus_space_read_4(gsc->iot, gsc->ioh, GBE_VT_VCMAP);
	screen->height = (val >> GBE_VT_VCMAP_ON_SHIFT) & 0xfff;

	if (screen->width == 0 || screen->height == 0) {
		printf("device has not been setup by firmware!\n");
		goto fail1;
	}

	/* Setup screen defaults. */
	screen->fb_size = GBE_FB_SIZE;
	screen->tm_size = GBE_TLB_SIZE * sizeof(uint16_t);
	screen->depth = 8;
	screen->linebytes = screen->width * screen->depth / 8;

	/* 
	 * Setup DMA for tilemap.
	 */
	if (bus_dmamap_create(gsc->dmat, screen->tm_size, 1, screen->tm_size,
	    0, BUS_DMA_NOWAIT, &tm_dmamap)) {
		printf("failed to create DMA map for tilemap!\n");
		goto fail1;
	}

	if (bus_dmamem_alloc(gsc->dmat, screen->tm_size, 65536, 0, tm_segs, 1,
	    &tm_nsegs, BUS_DMA_NOWAIT)) {
		printf("failed to allocate DMA memory for tilemap!\n");
		goto fail2;
	}

	if (bus_dmamem_map(gsc->dmat, tm_segs, tm_nsegs, screen->tm_size,
	    &screen->tm, BUS_DMA_COHERENT)) {
		printf("failed to map DMA memory for tilemap!\n");
		goto fail3;
	}

	if (bus_dmamap_load(gsc->dmat, tm_dmamap, screen->tm, screen->tm_size,
	    NULL, BUS_DMA_NOWAIT)){
		printf("failed to load DMA map for tilemap\n");
		goto fail4;
	}

	/* 
	 * Setup DMA for framebuffer.
	 */
	if (bus_dmamap_create(gsc->dmat, screen->fb_size, 1, screen->fb_size,
	    0, BUS_DMA_NOWAIT, &fb_dmamap)) {
		printf("failed to create DMA map for framebuffer!\n");
		goto fail5;
	}

	if (bus_dmamem_alloc(gsc->dmat, screen->fb_size, 65536, 0, fb_segs, 
	    1, &fb_nsegs, BUS_DMA_NOWAIT)) {
		printf("failed to allocate DMA memory for framebuffer!\n");
		goto fail6;
	}

	if (bus_dmamem_map(gsc->dmat, fb_segs, fb_nsegs, screen->fb_size,
	    &screen->fb, BUS_DMA_NOWAIT | BUS_DMA_COHERENT)) {
		printf("failed to map DMA memory for framebuffer!\n");
		goto fail7;
	}

	if (bus_dmamap_load(gsc->dmat, fb_dmamap, screen->fb, screen->fb_size,
	    NULL, BUS_DMA_NOWAIT)) {
		printf("failed to load DMA map for framebuffer\n");
		goto fail8;
	}

	/* 
	 * Setup DMA for rasops tile.
	 */
	if (bus_dmamap_create(gsc->dmat, GBE_TILE_SIZE, 1, GBE_TILE_SIZE,
	    0, BUS_DMA_NOWAIT, &ro_dmamap)) {
		printf("failed to create DMA map for rasops tile!\n");
		goto fail9;
	}

	if (bus_dmamem_alloc(gsc->dmat, GBE_TILE_SIZE, 65536, 0, ro_segs, 
	    1, &ro_nsegs, BUS_DMA_NOWAIT)) {
		printf("failed to allocate DMA memory for rasops tile!\n");
		goto fail10;
	}

	if (bus_dmamem_map(gsc->dmat, ro_segs, ro_nsegs, GBE_TILE_SIZE,
	    &screen->ro, BUS_DMA_NOWAIT | BUS_DMA_COHERENT)) {
		printf("failed to map DMA memory for rasops tile!\n");
		goto fail11;
	}

	if (bus_dmamap_load(gsc->dmat, ro_dmamap, screen->ro, GBE_TILE_SIZE,
	    NULL, BUS_DMA_NOWAIT)) {
		printf("failed to load DMA map for rasops tile\n");
		goto fail12;
	}

	screen->tm_phys = tm_dmamap->dm_segs[0].ds_addr;
	screen->fb_phys = fb_dmamap->dm_segs[0].ds_addr;
	screen->ro_phys = ro_dmamap->dm_segs[0].ds_addr;

	shutdownhook_establish((void(*)(void *))gbe_disable, self);

	gbe_init_screen(screen);
	gbe_disable(gsc);
	gbe_setup(gsc);
	gbe_enable(gsc);

	/* Load colourmap if required. */
	if (screen->depth == 8)
		gbe_loadcmap(screen, 0, 255);

	/* Clear framebuffer. */
	gbe_fillrect(gsc, 0, 0, screen->width, screen->height, 0);

	printf("rev %u, %iMB, %dx%d at %d bits\n", gsc->rev,
	    screen->fb_size >> 20, screen->width, screen->height,
	    screen->depth);

	/*
	 * Attach wsdisplay.
	 */

	/* Attach as console if necessary. */
	if (strncmp(bios_console, "video", 5) == 0) {
		screen->ri.ri_ops.alloc_attr(&screen->ri, 0, 0, 0, &attr);
		wsdisplay_cnattach(&gbe_stdscreen, &screen->ri, 0, 0, attr);
		gsc->console = 1;
	}

	waa.console = gsc->console;
	waa.scrdata = &gbe_screenlist;
	waa.accessops = &gbe_accessops;
	waa.accesscookie = screen;
	waa.defaultscreens = 0;
	config_found(self, &waa, wsemuldisplaydevprint);

	return;

fail12:
	bus_dmamem_unmap(gsc->dmat, screen->ro, GBE_TILE_SIZE);
fail11:
	bus_dmamem_free(gsc->dmat, ro_segs, ro_nsegs);
fail10:
	bus_dmamap_destroy(gsc->dmat, ro_dmamap);
fail9:
	bus_dmamap_unload(gsc->dmat, fb_dmamap);
fail8:
	bus_dmamem_unmap(gsc->dmat, screen->fb, screen->fb_size);
fail7:
	bus_dmamem_free(gsc->dmat, fb_segs, fb_nsegs);
fail6:
	bus_dmamap_destroy(gsc->dmat, fb_dmamap);
fail5:
	bus_dmamap_unload(gsc->dmat, tm_dmamap);
fail4:
	bus_dmamem_unmap(gsc->dmat, screen->tm, screen->tm_size);
fail3:
	bus_dmamem_free(gsc->dmat, tm_segs, tm_nsegs);
fail2:
	bus_dmamap_destroy(gsc->dmat, tm_dmamap);
fail1:
	bus_space_unmap(gsc->iot, gsc->re_ioh, RE_REG_SIZE);
fail0:
	bus_space_unmap(gsc->iot, gsc->ioh, GBE_REG_SIZE);
}

/*
 * GBE hardware specific functions.
 */

void
gbe_init_screen(struct gbe_screen *screen)
{
	uint16_t *tm;
	int i;

	/*
	 * Initialise screen.
	 */
	screen->mode = WSDISPLAYIO_MODE_EMUL;

	/* Initialise rasops. */
	memset(&screen->ri, 0, sizeof(struct rasops_info));

	screen->ri.ri_flg = RI_CENTER;
	screen->ri.ri_depth = screen->depth;
	screen->ri.ri_width = screen->width;
	screen->ri.ri_height = screen->height;
	screen->ri.ri_bits = (void *)screen->fb;
	screen->ri.ri_stride = screen->linebytes;

	if (screen->depth == 32) {
		screen->ri.ri_rpos = 24;
		screen->ri.ri_rnum = 8;
		screen->ri.ri_gpos = 16;
		screen->ri.ri_gnum = 8;
		screen->ri.ri_bpos = 8;
		screen->ri.ri_bnum = 8;
	} else if (screen->depth == 16) {
		screen->ri.ri_rpos = 10;
		screen->ri.ri_rnum = 5;
		screen->ri.ri_gpos = 5;
		screen->ri.ri_gnum = 5;
		screen->ri.ri_bpos = 0;
		screen->ri.ri_bnum = 5;
	}

	rasops_init(&screen->ri, screen->height / 8, screen->width / 8);

	/* Create a rasops instance that can draw into a single tile. */
	memcpy(&screen->ri_tile, &screen->ri, sizeof(struct rasops_info));
	screen->ri_tile.ri_flg = 0;
	screen->ri_tile.ri_width = GBE_TILE_WIDTH >> (screen->depth >> 4);
	screen->ri_tile.ri_height = GBE_TILE_HEIGHT;
	screen->ri_tile.ri_stride = screen->ri_tile.ri_width *
	    screen->depth / 8;
	screen->ri_tile.ri_xorigin = 0;
	screen->ri_tile.ri_yorigin = 0;
	screen->ri_tile.ri_bits = screen->ro;
	screen->ri_tile.ri_origbits = screen->ro;
	screen->ro_curpos = 0;

	screen->ri.ri_hw = screen->sc;

	screen->ri.ri_do_cursor = gbe_do_cursor;
	screen->ri.ri_ops.putchar = gbe_putchar;
	screen->ri.ri_ops.copyrows = gbe_copyrows;
	screen->ri.ri_ops.copycols = gbe_copycols;
	screen->ri.ri_ops.eraserows = gbe_eraserows;
	screen->ri.ri_ops.erasecols = gbe_erasecols;

	gbe_stdscreen.ncols = screen->ri.ri_cols;
	gbe_stdscreen.nrows = screen->ri.ri_rows;
	gbe_stdscreen.textops = &screen->ri.ri_ops;
	gbe_stdscreen.fontwidth = screen->ri.ri_font->fontwidth;
	gbe_stdscreen.fontheight = screen->ri.ri_font->fontheight;
	gbe_stdscreen.capabilities = screen->ri.ri_caps;

	/*
	 * Map framebuffer into tilemap. Each entry in the tilemap is 16 bits 
	 * wide. Each tile is 64KB or 2^16 bits, hence the last 16 bits of the 
	 * address will range from 0x0000 to 0xffff. As a result we simply 
	 * discard the lower 16 bits and store bits 17 through 32 as an entry
	 * in the tilemap.
	 */
	tm = (void *)screen->tm;
	for (i = 0; i < (screen->fb_size >> GBE_TILE_SHIFT) &&
	    i < GBE_TLB_SIZE; i++)
		tm[i] = (screen->fb_phys >> GBE_TILE_SHIFT) + i;
}

void
gbe_enable(struct gbe_softc *gsc)
{
	struct gbe_screen *screen = gsc->curscr;
	uint32_t val;
	int i;

	/* Enable dot clock. */
	val = bus_space_read_4(gsc->iot, gsc->ioh, GBE_DOTCLOCK);
	bus_space_write_4(gsc->iot, gsc->ioh, GBE_DOTCLOCK, 
	    val | GBE_DOTCLOCK_RUN);
	for (i = 0; i < 10000; i++) {
		val = bus_space_read_4(gsc->iot, gsc->ioh, GBE_DOTCLOCK);
		if ((val & GBE_DOTCLOCK_RUN) == GBE_DOTCLOCK_RUN)
			break;
		delay(10);
	}
	if (i == 10000)
		printf("timeout enabling dot clock!\n");

	/* Unfreeze pixel counter. */
	bus_space_write_4(gsc->iot, gsc->ioh, GBE_VT_XY, 0);
	for (i = 0; i < 10000; i++) {
		val = bus_space_read_4(gsc->iot, gsc->ioh, GBE_VT_XY);
		if ((val & GBE_VT_XY_FREEZE) == 0)
			break;
		delay(10);
	}
	if (i == 10000)
		printf("timeout unfreezing pixel counter!\n");

	/* Provide GBE with address of tilemap and enable DMA. */
	bus_space_write_4(gsc->iot, gsc->ioh, GBE_FB_CTRL, 
	    ((screen->tm_phys >> 9) << 
	    GBE_FB_CTRL_TILE_PTR_SHIFT) | GBE_FB_CTRL_DMA_ENABLE);
}

void
gbe_disable(struct gbe_softc *gsc)
{
	uint32_t val;
	int i;

	/* Nothing to do if the pixel counter is frozen! */
	val = bus_space_read_4(gsc->iot, gsc->ioh, GBE_VT_XY);
	if ((val & GBE_VT_XY_FREEZE) == GBE_VT_XY_FREEZE)
		return;

	val = bus_space_read_4(gsc->iot, gsc->ioh, GBE_DOTCLOCK);
	if ((val & GBE_DOTCLOCK_RUN) == 0) 
		return;

	/* Disable overlay and turn off hardware cursor. */
	bus_space_write_4(gsc->iot, gsc->ioh, GBE_OVERLAY_TILE, 0);
	bus_space_write_4(gsc->iot, gsc->ioh, GBE_CURSOR_CTRL, 0);
	bus_space_write_4(gsc->iot, gsc->ioh, GBE_DID_CTRL, 0);

	/* Disable DMA. */
	val = bus_space_read_4(gsc->iot, gsc->ioh, GBE_OVERLAY_CTRL);
	bus_space_write_4(gsc->iot, gsc->ioh, GBE_OVERLAY_CTRL, 
	    val & ~GBE_OVERLAY_CTRL_DMA_ENABLE);
	val = bus_space_read_4(gsc->iot, gsc->ioh, GBE_FB_CTRL);
	bus_space_write_4(gsc->iot, gsc->ioh, GBE_FB_CTRL, 
	    val & ~GBE_FB_CTRL_DMA_ENABLE);
	bus_space_write_4(gsc->iot, gsc->ioh, GBE_DID_CTRL, 0);
	for (i = 0; i < 100000; i++) {
		if ((bus_space_read_4(gsc->iot, gsc->ioh, GBE_OVERLAY_HW_CTRL)
		    & GBE_OVERLAY_CTRL_DMA_ENABLE) == 0 &&
		    (bus_space_read_4(gsc->iot, gsc->ioh, GBE_FB_HW_CTRL)
		    & GBE_FB_CTRL_DMA_ENABLE) == 0 &&
		    bus_space_read_4(gsc->iot, gsc->ioh, GBE_DID_HW_CTRL) == 0)
			break;
		delay(10);
	}
	if (i == 100000)
		printf("timeout disabling DMA!\n");

	/* Wait for the end of pixel refresh. */
	val = bus_space_read_4(gsc->iot, gsc->ioh, GBE_VT_VPIX)
	    & GBE_VT_VPIX_OFF_MASK;
	for (i = 0; i < 100000; i++) {
		if (((bus_space_read_4(gsc->iot, gsc->ioh, GBE_VT_XY)
		    & GBE_VT_XY_Y_MASK) >> GBE_VT_XY_Y_SHIFT) < val)
			break;
		delay(1);
	}
	if (i == 100000)
		printf("timeout waiting for pixel refresh!\n");
	for (i = 0; i < 100000; i++) {
		if (((bus_space_read_4(gsc->iot, gsc->ioh, GBE_VT_XY)
		    & GBE_VT_XY_Y_MASK) >> GBE_VT_XY_Y_SHIFT) > val)
			break;
		delay(1);
	}
	if (i == 100000)
		printf("timeout waiting for pixel refresh!\n");

	/* Freeze pixel counter. */
	bus_space_write_4(gsc->iot, gsc->ioh, GBE_VT_XY, GBE_VT_XY_FREEZE);
	for (i = 0; i < 100000; i++) {
		val = bus_space_read_4(gsc->iot, gsc->ioh, GBE_VT_XY);
		if ((val & GBE_VT_XY_FREEZE) == GBE_VT_XY_FREEZE)
			break;
		delay(10);
	}
	if (i == 100000)
		printf("timeout freezing pixel counter!\n");

	/* Disable dot clock. */
	val = bus_space_read_4(gsc->iot, gsc->ioh, GBE_DOTCLOCK);
	bus_space_write_4(gsc->iot, gsc->ioh, GBE_DOTCLOCK, 
	    val & ~GBE_DOTCLOCK_RUN);
	for (i = 0; i < 100000; i++) {
		val = bus_space_read_4(gsc->iot, gsc->ioh, GBE_DOTCLOCK);
		if ((val & GBE_DOTCLOCK_RUN) == 0)
			break;
		delay(10);
	}
	if (i == 100000)
		printf("timeout disabling dot clock!\n");

	/* Reset DMA fifo. */
	val = bus_space_read_4(gsc->iot, gsc->ioh, GBE_FB_SIZE_TILE);
	val &= ~(1 << GBE_FB_SIZE_TILE_FIFO_RESET_SHIFT);
	bus_space_write_4(gsc->iot, gsc->ioh, GBE_FB_SIZE_TILE, 
	    val | (1 << GBE_FB_SIZE_TILE_FIFO_RESET_SHIFT));
	bus_space_write_4(gsc->iot, gsc->ioh, GBE_FB_SIZE_TILE, val);
}

void
gbe_setup(struct gbe_softc *gsc)
{
	struct gbe_screen *screen = gsc->curscr;
	int i, t, cmode, tile_width, tiles_x, tiles_y;
	u_char *colour;
	uint16_t *tm;
	uint32_t val;
	uint64_t reg;

	/*
	 * Setup framebuffer.
	 */
	switch (screen->depth) {
	case 32:
		cmode = GBE_CMODE_RGB8;
		screen->bufmode = COLOUR_DEPTH_32 << BUFMODE_BUFDEPTH_SHIFT |
		    PIXEL_TYPE_RGB << BUFMODE_PIXTYPE_SHIFT |
		    COLOUR_DEPTH_32 << BUFMODE_PIXDEPTH_SHIFT;
		break;
	case 16:
		cmode = GBE_CMODE_ARGB5;
		screen->bufmode = COLOUR_DEPTH_16 << BUFMODE_BUFDEPTH_SHIFT |
		    PIXEL_TYPE_RGBA << BUFMODE_PIXTYPE_SHIFT |
		    COLOUR_DEPTH_16 << BUFMODE_PIXDEPTH_SHIFT;
		break;
	case 8:
	default:
		cmode = GBE_CMODE_I8;
		screen->bufmode = COLOUR_DEPTH_8 << BUFMODE_BUFDEPTH_SHIFT |
		    PIXEL_TYPE_CI << BUFMODE_PIXTYPE_SHIFT |
		    COLOUR_DEPTH_8 << BUFMODE_PIXDEPTH_SHIFT;
		break;
	}

	/* Calculate tile width in bytes and screen size in tiles. */
	tile_width = GBE_TILE_WIDTH >> (screen->depth >> 4);
	tiles_x = (screen->width + tile_width - 1) >>
	    (GBE_TILE_WIDTH_SHIFT - (screen->depth >> 4));
	tiles_y = (screen->height + GBE_TILE_HEIGHT - 1) >>
	    GBE_TILE_HEIGHT_SHIFT;

	if (screen->mode != WSDISPLAYIO_MODE_EMUL) {

		/*
		 * Setup the framebuffer in "linear" mode. We trick the
		 * framebuffer into linear mode by telling it that it is one
		 * tile wide and specifying an adjusted framebuffer height.
		 */ 

		bus_space_write_4(gsc->iot, gsc->ioh, GBE_FB_SIZE_TILE,
		    ((screen->depth >> 4) << GBE_FB_SIZE_TILE_DEPTH_SHIFT) |
		    (1 << GBE_FB_SIZE_TILE_WIDTH_SHIFT));

		bus_space_write_4(gsc->iot, gsc->ioh, GBE_FB_SIZE_PIXEL, 
		    (screen->width * screen->height / tile_width) <<
		        GBE_FB_SIZE_PIXEL_HEIGHT_SHIFT);

	} else {

		/*
		 * Setup the framebuffer in tiled mode. Provide the tile
		 * colour depth, screen width in whole and partial tiles,
		 * and the framebuffer height in pixels.
		 */

		bus_space_write_4(gsc->iot, gsc->ioh, GBE_FB_SIZE_TILE,
		    ((screen->depth >> 4) << GBE_FB_SIZE_TILE_DEPTH_SHIFT) |
		    ((screen->width / tile_width) <<
		        GBE_FB_SIZE_TILE_WIDTH_SHIFT) |
		    ((screen->width % tile_width != 0) ?
		        (screen->height / GBE_TILE_HEIGHT) : 0));

		bus_space_write_4(gsc->iot, gsc->ioh, GBE_FB_SIZE_PIXEL, 
		    screen->height << GBE_FB_SIZE_PIXEL_HEIGHT_SHIFT);

	}

	/* Set colour mode registers. */
	val = (cmode << GBE_WID_MODE_SHIFT) | GBE_BMODE_BOTH;
	for (i = 0; i < (32 * 4); i += 4)
		bus_space_write_4(gsc->iot, gsc->ioh, GBE_MODE + i, val);

	/*
	 * Initialise colourmap if required.
	 */
	if (screen->depth == 8) {
		for (i = 0; i < 16; i++) {
			colour = (u_char *)&rasops_cmap[i * 3];
			screen->cmap.cm_red[i] = colour[0];
			screen->cmap.cm_green[i] = colour[1];
			screen->cmap.cm_blue[i] = colour[2];
		}
		for (i = 240; i < 256; i++) {
			colour = (u_char *)&rasops_cmap[i * 3];
			screen->cmap.cm_red[i] = colour[0];
			screen->cmap.cm_green[i] = colour[1];
			screen->cmap.cm_blue[i] = colour[2];
		}
	}

	/*
	 * Setup an alpha ramp.
	 */
	for (i = 0; i < GBE_GMAP_ENTRIES; i++)
		bus_space_write_4(gsc->iot, gsc->ioh,
		    GBE_GMAP + i * sizeof(u_int32_t),
		    (i << 24) | (i << 16) | (i << 8));

	/*
	 * Initialise the rendering engine.
	 */
	val = screen->mode | BUF_TYPE_TLB_A << BUFMODE_BUFTYPE_SHIFT;
	bus_space_write_4(gsc->iot, gsc->re_ioh, RE_PP_BUFMODE_SRC, val);
	bus_space_write_4(gsc->iot, gsc->re_ioh, RE_PP_BUFMODE_DST, val);
	bus_space_write_4(gsc->iot, gsc->re_ioh, RE_PP_CLIPMODE, 0);
	bus_space_write_4(gsc->iot, gsc->re_ioh, RE_PP_COLOUR_MASK, 0xffffffff);
	bus_space_write_4(gsc->iot, gsc->re_ioh, RE_PP_PIXEL_XFER_X_STEP, 1);
	bus_space_write_4(gsc->iot, gsc->re_ioh, RE_PP_PIXEL_XFER_Y_STEP, 1);
	bus_space_write_4(gsc->iot, gsc->re_ioh, RE_PP_WINOFFSET_DST, 0);
	bus_space_write_4(gsc->iot, gsc->re_ioh, RE_PP_WINOFFSET_SRC, 0);

	/*
	 * Load framebuffer tiles into TLB A. Each TLB consists of a 16x16
	 * tile array representing 2048x2048 pixels. Each entry in the TLB
	 * consists of four 16-bit entries which represent bits 17:32 of the
	 * 64KB tile address. As a result, we can make use of the tilemap
	 * which already stores tile entries in the same format.
	 */
	tm = (void *)screen->tm;
	for (i = 0, t = 0; i < GBE_TLB_SIZE; i++) {
		reg <<= 16;
		if (i % 16 < tiles_x)
			reg |= (tm[t++] | 0x8000);
		if (i % 4 == 3)
			bus_space_write_8(gsc->iot, gsc->re_ioh,
			    RE_TLB_A + (i >> 2) * 8, reg);
	}

	/* Load single tile into TLB B for rasops. */
	bus_space_write_8(gsc->iot, gsc->re_ioh,
	    RE_TLB_B, (screen->ro_phys >> 16 | 0x8000) << 48);
}

void
gbe_wait_re_idle(struct gbe_softc *gsc)
{
	int i;

	/* Wait until rendering engine is idle. */
	for (i = 0; i < 100000; i++) {
		if (bus_space_read_4(gsc->iot, gsc->re_ioh, RE_PP_STATUS) &
		    RE_PP_STATUS_IDLE)
			break; 
		delay(1);
	}
	if (i == 100000)
		printf("%s: rendering engine did not become idle!\n",
		    gsc->sc_dev.dv_xname);
}

/*
 * Interfaces for wscons.
 */

int
gbe_ioctl(void *v, u_long cmd, caddr_t data, int flags, struct proc *p)
{
	struct gbe_screen *screen = (struct gbe_screen *)v;
	int rc, mode;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_GBE;
		break;

	case WSDISPLAYIO_GINFO:
	{
		struct wsdisplay_fbinfo *fb = (struct wsdisplay_fbinfo *)data;

		fb->height = screen->height;
		fb->width = screen->width;
		fb->depth = screen->depth;
		fb->cmsize = screen->depth == 8 ? 256 : 0;
	}
		break;

	case WSDISPLAYIO_LINEBYTES:
		*(u_int *)data = screen->linebytes;
		break;

	case WSDISPLAYIO_GETCMAP:
		if (screen->depth == 8) {
			struct wsdisplay_cmap *cm =
			    (struct wsdisplay_cmap *)data;

			rc = gbe_getcmap(&screen->cmap, cm);
			if (rc != 0)
				return (rc);
		}
		break;

	case WSDISPLAYIO_PUTCMAP:
		if (screen->depth == 8) {
			struct wsdisplay_cmap *cm =
			    (struct wsdisplay_cmap *)data;

			rc = gbe_putcmap(&screen->cmap, cm);
			if (rc != 0)
				return (rc);
			gbe_loadcmap(screen, cm->index, cm->index + cm->count);
		}
		break;

	case WSDISPLAYIO_GMODE:
		*(u_int *)data = screen->mode;
		break;

	case WSDISPLAYIO_SMODE:
		mode = *(u_int *)data;
		if (mode == WSDISPLAYIO_MODE_EMUL ||
		    mode == WSDISPLAYIO_MODE_MAPPED ||
		    mode == WSDISPLAYIO_MODE_DUMBFB) {

			screen->mode = mode;

			gbe_disable((struct gbe_softc *)screen->sc);
			gbe_setup((struct gbe_softc *)screen->sc);
			gbe_enable((struct gbe_softc *)screen->sc);

			/* Clear framebuffer if entering emulated mode. */
			if (screen->mode == WSDISPLAYIO_MODE_EMUL)
				gbe_fillrect((struct gbe_softc *)screen->sc,
				    0, 0, screen->width, screen->height, 0);
		}
		break;

	case WSDISPLAYIO_GVIDEO:
	case WSDISPLAYIO_SVIDEO:
		/* Handled by the upper layer. */
		break;

	default:
		return (-1);
	}

	return (0);
}

paddr_t
gbe_mmap(void *v, off_t offset, int protection)
{
	struct gbe_screen *screen = (void *)v;
	paddr_t pa;

	if (offset >= 0 && offset < screen->fb_size)
		pa = screen->fb_phys + offset;
	else
		pa = -1;

	return (pa);
}

int
gbe_alloc_screen(void *v, const struct wsscreen_descr *type, void **cookiep,
    int *curxp, int *curyp, long *attrp)
{
	struct gbe_screen *screen = (struct gbe_screen *)v;
	struct gbe_softc *gsc = (struct gbe_softc *)screen->sc;

	/* We do not allow multiple consoles at the moment. */
	if (gsc->screens > 0)
		return (ENOMEM);

	gsc->screens++;

	/* Return rasops_info via cookie. */
	*cookiep = &screen->ri;

	/* Move cursor to top left of screen. */
	*curxp = 0;
	*curyp = 0;

	/* Correct screen attributes. */
	screen->ri.ri_ops.alloc_attr(&screen->ri, 0, 0, 0, attrp);

	return (0);
}

void
gbe_free_screen(void *v, void *cookie)
{
	/* We do not allow multiple consoles at the moment. */
}

int
gbe_show_screen(void *v, void *cookie, int waitok, void (*cb)(void *, int, int),
    void *cbarg)
{
	/* We do not allow multiple consoles at the moment. */
	return (0);
}

void
gbe_burner(void *v, u_int on, u_int flags)
{
}

/*
 * Colour map handling for indexed modes.
 */

void
gbe_setcolour(struct gbe_softc *gsc, u_int index, u_int8_t r, u_int8_t g,
    u_int8_t b)
{
	int i;

	/* Wait until the colourmap FIFO has free space. */
	for (i = 0; i < 10000; i++) {
		if (bus_space_read_4(gsc->iot, gsc->ioh, GBE_CMAP_FIFO)
		    < GBE_CMAP_FIFO_ENTRIES) 
			break;
		delay(10);
	}
	if (i == 10000)
		printf("colourmap FIFO has no free space!\n");

	bus_space_write_4(gsc->iot, gsc->ioh,
	    GBE_CMAP + index * sizeof(u_int32_t),
	    ((u_int)r << 24) | ((u_int)g << 16) | ((u_int)b << 8));
}

int
gbe_getcmap(struct gbe_cmap *cm, struct wsdisplay_cmap *rcm)
{
	u_int index = rcm->index, count = rcm->count;
	int rc;

	if (index >= 256 || count > 256 - index)
		return (EINVAL);

	if ((rc = copyout(&cm->cm_red[index], rcm->red, count)) != 0)
		return (rc);
	if ((rc = copyout(&cm->cm_green[index], rcm->green, count)) != 0)
		return (rc);
	if ((rc = copyout(&cm->cm_blue[index], rcm->blue, count)) != 0)
		return (rc);

	return (0);
}

int
gbe_putcmap(struct gbe_cmap *cm, struct wsdisplay_cmap *rcm)
{
	u_int index = rcm->index, count = rcm->count;
	int rc;

	if (index >= 256 || count > 256 - index)
		return (EINVAL);

	if ((rc = copyin(rcm->red, &cm->cm_red[index], count)) != 0)
		return (rc);
	if ((rc = copyin(rcm->green, &cm->cm_green[index], count)) != 0)
		return (rc);
	if ((rc = copyin(rcm->blue, &cm->cm_blue[index], count)) != 0)
		return (rc);

	return (0);
}

void
gbe_loadcmap(struct gbe_screen *screen, u_int start, u_int end)
{
	struct gbe_softc *gsc = (void *)screen->sc;
	struct gbe_cmap *cm = &screen->cmap;

	for (; start <= end; start++)
		gbe_setcolour(gsc, start,
		    cm->cm_red[start], cm->cm_green[start], cm->cm_blue[start]);
}

/*
 * Hardware accelerated functions for rasops.
 */

void
gbe_rop(struct gbe_softc *gsc, int x, int y, int w, int h, int op)
{

	gbe_wait_re_idle(gsc);

	bus_space_write_4(gsc->iot, gsc->re_ioh, RE_PP_PRIMITIVE,
	    PRIMITIVE_RECTANGLE | PRIMITIVE_LRTB);
	bus_space_write_4(gsc->iot, gsc->re_ioh, RE_PP_DRAWMODE,
	    DRAWMODE_BITMASK | DRAWMODE_BYTEMASK | DRAWMODE_PIXEL_XFER |
	    DRAWMODE_LOGIC_OP);
	bus_space_write_4(gsc->iot, gsc->re_ioh, RE_PP_LOGIC_OP, op);
	bus_space_write_4(gsc->iot, gsc->re_ioh, RE_PP_PIXEL_XFER_SRC,
	    (x << 16) | (y & 0xffff));
	bus_space_write_4(gsc->iot, gsc->re_ioh, RE_PP_VERTEX_X_0,
	    (x << 16) | (y & 0xffff));
	bus_space_write_4(gsc->iot, gsc->re_ioh, RE_PP_VERTEX_X_1 | RE_START,
	    ((x + w - 1) << 16) | ((y + h - 1) & 0xffff));
}

void
gbe_copyrect(struct gbe_softc *gsc, int src, int sx, int sy, int dx, int dy,
    int w, int h)
{
	int direction, x0, y0, x1, y1;

	if (sx >= dx && sy >= dy) {
		direction = PRIMITIVE_LRTB;
		x0 = dx;
		y0 = dy;
		x1 = dx + w - 1;
		y1 = dy + h - 1;
	} else if (sx >= dx && sy < dy) {
		direction = PRIMITIVE_LRBT;
		sy = sy + h - 1;
		x0 = dx;
		y0 = dy + h - 1;
		x1 = dx + w - 1;
		y1 = dy;
	} else if (sx < dx && sy >= dy) {
		direction = PRIMITIVE_RLTB;
		sx = sx + w - 1;
		x0 = dx + w - 1;
		y0 = dy;
		x1 = dx;
		y1 = dy + h - 1;
	} else if (sx < dx && sy < dy) {
		direction = PRIMITIVE_RLBT;
		sy = sy + h - 1;
		sx = sx + w - 1;
		x0 = dx + w - 1;
		y0 = dy + h - 1;
		x1 = dx;
		y1 = dy;
	}

	gbe_wait_re_idle(gsc);

	if (src != BUF_TYPE_TLB_A) 
		bus_space_write_4(gsc->iot, gsc->re_ioh, RE_PP_BUFMODE_SRC,
		    gsc->curscr->bufmode | (src << BUFMODE_BUFTYPE_SHIFT));

	bus_space_write_4(gsc->iot, gsc->re_ioh, RE_PP_PRIMITIVE,
	    PRIMITIVE_RECTANGLE | direction);
	bus_space_write_4(gsc->iot, gsc->re_ioh, RE_PP_DRAWMODE,
	    DRAWMODE_BITMASK | DRAWMODE_BYTEMASK | DRAWMODE_PIXEL_XFER);
	bus_space_write_4(gsc->iot, gsc->re_ioh, RE_PP_PIXEL_XFER_SRC,
	    (sx << 16) | (sy & 0xffff));
	bus_space_write_4(gsc->iot, gsc->re_ioh, RE_PP_VERTEX_X_0,
	    (x0 << 16) | (y0 & 0xffff));
	bus_space_write_4(gsc->iot, gsc->re_ioh, RE_PP_VERTEX_X_1 | RE_START,
	    (x1 << 16) | (y1 & 0xffff));

	if (src != BUF_TYPE_TLB_A) {
		gbe_wait_re_idle(gsc);
		bus_space_write_4(gsc->iot, gsc->re_ioh, RE_PP_BUFMODE_SRC,
		    gsc->curscr->bufmode |
		    (BUF_TYPE_TLB_A << BUFMODE_BUFTYPE_SHIFT));
	}
}

void
gbe_fillrect(struct gbe_softc *gsc, int x, int y, int w, int h, int bg)
{

	gbe_wait_re_idle(gsc);

	bus_space_write_4(gsc->iot, gsc->re_ioh, RE_PP_PRIMITIVE,
	    PRIMITIVE_RECTANGLE | PRIMITIVE_LRTB);
	bus_space_write_4(gsc->iot, gsc->re_ioh, RE_PP_DRAWMODE,
	    DRAWMODE_BITMASK | DRAWMODE_BYTEMASK);
	bus_space_write_4(gsc->iot, gsc->re_ioh, RE_PP_SHADE_FG_COLOUR, bg);
	bus_space_write_4(gsc->iot, gsc->re_ioh, RE_PP_VERTEX_X_0,
	    (x << 16) | (y & 0xffff));
	bus_space_write_4(gsc->iot, gsc->re_ioh, RE_PP_VERTEX_X_1 | RE_START,
	    ((x + w - 1) << 16) | ((y + h - 1) & 0xffff));
}

int
gbe_do_cursor(struct rasops_info *ri)
{
	struct gbe_softc *sc = ri->ri_hw;
	int y, x, w, h;

	w = ri->ri_font->fontwidth;
	h = ri->ri_font->fontheight;
	x = ri->ri_xorigin + ri->ri_ccol * w;
	y = ri->ri_yorigin + ri->ri_crow * h;

	gbe_rop(sc, x, y, w, h, LOGIC_OP_XOR);

	return 0;
}

int
gbe_putchar(void *cookie, int row, int col, u_int uc, long attr)
{
	struct rasops_info *ri = cookie;
	struct gbe_softc *gsc = ri->ri_hw;
	struct gbe_screen *screen = gsc->curscr;
	struct rasops_info *ri_tile = &screen->ri_tile;
	int x, y, w, h;

	w = ri->ri_font->fontwidth;
	h = ri->ri_font->fontheight;
	x = ri->ri_xorigin + col * w;
	y = ri->ri_yorigin + row * h;

	ri_tile->ri_ops.putchar(ri_tile, 0, screen->ro_curpos, uc, attr);

	gbe_copyrect(gsc, BUF_TYPE_TLB_B, screen->ro_curpos * w, 0, x, y, w, h);

	screen->ro_curpos++;
	if ((screen->ro_curpos + 1) * w > screen->ri_tile.ri_width)
		screen->ro_curpos = 0;

	return 0;
}

int
gbe_copycols(void *cookie, int row, int src, int dst, int num)
{
	struct rasops_info *ri = cookie;
	struct gbe_softc *sc = ri->ri_hw;

	num *= ri->ri_font->fontwidth;
	src *= ri->ri_font->fontwidth;
	dst *= ri->ri_font->fontwidth;
	row *= ri->ri_font->fontheight;

	gbe_copyrect(sc, BUF_TYPE_TLB_A, ri->ri_xorigin + src,
	    ri->ri_yorigin + row, ri->ri_xorigin + dst, ri->ri_yorigin + row,
	    num, ri->ri_font->fontheight);

	return 0;
}

int
gbe_erasecols(void *cookie, int row, int col, int num, long attr)
{
	struct rasops_info *ri = cookie;
	struct gbe_softc *sc = ri->ri_hw;
	int bg, fg;

	ri->ri_ops.unpack_attr(cookie, attr, &fg, &bg, NULL);

	row *= ri->ri_font->fontheight;
	col *= ri->ri_font->fontwidth;
	num *= ri->ri_font->fontwidth;

	gbe_fillrect(sc, ri->ri_xorigin + col, ri->ri_yorigin + row,
	    num, ri->ri_font->fontheight, ri->ri_devcmap[bg]);

	return 0;
}

int
gbe_copyrows(void *cookie, int src, int dst, int num)
{
	struct rasops_info *ri = cookie;
	struct gbe_softc *sc = ri->ri_hw;
	
	num *= ri->ri_font->fontheight;
	src *= ri->ri_font->fontheight;
	dst *= ri->ri_font->fontheight;

	gbe_copyrect(sc, BUF_TYPE_TLB_A, ri->ri_xorigin, ri->ri_yorigin + src,
	    ri->ri_xorigin, ri->ri_yorigin + dst, ri->ri_emuwidth, num);

	return 0;
}

int
gbe_eraserows(void *cookie, int row, int num, long attr)
{
	struct rasops_info *ri = cookie;
	struct gbe_softc *sc = ri->ri_hw;
	int x, y, w, bg, fg;

	ri->ri_ops.unpack_attr(cookie, attr, &fg, &bg, NULL);

	if ((num == ri->ri_rows) && ISSET(ri->ri_flg, RI_FULLCLEAR)) {
		num = ri->ri_height;
		x = y = 0;
		w = ri->ri_width;
	} else {
		num *= ri->ri_font->fontheight;
		x = ri->ri_xorigin;
		y = ri->ri_yorigin + row * ri->ri_font->fontheight;
		w = ri->ri_emuwidth;
	}

	gbe_fillrect(sc, x, y, w, num, ri->ri_devcmap[bg]);

	return 0;
}

/*
 * Console functions for early display.
 */

int
gbe_cnprobe(bus_space_tag_t iot, bus_addr_t addr)
{
	bus_space_handle_t ioh;
	int val, width, height;

	/* Setup bus space mapping. */
	ioh = PHYS_TO_UNCACHED(addr);

	/* Determine resolution configured by firmware. */
	val = bus_space_read_4(iot, ioh, GBE_VT_HCMAP);
	width = (val >> GBE_VT_HCMAP_ON_SHIFT) & 0xfff;
	val = bus_space_read_4(iot, ioh, GBE_VT_VCMAP);
	height = (val >> GBE_VT_VCMAP_ON_SHIFT) & 0xfff;

	/* Ensure that the firmware has setup the device. */
	if (width != 0 && height != 0)
		return (1);
	else
		return (0);
}

int
gbe_cnattach(bus_space_tag_t iot, bus_addr_t addr)
{
	struct gbe_softc *gsc;
	uint32_t val;
	paddr_t pa;
	vaddr_t va;
	long attr;

	/*
	 * Setup GBE for use as early console.
	 */
	va = pmap_steal_memory(sizeof(struct gbe_softc), NULL, NULL);
	gsc = (struct gbe_softc *)va;
	gsc->curscr = &gbe_consdata;
	gbe_consdata.sc = (struct device *)gsc;
	
	/* Setup bus space mapping. */
	gsc->iot = iot;
	gsc->ioh = PHYS_TO_UNCACHED(addr);
	gsc->re_ioh = PHYS_TO_UNCACHED(RE_BASE);

	/* Determine GBE revision. */
	gsc->rev = bus_space_read_4(gsc->iot, gsc->ioh, GBE_CTRL_STAT) & 0xf;

	/* Determine resolution configured by firmware. */
	val = bus_space_read_4(gsc->iot, gsc->ioh, GBE_VT_HCMAP);
	gbe_consdata.width = (val >> GBE_VT_HCMAP_ON_SHIFT) & 0xfff;
	val = bus_space_read_4(gsc->iot, gsc->ioh, GBE_VT_VCMAP);
	gbe_consdata.height = (val >> GBE_VT_VCMAP_ON_SHIFT) & 0xfff;

	/* Ensure that the firmware has setup the device. */
	if (gbe_consdata.width == 0 || gbe_consdata.height == 0)
		return (ENXIO);

	/* Setup screen defaults. */
	gbe_consdata.fb_size = GBE_FB_SIZE;
	gbe_consdata.tm_size = GBE_TLB_SIZE * sizeof(uint16_t);
	gbe_consdata.depth = 8;
	gbe_consdata.linebytes = gbe_consdata.width * gbe_consdata.depth / 8;

	/* 
	 * Steal memory for tilemap - 64KB aligned and coherent.
	 */
	va = pmap_steal_memory(gbe_consdata.tm_size + 65536, NULL, NULL);
	pmap_extract(pmap_kernel(), va, &pa);
	gbe_consdata.tm_phys = ((pa >> 16) + 1) << 16;
	gbe_consdata.tm = (caddr_t)PHYS_TO_UNCACHED(gbe_consdata.tm_phys);
	
	/* 
	 * Steal memory for framebuffer - 64KB aligned and coherent.
	 */
	va = pmap_steal_memory(gbe_consdata.fb_size + 65536, NULL, NULL);
	pmap_extract(pmap_kernel(), va, &pa);
	gbe_consdata.fb_phys = ((pa >> 16) + 1) << 16;
	gbe_consdata.fb = (caddr_t)PHYS_TO_UNCACHED(gbe_consdata.fb_phys);

	/* 
	 * Steal memory for rasops tile - 64KB aligned and coherent.
	 */
	va = pmap_steal_memory(GBE_TILE_SIZE + 65536, NULL, NULL);
	pmap_extract(pmap_kernel(), va, &pa);
	gbe_consdata.ro_phys = ((pa >> 16) + 1) << 16;
	gbe_consdata.ro = (caddr_t)PHYS_TO_UNCACHED(gbe_consdata.ro_phys);

	/*
	 * Setup GBE hardware.
	 */
	gbe_init_screen(&gbe_consdata);
	gbe_disable(gsc);
	gbe_setup(gsc);
	gbe_enable(gsc);

	/* Load colourmap if required. */
	if (gbe_consdata.depth == 8)
		gbe_loadcmap(&gbe_consdata, 0, 255);

	/* Clear framebuffer. */
	gbe_fillrect(gsc, 0, 0, gbe_consdata.width, gbe_consdata.height, 0);

	/*
	 * Attach wsdisplay.
	 */
	gbe_consdata.ri.ri_ops.alloc_attr(&gbe_consdata.ri, 0, 0, 0, &attr);
	wsdisplay_cnattach(&gbe_stdscreen, &gbe_consdata.ri, 0, 0, attr);
	gbe_console = 1;

	return (0);
}
