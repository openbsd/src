/*	$OpenBSD: palm_lcd.c,v 1.2 2009/09/09 11:34:02 marex Exp $	*/
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
 * LCD driver for Palm (based on the Intel Lubbock driver).
 *
 * Controlling LCD is almost completely done through PXA2X0's
 * integrated LCD controller.  Codes for it is arm/xscale/pxa2x0_lcd.c.
 *
 * Codes in this file provide platform specific things including:
 *   LCD on/off switch and backlight brightness
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
#include <machine/palm_var.h>
#include <arm/xscale/pxa2x0var.h>
#include <arm/xscale/pxa2x0reg.h>
#include <arm/xscale/pxa2x0_lcd.h>

#include <dev/rasops/rasops.h>

void	lcd_attach(struct device *, struct device *, void *);
int	lcd_match(struct device *, void *, void *);
int	lcd_cnattach(void (*)(u_int, int));

/*
 * wsdisplay glue
 */
struct pxa2x0_wsscreen_descr
lcd_bpp16_screen = {
	{
		"std"
	},
	16,				/* bits per pixel */
	0,				/* no rotation */
};

static const struct wsscreen_descr *lcd_scr_descr[] = {
	&lcd_bpp16_screen.c
};

const struct wsscreen_list lcd_screen_list = {
	sizeof lcd_scr_descr / sizeof lcd_scr_descr[0], lcd_scr_descr
};

int	lcd_ioctl(void *, u_long, caddr_t, int, struct proc *);
void	lcd_burner(void *, u_int, u_int);
int	lcd_show_screen(void *, void *, int,
	    void (*)(void *, int, int), void *);
const struct lcd_panel_geometry *lcd_geom_get(void);

int	lcd_param(struct pxa2x0_lcd_softc *, u_long,
    struct wsdisplay_param *);

const struct wsdisplay_accessops lcd_accessops = {
	lcd_ioctl,
	pxa2x0_lcd_mmap,
	pxa2x0_lcd_alloc_screen,
	pxa2x0_lcd_free_screen,
	lcd_show_screen,
	NULL,	/* load_font */
	NULL,	/* scrollback */
	NULL,	/* getchar */
	NULL
};

struct cfattach lcd_pxaip_ca = {
	sizeof (struct pxa2x0_lcd_softc), lcd_match, lcd_attach
};

struct cfdriver lcd_cd = {
	NULL, "lcd", DV_DULL
};

const struct lcd_panel_geometry palm_t5_lcd =
{
	324,			/* Width */
	484,			/* Height */
	0,			/* No extra lines */

	LCDPANEL_ACTIVE | LCDPANEL_VSP | LCDPANEL_HSP,
	2,			/* clock divider */
	0,			/* AC bias pin freq */

	0x03,			/* horizontal sync pulse width */
	0x1e,			/* BLW */
	0x03,			/* ELW */

	0x00,			/* vertical sync pulse width */
	0x05,			/* BFW */
	0x08,			/* EFW */
};

const struct lcd_panel_geometry palm_z72_lcd =
{
	324,			/* Width */
	324,			/* Height */
	0,			/* No extra lines */

	LCDPANEL_ACTIVE | LCDPANEL_VSP | LCDPANEL_HSP,
	2,			/* clock divider */
	0,			/* AC bias pin freq */

	0x03,			/* horizontal sync pulse width */
	0x1a,			/* BLW */
	0x03,			/* ELW */

	0x00,			/* vertical sync pulse width */
	0x05,			/* BFW */
	0x08,			/* EFW */
};

const struct lcd_panel_geometry palm_tc_lcd =
{
	320,			/* Width */
	320,			/* Height */
	0,			/* No extra lines */

	LCDPANEL_ACTIVE | LCDPANEL_VSP | LCDPANEL_HSP,
	2,			/* clock divider */
	0,			/* AC bias pin freq */

	0x03,			/* horizontal sync pulse width */
	0x1d,			/* BLW */
	0x09,			/* ELW */

	0x00,			/* vertical sync pulse width */
	0x06,			/* BFW */
	0x07,			/* EFW */
};


int
lcd_match(struct device *parent, void *cf, void *aux)
{
	return 1;
}

const struct lcd_panel_geometry *lcd_geom_get(void)
{
	if (mach_is_palmtc)
		return &palm_tc_lcd;
	else if (mach_is_palmz72)
		return &palm_z72_lcd;
	else
		return &palm_t5_lcd;
}

void
lcd_attach(struct device *parent, struct device *self, void *aux)
{
	struct pxa2x0_lcd_softc *sc = (struct pxa2x0_lcd_softc *)self;
	struct wsemuldisplaydev_attach_args aa;
	extern int glass_console;

	printf("\n");

	pxa2x0_lcd_attach_sub(sc, aux, &lcd_bpp16_screen, lcd_geom_get(),
		glass_console);

	aa.console = glass_console;
	aa.scrdata = &lcd_screen_list;
	aa.accessops = &lcd_accessops;
	aa.accesscookie = sc;
	aa.defaultscreens = 0;

	(void)config_found(self, &aa, wsemuldisplaydevprint);
}

int
lcd_cnattach(void (*clkman)(u_int, int))
{
	return
	    (pxa2x0_lcd_cnattach(&lcd_bpp16_screen, lcd_geom_get(), clkman));
}

/*
 * wsdisplay accessops overrides
 */

int
lcd_ioctl(void *v, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct pxa2x0_lcd_softc *sc = v;
	int res = EINVAL;

	switch (cmd) {
	case WSDISPLAYIO_GETPARAM:
	case WSDISPLAYIO_SETPARAM:
		res = lcd_param(sc, cmd, (struct wsdisplay_param *)data);
		break;
	}

	if (res == EINVAL)
		res = pxa2x0_lcd_ioctl(v, cmd, data, flag, p);

	return res;
}

int
lcd_show_screen(void *v, void *cookie, int waitok,
    void (*cb)(void *, int, int), void *cbarg)
{
	int rc;

	if ((rc = pxa2x0_lcd_show_screen(v, cookie, waitok, cb, cbarg)) != 0)
		return (rc);

	return (0);
}

/*
 * wsdisplay I/O controls
 */

int
lcd_param(struct pxa2x0_lcd_softc *sc, u_long cmd,
    struct wsdisplay_param *dp)
{
	int res = EINVAL;

	switch (dp->param) {
	case WSDISPLAYIO_PARAM_CONTRAST:
		/* unsupported */
		res = ENOTTY;
		break;
	}

	return res;
}
