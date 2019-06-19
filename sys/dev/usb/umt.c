/* $OpenBSD: umt.c,v 1.1 2018/08/25 20:31:31 jcs Exp $ */
/*
 * USB multitouch touchpad driver for devices conforming to
 * Windows Precision Touchpad standard
 *
 * https://msdn.microsoft.com/en-us/library/windows/hardware/dn467314%28v=vs.85%29.aspx
 *
 * Copyright (c) 2016-2018 joshua stein <jcs@openbsd.org>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/ioctl.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbhid.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdevs.h>
#include <dev/usb/uhidev.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsmousevar.h>

#include <dev/hid/hid.h>
#include <dev/hid/hidmtvar.h>

struct umt_softc {
	struct uhidev	sc_hdev;
	struct hidmt	sc_mt;

	int		sc_rep_input;
	int		sc_rep_config;
	int		sc_rep_cap;
};

int	umt_enable(void *);
int	umt_open(struct uhidev *);
void	umt_intr(struct uhidev *, void *, u_int);
void	umt_disable(void *);
int	umt_ioctl(void *, u_long, caddr_t, int, struct proc *);

const struct wsmouse_accessops umt_accessops = {
	umt_enable,
	umt_ioctl,
	umt_disable,
};

int	umt_match(struct device *, void *, void *);
int	umt_find_winptp_reports(struct uhidev_softc *, void *, int,
	    struct umt_softc *);
void	umt_attach(struct device *, struct device *, void *);
int	umt_hidev_get_report(struct device *, int, int, void *, int);
int	umt_hidev_set_report(struct device *, int, int, void *, int);
int	umt_detach(struct device *, int);

struct cfdriver umt_cd = {
	NULL, "umt", DV_DULL
};

const struct cfattach umt_ca = {
	sizeof(struct umt_softc),
	umt_match,
	umt_attach,
	umt_detach
};

int
umt_match(struct device *parent, void *match, void *aux)
{
	struct uhidev_attach_arg *uha = (struct uhidev_attach_arg *)aux;
	int size;
	void *desc;

	if (uha->reportid == UHIDEV_CLAIM_ALLREPORTID) {
		uhidev_get_report_desc(uha->parent, &desc, &size);
		if (umt_find_winptp_reports(uha->parent, desc, size, NULL))
			return (UMATCH_DEVCLASS_DEVSUBCLASS);
	}

	return (UMATCH_NONE);
}

int
umt_find_winptp_reports(struct uhidev_softc *parent, void *desc, int size,
    struct umt_softc *sc)
{
	int repid;
	int input = 0, conf = 0, cap = 0;

	if (sc != NULL) {
		sc->sc_rep_input = -1;
		sc->sc_rep_config = -1;
		sc->sc_rep_cap = -1;
	}

	for (repid = 0; repid < parent->sc_nrepid; repid++) {
		if (hid_report_size(desc, size, hid_input, repid) == 0 &&
		    hid_report_size(desc, size, hid_output, repid) == 0 &&
		    hid_report_size(desc, size, hid_feature, repid) == 0)
			continue;

		if (hid_is_collection(desc, size, repid,
		    HID_USAGE2(HUP_DIGITIZERS, HUD_TOUCHPAD))) {
			input = 1;
			if (sc != NULL && sc->sc_rep_input == -1)
				sc->sc_rep_input = repid;
		} else if (hid_is_collection(desc, size, repid,
		    HID_USAGE2(HUP_DIGITIZERS, HUD_CONFIG))) {
			conf = 1;
			if (sc != NULL && sc->sc_rep_config == -1)
				sc->sc_rep_config = repid;
		}

		/* capabilities report could be anywhere */
		if (hid_locate(desc, size, HID_USAGE2(HUP_DIGITIZERS,
		    HUD_CONTACT_MAX), repid, hid_feature, NULL, NULL)) {
			cap = 1;
			if (sc != NULL && sc->sc_rep_cap == -1)
				sc->sc_rep_cap = repid;
		}
	}

	return (conf && input && cap);
}

void
umt_attach(struct device *parent, struct device *self, void *aux)
{
	struct umt_softc *sc = (struct umt_softc *)self;
	struct hidmt *mt = &sc->sc_mt;
	struct uhidev_attach_arg *uha = (struct uhidev_attach_arg *)aux;
	int size;
	void *desc;

	sc->sc_hdev.sc_intr = umt_intr;
	sc->sc_hdev.sc_parent = uha->parent;

	uhidev_get_report_desc(uha->parent, &desc, &size);
	umt_find_winptp_reports(uha->parent, desc, size, sc);

	memset(mt, 0, sizeof(sc->sc_mt));

	/* assume everything has "natural scrolling" where Y axis is reversed */
	mt->sc_flags = HIDMT_REVY;

	mt->hidev_report_type_conv = uhidev_report_type_conv;
	mt->hidev_get_report = umt_hidev_get_report;
	mt->hidev_set_report = umt_hidev_set_report;
	mt->sc_rep_input = sc->sc_rep_input;
	mt->sc_rep_config = sc->sc_rep_config;
	mt->sc_rep_cap = sc->sc_rep_cap;

	if (hidmt_setup(self, mt, desc, size) != 0)
		return;

	hidmt_attach(mt, &umt_accessops);
}

int
umt_hidev_get_report(struct device *self, int type, int id, void *data, int len)
{
	struct umt_softc *sc = (struct umt_softc *)self;
	int ret;

	ret = uhidev_get_report(sc->sc_hdev.sc_parent, type, id, data, len);
	return (ret < len);
}

int
umt_hidev_set_report(struct device *self, int type, int id, void *data, int len)
{
	struct umt_softc *sc = (struct umt_softc *)self;
	int ret;

	ret = uhidev_set_report(sc->sc_hdev.sc_parent, type, id, data, len);
	return (ret < len);
}

int
umt_detach(struct device *self, int flags)
{
	struct umt_softc *sc = (struct umt_softc *)self;
	struct hidmt *mt = &sc->sc_mt;

	return hidmt_detach(mt, flags);
}

void
umt_intr(struct uhidev *dev, void *buf, u_int len)
{
	struct umt_softc *sc = (struct umt_softc *)dev;
	struct hidmt *mt = &sc->sc_mt;

	if (!mt->sc_enabled)
		return;

	hidmt_input(mt, (uint8_t *)buf, len);
}

int
umt_enable(void *v)
{
	struct umt_softc *sc = v;
	struct hidmt *mt = &sc->sc_mt;
	int rv;

	if ((rv = hidmt_enable(mt)) != 0)
		return rv;

	rv = uhidev_open(&sc->sc_hdev);

	hidmt_set_input_mode(mt, HIDMT_INPUT_MODE_MT_TOUCHPAD);

	return rv;
}

void
umt_disable(void *v)
{
	struct umt_softc *sc = v;
	struct hidmt *mt = &sc->sc_mt;

	hidmt_disable(mt);
	uhidev_close(&sc->sc_hdev);
}

int
umt_ioctl(void *v, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct umt_softc *sc = v;
	struct hidmt *mt = &sc->sc_mt;
	int rc;

	rc = uhidev_ioctl(&sc->sc_hdev, cmd, data, flag, p);
	if (rc != -1)
		return rc;

	return hidmt_ioctl(mt, cmd, data, flag, p);
}
