/*	$OpenBSD: com_obio.c,v 1.1 2005/07/17 12:22:42 miod Exp $	*/

/*
 * Copyright (c) 2005, Miodrag Vallat.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Driver for the built-in modem on Tadpole SPARCbooks, which provides a
 * 16C450-compatible interface. Later models apparently are 16550A-compatible
 * (i.e. with a 16 byte FIFO).
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/tty.h>

#include <machine/bus.h>
#include <sparc/sparc/auxioreg.h>

#include <dev/ic/comreg.h>
#include <dev/ic/comvar.h>

int	com_obio_match(struct device *, void *, void *);
void	com_obio_attach(struct device *, struct device *, void *);

struct com_obio_softc {
	struct com_softc	sc_com;
	struct intrhand		sc_ih;
	struct rom_reg		sc_reg;
	int			sc_pwr;
};

struct cfattach com_obio_ca = {
	sizeof(struct com_obio_softc), com_obio_match, com_obio_attach
};

void	com_obio_disable(struct com_softc *);
int	com_obio_enable(struct com_softc *);

int
com_obio_match(struct device *parent, void *match, void *aux)
{
	struct confargs *ca = aux;

	return (strcmp("modem", ca->ca_ra.ra_name) == 0);
}

void
com_obio_attach(struct device *parent, struct device *self, void *aux)
{
	struct confargs *ca = aux;
	struct com_softc *sc = (void *)self;
	struct com_obio_softc *osc = (void *)sc;

	/* build a bus tag */
	osc->sc_reg = ca->ca_ra.ra_reg[0];

#if 0	/* not necessary as we don't compile com console support */
	sc->sc_iobase = -1;
#endif
	sc->sc_iot = &osc->sc_reg;

	if (ca->ca_ra.ra_nreg == 0 || bus_space_map(sc->sc_iot, 0,
	    ca->ca_ra.ra_len, 0, &sc->sc_ioh) != 0) {
		printf(": can't map modem registers\n");
		return;
	}

	osc->sc_ih.ih_fun = comintr;
	osc->sc_ih.ih_arg = sc;
	intr_establish(ca->ca_ra.ra_intr[0].int_pri, &osc->sc_ih, -1,
	    self->dv_xname);
	printf(" pri %d", ca->ca_ra.ra_intr[0].int_pri);

	sc->disable = com_obio_disable;
	sc->enable = com_obio_enable;
	osc->sc_pwr = getpropint(ca->ca_ra.ra_node, "pwr-on-auxio", 0);
	com_obio_enable(sc);
	sc->enabled = 1;

	sc->sc_frequency = COM_FREQ;
	com_attach_subr(sc);
}

void
com_obio_disable(struct com_softc *sc)
{
	struct com_obio_softc *osc = (void *)sc;

	if (osc->sc_pwr) {
		sb_auxregbisc(0, 0, AUXIO_MODEM | AUXIO_MODEM_RESET);
	}
}

int
com_obio_enable(struct com_softc *sc)
{
	struct com_obio_softc *osc = (void *)sc;

	if (osc->sc_pwr) {
		/* enable and reset modem for 20 usec */
		sb_auxregbisc(0, AUXIO_MODEM, AUXIO_MODEM_RESET);
		delay(20);
		/* end reset (active low) */
		sb_auxregbisc(0, AUXIO_MODEM_RESET, 0);
		/* give the device some time to settle */
		delay(100);
	}
	return (0);
}
