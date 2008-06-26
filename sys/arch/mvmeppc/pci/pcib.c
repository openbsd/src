/*	$OpenBSD: pcib.c,v 1.3 2008/06/26 05:42:12 ray Exp $	*/
/*	$NetBSD: pcib.c,v 1.6 1997/06/06 23:29:16 thorpej Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
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
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/bus.h>
#include <dev/isa/isavar.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include <dev/pci/pcidevs.h>
#include <mvmeppc/dev/ravenreg.h>

#include "isa.h"

int	pcibmatch(struct device *, void *, void *);
void	pcibattach(struct device *, struct device *, void *);
void	pcib_callback(struct device *);
int	pcib_print(void *, const char *);

struct ppc_bus_space ppc_isa_io, ppc_isa_mem;

struct cfattach pcib_ca = {
	sizeof(struct device), pcibmatch, pcibattach
};

struct cfdriver pcib_cd = {
	NULL, "pcib", DV_DULL
};

int
pcibmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct pci_attach_args *pa = aux;

	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_BRIDGE &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_BRIDGE_ISA)
		return (1);

	switch (PCI_VENDOR(pa->pa_id)) {
	case PCI_VENDOR_SYMPHONY:
		switch (PCI_PRODUCT(pa->pa_id)) {
		case PCI_PRODUCT_SYMPHONY_82C565:
			return (1);
		}
	}

	return (0);
}

void
pcibattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	
	ppc_isa_io = prep_isa_io_space_tag;
	ppc_isa_mem = prep_isa_mem_space_tag;
	
	/*
	 * Cannot attach isa bus now; must postpone for various reasons
	 */
		
	printf("\n");

	config_defer(self, pcib_callback);
}

void
pcib_callback(self)
	struct device *self;
{
	struct isabus_attach_args iba;

#if NPCIBIOS > 0
	pci_intr_post_fixup();
#endif

	/*
	 * Attach the ISA bus behind this bridge.
	 */
	memset(&iba, 0, sizeof(iba));
	iba.iba_busname = "isa";
	iba.iba_iot = &ppc_isa_io;
	iba.iba_memt = &ppc_isa_mem;
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
