/*	$OpenBSD: if_rtwn.c,v 1.19 2016/03/15 10:28:32 stsp Exp $	*/

/*-
 * Copyright (c) 2010 Damien Bergamini <damien.bergamini@free.fr>
 * Copyright (c) 2015 Stefan Sperling <stsp@openbsd.org>
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
 * PCI front-end for Realtek RTL8188CE driver.
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/task.h>
#include <sys/timeout.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/endian.h>

#include <machine/bus.h>
#include <machine/intr.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_radiotap.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/ic/r92creg.h>
#include <dev/ic/rtwnvar.h>

/*
 * Driver definitions.
 */

#define R92C_PUBQ_NPAGES	176
#define R92C_HPQ_NPAGES		41
#define R92C_LPQ_NPAGES		28
#define R92C_TXPKTBUF_COUNT	256
#define R92C_TX_PAGE_COUNT	\
	(R92C_PUBQ_NPAGES + R92C_HPQ_NPAGES + R92C_LPQ_NPAGES)
#define R92C_TX_PAGE_BOUNDARY	(R92C_TX_PAGE_COUNT + 1)

#define RTWN_NTXQUEUES			9
#define RTWN_RX_LIST_COUNT		256
#define RTWN_TX_LIST_COUNT		256

/* TX queue indices. */
#define RTWN_BK_QUEUE			0
#define RTWN_BE_QUEUE			1
#define RTWN_VI_QUEUE			2
#define RTWN_VO_QUEUE			3
#define RTWN_BEACON_QUEUE		4
#define RTWN_TXCMD_QUEUE		5
#define RTWN_MGNT_QUEUE			6
#define RTWN_HIGH_QUEUE			7
#define RTWN_HCCA_QUEUE			8

struct rtwn_rx_radiotap_header {
	struct ieee80211_radiotap_header wr_ihdr;
	uint8_t		wr_flags;
	uint8_t		wr_rate;
	uint16_t	wr_chan_freq;
	uint16_t	wr_chan_flags;
	uint8_t		wr_dbm_antsignal;
} __packed;

#define RTWN_RX_RADIOTAP_PRESENT			\
	(1 << IEEE80211_RADIOTAP_FLAGS |		\
	 1 << IEEE80211_RADIOTAP_RATE |			\
	 1 << IEEE80211_RADIOTAP_CHANNEL |		\
	 1 << IEEE80211_RADIOTAP_DBM_ANTSIGNAL)

struct rtwn_tx_radiotap_header {
	struct ieee80211_radiotap_header wt_ihdr;
	uint8_t		wt_flags;
	uint16_t	wt_chan_freq;
	uint16_t	wt_chan_flags;
} __packed;

#define RTWN_TX_RADIOTAP_PRESENT			\
	(1 << IEEE80211_RADIOTAP_FLAGS |		\
	 1 << IEEE80211_RADIOTAP_CHANNEL)

struct rtwn_rx_data {
	bus_dmamap_t		map;
	struct mbuf		*m;
};

struct rtwn_rx_ring {
	struct r92c_rx_desc_pci	*desc;
	bus_dmamap_t		map;
	bus_dma_segment_t	seg;
	int			nsegs;
	struct rtwn_rx_data	rx_data[RTWN_RX_LIST_COUNT];

};
struct rtwn_tx_data {
	bus_dmamap_t			map;
	struct mbuf			*m;
	struct ieee80211_node		*ni;
};

struct rtwn_tx_ring {
	bus_dmamap_t		map;
	bus_dma_segment_t	seg;
	int			nsegs;
	struct r92c_tx_desc_pci	*desc;
	struct rtwn_tx_data	tx_data[RTWN_TX_LIST_COUNT];
	int			queued;
	int			cur;
};

struct rtwn_pci_softc {
	struct device		sc_dev;
	struct rtwn_softc	sc_sc;

	struct rtwn_rx_ring	rx_ring;
	struct rtwn_tx_ring	tx_ring[RTWN_NTXQUEUES];
	uint32_t		qfullmsk;

	struct timeout		calib_to;
	struct timeout		scan_to;

	/* PCI specific goo. */
	bus_dma_tag_t 		sc_dmat;
	pci_chipset_tag_t	sc_pc;
	pcitag_t		sc_tag;
	void			*sc_ih;
	bus_space_tag_t		sc_st;
	bus_space_handle_t	sc_sh;
	bus_size_t		sc_mapsize;
	int			sc_cap_off;

#if NBPFILTER > 0
	caddr_t				sc_drvbpf;

	union {
		struct rtwn_rx_radiotap_header th;
		uint8_t	pad[64];
	}				sc_rxtapu;
#define sc_rxtap	sc_rxtapu.th
	int				sc_rxtap_len;

	union {
		struct rtwn_tx_radiotap_header th;
		uint8_t	pad[64];
	}				sc_txtapu;
#define sc_txtap	sc_txtapu.th
	int				sc_txtap_len;
#endif
};

#ifdef RTWN_DEBUG
#define DPRINTF(x)	do { if (rtwn_debug) printf x; } while (0)
#define DPRINTFN(n, x)	do { if (rtwn_debug >= (n)) printf x; } while (0)
extern int rtwn_debug;
#else
#define DPRINTF(x)
#define DPRINTFN(n, x)
#endif

/*
 * PCI configuration space registers.
 */
#define	RTWN_PCI_IOBA		0x10	/* i/o mapped base */
#define	RTWN_PCI_MMBA		0x18	/* memory mapped base */

#define RTWN_INT_ENABLE	(R92C_IMR_ROK | R92C_IMR_VODOK | R92C_IMR_VIDOK | \
			R92C_IMR_BEDOK | R92C_IMR_BKDOK | R92C_IMR_MGNTDOK | \
			R92C_IMR_HIGHDOK | R92C_IMR_BDOK | R92C_IMR_RDU | \
			R92C_IMR_RXFOVW)

static const struct pci_matchid rtwn_pci_devices[] = {
	{ PCI_VENDOR_REALTEK,	PCI_PRODUCT_REALTEK_RT8188 }
};

int		rtwn_pci_match(struct device *, void *, void *);
void		rtwn_pci_attach(struct device *, struct device *, void *);
int		rtwn_pci_detach(struct device *, int);
int		rtwn_pci_activate(struct device *, int);
int		rtwn_alloc_rx_list(struct rtwn_pci_softc *);
void		rtwn_reset_rx_list(struct rtwn_pci_softc *);
void		rtwn_free_rx_list(struct rtwn_pci_softc *);
void		rtwn_setup_rx_desc(struct rtwn_pci_softc *,
		    struct r92c_rx_desc_pci *, bus_addr_t, size_t, int);
int		rtwn_alloc_tx_list(struct rtwn_pci_softc *, int);
void		rtwn_reset_tx_list(struct rtwn_pci_softc *, int);
void		rtwn_free_tx_list(struct rtwn_pci_softc *, int);
void		rtwn_pci_write_1(void *, uint16_t, uint8_t);
void		rtwn_pci_write_2(void *, uint16_t, uint16_t);
void		rtwn_pci_write_4(void *, uint16_t, uint32_t);
uint8_t		rtwn_pci_read_1(void *, uint16_t);
uint16_t	rtwn_pci_read_2(void *, uint16_t);
uint32_t	rtwn_pci_read_4(void *, uint16_t);
void		rtwn_rx_frame(struct rtwn_pci_softc *,
		    struct r92c_rx_desc_pci *, struct rtwn_rx_data *, int);
int		rtwn_tx(void *, struct mbuf *, struct ieee80211_node *);
void		rtwn_tx_done(struct rtwn_pci_softc *, int);
void		rtwn_pci_stop(void *);
int		rtwn_intr(void *);
int		rtwn_is_oactive(void *);
int		rtwn_llt_write(struct rtwn_pci_softc *, uint32_t, uint32_t);
int		rtwn_llt_init(struct rtwn_pci_softc *);
int		rtwn_dma_init(void *);
void		rtwn_enable_intr(void *);
void		rtwn_disable_intr(void *);
void		rtwn_calib_to(void *);
void		rtwn_next_calib(void *);
void		rtwn_cancel_calib(void *);
void		rtwn_scan_to(void *);
void		rtwn_pci_next_scan(void *);
void		rtwn_cancel_scan(void *);

struct cfdriver rtwn_cd = {
	NULL, "rtwn", DV_IFNET
};

const struct cfattach rtwn_pci_ca = {
	sizeof(struct rtwn_pci_softc),
	rtwn_pci_match,
	rtwn_pci_attach,
	rtwn_pci_detach,
	rtwn_pci_activate
};

int
rtwn_pci_match(struct device *parent, void *match, void *aux)
{
	return (pci_matchbyid(aux, rtwn_pci_devices,
	    nitems(rtwn_pci_devices)));
}

void
rtwn_pci_attach(struct device *parent, struct device *self, void *aux)
{
	struct rtwn_pci_softc *sc = (struct rtwn_pci_softc*)self;
	struct pci_attach_args *pa = aux;
	struct ifnet *ifp;
	int i, error;
	pcireg_t memtype;
	pci_intr_handle_t ih;
	const char *intrstr;

	sc->sc_dmat = pa->pa_dmat;
	sc->sc_pc = pa->pa_pc;
	sc->sc_tag = pa->pa_tag;

	timeout_set(&sc->calib_to, rtwn_calib_to, sc);
	timeout_set(&sc->scan_to, rtwn_scan_to, sc);

	pci_set_powerstate(pa->pa_pc, pa->pa_tag, PCI_PMCSR_STATE_D0);

	/* Map control/status registers. */
	memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, RTWN_PCI_MMBA);
	error = pci_mapreg_map(pa, RTWN_PCI_MMBA, memtype, 0, &sc->sc_st,
	    &sc->sc_sh, NULL, &sc->sc_mapsize, 0);
	if (error != 0) {
		printf(": can't map mem space\n");
		return;
	}

	if (pci_intr_map_msi(pa, &ih) && pci_intr_map(pa, &ih)) {
		printf(": can't map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(sc->sc_pc, ih);
	sc->sc_ih = pci_intr_establish(sc->sc_pc, ih, IPL_NET,
	    rtwn_intr, sc, sc->sc_dev.dv_xname);
	if (sc->sc_ih == NULL) {
		printf(": can't establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}
	printf(": %s\n", intrstr);

	/* Disable PCIe Active State Power Management (ASPM). */
	if (pci_get_capability(sc->sc_pc, sc->sc_tag, PCI_CAP_PCIEXPRESS,
	    &sc->sc_cap_off, NULL)) {
		uint32_t lcsr = pci_conf_read(sc->sc_pc, sc->sc_tag,
		    sc->sc_cap_off + PCI_PCIE_LCSR);
		lcsr &= ~(PCI_PCIE_LCSR_ASPM_L0S | PCI_PCIE_LCSR_ASPM_L1);
		pci_conf_write(sc->sc_pc, sc->sc_tag,
		    sc->sc_cap_off + PCI_PCIE_LCSR, lcsr);
	}

	/* Allocate Tx/Rx buffers. */
	error = rtwn_alloc_rx_list(sc);
	if (error != 0) {
		printf("%s: could not allocate Rx buffers\n",
		    sc->sc_dev.dv_xname);
		return;
	}
	for (i = 0; i < RTWN_NTXQUEUES; i++) {
		error = rtwn_alloc_tx_list(sc, i);
		if (error != 0) {
			printf("%s: could not allocate Tx buffers\n",
			    sc->sc_dev.dv_xname);
			rtwn_free_rx_list(sc);
			return;
		}
	}

	/* Attach the bus-agnostic driver. */
	sc->sc_sc.sc_ops.cookie = sc;
	sc->sc_sc.sc_ops.write_1 = rtwn_pci_write_1;
	sc->sc_sc.sc_ops.write_2 = rtwn_pci_write_2;
	sc->sc_sc.sc_ops.write_4 = rtwn_pci_write_4;
	sc->sc_sc.sc_ops.read_1 = rtwn_pci_read_1;
	sc->sc_sc.sc_ops.read_2 = rtwn_pci_read_2;
	sc->sc_sc.sc_ops.read_4 = rtwn_pci_read_4;
	sc->sc_sc.sc_ops.tx = rtwn_tx;
	sc->sc_sc.sc_ops.dma_init = rtwn_dma_init;
	sc->sc_sc.sc_ops.enable_intr = rtwn_enable_intr;
	sc->sc_sc.sc_ops.disable_intr = rtwn_disable_intr;
	sc->sc_sc.sc_ops.stop = rtwn_pci_stop;
	sc->sc_sc.sc_ops.is_oactive = rtwn_is_oactive;
	sc->sc_sc.sc_ops.next_calib = rtwn_next_calib;
	sc->sc_sc.sc_ops.cancel_calib = rtwn_cancel_calib;
	sc->sc_sc.sc_ops.next_scan = rtwn_pci_next_scan;
	sc->sc_sc.sc_ops.cancel_scan = rtwn_cancel_scan;

	error = rtwn_attach(&sc->sc_dev, &sc->sc_sc);
	if (error != 0) {
		rtwn_free_rx_list(sc);
		for (i = 0; i < RTWN_NTXQUEUES; i++)
			rtwn_free_tx_list(sc, i);
		return;
	}

	/* ifp is now valid */
	ifp = &sc->sc_sc.sc_ic.ic_if;
#if NBPFILTER > 0
	bpfattach(&sc->sc_drvbpf, ifp, DLT_IEEE802_11_RADIO,
	    sizeof(struct ieee80211_frame) + IEEE80211_RADIOTAP_HDRLEN);

	sc->sc_rxtap_len = sizeof(sc->sc_rxtapu);
	sc->sc_rxtap.wr_ihdr.it_len = htole16(sc->sc_rxtap_len);
	sc->sc_rxtap.wr_ihdr.it_present = htole32(RTWN_RX_RADIOTAP_PRESENT);

	sc->sc_txtap_len = sizeof(sc->sc_txtapu);
	sc->sc_txtap.wt_ihdr.it_len = htole16(sc->sc_txtap_len);
	sc->sc_txtap.wt_ihdr.it_present = htole32(RTWN_TX_RADIOTAP_PRESENT);
#endif
}

int
rtwn_pci_detach(struct device *self, int flags)
{
	struct rtwn_pci_softc *sc = (struct rtwn_pci_softc *)self;
	int s, i;

	s = splnet();

	if (timeout_initialized(&sc->calib_to))
		timeout_del(&sc->calib_to);
	if (timeout_initialized(&sc->scan_to))
		timeout_del(&sc->scan_to);

	rtwn_detach(&sc->sc_sc, flags);

	/* Free Tx/Rx buffers. */
	for (i = 0; i < RTWN_NTXQUEUES; i++)
		rtwn_free_tx_list(sc, i);
	rtwn_free_rx_list(sc);
	splx(s);

	return (0);
}

int
rtwn_pci_activate(struct device *self, int act)
{
	struct rtwn_pci_softc *sc = (struct rtwn_pci_softc *)self;

	return rtwn_activate(&sc->sc_sc, act);
}

void
rtwn_setup_rx_desc(struct rtwn_pci_softc *sc, struct r92c_rx_desc_pci *desc,
    bus_addr_t addr, size_t len, int idx)
{
	memset(desc, 0, sizeof(*desc));
	desc->rxdw0 = htole32(SM(R92C_RXDW0_PKTLEN, len) |
		((idx == RTWN_RX_LIST_COUNT - 1) ? R92C_RXDW0_EOR : 0));
	desc->rxbufaddr = htole32(addr);
	bus_space_barrier(sc->sc_st, sc->sc_sh, 0, sc->sc_mapsize,
	    BUS_SPACE_BARRIER_WRITE);
	desc->rxdw0 |= htole32(R92C_RXDW0_OWN);
}

int
rtwn_alloc_rx_list(struct rtwn_pci_softc *sc)
{
	struct rtwn_rx_ring *rx_ring = &sc->rx_ring;
	struct rtwn_rx_data *rx_data;
	size_t size;
	int i, error = 0;

	/* Allocate Rx descriptors. */
	size = sizeof(struct r92c_rx_desc_pci) * RTWN_RX_LIST_COUNT;
	error = bus_dmamap_create(sc->sc_dmat, size, 1, size, 0, BUS_DMA_NOWAIT,
		&rx_ring->map);
	if (error != 0) {
		printf("%s: could not create rx desc DMA map\n",
		    sc->sc_dev.dv_xname);
		rx_ring->map = NULL;
		goto fail;
	}

	error = bus_dmamem_alloc(sc->sc_dmat, size, 0, 0, &rx_ring->seg, 1,
	    &rx_ring->nsegs, BUS_DMA_NOWAIT | BUS_DMA_ZERO);
	if (error != 0) {
		printf("%s: could not allocate rx desc\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	error = bus_dmamem_map(sc->sc_dmat, &rx_ring->seg, rx_ring->nsegs,
	    size, (caddr_t *)&rx_ring->desc, 
	    BUS_DMA_NOWAIT | BUS_DMA_COHERENT);
	if (error != 0) {
		bus_dmamem_free(sc->sc_dmat, &rx_ring->seg, rx_ring->nsegs);
		rx_ring->desc = NULL;
		printf("%s: could not map rx desc\n", sc->sc_dev.dv_xname);
		goto fail;
	}

	error = bus_dmamap_load_raw(sc->sc_dmat, rx_ring->map, &rx_ring->seg,
	    1, size, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not load rx desc\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	bus_dmamap_sync(sc->sc_dmat, rx_ring->map, 0, size,
	    BUS_DMASYNC_PREWRITE);

	/* Allocate Rx buffers. */
	for (i = 0; i < RTWN_RX_LIST_COUNT; i++) {
		rx_data = &rx_ring->rx_data[i];

		error = bus_dmamap_create(sc->sc_dmat, MCLBYTES, 1, MCLBYTES,
		    0, BUS_DMA_NOWAIT, &rx_data->map);
		if (error != 0) {
			printf("%s: could not create rx buf DMA map\n",
			    sc->sc_dev.dv_xname);
			goto fail;
		}

		rx_data->m = MCLGETI(NULL, M_DONTWAIT, NULL, MCLBYTES);
		if (rx_data->m == NULL) {
			printf("%s: could not allocate rx mbuf\n",
			    sc->sc_dev.dv_xname);
			error = ENOMEM;
			goto fail;
		}

		error = bus_dmamap_load(sc->sc_dmat, rx_data->map,
		    mtod(rx_data->m, void *), MCLBYTES, NULL,
		    BUS_DMA_NOWAIT | BUS_DMA_READ);
		if (error != 0) {
			printf("%s: could not load rx buf DMA map\n",
			    sc->sc_dev.dv_xname);
			goto fail;
		}

		rtwn_setup_rx_desc(sc, &rx_ring->desc[i],
		    rx_data->map->dm_segs[0].ds_addr, MCLBYTES, i);
	}
fail:	if (error != 0)
		rtwn_free_rx_list(sc);
	return (error);
}

void
rtwn_reset_rx_list(struct rtwn_pci_softc *sc)
{
	struct rtwn_rx_ring *rx_ring = &sc->rx_ring;
	struct rtwn_rx_data *rx_data;
	int i;

	for (i = 0; i < RTWN_RX_LIST_COUNT; i++) {
		rx_data = &rx_ring->rx_data[i];
		rtwn_setup_rx_desc(sc, &rx_ring->desc[i],
		    rx_data->map->dm_segs[0].ds_addr, MCLBYTES, i);
	}
}

void
rtwn_free_rx_list(struct rtwn_pci_softc *sc)
{
	struct rtwn_rx_ring *rx_ring = &sc->rx_ring;
	struct rtwn_rx_data *rx_data;
	int i, s;

	s = splnet();

	if (rx_ring->map) {
		if (rx_ring->desc) {
			bus_dmamap_unload(sc->sc_dmat, rx_ring->map);
			bus_dmamem_unmap(sc->sc_dmat, (caddr_t)rx_ring->desc,
			    sizeof (struct r92c_rx_desc_pci) *
			    RTWN_RX_LIST_COUNT);
			bus_dmamem_free(sc->sc_dmat, &rx_ring->seg,
			    rx_ring->nsegs);
			rx_ring->desc = NULL;
		}
		bus_dmamap_destroy(sc->sc_dmat, rx_ring->map);
		rx_ring->map = NULL;
	}

	for (i = 0; i < RTWN_RX_LIST_COUNT; i++) {
		rx_data = &rx_ring->rx_data[i];

		if (rx_data->m != NULL) {
			bus_dmamap_unload(sc->sc_dmat, rx_data->map);
			m_freem(rx_data->m);
			rx_data->m = NULL;
		}
		bus_dmamap_destroy(sc->sc_dmat, rx_data->map);
		rx_data->map = NULL;
	}

	splx(s);
}

int
rtwn_alloc_tx_list(struct rtwn_pci_softc *sc, int qid)
{
	struct rtwn_tx_ring *tx_ring = &sc->tx_ring[qid];
	struct rtwn_tx_data *tx_data;
	int i = 0, error = 0;

	error = bus_dmamap_create(sc->sc_dmat,
	    sizeof (struct r92c_tx_desc_pci) * RTWN_TX_LIST_COUNT, 1,
	    sizeof (struct r92c_tx_desc_pci) * RTWN_TX_LIST_COUNT, 0,
	    BUS_DMA_NOWAIT, &tx_ring->map);
	if (error != 0) {
		printf("%s: could not create tx ring DMA map\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	error = bus_dmamem_alloc(sc->sc_dmat,
	    sizeof (struct r92c_tx_desc_pci) * RTWN_TX_LIST_COUNT, PAGE_SIZE, 0,
	    &tx_ring->seg, 1, &tx_ring->nsegs, BUS_DMA_NOWAIT | BUS_DMA_ZERO);
	if (error != 0) {
		printf("%s: could not allocate tx ring DMA memory\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	error = bus_dmamem_map(sc->sc_dmat, &tx_ring->seg, tx_ring->nsegs,
	    sizeof (struct r92c_tx_desc_pci) * RTWN_TX_LIST_COUNT,
	    (caddr_t *)&tx_ring->desc, BUS_DMA_NOWAIT);
	if (error != 0) {
		bus_dmamem_free(sc->sc_dmat, &tx_ring->seg, tx_ring->nsegs);
		printf("%s: can't map tx ring DMA memory\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	error = bus_dmamap_load(sc->sc_dmat, tx_ring->map, tx_ring->desc,
	    sizeof (struct r92c_tx_desc_pci) * RTWN_TX_LIST_COUNT, NULL,
	    BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not load tx ring DMA map\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	for (i = 0; i < RTWN_TX_LIST_COUNT; i++) {
		struct r92c_tx_desc_pci *desc = &tx_ring->desc[i];

		/* setup tx desc */
		desc->nextdescaddr = htole32(tx_ring->map->dm_segs[0].ds_addr
		  + sizeof(struct r92c_tx_desc_pci)
		  * ((i + 1) % RTWN_TX_LIST_COUNT));

		tx_data = &tx_ring->tx_data[i];
		error = bus_dmamap_create(sc->sc_dmat, MCLBYTES, 1, MCLBYTES,
		    0, BUS_DMA_NOWAIT, &tx_data->map);
		if (error != 0) {
			printf("%s: could not create tx buf DMA map\n",
			    sc->sc_dev.dv_xname);
			goto fail;
		}
		tx_data->m = NULL;
		tx_data->ni = NULL;
	}
fail:
	if (error != 0)
		rtwn_free_tx_list(sc, qid);
	return (error);
}

void
rtwn_reset_tx_list(struct rtwn_pci_softc *sc, int qid)
{
	struct ieee80211com *ic = &sc->sc_sc.sc_ic;
	struct rtwn_tx_ring *tx_ring = &sc->tx_ring[qid];
	int i;

	for (i = 0; i < RTWN_TX_LIST_COUNT; i++) {
		struct r92c_tx_desc_pci *desc = &tx_ring->desc[i];
		struct rtwn_tx_data *tx_data = &tx_ring->tx_data[i];

		memset(desc, 0, sizeof(*desc) -
		    (sizeof(desc->reserved) + sizeof(desc->nextdescaddr64) +
		    sizeof(desc->nextdescaddr)));

		if (tx_data->m != NULL) {
			bus_dmamap_unload(sc->sc_dmat, tx_data->map);
			m_freem(tx_data->m);
			tx_data->m = NULL;
			ieee80211_release_node(ic, tx_data->ni);
			tx_data->ni = NULL;
		}
	}

	bus_dmamap_sync(sc->sc_dmat, tx_ring->map, 0, MCLBYTES,
	    BUS_DMASYNC_POSTWRITE);

	sc->qfullmsk &= ~(1 << qid);
	tx_ring->queued = 0;
	tx_ring->cur = 0;
}

void
rtwn_free_tx_list(struct rtwn_pci_softc *sc, int qid)
{
	struct rtwn_tx_ring *tx_ring = &sc->tx_ring[qid];
	struct rtwn_tx_data *tx_data;
	int i;

	if (tx_ring->map != NULL) {
		if (tx_ring->desc != NULL) {
			bus_dmamap_unload(sc->sc_dmat, tx_ring->map);
			bus_dmamem_unmap(sc->sc_dmat, (caddr_t)tx_ring->desc,
			    sizeof (struct r92c_tx_desc_pci) *
			    RTWN_TX_LIST_COUNT);
			bus_dmamem_free(sc->sc_dmat, &tx_ring->seg, tx_ring->nsegs);
		}
		bus_dmamap_destroy(sc->sc_dmat, tx_ring->map);
	}

	for (i = 0; i < RTWN_TX_LIST_COUNT; i++) {
		tx_data = &tx_ring->tx_data[i];

		if (tx_data->m != NULL) {
			bus_dmamap_unload(sc->sc_dmat, tx_data->map);
			m_freem(tx_data->m);
			tx_data->m = NULL;
		}
		bus_dmamap_destroy(sc->sc_dmat, tx_data->map);
	}

	sc->qfullmsk &= ~(1 << qid);
	tx_ring->queued = 0;
	tx_ring->cur = 0;
}

void
rtwn_pci_write_1(void *cookie, uint16_t addr, uint8_t val)
{
	struct rtwn_pci_softc *sc = cookie;

	bus_space_write_1(sc->sc_st, sc->sc_sh, addr, val);
}

void
rtwn_pci_write_2(void *cookie, uint16_t addr, uint16_t val)
{
	struct rtwn_pci_softc *sc = cookie;

	val = htole16(val);
	bus_space_write_2(sc->sc_st, sc->sc_sh, addr, val);
}

void
rtwn_pci_write_4(void *cookie, uint16_t addr, uint32_t val)
{
	struct rtwn_pci_softc *sc = cookie;

	val = htole32(val);
	bus_space_write_4(sc->sc_st, sc->sc_sh, addr, val);
}

uint8_t
rtwn_pci_read_1(void *cookie, uint16_t addr)
{
	struct rtwn_pci_softc *sc = cookie;

	return bus_space_read_1(sc->sc_st, sc->sc_sh, addr);
}

uint16_t
rtwn_pci_read_2(void *cookie, uint16_t addr)
{
	struct rtwn_pci_softc *sc = cookie;

	return bus_space_read_2(sc->sc_st, sc->sc_sh, addr);
}

uint32_t
rtwn_pci_read_4(void *cookie, uint16_t addr)
{
	struct rtwn_pci_softc *sc = cookie;

	return bus_space_read_4(sc->sc_st, sc->sc_sh, addr);
}

void
rtwn_rx_frame(struct rtwn_pci_softc *sc, struct r92c_rx_desc_pci *rx_desc,
    struct rtwn_rx_data *rx_data, int desc_idx)
{
	struct ieee80211com *ic = &sc->sc_sc.sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct ieee80211_rxinfo rxi;
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni;
	struct r92c_rx_phystat *phy = NULL;
	uint32_t rxdw0, rxdw3;
	struct mbuf *m, *m1;
	uint8_t rate;
	int8_t rssi = 0;
	int infosz, pktlen, shift, error;

	rxdw0 = letoh32(rx_desc->rxdw0);
	rxdw3 = letoh32(rx_desc->rxdw3);

	if (__predict_false(rxdw0 & (R92C_RXDW0_CRCERR | R92C_RXDW0_ICVERR))) {
		/*
		 * This should not happen since we setup our Rx filter
		 * to not receive these frames.
		 */
		ifp->if_ierrors++;
		return;
	}

	pktlen = MS(rxdw0, R92C_RXDW0_PKTLEN);
	if (__predict_false(pktlen < sizeof(*wh) || pktlen > MCLBYTES)) {
		ifp->if_ierrors++;
		return;
	}

	rate = MS(rxdw3, R92C_RXDW3_RATE);
	infosz = MS(rxdw0, R92C_RXDW0_INFOSZ) * 8;
	if (infosz > sizeof(struct r92c_rx_phystat))
		infosz = sizeof(struct r92c_rx_phystat);
	shift = MS(rxdw0, R92C_RXDW0_SHIFT);

	/* Get RSSI from PHY status descriptor if present. */
	if (infosz != 0 && (rxdw0 & R92C_RXDW0_PHYST)) {
		phy = mtod(rx_data->m, struct r92c_rx_phystat *);
		rssi = rtwn_get_rssi(&sc->sc_sc, rate, phy);
		/* Update our average RSSI. */
		rtwn_update_avgrssi(&sc->sc_sc, rate, rssi);
	}

	DPRINTFN(5, ("Rx frame len=%d rate=%d infosz=%d shift=%d rssi=%d\n",
	    pktlen, rate, infosz, shift, rssi));

	m1 = MCLGETI(NULL, M_DONTWAIT, NULL, MCLBYTES);
	if (m1 == NULL) {
		ifp->if_ierrors++;
		return;
	}
	bus_dmamap_unload(sc->sc_dmat, rx_data->map);
	error = bus_dmamap_load(sc->sc_dmat, rx_data->map,
	    mtod(m1, void *), MCLBYTES, NULL,
	    BUS_DMA_NOWAIT | BUS_DMA_READ);
	if (error != 0) {
		m_freem(m1);

		if (bus_dmamap_load_mbuf(sc->sc_dmat, rx_data->map,
		    rx_data->m, BUS_DMA_NOWAIT))
			panic("%s: could not load old RX mbuf",
			    sc->sc_dev.dv_xname);

		/* Physical address may have changed. */
		rtwn_setup_rx_desc(sc, rx_desc,
		    rx_data->map->dm_segs[0].ds_addr, MCLBYTES, desc_idx);

		ifp->if_ierrors++;
		return;
	}

	/* Finalize mbuf. */
	m = rx_data->m;
	rx_data->m = m1;
	m->m_pkthdr.len = m->m_len = pktlen + infosz + shift;

	/* Update RX descriptor. */
	rtwn_setup_rx_desc(sc, rx_desc, rx_data->map->dm_segs[0].ds_addr,
	    MCLBYTES, desc_idx);

	/* Get ieee80211 frame header. */
	if (rxdw0 & R92C_RXDW0_PHYST)
		m_adj(m, infosz + shift);
	else
		m_adj(m, shift);
	wh = mtod(m, struct ieee80211_frame *);

#if NBPFILTER > 0
	if (__predict_false(sc->sc_drvbpf != NULL)) {
		struct rtwn_rx_radiotap_header *tap = &sc->sc_rxtap;
		struct mbuf mb;

		tap->wr_flags = 0;
		/* Map HW rate index to 802.11 rate. */
		tap->wr_flags = 2;
		if (!(rxdw3 & R92C_RXDW3_HT)) {
			switch (rate) {
			/* CCK. */
			case  0: tap->wr_rate =   2; break;
			case  1: tap->wr_rate =   4; break;
			case  2: tap->wr_rate =  11; break;
			case  3: tap->wr_rate =  22; break;
			/* OFDM. */
			case  4: tap->wr_rate =  12; break;
			case  5: tap->wr_rate =  18; break;
			case  6: tap->wr_rate =  24; break;
			case  7: tap->wr_rate =  36; break;
			case  8: tap->wr_rate =  48; break;
			case  9: tap->wr_rate =  72; break;
			case 10: tap->wr_rate =  96; break;
			case 11: tap->wr_rate = 108; break;
			}
		} else if (rate >= 12) {	/* MCS0~15. */
			/* Bit 7 set means HT MCS instead of rate. */
			tap->wr_rate = 0x80 | (rate - 12);
		}
		tap->wr_dbm_antsignal = rssi;
		tap->wr_chan_freq = htole16(ic->ic_ibss_chan->ic_freq);
		tap->wr_chan_flags = htole16(ic->ic_ibss_chan->ic_flags);

		mb.m_data = (caddr_t)tap;
		mb.m_len = sc->sc_rxtap_len;
		mb.m_next = m;
		mb.m_nextpkt = NULL;
		mb.m_type = 0;
		mb.m_flags = 0;
		bpf_mtap(sc->sc_drvbpf, &mb, BPF_DIRECTION_IN);
	}
#endif

	ni = ieee80211_find_rxnode(ic, wh);
	rxi.rxi_flags = 0;
	rxi.rxi_rssi = rssi;
	rxi.rxi_tstamp = 0;	/* Unused. */
	ieee80211_input(ifp, m, ni, &rxi);
	/* Node is no longer needed. */
	ieee80211_release_node(ic, ni);
}

int
rtwn_tx(void *cookie, struct mbuf *m, struct ieee80211_node *ni)
{
	struct rtwn_pci_softc *sc = cookie;
	struct ieee80211com *ic = &sc->sc_sc.sc_ic;
	struct ieee80211_frame *wh;
	struct ieee80211_key *k = NULL;
	struct rtwn_tx_ring *tx_ring;
	struct rtwn_tx_data *data;
	struct r92c_tx_desc_pci *txd;
	uint16_t qos;
	uint8_t raid, type, tid, qid;
	int hasqos, error;

	wh = mtod(m, struct ieee80211_frame *);
	type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;

	if (wh->i_fc[1] & IEEE80211_FC1_PROTECTED) {
		k = ieee80211_get_txkey(ic, wh, ni);
		if ((m = ieee80211_encrypt(ic, m, k)) == NULL)
			return (ENOBUFS);
		wh = mtod(m, struct ieee80211_frame *);
	}

	if ((hasqos = ieee80211_has_qos(wh))) {
		qos = ieee80211_get_qos(wh);
		tid = qos & IEEE80211_QOS_TID;
		qid = ieee80211_up_to_ac(ic, tid);
	} else if (type != IEEE80211_FC0_TYPE_DATA) {
		qid = RTWN_VO_QUEUE;
	} else
		qid = RTWN_BE_QUEUE;

	/* Grab a Tx buffer from the ring. */
	tx_ring = &sc->tx_ring[qid];
	data = &tx_ring->tx_data[tx_ring->cur];
	if (data->m != NULL) {
		m_freem(m);
		return (ENOBUFS);
	}

	/* Fill Tx descriptor. */
	txd = &tx_ring->desc[tx_ring->cur];
	if (htole32(txd->txdw0) & R92C_RXDW0_OWN) {
		m_freem(m);
		return (ENOBUFS);
	}
	txd->txdw0 = htole32(
	    SM(R92C_TXDW0_PKTLEN, m->m_pkthdr.len) |
	    SM(R92C_TXDW0_OFFSET, sizeof(*txd)) |
	    R92C_TXDW0_FSG | R92C_TXDW0_LSG);
	if (IEEE80211_IS_MULTICAST(wh->i_addr1))
		txd->txdw0 |= htole32(R92C_TXDW0_BMCAST);

	txd->txdw1 = 0;
#ifdef notyet
	if (k != NULL) {
		switch (k->k_cipher) {
		case IEEE80211_CIPHER_WEP40:
		case IEEE80211_CIPHER_WEP104:
		case IEEE80211_CIPHER_TKIP:
			cipher = R92C_TXDW1_CIPHER_RC4;
			break;
		case IEEE80211_CIPHER_CCMP:
			cipher = R92C_TXDW1_CIPHER_AES;
			break;
		default:
			cipher = R92C_TXDW1_CIPHER_NONE;
		}
		txd->txdw1 |= htole32(SM(R92C_TXDW1_CIPHER, cipher));
	}
#endif
	txd->txdw4 = 0;
	txd->txdw5 = 0;
	if (!IEEE80211_IS_MULTICAST(wh->i_addr1) &&
	    type == IEEE80211_FC0_TYPE_DATA) {
		if (ic->ic_curmode == IEEE80211_MODE_11B)
			raid = R92C_RAID_11B;
		else
			raid = R92C_RAID_11BG;
		txd->txdw1 |= htole32(
		    SM(R92C_TXDW1_MACID, R92C_MACID_BSS) |
		    SM(R92C_TXDW1_QSEL, R92C_TXDW1_QSEL_BE) |
		    SM(R92C_TXDW1_RAID, raid) |
		    R92C_TXDW1_AGGBK);

		if (ic->ic_flags & IEEE80211_F_USEPROT) {
			if (ic->ic_protmode == IEEE80211_PROT_CTSONLY) {
				txd->txdw4 |= htole32(R92C_TXDW4_CTS2SELF |
				    R92C_TXDW4_HWRTSEN);
			} else if (ic->ic_protmode == IEEE80211_PROT_RTSCTS) {
				txd->txdw4 |= htole32(R92C_TXDW4_RTSEN |
				    R92C_TXDW4_HWRTSEN);
			}
		}
		/* Send RTS at OFDM24. */
		txd->txdw4 |= htole32(SM(R92C_TXDW4_RTSRATE, 8));
		txd->txdw5 |= htole32(SM(R92C_TXDW5_RTSRATE_FBLIMIT, 0xf));
		/* Send data at OFDM54. */
		txd->txdw5 |= htole32(SM(R92C_TXDW5_DATARATE, 11));
		txd->txdw5 |= htole32(SM(R92C_TXDW5_DATARATE_FBLIMIT, 0x1f));

	} else {
		txd->txdw1 |= htole32(
		    SM(R92C_TXDW1_MACID, 0) |
		    SM(R92C_TXDW1_QSEL, R92C_TXDW1_QSEL_MGNT) |
		    SM(R92C_TXDW1_RAID, R92C_RAID_11B));

		/* Force CCK1. */
		txd->txdw4 |= htole32(R92C_TXDW4_DRVRATE);
		txd->txdw5 |= htole32(SM(R92C_TXDW5_DATARATE, 0));
	}
	/* Set sequence number (already little endian). */
	txd->txdseq = *(uint16_t *)wh->i_seq;

	if (!hasqos) {
		/* Use HW sequence numbering for non-QoS frames. */
		txd->txdw4  |= htole32(R92C_TXDW4_HWSEQ);
		txd->txdseq |= htole16(0x8000);		/* WTF? */
	} else
		txd->txdw4 |= htole32(R92C_TXDW4_QOS);

	error = bus_dmamap_load_mbuf(sc->sc_dmat, data->map, m,
	    BUS_DMA_NOWAIT | BUS_DMA_WRITE);
	if (error && error != EFBIG) {
		printf("%s: can't map mbuf (error %d)\n",
		    sc->sc_dev.dv_xname, error);
		m_freem(m);
		return error;
	}
	if (error != 0) {
		/* Too many DMA segments, linearize mbuf. */
		if (m_defrag(m, M_DONTWAIT)) {
			m_freem(m);
			return ENOBUFS;
		}

		error = bus_dmamap_load_mbuf(sc->sc_dmat, data->map, m,
		    BUS_DMA_NOWAIT | BUS_DMA_WRITE);
		if (error != 0) {
			printf("%s: can't map mbuf (error %d)\n",
			    sc->sc_dev.dv_xname, error);
			m_freem(m);
			return error;
		}
	}

	txd->txbufaddr = htole32(data->map->dm_segs[0].ds_addr);
	txd->txbufsize = htole16(m->m_pkthdr.len);
	bus_space_barrier(sc->sc_st, sc->sc_sh, 0, sc->sc_mapsize,
	    BUS_SPACE_BARRIER_WRITE);
	txd->txdw0 |= htole32(R92C_TXDW0_OWN);

	bus_dmamap_sync(sc->sc_dmat, tx_ring->map, 0, MCLBYTES,
	    BUS_DMASYNC_POSTWRITE);
	bus_dmamap_sync(sc->sc_dmat, data->map, 0, MCLBYTES,
	    BUS_DMASYNC_POSTWRITE);

	data->m = m;
	data->ni = ni;

#if NBPFILTER > 0
	if (__predict_false(sc->sc_drvbpf != NULL)) {
		struct rtwn_tx_radiotap_header *tap = &sc->sc_txtap;
		struct mbuf mb;

		tap->wt_flags = 0;
		tap->wt_chan_freq = htole16(ic->ic_bss->ni_chan->ic_freq);
		tap->wt_chan_flags = htole16(ic->ic_bss->ni_chan->ic_flags);

		mb.m_data = (caddr_t)tap;
		mb.m_len = sc->sc_txtap_len;
		mb.m_next = m;
		mb.m_nextpkt = NULL;
		mb.m_type = 0;
		mb.m_flags = 0;
		bpf_mtap(sc->sc_drvbpf, &mb, BPF_DIRECTION_OUT);
	}
#endif

	tx_ring->cur = (tx_ring->cur + 1) % RTWN_TX_LIST_COUNT;
	tx_ring->queued++;

	if (tx_ring->queued >= (RTWN_TX_LIST_COUNT - 1))
		sc->qfullmsk |= (1 << qid);

	/* Kick TX. */
	rtwn_pci_write_2(sc, R92C_PCIE_CTRL_REG, (1 << qid));

	return (0);
}

void
rtwn_tx_done(struct rtwn_pci_softc *sc, int qid)
{
	struct ieee80211com *ic = &sc->sc_sc.sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct rtwn_tx_ring *tx_ring = &sc->tx_ring[qid];
	struct rtwn_tx_data *tx_data;
	struct r92c_tx_desc_pci *tx_desc;
	int i;

	bus_dmamap_sync(sc->sc_dmat, tx_ring->map, 0, MCLBYTES,
	    BUS_DMASYNC_POSTREAD);

	for (i = 0; i < RTWN_TX_LIST_COUNT; i++) {
		tx_data = &tx_ring->tx_data[i];
		if (tx_data->m == NULL)
			continue;

		tx_desc = &tx_ring->desc[i];
		if (letoh32(tx_desc->txdw0) & R92C_TXDW0_OWN)
			continue;

		bus_dmamap_unload(sc->sc_dmat, tx_data->map);
		m_freem(tx_data->m);
		tx_data->m = NULL;
		ieee80211_release_node(ic, tx_data->ni);
		tx_data->ni = NULL;

		ifp->if_opackets++;
		sc->sc_sc.sc_tx_timer = 0;
		tx_ring->queued--;
	}

	if (tx_ring->queued < (RTWN_TX_LIST_COUNT - 1))
		sc->qfullmsk &= ~(1 << qid);
	
	if (sc->qfullmsk == 0) {
		ifq_clr_oactive(&ifp->if_snd);
		(*ifp->if_start)(ifp);
	}
}

void
rtwn_pci_stop(void *cookie)
{
	struct rtwn_pci_softc *sc = cookie;
	int i;

	for (i = 0; i < RTWN_NTXQUEUES; i++)
		rtwn_reset_tx_list(sc, i);
	rtwn_reset_rx_list(sc);
}

int
rtwn_intr(void *xsc)
{
	struct rtwn_pci_softc *sc = xsc;
	u_int32_t status;
	int i;

	status = rtwn_pci_read_4(sc, R92C_HISR);
	if (status == 0 || status == 0xffffffff)
		return (0);

	/* Disable interrupts. */
	rtwn_pci_write_4(sc, R92C_HIMR, 0x00000000);

	/* Ack interrupts. */
	rtwn_pci_write_4(sc, R92C_HISR, status);

	/* Vendor driver treats RX errors like ROK... */
	if (status & (R92C_IMR_ROK | R92C_IMR_RXFOVW | R92C_IMR_RDU)) {
		bus_dmamap_sync(sc->sc_dmat, sc->rx_ring.map, 0,
		    sizeof(struct r92c_rx_desc_pci) * RTWN_RX_LIST_COUNT,
		    BUS_DMASYNC_POSTREAD);

		for (i = 0; i < RTWN_RX_LIST_COUNT; i++) {
			struct r92c_rx_desc_pci *rx_desc = &sc->rx_ring.desc[i];
			struct rtwn_rx_data *rx_data = &sc->rx_ring.rx_data[i];

			if (letoh32(rx_desc->rxdw0) & R92C_RXDW0_OWN)
				continue;

			rtwn_rx_frame(sc, rx_desc, rx_data, i);
		}
	}

	if (status & R92C_IMR_BDOK)
		rtwn_tx_done(sc, RTWN_BEACON_QUEUE);
	if (status & R92C_IMR_HIGHDOK)
		rtwn_tx_done(sc, RTWN_HIGH_QUEUE);
	if (status & R92C_IMR_MGNTDOK)
		rtwn_tx_done(sc, RTWN_MGNT_QUEUE);
	if (status & R92C_IMR_BKDOK)
		rtwn_tx_done(sc, RTWN_BK_QUEUE);
	if (status & R92C_IMR_BEDOK)
		rtwn_tx_done(sc, RTWN_BE_QUEUE);
	if (status & R92C_IMR_VIDOK)
		rtwn_tx_done(sc, RTWN_VI_QUEUE);
	if (status & R92C_IMR_VODOK)
		rtwn_tx_done(sc, RTWN_VO_QUEUE);

	/* Enable interrupts. */
	rtwn_pci_write_4(sc, R92C_HIMR, RTWN_INT_ENABLE);

	return (1);
}

int
rtwn_is_oactive(void *cookie)
{
	struct rtwn_pci_softc *sc = cookie;
	
	return (sc->qfullmsk != 0);
}

int
rtwn_llt_write(struct rtwn_pci_softc *sc, uint32_t addr, uint32_t data)
{
	int ntries;

	rtwn_pci_write_4(sc, R92C_LLT_INIT,
	    SM(R92C_LLT_INIT_OP, R92C_LLT_INIT_OP_WRITE) |
	    SM(R92C_LLT_INIT_ADDR, addr) |
	    SM(R92C_LLT_INIT_DATA, data));
	/* Wait for write operation to complete. */
	for (ntries = 0; ntries < 20; ntries++) {
		if (MS(rtwn_pci_read_4(sc, R92C_LLT_INIT), R92C_LLT_INIT_OP) ==
		    R92C_LLT_INIT_OP_NO_ACTIVE)
			return (0);
		DELAY(5);
	}
	return (ETIMEDOUT);
}

int
rtwn_llt_init(struct rtwn_pci_softc *sc)
{
	int i, error;

	/* Reserve pages [0; R92C_TX_PAGE_COUNT]. */
	for (i = 0; i < R92C_TX_PAGE_COUNT; i++) {
		if ((error = rtwn_llt_write(sc, i, i + 1)) != 0)
			return (error);
	}
	/* NB: 0xff indicates end-of-list. */
	if ((error = rtwn_llt_write(sc, i, 0xff)) != 0)
		return (error);
	/*
	 * Use pages [R92C_TX_PAGE_COUNT + 1; R92C_TXPKTBUF_COUNT - 1]
	 * as ring buffer.
	 */
	for (++i; i < R92C_TXPKTBUF_COUNT - 1; i++) {
		if ((error = rtwn_llt_write(sc, i, i + 1)) != 0)
			return (error);
	}
	/* Make the last page point to the beginning of the ring buffer. */
	error = rtwn_llt_write(sc, i, R92C_TX_PAGE_COUNT + 1);
	return (error);
}

int
rtwn_dma_init(void *cookie)
{
	struct rtwn_pci_softc *sc = cookie;
	uint32_t reg;
	int error;

	/* Initialize LLT table. */
	error = rtwn_llt_init(sc);
	if (error != 0)
		return error;

	/* Set number of pages for normal priority queue. */
	rtwn_pci_write_2(sc, R92C_RQPN_NPQ, 0);
	rtwn_pci_write_4(sc, R92C_RQPN,
	    /* Set number of pages for public queue. */
	    SM(R92C_RQPN_PUBQ, R92C_PUBQ_NPAGES) |
	    /* Set number of pages for high priority queue. */
	    SM(R92C_RQPN_HPQ, R92C_HPQ_NPAGES) |
	    /* Set number of pages for low priority queue. */
	    SM(R92C_RQPN_LPQ, R92C_LPQ_NPAGES) |
	    /* Load values. */
	    R92C_RQPN_LD);

	rtwn_pci_write_1(sc, R92C_TXPKTBUF_BCNQ_BDNY, R92C_TX_PAGE_BOUNDARY);
	rtwn_pci_write_1(sc, R92C_TXPKTBUF_MGQ_BDNY, R92C_TX_PAGE_BOUNDARY);
	rtwn_pci_write_1(sc, R92C_TXPKTBUF_WMAC_LBK_BF_HD,
	    R92C_TX_PAGE_BOUNDARY);
	rtwn_pci_write_1(sc, R92C_TRXFF_BNDY, R92C_TX_PAGE_BOUNDARY);
	rtwn_pci_write_1(sc, R92C_TDECTRL + 1, R92C_TX_PAGE_BOUNDARY);

	reg = rtwn_pci_read_2(sc, R92C_TRXDMA_CTRL);
	reg &= ~R92C_TRXDMA_CTRL_QMAP_M;
	reg |= 0xF771; 
	rtwn_pci_write_2(sc, R92C_TRXDMA_CTRL, reg);

	rtwn_pci_write_4(sc, R92C_TCR,
	    R92C_TCR_CFENDFORM | (1 << 12) | (1 << 13));

	/* Configure Tx DMA. */
	rtwn_pci_write_4(sc, R92C_BKQ_DESA,
		sc->tx_ring[RTWN_BK_QUEUE].map->dm_segs[0].ds_addr);
	rtwn_pci_write_4(sc, R92C_BEQ_DESA,
		sc->tx_ring[RTWN_BE_QUEUE].map->dm_segs[0].ds_addr);
	rtwn_pci_write_4(sc, R92C_VIQ_DESA,
		sc->tx_ring[RTWN_VI_QUEUE].map->dm_segs[0].ds_addr);
	rtwn_pci_write_4(sc, R92C_VOQ_DESA,
		sc->tx_ring[RTWN_VO_QUEUE].map->dm_segs[0].ds_addr);
	rtwn_pci_write_4(sc, R92C_BCNQ_DESA,
		sc->tx_ring[RTWN_BEACON_QUEUE].map->dm_segs[0].ds_addr);
	rtwn_pci_write_4(sc, R92C_MGQ_DESA,
		sc->tx_ring[RTWN_MGNT_QUEUE].map->dm_segs[0].ds_addr);
	rtwn_pci_write_4(sc, R92C_HQ_DESA,
		sc->tx_ring[RTWN_HIGH_QUEUE].map->dm_segs[0].ds_addr);

	/* Configure Rx DMA. */
	rtwn_pci_write_4(sc, R92C_RX_DESA, sc->rx_ring.map->dm_segs[0].ds_addr);

	/* Set Tx/Rx transfer page boundary. */
	rtwn_pci_write_2(sc, R92C_TRXFF_BNDY + 2, 0x27ff);

	/* Set Tx/Rx transfer page size. */
	rtwn_pci_write_1(sc, R92C_PBP,
	    SM(R92C_PBP_PSRX, R92C_PBP_128) |
	    SM(R92C_PBP_PSTX, R92C_PBP_128));

	return (0);
}

void
rtwn_enable_intr(void *cookie)
{
	struct rtwn_pci_softc *sc = cookie;

	/* CLear pending interrupts. */
	rtwn_pci_write_4(sc, R92C_HISR, 0xffffffff);

	/* Enable interrupts. */
	rtwn_pci_write_4(sc, R92C_HIMR, RTWN_INT_ENABLE);
}

void
rtwn_disable_intr(void *cookie)
{
	struct rtwn_pci_softc *sc = cookie;

	/* Disable interrupts. */
	rtwn_pci_write_4(sc, R92C_HISR, 0x00000000);
	rtwn_pci_write_4(sc, R92C_HIMR, 0x00000000);
}

void
rtwn_calib_to(void *arg)
{
	struct rtwn_pci_softc *sc = arg;

	rtwn_calib(&sc->sc_sc);
}

void
rtwn_next_calib(void *cookie)
{
	struct rtwn_pci_softc *sc = cookie;

	timeout_add_sec(&sc->calib_to, 2);
}

void
rtwn_cancel_calib(void *cookie)
{
	struct rtwn_pci_softc *sc = cookie;

	if (timeout_initialized(&sc->calib_to))
		timeout_del(&sc->calib_to);
}

void
rtwn_scan_to(void *arg)
{
	struct rtwn_pci_softc *sc = arg;

	rtwn_next_scan(&sc->sc_sc);
}

void
rtwn_pci_next_scan(void *cookie)
{
	struct rtwn_pci_softc *sc = cookie;

	timeout_add_msec(&sc->scan_to, 200);
}

void
rtwn_cancel_scan(void *cookie)
{
	struct rtwn_pci_softc *sc = cookie;

	if (timeout_initialized(&sc->scan_to))
		timeout_del(&sc->scan_to);
}
