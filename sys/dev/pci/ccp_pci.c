/*	$OpenBSD: ccp_pci.c,v 1.13 2024/09/04 07:45:08 jsg Exp $ */

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
#include <sys/device.h>
#include <sys/timeout.h>

#include <machine/bus.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcivar.h>

#include <dev/ic/ccpvar.h>
#include <dev/ic/pspvar.h>

#include "psp.h"

#define CCP_PCI_BAR	0x18

int	ccp_pci_match(struct device *, void *, void *);
void	ccp_pci_attach(struct device *, struct device *, void *);

void	ccp_pci_intr_map(struct ccp_softc *, struct pci_attach_args *);
void	ccp_pci_psp_attach(struct ccp_softc *, struct pci_attach_args *);

const struct cfattach ccp_pci_ca = {
	sizeof(struct ccp_softc),
	ccp_pci_match,
	ccp_pci_attach,
};

static const struct pci_matchid ccp_pci_devices[] = {
	{ PCI_VENDOR_AMD,	PCI_PRODUCT_AMD_16_CCP },
	{ PCI_VENDOR_AMD,	PCI_PRODUCT_AMD_17_CCP_1 },
	{ PCI_VENDOR_AMD,	PCI_PRODUCT_AMD_17_CCP_2 },
	{ PCI_VENDOR_AMD,	PCI_PRODUCT_AMD_17_1X_CCP },
	{ PCI_VENDOR_AMD,	PCI_PRODUCT_AMD_17_3X_CCP },
	{ PCI_VENDOR_AMD,	PCI_PRODUCT_AMD_17_90_CCP },
	{ PCI_VENDOR_AMD,	PCI_PRODUCT_AMD_19_1X_PSP },
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

	ccp_pci_intr_map(sc, pa);

	ccp_attach(sc);

	ccp_pci_psp_attach(sc, pa);
}

void
ccp_pci_intr_map(struct ccp_softc *sc, struct pci_attach_args *pa)
{
#if NPSP > 0
	pci_intr_handle_t ih;
	const char *intrstr = NULL;

	/* clear and disable interrupts */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, PSP_REG_INTEN, 0);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, PSP_REG_INTSTS, -1);

	if (pci_intr_map_msix(pa, 0, &ih) != 0 &&
	    pci_intr_map_msi(pa, &ih) != 0 && pci_intr_map(pa, &ih) != 0) {
		printf(": couldn't map interrupt\n");
		return;
	}

	intrstr = pci_intr_string(pa->pa_pc, ih);
	sc->sc_irqh = pci_intr_establish(pa->pa_pc, ih, IPL_BIO, psp_sev_intr,
	    sc, sc->sc_dev.dv_xname);
	if (sc->sc_irqh != NULL)
		printf(": %s", intrstr);
#endif
}

void
ccp_pci_psp_attach(struct ccp_softc *sc, struct pci_attach_args *pa)
{
#if NPSP > 0
	struct psp_attach_args arg;
	struct device *self = (struct device *)sc;

	memset(&arg, 0, sizeof(arg));
	arg.iot = sc->sc_iot;
	arg.ioh = sc->sc_ioh;
	arg.dmat = pa->pa_dmat;
	arg.capabilities = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
	    PSP_REG_CAPABILITIES);

	sc->sc_psp = config_found_sm(self, &arg, pspprint, pspsubmatch);
	if (sc->sc_psp == NULL) {
		pci_intr_disestablish(pa->pa_pc, sc->sc_irqh);
		return;
	}

	/* enable interrupts */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, PSP_REG_INTEN, -1);
#endif
}
