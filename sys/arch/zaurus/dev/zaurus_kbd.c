/* $OpenBSD: zaurus_kbd.c,v 1.24 2005/11/11 16:58:46 deraadt Exp $ */
/*
 * Copyright (c) 2005 Dale Rahn <drahn@openbsd.org>
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
#include <sys/timeout.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/signalvar.h>

#include <arm/xscale/pxa2x0reg.h>
#include <arm/xscale/pxa2x0_gpio.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wskbdvar.h>
#include <dev/wscons/wsksymdef.h>
#include <dev/wscons/wsksymvar.h>

#include <zaurus/dev/zaurus_kbdmap.h>

#include "apm.h"

const int
gpio_sense_pins_c3000[] = {
	12,
	17,
	91,
	34,
	36,
	38,
	39,
	-1
};

const int
gpio_strobe_pins_c3000[] = {
	88,
	23,
	24,
	25,
	26,
	27,
	52,
	103,
	107,
	-1,
	108,
	114
};

const int stuck_keys[] = {
	7,
	15,
	23,
	31
};


#define REP_DELAY1 400
#define REP_DELAYN 100

struct zkbd_softc {
	struct device sc_dev;

	const int *sc_sense_array;
	const int *sc_strobe_array;
	int sc_nsense;
	int sc_nstrobe;

	short sc_onkey_pin;
	short sc_sync_pin;
	short sc_swa_pin;
	short sc_swb_pin;
	char *sc_okeystate;
	char *sc_keystate;
	char sc_hinge;		/* 0=open, 1=nonsense, 2=backwards, 3=closed */
	char sc_maxkbdcol;

	struct timeout sc_roll_to;

	/* console stuff */
	int sc_polling;
	int sc_pollUD;
	int sc_pollkey;

	/* wskbd bits */
	struct device   *sc_wskbddev;
	int sc_rawkbd;
#ifdef WSDISPLAY_COMPAT_RAWKBD
	const char *sc_xt_keymap;
	struct timeout sc_rawrepeat_ch;
#define MAXKEYS 20
	char sc_rep[MAXKEYS];
	int sc_nrep;
#endif
	void *sc_powerhook;
};

struct zkbd_softc *zkbd_dev; /* XXX */

int zkbd_match(struct device *, void *, void *);
void zkbd_attach(struct device *, struct device *, void *);

int zkbd_irq(void *v);
void zkbd_poll(void *v);
int zkbd_on(void *v);
int zkbd_sync(void *v);
int zkbd_hinge(void *v);
void zkbd_power(int why, void *arg);

int zkbd_modstate;

struct cfattach zkbd_ca = {
	sizeof(struct zkbd_softc), zkbd_match, zkbd_attach
};

struct cfdriver zkbd_cd = {
	NULL, "zkbd", DV_DULL
};

int zkbd_enable(void *, int);
void zkbd_set_leds(void *, int);
int zkbd_ioctl(void *, u_long, caddr_t, int, struct proc *);
void zkbd_rawrepeat(void *v);

struct wskbd_accessops zkbd_accessops = {
	zkbd_enable,
	zkbd_set_leds,
	zkbd_ioctl,
};

void zkbd_cngetc(void *, u_int *, int *);
void zkbd_cnpollc(void *, int);
 
struct wskbd_consops zkbd_consops = {
        zkbd_cngetc,
        zkbd_cnpollc,
};              

struct wskbd_mapdata zkbd_keymapdata = {
        zkbd_keydesctab,   
        KB_US,
};



int
zkbd_match(struct device *parent, void *cf, void *aux)
{
	return 1;
}


void
zkbd_attach(struct device *parent, struct device *self, void *aux)
{
	struct zkbd_softc *sc = (struct zkbd_softc *)self;
	struct wskbddev_attach_args a;
	int pin, i;
	extern int glass_console;

	zkbd_dev = sc;
	sc->sc_polling = 0;
#ifdef WSDISPLAY_COMPAT_RAWKBD
	sc->sc_rawkbd = 0;
#endif
	/* Determine which system we are - XXX */

	sc->sc_powerhook = powerhook_establish(zkbd_power, sc);
	if (sc->sc_powerhook == NULL) {
		printf(": unable to establish powerhook\n");
		return;
	}

	if (1 /* C3000 */) {
		sc->sc_sense_array = gpio_sense_pins_c3000;
		sc->sc_strobe_array = gpio_strobe_pins_c3000;
		sc->sc_nsense = sizeof(gpio_sense_pins_c3000)/sizeof(int);
		sc->sc_nstrobe = sizeof(gpio_strobe_pins_c3000)/sizeof(int);
		sc->sc_maxkbdcol = 10;
		sc->sc_onkey_pin = 95;
		sc->sc_sync_pin = 16;
		sc->sc_swa_pin = 97;
		sc->sc_swb_pin = 96;
#ifdef WSDISPLAY_COMPAT_RAWKBD
		sc->sc_xt_keymap = xt_keymap;
#endif
	} /* XXX */

	sc->sc_okeystate = malloc(sc->sc_nsense * sc->sc_nstrobe,
	    M_DEVBUF, M_NOWAIT);
	bzero(sc->sc_okeystate, (sc->sc_nsense * sc->sc_nstrobe));

	sc->sc_keystate = malloc(sc->sc_nsense * sc->sc_nstrobe,
	    M_DEVBUF, M_NOWAIT);
	bzero(sc->sc_keystate, (sc->sc_nsense * sc->sc_nstrobe));

	/* set all the strobe bits */
	for (i = 0; i < sc->sc_nstrobe; i++) {
		pin = sc->sc_strobe_array[i];
		if (pin == -1) {
			continue;
		}
		pxa2x0_gpio_set_function(pin, GPIO_SET|GPIO_OUT);
	}
	/* set all the sense bits */
	for (i = 0; i < sc->sc_nsense; i++) {
		pin = sc->sc_sense_array[i];
		if (pin == -1) {
			continue;
		}
		pxa2x0_gpio_set_function(pin, GPIO_IN);
		pxa2x0_gpio_intr_establish(pin, IST_EDGE_BOTH, IPL_TTY,
		    zkbd_irq, sc, sc->sc_dev.dv_xname);
	}
	pxa2x0_gpio_intr_establish(sc->sc_onkey_pin, IST_EDGE_BOTH, IPL_TTY,
	    zkbd_on, sc, sc->sc_dev.dv_xname);
	pxa2x0_gpio_intr_establish(sc->sc_sync_pin, IST_EDGE_RISING, IPL_TTY,
	    zkbd_sync, sc, sc->sc_dev.dv_xname);
	pxa2x0_gpio_intr_establish(sc->sc_swa_pin, IST_EDGE_BOTH, IPL_TTY,
	    zkbd_hinge, sc, sc->sc_dev.dv_xname);
	pxa2x0_gpio_intr_establish(sc->sc_swb_pin, IST_EDGE_BOTH, IPL_TTY,
	    zkbd_hinge, sc, sc->sc_dev.dv_xname);

	if (glass_console) {
		wskbd_cnattach(&zkbd_consops, sc, &zkbd_keymapdata);
		a.console = 1;
	} else {
		a.console = 0;
	}

	a.keymap = &zkbd_keymapdata;
	a.accessops = &zkbd_accessops;
	a.accesscookie = sc;

	printf("\n");

	zkbd_hinge(sc);		/* to initialize sc_hinge */

	sc->sc_wskbddev = config_found(self, &a, wskbddevprint);

	timeout_set(&(sc->sc_roll_to), zkbd_poll, sc);
#ifdef WSDISPLAY_COMPAT_RAWKBD
	timeout_set(&sc->sc_rawrepeat_ch, zkbd_rawrepeat, sc);
#endif

}

#ifdef WSDISPLAY_COMPAT_RAWKBD
void
zkbd_rawrepeat(void *v)
{
	struct zkbd_softc *sc = v;
	int s;
		
	s = spltty();
	wskbd_rawinput(sc->sc_wskbddev, sc->sc_rep, sc->sc_nrep);
	splx(s);
	timeout_add(&sc->sc_rawrepeat_ch, hz * REP_DELAYN / 1000);
}
#endif

/* XXX only deal with keys that can be pressed when display is open? */
/* XXX are some not in the array? */
/* handle keypress interrupt */
int
zkbd_irq(void *v)
{
	zkbd_poll(v);

	return 1;
}

void
zkbd_poll(void *v)
{
	struct zkbd_softc *sc = v;
	int i, j, col, pin, type, keysdown = 0, s;
	int stuck;
	int keystate;
#ifdef WSDISPLAY_COMPAT_RAWKBD
	int npress = 0, ncbuf = 0, c;
	char cbuf[MAXKEYS *2];
#endif

	s = spltty();

	/* discharge all */
	for (i = 0; i < sc->sc_nstrobe; i++) {
		pin = sc->sc_strobe_array[i];
		if (pin != -1) {
			pxa2x0_gpio_clear_bit(pin);
			pxa2x0_gpio_set_dir(pin, GPIO_IN);
		}
	}

	delay (10);
	for(col = 0; col < sc->sc_nstrobe; col++) {
		if (sc->sc_strobe_array[i] == -1)
			continue;

		pin = sc->sc_strobe_array[col];

		/* activate_col */
		pxa2x0_gpio_set_bit(pin);
		pxa2x0_gpio_set_dir(pin, GPIO_OUT);

		/* wait activate delay */
		delay(10);

		/* read row */
		for (i = 0; i < sc->sc_nsense; i++) {
			int bit;

			if (sc->sc_sense_array[i] == -1) 
				continue;

			bit = pxa2x0_gpio_get_bit(sc->sc_sense_array[i]);
			if (bit && sc->sc_hinge && col < sc->sc_maxkbdcol)
				continue;
			sc->sc_keystate[i + (col * sc->sc_nsense)] = bit;
		}

		/* reset_col */
		pxa2x0_gpio_set_dir(pin, GPIO_IN);
		/* wait discharge delay */
		delay(10);
	}
	/* charge all */
	for (i = 0; i < sc->sc_nstrobe; i++) {
		pin = sc->sc_strobe_array[i];
		if (pin != -1) {
			pxa2x0_gpio_set_bit(pin);
			pxa2x0_gpio_set_dir(pin, GPIO_OUT);
		}
	}

	/* force the irqs to clear as we have just played with them. */
	for (i = 0; i < sc->sc_nsense; i++)
		if (sc->sc_sense_array[i] != -1)
			pxa2x0_gpio_clear_intr(sc->sc_sense_array[i]);

	/* process after resetting interrupt */

	zkbd_modstate = (
		(sc->sc_keystate[84] ? (1 << 0) : 0) | /* shift */
		(sc->sc_keystate[93] ? (1 << 1) : 0) | /* Fn */
		(sc->sc_keystate[14] ? (1 << 2) : 0)); /* 'alt' */

	for (i = 0; i < (sc->sc_nsense * sc->sc_nstrobe); i++) {
		stuck = 0;
		/* extend  xt_keymap to do this faster. */
		/* ignore 'stuck' keys' */
		for (j = 0; j < sizeof(stuck_keys)/sizeof(stuck_keys[0]); j++) {
			if (stuck_keys[j] == i) {
				stuck = 1 ;
				break;
			}
		}
		if (stuck)
			continue;
		keystate = sc->sc_keystate[i];

		keysdown |= keystate; /* if any keys held */

#ifdef WSDISPLAY_COMPAT_RAWKBD
		if (sc->sc_polling == 0 && sc->sc_rawkbd) {
			if ((keystate) || (sc->sc_okeystate[i] != keystate)) {
				c = sc->sc_xt_keymap[i];
				if (c & 0x80) {
					cbuf[ncbuf++] = 0xe0;
				}
				cbuf[ncbuf] = c & 0x7f;

				if (keystate) {
					if (c & 0x80) {
						sc->sc_rep[npress++] = 0xe0;
					}
					sc->sc_rep[npress++] = c & 0x7f;
				} else {
					cbuf[ncbuf] |= 0x80;
				}
				ncbuf++;
				sc->sc_okeystate[i] = keystate;
			}
		}
#endif

		if ((!sc->sc_rawkbd) && (sc->sc_okeystate[i] != keystate)) {

			type = keystate ? WSCONS_EVENT_KEY_DOWN :
			    WSCONS_EVENT_KEY_UP;

#if 0
			printf("key %d %s\n", i,
			    keystate ? "pressed" : "released");
#endif

			if (sc->sc_polling) {
				sc->sc_pollkey = i;
				sc->sc_pollUD = type;
			} else {
				wskbd_input(sc->sc_wskbddev, type, i);
			}

			sc->sc_okeystate[i] = keystate;
		}
	}

#ifdef WSDISPLAY_COMPAT_RAWKBD
	if (sc->sc_polling == 0 && sc->sc_rawkbd) {
		wskbd_rawinput(sc->sc_wskbddev, cbuf, ncbuf);
		sc->sc_nrep = npress;
		if (npress != 0)
			timeout_add(&sc->sc_rawrepeat_ch, hz * REP_DELAY1/1000);
		else 
			timeout_del(&sc->sc_rawrepeat_ch);
	}
#endif
	if (keysdown)
		timeout_add(&(sc->sc_roll_to), hz / 8); /* how long?*/
	else 
		timeout_del(&(sc->sc_roll_to)); /* always cancel? */

	splx(s);
}

#if NAPM > 0
extern	int kbd_reset;
extern	int apm_suspends;
static	int zkbdondown;				/* on key is pressed */
static	struct timeval zkbdontv = { 0, 0 };	/* last on key event */
const	struct timeval zkbdhalttv = { 3, 0 };	/*  3s for safe shutdown */
const	struct timeval zkbdsleeptv = { 0, 250000 };	/* .25s for suspend */
#endif

int
zkbd_on(void *v)
{
#if NAPM > 0
	struct zkbd_softc *sc = v;
	int down = pxa2x0_gpio_get_bit(sc->sc_onkey_pin) ? 1 : 0;

	/*
	 * Change run mode depending on how long the key is held down.
	 * Ignore the key if it gets pressed while the lid is closed.
	 *
	 * Keys can bounce and we have to work around missed interrupts.
	 * Only the second edge is detected upon exit from sleep mode.
	 */
	if (down) {
		if (sc->sc_hinge == 3) {
			zkbdondown = 0;
		} else {
			microuptime(&zkbdontv);
			zkbdondown = 1;
		}
	} else if (zkbdondown) {
		if (ratecheck(&zkbdontv, &zkbdhalttv)) {
			if (kbd_reset == 1) {
				kbd_reset = 0;
				psignal(initproc, SIGUSR1);
			}
		} else if (ratecheck(&zkbdontv, &zkbdsleeptv)) {
			apm_suspends++;
		}
		zkbdondown = 0;
	}
#endif
#if 0
	printf("on key pressed\n");
#endif
	return 1;
}

int
zkbd_sync(void *v)
{
#if 0
	printf("sync button pressed\n");
#endif
	return 1;
}

int
zkbd_hinge(void *v)
{
	struct zkbd_softc *sc = v;
	int a = pxa2x0_gpio_get_bit(sc->sc_swa_pin) ? 1 : 0;
	int b = pxa2x0_gpio_get_bit(sc->sc_swb_pin) ? 2 : 0;
	extern void lcd_blank(int);

#if 0
	printf("hinge event A %d B %d\n", a, b);
#endif
	sc->sc_hinge = a | b;

	if (sc->sc_hinge == 3)
		lcd_blank(1);
	else
		lcd_blank(0);

	return 1;
}

int
zkbd_enable(void *v, int on)
{
        return 0;
}
        
void
zkbd_set_leds(void *v, int on)
{
}

int
zkbd_ioctl(void *v, u_long cmd, caddr_t data, int flag, struct proc *p)
{
#ifdef WSDISPLAY_COMPAT_RAWKBD
	struct zkbd_softc *sc = v;
#endif

	switch (cmd) {

	case WSKBDIO_GTYPE:
		*(int *)data = WSKBD_TYPE_ZAURUS;
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
 
	}
	/* kbdioctl(...); */

	return -1;
}

/* implement polling for zaurus_kbd */
void
zkbd_cngetc(void *v, u_int *type, int *data)
{               
	struct zkbd_softc *sc = zkbd_dev;
	sc->sc_pollkey = -1;
	sc->sc_pollUD = -1;
	sc->sc_polling = 1;
	while (sc->sc_pollkey == -1) {
		zkbd_poll(zkbd_dev);
		DELAY(10000);	/* XXX */
	}
	sc->sc_polling = 0;
	*data = sc->sc_pollkey;
	*type = sc->sc_pollUD;
}

void
zkbd_cnpollc(void *v, int on)
{
}

void
zkbd_power(int why, void *arg)
{
	struct zkbd_softc *sc = arg;
	int a = pxa2x0_gpio_get_bit(sc->sc_swa_pin) ? 1 : 0;
	int b = pxa2x0_gpio_get_bit(sc->sc_swb_pin) ? 2 : 0;

	/* probably should check why */
	sc->sc_hinge = a | b;
}
