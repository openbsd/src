/*	$NetBSD: cia.c,v 1.5.4.1 1996/06/10 00:02:39 cgd Exp $	*/

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

struct cfattach cia_ca = {
	sizeof(struct cia_softc), ciamatch, ciaattach,
};

struct cfdriver cia_cd = {
	NULL, "cia", DV_DULL,
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
	if (strcmp(ca->ca_name, cia_cd.cd_name) != 0)
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

        cia_bus_io_init(&ccp->cc_bc, ccp);
        cia_bus_mem_init(&ccp->cc_bc, ccp);
        cia_pci_init(&ccp->cc_pc, ccp);

	ccp->cc_hae_mem = REGVAL(CIA_CSR_HAE_MEM);
	ccp->cc_hae_io = REGVAL(CIA_CSR_HAE_IO);
}

void
ciaattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct confargs *ca = aux;
	struct cia_softc *sc = (struct cia_softc *)self;
	struct cia_config *ccp;
	struct pcibus_attach_args pba;

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
		pci_kn20aa_pickintr(ccp);
#ifdef EVCNT_COUNTERS
		evcnt_attach(self, "intr", &kn20aa_intr_evcnt);
#endif
		break;
#endif
	default:
		panic("ciaattach: shouldn't be here, really...");
	}

	pba.pba_busname = "pci";
	pba.pba_bc = &ccp->cc_bc;
	pba.pba_pc = &ccp->cc_pc;
	pba.pba_bus = 0;
	config_found(self, &pba, ciaprint);
}

static int
ciaprint(aux, pnp)
	void *aux;
	char *pnp;
{
	register struct pcibus_attach_args *pba = aux;

	/* only PCIs can attach to CIAs; easy. */
	if (pnp)
		printf("%s at %s", pba->pba_busname, pnp);
	printf(" bus %d", pba->pba_bus);
	return (UNCONF);
}
