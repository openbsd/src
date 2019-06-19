/*	$OpenBSD: ccp_pci.c,v 1.3 2019/05/05 09:09:59 kettenis Exp $ */

/*
 * Copyright (c) 2018 David Gwynne <dlg@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/rwlock.h>
#include <sys/device.h>
#include <sys/timeout.h>
#include <sys/queue.h>

#include <machine/bus.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcivar.h>

#include <dev/ic/ccpvar.h>

#define CCP_PCI_BAR	0x18

int	ccp_pci_match(struct device *, void *, void *);
void	ccp_pci_attach(struct device *, struct device *, void *);

struct cfattach ccp_pci_ca = {
	sizeof(struct ccp_softc),
	ccp_pci_match,
	ccp_pci_attach,
};

static const struct pci_matchid ccp_pci_devices[] = {
	{ PCI_VENDOR_AMD,	PCI_PRODUCT_AMD_AMD64_16_CCP },
	{ PCI_VENDOR_AMD,	PCI_PRODUCT_AMD_AMD64_17_CCP_1 },
	{ PCI_VENDOR_AMD,	PCI_PRODUCT_AMD_AMD64_17_CCP_2 },
	{ PCI_VENDOR_AMD,	PCI_PRODUCT_AMD_AMD64_17_1X_CCP },
};

int
ccp_pci_match(struct device *parent, void *match, void *aux)
{
	return (pci_matchbyid(aux, ccp_pci_devices, nitems(ccp_pci_devices)));
}

void
ccp_pci_attach(struct device *parent, struct device *self, void *aux)
{
	struct ccp_softc *sc = (struct ccp_softc *)self;
	struct pci_attach_args *pa = aux;
	pcireg_t memtype;

	memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, CCP_PCI_BAR);
	if (PCI_MAPREG_TYPE(memtype) != PCI_MAPREG_TYPE_MEM) {
		printf(": wrong memory type\n");
		return;
	}

	if (pci_mapreg_map(pa, CCP_PCI_BAR, memtype, 0,
	    &sc->sc_iot, &sc->sc_ioh, NULL, NULL, 0) != 0) {
		printf(": cannot map registers\n");
		return;
	}

	ccp_attach(sc);
}
