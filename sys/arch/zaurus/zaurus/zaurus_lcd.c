/*	$OpenBSD: zaurus_lcd.c,v 1.4 2005/01/05 18:11:57 miod Exp $	*/
/* $NetBSD: lubbock_lcd.c,v 1.1 2003/08/09 19:38:53 bsh Exp $ */

/*
 * Copyright (c) 2002, 2003  Genetec Corporation.  All rights reserved.
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
 * LCD driver for Intel Lubbock.
 *
 * Controlling LCD is almost completely done through PXA2X0's
 * integrated LCD controller.  Codes for it is arm/xscale/pxa2x0_lcd.c.
 *
 * Codes in this file provide platform specific things including:
 *   LCD on/off switch in on-board PLD register.
 *   LCD panel geometry
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/malloc.h>

#include <dev/cons.h> 
#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h> 
#include <dev/wscons/wscons_callbacks.h>

#include <machine/bus.h>
#include <arm/xscale/pxa2x0var.h>
#include <arm/xscale/pxa2x0reg.h>
#include <arm/xscale/pxa2x0_lcd.h>

#include <machine/zaurus_reg.h>
#include <machine/zaurus_var.h>

int	lcd_match(struct device *, void *, void *);
void	lcd_attach(struct device *, struct device *, void *);
int	lcdintr(void *);

/*
 * wsdisplay glue
 */
struct pxa2x0_wsscreen_descr
lcd_bpp16_screen = {
	{
		"std" /* "bpp16" */
	},
	16				/* bits per pixel */
#ifdef notyet
}, lcd_bpp8_screen = {
	{
		"bpp8"
	},
	8				/* bits per pixel */
}, lcd_bpp4_screen = {
	{
		"bpp4"
	},
	4				/* bits per pixel */
#endif
};


static const struct wsscreen_descr *lcd_scr_descr[] = {
#ifdef notyet
	/* bpp4 needs a patch to rasops4 */
	&lcd_bpp4_screen.c,
	&lcd_bpp8_screen.c,
#endif
	&lcd_bpp16_screen.c,
};

const struct wsscreen_list lcd_screen_list = {
	sizeof lcd_scr_descr / sizeof lcd_scr_descr[0], lcd_scr_descr
};

void	lcd_burner(void *, u_int, u_int);
int	lcd_show_screen(void *, void *, int,
	    void (*)(void *, int, int), void *);

const struct wsdisplay_accessops lcd_accessops = {
	pxa2x0_lcd_ioctl,
	pxa2x0_lcd_mmap,
	pxa2x0_lcd_alloc_screen,
	pxa2x0_lcd_free_screen,
	lcd_show_screen,
	NULL,	/* load_font */
	NULL,	/* scrollback */
	NULL,	/* getchar */
	lcd_burner
};

struct cfattach lcd_obio_ca = {
	sizeof (struct pxa2x0_lcd_softc), lcd_match, lcd_attach
};
	 
struct cfdriver lcd_cd = {
	NULL, "lcd_obio", DV_DULL
};

int
lcd_match(struct device *parent, void *cf, void *aux)
{
	return 1;
}

#define CURRENT_DISPLAY &sharp_zaurus_C3000

static const struct lcd_panel_geometry sharp_zaurus_C3000 =
{
    480,			/* Width */
    640,			/* Height */
    0,				/* No extra lines */

    LCDPANEL_ACTIVE | LCDPANEL_VSP | LCDPANEL_HSP,
    1,				/* clock divider */
    0,				/* AC bias pin freq */

    0x27,			/* horizontal sync pulse width */
    0x2e,			/* BLW */
    0x7d,			/* ELW */

    2,				/* vertical sync pulse width */
    1,				/* BFW */
    0,				/* EFW */
};

void
lcd_attach(struct device *parent, struct device *self, void *aux)
{
	struct pxa2x0_lcd_softc *sc = (struct pxa2x0_lcd_softc *)self;
	struct wsemuldisplaydev_attach_args aa;

	pxa2x0_lcd_attach_sub(sc, aux, CURRENT_DISPLAY);

	/* make wsdisplay screen list */
	pxa2x0_lcd_setup_wsscreen(sc, &lcd_bpp16_screen, CURRENT_DISPLAY, NULL);
#ifdef notyet
	pxa2x0_lcd_setup_wsscreen(sc, &lcd_bpp8_screen, CURRENT_DISPLAY, NULL);
	pxa2x0_lcd_setup_wsscreen(sc, &lcd_bpp4_screen, CURRENT_DISPLAY, NULL);
#endif

	aa.console = 0;			/* XXX */
	aa.scrdata = &lcd_screen_list;
	aa.accessops = &lcd_accessops;
	aa.accesscookie = sc;

	printf("\n");

	(void)config_found(self, &aa, wsemuldisplaydevprint);
}

void
lcd_burner(void *v, u_int on, u_int flags)
{
	struct obio_softc *osc = 
	    (struct obio_softc *)((struct device *)v)->dv_parent;
	uint16_t reg;

	reg = bus_space_read_2(osc->sc_iot, osc->sc_obioreg_ioh,
	    LUBBOCK_MISCWR);
	if (on)
		reg |= MISCWR_LCDDISP;
	else
		reg &= ~MISCWR_LCDDISP;
	bus_space_write_2(osc->sc_iot, osc->sc_obioreg_ioh,
	    LUBBOCK_MISCWR, reg);
}

int
lcd_show_screen(void *v, void *cookie, int waitok,
    void (*cb)(void *, int, int), void *cbarg)
{
	int rc;

	if ((rc = pxa2x0_lcd_show_screen(v, cookie, waitok, cb, cbarg)) != 0)
		return (rc);
	
	/* Turn on LCD */
	lcd_burner(v, 1, 0);

	return (0);
}
