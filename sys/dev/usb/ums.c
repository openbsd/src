/*	$OpenBSD: ums.c,v 1.40 2014/12/28 15:24:08 matthieu Exp $ */
/*	$NetBSD: ums.c,v 1.60 2003/03/11 16:44:00 augustss Exp $	*/

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

/*
 * HID spec: http://www.usb.org/developers/devclass_docs/HID1_11.pdf
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/ioctl.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbhid.h>

#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>
#include <dev/usb/usb_quirks.h>
#include <dev/usb/uhidev.h>
#include <dev/usb/hid.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsmousevar.h>

#include <dev/usb/hidmsvar.h>

struct ums_softc {
	struct uhidev	sc_hdev;
	struct hidms	sc_ms;
};

void ums_intr(struct uhidev *addr, void *ibuf, u_int len);

int	ums_enable(void *);
void	ums_disable(void *);
int	ums_ioctl(void *, u_long, caddr_t, int, struct proc *);

const struct wsmouse_accessops ums_accessops = {
	ums_enable,
	ums_ioctl,
	ums_disable,
};

int ums_match(struct device *, void *, void *);
void ums_attach(struct device *, struct device *, void *);
int ums_detach(struct device *, int);

struct cfdriver ums_cd = {
	NULL, "ums", DV_DULL
};

const struct cfattach ums_ca = {
	sizeof(struct ums_softc), ums_match, ums_attach, ums_detach
};

int
ums_match(struct device *parent, void *match, void *aux)
{
	struct uhidev_attach_arg *uha = (struct uhidev_attach_arg *)aux;
	int size;
	void *desc;

	uhidev_get_report_desc(uha->parent, &desc, &size);

	if (hid_is_collection(desc, size, uha->reportid,
	    HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_POINTER)))
		return (UMATCH_IFACECLASS);

	if (hid_is_collection(desc, size, uha->reportid,
	    HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_MOUSE)))
		return (UMATCH_IFACECLASS);

	if (hid_is_collection(desc, size, uha->reportid,
	    HID_USAGE2(HUP_DIGITIZERS, HUD_TOUCHSCREEN)))
		return (UMATCH_IFACECLASS);

	if (hid_is_collection(desc, size, uha->reportid,
	    HID_USAGE2(HUP_DIGITIZERS, HUD_PEN)))
		return (UMATCH_IFACECLASS);

	return (UMATCH_NONE);
}

void
ums_attach(struct device *parent, struct device *self, void *aux)
{
	struct ums_softc *sc = (struct ums_softc *)self;
	struct hidms *ms = &sc->sc_ms;
	struct uhidev_attach_arg *uha = (struct uhidev_attach_arg *)aux;
	struct usb_attach_arg *uaa = uha->uaa;
	int size, repid;
	void *desc;
	u_int32_t quirks;

	sc->sc_hdev.sc_intr = ums_intr;
	sc->sc_hdev.sc_parent = uha->parent;
	sc->sc_hdev.sc_udev = uaa->device;
	sc->sc_hdev.sc_report_id = uha->reportid;

	quirks = usbd_get_quirks(sc->sc_hdev.sc_udev)->uq_flags;
	uhidev_get_report_desc(uha->parent, &desc, &size);
	repid = uha->reportid;
	sc->sc_hdev.sc_isize = hid_report_size(desc, size, hid_input, repid);
	sc->sc_hdev.sc_osize = hid_report_size(desc, size, hid_output, repid);
	sc->sc_hdev.sc_fsize = hid_report_size(desc, size, hid_feature, repid);

	if (hidms_setup(self, ms, quirks, uha->reportid, desc, size) != 0)
		return;

	/*
	 * The Microsoft Wireless Notebook Optical Mouse 3000 Model 1049 has
	 * five Report IDs: 19, 23, 24, 17, 18 (in the order they appear in
	 * report descriptor), it seems that report 17 contains the necessary
	 * mouse information (3-buttons, X, Y, wheel) so we specify it
	 * manually.
	 */
	if (uaa->vendor == USB_VENDOR_MICROSOFT &&
	    uaa->product == USB_PRODUCT_MICROSOFT_WLNOTEBOOK3) {
		ms->sc_flags = HIDMS_Z;
		ms->sc_num_buttons = 3;
		/* XXX change sc_hdev isize to 5? */
		ms->sc_loc_x.pos = 8;
		ms->sc_loc_y.pos = 16;
		ms->sc_loc_z.pos = 24;
		ms->sc_loc_btn[0].pos = 0;
		ms->sc_loc_btn[1].pos = 1;
		ms->sc_loc_btn[2].pos = 2;
	}

	hidms_attach(ms, &ums_accessops);
}

int
ums_detach(struct device *self, int flags)
{
	struct ums_softc *sc = (struct ums_softc *)self;
	struct hidms *ms = &sc->sc_ms;

	return hidms_detach(ms, flags);
}

void
ums_intr(struct uhidev *addr, void *buf, u_int len)
{
	struct ums_softc *sc = (struct ums_softc *)addr;
	struct hidms *ms = &sc->sc_ms;

	if (ms->sc_enabled != 0)
		hidms_input(ms, (uint8_t *)buf, len);
}

int
ums_enable(void *v)
{
	struct ums_softc *sc = v;
	struct hidms *ms = &sc->sc_ms;
	int rv;

	if (usbd_is_dying(sc->sc_hdev.sc_udev))
		return EIO;

	if ((rv = hidms_enable(ms)) != 0)
		return rv;

	return uhidev_open(&sc->sc_hdev);
}

void
ums_disable(void *v)
{
	struct ums_softc *sc = v;
	struct hidms *ms = &sc->sc_ms;

	hidms_disable(ms);
	uhidev_close(&sc->sc_hdev);
}

int
ums_ioctl(void *v, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct ums_softc *sc = v;
	struct hidms *ms = &sc->sc_ms;
	int rc;

	rc = uhidev_ioctl(&sc->sc_hdev, cmd, data, flag, p);
	if (rc != -1)
		return rc;
	rc = hidms_ioctl(ms, cmd, data, flag, p);
	if (rc != -1)
		return rc;

	switch (cmd) {
	case WSMOUSEIO_GTYPE:
		*(u_int *)data = WSMOUSE_TYPE_USB;
		return 0;
	default:
		return -1;
	}
}
