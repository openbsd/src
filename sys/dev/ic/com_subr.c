/*	$OpenBSD: com_subr.c,v 1.5 2005/11/02 22:35:06 fgsch Exp $	*/

/*
 * Copyright (c) 1997 - 1999, Jason Downs.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*-
 * Copyright (c) 1993, 1994, 1995, 1996
 *	Charles M. Hannum.  All rights reserved.
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
 * 3. Neither the name of the University nor the names of its contributors
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

/*
 * COM driver, based on HP dca driver
 * uses National Semiconductor NS16450/NS16550AF UART
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/tty.h>

#include "com.h"
#ifdef i386
#include "pccom.h"
#else
#define	NPCCOM	0
#endif

#include <machine/bus.h>
#if defined(__sparc64__) || !defined(__sparc__)
#include <machine/intr.h>
#endif

#if !defined(__sparc__) || defined(__sparc64__)
#define	COM_CONSOLE
#include <dev/cons.h>
#endif

#include <dev/ic/comreg.h>
#include <dev/ic/ns16550reg.h>
#define	com_lcr	com_cfcr

#if NCOM > 0
#include <dev/ic/comvar.h>
#endif
#if NPCCOM > 0
#include <dev/isa/isavar.h>
#include <i386/isa/pccomvar.h>
#endif

#ifdef COM_PXA2X0
#define com_isr 8
#define ISR_SEND	(ISR_RXPL | ISR_XMODE | ISR_XMITIR)
#define ISR_RECV	(ISR_RXPL | ISR_XMODE | ISR_RCVEIR)
#endif

#ifdef	COM_CONSOLE
#include <sys/conf.h>
cdev_decl(com);
#endif

void	com_enable_debugport(struct com_softc *);
void	com_fifo_probe(struct com_softc *);

#if defined(COM_CONSOLE) || defined(KGDB)
void
com_enable_debugport(sc)
	struct com_softc *sc;
{
	int s;

	/* Turn on line break interrupt, set carrier. */
	s = splhigh();
#ifdef KGDB
	SET(sc->sc_ier, IER_ERXRDY);
#ifdef COM_PXA2X0
	if (sc->sc_uarttype == COM_UART_PXA2X0)
		sc->sc_ier |= IER_EUART | IER_ERXTOUT;
#endif
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, com_ier, sc->sc_ier);
#endif
	SET(sc->sc_mcr, MCR_DTR | MCR_RTS | MCR_IENABLE);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, com_mcr, sc->sc_mcr);

	splx(s);
}
#endif	/* COM_CONSOLED || KGDB */

void
com_attach_subr(sc)
	struct com_softc *sc;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	u_int8_t lcr;

	sc->sc_ier = 0;
#ifdef COM_PXA2X0
	if (sc->sc_uarttype == COM_UART_PXA2X0)
		sc->sc_ier |= IER_EUART;
#endif
	/* disable interrupts */
	bus_space_write_1(iot, ioh, com_ier, sc->sc_ier);

#ifdef COM_CONSOLE
	if (sc->sc_iobase == comconsaddr) {
		comconsattached = 1;
		delay(10000);			/* wait for output to finish */
		SET(sc->sc_hwflags, COM_HW_CONSOLE);
		SET(sc->sc_swflags, COM_SW_SOFTCAR);
	}
#endif

	/*
	 * Probe for all known forms of UART.
	 */
	lcr = bus_space_read_1(iot, ioh, com_lcr);
	bus_space_write_1(iot, ioh, com_lcr, LCR_EFR);
	bus_space_write_1(iot, ioh, com_efr, 0);
	bus_space_write_1(iot, ioh, com_lcr, 0);

	bus_space_write_1(iot, ioh, com_fifo, FIFO_ENABLE);
	delay(100);

#ifdef COM_PXA2X0
	/* Attachment driver presets COM_UART_PXA2X0. */
	if (sc->sc_uarttype != COM_UART_PXA2X0)
#endif
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

#if NPCCOM > 0	/* until com works with large FIFOs */
	if (sc->sc_uarttype == COM_UART_ST16650V2) { /* Probe for XR16850s */
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

	if (sc->sc_uarttype == COM_UART_16550A) { /* Probe for TI16750s */
		bus_space_write_1(iot, ioh, com_lcr, lcr | LCR_DLAB);
		bus_space_write_1(iot, ioh, com_fifo,
		    FIFO_ENABLE | FIFO_ENABLE_64BYTE);
		if ((bus_space_read_1(iot, ioh, com_iir) >> 5) == 7) {
#if 0
			bus_space_write_1(iot, ioh, com_lcr, 0);
			if ((bus_space_read_1(iot, ioh, com_iir) >> 5) == 6)
#endif
				sc->sc_uarttype = COM_UART_TI16750;
		}
		bus_space_write_1(iot, ioh, com_fifo, FIFO_ENABLE);
	}

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
		if (sc->sc_fifolen == 0)
			sc->sc_fifolen = 16;
		printf(": ns16550a, %d byte fifo\n", sc->sc_fifolen);
		SET(sc->sc_hwflags, COM_HW_FIFO);
		break;
#ifdef COM_PXA2X0
	case COM_UART_PXA2X0:
		printf(": pxa2x0, 32 byte fifo");
		SET(sc->sc_hwflags, COM_HW_FIFO);
		sc->sc_fifolen = 32;
		if (sc->sc_iobase == comsiraddr) {
			SET(sc->sc_hwflags, COM_HW_SIR);
			printf(" (SIR)");
		}
		printf("\n");
		break;
#endif
	case COM_UART_ST16650:
		printf(": st16650, no working fifo\n");
		break;
	case COM_UART_ST16650V2:
		if (sc->sc_fifolen == 0)
			sc->sc_fifolen = 32;
		printf(": st16650, %d byte fifo\n", sc->sc_fifolen);
		SET(sc->sc_hwflags, COM_HW_FIFO);
		break;
	case COM_UART_TI16750:
		printf(": ti16750, 64 byte fifo\n");
		SET(sc->sc_hwflags, COM_HW_FIFO);
		sc->sc_fifolen = 64;
		break;
#if NPCCOM > 0
	case COM_UART_XR16850:
		printf(": xr16850 (rev %d), 128 byte fifo\n", sc->sc_uartrev);
		SET(sc->sc_hwflags, COM_HW_FIFO);
		sc->sc_fifolen = 128;
		break;
#endif
	default:
		panic("comattach: bad fifo type");
	}

#ifdef notyet
	com_fifo_probe(sc);
#endif

	if (sc->sc_fifolen == 0) {
		CLR(sc->sc_hwflags, COM_HW_FIFO);
		sc->sc_fifolen = 1;
	}

	/* clear and disable fifo */
	bus_space_write_1(iot, ioh, com_fifo, FIFO_RCV_RST | FIFO_XMT_RST);
	(void)bus_space_read_1(iot, ioh, com_data);
	bus_space_write_1(iot, ioh, com_fifo, 0);

	sc->sc_mcr = 0;
	bus_space_write_1(iot, ioh, com_mcr, sc->sc_mcr);

#ifdef KGDB
	/*
	 * Allow kgdb to "take over" this port.  If this is
	 * the kgdb device, it has exclusive use.
	 */

	if (iot == com_kgdb_iot && sc->sc_iobase == com_kgdb_addr &&
	    !ISSET(sc->sc_hwflags, COM_HW_CONSOLE)) {
		printf("%s: kgdb\n", sc->sc_dev.dv_xname);
		SET(sc->sc_hwflags, COM_HW_KGDB);
	}
#endif /* KGDB */

#ifdef COM_CONSOLE
	if (ISSET(sc->sc_hwflags, COM_HW_CONSOLE)) {
		int maj;

		/* locate the major number */
		for (maj = 0; maj < nchrdev; maj++)
			if (cdevsw[maj].d_open == comopen)
				break;

		if (maj < nchrdev && cn_tab->cn_dev == NODEV)
			cn_tab->cn_dev = makedev(maj, sc->sc_dev.dv_unit);

		printf("%s: console\n", sc->sc_dev.dv_xname);
	}
#endif

	timeout_set(&sc->sc_diag_tmo, comdiag, sc);
	timeout_set(&sc->sc_dtr_tmo, com_raisedtr, sc);
#if NCOM > 0
#ifdef __HAVE_GENERIC_SOFT_INTERRUPTS
	sc->sc_si = softintr_establish(IPL_TTY, comsoft, sc);
	if (sc->sc_si == NULL)
		panic("%s: can't establish soft interrupt",
		    sc->sc_dev.dv_xname);
#else
	timeout_set(&sc->sc_comsoft_tmo, comsoft, sc);
#endif
#endif	/* NCOM */

	/*
	 * If there are no enable/disable functions, assume the device
	 * is always enabled.
	 */
	if (!sc->enable)
		sc->enabled = 1;

#if defined(COM_CONSOLE) || defined(KGDB)
	if (ISSET(sc->sc_hwflags, COM_HW_CONSOLE|COM_HW_KGDB))
		com_enable_debugport(sc);
#endif
}

void
com_fifo_probe(struct com_softc *sc)
{
	bus_space_handle_t ioh = sc->sc_ioh;
	bus_space_tag_t iot = sc->sc_iot;
	u_int8_t fifo, ier;
	int timo, len;

	if (!ISSET(sc->sc_hwflags, COM_HW_FIFO))
		return;

	ier = 0;
#ifdef COM_PXA2X0
	if (sc->sc_uarttype == COM_UART_PXA2X0)
		ier |= IER_EUART;
#endif
	bus_space_write_1(iot, ioh, com_ier, ier);
	bus_space_write_1(iot, ioh, com_lcr, LCR_DLAB);
	bus_space_write_1(iot, ioh, com_dlbl, 3);
	bus_space_write_1(iot, ioh, com_dlbh, 0);
	bus_space_write_1(iot, ioh, com_lcr, LCR_PNONE | LCR_8BITS);
	bus_space_write_1(iot, ioh, com_mcr, MCR_LOOPBACK);

	fifo = FIFO_ENABLE | FIFO_RCV_RST | FIFO_XMT_RST;
	if (sc->sc_uarttype == COM_UART_TI16750)
		fifo |= FIFO_ENABLE_64BYTE;

	bus_space_write_1(iot, ioh, com_fifo, fifo);

	for (len = 0; len < 256; len++) {
		bus_space_write_1(iot, ioh, com_data, (len + 1));
		timo = 2000;
		while (!ISSET(bus_space_read_1(iot, ioh, com_lsr),
		    LSR_TXRDY) && --timo)
			delay(1);
		if (!timo)
			break;
	}

	delay(100);

	for (len = 0; len < 256; len++) {
		timo = 2000;
		while (!ISSET(bus_space_read_1(iot, ioh, com_lsr),
		    LSR_RXRDY) && --timo)
			delay(1);
		if (!timo || bus_space_read_1(iot, ioh, com_data) != (len + 1))
			break;
	}

	/* For safety, always use the smaller value. */
	if (sc->sc_fifolen > len) {
		printf("%s: probed fifo depth: %d bytes\n",
		    sc->sc_dev.dv_xname, len);
		sc->sc_fifolen = len;
	}
}
