/* $OpenBSD: bwfm.c,v 1.12 2017/10/18 20:24:20 patrick Exp $ */
/*
 * Copyright (c) 2010-2016 Broadcom Corporation
 * Copyright (c) 2016,2017 Patrick Wildt <patrick@blueri.se>
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

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/sockio.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <net80211/ieee80211_var.h>

#include <dev/ic/bwfmvar.h>
#include <dev/ic/bwfmreg.h>

/* #define BWFM_DEBUG */
#ifdef BWFM_DEBUG
#define DPRINTF(x)	do { if (bwfm_debug > 0) printf x; } while (0)
#define DPRINTFN(n, x)	do { if (bwfm_debug >= (n)) printf x; } while (0)
static int bwfm_debug = 1;
#else
#define DPRINTF(x)	do { ; } while (0)
#define DPRINTFN(n, x)	do { ; } while (0)
#endif

#define DEVNAME(sc)	((sc)->sc_dev.dv_xname)

void	 bwfm_start(struct ifnet *);
void	 bwfm_init(struct ifnet *);
void	 bwfm_stop(struct ifnet *);
void	 bwfm_watchdog(struct ifnet *);
int	 bwfm_ioctl(struct ifnet *, u_long, caddr_t);
int	 bwfm_media_change(struct ifnet *);
void	 bwfm_media_status(struct ifnet *, struct ifmediareq *);

int	 bwfm_chip_attach(struct bwfm_softc *);
int	 bwfm_chip_detach(struct bwfm_softc *, int);
struct bwfm_core *bwfm_chip_get_core(struct bwfm_softc *, int);
struct bwfm_core *bwfm_chip_get_pmu(struct bwfm_softc *);
int	 bwfm_chip_ai_isup(struct bwfm_softc *, struct bwfm_core *);
void	 bwfm_chip_ai_disable(struct bwfm_softc *, struct bwfm_core *,
	     uint32_t, uint32_t);
void	 bwfm_chip_ai_reset(struct bwfm_softc *, struct bwfm_core *,
	     uint32_t, uint32_t, uint32_t);
void	 bwfm_chip_dmp_erom_scan(struct bwfm_softc *);
int	 bwfm_chip_dmp_get_regaddr(struct bwfm_softc *, uint32_t *,
	     uint32_t *, uint32_t *);
void	 bwfm_chip_cr4_set_passive(struct bwfm_softc *);
void	 bwfm_chip_ca7_set_passive(struct bwfm_softc *);
void	 bwfm_chip_cm3_set_passive(struct bwfm_softc *);

int	 bwfm_proto_bcdc_query_dcmd(struct bwfm_softc *, int,
	     int, char *, size_t *);
int	 bwfm_proto_bcdc_set_dcmd(struct bwfm_softc *, int,
	     int, char *, size_t);

int	 bwfm_fwvar_cmd_get_data(struct bwfm_softc *, int, void *, size_t);
int	 bwfm_fwvar_cmd_set_data(struct bwfm_softc *, int, void *, size_t);
int	 bwfm_fwvar_cmd_get_int(struct bwfm_softc *, int, uint32_t *);
int	 bwfm_fwvar_cmd_set_int(struct bwfm_softc *, int, uint32_t);
int	 bwfm_fwvar_var_get_data(struct bwfm_softc *, char *, void *, size_t);
int	 bwfm_fwvar_var_set_data(struct bwfm_softc *, char *, void *, size_t);
int	 bwfm_fwvar_var_get_int(struct bwfm_softc *, char *, uint32_t *);
int	 bwfm_fwvar_var_set_int(struct bwfm_softc *, char *, uint32_t);

void	 bwfm_scan(struct bwfm_softc *);
void	 bwfm_scan_timeout(void *);

void	 bwfm_rx(struct bwfm_softc *, char *, size_t);
void	 bwfm_rx_event(struct bwfm_softc *, char *, size_t);
void	 bwfm_scan_node(struct bwfm_softc *, struct bwfm_bss_info *, size_t);

extern void ieee80211_node2req(struct ieee80211com *,
	     const struct ieee80211_node *, struct ieee80211_nodereq *);
extern void ieee80211_req2node(struct ieee80211com *,
	     const struct ieee80211_nodereq *, struct ieee80211_node *);

uint8_t bwfm_2ghz_channels[] = {
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
};
uint8_t bwfm_5ghz_channels[] = {
	34, 36, 38, 40, 42, 44, 46, 48, 52, 56, 60, 64, 100, 104, 108, 112,
	116, 120, 124, 128, 132, 136, 140, 144, 149, 153, 157, 161, 165,
};

struct bwfm_proto_ops bwfm_proto_bcdc_ops = {
	.proto_query_dcmd = bwfm_proto_bcdc_query_dcmd,
	.proto_set_dcmd = bwfm_proto_bcdc_set_dcmd,
};

struct cfdriver bwfm_cd = {
	NULL, "bwfm", DV_IFNET
};

void
bwfm_attach(struct bwfm_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	uint32_t bandlist[3], tmp;
	int i, nbands, nmode, vhtmode;

	if (bwfm_fwvar_cmd_get_int(sc, BWFM_C_GET_VERSION, &tmp)) {
		printf("%s: could not read io type\n", DEVNAME(sc));
		return;
	} else
		sc->sc_io_type = tmp;
	if (bwfm_fwvar_var_get_data(sc, "cur_etheraddr", ic->ic_myaddr,
	    sizeof(ic->ic_myaddr))) {
		printf("%s: could not read mac address\n", DEVNAME(sc));
		return;
	}
	printf("%s: address %s\n", DEVNAME(sc), ether_sprintf(ic->ic_myaddr));

	/*
	 * This hardware is supposed to be run with wpa_supplicant, but since
	 * we want to handle basic authentication in our stack, we _need_ to
	 * rely on the firmware's handshake algorithm.  If it doesn't exist we
	 * will have to think about something new.
	 */
	if (bwfm_fwvar_var_get_int(sc, "sup_wpa", &tmp)) {
		printf("%s: no supplicant in firmware, bailing\n", DEVNAME(sc));
		return;
	}

	ic->ic_caps = IEEE80211_C_RSN;	/* WPA/RSN */

	if (bwfm_fwvar_var_get_int(sc, "nmode", &nmode))
		nmode = 0;
	if (bwfm_fwvar_var_get_int(sc, "vhtmode", &vhtmode))
		vhtmode = 0;
	if (bwfm_fwvar_cmd_get_data(sc, BWFM_C_GET_BANDLIST, bandlist,
	    sizeof(bandlist))) {
		printf("%s: couldn't get supported band list\n", DEVNAME(sc));
		return;
	}
	nbands = letoh32(bandlist[0]);
	for (i = 1; i <= nbands && i < nitems(bandlist); i++) {
		switch (letoh32(bandlist[i])) {
		case BWFM_BAND_2G:
			DPRINTF(("%s: 2G HT %d VHT %d\n",
			    DEVNAME(sc), nmode, vhtmode));
			ic->ic_sup_rates[IEEE80211_MODE_11B] =
			    ieee80211_std_rateset_11b;
			ic->ic_sup_rates[IEEE80211_MODE_11G] =
			    ieee80211_std_rateset_11g;

			for (i = 0; i < nitems(bwfm_2ghz_channels); i++) {
				uint8_t chan = bwfm_2ghz_channels[i];
				ic->ic_channels[chan].ic_freq =
				    ieee80211_ieee2mhz(chan, IEEE80211_CHAN_2GHZ);
				ic->ic_channels[chan].ic_flags =
				    IEEE80211_CHAN_CCK | IEEE80211_CHAN_OFDM |
				    IEEE80211_CHAN_DYN | IEEE80211_CHAN_2GHZ;
				if (nmode)
					ic->ic_channels[chan].ic_flags |=
					    IEEE80211_CHAN_HT;
			}
			break;
		case BWFM_BAND_5G:
			DPRINTF(("%s: 5G HT %d VHT %d\n",
			    DEVNAME(sc), nmode, vhtmode));
			ic->ic_sup_rates[IEEE80211_MODE_11A] =
			    ieee80211_std_rateset_11a;

			for (i = 0; i < nitems(bwfm_5ghz_channels); i++) {
				uint8_t chan = bwfm_5ghz_channels[i];
				ic->ic_channels[chan].ic_freq =
				    ieee80211_ieee2mhz(chan, IEEE80211_CHAN_5GHZ);
				ic->ic_channels[chan].ic_flags =
				    IEEE80211_CHAN_A;
				if (nmode)
					ic->ic_channels[chan].ic_flags |=
					    IEEE80211_CHAN_HT;
			}
			break;
		default:
			printf("%s: unsupported band 0x%x\n", DEVNAME(sc),
			    letoh32(bandlist[i]));
			break;
		}
	}

	/* IBSS channel undefined for now. */
	ic->ic_ibss_chan = &ic->ic_channels[0];

	/* Init some net80211 stuff we abuse. */
	ieee80211_node_attach(ifp);

	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = bwfm_ioctl;
	ifp->if_start = bwfm_start;
	ifp->if_watchdog = bwfm_watchdog;
	memcpy(ifp->if_xname, DEVNAME(sc), IFNAMSIZ);

	ifmedia_init(&sc->sc_media, 0, bwfm_media_change, bwfm_media_status);
	ifmedia_set(&sc->sc_media,
	    IFM_MAKEWORD(IFM_IEEE80211, IFM_AUTO, 0, 0));
	if_attach(ifp);
	memcpy(((struct arpcom *)ifp)->ac_enaddr, ic->ic_myaddr,
	    ETHER_ADDR_LEN);
	ether_ifattach(ifp);
}

int
bwfm_detach(struct bwfm_softc *sc, int flags)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	ether_ifdetach(ifp);
	if_detach(ifp);
	return 0;
}

void
bwfm_start(struct ifnet *ifp)
{
	struct bwfm_softc *sc = ifp->if_softc;
	struct mbuf *m;
	int error;

	if (!(ifp->if_flags & IFF_RUNNING))
		return;
	if (ifq_is_oactive(&ifp->if_snd))
		return;
	if (IFQ_IS_EMPTY(&ifp->if_snd))
		return;

	/* TODO: return if no link? */

	m = ifq_deq_begin(&ifp->if_snd);
	while (m != NULL) {
		error = sc->sc_bus_ops->bs_txdata(sc, m);
		if (error == ENOBUFS) {
			ifq_deq_rollback(&ifp->if_snd, m);
			ifq_set_oactive(&ifp->if_snd);
			break;
		}
		if (error == EFBIG) {
			ifq_deq_commit(&ifp->if_snd, m);
			m_freem(m); /* give up: drop it */
			ifp->if_oerrors++;
			continue;
		}

		/* Now we are committed to transmit the packet. */
		ifq_deq_commit(&ifp->if_snd, m);

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_OUT);
#endif

		m = ifq_deq_begin(&ifp->if_snd);
	}
}

void
bwfm_init(struct ifnet *ifp)
{
	struct bwfm_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	uint8_t evmask[BWFM_EVENT_MASK_LEN];
	struct bwfm_ext_join_params *params;
	struct bwfm_join_pref_params join_pref[2];

	if (bwfm_fwvar_var_set_int(sc, "mpc", 1)) {
		printf("%s: could not set mpc\n", DEVNAME(sc));
		return;
	}

	/* Select target by RSSI (boost on 5GHz) */
	join_pref[0].type = BWFM_JOIN_PREF_RSSI_DELTA;
	join_pref[0].len = 2;
	join_pref[0].rssi_gain = BWFM_JOIN_PREF_RSSI_BOOST;
	join_pref[0].band = BWFM_JOIN_PREF_BAND_5G;
	join_pref[1].type = BWFM_JOIN_PREF_RSSI;
	join_pref[1].len = 2;
	join_pref[1].rssi_gain = 0;
	join_pref[1].band = 0;
	if (bwfm_fwvar_var_set_data(sc, "join_pref", join_pref,
	    sizeof(join_pref))) {
		printf("%s: could not set join pref\n", DEVNAME(sc));
		return;
	}

	if (bwfm_fwvar_var_get_data(sc, "event_msgs", evmask, sizeof(evmask))) {
		printf("%s: could not get event mask\n", DEVNAME(sc));
		return;
	}
	evmask[BWFM_E_IF / 8] |= 1 << (BWFM_E_IF % 8);
	evmask[BWFM_E_ESCAN_RESULT / 8] |= 1 << (BWFM_E_ESCAN_RESULT % 8);
	evmask[BWFM_E_SET_SSID / 8] |= 1 << (BWFM_E_SET_SSID % 8);
	if (bwfm_fwvar_var_set_data(sc, "event_msgs", evmask, sizeof(evmask))) {
		printf("%s: could not set event mask\n", DEVNAME(sc));
		return;
	}

	if (bwfm_fwvar_cmd_set_int(sc, BWFM_C_SET_SCAN_CHANNEL_TIME,
	    BWFM_DEFAULT_SCAN_CHANNEL_TIME)) {
		printf("%s: could not set scan channel time\n", DEVNAME(sc));
		return;
	}
	if (bwfm_fwvar_cmd_set_int(sc, BWFM_C_SET_SCAN_UNASSOC_TIME,
	    BWFM_DEFAULT_SCAN_UNASSOC_TIME)) {
		printf("%s: could not set scan unassoc time\n", DEVNAME(sc));
		return;
	}
	if (bwfm_fwvar_cmd_set_int(sc, BWFM_C_SET_SCAN_PASSIVE_TIME,
	    BWFM_DEFAULT_SCAN_PASSIVE_TIME)) {
		printf("%s: could not set scan passive time\n", DEVNAME(sc));
		return;
	}

	if (bwfm_fwvar_cmd_set_int(sc, BWFM_C_SET_PM, 2)) {
		printf("%s: could not set power\n", DEVNAME(sc));
		return;
	}

	bwfm_fwvar_var_set_int(sc, "txbf", 1);
	bwfm_fwvar_cmd_set_int(sc, BWFM_C_UP, 0);
	bwfm_fwvar_cmd_set_int(sc, BWFM_C_SET_INFRA, 1);
	bwfm_fwvar_cmd_set_int(sc, BWFM_C_SET_AP, 0);

	/* Disable all offloading (ARP, NDP, TCP/UDP cksum). */
	bwfm_fwvar_var_set_int(sc, "arp_ol", 0);
	bwfm_fwvar_var_set_int(sc, "arpoe", 0);
	bwfm_fwvar_var_set_int(sc, "ndoe", 0);
	bwfm_fwvar_var_set_int(sc, "toe", 0);

	/*
	 * Use the firmware supplicant to handle the WPA handshake.
	 * As long as we're still figuring things out this is ok, but
	 * it would be better to handle the handshake in our stack.
	 */
	bwfm_fwvar_var_set_int(sc, "sup_wpa", 1);

	/*
	 * OPEN: Open or WPA/WPA2 on newer Chips/Firmware.
	 * SHARED KEY: WEP.
	 * AUTO: Automatic, probably for older Chips/Firmware.
	 */
	if (ic->ic_flags & IEEE80211_F_PSK) {
		struct bwfm_wsec_pmk pmk;
		uint32_t wsec = 0;
		uint32_t wpa = 0;
		int i;

		pmk.key_len = htole16(sizeof(ic->ic_psk) << 1);
		pmk.flags = htole16(BWFM_WSEC_PASSPHRASE);
		for (i = 0; i < 32; i++)
			snprintf(&pmk.key[2 * i], 3, "%02x",
			    ic->ic_psk[i]);
		bwfm_fwvar_cmd_set_data(sc, BWFM_C_SET_WSEC_PMK, &pmk,
		    sizeof(pmk));

		if (ic->ic_rsnprotos & IEEE80211_PROTO_WPA) {
			if (ic->ic_rsnakms & IEEE80211_AKM_PSK)
				wpa |= BWFM_WPA_AUTH_WPA_PSK;
			if (ic->ic_rsnakms & IEEE80211_AKM_8021X)
				wpa |= BWFM_WPA_AUTH_WPA_UNSPECIFIED;
		}
		if (ic->ic_rsnprotos & IEEE80211_PROTO_RSN) {
			if (ic->ic_rsnakms & IEEE80211_AKM_PSK)
				wpa |= BWFM_WPA_AUTH_WPA2_PSK;
			if (ic->ic_rsnakms & IEEE80211_AKM_SHA256_PSK)
				wpa |= BWFM_WPA_AUTH_WPA2_PSK_SHA256;
			if (ic->ic_rsnakms & IEEE80211_AKM_8021X)
				wpa |= BWFM_WPA_AUTH_WPA2_UNSPECIFIED;
			if (ic->ic_rsnakms & IEEE80211_AKM_SHA256_8021X)
				wpa |= BWFM_WPA_AUTH_WPA2_1X_SHA256;
		}
		if (ic->ic_rsnciphers & IEEE80211_WPA_CIPHER_TKIP ||
		    ic->ic_rsngroupcipher & IEEE80211_WPA_CIPHER_TKIP)
			wsec |= BWFM_WSEC_TKIP;
		if (ic->ic_rsnciphers & IEEE80211_WPA_CIPHER_CCMP ||
		    ic->ic_rsngroupcipher & IEEE80211_WPA_CIPHER_CCMP)
			wsec |= BWFM_WSEC_AES;

		bwfm_fwvar_var_set_int(sc, "wpa_auth", wpa);
		bwfm_fwvar_var_set_int(sc, "wsec", wsec);
	} else {
		bwfm_fwvar_var_set_int(sc, "wpa_auth", BWFM_WPA_AUTH_DISABLED);
		bwfm_fwvar_var_set_int(sc, "wsec", BWFM_WSEC_NONE);
	}
	bwfm_fwvar_var_set_int(sc, "auth", BWFM_AUTH_OPEN);
	bwfm_fwvar_var_set_int(sc, "mfp", BWFM_MFP_NONE);

	if (ic->ic_des_esslen && ic->ic_des_esslen < BWFM_MAX_SSID_LEN) {
		params = malloc(sizeof(*params), M_TEMP, M_WAITOK | M_ZERO);
		memcpy(params->ssid.ssid, ic->ic_des_essid, ic->ic_des_esslen);
		params->ssid.len = htole32(ic->ic_des_esslen);
		memset(params->assoc.bssid, 0xff, sizeof(params->assoc.bssid));
		params->scan.scan_type = -1;
		params->scan.nprobes = htole32(-1);
		params->scan.active_time = htole32(-1);
		params->scan.passive_time = htole32(-1);
		params->scan.home_time = htole32(-1);
		if (bwfm_fwvar_var_set_data(sc, "join", params, sizeof(*params))) {
			struct bwfm_join_params join;
			memset(&join, 0, sizeof(join));
			memcpy(join.ssid.ssid, ic->ic_des_essid, ic->ic_des_esslen);
			join.ssid.len = htole32(ic->ic_des_esslen);
			memset(join.assoc.bssid, 0xff, sizeof(join.assoc.bssid));
			bwfm_fwvar_cmd_set_data(sc, BWFM_C_SET_SSID, &join,
			    sizeof(join));
		}
		free(params, M_TEMP, sizeof(*params));
	}

	/* XXX: added for testing only, remove */
	bwfm_fwvar_var_set_int(sc, "allmulti", 1);
	bwfm_fwvar_cmd_set_int(sc, BWFM_C_SET_PROMISC, 1);

	ifp->if_flags |= IFF_RUNNING;
	ifq_clr_oactive(&ifp->if_snd);
}

void
bwfm_stop(struct ifnet *ifp)
{
	struct bwfm_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;

	sc->sc_tx_timer = 0;
	ifp->if_timer = 0;
	ifp->if_flags &= ~IFF_RUNNING;
	ifq_clr_oactive(&ifp->if_snd);

	/* In case we were scanning, release the scan "lock". */
	ic->ic_scan_lock = IEEE80211_SCAN_UNLOCKED;

	bwfm_fwvar_cmd_set_int(sc, BWFM_C_DOWN, 1);
	bwfm_fwvar_cmd_set_int(sc, BWFM_C_SET_PM, 0);
}

void
bwfm_watchdog(struct ifnet *ifp)
{
	struct bwfm_softc *sc = ifp->if_softc;

	ifp->if_timer = 0;

	if (sc->sc_tx_timer > 0) {
		if (--sc->sc_tx_timer == 0) {
			printf("%s: device timeout\n", DEVNAME(sc));
			ifp->if_oerrors++;
			return;
		}
		ifp->if_timer = 1;
	}
	ieee80211_watchdog(ifp);
}

int
bwfm_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct bwfm_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_nwid nwid;
	struct ieee80211_wpapsk *psk;
	struct ieee80211_nodereq *nr, nrbuf;
	struct ieee80211_nodereq_all *na;
	struct ieee80211_node *ni;
	int s, i, error = 0;
	uint32_t flags;

	s = splnet();
	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		bwfm_init(ifp);
		break;
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (!(ifp->if_flags & IFF_RUNNING))
				bwfm_init(ifp);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				bwfm_stop(ifp);
		}
		break;
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_media, cmd);
		break;
	case SIOCG80211ALLNODES:
		na = (struct ieee80211_nodereq_all *)data;
		na->na_nodes = i = 0;
		ni = RBT_MIN(ieee80211_tree, &ic->ic_tree);
		while (ni && na->na_size >=
		    i + sizeof(struct ieee80211_nodereq)) {
			ieee80211_node2req(ic, ni, &nrbuf);
			error = copyout(&nrbuf, (caddr_t)na->na_node + i,
			    sizeof(struct ieee80211_nodereq));
			if (error)
				break;
			i += sizeof(struct ieee80211_nodereq);
			na->na_nodes++;
			ni = RBT_NEXT(ieee80211_tree, ni);
		}
		break;
	case SIOCG80211FLAGS:
		flags = ic->ic_flags;
#ifndef IEEE80211_STA_ONLY
		if (ic->ic_opmode != IEEE80211_M_HOSTAP)
#endif
			flags &= ~IEEE80211_F_HOSTAPMASK;
		ifr->ifr_flags = flags >> IEEE80211_F_USERSHIFT;
		break;
	case SIOCS80211FLAGS:
		if ((error = suser(curproc, 0)) != 0)
			break;
		flags = (u_int32_t)ifr->ifr_flags << IEEE80211_F_USERSHIFT;
		if (
#ifndef IEEE80211_STA_ONLY
		    ic->ic_opmode != IEEE80211_M_HOSTAP &&
#endif
		    (flags & IEEE80211_F_HOSTAPMASK)) {
			error = EINVAL;
			break;
		}
		ic->ic_flags = (ic->ic_flags & ~IEEE80211_F_USERMASK) | flags;
		error = ENETRESET;
		break;
	case SIOCG80211NODE:
		nr = (struct ieee80211_nodereq *)data;
		ni = ieee80211_find_node(ic, nr->nr_macaddr);
		if (ni == NULL) {
			error = ENOENT;
			break;
		}
		ieee80211_node2req(ic, ni, nr);
		break;
	case SIOCS80211NODE:
		if ((error = suser(curproc, 0)) != 0)
			break;
#ifndef IEEE80211_STA_ONLY
		if (ic->ic_opmode == IEEE80211_M_HOSTAP) {
			error = EINVAL;
			break;
		}
#endif
		nr = (struct ieee80211_nodereq *)data;

		ni = ieee80211_find_node(ic, nr->nr_macaddr);
		if (ni == NULL)
			ni = ieee80211_alloc_node(ic, nr->nr_macaddr);
		if (ni == NULL) {
			error = ENOENT;
			break;
		}

		if (nr->nr_flags & IEEE80211_NODEREQ_COPY)
			ieee80211_req2node(ic, nr, ni);
		break;
	case SIOCS80211NWID:
		if ((error = suser(curproc, 0)) != 0)
			break;
		if ((error = copyin(ifr->ifr_data, &nwid, sizeof(nwid))) != 0)
			break;
		if (nwid.i_len > IEEE80211_NWID_LEN) {
			error = EINVAL;
			break;
		}
		memset(ic->ic_des_essid, 0, IEEE80211_NWID_LEN);
		ic->ic_des_esslen = nwid.i_len;
		memcpy(ic->ic_des_essid, nwid.i_nwid, nwid.i_len);
		error = ENETRESET;
		break;
	case SIOCG80211NWID:
		memset(&nwid, 0, sizeof(nwid));
		nwid.i_len = ic->ic_des_esslen;
		memcpy(nwid.i_nwid, ic->ic_des_essid, nwid.i_len);
		error = copyout(&nwid, ifr->ifr_data, sizeof(nwid));
		break;
	case SIOCS80211SCAN:
		if ((error = suser(curproc, 0)) != 0)
			break;
		if (ic->ic_opmode == IEEE80211_M_HOSTAP)
			break;
		if (ic->ic_scan_lock == IEEE80211_SCAN_UNLOCKED) {
			ieee80211_clean_cached(ic);
			bwfm_scan(sc);
			timeout_set(&sc->sc_scan_timeout,
			    bwfm_scan_timeout, sc);
			timeout_add_msec(&sc->sc_scan_timeout,
			    10000);
		}
		/* Let the userspace process wait for completion */
		error = tsleep(&ic->ic_scan_lock, PCATCH, "80211scan",
		    hz * IEEE80211_SCAN_TIMEOUT);
		break;
	case SIOCS80211WPAPSK:
		if ((error = suser(curproc, 0)) != 0)
			break;
		psk = (struct ieee80211_wpapsk *)data;
		if (psk->i_enabled) {
			ic->ic_flags |= IEEE80211_F_PSK;
			memcpy(ic->ic_psk, psk->i_psk, sizeof(ic->ic_psk));
		} else {
			ic->ic_flags &= ~IEEE80211_F_PSK;
			memset(ic->ic_psk, 0, sizeof(ic->ic_psk));
		}
		error = ENETRESET;
		break;
	case SIOCG80211WPAPSK:
		psk = (struct ieee80211_wpapsk *)data;
		if (ic->ic_flags & IEEE80211_F_PSK) {
			psk->i_enabled = 1;
			/* do not show any keys to non-root user */
			if (suser(curproc, 0) != 0) {
				psk->i_enabled = 2;
				memset(psk->i_psk, 0, sizeof(psk->i_psk));
				break;  /* return ok but w/o key */
			}
			memcpy(psk->i_psk, ic->ic_psk, sizeof(psk->i_psk));
		} else
			psk->i_enabled = 0;
		break;
	case SIOCG80211ALLCHANS:
	case SIOCG80211STATS:
	case SIOCS80211WPAPARMS:
	case SIOCG80211WPAPARMS:
		error = ieee80211_ioctl(ifp, cmd, data);
		break;
	case SIOCS80211NWKEY:
	case SIOCG80211NWKEY:
	case SIOCS80211WMMPARMS:
	case SIOCG80211WMMPARMS:
	case SIOCS80211KEYAVAIL:
	case SIOCS80211KEYRUN:
	case SIOCS80211POWER:
	case SIOCG80211POWER:
	case SIOCS80211BSSID:
	case SIOCG80211BSSID:
	case SIOCS80211CHANNEL:
	case SIOCG80211CHANNEL:
	case SIOCS80211TXPOWER:
	case SIOCG80211TXPOWER:
	case SIOCS80211DELNODE:
	default:
		error = ether_ioctl(ifp, &sc->sc_ic.ic_ac, cmd, data);
	}
	if (error == ENETRESET) {
		if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) ==
		    (IFF_UP | IFF_RUNNING)) {
			bwfm_stop(ifp);
			bwfm_init(ifp);
		}
		error = 0;
	}
	splx(s);
	return error;
}

int
bwfm_media_change(struct ifnet *ifp)
{
	return 0;
}

void
bwfm_media_status(struct ifnet *ifp, struct ifmediareq *imr)
{
}

/* Chip initialization (SDIO, PCIe) */
int
bwfm_chip_attach(struct bwfm_softc *sc)
{
	struct bwfm_core *core;
	int need_socram = 0;
	int has_socram = 0;
	int cpu_found = 0;
	uint32_t val;

	LIST_INIT(&sc->sc_chip.ch_list);

	if (sc->sc_buscore_ops->bc_prepare(sc) != 0) {
		printf("%s: failed buscore prepare\n", DEVNAME(sc));
		return 1;
	}

	val = sc->sc_buscore_ops->bc_read(sc,
	    BWFM_CHIP_BASE + BWFM_CHIP_REG_CHIPID);
	sc->sc_chip.ch_chip = BWFM_CHIP_CHIPID_ID(val);
	sc->sc_chip.ch_chiprev = BWFM_CHIP_CHIPID_REV(val);

	if ((sc->sc_chip.ch_chip > 0xa000) || (sc->sc_chip.ch_chip < 0x4000))
		snprintf(sc->sc_chip.ch_name, sizeof(sc->sc_chip.ch_name),
		    "%d", sc->sc_chip.ch_chip);
	else
		snprintf(sc->sc_chip.ch_name, sizeof(sc->sc_chip.ch_name),
		    "%x", sc->sc_chip.ch_chip);

	switch (BWFM_CHIP_CHIPID_TYPE(val))
	{
	case BWFM_CHIP_CHIPID_TYPE_SOCI_SB:
		printf("%s: SoC interconnect SB not implemented\n",
		    DEVNAME(sc));
		return 1;
	case BWFM_CHIP_CHIPID_TYPE_SOCI_AI:
		sc->sc_chip.ch_core_isup = bwfm_chip_ai_isup;
		sc->sc_chip.ch_core_disable = bwfm_chip_ai_disable;
		sc->sc_chip.ch_core_reset = bwfm_chip_ai_reset;
		bwfm_chip_dmp_erom_scan(sc);
		break;
	default:
		printf("%s: SoC interconnect %d unknown\n",
		    DEVNAME(sc), BWFM_CHIP_CHIPID_TYPE(val));
		return 1;
	}

	LIST_FOREACH(core, &sc->sc_chip.ch_list, co_link) {
		DPRINTF(("%s: 0x%x:%-2d base 0x%08x wrap 0x%08x\n",
		    DEVNAME(sc), core->co_id, core->co_rev,
		    core->co_base, core->co_wrapbase));

		switch (core->co_id) {
		case BWFM_AGENT_CORE_ARM_CM3:
			need_socram = true;
			/* FALLTHROUGH */
		case BWFM_AGENT_CORE_ARM_CR4:
		case BWFM_AGENT_CORE_ARM_CA7:
			cpu_found = true;
			break;
		case BWFM_AGENT_INTERNAL_MEM:
			has_socram = true;
			break;
		default:
			break;
		}
	}

	if (!cpu_found) {
		printf("%s: CPU core not detected\n", DEVNAME(sc));
		return 1;
	}
	if (need_socram && !has_socram) {
		printf("%s: RAM core not provided\n", DEVNAME(sc));
		return 1;
	}

	if (bwfm_chip_get_core(sc, BWFM_AGENT_CORE_ARM_CR4) != NULL)
		bwfm_chip_cr4_set_passive(sc);
	if (bwfm_chip_get_core(sc, BWFM_AGENT_CORE_ARM_CA7) != NULL)
		bwfm_chip_ca7_set_passive(sc);
	if (bwfm_chip_get_core(sc, BWFM_AGENT_CORE_ARM_CM3) != NULL)
		bwfm_chip_cm3_set_passive(sc);

	if (sc->sc_buscore_ops->bc_reset) {
		sc->sc_buscore_ops->bc_reset(sc);
		if (bwfm_chip_get_core(sc, BWFM_AGENT_CORE_ARM_CR4) != NULL)
			bwfm_chip_cr4_set_passive(sc);
		if (bwfm_chip_get_core(sc, BWFM_AGENT_CORE_ARM_CA7) != NULL)
			bwfm_chip_ca7_set_passive(sc);
		if (bwfm_chip_get_core(sc, BWFM_AGENT_CORE_ARM_CM3) != NULL)
			bwfm_chip_cm3_set_passive(sc);
	}

	/* TODO: get raminfo */

	core = bwfm_chip_get_core(sc, BWFM_AGENT_CORE_CHIPCOMMON);
	sc->sc_chip.ch_cc_caps = sc->sc_buscore_ops->bc_read(sc,
	    core->co_base + BWFM_CHIP_REG_CAPABILITIES);
	sc->sc_chip.ch_cc_caps_ext = sc->sc_buscore_ops->bc_read(sc,
	    core->co_base + BWFM_CHIP_REG_CAPABILITIES_EXT);

	core = bwfm_chip_get_pmu(sc);
	if (sc->sc_chip.ch_cc_caps & BWFM_CHIP_REG_CAPABILITIES_PMU) {
		sc->sc_chip.ch_pmucaps = sc->sc_buscore_ops->bc_read(sc,
		    core->co_base + BWFM_CHIP_REG_PMUCAPABILITIES);
		sc->sc_chip.ch_pmurev = sc->sc_chip.ch_pmucaps &
		    BWFM_CHIP_REG_PMUCAPABILITIES_REV_MASK;
	}

	if (sc->sc_buscore_ops->bc_setup)
		sc->sc_buscore_ops->bc_setup(sc);

	return 0;
}

struct bwfm_core *
bwfm_chip_get_core(struct bwfm_softc *sc, int id)
{
	struct bwfm_core *core;

	LIST_FOREACH(core, &sc->sc_chip.ch_list, co_link) {
		if (core->co_id == id)
			return core;
	}

	return NULL;
}

struct bwfm_core *
bwfm_chip_get_pmu(struct bwfm_softc *sc)
{
	struct bwfm_core *cc, *pmu;

	cc = bwfm_chip_get_core(sc, BWFM_AGENT_CORE_CHIPCOMMON);
	if (cc->co_rev >= 35 && sc->sc_chip.ch_cc_caps_ext &
	    BWFM_CHIP_REG_CAPABILITIES_EXT_AOB_PRESENT) {
		pmu = bwfm_chip_get_core(sc, BWFM_AGENT_CORE_PMU);
		if (pmu)
			return pmu;
	}

	return cc;
}

/* Functions for the AI interconnect */
int
bwfm_chip_ai_isup(struct bwfm_softc *sc, struct bwfm_core *core)
{
	uint32_t ioctl, reset;

	ioctl = sc->sc_buscore_ops->bc_read(sc,
	    core->co_wrapbase + BWFM_AGENT_IOCTL);
	reset = sc->sc_buscore_ops->bc_read(sc,
	    core->co_wrapbase + BWFM_AGENT_RESET_CTL);

	if (((ioctl & (BWFM_AGENT_IOCTL_FGC | BWFM_AGENT_IOCTL_CLK)) ==
	    BWFM_AGENT_IOCTL_CLK) &&
	    ((reset & BWFM_AGENT_RESET_CTL_RESET) == 0))
		return 1;

	return 0;
}

void
bwfm_chip_ai_disable(struct bwfm_softc *sc, struct bwfm_core *core,
    uint32_t prereset, uint32_t reset)
{
	uint32_t val;
	int i;

	val = sc->sc_buscore_ops->bc_read(sc,
	    core->co_wrapbase + BWFM_AGENT_RESET_CTL);
	if ((val & BWFM_AGENT_RESET_CTL_RESET) == 0) {

		sc->sc_buscore_ops->bc_write(sc,
		    core->co_wrapbase + BWFM_AGENT_IOCTL,
		    prereset | BWFM_AGENT_IOCTL_FGC | BWFM_AGENT_IOCTL_CLK);
		sc->sc_buscore_ops->bc_read(sc,
		    core->co_wrapbase + BWFM_AGENT_IOCTL);

		sc->sc_buscore_ops->bc_write(sc,
		    core->co_wrapbase + BWFM_AGENT_RESET_CTL,
		    BWFM_AGENT_RESET_CTL_RESET);
		delay(20);

		for (i = 300; i > 0; i--) {
			if (sc->sc_buscore_ops->bc_read(sc,
			    core->co_wrapbase + BWFM_AGENT_RESET_CTL) ==
			    BWFM_AGENT_RESET_CTL_RESET)
				break;
		}
		if (i == 0)
			printf("%s: timeout on core reset\n", DEVNAME(sc));
	}

	sc->sc_buscore_ops->bc_write(sc,
	    core->co_wrapbase + BWFM_AGENT_IOCTL,
	    reset | BWFM_AGENT_IOCTL_FGC | BWFM_AGENT_IOCTL_CLK);
	sc->sc_buscore_ops->bc_read(sc,
	    core->co_wrapbase + BWFM_AGENT_IOCTL);
}

void
bwfm_chip_ai_reset(struct bwfm_softc *sc, struct bwfm_core *core,
    uint32_t prereset, uint32_t reset, uint32_t postreset)
{
	int i;

	bwfm_chip_ai_disable(sc, core, prereset, reset);

	for (i = 50; i > 0; i--) {
		if ((sc->sc_buscore_ops->bc_read(sc,
		    core->co_wrapbase + BWFM_AGENT_RESET_CTL) &
		    BWFM_AGENT_RESET_CTL_RESET) == 0)
			break;
		sc->sc_buscore_ops->bc_write(sc,
		    core->co_wrapbase + BWFM_AGENT_RESET_CTL, 0);
		delay(60);
	}
	if (i == 0)
		printf("%s: timeout on core reset\n", DEVNAME(sc));

	sc->sc_buscore_ops->bc_write(sc,
	    core->co_wrapbase + BWFM_AGENT_IOCTL,
	    postreset | BWFM_AGENT_IOCTL_CLK);
	sc->sc_buscore_ops->bc_read(sc,
	    core->co_wrapbase + BWFM_AGENT_IOCTL);
}

void
bwfm_chip_dmp_erom_scan(struct bwfm_softc *sc)
{
	uint32_t erom, val, base, wrap;
	uint8_t type = 0;
	uint16_t id;
	uint8_t nmw, nsw, rev;
	struct bwfm_core *core;

	erom = sc->sc_buscore_ops->bc_read(sc,
	    BWFM_CHIP_BASE + BWFM_CHIP_REG_EROMPTR);
	while (type != BWFM_DMP_DESC_EOT) {
		val = sc->sc_buscore_ops->bc_read(sc, erom);
		type = val & BWFM_DMP_DESC_MASK;
		erom += 4;

		if (type != BWFM_DMP_DESC_COMPONENT)
			continue;

		id = (val & BWFM_DMP_COMP_PARTNUM)
		    >> BWFM_DMP_COMP_PARTNUM_S;

		val = sc->sc_buscore_ops->bc_read(sc, erom);
		type = val & BWFM_DMP_DESC_MASK;
		erom += 4;

		if (type != BWFM_DMP_DESC_COMPONENT) {
			printf("%s: not component descriptor\n", DEVNAME(sc));
			return;
		}

		nmw = (val & BWFM_DMP_COMP_NUM_MWRAP)
		    >> BWFM_DMP_COMP_NUM_MWRAP_S;
		nsw = (val & BWFM_DMP_COMP_NUM_SWRAP)
		    >> BWFM_DMP_COMP_NUM_SWRAP_S;
		rev = (val & BWFM_DMP_COMP_REVISION)
		    >> BWFM_DMP_COMP_REVISION_S;

		if (nmw + nsw == 0 && id != BWFM_AGENT_CORE_PMU)
			continue;

		if (bwfm_chip_dmp_get_regaddr(sc, &erom, &base, &wrap))
			continue;

		core = malloc(sizeof(*core), M_DEVBUF, M_WAITOK);
		core->co_id = id;
		core->co_base = base;
		core->co_wrapbase = wrap;
		core->co_rev = rev;
		LIST_INSERT_HEAD(&sc->sc_chip.ch_list, core, co_link);
	}
}

int
bwfm_chip_dmp_get_regaddr(struct bwfm_softc *sc, uint32_t *erom,
    uint32_t *base, uint32_t *wrap)
{
	uint8_t type = 0, mpnum = 0;
	uint8_t stype, sztype, wraptype;
	uint32_t val;

	*base = 0;
	*wrap = 0;

	val = sc->sc_buscore_ops->bc_read(sc, *erom);
	type = val & BWFM_DMP_DESC_MASK;
	if (type == BWFM_DMP_DESC_MASTER_PORT) {
		mpnum = (val & BWFM_DMP_MASTER_PORT_NUM)
		    >> BWFM_DMP_MASTER_PORT_NUM_S;
		wraptype = BWFM_DMP_SLAVE_TYPE_MWRAP;
		*erom += 4;
	} else if ((type & ~BWFM_DMP_DESC_ADDRSIZE_GT32) ==
	    BWFM_DMP_DESC_ADDRESS)
		wraptype = BWFM_DMP_SLAVE_TYPE_SWRAP;
	else
		return 1;

	do {
		do {
			val = sc->sc_buscore_ops->bc_read(sc, *erom);
			type = val & BWFM_DMP_DESC_MASK;
			if (type == BWFM_DMP_DESC_COMPONENT)
				return 0;
			if (type == BWFM_DMP_DESC_EOT)
				return 1;
			*erom += 4;
		} while ((type & ~BWFM_DMP_DESC_ADDRSIZE_GT32) !=
		     BWFM_DMP_DESC_ADDRESS);

		if (type & BWFM_DMP_DESC_ADDRSIZE_GT32)
			*erom += 4;

		sztype = (val & BWFM_DMP_SLAVE_SIZE_TYPE)
		    >> BWFM_DMP_SLAVE_SIZE_TYPE_S;
		if (sztype == BWFM_DMP_SLAVE_SIZE_DESC) {
			val = sc->sc_buscore_ops->bc_read(sc, *erom);
			type = val & BWFM_DMP_DESC_MASK;
			if (type & BWFM_DMP_DESC_ADDRSIZE_GT32)
				*erom += 8;
			else
				*erom += 4;
		}
		if (sztype != BWFM_DMP_SLAVE_SIZE_4K)
			continue;

		stype = (val & BWFM_DMP_SLAVE_TYPE) >> BWFM_DMP_SLAVE_TYPE_S;
		if (*base == 0 && stype == BWFM_DMP_SLAVE_TYPE_SLAVE)
			*base = val & BWFM_DMP_SLAVE_ADDR_BASE;
		if (*wrap == 0 && stype == wraptype)
			*wrap = val & BWFM_DMP_SLAVE_ADDR_BASE;
	} while (*base == 0 || *wrap == 0);

	return 0;
}

/* Core configuration */
void
bwfm_chip_cr4_set_passive(struct bwfm_softc *sc)
{
	panic("%s: CR4 not supported", DEVNAME(sc));
}

void
bwfm_chip_ca7_set_passive(struct bwfm_softc *sc)
{
	panic("%s: CA7 not supported", DEVNAME(sc));
}

void
bwfm_chip_cm3_set_passive(struct bwfm_softc *sc)
{
	struct bwfm_core *core;

	core = bwfm_chip_get_core(sc, BWFM_AGENT_CORE_ARM_CM3);
	sc->sc_chip.ch_core_disable(sc, core, 0, 0);
	core = bwfm_chip_get_core(sc, BWFM_AGENT_CORE_80211);
	sc->sc_chip.ch_core_reset(sc, core, BWFM_AGENT_D11_IOCTL_PHYRESET |
	    BWFM_AGENT_D11_IOCTL_PHYCLOCKEN, BWFM_AGENT_D11_IOCTL_PHYCLOCKEN,
	    BWFM_AGENT_D11_IOCTL_PHYCLOCKEN);
	core = bwfm_chip_get_core(sc, BWFM_AGENT_INTERNAL_MEM);
	sc->sc_chip.ch_core_reset(sc, core, 0, 0, 0);

	if (sc->sc_chip.ch_chip == BRCM_CC_43430_CHIP_ID) {
		sc->sc_buscore_ops->bc_write(sc,
		    core->co_base + BWFM_SOCRAM_BANKIDX, 3);
		sc->sc_buscore_ops->bc_write(sc,
		    core->co_base + BWFM_SOCRAM_BANKPDA, 0);
	}
}

/* BCDC protocol implementation */
int
bwfm_proto_bcdc_query_dcmd(struct bwfm_softc *sc, int ifidx,
    int cmd, char *buf, size_t *len)
{
	struct bwfm_proto_bcdc_dcmd *dcmd;
	size_t size = sizeof(dcmd->hdr) + *len;
	static int reqid = 0;
	int ret = 1;

	reqid++;

	dcmd = malloc(sizeof(*dcmd), M_TEMP, M_WAITOK | M_ZERO);
	if (*len > sizeof(dcmd->buf))
		goto err;

	dcmd->hdr.cmd = htole32(cmd);
	dcmd->hdr.len = htole32(*len);
	dcmd->hdr.flags |= BWFM_BCDC_DCMD_GET;
	dcmd->hdr.flags |= BWFM_BCDC_DCMD_ID_SET(reqid);
	dcmd->hdr.flags |= BWFM_BCDC_DCMD_IF_SET(ifidx);
	dcmd->hdr.flags = htole32(dcmd->hdr.flags);
	memcpy(&dcmd->buf, buf, *len);

	if (sc->sc_bus_ops->bs_txctl(sc, (void *)dcmd,
	     sizeof(dcmd->hdr) + *len)) {
		DPRINTF(("%s: tx failed\n", DEVNAME(sc)));
		goto err;
	}

	do {
		if (sc->sc_bus_ops->bs_rxctl(sc, (void *)dcmd, &size)) {
			DPRINTF(("%s: rx failed\n", DEVNAME(sc)));
			goto err;
		}
		dcmd->hdr.cmd = letoh32(dcmd->hdr.cmd);
		dcmd->hdr.len = letoh32(dcmd->hdr.len);
		dcmd->hdr.flags = letoh32(dcmd->hdr.flags);
		dcmd->hdr.status = letoh32(dcmd->hdr.status);
	} while (BWFM_BCDC_DCMD_ID_GET(dcmd->hdr.flags) != reqid);

	if (BWFM_BCDC_DCMD_ID_GET(dcmd->hdr.flags) != reqid) {
		printf("%s: unexpected request id\n", DEVNAME(sc));
		goto err;
	}

	if (buf) {
		if (size > *len)
			size = *len;
		if (size < *len)
			*len = size;
		memcpy(buf, dcmd->buf, *len);
	}

	if (dcmd->hdr.flags & BWFM_BCDC_DCMD_ERROR)
		ret = dcmd->hdr.status;
	else
		ret = 0;
err:
	free(dcmd, M_TEMP, sizeof(*dcmd));
	return ret;
}

int
bwfm_proto_bcdc_set_dcmd(struct bwfm_softc *sc, int ifidx,
    int cmd, char *buf, size_t len)
{
	struct bwfm_proto_bcdc_dcmd *dcmd;
	size_t size = sizeof(dcmd->hdr) + len;
	int reqid = 0;
	int ret = 1;

	reqid++;

	dcmd = malloc(sizeof(*dcmd), M_TEMP, M_WAITOK | M_ZERO);
	if (len > sizeof(dcmd->buf))
		goto err;

	dcmd->hdr.cmd = htole32(cmd);
	dcmd->hdr.len = htole32(len);
	dcmd->hdr.flags |= BWFM_BCDC_DCMD_SET;
	dcmd->hdr.flags |= BWFM_BCDC_DCMD_ID_SET(reqid);
	dcmd->hdr.flags |= BWFM_BCDC_DCMD_IF_SET(ifidx);
	dcmd->hdr.flags = htole32(dcmd->hdr.flags);
	memcpy(&dcmd->buf, buf, len);

	if (sc->sc_bus_ops->bs_txctl(sc, (void *)dcmd, size)) {
		DPRINTF(("%s: tx failed\n", DEVNAME(sc)));
		goto err;
	}

	do {
		if (sc->sc_bus_ops->bs_rxctl(sc, (void *)dcmd, &size)) {
			DPRINTF(("%s: rx failed\n", DEVNAME(sc)));
			goto err;
		}
		dcmd->hdr.cmd = letoh32(dcmd->hdr.cmd);
		dcmd->hdr.len = letoh32(dcmd->hdr.len);
		dcmd->hdr.flags = letoh32(dcmd->hdr.flags);
		dcmd->hdr.status = letoh32(dcmd->hdr.status);
	} while (BWFM_BCDC_DCMD_ID_GET(dcmd->hdr.flags) != reqid);

	if (BWFM_BCDC_DCMD_ID_GET(dcmd->hdr.flags) != reqid) {
		printf("%s: unexpected request id\n", DEVNAME(sc));
		goto err;
	}

	if (dcmd->hdr.flags & BWFM_BCDC_DCMD_ERROR)
		return dcmd->hdr.status;

	ret = 0;
err:
	free(dcmd, M_TEMP, sizeof(*dcmd));
	return ret;
}

/* FW Variable code */
int
bwfm_fwvar_cmd_get_data(struct bwfm_softc *sc, int cmd, void *data, size_t len)
{
	return sc->sc_proto_ops->proto_query_dcmd(sc, 0, cmd, data, &len);
}

int
bwfm_fwvar_cmd_set_data(struct bwfm_softc *sc, int cmd, void *data, size_t len)
{
	return sc->sc_proto_ops->proto_set_dcmd(sc, 0, cmd, data, len);
}

int
bwfm_fwvar_cmd_get_int(struct bwfm_softc *sc, int cmd, uint32_t *data)
{
	int ret;
	ret = bwfm_fwvar_cmd_get_data(sc, cmd, data, sizeof(*data));
	*data = letoh32(*data);
	return ret;
}

int
bwfm_fwvar_cmd_set_int(struct bwfm_softc *sc, int cmd, uint32_t data)
{
	data = htole32(data);
	return bwfm_fwvar_cmd_set_data(sc, cmd, &data, sizeof(data));
}

int
bwfm_fwvar_var_get_data(struct bwfm_softc *sc, char *name, void *data, size_t len)
{
	char *buf;
	int ret;

	buf = malloc(strlen(name) + 1 + len, M_TEMP, M_WAITOK);
	memcpy(buf, name, strlen(name) + 1);
	memcpy(buf + strlen(name) + 1, data, len);
	ret = bwfm_fwvar_cmd_get_data(sc, BWFM_C_GET_VAR,
	    buf, strlen(name) + 1 + len);
	memcpy(data, buf, len);
	free(buf, M_TEMP, strlen(name) + 1 + len);
	return ret;
}

int
bwfm_fwvar_var_set_data(struct bwfm_softc *sc, char *name, void *data, size_t len)
{
	char *buf;
	int ret;

	buf = malloc(strlen(name) + 1 + len, M_TEMP, M_WAITOK);
	memcpy(buf, name, strlen(name) + 1);
	memcpy(buf + strlen(name) + 1, data, len);
	ret = bwfm_fwvar_cmd_set_data(sc, BWFM_C_SET_VAR,
	    buf, strlen(name) + 1 + len);
	free(buf, M_TEMP, strlen(name) + 1 + len);
	return ret;
}

int
bwfm_fwvar_var_get_int(struct bwfm_softc *sc, char *name, uint32_t *data)
{
	int ret;
	ret = bwfm_fwvar_var_get_data(sc, name, data, sizeof(*data));
	*data = letoh32(*data);
	return ret;
}

int
bwfm_fwvar_var_set_int(struct bwfm_softc *sc, char *name, uint32_t data)
{
	data = htole32(data);
	return bwfm_fwvar_var_set_data(sc, name, &data, sizeof(data));
}

/* 802.11 code */
void
bwfm_scan(struct bwfm_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct bwfm_escan_params *params;
	uint32_t nssid = 0, nchannel = 0;
	size_t params_size;

	ic->ic_scan_lock = IEEE80211_SCAN_LOCKED;

#if 0
	/* Active scan is used for scanning for an SSID */
	bwfm_fwvar_cmd_set_int(sc, BWFM_C_SET_PASSIVE_SCAN, 0);
#endif
	bwfm_fwvar_cmd_set_int(sc, BWFM_C_SET_PASSIVE_SCAN, 1);

	params_size = sizeof(*params);
	params_size += sizeof(uint32_t) * ((nchannel + 1) / 2);
	params_size += sizeof(struct bwfm_ssid) * nssid;

	params = malloc(params_size, M_TEMP, M_WAITOK | M_ZERO);
	memset(params->scan_params.bssid, 0xff,
	    sizeof(params->scan_params.bssid));
	params->scan_params.bss_type = 2;
	params->scan_params.nprobes = htole32(-1);
	params->scan_params.active_time = htole32(-1);
	params->scan_params.passive_time = htole32(-1);
	params->scan_params.home_time = htole32(-1);
	params->version = htole32(BWFM_ESCAN_REQ_VERSION);
	params->action = htole16(WL_ESCAN_ACTION_START);
	params->sync_id = htole16(0x1234);

#if 0
	/* Scan a specific channel */
	params->scan_params.channel_list[0] = htole16(
	    (1 & 0xff) << 0 |
	    (3 & 0x3) << 8 |
	    (2 & 0x3) << 10 |
	    (2 & 0x3) << 12
	    );
	params->scan_params.channel_num = htole32(
	    (1 & 0xffff) << 0
	    );
#endif

	bwfm_fwvar_var_set_data(sc, "escan", params, params_size);
	free(params, M_TEMP, params_size);
}

void
bwfm_scan_timeout(void *arg)
{
	struct bwfm_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	DPRINTF(("%s: scan timeout\n", DEVNAME(sc)));
	ic->ic_scan_lock = IEEE80211_SCAN_UNLOCKED;
	wakeup(&ic->ic_scan_lock);
}

void
bwfm_rx(struct bwfm_softc *sc, char *buf, size_t len)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct bwfm_event *e = (void *)buf;
	struct mbuf_list ml = MBUF_LIST_INITIALIZER();
	struct mbuf *m;
	char *mb;

	printf("%s: buf %p len %lu\n", __func__, buf, len);

	if (len >= sizeof(e->ehdr) &&
	    ntohs(e->ehdr.ether_type) == BWFM_ETHERTYPE_LINK_CTL &&
	    memcmp(BWFM_BRCM_OUI, e->hdr.oui, sizeof(e->hdr.oui)) == 0 &&
	    ntohs(e->hdr.usr_subtype) == BWFM_BRCM_SUBTYPE_EVENT)
		bwfm_rx_event(sc, buf, len);

	if (__predict_false(len > MCLBYTES))
		return;
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (__predict_false(m == NULL))
		return;
	if (len > MHLEN) {
		MCLGET(m, M_DONTWAIT);
		if (!(m->m_flags & M_EXT)) {
			m_free(m);
			return;
		}
	}
	mb = mtod(m, char *);
	memcpy(mb, buf, len);
	m->m_pkthdr.len = m->m_len = len;

	ml_enqueue(&ml, m);
	if_input(ifp, &ml);
}

void
bwfm_rx_event(struct bwfm_softc *sc, char *buf, size_t len)
{
	struct ieee80211com *ic = &sc->sc_ic;
	//struct ifnet *ifp = &ic->ic_if;
	struct bwfm_event *e = (void *)buf;

	if (ntohl(e->msg.event_type) >= BWFM_E_LAST)
		return;

	switch (ntohl(e->msg.event_type)) {
	case BWFM_E_ESCAN_RESULT: {
		struct bwfm_escan_results *res = (void *)(buf + sizeof(*e));
		struct bwfm_bss_info *bss;
		int i;
		if (ntohl(e->msg.status) != BWFM_E_STATUS_PARTIAL) {
			timeout_del(&sc->sc_scan_timeout);
			ic->ic_scan_lock = IEEE80211_SCAN_UNLOCKED;
			wakeup(&ic->ic_scan_lock);
			break;
		}
		len -= sizeof(*e);
		if (len < sizeof(*res) || len < letoh32(res->buflen)) {
			printf("%s: results too small\n", DEVNAME(sc));
			return;
		}
		len -= sizeof(*res);
		if (len < letoh16(res->bss_count) * sizeof(struct bwfm_bss_info)) {
			printf("%s: results too small\n", DEVNAME(sc));
			return;
		}
		bss = &res->bss_info[0];
		for (i = 0; i < letoh16(res->bss_count); i++) {
			bwfm_scan_node(sc, &res->bss_info[i], len);
			len -= sizeof(*bss) + letoh32(bss->length);
			bss = (void *)((char *)bss) + letoh32(bss->length);
			if (len <= 0)
				break;
		}
		break;
		}
	default:
		printf("%s: buf %p len %lu datalen %u code %u status %u"
		    " reason %u\n", __func__, buf, len, ntohl(e->msg.datalen),
		    ntohl(e->msg.event_type), ntohl(e->msg.status),
		    ntohl(e->msg.reason));
		break;
	}
}

void
bwfm_scan_node(struct bwfm_softc *sc, struct bwfm_bss_info *bss, size_t len)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni;
	int chan;

	ni = ieee80211_alloc_node(ic, bss->bssid);
	IEEE80211_ADDR_COPY(ni->ni_macaddr, bss->bssid);
	IEEE80211_ADDR_COPY(ni->ni_bssid, bss->bssid);
	ni->ni_esslen = min(bss->ssid_len, sizeof(bss->ssid));
	ni->ni_esslen = min(ni->ni_esslen, IEEE80211_NWID_LEN);
	memcpy(ni->ni_essid, bss->ssid, ni->ni_esslen);
	chan = (bss->chanspec >> BWFM_CHANSPEC_CHAN_SHIFT)
	    & BWFM_CHANSPEC_CHAN_MASK;
	ni->ni_chan = &ic->ic_channels[chan];
	ni->ni_rssi = letoh32(bss->rssi);
}
