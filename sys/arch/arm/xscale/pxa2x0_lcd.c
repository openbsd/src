/*	$OpenBSD: pxa2x0_lcd.c,v 1.10 2005/01/17 04:22:34 drahn Exp $ */
/* $NetBSD: pxa2x0_lcd.c,v 1.8 2003/10/03 07:24:05 bsh Exp $ */

/*
 * Copyright (c) 2002  Genetec Corporation.  All rights reserved.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed for the NetBSD Project by
 *	Genetec Corporation.
 * 4. The name of Genetec Corporation may not be used to endorse or 
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
 * Support PXA2[15]0's integrated LCD controller.
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

#include <arm/xscale/pxa2x0var.h>
#include <arm/xscale/pxa2x0reg.h>
#include <arm/xscale/pxa2x0_lcd.h>
#include <arm/xscale/pxa2x0_gpio.h>

int lcdintr(void *);

void
pxa2x0_lcd_geometry(struct pxa2x0_lcd_softc *sc,
    const struct lcd_panel_geometry *info)
{
	int lines;
	bus_space_tag_t iot = sc->iot;
	bus_space_handle_t ioh = sc->ioh;
	uint32_t ccr0;

	sc->geometry = info;

	ccr0 = LCCR0_IMASK;
	if (info->panel_info & LCDPANEL_ACTIVE)
		ccr0 |= LCCR0_PAS;	/* active mode */
	if ((info->panel_info & (LCDPANEL_DUAL|LCDPANEL_ACTIVE))
	    == LCDPANEL_DUAL)
		ccr0 |= LCCR0_SDS; /* dual panel */
	if (info->panel_info & LCDPANEL_MONOCHROME)
		ccr0 |= LCCR0_CMS;
	/* XXX - Zaurus C3000 */
	ccr0 |= LCCR0_LDDALT | 
	    LCCR0_OUC |
	    LCCR0_CMDIM |
	    LCCR0_RDSTM;

	bus_space_write_4(iot, ioh, LCDC_LCCR0, ccr0);

	bus_space_write_4(iot, ioh, LCDC_LCCR1,
	    (info->panel_width-1)
	    | ((info->hsync_pulse_width-1)<<10)
	    | ((info->end_line_wait-1)<<16)
	    | ((info->beg_line_wait-1)<<24));

	if (info->panel_info & LCDPANEL_DUAL)
		lines = info->panel_height/2 + info->extra_lines;
	else
		lines = info->panel_height + info->extra_lines;

	bus_space_write_4(iot, ioh, LCDC_LCCR2,
	    (lines-1)
	    | (info->vsync_pulse_width<<10)
	    | (info->end_frame_wait<<16)
	    | (info->beg_frame_wait<<24));

	bus_space_write_4(iot, ioh, LCDC_LCCR3,
	    (info->pixel_clock_div<<0)
	    | (info->ac_bias << 8)
	    | ((info->panel_info & 
		(LCDPANEL_VSP|LCDPANEL_HSP|LCDPANEL_PCP|LCDPANEL_OEP))
		<<20)
	    | (4 << 24) /* 16bpp */
	    | ((info->panel_info & LCDPANEL_DPC) ? (1<<27) : 0)
	    );
}

void
pxa2x0_lcd_attach_sub(struct pxa2x0_lcd_softc *sc, 
    struct pxaip_attach_args *pxa, const struct lcd_panel_geometry *geom)
{
	bus_space_tag_t iot = pxa->pxa_iot;
	bus_space_handle_t ioh;
	int error, nldd;
	u_int32_t lccr0, lscr;

	sc->n_screens = 0;
	LIST_INIT(&sc->screens);

	/* map controller registers */
	error = bus_space_map(iot, PXA2X0_LCDC_BASE, PXA2X0_LCDC_SIZE, 0, &ioh);
	if (error) {
		printf(": failed to map registers %d", error);
		return;
	}

	sc->iot = iot;
	sc->ioh = ioh;
	sc->dma_tag = &pxa2x0_bus_dma_tag;

	sc->ih = pxa2x0_intr_establish(17, IPL_BIO, lcdintr, sc,
	    sc->dev.dv_xname);
	if (sc->ih == NULL)
		printf("%s: unable to establish interrupt at irq %d",
		    sc->dev.dv_xname, 17);

	/* Initialize LCD controller */

	/* Check if LCD is enabled before programming, it should not
	 * be enabled while it is being reprogrammed, therefore disable
	 * it first.
	 */
	lccr0 = bus_space_read_4(iot, ioh, LCDC_LCCR0);
	if (lccr0 & LCCR0_ENB) {
		lccr0 |= LCCR0_LDM;
		bus_space_write_4(iot, ioh, LCDC_LCCR0, lccr0);
		lccr0 = bus_space_read_4(iot, ioh, LCDC_LCCR0); /* paranoia */
		lccr0 |= LCCR0_DIS;
		bus_space_write_4(iot, ioh, LCDC_LCCR0, lccr0);
		do {
			lscr = bus_space_read_4(iot, ioh, LCDC_LCSR); 
		} while (!(lscr & LCSR_LDD));
	}

	/* enable clock */
	pxa2x0_clkman_config(CKEN_LCD, 1);

	bus_space_write_4(iot, ioh, LCDC_LCCR0, LCCR0_IMASK);

	/*
	 * setup GP[77:58] for LCD
	 */
	/* Always use [FLP]CLK, ACBIAS */
	pxa2x0_gpio_set_function(74, GPIO_ALT_FN_2_OUT);
	pxa2x0_gpio_set_function(75, GPIO_ALT_FN_2_OUT);
	pxa2x0_gpio_set_function(76, GPIO_ALT_FN_2_OUT);
	pxa2x0_gpio_set_function(77, GPIO_ALT_FN_2_OUT);

	if ((geom->panel_info & LCDPANEL_ACTIVE) ||
	    ((geom->panel_info & (LCDPANEL_MONOCHROME|LCDPANEL_DUAL)) ==
	    LCDPANEL_DUAL)) {
		/* active and color dual panel need L_DD[15:0] */
		nldd = 16;
	} else
	if ((geom->panel_info & LCDPANEL_DUAL) ||
	    !(geom->panel_info & LCDPANEL_MONOCHROME)) {
		/* dual or color need L_DD[7:0] */
		nldd = 8;
	} else {
		/* Otherwise just L_DD[3:0] */
		nldd = 4;
	}

	while (nldd--)
		pxa2x0_gpio_set_function(58 + nldd, GPIO_ALT_FN_2_OUT);

	pxa2x0_lcd_geometry(sc, geom);
}

int
lcdintr(void *arg)
{
	struct pxa2x0_lcd_softc *sc = arg;
	bus_space_tag_t iot = sc->iot;
	bus_space_handle_t ioh = sc->ioh;

	static uint32_t status;

	status = bus_space_read_4(iot, ioh, LCDC_LCSR);
	/* Clear sticky status bits */
	bus_space_write_4(iot, ioh, LCDC_LCSR, status);

	return 1;
}

void
pxa2x0_lcd_start_dma(struct pxa2x0_lcd_softc *sc,
    struct pxa2x0_lcd_screen *scr)
{
	uint32_t tmp;
	bus_space_tag_t iot = sc->iot;
	bus_space_handle_t ioh = sc->ioh;
	int val, save;

	save = disable_interrupts(I32_bit);

	switch (scr->depth) {
	case 1: val = 0; break;
	case 2: val = 1; break;
	case 4: val = 2; break;
	case 8: val = 3; break;
	case 16:
		/* FALLTHROUGH */
	default:
		val = 4; break;		
	}

	tmp = bus_space_read_4(iot, ioh, LCDC_LCCR3);
	bus_space_write_4(iot, ioh, LCDC_LCCR3, 
	    (tmp & ~LCCR3_BPP) | (val << LCCR3_BPP_SHIFT));

	bus_space_write_4(iot, ioh, LCDC_FDADR0, 
	    scr->depth == 16 ? scr->dma_desc_pa :
	    scr->dma_desc_pa + 2 * sizeof (struct lcd_dma_descriptor));
	bus_space_write_4(iot, ioh, LCDC_FDADR1, 
	    scr->dma_desc_pa + 1 * sizeof (struct lcd_dma_descriptor));

	/* clear status */
	bus_space_write_4(iot, ioh, LCDC_LCSR, 0);

	delay(1000);			/* ??? */

	/* Enable LCDC */
	tmp = bus_space_read_4(iot, ioh, LCDC_LCCR0);
	/*tmp &= ~LCCR0_SFM;*/
	bus_space_write_4(iot, ioh, LCDC_LCCR0, tmp | LCCR0_ENB);

	restore_interrupts(save);

}

static void
pxa2x0_lcd_stop_dma(struct pxa2x0_lcd_softc *sc)
{
	/* Stop LCD DMA after current frame */
	bus_space_write_4(sc->iot, sc->ioh, LCDC_LCCR0,
	    LCCR0_DIS |
	    bus_space_read_4(sc->iot, sc->ioh, LCDC_LCCR0));

	/* wait for disabling done.
	   XXX: use interrupt. */
	while (LCCR0_ENB &
	    bus_space_read_4(sc->iot, sc->ioh, LCDC_LCCR0))
		;

	bus_space_write_4(sc->iot, sc->ioh, LCDC_LCCR0,
	    ~LCCR0_DIS &
	    bus_space_read_4(sc->iot, sc->ioh, LCDC_LCCR0));
}

#define _rgb(r,g,b)	(((r)<<11) | ((g)<<5) | b)
#define rgb(r,g,b)	_rgb((r)>>1,g,(b)>>1)

#define L	0x1f			/* low intensity */
#define H	0x3f			/* high intensity */

static uint16_t basic_color_map[] = {
	rgb(	0,   0,   0),		/* black */
	rgb(	L,   0,   0),		/* red */
	rgb(	0,   L,   0),		/* green */
	rgb(	L,   L,   0),		/* brown */
	rgb(	0,   0,   L),		/* blue */
	rgb(	L,   0,   L),		/* magenta */
	rgb(	0,   L,   L),		/* cyan */
	rgb( 0x31,0x31,0x31),		/* white */

	rgb(	L,   L,   L),		/* black */
	rgb(	H,   0,   0),		/* red */
	rgb(	0,   H,   0),		/* green */
	rgb(	H,   H,   0),		/* brown */
	rgb(	0,   0,   H),		/* blue */
	rgb(	H,   0,   H),		/* magenta */
	rgb(	0,   H,   H),		/* cyan */
	rgb(	H,   H,   H)
};

#undef H
#undef L

static void
init_palette(uint16_t *buf, int depth)
{
	int i;

	/* convert RGB332 to RGB565 */
	switch (depth) {
	case 8:
	case 4:
#if 0
		for (i=0; i <= 255; ++i) {
			buf[i] = ((9 * ((i>>5) & 0x07)) <<11) |
			    ((9 * ((i>>2) & 0x07)) << 5) |
			    ((21 * (i & 0x03))/2);
		}
#else
		memcpy(buf, basic_color_map, sizeof basic_color_map);
		for (i=16; i < (1<<depth); ++i)
			buf[i] = 0xffff;
#endif
		break;
	case 16:
		/* palette is not needed */
		break;
	default:
		/* other depths are not supported */
		break;
	}
}

struct pxa2x0_lcd_screen *
pxa2x0_lcd_new_screen(struct pxa2x0_lcd_softc *sc,
    int depth)
{
	struct pxa2x0_lcd_screen *scr = NULL;
	int width, height;
	bus_size_t size;
	int error, palette_size;
	int busdma_flag = (cold ? BUS_DMA_NOWAIT : BUS_DMA_WAITOK);
	struct lcd_dma_descriptor *desc;
	paddr_t buf_pa, desc_pa;

	width = sc->geometry->panel_width;
	height = sc->geometry->panel_height;
	palette_size = 0;

	switch (depth) {
	case 1:
	case 2:
	case 4:
	case 8:
		palette_size = (1<<depth) * sizeof (uint16_t);
		/* FALLTHROUGH */
	case 16:
		size = roundup(width,4)*depth/8 * height;
		break;
	default:
		printf("%s: Unknown depth (%d)\n", sc->dev.dv_xname, depth);
		return NULL;
	}

	scr = malloc(sizeof *scr, M_DEVBUF, (cold ? M_NOWAIT : M_WAITOK));

	if (scr == NULL)
		return NULL;

	bzero (scr, sizeof *scr);

	scr->nsegs = 0;
	scr->depth = depth;
	scr->buf_size = size;
	scr->buf_va = NULL;
	size = roundup(size,16) + 3 * sizeof (struct lcd_dma_descriptor)
	    + palette_size;

	error = bus_dmamem_alloc(sc->dma_tag, size, 16, 0,
	    scr->segs, 1, &(scr->nsegs), busdma_flag);

	if (error || scr->nsegs != 1) {
		/* XXX: Actually we can handle nsegs > 1 case by means
		   of multiple DMA descriptors for a panel.  It would
		    make code here a bit hairy */
		goto bad;
	}

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
	if (error) {
		goto bad;
	}

	buf_pa = scr->segs[0].ds_addr;
	desc_pa = buf_pa + roundup(size, PAGE_SIZE) - 3*sizeof *desc;

	/* make descriptors at the top of mapped memory */
	desc = (struct lcd_dma_descriptor *)(
		(caddr_t)(scr->buf_va) + roundup(size, PAGE_SIZE) -
			  3*sizeof *desc);

	desc[0].fdadr = desc_pa;
	desc[0].fsadr = buf_pa;
	desc[0].ldcmd = scr->buf_size;

	if (palette_size) {
		init_palette((uint16_t *)((char *)desc - palette_size), depth);

		desc[2].fdadr = desc_pa; /* chain to panel 0 */
		desc[2].fsadr = desc_pa - palette_size;
		desc[2].ldcmd = palette_size | LDCMD_PAL;
	}

	if (sc->geometry->panel_info & LCDPANEL_DUAL) {
		/* Dual panel */
		desc[1].fdadr = desc_pa + sizeof *desc;
		desc[1].fsadr = buf_pa + scr->buf_size/2;
		desc[0].ldcmd = desc[1].ldcmd = scr->buf_size/2;

	}

#if 0
	desc[0].ldcmd |= LDCMD_SOFINT;
	desc[1].ldcmd |= LDCMD_SOFINT;
#endif

	scr->dma_desc = desc;
	scr->dma_desc_pa = desc_pa;
	scr->map_size = size;		/* used when unmap this. */

	LIST_INSERT_HEAD(&(sc->screens), scr, link);
	sc->n_screens++;
	
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

/*
 * Initialize struct wsscreen_descr based on values calculated by 
 * raster operation subsystem.
 */
int
pxa2x0_lcd_setup_wsscreen(struct pxa2x0_lcd_softc *sc,
    struct pxa2x0_wsscreen_descr *descr,
    const struct lcd_panel_geometry *geom,
    const char *fontname)
{
	int width = geom->panel_width;
	int height = geom->panel_height;
	int cookie = -1;
	struct rasops_info *rinfo;

	rinfo = &sc->sc_ro;
	memset(rinfo, 0, sizeof(struct rasops_info));

#ifdef notyet
	if (fontname) {
		wsfont_init();
		cookie = wsfont_find((char *)fontname, 0, 0, 0, 
		    WSDISPLAY_FONTORDER_L2R, WSDISPLAY_FONTORDER_L2R);
		if (cookie < 0 ||
		    wsfont_lock(cookie, &rinfo.ri_font))
			return -1;
	} else {
		/* let rasops_init() choose any font */
	}
#endif

	/* let rasops_init calculate # of cols and rows in character */
	rinfo->ri_flg = 0;
	rinfo->ri_depth = descr->depth;
	rinfo->ri_bits = NULL;
	rinfo->ri_width = width;
	rinfo->ri_height = height;
	rinfo->ri_stride = width * rinfo->ri_depth / 8;
	rinfo->ri_wsfcookie = cookie;

	if (descr->depth == 16) {
		rinfo->ri_rnum = 5;
		rinfo->ri_rpos = 11;
		rinfo->ri_gnum = 6;
		rinfo->ri_gpos = 5;
		rinfo->ri_bnum = 5;
		rinfo->ri_bpos = 0;
	}

	rasops_init(rinfo, 100, 100);

	descr->c.nrows = rinfo->ri_rows;
	descr->c.ncols = rinfo->ri_cols;
	descr->c.capabilities = rinfo->ri_caps;
	descr->c.textops = &rinfo->ri_ops;

	return cookie;
}

int
pxa2x0_lcd_setup_console(struct pxa2x0_lcd_softc *sc,
    const struct pxa2x0_wsscreen_descr *descr)
{
	struct pxa2x0_lcd_screen *scr;

	scr = pxa2x0_lcd_new_screen(sc, descr->depth);
	if (scr == NULL)
		return (ENOMEM);
	
	sc->sc_ro.ri_hw = (void *)scr;
	sc->sc_ro.ri_bits = scr->buf_va;
	bcopy(&sc->sc_ro, &scr->rinfo, sizeof(struct rasops_info));

	pxa2x0_lcd_show_screen(sc, &sc->sc_ro, 0, NULL, NULL);
	return (0);
}

int
pxa2x0_lcd_show_screen(void *v, void *cookie, int waitok,
    void (*cb)(void *, int, int), void *cbarg)
{
	struct pxa2x0_lcd_softc *sc = v;
	struct rasops_info *ri = cookie;
	struct pxa2x0_lcd_screen *scr = ri->ri_hw, *old;
	
	old = sc->active;
	if (old == scr)
		return 0;

	if (old)
		pxa2x0_lcd_stop_dma(sc);
	
	pxa2x0_lcd_start_dma(sc, scr);

	sc->active = scr;
	return 0;
}

int
pxa2x0_lcd_alloc_screen(void *v, const struct wsscreen_descr *_type,
    void **cookiep, int *curxp, int *curyp, long *attrp)
{
	struct pxa2x0_lcd_softc *sc = v;
	struct pxa2x0_lcd_screen *scr;
	struct rasops_info *ri;
	struct pxa2x0_wsscreen_descr *type = (struct pxa2x0_wsscreen_descr *)_type;

	scr = pxa2x0_lcd_new_screen(sc, type->depth);
	if (scr == NULL)
		return (ENOMEM);

	/*
	 * initialize raster operation for this screen.
	 */
	ri = &scr->rinfo;
	ri->ri_hw = (void *)scr;
	ri->ri_flg = 0;
	ri->ri_depth = type->depth;
	ri->ri_bits = scr->buf_va;
	ri->ri_width = sc->geometry->panel_width;
	ri->ri_height = sc->geometry->panel_height;
	ri->ri_stride = ri->ri_width * ri->ri_depth / 8;
#ifdef notyet
	ri->ri_wsfcookie = -1;	/* XXX */
#endif

	if (type->depth == 16) {
		ri->ri_rnum = 5;
		ri->ri_rpos = 11;
		ri->ri_gnum = 6;
		ri->ri_gpos = 5;
		ri->ri_bnum = 5;
		ri->ri_bpos = 0;
	}

	rasops_init(ri, type->c.nrows, type->c.ncols);

	ri->ri_ops.alloc_attr(ri, 0, 0, 0, attrp);

	*cookiep = ri;
	*curxp = 0;
	*curyp = 0;

	return 0;
}

void
pxa2x0_lcd_free_screen(void *v, void *cookie)
{
	struct pxa2x0_lcd_softc *sc = v;
	struct rasops_info *ri = cookie;
	struct pxa2x0_lcd_screen *scr = ri->ri_hw;

	LIST_REMOVE(scr, link);
	sc->n_screens--;
	if (scr == sc->active) {
		/* at first, we need to stop LCD DMA */
		sc->active = NULL;

		printf("lcd_free on active screen\n");

		pxa2x0_lcd_stop_dma(sc);
	}

	if (scr->buf_va)
		bus_dmamem_unmap(sc->dma_tag, scr->buf_va, scr->map_size);

	if (scr->nsegs > 0)
		bus_dmamem_free(sc->dma_tag, scr->segs, scr->nsegs);

	free(scr, M_DEVBUF);
}

int
pxa2x0_lcd_ioctl(void *v, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct pxa2x0_lcd_softc *sc = v;
	struct wsdisplay_fbinfo *wsdisp_info;
	struct pxa2x0_lcd_screen *scr = sc->active;  /* ??? */

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_PXALCD; /* XXX */
		break;

	case WSDISPLAYIO_GINFO:
		wsdisp_info = (struct wsdisplay_fbinfo *)data;

		wsdisp_info->height = sc->geometry->panel_height;
		wsdisp_info->width = sc->geometry->panel_width;
		wsdisp_info->depth = 16; /* XXX */
		wsdisp_info->cmsize = 0;
		break;

	case WSDISPLAYIO_GETCMAP:
	case WSDISPLAYIO_PUTCMAP:
		return EINVAL;	/* XXX Colormap */

	case WSDISPLAYIO_SVIDEO:
	case WSDISPLAYIO_GVIDEO:
		break;

	case WSDISPLAYIO_GCURPOS:
	case WSDISPLAYIO_SCURPOS:
	case WSDISPLAYIO_GCURMAX:
	case WSDISPLAYIO_GCURSOR:
	case WSDISPLAYIO_SCURSOR:
		return -1;	/* not implemented */

        case WSDISPLAYIO_LINEBYTES:
		*(u_int *)data = scr->rinfo.ri_stride;
		break;
	}
	return (0);
}

paddr_t
pxa2x0_lcd_mmap(void *v, off_t offset, int prot)
{
	struct pxa2x0_lcd_softc *sc = v;
	struct pxa2x0_lcd_screen *screen = sc->active;  /* ??? */

	if ((offset & PAGE_MASK) != 0)
		return (-1);

	if (screen == NULL)
		return (-1);

	if (offset < 0 ||
	    offset > screen->rinfo.ri_stride * screen->rinfo.ri_height)
		return (-1);

	return (bus_dmamem_mmap(sc->dma_tag, screen->segs, screen->nsegs,
	    offset, prot, BUS_DMA_WAITOK | BUS_DMA_COHERENT));
}
