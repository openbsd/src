/*	$OpenBSD: if_pgt_pci.c,v 1.2 2006/08/24 23:55:35 mglocker Exp $  */

/*
 * Copyright (c) 2006 Marcus Glocker <mglocker@openbsd.org>
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
 * PCI front-end for the PrismGT
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
#include <net80211/ieee80211_amrr.h>
#include <net80211/ieee80211_rssadapt.h>
#include <net80211/ieee80211_radiotap.h>

#include <dev/ic/pgtreg.h>
#include <dev/ic/pgtvar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

/* Base Address Register */
#define PGT_PCI_BAR0	0x10

int	pgt_pci_match(struct device *, void *, void *);
void	pgt_pci_attach(struct device *, struct device *, void *);
int	pgt_pci_detach(struct device *, int);

struct pgt_pci_softc {
	struct pgt_softc	sc_pgt;

	pci_chipset_tag_t       sc_pc;
	void 			*sc_ih;
	bus_size_t		sc_mapsize;
	pcireg_t		sc_bar0_val;
};

struct cfattach pgt_pci_ca = {
	sizeof(struct pgt_pci_softc), pgt_pci_match, pgt_pci_attach,
	pgt_pci_detach
};

const struct pci_matchid pgt_pci_devices[] = {
	{ PCI_VENDOR_INTERSIL, PCI_PRODUCT_INTERSIL_ISL3877 },
	{ PCI_VENDOR_INTERSIL, PCI_PRODUCT_INTERSIL_ISL3890 }
};

int
pgt_pci_match(struct device *parent, void *match, void *aux)
{
	return (pci_matchbyid((struct pci_attach_args *)aux, pgt_pci_devices,
	    sizeof(pgt_pci_devices) / sizeof(pgt_pci_devices[0])));
}

void
pgt_pci_attach(struct device *parent, struct device *self, void *aux)
{
	struct pgt_pci_softc *psc = (struct pgt_pci_softc *)self;
	struct pgt_softc *sc = &psc->sc_pgt;
	struct pci_attach_args *pa = aux;
	const char *intrstr = NULL;
	bus_addr_t base;
	pci_intr_handle_t ih;
	int error;

	sc->sc_cbdmat = pa->pa_dmat;
	psc->sc_pc = pa->pa_pc;

	/* map control / status registers */
	error = pci_mapreg_map(pa, PGT_PCI_BAR0,
	    PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT, 0,
	    &sc->sc_iotag, &sc->sc_iohandle, &base, &psc->sc_mapsize, 0);
	if (error != 0) {
		printf(": could not map memory space\n");
		return;
	}
	psc->sc_bar0_val = base | PCI_MAPREG_TYPE_MEM;

	/* map interrupt */
	if (pci_intr_map(pa, &ih) != 0) {
		printf(": could not map interrupt\n");
		return;
	}

	/* establish interrupt */
	intrstr = pci_intr_string(psc->sc_pc, ih);
	psc->sc_ih = pci_intr_establish(psc->sc_pc, ih, IPL_NET, pgt_intr, sc,
	    sc->sc_dev.dv_xname);
	if (psc->sc_ih == NULL) {
		printf(": could not establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}
	printf(": %s\n", intrstr);

	pgt_attach(sc);
}

int
pgt_pci_detach(struct device *self, int flags)
{
	struct pgt_pci_softc *psc = (struct pgt_pci_softc *)self;
	struct pgt_softc *sc = &psc->sc_pgt;

	pgt_detach(sc);
	pci_intr_disestablish(psc->sc_pc, psc->sc_ih);

	return (0);
}

#if 0
/*-
 * Copyright (c) 2004 Fujitsu Laboratories of America, Inc.
 * Copyright (c) 2004 Brian Fundakowski Feldman
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: if_pgt_pci.c,v 1.2 2006/08/24 23:55:35 mglocker Exp $
 */

#include <sys/cdefs.h>
#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include <machine/bus.h>
#include <machine/endian.h>
#include <machine/intr.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#endif

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_radiotap.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/ic/if_pffreg.h>
#include <dev/ic/if_pffvar.h>

int     pgt_pci_match(struct device *, void *, void *);
void    pgt_pci_attach(struct device *, struct device *, void *);
int     pgt_pci_detach(struct device *, int);

struct cfattach pgt_pci_ca = {
        sizeof (struct pgt_softc), pgt_pci_match, pgt_pci_attach,
        pgt_pci_detach
};

static const struct pgt_ident {
	enum pgt_dev_type	dev_type;
	uint16_t		vendor;
	uint16_t		device;
	uint16_t		subvendor;
	uint16_t		subdevice;
} pgt_ident_table[] = {
	{	/* 3COM 3CRWE154G72 Wireless LAN adapter */
	  PFF_DEV_3COM6001,
	  PCI_VENDOR_3COM, PCI_PRODUCT_3COM_3CRWE154G72,
	  PCI_VENDOR_3COM, PCI_PRODUCT_3COM_3CRWE154G72
	},
	{	/* D-Link Air Plus Xtreme G A1 - DWL-g650 A1" */
	  PFF_DEV_ISL3890,
	  PCI_VENDOR_INTERSIL, PCI_PRODUCT_INTERSIL_ISL3890,
	  PCI_VENDOR_DLINK, 0x3202
	},
	{	/* I-O Data WN-G54/CB - WN-G54/CB" */
	  PFF_DEV_ISL3890,
	  PCI_VENDOR_INTERSIL, PCI_PRODUCT_INTERSIL_ISL3890,
	  PCI_VENDOR_IODATA, 0xd019
	},
	{	/* NETGEAR WG511" */
	  PFF_DEV_ISL3890,
	  PCI_VENDOR_INTERSIL, PCI_PRODUCT_INTERSIL_ISL3890,
	  PCI_VENDOR_NETGEAR, 0x4800
	},
	{	/* PLANEX GW-DS54G" */
	  PFF_DEV_ISL3890,
	  PCI_VENDOR_INTERSIL, PCI_PRODUCT_INTERSIL_ISL3890,
	  PCI_VENDOR_I4, 0x0020
	},
	{	/* EZ Connect g 2.4GHz 54 Mbps Wireless PCI Card - SMC2802W" */
	  PFF_DEV_ISL3890,
	  PCI_VENDOR_INTERSIL, PCI_PRODUCT_INTERSIL_ISL3890,
	  PCI_VENDOR_SMC, 0x2802
	},
	{	/* EZ Connect g 2.4GHz 54 Mbps Wireless Cardbus Adapter - SMC2835W" */
	  PFF_DEV_ISL3890,
	  PCI_VENDOR_INTERSIL, PCI_PRODUCT_INTERSIL_ISL3890,
	  PCI_VENDOR_SMC, 0x2835
	},
	{	/* I4 Z-Com XG-600" */
	  PFF_DEV_ISL3890,
	  PCI_VENDOR_INTERSIL, PCI_PRODUCT_INTERSIL_ISL3890,
	  PCI_VENDOR_I4, 0x0014
	},
	{	/* I4 Z-Com XG-900/PLANEX GW-DS54G" */
	  PFF_DEV_ISL3890,
	  PCI_VENDOR_INTERSIL, PCI_PRODUCT_INTERSIL_ISL3890,
	  PCI_VENDOR_I4, 0x0020
	},
	{	/* SMC 2802Wv2" */
	  PFF_DEV_ISL3890,
	  PCI_VENDOR_INTERSIL, PCI_PRODUCT_INTERSIL_ISL3890,
	  PCI_VENDOR_ACCTON, 0xee03
	},
	{	/* SMC 2835Wv2" */
	  PFF_DEV_ISL3890,
	  PCI_VENDOR_INTERSIL, PCI_PRODUCT_INTERSIL_ISL3890,
	  PCI_VENDOR_SMC, 0xa835
	},
	{	/* Intersil PRISM Indigo Wireless LAN adapter" */
	  PFF_DEV_ISL3877,
	  PCI_VENDOR_INTERSIL, PCI_PRODUCT_INTERSIL_ISL3877,
	  0, 0
	},
	{	/* Intersil PRISM Duette/Prism GT Wireless LAN adapter" */
	  PFF_DEV_ISL3890,
	  PCI_VENDOR_INTERSIL, PCI_PRODUCT_INTERSIL_ISL3890,
	  0, 0
	},
	{ 0, 0, 0, 0, 0 }
};

static const struct pgt_ident *
pgt_match(struct pci_attach_args *pa)
{
	u_int32_t subsysid;
	int i;

	subsysid = pci_conf_read(pa->pa_pc, pa->pa_tag,
	    PCI_SUBSYS_ID_REG);
	for (i = 0; pgt_ident_table[i].dev_type; i++) {
		if (PCI_VENDOR(pa->pa_id) == pgt_ident_table[i].vendor &&
		    PCI_PRODUCT(pa->pa_id) == pgt_ident_table[i].device &&
		    (pgt_ident_table[i].subvendor == 0x0000 ||
		    PCI_VENDOR(subsysid) == pgt_ident_table[i].subvendor) &&
		    (pgt_ident_table[i].subdevice == 0x0000 ||
		    PCI_PRODUCT(subsysid) == pgt_ident_table[i].subdevice))
			return &pgt_ident_table[i];
	}
	return (NULL);
}

int
pgt_pci_match(struct device *parent, void *match, void *aux)
{
	struct pci_attach_args *pa = aux;
	const struct pgt_ident *id;

	id = pgt_match(pa);
	if (id == NULL)
		return (0);
	return (1);
}

void
pgt_load_busaddr(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	if (error == 0)
		*(bus_addr_t *)arg = segs->ds_addr;
}

void
pgt_pci_attach(struct device *parent, struct device *self, void *aux)
{
	struct pci_attach_args *pa = aux;
	const struct pgt_ident *id;
	struct pgt_softc *sc = (void *)self;
	void *vaddr;
	size_t size;
	int error, i, rid;

	id = pgt_match(pa);
	sc->sc_dev_type = id->dev_type;
	for (i = 0; i < PFF_QUEUE_COUNT; i++) {
		TAILQ_INIT(&sc->sc_freeq[i]);
		TAILQ_INIT(&sc->sc_dirtyq[i]);
	}
	pci_enable_busmaster(dev);
	rid = 0;
	sc->sc_intres = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE | RF_SHAREABLE);
	if (sc->sc_intres == NULL) {
		printf("%s: cannot allocate IRQ", sc->sc_dev.dv_xname);
		error = ENXIO;
		goto out;
	}
	rid = PCIR_BAR(0);
	sc->sc_iores = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->sc_iores == NULL) {
		printf("%s: cannot map IO\n", sc->sc_dev.dv_xname);
		error = ENXIO;
		goto out;
	}
	sc->sc_iotag = rman_get_bustag(sc->sc_iores);
	sc->sc_iohandle = rman_get_bushandle(sc->sc_iores);
	if (rman_get_size(sc->sc_iores) < PFF_DIRECT_MEMORY_OFFSET +
	    PFF_DIRECT_MEMORY_SIZE) {
		printf("%s: IO range too small (%lu)\n",
		    sc->sc_dev.dv_xname,
		    rman_get_size(sc->sc_iores));
		error = ENXIO;
		goto out;
	}
	size = sizeof(struct pgt_control_block);
	error = bus_dma_tag_create(NULL, 4, 0, BUS_SPACE_MAXADDR_32BIT,
	    BUS_SPACE_MAXADDR, NULL, NULL, size, 1, size,
	    BUS_DMA_ALLOCNOW, NULL, NULL, &sc->sc_cbdmat);
	if (error != 0) {
		printf("%s: could not set up control block tag: %d\n",
		    sc->sc_dev.dv_xname, error);
		goto out;
	}
	error = bus_dmamem_alloc(sc->sc_cbdmat, &vaddr,
	    BUS_DMA_WAITOK | BUS_DMA_COHERENT | BUS_DMA_ZERO, &sc->sc_cbdmam);
	if (error) {
		printf("%s: could not set up control block map: %d\n",
		    sc->sc_dev.dv_xname, error);
		goto out;
	}
	sc->sc_cb = vaddr;
	error = bus_dmamap_load(sc->sc_cbdmat, sc->sc_cbdmam, sc->sc_cb,
	    size, pgt_load_busaddr, &sc->sc_cbdmabusaddr, 0);
	if (error) {
		printf("%s: could not load control block: %d\n",
		    sc->sc_dev.dv_xname, error);
		goto out;
	}
	/*
	 * Allocate the power-saving mode frame buffering area.
	 */
	size = PFF_FRAG_SIZE * PFF_PSM_BUFFER_FRAME_COUNT;
	error = bus_dma_tag_create(NULL, 4, 0, BUS_SPACE_MAXADDR_32BIT,
	    BUS_SPACE_MAXADDR, NULL, NULL, size, 1, size,
	    BUS_DMA_ALLOCNOW, NULL, NULL, &sc->sc_psmdmat);
	if (error != 0) {
		printf("%s: could not set up psm tag: %d\n",
		    sc->sc_dev.dv_xname, error);
		goto out;
	}
	error = bus_dmamem_alloc(sc->sc_psmdmat, &vaddr,
	    BUS_DMA_WAITOK | BUS_DMA_COHERENT | BUS_DMA_ZERO, &sc->sc_psmdmam);
	if (error) {
		printf("%s: could not set up psm buffer: %d\n",
		    sc->sc_dev.dv_xname, error);
		goto out;
	}
	sc->sc_psmbuf = vaddr;
	error = bus_dmamap_load(sc->sc_psmdmat, sc->sc_psmdmam, sc->sc_psmbuf,
	    size, pgt_load_busaddr, &sc->sc_psmdmabusaddr, 0);
	if (error) {
		printf("%s: could not load psm buffer: %d\n",
		    sc->sc_dev.dv_xname, error);
		goto out;
	}
	/*
	 * Allocate an mbuf-cluster-sized DMA tag for every pgt_frag in
	 * the pgt_control_block (mirrored in the pgt_descqs).
	 */
	i = PFF_QUEUE_DATA_RX_SIZE + PFF_QUEUE_DATA_TX_SIZE +
	    PFF_QUEUE_DATA_RX_SIZE + PFF_QUEUE_DATA_TX_SIZE +
	    PFF_QUEUE_MGMT_SIZE + PFF_QUEUE_MGMT_SIZE;
	error = bus_dma_tag_create(NULL, 4, 0, BUS_SPACE_MAXADDR_32BIT,
	    BUS_SPACE_MAXADDR, NULL, NULL, PFF_FRAG_SIZE, i, PFF_FRAG_SIZE,
	    BUS_DMA_ALLOCNOW, NULL, NULL, &sc->sc_fragdmat);
	if (error != 0) {
		printf("%s: could not set up fragment tags: %d\n",
		    sc->sc_dev.dv_xname, error);
		goto out;
	}
	error = pgt_pci_attach_queue(sc, PFF_QUEUE_DATA_LOW_RX);
	if (error == 0)
		error = pgt_pci_attach_queue(sc, PFF_QUEUE_DATA_LOW_TX);
	if (error == 0)
		error = pgt_pci_attach_queue(sc, PFF_QUEUE_DATA_HIGH_RX);
	if (error == 0)
		error = pgt_pci_attach_queue(sc, PFF_QUEUE_DATA_HIGH_TX);
	if (error == 0)
		error = pgt_pci_attach_queue(sc, PFF_QUEUE_MGMT_RX);
	if (error == 0)
		error = pgt_pci_attach_queue(sc, PFF_QUEUE_MGMT_TX);
	if (error != 0)
		goto out;
	error = pgt_attach(dev);
out:
	if (error)
		pgt_pci_release(dev, sc);
	return (error);
}

static void
pgt_pci_release(device_t dev, struct pgt_softc *sc)
{
	if (sc->sc_fragdmat != NULL) {
		pgt_pci_detach_queue(sc, PFF_QUEUE_DATA_LOW_RX);
		pgt_pci_detach_queue(sc, PFF_QUEUE_DATA_LOW_TX);
		pgt_pci_detach_queue(sc, PFF_QUEUE_DATA_HIGH_RX);
		pgt_pci_detach_queue(sc, PFF_QUEUE_DATA_HIGH_TX);
		pgt_pci_detach_queue(sc, PFF_QUEUE_MGMT_RX);
		pgt_pci_detach_queue(sc, PFF_QUEUE_MGMT_TX);
		bus_dma_tag_destroy(sc->sc_fragdmat);
		sc->sc_fragdmat = NULL;
	}
	if (sc->sc_psmdmabusaddr != 0) {
		bus_dmamap_unload(sc->sc_psmdmat, sc->sc_psmdmam);
		sc->sc_psmdmabusaddr = 0;
	}
	if (sc->sc_psmbuf != NULL) {
		bus_dmamem_free(sc->sc_psmdmat, sc->sc_psmbuf, sc->sc_psmdmam);
		sc->sc_psmbuf = NULL;
		sc->sc_psmdmam = NULL;
	}
	if (sc->sc_psmdmat != NULL) {
		bus_dma_tag_destroy(sc->sc_psmdmat);
		sc->sc_psmdmat = NULL;
	}
	if (sc->sc_cbdmabusaddr != 0) {
		bus_dmamap_unload(sc->sc_psmdmat, sc->sc_psmdmam);
		sc->sc_cbdmabusaddr = 0;
	}
	if (sc->sc_cb != NULL) {
		bus_dmamem_free(sc->sc_cbdmat, sc->sc_cb, sc->sc_cbdmam);
		sc->sc_cb = NULL;
		sc->sc_cbdmam = NULL;
	}
	if (sc->sc_cbdmat != NULL) {
		bus_dma_tag_destroy(sc->sc_cbdmat);
		sc->sc_cbdmat = NULL;
	}
	if (sc->sc_iores != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY, PCIR_BAR(0),
		    sc->sc_iores);
		sc->sc_iores = NULL;
	}
	if (sc->sc_intres != NULL) {
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->sc_intres);
		sc->sc_intres = NULL;
	}
}

static int
pgt_pci_detach(device_t dev)
{
	int error;

	error = pgt_detach(dev);
	if (error)
		return (error);
	pgt_pci_release(dev, device_get_softc(dev));
	return (error);
}

static int
pgt_pci_shutdown(device_t dev)
{
	struct pgt_softc *sc;

	sc = device_get_softc(dev);
	pgt_reboot(sc);
	return (0);
}

static int
pgt_pci_attach_queue(struct pgt_softc *sc, enum pgt_queue pq)
{
	struct pgt_desc *pd;
	struct pgt_frag *pcbqueue;
	size_t i, qsize;
	int error;

	switch (pq) {
	case PFF_QUEUE_DATA_LOW_RX:
		pcbqueue = sc->sc_cb->pcb_data_low_rx;
		qsize = PFF_QUEUE_DATA_RX_SIZE;
		break;
	case PFF_QUEUE_DATA_LOW_TX:
		pcbqueue = sc->sc_cb->pcb_data_low_tx;
		qsize = PFF_QUEUE_DATA_TX_SIZE;
		break;
	case PFF_QUEUE_DATA_HIGH_RX:
		pcbqueue = sc->sc_cb->pcb_data_high_rx;
		qsize = PFF_QUEUE_DATA_RX_SIZE;
		break;
	case PFF_QUEUE_DATA_HIGH_TX:
		pcbqueue = sc->sc_cb->pcb_data_high_tx;
		qsize = PFF_QUEUE_DATA_TX_SIZE;
		break;
	case PFF_QUEUE_MGMT_RX:
		pcbqueue = sc->sc_cb->pcb_mgmt_rx;
		qsize = PFF_QUEUE_MGMT_SIZE;
		break;
	case PFF_QUEUE_MGMT_TX:
		pcbqueue = sc->sc_cb->pcb_mgmt_tx;
		qsize = PFF_QUEUE_MGMT_SIZE;
		break;
	}
	for (i = 0; i < qsize; i++) {
		pd = malloc(sizeof(*pd), M_PFF, M_WAITOK);
		bzero(pd, sizeof *pd);
		error = bus_dmamem_alloc(sc->sc_fragdmat,
		    &pd->pd_mem, BUS_DMA_WAITOK | BUS_DMA_ZERO,
		    &pd->pd_dmam);
		if (error) {
			printf("%s: error allocating fragment "
			    "%u on queue %u: %d\n",
			    sc->sc_dev.dv_xname, i, pq, error);
			free(pd, M_PFF);
			break;
		}
		if (pgt_queue_is_rx(pq)) {
			error = bus_dmamap_load(sc->sc_fragdmat, pd->pd_dmam,
			    pd->pd_mem, PFF_FRAG_SIZE, pgt_load_busaddr,
			    &pd->pd_dmaaddr, 0);
			if (error) {
				printf("%s: error loading "
				    "fragment %u on queue %u: %d\n",
				    sc->sc_dev.dv_xname, i, pq, error);
				bus_dmamem_free(sc->sc_fragdmat, pd->pd_mem,
				    pd->pd_dmam);
				free(pd, M_PFF);
				break;
			}
		}
		TAILQ_INSERT_TAIL(&sc->sc_freeq[pq], pd, pd_link);
	}
	return (error);
}

static void
pgt_pci_detach_queue(struct pgt_softc *sc, enum pgt_queue pq)
{
	struct pgt_desc *pd;
	while (!TAILQ_EMPTY(&sc->sc_freeq[pq])) {
		pd = TAILQ_FIRST(&sc->sc_freeq[pq]);
		TAILQ_REMOVE_HEAD(&sc->sc_freeq[pq], pd_link);
		if (pd->pd_dmaaddr != 0) {
			bus_dmamap_unload(sc->sc_fragdmat, pd->pd_dmam);
			pd->pd_dmaaddr = 0;
		}
		bus_dmamem_free(sc->sc_fragdmat, pd->pd_mem, pd->pd_dmam);
		free(pd, M_PFF);
	}
}
#endif
