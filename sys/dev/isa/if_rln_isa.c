/*	$OpenBSD: if_rln_isa.c,v 1.1 1999/07/31 06:10:04 d Exp $	*/

/*
 * David Leonard <d@openbsd.org>, 1999. Public domain.
 *
 * RangeLAN2 ISA card.
 *
 * The DIP switch settings on the card to set the i/o base address are
 *
 *     addr: 1 2 3 4 5 6 7  ['X' = on = down = towards the red/green leds]
 *	100: - - - - - X -
 *	120: - - X - - X -
 *	140: - - - X - X -
 *	218: X X - - - - X
 *	270: - X X X - - X  [default]
 *	280: - - - - X - X
 *	290: - X - - X - X
 *	298: X X - - X - X
 *	2a0: - - X - X - X
 *	2a8: X - X - X - X
 *	2e0: - - X X X - X
 *	300: - - - - - X X
 *	310: - X - - - X X
 *	358: X X - X - X X
 *	360: - - X X - X X
 *	368: X - X X - X X
 *
 * PnP:
 *	PXM0100 "Symphony Cordless PnP ISA Card"
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/device.h>
#include <sys/queue.h>

#include <net/if.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#include <machine/cpu.h>
#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/ic/rln.h>
#include <dev/ic/rlnvar.h>
#include <dev/ic/rlnreg.h>

#include <dev/isa/isavar.h>

static int rln_isa_probe __P((struct device *, void *, void *));
static void rln_isa_attach __P((struct device *, struct device *, void *));

struct cfattach rln_isa_ca = {
	sizeof(struct rln_softc), rln_isa_probe, rln_isa_attach
};

static const int rln_irq[] = {
	3, 4, 5, 7, 10, 11, 12, 15
};
#define NRLN_IRQ	(sizeof(rln_irq) / sizeof(rln_irq[0]))

static int
rln_isa_probe(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct isa_attach_args *ia = aux;
	struct rln_softc *sc = match;

	/*
	 * The i/o base addr is set by the dip switches
	 * and must be specified in the kernel config.
	 */
	if (ia->ia_iobase == IOBASEUNK)
		return (0);

	/* Attempt a card reset through the io ports */
	sc->sc_iot = ia->ia_iot;
	sc->sc_width = 0;	/* Force width probe */
	if (bus_space_map(sc->sc_iot, ia->ia_iobase, RLN_NPORTS, 0, 
	    &sc->sc_ioh))
		return (0);

	if (rln_reset(sc)) {
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, RLN_NPORTS);
		return (0);
	}

	ia->ia_iosize = RLN_NPORTS;
	return (1);
}

static void
rln_isa_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct rln_softc *sc = (void *)self;
	struct isa_attach_args *ia = aux;
	int irq = ia->ia_irq;
	int mask;
	int i;

	if (irq == IRQUNK) {
		/* Allocate a valid IRQ. */
		mask = 0;
		for (i = 0; i < NRLN_IRQ; i++)
			mask |= (1 << rln_irq[i]);
		if (isa_intr_alloc(ia->ia_ic, mask, IST_EDGE, &irq))
			panic("rln_isa_attach: can't allocate irq");
	} 
#ifdef DIAGNOSTIC
	else {
		/* Check given IRQ is valid. */
		for (i = 0; i < NRLN_IRQ; i++)
			if (irq == rln_irq[i])
				break;
		if (i == NRLN_IRQ)
			printf("rln_isa_probe: using invalid irq %d\n", irq);
	}
#endif

	printf(":");

	sc->sc_ih = isa_intr_establish(ia->ia_ic, irq, IST_EDGE,
	    IPL_NET, rlnintr, sc, sc->sc_dev.dv_xname);
#ifdef DIAGNOSTIC
	if (sc->sc_ih == NULL)
		panic("rln_isa_attach: couldn't establish interrupt");
#endif

	/* Tell the card which IRQ to use. */
	sc->sc_irq = irq;
	sc->sc_width = 0;	/* re-probe width */

	printf("%s: RangeLAN2 7100", sc->sc_dev.dv_xname);
	rlnconfig(sc);
	printf("\n");
}
