/*	$OpenBSD: btms.c,v 1.6 2011/03/04 23:57:52 kettenis Exp $	*/
/*	$NetBSD: btms.c,v 1.8 2008/09/09 03:54:56 cube Exp $	*/

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
#include <sys/proc.h>
#include <sys/systm.h>

#include <netbt/bluetooth.h>

#include <dev/bluetooth/bthid.h>
#include <dev/bluetooth/bthidev.h>

#include <dev/usb/hid.h>
#include <dev/usb/usb.h>
#include <dev/usb/usbhid.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsmousevar.h>

#include <dev/usb/hidmsvar.h>

struct btms_softc {
	struct bthidev		sc_hidev;
	struct hidms		sc_ms;
};

/* autoconf(9) methods */
int	btms_match(struct device *, void *, void *);
void	btms_attach(struct device *, struct device *, void *);
int	btms_detach(struct device *, int);

struct cfdriver btms_cd = {
	NULL, "btms", DV_DULL
};

const struct cfattach btms_ca = {
	sizeof(struct btms_softc),
	btms_match,
	btms_attach,
	btms_detach,
	/* XXX activate */
};

/* wsmouse(4) accessops */
int	btms_wsmouse_enable(void *);
int	btms_wsmouse_ioctl(void *, u_long, caddr_t, int, struct proc *);
void	btms_wsmouse_disable(void *);

const struct wsmouse_accessops btms_wsmouse_accessops = {
	btms_wsmouse_enable,
	btms_wsmouse_ioctl,
	btms_wsmouse_disable,
};

/* bthid methods */
void	btms_input(struct bthidev *, uint8_t *, int);


int
btms_match(struct device *parent, void *match, void *aux)
{
	struct bthidev_attach_args *ba = aux;

	if (hid_is_collection(ba->ba_desc, ba->ba_dlen, ba->ba_id,
	    HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_MOUSE)))
		return 1;

	return 0;
}

void
btms_attach(struct device *parent, struct device *self, void *aux)
{
	struct btms_softc *sc = (struct btms_softc *)self;
	struct hidms *ms = &sc->sc_ms;
	struct bthidev_attach_args *ba = aux;

	ba->ba_input = btms_input;			/* XXX ugly */

	if (hidms_setup(self, ms, 0, ba->ba_id, ba->ba_desc, ba->ba_dlen) != 0)
		return;

	hidms_attach(ms, &btms_wsmouse_accessops);
}

int
btms_detach(struct device *self, int flags)
{
	struct btms_softc *sc = (struct btms_softc *)self;
	struct hidms *ms = &sc->sc_ms;

	return hidms_detach(ms, flags);
}

int
btms_wsmouse_enable(void *self)
{
	struct btms_softc *sc = (struct btms_softc *)self;
	struct hidms *ms = &sc->sc_ms;

	return hidms_enable(ms);
}

int
btms_wsmouse_ioctl(void *self, u_long cmd, caddr_t data, int flag,
    struct proc *p)
{
	struct btms_softc *sc = (struct btms_softc *)self;
	struct hidms *ms = &sc->sc_ms;
	int rc;

	rc = hidms_ioctl(ms, cmd, data, flag, p);
	if (rc != -1)
		return rc;

	switch (cmd) {
	case WSMOUSEIO_GTYPE:
		*(u_int *)data = WSMOUSE_TYPE_BLUETOOTH;
		return 0;
	default:
		return -1;
	}
}

void
btms_wsmouse_disable(void *self)
{
	struct btms_softc *sc = (struct btms_softc *)self;
	struct hidms *ms = &sc->sc_ms;

	hidms_disable(ms);
}

void
btms_input(struct bthidev *self, uint8_t *data, int len)
{
	struct btms_softc *sc = (struct btms_softc *)self;
	struct hidms *ms = &sc->sc_ms;

	if (ms->sc_enabled != 0)
		hidms_input(ms, data, len);
}
