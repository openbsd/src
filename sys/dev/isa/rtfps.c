/*	$OpenBSD: rtfps.c,v 1.19 2002/03/14 01:26:56 millert Exp $       */
/*	$NetBSD: rtfps.c,v 1.27 1996/10/21 22:41:18 thorpej Exp $	*/

/*
 * Copyright (c) 1996 Christopher G. Demetriou.  All rights reserved.
 * Copyright (c) 1995 Charles Hannum.  All rights reserved.
 *
 * This code is derived from public-domain software written by
 * Roland McGrath.
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
 *	This product includes software developed by Charles Hannum.
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
#include <sys/termios.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/isa/isavar.h>
#include <dev/ic/comreg.h>
#include <dev/ic/comvar.h>

#define	NSLAVES	4

struct rtfps_softc {
	struct device sc_dev;
	void *sc_ih;

	bus_space_tag_t sc_iot;
	int sc_iobase;
	int sc_irqport;
	bus_space_handle_t sc_irqioh;

	int sc_alive;			/* mask of slave units attached */
	void *sc_slaves[NSLAVES];	/* com device unit numbers */
	bus_space_handle_t sc_slaveioh[NSLAVES];
};

int rtfpsprobe(struct device *, void *, void *);
void rtfpsattach(struct device *, struct device *, void *);
int rtfpsintr(void *);
int rtfpsprint(void *, const char *);

struct cfattach rtfps_ca = {
	sizeof(struct rtfps_softc), rtfpsprobe, rtfpsattach
};

struct cfdriver rtfps_cd = {
	NULL, "rtfps", DV_TTY
};

int
rtfpsprobe(parent, self, aux)
	struct device *parent;
	void *self;
	void *aux;
{
	struct isa_attach_args *ia = aux;
	int iobase = ia->ia_iobase;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	int i, rv = 1;

	/*
	 * Do the normal com probe for the first UART and assume
	 * its presence, and the ability to map the other UARTS,
	 * means there is a multiport board there.
	 * XXX Needs more robustness.
	 */

	/* if the first port is in use as console, then it. */
	if (iobase == comconsaddr && !comconsattached)
		goto checkmappings;

	if (bus_space_map(iot, iobase, COM_NPORTS, 0, &ioh)) {
		rv = 0;
		goto out;
	}
	rv = comprobe1(iot, ioh);
	bus_space_unmap(iot, ioh, COM_NPORTS);
	if (rv == 0)
		goto out;

checkmappings:
	for (i = 1; i < NSLAVES; i++) {
		iobase += COM_NPORTS;

		if (iobase == comconsaddr && !comconsattached)
			continue;

		if (bus_space_map(iot, iobase, COM_NPORTS, 0, &ioh)) {
			rv = 0;
			goto out;
		}
		bus_space_unmap(iot, ioh, COM_NPORTS);
	}

out:
	if (rv)
		ia->ia_iosize = NSLAVES * COM_NPORTS;
	return (rv);
}

int
rtfpsprint(aux, pnp)
	void *aux;
	const char *pnp;
{
	struct commulti_attach_args *ca = aux;

	if (pnp)
		printf("com at %s", pnp);
	printf(" slave %d", ca->ca_slave);
	return (UNCONF);
}

void
rtfpsattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct rtfps_softc *sc = (void *)self;
	struct isa_attach_args *ia = aux;
	struct commulti_attach_args ca;
	static int irqport[] = {
		IOBASEUNK, IOBASEUNK, IOBASEUNK, IOBASEUNK,
		IOBASEUNK, IOBASEUNK, IOBASEUNK, IOBASEUNK,
		IOBASEUNK,     0x2f2,     0x6f2,     0x6f3,
		IOBASEUNK, IOBASEUNK, IOBASEUNK, IOBASEUNK
	};
	bus_space_tag_t iot = ia->ia_iot;
	int i;

	sc->sc_iot = ia->ia_iot;
	sc->sc_iobase = ia->ia_iobase;

	if (ia->ia_irq >= 16 || irqport[ia->ia_irq] == IOBASEUNK)
		panic("rtfpsattach: invalid irq");
	sc->sc_irqport = irqport[ia->ia_irq];

	for (i = 0; i < NSLAVES; i++)
		if (bus_space_map(iot, sc->sc_iobase + i * COM_NPORTS,
		    COM_NPORTS, 0, &sc->sc_slaveioh[i]))
			panic("rtfpsattach: couldn't map slave %d", i);
	if (bus_space_map(iot, sc->sc_irqport, 1, 0, &sc->sc_irqioh))
		panic("rtfpsattach: couldn't map irq port at 0x%x",
		    sc->sc_irqport);

	bus_space_write_1(iot, sc->sc_irqioh, 0, 0);

	printf("\n");

	for (i = 0; i < NSLAVES; i++) {
		ca.ca_slave = i;
		ca.ca_iot = sc->sc_iot;
		ca.ca_ioh = sc->sc_slaveioh[i];
		ca.ca_iobase = sc->sc_iobase + i * COM_NPORTS;
		ca.ca_noien = 0;

		sc->sc_slaves[i] = config_found(self, &ca, rtfpsprint);
		if (sc->sc_slaves[i] != NULL)
			sc->sc_alive |= 1 << i;
	}

	sc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq, IST_EDGE,
	    IPL_TTY, rtfpsintr, sc, sc->sc_dev.dv_xname);
}

int
rtfpsintr(arg)
	void *arg;
{
	struct rtfps_softc *sc = arg;
	bus_space_tag_t iot = sc->sc_iot;
	int alive = sc->sc_alive;

	bus_space_write_1(iot, sc->sc_irqioh, 0, 0);

#define	TRY(n) \
	if (alive & (1 << (n))) \
		comintr(sc->sc_slaves[n]);
	TRY(0);
	TRY(1);
	TRY(2);
	TRY(3);
#undef TRY

	return (1);
}
