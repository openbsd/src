/*	$OpenBSD: comkbd_ebus.c,v 1.1 2002/01/24 15:54:37 jason Exp $	*/

/*
 * Copyright (c) 2002 Jason L. Wright (jason@thought.net)
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
 *	This product includes software developed by Jason L. Wright
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/tty.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/syslog.h>

#include <machine/bus.h>
#include <machine/autoconf.h>
#include <machine/openfirm.h>

#include <sparc64/dev/ebusreg.h>
#include <sparc64/dev/ebusvar.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wskbdvar.h>
#include <dev/wscons/wsksymdef.h>
#include <dev/wscons/wsksymvar.h>

#include <dev/ic/comreg.h>
#include <dev/ic/comvar.h>
#include <dev/ic/ns16550reg.h>

#include <dev/cons.h>

/* keyboard commands (host->kbd) */
#define	SKBD_CMD_RESET		0x01
#define	SKBD_CMD_BELLON		0x02
#define	SKBD_CMD_BELLOFF	0x03
#define	SKBD_CMD_CLICKON	0x0a
#define	SKBD_CMD_CLICKOFF	0x0b
#define	SKBD_CMD_SETLED		0x0e
#define	SKBD_CMD_LAYOUT		0x0f

/* keyboard responses (kbd->host) */
#define	SKBD_RSP_RESET_OK	0x04	/* normal reset status */
#define	SKBD_RSP_IDLE		0x7f	/* no keys down */
#define	SKBD_RSP_LAYOUT		0xfe	/* layout follows */
#define	SKBD_RSP_RESET		0xff	/* reset status follows */

#define	SKBD_LED_NUMLOCK	0x01
#define	SKBD_LED_COMPOSE	0x02
#define	SKBD_LED_SCROLLLOCK	0x04
#define	SKBD_LED_CAPSLOCK	0x08

#define	SKBD_STATE_RESET	0
#define	SKBD_STATE_LAYOUT	1
#define	SKBD_STATE_GETKEY	2

#define	KC(n)	KS_KEYCODE(n)
const keysym_t comkbd_keydesc_us[] = {
    KC(0x01), KS_Cmd,
    KC(0x02), KS_Cmd_BrightnessDown,
    KC(0x04), KS_Cmd_BrightnessUp,
    KC(0x05),				KS_f1,
    KC(0x06),				KS_f2,
    KC(0x07),				KS_f10,
    KC(0x08),				KS_f3,
    KC(0x09),				KS_f11,
    KC(0x0a),				KS_f4,
    KC(0x0b),				KS_f12,
    KC(0x0c),				KS_f5,
    KC(0x0d),				KS_Alt_R,
    KC(0x0e),				KS_f6,
    KC(0x10),				KS_f7,
    KC(0x11),				KS_f8,
    KC(0x12),				KS_f9,
    KC(0x13),				KS_Alt_L,
    KC(0x14),				KS_Up,
    KC(0x15),				KS_Pause,
    KC(0x16),				KS_Print_Screen,
    KC(0x18),				KS_Left,
    KC(0x19),				KS_Hold_Screen,
    KC(0x1b),				KS_Down,
    KC(0x1c),				KS_Right,
    KC(0x1d),				KS_Escape,
    KC(0x1e),				KS_1,		KS_exclam,
    KC(0x1f),				KS_2,		KS_at,
    KC(0x20),				KS_3,		KS_numbersign,
    KC(0x21),				KS_4,		KS_dollar,
    KC(0x22),				KS_5,		KS_percent,
    KC(0x23),				KS_6,		KS_asciicircum,
    KC(0x24),				KS_7,		KS_ampersand,
    KC(0x25),				KS_8,		KS_asterisk,
    KC(0x26),				KS_9,		KS_parenleft,
    KC(0x27),				KS_0,		KS_parenright,
    KC(0x28),				KS_minus,	KS_underscore,
    KC(0x29),				KS_equal,	KS_plus,
    KC(0x2a),				KS_grave,	KS_asciitilde,
    KC(0x2b),				KS_BackSpace,
    KC(0x2c),				KS_Insert,
    KC(0x2d),				KS_KP_Equal,
    KC(0x2e),				KS_KP_Divide,
    KC(0x2f),				KS_KP_Multiply,
    KC(0x32),				KS_KP_Delete,
    KC(0x34),				KS_Home,
    KC(0x35),				KS_Tab,
    KC(0x36),				KS_q,
    KC(0x37),				KS_w,
    KC(0x38),				KS_e,
    KC(0x39),				KS_r,
    KC(0x3a),				KS_t,
    KC(0x3b),				KS_y,
    KC(0x3c),				KS_u,
    KC(0x3d),				KS_i,
    KC(0x3e),				KS_o,
    KC(0x3f),				KS_p,
    KC(0x40),				KS_bracketleft,	KS_braceleft,
    KC(0x41),				KS_bracketright,KS_braceright,
    KC(0x42),				KS_Delete,
    KC(0x43),				KS_Multi_key,
    KC(0x44),				KS_KP_Home,	KS_KP_7,
    KC(0x45),				KS_KP_Up,	KS_KP_8,
    KC(0x46),				KS_KP_Prior,	KS_KP_9,
    KC(0x47),				KS_KP_Subtract,
    KC(0x4a),				KS_End,
    KC(0x4c),				KS_Control_L,
    KC(0x4d), KS_Cmd_Debugger,		KS_a,
    KC(0x4e),				KS_s,
    KC(0x4f),				KS_d,
    KC(0x50),				KS_f,
    KC(0x51),				KS_g,
    KC(0x52),				KS_h,
    KC(0x53),				KS_j,
    KC(0x54),				KS_k,
    KC(0x55),				KS_l,
    KC(0x56),				KS_semicolon,	KS_colon,
    KC(0x57),				KS_apostrophe,	KS_quotedbl,
    KC(0x58),				KS_backslash,	KS_bar,
    KC(0x59),				KS_Return,
    KC(0x5a),				KS_KP_Enter,
    KC(0x5b),				KS_KP_Left,	KS_KP_4,
    KC(0x5c),				KS_KP_Begin,	KS_KP_5,
    KC(0x5d),				KS_KP_Right,	KS_KP_6,
    KC(0x5e),				KS_KP_Insert,	KS_KP_0,
    KC(0x5f),				KS_Find,
    KC(0x60),				KS_Prior,
    KC(0x62),				KS_Num_Lock,
    KC(0x63),				KS_Shift_L,
    KC(0x64),				KS_z,
    KC(0x65),				KS_x,
    KC(0x66),				KS_c,
    KC(0x67),				KS_v,
    KC(0x68),				KS_b,
    KC(0x69),				KS_n,
    KC(0x6a),				KS_m,
    KC(0x6b),				KS_comma,	KS_less,
    KC(0x6c),				KS_period,	KS_greater,
    KC(0x6d),				KS_slash,	KS_question,
    KC(0x6e),				KS_Shift_R,
    KC(0x6f),				KS_Linefeed,
    KC(0x70),				KS_KP_End,	KS_KP_1,
    KC(0x71),				KS_KP_Down,	KS_KP_2,
    KC(0x72),				KS_KP_Next,	KS_KP_3,
    KC(0x76),				KS_Help,
    KC(0x77),				KS_Caps_Lock,
    KC(0x78),				KS_Meta_L,
    KC(0x79),				KS_space,
    KC(0x7a),				KS_Meta_R,
    KC(0x7b),				KS_Next,
    KC(0x7d),				KS_KP_Add,
};

#define KBD_MAP(name, base, map) \
    { name, base, sizeof(map)/sizeof(keysym_t), map }

const struct wscons_keydesc comkbd_keydesctab[] = {
	KBD_MAP(KB_US, 0, comkbd_keydesc_us),
	{0, 0, 0, 0},
};

struct wskbd_mapdata comkbd_keymapdata = {
	comkbd_keydesctab, KB_US
};

#define	COMK_RX_RING	64
#define	COMK_TX_RING	64

struct comkbd_softc {
	struct device sc_dv;		/* us, as a device */
	bus_space_tag_t sc_iot;		/* bus tag */
	bus_space_handle_t sc_ioh;	/* bus handle */
	void *sc_ih, *sc_si;		/* interrupt vectors */

	u_int sc_rxcnt;
	u_int8_t sc_rxbuf[COMK_RX_RING];
	u_int8_t *sc_rxbeg, *sc_rxend, *sc_rxget, *sc_rxput;

	u_int sc_txcnt;
	u_int8_t sc_txbuf[COMK_TX_RING];
	u_int8_t *sc_txbeg, *sc_txend, *sc_txget, *sc_txput;

	u_int8_t sc_ier;

	struct device *sc_wskbddev;	/* child wskbd */
	int sc_leds;
	u_int8_t sc_kbdstate;
	int sc_layout;
};

#define	COM_WRITE(sc,r,v) \
    bus_space_write_1((sc)->sc_iot, (sc)->sc_ioh, (r), (v))
#define	COM_READ(sc,r) \
    bus_space_read_1((sc)->sc_iot, (sc)->sc_ioh, (r))

int comkbd_match __P((struct device *, void *, void *));
void comkbd_attach __P((struct device *, struct device *, void *));
int comkbd_iskbd __P((int));

/* wskbd glue */
void comkbd_cnpollc __P((void *, int));
void comkbd_cngetc __P((void *, u_int *, int *));
int comkbd_enable __P((void *, int));
void comkbd_setleds __P((void *, int));
int comkbd_getleds __P((void *));
int comkbd_ioctl __P((void *, u_long, caddr_t, int, struct proc *));

/* internals */
void comkbd_enqueue __P((struct comkbd_softc *, u_int8_t *, u_int));
void comkbd_raw __P((struct comkbd_softc *, u_int8_t));
void comkbd_init __P((struct comkbd_softc *));
void comkbd_putc __P((struct comkbd_softc *, u_int8_t));
int comkbd_intr __P((void *));
void comkbd_soft __P((void *));

struct cfattach comkbd_ca = {
	sizeof(struct comkbd_softc), comkbd_match, comkbd_attach
};

struct cfdriver comkbd_cd = {
	NULL, "comkbd", DV_DULL
};

char *comkbd_names[] = {
	"su",
	"su_pnp",
	NULL
};

struct wskbd_accessops comkbd_accessops = {
	comkbd_enable,
	comkbd_setleds,
	comkbd_ioctl
};

struct wskbd_consops comkbd_consops = {
	comkbd_cngetc,
	comkbd_cnpollc
};

int
comkbd_iskbd(node)
	int node;
{
	if (OF_getproplen(node, "keyboard") == 0)
		return (10);
	return (0);
}

int
comkbd_match(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct ebus_attach_args *ea = aux;
	int i;

	for (i = 0; comkbd_names[i]; i++)
		if (strcmp(ea->ea_name, comkbd_names[i]) == 0)
			return (comkbd_iskbd(ea->ea_node));

	if (strcmp(ea->ea_name, "serial") == 0) {
		char compat[80];

		if ((i = OF_getproplen(ea->ea_node, "compatible")) &&
		    OF_getprop(ea->ea_node, "compatible", compat,
			sizeof(compat)) == i) {
			if (strcmp(compat, "su16550") == 0 ||
			    strcmp(compat, "su") == 0)
				return (comkbd_iskbd(ea->ea_node));
		}
	}
	return (0);
}

void
comkbd_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct comkbd_softc *sc = (void *)self;
	struct ebus_attach_args *ea = aux;
	struct wskbddev_attach_args a;
	int console;

	sc->sc_iot = ea->ea_bustag;

	sc->sc_rxget = sc->sc_rxput = sc->sc_rxbeg = sc->sc_rxbuf;
	sc->sc_rxend = sc->sc_rxbuf + COMK_RX_RING;
	sc->sc_rxcnt = 0;

	sc->sc_txget = sc->sc_txput = sc->sc_txbeg = sc->sc_txbuf;
	sc->sc_txend = sc->sc_txbuf + COMK_TX_RING;
	sc->sc_txcnt = 0;

	console = (ea->ea_node == OF_instance_to_package(OF_stdin()));

	sc->sc_si = softintr_establish(IPL_TTY, comkbd_soft, sc);
	if (sc->sc_si == NULL) {
		printf(": can't get soft intr\n");
		return;
	}

	sc->sc_ih = bus_intr_establish(ea->ea_bustag,
	    ea->ea_intrs[0], IPL_TTY, 0, comkbd_intr, sc);
	if (sc->sc_ih == NULL) {
		printf(": can't get hard intr\n");
		return;
	}

	/* Use prom address if available, otherwise map it. */
	if (ea->ea_nvaddrs)
		sc->sc_ioh = (bus_space_handle_t)ea->ea_vaddrs[0];
	else if (ebus_bus_map(sc->sc_iot, 0,
			      EBUS_PADDR_FROM_REG(&ea->ea_regs[0]),
			      ea->ea_regs[0].size,
			      BUS_SPACE_MAP_LINEAR,
			      0, &sc->sc_ioh) != 0) {
		printf(": can't map register space\n");
                return;
	}

	if (console) {
		comkbd_init(sc);
		cn_tab->cn_dev = makedev(77, sc->sc_dv.dv_unit); /* XXX */
		cn_tab->cn_pollc = wskbd_cnpollc;
		cn_tab->cn_getc = wskbd_cngetc;
		wskbd_cnattach(&comkbd_consops, sc, &comkbd_keymapdata);
		sc->sc_ier = IER_ETXRDY | IER_ERXRDY;
		COM_WRITE(sc, com_ier, sc->sc_ier);
		COM_READ(sc, com_iir);
		COM_WRITE(sc, com_mcr, MCR_IENABLE | MCR_DTR | MCR_RTS);
	}

	a.console = console;
	a.keymap = &comkbd_keymapdata;
	a.accessops = &comkbd_accessops;
	a.accesscookie = sc;
	sc->sc_wskbddev = config_found(self, &a, wskbddevprint);
}

void
comkbd_cnpollc(vsc, on)
	void *vsc;
	int on;
{
}

void
comkbd_cngetc(v, type, data)
	void *v;
	u_int *type;
	int *data;
{
	struct comkbd_softc *sc = v;
	int s;
	u_int8_t c;

	s = splhigh();
	while (1) {
		if (COM_READ(sc, com_lsr) & LSR_RXRDY)
			break;
	}
	c = COM_READ(sc, com_data);
	COM_READ(sc, com_iir);
	splx(s);

	switch (c) {
	case SKBD_RSP_IDLE:
		*type = WSCONS_EVENT_ALL_KEYS_UP;
		*data = 0;
		break;
	default:
		*type = (c & 0x80) ?
		    WSCONS_EVENT_KEY_UP : WSCONS_EVENT_KEY_DOWN;
		*data = c & 0x7f;
		break;
	}
}

int
comkbd_ioctl(v, cmd, data, flag, p)
	void *v;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	switch (cmd) {
	case WSKBDIO_GTYPE:
		*(int *)data = WSKBD_TYPE_SUN;
		return (0);
	case WSKBDIO_SETLEDS:
		comkbd_setleds(v, *(int *)data);
		return (0);
	case WSKBDIO_GETLEDS:
		*(int *)data = comkbd_getleds(v);
		return (0);
	}
	return (-1);
}

int
comkbd_enable(vsc, on)
	void *vsc;
	int on;
{
	return (0);
}

int
comkbd_getleds(v)
	void *v;
{
	struct comkbd_softc *sc = v;

	return (sc->sc_leds);
}

void
comkbd_setleds(v, wled)
	void *v;
	int wled;
{
	struct comkbd_softc *sc = v;
	u_int8_t sled = 0;
	u_int8_t cmd[2];

	sc->sc_leds = wled;

	if (wled & WSKBD_LED_CAPS)
		sled |= SKBD_LED_CAPSLOCK;
	if (wled & WSKBD_LED_NUM)
		sled |= SKBD_LED_NUMLOCK;
	if (wled & WSKBD_LED_SCROLL)
		sled |= SKBD_LED_SCROLLLOCK;
	if (wled & WSKBD_LED_COMPOSE)
		sled |= SKBD_LED_COMPOSE;

	cmd[0] = SKBD_CMD_SETLED;
	cmd[1] = sled;
	comkbd_enqueue(sc, cmd, sizeof(cmd));
}

void
comkbd_putc(sc, c)
	struct comkbd_softc *sc;
	u_int8_t c;
{
	int s, timo;

	s = splhigh();

	timo = 150000;
	while (--timo) {
		if (COM_READ(sc, com_lsr) & LSR_TXRDY)
			break;
	}

	COM_WRITE(sc, com_data, c);
	bus_space_barrier(sc->sc_iot, sc->sc_ioh, 0, COM_NPORTS,
	    BUS_SPACE_BARRIER_READ | BUS_SPACE_BARRIER_WRITE);

	timo = 150000;
	while (--timo) {
		if (COM_READ(sc, com_lsr) & LSR_TXRDY)
			break;
	}

	splx(s);
}

void
comkbd_enqueue(sc, buf, buflen)
	struct comkbd_softc *sc;
	u_int8_t *buf;
	u_int buflen;
{
	int s;
	u_int i;

	s = spltty();

	/* See if there is room... */
	if ((sc->sc_txcnt + buflen) > COMK_TX_RING)
		return;

	for (i = 0; i < buflen; i++) {
		*sc->sc_txget = *buf;
		buf++;
		sc->sc_txcnt++;
		sc->sc_txget++;
		if (sc->sc_txget == sc->sc_txend)
			sc->sc_txget = sc->sc_txbeg;
	}

	comkbd_soft(sc);

	splx(s);
}

void
comkbd_soft(vsc)
	void *vsc;
{
	struct comkbd_softc *sc = vsc;
	u_int type;
	int value;
	u_int8_t c;

	while (sc->sc_rxcnt) {
		c = *sc->sc_rxget;
		if (++sc->sc_rxget == sc->sc_rxend)
			sc->sc_rxget = sc->sc_rxbeg;
		sc->sc_rxcnt--;
		switch (c) {
		case SKBD_RSP_IDLE:
			type = WSCONS_EVENT_ALL_KEYS_UP;
			value = 0;
			break;
		default:
			type = (c & 0x80) ?
			    WSCONS_EVENT_KEY_UP : WSCONS_EVENT_KEY_DOWN;
			value = c & 0x7f;
			break;
		}
		wskbd_input(sc->sc_wskbddev, type, value);
	}

	if (sc->sc_txcnt) {
		c = sc->sc_ier | IER_ETXRDY;
		if (c != sc->sc_ier) {
			COM_WRITE(sc, com_ier, c);
			sc->sc_ier = c;
		}
		if (COM_READ(sc, com_lsr) & LSR_TXRDY) {
			sc->sc_txcnt--;
			COM_WRITE(sc, com_data, *sc->sc_txput);
			if (++sc->sc_txput == sc->sc_txend)
				sc->sc_txput = sc->sc_txbeg;
		}
	}
}

int
comkbd_intr(vsc)
	void *vsc;
{
	struct comkbd_softc *sc = vsc;
	u_int8_t iir, lsr, data;
	int needsoft = 0;

	/* Nothing to do */
	iir = COM_READ(sc, com_iir);
	if (iir & IIR_NOPEND)
		return (0);

	for (;;) {
		lsr = COM_READ(sc, com_lsr);
		if (lsr & LSR_RXRDY) {
			needsoft = 1;

			do {
				data = COM_READ(sc, com_data);
				if (sc->sc_rxcnt != COMK_RX_RING) {
					*sc->sc_rxput = data;
					if (++sc->sc_rxput == sc->sc_rxend)
						sc->sc_rxput = sc->sc_rxbeg;
					sc->sc_rxcnt++;
				}
				lsr = COM_READ(sc, com_lsr);
			} while (lsr & LSR_RXRDY);
		}

		if (lsr & LSR_TXRDY) {
			if (sc->sc_txcnt == 0) {
				/* Nothing further to send */
				sc->sc_ier &= ~IER_ETXRDY;
				COM_WRITE(sc, com_ier, sc->sc_ier);
			} else
				needsoft = 1;
		}

		iir = COM_READ(sc, com_iir);
		if (iir & IIR_NOPEND)
			break;
	}

	if (needsoft)
		softintr_schedule(sc->sc_si);

	return (1);
}

void
comkbd_init(sc)
	struct comkbd_softc *sc;
{
	u_int8_t stat, c;
	int tries;

	for (tries = 5; tries != 0; tries--) {
		int ltries;

		sc->sc_leds = 0;
		sc->sc_layout = -1;

		/* Send reset request */
		comkbd_putc(sc, SKBD_CMD_RESET);

		ltries = 1000;
		while (--ltries > 0) {
			stat = COM_READ(sc,com_lsr);
			if (stat & LSR_RXRDY) {
				c = COM_READ(sc, com_data);
				
				comkbd_raw(sc, c);
				if (sc->sc_kbdstate == SKBD_STATE_RESET)
					break;
			}
			DELAY(1000);
		}
		if (ltries == 0)
			continue;

		/* Wait for reset to finish. */
		ltries = 1000;
		while (--ltries > 0) {
			stat = COM_READ(sc, com_lsr);
			if (stat & LSR_RXRDY) {
				c = COM_READ(sc, com_data);
				comkbd_raw(sc, c);
				if (sc->sc_kbdstate == SKBD_STATE_GETKEY)
					break;
			}
			DELAY(1000);
		}
		if (ltries == 0)
			continue;


		/* Send layout request */
		comkbd_putc(sc, SKBD_CMD_LAYOUT);

		ltries = 1000;
		while (--ltries > 0) {
			stat = COM_READ(sc, com_lsr);
			if (stat & LSR_RXRDY) {
				c = COM_READ(sc, com_data);
				comkbd_raw(sc, c);
				if (sc->sc_layout != -1)
					break;
			}
			DELAY(1000);
		}
		if (ltries != 0)
			break;
	}
	if (tries == 0)
		printf(": reset timeout\n");
	else
		printf(": layout %d\n", sc->sc_layout);
}

void
comkbd_raw(sc, c)
	struct comkbd_softc *sc;
	u_int8_t c;
{
	int claimed = 0;

	if (sc->sc_kbdstate == SKBD_STATE_LAYOUT) {
		sc->sc_kbdstate = SKBD_STATE_GETKEY;
		sc->sc_layout = c;
		return;
	}

	switch (c) {
	case SKBD_RSP_RESET:
		sc->sc_kbdstate = SKBD_STATE_RESET;
		claimed = 1;
		break;
	case SKBD_RSP_LAYOUT:
		sc->sc_kbdstate = SKBD_STATE_LAYOUT;
		claimed = 1;
		break;
	case SKBD_RSP_IDLE:
		sc->sc_kbdstate = SKBD_STATE_GETKEY;
		claimed = 1;
	}

	if (claimed)
		return;

	switch (sc->sc_kbdstate) {
	case SKBD_STATE_RESET:
		sc->sc_kbdstate = SKBD_STATE_GETKEY;
		if (c != SKBD_RSP_RESET_OK)
			printf("%s: reset1 invalid code 0x%02x\n",
			    sc->sc_dv.dv_xname, c);
		break;
	case SKBD_STATE_GETKEY:
		break;
	}
}
