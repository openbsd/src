/*	$OpenBSD: com_obio.c,v 1.5 2008/06/26 05:42:09 ray Exp $	*/
/*	$NetBSD: com_obio.c,v 1.9 2005/12/11 12:17:09 christos Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas <matt@3am-software.com>.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/termios.h>

#include <machine/bus.h>

#include <arm/xscale/i80321var.h>
#include <armish/dev/obiovar.h>

#include <dev/ic/comreg.h>
#include <dev/ic/comvar.h>

struct com_obio_softc {
	struct com_softc sc_com;

	void *sc_ih;
};

int	com_obio_match(struct device *, void *, void *);
void	com_obio_attach(struct device *, struct device *, void *);

struct cfattach com_obio_ca = {
	sizeof(struct com_obio_softc), com_obio_match, com_obio_attach
};

struct cfdriver com_obio_cd = {
	NULL, "com_obio", DV_DULL
};

int com_irq_override = -1;

int
com_obio_match(struct device *parent, void *cf, void *aux)
{
	struct obio_attach_args *oba = aux;

	/* if the irq does not match, do not attach */
	if (com_irq_override != -1)
		oba->oba_irq = com_irq_override;

	/* We take it on faith that the device is there. */
	return (1);
}


void
com_obio_attach(struct device *parent, struct device *self, void *aux)
{
	struct obio_attach_args *oba = aux;
	struct com_obio_softc *osc = (void *) self;
	struct com_softc *sc = &osc->sc_com;
	int error;

	sc->sc_iot = oba->oba_st;
	sc->sc_iobase = oba->oba_addr;
	sc->sc_frequency = COM_FREQ;
/* 	sc->sc_hwflags = COM_HW_NO_TXPRELOAD; */
	sc->sc_hwflags = 0;
	error = bus_space_map(sc->sc_iot, oba->oba_addr, 8, 0, &sc->sc_ioh);

	if (error) {
		printf(": failed to map registers: %d\n", error);
		return;
	}

	com_attach_subr(sc);
	osc->sc_ih = i80321_intr_establish(oba->oba_irq, IPL_TTY,
	    comintr, sc, sc->sc_dev.dv_xname);
	if (osc->sc_ih == NULL)
		printf("%s: unable to establish interrupt at CPLD irq %d\n",
		    sc->sc_dev.dv_xname, oba->oba_irq);
}
