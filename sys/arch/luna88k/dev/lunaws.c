/*	$OpenBSD: lunaws.c,v 1.10 2014/01/26 17:48:07 miod Exp $	*/
/* $NetBSD: lunaws.c,v 1.6 2002/03/17 19:40:42 atatat Exp $ */

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Tohru Nishimura.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/device.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wskbdvar.h>
#include <dev/wscons/wsksymdef.h>
#include <dev/wscons/wsksymvar.h>
#ifdef WSDISPLAY_COMPAT_RAWKBD
#include <dev/wscons/wskbdraw.h>
#endif
#include "wsmouse.h"
#if NWSMOUSE > 0
#include <dev/wscons/wsmousevar.h>
#endif

#include <luna88k/dev/omkbdmap.h>
#include <luna88k/dev/sioreg.h>
#include <luna88k/dev/siovar.h>

static const u_int8_t ch1_regs[6] = {
	WR0_RSTINT,				/* Reset E/S Interrupt */
	WR1_RXALLS,				/* Rx per char, No Tx */
	0,					/* */
	WR3_RX8BIT | WR3_RXENBL,		/* Rx */
	WR4_BAUD96 | WR4_STOP1 | WR4_NPARITY,	/* Tx/Rx */
	WR5_TX8BIT | WR5_TXENBL,		/* Tx */
};

struct ws_softc {
	struct device	sc_dv;
	struct sioreg	*sc_ctl;
	u_int8_t	sc_wr[6];
	struct device	*sc_wskbddev;
#if NWSMOUSE > 0
	struct device	*sc_wsmousedev;
	int		sc_msreport;
	int		buttons, dx, dy;
#endif

#ifdef WSDISPLAY_COMPAT_RAWKBD
	int		sc_rawkbd;
#endif
};

void omkbd_input(void *, int);
void omkbd_decode(void *, int, u_int *, int *);
int  omkbd_enable(void *, int);
void omkbd_set_leds(void *, int);
int  omkbd_ioctl(void *, u_long, caddr_t, int, struct proc *);

const struct wskbd_mapdata omkbd_keymapdata = {
	omkbd_keydesctab,
#ifdef	OMKBD_LAYOUT
	OMKBD_LAYOUT,
#else
	KB_JP | KB_DEFAULT,
#endif
};

const struct wskbd_accessops omkbd_accessops = {
	omkbd_enable,
	omkbd_set_leds,
	omkbd_ioctl,
};

void	ws_cnattach(void);
void ws_cngetc(void *, u_int *, int *);
void ws_cnpollc(void *, int);
const struct wskbd_consops ws_consops = {
	ws_cngetc,
	ws_cnpollc,
	NULL	/* bell */
};

#if NWSMOUSE > 0
int  omms_enable(void *);
int  omms_ioctl(void *, u_long, caddr_t, int, struct proc *);
void omms_disable(void *);

const struct wsmouse_accessops omms_accessops = {
	omms_enable,
	omms_ioctl,
	omms_disable,
};
#endif

void wsintr(int);

int  wsmatch(struct device *, void *, void *);
void wsattach(struct device *, struct device *, void *);
int  ws_submatch_kbd(struct device *, void *, void *);
#if NWSMOUSE > 0
int  ws_submatch_mouse(struct device *, void *, void *);
#endif

const struct cfattach ws_ca = {
	sizeof(struct ws_softc), wsmatch, wsattach
};

struct cfdriver ws_cd = {
        NULL, "ws", DV_TTY
};

extern int  syscngetc(dev_t);
extern void syscnputc(dev_t, int);

int
wsmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct sio_attach_args *args = aux;

	if (args->channel != 1)
		return 0;
	return 1;
}

void
wsattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct ws_softc *sc = (struct ws_softc *)self;
	struct sio_softc *scp = (struct sio_softc *)parent;
	struct sio_attach_args *args = aux;
	struct wskbddev_attach_args a;

	sc->sc_ctl = (struct sioreg *)scp->scp_ctl + 1;
	bcopy(ch1_regs, sc->sc_wr, sizeof(ch1_regs));
	scp->scp_intr[1] = wsintr;
	
	setsioreg(sc->sc_ctl, WR0, sc->sc_wr[WR0]);
	setsioreg(sc->sc_ctl, WR4, sc->sc_wr[WR4]);
	setsioreg(sc->sc_ctl, WR3, sc->sc_wr[WR3]);
	setsioreg(sc->sc_ctl, WR5, sc->sc_wr[WR5]);
	setsioreg(sc->sc_ctl, WR0, sc->sc_wr[WR0]);
	setsioreg(sc->sc_ctl, WR1, sc->sc_wr[WR1]);

	syscnputc((dev_t)1, 0x20); /* keep quiet mouse */

	printf("\n");

	a.console = (args->hwflags == 1);
	a.keymap = &omkbd_keymapdata;
	a.accessops = &omkbd_accessops;
	a.accesscookie = (void *)sc;
	sc->sc_wskbddev = config_found_sm(self, &a, wskbddevprint,
					ws_submatch_kbd);

#if NWSMOUSE > 0
	{
	struct wsmousedev_attach_args b;
	b.accessops = &omms_accessops;
	b.accesscookie = (void *)sc;	
	sc->sc_wsmousedev = config_found_sm(self, &b, wsmousedevprint,
					ws_submatch_mouse);
	sc->sc_msreport = 0;
	}
#endif
}

int
ws_submatch_kbd(parent, match, aux)
        struct device *parent;
        void *match, *aux;
{
	struct cfdata *cf = match;

        if (strcmp(cf->cf_driver->cd_name, "wskbd"))
                return (0);
        return ((*cf->cf_attach->ca_match)(parent, cf, aux));
}

#if NWSMOUSE > 0

int
ws_submatch_mouse(parent, match, aux)
        struct device *parent;
        void *match, *aux;
{
	struct cfdata *cf = match;

        if (strcmp(cf->cf_driver->cd_name, "wsmouse"))
                return (0);
        return ((*cf->cf_attach->ca_match)(parent, cf, aux));
}

#endif

/*ARGSUSED*/
void
wsintr(chan)
	int chan;
{
	struct ws_softc *sc = ws_cd.cd_devs[0];
	struct sioreg *sio = sc->sc_ctl;
	u_int code;
	int rr;

	rr = getsiocsr(sio);
	if (rr & RR_RXRDY) {
		do {
			code = sio->sio_data;
			if (rr & (RR_FRAMING | RR_OVERRUN | RR_PARITY)) {
				sio->sio_cmd = WR0_ERRRST;
				continue;
			}
#if NWSMOUSE > 0
			/*
			 * if (code >= 0x80 && code <= 0x87), then
			 * it's the first byte of 3 byte long mouse report
			 * 	code[0] & 07 -> LMR button condition
			 *	code[1], [2] -> x,y delta
			 * otherwise, key press or release event.
			 */
			if (sc->sc_msreport == 0) {
				if (code < 0x80 || code > 0x87) {
					omkbd_input(sc, code);
					continue;
				}
				code = (code & 07) ^ 07;
				/* LMR->RML: wsevent counts 0 for leftmost */
				sc->buttons = (code & 02);
				if (code & 01)
					sc->buttons |= 04;
				if (code & 04)
					sc->buttons |= 01;
				sc->sc_msreport = 1;
			}
			else if (sc->sc_msreport == 1) {
				sc->dx = (signed char)code;
				sc->sc_msreport = 2;
			}
			else if (sc->sc_msreport == 2) {
				sc->dy = (signed char)code;
				if (sc->sc_wsmousedev != NULL)
					wsmouse_input(sc->sc_wsmousedev,
					    sc->buttons, sc->dx, sc->dy, 0, 0,
					    WSMOUSE_INPUT_DELTA);
				sc->sc_msreport = 0;
			}
#else
			omkbd_input(sc, code);
#endif
		} while ((rr = getsiocsr(sio)) & RR_RXRDY);
	}
	if (rr & RR_TXRDY)
		sio->sio_cmd = WR0_RSTPEND;
	/* not capable of transmit, yet */
}

void
omkbd_input(v, data)
	void *v;
	int data;
{
	struct ws_softc *sc = v;
	u_int type;
	int key;

	omkbd_decode(v, data, &type, &key);

#if WSDISPLAY_COMPAT_RAWKBD
	if (sc->sc_rawkbd) {
		u_char cbuf[2];
		int c, j = 0;

		c = omkbd_raw[key];
		if (c != RAWKEY_Null) {
			/* fake extended scancode if necessary */
			if (c & 0x80)
				cbuf[j++] = 0xe0;
			cbuf[j] = c & 0x7f;
			if (type == WSCONS_EVENT_KEY_UP)
				cbuf[j] |= 0x80;
			j++;

			wskbd_rawinput(sc->sc_wskbddev, cbuf, j);
		}
	} else
#endif
	{
		if (sc->sc_wskbddev != NULL)
			wskbd_input(sc->sc_wskbddev, type, key);	
	}
}

void
omkbd_decode(v, datain, type, dataout)
	void *v;
	int datain;
	u_int *type;
	int *dataout;
{
	*type = (datain & 0x80) ? WSCONS_EVENT_KEY_UP : WSCONS_EVENT_KEY_DOWN;
	*dataout = datain & 0x7f;
}

void
ws_cngetc(v, type, data)
	void *v;
	u_int *type;
	int *data;
{
	int code;

	code = syscngetc((dev_t)1);
	omkbd_decode(v, code, type, data);
}

void
ws_cnpollc(v, on)
	void *v;
        int on;
{
}

/* EXPORT */ void
ws_cnattach()
{
	static int voidfill;

	/* XXX need CH.B initialization XXX */

	wskbd_cnattach(&ws_consops, &voidfill, &omkbd_keymapdata);
}

int
omkbd_enable(v, on)
	void *v;
	int on;
{
	return 0;
}

void
omkbd_set_leds(v, leds)
	void *v;
	int leds;
{
#if 0
	syscnputc((dev_t)1, 0x10); /* kana LED on */
	syscnputc((dev_t)1, 0x00); /* kana LED off */
	syscnputc((dev_t)1, 0x11); /* caps LED on */
	syscnputc((dev_t)1, 0x01); /* caps LED off */
#endif
}

int
omkbd_ioctl(v, cmd, data, flag, p)
	void *v;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
#if WSDISPLAY_COMPAT_RAWKBD
	struct ws_softc *sc = v;
#endif

	switch (cmd) {
	case WSKBDIO_GTYPE:
		*(int *)data = WSKBD_TYPE_LUNA;
		return 0;
	case WSKBDIO_SETLEDS:
	case WSKBDIO_GETLEDS:
	case WSKBDIO_COMPLEXBELL:	/* XXX capable of complex bell */
		return -1;
#if WSDISPLAY_COMPAT_RAWKBD
	case WSKBDIO_SETMODE:
		sc->sc_rawkbd = *(int *)data == WSKBD_RAW;
		return 0;
	case WSKBDIO_GETMODE:
		*(int *)data = sc->sc_rawkbd;
		return 0;
#endif
	}
	return -1;
}

#if NWSMOUSE > 0

int
omms_enable(v)
	void *v;
{
	struct ws_softc *sc = v;

	syscnputc((dev_t)1, 0x60); /* enable 3 byte long mouse reporting */
	sc->sc_msreport = 0;
	return 0;
}

/*ARGUSED*/
int
omms_ioctl(v, cmd, data, flag, p)
	void *v;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
#if 0
	struct ws_softc *sc = v;
#endif

	switch (cmd) {
	case WSMOUSEIO_GTYPE:
		*(u_int *)data = WSMOUSE_TYPE_LUNA;
		return 0;
	}

	return -1;
}

void
omms_disable(v)
	void *v;
{
	struct ws_softc *sc = v;

	syscnputc((dev_t)1, 0x20); /* quiet mouse */
	sc->sc_msreport = 0;
}
#endif
