/*	$OpenBSD: com_supio.c,v 1.3 2002/01/30 20:45:34 nordin Exp $	*/
/*	$NetBSD: com_supio.c,v 1.3 1997/08/27 20:41:30 is Exp $	*/

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

#include <dev/isa/isavar.h>
#include <dev/ic/comreg.h>
#include <dev/ic/comvar.h>

#include <amiga/amiga/isr.h>
#include <amiga/dev/supio.h>

struct comsupio_softc {
	struct com_softc sc_com;
	struct isr sc_isr;
};

int com_supio_match __P((struct device *, void *, void *));
void com_supio_attach __P((struct device *, struct device *, void *));
void com_supio_cleanup __P((void *));

static int      comconsaddr;
static bus_space_handle_t comconsioh; 
#if 0
static int      comconsattached;
static bus_space_tag_t comconstag;
static int comconsrate; 
static tcflag_t comconscflag;
#endif

struct cfattach com_supio_ca = {
	sizeof(struct comsupio_softc), com_supio_match, com_supio_attach
};

int
com_supio_match(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct cfdata *cfp = match;
	bus_space_tag_t iot;
	int iobase;
	int rv = 1;
	struct supio_attach_args *supa = aux;

	iot = supa->supio_iot;
	iobase = supa->supio_iobase;

	if (strcmp(supa->supio_name,"com") || (cfp->cf_unit > 1))
		return 0;
#if 0
	/* if it's in use as console, it's there. */
	if (iobase != comconsaddr || comconsattached) {
		if (bus_space_map(iot, iobase, COM_NPORTS, 0, &ioh)) {
			return 0;
		}
		rv = comprobe1(iot, ioh, iobase);
		bus_space_unmap(iot, ioh, COM_NPORTS);
	}
#endif
	return (rv);
}

void
com_supio_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
#ifdef __OpenBSD__
	struct comsupio_softc *sc = (void *)self;
	struct com_softc *csc = &sc->sc_com;
	int iobase;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	struct supio_attach_args *supa = aux;

	csc->sc_hwflags = 0;
	csc->sc_swflags = 0;

	iobase = csc->sc_iobase = supa->supio_iobase;
	iot = csc->sc_iot = supa->supio_iot;
	if (iobase != comconsaddr) {
	        if (bus_space_map(iot, iobase, COM_NPORTS, 0, &ioh))
			panic("comattach: io mapping failed");
	} else
	        ioh = comconsioh;

	csc->sc_iot = iot;
	csc->sc_ioh = ioh;
	csc->sc_iobase = iobase;

	if (iobase == comconsaddr) {
		comconsattached = 1;

		/* 
		 * Need to reset baud rate, etc. of next print so reset
		 * comconsinit.  Also make sure console is always "hardwired".
		 */
		delay(1000);			/* wait for output to finish */
		comconsinit = 0;
		SET(csc->sc_hwflags, COM_HW_CONSOLE);
		SET(csc->sc_swflags, COM_SW_SOFTCAR);
	}


	/* look for a NS 16550AF UART with FIFOs */
	bus_space_write_1(iot, ioh, com_fifo,
	    FIFO_ENABLE | FIFO_RCV_RST | FIFO_XMT_RST | FIFO_TRIGGER_14);
	delay(100);
	if (ISSET(bus_space_read_1(iot, ioh, com_iir), IIR_FIFO_MASK) ==
	    IIR_FIFO_MASK) {
		if (ISSET(bus_space_read_1(iot, ioh, com_fifo),
		    FIFO_TRIGGER_14) == FIFO_TRIGGER_14) {
			SET(csc->sc_hwflags, COM_HW_FIFO);
			printf(": ns16550a, working fifo\n");
		} else
			printf(": ns16550, broken fifo\n");
	} else
		printf(": ns8250 or ns16450, no fifo\n");
	bus_space_write_1(iot, ioh, com_fifo, 0);

	/* disable interrupts */
	bus_space_write_1(iot, ioh, com_ier, 0);
	bus_space_write_1(iot, ioh, com_mcr, 0);

	if (amiga_ttyspl < (PSL_S|PSL_IPL5)) {
		printf("%s: raising amiga_ttyspl from 0x%x to 0x%x\n",
		    csc->sc_dev.dv_xname, amiga_ttyspl, PSL_S|PSL_IPL5);
		amiga_ttyspl = PSL_S|PSL_IPL5;
	}
	sc->sc_isr.isr_intr = comintr;
	sc->sc_isr.isr_arg = csc;
	sc->sc_isr.isr_ipl = 5;
	add_isr(&sc->sc_isr);

#ifdef KGDB
	if (kgdb_dev == makedev(commajor, unit)) {
		if (ISSET(csc->sc_hwflags, COM_HW_CONSOLE))
			kgdb_dev = -1;	/* can't debug over console port */
		else {
			cominit(iot, ioh, kgdb_rate);
			if (kgdb_debug_init) {
				/*
				 * Print prefix of device name,
				 * let kgdb_connect print the rest.
				 */
				printf("%s: ", csc->sc_dev.dv_xname);
				kgdb_connect(1);
			} else
				printf("%s: kgdb enabled\n",
				    csc->sc_dev.dv_xname);
		}
	}
#endif

	/* XXX maybe move up some? */
	if (ISSET(csc->sc_hwflags, COM_HW_CONSOLE))
		printf("%s: console\n", csc->sc_dev.dv_xname);
#else /* __OpenBSD__ */
	struct comsupio_softc *sc = (void *)self;
	struct com_softc *csc = &sc->sc_com;
	int iobase;
	bus_space_tag_t iot;
	struct supio_attach_args *supa = aux;

	/*
	 * We're living on a superio chip.
	 */
	iobase = csc->sc_iobase = supa->supio_iobase;
	iot = csc->sc_iot = supa->supio_iot;
        if (iobase != comconsaddr) {
                if (bus_space_map(iot, iobase, COM_NPORTS, 0, &csc->sc_ioh))
			panic("comattach: io mapping failed");
	} else
                csc->sc_ioh = comconsioh;

	printf(" port 0x%x", iobase);
	com_attach_subr(csc);

	if (amiga_ttyspl < (PSL_S|PSL_IPL5)) {
		printf("%s: raising amiga_ttyspl from 0x%x to 0x%x\n",
		    csc->sc_dev.dv_xname, amiga_ttyspl, PSL_S|PSL_IPL5);
		amiga_ttyspl = PSL_S|PSL_IPL5;
	}
	sc->sc_isr.isr_intr = comintr;
	sc->sc_isr.isr_arg = csc;
	sc->sc_isr.isr_ipl = 5;
	add_isr(&sc->sc_isr);

	/*
	 * Shutdown hook for buggy BIOSs that don't recognize the UART
	 * without a disabled FIFO.
	 */
	if (shutdownhook_establish(com_supio_cleanup, csc) == NULL)
		panic("comsupio: could not establish shutdown hook");
#endif /* __OpenBSD__ */
}

void
com_supio_cleanup(arg)
	void *arg;
{
	struct com_softc *sc = arg;

	if (ISSET(sc->sc_hwflags, COM_HW_FIFO))
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, com_fifo, 0);
}
