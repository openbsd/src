/*	$NetBSD: cia.c,v 1.1 1995/11/23 02:37:24 cgd Exp $	*/

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
#include <sys/malloc.h>
#include <sys/device.h>
#include <vm/vm.h>

#include <machine/autoconf.h>
#include <machine/rpb.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <alpha/pci/ciareg.h>
#include <alpha/pci/ciavar.h>
#if defined(DEC_KN20AA)
#include <alpha/pci/pci_kn20aa.h>
#endif

int	ciamatch __P((struct device *, void *, void *));
void	ciaattach __P((struct device *, struct device *, void *));

struct cfdriver ciacd = {
	NULL, "cia", ciamatch, ciaattach, DV_DULL,
	    sizeof(struct cia_softc)
};

static int	ciaprint __P((void *, char *pnp));

#define	REGVAL(r)	(*(int32_t *)phystok0seg(r))

/* There can be only one. */
int ciafound;
struct cia_config cia_configuration;

int
ciamatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct cfdata *cf = match;
	struct confargs *ca = aux;

	/* Make sure that we're looking for a CIA. */
	if (strcmp(ca->ca_name, ciacd.cd_name) != 0)
		return (0);

	if (ciafound)
		return (0);

	return (1);
}

/*
 * Set up the chipset's function pointers.
 */
void
cia_init(ccp)
	struct cia_config *ccp;
{

	/*
	 * Can't set up SGMAP data here; can be called before malloc().
	 */

	ccp->cc_conffns = &cia_conf_fns;
	ccp->cc_confarg = ccp;
	ccp->cc_dmafns = &cia_dma_fns;
	ccp->cc_dmaarg = ccp;
	/* Interrupt routines set up in 'attach' */
	ccp->cc_memfns = &cia_mem_fns;
	ccp->cc_memarg = ccp;
	ccp->cc_piofns = &cia_pio_fns;
	ccp->cc_pioarg = ccp;
}

void
ciaattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct confargs *ca = aux;
	struct cia_softc *sc = (struct cia_softc *)self;
	struct cia_config *ccp;
	struct pci_attach_args pa;

	/* note that we've attached the chipset; can't have 2 CIAs. */
	ciafound = 1;

	/*
	 * set up the chipset's info; done once at console init time
	 * (maybe), but doesn't hurt to do twice.
	 */
	ccp = sc->sc_ccp = &cia_configuration;
	cia_init(ccp);

	/* XXX print chipset information */
	printf("\n");

	switch (hwrpb->rpb_type) {
#if defined(DEC_KN20AA)
	case ST_DEC_KN20AA:
		pci_kn20aa_pickintr(ccp->cc_conffns, ccp->cc_confarg,
		    ccp->cc_piofns, ccp->cc_pioarg,
		    &ccp->cc_intrfns, &ccp->cc_intrarg);
#ifdef EVCNT_COUNTERS
		evcnt_attach(self, "intr", &kn20aa_intr_evcnt);
#endif
		break;
#endif
	default:
		panic("ciaattach: shouldn't be here, really...");
	}

	pa.pa_bus = 0;
	pa.pa_maxdev = 32;
	pa.pa_burstlog2 = 8;

	pa.pa_conffns = ccp->cc_conffns;
	pa.pa_confarg = ccp->cc_confarg;
	pa.pa_dmafns = ccp->cc_dmafns;
	pa.pa_dmaarg = ccp->cc_dmaarg;
	pa.pa_intrfns = ccp->cc_intrfns;
	pa.pa_intrarg = ccp->cc_intrarg;
	pa.pa_memfns = ccp->cc_memfns;
	pa.pa_memarg = ccp->cc_memarg;
	pa.pa_piofns = ccp->cc_piofns;
	pa.pa_pioarg = ccp->cc_pioarg;

	config_found(self, &pa, ciaprint);
}

static int
ciaprint(aux, pnp)
	void *aux;
	char *pnp;
{
	register struct pci_attach_args *pa = aux;

	/* only PCIs can attach to CIAs; easy. */
	if (pnp)
		printf("pci at %s", pnp);
	printf(" bus %d", pa->pa_bus);
	return (UNCONF);
}
