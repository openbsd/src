/*	$OpenBSD: com_lbus.c,v 1.9 2009/03/29 21:53:52 sthen Exp $ */

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
#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/ic/comreg.h>
#include <dev/ic/comvar.h>
#include <dev/ic/ns16550reg.h>

#include <mips64/archtype.h>

#include <sgi/localbus/macebus.h>

int	com_macebus_probe(struct device *, void *, void *);
void	com_macebus_attach(struct device *, struct device *, void *);

struct cfattach com_macebus_ca = {
	sizeof(struct com_softc), com_macebus_probe, com_macebus_attach
};

int
com_macebus_probe(struct device *parent, void *match, void *aux)
{
	struct confargs *ca = aux;
	bus_space_handle_t ioh;
	int rv;

	/* If it's in use as the console, then it's there. */
	if (ca->ca_baseaddr == comconsaddr && !comconsattached)
		return (1);

	if (bus_space_map(ca->ca_iot, ca->ca_baseaddr, COM_NPORTS, 0, &ioh))
		return (0);

	rv = comprobe1(ca->ca_iot, ioh);
	bus_space_unmap(ca->ca_iot, ioh, COM_NPORTS);

	return rv;
}

void
com_macebus_attach(struct device *parent, struct device *self, void *aux)
{
	struct com_softc *sc = (void *)self;
	struct confargs *ca = aux;

	sc->sc_iot = ca->ca_iot;
	sc->sc_hwflags = 0;
	sc->sc_swflags = 0;
	sc->sc_iobase = ca->ca_baseaddr;
	sc->sc_frequency = 1843200;

	/* If it's in use as the console, then it's there. */
	if (ca->ca_baseaddr == comconsaddr && !comconsattached) {
		if (comcnattach(sc->sc_iot, sc->sc_iobase, TTYDEF_SPEED,
		    sc->sc_frequency, (TTYDEF_CFLAG & ~(CSIZE | PARENB)) | CS8))
			panic("failed to setup serial console!");
		sc->sc_ioh = comconsioh;
	} else {
		if (bus_space_map(sc->sc_iot, ca->ca_baseaddr, COM_NPORTS, 0,
		    &sc->sc_ioh)) {
			printf(": can't map i/o space\n");
			return;
		}
	}

	com_attach_subr(sc);

	macebus_intr_establish(NULL, ca->ca_intr, IST_EDGE, IPL_TTY, comintr,
	    (void *)sc, sc->sc_dev.dv_xname);
}
