/*	$OpenBSD: pxa2x0_lcd.h,v 1.4 2005/01/05 19:12:47 miod Exp $ */
/* $NetBSD: pxa2x0_lcd.h,v 1.2 2003/06/17 09:43:14 bsh Exp $ */
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


#ifndef _ARM_XSCALE_PXA2X0_LCD_H
#define _ARM_XSCALE_PXA2X0_LCD_H

#include <dev/rasops/rasops.h>
#include <machine/bus.h>

/* LCD Contoroller */

struct	lcd_dma_descriptor {
	uint32_t	fdadr;	/* next frame descriptor */
	uint32_t	fsadr;	/* frame start address */
	uint32_t	fidr;	/* frame ID */
	uint32_t	ldcmd;	/* DMA command */
#define	LDCMD_PAL	(1U<<26)	/* Pallet buffer */
#define LDCMD_SOFINT	(1U<<22)	/* Start of Frame interrupt */
#define LDCMD_EOFINT	(1U<<21)	/* End of Frame interrupt */
};


struct pxa2x0_lcd_screen {
	LIST_ENTRY(pxa2x0_lcd_screen) link;

	/* Frame buffer */
	bus_dmamap_t dma;
	bus_dma_segment_t segs[1];
	int 	nsegs;
	size_t  buf_size;
	size_t  map_size;
	void 	*buf_va;
	int	depth;

	/* DMA frame descriptor */
	struct	lcd_dma_descriptor *dma_desc;
	paddr_t	dma_desc_pa;

	/* rasterop */
	struct rasops_info rinfo;
};

struct pxa2x0_lcd_softc {
	struct device  dev;
	/* control register */
	bus_space_tag_t  	iot;
	bus_space_handle_t	ioh;
	bus_dma_tag_t		dma_tag;
	
	const struct lcd_panel_geometry *geometry;

	int n_screens;
	LIST_HEAD(, pxa2x0_lcd_screen) screens;
	struct pxa2x0_lcd_screen *active;
	void *ih;			/* interrupt handler */
};

void pxa2x0_lcd_attach_sub(struct pxa2x0_lcd_softc *, struct pxaip_attach_args *,
			   const struct lcd_panel_geometry *);
void pxa2x0_lcd_start_dma(struct pxa2x0_lcd_softc *, struct pxa2x0_lcd_screen *);

struct lcd_panel_geometry {
	short panel_width;
	short panel_height;
	short extra_lines;

	short panel_info;
#define LCDPANEL_VSP	(1<<0)		/* L_FCLK pin is active low */
#define LCDPANEL_HSP	(1<<1)		/* L_LCLK pin is active low */
#define LCDPANEL_PCP	(1<<2)		/* use L_PCLK falling edge */
#define LCDPANEL_OEP	(1<<3)		/* L_BIAS pin is active low */
#define LCDPANEL_DPC	(1<<4)		/* double pixel clock mode */

#define LCDPANEL_DUAL	(1<<5)		/* Dual or single */
#define LCDPANEL_SINGLE 0
#define LCDPANEL_ACTIVE	(1<<6)		/* Active or Passive */
#define LCDPANEL_PASSIVE 0
#define LCDPANEL_MONOCHROME (1<<7)	/* depth=1 */

	short pixel_clock_div;		/* pixel clock divider */
	short ac_bias;			/* AC bias pin frequency */

	short hsync_pulse_width;	/* Horizontao sync pulse width */
	short beg_line_wait;		/* beginning of line wait (BLW) */
	short end_line_wait;		/* end of line pxel wait (ELW) */

	short vsync_pulse_width;	/* vertical sync pulse width */
	short beg_frame_wait;		/* beginning of frame wait (BFW) */
	short end_frame_wait;		/* end of frame wait (EFW) */
};

void pxa2x0_lcd_geometry(struct pxa2x0_lcd_softc *,
    const struct lcd_panel_geometry *);
struct pxa2x0_lcd_screen *pxa2x0_lcd_new_screen(struct pxa2x0_lcd_softc *, int);

/*
 * we need bits-per-pixel value to configure wsdisplay screen
 */
struct pxa2x0_wsscreen_descr {
	struct wsscreen_descr  c;	/* standard descriptor */
	int depth;			/* bits per pixel */
};

int pxa2x0_lcd_setup_wsscreen(struct pxa2x0_wsscreen_descr *,
    const struct lcd_panel_geometry *, const char * );

int pxa2x0_lcd_show_screen(void *, void *, int, void (*)(void *, int, int),
    void *);
int pxa2x0_lcd_ioctl(void *, u_long, caddr_t, int, struct proc *);
paddr_t	pxa2x0_lcd_mmap(void *, off_t, int);
int pxa2x0_lcd_alloc_screen(void *, const struct wsscreen_descr *,
    void **, int *, int *, long *);
void pxa2x0_lcd_free_screen(void *, void *);

extern const struct wsdisplay_emulops pxa2x0_lcd_emulops;

#endif /* _ARM_XSCALE_PXA2X0_LCD_H */
