/*	$OpenBSD: dzkbd.c,v 1.14 2008/08/20 16:31:41 miod Exp $	*/
/*	$NetBSD: dzkbd.c,v 1.1 2000/12/02 17:03:55 ragge Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)kbd.c	8.2 (Berkeley) 10/30/93
 */

/*
 * LK200/LK400 keyboard attached to line 0 of the DZ*-11
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/malloc.h>
#include <sys/timeout.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wskbdvar.h>
#include <dev/wscons/wsksymdef.h>
#include <dev/wscons/wsksymvar.h>
#include <vax/dec/wskbdmap_lk201.h>

#include <machine/bus.h> 

#include <vax/qbus/dzreg.h>
#include <vax/qbus/dzvar.h>

#include <vax/dec/dzkbdvar.h>
#include <vax/dec/lk201reg.h>
#include <vax/dec/lk201var.h>

struct dzkbd_internal {
	struct dz_linestate *dzi_ls;
	struct lk201_state dzi_ks;
};

struct dzkbd_internal dzkbd_console_internal;

struct dzkbd_softc {
	struct device dzkbd_dev;	/* required first: base device */

	struct dzkbd_internal *sc_itl;
	int sc_enabled;
	struct device *sc_wskbddev;
};

int	dzkbd_match(struct device *, struct cfdata *, void *);
void	dzkbd_attach(struct device *, struct device *, void *);

struct cfattach dzkbd_ca = {
	sizeof(struct dzkbd_softc), (cfmatch_t)dzkbd_match, dzkbd_attach,
};

int	dzkbd_enable(void *, int);
void	dzkbd_set_leds(void *, int);
int	dzkbd_ioctl(void *, u_long, caddr_t, int, struct proc *);

const struct wskbd_accessops dzkbd_accessops = {
	dzkbd_enable,
	dzkbd_set_leds,
	dzkbd_ioctl,
};

void	dzkbd_cngetc(void *, u_int *, int *);
void	dzkbd_cnpollc(void *, int);

const struct wskbd_consops dzkbd_consops = {
	dzkbd_cngetc,
	dzkbd_cnpollc,
};

const struct wskbd_mapdata dzkbd_keymapdata = {
	lkkbd_keydesctab,
#ifdef LKKBD_LAYOUT
	LKKBD_LAYOUT,
#else
	KB_US,
#endif
};

int	dzkbd_input(void *, int);
int	dzkbd_sendchar(void *, int);

/*
 * kbd_match: how is this dz line configured?
 */
int
dzkbd_match(struct device *parent, struct cfdata *cf, void *aux)
{
	struct dzkm_attach_args *daa = aux;

#define DZCF_LINE 0
#define DZCF_LINE_DEFAULT 0

	/* Exact match is better than wildcard. */
	if (cf->cf_loc[DZCF_LINE] == daa->daa_line)
		return 2;

	/* This driver accepts wildcard. */
	if (cf->cf_loc[DZCF_LINE] == DZCF_LINE_DEFAULT)
		return 1;

	return 0;
}

void
dzkbd_attach(struct device *parent, struct device *self, void *aux)
{
	struct dz_softc *dz = (void *)parent;
	struct dzkbd_softc *dzkbd = (void *)self;
	struct dzkm_attach_args *daa = aux;
	struct dz_linestate *ls;
	struct dzkbd_internal *dzi;
	struct wskbddev_attach_args a;
	int isconsole;

	dz->sc_dz[daa->daa_line].dz_catch = dzkbd_input;
	dz->sc_dz[daa->daa_line].dz_private = dzkbd;
	ls = &dz->sc_dz[daa->daa_line];

	isconsole = (daa->daa_flags & DZKBD_CONSOLE);

	if (isconsole) {
		dzi = &dzkbd_console_internal;
		dzkbd->sc_enabled = 1;
	} else {
		dzi = malloc(sizeof(struct dzkbd_internal), M_DEVBUF, M_NOWAIT);
		if (dzi == NULL) {
			printf(": out of memory\n");
			return;
		}
		dzi->dzi_ks.attmt.sendchar = dzkbd_sendchar;
		dzi->dzi_ks.attmt.cookie = ls;
	}
	dzi->dzi_ks.device = self;
	dzi->dzi_ls = ls;
	dzkbd->sc_itl = dzi;

	printf("\n");

	if (!isconsole)
		lk201_init(&dzi->dzi_ks);

	a.console = dzi == &dzkbd_console_internal;
	a.keymap = &dzkbd_keymapdata;
	a.accessops = &dzkbd_accessops;
	a.accesscookie = dzkbd;

	dzkbd->sc_wskbddev = config_found(self, &a, wskbddevprint);
}

int
dzkbd_cnattach()
{
	/*
	 * Early operation (especially keyboard initialization)
	 * requires the help of the serial console routines, which
	 * need to be initialized to work with the keyboard line.
	 */
	dzcninit_internal(0, 1);

	dzkbd_console_internal.dzi_ks.attmt.sendchar = dzkbd_sendchar;
	dzkbd_console_internal.dzi_ks.attmt.cookie = NULL;
	lk201_init(&dzkbd_console_internal.dzi_ks);
	dzkbd_console_internal.dzi_ls = NULL;

	wskbd_cnattach(&dzkbd_consops, &dzkbd_console_internal,
	    &dzkbd_keymapdata);

	return 0;
}

int
dzkbd_enable(void *v, int on)
{
	struct dzkbd_softc *sc = v;

	sc->sc_enabled = on;
	return 0;
}

void
dzkbd_cngetc(void *v, u_int *type, int *data)
{
	struct dzkbd_internal *dzi = v;
#if 0
	int line = dzi->dzi_ls != NULL ? dzi->dzi_ls->dz_line : 0;
#else
	int line = 0;	/* keyboard */
#endif
	int c, s;

	do {
		s = spltty();
		c = dzcngetc_internal(line);
		splx(s);
	} while (lk201_decode(&dzi->dzi_ks, 1, 0, c, type, data) == LKD_NODATA);
}

void
dzkbd_cnpollc(void *v, int on)
{
#if 0
	struct dzkbd_internal *dzi = v;
#endif
}

void
dzkbd_set_leds(void *v, int leds)
{
	struct dzkbd_softc *sc = (struct dzkbd_softc *)v;

	lk201_set_leds(&sc->sc_itl->dzi_ks, leds);
}

int
dzkbd_ioctl(void *v, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct dzkbd_softc *sc = (struct dzkbd_softc *)v;

	switch (cmd) {
	case WSKBDIO_GTYPE:
		*(int *)data = lk201_get_type(&sc->sc_itl->dzi_ks);
		return 0;
	case WSKBDIO_SETLEDS:
		lk201_set_leds(&sc->sc_itl->dzi_ks, *(int *)data);
		return 0;
	case WSKBDIO_GETLEDS:
		*(int *)data = lk201_get_leds(&sc->sc_itl->dzi_ks);
		return 0;
	case WSKBDIO_COMPLEXBELL:
		lk201_bell(&sc->sc_itl->dzi_ks,
			   (struct wskbd_bell_data *)data);
		return 0;
	}
	return -1;
}

int
dzkbd_input(void *v, int data)
{
	struct dzkbd_softc *sc = (struct dzkbd_softc *)v;
	u_int type;
	int val;
	int decode;

	/*
	 * We want to run through lk201_decode always, so that a late plugged
	 * keyboard will get configured correctly.
	 */
	do {
		decode = lk201_decode(&sc->sc_itl->dzi_ks, sc->sc_enabled, 1,
		    data, &type, &val);
		if (decode != LKD_NODATA)
			wskbd_input(sc->sc_wskbddev, type, val);
	} while (decode == LKD_MORE);

	return(1);
}

int
dzkbd_sendchar(void *v, int c)
{
	dzputc((struct dz_linestate *)v, c);
	return (0);
}
