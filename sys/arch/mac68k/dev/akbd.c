/*	$OpenBSD: akbd.c,v 1.6 2006/01/13 19:36:43 miod Exp $	*/
/*	$NetBSD: akbd.c,v 1.17 2005/01/15 16:00:59 chs Exp $	*/

/*
 * Copyright (C) 1998	Colin Wood
 * All rights reserved.
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
 *	This product includes software developed by Colin Wood.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/timeout.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/fcntl.h>
#include <sys/poll.h>
#include <sys/selinfo.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/systm.h>

#include "wskbd.h"

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wskbdvar.h>
#include <dev/wscons/wsksymdef.h>
#include <dev/wscons/wsksymvar.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/viareg.h>

#include <mac68k/dev/adbvar.h>
#include <mac68k/dev/akbdmap.h>
#include <mac68k/dev/akbdvar.h>

#define KEYBOARD_ARRAY
#include <machine/keyboard.h>

/*
 * Function declarations.
 */
int	akbdmatch(struct device *, void *, void *);
void	akbdattach(struct device *, struct device *, void *);
void	kbd_adbcomplete(caddr_t, caddr_t, int);
void	kbd_processevent(adb_event_t *, struct akbd_softc *);
int	akbd_is_console(void);

/* Driver definition. */
struct cfattach akbd_ca = {
	sizeof(struct akbd_softc), akbdmatch, akbdattach
};
struct cfdriver akbd_cd = {
	NULL, "akbd", DV_DULL
};

int akbd_enable(void *, int);
void akbd_set_leds(void *, int);
int akbd_ioctl(void *, u_long, caddr_t, int, struct proc *);
int akbd_intr(adb_event_t *, struct akbd_softc *);
void akbd_rawrepeat(void *v);


struct wskbd_accessops akbd_accessops = {
	akbd_enable,
	akbd_set_leds,
	akbd_ioctl,
};

struct wskbd_mapdata akbd_keymapdata = {
	akbd_keydesctab,
#ifdef AKBD_LAYOUT
	AKBD_LAYOUT,
#else
	KB_US,
#endif
};

int
akbdmatch(struct device *parent, void *vcf, void *aux)
{
	struct adb_attach_args *aa_args = (struct adb_attach_args *)aux;

	if (aa_args->origaddr == ADBADDR_KBD)
		return 1;
	else
		return 0;
}

void
akbdattach(struct device *parent, struct device *self, void *aux)
{
	ADBSetInfoBlock adbinfo;
	struct akbd_softc *sc = (struct akbd_softc *)self;
	struct adb_attach_args *aa_args = (struct adb_attach_args *)aux;
	int error, kbd_done;
	short cmd;
	u_char buffer[9];
#if NWSKBD > 0
	struct wskbddev_attach_args a;
	static int akbd_console_initted;
	int wskbd_eligible;

	wskbd_eligible = 1;
#endif

	sc->origaddr = aa_args->origaddr;
	sc->adbaddr = aa_args->adbaddr;
	sc->handler_id = aa_args->handler_id;

	sc->sc_leds = (u_int8_t)0x00;	/* initially off */

	adbinfo.siServiceRtPtr = (Ptr)kbd_adbcomplete;
	adbinfo.siDataAreaAddr = (caddr_t)sc;

	printf(": ");
	switch (sc->handler_id) {
	case ADB_STDKBD:
		printf("standard keyboard\n");
		break;
	case ADB_ISOKBD:
		printf("standard keyboard (ISO layout)\n");
		break;
	case ADB_EXTKBD:
		cmd = ADBTALK(sc->adbaddr, 1);
		kbd_done =
		    (adb_op_sync((Ptr)buffer, (Ptr)0, (Ptr)0, cmd) == 0);

		/* Ignore Logitech MouseMan/Trackman pseudo keyboard */
		if (kbd_done && buffer[1] == 0x9a && buffer[2] == 0x20) {
			printf("Mouseman (non-EMP) pseudo keyboard\n");
			adbinfo.siServiceRtPtr = (Ptr)0;
			adbinfo.siDataAreaAddr = (Ptr)0;
#if NWSKBD > 0
			wskbd_eligible = 0;
#endif /* NWSKBD > 0 */
		} else if (kbd_done && buffer[1] == 0x9a && buffer[2] == 0x21) {
			printf("Trackman (non-EMP) pseudo keyboard\n");
			adbinfo.siServiceRtPtr = (Ptr)0;
			adbinfo.siDataAreaAddr = (Ptr)0;
#if NWSKBD > 0
			wskbd_eligible = 0;
#endif /* NWSKBD > 0 */
		} else {
			printf("extended keyboard\n");
#ifdef notyet
			blinkleds(sc);
#endif
		}
		break;
	case ADB_EXTISOKBD:
		printf("extended keyboard (ISO layout)\n");
#ifdef notyet
		blinkleds(sc);
#endif
		break;
	case ADB_KBDII:
		printf("keyboard II\n");
		break;
	case ADB_ISOKBDII:
		printf("keyboard II (ISO layout)\n");
		break;
	case ADB_PBKBD:
		printf("PowerBook keyboard\n");
		break;
	case ADB_PBISOKBD:
		printf("PowerBook keyboard (ISO layout)\n");
		break;
	case ADB_ADJKPD:
		printf("adjustable keypad\n");
#if NWSKBD > 0
		wskbd_eligible = 0;
#endif /* NWSKBD > 0 */
		break;
	case ADB_ADJKBD:
		printf("adjustable keyboard\n");
		break;
	case ADB_ADJISOKBD:
		printf("adjustable keyboard (ISO layout)\n");
		break;
	case ADB_ADJJAPKBD:
		printf("adjustable keyboard (Japanese layout)\n");
		break;
	case ADB_PBEXTISOKBD:
		printf("PowerBook extended keyboard (ISO layout)\n");
		break;
	case ADB_PBEXTJAPKBD:
		printf("PowerBook extended keyboard (Japanese layout)\n");
		break;
	case ADB_JPKBDII:
		printf("keyboard II (Japanese layout)\n");
		break;
	case ADB_PBEXTKBD:
		printf("PowerBook extended keyboard\n");
		break;
	case ADB_DESIGNKBD:
		printf("extended keyboard\n");
#ifdef notyet
		blinkleds(sc);
#endif
		break;
	case ADB_PBJPKBD:
		printf("PowerBook keyboard (Japanese layout)\n");
		break;
	case ADB_PBG3JPKBD:
		printf("PowerBook G3 keyboard (Japanese layout)\n");
		break;
	case ADB_PBG4KBD:
		printf("PowerBook G4 keyboard (Inverted T)\n");
		break;
	case ADB_IBITISOKBD:
		printf("iBook keyboard with inverted T (ISO layout)\n");
		break;
	default:
		printf("mapped device (%d)\n", sc->handler_id);
#if NWSKBD > 0
		wskbd_eligible = 0;
#endif /* NWSKBD > 0 */
		break;
	}
	error = SetADBInfo(&adbinfo, sc->adbaddr);
#ifdef ADB_DEBUG
	if (adb_debug)
		printf("akbd: returned %d from SetADBInfo\n", error);
#endif

#ifdef WSDISPLAY_COMPAT_RAWKBD
	timeout_set(&sc->sc_rawrepeat_ch, akbd_rawrepeat, sc);
#endif

#if NWSKBD > 0
	if (akbd_is_console() && wskbd_eligible)
		a.console = (++akbd_console_initted == 1);
	else
		a.console = 0;
	a.keymap = &akbd_keymapdata;
	a.accessops = &akbd_accessops;
	a.accesscookie = sc;

	sc->sc_wskbddev = config_found(self, &a, wskbddevprint);
#endif
}


/*
 * Handle putting the keyboard data received from the ADB into
 * an ADB event record.
 */
void
kbd_adbcomplete(caddr_t buffer, caddr_t data_area, int adb_command)
{
	adb_event_t event;
	struct akbd_softc *ksc;
	int adbaddr;
#ifdef ADB_DEBUG
	int i;

	if (adb_debug)
		printf("adb: transaction completion\n");
#endif

	adbaddr = ADB_CMDADDR(adb_command);
	ksc = (struct akbd_softc *)data_area;

	event.addr = adbaddr;
	event.hand_id = ksc->handler_id;
	event.def_addr = ksc->origaddr;
	event.byte_count = buffer[0];
	memcpy(event.bytes, buffer + 1, event.byte_count);

#ifdef ADB_DEBUG
	if (adb_debug) {
		printf("akbd: from %d at %d (org %d) %d:", event.addr,
		    event.hand_id, event.def_addr, buffer[0]);
		for (i = 1; i <= buffer[0]; i++)
			printf(" %x", buffer[i]);
		printf("\n");
	}
#endif

	microtime(&event.timestamp);

	kbd_processevent(&event, ksc);
}

/*
 * Given a keyboard ADB event, record the keycodes and call the key
 * repeat handler, optionally passing the event through the mouse
 * button emulation handler first.
 */
void
kbd_processevent(adb_event_t *event, struct akbd_softc *ksc)
{
        adb_event_t new_event;

        new_event = *event;
	new_event.u.k.key = event->bytes[0];
	new_event.bytes[1] = 0xff;
#if NWSKBD > 0
	if (ksc->sc_wskbddev != NULL) /* wskbd is attached? */
		akbd_intr(&new_event, ksc);
#endif
	if (event->bytes[1] != 0xff) {
		new_event.u.k.key = event->bytes[1];
		new_event.bytes[0] = event->bytes[1];
		new_event.bytes[1] = 0xff;
#if NWSKBD > 0
		if (ksc->sc_wskbddev != NULL) /* wskbd is attached? */
			akbd_intr(&new_event, ksc);
#endif
	}

}

int
akbd_enable(void *v, int on)
{
	return 0;
}

void
akbd_set_leds(void *v, int on)
{
}

int
akbd_ioctl(void *v, u_long cmd, caddr_t data, int flag, struct proc *p)
{
#ifdef WSDISPLAY_COMPAT_RAWKBD
	struct akbd_softc *sc = v;
#endif

	switch (cmd) {

	case WSKBDIO_GTYPE:
		*(int *)data = WSKBD_TYPE_ADB;
		return 0;
	case WSKBDIO_SETLEDS:
		return 0;
	case WSKBDIO_GETLEDS:
		*(int *)data = 0;
		return 0;
#ifdef WSDISPLAY_COMPAT_RAWKBD
	case WSKBDIO_SETMODE:
		sc->sc_rawkbd = *(int *)data == WSKBD_RAW;
		timeout_del(&sc->sc_rawrepeat_ch);
		return (0);
#endif
	case WSKBDIO_BELL:
	case WSKBDIO_COMPLEXBELL:
#define d ((struct wskbd_bell_data *)data)
		mac68k_ring_bell(d->pitch, d->period * hz / 1000, d->volume);
#undef d
		return (0);

	default:
		return (-1);
	}
}

#ifdef WSDISPLAY_COMPAT_RAWKBD
void
akbd_rawrepeat(void *v)
{
	struct akbd_softc *sc = v;
	int s;

	s = spltty();
	wskbd_rawinput(sc->sc_wskbddev, sc->sc_rep, sc->sc_nrep);
	splx(s);
	timeout_add(&sc->sc_rawrepeat_ch, hz * REP_DELAYN / 1000);
}
#endif

int adb_polledkey;
int
akbd_intr(adb_event_t *event, struct akbd_softc *sc)
{
	int key, press, val;
	int type;
	static int shift;

	key = event->u.k.key;

	/*
	 * Caps lock is weird. The key sequence generated is:
	 * press:   down(57) [57]  (LED turns on)
	 * release: up(127)  [255]
	 * press:   up(127)  [255]
	 * release: up(57)   [185] (LED turns off)
	 */
	if (ADBK_KEYVAL(key) == ADBK_CAPSLOCK)
		shift = 0;

	if (key == 255) {
		if (shift == 0) {
			key = ADBK_KEYUP(ADBK_CAPSLOCK);
			shift = 1;
		} else {
			key = ADBK_KEYDOWN(ADBK_CAPSLOCK);
			shift = 0;
		}
	}

	press = ADBK_PRESS(key);
	val = ADBK_KEYVAL(key);

	type = press ? WSCONS_EVENT_KEY_DOWN : WSCONS_EVENT_KEY_UP;

	if (adb_polling) {
		adb_polledkey = key;
#ifdef WSDISPLAY_COMPAT_RAWKBD
	} else if (sc->sc_rawkbd) {
		char cbuf[MAXKEYS *2];
		int c, j, s;
		int npress;

		j = npress = 0;

		c = keyboard[val][3];
		if (c == 0) {
			return 0; /* XXX */
		}
		if (c & 0x80)
			cbuf[j++] = 0xe0;
		cbuf[j] = c & 0x7f;
		if (type == WSCONS_EVENT_KEY_UP) {
			cbuf[j] |= 0x80;
		} else {
			/* this only records last key pressed */
			if (c & 0x80)
				sc->sc_rep[npress++] = 0xe0;
			sc->sc_rep[npress++] = c & 0x7f;
		}
		j++;
		s = spltty();
		wskbd_rawinput(sc->sc_wskbddev, cbuf, j);
		splx(s);
		timeout_del(&sc->sc_rawrepeat_ch);
		sc->sc_nrep = npress;
		if (npress != 0)
			timeout_add(&sc->sc_rawrepeat_ch, hz * REP_DELAY1/1000);
		return 0;
#endif
	} else {
		wskbd_input(sc->sc_wskbddev, type, val);
	}

	return 0;
}

int
akbd_is_console(void)
{
	extern struct mac68k_machine_S mac68k_machine;

	return ((mac68k_machine.serial_console & 0x03) == 0);
}

void akbd_cnbell(void *, u_int, u_int, u_int);
void akbd_cngetc(void *, u_int *, int *);
void akbd_cnpollc(void *, int);

struct wskbd_consops akbd_consops = {
	akbd_cngetc,
	akbd_cnpollc,
	akbd_cnbell
};

int
akbd_cnattach(void)
{
	extern struct wskbd_mapdata akbd_keymapdata;

	wskbd_cnattach(&akbd_consops, NULL, &akbd_keymapdata);

	return 0;
}

void
akbd_cngetc(void *v, u_int *type, int *data)
{
	int intbits, key, press, val;
	int s;
	extern int adb_polledkey;
	extern int adb_intr(void *);
	extern void pm_intr(void *);

	s = splhigh();

	adb_polledkey = -1;
	adb_polling = 1;

	while (adb_polledkey == -1) {
		intbits = via_reg(VIA1, vIFR);

		if (intbits & V1IF_ADBRDY) {
			adb_intr(NULL);
			via_reg(VIA1, vIFR) = V1IF_ADBRDY;
		}
		if (intbits & 0x10) {
			pm_intr(NULL);
			via_reg(VIA1, vIFR) = 0x10;
		}
	}

	adb_polling = 0;
	splx(s);

	key = adb_polledkey;
	press = ADBK_PRESS(key);
	val = ADBK_KEYVAL(key);

	*data = val;
	*type = press ? WSCONS_EVENT_KEY_DOWN : WSCONS_EVENT_KEY_UP;
}

void
akbd_cnpollc(void *v, int on)
{
}

void
akbd_cnbell(void *v, u_int pitch, u_int period, u_int volume)
{
	mac68k_ring_bell(pitch, period * hz / 1000, volume);
}
