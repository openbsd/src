/*	$OpenBSD: akbd.c,v 1.8 2010/05/23 15:09:38 deraadt Exp $	*/
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
#include <sys/systm.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wskbdvar.h>
#include <dev/wscons/wsksymdef.h>
#include <dev/wscons/wsksymvar.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>

#include <dev/adb/adb.h>
#include <dev/adb/akbdmap.h>
#include <dev/adb/akbdvar.h>

#ifdef WSDISPLAY_COMPAT_RAWKBD
#define KEYBOARD_ARRAY
#endif
#include <dev/adb/keyboard.h>

/*
 * Function declarations.
 */
int	akbdmatch(struct device *, void *, void *);
void	akbdattach(struct device *, struct device *, void *);

/* Driver definition. */
struct cfattach akbd_ca = {
	sizeof(struct akbd_softc), akbdmatch, akbdattach
};
struct cfdriver akbd_cd = {
	NULL, "akbd", DV_DULL
};

int	akbd_enable(void *, int);
void	akbd_set_leds(void *, int);
int	akbd_ioctl(void *, u_long, caddr_t, int, struct proc *);


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

void	akbd_adbcomplete(caddr_t, caddr_t, int);
void	akbd_capslockwrapper(struct akbd_softc *, int);
void	akbd_input(struct akbd_softc *, int);
void	akbd_processevent(struct akbd_softc *, adb_event_t *);
void	akbd_rawrepeat(void *v);
#ifdef notyet
u_char	getleds(int);
int	setleds(struct akbd_softc *, u_char);
void	blinkleds(struct akbd_softc *);
#endif

int
akbdmatch(struct device *parent, void *vcf, void *aux)
{
	struct adb_attach_args *aa_args = (struct adb_attach_args *)aux;

	if (aa_args->origaddr == ADBADDR_KBD)
		return (1);
	else
		return (0);
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
	struct wskbddev_attach_args a;
	static int akbd_console_initted;
	int wskbd_eligible = 1;

	sc->origaddr = aa_args->origaddr;
	sc->adbaddr = aa_args->adbaddr;
	sc->handler_id = aa_args->handler_id;

	sc->sc_leds = (u_int8_t)0x00;	/* initially off */
	sc->sc_caps = 0;

	adbinfo.siServiceRtPtr = (Ptr)akbd_adbcomplete;
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
		    (adb_op_sync((Ptr)buffer, cmd) == 0);

		/* Ignore Logitech MouseMan/Trackman pseudo keyboard */
		if (kbd_done && buffer[1] == 0x9a && buffer[2] == 0x20) {
			printf("Mouseman (non-EMP) pseudo keyboard\n");
			adbinfo.siServiceRtPtr = (Ptr)0;
			adbinfo.siDataAreaAddr = (Ptr)0;
			wskbd_eligible = 0;
		} else if (kbd_done && buffer[1] == 0x9a && buffer[2] == 0x21) {
			printf("Trackman (non-EMP) pseudo keyboard\n");
			adbinfo.siServiceRtPtr = (Ptr)0;
			adbinfo.siDataAreaAddr = (Ptr)0;
			wskbd_eligible = 0;
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
		wskbd_eligible = 0;
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
#if 0
		wskbd_eligible = 0;
#endif
		break;
	}
	error = set_adb_info(&adbinfo, sc->adbaddr);
#ifdef ADB_DEBUG
	if (adb_debug)
		printf("akbd: returned %d from set_adb_info\n", error);
#endif

#ifdef WSDISPLAY_COMPAT_RAWKBD
	timeout_set(&sc->sc_rawrepeat_ch, akbd_rawrepeat, sc);
#endif

	if (akbd_is_console() && wskbd_eligible)
		a.console = (++akbd_console_initted == 1);
	else
		a.console = 0;
	a.keymap = &akbd_keymapdata;
	a.accessops = &akbd_accessops;
	a.accesscookie = sc;

	sc->sc_wskbddev = config_found(self, &a, wskbddevprint);
}


/*
 * Handle putting the keyboard data received from the ADB into
 * an ADB event record.
 */
void
akbd_adbcomplete(caddr_t buffer, caddr_t data_area, int adb_command)
{
	adb_event_t event;
	struct akbd_softc *sc;
	int adbaddr;
#ifdef ADB_DEBUG
	int i;

	if (adb_debug)
		printf("adb: transaction completion\n");
#endif

	adbaddr = ADB_CMDADDR(adb_command);
	sc = (struct akbd_softc *)data_area;

	event.byte_count = buffer[0];
	memcpy(event.bytes, buffer + 1, event.byte_count);

#ifdef ADB_DEBUG
	if (adb_debug) {
		printf("akbd: from %d at %d (org %d) %d:", adbaddr,
		    sc->handler_id, sc->origaddr, buffer[0]);
		for (i = 1; i <= buffer[0]; i++)
			printf(" %x", buffer[i]);
		printf("\n");
	}
#endif

	if (sc->sc_wskbddev != NULL)
		akbd_processevent(sc, &event);
}

#ifdef notyet
/*
 * Get the actual hardware LED state and convert it to softc format.
 */
u_char
getleds(int addr)
{
	short cmd;
	u_char buffer[9], leds;

	leds = 0x00;	/* all off */
	buffer[0] = 0;

	cmd = ADBTALK(addr, 2);
	if (adb_op_sync((Ptr)buffer, cmd) == 0 &&
	    buffer[0] > 0)
		leds = ~(buffer[2]) & 0x07;

	return (leds);
}

/*
 * Set the keyboard LED's.
 *
 * Automatically translates from ioctl/softc format to the
 * actual keyboard register format
 */
int
setleds(struct akbd_softc *sc, u_char leds)
{
	int addr;
	short cmd;
	u_char buffer[9];

	addr = sc->adbaddr;
	buffer[0] = 0;

	cmd = ADBTALK(addr, 2);
	if (adb_op_sync((Ptr)buffer, cmd) || buffer[0] == 0)
		return (EIO);

	leds = ~leds & 0x07;
	buffer[2] &= 0xf8;
	buffer[2] |= leds;

	cmd = ADBLISTEN(addr, 2);
	adb_op_sync((Ptr)buffer, cmd);

	/* talk R2 */
	cmd = ADBTALK(addr, 2);
	if (adb_op_sync((Ptr)buffer, cmd) || buffer[0] == 0)
		return (EIO);

	if ((buffer[2] & 0xf8) != leds)
		return (EIO);
	else
		return (0);
}

/*
 * Toggle all of the LED's on and off, just for show.
 */
void
blinkleds(struct akbd_softc *sc)
{
	u_char origleds;

	origleds = getleds(sc->adbaddr);
	setleds(sc, LED_NUMLOCK | LED_CAPSLOCK | LED_SCROLL_LOCK);
	delay(400000);
	setleds(sc, origleds);

	if (origleds & LED_NUMLOCK)
		sc->sc_leds |= WSKBD_LED_NUM;
	if (origleds & LED_CAPSLOCK)
		sc->sc_leds |= WSKBD_LED_CAPS;
	if (origleds & LED_SCROLL_LOCK)
		sc->sc_leds |= WSKBD_LED_SCROLL;
}
#endif

int
akbd_enable(void *v, int on)
{
	return 0;
}

void
akbd_set_leds(void *v, int on)
{
#ifdef notyet
	struct akbd_softc *sc = v;
	int leds;

	if (sc->sc_extended) {
		if (sc->sc_leds == on)
			return;

		leds = 0;
		if (on & WSKBD_LED_NUM)
			leds |= LED_NUMLOCK;
		if (on & WSKBD_LED_CAPS)
			leds |= LED_CAPSLOCK;
		if (on & WSKBD_LED_SCROLL)
			leds |= LED_SCROLL_LOCK;

		setleds(sc, leds);
	}
#endif
}

int
akbd_ioctl(void *v, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct akbd_softc *sc = v;

	switch (cmd) {

	case WSKBDIO_GTYPE:
		*(int *)data = WSKBD_TYPE_ADB;
		return 0;
	case WSKBDIO_SETLEDS:
		akbd_set_leds(v, *(int *)data);
		return 0;
	case WSKBDIO_GETLEDS:
		*(int *)data = sc->sc_leds;
		return 0;
#ifdef WSDISPLAY_COMPAT_RAWKBD
	case WSKBDIO_SETMODE:
		sc->sc_rawkbd = *(int *)data == WSKBD_RAW;
		timeout_del(&sc->sc_rawrepeat_ch);
		return (0);
#endif

#if defined(__mac68k__)	/* XXX not worth creating akbd_machdep_ioctl() */
	case WSKBDIO_BELL:
	case WSKBDIO_COMPLEXBELL:
#define d ((struct wskbd_bell_data *)data)
		mac68k_ring_bell(d->pitch, d->period * hz / 1000, d->volume);
#undef d
		return (0);
#endif

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
	timeout_add_msec(&sc->sc_rawrepeat_ch, REP_DELAYN);
}
#endif

/*
 * The ``caps lock'' key is special: since on earlier keyboards, the physical
 * key stays down when pressed, we will get a notification of the key press,
 * but not of the key release. Then, when it is pressed again, we will not get
 * a notification of the key press, but will see the key release.
 *
 * This is not exactly true. We see the missing release and press events both
 * as the release of the power (reset) key.
 *
 * To avoid confusing them with real power key presses, we maintain two
 * states for the caps lock key: logically down (from wscons' point of view),
 * and ``physically'' down (from the adb messages point of view), to ignore
 * the power key. But since one may press the power key while the caps lock
 * is held down, we also have to remember the state of the power key... this
 * is quite messy.
 */

/*
 * Values for caps lock state machine
 */
#define	CL_DOWN_ADB	0x01
#define	CL_DOWN_LOGICAL	0x02
#define	CL_DOWN_RESET	0x04

/*
 * Given a keyboard ADB event, decode the keycodes and pass them to wskbd.
 */
void
akbd_processevent(struct akbd_softc *sc, adb_event_t *event)
{
	switch (event->byte_count) {
	case 1:
		akbd_capslockwrapper(sc, event->bytes[0]);
		break;
	case 2:
		/*
		 * The reset (or power) key sends 0x7f7f on press and
		 * 0xffff on release, and we ignore it.
		 */
		if (event->bytes[0] == event->bytes[1] &&
		    ADBK_KEYVAL(event->bytes[0]) == ADBK_RESET) {
			if (event->bytes[0] == ADBK_KEYDOWN(ADBK_RESET))
				SET(sc->sc_caps, CL_DOWN_RESET);
			else {
				if (ISSET(sc->sc_caps, CL_DOWN_RESET))
					CLR(sc->sc_caps, CL_DOWN_RESET);
				else if (ISSET(sc->sc_caps, CL_DOWN_ADB)) {
					akbd_input(sc, ISSET(sc->sc_caps,
					    CL_DOWN_LOGICAL) ?
					      ADBK_KEYDOWN(ADBK_CAPSLOCK) :
					      ADBK_KEYUP(ADBK_CAPSLOCK));
					sc->sc_caps ^= CL_DOWN_LOGICAL;
				}
			}
		} else {
			akbd_capslockwrapper(sc, event->bytes[0]);
			akbd_capslockwrapper(sc, event->bytes[1]);
		}
		break;
	default:
#ifdef DIAGNOSTIC
		printf("%s: unexpected message length %d\n",
		    sc->sc_dev.dv_xname, event->byte_count);
#endif
		break;
	}

}

void
akbd_capslockwrapper(struct akbd_softc *sc, int key)
{
	if (ADBK_KEYVAL(key) == ADBK_CAPSLOCK)
		sc->sc_caps ^= CL_DOWN_ADB;

	if (key != 0xff)
		akbd_input(sc, key);
}

int adb_polledkey;
void
akbd_input(struct akbd_softc *sc, int key)
{
	int press, val;
	int type;

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

		c = keyboard[val];
		if (c == 0) {
			return; /* XXX */
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
			timeout_add_msec(&sc->sc_rawrepeat_ch, REP_DELAY1);
#endif
	} else {
		wskbd_input(sc->sc_wskbddev, type, val);
	}
}
