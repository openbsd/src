/*	$OpenBSD: com_iof.c,v 1.5 2009/10/16 00:15:46 miod Exp $	*/

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

#include <dev/ic/comreg.h>
#include <dev/ic/comvar.h>
#include <dev/ic/ns16550reg.h>

#include <sgi/pci/iofvar.h>

int	com_iof_probe(struct device *, void *, void *);
void	com_iof_attach(struct device *, struct device *, void *);

struct cfattach com_iof_ca = {
	sizeof(struct com_softc), com_iof_probe, com_iof_attach
};

extern struct cfdriver com_cd;

int
com_iof_probe(struct device *parent, void *match, void *aux)
{
	struct cfdata *cf = match;
	struct iof_attach_args *iaa = aux;
	bus_space_tag_t iot = iaa->iaa_memt;
	bus_space_handle_t ioh;
	int rv = 0, console;

	if (strcmp(iaa->iaa_name, com_cd.cd_name) != 0)
		return 0;

	console = iaa->iaa_memh + iaa->iaa_base ==
	    comconsiot->bus_base + comconsaddr;

	/* if it's in use as console, it's there. */
	if (!(console && !comconsattached)) {
		if (bus_space_subregion(iot, iaa->iaa_memh,
		    iaa->iaa_base, COM_NPORTS, &ioh) == 0)
			rv = comprobe1(iot, ioh);
	} else
		rv = 1;

	/* make a config stanza with exact locators match over a generic line */
	if (cf->cf_loc[0] != -1)
		rv += rv;

	return rv;
}

void
com_iof_attach(struct device *parent, struct device *self, void *aux)
{
	struct com_softc *sc = (void *)self;
	struct iof_attach_args *iaa = aux;
	bus_space_handle_t ioh;
	int console;

	console = iaa->iaa_memh + iaa->iaa_base ==
	    comconsiot->bus_base + comconsaddr;

	sc->sc_hwflags = 0;
	sc->sc_swflags = 0;
	sc->sc_frequency = iaa->iaa_clock;

	/* if it's in use as console, it's there. */
	if (!(console && !comconsattached)) {
		sc->sc_iot = iaa->iaa_memt;
		sc->sc_iobase = iaa->iaa_base;

		if (bus_space_subregion(iaa->iaa_memt, iaa->iaa_memh,
		    iaa->iaa_base, COM_NPORTS, &ioh) != 0) {
			printf(": can't map registers\n");
			return;
		}
	} else {
		/*
		 * If we are the console, reuse the existing bus_space
		 * information, so that comcnattach() invokes bus_space_map()
		 * with correct parameters.
		 */
		sc->sc_iot = comconsiot;
		sc->sc_iobase = comconsaddr;

		if (comcnattach(sc->sc_iot, sc->sc_iobase, comconsrate,
		    sc->sc_frequency, (TTYDEF_CFLAG & ~(CSIZE | PARENB)) | CS8))
			panic("can't setup serial console");
		ioh = comconsioh;
	}

	sc->sc_ioh = ioh;

	com_attach_subr(sc);

	iof_intr_establish(parent, iaa->iaa_dev, IPL_TTY, comintr,
	    (void *)sc, sc->sc_dev.dv_xname);
}
