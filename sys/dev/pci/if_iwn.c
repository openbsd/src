/*	$OpenBSD: if_iwn.c,v 1.7 2007/09/10 17:55:48 damien Exp $	*/

/*-
 * Copyright (c) 2007
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
 * Driver for Intel Wireless WiFi Link 4965AGN 802.11 network adapters.
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/sensors.h>

#include <machine/bus.h>
#include <machine/endian.h>
#include <machine/intr.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_amrr.h>
#include <net80211/ieee80211_radiotap.h>

#include <dev/pci/if_iwnreg.h>
#include <dev/pci/if_iwnvar.h>

static const struct pci_matchid iwn_devices[] = {
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_PRO_WL_4965AGN_1 },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_PRO_WL_4965AGN_2 }
};

int		iwn_match(struct device *, void *, void *);
void		iwn_attach(struct device *, struct device *, void *);
void		iwn_sensor_attach(struct iwn_softc *);
void		iwn_radiotap_attach(struct iwn_softc *);
void		iwn_power(int, void *);
int		iwn_dma_contig_alloc(bus_dma_tag_t, struct iwn_dma_info *,
		    void **, bus_size_t, bus_size_t, int);
void		iwn_dma_contig_free(struct iwn_dma_info *);
int		iwn_alloc_shared(struct iwn_softc *);
void		iwn_free_shared(struct iwn_softc *);
int		iwn_alloc_kw(struct iwn_softc *);
void		iwn_free_kw(struct iwn_softc *);
int		iwn_alloc_fwmem(struct iwn_softc *);
void		iwn_free_fwmem(struct iwn_softc *);
struct		iwn_rbuf *iwn_alloc_rbuf(struct iwn_softc *);
void		iwn_free_rbuf(caddr_t, u_int, void *);
int		iwn_alloc_rpool(struct iwn_softc *);
void		iwn_free_rpool(struct iwn_softc *);
int		iwn_alloc_rx_ring(struct iwn_softc *, struct iwn_rx_ring *);
void		iwn_reset_rx_ring(struct iwn_softc *, struct iwn_rx_ring *);
void		iwn_free_rx_ring(struct iwn_softc *, struct iwn_rx_ring *);
int		iwn_alloc_tx_ring(struct iwn_softc *, struct iwn_tx_ring *,
		    int, int);
void		iwn_reset_tx_ring(struct iwn_softc *, struct iwn_tx_ring *);
void		iwn_free_tx_ring(struct iwn_softc *, struct iwn_tx_ring *);
struct		ieee80211_node *iwn_node_alloc(struct ieee80211com *);
void		iwn_newassoc(struct ieee80211com *, struct ieee80211_node *,
		    int);
int		iwn_media_change(struct ifnet *);
int		iwn_newstate(struct ieee80211com *, enum ieee80211_state, int);
void		iwn_mem_lock(struct iwn_softc *);
void		iwn_mem_unlock(struct iwn_softc *);
uint32_t	iwn_mem_read(struct iwn_softc *, uint32_t);
void		iwn_mem_write(struct iwn_softc *, uint32_t, uint32_t);
void		iwn_mem_write_region_4(struct iwn_softc *, uint32_t,
		    const uint32_t *, int);
int		iwn_eeprom_lock(struct iwn_softc *);
void		iwn_eeprom_unlock(struct iwn_softc *);
int		iwn_read_prom_data(struct iwn_softc *, uint32_t, void *, int);
int		iwn_load_microcode(struct iwn_softc *, const uint8_t *, int);
int		iwn_load_firmware(struct iwn_softc *);
void		iwn_calib_timeout(void *);
void		iwn_iter_func(void *, struct ieee80211_node *);
void		iwn_ampdu_rx_start(struct iwn_softc *, struct iwn_rx_desc *);
void		iwn_rx_intr(struct iwn_softc *, struct iwn_rx_desc *,
		    struct iwn_rx_data *);
void		iwn_rx_statistics(struct iwn_softc *, struct iwn_rx_desc *);
void		iwn_tx_intr(struct iwn_softc *, struct iwn_rx_desc *);
void		iwn_cmd_intr(struct iwn_softc *, struct iwn_rx_desc *);
void		iwn_notif_intr(struct iwn_softc *);
int		iwn_intr(void *);
void		iwn_read_eeprom(struct iwn_softc *);
void		iwn_read_eeprom_channels(struct iwn_softc *, int);
void		iwn_print_power_group(struct iwn_softc *, int);
uint8_t		iwn_plcp_signal(int);
int		iwn_tx_data(struct iwn_softc *, struct mbuf *,
		    struct ieee80211_node *, int);
void		iwn_start(struct ifnet *);
void		iwn_watchdog(struct ifnet *);
int		iwn_ioctl(struct ifnet *, u_long, caddr_t);
int		iwn_cmd(struct iwn_softc *, int, const void *, int, int);
int		iwn_setup_node_mrr(struct iwn_softc *, uint8_t, int);
int		iwn_set_key(struct ieee80211com *, struct ieee80211_node *,
		    const struct ieee80211_key *);
void		iwn_edcaupdate(struct ieee80211com *);
void		iwn_set_led(struct iwn_softc *, uint8_t, uint8_t, uint8_t);
int		iwn_set_critical_temp(struct iwn_softc *);
void		iwn_enable_tsf(struct iwn_softc *, struct ieee80211_node *);
void		iwn_power_calibration(struct iwn_softc *, int);
int		iwn_set_txpower(struct iwn_softc *,
		    struct ieee80211_channel *, int);
int		iwn_get_rssi(const struct iwn_rx_stat *);
int		iwn_get_noise(const struct iwn_rx_general_stats *);
int		iwn_get_temperature(struct iwn_softc *);
int		iwn_init_sensitivity(struct iwn_softc *);
void		iwn_compute_differential_gain(struct iwn_softc *,
		    const struct iwn_rx_general_stats *);
void		iwn_tune_sensitivity(struct iwn_softc *,
		    const struct iwn_rx_stats *);
int		iwn_send_sensitivity(struct iwn_softc *);
int		iwn_auth(struct iwn_softc *);
int		iwn_run(struct iwn_softc *);
int		iwn_scan(struct iwn_softc *, uint16_t);
int		iwn_config(struct iwn_softc *);
void		iwn_post_alive(struct iwn_softc *);
void		iwn_stop_master(struct iwn_softc *);
int		iwn_reset(struct iwn_softc *);
void		iwn_hw_config(struct iwn_softc *);
int		iwn_init(struct ifnet *);
void		iwn_stop(struct ifnet *, int);

#define IWN_DEBUG

#ifdef IWN_DEBUG
#define DPRINTF(x)	do { if (iwn_debug > 0) printf x; } while (0)
#define DPRINTFN(n, x)	do { if (iwn_debug >= (n)) printf x; } while (0)
int iwn_debug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n, x)
#endif

struct cfattach iwn_ca = {
	sizeof (struct iwn_softc), iwn_match, iwn_attach
};

int
iwn_match(struct device *parent, void *match, void *aux)
{
	return pci_matchbyid((struct pci_attach_args *)aux, iwn_devices,
	    sizeof (iwn_devices) / sizeof (iwn_devices[0]));
}

/* Base Address Register */
#define IWN_PCI_BAR0	0x10

void
iwn_attach(struct device *parent, struct device *self, void *aux)
{
	struct iwn_softc *sc = (struct iwn_softc *)self;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct pci_attach_args *pa = aux;
	const char *intrstr;
	pci_intr_handle_t ih;
	pcireg_t memtype, data;
	int i, error;

	sc->sc_pct = pa->pa_pc;
	sc->sc_pcitag = pa->pa_tag;
	sc->sc_dmat = pa->pa_dmat;

	/* clear device specific PCI configuration register 0x41 */
	data = pci_conf_read(sc->sc_pct, sc->sc_pcitag, 0x40);
	data &= ~0x0000ff00;
	pci_conf_write(sc->sc_pct, sc->sc_pcitag, 0x40, data);

	memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, IWN_PCI_BAR0);
	error = pci_mapreg_map(pa, IWN_PCI_BAR0, memtype, 0, &sc->sc_st,
	    &sc->sc_sh, NULL, &sc->sc_sz, 0);
	if (error != 0) {
		printf(": could not map memory space\n");
		return;
	}

	if (pci_intr_map(pa, &ih) != 0) {
		printf(": could not map interrupt\n");
		return;
	}

	intrstr = pci_intr_string(sc->sc_pct, ih);
	sc->sc_ih = pci_intr_establish(sc->sc_pct, ih, IPL_NET, iwn_intr, sc,
	    sc->sc_dev.dv_xname);
	if (sc->sc_ih == NULL) {
		printf(": could not establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}
	printf(": %s", intrstr);

	/*
	 * Put adapter into a known state.
	 */
	if ((error = iwn_reset(sc)) != 0) {
		printf(": could not reset adapter\n");
		return;
	}

	/*
	 * Allocate DMA memory for firmware transfers.
	 */
	if ((error = iwn_alloc_fwmem(sc)) != 0) {
		printf(": could not allocate firmware memory\n");
		return;
	}

	/*
	 * Allocate a "keep warm" page.
	 */
	if ((error = iwn_alloc_kw(sc)) != 0) {
		printf(": could not allocate keep warm page\n");
		goto fail1;
	}

	/*
	 * Allocate shared area (communication area).
	 */
	if ((error = iwn_alloc_shared(sc)) != 0) {
		printf(": could not allocate shared area\n");
		goto fail2;
	}

	/*
	 * Allocate Rx buffers and Tx/Rx rings.
	 */
	if ((error = iwn_alloc_rpool(sc)) != 0) {
		printf(": could not allocate Rx buffers\n");
		goto fail3;
	}

	for (i = 0; i < IWN_NTXQUEUES; i++) {
		struct iwn_tx_ring *txq = &sc->txq[i];
		error = iwn_alloc_tx_ring(sc, txq, IWN_TX_RING_COUNT, i);
		if (error != 0) {
			printf(": could not allocate Tx ring %d\n", i);
			goto fail4;
		}
	}

	error = iwn_alloc_rx_ring(sc, &sc->rxq);
	if (error != 0) {
		printf(": could not allocate Rx ring\n");
		goto fail4;
	}

	ic->ic_phytype = IEEE80211_T_OFDM;	/* not only, but not used */
	ic->ic_opmode = IEEE80211_M_STA;	/* default to BSS mode */
	ic->ic_state = IEEE80211_S_INIT;

	/* set device capabilities */
	ic->ic_caps =
	    IEEE80211_C_WEP |		/* s/w WEP */
	    IEEE80211_C_MONITOR |	/* monitor mode supported */
	    IEEE80211_C_TXPMGT |	/* tx power management */
	    IEEE80211_C_SHSLOT |	/* short slot time supported */
	    IEEE80211_C_SHPREAMBLE;	/* short preamble supported */

	/* read supported channels and MAC address from EEPROM */
	iwn_read_eeprom(sc);

	/* set supported .11a, .11b and .11g rates */
	ic->ic_sup_rates[IEEE80211_MODE_11A] = ieee80211_std_rateset_11a;
	ic->ic_sup_rates[IEEE80211_MODE_11B] = ieee80211_std_rateset_11b;
	ic->ic_sup_rates[IEEE80211_MODE_11G] = ieee80211_std_rateset_11g;

	/* IBSS channel undefined for now */
	ic->ic_ibss_chan = &ic->ic_channels[0];

	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_init = iwn_init;
	ifp->if_ioctl = iwn_ioctl;
	ifp->if_start = iwn_start;
	ifp->if_watchdog = iwn_watchdog;
	IFQ_SET_READY(&ifp->if_snd);
	memcpy(ifp->if_xname, sc->sc_dev.dv_xname, IFNAMSIZ);

	if_attach(ifp);
	ieee80211_ifattach(ifp);
	ic->ic_node_alloc = iwn_node_alloc;
	ic->ic_newassoc = iwn_newassoc;
	ic->ic_set_key = iwn_set_key;

	/* override state transition machine */
	sc->sc_newstate = ic->ic_newstate;
	ic->ic_newstate = iwn_newstate;
	ieee80211_media_init(ifp, iwn_media_change, ieee80211_media_status);

	sc->amrr.amrr_min_success_threshold =  1;
	sc->amrr.amrr_max_success_threshold = 15;

	iwn_sensor_attach(sc);
	iwn_radiotap_attach(sc);

	timeout_set(&sc->calib_to, iwn_calib_timeout, sc);

	sc->powerhook = powerhook_establish(iwn_power, sc);

	return;

	/* free allocated memory if something failed during attachment */
fail4:	while (--i >= 0)
		iwn_free_tx_ring(sc, &sc->txq[i]);
	iwn_free_rpool(sc);
fail3:	iwn_free_shared(sc);
fail2:	iwn_free_kw(sc);
fail1:	iwn_free_fwmem(sc);
}

/*
 * Attach the adapter's on-board thermal sensor to the sensors framework.
 */
void
iwn_sensor_attach(struct iwn_softc *sc)
{
	strlcpy(sc->sensordev.xname, sc->sc_dev.dv_xname,
	    sizeof sc->sensordev.xname);
	sc->sensor.type = SENSOR_TEMP;
	/* temperature invalid until interface is up */
	sc->sensor.value = 0;
	sc->sensor.flags = SENSOR_FINVALID;
	sensor_attach(&sc->sensordev, &sc->sensor);
	sensordev_install(&sc->sensordev);
}

/*
 * Attach the interface to 802.11 radiotap.
 */
void
iwn_radiotap_attach(struct iwn_softc *sc)
{
#if NBPFILTER > 0
	bpfattach(&sc->sc_drvbpf, &sc->sc_ic.ic_if, DLT_IEEE802_11_RADIO,
	    sizeof (struct ieee80211_frame) + IEEE80211_RADIOTAP_HDRLEN);

	sc->sc_rxtap_len = sizeof sc->sc_rxtapu;
	sc->sc_rxtap.wr_ihdr.it_len = htole16(sc->sc_rxtap_len);
	sc->sc_rxtap.wr_ihdr.it_present = htole32(IWN_RX_RADIOTAP_PRESENT);

	sc->sc_txtap_len = sizeof sc->sc_txtapu;
	sc->sc_txtap.wt_ihdr.it_len = htole16(sc->sc_txtap_len);
	sc->sc_txtap.wt_ihdr.it_present = htole32(IWN_TX_RADIOTAP_PRESENT);
#endif
}

void
iwn_power(int why, void *arg)
{
	struct iwn_softc *sc = arg;
	struct ifnet *ifp;
	pcireg_t data;
	int s;

	if (why != PWR_RESUME)
		return;

	/* clear device specific PCI configuration register 0x41 */
	data = pci_conf_read(sc->sc_pct, sc->sc_pcitag, 0x40);
	data &= ~0x0000ff00;
	pci_conf_write(sc->sc_pct, sc->sc_pcitag, 0x40, data);

	s = splnet();
	ifp = &sc->sc_ic.ic_if;
	if (ifp->if_flags & IFF_UP) {
		ifp->if_init(ifp);
		if (ifp->if_flags & IFF_RUNNING)
			ifp->if_start(ifp);
	}
	splx(s);
}

int
iwn_dma_contig_alloc(bus_dma_tag_t tag, struct iwn_dma_info *dma, void **kvap,
    bus_size_t size, bus_size_t alignment, int flags)
{
	int nsegs, error;

	dma->tag = tag;
	dma->size = size;

	error = bus_dmamap_create(tag, size, 1, size, 0, flags, &dma->map);
	if (error != 0)
		goto fail;

	error = bus_dmamem_alloc(tag, size, alignment, 0, &dma->seg, 1, &nsegs,
	    flags);
	if (error != 0)
		goto fail;

	error = bus_dmamem_map(tag, &dma->seg, 1, size, &dma->vaddr, flags);
	if (error != 0)
		goto fail;

	error = bus_dmamap_load_raw(tag, dma->map, &dma->seg, 1, size, flags);
	if (error != 0)
		goto fail;

	memset(dma->vaddr, 0, size);

	dma->paddr = dma->map->dm_segs[0].ds_addr;
	if (kvap != NULL)
		*kvap = dma->vaddr;

	return 0;

fail:	iwn_dma_contig_free(dma);
	return error;
}

void
iwn_dma_contig_free(struct iwn_dma_info *dma)
{
	if (dma->map != NULL) {
		if (dma->vaddr != NULL) {
			bus_dmamap_unload(dma->tag, dma->map);
			bus_dmamem_unmap(dma->tag, dma->vaddr, dma->size);
			bus_dmamem_free(dma->tag, &dma->seg, 1);
			dma->vaddr = NULL;
		}
		bus_dmamap_destroy(dma->tag, dma->map);
		dma->map = NULL;
	}
}

int
iwn_alloc_shared(struct iwn_softc *sc)
{
	/* must be aligned on a 1KB boundary */
	return iwn_dma_contig_alloc(sc->sc_dmat, &sc->shared_dma,
	    (void **)&sc->shared, sizeof (struct iwn_shared), 1024,
	    BUS_DMA_NOWAIT);
}

void
iwn_free_shared(struct iwn_softc *sc)
{
	iwn_dma_contig_free(&sc->shared_dma);
}

int
iwn_alloc_kw(struct iwn_softc *sc)
{
	/* must be aligned on a 16-byte boundary */
	return iwn_dma_contig_alloc(sc->sc_dmat, &sc->kw_dma, NULL,
	    PAGE_SIZE, 16, BUS_DMA_NOWAIT);
}

void
iwn_free_kw(struct iwn_softc *sc)
{
	iwn_dma_contig_free(&sc->kw_dma);
}

int
iwn_alloc_fwmem(struct iwn_softc *sc)
{
	/* allocate enough contiguous space to store text and data */
	return iwn_dma_contig_alloc(sc->sc_dmat, &sc->fw_dma, NULL,
	    IWN_FW_MAIN_TEXT_MAXSZ + IWN_FW_MAIN_DATA_MAXSZ, 16,
	    BUS_DMA_NOWAIT);
}

void
iwn_free_fwmem(struct iwn_softc *sc)
{
	iwn_dma_contig_free(&sc->fw_dma);
}

struct iwn_rbuf *
iwn_alloc_rbuf(struct iwn_softc *sc)
{
	struct iwn_rbuf *rbuf;

	rbuf = SLIST_FIRST(&sc->rxq.freelist);
	if (rbuf == NULL)
		return NULL;
	SLIST_REMOVE_HEAD(&sc->rxq.freelist, next);
	return rbuf;
}

/*
 * This is called automatically by the network stack when the mbuf to which
 * our Rx buffer is attached is freed.
 */
void
iwn_free_rbuf(caddr_t buf, u_int size, void *arg)
{
	struct iwn_rbuf *rbuf = arg;
	struct iwn_softc *sc = rbuf->sc;

	/* put the buffer back in the free list */
	SLIST_INSERT_HEAD(&sc->rxq.freelist, rbuf, next);
}

int
iwn_alloc_rpool(struct iwn_softc *sc)
{
	struct iwn_rx_ring *ring = &sc->rxq;
	int i, error;

	/* allocate a big chunk of DMA'able memory.. */
	error = iwn_dma_contig_alloc(sc->sc_dmat, &ring->buf_dma, NULL,
	    IWN_RBUF_COUNT * IWN_RBUF_SIZE, PAGE_SIZE, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not allocate Rx buffers DMA memory\n",
		    sc->sc_dev.dv_xname);
		return error;
	}

	/* ..and split it into chunks of "rbufsz" bytes */
	SLIST_INIT(&ring->freelist);
	for (i = 0; i < IWN_RBUF_COUNT; i++) {
		struct iwn_rbuf *rbuf = &ring->rbuf[i];

		rbuf->sc = sc;	/* backpointer for callbacks */
		rbuf->vaddr = ring->buf_dma.vaddr + i * IWN_RBUF_SIZE;
		rbuf->paddr = ring->buf_dma.paddr + i * IWN_RBUF_SIZE;

		SLIST_INSERT_HEAD(&ring->freelist, rbuf, next);
	}
	return 0;
}

void
iwn_free_rpool(struct iwn_softc *sc)
{
	iwn_dma_contig_free(&sc->rxq.buf_dma);
}

int
iwn_alloc_rx_ring(struct iwn_softc *sc, struct iwn_rx_ring *ring)
{
	int i, error;

	ring->cur = 0;

	error = iwn_dma_contig_alloc(sc->sc_dmat, &ring->desc_dma,
	    (void **)&ring->desc, IWN_RX_RING_COUNT * sizeof (uint32_t),
	    IWN_RING_DMA_ALIGN, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not allocate rx ring DMA memory\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	/*
	 * Setup Rx buffers.
	 */
	for (i = 0; i < IWN_RX_RING_COUNT; i++) {
		struct iwn_rx_data *data = &ring->data[i];
		struct iwn_rbuf *rbuf;

		MGETHDR(data->m, M_DONTWAIT, MT_DATA);
		if (data->m == NULL) {
			printf("%s: could not allocate rx mbuf\n",
			    sc->sc_dev.dv_xname);
			error = ENOMEM;
			goto fail;
		}
		if ((rbuf = iwn_alloc_rbuf(sc)) == NULL) {
			m_freem(data->m);
			data->m = NULL;
			printf("%s: could not allocate rx buffer\n",
			    sc->sc_dev.dv_xname);
			error = ENOMEM;
			goto fail;
		}
		/* attach Rx buffer to mbuf */
		MEXTADD(data->m, rbuf->vaddr, IWN_RBUF_SIZE, 0, iwn_free_rbuf,
		    rbuf);

		/* Rx buffers are aligned on a 256-byte boundary */
		ring->desc[i] = htole32(rbuf->paddr >> 8);
	}

	return 0;

fail:	iwn_free_rx_ring(sc, ring);
	return error;
}

void
iwn_reset_rx_ring(struct iwn_softc *sc, struct iwn_rx_ring *ring)
{
	int ntries;

	iwn_mem_lock(sc);

	IWN_WRITE(sc, IWN_RX_CONFIG, 0);
	for (ntries = 0; ntries < 100; ntries++) {
		if (IWN_READ(sc, IWN_RX_STATUS) & IWN_RX_IDLE)
			break;
		DELAY(10);
	}
#ifdef IWN_DEBUG
	if (ntries == 100 && iwn_debug > 0)
		printf("%s: timeout resetting Rx ring\n", sc->sc_dev.dv_xname);
#endif
	iwn_mem_unlock(sc);

	ring->cur = 0;
}

void
iwn_free_rx_ring(struct iwn_softc *sc, struct iwn_rx_ring *ring)
{
	int i;

	iwn_dma_contig_free(&ring->desc_dma);

	for (i = 0; i < IWN_RX_RING_COUNT; i++) {
		if (ring->data[i].m != NULL)
			m_freem(ring->data[i].m);
	}
}

int
iwn_alloc_tx_ring(struct iwn_softc *sc, struct iwn_tx_ring *ring, int count,
    int qid)
{
	int i, error;

	ring->qid = qid;
	ring->count = count;
	ring->queued = 0;
	ring->cur = 0;

	error = iwn_dma_contig_alloc(sc->sc_dmat, &ring->desc_dma,
	    (void **)&ring->desc, count * sizeof (struct iwn_tx_desc),
	    IWN_RING_DMA_ALIGN, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not allocate tx ring DMA memory\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	error = iwn_dma_contig_alloc(sc->sc_dmat, &ring->cmd_dma,
	    (void **)&ring->cmd, count * sizeof (struct iwn_tx_cmd), 4,
	    BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not allocate tx cmd DMA memory\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	ring->data = malloc(count * sizeof (struct iwn_tx_data), M_DEVBUF,
	    M_NOWAIT | M_ZERO);
	if (ring->data == NULL) {
		printf("%s: could not allocate tx data slots\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	for (i = 0; i < count; i++) {
		struct iwn_tx_data *data = &ring->data[i];

		error = bus_dmamap_create(sc->sc_dmat, MCLBYTES,
		    IWN_MAX_SCATTER - 1, MCLBYTES, 0, BUS_DMA_NOWAIT,
		    &data->map);
		if (error != 0) {
			printf("%s: could not create tx buf DMA map\n",
			    sc->sc_dev.dv_xname);
			goto fail;
		}
	}

	return 0;

fail:	iwn_free_tx_ring(sc, ring);
	return error;
}

void
iwn_reset_tx_ring(struct iwn_softc *sc, struct iwn_tx_ring *ring)
{
	uint32_t tmp;
	int i, ntries;

	iwn_mem_lock(sc);

	IWN_WRITE(sc, IWN_TX_CONFIG(ring->qid), 0);
	for (ntries = 0; ntries < 100; ntries++) {
		tmp = IWN_READ(sc, IWN_TX_STATUS);
		if ((tmp & IWN_TX_IDLE(ring->qid)) == IWN_TX_IDLE(ring->qid))
			break;
		DELAY(10);
	}
#ifdef IWN_DEBUG
	if (ntries == 100 && iwn_debug > 1) {
		printf("%s: timeout resetting Tx ring %d\n",
		    sc->sc_dev.dv_xname, ring->qid);
	}
#endif
	iwn_mem_unlock(sc);

	for (i = 0; i < ring->count; i++) {
		struct iwn_tx_data *data = &ring->data[i];

		if (data->m != NULL) {
			bus_dmamap_unload(sc->sc_dmat, data->map);
			m_freem(data->m);
			data->m = NULL;
		}
	}

	ring->queued = 0;
	ring->cur = 0;
}

void
iwn_free_tx_ring(struct iwn_softc *sc, struct iwn_tx_ring *ring)
{
	int i;

	iwn_dma_contig_free(&ring->desc_dma);
	iwn_dma_contig_free(&ring->cmd_dma);

	if (ring->data != NULL) {
		for (i = 0; i < ring->count; i++) {
			struct iwn_tx_data *data = &ring->data[i];

			if (data->m != NULL) {
				bus_dmamap_unload(sc->sc_dmat, data->map);
				m_freem(data->m);
			}
		}
		free(ring->data, M_DEVBUF);
	}
}

struct ieee80211_node *
iwn_node_alloc(struct ieee80211com *ic)
{
	return malloc(sizeof (struct iwn_node), M_DEVBUF, M_NOWAIT | M_ZERO);
}

void
iwn_newassoc(struct ieee80211com *ic, struct ieee80211_node *ni, int isnew)
{
	struct iwn_softc *sc = ic->ic_if.if_softc;
	int i;

	ieee80211_amrr_node_init(&sc->amrr, &((struct iwn_node *)ni)->amn);

	/* set rate to some reasonable initial value */
	for (i = ni->ni_rates.rs_nrates - 1;
	     i > 0 && (ni->ni_rates.rs_rates[i] & IEEE80211_RATE_VAL) > 72;
	     i--);
	ni->ni_txrate = i;
}

int
iwn_media_change(struct ifnet *ifp)
{
	int error;

	error = ieee80211_media_change(ifp);
	if (error != ENETRESET)
		return error;

	if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) == (IFF_UP | IFF_RUNNING))
		iwn_init(ifp);

	return 0;
}

int
iwn_newstate(struct ieee80211com *ic, enum ieee80211_state nstate, int arg)
{
	struct ifnet *ifp = &ic->ic_if;
	struct iwn_softc *sc = ifp->if_softc;
	int error;

	timeout_del(&sc->calib_to);

	if (ic->ic_state == IEEE80211_S_SCAN)
		ic->ic_scan_lock = IEEE80211_SCAN_UNLOCKED;

	switch (nstate) {
	case IEEE80211_S_SCAN:
		/* make the link LED blink while we're scanning */
		iwn_set_led(sc, IWN_LED_LINK, 20, 2);

		if ((error = iwn_scan(sc, IEEE80211_CHAN_G)) != 0) {
			printf("%s: could not initiate scan\n",
			    sc->sc_dev.dv_xname);
			return error;
		}
		ic->ic_state = nstate;
		return 0;

	case IEEE80211_S_ASSOC:
		if (ic->ic_state != IEEE80211_S_RUN)
			break;
		/* FALLTHROUGH */
	case IEEE80211_S_AUTH:
		/* reset state to handle reassociations correctly */
		sc->config.associd = 0;
		sc->config.filter &= ~htole32(IWN_FILTER_BSS);
		sc->calib.state = IWN_CALIB_STATE_INIT;

		if ((error = iwn_auth(sc)) != 0) {
			printf("%s: could not move to auth state\n",
			    sc->sc_dev.dv_xname);
			return error;
		}
		break;

	case IEEE80211_S_RUN:
		if ((error = iwn_run(sc)) != 0) {
			printf("%s: could not move to run state\n",
			    sc->sc_dev.dv_xname);
			return error;
		}
		break;

	case IEEE80211_S_INIT:
		sc->calib.state = IWN_CALIB_STATE_INIT;
		break;
	}

	return sc->sc_newstate(ic, nstate, arg);
}

/*
 * Grab exclusive access to NIC memory.
 */
void
iwn_mem_lock(struct iwn_softc *sc)
{
	uint32_t tmp;
	int ntries;

	tmp = IWN_READ(sc, IWN_GPIO_CTL);
	IWN_WRITE(sc, IWN_GPIO_CTL, tmp | IWN_GPIO_MAC);

	/* spin until we actually get the lock */
	for (ntries = 0; ntries < 1000; ntries++) {
		if ((IWN_READ(sc, IWN_GPIO_CTL) &
		    (IWN_GPIO_CLOCK | IWN_GPIO_SLEEP)) == IWN_GPIO_CLOCK)
			break;
		DELAY(10);
	}
	if (ntries == 1000)
		printf("%s: could not lock memory\n", sc->sc_dev.dv_xname);
}

/*
 * Release lock on NIC memory.
 */
void
iwn_mem_unlock(struct iwn_softc *sc)
{
	uint32_t tmp = IWN_READ(sc, IWN_GPIO_CTL);
	IWN_WRITE(sc, IWN_GPIO_CTL, tmp & ~IWN_GPIO_MAC);
}

uint32_t
iwn_mem_read(struct iwn_softc *sc, uint32_t addr)
{
	IWN_WRITE(sc, IWN_READ_MEM_ADDR, IWN_MEM_4 | addr);
	return IWN_READ(sc, IWN_READ_MEM_DATA);
}

void
iwn_mem_write(struct iwn_softc *sc, uint32_t addr, uint32_t data)
{
	IWN_WRITE(sc, IWN_WRITE_MEM_ADDR, IWN_MEM_4 | addr);
	IWN_WRITE(sc, IWN_WRITE_MEM_DATA, data);
}

void
iwn_mem_write_region_4(struct iwn_softc *sc, uint32_t addr,
    const uint32_t *data, int wlen)
{
	for (; wlen > 0; wlen--, data++, addr += 4)
		iwn_mem_write(sc, addr, *data);
}

int
iwn_eeprom_lock(struct iwn_softc *sc)
{
	uint32_t tmp;
	int ntries;

	tmp = IWN_READ(sc, IWN_HWCONFIG);
	IWN_WRITE(sc, IWN_HWCONFIG, tmp | IWN_HW_EEPROM_LOCKED);

	/* spin until we actually get the lock */
	for (ntries = 0; ntries < 100; ntries++) {
		if (IWN_READ(sc, IWN_HWCONFIG) & IWN_HW_EEPROM_LOCKED)
			return 0;
		DELAY(10);
	}
	return ETIMEDOUT;
}

void
iwn_eeprom_unlock(struct iwn_softc *sc)
{
	uint32_t tmp = IWN_READ(sc, IWN_HWCONFIG);
	IWN_WRITE(sc, IWN_HWCONFIG, tmp & ~IWN_HW_EEPROM_LOCKED);
}

/*
 * Read `len' bytes from the EEPROM.  We access the EEPROM through the MAC
 * instead of using the traditional bit-bang method.
 */
int
iwn_read_prom_data(struct iwn_softc *sc, uint32_t addr, void *data, int len)
{
	uint8_t *out = data;
	uint32_t val;
	int ntries;

	iwn_mem_lock(sc);
	for (; len > 0; len -= 2, addr++) {
		IWN_WRITE(sc, IWN_EEPROM_CTL, addr << 2);

		for (ntries = 0; ntries < 10; ntries++) {
			if ((val = IWN_READ(sc, IWN_EEPROM_CTL)) &
			    IWN_EEPROM_READY)
				break;
			DELAY(5);
		}
		if (ntries == 10) {
			printf("%s: could not read EEPROM\n",
			    sc->sc_dev.dv_xname);
			return ETIMEDOUT;
		}
		*out++ = val >> 16;
		if (len > 1)
			*out++ = val >> 24;
	}
	iwn_mem_unlock(sc);

	return 0;
}

/*
 * The firmware boot code is small and is intended to be copied directly into
 * the NIC internal memory.
 */
int
iwn_load_microcode(struct iwn_softc *sc, const uint8_t *ucode, int size)
{
	int ntries;

	size /= sizeof (uint32_t);

	iwn_mem_lock(sc);

	/* copy microcode image into NIC memory */
	iwn_mem_write_region_4(sc, IWN_MEM_UCODE_BASE,
	    (const uint32_t *)ucode, size);

	iwn_mem_write(sc, IWN_MEM_UCODE_SRC, 0);
	iwn_mem_write(sc, IWN_MEM_UCODE_DST, IWN_FW_TEXT);
	iwn_mem_write(sc, IWN_MEM_UCODE_SIZE, size);

	/* run microcode */
	iwn_mem_write(sc, IWN_MEM_UCODE_CTL, IWN_UC_RUN);

	/* wait for transfer to complete */
	for (ntries = 0; ntries < 1000; ntries++) {
		if (!(iwn_mem_read(sc, IWN_MEM_UCODE_CTL) & IWN_UC_RUN))
			break;
		DELAY(10);
	}
	if (ntries == 1000) {
		iwn_mem_unlock(sc);
		printf("%s: could not load boot firmware\n",
		    sc->sc_dev.dv_xname);
		return ETIMEDOUT;
	}
	iwn_mem_write(sc, IWN_MEM_UCODE_CTL, IWN_UC_ENABLE);

	iwn_mem_unlock(sc);

	return 0;
}

int
iwn_load_firmware(struct iwn_softc *sc)
{
	struct iwn_dma_info *dma = &sc->fw_dma;
	const struct iwn_firmware_hdr *hdr;
	const uint8_t *init_text, *init_data, *main_text, *main_data;
	const uint8_t *boot_text;
	uint32_t init_textsz, init_datasz, main_textsz, main_datasz;
	uint32_t boot_textsz;
	u_char *fw;
	size_t size;
	int error;

	/* load firmware image from disk */
	if ((error = loadfirmware("iwn-4965agn", &fw, &size)) != 0) {
		printf("%s: error, %d, could not read firmware %s\n",
		    sc->sc_dev.dv_xname, error, "iwn-4965agn");
		goto fail1;
	}

	/* extract firmware header information */
	if (size < sizeof (struct iwn_firmware_hdr)) {
		printf("%s: truncated firmware header: %d bytes\n",
		    sc->sc_dev.dv_xname, size);
		error = EINVAL;
		goto fail2;
	}
	hdr = (const struct iwn_firmware_hdr *)fw;
	main_textsz = letoh32(hdr->main_textsz);
	main_datasz = letoh32(hdr->main_datasz);
	init_textsz = letoh32(hdr->init_textsz);
	init_datasz = letoh32(hdr->init_datasz);
	boot_textsz = letoh32(hdr->boot_textsz);

	/* sanity-check firmware segments sizes */
	if (main_textsz > IWN_FW_MAIN_TEXT_MAXSZ ||
	    main_datasz > IWN_FW_MAIN_DATA_MAXSZ ||
	    init_textsz > IWN_FW_INIT_TEXT_MAXSZ ||
	    init_datasz > IWN_FW_INIT_DATA_MAXSZ ||
	    boot_textsz > IWN_FW_BOOT_TEXT_MAXSZ ||
	    (boot_textsz & 3) != 0) {
		printf("%s: invalid firmware header\n", sc->sc_dev.dv_xname);
		error = EINVAL;
		goto fail2;
	}

	/* check that all firmware segments are present */
	if (size < sizeof (struct iwn_firmware_hdr) + main_textsz +
	    main_datasz + init_textsz + init_datasz + boot_textsz) {
		printf("%s: firmware file too short: %d bytes\n",
		    sc->sc_dev.dv_xname, size);
		error = EINVAL;
		goto fail2;
	}

	/* get pointers to firmware segments */
	main_text = (const uint8_t *)(hdr + 1);
	main_data = main_text + main_textsz;
	init_text = main_data + main_datasz;
	init_data = init_text + init_textsz;
	boot_text = init_data + init_datasz;

	/* copy initialization images into pre-allocated DMA-safe memory */
	memcpy(dma->vaddr, init_data, init_datasz);
	memcpy(dma->vaddr + IWN_FW_INIT_DATA_MAXSZ, init_text, init_textsz);

	/* tell adapter where to find initialization images */
	iwn_mem_lock(sc);
	iwn_mem_write(sc, IWN_MEM_DATA_BASE, dma->paddr >> 4);
	iwn_mem_write(sc, IWN_MEM_DATA_SIZE, init_datasz);
	iwn_mem_write(sc, IWN_MEM_TEXT_BASE,
	    (dma->paddr + IWN_FW_INIT_DATA_MAXSZ) >> 4);
	iwn_mem_write(sc, IWN_MEM_TEXT_SIZE, init_textsz);
	iwn_mem_unlock(sc);

	/* load firmware boot code */
	if ((error = iwn_load_microcode(sc, boot_text, boot_textsz)) != 0) {
		printf("%s: could not load boot firmware\n",
		    sc->sc_dev.dv_xname);
		goto fail2;
	}

	/* now press "execute" ;-) */
	IWN_WRITE(sc, IWN_RESET, 0);

	/* wait at most one second for first alive notification */
	if ((error = tsleep(sc, PCATCH, "iwninit", hz)) != 0) {
		/* this isn't what was supposed to happen.. */
		printf("%s: timeout waiting for adapter to initialize\n",
		    sc->sc_dev.dv_xname);
		goto fail2;
	}

	/* copy runtime images into pre-allocated DMA-safe memory */
	memcpy(dma->vaddr, main_data, main_datasz);
	memcpy(dma->vaddr + IWN_FW_MAIN_DATA_MAXSZ, main_text, main_textsz);

	/* tell adapter where to find runtime images */
	iwn_mem_lock(sc);
	iwn_mem_write(sc, IWN_MEM_DATA_BASE, dma->paddr >> 4);
	iwn_mem_write(sc, IWN_MEM_DATA_SIZE, main_datasz);
	iwn_mem_write(sc, IWN_MEM_TEXT_BASE,
	    (dma->paddr + IWN_FW_MAIN_DATA_MAXSZ) >> 4);
	iwn_mem_write(sc, IWN_MEM_TEXT_SIZE, IWN_FW_UPDATED | main_textsz);
	iwn_mem_unlock(sc);

	/* wait at most one second for second alive notification */
	if ((error = tsleep(sc, PCATCH, "iwninit", hz)) != 0) {
		/* this isn't what was supposed to happen.. */
		printf("%s: timeout waiting for adapter to initialize\n",
		    sc->sc_dev.dv_xname);
	}

fail2:	free(fw, M_DEVBUF);
fail1:	return error;
}

void
iwn_calib_timeout(void *arg)
{
	struct iwn_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	int s;

	/* automatic rate control triggered every 500ms */
	if (ic->ic_fixed_rate == -1) {
		s = splnet();
		if (ic->ic_opmode == IEEE80211_M_STA)
			iwn_iter_func(sc, ic->ic_bss);
		else
			ieee80211_iterate_nodes(ic, iwn_iter_func, sc);
		splx(s);
	}

	/* automatic calibration every 60s */
	if (++sc->calib_cnt >= 120) {
		DPRINTF(("sending request for statistics\n"));
		(void)iwn_cmd(sc, IWN_CMD_GET_STATISTICS, NULL, 0, 1);
		sc->calib_cnt = 0;
	}

	timeout_add(&sc->calib_to, hz / 2);
}

void
iwn_iter_func(void *arg, struct ieee80211_node *ni)
{
	struct iwn_softc *sc = arg;
	struct iwn_node *wn = (struct iwn_node *)ni;

	ieee80211_amrr_choose(&sc->amrr, ni, &wn->amn);
}

void
iwn_ampdu_rx_start(struct iwn_softc *sc, struct iwn_rx_desc *desc)
{
	struct iwn_rx_stat *stat;

	DPRINTFN(2, ("received AMPDU stats\n"));
	/* save Rx statistics, they will be used on IWN_AMPDU_RX_DONE */
	stat = (struct iwn_rx_stat *)(desc + 1);
	memcpy(&sc->last_rx_stat, stat, sizeof (*stat));
	sc->last_rx_valid = 1;
}

void
iwn_rx_intr(struct iwn_softc *sc, struct iwn_rx_desc *desc,
    struct iwn_rx_data *data)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct iwn_rx_ring *ring = &sc->rxq;
	struct iwn_rbuf *rbuf;
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni;
	struct mbuf *m, *mnew;
	struct iwn_rx_stat *stat;
	caddr_t head;
	uint32_t *tail;
	int len, rssi;

	if (desc->type == IWN_AMPDU_RX_DONE) {
		/* check for prior AMPDU_RX_START */
		if (!sc->last_rx_valid) {
			DPRINTF(("missing AMPDU_RX_START\n"));
			ifp->if_ierrors++;
			return;
		}
		sc->last_rx_valid = 0;
		stat = &sc->last_rx_stat;
	} else
		stat = (struct iwn_rx_stat *)(desc + 1);

	if (stat->cfg_phy_len > IWN_STAT_MAXLEN) {
		printf("%s: invalid rx statistic header\n",
		    sc->sc_dev.dv_xname);
		ifp->if_ierrors++;
		return;
	}
	if (desc->type == IWN_AMPDU_RX_DONE) {
		struct iwn_rx_ampdu *ampdu =
		    (struct iwn_rx_ampdu *)(desc + 1);
		head = (caddr_t)(ampdu + 1);
		tail = (uint32_t *)(head + (len = letoh16(ampdu->len)));
	} else {
		head = (caddr_t)(stat + 1) + stat->cfg_phy_len;
		tail = (uint32_t *)(head + (len = letoh16(stat->len)));
	}

	/* discard Rx frames with bad CRC early */
	if ((letoh32(*tail) & IWN_RX_NOERROR) != IWN_RX_NOERROR) {
		DPRINTFN(2, ("rx flags error %x\n", letoh32(*tail)));
		ifp->if_ierrors++;
		return;
	}
	/* XXX for ieee80211_find_rxnode() */
	if (len < sizeof (struct ieee80211_frame)) {
		DPRINTF(("frame too short: %d\n", len));
		ic->ic_stats.is_rx_tooshort++;
		ifp->if_ierrors++;
		return;
	}

	m = data->m;

	/* finalize mbuf */
	m->m_pkthdr.rcvif = ifp;
	m->m_data = head;
	m->m_pkthdr.len = m->m_len = len;

	if ((rbuf = SLIST_FIRST(&sc->rxq.freelist)) != NULL) {
		MGETHDR(mnew, M_DONTWAIT, MT_DATA);
		if (mnew == NULL) {
			ic->ic_stats.is_rx_nombuf++;
			ifp->if_ierrors++;
			return;
		}

		/* attach Rx buffer to mbuf */
		MEXTADD(mnew, rbuf->vaddr, IWN_RBUF_SIZE, 0, iwn_free_rbuf,
		    rbuf);
		SLIST_REMOVE_HEAD(&sc->rxq.freelist, next);

		data->m = mnew;

		/* update Rx descriptor */
		ring->desc[ring->cur] = htole32(rbuf->paddr >> 8);
	} else {
		/* no free rbufs, copy frame */
		m = m_copym2(m, 0, M_COPYALL, M_DONTWAIT);
		if (m == NULL) {
			/* no free mbufs either, drop frame */
			ic->ic_stats.is_rx_nombuf++;
			ifp->if_ierrors++;
			return;
		}
	}

	rssi = iwn_get_rssi(stat);

#if NBPFILTER > 0
	if (sc->sc_drvbpf != NULL) {
		struct mbuf mb;
		struct iwn_rx_radiotap_header *tap = &sc->sc_rxtap;

		tap->wr_flags = 0;
		tap->wr_chan_freq =
		    htole16(ic->ic_channels[stat->chan].ic_freq);
		tap->wr_chan_flags =
		    htole16(ic->ic_channels[stat->chan].ic_flags);
		tap->wr_dbm_antsignal = (int8_t)rssi;
		tap->wr_dbm_antnoise = (int8_t)sc->noise;
		tap->wr_tsft = stat->tstamp;
		switch (stat->rate) {
		/* CCK rates */
		case  10: tap->wr_rate =   2; break;
		case  20: tap->wr_rate =   4; break;
		case  55: tap->wr_rate =  11; break;
		case 110: tap->wr_rate =  22; break;
		/* OFDM rates */
		case 0xd: tap->wr_rate =  12; break;
		case 0xf: tap->wr_rate =  18; break;
		case 0x5: tap->wr_rate =  24; break;
		case 0x7: tap->wr_rate =  36; break;
		case 0x9: tap->wr_rate =  48; break;
		case 0xb: tap->wr_rate =  72; break;
		case 0x1: tap->wr_rate =  96; break;
		case 0x3: tap->wr_rate = 108; break;
		/* unknown rate: should not happen */
		default:  tap->wr_rate =   0;
		}

		mb.m_data = (caddr_t)tap;
		mb.m_len = sc->sc_rxtap_len;
		mb.m_next = m;
		mb.m_nextpkt = NULL;
		mb.m_type = 0;
		mb.m_flags = 0;
		bpf_mtap(sc->sc_drvbpf, &mb, BPF_DIRECTION_IN);
	}
#endif

	/* grab a reference to the source node */
	wh = mtod(m, struct ieee80211_frame *);
	ni = ieee80211_find_rxnode(ic, wh);

	/* send the frame to the 802.11 layer */
	ieee80211_input(ifp, m, ni, rssi, 0);

	/* node is no longer needed */
	ieee80211_release_node(ic, ni);
}

void
iwn_rx_statistics(struct iwn_softc *sc, struct iwn_rx_desc *desc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct iwn_calib_state *calib = &sc->calib;
	struct iwn_stats *stats = (struct iwn_stats *)(desc + 1);

	/* ignore beacon statistics received during a scan */
	if (ic->ic_state != IEEE80211_S_RUN)
		return;

	DPRINTFN(3, ("received statistics (cmd=%d)\n", desc->type));
	sc->calib_cnt = 0;	/* reset timeout */

	/* test if temperature has changed */
	if (stats->general.temp != sc->rawtemp) {
		int temp;

		sc->rawtemp = stats->general.temp;
		temp = iwn_get_temperature(sc);
		DPRINTFN(2, ("temperature=%d\n", temp));

		/* update temperature sensor */
		sc->sensor.value = IWN_CTOMUK(temp);

		/* update Tx power if need be */
		iwn_power_calibration(sc, temp);
	}

	if (desc->type != IWN_BEACON_STATISTICS)
		return;	/* reply to a statistics request */

	sc->noise = iwn_get_noise(&stats->rx.general);
	DPRINTFN(3, ("noise=%d\n", sc->noise));

	/* test that RSSI and noise are present in stats report */
	if (letoh32(stats->rx.general.flags) != 1) {
		DPRINTF(("received statistics without RSSI\n"));
		return;
	}

	if (calib->state == IWN_CALIB_STATE_ASSOC)
		iwn_compute_differential_gain(sc, &stats->rx.general);
	else if (calib->state == IWN_CALIB_STATE_RUN)
		iwn_tune_sensitivity(sc, &stats->rx);
}

void
iwn_tx_intr(struct iwn_softc *sc, struct iwn_rx_desc *desc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct iwn_tx_ring *ring = &sc->txq[desc->qid & 0xf];
	struct iwn_tx_data *data = &ring->data[desc->idx];
	struct iwn_tx_stat *stat = (struct iwn_tx_stat *)(desc + 1);
	struct iwn_node *wn = (struct iwn_node *)data->ni;
	uint32_t status;

	DPRINTFN(4, ("tx done: qid=%d idx=%d retries=%d nkill=%d rate=%x "
	    "duration=%d status=%x\n", desc->qid, desc->idx, stat->ntries,
	    stat->nkill, stat->rate, letoh16(stat->duration),
	    letoh32(stat->status)));

	/*
	 * Update rate control statistics for the node.
	 */
	wn->amn.amn_txcnt++;
	if (stat->ntries > 0) {
		DPRINTFN(3, ("tx intr ntries %d\n", stat->ntries));
		wn->amn.amn_retrycnt++;
	}

	status = letoh32(stat->status) & 0xff;
	if (status != 1 && status != 2)
		ifp->if_oerrors++;
	else
		ifp->if_opackets++;

	bus_dmamap_unload(sc->sc_dmat, data->map);
	m_freem(data->m);
	data->m = NULL;
	ieee80211_release_node(ic, data->ni);
	data->ni = NULL;

	ring->queued--;

	sc->sc_tx_timer = 0;
	ifp->if_flags &= ~IFF_OACTIVE;
	(*ifp->if_start)(ifp);
}

void
iwn_cmd_intr(struct iwn_softc *sc, struct iwn_rx_desc *desc)
{
	struct iwn_tx_ring *ring = &sc->txq[4];
	struct iwn_tx_data *data;

	if ((desc->qid & 0xf) != 4)
		return;	/* not a command ack */

	data = &ring->data[desc->idx];

	/* if the command was mapped in a mbuf, free it */
	if (data->m != NULL) {
		bus_dmamap_unload(sc->sc_dmat, data->map);
		m_freem(data->m);
		data->m = NULL;
	}

	wakeup(&ring->cmd[desc->idx]);
}

void
iwn_notif_intr(struct iwn_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	uint16_t hw;

	hw = letoh16(sc->shared->closed_count);
	while (sc->rxq.cur != hw) {
		struct iwn_rx_data *data = &sc->rxq.data[sc->rxq.cur];
		struct iwn_rx_desc *desc = (void *)data->m->m_ext.ext_buf;

		DPRINTFN(4,("rx notification qid=%x idx=%d flags=%x type=%d "
		    "len=%d\n", desc->qid, desc->idx, desc->flags, desc->type,
		    letoh32(desc->len)));

		if (!(desc->qid & 0x80))	/* reply to a command */
			iwn_cmd_intr(sc, desc);

		switch (desc->type) {
		case IWN_RX_DONE:
		case IWN_AMPDU_RX_DONE:
			iwn_rx_intr(sc, desc, data);
			break;

		case IWN_AMPDU_RX_START:
			iwn_ampdu_rx_start(sc, desc);
			break;

		case IWN_TX_DONE:
			/* a 802.11 frame has been transmitted */
			iwn_tx_intr(sc, desc);
			break;

		case IWN_RX_STATISTICS:
		case IWN_BEACON_STATISTICS:
			iwn_rx_statistics(sc, desc);
			break;

		case IWN_UC_READY:
		{
			struct iwn_ucode_info *uc =
			    (struct iwn_ucode_info *)(desc + 1);

			/* the microcontroller is ready */
			DPRINTF(("microcode alive notification version=%d.%d "
			    "subtype=%x alive=%x\n", uc->major, uc->minor,
			    uc->subtype, letoh32(uc->valid)));

			if (letoh32(uc->valid) != 1) {
				printf("%s: microcontroller initialization "
				    "failed\n", sc->sc_dev.dv_xname);
				break;
			}
			if (uc->subtype == IWN_UCODE_INIT) {
				/* save microcontroller's report */
				memcpy(&sc->ucode_info, uc, sizeof (*uc));
			}
			break;
		}
		case IWN_STATE_CHANGED:
		{
			uint32_t *status = (uint32_t *)(desc + 1);

			/* enabled/disabled notification */
			DPRINTF(("state changed to %x\n", letoh32(*status)));

			if (letoh32(*status) & 1) {
				/* the radio button has to be pushed */
				printf("%s: Radio transmitter is off\n",
				    sc->sc_dev.dv_xname);
				/* turn the interface down */
				ifp->if_flags &= ~IFF_UP;
				iwn_stop(ifp, 1);
				return;	/* no further processing */
			}
			break;
		}
		case IWN_START_SCAN:
		{
			struct iwn_start_scan *scan =
			    (struct iwn_start_scan *)(desc + 1);

			DPRINTFN(2, ("scanning channel %d status %x\n",
			    scan->chan, letoh32(scan->status)));

			/* fix current channel */
			ic->ic_bss->ni_chan = &ic->ic_channels[scan->chan];
			break;
		}
		case IWN_STOP_SCAN:
		{
			struct iwn_stop_scan *scan =
			    (struct iwn_stop_scan *)(desc + 1);

			DPRINTF(("scan finished nchan=%d status=%d chan=%d\n",
			    scan->nchan, scan->status, scan->chan));

			if (scan->status == 1 && scan->chan <= 14) {
				/*
				 * We just finished scanning 802.11g channels,
				 * start scanning 802.11a ones.
				 */
				if (iwn_scan(sc, IEEE80211_CHAN_A) == 0)
					break;
			}
			ieee80211_end_scan(ifp);
			break;
		}
		}

		sc->rxq.cur = (sc->rxq.cur + 1) % IWN_RX_RING_COUNT;
	}

	/* tell the firmware what we have processed */
	hw = (hw == 0) ? IWN_RX_RING_COUNT - 1 : hw - 1;
	IWN_WRITE(sc, IWN_RX_WIDX, hw & ~7);
}

int
iwn_intr(void *arg)
{
	struct iwn_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_ic.ic_if;
	uint32_t r1, r2;

	/* disable interrupts */
	IWN_WRITE(sc, IWN_MASK, 0);

	r1 = IWN_READ(sc, IWN_INTR);
	r2 = IWN_READ(sc, IWN_INTR_STATUS);

	if (r1 == 0 && r2 == 0) {
		if (ifp->if_flags & IFF_UP)
			IWN_WRITE(sc, IWN_MASK, IWN_INTR_MASK);
		return 0;	/* not for us */
	}

	if (r1 == 0xffffffff)
		return 0;	/* hardware gone */

	/* ack interrupts */
	IWN_WRITE(sc, IWN_INTR, r1);
	IWN_WRITE(sc, IWN_INTR_STATUS, r2);

	DPRINTFN(6, ("interrupt reg1=%x reg2=%x\n", r1, r2));

	if (r1 & IWN_RF_TOGGLED) {
		uint32_t tmp = IWN_READ(sc, IWN_GPIO_CTL);
		printf("%s: RF switch: radio %s\n", sc->sc_dev.dv_xname,
		    (tmp & IWN_GPIO_RF_ENABLED) ? "enabled" : "disabled");
	}
	if (r1 & IWN_CT_REACHED) {
		printf("%s: critical temperature reached!\n",
		    sc->sc_dev.dv_xname);
	}
	if (r1 & (IWN_SW_ERROR | IWN_HW_ERROR)) {
		printf("%s: fatal firmware error\n", sc->sc_dev.dv_xname);
		ifp->if_flags &= ~IFF_UP;
		iwn_stop(ifp, 1);
		return 1;
	}
	if (r1 & (IWN_RX_INTR | IWN_SW_RX_INTR))
		iwn_notif_intr(sc);

	if (r1 & IWN_ALIVE_INTR)
		wakeup(sc);

	/* re-enable interrupts */
	if (ifp->if_flags & IFF_UP)
		IWN_WRITE(sc, IWN_MASK, IWN_INTR_MASK);

	return 1;
}

uint8_t
iwn_plcp_signal(int rate)
{
	switch (rate) {
	/* CCK rates (returned values are device-dependent) */
	case 2:		return 10;
	case 4:		return 20;
	case 11:	return 55;
	case 22:	return 110;

	/* OFDM rates (cf IEEE Std 802.11a-1999, pp. 14 Table 80) */
	/* R1-R4, (u)ral is R4-R1 */
	case 12:	return 0xd;
	case 18:	return 0xf;
	case 24:	return 0x5;
	case 36:	return 0x7;
	case 48:	return 0x9;
	case 72:	return 0xb;
	case 96:	return 0x1;
	case 108:	return 0x3;
	case 120:	return 0x3;
	}
	/* unknown rate (should not get there) */
	return 0;
}

/* determine if a given rate is CCK or OFDM */
#define IWN_RATE_IS_OFDM(rate) ((rate) >= 12 && (rate) != 22)

int
iwn_tx_data(struct iwn_softc *sc, struct mbuf *m0, struct ieee80211_node *ni,
    int ac)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct iwn_tx_ring *ring = &sc->txq[ac];
	struct iwn_tx_desc *desc;
	struct iwn_tx_data *data;
	struct iwn_tx_cmd *cmd;
	struct iwn_cmd_data *tx;
	struct ieee80211_frame *wh;
	struct mbuf *mnew;
	bus_addr_t paddr;
	uint32_t flags;
	uint8_t type;
	u_int hdrlen;
	int i, rate, error, pad, ovhd = 0;

	desc = &ring->desc[ring->cur];
	data = &ring->data[ring->cur];

	wh = mtod(m0, struct ieee80211_frame *);
	hdrlen = ieee80211_get_hdrlen(wh);
	type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;

	/* pickup a rate */
	if (IEEE80211_IS_MULTICAST(wh->i_addr1) ||
	    type != IEEE80211_FC0_TYPE_DATA) {
		/* mgmt/multicast frames are sent at the lowest avail. rate */
		rate = ni->ni_rates.rs_rates[0];
	} else if (ic->ic_fixed_rate != -1) {
		rate = ic->ic_sup_rates[ic->ic_curmode].
		    rs_rates[ic->ic_fixed_rate];
	} else
		rate = ni->ni_rates.rs_rates[ni->ni_txrate];
	rate &= IEEE80211_RATE_VAL;

#if NBPFILTER > 0
	if (sc->sc_drvbpf != NULL) {
		struct mbuf mb;
		struct iwn_tx_radiotap_header *tap = &sc->sc_txtap;

		tap->wt_flags = 0;
		tap->wt_chan_freq = htole16(ni->ni_chan->ic_freq);
		tap->wt_chan_flags = htole16(ni->ni_chan->ic_flags);
		tap->wt_rate = rate;
		tap->wt_hwqueue = ac;
		if (wh->i_fc[1] & IEEE80211_FC1_WEP)
			tap->wt_flags |= IEEE80211_RADIOTAP_F_WEP;

		mb.m_data = (caddr_t)tap;
		mb.m_len = sc->sc_txtap_len;
		mb.m_next = m0;
		mb.m_nextpkt = NULL;
		mb.m_type = 0;
		mb.m_flags = 0;
		bpf_mtap(sc->sc_drvbpf, &mb, BPF_DIRECTION_OUT);
	}
#endif

	cmd = &ring->cmd[ring->cur];
	cmd->code = IWN_CMD_TX_DATA;
	cmd->flags = 0;
	cmd->qid = ring->qid;
	cmd->idx = ring->cur;

	tx = (struct iwn_cmd_data *)cmd->data;
	/* no need to bzero tx, all fields are reinitialized here */

	if (wh->i_fc[1] & IEEE80211_FC1_PROTECTED) {
		const struct ieee80211_key *key =
		    &ic->ic_nw_keys[ic->ic_wep_txkey];
		if (key->k_cipher == IEEE80211_CIPHER_WEP40)
			tx->security = IWN_CIPHER_WEP40;
		else
			tx->security = IWN_CIPHER_WEP104;
		tx->security |= ic->ic_wep_txkey << 6;
		memcpy(&tx->key[3], key->k_key, key->k_len);
		/* compute crypto overhead */
		ovhd = IEEE80211_WEP_TOTLEN;
	} else
		tx->security = 0;

	flags = IWN_TX_AUTO_SEQ;
	if (!IEEE80211_IS_MULTICAST(wh->i_addr1))
		flags |= IWN_TX_NEED_ACK;

	if (IEEE80211_IS_MULTICAST(wh->i_addr1) ||
	    type != IEEE80211_FC0_TYPE_DATA)
		tx->id = IWN_ID_BROADCAST;
	else
		tx->id = IWN_ID_BSS;

	/* check if RTS/CTS or CTS-to-self protection must be used */
	if (!IEEE80211_IS_MULTICAST(wh->i_addr1)) {
		/* multicast frames are not sent at OFDM rates in 802.11b/g */
		if (m0->m_pkthdr.len + ovhd + IEEE80211_CRC_LEN >
		    ic->ic_rtsthreshold) {
			flags |= IWN_TX_NEED_RTS | IWN_TX_FULL_TXOP;
		} else if ((ic->ic_flags & IEEE80211_F_USEPROT) &&
		    IWN_RATE_IS_OFDM(rate)) {
			if (ic->ic_protmode == IEEE80211_PROT_CTSONLY)
				flags |= IWN_TX_NEED_CTS | IWN_TX_FULL_TXOP;
			else if (ic->ic_protmode == IEEE80211_PROT_RTSCTS)
				flags |= IWN_TX_NEED_RTS | IWN_TX_FULL_TXOP;
		}
	}

	if (type == IEEE80211_FC0_TYPE_MGT) {
		uint8_t subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;

		/* tell h/w to set timestamp in probe responses */
		if (subtype == IEEE80211_FC0_SUBTYPE_PROBE_RESP)
			flags |= IWN_TX_INSERT_TSTAMP;

		if (subtype == IEEE80211_FC0_SUBTYPE_ASSOC_REQ ||
		    subtype == IEEE80211_FC0_SUBTYPE_REASSOC_REQ)
			tx->timeout = htole16(3);
		else
			tx->timeout = htole16(2);
	} else
		tx->timeout = htole16(0);

	if (hdrlen & 3) {
		/* first segment's length must be a multiple of 4 */
		flags |= IWN_TX_NEED_PADDING;
		pad = 4 - (hdrlen & 3);
	} else
		pad = 0;

	tx->flags = htole32(flags);
	tx->len = htole16(m0->m_pkthdr.len);
	tx->rate = iwn_plcp_signal(rate);
	tx->rts_ntries = 60;
	tx->data_ntries = 15;
	tx->lifetime = htole32(IWN_LIFETIME_INFINITE);

	/* XXX alternate between Ant A and Ant B ? */
	tx->rflags = IWN_RFLAG_ANT_B;
	if (tx->id == IWN_ID_BROADCAST) {
		tx->ridx = IWN_MAX_TX_RETRIES - 1;
		if (!IWN_RATE_IS_OFDM(rate))
			tx->rflags |= IWN_RFLAG_CCK;
	} else {
		tx->ridx = 0;
		/* tell adapter to ignore rflags */
		tx->flags |= htole32(IWN_TX_USE_NODE_RATE);
	}

	/* copy and trim IEEE802.11 header */
	memcpy((uint8_t *)(tx + 1), wh, hdrlen);
	m_adj(m0, hdrlen);

	error = bus_dmamap_load_mbuf(sc->sc_dmat, data->map, m0,
	    BUS_DMA_NOWAIT);
	if (error != 0 && error != EFBIG) {
		printf("%s: could not map mbuf (error %d)\n",
		    sc->sc_dev.dv_xname, error);
		m_freem(m0);
		return error;
	}
	if (error != 0) {
		/* too many fragments, linearize */

		MGETHDR(mnew, M_DONTWAIT, MT_DATA);
		if (mnew == NULL) {
			m_freem(m0);
			return ENOMEM;
		}
		M_DUP_PKTHDR(mnew, m0);
		if (m0->m_pkthdr.len > MHLEN) {
			MCLGET(mnew, M_DONTWAIT);
			if (!(mnew->m_flags & M_EXT)) {
				m_freem(m0);
				m_freem(mnew);
				return ENOMEM;
			}
		}

		m_copydata(m0, 0, m0->m_pkthdr.len, mtod(mnew, caddr_t));
		m_freem(m0);
		mnew->m_len = mnew->m_pkthdr.len;
		m0 = mnew;

		error = bus_dmamap_load_mbuf(sc->sc_dmat, data->map, m0,
		    BUS_DMA_NOWAIT);
		if (error != 0) {
			printf("%s: could not map mbuf (error %d)\n",
			    sc->sc_dev.dv_xname, error);
			m_freem(m0);
			return error;
		}
	}

	data->m = m0;
	data->ni = ni;

	DPRINTFN(4, ("sending data: qid=%d idx=%d len=%d nsegs=%d\n",
	    ring->qid, ring->cur, m0->m_pkthdr.len, data->map->dm_nsegs));

	paddr = ring->cmd_dma.paddr + ring->cur * sizeof (struct iwn_tx_cmd);
	tx->loaddr = htole32(paddr + 4 +
	    offsetof(struct iwn_cmd_data, ntries));
	tx->hiaddr = 0;	/* limit to 32-bit physical addresses */

	/* first scatter/gather segment is used by the tx data command */
	IWN_SET_DESC_NSEGS(desc, 1 + data->map->dm_nsegs);
	IWN_SET_DESC_SEG(desc, 0, paddr, 4 + sizeof (*tx) + hdrlen + pad);
	for (i = 1; i <= data->map->dm_nsegs; i++) {
		IWN_SET_DESC_SEG(desc, i, data->map->dm_segs[i - 1].ds_addr,
		     data->map->dm_segs[i - 1].ds_len);
	}
	sc->shared->len[ring->qid][ring->cur] =
	    htole16(hdrlen + m0->m_pkthdr.len + 8);

	ring->queued++;

	/* kick ring */
	ring->cur = (ring->cur + 1) % IWN_TX_RING_COUNT;
	IWN_WRITE(sc, IWN_TX_WIDX, ring->qid << 8 | ring->cur);

	return 0;
}

void
iwn_start(struct ifnet *ifp)
{
	struct iwn_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni;
	struct mbuf *m0;

	/*
	 * net80211 may still try to send management frames even if the
	 * IFF_RUNNING flag is not set...
	 */
	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

	for (;;) {
		IF_POLL(&ic->ic_mgtq, m0);
		if (m0 != NULL) {
			/* management frames go into ring 0 */
			if (sc->txq[0].queued >= sc->txq[0].count - 8) {
				ifp->if_flags |= IFF_OACTIVE;
				break;
			}
			IF_DEQUEUE(&ic->ic_mgtq, m0);

			ni = (struct ieee80211_node *)m0->m_pkthdr.rcvif;
			m0->m_pkthdr.rcvif = NULL;
#if NBPFILTER > 0
			if (ic->ic_rawbpf != NULL)
				bpf_mtap(ic->ic_rawbpf, m0, BPF_DIRECTION_OUT);
#endif
			if (iwn_tx_data(sc, m0, ni, 0) != 0)
				break;

		} else {
			if (ic->ic_state != IEEE80211_S_RUN)
				break;
			IFQ_POLL(&ifp->if_snd, m0);
			if (m0 == NULL)
				break;
			if (sc->txq[0].queued >= sc->txq[0].count - 8) {
				/* there is no place left in this ring */
				ifp->if_flags |= IFF_OACTIVE;
				break;
			}
			IFQ_DEQUEUE(&ifp->if_snd, m0);
#if NBPFILTER > 0
			if (ifp->if_bpf != NULL)
				bpf_mtap(ifp->if_bpf, m0, BPF_DIRECTION_OUT);
#endif
			m0 = ieee80211_encap(ifp, m0, &ni);
			if (m0 == NULL)
				continue;
#if NBPFILTER > 0
			if (ic->ic_rawbpf != NULL)
				bpf_mtap(ic->ic_rawbpf, m0, BPF_DIRECTION_OUT);
#endif
			if (iwn_tx_data(sc, m0, ni, 0) != 0) {
				if (ni != NULL)
					ieee80211_release_node(ic, ni);
				ifp->if_oerrors++;
				break;
			}
		}

		sc->sc_tx_timer = 5;
		ifp->if_timer = 1;
	}
}

void
iwn_watchdog(struct ifnet *ifp)
{
	struct iwn_softc *sc = ifp->if_softc;

	ifp->if_timer = 0;

	if (sc->sc_tx_timer > 0) {
		if (--sc->sc_tx_timer == 0) {
			printf("%s: device timeout\n", sc->sc_dev.dv_xname);
			ifp->if_flags &= ~IFF_UP;
			iwn_stop(ifp, 1);
			ifp->if_oerrors++;
			return;
		}
		ifp->if_timer = 1;
	}

	ieee80211_watchdog(ifp);
}

int
iwn_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct iwn_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifaddr *ifa;
	struct ifreq *ifr;
	int s, error = 0;

	s = splnet();

	switch (cmd) {
	case SIOCSIFADDR:
		ifa = (struct ifaddr *)data;
		ifp->if_flags |= IFF_UP;
#ifdef INET
		if (ifa->ifa_addr->sa_family == AF_INET)
			arp_ifinit(&ic->ic_ac, ifa);
#endif
		/* FALLTHROUGH */
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (!(ifp->if_flags & IFF_RUNNING))
				iwn_init(ifp);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				iwn_stop(ifp, 1);
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

	default:
		error = ieee80211_ioctl(ifp, cmd, data);
	}

	if (error == ENETRESET) {
		if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) ==
		    (IFF_UP | IFF_RUNNING))
			iwn_init(ifp);
		error = 0;
	}

	splx(s);
	return error;
}

void
iwn_read_eeprom(struct iwn_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	char domain[4];
	uint16_t val;
	int i, error;

	if ((error = iwn_eeprom_lock(sc)) != 0) {
		printf("%s: could not lock EEPROM (error=%d)\n",
		    sc->sc_dev.dv_xname, error);
		return;
	}
	/* read and print regulatory domain */
	iwn_read_prom_data(sc, IWN_EEPROM_DOMAIN, domain, 4);
	printf(", %.4s", domain);

	/* read and print MAC address */
	iwn_read_prom_data(sc, IWN_EEPROM_MAC, ic->ic_myaddr, 6);
	printf(", address %s\n", ether_sprintf(ic->ic_myaddr));

	/* read the list of authorized channels */
	for (i = 0; i < IWN_CHAN_BANDS_COUNT; i++)
		iwn_read_eeprom_channels(sc, i);

	/* read maximum allowed Tx power for 2GHz and 5GHz bands */
	iwn_read_prom_data(sc, IWN_EEPROM_MAXPOW, &val, 2);
	sc->maxpwr2GHz = val & 0xff;
	sc->maxpwr5GHz = val >> 8;
	/* check that EEPROM values are correct */
	if (sc->maxpwr5GHz < 20 || sc->maxpwr5GHz > 50)
		sc->maxpwr5GHz = 38;
	if (sc->maxpwr2GHz < 20 || sc->maxpwr2GHz > 50)
		sc->maxpwr2GHz = 38;
	DPRINTF(("maxpwr 2GHz=%d 5GHz=%d\n", sc->maxpwr2GHz, sc->maxpwr5GHz));

	/* read voltage at which samples were taken */
	iwn_read_prom_data(sc, IWN_EEPROM_VOLTAGE, &val, 2);
	sc->eeprom_voltage = (int16_t)letoh16(val);
	DPRINTF(("voltage=%d (in 0.3V)\n", sc->eeprom_voltage));

	/* read power groups */
	iwn_read_prom_data(sc, IWN_EEPROM_BANDS, sc->bands, sizeof sc->bands);
#ifdef IWN_DEBUG
	if (iwn_debug > 0) {
		for (i = 0; i < IWN_NBANDS; i++)
			iwn_print_power_group(sc, i);
	}
#endif
	iwn_eeprom_unlock(sc);
}

void
iwn_read_eeprom_channels(struct iwn_softc *sc, int n)
{
	struct ieee80211com *ic = &sc->sc_ic;
	const struct iwn_chan_band *band = &iwn_bands[n];
	struct iwn_eeprom_chan channels[IWN_MAX_CHAN_PER_BAND];
	int chan, i;

	iwn_read_prom_data(sc, band->addr, channels,
	    band->nchan * sizeof (struct iwn_eeprom_chan));

	for (i = 0; i < band->nchan; i++) {
		if (!(channels[i].flags & IWN_EEPROM_CHAN_VALID))
			continue;

		chan = band->chan[i];

		if (n == 0) {	/* 2GHz band */
			ic->ic_channels[chan].ic_freq =
			    ieee80211_ieee2mhz(chan, IEEE80211_CHAN_2GHZ);
			ic->ic_channels[chan].ic_flags =
			    IEEE80211_CHAN_CCK | IEEE80211_CHAN_OFDM |
			    IEEE80211_CHAN_DYN | IEEE80211_CHAN_2GHZ;

		} else {	/* 5GHz band */
			/*
			 * Some adapters support channels 7, 8, 11 and 12
			 * both in the 2GHz *and* 5GHz bands.
			 * Because of limitations in our net80211(9) stack,
			 * we can't support these channels in 5GHz band.
			 */
			if (chan <= 14)
				continue;

			ic->ic_channels[chan].ic_freq =
			    ieee80211_ieee2mhz(chan, IEEE80211_CHAN_5GHZ);
			ic->ic_channels[chan].ic_flags = IEEE80211_CHAN_A;
		}

		/* is active scan allowed on this channel? */
		if (!(channels[i].flags & IWN_EEPROM_CHAN_ACTIVE)) {
			ic->ic_channels[chan].ic_flags |=
			    IEEE80211_CHAN_PASSIVE;
		}

		/* save maximum allowed power for this channel */
		sc->maxpwr[chan] = channels[i].maxpwr;

		DPRINTF(("adding chan %d flags=0x%x maxpwr=%d\n",
		    chan, channels[i].flags, sc->maxpwr[chan]));
	}
}

#ifdef IWN_DEBUG
void
iwn_print_power_group(struct iwn_softc *sc, int i)
{
	struct iwn_eeprom_band *band = &sc->bands[i];
	struct iwn_eeprom_chan_samples *chans = band->chans;
	int j, c;

	printf("===band %d===\n", i);
	printf("chan lo=%d, chan hi=%d\n", band->lo, band->hi);
	printf("chan1 num=%d\n", chans[0].num);
	for (c = 0; c < IWN_NTXCHAINS; c++) {
		for (j = 0; j < IWN_NSAMPLES; j++) {
			printf("chain %d, sample %d: temp=%d gain=%d "
			    "power=%d pa_det=%d\n", c, j,
			    chans[0].samples[c][j].temp,
			    chans[0].samples[c][j].gain,
			    chans[0].samples[c][j].power,
			    chans[0].samples[c][j].pa_det);
		}
	}
	printf("chan2 num=%d\n", chans[1].num);
	for (c = 0; c < IWN_NTXCHAINS; c++) {
		for (j = 0; j < IWN_NSAMPLES; j++) {
			printf("chain %d, sample %d: temp=%d gain=%d "
			    "power=%d pa_det=%d\n", c, j,
			    chans[1].samples[c][j].temp,
			    chans[1].samples[c][j].gain,
			    chans[1].samples[c][j].power,
			    chans[1].samples[c][j].pa_det);
		}
	}
}
#endif

/*
 * Send a command to the firmware.
 */
int
iwn_cmd(struct iwn_softc *sc, int code, const void *buf, int size, int async)
{
	struct iwn_tx_ring *ring = &sc->txq[4];
	struct iwn_tx_desc *desc;
	struct iwn_tx_cmd *cmd;
	bus_addr_t paddr;

	KASSERT(size <= sizeof cmd->data);

	desc = &ring->desc[ring->cur];
	cmd = &ring->cmd[ring->cur];

	cmd->code = code;
	cmd->flags = 0;
	cmd->qid = ring->qid;
	cmd->idx = ring->cur;
	memcpy(cmd->data, buf, size);

	paddr = ring->cmd_dma.paddr + ring->cur * sizeof (struct iwn_tx_cmd);

	IWN_SET_DESC_NSEGS(desc, 1);
	IWN_SET_DESC_SEG(desc, 0, paddr, 4 + size);
	sc->shared->len[ring->qid][ring->cur] = htole16(8);

	/* kick cmd ring */
	ring->cur = (ring->cur + 1) % IWN_TX_RING_COUNT;
	IWN_WRITE(sc, IWN_TX_WIDX, ring->qid << 8 | ring->cur);

	return async ? 0 : tsleep(cmd, PCATCH, "iwncmd", hz);
}

/*
 * Configure hardware multi-rate retries for one node.
 */
int
iwn_setup_node_mrr(struct iwn_softc *sc, uint8_t id, int async)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct iwn_cmd_mrr mrr;
	int i, ridx;

	memset(&mrr, 0, sizeof mrr);
	mrr.id = id;
	mrr.ssmask = 2;
	mrr.dsmask = 3;
	mrr.ampdu_disable = 3;
	mrr.ampdu_limit = 4000;

	if (id == IWN_ID_BSS)
		ridx = IWN_OFDM54;
	else if (ic->ic_curmode == IEEE80211_MODE_11A)
		ridx = IWN_OFDM6;
	else
		ridx = IWN_CCK1;
	for (i = 0; i < IWN_MAX_TX_RETRIES; i++) {
		mrr.table[i].rate = iwn_ridx_to_plcp[ridx];
		mrr.table[i].rflags = IWN_RFLAG_ANT_B;
		if (ridx <= IWN_CCK11)
			mrr.table[i].rflags |= IWN_RFLAG_CCK;
		ridx = iwn_prev_ridx[ridx];
	}
	return iwn_cmd(sc, IWN_CMD_NODE_MRR_SETUP, &mrr, sizeof mrr, async);
}

/*
 * Install a pairwise key into the hardware.
 */
int
iwn_set_key(struct ieee80211com *ic, struct ieee80211_node *ni,
    const struct ieee80211_key *k)
{
	struct iwn_softc *sc = ic->ic_softc;
	struct iwn_node_info node;

	if (k->k_flags & IEEE80211_KEY_GROUP)
		return 0;

	memset(&node, 0, sizeof node);

	switch (k->k_cipher) {
	case IEEE80211_CIPHER_CCMP:
		node.security = htole16(IWN_CIPHER_CCMP);
		node.security |= htole16(k->k_id << 8);
		memcpy(node.key, k->k_key, k->k_len);
		break;
	default:
		return 0;
	}

	node.id = IWN_ID_BSS;
	IEEE80211_ADDR_COPY(node.macaddr, ni->ni_macaddr);
	node.control = IWN_NODE_UPDATE;
	node.flags = IWN_FLAG_SET_KEY;

	return iwn_cmd(sc, IWN_CMD_ADD_NODE, &node, sizeof node, 1);
}

void
iwn_edcaupdate(struct ieee80211com *ic)
{
#define IWN_EXP2(x)	((1 << (x)) - 1)	/* CWmin = 2^ECWmin - 1 */
	struct iwn_softc *sc = ic->ic_softc;
	struct iwn_edca_params cmd;
	int aci;

	memset(&cmd, 0, sizeof cmd);
	cmd.flags = htole32(IWN_EDCA_UPDATE);
	for (aci = 0; aci < EDCA_NUM_AC; aci++) {
		const struct ieee80211_edca_ac_params *ac =
		    &ic->ic_edca_ac[aci];
		cmd.ac[aci].aifsn = ac->ac_aifsn;
		cmd.ac[aci].cwmin = htole16(IWN_EXP2(ac->ac_ecwmin));
		cmd.ac[aci].cwmax = htole16(IWN_EXP2(ac->ac_ecwmax));
		cmd.ac[aci].txoplimit =
		    htole16(IEEE80211_TXOP_TO_US(ac->ac_txoplimit));
	}
	(void)iwn_cmd(sc, IWN_CMD_EDCA_PARAMS, &cmd, sizeof cmd, 1);
#undef IWN_EXP2
}

void
iwn_set_led(struct iwn_softc *sc, uint8_t which, uint8_t off, uint8_t on)
{
	struct iwn_cmd_led led;

	led.which = which;
	led.unit = htole32(100000);	/* on/off in unit of 100ms */
	led.off = off;
	led.on = on;

	(void)iwn_cmd(sc, IWN_CMD_SET_LED, &led, sizeof led, 1);
}

/*
 * Set the critical temperature at which the firmware will automatically stop
 * the radio transmitter.
 */
int
iwn_set_critical_temp(struct iwn_softc *sc)
{
	struct iwn_ucode_info *uc = &sc->ucode_info;
	struct iwn_critical_temp crit;
	uint32_t r1, r2, r3, temp;

	r1 = letoh32(uc->temp[0].chan20MHz);
	r2 = letoh32(uc->temp[1].chan20MHz);
	r3 = letoh32(uc->temp[2].chan20MHz);
	/* inverse function of iwn_get_temperature() */
	temp = r2 + (IWN_CTOK(110) * (r3 - r1)) / 259;

	IWN_WRITE(sc, IWN_UCODE_CLR, IWN_CTEMP_STOP_RF);

	memset(&crit, 0, sizeof crit);
	crit.tempR = htole32(temp);
	DPRINTF(("setting critical temperature to %u\n", temp));
	return iwn_cmd(sc, IWN_CMD_SET_CRITICAL_TEMP, &crit, sizeof crit, 0);
}

void
iwn_enable_tsf(struct iwn_softc *sc, struct ieee80211_node *ni)
{
	struct iwn_cmd_tsf tsf;
	uint64_t val, mod;

	memset(&tsf, 0, sizeof tsf);
	memcpy(&tsf.tstamp, ni->ni_tstamp, sizeof (uint64_t));
	tsf.bintval = htole16(ni->ni_intval);
	tsf.lintval = htole16(10);

	/* compute remaining time until next beacon */
	val = (uint64_t)ni->ni_intval * 1024;	/* msecs -> usecs */
	mod = letoh64(tsf.tstamp) % val;
	tsf.binitval = htole32((uint32_t)(val - mod));

	DPRINTF(("TSF bintval=%u tstamp=%llu, init=%u\n",
	    ni->ni_intval, letoh64(tsf.tstamp), (uint32_t)(val - mod)));

	if (iwn_cmd(sc, IWN_CMD_TSF, &tsf, sizeof tsf, 1) != 0)
		printf("%s: could not enable TSF\n", sc->sc_dev.dv_xname);
}

void
iwn_power_calibration(struct iwn_softc *sc, int temp)
{
	struct ieee80211com *ic = &sc->sc_ic;

	DPRINTF(("temperature %d->%d\n", sc->temp, temp));

	/* adjust Tx power if need be (delta >= 3°C) */
	if (abs(temp - sc->temp) < 3)
		return;

	sc->temp = temp;

	DPRINTF(("setting Tx power for channel %d\n",
	    ieee80211_chan2ieee(ic, ic->ic_bss->ni_chan)));
	if (iwn_set_txpower(sc, ic->ic_bss->ni_chan, 1) != 0) {
		/* just warn, too bad for the automatic calibration... */
		printf("%s: could not adjust Tx power\n", sc->sc_dev.dv_xname);
	}
}

/*
 * Set Tx power for a given channel (each rate has its own power settings).
 * This function takes into account the regulatory information from EEPROM,
 * the current temperature and the current voltage.
 */
int
iwn_set_txpower(struct iwn_softc *sc, struct ieee80211_channel *ch, int async)
{
/* fixed-point arithmetic division using a n-bit fractional part */
#define fdivround(a, b, n)	\
	((((1 << n) * (a)) / (b) + (1 << n) / 2) / (1 << n))
/* linear interpolation */
#define interpolate(x, x1, y1, x2, y2, n)	\
	((y1) + fdivround(((int)(x) - (x1)) * ((y2) - (y1)), (x2) - (x1), n))

	static const int tdiv[IWN_NATTEN_GROUPS] = { 9, 8, 8, 8, 6 };
	struct ieee80211com *ic = &sc->sc_ic;
	struct iwn_ucode_info *uc = &sc->ucode_info;
	struct iwn_cmd_txpower cmd;
	struct iwn_eeprom_chan_samples *chans;
	const uint8_t *rf_gain, *dsp_gain;
	int32_t vdiff, tdiff;
	int i, c, grp, maxpwr;
	u_int chan;

	/* get channel number */
	chan = ieee80211_chan2ieee(ic, ch);

	memset(&cmd, 0, sizeof cmd);
	cmd.band = IEEE80211_IS_CHAN_5GHZ(ch) ? 0 : 1;
	cmd.chan = chan;

	if (IEEE80211_IS_CHAN_5GHZ(ch)) {
		maxpwr   = sc->maxpwr5GHz;
		rf_gain  = iwn_rf_gain_5ghz;
		dsp_gain = iwn_dsp_gain_5ghz;
	} else {
		maxpwr   = sc->maxpwr2GHz;
		rf_gain  = iwn_rf_gain_2ghz;
		dsp_gain = iwn_dsp_gain_2ghz;
	}

	/* compute voltage compensation */
	vdiff = ((int32_t)letoh32(uc->volt) - sc->eeprom_voltage) / 7;
	if (vdiff > 0)
		vdiff *= 2;
	if (abs(vdiff) > 2)
		vdiff = 0;
	DPRINTF(("voltage compensation=%d (UCODE=%d, EEPROM=%d)\n",
	    vdiff, letoh32(uc->volt), sc->eeprom_voltage));

	/* get channel's attenuation group */
	if (chan <= 20)		/* 1-20 */
		grp = 4;
	else if (chan <= 43)	/* 34-43 */
		grp = 0;
	else if (chan <= 70)	/* 44-70 */
		grp = 1;
	else if (chan <= 124)	/* 71-124 */
		grp = 2;
	else			/* 125-200 */
		grp = 3;
	DPRINTF(("chan %d, attenuation group=%d\n", chan, grp));

	/* get channel's sub-band */
	for (i = 0; i < IWN_NBANDS; i++)
		if (sc->bands[i].lo != 0 &&
		    sc->bands[i].lo <= chan && chan <= sc->bands[i].hi)
			break;
	chans = sc->bands[i].chans;
	DPRINTF(("chan %d sub-band=%d\n", chan, i));

	for (c = 0; c < IWN_NTXCHAINS; c++) {
		uint8_t power, gain, temp;
		int maxchpwr, pwr, ridx, idx;

		power = interpolate(chan,
		    chans[0].num, chans[0].samples[c][1].power,
		    chans[1].num, chans[1].samples[c][1].power, 1);
		gain  = interpolate(chan,
		    chans[0].num, chans[0].samples[c][1].gain,
		    chans[1].num, chans[1].samples[c][1].gain, 1);
		temp  = interpolate(chan,
		    chans[0].num, chans[0].samples[c][1].temp,
		    chans[1].num, chans[1].samples[c][1].temp, 1);
		DPRINTF(("Tx chain %d: power=%d gain=%d temp=%d\n",
		    c, power, gain, temp));

		/* compute temperature compensation */
		tdiff = ((sc->temp - temp) * 2) / tdiv[grp];
		DPRINTF(("temperature compensation=%d (current=%d, "
		    "EEPROM=%d)\n", tdiff, sc->temp, temp));

		for (ridx = 0; ridx <= IWN_RIDX_MAX; ridx++) {
			maxchpwr = sc->maxpwr[chan] * 2;
			if ((ridx / 8) & 1) {
				/* MIMO: decrease Tx power (-3dB) */
				maxchpwr -= 6;
			}

			pwr = maxpwr - 10;

			/* decrease power for highest OFDM rates */
			if ((ridx % 8) == 5)		/* 48Mbit/s */
				pwr -= 5;
			else if ((ridx % 8) == 6)	/* 54Mbit/s */
				pwr -= 7;
			else if ((ridx % 8) == 7)	/* 60Mbit/s */
				pwr -= 10;

			if (pwr > maxchpwr)
				pwr = maxchpwr;

			idx = gain - (pwr - power) - tdiff - vdiff;
			if ((ridx / 8) & 1)	/* MIMO */
				idx += (int32_t)letoh32(uc->atten[grp][c]);

			if (cmd.band == 0)
				idx += 9;	/* 5GHz */
			if (ridx == IWN_RIDX_MAX)
				idx += 5;	/* CCK */

			/* make sure idx stays in a valid range */
			if (idx < 0)
				idx = 0;
			else if (idx > IWN_MAX_PWR_INDEX)
				idx = IWN_MAX_PWR_INDEX;

			DPRINTF(("Tx chain %d, rate idx %d: power=%d\n",
			    c, ridx, idx));
			cmd.power[ridx].rf_gain[c] = rf_gain[idx];
			cmd.power[ridx].dsp_gain[c] = dsp_gain[idx];
		}
	}

	DPRINTF(("setting tx power for chan %d\n", chan));
	return iwn_cmd(sc, IWN_CMD_TXPOWER, &cmd, sizeof cmd, async);

#undef interpolate
#undef fdivround
}

/*
 * Get the best (maximum) RSSI among Rx antennas (in dBm).
 */
int
iwn_get_rssi(const struct iwn_rx_stat *stat)
{
	uint8_t mask, agc;
	int rssi;

	mask = (letoh16(stat->antenna) >> 4) & 0x7;
	agc  = (letoh16(stat->agc) >> 7) & 0x7f;

	rssi = 0;
	if (mask & (1 << 0))	/* Ant A */
		rssi = max(rssi, stat->rssi[0]);
	if (mask & (1 << 1))	/* Ant B */
		rssi = max(rssi, stat->rssi[2]);
	if (mask & (1 << 2))	/* Ant C */
		rssi = max(rssi, stat->rssi[4]);

	return rssi - agc - IWN_RSSI_TO_DBM;
}

/*
 * Get the average noise among Rx antennas (in dBm).
 */
int
iwn_get_noise(const struct iwn_rx_general_stats *stats)
{
	int i, total, nbant, noise;

	total = nbant = 0;
	for (i = 0; i < 3; i++) {
		if ((noise = letoh32(stats->noise[i]) & 0xff) == 0)
			continue;
		total += noise;
		nbant++;
	}
	/* there should be at least one antenna but check anyway */
	return (nbant == 0) ? -127 : (total / nbant) - 107;
}

/*
 * Read temperature (in degC) from the on-board thermal sensor.
 */
int
iwn_get_temperature(struct iwn_softc *sc)
{
	struct iwn_ucode_info *uc = &sc->ucode_info;
	int32_t r1, r2, r3, r4, temp;

	r1 = letoh32(uc->temp[0].chan20MHz);
	r2 = letoh32(uc->temp[1].chan20MHz);
	r3 = letoh32(uc->temp[2].chan20MHz);
	r4 = letoh32(sc->rawtemp);

	if (r1 == r3)	/* prevents division by 0 (should not happen) */
		return 0;

	/* sign-extend 23-bit R4 value to 32-bit */
	r4 = (r4 << 8) >> 8;
	/* compute temperature */
	temp = (259 * (r4 - r2)) / (r3 - r1);
	temp = (temp * 97) / 100 + 8;

	DPRINTF(("temperature %dK/%dC\n", temp, IWN_KTOC(temp)));
	return IWN_KTOC(temp);
}

/*
 * Initialize sensitivity calibration state machine.
 */
int
iwn_init_sensitivity(struct iwn_softc *sc)
{
	struct iwn_calib_state *calib = &sc->calib;
	struct iwn_phy_calib_cmd cmd;
	int error;

	/* reset calibration state */
	memset(calib, 0, sizeof (*calib));
	calib->state = IWN_CALIB_STATE_INIT;
	calib->cck_state = IWN_CCK_STATE_HIFA;
	/* initial values taken from the reference driver */
	calib->corr_ofdm_x1     = 105;
	calib->corr_ofdm_mrc_x1 = 220;
	calib->corr_ofdm_x4     =  90;
	calib->corr_ofdm_mrc_x4 = 170;
	calib->corr_cck_x4      = 125;
	calib->corr_cck_mrc_x4  = 200;
	calib->energy_cck       = 100;

	/* write initial sensitivity values */
	if ((error = iwn_send_sensitivity(sc)) != 0)
		return error;

	memset(&cmd, 0, sizeof cmd);
	cmd.code = IWN_SET_DIFF_GAIN;
	/* differential gains initially set to 0 for all 3 antennas */
	DPRINTF(("setting differential gains\n"));
	return iwn_cmd(sc, IWN_PHY_CALIB, &cmd, sizeof cmd, 1);
}

/*
 * Collect noise and RSSI statistics for the first 20 beacons received
 * after association and use them to determine connected antennas and
 * set differential gains.
 */
void
iwn_compute_differential_gain(struct iwn_softc *sc,
    const struct iwn_rx_general_stats *stats)
{
	struct iwn_calib_state *calib = &sc->calib;
	struct iwn_phy_calib_cmd cmd;
	int i, val;

	/* accumulate RSSI and noise for all 3 antennas */
	for (i = 0; i < 3; i++) {
		calib->rssi[i] += letoh32(stats->rssi[i]) & 0xff;
		calib->noise[i] += letoh32(stats->noise[i]) & 0xff;
	}

	/* we update differential gain only once after 20 beacons */
	if (++calib->nbeacons < 20)
		return;

	/* determine antenna with highest average RSSI */
	val = max(calib->rssi[0], calib->rssi[1]);
	val = max(calib->rssi[2], val);

	/* determine which antennas are connected */
	sc->antmsk = 0;
	for (i = 0; i < 3; i++)
		if (val - calib->rssi[i] <= 15 * 20)
			sc->antmsk |= 1 << i;
	/* if neither Ant A and Ant B are connected.. */
	if ((sc->antmsk & (1 << 0 | 1 << 1)) == 0)
		sc->antmsk |= 1 << 1;	/* ..mark Ant B as connected! */

	/* get minimal noise among connected antennas */
	val = INT_MAX;	/* ok, there's at least one */
	for (i = 0; i < 3; i++)
		if (sc->antmsk & (1 << i))
			val = min(calib->noise[i], val);

	memset(&cmd, 0, sizeof cmd);
	cmd.code = IWN_SET_DIFF_GAIN;
	/* set differential gains for connected antennas */
	for (i = 0; i < 3; i++) {
		if (sc->antmsk & (1 << i)) {
			cmd.gain[i] = (calib->noise[i] - val) / 30;
			/* limit differential gain to 3 */
			cmd.gain[i] = min(cmd.gain[i], 3);
			cmd.gain[i] |= IWN_GAIN_SET;
		}
	}
	DPRINTF(("setting differential gains Ant A/B/C: %x/%x/%x (%x)\n",
	    cmd.gain[0], cmd.gain[1], cmd.gain[2], sc->antmsk));
	if (iwn_cmd(sc, IWN_PHY_CALIB, &cmd, sizeof cmd, 1) == 0)
		calib->state = IWN_CALIB_STATE_RUN;
}

/*
 * Tune RF Rx sensitivity based on the number of false alarms detected
 * during the last beacon period.
 */
void
iwn_tune_sensitivity(struct iwn_softc *sc, const struct iwn_rx_stats *stats)
{
#define inc_clip(val, inc, max)			\
	if ((val) < (max)) {			\
		if ((val) < (max) - (inc))	\
			(val) += (inc);		\
		else				\
			(val) = (max);		\
		needs_update = 1;		\
	}
#define dec_clip(val, dec, min)			\
	if ((val) > (min)) {			\
		if ((val) > (min) + (dec))	\
			(val) -= (dec);		\
		else				\
			(val) = (min);		\
		needs_update = 1;		\
	}

	struct iwn_calib_state *calib = &sc->calib;
	uint32_t val, rxena, fa;
	uint32_t energy[3], energy_min;
	uint8_t noise[3], noise_ref;
	int i, needs_update = 0;

	/* check that we've been enabled long enough */
	if ((rxena = letoh32(stats->general.load)) == 0)
		return;

	/* compute number of false alarms since last call for OFDM */
	fa  = letoh32(stats->ofdm.bad_plcp) - calib->bad_plcp_ofdm;
	fa += letoh32(stats->ofdm.fa) - calib->fa_ofdm;
	fa *= 200 * 1024;	/* 200TU */

	/* save counters values for next call */
	calib->bad_plcp_ofdm = letoh32(stats->ofdm.bad_plcp);
	calib->fa_ofdm = letoh32(stats->ofdm.fa);

	if (fa > 50 * rxena) {
		/* high false alarm count, decrease sensitivity */
		DPRINTFN(2, ("OFDM high false alarm count: %u\n", fa));
		inc_clip(calib->corr_ofdm_x1,     1, 140);
		inc_clip(calib->corr_ofdm_mrc_x1, 1, 270);
		inc_clip(calib->corr_ofdm_x4,     1, 120);
		inc_clip(calib->corr_ofdm_mrc_x4, 1, 210);

	} else if (fa < 5 * rxena) {
		/* low false alarm count, increase sensitivity */
		DPRINTFN(2, ("OFDM low false alarm count: %u\n", fa));
		dec_clip(calib->corr_ofdm_x1,     1, 105);
		dec_clip(calib->corr_ofdm_mrc_x1, 1, 220);
		dec_clip(calib->corr_ofdm_x4,     1,  85);
		dec_clip(calib->corr_ofdm_mrc_x4, 1, 170);
	}

	/* compute maximum noise among 3 antennas */
	for (i = 0; i < 3; i++)
		noise[i] = (letoh32(stats->general.noise[i]) >> 8) & 0xff;
	val = max(noise[0], noise[1]);
	val = max(noise[2], val);
	/* insert it into our samples table */
	calib->noise_samples[calib->cur_noise_sample] = val;
	calib->cur_noise_sample = (calib->cur_noise_sample + 1) % 20;

	/* compute maximum noise among last 20 samples */
	noise_ref = calib->noise_samples[0];
	for (i = 1; i < 20; i++)
		noise_ref = max(noise_ref, calib->noise_samples[i]);

	/* compute maximum energy among 3 antennas */
	for (i = 0; i < 3; i++)
		energy[i] = letoh32(stats->general.energy[i]);
	val = min(energy[0], energy[1]);
	val = min(energy[2], val);
	/* insert it into our samples table */
	calib->energy_samples[calib->cur_energy_sample] = val;
	calib->cur_energy_sample = (calib->cur_energy_sample + 1) % 10;

	/* compute minimum energy among last 10 samples */
	energy_min = calib->energy_samples[0];
	for (i = 1; i < 10; i++)
		energy_min = max(energy_min, calib->energy_samples[i]);
	energy_min += 6;

	/* compute number of false alarms since last call for CCK */
	fa  = letoh32(stats->cck.bad_plcp) - calib->bad_plcp_cck;
	fa += letoh32(stats->cck.fa) - calib->fa_cck;
	fa *= 200 * 1024;	/* 200TU */

	/* save counters values for next call */
	calib->bad_plcp_cck = letoh32(stats->cck.bad_plcp);
	calib->fa_cck = letoh32(stats->cck.fa);

	if (fa > 50 * rxena) {
		/* high false alarm count, decrease sensitivity */
		DPRINTFN(2, ("CCK high false alarm count: %u\n", fa));
		calib->cck_state = IWN_CCK_STATE_HIFA;
		calib->low_fa = 0;

		if (calib->corr_cck_x4 > 160) {
			calib->noise_ref = noise_ref;
			if (calib->energy_cck > 2)
				dec_clip(calib->energy_cck, 2, energy_min);
		}
		if (calib->corr_cck_x4 < 160) {
			calib->corr_cck_x4 = 161;
			needs_update = 1;
		} else
			inc_clip(calib->corr_cck_x4, 3, 200);

		inc_clip(calib->corr_cck_mrc_x4, 3, 400);

	} else if (fa < 5 * rxena) {
		/* low false alarm count, increase sensitivity */
		DPRINTFN(2, ("CCK low false alarm count: %u\n", fa));
		calib->cck_state = IWN_CCK_STATE_LOFA;
		calib->low_fa++;

		if (calib->cck_state != 0 &&
		    ((calib->noise_ref - noise_ref) > 2 ||
		     calib->low_fa > 100)) {
			inc_clip(calib->energy_cck,      2,  97);
			dec_clip(calib->corr_cck_x4,     3, 125);
			dec_clip(calib->corr_cck_mrc_x4, 3, 200);
		}
	} else {
		/* not worth to increase or decrease sensitivity */
		DPRINTFN(2, ("CCK normal false alarm count: %u\n", fa));
		calib->low_fa = 0;
		calib->noise_ref = noise_ref;

		if (calib->cck_state == IWN_CCK_STATE_HIFA) {
			/* previous interval had many false alarms */
			dec_clip(calib->energy_cck, 8, energy_min);
		}
		calib->cck_state = IWN_CCK_STATE_INIT;
	}

	if (needs_update)
		(void)iwn_send_sensitivity(sc);
#undef dec_clip
#undef inc_clip
}

int
iwn_send_sensitivity(struct iwn_softc *sc)
{
	struct iwn_calib_state *calib = &sc->calib;
	struct iwn_sensitivity_cmd cmd;

	memset(&cmd, 0, sizeof cmd);
	cmd.which = IWN_SENSITIVITY_WORKTBL;
	/* OFDM modulation */
	cmd.corr_ofdm_x1     = letoh16(calib->corr_ofdm_x1);
	cmd.corr_ofdm_mrc_x1 = letoh16(calib->corr_ofdm_mrc_x1);
	cmd.corr_ofdm_x4     = letoh16(calib->corr_ofdm_x4);
	cmd.corr_ofdm_mrc_x4 = letoh16(calib->corr_ofdm_mrc_x4);
	cmd.energy_ofdm      = letoh16(100);
	cmd.energy_ofdm_th   = letoh16(62);
	/* CCK modulation */
	cmd.corr_cck_x4      = letoh16(calib->corr_cck_x4);
	cmd.corr_cck_mrc_x4  = letoh16(calib->corr_cck_mrc_x4);
	cmd.energy_cck       = letoh16(calib->energy_cck);
	/* Barker modulation: use default values */
	cmd.corr_barker      = letoh16(190);
	cmd.corr_barker_mrc  = letoh16(390);

	DPRINTFN(2, ("setting sensitivity %d/%d/%d/%d/%d/%d/%d\n",
	    calib->corr_ofdm_x1, calib->corr_ofdm_mrc_x1, calib->corr_ofdm_x4,
	    calib->corr_ofdm_mrc_x4, calib->corr_cck_x4,
	    calib->corr_cck_mrc_x4, calib->energy_cck));
	return iwn_cmd(sc, IWN_SENSITIVITY, &cmd, sizeof cmd, 1);
}

int
iwn_auth(struct iwn_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni = ic->ic_bss;
	struct iwn_node_info node;
	int error;

	/* update adapter's configuration */
	IEEE80211_ADDR_COPY(sc->config.bssid, ni->ni_bssid);
	sc->config.chan = ieee80211_chan2ieee(ic, ni->ni_chan);
	sc->config.flags = htole32(IWN_CONFIG_TSF);
	if (IEEE80211_IS_CHAN_2GHZ(ni->ni_chan)) {
		sc->config.flags |= htole32(IWN_CONFIG_AUTO |
		    IWN_CONFIG_24GHZ);
	}
	switch (ic->ic_curmode) {
	case IEEE80211_MODE_11A:
		sc->config.cck_mask  = 0;
		sc->config.ofdm_mask = 0x15;
		break;
	case IEEE80211_MODE_11B:
		sc->config.cck_mask  = 0x03;
		sc->config.ofdm_mask = 0;
		break;
	default:	/* assume 802.11b/g */
		sc->config.cck_mask  = 0x0f;
		sc->config.ofdm_mask = 0x15;
	}
	if (ic->ic_flags & IEEE80211_F_SHSLOT)
		sc->config.flags |= htole32(IWN_CONFIG_SHSLOT);
	if (ic->ic_flags & IEEE80211_F_SHPREAMBLE)
		sc->config.flags |= htole32(IWN_CONFIG_SHPREAMBLE);
	DPRINTF(("config chan %d flags %x cck %x ofdm %x\n", sc->config.chan,
	    sc->config.flags, sc->config.cck_mask, sc->config.ofdm_mask));
	error = iwn_cmd(sc, IWN_CMD_CONFIGURE, &sc->config,
	    sizeof (struct iwn_config), 1);
	if (error != 0) {
		printf("%s: could not configure\n", sc->sc_dev.dv_xname);
		return error;
	}

	/* configuration has changed, set Tx power accordingly */
	if ((error = iwn_set_txpower(sc, ni->ni_chan, 1)) != 0) {
		printf("%s: could not set Tx power\n", sc->sc_dev.dv_xname);
		return error;
	}

	/*
	 * Reconfiguring clears the adapter's nodes table so we must
	 * add the broadcast node again.
	 */
	memset(&node, 0, sizeof node);
	IEEE80211_ADDR_COPY(node.macaddr, etherbroadcastaddr);
	node.id = IWN_ID_BROADCAST;
	DPRINTF(("adding broadcast node\n"));
	error = iwn_cmd(sc, IWN_CMD_ADD_NODE, &node, sizeof node, 1);
	if (error != 0) {
		printf("%s: could not add broadcast node\n",
		    sc->sc_dev.dv_xname);
		return error;
	}
	DPRINTF(("setting MRR for node %d\n", node.id));
	if ((error = iwn_setup_node_mrr(sc, node.id, 1)) != 0) {
		printf("%s: could not setup MRR for broadcast node\n",
		    sc->sc_dev.dv_xname, node.id);
		return error;
	}

	return 0;
}

/*
 * Configure the adapter for associated state.
 */
int
iwn_run(struct iwn_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni = ic->ic_bss;
	struct iwn_node_info node;
	int error;

	if (ic->ic_opmode == IEEE80211_M_MONITOR) {
		/* link LED blinks while monitoring */
		iwn_set_led(sc, IWN_LED_LINK, 5, 5);
		return 0;
	}

	iwn_enable_tsf(sc, ni);

	/* update adapter's configuration */
	sc->config.associd = htole16(ni->ni_associd & ~0xc000);
	/* short preamble/slot time are negotiated when associating */
	sc->config.flags &= ~htole32(IWN_CONFIG_SHPREAMBLE |
	    IWN_CONFIG_SHSLOT);
	if (ic->ic_flags & IEEE80211_F_SHSLOT)
		sc->config.flags |= htole32(IWN_CONFIG_SHSLOT);
	if (ic->ic_flags & IEEE80211_F_SHPREAMBLE)
		sc->config.flags |= htole32(IWN_CONFIG_SHPREAMBLE);
	sc->config.filter |= htole32(IWN_FILTER_BSS);

	DPRINTF(("config chan %d flags %x\n", sc->config.chan,
	    sc->config.flags));
	error = iwn_cmd(sc, IWN_CMD_CONFIGURE, &sc->config,
	    sizeof (struct iwn_config), 1);
	if (error != 0) {
		printf("%s: could not update configuration\n",
		    sc->sc_dev.dv_xname);
		return error;
	}

	/* configuration has changed, set Tx power accordingly */
	if ((error = iwn_set_txpower(sc, ni->ni_chan, 1)) != 0) {
		printf("%s: could not set Tx power\n",
		    sc->sc_dev.dv_xname);
		return error;
	}

	/* add BSS node */
	memset(&node, 0, sizeof node);
	IEEE80211_ADDR_COPY(node.macaddr, ni->ni_macaddr);
	node.id = IWN_ID_BSS;
	node.htflags = htole32(3 << IWN_AMDPU_SIZE_FACTOR_SHIFT |
	    5 << IWN_AMDPU_DENSITY_SHIFT);
	DPRINTF(("adding BSS node\n"));
	error = iwn_cmd(sc, IWN_CMD_ADD_NODE, &node, sizeof node, 1);
	if (error != 0) {
		printf("%s: could not add BSS node\n", sc->sc_dev.dv_xname);
		return error;
	}
	DPRINTF(("setting MRR for node %d\n", node.id));
	if ((error = iwn_setup_node_mrr(sc, node.id, 1)) != 0) {
		printf("%s: could not setup MRR for node %d\n",
		    sc->sc_dev.dv_xname, node.id);
		return error;
	}

	if (ic->ic_opmode == IEEE80211_M_STA) {
		/* fake a join to init the tx rate */
		iwn_newassoc(ic, ni, 1);
	}

	if ((error = iwn_init_sensitivity(sc)) != 0) {
		printf("%s: could not set sensitivity\n",
		    sc->sc_dev.dv_xname);
		return error;
	}

	/* start periodic calibration timer */
	sc->calib.state = IWN_CALIB_STATE_ASSOC;
	sc->calib_cnt = 0;
	timeout_add(&sc->calib_to, hz / 2);

	/* link LED always on while associated */
	iwn_set_led(sc, IWN_LED_LINK, 0, 1);

	return 0;
}

/*
 * Send a scan request to the firmware.  Since this command is huge, we map it
 * into a mbuf instead of using the pre-allocated set of commands.
 */
int
iwn_scan(struct iwn_softc *sc, uint16_t flags)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct iwn_tx_ring *ring = &sc->txq[4];
	struct iwn_tx_desc *desc;
	struct iwn_tx_data *data;
	struct iwn_tx_cmd *cmd;
	struct iwn_cmd_data *tx;
	struct iwn_scan_hdr *hdr;
	struct iwn_scan_essid *essid;
	struct iwn_scan_chan *chan;
	struct ieee80211_frame *wh;
	struct ieee80211_rateset *rs;
	struct ieee80211_channel *c;
	enum ieee80211_phymode mode;
	uint8_t *frm;
	int pktlen, error;

	desc = &ring->desc[ring->cur];
	data = &ring->data[ring->cur];

	MGETHDR(data->m, M_DONTWAIT, MT_DATA);
	if (data->m == NULL) {
		printf("%s: could not allocate mbuf for scan command\n",
		    sc->sc_dev.dv_xname);
		return ENOMEM;
	}
	MCLGET(data->m, M_DONTWAIT);
	if (!(data->m->m_flags & M_EXT)) {
		m_freem(data->m);
		data->m = NULL;
		printf("%s: could not allocate mbuf for scan command\n",
		    sc->sc_dev.dv_xname);
		return ENOMEM;
	}

	cmd = mtod(data->m, struct iwn_tx_cmd *);
	cmd->code = IWN_CMD_SCAN;
	cmd->flags = 0;
	cmd->qid = ring->qid;
	cmd->idx = ring->cur;

	hdr = (struct iwn_scan_hdr *)cmd->data;
	memset(hdr, 0, sizeof (struct iwn_scan_hdr));
	/*
	 * Move to the next channel if no packets are received within 5 msecs
	 * after sending the probe request (this helps to reduce the duration
	 * of active scans).
	 */
	hdr->quiet = htole16(5);	/* timeout in milliseconds */
	hdr->plcp_threshold = htole16(1);	/* min # of packets */

	/* select Ant B and Ant C for scanning */
	hdr->rxchain = htole16(0x3e1 | 7 << IWN_RXCHAIN_ANTMSK_SHIFT);

	tx = (struct iwn_cmd_data *)(hdr + 1);
	memset(tx, 0, sizeof (struct iwn_cmd_data));
	tx->flags = htole32(IWN_TX_AUTO_SEQ);
	tx->id = IWN_ID_BROADCAST;
	tx->lifetime = htole32(IWN_LIFETIME_INFINITE);
	tx->rflags = IWN_RFLAG_ANT_B;

	if (flags & IEEE80211_CHAN_A) {
		hdr->crc_threshold = htole16(1);
		/* send probe requests at 6Mbps */
		tx->rate = iwn_ridx_to_plcp[IWN_OFDM6];
	} else {
		hdr->flags = htole32(IWN_CONFIG_24GHZ | IWN_CONFIG_AUTO);
		/* send probe requests at 1Mbps */
		tx->rate = iwn_ridx_to_plcp[IWN_CCK1];
		tx->rflags |= IWN_RFLAG_CCK;
	}

	essid = (struct iwn_scan_essid *)(tx + 1);
	memset(essid, 0, 4 * sizeof (struct iwn_scan_essid));
	essid[0].id  = IEEE80211_ELEMID_SSID;
	essid[0].len = ic->ic_des_esslen;
	memcpy(essid[0].data, ic->ic_des_essid, ic->ic_des_esslen);

	/*
	 * Build a probe request frame.  Most of the following code is a
	 * copy & paste of what is done in net80211.
	 */
	wh = (struct ieee80211_frame *)&essid[4];
	wh->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_MGT |
	    IEEE80211_FC0_SUBTYPE_PROBE_REQ;
	wh->i_fc[1] = IEEE80211_FC1_DIR_NODS;
	IEEE80211_ADDR_COPY(wh->i_addr1, etherbroadcastaddr);
	IEEE80211_ADDR_COPY(wh->i_addr2, ic->ic_myaddr);
	IEEE80211_ADDR_COPY(wh->i_addr3, etherbroadcastaddr);
	*(u_int16_t *)&wh->i_dur[0] = 0;	/* filled by h/w */
	*(u_int16_t *)&wh->i_seq[0] = 0;	/* filled by h/w */

	frm = (uint8_t *)(wh + 1);

	/* add empty SSID IE (firmware generates it for directed scans) */
	frm = ieee80211_add_ssid(frm, NULL, 0);

	mode = ieee80211_chan2mode(ic, ic->ic_ibss_chan);
	rs = &ic->ic_sup_rates[mode];

	/* add supported rates IE */
	frm = ieee80211_add_rates(frm, rs);

	/* add supported xrates IE */
	if (rs->rs_nrates > IEEE80211_RATE_SIZE)
		frm = ieee80211_add_xrates(frm, rs);

	/* setup length of probe request */
	tx->len = htole16(frm - (uint8_t *)wh);

	chan = (struct iwn_scan_chan *)frm;
	for (c  = &ic->ic_channels[1];
	     c <= &ic->ic_channels[IEEE80211_CHAN_MAX]; c++) {
		if ((c->ic_flags & flags) != flags)
			continue;

		chan->chan = ieee80211_chan2ieee(ic, c);
		chan->flags = 0;
		if (!(c->ic_flags & IEEE80211_CHAN_PASSIVE)) {
			chan->flags |= IWN_CHAN_ACTIVE;
			if (ic->ic_des_esslen != 0)
				chan->flags |= IWN_CHAN_DIRECT;
		}
		chan->dsp_gain = 0x6e;
		if (IEEE80211_IS_CHAN_5GHZ(c)) {
			chan->rf_gain = 0x3b;
			chan->active  = htole16(10);
			chan->passive = htole16(110);
		} else {
			chan->rf_gain = 0x28;
			chan->active  = htole16(20);
			chan->passive = htole16(120);
		}
		hdr->nchan++;
		chan++;

		frm += sizeof (struct iwn_scan_chan);
	}

	hdr->len = htole16(frm - (uint8_t *)hdr);
	pktlen = frm - (uint8_t *)cmd;

	error = bus_dmamap_load(sc->sc_dmat, data->map, cmd, pktlen, NULL,
	    BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not map scan command\n",
		    sc->sc_dev.dv_xname);
		m_freem(data->m);
		data->m = NULL;
		return error;
	}

	IWN_SET_DESC_NSEGS(desc, 1);
	IWN_SET_DESC_SEG(desc, 0, data->map->dm_segs[0].ds_addr,
	    data->map->dm_segs[0].ds_len);
	sc->shared->len[ring->qid][ring->cur] = htole16(8);

	/* kick cmd ring */
	ring->cur = (ring->cur + 1) % IWN_TX_RING_COUNT;
	IWN_WRITE(sc, IWN_TX_WIDX, ring->qid << 8 | ring->cur);

	return 0;	/* will be notified async. of failure/success */
}

int
iwn_config(struct iwn_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct iwn_power power;
	struct iwn_bluetooth bluetooth;
	struct iwn_node_info node;
	int error;

	/* set power mode */
	memset(&power, 0, sizeof power);
	power.flags = htole16(IWN_POWER_CAM | 0x8);
	DPRINTF(("setting power mode\n"));
	error = iwn_cmd(sc, IWN_CMD_SET_POWER_MODE, &power, sizeof power, 0);
	if (error != 0) {
		printf("%s: could not set power mode\n", sc->sc_dev.dv_xname);
		return error;
	}

	/* configure bluetooth coexistence */
	memset(&bluetooth, 0, sizeof bluetooth);
	bluetooth.flags = 3;
	bluetooth.lead = 0xaa;
	bluetooth.kill = 1;
	DPRINTF(("configuring bluetooth coexistence\n"));
	error = iwn_cmd(sc, IWN_CMD_BLUETOOTH, &bluetooth, sizeof bluetooth,
	    0);
	if (error != 0) {
		printf("%s: could not configure bluetooth coexistence\n",
		    sc->sc_dev.dv_xname);
		return error;
	}

	/* configure adapter */
	memset(&sc->config, 0, sizeof (struct iwn_config));
	IEEE80211_ADDR_COPY(ic->ic_myaddr, LLADDR(ifp->if_sadl));
	IEEE80211_ADDR_COPY(sc->config.myaddr, ic->ic_myaddr);
	IEEE80211_ADDR_COPY(sc->config.wlap, ic->ic_myaddr);
	/* set default channel */
	sc->config.chan = ieee80211_chan2ieee(ic, ic->ic_ibss_chan);
	sc->config.flags = htole32(IWN_CONFIG_TSF);
	if (IEEE80211_IS_CHAN_2GHZ(ic->ic_ibss_chan)) {
		sc->config.flags |= htole32(IWN_CONFIG_AUTO |
		    IWN_CONFIG_24GHZ);
	}
	sc->config.filter = 0;
	switch (ic->ic_opmode) {
	case IEEE80211_M_STA:
		sc->config.mode = IWN_MODE_STA;
		sc->config.filter |= htole32(IWN_FILTER_MULTICAST);
		break;
	case IEEE80211_M_IBSS:
	case IEEE80211_M_AHDEMO:
		sc->config.mode = IWN_MODE_IBSS;
		break;
	case IEEE80211_M_HOSTAP:
		sc->config.mode = IWN_MODE_HOSTAP;
		break;
	case IEEE80211_M_MONITOR:
		sc->config.mode = IWN_MODE_MONITOR;
		sc->config.filter |= htole32(IWN_FILTER_MULTICAST |
		    IWN_FILTER_CTL | IWN_FILTER_PROMISC);
		break;
	}
	sc->config.cck_mask  = 0x0f;	/* not yet negotiated */
	sc->config.ofdm_mask = 0xff;	/* not yet negotiated */
	sc->config.ht_single_mask = 0xff;
	sc->config.ht_dual_mask = 0xff;
	sc->config.rxchain = htole16(0x2800 | 7 << IWN_RXCHAIN_ANTMSK_SHIFT);
	DPRINTF(("setting configuration\n"));
	error = iwn_cmd(sc, IWN_CMD_CONFIGURE, &sc->config,
	    sizeof (struct iwn_config), 0);
	if (error != 0) {
		printf("%s: configure command failed\n", sc->sc_dev.dv_xname);
		return error;
	}

	/* configuration has changed, set Tx power accordingly */
	if ((error = iwn_set_txpower(sc, ic->ic_ibss_chan, 0)) != 0) {
		printf("%s: could not set Tx power\n", sc->sc_dev.dv_xname);
		return error;
	}

	/* add broadcast node */
	memset(&node, 0, sizeof node);
	IEEE80211_ADDR_COPY(node.macaddr, etherbroadcastaddr);
	node.id = IWN_ID_BROADCAST;
	DPRINTF(("adding broadcast node\n"));
	error = iwn_cmd(sc, IWN_CMD_ADD_NODE, &node, sizeof node, 0);
	if (error != 0) {
		printf("%s: could not add broadcast node\n",
		    sc->sc_dev.dv_xname);
		return error;
	}
	DPRINTF(("setting MRR for node %d\n", node.id));
	if ((error = iwn_setup_node_mrr(sc, node.id, 0)) != 0) {
		printf("%s: could not setup MRR for node %d\n",
		    sc->sc_dev.dv_xname, node.id);
		return error;
	}

	if ((error = iwn_set_critical_temp(sc)) != 0) {
		printf("%s: could not set critical temperature\n",
		    sc->sc_dev.dv_xname);
		return error;
	}

	return 0;
}

/*
 * Do post-alive initialization of the NIC (after firmware upload).
 */
void
iwn_post_alive(struct iwn_softc *sc)
{
	uint32_t base;
	uint16_t offset;
	int qid;

	iwn_mem_lock(sc);

	/* clear SRAM */
	base = iwn_mem_read(sc, IWN_SRAM_BASE);
	for (offset = 0x380; offset < 0x520; offset += 4) {
		IWN_WRITE(sc, IWN_MEM_WADDR, base + offset);
		IWN_WRITE(sc, IWN_MEM_WDATA, 0);
	}

	/* shared area is aligned on a 1K boundary */
	iwn_mem_write(sc, IWN_SRAM_BASE, sc->shared_dma.paddr >> 10);
	iwn_mem_write(sc, IWN_SELECT_QCHAIN, 0);

	for (qid = 0; qid < IWN_NTXQUEUES; qid++) {
		iwn_mem_write(sc, IWN_QUEUE_RIDX(qid), 0);
		IWN_WRITE(sc, IWN_TX_WIDX, qid << 8 | 0);

		/* set sched. window size */
		IWN_WRITE(sc, IWN_MEM_WADDR, base + IWN_QUEUE_OFFSET(qid));
		IWN_WRITE(sc, IWN_MEM_WDATA, 64);
		/* set sched. frame limit */
		IWN_WRITE(sc, IWN_MEM_WADDR, base + IWN_QUEUE_OFFSET(qid) + 4);
		IWN_WRITE(sc, IWN_MEM_WDATA, 10 << 16);
	}

	/* enable interrupts for all 16 queues */
	iwn_mem_write(sc, IWN_QUEUE_INTR_MASK, 0xffff);

	/* identify active Tx rings (0-7) */
	iwn_mem_write(sc, IWN_TX_ACTIVE, 0xff);

	/* mark Tx rings (4 EDCA + cmd + 2 HCCA) as active */
	for (qid = 0; qid < 7; qid++) {
		iwn_mem_write(sc, IWN_TXQ_STATUS(qid),
		    IWN_TXQ_STATUS_ACTIVE | qid << 1);
	}

	iwn_mem_unlock(sc);
}

void
iwn_stop_master(struct iwn_softc *sc)
{
	uint32_t tmp;
	int ntries;

	tmp = IWN_READ(sc, IWN_RESET);
	IWN_WRITE(sc, IWN_RESET, tmp | IWN_STOP_MASTER);

	tmp = IWN_READ(sc, IWN_GPIO_CTL);
	if ((tmp & IWN_GPIO_PWR_STATUS) == IWN_GPIO_PWR_SLEEP)
		return;	/* already asleep */

	for (ntries = 0; ntries < 100; ntries++) {
		if (IWN_READ(sc, IWN_RESET) & IWN_MASTER_DISABLED)
			break;
		DELAY(10);
	}
	if (ntries == 100) {
		printf("%s: timeout waiting for master\n",
		    sc->sc_dev.dv_xname);
	}
}

int
iwn_reset(struct iwn_softc *sc)
{
	uint32_t tmp;
	int ntries;

	/* clear any pending interrupts */
	IWN_WRITE(sc, IWN_INTR, 0xffffffff);

	tmp = IWN_READ(sc, IWN_CHICKEN);
	IWN_WRITE(sc, IWN_CHICKEN, tmp | IWN_CHICKEN_DISLOS);

	tmp = IWN_READ(sc, IWN_GPIO_CTL);
	IWN_WRITE(sc, IWN_GPIO_CTL, tmp | IWN_GPIO_INIT);

	/* wait for clock stabilization */
	for (ntries = 0; ntries < 1000; ntries++) {
		if (IWN_READ(sc, IWN_GPIO_CTL) & IWN_GPIO_CLOCK)
			break;
		DELAY(10);
	}
	if (ntries == 1000) {
		printf("%s: timeout waiting for clock stabilization\n",
		    sc->sc_dev.dv_xname);
		return ETIMEDOUT;
	}
	return 0;
}

void
iwn_hw_config(struct iwn_softc *sc)
{
	uint32_t tmp, hw;

	/* enable interrupts mitigation */
	IWN_WRITE(sc, IWN_INTR_MIT, 512 / 32);

	/* voodoo from the reference driver */
	tmp = pci_conf_read(sc->sc_pct, sc->sc_pcitag, PCI_CLASS_REG);
	tmp = PCI_REVISION(tmp);
	if ((tmp & 0x80) && (tmp & 0x7f) < 8) {
		/* enable "no snoop" field */
		tmp = pci_conf_read(sc->sc_pct, sc->sc_pcitag, 0xe8);
		tmp &= ~IWN_DIS_NOSNOOP;
		pci_conf_write(sc->sc_pct, sc->sc_pcitag, 0xe8, tmp);
	}

	/* disable L1 entry to work around a hardware bug */
	tmp = pci_conf_read(sc->sc_pct, sc->sc_pcitag, 0xf0);
	tmp &= ~IWN_ENA_L1;
	pci_conf_write(sc->sc_pct, sc->sc_pcitag, 0xf0, tmp);

	hw = IWN_READ(sc, IWN_HWCONFIG);
	IWN_WRITE(sc, IWN_HWCONFIG, hw | 0x310);

	iwn_mem_lock(sc);
	tmp = iwn_mem_read(sc, IWN_MEM_POWER);
	iwn_mem_write(sc, IWN_MEM_POWER, tmp | IWN_POWER_RESET);
	DELAY(5);
	tmp = iwn_mem_read(sc, IWN_MEM_POWER);
	iwn_mem_write(sc, IWN_MEM_POWER, tmp & ~IWN_POWER_RESET);
	iwn_mem_unlock(sc);
}

int
iwn_init(struct ifnet *ifp)
{
	struct iwn_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	uint32_t tmp;
	int error, qid;

	if ((error = iwn_reset(sc)) != 0) {
		printf("%s: could not reset adapter\n", sc->sc_dev.dv_xname);
		goto fail1;
	}

	iwn_mem_lock(sc);
	iwn_mem_read(sc, IWN_CLOCK_CTL);
	iwn_mem_write(sc, IWN_CLOCK_CTL, 0xa00);
	iwn_mem_read(sc, IWN_CLOCK_CTL);
	iwn_mem_unlock(sc);

	DELAY(20);

	iwn_mem_lock(sc);
	tmp = iwn_mem_read(sc, IWN_MEM_PCIDEV);
	iwn_mem_write(sc, IWN_MEM_PCIDEV, tmp | 0x800);
	iwn_mem_unlock(sc);

	iwn_mem_lock(sc);
	tmp = iwn_mem_read(sc, IWN_MEM_POWER);
	iwn_mem_write(sc, IWN_MEM_POWER, tmp & ~0x03000000);
	iwn_mem_unlock(sc);

	iwn_hw_config(sc);

	/* init Rx ring */
	iwn_mem_lock(sc);
	IWN_WRITE(sc, IWN_RX_CONFIG, 0);
	IWN_WRITE(sc, IWN_RX_WIDX, 0);
	/* Rx ring is aligned on a 256-byte boundary */
	IWN_WRITE(sc, IWN_RX_BASE, sc->rxq.desc_dma.paddr >> 8);
	/* shared area is aligned on a 16-byte boundary */
	IWN_WRITE(sc, IWN_RW_WIDX_PTR, (sc->shared_dma.paddr +
	    offsetof(struct iwn_shared, closed_count)) >> 4);
	IWN_WRITE(sc, IWN_RX_CONFIG, 0x80601000);
	iwn_mem_unlock(sc);

	IWN_WRITE(sc, IWN_RX_WIDX, (IWN_RX_RING_COUNT - 1) & ~7);

	iwn_mem_lock(sc);
	iwn_mem_write(sc, IWN_TX_ACTIVE, 0);

	/* set physical address of "keep warm" page */
	IWN_WRITE(sc, IWN_KW_BASE, sc->kw_dma.paddr >> 4);

	/* init Tx rings */
	for (qid = 0; qid < IWN_NTXQUEUES; qid++) {
		struct iwn_tx_ring *txq = &sc->txq[qid];
		IWN_WRITE(sc, IWN_TX_BASE(qid), txq->desc_dma.paddr >> 8);
		IWN_WRITE(sc, IWN_TX_CONFIG(qid), 0x80000008);
	}
	iwn_mem_unlock(sc);

	/* clear "radio off" and "disable command" bits (reversed logic) */
	IWN_WRITE(sc, IWN_UCODE_CLR, IWN_RADIO_OFF);
	IWN_WRITE(sc, IWN_UCODE_CLR, IWN_DISABLE_CMD);

	/* clear any pending interrupts */
	IWN_WRITE(sc, IWN_INTR, 0xffffffff);
	/* enable interrupts */
	IWN_WRITE(sc, IWN_MASK, IWN_INTR_MASK);

	/* not sure why/if this is necessary... */
	IWN_WRITE(sc, IWN_UCODE_CLR, IWN_RADIO_OFF);
	IWN_WRITE(sc, IWN_UCODE_CLR, IWN_RADIO_OFF);

	/* check that the radio is not disabled by RF switch */
	if (!(IWN_READ(sc, IWN_GPIO_CTL) & IWN_GPIO_RF_ENABLED)) {
		printf("%s: radio is disabled by hardware switch\n",
		    sc->sc_dev.dv_xname);
		error = EPERM;	/* XXX ;-) */
		goto fail1;
	}

	if ((error = iwn_load_firmware(sc)) != 0) {
		printf("%s: could not load firmware\n", sc->sc_dev.dv_xname);
		goto fail1;
	}

	/* firmware has notified us that it is alive.. */
	iwn_post_alive(sc);	/* ..do post alive initialization */

	sc->rawtemp = sc->ucode_info.temp[3].chan20MHz;
	sc->temp = iwn_get_temperature(sc);
	DPRINTF(("temperature=%d\n", sc->temp));
	sc->sensor.value = IWN_CTOMUK(sc->temp);
	sc->sensor.flags &= ~SENSOR_FINVALID;

	if ((error = iwn_config(sc)) != 0) {
		printf("%s: could not configure device\n",
		    sc->sc_dev.dv_xname);
		goto fail1;
	}

	ifp->if_flags &= ~IFF_OACTIVE;
	ifp->if_flags |= IFF_RUNNING;

	if (ic->ic_opmode != IEEE80211_M_MONITOR)
		ieee80211_begin_scan(ifp);
	else
		ieee80211_new_state(ic, IEEE80211_S_RUN, -1);

	return 0;

fail1:	iwn_stop(ifp, 1);
	return error;
}

void
iwn_stop(struct ifnet *ifp, int disable)
{
	struct iwn_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	uint32_t tmp;
	int i;

	ifp->if_timer = sc->sc_tx_timer = 0;
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	ieee80211_new_state(ic, IEEE80211_S_INIT, -1);

	IWN_WRITE(sc, IWN_RESET, IWN_NEVO_RESET);

	/* disable interrupts */
	IWN_WRITE(sc, IWN_MASK, 0);
	IWN_WRITE(sc, IWN_INTR, 0xffffffff);
	IWN_WRITE(sc, IWN_INTR_STATUS, 0xffffffff);

	/* make sure we no longer hold the memory lock */
	iwn_mem_unlock(sc);

	/* reset all Tx rings */
	for (i = 0; i < IWN_NTXQUEUES; i++)
		iwn_reset_tx_ring(sc, &sc->txq[i]);

	/* reset Rx ring */
	iwn_reset_rx_ring(sc, &sc->rxq);

	/* temperature is no longer valid */
	sc->sensor.value = 0;
	sc->sensor.flags |= SENSOR_FINVALID;

	iwn_mem_lock(sc);
	iwn_mem_write(sc, IWN_MEM_CLOCK2, 0x200);
	iwn_mem_unlock(sc);

	DELAY(5);

	iwn_stop_master(sc);
	tmp = IWN_READ(sc, IWN_RESET);
	IWN_WRITE(sc, IWN_RESET, tmp | IWN_SW_RESET);
}

struct cfdriver iwn_cd = {
	NULL, "iwn", DV_IFNET
};
