/*	$NetBSD: sio.c,v 1.2 1995/08/03 01:17:25 cgd Exp $	*/

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

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <machine/autoconf.h>

int	siomatch __P((struct device *, void *, void *));
void	sioattach __P((struct device *, struct device *, void *));

struct cfdriver siocd = {
	NULL, "sio", siomatch, sioattach, DV_DULL, sizeof(struct device)
};

static int	sioprint __P((void *, char *pnp));

int
siomatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct cfdata *cf = match;
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_INTEL ||
	    PCI_PRODUCT(pa->pa_id) != PCI_PRODUCT_INTEL_SIO)
		return (0);

	return (1);
}

void
sioattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct pci_attach_args *pa = aux;
	struct confargs nca;
	u_int rev;
	char *type;

	rev = pa->pa_class & 0xff;
	if (rev < 3) {
		type = "I";
		/* XXX PCI IRQ MAPPING FUNCTION */
	} else {
		type = "II";
		/* XXX PCI IRQ MAPPING FUNCTION */
	}
	printf(": Saturn %s PCI->ISA bridge (revision 0x%x)\n", type, rev);
	if (rev < 3)
		printf("%s: WARNING: SIO I SUPPORT UNTESTED\n",
		    parent->dv_xname);

	/* attach the ISA bus that hangs off of it... */
	nca.ca_name = "isa";
	nca.ca_slot = 0;
	nca.ca_offset = 0;
	nca.ca_bus = NULL;
	if (!config_found(self, &nca, sioprint))
		panic("sioattach: couldn't attach ISA bus");
}

static int
sioprint(aux, pnp)
	void *aux;
	char *pnp;
{
        register struct confargs *ca = aux;

        if (pnp)
                printf("%s at %s", ca->ca_name, pnp);
        return (UNCONF);
}
