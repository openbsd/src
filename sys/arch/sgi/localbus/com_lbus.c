/*	$OpenBSD: com_lbus.c,v 1.4 2004/10/20 12:49:15 pefo Exp $ */

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

int	com_localbus_probe(struct device *, void *, void *);
void	com_localbus_attach(struct device *, struct device *, void *);


struct cfattach com_localbus_ca = {
	sizeof(struct com_softc), com_localbus_probe, com_localbus_attach
};

struct cfattach com_xbow_ca = {
	sizeof(struct com_softc), com_localbus_probe, com_localbus_attach
};

extern void com_raisedtr(void *);
extern struct timeout compoll_to;

int
com_localbus_probe(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	struct cfdata *cf = match;
	struct confargs *ca = aux;
	bus_addr_t iobase, rv = 0;

	/*
	 *  Check if this is our com. If low nibble is 0 match
	 *  against system CLASS. Else a perfect match is checked.
	 */
	if ((ca->ca_sys & 0x000f) == 0) {
		if (ca->ca_sys != (sys_config.system_type & 0xfff0))
			return 0;
	} else if (ca->ca_sys != sys_config.system_type)
		return 0;

	iobase = (bus_addr_t)sys_config.cons_ioaddr[cf->cf_unit];
	if (iobase) {
		iot = sys_config.cons_iot;
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
com_localbus_attach(parent, self, aux)
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
	iobase = (bus_addr_t)sys_config.cons_ioaddr[sc->sc_dev.dv_unit];
	intr = ca->ca_intr;
	sc->sc_iobase = iobase;
	sc->sc_frequency = sys_config.cons_baudclk;

	sc->sc_iot = sys_config.cons_iot;

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

	BUS_INTR_ESTABLISH(ca, NULL, intr, IST_EDGE, IPL_TTY,
				comintr, (void *)sc, sc->sc_dev.dv_xname);

}
