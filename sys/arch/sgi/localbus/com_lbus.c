/*	$OpenBSD: com_lbus.c,v 1.5 2008/02/20 18:46:20 miod Exp $ */

/*
 * Copyright (c) 2001-2004 Opsycon AB  (www.opsycon.se / www.opsycon.com)
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/tty.h>

#include <machine/autoconf.h>
#include <mips64/archtype.h>
#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/ic/comreg.h>
#include <dev/ic/comvar.h>
#include <dev/ic/ns16550reg.h>

#include <sgi/localbus/macebus.h>

int	com_macebus_probe(struct device *, void *, void *);
void	com_macebus_attach(struct device *, struct device *, void *);


struct cfattach com_macebus_ca = {
	sizeof(struct com_softc), com_macebus_probe, com_macebus_attach
};

extern void com_raisedtr(void *);
extern struct timeout compoll_to;

int
com_macebus_probe(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	struct confargs *ca = aux;
	bus_addr_t iobase, rv = 0;

	iobase = (bus_addr_t)ca->ca_baseaddr;
	if (iobase) {
		iot = ca->ca_iot;
		/* if it's in use as console, it's there. */
		if (!(iobase == comconsaddr && !comconsattached)) {
			bus_space_map(iot, iobase, COM_NPORTS, 0, &ioh);
			rv = comprobe1(iot, ioh);
		} else {
			rv = 1;
		}
	}
	return (rv);
}

void
com_macebus_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct com_softc *sc = (void *)self;
	int intr;
	bus_addr_t iobase;
	bus_space_handle_t ioh;
	struct confargs *ca = aux;

	sc->sc_hwflags = 0;
	sc->sc_swflags = 0;
	iobase = (bus_addr_t)ca->ca_baseaddr;
	intr = ca->ca_intr;
	sc->sc_iobase = iobase;
	sc->sc_frequency = sys_config.cons_baudclk;	/* XXX */

	sc->sc_iot = ca->ca_iot;

	/* if it's in use as console, it's there. */
	if (!(iobase == comconsaddr && !comconsattached)) {
		if (bus_space_map(sc->sc_iot, iobase, COM_NPORTS, 0, &ioh)) {
			panic("unexpected bus_space_map failure");
		}
	}
	else {
		ioh = comconsioh;
	}

	sc->sc_ioh = ioh;

	com_attach_subr(sc);

	/* Enable IE pin. Some boards are not edge sensitive */
	SET(sc->sc_mcr, MCR_IENABLE);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, com_mcr, sc->sc_mcr);

	macebus_intr_establish(NULL, intr, IST_EDGE, IPL_TTY,
				comintr, (void *)sc, sc->sc_dev.dv_xname);

}
