/* $OpenBSD: pcppi.c,v 1.1 1999/01/02 00:02:44 niklas Exp $ */
/* $NetBSD: pcppi.c,v 1.1 1998/04/15 20:26:18 drochner Exp $ */

/*
 * Copyright (c) 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/device.h>
#include <sys/errno.h>

#include <machine/bus.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <dev/isa/pcppireg.h>
#include <dev/isa/pcppivar.h>

#include <dev/ic/i8253reg.h>

struct pcppi_softc {
	struct device sc_dv;

	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ppi_ioh, sc_pit1_ioh;

	int sc_bellactive, sc_bellpitch;
	int sc_slp;
};

#define __BROKEN_INDIRECT_CONFIG /* XXX */
#ifdef __BROKEN_INDIRECT_CONFIG
int	pcppi_match __P((struct device *, void *, void *));
#else
int	pcppi_match __P((struct device *, struct cfdata *, void *));
#endif
void	pcppi_attach __P((struct device *, struct device *, void *));

struct cfattach pcppi_ca = {
	sizeof(struct pcppi_softc), pcppi_match, pcppi_attach,
};

struct cfdriver pcppi_cd = {
	NULL, "pcppi", DV_DULL
};

static void pcppi_bell_stop __P((void*));

#define PCPPIPRI (PZERO - 1)

int
pcppi_match(parent, match, aux)
	struct device *parent;
#ifdef __BROKEN_INDIRECT_CONFIG
	void *match;
#else
	struct cfdata *match;
#endif
	void *aux;
{
	struct isa_attach_args *ia = aux;
	bus_space_handle_t ppi_ioh, pit1_ioh;
	int have_pit1, have_ppi, rv;
	u_int8_t v, nv;

	/* If values are hardwired to something that they can't be, punt. */
	if ((ia->ia_iobase != IOBASEUNK && ia->ia_iobase != IO_PPI) ||
	    ia->ia_maddr != MADDRUNK || ia->ia_msize != 0 ||
	    ia->ia_irq != IRQUNK || ia->ia_drq != DRQUNK)
		return (0);

	rv = 0;
	have_pit1 = have_ppi = 0;

	if (bus_space_map(ia->ia_iot, IO_TIMER1, 4, 0, &pit1_ioh))
		goto lose;
	have_pit1 = 1;
	if (bus_space_map(ia->ia_iot, IO_PPI, 1, 0, &ppi_ioh))
		goto lose;
	have_ppi = 1;

	/*
	 * Check for existence of PPI.  Realistically, this is either going to
	 * be here or nothing is going to be here.
	 *
	 * We don't want to have any chance of changing speaker output (which
	 * this test might, if it crashes in the middle, or something;
	 * normally it's be to quick to produce anthing audible), but
	 * many "combo chip" mock-PPI's don't seem to support the top bit
	 * of Port B as a settable bit.  The bottom bit has to be settable,
	 * since the speaker driver hardware still uses it.
	 */
	v = bus_space_read_1(ia->ia_iot, ppi_ioh, 0);		/* XXX */
	bus_space_write_1(ia->ia_iot, ppi_ioh, 0, v ^ 0x01);	/* XXX */
	nv = bus_space_read_1(ia->ia_iot, ppi_ioh, 0);		/* XXX */
	if (((nv ^ v) & 0x01) == 0x01)
		rv = 1;
	bus_space_write_1(ia->ia_iot, ppi_ioh, 0, v);		/* XXX */
	nv = bus_space_read_1(ia->ia_iot, ppi_ioh, 0);		/* XXX */
	if (((nv ^ v) & 0x01) != 0x00) {
		rv = 0;
		goto lose;
	}

	/*
	 * We assume that the programmable interval timer is there.
	 */

lose:
	if (have_pit1)
		bus_space_unmap(ia->ia_iot, pit1_ioh, 4);
	if (have_ppi)
		bus_space_unmap(ia->ia_iot, ppi_ioh, 1);
	if (rv) {
		ia->ia_iobase = IO_PPI;
		ia->ia_iosize = 0x1;
		ia->ia_msize = 0x0;
	}
	return (rv);
}

void
pcppi_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct pcppi_softc *sc = (struct pcppi_softc *)self;
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot;
	struct pcppi_attach_args pa;

	sc->sc_iot = iot = ia->ia_iot;

	if (bus_space_map(iot, IO_TIMER1, 4, 0, &sc->sc_pit1_ioh) ||
	    bus_space_map(iot, IO_PPI, 1, 0, &sc->sc_ppi_ioh))
		panic("pcppi_attach: couldn't map");

	printf("\n");

	sc->sc_bellactive = sc->sc_bellpitch = sc->sc_slp = 0;

	pa.pa_cookie = sc;
	while (config_found(self, &pa, 0));
}

void
pcppi_bell(self, pitch, period, slp)
	pcppi_tag_t self;
	int pitch, period;
	int slp;
{
	struct pcppi_softc *sc = self;
	int s1, s2;

	s1 = spltty(); /* ??? */
	if (sc->sc_bellactive) {
		untimeout(pcppi_bell_stop, sc);
		if (sc->sc_slp)
			wakeup(pcppi_bell_stop);
	}
	if (pitch == 0 || period == 0) {
		pcppi_bell_stop(sc);
		sc->sc_bellpitch = 0;
		splx(s1);
		return;
	}
	if (!sc->sc_bellactive || sc->sc_bellpitch != pitch) {
		s2 = splhigh();
		bus_space_write_1(sc->sc_iot, sc->sc_pit1_ioh, TIMER_MODE,
		    TIMER_SEL2 | TIMER_16BIT | TIMER_SQWAVE);
		bus_space_write_1(sc->sc_iot, sc->sc_pit1_ioh, TIMER_CNTR2,
		    TIMER_DIV(pitch) % 256);
		bus_space_write_1(sc->sc_iot, sc->sc_pit1_ioh, TIMER_CNTR2,
		    TIMER_DIV(pitch) / 256);
		splx(s2);
		/* enable speaker */
		bus_space_write_1(sc->sc_iot, sc->sc_ppi_ioh, 0,
			bus_space_read_1(sc->sc_iot, sc->sc_ppi_ioh, 0)
			| PIT_SPKR);
	}
	sc->sc_bellpitch = pitch;

	sc->sc_bellactive = 1;
	timeout(pcppi_bell_stop, sc, period);
	if (slp) {
		sc->sc_slp = 1;
		tsleep(pcppi_bell_stop, PCPPIPRI | PCATCH, "bell", 0);
		sc->sc_slp = 0;
	}
	splx(s1);
}

static void
pcppi_bell_stop(arg)
	void *arg;
{
	struct pcppi_softc *sc = arg;
	int s;

	s = spltty(); /* ??? */
	/* disable bell */
	bus_space_write_1(sc->sc_iot, sc->sc_ppi_ioh, 0,
			  bus_space_read_1(sc->sc_iot, sc->sc_ppi_ioh, 0)
			  & ~PIT_SPKR);
	sc->sc_bellactive = 0;
	if (sc->sc_slp)
		wakeup(pcppi_bell_stop);
	splx(s);
}
