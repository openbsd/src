/*	$NetBSD: lca.c,v 1.1 1995/11/23 02:37:38 cgd Exp $	*/

/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Jeffrey Hsu
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
#include <alpha/pci/lcareg.h>
#include <alpha/pci/lcavar.h>

int	lcamatch __P((struct device *, void *, void *));
void	lcaattach __P((struct device *, struct device *, void *));

struct cfdriver lcacd = {
	NULL, "lca", lcamatch, lcaattach, DV_DULL,
	    sizeof(struct lca_softc)
};

static int	lcaprint __P((void *, char *pnp));

/* There can be only one. */
int lcafound;
struct lca_config lca_configuration;

int
lcamatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct cfdata *cf = match;
	struct confargs *ca = aux;

	/* Make sure that we're looking for a LCA. */
	if (strcmp(ca->ca_name, lcacd.cd_name) != 0)
		return (0);

	if (lcafound)
		return (0);

	return (1);
}

/*
 * Set up the chipset's function pointers.
 */
void
lca_init(lcp)
	struct lca_config *lcp;
{

	/*
	 * Can't set up SGMAP data here; can be called before malloc().
	 */

	lcp->lc_conffns = &lca_conf_fns;
	lcp->lc_confarg = lcp;
	lcp->lc_dmafns = &lca_dma_fns;
	lcp->lc_dmaarg = lcp;
	/* Interrupt routines set up in 'attach' */
	lcp->lc_memfns = &lca_mem_fns;
	lcp->lc_memarg = lcp;
	lcp->lc_piofns = &lca_pio_fns;
	lcp->lc_pioarg = lcp;

/*
printf("lca_init: before IOC_HAE=0x%x\n", REGVAL(LCA_IOC_HAE));
	REGVAL(LCA_IOC_HAE) = 0; */

	REGVAL(LCA_IOC_CONF) = 0;

	/* Turn off DMA window enables in Window Base Registers */
/*	REGVAL(LCA_IOC_W_BASE0) = 0;
	REGVAL(LCA_IOC_W_BASE1) = 0; */
	wbflush();
}

#ifdef notdef
void
lca_init_sgmap(lcp)
	struct lca_config *lcp;
{

	/* XXX */
	lcp->lc_sgmap = malloc(1024 * 8, M_DEVBUF, M_WAITOK);
	bzero(lcp->lc_sgmap, 1024 * 8);		/* clear all entries. */

	REGVAL(LCA_IOC_W_BASE0) = 0;
	wbflush();

	/* Set up Translated Base Register 1; translate to sybBus addr 0. */
	/* check size against APEC XXX JH */
        REGVAL(LCA_IOC_T_BASE_0) = vtophys(lcp->lc_sgmap) >> 1;

        /* Set up PCI mask register 1; map 8MB space. */
        REGVAL(LCA_IOC_W_MASK0) = 0x00700000;

        /* Enable window 1; from PCI address 8MB, direct mapped. */
        REGVAL(LCA_IOC_W_BASE0) = 0x300800000;
        wbflush();
}
#endif

void
lcaattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct confargs *ca = aux;
	struct lca_softc *sc = (struct lca_softc *)self;
	struct lca_config *lcp;
	struct pci_attach_args pa;

	/* note that we've attached the chipset; can't have 2 LCAs. */
	/* Um, not sure about this.  XXX JH */
	lcafound = 1;

	/*
	 * set up the chipset's info; done once at console init time
	 * (maybe), but doesn't hurt to do twice.
	 */
	lcp = sc->sc_lcp = &lca_configuration;
	lca_init(lcp);
#ifdef notdef
	lca_init_sgmap(lcp);
#endif

	/* XXX print chipset information */
	printf("\n");

	switch (hwrpb->rpb_type) {
#if defined(DEC_AXPPCI_33)
	case ST_DEC_AXPPCI_33:
		pci_axppci_33_pickintr(lcp->lc_conffns, lcp->lc_confarg,
		    lcp->lc_piofns, lcp->lc_pioarg,
		    &lcp->lc_intrfns, &lcp->lc_intrarg);
		break;
#endif
	default:
		panic("lcaattach: shouldn't be here, really...");
	}

	pa.pa_bus = 0;
	pa.pa_maxdev = 13;
	pa.pa_burstlog2 = 8;

	pa.pa_conffns = lcp->lc_conffns;
	pa.pa_confarg = lcp->lc_confarg;
	pa.pa_dmafns = lcp->lc_dmafns;
	pa.pa_dmaarg = lcp->lc_dmaarg;
	pa.pa_intrfns = lcp->lc_intrfns;
	pa.pa_intrarg = lcp->lc_intrarg;
	pa.pa_memfns = lcp->lc_memfns;
	pa.pa_memarg = lcp->lc_memarg;
	pa.pa_piofns = lcp->lc_piofns;
	pa.pa_pioarg = lcp->lc_pioarg;

	config_found(self, &pa, lcaprint);
}

static int
lcaprint(aux, pnp)
	void *aux;
	char *pnp;
{
        register struct pci_attach_args *pa = aux;

	/* what does this do?  XXX JH */
	/* only PCIs can attach to LCAes; easy. */
	if (pnp)
		printf("pci at %s", pnp);
	printf(" bus %d", pa->pa_bus);
	return (UNCONF);
}
