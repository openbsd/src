/*	$OpenBSD: isa.c,v 1.23 1997/12/21 14:41:23 downsj Exp $	*/
/*	$NetBSD: isa.c,v 1.85 1996/05/14 00:31:04 thorpej Exp $	*/

/*-
 * Copyright (c) 1993, 1994 Charles Hannum.  All rights reserved.
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
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/extent.h>

#include <machine/intr.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

int isamatch __P((struct device *, void *, void *));
void isaattach __P((struct device *, struct device *, void *));

struct cfattach isa_ca = {
	sizeof(struct isa_softc), isamatch, isaattach
};

struct cfdriver isa_cd = {
	NULL, "isa", DV_DULL, 1
};

int
isamatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct cfdata *cf = match;
	struct isabus_attach_args *iba = aux;

	if (strcmp(iba->iba_busname, cf->cf_driver->cd_name))
		return (0);

	/* XXX check other indicators */

        return (1);
}

void
isaattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct isa_softc *sc = (struct isa_softc *)self;
	struct isabus_attach_args *iba = aux;

	isa_attach_hook(parent, self, iba);
	printf("\n");

	sc->sc_iot = iba->iba_iot;
	sc->sc_memt = iba->iba_memt;
	sc->sc_ic = iba->iba_ic;

	/*
	 * Map port 0x84, which causes a 1.25us delay when read.
	 * We do this now, since several drivers need it.
	 * XXX this port doesn't exist on all ISA busses...
	 */
	if (bus_space_map(sc->sc_iot, 0x84, 1, 0, &sc->sc_delaybah))
		panic("isaattach: can't map `delay port'");	/* XXX */

	TAILQ_INIT(&sc->sc_subdevs);
	config_scan(isascan, self);
}

int
isaprint(aux, isa)
	void *aux;
	const char *isa;
{
	struct isa_attach_args *ia = aux;

	if (ia->ia_iosize)
		printf(" port 0x%x", ia->ia_iobase);
	if (ia->ia_iosize > 1)
		printf("-0x%x", ia->ia_iobase + ia->ia_iosize - 1);
	if (ia->ia_msize)
		printf(" iomem 0x%x", ia->ia_maddr);
	if (ia->ia_msize > 1)
		printf("-0x%x", ia->ia_maddr + ia->ia_msize - 1);
	if (ia->ia_irq != IRQUNK)
		printf(" irq %d", ia->ia_irq);
	if (ia->ia_drq != DRQUNK)
		printf(" drq %d", ia->ia_drq);
	return (UNCONF);
}

void
isascan(parent, match)
	struct device *parent;
	void *match;
{
	struct isa_softc *sc = (struct isa_softc *)parent;
	struct device *dev = match;
	struct cfdata *cf = dev->dv_cfdata;
	struct isa_attach_args ia;

	ia.ia_iot = sc->sc_iot;
	ia.ia_memt = sc->sc_memt;
	ia.ia_ic = sc->sc_ic;
	ia.ia_iobase = cf->cf_loc[0];
	ia.ia_iosize = 0x666;
	ia.ia_maddr = cf->cf_loc[2];
	ia.ia_msize = cf->cf_loc[3];
	ia.ia_irq = cf->cf_loc[4] == 2 ? 9 : cf->cf_loc[4];
	ia.ia_drq = cf->cf_loc[5];
	ia.ia_delaybah = sc->sc_delaybah;

	if (cf->cf_fstate == FSTATE_STAR) {
		struct isa_attach_args ia2 = ia;

		while ((*cf->cf_attach->ca_match)(parent, dev, &ia2) > 0) {
			if (ia2.ia_iosize == 0x666) {
				printf("%s: iosize not repaired by driver\n",
				    sc->sc_dev.dv_xname);
				ia2.ia_iosize = 0;
			}
			config_attach(parent, dev, &ia2, isaprint);
			dev = config_make_softc(parent, cf);
			ia2 = ia;
		}
		free(dev, M_DEVBUF);
		return;
	}

	if ((*cf->cf_attach->ca_match)(parent, dev, &ia) > 0)
		config_attach(parent, dev, &ia, isaprint);
	else
		free(dev, M_DEVBUF);
}

char *
isa_intr_typename(type)
	int type;
{

	switch (type) {
        case IST_NONE:
		return ("none");
        case IST_PULSE:
		return ("pulsed");
        case IST_EDGE:
		return ("edge-triggered");
        case IST_LEVEL:
		return ("level-triggered");
	default:
		panic("isa_intr_typename: invalid type %d", type);
	}
}
