/*	$OpenBSD: comkbd_ebus.c,v 1.2 2002/01/25 03:24:53 jason Exp $	*/

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

#include <dev/sun/sunkbdreg.h>
#include <dev/sun/sunkbdvar.h>

#include <dev/ic/comreg.h>
#include <dev/ic/comvar.h>
#include <dev/ic/ns16550reg.h>

#include <dev/cons.h>

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
		wskbd_cnattach(&comkbd_consops, sc, &sunkbd_keymapdata);
		sc->sc_ier = IER_ETXRDY | IER_ERXRDY;
		COM_WRITE(sc, com_ier, sc->sc_ier);
		COM_READ(sc, com_iir);
		COM_WRITE(sc, com_mcr, MCR_IENABLE | MCR_DTR | MCR_RTS);
	}

	a.console = console;
	a.keymap = &sunkbd_keymapdata;
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
