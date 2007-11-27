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

struct gbe_softc {
	struct device sc_dev;
	struct rasops_info ri;		/* Raster display info */

	bus_space_tag_t iot;
	bus_space_handle_t ioh;

	bus_dma_tag_t dmat;

	bus_dmamap_t tm_dmamap;
	bus_dmamap_t fb_dmamap;

	bus_dma_segment_t tm_segs[1];
	bus_dma_segment_t fb_segs[1];

	int tm_nsegs;
	int fb_nsegs;

	int fb_size;			/* Size of framebuffer memory */
	int tm_size;			/* Size of tilemap memory */

	caddr_t tilemap;		/* Address of tilemap memory */
	caddr_t fb;			/* Address of framebuffer memory */

	int rev;			/* Revision */
	int console;			/* Is this the console? */
	int screens;			/* No of screens allocated */

	int width;			/* Width in pixels */
	int height;			/* Height in pixels */
	int depth;			/* Colour depth in bits */
	int linebytes;			/* Bytes per line */
};

void	gbe_disable(struct gbe_softc *);

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

struct wsscreen_descr gbe_stdscreen = {
	"std",		/* Screen name */
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
	struct wsemuldisplaydev_attach_args waa;
	uint16_t *tm;
	uint32_t val;
	int i;
#ifdef notyet
	char *cp;
#endif

	printf(": ");

	/* GBE isn't strictly on the crimebus, but use this for now... */
	gsc->iot = &crimebus_tag;
	gsc->dmat = &mace_bus_dma_tag;
	gsc->console = 0; /* XXX for now! */
	gsc->screens = 0;

	/* 
	 * Setup bus space mapping.
	 */
	if (bus_space_map(gsc->iot, GBE_BASE - CRIMEBUS_BASE, 0x100000, 
	    BUS_SPACE_MAP_LINEAR, &gsc->ioh)) {
		printf("failed to map bus space!\n");
		return;
	}

	/* Determine GBE revision. */
	gsc->rev = bus_space_read_4(gsc->iot, gsc->ioh, GBE_CTRL_STAT) & 0xf;

	/* Determine resolution configured by firmware. */
	val = bus_space_read_4(gsc->iot, gsc->ioh, GBE_VT_HCMAP);
	gsc->width = (val >> GBE_VT_HCMAP_ON_SHIFT) & 0xfff;
	val = bus_space_read_4(gsc->iot, gsc->ioh, GBE_VT_VCMAP);
	gsc->height = (val >> GBE_VT_VCMAP_ON_SHIFT) & 0xfff;

	if (gsc->width == 0 || gsc->height == 0) {
		printf("device has not been setup by firmware!\n");
		goto fail0;
	}

	/* Determine colour depth configured by firmware. */
	val = bus_space_read_4(gsc->iot, gsc->ioh, GBE_FB_SIZE_TILE);
	gsc->depth = 0x8 << ((val >> GBE_FB_SIZE_TILE_DEPTH_SHIFT) & 0x3);

	/* XXX 8MB and 32bpp for now. */
	gsc->fb_size = 8 * 1024 * 1024;
	gsc->tm_size = GBE_TLB_SIZE * sizeof(uint16_t);
	gsc->depth = 32;
	gsc->linebytes = gsc->width * gsc->depth / 8;

	/* 
	 * Setup DMA for tile map.
	 */

	if (bus_dmamap_create(gsc->dmat, gsc->tm_size, 1, gsc->tm_size, 0,
	    BUS_DMA_NOWAIT, &gsc->tm_dmamap)) {
		printf("failed to create DMA map for tile map!\n");
		goto fail0;
	}

	if (bus_dmamem_alloc(gsc->dmat, gsc->tm_size, 65536, 0, gsc->tm_segs, 1,
	    &gsc->tm_nsegs, BUS_DMA_NOWAIT)) {
		printf("failed to allocate DMA memory for tile map!\n");
		goto fail1;
	}

	if (bus_dmamem_map(gsc->dmat, gsc->tm_segs, gsc->tm_nsegs, gsc->tm_size,
	    &gsc->tilemap, BUS_DMA_COHERENT)) {
		printf("failed to map DMA memory for tile map!\n");
		goto fail2;
	}

	if (bus_dmamap_load(gsc->dmat, gsc->tm_dmamap, gsc->tilemap, 
	    gsc->tm_size, NULL, BUS_DMA_NOWAIT)){
		printf("failed to load DMA map for tilemap\n");
		goto fail3;
	}

	/* 
	 * Setup DMA for framebuffer.
	 */

	if (bus_dmamap_create(gsc->dmat, gsc->fb_size, 1, gsc->fb_size, 0, 
	    BUS_DMA_NOWAIT, &gsc->fb_dmamap)) {
		printf("failed to create DMA map for framebuffer!\n");
		goto fail4;
	}

	if (bus_dmamem_alloc(gsc->dmat, gsc->fb_size, 65536, 0, gsc->fb_segs, 
	    1, &gsc->fb_nsegs, BUS_DMA_NOWAIT)) {
		printf("failed to allocate DMA memory for framebuffer!\n");
		goto fail5;
	}

	if (bus_dmamem_map(gsc->dmat, gsc->fb_segs, gsc->fb_nsegs, gsc->fb_size,
	    &gsc->fb, BUS_DMA_NOWAIT | BUS_DMA_COHERENT)) {
		printf("failed to map DMA memory for framebuffer!\n");
		goto fail6;
	}

	if (bus_dmamap_load(gsc->dmat, gsc->fb_dmamap, gsc->fb, gsc->fb_size, 
	    NULL, BUS_DMA_NOWAIT)) {
		printf("failed to load DMA map for framebuffer\n");
		goto fail7;
	}

	shutdownhook_establish((void(*)(void *))gbe_disable, self);

	/*
	 * Initialise rasops.
	 */
	memset(&gsc->ri, 0, sizeof(struct rasops_info));

	gsc->ri.ri_flg = RI_CENTER | RI_CLEAR;
	gsc->ri.ri_depth = gsc->depth;
	gsc->ri.ri_width = gsc->width;
	gsc->ri.ri_height = gsc->height;
	gsc->ri.ri_bits = (void *)gsc->fb;
	gsc->ri.ri_stride = gsc->linebytes;

	if (gsc->depth == 32) {
		gsc->ri.ri_rpos = 24;
		gsc->ri.ri_rnum = 8;
		gsc->ri.ri_gpos = 16;
		gsc->ri.ri_gnum = 8;
		gsc->ri.ri_bpos = 8;
		gsc->ri.ri_bnum = 8;
	}

	rasops_init(&gsc->ri, gsc->height / 8, gsc->width / 8);

	/*
	 * Map framebuffer into tile map. Each entry in the tilemap is 16 bits 
	 * wide. Each tile is 64KB or 2^16 bits, hence the last 16 bits of the 
	 * address will range from 0x0000 to 0xffff. As a result we simply 
	 * discard the lower 16 bits and store bits 17 through 32 as an entry
	 * in the tilemap.
	 */
	tm = (void *)gsc->tilemap;
	for (i = 0; i < (gsc->fb_size >> GBE_TILE_SHIFT); i++)
	    tm[i] = (gsc->fb_dmamap->dm_segs[0].ds_addr >> GBE_TILE_SHIFT) + i;

	/* Disable DMA. */
	val = bus_space_read_4(gsc->iot, gsc->ioh, GBE_OVERLAY_CTRL);
	bus_space_write_4(gsc->iot, gsc->ioh, GBE_OVERLAY_CTRL, 
	    val & ~GBE_OVERLAY_CTRL_DMA_ENABLE);
	delay(1000);
	val = bus_space_read_4(gsc->iot, gsc->ioh, GBE_FB_CTRL);
	bus_space_write_4(gsc->iot, gsc->ioh, GBE_FB_CTRL, 
	    val & ~GBE_FB_CTRL_DMA_ENABLE);
	delay(1000);
	bus_space_write_4(gsc->iot, gsc->ioh, GBE_DID_CTRL, 0);
	delay(1000);

	/* Freeze pixel counter. */
	bus_space_write_4(gsc->iot, gsc->ioh, GBE_VT_XY, GBE_VT_XY_FREEZE);
	delay(10000);
	for (i = 0; i < 10000; i++) {
		val = bus_space_read_4(gsc->iot, gsc->ioh, GBE_VT_XY);
		if ((val & GBE_VT_XY_FREEZE) == GBE_VT_XY_FREEZE)
			break;
		delay(10);
	}
	if (i == 10000)
		printf("timeout freezing pixel counter!\n");

	/* Disable dot clock. */
	val = bus_space_read_4(gsc->iot, gsc->ioh, GBE_DOTCLOCK);
	bus_space_write_4(gsc->iot, gsc->ioh, GBE_DOTCLOCK, 
	    val & ~GBE_DOTCLOCK_RUN);
	delay(10000);
	for (i = 0; i < 10000; i++) {
		val = bus_space_read_4(gsc->iot, gsc->ioh, GBE_DOTCLOCK);
		if ((val & GBE_DOTCLOCK_RUN) == 0)
			break;
		delay(10);
	}
	if (i == 10000)
		printf("timeout disabling dot clock!\n");

	/* Reset DMA fifo. */
        val = bus_space_read_4(gsc->iot, gsc->ioh, GBE_FB_SIZE_TILE);
	val &= ~(1 << GBE_FB_SIZE_TILE_FIFO_RESET_SHIFT);
	bus_space_write_4(gsc->iot, gsc->ioh, GBE_FB_SIZE_TILE, 
	    val | (1 << GBE_FB_SIZE_TILE_FIFO_RESET_SHIFT));
	bus_space_write_4(gsc->iot, gsc->ioh, GBE_FB_SIZE_TILE, val);

	/* Disable overlay and turn off hardware cursor. */
	bus_space_write_4(gsc->iot, gsc->ioh, GBE_OVERLAY_TILE, 0);
	bus_space_write_4(gsc->iot, gsc->ioh, GBE_CURSOR_CTRL, 0);
	bus_space_write_4(gsc->iot, gsc->ioh, GBE_DID_CTRL, 0);

	/* Trick framebuffer into linear mode. */
	i = gsc->width * gsc->height / (512 / (gsc->depth >> 3));
	bus_space_write_4(gsc->iot, gsc->ioh, GBE_FB_SIZE_PIXEL, 
	    i << GBE_FB_SIZE_PIXEL_HEIGHT_SHIFT);

	/* Setup framebuffer - fixed at 32bpp for now. */
	bus_space_write_4(gsc->iot, gsc->ioh, GBE_FB_SIZE_TILE,
	    (1 << GBE_FB_SIZE_TILE_WIDTH_SHIFT) | 
	    (GBE_FB_DEPTH_32 << GBE_FB_SIZE_TILE_DEPTH_SHIFT));
	val = (GBE_CMODE_RGB8 << GBE_WID_MODE_SHIFT) | GBE_BMODE_BOTH;
	for (i = 0; i < (32 * 4); i += 4)
		bus_space_write_4(gsc->iot, gsc->ioh, GBE_MODE + i, val);

	/* Enable dot clock. */
	val = bus_space_read_4(gsc->iot, gsc->ioh, GBE_DOTCLOCK);
	bus_space_write_4(gsc->iot, gsc->ioh, GBE_DOTCLOCK, 
	    val | GBE_DOTCLOCK_RUN);
	delay(10000);
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
	delay(10000);
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
	    ((gsc->tm_dmamap->dm_segs[0].ds_addr >> 9) << 
	    GBE_FB_CTRL_TILE_PTR_SHIFT) | GBE_FB_CTRL_DMA_ENABLE);

	printf("rev %u, %iMB, %dx%d at %d bits\n", gsc->rev, gsc->fb_size >> 20,
	    gsc->width, gsc->height, gsc->depth);

#ifdef notyet
	cp = Bios_GetEnvironmentVariable("ConsoleOut");
        if (cp != NULL && strncmp(cp, "video", 5) == 0) {
		wsdisplay_cnattach(&gbe_stdscreen, &gsc->ri, 0, 0, /*defattr*/ 0);
		gsc->console = 1;
        }
#endif

	/*
	 * Setup default screen and attach wsdisplay.
	 */

	gbe_stdscreen.ncols = gsc->ri.ri_cols;
	gbe_stdscreen.nrows = gsc->ri.ri_rows;
	gbe_stdscreen.textops = &gsc->ri.ri_ops;
	gbe_stdscreen.fontwidth = gsc->ri.ri_font->fontwidth;
	gbe_stdscreen.fontheight = gsc->ri.ri_font->fontheight;
	gbe_stdscreen.capabilities = gsc->ri.ri_caps;

	waa.console = gsc->console;
	waa.scrdata = &gbe_screenlist;
	waa.accessops = &gbe_accessops;
	waa.accesscookie = self;
	config_found(self, &waa, wsemuldisplaydevprint);

	return;

fail7:
	bus_dmamem_unmap(gsc->dmat, gsc->fb, gsc->fb_size);
fail6:
	bus_dmamem_free(gsc->dmat, gsc->fb_segs, gsc->fb_nsegs);
fail5:
	bus_dmamap_destroy(gsc->dmat, gsc->fb_dmamap);
fail4:
	bus_dmamap_unload(gsc->dmat, gsc->tm_dmamap);
fail3:
	bus_dmamem_unmap(gsc->dmat, gsc->tilemap, gsc->tm_size);
fail2:
	bus_dmamem_free(gsc->dmat, gsc->tm_segs, gsc->tm_nsegs);
fail1:
	bus_dmamap_destroy(gsc->dmat, gsc->tm_dmamap);
fail0:
	bus_space_unmap(gsc->iot, gsc->ioh, 0x100000);
}

/*
 * GBE hardware specific functions.
 */

void
gbe_disable(struct gbe_softc *gsc)
{
	uint32_t val;

	val = bus_space_read_4(gsc->iot, gsc->ioh, GBE_FB_CTRL);
	bus_space_write_4(gsc->iot, gsc->ioh, GBE_FB_CTRL, 
	    val & ~GBE_FB_CTRL_DMA_ENABLE);
}

/*
 * Interfaces for wscons.
 */

int
gbe_ioctl(void *v, u_long cmd, caddr_t data, int flags, struct proc *p)
{
	struct gbe_softc *gsc = (struct gbe_softc *)v;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_GBE;
		break;

	case WSDISPLAYIO_GINFO:
	{
		struct wsdisplay_fbinfo *fb = (struct wsdisplay_fbinfo *)data;

		fb->height = gsc->height;
		fb->width = gsc->width;
		fb->depth = gsc->depth;
		fb->cmsize = gsc->depth == 8 ? 256 : 0;
	}
		break;

	case WSDISPLAYIO_LINEBYTES:
		*(u_int *)data = gsc->linebytes;
		break;

	case WSDISPLAYIO_GETCMAP:
		if (gsc->depth == 8) {
#ifdef notyet
			struct wsdisplay_cmap *cm =
			    (struct wsdisplay_cmap *)data;

			rc = gbe_getcmap(&scr->scr_cmap, cm);
			if (rc != 0)
				return (rc);
#endif
		}
		break;

	case WSDISPLAYIO_PUTCMAP:
		if (gsc->depth == 8) {
#ifdef notyet
			struct wsdisplay_cmap *cm =
			    (struct wsdisplay_cmap *)data;

			rc = gbe_putcmap(&scr->scr_cmap, cm);
			if (rc != 0)
				return (rc);
			gbe_loadcmap(sc, cm->index, cm->index + cm->count);
#endif
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
	/* Not at the moment. */
	return (-1);
}

int
gbe_alloc_screen(void *v, const struct wsscreen_descr *type, void **cookiep,
    int *curxp, int *curyp, long *attrp)
{
	struct gbe_softc *gsc = (struct gbe_softc *)v;

	/* We do not allow multiple consoles at the moment. */
	if (gsc->screens > 0)
		return (ENOMEM);

	gsc->screens++;

	/* Return rasops_info via cookie. */
	*cookiep = &gsc->ri;

	/* Move cursor to top left of screen. */
	*curxp = 0;
	*curyp = 0;

	/* Correct screen attributes. */
	gsc->ri.ri_ops.alloc_attr(&gsc->ri, 0, 0, 0, attrp);

	return 0;
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
