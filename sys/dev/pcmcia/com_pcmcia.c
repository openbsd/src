/*	$OpenBSD: com_pcmcia.c,v 1.3 1998/03/05 14:39:38 niklas Exp $	*/
/*	$NetBSD: com.c,v 1.82.4.1 1996/06/02 09:08:00 mrg Exp $	*/

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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/tty.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/isa/isavar.h>

#include <dev/ic/comreg.h>
#include <dev/ic/comvar.h>
#include <dev/ic/ns16550reg.h>

#include <dev/pcmcia/pcmciavar.h>

/* Macros to clear/set/test flags. */
#define	SET(t, f)	(t) |= (f)
#define	CLR(t, f)	(t) &= ~(f)
#define	ISSET(t, f)	((t) & (f))

int	com_pcmcia_match __P((struct device *, void *, void *));
void	com_pcmcia_attach __P((struct device *, struct device *, void *));
int	com_pcmcia_detach __P((struct device *));

int	com_pcmcia_mod __P((struct pcmcia_link *pc_link, struct device *self,
	    struct pcmcia_conf *pc_cf, struct cfdata *cf));
int	com_pcmcia_isa_attach __P((struct device *, void *, void *,
	    struct pcmcia_link *));
int	com_pcmcia_remove __P((struct pcmcia_link *, struct device *));
int	com_pcmcia_probe __P((struct device *, void *, void *));

struct cfattach com_pcmcia_ca = {
	sizeof(struct com_softc), com_pcmcia_match, com_pcmcia_attach,
	com_pcmcia_detach
};

/* additional setup needed for pcmcia devices */
/* modify config entry */
int 
com_pcmcia_mod(pc_link, self, pc_cf, cf)
    struct pcmcia_link *pc_link;
    struct device *self;
    struct pcmcia_conf *pc_cf; 
    struct cfdata *cf;
{               
	int err; 

	if (!(err = PCMCIA_BUS_CONFIG(pc_link->adapter, pc_link, self,
	    pc_cf, cf))) {
		pc_cf->memwin = 0;
		if (pc_cf->cfgtype == 0) 
		pc_cf->cfgtype = CFGENTRYID; /* determine from ioaddr */
	}
	return err;
}

static struct pcmcia_com {
	struct pcmcia_device pcd;
} pcmcia_com =  {
	{"PCMCIA Modem card", com_pcmcia_mod, com_pcmcia_isa_attach,
	 NULL, com_pcmcia_remove}
};          


struct pcmciadevs pcmcia_com_devs[] = {
	{ "com", 0,
	NULL, "*MODEM*", NULL, NULL,
	NULL, (void *)&pcmcia_com 
	},
	{ "com", 0,
	NULL, NULL, "*MODEM*", NULL,
	NULL, (void *)&pcmcia_com 
	},
	{ "com", 0,
	NULL, NULL, NULL, "*MODEM*",
	NULL, (void *)&pcmcia_com 
	},
	{NULL}
};
#define ncom_pcmcia_devs sizeof(pcmcia_com_devs)/sizeof(pcmcia_com_devs[0])

int
com_pcmcia_match(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	return pcmcia_slave_match(parent, match, aux, pcmcia_com_devs,
	    ncom_pcmcia_devs);
}

int
com_pcmcia_isa_attach(parent, match, aux, pc_link)
	struct device *parent;
	void *match;
	void *aux;
	struct pcmcia_link *pc_link;
{
	struct isa_attach_args *ia = aux;
	struct com_softc *sc = match;
	int rval;

	if ((rval = com_pcmcia_probe(parent, sc->sc_dev.dv_cfdata, ia))) {
		if (ISSET(pc_link->flags, PCMCIA_REATTACH)) {
#ifdef COM_DEBUG
			printf("comreattach, hwflags=%x\n", sc->sc_hwflags);
#endif
			sc->sc_hwflags = COM_HW_REATTACH | (sc->sc_hwflags &
			    (COM_HW_ABSENT_PENDING|COM_HW_CONSOLE));
		} else
			sc->sc_hwflags = 0;
		sc->sc_ic = ia->ia_ic;
	}
	return rval;
}


/*
 * Called by config_detach attempts, shortly after com_pcmcia_remove
 * was called.
 */
int
com_pcmcia_detach(self)
	struct device *self;
{
	struct com_softc *sc = (void *)self;

	if (ISSET(sc->sc_hwflags, COM_HW_ABSENT_PENDING)) {
		/* don't let it really be detached, it is still open */
		return EBUSY;
	}
	return 0;		/* OK! */
}

/*
 * called by pcmcia framework to accept/reject remove attempts.
 * If we return 0, then the detach will proceed.
 */
int
com_pcmcia_remove(pc_link, self)
	struct pcmcia_link *pc_link;
	struct device *self;
{
	struct com_softc *sc = (void *)self;
	struct tty *tp;
	int s;

	if (!sc->sc_tty)
		goto ok;
	tp = sc->sc_tty;

	/* not in use ?  if so, return "OK" */
	if (!ISSET(tp->t_state, TS_ISOPEN) &&
	    !ISSET(tp->t_state, TS_WOPEN)) {
		ttyfree(sc->sc_tty);
		sc->sc_tty = NULL;
    ok:
		isa_intr_disestablish(sc->sc_ic, sc->sc_ih);
		sc->sc_ih = NULL;
		SET(sc->sc_hwflags, COM_HW_ABSENT);
		return 0;		/* OK! */
	}
	/*
	 * Not easily removed.  Put device into a dead state, clean state
	 * as best we can.  notify all waiters.
	 */
	SET(sc->sc_hwflags, COM_HW_ABSENT|COM_HW_ABSENT_PENDING);
#ifdef COM_DEBUG
	printf("pending detach flags %x\n", sc->sc_hwflags);
#endif

	s = spltty();
	com_absent_notify(sc);
	splx(s);

	return 0;
}

#if 0
void
com_pcmcia_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct pcmcia_attach_args *paa = aux;
	
	printf("com_pcmcia_attach %p %p %p\n", parent, self, aux);
	delay(2000000);
	if (!pcmcia_configure(parent, self, paa->paa_link)) {
		struct com_softc *sc = (void *)self;
		sc->sc_hwflags |= COM_HW_ABSENT;
		printf(": not attached\n");
	}
}
#endif

int
com_pcmcia_probe(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int iobase, needioh;
	int rv = 1;
	struct isa_attach_args *ia = aux;

	iot = ia->ia_iot;
	iobase = ia->ia_iobase;
	needioh = 1;

	/* if it's in use as console, it's there. */
	if (iobase == comconsaddr && !comconsattached)
		goto out;

	if (needioh && bus_space_map(iot, iobase, COM_NPORTS, 0, &ioh)) {
		rv = 0;
		goto out;
	}
	rv = comprobe1(iot, ioh);
	if (needioh)
		bus_space_unmap(iot, ioh, COM_NPORTS);

out:
	ia->ia_iosize = COM_NPORTS;
	ia->ia_msize = 0;
	return (rv);
}

void
com_pcmcia_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct com_softc *sc = (void *)self;
	int iobase, irq;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	struct isa_attach_args *ia = aux;

	if (ISSET(sc->sc_hwflags, COM_HW_REATTACH)) {
		int s;
		s = spltty();
		com_absent_notify(sc);
		splx(s);
	} else
	    sc->sc_hwflags = 0;
	sc->sc_swflags = 0;

	/*
	 * We're living on an isa.
	 */
	iobase = ia->ia_iobase;
	iot = ia->ia_iot;
        if (iobase != comconsaddr) {
                if (bus_space_map(iot, iobase, COM_NPORTS, 0, &ioh))
			panic("comattach: io mapping failed");
	} else
                ioh = comconsioh;
	irq = ia->ia_irq;

	sc->sc_iot = iot;
	sc->sc_ioh = ioh;
	sc->sc_iobase = iobase;

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
	bus_space_write_1(iot, ioh, com_fifo,
	    FIFO_ENABLE | FIFO_RCV_RST | FIFO_XMT_RST | FIFO_TRIGGER_14);
	delay(100);
	if (ISSET(bus_space_read_1(iot, ioh, com_iir), IIR_FIFO_MASK) ==
	    IIR_FIFO_MASK) {
		if (ISSET(bus_space_read_1(iot, ioh, com_fifo),
		    FIFO_TRIGGER_14) == FIFO_TRIGGER_14) {
			SET(sc->sc_hwflags, COM_HW_FIFO);
			printf(": ns16550a, working fifo\n");
		} else
			printf(": ns16550, broken fifo\n");
	} else
		printf(": ns8250 or ns16450, no fifo\n");
	bus_space_write_1(iot, ioh, com_fifo, 0);

	/* disable interrupts */
	bus_space_write_1(iot, ioh, com_ier, 0);
	bus_space_write_1(iot, ioh, com_mcr, 0);

	if (irq != IRQUNK) {
		struct isa_attach_args *ia = aux;

		sc->sc_ih = isa_intr_establish(ia->ia_ic, irq,
		    IST_EDGE, IPL_TTY, comintr, sc, sc->sc_dev.dv_xname);
	}

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
