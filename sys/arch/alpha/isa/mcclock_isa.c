/*	$OpenBSD: mcclock_isa.c,v 1.2 1996/07/29 22:59:47 niklas Exp $	*/
/*	$NetBSD: mcclock_isa.c,v 1.2 1996/04/17 22:22:46 cgd Exp $	*/

/*
 * Copyright (c) 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/bus.h>

#include <alpha/alpha/clockvar.h>
#include <alpha/alpha/mcclockvar.h>
#include <dev/ic/mc146818reg.h>
#include <dev/isa/isavar.h>

struct mcclock_isa_softc {
	struct mcclock_softc	sc_mcclock;

	bus_chipset_tag_t	sc_bc;
	bus_io_handle_t		sc_ioh;
};

int	mcclock_isa_match __P((struct device *, void *, void *));
void	mcclock_isa_attach __P((struct device *, struct device *, void *));

struct cfattach mcclock_isa_ca = {
	sizeof (struct mcclock_isa_softc), mcclock_isa_match,
	    mcclock_isa_attach, 
};

void	mcclock_isa_write __P((struct mcclock_softc *, u_int, u_int));
u_int	mcclock_isa_read __P((struct mcclock_softc *, u_int));

const struct mcclock_busfns mcclock_isa_busfns = {
	mcclock_isa_write, mcclock_isa_read,
};

int
mcclock_isa_match(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct isa_attach_args *ia = aux;

	if (ia->ia_iobase != 0x70 && ia->ia_iobase != -1)
		return (0);

	ia->ia_iobase = 0x70;		/* XXX */
	ia->ia_iosize = 2;		/* XXX */
	ia->ia_msize = 0;

	return (1);
}

void
mcclock_isa_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct isa_attach_args *ia = aux;
	struct mcclock_isa_softc *sc = (struct mcclock_isa_softc *)self;

	sc->sc_bc = ia->ia_bc;
	if (bus_io_map(sc->sc_bc, ia->ia_iobase, ia->ia_iosize, &sc->sc_ioh))
		panic("mcclock_isa_attach: couldn't map clock I/O space");

	mcclock_attach(&sc->sc_mcclock, &mcclock_isa_busfns);
}

void
mcclock_isa_write(mcsc, reg, datum)
	struct mcclock_softc *mcsc;
	u_int reg, datum;
{
	struct mcclock_isa_softc *sc = (struct mcclock_isa_softc *)mcsc;
	bus_chipset_tag_t bc = sc->sc_bc;
	bus_io_handle_t ioh = sc->sc_ioh;

	bus_io_write_1(bc, ioh, 0, reg);
	bus_io_write_1(bc, ioh, 1, datum);
}

u_int
mcclock_isa_read(mcsc, reg)
	struct mcclock_softc *mcsc;
	u_int reg;
{
	struct mcclock_isa_softc *sc = (struct mcclock_isa_softc *)mcsc;
	bus_chipset_tag_t bc = sc->sc_bc;
	bus_io_handle_t ioh = sc->sc_ioh;

	bus_io_write_1(bc, ioh, 0, reg);
	return bus_io_read_1(bc, ioh, 1);
}
