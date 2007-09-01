/*	$OpenBSD: btkbd.c,v 1.2 2007/09/01 17:06:26 xsa Exp $	*/
/*	$NetBSD: btkbd.c,v 1.7 2007/07/09 21:00:31 ad Exp $	*/

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

/*
 * based on dev/usb/ukbd.c
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

#define	MAXKEYCODE		6
#define MAXMOD			8	/* max 32 */
#define MAXKEYS			(MAXMOD + (2 * MAXKEYCODE))

struct btkbd_data {
	uint32_t		modifiers;
	uint8_t			keycode[MAXKEYCODE];
};

struct btkbd_mod {
	uint32_t		mask;
	uint8_t			key;
};

struct btkbd_softc {
	struct bthidev		 sc_hidev;	/* device+ */
	struct device		*sc_wskbd;	/* child */
	int			 sc_enabled;

	int			(*sc_output)	/* output method */
				(struct bthidev *, uint8_t *, int);

	/* stored data */
	struct btkbd_data	 sc_odata;
	struct btkbd_data	 sc_ndata;

	/* input reports */
	int			 sc_nmod;
	struct hid_location	 sc_modloc[MAXMOD];
	struct btkbd_mod	 sc_mods[MAXMOD];

	int			 sc_nkeycode;
	struct hid_location	 sc_keycodeloc;

	/* output reports */
	struct hid_location	 sc_numloc;
	struct hid_location	 sc_capsloc;
	struct hid_location	 sc_scroloc;
	int			 sc_leds;

#ifdef WSDISPLAY_COMPAT_RAWKBD
	int			 sc_rawkbd;
#ifdef BTKBD_REPEAT
	struct timeout		 sc_repeat;
	int			 sc_nrep;
	char			 sc_rep[MAXKEYS];
#endif
#endif
};

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
};

int	btkbd_enable(void *, int);
void	btkbd_set_leds(void *, int);
int	btkbd_ioctl(void *, u_long, caddr_t, int, struct proc *);

const struct wskbd_accessops btkbd_accessops = {
	btkbd_enable,
	btkbd_set_leds,
	btkbd_ioctl
};

/* wskbd(4) keymap data */
extern const struct wscons_keydesc ukbd_keydesctab[];

const struct wskbd_mapdata btkbd_keymapdata = {
	ukbd_keydesctab,
#if defined(BTKBD_LAYOUT)
	BTKBD_LAYOUT,
#elif defined(PCKBD_LAYOUT)
	PCKBD_LAYOUT,
#else
	KB_US,
#endif
};

/* bthid methods */
void btkbd_input(struct bthidev *, uint8_t *, int);

/* internal prototypes */
const char *btkbd_parse_desc(struct btkbd_softc *, int, void *, int);

#ifdef WSDISPLAY_COMPAT_RAWKBD
#ifdef BTKBD_REPEAT
void btkbd_repeat(void *);
#endif
#endif


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
	struct bthidev_attach_args *ba = aux;
	struct wskbddev_attach_args wska;
	const char *parserr;

	sc->sc_output = ba->ba_output;
	ba->ba_input = btkbd_input;

	parserr = btkbd_parse_desc(sc, ba->ba_id, ba->ba_desc, ba->ba_dlen);
	if (parserr != NULL) {
		printf("%s\n", parserr);
		return;
	}

	printf("\n");

#ifdef WSDISPLAY_COMPAT_RAWKBD
#ifdef BTKBD_REPEAT
	timeout_set(&sc->sc_repeat, NULL, NULL);
	/* callout_setfunc(&sc->sc_repeat, btkbd_repeat, sc); */
#endif
#endif

	wska.console = 0;
	wska.keymap = &btkbd_keymapdata;
	wska.accessops = &btkbd_accessops;
	wska.accesscookie = sc;

	sc->sc_wskbd = config_found((struct device *)sc, &wska, wskbddevprint);
}

int
btkbd_detach(struct device *self, int flags)
{
	struct btkbd_softc *sc = (struct btkbd_softc *)self;
	int err = 0;

#ifdef WSDISPLAY_COMPAT_RAWKBD
#ifdef BTKBD_REPEAT
	timeout_del(&sc->sc_repeat);
#endif
#endif

	if (sc->sc_wskbd != NULL) {
		err = config_detach(sc->sc_wskbd, flags);
		sc->sc_wskbd = NULL;
	}

	return err;
}

const char *
btkbd_parse_desc(struct btkbd_softc *sc, int id, void *desc, int dlen)
{
	struct hid_data *d;
	struct hid_item h;
	int imod;

	imod = 0;
	sc->sc_nkeycode = 0;
	d = hid_start_parse(desc, dlen, hid_input);
	while (hid_get_item(d, &h)) {
		if (h.kind != hid_input || (h.flags & HIO_CONST) ||
		    HID_GET_USAGE_PAGE(h.usage) != HUP_KEYBOARD ||
		    h.report_ID != id)
			continue;

		if (h.flags & HIO_VARIABLE) {
			if (h.loc.size != 1)
				return ("bad modifier size");

			/* Single item */
			if (imod < MAXMOD) {
				sc->sc_modloc[imod] = h.loc;
				sc->sc_mods[imod].mask = 1 << imod;
				sc->sc_mods[imod].key = HID_GET_USAGE(h.usage);
				imod++;
			} else
				return ("too many modifier keys");
		} else {
			/* Array */
			if (h.loc.size != 8)
				return ("key code size != 8");

			if (h.loc.count > MAXKEYCODE)
				return ("too many key codes");

			if (h.loc.pos % 8 != 0)
				return ("key codes not on byte boundary");

			if (sc->sc_nkeycode != 0)
				return ("multiple key code arrays\n");

			sc->sc_keycodeloc = h.loc;
			sc->sc_nkeycode = h.loc.count;
		}
	}
	sc->sc_nmod = imod;
	hid_end_parse(d);

	hid_locate(desc, dlen, HID_USAGE2(HUP_LEDS, HUD_LED_NUM_LOCK),
	    id, hid_output, &sc->sc_numloc, NULL);

	hid_locate(desc, dlen, HID_USAGE2(HUP_LEDS, HUD_LED_CAPS_LOCK),
	    id, hid_output, &sc->sc_capsloc, NULL);

	hid_locate(desc, dlen, HID_USAGE2(HUP_LEDS, HUD_LED_SCROLL_LOCK),
	    id, hid_output, &sc->sc_scroloc, NULL);

	return (NULL);
}

int
btkbd_enable(void *self, int on)
{
	struct btkbd_softc *sc = (struct btkbd_softc *)self;

	sc->sc_enabled = on;
	return 0;
}

void
btkbd_set_leds(void *self, int leds)
{
	struct btkbd_softc *sc = (struct btkbd_softc *)self;
	uint8_t report;

	if (sc->sc_leds == leds)
		return;

	sc->sc_leds = leds;

	/*
	 * This is not totally correct, since we did not check the
	 * report size from the descriptor but for keyboards it should
	 * just be a single byte with the relevant bits set.
	 */
	report = 0;
	if ((leds & WSKBD_LED_SCROLL) && sc->sc_scroloc.size == 1)
		report |= 1 << sc->sc_scroloc.pos;

	if ((leds & WSKBD_LED_NUM) && sc->sc_numloc.size == 1)
		report |= 1 << sc->sc_numloc.pos;

	if ((leds & WSKBD_LED_CAPS) && sc->sc_capsloc.size == 1)
		report |= 1 << sc->sc_capsloc.pos;

	if (sc->sc_output)
		(*sc->sc_output)(&sc->sc_hidev, &report, sizeof(report));
}

int
btkbd_ioctl(void *self, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct btkbd_softc *sc = (struct btkbd_softc *)self;

	switch (cmd) {
	case WSKBDIO_GTYPE:
		*(int *)data = WSKBD_TYPE_BLUETOOTH;
		return 0;

	case WSKBDIO_SETLEDS:
		btkbd_set_leds(sc, *(int *)data);
		return 0;

	case WSKBDIO_GETLEDS:
		*(int *)data = sc->sc_leds;
		return 0;

#ifdef WSDISPLAY_COMPAT_RAWKBD
	case WSKBDIO_SETMODE:
		sc->sc_rawkbd = (*(int *)data == WSKBD_RAW);
#ifdef BTKBD_REPEAT
		timeout_del(&sc->sc_repeat);
#endif
		return 0;
#endif
	}
	return -1;
}

#ifdef WSDISPLAY_COMPAT_RAWKBD
#define NN 0			/* no translation */
/*
 * Translate USB keycodes to US keyboard XT scancodes.
 * Scancodes >= 0x80 represent EXTENDED keycodes.
 *
 * See http://www.microsoft.com/HWDEV/TECH/input/Scancode.asp
 */
const u_int8_t btkbd_trtab[256] = {
      NN,   NN,   NN,   NN, 0x1e, 0x30, 0x2e, 0x20, /* 00 - 07 */
    0x12, 0x21, 0x22, 0x23, 0x17, 0x24, 0x25, 0x26, /* 08 - 0f */
    0x32, 0x31, 0x18, 0x19, 0x10, 0x13, 0x1f, 0x14, /* 10 - 17 */
    0x16, 0x2f, 0x11, 0x2d, 0x15, 0x2c, 0x02, 0x03, /* 18 - 1f */
    0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, /* 20 - 27 */
    0x1c, 0x01, 0x0e, 0x0f, 0x39, 0x0c, 0x0d, 0x1a, /* 28 - 2f */
    0x1b, 0x2b, 0x2b, 0x27, 0x28, 0x29, 0x33, 0x34, /* 30 - 37 */
    0x35, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, /* 38 - 3f */
    0x41, 0x42, 0x43, 0x44, 0x57, 0x58, 0xaa, 0x46, /* 40 - 47 */
    0x7f, 0xd2, 0xc7, 0xc9, 0xd3, 0xcf, 0xd1, 0xcd, /* 48 - 4f */
    0xcb, 0xd0, 0xc8, 0x45, 0xb5, 0x37, 0x4a, 0x4e, /* 50 - 57 */
    0x9c, 0x4f, 0x50, 0x51, 0x4b, 0x4c, 0x4d, 0x47, /* 58 - 5f */
    0x48, 0x49, 0x52, 0x53, 0x56, 0xdd,   NN, 0x59, /* 60 - 67 */
    0x5d, 0x5e, 0x5f,   NN,   NN,   NN,   NN,   NN, /* 68 - 6f */
      NN,   NN,   NN,   NN,   NN,   NN,   NN,   NN, /* 70 - 77 */
      NN,   NN,   NN,   NN,   NN,   NN,   NN,   NN, /* 78 - 7f */
      NN,   NN,   NN,   NN,   NN, 0x7e,   NN, 0x73, /* 80 - 87 */
    0x70, 0x7d, 0x79, 0x7b, 0x5c,   NN,   NN,   NN, /* 88 - 8f */
      NN,   NN, 0x78, 0x77, 0x76,   NN,   NN,   NN, /* 90 - 97 */
      NN,   NN,   NN,   NN,   NN,   NN,   NN,   NN, /* 98 - 9f */
      NN,   NN,   NN,   NN,   NN,   NN,   NN,   NN, /* a0 - a7 */
      NN,   NN,   NN,   NN,   NN,   NN,   NN,   NN, /* a8 - af */
      NN,   NN,   NN,   NN,   NN,   NN,   NN,   NN, /* b0 - b7 */
      NN,   NN,   NN,   NN,   NN,   NN,   NN,   NN, /* b8 - bf */
      NN,   NN,   NN,   NN,   NN,   NN,   NN,   NN, /* c0 - c7 */
      NN,   NN,   NN,   NN,   NN,   NN,   NN,   NN, /* c8 - cf */
      NN,   NN,   NN,   NN,   NN,   NN,   NN,   NN, /* d0 - d7 */
      NN,   NN,   NN,   NN,   NN,   NN,   NN,   NN, /* d8 - df */
    0x1d, 0x2a, 0x38, 0xdb, 0x9d, 0x36, 0xb8, 0xdc, /* e0 - e7 */
      NN,   NN,   NN,   NN,   NN,   NN,   NN,   NN, /* e8 - ef */
      NN,   NN,   NN,   NN,   NN,   NN,   NN,   NN, /* f0 - f7 */
      NN,   NN,   NN,   NN,   NN,   NN,   NN,   NN, /* f8 - ff */
};
#endif

#define KEY_ERROR	0x01
#define PRESS		0x000
#define RELEASE		0x100
#define CODEMASK	0x0ff
#define ADDKEY(c)	ibuf[nkeys++] = (c)
#define REP_DELAY1	400
#define REP_DELAYN	100

void
btkbd_input(struct bthidev *self, uint8_t *data, int len)
{
	struct btkbd_softc *sc = (struct btkbd_softc *)self;
	struct btkbd_data *ud = &sc->sc_ndata;
	uint16_t ibuf[MAXKEYS];
	uint32_t mod, omod;
	int nkeys, i, j;
	int key;
	int s;

	if (sc->sc_wskbd == NULL || sc->sc_enabled == 0)
		return;

	/* extract key modifiers */
	ud->modifiers = 0;
	for (i = 0 ; i < sc->sc_nmod ; i++)
		if (hid_get_data(data, &sc->sc_modloc[i]))
			ud->modifiers |= sc->sc_mods[i].mask;

	/* extract keycodes */
	memcpy(ud->keycode, data + (sc->sc_keycodeloc.pos / 8),
	    sc->sc_nkeycode);

	if (ud->keycode[0] == KEY_ERROR)
		return;		/* ignore  */

	nkeys = 0;
	mod = ud->modifiers;
	omod = sc->sc_odata.modifiers;
	if (mod != omod)
		for (i = 0 ; i < sc->sc_nmod ; i++)
			if ((mod & sc->sc_mods[i].mask) !=
			    (omod & sc->sc_mods[i].mask))
				ADDKEY(sc->sc_mods[i].key |
				    (mod & sc->sc_mods[i].mask
				    ? PRESS : RELEASE));

	if (memcmp(ud->keycode, sc->sc_odata.keycode, sc->sc_nkeycode) != 0) {
		/* Check for released keys. */
		for (i = 0 ; i < sc->sc_nkeycode ; i++) {
			key = sc->sc_odata.keycode[i];
			if (key == 0)
				continue;

			for (j = 0 ; j < sc->sc_nkeycode ; j++)
				if (key == ud->keycode[j])
					goto rfound;

			ADDKEY(key | RELEASE);

		rfound:
			;
		}

		/* Check for pressed keys. */
		for (i = 0 ; i < sc->sc_nkeycode ; i++) {
			key = ud->keycode[i];
			if (key == 0)
				continue;

			for (j = 0; j < sc->sc_nkeycode; j++)
				if (key == sc->sc_odata.keycode[j])
					goto pfound;

			ADDKEY(key | PRESS);
		pfound:
			;
		}
	}
	sc->sc_odata = *ud;

	if (nkeys == 0)
		return;

#ifdef WSDISPLAY_COMPAT_RAWKBD
	if (sc->sc_rawkbd) {
		u_char cbuf[MAXKEYS * 2];
		int c;
		int npress;

		for (npress = i = j = 0 ; i < nkeys ; i++) {
			key = ibuf[i];
			c = btkbd_trtab[key & CODEMASK];
			if (c == NN)
				continue;

			if (c & 0x80)
				cbuf[j++] = 0xe0;

			cbuf[j] = c & 0x7f;
			if (key & RELEASE)
				cbuf[j] |= 0x80;
#ifdef BTKBD_REPEAT
			else {
				/* remember pressed keys for autorepeat */
				if (c & 0x80)
					sc->sc_rep[npress++] = 0xe0;

				sc->sc_rep[npress++] = c & 0x7f;
			}
#endif

			j++;
		}

		s = spltty();
		wskbd_rawinput(sc->sc_wskbd, cbuf, j);
		splx(s);
#ifdef BTKBD_REPEAT
		timeout_del(&sc->sc_repeat);
		if (npress != 0) {
			sc->sc_nrep = npress;
			timeout_del(&sc->sc_repeat);
			timeout_set(&sc->sc_repeat, btkbd_repeat, sc);
			timeout_add(&sc->sc_repeat, hz * REP_DELAY1 / 1000);
		}
#endif
		return;
	}
#endif

	s = spltty();
	for (i = 0 ; i < nkeys ; i++) {
		key = ibuf[i];
		wskbd_input(sc->sc_wskbd,
		    key & RELEASE ? WSCONS_EVENT_KEY_UP : WSCONS_EVENT_KEY_DOWN,
		    key & CODEMASK);
	}
	splx(s);
}

#ifdef WSDISPLAY_COMPAT_RAWKBD
#ifdef BTKBD_REPEAT
void
btkbd_repeat(void *arg)
{
	struct btkbd_softc *sc = arg;
	int s;

	s = spltty();
	wskbd_rawinput(sc->sc_wskbd, sc->sc_rep, sc->sc_nrep);
	splx(s);
	timeout_del(&sc->sc_repeat);
	timeout_set(&sc->sc_repeat, btkbd_repeat, sc);
	timeout_add(&sc->sc_repeat, hz * REP_DELAYN / 1000);
}
#endif
#endif
