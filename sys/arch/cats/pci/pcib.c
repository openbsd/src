/*	$OpenBSD: pcib.c,v 1.2 2004/05/19 04:11:31 drahn Exp $	*/
/*	$NetBSD: pcib.c,v 1.4 2003/01/01 01:25:34 thorpej Exp $	*/

/*-
 * Copyright (c) 1996, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * 	from:  i386/pci/pcib.c,v 1.12
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/bus.h>

#include <dev/isa/isavar.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include <dev/pci/pcidevs.h>

#include "isadma.h"

int	pcibmatch (struct device *, void *, void *);
void	pcibattach (struct device *, struct device *, void *);

struct cfattach pcib_ca = {
	sizeof(struct device), pcibmatch, pcibattach
};

struct cfdriver pcib_cd = {
	NULL, "pcib", DV_DULL
};


void	pcib_callback (struct device *);
int	pcib_print (void *, const char *);

int
pcibmatch(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct pci_attach_args *pa = aux;

	/*
	 * Match tested PCI-ISA bridges.
	 */
	switch (PCI_VENDOR(pa->pa_id)) {
	case PCI_VENDOR_ALI:
		switch (PCI_PRODUCT(pa->pa_id)) {
		case PCI_PRODUCT_ALI_M1533:
		case PCI_PRODUCT_ALI_M1543:
			return (1);
		}
		break;
	}

	return (0);
}

void
pcibattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct pci_attach_args *pa = aux;
	char devinfo[256];

	printf("\n");

	/*
	 * Just print out a description and set the ISA bus
	 * callback.
	 */
	pci_devinfo(pa->pa_id, pa->pa_class, 0, devinfo, sizeof devinfo);
	printf("%s: %s (rev. 0x%02x)\n", self->dv_xname, devinfo,
	    PCI_REVISION(pa->pa_class));

	/* Set the ISA bus callback */
	config_defer(self, pcib_callback);
}

void
pcib_callback(self)
	struct device *self;
{
	struct isabus_attach_args iba;

	/*
	 * Attach the ISA bus behind this bridge.
	 */
	memset(&iba, 0, sizeof(iba));
	iba.iba_busname = "isa";
	iba.iba_iot = &isa_io_bs_tag;
	iba.iba_memt = &isa_mem_bs_tag;
#if NISADMA > 0
	iba.iba_dmat = &isa_bus_dma_tag;
#endif
	config_found(self, &iba, pcib_print);
}

int
pcib_print(aux, pnp)
	void *aux;
	const char *pnp;
{

	/* Only ISAs can attach to pcib's; easy. */
	if (pnp)
		printf("isa at %s", pnp);
	return (UNCONF);
}
