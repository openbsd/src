/*	$OpenBSD: ral.c,v 1.1 2005/02/15 20:51:20 damien Exp $  */

/*-
 * Copyright (c) 2005
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

/*-
 * Ralink Technology RT2500 chipset driver
 * http://www.ralinktech.com/
 *
 * The chipset includes a RT2525 Radio Transceiver and a RT2560 MAC/BBP.
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
#include <sys/timeout.h>
#include <sys/conf.h>

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
#include <net80211/ieee80211_rssadapt.h>
#include <net80211/ieee80211_radiotap.h>

#include <dev/ic/ralreg.h>
#include <dev/ic/ralvar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#define RAL_DEBUG

#ifdef RAL_DEBUG
#define DPRINTF(x)	if (ral_debug > 0) printf x
#define DPRINTFN(n, x)	if (ral_debug >= (n)) printf x
int ral_debug = 14;
#else
#define DPRINTF(x)
#define DPRINTFN(n, x)
#endif

static const struct ieee80211_rateset ral_rateset_11a =
	{ 8, { 12, 18, 24, 36, 48, 72, 96, 108 } };

static const struct ieee80211_rateset ral_rateset_11b =
	{ 4, { 2, 4, 11, 22 } };

static const struct ieee80211_rateset ral_rateset_11g =
	{ 12, { 2, 4, 11, 22, 12, 18, 24, 36, 48, 72, 96, 108 } };

int	ral_alloc_tx_ring(struct ral_softc *, struct ral_tx_ring *, int);
void	ral_reset_tx_ring(struct ral_softc *, struct ral_tx_ring *);
void	ral_free_tx_ring(struct ral_softc *, struct ral_tx_ring *);
int	ral_alloc_rx_ring(struct ral_softc *, struct ral_rx_ring *, int);
void	ral_reset_rx_ring(struct ral_softc *, struct ral_rx_ring *);
void	ral_free_rx_ring(struct ral_softc *, struct ral_rx_ring *);
int	ral_media_change(struct ifnet *);
void	ral_next_scan(void *);
void	ral_rssadapt_updatestats(void *);
int	ral_newstate(struct ieee80211com *, enum ieee80211_state, int);
uint16_t	ral_eeprom_read(struct ral_softc *, uint8_t);
void	ral_encryption_intr(struct ral_softc *);
void	ral_tx_intr(struct ral_softc *);
void	ral_prio_intr(struct ral_softc *);
void	ral_decryption_intr(struct ral_softc *);
void	ral_rx_intr(struct ral_softc *);
void	ral_beacon_expire(struct ral_softc *);
void	ral_wakeup_expire(struct ral_softc *);
uint8_t	ral_plcp_signal(int);
#if 0
int	ral_tx_bcn(struct ral_softc *, struct mbuf *, struct ieee80211_node *);
#endif
int	ral_tx_mgt(struct ral_softc *, struct mbuf *, struct ieee80211_node *);
int	ral_tx_data(struct ral_softc *, struct mbuf *, struct ieee80211_node *);
void	ral_start(struct ifnet *);
void	ral_watchdog(struct ifnet *);
int	ral_ioctl(struct ifnet *, u_long, caddr_t);
void	ral_bbp_write(struct ral_softc *, uint8_t, uint8_t);
uint8_t	ral_bbp_read(struct ral_softc *, uint8_t);
void	ral_rf_write(struct ral_softc *, uint8_t, uint32_t);
void	ral_set_chan(struct ral_softc *, struct ieee80211_channel *);
void	ral_disable_rf_tune(struct ral_softc *);
void	ral_enable_tsf_sync(struct ral_softc *);
void	ral_update_plcp(struct ral_softc *);
void	ral_update_led(struct ral_softc *, int, int);
void	ral_set_bssid(struct ral_softc *, uint8_t *);
void	ral_set_macaddr(struct ral_softc *, uint8_t *);
int	ral_read_eeprom(struct ral_softc *);
int	ral_bbp_init(struct ral_softc *);
int	ral_init(struct ifnet *);
void	ral_stop(struct ifnet *, int);

/*
 * Default values for MAC registers; values taken from the reference driver.
 */
static const struct {
	uint32_t	reg;
	uint32_t	val;
} ral_def_mac[] = {
	{ RAL_PSCSR0,    0x00020002 },
	{ RAL_PSCSR1,    0x00000002 },
	{ RAL_PSCSR2,    0x00020002 },
	{ RAL_PSCSR3,    0x00000002 },
	{ RAL_TIMECSR,   0x00003f21 },
	{ RAL_CSR9,      0x00000780 },
	{ RAL_CSR11,     0x07041483 },
	{ RAL_CSR18,     0x00140000 },
	{ RAL_CSR19,     0x016C0028 },
	{ RAL_CNT3,      0x00000000 },
	{ RAL_TXCSR1,    0x07614562 },
	{ RAL_TXCSR8,    0x8c8d8b8a },
	{ RAL_ARTCSR0,   0x7038140a },
	{ RAL_ARTCSR1,   0x1d21252d },
	{ RAL_ARTCSR2,   0x1919191d },
	{ RAL_RXCSR0,    0xffffffff },
	{ RAL_RXCSR3,    0xb3aab3af },
	{ RAL_PCICSR,    0x000003b8 },
	{ RAL_PWRCSR0,   0x3f3b3100 },
	{ RAL_GPIOCSR,   0x0000ff00 },
	{ RAL_TESTCSR,   0x000000f0 },
	{ RAL_PWRCSR1,   0x000001ff },
	{ RAL_MACCSR0,   0x00213223 },
	{ RAL_MACCSR1,   0x00235518 },
	{ RAL_MACCSR2,   0x00000040 },
	{ RAL_RALINKCSR, 0x9a009a11 },
	{ RAL_CSR7,      0xffffffff },
	{ RAL_BBPCSR1,   0x82188200 },
	{ RAL_TXACKCSR0, 0x00000020 },
	{ RAL_SECCSR3,   0x0000e78f }
};

/*
 * Default values for BBP registers; values taken from the reference driver.
 */
static const struct {
	uint8_t	reg;
	uint8_t	val;
} ral_def_bbp[] = {
	{  3, 0x02 },
	{  4, 0x19 },
	{ 14, 0x1c },
	{ 15, 0x30 },
	{ 16, 0xac },
	{ 17, 0x48 },
	{ 18, 0x18 },
	{ 19, 0xff },
	{ 20, 0x1e },
	{ 21, 0x08 },
	{ 22, 0x08 },
	{ 23, 0x08 },
	{ 24, 0x70 },
	{ 25, 0x40 },
	{ 26, 0x08 },
	{ 27, 0x23 },
	{ 30, 0x10 },
	{ 31, 0x2b },
	{ 32, 0xb9 },
	{ 34, 0x12 },
	{ 35, 0x50 },
	{ 39, 0xc4 },
	{ 40, 0x02 },
	{ 41, 0x60 },
	{ 53, 0x10 },
	{ 54, 0x18 },
	{ 56, 0x08 },
	{ 57, 0x10 },
	{ 58, 0x08 },
	{ 61, 0x6d },
	{ 62, 0x10 }
};

/*
 * Default values for RF register R2 indexed by channel numbers; values taken
 * from the reference driver.
 */
static const uint32_t ral_rf2522_r2[] = {
	0x307f6, 0x307fb, 0x30800, 0x30805, 0x3080a, 0x3080f, 0x30814,
	0x30819, 0x3081e, 0x30823, 0x30828, 0x3082d, 0x30832, 0x3083e
};

static const uint32_t ral_rf2523_r2[] = {
	0x00327, 0x00328, 0x00329, 0x0032a, 0x0032b, 0x0032c, 0x0032d,
	0x0032e, 0x0032f, 0x00340, 0x00341, 0x00342, 0x00343, 0x00346
};

static const uint32_t ral_rf2524_r2[] = {
	0x00327, 0x00328, 0x00329, 0x0032a, 0x0032b, 0x0032c, 0x0032d,
	0x0032e, 0x0032f, 0x00340, 0x00341, 0x00342, 0x00343, 0x00346
};

static const uint32_t ral_rf2525_r2[] = {
	0x20327, 0x20328, 0x20329, 0x2032a, 0x2032b, 0x2032c, 0x2032d,
	0x2032e, 0x2032f, 0x20340, 0x20341, 0x20342, 0x20343, 0x20346
};

static const uint32_t ral_rf2525_hi_r2[] = {
	0x2032f, 0x20340, 0x20341, 0x20342, 0x20343, 0x20344, 0x20345,
	0x20346, 0x20347, 0x20348, 0x20349, 0x2034a, 0x2034b, 0x2034e
};

static const uint32_t ral_rf2525e_r2[] = {
	0x2044d, 0x2044e, 0x2044f, 0x20460, 0x20461, 0x20462, 0x20463,
	0x20464, 0x20465, 0x20466, 0x20467, 0x20468, 0x20469, 0x2046b
};

/*
 * For dual-band RF, RF registers R1 and R4 also depend on channel number;
 * values taken from the reference driver.
 */
static const struct {
	uint8_t		chan;
	uint32_t	r1;
	uint32_t	r2;
	uint32_t	r4;
} ral_rf5222[] = {
	/* channels on the 2.4GHz band */
	{   1, 0x08808, 0x0044d, 0x00282 },
	{   2, 0x08808, 0x0044e, 0x00282 },
	{   3, 0x08808, 0x0044f, 0x00282 },
	{   4, 0x08808, 0x00460, 0x00282 },
	{   5, 0x08808, 0x00461, 0x00282 },
	{   6, 0x08808, 0x00462, 0x00282 },
	{   7, 0x08808, 0x00463, 0x00282 },
	{   8, 0x08808, 0x00464, 0x00282 },
	{   9, 0x08808, 0x00465, 0x00282 },
	{  10, 0x08808, 0x00466, 0x00282 },
	{  11, 0x08808, 0x00467, 0x00282 },
	{  12, 0x08808, 0x00468, 0x00282 },
	{  13, 0x08808, 0x00469, 0x00282 },
	{  14, 0x08808, 0x0046b, 0x00286 },

	/* channels on the 5.2GHz band */
	{  36, 0x08804, 0x06225, 0x00287 },
	{  40, 0x08804, 0x06226, 0x00287 },
	{  44, 0x08804, 0x06227, 0x00287 },
	{  48, 0x08804, 0x06228, 0x00287 },
	{  52, 0x08804, 0x06229, 0x00287 },
	{  56, 0x08804, 0x0622a, 0x00287 },
	{  60, 0x08804, 0x0622b, 0x00287 },
	{  64, 0x08804, 0x0622c, 0x00287 },

	{ 100, 0x08804, 0x02200, 0x00283 },
	{ 104, 0x08804, 0x02201, 0x00283 },
	{ 108, 0x08804, 0x02202, 0x00283 },
	{ 112, 0x08804, 0x02203, 0x00283 },
	{ 116, 0x08804, 0x02204, 0x00283 },
	{ 120, 0x08804, 0x02205, 0x00283 },
	{ 124, 0x08804, 0x02206, 0x00283 },
	{ 128, 0x08804, 0x02207, 0x00283 },
	{ 132, 0x08804, 0x02208, 0x00283 },
	{ 136, 0x08804, 0x02209, 0x00283 },
	{ 140, 0x08804, 0x0220a, 0x00283 },

	{ 149, 0x08808, 0x02429, 0x00281 },
	{ 153, 0x08808, 0x0242b, 0x00281 },
	{ 157, 0x08808, 0x0242d, 0x00281 },
	{ 161, 0x08808, 0x0242f, 0x00281 }
};

int
ral_attach(struct ral_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	uint32_t val;
	int i;

	timeout_set(&sc->scan_ch, ral_next_scan, sc);
	timeout_set(&sc->rssadapt_ch, ral_rssadapt_updatestats, sc);

	/* retrieve RT2560 revision */
	sc->asic_rev = RAL_READ(sc, RAL_CSR0);

	if (ral_read_eeprom(sc) != 0) {
		printf(": could not read EEPROM\n");
		goto fail;
	}

	/*
	 * Allocate Tx and Rx rings.
	 */
	if (ral_alloc_tx_ring(sc, &sc->txq, RAL_TX_RING_COUNT) != 0) {
		printf(": could not allocate Tx ring\n");
		goto fail;
	}

	if (ral_alloc_tx_ring(sc, &sc->atimq, RAL_ATIM_RING_COUNT) != 0) {
		printf(": could not allocate ATIM ring\n");
		goto fail;
	}

	if (ral_alloc_tx_ring(sc, &sc->prioq, RAL_PRIO_RING_COUNT) != 0) {
		printf(": could not allocate Prio ring\n");
		goto fail;
	}

	if (ral_alloc_tx_ring(sc, &sc->bcnq, RAL_BEACON_RING_COUNT) != 0) {
		printf(": could not allocate Beacon ring\n");
		goto fail;
	}

	if (ral_alloc_rx_ring(sc, &sc->rxq, RAL_RX_RING_COUNT) != 0) {
		printf(": could not allocate Rx ring\n");
		goto fail;
	}

	ic->ic_phytype = IEEE80211_T_OFDM; /* not only, but not used */
	ic->ic_opmode = IEEE80211_M_STA; /* default to BSS mode */
	ic->ic_state = IEEE80211_S_INIT;

	/* set device capabilities */
	ic->ic_caps = IEEE80211_C_MONITOR | IEEE80211_C_IBSS |
	    IEEE80211_C_SHPREAMBLE | IEEE80211_C_PMGT | IEEE80211_C_TXPMGT |
	    IEEE80211_C_WEP;

	/* read MAC address */
	val = RAL_READ(sc, RAL_CSR3);
	ic->ic_myaddr[0] = val & 0xff;
	ic->ic_myaddr[1] = (val >>  8) & 0xff;
	ic->ic_myaddr[2] = (val >> 16) & 0xff;
	ic->ic_myaddr[3] = (val >> 24) & 0xff;
	val = RAL_READ(sc, RAL_CSR4);
	ic->ic_myaddr[4] = val & 0xff;
	ic->ic_myaddr[5] = (val >> 8) & 0xff;

	printf(", address %s\n", ether_sprintf(ic->ic_myaddr));

	if (sc->rf_rev == RAL_RF_5222) {
		/* set supported .11a rates */
		ic->ic_sup_rates[IEEE80211_MODE_11A] = ral_rateset_11a;

		/* set supported .11a channels */
		for (i = 36; i <= 64; i += 4) {
			ic->ic_channels[i].ic_freq =
			    ieee80211_ieee2mhz(i, IEEE80211_CHAN_5GHZ);
			ic->ic_channels[i].ic_flags = IEEE80211_CHAN_A;
		}
		for (i = 100; i <= 140; i += 4) {
			ic->ic_channels[i].ic_freq =
			    ieee80211_ieee2mhz(i, IEEE80211_CHAN_5GHZ);
			ic->ic_channels[i].ic_flags = IEEE80211_CHAN_A;
		}
		for (i = 149; i <= 161; i += 4) {
			ic->ic_channels[i].ic_freq =
			    ieee80211_ieee2mhz(i, IEEE80211_CHAN_5GHZ);
			ic->ic_channels[i].ic_flags = IEEE80211_CHAN_A;
		}
	}

	/* set supported .11b and .11g rates */
	ic->ic_sup_rates[IEEE80211_MODE_11B] = ral_rateset_11b;
	ic->ic_sup_rates[IEEE80211_MODE_11G] = ral_rateset_11g;

	/* set supported .11b and .11g channels (1 through 14) */
	for (i = 1; i <= 11; i++) {
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
	ifp->if_init = ral_init;
	ifp->if_ioctl = ral_ioctl;
	ifp->if_start = ral_start;
	ifp->if_watchdog = ral_watchdog;
	IFQ_SET_READY(&ifp->if_snd);
	memcpy(ifp->if_xname, sc->sc_dev.dv_xname, IFNAMSIZ);

	if_attach(ifp);
	ieee80211_ifattach(ifp);
	/* override state transition machine */
	sc->sc_newstate = ic->ic_newstate;
	ic->ic_newstate = ral_newstate;
	ieee80211_media_init(ifp, ral_media_change, ieee80211_media_status);

#if NBPFILTER > 0
	bpfattach(&sc->sc_drvbpf, ifp, DLT_IEEE802_11_RADIO,
	    sizeof (struct ieee80211_frame) + 64);

	sc->sc_rxtap_len = sizeof sc->sc_rxtapu;
	sc->sc_rxtap.wr_ihdr.it_len = htole16(sc->sc_rxtap_len);
	sc->sc_rxtap.wr_ihdr.it_present = htole32(RAL_RX_RADIOTAP_PRESENT);

	sc->sc_txtap_len = sizeof sc->sc_txtapu;
	sc->sc_txtap.wt_ihdr.it_len = htole16(sc->sc_txtap_len);
	sc->sc_txtap.wt_ihdr.it_present = htole32(RAL_TX_RADIOTAP_PRESENT);
#endif

	return 0;

fail:	/*ral_detach(sc);*/
	return ENXIO;
}

int
ral_detach(struct ral_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ic.ic_if;

	timeout_del(&sc->scan_ch);
	timeout_del(&sc->rssadapt_ch);

#if NBPFILTER > 0
	bpfdetach(ifp);
#endif
	ieee80211_ifdetach(ifp);
	if_detach(ifp);

	ral_free_tx_ring(sc, &sc->txq);
	ral_free_tx_ring(sc, &sc->prioq);
	ral_free_tx_ring(sc, &sc->atimq);
	ral_free_tx_ring(sc, &sc->bcnq);
	ral_free_rx_ring(sc, &sc->rxq);

	return 0;
}

int
ral_alloc_tx_ring(struct ral_softc *sc, struct ral_tx_ring *ring, int count)
{
	int i, nsegs, error;

	ring->count = count;
	ring->queued = 0;
	ring->cur = ring->next = 0;
	ring->cur_encrypt = ring->next_encrypt = 0;

	error = bus_dmamap_create(sc->sc_dmat, count * RAL_TX_DESC_SIZE, 1,
	    count * RAL_TX_DESC_SIZE, 0, BUS_DMA_NOWAIT, &ring->map);
	if (error != 0) {
		printf("%s: could not create desc DMA map\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	error = bus_dmamem_alloc(sc->sc_dmat, count * RAL_TX_DESC_SIZE,
	    PAGE_SIZE, 0, &ring->seg, 1, &nsegs, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not allocate DMA memory\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	error = bus_dmamem_map(sc->sc_dmat, &ring->seg, nsegs,
	    count * RAL_TX_DESC_SIZE, (caddr_t *)&ring->desc, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not map desc DMA memory\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	error = bus_dmamap_load(sc->sc_dmat, ring->map, ring->desc,
	    count * RAL_TX_DESC_SIZE, NULL, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not load desc DMA map\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	memset(ring->desc, 0, count * RAL_TX_DESC_SIZE);
	ring->physaddr = ring->map->dm_segs->ds_addr;

	ring->data = malloc(count * sizeof (struct ral_tx_data), M_DEVBUF,
	    M_NOWAIT);
	if (ring->data == NULL) {
		printf("%s: could not allocate soft data\n",
		    sc->sc_dev.dv_xname);
		error = ENOMEM;
		goto fail;
	}

	for (i = 0; i < count; i++) {
		error = bus_dmamap_create(sc->sc_dmat, MCLBYTES,
		    RAL_MAX_SCATTER, MCLBYTES, 0, BUS_DMA_NOWAIT,
		    &ring->data[i].map);
		if (error != 0) {
			printf("%s: could not create DMA map\n",
			    sc->sc_dev.dv_xname);
			goto fail;
		}
	}

	return 0;

fail:	ral_free_tx_ring(sc, ring);
	return error;
}

void
ral_reset_tx_ring(struct ral_softc *sc, struct ral_tx_ring *ring)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ral_tx_desc *desc;
	struct ral_tx_data *data;
	int i;

	for (i = 0; i < ring->count; i++) {
		desc = &ring->desc[i];
		data = &ring->data[i];

		if (data->m != NULL) {
			bus_dmamap_sync(sc->sc_dmat, data->map, 0,
			    data->map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->sc_dmat, data->map);
			m_freem(data->m);
			data->m = NULL;
		}

		if (data->ni != NULL && data->ni != ic->ic_bss) {
			ieee80211_free_node(ic, data->ni);
			data->ni = NULL;
		}

		desc->flags = 0;
	}

	bus_dmamap_sync(sc->sc_dmat, ring->map, 0, ring->map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	ring->queued = 0;
	ring->cur = ring->next = 0;
	ring->cur_encrypt = ring->next_encrypt = 0;
}

void
ral_free_tx_ring(struct ral_softc *sc, struct ral_tx_ring *ring)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ral_tx_data *data;
	int i;

	if (ring->desc != NULL) {
		bus_dmamap_sync(sc->sc_dmat, ring->map, 0,
		    ring->map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, ring->map);
		bus_dmamem_free(sc->sc_dmat, &ring->seg, 1);
	}

	if (ring->data != NULL) {
		for (i = 0; i < ring->count; i++) {
			data = &ring->data[i];

			if (data->m != NULL) {
				bus_dmamap_sync(sc->sc_dmat, data->map, 0,
				    data->map->dm_mapsize,
				    BUS_DMASYNC_POSTWRITE);
				bus_dmamap_unload(sc->sc_dmat, data->map);
				m_freem(data->m);
			}

			if (data->ni != NULL && data->ni != ic->ic_bss)
				ieee80211_free_node(ic, data->ni);

			if (data->map != NULL)
				bus_dmamap_destroy(sc->sc_dmat, data->map);
		}
		free(ring->data, M_DEVBUF);
	}
}

int
ral_alloc_rx_ring(struct ral_softc *sc, struct ral_rx_ring *ring, int count)
{
	struct ral_rx_desc *desc;
	struct ral_rx_data *data;
	int i, nsegs, error;

	ring->count = count;
	ring->cur = ring->next = 0;
	ring->cur_decrypt = 0;

	error = bus_dmamap_create(sc->sc_dmat, count * RAL_RX_DESC_SIZE, 1,
	    count * RAL_RX_DESC_SIZE, 0, BUS_DMA_NOWAIT, &ring->map);
	if (error != 0) {
		printf("%s: could not create desc DMA map\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	error = bus_dmamem_alloc(sc->sc_dmat, count * RAL_RX_DESC_SIZE,
	    PAGE_SIZE, 0, &ring->seg, 1, &nsegs, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not allocate DMA memory\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	error = bus_dmamem_map(sc->sc_dmat, &ring->seg, nsegs,
	    count * RAL_RX_DESC_SIZE, (caddr_t *)&ring->desc, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not map desc DMA memory\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	error = bus_dmamap_load(sc->sc_dmat, ring->map, ring->desc,
	    count * RAL_RX_DESC_SIZE, NULL, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not load desc DMA map\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	memset(ring->desc, 0, count * RAL_RX_DESC_SIZE);
	ring->physaddr = ring->map->dm_segs->ds_addr;

	ring->data = malloc(count * sizeof (struct ral_rx_data), M_DEVBUF,
	    M_NOWAIT);
	if (ring->data == NULL) {
		printf("%s: could not allocate soft data\n",
		    sc->sc_dev.dv_xname);
		error = ENOMEM;
		goto fail;
	}

	/*
	 * Pre-allocate Rx buffers and populate Rx ring
	 */
	for (i = 0; i < count; i++) {
		desc = &sc->rxq.desc[i];
		data = &sc->rxq.data[i];

		error = bus_dmamap_create(sc->sc_dmat, MCLBYTES, 1, MCLBYTES,
		    0, BUS_DMA_NOWAIT, &data->map);
		if (error != 0) {
			printf("%s: could not create DMA map\n",
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
			printf("%s: could not allocate rx mbuf cluster\n",
			    sc->sc_dev.dv_xname);
			error = ENOMEM;
			goto fail;
		}

		error = bus_dmamap_load(sc->sc_dmat, data->map,
		    mtod(data->m, void *), MCLBYTES, NULL, BUS_DMA_NOWAIT);
		if (error != 0) {
			printf("%s: could not load rx buf DMA map",
			    sc->sc_dev.dv_xname);
			goto fail;
		}

		desc->flags = htole32(RAL_RX_BUSY);
		desc->physaddr = htole32(data->map->dm_segs->ds_addr);
	}

	bus_dmamap_sync(sc->sc_dmat, ring->map, 0, ring->map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	return 0;

fail:	ral_free_rx_ring(sc, ring);
	return error;
}

void
ral_reset_rx_ring(struct ral_softc *sc, struct ral_rx_ring *ring)
{
	int i;

	for (i = 0; i < ring->count; i++) {
		ring->desc[i].flags = htole32(RAL_RX_BUSY);
		ring->data[i].drop = 0;
	}

	bus_dmamap_sync(sc->sc_dmat, ring->map, 0, ring->map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	ring->cur = ring->next = 0;
	ring->cur_decrypt = 0;
}

void
ral_free_rx_ring(struct ral_softc *sc, struct ral_rx_ring *ring)
{
	struct ral_rx_data *data;
	int i;

	if (ring->desc != NULL) {
		bus_dmamap_sync(sc->sc_dmat, ring->map, 0,
		    ring->map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, ring->map);
		bus_dmamem_free(sc->sc_dmat, &ring->seg, 1);
	}

	if (ring->data != NULL) {
		for (i = 0; i < ring->count; i++) {
			data = &ring->data[i];

			if (data->m != NULL) {
				bus_dmamap_sync(sc->sc_dmat, data->map, 0,
				    data->map->dm_mapsize,
				    BUS_DMASYNC_POSTREAD);
				bus_dmamap_unload(sc->sc_dmat, data->map);
				m_freem(data->m);
			}

			if (data->map != NULL)
				bus_dmamap_destroy(sc->sc_dmat, data->map);
		}
		free(ring->data, M_DEVBUF);
	}
}

int
ral_media_change(struct ifnet *ifp)
{
	int error;

	error = ieee80211_media_change(ifp);
	if (error != ENETRESET)
		return error;

	if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) == (IFF_UP | IFF_RUNNING))
		ral_init(ifp);

	return 0;
}

/*
 * This function is called periodically (every 200ms) during scanning to
 * switch from one channel to another.
 */
void
ral_next_scan(void *arg)
{
	struct ral_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;

	if (ic->ic_state == IEEE80211_S_SCAN)
		ieee80211_next_scan(ifp);
}

/*
 * This function is called periodically (every 100ms) in RUN state to update
 * the rate adaptation statistics.
 */
void
ral_rssadapt_updatestats(void *arg)
{
	struct ral_softc *sc = arg;

	ieee80211_rssadapt_updatestats(&sc->rssadapt);

	timeout_add(&sc->rssadapt_ch, hz / 10);
}

int
ral_newstate(struct ieee80211com *ic, enum ieee80211_state nstate, int arg)
{
	struct ral_softc *sc = ic->ic_if.if_softc;
        enum ieee80211_state ostate;

	ostate = ic->ic_state;
	timeout_del(&sc->scan_ch);

	switch (nstate) {
	case IEEE80211_S_INIT:
		timeout_del(&sc->rssadapt_ch);

		switch (ostate) {
		case IEEE80211_S_RUN:
			/* abort TSF synchronization */
			RAL_WRITE(sc, RAL_CSR14, 0);

			/* turn activity led off */
			if (sc->led_mode != RAL_LED_MODE_SINGLE)
				ral_update_led(sc, 0, 0);

			sc->sc_newstate(ic, nstate, arg);
			break;

		default:
			sc->sc_newstate(ic, nstate, arg);
		}
		break;

	case IEEE80211_S_SCAN:
		ral_set_chan(sc, ic->ic_bss->ni_chan);
		timeout_add(&sc->scan_ch, hz / 5);
		sc->sc_newstate(ic, nstate, arg);
		break;

	case IEEE80211_S_AUTH:
		ral_set_chan(sc, ic->ic_bss->ni_chan);
		sc->sc_newstate(ic, nstate, arg);
		break;

	case IEEE80211_S_ASSOC:
		sc->sc_newstate(ic, nstate, arg);
		break;

	case IEEE80211_S_RUN:
		ral_set_bssid(sc, ic->ic_bss->ni_bssid);

		ral_enable_tsf_sync(sc);

		if (sc->led_mode != RAL_LED_MODE_SINGLE)
			ral_update_led(sc, 1, 0);

		timeout_add(&sc->rssadapt_ch, hz / 10);

		sc->sc_newstate(ic, nstate, arg);
		break;
	}

	return 0;
}

/*
 * Read 16 bits at address 'addr' from the serial EEPROM (either 93C46 or
 * 93C66).
 */
uint16_t
ral_eeprom_read(struct ral_softc *sc, uint8_t addr)
{
	uint32_t tmp;
	uint16_t val;
	int n;

	/* Clock C once before the first command */
	RAL_EEPROM_CTL(sc, 0);

	RAL_EEPROM_CTL(sc, RAL_EEPROM_S);
	RAL_EEPROM_CTL(sc, RAL_EEPROM_S | RAL_EEPROM_C);
	RAL_EEPROM_CTL(sc, RAL_EEPROM_S);

	/* Write start bit (1) */
	RAL_EEPROM_CTL(sc, RAL_EEPROM_S | RAL_EEPROM_D);
	RAL_EEPROM_CTL(sc, RAL_EEPROM_S | RAL_EEPROM_D | RAL_EEPROM_C);

	/* Write READ opcode (10) */
	RAL_EEPROM_CTL(sc, RAL_EEPROM_S | RAL_EEPROM_D);
	RAL_EEPROM_CTL(sc, RAL_EEPROM_S | RAL_EEPROM_D | RAL_EEPROM_C);
	RAL_EEPROM_CTL(sc, RAL_EEPROM_S);
	RAL_EEPROM_CTL(sc, RAL_EEPROM_S | RAL_EEPROM_C);

	/* Write address (A5-A0 or A7-A0) */
	n = (RAL_READ(sc, RAL_CSR21) & RAL_CSR21_93C46) ? 5 : 7;
	for (; n >= 0; n--) {
		RAL_EEPROM_CTL(sc, RAL_EEPROM_S |
		    (((addr >> n) & 1) << RAL_EEPROM_SHIFT_D));
		RAL_EEPROM_CTL(sc, RAL_EEPROM_S |
		    (((addr >> n) & 1) << RAL_EEPROM_SHIFT_D) | RAL_EEPROM_C);
	}

	RAL_EEPROM_CTL(sc, RAL_EEPROM_S);

	/* Read data Q15-Q0 */
	val = 0;
	for (n = 15; n >= 0; n--) {
		RAL_EEPROM_CTL(sc, RAL_EEPROM_S | RAL_EEPROM_C);
		tmp = RAL_READ(sc, RAL_CSR21);
		val |= ((tmp & RAL_EEPROM_Q) >> RAL_EEPROM_SHIFT_Q) << n;
		RAL_EEPROM_CTL(sc, RAL_EEPROM_S);
	}

	RAL_EEPROM_CTL(sc, 0);

	/* Clear Chip Select and clock C */
	RAL_EEPROM_CTL(sc, RAL_EEPROM_S);
	RAL_EEPROM_CTL(sc, 0);
	RAL_EEPROM_CTL(sc, RAL_EEPROM_C);

	return letoh16(val);
}

/*
 * Some frames have been processed by the hardware cipher engine and are ready
 * for transmission.
 */
void
ral_encryption_intr(struct ral_softc *sc)
{
	struct ral_tx_desc *desc;
	int hw;

	/* retrieve last descriptor index processed by cipher engine */
	hw = (RAL_READ(sc, RAL_SECCSR1) - sc->txq.physaddr) / RAL_TX_DESC_SIZE;

	for (; sc->txq.next_encrypt != hw;) {
		desc = &sc->txq.desc[sc->txq.next_encrypt];

		bus_dmamap_sync(sc->sc_dmat, sc->txq.map,
		    sc->txq.next_encrypt * RAL_TX_DESC_SIZE, RAL_TX_DESC_SIZE,
		    BUS_DMASYNC_POSTREAD);

		if ((letoh32(desc->flags) & RAL_TX_BUSY) ||
		    (letoh32(desc->flags) & RAL_TX_CIPHER_BUSY))
			break;

		/* for TKIP, swap eiv field to fix a bug in ASIC */
		if ((letoh32(desc->flags) & RAL_TX_CIPHER_MASK) ==
		    RAL_TX_CIPHER_TKIP)
			desc->eiv = swap32(desc->eiv);

		/* mark the frame ready for transmission */
		desc->flags |= htole32(RAL_TX_BUSY | RAL_TX_VALID);

		bus_dmamap_sync(sc->sc_dmat, sc->txq.map,
		    sc->txq.next_encrypt * RAL_TX_DESC_SIZE, RAL_TX_DESC_SIZE,
		    BUS_DMASYNC_PREWRITE);

		DPRINTFN(5, ("encryption done idx=%u\n", sc->txq.next_encrypt));

		sc->txq.next_encrypt =
		    (sc->txq.next_encrypt + 1) % RAL_TX_RING_COUNT;
	}

	/* kick Tx */
	RAL_WRITE(sc, RAL_TXCSR0, RAL_TXCSR0_KICK_TX);
}

void
ral_tx_intr(struct ral_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct ral_tx_desc *desc;
	struct ral_tx_data *data;

	for (;;) {
		desc = &sc->txq.desc[sc->txq.next];
		data = &sc->txq.data[sc->txq.next];

		bus_dmamap_sync(sc->sc_dmat, sc->txq.map,
		    sc->txq.next * RAL_TX_DESC_SIZE, RAL_TX_DESC_SIZE,
		    BUS_DMASYNC_POSTREAD);

		if ((letoh32(desc->flags) & RAL_TX_BUSY) ||
		    (letoh32(desc->flags) & RAL_TX_CIPHER_BUSY) ||
		    !(letoh32(desc->flags) & RAL_TX_VALID))
			break;

		switch (letoh32(desc->flags) & RAL_TX_RESULT_MASK) {
		case RAL_TX_SUCCESS:
			DPRINTFN(10, ("data frame sent successfully\n"));
			ieee80211_rssadapt_raise_rate(ic, &sc->rssadapt,
			    &data->id);
			break;

		case RAL_TX_SUCCESS_RETRY:
			DPRINTFN(2, ("data frame sent after %u retries\n",
			    (letoh32(desc->flags) >> 5) & 0x7));
			ieee80211_rssadapt_lower_rate(ic, data->ni,
			    &sc->rssadapt, &data->id);
			break;

		case RAL_TX_FAIL_RETRY:
			DPRINTFN(2, ("sending data frame failed (too much "
			    "retries)\n"));
			ieee80211_rssadapt_lower_rate(ic, data->ni,
			    &sc->rssadapt, &data->id);
			break;

		case RAL_TX_FAIL_INVALID:
		case RAL_TX_FAIL_OTHER:
		default:
			printf("%s: sending data frame failed 0x%08x\n",
			    sc->sc_dev.dv_xname, letoh32(desc->flags));
		}

		bus_dmamap_sync(sc->sc_dmat, data->map, 0,
		    data->map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, data->map);
		m_freem(data->m);
		data->m = NULL;
		if (data->ni == ic->ic_bss)
			ieee80211_unref_node(&data->ni);
		else
			ieee80211_free_node(ic, data->ni);
		data->ni = NULL;

		/* descriptor is no longer valid */
		desc->flags &= ~htole32(RAL_TX_VALID);

		bus_dmamap_sync(sc->sc_dmat, sc->txq.map,
		    sc->txq.next * RAL_TX_DESC_SIZE, RAL_TX_DESC_SIZE,
		    BUS_DMASYNC_PREWRITE);

		DPRINTFN(5, ("tx done idx=%u\n", sc->txq.next));

		sc->txq.queued--;
		sc->txq.next = (sc->txq.next + 1) % RAL_TX_RING_COUNT;
	}

	sc->sc_tx_timer = 0;
	ifp->if_flags &= ~IFF_OACTIVE;
	ral_start(ifp);
}

void
ral_prio_intr(struct ral_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct ral_tx_desc *desc;
	struct ral_tx_data *data;

	for (;;) {
		desc = &sc->prioq.desc[sc->prioq.next];
		data = &sc->prioq.data[sc->prioq.next];

		bus_dmamap_sync(sc->sc_dmat, sc->prioq.map,
		    sc->prioq.next * RAL_TX_DESC_SIZE, RAL_TX_DESC_SIZE,
		    BUS_DMASYNC_POSTREAD);

		if ((letoh32(desc->flags) & RAL_TX_BUSY) ||
		    !(letoh32(desc->flags) & RAL_TX_VALID))
			break;

		switch (letoh32(desc->flags) & RAL_TX_RESULT_MASK) {
		case RAL_TX_SUCCESS:
			DPRINTFN(10, ("mgt frame sent successfully\n"));
			break;

		case RAL_TX_SUCCESS_RETRY:
			DPRINTFN(10, ("mgt frame sent after %u retries\n",
			    (letoh32(desc->flags) >> 5) & 0x7));
			break;

		case RAL_TX_FAIL_RETRY:
			DPRINTFN(10, ("sending mgt frame failed (too much "
			    "retries)\n"));
			break;

		case RAL_TX_FAIL_INVALID:
		case RAL_TX_FAIL_OTHER:
		default:
			printf("%s: sending mgt frame failed 0x%08x\n",
			    sc->sc_dev.dv_xname, letoh32(desc->flags));
		}

		bus_dmamap_sync(sc->sc_dmat, data->map, 0,
		    data->map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, data->map);
		m_freem(data->m);
		data->m = NULL;
		if (data->ni == ic->ic_bss)
			ieee80211_unref_node(&data->ni);
		else
			ieee80211_free_node(ic, data->ni);
		data->ni = NULL;

		/* descriptor is no longer valid */
		desc->flags &= ~htole32(RAL_TX_VALID);

		bus_dmamap_sync(sc->sc_dmat, sc->prioq.map,
		    sc->prioq.next * RAL_TX_DESC_SIZE, RAL_TX_DESC_SIZE,
		    BUS_DMASYNC_PREWRITE);

		DPRINTFN(5, ("prio done idx=%u\n", sc->prioq.next));

		sc->prioq.queued--;
		sc->prioq.next = (sc->prioq.next + 1) % RAL_PRIO_RING_COUNT;
	}

	sc->sc_tx_timer = 0;
	ifp->if_flags &= ~IFF_OACTIVE;
	ral_start(ifp);
}

/*
 * Some frames were processed by the hardware cipher engine and are ready for
 * transmission to the IEEE802.11 layer.
 */
void
ral_decryption_intr(struct ral_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct ral_rx_desc *desc;
	struct ral_rx_data *data;
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni;
	struct mbuf *m;
	int hw, error;

	/* retrieve last decriptor index processed by cipher engine */
	hw = (RAL_READ(sc, RAL_SECCSR0) - sc->rxq.physaddr) / RAL_RX_DESC_SIZE;

	for (; sc->rxq.cur_decrypt != hw;) {
		desc = &sc->rxq.desc[sc->rxq.cur_decrypt];
		data = &sc->rxq.data[sc->rxq.cur_decrypt];

		bus_dmamap_sync(sc->sc_dmat, sc->rxq.map,
		    sc->rxq.cur_decrypt * RAL_TX_DESC_SIZE, RAL_TX_DESC_SIZE,
		    BUS_DMASYNC_POSTREAD);

		if ((letoh32(desc->flags) & RAL_RX_BUSY) ||
		    (letoh32(desc->flags) & RAL_RX_CIPHER_BUSY))
			break;

		if (data->drop)
			goto skip;

		if ((letoh32(desc->flags) & RAL_RX_CIPHER_MASK) != 0 &&
		    (letoh32(desc->flags) & RAL_RX_ICV_ERROR))
			goto skip;

		bus_dmamap_sync(sc->sc_dmat, data->map, 0,
		    data->map->dm_mapsize, BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->sc_dmat, data->map);

		/* get timestamp (high and low 32 bits) */
		RAL_READ(sc, RAL_CSR17);
		RAL_READ(sc, RAL_CSR16);

		/* finalize mbuf */
		m = data->m;
		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = m->m_len =
		    (letoh32(desc->flags) >> 16) & 0xfff;

		wh = mtod(m, struct ieee80211_frame *);
		ni = ieee80211_find_rxnode(ic, wh);

		/* send the frame to the 802.11 layer */
		ieee80211_input(ifp, m, ni, desc->rssi, 0);

		/* give rssi to the rate adatation algorithm */
		ieee80211_rssadapt_input(ic, ni, &sc->rssadapt, desc->rssi);

		MGETHDR(data->m, M_DONTWAIT, MT_DATA);
		if (data->m == NULL) {
			printf("%s: could not allocate rx mbuf\n",
			    sc->sc_dev.dv_xname);
			break;
		}

		MCLGET(data->m, M_DONTWAIT);
		if (!(data->m->m_flags & M_EXT)) {
			printf("%s: could not allocate rx mbuf cluster\n",
			    sc->sc_dev.dv_xname);
			m_freem(data->m);
			data->m = NULL;
			break;
		}

		error = bus_dmamap_load(sc->sc_dmat, data->map,
		    mtod(data->m, void *), MCLBYTES, NULL, BUS_DMA_NOWAIT);
		if (error != 0) {
			printf("%s: could not load rx buf DMA map\n",
			    sc->sc_dev.dv_xname);
			m_freem(data->m);
			data->m = NULL;
			break;
		}

		desc->physaddr = htole32(data->map->dm_segs->ds_addr);
skip:		desc->flags = htole32(RAL_RX_BUSY);

		bus_dmamap_sync(sc->sc_dmat, sc->rxq.map,
		    sc->rxq.cur_decrypt * RAL_TX_DESC_SIZE, RAL_TX_DESC_SIZE,
		    BUS_DMASYNC_PREWRITE);

		DPRINTFN(15, ("decryption done idx=%u\n", sc->rxq.cur_decrypt));

		sc->rxq.cur_decrypt =
		    (sc->rxq.cur_decrypt + 1) % RAL_RX_RING_COUNT;
	}
}

/*
 * Some frames were received. Pass them to the hardware cipher engine before
 * sending them to the 802.11 layer.
 */
void
ral_rx_intr(struct ral_softc *sc)
{
	struct ral_rx_desc *desc;
	struct ral_rx_data *data;

	for (;;) {
		desc = &sc->rxq.desc[sc->rxq.cur];
		data = &sc->rxq.data[sc->rxq.cur];

		bus_dmamap_sync(sc->sc_dmat, sc->rxq.map,
		    sc->rxq.cur * RAL_RX_DESC_SIZE, RAL_RX_DESC_SIZE,
		    BUS_DMASYNC_POSTREAD);

		if ((letoh32(desc->flags) & RAL_RX_BUSY) ||
		    (letoh32(desc->flags) & RAL_RX_CIPHER_BUSY))
			break;

		data->drop = 0;

		if ((letoh32(desc->flags) & RAL_RX_PHY_ERROR) ||
		    (letoh32(desc->flags) & RAL_RX_CRC_ERROR)) {
			/*
			 * this should not happen since we did not request
			 * to receive those frames when we filled RXCSR0
			 */
			DPRINTFN(5, ("PHY or CRC error flags 0x%08x\n",
			    letoh32(desc->flags)));
			data->drop = 1;
		}

		if (((letoh32(desc->flags) >> 16) & 0xfff) > MCLBYTES) {
			DPRINTFN(5, ("bad length\n"));
			data->drop = 1;
		}

		/* mark the frame for decryption */
		desc->flags |= htole32(RAL_RX_CIPHER_BUSY);

		bus_dmamap_sync(sc->sc_dmat, sc->rxq.map,
		    sc->rxq.cur * RAL_RX_DESC_SIZE, RAL_RX_DESC_SIZE,
		    BUS_DMASYNC_PREWRITE);

		DPRINTFN(15, ("rx done idx=%u\n", sc->rxq.cur));

		sc->rxq.cur = (sc->rxq.cur + 1) % RAL_RX_RING_COUNT;
	}

	/* kick decrypt */
	RAL_WRITE(sc, RAL_SECCSR0, RAL_SECCSR0_KICK);
}

/*
 * This function is called periodically in IBSS mode when a new beacon must be
 * sent out.
 */
void
ral_beacon_expire(struct ral_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ral_tx_data *data;

	if (ic->ic_opmode != IEEE80211_M_IBSS)
		return;

	data = &sc->bcnq.data[sc->bcnq.next];

	bus_dmamap_sync(sc->sc_dmat, data->map, 0, data->map->dm_mapsize,
	    BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(sc->sc_dmat, data->map);
#if 0
	ieee80211_beacon_update(ic, data->ni, &sc->sc_bo, data->m, 1);
	ral_tx_bcn(sc, data->m, data->ni);
#endif
	DPRINTFN(15, ("beacon expired\n"));

	sc->bcnq.next = (sc->bcnq.next + 1) % RAL_BEACON_RING_COUNT;
}

void
ral_wakeup_expire(struct ral_softc *sc)
{
	DPRINTFN(10, ("wakeup expired\n"));
}

int
ral_intr(void *arg)
{
	struct ral_softc *sc = arg;
	uint32_t r;

	/* disable interrupts */
	RAL_WRITE(sc, RAL_CSR8, 0xffffffff);

	r = RAL_READ(sc, RAL_CSR7);
	RAL_WRITE(sc, RAL_CSR7, r);

	if (r & RAL_CSR7_BEACON_EXPIRE)
		ral_beacon_expire(sc);

	if (r & RAL_CSR7_WAKEUP_EXPIRE)
		ral_wakeup_expire(sc);

	if (r & RAL_CSR7_ENCRYPTION_DONE)
		ral_encryption_intr(sc);

	if (r & RAL_CSR7_TX_DONE)
		ral_tx_intr(sc);

	if (r & RAL_CSR7_PRIO_DONE)
		ral_prio_intr(sc);

	if (r & RAL_CSR7_DECRYPTION_DONE)
		ral_decryption_intr(sc);

	if (r & RAL_CSR7_RX_DONE)
		ral_rx_intr(sc);

	/* re-enable interrupts */
	RAL_WRITE(sc, RAL_CSR8, RAL_CSR8_MASK);
	
	return 1;
}

uint8_t
ral_plcp_signal(int rate)
{
	switch (rate) {
	/* CCK rates (returned values are device-dependent) */
	case 2:		return 0x0;
	case 4:		return 0x1;
	case 11:	return 0x2;
	case 22:	return 0x3;

	/* OFDM rates (cf IEEE Std 802.11a-1999, pp. 14 Table 80) */
	case 12:	return 0xb;
	case 18:	return 0xf;
	case 24:	return 0xa;
	case 36:	return 0xe;
	case 48:	return 0x9;
	case 72:	return 0xd;
	case 96:	return 0x8;
	case 108:	return 0xc;

	/* unsupported rates (should not get there) */
	default:	return 0xff;
	}
}

#if 0
#define RAL_RATE_IS_OFDM(rate) (((rate) % 3) == 0)
#else
/* quickly determine if a rate is CCK or OFDM */
#define RAL_RATE_IS_OFDM(rate) ((rate) >= 12 && (rate) != 22)
#endif

#if 0
int
ral_tx_bcn(struct ral_softc *sc, struct mbuf *m0, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ral_tx_desc *desc;
	struct ral_tx_data *data;
	struct ieee80211_frame *wh;
	struct ieee80211_duration d0, dn;
	int npkt, error;

	desc = &sc->bcnq.desc[sc->bcnq.cur];
	data = &sc->bcnq.data[sc->bcnq.cur];

	wh = mtod(m0, struct ieee80211_frame *);

	/* XXX beacons are always sent at 2Mbps */
	error = ieee80211_compute_duration(wh, m0->m_pkthdr.len, ic->ic_flags,
	    ic->ic_fragthreshold, 4, &d0, &dn, &npkt,
	    ifp->if_flags & IFF_DEBUG);
	if (error != 0) {
		printf("%s: could not compute duration\n", sc->sc_dev.dv_xname);
		m_freem(m0);
		return error;
	}

	error = bus_dmamap_load_mbuf(sc->sc_dmat, data->map, m0,
	    BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not map mbuf (error %d)\n",
		    sc->sc_dev.dv_xname, error);
		m_freem(m0);
		return error;
	}

	data->m = m0;
	data->ni = ni;

	desc->physaddr = htole32(data->map->dm_segs->ds_addr);

	desc->flags = htole32(RAL_TX_VALID | RAL_TX_BUSY |
	    RAL_TX_INSERT_TIMESTAMP | RAL_TX_IFS_NEW_BACKOFF);
	desc->flags |= htole32(m0->m_pkthdr.len) << 16;

	desc->wme = htole32(
	    8 << RAL_WME_CWMAX_BITS_SHIFT |
	    3 << RAL_WME_CWMIN_BITS_SHIFT |
	    2 << RAL_WME_AIFSN_BITS_SHIFT);

	desc->plcp_length = htole16(d0.d_plcp_len);
	desc->plcp_service = 4;
	desc->plcp_signal = ral_plcp_signal(4);
	if (ic->ic_flags & IEEE80211_F_SHPREAMBLE)
		desc->plcp_signal |= 0x8;

	bus_dmamap_sync(sc->sc_dmat, data->map, 0, data->map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(sc->sc_dmat, sc->bcnq.map,
	    sc->bcnq.cur * RAL_TX_DESC_SIZE, RAL_TX_DESC_SIZE,
	    BUS_DMASYNC_PREWRITE);

	sc->bcnq.cur = (sc->bcnq.cur + 1) % RAL_BEACON_RING_COUNT;

	return 0;
}
#endif

int
ral_tx_mgt(struct ral_softc *sc, struct mbuf *m0, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct ral_tx_desc *desc;
	struct ral_tx_data *data;
	struct ieee80211_frame *wh;
	struct ieee80211_duration d0, dn;
	int npkt, error;

	desc = &sc->prioq.desc[sc->prioq.cur];
	data = &sc->prioq.data[sc->prioq.cur];

	wh = mtod(m0, struct ieee80211_frame *);

	/* XXX management frames are always sent at 2Mbps */
	error = ieee80211_compute_duration(wh, m0->m_pkthdr.len, ic->ic_flags,
	    ic->ic_fragthreshold, 4, &d0, &dn, &npkt,
	    ifp->if_flags & IFF_DEBUG);
	if (error != 0) {
		printf("%s: could not compute duration\n", sc->sc_dev.dv_xname);
		m_freem(m0);
		return error;
	}

	error = bus_dmamap_load_mbuf(sc->sc_dmat, data->map, m0,
	    BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not map mbuf (error %d)\n",
		    sc->sc_dev.dv_xname, error);
		m_freem(m0);
		return error;
	}

	data->m = m0;
	data->ni = ni;

	desc->physaddr = htole32(data->map->dm_segs->ds_addr);

	desc->flags = htole32(RAL_TX_VALID | RAL_TX_BUSY);
	desc->flags |= htole32(m0->m_pkthdr.len) << 16;

	if (!IEEE80211_IS_MULTICAST(wh->i_addr1)) {
		desc->flags |= htole32(RAL_TX_NEED_ACK);

		*(uint16_t *)wh->i_dur = htole16(d0.d_data_dur);

		if ((wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) ==
		    IEEE80211_FC0_SUBTYPE_PROBE_RESP)
			desc->flags |= htole32(RAL_TX_INSERT_TIMESTAMP);
	}

	desc->wme = htole32(
	    8 << RAL_WME_CWMAX_BITS_SHIFT |
	    3 << RAL_WME_CWMIN_BITS_SHIFT |
	    2 << RAL_WME_AIFSN_BITS_SHIFT);

	desc->plcp_length = htole16(d0.d_plcp_len);
	desc->plcp_service = 4;
	desc->plcp_signal = ral_plcp_signal(4);
	if (ic->ic_flags & IEEE80211_F_SHPREAMBLE)
		desc->plcp_signal |= 0x8;

	bus_dmamap_sync(sc->sc_dmat, data->map, 0, data->map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(sc->sc_dmat, sc->prioq.map,
	    sc->prioq.cur * RAL_TX_DESC_SIZE, RAL_TX_DESC_SIZE,
	    BUS_DMASYNC_PREWRITE);

	DPRINTFN(10, ("sending mgt frame len=%u idx=%u duration=%u plcp=%u\n",
	    m0->m_pkthdr.len, sc->prioq.cur, d0.d_data_dur, d0.d_plcp_len));

	/* kick prio */
	sc->prioq.queued++;
	sc->prioq.cur = (sc->prioq.cur + 1) % RAL_PRIO_RING_COUNT;
	RAL_WRITE(sc, RAL_TXCSR0, RAL_TXCSR0_KICK_PRIO);

	return 0;
}

int
ral_tx_data(struct ral_softc *sc, struct mbuf *m0, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct ral_tx_desc *desc;
	struct ral_tx_data *data;
	struct ieee80211_frame *wh;
	struct ieee80211_frame_rts *rts;
	struct ieee80211_duration d0, dn;
	struct mbuf *m, *mnew;
	int npkt, error;

	wh = mtod(m0, struct ieee80211_frame *);

	ni->ni_txrate = ieee80211_rssadapt_choose(&sc->rssadapt, &ni->ni_rates,
	    wh, m0->m_pkthdr.len, ic->ic_fixed_rate, NULL, 0);

	error = ieee80211_compute_duration(wh, m0->m_pkthdr.len, ic->ic_flags,
	    ic->ic_fragthreshold, ni->ni_rates.rs_rates[ni->ni_txrate], &d0,
	    &dn, &npkt, ifp->if_flags & IFF_DEBUG);
	if (error != 0) {
		printf("%s: could not compute duration\n", sc->sc_dev.dv_xname);
		m_freem(m0);
		return error;
	}

	if (m0->m_pkthdr.len > ic->ic_rtsthreshold) {
		desc = &sc->txq.desc[sc->txq.cur_encrypt];
		data = &sc->txq.data[sc->txq.cur_encrypt];

		/* switch to long retry */
		/* switch to frame gap SIFS */

		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if (m == NULL) {
			printf("%s: could not allocate RTS/CTS frame\n",
			    sc->sc_dev.dv_xname);
			return ENOMEM;
		}

		rts = mtod(m, struct ieee80211_frame_rts *);
		rts->i_fc[0] = IEEE80211_FC0_TYPE_CTL |
		    IEEE80211_FC0_SUBTYPE_RTS;
		*(uint16_t *)rts->i_dur = htole16(d0.d_rts_dur);
		memcpy(rts->i_ra, wh->i_addr1, IEEE80211_ADDR_LEN);
		memcpy(rts->i_ta, wh->i_addr2, IEEE80211_ADDR_LEN);

		error = bus_dmamap_load_mbuf(sc->sc_dmat, data->map, m,
		    BUS_DMA_NOWAIT);
		if (error != 0) {
			printf("%s: could not map mbuf (error %d)\n",
			    sc->sc_dev.dv_xname, error);
			m_freem(m);
			m_freem(m0);
			return error;
		}

		data->m = m;
		data->ni = ni;

		bus_dmamap_sync(sc->sc_dmat, data->map, 0,
		    data->map->dm_mapsize, BUS_DMASYNC_PREWRITE);

		desc->flags = htole32(RAL_TX_VALID | RAL_TX_BUSY);

		desc->physaddr = htole32(data->map->dm_segs->ds_addr);

		sc->txq.queued++;
		sc->txq.cur_encrypt =
		    (sc->txq.cur_encrypt + 1) % RAL_TX_RING_COUNT;
	}

	data = &sc->txq.data[sc->txq.cur_encrypt];
	desc = &sc->txq.desc[sc->txq.cur_encrypt];

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
		MCLGET(mnew, M_DONTWAIT);
		if (!(mnew->m_flags & M_EXT)) {
			m_freem(m0);
			m_freem(mnew);
			return ENOMEM;
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

	/* remember link conditions for rate adaptation algorithm */
	data->id.id_len = m0->m_pkthdr.len;
	data->id.id_rateidx = ni->ni_txrate;
	data->id.id_node = ni;
	data->id.id_rssi = ni->ni_rssi;

	desc->physaddr = htole32(data->map->dm_segs->ds_addr);

	desc->flags = htole32(RAL_TX_CIPHER_BUSY);
	desc->flags |= htole32(m0->m_pkthdr.len) << 16;

	if (!IEEE80211_IS_MULTICAST(wh->i_addr1)) {
		desc->flags |= htole32(RAL_TX_NEED_ACK);

		*(uint16_t *)wh->i_dur = htole16(d0.d_data_dur);
	}

	desc->wme = htole32(
	    8 << RAL_WME_CWMAX_BITS_SHIFT |
	    3 << RAL_WME_CWMIN_BITS_SHIFT |
	    2 << RAL_WME_AIFSN_BITS_SHIFT);

	desc->plcp_length = htole16(d0.d_plcp_len);
	desc->plcp_service = 4;
	if (d0.d_residue != 0)
		desc->plcp_service |= RAL_PLCP_LENGEXT;
	desc->plcp_signal =
	    ral_plcp_signal(ni->ni_rates.rs_rates[ni->ni_txrate]);
	if (ic->ic_flags & IEEE80211_F_SHPREAMBLE)
		desc->plcp_signal |= 0x8;

	bus_dmamap_sync(sc->sc_dmat, data->map, 0, data->map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(sc->sc_dmat, sc->txq.map,
	    sc->txq.cur_encrypt * RAL_TX_DESC_SIZE, RAL_TX_DESC_SIZE,
	    BUS_DMASYNC_PREWRITE);

	DPRINTFN(10, ("sending data frame len=%u idx=%u phys=%x plcp=%u "
	    "residue=%u\n", m0->m_pkthdr.len, sc->txq.cur_encrypt,
	    d0.d_data_dur, d0.d_plcp_len, d0.d_residue));

	/* kick encrypt */
	sc->txq.queued++;
	sc->txq.cur_encrypt = (sc->txq.cur_encrypt + 1) % RAL_TX_RING_COUNT;
	RAL_WRITE(sc, RAL_SECCSR1, RAL_SECCSR1_KICK);

	return 0;
}

void
ral_start(struct ifnet *ifp)
{
	struct ral_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct mbuf *m0;
	struct ieee80211_node *ni;

	for (;;) {
		IF_POLL(&ic->ic_mgtq, m0);
		if (m0 != NULL) {
			if (sc->prioq.queued >= RAL_PRIO_RING_COUNT) {
				ifp->if_flags |= IFF_OACTIVE;
				break;
			}
			IF_DEQUEUE(&ic->ic_mgtq, m0);

			ni = (struct ieee80211_node *)m0->m_pkthdr.rcvif;
			m0->m_pkthdr.rcvif = NULL;
#if NBPFILTER > 0
			if (ic->ic_rawbpf != NULL)
				bpf_mtap(ic->ic_rawbpf, m0);
#endif
			if (ral_tx_mgt(sc, m0, ni) != 0)
				break;

		} else {
			if (ic->ic_state != IEEE80211_S_RUN)
				break;
			IFQ_DEQUEUE(&ifp->if_snd, m0);
			if (m0 == NULL)
				break;
			if (sc->txq.queued >= RAL_TX_RING_COUNT) {
				IF_PREPEND(&ifp->if_snd, m0);
				ifp->if_flags |= IFF_OACTIVE;
				break;
			}
#if NBPFILTER > 0
			if (ifp->if_bpf != NULL)
				bpf_mtap(ifp->if_bpf, m0);
#endif
			m0 = ieee80211_encap(ifp, m0, &ni);
			if (m0 == NULL)
				continue;
#if NBPFILTER > 0
			if (ic->ic_rawbpf != NULL)
				bpf_mtap(ic->ic_rawbpf, m0);
#endif
			if (ral_tx_data(sc, m0, ni) != 0) {
				if (ni != NULL && ni != ic->ic_bss)
					ieee80211_free_node(ic, ni);
				break;
			}
		}

		sc->sc_tx_timer = 5;
		ifp->if_timer = 1;
	}
}

void
ral_watchdog(struct ifnet *ifp)
{
	struct ral_softc *sc = ifp->if_softc;

	ifp->if_timer = 0;

	if (sc->sc_tx_timer > 0) {
		if (--sc->sc_tx_timer == 0) {
			printf("%s: device timeout\n", sc->sc_dev.dv_xname);
			return;
		}
		ifp->if_timer = 1;
	}

	ieee80211_watchdog(ifp);
}

int
ral_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	int s, error = 0;

	s = splnet();
	
	switch (cmd) {
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (!(ifp->if_flags & IFF_RUNNING))
				ral_init(ifp);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				ral_stop(ifp, 1);
		}
		break;

	default:
		error = ieee80211_ioctl(ifp, cmd, data);
	}

	if (error == ENETRESET && cmd != SIOCADDMULTI) {
		if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) ==
		    (IFF_UP | IFF_RUNNING))
			ral_init(ifp);
		error = 0;
	}

	splx(s);

	return error;
}

void
ral_bbp_write(struct ral_softc *sc, uint8_t reg, uint8_t val)
{
	uint32_t tmp;
	int ntries;

	for (ntries = 0; ntries < 100; ntries++) {
		if (!(RAL_READ(sc, RAL_BBPCSR) & RAL_BBP_BUSY))
			break;
		DELAY(1);
	}
	if (ntries == 100) {
		printf("%s: could not write to BBP\n", sc->sc_dev.dv_xname);
		return;
	}

	tmp = RAL_BBP_WRITE | RAL_BBP_BUSY | reg << 8 | val;
	RAL_WRITE(sc, RAL_BBPCSR, tmp);

	DPRINTFN(15, ("BBP R%u <- 0x%02x\n", reg, val));
}

uint8_t
ral_bbp_read(struct ral_softc *sc, uint8_t reg)
{
	uint32_t val;
	int ntries;

	val = RAL_BBP_BUSY | reg << 8;
	RAL_WRITE(sc, RAL_BBPCSR, val);

	for (ntries = 0; ntries < 100; ntries++) {
		val = RAL_READ(sc, RAL_BBPCSR);
		if (!(val & RAL_BBP_BUSY))
			return val & 0xff;
		DELAY(1);
	}

	printf("%s: could not read from BBP\n", sc->sc_dev.dv_xname);
	return 0;
}

void
ral_rf_write(struct ral_softc *sc, uint8_t reg, uint32_t val)
{
	uint32_t tmp;
	int ntries;

	for (ntries = 0; ntries < 100; ntries++) {
		if (!(RAL_READ(sc, RAL_RFCSR) & RAL_RF_BUSY))
			break;
		DELAY(1);
	}
	if (ntries == 100) {
		printf("%s: could not write to RF\n", sc->sc_dev.dv_xname);
		return;
	}

	tmp = RAL_RF_BUSY | RAL_RF_20BIT | (val & 0xfffff) << 2 | (reg & 0x3);
	RAL_WRITE(sc, RAL_RFCSR, tmp);

	/* remember last written value in sc */
	sc->rf_regs[reg] = val;

	DPRINTFN(15, ("RF R[%u] <- 0x%05x\n", reg & 0x3, val & 0xfffff));
}

void
ral_set_chan(struct ral_softc *sc, struct ieee80211_channel *c)
{
#define N(a)	(sizeof (a) / sizeof ((a)[0]))
	struct ieee80211com *ic = &sc->sc_ic;
	uint8_t power, tmp;
	u_int i, chan;

	chan = ieee80211_chan2ieee(ic, c);
	if (chan == 0 || chan == IEEE80211_CHAN_ANY)
		return;

	if (IEEE80211_IS_CHAN_2GHZ(c))
		power = min(sc->txpow[chan - 1], 31);
	else
		power = 31;

	switch (sc->rf_rev) {
	case RAL_RF_2522:
		ral_rf_write(sc, RAL_RF1, 0x00814);
		ral_rf_write(sc, RAL_RF2, ral_rf2522_r2[chan - 1]);
		ral_rf_write(sc, RAL_RF3, power << 7 | 0x00040);
		break;

	case RAL_RF_2523:
		ral_rf_write(sc, RAL_RF1, 0x08804);
		ral_rf_write(sc, RAL_RF2, ral_rf2523_r2[chan - 1]);
		ral_rf_write(sc, RAL_RF3, power << 7 | 0x38044);
		ral_rf_write(sc, RAL_RF4, (chan == 14) ? 0x00280 : 0x00286);
		break;

	case RAL_RF_2524:
		ral_rf_write(sc, RAL_RF1, 0x0c808);
		ral_rf_write(sc, RAL_RF2, ral_rf2524_r2[chan - 1]);
		ral_rf_write(sc, RAL_RF3, power << 7 | 0x00040);
		ral_rf_write(sc, RAL_RF4, (chan == 14) ? 0x00280 : 0x00286);
		break;

	case RAL_RF_2525:
		ral_rf_write(sc, RAL_RF1, 0x08808);
		ral_rf_write(sc, RAL_RF2, ral_rf2525_hi_r2[chan - 1]);
		ral_rf_write(sc, RAL_RF3, power << 7 | 0x18044);
		ral_rf_write(sc, RAL_RF4, (chan == 14) ? 0x00280 : 0x00286);

		ral_rf_write(sc, RAL_RF1, 0x08808);
		ral_rf_write(sc, RAL_RF2, ral_rf2525_r2[chan - 1]);
		ral_rf_write(sc, RAL_RF3, power << 7 | 0x18044);
		ral_rf_write(sc, RAL_RF4, (chan == 14) ? 0x00280 : 0x00286);
		break;

	case RAL_RF_2525E:
		ral_rf_write(sc, RAL_RF1, 0x08808);
		ral_rf_write(sc, RAL_RF2, ral_rf2525e_r2[chan - 1]);
		ral_rf_write(sc, RAL_RF3, power << 7 | 0x18044);
		ral_rf_write(sc, RAL_RF4, (chan == 14) ? 0x00286 : 0x00282);
		break;

	/* dual-band RF */
	case RAL_RF_5222:
		for (i = 0; i < N(ral_rf5222); i++)
			if (ral_rf5222[i].chan == chan)
				break;

		if (i < N(ral_rf5222)) {
			ral_rf_write(sc, RAL_RF1, ral_rf5222[i].r1);
			ral_rf_write(sc, RAL_RF2, ral_rf5222[i].r2);
			ral_rf_write(sc, RAL_RF3, power << 7 | 0x00040);
			ral_rf_write(sc, RAL_RF4, ral_rf5222[i].r4);
		}
		break;
	}

	if (sc->sc_ic.ic_state != IEEE80211_S_SCAN) {
		/* set Japan filter bit for channel 14 */
		tmp = ral_bbp_read(sc, 70);

		tmp &= ~RAL_JAPAN_FILTER;
		if (chan == 14)
			tmp |= RAL_JAPAN_FILTER;

		ral_bbp_write(sc, 70, tmp);

		DELAY(1000); /* RF needs a 1ms delay here */
		ral_disable_rf_tune(sc);

		/* clear CRC errors */
		RAL_READ(sc, RAL_CNT0);
	}
#undef N
}

/*
 * Disable RF auto-tuning.
 */
void
ral_disable_rf_tune(struct ral_softc *sc)
{
	uint32_t tmp;

	if (sc->rf_rev != RAL_RF_2523) {
		tmp = sc->rf_regs[RAL_RF1] & ~RAL_RF1_AUTOTUNE;
		ral_rf_write(sc, RAL_RF1, tmp);
	}

	tmp = sc->rf_regs[RAL_RF3] & ~RAL_RF3_AUTOTUNE;
	ral_rf_write(sc, RAL_RF3, tmp);
}

/* cf IEEE80211-1999 pp123 */
void
ral_enable_tsf_sync(struct ral_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	uint32_t tmp;

	/* first, disable TSF synchronization */
	RAL_WRITE(sc, RAL_CSR14, 0);

	/* set beacon interval and CfpMaxDuration in 1/16TU */
	tmp = 16 * ic->ic_bss->ni_intval;
	RAL_WRITE(sc, RAL_CSR12, tmp);

/* voir ni->ni_timoff ou on doit trouver le cfp et le atim */

	/* set CFP Period to 0 */
	RAL_WRITE(sc, RAL_CSR13, 0);

	/* set bcncsr1 (CWMin + preload) */
	tmp = 5 << 16 | (IEEE80211_DUR_DS_LONG_PREAMBLE + 240);
	RAL_WRITE(sc, RAL_BCNCSR1, tmp);

	if (ic->ic_opmode == IEEE80211_M_IBSS)
		RAL_WRITE(sc, RAL_CSR14, RAL_CSR14_TSF_SYNC_IBSS |
		    RAL_CSR14_TSF_AUTOCOUNT | RAL_CSR14_BCN_RELOAD |
		    RAL_CSR14_GENERATE_BEACON);
	else
		RAL_WRITE(sc, RAL_CSR14, RAL_CSR14_TSF_SYNC_BSS |
		    RAL_CSR14_TSF_AUTOCOUNT | RAL_CSR14_BCN_RELOAD);
}

void
ral_update_plcp(struct ral_softc *sc)
{
	/* no short preamble for 1Mbps */
	RAL_WRITE(sc, RAL_ARCSR2, 0x00700400);

	if (!(sc->sc_ic.ic_flags & IEEE80211_F_SHPREAMBLE)) {
		/* values taken from the reference driver */
		RAL_WRITE(sc, RAL_ARCSR3, 0x00380401);
		RAL_WRITE(sc, RAL_ARCSR4, 0x00150402);
		RAL_WRITE(sc, RAL_ARCSR5, 0x000b8403);
	} else {
		/* same values as above or'ed 0x8 */
		RAL_WRITE(sc, RAL_ARCSR3, 0x00380409);
		RAL_WRITE(sc, RAL_ARCSR4, 0x0015040a);
		RAL_WRITE(sc, RAL_ARCSR5, 0x000b840b);
	}
}

void
ral_update_led(struct ral_softc *sc, int led1, int led2)
{
	uint32_t tmp;

	/* set ON period to 70ms and OFF period to 30ms */
	tmp = led1 << 16 | led2 << 17 | 70 << 8 | 30;
	RAL_WRITE(sc, RAL_LEDCSR, tmp);
}

void
ral_set_bssid(struct ral_softc *sc, uint8_t *bssid)
{
	uint32_t tmp;

	tmp = bssid[0] | bssid[1] << 8 | bssid[2] << 16 | bssid[3] << 24;
	RAL_WRITE(sc, RAL_CSR5, tmp);

	tmp = bssid[4] | bssid[5] << 8;
	RAL_WRITE(sc, RAL_CSR6, tmp);

	DPRINTFN(10, ("setting BSSID to %s\n", ether_sprintf(bssid)));
}

void
ral_set_macaddr(struct ral_softc *sc, uint8_t *addr)
{
	uint32_t tmp;

	tmp = addr[0] | addr[1] << 8 | addr[2] << 16 | addr[3] << 24;
	RAL_WRITE(sc, RAL_CSR3, tmp);

	tmp = addr[4] | addr[5] << 8;
	RAL_WRITE(sc, RAL_CSR4, tmp);

	DPRINTFN(10, ("setting MAC address to %s\n", ether_sprintf(addr)));
}

int
ral_read_eeprom(struct ral_softc *sc)
{
	uint16_t val;
	const char *rf;
	int i;

	val = ral_eeprom_read(sc, RAL_EEPROM_VERSION);
	sc->eeprom_rev = val >> 8;

	if (sc->eeprom_rev != 1) {
		printf("%s: unknown EEPROM rev. 0x%x\n", sc->sc_dev.dv_xname,
		    sc->eeprom_rev);
		return EIO;
	}

	val = ral_eeprom_read(sc, RAL_EEPROM_ANTENNA);
	sc->rf_rev = (val >> 11) & 0x1f;
	sc->led_mode = (val >> 6) & 0x7;
	sc->hw_radio = (val >> 10) & 0x1;
	sc->rx_ant = (val >> 4) & 0x3;
	sc->tx_ant = (val >> 2) & 0x3;
	sc->nb_ant = val & 0x3;
/*	DPRINTFN(10, ("RF rev. 0x%02x, Tx Ant %u, Rx Ant %u, Nb Ant %u, "
	    "Led Mode %u, HW radio %s\n",
	    sc->rf_rev, sc->tx_ant, sc->rx_ant, sc->nb_ant, sc->led_mode,
	    sc->hw_radio ? "yes" : "no"));*/

	switch (sc->rf_rev) {
	case RAL_RF_2522:  rf = "RT2522";  break;
	case RAL_RF_2523:  rf = "RT2523";  break;
	case RAL_RF_2524:  rf = "RT2524";  break;
	case RAL_RF_2525:  rf = "RT2525";  break;
	case RAL_RF_2525E: rf = "RT2525e"; break;
	case RAL_RF_5222:  rf = "RT5222";  break;
	default:	   rf = "unknown";
	}

	printf("%s: MAC/BBP RT2560 (rev 0x%02x), RF %s\n", sc->sc_dev.dv_xname,
	    sc->asic_rev, rf);

	val = ral_eeprom_read(sc, RAL_EEPROM_CONFIG);
	if (val == 0xffff)
		val = 0;

	sc->bbp_tune = !((val >> 1) & 0x1);

	/* read default values for BBP registers */
	for (i = 0; i < 16; i++) {
		val = ral_eeprom_read(sc, RAL_EEPROM_BBP_BASE + i);
		sc->bbp_prom[i].reg = val >> 8;
		sc->bbp_prom[i].val = val & 0xff;
	}

	/* read Tx power for all b/g channels */
	for (i = 0; i < 14 / 2; i++) {
		val = ral_eeprom_read(sc, RAL_EEPROM_TXPOWER_BASE + i);
		sc->txpow[i * 2] = val >> 8;
		sc->txpow[i * 2 + 1] = val & 0xff;
	}

	return 0;
}

int
ral_bbp_init(struct ral_softc *sc)
{
#define N(a)	(sizeof (a) / sizeof ((a)[0]))
	int i, ntries;

	/* wait for BBP to be ready */
	for (ntries = 0; ntries < 100; ntries++) {
		if (ral_bbp_read(sc, RAL_BBP_VERSION) != 0)
			break;
		DELAY(1);
	}
	if (ntries == 100) {
		printf("%s: timeout waiting for BBP\n", sc->sc_dev.dv_xname);
		return EIO;
	}

	/* initialize BBP registers to default values */
	for (i = 0; i < N(ral_def_bbp); i++)
		ral_bbp_write(sc, ral_def_bbp[i].reg, ral_def_bbp[i].val);

	/* initialize BBP registers to values stored in EEPROM */
	for (i = 0; i < 16; i++) {
		if (sc->bbp_prom[i].reg == 0xff)
			continue;
		ral_bbp_write(sc, sc->bbp_prom[i].reg, sc->bbp_prom[i].val);
	}

	return 0;
#undef N
}

int
ral_init(struct ifnet *ifp)
{
#define N(a)	(sizeof (a) / sizeof ((a)[0]))
	struct ral_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	uint32_t tmp;
	int i;

	/* for CardBus, power on the socket */
	if (sc->sc_enable != NULL)
		(*sc->sc_enable)(sc);

	ral_stop(ifp, 0);

	/* setup tx rings */
	tmp = RAL_PRIO_RING_COUNT << 24 |
	      RAL_ATIM_RING_COUNT << 16 |
	      RAL_TX_RING_COUNT   <<  8 |
	      RAL_TX_DESC_SIZE;
	RAL_WRITE(sc, RAL_TXCSR2, tmp);
	RAL_WRITE(sc, RAL_TXCSR3, sc->txq.physaddr);
	RAL_WRITE(sc, RAL_TXCSR4, sc->atimq.physaddr);
	RAL_WRITE(sc, RAL_TXCSR5, sc->prioq.physaddr);
	RAL_WRITE(sc, RAL_TXCSR6, sc->bcnq.physaddr);

	/* setup rx ring */
	tmp = RAL_RX_RING_COUNT << 8 | RAL_RX_DESC_SIZE;
	RAL_WRITE(sc, RAL_RXCSR1, tmp);
	RAL_WRITE(sc, RAL_RXCSR2, sc->rxq.physaddr);

	/* initialize MAC registers to default values */
	for (i = 0; i < N(ral_def_mac); i++)
		RAL_WRITE(sc, ral_def_mac[i].reg, ral_def_mac[i].val);

	/* set basic rates mask */
	RAL_WRITE(sc, RAL_ARCSR1, 0x15f);

	IEEE80211_ADDR_COPY(ic->ic_myaddr, LLADDR(ifp->if_sadl));
	ral_set_macaddr(sc, ic->ic_myaddr);

	ral_update_plcp(sc);
	ral_update_led(sc, 0, 0);

	/* set soft reset and host ready */
	RAL_WRITE(sc, RAL_CSR1, RAL_CSR1_SOFT_RESET);
	RAL_WRITE(sc, RAL_CSR1, RAL_CSR1_HOST_READY);

	if (ral_bbp_init(sc) != 0) {
		ral_stop(ifp, 1);
		return 1;
	}

	/* set default BSS channel */
	ic->ic_bss->ni_chan = &ic->ic_channels[1];
	ral_set_chan(sc, ic->ic_bss->ni_chan);

	/* kick Rx */
	if (ic->ic_opmode != IEEE80211_M_MONITOR)
		RAL_WRITE(sc, RAL_RXCSR0,
		    RAL_RXCSR0_DROP_CRC | RAL_RXCSR0_DROP_PHY |
		    RAL_RXCSR0_DROP_CTL | RAL_RXCSR0_DROP_NOT_TO_ME |
		    RAL_RXCSR0_DROP_TODS | RAL_RXCSR0_DROP_BAD_VERSION);
	else
		RAL_WRITE(sc, RAL_RXCSR0, 0);

	/* clear old FCS and Rx FIFO errors */
	RAL_READ(sc, RAL_CNT0);
	RAL_READ(sc, RAL_CNT4);

	/* clear any pending interrupts */
	RAL_WRITE(sc, RAL_CSR7, 0xffffffff);

	/* enable interrupts */
	RAL_WRITE(sc, RAL_CSR8, RAL_CSR8_MASK);

	ifp->if_flags &= ~IFF_OACTIVE;
	ifp->if_flags |= IFF_RUNNING;

	if (ic->ic_opmode != IEEE80211_M_MONITOR)
		ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);

	return 0;
#undef N
}

void
ral_stop(struct ifnet *ifp, int disable)
{
	struct ral_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;

	ieee80211_new_state(ic, IEEE80211_S_INIT, -1);

	/* abort Tx */
	RAL_WRITE(sc, RAL_TXCSR0, RAL_TXCSR0_ABORT);

	/* disable Rx */
	RAL_WRITE(sc, RAL_RXCSR0, RAL_RXCSR0_DISABLE);

	/* reset ASIC (and thus, BBP) */
	RAL_WRITE(sc, RAL_CSR1, RAL_CSR1_SOFT_RESET);
	RAL_WRITE(sc, RAL_CSR1, 0);

	/* disable interrupts */
	RAL_WRITE(sc, RAL_CSR8, 0xffffffff);

	/* reset Tx and Rx rings */
	ral_reset_tx_ring(sc, &sc->txq);
	ral_reset_tx_ring(sc, &sc->prioq);
	ral_reset_tx_ring(sc, &sc->atimq);
	ral_reset_tx_ring(sc, &sc->bcnq);
	ral_reset_rx_ring(sc, &sc->rxq);

	/* for CardBus, power down the socket */
	if (disable && sc->sc_disable != NULL)
		(*sc->sc_disable)(sc);

	sc->sc_tx_timer = 0;
	ifp->if_timer = 0;
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
}

struct cfdriver ral_cd = {
        0, "ral", DV_IFNET
};
