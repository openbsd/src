/* $OpenBSD: s3c24x0_lcd.c,v 1.3 2009/09/05 14:09:33 miod Exp $ */
/* $NetBSD: s3c24x0_lcd.c,v 1.6 2007/12/15 00:39:15 perry Exp $ */

/*
 * Copyright (c) 2004  Genetec Corporation.  All rights reserved.
 * Written by Hiroyuki Bessho for Genetec Corporation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of Genetec Corporation may not be used to endorse or 
 *    promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY GENETEC CORPORATION ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GENETEC CORPORATION
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Support S3C24[10]0's integrated LCD controller.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/kernel.h>			/* for cold */

#include <uvm/uvm_extern.h>

#include <dev/cons.h> 
#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h> 
#include <dev/wscons/wscons_callbacks.h>
#include <dev/rasops/rasops.h>
#include <dev/wsfont/wsfont.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <arm/cpufunc.h>

#include <arm/s3c2xx0/s3c24x0var.h>
#include <arm/s3c2xx0/s3c24x0reg.h>
#include <arm/s3c2xx0/s3c24x0_lcd.h>

#include "wsdisplay.h"

int lcdintr(void *);
static void init_palette(struct s3c24x0_lcd_softc *,
    struct s3c24x0_lcd_screen *);

#ifdef LCD_DEBUG
static void
dump_lcdcon(const char *title, bus_space_tag_t iot, bus_space_handle_t ioh)
{
	int i;

	printf("%s\n", title);
	for(i=LCDC_LCDCON1; i <= LCDC_LCDSADDR3; i+=4) {
		if (i%16 == 0)
			printf("\n%03x: ", i);
		printf("%08x ", bus_space_read_4(iot, ioh, i));
	}

	printf("\n");
}

void draw_test_pattern(struct s3c24x0_lcd_softc *,
	    struct s3c24x0_lcd_screen *scr);

#endif

void
s3c24x0_set_lcd_panel_info(struct s3c24x0_lcd_softc *sc,
    struct s3c24x0_lcd_panel_info *info)
{
	bus_space_tag_t iot = sc->iot;
	bus_space_handle_t ioh = sc->ioh;
	uint32_t reg;
	int clkval;
	int tft = s3c24x0_lcd_panel_tft(info);
	int hclk = s3c2xx0_softc->sc_hclk;

	sc->panel_info = info;

	/* Set LCDCON1. BPPMODE and ENVID are set later */
	if (tft) {
		clkval = (hclk / info->pixel_clock  / 2) - 1;
		printf("clk %x hclk %x pix %d\n", clkval, hclk, info->pixel_clock);
	} else {
		/* STN display */
		clkval = max(2, hclk / info->pixel_clock / 2);
		printf("clk %x hclk %x pix %d\n", clkval, hclk, info->pixel_clock);
	}

	reg =  (info->lcdcon1 & ~LCDCON1_CLKVAL_MASK) |
		(clkval << LCDCON1_CLKVAL_SHIFT);
	reg &= ~LCDCON1_ENVID;
	printf("lcdcon1 old %x, new %x\n", bus_space_read_4(iot, ioh, LCDC_LCDCON1), reg);
	bus_space_write_4(iot, ioh, LCDC_LCDCON1, reg);

#if 0
	printf("hclk=%d pixel clock=%d, clkval = %x lcdcon1=%x\n",
	    hclk, info->pixel_clock, clkval, reg);
#endif

	printf("lcdcon2 old %x, new %x\n", bus_space_read_4(iot, ioh, LCDC_LCDCON2), info->lcdcon2);
	bus_space_write_4(iot, ioh, LCDC_LCDCON2, info->lcdcon2);
	printf("lcdcon3 old %x, new %x\n", bus_space_read_4(iot, ioh, LCDC_LCDCON3), info->lcdcon3);
	bus_space_write_4(iot, ioh, LCDC_LCDCON3, info->lcdcon3);
	printf("lcdcon4 old %x, new %x\n", bus_space_read_4(iot, ioh, LCDC_LCDCON4), info->lcdcon4);
	bus_space_write_4(iot, ioh, LCDC_LCDCON4, info->lcdcon4);
	printf("lcdcon5 old %x, new %x\n", bus_space_read_4(iot, ioh, LCDC_LCDCON5), info->lcdcon5);
	bus_space_write_4(iot, ioh, LCDC_LCDCON5, info->lcdcon5);
	printf("lpcsel old %x, new %x\n", bus_space_read_4(iot, ioh, LCDC_LPCSEL), info->lpcsel);
	bus_space_write_4(iot, ioh, LCDC_LPCSEL, info->lpcsel);
}

void
s3c24x0_lcd_attach_sub(struct s3c24x0_lcd_softc *sc, 
    struct s3c2xx0_attach_args *sa,
    struct s3c24x0_lcd_panel_info *panel_info)
{
	bus_space_tag_t iot = sa->sa_iot;
	bus_space_handle_t ioh;
	int error;

	sc->n_screens = 0;
	LIST_INIT(&sc->screens);

	/* map controller registers */
	error = bus_space_map(iot, sa->sa_addr, S3C24X0_LCDC_SIZE, 0, &ioh);
	if (error) {
		printf(": failed to map registers %d", error);
		return;
	}

	sc->iot = iot;
	sc->ioh = ioh;
	sc->dma_tag = sa->sa_dmat;

#ifdef notyet
	sc->ih = s3c24x0_intr_establish(sa->sa_intr, IPL_BIO, lcdintr, sc);
	if (sc->ih == NULL)
		printf("%s: unable to establish interrupt at irq %d",
		    sc->dev.dv_xname, sa->sa_intr);
#endif

	/* mask LCD interrupts */
	bus_space_write_4(iot, ioh, LCDC_LCDINTMSK, LCDINT_FICNT|LCDINT_FRSYN);

	/* Initialize controller registers based on panel geometry*/
	s3c24x0_set_lcd_panel_info(sc, panel_info);

	/* XXX: enable clock to LCD controller */
}


#ifdef notyet
int
lcdintr(void *arg)
{
	struct s3c24x0_lcd_softc *sc = arg;
	bus_space_tag_t iot = sc->iot;
	bus_space_handle_t ioh = sc->ioh;

	static uint32_t status;

	return 1;
}
#endif

int
s3c24x0_lcd_start_dma(struct s3c24x0_lcd_softc *sc,
    struct s3c24x0_lcd_screen *scr)
{
	bus_space_tag_t iot = sc->iot;
	bus_space_handle_t ioh = sc->ioh;
	const struct s3c24x0_lcd_panel_info *info = sc->panel_info;
	int tft = s3c24x0_lcd_panel_tft(info);
	int dual_panel = 
	    (info->lcdcon1 & LCDCON1_PNRMODE_MASK) == LCDCON1_PNRMODE_DUALSTN4;
	uint32_t lcdcon1, val;
	paddr_t pa;
	int depth = scr->depth;
	int stride = scr->stride;
	int panel_height = info->panel_height;
	int panel_width = info->panel_width;
	int offsize;

	switch (depth) {
	case 1: val = LCDCON1_BPPMODE_STN1; break;
	case 2: val = LCDCON1_BPPMODE_STN2; break;
	case 4: val = LCDCON1_BPPMODE_STN4; break;
	case 8: val = LCDCON1_BPPMODE_STN8; break;
	case 12: 
		if (tft)
			return -1;
		val = LCDCON1_BPPMODE_STN12;
		break;
	case 16:
		if (!tft)
			return -1;	
		val = LCDCON1_BPPMODE_TFT16;
		break;
	case 24:
		if (!tft)
			return -1;
		val = LCDCON1_BPPMODE_TFT24;
		break;
	default:
		return -1;
	}

	if (tft)
		val |= LCDCON1_BPPMODE_TFTX;

	lcdcon1 = bus_space_read_4(iot, ioh, LCDC_LCDCON1);
	lcdcon1 &= ~(LCDCON1_BPPMODE_MASK|LCDCON1_ENVID);
	lcdcon1 |= val;
	bus_space_write_4(iot, ioh, LCDC_LCDCON1, lcdcon1);

	/* Adjust LCDCON3.HOZVAL to meet with restriction */
	val = roundup(panel_width, 16 / depth);
	bus_space_write_4(iot, ioh, LCDC_LCDCON3,
	    (info->lcdcon3 & ~LCDCON3_HOZVAL_MASK) |
	    (val - 1) << LCDCON3_HOZVAL_SHIFT);

	pa = scr->segs[0].ds_addr;
	bus_space_write_4(iot, ioh, LCDC_LCDSADDR1, pa >> 1);

	if (dual_panel) {
		/* XXX */
	}
	else {
		pa += stride * panel_height;
		bus_space_write_4(iot, ioh, LCDC_LCDSADDR2, pa >> 1);
	}

	offsize = stride / sizeof (uint16_t) - (panel_width * depth / 16);
	printf("offset %x %x %x\n", stride, 
	    (panel_width * depth / 16),
	    (offsize << LCDSADDR3_OFFSIZE_SHIFT) |
	    (panel_width * depth / 16));


	bus_space_write_4(iot, ioh, LCDC_LCDSADDR3,
	    (offsize << LCDSADDR3_OFFSIZE_SHIFT) |
	    (panel_width * depth / 16));

	/* set byte- or halfword- swap based on the depth */
	val = bus_space_read_4(iot, ioh, LCDC_LCDCON5);
	val &= ~(LCDCON5_BSWP|LCDCON5_HWSWP);
	switch(depth) {
	case 2:
	case 4:
	case 8:
		val |= LCDCON5_BSWP;
		break;
	case 16:
		val |= LCDCON5_HWSWP;
		break;
	}
	bus_space_write_4(iot, ioh, LCDC_LCDCON5, val);


	init_palette(sc, scr);

#if 0
	bus_space_write_4(iot, ioh, LCDC_TPAL, TPAL_TPALEN|
	    (0xff<<TPAL_BLUE_SHIFT));
#endif

	/* Enable LCDC */
	bus_space_write_4(iot, ioh, LCDC_LCDCON1, lcdcon1 | LCDCON1_ENVID);

	sc->lcd_on = 1;

#ifdef LCD_DEBUG
	dump_lcdcon(__func__, iot, ioh);
#endif

	return 0;
}

void
s3c24x0_lcd_power(struct s3c24x0_lcd_softc *sc, int on)
{
	bus_space_tag_t iot = sc->iot;
	bus_space_handle_t ioh = sc->ioh;
	uint32_t reg;

	reg = bus_space_read_4(iot, ioh, LCDC_LCDCON5);

	if (on)
		reg |= LCDCON5_PWREN;
	else
		reg &= ~LCDCON5_PWREN;

	bus_space_write_4(iot, ioh, LCDC_LCDCON5, reg);
}

struct s3c24x0_lcd_screen *
s3c24x0_lcd_new_screen(struct s3c24x0_lcd_softc *sc, 
    int virtual_width, int virtual_height, int depth)
{
	struct s3c24x0_lcd_screen *scr = NULL;
	int width, height;
	bus_size_t size;
        int error, pallet_size;
	int busdma_flag = (cold ? BUS_DMA_NOWAIT : BUS_DMA_WAITOK) |
	    BUS_DMA_WRITE;
	paddr_t align;
	const struct s3c24x0_lcd_panel_info *panel_info = sc->panel_info;


#ifdef DIAGNOSTIC
	if (size > 1 << 22) {
		printf("%s: too big screen size\n", sc->dev.dv_xname);
		return NULL;
	}
#endif

	width = panel_info->panel_width;
	height = panel_info->panel_height;
	pallet_size = 0;

	switch (depth) {
	case 1: case 2: case 4: case 8:
		virtual_width = roundup(virtual_width, 16 / depth);
		break;
	case 16:
		break;
	case 12: case 24:
	default:
		printf("%s: Unknown depth (%d)\n",
		    sc->dev.dv_xname, depth);
		return NULL;
	}

	scr = malloc(sizeof *scr, M_DEVBUF, 
	    M_ZERO | (cold ? M_NOWAIT : M_WAITOK));

	if (scr == NULL)
		return NULL;

	scr->nsegs = 0;
	scr->depth = depth;
	scr->stride = virtual_width * depth / 8;
	scr->buf_size = size = scr->stride * virtual_height;
	scr->buf_va = NULL;

	/* calculate the alignment for LCD frame buffer.
	   the buffer can't across 4MB boundary */
	align = 1 << 20;
	while (align < size)
		align <<= 1;

        error = bus_dmamem_alloc(sc->dma_tag, size, align, 0,
	    scr->segs, 1, &(scr->nsegs), busdma_flag);

        if (error || scr->nsegs != 1)
		goto bad;

	error = bus_dmamem_map(sc->dma_tag, scr->segs, scr->nsegs,
	    size, (caddr_t *)&(scr->buf_va), busdma_flag | BUS_DMA_COHERENT);
	if (error)
		goto bad;


	memset (scr->buf_va, 0, scr->buf_size);

	/* map memory for DMA */
	if (bus_dmamap_create(sc->dma_tag, 1024*1024*2, 1, 
	    1024*1024*2, 0,  busdma_flag, &scr->dma))
		goto bad;
	error = bus_dmamap_load(sc->dma_tag, scr->dma,
	    scr->buf_va, size, NULL, busdma_flag);
	if (error)
		goto bad;

	LIST_INSERT_HEAD(&(sc->screens), scr, link);
	sc->n_screens++;

#ifdef LCD_DEBUG
	draw_test_pattern(sc, scr);
	dump_lcdcon(__func__, sc->iot, sc->ioh);
#endif
	return scr;

 bad:
	if (scr) {
		if (scr->buf_va)
			bus_dmamem_unmap(sc->dma_tag, scr->buf_va, size);
		if (scr->nsegs)
			bus_dmamem_free(sc->dma_tag, scr->segs, scr->nsegs);
		free(scr, M_DEVBUF);
	}
	return NULL;
}


#define _rgb(r,g,b)	(((r)<<11) | ((g)<<5) | b)
#define rgb(r,g,b)	_rgb((r)>>1,g,(b)>>1)

#define L	0x30			/* low intensity */
#define H	0x3f			/* hight intensity */

static const uint16_t basic_color_map[] = {
	rgb(	0,   0,   0),		/* black */
	rgb(	L,   0,   0),		/* red */
	rgb(	0,   L,   0),		/* green */
	rgb(	L,   L,   0),		/* brown */
	rgb(	0,   0,   L),		/* blue */
	rgb(	L,   0,   L),		/* magenta */
	rgb(	0,   L,   L),		/* cyan */
	_rgb(0x1c,0x38,0x1c),		/* white */

	rgb(	L,   L,   L),		/* black */
	rgb(	H,   0,   0),		/* red */
	rgb(	0,   H,   0),		/* green */
	rgb(	H,   H,   0),		/* brown */
	rgb(	0,   0,   H),		/* blue */
	rgb(	H,   0,   H),		/* magenta */
	rgb(	0,   H,   H),		/* cyan */
	rgb(	H,   H,   H),		/* white */
};

#define COLORMAP_LEN (sizeof basic_color_map / sizeof basic_color_map[0])

#undef H
#undef L

static void
init_palette(struct s3c24x0_lcd_softc *sc, struct s3c24x0_lcd_screen *scr)
{
	int depth = scr->depth;
	bus_space_tag_t iot = sc->iot;
	bus_space_handle_t ioh = sc->ioh;
	int i;

	i = 0;

	switch(depth) {
	default:
	case 16:		/* not using palette */
		return;
	case 8:
		while (i < COLORMAP_LEN) {
			bus_space_write_4(iot, ioh, LCDC_PALETTE + 4*i,
			    basic_color_map[i]);
			++i;
		}
		break;
	case 4:
	case 2:
		/* XXX */
		break;
	case 1:
		bus_space_write_4(iot, ioh, LCDC_PALETTE + 4 * i,
		    basic_color_map[i]); /* black */
		++i;
		bus_space_write_4(iot, ioh, LCDC_PALETTE + 4 * i,
		    basic_color_map[7]); /* white */
		break;
	}

#ifdef DIAGNOSTIC
	/* Fill unused entries */
	for ( ; i < 256; ++i )
		bus_space_write_4(iot, ioh, LCDC_PALETTE + 4 * i, 
		    basic_color_map[1]); /* red */
#endif
}


#if NWSDISPLAY > 0

static void
s3c24x0_lcd_stop_dma(struct s3c24x0_lcd_softc *sc)
{
	/* Stop LCD output */
	bus_space_write_4(sc->iot, sc->ioh, LCDC_LCDCON1,
	    ~LCDCON1_ENVID &
	    bus_space_read_4(sc->iot, sc->ioh, LCDC_LCDCON1));


	sc->lcd_on = 0;
}

int
s3c24x0_lcd_show_screen(void *v, void *cookie, int waitok,
    void (*cb)(void *, int, int), void *cbarg)
{
	struct s3c24x0_lcd_softc *sc = v;
	struct rasops_info *ri = cookie;
	struct s3c24x0_lcd_screen *old, *scr = ri->ri_hw;
	
	/* XXX: make sure the clock is provided for LCD controller */

	old = sc->active;
	if (old == scr && sc->lcd_on)
		return 0;

	if (old)
		s3c24x0_lcd_stop_dma(sc);
	
	s3c24x0_lcd_start_dma(sc, scr);
	sc->active = scr;
	s3c24x0_lcd_power(sc, 1);

	/* XXX: callback */

	return 0;
}

int
s3c24x0_lcd_alloc_screen(void *v, const struct wsscreen_descr *_type,
    void **cookiep, int *curxp, int *curyp, long *attrp)
{
	struct s3c24x0_lcd_softc *sc = v;
	struct s3c24x0_lcd_screen *scr;
	struct rasops_info *ri;
	struct s3c24x0_wsscreen_descr *type =
	    (struct s3c24x0_wsscreen_descr *)_type;

	int width, height;

	width = type->c.ncols * type->c.fontwidth;
	height = type->c.nrows * type->c.fontwidth;

	if (width < sc->panel_info->panel_width)
		width =   sc->panel_info->panel_width;
	if (height < sc->panel_info->panel_height)
		height =   sc->panel_info->panel_height;


	scr = s3c24x0_lcd_new_screen(sc, width, height, type->depth);
	if (scr == NULL)
		return -1;
	
	/*
	 * initialize raster operation for this screen.
	 */
	ri = &scr->rinfo;
	ri->ri_flg = 0;
	ri->ri_depth = type->depth;
	ri->ri_bits = scr->buf_va;
	ri->ri_width = width;
	ri->ri_height = height;
	ri->ri_stride = scr->stride;

	if (type->c.nrows == 0)
		rasops_init(ri, 100, 100);
	else
		rasops_init(ri, type->c.nrows, type->c.ncols);

	ri->ri_ops.alloc_attr(ri, 0, 0, 0, attrp);

	type->c.nrows = ri->ri_rows;
	type->c.ncols = ri->ri_cols;
	type->c.capabilities = ri->ri_caps;
	type->c.textops = &ri->ri_ops;

	ri->ri_hw = (void *)scr;
	*cookiep = ri;
	*curxp = 0;
	*curyp = 0;

	return 0;
}


void
s3c24x0_lcd_free_screen(void *v, void *cookie)
{
	struct s3c24x0_lcd_softc *sc = v;
	struct rasops_info *ri = cookie;
	struct s3c24x0_lcd_screen *scr = ri->ri_hw;


	LIST_REMOVE(scr, link);
	sc->n_screens--;
	if (scr == sc->active) {
		sc->active = NULL;

		/* XXX: We need a good procedure to shutdown the LCD. */

		s3c24x0_lcd_stop_dma(sc);
		s3c24x0_lcd_power(sc, 0);
	}

	if (scr->buf_va)
		bus_dmamem_unmap(sc->dma_tag, scr->buf_va, scr->map_size);

	if (scr->nsegs > 0)
		bus_dmamem_free(sc->dma_tag, scr->segs, scr->nsegs);

	free(scr, M_DEVBUF);
}

int
s3c24x0_lcd_ioctl(void *v, u_long cmd, caddr_t data, int flag,
    struct proc *p)
{
	struct s3c24x0_lcd_softc *sc = v;
	struct wsdisplay_fbinfo *wsdisp_info;
	struct s3c24x0_lcd_screen *scr;


	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_UNKNOWN; /* XXX */
		return 0;

	case WSDISPLAYIO_GINFO:
		wsdisp_info = (struct wsdisplay_fbinfo *)data;

		wsdisp_info->height = sc->panel_info->panel_height;
		wsdisp_info->width = sc->panel_info->panel_width;
		wsdisp_info->depth = 16; /* XXX */
		wsdisp_info->cmsize = 0;
		return 0;

	case WSDISPLAYIO_GETCMAP:
	case WSDISPLAYIO_PUTCMAP:
		return EINVAL;	/* XXX Colormap */

	case WSDISPLAYIO_SVIDEO:
		if (*(int *)data == WSDISPLAYIO_VIDEO_ON) {
			scr = sc->active;
			if (scr == NULL)
				scr = LIST_FIRST(&sc->screens);

			if (scr == NULL)
				return ENXIO;

			s3c24x0_lcd_show_screen(sc, scr, 1, NULL, NULL);
		}
		else {
			s3c24x0_lcd_stop_dma(sc);
			s3c24x0_lcd_power(sc, 0);
		}
		return 0;

	case WSDISPLAYIO_GVIDEO:
		*(u_int *)data = sc->lcd_on;
		return 0;

	case WSDISPLAYIO_GCURPOS:
	case WSDISPLAYIO_SCURPOS:
	case WSDISPLAYIO_GCURMAX:
	case WSDISPLAYIO_GCURSOR:
	case WSDISPLAYIO_SCURSOR:
		return -1;      /* not implemented */

	}

	return 0;
}

paddr_t
s3c24x0_lcd_mmap(void *v, off_t offset, int prot)
{
	struct s3c24x0_lcd_softc *sc = v;
	struct s3c24x0_lcd_screen *screen = sc->active;  /* ??? */

	if (screen == NULL)
		return -1;

	return bus_dmamem_mmap(sc->dma_tag, screen->segs, screen->nsegs,
	    offset, prot, BUS_DMA_WAITOK|BUS_DMA_COHERENT);
	return -1;
}


#if 0
static void
s3c24x0_lcd_cursor(void *cookie, int on, int row, int col)
{
	struct s3c24x0_lcd_screen *scr = cookie;

	(* scr->rinfo.ri_ops.cursor)(&scr->rinfo, on, row, col);
}

static int
s3c24x0_lcd_mapchar(void *cookie, int c, unsigned int *cp)
{
	struct s3c24x0_lcd_screen *scr = cookie;

	return (* scr->rinfo.ri_ops.mapchar)(&scr->rinfo, c, cp);
}

static int
s3c24x0_lcd_putchar(void *cookie, int row, int col, u_int uc, long attr)
{
	struct s3c24x0_lcd_screen *scr = cookie;

	return (* scr->rinfo.ri_ops.putchar)(&scr->rinfo,
	    row, col, uc, attr);
}

static int
s3c24x0_lcd_copycols(void *cookie, int row, int src, int dst, int num)
{
	struct s3c24x0_lcd_screen *scr = cookie;

	return (* scr->rinfo.ri_ops.copycols)(&scr->rinfo,
	    row, src, dst, num);
}

static int
s3c24x0_lcd_erasecols(void *cookie, int row, int col, int num, long attr)
{
	struct s3c24x0_lcd_screen *scr = cookie;

	return (* scr->rinfo.ri_ops.erasecols)(&scr->rinfo,
	    row, col, num, attr);
}

static int
s3c24x0_lcd_copyrows(void *cookie, int src, int dst, int num)
{
	struct s3c24x0_lcd_screen *scr = cookie;

	return (* scr->rinfo.ri_ops.copyrows)(&scr->rinfo,
	    src, dst, num);
}

static int
s3c24x0_lcd_eraserows(void *cookie, int row, int num, long attr)
{
	struct s3c24x0_lcd_screen *scr = cookie;

	return (* scr->rinfo.ri_ops.eraserows)(&scr->rinfo,
	    row, num, attr);
}

static int
s3c24x0_lcd_alloc_attr(void *cookie, int fg, int bg, int flg, long *attr)
{
	struct s3c24x0_lcd_screen *scr = cookie;

	return (* scr->rinfo.ri_ops.allocattr)(&scr->rinfo,
	    fg, bg, flg, attr);
}


const struct wsdisplay_emulops s3c24x0_lcd_emulops = {
	s3c24x0_lcd_cursor,
	s3c24x0_lcd_mapchar,
	s3c24x0_lcd_putchar,
	s3c24x0_lcd_copycols,
	s3c24x0_lcd_erasecols,
	s3c24x0_lcd_copyrows,
	s3c24x0_lcd_eraserows,
	s3c24x0_lcd_alloc_attr
};
#endif

#endif /* NWSDISPLAY > 0 */
