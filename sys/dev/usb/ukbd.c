/*	$OpenBSD: ukbd.c,v 1.8 2001/10/25 14:36:11 drahn Exp $	*/
/*      $NetBSD: ukbd.c,v 1.66 2001/04/06 22:54:15 augustss Exp $        */

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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
 * HID spec: http://www.usb.org/developers/data/devclass/hid1_1.pdf
 */

#include <sys/param.h>
#include <sys/systm.h>
#if defined(__OpenBSD__)
#include <sys/timeout.h>
#else
#include <sys/callout.h>
#endif
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/file.h>
#include <sys/select.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/poll.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbhid.h>

#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>
#include <dev/usb/usb_quirks.h>
#include <dev/usb/hid.h>
#include <dev/usb/ukbdvar.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wskbdvar.h>
#include <dev/wscons/wsksymdef.h>
#include <dev/wscons/wsksymvar.h>

#if defined(__NetBSD__)
#include "opt_wsdisplay_compat.h"
#endif

#ifdef UKBD_DEBUG
#define DPRINTF(x)	if (ukbddebug) logprintf x
#define DPRINTFN(n,x)	if (ukbddebug>(n)) logprintf x
int	ukbddebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

#define NKEYCODE 6

#define NUM_LOCK 0x01
#define CAPS_LOCK 0x02
#define SCROLL_LOCK 0x04

struct ukbd_data {
	u_int8_t	modifiers;
#define MOD_CONTROL_L	0x01
#define MOD_CONTROL_R	0x10
#define MOD_SHIFT_L	0x02
#define MOD_SHIFT_R	0x20
#define MOD_ALT_L	0x04
#define MOD_ALT_R	0x40
#define MOD_WIN_L	0x08
#define MOD_WIN_R	0x80
	u_int8_t	reserved;
	u_int8_t	keycode[NKEYCODE];
};

#define PRESS    0x000
#define RELEASE  0x100
#define CODEMASK 0x0ff

/* Translate USB bitmap to USB keycode. */
#define NMOD 8
Static const struct {
	int mask, key;
} ukbd_mods[NMOD] = {
	{ MOD_CONTROL_L, 224 },
	{ MOD_CONTROL_R, 228 },
	{ MOD_SHIFT_L,   225 },
	{ MOD_SHIFT_R,   229 },
	{ MOD_ALT_L,     226 },
	{ MOD_ALT_R,     230 },
	{ MOD_WIN_L,     227 },
	{ MOD_WIN_R,     231 },
};

#if defined(WSDISPLAY_COMPAT_RAWKBD)
#define NN 0			/* no translation */
/* 
 * Translate USB keycodes to US keyboard XT scancodes.
 * Scancodes >= 128 represent EXTENDED keycodes.
 */
Static const u_int8_t ukbd_trtab[256] = {
	  NN,  NN,  NN,  NN,  30,  48,  46,  32, /* 00 - 07 */
	  18,  33,  34,  35,  23,  36,  37,  38, /* 08 - 0F */
	  50,  49,  24,  25,  16,  19,  31,  20, /* 10 - 17 */
	  22,  47,  17,  45,  21,  44,   2,   3, /* 18 - 1F */
	   4,   5,   6,   7,   8,   9,  10,  11, /* 20 - 27 */
	  28,   1,  14,  15,  57,  12,  13,  26, /* 28 - 2F */
	  27,  43,  43,  39,  40,  41,  51,  52, /* 30 - 37 */
	  53,  58,  59,  60,  61,  62,  63,  64, /* 38 - 3F */
	  65,  66,  67,  68,  87,  88, 170,  70, /* 40 - 47 */
	 127, 210, 199, 201, 211, 207, 209, 205, /* 48 - 4F */
	 203, 208, 200,  69, 181,  55,  74,  78, /* 50 - 57 */
	 156,  79,  80,  81,  75,  76,  77,  71, /* 58 - 5F */
          72,  73,  82,  83,  86, 221,  NN,  NN, /* 60 - 67 */
          NN,  NN,  NN,  NN,  NN,  NN,  NN,  NN, /* 68 - 6F */
          NN,  NN,  NN,  NN,  NN,  NN,  NN,  NN, /* 70 - 77 */
          NN,  NN,  NN,  NN,  NN,  NN,  NN,  NN, /* 78 - 7F */
          NN,  NN,  NN,  NN,  NN,  NN,  NN,  NN, /* 80 - 87 */
          NN,  NN,  NN,  NN,  NN,  NN,  NN,  NN, /* 88 - 8F */
          NN,  NN,  NN,  NN,  NN,  NN,  NN,  NN, /* 90 - 97 */
          NN,  NN,  NN,  NN,  NN,  NN,  NN,  NN, /* 98 - 9F */
          NN,  NN,  NN,  NN,  NN,  NN,  NN,  NN, /* A0 - A7 */
          NN,  NN,  NN,  NN,  NN,  NN,  NN,  NN, /* A8 - AF */
          NN,  NN,  NN,  NN,  NN,  NN,  NN,  NN, /* B0 - B7 */
          NN,  NN,  NN,  NN,  NN,  NN,  NN,  NN, /* B8 - BF */
          NN,  NN,  NN,  NN,  NN,  NN,  NN,  NN, /* C0 - C7 */
          NN,  NN,  NN,  NN,  NN,  NN,  NN,  NN, /* C8 - CF */
          NN,  NN,  NN,  NN,  NN,  NN,  NN,  NN, /* D0 - D7 */
          NN,  NN,  NN,  NN,  NN,  NN,  NN,  NN, /* D8 - DF */
          29,  42,  56, 219,  157, 54,  184,220, /* E0 - E7 */
          NN,  NN,  NN,  NN,  NN,  NN,  NN,  NN, /* E8 - EF */
          NN,  NN,  NN,  NN,  NN,  NN,  NN,  NN, /* F0 - F7 */
          NN,  NN,  NN,  NN,  NN,  NN,  NN,  NN, /* F8 - FF */
};
#endif /* defined(WSDISPLAY_COMPAT_RAWKBD) */

#define KEY_ERROR 0x01

#define MAXKEYS (NMOD+2*NKEYCODE)

struct ukbd_softc {
	USBBASEDEVICE	sc_dev;		/* base device */
	usbd_device_handle sc_udev;
	usbd_interface_handle sc_iface;	/* interface */
	usbd_pipe_handle sc_intrpipe;	/* interrupt pipe */
	int sc_ep_addr;

	struct ukbd_data sc_ndata;
	struct ukbd_data sc_odata;

	char sc_enabled;

	int sc_console_keyboard;	/* we are the console keyboard */

	char sc_debounce;		/* for quirk handling */
	struct ukbd_data sc_data;	/* for quirk handling */

	int sc_leds;

#if defined(__OpenBSD__)
	struct timeout sc_delay;	/* for quirk handling */
	struct timeout sc_rawrepeat_ch;
#else
	struct callout sc_delay;	/* for quirk handling */
	struct callout sc_rawrepeat_ch;
#endif

#if defined(__NetBSD__) || defined(__OpenBSD__)
	struct device *sc_wskbddev;
#if defined(WSDISPLAY_COMPAT_RAWKBD)
#define REP_DELAY1 400
#define REP_DELAYN 100
	int sc_rawkbd;
	int sc_nrep;
	char sc_rep[MAXKEYS];
#endif /* defined(WSDISPLAY_COMPAT_RAWKBD) */

	int sc_polling;
	int sc_npollchar;
	u_int16_t sc_pollchars[MAXKEYS];
#endif

	u_char sc_dying;
};

#ifdef UKBD_DEBUG
#define UKBDTRACESIZE 64
struct ukbdtraceinfo {
	int unit;
	struct timeval tv;
	struct ukbd_data ud;
};
struct ukbdtraceinfo ukbdtracedata[UKBDTRACESIZE];
int ukbdtraceindex = 0;
int ukbdtrace = 0;
void ukbdtracedump(void);
void
ukbdtracedump(void)
{
	int i;
	for (i = 0; i < UKBDTRACESIZE; i++) {
		struct ukbdtraceinfo *p = 
		    &ukbdtracedata[(i+ukbdtraceindex)%UKBDTRACESIZE];
		printf("%lu.%06lu: mod=0x%02x key0=0x%02x key1=0x%02x "
		       "key2=0x%02x key3=0x%02x\n",
		       p->tv.tv_sec, p->tv.tv_usec,
		       p->ud.modifiers, p->ud.keycode[0], p->ud.keycode[1],
		       p->ud.keycode[2], p->ud.keycode[3]);
	}
}
#endif

#define	UKBDUNIT(dev)	(minor(dev))
#define	UKBD_CHUNK	128	/* chunk size for read */
#define	UKBD_BSIZE	1020	/* buffer size */

Static int	ukbd_is_console;

Static void	ukbd_cngetc(void *, u_int *, int *);
Static void	ukbd_cnpollc(void *, int);

#if defined(__NetBSD__) || defined(__OpenBSD__)
const struct wskbd_consops ukbd_consops = {
	ukbd_cngetc,
	ukbd_cnpollc,
};
#endif

Static void	ukbd_intr(usbd_xfer_handle, usbd_private_handle, usbd_status);
Static void	ukbd_decode(struct ukbd_softc *sc, struct ukbd_data *ud);
Static void	ukbd_delayed_decode(void *addr);

Static int	ukbd_enable(void *, int);
Static void	ukbd_set_leds(void *, int);

#if defined(__NetBSD__) || defined(__OpenBSD__)
Static int	ukbd_ioctl(void *, u_long, caddr_t, int, struct proc *);
#ifdef WSDISPLAY_COMPAT_RAWKBD
Static void	ukbd_rawrepeat(void *v);
#endif

const struct wskbd_accessops ukbd_accessops = {
	ukbd_enable,
	ukbd_set_leds,
	ukbd_ioctl,
};

extern const struct wscons_keydesc ukbd_keydesctab[];

const struct wskbd_mapdata ukbd_keymapdata = {
	ukbd_keydesctab,
#ifdef UKBD_LAYOUT
	UKBD_LAYOUT,
#else
	KB_US,
#endif
};
#endif

USB_DECLARE_DRIVER(ukbd);

USB_MATCH(ukbd)
{
	USB_MATCH_START(ukbd, uaa);
	usb_interface_descriptor_t *id;
	
	/* Check that this is a keyboard that speaks the boot protocol. */
	if (uaa->iface == NULL)
		return (UMATCH_NONE);
	id = usbd_get_interface_descriptor(uaa->iface);
	if (id == NULL ||
	    id->bInterfaceClass != UICLASS_HID || 
	    id->bInterfaceSubClass != UISUBCLASS_BOOT ||
	    id->bInterfaceProtocol != UIPROTO_BOOT_KEYBOARD)
		return (UMATCH_NONE);
	return (UMATCH_IFACECLASS_IFACESUBCLASS_IFACEPROTO);
}

USB_ATTACH(ukbd)
{
	USB_ATTACH_START(ukbd, sc, uaa);
	usbd_interface_handle iface = uaa->iface;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	usbd_status err;
	u_int32_t qflags;
	char devinfo[1024];
#if defined(__NetBSD__) || defined(__OpenBSD__)
	struct wskbddev_attach_args a;
#else
	int i;
#endif
	
	sc->sc_udev = uaa->device;
	sc->sc_iface = iface;
	id = usbd_get_interface_descriptor(iface);
	usbd_devinfo(uaa->device, 0, devinfo);
	USB_ATTACH_SETUP;
	printf("%s: %s, iclass %d/%d\n", USBDEVNAME(sc->sc_dev),
	       devinfo, id->bInterfaceClass, id->bInterfaceSubClass);

	ed = usbd_interface2endpoint_descriptor(iface, 0);
	if (ed == NULL) {
		printf("%s: could not read endpoint descriptor\n",
		       USBDEVNAME(sc->sc_dev));
		USB_ATTACH_ERROR_RETURN;
	}

	DPRINTFN(10,("ukbd_attach: bLength=%d bDescriptorType=%d "
		     "bEndpointAddress=%d-%s bmAttributes=%d wMaxPacketSize=%d"
		     " bInterval=%d\n",
		     ed->bLength, ed->bDescriptorType, 
		     ed->bEndpointAddress & UE_ADDR,
		     UE_GET_DIR(ed->bEndpointAddress)==UE_DIR_IN? "in" : "out",
		     ed->bmAttributes & UE_XFERTYPE,
		     UGETW(ed->wMaxPacketSize), ed->bInterval));

	if (UE_GET_DIR(ed->bEndpointAddress) != UE_DIR_IN ||
	    (ed->bmAttributes & UE_XFERTYPE) != UE_INTERRUPT) {
		printf("%s: unexpected endpoint\n",
		       USBDEVNAME(sc->sc_dev));
		USB_ATTACH_ERROR_RETURN;
	}

	qflags = usbd_get_quirks(uaa->device)->uq_flags;
	if ((qflags & UQ_NO_SET_PROTO) == 0) {
		err = usbd_set_protocol(iface, 0);
		DPRINTFN(5, ("ukbd_attach: protocol set\n"));
		if (err) {
			printf("%s: set protocol failed\n",
			    USBDEVNAME(sc->sc_dev));
			USB_ATTACH_ERROR_RETURN;
		}
	}
	sc->sc_debounce = (qflags & UQ_SPUR_BUT_UP) != 0;

	/* Ignore if SETIDLE fails since it is not crucial. */
	(void)usbd_set_idle(iface, 0, 0);

	sc->sc_ep_addr = ed->bEndpointAddress;

	/*
	 * Remember if we're the console keyboard.
	 *
	 * XXX This always picks the first keyboard on the
	 * first USB bus, but what else can we really do?
	 */
	if ((sc->sc_console_keyboard = ukbd_is_console) != 0) {
		/* Don't let any other keyboard have it. */
		ukbd_is_console = 0;
	}

	if (sc->sc_console_keyboard) {
		DPRINTF(("ukbd_attach: console keyboard sc=%p\n", sc));
		wskbd_cnattach(&ukbd_consops, sc, &ukbd_keymapdata);
		ukbd_enable(sc, 1);
	}

	a.console = sc->sc_console_keyboard;

	a.keymap = &ukbd_keymapdata;

	a.accessops = &ukbd_accessops;
	a.accesscookie = sc;

#if defined(__OpenBSD__)
#ifdef WSDISPLAY_COMPAT_RAWKBD
	timeout_set(&sc->sc_rawrepeat_ch, ukbd_rawrepeat, sc);
#endif
	timeout_set(&sc->sc_delay, ukbd_delayed_decode, sc);
#endif

#if defined(__NetBSD__)
	callout_init(&sc->sc_rawrepeat_ch);
	callout_init(&sc->sc_delay);
#endif

	/* Flash the leds; no real purpose, just shows we're alive. */
	ukbd_set_leds(sc, WSKBD_LED_SCROLL | WSKBD_LED_NUM | WSKBD_LED_CAPS);
	usbd_delay_ms(uaa->device, 400);
	ukbd_set_leds(sc, 0);

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev,
			   USBDEV(sc->sc_dev));

	sc->sc_wskbddev = config_found(self, &a, wskbddevprint);

	USB_ATTACH_SUCCESS_RETURN;
}

int
ukbd_enable(void *v, int on)
{
	struct ukbd_softc *sc = v;
	usbd_status err;

	if (on && sc->sc_dying)
		return (EIO);

	/* Should only be called to change state */
	if (sc->sc_enabled == on) {
#ifdef DIAGNOSTIC
		printf("ukbd_enable: %s: bad call on=%d\n", 
		       USBDEVNAME(sc->sc_dev), on);
#endif
		return (EBUSY);
	}

	DPRINTF(("ukbd_enable: sc=%p on=%d\n", sc, on));
	if (on) {
		/* Set up interrupt pipe. */
		err = usbd_open_pipe_intr(sc->sc_iface, sc->sc_ep_addr, 
			  USBD_SHORT_XFER_OK, &sc->sc_intrpipe, sc,
			  &sc->sc_ndata, sizeof(sc->sc_ndata), ukbd_intr,
			  USBD_DEFAULT_INTERVAL);
		if (err)
			return (EIO);
	} else {
		/* Disable interrupts. */
		usbd_abort_pipe(sc->sc_intrpipe);
		usbd_close_pipe(sc->sc_intrpipe);
		sc->sc_intrpipe = NULL;
	}
	sc->sc_enabled = on;

	return (0);
}

int
ukbd_activate(device_ptr_t self, enum devact act)
{
	struct ukbd_softc *sc = (struct ukbd_softc *)self;
	int rv = 0;

	switch (act) {
	case DVACT_ACTIVATE:
		return (EOPNOTSUPP);
		break;

	case DVACT_DEACTIVATE:
		if (sc->sc_wskbddev != NULL)
			rv = config_deactivate(sc->sc_wskbddev);
		sc->sc_dying = 1;
		break;
	}
	return (rv);
}

USB_DETACH(ukbd)
{
	USB_DETACH_START(ukbd, sc);
	int rv = 0;

	DPRINTF(("ukbd_detach: sc=%p flags=%d\n", sc, flags));

	if (sc->sc_console_keyboard) {
#if 0
		/*
		 * XXX Should probably disconnect our consops,
		 * XXX and either notify some other keyboard that
		 * XXX it can now be the console, or if there aren't
		 * XXX any more USB keyboards, set ukbd_is_console
		 * XXX back to 1 so that the next USB keyboard attached
		 * XXX to the system will get it.
		 */
		panic("ukbd_detach: console keyboard");
#else
		/*
		 * Disconnect our consops and set ukbd_is_console
		 * back to 1 so that the next USB keyboard attached
		 * to the system will get it.
		 * XXX Should notify some other keyboard that it can be
		 * XXX console, if there are any other keyboards.
		 */
		printf("%s: was console keyboard\n", USBDEVNAME(sc->sc_dev));
		wskbd_cndetach();
		ukbd_is_console = 1;
#endif
	}
	/* No need to do reference counting of ukbd, wskbd has all the goo. */
	if (sc->sc_wskbddev != NULL)
		rv = config_detach(sc->sc_wskbddev, flags);

	/* The console keyboard does not get a disable call, so check pipe. */
	if (sc->sc_intrpipe != NULL) {
		usbd_abort_pipe(sc->sc_intrpipe);
		usbd_close_pipe(sc->sc_intrpipe);
		sc->sc_intrpipe = NULL;
	}

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev,
			   USBDEV(sc->sc_dev));

	return (rv);
}

void
ukbd_intr(xfer, addr, status)
	usbd_xfer_handle xfer;
	usbd_private_handle addr;
	usbd_status status;
{
	struct ukbd_softc *sc = addr;
	struct ukbd_data *ud = &sc->sc_ndata;

	DPRINTFN(5, ("ukbd_intr: status=%d\n", status));
	if (status == USBD_CANCELLED)
		return;

	if (status) {
		DPRINTF(("ukbd_intr: status=%d\n", status));
		usbd_clear_endpoint_stall_async(sc->sc_intrpipe);
		return;
	}

	if (sc->sc_debounce) {
		/*
		 * Some keyboards have a peculiar quirk.  They sometimes
		 * generate a key up followed by a key down for the same
		 * key after about 10 ms.
		 * We avoid this bug by holding off decoding for 20 ms.
		 */
		sc->sc_data = *ud;
#if defined(__OpenBSD__)
		timeout_add(&sc->sc_delay, hz / 50);
#else
		callout_reset(&sc->sc_delay, hz / 50, ukbd_delayed_decode, sc);
#endif
	} else {
		ukbd_decode(sc, ud);
	}
}

void
ukbd_delayed_decode(void *addr)
{
	struct ukbd_softc *sc = addr;

	ukbd_decode(sc, &sc->sc_data);
}

void
ukbd_decode(struct ukbd_softc *sc, struct ukbd_data *ud)
{
	int mod, omod;
	u_int16_t ibuf[MAXKEYS];	/* chars events */
	int s;
	int nkeys, i, j;
	int key;
#define ADDKEY(c) ibuf[nkeys++] = (c)

#ifdef UKBD_DEBUG
	/* 
	 * Keep a trace of the last events.  Using printf changes the
	 * timing, so this can be useful sometimes.
	 */
	if (ukbdtrace) {
		struct ukbdtraceinfo *p = &ukbdtracedata[ukbdtraceindex];
		p->unit = sc->sc_dev.dv_unit;
		microtime(&p->tv);
		p->ud = *ud;
		if (++ukbdtraceindex >= UKBDTRACESIZE)
			ukbdtraceindex = 0;
	}
	if (ukbddebug > 5) {
		struct timeval tv;
		microtime(&tv);
		DPRINTF((" at %lu.%06lu  mod=0x%02x key0=0x%02x key1=0x%02x "
			 "key2=0x%02x key3=0x%02x\n",
			 tv.tv_sec, tv.tv_usec,
			 ud->modifiers, ud->keycode[0], ud->keycode[1],
			 ud->keycode[2], ud->keycode[3]));
	}
#endif

	if (ud->keycode[0] == KEY_ERROR) {
		DPRINTF(("ukbd_intr: KEY_ERROR\n"));
		return;		/* ignore  */
	}
	nkeys = 0;
	mod = ud->modifiers;
	omod = sc->sc_odata.modifiers;
	if (mod != omod)
		for (i = 0; i < NMOD; i++)
			if (( mod & ukbd_mods[i].mask) != 
			    (omod & ukbd_mods[i].mask))
				ADDKEY(ukbd_mods[i].key | 
				       (mod & ukbd_mods[i].mask 
					  ? PRESS : RELEASE));
	if (memcmp(ud->keycode, sc->sc_odata.keycode, NKEYCODE) != 0) {
		/* Check for released keys. */
		for (i = 0; i < NKEYCODE; i++) {
			key = sc->sc_odata.keycode[i];
			if (key == 0)
				continue;
			for (j = 0; j < NKEYCODE; j++)
				if (key == ud->keycode[j])
					goto rfound;
			DPRINTFN(3,("ukbd_intr: relse key=0x%02x\n", key));
			ADDKEY(key | RELEASE);
		rfound:
			;
		}
		
		/* Check for pressed keys. */
		for (i = 0; i < NKEYCODE; i++) {
			key = ud->keycode[i];
			if (key == 0)
				continue;
			for (j = 0; j < NKEYCODE; j++)
				if (key == sc->sc_odata.keycode[j])
					goto pfound;
			DPRINTFN(2,("ukbd_intr: press key=0x%02x\n", key));
			ADDKEY(key | PRESS);
		pfound:
			;
		}
	}
	sc->sc_odata = *ud;

	if (nkeys == 0)
		return;

	if (sc->sc_polling) {
		DPRINTFN(1,("ukbd_intr: pollchar = 0x%03x\n", ibuf[0]));
		memcpy(sc->sc_pollchars, ibuf, nkeys * sizeof(u_int16_t));
		sc->sc_npollchar = nkeys;
		return;
	}
#ifdef WSDISPLAY_COMPAT_RAWKBD
	if (sc->sc_rawkbd) {
		char cbuf[MAXKEYS * 2];
		int c;
		int npress;

		for (npress = i = j = 0; i < nkeys; i++) {
			key = ibuf[i];
			c = ukbd_trtab[key & CODEMASK];
			if (c == NN)
				continue;
			if (c & 0x80)
				cbuf[j++] = 0xe0;
			cbuf[j] = c & 0x7f;
			if (key & RELEASE)
				cbuf[j] |= 0x80;
			else {
				/* remember pressed keys for autorepeat */
				if (c & 0x80)
					sc->sc_rep[npress++] = 0xe0;
				sc->sc_rep[npress++] = c & 0x7f;
			}
			DPRINTFN(1,("ukbd_intr: raw = %s0x%02x\n", 
				    c & 0x80 ? "0xe0 " : "",
				    cbuf[j]));
			j++;
		}
		s = spltty();
		wskbd_rawinput(sc->sc_wskbddev, cbuf, j);
		splx(s);
#if defined(__OpenBSD__) 
		timeout_del(&sc->sc_rawrepeat_ch);
#else
		callout_stop(&sc->sc_rawrepeat_ch);
#endif
		if (npress != 0) {
			sc->sc_nrep = npress;
#if defined(__OpenBSD__)
			timeout_add(&sc->sc_rawrepeat_ch, hz * REP_DELAY1/1000);
#else
			callout_reset(&sc->sc_rawrepeat_ch,
			    hz * REP_DELAY1 / 1000, ukbd_rawrepeat, sc);
#endif
		}
		return;
	}
#endif

	s = spltty();
	for (i = 0; i < nkeys; i++) {
		key = ibuf[i];
		wskbd_input(sc->sc_wskbddev, 
		    key&RELEASE ? WSCONS_EVENT_KEY_UP : WSCONS_EVENT_KEY_DOWN,
		    key&CODEMASK);
	}
	splx(s);
}

void
ukbd_set_leds(void *v, int leds)
{
	struct ukbd_softc *sc = v;
	u_int8_t res;

	DPRINTF(("ukbd_set_leds: sc=%p leds=%d\n", sc, leds));

	if (sc->sc_dying)
		return;

	sc->sc_leds = leds;
	res = 0;
	if (leds & WSKBD_LED_SCROLL)
		res |= SCROLL_LOCK;
	if (leds & WSKBD_LED_NUM)
		res |= NUM_LOCK;
	if (leds & WSKBD_LED_CAPS)
		res |= CAPS_LOCK;
	res |= leds & 0xf8;
	usbd_set_report_async(sc->sc_iface, UHID_OUTPUT_REPORT, 0, &res, 1);
}

#ifdef WSDISPLAY_COMPAT_RAWKBD
void
ukbd_rawrepeat(void *v)
{
	struct ukbd_softc *sc = v;
	int s;

	s = spltty();
	wskbd_rawinput(sc->sc_wskbddev, sc->sc_rep, sc->sc_nrep);
	splx(s);
#if defined(__OpenBSD__)
	timeout_add(&sc->sc_rawrepeat_ch, hz * REP_DELAYN / 1000);
#else
	callout_reset(&sc->sc_rawrepeat_ch, hz * REP_DELAYN / 1000,
	    ukbd_rawrepeat, sc);
#endif
}
#endif

int
ukbd_ioctl(void *v, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct ukbd_softc *sc = v;

	switch (cmd) {
	case WSKBDIO_GTYPE:
		*(int *)data = WSKBD_TYPE_USB;
		return (0);
	case WSKBDIO_SETLEDS:
		ukbd_set_leds(v, *(int *)data);
		return (0);
	case WSKBDIO_GETLEDS:
		*(int *)data = sc->sc_leds;
		return (0);
#ifdef WSDISPLAY_COMPAT_RAWKBD
	case WSKBDIO_SETMODE:
		DPRINTF(("ukbd_ioctl: set raw = %d\n", *(int *)data));
		sc->sc_rawkbd = *(int *)data == WSKBD_RAW;
#if defined(__OpenBSD__)
		timeout_del(&sc->sc_rawrepeat_ch);
#else
		callout_stop(&sc->sc_rawrepeat_ch);
#endif
		return (0);
#endif
	}
	return (-1);
}

/* Console interface. */
void
ukbd_cngetc(void *v, u_int *type, int *data)
{
	struct ukbd_softc *sc = v;
	int s;
	int c;

	DPRINTFN(0,("ukbd_cngetc: enter\n"));
	s = splusb();
	sc->sc_polling = 1;
	while(sc->sc_npollchar <= 0)
		usbd_dopoll(sc->sc_iface);
	sc->sc_polling = 0;
	c = sc->sc_pollchars[0];
	sc->sc_npollchar--;
	memcpy(sc->sc_pollchars, sc->sc_pollchars+1, 
	       sc->sc_npollchar * sizeof(u_int16_t));
	*type = c & RELEASE ? WSCONS_EVENT_KEY_UP : WSCONS_EVENT_KEY_DOWN;
	*data = c & CODEMASK;
	splx(s);
	DPRINTFN(0,("ukbd_cngetc: return 0x%02x\n", c));
}

void
ukbd_cnpollc(void *v, int on)
{
	struct ukbd_softc *sc = v;
	usbd_device_handle dev;

	DPRINTFN(2,("ukbd_cnpollc: sc=%p on=%d\n", v, on));

	(void)usbd_interface2device_handle(sc->sc_iface,&dev);
	usbd_set_polling(dev, on);
}

int
ukbd_cnattach(void)
{

	/*
	 * XXX USB requires too many parts of the kernel to be running
	 * XXX in order to work, so we can't do much for the console
	 * XXX keyboard until autconfiguration has run its course.
	 */
	ukbd_is_console = 1;
	return (0);
}
