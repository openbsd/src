/*	$OpenBSD: ukbd.c,v 1.53 2010/08/02 23:17:34 miod Exp $	*/
/*      $NetBSD: ukbd.c,v 1.85 2003/03/11 16:44:00 augustss Exp $        */

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
#include <sys/timeout.h>
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
#include <dev/usb/ukbdvar.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wskbdvar.h>
#include <dev/wscons/wsksymdef.h>
#include <dev/wscons/wsksymvar.h>

#include <dev/usb/hidkbdsc.h>
#include <dev/usb/hidkbdvar.h>

#ifdef UKBD_DEBUG
#define DPRINTF(x)	do { if (ukbddebug) printf x; } while (0)
#define DPRINTFN(n,x)	do { if (ukbddebug>(n)) printf x; } while (0)
int	ukbddebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

const kbd_t ukbd_countrylayout[1 + HCC_MAX] = {
	(kbd_t)-1,
	(kbd_t)-1,	/* arabic */
	KB_BE,		/* belgian */
	(kbd_t)-1,	/* canadian bilingual */
	KB_CF,		/* canadian french */
	(kbd_t)-1,	/* czech */
	KB_DK,		/* danish */
	(kbd_t)-1,	/* finnish */
	KB_FR,		/* french */
	KB_DE,		/* german */
	(kbd_t)-1,	/* greek */
	(kbd_t)-1,	/* hebrew */
	KB_HU,		/* hungary */
	(kbd_t)-1,	/* international (iso) */
	KB_IT,		/* italian */
	KB_JP,		/* japanese (katakana) */
	(kbd_t)-1,	/* korean */
	KB_LA,		/* latin american */
	(kbd_t)-1,	/* netherlands/dutch */
	KB_NO,		/* norwegian */
	(kbd_t)-1,	/* persian (farsi) */
	KB_PL,		/* polish */
	KB_PT,		/* portuguese */
	KB_RU,		/* russian */
	(kbd_t)-1,	/* slovakia */
	KB_ES,		/* spanish */
	KB_SV,		/* swedish */
	KB_SF,		/* swiss french */
	KB_SG,		/* swiss german */
	(kbd_t)-1,	/* switzerland */
	(kbd_t)-1,	/* taiwan */
	KB_TR,		/* turkish Q */
	KB_UK,		/* uk */
	KB_US,		/* us */
	(kbd_t)-1,	/* yugoslavia */
	(kbd_t)-1	/* turkish F */
};

struct ukbd_softc {
	struct uhidev	sc_hdev;
	struct hidkbd	sc_kbd;
	u_char		sc_dying;
	int		sc_spl;
};

void	ukbd_cngetc(void *, u_int *, int *);
void	ukbd_cnpollc(void *, int);
void	ukbd_cnbell(void *, u_int, u_int, u_int);

const struct wskbd_consops ukbd_consops = {
	ukbd_cngetc,
	ukbd_cnpollc,
	ukbd_cnbell,
};

void	ukbd_intr(struct uhidev *addr, void *ibuf, u_int len);

int	ukbd_enable(void *, int);
void	ukbd_set_leds(void *, int);
int	ukbd_ioctl(void *, u_long, caddr_t, int, struct proc *);

const struct wskbd_accessops ukbd_accessops = {
	ukbd_enable,
	ukbd_set_leds,
	ukbd_ioctl,
};

int ukbd_match(struct device *, void *, void *); 
void ukbd_attach(struct device *, struct device *, void *); 
int ukbd_detach(struct device *, int); 
int ukbd_activate(struct device *, int); 

struct cfdriver ukbd_cd = { 
	NULL, "ukbd", DV_DULL 
}; 

const struct cfattach ukbd_ca = { 
	sizeof(struct ukbd_softc), 
	ukbd_match, 
	ukbd_attach, 
	ukbd_detach, 
	ukbd_activate, 
};

int
ukbd_match(struct device *parent, void *match, void *aux)
{
	struct usb_attach_arg *uaa = aux;
	struct uhidev_attach_arg *uha = (struct uhidev_attach_arg *)uaa;
	int size;
	void *desc;

	uhidev_get_report_desc(uha->parent, &desc, &size);
	if (!hid_is_collection(desc, size, uha->reportid,
			       HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_KEYBOARD)))
		return (UMATCH_NONE);

	return (UMATCH_IFACECLASS);
}

void
ukbd_attach(struct device *parent, struct device *self, void *aux)
{
	struct ukbd_softc *sc = (struct ukbd_softc *)self;
	struct hidkbd *kbd = &sc->sc_kbd;
	struct usb_attach_arg *uaa = aux;
	struct uhidev_attach_arg *uha = (struct uhidev_attach_arg *)uaa;
	usb_hid_descriptor_t *hid;
	u_int32_t qflags;
	int dlen, repid;
	void *desc;
	kbd_t layout = (kbd_t)-1;

	sc->sc_hdev.sc_intr = ukbd_intr;
	sc->sc_hdev.sc_parent = uha->parent;
	sc->sc_hdev.sc_report_id = uha->reportid;

	uhidev_get_report_desc(uha->parent, &desc, &dlen);
	repid = uha->reportid;
	sc->sc_hdev.sc_isize = hid_report_size(desc, dlen, hid_input, repid);
	sc->sc_hdev.sc_osize = hid_report_size(desc, dlen, hid_output, repid);
	sc->sc_hdev.sc_fsize = hid_report_size(desc, dlen, hid_feature, repid);

	qflags = usbd_get_quirks(uha->parent->sc_udev)->uq_flags;
	if (hidkbd_attach(self, kbd, 1, qflags, repid, desc, dlen) != 0)
		return;

	if (uha->uaa->vendor == USB_VENDOR_TOPRE &&
	    uha->uaa->product == USB_PRODUCT_TOPRE_HHKB) {
		/* ignore country code on purpose */
	} else {
		hid = usbd_get_hid_descriptor(uha->uaa->iface);

		if (hid->bCountryCode <= HCC_MAX)
			layout = ukbd_countrylayout[hid->bCountryCode];
#ifdef DIAGNOSTIC
		if (hid->bCountryCode != 0)
			printf(", country code %d", hid->bCountryCode);
#endif
	}
	if (layout == (kbd_t)-1) {
#ifdef UKBD_LAYOUT
		layout = UKBD_LAYOUT;
#else
		layout = KB_US;
#endif
	}

	printf("\n");

	if (kbd->sc_console_keyboard) {
		extern struct wskbd_mapdata ukbd_keymapdata;

		DPRINTF(("ukbd_attach: console keyboard sc=%p\n", sc));
		ukbd_keymapdata.layout = layout;
		wskbd_cnattach(&ukbd_consops, sc, &ukbd_keymapdata);
		ukbd_enable(sc, 1);
	}

	/* Flash the leds; no real purpose, just shows we're alive. */
	ukbd_set_leds(sc, WSKBD_LED_SCROLL | WSKBD_LED_NUM | WSKBD_LED_CAPS);
	usbd_delay_ms(uha->parent->sc_udev, 400);
	ukbd_set_leds(sc, 0);

	hidkbd_attach_wskbd(kbd, layout, &ukbd_accessops);
}

int
ukbd_activate(struct device *self, int act)
{
	struct ukbd_softc *sc = (struct ukbd_softc *)self;
	struct hidkbd *kbd = &sc->sc_kbd;
	int rv = 0;

	switch (act) {
	case DVACT_ACTIVATE:
		break;

	case DVACT_DEACTIVATE:
		if (kbd->sc_wskbddev != NULL)
			rv = config_deactivate(kbd->sc_wskbddev);
		sc->sc_dying = 1;
		break;
	}
	return (rv);
}

int
ukbd_detach(struct device *self, int flags)
{
	struct ukbd_softc *sc = (struct ukbd_softc *)self;
	struct hidkbd *kbd = &sc->sc_kbd;
	int rv;

	rv = hidkbd_detach(kbd, flags);

	/* The console keyboard does not get a disable call, so check pipe. */
	if (sc->sc_hdev.sc_state & UHIDEV_OPEN)
		uhidev_close(&sc->sc_hdev);

	return (rv);
}

void
ukbd_intr(struct uhidev *addr, void *ibuf, u_int len)
{
	struct ukbd_softc *sc = (struct ukbd_softc *)addr;
	struct hidkbd *kbd = &sc->sc_kbd;

	if (kbd->sc_enabled != 0)
		hidkbd_input(kbd, (uint8_t *)ibuf, len);
}

int
ukbd_enable(void *v, int on)
{
	struct ukbd_softc *sc = v;
	struct hidkbd *kbd = &sc->sc_kbd;
	int rv;

	if (on && sc->sc_dying)
		return EIO;

	if ((rv = hidkbd_enable(kbd, on)) != 0)
		return rv;

	if (on) {
		return uhidev_open(&sc->sc_hdev);
	} else {
		uhidev_close(&sc->sc_hdev);
		return 0;
	}
}

void
ukbd_set_leds(void *v, int leds)
{
	struct ukbd_softc *sc = v;
	struct hidkbd *kbd = &sc->sc_kbd;
	u_int8_t res;

	if (sc->sc_dying)
		return;

	if (hidkbd_set_leds(kbd, leds, &res) != 0)
		uhidev_set_report_async(&sc->sc_hdev, UHID_OUTPUT_REPORT,
		    &res, 1);
}

int
ukbd_ioctl(void *v, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct ukbd_softc *sc = v;
	struct hidkbd *kbd = &sc->sc_kbd;
	int rc;

	switch (cmd) {
	case WSKBDIO_GTYPE:
		*(int *)data = WSKBD_TYPE_USB;
		return (0);
	case WSKBDIO_SETLEDS:
		ukbd_set_leds(v, *(int *)data);
		return (0);
	default:
		rc = uhidev_ioctl(&sc->sc_hdev, cmd, data, flag, p);
		if (rc != -1)
			return rc;
		else
			return hidkbd_ioctl(kbd, cmd, data, flag, p);
	}
}

/* Console interface. */
void
ukbd_cngetc(void *v, u_int *type, int *data)
{
	struct ukbd_softc *sc = v;
	struct hidkbd *kbd = &sc->sc_kbd;

	DPRINTFN(0,("ukbd_cngetc: enter\n"));
	kbd->sc_polling = 1;
	while (kbd->sc_npollchar <= 0)
		usbd_dopoll(sc->sc_hdev.sc_parent->sc_iface);
	kbd->sc_polling = 0;
	hidkbd_cngetc(kbd, type, data);
	DPRINTFN(0,("ukbd_cngetc: return 0x%02x\n", *data));
}

void
ukbd_cnpollc(void *v, int on)
{
	struct ukbd_softc *sc = v;
	usbd_device_handle dev;

	DPRINTFN(2,("ukbd_cnpollc: sc=%p on=%d\n", v, on));

	usbd_interface2device_handle(sc->sc_hdev.sc_parent->sc_iface, &dev);
	if (on)
		sc->sc_spl = splusb();
	else
		splx(sc->sc_spl);
	usbd_set_polling(dev, on);
}

void
ukbd_cnbell(void *v, u_int pitch, u_int period, u_int volume) 
{
	hidkbd_bell(pitch, period, volume, 1);
}	

int
ukbd_cnattach(void)
{

	/*
	 * XXX USB requires too many parts of the kernel to be running
	 * XXX in order to work, so we can't do much for the console
	 * XXX keyboard until autconfiguration has run its course.
	 */
	hidkbd_is_console = 1;
	return (0);
}
