/*	$OpenBSD: cy_isa.c,v 1.8 2002/09/14 15:00:03 art Exp $	*/

/*
 * cy_isa.c
 *
 * Driver for Cyclades Cyclom-8/16/32 multiport serial cards
 * (currently not tested with Cyclom-32 cards)
 *
 * Timo Rossi, 1996
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/bus.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isareg.h>

#include <dev/ic/cd1400reg.h>
#include <dev/ic/cyreg.h>

static int cy_isa_probe(struct device *, void *, void *);
void cy_isa_attach(struct device *, struct device *, void *);

struct cfattach cy_isa_ca = {
	sizeof(struct cy_softc), cy_isa_probe, cy_isa_attach
};

int
cy_isa_probe(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	int card = ((struct device *)match)->dv_unit;
	struct isa_attach_args *ia = aux;
	bus_space_tag_t memt;
	bus_space_handle_t memh;
	int ret;

	if (ia->ia_irq == IRQUNK) {
		printf("cy%d error: interrupt not defined\n", card);
		return (0);
	}

	memt = ia->ia_memt;
	if (bus_space_map(memt, ia->ia_maddr, 0x2000, 0, &memh) != 0)
		return (0);

	ret = cy_probe_common(memt, memh, CY_BUSTYPE_ISA);
	bus_space_unmap(memt, memh, 0x2000);
	if (ret == 0)
		return (0);

	ia->ia_iosize = 0;
	ia->ia_msize = 0x2000;
	return (1);
}

void
cy_isa_attach(parent, self, aux)
        struct device *parent, *self;
        void *aux;
{
	struct cy_softc *sc = (struct cy_softc *)self;
	struct isa_attach_args *ia = aux;

	sc->sc_bustype = CY_BUSTYPE_ISA;
	sc->sc_memt = ia->ia_memt;

	if (bus_space_map(ia->ia_memt, ia->ia_maddr, 0x2000, 0,
	    &sc->sc_memh) != 0)
		return;

	sc->sc_nr_cd1400s = cy_probe_common(sc->sc_memt, sc->sc_memh,
	    CY_BUSTYPE_ISA);

	cy_attach(parent, self);

	sc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq,
	    IST_EDGE, IPL_TTY, cy_intr, sc, sc->sc_dev.dv_xname);

	if (sc->sc_ih == NULL)
		panic("cy: couldn't establish interrupt");
}
