/*	$NetBSD: apecs.c,v 1.4 1995/11/23 02:37:11 cgd Exp $	*/

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
#include <alpha/pci/apecsreg.h>
#include <alpha/pci/apecsvar.h>

int	apecsmatch __P((struct device *, void *, void *));
void	apecsattach __P((struct device *, struct device *, void *));

struct cfdriver apecscd = {
	NULL, "apecs", apecsmatch, apecsattach, DV_DULL,
	    sizeof(struct apecs_softc)
};

static int	apecsprint __P((void *, char *pnp));

#define	REGVAL(r)	(*(int32_t *)phystok0seg(r))

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
	if (strcmp(ca->ca_name, apecscd.cd_name) != 0)
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

	acp->ac_conffns = &apecs_conf_fns;
	acp->ac_confarg = acp;
	acp->ac_dmafns = &apecs_dma_fns;
	acp->ac_dmaarg = acp;
	/* Interrupt routines set up in 'attach' */
	acp->ac_memfns = &apecs_mem_fns;
	acp->ac_memarg = acp;
	acp->ac_piofns = &apecs_pio_fns;
	acp->ac_pioarg = acp;

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
	struct confargs *ca = aux;
	struct apecs_softc *sc = (struct apecs_softc *)self;
	struct apecs_config *acp;
	struct pci_attach_args pa;

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
		pci_2100_a50_pickintr(acp->ac_conffns, acp->ac_confarg,
		    acp->ac_piofns, acp->ac_pioarg,
		    &acp->ac_intrfns, &acp->ac_intrarg);
		break;
#endif
	default:
		panic("apecsattach: shouldn't be here, really...");
	}

	pa.pa_bus = 0;
	pa.pa_maxdev = 32;
	pa.pa_burstlog2 = 8;

	pa.pa_conffns = acp->ac_conffns;
	pa.pa_confarg = acp->ac_confarg;
	pa.pa_dmafns = acp->ac_dmafns;
	pa.pa_dmaarg = acp->ac_dmaarg;
	pa.pa_intrfns = acp->ac_intrfns;
	pa.pa_intrarg = acp->ac_intrarg;
	pa.pa_memfns = acp->ac_memfns;
	pa.pa_memarg = acp->ac_memarg;
	pa.pa_piofns = acp->ac_piofns;
	pa.pa_pioarg = acp->ac_pioarg;

	config_found(self, &pa, apecsprint);
}

static int
apecsprint(aux, pnp)
	void *aux;
	char *pnp;
{
        register struct pci_attach_args *pa = aux;

	/* only PCIs can attach to APECSes; easy. */
	if (pnp)
		printf("pci at %s", pnp);
	printf(" bus %d", pa->pa_bus);
	return (UNCONF);
}
