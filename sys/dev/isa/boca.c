/*	$NetBSD: boca.c,v 1.5 1995/06/26 04:08:06 cgd Exp $	*/

/*
 * Copyright (c) 1995 Charles Hannum.  All rights reserved.
 *
 * This code is derived from public-domain software written by
 * Roland McGrath, and information provided by David Muir Sharnoff.
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

struct boca_softc {
	struct device sc_dev;
	void *sc_ih;

	int sc_iobase;
	int sc_alive;		/* mask of slave units attached */
	void *sc_slaves[8];	/* com device unit numbers */
};

int bocaprobe();
void bocaattach();
int bocaintr __P((void *));

struct cfdriver bocacd = {
	NULL, "boca", bocaprobe, bocaattach, DV_TTY, sizeof(struct boca_softc)
};

int
bocaprobe(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct isa_attach_args *ia = aux;

	/*
	 * Do the normal com probe for the first UART and assume
	 * its presence means there is a multiport board there.
	 * XXX Needs more robustness.
	 */
	ia->ia_iosize = 8 * 8;
	return (comprobe1(ia->ia_iobase));
}

struct boca_attach_args {
	int ba_slave;
};

int
bocasubmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct boca_softc *sc = (void *)parent;
	struct cfdata *cf = match;
	struct isa_attach_args *ia = aux;
	struct boca_attach_args *ba = ia->ia_aux;

	if (cf->cf_loc[0] != -1 && cf->cf_loc[0] != ba->ba_slave)
		return (0);
	return ((*cf->cf_driver->cd_match)(parent, match, ia));
}

int
bocaprint(aux, boca)
	void *aux;
	char *boca;
{
	struct isa_attach_args *ia = aux;
	struct boca_attach_args *ba = ia->ia_aux;

	printf(" slave %d", ba->ba_slave);
}

void
bocaattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct boca_softc *sc = (void *)self;
	struct isa_attach_args *ia = aux;
	struct boca_attach_args ba;
	struct isa_attach_args isa;
	int subunit;

	sc->sc_iobase = ia->ia_iobase;

	printf("\n");

	isa.ia_aux = &ba;
	for (ba.ba_slave = 0; ba.ba_slave < 8; ba.ba_slave++) {
		struct cfdata *cf;
		isa.ia_iobase = sc->sc_iobase + 8 * ba.ba_slave;
		isa.ia_iosize = 0x666;
		isa.ia_irq = IRQUNK;
		isa.ia_drq = DRQUNK;
		isa.ia_msize = 0;
		if ((cf = config_search(bocasubmatch, self, &isa)) != 0) {
			subunit = cf->cf_unit;	/* can change if unit == * */
			config_attach(self, cf, &isa, bocaprint);
			sc->sc_slaves[ba.ba_slave] =
			    cf->cf_driver->cd_devs[subunit];
			sc->sc_alive |= 1 << ba.ba_slave;
		}
	}

	sc->sc_ih = isa_intr_establish(ia->ia_irq, IST_EDGE, IPL_TTY, bocaintr,
	    sc);
}

int
bocaintr(arg)
	void *arg;
{
	struct boca_softc *sc = arg;
	int iobase = sc->sc_iobase;
	int alive = sc->sc_alive;
	int bits;

	bits = inb(iobase | 0x07) & alive;
	if (bits == 0)
		return (0);

	for (;;) {
#define	TRY(n) \
		if (bits & (1 << (n))) \
			comintr(sc->sc_slaves[n]);
		TRY(0);
		TRY(1);
		TRY(2);
		TRY(3);
		TRY(4);
		TRY(5);
		TRY(6);
		TRY(7);
#undef TRY
		bits = inb(iobase | 0x07) & alive;
		if (bits == 0)
			return (1);
 	}
}
