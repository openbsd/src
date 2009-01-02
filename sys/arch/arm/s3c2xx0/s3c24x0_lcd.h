/* $NetBSD: s3c24x0_lcd.h,v 1.4 2007/03/04 05:59:38 christos Exp $ */

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


#ifndef _ARM_S3C2XX0_S3C24X0_LCD_H
#define	_ARM_S3C2XX0_S3C24X0_LCD_H

#include <dev/rasops/rasops.h>
#include <machine/bus.h>

/* LCD Contoroller */

struct s3c24x0_lcd_screen {
	LIST_ENTRY(s3c24x0_lcd_screen) link;

	/* Frame buffer */
	bus_dmamap_t dma;
	bus_dma_segment_t segs[1];
	int 	nsegs;
	size_t  buf_size;
	size_t  map_size;
	void 	*buf_va;
	int	depth;
	int	stride;

	/* rasterop */
	struct rasops_info rinfo;
};

struct s3c24x0_lcd_softc {
	struct device  dev;

	/* control registers */
	bus_space_tag_t  	iot;
	bus_space_handle_t	ioh;
	bus_dma_tag_t    	dma_tag;
	
	struct s3c24x0_lcd_panel_info *panel_info;

	LIST_HEAD(, s3c24x0_lcd_screen) screens;
	struct s3c24x0_lcd_screen *active;
	void *ih;			/* interrupt handler */

	int n_screens;
	int lcd_on;			/* LCD is turned on */
};

void s3c24x0_lcd_attach_sub(struct s3c24x0_lcd_softc *,
    struct s3c2xx0_attach_args *, struct s3c24x0_lcd_panel_info *);
int s3c24x0_lcd_start_dma(struct s3c24x0_lcd_softc *,
    struct s3c24x0_lcd_screen *);

struct s3c24x0_lcd_panel_info {
	short panel_width;
	short panel_height;
	int   pixel_clock;		/* in Hz */

	/* Initial values to go to LCD controll registers */
	uint32_t lcdcon1;
	uint32_t lcdcon2;
	uint32_t lcdcon3;
	uint32_t lcdcon4;
	uint32_t lcdcon5;
	uint32_t lpcsel;

#define	s3c24x0_lcd_panel_tft(info)	\
	(((info)->lcdcon1 & LCDCON1_PNRMODE_MASK) == LCDCON1_PNRMODE_TFT)
};

void s3c24x0_set_lcd_panel_info(struct s3c24x0_lcd_softc *,
    struct s3c24x0_lcd_panel_info *);

struct s3c24x0_lcd_screen *s3c24x0_lcd_new_screen(struct s3c24x0_lcd_softc *,
    int, int, int);

/*
 * we need bits-per-pixel value to configure wsdisplay screen
 */
struct s3c24x0_wsscreen_descr {
	struct wsscreen_descr  c;	/* standard descriptor */
	int depth;			/* bits per pixel */
};

int s3c24x0_lcd_show_screen(void *, void *, int, void (*)(void *, int, int),
    void *);
int s3c24x0_lcd_ioctl(void *v, u_long cmd, caddr_t data, int flag,
    struct proc *p);
paddr_t	s3c24x0_lcd_mmap(void *, off_t, int);
int s3c24x0_lcd_alloc_screen(void *, const struct wsscreen_descr *, void **,
    int *, int *, long *);
void s3c24x0_lcd_free_screen(void *, void *);
void s3c24x0_lcd_power(struct s3c24x0_lcd_softc *, int);

extern const struct wsdisplay_emulops s3c24x0_lcd_emulops;
    
#endif /* _ARM_S3C2XX0_S3C24X0_LCD_H */
