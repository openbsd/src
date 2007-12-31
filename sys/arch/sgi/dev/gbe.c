/*	$OpenBSD: gbe.c,v 1.4 2007/12/31 12:46:14 jsing Exp $ */

/*
 * Copyright (c) 2007, Joel Sing <jsing@openbsd.org>
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
 * Graphics Back End (GBE) Framebuffer for SGI O2
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
#include <sgi/localbus/macebus.h>

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

	struct rasops_info ri;		/* Raster display info. */
	struct gbe_cmap cmap;		/* Display colour map. */

	int fb_size;			/* Size of framebuffer memory. */
	int tm_size;			/* Size of tilemap memory. */

	caddr_t tm;			/* Address of tilemap memory. */
	paddr_t tm_phys;		/* Physical address of tilemap. */
	caddr_t fb;			/* Address of framebuffer memory. */
	paddr_t fb_phys;		/* Physical address of framebuffer. */

	int width;			/* Width in pixels. */
	int height;			/* Height in pixels. */
	int depth;			/* Colour depth in bits. */
	int linebytes;			/* Bytes per line. */
};

/*
 * GBE device data.
 */
struct gbe_softc {
	struct device sc_dev;

	bus_space_tag_t iot;
	bus_space_handle_t ioh;
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
	/* GBE framebuffer only on SGI O2 (for now anyway). */
	if (sys_config.system_type == SGI_O2)
		return 1;

	return 0;
}

void
gbe_attach(struct device *parent, struct device *self, void *aux)
{
	struct gbe_softc *gsc = (void*)self;
	struct gbe_screen *screen;
	struct wsemuldisplaydev_attach_args waa;
	bus_dma_segment_t tm_segs[1];
	bus_dma_segment_t fb_segs[1];
	bus_dmamap_t tm_dmamap;
	bus_dmamap_t fb_dmamap;
	int tm_nsegs;
	int fb_nsegs;
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

		gsc->ioh = PHYS_TO_XKPHYS(GBE_BASE, CCA_NC);
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
	 * Setup bus space mapping.
	 */
	if (bus_space_map(gsc->iot, GBE_BASE - CRIMEBUS_BASE, GBE_REG_SIZE, 
	    BUS_SPACE_MAP_LINEAR, &gsc->ioh)) {
		printf("failed to map bus space!\n");
		return;
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
		goto fail0;
	}

	/* Setup screen defaults. */
	screen->fb_size = GBE_FB_SIZE;
	screen->tm_size = GBE_TLB_SIZE * sizeof(uint16_t);
	screen->depth = 8;
	screen->linebytes = screen->width * screen->depth / 8;

	/* 
	 * Setup DMA for tile map.
	 */
	if (bus_dmamap_create(gsc->dmat, screen->tm_size, 1, screen->tm_size,
	    0, BUS_DMA_NOWAIT, &tm_dmamap)) {
		printf("failed to create DMA map for tile map!\n");
		goto fail0;
	}

	if (bus_dmamem_alloc(gsc->dmat, screen->tm_size, 65536, 0, tm_segs, 1,
	    &tm_nsegs, BUS_DMA_NOWAIT)) {
		printf("failed to allocate DMA memory for tile map!\n");
		goto fail1;
	}

	if (bus_dmamem_map(gsc->dmat, tm_segs, tm_nsegs, screen->tm_size,
	    &screen->tm, BUS_DMA_COHERENT)) {
		printf("failed to map DMA memory for tile map!\n");
		goto fail2;
	}

	if (bus_dmamap_load(gsc->dmat, tm_dmamap, screen->tm, screen->tm_size,
	    NULL, BUS_DMA_NOWAIT)){
		printf("failed to load DMA map for tilemap\n");
		goto fail3;
	}

	/* 
	 * Setup DMA for framebuffer.
	 */
	if (bus_dmamap_create(gsc->dmat, screen->fb_size, 1, screen->fb_size,
	    0, BUS_DMA_NOWAIT, &fb_dmamap)) {
		printf("failed to create DMA map for framebuffer!\n");
		goto fail4;
	}

	if (bus_dmamem_alloc(gsc->dmat, screen->fb_size, 65536, 0, fb_segs, 
	    1, &fb_nsegs, BUS_DMA_NOWAIT)) {
		printf("failed to allocate DMA memory for framebuffer!\n");
		goto fail5;
	}

	if (bus_dmamem_map(gsc->dmat, fb_segs, fb_nsegs, screen->fb_size,
	    &screen->fb, BUS_DMA_NOWAIT | BUS_DMA_COHERENT)) {
		printf("failed to map DMA memory for framebuffer!\n");
		goto fail6;
	}

	if (bus_dmamap_load(gsc->dmat, fb_dmamap, screen->fb, screen->fb_size,
	    NULL, BUS_DMA_NOWAIT)) {
		printf("failed to load DMA map for framebuffer\n");
		goto fail7;
	}

	screen->tm_phys = tm_dmamap->dm_segs[0].ds_addr;
	screen->fb_phys = fb_dmamap->dm_segs[0].ds_addr;

	shutdownhook_establish((void(*)(void *))gbe_disable, self);

	gbe_init_screen(screen);
	gbe_disable(gsc);
	gbe_setup(gsc);
	gbe_enable(gsc);

	/* Load colourmap if required. */
	if (screen->depth == 8)
		gbe_loadcmap(screen, 0, 255);

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
	config_found(self, &waa, wsemuldisplaydevprint);

	return;

fail7:
	bus_dmamem_unmap(gsc->dmat, screen->fb, screen->fb_size);
fail6:
	bus_dmamem_free(gsc->dmat, fb_segs, fb_nsegs);
fail5:
	bus_dmamap_destroy(gsc->dmat, fb_dmamap);
fail4:
	bus_dmamap_unload(gsc->dmat, tm_dmamap);
fail3:
	bus_dmamem_unmap(gsc->dmat, screen->tm, screen->tm_size);
fail2:
	bus_dmamem_free(gsc->dmat, tm_segs, tm_nsegs);
fail1:
	bus_dmamap_destroy(gsc->dmat, tm_dmamap);
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
	 * Initialise rasops.
	 */
	memset(&screen->ri, 0, sizeof(struct rasops_info));

	screen->ri.ri_flg = RI_CENTER | RI_CLEAR;
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

	gbe_stdscreen.ncols = screen->ri.ri_cols;
	gbe_stdscreen.nrows = screen->ri.ri_rows;
	gbe_stdscreen.textops = &screen->ri.ri_ops;
	gbe_stdscreen.fontwidth = screen->ri.ri_font->fontwidth;
	gbe_stdscreen.fontheight = screen->ri.ri_font->fontheight;
	gbe_stdscreen.capabilities = screen->ri.ri_caps;

	/*
	 * Map framebuffer into tile map. Each entry in the tilemap is 16 bits 
	 * wide. Each tile is 64KB or 2^16 bits, hence the last 16 bits of the 
	 * address will range from 0x0000 to 0xffff. As a result we simply 
	 * discard the lower 16 bits and store bits 17 through 32 as an entry
	 * in the tilemap.
	 */
	tm = (void *)screen->tm;
	for (i = 0; i < (screen->fb_size >> GBE_TILE_SHIFT); i++)
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

	/* Provide GBE with address of tile map and enable DMA. */
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
	uint32_t val;
	int i, cmode;
	u_char *colour;

	/*
	 * Setup framebuffer.
	 */
	switch (screen->depth) {
	case 32:
		cmode = GBE_CMODE_RGB8;
		break;
	case 16:
		cmode = GBE_CMODE_ARGB5;
		break;
	case 8:
	default:
		cmode = GBE_CMODE_I8;
		break;
	}

	/* Trick framebuffer into linear mode. */
	i = screen->width * screen->height / (512 / (screen->depth >> 3));
	bus_space_write_4(gsc->iot, gsc->ioh, GBE_FB_SIZE_PIXEL, 
	    i << GBE_FB_SIZE_PIXEL_HEIGHT_SHIFT);

	bus_space_write_4(gsc->iot, gsc->ioh, GBE_FB_SIZE_TILE,
	    (1 << GBE_FB_SIZE_TILE_WIDTH_SHIFT) | 
	    ((screen->depth >> 4) << GBE_FB_SIZE_TILE_DEPTH_SHIFT));
	
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

}

/*
 * Interfaces for wscons.
 */

int
gbe_ioctl(void *v, u_long cmd, caddr_t data, int flags, struct proc *p)
{
	struct gbe_screen *screen = (struct gbe_screen *)v;
	int rc;

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
		pa = atop(screen->fb_phys + offset);
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
 * Console functions for early display.
 */

int
gbe_cnprobe(bus_space_tag_t iot, bus_addr_t addr)
{
	bus_space_handle_t ioh;
	int val, width, height;

	/* Setup bus space mapping. */
	ioh = PHYS_TO_XKPHYS(addr, CCA_NC);

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
	struct gbe_softc gsc;
	vaddr_t va;
	paddr_t pa;
	uint32_t val;
	long attr;

	/*
	 * Setup GBE for use as early console.
	 */
	gsc.curscr = &gbe_consdata;
	gbe_consdata.sc = (void *)&gsc;
	
	/* Setup bus space mapping. */
	gsc.iot = iot;
	gsc.ioh = PHYS_TO_XKPHYS(addr, CCA_NC);

	/* Determine GBE revision. */
	gsc.rev = bus_space_read_4(gsc.iot, gsc.ioh, GBE_CTRL_STAT) & 0xf;

	/* Determine resolution configured by firmware. */
	val = bus_space_read_4(gsc.iot, gsc.ioh, GBE_VT_HCMAP);
	gbe_consdata.width = (val >> GBE_VT_HCMAP_ON_SHIFT) & 0xfff;
	val = bus_space_read_4(gsc.iot, gsc.ioh, GBE_VT_VCMAP);
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
	gbe_consdata.tm = (caddr_t)PHYS_TO_XKPHYS(gbe_consdata.tm_phys, CCA_NC);
	
	/* 
	 * Steal memory for framebuffer - 64KB aligned and coherent.
	 */
	va = pmap_steal_memory(gbe_consdata.fb_size + 65536, NULL, NULL);
	pmap_extract(pmap_kernel(), va, &pa);
	gbe_consdata.fb_phys = ((pa >> 16) + 1) << 16;
	gbe_consdata.fb = (caddr_t)PHYS_TO_XKPHYS(gbe_consdata.fb_phys, CCA_NC);

	/*
	 * Setup GBE hardware.
	 */
	gbe_init_screen(&gbe_consdata);
	gbe_disable(&gsc);
	gbe_setup(&gsc);
	gbe_enable(&gsc);

	/* Load colourmap if required. */
	if (gbe_consdata.depth == 8)
		gbe_loadcmap(&gbe_consdata, 0, 255);

	/*
	 * Attach wsdisplay.
	 */
	gbe_consdata.ri.ri_ops.alloc_attr(&gbe_consdata.ri, 0, 0, 0, &attr);
	wsdisplay_cnattach(&gbe_stdscreen, &gbe_consdata.ri, 0, 0, attr);
	gbe_console = 1;

	return (1);
}
