/*	$OpenBSD: athn.c,v 1.25 2010/02/16 18:49:31 damien Exp $	*/

/*-
 * Copyright (c) 2009 Damien Bergamini <damien.bergamini@free.fr>
 * Copyright (c) 2008-2009 Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
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
 * Driver for Atheros 802.11a/g/n chipsets.
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
#include <sys/queue.h>
#include <sys/timeout.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/stdint.h>	/* uintptr_t */

#include <machine/bus.h>
#include <machine/endian.h>
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
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_amrr.h>
#include <net80211/ieee80211_radiotap.h>

#include <dev/ic/athnreg.h>
#include <dev/ic/athnvar.h>

#ifdef ATHN_DEBUG
int athn_debug = 0;
#endif

void		athn_radiotap_attach(struct athn_softc *);
void		athn_get_chanlist(struct athn_softc *);
const char *	athn_get_mac_name(struct athn_softc *);
const char *	athn_get_rf_name(struct athn_softc *);
int		athn_tx_alloc(struct athn_softc *);
void		athn_tx_free(struct athn_softc *);
int		athn_rx_alloc(struct athn_softc *);
void		athn_rx_free(struct athn_softc *);
void		athn_rx_start(struct athn_softc *);
void		athn_led_init(struct athn_softc *);
void		athn_btcoex_init(struct athn_softc *);
void		athn_btcoex_enable(struct athn_softc *);
void		athn_btcoex_disable(struct athn_softc *);
void		athn_rfsilent_init(struct athn_softc *);
void		athn_set_rxfilter(struct athn_softc *, uint32_t);
int		athn_gpio_read(struct athn_softc *, int);
void		athn_gpio_write(struct athn_softc *, int, int);
void		athn_gpio_config_output(struct athn_softc *, int, int);
void		athn_gpio_config_input(struct athn_softc *, int);
void		athn_get_chipid(struct athn_softc *);
int		athn_reset_power_on(struct athn_softc *);
int		athn_reset(struct athn_softc *, int);
void		athn_init_pll(struct athn_softc *,
		    const struct ieee80211_channel *);
int		athn_set_power_awake(struct athn_softc *);
void		athn_set_power_sleep(struct athn_softc *);
void		athn_write_serdes(struct athn_softc *, const uint32_t [9]);
void		athn_config_pcie(struct athn_softc *);
void		athn_config_nonpcie(struct athn_softc *);
uint8_t		athn_get_rf_rev(struct athn_softc *);
int		athn_set_chan(struct athn_softc *, struct ieee80211_channel *,
		    struct ieee80211_channel *);
int		athn_switch_chan(struct athn_softc *,
		    struct ieee80211_channel *, struct ieee80211_channel *);
void		athn_get_delta_slope(uint32_t, uint32_t *, uint32_t *);
void		athn_set_delta_slope(struct athn_softc *,
		    struct ieee80211_channel *, struct ieee80211_channel *);
void		athn_set_phy(struct athn_softc *, struct ieee80211_channel *,
		    struct ieee80211_channel *);
int		athn_read_rom_word(struct athn_softc *, uint32_t, uint16_t *);
int		athn_read_rom(struct athn_softc *);
void		athn_swap_rom(struct athn_softc *);
void		athn_reset_key(struct athn_softc *, int);
int		athn_set_key(struct ieee80211com *, struct ieee80211_node *,
		    struct ieee80211_key *);
void		athn_delete_key(struct ieee80211com *, struct ieee80211_node *,
		    struct ieee80211_key *);
void		athn_iter_func(void *, struct ieee80211_node *);
void		athn_calib_to(void *);
void		athn_do_calib(struct athn_softc *);
int		athn_init_calib(struct athn_softc *,
		    struct ieee80211_channel *, struct ieee80211_channel *);
void		athn_init_chains(struct athn_softc *);
uint8_t		athn_get_vpd(uint8_t, const uint8_t *, const uint8_t *, int);
int		athn_interpolate(int, int, int, int, int);
void		athn_init_baseband(struct athn_softc *);
void		athn_init_dma(struct athn_softc *);
void		athn_inc_tx_trigger_level(struct athn_softc *);
int		athn_stop_rx_dma(struct athn_softc *);
int		athn_rx_abort(struct athn_softc *);
int		athn_tx_pending(struct athn_softc *, int);
void		athn_stop_tx_dma(struct athn_softc *, int);
void		athn_tx_reclaim(struct athn_softc *, int);
void		athn_rx_radiotap(struct athn_softc *, struct mbuf *,
		    struct ar_rx_desc *);
void		athn_rx_intr(struct athn_softc *);
int		athn_tx_process(struct athn_softc *, int);
void		athn_tx_intr(struct athn_softc *);
int		athn_txtime(struct athn_softc *, int, int, u_int);
int		athn_tx(struct athn_softc *, struct mbuf *,
		    struct ieee80211_node *);
void		athn_set_beacon_timers(struct athn_softc *);
void		athn_set_rf_mode(struct athn_softc *,
		    struct ieee80211_channel *);
void		athn_set_opmode(struct athn_softc *);
void		athn_set_bss(struct athn_softc *, struct ieee80211_node *);
void		athn_enable_interrupts(struct athn_softc *);
void		athn_disable_interrupts(struct athn_softc *);
void		athn_hw_init(struct athn_softc *, struct ieee80211_channel *,
		    struct ieee80211_channel *);
void		athn_init_qos(struct athn_softc *);
int		athn_hw_reset(struct athn_softc *, struct ieee80211_channel *,
		    struct ieee80211_channel *);
struct		ieee80211_node *athn_node_alloc(struct ieee80211com *);
void		athn_newassoc(struct ieee80211com *, struct ieee80211_node *,
		    int);
int		athn_media_change(struct ifnet *);
void		athn_next_scan(void *);
int		athn_newstate(struct ieee80211com *, enum ieee80211_state,
		    int);
void		athn_updateedca(struct ieee80211com *);
void		athn_updateslot(struct ieee80211com *);
void		athn_start(struct ifnet *);
void		athn_watchdog(struct ifnet *);
void		athn_set_multi(struct athn_softc *);
int		athn_ioctl(struct ifnet *, u_long, caddr_t);
int		athn_init(struct ifnet *);
void		athn_stop(struct ifnet *, int);
void		athn_init_tx_queues(struct athn_softc *);
void		athn_calib_iq(struct athn_softc *);
void		athn_calib_adc_gain(struct athn_softc *);
void		athn_calib_adc_dc_off(struct athn_softc *);
void		athn_read_noisefloor(struct athn_softc *, int16_t *,
		    int16_t *);
void		athn_get_noisefloor(struct athn_softc *,
		    struct ieee80211_channel *);
void		athn_write_noisefloor(struct athn_softc *, int16_t *,
		    int16_t *);
void		athn_bb_load_noisefloor(struct athn_softc *);
void		athn_noisefloor_calib(struct athn_softc *);
int32_t		athn_ani_get_rssi(struct athn_softc *);
void		athn_ani_set_noise_immunity_level(struct athn_softc *, int);
void		athn_ani_ena_ofdm_weak_signal(struct athn_softc *);
void		athn_ani_dis_ofdm_weak_signal(struct athn_softc *);
void		athn_ani_set_cck_weak_signal(struct athn_softc *, int);
void		athn_ani_set_firstep_level(struct athn_softc *, int);
void		athn_ani_set_spur_immunity_level(struct athn_softc *, int);
void		athn_ani_ofdm_err_trigger(struct athn_softc *);
void		athn_ani_cck_err_trigger(struct athn_softc *);
void		athn_ani_lower_immunity(struct athn_softc *);
void		athn_ani_restart(struct athn_softc *);
void		athn_ani_monitor(struct athn_softc *);

struct cfdriver athn_cd = {
	NULL, "athn", DV_IFNET
};

int
athn_attach(struct athn_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct ar_base_eep_header *base;
	uint8_t eep_ver, kc_entries_exp;
	int error;

	if ((error = athn_reset_power_on(sc)) != 0) {
		printf(": could not reset chip\n");
		return (error);
	}

	if (AR_SREV_5416(sc) || AR_SREV_9160(sc))
		ar5416_attach(sc);
	else if (AR_SREV_9280(sc))
		ar9280_attach(sc);
	else if (AR_SREV_9285(sc))
		ar9285_attach(sc);
	else if (AR_SREV_9287(sc))
		ar9287_attach(sc);

	if ((error = athn_set_power_awake(sc)) != 0) {
		printf(": could not wakeup chip\n");
		return (error);
	}

	/* Retrieve PHY Revision. */
	sc->phy_rev = AR_READ(sc, AR_PHY_CHIP_ID);

	if (!(sc->flags & ATHN_FLAG_PCIE))
		athn_config_nonpcie(sc);
	else
		athn_config_pcie(sc);

	/* Allow access to analog chips. */
	AR_WRITE(sc, AR_PHY(0), 0x00000007);
	/* Get RF revision. */
	sc->rf_rev = athn_get_rf_rev(sc);

	/* Read entire ROM content. */
	if ((error = athn_read_rom(sc)) != 0) {
		printf(": could not read ROM\n");
		return (error);
	}
	base = sc->eep;

	/* We can put the chip in sleep state now. */
	athn_set_power_sleep(sc);

	eep_ver = (base->version >> 12) & 0xf;
	sc->eep_rev = (base->version & 0xfff);
	if (eep_ver != AR_EEP_VER || sc->eep_rev == 0) {
		printf(": unsupported ROM version %d.%d\n",  eep_ver,
		    sc->eep_rev);
		return (EINVAL);
	}

	sc->ops.setup(sc);

	IEEE80211_ADDR_COPY(ic->ic_myaddr, base->macAddr);
	printf(", address %s\n", ether_sprintf(ic->ic_myaddr));

	/* Check if we have a hardware radio switch. */
	if (base->rfSilent & AR_EEP_RFSILENT_ENABLED) {
		sc->flags |= ATHN_FLAG_RFSILENT;
		/* Get GPIO pin used by hardware radio switch. */
		sc->rfsilent_pin = MS(base->rfSilent,
		    AR_EEP_RFSILENT_GPIO_SEL);
		/* Get polarity of hardware radio switch. */
		if (base->rfSilent & AR_EEP_RFSILENT_POLARITY)
			sc->flags |= ATHN_FLAG_RFSILENT_REVERSED;
		DPRINTF(("Found RF switch connected to GPIO pin %d\n",
		    sc->rfsilent_pin));
	}

	/* Get the number of HW key cache entries. */
	kc_entries_exp = MS(base->deviceCap, AR_EEP_DEVCAP_KC_ENTRIES);
	sc->kc_entries = (kc_entries_exp != 0) ?
	    1 << kc_entries_exp : AR_KEYTABLE_SIZE;
	DPRINTF(("%d key cache entries\n", sc->kc_entries));
	/*
	 * In HostAP mode, the number of STAs that we can handle is
	 * limited by the number of entries in the HW key cache.
	 * XXX TKIP MMIC
	 */
	ic->ic_max_nnodes = sc->kc_entries - IEEE80211_GROUP_NKID;

	DPRINTF(("using %s loop power control\n",
	    (sc->flags & ATHN_FLAG_OLPC) ? "open" : "closed"));

	sc->txchainmask = base->txMask;
	if (sc->mac_ver == AR_SREV_VERSION_5416_PCI &&
	    !(base->opCapFlags & AR_OPFLAGS_11A)) {
		/* NB: Linux has a bug here. */
		/* For single-band AR5416 PCI, use GPIO pin 0. */
		sc->rxchainmask = athn_gpio_read(sc, 0) ? 0x5 : 0x7;
	} else
		sc->rxchainmask = base->rxMask;
	DPRINTF(("txchainmask=0x%x rxchainmask=0x%x\n",
	    sc->txchainmask, sc->rxchainmask));
	/* Count the number of bits set (in lowest 3 bits). */
	sc->ntxchains =
	    ((sc->txchainmask >> 2) & 1) +
	    ((sc->txchainmask >> 1) & 1) +
	    ((sc->txchainmask >> 0) & 1);
	sc->nrxchains =
	    ((sc->rxchainmask >> 2) & 1) +
	    ((sc->rxchainmask >> 1) & 1) +
	    ((sc->rxchainmask >> 0) & 1);

	error = athn_tx_alloc(sc);
	if (error != 0) {
		printf("%s: could not allocate Tx DMA resources\n",
		    sc->sc_dev.dv_xname);
		return (error);
	}
	error = athn_rx_alloc(sc);
	if (error != 0) {
		printf("%s: could not allocate Rx DMA resources\n",
		    sc->sc_dev.dv_xname);
		return (error);
	}

	if (AR_SINGLE_CHIP(sc)) {
		printf("%s: %s rev %d (%dT%dR), ROM rev %d\n",
		    sc->sc_dev.dv_xname, athn_get_mac_name(sc), sc->mac_rev,
		    sc->ntxchains, sc->nrxchains, sc->eep_rev);
	} else {
		printf("%s: MAC %s rev %d, RF %s (%dT%dR), ROM rev %d\n",
		    sc->sc_dev.dv_xname, athn_get_mac_name(sc), sc->mac_rev,
		    athn_get_rf_name(sc), sc->ntxchains, sc->nrxchains,
		    sc->eep_rev);
	}

	timeout_set(&sc->scan_to, athn_next_scan, sc);
	timeout_set(&sc->calib_to, athn_calib_to, sc);

	sc->amrr.amrr_min_success_threshold =  1;
	sc->amrr.amrr_max_success_threshold = 15;

	ic->ic_phytype = IEEE80211_T_OFDM;	/* not only, but not used */
	ic->ic_opmode = IEEE80211_M_STA;	/* default to BSS mode */
	ic->ic_state = IEEE80211_S_INIT;

	/* Set device capabilities. */
	ic->ic_caps =
	    IEEE80211_C_WEP |		/* WEP */
	    IEEE80211_C_RSN |		/* WPA/RSN */
	    IEEE80211_C_MONITOR |	/* monitor mode supported */
	    IEEE80211_C_SHSLOT |	/* short slot time supported */
	    IEEE80211_C_SHPREAMBLE |	/* short preamble supported */
	    IEEE80211_C_PMGT;		/* power saving supported */

#ifndef IEEE80211_NO_HT
	if (base->opCapFlags & AR_OPFLAGS_11N) {
		/* Set HT capabilities. */
		ic->ic_htcaps =
		    IEEE80211_HTCAP_SMPS_DIS |
		    IEEE80211_HTCAP_CBW20_40 |
		    IEEE80211_HTCAP_SGI40 |
		    IEEE80211_HTCAP_DSSSCCK40;
	}
#endif

	/* Set supported rates. */
	if (base->opCapFlags & AR_OPFLAGS_11G) {
		ic->ic_sup_rates[IEEE80211_MODE_11B] =
		    ieee80211_std_rateset_11b;
		ic->ic_sup_rates[IEEE80211_MODE_11G] =
		    ieee80211_std_rateset_11g;
	}
	if (base->opCapFlags & AR_OPFLAGS_11A) {
		ic->ic_sup_rates[IEEE80211_MODE_11A] =
		    ieee80211_std_rateset_11a;
	}
#ifndef IEEE80211_NO_HT
	if (base->opCapFlags & AR_OPFLAGS_11N) {
		/* Set supported HT rates. */
		ic->ic_sup_mcs[0] = 0xff;
		if (sc->nrxchains > 1)
			ic->ic_sup_mcs[1] = 0xff;
	}
#endif

	/* Get the list of auhtorized/supported channels. */
	athn_get_chanlist(sc);

	/* IBSS channel undefined for now. */
	ic->ic_ibss_chan = &ic->ic_channels[0];

	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_init = athn_init;
	ifp->if_ioctl = athn_ioctl;
	ifp->if_start = athn_start;
	ifp->if_watchdog = athn_watchdog;
	IFQ_SET_READY(&ifp->if_snd);
	memcpy(ifp->if_xname, sc->sc_dev.dv_xname, IFNAMSIZ);

	if_attach(ifp);
	ieee80211_ifattach(ifp);
	ic->ic_node_alloc = athn_node_alloc;
	ic->ic_newassoc = athn_newassoc;
	ic->ic_updateslot = athn_updateslot;
	ic->ic_updateedca = athn_updateedca;
#ifdef notyet
	ic->ic_set_key = athn_set_key;
	ic->ic_delete_key = athn_delete_key;
#endif

	/* Override 802.11 state transition machine. */
	sc->sc_newstate = ic->ic_newstate;
	ic->ic_newstate = athn_newstate;
	ieee80211_media_init(ifp, athn_media_change, ieee80211_media_status);

#if NBPFILTER > 0
	athn_radiotap_attach(sc);
#endif

	return (0);
}

void
athn_detach(struct athn_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ic.ic_if;
	int qid;

	timeout_del(&sc->scan_to);
	timeout_del(&sc->calib_to);

	/* Free Tx/Rx DMA resources. */
	for (qid = 0; qid < ATHN_QID_COUNT; qid++)
		athn_tx_reclaim(sc, qid);
	athn_tx_free(sc);
	athn_rx_free(sc);
	/* Free ROM copy. */
	if (sc->eep != NULL)
		free(sc->eep, M_DEVBUF);

	ieee80211_ifdetach(ifp);
	if_detach(ifp);
}

#if NBPFILTER > 0
/*
 * Attach the interface to 802.11 radiotap.
 */
void
athn_radiotap_attach(struct athn_softc *sc)
{
	bpfattach(&sc->sc_drvbpf, &sc->sc_ic.ic_if, DLT_IEEE802_11_RADIO,
	    sizeof (struct ieee80211_frame) + IEEE80211_RADIOTAP_HDRLEN);

	sc->sc_rxtap_len = sizeof sc->sc_rxtapu;
	sc->sc_rxtap.wr_ihdr.it_len = htole16(sc->sc_rxtap_len);
	sc->sc_rxtap.wr_ihdr.it_present = htole32(ATHN_RX_RADIOTAP_PRESENT);

	sc->sc_txtap_len = sizeof sc->sc_txtapu;
	sc->sc_txtap.wt_ihdr.it_len = htole16(sc->sc_txtap_len);
	sc->sc_txtap.wt_ihdr.it_present = htole32(ATHN_TX_RADIOTAP_PRESENT);
}
#endif

void
athn_get_chanlist(struct athn_softc *sc)
{
	struct ar_base_eep_header *base = sc->eep;
	struct ieee80211com *ic = &sc->sc_ic;
	uint8_t chan;
	int i;

	/* XXX Regulatory domain. */
	if (base->opCapFlags & AR_OPFLAGS_11G) {
		for (i = 1; i <= 14; i++) {
			chan = i;
			ic->ic_channels[chan].ic_freq =
			    ieee80211_ieee2mhz(chan, IEEE80211_CHAN_2GHZ);
			ic->ic_channels[chan].ic_flags =
			    IEEE80211_CHAN_CCK | IEEE80211_CHAN_OFDM |
			    IEEE80211_CHAN_DYN | IEEE80211_CHAN_2GHZ;
		}
	}
	if (base->opCapFlags & AR_OPFLAGS_11A) {
		for (i = 0; i < nitems(athn_5ghz_chans); i++) {
			chan = athn_5ghz_chans[i];
			ic->ic_channels[chan].ic_freq =
			    ieee80211_ieee2mhz(chan, IEEE80211_CHAN_5GHZ);
			ic->ic_channels[chan].ic_flags = IEEE80211_CHAN_A;
		}
	}
}

int
athn_tx_alloc(struct athn_softc *sc)
{
	struct athn_tx_buf *bf;
	bus_size_t size;
	int error, nsegs, i;

	/*
	 * Allocate a pool of Tx descriptors shared between all Tx queues.
	 */
	size = ATHN_NTXBUFS * ATHN_MAX_SCATTER * sizeof (struct ar_tx_desc);

	error = bus_dmamap_create(sc->sc_dmat, size, 1, size, 0,
	    BUS_DMA_NOWAIT, &sc->map);
	if (error != 0)
		goto fail;

	error = bus_dmamem_alloc(sc->sc_dmat, size, 4, 0, &sc->seg, 1,
	    &nsegs, BUS_DMA_NOWAIT | BUS_DMA_ZERO);
	if (error != 0)
		goto fail;

	error = bus_dmamem_map(sc->sc_dmat, &sc->seg, 1, size,
	    (caddr_t *)&sc->descs, BUS_DMA_NOWAIT | BUS_DMA_COHERENT);
	if (error != 0)
		goto fail;

	error = bus_dmamap_load_raw(sc->sc_dmat, sc->map, &sc->seg, 1, size,
	    BUS_DMA_NOWAIT);
	if (error != 0)
		goto fail;

	SIMPLEQ_INIT(&sc->txbufs);
	for (i = 0; i < ATHN_NTXBUFS; i++) {
		bf = &sc->txpool[i];

		error = bus_dmamap_create(sc->sc_dmat, ATHN_TXBUFSZ,
		    ATHN_MAX_SCATTER, ATHN_TXBUFSZ, 0, BUS_DMA_NOWAIT,
		    &bf->bf_map);
		if (error != 0) {
			printf("%s: could not create Tx buf DMA map\n",
			    sc->sc_dev.dv_xname);
			goto fail;
		}

		bf->bf_descs = &sc->descs[i * ATHN_MAX_SCATTER];
		bf->bf_daddr = sc->map->dm_segs[0].ds_addr +
		    i * ATHN_MAX_SCATTER * sizeof (struct ar_tx_desc);

		SIMPLEQ_INSERT_TAIL(&sc->txbufs, bf, bf_list);
	}
	return (0);
 fail:
	athn_tx_free(sc);
	return (error);
}

void
athn_tx_free(struct athn_softc *sc)
{
	struct athn_tx_buf *bf;
	int i;

	for (i = 0; i < ATHN_NTXBUFS; i++) {
		bf = &sc->txpool[i];

		if (bf->bf_map != NULL)
			bus_dmamap_destroy(sc->sc_dmat, bf->bf_map);
	}
	/* Free Tx descriptors. */
	if (sc->map != NULL) {
		if (sc->descs != NULL) {
			bus_dmamap_unload(sc->sc_dmat, sc->map);
			bus_dmamem_unmap(sc->sc_dmat, (caddr_t)sc->descs,
			    ATHN_NTXBUFS * ATHN_MAX_SCATTER *
			    sizeof (struct ar_tx_desc));
			bus_dmamem_free(sc->sc_dmat, &sc->seg, 1);
		}
		bus_dmamap_destroy(sc->sc_dmat, sc->map);
	}
}

int
athn_rx_alloc(struct athn_softc *sc)
{
	struct athn_rxq *rxq = &sc->rxq;
	struct athn_rx_buf *bf;
	struct ar_rx_desc *ds;
	bus_size_t size;
	int error, nsegs, i;

	size = ATHN_NRXBUFS * sizeof (struct ar_rx_desc);

	error = bus_dmamap_create(sc->sc_dmat, size, 1, size, 0,
	    BUS_DMA_NOWAIT, &rxq->map);
	if (error != 0)
		goto fail;

	error = bus_dmamem_alloc(sc->sc_dmat, size, 0, 0, &rxq->seg, 1,
	    &nsegs, BUS_DMA_NOWAIT | BUS_DMA_ZERO);
	if (error != 0)
		goto fail;

	error = bus_dmamem_map(sc->sc_dmat, &rxq->seg, 1, size,
	    (caddr_t *)&rxq->descs, BUS_DMA_NOWAIT | BUS_DMA_COHERENT);
	if (error != 0)
		goto fail;

	error = bus_dmamap_load_raw(sc->sc_dmat, rxq->map, &rxq->seg, 1,
	    size, BUS_DMA_NOWAIT);
	if (error != 0)
		goto fail;

	for (i = 0; i < ATHN_NRXBUFS; i++) {
		bf = &rxq->bf[i];
		ds = &rxq->descs[i];

		error = bus_dmamap_create(sc->sc_dmat, ATHN_RXBUFSZ, 1,
		    ATHN_RXBUFSZ, 0, BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW,
		    &bf->bf_map);
		if (error != 0) {
			printf("%s: could not create Rx buf DMA map\n",
			    sc->sc_dev.dv_xname);
			goto fail;
		}
		/*
		 * Assumes MCLGETI returns cache-line-size aligned buffers.
		 */
		bf->bf_m = MCLGETI(NULL, M_DONTWAIT, NULL, ATHN_RXBUFSZ);
		if (bf->bf_m == NULL) {
			printf("%s: could not allocate Rx mbuf\n",
			    sc->sc_dev.dv_xname);
			error = ENOBUFS;
			goto fail;
		}

		error = bus_dmamap_load(sc->sc_dmat, bf->bf_map,
		    mtod(bf->bf_m, void *), ATHN_RXBUFSZ, NULL,
		    BUS_DMA_NOWAIT | BUS_DMA_READ);
		if (error != 0) {
			printf("%s: could not DMA map Rx buffer\n",
			    sc->sc_dev.dv_xname);
			goto fail;
		}

		bus_dmamap_sync(sc->sc_dmat, bf->bf_map, 0,
		    bf->bf_map->dm_mapsize, BUS_DMASYNC_PREREAD);

		bf->bf_desc = ds;
		bf->bf_daddr = rxq->map->dm_segs[0].ds_addr +
		    i * sizeof (struct ar_rx_desc);
	}
	return (0);
 fail:
	athn_rx_free(sc);
	return (error);
}

void
athn_rx_free(struct athn_softc *sc)
{
	struct athn_rxq *rxq = &sc->rxq;
	struct athn_rx_buf *bf;
	int i;

	for (i = 0; i < ATHN_NRXBUFS; i++) {
		bf = &rxq->bf[i];

		if (bf->bf_map != NULL)
			bus_dmamap_destroy(sc->sc_dmat, bf->bf_map);
		if (bf->bf_m != NULL)
			m_freem(bf->bf_m);
	}
	/* Free Rx descriptors. */
	if (rxq->descs != NULL) {
		if (rxq->descs != NULL) {
			bus_dmamap_unload(sc->sc_dmat, rxq->map);
			bus_dmamem_unmap(sc->sc_dmat, (caddr_t)rxq->descs,
			    ATHN_NRXBUFS * sizeof (struct ar_rx_desc));
			bus_dmamem_free(sc->sc_dmat, &rxq->seg, 1);
		}
		bus_dmamap_destroy(sc->sc_dmat, rxq->map);
	}
}

void
athn_rx_start(struct athn_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct athn_rxq *rxq = &sc->rxq;
	struct athn_rx_buf *bf;
	struct ar_rx_desc *ds;
	uint32_t rfilt;
	int i;

	/* Setup and link Rx descriptors. */
	SIMPLEQ_INIT(&rxq->head);
	rxq->lastds = NULL;
	for (i = 0; i < ATHN_NRXBUFS; i++) {
		bf = &rxq->bf[i];
		ds = bf->bf_desc;

		memset(ds, 0, sizeof (*ds));
		ds->ds_data = bf->bf_map->dm_segs[0].ds_addr;
		ds->ds_ctl1 = SM(AR_RXC1_BUF_LEN, ATHN_RXBUFSZ);

		if (rxq->lastds != NULL)
			rxq->lastds->ds_link = bf->bf_daddr;
		SIMPLEQ_INSERT_TAIL(&rxq->head, bf, bf_list);
		rxq->lastds = ds;
	}
	bus_dmamap_sync(sc->sc_dmat, rxq->map, 0, rxq->map->dm_mapsize,
	    BUS_DMASYNC_PREREAD);

	/* Enable Rx. */
	AR_WRITE(sc, AR_RXDP, SIMPLEQ_FIRST(&rxq->head)->bf_daddr);
	AR_WRITE(sc, AR_CR, AR_CR_RXE);

	/* Set Rx filter. */
	rfilt = AR_RX_FILTER_UCAST | AR_RX_FILTER_BCAST | AR_RX_FILTER_MCAST;
#ifndef IEEE80211_NO_HT
	/* Want Compressed Block Ack Requests. */
	rfilt |= AR_RX_FILTER_COMPR_BAR;
#endif
	if (ic->ic_opmode != IEEE80211_M_STA) {
		rfilt |= AR_RX_FILTER_PROBEREQ;
		if (ic->ic_opmode == IEEE80211_M_MONITOR)
			rfilt |= AR_RX_FILTER_PROM;
#ifndef IEEE80211_STA_ONLY
		if (AR_SREV_9280_10_OR_LATER(sc) &&
		    ic->ic_opmode == IEEE80211_M_HOSTAP)
			rfilt |= AR_RX_FILTER_PSPOLL;
#endif
		rfilt |= AR_RX_FILTER_BEACON;
	} else
		rfilt |= AR_RX_FILTER_BEACON; /* XXX AR_RX_FILTER_MYBEACON */
	athn_set_rxfilter(sc, rfilt);

	/* Set BSSID mask. */
	AR_WRITE(sc, AR_BSSMSKL, 0xffffffff);
	AR_WRITE(sc, AR_BSSMSKU, 0xffff);

	athn_set_opmode(sc);

	/* Set multicast filter. */
	AR_WRITE(sc, AR_MCAST_FIL0, 0xffffffff);
	AR_WRITE(sc, AR_MCAST_FIL1, 0xffffffff);

	AR_WRITE(sc, AR_FILT_OFDM, 0);
	AR_WRITE(sc, AR_FILT_CCK, 0);
	AR_WRITE(sc, AR_MIBC, 0);
	AR_WRITE(sc, AR_PHY_ERR_MASK_1, AR_PHY_ERR_OFDM_TIMING);
	AR_WRITE(sc, AR_PHY_ERR_MASK_2, AR_PHY_ERR_CCK_TIMING);

	/* XXX ANI. */
	AR_WRITE(sc, AR_PHY_ERR_1, 0);
	AR_WRITE(sc, AR_PHY_ERR_2, 0);

	/* Disable HW crypto for now. */
	AR_SETBITS(sc, AR_DIAG_SW, AR_DIAG_ENCRYPT_DIS | AR_DIAG_DECRYPT_DIS);

	/* Start PCU Rx. */
	AR_CLRBITS(sc, AR_DIAG_SW, AR_DIAG_RX_DIS | AR_DIAG_RX_ABORT);
}

void
athn_set_rxfilter(struct athn_softc *sc, uint32_t rfilt)
{
	AR_WRITE(sc, AR_RX_FILTER, rfilt);

#ifdef notyet
	reg = AR_READ(sc, AR_PHY_ERR);
	reg &= (AR_PHY_ERR_RADAR | AR_PHY_ERR_OFDM_TIMING |
	    AR_PHY_ERR_CCK_TIMING);
	AR_WRITE(sc, AR_PHY_ERR, reg);
	if (reg != 0)
		AR_SETBITS(sc, AR_RXCFG, AR_RXCFG_ZLFDMA);
	else
		AR_CLRBITS(sc, AR_RXCFG, AR_RXCFG_ZLFDMA);
#else
	AR_WRITE(sc, AR_PHY_ERR, 0);
	AR_CLRBITS(sc, AR_RXCFG, AR_RXCFG_ZLFDMA);
#endif
}

int
athn_intr(void *xsc)
{
	struct athn_softc *sc = xsc;
	struct ifnet *ifp = &sc->sc_ic.ic_if;
	uint32_t intr, intr2, intr5, sync;

	if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) !=
	    (IFF_UP | IFF_RUNNING))
		return (1);

	/* Get pending interrupts. */
	intr = AR_READ(sc, AR_INTR_ASYNC_CAUSE);
	if (!(intr & AR_INTR_MAC_IRQ) || intr == AR_INTR_SPURIOUS) {
		intr = AR_READ(sc, AR_INTR_SYNC_CAUSE);
		if (intr == AR_INTR_SPURIOUS || (intr & sc->isync) == 0)
			return (1);	/* Not for us. */
	}

	if ((AR_READ(sc, AR_INTR_ASYNC_CAUSE) & AR_INTR_MAC_IRQ) &&
	    (AR_READ(sc, AR_RTC_STATUS) & AR_RTC_STATUS_M) == AR_RTC_STATUS_ON)
		intr = AR_READ(sc, AR_ISR);
	else
		intr = 0;
	sync = AR_READ(sc, AR_INTR_SYNC_CAUSE) & sc->isync;
	if (intr == 0 && sync == 0)
		return (1);	/* Not for us. */

	if (intr != 0) {
		if (intr & AR_ISR_BCNMISC) {
			intr2 = AR_READ(sc, AR_ISR_S2);
			if (intr2 & AR_ISR_S2_TIM)
				/* TBD */;
			if (intr2 & AR_ISR_S2_TSFOOR)
				/* TBD */;
		}
		intr = AR_READ(sc, AR_ISR_RAC);
		if (intr == AR_INTR_SPURIOUS)
			return (0);

		if (intr & (AR_ISR_RXMINTR | AR_ISR_RXINTM))
			athn_rx_intr(sc);
		if (intr & (AR_ISR_RXOK | AR_ISR_RXERR | AR_ISR_RXORN))
			athn_rx_intr(sc);

		if (intr & (AR_ISR_TXOK | AR_ISR_TXDESC |
		    AR_ISR_TXERR | AR_ISR_TXEOL))
			athn_tx_intr(sc);

		if (intr & AR_ISR_GENTMR) {
			intr5 = AR_READ(sc, AR_ISR_S5_S);
			if (intr5 & AR_ISR_GENTMR) {
				DPRINTF(("GENTMR trigger=%d thresh=%d\n",
				    MS(intr5, AR_ISR_S5_GENTIMER_TRIG),
				    MS(intr5, AR_ISR_S5_GENTIMER_THRESH)));
			}
		}

		intr5 = AR_READ(sc, AR_ISR_S5_S);
		if (intr5 & AR_ISR_S5_TIM_TIMER)
			/* TBD */;
	}
	if (sync != 0) {
		if (sync & (AR_INTR_SYNC_HOST1_FATAL |
		    AR_INTR_SYNC_HOST1_PERR))
			/* TBD */;

		if (sync & AR_INTR_SYNC_RADM_CPL_TIMEOUT) {
			AR_WRITE(sc, AR_RC, AR_RC_HOSTIF);
			AR_WRITE(sc, AR_RC, 0);
		}

		if ((sc->flags & ATHN_FLAG_RFSILENT) &&
		    (sync & AR_INTR_SYNC_GPIO_PIN(sc->rfsilent_pin))) {
			printf("%s: radio switch turned off\n",
			    sc->sc_dev.dv_xname);
			/* Turn the interface down. */
			ifp->if_flags &= ~IFF_UP;
			athn_stop(ifp, 1);
			return (0);
		}

		AR_WRITE(sc, AR_INTR_SYNC_CAUSE, sync);
		(void)AR_READ(sc, AR_INTR_SYNC_CAUSE);
	}
	return (0);
}

/*
 * Access to General Purpose Input/Output ports.
 */
int
athn_gpio_read(struct athn_softc *sc, int pin)
{
	KASSERT(pin < sc->ngpiopins);
	return ((AR_READ(sc, AR_GPIO_IN_OUT) >> (sc->ngpiopins + pin)) & 1);
}

void
athn_gpio_write(struct athn_softc *sc, int pin, int set)
{
	uint32_t reg;

	KASSERT(pin < sc->ngpiopins);
	reg = AR_READ(sc, AR_GPIO_IN_OUT);
	if (set)
		reg |= 1 << pin;
	else
		reg &= ~(1 << pin);
	AR_WRITE(sc, AR_GPIO_IN_OUT, reg);
}

void
athn_gpio_config_input(struct athn_softc *sc, int pin)
{
	uint32_t reg;

	reg = AR_READ(sc, AR_GPIO_OE_OUT);
	reg &= ~(AR_GPIO_OE_OUT_DRV_M << (pin * 2));
	reg |= AR_GPIO_OE_OUT_DRV_NO << (pin * 2);
	AR_WRITE(sc, AR_GPIO_OE_OUT, reg);
}

void
athn_gpio_config_output(struct athn_softc *sc, int pin, int type)
{
	uint32_t reg;
	int mux, off;

	mux = pin / 6;
	off = pin % 6;

	reg = AR_READ(sc, AR_GPIO_OUTPUT_MUX(mux));
	if (!AR_SREV_9280_20_OR_LATER(sc) && mux == 0)
		reg = (reg & ~0x1f0) | (reg & 0x1f0) << 1;
	reg &= ~(0x1f << (off * 5));
	reg |= (type & 0x1f) << (off * 5);
	AR_WRITE(sc, AR_GPIO_OUTPUT_MUX(mux), reg);

	reg = AR_READ(sc, AR_GPIO_OE_OUT);
	reg &= ~(AR_GPIO_OE_OUT_DRV_M << (pin * 2));
	reg |= AR_GPIO_OE_OUT_DRV_ALL << (pin * 2);
	AR_WRITE(sc, AR_GPIO_OE_OUT, reg);
}

void
athn_get_chipid(struct athn_softc *sc)
{
	uint32_t reg;

	reg = AR_READ(sc, AR_SREV);
	if (MS(reg, AR_SREV_ID) == 0xff) {
		sc->mac_ver = MS(reg, AR_SREV_VERSION2);
		sc->mac_rev = MS(reg, AR_SREV_REVISION2);
		if (!(reg & AR_SREV_TYPE2_HOST_MODE))
			sc->flags |= ATHN_FLAG_PCIE;
	} else {
		sc->mac_ver = MS(reg, AR_SREV_VERSION);
		sc->mac_rev = MS(reg, AR_SREV_REVISION);
		if (sc->mac_ver == AR_SREV_VERSION_5416_PCIE)
			sc->flags |= ATHN_FLAG_PCIE;
	}
}

const char *
athn_get_mac_name(struct athn_softc *sc)
{
	switch (sc->mac_ver) {
	case AR_SREV_VERSION_5416_PCI:
		return ("AR5416");
	case AR_SREV_VERSION_5416_PCIE:
		return ("AR5418");
	case AR_SREV_VERSION_9160:
		return ("AR9160");
	case AR_SREV_VERSION_9280:
		return ("AR9280");
	case AR_SREV_VERSION_9285:
		return ("AR9285");
	case AR_SREV_VERSION_9287:
		return ("AR9287");
	}
	return ("unknown");
}

const char *
athn_get_rf_name(struct athn_softc *sc)
{
	switch (sc->rf_rev) {
	case AR_RAD5133_SREV_MAJOR:	/* Dual-band 3T3R. */
		return ("AR5133");
	case AR_RAD2133_SREV_MAJOR:	/* Single-band 3T3R. */
		return ("AR2133");
	case AR_RAD5122_SREV_MAJOR:	/* Dual-band 2T2R. */
		return ("AR5122");
	case AR_RAD2122_SREV_MAJOR:	/* Single-band 2T2R. */
		return ("AR2122");
	}
	return ("unknown");
}

int
athn_reset_power_on(struct athn_softc *sc)
{
	int ntries;

	/* Set force wake. */
	AR_WRITE(sc, AR_RTC_FORCE_WAKE,
	    AR_RTC_FORCE_WAKE_EN | AR_RTC_FORCE_WAKE_ON_INT);

	/* Make sure no DMA is active by doing an AHB reset. */
	AR_WRITE(sc, AR_RC, AR_RC_AHB);

	/* RTC reset and clear. */
	AR_WRITE(sc, AR_RTC_RESET, 0);
	DELAY(2);
	AR_WRITE(sc, AR_RC, 0);
	AR_WRITE(sc, AR_RTC_RESET, 1);

	/* Poll until RTC is ON. */
	for (ntries = 0; ntries < 1000; ntries++) {
		if ((AR_READ(sc, AR_RTC_STATUS) & AR_RTC_STATUS_M) ==
		    AR_RTC_STATUS_ON)
			break;
		DELAY(10);
	}
	if (ntries == 1000) {
		DPRINTF(("RTC not waking up\n"));
		return (ETIMEDOUT);
	}

	/* Read hardware revision. */
	athn_get_chipid(sc);

	return (athn_reset(sc, 0));
}

int
athn_reset(struct athn_softc *sc, int cold)
{
	int ntries;

	/* Set force wake. */
	AR_WRITE(sc, AR_RTC_FORCE_WAKE,
	    AR_RTC_FORCE_WAKE_EN | AR_RTC_FORCE_WAKE_ON_INT);

	if (AR_READ(sc, AR_INTR_SYNC_CAUSE) &
	    (AR_INTR_SYNC_LOCAL_TIMEOUT | AR_INTR_SYNC_RADM_CPL_TIMEOUT)) {
		AR_WRITE(sc, AR_INTR_SYNC_ENABLE, 0);
		AR_WRITE(sc, AR_RC, AR_RC_AHB | AR_RC_HOSTIF);
	} else
		AR_WRITE(sc, AR_RC, AR_RC_AHB);

	AR_WRITE(sc, AR_RTC_RC, AR_RTC_RC_MAC_WARM |
	    (cold ? AR_RTC_RC_MAC_COLD : 0));
	DELAY(50);
	AR_WRITE(sc, AR_RTC_RC, 0);
	for (ntries = 0; ntries < 1000; ntries++) {
		if (!(AR_READ(sc, AR_RTC_RC) &
		      (AR_RTC_RC_MAC_WARM | AR_RTC_RC_MAC_COLD)))
			break;
		DELAY(10);
	}
	if (ntries == 1000) {
		DPRINTF(("RTC stuck in MAC reset\n"));
		return (ETIMEDOUT);
	}
	AR_WRITE(sc, AR_RC, 0);
	return (0);
}

int
athn_set_power_awake(struct athn_softc *sc)
{
	int ntries, error;

	/* Do a Power-On-Reset if shutdown. */
	if ((AR_READ(sc, AR_RTC_STATUS) & AR_RTC_STATUS_M) ==
	    AR_RTC_STATUS_SHUTDOWN) {
		if ((error = athn_reset_power_on(sc)) != 0)
			return (error);
		athn_init_pll(sc, NULL);
	}
	AR_SETBITS(sc, AR_RTC_FORCE_WAKE, AR_RTC_FORCE_WAKE_EN);
	DELAY(50);	/* Give chip the chance to awake. */

	/* Poll until RTC is ON. */
	for (ntries = 0; ntries < 4000; ntries++) {
		if ((AR_READ(sc, AR_RTC_STATUS) & AR_RTC_STATUS_M) ==
		    AR_RTC_STATUS_ON)
			break;
		DELAY(50);
		AR_SETBITS(sc, AR_RTC_FORCE_WAKE, AR_RTC_FORCE_WAKE_EN);
	}
	if (ntries == 4000) {
		DPRINTF(("RTC not waking up\n"));
		return (ETIMEDOUT);
	}

	AR_CLRBITS(sc, AR_STA_ID1, AR_STA_ID1_PWR_SAV);
	return (0);
}

void
athn_set_power_sleep(struct athn_softc *sc)
{
	AR_SETBITS(sc, AR_STA_ID1, AR_STA_ID1_PWR_SAV);
	AR_CLRBITS(sc, AR_RTC_FORCE_WAKE, AR_RTC_FORCE_WAKE_EN);
	AR_WRITE(sc, AR_RC, AR_RC_AHB | AR_RC_HOSTIF);
	/*
	 * NB: Clearing RTC_RESET_EN when setting the chip to sleep mode
	 * results in high power consumption on AR5416 chipsets.
	 */
	if (!AR_SREV_5416(sc))
		AR_CLRBITS(sc, AR_RTC_RESET, AR_RTC_RESET_EN);
}

void
athn_init_pll(struct athn_softc *sc, const struct ieee80211_channel *c)
{
	uint32_t pll;

	if (AR_SREV_9280_10_OR_LATER(sc)) {
		pll = SM(AR_RTC_9160_PLL_REFDIV, 0x05);
		if (c != NULL && IEEE80211_IS_CHAN_5GHZ(c)) {
			if (AR_SREV_9280_20(sc)) {
		 		/* Workaround for AR9280 2.0/5GHz. */
				if ((c->ic_freq % 20) == 0 ||
				    (c->ic_freq % 10) == 0)
					pll = 0x2850;
				else
					pll = 0x142c;
			} else
				pll |= SM(AR_RTC_9160_PLL_DIV, 0x28);
		} else
			pll |= SM(AR_RTC_9160_PLL_DIV, 0x2c);
	} else if (AR_SREV_9160_10_OR_LATER(sc)) {
		pll = SM(AR_RTC_9160_PLL_REFDIV, 0x05);
		if (c != NULL && IEEE80211_IS_CHAN_5GHZ(c))
			pll |= SM(AR_RTC_9160_PLL_DIV, 0x50);
		else
			pll |= SM(AR_RTC_9160_PLL_DIV, 0x58);
	} else {
		pll = AR_RTC_PLL_REFDIV_5 | AR_RTC_PLL_DIV2;
		if (c != NULL && IEEE80211_IS_CHAN_5GHZ(c))
			pll |= SM(AR_RTC_PLL_DIV, 0x0a);
		else
			pll |= SM(AR_RTC_PLL_DIV, 0x0b);
	}
	DPRINTFN(5, ("AR_RTC_PLL_CONTROL=0x%08x\n", pll));
	AR_WRITE(sc, AR_RTC_PLL_CONTROL, pll);
	DELAY(100);
	AR_WRITE(sc, AR_RTC_SLEEP_CLK, AR_RTC_FORCE_DERIVED_CLK);
}

void
athn_write_serdes(struct athn_softc *sc, const uint32_t val[9])
{
	int i;

	/* Write 288-bit value to Serializer/Deserializer. */
	for (i = 0; i < 288 / 32; i++)
		AR_WRITE(sc, AR_PCIE_SERDES, val[i]);
	AR_WRITE(sc, AR_PCIE_SERDES2, 0);
}

void
athn_config_pcie(struct athn_softc *sc)
{
	/* Disable PLL when in L0s as well as receiver clock when in L1. */
	athn_write_serdes(sc, sc->serdes);

	DELAY(1000);
	/* Allow forcing of PCIe core into L1 state. */
	AR_SETBITS(sc, AR_PCIE_PM_CTRL, AR_PCIE_PM_CTRL_ENA);

#ifndef ATHN_PCIE_WAEN
	AR_WRITE(sc, AR_WA, sc->workaround);
#else
	AR_WRITE(sc, AR_WA, ATHN_PCIE_WAEN);
#endif
}

void
athn_config_nonpcie(struct athn_softc *sc)
{
	athn_write_serdes(sc, ar_nonpcie_serdes);
}

uint8_t
athn_reverse_bits(uint8_t v, int nbits)
{
	KASSERT(nbits <= 8);
	v = ((v >> 1) & 0x55) | ((v & 0x55) << 1);
	v = ((v >> 2) & 0x33) | ((v & 0x33) << 2);
	v = ((v >> 4) & 0x0f) | ((v & 0x0f) << 4);
	return (v >> (8 - nbits));
}

uint8_t
athn_get_rf_rev(struct athn_softc *sc)
{
	uint8_t rev, reg;
	int i;

	AR_WRITE(sc, AR_PHY(0x36), 0x00007058);
	for (i = 0; i < 8; i++)
		AR_WRITE(sc, AR_PHY(0x20), 0x00010000);
	reg = (AR_READ(sc, AR_PHY(256)) >> 24) & 0xff;
	reg = (reg & 0xf0) >> 4 | (reg & 0x0f) << 4;

	rev = athn_reverse_bits(reg, 8);
	if ((rev & AR_RADIO_SREV_MAJOR) == 0)
		rev = AR_RAD5133_SREV_MAJOR;
	return (rev);
}

static __inline uint32_t
athn_synth_delay(struct athn_softc *sc)
{
	uint32_t delay;

	delay = MS(AR_READ(sc, AR_PHY_RX_DELAY), AR_PHY_RX_DELAY_DELAY);
	if (sc->sc_ic.ic_curmode == IEEE80211_MODE_11B)
		delay = (delay * 4) / 22;
	else
		delay = delay / 10;	/* in 100ns steps */
	return (delay);
}

int
athn_set_chan(struct athn_softc *sc, struct ieee80211_channel *c,
    struct ieee80211_channel *extc)
{
	struct athn_ops *ops = &sc->ops;
	int error, qid, ntries;

	/* Check that Tx is stopped, otherwise RF Bus grant will not work. */
	for (qid = 0; qid < ATHN_QID_COUNT; qid++)
		if (athn_tx_pending(sc, qid))
			return (EBUSY);

	/* Request RF Bus grant. */
	AR_WRITE(sc, AR_PHY_RFBUS_REQ, AR_PHY_RFBUS_REQ_EN);
	for (ntries = 0; ntries < 10000; ntries++) {
		if (AR_READ(sc, AR_PHY_RFBUS_GRANT) & AR_PHY_RFBUS_GRANT_EN)
			break;
		DELAY(10);
	}
	if (ntries == 10000) {
		DPRINTF(("could not kill baseband Rx"));
		return (ETIMEDOUT);
	}

	athn_set_phy(sc, c, extc);

	/* Change the synthesizer. */
	if ((error = ops->set_synth(sc, c, extc)) != 0)
		return (error);

	sc->curchan = c;
	sc->curchanext = extc;

	/* Set transmit power values for new channel. */
	ops->set_txpower(sc, c, extc);

	/* Wait for the synthesizer to settle. */
	DELAY(AR_BASE_PHY_ACTIVE_DELAY + athn_synth_delay(sc));

	/* Release the RF Bus grant. */
	AR_WRITE(sc, AR_PHY_RFBUS_REQ, 0);

	/* Write delta slope coeffs for modes where OFDM may be used. */
	if (sc->sc_ic.ic_curmode != IEEE80211_MODE_11B)
		athn_set_delta_slope(sc, c, extc);

	ops->spur_mitigate(sc, c, extc);
	/* XXX Load noisefloor values and start calibration. */

	return (0);
}

int
athn_switch_chan(struct athn_softc *sc, struct ieee80211_channel *c,
    struct ieee80211_channel *extc)
{
	int error, qid;

	/* Disable interrupts. */
	athn_disable_interrupts(sc);

	/* Stop all Tx queues. */
	for (qid = 0; qid < ATHN_QID_COUNT; qid++)
		athn_stop_tx_dma(sc, qid);
	for (qid = 0; qid < ATHN_QID_COUNT; qid++)
		athn_tx_reclaim(sc, qid);

	/* Stop Rx. */
	AR_SETBITS(sc, AR_DIAG_SW, AR_DIAG_RX_DIS);
	AR_WRITE(sc, AR_MIBC, AR_MIBC_FMC);
	AR_WRITE(sc, AR_MIBC, AR_MIBC_CMC);
	AR_WRITE(sc, AR_FILT_OFDM, 0);
	AR_WRITE(sc, AR_FILT_CCK, 0);
	athn_set_rxfilter(sc, 0);
	error = athn_stop_rx_dma(sc);
	if (error != 0)
		goto reset;

	/* AR9280 always needs a full reset. */
	if (AR_SREV_9280(sc))
		goto reset;

	/* If band or bandwidth changes, we need to do a full reset. */
	if (c->ic_flags != sc->curchan->ic_flags ||
	    ((extc != NULL) ^ (sc->curchanext != NULL))) {
		DPRINTFN(2, ("channel band switch\n"));
		goto reset;
	}
	error = athn_set_power_awake(sc);
	if (error != 0)
		goto reset;

	error = athn_set_chan(sc, c, extc);
	if (error != 0) {
 reset:		/* Error found, try a full reset. */
		DPRINTFN(3, ("needs a full reset\n"));
		error = athn_hw_reset(sc, c, extc);
		if (error != 0)	/* Hopeless case. */
			return (error);
	}
	athn_rx_start(sc);

	/* Re-enable interrupts. */
	athn_enable_interrupts(sc);
	return (0);
}

void
athn_get_delta_slope(uint32_t coeff, uint32_t *exponent, uint32_t *mantissa)
{
#define COEFF_SCALE_SHIFT	24
	uint32_t exp, man;

	/* exponent = 14 - floor(log2(coeff)) */
	for (exp = 31; exp > 0; exp--)
		if (coeff & (1 << exp))
			break;
	exp = 14 - (exp - COEFF_SCALE_SHIFT);

	/* mantissa = floor(coeff * 2^exponent + 0.5) */
	man = coeff + (1 << (COEFF_SCALE_SHIFT - exp - 1));

	*mantissa = man >> (COEFF_SCALE_SHIFT - exp);
	*exponent = exp - 16;
#undef COEFF_SCALE_SHIFT
}

void
athn_set_delta_slope(struct athn_softc *sc, struct ieee80211_channel *c,
    struct ieee80211_channel *extc)
{
	uint32_t coeff, exp, man, reg;

	/* Set Delta Slope (exponent and mantissa). */
	coeff = (100 << 24) / c->ic_freq;
	athn_get_delta_slope(coeff, &exp, &man);
	DPRINTFN(5, ("delta slope coeff exp=%u man=%u\n", exp, man));

	reg = AR_READ(sc, AR_PHY_TIMING3);
	reg = RW(reg, AR_PHY_TIMING3_DSC_EXP, exp);
	reg = RW(reg, AR_PHY_TIMING3_DSC_MAN, man);
	AR_WRITE(sc, AR_PHY_TIMING3, reg);

	/* For Short GI, coeff is 9/10 that of normal coeff. */
	coeff = (9 * coeff) / 10;
	athn_get_delta_slope(coeff, &exp, &man);
	DPRINTFN(5, ("delta slope coeff exp=%u man=%u\n", exp, man));

	reg = AR_READ(sc, AR_PHY_HALFGI);
	reg = RW(reg, AR_PHY_HALFGI_DSC_EXP, exp);
	reg = RW(reg, AR_PHY_HALFGI_DSC_MAN, man);
	AR_WRITE(sc, AR_PHY_HALFGI, reg);
}

void
athn_set_phy(struct athn_softc *sc, struct ieee80211_channel *c,
    struct ieee80211_channel *extc)
{
	uint32_t phy;

	if (AR_SREV_9285_10_OR_LATER(sc))
		phy = AR_READ(sc, AR_PHY_TURBO) & AR_PHY_FC_ENABLE_DAC_FIFO;
	else
		phy = 0;
	phy |= AR_PHY_FC_HT_EN | AR_PHY_FC_SHORT_GI_40 |
	    AR_PHY_FC_SINGLE_HT_LTF1 | AR_PHY_FC_WALSH;
	if (extc != NULL) {
		phy |= AR_PHY_FC_DYN2040_EN;
		if (extc > c)	/* XXX */
			phy |= AR_PHY_FC_DYN2040_PRI_CH;
	}
	AR_WRITE(sc, AR_PHY_TURBO, phy);

	AR_WRITE(sc, AR_2040_MODE,
	    (extc != NULL) ? AR_2040_JOINED_RX_CLEAR : 0);

	AR_WRITE(sc, AR_GTXTO, SM(AR_GTXTO_TIMEOUT_LIMIT, 25));
	AR_WRITE(sc, AR_CST, SM(AR_CST_TIMEOUT_LIMIT, 15));
}

int
athn_read_rom_word(struct athn_softc *sc, uint32_t addr, uint16_t *val)
{
	uint32_t reg;
	int ntries;

	/* Read 16-bit value from ROM. */
	reg = AR_READ(sc, AR_EEPROM_OFFSET(addr));
	for (ntries = 0; ntries < 1000; ntries++) {
		reg = AR_READ(sc, AR_EEPROM_STATUS_DATA);
		if (!(reg & (AR_EEPROM_STATUS_DATA_BUSY |
		    AR_EEPROM_STATUS_DATA_PROT_ACCESS))) {
			*val = MS(reg, AR_EEPROM_STATUS_DATA_VAL);
			return (0);
		}
		DELAY(10);
	}
	*val = 0xffff;
	return (ETIMEDOUT);
}

int
athn_read_rom(struct athn_softc *sc)
{
	uint32_t addr, end;
	uint16_t magic, sum, *eep;
	int need_swap = 0;
	int error;

	/* Determine ROM endianness. */
	error = athn_read_rom_word(sc, AR_EEPROM_MAGIC_OFFSET, &magic);
	if (error != 0)
		return (error);
	if (magic != AR_EEPROM_MAGIC) {
		if (magic != swap16(AR_EEPROM_MAGIC)) {
			DPRINTF(("invalid ROM magic 0x%x != 0x%x\n",
			    magic, AR_EEPROM_MAGIC));
			return (EIO);
		}
		DPRINTF(("non-native ROM endianness\n"));
		need_swap = 1;
	}

	/* Allocate space to store ROM in host memory. */
	sc->eep = malloc(sc->eep_size, M_DEVBUF, M_NOWAIT);
	if (sc->eep == NULL)
		return (ENOMEM);

	/* Read entire ROM and compute checksum. */
	sum = 0;
	eep = sc->eep;
	end = sc->eep_base + sc->eep_size / sizeof (uint16_t);
	for (addr = sc->eep_base; addr < end; addr++, eep++) {
		if ((error = athn_read_rom_word(sc, addr, eep)) != 0) {
			DPRINTF(("could not read ROM at 0x%x\n", addr));
			return (error);
		}
		if (need_swap)
			*eep = swap16(*eep);
		sum ^= *eep;
	}
	if (sum != 0xffff) {
		printf("%s: bad ROM checksum 0x%04x\n",
		    sc->sc_dev.dv_xname, sum);
		return (EIO);
	}
	if (need_swap)
		athn_swap_rom(sc);

	return (0);
}

void
athn_swap_rom(struct athn_softc *sc)
{
	struct ar_base_eep_header *base = sc->eep;

	/* Swap common fields first. */
	base->length = swap16(base->length);
	base->version = swap16(base->version);
	base->regDmn[0] = swap16(base->regDmn[0]);
	base->regDmn[1] = swap16(base->regDmn[1]);
	base->rfSilent = swap16(base->rfSilent);
	base->blueToothOptions = swap16(base->blueToothOptions);
	base->deviceCap = swap16(base->deviceCap);

	/* Swap device-dependent fields. */
	sc->ops.swap_rom(sc);
}

void
athn_reset_key(struct athn_softc *sc, int entry)
{
	/*
	 * NB: Key cache registers access special memory area that requires
	 * two 32-bit writes to actually update the values in the internal
	 * memory.  Consequently, writes must be grouped by pair.
	 */
	AR_WRITE(sc, AR_KEYTABLE_KEY0(entry), 0);
	AR_WRITE(sc, AR_KEYTABLE_KEY1(entry), 0);

	AR_WRITE(sc, AR_KEYTABLE_KEY2(entry), 0);
	AR_WRITE(sc, AR_KEYTABLE_KEY3(entry), 0);

	AR_WRITE(sc, AR_KEYTABLE_KEY4(entry), 0);
	AR_WRITE(sc, AR_KEYTABLE_TYPE(entry), AR_KEYTABLE_TYPE_CLR);

	AR_WRITE(sc, AR_KEYTABLE_MAC0(entry), 0);
	AR_WRITE(sc, AR_KEYTABLE_MAC1(entry), 0);
}

int
athn_set_key(struct ieee80211com *ic, struct ieee80211_node *ni,
    struct ieee80211_key *k)
{
	struct athn_softc *sc = ic->ic_softc;
	uint32_t type, lo, hi;
	uint16_t keybuf[8], micbuf[8];
	const uint8_t *addr;
	uintptr_t entry, micentry;

	switch (k->k_cipher) {
	case IEEE80211_CIPHER_WEP40:
		type = AR_KEYTABLE_TYPE_40;
		break;
	case IEEE80211_CIPHER_WEP104:
		type = AR_KEYTABLE_TYPE_104;
		break;
	case IEEE80211_CIPHER_TKIP:
		type = AR_KEYTABLE_TYPE_TKIP;
		break;
	case IEEE80211_CIPHER_CCMP:
		type = AR_KEYTABLE_TYPE_CCM;
		break;
	default:
		/* Fallback to software crypto for other ciphers. */
		return (ieee80211_set_key(ic, ni, k));
	}

	memset(keybuf, 0, sizeof keybuf);
	memcpy(keybuf, k->k_key, MIN(k->k_len, 16));

	if (!(k->k_flags & IEEE80211_KEY_GROUP))
		entry = IEEE80211_GROUP_NKID + IEEE80211_AID(ni->ni_associd);
	else
		entry = k->k_id;
	k->k_priv = (void *)entry;

	/* NB: See note about key cache registers access above. */
	if (type == AR_KEYTABLE_TYPE_TKIP) {
		micentry = entry + 64;

		/* XXX Split MIC. */
		AR_WRITE(sc, AR_KEYTABLE_KEY0(micentry),
		    micbuf[0] | micbuf[1] << 16);
		AR_WRITE(sc, AR_KEYTABLE_KEY1(micentry), micbuf[2]);

		AR_WRITE(sc, AR_KEYTABLE_KEY2(micentry),
		    micbuf[3] | micbuf[4] << 16);
		AR_WRITE(sc, AR_KEYTABLE_KEY3(micentry), micbuf[5]);

		AR_WRITE(sc, AR_KEYTABLE_KEY4(micentry),
		    micbuf[6] | micbuf[7] << 16);
		AR_WRITE(sc, AR_KEYTABLE_TYPE(micentry), AR_KEYTABLE_TYPE_CLR);

		/* MAC address registers are reserved for the MIC entry. */
		AR_WRITE(sc, AR_KEYTABLE_MAC0(micentry), 0);
		AR_WRITE(sc, AR_KEYTABLE_MAC1(micentry), 0);
	} else {
		AR_WRITE(sc, AR_KEYTABLE_KEY0(entry),
		    keybuf[0] | keybuf[1] << 16);
		AR_WRITE(sc, AR_KEYTABLE_KEY1(entry), keybuf[2]);

		AR_WRITE(sc, AR_KEYTABLE_KEY2(entry),
		    keybuf[3] | keybuf[4] << 16);
		AR_WRITE(sc, AR_KEYTABLE_KEY3(entry), keybuf[5]);

		AR_WRITE(sc, AR_KEYTABLE_KEY4(entry),
		    keybuf[6] | keybuf[7] << 16);
		AR_WRITE(sc, AR_KEYTABLE_TYPE(entry), type);
	}

	/* Clear keys from the stack. */
	memset(keybuf, 0, sizeof keybuf);
	memset(micbuf, 0, sizeof micbuf);

	if (!(k->k_flags & IEEE80211_KEY_GROUP)) {
		addr = ni->ni_macaddr;
		lo = addr[0] | addr[1] << 8 | addr[2] << 16 | addr[3] << 24;
		hi = addr[4] | addr[5] << 8;
		lo = lo >> 1 | hi << 31;
		hi = hi >> 1;
	} else
		lo = hi = 0;
	AR_WRITE(sc, AR_KEYTABLE_MAC0(entry), lo);
	AR_WRITE(sc, AR_KEYTABLE_MAC1(entry), hi | AR_KEYTABLE_VALID);
	return (0);
}

void
athn_delete_key(struct ieee80211com *ic, struct ieee80211_node *ni,
    struct ieee80211_key *k)
{
	struct athn_softc *sc = ic->ic_softc;
	uintptr_t entry;

	switch (k->k_cipher) {
	case IEEE80211_CIPHER_WEP40:
	case IEEE80211_CIPHER_WEP104:
	case IEEE80211_CIPHER_TKIP:
	case IEEE80211_CIPHER_CCMP:
		entry = (uintptr_t)k->k_priv;
		athn_reset_key(sc, entry);
		break;
	default:
		/* Fallback to software crypto for other ciphers. */
		ieee80211_delete_key(ic, ni, k);
	}
}

void
athn_led_init(struct athn_softc *sc)
{
	athn_gpio_config_output(sc, sc->led_pin, AR_GPIO_OUTPUT_MUX_AS_OUTPUT);
	/* LED off, active low. */
	athn_gpio_write(sc, sc->led_pin, 1);
}

#ifdef ATHN_BT_COEXISTENCE
void
athn_btcoex_init(struct athn_softc *sc)
{
	uint32_t reg;

	if (sc->flags & ATHN_FLAG_BTCOEX2WIRE) {
		/* Connect bt_active to baseband. */
		AR_CLRBITS(sc, AR_GPIO_INPUT_EN_VAL,
		    AR_GPIO_INPUT_EN_VAL_BT_PRIORITY_DEF |
		    AR_GPIO_INPUT_EN_VAL_BT_FREQUENCY_DEF);
		AR_SETBITS(sc, AR_GPIO_INPUT_EN_VAL,
		    AR_GPIO_INPUT_EN_VAL_BT_ACTIVE_BB);

		reg = AR_READ(sc, AR_GPIO_INPUT_MUX1);
		reg = RW(reg, AR_GPIO_INPUT_MUX1_BT_ACTIVE,
		    AR_GPIO_BTACTIVE_PIN);
		AR_WRITE(sc, AR_GPIO_INPUT_MUX1, reg);

		athn_gpio_config_input(sc, AR_GPIO_BTACTIVE_PIN);
	} else {	/* 3-wire. */
		AR_SETBITS(sc, AR_GPIO_INPUT_EN_VAL,
		    AR_GPIO_INPUT_EN_VAL_BT_PRIORITY_BB |
		    AR_GPIO_INPUT_EN_VAL_BT_ACTIVE_BB);

		reg = AR_READ(sc, AR_GPIO_INPUT_MUX1);
		reg = RW(reg, AR_GPIO_INPUT_MUX1_BT_ACTIVE,
		    AR_GPIO_BTACTIVE_PIN);
		reg = RW(reg, AR_GPIO_INPUT_MUX1_BT_PRIORITY,
		    AR_GPIO_BTPRIORITY_PIN);
		AR_WRITE(sc, AR_GPIO_INPUT_MUX1, reg);

		athn_gpio_config_input(sc, AR_GPIO_BTACTIVE_PIN);
		athn_gpio_config_input(sc, AR_GPIO_BTPRIORITY_PIN);
	}
}

void
athn_btcoex_enable(struct athn_softc *sc)
{
	uint32_t reg;

	if (sc->flags & ATHN_FLAG_BTCOEX3WIRE) {
		AR_WRITE(sc, AR_BT_COEX_MODE,
		    SM(AR_BT_MODE, AR_BT_MODE_SLOTTED) |
		    SM(AR_BT_PRIORITY_TIME, 2) |
		    SM(AR_BT_FIRST_SLOT_TIME, 5) |
		    SM(AR_BT_QCU_THRESH, ATHN_QID_AC_BE) |
		    AR_BT_TXSTATE_EXTEND | AR_BT_TX_FRAME_EXTEND |
		    AR_BT_QUIET | AR_BT_RX_CLEAR_POLARITY);
		AR_WRITE(sc, AR_BT_COEX_WEIGHT,
		    SM(AR_BTCOEX_BT_WGHT, AR_STOMP_LOW_BT_WGHT) |
		    SM(AR_BTCOEX_WL_WGHT, AR_STOMP_LOW_WL_WGHT));
		AR_WRITE(sc, AR_BT_COEX_MODE2,
		    SM(AR_BT_BCN_MISS_THRESH, 50) |
		    AR_BT_HOLD_RX_CLEAR | AR_BT_DISABLE_BT_ANT);

		AR_SETBITS(sc, AR_QUIET1, AR_QUIET1_QUIET_ACK_CTS_ENABLE);
		AR_CLRBITS(sc, AR_PCU_MISC, AR_PCU_BT_ANT_PREVENT_RX);

		athn_gpio_config_output(sc, AR_GPIO_WLANACTIVE_PIN,
		    AR_GPIO_OUTPUT_MUX_AS_RX_CLEAR_EXTERNAL);

	} else {	/* 2-wire. */
		athn_gpio_config_output(sc, AR_GPIO_WLANACTIVE_PIN,
		    AR_GPIO_OUTPUT_MUX_AS_TX_FRAME);
	}
	reg = AR_READ(sc, AR_GPIO_PDPU);
	reg &= ~(0x3 << (AR_GPIO_WLANACTIVE_PIN * 2));
	reg |= 0x2 << (AR_GPIO_WLANACTIVE_PIN * 2);
	AR_WRITE(sc, AR_GPIO_PDPU, reg);

	/* Disable PCIe Active State Power Management (ASPM). */
	if (sc->sc_disable_aspm != NULL)
		sc->sc_disable_aspm(sc);

	/* XXX Start periodic timer. */
}

void
athn_btcoex_disable(struct athn_softc *sc)
{
	athn_gpio_write(sc, AR_GPIO_WLANACTIVE_PIN, 0);

	athn_gpio_config_output(sc, AR_GPIO_WLANACTIVE_PIN,
	    AR_GPIO_OUTPUT_MUX_AS_OUTPUT);

	if (sc->flags & ATHN_FLAG_BTCOEX3WIRE) {
		AR_WRITE(sc, AR_BT_COEX_MODE,
		    SM(AR_BT_MODE, AR_BT_MODE_DISABLED) | AR_BT_QUIET);
		AR_WRITE(sc, AR_BT_COEX_WEIGHT, 0);
		AR_WRITE(sc, AR_BT_COEX_MODE2, 0);
		/* XXX Stop periodic timer. */
	}
	/* XXX Restore ASPM setting? */
}
#endif

void
athn_rfsilent_init(struct athn_softc *sc)
{
	uint32_t reg;

	/* Configure hardware radio switch. */
	AR_SETBITS(sc, AR_GPIO_INPUT_EN_VAL, AR_GPIO_INPUT_EN_VAL_RFSILENT_BB);
	reg = AR_READ(sc, AR_GPIO_INPUT_MUX2);
	reg = RW(reg, AR_GPIO_INPUT_MUX2_RFSILENT, 0);
	AR_WRITE(sc, AR_GPIO_INPUT_MUX2, reg);
	athn_gpio_config_input(sc, sc->rfsilent_pin);
	AR_SETBITS(sc, AR_PHY_TEST, AR_PHY_TEST_RFSILENT_BB);
	if (!(sc->flags & ATHN_FLAG_RFSILENT_REVERSED)) {
		AR_SETBITS(sc, AR_GPIO_INTR_POL,
		    AR_GPIO_INTR_POL_PIN(sc->rfsilent_pin));
	}
}

void
athn_iter_func(void *arg, struct ieee80211_node *ni)
{
	struct athn_softc *sc = arg;
	struct athn_node *an = (struct athn_node *)ni;

	ieee80211_amrr_choose(&sc->amrr, ni, &an->amn);
}

void
athn_calib_to(void *arg)
{
	struct athn_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	int s;

	s = splnet();
#ifdef notyet
	/* XXX ANI. */
	/* XXX OLPC temperature compensation. */

	if (AR_READ(sc, AR_PHY_TIMING_CTRL4_0) & AR_PHY_TIMING_CTRL4_DO_CAL) {
		/* Calibration in progress, come back later. */
		timeout_add_msec(&sc->calib_to, 500);
		splx(s);
		return;
	}
	if (sc->calib_mask & ATHN_CAL_ADC_GAIN)
		athn_calib_iq(sc);
	else if (sc->calib_mask & ATHN_CAL_ADC_DC)
		athn_calib_adc_gain(sc);
	else if (sc->calib_mask & ATHN_CAL_IQ)
		athn_calib_adc_dc_off(sc);
#endif
	if (ic->ic_fixed_rate == -1) {
		if (ic->ic_opmode == IEEE80211_M_STA)
			athn_iter_func(sc, ic->ic_bss);
		else
			ieee80211_iterate_nodes(ic, athn_iter_func, sc);
	}
	timeout_add_msec(&sc->calib_to, 500);
	splx(s);
}

void
athn_do_calib(struct athn_softc *sc)
{
	int log = AR_MAX_LOG_CAL;	/* XXX */
	uint32_t mode = 0, reg;

	reg = AR_READ(sc, AR_PHY_TIMING_CTRL4_0);
	reg = RW(reg, AR_PHY_TIMING_CTRL4_IQCAL_LOG_COUNT_MAX, log);
	AR_WRITE(sc, AR_PHY_TIMING_CTRL4_0, reg);

	if (sc->calib_mask & ATHN_CAL_ADC_GAIN)
		mode = AR_PHY_CALMODE_ADC_GAIN;
	else if (sc->calib_mask & ATHN_CAL_ADC_DC)
		mode = AR_PHY_CALMODE_ADC_DC_PER;
	else if (sc->calib_mask & ATHN_CAL_IQ)
		mode = AR_PHY_CALMODE_IQ;
	AR_WRITE(sc, AR_PHY_CALMODE, mode);

	AR_SETBITS(sc, AR_PHY_TIMING_CTRL4_0, AR_PHY_TIMING_CTRL4_DO_CAL);
}

void
athn_calib_iq(struct athn_softc *sc)
{
	struct athn_iq_cal *cal;
	uint32_t reg, i_coff_denom, q_coff_denom;
	int32_t i_coff, q_coff;
	int i, iq_corr_neg;

	for (i = 0; i < AR_MAX_CHAINS; i++) {
		cal = &sc->calib.iq[i];

		/* Accumulate IQ calibration measures (clear on read). */
		cal->pwr_meas_i += AR_READ(sc, AR_PHY_CAL_MEAS_0(i));
		cal->pwr_meas_q += AR_READ(sc, AR_PHY_CAL_MEAS_1(i));
		cal->iq_corr_meas +=
		    (int32_t)AR_READ(sc, AR_PHY_CAL_MEAS_2(i));
	}
	if (++sc->calib.nsamples < AR_CAL_SAMPLES) {
		/* Not enough samples accumulated, continue. */
		athn_do_calib(sc);
		return;
	}

	for (i = 0; i < sc->nrxchains; i++) {
		cal = &sc->calib.iq[i];

		if (cal->pwr_meas_q == 0)
			continue;

		if ((iq_corr_neg = cal->iq_corr_meas < 0))
			cal->iq_corr_meas = -cal->iq_corr_meas;

		i_coff_denom =
		    (cal->pwr_meas_i / 2 + cal->pwr_meas_q / 2) / 128;
		q_coff_denom = cal->pwr_meas_q / 64;

		if (i_coff_denom == 0 || q_coff_denom == 0)
			continue;	/* Prevents division by zero. */

		i_coff = cal->iq_corr_meas / i_coff_denom;
		q_coff = (cal->pwr_meas_i / q_coff_denom) - 64;

		/* Negate i_coff if iq_corr_meas is positive. */
		if (!iq_corr_neg)
			i_coff = 0x40 - (i_coff & 0x3f);
		if (q_coff > 15)
			q_coff = 15;
		else if (q_coff <= -16)
			q_coff = 16;

		DPRINTFN(2, ("IQ calibration for chain %d\n", i));
		reg = AR_READ(sc, AR_PHY_TIMING_CTRL4(i));
		reg = RW(reg, AR_PHY_TIMING_CTRL4_IQCORR_Q_I_COFF, i_coff);
		reg = RW(reg, AR_PHY_TIMING_CTRL4_IQCORR_Q_Q_COFF, q_coff);
		AR_WRITE(sc, AR_PHY_TIMING_CTRL4(i), reg);
	}

	AR_SETBITS(sc, AR_PHY_TIMING_CTRL4_0,
	    AR_PHY_TIMING_CTRL4_IQCORR_ENABLE);
}

void
athn_calib_adc_gain(struct athn_softc *sc)
{
	struct athn_adc_cal *cal;
	uint32_t reg, gain_mismatch_i, gain_mismatch_q;
	int i;

	for (i = 0; i < AR_MAX_CHAINS; i++) {
		cal = &sc->calib.adc_gain[i];

		/* Accumulate ADC gain measures (clear on read). */
		cal->pwr_meas_odd_i  += AR_READ(sc, AR_PHY_CAL_MEAS_0(i));
		cal->pwr_meas_even_i += AR_READ(sc, AR_PHY_CAL_MEAS_1(i));
		cal->pwr_meas_odd_q  += AR_READ(sc, AR_PHY_CAL_MEAS_2(i));
		cal->pwr_meas_even_q += AR_READ(sc, AR_PHY_CAL_MEAS_3(i));
	}
	if (++sc->calib.nsamples < AR_CAL_SAMPLES) {
		/* Not enough samples accumulated, continue. */
		athn_do_calib(sc);
		return;
	}

	for (i = 0; i < sc->nrxchains; i++) {
		cal = &sc->calib.adc_gain[i];

		if (cal->pwr_meas_odd_i == 0 || cal->pwr_meas_even_q == 0)
			continue;	/* Prevents division by zero. */

		gain_mismatch_i =
		    (cal->pwr_meas_even_i * 32) / cal->pwr_meas_odd_i;
		gain_mismatch_q =
		    (cal->pwr_meas_odd_q * 32) / cal->pwr_meas_even_q;

		DPRINTFN(2, ("ADC gain calibration for chain %d\n", i));
		reg = AR_READ(sc, AR_PHY_NEW_ADC_DC_GAIN_CORR(i));
		reg = RW(reg, AR_PHY_NEW_ADC_DC_GAIN_IGAIN, gain_mismatch_i);
		reg = RW(reg, AR_PHY_NEW_ADC_DC_GAIN_QGAIN, gain_mismatch_q);
		AR_WRITE(sc, AR_PHY_NEW_ADC_DC_GAIN_CORR(i), reg);
	}

	AR_SETBITS(sc, AR_PHY_NEW_ADC_DC_GAIN_CORR(0),
	    AR_PHY_NEW_ADC_GAIN_CORR_ENABLE);
}

void
athn_calib_adc_dc_off(struct athn_softc *sc)
{
	struct athn_adc_cal *cal;
	int32_t dc_offset_mismatch_i, dc_offset_mismatch_q;
	uint32_t reg;
	int count, i;

	for (i = 0; i < AR_MAX_CHAINS; i++) {
		cal = &sc->calib.adc_dc_offset[i];

		/* Accumulate ADC DC offset measures (clear on read). */
		cal->pwr_meas_odd_i  += AR_READ(sc, AR_PHY_CAL_MEAS_0(i));
		cal->pwr_meas_even_i += AR_READ(sc, AR_PHY_CAL_MEAS_1(i));
		cal->pwr_meas_odd_q  += AR_READ(sc, AR_PHY_CAL_MEAS_2(i));
		cal->pwr_meas_even_q += AR_READ(sc, AR_PHY_CAL_MEAS_3(i));
	}
	if (++sc->calib.nsamples < AR_CAL_SAMPLES) {
		/* Not enough samples accumulated, continue. */
		athn_do_calib(sc);
		return;
	}

	count = (1 << (AR_MAX_LOG_CAL + 5)) * sc->calib.nsamples;

	for (i = 0; i < sc->nrxchains; i++) {
		cal = &sc->calib.adc_dc_offset[i];

		dc_offset_mismatch_i =
		    (cal->pwr_meas_even_i - cal->pwr_meas_odd_i * 2) / count;
		dc_offset_mismatch_q =
		    (cal->pwr_meas_odd_q - cal->pwr_meas_even_q * 2) / count;

		DPRINTFN(2, ("ADC DC offset calibration for chain %d\n", i));
		reg = AR_READ(sc, AR_PHY_NEW_ADC_DC_GAIN_CORR(i));
		reg = RW(reg, AR_PHY_NEW_ADC_DC_GAIN_QDC,
		    dc_offset_mismatch_q);
		reg = RW(reg, AR_PHY_NEW_ADC_DC_GAIN_IDC,
		    dc_offset_mismatch_i);
		AR_WRITE(sc, AR_PHY_NEW_ADC_DC_GAIN_CORR(i), reg);
	}

	AR_SETBITS(sc, AR_PHY_NEW_ADC_DC_GAIN_CORR(0),
	    AR_PHY_NEW_ADC_DC_OFFSET_CORR_ENABLE);
}

void
athn_read_noisefloor(struct athn_softc *sc, int16_t *nf, int16_t *nf_ext)
{
/* Sign-extend 9-bit value to 16-bit. */
#define SIGN_EXT(v)	((((int16_t)(v)) << 7) >> 7)
	uint32_t reg;
	int i;

	for (i = 0; i < sc->nrxchains; i++) {
		reg = AR_READ(sc, AR_PHY_CCA(i));
		if (AR_SREV_9280_10_OR_LATER(sc))
			nf[i] = MS(reg, AR9280_PHY_MINCCA_PWR);
		else
			nf[i] = MS(reg, AR_PHY_MINCCA_PWR);
		nf[i] = SIGN_EXT(nf[i]);

		reg = AR_READ(sc, AR_PHY_EXT_CCA(i));
		if (AR_SREV_9280_10_OR_LATER(sc))
			nf_ext[i] = MS(reg, AR9280_PHY_EXT_MINCCA_PWR);
		else
			nf_ext[i] = MS(reg, AR_PHY_EXT_MINCCA_PWR);
		nf_ext[i] = SIGN_EXT(nf_ext[i]);
	}
#undef SIGN_EXT
}

void
athn_write_noisefloor(struct athn_softc *sc, int16_t *nf, int16_t *nf_ext)
{
	uint32_t reg;
	int i;

	for (i = 0; i < sc->nrxchains; i++) {
		reg = AR_READ(sc, AR_PHY_CCA(i));
		reg = RW(reg, AR_PHY_MAXCCA_PWR, nf[i]);
		AR_WRITE(sc, AR_PHY_CCA(i), reg);

		reg = AR_READ(sc, AR_PHY_EXT_CCA(i));
		reg = RW(reg, AR_PHY_EXT_MAXCCA_PWR, nf_ext[i]);
		AR_WRITE(sc, AR_PHY_EXT_CCA(i), reg);
	}
}

void
athn_get_noisefloor(struct athn_softc *sc, struct ieee80211_channel *c)
{
	int16_t nf[AR_MAX_CHAINS], nf_ext[AR_MAX_CHAINS];
	int i;

	if (AR_READ(sc, AR_PHY_AGC_CONTROL) & AR_PHY_AGC_CONTROL_NF) {
		/* Noisefloor calibration not finished. */
		return;
	}
	/* Noisefloor calibration is finished. */
	athn_read_noisefloor(sc, nf, nf_ext);

	/* Update noisefloor history. */
	for (i = 0; i < sc->nrxchains; i++) {
		sc->nf_hist[sc->nf_hist_cur].nf[i] = nf[i];
		sc->nf_hist[sc->nf_hist_cur].nf_ext[i] = nf_ext[i];
	}
	if (++sc->nf_hist_cur >= ATHN_NF_CAL_HIST_MAX)
		sc->nf_hist_cur = 0;
}

void
athn_bb_load_noisefloor(struct athn_softc *sc)
{
	int16_t nf[AR_MAX_CHAINS], nf_ext[AR_MAX_CHAINS];
	int i, ntries;

	/* Write filtered noisefloor values. */
	for (i = 0; i < sc->nrxchains; i++) {
		nf[i] = sc->nf_priv[i] * 2;
		nf_ext[i] = sc->nf_ext_priv[i] * 2;
	}
	athn_write_noisefloor(sc, nf, nf_ext);

	/* Load filtered noisefloor values into baseband. */
	AR_CLRBITS(sc, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_ENABLE_NF);
	AR_CLRBITS(sc, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_NO_UPDATE_NF);
	AR_SETBITS(sc, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_NF);
	/* Wait for load to complete. */
	for (ntries = 0; ntries < 5; ntries++) {
		if (!(AR_READ(sc, AR_PHY_AGC_CONTROL) & AR_PHY_AGC_CONTROL_NF))
			break;
		DELAY(50);
	}
#ifdef ATHN_DEBUG
	if (ntries == 5 && athn_debug > 0)
		printf("failed to load noisefloor values\n");
#endif

	/* Restore noisefloor values to initial (max) values. */
	for (i = 0; i < AR_MAX_CHAINS; i++)
		nf[i] = nf_ext[i] = -50 * 2;
	athn_write_noisefloor(sc, nf, nf_ext);
}

void
athn_noisefloor_calib(struct athn_softc *sc)
{
	AR_SETBITS(sc, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_ENABLE_NF);
	AR_SETBITS(sc, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_NO_UPDATE_NF);
	AR_SETBITS(sc, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_NF);
}

int
athn_init_calib(struct athn_softc *sc, struct ieee80211_channel *c,
    struct ieee80211_channel *extc)
{
	int error;

	if (AR_SREV_9285_12_OR_LATER(sc))
		error = ar9285_1_2_init_calib(sc, c, extc);
	else
		error = ar5416_init_calib(sc, c, extc);
	if (error != 0)
		return (error);

	/* Do PA calibration. */
	if (AR_SREV_9285_11_OR_LATER(sc))
		ar9285_pa_calib(sc);

	/* Do noisefloor calibration. */
	AR_SETBITS(sc, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_NF);

	if (AR_SREV_9160_10_OR_LATER(sc)) {
		/* Enable IQ calibration. */
		sc->calib_mask = ATHN_CAL_IQ;

		/* Enable ADC gain and ADC DC offset calibrations. */
		if (IEEE80211_IS_CHAN_5GHZ(c) || extc != NULL)
			sc->calib_mask |= ATHN_CAL_ADC_GAIN | ATHN_CAL_ADC_DC;

		athn_do_calib(sc);
	}
	return (0);
}

/*
 * Anti-noise immunity.
 */
int32_t
athn_ani_get_rssi(struct athn_softc *sc)
{
	return (0);	/* XXX */
}

void
athn_ani_set_noise_immunity_level(struct athn_softc *sc, int level)
{
	int high = level == 4;
	uint32_t reg;

	reg = AR_READ(sc, AR_PHY_DESIRED_SZ);
	reg = RW(reg, AR_PHY_DESIRED_SZ_TOT_DES, high ? -62 : -55);
	AR_WRITE(sc, AR_PHY_DESIRED_SZ, reg);

	reg = AR_READ(sc, AR_PHY_AGC_CTL1);
	reg = RW(reg, AR_PHY_AGC_CTL1_COARSE_LOW, high ? -70 : -64);
	reg = RW(reg, AR_PHY_AGC_CTL1_COARSE_HIGH, high ? -12 : -14);
	AR_WRITE(sc, AR_PHY_AGC_CTL1, reg);

	reg = AR_READ(sc, AR_PHY_FIND_SIG);
	reg = RW(reg, AR_PHY_FIND_SIG_FIRPWR, high ? -80 : -78);
	AR_WRITE(sc, AR_PHY_FIND_SIG, reg);

	sc->ani.noise_immunity_level = level;
}

void
athn_ani_ena_ofdm_weak_signal(struct athn_softc *sc)
{
	uint32_t reg;

	reg = AR_READ(sc, AR_PHY_SFCORR_LOW);
	reg = RW(reg, AR_PHY_SFCORR_LOW_M1_THRESH_LOW, 50);
	reg = RW(reg, AR_PHY_SFCORR_LOW_M2_THRESH_LOW, 40);
	reg = RW(reg, AR_PHY_SFCORR_LOW_M2COUNT_THR_LOW, 48);
	AR_WRITE(sc, AR_PHY_SFCORR_LOW, reg);

	reg = AR_READ(sc, AR_PHY_SFCORR);
	reg = RW(reg, AR_PHY_SFCORR_M1_THRESH, 77);
	reg = RW(reg, AR_PHY_SFCORR_M2_THRESH, 64);
	reg = RW(reg, AR_PHY_SFCORR_M2COUNT_THR, 16);
	AR_WRITE(sc, AR_PHY_SFCORR, reg);

	reg = AR_READ(sc, AR_PHY_SFCORR_EXT);
	reg = RW(reg, AR_PHY_SFCORR_EXT_M1_THRESH_LOW, 50);
	reg = RW(reg, AR_PHY_SFCORR_EXT_M2_THRESH_LOW, 40);
	reg = RW(reg, AR_PHY_SFCORR_EXT_M1_THRESH, 77);
	reg = RW(reg, AR_PHY_SFCORR_EXT_M2_THRESH, 64);
	AR_WRITE(sc, AR_PHY_SFCORR_EXT, reg);

	AR_SETBITS(sc, AR_PHY_SFCORR_LOW,
	    AR_PHY_SFCORR_LOW_USE_SELF_CORR_LOW);

	sc->ani.ofdm_weak_signal = 1;
}

void
athn_ani_dis_ofdm_weak_signal(struct athn_softc *sc)
{
	uint32_t reg;

	reg = AR_READ(sc, AR_PHY_SFCORR_LOW);
	reg = RW(reg, AR_PHY_SFCORR_LOW_M1_THRESH_LOW, 127);
	reg = RW(reg, AR_PHY_SFCORR_LOW_M2_THRESH_LOW, 127);
	reg = RW(reg, AR_PHY_SFCORR_LOW_M2COUNT_THR_LOW, 63);
	AR_WRITE(sc, AR_PHY_SFCORR_LOW, reg);

	reg = AR_READ(sc, AR_PHY_SFCORR);
	reg = RW(reg, AR_PHY_SFCORR_M1_THRESH, 127);
	reg = RW(reg, AR_PHY_SFCORR_M2_THRESH, 127);
	reg = RW(reg, AR_PHY_SFCORR_M2COUNT_THR, 31);
	AR_WRITE(sc, AR_PHY_SFCORR, reg);

	reg = AR_READ(sc, AR_PHY_SFCORR_EXT);
	reg = RW(reg, AR_PHY_SFCORR_EXT_M1_THRESH_LOW, 127);
	reg = RW(reg, AR_PHY_SFCORR_EXT_M2_THRESH_LOW, 127);
	reg = RW(reg, AR_PHY_SFCORR_EXT_M1_THRESH, 127);
	reg = RW(reg, AR_PHY_SFCORR_EXT_M2_THRESH, 127);
	AR_WRITE(sc, AR_PHY_SFCORR_EXT, reg);

	AR_CLRBITS(sc, AR_PHY_SFCORR_LOW,
	    AR_PHY_SFCORR_LOW_USE_SELF_CORR_LOW);

	sc->ani.ofdm_weak_signal = 0;
}

void
athn_ani_set_cck_weak_signal(struct athn_softc *sc, int high)
{
	uint32_t reg;

	reg = AR_READ(sc, AR_PHY_CCK_DETECT);
	reg = RW(reg, AR_PHY_CCK_DETECT_WEAK_SIG_THR_CCK, high ? 6 : 8);
	AR_WRITE(sc, AR_PHY_CCK_DETECT, reg);

	sc->ani.cck_weak_signal = high;
}

void
athn_ani_set_firstep_level(struct athn_softc *sc, int level)
{
	uint32_t reg;

	reg = AR_READ(sc, AR_PHY_FIND_SIG);
	reg = RW(reg, AR_PHY_FIND_SIG_FIRSTEP, level * 4);
	AR_WRITE(sc, AR_PHY_FIND_SIG, reg);

	sc->ani.firstep_level = level;
}

void
athn_ani_set_spur_immunity_level(struct athn_softc *sc, int level)
{
	uint32_t reg;

	reg = AR_READ(sc, AR_PHY_TIMING5);
	reg = RW(reg, AR_PHY_TIMING5_CYCPWR_THR1, (level + 1) * 2);
	AR_WRITE(sc, AR_PHY_TIMING5, reg);

	sc->ani.spur_immunity_level = level;
}

void
athn_ani_ofdm_err_trigger(struct athn_softc *sc)
{
	struct athn_ani *ani = &sc->ani;
	int32_t rssi;

	/* First, raise noise immunity level, up to max. */
	if (ani->noise_immunity_level < 4) {
		athn_ani_set_noise_immunity_level(sc,
		    ani->noise_immunity_level + 1);
		return;
	}

	/* Then, raise our spur immunity level, up to max. */
	if (ani->spur_immunity_level < 7) {
		athn_ani_set_spur_immunity_level(sc,
		    ani->spur_immunity_level + 1);
		return;
	}

#ifndef IEEE80211_STA_ONLY
	if (sc->sc_ic.ic_opmode == IEEE80211_M_HOSTAP) {
		if (ani->firstep_level < 2)
			athn_ani_set_firstep_level(sc,
			    ani->firstep_level + 1);
		return;
	}
#endif
	rssi = athn_ani_get_rssi(sc);
	if (rssi > ATHN_ANI_RSSI_THR_HIGH) {
		/*
		 * Beacon RSSI is high, turn off OFDM weak signal detection
		 * or raise first step level as last resort.
		 */
		if (ani->ofdm_weak_signal) {
			athn_ani_dis_ofdm_weak_signal(sc);
			athn_ani_set_spur_immunity_level(sc, 0);
		} else if (ani->firstep_level < 2) {
			athn_ani_set_firstep_level(sc,
			    ani->firstep_level + 1);
		}
	} else if (rssi > ATHN_ANI_RSSI_THR_LOW) {
		/*
		 * Beacon RSSI is in mid range, we need OFDM weak signal
		 * detection but we can raise first step level.
		 */
		if (!ani->ofdm_weak_signal)
			athn_ani_ena_ofdm_weak_signal(sc);
		if (ani->firstep_level < 2) {
			athn_ani_set_firstep_level(sc,
			    ani->firstep_level + 1);
		}
	} else if (sc->sc_ic.ic_curmode != IEEE80211_MODE_11A) {
		/*
		 * Beacon RSSI is low, if in b/g mode, turn off OFDM weak
		 * signal detection and zero first step level to maximize
		 * CCK sensitivity.
		 */
		if (ani->ofdm_weak_signal)
			athn_ani_dis_ofdm_weak_signal(sc);
		if (ani->firstep_level > 0)
			athn_ani_set_firstep_level(sc, 0);
	}
}

void
athn_ani_cck_err_trigger(struct athn_softc *sc)
{
	struct athn_ani *ani = &sc->ani;
	int32_t rssi;

	/* Raise noise immunity level, up to max. */
	if (ani->noise_immunity_level < 4) {
		athn_ani_set_noise_immunity_level(sc,
		    ani->noise_immunity_level + 1);
		return;
	}

#ifndef IEEE80211_STA_ONLY
	if (sc->sc_ic.ic_opmode == IEEE80211_M_HOSTAP) {
		if (ani->firstep_level < 2)
			athn_ani_set_firstep_level(sc,
			    ani->firstep_level + 1);
		return;
	}
#endif
	rssi = athn_ani_get_rssi(sc);
	if (rssi > ATHN_ANI_RSSI_THR_LOW) {
		/*
		 * Beacon RSSI is in mid or high range, raise first step
		 * level.
		 */
		if (ani->firstep_level < 2)
			athn_ani_set_firstep_level(sc,
			    ani->firstep_level + 1);
	} else if (sc->sc_ic.ic_curmode != IEEE80211_MODE_11A) {
		/*
		 * Beacon RSSI is low, zero first step level to maximize
		 * CCK sensitivity.
		 */
		if (ani->firstep_level > 0)
			athn_ani_set_firstep_level(sc, 0);
	}
}

void
athn_ani_lower_immunity(struct athn_softc *sc)
{
	struct athn_ani *ani = &sc->ani;
	int32_t rssi;

#ifndef IEEE80211_STA_ONLY
	if (sc->sc_ic.ic_opmode == IEEE80211_M_HOSTAP) {
		if (ani->firstep_level > 0)
			athn_ani_set_firstep_level(sc,
			    ani->firstep_level - 1);
		return;
	}
#endif
	rssi = athn_ani_get_rssi(sc);
	if (rssi > ATHN_ANI_RSSI_THR_HIGH) {
		/*
		 * Beacon RSSI is high, leave OFDM weak signal detection
		 * off or it may oscillate.
		 */
	} else if (rssi > ATHN_ANI_RSSI_THR_LOW) {
		/*
		 * Beacon RSSI is in mid range, turn on OFDM weak signal
		 * detection or lower first step level.
		 */
		if (!ani->ofdm_weak_signal) {
			athn_ani_ena_ofdm_weak_signal(sc);
			return;
		}
		if (ani->firstep_level > 0) {
			athn_ani_set_firstep_level(sc,
			    ani->firstep_level - 1);
			return;
		}
	} else {
		/* Beacon RSSI is low, lower first step level. */
		if (ani->firstep_level > 0) {
			athn_ani_set_firstep_level(sc,
			    ani->firstep_level - 1);
			return;
		}
	}
	/*
	 * Lower spur immunity level down to zero, or if all else fails,
	 * lower noise immunity level down to zero.
	 */
	if (ani->spur_immunity_level > 0)
		athn_ani_set_spur_immunity_level(sc,
		    ani->spur_immunity_level - 1);
	else if (ani->noise_immunity_level > 0)
		athn_ani_set_noise_immunity_level(sc,
		    ani->noise_immunity_level - 1);
}

void
athn_ani_restart(struct athn_softc *sc)
{
	struct athn_ani *ani = &sc->ani;

	AR_WRITE(sc, AR_PHY_ERR_1, 0);
	AR_WRITE(sc, AR_PHY_ERR_2, 0);
	AR_WRITE(sc, AR_PHY_ERR_MASK_1, AR_PHY_ERR_OFDM_TIMING);
	AR_WRITE(sc, AR_PHY_ERR_MASK_2, AR_PHY_ERR_CCK_TIMING);

	ani->listen_time = 0;
	ani->ofdm_phy_err_count = 0;
	ani->cck_phy_err_count = 0;
}

void
athn_ani_monitor(struct athn_softc *sc)
{
	struct athn_ani *ani = &sc->ani;
	uint32_t cyccnt, txfcnt, rxfcnt, phy1, phy2;
	int32_t cycdelta, txfdelta, rxfdelta;
	int32_t listen_time;

	txfcnt = AR_READ(sc, AR_TFCNT);	/* Tx frame count. */
	rxfcnt = AR_READ(sc, AR_RFCNT);	/* Rx frame count. */
	cyccnt = AR_READ(sc, AR_CCCNT);	/* Cycle count. */

	if (ani->cyccnt != 0 && ani->cyccnt <= cyccnt) {
		cycdelta = cyccnt - ani->cyccnt;
		txfdelta = txfcnt - ani->txfcnt;
		rxfdelta = rxfcnt - ani->rxfcnt;
		listen_time = (cycdelta - txfdelta - rxfdelta) / 44000;
	} else
		listen_time = 0;

	ani->cyccnt = cyccnt;
	ani->txfcnt = txfcnt;
	ani->rxfcnt = rxfcnt;

	if (listen_time < 0) {
		athn_ani_restart(sc);
		return;
	}
	ani->listen_time += listen_time;

	phy1 = AR_READ(sc, AR_PHY_ERR_1);
	phy2 = AR_READ(sc, AR_PHY_ERR_2);

	if (phy1 < ani->ofdm_phy_err_base) {
		AR_WRITE(sc, AR_PHY_ERR_1, ani->ofdm_phy_err_base);
		AR_WRITE(sc, AR_PHY_ERR_MASK_1, AR_PHY_ERR_OFDM_TIMING);
	}
	if (phy2 < ani->cck_phy_err_base) {
		AR_WRITE(sc, AR_PHY_ERR_2, ani->cck_phy_err_base);
		AR_WRITE(sc, AR_PHY_ERR_MASK_2, AR_PHY_ERR_CCK_TIMING);
	}
	if (phy1 < ani->ofdm_phy_err_base || phy2 < ani->cck_phy_err_base)
		return;

	ani->ofdm_phy_err_count = phy1 - ani->ofdm_phy_err_base;
	ani->cck_phy_err_count = phy2 - ani->cck_phy_err_base;

	if (ani->listen_time > 5 * ATHN_ANI_PERIOD) {
		/* Check to see if we need to lower immunity. */
		if (ani->ofdm_phy_err_count <=
		    ani->listen_time * ani->ofdm_trig_low / 1000 &&
		    ani->cck_phy_err_count <=
		    ani->listen_time * ani->cck_trig_low / 1000)
			athn_ani_lower_immunity(sc);
		athn_ani_restart(sc);

	} else if (ani->listen_time > ATHN_ANI_PERIOD) {
		/* Check to see if we need to raise immunity. */
		if (ani->ofdm_phy_err_count >
		    ani->listen_time * ani->ofdm_trig_high / 1000) {
			athn_ani_ofdm_err_trigger(sc);
			athn_ani_restart(sc);
		} else if (ani->cck_phy_err_count >
		    ani->listen_time * ani->cck_trig_high / 1000) {
			athn_ani_cck_err_trigger(sc);
			athn_ani_restart(sc);
		}
	}
}

uint8_t
athn_chan2fbin(struct ieee80211_channel *c)
{
	if (IEEE80211_IS_CHAN_2GHZ(c))
		return (c->ic_freq - 2300);
	else
		return ((c->ic_freq - 4800) / 5);
}

void
athn_init_chains(struct athn_softc *sc)
{
	if (sc->rxchainmask == 0x5 || sc->txchainmask == 0x5)
		AR_SETBITS(sc, AR_PHY_ANALOG_SWAP, AR_PHY_SWAP_ALT_CHAIN);

	/* Setup chain masks. */
	if (sc->mac_ver <= AR_SREV_VERSION_9160 &&
	    (sc->rxchainmask == 0x3 || sc->rxchainmask == 0x5)) {
		AR_WRITE(sc, AR_PHY_RX_CHAINMASK,  0x7);
		AR_WRITE(sc, AR_PHY_CAL_CHAINMASK, 0x7);
	} else {
		AR_WRITE(sc, AR_PHY_RX_CHAINMASK,  sc->rxchainmask);
		AR_WRITE(sc, AR_PHY_CAL_CHAINMASK, sc->rxchainmask);
	}
	AR_WRITE(sc, AR_SELFGEN_MASK, sc->txchainmask);
}

void
athn_get_pier_ival(uint8_t fbin, const uint8_t *pierfreq, int npiers,
    int *lo, int *hi)
{
	int i;

	for (i = 0; i < npiers; i++)
		if (pierfreq[i] == AR_BCHAN_UNUSED ||
		    pierfreq[i] > fbin)
			break;
	*hi = i;
	*lo = *hi - 1;
	if (*lo == -1)
		*lo = *hi;
	else if (*hi == npiers || pierfreq[*hi] == AR_BCHAN_UNUSED)
		*hi = *lo;
}

uint8_t
athn_get_vpd(uint8_t pwr, const uint8_t *pwrPdg, const uint8_t *vpdPdg,
    int nicepts)
{
	uint8_t vpd;
	int i, lo, hi;

	for (i = 0; i < nicepts; i++)
		if (pwrPdg[i] > pwr)
			break;
	hi = i;
	lo = hi - 1;
	if (lo == -1)
		lo = hi;
	else if (hi == nicepts)
		hi = lo;

	vpd = athn_interpolate(pwr, pwrPdg[lo], vpdPdg[lo],
	    pwrPdg[hi], vpdPdg[hi]);
	return (vpd);
}

void
athn_get_pdadcs(struct athn_softc *sc, uint8_t fbin, struct athn_pier *lopier,
    struct athn_pier *hipier, int nxpdgains, int nicepts, uint8_t overlap,
    uint8_t *boundaries, uint8_t *pdadcs)
{
#define DB(x)	((x) / 2)	/* Convert half dB to dB. */
	uint8_t minpwr[AR_PD_GAINS_IN_MASK], maxpwr[AR_PD_GAINS_IN_MASK];
	uint8_t vpd[AR_MAX_PWR_RANGE_IN_HALF_DB], pwr;
	uint8_t lovpd, hivpd, boundary;
	int16_t ss, delta, vpdstep, val;
	int i, j, npdadcs, nvpds, maxidx, tgtidx;

	/* Compute min and max power in half dB for each pdGain. */
	for (i = 0; i < nxpdgains; i++) {
		minpwr[i] = MAX(lopier->pwr[i][0], hipier->pwr[i][0]);
		maxpwr[i] = MIN(lopier->pwr[i][nicepts - 1],
		    hipier->pwr[i][nicepts - 1]);
	}

	npdadcs = 0;
	for (i = 0; i < nxpdgains; i++) {
		if (i != nxpdgains - 1)
			boundaries[i] = DB(maxpwr[i] + minpwr[i + 1]) / 2;
		else
			boundaries[i] = DB(maxpwr[i]);
		if (boundaries[i] > AR_MAX_RATE_POWER)
			boundaries[i] = AR_MAX_RATE_POWER;

		if (i == 0 && !AR_SREV_5416_20_OR_LATER(sc)) {
			/* Fix the gain delta (AR5416 1.0 only.) */
			delta = boundaries[0] - 23;
			boundaries[0] = 23;
		} else
			delta = 0;

		/* Find starting index for this pdGain. */
		if (i != 0) {
			ss = boundaries[i - 1] - DB(minpwr[i]) -
			    overlap + 1 + delta;
		} else if (AR_SREV_9280_10_OR_LATER(sc)) {
			ss = -DB(minpwr[i]);
		} else
			ss = 0;

		/* Compute Vpd table for this pdGain. */
		nvpds = DB(maxpwr[i] - minpwr[i]) + 1;
		pwr = minpwr[i];
		for (j = 0; j < nvpds; j++) {
			/* Get lower and higher Vpd. */
			lovpd = athn_get_vpd(pwr, lopier->pwr[i],
			    lopier->vpd[i], nicepts);
			hivpd = athn_get_vpd(pwr, hipier->pwr[i],
			    hipier->vpd[i], nicepts);

			/* Interpolate the final Vpd. */
			vpd[j] = athn_interpolate(fbin,
			    lopier->fbin, lovpd, hipier->fbin, hivpd);

			pwr += 2;	/* In half dB. */
		}

		/* Extrapolate data for ss < 0. */
		if (vpd[1] > vpd[0])
			vpdstep = vpd[1] - vpd[0];
		else
			vpdstep = 1;
		while (ss < 0 && npdadcs < AR_NUM_PDADC_VALUES - 1) {
			val = vpd[0] + ss * vpdstep;
			pdadcs[npdadcs++] = MAX(val, 0);
			ss++;
		}

		tgtidx = boundaries[i] + overlap - DB(minpwr[i]);
		maxidx = MIN(tgtidx, nvpds);
		while (ss < maxidx && npdadcs < AR_NUM_PDADC_VALUES - 1)
			pdadcs[npdadcs++] = vpd[ss++];

		if (tgtidx <= maxidx)
			continue;

		/* Extrapolate data for maxidx <= ss <= tgtidx. */
		if (vpd[nvpds - 1] > vpd[nvpds - 2])
			vpdstep = vpd[nvpds - 1] - vpd[nvpds - 2];
		else
			vpdstep = 1;
		while (ss <= tgtidx && npdadcs < AR_NUM_PDADC_VALUES - 1) {
			val = vpd[nvpds - 1] + (ss - maxidx + 1) * vpdstep;
			pdadcs[npdadcs++] = MIN(val, 255);
			ss++;
		}
	}

	/* Fill remaining PDADC and boundaries entries. */
	if (AR_SREV_9285(sc))
		boundary = AR9285_PD_GAIN_BOUNDARY_DEFAULT;
	else	/* Fill with latest. */
		boundary = boundaries[nxpdgains - 1];

	for (; nxpdgains < AR_PD_GAINS_IN_MASK; nxpdgains++)
		boundaries[nxpdgains] = boundary;

	for (; npdadcs < AR_NUM_PDADC_VALUES; npdadcs++)
		pdadcs[npdadcs] = pdadcs[npdadcs - 1];
#undef DB
}

int
athn_interpolate(int x, int x1, int y1, int x2, int y2)
{
	if (x1 == x2)	/* Prevents division by zero. */
		return (y1);
	/* Linear interpolation. */
	return (y1 + ((x - x1) * (y2 - y1)) / (x2 - x1));
}

void
athn_get_lg_tpow(struct athn_softc *sc, struct ieee80211_channel *c,
    uint8_t ctl, const struct ar_cal_target_power_leg *tgt, int nchans,
    uint8_t tpow[4])
{
	uint8_t fbin;
	int i, lo, hi;

	/* Find interval (lower and upper indices.) */
	fbin = athn_chan2fbin(c);
	for (i = 0; i < nchans; i++) {
		if (tgt[i].bChannel == AR_BCHAN_UNUSED ||
		    tgt[i].bChannel > fbin)
			break;
	}
	hi = i;
	lo = hi - 1;
	if (lo == -1)
		lo = hi;
	else if (hi == nchans || tgt[hi].bChannel == AR_BCHAN_UNUSED)
		hi = lo;

	/* Interpolate values. */
	for (i = 0; i < 4; i++) {
		tpow[i] = athn_interpolate(fbin,
		    tgt[lo].bChannel, tgt[lo].tPow2x[i],
		    tgt[hi].bChannel, tgt[hi].tPow2x[i]);
	}
	/* XXX Apply conformance test limit. */
}

#ifndef IEEE80211_NO_HT
void
athn_get_ht_tpow(struct athn_softc *sc, struct ieee80211_channel *c,
    uint8_t ctl, const struct ar_cal_target_power_ht *tgt, int nchans,
    uint8_t tpow[8])
{
	uint8_t fbin;
	int i, lo, hi;

	/* Find interval (lower and upper indices.) */
	fbin = athn_chan2fbin(c);
	for (i = 0; i < nchans; i++) {
		if (tgt[i].bChannel == AR_BCHAN_UNUSED ||
		    tgt[i].bChannel > fbin)
			break;
	}
	hi = i;
	lo = hi - 1;
	if (lo == -1)
		lo = hi;
	else if (hi == nchans || tgt[hi].bChannel == AR_BCHAN_UNUSED)
		hi = lo;

	/* Interpolate values. */
	for (i = 0; i < 8; i++) {
		tpow[i] = athn_interpolate(fbin,
		    tgt[lo].bChannel, tgt[lo].tPow2x[i],
		    tgt[hi].bChannel, tgt[hi].tPow2x[i]);
	}
	/* XXX Apply conformance test limit. */
}
#endif

void
athn_write_txpower(struct athn_softc *sc, int16_t power[ATHN_POWER_COUNT])
{
	AR_WRITE(sc, AR_PHY_POWER_TX_RATE1,
	    (power[ATHN_POWER_OFDM18  ] & 0x3f) << 24 |
	    (power[ATHN_POWER_OFDM12  ] & 0x3f) << 16 |
	    (power[ATHN_POWER_OFDM9   ] & 0x3f) <<  8 |
	    (power[ATHN_POWER_OFDM6   ] & 0x3f));
	AR_WRITE(sc, AR_PHY_POWER_TX_RATE2,
	    (power[ATHN_POWER_OFDM54  ] & 0x3f) << 24 |
	    (power[ATHN_POWER_OFDM48  ] & 0x3f) << 16 |
	    (power[ATHN_POWER_OFDM36  ] & 0x3f) <<  8 |
	    (power[ATHN_POWER_OFDM24  ] & 0x3f));
	AR_WRITE(sc, AR_PHY_POWER_TX_RATE3,
	    (power[ATHN_POWER_CCK2_SP ] & 0x3f) << 24 |
	    (power[ATHN_POWER_CCK2_LP ] & 0x3f) << 16 |
	    (power[ATHN_POWER_XR      ] & 0x3f) <<  8 |
	    (power[ATHN_POWER_CCK1_LP ] & 0x3f));
	AR_WRITE(sc, AR_PHY_POWER_TX_RATE4,
	    (power[ATHN_POWER_CCK11_SP] & 0x3f) << 24 |
	    (power[ATHN_POWER_CCK11_LP] & 0x3f) << 16 |
	    (power[ATHN_POWER_CCK55_SP] & 0x3f) <<  8 |
	    (power[ATHN_POWER_CCK55_LP] & 0x3f));
#ifndef IEEE80211_NO_HT
	AR_WRITE(sc, AR_PHY_POWER_TX_RATE5,
	    (power[ATHN_POWER_HT20(3) ] & 0x3f) << 24 |
	    (power[ATHN_POWER_HT20(2) ] & 0x3f) << 16 |
	    (power[ATHN_POWER_HT20(1) ] & 0x3f) <<  8 |
	    (power[ATHN_POWER_HT20(0) ] & 0x3f));
	AR_WRITE(sc, AR_PHY_POWER_TX_RATE6,
	    (power[ATHN_POWER_HT20(7) ] & 0x3f) << 24 |
	    (power[ATHN_POWER_HT20(6) ] & 0x3f) << 16 |
	    (power[ATHN_POWER_HT20(5) ] & 0x3f) <<  8 |
	    (power[ATHN_POWER_HT20(4) ] & 0x3f));
	AR_WRITE(sc, AR_PHY_POWER_TX_RATE7,
	    (power[ATHN_POWER_HT40(3) ] & 0x3f) << 24 |
	    (power[ATHN_POWER_HT40(2) ] & 0x3f) << 16 |
	    (power[ATHN_POWER_HT40(1) ] & 0x3f) <<  8 |
	    (power[ATHN_POWER_HT40(0) ] & 0x3f));
	AR_WRITE(sc, AR_PHY_POWER_TX_RATE8,
	    (power[ATHN_POWER_HT40(7) ] & 0x3f) << 24 |
	    (power[ATHN_POWER_HT40(6) ] & 0x3f) << 16 |
	    (power[ATHN_POWER_HT40(5) ] & 0x3f) <<  8 |
	    (power[ATHN_POWER_HT40(4) ] & 0x3f));
	AR_WRITE(sc, AR_PHY_POWER_TX_RATE9,
	    (power[ATHN_POWER_OFDM_EXT] & 0x3f) << 24 |
	    (power[ATHN_POWER_CCK_EXT ] & 0x3f) << 16 |
	    (power[ATHN_POWER_OFDM_DUP] & 0x3f) <<  8 |
	    (power[ATHN_POWER_CCK_DUP ] & 0x3f));
#endif
}

void
athn_init_baseband(struct athn_softc *sc)
{
	uint32_t synth_delay;

	synth_delay = athn_synth_delay(sc);
	/* Activate the PHY (includes baseband activate and synthesizer on). */
	AR_WRITE(sc, AR_PHY_ACTIVE, AR_PHY_ACTIVE_EN);
	DELAY(AR_BASE_PHY_ACTIVE_DELAY + synth_delay);
}

void
athn_init_dma(struct athn_softc *sc)
{
	uint32_t reg;

	/* Set AHB not to do cacheline prefetches. */
	AR_SETBITS(sc, AR_AHB_MODE, AR_AHB_PREFETCH_RD_EN);

	reg = AR_READ(sc, AR_TXCFG);
	/* Let MAC DMA reads be in 128-byte chunks. */
	reg = RW(reg, AR_TXCFG_DMASZ, AR_DMASZ_128B);

	/* Set initial Tx trigger level. */
	if (AR_SREV_9285(sc))
		reg = RW(reg, AR_TXCFG_FTRIG, AR_TXCFG_FTRIG_256B);
	else
		reg = RW(reg, AR_TXCFG_FTRIG, AR_TXCFG_FTRIG_512B);
	AR_WRITE(sc, AR_TXCFG, reg);

	/* Let MAC DMA writes be in 128-byte chunks. */
	reg = AR_READ(sc, AR_RXCFG);
	reg = RW(reg, AR_RXCFG_DMASZ, AR_DMASZ_128B);
	AR_WRITE(sc, AR_RXCFG, reg);

	/* Setup Rx FIFO threshold to hold off Tx activities. */
	AR_WRITE(sc, AR_RXFIFO_CFG, 512);

	/* Reduce the number of entries in PCU TXBUF to avoid wrap around. */
	AR_WRITE(sc, AR_PCU_TXBUF_CTRL, AR_SREV_9285(sc) ?
	    AR9285_PCU_TXBUF_CTRL_USABLE_SIZE :
	    AR_PCU_TXBUF_CTRL_USABLE_SIZE);
}

void
athn_inc_tx_trigger_level(struct athn_softc *sc)
{
	uint32_t reg, ftrig;

	reg = AR_READ(sc, AR_TXCFG);
	ftrig = MS(reg, AR_TXCFG_FTRIG);
	/*
	 * NB: The AR9285 and all single-stream parts have an issue that
	 * limits the size of the PCU Tx FIFO to 2KB instead of 4KB.
	 */
	if (ftrig == (AR_SREV_9285(sc) ? 0x1f : 0x3f))
		return;		/* Already at max. */
	reg = RW(reg, AR_TXCFG_FTRIG, ftrig + 1);
	AR_WRITE(sc, AR_TXCFG, reg);
}

int
athn_stop_rx_dma(struct athn_softc *sc)
{
	int ntries;

	AR_WRITE(sc, AR_CR, AR_CR_RXD);
	/* Wait for Rx enable bit to go low. */
	for (ntries = 0; ntries < 100; ntries++) {
		if (!(AR_READ(sc, AR_CR) & AR_CR_RXE))
			return (0);
		DELAY(100);
	}
	DPRINTF(("Rx DMA failed to stop\n"));
	return (ETIMEDOUT);
}

int
athn_rx_abort(struct athn_softc *sc)
{
	int ntries;

	AR_SETBITS(sc, AR_DIAG_SW, AR_DIAG_RX_DIS | AR_DIAG_RX_ABORT);
	for (ntries = 0; ntries < 1000; ntries++) {
		if (MS(AR_READ(sc, AR_OBS_BUS_1), AR_OBS_BUS_1_RX_STATE) == 0)
			return (0);
		DELAY(10);
	}
	DPRINTF(("Rx failed to go idle in 10ms\n"));
	AR_CLRBITS(sc, AR_DIAG_SW, AR_DIAG_RX_DIS | AR_DIAG_RX_ABORT);
	return (ETIMEDOUT);
}

int
athn_tx_pending(struct athn_softc *sc, int qid)
{
	return (MS(AR_READ(sc, AR_QSTS(qid)), AR_Q_STS_PEND_FR_CNT) != 0 ||
	    (AR_READ(sc, AR_Q_TXE) & (1 << qid)) != 0);
}

void
athn_stop_tx_dma(struct athn_softc *sc, int qid)
{
	uint32_t tsflo;
	int ntries, i;

	AR_WRITE(sc, AR_Q_TXD, 1 << qid);
	for (ntries = 0; ntries < 40; ntries++) {
		if (!athn_tx_pending(sc, qid))
			break;
		DELAY(100);
	}
	if (ntries == 40) {
		for (i = 0; i < 2; i++) {
			tsflo = AR_READ(sc, AR_TSF_L32) / 1024;
			AR_WRITE(sc, AR_QUIET2,
			    SM(AR_QUIET2_QUIET_DUR, 10));
			AR_WRITE(sc, AR_QUIET_PERIOD, 100);
			AR_WRITE(sc, AR_NEXT_QUIET_TIMER, tsflo);
			AR_SETBITS(sc, AR_TIMER_MODE, AR_QUIET_TIMER_EN);
			if (AR_READ(sc, AR_TSF_L32) / 1024 != tsflo)
				break;
		}
		AR_SETBITS(sc, AR_DIAG_SW, AR_DIAG_FORCE_CH_IDLE_HIGH);
		DELAY(200);
		AR_CLRBITS(sc, AR_TIMER_MODE, AR_QUIET_TIMER_EN);

		for (ntries = 0; ntries < 40; ntries++) {
			if (!athn_tx_pending(sc, qid))
				break;
			DELAY(100);
		}

		AR_CLRBITS(sc, AR_DIAG_SW, AR_DIAG_FORCE_CH_IDLE_HIGH);
	}
	AR_WRITE(sc, AR_Q_TXD, 0);
}

void
athn_tx_reclaim(struct athn_softc *sc, int qid)
{
	struct athn_txq *txq = &sc->txq[qid];
	struct athn_tx_buf *bf;

	/* Reclaim all buffers queued in the specified Tx queue. */
	/* NB: Tx DMA must be stopped. */
	while ((bf = SIMPLEQ_FIRST(&txq->head)) != NULL) {
		SIMPLEQ_REMOVE_HEAD(&txq->head, bf_list);

		bus_dmamap_sync(sc->sc_dmat, bf->bf_map, 0,
		    bf->bf_map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, bf->bf_map);
		m_freem(bf->bf_m);
		bf->bf_m = NULL;
		bf->bf_ni = NULL;	/* Nodes already freed! */

		/* Link Tx buffer back to global free list. */
		SIMPLEQ_INSERT_TAIL(&sc->txbufs, bf, bf_list);
	}
}

#if NBPFILTER > 0
void
athn_rx_radiotap(struct athn_softc *sc, struct mbuf *m, struct ar_rx_desc *ds)
{
#define IEEE80211_RADIOTAP_F_SHORTGI	0x80	/* XXX from FBSD */

	struct athn_rx_radiotap_header *tap = &sc->sc_rxtap;
	struct ieee80211com *ic = &sc->sc_ic;
	struct mbuf mb;
	uint64_t tsf;
	uint32_t tstamp;
	uint8_t rate;

	/* Extend the 15-bit timestamp from Rx descriptor to 64-bit TSF. */
	tstamp = ds->ds_status2;
	tsf = AR_READ(sc, AR_TSF_U32);
	tsf = tsf << 32 | AR_READ(sc, AR_TSF_L32);
	if ((tsf & 0x7fff) < tstamp)
		tsf -= 0x8000;
	tsf = (tsf & ~0x7fff) | tstamp;

	tap->wr_flags = IEEE80211_RADIOTAP_F_FCS;
	tap->wr_tsft = htole64(tsf);
	tap->wr_chan_freq = htole16(ic->ic_bss->ni_chan->ic_freq);
	tap->wr_chan_flags = htole16(ic->ic_bss->ni_chan->ic_flags);
	tap->wr_dbm_antsignal = MS(ds->ds_status4, AR_RXS4_RSSI_COMBINED);
	/* XXX noise. */
	tap->wr_antenna = MS(ds->ds_status3, AR_RXS3_ANTENNA);
	tap->wr_rate = 0;	/* In case it can't be found below. */
	if (AR_SREV_5416_20_OR_LATER(sc))
		rate = MS(ds->ds_status0, AR_RXS0_RATE);
	else
		rate = MS(ds->ds_status3, AR_RXS3_RATE);
	if (rate & 0x80) {		/* HT. */
		/* Bit 7 set means HT MCS instead of rate. */
		tap->wr_rate = rate;
		if (!(ds->ds_status3 & AR_RXS3_GI))
			tap->wr_flags |= IEEE80211_RADIOTAP_F_SHORTGI;

	} else if (rate & 0x10) {	/* CCK. */
		if (rate & 0x04)
			tap->wr_flags |= IEEE80211_RADIOTAP_F_SHORTPRE;
		switch (rate & ~0x14) {
		case 0xb: tap->wr_rate =   2; break;
		case 0xa: tap->wr_rate =   4; break;
		case 0x9: tap->wr_rate =  11; break;
		case 0x8: tap->wr_rate =  22; break;
		}
	} else {			/* OFDM. */
		switch (rate) {
		case 0xb: tap->wr_rate =  12; break;
		case 0xf: tap->wr_rate =  18; break;
		case 0xa: tap->wr_rate =  24; break;
		case 0xe: tap->wr_rate =  36; break;
		case 0x9: tap->wr_rate =  48; break;
		case 0xd: tap->wr_rate =  72; break;
		case 0x8: tap->wr_rate =  96; break;
		case 0xc: tap->wr_rate = 108; break;
		}
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

static __inline int
athn_rx_process(struct athn_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct athn_rxq *rxq = &sc->rxq;
	struct athn_rx_buf *bf, *nbf;
	struct ar_rx_desc *ds;
	struct ieee80211_frame *wh;
	struct ieee80211_rxinfo rxi;
	struct ieee80211_node *ni;
	struct mbuf *m, *m1;
	int error, len;

	bf = SIMPLEQ_FIRST(&rxq->head);
	if (__predict_false(bf == NULL)) {	/* Should not happen. */
		printf("%s: Rx queue is empty!\n", sc->sc_dev.dv_xname);
		return (ENOENT);
	}
	ds = bf->bf_desc;

	if (!(ds->ds_status8 & AR_RXS8_DONE)) {
		/*
		 * On some parts, the status words can get corrupted
		 * (including the "done" bit), so we check the next
		 * descriptor "done" bit.  If it is set, it is a good
		 * indication that the status words are corrupted, so
		 * we skip this descriptor and drop the frame.
		 */
		nbf = SIMPLEQ_NEXT(bf, bf_list);
		if (nbf != NULL &&
		    (nbf->bf_desc->ds_status8 & AR_RXS8_DONE)) {
			DPRINTF(("corrupted descriptor status=0x%x\n",
			    ds->ds_status8));
			/* HW will not "move" RXDP in this case, so do it. */
			AR_WRITE(sc, AR_RXDP, nbf->bf_daddr);
			ifp->if_ierrors++;
			goto skip;
		}
		return (EBUSY);
	}

	if (__predict_false(ds->ds_status1 & AR_RXS1_MORE)) {
		/* Drop frames that span multiple Rx descriptors. */
		DPRINTF(("dropping split frame\n"));
		ifp->if_ierrors++;
		goto skip;
	}
	if (!(ds->ds_status8 & AR_RXS8_FRAME_OK)) {
		if (ds->ds_status8 & AR_RXS8_CRC_ERR)
			DPRINTFN(6, ("CRC error\n"));
		else if (ds->ds_status8 & AR_RXS8_PHY_ERR)
			DPRINTFN(6, ("PHY error=0x%x\n",
			    MS(ds->ds_status8, AR_RXS8_PHY_ERR_CODE)));
		else if (ds->ds_status8 & AR_RXS8_DECRYPT_CRC_ERR)
			DPRINTFN(6, ("Decryption CRC error\n"));
		else if (ds->ds_status8 & AR_RXS8_MICHAEL_ERR) {
			DPRINTFN(2, ("Michael MIC failure\n"));
			/* Report Michael MIC failures to net80211. */
			ic->ic_stats.is_rx_locmicfail++;
			ieee80211_michael_mic_failure(ic, 0);
			/*
			 * XXX Check that it is not a control frame
			 * (invalid MIC failures on valid ctl frames.)
			 */
		}
		ifp->if_ierrors++;
		goto skip;
	}

	len = MS(ds->ds_status1, AR_RXS1_DATA_LEN);
	if (__predict_false(len == 0 || len > ATHN_RXBUFSZ)) {
		DPRINTF(("corrupted descriptor length=%d\n", len));
		ifp->if_ierrors++;
		goto skip;
	}

	/* Allocate a new Rx buffer. */
	m1 = MCLGETI(NULL, M_DONTWAIT, NULL, ATHN_RXBUFSZ);
	if (__predict_false(m1 == NULL)) {
		ic->ic_stats.is_rx_nombuf++;
		ifp->if_ierrors++;
		goto skip;
	}

	/* Sync and unmap the old Rx buffer. */
	bus_dmamap_sync(sc->sc_dmat, bf->bf_map, 0, ATHN_RXBUFSZ,
	    BUS_DMASYNC_POSTREAD);
	bus_dmamap_unload(sc->sc_dmat, bf->bf_map);

	/* Map the new Rx buffer. */
	error = bus_dmamap_load(sc->sc_dmat, bf->bf_map, mtod(m1, void *),
	    ATHN_RXBUFSZ, NULL, BUS_DMA_NOWAIT | BUS_DMA_READ);
	if (__predict_false(error != 0)) {
		m_freem(m1);

		/* Remap the old Rx buffer or panic. */
		error = bus_dmamap_load(sc->sc_dmat, bf->bf_map,
		    mtod(bf->bf_m, void *), ATHN_RXBUFSZ, NULL,
		    BUS_DMA_NOWAIT | BUS_DMA_READ);
		KASSERT(error != 0);
		ifp->if_ierrors++;
		goto skip;
	}

	bus_dmamap_sync(sc->sc_dmat, bf->bf_map, 0, bf->bf_map->dm_mapsize,
	    BUS_DMASYNC_PREREAD);

	/* Write physical address of new Rx buffer. */
	ds->ds_data = bf->bf_map->dm_segs[0].ds_addr;

	m = bf->bf_m;
	bf->bf_m = m1;

	/* Finalize mbuf. */
	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = m->m_len = len;

	/* Grab a reference to the source node. */
	wh = mtod(m, struct ieee80211_frame *);
	ni = ieee80211_find_rxnode(ic, wh);

	/* Remove any HW padding after the 802.11 header. */
	if (!(wh->i_fc[0] & IEEE80211_FC0_TYPE_CTL)) {
		u_int hdrlen = ieee80211_get_hdrlen(wh);
		if (hdrlen & 3) {
			ovbcopy(wh, (caddr_t)wh + 2, hdrlen);
			m_adj(m, 2);	/* XXX sure? */
		}
	}
#if NBPFILTER > 0
	if (__predict_false(sc->sc_drvbpf != NULL))
		athn_rx_radiotap(sc, m, ds);
#endif
	/* Trim 802.11 FCS after radiotap. */
	m_adj(m, -IEEE80211_CRC_LEN);

	/* Send the frame to the 802.11 layer. */
	rxi.rxi_flags = 0;	/* XXX */
	rxi.rxi_rssi = MS(ds->ds_status4, AR_RXS4_RSSI_COMBINED);
	rxi.rxi_tstamp = ds->ds_status2;
	ieee80211_input(ifp, m, ni, &rxi);

	/* Node is no longer needed. */
	ieee80211_release_node(ic, ni);

 skip:
	/* Unlink this descriptor from head. */
	SIMPLEQ_REMOVE_HEAD(&rxq->head, bf_list);
	memset(&ds->ds_status0, 0, 36);	/* XXX Really needed? */
	ds->ds_status8 &= ~AR_RXS8_DONE;
	ds->ds_link = 0;

	/* Re-use this descriptor and link it to tail. */
	if (__predict_true(!SIMPLEQ_EMPTY(&rxq->head)))
		rxq->lastds->ds_link = bf->bf_daddr;
	else
		AR_WRITE(sc, AR_RXDP, bf->bf_daddr);
	SIMPLEQ_INSERT_TAIL(&rxq->head, bf, bf_list);
	rxq->lastds = ds;

	/* Re-enable Rx. */
	AR_WRITE(sc, AR_CR, AR_CR_RXE);
	return (0);
}

void
athn_rx_intr(struct athn_softc *sc)
{
	while (athn_rx_process(sc) == 0);
}

int
athn_tx_process(struct athn_softc *sc, int qid)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct athn_txq *txq = &sc->txq[qid];
	struct athn_node *an;
	struct athn_tx_buf *bf;
	struct ar_tx_desc *ds;
	uint8_t failcnt;

	bf = SIMPLEQ_FIRST(&txq->head);
	if (__predict_false(bf == NULL))
		return (ENOENT);
	/* Get descriptor of last DMA segment. */
	ds = &bf->bf_descs[bf->bf_map->dm_nsegs - 1];

	if (!(ds->ds_status9 & AR_TXS9_DONE))
		return (EBUSY);

	SIMPLEQ_REMOVE_HEAD(&txq->head, bf_list);
	ifp->if_opackets++;

	sc->sc_tx_timer = 0;

	if (ds->ds_status1 & AR_TXS1_EXCESSIVE_RETRIES)
		ifp->if_oerrors++;

	if (ds->ds_status1 & AR_TXS1_UNDERRUN)
		athn_inc_tx_trigger_level(sc);

	an = (struct athn_node *)bf->bf_ni;
	/*
	 * NB: the data fail count contains the number of un-acked tries
	 * for the final series used.  We must add the number of tries for
	 * each series that was fully processed.
	 */
	failcnt  = MS(ds->ds_status1, AR_TXS1_DATA_FAIL_CNT);
	/* XXX Assume two tries per series. */
	failcnt += MS(ds->ds_status9, AR_TXS9_FINAL_IDX) * 2;

	/* Update rate control statistics. */
	an->amn.amn_txcnt++;
	if (failcnt > 0)
		an->amn.amn_retrycnt++;

	DPRINTFN(5, ("Tx done qid=%d status1=%d fail count=%d\n",
	    qid, ds->ds_status1, failcnt));

	bus_dmamap_sync(sc->sc_dmat, bf->bf_map, 0, bf->bf_map->dm_mapsize,
	    BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(sc->sc_dmat, bf->bf_map);

	m_freem(bf->bf_m);
	bf->bf_m = NULL;
	ieee80211_release_node(ic, bf->bf_ni);
	bf->bf_ni = NULL;

	/* Link Tx buffer back to global free list. */
	SIMPLEQ_INSERT_TAIL(&sc->txbufs, bf, bf_list);
	return (0);
}

void
athn_tx_intr(struct athn_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	uint16_t mask = 0;
	uint32_t reg;
	int qid;

	reg = AR_READ(sc, AR_ISR_S0_S);
	mask |= MS(reg, AR_ISR_S0_QCU_TXOK);
	mask |= MS(reg, AR_ISR_S0_QCU_TXDESC);

	reg = AR_READ(sc, AR_ISR_S1_S);
	mask |= MS(reg, AR_ISR_S1_QCU_TXERR);
	mask |= MS(reg, AR_ISR_S1_QCU_TXEOL);

	DPRINTFN(4, ("Tx interrupt mask=0x%x\n", mask));
	for (qid = 0; mask != 0; mask >>= 1, qid++) {
		if (mask & 1)
			while (athn_tx_process(sc, qid) == 0);
	}
	if (!SIMPLEQ_EMPTY(&sc->txbufs)) {
		ifp->if_flags &= ~IFF_OACTIVE;
		athn_start(ifp);
	}
}

int
athn_txtime(struct athn_softc *sc, int len, int ridx, u_int flags)
{
#define divround(a, b)	(((a) + (b) - 1) / (b))
	int txtime;

	/* XXX HT. */
	if (athn_rates[ridx].phy == IEEE80211_T_OFDM) {
		txtime = divround(8 + 4 * len + 3, athn_rates[ridx].rate);
		/* SIFS is 10us for 11g but Signal Extension adds 6us. */
		txtime = 16 + 4 + 4 * txtime + 16;
	} else {
		txtime = divround(16 * len, athn_rates[ridx].rate);
		if (ridx != ATHN_RIDX_CCK1 && (flags & IEEE80211_F_SHPREAMBLE))
			txtime +=  72 + 24;
		else
			txtime += 144 + 48;
		txtime += 10;	/* 10us SIFS. */
	}
	return (txtime);
#undef divround
}

int
athn_tx(struct athn_softc *sc, struct mbuf *m, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_key *k = NULL;
	struct ieee80211_frame *wh;
	struct athn_series series[4];
	struct ar_tx_desc *ds, *lastds;
	struct athn_txq *txq;
	struct athn_tx_buf *bf;
	struct athn_node *an = (void *)ni;
	struct mbuf *m1;
	uintptr_t entry;
	uint16_t qos;
	uint8_t txpower, type, encrtype, tid, ridx[4];
	int i, error, totlen, hasqos, qid;

	/* Grab a Tx buffer from our global free list. */
	bf = SIMPLEQ_FIRST(&sc->txbufs);
	KASSERT(bf != NULL);
	SIMPLEQ_REMOVE_HEAD(&sc->txbufs, bf_list);

	/* Map 802.11 frame type to hardware frame type. */
	wh = mtod(m, struct ieee80211_frame *);
	if ((wh->i_fc[0] &
	    (IEEE80211_FC0_TYPE_MASK | IEEE80211_FC0_SUBTYPE_MASK)) ==
	    (IEEE80211_FC0_TYPE_MGT  | IEEE80211_FC0_SUBTYPE_BEACON))
		type = AR_FRAME_TYPE_BEACON;
	else if ((wh->i_fc[0] &
	    (IEEE80211_FC0_TYPE_MASK | IEEE80211_FC0_SUBTYPE_MASK)) ==
	    (IEEE80211_FC0_TYPE_MGT  | IEEE80211_FC0_SUBTYPE_PROBE_RESP))
		type = AR_FRAME_TYPE_PROBE_RESP;
	else if ((wh->i_fc[0] &
	    (IEEE80211_FC0_TYPE_MASK | IEEE80211_FC0_SUBTYPE_MASK)) ==
	    (IEEE80211_FC0_TYPE_MGT  | IEEE80211_FC0_SUBTYPE_ATIM))
		type = AR_FRAME_TYPE_ATIM;
	else if ((wh->i_fc[0] &
	    (IEEE80211_FC0_TYPE_MASK | IEEE80211_FC0_SUBTYPE_MASK)) ==
	    (IEEE80211_FC0_TYPE_CTL  | IEEE80211_FC0_SUBTYPE_PS_POLL))
		type = AR_FRAME_TYPE_PSPOLL;
	else
		type = AR_FRAME_TYPE_NORMAL;

	if (wh->i_fc[1] & IEEE80211_FC1_PROTECTED) {
		k = ieee80211_get_txkey(ic, wh, ni);
		if ((m = ieee80211_encrypt(ic, m, k)) == NULL)
			return (ENOBUFS);
		wh = mtod(m, struct ieee80211_frame *);
	}

	/* XXX 2-byte padding for QoS and 4-addr headers. */

	/* Select the HW Tx queue to use for this frame. */
	if ((hasqos = ieee80211_has_qos(wh))) {
		qos = ieee80211_get_qos(wh);
		tid = qos & IEEE80211_QOS_TID;
		qid = athn_ac2qid[ieee80211_up_to_ac(ic, tid)];
	} else if (type == AR_FRAME_TYPE_BEACON) {
		qid = ATHN_QID_BEACON;
	} else if (type == AR_FRAME_TYPE_PSPOLL) {
		qid = ATHN_QID_PSPOLL;
	} else
		qid = ATHN_QID_AC_BE;
	txq = &sc->txq[qid];

	/* Select the transmit rates to use for this frame. */
	if (IEEE80211_IS_MULTICAST(wh->i_addr1) ||
	    (wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) !=
	    IEEE80211_FC0_TYPE_DATA) {
		/* Use lowest rate for all tries. */
		ridx[0] = ridx[1] = ridx[2] = ridx[3] =
		    (ic->ic_curmode == IEEE80211_MODE_11A) ?
			ATHN_RIDX_OFDM6 : ATHN_RIDX_CCK1;
	} else if (ic->ic_fixed_rate != -1) {
		/* Use same fixed rate for all tries. */
		ridx[0] = ridx[1] = ridx[2] = ridx[3] =
		    sc->fixed_ridx;
	} else {
		int txrate = ni->ni_txrate;
		/* Use fallback table of the node. */
		for (i = 0; i < 4; i++) {
			ridx[i] = an->ridx[txrate];
			txrate = an->fallback[txrate];
		}
	}

#if NBPFILTER > 0
	if (__predict_false(sc->sc_drvbpf != NULL)) {
		struct athn_tx_radiotap_header *tap = &sc->sc_txtap;
		struct mbuf mb;

		tap->wt_flags = 0;
		/* Use initial transmit rate. */
		tap->wt_rate = athn_rates[ridx[0]].rate;
		tap->wt_chan_freq = htole16(ic->ic_bss->ni_chan->ic_freq);
		tap->wt_chan_flags = htole16(ic->ic_bss->ni_chan->ic_flags);
		tap->wt_hwqueue = qid;
		if (ridx[0] != ATHN_RIDX_CCK1 &&
		    (ic->ic_flags & IEEE80211_F_SHPREAMBLE))
			tap->wt_flags |= IEEE80211_RADIOTAP_F_SHORTPRE;
		mb.m_data = (caddr_t)tap;
		mb.m_len = sc->sc_txtap_len;
		mb.m_next = m;
		mb.m_nextpkt = NULL;
		mb.m_type = 0;
		mb.m_flags = 0;
		bpf_mtap(sc->sc_drvbpf, &mb, BPF_DIRECTION_OUT);
	}
#endif

	/* DMA map mbuf. */
	error = bus_dmamap_load_mbuf(sc->sc_dmat, bf->bf_map, m,
	    BUS_DMA_NOWAIT | BUS_DMA_WRITE);
	if (__predict_false(error != 0)) {
		if (error != EFBIG) {
			printf("%s: can't map mbuf (error %d)\n",
			    sc->sc_dev.dv_xname, error);
			m_freem(m);
			return (error);
		}
		/*
		 * DMA mapping requires too many DMA segments; linearize
		 * mbuf in kernel virtual address space and retry.
		 */
		MGETHDR(m1, M_DONTWAIT, MT_DATA);
		if (m1 == NULL) {
			m_freem(m);
			return (ENOBUFS);
		}
		if (m->m_pkthdr.len > MHLEN) {
			MCLGET(m1, M_DONTWAIT);
			if (!(m1->m_flags & M_EXT)) {
				m_freem(m);
				m_freem(m1);
				return (ENOBUFS);
			}
		}
		m_copydata(m, 0, m->m_pkthdr.len, mtod(m1, caddr_t));
		m1->m_pkthdr.len = m1->m_len = m->m_pkthdr.len;
		m_freem(m);
		m = m1;

		error = bus_dmamap_load_mbuf(sc->sc_dmat, bf->bf_map, m,
		    BUS_DMA_NOWAIT | BUS_DMA_WRITE);
		if (error != 0) {
			printf("%s: can't map mbuf (error %d)\n",
			    sc->sc_dev.dv_xname, error);
			m_freem(m);
			return (error);
		}
	}
	bf->bf_m = m;
	bf->bf_ni = ni;

	wh = mtod(m, struct ieee80211_frame *);

	totlen = m->m_pkthdr.len + IEEE80211_CRC_LEN;

	/* Clear all Tx descriptors that we will use. */
	memset(bf->bf_descs, 0, bf->bf_map->dm_nsegs * sizeof (*ds));

	/* Setup first Tx descriptor. */
	ds = &bf->bf_descs[0];

	ds->ds_ctl0 = AR_TXC0_INTR_REQ | AR_TXC0_CLR_DEST_MASK;
	txpower = AR_MAX_RATE_POWER;	/* Get from per-rate registers. */
	ds->ds_ctl0 |= SM(AR_TXC0_XMIT_POWER, txpower);

	ds->ds_ctl1 = SM(AR_TXC1_FRAME_TYPE, type);

	if (IEEE80211_IS_MULTICAST(wh->i_addr1) ||
	    (hasqos && (qos & IEEE80211_QOS_ACK_POLICY_MASK) ==
	     IEEE80211_QOS_ACK_POLICY_NOACK))
		ds->ds_ctl1 |= AR_TXC1_NO_ACK;

	if (0 && wh->i_fc[1] & IEEE80211_FC1_PROTECTED) {
		/* Retrieve key for encryption. */
		k = ieee80211_get_txkey(ic, wh, ni);
		/*
		 * Map 802.11 cipher to hardware encryption type and
		 * compute crypto overhead.
		 */
		switch (k->k_cipher) {
		case IEEE80211_CIPHER_WEP40:
		case IEEE80211_CIPHER_WEP104:
			encrtype = AR_ENCR_TYPE_WEP;
			totlen += 8;
			break;
		case IEEE80211_CIPHER_TKIP:
			encrtype = AR_ENCR_TYPE_TKIP;
			totlen += 20;
			break;
		case IEEE80211_CIPHER_CCMP:
			encrtype = AR_ENCR_TYPE_AES;
			totlen += 16;
			break;
		default:
			panic("unsupported cipher");	/* XXX BIP? */
		}
		/*
		 * NB: The key cache entry index is stored in the key
		 * private field when the key is installed.
		 */
		entry = (uintptr_t)k->k_priv;
		ds->ds_ctl1 |= SM(AR_TXC1_DEST_IDX, entry);
		ds->ds_ctl0 |= AR_TXC0_DEST_IDX_VALID;
	} else
		encrtype = AR_ENCR_TYPE_CLEAR;
	ds->ds_ctl6 = SM(AR_TXC6_ENCR_TYPE, encrtype);

	/* Check if frame must be protected using RTS/CTS or CTS-to-self. */
	if (!IEEE80211_IS_MULTICAST(wh->i_addr1)) {
		/* NB: Group frames are sent using CCK in 802.11b/g. */
		if (totlen > ic->ic_rtsthreshold) {
			ds->ds_ctl0 |= AR_TXC0_RTS_ENABLE;
		} else if ((ic->ic_flags & IEEE80211_F_USEPROT) &&
		    athn_rates[ridx[0]].phy == IEEE80211_T_OFDM) {
			if (ic->ic_protmode == IEEE80211_PROT_RTSCTS)
				ds->ds_ctl0 |= AR_TXC0_RTS_ENABLE;
			else if (ic->ic_protmode == IEEE80211_PROT_CTSONLY)
				ds->ds_ctl0 |= AR_TXC0_CTS_ENABLE;
		}
	}
	if (ds->ds_ctl0 & (AR_TXC0_RTS_ENABLE | AR_TXC0_CTS_ENABLE)) {
		/* Disable multi-rate retries when protection is used. */
		ridx[1] = ridx[2] = ridx[3] = ridx[0];
	}
	/* Setup multi-rate retries. */
	for (i = 0; i < 4; i++) {
		series[i].hwrate = athn_rates[ridx[i]].hwrate;
		if (athn_rates[ridx[i]].phy == IEEE80211_T_DS &&
		    ridx[i] != ATHN_RIDX_CCK1 &&
		    (ic->ic_flags & IEEE80211_F_SHPREAMBLE))
			series[i].hwrate |= 0x04;
		series[i].dur = 0;
	}
	if (!(ds->ds_ctl1 & AR_TXC1_NO_ACK)) {
		/* Compute duration for each series. */
		for (i = 0; i < 4; i++) {
			series[i].dur = athn_txtime(sc, IEEE80211_ACK_LEN,
			    athn_rates[ridx[i]].rspridx, ic->ic_flags);
		}
	}

	/* Write number of tries for each series. */
	ds->ds_ctl2 =
	    SM(AR_TXC2_XMIT_DATA_TRIES0, 2) |
	    SM(AR_TXC2_XMIT_DATA_TRIES1, 2) |
	    SM(AR_TXC2_XMIT_DATA_TRIES2, 2) |
	    SM(AR_TXC2_XMIT_DATA_TRIES3, 4);

	/* Tell HW to update duration field in 802.11 header. */
	if (type != AR_FRAME_TYPE_PSPOLL)
		ds->ds_ctl2 |= AR_TXC2_DUR_UPDATE_ENA;

	/* Write Tx rate for each series. */
	ds->ds_ctl3 =
	    SM(AR_TXC3_XMIT_RATE0, series[0].hwrate) |
	    SM(AR_TXC3_XMIT_RATE1, series[1].hwrate) |
	    SM(AR_TXC3_XMIT_RATE2, series[2].hwrate) |
	    SM(AR_TXC3_XMIT_RATE3, series[3].hwrate);

	/* Write duration for each series. */
	ds->ds_ctl4 =
	    SM(AR_TXC4_PACKET_DUR0, series[0].dur) |
	    SM(AR_TXC4_PACKET_DUR1, series[1].dur);
	ds->ds_ctl5 =
	    SM(AR_TXC5_PACKET_DUR2, series[2].dur) |
	    SM(AR_TXC5_PACKET_DUR3, series[3].dur);

	/* Use the same Tx chains for all tries. */
	ds->ds_ctl7 =
	    SM(AR_TXC7_CHAIN_SEL0, sc->txchainmask) |
	    SM(AR_TXC7_CHAIN_SEL1, sc->txchainmask) |
	    SM(AR_TXC7_CHAIN_SEL2, sc->txchainmask) |
	    SM(AR_TXC7_CHAIN_SEL3, sc->txchainmask);
#ifdef notyet
#ifndef IEEE80211_NO_HT
	/* Use the same short GI setting for all tries. */
	if (ic->ic_flags & IEEE80211_F_SHGI)
		ds->ds_ctl7 |= AR_TXC7_GI0123;
	/* Use the same channel width for all tries. */
	if (ic->ic_flags & IEEE80211_F_CBW40)
		ds->ds_ctl7 |= AR_TXC7_2040_0123;
#endif
#endif

	if (ds->ds_ctl0 & (AR_TXC0_RTS_ENABLE | AR_TXC0_CTS_ENABLE)) {
		uint8_t protridx, hwrate;
		uint16_t dur = 0;

		/* Use the same protection mode for all tries. */
		if (ds->ds_ctl0 & AR_TXC0_RTS_ENABLE) {
			ds->ds_ctl4 |= AR_TXC4_RTSCTS_QUAL01;
			ds->ds_ctl5 |= AR_TXC5_RTSCTS_QUAL23;
		}
		/* Select protection rate (suboptimal but ok.) */
		protridx = (ic->ic_curmode == IEEE80211_MODE_11A) ?
		    ATHN_RIDX_OFDM6 : ATHN_RIDX_CCK2;
		if (ds->ds_ctl0 & AR_TXC0_RTS_ENABLE) {
			/* Account for CTS duration. */
			dur += athn_txtime(sc, IEEE80211_ACK_LEN,
			    athn_rates[protridx].rspridx, ic->ic_flags);
		}
		dur += athn_txtime(sc, totlen, ridx[0], ic->ic_flags);
		if (!(ds->ds_ctl1 & AR_TXC1_NO_ACK)) {
			/* Account for ACK duration. */
			dur += athn_txtime(sc, IEEE80211_ACK_LEN,
			    athn_rates[ridx[0]].rspridx, ic->ic_flags);
		}
		/* Write protection frame duration and rate. */
		ds->ds_ctl2 |= SM(AR_TXC2_BURST_DUR, dur);
		hwrate = athn_rates[protridx].hwrate;
		if (protridx == ATHN_RIDX_CCK2 &&
		    (ic->ic_flags & IEEE80211_F_SHPREAMBLE))
			hwrate |= 0x04;
		ds->ds_ctl7 |= SM(AR_TXC7_RTSCTS_RATE, hwrate);
	}

	/* Finalize first Tx descriptor and fill others (if any.) */
	ds->ds_ctl0 |= SM(AR_TXC0_FRAME_LEN, totlen);

	for (i = 0; i < bf->bf_map->dm_nsegs; i++, ds++) {
		ds->ds_data = bf->bf_map->dm_segs[i].ds_addr;
		ds->ds_ctl1 |= SM(AR_TXC1_BUF_LEN,
		    bf->bf_map->dm_segs[i].ds_len);

		if (i != bf->bf_map->dm_nsegs - 1)
			ds->ds_ctl1 |= AR_TXC1_MORE;
		ds->ds_link = 0;

		/* Chain Tx descriptor. */
		if (i != 0)
			lastds->ds_link = bf->bf_daddr + i * sizeof (*ds);
		lastds = ds;
	}
	if (txq->lastds != NULL)
		txq->lastds->ds_link = bf->bf_daddr;
	else
		AR_WRITE(sc, AR_QTXDP(qid), bf->bf_daddr);
	txq->lastds = lastds;
	SIMPLEQ_INSERT_TAIL(&txq->head, bf, bf_list);

	DPRINTFN(6, ("Tx qid=%d nsegs=%d ctl0=0x%x ctl1=0x%x ctl3=0x%x\n",
	    qid, bf->bf_map->dm_nsegs, bf->bf_descs[0].ds_ctl0,
	    bf->bf_descs[0].ds_ctl1, bf->bf_descs[0].ds_ctl3));

	bus_dmamap_sync(sc->sc_dmat, bf->bf_map, 0, bf->bf_map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	/* Kick Tx. */
	AR_WRITE(sc, AR_Q_TXE, 1 << qid);
	return (0);
}

void
athn_init_tx_queues(struct athn_softc *sc)
{
	int qid;

	for (qid = 0; qid < ATHN_QID_COUNT; qid++) {
		SIMPLEQ_INIT(&sc->txq[qid].head);
		sc->txq[qid].lastds = NULL;

		AR_WRITE(sc, AR_DRETRY_LIMIT(qid),
		    SM(AR_D_RETRY_LIMIT_STA_SH, 32) |
		    SM(AR_D_RETRY_LIMIT_STA_LG, 32) |
		    SM(AR_D_RETRY_LIMIT_FR_SH, 10));
		AR_WRITE(sc, AR_QMISC(qid),
		    AR_Q_MISC_DCU_EARLY_TERM_REQ);
		AR_WRITE(sc, AR_DMISC(qid),
		    SM(AR_D_MISC_BKOFF_THRESH, 2) |
		    AR_D_MISC_CW_BKOFF_EN | AR_D_MISC_FRAG_WAIT_EN);
	}

	/* Init beacon queue. */
	AR_SETBITS(sc, AR_QMISC(ATHN_QID_BEACON),
	    AR_Q_MISC_FSP_DBA_GATED | AR_Q_MISC_BEACON_USE |
	    AR_Q_MISC_CBR_INCR_DIS1);
	AR_SETBITS(sc, AR_DMISC(ATHN_QID_BEACON),
	    SM(AR_D_MISC_ARB_LOCKOUT_CNTRL,
	       AR_D_MISC_ARB_LOCKOUT_CNTRL_GLOBAL) |
	    AR_D_MISC_BEACON_USE |
	    AR_D_MISC_POST_FR_BKOFF_DIS);

	/* Init CAB queue. */
	AR_SETBITS(sc, AR_QMISC(ATHN_QID_CAB),
	    AR_Q_MISC_FSP_DBA_GATED | AR_Q_MISC_CBR_INCR_DIS1 |
	    AR_Q_MISC_CBR_INCR_DIS0);
	AR_SETBITS(sc, AR_DMISC(ATHN_QID_CAB),
	    SM(AR_D_MISC_ARB_LOCKOUT_CNTRL,
	       AR_D_MISC_ARB_LOCKOUT_CNTRL_GLOBAL));

	/* Init PS-Poll queue. */
	AR_SETBITS(sc, AR_QMISC(ATHN_QID_PSPOLL),
	    AR_Q_MISC_CBR_INCR_DIS1);

	/* Init UAPSD queue. */
	AR_SETBITS(sc, AR_DMISC(ATHN_QID_UAPSD),
	    AR_D_MISC_POST_FR_BKOFF_DIS);

	/* Enable DESC interrupts for all Tx queues. */
	AR_WRITE(sc, AR_IMR_S0, 0x00ff0000);
	/* Enable EOL interrupts for all Tx queues except UAPSD. */
	AR_WRITE(sc, AR_IMR_S1, 0x00df0000);
}

void
athn_set_viterbi_mask(struct athn_softc *sc, int bin)
{
	uint32_t mask[4], reg;
	uint8_t m[62], p[62];	/* XXX use bit arrays? */
	int i, bit, cur;

	/* Compute pilot mask. */
	cur = -6000;
	for (i = 0; i < 4; i++) {
		mask[i] = 0;
		for (bit = 0; bit < 30; bit++) {
			if (abs(cur - bin) < 100)
				mask[i] |= 1 << bit;
			cur += 100;
		}
		if (cur == 0)	/* Skip entry "0". */
			cur = 100;
	}
	/* Write entries from -6000 to -3100. */
	AR_WRITE(sc, AR_PHY_TIMING7, mask[0]);
	AR_WRITE(sc, AR_PHY_TIMING9, mask[0]);
	/* Write entries from -3000 to -100. */
	AR_WRITE(sc, AR_PHY_TIMING8, mask[1]);
	AR_WRITE(sc, AR_PHY_TIMING10, mask[1]);
	/* Write entries from 100 to 3000. */
	AR_WRITE(sc, AR_PHY_PILOT_MASK_01_30, mask[2]);
	AR_WRITE(sc, AR_PHY_CHANNEL_MASK_01_30, mask[2]);
	/* Write entries from 3100 to 6000. */
	AR_WRITE(sc, AR_PHY_PILOT_MASK_31_60, mask[3]);
	AR_WRITE(sc, AR_PHY_CHANNEL_MASK_31_60, mask[3]);

	/* Compute viterbi mask. */
	for (cur = 6100; cur >= 0; cur -= 100)
		p[+cur / 100] = abs(cur - bin) < 75;
	for (cur = -100; cur >= -6100; cur -= 100)
		m[-cur / 100] = abs(cur - bin) < 75;

	/* Write viterbi mask (XXX needs to be reworked.) */
	reg =
	    m[46] << 30 | m[47] << 28 | m[48] << 26 | m[49] << 24 |
	    m[50] << 22 | m[51] << 20 | m[52] << 18 | m[53] << 16 |
	    m[54] << 14 | m[55] << 12 | m[56] << 10 | m[57] <<  8 |
	    m[58] <<  6 | m[59] <<  4 | m[60] <<  2 | m[61] <<  0;
	AR_WRITE(sc, AR_PHY_BIN_MASK_1, reg);
	AR_WRITE(sc, AR_PHY_VIT_MASK2_M_46_61, reg);

	/* XXX m[48] should be m[38] ? */
	reg =             m[31] << 28 | m[32] << 26 | m[33] << 24 |
	    m[34] << 22 | m[35] << 20 | m[36] << 18 | m[37] << 16 |
	    m[48] << 14 | m[39] << 12 | m[40] << 10 | m[41] <<  8 |
	    m[42] <<  6 | m[43] <<  4 | m[44] <<  2 | m[45] <<  0;
	AR_WRITE(sc, AR_PHY_BIN_MASK_2, reg);
	AR_WRITE(sc, AR_PHY_VIT_MASK2_M_31_45, reg);

	/* XXX This one is weird too. */
	reg =
	    m[16] << 30 | m[16] << 28 | m[18] << 26 | m[18] << 24 |
	    m[20] << 22 | m[20] << 20 | m[22] << 18 | m[22] << 16 |
	    m[24] << 14 | m[24] << 12 | m[25] << 10 | m[26] <<  8 |
	    m[27] <<  6 | m[28] <<  4 | m[29] <<  2 | m[30] <<  0;
	AR_WRITE(sc, AR_PHY_BIN_MASK_3, reg);
	AR_WRITE(sc, AR_PHY_VIT_MASK2_M_16_30, reg);

	reg =
	    m[ 0] << 30 | m[ 1] << 28 | m[ 2] << 26 | m[ 3] << 24 |
	    m[ 4] << 22 | m[ 5] << 20 | m[ 6] << 18 | m[ 7] << 16 |
	    m[ 8] << 14 | m[ 9] << 12 | m[10] << 10 | m[11] <<  8 |
	    m[12] <<  6 | m[13] <<  4 | m[14] <<  2 | m[15] <<  0;
	AR_WRITE(sc, AR_PHY_MASK_CTL, reg);
	AR_WRITE(sc, AR_PHY_VIT_MASK2_M_00_15, reg);

	reg =             p[15] << 28 | p[14] << 26 | p[13] << 24 |
	    p[12] << 22 | p[11] << 20 | p[10] << 18 | p[ 9] << 16 |
	    p[ 8] << 14 | p[ 7] << 12 | p[ 6] << 10 | p[ 5] <<  8 |
	    p[ 4] <<  6 | p[ 3] <<  4 | p[ 2] <<  2 | p[ 1] <<  0;
	AR_WRITE(sc, AR_PHY_BIN_MASK2_1, reg);
	AR_WRITE(sc, AR_PHY_VIT_MASK2_P_15_01, reg);

	reg =             p[30] << 28 | p[29] << 26 | p[28] << 24 |
	    p[27] << 22 | p[26] << 20 | p[25] << 18 | p[24] << 16 |
	    p[23] << 14 | p[22] << 12 | p[21] << 10 | p[20] <<  8 |
	    p[19] <<  6 | p[18] <<  4 | p[17] <<  2 | p[16] <<  0;
	AR_WRITE(sc, AR_PHY_BIN_MASK2_2, reg);
	AR_WRITE(sc, AR_PHY_VIT_MASK2_P_30_16, reg);

	reg =             p[45] << 28 | p[44] << 26 | p[43] << 24 |
	    p[42] << 22 | p[41] << 20 | p[40] << 18 | p[39] << 16 |
	    p[38] << 14 | p[37] << 12 | p[36] << 10 | p[35] <<  8 |
	    p[34] <<  6 | p[33] <<  4 | p[32] <<  2 | p[31] <<  0;
	AR_WRITE(sc, AR_PHY_BIN_MASK2_3, reg);
	AR_WRITE(sc, AR_PHY_VIT_MASK2_P_45_31, reg);

	reg =
	    p[61] << 30 | p[60] << 28 | p[59] << 26 | p[58] << 24 |
	    p[57] << 22 | p[56] << 20 | p[55] << 18 | p[54] << 16 |
	    p[53] << 14 | p[52] << 12 | p[51] << 10 | p[50] <<  8 |
	    p[49] <<  6 | p[48] <<  4 | p[47] <<  2 | p[46] <<  0;
	AR_WRITE(sc, AR_PHY_BIN_MASK2_4, reg);
	AR_WRITE(sc, AR_PHY_VIT_MASK2_P_61_46, reg);
}

void
athn_set_beacon_timers(struct athn_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni = ic->ic_bss;
	uint32_t tsfhi, tsflo, tsftu, reg;
	uint32_t intval, next_tbtt, next_dtim;
	int dtim_period, dtim_count, rem_dtim_count;

	tsfhi = AR_READ(sc, AR_TSF_U32);
	tsflo = AR_READ(sc, AR_TSF_L32);
	tsftu = AR_TSF_TO_TU(tsfhi, tsflo) + AR_FUDGE;

	/* Beacon interval in TU. */
	intval = ni->ni_intval;

	next_tbtt = roundup(tsftu, intval);
#ifdef notyet
	dtim_period = ic->ic_dtim_period;
	if (dtim_period <= 0)
#endif
		dtim_period = 1;	/* Assume all TIMs are DTIMs. */

#ifdef notyet
	dtim_count = ic->ic_dtim_count;
	if (dtim_count >= dtim_period)	/* Should not happen. */
#endif
		dtim_count = 0;	/* Assume last TIM was a DTIM. */

	/* Compute number of remaining TIMs until next DTIM. */
	rem_dtim_count = 0;	/* XXX */
	next_dtim = next_tbtt + rem_dtim_count * intval;

	AR_WRITE(sc, AR_NEXT_TBTT_TIMER, next_tbtt * IEEE80211_DUR_TU);
	AR_WRITE(sc, AR_BEACON_PERIOD, intval * IEEE80211_DUR_TU);
	AR_WRITE(sc, AR_DMA_BEACON_PERIOD, intval * IEEE80211_DUR_TU);

	/*
	 * Set the number of consecutive beacons to miss before raising
	 * a BMISS interrupt to 10.
	 */
	reg = AR_READ(sc, AR_RSSI_THR);
	reg = RW(reg, AR_RSSI_THR_BM_THR, 10);
	AR_WRITE(sc, AR_RSSI_THR, reg);

	AR_WRITE(sc, AR_NEXT_DTIM,
	    (next_dtim - AR_SLEEP_SLOP) * IEEE80211_DUR_TU);
	AR_WRITE(sc, AR_NEXT_TIM,
	    (next_tbtt - AR_SLEEP_SLOP) * IEEE80211_DUR_TU);

	/* CAB timeout is in 1/8 TU. */
	AR_WRITE(sc, AR_SLEEP1,
	    SM(AR_SLEEP1_CAB_TIMEOUT, AR_CAB_TIMEOUT_VAL * 8) |
	    AR_SLEEP1_ASSUME_DTIM);
	AR_WRITE(sc, AR_SLEEP2,
	    SM(AR_SLEEP2_BEACON_TIMEOUT, AR_MIN_BEACON_TIMEOUT_VAL));

	AR_WRITE(sc, AR_TIM_PERIOD, intval * IEEE80211_DUR_TU);
	AR_WRITE(sc, AR_DTIM_PERIOD, dtim_period * intval * IEEE80211_DUR_TU);

	AR_SETBITS(sc, AR_TIMER_MODE,
	    AR_TBTT_TIMER_EN | AR_TIM_TIMER_EN | AR_DTIM_TIMER_EN);

	/* Set TSF out-of-range threshold (fixed at 16k us). */
	AR_WRITE(sc, AR_TSFOOR_THRESHOLD, 0x4240);
}

void
athn_set_rf_mode(struct athn_softc *sc, struct ieee80211_channel *c)
{
	uint32_t reg;

	reg = IEEE80211_IS_CHAN_2GHZ(c) ?
	    AR_PHY_MODE_DYNAMIC : AR_PHY_MODE_OFDM;
	if (!AR_SREV_9280_10_OR_LATER(sc)) {
		reg |= IEEE80211_IS_CHAN_2GHZ(c) ?
		    AR_PHY_MODE_RF2GHZ : AR_PHY_MODE_RF5GHZ;
	} else if (AR_SREV_9280_20(sc) && 0 /* XXX */) {
		reg |= AR_PHY_MODE_DYNAMIC | AR_PHY_MODE_DYN_CCK_DISABLE;
	}
	AR_WRITE(sc, AR_PHY_MODE, reg);
}

void
athn_set_opmode(struct athn_softc *sc)
{
	uint32_t reg;

	switch (sc->sc_ic.ic_opmode) {
#ifndef IEEE80211_STA_ONLY
	case IEEE80211_M_HOSTAP:
		reg = AR_READ(sc, AR_STA_ID1);
		reg &= ~AR_STA_ID1_ADHOC;
		reg |= AR_STA_ID1_STA_AP | AR_STA_ID1_KSRCH_MODE;
		AR_WRITE(sc, AR_STA_ID1, reg);

		AR_CLRBITS(sc, AR_CFG, AR_CFG_AP_ADHOC_INDICATION);
		break;
	case IEEE80211_M_IBSS:
	case IEEE80211_M_AHDEMO:
		reg = AR_READ(sc, AR_STA_ID1);
		reg &= ~AR_STA_ID1_STA_AP;
		reg |= AR_STA_ID1_ADHOC | AR_STA_ID1_KSRCH_MODE;
		AR_WRITE(sc, AR_STA_ID1, reg);

		AR_SETBITS(sc, AR_CFG, AR_CFG_AP_ADHOC_INDICATION);
		break;
#endif
	default:
		reg = AR_READ(sc, AR_STA_ID1);
		reg &= ~(AR_STA_ID1_ADHOC | AR_STA_ID1_STA_AP);
		reg |= AR_STA_ID1_KSRCH_MODE;
		AR_WRITE(sc, AR_STA_ID1, reg);
		break;
	}
}

void
athn_set_bss(struct athn_softc *sc, struct ieee80211_node *ni)
{
	const uint8_t *bssid = ni->ni_bssid;

	AR_WRITE(sc, AR_BSS_ID0, bssid[0] | bssid[1] << 8 |
	    bssid[2] << 16 | bssid[3] << 24);
	AR_WRITE(sc, AR_BSS_ID1, bssid[4] | bssid[5] << 8 |
	    SM(AR_BSS_ID1_AID, IEEE80211_AID(ni->ni_associd)));
}

void
athn_enable_interrupts(struct athn_softc *sc)
{
	uint32_t mask, mask2;

	athn_disable_interrupts(sc);	/* XXX */

	mask = AR_IMR_TXDESC | AR_IMR_TXEOL | AR_IMR_RXERR | AR_IMR_RXEOL |
	    AR_IMR_RXORN | AR_IMR_GENTMR | AR_IMR_BCNMISC | AR_IMR_RXMINTR |
	    AR_IMR_RXINTM;
	AR_WRITE(sc, AR_IMR, mask);

	mask2 = AR_READ(sc, AR_IMR_S2);
	mask2 &= ~(AR_IMR_S2_TIM | AR_IMR_S2_DTIM | AR_IMR_S2_DTIMSYNC |
	    AR_IMR_S2_CABEND | AR_IMR_S2_CABTO | AR_IMR_S2_TSFOOR);
	mask2 |= AR_IMR_S2_GTT | AR_IMR_S2_CST;
	AR_WRITE(sc, AR_IMR_S2, mask2);

	AR_CLRBITS(sc, AR_IMR_S5, AR_IMR_S5_TIM_TIMER);

	AR_WRITE(sc, AR_IER, AR_IER_ENABLE);

	AR_WRITE(sc, AR_INTR_ASYNC_ENABLE, AR_INTR_MAC_IRQ);
	AR_WRITE(sc, AR_INTR_ASYNC_MASK, AR_INTR_MAC_IRQ);

	AR_WRITE(sc, AR_INTR_SYNC_ENABLE, sc->isync);
	AR_WRITE(sc, AR_INTR_SYNC_MASK, sc->isync);
}

void
athn_disable_interrupts(struct athn_softc *sc)
{
	AR_WRITE(sc, AR_IER, 0);
	(void)AR_READ(sc, AR_IER);

	AR_WRITE(sc, AR_INTR_ASYNC_ENABLE, 0);
	(void)AR_READ(sc, AR_INTR_ASYNC_ENABLE);

	AR_WRITE(sc, AR_INTR_SYNC_ENABLE, 0);
	(void)AR_READ(sc, AR_INTR_SYNC_ENABLE);

	AR_WRITE(sc, AR_IMR, 0);

	AR_CLRBITS(sc, AR_IMR_S2, AR_IMR_S2_TIM | AR_IMR_S2_DTIM |
	    AR_IMR_S2_DTIMSYNC | AR_IMR_S2_CABEND | AR_IMR_S2_CABTO |
	    AR_IMR_S2_TSFOOR | AR_IMR_S2_GTT | AR_IMR_S2_CST);

	AR_CLRBITS(sc, AR_IMR_S5, AR_IMR_S5_TIM_TIMER);
}

void
athn_hw_init(struct athn_softc *sc, struct ieee80211_channel *c,
    struct ieee80211_channel *extc)
{
	struct athn_ops *ops = &sc->ops;
	const struct athn_ini *ini = sc->ini;
	const uint32_t *pvals;
	int i;

	AR_WRITE(sc, AR_PHY(0), 0x00000007);
	AR_WRITE(sc, AR_PHY_ADC_SERIAL_CTL, AR_PHY_SEL_EXTERNAL_RADIO);

	if (!AR_SINGLE_CHIP(sc))
		ar5416_reset_addac(sc, c);

	AR_WRITE(sc, AR_PHY_ADC_SERIAL_CTL, AR_PHY_SEL_INTERNAL_ADDAC);

	/* First initialization step (depends on channel band/bandwidth). */
#ifndef IEEE80211_NO_HT
	if (extc != NULL) {
		if (IEEE80211_IS_CHAN_2GHZ(c))
			pvals = ini->vals_2g40;
		else
			pvals = ini->vals_5g40;
	} else
#endif
	{
		if (IEEE80211_IS_CHAN_2GHZ(c))
			pvals = ini->vals_2g20;
		else
			pvals = ini->vals_5g20;
	}
	DPRINTFN(4, ("writing per-mode init vals\n"));
	for (i = 0; i < ini->nregs; i++) {
		AR_WRITE(sc, ini->regs[i], pvals[i]);
		if (AR_IS_ANALOG_REG(ini->regs[i]))
			DELAY(100);
		if ((i & 0x1f) == 0)
			DELAY(1);
	}

	if (AR_SREV_9280_20(sc) || AR_SREV_9287_10_OR_LATER(sc))
		ar9280_reset_rx_gain(sc, c);
	if (AR_SREV_9280_20(sc) || AR_SREV_9285_12(sc) ||
	    AR_SREV_9287_10_OR_LATER(sc))
		ar9280_reset_tx_gain(sc, c);

	/* Second initialization step (common to all channels). */
	DPRINTFN(4, ("writing common init vals\n"));
	for (i = 0; i < ini->ncmregs; i++) {
		AR_WRITE(sc, ini->cmregs[i], ini->cmvals[i]);
		if (AR_IS_ANALOG_REG(ini->cmregs[i]))
			DELAY(100);
		if ((i & 0x1f) == 0)
			DELAY(1);
	}

	if (!AR_SINGLE_CHIP(sc))
		ar5416_reset_bb_gain(sc, c);

	/*
	 * Set the RX_ABORT and RX_DIS bits to prevent frames with corrupted
	 * descriptor status.
	 */
	AR_SETBITS(sc, AR_DIAG_SW, AR_DIAG_RX_DIS | AR_DIAG_RX_ABORT);

	/* Hardware workarounds for occasional Rx data corruption. */
	if (AR_SREV_9287_10_OR_LATER(sc))
		AR_CLRBITS(sc, AR_PCU_MISC_MODE2, AR_PCU_MISC_MODE2_HWWAR1);
	else if (AR_SREV_9280_10_OR_LATER(sc))
		AR_CLRBITS(sc, AR_PCU_MISC_MODE2, AR_PCU_MISC_MODE2_HWWAR1 |
		    AR_PCU_MISC_MODE2_HWWAR2);

	if (AR_SREV_5416_20_OR_LATER(sc) && !AR_SREV_9280_10_OR_LATER(sc)) {
		/* Disable baseband clock gating. */
		AR_WRITE(sc, AR_PHY(651), 0x11);
	}

	athn_set_phy(sc, c, extc);
	athn_init_chains(sc);

	if (sc->flags & ATHN_FLAG_OLPC)
		sc->ops.olpc_init(sc);

	ops->set_txpower(sc, c, extc);

	if (!AR_SINGLE_CHIP(sc))
		ar5416_rf_reset(sc, c);
}

void
athn_init_qos(struct athn_softc *sc)
{
	/* Initialize QoS settings. */
	AR_WRITE(sc, AR_MIC_QOS_CONTROL, 0x100aa);
	AR_WRITE(sc, AR_MIC_QOS_SELECT, 0x3210);
	AR_WRITE(sc, AR_QOS_NO_ACK,
	    SM(AR_QOS_NO_ACK_TWO_BIT, 2) |
	    SM(AR_QOS_NO_ACK_BIT_OFF, 5) |
	    SM(AR_QOS_NO_ACK_BYTE_OFF, 0));
	AR_WRITE(sc, AR_TXOP_X, AR_TXOP_X_VAL);
	/* Initialize TXOP for all TIDs. */
	AR_WRITE(sc, AR_TXOP_0_3,   0xffffffff);
	AR_WRITE(sc, AR_TXOP_4_7,   0xffffffff);
	AR_WRITE(sc, AR_TXOP_8_11,  0xffffffff);
	AR_WRITE(sc, AR_TXOP_12_15, 0xffffffff);
}

int
athn_hw_reset(struct athn_softc *sc, struct ieee80211_channel *c,
    struct ieee80211_channel *extc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct athn_ops *ops = &sc->ops;
	uint32_t reg, def_ant, sta_id1, cfg_led, tsflo, tsfhi;
	int i, error;

	/* XXX not if already awake */
	if ((error = athn_set_power_awake(sc)) != 0) {
		printf("%s: could not wakeup chip\n", sc->sc_dev.dv_xname);
		return (error);
	}

	/* Preserve the antenna on a channel switch. */
	if ((def_ant = AR_READ(sc, AR_DEF_ANTENNA)) == 0)
		def_ant = 1;
	/* Preserve other registers. */
	sta_id1 = AR_READ(sc, AR_STA_ID1) & AR_STA_ID1_BASE_RATE_11B;
	cfg_led = AR_READ(sc, AR_CFG_LED) & (AR_CFG_LED_ASSOC_CTL_M |
	    AR_CFG_LED_MODE_SEL_M | AR_CFG_LED_BLINK_THRESH_SEL_M |
	    AR_CFG_LED_BLINK_SLOW);

	/* Mark PHY as inactive. */
	AR_WRITE(sc, AR_PHY_ACTIVE, AR_PHY_ACTIVE_DIS);

	if (AR_SREV_9280(sc) && (sc->flags & ATHN_FLAG_OLPC)) {
		/* Save TSF before it gets cleared. */
		tsfhi = AR_READ(sc, AR_TSF_U32);
		tsflo = AR_READ(sc, AR_TSF_L32);

		/* NB: RTC reset clears TSF. */
		error = athn_reset_power_on(sc);
	} else
		error = athn_reset(sc, 0);
	if (error != 0) {
		printf("%s: could not reset chip (error=%d)\n",
		    sc->sc_dev.dv_xname);
		return (error);
	}

	/* XXX not if already awake */
	if ((error = athn_set_power_awake(sc)) != 0) {
		printf("%s: could not wakeup chip\n", sc->sc_dev.dv_xname);
		return (error);
	}

	athn_init_pll(sc, c);
	athn_set_rf_mode(sc, c);

	if (sc->flags & ATHN_FLAG_RFSILENT) {
		/* Check that the radio is not disabled by hardware switch. */
		reg = athn_gpio_read(sc, sc->rfsilent_pin);
		if (sc->flags & ATHN_FLAG_RFSILENT_REVERSED)
			reg = !reg;
		if (!reg) {
			printf("%s: radio is disabled by hardware switch\n",
			    sc->sc_dev.dv_xname);
			return (EPERM);
		}
	}
	if (AR_SREV_9280(sc) && (sc->flags & ATHN_FLAG_OLPC)) {
		/* Restore TSF if it got cleared. */
		AR_WRITE(sc, AR_TSF_L32, tsflo);
		AR_WRITE(sc, AR_TSF_U32, tsfhi);
	}

	if (AR_SREV_9280_10_OR_LATER(sc))
		AR_SETBITS(sc, AR_GPIO_INPUT_EN_VAL, AR_GPIO_JTAG_DISABLE);

	if (AR_SREV_9287_12_OR_LATER(sc))
		ar9287_1_2_enable_async_fifo(sc);

	/* Write init values to hardware. */
	athn_hw_init(sc, c, extc);

	/*
	 * Only >=AR9280 2.0 parts are capable of encrypting unicast
	 * management frames using CCMP.
	 */
	if (AR_SREV_9280_20_OR_LATER(sc)) {
		reg = AR_READ(sc, AR_AES_MUTE_MASK1);
		/* Do not mask the subtype field in management frames. */
		reg = RW(reg, AR_AES_MUTE_MASK1_FC0_MGMT, 0xff);
		reg = RW(reg, AR_AES_MUTE_MASK1_FC1_MGMT,
		    ~(IEEE80211_FC1_RETRY | IEEE80211_FC1_PWR_MGT |
		      IEEE80211_FC1_MORE_DATA));
		AR_WRITE(sc, AR_AES_MUTE_MASK1, reg);
	} else if (AR_SREV_9160_10_OR_LATER(sc)) {
		/* Disable hardware crypto for management frames. */
		AR_CLRBITS(sc, AR_PCU_MISC_MODE2,
		    AR_PCU_MISC_MODE2_MGMT_CRYPTO_ENABLE);
		AR_SETBITS(sc, AR_PCU_MISC_MODE2,
		    AR_PCU_MISC_MODE2_NO_CRYPTO_FOR_NON_DATA_PKT);
	}

	if (ic->ic_curmode != IEEE80211_MODE_11B)
		athn_set_delta_slope(sc, c, extc);

	ops->spur_mitigate(sc, c, extc);
	ops->init_from_rom(sc, c, extc);

	AR_WRITE(sc, AR_STA_ID0,
	    ic->ic_myaddr[0] <<  0 | ic->ic_myaddr[1] <<  8 |
	    ic->ic_myaddr[2] << 16 | ic->ic_myaddr[3] << 24);
	/* XXX */
	AR_WRITE(sc, AR_STA_ID1,
	    ic->ic_myaddr[4] <<  0 | ic->ic_myaddr[5] <<  8 |
	    sta_id1 | AR_STA_ID1_RTS_USE_DEF | AR_STA_ID1_CRPT_MIC_ENABLE);

	athn_set_opmode(sc);

	AR_WRITE(sc, AR_BSSMSKL, 0xffffffff);
	AR_WRITE(sc, AR_BSSMSKU, 0xffff);

	/* Restore previous antenna. */
	AR_WRITE(sc, AR_DEF_ANTENNA, def_ant);

	AR_WRITE(sc, AR_BSS_ID0, 0);
	AR_WRITE(sc, AR_BSS_ID1, 0);

	AR_WRITE(sc, AR_ISR, 0xffffffff);

	AR_WRITE(sc, AR_RSSI_THR, SM(AR_RSSI_THR_BM_THR, 7));

	error = ops->set_synth(sc, c, extc);
	if (error != 0) {
		printf("%s: could not set channel\n", sc->sc_dev.dv_xname);
		return (error);
	}

	for (i = 0; i < AR_NUM_DCU; i++)
		AR_WRITE(sc, AR_DQCUMASK(i), 1 << i);

	athn_init_tx_queues(sc);

	/* Initialize interrupt mask. */
	sc->imask = AR_IMR_DEFAULT;
#ifndef IEEE80211_STA_ONLY
	if (ic->ic_opmode == IEEE80211_M_HOSTAP)
		sc->imask |= AR_IMR_MIB;
#endif
	AR_WRITE(sc, AR_IMR, sc->imask);
	AR_SETBITS(sc, AR_IMR_S2, AR_IMR_S2_GTT);
	AR_WRITE(sc, AR_INTR_SYNC_CAUSE, 0xffffffff);
	sc->isync = AR_INTR_SYNC_DEFAULT;
	if (sc->flags & ATHN_FLAG_RFSILENT)
		sc->isync |= AR_INTR_SYNC_GPIO_PIN(sc->rfsilent_pin);
	AR_WRITE(sc, AR_INTR_SYNC_ENABLE, sc->isync);
	AR_WRITE(sc, AR_INTR_SYNC_MASK, 0);

	athn_init_qos(sc);

	if (!AR_SREV_9280_10(sc))
		AR_SETBITS(sc, AR_PCU_MISC, AR_PCU_MIC_NEW_LOC_ENA);

	if (AR_SREV_9287_12_OR_LATER(sc))
		ar9287_1_2_setup_async_fifo(sc);

	/* Disable sequence number generation in hardware. */
	AR_SETBITS(sc, AR_STA_ID1, AR_STA_ID1_PRESERVE_SEQNUM);

	athn_init_dma(sc);

	/* Program OBS bus to see MAC interrupts. */
	AR_WRITE(sc, AR_OBS, 8);

	/* Setup interrupt mitigation. */
	AR_WRITE(sc, AR_RIMT, SM(AR_RIMT_FIRST, 2000) | SM(AR_RIMT_LAST, 500));

	athn_init_baseband(sc);

	if ((error = athn_init_calib(sc, c, extc)) != 0) {
		printf("%s: could not initialize calibration\n",
		    sc->sc_dev.dv_xname);
		return (error);
	}

	if (sc->rxchainmask == 0x3 || sc->rxchainmask == 0x5) {
		/* XXX why again? */
		AR_WRITE(sc, AR_PHY_RX_CHAINMASK,  sc->rxchainmask);
		AR_WRITE(sc, AR_PHY_CAL_CHAINMASK, sc->rxchainmask);
	}

	AR_WRITE(sc, AR_CFG_LED, cfg_led | AR_CFG_SCLK_32KHZ);

#if BYTE_ORDER == BIG_ENDIAN
	/* Default is little-endian, turn on swapping for big-endian. */
	AR_WRITE(sc, AR_CFG, AR_CFG_SWTD | AR_CFG_SWRD);
#endif
	return (0);
}

struct ieee80211_node *
athn_node_alloc(struct ieee80211com *ic)
{
	return (malloc(sizeof (struct athn_node), M_DEVBUF,
	    M_NOWAIT | M_ZERO));
}

void
athn_newassoc(struct ieee80211com *ic, struct ieee80211_node *ni, int isnew)
{
	struct athn_softc *sc = ic->ic_softc;
	struct athn_node *an = (void *)ni;
	struct ieee80211_rateset *rs = &ni->ni_rates;
	uint8_t rate;
	int ridx, i, j;

	ieee80211_amrr_node_init(&sc->amrr, &an->amn);
	/* Start at lowest available bit-rate, AMRR will raise. */
	ni->ni_txrate = 0;

	for (i = 0; i < rs->rs_nrates; i++) {
		rate = rs->rs_rates[i] & IEEE80211_RATE_VAL;

		/* Map 802.11 rate to HW rate index. */
		for (ridx = 0; ridx <= ATHN_RIDX_MAX; ridx++)
			if (athn_rates[ridx].rate == rate)
				break;
		an->ridx[i] = ridx;
		DPRINTFN(2, ("rate %d index %d\n", rate, ridx));

		/* Compute fallback rate for retries. */
		an->fallback[i] = i;
		for (j = i - 1; j >= 0; j--) {
			if (athn_rates[an->ridx[j]].phy ==
			    athn_rates[an->ridx[i]].phy) {
				an->fallback[i] = j;
				break;
			}
		}
		DPRINTFN(2, ("%d fallbacks to %d\n", i, an->fallback[i]));
	}
}

int
athn_media_change(struct ifnet *ifp)
{
	struct athn_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	uint8_t rate, ridx;
	int error;

	error = ieee80211_media_change(ifp);
	if (error != ENETRESET)
		return (error);

	if (ic->ic_fixed_rate != -1) {
		rate = ic->ic_sup_rates[ic->ic_curmode].
		    rs_rates[ic->ic_fixed_rate] & IEEE80211_RATE_VAL;
		/* Map 802.11 rate to HW rate index. */
		for (ridx = 0; ridx <= ATHN_RIDX_MAX; ridx++)
			if (athn_rates[ridx].rate == rate)
				break;
		sc->fixed_ridx = ridx;
	}
	if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) ==
	    (IFF_UP | IFF_RUNNING)) {
		athn_stop(ifp, 0);
		error = athn_init(ifp);
	}
	return (error);
}

void
athn_next_scan(void *arg)
{
	struct athn_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	int s;

	s = splnet();
	if (ic->ic_state == IEEE80211_S_SCAN)
		ieee80211_next_scan(&ic->ic_if);
	splx(s);
}

int
athn_newstate(struct ieee80211com *ic, enum ieee80211_state nstate, int arg)
{
	struct ifnet *ifp = &ic->ic_if;
	struct athn_softc *sc = ifp->if_softc;
	int error;

	timeout_del(&sc->calib_to);
	if (nstate != IEEE80211_S_SCAN)
		athn_gpio_write(sc, sc->led_pin, 1);

	switch (nstate) {
	case IEEE80211_S_INIT:
		break;
	case IEEE80211_S_SCAN:
		/* Make the LED blink while scanning. */
		athn_gpio_write(sc, sc->led_pin,
		    !athn_gpio_read(sc, sc->led_pin));
		error = athn_switch_chan(sc, ic->ic_bss->ni_chan, NULL);
		if (error != 0)
			return (error);
		timeout_add_msec(&sc->scan_to, 200);
		break;
	case IEEE80211_S_AUTH:
		error = athn_switch_chan(sc, ic->ic_bss->ni_chan, NULL);
		if (error != 0)
			return (error);
		break;
	case IEEE80211_S_ASSOC:
		break;
	case IEEE80211_S_RUN:
		athn_gpio_write(sc, sc->led_pin, 0);

		if (ic->ic_opmode == IEEE80211_M_MONITOR)
			break;

		/* Fake a join to initialize the Tx rate. */
		athn_newassoc(ic, ic->ic_bss, 1);

		athn_set_bss(sc, ic->ic_bss);
		athn_disable_interrupts(sc);
		athn_set_beacon_timers(sc);
		/* XXX Enable BMISS interrupts. */
		athn_enable_interrupts(sc);
		/* XXX Start ANI. */

		timeout_add_msec(&sc->calib_to, 500);
		break;
	}

	return (sc->sc_newstate(ic, nstate, arg));
}

void
athn_updateedca(struct ieee80211com *ic)
{
#define ATHN_EXP2(x)	((1 << (x)) - 1)	/* CWmin = 2^ECWmin - 1 */
	struct athn_softc *sc = ic->ic_softc;
	const struct ieee80211_edca_ac_params *ac;
	int aci, qid;

	for (aci = 0; aci < EDCA_NUM_AC; aci++) {
		ac = &ic->ic_edca_ac[aci];
		qid = athn_ac2qid[aci];

		AR_WRITE(sc, AR_DLCL_IFS(qid),
		    SM(AR_D_LCL_IFS_CWMIN, ATHN_EXP2(ac->ac_ecwmin)) |
		    SM(AR_D_LCL_IFS_CWMAX, ATHN_EXP2(ac->ac_ecwmax)) |
		    SM(AR_D_LCL_IFS_AIFS, ac->ac_aifsn));
		if (ac->ac_txoplimit != 0) {
			AR_WRITE(sc, AR_DCHNTIME(qid),
			    SM(AR_D_CHNTIME_DUR,
			       IEEE80211_TXOP_TO_US(ac->ac_txoplimit)) |
			    AR_D_CHNTIME_EN);
		} else
			AR_WRITE(sc, AR_DCHNTIME(qid), 0);
	}
#undef ATHN_EXP2
}

void
athn_updateslot(struct ieee80211com *ic)
{
	struct athn_softc *sc = ic->ic_softc;
	uint32_t clks;

	if (ic->ic_curmode == IEEE80211_MODE_11B)
		clks = AR_CLOCK_RATE_CCK;
	else if (ic->ic_curmode == IEEE80211_MODE_11A)
		clks = AR_CLOCK_RATE_5GHZ_OFDM;
	else
		clks = AR_CLOCK_RATE_2GHZ_OFDM;
	clks *= (ic->ic_flags & IEEE80211_F_SHSLOT) ? 9 : 20;

	/* XXX 40MHz. */

	AR_WRITE(sc, AR_D_GBL_IFS_SLOT, clks);
}

void
athn_start(struct ifnet *ifp)
{
	struct athn_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni;
	struct mbuf *m;

	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

	for (;;) {
		if (SIMPLEQ_EMPTY(&sc->txbufs)) {
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}
		/* Send pending management frames first. */
		IF_DEQUEUE(&ic->ic_mgtq, m);
		if (m != NULL) {
			ni = (void *)m->m_pkthdr.rcvif;
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
		if (athn_tx(sc, m, ni) != 0) {
			ieee80211_release_node(ic, ni);
			ifp->if_oerrors++;
			continue;
		}

		sc->sc_tx_timer = 5;
		ifp->if_timer = 1;
	}
}

void
athn_watchdog(struct ifnet *ifp)
{
	struct athn_softc *sc = ifp->if_softc;

	ifp->if_timer = 0;

	if (sc->sc_tx_timer > 0) {
		if (--sc->sc_tx_timer == 0) {
			printf("%s: device timeout\n", sc->sc_dev.dv_xname);
			athn_stop(ifp, 1);
			(void)athn_init(ifp);
			ifp->if_oerrors++;
			return;
		}
		ifp->if_timer = 1;
	}

	ieee80211_watchdog(ifp);
}

void
athn_set_multi(struct athn_softc *sc)
{
	struct arpcom *ac = &sc->sc_ic.ic_ac;
	struct ifnet *ifp = &ac->ac_if;
	struct ether_multi *enm;
	struct ether_multistep step;
	const uint8_t *addr;
	uint32_t val, lo, hi;
	uint8_t bit;

	if ((ifp->if_flags & (IFF_ALLMULTI | IFF_PROMISC)) != 0) {
		lo = hi = 0xffffffff;
		goto done;
	}
	lo = hi = 0;
	ETHER_FIRST_MULTI(step, ac, enm);
	while (enm != NULL) {
		if (memcmp(enm->enm_addrlo, enm->enm_addrhi, 6) != 0) {
			ifp->if_flags |= IFF_ALLMULTI;
			lo = hi = 0xffffffff;
			goto done;
		}
		addr = enm->enm_addrlo;
		/* Calculate the XOR value of all eight 6-bit words. */
		val = addr[0] | addr[1] << 8 | addr[2] << 16;
		bit  = (val >> 18) ^ (val >> 12) ^ (val >> 6) ^ val;
		val = addr[3] | addr[4] << 8 | addr[5] << 16;
		bit ^= (val >> 18) ^ (val >> 12) ^ (val >> 6) ^ val;
		bit &= 0x3f;
		if (bit < 32)
			lo |= 1 << bit;
		else
			hi |= 1 << (bit - 32);
		ETHER_NEXT_MULTI(step, enm);
	}
 done:
	AR_WRITE(sc, AR_MCAST_FIL0, lo);
	AR_WRITE(sc, AR_MCAST_FIL1, hi);
}

int
athn_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct athn_softc *sc = ifp->if_softc;
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
			if ((ifp->if_flags & IFF_RUNNING) &&
			    ((ifp->if_flags ^ sc->sc_if_flags) &
			     (IFF_ALLMULTI | IFF_PROMISC)) != 0) {
				athn_set_multi(sc);
			} else if (!(ifp->if_flags & IFF_RUNNING))
				error = athn_init(ifp);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				athn_stop(ifp, 1);
		}
		sc->sc_if_flags = ifp->if_flags;
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		ifr = (struct ifreq *)data;
		error = (cmd == SIOCADDMULTI) ?
		    ether_addmulti(ifr, &ic->ic_ac) :
		    ether_delmulti(ifr, &ic->ic_ac);
		if (error == ENETRESET) {
			athn_set_multi(sc);
			error = 0;
		}
		break;

	case SIOCS80211CHANNEL:
		error = ieee80211_ioctl(ifp, cmd, data);
		if (error == ENETRESET &&
		    ic->ic_opmode == IEEE80211_M_MONITOR) {
			if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) ==
			    (IFF_UP | IFF_RUNNING))
				athn_switch_chan(sc, ic->ic_ibss_chan, NULL);
			error = 0;
		}
		break;

	default:
		error = ieee80211_ioctl(ifp, cmd, data);
	}

	if (error == ENETRESET) {
		error = 0;
		if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) ==
		    (IFF_UP | IFF_RUNNING)) {
			athn_stop(ifp, 0);
			error = athn_init(ifp);
		}
	}

	splx(s);
	return (error);
}

int
athn_init(struct ifnet *ifp)
{
	struct athn_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_channel *c, *extc;
	int i, error;

	c = sc->curchan = ic->ic_bss->ni_chan = ic->ic_ibss_chan;
	extc = sc->curchanext = NULL;

	/* In case a new MAC address has been configured. */
	IEEE80211_ADDR_COPY(ic->ic_myaddr, LLADDR(ifp->if_sadl));

	/* For CardBus, power on the socket. */
	if (sc->sc_enable != NULL) {
		if ((error = sc->sc_enable(sc)) != 0) {
			printf("%s: could not enable device\n",
			    sc->sc_dev.dv_xname);
			goto fail;
		}
		if ((error = athn_reset_power_on(sc)) != 0) {
			printf("%s: could not power on device\n",
			    sc->sc_dev.dv_xname);
			goto fail;
		}
	}
	if (!(sc->flags & ATHN_FLAG_PCIE))
		athn_config_nonpcie(sc);
	else
		athn_config_pcie(sc);

	/* Reset HW key cache entries. */
	for (i = 0; i < sc->kc_entries; i++)
		athn_reset_key(sc, i);

	AR_SETBITS(sc, AR_PHY_CCK_DETECT,
	    AR_PHY_CCK_DETECT_BB_ENABLE_ANT_FAST_DIV);

#ifdef ATHN_BT_COEXISTENCE
	/* Configure bluetooth coexistence for combo chips. */
	if (sc->flags & ATHN_FLAG_BTCOEX)
		athn_btcoex_init(sc);
#endif

	/* Configure LED. */
	athn_led_init(sc);

	/* Configure hardware radio switch. */
	if (sc->flags & ATHN_FLAG_RFSILENT)
		athn_rfsilent_init(sc);

	if ((error = athn_hw_reset(sc, c, extc)) != 0) {
		printf("%s: unable to reset hardware; reset status %d\n",
		    sc->sc_dev.dv_xname, error);
		goto fail;
	}

	/* Enable Rx. */
	athn_rx_start(sc);

	/* Enable interrupts. */
	athn_enable_interrupts(sc);

#ifdef ATHN_BT_COEXISTENCE
	/* Enable bluetooth coexistence for combo chips. */
	if (sc->flags & ATHN_FLAG_BTCOEX)
		athn_btcoex_enable(sc);
#endif

	ifp->if_flags &= ~IFF_OACTIVE;
	ifp->if_flags |= IFF_RUNNING;

#ifdef notyet
	if (ic->ic_flags & IEEE80211_F_WEPON) {
		/* Configure WEP keys. */
		for (i = 0; i < IEEE80211_WEP_NKID; i++)
			(void)athn_set_key(ic, NULL, &ic->ic_nw_keys[i]);
	}
#endif
	if (ic->ic_opmode == IEEE80211_M_MONITOR)
		ieee80211_new_state(ic, IEEE80211_S_RUN, -1);
	else
		ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);

	return (0);
 fail:
	athn_stop(ifp, 1);
	return (error);
}

void
athn_stop(struct ifnet *ifp, int disable)
{
	struct athn_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	int qid;

	ifp->if_timer = sc->sc_tx_timer = 0;
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	timeout_del(&sc->scan_to);
	/* In case we were scanning, release the scan "lock". */
	ic->ic_scan_lock = IEEE80211_SCAN_UNLOCKED;

	ieee80211_new_state(ic, IEEE80211_S_INIT, -1);

#ifdef ATHN_BT_COEXISTENCE
	/* Disable bluetooth coexistence for combo chips. */
	if (sc->flags & ATHN_FLAG_BTCOEX)
		athn_btcoex_disable(sc);
#endif

	/* Disable interrupts. */
	athn_disable_interrupts(sc);
	/* Acknowledge interrupts (avoids interrupt storms.) */
	AR_WRITE(sc, AR_INTR_SYNC_CAUSE, 0xffffffff);
	AR_WRITE(sc, AR_INTR_SYNC_MASK, 0);

	for (qid = 0; qid < ATHN_QID_COUNT; qid++)
		athn_stop_tx_dma(sc, qid);
	/* XXX call athn_hw_reset if Tx still pending? */
	for (qid = 0; qid < ATHN_QID_COUNT; qid++)
		athn_tx_reclaim(sc, qid);

	/* Stop Rx. */
	AR_SETBITS(sc, AR_DIAG_SW, AR_DIAG_RX_DIS);
	AR_WRITE(sc, AR_MIBC, AR_MIBC_FMC);
	AR_WRITE(sc, AR_MIBC, AR_MIBC_CMC);
	AR_WRITE(sc, AR_FILT_OFDM, 0);
	AR_WRITE(sc, AR_FILT_CCK, 0);
	athn_set_rxfilter(sc, 0);
	athn_stop_rx_dma(sc);

	athn_reset(sc, 0);
	athn_init_pll(sc, NULL);
	athn_set_power_awake(sc);
	athn_reset(sc, 1);
	athn_init_pll(sc, NULL);

	athn_set_power_sleep(sc);

	/* For CardBus, power down the socket. */
	if (disable && sc->sc_disable != NULL)
		sc->sc_disable(sc);
}
