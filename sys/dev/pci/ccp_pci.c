/*	$OpenBSD: ccp_pci.c,v 1.12 2024/09/03 00:23:05 jsg Exp $ */

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

#define CCP_PCI_BAR	0x18

int	ccp_pci_match(struct device *, void *, void *);
void	ccp_pci_attach(struct device *, struct device *, void *);

void	psp_pci_attach(struct device *, struct device *, void *);
int	psp_pci_intr(void *);

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

	psp_pci_attach(parent, self, aux);

	ccp_attach(sc);
}

void
psp_pci_attach(struct device *parent, struct device *self, void *aux)
{
	struct ccp_softc *sc = (struct ccp_softc *)self;
	struct pci_attach_args *pa = aux;
	pci_intr_handle_t ih;
	const char *intrstr = NULL;

	sc->sc_dmat = pa->pa_dmat;

	sc->sc_capabilities = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
	    PSP_REG_CAPABILITIES);

	/* clear and disable interrupts */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, PSP_REG_INTEN, 0);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, PSP_REG_INTSTS, -1);

	if (pci_intr_map_msix(pa, 0, &ih) != 0 &&
	    pci_intr_map_msi(pa, &ih) != 0 && pci_intr_map(pa, &ih) != 0) {
		printf(": couldn't map interrupt\n");
		return;
	}

	intrstr = pci_intr_string(pa->pa_pc, ih);
	sc->sc_ih = pci_intr_establish(pa->pa_pc, ih, IPL_BIO, psp_pci_intr,
	    sc, sc->sc_dev.dv_xname);
	if (sc->sc_ih != NULL)
		printf(": %s", intrstr);

	if (!psp_attach(sc)) {
		pci_intr_disestablish(pa->pa_pc, sc->sc_ih);
		sc->sc_ih = NULL;
		return;
	}

	/* enable interrupts */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, PSP_REG_INTEN, -1);
}

int
psp_pci_intr(void *arg)
{
	struct ccp_softc *sc = arg;
	uint32_t status;

	status = bus_space_read_4(sc->sc_iot, sc->sc_ioh, PSP_REG_INTSTS);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, PSP_REG_INTSTS, status);

	if (sc->sc_sev_intr)
		return (sc->sc_sev_intr(sc, status));

	return (1);
}
