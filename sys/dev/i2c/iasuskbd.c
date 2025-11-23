/*	$OpenBSD: iasuskbd.c,v 1.2 2025/11/23 21:28:15 kettenis Exp $	*/
/*
 * Copyright (c) 2025 Mark Kettenis <kettenis@openbsd.org>
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
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/systm.h>

#include <dev/hid/hid.h>

#include <dev/i2c/i2cvar.h>
#include <dev/i2c/ihidev.h>

#include <dev/usb/usbdevs.h>

#define I2C_HID_PRODUCT_ASUS_S5507QA	0x4543	/* Vivobook S 15 */

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wskbdvar.h>
#include <dev/wscons/wsksymdef.h>
#include <dev/wscons/wsksymvar.h>

#define IASUSKBD_KBD_BACKLIGHT_SUPPORT	0x01

struct iasuskbd_softc {
	struct ihidev	 	sc_hdev;

	/*
	 * Only the first element is populated whereas the second
	 * remains zeroed out since such trailing sentinel is required
	 * by wskbd_load_keymap().
	 */
	struct wscons_keydesc	sc_keydesc[2];
	struct wskbd_mapdata	sc_keymap;
	struct device		*sc_wskbddev;
	int			sc_lastcode;

	char			*sc_cmdbuf;
	size_t		 	sc_cmdbuflen;
	unsigned int	 	sc_backlight;
};

int	iasuskbd_match(struct device *, void *, void *);
void	iasuskbd_attach(struct device *, struct device *, void *);
int	iasuskbd_activate(struct device *, int);
void	iasuskbd_attach_wskbd(struct iasuskbd_softc *);
void	iasuskbd_intr(struct ihidev *, void *, u_int);
int	iasuskbd_do_set_backlight(struct iasuskbd_softc *, unsigned int);

int	iasuskbd_get_backlight(struct wskbd_backlight *);
int	iasuskbd_set_backlight(struct wskbd_backlight *);
extern int (*wskbd_get_backlight)(struct wskbd_backlight *);
extern int (*wskbd_set_backlight)(struct wskbd_backlight *);

struct cfdriver iasuskbd_cd = {
	NULL, "iasuskbd", DV_DULL
};

const struct cfattach iasuskbd_ca = {
	sizeof(struct iasuskbd_softc), iasuskbd_match, iasuskbd_attach, NULL,
	iasuskbd_activate
};

int
iasuskbd_match(struct device *parent, void *match, void *aux)
{
	struct ihidev_attach_arg *iha = aux;
	void *desc;
	int size;

	if (iha->parent->hid_desc.wVendorID != USB_VENDOR_ASUS ||
	    iha->parent->hid_desc.wProductID != I2C_HID_PRODUCT_ASUS_S5507QA)
		return IMATCH_NONE;

	ihidev_get_report_desc(iha->parent, &desc, &size);
	if (!hid_is_collection(desc, size, iha->reportid,
	    HID_USAGE2(0xff31, 0x0076)))
		return IMATCH_NONE;

	return IMATCH_VENDOR_PRODUCT;
}

void
iasuskbd_attach(struct device *parent, struct device *self, void *aux)
{
	struct iasuskbd_softc *sc = (struct iasuskbd_softc *)self;
	struct ihidev_attach_arg *iha = aux;
	const char get_functions_cmd[] = { 0x05, 0x20, 0x31, 0x00, 0x08 };
	const char disable_rgb_cmd[] = { 0xd0, 0x8f, 0x01 };
	void *desc;
	int repid, size;
	int error;

	printf("\n");

	sc->sc_hdev.sc_intr = iasuskbd_intr;
	sc->sc_hdev.sc_parent = iha->parent;
	sc->sc_hdev.sc_report_id = iha->reportid;

	ihidev_get_report_desc(iha->parent, &desc, &size);
	repid = iha->reportid;
	sc->sc_hdev.sc_isize = hid_report_size(desc, size, hid_input, repid);
	sc->sc_hdev.sc_osize = hid_report_size(desc, size, hid_output, repid);
	sc->sc_hdev.sc_fsize = hid_report_size(desc, size, hid_feature, repid);

	sc->sc_cmdbuflen = MAX(sc->sc_hdev.sc_fsize, 6);
	sc->sc_cmdbuf = malloc(sc->sc_cmdbuflen, M_DEVBUF, M_WAITOK);

	memset(sc->sc_cmdbuf, 0, sc->sc_cmdbuflen);
	memcpy(sc->sc_cmdbuf, get_functions_cmd, sizeof(get_functions_cmd));
	ihidev_set_report(parent, I2C_HID_REPORT_TYPE_FEATURE,
	    repid, sc->sc_cmdbuf, sc->sc_cmdbuflen);
	error = ihidev_get_report(parent, I2C_HID_REPORT_TYPE_FEATURE,
	    repid, sc->sc_cmdbuf, sc->sc_cmdbuflen);
	if (error) {
		free(sc->sc_cmdbuf, M_DEVBUF, sc->sc_cmdbuflen);
		return;
	}

	if ((sc->sc_cmdbuf[5] & IASUSKBD_KBD_BACKLIGHT_SUPPORT) == 0) {
		free(sc->sc_cmdbuf, M_DEVBUF, sc->sc_cmdbuflen);
		return;
	}

	memset(sc->sc_cmdbuf, 0, sc->sc_cmdbuflen);
	memcpy(sc->sc_cmdbuf, disable_rgb_cmd, sizeof(disable_rgb_cmd));
	ihidev_set_report(parent, I2C_HID_REPORT_TYPE_FEATURE,
	    repid, sc->sc_cmdbuf, sc->sc_cmdbuflen);

	sc->sc_backlight = 0;
	iasuskbd_do_set_backlight(sc, sc->sc_backlight);

	wskbd_get_backlight = iasuskbd_get_backlight;
	wskbd_set_backlight = iasuskbd_set_backlight;

	sc->sc_lastcode = -1;
	iasuskbd_attach_wskbd(sc);
}

int
iasuskbd_activate(struct device *self, int act)
{
	struct iasuskbd_softc *sc = (struct iasuskbd_softc *)self;

	switch (act) {
	case DVACT_QUIESCE:
		iasuskbd_do_set_backlight(sc, 0);
		break;
	case DVACT_WAKEUP:
		iasuskbd_do_set_backlight(sc, sc->sc_backlight);
		break;
	}

	return 0;
}

int
iasuskbd_enable(void *v, int on)
{
	struct iasuskbd_softc *sc = v;
	int error = 0;

	if (on)
		error = ihidev_open(&sc->sc_hdev);
	else
		ihidev_close(&sc->sc_hdev);
	return error;
}

int
iasuskbd_ioctl(void *v, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	switch (cmd) {
	case WSKBDIO_GTYPE:
		*(int *)data = WSKBD_TYPE_USB;
		return 0;
	case WSKBDIO_GETLEDS:
		*(int *)data = 0;
		return 0;
	}

	return -1;
}

const keysym_t iasuskbd_keydesc[] = {
	KS_KEYCODE(0x10), KS_Cmd_BrightnessDown,
	KS_KEYCODE(0x20), KS_Cmd_BrightnessUp,
	KS_KEYCODE(0xc7), KS_Cmd_KbdBacklightToggle,
};

void
iasuskbd_attach_wskbd(struct iasuskbd_softc *sc)
{
	static const struct wskbd_accessops accessops = {
		.enable		= iasuskbd_enable,
		.ioctl		= iasuskbd_ioctl,
	};
	struct wskbddev_attach_args a = {
		.console	= 0,
		.keymap		= &sc->sc_keymap,
		.accessops	= &accessops,
		.accesscookie	= sc,
	};

	sc->sc_keydesc[0].name = KB_US;
	sc->sc_keydesc[0].base = 0;
	sc->sc_keydesc[0].map = iasuskbd_keydesc;
	sc->sc_keydesc[0].map_size = nitems(iasuskbd_keydesc);
	sc->sc_keymap.keydesc = sc->sc_keydesc;
	sc->sc_keymap.layout = KB_US | KB_NOENCODING;
	sc->sc_wskbddev = config_found((struct device *)sc, &a, wskbddevprint);	
}

void
iasuskbd_intr(struct ihidev *addr, void *data, u_int len)
{
	struct iasuskbd_softc *sc = (struct iasuskbd_softc *)addr;
	
	if (len > 0) {
		int code = *(uint8_t *)data;

		if (code != sc->sc_lastcode && sc->sc_lastcode != -1) {
			wskbd_input(sc->sc_wskbddev, WSCONS_EVENT_KEY_UP,
			    sc->sc_lastcode);
		}
		if (code != sc->sc_lastcode) {
			wskbd_input(sc->sc_wskbddev, WSCONS_EVENT_KEY_DOWN,
			    code);
			sc->sc_lastcode = code;
		}
	}
}

int
iasuskbd_do_set_backlight(struct iasuskbd_softc *sc, unsigned int val)
{
	const char set_backlight_cmd[] = { 0xba, 0xc5, 0xc4, 0x00 };
	int error;

	if (val > 3)
		return EINVAL;

	memset(sc->sc_cmdbuf, 0, sc->sc_cmdbuflen);
	memcpy(sc->sc_cmdbuf, set_backlight_cmd, sizeof(set_backlight_cmd));
	sc->sc_cmdbuf[3] = val;
	error = ihidev_set_report((struct device *)sc->sc_hdev.sc_parent,
	    I2C_HID_REPORT_TYPE_FEATURE, sc->sc_hdev.sc_report_id,
	    sc->sc_cmdbuf, sc->sc_cmdbuflen);
	if (error)
		return EIO;

	return 0;
}

int
iasuskbd_get_backlight(struct wskbd_backlight *kbl)
{
	struct iasuskbd_softc *sc = iasuskbd_cd.cd_devs[0];

	KASSERT(sc != NULL);

	kbl->min = 0;
	kbl->max = 3;
	kbl->curval = sc->sc_backlight;
	return 0;
}

int
iasuskbd_set_backlight(struct wskbd_backlight *kbl)
{
	struct iasuskbd_softc *sc = iasuskbd_cd.cd_devs[0];
	int error;

	KASSERT(sc != NULL);

	error = iasuskbd_do_set_backlight(sc, kbl->curval);
	if (error)
		return error;

	sc->sc_backlight = kbl->curval;
	return 0;
}
