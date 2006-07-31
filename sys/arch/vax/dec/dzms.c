/*	$OpenBSD: dzms.c,v 1.6 2006/07/31 18:50:13 miod Exp $	*/
/*	$NetBSD: dzms.c,v 1.1 2000/12/02 17:03:55 ragge Exp $	*/

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
 *	@(#)ms.c	8.1 (Berkeley) 6/11/93
 */

/*
 * VSXXX mice attached to line 1 of the DZ*
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/tty.h>

#include <machine/bus.h>

#include <vax/qbus/dzreg.h>
#include <vax/qbus/dzvar.h>

#include <vax/dec/dzkbdvar.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsmousevar.h>

/*
 * Command characters for the mouse.
 */
#define MOUSE_SELF_TEST		'T'
#define MOUSE_INCREMENTAL	'R'

/*
 * Mouse output bits.
 *
 *     	MOUSE_START_FRAME	Start of report frame bit.
 *	MOUSE_X_SIGN		Sign bit for X.
 *	MOUSE_Y_SIGN		Sign bit for Y.
 *	MOUSE_X_OFFSET		X offset to start cursor at.
 *	MOUSE_Y_OFFSET		Y offset to start cursor at.
 */
#define MOUSE_START_FRAME	0x80
#define MOUSE_X_SIGN		0x10
#define MOUSE_Y_SIGN		0x08

/*
 * Definitions for mouse buttons
 */
#define RIGHT_BUTTON		0x01
#define MIDDLE_BUTTON		0x02
#define LEFT_BUTTON		0x04

struct dzms_softc {		/* driver status information */
	struct	device dzms_dev;	/* required first: base device */
	struct	dz_linestate *dzms_ls;

	int sc_enabled;		/* input enabled? */
	int sc_selftest;

	int inputstate;
	u_int buttons;
	int dx, dy;

	struct device *sc_wsmousedev;
};

int	dzms_match(struct device *, struct cfdata *, void *);
void	dzms_attach(struct device *, struct device *, void *);

struct cfattach dzms_ca = {
	sizeof(struct dzms_softc), (cfmatch_t)dzms_match, dzms_attach,
};

struct	cfdriver lkms_cd = {
	NULL, "lkms", DV_DULL
};

int	dzms_enable(void *);
int	dzms_ioctl(void *, u_long, caddr_t, int, struct proc *);
void	dzms_disable(void *);

const struct wsmouse_accessops dzms_accessops = {
	dzms_enable,
	dzms_ioctl,
	dzms_disable,
};

int	dzms_input(void *, int);

int
dzms_match(struct device *parent, struct cfdata *cf, void *aux)
{
	struct dzkm_attach_args *daa = aux;

#define DZCF_LINE 0
#define DZCF_LINE_DEFAULT 1

	/* Exact match is better than wildcard. */
	if (cf->cf_loc[DZCF_LINE] == daa->daa_line)
		return 2;

	/* This driver accepts wildcard. */
	if (cf->cf_loc[DZCF_LINE] == DZCF_LINE_DEFAULT)
		return 1;

	return 0;
}

void
dzms_attach(struct device *parent, struct device *self, void *aux)
{
	struct dz_softc *dz = (void *)parent;
	struct dzms_softc *dzms = (void *)self;
	struct dzkm_attach_args *daa = aux;
	struct dz_linestate *ls;
	struct wsmousedev_attach_args a;

	dz->sc_dz[daa->daa_line].dz_catch = dzms_input;
	dz->sc_dz[daa->daa_line].dz_private = dzms;
	ls = &dz->sc_dz[daa->daa_line];
	dzms->dzms_ls = ls;

	printf("\n");

	a.accessops = &dzms_accessops;
	a.accesscookie = dzms;

	dzms->sc_enabled = 0;
	dzms->sc_selftest = 0;
	dzms->sc_wsmousedev = config_found(self, &a, wsmousedevprint);
}

int
dzms_enable(void *v)
{
	struct dzms_softc *sc = v;

	if (sc->sc_enabled)
		return EBUSY;

	sc->sc_selftest = 4;	/* wait for 4 byte reply upto 1/2 sec */
	dzputc(sc->dzms_ls, MOUSE_SELF_TEST);
	(void)tsleep(dzms_enable, TTIPRI, "dzmsopen", hz / 2);
	if (sc->sc_selftest != 0) {
		sc->sc_selftest = 0;
		return ENXIO;
	}
	DELAY(150);
	dzputc(sc->dzms_ls, MOUSE_INCREMENTAL);
	sc->sc_enabled = 1;
	sc->inputstate = 0;
	return 0;
}

void
dzms_disable(void *v)
{
	struct dzms_softc *sc = v;

	sc->sc_enabled = 0;
}

int
dzms_ioctl(void *v, u_long cmd, caddr_t data, int flag, struct proc *p)
{
#if 0
	struct dzms_softc *sc = v;
#endif

	switch (cmd) {
	case WSMOUSEIO_GTYPE:
		*(int *)data = WSMOUSE_TYPE_VSXXX;
		return 0;
	}

	return -1;
}

int
dzms_input(void *vsc, int data)
{
	struct dzms_softc *sc = vsc;

	if (!sc->sc_enabled) {
		if (sc->sc_selftest > 0) {
			sc->sc_selftest--;
			if (sc->sc_selftest == 0)
				wakeup(dzms_enable);
		}
		return (1);
	}

#define WSMS_BUTTON1    0x01
#define WSMS_BUTTON2    0x02
#define WSMS_BUTTON3    0x04

	if ((data & MOUSE_START_FRAME) != 0)
		sc->inputstate = 1;
	else
		sc->inputstate++;

	if (sc->inputstate == 1) {
		sc->buttons = 0;
		if ((data & LEFT_BUTTON) != 0)
			sc->buttons |= WSMS_BUTTON1;
		if ((data & MIDDLE_BUTTON) != 0)
			sc->buttons |= WSMS_BUTTON2;
		if ((data & RIGHT_BUTTON) != 0)
			sc->buttons |= WSMS_BUTTON3;
	    
		sc->dx = data & MOUSE_X_SIGN;
		sc->dy = data & MOUSE_Y_SIGN;
	} else if (sc->inputstate == 2) {
		if (sc->dx == 0)
			sc->dx = -data;
		else
			sc->dx = data;
	} else if (sc->inputstate == 3) {
		sc->inputstate = 0;
		if (sc->dy == 0)
			sc->dy = -data;
		else
			sc->dy = data;
		wsmouse_input(sc->sc_wsmousedev, sc->buttons,
		    sc->dx, sc->dy, 0, WSMOUSE_INPUT_DELTA);
	}

	return (1);
}
