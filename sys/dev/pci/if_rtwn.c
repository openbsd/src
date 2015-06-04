/*	$OpenBSD: if_rtwn.c,v 1.1 2015/06/04 21:08:40 stsp Exp $	*/

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
 * Driver for Realtek RTL8188CE
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
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_radiotap.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/pci/if_rtwnreg.h>

#ifdef RTWN_DEBUG
#define DPRINTF(x)	do { if (rtwn_debug) printf x; } while (0)
#define DPRINTFN(n, x)	do { if (rtwn_debug >= (n)) printf x; } while (0)
int rtwn_debug = 0;
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

int		rtwn_match(struct device *, void *, void *);
void		rtwn_attach(struct device *, struct device *, void *);
int		rtwn_detach(struct device *, int);
int		rtwn_activate(struct device *, int);
int		rtwn_alloc_rx_list(struct rtwn_softc *);
void		rtwn_reset_rx_list(struct rtwn_softc *);
void		rtwn_free_rx_list(struct rtwn_softc *);
void		rtwn_setup_rx_desc(struct rtwn_softc *, struct r92c_rx_desc *,
		    bus_addr_t, size_t, int);
int		rtwn_alloc_tx_list(struct rtwn_softc *, int);
void		rtwn_reset_tx_list(struct rtwn_softc *, int);
void		rtwn_free_tx_list(struct rtwn_softc *, int);
void		rtwn_write_1(struct rtwn_softc *, uint16_t, uint8_t);
void		rtwn_write_2(struct rtwn_softc *, uint16_t, uint16_t);
void		rtwn_write_4(struct rtwn_softc *, uint16_t, uint32_t);
uint8_t		rtwn_read_1(struct rtwn_softc *, uint16_t);
uint16_t	rtwn_read_2(struct rtwn_softc *, uint16_t);
uint32_t	rtwn_read_4(struct rtwn_softc *, uint16_t);
int		rtwn_fw_cmd(struct rtwn_softc *, uint8_t, const void *, int);
void		rtwn_rf_write(struct rtwn_softc *, int, uint8_t, uint32_t);
uint32_t	rtwn_rf_read(struct rtwn_softc *, int, uint8_t);
void		rtwn_cam_write(struct rtwn_softc *, uint32_t, uint32_t);
int		rtwn_llt_write(struct rtwn_softc *, uint32_t, uint32_t);
uint8_t		rtwn_efuse_read_1(struct rtwn_softc *, uint16_t);
void		rtwn_efuse_read(struct rtwn_softc *);
int		rtwn_read_chipid(struct rtwn_softc *);
void		rtwn_read_rom(struct rtwn_softc *);
int		rtwn_media_change(struct ifnet *);
int		rtwn_ra_init(struct rtwn_softc *);
void		rtwn_tsf_sync_enable(struct rtwn_softc *);
void		rtwn_set_led(struct rtwn_softc *, int, int);
void		rtwn_calib_to(void *);
void		rtwn_next_scan(void *);
int		rtwn_newstate(struct ieee80211com *, enum ieee80211_state,
		    int);
void		rtwn_updateedca(struct ieee80211com *);
int		rtwn_set_key(struct ieee80211com *, struct ieee80211_node *,
		    struct ieee80211_key *);
void		rtwn_delete_key(struct ieee80211com *,
		    struct ieee80211_node *, struct ieee80211_key *);
void		rtwn_update_avgrssi(struct rtwn_softc *, int, int8_t);
int8_t		rtwn_get_rssi(struct rtwn_softc *, int, void *);
void		rtwn_rx_frame(struct rtwn_softc *, struct r92c_rx_desc *,
		    struct rtwn_rx_data *, int);
int		rtwn_tx(struct rtwn_softc *, struct mbuf *,
		    struct ieee80211_node *);
void		rtwn_tx_done(struct rtwn_softc *, int);
void		rtwn_start(struct ifnet *);
void		rtwn_watchdog(struct ifnet *);
int		rtwn_ioctl(struct ifnet *, u_long, caddr_t);
int		rtwn_power_on(struct rtwn_softc *);
int		rtwn_llt_init(struct rtwn_softc *);
void		rtwn_fw_reset(struct rtwn_softc *);
int		rtwn_fw_loadpage(struct rtwn_softc *, int, uint8_t *, int);
int		rtwn_load_firmware(struct rtwn_softc *);
int		rtwn_dma_init(struct rtwn_softc *);
void		rtwn_mac_init(struct rtwn_softc *);
void		rtwn_bb_init(struct rtwn_softc *);
void		rtwn_rf_init(struct rtwn_softc *);
void		rtwn_cam_init(struct rtwn_softc *);
void		rtwn_pa_bias_init(struct rtwn_softc *);
void		rtwn_rxfilter_init(struct rtwn_softc *);
void		rtwn_edca_init(struct rtwn_softc *);
void		rtwn_write_txpower(struct rtwn_softc *, int, uint16_t[]);
void		rtwn_get_txpower(struct rtwn_softc *, int,
		    struct ieee80211_channel *, struct ieee80211_channel *,
		    uint16_t[]);
void		rtwn_set_txpower(struct rtwn_softc *,
		    struct ieee80211_channel *, struct ieee80211_channel *);
void		rtwn_set_chan(struct rtwn_softc *,
		    struct ieee80211_channel *, struct ieee80211_channel *);
int		rtwn_iq_calib_chain(struct rtwn_softc *, int, uint16_t[],
		    uint16_t[]);
void		rtwn_iq_calib(struct rtwn_softc *);
void		rtwn_lc_calib(struct rtwn_softc *);
void		rtwn_temp_calib(struct rtwn_softc *);
int		rtwn_init(struct ifnet *);
void		rtwn_init_task(void *);
void		rtwn_stop(struct ifnet *);
int		rtwn_intr(void *);

/* Aliases. */
#define	rtwn_bb_write	rtwn_write_4
#define rtwn_bb_read	rtwn_read_4

struct cfdriver rtwn_cd = {
	NULL, "rtwn", DV_IFNET
};

const struct cfattach rtwn_ca = {
	sizeof(struct rtwn_softc),
	rtwn_match,
	rtwn_attach,
	rtwn_detach,
	rtwn_activate
};

int
rtwn_match(struct device *parent, void *match, void *aux)
{
	return (pci_matchbyid(aux, rtwn_pci_devices,
	    nitems(rtwn_pci_devices)));
}

void
rtwn_attach(struct device *parent, struct device *self, void *aux)
{
	struct rtwn_softc *sc = (struct rtwn_softc *)self;
	struct pci_attach_args *pa = aux;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	int i, error;
	pcireg_t memtype;
	pci_intr_handle_t ih;
	const char *intrstr;

	sc->sc_dmat = pa->pa_dmat;
	sc->sc_pc = pa->pa_pc;
	sc->sc_tag = pa->pa_tag;

	timeout_set(&sc->scan_to, rtwn_next_scan, sc);
	timeout_set(&sc->calib_to, rtwn_calib_to, sc);

	task_set(&sc->init_task, rtwn_init_task, sc);

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

	error = rtwn_read_chipid(sc);
	if (error != 0) {
		printf("%s: unsupported test chip\n", sc->sc_dev.dv_xname);
		return;
	}

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
			return;
		}
	}

	/* Determine number of Tx/Rx chains. */
	if (sc->chip & RTWN_CHIP_92C) {
		sc->ntxchains = (sc->chip & RTWN_CHIP_92C_1T2R) ? 1 : 2;
		sc->nrxchains = 2;
	} else {
		sc->ntxchains = 1;
		sc->nrxchains = 1;
	}
	rtwn_read_rom(sc);

	printf("%s: MAC/BB RTL%s, RF 6052 %dT%dR, address %s\n",
	    sc->sc_dev.dv_xname,
	    (sc->chip & RTWN_CHIP_92C) ? "8192CE" : "8188CE",
	    sc->ntxchains, sc->nrxchains,
	    ether_sprintf(ic->ic_myaddr));

	ic->ic_phytype = IEEE80211_T_OFDM;	/* Not only, but not used. */
	ic->ic_opmode = IEEE80211_M_STA;	/* Default to BSS mode. */
	ic->ic_state = IEEE80211_S_INIT;

	/* Set device capabilities. */
	ic->ic_caps =
	    IEEE80211_C_MONITOR |	/* Monitor mode supported. */
	    IEEE80211_C_SHPREAMBLE |	/* Short preamble supported. */
	    IEEE80211_C_SHSLOT |	/* Short slot time supported. */
	    IEEE80211_C_WEP |		/* WEP. */
	    IEEE80211_C_RSN;		/* WPA/RSN. */

#ifndef IEEE80211_NO_HT
	/* Set HT capabilities. */
	ic->ic_htcaps =
	    IEEE80211_HTCAP_CBW20_40 |
	    IEEE80211_HTCAP_DSSSCCK40;
	/* Set supported HT rates. */
	for (i = 0; i < sc->nrxchains; i++)
		ic->ic_sup_mcs[i] = 0xff;
#endif

	/* Set supported .11b and .11g rates. */
	ic->ic_sup_rates[IEEE80211_MODE_11B] = ieee80211_std_rateset_11b;
	ic->ic_sup_rates[IEEE80211_MODE_11G] = ieee80211_std_rateset_11g;

	/* Set supported .11b and .11g channels (1 through 14). */
	for (i = 1; i <= 14; i++) {
		ic->ic_channels[i].ic_freq =
		    ieee80211_ieee2mhz(i, IEEE80211_CHAN_2GHZ);
		ic->ic_channels[i].ic_flags =
		    IEEE80211_CHAN_CCK | IEEE80211_CHAN_OFDM |
		    IEEE80211_CHAN_DYN | IEEE80211_CHAN_2GHZ;
	}

#ifdef notyet
	/*
	 * The number of STAs that we can support is limited by the number
	 * of CAM entries used for hardware crypto.
	 */
	ic->ic_max_nnodes = R92C_CAM_ENTRY_COUNT - 4;
	if (ic->ic_max_nnodes > IEEE80211_CACHE_SIZE)
		ic->ic_max_nnodes = IEEE80211_CACHE_SIZE;
#endif

	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = rtwn_ioctl;
	ifp->if_start = rtwn_start;
	ifp->if_watchdog = rtwn_watchdog;
	IFQ_SET_READY(&ifp->if_snd);
	memcpy(ifp->if_xname, sc->sc_dev.dv_xname, IFNAMSIZ);

	if_attach(ifp);
	ieee80211_ifattach(ifp);
	ic->ic_updateedca = rtwn_updateedca;
#ifdef notyet
	ic->ic_set_key = rtwn_set_key;
	ic->ic_delete_key = rtwn_delete_key;
#endif
	/* Override state transition machine. */
	sc->sc_newstate = ic->ic_newstate;
	ic->ic_newstate = rtwn_newstate;
	ieee80211_media_init(ifp, rtwn_media_change, ieee80211_media_status);

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
rtwn_detach(struct device *self, int flags)
{
	struct rtwn_softc *sc = (struct rtwn_softc *)self;
	struct ifnet *ifp = &sc->sc_ic.ic_if;
	int s, i;

	s = splnet();

	if (timeout_initialized(&sc->scan_to))
		timeout_del(&sc->scan_to);
	if (timeout_initialized(&sc->calib_to))
		timeout_del(&sc->calib_to);

	task_del(systq, &sc->init_task);

	if (ifp->if_softc != NULL) {
		ieee80211_ifdetach(ifp);
		if_detach(ifp);
	}

	/* Free Tx/Rx buffers. */
	for (i = 0; i < RTWN_NTXQUEUES; i++)
		rtwn_free_tx_list(sc, i);
	rtwn_free_rx_list(sc);
	splx(s);

	return (0);
}

int
rtwn_activate(struct device *self, int act)
{
	struct rtwn_softc *sc = (struct rtwn_softc *)self;
	struct ifnet *ifp = &sc->sc_ic.ic_if;

	switch (act) {
	case DVACT_SUSPEND:
		if (ifp->if_flags & IFF_RUNNING)
			rtwn_stop(ifp);
		break;
	case DVACT_WAKEUP:
		rtwn_init_task(sc);
		break;
	}
	return (0);
}

void
rtwn_setup_rx_desc(struct rtwn_softc *sc, struct r92c_rx_desc *desc,
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
rtwn_alloc_rx_list(struct rtwn_softc *sc)
{
	struct rtwn_rx_ring *rx_ring = &sc->rx_ring;
	struct rtwn_rx_data *rx_data;
	size_t size;
	int i, error = 0;

	/* Allocate Rx descriptors. */
	size = sizeof(struct r92c_rx_desc) * RTWN_RX_LIST_COUNT;
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
rtwn_reset_rx_list(struct rtwn_softc *sc)
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
rtwn_free_rx_list(struct rtwn_softc *sc)
{
	struct rtwn_rx_ring *rx_ring = &sc->rx_ring;
	struct rtwn_rx_data *rx_data;
	int i, s;

	s = splnet();

	if (rx_ring->map) {
		if (rx_ring->desc) {
			bus_dmamap_unload(sc->sc_dmat, rx_ring->map);
			bus_dmamem_unmap(sc->sc_dmat, (caddr_t)rx_ring->desc,
			    sizeof (struct r92c_rx_desc) * RTWN_RX_LIST_COUNT);
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
rtwn_alloc_tx_list(struct rtwn_softc *sc, int qid)
{
	struct rtwn_tx_ring *tx_ring = &sc->tx_ring[qid];
	struct rtwn_tx_data *tx_data;
	int i = 0, error = 0;

	error = bus_dmamap_create(sc->sc_dmat,
	    sizeof (struct r92c_tx_desc) * RTWN_TX_LIST_COUNT, 1,
	    sizeof (struct r92c_tx_desc) * RTWN_TX_LIST_COUNT, 0, BUS_DMA_NOWAIT,
	    &tx_ring->map);
	if (error != 0) {
		printf("%s: could not create tx ring DMA map\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	error = bus_dmamem_alloc(sc->sc_dmat,
	    sizeof (struct r92c_tx_desc) * RTWN_TX_LIST_COUNT, PAGE_SIZE, 0,
	    &tx_ring->seg, 1, &tx_ring->nsegs, BUS_DMA_NOWAIT | BUS_DMA_ZERO);
	if (error != 0) {
		printf("%s: could not allocate tx ring DMA memory\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	error = bus_dmamem_map(sc->sc_dmat, &tx_ring->seg, tx_ring->nsegs,
	    sizeof (struct r92c_tx_desc) * RTWN_TX_LIST_COUNT,
	    (caddr_t *)&tx_ring->desc, BUS_DMA_NOWAIT);
	if (error != 0) {
		bus_dmamem_free(sc->sc_dmat, &tx_ring->seg, tx_ring->nsegs);
		printf("%s: can't map tx ring DMA memory\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	error = bus_dmamap_load(sc->sc_dmat, tx_ring->map, tx_ring->desc,
	    sizeof (struct r92c_tx_desc) * RTWN_TX_LIST_COUNT, NULL,
	    BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not load tx ring DMA map\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	for (i = 0; i < RTWN_TX_LIST_COUNT; i++) {
		struct r92c_tx_desc *desc = &tx_ring->desc[i];

		/* setup tx desc */
		desc->nextdescaddr = htole32(tx_ring->map->dm_segs[0].ds_addr
		  + sizeof(struct r92c_tx_desc)
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
rtwn_reset_tx_list(struct rtwn_softc *sc, int qid)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct rtwn_tx_ring *tx_ring = &sc->tx_ring[qid];
	int i;

	for (i = 0; i < RTWN_TX_LIST_COUNT; i++) {
		struct r92c_tx_desc *desc = &tx_ring->desc[i];
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
rtwn_free_tx_list(struct rtwn_softc *sc, int qid)
{
	struct rtwn_tx_ring *tx_ring = &sc->tx_ring[qid];
	struct rtwn_tx_data *tx_data;
	int i;

	if (tx_ring->map != NULL) {
		if (tx_ring->desc != NULL) {
			bus_dmamap_unload(sc->sc_dmat, tx_ring->map);
			bus_dmamem_unmap(sc->sc_dmat, (caddr_t)tx_ring->desc,
			    sizeof (struct r92c_tx_desc) * RTWN_TX_LIST_COUNT);
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
rtwn_write_1(struct rtwn_softc *sc, uint16_t addr, uint8_t val)
{
	bus_space_write_1(sc->sc_st, sc->sc_sh, addr, val);
}

void
rtwn_write_2(struct rtwn_softc *sc, uint16_t addr, uint16_t val)
{
	val = htole16(val);
	bus_space_write_2(sc->sc_st, sc->sc_sh, addr, val);
}

void
rtwn_write_4(struct rtwn_softc *sc, uint16_t addr, uint32_t val)
{
	val = htole32(val);
	bus_space_write_4(sc->sc_st, sc->sc_sh, addr, val);
}

uint8_t
rtwn_read_1(struct rtwn_softc *sc, uint16_t addr)
{
	return bus_space_read_1(sc->sc_st, sc->sc_sh, addr);
}

uint16_t
rtwn_read_2(struct rtwn_softc *sc, uint16_t addr)
{
	return bus_space_read_2(sc->sc_st, sc->sc_sh, addr);
}

uint32_t
rtwn_read_4(struct rtwn_softc *sc, uint16_t addr)
{
	return bus_space_read_4(sc->sc_st, sc->sc_sh, addr);
}

int
rtwn_fw_cmd(struct rtwn_softc *sc, uint8_t id, const void *buf, int len)
{
	struct r92c_fw_cmd cmd;
	int ntries;

	/* Wait for current FW box to be empty. */
	for (ntries = 0; ntries < 100; ntries++) {
		if (!(rtwn_read_1(sc, R92C_HMETFR) & (1 << sc->fwcur)))
			break;
		DELAY(1);
	}
	if (ntries == 100) {
		printf("%s: could not send firmware command %d\n",
		    sc->sc_dev.dv_xname, id);
		return (ETIMEDOUT);
	}
	memset(&cmd, 0, sizeof(cmd));
	cmd.id = id;
	if (len > 3)
		cmd.id |= R92C_CMD_FLAG_EXT;
	KASSERT(len <= sizeof(cmd.msg));
	memcpy(cmd.msg, buf, len);

	/* Write the first word last since that will trigger the FW. */
	rtwn_write_2(sc, R92C_HMEBOX_EXT(sc->fwcur), *((uint8_t *)&cmd + 4));
	rtwn_write_4(sc, R92C_HMEBOX(sc->fwcur), *((uint8_t *)&cmd + 0));

	sc->fwcur = (sc->fwcur + 1) % R92C_H2C_NBOX;
	return (0);
}

void
rtwn_rf_write(struct rtwn_softc *sc, int chain, uint8_t addr, uint32_t val)
{
	rtwn_bb_write(sc, R92C_LSSI_PARAM(chain),
	    SM(R92C_LSSI_PARAM_ADDR, addr) |
	    SM(R92C_LSSI_PARAM_DATA, val));
}

uint32_t
rtwn_rf_read(struct rtwn_softc *sc, int chain, uint8_t addr)
{
	uint32_t reg[R92C_MAX_CHAINS], val;

	reg[0] = rtwn_bb_read(sc, R92C_HSSI_PARAM2(0));
	if (chain != 0)
		reg[chain] = rtwn_bb_read(sc, R92C_HSSI_PARAM2(chain));

	rtwn_bb_write(sc, R92C_HSSI_PARAM2(0),
	    reg[0] & ~R92C_HSSI_PARAM2_READ_EDGE);
	DELAY(1000);

	rtwn_bb_write(sc, R92C_HSSI_PARAM2(chain),
	    RW(reg[chain], R92C_HSSI_PARAM2_READ_ADDR, addr) |
	    R92C_HSSI_PARAM2_READ_EDGE);
	DELAY(1000);

	rtwn_bb_write(sc, R92C_HSSI_PARAM2(0),
	    reg[0] | R92C_HSSI_PARAM2_READ_EDGE);
	DELAY(1000);

	if (rtwn_bb_read(sc, R92C_HSSI_PARAM1(chain)) & R92C_HSSI_PARAM1_PI)
		val = rtwn_bb_read(sc, R92C_HSPI_READBACK(chain));
	else
		val = rtwn_bb_read(sc, R92C_LSSI_READBACK(chain));
	return (MS(val, R92C_LSSI_READBACK_DATA));
}

void
rtwn_cam_write(struct rtwn_softc *sc, uint32_t addr, uint32_t data)
{
	rtwn_write_4(sc, R92C_CAMWRITE, data);
	rtwn_write_4(sc, R92C_CAMCMD,
	    R92C_CAMCMD_POLLING | R92C_CAMCMD_WRITE |
	    SM(R92C_CAMCMD_ADDR, addr));
}

int
rtwn_llt_write(struct rtwn_softc *sc, uint32_t addr, uint32_t data)
{
	int ntries;

	rtwn_write_4(sc, R92C_LLT_INIT,
	    SM(R92C_LLT_INIT_OP, R92C_LLT_INIT_OP_WRITE) |
	    SM(R92C_LLT_INIT_ADDR, addr) |
	    SM(R92C_LLT_INIT_DATA, data));
	/* Wait for write operation to complete. */
	for (ntries = 0; ntries < 20; ntries++) {
		if (MS(rtwn_read_4(sc, R92C_LLT_INIT), R92C_LLT_INIT_OP) ==
		    R92C_LLT_INIT_OP_NO_ACTIVE)
			return (0);
		DELAY(5);
	}
	return (ETIMEDOUT);
}

uint8_t
rtwn_efuse_read_1(struct rtwn_softc *sc, uint16_t addr)
{
	uint32_t reg;
	int ntries;

	reg = rtwn_read_4(sc, R92C_EFUSE_CTRL);
	reg = RW(reg, R92C_EFUSE_CTRL_ADDR, addr);
	reg &= ~R92C_EFUSE_CTRL_VALID;
	rtwn_write_4(sc, R92C_EFUSE_CTRL, reg);
	/* Wait for read operation to complete. */
	for (ntries = 0; ntries < 100; ntries++) {
		reg = rtwn_read_4(sc, R92C_EFUSE_CTRL);
		if (reg & R92C_EFUSE_CTRL_VALID)
			return (MS(reg, R92C_EFUSE_CTRL_DATA));
		DELAY(5);
	}
	printf("%s: could not read efuse byte at address 0x%x\n",
	    sc->sc_dev.dv_xname, addr);
	return (0xff);
}

void
rtwn_efuse_read(struct rtwn_softc *sc)
{
	uint8_t *rom = (uint8_t *)&sc->rom;
	uint16_t addr = 0;
	uint32_t reg;
	uint8_t off, msk;
	int i;

	reg = rtwn_read_2(sc, R92C_SYS_ISO_CTRL);
	if (!(reg & R92C_SYS_ISO_CTRL_PWC_EV12V)) {
		rtwn_write_2(sc, R92C_SYS_ISO_CTRL,
		    reg | R92C_SYS_ISO_CTRL_PWC_EV12V);
	}
	reg = rtwn_read_2(sc, R92C_SYS_FUNC_EN);
	if (!(reg & R92C_SYS_FUNC_EN_ELDR)) {
		rtwn_write_2(sc, R92C_SYS_FUNC_EN,
		    reg | R92C_SYS_FUNC_EN_ELDR);
	}
	reg = rtwn_read_2(sc, R92C_SYS_CLKR);
	if ((reg & (R92C_SYS_CLKR_LOADER_EN | R92C_SYS_CLKR_ANA8M)) !=
	    (R92C_SYS_CLKR_LOADER_EN | R92C_SYS_CLKR_ANA8M)) {
		rtwn_write_2(sc, R92C_SYS_CLKR,
		    reg | R92C_SYS_CLKR_LOADER_EN | R92C_SYS_CLKR_ANA8M);
	}
	memset(&sc->rom, 0xff, sizeof(sc->rom));
	while (addr < 512) {
		reg = rtwn_efuse_read_1(sc, addr);
		if (reg == 0xff)
			break;
		addr++;
		off = reg >> 4;
		msk = reg & 0xf;
		for (i = 0; i < 4; i++) {
			if (msk & (1 << i))
				continue;
			rom[off * 8 + i * 2 + 0] =
			    rtwn_efuse_read_1(sc, addr);
			addr++;
			rom[off * 8 + i * 2 + 1] =
			    rtwn_efuse_read_1(sc, addr);
			addr++;
		}
	}
#ifdef RTWN_DEBUG
	if (rtwn_debug >= 2) {
		/* Dump ROM content. */
		printf("\n");
		for (i = 0; i < sizeof(sc->rom); i++)
			printf("%02x:", rom[i]);
		printf("\n");
	}
#endif
}

/* rtwn_read_chipid: reg=0x40073b chipid=0x0 */
int
rtwn_read_chipid(struct rtwn_softc *sc)
{
	uint32_t reg;

	reg = rtwn_read_4(sc, R92C_SYS_CFG);
	if (reg & R92C_SYS_CFG_TRP_VAUX_EN)
		/* Unsupported test chip. */
		return (EIO);

	if (reg & R92C_SYS_CFG_TYPE_92C) {
		sc->chip |= RTWN_CHIP_92C;
		/* Check if it is a castrated 8192C. */
		if (MS(rtwn_read_4(sc, R92C_HPON_FSM),
		    R92C_HPON_FSM_CHIP_BONDING_ID) ==
		    R92C_HPON_FSM_CHIP_BONDING_ID_92C_1T2R)
			sc->chip |= RTWN_CHIP_92C_1T2R;
	}
	if (reg & R92C_SYS_CFG_VENDOR_UMC) {
		sc->chip |= RTWN_CHIP_UMC;
		if (MS(reg, R92C_SYS_CFG_CHIP_VER_RTL) == 0)
			sc->chip |= RTWN_CHIP_UMC_A_CUT;
	}
	return (0);
}

void
rtwn_read_rom(struct rtwn_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct r92c_rom *rom = &sc->rom;

	/* Read full ROM image. */
	rtwn_efuse_read(sc);

	if (rom->id != 0x8129) {
		printf("%s: invalid EEPROM ID 0x%x\n",
			sc->sc_dev.dv_xname, rom->id);
	}

	/* XXX Weird but this is what the vendor driver does. */
	sc->pa_setting = rtwn_efuse_read_1(sc, 0x1fa);
	DPRINTF(("PA setting=0x%x\n", sc->pa_setting));

	sc->board_type = MS(rom->rf_opt1, R92C_ROM_RF1_BOARD_TYPE);

	sc->regulatory = MS(rom->rf_opt1, R92C_ROM_RF1_REGULATORY);
	DPRINTF(("regulatory type=%d\n", sc->regulatory));

	IEEE80211_ADDR_COPY(ic->ic_myaddr, rom->macaddr);
}

int
rtwn_media_change(struct ifnet *ifp)
{
	int error;

	error = ieee80211_media_change(ifp);
	if (error != ENETRESET)
		return (error);

	if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) ==
	    (IFF_UP | IFF_RUNNING)) {
		rtwn_stop(ifp);
		rtwn_init(ifp);
	}
	return (0);
}

/*
 * Initialize rate adaptation in firmware.
 */
int
rtwn_ra_init(struct rtwn_softc *sc)
{
	static const uint8_t map[] =
	    { 2, 4, 11, 22, 12, 18, 24, 36, 48, 72, 96, 108 };
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni = ic->ic_bss;
	struct ieee80211_rateset *rs = &ni->ni_rates;
	struct r92c_fw_cmd_macid_cfg cmd;
	uint32_t rates, basicrates;
	uint8_t mode;
	int maxrate, maxbasicrate, error, i, j;

	/* Get normal and basic rates mask. */
	rates = basicrates = 0;
	maxrate = maxbasicrate = 0;
	for (i = 0; i < rs->rs_nrates; i++) {
		/* Convert 802.11 rate to HW rate index. */
		for (j = 0; j < nitems(map); j++)
			if ((rs->rs_rates[i] & IEEE80211_RATE_VAL) == map[j])
				break;
		if (j == nitems(map))	/* Unknown rate, skip. */
			continue;
		rates |= 1 << j;
		if (j > maxrate)
			maxrate = j;
		if (rs->rs_rates[i] & IEEE80211_RATE_BASIC) {
			basicrates |= 1 << j;
			if (j > maxbasicrate)
				maxbasicrate = j;
		}
	}
	if (ic->ic_curmode == IEEE80211_MODE_11B)
		mode = R92C_RAID_11B;
	else
		mode = R92C_RAID_11BG;
	DPRINTF(("mode=0x%x rates=0x%08x, basicrates=0x%08x\n",
	    mode, rates, basicrates));

	/* Set rates mask for group addressed frames. */
	cmd.macid = RTWN_MACID_BC | RTWN_MACID_VALID;
	cmd.mask = htole32(mode << 28 | basicrates);
	error = rtwn_fw_cmd(sc, R92C_CMD_MACID_CONFIG, &cmd, sizeof(cmd));
	if (error != 0) {
		printf("%s: could not add broadcast station\n",
		    sc->sc_dev.dv_xname);
		return (error);
	}
	/* Set initial MRR rate. */
	DPRINTF(("maxbasicrate=%d\n", maxbasicrate));
	rtwn_write_1(sc, R92C_INIDATA_RATE_SEL(RTWN_MACID_BC),
	    maxbasicrate);

	/* Set rates mask for unicast frames. */
	cmd.macid = RTWN_MACID_BSS | RTWN_MACID_VALID;
	cmd.mask = htole32(mode << 28 | rates);
	error = rtwn_fw_cmd(sc, R92C_CMD_MACID_CONFIG, &cmd, sizeof(cmd));
	if (error != 0) {
		printf("%s: could not add BSS station\n",
		    sc->sc_dev.dv_xname);
		return (error);
	}
	/* Set initial MRR rate. */
	DPRINTF(("maxrate=%d\n", maxrate));
	rtwn_write_1(sc, R92C_INIDATA_RATE_SEL(RTWN_MACID_BSS),
	    maxrate);

	/* Configure Automatic Rate Fallback Register. */
	if (ic->ic_curmode == IEEE80211_MODE_11B) {
		if (rates & 0x0c)
			rtwn_write_4(sc, R92C_ARFR(0), htole32(rates & 0x0d));
		else
			rtwn_write_4(sc, R92C_ARFR(0), htole32(rates & 0x0f));
	} else
		rtwn_write_4(sc, R92C_ARFR(0), htole32(rates & 0x0ff5));

	/* Indicate highest supported rate. */
	ni->ni_txrate = rs->rs_nrates - 1;
	return (0);
}

void
rtwn_tsf_sync_enable(struct rtwn_softc *sc)
{
	struct ieee80211_node *ni = sc->sc_ic.ic_bss;
	uint64_t tsf;

	/* Enable TSF synchronization. */
	rtwn_write_1(sc, R92C_BCN_CTRL,
	    rtwn_read_1(sc, R92C_BCN_CTRL) & ~R92C_BCN_CTRL_DIS_TSF_UDT0);

	rtwn_write_1(sc, R92C_BCN_CTRL,
	    rtwn_read_1(sc, R92C_BCN_CTRL) & ~R92C_BCN_CTRL_EN_BCN);

	/* Set initial TSF. */
	memcpy(&tsf, ni->ni_tstamp, 8);
	tsf = letoh64(tsf);
	tsf = tsf - (tsf % (ni->ni_intval * IEEE80211_DUR_TU));
	tsf -= IEEE80211_DUR_TU;
	rtwn_write_4(sc, R92C_TSFTR + 0, tsf);
	rtwn_write_4(sc, R92C_TSFTR + 4, tsf >> 32);

	rtwn_write_1(sc, R92C_BCN_CTRL,
	    rtwn_read_1(sc, R92C_BCN_CTRL) | R92C_BCN_CTRL_EN_BCN);
}

void
rtwn_set_led(struct rtwn_softc *sc, int led, int on)
{
	uint8_t reg;

	if (led == RTWN_LED_LINK) {
		reg = rtwn_read_1(sc, R92C_LEDCFG0) & 0x70;
		if (!on)
			reg |= R92C_LEDCFG0_DIS;
		rtwn_write_1(sc, R92C_LEDCFG0, reg);
		sc->ledlink = on;	/* Save LED state. */
	}
}

void
rtwn_calib_to(void *arg)
{
	struct rtwn_softc *sc = arg;
	struct r92c_fw_cmd_rssi cmd;

	if (sc->avg_pwdb != -1) {
		/* Indicate Rx signal strength to FW for rate adaptation. */
		memset(&cmd, 0, sizeof(cmd));
		cmd.macid = 0;	/* BSS. */
		cmd.pwdb = sc->avg_pwdb;
		DPRINTFN(3, ("sending RSSI command avg=%d\n", sc->avg_pwdb));
		rtwn_fw_cmd(sc, R92C_CMD_RSSI_SETTING, &cmd, sizeof(cmd));
	}

	/* Do temperature compensation. */
	rtwn_temp_calib(sc);

	timeout_add_sec(&sc->calib_to, 2);
}

void
rtwn_next_scan(void *arg)
{
	struct rtwn_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	int s;

	s = splnet();
	if (ic->ic_state == IEEE80211_S_SCAN)
		ieee80211_next_scan(&ic->ic_if);
	splx(s);
}

int
rtwn_newstate(struct ieee80211com *ic, enum ieee80211_state nstate, int arg)
{
	struct rtwn_softc *sc = ic->ic_softc;
	struct ieee80211_node *ni;
	enum ieee80211_state ostate;
	uint32_t reg;
	int s;

	s = splnet();
	ostate = ic->ic_state;

	if (nstate != ostate)
		DPRINTF(("newstate %s -> %s\n",
		    ieee80211_state_name[ostate],
		    ieee80211_state_name[nstate]));

	if (ostate == IEEE80211_S_RUN) {
		/* Stop calibration. */
		timeout_del(&sc->calib_to);

		/* Turn link LED off. */
		rtwn_set_led(sc, RTWN_LED_LINK, 0);

		/* Set media status to 'No Link'. */
		reg = rtwn_read_4(sc, R92C_CR);
		reg = RW(reg, R92C_CR_NETTYPE, R92C_CR_NETTYPE_NOLINK);
		rtwn_write_4(sc, R92C_CR, reg);

		/* Stop Rx of data frames. */
		rtwn_write_2(sc, R92C_RXFLTMAP2, 0);

		/* Rest TSF. */
		rtwn_write_1(sc, R92C_DUAL_TSF_RST, 0x03);

		/* Disable TSF synchronization. */
		rtwn_write_1(sc, R92C_BCN_CTRL,
		    rtwn_read_1(sc, R92C_BCN_CTRL) |
		    R92C_BCN_CTRL_DIS_TSF_UDT0);

		/* Reset EDCA parameters. */
		rtwn_write_4(sc, R92C_EDCA_VO_PARAM, 0x002f3217);
		rtwn_write_4(sc, R92C_EDCA_VI_PARAM, 0x005e4317);
		rtwn_write_4(sc, R92C_EDCA_BE_PARAM, 0x00105320);
		rtwn_write_4(sc, R92C_EDCA_BK_PARAM, 0x0000a444);
	}
	switch (nstate) {
	case IEEE80211_S_INIT:
		/* Turn link LED off. */
		rtwn_set_led(sc, RTWN_LED_LINK, 0);
		break;
	case IEEE80211_S_SCAN:
		if (ostate != IEEE80211_S_SCAN) {
			/* Allow Rx from any BSSID. */
			rtwn_write_4(sc, R92C_RCR,
			    rtwn_read_4(sc, R92C_RCR) &
			    ~(R92C_RCR_CBSSID_DATA | R92C_RCR_CBSSID_BCN));

			/* Set gain for scanning. */
			reg = rtwn_bb_read(sc, R92C_OFDM0_AGCCORE1(0));
			reg = RW(reg, R92C_OFDM0_AGCCORE1_GAIN, 0x20);
			rtwn_bb_write(sc, R92C_OFDM0_AGCCORE1(0), reg);

			reg = rtwn_bb_read(sc, R92C_OFDM0_AGCCORE1(1));
			reg = RW(reg, R92C_OFDM0_AGCCORE1_GAIN, 0x20);
			rtwn_bb_write(sc, R92C_OFDM0_AGCCORE1(1), reg);
		}

		/* Make link LED blink during scan. */
		rtwn_set_led(sc, RTWN_LED_LINK, !sc->ledlink);

		/* Pause AC Tx queues. */
		rtwn_write_1(sc, R92C_TXPAUSE,
		    rtwn_read_1(sc, R92C_TXPAUSE) | 0x0f);
		rtwn_set_chan(sc, ic->ic_bss->ni_chan, NULL);
		timeout_add_msec(&sc->scan_to, 200);
		break;

	case IEEE80211_S_AUTH:
		/* Set initial gain under link. */
		reg = rtwn_bb_read(sc, R92C_OFDM0_AGCCORE1(0));
		reg = RW(reg, R92C_OFDM0_AGCCORE1_GAIN, 0x32);
		rtwn_bb_write(sc, R92C_OFDM0_AGCCORE1(0), reg);

		reg = rtwn_bb_read(sc, R92C_OFDM0_AGCCORE1(1));
		reg = RW(reg, R92C_OFDM0_AGCCORE1_GAIN, 0x32);
		rtwn_bb_write(sc, R92C_OFDM0_AGCCORE1(1), reg);

		rtwn_set_chan(sc, ic->ic_bss->ni_chan, NULL);
		break;
	case IEEE80211_S_ASSOC:
		break;
	case IEEE80211_S_RUN:
		if (ic->ic_opmode == IEEE80211_M_MONITOR) {
			rtwn_set_chan(sc, ic->ic_ibss_chan, NULL);

			/* Enable Rx of data frames. */
			rtwn_write_2(sc, R92C_RXFLTMAP2, 0xffff);

			/* Turn link LED on. */
			rtwn_set_led(sc, RTWN_LED_LINK, 1);
			break;
		}
		ni = ic->ic_bss;

		/* Set media status to 'Associated'. */
		reg = rtwn_read_4(sc, R92C_CR);
		reg = RW(reg, R92C_CR_NETTYPE, R92C_CR_NETTYPE_INFRA);
		rtwn_write_4(sc, R92C_CR, reg);

		/* Set BSSID. */
		rtwn_write_4(sc, R92C_BSSID + 0, LE_READ_4(&ni->ni_bssid[0]));
		rtwn_write_4(sc, R92C_BSSID + 4, LE_READ_2(&ni->ni_bssid[4]));

		if (ic->ic_curmode == IEEE80211_MODE_11B)
			rtwn_write_1(sc, R92C_INIRTS_RATE_SEL, 0);
		else	/* 802.11b/g */
			rtwn_write_1(sc, R92C_INIRTS_RATE_SEL, 3);

		/* Enable Rx of data frames. */
		rtwn_write_2(sc, R92C_RXFLTMAP2, 0xffff);

		/* Flush all AC queues. */
		rtwn_write_1(sc, R92C_TXPAUSE, 0);

		/* Set beacon interval. */
		rtwn_write_2(sc, R92C_BCN_INTERVAL, ni->ni_intval);

		/* Allow Rx from our BSSID only. */
		rtwn_write_4(sc, R92C_RCR,
		    rtwn_read_4(sc, R92C_RCR) |
		    R92C_RCR_CBSSID_DATA | R92C_RCR_CBSSID_BCN);

		/* Enable TSF synchronization. */
		rtwn_tsf_sync_enable(sc);

		rtwn_write_1(sc, R92C_SIFS_CCK + 1, 10);
		rtwn_write_1(sc, R92C_SIFS_OFDM + 1, 10);
		rtwn_write_1(sc, R92C_SPEC_SIFS + 1, 10);
		rtwn_write_1(sc, R92C_MAC_SPEC_SIFS + 1, 10);
		rtwn_write_1(sc, R92C_R2T_SIFS + 1, 10);
		rtwn_write_1(sc, R92C_T2T_SIFS + 1, 10);

		/* Intialize rate adaptation. */
		rtwn_ra_init(sc);
		/* Turn link LED on. */
		rtwn_set_led(sc, RTWN_LED_LINK, 1);

		sc->avg_pwdb = -1;	/* Reset average RSSI. */
		/* Reset temperature calibration state machine. */
		sc->thcal_state = 0;
		sc->thcal_lctemp = 0;
		/* Start periodic calibration. */
		timeout_add_sec(&sc->calib_to, 2);
		break;
	}
	(void)sc->sc_newstate(ic, nstate, arg);
	splx(s);

	return (0);
}

void
rtwn_updateedca(struct ieee80211com *ic)
{
	struct rtwn_softc *sc = ic->ic_softc;
	const uint16_t aci2reg[EDCA_NUM_AC] = {
		R92C_EDCA_BE_PARAM,
		R92C_EDCA_BK_PARAM,
		R92C_EDCA_VI_PARAM,
		R92C_EDCA_VO_PARAM
	};
	struct ieee80211_edca_ac_params *ac;
	int s, aci, aifs, slottime;

	s = splnet();
	slottime = (ic->ic_flags & IEEE80211_F_SHSLOT) ? 9 : 20;
	for (aci = 0; aci < EDCA_NUM_AC; aci++) {
		ac = &ic->ic_edca_ac[aci];
		/* AIFS[AC] = AIFSN[AC] * aSlotTime + aSIFSTime. */
		aifs = ac->ac_aifsn * slottime + 10;
		rtwn_write_4(sc, aci2reg[aci],
		    SM(R92C_EDCA_PARAM_TXOP, ac->ac_txoplimit) |
		    SM(R92C_EDCA_PARAM_ECWMIN, ac->ac_ecwmin) |
		    SM(R92C_EDCA_PARAM_ECWMAX, ac->ac_ecwmax) |
		    SM(R92C_EDCA_PARAM_AIFS, aifs));
	}
	splx(s);
}

int
rtwn_set_key(struct ieee80211com *ic, struct ieee80211_node *ni,
    struct ieee80211_key *k)
{
	struct rtwn_softc *sc = ic->ic_softc;
	static const uint8_t etherzeroaddr[6] = { 0 };
	const uint8_t *macaddr;
	uint8_t keybuf[16], algo;
	int i, entry;

	/* Defer setting of WEP keys until interface is brought up. */
	if ((ic->ic_if.if_flags & (IFF_UP | IFF_RUNNING)) !=
	    (IFF_UP | IFF_RUNNING))
		return (0);

	/* Map net80211 cipher to HW crypto algorithm. */
	switch (k->k_cipher) {
	case IEEE80211_CIPHER_WEP40:
		algo = R92C_CAM_ALGO_WEP40;
		break;
	case IEEE80211_CIPHER_WEP104:
		algo = R92C_CAM_ALGO_WEP104;
		break;
	case IEEE80211_CIPHER_TKIP:
		algo = R92C_CAM_ALGO_TKIP;
		break;
	case IEEE80211_CIPHER_CCMP:
		algo = R92C_CAM_ALGO_AES;
		break;
	default:
		/* Fallback to software crypto for other ciphers. */
		return (ieee80211_set_key(ic, ni, k));
	}
	if (k->k_flags & IEEE80211_KEY_GROUP) {
		macaddr = etherzeroaddr;
		entry = k->k_id;
	} else {
		macaddr = ic->ic_bss->ni_macaddr;
		entry = 4;
	}
	/* Write key. */
	memset(keybuf, 0, sizeof(keybuf));
	memcpy(keybuf, k->k_key, MIN(k->k_len, sizeof(keybuf)));
	for (i = 0; i < 4; i++) {
		rtwn_cam_write(sc, R92C_CAM_KEY(entry, i),
		    LE_READ_4(&keybuf[i * 4]));
	}
	/* Write CTL0 last since that will validate the CAM entry. */
	rtwn_cam_write(sc, R92C_CAM_CTL1(entry),
	    LE_READ_4(&macaddr[2]));
	rtwn_cam_write(sc, R92C_CAM_CTL0(entry),
	    SM(R92C_CAM_ALGO, algo) |
	    SM(R92C_CAM_KEYID, k->k_id) |
	    SM(R92C_CAM_MACLO, LE_READ_2(&macaddr[0])) |
	    R92C_CAM_VALID);

	return (0);
}

void
rtwn_delete_key(struct ieee80211com *ic, struct ieee80211_node *ni,
    struct ieee80211_key *k)
{
	struct rtwn_softc *sc = ic->ic_softc;
	int i, entry;

	if (!(ic->ic_if.if_flags & IFF_RUNNING) ||
	    ic->ic_state != IEEE80211_S_RUN)
		return;	/* Nothing to do. */

	if (k->k_flags & IEEE80211_KEY_GROUP)
		entry = k->k_id;
	else
		entry = 4;
	rtwn_cam_write(sc, R92C_CAM_CTL0(entry), 0);
	rtwn_cam_write(sc, R92C_CAM_CTL1(entry), 0);
	/* Clear key. */
	for (i = 0; i < 4; i++)
		rtwn_cam_write(sc, R92C_CAM_KEY(entry, i), 0);
}

void
rtwn_update_avgrssi(struct rtwn_softc *sc, int rate, int8_t rssi)
{
	int pwdb;

	/* Convert antenna signal to percentage. */
	if (rssi <= -100 || rssi >= 20)
		pwdb = 0;
	else if (rssi >= 0)
		pwdb = 100;
	else
		pwdb = 100 + rssi;
	if (rate <= 3) {
		/* CCK gain is smaller than OFDM/MCS gain. */
		pwdb += 6;
		if (pwdb > 100)
			pwdb = 100;
		if (pwdb <= 14)
			pwdb -= 4;
		else if (pwdb <= 26)
			pwdb -= 8;
		else if (pwdb <= 34)
			pwdb -= 6;
		else if (pwdb <= 42)
			pwdb -= 2;
	}
	if (sc->avg_pwdb == -1)	/* Init. */
		sc->avg_pwdb = pwdb;
	else if (sc->avg_pwdb < pwdb)
		sc->avg_pwdb = ((sc->avg_pwdb * 19 + pwdb) / 20) + 1;
	else
		sc->avg_pwdb = ((sc->avg_pwdb * 19 + pwdb) / 20);
	DPRINTFN(4, ("PWDB=%d EMA=%d\n", pwdb, sc->avg_pwdb));
}

int8_t
rtwn_get_rssi(struct rtwn_softc *sc, int rate, void *physt)
{
	static const int8_t cckoff[] = { 16, -12, -26, -46 };
	struct r92c_rx_phystat *phy;
	struct r92c_rx_cck *cck;
	uint8_t rpt;
	int8_t rssi;

	if (rate <= 3) {
		cck = (struct r92c_rx_cck *)physt;
		if (sc->sc_flags & RTWN_FLAG_CCK_HIPWR) {
			rpt = (cck->agc_rpt >> 5) & 0x3;
			rssi = (cck->agc_rpt & 0x1f) << 1;
		} else {
			rpt = (cck->agc_rpt >> 6) & 0x3;
			rssi = cck->agc_rpt & 0x3e;
		}
		rssi = cckoff[rpt] - rssi;
	} else {	/* OFDM/HT. */
		phy = (struct r92c_rx_phystat *)physt;
		rssi = ((letoh32(phy->phydw1) >> 1) & 0x7f) - 110;
	}
	return (rssi);
}

void
rtwn_rx_frame(struct rtwn_softc *sc, struct r92c_rx_desc *rx_desc,
    struct rtwn_rx_data *rx_data, int desc_idx)
{
	struct ieee80211com *ic = &sc->sc_ic;
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
		rssi = rtwn_get_rssi(sc, rate, phy);
		/* Update our average RSSI. */
		rtwn_update_avgrssi(sc, rate, rssi);
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
	m->m_pkthdr.rcvif = ifp;
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
rtwn_tx(struct rtwn_softc *sc, struct mbuf *m, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_frame *wh;
	struct ieee80211_key *k = NULL;
	struct rtwn_tx_ring *tx_ring;
	struct rtwn_tx_data *data;
	struct r92c_tx_desc *txd;
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
		    SM(R92C_TXDW1_MACID, RTWN_MACID_BSS) |
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
	rtwn_write_2(sc, R92C_PCIE_CTRL_REG, (1 << qid));

	return (0);
}

void
rtwn_tx_done(struct rtwn_softc *sc, int qid)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct rtwn_tx_ring *tx_ring = &sc->tx_ring[qid];
	struct rtwn_tx_data *tx_data;
	struct r92c_tx_desc *tx_desc;
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
		sc->sc_tx_timer = 0;
		tx_ring->queued--;
	}

	if (tx_ring->queued < (RTWN_TX_LIST_COUNT - 1))
		sc->qfullmsk &= ~(1 << qid);
	
	if (sc->qfullmsk == 0) {
		ifp->if_flags &= (~IFF_OACTIVE);
		(*ifp->if_start)(ifp);
	}
}

void
rtwn_start(struct ifnet *ifp)
{
	struct rtwn_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni;
	struct mbuf *m;

	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

	for (;;) {
		if (sc->qfullmsk != 0) {
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}
		/* Send pending management frames first. */
		IF_DEQUEUE(&ic->ic_mgtq, m);
		if (m != NULL) {
			ni = m->m_pkthdr.ph_cookie;
			goto sendit;
		}
		if (ic->ic_state != IEEE80211_S_RUN)
			break;

		/* Encapsulate and send data frames. */
		IFQ_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL)
			break;
#if NBPFILTER > 0
		if (ifp->if_bpf != NULL)
			bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_OUT);
#endif
		if ((m = ieee80211_encap(ifp, m, &ni)) == NULL)
			continue;
sendit:
#if NBPFILTER > 0
		if (ic->ic_rawbpf != NULL)
			bpf_mtap(ic->ic_rawbpf, m, BPF_DIRECTION_OUT);
#endif
		if (rtwn_tx(sc, m, ni) != 0) {
			ieee80211_release_node(ic, ni);
			ifp->if_oerrors++;
			continue;
		}

		sc->sc_tx_timer = 5;
		ifp->if_timer = 1;
	}
}

void
rtwn_watchdog(struct ifnet *ifp)
{
	struct rtwn_softc *sc = ifp->if_softc;

	ifp->if_timer = 0;

	if (sc->sc_tx_timer > 0) {
		if (--sc->sc_tx_timer == 0) {
			printf("%s: device timeout\n", sc->sc_dev.dv_xname);
			task_add(systq, &sc->init_task);
			ifp->if_oerrors++;
			return;
		}
		ifp->if_timer = 1;
	}
	ieee80211_watchdog(ifp);
}

int
rtwn_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct rtwn_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifaddr *ifa;
	struct ifreq *ifr;
	int s, error = 0;

	s = splnet();
	/*
	 * Prevent processes from entering this function while another
	 * process is tsleep'ing in it.
	 */
	while ((sc->sc_flags & RTWN_FLAG_BUSY) && error == 0)
		error = tsleep(&sc->sc_flags, PCATCH, "rtwnioc", 0);
	if (error != 0) {
		splx(s);
		return error;
	}
	sc->sc_flags |= RTWN_FLAG_BUSY;

	switch (cmd) {
	case SIOCSIFADDR:
		ifa = (struct ifaddr *)data;
		ifp->if_flags |= IFF_UP;
		if (ifa->ifa_addr->sa_family == AF_INET)
			arp_ifinit(&ic->ic_ac, ifa);
		/* FALLTHROUGH */
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (!(ifp->if_flags & IFF_RUNNING))
				rtwn_init(ifp);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				rtwn_stop(ifp);
		}
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		ifr = (struct ifreq *)data;
		error = (cmd == SIOCADDMULTI) ?
		    ether_addmulti(ifr, &ic->ic_ac) :
		    ether_delmulti(ifr, &ic->ic_ac);
		if (error == ENETRESET)
			error = 0;
		break;
	case SIOCS80211CHANNEL:
		error = ieee80211_ioctl(ifp, cmd, data);
		if (error == ENETRESET &&
		    ic->ic_opmode == IEEE80211_M_MONITOR) {
			if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) ==
			    (IFF_UP | IFF_RUNNING))
				rtwn_set_chan(sc, ic->ic_ibss_chan, NULL);
			error = 0;
		}
		break;
	default:
		error = ieee80211_ioctl(ifp, cmd, data);
	}

	if (error == ENETRESET) {
		if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) ==
		    (IFF_UP | IFF_RUNNING)) {
			rtwn_stop(ifp);
			rtwn_init(ifp);
		}
		error = 0;
	}
	sc->sc_flags &= ~RTWN_FLAG_BUSY;
	wakeup(&sc->sc_flags);
	splx(s);

	return (error);
}

int
rtwn_power_on(struct rtwn_softc *sc)
{
	uint32_t reg;
	int ntries;

	/* Wait for autoload done bit. */
	for (ntries = 0; ntries < 1000; ntries++) {
		if (rtwn_read_1(sc, R92C_APS_FSMCO) & R92C_APS_FSMCO_PFM_ALDN)
			break;
		DELAY(5);
	}
	if (ntries == 1000) {
		printf("%s: timeout waiting for chip autoload\n",
		    sc->sc_dev.dv_xname);
		return (ETIMEDOUT);
	}

	/* Unlock ISO/CLK/Power control register. */
	rtwn_write_1(sc, R92C_RSV_CTRL, 0);

	/* TODO: check if we need this for 8188CE */
	if (sc->board_type != R92C_BOARD_TYPE_DONGLE) {
		/* bt coex */
		reg = rtwn_read_4(sc, R92C_APS_FSMCO);
		reg |= (R92C_APS_FSMCO_SOP_ABG |
			R92C_APS_FSMCO_SOP_AMB |
			R92C_APS_FSMCO_XOP_BTCK);
		rtwn_write_4(sc, R92C_APS_FSMCO, reg);
	}

	/* Move SPS into PWM mode. */
	rtwn_write_1(sc, R92C_SPS0_CTRL, 0x2b);

	/* Set low byte to 0x0f, leave others unchanged. */
	rtwn_write_4(sc, R92C_AFE_XTAL_CTRL,
	    (rtwn_read_4(sc, R92C_AFE_XTAL_CTRL) & 0xffffff00) | 0x0f);

	/* TODO: check if we need this for 8188CE */
	if (sc->board_type != R92C_BOARD_TYPE_DONGLE) {
		/* bt coex */
		reg = rtwn_read_4(sc, R92C_AFE_XTAL_CTRL);
		reg &= (~0x00024800); /* XXX magic from linux */
		rtwn_write_4(sc, R92C_AFE_XTAL_CTRL, reg);
	}

	rtwn_write_2(sc, R92C_SYS_ISO_CTRL,
	  (rtwn_read_2(sc, R92C_SYS_ISO_CTRL) & 0xff) |
	  R92C_SYS_ISO_CTRL_PWC_EV12V | R92C_SYS_ISO_CTRL_DIOR);
	DELAY(200);

	/* TODO: linux does additional btcoex stuff here */

	/* Auto enable WLAN. */
	rtwn_write_2(sc, R92C_APS_FSMCO,
	    rtwn_read_2(sc, R92C_APS_FSMCO) | R92C_APS_FSMCO_APFM_ONMAC);
	for (ntries = 0; ntries < 1000; ntries++) {
		if (!(rtwn_read_2(sc, R92C_APS_FSMCO) &
		    R92C_APS_FSMCO_APFM_ONMAC))
			break;
		DELAY(5);
	}
	if (ntries == 1000) {
		printf("%s: timeout waiting for MAC auto ON\n",
		    sc->sc_dev.dv_xname);
		return (ETIMEDOUT);
	}

	/* Enable radio, GPIO and LED functions. */
	rtwn_write_2(sc, R92C_APS_FSMCO,
	    R92C_APS_FSMCO_AFSM_PCIE |
	    R92C_APS_FSMCO_PDN_EN |
	    R92C_APS_FSMCO_PFM_ALDN);
	/* Release RF digital isolation. */
	rtwn_write_2(sc, R92C_SYS_ISO_CTRL,
	    rtwn_read_2(sc, R92C_SYS_ISO_CTRL) & ~R92C_SYS_ISO_CTRL_DIOR);

	if (sc->chip & RTWN_CHIP_92C)
		rtwn_write_1(sc, R92C_PCIE_CTRL_REG + 3, 0x77);
	else
		rtwn_write_1(sc, R92C_PCIE_CTRL_REG + 3, 0x22);

	rtwn_write_4(sc, R92C_INT_MIG, 0);

	if (sc->board_type != R92C_BOARD_TYPE_DONGLE) {
		/* bt coex */
		reg = rtwn_read_4(sc, R92C_AFE_XTAL_CTRL + 2);
		reg &= 0xfd; /* XXX magic from linux */
		rtwn_write_4(sc, R92C_AFE_XTAL_CTRL + 2, reg);
	}

	rtwn_write_1(sc, R92C_GPIO_MUXCFG,
	    rtwn_read_1(sc, R92C_GPIO_MUXCFG) & ~R92C_GPIO_MUXCFG_RFKILL);

	reg = rtwn_read_1(sc, R92C_GPIO_IO_SEL);
	if (!(reg & R92C_GPIO_IO_SEL_RFKILL)) {
		printf("%s: radio is disabled by hardware switch\n",
		    sc->sc_dev.dv_xname);
		return (EPERM);	/* :-) */
	}

	/* Initialize MAC. */
	reg = rtwn_read_1(sc, R92C_APSD_CTRL);
	rtwn_write_1(sc, R92C_APSD_CTRL,
	    rtwn_read_1(sc, R92C_APSD_CTRL) & ~R92C_APSD_CTRL_OFF);
	for (ntries = 0; ntries < 200; ntries++) {
		if (!(rtwn_read_1(sc, R92C_APSD_CTRL) &
		    R92C_APSD_CTRL_OFF_STATUS))
			break;
		DELAY(500);
	}
	if (ntries == 200) {
		printf("%s: timeout waiting for MAC initialization\n",
		    sc->sc_dev.dv_xname);
		return (ETIMEDOUT);
	}

	/* Enable MAC DMA/WMAC/SCHEDULE/SEC blocks. */
	reg = rtwn_read_2(sc, R92C_CR);
	reg |= R92C_CR_HCI_TXDMA_EN | R92C_CR_HCI_RXDMA_EN |
	    R92C_CR_TXDMA_EN | R92C_CR_RXDMA_EN | R92C_CR_PROTOCOL_EN |
	    R92C_CR_SCHEDULE_EN | R92C_CR_MACTXEN | R92C_CR_MACRXEN |
	    R92C_CR_ENSEC;
	rtwn_write_2(sc, R92C_CR, reg);

	rtwn_write_1(sc, 0xfe10, 0x19);

	return (0);
}

int
rtwn_llt_init(struct rtwn_softc *sc)
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

void
rtwn_fw_reset(struct rtwn_softc *sc)
{
	uint16_t reg;
	int ntries;

	/* Tell 8051 to reset itself. */
	rtwn_write_1(sc, R92C_HMETFR + 3, 0x20);

	/* Wait until 8051 resets by itself. */
	for (ntries = 0; ntries < 100; ntries++) {
		reg = rtwn_read_2(sc, R92C_SYS_FUNC_EN);
		if (!(reg & R92C_SYS_FUNC_EN_CPUEN))
			goto sleep;
		DELAY(50);
	}
	/* Force 8051 reset. */
	rtwn_write_2(sc, R92C_SYS_FUNC_EN, reg & ~R92C_SYS_FUNC_EN_CPUEN);
sleep:
	/* 
	 * We must sleep for one second to let the firmware settle.
	 * Accessing registers too early will hang the whole system.
	 */
	tsleep(&reg, 0, "rtwnrst", hz);
}

int
rtwn_fw_loadpage(struct rtwn_softc *sc, int page, uint8_t *buf, int len)
{
	uint32_t reg;
	int off, mlen, error = 0, i;

	reg = rtwn_read_4(sc, R92C_MCUFWDL);
	reg = RW(reg, R92C_MCUFWDL_PAGE, page);
	rtwn_write_4(sc, R92C_MCUFWDL, reg);

	DELAY(5);

	off = R92C_FW_START_ADDR;
	while (len > 0) {
		if (len > 196)
			mlen = 196;
		else if (len > 4)
			mlen = 4;
		else
			mlen = 1;
		for (i = 0; i < mlen; i++)
			rtwn_write_1(sc, off++, buf[i]);
		buf += mlen;
		len -= mlen;
	}

	return (error);
}

int
rtwn_load_firmware(struct rtwn_softc *sc)
{
	const struct r92c_fw_hdr *hdr;
	const char *name;
	u_char *fw, *ptr;
	size_t len;
	uint32_t reg;
	int mlen, ntries, page, error;

	/* Read firmware image from the filesystem. */
	if ((sc->chip & (RTWN_CHIP_UMC_A_CUT | RTWN_CHIP_92C)) ==
	    RTWN_CHIP_UMC_A_CUT)
		name = "rtwn-rtl8192cfwU";
	else
		name = "rtwn-rtl8192cfwU_B";
	if ((error = loadfirmware(name, &fw, &len)) != 0) {
		printf("%s: could not read firmware %s (error %d)\n",
		    sc->sc_dev.dv_xname, name, error);
		return (error);
	}
	if (len < sizeof(*hdr)) {
		printf("%s: firmware too short\n", sc->sc_dev.dv_xname);
		error = EINVAL;
		goto fail;
	}
	ptr = fw;
	hdr = (const struct r92c_fw_hdr *)ptr;
	/* Check if there is a valid FW header and skip it. */
	if ((letoh16(hdr->signature) >> 4) == 0x88c ||
	    (letoh16(hdr->signature) >> 4) == 0x92c) {
		DPRINTF(("FW V%d.%d %02d-%02d %02d:%02d\n",
		    letoh16(hdr->version), letoh16(hdr->subversion),
		    hdr->month, hdr->date, hdr->hour, hdr->minute));
		ptr += sizeof(*hdr);
		len -= sizeof(*hdr);
	}

	if (rtwn_read_1(sc, R92C_MCUFWDL) & R92C_MCUFWDL_RAM_DL_SEL)
		rtwn_fw_reset(sc);

	/* Enable FW download. */
	rtwn_write_2(sc, R92C_SYS_FUNC_EN,
	    rtwn_read_2(sc, R92C_SYS_FUNC_EN) |
	    R92C_SYS_FUNC_EN_CPUEN);
	rtwn_write_1(sc, R92C_MCUFWDL,
	    rtwn_read_1(sc, R92C_MCUFWDL) | R92C_MCUFWDL_EN);
	rtwn_write_1(sc, R92C_MCUFWDL + 2,
	    rtwn_read_1(sc, R92C_MCUFWDL + 2) & ~0x08);

	/* Reset the FWDL checksum. */
	rtwn_write_1(sc, R92C_MCUFWDL,
	    rtwn_read_1(sc, R92C_MCUFWDL) | R92C_MCUFWDL_CHKSUM_RPT);

	for (page = 0; len > 0; page++) {
		mlen = MIN(len, R92C_FW_PAGE_SIZE);
		error = rtwn_fw_loadpage(sc, page, ptr, mlen);
		if (error != 0) {
			printf("%s: could not load firmware page %d\n",
			    sc->sc_dev.dv_xname, page);
			goto fail;
		}
		ptr += mlen;
		len -= mlen;
	}

	/* Disable FW download. */
	rtwn_write_1(sc, R92C_MCUFWDL,
	    rtwn_read_1(sc, R92C_MCUFWDL) & ~R92C_MCUFWDL_EN);
	rtwn_write_1(sc, R92C_MCUFWDL + 1, 0);

	/* Wait for checksum report. */
	for (ntries = 0; ntries < 1000; ntries++) {
		if (rtwn_read_4(sc, R92C_MCUFWDL) & R92C_MCUFWDL_CHKSUM_RPT)
			break;
		DELAY(5);
	}
	if (ntries == 1000) {
		printf("%s: timeout waiting for checksum report\n",
		    sc->sc_dev.dv_xname);
		error = ETIMEDOUT;
		goto fail;
	}

	reg = rtwn_read_4(sc, R92C_MCUFWDL);
	reg = (reg & ~R92C_MCUFWDL_WINTINI_RDY) | R92C_MCUFWDL_RDY;
	rtwn_write_4(sc, R92C_MCUFWDL, reg);
	/* Wait for firmware readiness. */
	for (ntries = 0; ntries < 1000; ntries++) {
		if (rtwn_read_4(sc, R92C_MCUFWDL) & R92C_MCUFWDL_WINTINI_RDY)
			break;
		DELAY(5);
	}
	if (ntries == 1000) {
		printf("%s: timeout waiting for firmware readiness\n",
		    sc->sc_dev.dv_xname);
		error = ETIMEDOUT;
		goto fail;
	}
 fail:
	free(fw, M_DEVBUF, 0);
	return (error);
}

int
rtwn_dma_init(struct rtwn_softc *sc)
{
	uint32_t reg;
	int error;

	/* Initialize LLT table. */
	error = rtwn_llt_init(sc);
	if (error != 0)
		return error;

	/* Set number of pages for normal priority queue. */
	rtwn_write_2(sc, R92C_RQPN_NPQ, 0);
	rtwn_write_4(sc, R92C_RQPN,
	    /* Set number of pages for public queue. */
	    SM(R92C_RQPN_PUBQ, R92C_PUBQ_NPAGES) |
	    /* Set number of pages for high priority queue. */
	    SM(R92C_RQPN_HPQ, R92C_HPQ_NPAGES) |
	    /* Set number of pages for low priority queue. */
	    SM(R92C_RQPN_LPQ, R92C_LPQ_NPAGES) |
	    /* Load values. */
	    R92C_RQPN_LD);

	rtwn_write_1(sc, R92C_TXPKTBUF_BCNQ_BDNY, R92C_TX_PAGE_BOUNDARY);
	rtwn_write_1(sc, R92C_TXPKTBUF_MGQ_BDNY, R92C_TX_PAGE_BOUNDARY);
	rtwn_write_1(sc, R92C_TXPKTBUF_WMAC_LBK_BF_HD, R92C_TX_PAGE_BOUNDARY);
	rtwn_write_1(sc, R92C_TRXFF_BNDY, R92C_TX_PAGE_BOUNDARY);
	rtwn_write_1(sc, R92C_TDECTRL + 1, R92C_TX_PAGE_BOUNDARY);

	reg = rtwn_read_2(sc, R92C_TRXDMA_CTRL);
	reg &= ~R92C_TRXDMA_CTRL_QMAP_M;
	reg |= 0xF771; 
	rtwn_write_2(sc, R92C_TRXDMA_CTRL, reg);

	rtwn_write_4(sc, R92C_TCR, R92C_TCR_CFENDFORM | (1 << 12) | (1 << 13));

	/* Configure Tx DMA. */
	rtwn_write_4(sc, R92C_BKQ_DESA,
		sc->tx_ring[RTWN_BK_QUEUE].map->dm_segs[0].ds_addr);
	rtwn_write_4(sc, R92C_BEQ_DESA,
		sc->tx_ring[RTWN_BE_QUEUE].map->dm_segs[0].ds_addr);
	rtwn_write_4(sc, R92C_VIQ_DESA,
		sc->tx_ring[RTWN_VI_QUEUE].map->dm_segs[0].ds_addr);
	rtwn_write_4(sc, R92C_VOQ_DESA,
		sc->tx_ring[RTWN_VO_QUEUE].map->dm_segs[0].ds_addr);
	rtwn_write_4(sc, R92C_BCNQ_DESA,
		sc->tx_ring[RTWN_BEACON_QUEUE].map->dm_segs[0].ds_addr);
	rtwn_write_4(sc, R92C_MGQ_DESA,
		sc->tx_ring[RTWN_MGNT_QUEUE].map->dm_segs[0].ds_addr);
	rtwn_write_4(sc, R92C_HQ_DESA,
		sc->tx_ring[RTWN_HIGH_QUEUE].map->dm_segs[0].ds_addr);

	/* Configure Rx DMA. */
	rtwn_write_4(sc, R92C_RX_DESA, sc->rx_ring.map->dm_segs[0].ds_addr);

	/* Set Tx/Rx transfer page boundary. */
	rtwn_write_2(sc, R92C_TRXFF_BNDY + 2, 0x27ff);

	/* Set Tx/Rx transfer page size. */
	rtwn_write_1(sc, R92C_PBP,
	    SM(R92C_PBP_PSRX, R92C_PBP_128) |
	    SM(R92C_PBP_PSTX, R92C_PBP_128));
	return (0);
}

void
rtwn_mac_init(struct rtwn_softc *sc)
{
	int i;

	/* Write MAC initialization values. */
	for (i = 0; i < nitems(rtl8192ce_mac); i++)
		rtwn_write_1(sc, rtl8192ce_mac[i].reg, rtl8192ce_mac[i].val);
}

void
rtwn_bb_init(struct rtwn_softc *sc)
{
	const struct rtwn_bb_prog *prog;
	uint32_t reg;
	int i;

	/* Enable BB and RF. */
	rtwn_write_2(sc, R92C_SYS_FUNC_EN,
	    rtwn_read_2(sc, R92C_SYS_FUNC_EN) |
	    R92C_SYS_FUNC_EN_BBRSTB | R92C_SYS_FUNC_EN_BB_GLB_RST |
	    R92C_SYS_FUNC_EN_DIO_RF);

	rtwn_write_2(sc, R92C_AFE_PLL_CTRL, 0xdb83);

	rtwn_write_1(sc, R92C_RF_CTRL,
	    R92C_RF_CTRL_EN | R92C_RF_CTRL_RSTB | R92C_RF_CTRL_SDMRSTB);

	rtwn_write_1(sc, R92C_SYS_FUNC_EN,
	    R92C_SYS_FUNC_EN_DIO_PCIE | R92C_SYS_FUNC_EN_PCIEA |
	    R92C_SYS_FUNC_EN_PPLL | R92C_SYS_FUNC_EN_BB_GLB_RST |
	    R92C_SYS_FUNC_EN_BBRSTB);

	rtwn_write_1(sc, R92C_AFE_XTAL_CTRL + 1, 0x80);

	rtwn_write_4(sc, R92C_LEDCFG0,
	    rtwn_read_4(sc, R92C_LEDCFG0) | 0x00800000);

	/* Select BB programming. */ 
	prog = (sc->chip & RTWN_CHIP_92C) ?
	    &rtl8192ce_bb_prog_2t : &rtl8192ce_bb_prog_1t;

	/* Write BB initialization values. */
	for (i = 0; i < prog->count; i++) {
		rtwn_bb_write(sc, prog->regs[i], prog->vals[i]);
		DELAY(1);
	}

	if (sc->chip & RTWN_CHIP_92C_1T2R) {
		/* 8192C 1T only configuration. */
		reg = rtwn_bb_read(sc, R92C_FPGA0_TXINFO);
		reg = (reg & ~0x00000003) | 0x2;
		rtwn_bb_write(sc, R92C_FPGA0_TXINFO, reg);

		reg = rtwn_bb_read(sc, R92C_FPGA1_TXINFO);
		reg = (reg & ~0x00300033) | 0x00200022;
		rtwn_bb_write(sc, R92C_FPGA1_TXINFO, reg);

		reg = rtwn_bb_read(sc, R92C_CCK0_AFESETTING);
		reg = (reg & ~0xff000000) | 0x45 << 24;
		rtwn_bb_write(sc, R92C_CCK0_AFESETTING, reg);

		reg = rtwn_bb_read(sc, R92C_OFDM0_TRXPATHENA);
		reg = (reg & ~0x000000ff) | 0x23;
		rtwn_bb_write(sc, R92C_OFDM0_TRXPATHENA, reg);

		reg = rtwn_bb_read(sc, R92C_OFDM0_AGCPARAM1);
		reg = (reg & ~0x00000030) | 1 << 4;
		rtwn_bb_write(sc, R92C_OFDM0_AGCPARAM1, reg);

		reg = rtwn_bb_read(sc, 0xe74);
		reg = (reg & ~0x0c000000) | 2 << 26;
		rtwn_bb_write(sc, 0xe74, reg);
		reg = rtwn_bb_read(sc, 0xe78);
		reg = (reg & ~0x0c000000) | 2 << 26;
		rtwn_bb_write(sc, 0xe78, reg);
		reg = rtwn_bb_read(sc, 0xe7c);
		reg = (reg & ~0x0c000000) | 2 << 26;
		rtwn_bb_write(sc, 0xe7c, reg);
		reg = rtwn_bb_read(sc, 0xe80);
		reg = (reg & ~0x0c000000) | 2 << 26;
		rtwn_bb_write(sc, 0xe80, reg);
		reg = rtwn_bb_read(sc, 0xe88);
		reg = (reg & ~0x0c000000) | 2 << 26;
		rtwn_bb_write(sc, 0xe88, reg);
	}

	/* Write AGC values. */
	for (i = 0; i < prog->agccount; i++) {
		rtwn_bb_write(sc, R92C_OFDM0_AGCRSSITABLE,
		    prog->agcvals[i]);
		DELAY(1);
	}

	if (rtwn_bb_read(sc, R92C_HSSI_PARAM2(0)) &
	    R92C_HSSI_PARAM2_CCK_HIPWR)
		sc->sc_flags |= RTWN_FLAG_CCK_HIPWR;
}

void
rtwn_rf_init(struct rtwn_softc *sc)
{
	const struct rtwn_rf_prog *prog;
	uint32_t reg, type;
	int i, j, idx, off;

	/* Select RF programming based on board type. */
	if (!(sc->chip & RTWN_CHIP_92C)) {
		if (sc->board_type == R92C_BOARD_TYPE_MINICARD)
			prog = rtl8188ce_rf_prog;
		else if (sc->board_type == R92C_BOARD_TYPE_HIGHPA)
			prog = rtl8188ru_rf_prog;
		else
			prog = rtl8188cu_rf_prog;
	} else
		prog = rtl8192ce_rf_prog;

	for (i = 0; i < sc->nrxchains; i++) {
		/* Save RF_ENV control type. */
		idx = i / 2;
		off = (i % 2) * 16;
		reg = rtwn_bb_read(sc, R92C_FPGA0_RFIFACESW(idx));
		type = (reg >> off) & 0x10;

		/* Set RF_ENV enable. */
		reg = rtwn_bb_read(sc, R92C_FPGA0_RFIFACEOE(i));
		reg |= 0x100000;
		rtwn_bb_write(sc, R92C_FPGA0_RFIFACEOE(i), reg);
		DELAY(1);
		/* Set RF_ENV output high. */
		reg = rtwn_bb_read(sc, R92C_FPGA0_RFIFACEOE(i));
		reg |= 0x10;
		rtwn_bb_write(sc, R92C_FPGA0_RFIFACEOE(i), reg);
		DELAY(1);
		/* Set address and data lengths of RF registers. */
		reg = rtwn_bb_read(sc, R92C_HSSI_PARAM2(i));
		reg &= ~R92C_HSSI_PARAM2_ADDR_LENGTH;
		rtwn_bb_write(sc, R92C_HSSI_PARAM2(i), reg);
		DELAY(1);
		reg = rtwn_bb_read(sc, R92C_HSSI_PARAM2(i));
		reg &= ~R92C_HSSI_PARAM2_DATA_LENGTH;
		rtwn_bb_write(sc, R92C_HSSI_PARAM2(i), reg);
		DELAY(1);

		/* Write RF initialization values for this chain. */
		for (j = 0; j < prog[i].count; j++) {
			if (prog[i].regs[j] >= 0xf9 &&
			    prog[i].regs[j] <= 0xfe) {
				/*
				 * These are fake RF registers offsets that
				 * indicate a delay is required.
				 */
				DELAY(50);
				continue;
			}
			rtwn_rf_write(sc, i, prog[i].regs[j],
			    prog[i].vals[j]);
			DELAY(1);
		}

		/* Restore RF_ENV control type. */
		reg = rtwn_bb_read(sc, R92C_FPGA0_RFIFACESW(idx));
		reg &= ~(0x10 << off) | (type << off);
		rtwn_bb_write(sc, R92C_FPGA0_RFIFACESW(idx), reg);

		/* Cache RF register CHNLBW. */
		sc->rf_chnlbw[i] = rtwn_rf_read(sc, i, R92C_RF_CHNLBW);
	}

	if ((sc->chip & (RTWN_CHIP_UMC_A_CUT | RTWN_CHIP_92C)) ==
	    RTWN_CHIP_UMC_A_CUT) {
		rtwn_rf_write(sc, 0, R92C_RF_RX_G1, 0x30255);
		rtwn_rf_write(sc, 0, R92C_RF_RX_G2, 0x50a00);
	}
}

void
rtwn_cam_init(struct rtwn_softc *sc)
{
	/* Invalidate all CAM entries. */
	rtwn_write_4(sc, R92C_CAMCMD,
	    R92C_CAMCMD_POLLING | R92C_CAMCMD_CLR);
}

void
rtwn_pa_bias_init(struct rtwn_softc *sc)
{
	uint8_t reg;
	int i;

	for (i = 0; i < sc->nrxchains; i++) {
		if (sc->pa_setting & (1 << i))
			continue;
		rtwn_rf_write(sc, i, R92C_RF_IPA, 0x0f406);
		rtwn_rf_write(sc, i, R92C_RF_IPA, 0x4f406);
		rtwn_rf_write(sc, i, R92C_RF_IPA, 0x8f406);
		rtwn_rf_write(sc, i, R92C_RF_IPA, 0xcf406);
	}
	if (!(sc->pa_setting & 0x10)) {
		reg = rtwn_read_1(sc, 0x16);
		reg = (reg & ~0xf0) | 0x90;
		rtwn_write_1(sc, 0x16, reg);
	}
}

void
rtwn_rxfilter_init(struct rtwn_softc *sc)
{
	/* Initialize Rx filter. */
	/* TODO: use better filter for monitor mode. */
	rtwn_write_4(sc, R92C_RCR,
	    R92C_RCR_AAP | R92C_RCR_APM | R92C_RCR_AM | R92C_RCR_AB |
	    R92C_RCR_APP_ICV | R92C_RCR_AMF | R92C_RCR_HTC_LOC_CTRL |
	    R92C_RCR_APP_MIC | R92C_RCR_APP_PHYSTS);
	/* Accept all multicast frames. */
	rtwn_write_4(sc, R92C_MAR + 0, 0xffffffff);
	rtwn_write_4(sc, R92C_MAR + 4, 0xffffffff);
	/* Accept all management frames. */
	rtwn_write_2(sc, R92C_RXFLTMAP0, 0xffff);
	/* Reject all control frames. */
	rtwn_write_2(sc, R92C_RXFLTMAP1, 0x0000);
	/* Accept all data frames. */
	rtwn_write_2(sc, R92C_RXFLTMAP2, 0xffff);
}

void
rtwn_edca_init(struct rtwn_softc *sc)
{
	rtwn_write_2(sc, R92C_SPEC_SIFS, 0x1010);
	rtwn_write_2(sc, R92C_MAC_SPEC_SIFS, 0x1010);
	rtwn_write_2(sc, R92C_SIFS_CCK, 0x1010);
	rtwn_write_2(sc, R92C_SIFS_OFDM, 0x0e0e);
	rtwn_write_4(sc, R92C_EDCA_BE_PARAM, 0x005ea42b);
	rtwn_write_4(sc, R92C_EDCA_BK_PARAM, 0x0000a44f);
	rtwn_write_4(sc, R92C_EDCA_VI_PARAM, 0x005e4322);
	rtwn_write_4(sc, R92C_EDCA_VO_PARAM, 0x002f3222);
}

void
rtwn_write_txpower(struct rtwn_softc *sc, int chain,
    uint16_t power[RTWN_RIDX_COUNT])
{
	uint32_t reg;

	/* Write per-CCK rate Tx power. */
	if (chain == 0) {
		reg = rtwn_bb_read(sc, R92C_TXAGC_A_CCK1_MCS32);
		reg = RW(reg, R92C_TXAGC_A_CCK1,  power[0]);
		rtwn_bb_write(sc, R92C_TXAGC_A_CCK1_MCS32, reg);
		reg = rtwn_bb_read(sc, R92C_TXAGC_B_CCK11_A_CCK2_11);
		reg = RW(reg, R92C_TXAGC_A_CCK2,  power[1]);
		reg = RW(reg, R92C_TXAGC_A_CCK55, power[2]);
		reg = RW(reg, R92C_TXAGC_A_CCK11, power[3]);
		rtwn_bb_write(sc, R92C_TXAGC_B_CCK11_A_CCK2_11, reg);
	} else {
		reg = rtwn_bb_read(sc, R92C_TXAGC_B_CCK1_55_MCS32);
		reg = RW(reg, R92C_TXAGC_B_CCK1,  power[0]);
		reg = RW(reg, R92C_TXAGC_B_CCK2,  power[1]);
		reg = RW(reg, R92C_TXAGC_B_CCK55, power[2]);
		rtwn_bb_write(sc, R92C_TXAGC_B_CCK1_55_MCS32, reg);
		reg = rtwn_bb_read(sc, R92C_TXAGC_B_CCK11_A_CCK2_11);
		reg = RW(reg, R92C_TXAGC_B_CCK11, power[3]);
		rtwn_bb_write(sc, R92C_TXAGC_B_CCK11_A_CCK2_11, reg);
	}
	/* Write per-OFDM rate Tx power. */
	rtwn_bb_write(sc, R92C_TXAGC_RATE18_06(chain),
	    SM(R92C_TXAGC_RATE06, power[ 4]) |
	    SM(R92C_TXAGC_RATE09, power[ 5]) |
	    SM(R92C_TXAGC_RATE12, power[ 6]) |
	    SM(R92C_TXAGC_RATE18, power[ 7]));
	rtwn_bb_write(sc, R92C_TXAGC_RATE54_24(chain),
	    SM(R92C_TXAGC_RATE24, power[ 8]) |
	    SM(R92C_TXAGC_RATE36, power[ 9]) |
	    SM(R92C_TXAGC_RATE48, power[10]) |
	    SM(R92C_TXAGC_RATE54, power[11]));
	/* Write per-MCS Tx power. */
	rtwn_bb_write(sc, R92C_TXAGC_MCS03_MCS00(chain),
	    SM(R92C_TXAGC_MCS00,  power[12]) |
	    SM(R92C_TXAGC_MCS01,  power[13]) |
	    SM(R92C_TXAGC_MCS02,  power[14]) |
	    SM(R92C_TXAGC_MCS03,  power[15]));
	rtwn_bb_write(sc, R92C_TXAGC_MCS07_MCS04(chain),
	    SM(R92C_TXAGC_MCS04,  power[16]) |
	    SM(R92C_TXAGC_MCS05,  power[17]) |
	    SM(R92C_TXAGC_MCS06,  power[18]) |
	    SM(R92C_TXAGC_MCS07,  power[19]));
	rtwn_bb_write(sc, R92C_TXAGC_MCS11_MCS08(chain),
	    SM(R92C_TXAGC_MCS08,  power[20]) |
	    SM(R92C_TXAGC_MCS09,  power[21]) |
	    SM(R92C_TXAGC_MCS10,  power[22]) |
	    SM(R92C_TXAGC_MCS11,  power[23]));
	rtwn_bb_write(sc, R92C_TXAGC_MCS15_MCS12(chain),
	    SM(R92C_TXAGC_MCS12,  power[24]) |
	    SM(R92C_TXAGC_MCS13,  power[25]) |
	    SM(R92C_TXAGC_MCS14,  power[26]) |
	    SM(R92C_TXAGC_MCS15,  power[27]));
}

void
rtwn_get_txpower(struct rtwn_softc *sc, int chain,
    struct ieee80211_channel *c, struct ieee80211_channel *extc,
    uint16_t power[RTWN_RIDX_COUNT])
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct r92c_rom *rom = &sc->rom;
	uint16_t cckpow, ofdmpow, htpow, diff, max;
	const struct rtwn_txpwr *base;
	int ridx, chan, group;

	/* Determine channel group. */
	chan = ieee80211_chan2ieee(ic, c);	/* XXX center freq! */
	if (chan <= 3)
		group = 0;
	else if (chan <= 9)
		group = 1;
	else
		group = 2;

	/* Get original Tx power based on board type and RF chain. */
	if (!(sc->chip & RTWN_CHIP_92C)) {
		if (sc->board_type == R92C_BOARD_TYPE_HIGHPA)
			base = &rtl8188ru_txagc[chain];
		else
			base = &rtl8192cu_txagc[chain];
	} else
		base = &rtl8192cu_txagc[chain];

	memset(power, 0, RTWN_RIDX_COUNT * sizeof(power[0]));
	if (sc->regulatory == 0) {
		for (ridx = 0; ridx <= 3; ridx++)
			power[ridx] = base->pwr[0][ridx];
	}
	for (ridx = 4; ridx < RTWN_RIDX_COUNT; ridx++) {
		if (sc->regulatory == 3) {
			power[ridx] = base->pwr[0][ridx];
			/* Apply vendor limits. */
			if (extc != NULL)
				max = rom->ht40_max_pwr[group];
			else
				max = rom->ht20_max_pwr[group];
			max = (max >> (chain * 4)) & 0xf;
			if (power[ridx] > max)
				power[ridx] = max;
		} else if (sc->regulatory == 1) {
			if (extc == NULL)
				power[ridx] = base->pwr[group][ridx];
		} else if (sc->regulatory != 2)
			power[ridx] = base->pwr[0][ridx];
	}

	/* Compute per-CCK rate Tx power. */
	cckpow = rom->cck_tx_pwr[chain][group];
	for (ridx = 0; ridx <= 3; ridx++) {
		power[ridx] += cckpow;
		if (power[ridx] > R92C_MAX_TX_PWR)
			power[ridx] = R92C_MAX_TX_PWR;
	}

	htpow = rom->ht40_1s_tx_pwr[chain][group];
	if (sc->ntxchains > 1) {
		/* Apply reduction for 2 spatial streams. */
		diff = rom->ht40_2s_tx_pwr_diff[group];
		diff = (diff >> (chain * 4)) & 0xf;
		htpow = (htpow > diff) ? htpow - diff : 0;
	}

	/* Compute per-OFDM rate Tx power. */
	diff = rom->ofdm_tx_pwr_diff[group];
	diff = (diff >> (chain * 4)) & 0xf;
	ofdmpow = htpow + diff;	/* HT->OFDM correction. */
	for (ridx = 4; ridx <= 11; ridx++) {
		power[ridx] += ofdmpow;
		if (power[ridx] > R92C_MAX_TX_PWR)
			power[ridx] = R92C_MAX_TX_PWR;
	}

	/* Compute per-MCS Tx power. */
	if (extc == NULL) {
		diff = rom->ht20_tx_pwr_diff[group];
		diff = (diff >> (chain * 4)) & 0xf;
		htpow += diff;	/* HT40->HT20 correction. */
	}
	for (ridx = 12; ridx <= 27; ridx++) {
		power[ridx] += htpow;
		if (power[ridx] > R92C_MAX_TX_PWR)
			power[ridx] = R92C_MAX_TX_PWR;
	}
#ifdef RTWN_DEBUG
	if (rtwn_debug >= 4) {
		/* Dump per-rate Tx power values. */
		printf("Tx power for chain %d:\n", chain);
		for (ridx = 0; ridx < RTWN_RIDX_COUNT; ridx++)
			printf("Rate %d = %u\n", ridx, power[ridx]);
	}
#endif
}

void
rtwn_set_txpower(struct rtwn_softc *sc, struct ieee80211_channel *c,
    struct ieee80211_channel *extc)
{
	uint16_t power[RTWN_RIDX_COUNT];
	int i;

	for (i = 0; i < sc->ntxchains; i++) {
		/* Compute per-rate Tx power values. */
		rtwn_get_txpower(sc, i, c, extc, power);
		/* Write per-rate Tx power values to hardware. */
		rtwn_write_txpower(sc, i, power);
	}
}

void
rtwn_set_chan(struct rtwn_softc *sc, struct ieee80211_channel *c,
    struct ieee80211_channel *extc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	u_int chan;
	int i;

	chan = ieee80211_chan2ieee(ic, c);	/* XXX center freq! */

	/* Set Tx power for this new channel. */
	rtwn_set_txpower(sc, c, extc);

	for (i = 0; i < sc->nrxchains; i++) {
		rtwn_rf_write(sc, i, R92C_RF_CHNLBW,
		    RW(sc->rf_chnlbw[i], R92C_RF_CHNLBW_CHNL, chan));
	}
#ifndef IEEE80211_NO_HT
	if (extc != NULL) {
		uint32_t reg;

		/* Is secondary channel below or above primary? */
		int prichlo = c->ic_freq < extc->ic_freq;

		rtwn_write_1(sc, R92C_BWOPMODE,
		    rtwn_read_1(sc, R92C_BWOPMODE) & ~R92C_BWOPMODE_20MHZ);

		reg = rtwn_read_1(sc, R92C_RRSR + 2);
		reg = (reg & ~0x6f) | (prichlo ? 1 : 2) << 5;
		rtwn_write_1(sc, R92C_RRSR + 2, reg);

		rtwn_bb_write(sc, R92C_FPGA0_RFMOD,
		    rtwn_bb_read(sc, R92C_FPGA0_RFMOD) | R92C_RFMOD_40MHZ);
		rtwn_bb_write(sc, R92C_FPGA1_RFMOD,
		    rtwn_bb_read(sc, R92C_FPGA1_RFMOD) | R92C_RFMOD_40MHZ);

		/* Set CCK side band. */
		reg = rtwn_bb_read(sc, R92C_CCK0_SYSTEM);
		reg = (reg & ~0x00000010) | (prichlo ? 0 : 1) << 4;
		rtwn_bb_write(sc, R92C_CCK0_SYSTEM, reg);

		reg = rtwn_bb_read(sc, R92C_OFDM1_LSTF);
		reg = (reg & ~0x00000c00) | (prichlo ? 1 : 2) << 10;
		rtwn_bb_write(sc, R92C_OFDM1_LSTF, reg);

		rtwn_bb_write(sc, R92C_FPGA0_ANAPARAM2,
		    rtwn_bb_read(sc, R92C_FPGA0_ANAPARAM2) &
		    ~R92C_FPGA0_ANAPARAM2_CBW20);

		reg = rtwn_bb_read(sc, 0x818);
		reg = (reg & ~0x0c000000) | (prichlo ? 2 : 1) << 26;
		rtwn_bb_write(sc, 0x818, reg);

		/* Select 40MHz bandwidth. */
		rtwn_rf_write(sc, 0, R92C_RF_CHNLBW,
		    (sc->rf_chnlbw[0] & ~0xfff) | chan);
	} else
#endif
	{
		rtwn_write_1(sc, R92C_BWOPMODE,
		    rtwn_read_1(sc, R92C_BWOPMODE) | R92C_BWOPMODE_20MHZ);

		rtwn_bb_write(sc, R92C_FPGA0_RFMOD,
		    rtwn_bb_read(sc, R92C_FPGA0_RFMOD) & ~R92C_RFMOD_40MHZ);
		rtwn_bb_write(sc, R92C_FPGA1_RFMOD,
		    rtwn_bb_read(sc, R92C_FPGA1_RFMOD) & ~R92C_RFMOD_40MHZ);

		rtwn_bb_write(sc, R92C_FPGA0_ANAPARAM2,
		    rtwn_bb_read(sc, R92C_FPGA0_ANAPARAM2) |
		    R92C_FPGA0_ANAPARAM2_CBW20);

		/* Select 20MHz bandwidth. */
		rtwn_rf_write(sc, 0, R92C_RF_CHNLBW,
		    (sc->rf_chnlbw[0] & ~0xfff) | R92C_RF_CHNLBW_BW20 | chan);
	}
}

int
rtwn_iq_calib_chain(struct rtwn_softc *sc, int chain, uint16_t tx[2],
    uint16_t rx[2])
{
	uint32_t status;
	int offset = chain * 0x20;

	if (chain == 0) {	/* IQ calibration for chain 0. */
		/* IQ calibration settings for chain 0. */
		rtwn_bb_write(sc, 0xe30, 0x10008c1f);
		rtwn_bb_write(sc, 0xe34, 0x10008c1f);
		rtwn_bb_write(sc, 0xe38, 0x82140102);

		if (sc->ntxchains > 1) {
			rtwn_bb_write(sc, 0xe3c, 0x28160202);	/* 2T */
			/* IQ calibration settings for chain 1. */
			rtwn_bb_write(sc, 0xe50, 0x10008c22);
			rtwn_bb_write(sc, 0xe54, 0x10008c22);
			rtwn_bb_write(sc, 0xe58, 0x82140102);
			rtwn_bb_write(sc, 0xe5c, 0x28160202);
		} else
			rtwn_bb_write(sc, 0xe3c, 0x28160502);	/* 1T */

		/* LO calibration settings. */
		rtwn_bb_write(sc, 0xe4c, 0x001028d1);
		/* We're doing LO and IQ calibration in one shot. */
		rtwn_bb_write(sc, 0xe48, 0xf9000000);
		rtwn_bb_write(sc, 0xe48, 0xf8000000);

	} else {		/* IQ calibration for chain 1. */
		/* We're doing LO and IQ calibration in one shot. */
		rtwn_bb_write(sc, 0xe60, 0x00000002);
		rtwn_bb_write(sc, 0xe60, 0x00000000);
	}

	/* Give LO and IQ calibrations the time to complete. */
	DELAY(1);

	/* Read IQ calibration status. */
	status = rtwn_bb_read(sc, 0xeac);

	if (status & (1 << (28 + chain * 3)))
		return (0);	/* Tx failed. */
	/* Read Tx IQ calibration results. */
	tx[0] = (rtwn_bb_read(sc, 0xe94 + offset) >> 16) & 0x3ff;
	tx[1] = (rtwn_bb_read(sc, 0xe9c + offset) >> 16) & 0x3ff;
	if (tx[0] == 0x142 || tx[1] == 0x042)
		return (0);	/* Tx failed. */

	if (status & (1 << (27 + chain * 3)))
		return (1);	/* Rx failed. */
	/* Read Rx IQ calibration results. */
	rx[0] = (rtwn_bb_read(sc, 0xea4 + offset) >> 16) & 0x3ff;
	rx[1] = (rtwn_bb_read(sc, 0xeac + offset) >> 16) & 0x3ff;
	if (rx[0] == 0x132 || rx[1] == 0x036)
		return (1);	/* Rx failed. */

	return (3);	/* Both Tx and Rx succeeded. */
}

void
rtwn_iq_calib(struct rtwn_softc *sc)
{
	/* TODO */
}

void
rtwn_lc_calib(struct rtwn_softc *sc)
{
	uint32_t rf_ac[2];
	uint8_t txmode;
	int i;

	txmode = rtwn_read_1(sc, R92C_OFDM1_LSTF + 3);
	if ((txmode & 0x70) != 0) {
		/* Disable all continuous Tx. */
		rtwn_write_1(sc, R92C_OFDM1_LSTF + 3, txmode & ~0x70);

		/* Set RF mode to standby mode. */
		for (i = 0; i < sc->nrxchains; i++) {
			rf_ac[i] = rtwn_rf_read(sc, i, R92C_RF_AC);
			rtwn_rf_write(sc, i, R92C_RF_AC,
			    RW(rf_ac[i], R92C_RF_AC_MODE,
				R92C_RF_AC_MODE_STANDBY));
		}
	} else {
		/* Block all Tx queues. */
		rtwn_write_1(sc, R92C_TXPAUSE, 0xff);
	}
	/* Start calibration. */
	rtwn_rf_write(sc, 0, R92C_RF_CHNLBW,
	    rtwn_rf_read(sc, 0, R92C_RF_CHNLBW) | R92C_RF_CHNLBW_LCSTART);

	/* Give calibration the time to complete. */
	DELAY(100);

	/* Restore configuration. */
	if ((txmode & 0x70) != 0) {
		/* Restore Tx mode. */
		rtwn_write_1(sc, R92C_OFDM1_LSTF + 3, txmode);
		/* Restore RF mode. */
		for (i = 0; i < sc->nrxchains; i++)
			rtwn_rf_write(sc, i, R92C_RF_AC, rf_ac[i]);
	} else {
		/* Unblock all Tx queues. */
		rtwn_write_1(sc, R92C_TXPAUSE, 0x00);
	}
}

void
rtwn_temp_calib(struct rtwn_softc *sc)
{
	int temp;

	if (sc->thcal_state == 0) {
		/* Start measuring temperature. */
		rtwn_rf_write(sc, 0, R92C_RF_T_METER, 0x60);
		sc->thcal_state = 1;
		return;
	}
	sc->thcal_state = 0;

	/* Read measured temperature. */
	temp = rtwn_rf_read(sc, 0, R92C_RF_T_METER) & 0x1f;
	if (temp == 0)	/* Read failed, skip. */
		return;
	DPRINTFN(2, ("temperature=%d\n", temp));

	/*
	 * Redo LC calibration if temperature changed significantly since
	 * last calibration.
	 */
	if (sc->thcal_lctemp == 0) {
		/* First LC calibration is performed in rtwn_init(). */
		sc->thcal_lctemp = temp;
	} else if (abs(temp - sc->thcal_lctemp) > 1) {
		DPRINTF(("LC calib triggered by temp: %d -> %d\n",
		    sc->thcal_lctemp, temp));
		rtwn_lc_calib(sc);
		/* Record temperature of last LC calibration. */
		sc->thcal_lctemp = temp;
	}
}

int
rtwn_init(struct ifnet *ifp)
{
	struct rtwn_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	uint32_t reg;
	int i, error;

	/* Init firmware commands ring. */
	sc->fwcur = 0;

	/* Power on adapter. */
	error = rtwn_power_on(sc);
	if (error != 0) {
		printf("%s: could not power on adapter\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	/* Initialize DMA. */
	error = rtwn_dma_init(sc);
	if (error != 0) {
		printf("%s: could not initialize DMA\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	/* Set info size in Rx descriptors (in 64-bit words). */
	rtwn_write_1(sc, R92C_RX_DRVINFO_SZ, 4);

	/* Disable interrupts. */
	rtwn_write_4(sc, R92C_HISR, 0x00000000);
	rtwn_write_4(sc, R92C_HIMR, 0x00000000);

	/* Set MAC address. */
	IEEE80211_ADDR_COPY(ic->ic_myaddr, LLADDR(ifp->if_sadl));
	for (i = 0; i < IEEE80211_ADDR_LEN; i++)
		rtwn_write_1(sc, R92C_MACID + i, ic->ic_myaddr[i]);

	/* Set initial network type. */
	reg = rtwn_read_4(sc, R92C_CR);
	reg = RW(reg, R92C_CR_NETTYPE, R92C_CR_NETTYPE_INFRA);
	rtwn_write_4(sc, R92C_CR, reg);

	rtwn_rxfilter_init(sc);

	reg = rtwn_read_4(sc, R92C_RRSR);
	reg = RW(reg, R92C_RRSR_RATE_BITMAP, R92C_RRSR_RATE_ALL);
	rtwn_write_4(sc, R92C_RRSR, reg);

	/* Set short/long retry limits. */
	rtwn_write_2(sc, R92C_RL,
	    SM(R92C_RL_SRL, 0x07) | SM(R92C_RL_LRL, 0x07));

	/* Initialize EDCA parameters. */
	rtwn_edca_init(sc);

	/* Set data and response automatic rate fallback retry counts. */
	rtwn_write_4(sc, R92C_DARFRC + 0, 0x01000000);
	rtwn_write_4(sc, R92C_DARFRC + 4, 0x07060504);
	rtwn_write_4(sc, R92C_RARFRC + 0, 0x01000000);
	rtwn_write_4(sc, R92C_RARFRC + 4, 0x07060504);

	rtwn_write_2(sc, R92C_FWHW_TXQ_CTRL, 0x1f80);

	/* Set ACK timeout. */
	rtwn_write_1(sc, R92C_ACKTO, 0x40);

	/* Initialize beacon parameters. */
	rtwn_write_2(sc, R92C_TBTT_PROHIBIT, 0x6404);
	rtwn_write_1(sc, R92C_DRVERLYINT, 0x05);
	rtwn_write_1(sc, R92C_BCNDMATIM, 0x02);
	rtwn_write_2(sc, R92C_BCNTCFG, 0x660f);

	/* Setup AMPDU aggregation. */
	rtwn_write_4(sc, R92C_AGGLEN_LMT, 0x99997631);	/* MCS7~0 */
	rtwn_write_1(sc, R92C_AGGR_BREAK_TIME, 0x16);

	rtwn_write_1(sc, R92C_BCN_MAX_ERR, 0xff);
	rtwn_write_1(sc, R92C_BCN_CTRL, R92C_BCN_CTRL_DIS_TSF_UDT0);

	rtwn_write_4(sc, R92C_PIFS, 0x1c);
	rtwn_write_4(sc, R92C_MCUTST_1, 0x0);

	/* Load 8051 microcode. */
	error = rtwn_load_firmware(sc);
	if (error != 0)
		goto fail;

	/* Initialize MAC/BB/RF blocks. */
	rtwn_mac_init(sc);
	rtwn_bb_init(sc);
	rtwn_rf_init(sc);

	/* Turn CCK and OFDM blocks on. */
	reg = rtwn_bb_read(sc, R92C_FPGA0_RFMOD);
	reg |= R92C_RFMOD_CCK_EN;
	rtwn_bb_write(sc, R92C_FPGA0_RFMOD, reg);
	reg = rtwn_bb_read(sc, R92C_FPGA0_RFMOD);
	reg |= R92C_RFMOD_OFDM_EN;
	rtwn_bb_write(sc, R92C_FPGA0_RFMOD, reg);

	/* Clear per-station keys table. */
	rtwn_cam_init(sc);

	/* Enable hardware sequence numbering. */
	rtwn_write_1(sc, R92C_HWSEQ_CTRL, 0xff);

	/* Perform LO and IQ calibrations. */
	rtwn_iq_calib(sc);
	/* Perform LC calibration. */
	rtwn_lc_calib(sc);

	rtwn_pa_bias_init(sc);

	/* Initialize GPIO setting. */
	rtwn_write_1(sc, R92C_GPIO_MUXCFG,
	    rtwn_read_1(sc, R92C_GPIO_MUXCFG) & ~R92C_GPIO_MUXCFG_ENBT);

	/* Fix for lower temperature. */
	rtwn_write_1(sc, 0x15, 0xe9);

	/* Set default channel. */
	ic->ic_bss->ni_chan = ic->ic_ibss_chan;
	rtwn_set_chan(sc, ic->ic_ibss_chan, NULL);

	/* CLear pending interrupts. */
	rtwn_write_4(sc, R92C_HISR, 0xffffffff);

	/* Enable interrupts. */
	rtwn_write_4(sc, R92C_HIMR, RTWN_INT_ENABLE);

	/* We're ready to go. */
	ifp->if_flags &= ~IFF_OACTIVE;
	ifp->if_flags |= IFF_RUNNING;

#ifdef notyet
	if (ic->ic_flags & IEEE80211_F_WEPON) {
		/* Install WEP keys. */
		for (i = 0; i < IEEE80211_WEP_NKID; i++)
			rtwn_set_key(ic, NULL, &ic->ic_nw_keys[i]);
	}
#endif
	if (ic->ic_opmode == IEEE80211_M_MONITOR)
		ieee80211_new_state(ic, IEEE80211_S_RUN, -1);
	else
		ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);
	return (0);
 fail:
	rtwn_stop(ifp);
	return (error);
}

void
rtwn_init_task(void *arg1)
{
	struct rtwn_softc *sc = arg1;
	struct ifnet *ifp = &sc->sc_ic.ic_if;
	int s;

	s = splnet();
	while (sc->sc_flags & RTWN_FLAG_BUSY)
		tsleep(&sc->sc_flags, 0, "rtwnpwr", 0);
	sc->sc_flags |= RTWN_FLAG_BUSY;

	rtwn_stop(ifp);

	if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) == IFF_UP)
		rtwn_init(ifp);

	sc->sc_flags &= ~RTWN_FLAG_BUSY;
	wakeup(&sc->sc_flags);
	splx(s);
}

void
rtwn_stop(struct ifnet *ifp)
{
	struct rtwn_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	int s, i;
	u_int16_t reg;

	sc->sc_tx_timer = 0;
	ifp->if_timer = 0;
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	s = splnet();
	ieee80211_new_state(ic, IEEE80211_S_INIT, -1);

	timeout_del(&sc->scan_to);
	timeout_del(&sc->calib_to);

	task_del(systq, &sc->init_task);

	/* Disable interrupts. */
	rtwn_write_4(sc, R92C_HISR, 0x00000000);
	rtwn_write_4(sc, R92C_HIMR, 0x00000000);

	/* Stop hardware. */
	rtwn_write_1(sc, R92C_TXPAUSE, 0xff);
	rtwn_write_1(sc, R92C_RF_CTRL, 0x00);
	reg = rtwn_read_1(sc, R92C_SYS_FUNC_EN);
	reg |= R92C_SYS_FUNC_EN_BB_GLB_RST;
	rtwn_write_1(sc, R92C_SYS_FUNC_EN, reg);
	reg &= ~R92C_SYS_FUNC_EN_BB_GLB_RST;
	rtwn_write_1(sc, R92C_SYS_FUNC_EN, reg);
	reg = rtwn_read_2(sc, R92C_CR);
	reg &= ~(R92C_CR_HCI_TXDMA_EN | R92C_CR_HCI_RXDMA_EN |
	    R92C_CR_TXDMA_EN | R92C_CR_RXDMA_EN | R92C_CR_PROTOCOL_EN |
	    R92C_CR_SCHEDULE_EN | R92C_CR_MACTXEN | R92C_CR_MACRXEN |
	    R92C_CR_ENSEC);
	rtwn_write_2(sc, R92C_CR, reg);
	if (rtwn_read_1(sc, R92C_MCUFWDL) & R92C_MCUFWDL_RAM_DL_SEL)
		rtwn_fw_reset(sc);
	/* TODO: linux does additional btcoex stuff here */
	rtwn_write_2(sc, R92C_AFE_PLL_CTRL, 0x80); /* linux magic number */
	rtwn_write_1(sc, R92C_SPS0_CTRL, 0x23); /* ditto */
	rtwn_write_1(sc, R92C_AFE_XTAL_CTRL, 0x0e); /* different with btcoex */
	rtwn_write_1(sc, R92C_RSV_CTRL, 0x0e);
	rtwn_write_1(sc, R92C_APS_FSMCO, R92C_APS_FSMCO_PDN_EN);

	for (i = 0; i < RTWN_NTXQUEUES; i++)
		rtwn_reset_tx_list(sc, i);
	rtwn_reset_rx_list(sc);

	splx(s);
}

int
rtwn_intr(void *xsc)
{
	struct rtwn_softc *sc = xsc;
	u_int32_t status;
	int i;

	status = rtwn_read_4(sc, R92C_HISR);
	if (status == 0 || status == 0xffffffff)
		return (0);

	/* Disable interrupts. */
	rtwn_write_4(sc, R92C_HIMR, 0x00000000);

	/* Ack interrupts. */
	rtwn_write_4(sc, R92C_HISR, status);

	/* Vendor driver treats RX errors like ROK... */
	if (status & (R92C_IMR_ROK | R92C_IMR_RXFOVW | R92C_IMR_RDU)) {
		bus_dmamap_sync(sc->sc_dmat, sc->rx_ring.map, 0,
		    sizeof(struct r92c_rx_desc) * RTWN_RX_LIST_COUNT,
		    BUS_DMASYNC_POSTREAD);

		for (i = 0; i < RTWN_RX_LIST_COUNT; i++) {
			struct r92c_rx_desc *rx_desc = &sc->rx_ring.desc[i];
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
	rtwn_write_4(sc, R92C_HIMR, RTWN_INT_ENABLE);

	return (1);
}
