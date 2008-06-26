/*	$OpenBSD: pchb.c,v 1.11 2008/06/26 05:42:12 ray Exp $	*/
/*	$NetBSD: pchb.c,v 1.4 2000/01/25 07:19:11 tsubai Exp $	*/

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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
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

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

int	pchbmatch(struct device *, void *, void *);
void	pchbattach(struct device *, struct device *, void *);

struct cfattach pchb_ca = {
	sizeof(struct device), pchbmatch, pchbattach
};

struct cfdriver pchb_cd = {
	NULL, "pchb", DV_DULL
};

int
pchbmatch(struct device *parent, void *cf, void *aux)
{
	struct pci_attach_args *pa = aux;

	/*
	 * Match all known PCI host chipsets.
	 */
	switch (PCI_VENDOR(pa->pa_id)) {
	case PCI_VENDOR_APPLE:
		switch (PCI_PRODUCT(pa->pa_id)) {
		case PCI_PRODUCT_APPLE_BANDIT:
		case PCI_PRODUCT_APPLE_UNINORTH:
		case PCI_PRODUCT_APPLE_UNINORTHETH:
		case PCI_PRODUCT_APPLE_UNINORTH_AGP:
		case PCI_PRODUCT_APPLE_PANGEA:
		case PCI_PRODUCT_APPLE_PANGEA_PCI:
		case PCI_PRODUCT_APPLE_PANGEA_AGP:
		case PCI_PRODUCT_APPLE_UNINORTH2:
		case PCI_PRODUCT_APPLE_UNINORTH2ETH:
		case PCI_PRODUCT_APPLE_UNINORTH2_AGP:
		case PCI_PRODUCT_APPLE_UNINORTH_AGP3:
		case PCI_PRODUCT_APPLE_UNINORTH5:
		case PCI_PRODUCT_APPLE_UNINORTH6:
		case PCI_PRODUCT_APPLE_SHASTA_HT:
		case PCI_PRODUCT_APPLE_K2:
		case PCI_PRODUCT_APPLE_K2_AGP:
		case PCI_PRODUCT_APPLE_INTREPID2_AGP:
		case PCI_PRODUCT_APPLE_INTREPID2_PCI1:
		case PCI_PRODUCT_APPLE_INTREPID2_PCI2:
			return (1);
		}
		break;

	case PCI_VENDOR_MOT:
		switch (PCI_PRODUCT(pa->pa_id)) {
		case PCI_PRODUCT_MOT_MPC106:
			return (1);
		}
		break;
	}

	return (0);
}

/*ARGSUSED*/
void
pchbattach(struct device *parent, struct device  *self, void *aux)
{
	printf("\n");

	/*
	 * All we do is print out a description.  Eventually, we
	 * might want to add code that does something that's
	 * possibly chipset-specific.
	 */

	/*
	pci_devinfo(pa->pa_id, pa->pa_class, 0, devinfo, sizeof devinfo);
	printf("%s: %s (rev. 0x%02x)\n", self->dv_xname, devinfo,
	    PCI_REVISION(pa->pa_class));
	*/
}
