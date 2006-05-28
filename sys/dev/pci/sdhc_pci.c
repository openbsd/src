/*	$OpenBSD: sdhc_pci.c,v 1.1 2006/05/28 17:21:14 uwe Exp $	*/

/*
 * Copyright (c) 2006 Uwe Stuehler <uwe@openbsd.org>
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
#include <sys/device.h>
#include <sys/systm.h>

#include <dev/pci/pcivar.h>
#include <dev/sdmmc/sdhcreg.h>
#include <dev/sdmmc/sdhcvar.h>
#include <dev/sdmmc/sdmmcvar.h>

struct sdhc_pci_softc {
	struct sdhc_softc sc;
	void *sc_ih;
};

int	sdhc_pci_match(struct device *, void *, void *);
void	sdhc_pci_attach(struct device *, struct device *, void *);

struct cfattach sdhc_pci_ca = {
	sizeof(struct sdhc_pci_softc), sdhc_pci_match, sdhc_pci_attach
};

int
sdhc_pci_match(struct device *parent, void *match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_SYSTEM &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_SYSTEM_SDHC)
		return 1;

	return 0;
}

void
sdhc_pci_attach(struct device *parent, struct device *self, void *aux)
{
	struct sdhc_pci_softc *sc = (struct sdhc_pci_softc *)self;
	struct pci_attach_args *pa = aux;
	pci_intr_handle_t ih;
	char const *intrstr;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	bus_size_t size;
	int usedma;
	int reg;

	if (pci_intr_map(pa, &ih)) {
		printf(": can't map interrupt\n");
		return;
	}

	intrstr = pci_intr_string(pa->pa_pc, ih);
	sc->sc_ih = pci_intr_establish(pa->pa_pc, ih, IPL_SDMMC,
	    sdhc_intr, sc, sc->sc.sc_dev.dv_xname);
	if (sc->sc_ih == NULL) {
		printf(": can't establish interrupt\n");
		return;
	}
	printf(": %s\n", intrstr);

	/* Enable use of DMA if supported by the interface. */
	usedma = PCI_INTERFACE(pa->pa_class) == SDHC_PCI_INTERFACE_DMA;

	/*
	 * Map and attach all hosts supported by the host controller.
	 */
	for (reg = SDHC_PCI_BAR_START; reg < SDHC_PCI_BAR_END; reg += 4) {

		if (pci_mem_find(pa->pa_pc, pa->pa_tag, reg,
		    NULL, NULL, NULL) != 0)
			continue;

		if (pci_mapreg_map(pa, reg, PCI_MAPREG_TYPE_MEM, 0,
		    &iot, &ioh, NULL, &size, 0)) {
			printf("%s at 0x%x: can't map registers\n",
			    sc->sc.sc_dev.dv_xname, reg);
			continue;
		}

		if (sdhc_host_found(&sc->sc, iot, ioh, size, usedma) != 0)
			printf("%s at 0x%x: can't initialize host\n",
			    sc->sc.sc_dev.dv_xname, reg);
	}

	/*
	 * Establish power and shutdown hooks.
	 */
	(void)powerhook_establish(sdhc_power, &sc->sc);
	(void)shutdownhook_establish(sdhc_shutdown, &sc->sc);
}
