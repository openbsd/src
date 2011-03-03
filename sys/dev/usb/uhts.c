/*	$OpenBSD: uhts.c,v 1.2 2011/03/03 21:48:49 kettenis Exp $ */
/*
 * Copyright (c) 2009 Matthieu Herrb <matthieu@herrb.eu>
 * Copyright (c) 2007 Robert Nagy <robert@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/file.h>
#include <sys/selinfo.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/poll.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbhid.h>

#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>
#include <dev/usb/uhidev.h>
#include <dev/usb/hid.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsmousevar.h>

#ifdef USB_DEBUG
#define DPRINTF(x)	do { if (uhtsdebug) printf x; } while (0)
#define DPRINTFN(n,x)	do { if (uhtsdebug>(n)) printf x; } while (0)
int	uhtsdebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

#define UHTSUNIT(s)	(minor(s))

struct tsscale {
	int	minx, maxx;
	int	miny, maxy;
	int	swapxy;
	int	resx, resy;
};

struct uhts_softc {
	struct uhidev sc_hdev;
	struct hid_location sc_loc_x, sc_loc_y;
	struct hid_location sc_loc_btn;
	int sc_enabled;
	u_int32_t sc_buttons;	/* mouse button status */
	int sc_rawmode;
	int sc_oldx, sc_oldy;
	struct tsscale sc_tsscale;
	struct device *sc_wsmousedev;
	char sc_dying;
};

struct uhts_pos {
	int down;
	int x, y;
	int z;			/* touch pressure */
};

void uhts_intr(struct uhidev *, void *, u_int);
int uhts_enable(void *);
void uhts_disable(void *);
int uhts_ioctl(void *, u_long, caddr_t, int, struct proc *);

const struct wsmouse_accessops uhts_accessops = {
	uhts_enable,
	uhts_ioctl,
	uhts_disable
};

int uhts_match(struct device *, void *, void *);
void uhts_attach(struct device *, struct device *, void *);
int uhts_detach(struct device *, int);
int uhts_activate(struct device *, int);
void uhts_parse_desc(struct uhts_softc *);

struct cfdriver uhts_cd = {
	NULL, "uhts", DV_DULL
};

const struct cfattach uhts_ca = {
	sizeof(struct uhts_softc),
	uhts_match,
	uhts_attach,
	uhts_detach,
	uhts_activate
};

int
uhts_match(struct device *parent, void *match, void *aux)
{
	struct usb_attach_arg *uaa = aux;
	struct uhidev_attach_arg *uha = (struct uhidev_attach_arg *)uaa;
	int size;
	void *desc;

	uhidev_get_report_desc(uha->parent, &desc, &size);
	if (!hid_is_collection(desc, size, uha->reportid,
		HID_USAGE2(HUP_DIGITIZERS, HUD_TOUCHSCREEN)))
		return (UMATCH_NONE);
	return (UMATCH_IFACECLASS);
}

void
uhts_parse_desc(struct uhts_softc *sc)
{
	struct hid_data *d;
	struct hid_item h;
	int size;
	void *desc;

	uhidev_get_report_desc(sc->sc_hdev.sc_parent, &desc, &size);
	d = hid_start_parse(desc, size, hid_input);
	while (hid_get_item(d, &h)) {
		if (h.kind != hid_input ||
		    HID_GET_USAGE_PAGE(h.usage) != HUP_GENERIC_DESKTOP ||
		    h.report_ID != sc->sc_hdev.sc_report_id)
			continue;
		DPRINTF(("uhts: usage=0x%x range %d..%d\n",
			h.usage, h.logical_minimum, h.logical_maximum));
		switch (HID_GET_USAGE(h.usage)) {
		case HUG_X:
			sc->sc_tsscale.minx = h.logical_minimum;
			sc->sc_tsscale.maxx = h.logical_maximum;
			break;
		case HUG_Y:
			sc->sc_tsscale.miny = h.logical_minimum;
			sc->sc_tsscale.maxy = h.logical_maximum;
			break;
		}
	}
}

void
uhts_attach(struct device *parent, struct device *self, void *aux)
{
	struct uhts_softc *sc = (struct uhts_softc *)self;
	struct usb_attach_arg *uaa = aux;
	struct uhidev_attach_arg *uha = (struct uhidev_attach_arg *)uaa;
	struct wsmousedev_attach_args a;
	void *desc;
	int size;
	u_int32_t flags;

	sc->sc_hdev.sc_intr = uhts_intr;
	sc->sc_hdev.sc_parent = uha->parent;
	sc->sc_hdev.sc_report_id = uha->reportid;

	uhidev_get_report_desc(uha->parent, &desc, &size);

	if (!hid_locate(desc, size, HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_X),
	       uha->reportid, hid_input, &sc->sc_loc_x, &flags)) {
		printf("\n%s: touchscreen has no X report\n",
		       sc->sc_hdev.sc_dev.dv_xname);
		return;
	}
	if (!hid_locate(desc, size, HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_Y),
	       uha->reportid, hid_input, &sc->sc_loc_y, &flags)) {
		printf("\n%s: touchscreen has no Y report\n",
		       sc->sc_hdev.sc_dev.dv_xname);
		return;
	}
	if (!hid_locate(desc, size, HID_USAGE2(HUP_DIGITIZERS,
		    HUD_TIP_SWITCH), uha->reportid, hid_input,
		&sc->sc_loc_btn, 0)){
		printf("\n%s: touch screen has no button\n",
		    sc->sc_hdev.sc_dev.dv_xname);
		return;
	}
	printf("\n");

	a.accessops = &uhts_accessops;
	a.accesscookie = sc;

	uhts_parse_desc(sc);
	sc->sc_rawmode = 0;
	/*  wild guess */
	sc->sc_tsscale.swapxy = 0;
	sc->sc_tsscale.resx = 1024;
	sc->sc_tsscale.resy = 768;

	sc->sc_wsmousedev = config_found(self, &a, wsmousedevprint);
}

int
uhts_activate(struct device *self, int act)
{
	struct uhts_softc *sc = (struct uhts_softc *)self;
	int rv = 0;

	switch (act) {
	case DVACT_ACTIVATE:
		break;
	case DVACT_DEACTIVATE:
		if (sc->sc_wsmousedev != NULL)
			rv = config_deactivate(sc->sc_wsmousedev);
		sc->sc_dying = 1;
		break;
	}
	return (rv);
}

int
uhts_detach(struct device *self, int flags)
{
	struct uhts_softc *sc = (struct uhts_softc *)self;
	int rv = 0;

	DPRINTF(("uhts_detach: sc=%p flags=%d\n", sc, flags));

	/* wsmouse takes care of reference counting */
	if (sc->sc_wsmousedev != NULL)
		rv = config_detach(sc->sc_wsmousedev, flags);
	return (rv);
}

void
uhts_intr(struct uhidev *addr, void *buf, u_int len)
{
	struct uhts_softc *sc = (struct uhts_softc *)addr;
	u_char *ibuf = (u_char *)buf;
	struct uhts_pos tp;
	int x, y, s;
	u_int32_t buttons = 0;

	DPRINTFN(5, ("uhts_intr: len=%d\n", len));

	x = hid_get_data(ibuf, &sc->sc_loc_x);
	y = hid_get_data(ibuf, &sc->sc_loc_y);
	if (hid_get_data(ibuf, &sc->sc_loc_btn))
		buttons = 1;

	DPRINTFN(10, ("uhts_intr: x:%d y:%d buttons:0x%x\n",
		x, y, buttons));

	if (buttons) {
		if (sc->sc_tsscale.swapxy && !sc->sc_rawmode) {
			/* Swap X/Y-Axis */
			tp.y = x;
			tp.x = y;
		} else {
			tp.x = x;
			tp.y = y;
		}
		if (!sc->sc_rawmode &&
		    (sc->sc_tsscale.maxx - sc->sc_tsscale.minx) != 0 &&
		    (sc->sc_tsscale.maxy - sc->sc_tsscale.miny) != 0) {
			/* Scale down to the screen resolution. */
			tp.x = ((tp.x - sc->sc_tsscale.minx) *
			    sc->sc_tsscale.resx) /
			    (sc->sc_tsscale.maxx - sc->sc_tsscale.minx);
			tp.y = ((tp.y - sc->sc_tsscale.miny) *
			    sc->sc_tsscale.resy) /
			    (sc->sc_tsscale.maxy - sc->sc_tsscale.miny);
		}
	} else {
		tp.x = sc->sc_oldx;
		tp.y = sc->sc_oldy;
	}

	sc->sc_buttons = buttons;
	tp.z = buttons;

	if (sc->sc_wsmousedev != NULL) {
		s = spltty();
		wsmouse_input(sc->sc_wsmousedev, buttons, tp.x, tp.y, tp.z, 0,
		    WSMOUSE_INPUT_ABSOLUTE_X | WSMOUSE_INPUT_ABSOLUTE_Y |
		    WSMOUSE_INPUT_ABSOLUTE_Z);
		sc->sc_oldx = tp.x;
		sc->sc_oldy = tp.y;
		splx(s);
	}
}

int
uhts_enable(void *v)
{
	struct uhts_softc *sc = v;

	DPRINTFN(1, ("uhts_enable: sc=%p\n", sc));

	if (sc->sc_dying)
		return (EIO);

	if (sc->sc_enabled)
		return (EBUSY);

	sc->sc_enabled = 1;
	sc->sc_buttons = 0;

	return (uhidev_open(&sc->sc_hdev));
}

void
uhts_disable(void *v)
{
	struct uhts_softc *sc = v;

	DPRINTFN(1, ("uhts_disable: sc=%p\n", sc));
#ifdef DIAGNOSTIC
	if (!sc->sc_enabled) {
		printf("uhts_disable: not enabled\n");
		return;
	}
#endif
	sc->sc_enabled = 0;
	uhidev_close(&sc->sc_hdev);
}

int
uhts_ioctl(void *v, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct uhts_softc *sc = v;
	struct wsmouse_calibcoords *wsmc = (struct wsmouse_calibcoords *)data;
	int error = 0;

	DPRINTF(("uhts_ioctl(%d, '%c', %d)\n",
	    IOCPARM_LEN(cmd), IOCGROUP(cmd), cmd & 0xff));

	switch (cmd) {
	case WSMOUSEIO_GTYPE:
		*(u_int *)data = WSMOUSE_TYPE_TPANEL;
		return (0);
	case WSMOUSEIO_SCALIBCOORDS:
		if (!(wsmc->minx >= 0 && wsmc->maxx >= 0 &&
		    wsmc->miny >= 0 && wsmc->maxy >= 0 &&
		    wsmc->resx >= 0 && wsmc->resy >= 0 &&
		    wsmc->minx < 32768 && wsmc->maxx < 32768 &&
		    wsmc->miny < 32768 && wsmc->maxy < 32768 &&
		    (wsmc->maxx - wsmc->minx) != 0 &&
		    (wsmc->maxy - wsmc->miny) != 0 &&
		    wsmc->resx < 32768 && wsmc->resy < 32768 &&
		    wsmc->swapxy >= 0 && wsmc->swapxy <= 1 &&
		    wsmc->samplelen >= 0 && wsmc->samplelen <= 1))
			return (EINVAL);

		sc->sc_tsscale.minx = wsmc->minx;
		sc->sc_tsscale.maxx = wsmc->maxx;
		sc->sc_tsscale.miny = wsmc->miny;
		sc->sc_tsscale.maxy = wsmc->maxy;
		sc->sc_tsscale.swapxy = wsmc->swapxy;
		sc->sc_tsscale.resx = wsmc->resx;
		sc->sc_tsscale.resy = wsmc->resy;
		sc->sc_rawmode = wsmc->samplelen;
		break;
	case WSMOUSEIO_GCALIBCOORDS:
		wsmc->minx = sc->sc_tsscale.minx;
		wsmc->maxx = sc->sc_tsscale.maxx;
		wsmc->miny = sc->sc_tsscale.miny;
		wsmc->maxy = sc->sc_tsscale.maxy;
		wsmc->swapxy = sc->sc_tsscale.swapxy;
		wsmc->resx = sc->sc_tsscale.resx;
		wsmc->resy = sc->sc_tsscale.resy;
		wsmc->samplelen = sc->sc_rawmode;
		break;
	default:
		error = ENOTTY;
		break;
	}

	return error;
}
