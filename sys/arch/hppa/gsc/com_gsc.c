/*	$OpenBSD: com_gsc.c,v 1.2 1998/11/23 03:16:28 mickey Exp $	*/

/*
 * Copyright (c) 1998 Michael Shalayeff
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
 *	This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/tty.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/iomod.h>
#include <machine/autoconf.h>

#include <dev/ic/comreg.h>
#include <dev/ic/comvar.h>

#include <hppa/dev/cpudevs.h>
#include <hppa/gsc/gscbusvar.h>

int	com_gsc_probe __P((struct device *, void *, void *));
void	com_gsc_attach __P((struct device *, struct device *, void *));


struct cfattach com_gsc_ca = {
	sizeof(struct com_softc), com_gsc_probe, com_gsc_attach
};

int
com_gsc_probe(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	register struct confargs *ca = aux;
	bus_space_handle_t ioh;
	int rv;

	if (ca->ca_type.iodc_type != HPPA_TYPE_FIO ||
	    (ca->ca_type.iodc_sv_model != HPPA_FIO_GRS232 &&
	     (ca->ca_type.iodc_sv_model != HPPA_FIO_RS232)))
		return 0;

	if (bus_space_map(ca->ca_iot, ca->ca_hpa, IOMOD_HPASIZE, 0, &ioh))
		return 0;

	rv = comprobe1(ca->ca_iot, ioh | IOMOD_DEVOFFSET);
	bus_space_unmap(ca->ca_iot, ioh, IOMOD_HPASIZE);
	return rv;
}

void
com_gsc_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	register struct com_softc *sc = (void *)self;
	register struct gsc_attach_args *ga = aux;

	sc->sc_hwflags = 0;
	sc->sc_swflags = 0;
	sc->sc_iobase = (bus_addr_t)ga->ga_hpa;
	sc->sc_iot = HPPA_BUS_TAG_SET_BYTE(ga->ga_iot);

	if (bus_space_map(sc->sc_iot, sc->sc_iobase, IOMOD_HPASIZE,
			  0, &sc->sc_ioh))
		panic ("com_gsc_attach: mapping io space");

	sc->sc_ih = gsc_intr_establish(ga->ga_ic, IPL_TTY, comintr, sc,
				       sc->sc_dev.dv_xname);

	sc->sc_ioh |= IOMOD_DEVOFFSET;
	com_attach_subr(sc);
}

