/*	$OpenBSD: rtfps.c,v 1.4 1996/03/08 16:43:12 niklas Exp $       */
/*	$NetBSD: rtfps.c,v 1.14 1995/12/24 02:31:48 mycroft Exp $       */

/*
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

#include <machine/pio.h>

#include <dev/isa/isavar.h>

struct rtfps_softc {
	struct device sc_dev;
	void *sc_ih;

	int sc_iobase;
	int sc_irqport;
	int sc_alive;		/* mask of slave units attached */
	void *sc_slaves[4];	/* com device unit numbers */
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

	/*
	 * Do the normal com probe for the first UART and assume
	 * its presence means there is a multiport board there.
	 * XXX Needs more robustness.
	 */
	ia->ia_iosize = 4 * 8;
	return comprobe1(ia->ia_iobase);
}

struct rtfps_attach_args {
	int ra_slave;
};

int
rtfpssubmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct rtfps_softc *sc = (void *)parent;
	struct cfdata *cf = match;
	struct isa_attach_args *ia = aux;
	struct rtfps_attach_args *ra = ia->ia_aux;

	if (cf->cf_loc[0] != -1 && cf->cf_loc[0] != ra->ra_slave)
		return (0);
	return ((*cf->cf_driver->cd_match)(parent, match, ia));
}

int
rtfpsprint(aux, rtfps)
	void *aux;
	char *rtfps;
{
	struct isa_attach_args *ia = aux;
	struct rtfps_attach_args *ra = ia->ia_aux;

	printf(" slave %d", ra->ra_slave);
}

void
rtfpsattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct rtfps_softc *sc = (void *)self;
	struct isa_attach_args *ia = aux;
	struct rtfps_attach_args ra;
	struct isa_attach_args isa;
	static int irqport[] = {
		IOBASEUNK, IOBASEUNK, IOBASEUNK, IOBASEUNK,
		IOBASEUNK, IOBASEUNK, IOBASEUNK, IOBASEUNK,
		IOBASEUNK,     0x2f2,     0x6f2,     0x6f3,
		IOBASEUNK, IOBASEUNK, IOBASEUNK, IOBASEUNK
	};
	int subunit;

	sc->sc_iobase = ia->ia_iobase;

	if (ia->ia_irq >= 16 || irqport[ia->ia_irq] == IOBASEUNK)
		panic("rtfpsattach: invalid irq");
	sc->sc_irqport = irqport[ia->ia_irq];

	outb(sc->sc_irqport, 0);

	printf("\n");

	isa.ia_aux = &ra;
	for (ra.ra_slave = 0; ra.ra_slave < 4; ra.ra_slave++) {
		struct cfdata *cf;
		isa.ia_iobase = sc->sc_iobase + 8 * ra.ra_slave;
		isa.ia_iosize = 0x666;
		isa.ia_irq = IRQUNK;
		isa.ia_drq = DRQUNK;
		isa.ia_msize = 0;
		if ((cf = config_search(rtfpssubmatch, self, &isa)) != 0) {
			subunit = cf->cf_unit;	/* can change if unit == * */
			config_attach(self, cf, &isa, rtfpsprint);
			sc->sc_slaves[ra.ra_slave] =
			    cf->cf_driver->cd_devs[subunit];
			sc->sc_alive |= 1 << ra.ra_slave;
		}
	}

	sc->sc_ih = isa_intr_establish(ia->ia_irq, IST_EDGE, IPL_TTY, rtfpsintr,
				       sc, sc->sc_dev.dv_xname);
}

int
rtfpsintr(arg)
	void *arg;
{
	struct rtfps_softc *sc = arg;
	int iobase = sc->sc_iobase;
	int alive = sc->sc_alive;

	outb(sc->sc_irqport, 0);

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
