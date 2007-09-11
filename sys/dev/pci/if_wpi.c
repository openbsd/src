/*	$OpenBSD: if_wpi.c,v 1.56 2007/09/11 18:52:32 damien Exp $	*/

/*-
 * Copyright (c) 2006, 2007
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

#include <dev/pci/if_wpireg.h>
#include <dev/pci/if_wpivar.h>

static const struct pci_matchid wpi_devices[] = {
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_PRO_WL_3945ABG_1 },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_PRO_WL_3945ABG_2 }
};

int		wpi_match(struct device *, void *, void *);
void		wpi_attach(struct device *, struct device *, void *);
void		wpi_sensor_attach(struct wpi_softc *);
void		wpi_radiotap_attach(struct wpi_softc *);
void		wpi_power(int, void *);
int		wpi_dma_contig_alloc(bus_dma_tag_t, struct wpi_dma_info *,
		    void **, bus_size_t, bus_size_t, int);
void		wpi_dma_contig_free(struct wpi_dma_info *);
int		wpi_alloc_shared(struct wpi_softc *);
void		wpi_free_shared(struct wpi_softc *);
int		wpi_alloc_fwmem(struct wpi_softc *);
void		wpi_free_fwmem(struct wpi_softc *);
struct		wpi_rbuf *wpi_alloc_rbuf(struct wpi_softc *);
void		wpi_free_rbuf(caddr_t, u_int, void *);
int		wpi_alloc_rpool(struct wpi_softc *);
void		wpi_free_rpool(struct wpi_softc *);
int		wpi_alloc_rx_ring(struct wpi_softc *, struct wpi_rx_ring *);
void		wpi_reset_rx_ring(struct wpi_softc *, struct wpi_rx_ring *);
void		wpi_free_rx_ring(struct wpi_softc *, struct wpi_rx_ring *);
int		wpi_alloc_tx_ring(struct wpi_softc *, struct wpi_tx_ring *,
		    int, int);
void		wpi_reset_tx_ring(struct wpi_softc *, struct wpi_tx_ring *);
void		wpi_free_tx_ring(struct wpi_softc *, struct wpi_tx_ring *);
struct		ieee80211_node *wpi_node_alloc(struct ieee80211com *);
void		wpi_newassoc(struct ieee80211com *, struct ieee80211_node *,
		    int);
int		wpi_media_change(struct ifnet *);
int		wpi_newstate(struct ieee80211com *, enum ieee80211_state, int);
void		wpi_mem_lock(struct wpi_softc *);
void		wpi_mem_unlock(struct wpi_softc *);
uint32_t	wpi_mem_read(struct wpi_softc *, uint16_t);
void		wpi_mem_write(struct wpi_softc *, uint16_t, uint32_t);
void		wpi_mem_write_region_4(struct wpi_softc *, uint16_t,
		    const uint32_t *, int);
int		wpi_read_prom_data(struct wpi_softc *, uint32_t, void *, int);
int		wpi_load_microcode(struct wpi_softc *, const uint8_t *, int);
int		wpi_load_firmware(struct wpi_softc *);
void		wpi_calib_timeout(void *);
void		wpi_iter_func(void *, struct ieee80211_node *);
void		wpi_power_calibration(struct wpi_softc *, int);
void		wpi_rx_intr(struct wpi_softc *, struct wpi_rx_desc *,
		    struct wpi_rx_data *);
void		wpi_tx_intr(struct wpi_softc *, struct wpi_rx_desc *);
void		wpi_cmd_intr(struct wpi_softc *, struct wpi_rx_desc *);
void		wpi_notif_intr(struct wpi_softc *);
int		wpi_intr(void *);
void		wpi_read_eeprom(struct wpi_softc *);
void		wpi_read_eeprom_channels(struct wpi_softc *, int);
void		wpi_read_eeprom_group(struct wpi_softc *, int);
uint8_t		wpi_plcp_signal(int);
int		wpi_tx_data(struct wpi_softc *, struct mbuf *,
		    struct ieee80211_node *, int);
void		wpi_start(struct ifnet *);
void		wpi_watchdog(struct ifnet *);
int		wpi_ioctl(struct ifnet *, u_long, caddr_t);
int		wpi_cmd(struct wpi_softc *, int, const void *, int, int);
int		wpi_mrr_setup(struct wpi_softc *);
int		wpi_set_key(struct ieee80211com *, struct ieee80211_node *,
		    const struct ieee80211_key *);
void		wpi_updateedca(struct ieee80211com *);
void		wpi_set_led(struct wpi_softc *, uint8_t, uint8_t, uint8_t);
void		wpi_enable_tsf(struct wpi_softc *, struct ieee80211_node *);
int		wpi_set_txpower(struct wpi_softc *,
		    struct ieee80211_channel *, int);
int		wpi_get_power_index(struct wpi_softc *,
		    struct wpi_power_group *, struct ieee80211_channel *, int);
int		wpi_auth(struct wpi_softc *);
int		wpi_scan(struct wpi_softc *, uint16_t);
int		wpi_config(struct wpi_softc *);
void		wpi_stop_master(struct wpi_softc *);
int		wpi_power_up(struct wpi_softc *);
int		wpi_reset(struct wpi_softc *);
void		wpi_hw_config(struct wpi_softc *);
int		wpi_init(struct ifnet *);
void		wpi_stop(struct ifnet *, int);

#ifdef WPI_DEBUG
#define DPRINTF(x)	do { if (wpi_debug > 0) printf x; } while (0)
#define DPRINTFN(n, x)	do { if (wpi_debug >= (n)) printf x; } while (0)
int wpi_debug = 1;
#else
#define DPRINTF(x)
#define DPRINTFN(n, x)
#endif

struct cfattach wpi_ca = {
	sizeof (struct wpi_softc), wpi_match, wpi_attach
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
	int i, error;

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
	 * Allocate DMA memory for firmware transfers.
	 */
	if ((error = wpi_alloc_fwmem(sc)) != 0) {
		printf(": could not allocate firmware memory\n");
		return;
	}

	/*
	 * Allocate shared page and Tx/Rx rings.
	 */
	if ((error = wpi_alloc_shared(sc)) != 0) {
		printf(": could not allocate shared area\n");
		goto fail1;
	}

	if ((error = wpi_alloc_rpool(sc)) != 0) {
		printf(": could not allocate Rx buffers\n");
		goto fail2;
	}

	for (i = 0; i < WPI_NTXQUEUES; i++) {
		struct wpi_tx_ring *txq = &sc->txq[i];
		error = wpi_alloc_tx_ring(sc, txq, WPI_TX_RING_COUNT, i);
		if (error != 0) {
			printf(": could not allocate Tx ring %d\n", i);
			goto fail3;
		}
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

	/* read supported channels and MAC address from EEPROM */
	wpi_read_eeprom(sc);

	/* set supported .11a, .11b and .11g rates */
	ic->ic_sup_rates[IEEE80211_MODE_11A] = ieee80211_std_rateset_11a;
	ic->ic_sup_rates[IEEE80211_MODE_11B] = ieee80211_std_rateset_11b;
	ic->ic_sup_rates[IEEE80211_MODE_11G] = ieee80211_std_rateset_11g;

	/* IBSS channel undefined for now */
	ic->ic_ibss_chan = &ic->ic_channels[0];

	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_init = wpi_init;
	ifp->if_ioctl = wpi_ioctl;
	ifp->if_start = wpi_start;
	ifp->if_watchdog = wpi_watchdog;
	IFQ_SET_READY(&ifp->if_snd);
	memcpy(ifp->if_xname, sc->sc_dev.dv_xname, IFNAMSIZ);

	if_attach(ifp);
	ieee80211_ifattach(ifp);
	ic->ic_node_alloc = wpi_node_alloc;
	ic->ic_newassoc = wpi_newassoc;
	ic->ic_set_key = wpi_set_key;
	ic->ic_updateedca = wpi_updateedca;

	/* override state transition machine */
	sc->sc_newstate = ic->ic_newstate;
	ic->ic_newstate = wpi_newstate;
	ieee80211_media_init(ifp, wpi_media_change, ieee80211_media_status);

	sc->amrr.amrr_min_success_threshold =  1;
	sc->amrr.amrr_max_success_threshold = 15;

	wpi_sensor_attach(sc);
	wpi_radiotap_attach(sc);

	timeout_set(&sc->calib_to, wpi_calib_timeout, sc);

	sc->powerhook = powerhook_establish(wpi_power, sc);

	return;

	/* free allocated memory if something failed during attachment */
fail3:	while (--i >= 0)
		wpi_free_tx_ring(sc, &sc->txq[i]);
	wpi_free_rpool(sc);
fail2:	wpi_free_shared(sc);
fail1:	wpi_free_fwmem(sc);
}

/*
 * Attach the adapter's on-board thermal sensor to the sensors framework.
 */
void
wpi_sensor_attach(struct wpi_softc *sc)
{
	strlcpy(sc->sensordev.xname, sc->sc_dev.dv_xname,
	    sizeof sc->sensordev.xname);
	strlcpy(sc->sensor.desc, "temperature 0 - 285",
	    sizeof sc->sensor.desc);
	sc->sensor.type = SENSOR_INTEGER;	/* not in muK! */
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
wpi_radiotap_attach(struct wpi_softc *sc)
{
#if NBPFILTER > 0
	bpfattach(&sc->sc_drvbpf, &sc->sc_ic.ic_if, DLT_IEEE802_11_RADIO,
	    sizeof (struct ieee80211_frame) + IEEE80211_RADIOTAP_HDRLEN);

	sc->sc_rxtap_len = sizeof sc->sc_rxtapu;
	sc->sc_rxtap.wr_ihdr.it_len = htole16(sc->sc_rxtap_len);
	sc->sc_rxtap.wr_ihdr.it_present = htole32(WPI_RX_RADIOTAP_PRESENT);

	sc->sc_txtap_len = sizeof sc->sc_txtapu;
	sc->sc_txtap.wt_ihdr.it_len = htole16(sc->sc_txtap_len);
	sc->sc_txtap.wt_ihdr.it_present = htole32(WPI_TX_RADIOTAP_PRESENT);
#endif
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
wpi_dma_contig_alloc(bus_dma_tag_t tag, struct wpi_dma_info *dma, void **kvap,
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

fail:	wpi_dma_contig_free(dma);
	return error;
}

void
wpi_dma_contig_free(struct wpi_dma_info *dma)
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

/*
 * Allocate a shared page between host and NIC.
 */
int
wpi_alloc_shared(struct wpi_softc *sc)
{
	/* must be aligned on a 4K-page boundary */
	return wpi_dma_contig_alloc(sc->sc_dmat, &sc->shared_dma,
	    (void **)&sc->shared, sizeof (struct wpi_shared), PAGE_SIZE,
	    BUS_DMA_NOWAIT);
}

void
wpi_free_shared(struct wpi_softc *sc)
{
	wpi_dma_contig_free(&sc->shared_dma);
}

/*
 * Allocate DMA-safe memory for firmware transfer.
 */
int
wpi_alloc_fwmem(struct wpi_softc *sc)
{
	/* allocate enough contiguous space to store text and data */
	return wpi_dma_contig_alloc(sc->sc_dmat, &sc->fw_dma, NULL,
	    WPI_FW_MAIN_TEXT_MAXSZ + WPI_FW_MAIN_DATA_MAXSZ, 0,
	    BUS_DMA_NOWAIT);
}

void
wpi_free_fwmem(struct wpi_softc *sc)
{
	wpi_dma_contig_free(&sc->fw_dma);
}

struct wpi_rbuf *
wpi_alloc_rbuf(struct wpi_softc *sc)
{
	struct wpi_rbuf *rbuf;

	rbuf = SLIST_FIRST(&sc->rxq.freelist);
	if (rbuf == NULL)
		return NULL;
	SLIST_REMOVE_HEAD(&sc->rxq.freelist, next);
	return rbuf;
}

/*
 * This is called automatically by the network stack when the mbuf to which our
 * Rx buffer is attached is freed.
 */
void
wpi_free_rbuf(caddr_t buf, u_int size, void *arg)
{
	struct wpi_rbuf *rbuf = arg;
	struct wpi_softc *sc = rbuf->sc;

	/* put the buffer back in the free list */
	SLIST_INSERT_HEAD(&sc->rxq.freelist, rbuf, next);
}

int
wpi_alloc_rpool(struct wpi_softc *sc)
{
	struct wpi_rx_ring *ring = &sc->rxq;
	int i, error;

	/* allocate a big chunk of DMA'able memory.. */
	error = wpi_dma_contig_alloc(sc->sc_dmat, &ring->buf_dma, NULL,
	    WPI_RBUF_COUNT * WPI_RBUF_SIZE, PAGE_SIZE, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not allocate Rx buffers DMA memory\n",
		    sc->sc_dev.dv_xname);
		return error;
	}

	/* ..and split it into 3KB chunks */
	SLIST_INIT(&ring->freelist);
	for (i = 0; i < WPI_RBUF_COUNT; i++) {
		struct wpi_rbuf *rbuf = &ring->rbuf[i];

		rbuf->sc = sc;	/* backpointer for callbacks */
		rbuf->vaddr = ring->buf_dma.vaddr + i * WPI_RBUF_SIZE;
		rbuf->paddr = ring->buf_dma.paddr + i * WPI_RBUF_SIZE;

		SLIST_INSERT_HEAD(&ring->freelist, rbuf, next);
	}
	return 0;
}

void
wpi_free_rpool(struct wpi_softc *sc)
{
	wpi_dma_contig_free(&sc->rxq.buf_dma);
}

int
wpi_alloc_rx_ring(struct wpi_softc *sc, struct wpi_rx_ring *ring)
{
	int i, error;

	ring->cur = 0;

	error = wpi_dma_contig_alloc(sc->sc_dmat, &ring->desc_dma,
	    (void **)&ring->desc, WPI_RX_RING_COUNT * sizeof (uint32_t),
	    WPI_RING_DMA_ALIGN, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not allocate rx ring DMA memory\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	/*
	 * Setup Rx buffers.
	 */
	for (i = 0; i < WPI_RX_RING_COUNT; i++) {
		struct wpi_rx_data *data = &ring->data[i];
		struct wpi_rbuf *rbuf;

		MGETHDR(data->m, M_DONTWAIT, MT_DATA);
		if (data->m == NULL) {
			printf("%s: could not allocate rx mbuf\n",
			    sc->sc_dev.dv_xname);
			error = ENOMEM;
			goto fail;
		}
		if ((rbuf = wpi_alloc_rbuf(sc)) == NULL) {
			m_freem(data->m);
			data->m = NULL;
			printf("%s: could not allocate rx buffer\n",
			    sc->sc_dev.dv_xname);
			error = ENOMEM;
			goto fail;
		}
		/* attach Rx buffer to mbuf */
		MEXTADD(data->m, rbuf->vaddr, WPI_RBUF_SIZE, 0, wpi_free_rbuf,
		    rbuf);

		ring->desc[i] = htole32(rbuf->paddr);
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
	int i;

	wpi_dma_contig_free(&ring->desc_dma);

	for (i = 0; i < WPI_RX_RING_COUNT; i++) {
		if (ring->data[i].m != NULL)
			m_freem(ring->data[i].m);
	}
}

int
wpi_alloc_tx_ring(struct wpi_softc *sc, struct wpi_tx_ring *ring, int count,
    int qid)
{
	int i, error;

	ring->qid = qid;
	ring->count = count;
	ring->queued = 0;
	ring->cur = 0;

	error = wpi_dma_contig_alloc(sc->sc_dmat, &ring->desc_dma,
	    (void **)&ring->desc, count * sizeof (struct wpi_tx_desc),
	    WPI_RING_DMA_ALIGN, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not allocate tx ring DMA memory\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	/* update shared page with ring's base address */
	sc->shared->txbase[qid] = htole32(ring->desc_dma.paddr);

	error = wpi_dma_contig_alloc(sc->sc_dmat, &ring->cmd_dma,
	    (void **)&ring->cmd, count * sizeof (struct wpi_tx_cmd), 4,
	    BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not allocate tx cmd DMA memory\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	ring->data = malloc(count * sizeof (struct wpi_tx_data), M_DEVBUF,
	    M_NOWAIT | M_ZERO);
	if (ring->data == NULL) {
		printf("%s: could not allocate tx data slots\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	for (i = 0; i < count; i++) {
		struct wpi_tx_data *data = &ring->data[i];

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
	uint32_t tmp;
	int i, ntries;

	wpi_mem_lock(sc);

	WPI_WRITE(sc, WPI_TX_CONFIG(ring->qid), 0);
	for (ntries = 0; ntries < 100; ntries++) {
		tmp = WPI_READ(sc, WPI_TX_STATUS);
		if ((tmp & WPI_TX_IDLE(ring->qid)) == WPI_TX_IDLE(ring->qid))
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
		struct wpi_tx_data *data = &ring->data[i];

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
	int i;

	wpi_dma_contig_free(&ring->desc_dma);
	wpi_dma_contig_free(&ring->cmd_dma);

	if (ring->data != NULL) {
		for (i = 0; i < ring->count; i++) {
			struct wpi_tx_data *data = &ring->data[i];

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
	return malloc(sizeof (struct wpi_node), M_DEVBUF, M_NOWAIT | M_ZERO);
}

void
wpi_newassoc(struct ieee80211com *ic, struct ieee80211_node *ni, int isnew)
{
	struct wpi_softc *sc = ic->ic_if.if_softc;
	int i;

	ieee80211_amrr_node_init(&sc->amrr, &((struct wpi_node *)ni)->amn);

	/* set rate to some reasonable initial value */
	for (i = ni->ni_rates.rs_nrates - 1;
	     i > 0 && (ni->ni_rates.rs_rates[i] & IEEE80211_RATE_VAL) > 72;
	     i--);
	ni->ni_txrate = i;
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
	struct ieee80211_node *ni;
	int error;

	timeout_del(&sc->calib_to);

	if (ic->ic_state == IEEE80211_S_SCAN)
		ic->ic_scan_lock = IEEE80211_SCAN_UNLOCKED;

	switch (nstate) {
	case IEEE80211_S_SCAN:
		/* make the link LED blink while we're scanning */
		wpi_set_led(sc, WPI_LED_LINK, 20, 2);

		if ((error = wpi_scan(sc, IEEE80211_CHAN_G)) != 0) {
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
		sc->config.filter &= ~htole32(WPI_FILTER_BSS);

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
		ni = ic->ic_bss;

		wpi_enable_tsf(sc, ni);

		/* update adapter's configuration */
		sc->config.associd = htole16(ni->ni_associd & ~0xc000);
		/* short preamble/slot time are negotiated when associating */
		sc->config.flags &= ~htole32(WPI_CONFIG_SHPREAMBLE |
		    WPI_CONFIG_SHSLOT);
		if (ic->ic_flags & IEEE80211_F_SHSLOT)
			sc->config.flags |= htole32(WPI_CONFIG_SHSLOT);
		if (ic->ic_flags & IEEE80211_F_SHPREAMBLE)
			sc->config.flags |= htole32(WPI_CONFIG_SHPREAMBLE);
		sc->config.filter |= htole32(WPI_FILTER_BSS);

		DPRINTF(("config chan %d flags %x\n", sc->config.chan,
		    sc->config.flags));
		error = wpi_cmd(sc, WPI_CMD_CONFIGURE, &sc->config,
		    sizeof (struct wpi_config), 1);
		if (error != 0) {
			printf("%s: could not update configuration\n",
			    sc->sc_dev.dv_xname);
			return error;
		}

		/* configuration has changed, set Tx power accordingly */
		if ((error = wpi_set_txpower(sc, ni->ni_chan, 1)) != 0) {
			printf("%s: could not set Tx power\n",
			    sc->sc_dev.dv_xname);
			return error;
		}

		if (ic->ic_opmode == IEEE80211_M_STA) {
			/* fake a join to init the tx rate */
			wpi_newassoc(ic, ni, 1);
		}

		/* start periodic calibration timer */
		sc->calib_cnt = 0;
		timeout_add(&sc->calib_to, hz / 2);

		/* link LED always on while associated */
		wpi_set_led(sc, WPI_LED_LINK, 0, 1);
		break;

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
 * Read `len' bytes from the EEPROM.  We access the EEPROM through the MAC
 * instead of using the traditional bit-bang method.
 */
int
wpi_read_prom_data(struct wpi_softc *sc, uint32_t addr, void *data, int len)
{
	uint8_t *out = data;
	uint32_t val;
	int ntries;

	wpi_mem_lock(sc);
	for (; len > 0; len -= 2, addr++) {
		WPI_WRITE(sc, WPI_EEPROM_CTL, addr << 2);

		for (ntries = 0; ntries < 10; ntries++) {
			if ((val = WPI_READ(sc, WPI_EEPROM_CTL)) &
			    WPI_EEPROM_READY)
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
	wpi_mem_unlock(sc);

	return 0;
}

/*
 * The firmware boot code is small and is intended to be copied directly into
 * the NIC internal memory.
 */
int
wpi_load_microcode(struct wpi_softc *sc, const uint8_t *ucode, int size)
{
	int ntries;

	size /= sizeof (uint32_t);

	wpi_mem_lock(sc);

	/* copy microcode image into NIC memory */
	wpi_mem_write_region_4(sc, WPI_MEM_UCODE_BASE,
	    (const uint32_t *)ucode, size);

	wpi_mem_write(sc, WPI_MEM_UCODE_SRC, 0);
	wpi_mem_write(sc, WPI_MEM_UCODE_DST, WPI_FW_TEXT);
	wpi_mem_write(sc, WPI_MEM_UCODE_SIZE, size);

	/* run microcode */
	wpi_mem_write(sc, WPI_MEM_UCODE_CTL, WPI_UC_RUN);

	/* wait for transfer to complete */
	for (ntries = 0; ntries < 1000; ntries++) {
		if (!(wpi_mem_read(sc, WPI_MEM_UCODE_CTL) & WPI_UC_RUN))
			break;
		DELAY(10);
	}
	if (ntries == 1000) {
		wpi_mem_unlock(sc);
		printf("%s: could not load boot firmware\n",
		    sc->sc_dev.dv_xname);
		return ETIMEDOUT;
	}
	wpi_mem_write(sc, WPI_MEM_UCODE_CTL, WPI_UC_ENABLE);

	wpi_mem_unlock(sc);

	return 0;
}

int
wpi_load_firmware(struct wpi_softc *sc)
{
	struct wpi_dma_info *dma = &sc->fw_dma;
	const struct wpi_firmware_hdr *hdr;
	const uint8_t *init_text, *init_data, *main_text, *main_data;
	const uint8_t *boot_text;
	uint32_t init_textsz, init_datasz, main_textsz, main_datasz;
	uint32_t boot_textsz;
	u_char *fw;
	size_t size;
	int error;

	/* load firmware image from disk */
	if ((error = loadfirmware("wpi-3945abg", &fw, &size)) != 0) {
		printf("%s: error, %d, could not read firmware %s\n",
		    sc->sc_dev.dv_xname, error, "wpi-3945abg");
		goto fail1;
	}

	/* extract firmware header information */
	if (size < sizeof (struct wpi_firmware_hdr)) {
		printf("%s: truncated firmware header: %d bytes\n",
		    sc->sc_dev.dv_xname, size);
		error = EINVAL;
		goto fail2;
	}
	hdr = (const struct wpi_firmware_hdr *)fw;
	main_textsz = letoh32(hdr->main_textsz);
	main_datasz = letoh32(hdr->main_datasz);
	init_textsz = letoh32(hdr->init_textsz);
	init_datasz = letoh32(hdr->init_datasz);
	boot_textsz = letoh32(hdr->boot_textsz);

	/* sanity-check firmware segments sizes */
	if (main_textsz > WPI_FW_MAIN_TEXT_MAXSZ ||
	    main_datasz > WPI_FW_MAIN_DATA_MAXSZ ||
	    init_textsz > WPI_FW_INIT_TEXT_MAXSZ ||
	    init_datasz > WPI_FW_INIT_DATA_MAXSZ ||
	    boot_textsz > WPI_FW_BOOT_TEXT_MAXSZ ||
	    (boot_textsz & 3) != 0) {
		printf("%s: invalid firmware header\n", sc->sc_dev.dv_xname);
		error = EINVAL;
		goto fail2;
	}

	/* check that all firmware segments are present */
	if (size < sizeof (struct wpi_firmware_hdr) + main_textsz +
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
	memcpy(dma->vaddr + WPI_FW_INIT_DATA_MAXSZ, init_text, init_textsz);

	/* tell adapter where to find initialization images */
	wpi_mem_lock(sc);
	wpi_mem_write(sc, WPI_MEM_DATA_BASE, dma->paddr);
	wpi_mem_write(sc, WPI_MEM_DATA_SIZE, init_datasz);
	wpi_mem_write(sc, WPI_MEM_TEXT_BASE,
	    dma->paddr + WPI_FW_INIT_DATA_MAXSZ);
	wpi_mem_write(sc, WPI_MEM_TEXT_SIZE, init_textsz);
	wpi_mem_unlock(sc);

	/* load firmware boot code */
	if ((error = wpi_load_microcode(sc, boot_text, boot_textsz)) != 0) {
		printf("%s: could not load boot firmware\n",
		    sc->sc_dev.dv_xname);
		goto fail2;
	}

	/* now press "execute" ;-) */
	WPI_WRITE(sc, WPI_RESET, 0);

	/* wait at most one second for first alive notification */
	if ((error = tsleep(sc, PCATCH, "wpiinit", hz)) != 0) {
		/* this isn't what was supposed to happen.. */
		printf("%s: timeout waiting for adapter to initialize\n",
		    sc->sc_dev.dv_xname);
		goto fail2;
	}

	/* copy runtime images into pre-allocated DMA-safe memory */
	memcpy(dma->vaddr, main_data, main_datasz);
	memcpy(dma->vaddr + WPI_FW_MAIN_DATA_MAXSZ, main_text, main_textsz);

	/* tell adapter where to find runtime images */
	wpi_mem_lock(sc);
	wpi_mem_write(sc, WPI_MEM_DATA_BASE, dma->paddr);
	wpi_mem_write(sc, WPI_MEM_DATA_SIZE, main_datasz);
	wpi_mem_write(sc, WPI_MEM_TEXT_BASE,
	    dma->paddr + WPI_FW_MAIN_DATA_MAXSZ);
	wpi_mem_write(sc, WPI_MEM_TEXT_SIZE, WPI_FW_UPDATED | main_textsz);
	wpi_mem_unlock(sc);

	/* wait at most one second for second alive notification */
	if ((error = tsleep(sc, PCATCH, "wpiinit", hz)) != 0) {
		/* this isn't what was supposed to happen.. */
		printf("%s: timeout waiting for adapter to initialize\n",
		    sc->sc_dev.dv_xname);
	}

fail2:	free(fw, M_DEVBUF);
fail1:	return error;
}

void
wpi_calib_timeout(void *arg)
{
	struct wpi_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	int temp, s;

	/* automatic rate control triggered every 500ms */
	if (ic->ic_fixed_rate == -1) {
		s = splnet();
		if (ic->ic_opmode == IEEE80211_M_STA)
			wpi_iter_func(sc, ic->ic_bss);
		else
			ieee80211_iterate_nodes(ic, wpi_iter_func, sc);
		splx(s);
	}

	/* update sensor data */
	temp = (int)WPI_READ(sc, WPI_TEMPERATURE);
	sc->sensor.value = temp + 260;

	/* automatic power calibration every 60s */
	if (++sc->calib_cnt >= 120) {
		wpi_power_calibration(sc, temp);
		sc->calib_cnt = 0;
	}

	timeout_add(&sc->calib_to, hz / 2);
}

void
wpi_iter_func(void *arg, struct ieee80211_node *ni)
{
	struct wpi_softc *sc = arg;
	struct wpi_node *wn = (struct wpi_node *)ni;

	ieee80211_amrr_choose(&sc->amrr, ni, &wn->amn);
}

/*
 * This function is called periodically (every 60 seconds) to adjust output
 * power to temperature changes.
 */
void
wpi_power_calibration(struct wpi_softc *sc, int temp)
{
	/* sanity-check read value */
	if (temp < -260 || temp > 25) {
		/* this can't be correct, ignore */
		DPRINTF(("out-of-range temperature reported: %d\n", temp));
		return;
	}

	DPRINTF(("temperature %d->%d\n", sc->temp, temp));

	/* adjust Tx power if need be */
	if (abs(temp - sc->temp) <= 6)
		return;

	sc->temp = temp;

	if (wpi_set_txpower(sc, sc->sc_ic.ic_bss->ni_chan, 1) != 0) {
		/* just warn, too bad for the automatic calibration... */
		printf("%s: could not adjust Tx power\n",
		    sc->sc_dev.dv_xname);
	}
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
	struct wpi_rbuf *rbuf;
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni;
	struct mbuf *m, *mnew;

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

	/*
	 * Discard Rx frames with bad CRC early (XXX we may want to pass them
	 * to radiotap in monitor mode).
	 */
	if ((letoh32(tail->flags) & WPI_RX_NOERROR) != WPI_RX_NOERROR) {
		DPRINTFN(2, ("rx tail flags error %x\n",
		    letoh32(tail->flags)));
		ifp->if_ierrors++;
		return;
	}

	m = data->m;
	/* finalize mbuf */
	m->m_pkthdr.rcvif = ifp;
	m->m_data = (caddr_t)(head + 1);
	m->m_pkthdr.len = m->m_len = letoh16(head->len);

	if ((rbuf = SLIST_FIRST(&sc->rxq.freelist)) != NULL) {
		MGETHDR(mnew, M_DONTWAIT, MT_DATA);
		if (mnew == NULL) {
			ifp->if_ierrors++;
			return;
		}

		/* attach Rx buffer to mbuf */
		MEXTADD(mnew, rbuf->vaddr, WPI_RBUF_SIZE, 0, wpi_free_rbuf,
		    rbuf);
		SLIST_REMOVE_HEAD(&sc->rxq.freelist, next);

		data->m = mnew;

		/* update Rx descriptor */
		ring->desc[ring->cur] = htole32(rbuf->paddr);
	} else {
		/* no free rbufs, copy frame */
		m = m_copym2(m, 0, M_COPYALL, M_DONTWAIT);
		if (m == NULL) {
			/* no free mbufs either, drop frame */
			ifp->if_ierrors++;
			return;
		}
	}

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
	ieee80211_input(ifp, m, ni, stat->rssi, 0);

	/* node is no longer needed */
	ieee80211_release_node(ic, ni);
}

void
wpi_tx_intr(struct wpi_softc *sc, struct wpi_rx_desc *desc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct wpi_tx_ring *ring = &sc->txq[desc->qid & 0x3];
	struct wpi_tx_data *data = &ring->data[desc->idx];
	struct wpi_tx_stat *stat = (struct wpi_tx_stat *)(desc + 1);
	struct wpi_node *wn = (struct wpi_node *)data->ni;

	DPRINTFN(4, ("tx done: qid=%d idx=%d retries=%d nkill=%d rate=%x "
	    "duration=%d status=%x\n", desc->qid, desc->idx, stat->ntries,
	    stat->nkill, stat->rate, letoh32(stat->duration),
	    letoh32(stat->status)));

	/*
	 * Update rate control statistics for the node.
	 * XXX we should not count mgmt frames since they're always sent at
	 * the lowest available bit-rate.
	 */
	wn->amn.amn_txcnt++;
	if (stat->ntries > 0) {
		DPRINTFN(3, ("tx intr ntries %d\n", stat->ntries));
		wn->amn.amn_retrycnt++;
	}

	if ((letoh32(stat->status) & 0xff) != 1)
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
wpi_cmd_intr(struct wpi_softc *sc, struct wpi_rx_desc *desc)
{
	struct wpi_tx_ring *ring = &sc->txq[4];
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
	uint32_t hw;

	hw = letoh32(sc->shared->next);
	while (sc->rxq.cur != hw) {
		struct wpi_rx_data *data = &sc->rxq.data[sc->rxq.cur];
		struct wpi_rx_desc *desc = (void *)data->m->m_ext.ext_buf;

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
			wpi_tx_intr(sc, desc);
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
				printf("%s: microcontroller initialization "
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
				/* turn the interface down */
				ifp->if_flags &= ~IFF_UP;
				wpi_stop(ifp, 1);
				return;	/* no further processing */
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
		{
			struct wpi_stop_scan *scan =
			    (struct wpi_stop_scan *)(desc + 1);

			DPRINTF(("scan finished nchan=%d status=%d chan=%d\n",
			    scan->nchan, scan->status, scan->chan));

			if (scan->status == 1 && scan->chan <= 14) {
				/*
				 * We just finished scanning 802.11g channels,
				 * start scanning 802.11a ones.
				 */
				if (wpi_scan(sc, IEEE80211_CHAN_A) == 0)
					break;
			}
			ieee80211_end_scan(ifp);
			break;
		}
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
		/* SYSTEM FAILURE, SYSTEM FAILURE */
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
	if (ifp->if_flags & IFF_UP)
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

/* quickly determine if a given rate is CCK or OFDM */
#define WPI_RATE_IS_OFDM(rate) ((rate) >= 12 && (rate) != 22)

int
wpi_tx_data(struct wpi_softc *sc, struct mbuf *m0, struct ieee80211_node *ni,
    int ac)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct wpi_tx_ring *ring = &sc->txq[ac];
	struct wpi_tx_desc *desc;
	struct wpi_tx_data *data;
	struct wpi_tx_cmd *cmd;
	struct wpi_cmd_data *tx;
	struct ieee80211_frame *wh;
	struct mbuf *mnew;
	u_int hdrlen;
	int i, rate, error, ovhd = 0;

	desc = &ring->desc[ring->cur];
	data = &ring->data[ring->cur];

	wh = mtod(m0, struct ieee80211_frame *);
	hdrlen = ieee80211_get_hdrlen(wh);

	/* pickup a rate */
	if (IEEE80211_IS_MULTICAST(wh->i_addr1) ||
	    ((wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) ==
	     IEEE80211_FC0_TYPE_MGT)) {
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
		struct wpi_tx_radiotap_header *tap = &sc->sc_txtap;

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
	cmd->code = WPI_CMD_TX_DATA;
	cmd->flags = 0;
	cmd->qid = ring->qid;
	cmd->idx = ring->cur;

	tx = (struct wpi_cmd_data *)cmd->data;
	/* no need to zero tx, all fields are reinitialized here */
	tx->flags = 0;

	if (wh->i_fc[1] & IEEE80211_FC1_PROTECTED) {
		const struct ieee80211_key *key =
		    &ic->ic_nw_keys[ic->ic_wep_txkey];
		if (key->k_cipher == IEEE80211_CIPHER_WEP40)
			tx->security = WPI_CIPHER_WEP40;
		else
			tx->security = WPI_CIPHER_WEP104;
		tx->security |= ic->ic_wep_txkey << 6;
		memcpy(&tx->key[3], key->k_key, key->k_len);
		/* compute crypto overhead */
		ovhd = IEEE80211_WEP_TOTLEN;
	} else
		tx->security = 0;

	if (!IEEE80211_IS_MULTICAST(wh->i_addr1)) {
		tx->id = WPI_ID_BSS;
		tx->flags |= htole32(WPI_TX_NEED_ACK);
	} else
		tx->id = WPI_ID_BROADCAST;

	/* check if RTS/CTS or CTS-to-self protection must be used */
	if (!IEEE80211_IS_MULTICAST(wh->i_addr1)) {
		/* multicast frames are not sent at OFDM rates in 802.11b/g */
		if (m0->m_pkthdr.len + ovhd + IEEE80211_CRC_LEN >
		    ic->ic_rtsthreshold) {
			tx->flags |= htole32(WPI_TX_NEED_RTS |
			    WPI_TX_FULL_TXOP);
		} else if ((ic->ic_flags & IEEE80211_F_USEPROT) &&
		    WPI_RATE_IS_OFDM(rate)) {
			if (ic->ic_protmode == IEEE80211_PROT_CTSONLY) {
				tx->flags |= htole32(WPI_TX_NEED_CTS |
				    WPI_TX_FULL_TXOP);
			} else if (ic->ic_protmode == IEEE80211_PROT_RTSCTS) {
				tx->flags |= htole32(WPI_TX_NEED_RTS |
				    WPI_TX_FULL_TXOP);
			}
		}
	}

	tx->flags |= htole32(WPI_TX_AUTO_SEQ);

	if ((wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) ==
	    IEEE80211_FC0_TYPE_MGT) {
		uint8_t subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;

		/* tell h/w to set timestamp in probe responses */
		if (subtype == IEEE80211_FC0_SUBTYPE_PROBE_RESP)
			tx->flags |= htole32(WPI_TX_INSERT_TSTAMP);

		if (subtype == IEEE80211_FC0_SUBTYPE_ASSOC_REQ ||
		    subtype == IEEE80211_FC0_SUBTYPE_REASSOC_REQ)
			tx->timeout = htole16(3);
		else
			tx->timeout = htole16(2);
	} else
		tx->timeout = htole16(0);

	tx->len = htole16(m0->m_pkthdr.len);
	tx->rate = wpi_plcp_signal(rate);
	tx->rts_ntries = 7;
	tx->data_ntries = 15;
	tx->ofdm_mask = 0xff;
	tx->cck_mask = 0x0f;
	tx->lifetime = htole32(WPI_LIFETIME_INFINITE);

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

	/* first scatter/gather segment is used by the tx data command */
	desc->flags = htole32(WPI_PAD32(m0->m_pkthdr.len) << 28 |
	    (1 + data->map->dm_nsegs) << 24);
	desc->segs[0].addr = htole32(ring->cmd_dma.paddr +
	    ring->cur * sizeof (struct wpi_tx_cmd));
	desc->segs[0].len  = htole32(4 + sizeof (struct wpi_cmd_data) +
	    ((hdrlen + 3) & ~3));
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
			ifp->if_flags &= ~IFF_UP;
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

void
wpi_read_eeprom(struct wpi_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	char domain[4];
	int i;

	wpi_read_prom_data(sc, WPI_EEPROM_CAPABILITIES, &sc->cap, 1);
	wpi_read_prom_data(sc, WPI_EEPROM_REVISION, &sc->rev, 2);
	wpi_read_prom_data(sc, WPI_EEPROM_TYPE, &sc->type, 1);

	DPRINTF(("cap=%x rev=%x type=%x\n", sc->cap, letoh16(sc->rev),
	    sc->type));

	/* read and print regulatory domain */
	wpi_read_prom_data(sc, WPI_EEPROM_DOMAIN, domain, 4);
	printf(", %.4s", domain);

	/* read and print MAC address */
	wpi_read_prom_data(sc, WPI_EEPROM_MAC, ic->ic_myaddr, 6);
	printf(", address %s\n", ether_sprintf(ic->ic_myaddr));

	/* read the list of authorized channels */
	for (i = 0; i < WPI_CHAN_BANDS_COUNT; i++)
		wpi_read_eeprom_channels(sc, i);

	/* read the list of power groups */
	for (i = 0; i < WPI_POWER_GROUPS_COUNT; i++)
		wpi_read_eeprom_group(sc, i);
}

void
wpi_read_eeprom_channels(struct wpi_softc *sc, int n)
{
	struct ieee80211com *ic = &sc->sc_ic;
	const struct wpi_chan_band *band = &wpi_bands[n];
	struct wpi_eeprom_chan channels[WPI_MAX_CHAN_PER_BAND];
	int chan, i;

	wpi_read_prom_data(sc, band->addr, channels,
	    band->nchan * sizeof (struct wpi_eeprom_chan));

	for (i = 0; i < band->nchan; i++) {
		if (!(channels[i].flags & WPI_EEPROM_CHAN_VALID))
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
			 * Some 3945ABG adapters support channels 7, 8, 11
			 * and 12 in the 2GHz *and* 5GHz bands.
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
		if (!(channels[i].flags & WPI_EEPROM_CHAN_ACTIVE)) {
			ic->ic_channels[chan].ic_flags |=
			    IEEE80211_CHAN_PASSIVE;
		}

		/* save maximum allowed power for this channel */
		sc->maxpwr[chan] = channels[i].maxpwr;

		DPRINTF(("adding chan %d flags=0x%x maxpwr=%d\n",
		    chan, channels[i].flags, sc->maxpwr[chan]));
	}
}

void
wpi_read_eeprom_group(struct wpi_softc *sc, int n)
{
	struct wpi_power_group *group = &sc->groups[n];
	struct wpi_eeprom_group rgroup;
	int i;

	wpi_read_prom_data(sc, WPI_EEPROM_POWER_GRP + n * 32, &rgroup,
	    sizeof rgroup);

	/* save power group information */
	group->chan   = rgroup.chan;
	group->maxpwr = rgroup.maxpwr;
	/* temperature at which the samples were taken */
	group->temp   = (int16_t)letoh16(rgroup.temp);

	DPRINTF(("power group %d: chan=%d maxpwr=%d temp=%d\n", n,
	    group->chan, group->maxpwr, group->temp));

	for (i = 0; i < WPI_SAMPLES_COUNT; i++) {
		group->samples[i].index = rgroup.samples[i].index;
		group->samples[i].power = rgroup.samples[i].power;

		DPRINTF(("\tsample %d: index=%d power=%d\n", i,
		    group->samples[i].index, group->samples[i].power));
	}
}

/*
 * Send a command to the firmware.
 */
int
wpi_cmd(struct wpi_softc *sc, int code, const void *buf, int size, int async)
{
	struct wpi_tx_ring *ring = &sc->txq[4];
	struct wpi_tx_desc *desc;
	struct wpi_tx_cmd *cmd;

	KASSERT(size <= sizeof cmd->data);

	desc = &ring->desc[ring->cur];
	cmd = &ring->cmd[ring->cur];

	cmd->code = code;
	cmd->flags = 0;
	cmd->qid = ring->qid;
	cmd->idx = ring->cur;
	memcpy(cmd->data, buf, size);

	desc->flags = htole32(WPI_PAD32(size) << 28 | 1 << 24);
	desc->segs[0].addr = htole32(ring->cmd_dma.paddr +
	    ring->cur * sizeof (struct wpi_tx_cmd));
	desc->segs[0].len  = htole32(4 + size);

	/* kick cmd ring */
	ring->cur = (ring->cur + 1) % WPI_TX_RING_COUNT;
	WPI_WRITE(sc, WPI_TX_WIDX, ring->qid << 8 | ring->cur);

	return async ? 0 : tsleep(cmd, PCATCH, "wpicmd", hz);
}

/*
 * Configure h/w multi-rate retries.
 */
int
wpi_mrr_setup(struct wpi_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
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
		mrr.rates[i].next = (i == WPI_OFDM6) ?
		    ((ic->ic_curmode == IEEE80211_MODE_11A) ?
			WPI_OFDM6 : WPI_CCK2) :
		    i - 1;
		/* try one time at this rate before falling back to "next" */
		mrr.rates[i].ntries = 1;
	}

	/* setup MRR for control frames */
	mrr.which = htole32(WPI_MRR_CTL);
	error = wpi_cmd(sc, WPI_CMD_MRR_SETUP, &mrr, sizeof mrr, 0);
	if (error != 0) {
		printf("%s: could not setup MRR for control frames\n",
		    sc->sc_dev.dv_xname);
		return error;
	}

	/* setup MRR for data frames */
	mrr.which = htole32(WPI_MRR_DATA);
	error = wpi_cmd(sc, WPI_CMD_MRR_SETUP, &mrr, sizeof mrr, 0);
	if (error != 0) {
		printf("%s: could not setup MRR for data frames\n",
		    sc->sc_dev.dv_xname);
		return error;
	}

	return 0;
}

/*
 * Install a pairwise key into the hardware.
 */
int
wpi_set_key(struct ieee80211com *ic, struct ieee80211_node *ni,
    const struct ieee80211_key *k)
{
	struct wpi_softc *sc = ic->ic_softc;
	struct wpi_node_info node;

	if (k->k_flags & IEEE80211_KEY_GROUP)
		return 0;

	memset(&node, 0, sizeof node);

	switch (k->k_cipher) {
	case IEEE80211_CIPHER_CCMP:
		node.security = htole16(WPI_CIPHER_CCMP);
		node.security |= htole16(k->k_id << 8);
		memcpy(node.key, k->k_key, k->k_len);
		break;
	default:
		return 0;
	}

	node.id = WPI_ID_BSS;
	IEEE80211_ADDR_COPY(node.macaddr, ni->ni_macaddr);
	node.control = WPI_NODE_UPDATE;
	node.flags = WPI_FLAG_SET_KEY;

	return wpi_cmd(sc, WPI_CMD_ADD_NODE, &node, sizeof node, 1);
}

void
wpi_updateedca(struct ieee80211com *ic)
{
#define WPI_EXP2(x)	((1 << (x)) - 1)	/* CWmin = 2^ECWmin - 1 */
	struct wpi_softc *sc = ic->ic_softc;
	struct wpi_edca_params cmd;
	int aci;

	memset(&cmd, 0, sizeof cmd);
	cmd.flags = htole32(WPI_EDCA_UPDATE);
	for (aci = 0; aci < EDCA_NUM_AC; aci++) {
		const struct ieee80211_edca_ac_params *ac =
		    &ic->ic_edca_ac[aci];
		cmd.ac[aci].aifsn = ac->ac_aifsn;
		cmd.ac[aci].cwmin = htole16(WPI_EXP2(ac->ac_ecwmin));
		cmd.ac[aci].cwmax = htole16(WPI_EXP2(ac->ac_ecwmax));
		cmd.ac[aci].txoplimit =
		    htole16(IEEE80211_TXOP_TO_US(ac->ac_txoplimit));
	}
	(void)wpi_cmd(sc, WPI_CMD_EDCA_PARAMS, &cmd, sizeof cmd, 1);
#undef WPI_EXP2
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

	if (wpi_cmd(sc, WPI_CMD_TSF, &tsf, sizeof tsf, 1) != 0)
		printf("%s: could not enable TSF\n", sc->sc_dev.dv_xname);
}

/*
 * Update Tx power to match what is defined for channel `c'.
 */
int
wpi_set_txpower(struct wpi_softc *sc, struct ieee80211_channel *c, int async)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct wpi_power_group *group;
	struct wpi_cmd_txpower txpower;
	u_int chan;
	int i;

	/* get channel number */
	chan = ieee80211_chan2ieee(ic, c);

	/* find the power group to which this channel belongs */
	if (IEEE80211_IS_CHAN_5GHZ(c)) {
		for (group = &sc->groups[1]; group < &sc->groups[4]; group++)
			if (chan <= group->chan)
				break;
	} else
		group = &sc->groups[0];

	memset(&txpower, 0, sizeof txpower);
	txpower.band = IEEE80211_IS_CHAN_5GHZ(c) ? 0 : 1;
	txpower.chan = htole16(chan);

	/* set Tx power for all OFDM and CCK rates */
	for (i = 0; i <= 11 ; i++) {
		/* retrieve Tx power for this channel/rate combination */
		int idx = wpi_get_power_index(sc, group, c,
		    wpi_ridx_to_rate[i]);

		txpower.rates[i].plcp = wpi_ridx_to_plcp[i];

		if (IEEE80211_IS_CHAN_5GHZ(c)) {
			txpower.rates[i].rf_gain = wpi_rf_gain_5ghz[idx];
			txpower.rates[i].dsp_gain = wpi_dsp_gain_5ghz[idx];
		} else {
			txpower.rates[i].rf_gain = wpi_rf_gain_2ghz[idx];
			txpower.rates[i].dsp_gain = wpi_dsp_gain_2ghz[idx];
		}
		DPRINTF(("chan %d/rate %d: power index %d\n", chan,
		    wpi_ridx_to_rate[i], idx));
	}

	return wpi_cmd(sc, WPI_CMD_TXPOWER, &txpower, sizeof txpower, async);
}

/*
 * Determine Tx power index for a given channel/rate combination.
 * This takes into account the regulatory information from EEPROM and the
 * current temperature.
 */
int
wpi_get_power_index(struct wpi_softc *sc, struct wpi_power_group *group,
    struct ieee80211_channel *c, int rate)
{
/* fixed-point arithmetic division using a n-bit fractional part */
#define fdivround(a, b, n)	\
	((((1 << n) * (a)) / (b) + (1 << n) / 2) / (1 << n))

/* linear interpolation */
#define interpolate(x, x1, y1, x2, y2, n)	\
	((y1) + fdivround(((x) - (x1)) * ((y2) - (y1)), (x2) - (x1), n))

	struct ieee80211com *ic = &sc->sc_ic;
	struct wpi_power_sample *sample;
	int pwr, idx;
	u_int chan;

	/* get channel number */
	chan = ieee80211_chan2ieee(ic, c);

	/* default power is group's maximum power - 3dB */
	pwr = group->maxpwr / 2;

	/* decrease power for highest OFDM rates to reduce distortion */
	switch (rate) {
	case 72:	/* 36Mb/s */
		pwr -= IEEE80211_IS_CHAN_2GHZ(c) ? 0 :  5;
		break;
	case 96:	/* 48Mb/s */
		pwr -= IEEE80211_IS_CHAN_2GHZ(c) ? 7 : 10;
		break;
	case 108:	/* 54Mb/s */
		pwr -= IEEE80211_IS_CHAN_2GHZ(c) ? 9 : 12;
		break;
	}

	/* never exceed channel's maximum allowed Tx power */
	pwr = min(pwr, sc->maxpwr[chan]);

	/* retrieve power index into gain tables from samples */
	for (sample = group->samples; sample < &group->samples[3]; sample++)
		if (pwr > sample[1].power)
			break;
	/* fixed-point linear interpolation using a 19-bit fractional part */
	idx = interpolate(pwr, sample[0].power, sample[0].index,
	    sample[1].power, sample[1].index, 19);

	/*-
	 * Adjust power index based on current temperature:
	 * - if cooler than factory-calibrated: decrease output power
	 * - if warmer than factory-calibrated: increase output power
	 */
	idx -= (sc->temp - group->temp) * 11 / 100;

	/* decrease power for CCK rates (-5dB) */
	if (!WPI_RATE_IS_OFDM(rate))
		idx += 10;

	/* keep power index in a valid range */
	if (idx < 0)
		return 0;
	if (idx > WPI_MAX_PWR_INDEX)
		return WPI_MAX_PWR_INDEX;
	return idx;

#undef interpolate
#undef fdivround
}

int
wpi_auth(struct wpi_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni = ic->ic_bss;
	struct wpi_node_info node;
	int error;

	/* update adapter's configuration */
	IEEE80211_ADDR_COPY(sc->config.bssid, ni->ni_bssid);
	sc->config.chan = ieee80211_chan2ieee(ic, ni->ni_chan);
	if (IEEE80211_IS_CHAN_2GHZ(ni->ni_chan)) {
		sc->config.flags |= htole32(WPI_CONFIG_AUTO |
		    WPI_CONFIG_24GHZ);
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

	/* configuration has changed, set Tx power accordingly */
	if ((error = wpi_set_txpower(sc, ni->ni_chan, 1)) != 0) {
		printf("%s: could not set Tx power\n", sc->sc_dev.dv_xname);
		return error;
	}

	/* add default node */
	memset(&node, 0, sizeof node);
	IEEE80211_ADDR_COPY(node.macaddr, ni->ni_bssid);
	node.id = WPI_ID_BSS;
	node.rate = (ic->ic_curmode == IEEE80211_MODE_11A) ?
	    wpi_plcp_signal(12) : wpi_plcp_signal(2);
	node.action = htole32(WPI_ACTION_SET_RATE);
	node.antenna = WPI_ANTENNA_BOTH;
	error = wpi_cmd(sc, WPI_CMD_ADD_NODE, &node, sizeof node, 1);
	if (error != 0) {
		printf("%s: could not add BSS node\n", sc->sc_dev.dv_xname);
		return error;
	}

	return 0;
}

/*
 * Send a scan request to the firmware.  Since this command is huge, we map it
 * into a mbuf instead of using the pre-allocated set of commands.
 */
int
wpi_scan(struct wpi_softc *sc, uint16_t flags)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct wpi_tx_ring *ring = &sc->txq[4];
	struct wpi_tx_desc *desc;
	struct wpi_tx_data *data;
	struct wpi_tx_cmd *cmd;
	struct wpi_scan_hdr *hdr;
	struct wpi_cmd_data *tx;
	struct wpi_scan_essid *essid;
	struct wpi_scan_chan *chan;
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

	cmd = mtod(data->m, struct wpi_tx_cmd *);
	cmd->code = WPI_CMD_SCAN;
	cmd->flags = 0;
	cmd->qid = ring->qid;
	cmd->idx = ring->cur;

	hdr = (struct wpi_scan_hdr *)cmd->data;
	memset(hdr, 0, sizeof (struct wpi_scan_hdr));
	/*
	 * Move to the next channel if no packets are received within 5 msecs
	 * after sending the probe request (this helps to reduce the duration
	 * of active scans).
	 */
	hdr->quiet = htole16(5);	/* timeout in milliseconds */
	hdr->plcp_threshold = htole16(1);	/* min # of packets */

	tx = (struct wpi_cmd_data *)(hdr + 1);
	memset(tx, 0, sizeof (struct wpi_cmd_data));
	tx->flags = htole32(WPI_TX_AUTO_SEQ);
	tx->id = WPI_ID_BROADCAST;
	tx->lifetime = htole32(WPI_LIFETIME_INFINITE);

	if (flags & IEEE80211_CHAN_A) {
		hdr->crc_threshold = htole16(1);
		/* send probe requests at 6Mbps */
		tx->rate = wpi_ridx_to_plcp[WPI_OFDM6];
	} else {
		hdr->flags = htole32(WPI_CONFIG_24GHZ | WPI_CONFIG_AUTO);
		/* send probe requests at 1Mbps */
		tx->rate = wpi_ridx_to_plcp[WPI_CCK1];
	}

	essid = (struct wpi_scan_essid *)(tx + 1);
	memset(essid, 0, 4 * sizeof (struct wpi_scan_essid));
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

	/* add SSID IE */
	frm = ieee80211_add_ssid(frm, ic->ic_des_essid, ic->ic_des_esslen);

	mode = ieee80211_chan2mode(ic, ic->ic_ibss_chan);
	rs = &ic->ic_sup_rates[mode];

	/* add supported rates IE */
	frm = ieee80211_add_rates(frm, rs);

	/* add supported xrates IE */
	if (rs->rs_nrates > IEEE80211_RATE_SIZE)
		frm = ieee80211_add_xrates(frm, rs);

	/* setup length of probe request */
	tx->len = htole16(frm - (uint8_t *)wh);

	chan = (struct wpi_scan_chan *)frm;
	for (c  = &ic->ic_channels[1];
	     c <= &ic->ic_channels[IEEE80211_CHAN_MAX]; c++) {
		if ((c->ic_flags & flags) != flags)
			continue;

		chan->chan = ieee80211_chan2ieee(ic, c);
		chan->flags = 0;
		if (!(c->ic_flags & IEEE80211_CHAN_PASSIVE)) {
			chan->flags |= WPI_CHAN_ACTIVE;
			if (ic->ic_des_esslen != 0)
				chan->flags |= WPI_CHAN_DIRECT;
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

		frm += sizeof (struct wpi_scan_chan);
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

	desc->flags = htole32(WPI_PAD32(pktlen) << 28 | 1 << 24);
	desc->segs[0].addr = htole32(data->map->dm_segs[0].ds_addr);
	desc->segs[0].len  = htole32(data->map->dm_segs[0].ds_len);

	/* kick cmd ring */
	ring->cur = (ring->cur + 1) % WPI_TX_RING_COUNT;
	WPI_WRITE(sc, WPI_TX_WIDX, ring->qid << 8 | ring->cur);

	return 0;	/* will be notified async. of failure/success */
}

int
wpi_config(struct wpi_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct wpi_power power;
	struct wpi_bluetooth bluetooth;
	struct wpi_node_info node;
	int error;

	/* set power mode */
	memset(&power, 0, sizeof power);
	power.flags = htole32(WPI_POWER_CAM | 0x8);
	error = wpi_cmd(sc, WPI_CMD_SET_POWER_MODE, &power, sizeof power, 0);
	if (error != 0) {
		printf("%s: could not set power mode\n", sc->sc_dev.dv_xname);
		return error;
	}

	/* configure bluetooth coexistence */
	memset(&bluetooth, 0, sizeof bluetooth);
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
	memset(&sc->config, 0, sizeof (struct wpi_config));
	IEEE80211_ADDR_COPY(ic->ic_myaddr, LLADDR(ifp->if_sadl));
	IEEE80211_ADDR_COPY(sc->config.myaddr, ic->ic_myaddr);
	/* set default channel */
	sc->config.chan = ieee80211_chan2ieee(ic, ic->ic_ibss_chan);
	sc->config.flags = htole32(WPI_CONFIG_TSF);
	if (IEEE80211_IS_CHAN_2GHZ(ic->ic_ibss_chan)) {
		sc->config.flags |= htole32(WPI_CONFIG_AUTO |
		    WPI_CONFIG_24GHZ);
	}
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

	/* configuration has changed, set Tx power accordingly */
	if ((error = wpi_set_txpower(sc, ic->ic_ibss_chan, 0)) != 0) {
		printf("%s: could not set Tx power\n", sc->sc_dev.dv_xname);
		return error;
	}

	/* add broadcast node */
	memset(&node, 0, sizeof node);
	IEEE80211_ADDR_COPY(node.macaddr, etherbroadcastaddr);
	node.id = WPI_ID_BROADCAST;
	node.rate = wpi_plcp_signal(2);
	node.action = htole32(WPI_ACTION_SET_RATE);
	node.antenna = WPI_ANTENNA_BOTH;
	error = wpi_cmd(sc, WPI_CMD_ADD_NODE, &node, sizeof node, 0);
	if (error != 0) {
		printf("%s: could not add broadcast node\n",
		    sc->sc_dev.dv_xname);
		return error;
	}

	if ((error = wpi_mrr_setup(sc)) != 0) {
		printf("%s: could not setup MRR\n", sc->sc_dev.dv_xname);
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
	uint32_t rev, hw;

	/* voodoo from the reference driver */
	hw = WPI_READ(sc, WPI_HWCONFIG);

	rev = pci_conf_read(sc->sc_pct, sc->sc_pcitag, PCI_CLASS_REG);
	rev = PCI_REVISION(rev);
	if ((rev & 0xc0) == 0x40)
		hw |= WPI_HW_ALM_MB;
	else if (!(rev & 0x80))
		hw |= WPI_HW_ALM_MM;

	if (sc->cap == 0x80)
		hw |= WPI_HW_SKU_MRC;

	hw &= ~WPI_HW_REV_D;
	if ((letoh16(sc->rev) & 0xf0) == 0xd0)
		hw |= WPI_HW_REV_D;

	if (sc->type > 1)
		hw |= WPI_HW_TYPE_B;

	DPRINTF(("setting h/w config %x\n", hw));
	WPI_WRITE(sc, WPI_HWCONFIG, hw);
}

int
wpi_init(struct ifnet *ifp)
{
	struct wpi_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
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

	/* not sure why/if this is necessary... */
	WPI_WRITE(sc, WPI_UCODE_CLR, WPI_RADIO_OFF);
	WPI_WRITE(sc, WPI_UCODE_CLR, WPI_RADIO_OFF);

	if ((error = wpi_load_firmware(sc)) != 0) {
		printf("%s: could not load firmware\n", sc->sc_dev.dv_xname);
		goto fail1;
	}

	/* check that the radio is not disabled by RF switch */
	wpi_mem_lock(sc);
	tmp = wpi_mem_read(sc, WPI_MEM_RFKILL);
	wpi_mem_unlock(sc);
	if (!(tmp & 1)) {
		printf("%s: radio is disabled by hardware switch\n",
		    sc->sc_dev.dv_xname);
		error = EPERM;	/* XXX ;-) */
		goto fail1;
	}

	/* wait for thermal sensors to calibrate */
	for (ntries = 0; ntries < 1000; ntries++) {
		if ((sc->temp = (int)WPI_READ(sc, WPI_TEMPERATURE)) != 0)
			break;
		DELAY(10);
	}
	if (ntries == 1000) {
		printf("%s: timeout waiting for thermal sensors calibration\n",
		    sc->sc_dev.dv_xname);
		error = ETIMEDOUT;
		goto fail1;
	}
	DPRINTF(("temperature %d\n", sc->temp));
	sc->sensor.value = sc->temp + 260;
	sc->sensor.flags &= ~SENSOR_FINVALID;

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

fail1:	wpi_stop(ifp, 1);
	return error;
}

void
wpi_stop(struct ifnet *ifp, int disable)
{
	struct wpi_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	uint32_t tmp;
	int i;

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
	for (i = 0; i < WPI_NTXQUEUES; i++)
		wpi_reset_tx_ring(sc, &sc->txq[i]);

	/* reset Rx ring */
	wpi_reset_rx_ring(sc, &sc->rxq);

	/* temperature is no longer valid */
	sc->sensor.value = 0;
	sc->sensor.flags |= SENSOR_FINVALID;

	wpi_mem_lock(sc);
	wpi_mem_write(sc, WPI_MEM_CLOCK2, 0x200);
	wpi_mem_unlock(sc);

	DELAY(5);

	wpi_stop_master(sc);
	tmp = WPI_READ(sc, WPI_RESET);
	WPI_WRITE(sc, WPI_RESET, tmp | WPI_SW_RESET);
}

struct cfdriver wpi_cd = {
	NULL, "wpi", DV_IFNET
};
