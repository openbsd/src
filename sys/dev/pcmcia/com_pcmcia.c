/*	$OpenBSD: com_pcmcia.c,v 1.11 1999/01/21 08:55:08 niklas Exp $	*/
/*	$NetBSD: com_pcmcia.c,v 1.15 1998/08/22 17:47:58 msaitoh Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
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

/*-
 * Copyright (c) 1991 The Regents of the University of California.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)com.c	7.5 (Berkeley) 5/16/91
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/tty.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/types.h>
#include <sys/device.h>

#include <machine/intr.h>
#include <machine/bus.h>

#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciadevs.h>

#include <dev/isa/isavar.h>

#include <dev/ic/comreg.h>
#ifdef i386
#include <i386/isa/pccomvar.h>
#else
#include <dev/ic/comvar.h>
#endif
#include <dev/ic/ns16550reg.h>

#include <dev/isa/isareg.h>

#include "com.h"
#ifdef i386
#include "pccom.h"
#endif

#define	com_lcr		com_cfcr
#define	SET(t, f)	(t) |= (f)

struct com_dev {
	char *name;
	char *cis1_info[4];
};

/* Devices that we need to match by CIS strings */
static struct com_dev com_devs[] = {
	{ PCMCIA_STR_MEGAHERTZ_XJ2288,
	  PCMCIA_CIS_MEGAHERTZ_XJ2288 },
};


static int com_devs_size = sizeof(com_devs) / sizeof(com_devs[0]);
static struct com_dev *com_dev_match __P((struct pcmcia_card *));

int com_pcmcia_match __P((struct device *, void *, void *));
void com_pcmcia_attach __P((struct device *, struct device *, void *));
void com_pcmcia_cleanup __P((void *));

int com_pcmcia_enable __P((struct com_softc *));
void com_pcmcia_disable __P((struct com_softc *));
int com_pcmcia_enable1 __P((struct com_softc *));
void com_pcmcia_disable1 __P((struct com_softc *));

void com_attach __P((struct com_softc *));

struct com_pcmcia_softc {
	struct com_softc sc_com;		/* real "com" softc */

	/* PCMCIA-specific goo */
	struct pcmcia_io_handle sc_pcioh;	/* PCMCIA i/o space info */
	int sc_io_window;			/* our i/o window */
	struct pcmcia_function *sc_pf;		/* our PCMCIA function */
	void *sc_ih;				/* interrupt handler */
};

#if NCOM_PCMCIA
struct cfattach com_pcmcia_ca = {
	sizeof(struct com_pcmcia_softc), com_pcmcia_match, com_pcmcia_attach
};
#elif NPCCOM_PCMCIA
struct cfattach pccom_pcmcia_ca = {
	sizeof(struct com_pcmcia_softc), com_pcmcia_match, com_pcmcia_attach
};
#endif

/* Look for pcmcia cards with particular CIS strings */
static struct com_dev *
com_dev_match(card)
	struct pcmcia_card *card;
{
	int i, j;

	for (i = 0; i < com_devs_size; i++) {
		for (j = 0; j < 4; j++)
			if (com_devs[i].cis1_info[j] &&
			    strcmp(com_devs[i].cis1_info[j],
				   card->cis1_info[j]))
				break;
		if (j == 4)
			return &com_devs[i];
	}

	return NULL;
}


int
com_pcmcia_match(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	int comportmask;
	struct pcmcia_attach_args *pa = aux;
	struct pcmcia_config_entry *cfe;

	/* 1. Does it claim to be a serial device? */
	if (pa->pf->function == PCMCIA_FUNCTION_SERIAL)
	    return 1;

	/* 2. Does it have all four 'standard' port ranges? */
	comportmask = 0;
	for (cfe = pa->pf->cfe_head.sqh_first; cfe;
	     cfe = cfe->cfe_list.sqe_next) {
		switch (cfe->iospace[0].start) {
		case IO_COM1:
			comportmask |= 1;
			break;
		case IO_COM2:
			comportmask |= 2;
			break;
		case IO_COM3:
			comportmask |= 4;
			break;
		case IO_COM4:
			comportmask |= 8;
			break;
		}
	}

	if (comportmask == 15)
		return 1;

	/* 3. Is this a card we know about? */
	if (com_dev_match(pa->card) != NULL)
		return 1;

	return 0;
}

void
com_pcmcia_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct com_pcmcia_softc *psc = (void *) self;
	struct com_softc *sc = &psc->sc_com;
	struct pcmcia_attach_args *pa = aux;
	struct pcmcia_config_entry *cfe;
	int autoalloc = 0;

	psc->sc_pf = pa->pf;

retry:
	/* find a cfe we can use */

	for (cfe = pa->pf->cfe_head.sqh_first; cfe;
	     cfe = cfe->cfe_list.sqe_next) {
#if 0
		/*
		 * Some modem cards (e.g. Xircom CM33) also have
		 * mem space.  Don't bother with this check.
		 */
		if (cfe->num_memspace != 0)
			continue;
#endif

		if (cfe->num_iospace != 1)
			continue;

		if (!pcmcia_io_alloc(pa->pf,
		    autoalloc ? 0 : cfe->iospace[0].start,
		    cfe->iospace[0].length, (1 << cfe->iomask),
		    &psc->sc_pcioh)) {
			goto found;
		}
	}
	if (autoalloc == 0) {
		autoalloc = 1;
		goto retry;
	} else if (!cfe) {
		printf(": can't allocate i/o space\n");
		return;
	}

found:
	sc->sc_iot = psc->sc_pcioh.iot;
	sc->sc_ioh = psc->sc_pcioh.ioh;

	/* Enable the card. */
	pcmcia_function_init(pa->pf, cfe);
	if (com_pcmcia_enable1(sc))
		printf(": function enable failed\n");

#ifdef notyet
	sc->enabled = 1;
#endif

	/* map in the io space */

	if (pcmcia_io_map(pa->pf, ((cfe->flags & PCMCIA_CFE_IO16) ?
	    PCMCIA_WIDTH_IO16 : PCMCIA_WIDTH_IO8), 0, psc->sc_pcioh.size,
	    &psc->sc_pcioh, &psc->sc_io_window)) {
		printf(": can't map i/o space\n");
		return;
	}

	sc->sc_iobase = -1;
#ifdef notyet
	sc->sc_frequency = COM_FREQ;

	sc->enable = com_pcmcia_enable;
	sc->disable = com_pcmcia_disable;
	
	printf(": serial device");

	com_attach_subr(sc);
#endif
	com_attach(sc);

	/* establish the interrupt. */
	psc->sc_ih = pcmcia_intr_establish(pa->pf, IPL_TTY, comintr, sc);
	if (psc->sc_ih == NULL)
		printf("%s: couldn't establish interrupt\n",
		    sc->sc_dev.dv_xname);

#ifdef notyet
	sc->enabled = 0;
	
	com_pcmcia_disable1(sc);
#endif
}

int
com_pcmcia_enable(sc)
	struct com_softc *sc;
{
	struct com_pcmcia_softc *psc = (struct com_pcmcia_softc *) sc;
	struct pcmcia_function *pf = psc->sc_pf;

	/* establish the interrupt. */
	psc->sc_ih = pcmcia_intr_establish(pf, IPL_TTY, comintr, sc);
	if (psc->sc_ih == NULL) {
		printf("%s: couldn't establish interrupt\n",
		    sc->sc_dev.dv_xname);
		return (1);
	}
	return com_pcmcia_enable1(sc);
}

int
com_pcmcia_enable1(sc)
	struct com_softc *sc;
{
	struct com_pcmcia_softc *psc = (struct com_pcmcia_softc *) sc;
	struct pcmcia_function *pf = psc->sc_pf;
	int ret;

	if ((ret = pcmcia_function_enable(pf)))
	    return(ret);

	if (psc->sc_pf->sc->card.product == PCMCIA_PRODUCT_3COM_3C562) {
		int reg;

		/* turn off the ethernet-disable bit */

		reg = pcmcia_ccr_read(pf, PCMCIA_CCR_OPTION);
		if (reg & 0x08) {
		    reg &= ~0x08;
		    pcmcia_ccr_write(pf, PCMCIA_CCR_OPTION, reg);
		}
	}

	return(ret);
}

void
com_pcmcia_disable(sc)
	struct com_softc *sc;
{
	struct com_pcmcia_softc *psc = (struct com_pcmcia_softc *) sc;

	com_pcmcia_disable1(sc);
	pcmcia_intr_disestablish(psc->sc_pf, psc->sc_ih);
}

void
com_pcmcia_disable1(sc)
	struct com_softc *sc;
{
	struct com_pcmcia_softc *psc = (struct com_pcmcia_softc *) sc;

	pcmcia_function_disable(psc->sc_pf);
}

/* 
 * XXX This should be handled by a generic attach
 */
void
com_attach(sc)
	struct com_softc *sc;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	u_int8_t lcr;

	sc->sc_hwflags = 0;
	sc->sc_swflags = 0;

	/*
	 * Probe for all known forms of UART.
	 */
	lcr = bus_space_read_1(iot, ioh, com_lcr);

	bus_space_write_1(iot, ioh, com_lcr, LCR_EFR);
	bus_space_write_1(iot, ioh, com_efr, 0);
	bus_space_write_1(iot, ioh, com_lcr, 0);

	bus_space_write_1(iot, ioh, com_fifo, FIFO_ENABLE);
	delay(100);

	switch(bus_space_read_1(iot, ioh, com_iir) >> 6) {
	case 0:
		sc->sc_uarttype = COM_UART_16450;
		break;
	case 2:
		sc->sc_uarttype = COM_UART_16550;
		break;
	case 3:
		sc->sc_uarttype = COM_UART_16550A;
		break;
	default:
		sc->sc_uarttype = COM_UART_UNKNOWN;
		break;
	}

	if (sc->sc_uarttype == COM_UART_16550A) { /* Probe for ST16650s */
		bus_space_write_1(iot, ioh, com_lcr, lcr | LCR_DLAB);
		if (bus_space_read_1(iot, ioh, com_efr) == 0) {
			sc->sc_uarttype = COM_UART_ST16650;
		} else {
			bus_space_write_1(iot, ioh, com_lcr, LCR_EFR);
			if (bus_space_read_1(iot, ioh, com_efr) == 0)
				sc->sc_uarttype = COM_UART_ST16650V2;
		}
	}

#ifdef i386
	if (sc->sc_uarttype == COM_UART_ST16650V2) {	/* Probe for XR16850s */
		u_int8_t dlbl, dlbh;

		/* Enable latch access and get the current values. */
		bus_space_write_1(iot, ioh, com_lcr, lcr | LCR_DLAB);
		dlbl = bus_space_read_1(iot, ioh, com_dlbl);
		dlbh = bus_space_read_1(iot, ioh, com_dlbh);

		/* Zero out the latch divisors */
		bus_space_write_1(iot, ioh, com_dlbl, 0);
		bus_space_write_1(iot, ioh, com_dlbh, 0);

		if (bus_space_read_1(iot, ioh, com_dlbh) == 0x10) {
			sc->sc_uarttype = COM_UART_XR16850;
			sc->sc_uartrev = bus_space_read_1(iot, ioh, com_dlbl);
		}

		/* Reset to original. */
		bus_space_write_1(iot, ioh, com_dlbl, dlbl);
		bus_space_write_1(iot, ioh, com_dlbh, dlbh);
	}
#endif
	
	/* Reset the LCR (latch access is probably enabled). */
	bus_space_write_1(iot, ioh, com_lcr, lcr);
	if (sc->sc_uarttype == COM_UART_16450) { /* Probe for 8250 */
		u_int8_t scr0, scr1, scr2;

		scr0 = bus_space_read_1(iot, ioh, com_scratch);
		bus_space_write_1(iot, ioh, com_scratch, 0xa5);
		scr1 = bus_space_read_1(iot, ioh, com_scratch);
		bus_space_write_1(iot, ioh, com_scratch, 0x5a);
		scr2 = bus_space_read_1(iot, ioh, com_scratch);
		bus_space_write_1(iot, ioh, com_scratch, scr0);

		if ((scr1 != 0xa5) || (scr2 != 0x5a))
			sc->sc_uarttype = COM_UART_8250;
	}

	/*
	 * Print UART type and initialize ourself.
	 */
	sc->sc_fifolen = 1;	/* default */
	switch (sc->sc_uarttype) {
	case COM_UART_UNKNOWN:
		printf(": unknown uart\n");
		break;
	case COM_UART_8250:
		printf(": ns8250, no fifo\n");
		break;
	case COM_UART_16450:
		printf(": ns16450, no fifo\n");
		break;
	case COM_UART_16550:
		printf(": ns16550, no working fifo\n");
		break;
	case COM_UART_16550A:
		printf(": ns16550a, 16 byte fifo\n");
		SET(sc->sc_hwflags, COM_HW_FIFO);
		sc->sc_fifolen = 16;
		break;
	case COM_UART_ST16650:
		printf(": st16650, no working fifo\n");
		break;
	case COM_UART_ST16650V2:
		printf(": st16650, 32 byte fifo\n");
		SET(sc->sc_hwflags, COM_HW_FIFO);
		sc->sc_fifolen = 32;
		break;
#ifdef i386
	case COM_UART_XR16850:
		printf(": xr16850 (rev %d), 128 byte fifo\n", sc->sc_uartrev);
		SET(sc->sc_hwflags, COM_HW_FIFO);
		sc->sc_fifolen = 128;
		break;
#endif
	default:
		panic("comattach: bad fifo type");
	}

	/* clear and disable fifo */
	bus_space_write_1(iot, ioh, com_fifo, FIFO_RCV_RST | FIFO_XMT_RST);
	(void)bus_space_read_1(iot, ioh, com_data);
	bus_space_write_1(iot, ioh, com_fifo, 0);
}
