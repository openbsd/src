/*	$OpenBSD: ast.c,v 1.4 1996/03/08 16:42:48 niklas Exp $	*/
/*	$NetBSD: ast.c,v 1.18 1995/06/26 04:08:04 cgd Exp $	*/

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

struct ast_softc {
	struct device sc_dev;
	void *sc_ih;

	int sc_iobase;
	int sc_alive;		/* mask of slave units attached */
	void *sc_slaves[4];	/* com device unit numbers */
};

int astprobe();
void astattach();
int astintr __P((void *));

struct cfdriver astcd = {
	NULL, "ast", astprobe, astattach, DV_TTY, sizeof(struct ast_softc)
};

int
astprobe(parent, self, aux)
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
	return (comprobe1(ia->ia_iobase));
}

struct ast_attach_args {
	int aa_slave;
};

int
astsubmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct ast_softc *sc = (void *)parent;
	struct cfdata *cf = match;
	struct isa_attach_args *ia = aux;
	struct ast_attach_args *aa = ia->ia_aux;

	if (cf->cf_loc[0] != -1 && cf->cf_loc[0] != aa->aa_slave)
		return (0);
	return ((*cf->cf_driver->cd_match)(parent, match, ia));
}

int
astprint(aux, ast)
	void *aux;
	char *ast;
{
	struct isa_attach_args *ia = aux;
	struct ast_attach_args *aa = ia->ia_aux;

	printf(" slave %d", aa->aa_slave);
}

void
astattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct ast_softc *sc = (void *)self;
	struct isa_attach_args *ia = aux;
	struct ast_attach_args aa;
	struct isa_attach_args isa;
	int subunit;

	sc->sc_iobase = ia->ia_iobase;

	/*
	 * Enable the master interrupt.
	 */
	outb(sc->sc_iobase | 0x1f, 0x80);

	printf("\n");

	isa.ia_aux = &aa;
	for (aa.aa_slave = 0; aa.aa_slave < 4; aa.aa_slave++) {
		struct cfdata *cf;
		isa.ia_iobase = sc->sc_iobase + 8 * aa.aa_slave;
		isa.ia_iosize = 0x666;
		isa.ia_irq = IRQUNK;
		isa.ia_drq = DRQUNK;
		isa.ia_msize = 0;
		if ((cf = config_search(astsubmatch, self, &isa)) != 0) {
			subunit = cf->cf_unit;	/* can change if unit == * */
			config_attach(self, cf, &isa, astprint);
			sc->sc_slaves[aa.aa_slave] =
			    cf->cf_driver->cd_devs[subunit];
			sc->sc_alive |= 1 << aa.aa_slave;
		}
	}

	sc->sc_ih = isa_intr_establish(ia->ia_irq, IST_EDGE, IPL_TTY, astintr,
	    sc, sc->sc_dev.dv_xname);
}

int
astintr(arg)
	void *arg;
{
	struct ast_softc *sc = arg;
	int iobase = sc->sc_iobase;
	int alive = sc->sc_alive;
	int bits;

	bits = ~inb(iobase | 0x1f) & alive;
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
#undef TRY
		bits = ~inb(iobase | 0x1f) & alive;
		if (bits == 0)
			return (1);
 	}
}
