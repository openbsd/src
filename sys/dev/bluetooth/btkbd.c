/*	$OpenBSD: btkbd.c,v 1.7 2010/08/05 13:13:17 miod Exp $	*/
/*	$NetBSD: btkbd.c,v 1.10 2008/09/09 03:54:56 cube Exp $	*/

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at
 * Carlstedt Research & Technology.
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

/*-
 * Copyright (c) 2006 Itronix Inc.
 * All rights reserved.
 *
 * Written by Iain Hibbert for Itronix Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of Itronix Inc. may not be used to endorse
 *    or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ITRONIX INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL ITRONIX INC. BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/systm.h>

#include <netbt/bluetooth.h>

#include <dev/bluetooth/bthid.h>
#include <dev/bluetooth/bthidev.h>

#include <dev/usb/hid.h>
#include <dev/usb/usb.h>
#include <dev/usb/usbhid.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wskbdvar.h>
#include <dev/wscons/wsksymdef.h>
#include <dev/wscons/wsksymvar.h>

#include <dev/usb/hidkbdsc.h>
#include <dev/usb/hidkbdvar.h>

struct btkbd_softc {
	struct bthidev		 sc_hidev;	/* device */
	struct hidkbd		 sc_kbd;	/* keyboard state */
	int			(*sc_output)	/* output method */
				(struct bthidev *, uint8_t *, int, int);
	int			 sc_inintr;
};

/* autoconf(9) methods */
int	btkbd_match(struct device *, void *, void *);
void	btkbd_attach(struct device *, struct device *, void *);
int	btkbd_detach(struct device *, int);

struct cfdriver btkbd_cd = {
	NULL, "btkbd", DV_DULL
};

const struct cfattach btkbd_ca = {
	sizeof(struct btkbd_softc),
	btkbd_match,
	btkbd_attach,
	btkbd_detach,
	/* XXX activate */
};

/* wskbd(4) accessops */
int	btkbd_enable(void *, int);
void	btkbd_set_leds(void *, int);
int	btkbd_ioctl(void *, u_long, caddr_t, int, struct proc *);

const struct wskbd_accessops btkbd_accessops = {
	btkbd_enable,
	btkbd_set_leds,
	btkbd_ioctl
};

/* bthid methods */
void	btkbd_input(struct bthidev *, uint8_t *, int);

int
btkbd_match(struct device *self, void *match, void *aux)
{
	struct bthidev_attach_args *ba = aux;

	if (hid_is_collection(ba->ba_desc, ba->ba_dlen, ba->ba_id,
	    HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_KEYBOARD)))
		return 1;

	return 0;
}

void
btkbd_attach(struct device *parent, struct device *self, void *aux)
{
	struct btkbd_softc *sc = (struct btkbd_softc *)self;
	struct hidkbd *kbd = &sc->sc_kbd;
	struct bthidev_attach_args *ba = aux;
	kbd_t layout;

	sc->sc_output = ba->ba_output;
	ba->ba_input = btkbd_input;			/* XXX ugly */

	if (hidkbd_attach(self, kbd, 0, 0,
	    ba->ba_id, ba->ba_desc, ba->ba_dlen) != 0)
		return;

	printf("\n");

#if defined(BTKBD_LAYOUT)
	layout = BTKBD_LAYOUT;
#else
	layout = KB_US;
#endif
	hidkbd_attach_wskbd(kbd, layout, &btkbd_accessops);
}

int
btkbd_detach(struct device *self, int flags)
{
	struct btkbd_softc *sc = (struct btkbd_softc *)self;

	return hidkbd_detach(&sc->sc_kbd, flags);
}

int
btkbd_enable(void *self, int on)
{
	struct btkbd_softc *sc = (struct btkbd_softc *)self;
	struct hidkbd *kbd = &sc->sc_kbd;

	return hidkbd_enable(kbd, on);
}

void
btkbd_set_leds(void *self, int leds)
{
	struct btkbd_softc *sc = (struct btkbd_softc *)self;
	struct hidkbd *kbd = &sc->sc_kbd;
	uint8_t report;

	if (hidkbd_set_leds(kbd, leds, &report) != 0) {
		if (sc->sc_output != NULL)
			(*sc->sc_output)(&sc->sc_hidev, &report,
			    sizeof(report), sc->sc_inintr);
	}
}

int
btkbd_ioctl(void *self, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct btkbd_softc *sc = (struct btkbd_softc *)self;
	struct hidkbd *kbd = &sc->sc_kbd;

	switch (cmd) {
	case WSKBDIO_GTYPE:
		*(int *)data = WSKBD_TYPE_BLUETOOTH;
		return 0;
	case WSKBDIO_SETLEDS:
		btkbd_set_leds(sc, *(int *)data);
		return 0;
	default:
		return hidkbd_ioctl(kbd, cmd, data, flag, p);
	}
}

void
btkbd_input(struct bthidev *self, uint8_t *data, int len)
{
	struct btkbd_softc *sc = (struct btkbd_softc *)self;
	struct hidkbd *kbd = &sc->sc_kbd;

	if (kbd->sc_enabled != 0) {
		sc->sc_inintr = 1;
		hidkbd_input(kbd, data, len);
		sc->sc_inintr = 0;
	}
}
