/*	$OpenBSD: if_iwi.c,v 1.81 2007/07/05 21:38:19 damien Exp $	*/

/*-
 * Copyright (c) 2004-2006
 *      Damien Bergamini <damien.bergamini@free.fr>. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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
 */

/*
 * Driver for Intel PRO/Wireless 2200BG/2915ABG 802.11 network adapters.
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

#include <dev/rndvar.h>
#include <crypto/arc4.h>

#include <dev/pci/if_iwireg.h>
#include <dev/pci/if_iwivar.h>

const struct pci_matchid iwi_devices[] = {
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_PRO_WL_2200BG },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_PRO_WL_2225BG },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_PRO_WL_2915ABG_1 },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_PRO_WL_2915ABG_2 }
};

int		iwi_match(struct device *, void *, void *);
void		iwi_attach(struct device *, struct device *, void *);
void		iwi_power(int, void *);
int		iwi_alloc_cmd_ring(struct iwi_softc *, struct iwi_cmd_ring *);
void		iwi_reset_cmd_ring(struct iwi_softc *, struct iwi_cmd_ring *);
void		iwi_free_cmd_ring(struct iwi_softc *, struct iwi_cmd_ring *);
int		iwi_alloc_tx_ring(struct iwi_softc *, struct iwi_tx_ring *,
		    bus_size_t, bus_size_t);
void		iwi_reset_tx_ring(struct iwi_softc *, struct iwi_tx_ring *);
void		iwi_free_tx_ring(struct iwi_softc *, struct iwi_tx_ring *);
int		iwi_alloc_rx_ring(struct iwi_softc *, struct iwi_rx_ring *);
void		iwi_reset_rx_ring(struct iwi_softc *, struct iwi_rx_ring *);
void		iwi_free_rx_ring(struct iwi_softc *, struct iwi_rx_ring *);
int		iwi_media_change(struct ifnet *);
void		iwi_media_status(struct ifnet *, struct ifmediareq *);
uint16_t	iwi_read_prom_word(struct iwi_softc *, uint8_t);
int		iwi_find_txnode(struct iwi_softc *, const uint8_t *);
int		iwi_newstate(struct ieee80211com *, enum ieee80211_state, int);
uint8_t		iwi_rate(int);
void		iwi_frame_intr(struct iwi_softc *, struct iwi_rx_data *,
		    struct iwi_frame *);
void		iwi_notification_intr(struct iwi_softc *, struct iwi_rx_data *,
		    struct iwi_notif *);
void		iwi_rx_intr(struct iwi_softc *);
void		iwi_tx_intr(struct iwi_softc *, struct iwi_tx_ring *);
int		iwi_intr(void *);
int		iwi_cmd(struct iwi_softc *, uint8_t, void *, uint8_t, int);
int		iwi_tx_start(struct ifnet *, struct mbuf *,
		    struct ieee80211_node *);
void		iwi_start(struct ifnet *);
void		iwi_watchdog(struct ifnet *);
int		iwi_ioctl(struct ifnet *, u_long, caddr_t);
void		iwi_stop_master(struct iwi_softc *);
int		iwi_reset(struct iwi_softc *);
int		iwi_load_ucode(struct iwi_softc *, const char *, int);
int		iwi_load_firmware(struct iwi_softc *, const char *, int);
int		iwi_config(struct iwi_softc *);
int		iwi_set_chan(struct iwi_softc *, struct ieee80211_channel *);
int		iwi_scan(struct iwi_softc *);
int		iwi_auth_and_assoc(struct iwi_softc *);
int		iwi_init(struct ifnet *);
void		iwi_stop(struct ifnet *, int);

static __inline uint8_t
MEM_READ_1(struct iwi_softc *sc, uint32_t addr)
{
	CSR_WRITE_4(sc, IWI_CSR_INDIRECT_ADDR, addr);
	return CSR_READ_1(sc, IWI_CSR_INDIRECT_DATA);
}

static __inline uint32_t
MEM_READ_4(struct iwi_softc *sc, uint32_t addr)
{
	CSR_WRITE_4(sc, IWI_CSR_INDIRECT_ADDR, addr);
	return CSR_READ_4(sc, IWI_CSR_INDIRECT_DATA);
}

#ifdef IWI_DEBUG
#define DPRINTF(x)	do { if (iwi_debug > 0) printf x; } while (0)
#define DPRINTFN(n, x)	do { if (iwi_debug >= (n)) printf x; } while (0)
int iwi_debug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n, x)
#endif

struct cfattach iwi_ca = {
	sizeof (struct iwi_softc), iwi_match, iwi_attach
};

int
iwi_match(struct device *parent, void *match, void *aux)
{
	return pci_matchbyid((struct pci_attach_args *)aux, iwi_devices,
	    sizeof (iwi_devices) / sizeof (iwi_devices[0]));
}

/* Base Address Register */
#define IWI_PCI_BAR0	0x10

void
iwi_attach(struct device *parent, struct device *self, void *aux)
{
	struct iwi_softc *sc = (struct iwi_softc *)self;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct pci_attach_args *pa = aux;
	const char *intrstr;
	bus_space_tag_t memt;
	bus_space_handle_t memh;
	pci_intr_handle_t ih;
	pcireg_t data;
	uint16_t val;
	int error, i;

	sc->sc_pct = pa->pa_pc;
	sc->sc_pcitag = pa->pa_tag;

	/* clear device specific PCI configuration register 0x41 */
	data = pci_conf_read(sc->sc_pct, sc->sc_pcitag, 0x40);
	data &= ~0x0000ff00;
	pci_conf_write(sc->sc_pct, sc->sc_pcitag, 0x40, data);

	/* map the register window */
	error = pci_mapreg_map(pa, IWI_PCI_BAR0, PCI_MAPREG_TYPE_MEM |
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
	sc->sc_ih = pci_intr_establish(sc->sc_pct, ih, IPL_NET, iwi_intr, sc,
	    sc->sc_dev.dv_xname);
	if (sc->sc_ih == NULL) {
		printf(": could not establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}
	printf(": %s", intrstr);

	if (iwi_reset(sc) != 0) {
		printf(": could not reset adapter\n");
		return;
	}

	/*
	 * Allocate rings.
	 */
	error = iwi_alloc_cmd_ring(sc, &sc->cmdq);
	if (error != 0) {
		printf(": could not allocate Cmd ring\n");
		return;
	}

	error = iwi_alloc_tx_ring(sc, &sc->txq[0], IWI_CSR_TX1_RIDX,
	    IWI_CSR_TX1_WIDX);
	if (error != 0) {
		printf(": could not allocate Tx ring 1\n");
		goto fail1;
	}

	error = iwi_alloc_tx_ring(sc, &sc->txq[1], IWI_CSR_TX2_RIDX,
	    IWI_CSR_TX2_WIDX);
	if (error != 0) {
		printf(": could not allocate Tx ring 2\n");
		goto fail2;
	}

	error = iwi_alloc_tx_ring(sc, &sc->txq[2], IWI_CSR_TX3_RIDX,
	    IWI_CSR_TX3_WIDX);
	if (error != 0) {
		printf(": could not allocate Tx ring 3\n");
		goto fail3;
	}

	error = iwi_alloc_tx_ring(sc, &sc->txq[3], IWI_CSR_TX4_RIDX,
	    IWI_CSR_TX4_WIDX);
	if (error != 0) {
		printf(": could not allocate Tx ring 4\n");
		goto fail4;
	}

	error = iwi_alloc_rx_ring(sc, &sc->rxq);
	if (error != 0) {
		printf(": could not allocate Rx ring\n");
		goto fail5;
	}

	ic->ic_phytype = IEEE80211_T_OFDM;	/* not only, but not used */
	ic->ic_opmode = IEEE80211_M_STA;	/* default to BSS mode */
	ic->ic_state = IEEE80211_S_INIT;

	/* set device capabilities */
	ic->ic_caps =
	    IEEE80211_C_IBSS |		/* IBSS mode supported */
	    IEEE80211_C_MONITOR |	/* monitor mode supported */
	    IEEE80211_C_TXPMGT |	/* tx power management */
	    IEEE80211_C_SHPREAMBLE |	/* short preamble supported */
	    IEEE80211_C_SHSLOT |	/* short slot time supported */
	    IEEE80211_C_WEP |		/* h/w WEP supported */
	    IEEE80211_C_SCANALL;	/* h/w scanning */

	/* read MAC address from EEPROM */
	val = iwi_read_prom_word(sc, IWI_EEPROM_MAC + 0);
	ic->ic_myaddr[0] = val & 0xff;
	ic->ic_myaddr[1] = val >> 8;
	val = iwi_read_prom_word(sc, IWI_EEPROM_MAC + 1);
	ic->ic_myaddr[2] = val & 0xff;
	ic->ic_myaddr[3] = val >> 8;
	val = iwi_read_prom_word(sc, IWI_EEPROM_MAC + 2);
	ic->ic_myaddr[4] = val & 0xff;
	ic->ic_myaddr[5] = val >> 8;

	printf(", address %s\n", ether_sprintf(ic->ic_myaddr));

	if (PCI_PRODUCT(pa->pa_id) >= PCI_PRODUCT_INTEL_PRO_WL_2915ABG_1) {
		/* set supported .11a rates */
		ic->ic_sup_rates[IEEE80211_MODE_11A] = ieee80211_std_rateset_11a;

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
	}

	/* set supported .11b and .11g rates */
	ic->ic_sup_rates[IEEE80211_MODE_11B] = ieee80211_std_rateset_11b;
	ic->ic_sup_rates[IEEE80211_MODE_11G] = ieee80211_std_rateset_11g;

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
	ifp->if_init = iwi_init;
	ifp->if_ioctl = iwi_ioctl;
	ifp->if_start = iwi_start;
	ifp->if_watchdog = iwi_watchdog;
	IFQ_SET_READY(&ifp->if_snd);
	bcopy(sc->sc_dev.dv_xname, ifp->if_xname, IFNAMSIZ);

	if_attach(ifp);
	ieee80211_ifattach(ifp);
	/* override state transition machine */
	sc->sc_newstate = ic->ic_newstate;
	ic->ic_newstate = iwi_newstate;
	ieee80211_media_init(ifp, iwi_media_change, iwi_media_status);

	sc->powerhook = powerhook_establish(iwi_power, sc);

#if NBPFILTER > 0
	bpfattach(&sc->sc_drvbpf, ifp, DLT_IEEE802_11_RADIO,
	    sizeof (struct ieee80211_frame) + IEEE80211_RADIOTAP_HDRLEN);

	sc->sc_rxtap_len = sizeof sc->sc_rxtapu;
	sc->sc_rxtap.wr_ihdr.it_len = htole16(sc->sc_rxtap_len);
	sc->sc_rxtap.wr_ihdr.it_present = htole32(IWI_RX_RADIOTAP_PRESENT);

	sc->sc_txtap_len = sizeof sc->sc_txtapu;
	sc->sc_txtap.wt_ihdr.it_len = htole16(sc->sc_txtap_len);
	sc->sc_txtap.wt_ihdr.it_present = htole32(IWI_TX_RADIOTAP_PRESENT);
#endif

	return;

fail5:	iwi_free_tx_ring(sc, &sc->txq[3]);
fail4:	iwi_free_tx_ring(sc, &sc->txq[2]);
fail3:	iwi_free_tx_ring(sc, &sc->txq[1]);
fail2:	iwi_free_tx_ring(sc, &sc->txq[0]);
fail1:	iwi_free_cmd_ring(sc, &sc->cmdq);
}

void
iwi_power(int why, void *arg)
{
	struct iwi_softc *sc = arg;
	struct ifnet *ifp;
	pcireg_t data;

	if (why != PWR_RESUME)
		return;

	/* clear device specific PCI configuration register 0x41 */
	data = pci_conf_read(sc->sc_pct, sc->sc_pcitag, 0x40);
	data &= ~0x0000ff00;
	pci_conf_write(sc->sc_pct, sc->sc_pcitag, 0x40, data);

	ifp = &sc->sc_ic.ic_if;
	if (ifp->if_flags & IFF_UP) {
		ifp->if_init(ifp);
		if (ifp->if_flags & IFF_RUNNING)
			ifp->if_start(ifp);
	}
}

int
iwi_alloc_cmd_ring(struct iwi_softc *sc, struct iwi_cmd_ring *ring)
{
	int nsegs, error;

	ring->queued = 0;
	ring->cur = ring->next = 0;

	error = bus_dmamap_create(sc->sc_dmat,
	    sizeof (struct iwi_cmd_desc) * IWI_CMD_RING_COUNT, 1,
	    sizeof (struct iwi_cmd_desc) * IWI_CMD_RING_COUNT, 0,
	    BUS_DMA_NOWAIT, &ring->map);
	if (error != 0) {
		printf("%s: could not create cmd ring DMA map\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	error = bus_dmamem_alloc(sc->sc_dmat,
	    sizeof (struct iwi_cmd_desc) * IWI_CMD_RING_COUNT, PAGE_SIZE, 0,
	    &ring->seg, 1, &nsegs, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not allocate cmd ring DMA memory\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	error = bus_dmamem_map(sc->sc_dmat, &ring->seg, nsegs,
	    sizeof (struct iwi_cmd_desc) * IWI_CMD_RING_COUNT,
	    (caddr_t *)&ring->desc, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not map cmd ring DMA memory\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	error = bus_dmamap_load(sc->sc_dmat, ring->map, ring->desc,
	    sizeof (struct iwi_cmd_desc) * IWI_CMD_RING_COUNT, NULL,
	    BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not load cmd ring DMA map\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	bzero(ring->desc, sizeof (struct iwi_cmd_desc) * IWI_CMD_RING_COUNT);
	return 0;

fail:	iwi_free_cmd_ring(sc, ring);
	return error;
}

void
iwi_reset_cmd_ring(struct iwi_softc *sc, struct iwi_cmd_ring *ring)
{
	ring->queued = 0;
	ring->cur = ring->next = 0;
}

void
iwi_free_cmd_ring(struct iwi_softc *sc, struct iwi_cmd_ring *ring)
{
	if (ring->map != NULL) {
		if (ring->desc != NULL) {
			bus_dmamap_unload(sc->sc_dmat, ring->map);
			bus_dmamem_unmap(sc->sc_dmat, (caddr_t)ring->desc,
			    sizeof (struct iwi_cmd_desc) * IWI_CMD_RING_COUNT);
			bus_dmamem_free(sc->sc_dmat, &ring->seg, 1);
		}
		bus_dmamap_destroy(sc->sc_dmat, ring->map);
	}
}

int
iwi_alloc_tx_ring(struct iwi_softc *sc, struct iwi_tx_ring *ring,
    bus_size_t csr_ridx, bus_size_t csr_widx)
{
	struct iwi_tx_data *data;
	int i, nsegs, error;

	ring->queued = 0;
	ring->cur = ring->next = 0;
	ring->csr_ridx = csr_ridx;
	ring->csr_widx = csr_widx;

	error = bus_dmamap_create(sc->sc_dmat,
	    sizeof (struct iwi_tx_desc) * IWI_TX_RING_COUNT, 1,
	    sizeof (struct iwi_tx_desc) * IWI_TX_RING_COUNT, 0, BUS_DMA_NOWAIT,
	    &ring->map);
	if (error != 0) {
		printf("%s: could not create tx ring DMA map\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	error = bus_dmamem_alloc(sc->sc_dmat,
	    sizeof (struct iwi_tx_desc) * IWI_TX_RING_COUNT, PAGE_SIZE, 0,
	    &ring->seg, 1, &nsegs, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not allocate tx ring DMA memory\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	error = bus_dmamem_map(sc->sc_dmat, &ring->seg, nsegs,
	    sizeof (struct iwi_tx_desc) * IWI_TX_RING_COUNT,
	    (caddr_t *)&ring->desc, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not map tx ring DMA memory\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	error = bus_dmamap_load(sc->sc_dmat, ring->map, ring->desc,
	    sizeof (struct iwi_tx_desc) * IWI_TX_RING_COUNT, NULL,
	    BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not load tx ring DMA map\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	bzero(ring->desc, sizeof (struct iwi_tx_desc) * IWI_TX_RING_COUNT);

	for (i = 0; i < IWI_TX_RING_COUNT; i++) {
		data = &ring->data[i];

		error = bus_dmamap_create(sc->sc_dmat, MCLBYTES,
		    IWI_MAX_SCATTER, MCLBYTES, 0, BUS_DMA_NOWAIT, &data->map);
		if (error != 0) {
			printf("%s: could not create tx buf DMA map\n",
			    sc->sc_dev.dv_xname);
			goto fail;
		}
	}

	return 0;

fail:	iwi_free_tx_ring(sc, ring);
	return error;
}

void
iwi_reset_tx_ring(struct iwi_softc *sc, struct iwi_tx_ring *ring)
{
	struct iwi_tx_data *data;
	int i;

	for (i = 0; i < IWI_TX_RING_COUNT; i++) {
		data = &ring->data[i];

		if (data->m != NULL) {
			bus_dmamap_unload(sc->sc_dmat, data->map);
			m_freem(data->m);
			data->m = NULL;
		}
	}

	ring->queued = 0;
	ring->cur = ring->next = 0;
}

void
iwi_free_tx_ring(struct iwi_softc *sc, struct iwi_tx_ring *ring)
{
	struct iwi_tx_data *data;
	int i;

	if (ring->map != NULL) {
		if (ring->desc != NULL) {
			bus_dmamap_unload(sc->sc_dmat, ring->map);
			bus_dmamem_unmap(sc->sc_dmat, (caddr_t)ring->desc,
			    sizeof (struct iwi_tx_desc) * IWI_TX_RING_COUNT);
			bus_dmamem_free(sc->sc_dmat, &ring->seg, 1);
		}
		bus_dmamap_destroy(sc->sc_dmat, ring->map);
	}

	for (i = 0; i < IWI_TX_RING_COUNT; i++) {
		data = &ring->data[i];

		if (data->m != NULL) {
			bus_dmamap_unload(sc->sc_dmat, data->map);
			m_freem(data->m);
		}
		bus_dmamap_destroy(sc->sc_dmat, data->map);
	}
}

int
iwi_alloc_rx_ring(struct iwi_softc *sc, struct iwi_rx_ring *ring)
{
	struct iwi_rx_data *data;
	int i, error;

	ring->cur = 0;

	for (i = 0; i < IWI_RX_RING_COUNT; i++) {
		data = &sc->rxq.data[i];

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
		    mtod(data->m, void *), MCLBYTES, NULL, BUS_DMA_NOWAIT);
		if (error != 0) {
			printf("%s: could not load rx buf DMA map\n",
			    sc->sc_dev.dv_xname);
			goto fail;
		}

		data->reg = IWI_CSR_RX_BASE + i * 4;
	}

	return 0;

fail:	iwi_free_rx_ring(sc, ring);
	return error;
}

void
iwi_reset_rx_ring(struct iwi_softc *sc, struct iwi_rx_ring *ring)
{
	ring->cur = 0;
}

void
iwi_free_rx_ring(struct iwi_softc *sc, struct iwi_rx_ring *ring)
{
	struct iwi_rx_data *data;
	int i;

	for (i = 0; i < IWI_RX_RING_COUNT; i++) {
		data = &sc->rxq.data[i];

		if (data->m != NULL) {
			bus_dmamap_unload(sc->sc_dmat, data->map);
			m_freem(data->m);
		}
		bus_dmamap_destroy(sc->sc_dmat, data->map);
	}
}

int
iwi_media_change(struct ifnet *ifp)
{
	int error;

	error = ieee80211_media_change(ifp);
	if (error != ENETRESET)
		return error;

	if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) == (IFF_UP | IFF_RUNNING))
		iwi_init(ifp);

	return 0;
}

void
iwi_media_status(struct ifnet *ifp, struct ifmediareq *imr)
{
	struct iwi_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	uint32_t val;
	int rate;

	imr->ifm_status = IFM_AVALID;
	imr->ifm_active = IFM_IEEE80211;
	if (ic->ic_state == IEEE80211_S_RUN)
		imr->ifm_status |= IFM_ACTIVE;

	/* read current transmission rate from adapter */
	val = CSR_READ_4(sc, IWI_CSR_CURRENT_TX_RATE);
	/* convert PLCP signal to 802.11 rate */
	rate = iwi_rate(val);

	imr->ifm_active |= ieee80211_rate2media(ic, rate, ic->ic_curmode);
	switch (ic->ic_opmode) {
	case IEEE80211_M_STA:
		break;
	case IEEE80211_M_IBSS:
		imr->ifm_active |= IFM_IEEE80211_ADHOC;
		break;
	case IEEE80211_M_MONITOR:
		imr->ifm_active |= IFM_IEEE80211_MONITOR;
		break;
	case IEEE80211_M_AHDEMO:
	case IEEE80211_M_HOSTAP:
		/* should not get there */
		break;
	}
}

/*
 * This is only used for IBSS mode where the firmware expect an index to an
 * internal node table instead of a destination address.
 */
int
iwi_find_txnode(struct iwi_softc *sc, const uint8_t *macaddr)
{
	struct iwi_node node;
	int i;

	for (i = 0; i < sc->nsta; i++)
		if (IEEE80211_ADDR_EQ(sc->sta[i], macaddr))
			return i;	/* already existing node */

	if (i == IWI_MAX_NODE)
		return -1;	/* no place left in neighbor table */

	/* save this new node in our softc table */
	IEEE80211_ADDR_COPY(sc->sta[i], macaddr);
	sc->nsta = i;

	/* write node information into NIC memory */
	bzero(&node, sizeof node);
	IEEE80211_ADDR_COPY(node.bssid, macaddr);

	CSR_WRITE_REGION_1(sc, IWI_CSR_NODE_BASE + i * sizeof node,
	    (uint8_t *)&node, sizeof node);

	return i;
}

int
iwi_newstate(struct ieee80211com *ic, enum ieee80211_state nstate, int arg)
{
	struct iwi_softc *sc = ic->ic_softc;
	enum ieee80211_state ostate;
	uint32_t tmp;

	ostate = ic->ic_state;

	switch (nstate) {
	case IEEE80211_S_SCAN:
		iwi_scan(sc);
		break;

	case IEEE80211_S_AUTH:
		iwi_auth_and_assoc(sc);
		break;

	case IEEE80211_S_RUN:
		if (ic->ic_opmode == IEEE80211_M_IBSS) {
			sc->nsta = 0;	/* flush IBSS nodes */
			ieee80211_new_state(ic, IEEE80211_S_AUTH, -1);
		} else if (ic->ic_opmode == IEEE80211_M_MONITOR)
			iwi_set_chan(sc, ic->ic_ibss_chan);

		/* assoc led on */
		tmp = MEM_READ_4(sc, IWI_MEM_EVENT_CTL) & IWI_LED_MASK;
		MEM_WRITE_4(sc, IWI_MEM_EVENT_CTL, tmp | IWI_LED_ASSOC);
		break;

	case IEEE80211_S_INIT:
		if (ostate != IEEE80211_S_RUN)
			break;

		/* assoc led off */
		tmp = MEM_READ_4(sc, IWI_MEM_EVENT_CTL) & IWI_LED_MASK;
		MEM_WRITE_4(sc, IWI_MEM_EVENT_CTL, tmp & ~IWI_LED_ASSOC);
		break;

	case IEEE80211_S_ASSOC:
		break;
	}

	ic->ic_state = nstate;
	return 0;
}

/*
 * Read 16 bits at address 'addr' from the serial EEPROM.
 * DON'T PLAY WITH THIS CODE UNLESS YOU KNOW *EXACTLY* WHAT YOU'RE DOING!
 */
uint16_t
iwi_read_prom_word(struct iwi_softc *sc, uint8_t addr)
{
	uint32_t tmp;
	uint16_t val;
	int n;

	/* clock C once before the first command */
	IWI_EEPROM_CTL(sc, 0);
	IWI_EEPROM_CTL(sc, IWI_EEPROM_S);
	IWI_EEPROM_CTL(sc, IWI_EEPROM_S | IWI_EEPROM_C);
	IWI_EEPROM_CTL(sc, IWI_EEPROM_S);

	/* write start bit (1) */
	IWI_EEPROM_CTL(sc, IWI_EEPROM_S | IWI_EEPROM_D);
	IWI_EEPROM_CTL(sc, IWI_EEPROM_S | IWI_EEPROM_D | IWI_EEPROM_C);

	/* write READ opcode (10) */
	IWI_EEPROM_CTL(sc, IWI_EEPROM_S | IWI_EEPROM_D);
	IWI_EEPROM_CTL(sc, IWI_EEPROM_S | IWI_EEPROM_D | IWI_EEPROM_C);
	IWI_EEPROM_CTL(sc, IWI_EEPROM_S);
	IWI_EEPROM_CTL(sc, IWI_EEPROM_S | IWI_EEPROM_C);

	/* write address A7-A0 */
	for (n = 7; n >= 0; n--) {
		IWI_EEPROM_CTL(sc, IWI_EEPROM_S |
		    (((addr >> n) & 1) << IWI_EEPROM_SHIFT_D));
		IWI_EEPROM_CTL(sc, IWI_EEPROM_S |
		    (((addr >> n) & 1) << IWI_EEPROM_SHIFT_D) | IWI_EEPROM_C);
	}

	IWI_EEPROM_CTL(sc, IWI_EEPROM_S);

	/* read data Q15-Q0 */
	val = 0;
	for (n = 15; n >= 0; n--) {
		IWI_EEPROM_CTL(sc, IWI_EEPROM_S | IWI_EEPROM_C);
		IWI_EEPROM_CTL(sc, IWI_EEPROM_S);
		tmp = MEM_READ_4(sc, IWI_MEM_EEPROM_CTL);
		val |= ((tmp & IWI_EEPROM_Q) >> IWI_EEPROM_SHIFT_Q) << n;
	}

	IWI_EEPROM_CTL(sc, 0);

	/* clear Chip Select and clock C */
	IWI_EEPROM_CTL(sc, IWI_EEPROM_S);
	IWI_EEPROM_CTL(sc, 0);
	IWI_EEPROM_CTL(sc, IWI_EEPROM_C);

	return val;
}

uint8_t
iwi_rate(int plcp)
{
	switch (plcp) {
	/* CCK rates (values are device-dependent) */
	case  10:	return 2;
	case  20:	return 4;
	case  55:	return 11;
	case 110:	return 22;

	/* OFDM rates (cf IEEE Std 802.11a-1999, pp. 14 Table 80) */
	case 0xd:	return 12;
	case 0xf:	return 18;
	case 0x5:	return 24;
	case 0x7:	return 36;
	case 0x9:	return 48;
	case 0xb:	return 72;
	case 0x1:	return 96;
	case 0x3:	return 108;

	/* unknown rate: should not happen */
	default:	return 0;
	}
}

void
iwi_frame_intr(struct iwi_softc *sc, struct iwi_rx_data *data,
    struct iwi_frame *frame)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct mbuf *mnew, *m;
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni;
	int error;

	DPRINTFN(5, ("received frame len=%u chan=%u rssi=%u\n",
	    letoh16(frame->len), frame->chan, frame->rssi_dbm));

	if (letoh16(frame->len) < sizeof (struct ieee80211_frame_min) ||
	    letoh16(frame->len) > MCLBYTES) {
		DPRINTF(("%s: bad frame length\n", sc->sc_dev.dv_xname));
		ifp->if_ierrors++;
		return;
	}

	/*
	 * Try to allocate a new mbuf for this ring element and load it before
	 * processing the current mbuf.  If the ring element cannot be loaded,
	 * drop the received packet and reuse the old mbuf.  In the unlikely
	 * case that the old mbuf can't be reloaded either, explicitly panic.
	 */
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
	CSR_WRITE_4(sc, data->reg, data->map->dm_segs[0].ds_addr);

	/* finalize mbuf */
	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = m->m_len = sizeof (struct iwi_hdr) +
	    sizeof (struct iwi_frame) + letoh16(frame->len);
	m_adj(m, sizeof (struct iwi_hdr) + sizeof (struct iwi_frame));

	wh = mtod(m, struct ieee80211_frame *);

	if ((wh->i_fc[1] & IEEE80211_FC1_WEP) &&
	    ic->ic_opmode != IEEE80211_M_MONITOR) {
		/*
		 * Hardware decrypts the frame itself but leaves the WEP bit
		 * set in the 802.11 header and doesn't remove the IV and CRC
		 * fields.
		 */
		wh->i_fc[1] &= ~IEEE80211_FC1_WEP;
		ovbcopy(wh, (char *)wh + IEEE80211_WEP_IVLEN +
		    IEEE80211_WEP_KIDLEN, sizeof (struct ieee80211_frame));
		m_adj(m, IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN);
		m_adj(m, -IEEE80211_WEP_CRCLEN);
		wh = mtod(m, struct ieee80211_frame *);
	}

#if NBPFILTER > 0
	if (sc->sc_drvbpf != NULL) {
		struct mbuf mb;
		struct iwi_rx_radiotap_header *tap = &sc->sc_rxtap;

		tap->wr_flags = 0;
		tap->wr_rate = iwi_rate(frame->rate);
		tap->wr_chan_freq =
		    htole16(ic->ic_channels[frame->chan].ic_freq);
		tap->wr_chan_flags =
		    htole16(ic->ic_channels[frame->chan].ic_flags);
		tap->wr_antsignal = frame->signal;
		tap->wr_antenna = frame->antenna & 0x3;
		if (frame->antenna & 0x40)
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

	ni = ieee80211_find_rxnode(ic, wh);

	/* send the frame to the upper layer */
	ieee80211_input(ifp, m, ni, frame->rssi_dbm, 0);

	/* node is no longer needed */
	ieee80211_release_node(ic, ni);
}

void
iwi_notification_intr(struct iwi_softc *sc, struct iwi_rx_data *data,
    struct iwi_notif *notif)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;

	switch (notif->type) {
	case IWI_NOTIF_TYPE_SCAN_CHANNEL:
	{
#ifdef IWI_DEBUG
		struct iwi_notif_scan_channel *chan =
		    (struct iwi_notif_scan_channel *)(notif + 1);
#endif
		DPRINTFN(2, ("Scanning channel (%u)\n", chan->nchan));
		break;
	}
	case IWI_NOTIF_TYPE_SCAN_COMPLETE:
	{
#ifdef IWI_DEBUG
		struct iwi_notif_scan_complete *scan =
		    (struct iwi_notif_scan_complete *)(notif + 1);
#endif
		DPRINTFN(2, ("Scan completed (%u, %u)\n", scan->nchan,
		    scan->status));

		/* monitor mode uses scan to set the channel ... */
		if (ic->ic_opmode != IEEE80211_M_MONITOR)
			ieee80211_end_scan(ifp);
		else
			iwi_set_chan(sc, ic->ic_ibss_chan);
		break;
	}
	case IWI_NOTIF_TYPE_AUTHENTICATION:
	{
		struct iwi_notif_authentication *auth =
		    (struct iwi_notif_authentication *)(notif + 1);

		DPRINTFN(2, ("Authentication (%u)\n", auth->state));

		switch (auth->state) {
		case IWI_AUTHENTICATED:
			ieee80211_new_state(ic, IEEE80211_S_ASSOC, -1);
			break;

		case IWI_DEAUTHENTICATED:
			break;

		default:
			printf("%s: unknown authentication state %u\n",
			    sc->sc_dev.dv_xname, auth->state);
		}
		break;
	}
	case IWI_NOTIF_TYPE_ASSOCIATION:
	{
		struct iwi_notif_association *assoc =
		    (struct iwi_notif_association *)(notif + 1);

		DPRINTFN(2, ("Association (%u, %u)\n", assoc->state,
		    assoc->status));

		switch (assoc->state) {
		case IWI_AUTHENTICATED:
			/* re-association, do nothing */
			break;

		case IWI_ASSOCIATED:
			ieee80211_new_state(ic, IEEE80211_S_RUN, -1);
			break;

		case IWI_DEASSOCIATED:
			ieee80211_begin_scan(ifp);
			break;

		default:
			printf("%s: unknown association state %u\n",
			    sc->sc_dev.dv_xname, assoc->state);
		}
		break;
	}
	case IWI_NOTIF_TYPE_BEACON:	
	{
		struct iwi_notif_beacon *beacon =
		    (struct iwi_notif_beacon *)(notif + 1);

		if (letoh32(beacon->status) == IWI_BEACON_MISSED) {
			/* XXX should roam when too many beacons missed */
			DPRINTFN(2, ("%s: %u beacon(s) missed\n",
			    sc->sc_dev.dv_xname, letoh32(beacon->count)));
		}
		break;
	}
	case IWI_NOTIF_TYPE_BAD_LINK:
		DPRINTFN(2, ("link deterioration detected\n"));
		break;

	case IWI_NOTIF_TYPE_NOISE:
		DPRINTFN(5, ("Measured noise %u\n",
		    letoh32(*(uint32_t *)(notif + 1)) & 0xff));
		break;

	default:
		DPRINTFN(5, ("Notification (%u)\n", notif->type));
	}
}

void
iwi_rx_intr(struct iwi_softc *sc)
{
	struct iwi_rx_data *data;
	struct iwi_hdr *hdr;
	uint32_t hw;

	hw = CSR_READ_4(sc, IWI_CSR_RX_RIDX);

	for (; sc->rxq.cur != hw;) {
		data = &sc->rxq.data[sc->rxq.cur];

		bus_dmamap_sync(sc->sc_dmat, data->map, 0, MCLBYTES,
		    BUS_DMASYNC_POSTREAD);

		hdr = mtod(data->m, struct iwi_hdr *);

		switch (hdr->type) {
		case IWI_HDR_TYPE_FRAME:
			iwi_frame_intr(sc, data,
			    (struct iwi_frame *)(hdr + 1));
			break;

		case IWI_HDR_TYPE_NOTIF:
			iwi_notification_intr(sc, data,
			    (struct iwi_notif *)(hdr + 1));
			break;

		default:
			printf("%s: unknown hdr type %u\n",
			    sc->sc_dev.dv_xname, hdr->type);
		}

		sc->rxq.cur = (sc->rxq.cur + 1) % IWI_RX_RING_COUNT;
	}

	/* tell the firmware what we have processed */
	hw = (hw == 0) ? IWI_RX_RING_COUNT - 1 : hw - 1;
	CSR_WRITE_4(sc, IWI_CSR_RX_WIDX, hw);
}

void
iwi_tx_intr(struct iwi_softc *sc, struct iwi_tx_ring *txq)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct iwi_tx_data *data;
	uint32_t hw;

	hw = CSR_READ_4(sc, txq->csr_ridx);

	for (; txq->next != hw;) {
		data = &txq->data[txq->next];

		bus_dmamap_unload(sc->sc_dmat, data->map);
		m_freem(data->m);
		data->m = NULL;
		ieee80211_release_node(ic, data->ni);
		data->ni = NULL;

		ifp->if_opackets++;

		txq->queued--;
		txq->next = (txq->next + 1) % IWI_TX_RING_COUNT;
	}

	sc->sc_tx_timer = 0;
	ifp->if_flags &= ~IFF_OACTIVE;
	(*ifp->if_start)(ifp);
}

int
iwi_intr(void *arg)
{
	struct iwi_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_ic.ic_if;
	uint32_t r;

	if ((r = CSR_READ_4(sc, IWI_CSR_INTR)) == 0 || r == 0xffffffff)
		return 0;

	/* disable interrupts */
	CSR_WRITE_4(sc, IWI_CSR_INTR_MASK, 0);

	/* acknowledge interrupts */
	CSR_WRITE_4(sc, IWI_CSR_INTR, r);

	if (r & IWI_INTR_FATAL_ERROR) {
		printf("%s: fatal firmware error\n", sc->sc_dev.dv_xname);
		ifp->if_flags &= ~IFF_UP;
		iwi_stop(ifp, 1);
		return 1;
	}

	if (r & IWI_INTR_FW_INITED)
		wakeup(sc);

	if (r & IWI_INTR_RADIO_OFF) {
		DPRINTF(("radio transmitter off\n"));
		ifp->if_flags &= ~IFF_UP;
		iwi_stop(ifp, 1);
		return 1;
	}

	if (r & IWI_INTR_CMD_DONE) {
		/* kick next pending command if any */
		sc->cmdq.next = (sc->cmdq.next + 1) % IWI_CMD_RING_COUNT;
		if (--sc->cmdq.queued > 0)
			CSR_WRITE_4(sc, IWI_CSR_CMD_WIDX, sc->cmdq.next);

		wakeup(sc);
	}

	if (r & IWI_INTR_TX1_DONE)
		iwi_tx_intr(sc, &sc->txq[0]);

	if (r & IWI_INTR_TX2_DONE)
		iwi_tx_intr(sc, &sc->txq[1]);

	if (r & IWI_INTR_TX3_DONE)
		iwi_tx_intr(sc, &sc->txq[2]);

	if (r & IWI_INTR_TX4_DONE)
		iwi_tx_intr(sc, &sc->txq[3]);

	if (r & IWI_INTR_RX_DONE)
		iwi_rx_intr(sc);

	/* re-enable interrupts */
	CSR_WRITE_4(sc, IWI_CSR_INTR_MASK, IWI_INTR_MASK);

	return 1;
}

int
iwi_cmd(struct iwi_softc *sc, uint8_t type, void *data, uint8_t len, int async)
{
	struct iwi_cmd_desc *desc;

	desc = &sc->cmdq.desc[sc->cmdq.cur];
	desc->hdr.type = IWI_HDR_TYPE_COMMAND;
	desc->hdr.flags = IWI_HDR_FLAG_IRQ;
	desc->type = type;
	desc->len = len;
	bcopy(data, desc->data, len);

	bus_dmamap_sync(sc->sc_dmat, sc->cmdq.map,
	    sc->cmdq.cur * sizeof (struct iwi_cmd_desc),
	    sizeof (struct iwi_cmd_desc), BUS_DMASYNC_PREWRITE);

	DPRINTFN(2, ("sending command idx=%u type=%u len=%u\n", sc->cmdq.cur,
	    type, len));

	sc->cmdq.cur = (sc->cmdq.cur + 1) % IWI_CMD_RING_COUNT;

	/* don't kick cmd immediately if another async command is pending */
	if (++sc->cmdq.queued == 1) {
		sc->cmdq.next = sc->cmdq.cur;
		CSR_WRITE_4(sc, IWI_CSR_CMD_WIDX, sc->cmdq.next);
	}

	return async ? 0 : tsleep(sc, 0, "iwicmd", hz);
}

int
iwi_tx_start(struct ifnet *ifp, struct mbuf *m0, struct ieee80211_node *ni)
{
	struct iwi_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct iwi_tx_data *data;
	struct iwi_tx_desc *desc;
	struct iwi_tx_ring *txq = &sc->txq[0];
	struct mbuf *mnew;
	int error, i, station = 0;

#if NBPFILTER > 0
	if (sc->sc_drvbpf != NULL) {
		struct mbuf mb;
		struct iwi_tx_radiotap_header *tap = &sc->sc_txtap;

		tap->wt_flags = 0;
		tap->wt_chan_freq = htole16(ic->ic_bss->ni_chan->ic_freq);
		tap->wt_chan_flags = htole16(ic->ic_bss->ni_chan->ic_flags);

		mb.m_data = (caddr_t)tap;
		mb.m_len = sc->sc_txtap_len;
		mb.m_next = m0;
		mb.m_nextpkt = NULL;
		mb.m_type = 0;
		mb.m_flags = 0;
		bpf_mtap(sc->sc_drvbpf, &mb, BPF_DIRECTION_OUT);
	}
#endif

	data = &txq->data[txq->cur];
	desc = &txq->desc[txq->cur];

	/* save and trim IEEE802.11 header */
	m_copydata(m0, 0, sizeof (struct ieee80211_frame), (caddr_t)&desc->wh);
	m_adj(m0, sizeof (struct ieee80211_frame));

	if (ic->ic_opmode == IEEE80211_M_IBSS) {
		station = iwi_find_txnode(sc, desc->wh.i_addr1);
		if (station == -1) {
			m_freem(m0);
			ieee80211_release_node(ic, ni);
			ifp->if_oerrors++;
			return 0;
		}
	}

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

	desc->hdr.type = IWI_HDR_TYPE_DATA;
	desc->hdr.flags = IWI_HDR_FLAG_IRQ;
	desc->cmd = IWI_DATA_CMD_TX;
	desc->len = htole16(m0->m_pkthdr.len);
	desc->station = station;
	desc->flags = 0;
	desc->xflags = 0;

	if (!IEEE80211_IS_MULTICAST(desc->wh.i_addr1))
		desc->flags |= IWI_DATA_FLAG_NEED_ACK;

	if (desc->wh.i_fc[1] & IEEE80211_FC1_WEP) {
		desc->wep_txkey = ic->ic_wep_txkey |
		    (ic->ic_nw_keys[ic->ic_wep_txkey].wk_len <= 5) ?
			IWI_DATA_KEY_WEP40 : IWI_DATA_KEY_WEP104;
	} else {
		desc->flags |= IWI_DATA_FLAG_NO_WEP;
		desc->wep_txkey = 0;
	}
	if (ic->ic_flags & IEEE80211_F_SHPREAMBLE)
		desc->flags |= IWI_DATA_FLAG_SHPREAMBLE;

	if (ic->ic_curmode == IEEE80211_MODE_11B)
		desc->xflags |= IWI_DATA_XFLAG_CCK;

	desc->nseg = htole32(data->map->dm_nsegs);
	for (i = 0; i < data->map->dm_nsegs; i++) {
		desc->seg_addr[i] = htole32(data->map->dm_segs[i].ds_addr);
		desc->seg_len[i]  = htole16(data->map->dm_segs[i].ds_len);
	}

	bus_dmamap_sync(sc->sc_dmat, data->map, 0, data->map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(sc->sc_dmat, txq->map,
	    txq->cur * sizeof (struct iwi_tx_desc),
	    sizeof (struct iwi_tx_desc), BUS_DMASYNC_PREWRITE);

	DPRINTFN(5, ("sending data frame idx=%u len=%u nseg=%u\n", txq->cur,
	    letoh16(desc->len), data->map->dm_nsegs));

	txq->queued++;
	txq->cur = (txq->cur + 1) % IWI_TX_RING_COUNT;
	CSR_WRITE_4(sc, txq->csr_widx, txq->cur);

	return 0;
}

void
iwi_start(struct ifnet *ifp)
{
	struct iwi_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct mbuf *m0;
	struct ieee80211_node *ni;

	if (ic->ic_state != IEEE80211_S_RUN)
		return;

	for (;;) {
		IFQ_POLL(&ifp->if_snd, m0);
		if (m0 == NULL)
			break;

		if (sc->txq[0].queued >= IWI_TX_RING_COUNT - 8) {
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

		if (iwi_tx_start(ifp, m0, ni) != 0) {
			if (ni != NULL)
				ieee80211_release_node(ic, ni);
			ifp->if_oerrors++;
			break;
		}

		/* start watchdog timer */
		sc->sc_tx_timer = 5;
		ifp->if_timer = 1;
	}
}

void
iwi_watchdog(struct ifnet *ifp)
{
	struct iwi_softc *sc = ifp->if_softc;

	ifp->if_timer = 0;

	if (sc->sc_tx_timer > 0) {
		if (--sc->sc_tx_timer == 0) {
			printf("%s: device timeout\n", sc->sc_dev.dv_xname);
			ifp->if_flags &= ~IFF_UP;
			iwi_stop(ifp, 1);
			ifp->if_oerrors++;
			return;
		}
		ifp->if_timer = 1;
	}

	ieee80211_watchdog(ifp);
}

int
iwi_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct iwi_softc *sc = ifp->if_softc;
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
				iwi_init(ifp);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				iwi_stop(ifp, 1);
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

	case SIOCG80211TXPOWER:
		/*
		 * If the hardware radio transmitter switch is off, report a
		 * tx power of IEEE80211_TXPOWER_MIN to indicate that radio
		 * transmitter is killed.
		 */
		((struct ieee80211_txpower *)data)->i_val =
		    (CSR_READ_4(sc, IWI_CSR_IO) & IWI_IO_RADIO_ENABLED) ?
		    sc->sc_ic.ic_txpower : IEEE80211_TXPOWER_MIN;
		break;

	default:
		error = ieee80211_ioctl(ifp, cmd, data);
	}

	if (error == ENETRESET) {
		if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) ==
		    (IFF_UP | IFF_RUNNING))
			iwi_init(ifp);
		error = 0;
	}

	splx(s);
	return error;
}

void
iwi_stop_master(struct iwi_softc *sc)
{
	int ntries;

	/* disable interrupts */
	CSR_WRITE_4(sc, IWI_CSR_INTR_MASK, 0);

	CSR_WRITE_4(sc, IWI_CSR_RST, IWI_RST_STOP_MASTER);
	for (ntries = 0; ntries < 5; ntries++) {
		if (CSR_READ_4(sc, IWI_CSR_RST) & IWI_RST_MASTER_DISABLED)
			break;
		DELAY(10);
	}
	if (ntries == 5) {
		printf("%s: timeout waiting for master\n",
		    sc->sc_dev.dv_xname);
	}

	CSR_WRITE_4(sc, IWI_CSR_RST, CSR_READ_4(sc, IWI_CSR_RST) |
	    IWI_RST_PRINCETON_RESET);

	sc->flags &= ~IWI_FLAG_FW_INITED;
}

int
iwi_reset(struct iwi_softc *sc)
{
	int i, ntries;

	iwi_stop_master(sc);

	/* move adapter to D0 state */
	CSR_WRITE_4(sc, IWI_CSR_CTL, CSR_READ_4(sc, IWI_CSR_CTL) |
	    IWI_CTL_INIT);

	CSR_WRITE_4(sc, IWI_CSR_READ_INT, IWI_READ_INT_INIT_HOST);

	/* wait for clock stabilization */
	for (ntries = 0; ntries < 1000; ntries++) {
		if (CSR_READ_4(sc, IWI_CSR_CTL) & IWI_CTL_CLOCK_READY)
			break;
		DELAY(200);
	}
	if (ntries == 1000) {
		printf("%s: timeout waiting for clock stabilization\n",
		    sc->sc_dev.dv_xname);
		return ETIMEDOUT;
	}

	CSR_WRITE_4(sc, IWI_CSR_RST, CSR_READ_4(sc, IWI_CSR_RST) |
	    IWI_RST_SW_RESET);

	DELAY(10);

	CSR_WRITE_4(sc, IWI_CSR_CTL, CSR_READ_4(sc, IWI_CSR_CTL) |
	    IWI_CTL_INIT);

	/* clear NIC memory */
	CSR_WRITE_4(sc, IWI_CSR_AUTOINC_ADDR, 0);
	for (i = 0; i < 0xc000; i++)
		CSR_WRITE_4(sc, IWI_CSR_AUTOINC_DATA, 0);

	return 0;
}

int
iwi_load_ucode(struct iwi_softc *sc, const char *data, int size)
{
	const uint16_t *w;
	int ntries, i;

	CSR_WRITE_4(sc, IWI_CSR_RST, CSR_READ_4(sc, IWI_CSR_RST) |
	    IWI_RST_STOP_MASTER);
	for (ntries = 0; ntries < 5; ntries++) {
		if (CSR_READ_4(sc, IWI_CSR_RST) & IWI_RST_MASTER_DISABLED)
			break;
		DELAY(10);
	}
	if (ntries == 5) {
		printf("%s: timeout waiting for master\n",
		    sc->sc_dev.dv_xname);
		return ETIMEDOUT;
	}

	MEM_WRITE_4(sc, 0x3000e0, 0x80000000);
	DELAY(5000);

	CSR_WRITE_4(sc, IWI_CSR_RST, CSR_READ_4(sc, IWI_CSR_RST) &
	    ~IWI_RST_PRINCETON_RESET);

	DELAY(5000);
	MEM_WRITE_4(sc, 0x3000e0, 0);
	DELAY(1000);
	MEM_WRITE_4(sc, IWI_MEM_EVENT_CTL, 1);
	DELAY(1000);
	MEM_WRITE_4(sc, IWI_MEM_EVENT_CTL, 0);
	DELAY(1000);
	MEM_WRITE_1(sc, 0x200000, 0x00);
	MEM_WRITE_1(sc, 0x200000, 0x40);
	DELAY(1000);

	/* adapter is buggy, we must set the address for each word */
	for (w = (const uint16_t *)data; size > 0; w++, size -= 2)
		MEM_WRITE_2(sc, 0x200010, htole16(*w));

	MEM_WRITE_1(sc, 0x200000, 0x00);
	MEM_WRITE_1(sc, 0x200000, 0x80);

	/* wait until we get an answer */
	for (ntries = 0; ntries < 100; ntries++) {
		if (MEM_READ_1(sc, 0x200000) & 1)
			break;
		DELAY(100);
	}
	if (ntries == 100) {
		printf("%s: timeout waiting for ucode to initialize\n",
		    sc->sc_dev.dv_xname);
		return ETIMEDOUT;
	}

	/* read the answer or the firmware will not initialize properly */
	for (i = 0; i < 7; i++)
		MEM_READ_4(sc, 0x200004);

	MEM_WRITE_1(sc, 0x200000, 0x00);

	return 0;
}

/* macro to handle unaligned little endian data in firmware image */
#define GETLE32(p) ((p)[0] | (p)[1] << 8 | (p)[2] << 16 | (p)[3] << 24)

int
iwi_load_firmware(struct iwi_softc *sc, const char *data, int size)
{
	bus_dmamap_t map;
	bus_dma_segment_t seg;
	caddr_t virtaddr;
	u_char *p, *end;
	uint32_t sentinel, ctl, src, dst, sum, len, mlen;
	int ntries, nsegs, error;

	/* allocate DMA memory to store firmware image */
	error = bus_dmamap_create(sc->sc_dmat, size, 1, size, 0,
	    BUS_DMA_NOWAIT, &map);
	if (error != 0) {
		printf("%s: could not create firmware DMA map\n",
		    sc->sc_dev.dv_xname);
		goto fail1;
	}

	error = bus_dmamem_alloc(sc->sc_dmat, size, PAGE_SIZE, 0, &seg, 1,
	    &nsegs, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could allocate firmware DMA memory\n",
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
	    BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not load firmware DMA map\n",
		    sc->sc_dev.dv_xname);
		goto fail4;
	}

	/* copy firmware image to DMA memory */
	bcopy(data, virtaddr, size);

	/* make sure the adapter will get up-to-date values */
	bus_dmamap_sync(sc->sc_dmat, map, 0, size, BUS_DMASYNC_PREWRITE);

	/* tell the adapter where the command blocks are stored */
	MEM_WRITE_4(sc, 0x3000a0, 0x27000);

	/*
	 * Store command blocks into adapter's internal memory using register
	 * indirections. The adapter will read the firmware image through DMA
	 * using information stored in command blocks.
	 */
	src = map->dm_segs[0].ds_addr;
	p = virtaddr;
	end = p + size;
	CSR_WRITE_4(sc, IWI_CSR_AUTOINC_ADDR, 0x27000);

	while (p < end) {
		dst = GETLE32(p); p += 4; src += 4;
		len = GETLE32(p); p += 4; src += 4;
		p += len;

		while (len > 0) {
			mlen = min(len, IWI_CB_MAXDATALEN);

			ctl = IWI_CB_DEFAULT_CTL | mlen;
			sum = ctl ^ src ^ dst;

			/* write a command block */
			CSR_WRITE_4(sc, IWI_CSR_AUTOINC_DATA, ctl);
			CSR_WRITE_4(sc, IWI_CSR_AUTOINC_DATA, src);
			CSR_WRITE_4(sc, IWI_CSR_AUTOINC_DATA, dst);
			CSR_WRITE_4(sc, IWI_CSR_AUTOINC_DATA, sum);

			src += mlen;
			dst += mlen;
			len -= mlen;
		}
	}

	/* write a fictive final command block (sentinel) */
	sentinel = CSR_READ_4(sc, IWI_CSR_AUTOINC_ADDR);
	CSR_WRITE_4(sc, IWI_CSR_AUTOINC_DATA, 0);

	CSR_WRITE_4(sc, IWI_CSR_RST, CSR_READ_4(sc, IWI_CSR_RST) &
	    ~(IWI_RST_MASTER_DISABLED | IWI_RST_STOP_MASTER));

	/* tell the adapter to start processing command blocks */
	MEM_WRITE_4(sc, 0x3000a4, 0x540100);

	/* wait until the adapter has processed all command blocks */
	for (ntries = 0; ntries < 400; ntries++) {
		if (MEM_READ_4(sc, 0x3000d0) >= sentinel)
			break;
		DELAY(100);
	}
	if (ntries == 400) {
		printf("%s: timeout processing cb\n", sc->sc_dev.dv_xname);
		error = ETIMEDOUT;
		goto fail5;
	}

	/* we're done with command blocks processing */
	MEM_WRITE_4(sc, 0x3000a4, 0x540c00);

	/* allow interrupts so we know when the firmware is inited */
	CSR_WRITE_4(sc, IWI_CSR_INTR_MASK, IWI_INTR_MASK);

	/* tell the adapter to initialize the firmware */
	CSR_WRITE_4(sc, IWI_CSR_RST, 0);

	CSR_WRITE_4(sc, IWI_CSR_CTL, CSR_READ_4(sc, IWI_CSR_CTL) |
	    IWI_CTL_ALLOW_STANDBY);

	/* wait at most one second for firmware initialization to complete */
	if ((error = tsleep(sc, 0, "iwiinit", hz)) != 0) {
		printf("%s: timeout waiting for firmware initialization to "
		    "complete\n", sc->sc_dev.dv_xname);
		goto fail5;
	}

fail5:	bus_dmamap_sync(sc->sc_dmat, map, 0, size, BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(sc->sc_dmat, map);
fail4:	bus_dmamem_unmap(sc->sc_dmat, virtaddr, size);
fail3:	bus_dmamem_free(sc->sc_dmat, &seg, 1);
fail2:	bus_dmamap_destroy(sc->sc_dmat, map);
fail1:	return error;
}

int
iwi_config(struct iwi_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct iwi_configuration config;
	struct iwi_rateset rs;
	struct iwi_txpower power;
	struct ieee80211_wepkey *k;
	struct iwi_wep_key wepkey;
	uint32_t data;
	int error, nchan, i;

	IEEE80211_ADDR_COPY(ic->ic_myaddr, LLADDR(ifp->if_sadl));
	DPRINTF(("Setting MAC address to %s\n", ether_sprintf(ic->ic_myaddr)));
	error = iwi_cmd(sc, IWI_CMD_SET_MAC_ADDRESS, ic->ic_myaddr,
	    IEEE80211_ADDR_LEN, 0);
	if (error != 0)
		return error;

	bzero(&config, sizeof config);
	config.multicast_enabled = 1;
	config.silence_threshold = 30;
	config.report_noise = 1;
	config.answer_pbreq = (ic->ic_opmode == IEEE80211_M_IBSS) ? 1 : 0;
	DPRINTF(("Configuring adapter\n"));
	error = iwi_cmd(sc, IWI_CMD_SET_CONFIG, &config, sizeof config, 0);
	if (error != 0)
		return error;

	data = htole32(IWI_POWER_MODE_CAM);
	DPRINTF(("Setting power mode to %u\n", letoh32(data)));
	error = iwi_cmd(sc, IWI_CMD_SET_POWER_MODE, &data, sizeof data, 0);
	if (error != 0)
		return error;

	data = htole32(ic->ic_rtsthreshold);
	DPRINTF(("Setting RTS threshold to %u\n", letoh32(data)));
	error = iwi_cmd(sc, IWI_CMD_SET_RTS_THRESHOLD, &data, sizeof data, 0);
	if (error != 0)
		return error;

	data = htole32(ic->ic_fragthreshold);
	DPRINTF(("Setting fragmentation threshold to %u\n", letoh32(data)));
	error = iwi_cmd(sc, IWI_CMD_SET_FRAG_THRESHOLD, &data, sizeof data, 0);
	if (error != 0)
		return error;

	/*
	 * Set default Tx power for 802.11b/g and 802.11a channels.
	 */
	nchan = 0;
	for (i = 0; i <= IEEE80211_CHAN_MAX; i++) {
		if (!IEEE80211_IS_CHAN_2GHZ(&ic->ic_channels[i]))
			continue;
		power.chan[nchan].chan = i;
		power.chan[nchan].power = IWI_TXPOWER_MAX;
		nchan++;
	}
	power.nchan = nchan;

	power.mode = IWI_MODE_11G;
	DPRINTF(("Setting .11g channels tx power\n"));
	error = iwi_cmd(sc, IWI_CMD_SET_TX_POWER, &power, sizeof power, 0);
	if (error != 0)
		return error;

	power.mode = IWI_MODE_11B;
	DPRINTF(("Setting .11b channels tx power\n"));
	error = iwi_cmd(sc, IWI_CMD_SET_TX_POWER, &power, sizeof power, 0);
	if (error != 0)
		return error;

	nchan = 0;
	for (i = 0; i <= IEEE80211_CHAN_MAX; i++) {
		if (!IEEE80211_IS_CHAN_5GHZ(&ic->ic_channels[i]))
			continue;
		power.chan[nchan].chan = i;
		power.chan[nchan].power = IWI_TXPOWER_MAX;
		nchan++;
	}
	power.nchan = nchan;

	if (nchan > 0) {	/* 2915ABG only */
		power.mode = IWI_MODE_11A;
		DPRINTF(("Setting .11a channels tx power\n"));
		error = iwi_cmd(sc, IWI_CMD_SET_TX_POWER, &power, sizeof power,
		    0);
		if (error != 0)
			return error;
	}

	rs.mode = IWI_MODE_11G;
	rs.type = IWI_RATESET_TYPE_SUPPORTED;
	rs.nrates = ic->ic_sup_rates[IEEE80211_MODE_11G].rs_nrates;
	bcopy(ic->ic_sup_rates[IEEE80211_MODE_11G].rs_rates, rs.rates,
	    rs.nrates);
	DPRINTF(("Setting .11bg supported rates (%u)\n", rs.nrates));
	error = iwi_cmd(sc, IWI_CMD_SET_RATES, &rs, sizeof rs, 0);
	if (error != 0)
		return error;

	rs.mode = IWI_MODE_11A;
	rs.type = IWI_RATESET_TYPE_SUPPORTED;
	rs.nrates = ic->ic_sup_rates[IEEE80211_MODE_11A].rs_nrates;
	bcopy(ic->ic_sup_rates[IEEE80211_MODE_11A].rs_rates, rs.rates,
	    rs.nrates);
	DPRINTF(("Setting .11a supported rates (%u)\n", rs.nrates));
	error = iwi_cmd(sc, IWI_CMD_SET_RATES, &rs, sizeof rs, 0);
	if (error != 0)
		return error;

	/* if we have a desired ESSID, set it now */
	if (ic->ic_des_esslen != 0) {
#ifdef IWI_DEBUG
		if (iwi_debug > 0) {
			printf("Setting desired ESSID to ");
			ieee80211_print_essid(ic->ic_des_essid,
			    ic->ic_des_esslen);
			printf("\n");
		}
#endif
		error = iwi_cmd(sc, IWI_CMD_SET_ESSID, ic->ic_des_essid,
		    ic->ic_des_esslen, 0);
		if (error != 0)
			return error;
	}

	data = htole32(arc4random());
	DPRINTF(("Setting initialization vector to %u\n", letoh32(data)));
	error = iwi_cmd(sc, IWI_CMD_SET_IV, &data, sizeof data, 0);
	if (error != 0)
		return error;

	if (ic->ic_flags & IEEE80211_F_WEPON) {
		k = ic->ic_nw_keys;
		for (i = 0; i < IEEE80211_WEP_NKID; i++, k++) {
			wepkey.cmd = IWI_WEP_KEY_CMD_SETKEY;
			wepkey.idx = i;
			wepkey.len = k->wk_len;
			bzero(wepkey.key, sizeof wepkey.key);
			bcopy(k->wk_key, wepkey.key, k->wk_len);
			DPRINTF(("Setting wep key index %u len %u\n",
			    wepkey.idx, wepkey.len));
			error = iwi_cmd(sc, IWI_CMD_SET_WEP_KEY, &wepkey,
			    sizeof wepkey, 0);
			if (error != 0)
				return error;
		}
	}

	/* enable adapter */
	DPRINTF(("Enabling adapter\n"));
	return iwi_cmd(sc, IWI_CMD_ENABLE, NULL, 0, 0);
}

int
iwi_set_chan(struct iwi_softc *sc, struct ieee80211_channel *chan)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct iwi_scan scan;

	bzero(&scan, sizeof scan);
	memset(scan.type, IWI_SCAN_TYPE_PASSIVE, sizeof scan.type);
	scan.passive = htole16(2000);
	scan.channels[0] = 1 |
	    (IEEE80211_IS_CHAN_5GHZ(chan) ? IWI_CHAN_5GHZ : IWI_CHAN_2GHZ);
	scan.channels[1] = ieee80211_chan2ieee(ic, chan);

	DPRINTF(("Setting channel to %u\n", ieee80211_chan2ieee(ic, chan)));
	return iwi_cmd(sc, IWI_CMD_SCAN, &scan, sizeof scan, 1);
}

int
iwi_scan(struct iwi_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct iwi_scan scan;
	uint8_t *p;
	int i, count;

	bzero(&scan, sizeof scan);

	if (ic->ic_des_esslen != 0) {
		scan.bdirected = htole16(40);
		memset(scan.type, IWI_SCAN_TYPE_BDIRECTED, sizeof scan.type);
	} else {
		scan.broadcast = htole16(40);
		memset(scan.type, IWI_SCAN_TYPE_BROADCAST, sizeof scan.type);
	}

	p = scan.channels;
	count = 0;
	for (i = 0; i <= IEEE80211_CHAN_MAX; i++) {
		if (IEEE80211_IS_CHAN_5GHZ(&ic->ic_channels[i])) {
			*++p = i;
			count++;
		}
	}
	*(p - count) = IWI_CHAN_5GHZ | count;

	p = (count > 0) ? p + 1 : scan.channels;
	count = 0;
	for (i = 0; i <= IEEE80211_CHAN_MAX; i++) {
		if (IEEE80211_IS_CHAN_2GHZ(&ic->ic_channels[i])) {
			*++p = i;
			count++;
		}
	}
	*(p - count) = IWI_CHAN_2GHZ | count;

	DPRINTF(("Start scanning\n"));
	return iwi_cmd(sc, IWI_CMD_SCAN, &scan, sizeof scan, 1);
}

int
iwi_auth_and_assoc(struct iwi_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni = ic->ic_bss;
	struct iwi_configuration config;
	struct iwi_associate assoc;
	struct iwi_rateset rs;
	uint16_t capinfo;
	uint32_t data;
	int error;

	/* update adapter configuration */
	bzero(&config, sizeof config);
	config.multicast_enabled = 1;
	config.silence_threshold = 30;
	config.report_noise = 1;
	config.answer_pbreq = (ic->ic_opmode == IEEE80211_M_IBSS) ? 1 : 0;
	if (ic->ic_curmode == IEEE80211_MODE_11G)
		config.bg_autodetection = 1;
	DPRINTF(("Configuring adapter\n"));
	error = iwi_cmd(sc, IWI_CMD_SET_CONFIG, &config, sizeof config, 1);
	if (error != 0)
		return error;

#ifdef IWI_DEBUG
	if (iwi_debug > 0) {
		printf("Setting ESSID to ");
		ieee80211_print_essid(ni->ni_essid, ni->ni_esslen);
		printf("\n");
	}
#endif
	error = iwi_cmd(sc, IWI_CMD_SET_ESSID, ni->ni_essid, ni->ni_esslen, 1);
	if (error != 0)
		return error;

	/* the rate set has already been "negotiated" */
	rs.mode = IEEE80211_IS_CHAN_5GHZ(ni->ni_chan) ? IWI_MODE_11A :
	    IWI_MODE_11G;
	rs.type = IWI_RATESET_TYPE_NEGOTIATED;
	rs.nrates = ni->ni_rates.rs_nrates;
	if (rs.nrates > sizeof rs.rates) {
#ifdef DIAGNOSTIC
		/* should not happen since the rates are negotiated */
		printf("%s: XXX too many rates (count=%d, last=%d)\n",
		    sc->sc_dev.dv_xname, ni->ni_rates.rs_nrates,
		    ni->ni_rates.rs_rates[ni->ni_rates.rs_nrates - 1] &
		    IEEE80211_RATE_VAL);
#endif
		rs.nrates = sizeof rs.rates;
	}
	bcopy(ni->ni_rates.rs_rates, rs.rates, rs.nrates);
	DPRINTF(("Setting negotiated rates (%u)\n", rs.nrates));
	error = iwi_cmd(sc, IWI_CMD_SET_RATES, &rs, sizeof rs, 1);
	if (error != 0)
		return error;

	data = htole32(ni->ni_rssi);
	DPRINTF(("Setting sensitivity to %d\n", (int8_t)ni->ni_rssi));
	error = iwi_cmd(sc, IWI_CMD_SET_SENSITIVITY, &data, sizeof data, 1);
	if (error != 0)
		return error;

	bzero(&assoc, sizeof assoc);
	if (ic->ic_flags & IEEE80211_F_SIBSS)
		assoc.type = IWI_ASSOC_SIBSS;
	else
		assoc.type = IWI_ASSOC_ASSOCIATE;
	if (ic->ic_curmode == IEEE80211_MODE_11A)
		assoc.mode = IWI_MODE_11A;
	else if (ic->ic_curmode == IEEE80211_MODE_11B)
		assoc.mode = IWI_MODE_11B;
	else	/* assume 802.11b/g */
		assoc.mode = IWI_MODE_11G;
	assoc.chan = ieee80211_chan2ieee(ic, ni->ni_chan);
#if 0
	if (ni->ni_challenge != NULL)	/* XXX */
		assoc.auth = (ic->ic_wep_txkey << 4) | IWI_AUTH_SHARED;
#endif
	if (ic->ic_flags & IEEE80211_F_SHPREAMBLE)
		assoc.plen = IWI_ASSOC_SHPREAMBLE;
	bcopy(ni->ni_tstamp, assoc.tstamp, 8);
	capinfo = IEEE80211_CAPINFO_ESS;
	if (ic->ic_flags & IEEE80211_F_WEPON)
		capinfo |= IEEE80211_CAPINFO_PRIVACY;
	if ((ic->ic_flags & IEEE80211_F_SHPREAMBLE) &&
	    IEEE80211_IS_CHAN_2GHZ(ni->ni_chan))
		capinfo |= IEEE80211_CAPINFO_SHORT_PREAMBLE;
	if (ic->ic_flags & IEEE80211_F_SHSLOT)
		capinfo |= IEEE80211_CAPINFO_SHORT_SLOTTIME;
	assoc.capinfo = htole16(capinfo);

	assoc.lintval = htole16(ic->ic_lintval);
	assoc.intval = htole16(ni->ni_intval);
	IEEE80211_ADDR_COPY(assoc.bssid, ni->ni_bssid);
	if (ic->ic_opmode == IEEE80211_M_IBSS)
		IEEE80211_ADDR_COPY(assoc.dst, etherbroadcastaddr);
	else
		IEEE80211_ADDR_COPY(assoc.dst, ni->ni_bssid);

	DPRINTF(("Trying to associate to %s channel %u auth %u\n",
	    ether_sprintf(assoc.bssid), assoc.chan, assoc.auth));
	return iwi_cmd(sc, IWI_CMD_ASSOCIATE, &assoc, sizeof assoc, 1);
}

int
iwi_init(struct ifnet *ifp)
{
	struct iwi_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct iwi_firmware_hdr *hdr;
	const char *name, *fw;
	u_char *data;
	size_t size;
	int i, error;

	iwi_stop(ifp, 0);

	if ((error = iwi_reset(sc)) != 0) {
		printf("%s: could not reset adapter\n", sc->sc_dev.dv_xname);
		goto fail1;
	}

	switch (sc->sc_ic.ic_opmode) {
	case IEEE80211_M_STA:
	case IEEE80211_M_HOSTAP:
		name = "iwi-bss";
		break;
	case IEEE80211_M_IBSS:
	case IEEE80211_M_AHDEMO:
		name = "iwi-ibss";
		break;
	case IEEE80211_M_MONITOR:
		name = "iwi-monitor";
		break;
	default:
		name = NULL;	/* should not get there */
	}

	if ((error = loadfirmware(name, &data, &size)) != 0) {
		printf("%s: could not read firmware %s\n",
		    sc->sc_dev.dv_xname, name);
		goto fail1;
	}

	if (size < sizeof (struct iwi_firmware_hdr)) {
		printf("%s: firmware image too short: %u bytes\n",
		    sc->sc_dev.dv_xname, size);
		error = EINVAL;
		goto fail2;
	}

	hdr = (struct iwi_firmware_hdr *)data;

	if (hdr->vermaj < 3 || hdr->bootsz == 0 || hdr->ucodesz == 0 ||
	    hdr->mainsz == 0) {
		printf("%s: firmware image too old (need at least 3.0)\n",
		    sc->sc_dev.dv_xname);
		error = EINVAL;
		goto fail2;
	}

	if (size < sizeof (struct iwi_firmware_hdr) + letoh32(hdr->bootsz) +
	    letoh32(hdr->ucodesz) + letoh32(hdr->mainsz)) {
		printf("%s: firmware image too short: %u bytes\n",
		    sc->sc_dev.dv_xname, size);
		error = EINVAL;
		goto fail2;
	}

	fw = (const char *)data + sizeof (struct iwi_firmware_hdr);
	if ((error = iwi_load_firmware(sc, fw, letoh32(hdr->bootsz))) != 0) {
		printf("%s: could not load boot firmware\n",
		    sc->sc_dev.dv_xname);
		goto fail2;
	}

	fw = (const char *)data + sizeof (struct iwi_firmware_hdr) +
	    letoh32(hdr->bootsz);
	if ((error = iwi_load_ucode(sc, fw, letoh32(hdr->ucodesz))) != 0) {
		printf("%s: could not load microcode\n", sc->sc_dev.dv_xname);
		goto fail2;
	}

	iwi_stop_master(sc);

	CSR_WRITE_4(sc, IWI_CSR_CMD_BASE, sc->cmdq.map->dm_segs[0].ds_addr);
	CSR_WRITE_4(sc, IWI_CSR_CMD_SIZE, IWI_CMD_RING_COUNT);
	CSR_WRITE_4(sc, IWI_CSR_CMD_WIDX, sc->cmdq.cur);

	CSR_WRITE_4(sc, IWI_CSR_TX1_BASE, sc->txq[0].map->dm_segs[0].ds_addr);
	CSR_WRITE_4(sc, IWI_CSR_TX1_SIZE, IWI_TX_RING_COUNT);
	CSR_WRITE_4(sc, IWI_CSR_TX1_WIDX, sc->txq[0].cur);

	CSR_WRITE_4(sc, IWI_CSR_TX2_BASE, sc->txq[1].map->dm_segs[0].ds_addr);
	CSR_WRITE_4(sc, IWI_CSR_TX2_SIZE, IWI_TX_RING_COUNT);
	CSR_WRITE_4(sc, IWI_CSR_TX2_WIDX, sc->txq[1].cur);

	CSR_WRITE_4(sc, IWI_CSR_TX3_BASE, sc->txq[2].map->dm_segs[0].ds_addr);
	CSR_WRITE_4(sc, IWI_CSR_TX3_SIZE, IWI_TX_RING_COUNT);
	CSR_WRITE_4(sc, IWI_CSR_TX3_WIDX, sc->txq[2].cur);

	CSR_WRITE_4(sc, IWI_CSR_TX4_BASE, sc->txq[3].map->dm_segs[0].ds_addr);
	CSR_WRITE_4(sc, IWI_CSR_TX4_SIZE, IWI_TX_RING_COUNT);
	CSR_WRITE_4(sc, IWI_CSR_TX4_WIDX, sc->txq[3].cur);

	for (i = 0; i < IWI_RX_RING_COUNT; i++) {
		struct iwi_rx_data *data = &sc->rxq.data[i];
		CSR_WRITE_4(sc, data->reg, data->map->dm_segs[0].ds_addr);
	}

	CSR_WRITE_4(sc, IWI_CSR_RX_WIDX, IWI_RX_RING_COUNT - 1);

	fw = (const char *)data + sizeof (struct iwi_firmware_hdr) +
	    letoh32(hdr->bootsz) + letoh32(hdr->ucodesz);
	if ((error = iwi_load_firmware(sc, fw, letoh32(hdr->mainsz))) != 0) {
		printf("%s: could not load main firmware\n",
		    sc->sc_dev.dv_xname);
		goto fail2;
	}

	free(data, M_DEVBUF);
	sc->flags |= IWI_FLAG_FW_INITED;

	if ((error = iwi_config(sc)) != 0) {
		printf("%s: device configuration failed\n",
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

fail2:	free(data, M_DEVBUF);
fail1:	iwi_stop(ifp, 0);
	return error;
}

void
iwi_stop(struct ifnet *ifp, int disable)
{
	struct iwi_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	int i;

	sc->sc_tx_timer = 0;
	ifp->if_timer = 0;
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	ieee80211_new_state(ic, IEEE80211_S_INIT, -1);

	iwi_stop_master(sc);

	CSR_WRITE_4(sc, IWI_CSR_RST, IWI_RST_SW_RESET);

	/* reset rings */
	iwi_reset_cmd_ring(sc, &sc->cmdq);
	for (i = 0; i < 4; i++)
		iwi_reset_tx_ring(sc, &sc->txq[i]);
	iwi_reset_rx_ring(sc, &sc->rxq);
}

struct cfdriver iwi_cd = {
	NULL, "iwi", DV_IFNET
};
