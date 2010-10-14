/*	$OpenBSD: pcib.c,v 1.3 2010/10/14 21:23:04 pirofti Exp $	*/

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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/autoconf.h>
#include <machine/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/isa/isavar.h>

#include <dev/pci/glxreg.h>
#include <dev/pci/glxvar.h>

#include "isa.h"

int	pcibmatch(struct device *, void *, void *);
void	pcibattach(struct device *, struct device *, void *);
void	pcib_callback(struct device *);
int	pcib_print(void *, const char *);

struct pcib_softc {
	struct device	sc_dev;
	bus_space_tag_t	sc_iot;
	bus_space_tag_t	sc_memt;
	bus_dma_tag_t	sc_dmat;
};

const struct cfattach pcib_ca = {
	sizeof(struct pcib_softc), pcibmatch, pcibattach
};

struct cfdriver pcib_cd = {
	NULL, "pcib", DV_DULL
};

int
pcibmatch(struct device *parent, void *match, void *aux)
{
	struct pci_attach_args *pa = aux;

	switch (PCI_VENDOR(pa->pa_id)) {
	case PCI_VENDOR_INTEL:
		switch (PCI_PRODUCT(pa->pa_id)) {
		case PCI_PRODUCT_INTEL_SIO:
		case PCI_PRODUCT_INTEL_82371MX:
		case PCI_PRODUCT_INTEL_82371AB_ISA:
		case PCI_PRODUCT_INTEL_82440MX_ISA:
			/* The above bridges mis-identify themselves */
			return (1);
		}
		break;
	case PCI_VENDOR_SIS:
		switch (PCI_PRODUCT(pa->pa_id)) {
		case PCI_PRODUCT_SIS_85C503:
			/* mis-identifies itself as a miscellaneous prehistoric */
			return (1);
		}
		break;
	case PCI_VENDOR_VIATECH:
		switch (PCI_PRODUCT(pa->pa_id)) {
		case PCI_PRODUCT_VIATECH_VT82C686A_SMB:
			/* mis-identifies itself as a ISA bridge */
			return (0);
		}
		break;
	}

	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_BRIDGE &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_BRIDGE_ISA)
		return (1);

	return (0);
}

void
pcibattach(struct device *parent, struct device *self, void *aux)
{
	struct pcib_softc *sc = (struct pcib_softc *)self;
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;

	printf("\n");

	/*
	 * Wait until all PCI devices are attached before attaching isa;
	 * otherwise this might mess the interrupt setup on some systems.
	 */
	sc->sc_iot = pa->pa_iot;
	sc->sc_memt = pa->pa_memt;
	sc->sc_dmat = pa->pa_dmat;
	config_defer(self, pcib_callback);
}

void
pcib_callback(struct device *self)
{
	struct pcib_softc *sc = (struct pcib_softc *)self;
	struct isabus_attach_args iba;

	/*
	 * Attach the ISA bus behind this bridge.
	 * Note that, since we only have a few legacy I/O devices and
	 * no ISA slots, we can attach immediately.
	 */
	memset(&iba, 0, sizeof(iba));
	iba.iba_busname = "isa";
	iba.iba_iot = sc->sc_iot;
	iba.iba_memt = sc->sc_memt;
#if NISADMA > 0
	iba.iba_dmat = sc->sc_dmat;
#endif
	iba.iba_ic = sys_platform->isa_chipset;
	if (iba.iba_ic != NULL)
		config_found(self, &iba, pcib_print);
}

int
pcib_print(void *aux, const char *pnp)
{
	/* Only ISAs can attach to pcib's; easy. */
	if (pnp)
		printf("isa at %s", pnp);
	return (UNCONF);
}
