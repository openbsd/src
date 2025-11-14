/* $OpenBSD: ispi_pci.c,v 1.1 2025/11/14 01:55:07 jcs Exp $ */
/*
 * Intel LPSS SPI controller
 * PCI attachment
 *
 * Copyright (c) 2015-2018 joshua stein <jcs@openbsd.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
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
#include <sys/kthread.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/lpssreg.h>

#include <dev/ic/ispivar.h>

int	ispi_pci_match(struct device *, void *, void *);
void	ispi_pci_attach(struct device *, struct device *, void *);
int	ispi_pci_activate(struct device *, int);
void	ispi_pci_bus_scan(struct ispi_softc *);

const struct cfattach ispi_pci_ca = {
	sizeof(struct ispi_softc),
	ispi_pci_match,
	ispi_pci_attach,
	NULL,
	ispi_pci_activate,
};

const struct pci_matchid ispi_pci_ids[] = {
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_100SERIES_LP_SPI_3 },
};

int
ispi_pci_match(struct device *parent, void *match, void *aux)
{
	return (pci_matchbyid(aux, ispi_pci_ids, nitems(ispi_pci_ids)));
}

void
ispi_pci_attach(struct device *parent, struct device *self, void *aux)
{
	struct ispi_softc *sc = (struct ispi_softc *)self;
	struct pci_attach_args *pa = aux;
	bus_size_t iosize;
	pci_intr_handle_t ih;
	const char *intrstr = NULL;
	uint8_t type;

	memcpy(&sc->sc_paa, pa, sizeof(sc->sc_paa));

	pci_set_powerstate(pa->pa_pc, pa->pa_tag, PCI_PMCSR_STATE_D0);

	if (pci_mapreg_map(pa, PCI_MAPREG_START, PCI_MAPREG_MEM_TYPE_64BIT, 0,
	    &sc->sc_iot, &sc->sc_ioh, NULL, &iosize, 0)) {
		printf(": can't map mem space\n");
		return;
	}

	sc->sc_caps = bus_space_read_4(sc->sc_iot, sc->sc_ioh, LPSS_CAPS);
	type = (sc->sc_caps & LPSS_CAPS_TYPE_MASK) >> LPSS_CAPS_TYPE_SHIFT;
	if (type != LPSS_CAPS_TYPE_SPI) {
		printf(": type %d not supported\n", type);
		return;
	}

	switch (PCI_PRODUCT(pa->pa_id)) {
	case PCI_PRODUCT_INTEL_100SERIES_LP_SPI_3:
		/* SPT */
		sc->sc_lpss_reg_offset = 0x200;
		sc->sc_reg_cs_ctrl = 0x24;
		sc->sc_rx_threshold = 1;
		sc->sc_tx_threshold = 32;
		sc->sc_tx_threshold_hi = 56;
		sc->sc_ssp_clk = 9600000;
		break;
	default:
		printf(": unknown parameters\n");
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, iosize);
		return;
	}

	/* un-reset - page 958 */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, LPSS_RESETS,
	    (LPSS_RESETS_FUNC | LPSS_RESETS_IDMA));

	/* install interrupt handler */
	if (pci_intr_map(&sc->sc_paa, &ih) == 0) {
		intrstr = pci_intr_string(sc->sc_paa.pa_pc, ih);
		sc->sc_ih = pci_intr_establish(sc->sc_paa.pa_pc, ih, IPL_BIO,
		    ispi_intr, sc, sc->sc_dev.dv_xname);
		if (sc->sc_ih != NULL) {
			printf(": %s", intrstr);
		}
	}

	printf("\n");

	rw_init(&sc->sc_buslock, sc->sc_dev.dv_xname);

	ispi_init(sc);

	/* setup spi controller */
	sc->sc_spi_tag.sc_cookie = sc;
	sc->sc_spi_tag.sc_config = ispi_config;
	sc->sc_spi_tag.sc_transfer = ispi_transfer;
	sc->sc_spi_tag.sc_acquire_bus = ispi_acquire_bus;
	sc->sc_spi_tag.sc_release_bus = ispi_release_bus;

	ispi_pci_bus_scan(sc);
}

int
ispi_pci_activate(struct device *self, int act)
{
	struct ispi_softc *sc = (struct ispi_softc *)self;

	switch (act) {
	case DVACT_WAKEUP:
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, LPSS_RESETS,
		    (LPSS_RESETS_FUNC | LPSS_RESETS_IDMA));
		break;
	}

	ispi_activate(self, act);

	return 0;
}

void
ispi_pci_bus_scan(struct ispi_softc *sc)
{
#if NACPI > 0
	struct aml_node *node = acpi_pci_match((struct device *)sc, &sc->sc_paa);

	if (node == NULL)
		return;

	sc->sc_devnode = node;
	aml_find_node(node, "_HID", ispi_acpi_found_hid, sc);
#endif
}
