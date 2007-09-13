/*	$OpenBSD: if_bwi_pci.c,v 1.2 2007/09/13 08:28:37 mglocker Exp $ */

/*
 * Copyright (c) 2007 Marcus Glocker <mglocker@openbsd.org>
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
 * PCI front-end for the Broadcom AirForce
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
#include <net80211/ieee80211_radiotap.h>

#include <dev/ic/bwivar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

/* Base Address Register */
#define BWI_PCI_BAR0	0x10

int		bwi_pci_match(struct device *, void *, void *);
void		bwi_pci_attach(struct device *, struct device *, void *);
int		bwi_pci_detach(struct device *, int);
void		bwi_pci_conf_write(void *, uint32_t, uint32_t);
uint32_t	bwi_pci_conf_read(void *, uint32_t);

struct bwi_pci_softc {
	struct bwi_softc	 psc_bwi;

	pci_chipset_tag_t        psc_pc;
	pcitag_t		 psc_pcitag;
	void 			*psc_ih;

	bus_size_t		 psc_mapsize;
};

struct cfattach bwi_pci_ca = {
	sizeof(struct bwi_pci_softc), bwi_pci_match, bwi_pci_attach,
	bwi_pci_detach
};

const struct pci_matchid bwi_pci_devices[] = {
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM4303 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM4306 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM4306_2 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM4307 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM4309 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM4311 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM4312 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM4318 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM4319 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM4322 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM43XG }
};

int
bwi_pci_match(struct device *parent, void *match, void *aux)
{
	return (pci_matchbyid((struct pci_attach_args *)aux, bwi_pci_devices,
	    sizeof(bwi_pci_devices) / sizeof(bwi_pci_devices[0])));
}

void
bwi_pci_attach(struct device *parent, struct device *self, void *aux)
{
	struct bwi_pci_softc *psc = (struct bwi_pci_softc *)self;
	struct pci_attach_args *pa = aux;
	struct bwi_softc *sc = &psc->psc_bwi;
	const char *intrstr = NULL;
	pci_intr_handle_t ih;
	int error;

	sc->sc_dmat = pa->pa_dmat;
	psc->psc_pc = pa->pa_pc;

	/* map control / status registers */
	error = pci_mapreg_map(pa, BWI_PCI_BAR0,
	    PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT, 0,
	    &sc->sc_mem_bt, &sc->sc_mem_bh, NULL, &psc->psc_mapsize, 0);
	if (error != 0) {
		printf(": could not map memory space\n");
		return;
	}

	/* map interrupt */
	if (pci_intr_map(pa, &ih) != 0) {
		printf(": could not map interrupt\n");
		return;
	}

	/* establish interrupt */
	intrstr = pci_intr_string(psc->psc_pc, ih);
	psc->psc_ih = pci_intr_establish(psc->psc_pc, ih, IPL_NET, bwi_intr, sc,
	    sc->sc_dev.dv_xname);
	if (psc->psc_ih == NULL) {
		printf(": could not establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}
	printf(": %s", intrstr);

	/* we need to access PCI config space from the driver */
	sc->sc_conf_write = bwi_pci_conf_write;
	sc->sc_conf_read = bwi_pci_conf_read;

	bwi_attach(sc);
}

int
bwi_pci_detach(struct device *self, int flags)
{
	struct bwi_pci_softc *psc = (struct bwi_pci_softc *)self;
	struct bwi_softc *sc = &psc->psc_bwi;

	bwi_detach(sc);
	pci_intr_disestablish(psc->psc_pc, psc->psc_ih);

	return (0);
}

void
bwi_pci_conf_write(void *self, uint32_t reg, uint32_t val)
{
	struct bwi_pci_softc *psc = (struct bwi_pci_softc *)self;

	pci_conf_write(psc->psc_pc, psc->psc_pcitag, reg, val);
}

uint32_t
bwi_pci_conf_read(void *self, uint32_t reg)
{
	struct bwi_pci_softc *psc = (struct bwi_pci_softc *)self;

	return (pci_conf_read(psc->psc_pc, psc->psc_pcitag, reg));
}
