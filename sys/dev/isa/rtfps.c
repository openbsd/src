/*	$OpenBSD: rtfps.c,v 1.7 1996/04/18 23:47:45 niklas Exp $       */
/*	$NetBSD: rtfps.c,v 1.17 1996/03/10 09:01:28 cgd Exp $	*/

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
#include <sys/device.h>

#include <machine/bus.h>

#include <dev/isa/isavar.h>
#include <dev/isa/comreg.h>
#include <dev/isa/comvar.h>

#define	NSLAVES	4

struct rtfps_softc {
	struct device sc_dev;
	void *sc_ih;

	bus_chipset_tag_t sc_bc;
	int sc_iobase;
	int sc_irqport;
	bus_io_handle_t sc_irqioh;

	int sc_alive;			/* mask of slave units attached */
	void *sc_slaves[NSLAVES];	/* com device unit numbers */
	bus_io_handle_t sc_slaveioh[NSLAVES];
};

int rtfpsprobe();
void rtfpsattach();
int rtfpsintr __P((void *));

struct cfdriver rtfpscd = {
	NULL, "rtfps", rtfpsprobe, rtfpsattach, DV_TTY, sizeof(struct rtfps_softc)
};

int
rtfpsprobe(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct isa_attach_args *ia = aux;
	int iobase = ia->ia_iobase;
	bus_chipset_tag_t bc = ia->ia_bc;
	bus_io_handle_t ioh;
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

	if (bus_io_map(bc, iobase, COM_NPORTS, &ioh)) {
		rv = 0;
		goto out;
	}
	rv = comprobe1(bc, ioh, iobase);
	bus_io_unmap(bc, ioh, COM_NPORTS);
	if (rv == 0)
		goto out;

checkmappings:
	for (i = 1; i < NSLAVES; i++) {
		iobase += COM_NPORTS;

		if (iobase == comconsaddr && !comconsattached)
			continue;

		if (bus_io_map(bc, iobase, COM_NPORTS, &ioh)) {
			rv = 0;
			goto out;
		}
		bus_io_unmap(bc, ioh, COM_NPORTS);
	}

out:
	if (rv)
		ia->ia_iosize = NSLAVES * COM_NPORTS;
	return (rv);
}

int
rtfpsprint(aux, pnp)
	void *aux;
	char *pnp;
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
	struct isa_attach_args isa;
	static int irqport[] = {
		IOBASEUNK, IOBASEUNK, IOBASEUNK, IOBASEUNK,
		IOBASEUNK, IOBASEUNK, IOBASEUNK, IOBASEUNK,
		IOBASEUNK,     0x2f2,     0x6f2,     0x6f3,
		IOBASEUNK, IOBASEUNK, IOBASEUNK, IOBASEUNK
	};
	int i, subunit;

	sc->sc_bc = ia->ia_bc;
	sc->sc_iobase = ia->ia_iobase;

	if (ia->ia_irq >= 16 || irqport[ia->ia_irq] == IOBASEUNK)
		panic("rtfpsattach: invalid irq");
	sc->sc_irqport = irqport[ia->ia_irq];

	for (i = 0; i < NSLAVES; i++)
		if (bus_io_map(bc, sc->sc_iobase + i * COM_NPORTS, COM_NPORTS,
		    &sc->sc_slaveioh[i]))
			panic("rtfpsattach: couldn't map slave %d", i);
	if (bus_io_map(bc, sc->sc_irqport, 1, &sc->sc_irqioh))
		panic("rtfpsattach: couldn't map irq port at 0x%x\n",
		    sc->sc_irqport);

	bus_io_write_1(bc, sc->sc_irqioh, 0, 0);

	printf("\n");

	for (i = 0; i < NSLAVES; i++) {
		struct cfdata *match;

		ca.ca_slave = i;
		ca.ca_bc = sc->sc_bc;
		ca.ca_ioh = sc->sc_slaveioh[i];
		ca.ca_iobase = sc->sc_iobase + i * COM_NPORTS;
		ca.ca_noien = 0;

		/* mimic config_found(), but with special functionality */
		if ((match = config_search(NULL, self, &ca)) != NULL) {
			subunit = match->cf_unit; /* can change if unit == * */
			config_attach(self, match, &ca, rtfpsprint);
			sc->sc_slaves[i] = match->cf_driver->cd_devs[subunit];
			sc->sc_alive |= 1 << i;
		} else {
			rtfpsprint(&ca, self->dv_xname);
			printf(" not configured\n");
		}
	}

	sc->sc_ih = isa_intr_establish(ia->ia_irq, IST_EDGE, IPL_TTY,
	    rtfpsintr, sc, sc->sc_dev.dv_xname);
}

int
rtfpsintr(arg)
	void *arg;
{
	struct rtfps_softc *sc = arg;
	bus_chipset_tag_t bc = sc->sc_bc;
	int alive = sc->sc_alive;

	bus_io_write_1(bc, sc->sc_irqioh, 0, 0);

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
