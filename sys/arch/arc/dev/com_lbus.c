/*	$OpenBSD: com_lbus.c,v 1.7 1998/03/16 09:38:41 pefo Exp $	*/

/*
 * Copyright (c) 1993, 1994 Charles Hannum.
 * Copyright (c) 1990 William F. Jolitz, TeleMuse
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
 *	This software is a component of "386BSD" developed by 
 *	William F. Jolitz, TeleMuse.
 * 4. Neither the name of the developer nor the name "386BSD"
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS A COMPONENT OF 386BSD DEVELOPED BY WILLIAM F. JOLITZ 
 * AND IS INTENDED FOR RESEARCH AND EDUCATIONAL PURPOSES ONLY. THIS 
 * SOFTWARE SHOULD NOT BE CONSIDERED TO BE A COMMERCIAL PRODUCT. 
 * THE DEVELOPER URGES THAT USERS WHO REQUIRE A COMMERCIAL PRODUCT 
 * NOT MAKE USE OF THIS WORK.
 *
 * FOR USERS WHO WISH TO UNDERSTAND THE 386BSD SYSTEM DEVELOPED
 * BY WILLIAM F. JOLITZ, WE RECOMMEND THE USER STUDY WRITTEN 
 * REFERENCES SUCH AS THE  "PORTING UNIX TO THE 386" SERIES 
 * (BEGINNING JANUARY 1991 "DR. DOBBS JOURNAL", USA AND BEGINNING 
 * JUNE 1991 "UNIX MAGAZIN", GERMANY) BY WILLIAM F. JOLITZ AND 
 * LYNNE GREER JOLITZ, AS WELL AS OTHER BOOKS ON UNIX AND THE 
 * ON-LINE 386BSD USER MANUAL BEFORE USE. A BOOK DISCUSSING THE INTERNALS 
 * OF 386BSD ENTITLED "386BSD FROM THE INSIDE OUT" WILL BE AVAILABLE LATE 1992.
 *
 * THIS SOFTWARE IS PROVIDED BY THE DEVELOPER ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE DEVELOPER BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Device Driver for Serial ports
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/tty.h>

#include <machine/autoconf.h>
#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/isa/isavar.h>	/* XXX for isa_chipset_tag_t in com_softc */

#include <dev/ic/comreg.h>
#include <dev/ic/comvar.h>
#include <dev/ic/ns16550reg.h>

/* Macros to clear/set/test flags. */
#define	SET(t, f)	(t) |= (f)
#define	CLR(t, f)	(t) &= ~(f)
#define	ISSET(t, f)	((t) & (f))

int	com_localbus_probe __P((struct device *, void *, void *));
void	com_localbus_attach __P((struct device *, struct device *, void *));


struct cfattach com_pica_ca = {
	sizeof(struct com_softc), com_localbus_probe, com_localbus_attach
};

struct cfattach com_algor_ca = {
	sizeof(struct com_softc), com_localbus_probe, com_localbus_attach
};

int
com_localbus_probe(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int iobase;
	int rv = 1;
	struct confargs *ca = aux;

	if(!BUS_MATCHNAME(ca, "com"))
		return(0);
	ioh = iobase = (long)BUS_CVTADDR(ca);
	iot = &arc_bus_io;

	/* if it's in use as console, it's there. */
	if (iobase == comconsaddr && !comconsattached) {
		return(rv);
	}
	rv = comprobe1(iot, ioh);
	return (rv);
}

void
com_localbus_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct com_softc *sc = (void *)self;
	int iobase;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	struct confargs *ca = aux;

	sc->sc_hwflags = 0;
	sc->sc_swflags = 0;
	sc->sc_iobase = iobase = (bus_addr_t)BUS_CVTADDR(ca);
	sc->sc_ioh = ioh = (bus_space_handle_t)iobase;
	sc->sc_iot = iot = &arc_bus_io;

	if (iobase == comconsaddr) {
		comconsattached = 1;

		/* 
		 * Need to reset baud rate, etc. of next print so reset
		 * comconsinit.  Also make sure console is always "hardwired".
		 */
		delay(1000);			/* wait for output to finish */
		comconsinit = 0;
		SET(sc->sc_hwflags, COM_HW_CONSOLE);
		SET(sc->sc_swflags, COM_SW_SOFTCAR);
	}


	/* look for a NS 16550AF UART with FIFOs */
	sc->sc_fifolen = 1;
	bus_space_write_1(iot, ioh, com_fifo,
	    FIFO_ENABLE | FIFO_RCV_RST | FIFO_XMT_RST | FIFO_TRIGGER_14);
	delay(100);
	if (ISSET(bus_space_read_1(iot, ioh, com_iir), IIR_FIFO_MASK) ==
	    IIR_FIFO_MASK) {
		if (ISSET(bus_space_read_1(iot, ioh, com_fifo),
		    FIFO_TRIGGER_14) == FIFO_TRIGGER_14) {
			SET(sc->sc_hwflags, COM_HW_FIFO);
			printf(": ns16550a, working fifo\n");
			sc->sc_fifolen = 16;
		}
		else {
			printf(": ns16550, broken fifo\n");
		}
	}
	else {
		printf(": ns8250 or ns16450, no fifo\n");
	}
	bus_space_write_1(iot, ioh, com_fifo, 0);

	/* disable interrupts */
	bus_space_write_1(iot, ioh, com_ier, 0);
	bus_space_write_1(iot, ioh, com_mcr, 0);

	BUS_INTR_ESTABLISH(ca, comintr, (void *)(long)sc);

#ifdef KGDB
	if (kgdb_dev == makedev(commajor, unit)) {
		if (ISSET(sc->sc_hwflags, COM_HW_CONSOLE))
			kgdb_dev = -1;	/* can't debug over console port */
		else {
			cominit(iot, ioh, kgdb_rate);
			if (kgdb_debug_init) {
				/*
				 * Print prefix of device name,
				 * let kgdb_connect print the rest.
				 */
				printf("%s: ", sc->sc_dev.dv_xname);
				kgdb_connect(1);
			} else
				printf("%s: kgdb enabled\n",
				    sc->sc_dev.dv_xname);
		}
	}
#endif

	/* XXX maybe move up some? */
	if (ISSET(sc->sc_hwflags, COM_HW_CONSOLE))
		printf("%s: console\n", sc->sc_dev.dv_xname);
}
