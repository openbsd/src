/*	$OpenBSD: ukbd.c,v 1.69 2014/12/11 18:39:27 mpi Exp $	*/
/*      $NetBSD: ukbd.c,v 1.85 2003/03/11 16:44:00 augustss Exp $        */

/*
 * Copyright (c) 2010 Miodrag Vallat.
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
	struct uhidev		sc_hdev;
#define sc_ledsize		sc_hdev.sc_osize

	struct hidkbd		sc_kbd;

	int			sc_spl;

	struct hid_location	sc_apple_fn;

	void			(*sc_munge)(void *, uint8_t *, u_int);
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

struct cfdriver ukbd_cd = { 
	NULL, "ukbd", DV_DULL 
}; 

const struct cfattach ukbd_ca = {
	sizeof(struct ukbd_softc), ukbd_match, ukbd_attach, ukbd_detach
};

struct ukbd_translation {
	uint8_t original;
	uint8_t translation;
};

#ifdef __loongson__
void	ukbd_gdium_munge(void *, uint8_t *, u_int);
#endif
void	ukbd_apple_munge(void *, uint8_t *, u_int);
void	ukbd_apple_iso_munge(void *, uint8_t *, u_int);
uint8_t	ukbd_translate(const struct ukbd_translation *, size_t, uint8_t);

int
ukbd_match(struct device *parent, void *match, void *aux)
{
	struct uhidev_attach_arg *uha = (struct uhidev_attach_arg *)aux;
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
	struct uhidev_attach_arg *uha = (struct uhidev_attach_arg *)aux;
	struct usb_hid_descriptor *hid;
	u_int32_t qflags;
	int dlen, repid;
	int console = 1;
	void *desc;
	kbd_t layout = (kbd_t)-1;

	sc->sc_hdev.sc_intr = ukbd_intr;
	sc->sc_hdev.sc_parent = uha->parent;
	sc->sc_hdev.sc_udev = uha->uaa->device;
	sc->sc_hdev.sc_report_id = uha->reportid;

	uhidev_get_report_desc(uha->parent, &desc, &dlen);
	repid = uha->reportid;
	sc->sc_hdev.sc_isize = hid_report_size(desc, dlen, hid_input, repid);
	sc->sc_hdev.sc_osize = hid_report_size(desc, dlen, hid_output, repid);
	sc->sc_hdev.sc_fsize = hid_report_size(desc, dlen, hid_feature, repid);

	 /*
	  * Since the HID-Proxy is always detected before any
	  * real keyboard, do not let it grab the console.
	  */
	if (uha->uaa->vendor == USB_VENDOR_APPLE &&
	    uha->uaa->product == USB_PRODUCT_APPLE_BLUETOOTH_HCI)
		console = 0;

	qflags = usbd_get_quirks(sc->sc_hdev.sc_udev)->uq_flags;
	if (hidkbd_attach(self, kbd, console, qflags, repid, desc, dlen) != 0)
		return;

	if (uha->uaa->vendor == USB_VENDOR_APPLE) {
		int iso = 0;

		if ((uha->uaa->product == USB_PRODUCT_APPLE_FOUNTAIN_ISO) ||
 		    (uha->uaa->product == USB_PRODUCT_APPLE_GEYSER_ISO))
 		    	iso = 1;

		if (hid_locate(desc, dlen, HID_USAGE2(HUP_APPLE, HUG_FN_KEY),
		    uha->reportid, hid_input, &sc->sc_apple_fn, &qflags)) {
			if (qflags & HIO_VARIABLE) {
				if (iso)
					sc->sc_munge = ukbd_apple_iso_munge;
				else
					sc->sc_munge = ukbd_apple_munge;
			}
		}
	}

	if (uha->uaa->vendor == USB_VENDOR_TOPRE &&
	    uha->uaa->product == USB_PRODUCT_TOPRE_HHKB) {
		/* ignore country code on purpose */
	} else {
		usb_interface_descriptor_t *id;

		id = usbd_get_interface_descriptor(uha->uaa->iface);
		hid = usbd_get_hid_descriptor(uha->uaa->device, id);

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
		layout = KB_US | KB_DEFAULT;
#endif
	}

	printf("\n");

#ifdef __loongson__
	if (uha->uaa->vendor == USB_VENDOR_CYPRESS &&
	    uha->uaa->product == USB_PRODUCT_CYPRESS_LPRDK)
		sc->sc_munge = ukbd_gdium_munge;
#endif

	if (kbd->sc_console_keyboard) {
		extern struct wskbd_mapdata ukbd_keymapdata;

		DPRINTF(("ukbd_attach: console keyboard sc=%p\n", sc));
		ukbd_keymapdata.layout = layout;
		wskbd_cnattach(&ukbd_consops, sc, &ukbd_keymapdata);
		ukbd_enable(sc, 1);
	}

	/* Flash the leds; no real purpose, just shows we're alive. */
	ukbd_set_leds(sc, WSKBD_LED_SCROLL | WSKBD_LED_NUM |
		          WSKBD_LED_CAPS | WSKBD_LED_COMPOSE);
	usbd_delay_ms(sc->sc_hdev.sc_udev, 400);
	ukbd_set_leds(sc, 0);

	hidkbd_attach_wskbd(kbd, layout, &ukbd_accessops);
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

	if (kbd->sc_enabled != 0) {
		if (sc->sc_munge != NULL)
			(*sc->sc_munge)(sc, (uint8_t *)ibuf, len);
		hidkbd_input(kbd, (uint8_t *)ibuf, len);
	}
}

int
ukbd_enable(void *v, int on)
{
	struct ukbd_softc *sc = v;
	struct hidkbd *kbd = &sc->sc_kbd;
	int rv;

	if (on && usbd_is_dying(sc->sc_hdev.sc_udev))
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

	if (usbd_is_dying(sc->sc_hdev.sc_udev))
		return;

	if (sc->sc_ledsize && hidkbd_set_leds(kbd, leds, &res) != 0)
		uhidev_set_report_async(sc->sc_hdev.sc_parent,
		    UHID_OUTPUT_REPORT, sc->sc_hdev.sc_report_id, &res, 1);
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
		usbd_dopoll(sc->sc_hdev.sc_udev);
	kbd->sc_polling = 0;
	hidkbd_cngetc(kbd, type, data);
	DPRINTFN(0,("ukbd_cngetc: return 0x%02x\n", *data));
}

void
ukbd_cnpollc(void *v, int on)
{
	struct ukbd_softc *sc = v;

	DPRINTFN(2,("ukbd_cnpollc: sc=%p on=%d\n", v, on));

	if (on)
		sc->sc_spl = splusb();
	else
		splx(sc->sc_spl);
	usbd_set_polling(sc->sc_hdev.sc_udev, on);
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

uint8_t
ukbd_translate(const struct ukbd_translation *table, size_t tsize,
    uint8_t keycode)
{
	for (; tsize != 0; table++, tsize--)
		if (table->original == keycode)
			return table->translation;
	return 0;
}

void
ukbd_apple_munge(void *vsc, uint8_t *ibuf, u_int ilen)
{
	struct ukbd_softc *sc = vsc;
	struct hidkbd *kbd = &sc->sc_kbd;
	uint8_t *pos, *spos, *epos, xlat;

	static const struct ukbd_translation apple_fn_trans[] = {
		{ 40, 73 },	/* return -> insert */
		{ 42, 76 },	/* backspace -> delete */
#ifdef notyet
		{ 58, 0 },	/* F1 -> screen brightness down */
		{ 59, 0 },	/* F2 -> screen brightness up */
		{ 60, 0 },	/* F3 */
		{ 61, 0 },	/* F4 */
		{ 62, 0 },	/* F5 -> keyboard backlight down */
		{ 63, 0 },	/* F6 -> keyboard backlight up */
		{ 64, 0 },	/* F7 -> audio back */
		{ 65, 0 },	/* F8 -> audio pause/play */
		{ 66, 0 },	/* F9 -> audio next */
#endif
#ifdef __macppc__
		{ 60, 127 },	/* F3 -> audio mute */
		{ 61, 129 },	/* F4 -> audio lower */
		{ 62, 128 },	/* F5 -> audio raise */
#else
		{ 67, 127 },	/* F10 -> audio mute */
		{ 68, 129 },	/* F11 -> audio lower */
		{ 69, 128 },	/* F12 -> audio raise */
#endif
		{ 79, 77 },	/* right -> end */
		{ 80, 74 },	/* left -> home */
		{ 81, 78 },	/* down -> page down */
		{ 82, 75 }	/* up -> page up */
	};

	if (!hid_get_data(ibuf, ilen, &sc->sc_apple_fn))
		return;

	spos = ibuf + kbd->sc_keycodeloc.pos / 8;
	epos = spos + kbd->sc_nkeycode;

	for (pos = spos; pos != epos; pos++) {
		xlat = ukbd_translate(apple_fn_trans,
		    nitems(apple_fn_trans), *pos);
		if (xlat != 0)
			*pos = xlat;
	}
}

void
ukbd_apple_iso_munge(void *vsc, uint8_t *ibuf, u_int ilen)
{
	struct ukbd_softc *sc = vsc;
	struct hidkbd *kbd = &sc->sc_kbd;
	uint8_t *pos, *spos, *epos, xlat;

	static const struct ukbd_translation apple_iso_trans[] = {
		{ 53, 100 },	/* less -> grave */
		{ 100, 53 },
	};

	spos = ibuf + kbd->sc_keycodeloc.pos / 8;
	epos = spos + kbd->sc_nkeycode;

	for (pos = spos; pos != epos; pos++) {
		xlat = ukbd_translate(apple_iso_trans,
		    nitems(apple_iso_trans), *pos);
		if (xlat != 0)
			*pos = xlat;
	}

	ukbd_apple_munge(vsc, ibuf, ilen);
}

#ifdef __loongson__
/*
 * Software Fn- translation for Gdium Liberty keyboard.
 */
#define	GDIUM_FN_CODE	0x82
void
ukbd_gdium_munge(void *vsc, uint8_t *ibuf, u_int ilen)
{
	struct ukbd_softc *sc = vsc;
	struct hidkbd *kbd = &sc->sc_kbd;
	uint8_t *pos, *spos, *epos, xlat;
	int fn;

	static const struct ukbd_translation gdium_fn_trans[] = {
#ifdef notyet
		{ 58, 0 },	/* F1 -> toggle camera */
		{ 59, 0 },	/* F2 -> toggle wireless */
#endif
		{ 60, 127 },	/* F3 -> audio mute */
		{ 61, 128 },	/* F4 -> audio raise */
		{ 62, 129 },	/* F5 -> audio lower */
#ifdef notyet
		{ 63, 0 },	/* F6 -> toggle ext. video */
		{ 64, 0 },	/* F7 -> toggle mouse */
		{ 65, 0 },	/* F8 -> brightness up */
		{ 66, 0 },	/* F9 -> brightness down */
		{ 67, 0 },	/* F10 -> suspend */
		{ 68, 0 },	/* F11 -> user1 */
		{ 69, 0 },	/* F12 -> user2 */
		{ 70, 0 },	/* print screen -> sysrq */
#endif
		{ 76, 71 },	/* delete -> scroll lock */
		{ 81, 78 },	/* down -> page down */
		{ 82, 75 }	/* up -> page up */
	};

	spos = ibuf + kbd->sc_keycodeloc.pos / 8;
	epos = spos + kbd->sc_nkeycode;

	/*
	 * Check for Fn key being down and remove it from the report.
	 */

	fn = 0;
	for (pos = spos; pos != epos; pos++)
		if (*pos == GDIUM_FN_CODE) {
			fn = 1;
			*pos = 0;
			break;
		}

	/*
	 * Rewrite keycodes on the fly to perform Fn-key translation.
	 * Keycodes without a translation are passed unaffected.
	 */

	if (fn != 0)
		for (pos = spos; pos != epos; pos++) {
			xlat = ukbd_translate(gdium_fn_trans,
			    nitems(gdium_fn_trans), *pos);
			if (xlat != 0)
				*pos = xlat;
		}

}
#endif
