/*	$OpenBSD: pci.c,v 1.1 1996/04/18 23:48:02 niklas Exp $	*/
/*	$NetBSD: pci.c,v 1.15 1996/03/14 04:03:01 cgd Exp $	*/

/*
 * Copyright (c) 1995, 1996 Christopher G. Demetriou.  All rights reserved.
 * Copyright (c) 1994 Charles Hannum.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Charles Hannum.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * PCI bus autoconfiguration.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

int pcimatch __P((struct device *, void *, void *));
void pciattach __P((struct device *, struct device *, void *));

struct cfdriver pcicd = {
	NULL, "pci", pcimatch, pciattach, DV_DULL, sizeof(struct device)
};

int	pciprint __P((void *, char *));
int	pcisubmatch __P((struct device *, void *, void *));

int
pcimatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct cfdata *cf = match;
	struct pcibus_attach_args *pba = aux;

	if (strcmp(pba->pba_busname, cf->cf_driver->cd_name))
		return (0);

	/* Check the locators */
	if (cf->pcibuscf_bus != PCIBUS_UNK_BUS &&
	    cf->pcibuscf_bus != pba->pba_bus)
		return (0);

	/* sanity */
	if (pba->pba_bus < 0 || pba->pba_bus > 255)
		return (0);

	/*
	 * XXX check other (hardware?) indicators
	 */

	return 1;
}

void
pciattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct pcibus_attach_args *pba = aux;
	bus_chipset_tag_t bc;
	int device, function, nfunctions;

	pci_md_attach_hook(parent, self, pba);
	printf("\n");

	for (device = 0; device < PCI_MAX_DEVICE_NUMBER; device++) {
		pcitag_t tag;
		pcireg_t id, class;
		struct pci_attach_args pa;
		struct cfdata *cf;
		int supported;

		tag = pci_make_tag(pba->pba_bus, device, 0);
		id = pci_conf_read(tag, PCI_ID_REG);
		if (id == 0 || id == 0xffffffff)
			continue;

		nfunctions = 1;				/* XXX */

		for (function = 0; function < nfunctions; function++) {
			tag = pci_make_tag(pba->pba_bus, device, function);
			id = pci_conf_read(tag, PCI_ID_REG);
			if (id == 0 || id == 0xffffffff)
				continue;
			class = pci_conf_read(tag, PCI_CLASS_REG);

			pa.pa_bc = pba->pba_bc;
			pa.pa_device = device;
			pa.pa_function = function;
			pa.pa_tag = tag;
			pa.pa_id = id;
			pa.pa_class = class;

			config_found_sm(self, &pa, pciprint, pcisubmatch);
		}
	}
}

int
pciprint(aux, pnp)
	void *aux;
	char *pnp;
{
	register struct pci_attach_args *pa = aux;
	char devinfo[256];

	if (pnp) {
		pci_devinfo(pa->pa_id, pa->pa_class, 1, devinfo);
		printf("%s at %s", devinfo, pnp);
	}
	printf(" dev %d function %d", pa->pa_device, pa->pa_function);
	return (UNCONF);
}

int
pcisubmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct cfdata *cf = match;
	struct pci_attach_args *pa = aux;

	if (cf->pcicf_dev != PCI_UNK_DEV &&
	    cf->pcicf_dev != pa->pa_device)
		return 0;
	if (cf->pcicf_function != PCI_UNK_FUNCTION &&
	    cf->pcicf_function != pa->pa_function)
		return 0;
	return ((*cf->cf_driver->cd_match)(parent, match, aux));
}
