/*	$OpenBSD: com_ioc.c,v 1.4 2009/04/12 17:56:58 miod Exp $ */

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

#include <sgi/pci/iocvar.h>

int	com_ioc_probe(struct device *, void *, void *);
void	com_ioc_attach(struct device *, struct device *, void *);

struct cfattach com_ioc_ca = {
	sizeof(struct com_softc), com_ioc_probe, com_ioc_attach
};

int
com_ioc_probe(struct device *parent, void *match, void *aux)
{
	struct ioc_attach_args *iaa = aux;
	bus_space_tag_t iot = iaa->iaa_memt;
	bus_space_handle_t ioh;
	int rv = 0, console;

	console = iot->bus_base + iaa->iaa_base ==
	    comconsiot->bus_base + comconsaddr;

	/* if it's in use as console, it's there. */
	if (!(console && !comconsattached)) {
		bus_space_map(iot, iaa->iaa_base, COM_NPORTS, 0, &ioh);
		rv = comprobe1(iot, ioh);
		bus_space_unmap(iot, ioh, COM_NPORTS);
	} else
		rv = 1;

	return rv;
}

void
com_ioc_attach(struct device *parent, struct device *self, void *aux)
{
	struct com_softc *sc = (void *)self;
	struct ioc_attach_args *iaa = aux;
	bus_space_handle_t ioh;
	int console;

	console = iaa->iaa_memt->bus_base + iaa->iaa_base ==
	    comconsiot->bus_base + comconsaddr;

	sc->sc_hwflags = 0;
	sc->sc_swflags = 0;
	sc->sc_iobase = iaa->iaa_base;
	sc->sc_frequency = 22000000 / 3;
	sc->sc_iot = iaa->iaa_memt;

	/* if it's in use as console, it's there. */
	if (!(console && !comconsattached)) {
		if (bus_space_map(sc->sc_iot, sc->sc_iobase, COM_NPORTS, 0,
		    &ioh)) {
			printf(": can't map registers\n");
			return;
		}
	} else {
		ioh = comconsioh;
		if (comcnattach(sc->sc_iot, sc->sc_iobase, TTYDEF_SPEED,
		    sc->sc_frequency, (TTYDEF_CFLAG & ~(CSIZE | PARENB)) | CS8))
			panic("can't setup serial console");
	}

	sc->sc_ioh = ioh;

	com_attach_subr(sc);

	ioc_intr_establish(parent, iaa->iaa_dev, IPL_TTY, comintr,
	    (void *)sc, sc->sc_dev.dv_xname);
}
