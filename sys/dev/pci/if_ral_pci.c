/*	$OpenBSD: if_ral_pci.c,v 1.3 2005/02/19 12:11:40 damien Exp $  */

/*-
 * Copyright (c) 2005
 *	Damien Bergamini <damien.bergamini@free.fr>
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

/*
 * PCI front-end for the Ralink RT2500 driver.
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/timeout.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_rssadapt.h>
#include <net80211/ieee80211_radiotap.h>

#include <dev/ic/ralvar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

struct ral_pci_softc {
	struct ral_softc	sc_sc;

	/* PCI specific goo */
	pci_chipset_tag_t	sc_pc;
	void			*sc_ih;
	bus_size_t		sc_mapsize;
};

/* Base Address Register */
#define RAL_PCI_BAR0	0x10

int	ral_pci_match(struct device *, void *, void *);
void	ral_pci_attach(struct device *, struct device *, void *);
int	ral_pci_detach(struct device *, int);

struct cfattach ral_pci_ca = {
	sizeof (struct ral_pci_softc), ral_pci_match, ral_pci_attach,
	ral_pci_detach
};

int
ral_pci_match(struct device *parent, void *match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_RALINK &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_RALINK_RT2560)
		return 1;

	return 0;
}

void
ral_pci_attach(struct device *parent, struct device *self, void *aux)
{
	struct ral_pci_softc *psc = (struct ral_pci_softc *)self;
	struct ral_softc *sc = &psc->sc_sc;
	struct pci_attach_args *pa = aux;
	const char *intrstr;
	bus_addr_t base;
	pci_intr_handle_t ih;
	pcireg_t reg;
	int error;
	
	sc->sc_dmat = pa->pa_dmat;
	psc->sc_pc = pa->pa_pc;

	/* enable the appropriate bits in the PCI CSR */
	reg = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	reg |= PCI_COMMAND_MASTER_ENABLE | PCI_COMMAND_MEM_ENABLE;
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, reg);

	/* map control/status registers */
	error = pci_mapreg_map(pa, RAL_PCI_BAR0, PCI_MAPREG_TYPE_MEM |
	    PCI_MAPREG_MEM_TYPE_32BIT, 0, &sc->sc_st, &sc->sc_sh, &base,
	    &psc->sc_mapsize, 0);
	if (error != 0) {
		printf(": could not map memory space\n");
		return;
	}

	if (pci_intr_map(pa, &ih) != 0) {
		printf(": could not map interrupt\n");
		return;
	}

	intrstr = pci_intr_string(psc->sc_pc, ih);
	psc->sc_ih = pci_intr_establish(psc->sc_pc, ih, IPL_NET, ral_intr, sc,
	    sc->sc_dev.dv_xname);
	if (psc->sc_ih == NULL) {
		printf(": could not establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}
	printf(": %s", intrstr);

	ral_attach(sc);
}

int
ral_pci_detach(struct device *self, int flags)
{
	struct ral_pci_softc *psc = (struct ral_pci_softc *)self;
	struct ral_softc *sc = &psc->sc_sc;

	ral_detach(sc);
	pci_intr_disestablish(psc->sc_pc, psc->sc_ih);

	return 0;
}
