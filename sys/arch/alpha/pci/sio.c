/*	$NetBSD: sio.c,v 1.3 1995/11/23 02:38:16 cgd Exp $	*/

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
#include <sys/device.h>

#include <dev/isa/isavar.h>
#include <dev/eisa/eisavar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <alpha/pci/siovar.h>

int	siomatch __P((struct device *, void *, void *));
void	sioattach __P((struct device *, struct device *, void *));

struct cfdriver siocd = {
	NULL, "sio", siomatch, sioattach, DV_DULL, sizeof(struct device)
};

int	pcebmatch __P((struct device *, void *, void *));

struct cfdriver pcebcd = {
	NULL, "pceb", pcebmatch, sioattach, DV_DULL, sizeof(struct device)
};

static int	sioprint __P((void *, char *pnp));

int
siomatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct cfdata *cf = match;
	struct pcidev_attach_args *pda = aux;

	if (PCI_VENDOR(pda->pda_id) != PCI_VENDOR_INTEL ||
	    PCI_PRODUCT(pda->pda_id) != PCI_PRODUCT_INTEL_SIO)
		return (0);

	return (1);
}

int
pcebmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct cfdata *cf = match;
	struct pcidev_attach_args *pda = aux;

	if (PCI_VENDOR(pda->pda_id) != PCI_VENDOR_INTEL ||
	    PCI_PRODUCT(pda->pda_id) != PCI_PRODUCT_INTEL_PCEB)
		return (0);

	return (1);
}

void
sioattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct pcidev_attach_args *pda = aux;
	struct isa_attach_args ia;
	struct eisa_attach_args ea;
	int sio, haseisa;
	char devinfo[256];

	sio = (PCI_PRODUCT(pda->pda_id) == PCI_PRODUCT_INTEL_SIO);
	haseisa = (PCI_PRODUCT(pda->pda_id) == PCI_PRODUCT_INTEL_PCEB);

	pci_devinfo(pda->pda_id, pda->pda_class, 0, devinfo);
	printf(": %s (rev. 0x%02x)\n", devinfo,
	    PCI_REVISION(pda->pda_class));

	if (sio) {
		pci_revision_t rev;

		rev = PCI_REVISION(pda->pda_class);
		
		if (rev < 3)
			printf("%s: WARNING: SIO I SUPPORT UNTESTED\n",
			    self->dv_xname);
	}

#ifdef EVCNT_COUNTERS
	evcnt_attach(self, "intr", &sio_intr_evcnt);
#endif

	ia.ia_bus = BUS_ISA;
	ia.ia_dmafns = pda->pda_dmafns;
	ia.ia_dmaarg = pda->pda_dmaarg;
	ia.ia_intrfns = &sio_isa_intr_fns;
	ia.ia_intrarg = NULL;			/* XXX needs nothing */
	ia.ia_memfns = pda->pda_memfns;
	ia.ia_memarg = pda->pda_memarg;
	ia.ia_piofns = pda->pda_piofns;
	ia.ia_pioarg = pda->pda_pioarg;
	config_found(self, &ia, sioprint);

	if (haseisa) {
		ea.ea_bus = BUS_EISA;
		ea.ea_dmafns = pda->pda_dmafns;
		ea.ea_dmaarg = pda->pda_dmaarg;
		ea.ea_intrfns = &sio_isa_intr_fns;
		ea.ea_intrarg = NULL;		/* XXX needs nothing */
		ea.ea_memfns = pda->pda_memfns;
		ea.ea_memarg = pda->pda_memarg;
		ea.ea_piofns = pda->pda_piofns;
		ea.ea_pioarg = pda->pda_pioarg;
		config_found(self, &ea, sioprint);
	}
}

static int
sioprint(aux, pnp)
	void *aux;
	char *pnp;
{
        register struct isa_attach_args *ia = aux;

	/*
	 * XXX Assumes that the first fields of 'struct isa_attach_args'
	 * XXX and 'struct eisa_attach_args' are the same.
	 */

        if (pnp)
                printf("%s at %s", isa_bustype_name(ia->ia_bus), pnp);
        return (UNCONF);
}
