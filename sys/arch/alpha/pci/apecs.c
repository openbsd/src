/*	$NetBSD: apecs.c,v 1.7 1996/04/12 06:08:01 cgd Exp $	*/

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
#include <alpha/pci/apecsreg.h>
#include <alpha/pci/apecsvar.h>
#include <alpha/pci/pci_2100_a50.h>

int	apecsmatch __P((struct device *, void *, void *));
void	apecsattach __P((struct device *, struct device *, void *));

struct cfattach apecs_ca = {
	sizeof(struct apecs_softc), apecsmatch, apecsattach,
};

struct cfdriver apecs_cd = {
	NULL, "apecs", DV_DULL,
};

static int	apecsprint __P((void *, char *pnp));

/* There can be only one. */
int apecsfound;
struct apecs_config apecs_configuration;

int
apecsmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct cfdata *cf = match;
	struct confargs *ca = aux;

	/* Make sure that we're looking for an APECS. */
	if (strcmp(ca->ca_name, apecs_cd.cd_name) != 0)
		return (0);

	if (apecsfound)
		return (0);

	return (1);
}

/*
 * Set up the chipset's function pointers.
 */
void
apecs_init(acp)
	struct apecs_config *acp;
{

	acp->ac_comanche_pass2 =
	    (REGVAL(COMANCHE_ED) & COMANCHE_ED_PASS2) != 0;
	acp->ac_memwidth =
	    (REGVAL(COMANCHE_GCR) & COMANCHE_GCR_WIDEMEM) != 0 ? 128 : 64;
	acp->ac_epic_pass2 =
	    (REGVAL(EPIC_DCSR) & EPIC_DCSR_PASS2) != 0;

	/*
	 * Can't set up SGMAP data here; can be called before malloc().
	 */

	apecs_lca_bus_io_init(&acp->ac_bc, acp);
	apecs_lca_bus_mem_init(&acp->ac_bc, acp);
	apecs_pci_init(&acp->ac_pc, acp);

	/* Turn off DMA window enables in PCI Base Reg 1. */
	REGVAL(EPIC_PCI_BASE_1) = 0;
	wbflush();

	/* XXX SGMAP? */
}

void
apecsattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct apecs_softc *sc = (struct apecs_softc *)self;
	struct apecs_config *acp;
	struct pcibus_attach_args pba;

	/* note that we've attached the chipset; can't have 2 APECSes. */
	apecsfound = 1;

	/*
	 * set up the chipset's info; done once at console init time
	 * (maybe), but doesn't hurt to do twice.
	 */
	acp = sc->sc_acp = &apecs_configuration;
	apecs_init(acp);

	/* XXX SGMAP FOO */

	printf(": DECchip %s Core Logic chipset\n",
	    acp->ac_memwidth == 128 ? "21072" : "21071");
	printf("%s: DC21071-CA pass %d, %d-bit memory bus\n",
	    self->dv_xname, acp->ac_comanche_pass2 ? 2 : 1, acp->ac_memwidth);
	printf("%s: DC21071-DA pass %d\n", self->dv_xname,
	    acp->ac_epic_pass2 ? 2 : 1);
	/* XXX print bcache size */

	if (!acp->ac_epic_pass2)
		printf("WARNING: 21071-DA NOT PASS2... NO BETS...\n");

	switch (hwrpb->rpb_type) {
#if defined(DEC_2100_A50)
	case ST_DEC_2100_A50:
		pci_2100_a50_pickintr(acp);
		break;
#endif
	default:
		panic("apecsattach: shouldn't be here, really...");
	}

	pba.pba_busname = "pci";
	pba.pba_bc = &acp->ac_bc;
	pba.pba_pc = &acp->ac_pc;
	pba.pba_bus = 0;
	config_found(self, &pba, apecsprint);
}

static int
apecsprint(aux, pnp)
	void *aux;
	char *pnp;
{
        register struct pcibus_attach_args *pba = aux;

	/* only PCIs can attach to APECSes; easy. */
	if (pnp)
		printf("%s at %s", pba->pba_busname, pnp);
	printf(" bus %d", pba->pba_bus);
	return (UNCONF);
}
