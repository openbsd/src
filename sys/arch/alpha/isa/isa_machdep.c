/*	$NetBSD: isa_machdep.c,v 1.2 1995/08/03 01:23:08 cgd Exp $	*/

/*
 * Copyright (c) 1995 Carnegie-Mellon University.
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
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/device.h>

#include <machine/autoconf.h>
#include <machine/pio.h>
#include <machine/rpb.h>
#include <machine/vmparam.h>
#include <machine/pte.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <alpha/isa/isa_intr.h>
#include <alpha/isa/isa_dma.h>

int isamatch __P((struct device *, void *, void *));
void isaattach __P((struct device *, struct device *, void *));

struct cfdriver isacd = {
	NULL, "isa", isamatch, isaattach, DV_DULL, sizeof(struct isa_softc), 1
};

int
isamatch(parent, cfdata, aux)
	struct device *parent;
	void *cfdata, *aux;
{
	struct cfdata *cf = cfdata;
	struct confargs *ca = aux;

#if 0 /* XXX -- Assume that it's valid if unit number OK */
	/* It can only occur on the mainbus. */
	if (ca->ca_bus->ab_type != BUS_MAIN)
		return (0);

	/* Make sure that we're looking for this type of device. */
	if (!BUS_MATCHNAME(ca, "isa"))
		return (0);
#endif /* XXX */

	/* See if the unit number is valid. */
	switch (hwrpb->rpb_type) {
#if defined(DEC_2100_A50)
	case ST_DEC_2100_A50:
		if (cf->cf_unit > 0)
			return (0);
		break;
#endif
	default:
		return (0);
	}

	return (1);
}

void
isaattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct isa_softc *sc = (struct isa_softc *)self;

	printf("\n");

	TAILQ_INIT(&sc->sc_subdevs);

	/* XXX set up ISA DMA controllers? */

	config_scan(isascan, self);
}

void *
isa_intr_establish(irq, type, level, ih_fun, ih_arg)
	int irq;
	isa_intrtype type;
	isa_intrlevel level;
	int (*ih_fun)(void *);
	void *ih_arg;
{

	return (*isa_intr_fcns->isa_intr_establish)(irq, type, level,
	    ih_fun, ih_arg);
}

void
isa_intr_disestablish(handler)
	void *handler;
{

	(*isa_intr_fcns->isa_intr_disestablish)(handler);
}

int
isadma_map(addr, size, mappings, flags)
	caddr_t addr;
	vm_size_t size;
	vm_offset_t *mappings;
	int flags;
{

	(*isadma_fcns->isadma_map)(addr, size, mappings, flags);
}

void
isadma_unmap(addr, size, nmappings, mappings)
	caddr_t addr;
	vm_size_t size;
	int nmappings;
	vm_offset_t *mappings;
{

	(*isadma_fcns->isadma_unmap)(addr, size, nmappings, mappings);
}

void
isadma_copytobuf(addr, size, nmappings, mappings)
	caddr_t addr;
	vm_size_t size;
	int nmappings;
	vm_offset_t *mappings;
{

	(*isadma_fcns->isadma_copytobuf)(addr, size, nmappings, mappings);
}

void
isadma_copyfrombuf(addr, size, nmappings, mappings)
	caddr_t addr;
	vm_size_t size;
	int nmappings;
	vm_offset_t *mappings;
{

	(*isadma_fcns->isadma_copyfrombuf)(addr, size, nmappings, mappings);
}
