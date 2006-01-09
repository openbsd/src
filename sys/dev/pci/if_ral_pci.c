/*	$OpenBSD: if_ral_pci.c,v 1.6 2006/01/09 20:03:43 damien Exp $  */

/*-
 * Copyright (c) 2005, 2006
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
 * PCI front-end for the Ralink RT2560/RT2561/RT2561S/RT2661 driver.
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
#include <sys/device.h>

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

#include <dev/ic/rt2560var.h>
#include <dev/ic/rt2661var.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

static struct ral_opns {
	int	(*attach)(void *, int);
	int	(*detach)(void *);
	int	(*intr)(void *);

}  ral_rt2560_opns = {
	rt2560_attach,
	rt2560_detach,
	rt2560_intr

}, ral_rt2661_opns = {
	rt2661_attach,
	rt2661_detach,
	rt2661_intr
};

struct ral_pci_softc {
	union {
		struct rt2560_softc	sc_rt2560;
		struct rt2661_softc	sc_rt2661;
	} u;
#define sc_sc	u.sc_rt2560

	/* PCI specific goo */
	struct ral_opns		*sc_opns;
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

const struct pci_matchid ral_pci_devices[] = {
	{ PCI_VENDOR_RALINK, PCI_PRODUCT_RALINK_RT2560  },
	{ PCI_VENDOR_RALINK, PCI_PRODUCT_RALINK_RT2561  },
	{ PCI_VENDOR_RALINK, PCI_PRODUCT_RALINK_RT2561S },
	{ PCI_VENDOR_RALINK, PCI_PRODUCT_RALINK_RT2661  }
};

int
ral_pci_match(struct device *parent, void *match, void *aux)
{
	return (pci_matchbyid((struct pci_attach_args *)aux, ral_pci_devices,
	    sizeof (ral_pci_devices) / sizeof (ral_pci_devices[0])));
}

void
ral_pci_attach(struct device *parent, struct device *self, void *aux)
{
	struct ral_pci_softc *psc = (struct ral_pci_softc *)self;
	struct rt2560_softc *sc = &psc->sc_sc;
	struct pci_attach_args *pa = aux;
	const char *intrstr;
	bus_addr_t base;
	pci_intr_handle_t ih;
	int error;

	psc->sc_opns = (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_RALINK_RT2560) ?
	    &ral_rt2560_opns : &ral_rt2661_opns;

	sc->sc_dmat = pa->pa_dmat;
	psc->sc_pc = pa->pa_pc;

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
	psc->sc_ih = pci_intr_establish(psc->sc_pc, ih, IPL_NET,
	    psc->sc_opns->intr, sc, sc->sc_dev.dv_xname);
	if (psc->sc_ih == NULL) {
		printf(": could not establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}
	printf(": %s", intrstr);

	(*psc->sc_opns->attach)(sc, PCI_PRODUCT(pa->pa_id));
}

int
ral_pci_detach(struct device *self, int flags)
{
	struct ral_pci_softc *psc = (struct ral_pci_softc *)self;
	struct rt2560_softc *sc = &psc->sc_sc;

	(*psc->sc_opns->detach)(sc);
	pci_intr_disestablish(psc->sc_pc, psc->sc_ih);

	return 0;
}
