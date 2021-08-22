/*	$OpenBSD: ucc.c,v 1.4 2021/08/22 07:20:40 anton Exp $	*/

/*
 * Copyright (c) 2021 Anton Lindqvist <anton@openbsd.org>
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
#include <sys/device.h>
#include <sys/malloc.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbhid.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/uhidev.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wskbdvar.h>
#include <dev/wscons/wsksymdef.h>
#include <dev/wscons/wsksymvar.h>

/* #define UCC_DEBUG */
#ifdef UCC_DEBUG
#define DPRINTF(x...)	do { if (ucc_debug) printf(x); } while (0)
void	ucc_dump(const char *, u_char *, u_int);
int	ucc_debug = 1;
#else
#define DPRINTF(x...)
#define ucc_dump(prefix, data, len)
#endif

struct ucc_softc {
	struct uhidev		  sc_hdev;
	struct device		 *sc_wskbddev;

	/* Key mappings used in translating mode. */
	keysym_t		 *sc_map;
	u_int			  sc_maplen;
	u_int			  sc_mapsiz;
	u_int			  sc_nkeys;

	/* Key mappings used in raw mode. */
	const struct ucc_keysym	**sc_raw;
	u_int			  sc_rawlen;
	u_int			  sc_rawsiz;

	int			  sc_mode;

	/* Last pressed key. */
	union {
		int	sc_last_translate;
		u_char	sc_last_raw;
	};

	struct wscons_keydesc	  sc_keydesc[2];
	struct wskbd_mapdata	  sc_keymap;
};

struct ucc_keysym {
	const char	*us_name;
	int32_t		 us_usage;
	keysym_t	 us_key;
	u_char		 us_raw;
};

int	ucc_match(struct device *, void *, void *);
void	ucc_attach(struct device *, struct device *, void *);
int	ucc_detach(struct device *, int);
void	ucc_intr(struct uhidev *, void *, u_int);

void	ucc_attach_wskbd(struct ucc_softc *);
int	ucc_enable(void *, int);
void	ucc_set_leds(void *, int);
int	ucc_ioctl(void *, u_long, caddr_t, int, struct proc *);

int	ucc_parse_hid(struct ucc_softc *, void *, int);
int	ucc_bit_to_raw(struct ucc_softc *, u_int, u_char *);
int	ucc_usage_to_sym(int32_t, const struct ucc_keysym **);
void	ucc_raw_to_scancode(u_char *, int *, u_char, int);
void	ucc_input(struct ucc_softc *, u_int, int);
void	ucc_rawinput(struct ucc_softc *, u_char, int);
int	ucc_setbits(u_char *, int, u_int *);

struct cfdriver ucc_cd = {
	NULL, "ucc", DV_DULL
};

const struct cfattach ucc_ca = {
	sizeof(struct ucc_softc),
	ucc_match,
	ucc_attach,
	ucc_detach,
};

/*
 * Mapping of HID consumer control usages to key symbols.
 * The raw scan codes are taken from X11, see the media_common symbols in
 * dist/xkeyboard-config/symbols/inet.
 * Then use dist/xkeyboard-config/keycodes/xfree86 to resolve keys to the
 * corresponding raw scan code.
 */
static const struct ucc_keysym ucc_keysyms[] = {
#ifdef UCC_DEBUG
#define U(x)	#x, x
#else
#define U(x)	NULL, x
#endif
	{ U(HUC_MUTE),		KS_AudioMute,	0 },
	{ U(HUC_VOL_INC),	KS_AudioRaise,	0 },
	{ U(HUC_VOL_DEC),	KS_AudioLower,	0 },
	{ U(HUC_TRACK_NEXT),	0,		153 /* I19 = XF86AudioNext */ },
	{ U(HUC_TRACK_PREV),	0,		144 /* I10 = XF86AudioPrev */ },
	{ U(HUC_STOP),		0,		164 /* I24 = XF86AudioStop */ },
	{ U(HUC_PLAY_PAUSE),	0,		162 /* I22 = XF86AudioPlay, XF86AudioPause */ },
#undef U
};

int
ucc_match(struct device *parent, void *match, void *aux)
{
	struct uhidev_attach_arg *uha = (struct uhidev_attach_arg *)aux;
	void *desc;
	int size;

	uhidev_get_report_desc(uha->parent, &desc, &size);
	if (!hid_is_collection(desc, size, uha->reportid,
	    HID_USAGE2(HUP_CONSUMER, HUC_CONTROL)))
		return UMATCH_NONE;

	return UMATCH_IFACECLASS;
}

void
ucc_attach(struct device *parent, struct device *self, void *aux)
{
	struct ucc_softc *sc = (struct ucc_softc *)self;
	struct uhidev_attach_arg *uha = (struct uhidev_attach_arg *)aux;
	void *desc;
	int error, repid, size;

	sc->sc_mode = WSKBD_TRANSLATED;
	sc->sc_last_translate = -1;

	sc->sc_hdev.sc_intr = ucc_intr;
	sc->sc_hdev.sc_parent = uha->parent;
	sc->sc_hdev.sc_udev = uha->uaa->device;
	sc->sc_hdev.sc_report_id = uha->reportid;

	uhidev_get_report_desc(uha->parent, &desc, &size);
	repid = uha->reportid;
	sc->sc_hdev.sc_isize = hid_report_size(desc, size, hid_input, repid);
	sc->sc_hdev.sc_osize = hid_report_size(desc, size, hid_output, repid);
	sc->sc_hdev.sc_fsize = hid_report_size(desc, size, hid_feature, repid);

	error = ucc_parse_hid(sc, desc, size);
	if (error) {
		printf(" hid error %d\n", error);
		return;
	}

	printf(" %d key%s, %d mapping%s\n",
	    sc->sc_nkeys, sc->sc_nkeys == 1 ? "" : "s",
	    sc->sc_rawlen, sc->sc_rawlen == 1 ? "" : "s");

	/* Cannot load an empty map. */
	if (sc->sc_maplen > 0)
		ucc_attach_wskbd(sc);
}

int
ucc_detach(struct device *self, int flags)
{
	struct ucc_softc *sc = (struct ucc_softc *)self;
	int error = 0;

	if (sc->sc_wskbddev != NULL)
		error = config_detach(sc->sc_wskbddev, flags);
	uhidev_close(&sc->sc_hdev);
	free(sc->sc_map, M_USBDEV, sc->sc_mapsiz);
	free(sc->sc_raw, M_USBDEV, sc->sc_rawsiz);
	return error;
}

void
ucc_intr(struct uhidev *addr, void *data, u_int len)
{
	struct ucc_softc *sc = (struct ucc_softc *)addr;
	int raw = sc->sc_mode == WSKBD_RAW;
	u_int bit = 0;

	ucc_dump(__func__, data, len);

	if (ucc_setbits(data, len, &bit)) {
		/* All zeroes, assume key up event. */
		if (raw) {
			if (sc->sc_last_raw != 0) {
				ucc_rawinput(sc, sc->sc_last_raw, 1);
				sc->sc_last_raw = 0;
			}
		} else {
			if (sc->sc_last_translate != -1) {
				ucc_input(sc, sc->sc_last_translate, 1);
				sc->sc_last_translate = -1;
			}
		}
		return;
	}
	if (bit >= sc->sc_nkeys)
		goto unknown;

	if (raw) {
		u_char c;

		if (ucc_bit_to_raw(sc, bit, &c))
			goto unknown;
		if (c != 0) {
			ucc_rawinput(sc, c, 0);
			sc->sc_last_raw = c;
			return;
		}

		/*
		 * The pressed key does not have a corresponding raw scan code
		 * which implies that wsbkd must handle the pressed key as if
		 * being in translating mode, hence the fall through. This is
		 * only the case for volume related keys.
		 */
	}

	ucc_input(sc, bit, 0);
	if (!raw)
		sc->sc_last_translate = bit;
	return;

unknown:
	DPRINTF("%s: unknown key: bit %d\n", __func__, bit);
}

void
ucc_attach_wskbd(struct ucc_softc *sc)
{
	static const struct wskbd_accessops accessops = {
		.enable		= ucc_enable,
		.set_leds	= ucc_set_leds,
		.ioctl		= ucc_ioctl,
	};
	struct wskbddev_attach_args a = {
		.console	= 0,
		.keymap		= &sc->sc_keymap,
		.accessops	= &accessops,
		.accesscookie	= sc,
	};

	sc->sc_keydesc[0].name = KB_US;
	sc->sc_keydesc[0].base = 0;
	sc->sc_keydesc[0].map_size = sc->sc_maplen;
	sc->sc_keydesc[0].map = sc->sc_map;
	sc->sc_keymap.keydesc = sc->sc_keydesc;
	sc->sc_keymap.layout = KB_US | KB_DEFAULT;
	sc->sc_wskbddev = config_found(&sc->sc_hdev.sc_dev, &a, wskbddevprint);
}

int
ucc_enable(void *v, int on)
{
	struct ucc_softc *sc = (struct ucc_softc *)v;
	int error = 0;

	if (on)
		error = uhidev_open(&sc->sc_hdev);
	else
		uhidev_close(&sc->sc_hdev);
	return error;
}

void
ucc_set_leds(void *v, int leds)
{
}

int
ucc_ioctl(void *v, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	switch (cmd) {
	/* wsconsctl(8) stub */
	case WSKBDIO_GTYPE:
		*(int *)data = WSKBD_TYPE_USB;
		return 0;

	/* wsconsctl(8) stub */
	case WSKBDIO_GETLEDS:
		*(int *)data = 0;
		return 0;

#ifdef WSDISPLAY_COMPAT_RAWKBD
	case WSKBDIO_SETMODE: {
		struct ucc_softc *sc = (struct ucc_softc *)v;

		sc->sc_mode = *(int *)data;
		return 0;
	}
#endif
	}

	return -1;
}

/*
 * Parse the HID report and construct a mapping between the bits in the input
 * report and the corresponding pressed key.
 */
int
ucc_parse_hid(struct ucc_softc *sc, void *desc, int descsiz)
{
	struct hid_item hi;
	struct hid_data *hd;
	u_int mapsiz, rawsiz;
	int isize;

	/*
	 * The size of the input report is expressed in bytes where each bit in
	 * turn represents a pressed key. It's likely that the number of keys is
	 * less than this generous estimate.
	 */
	isize = sc->sc_hdev.sc_isize * 8;
	if (isize == 0)
		return ENXIO;

	/*
	 * Create mapping between each input bit and the corresponding key used
	 * in translating mode. Two entries are needed per bit in order
	 * construct a mapping.
	 */
	mapsiz = isize * 2;
	sc->sc_mapsiz = mapsiz * sizeof(*sc->sc_map);
	sc->sc_map = mallocarray(isize, 2 * sizeof(*sc->sc_map), M_USBDEV,
	    M_WAITOK | M_ZERO);

	/*
	 * Create mapping between each input bit and the corresponding scan
	 * code used in raw mode.
	 */
	rawsiz = isize;
	sc->sc_rawsiz = rawsiz * sizeof(*sc->sc_raw);
	sc->sc_raw = mallocarray(isize, sizeof(*sc->sc_raw), M_USBDEV,
	    M_WAITOK | M_ZERO);

	hd = hid_start_parse(desc, descsiz, hid_input);
	while (hid_get_item(hd, &hi)) {
		const struct ucc_keysym *us;
		int bit;

		if (HID_GET_USAGE_PAGE(hi.usage) != HUP_CONSUMER ||
		    HID_GET_USAGE(hi.usage) == HUC_CONTROL)
			continue;

		bit = sc->sc_nkeys++;
		if (ucc_usage_to_sym(HID_GET_USAGE(hi.usage), &us))
			continue;

		if (sc->sc_maplen + 2 >= mapsiz)
			return ENOMEM;
		sc->sc_map[sc->sc_maplen++] = KS_KEYCODE(bit);
		sc->sc_map[sc->sc_maplen++] = us->us_key;

		if (bit >= rawsiz)
			return ENOMEM;
		sc->sc_raw[bit] = us;
		sc->sc_rawlen++;

		DPRINTF("%s: bit %d, usage %s\n", __func__,
		    bit, us->us_name);
	}
	hid_end_parse(hd);

	return 0;
}

int
ucc_bit_to_raw(struct ucc_softc *sc, u_int bit, u_char *raw)
{
	if (bit >= sc->sc_nkeys)
		return 1;
	*raw = sc->sc_raw[bit]->us_raw;
	return 0;
}

int
ucc_usage_to_sym(int32_t usage, const struct ucc_keysym **us)
{
	int len = nitems(ucc_keysyms);
	int i;

	for (i = 0; i < len; i++) {
		if (ucc_keysyms[i].us_usage == usage) {
			*us = &ucc_keysyms[i];
			return 0;
		}
	}
	return 1;
}

void
ucc_input(struct ucc_softc *sc, u_int bit, int release)
{
	int s;

	s = spltty();
	wskbd_input(sc->sc_wskbddev,
	    release ? WSCONS_EVENT_KEY_UP : WSCONS_EVENT_KEY_DOWN, bit);
	splx(s);
}

void
ucc_rawinput(struct ucc_softc *sc, u_char c, int release)
{
	u_char buf[2];
	int len = 0;
	int s;

	if (c & 0x80)
		buf[len++] = 0xe0;
	buf[len++] = c & 0x7f;
	if (release)
		buf[len - 1] |= 0x80;

	s = spltty();
	wskbd_rawinput(sc->sc_wskbddev, buf, len);
	splx(s);
}

int
ucc_setbits(u_char *data, int len, u_int *bit)
{
	int i, j;

	for (i = 0; i < len; i++) {
		if (data[i] == 0)
			continue;

		for (j = 0; j < 8; j++) {
			if (data[i] & (1 << j)) {
				*bit = (i * 8) + j;
				return 0;
			}
		}
	}

	return 1;
}

#ifdef UCC_DEBUG

void
ucc_dump(const char *prefix, u_char *data, u_int len)
{
	u_int i;

	if (ucc_debug == 0)
		return;

	printf("%s:", prefix);
	for (i = 0; i < len; i++)
		printf(" %02x", data[i]);
	printf("\n");
}

#endif
