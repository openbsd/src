/*	$OpenBSD: if_wpi.c,v 1.14 2006/05/29 20:26:54 miod Exp $	*/

/*-
 * Copyright (c) 2006
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
 * Driver for Intel PRO/Wireless 3945ABG 802.11 network adapters.
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
#include <net80211/ieee80211_radiotap.h>

#include <dev/pci/if_wpireg.h>
#include <dev/pci/if_wpivar.h>

const struct pci_matchid wpi_devices[] = {
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_PRO_WL_3945ABG_1 },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_PRO_WL_3945ABG_2 }
};

/*
 * Supported rates for 802.11a/b/g modes (in 500Kbps unit).
 */
#ifdef notyet
static const struct ieee80211_rateset wpi_rateset_11a =
	{ 8, { 12, 18, 24, 36, 48, 72, 96, 108 } };
#endif
static const struct ieee80211_rateset wpi_rateset_11b =
	{ 4, { 2, 4, 11, 22 } };

static const struct ieee80211_rateset wpi_rateset_11g =
	{ 12, { 2, 4, 11, 22, 12, 18, 24, 36, 48, 72, 96, 108 } };

static const uint8_t wpi_ridx_to_plcp[] = {
	0xd, 0xf, 0x5, 0x7, 0x9, 0xb, 0x1, 0x3,	/* OFDM R1-R4 */
	10, 20, 55, 110	/* CCK */
};

int		wpi_match(struct device *, void *, void *);
void		wpi_attach(struct device *, struct device *, void *);
int		wpi_detach(struct device *, int);
void		wpi_power(int, void *);
int		wpi_dma_contig_alloc(struct wpi_softc *, struct wpi_dma_info *,
		    void **, bus_size_t, bus_size_t, int);
void		wpi_dma_contig_free(struct wpi_softc *, struct wpi_dma_info *);
int		wpi_alloc_shared(struct wpi_softc *);
void		wpi_free_shared(struct wpi_softc *);
int		wpi_alloc_rx_ring(struct wpi_softc *, struct wpi_rx_ring *);
void		wpi_reset_rx_ring(struct wpi_softc *, struct wpi_rx_ring *);
void		wpi_free_rx_ring(struct wpi_softc *, struct wpi_rx_ring *);
int		wpi_alloc_tx_ring(struct wpi_softc *, struct wpi_tx_ring *,
		    int, int);
void		wpi_reset_tx_ring(struct wpi_softc *, struct wpi_tx_ring *);
void		wpi_free_tx_ring(struct wpi_softc *, struct wpi_tx_ring *);
struct		ieee80211_node *wpi_node_alloc(struct ieee80211com *);
void		wpi_node_copy(struct ieee80211com *, struct ieee80211_node *,
		    const struct ieee80211_node *);
int		wpi_media_change(struct ifnet *);
int		wpi_newstate(struct ieee80211com *, enum ieee80211_state, int);
void		wpi_mem_lock(struct wpi_softc *);
void		wpi_mem_unlock(struct wpi_softc *);
uint32_t	wpi_mem_read(struct wpi_softc *, uint16_t);
void		wpi_mem_write(struct wpi_softc *, uint16_t, uint32_t);
void		wpi_mem_write_region_4(struct wpi_softc *, uint16_t,
		    const uint32_t *, int);
uint16_t	wpi_read_prom_word(struct wpi_softc *, uint32_t);
int		wpi_load_microcode(struct wpi_softc *, const char *, int);
int		wpi_load_firmware(struct wpi_softc *, uint32_t, const char *,
		    int);
void		wpi_rx_intr(struct wpi_softc *, struct wpi_rx_desc *,
		    struct wpi_rx_data *);
void		wpi_tx_intr(struct wpi_softc *, struct wpi_rx_desc *,
		    struct wpi_rx_data *);
void		wpi_cmd_intr(struct wpi_softc *, struct wpi_rx_desc *);
void		wpi_notif_intr(struct wpi_softc *);
int		wpi_intr(void *);
void		wpi_read_eeprom(struct wpi_softc *);
uint8_t		wpi_plcp_signal(int);
int		wpi_tx_data(struct wpi_softc *, struct mbuf *,
		    struct ieee80211_node *, int);
void		wpi_start(struct ifnet *);
void		wpi_watchdog(struct ifnet *);
int		wpi_ioctl(struct ifnet *, u_long, caddr_t);
int		wpi_cmd(struct wpi_softc *, int, const void *, int, int);
int		wpi_mrr_setup(struct wpi_softc *);
void		wpi_set_led(struct wpi_softc *, uint8_t, uint8_t, uint8_t);
void		wpi_enable_tsf(struct wpi_softc *, struct ieee80211_node *);
int		wpi_setup_beacon(struct wpi_softc *, struct ieee80211_node *);
int		wpi_auth(struct wpi_softc *);
int		wpi_scan(struct wpi_softc *);
int		wpi_config(struct wpi_softc *);
void		wpi_stop_master(struct wpi_softc *);
int		wpi_power_up(struct wpi_softc *);
int		wpi_reset(struct wpi_softc *);
void		wpi_hw_config(struct wpi_softc *);
int		wpi_init(struct ifnet *);
void		wpi_stop(struct ifnet *, int);

/* rate control algorithm: should be moved to net80211 */
void		wpi_amrr_init(struct wpi_amrr *);
void		wpi_amrr_init(struct wpi_amrr *);
void		wpi_amrr_timeout(void *);
void		wpi_amrr_ratectl(void *, struct ieee80211_node *);

#define WPI_DEBUG

#ifdef WPI_DEBUG
#define DPRINTF(x)	do { if (wpi_debug > 0) printf x; } while (0)
#define DPRINTFN(n, x)	do { if (wpi_debug >= (n)) printf x; } while (0)
int wpi_debug = 1;
#else
#define DPRINTF(x)
#define DPRINTFN(n, x)
#endif

struct cfattach wpi_ca = {
	sizeof (struct wpi_softc), wpi_match, wpi_attach, wpi_detach
};

int
wpi_match(struct device *parent, void *match, void *aux)
{
	return pci_matchbyid((struct pci_attach_args *)aux, wpi_devices,
	    sizeof (wpi_devices) / sizeof (wpi_devices[0]));
}

/* Base Address Register */
#define WPI_PCI_BAR0	0x10

void
wpi_attach(struct device *parent, struct device *self, void *aux)
{
	struct wpi_softc *sc = (struct wpi_softc *)self;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct pci_attach_args *pa = aux;
	const char *intrstr;
	bus_space_tag_t memt;
	bus_space_handle_t memh;
	pci_intr_handle_t ih;
	pcireg_t data;
	int i, ac, error;

	sc->sc_pct = pa->pa_pc;
	sc->sc_pcitag = pa->pa_tag;

	/* clear device specific PCI configuration register 0x41 */
	data = pci_conf_read(sc->sc_pct, sc->sc_pcitag, 0x40);
	data &= ~0x0000ff00;
	pci_conf_write(sc->sc_pct, sc->sc_pcitag, 0x40, data);

	/* map the register window */
	error = pci_mapreg_map(pa, WPI_PCI_BAR0, PCI_MAPREG_TYPE_MEM |
	    PCI_MAPREG_MEM_TYPE_32BIT, 0, &memt, &memh, NULL, &sc->sc_sz, 0);
	if (error != 0) {
		printf(": could not map memory space\n");
		return;
	}

	sc->sc_st = memt;
	sc->sc_sh = memh;
	sc->sc_dmat = pa->pa_dmat;

	if (pci_intr_map(pa, &ih) != 0) {
		printf(": could not map interrupt\n");
		return;
	}

	intrstr = pci_intr_string(sc->sc_pct, ih);
	sc->sc_ih = pci_intr_establish(sc->sc_pct, ih, IPL_NET, wpi_intr, sc,
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
	if ((error = wpi_reset(sc)) != 0) {
		printf(": could not reset adapter\n");
		return;
	}

	/*
	 * Allocate shared page and Tx/Rx rings.
	 */
	if ((error = wpi_alloc_shared(sc)) != 0) {
		printf(": could not allocate shared area\n");
		return;
	}

	for (ac = 0; ac < 4; ac++) {
		error = wpi_alloc_tx_ring(sc, &sc->txq[ac], WPI_TX_RING_COUNT,
		    ac);
		if (error != 0) {
			printf(": could not allocate Tx ring %d\n", ac);
			goto fail1;
		}
	}

	error = wpi_alloc_tx_ring(sc, &sc->cmdq, WPI_CMD_RING_COUNT, 4);
	if (error != 0) {
		printf(": could not allocate command ring\n");
		goto fail1;
	}

	error = wpi_alloc_tx_ring(sc, &sc->svcq, WPI_SVC_RING_COUNT, 5);
	if (error != 0) {
		printf(": could not allocate service ring\n");
		goto fail2;
	}

	error = wpi_alloc_rx_ring(sc, &sc->rxq);
	if (error != 0) {
		printf(": could not allocate Rx ring\n");
		goto fail3;
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

	wpi_read_eeprom(sc);
	printf(", address %s\n", ether_sprintf(ic->ic_myaddr));

#ifdef notyet
	/* set supported .11a rates */
	ic->ic_sup_rates[IEEE80211_MODE_11A] = wpi_rateset_11a;

	/* set supported .11a channels */
	for (i = 36; i <= 64; i += 4) {
		ic->ic_channels[i].ic_freq =
		    ieee80211_ieee2mhz(i, IEEE80211_CHAN_5GHZ);
		ic->ic_channels[i].ic_flags = IEEE80211_CHAN_A;
	}
	for (i = 149; i <= 165; i += 4) {
		ic->ic_channels[i].ic_freq =
		    ieee80211_ieee2mhz(i, IEEE80211_CHAN_5GHZ);
		ic->ic_channels[i].ic_flags = IEEE80211_CHAN_A;
	}
#endif

	/* set supported .11b and .11g rates */
	ic->ic_sup_rates[IEEE80211_MODE_11B] = wpi_rateset_11b;
	ic->ic_sup_rates[IEEE80211_MODE_11G] = wpi_rateset_11g;

	/* set supported .11b and .11g channels (1 through 14) */
	for (i = 1; i <= 14; i++) {
		ic->ic_channels[i].ic_freq =
		    ieee80211_ieee2mhz(i, IEEE80211_CHAN_2GHZ);
		ic->ic_channels[i].ic_flags =
		    IEEE80211_CHAN_CCK | IEEE80211_CHAN_OFDM |
		    IEEE80211_CHAN_DYN | IEEE80211_CHAN_2GHZ;
	}

	/* IBSS channel undefined for now */
	ic->ic_ibss_chan = &ic->ic_channels[0];

	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_init = wpi_init;
	ifp->if_ioctl = wpi_ioctl;
	ifp->if_start = wpi_start;
	ifp->if_watchdog = wpi_watchdog;
	IFQ_SET_READY(&ifp->if_snd);
	bcopy(sc->sc_dev.dv_xname, ifp->if_xname, IFNAMSIZ);

	if_attach(ifp);
	ieee80211_ifattach(ifp);
	ic->ic_node_alloc = wpi_node_alloc;
	ic->ic_node_copy = wpi_node_copy;

	/* override state transition machine */
	sc->sc_newstate = ic->ic_newstate;
	ic->ic_newstate = wpi_newstate;
	ieee80211_media_init(ifp, wpi_media_change, ieee80211_media_status);

	timeout_set(&sc->amrr_ch, wpi_amrr_timeout, sc);

	sc->powerhook = powerhook_establish(wpi_power, sc);

#if NBPFILTER > 0
	bpfattach(&sc->sc_drvbpf, ifp, DLT_IEEE802_11_RADIO,
	    sizeof (struct ieee80211_frame) + 64);

	sc->sc_rxtap_len = sizeof sc->sc_rxtapu;
	sc->sc_rxtap.wr_ihdr.it_len = htole16(sc->sc_rxtap_len);
	sc->sc_rxtap.wr_ihdr.it_present = htole32(WPI_RX_RADIOTAP_PRESENT);

	sc->sc_txtap_len = sizeof sc->sc_txtapu;
	sc->sc_txtap.wt_ihdr.it_len = htole16(sc->sc_txtap_len);
	sc->sc_txtap.wt_ihdr.it_present = htole32(WPI_TX_RADIOTAP_PRESENT);
#endif

	return;

fail3:	wpi_free_tx_ring(sc, &sc->svcq);
fail2:	wpi_free_tx_ring(sc, &sc->cmdq);
fail1:	while (--ac >= 0)
		wpi_free_tx_ring(sc, &sc->txq[ac]);
	wpi_free_shared(sc);
}

int
wpi_detach(struct device* self, int flags)
{
	struct wpi_softc *sc = (struct wpi_softc *)self;
	struct ifnet *ifp = &sc->sc_ic.ic_if;
	int ac;

	wpi_stop(ifp, 1);

	ieee80211_ifdetach(ifp);
	if_detach(ifp);

	for (ac = 0; ac < 4; ac++)
		wpi_free_tx_ring(sc, &sc->txq[ac]);
	wpi_free_tx_ring(sc, &sc->cmdq);
	wpi_free_tx_ring(sc, &sc->svcq);
	wpi_free_rx_ring(sc, &sc->rxq);
	wpi_free_shared(sc);

	if (sc->sc_ih != NULL) {
		pci_intr_disestablish(sc->sc_pct, sc->sc_ih);
		sc->sc_ih = NULL;
	}

	bus_space_unmap(sc->sc_st, sc->sc_sh, sc->sc_sz);

	return 0;
}

void
wpi_power(int why, void *arg)
{
	struct wpi_softc *sc = arg;
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
wpi_dma_contig_alloc(struct wpi_softc *sc, struct wpi_dma_info *dma,
    void **kvap, bus_size_t size, bus_size_t alignment, int flags)
{
	int nsegs, error;

	dma->size = size;

	error = bus_dmamap_create(sc->sc_dmat, size, 1, size, 0, flags,
	    &dma->map);
	if (error != 0) {
		printf("%s: could not create DMA map\n", sc->sc_dev.dv_xname);
		goto fail;
	}

	error = bus_dmamem_alloc(sc->sc_dmat, size, alignment, 0, &dma->seg,
	    1, &nsegs, flags);
	if (error != 0) {
		printf("%s: could not allocate DMA memory\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	error = bus_dmamem_map(sc->sc_dmat, &dma->seg, 1, size, &dma->vaddr,
	    flags);
	if (error != 0) {
		printf("%s: could not map DMA memory\n", sc->sc_dev.dv_xname);
		goto fail;
	}

	error = bus_dmamap_load_raw(sc->sc_dmat, dma->map, &dma->seg, 1, size,
	    flags);
	if (error != 0) {
		printf("%s: could not load DMA memory\n", sc->sc_dev.dv_xname);
		goto fail;
	}

	bzero(dma->vaddr, size);

	dma->paddr = dma->map->dm_segs[0].ds_addr;
	*kvap = dma->vaddr;

	return 0;

fail:	wpi_dma_contig_free(sc, dma);
	return error;
}

void
wpi_dma_contig_free(struct wpi_softc *sc, struct wpi_dma_info *dma)
{
	if (dma->map != NULL) {
		if (dma->vaddr != NULL) {
			bus_dmamap_unload(sc->sc_dmat, dma->map);
			bus_dmamem_unmap(sc->sc_dmat, dma->vaddr, dma->size);
			bus_dmamem_free(sc->sc_dmat, &dma->seg, 1);
			dma->vaddr = NULL;
		}
		bus_dmamap_destroy(sc->sc_dmat, dma->map);
		dma->map = NULL;
	}
}

/*
 * Allocate a shared page between host and NIC.
 */
int
wpi_alloc_shared(struct wpi_softc *sc)
{
	int error;

	/* must be aligned on a 4K-page boundary */
	error = wpi_dma_contig_alloc(sc, &sc->shared_dma,
	    (void **)&sc->shared, sizeof (struct wpi_shared), PAGE_SIZE,
	    BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not allocate shared area DMA memory\n",
		    sc->sc_dev.dv_xname);
	}

	return error;
}

void
wpi_free_shared(struct wpi_softc *sc)
{
	wpi_dma_contig_free(sc, &sc->shared_dma);
}

int
wpi_alloc_rx_ring(struct wpi_softc *sc, struct wpi_rx_ring *ring)
{
	struct wpi_rx_data *data;
	int i, error;

	ring->cur = 0;

	error = wpi_dma_contig_alloc(sc, &ring->desc_dma,
	    (void **)&ring->desc,
	    WPI_RX_RING_COUNT * sizeof (struct wpi_rx_desc),
	    WPI_RING_DMA_ALIGN, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not allocate rx ring DMA memory\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	/*
	 * Allocate Rx buffers.
	 */
	for (i = 0; i < WPI_RX_RING_COUNT; i++) {
		data = &ring->data[i];

		error = bus_dmamap_create(sc->sc_dmat, MCLBYTES, 1, MCLBYTES,
		    0, BUS_DMA_NOWAIT, &data->map);
		if (error != 0) {
			printf("%s: could not create rx buf DMA map\n",
			    sc->sc_dev.dv_xname);
			goto fail;
		}

		MGETHDR(data->m, M_DONTWAIT, MT_DATA);
		if (data->m == NULL) {
			printf("%s: could not allocate rx mbuf\n",
			    sc->sc_dev.dv_xname);
			error = ENOMEM;
			goto fail;
		}

		MCLGET(data->m, M_DONTWAIT);
		if (!(data->m->m_flags & M_EXT)) {
			m_freem(data->m);
			data->m = NULL;
			printf("%s: could not allocate rx mbuf cluster\n",
			    sc->sc_dev.dv_xname);
			error = ENOMEM;
			goto fail;
		}

		error = bus_dmamap_load(sc->sc_dmat, data->map,
		    mtod(data->m, void *), MCLBYTES, NULL, BUS_DMA_NOWAIT |
		    BUS_DMA_READ);
		if (error != 0) {
			printf("%s: could not load rx buf DMA map\n",
			    sc->sc_dev.dv_xname);
			goto fail;
		}

		ring->desc[i] = htole32(data->map->dm_segs[0].ds_addr);
	}

	return 0;

fail:	wpi_free_rx_ring(sc, ring);
	return error;
}

void
wpi_reset_rx_ring(struct wpi_softc *sc, struct wpi_rx_ring *ring)
{
	int ntries;

	wpi_mem_lock(sc);

	WPI_WRITE(sc, WPI_RX_CONFIG, 0);
	for (ntries = 0; ntries < 100; ntries++) {
		if (WPI_READ(sc, WPI_RX_STATUS) & WPI_RX_IDLE)
			break;
		DELAY(10);
	}
#ifdef WPI_DEBUG
	if (ntries == 100 && wpi_debug > 0)
		printf("%s: timeout resetting Rx ring\n", sc->sc_dev.dv_xname);
#endif
	wpi_mem_unlock(sc);

	ring->cur = 0;
}

void
wpi_free_rx_ring(struct wpi_softc *sc, struct wpi_rx_ring *ring)
{
	struct wpi_rx_data *data;
	int i;

	wpi_dma_contig_free(sc, &ring->desc_dma);

	for (i = 0; i < WPI_RX_RING_COUNT; i++) {
		data = &ring->data[i];

		if (data->m != NULL) {
			bus_dmamap_unload(sc->sc_dmat, data->map);
			m_freem(data->m);
		}
		bus_dmamap_destroy(sc->sc_dmat, data->map);
	}
}

int
wpi_alloc_tx_ring(struct wpi_softc *sc, struct wpi_tx_ring *ring, int count,
    int qid)
{
	struct wpi_tx_data *data;
	int i, error;

	ring->qid = qid;
	ring->count = count;
	ring->queued = 0;
	ring->cur = 0;

	error = wpi_dma_contig_alloc(sc, &ring->desc_dma,
	    (void **)&ring->desc, count * sizeof (struct wpi_tx_desc),
	    WPI_RING_DMA_ALIGN, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not allocate tx ring DMA memory\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	/* update shared page with ring's base address */
	sc->shared->txbase[qid] = htole32(ring->desc_dma.paddr);

	error = wpi_dma_contig_alloc(sc, &ring->cmd_dma, (void **)&ring->cmd,
	    count * sizeof (struct wpi_tx_cmd), 4, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not allocate tx cmd DMA memory\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	ring->data = malloc(count * sizeof (struct wpi_tx_data), M_DEVBUF,
	    M_NOWAIT);
	if (ring->data == NULL) {
		printf("%s: could not allocate tx data slots\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	bzero(ring->data, count * sizeof (struct wpi_tx_data));

	for (i = 0; i < count; i++) {
		data = &ring->data[i];

		error = bus_dmamap_create(sc->sc_dmat, MCLBYTES,
		    WPI_MAX_SCATTER - 1, MCLBYTES, 0, BUS_DMA_NOWAIT,
		    &data->map);
		if (error != 0) {
			printf("%s: could not create tx buf DMA map\n",
			    sc->sc_dev.dv_xname);
			goto fail;
		}
	}

	return 0;

fail:	wpi_free_tx_ring(sc, ring);
	return error;
}

void
wpi_reset_tx_ring(struct wpi_softc *sc, struct wpi_tx_ring *ring)
{
	struct wpi_tx_data *data;
	int i, ntries;

	wpi_mem_lock(sc);

	WPI_WRITE(sc, WPI_TX_CONFIG(ring->qid), 0);
	for (ntries = 0; ntries < 100; ntries++) {
		if (WPI_READ(sc, WPI_TX_STATUS) & WPI_TX_IDLE(ring->qid))
			break;
		DELAY(10);
	}
#ifdef WPI_DEBUG
	if (ntries == 100 && wpi_debug > 0) {
		printf("%s: timeout resetting Tx ring %d\n",
		    sc->sc_dev.dv_xname, ring->qid);
	}
#endif
	wpi_mem_unlock(sc);

	for (i = 0; i < ring->count; i++) {
		data = &ring->data[i];

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
wpi_free_tx_ring(struct wpi_softc *sc, struct wpi_tx_ring *ring)
{
	struct wpi_tx_data *data;
	int i;

	wpi_dma_contig_free(sc, &ring->desc_dma);
	wpi_dma_contig_free(sc, &ring->cmd_dma);

	if (ring->data != NULL) {
		for (i = 0; i < ring->count; i++) {
			data = &ring->data[i];

			if (data->m != NULL) {
				bus_dmamap_unload(sc->sc_dmat, data->map);
				m_freem(data->m);
			}
		}
		free(ring->data, M_DEVBUF);
	}
}

struct ieee80211_node *
wpi_node_alloc(struct ieee80211com *ic)
{
	struct wpi_amrr *amrr;

	amrr = malloc(sizeof (struct wpi_amrr), M_DEVBUF, M_NOWAIT);
	if (amrr != NULL) {
		bzero(amrr, sizeof (struct wpi_amrr));
		wpi_amrr_init(amrr);
	}
	return (struct ieee80211_node *)amrr;
}

void
wpi_node_copy(struct ieee80211com *ic, struct ieee80211_node *dst,
    const struct ieee80211_node *src)
{
	*dst = *src;
	wpi_amrr_init((struct wpi_amrr *)dst);
}

int
wpi_media_change(struct ifnet *ifp)
{
	int error;

	error = ieee80211_media_change(ifp);
	if (error != ENETRESET)
		return error;

	if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) == (IFF_UP | IFF_RUNNING))
		wpi_init(ifp);

	return 0;
}

int
wpi_newstate(struct ieee80211com *ic, enum ieee80211_state nstate, int arg)
{
	struct ifnet *ifp = &ic->ic_if;
	struct wpi_softc *sc = ifp->if_softc;
	int error;

	timeout_del(&sc->amrr_ch);

	switch (nstate) {
	case IEEE80211_S_SCAN:
		/* make the link LED blink while we're scanning */
		wpi_set_led(sc, WPI_LED_LINK, 20, 2);

		if ((error = wpi_scan(sc)) != 0) {
			printf("%s: could not initiate scan\n",
			    sc->sc_dev.dv_xname);
			return error;
		}
		ic->ic_state = nstate;
		return 0;

	case IEEE80211_S_AUTH:
		if ((error = wpi_auth(sc)) != 0) {
			printf("%s: could not send authentication request\n",
			    sc->sc_dev.dv_xname);
			return error;
		}
		break;

	case IEEE80211_S_RUN:
		if (ic->ic_opmode == IEEE80211_M_MONITOR) {
			/* link LED blinks while monitoring */
			wpi_set_led(sc, WPI_LED_LINK, 5, 5);
			break;
		}

		wpi_enable_tsf(sc, ic->ic_bss);

		/* update adapter's configuration */
		sc->config.state = htole16(WPI_CONFIG_ASSOCIATED);
		/* short preamble/slot time are negotiated when associating */
		sc->config.flags &= ~htole32(WPI_CONFIG_SHPREAMBLE |
		    WPI_CONFIG_SHSLOT);
		if (ic->ic_flags & IEEE80211_F_SHSLOT)
			sc->config.flags |= htole32(WPI_CONFIG_SHSLOT);
		if (ic->ic_flags & IEEE80211_F_SHPREAMBLE)
			sc->config.flags |= htole32(WPI_CONFIG_SHPREAMBLE);
		sc->config.filter |= htole32(WPI_FILTER_BSSID);

		DPRINTF(("config chan %d flags %x\n", sc->config.chan,
		    sc->config.flags));
		error = wpi_cmd(sc, WPI_CMD_CONFIGURE, &sc->config,
		    sizeof (struct wpi_config), 1);
		if (error != 0) {
			printf("%s: could not update configuration\n",
			    sc->sc_dev.dv_xname);
			return error;
		}

		/* start automatic rate control timer */
		timeout_add(&sc->amrr_ch, hz / 2);

		/* link LED always on while associated */
		wpi_set_led(sc, WPI_LED_LINK, 0, 1);
		break;

	case IEEE80211_S_ASSOC:
	case IEEE80211_S_INIT:
		break;
	}

	return sc->sc_newstate(ic, nstate, arg);
}

/*
 * Grab exclusive access to NIC memory.
 */
void
wpi_mem_lock(struct wpi_softc *sc)
{
	uint32_t tmp;
	int ntries;

	tmp = WPI_READ(sc, WPI_GPIO_CTL);
	WPI_WRITE(sc, WPI_GPIO_CTL, tmp | WPI_GPIO_MAC);

	/* spin until we actually get the lock */
	for (ntries = 0; ntries < 1000; ntries++) {
		if ((WPI_READ(sc, WPI_GPIO_CTL) &
		    (WPI_GPIO_CLOCK | WPI_GPIO_SLEEP)) == WPI_GPIO_CLOCK)
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
wpi_mem_unlock(struct wpi_softc *sc)
{
	uint32_t tmp = WPI_READ(sc, WPI_GPIO_CTL);
	WPI_WRITE(sc, WPI_GPIO_CTL, tmp & ~WPI_GPIO_MAC);
}

uint32_t
wpi_mem_read(struct wpi_softc *sc, uint16_t addr)
{
	WPI_WRITE(sc, WPI_READ_MEM_ADDR, WPI_MEM_4 | addr);
	return WPI_READ(sc, WPI_READ_MEM_DATA);
}

void
wpi_mem_write(struct wpi_softc *sc, uint16_t addr, uint32_t data)
{
	WPI_WRITE(sc, WPI_WRITE_MEM_ADDR, WPI_MEM_4 | addr);
	WPI_WRITE(sc, WPI_WRITE_MEM_DATA, data);
}

void
wpi_mem_write_region_4(struct wpi_softc *sc, uint16_t addr,
    const uint32_t *data, int wlen)
{
	for (; wlen > 0; wlen--, data++, addr += 4)
		wpi_mem_write(sc, addr, *data);
}

/*
 * Read 16 bits from the EEPROM.  We access EEPROM through the MAC instead of
 * using the traditional bit-bang method.
 */
uint16_t
wpi_read_prom_word(struct wpi_softc *sc, uint32_t addr)
{
	int ntries;
	uint32_t val;

	WPI_WRITE(sc, WPI_EEPROM_CTL, addr << 2);

	wpi_mem_lock(sc);
	for (ntries = 0; ntries < 10; ntries++) {
		if ((val = WPI_READ(sc, WPI_EEPROM_CTL)) & WPI_EEPROM_READY)
			break;
		DELAY(10);
	}
	wpi_mem_unlock(sc);

	if (ntries == 10) {
		printf("%s: could not read EEPROM\n", sc->sc_dev.dv_xname);
		return 0xdead;
	}
	return val >> 16;
}

/*
 * The firmware boot code is small and is intended to be copied directly into
 * the NIC internal memory.
 */
int
wpi_load_microcode(struct wpi_softc *sc, const char *ucode, int size)
{
	/* check that microcode size is a multiple of 4 */
	if (size & 3)
		return EINVAL;

	size /= sizeof (uint32_t);

	wpi_mem_lock(sc);

	/* copy microcode image into NIC memory */
	wpi_mem_write_region_4(sc, WPI_MEM_UCODE_BASE, (const uint32_t *)ucode,
	    size);

	wpi_mem_write(sc, WPI_MEM_UCODE_SRC, 0);
	wpi_mem_write(sc, WPI_MEM_UCODE_DST, WPI_FW_TEXT);
	wpi_mem_write(sc, WPI_MEM_UCODE_SIZE, size);

	/* run microcode */
	wpi_mem_write(sc, WPI_MEM_UCODE_CTL, WPI_UC_RUN);

	wpi_mem_unlock(sc);

	return 0;
}

/*
 * The firmware text and data segments are transferred to the NIC using DMA.
 * The driver just copies the firmware into DMA-safe memory and tells the NIC
 * where to find it.  Once the NIC has copied the firmware into its internal
 * memory, we can free our local copy in the driver.
 */
int
wpi_load_firmware(struct wpi_softc *sc, uint32_t target, const char *fw,
    int size)
{
	bus_dmamap_t map;
	bus_dma_segment_t seg;
	caddr_t virtaddr;
	struct wpi_tx_desc desc;
	int i, ntries, nsegs, error;

	/*
	 * Allocate DMA-safe memory to store the firmware.
	 */
	error = bus_dmamap_create(sc->sc_dmat, size, WPI_MAX_SCATTER,
	    WPI_MAX_SEG_LEN, 0, BUS_DMA_NOWAIT, &map);
	if (error != 0) {
		printf("%s: could not create firmware DMA map\n",
		    sc->sc_dev.dv_xname);
		goto fail1;
	}

	error = bus_dmamem_alloc(sc->sc_dmat, size, PAGE_SIZE, 0, &seg, 1,
	    &nsegs, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not allocate firmware DMA memory\n",
		    sc->sc_dev.dv_xname);
		goto fail2;
	}

	error = bus_dmamem_map(sc->sc_dmat, &seg, nsegs, size, &virtaddr,
	    BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not map firmware DMA memory\n",
		    sc->sc_dev.dv_xname);
		goto fail3;
	}

	error = bus_dmamap_load(sc->sc_dmat, map, virtaddr, size, NULL,
	    BUS_DMA_NOWAIT | BUS_DMA_WRITE);
	if (error != 0) {
		printf("%s: could not load firmware DMA map\n",
		    sc->sc_dev.dv_xname);
		goto fail4;
	}

	/* copy firmware image to DMA-safe memory */
	bcopy(fw, virtaddr, size);

	/* make sure the adapter will get up-to-date values */
	bus_dmamap_sync(sc->sc_dmat, map, 0, size, BUS_DMASYNC_PREWRITE);

	bzero(&desc, sizeof desc);
	desc.flags = htole32(WPI_PAD32(size) << 28 | map->dm_nsegs << 24);
	for (i = 0; i < map->dm_nsegs; i++) {
		desc.segs[i].addr = htole32(map->dm_segs[i].ds_addr);
		desc.segs[i].len  = htole32(map->dm_segs[i].ds_len);
	}

	wpi_mem_lock(sc);

	/* tell adapter where to copy image in its internal memory */
	WPI_WRITE(sc, WPI_FW_TARGET, target);

	WPI_WRITE(sc, WPI_TX_CONFIG(6), 0);

	/* copy firmware descriptor into NIC memory */
	WPI_WRITE_REGION_4(sc, WPI_TX_DESC(6), (uint32_t *)&desc,
	    sizeof desc / sizeof (uint32_t));

	WPI_WRITE(sc, WPI_TX_CREDIT(6), 0xfffff);
	WPI_WRITE(sc, WPI_TX_STATE(6), 0x4001);
	WPI_WRITE(sc, WPI_TX_CONFIG(6), 0x80000001);

	/* wait while the adapter is busy copying the firmware */
	for (ntries = 0; ntries < 100; ntries++) {
		if (WPI_READ(sc, WPI_TX_STATUS) & WPI_TX_IDLE(6))
			break;
		DELAY(1000);
	}
	if (ntries == 100) {
		printf("%s: timeout transferring firmware\n",
		    sc->sc_dev.dv_xname);
		error = ETIMEDOUT;
	}

	WPI_WRITE(sc, WPI_TX_CREDIT(6), 0);

	wpi_mem_unlock(sc);

	bus_dmamap_sync(sc->sc_dmat, map, 0, size, BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(sc->sc_dmat, map);
fail4:	bus_dmamem_unmap(sc->sc_dmat, virtaddr, size);
fail3:	bus_dmamem_free(sc->sc_dmat, &seg, 1);
fail2:	bus_dmamap_destroy(sc->sc_dmat, map);
fail1:	return error;
}

void
wpi_rx_intr(struct wpi_softc *sc, struct wpi_rx_desc *desc,
    struct wpi_rx_data *data)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct wpi_rx_ring *ring = &sc->rxq;
	struct wpi_rx_stat *stat;
	struct wpi_rx_head *head;
	struct wpi_rx_tail *tail;
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni;
	struct mbuf *m, *mnew;
	int error;

	stat = (struct wpi_rx_stat *)(desc + 1);

	if (stat->len > WPI_STAT_MAXLEN) {
		printf("%s: invalid rx statistic header\n",
		    sc->sc_dev.dv_xname);
		ifp->if_ierrors++;
		return;
	}

	head = (struct wpi_rx_head *)((caddr_t)(stat + 1) + stat->len);
	tail = (struct wpi_rx_tail *)((caddr_t)(head + 1) + letoh16(head->len));

	DPRINTFN(4, ("rx intr: idx=%d len=%d stat len=%d rssi=%d rate=%x "
	    "chan=%d tstamp=%llu\n", ring->cur, letoh32(desc->len),
	    letoh16(head->len), (int8_t)stat->rssi, head->rate, head->chan,
	    letoh64(tail->tstamp)));

	MGETHDR(mnew, M_DONTWAIT, MT_DATA);
	if (mnew == NULL) {
		ifp->if_ierrors++;
		return;
	}

	MCLGET(mnew, M_DONTWAIT);
	if (!(mnew->m_flags & M_EXT)) {
		m_freem(mnew);
		ifp->if_ierrors++;
		return;
	}

	bus_dmamap_unload(sc->sc_dmat, data->map);

	error = bus_dmamap_load(sc->sc_dmat, data->map, mtod(mnew, void *),
	    MCLBYTES, NULL, BUS_DMA_NOWAIT);
	if (error != 0) {
		m_freem(mnew);

		/* try to reload the old mbuf */
		error = bus_dmamap_load(sc->sc_dmat, data->map,
		    mtod(data->m, void *), MCLBYTES, NULL, BUS_DMA_NOWAIT);
		if (error != 0) {
			/* very unlikely that it will fail... */
			panic("%s: could not load old rx mbuf",
			    sc->sc_dev.dv_xname);
		}
		ifp->if_ierrors++;
		return;
	}

	m = data->m;
	data->m = mnew;

	/* update Rx descriptor */
	ring->desc[ring->cur] = htole32(data->map->dm_segs[0].ds_addr);

	/* finalize mbuf */
	m->m_pkthdr.rcvif = ifp;
	m->m_data = (caddr_t)(head + 1);
	m->m_pkthdr.len = m->m_len = letoh16(head->len);

#if NBPFILTER > 0
	if (sc->sc_drvbpf != NULL) {
		struct mbuf mb;
		struct wpi_rx_radiotap_header *tap = &sc->sc_rxtap;

		tap->wr_flags = 0;
		tap->wr_chan_freq =
		    htole16(ic->ic_channels[head->chan].ic_freq);
		tap->wr_chan_flags =
		    htole16(ic->ic_channels[head->chan].ic_flags);
		tap->wr_dbm_antsignal = (int8_t)(stat->rssi - WPI_RSSI_OFFSET);
		tap->wr_dbm_antnoise = (int8_t)letoh16(stat->noise);
		tap->wr_tsft = tail->tstamp;
		tap->wr_antenna = (letoh16(head->flags) >> 4) & 0xf;
		switch (head->rate) {
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
		if (letoh16(head->flags) & 0x4)
			tap->wr_flags |= IEEE80211_RADIOTAP_F_SHORTPRE;

		M_DUP_PKTHDR(&mb, m);
		mb.m_data = (caddr_t)tap;
		mb.m_len = sc->sc_rxtap_len;
		mb.m_next = m;
		mb.m_pkthdr.len += mb.m_len;
		bpf_mtap(sc->sc_drvbpf, &mb, BPF_DIRECTION_IN);
	}
#endif

	/* grab a reference to the source node */
	wh = mtod(m, struct ieee80211_frame *);
	ni = ieee80211_find_rxnode(ic, wh);

	/* send the frame to the 802.11 layer */
	ieee80211_input(ifp, m, ni, stat->rssi, 0);

	/* node is no longer needed */
	ieee80211_release_node(ic, ni);
}

void
wpi_tx_intr(struct wpi_softc *sc, struct wpi_rx_desc *desc,
    struct wpi_rx_data *data)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct wpi_tx_ring *ring = &sc->txq[desc->qid & 0x3];
	struct wpi_tx_data *txdata = &ring->data[desc->idx];
	struct wpi_tx_stat *stat = (struct wpi_tx_stat *)(desc + 1);
	struct wpi_amrr *amrr = (struct wpi_amrr *)txdata->ni;

	DPRINTFN(4, ("tx done: qid=%d idx=%d retries=%d nkill=%d rate=%x "
	    "duration=%d status=%x\n", desc->qid, desc->idx, stat->ntries,
	    stat->nkill, stat->rate, letoh32(stat->duration),
	    letoh32(stat->status)));

	/* update rate control statistics for the node */
	amrr->txcnt++;
	if (stat->ntries > 0) {
		DPRINTFN(3, ("tx intr ntries %d\n", stat->ntries));
		amrr->retrycnt++;
	}

	bus_dmamap_unload(sc->sc_dmat, data->map);

	m_freem(txdata->m);
	txdata->m = NULL;
	ieee80211_release_node(ic, txdata->ni);
	txdata->ni = NULL;

	if ((letoh32(stat->status) & 0xff) != 1)
		ifp->if_oerrors++;
	else
		ifp->if_opackets++;
	ring->queued--;

	sc->sc_tx_timer = 0;
	ifp->if_flags &= ~IFF_OACTIVE;
	(*ifp->if_start)(ifp);
}

void
wpi_cmd_intr(struct wpi_softc *sc, struct wpi_rx_desc *desc)
{
	struct wpi_tx_ring *ring = &sc->cmdq;
	struct wpi_tx_data *data;

	if ((desc->qid & 7) != 4)
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
wpi_notif_intr(struct wpi_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct wpi_rx_desc *desc;
	struct wpi_rx_data *data;
	uint32_t hw;

	hw = letoh32(sc->shared->next);
	while (sc->rxq.cur != hw) {
		data = &sc->rxq.data[sc->rxq.cur];

		desc = mtod(data->m, struct wpi_rx_desc *);

		DPRINTFN(4, ("rx notification qid=%x idx=%d flags=%x type=%d "
		    "len=%d\n", desc->qid, desc->idx, desc->flags, desc->type,
		    letoh32(desc->len)));

		if (!(desc->qid & 0x80))	/* reply to a command */
			wpi_cmd_intr(sc, desc);

		switch (desc->type) {
		case WPI_RX_DONE:
			/* a 802.11 frame was received */
			wpi_rx_intr(sc, desc, data);
			break;

		case WPI_TX_DONE:
			/* a 802.11 frame has been transmitted */
			wpi_tx_intr(sc, desc, data);
			break;

		case WPI_UC_READY:
		{
			struct wpi_ucode_info *uc =
			    (struct wpi_ucode_info *)(desc + 1);

			/* the microcontroller is ready */
			DPRINTF(("microcode alive notification version %x "
			    "alive %x\n", letoh32(uc->version),
			    letoh32(uc->valid)));

			if (letoh32(uc->valid) != 1) {
				printf("%s microcontroller initialization "
				    "failed\n", sc->sc_dev.dv_xname);
			}
			break;
		}
		case WPI_STATE_CHANGED:
		{
			uint32_t *status = (uint32_t *)(desc + 1);

			/* enabled/disabled notification */
			DPRINTF(("state changed to %x\n", letoh32(*status)));

			if (letoh32(*status) & 1) {
				/* the radio button has to be pushed */
				printf("%s: Radio transmitter is off\n",
				    sc->sc_dev.dv_xname);
			}
			break;
		}
		case WPI_START_SCAN:
		{
			struct wpi_start_scan *scan =
			    (struct wpi_start_scan *)(desc + 1);

			DPRINTFN(2, ("scanning channel %d status %x\n",
			    scan->chan, letoh32(scan->status)));

			/* fix current channel */
			ic->ic_bss->ni_chan = &ic->ic_channels[scan->chan];
			break;
		}
		case WPI_STOP_SCAN:
			DPRINTF(("scan finished\n"));
			ieee80211_end_scan(ifp);
			break;
		}

		sc->rxq.cur = (sc->rxq.cur + 1) % WPI_RX_RING_COUNT;
	}

	/* tell the firmware what we have processed */
	hw = (hw == 0) ? WPI_RX_RING_COUNT - 1 : hw - 1;
	WPI_WRITE(sc, WPI_RX_WIDX, hw & ~7);
}

int
wpi_intr(void *arg)
{
	struct wpi_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_ic.ic_if;
	uint32_t r;

	r = WPI_READ(sc, WPI_INTR);
	if (r == 0 || r == 0xffffffff)
		return 0;	/* not for us */

	DPRINTFN(6, ("interrupt reg %x\n", r));

	/* disable interrupts */
	WPI_WRITE(sc, WPI_MASK, 0);
	/* ack interrupts */
	WPI_WRITE(sc, WPI_INTR, r);

	if (r & (WPI_SW_ERROR | WPI_HW_ERROR)) {
		printf("%s: fatal firmware error\n", sc->sc_dev.dv_xname);
		ifp->if_flags &= ~IFF_UP;
		wpi_stop(ifp, 1);
		return 1;
	}

	if (r & WPI_RX_INTR)
		wpi_notif_intr(sc);

	if (r & WPI_ALIVE_INTR)	/* firmware initialized */
		wakeup(sc);

	/* re-enable interrupts */
	WPI_WRITE(sc, WPI_MASK, WPI_INTR_MASK);

	return 1;
}

uint8_t
wpi_plcp_signal(int rate)
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

	/* unsupported rates (should not get there) */
	default:	return 0;
	}
}

int
wpi_tx_data(struct wpi_softc *sc, struct mbuf *m0, struct ieee80211_node *ni,
    int ac)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct wpi_tx_ring *ring = &sc->txq[ac];
	struct wpi_tx_desc *desc;
	struct wpi_tx_data *data;
	struct wpi_tx_cmd *cmd;
	struct wpi_cmd_data *tx;
	struct ieee80211_frame *wh;
	struct mbuf *mnew;
	int i, rate, error;

	desc = &ring->desc[ring->cur];
	data = &ring->data[ring->cur];

	wh = mtod(m0, struct ieee80211_frame *);

	if (wh->i_fc[1] & IEEE80211_FC1_WEP) {
		m0 = ieee80211_wep_crypt(ifp, m0, 1);
		if (m0 == NULL)
			return ENOBUFS;

		/* packet header may have moved, reset our local pointer */
		wh = mtod(m0, struct ieee80211_frame *);
	}

	/* pickup a rate */
	if ((wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) ==
	    IEEE80211_FC0_TYPE_MGT) {
		/* mgmt frames are sent at the lowest available bit-rate */
		rate = ni->ni_rates.rs_rates[0];
	} else {
		if (ic->ic_fixed_rate != -1) {
			rate = ic->ic_sup_rates[ic->ic_curmode].
			    rs_rates[ic->ic_fixed_rate];
		} else
			rate = ni->ni_rates.rs_rates[ni->ni_txrate];
	}
	rate &= IEEE80211_RATE_VAL;

#if NBPFILTER > 0
	if (sc->sc_drvbpf != NULL) {
		struct mbuf mb;
		struct wpi_tx_radiotap_header *tap = &sc->sc_txtap;

		tap->wt_flags = 0;
		tap->wt_chan_freq = htole16(ni->ni_chan->ic_freq);
		tap->wt_chan_flags = htole16(ni->ni_chan->ic_flags);
		tap->wt_rate = rate;
		tap->wt_hwqueue = ac;
		if (wh->i_fc[1] & IEEE80211_FC1_WEP)
			tap->wt_flags |= IEEE80211_RADIOTAP_F_WEP;

		M_DUP_PKTHDR(&mb, m0);
		mb.m_data = (caddr_t)tap;
		mb.m_len = sc->sc_txtap_len;
		mb.m_next = m0;
		mb.m_pkthdr.len += mb.m_len;
		bpf_mtap(sc->sc_drvbpf, &mb, BPF_DIRECTION_OUT);
	}
#endif

	cmd = &ring->cmd[ring->cur];
	cmd->code = WPI_CMD_TX_DATA;
	cmd->flags = 0;
	cmd->qid = ring->qid;
	cmd->idx = ring->cur;

	tx = (struct wpi_cmd_data *)cmd->data;
	tx->flags = 0;

	if (!IEEE80211_IS_MULTICAST(wh->i_addr1))
		tx->flags |= htole32(WPI_TX_NEED_ACK);
	else if (m0->m_pkthdr.len + IEEE80211_CRC_LEN > ic->ic_rtsthreshold)
		tx->flags |= htole32(WPI_TX_NEED_RTS | WPI_TX_FULL_TXOP);

	tx->flags |= htole32(WPI_TX_AUTO_SEQ);

	/* retrieve destination node's id */
	tx->id = IEEE80211_IS_MULTICAST(wh->i_addr1) ? WPI_ID_BROADCAST :
	    WPI_ID_BSSID;

	if ((wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) ==
	    IEEE80211_FC0_TYPE_MGT) {
		/* tell h/w to set timestamp in probe responses */
		if ((wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) ==
		    IEEE80211_FC0_SUBTYPE_PROBE_RESP)
			tx->flags |= htole32(WPI_TX_INSERT_TSTAMP);

		if (((wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) ==
		     IEEE80211_FC0_SUBTYPE_ASSOC_REQ) ||
		    ((wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) ==
		     IEEE80211_FC0_SUBTYPE_REASSOC_REQ))
			tx->timeout = 3;
		else
			tx->timeout = 2;
	} else
		tx->timeout = 0;

	tx->rate = wpi_plcp_signal(rate);

	/* be very persistant at sending frames out */
	tx->rts_ntries = 7;
	tx->data_ntries = 15;

	tx->ofdm_mask = 0xff;
	tx->cck_mask = 0xf;;
	tx->lifetime = htole32(0xffffffff);

	tx->len = htole16(m0->m_pkthdr.len);

	/* save and trim IEEE802.11 header */
	m_copydata(m0, 0, sizeof (struct ieee80211_frame), (caddr_t)&tx->wh);
	m_adj(m0, sizeof (struct ieee80211_frame));

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

	/* first scatter/gather segment is used by the tx data command */
	desc->flags = htole32(WPI_PAD32(m0->m_pkthdr.len) << 28 |
	    (1 + data->map->dm_nsegs) << 24);
	desc->segs[0].addr = htole32(ring->cmd_dma.paddr +
	    ring->cur * sizeof (struct wpi_tx_cmd));
	desc->segs[0].len  = htole32(4 + sizeof (struct wpi_cmd_data));
	for (i = 1; i <= data->map->dm_nsegs; i++) {
		desc->segs[i].addr =
		    htole32(data->map->dm_segs[i - 1].ds_addr);
		desc->segs[i].len  =
		    htole32(data->map->dm_segs[i - 1].ds_len);
	}

	ring->queued++;

	/* kick ring */
	ring->cur = (ring->cur + 1) % WPI_TX_RING_COUNT;
	WPI_WRITE(sc, WPI_TX_WIDX, ring->qid << 8 | ring->cur);

	return 0;
}

void
wpi_start(struct ifnet *ifp)
{
	struct wpi_softc *sc = ifp->if_softc;
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
			if (wpi_tx_data(sc, m0, ni, 0) != 0)
				break;

		} else {
			if (ic->ic_state != IEEE80211_S_RUN)
				break;
			IFQ_DEQUEUE(&ifp->if_snd, m0);
			if (m0 == NULL)
				break;
			if (sc->txq[0].queued >= sc->txq[0].count - 8) {
				/* there is no place left in this ring */
				IF_PREPEND(&ifp->if_snd, m0);
				ifp->if_flags |= IFF_OACTIVE;
				break;
			}
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
			if (wpi_tx_data(sc, m0, ni, 0) != 0) {
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
wpi_watchdog(struct ifnet *ifp)
{
	struct wpi_softc *sc = ifp->if_softc;

	ifp->if_timer = 0;

	if (sc->sc_tx_timer > 0) {
		if (--sc->sc_tx_timer == 0) {
			printf("%s: device timeout\n", sc->sc_dev.dv_xname);
			wpi_stop(ifp, 1);
			ifp->if_oerrors++;
			return;
		}
		ifp->if_timer = 1;
	}

	ieee80211_watchdog(ifp);
}

int
wpi_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct wpi_softc *sc = ifp->if_softc;
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
				wpi_init(ifp);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				wpi_stop(ifp, 1);
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
			wpi_init(ifp);
		error = 0;
	}

	splx(s);
	return error;
}

/*
 * Extract various information from EEPROM.
 */
void
wpi_read_eeprom(struct wpi_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	uint16_t val;
	int i;

	/* read MAC address */
	val = wpi_read_prom_word(sc, WPI_EEPROM_MAC + 0);
	ic->ic_myaddr[0] = val & 0xff;
	ic->ic_myaddr[1] = val >> 8;
	val = wpi_read_prom_word(sc, WPI_EEPROM_MAC + 1);
	ic->ic_myaddr[2] = val & 0xff;
	ic->ic_myaddr[3] = val >> 8;
	val = wpi_read_prom_word(sc, WPI_EEPROM_MAC + 2);
	ic->ic_myaddr[4] = val & 0xff;
	ic->ic_myaddr[5] = val >> 8;

	/* read power settings for 2.4GHz channels */
	for (i = 0; i < 14; i++) {
		sc->pwr1[i] = wpi_read_prom_word(sc, WPI_EEPROM_PWR1 + i);
		sc->pwr2[i] = wpi_read_prom_word(sc, WPI_EEPROM_PWR2 + i);
		DPRINTF(("channel %d pwr1 0x%04x pwr2 0x%04x\n", i + 1,
		    sc->pwr1[i], sc->pwr2[i]));
	}
}

/*
 * Send a command to the firmware.
 */
int
wpi_cmd(struct wpi_softc *sc, int code, const void *buf, int size, int async)
{
	struct wpi_tx_ring *ring = &sc->cmdq;
	struct wpi_tx_desc *desc;
	struct wpi_tx_cmd *cmd;

	KASSERT(size <= sizeof cmd->data);

	desc = &ring->desc[ring->cur];
	cmd = &ring->cmd[ring->cur];

	cmd->code = code;
	cmd->flags = 0;
	cmd->qid = ring->qid;
	cmd->idx = ring->cur;
	bcopy(buf, cmd->data, size);

	desc->flags = htole32(WPI_PAD32(size) << 28 | 1 << 24);
	desc->segs[0].addr = htole32(ring->cmd_dma.paddr +
	    ring->cur * sizeof (struct wpi_tx_cmd));
	desc->segs[0].len  = htole32(4 + size);

	/* kick cmd ring */
	ring->cur = (ring->cur + 1) % WPI_CMD_RING_COUNT;
	WPI_WRITE(sc, WPI_TX_WIDX, ring->qid << 8 | ring->cur);

	return async ? 0 : tsleep(cmd, PCATCH, "wpicmd", hz);
}

/*
 * Configure h/w multi-rate retries.
 */
int
wpi_mrr_setup(struct wpi_softc *sc)
{
	struct wpi_mrr_setup mrr;
	int i, error;

	/* CCK rates (not used with 802.11a) */
	for (i = WPI_CCK1; i <= WPI_CCK11; i++) {
		mrr.rates[i].flags = 0;
		mrr.rates[i].plcp = wpi_ridx_to_plcp[i];
		/* fallback to the immediate lower CCK rate (if any) */
		mrr.rates[i].next = (i == WPI_CCK1) ? WPI_CCK1 : i - 1;
		/* try one time at this rate before falling back to "next" */
		mrr.rates[i].ntries = 1;
	}

	/* OFDM rates (not used with 802.11b) */
	for (i = WPI_OFDM6; i <= WPI_OFDM54; i++) {
		mrr.rates[i].flags = 0;
		mrr.rates[i].plcp = wpi_ridx_to_plcp[i];
		/* fallback to the immediate lower rate (if any) */
		/* we allow fallback from OFDM/6 to CCK/2 in 11b/g mode */
		mrr.rates[i].next = (i == WPI_OFDM6) ? WPI_CCK2 : i - 1;
		/* try one time at this rate before falling back to "next" */
		mrr.rates[i].ntries = 1;
	}

	/* setup MRR for control frames */
	mrr.which = htole32(WPI_MRR_CTL);
	error = wpi_cmd(sc, WPI_CMD_MRR_SETUP, &mrr, sizeof mrr, 1);
	if (error != 0) {
		printf("%s: could not setup MRR for control frames\n",
		    sc->sc_dev.dv_xname);
		return error;
	}

	/* setup MRR for data frames */
	mrr.which = htole32(WPI_MRR_DATA);
	error = wpi_cmd(sc, WPI_CMD_MRR_SETUP, &mrr, sizeof mrr, 1);
	if (error != 0) {
		printf("%s: could not setup MRR for data frames\n",
		    sc->sc_dev.dv_xname);
		return error;
	}

	return 0;
}

void
wpi_set_led(struct wpi_softc *sc, uint8_t which, uint8_t off, uint8_t on)
{
	struct wpi_cmd_led led;

	led.which = which;
	led.unit = htole32(100000);	/* on/off in unit of 100ms */
	led.off = off;
	led.on = on;

	(void)wpi_cmd(sc, WPI_CMD_SET_LED, &led, sizeof led, 1);
}

void
wpi_enable_tsf(struct wpi_softc *sc, struct ieee80211_node *ni)
{
	struct wpi_cmd_tsf tsf;

	bzero(&tsf, sizeof tsf);
	bcopy(ni->ni_tstamp, &tsf.tstamp, sizeof (uint64_t));
	tsf.bintval = htole16(ni->ni_intval);
	tsf.binitval = htole32(102400);	/* XXX */
	tsf.lintval = htole16(10);

	if (wpi_cmd(sc, WPI_CMD_TSF, &tsf, sizeof tsf, 1) != 0)
		printf("%s: could not enable TSF\n", sc->sc_dev.dv_xname);
}

/*
 * Build a beacon frame that the firmware will broadcast periodically in
 * IBSS or HostAP modes.
 */
int
wpi_setup_beacon(struct wpi_softc *sc, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct wpi_tx_ring *ring = &sc->cmdq;
	struct wpi_tx_desc *desc;
	struct wpi_tx_data *data;
	struct wpi_tx_cmd *cmd;
	struct wpi_cmd_beacon *bcn;
	struct mbuf *m0;
	int error;

	desc = &ring->desc[ring->cur];
	data = &ring->data[ring->cur];

	m0 = ieee80211_beacon_alloc(ic, ni);
	if (m0 == NULL) {
		printf("%s: could not allocate beacon frame\n",
		    sc->sc_dev.dv_xname);
		return ENOMEM;
	}

	cmd = &ring->cmd[ring->cur];
	cmd->code = WPI_CMD_SET_BEACON;
	cmd->flags = 0;
	cmd->qid = ring->qid;
	cmd->idx = ring->cur;

	bcn = (struct wpi_cmd_beacon *)cmd->data;
	bzero(bcn, sizeof (struct wpi_cmd_beacon));
	bcn->id = WPI_ID_BROADCAST;
	bcn->ofdm_mask = 0xff;
	bcn->cck_mask = 0xf;
	bcn->lifetime = htole32(0xffffffff);
	bcn->len = htole16(m0->m_pkthdr.len);
	bcn->rate = wpi_plcp_signal(2);
	bcn->flags = htole32(WPI_TX_AUTO_SEQ | WPI_TX_INSERT_TSTAMP);

	/* save and trim IEEE802.11 header */
	m_copydata(m0, 0, sizeof (struct ieee80211_frame), (caddr_t)&bcn->wh);
	m_adj(m0, sizeof (struct ieee80211_frame));

	/* assume beacon frame is contiguous */
	error = bus_dmamap_load(sc->sc_dmat, data->map, mtod(m0, void *),
	    m0->m_pkthdr.len, NULL, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not map beacon\n", sc->sc_dev.dv_xname);
		m_freem(m0);
		return error;
	}

	data->m = m0;

	/* first scatter/gather segment is used by the beacon command */
	desc->flags = htole32(WPI_PAD32(m0->m_pkthdr.len) << 28 | 2 << 24);
	desc->segs[0].addr = htole32(ring->cmd_dma.paddr +
	    ring->cur * sizeof (struct wpi_tx_cmd));
	desc->segs[0].len  = htole32(4 + sizeof (struct wpi_cmd_beacon));
	desc->segs[1].addr = htole32(data->map->dm_segs[0].ds_addr);
	desc->segs[1].len  = htole32(data->map->dm_segs[0].ds_len);

	/* kick cmd ring */
	ring->cur = (ring->cur + 1) % WPI_CMD_RING_COUNT;
	WPI_WRITE(sc, WPI_TX_WIDX, ring->qid << 8 | ring->cur);

	return 0;
}

int
wpi_auth(struct wpi_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni = ic->ic_bss;
	struct wpi_node node;
	int error;

	/* update adapter's configuration */
	IEEE80211_ADDR_COPY(sc->config.bssid, ni->ni_bssid);
	sc->config.chan = ieee80211_chan2ieee(ic, ni->ni_chan);
	if (ic->ic_curmode == IEEE80211_MODE_11B) {
		sc->config.cck_mask  = 0x03;
		sc->config.ofdm_mask = 0;
	} else if (IEEE80211_IS_CHAN_5GHZ(ni->ni_chan)) {
		sc->config.cck_mask  = 0;
		sc->config.ofdm_mask = 0x15;
	} else {	/* assume 802.11b/g */
		sc->config.cck_mask  = 0x0f;
		sc->config.ofdm_mask = 0x15;
	}
	if (ic->ic_flags & IEEE80211_F_SHSLOT)
		sc->config.flags |= htole32(WPI_CONFIG_SHSLOT);
	if (ic->ic_flags & IEEE80211_F_SHPREAMBLE)
		sc->config.flags |= htole32(WPI_CONFIG_SHPREAMBLE);

	DPRINTF(("config chan %d flags %x cck %x ofdm %x\n", sc->config.chan,
	    sc->config.flags, sc->config.cck_mask, sc->config.ofdm_mask));
	error = wpi_cmd(sc, WPI_CMD_CONFIGURE, &sc->config,
	    sizeof (struct wpi_config), 1);
	if (error != 0) {
		printf("%s: could not configure\n", sc->sc_dev.dv_xname);
		return error;
	}

	/* add default node */
	bzero(&node, sizeof node);
	IEEE80211_ADDR_COPY(node.bssid, ni->ni_bssid);
	node.id = WPI_ID_BSSID;
	node.rate = wpi_plcp_signal(2);
	error = wpi_cmd(sc, WPI_CMD_ADD_NODE, &node, sizeof node, 1);
	if (error != 0) {
		printf("%s: could not add BSS node\n", sc->sc_dev.dv_xname);
		return error;
	}

	error = wpi_mrr_setup(sc);
	if (error != 0) {
		printf("%s: could not setup MRR\n", sc->sc_dev.dv_xname);
		return error;
	}

	return 0;
}

/*
 * Send a scan request to the firmware.  Since this command is huge, we map it
 * into a mbuf instead of using the pre-allocated set of commands.
 */
int
wpi_scan(struct wpi_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct wpi_tx_ring *ring = &sc->cmdq;
	struct wpi_tx_desc *desc;
	struct wpi_tx_data *data;
	struct wpi_tx_cmd *cmd;
	struct wpi_scan_hdr *hdr;
	struct wpi_scan_chan *chan;
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni = ic->ic_bss;
	struct ieee80211_rateset *rs;
	enum ieee80211_phymode mode;
	uint8_t *frm;
	int i, pktlen, error;

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

	cmd = mtod(data->m, struct wpi_tx_cmd *);
	cmd->code = WPI_CMD_SCAN;
	cmd->flags = 0;
	cmd->qid = ring->qid;
	cmd->idx = ring->cur;

	hdr = (struct wpi_scan_hdr *)cmd->data;
	bzero(hdr, sizeof (struct wpi_scan_hdr));
	hdr->first = 1;
	hdr->nchan = 14;
	hdr->len = hdr->nchan * sizeof (struct wpi_scan_chan);
	hdr->quiet = htole16(5);
	hdr->threshold = htole16(1);
	hdr->filter = htole32(5);	/* XXX */
	hdr->rate = wpi_plcp_signal(2);
	hdr->id = WPI_ID_BROADCAST;
	hdr->mask = htole32(0xffffffff);
	hdr->esslen = ni->ni_esslen;
	bcopy(ni->ni_essid, hdr->essid, ni->ni_esslen);

	/*
	 * Build a probe request frame.  Most of the following code is a
	 * copy & paste of what is done in net80211.
	 */
	wh = (struct ieee80211_frame *)(hdr + 1);
	wh->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_MGT |
	    IEEE80211_FC0_SUBTYPE_PROBE_REQ;
	wh->i_fc[1] = IEEE80211_FC1_DIR_NODS;
	IEEE80211_ADDR_COPY(wh->i_addr1, etherbroadcastaddr);
	IEEE80211_ADDR_COPY(wh->i_addr2, ic->ic_myaddr);
	IEEE80211_ADDR_COPY(wh->i_addr3, etherbroadcastaddr);
	*(u_int16_t *)&wh->i_dur[0] = 0;	/* filled by h/w */
	*(u_int16_t *)&wh->i_seq[0] = 0;	/* filled by h/w */

	frm = (uint8_t *)(wh + 1);

	/* add essid IE */
	frm = ieee80211_add_ssid(frm, ni->ni_essid, ni->ni_esslen);

	mode = ieee80211_chan2mode(ic, ic->ic_ibss_chan);
	rs = &ic->ic_sup_rates[mode];

	/* add supported rates IE */
	frm = ieee80211_add_rates(frm, rs);

	/* add supported xrates IE */
	frm = ieee80211_add_xrates(frm, rs);

	/* setup length of probe request */
	hdr->length = htole16(frm - (uint8_t *)wh);

	/* XXX: align on a 4-byte boundary? */
	chan = (struct wpi_scan_chan *)frm;
	for (i = 1; i <= hdr->nchan; i++, chan++) {
		chan->flags = 3;
		chan->chan = i;
		chan->magic = htole16(0x62ab);
		chan->active = htole16(20);
		chan->passive = htole16(120);

		frm += sizeof (struct wpi_scan_chan);
	}

	pktlen = frm - mtod(data->m, uint8_t *);

	error = bus_dmamap_load(sc->sc_dmat, data->map, cmd, pktlen,
	    NULL, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not map scan command\n",
		    sc->sc_dev.dv_xname);
		m_freem(data->m);
		data->m = NULL;
		return error;
	}

	desc->flags = htole32(WPI_PAD32(pktlen) << 28 | 1 << 24);
	desc->segs[0].addr = htole32(data->map->dm_segs[0].ds_addr);
	desc->segs[0].len  = htole32(data->map->dm_segs[0].ds_len);

	/* kick cmd ring */
	ring->cur = (ring->cur + 1) % WPI_CMD_RING_COUNT;
	WPI_WRITE(sc, WPI_TX_WIDX, ring->qid << 8 | ring->cur);

	return 0;	/* will be notified async. of failure/success */
}

int
wpi_config(struct wpi_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct wpi_txpower txpower;
	struct wpi_power power;
	struct wpi_bluetooth bluetooth;
	struct wpi_node node;
	int error;

	/* Intel's binary only daemon is a joke.. */

	/* set Tx power for 2.4GHz channels (values read from EEPROM) */
	bzero(&txpower, sizeof txpower);
	bcopy(sc->pwr1, txpower.pwr1, 14 * sizeof (uint16_t));
	bcopy(sc->pwr2, txpower.pwr2, 14 * sizeof (uint16_t));
	error = wpi_cmd(sc, WPI_CMD_TXPOWER, &txpower, sizeof txpower, 0);
	if (error != 0) {
		printf("%s: could not set txpower\n", sc->sc_dev.dv_xname);
		return error;
	}

	/* set power mode */
	bzero(&power, sizeof power);
	power.flags = htole32(0x8);	/* XXX */
	error = wpi_cmd(sc, WPI_CMD_SET_POWER_MODE, &power, sizeof power, 0);
	if (error != 0) {
		printf("%s: could not set power mode\n", sc->sc_dev.dv_xname);
		return error;
	}

	/* configure bluetooth coexistence */
	bzero(&bluetooth, sizeof bluetooth);
	bluetooth.flags = 3;
	bluetooth.lead = 0xaa;
	bluetooth.kill = 1;
	error = wpi_cmd(sc, WPI_CMD_BLUETOOTH, &bluetooth, sizeof bluetooth,
	    0);
	if (error != 0) {
		printf("%s: could not configure bluetooth coexistence\n",
		    sc->sc_dev.dv_xname);
		return error;
	}

	/* configure adapter */
	bzero(&sc->config, sizeof (struct wpi_config));
	IEEE80211_ADDR_COPY(ic->ic_myaddr, LLADDR(ifp->if_sadl));
	IEEE80211_ADDR_COPY(sc->config.myaddr, ic->ic_myaddr);
	sc->config.chan = ieee80211_chan2ieee(ic, ic->ic_ibss_chan);
	sc->config.flags = htole32(WPI_CONFIG_TSF | WPI_CONFIG_AUTO |
	    WPI_CONFIG_24GHZ);
	sc->config.filter = 0;
	switch (ic->ic_opmode) {
	case IEEE80211_M_STA:
		sc->config.mode = WPI_MODE_STA;
		sc->config.filter |= htole32(WPI_FILTER_MULTICAST);
		break;
	case IEEE80211_M_IBSS:
	case IEEE80211_M_AHDEMO:
		sc->config.mode = WPI_MODE_IBSS;
		break;
	case IEEE80211_M_HOSTAP:
		sc->config.mode = WPI_MODE_HOSTAP;
		break;
	case IEEE80211_M_MONITOR:
		sc->config.mode = WPI_MODE_MONITOR;
		sc->config.filter |= htole32(WPI_FILTER_MULTICAST |
		    WPI_FILTER_CTL | WPI_FILTER_PROMISC);
		break;
	}
	sc->config.cck_mask  = 0x0f;	/* not yet negotiated */
	sc->config.ofdm_mask = 0xff;	/* not yet negotiated */
	error = wpi_cmd(sc, WPI_CMD_CONFIGURE, &sc->config,
	    sizeof (struct wpi_config), 0);
	if (error != 0) {
		printf("%s: configure command failed\n", sc->sc_dev.dv_xname);
		return error;
	}

	/* add broadcast node */
	bzero(&node, sizeof node);
	IEEE80211_ADDR_COPY(node.bssid, etherbroadcastaddr);
	node.id = WPI_ID_BROADCAST;
	node.rate = wpi_plcp_signal(2);
	error = wpi_cmd(sc, WPI_CMD_ADD_NODE, &node, sizeof node, 0);
	if (error != 0) {
		printf("%s: could not add broadcast node\n",
		    sc->sc_dev.dv_xname);
		return error;
	}

	return 0;
}

void
wpi_stop_master(struct wpi_softc *sc)
{
	uint32_t tmp;
	int ntries;

	tmp = WPI_READ(sc, WPI_RESET);
	WPI_WRITE(sc, WPI_RESET, tmp | WPI_STOP_MASTER);

	tmp = WPI_READ(sc, WPI_GPIO_CTL);
	if ((tmp & WPI_GPIO_PWR_STATUS) == WPI_GPIO_PWR_SLEEP)
		return;	/* already asleep */

	for (ntries = 0; ntries < 100; ntries++) {
		if (WPI_READ(sc, WPI_RESET) & WPI_MASTER_DISABLED)
			break;
		DELAY(10);
	}
	if (ntries == 100) {
		printf("%s: timeout waiting for master\n",
		    sc->sc_dev.dv_xname);
	}
}

int
wpi_power_up(struct wpi_softc *sc)
{
	uint32_t tmp;
	int ntries;

	wpi_mem_lock(sc);
	tmp = wpi_mem_read(sc, WPI_MEM_POWER);
	wpi_mem_write(sc, WPI_MEM_POWER, tmp & ~0x03000000);
	wpi_mem_unlock(sc);

	for (ntries = 0; ntries < 5000; ntries++) {
		if (WPI_READ(sc, WPI_GPIO_STATUS) & WPI_POWERED)
			break;
		DELAY(10);
	}
	if (ntries == 5000) {
		printf("%s: timeout waiting for NIC to power up\n",
		    sc->sc_dev.dv_xname);
		return ETIMEDOUT;
	}
	return 0;
}

int
wpi_reset(struct wpi_softc *sc)
{
	uint32_t tmp;
	int ntries;

	/* clear any pending interrupts */
	WPI_WRITE(sc, WPI_INTR, 0xffffffff);

	tmp = WPI_READ(sc, WPI_PLL_CTL);
	WPI_WRITE(sc, WPI_PLL_CTL, tmp | WPI_PLL_INIT);

	tmp = WPI_READ(sc, WPI_CHICKEN);
	WPI_WRITE(sc, WPI_CHICKEN, tmp | WPI_CHICKEN_RXNOLOS);

	tmp = WPI_READ(sc, WPI_GPIO_CTL);
	WPI_WRITE(sc, WPI_GPIO_CTL, tmp | WPI_GPIO_INIT);

	/* wait for clock stabilization */
	for (ntries = 0; ntries < 1000; ntries++) {
		if (WPI_READ(sc, WPI_GPIO_CTL) & WPI_GPIO_CLOCK)
			break;
		DELAY(10);
	}
	if (ntries == 1000) {
		printf("%s: timeout waiting for clock stabilization\n",
		    sc->sc_dev.dv_xname);
		return ETIMEDOUT;
	}

	/* initialize EEPROM */
	tmp = WPI_READ(sc, WPI_EEPROM_STATUS);
	if ((tmp & WPI_EEPROM_VERSION) == 0) {
		printf("%s: EEPROM not found\n", sc->sc_dev.dv_xname);
		return EIO;
	}
	WPI_WRITE(sc, WPI_EEPROM_STATUS, tmp & ~WPI_EEPROM_LOCKED);

	return 0;
}

void
wpi_hw_config(struct wpi_softc *sc)
{
	uint16_t val;
	uint32_t rev, hw;

	/* voodoo from the Linux "driver".. */
	hw = WPI_READ(sc, WPI_HWCONFIG);

	rev = pci_conf_read(sc->sc_pct, sc->sc_pcitag, PCI_CLASS_REG);
	rev = PCI_REVISION(rev);
	if ((rev & 0xc0) == 0x40)
		hw |= WPI_HW_ALM_MB;
	else if (!(rev & 0x80))
		hw |= WPI_HW_ALM_MM;

	val = wpi_read_prom_word(sc, WPI_EEPROM_CAPABILITIES);
	if ((val & 0xff) == 0x80)
		hw |= WPI_HW_SKU_MRC;

	val = wpi_read_prom_word(sc, WPI_EEPROM_REVISION);
	hw &= ~WPI_HW_REV_D;
	if ((val & 0xf0) == 0xd0)
		hw |= WPI_HW_REV_D;

	val = wpi_read_prom_word(sc, WPI_EEPROM_TYPE);
	if ((val & 0xff) > 1)
		hw |= WPI_HW_TYPE_B;

	DPRINTF(("setting h/w config %x\n", hw));
	WPI_WRITE(sc, WPI_HWCONFIG, hw);
}

int
wpi_init(struct ifnet *ifp)
{
	struct wpi_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	const struct wpi_firmware_hdr *hdr;
	const char *boot, *text, *data;
	u_char *fw;
	size_t size;
	uint32_t tmp;
	int qid, ntries, error;

	(void)wpi_reset(sc);

	wpi_mem_lock(sc);
	wpi_mem_write(sc, WPI_MEM_CLOCK1, 0xa00);
	DELAY(20);
	tmp = wpi_mem_read(sc, WPI_MEM_PCIDEV);
	wpi_mem_write(sc, WPI_MEM_PCIDEV, tmp | 0x800);
	wpi_mem_unlock(sc);

	(void)wpi_power_up(sc);
	wpi_hw_config(sc);

	/* init Rx ring */
	wpi_mem_lock(sc);
	WPI_WRITE(sc, WPI_RX_BASE, sc->rxq.desc_dma.paddr);
	WPI_WRITE(sc, WPI_RX_RIDX_PTR, sc->shared_dma.paddr +
	    offsetof(struct wpi_shared, next));
	WPI_WRITE(sc, WPI_RX_WIDX, (WPI_RX_RING_COUNT - 1) & ~7);
	WPI_WRITE(sc, WPI_RX_CONFIG, 0xa9601010);
	wpi_mem_unlock(sc);

	/* init Tx rings */
	wpi_mem_lock(sc);
	wpi_mem_write(sc, WPI_MEM_MODE, 2);	/* bypass mode */
	wpi_mem_write(sc, WPI_MEM_RA, 1);	/* enable RA0 */
	wpi_mem_write(sc, WPI_MEM_TXCFG, 0x3f);	/* enable all 6 Tx rings */
	wpi_mem_write(sc, WPI_MEM_BYPASS1, 0x10000);
	wpi_mem_write(sc, WPI_MEM_BYPASS2, 0x30002);
	wpi_mem_write(sc, WPI_MEM_MAGIC4, 4);
	wpi_mem_write(sc, WPI_MEM_MAGIC5, 5);

	WPI_WRITE(sc, WPI_TX_BASE_PTR, sc->shared_dma.paddr);
	WPI_WRITE(sc, WPI_MSG_CONFIG, 0xffff05a5);

	for (qid = 0; qid < 6; qid++) {
		WPI_WRITE(sc, WPI_TX_CTL(qid), 0);
		WPI_WRITE(sc, WPI_TX_BASE(qid), 0);
		WPI_WRITE(sc, WPI_TX_CONFIG(qid), 0x80200008);
	}
	wpi_mem_unlock(sc);

	/* clear "radio off" and "disable command" bits (reversed logic) */
	WPI_WRITE(sc, WPI_UCODE_CLR, WPI_RADIO_OFF);
	WPI_WRITE(sc, WPI_UCODE_CLR, WPI_DISABLE_CMD);

	/* clear any pending interrupts */
	WPI_WRITE(sc, WPI_INTR, 0xffffffff);
	/* enable interrupts */
	WPI_WRITE(sc, WPI_MASK, WPI_INTR_MASK);

	if ((error = loadfirmware("wpi-ucode", &fw, &size)) != 0) {
		printf("%s: could not read firmware file\n",
		    sc->sc_dev.dv_xname);
		goto fail1;
	}

	if (size < sizeof (struct wpi_firmware_hdr)) {
		printf("%s: firmware file too short: %d bytes\n",
		    sc->sc_dev.dv_xname, size);
		error = EINVAL;
		goto fail2;
	}

	hdr = (const struct wpi_firmware_hdr *)fw;
	if (size < sizeof (struct wpi_firmware_hdr) + letoh32(hdr->textsz) +
	    letoh32(hdr->datasz) + letoh32(hdr->bootsz)) {
		printf("%s: firmware file too short: %d bytes\n",
		    sc->sc_dev.dv_xname, size);
		error = EINVAL;
		goto fail2;
	}

	/* firmware image layout: |HDR|<--TEXT-->|<--DATA-->|<--BOOT-->| */
	text = (const char *)(hdr + 1);
	data = text + letoh32(hdr->textsz);
	boot = data + letoh32(hdr->datasz);

	/* load firmware boot code into NIC */
	error = wpi_load_microcode(sc, boot, letoh32(hdr->bootsz));
	if (error != 0) {
		printf("%s: could not load microcode\n", sc->sc_dev.dv_xname);
		goto fail2;
	}

	/* load firmware .text segment into NIC */
	error = wpi_load_firmware(sc, WPI_FW_TEXT, text, letoh32(hdr->textsz));
	if (error != 0) {
		printf("%s: could not load firmware\n", sc->sc_dev.dv_xname);
		goto fail2;
	}

	/* load firmware .data segment into NIC */
	error = wpi_load_firmware(sc, WPI_FW_DATA, data, letoh32(hdr->datasz));
	if (error != 0) {
		printf("%s: could not load firmware\n", sc->sc_dev.dv_xname);
		goto fail2;
	}

	free(fw, M_DEVBUF);

	/* now press "execute" ;-) */
	tmp = WPI_READ(sc, WPI_RESET);
	tmp &= ~(WPI_MASTER_DISABLED | WPI_STOP_MASTER | WPI_NEVO_RESET);
	WPI_WRITE(sc, WPI_RESET, tmp);

	/* ..and wait at most one second for adapter to initialize */
	if ((error = tsleep(sc, PCATCH, "wpiinit", hz)) != 0) {
		printf("%s: timeout waiting for adapter to initialize\n",
		    sc->sc_dev.dv_xname);
		goto fail1;
	}

	/* wait for thermal sensors to calibrate */
	for (ntries = 0; ntries < 1000; ntries++) {
		if (WPI_READ(sc, WPI_TEMPERATURE) != 0)
			break;
		DELAY(10);
	}
	if (ntries == 1000) {
		printf("%s: timeout waiting for thermal sensors calibration\n",
		    sc->sc_dev.dv_xname);
		error = ETIMEDOUT;
		goto fail1;
	}
	DPRINTF(("temperature %d\n", (int)WPI_READ(sc, WPI_TEMPERATURE)));

	if ((error = wpi_config(sc)) != 0) {
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

fail2:	free(fw, M_DEVBUF);
fail1:	wpi_stop(ifp, 1);
	return error;
}

void
wpi_stop(struct ifnet *ifp, int disable)
{
	struct wpi_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	uint32_t tmp;
	int ac;

	ifp->if_timer = sc->sc_tx_timer = 0;
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	ieee80211_new_state(ic, IEEE80211_S_INIT, -1);

	/* disable interrupts */
	WPI_WRITE(sc, WPI_MASK, 0);
	WPI_WRITE(sc, WPI_INTR, WPI_INTR_MASK);
	WPI_WRITE(sc, WPI_INTR_STATUS, 0xff);
	WPI_WRITE(sc, WPI_INTR_STATUS, 0x00070000);

	wpi_mem_lock(sc);
	wpi_mem_write(sc, WPI_MEM_MODE, 0);
	wpi_mem_unlock(sc);

	/* reset all Tx rings */
	for (ac = 0; ac < 4; ac++)
		wpi_reset_tx_ring(sc, &sc->txq[ac]);
	wpi_reset_tx_ring(sc, &sc->cmdq);
	wpi_reset_tx_ring(sc, &sc->svcq);

	/* reset Rx ring */
	wpi_reset_rx_ring(sc, &sc->rxq);

	wpi_mem_lock(sc);
	wpi_mem_write(sc, WPI_MEM_CLOCK2, 0x200);
	wpi_mem_unlock(sc);

	DELAY(5);

	wpi_stop_master(sc);

	tmp = WPI_READ(sc, WPI_RESET);
	WPI_WRITE(sc, WPI_RESET, tmp | WPI_SW_RESET);
}

/*-
 * Naive implementation of the Adaptive Multi Rate Retry algorithm:
 *     "IEEE 802.11 Rate Adaptation: A Practical Approach"
 *     Mathieu Lacage, Hossein Manshaei, Thierry Turletti
 *     INRIA Sophia - Projet Planete
 *     http://www-sop.inria.fr/rapports/sophia/RR-5208.html
 */
#define is_success(amrr)	\
	((amrr)->retrycnt < (amrr)->txcnt / 10)
#define is_failure(amrr)	\
	((amrr)->retrycnt > (amrr)->txcnt / 3)
#define is_enough(amrr)		\
	((amrr)->txcnt > 10)
#define is_min_rate(ni)		\
	((ni)->ni_txrate == 0)
#define is_max_rate(ni)		\
	((ni)->ni_txrate == (ni)->ni_rates.rs_nrates - 1)
#define increase_rate(ni)	\
	((ni)->ni_txrate++)
#define decrease_rate(ni)	\
	((ni)->ni_txrate--)
#define reset_cnt(amrr)		\
	do { (amrr)->txcnt = (amrr)->retrycnt = 0; } while (0)

#define WPI_AMRR_MIN_SUCCESS_THRESHOLD	 1
#define WPI_AMRR_MAX_SUCCESS_THRESHOLD	15

void
wpi_amrr_init(struct wpi_amrr *amrr)
{
	struct ieee80211_node *ni = &amrr->ni;
	int i;

	amrr->success = 0;
	amrr->recovery = 0;
	amrr->txcnt = amrr->retrycnt = 0;
	amrr->success_threshold = WPI_AMRR_MIN_SUCCESS_THRESHOLD;

	/* set rate to some reasonable initial value */
	ni = &amrr->ni;
	for (i = ni->ni_rates.rs_nrates - 1;
	     i > 0 && (ni->ni_rates.rs_rates[i] & IEEE80211_RATE_VAL) > 72;
	     i--);

	ni->ni_txrate = i;
}

void
wpi_amrr_timeout(void *arg)
{
	struct wpi_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;

	if (ic->ic_opmode == IEEE80211_M_STA)
		wpi_amrr_ratectl(NULL, ic->ic_bss);
	else
		ieee80211_iterate_nodes(ic, wpi_amrr_ratectl, NULL);

	timeout_add(&sc->amrr_ch, hz / 2);
}

/* ARGSUSED */
void
wpi_amrr_ratectl(void *arg, struct ieee80211_node *ni)
{
	struct wpi_amrr *amrr = (struct wpi_amrr *)ni;
	int need_change = 0;

	if (is_success(amrr) && is_enough(amrr)) {
		amrr->success++;
		if (amrr->success >= amrr->success_threshold &&
		    !is_max_rate(ni)) {
			amrr->recovery = 1;
			amrr->success = 0;
			increase_rate(ni);
			DPRINTFN(2, ("AMRR increasing rate %d (txcnt=%d "
			    "retrycnt=%d)\n", ni->ni_txrate, amrr->txcnt,
			    amrr->retrycnt));
			need_change = 1;
		} else {
			amrr->recovery = 0;
		}
	} else if (is_failure(amrr)) {
		amrr->success = 0;
		if (!is_min_rate(ni)) {
			if (amrr->recovery) {
				amrr->success_threshold *= 2;
				if (amrr->success_threshold >
				    WPI_AMRR_MAX_SUCCESS_THRESHOLD)
					amrr->success_threshold =
					    WPI_AMRR_MAX_SUCCESS_THRESHOLD;
			} else {
				amrr->success_threshold =
				    WPI_AMRR_MIN_SUCCESS_THRESHOLD;
			}
			decrease_rate(ni);
			DPRINTFN(2, ("AMRR decreasing rate %d (txcnt=%d "
			    "retrycnt=%d)\n", ni->ni_txrate, amrr->txcnt,
			    amrr->retrycnt));
			need_change = 1;
		}
		amrr->recovery = 0;	/* paper is incorrect */
	}

	if (is_enough(amrr) || need_change)
		reset_cnt(amrr);
}

struct cfdriver wpi_cd = {
	NULL, "wpi", DV_IFNET
};
