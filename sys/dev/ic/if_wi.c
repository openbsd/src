/*	$OpenBSD: if_wi.c,v 1.79 2002/08/30 08:19:49 fgsch Exp $	*/

/*
 * Copyright (c) 1997, 1998, 1999
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	From: if_wi.c,v 1.7 1999/07/04 14:40:22 wpaul Exp $
 */

/*
 * Lucent WaveLAN/IEEE 802.11 driver for OpenBSD.
 *
 * Originally written by Bill Paul <wpaul@ctr.columbia.edu>
 * Electrical Engineering Department
 * Columbia University, New York City
 */

/*
 * The WaveLAN/IEEE adapter is the second generation of the WaveLAN
 * from Lucent. Unlike the older cards, the new ones are programmed
 * entirely via a firmware-driven controller called the Hermes.
 * Unfortunately, Lucent will not release the Hermes programming manual
 * without an NDA (if at all). What they do release is an API library
 * called the HCF (Hardware Control Functions) which is supposed to
 * do the device-specific operations of a device driver for you. The
 * publically available version of the HCF library (the 'HCF Light') is
 * a) extremely gross, b) lacks certain features, particularly support
 * for 802.11 frames, and c) is contaminated by the GNU Public License.
 *
 * This driver does not use the HCF or HCF Light at all. Instead, it
 * programs the Hermes controller directly, using information gleaned
 * from the HCF Light code and corresponding documentation.
 */

#define WI_HERMES_AUTOINC_WAR	/* Work around data write autoinc bug. */
#define WI_HERMES_STATS_WAR	/* Work around stats counter bug. */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/device.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#endif

#include <net/if_ieee80211.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <machine/bus.h>

#include <dev/rndvar.h>

#include <dev/ic/if_wireg.h>
#include <dev/ic/if_wi_ieee.h>
#include <dev/ic/if_wivar.h>

#define BPF_MTAP(if,mbuf) bpf_mtap((if)->if_bpf, (mbuf))
#define BPFATTACH(if_bpf,if,dlt,sz)
#define STATIC

#ifdef WIDEBUG

u_int32_t	widebug = WIDEBUG;

#define WID_INTR	0x01
#define WID_START	0x02
#define WID_IOCTL	0x04
#define WID_INIT	0x08
#define WID_STOP	0x10
#define WID_RESET	0x20

#define DPRINTF(mask,args) if (widebug & (mask)) printf args;

#else	/* !WIDEBUG */
#define DPRINTF(mask,args)
#endif	/* WIDEBUG */

#if !defined(lint) && !defined(__OpenBSD__)
static const char rcsid[] =
	"$OpenBSD: if_wi.c,v 1.79 2002/08/30 08:19:49 fgsch Exp $";
#endif	/* lint */

#ifdef foo
static u_int8_t	wi_mcast_addr[6] = { 0x01, 0x60, 0x1D, 0x00, 0x01, 0x00 };
#endif

STATIC void wi_reset(struct wi_softc *);
STATIC int wi_ioctl(struct ifnet *, u_long, caddr_t);
STATIC void wi_start(struct ifnet *);
STATIC void wi_watchdog(struct ifnet *);
STATIC void wi_shutdown(void *);
STATIC void wi_rxeof(struct wi_softc *);
STATIC void wi_txeof(struct wi_softc *, int);
STATIC void wi_update_stats(struct wi_softc *);
STATIC void wi_setmulti(struct wi_softc *);

STATIC int wi_cmd(struct wi_softc *, int, int, int, int);
STATIC int wi_read_record(struct wi_softc *, struct wi_ltv_gen *);
STATIC int wi_write_record(struct wi_softc *, struct wi_ltv_gen *);
STATIC int wi_read_data(struct wi_softc *, int,
					int, caddr_t, int);
STATIC int wi_write_data(struct wi_softc *, int,
					int, caddr_t, int);
STATIC int wi_seek(struct wi_softc *, int, int, int);
STATIC int wi_alloc_nicmem(struct wi_softc *, int, int *);
STATIC void wi_inquire(void *);
STATIC int wi_setdef(struct wi_softc *, struct wi_req *);
STATIC void wi_get_id(struct wi_softc *);

STATIC int wi_media_change(struct ifnet *);
STATIC void wi_media_status(struct ifnet *, struct ifmediareq *);

STATIC int wi_set_ssid(struct ieee80211_nwid *, u_int8_t *, int);
STATIC int wi_set_nwkey(struct wi_softc *, struct ieee80211_nwkey *);
STATIC int wi_get_nwkey(struct wi_softc *, struct ieee80211_nwkey *);
STATIC int wi_sync_media(struct wi_softc *, int, int);
STATIC int wi_set_pm(struct wi_softc *, struct ieee80211_power *);
STATIC int wi_get_pm(struct wi_softc *, struct ieee80211_power *);

STATIC int wi_get_debug(struct wi_softc *, struct wi_req *);
STATIC int wi_set_debug(struct wi_softc *, struct wi_req *);

/* Autoconfig definition of driver back-end */
struct cfdriver wi_cd = {
	NULL, "wi", DV_IFNET
};

int
wi_attach(sc)
	struct wi_softc *sc;
{
	struct wi_ltv_macaddr	mac;
	struct wi_ltv_gen	gen;
	struct ifnet		*ifp;
	int			error;

	wi_cor_reset(sc);
	wi_reset(sc);

	/* Read the station address. */
	mac.wi_type = WI_RID_MAC_NODE;
	mac.wi_len = 4;
	error = wi_read_record(sc, (struct wi_ltv_gen *)&mac);
	if (error) {
		printf(": unable to read station address\n");
		return (error);
	}
	bcopy((char *)&mac.wi_mac_addr, (char *)&sc->sc_arpcom.ac_enaddr,
	    ETHER_ADDR_LEN);

	wi_get_id(sc);
	printf("address %s", ether_sprintf(sc->sc_arpcom.ac_enaddr));

	ifp = &sc->sc_arpcom.ac_if;
	bcopy(sc->sc_dev.dv_xname, ifp->if_xname, IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = wi_ioctl;
	ifp->if_start = wi_start;
	ifp->if_watchdog = wi_watchdog;
	ifp->if_baudrate = 10000000;
	IFQ_SET_READY(&ifp->if_snd);

	(void)wi_set_ssid(&sc->wi_node_name, WI_DEFAULT_NODENAME,
	    sizeof(WI_DEFAULT_NODENAME) - 1);
	(void)wi_set_ssid(&sc->wi_net_name, WI_DEFAULT_NETNAME,
	    sizeof(WI_DEFAULT_NETNAME) - 1);
	(void)wi_set_ssid(&sc->wi_ibss_name, WI_DEFAULT_IBSS,
	    sizeof(WI_DEFAULT_IBSS) - 1);

	sc->wi_portnum = WI_DEFAULT_PORT;
	sc->wi_ptype = WI_PORTTYPE_BSS;
	sc->wi_ap_density = WI_DEFAULT_AP_DENSITY;
	sc->wi_rts_thresh = WI_DEFAULT_RTS_THRESH;
	sc->wi_tx_rate = WI_DEFAULT_TX_RATE;
	sc->wi_max_data_len = WI_DEFAULT_DATALEN;
	sc->wi_create_ibss = WI_DEFAULT_CREATE_IBSS;
	sc->wi_pm_enabled = WI_DEFAULT_PM_ENABLED;
	sc->wi_max_sleep = WI_DEFAULT_MAX_SLEEP;
	sc->wi_roaming = WI_DEFAULT_ROAMING;
	sc->wi_authtype = WI_DEFAULT_AUTHTYPE;
	sc->wi_diversity = WI_DEFAULT_DIVERSITY;

	/*
	 * Read the default channel from the NIC. This may vary
	 * depending on the country where the NIC was purchased, so
	 * we can't hard-code a default and expect it to work for
	 * everyone.
	 */
	gen.wi_type = WI_RID_OWN_CHNL;
	gen.wi_len = 2;
	if (wi_read_record(sc, &gen) == 0)
		sc->wi_channel = letoh16(gen.wi_val);
	else
		sc->wi_channel = 3;

	/*
	 * Set flags based on firmware version.
	 */
	switch (sc->sc_firmware_type) {
	case WI_LUCENT:
		sc->wi_flags |= WI_FLAGS_HAS_ROAMING;
		if (sc->sc_sta_firmware_ver >= 60000)
			sc->wi_flags |= WI_FLAGS_HAS_MOR;
		if (sc->sc_sta_firmware_ver >= 60006) {
			sc->wi_flags |= WI_FLAGS_HAS_IBSS;
			sc->wi_flags |= WI_FLAGS_HAS_CREATE_IBSS;
		}
		sc->wi_ibss_port = htole16(1);
		break;
	case WI_INTERSIL:
		sc->wi_flags |= WI_FLAGS_HAS_ROAMING;
		if (sc->sc_sta_firmware_ver >= 800) {
			sc->wi_flags |= WI_FLAGS_HAS_HOSTAP;
			sc->wi_flags |= WI_FLAGS_HAS_IBSS;
			sc->wi_flags |= WI_FLAGS_HAS_CREATE_IBSS;
		}
		sc->wi_ibss_port = htole16(0);
		break;
	case WI_SYMBOL:
		sc->wi_flags |= WI_FLAGS_HAS_DIVERSITY;
		if (sc->sc_sta_firmware_ver >= 20000)
			sc->wi_flags |= WI_FLAGS_HAS_IBSS;
		if (sc->sc_sta_firmware_ver >= 25000)
			sc->wi_flags |= WI_FLAGS_HAS_CREATE_IBSS;
		sc->wi_ibss_port = htole16(4);
		break;
	}

	/*
	 * Find out if we support WEP on this card.
	 */
	gen.wi_type = WI_RID_WEP_AVAIL;
	gen.wi_len = 2;
	if (wi_read_record(sc, &gen) == 0 && gen.wi_val != htole16(0))
		sc->wi_flags |= WI_FLAGS_HAS_WEP;
	timeout_set(&sc->sc_timo, wi_inquire, sc);

	bzero((char *)&sc->wi_stats, sizeof(sc->wi_stats));

	/* Find supported rates. */
	gen.wi_type = WI_RID_DATA_RATES;
	gen.wi_len = 2;
	if (wi_read_record(sc, &gen))
		sc->wi_supprates = WI_SUPPRATES_1M | WI_SUPPRATES_2M |
		    WI_SUPPRATES_5M | WI_SUPPRATES_11M;
	else
		sc->wi_supprates = gen.wi_val;

	ifmedia_init(&sc->sc_media, 0, wi_media_change, wi_media_status);
#define	ADD(m, c)	ifmedia_add(&sc->sc_media, (m), (c), NULL)
	ADD(IFM_MAKEWORD(IFM_IEEE80211, IFM_AUTO, 0, 0), 0);
	ADD(IFM_MAKEWORD(IFM_IEEE80211, IFM_AUTO, IFM_IEEE80211_ADHOC, 0), 0);
	if (sc->wi_flags & WI_FLAGS_HAS_IBSS)
		ADD(IFM_MAKEWORD(IFM_IEEE80211, IFM_AUTO, IFM_IEEE80211_IBSS,
		    0), 0);
	if (sc->wi_flags & WI_FLAGS_HAS_CREATE_IBSS)
		ADD(IFM_MAKEWORD(IFM_IEEE80211, IFM_AUTO,
		    IFM_IEEE80211_IBSSMASTER, 0), 0);
	if (sc->wi_flags & WI_FLAGS_HAS_HOSTAP)
		ADD(IFM_MAKEWORD(IFM_IEEE80211, IFM_AUTO,
		    IFM_IEEE80211_HOSTAP, 0), 0);
	if (sc->wi_supprates & WI_SUPPRATES_1M) {
		ADD(IFM_MAKEWORD(IFM_IEEE80211, IFM_IEEE80211_DS1, 0, 0), 0);
		ADD(IFM_MAKEWORD(IFM_IEEE80211, IFM_IEEE80211_DS1,
		    IFM_IEEE80211_ADHOC, 0), 0);
		if (sc->wi_flags & WI_FLAGS_HAS_IBSS)
			ADD(IFM_MAKEWORD(IFM_IEEE80211, IFM_IEEE80211_DS1,
			    IFM_IEEE80211_IBSS, 0), 0);
		if (sc->wi_flags & WI_FLAGS_HAS_CREATE_IBSS)
			ADD(IFM_MAKEWORD(IFM_IEEE80211, IFM_IEEE80211_DS1,
			    IFM_IEEE80211_IBSSMASTER, 0), 0);
		if (sc->wi_flags & WI_FLAGS_HAS_HOSTAP)
			ADD(IFM_MAKEWORD(IFM_IEEE80211, IFM_IEEE80211_DS1,
			    IFM_IEEE80211_HOSTAP, 0), 0);
	}
	if (sc->wi_supprates & WI_SUPPRATES_2M) {
		ADD(IFM_MAKEWORD(IFM_IEEE80211, IFM_IEEE80211_DS2, 0, 0), 0);
		ADD(IFM_MAKEWORD(IFM_IEEE80211, IFM_IEEE80211_DS2,
		    IFM_IEEE80211_ADHOC, 0), 0);
		if (sc->wi_flags & WI_FLAGS_HAS_IBSS)
			ADD(IFM_MAKEWORD(IFM_IEEE80211, IFM_IEEE80211_DS2,
			    IFM_IEEE80211_IBSS, 0), 0);
		if (sc->wi_flags & WI_FLAGS_HAS_CREATE_IBSS)
			ADD(IFM_MAKEWORD(IFM_IEEE80211, IFM_IEEE80211_DS2,
			    IFM_IEEE80211_IBSSMASTER, 0), 0);
		if (sc->wi_flags & WI_FLAGS_HAS_HOSTAP)
			ADD(IFM_MAKEWORD(IFM_IEEE80211, IFM_IEEE80211_DS2,
			    IFM_IEEE80211_HOSTAP, 0), 0);
	}
	if (sc->wi_supprates & WI_SUPPRATES_5M) {
		ADD(IFM_MAKEWORD(IFM_IEEE80211, IFM_IEEE80211_DS5, 0, 0), 0);
		ADD(IFM_MAKEWORD(IFM_IEEE80211, IFM_IEEE80211_DS5,
		    IFM_IEEE80211_ADHOC, 0), 0);
		if (sc->wi_flags & WI_FLAGS_HAS_IBSS)
			ADD(IFM_MAKEWORD(IFM_IEEE80211, IFM_IEEE80211_DS5,
			    IFM_IEEE80211_IBSS, 0), 0);
		if (sc->wi_flags & WI_FLAGS_HAS_CREATE_IBSS)
			ADD(IFM_MAKEWORD(IFM_IEEE80211, IFM_IEEE80211_DS5,
			    IFM_IEEE80211_IBSSMASTER, 0), 0);
		if (sc->wi_flags & WI_FLAGS_HAS_HOSTAP)
			ADD(IFM_MAKEWORD(IFM_IEEE80211, IFM_IEEE80211_DS5,
			    IFM_IEEE80211_HOSTAP, 0), 0);
	}
	if (sc->wi_supprates & WI_SUPPRATES_11M) {
		ADD(IFM_MAKEWORD(IFM_IEEE80211, IFM_IEEE80211_DS11, 0, 0), 0);
		ADD(IFM_MAKEWORD(IFM_IEEE80211, IFM_IEEE80211_DS11,
		    IFM_IEEE80211_ADHOC, 0), 0);
		if (sc->wi_flags & WI_FLAGS_HAS_IBSS)
			ADD(IFM_MAKEWORD(IFM_IEEE80211, IFM_IEEE80211_DS11,
			    IFM_IEEE80211_IBSS, 0), 0);
		if (sc->wi_flags & WI_FLAGS_HAS_CREATE_IBSS)
			ADD(IFM_MAKEWORD(IFM_IEEE80211, IFM_IEEE80211_DS11,
			    IFM_IEEE80211_IBSSMASTER, 0), 0);
		if (sc->wi_flags & WI_FLAGS_HAS_HOSTAP)
			ADD(IFM_MAKEWORD(IFM_IEEE80211, IFM_IEEE80211_DS11,
			    IFM_IEEE80211_HOSTAP, 0), 0);
		ADD(IFM_MAKEWORD(IFM_IEEE80211, IFM_MANUAL, 0, 0), 0);
	}
#undef ADD
	ifmedia_set(&sc->sc_media,
	    IFM_MAKEWORD(IFM_IEEE80211, IFM_AUTO, 0, 0));

	/*
	 * Call MI attach routines.
	 */
	if_attach(ifp);
	ether_ifattach(ifp);
	printf("\n");

	sc->wi_flags |= WI_FLAGS_ATTACHED;

#if NBPFILTER > 0
	BPFATTACH(&sc->sc_arpcom.ac_if.if_bpf, ifp, DLT_EN10MB,
	    sizeof(struct ether_header));
#endif

	shutdownhook_establish(wi_shutdown, sc);

	wi_init(sc);
	wi_stop(sc);

	return (0);
}

int
wi_intr(vsc)
	void			*vsc;
{
	struct wi_softc		*sc = vsc;
	struct ifnet		*ifp;
	u_int16_t		status;

	DPRINTF(WID_INTR, ("wi_intr: sc %p\n", sc));

	ifp = &sc->sc_arpcom.ac_if;

	if (!(sc->wi_flags & WI_FLAGS_ATTACHED) || !(ifp->if_flags & IFF_UP)) {
		CSR_WRITE_2(sc, WI_EVENT_ACK, 0xFFFF);
		CSR_WRITE_2(sc, WI_INT_EN, 0);
		return (0);
	}

	/* Disable interrupts. */
	CSR_WRITE_2(sc, WI_INT_EN, 0);

	status = CSR_READ_2(sc, WI_EVENT_STAT);
	CSR_WRITE_2(sc, WI_EVENT_ACK, ~WI_INTRS);

	if (status & WI_EV_RX) {
		wi_rxeof(sc);
		CSR_WRITE_2(sc, WI_EVENT_ACK, WI_EV_RX);
	}

	if (status & WI_EV_TX) {
		wi_txeof(sc, status);
		CSR_WRITE_2(sc, WI_EVENT_ACK, WI_EV_TX);
	}

	if (status & WI_EV_ALLOC) {
		int			id;
		id = CSR_READ_2(sc, WI_ALLOC_FID);
		CSR_WRITE_2(sc, WI_EVENT_ACK, WI_EV_ALLOC);
		if (id == sc->wi_tx_data_id)
			wi_txeof(sc, status);
	}

	if (status & WI_EV_INFO) {
		wi_update_stats(sc);
		CSR_WRITE_2(sc, WI_EVENT_ACK, WI_EV_INFO);
	}

	if (status & WI_EV_TX_EXC) {
		wi_txeof(sc, status);
		CSR_WRITE_2(sc, WI_EVENT_ACK, WI_EV_TX_EXC);
	}

	if (status & WI_EV_INFO_DROP) {
		CSR_WRITE_2(sc, WI_EVENT_ACK, WI_EV_INFO_DROP);
	}

	/* Re-enable interrupts. */
	CSR_WRITE_2(sc, WI_INT_EN, WI_INTRS);

	if (!IFQ_IS_EMPTY(&ifp->if_snd))
		wi_start(ifp);

	return (1);
}

STATIC void
wi_rxeof(sc)
	struct wi_softc		*sc;
{
	struct ifnet		*ifp;
	struct ether_header	*eh;
	struct mbuf		*m;
	u_int16_t		msg_type;
	int			id;

	ifp = &sc->sc_arpcom.ac_if;

	id = CSR_READ_2(sc, WI_RX_FID);

	if (sc->wi_procframe || sc->wi_debug.wi_monitor) {
		struct wi_frame	*rx_frame;
		int		datlen, hdrlen;

		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if (m == NULL) {
			ifp->if_ierrors++;
			return;
		}
		MCLGET(m, M_DONTWAIT);
		if (!(m->m_flags & M_EXT)) {
			m_freem(m);
			ifp->if_ierrors++;
			return;
		}

		m->m_pkthdr.rcvif = ifp;

		if (wi_read_data(sc, id, 0, mtod(m, caddr_t),
		    sizeof(struct wi_frame))) {
			m_freem(m);
			ifp->if_ierrors++;
			return;
		}

		rx_frame = mtod(m, struct wi_frame *);

		if (rx_frame->wi_status & htole16(WI_STAT_BADCRC)) {
			m_freem(m);
			ifp->if_ierrors++;
			return;
		}

		switch ((letoh16(rx_frame->wi_status) & WI_STAT_MAC_PORT)
		    >> 8) {
		case 7:
			switch (letoh16(rx_frame->wi_frame_ctl) &
			    WI_FCTL_FTYPE) {
			case WI_FTYPE_DATA:
				hdrlen = WI_DATA_HDRLEN;
				datlen = letoh16(rx_frame->wi_dat_len);
				break;
			case WI_FTYPE_MGMT:
				hdrlen = WI_MGMT_HDRLEN;
				datlen = letoh16(rx_frame->wi_dat_len);
				break;
			case WI_FTYPE_CTL:
				hdrlen = WI_CTL_HDRLEN;
				datlen = 0;
				break;
			default:
				printf(WI_PRT_FMT ": received packet of "
				    "unknown type on port 7\n", WI_PRT_ARG(sc));
				m_freem(m);
				ifp->if_ierrors++;
				return;
			}
			break;
		case 0:
			hdrlen = WI_DATA_HDRLEN;
			datlen = letoh16(rx_frame->wi_dat_len);
			break;
		default:
			printf(WI_PRT_FMT ": received packet on invalid port "
			    "(wi_status=0x%x)\n", WI_PRT_ARG(sc),
			    letoh16(rx_frame->wi_status));
			m_freem(m);
			ifp->if_ierrors++;
			return;
		}

		if ((hdrlen + datlen + 2) > MCLBYTES) {
			m_freem(m);
			ifp->if_ierrors++;
			return;
		}

		if (wi_read_data(sc, id, hdrlen, mtod(m, caddr_t) + hdrlen,
		    datlen + 2)) {
			m_freem(m);
			ifp->if_ierrors++;
			return;
		}

		m->m_pkthdr.len = m->m_len = hdrlen + datlen;
	} else {
		struct wi_frame rx_frame;

		/* First read in the frame header */
		if (wi_read_data(sc, id, 0, (caddr_t)&rx_frame,
		    sizeof(rx_frame))) {
			ifp->if_ierrors++;
			return;
		}

		/* Drop undecryptable or packets with receive errors here */
		if (rx_frame.wi_status & htole16(WI_STAT_ERRSTAT)) {
			ifp->if_ierrors++;
			return;
		}

		/* Stash message type in host byte order for later use */
		msg_type = letoh16(rx_frame.wi_status) & WI_RXSTAT_MSG_TYPE;

		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if (m == NULL) {
			ifp->if_ierrors++;
			return;
		}
		MCLGET(m, M_DONTWAIT);
		if (!(m->m_flags & M_EXT)) {
			m_freem(m);
			ifp->if_ierrors++;
			return;
		}

		/* Align the data after the ethernet header */
		m->m_data = (caddr_t)ALIGN(m->m_data +
		    sizeof(struct ether_header)) - sizeof(struct ether_header);

		eh = mtod(m, struct ether_header *);
		m->m_pkthdr.rcvif = ifp;

		if (msg_type == WI_STAT_MGMT &&
		    sc->wi_ptype == WI_PORTTYPE_HOSTAP) {

			u_int16_t rxlen = letoh16(rx_frame.wi_dat_len);

			if ((WI_802_11_OFFSET_RAW + rxlen + 2) > MCLBYTES) {
				printf("%s: oversized mgmt packet received in "
				    "hostap mode (wi_dat_len=%d, "
				    "wi_status=0x%x)\n", sc->sc_dev.dv_xname,
				    rxlen, letoh16(rx_frame.wi_status));
				m_freem(m);
				ifp->if_ierrors++;  
				return;
			}

			/* Put the whole header in there. */
			bcopy(&rx_frame, mtod(m, void *),
			    sizeof(struct wi_frame));
			if (wi_read_data(sc, id, WI_802_11_OFFSET_RAW,
			    mtod(m, caddr_t) + WI_802_11_OFFSET_RAW,
			    rxlen + 2)) {
				m_freem(m);
				if (sc->sc_arpcom.ac_if.if_flags & IFF_DEBUG)
					printf("wihap: failed to copy header\n");
				ifp->if_ierrors++;
				return;
			}

			m->m_pkthdr.len = m->m_len =
			    WI_802_11_OFFSET_RAW + rxlen;

			/* XXX: consider giving packet to bhp? */

			wihap_mgmt_input(sc, &rx_frame, m);

			return;
		}

		switch (msg_type) {
		case WI_STAT_1042:
		case WI_STAT_TUNNEL:
		case WI_STAT_WMP_MSG:
			if ((letoh16(rx_frame.wi_dat_len) + WI_SNAPHDR_LEN) >
			    MCLBYTES) {
				printf(WI_PRT_FMT ": oversized packet received "
				    "(wi_dat_len=%d, wi_status=0x%x)\n",
				    WI_PRT_ARG(sc),
				    letoh16(rx_frame.wi_dat_len),
				    letoh16(rx_frame.wi_status));
				m_freem(m);
				ifp->if_ierrors++;
				return;
			}
			m->m_pkthdr.len = m->m_len =
			    letoh16(rx_frame.wi_dat_len) + WI_SNAPHDR_LEN;

			bcopy((char *)&rx_frame.wi_dst_addr,
			    (char *)&eh->ether_dhost, ETHER_ADDR_LEN);
			bcopy((char *)&rx_frame.wi_src_addr,
			    (char *)&eh->ether_shost, ETHER_ADDR_LEN);
			bcopy((char *)&rx_frame.wi_type,
			    (char *)&eh->ether_type, ETHER_TYPE_LEN);

			if (wi_read_data(sc, id, WI_802_11_OFFSET,
			    mtod(m, caddr_t) + sizeof(struct ether_header),
			    m->m_len + 2)) {
				ifp->if_ierrors++;
				m_freem(m);
				return;
			}
			break;
		default:
			if ((letoh16(rx_frame.wi_dat_len) +
			    sizeof(struct ether_header)) > MCLBYTES) {
				printf(WI_PRT_FMT ": oversized packet received "
				    "(wi_dat_len=%d, wi_status=0x%x)\n",
				    WI_PRT_ARG(sc),
				    letoh16(rx_frame.wi_dat_len),
				    letoh16(rx_frame.wi_status));
				m_freem(m);
				ifp->if_ierrors++;
				return;
			}
			m->m_pkthdr.len = m->m_len =
			    letoh16(rx_frame.wi_dat_len) +
			    sizeof(struct ether_header);

			if (wi_read_data(sc, id, WI_802_3_OFFSET,
			    mtod(m, caddr_t), m->m_len + 2)) {
				m_freem(m);
				ifp->if_ierrors++;
				return;
			}
			break;
		}

		ifp->if_ipackets++;

		if (sc->wi_ptype == WI_PORTTYPE_HOSTAP) {
			/*
			 * Give host AP code first crack at data packets.
			 * If it decides to handle it (or drop it), it will
			 * return a non-zero.  Otherwise, it is destined for
			 * this host.
			 */
			if (wihap_data_input(sc, &rx_frame, m))
				return;
		}
	}

#if NBPFILTER > 0
	/* Handle BPF listeners. */
	if (ifp->if_bpf)
		BPF_MTAP(ifp, m);
#endif

	/* Receive packet unless in procframe or monitor mode. */
	if (sc->wi_procframe || sc->wi_debug.wi_monitor)
		m_freem(m);
	else
		ether_input_mbuf(ifp, m);

	return;
}

STATIC void
wi_txeof(sc, status)
	struct wi_softc		*sc;
	int			status;
{
	struct ifnet		*ifp;

	ifp = &sc->sc_arpcom.ac_if;

	ifp->if_timer = 0;
	ifp->if_flags &= ~IFF_OACTIVE;

	if (status & WI_EV_TX_EXC)
		ifp->if_oerrors++;
	else
		ifp->if_opackets++;

	return;
}

void
wi_inquire(xsc)
	void			*xsc;
{
	struct wi_softc		*sc;
	struct ifnet		*ifp;
	int s, rv;

	sc = xsc;
	ifp = &sc->sc_arpcom.ac_if;

	timeout_add(&sc->sc_timo, hz * 60);

	/* Don't do this while we're transmitting */
	if (ifp->if_flags & IFF_OACTIVE)
		return;

	s = splnet();
	rv = wi_cmd(sc, WI_CMD_INQUIRE, WI_INFO_COUNTERS, 0, 0);
	splx(s);
	if (rv)
		printf(WI_PRT_FMT ": wi_cmd failed with %d\n", WI_PRT_ARG(sc),
		    rv);

	return;
}

void
wi_update_stats(sc)
	struct wi_softc		*sc;
{
	struct wi_ltv_gen	gen;
	u_int16_t		id;
	struct ifnet		*ifp;
	u_int32_t		*ptr;
	int			len, i;
	u_int16_t		t;

	ifp = &sc->sc_arpcom.ac_if;

	id = CSR_READ_2(sc, WI_INFO_FID);

	wi_read_data(sc, id, 0, (char *)&gen, 4);

	if (gen.wi_type == htole16(WI_INFO_SCAN_RESULTS)) {
		sc->wi_scanbuf_len = letoh16(gen.wi_len);
		wi_read_data(sc, id, 4, (caddr_t)sc->wi_scanbuf,
		    sc->wi_scanbuf_len * 2);
		return;
	} else if (gen.wi_type != htole16(WI_INFO_COUNTERS))
		return;

	/* Some card versions have a larger stats structure */
	len = (letoh16(gen.wi_len) - 1 < sizeof(sc->wi_stats) / 4) ?
	    letoh16(gen.wi_len) - 1 : sizeof(sc->wi_stats) / 4;

	ptr = (u_int32_t *)&sc->wi_stats;

	for (i = 0; i < len; i++) {
		t = CSR_READ_2(sc, WI_DATA1);
#ifdef WI_HERMES_STATS_WAR
		if (t > 0xF000)
			t = ~t & 0xFFFF;
#endif
		ptr[i] += t;
	}

	ifp->if_collisions = sc->wi_stats.wi_tx_single_retries +
	    sc->wi_stats.wi_tx_multi_retries +
	    sc->wi_stats.wi_tx_retry_limit;

	return;
}

STATIC int
wi_cmd(sc, cmd, val0, val1, val2)
	struct wi_softc		*sc;
	int			cmd;
	int			val0;
	int			val1;
	int			val2;
{
	int			i, s = 0;

	/* Wait for the busy bit to clear. */
	for (i = 0; i < WI_TIMEOUT; i++) {
		if (!(CSR_READ_2(sc, WI_COMMAND) & WI_CMD_BUSY))
			break;
		DELAY(10);
	}

	CSR_WRITE_2(sc, WI_PARAM0, val0);
	CSR_WRITE_2(sc, WI_PARAM1, val1);
	CSR_WRITE_2(sc, WI_PARAM2, val2);
	CSR_WRITE_2(sc, WI_COMMAND, cmd);

	for (i = WI_TIMEOUT; i--; DELAY(10)) {
		/*
		 * Wait for 'command complete' bit to be
		 * set in the event status register.
		 */
		s = CSR_READ_2(sc, WI_EVENT_STAT) & WI_EV_CMD;
		if (s) {
			/* Ack the event and read result code. */
			s = CSR_READ_2(sc, WI_STATUS);
			CSR_WRITE_2(sc, WI_EVENT_ACK, WI_EV_CMD);
#ifdef foo
			if ((s & WI_CMD_CODE_MASK) != (cmd & WI_CMD_CODE_MASK))
				return(EIO);
#endif
			if (s & WI_STAT_CMD_RESULT)
				return(EIO);
			break;
		}
	}

	if (i < 0)
		return(ETIMEDOUT);

	return(0);
}

STATIC void
wi_reset(sc)
	struct wi_softc		*sc;
{
	DPRINTF(WID_RESET, ("wi_reset: sc %p\n", sc));

	/* Symbol firmware cannot be initialized more than once. */
	if (sc->sc_firmware_type == WI_SYMBOL &&
	    (sc->wi_flags & WI_FLAGS_INITIALIZED))
		return;

	if (wi_cmd(sc, WI_CMD_INI, 0, 0, 0))
		printf(WI_PRT_FMT ": init failed\n", WI_PRT_ARG(sc));
	else
		sc->wi_flags |= WI_FLAGS_INITIALIZED;

	CSR_WRITE_2(sc, WI_INT_EN, 0);
	CSR_WRITE_2(sc, WI_EVENT_ACK, 0xFFFF);

	/* Calibrate timer. */
	WI_SETVAL(WI_RID_TICK_TIME, 8);

	return;
}

STATIC void
wi_cor_reset(sc)
	struct wi_softc		*sc;
{
	u_int8_t cor_value;

	DPRINTF(WID_RESET, ("wi_cor_reset: sc %p\n", sc));

	/*
	 * Do a soft reset of the card; this is required for Symbol cards.
	 * This shouldn't hurt other cards but there have been reports
	 * of the COR reset messing up old Lucent firmware revisions so
	 * we only soft reset Symbol cards for now.
	 */
	if (sc->sc_firmware_type == WI_SYMBOL) {
		cor_value = bus_space_read_1(sc->wi_ltag, sc->wi_lhandle,
		    sc->wi_cor_offset);
		bus_space_write_1(sc->wi_ltag, sc->wi_lhandle,
		    sc->wi_cor_offset, (cor_value | WI_COR_SOFT_RESET));
		DELAY(1000);
		bus_space_write_1(sc->wi_ltag, sc->wi_lhandle,
		    sc->wi_cor_offset, (cor_value & ~WI_COR_SOFT_RESET));
		DELAY(1000);
	}

	return;
}

/*
 * Read an LTV record from the NIC.
 */
STATIC int
wi_read_record(sc, ltv)
	struct wi_softc		*sc;
	struct wi_ltv_gen	*ltv;
{
	u_int8_t		*ptr;
	int			len, code;
	struct wi_ltv_gen	*oltv, p2ltv;

	if (sc->sc_firmware_type != WI_LUCENT) {
		oltv = ltv;
		switch (ltv->wi_type) {
		case WI_RID_ENCRYPTION:
			p2ltv.wi_type = WI_RID_P2_ENCRYPTION;
			p2ltv.wi_len = 2;
			ltv = &p2ltv;
			break;
		case WI_RID_TX_CRYPT_KEY:
			p2ltv.wi_type = WI_RID_P2_TX_CRYPT_KEY;
			p2ltv.wi_len = 2;
			ltv = &p2ltv;
			break;
		}
	}

	/* Tell the NIC to enter record read mode. */
	if (wi_cmd(sc, WI_CMD_ACCESS|WI_ACCESS_READ, ltv->wi_type, 0, 0))
		return(EIO);

	/* Seek to the record. */
	if (wi_seek(sc, ltv->wi_type, 0, WI_BAP1))
		return(EIO);

	/*
	 * Read the length and record type and make sure they
	 * match what we expect (this verifies that we have enough
	 * room to hold all of the returned data).
	 */
	len = CSR_READ_2(sc, WI_DATA1);
	if (len > ltv->wi_len)
		return(ENOSPC);
	code = CSR_READ_2(sc, WI_DATA1);
	if (code != ltv->wi_type)
		return(EIO);

	ltv->wi_len = len;
	ltv->wi_type = code;

	/* Now read the data. */
	ptr = (u_int8_t *)&ltv->wi_val;
	if (ltv->wi_len > 1)
		CSR_READ_RAW_2(sc, WI_DATA1, ptr, (ltv->wi_len-1)*2);

	if (ltv->wi_type == WI_RID_PORTTYPE && sc->wi_ptype == WI_PORTTYPE_IBSS
	    && ltv->wi_val == sc->wi_ibss_port) {
		/*
		 * Convert vendor IBSS port type to WI_PORTTYPE_IBSS.
		 * Since Lucent uses port type 1 for BSS *and* IBSS we
		 * have to rely on wi_ptype to distinguish this for us.
		 */
		ltv->wi_val = htole16(WI_PORTTYPE_IBSS);
	} else if (sc->sc_firmware_type != WI_LUCENT) {
		int v;

		switch (oltv->wi_type) {
		case WI_RID_TX_RATE:
		case WI_RID_CUR_TX_RATE:
			switch (letoh16(ltv->wi_val)) {
			case 1: v = 1; break;
			case 2: v = 2; break;
			case 3:	v = 6; break;
			case 4: v = 5; break;
			case 7: v = 7; break;
			case 8: v = 11; break;
			case 15: v = 3; break;
			default: v = 0x100 + letoh16(ltv->wi_val); break;
			}
			oltv->wi_val = htole16(v);
			break;
		case WI_RID_ENCRYPTION:
			oltv->wi_len = 2;
			if (ltv->wi_val & htole16(0x01))
				oltv->wi_val = htole16(1);
			else
				oltv->wi_val = htole16(0);
			break;
		case WI_RID_TX_CRYPT_KEY:
		case WI_RID_CNFAUTHMODE:
			oltv->wi_len = 2;
			oltv->wi_val = ltv->wi_val;
			break;
		}
	}

	return(0);
}

/*
 * Same as read, except we inject data instead of reading it.
 */
STATIC int
wi_write_record(sc, ltv)
	struct wi_softc		*sc;
	struct wi_ltv_gen	*ltv;
{
	u_int8_t		*ptr;
	u_int16_t		val;
	int			i;
	struct wi_ltv_gen	p2ltv;

	if (ltv->wi_type == WI_RID_PORTTYPE &&
	    letoh16(ltv->wi_val) == WI_PORTTYPE_IBSS) {
		/* Convert WI_PORTTYPE_IBSS to vendor IBSS port type. */
		p2ltv.wi_type = WI_RID_PORTTYPE;
		p2ltv.wi_len = 2;
		p2ltv.wi_val = sc->wi_ibss_port;
		ltv = &p2ltv;
	} else if (sc->sc_firmware_type != WI_LUCENT) {
		int v;

		switch (ltv->wi_type) {
		case WI_RID_TX_RATE:
			p2ltv.wi_type = WI_RID_TX_RATE;
			p2ltv.wi_len = 2;
			switch (letoh16(ltv->wi_val)) {
			case 1: v = 1; break;
			case 2: v = 2; break;
			case 3:	v = 15; break;
			case 5: v = 4; break;
			case 6: v = 3; break;
			case 7: v = 7; break;
			case 11: v = 8; break;
			default: return EINVAL;
			}
			p2ltv.wi_val = htole16(v);
			ltv = &p2ltv;
			break;
		case WI_RID_ENCRYPTION:
			p2ltv.wi_type = WI_RID_P2_ENCRYPTION;
			p2ltv.wi_len = 2;
			if (ltv->wi_val & htole16(0x01)) {
				val = PRIVACY_INVOKED;
				/*
				 * If using shared key WEP we must set the
				 * EXCLUDE_UNENCRYPTED bit.  Symbol cards
				 * need this bit set even when not using
				 * shared key. We can't just test for
				 * IEEE80211_AUTH_SHARED since Symbol cards
				 * have 2 shared key modes.
				 */
				if (sc->wi_authtype != IEEE80211_AUTH_OPEN ||
				    sc->sc_firmware_type == WI_SYMBOL)
					val |= EXCLUDE_UNENCRYPTED;
				/* TX encryption is broken in Host AP mode. */
				if (sc->wi_ptype == WI_PORTTYPE_HOSTAP)
					val |= HOST_ENCRYPT;
				p2ltv.wi_val = htole16(val);
			} else
				p2ltv.wi_val = htole16(HOST_ENCRYPT | HOST_DECRYPT);
			ltv = &p2ltv;
			break;
		case WI_RID_TX_CRYPT_KEY:
			p2ltv.wi_type = WI_RID_P2_TX_CRYPT_KEY;
			p2ltv.wi_len = 2;
			p2ltv.wi_val = ltv->wi_val;
			ltv = &p2ltv;
			break;
		case WI_RID_DEFLT_CRYPT_KEYS: {
				int error;
				int keylen;
				struct wi_ltv_str ws;
				struct wi_ltv_keys *wk = (struct wi_ltv_keys *)ltv;
				keylen = wk->wi_keys[sc->wi_tx_key].wi_keylen;

				for (i = 0; i < 4; i++) {
					bzero(&ws, sizeof(ws));
					ws.wi_len = (keylen > 5) ? 8 : 4;
					ws.wi_type = WI_RID_P2_CRYPT_KEY0 + i;
					bcopy(&wk->wi_keys[i].wi_keydat,
					    ws.wi_str, keylen);
					error = wi_write_record(sc,
					    (struct wi_ltv_gen *)&ws);
					if (error)
						return (error);
				}
			}
			return (0);
		}
	}

	if (wi_seek(sc, ltv->wi_type, 0, WI_BAP1))
		return(EIO);

	CSR_WRITE_2(sc, WI_DATA1, ltv->wi_len);
	CSR_WRITE_2(sc, WI_DATA1, ltv->wi_type);

	ptr = (u_int8_t *)&ltv->wi_val;
	if (ltv->wi_len > 1)
		CSR_WRITE_RAW_2(sc, WI_DATA1, ptr, (ltv->wi_len-1) *2);

	if (wi_cmd(sc, WI_CMD_ACCESS|WI_ACCESS_WRITE, ltv->wi_type, 0, 0))
		return(EIO);

	return(0);
}

STATIC int
wi_seek(sc, id, off, chan)
	struct wi_softc		*sc;
	int			id, off, chan;
{
	int			i;
	int			selreg, offreg;

	switch (chan) {
	case WI_BAP0:
		selreg = WI_SEL0;
		offreg = WI_OFF0;
		break;
	case WI_BAP1:
		selreg = WI_SEL1;
		offreg = WI_OFF1;
		break;
	default:
		printf(WI_PRT_FMT ": invalid data path: %x\n", WI_PRT_ARG(sc),
		    chan);
		return(EIO);
	}

	CSR_WRITE_2(sc, selreg, id);
	CSR_WRITE_2(sc, offreg, off);

	for (i = WI_TIMEOUT; i--; DELAY(10))
		if (!(CSR_READ_2(sc, offreg) & (WI_OFF_BUSY|WI_OFF_ERR)))
			break;

	if (i < 0)
		return(ETIMEDOUT);

	return(0);
}

STATIC int
wi_read_data(sc, id, off, buf, len)
	struct wi_softc		*sc;
	int			id, off;
	caddr_t			buf;
	int			len;
{
	u_int8_t		*ptr;

	if (wi_seek(sc, id, off, WI_BAP1))
		return(EIO);

	ptr = (u_int8_t *)buf;
	CSR_READ_RAW_2(sc, WI_DATA1, ptr, len);

	return(0);
}

/*
 * According to the comments in the HCF Light code, there is a bug in
 * the Hermes (or possibly in certain Hermes firmware revisions) where
 * the chip's internal autoincrement counter gets thrown off during
 * data writes: the autoincrement is missed, causing one data word to
 * be overwritten and subsequent words to be written to the wrong memory
 * locations. The end result is that we could end up transmitting bogus
 * frames without realizing it. The workaround for this is to write a
 * couple of extra guard words after the end of the transfer, then
 * attempt to read then back. If we fail to locate the guard words where
 * we expect them, we preform the transfer over again.
 */
STATIC int
wi_write_data(sc, id, off, buf, len)
	struct wi_softc		*sc;
	int			id, off;
	caddr_t			buf;
	int			len;
{
	u_int8_t		*ptr;

#ifdef WI_HERMES_AUTOINC_WAR
again:
#endif

	if (wi_seek(sc, id, off, WI_BAP0))
		return(EIO);

	ptr = (u_int8_t *)buf;
	CSR_WRITE_RAW_2(sc, WI_DATA0, ptr, len);

#ifdef WI_HERMES_AUTOINC_WAR
	CSR_WRITE_2(sc, WI_DATA0, 0x1234);
	CSR_WRITE_2(sc, WI_DATA0, 0x5678);

	if (wi_seek(sc, id, off + len, WI_BAP0))
		return(EIO);

	if (CSR_READ_2(sc, WI_DATA0) != 0x1234 ||
	    CSR_READ_2(sc, WI_DATA0) != 0x5678)
		goto again;
#endif

	return(0);
}

/*
 * Allocate a region of memory inside the NIC and zero
 * it out.
 */
STATIC int
wi_alloc_nicmem(sc, len, id)
	struct wi_softc		*sc;
	int			len;
	int			*id;
{
	int			i;

	if (wi_cmd(sc, WI_CMD_ALLOC_MEM, len, 0, 0)) {
		printf(WI_PRT_FMT ": failed to allocate %d bytes on NIC\n",
		    WI_PRT_ARG(sc), len);
		return(ENOMEM);
	}

	for (i = WI_TIMEOUT; i--; DELAY(10)) {
		if (CSR_READ_2(sc, WI_EVENT_STAT) & WI_EV_ALLOC)
			break;
	}

	if (i < 0)
		return(ETIMEDOUT);

	*id = CSR_READ_2(sc, WI_ALLOC_FID);
	CSR_WRITE_2(sc, WI_EVENT_ACK, WI_EV_ALLOC);

	if (wi_seek(sc, *id, 0, WI_BAP0))
		return(EIO);

	for (i = 0; i < len / 2; i++)
		CSR_WRITE_2(sc, WI_DATA0, 0);

	return(0);
}

STATIC void
wi_setmulti(sc)
	struct wi_softc		*sc;
{
	struct ifnet		*ifp;
	int			i = 0;
	struct wi_ltv_mcast	mcast;
	struct ether_multistep	step;
	struct ether_multi	*enm;

	ifp = &sc->sc_arpcom.ac_if;

	bzero((char *)&mcast, sizeof(mcast));

	mcast.wi_type = WI_RID_MCAST_LIST;
	mcast.wi_len = ((ETHER_ADDR_LEN / 2) * 16) + 1;

	if (ifp->if_flags & IFF_ALLMULTI || ifp->if_flags & IFF_PROMISC) {
		wi_write_record(sc, (struct wi_ltv_gen *)&mcast);
		return;
	}

	ETHER_FIRST_MULTI(step, &sc->sc_arpcom, enm);
	while (enm != NULL) {
		if (i >= 16) {
			bzero((char *)&mcast, sizeof(mcast));
			break;
		}

		/* Punt on ranges. */
		if (bcmp(enm->enm_addrlo, enm->enm_addrhi,
		    sizeof(enm->enm_addrlo)) != 0)
			break;
		bcopy(enm->enm_addrlo, (char *)&mcast.wi_mcast[i],
		    ETHER_ADDR_LEN);
		i++;
		ETHER_NEXT_MULTI(step, enm);
	}

	mcast.wi_len = (i * 3) + 1;
	wi_write_record(sc, (struct wi_ltv_gen *)&mcast);

	return;
}

STATIC int
wi_setdef(sc, wreq)
	struct wi_softc		*sc;
	struct wi_req		*wreq;
{
	struct ifnet		*ifp;
	int error = 0;

	ifp = &sc->sc_arpcom.ac_if;

	switch(wreq->wi_type) {
	case WI_RID_MAC_NODE:
		bcopy((char *)&wreq->wi_val, LLADDR(ifp->if_sadl),
		    ETHER_ADDR_LEN);
		bcopy((char *)&wreq->wi_val, (char *)&sc->sc_arpcom.ac_enaddr,
		    ETHER_ADDR_LEN);
		break;
	case WI_RID_PORTTYPE:
		error = wi_sync_media(sc, letoh16(wreq->wi_val[0]),
		    sc->wi_tx_rate);
		break;
	case WI_RID_TX_RATE:
		error = wi_sync_media(sc, sc->wi_ptype,
		    letoh16(wreq->wi_val[0]));
		break;
	case WI_RID_MAX_DATALEN:
		sc->wi_max_data_len = letoh16(wreq->wi_val[0]);
		break;
	case WI_RID_RTS_THRESH:
		sc->wi_rts_thresh = letoh16(wreq->wi_val[0]);
		break;
	case WI_RID_SYSTEM_SCALE:
		sc->wi_ap_density = letoh16(wreq->wi_val[0]);
		break;
	case WI_RID_CREATE_IBSS:
		sc->wi_create_ibss = letoh16(wreq->wi_val[0]);
		error = wi_sync_media(sc, sc->wi_ptype, sc->wi_tx_rate);
		break;
	case WI_RID_OWN_CHNL:
		sc->wi_channel = letoh16(wreq->wi_val[0]);
		break;
	case WI_RID_NODENAME:
		error = wi_set_ssid(&sc->wi_node_name,
		    (u_int8_t *)&wreq->wi_val[1], letoh16(wreq->wi_val[0]));
		break;
	case WI_RID_DESIRED_SSID:
		error = wi_set_ssid(&sc->wi_net_name,
		    (u_int8_t *)&wreq->wi_val[1], letoh16(wreq->wi_val[0]));
		break;
	case WI_RID_OWN_SSID:
		error = wi_set_ssid(&sc->wi_ibss_name,
		    (u_int8_t *)&wreq->wi_val[1], letoh16(wreq->wi_val[0]));
		break;
	case WI_RID_PM_ENABLED:
		sc->wi_pm_enabled = letoh16(wreq->wi_val[0]);
		break;
	case WI_RID_MICROWAVE_OVEN:
		sc->wi_mor_enabled = letoh16(wreq->wi_val[0]);
		break;
	case WI_RID_MAX_SLEEP:
		sc->wi_max_sleep = letoh16(wreq->wi_val[0]);
		break;
	case WI_RID_CNFAUTHMODE:
		sc->wi_authtype = letoh16(wreq->wi_val[0]);
		break;
	case WI_RID_ROAMING_MODE:
		sc->wi_roaming = letoh16(wreq->wi_val[0]);
		break;
	case WI_RID_SYMBOL_DIVERSITY:
		sc->wi_diversity = letoh16(wreq->wi_val[0]);
		break;
	case WI_RID_ENCRYPTION:
		sc->wi_use_wep = letoh16(wreq->wi_val[0]);
		break;
	case WI_RID_TX_CRYPT_KEY:
		sc->wi_tx_key = letoh16(wreq->wi_val[0]);
		break;
	case WI_RID_DEFLT_CRYPT_KEYS:
		bcopy((char *)wreq, (char *)&sc->wi_keys,
		    sizeof(struct wi_ltv_keys));
		break;
	default:
		error = EINVAL;
		break;
	}

	return (error);
}

STATIC int
wi_ioctl(ifp, command, data)
	struct ifnet		*ifp;
	u_long			command;
	caddr_t			data;
{
	int			s, error = 0;
	struct wi_softc		*sc;
	struct wi_req		wreq;
	struct ifreq		*ifr;
	struct proc		*p = curproc;
	struct ifaddr		*ifa = (struct ifaddr *)data;
	struct ieee80211_nwid	nwid;

	s = splimp();

	sc = ifp->if_softc;
	ifr = (struct ifreq *)data;

	if (!(sc->wi_flags & WI_FLAGS_ATTACHED)) {
		splx(s);
		return(ENODEV);
	}

	DPRINTF (WID_IOCTL, ("wi_ioctl: command %lu data %p\n",
	    command, data));

	if ((error = ether_ioctl(ifp, &sc->sc_arpcom, command, data)) > 0) {
		splx(s);
		return error;
	}

	switch(command) {
	case SIOCSWAVELAN:
	case SIOCSPRISM2DEBUG:
	case SIOCS80211NWID:
	case SIOCS80211NWKEY:
	case SIOCS80211POWER:
		error = suser(p->p_ucred, &p->p_acflag);
		if (error) {
			splx(s);
			return (error);
		}
	default:
		break;
	}

	switch(command) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			wi_init(sc);
			arp_ifinit(&sc->sc_arpcom, ifa);
			break;
#endif	/* INET */
		default:
			wi_init(sc);
			break;
		}
		break;

	case SIOCSIFMTU:
		if (ifr->ifr_mtu > ETHERMTU || ifr->ifr_mtu < ETHERMIN) {
			error = EINVAL;
		} else if (ifp->if_mtu != ifr->ifr_mtu) {
			ifp->if_mtu = ifr->ifr_mtu;
		}
		break;

	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING &&
			    ifp->if_flags & IFF_PROMISC &&
			    !(sc->wi_if_flags & IFF_PROMISC)) {
				if (sc->wi_ptype != WI_PORTTYPE_HOSTAP)
					WI_SETVAL(WI_RID_PROMISC, 1);
			} else if (ifp->if_flags & IFF_RUNNING &&
			    !(ifp->if_flags & IFF_PROMISC) &&
			    sc->wi_if_flags & IFF_PROMISC) {
				if (sc->wi_ptype != WI_PORTTYPE_HOSTAP)
					WI_SETVAL(WI_RID_PROMISC, 0);
			} else
				wi_init(sc);
		} else if (ifp->if_flags & IFF_RUNNING)
			wi_stop(sc);
		sc->wi_if_flags = ifp->if_flags;
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		/* Update our multicast list. */
		error = (command == SIOCADDMULTI) ?
		    ether_addmulti(ifr, &sc->sc_arpcom) :
		    ether_delmulti(ifr, &sc->sc_arpcom);
		if (error == ENETRESET) {
			/*
			 * Multicast list has changed; set the hardware filter
			 * accordingly.
			 */
			wi_setmulti(sc);
			error = 0;
		}
		break;
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_media, command);
		break;
	case SIOCGWAVELAN:
		error = copyin(ifr->ifr_data, &wreq, sizeof(wreq));
		if (error)
			break;
		if (wreq.wi_len > WI_MAX_DATALEN) {
			error = EINVAL;
			break;
		}
		switch (wreq.wi_type) {
		case WI_RID_IFACE_STATS:
			/* XXX native byte order */
			bcopy((char *)&sc->wi_stats, (char *)&wreq.wi_val,
			    sizeof(sc->wi_stats));
			wreq.wi_len = (sizeof(sc->wi_stats) / 2) + 1;
			break;
		case WI_RID_DEFLT_CRYPT_KEYS:
			/* For non-root user, return all-zeroes keys */
			if (suser(p->p_ucred, &p->p_acflag))
				bzero((char *)&wreq,
					sizeof(struct wi_ltv_keys));
			else
				bcopy((char *)&sc->wi_keys, (char *)&wreq,
					sizeof(struct wi_ltv_keys));
			break;
		case WI_RID_PROCFRAME:
			wreq.wi_len = 2;
			wreq.wi_val[0] = htole16(sc->wi_procframe);
			break;
		case WI_RID_PRISM2:
			wreq.wi_len = 2;
			wreq.wi_val[0] = htole16(sc->sc_firmware_type ==
			    WI_LUCENT ? 0 : 1);
			break;
		case WI_RID_SCAN_RES:
			if (sc->sc_firmware_type == WI_LUCENT) {
				memcpy((char *)wreq.wi_val,
				    (char *)sc->wi_scanbuf,
				    sc->wi_scanbuf_len * 2);
				wreq.wi_len = sc->wi_scanbuf_len;
				break;
			}
		default:
			if (wi_read_record(sc, (struct wi_ltv_gen *)&wreq)) {
				error = EINVAL;
			}
			break;
		}
		error = copyout(&wreq, ifr->ifr_data, sizeof(wreq));
		break;
	case SIOCSWAVELAN:
		error = copyin(ifr->ifr_data, &wreq, sizeof(wreq));
		if (error)
			break;
		error = EINVAL;
		if (wreq.wi_len > WI_MAX_DATALEN)
			break;
		switch (wreq.wi_type) {
		case WI_RID_IFACE_STATS:
			break;
		case WI_RID_MGMT_XMIT:
			error = wi_mgmt_xmit(sc, (caddr_t)&wreq.wi_val,
			    wreq.wi_len);
			break;
		case WI_RID_PROCFRAME:
			sc->wi_procframe = letoh16(wreq.wi_val[0]);
			error = 0;
			break;
		case WI_RID_SCAN_REQ:
			error = 0;
			if (sc->sc_firmware_type == WI_LUCENT)
				wi_cmd(sc, WI_CMD_INQUIRE,
				    WI_INFO_SCAN_RESULTS, 0, 0);
			else
				error = wi_write_record(sc,
				    (struct wi_ltv_gen *)&wreq);
			break;
		case WI_RID_SYMBOL_DIVERSITY:
		case WI_RID_ROAMING_MODE:
		case WI_RID_CREATE_IBSS:
		case WI_RID_MICROWAVE_OVEN:
		case WI_RID_OWN_SSID:
			/*
			 * Check for features that may not be supported
			 * (must be just before default case).
			 */
			if ((wreq.wi_type == WI_RID_SYMBOL_DIVERSITY &&
			    !(sc->wi_flags & WI_FLAGS_HAS_DIVERSITY)) ||
			    (wreq.wi_type == WI_RID_ROAMING_MODE &&
			    !(sc->wi_flags & WI_FLAGS_HAS_ROAMING)) ||
			    (wreq.wi_type == WI_RID_CREATE_IBSS &&
			    !(sc->wi_flags & WI_FLAGS_HAS_CREATE_IBSS)) ||
			    (wreq.wi_type == WI_RID_MICROWAVE_OVEN &&
			    !(sc->wi_flags & WI_FLAGS_HAS_MOR)) ||
			    (wreq.wi_type == WI_RID_OWN_SSID &&
			    wreq.wi_len != 0))
				break;
			/* FALLTHROUGH */
		default:
			error = wi_write_record(sc, (struct wi_ltv_gen *)&wreq);
			if (!error)
				error = wi_setdef(sc, &wreq);
			if (!error && (ifp->if_flags & IFF_UP))
				wi_init(sc);
		}
		break;
	case SIOCGPRISM2DEBUG:
		error = copyin(ifr->ifr_data, &wreq, sizeof(wreq));
		if (error)
			break;
		if (!(ifp->if_flags & IFF_RUNNING) ||
		    sc->sc_firmware_type == WI_LUCENT) {
			error = EIO;
			break;
		}
		error = wi_get_debug(sc, &wreq);
		if (error == 0)
			error = copyout(&wreq, ifr->ifr_data, sizeof(wreq));
		break;
	case SIOCSPRISM2DEBUG:
		error = copyin(ifr->ifr_data, &wreq, sizeof(wreq));
		if (error)
			break;
		error = wi_set_debug(sc, &wreq);
		break;
	case SIOCG80211NWID:
		if ((ifp->if_flags & IFF_UP) && sc->wi_net_name.i_len > 0) {
			/* Return the desired ID */
			error = copyout(&sc->wi_net_name, ifr->ifr_data,
			    sizeof(sc->wi_net_name));
		} else {
			wreq.wi_type = WI_RID_CURRENT_SSID;
			wreq.wi_len = WI_MAX_DATALEN;
			if (wi_read_record(sc, (struct wi_ltv_gen *)&wreq) ||
			    letoh16(wreq.wi_val[0]) > IEEE80211_NWID_LEN)
				error = EINVAL;
			else {
				wi_set_ssid(&nwid, (u_int8_t *)&wreq.wi_val[1],
				    letoh16(wreq.wi_val[0]));
				error = copyout(&nwid, ifr->ifr_data,
				    sizeof(nwid));
			}
		}
		break;
	case SIOCS80211NWID:
		error = copyin(ifr->ifr_data, &nwid, sizeof(nwid));
		if (error)
			break;
		if (nwid.i_len > IEEE80211_NWID_LEN) {
			error = EINVAL;
			break;
		}
		if (sc->wi_net_name.i_len == nwid.i_len &&
		    memcmp(sc->wi_net_name.i_nwid, nwid.i_nwid, nwid.i_len) == 0)
			break;
		wi_set_ssid(&sc->wi_net_name, nwid.i_nwid, nwid.i_len);
		WI_SETSTR(WI_RID_DESIRED_SSID, sc->wi_net_name);
		if (ifp->if_flags & IFF_UP)
			/* Reinitialize WaveLAN. */
			wi_init(sc);
		break;
	case SIOCS80211NWKEY:
		error = wi_set_nwkey(sc, (struct ieee80211_nwkey *)data);
		break;
	case SIOCG80211NWKEY:
		error = wi_get_nwkey(sc, (struct ieee80211_nwkey *)data);
		break;
	case SIOCS80211POWER:
		error = wi_set_pm(sc, (struct ieee80211_power *)data);
		break;
	case SIOCG80211POWER:
		error = wi_get_pm(sc, (struct ieee80211_power *)data);
		break;
	case SIOCHOSTAP_ADD:
	case SIOCHOSTAP_DEL:
	case SIOCHOSTAP_GET:
	case SIOCHOSTAP_GETALL:
	case SIOCHOSTAP_GFLAGS:
	case SIOCHOSTAP_SFLAGS:
		/* Send all Host AP specific ioctl's to Host AP code. */
		error = wihap_ioctl(sc, command, data);
		break;
	default:
		error = EINVAL;
		break;
	}

	splx(s);
	return(error);
}

STATIC void
wi_init(sc)
	struct wi_softc		*sc;
{
	struct ifnet		*ifp = &sc->sc_arpcom.ac_if;
	int			s;
	struct wi_ltv_macaddr	mac;
	int			id = 0;

	if (!(sc->wi_flags & WI_FLAGS_ATTACHED))
		return;

	DPRINTF(WID_INIT, ("wi_init: sc %p\n", sc));

	s = splimp();

	if (ifp->if_flags & IFF_RUNNING)
		wi_stop(sc);

	wi_reset(sc);

	/* Program max data length. */
	WI_SETVAL(WI_RID_MAX_DATALEN, sc->wi_max_data_len);

	/* Set the port type. */
	WI_SETVAL(WI_RID_PORTTYPE, sc->wi_ptype);

	/* Enable/disable IBSS creation. */
	WI_SETVAL(WI_RID_CREATE_IBSS, sc->wi_create_ibss);

	/* Program the RTS/CTS threshold. */
	WI_SETVAL(WI_RID_RTS_THRESH, sc->wi_rts_thresh);

	/* Program the TX rate */
	WI_SETVAL(WI_RID_TX_RATE, sc->wi_tx_rate);

	/* Access point density */
	WI_SETVAL(WI_RID_SYSTEM_SCALE, sc->wi_ap_density);

	/* Power Management Enabled */
	WI_SETVAL(WI_RID_PM_ENABLED, sc->wi_pm_enabled);

	/* Power Managment Max Sleep */
	WI_SETVAL(WI_RID_MAX_SLEEP, sc->wi_max_sleep);

	/* Set Roaming Mode unless this is a Symbol card. */
	if (sc->wi_flags & WI_FLAGS_HAS_ROAMING)
		WI_SETVAL(WI_RID_ROAMING_MODE, sc->wi_roaming);

	/* Set Antenna Diversity if this is a Symbol card. */
	if (sc->wi_flags & WI_FLAGS_HAS_DIVERSITY)
		WI_SETVAL(WI_RID_SYMBOL_DIVERSITY, sc->wi_diversity);

	/* Specify the network name */
	WI_SETSTR(WI_RID_DESIRED_SSID, sc->wi_net_name);

	/* Specify the IBSS name */
	if (sc->wi_net_name.i_len != 0 && (sc->wi_ptype == WI_PORTTYPE_HOSTAP ||
	    (sc->wi_create_ibss && sc->wi_ptype == WI_PORTTYPE_IBSS)))
		WI_SETSTR(WI_RID_OWN_SSID, sc->wi_net_name);
	else
		WI_SETSTR(WI_RID_OWN_SSID, sc->wi_ibss_name);

	/* Specify the frequency to use */
	WI_SETVAL(WI_RID_OWN_CHNL, sc->wi_channel);

	/* Program the nodename. */
	WI_SETSTR(WI_RID_NODENAME, sc->wi_node_name);

	/* Set our MAC address. */
	mac.wi_len = 4;
	mac.wi_type = WI_RID_MAC_NODE;
	bcopy((char *)&sc->sc_arpcom.ac_enaddr,
	   (char *)&mac.wi_mac_addr, ETHER_ADDR_LEN);
	wi_write_record(sc, (struct wi_ltv_gen *)&mac);

	/*
	 * Initialize promisc mode.
	 *	Being in the Host-AP mode causes
	 *	great deal of pain if promisc mode is set.
	 *	Therefore we avoid confusing the firmware
	 *	and always reset promisc mode in Host-AP regime,
	 *	it shows us all the packets anyway.
	 */
	if (sc->wi_ptype != WI_PORTTYPE_HOSTAP && ifp->if_flags & IFF_PROMISC)
		WI_SETVAL(WI_RID_PROMISC, 1);
	else
		WI_SETVAL(WI_RID_PROMISC, 0);

	/* Configure WEP. */
	if (sc->wi_flags & WI_FLAGS_HAS_WEP) {
		WI_SETVAL(WI_RID_ENCRYPTION, sc->wi_use_wep);
		WI_SETVAL(WI_RID_TX_CRYPT_KEY, sc->wi_tx_key);
		sc->wi_keys.wi_len = (sizeof(struct wi_ltv_keys) / 2) + 1;
		sc->wi_keys.wi_type = WI_RID_DEFLT_CRYPT_KEYS;
		wi_write_record(sc, (struct wi_ltv_gen *)&sc->wi_keys);
		if (sc->sc_firmware_type != WI_LUCENT && sc->wi_use_wep) {
			/*
			 * HWB3163 EVAL-CARD Firmware version less than 0.8.2.
			 *
			 * If promiscuous mode is disabled, the Prism2 chip
			 * does not work with WEP .
			 * I'm currently investigating the details of this.
			 * (ichiro@netbsd.org)
			 */
			 if (sc->sc_firmware_type == WI_INTERSIL &&
			    sc->sc_sta_firmware_ver < 802 ) {
				/* firm ver < 0.8.2 */
				WI_SETVAL(WI_RID_PROMISC, 1);
			 }
			 WI_SETVAL(WI_RID_CNFAUTHMODE, sc->wi_authtype);
		}
	}

	/* Set multicast filter. */
	wi_setmulti(sc);

	/* Enable desired port */
	wi_cmd(sc, WI_CMD_ENABLE | sc->wi_portnum, 0, 0, 0);

	if (wi_alloc_nicmem(sc, ETHER_MAX_LEN + sizeof(struct wi_frame) + 8, &id))
		printf(WI_PRT_FMT ": tx buffer allocation failed\n",
		    WI_PRT_ARG(sc));
	sc->wi_tx_data_id = id;

	if (wi_alloc_nicmem(sc, ETHER_MAX_LEN + sizeof(struct wi_frame) + 8, &id))
		printf(WI_PRT_FMT ": mgmt. buffer allocation failed\n",
		    WI_PRT_ARG(sc));
	sc->wi_tx_mgmt_id = id;

	/* enable interrupts */
	CSR_WRITE_2(sc, WI_INT_EN, WI_INTRS);

        wihap_init(sc);

	splx(s);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	timeout_add(&sc->sc_timo, hz * 60);

	return;
}

static const u_int32_t crc32_tab[] = {
	0x00000000L, 0x77073096L, 0xee0e612cL, 0x990951baL, 0x076dc419L,
	0x706af48fL, 0xe963a535L, 0x9e6495a3L, 0x0edb8832L, 0x79dcb8a4L,
	0xe0d5e91eL, 0x97d2d988L, 0x09b64c2bL, 0x7eb17cbdL, 0xe7b82d07L,
	0x90bf1d91L, 0x1db71064L, 0x6ab020f2L, 0xf3b97148L, 0x84be41deL,
	0x1adad47dL, 0x6ddde4ebL, 0xf4d4b551L, 0x83d385c7L, 0x136c9856L,
	0x646ba8c0L, 0xfd62f97aL, 0x8a65c9ecL, 0x14015c4fL, 0x63066cd9L,
	0xfa0f3d63L, 0x8d080df5L, 0x3b6e20c8L, 0x4c69105eL, 0xd56041e4L,
	0xa2677172L, 0x3c03e4d1L, 0x4b04d447L, 0xd20d85fdL, 0xa50ab56bL,
	0x35b5a8faL, 0x42b2986cL, 0xdbbbc9d6L, 0xacbcf940L, 0x32d86ce3L,
	0x45df5c75L, 0xdcd60dcfL, 0xabd13d59L, 0x26d930acL, 0x51de003aL,
	0xc8d75180L, 0xbfd06116L, 0x21b4f4b5L, 0x56b3c423L, 0xcfba9599L,
	0xb8bda50fL, 0x2802b89eL, 0x5f058808L, 0xc60cd9b2L, 0xb10be924L,
	0x2f6f7c87L, 0x58684c11L, 0xc1611dabL, 0xb6662d3dL, 0x76dc4190L,
	0x01db7106L, 0x98d220bcL, 0xefd5102aL, 0x71b18589L, 0x06b6b51fL,
	0x9fbfe4a5L, 0xe8b8d433L, 0x7807c9a2L, 0x0f00f934L, 0x9609a88eL,
	0xe10e9818L, 0x7f6a0dbbL, 0x086d3d2dL, 0x91646c97L, 0xe6635c01L,
	0x6b6b51f4L, 0x1c6c6162L, 0x856530d8L, 0xf262004eL, 0x6c0695edL,
	0x1b01a57bL, 0x8208f4c1L, 0xf50fc457L, 0x65b0d9c6L, 0x12b7e950L,
	0x8bbeb8eaL, 0xfcb9887cL, 0x62dd1ddfL, 0x15da2d49L, 0x8cd37cf3L,
	0xfbd44c65L, 0x4db26158L, 0x3ab551ceL, 0xa3bc0074L, 0xd4bb30e2L,
	0x4adfa541L, 0x3dd895d7L, 0xa4d1c46dL, 0xd3d6f4fbL, 0x4369e96aL,
	0x346ed9fcL, 0xad678846L, 0xda60b8d0L, 0x44042d73L, 0x33031de5L,
	0xaa0a4c5fL, 0xdd0d7cc9L, 0x5005713cL, 0x270241aaL, 0xbe0b1010L,
	0xc90c2086L, 0x5768b525L, 0x206f85b3L, 0xb966d409L, 0xce61e49fL,
	0x5edef90eL, 0x29d9c998L, 0xb0d09822L, 0xc7d7a8b4L, 0x59b33d17L,
	0x2eb40d81L, 0xb7bd5c3bL, 0xc0ba6cadL, 0xedb88320L, 0x9abfb3b6L,
	0x03b6e20cL, 0x74b1d29aL, 0xead54739L, 0x9dd277afL, 0x04db2615L,
	0x73dc1683L, 0xe3630b12L, 0x94643b84L, 0x0d6d6a3eL, 0x7a6a5aa8L,
	0xe40ecf0bL, 0x9309ff9dL, 0x0a00ae27L, 0x7d079eb1L, 0xf00f9344L,
	0x8708a3d2L, 0x1e01f268L, 0x6906c2feL, 0xf762575dL, 0x806567cbL,
	0x196c3671L, 0x6e6b06e7L, 0xfed41b76L, 0x89d32be0L, 0x10da7a5aL,
	0x67dd4accL, 0xf9b9df6fL, 0x8ebeeff9L, 0x17b7be43L, 0x60b08ed5L,
	0xd6d6a3e8L, 0xa1d1937eL, 0x38d8c2c4L, 0x4fdff252L, 0xd1bb67f1L,
	0xa6bc5767L, 0x3fb506ddL, 0x48b2364bL, 0xd80d2bdaL, 0xaf0a1b4cL,
	0x36034af6L, 0x41047a60L, 0xdf60efc3L, 0xa867df55L, 0x316e8eefL,
	0x4669be79L, 0xcb61b38cL, 0xbc66831aL, 0x256fd2a0L, 0x5268e236L,
	0xcc0c7795L, 0xbb0b4703L, 0x220216b9L, 0x5505262fL, 0xc5ba3bbeL,
	0xb2bd0b28L, 0x2bb45a92L, 0x5cb36a04L, 0xc2d7ffa7L, 0xb5d0cf31L,
	0x2cd99e8bL, 0x5bdeae1dL, 0x9b64c2b0L, 0xec63f226L, 0x756aa39cL,
	0x026d930aL, 0x9c0906a9L, 0xeb0e363fL, 0x72076785L, 0x05005713L,
	0x95bf4a82L, 0xe2b87a14L, 0x7bb12baeL, 0x0cb61b38L, 0x92d28e9bL,
	0xe5d5be0dL, 0x7cdcefb7L, 0x0bdbdf21L, 0x86d3d2d4L, 0xf1d4e242L,
	0x68ddb3f8L, 0x1fda836eL, 0x81be16cdL, 0xf6b9265bL, 0x6fb077e1L,
	0x18b74777L, 0x88085ae6L, 0xff0f6a70L, 0x66063bcaL, 0x11010b5cL,
	0x8f659effL, 0xf862ae69L, 0x616bffd3L, 0x166ccf45L, 0xa00ae278L,
	0xd70dd2eeL, 0x4e048354L, 0x3903b3c2L, 0xa7672661L, 0xd06016f7L,
	0x4969474dL, 0x3e6e77dbL, 0xaed16a4aL, 0xd9d65adcL, 0x40df0b66L,
	0x37d83bf0L, 0xa9bcae53L, 0xdebb9ec5L, 0x47b2cf7fL, 0x30b5ffe9L,
	0xbdbdf21cL, 0xcabac28aL, 0x53b39330L, 0x24b4a3a6L, 0xbad03605L,
	0xcdd70693L, 0x54de5729L, 0x23d967bfL, 0xb3667a2eL, 0xc4614ab8L,
	0x5d681b02L, 0x2a6f2b94L, 0xb40bbe37L, 0xc30c8ea1L, 0x5a05df1bL,
	0x2d02ef8dL
};

#define RC4STATE 256
#define RC4KEYLEN 16
#define RC4SWAP(x,y) \
    do { u_int8_t t = state[x]; state[x] = state[y]; state[y] = t; } while(0)

static void
wi_do_hostencrypt(struct wi_softc *sc, caddr_t buf, int len)
{
	u_int32_t i, crc, klen;
	u_int8_t state[RC4STATE], key[RC4KEYLEN];
	u_int8_t x, y, *dat;

	if (!sc->wi_icv_flag) {
		sc->wi_icv = arc4random();
		sc->wi_icv_flag++;
        } else
		sc->wi_icv++;
	/*
	 * Skip 'bad' IVs from Fluhrer/Mantin/Shamir:
	 * (B, 255, N) with 3 <= B < 8
	 */
	if (sc->wi_icv >= 0x03ff00 &&
            (sc->wi_icv & 0xf8ff00) == 0x00ff00)
                sc->wi_icv += 0x000100;

	/* prepend 24bit IV to tx key, byte order does not matter */
	key[0] = sc->wi_icv >> 16;
	key[1] = sc->wi_icv >> 8;
	key[2] = sc->wi_icv;

	klen = sc->wi_keys.wi_keys[sc->wi_tx_key].wi_keylen +
	    IEEE80211_WEP_IVLEN;
	klen = (klen >= RC4KEYLEN) ? RC4KEYLEN : RC4KEYLEN/2;
	bcopy((char *)&sc->wi_keys.wi_keys[sc->wi_tx_key].wi_keydat,
	    (char *)key + IEEE80211_WEP_IVLEN, klen - IEEE80211_WEP_IVLEN);

	/* rc4 keysetup */
	x = y = 0;
	for (i = 0; i < RC4STATE; i++)
		state[i] = i;
	for (i = 0; i < RC4STATE; i++) {
		y = (key[x] + state[i] + y) % RC4STATE;
		RC4SWAP(i, y);
		x = (x + 1) % klen;
	}

	/* output: IV, tx keyid, rc4(data), rc4(crc32(data)) */
	dat = buf;
	dat[0] = key[0];
	dat[1] = key[1];
	dat[2] = key[2];
	dat[3] = sc->wi_tx_key << 6;		/* pad and keyid */
	dat += 4;

	/* compute rc4 over data, crc32 over data */
	crc = ~0;
	x = y = 0;
	for (i = 0; i < len; i++) {
		x = (x + 1) % RC4STATE;
		y = (state[x] + y) % RC4STATE;
		RC4SWAP(x, y);
		crc = crc32_tab[(crc ^ dat[i]) & 0xff] ^ (crc >> 8);
		dat[i] ^= state[(state[x] + state[y]) % RC4STATE];
	}
	crc = ~crc;
	dat += len;

	/* append little-endian crc32 and encrypt */
	dat[0] = crc;
	dat[1] = crc >> 8;
	dat[2] = crc >> 16;
	dat[3] = crc >> 24;
	for (i = 0; i < IEEE80211_WEP_CRCLEN; i++) {
		x = (x + 1) % RC4STATE;
		y = (state[x] + y) % RC4STATE;
		RC4SWAP(x, y);
		dat[i] ^= state[(state[x] + state[y]) % RC4STATE];
	}
}

STATIC void
wi_start(ifp)
	struct ifnet		*ifp;
{
	struct wi_softc		*sc;
	struct mbuf		*m0;
	struct wi_frame		tx_frame;
	struct ether_header	*eh;
	int			id;

	sc = ifp->if_softc;

	DPRINTF(WID_START, ("wi_start: ifp %p sc %p\n", ifp, sc));

	if (!(sc->wi_flags & WI_FLAGS_ATTACHED))
		return;

	if (ifp->if_flags & IFF_OACTIVE)
		return;

nextpkt:
	IFQ_DEQUEUE(&ifp->if_snd, m0);
	if (m0 == NULL)
		return;

	bzero((char *)&tx_frame, sizeof(tx_frame));
	tx_frame.wi_frame_ctl = htole16(WI_FTYPE_DATA);
	id = sc->wi_tx_data_id;
	eh = mtod(m0, struct ether_header *);

	if (sc->wi_ptype == WI_PORTTYPE_HOSTAP) {
		if (!wihap_check_tx(&sc->wi_hostap_info, eh->ether_dhost,
		    &tx_frame.wi_tx_rate) && !(ifp->if_flags & IFF_PROMISC)) {
			if (ifp->if_flags & IFF_DEBUG)
				printf(WI_PRT_FMT
				    ": wi_start: dropping unassoc dst %s\n",
				    WI_PRT_ARG(sc),
				    ether_sprintf(eh->ether_dhost));
			m_freem(m0);
			goto nextpkt;
		}
	}

	/*
	 * Use RFC1042 encoding for IP and ARP datagrams,
	 * 802.3 for anything else.
	 */
	if (eh->ether_type == htons(ETHERTYPE_IP) ||
	    eh->ether_type == htons(ETHERTYPE_ARP) ||
	    eh->ether_type == htons(ETHERTYPE_REVARP) ||
	    eh->ether_type == htons(ETHERTYPE_IPV6)) {
		bcopy((char *)&eh->ether_dhost,
		    (char *)&tx_frame.wi_addr1, ETHER_ADDR_LEN);
		if (sc->wi_ptype == WI_PORTTYPE_HOSTAP) {
			tx_frame.wi_tx_ctl = htole16(WI_ENC_TX_MGMT); /* XXX */
			tx_frame.wi_frame_ctl |= htole16(WI_FCTL_FROMDS);
			if (sc->wi_use_wep)
				tx_frame.wi_frame_ctl |= htole16(WI_FCTL_WEP);
			bcopy((char *)&sc->sc_arpcom.ac_enaddr,
			    (char *)&tx_frame.wi_addr2, ETHER_ADDR_LEN);
			bcopy((char *)&eh->ether_shost,
			    (char *)&tx_frame.wi_addr3, ETHER_ADDR_LEN);
		} else
			bcopy((char *)&eh->ether_shost,
			    (char *)&tx_frame.wi_addr2, ETHER_ADDR_LEN);
		bcopy((char *)&eh->ether_dhost,
		    (char *)&tx_frame.wi_dst_addr, ETHER_ADDR_LEN);
		bcopy((char *)&eh->ether_shost,
		    (char *)&tx_frame.wi_src_addr, ETHER_ADDR_LEN);

		tx_frame.wi_dat_len = m0->m_pkthdr.len - WI_SNAPHDR_LEN;
		tx_frame.wi_dat[0] = htons(WI_SNAP_WORD0);
		tx_frame.wi_dat[1] = htons(WI_SNAP_WORD1);
		tx_frame.wi_len = htons(m0->m_pkthdr.len - WI_SNAPHDR_LEN);
		tx_frame.wi_type = eh->ether_type;

		if (sc->wi_ptype == WI_PORTTYPE_HOSTAP && sc->wi_use_wep) {

			/* Do host encryption. */
			bcopy(&tx_frame.wi_dat[0], &sc->wi_txbuf[4], 8);

			m_copydata(m0, sizeof(struct ether_header),
			    m0->m_pkthdr.len - sizeof(struct ether_header),
			    (caddr_t)&sc->wi_txbuf[12]);

			wi_do_hostencrypt(sc, (caddr_t)&sc->wi_txbuf,
			    tx_frame.wi_dat_len);

			tx_frame.wi_dat_len += IEEE80211_WEP_IVLEN +
			    IEEE80211_WEP_KIDLEN + IEEE80211_WEP_CRCLEN;

			tx_frame.wi_dat_len = htole16(tx_frame.wi_dat_len);
			wi_write_data(sc, id, 0, (caddr_t)&tx_frame,
			    sizeof(struct wi_frame));
			wi_write_data(sc, id, WI_802_11_OFFSET_RAW,
			    (caddr_t)&sc->wi_txbuf,
			    (m0->m_pkthdr.len -
			     sizeof(struct ether_header)) + 18);
		} else {
			m_copydata(m0, sizeof(struct ether_header),
			    m0->m_pkthdr.len - sizeof(struct ether_header),
			    (caddr_t)&sc->wi_txbuf);

			tx_frame.wi_dat_len = htole16(tx_frame.wi_dat_len);
			wi_write_data(sc, id, 0, (caddr_t)&tx_frame,
			    sizeof(struct wi_frame));
			wi_write_data(sc, id, WI_802_11_OFFSET,
			    (caddr_t)&sc->wi_txbuf,
			    (m0->m_pkthdr.len -
			     sizeof(struct ether_header)) + 2);
		}
	} else {
		tx_frame.wi_dat_len = htole16(m0->m_pkthdr.len);

		if (sc->wi_ptype == WI_PORTTYPE_HOSTAP && sc->wi_use_wep) {

			/* Do host encryption. (XXX - not implemented) */
			printf(WI_PRT_FMT
			    ": host encrypt not implemented for 802.3\n",
			    WI_PRT_ARG(sc));
		} else {
			m_copydata(m0, 0, m0->m_pkthdr.len,
			    (caddr_t)&sc->wi_txbuf);

			wi_write_data(sc, id, 0, (caddr_t)&tx_frame,
			    sizeof(struct wi_frame));
			wi_write_data(sc, id, WI_802_3_OFFSET,
			    (caddr_t)&sc->wi_txbuf, m0->m_pkthdr.len + 2);
		}
	}

#if NBPFILTER > 0
	/*
	 * If there's a BPF listner, bounce a copy of
	 * this frame to him.
	 */
	if (ifp->if_bpf)
		BPF_MTAP(ifp, m0);
#endif

	m_freem(m0);

	if (wi_cmd(sc, WI_CMD_TX|WI_RECLAIM, id, 0, 0))
		printf(WI_PRT_FMT ": wi_start: xmit failed\n", WI_PRT_ARG(sc));

	ifp->if_flags |= IFF_OACTIVE;

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;

	return;
}

STATIC int
wi_mgmt_xmit(sc, data, len)
	struct wi_softc		*sc;
	caddr_t			data;
	int			len;
{
	struct wi_frame		tx_frame;
	int			id;
	struct wi_80211_hdr	*hdr;
	caddr_t			dptr;

	if (!(sc->wi_flags & WI_FLAGS_ATTACHED))
		return(ENODEV);

	hdr = (struct wi_80211_hdr *)data;
	dptr = data + sizeof(struct wi_80211_hdr);

	bzero((char *)&tx_frame, sizeof(tx_frame));
	id = sc->wi_tx_mgmt_id;

	bcopy((char *)hdr, (char *)&tx_frame.wi_frame_ctl,
	   sizeof(struct wi_80211_hdr));

	tx_frame.wi_tx_ctl = htole16(WI_ENC_TX_MGMT);
	tx_frame.wi_dat_len = len - sizeof(struct wi_80211_hdr);
	tx_frame.wi_len = htole16(tx_frame.wi_dat_len);

	tx_frame.wi_dat_len = htole16(tx_frame.wi_dat_len);
	wi_write_data(sc, id, 0, (caddr_t)&tx_frame, sizeof(struct wi_frame));
	wi_write_data(sc, id, WI_802_11_OFFSET_RAW, dptr,
	    (len - sizeof(struct wi_80211_hdr)) + 2);

	if (wi_cmd(sc, WI_CMD_TX|WI_RECLAIM, id, 0, 0)) {
		printf(WI_PRT_FMT ": wi_mgmt_xmit: xmit failed\n",
		    WI_PRT_ARG(sc));
		return(EIO);
	}

	return(0);
}

STATIC void
wi_stop(sc)
	struct wi_softc		*sc;
{
	struct ifnet		*ifp;

	wihap_shutdown(sc);

	if (!(sc->wi_flags & WI_FLAGS_ATTACHED))
		return;

	DPRINTF(WID_STOP, ("wi_stop: sc %p\n", sc));

	timeout_del(&sc->sc_timo);

	ifp = &sc->sc_arpcom.ac_if;

	CSR_WRITE_2(sc, WI_INT_EN, 0);
	wi_cmd(sc, WI_CMD_DISABLE|sc->wi_portnum, 0, 0, 0);

	ifp->if_flags &= ~(IFF_RUNNING|IFF_OACTIVE);
	ifp->if_timer = 0;

	return;
}

STATIC void
wi_watchdog(ifp)
	struct ifnet		*ifp;
{
	struct wi_softc		*sc;

	sc = ifp->if_softc;

	printf(WI_PRT_FMT ": device timeout\n", WI_PRT_ARG(sc));

	wi_init(sc);

	ifp->if_oerrors++;

	return;
}

STATIC void
wi_shutdown(arg)
	void			*arg;
{
	struct wi_softc		*sc;

	sc = arg;
	wi_stop(sc);

	return;
}

STATIC void
wi_get_id(sc)
	struct wi_softc *sc;
{
	struct wi_ltv_ver	ver;
	u_int16_t		pri_fw_ver[3];
	const char		*p;

	/* get chip identity */
	bzero(&ver, sizeof(ver));
	ver.wi_type = WI_RID_CARD_ID;
	ver.wi_len = 5;
	wi_read_record(sc, (struct wi_ltv_gen *)&ver);
	switch (letoh16(ver.wi_ver[0])) {
		case WI_NIC_EVB2:
			p = "PRISM I HFA3841(EVB2)";
			sc->sc_firmware_type = WI_INTERSIL;
			break;
		case WI_NIC_HWB3763:
			p = "PRISM II HWB3763 rev.B";
			sc->sc_firmware_type = WI_INTERSIL;
			break;
		case WI_NIC_HWB3163:
			p = "PRISM II HWB3163 rev.A";
			sc->sc_firmware_type = WI_INTERSIL;
			break;
		case WI_NIC_HWB3163B:
			p = "PRISM II HWB3163 rev.B";
			sc->sc_firmware_type = WI_INTERSIL;
			break;
		case WI_NIC_EVB3:
			p = "PRISM II  HFA3842(EVB3)";
			sc->sc_firmware_type = WI_INTERSIL;
			break;
		case WI_NIC_HWB1153:
			p = "PRISM I HFA1153";
			sc->sc_firmware_type = WI_INTERSIL;
			break;
		case WI_NIC_P2_SST:
			p = "PRISM II HWB3163 SST-flash";
			sc->sc_firmware_type = WI_INTERSIL;
			break;
		case WI_NIC_PRISM2_5:
			p = "PRISM 2.5 ISL3873";
			sc->sc_firmware_type = WI_INTERSIL;
			break;
		case WI_NIC_3874A:
			p = "PRISM 2.5 ISL3874A(PCI)";
			sc->sc_firmware_type = WI_INTERSIL;
			break;
		case WI_NIC_37300P:
			p = "PRISM 2.5 ISL37300P";
			sc->sc_firmware_type = WI_INTERSIL;
			break;
		default:
			if (ver.wi_ver[0] & htole16(0x8000)) {
				p = "Unknown PRISM2 chip";
				sc->sc_firmware_type = WI_INTERSIL;
			} else {
				sc->sc_firmware_type = WI_LUCENT;
			}
			break;
	}

	/* get primary firmware version (XXX - how to do Lucent?) */
	if (sc->sc_firmware_type != WI_LUCENT) {
		bzero(&ver, sizeof(ver));
		ver.wi_type = WI_RID_PRI_IDENTITY;
		ver.wi_len = 5;
		wi_read_record(sc, (struct wi_ltv_gen *)&ver);
		pri_fw_ver[0] = letoh16(ver.wi_ver[2]);
		pri_fw_ver[1] = letoh16(ver.wi_ver[3]);
		pri_fw_ver[2] = letoh16(ver.wi_ver[1]);
	}

	/* get station firmware version */
	bzero(&ver, sizeof(ver));
	ver.wi_type = WI_RID_STA_IDENTITY;
	ver.wi_len = 5;
	wi_read_record(sc, (struct wi_ltv_gen *)&ver);
	ver.wi_ver[1] = letoh16(ver.wi_ver[1]);
	ver.wi_ver[2] = letoh16(ver.wi_ver[2]);
	ver.wi_ver[3] = letoh16(ver.wi_ver[3]);
	sc->sc_sta_firmware_ver = ver.wi_ver[2] * 10000 +
	    ver.wi_ver[3] * 100 + ver.wi_ver[1];

	if (sc->sc_firmware_type == WI_INTERSIL &&
	    (sc->sc_sta_firmware_ver == 10102 || sc->sc_sta_firmware_ver == 20102)) {
		struct wi_ltv_str sver;
		char *p;

		bzero(&sver, sizeof(sver));
		sver.wi_type = WI_RID_SYMBOL_IDENTITY;
		sver.wi_len = 7;
		/* value should be something like "V2.00-11" */
		if (wi_read_record(sc, (struct wi_ltv_gen *)&sver) == 0 &&
		    *(p = (char *)sver.wi_str) >= 'A' &&
		    p[2] == '.' && p[5] == '-' && p[8] == '\0') {
			sc->sc_firmware_type = WI_SYMBOL;
			sc->sc_sta_firmware_ver = (p[1] - '0') * 10000 +
			    (p[3] - '0') * 1000 + (p[4] - '0') * 100 +
			    (p[6] - '0') * 10 + (p[7] - '0');
		}
	}

	if (sc->sc_firmware_type == WI_LUCENT) {
		printf("\n%s: Firmware %d.%d variant %d, ", WI_PRT_ARG(sc),
		    ver.wi_ver[2], ver.wi_ver[3], ver.wi_ver[1]);
	} else {
		printf("\n%s: %s%s, Firmware %d.%d.%d (primary), %d.%d.%d (station), ",
		    WI_PRT_ARG(sc),
		    sc->sc_firmware_type == WI_SYMBOL ? "Symbol " : "", p,
		    pri_fw_ver[0], pri_fw_ver[1], pri_fw_ver[2],
		    sc->sc_sta_firmware_ver / 10000,
		    (sc->sc_sta_firmware_ver % 10000) / 100,
		    sc->sc_sta_firmware_ver % 100);
	}

	return;
}

STATIC int
wi_sync_media(sc, ptype, txrate)
	struct wi_softc *sc;
	int ptype;
	int txrate;
{
	int media = sc->sc_media.ifm_cur->ifm_media;
	int options = IFM_OPTIONS(media);
	int subtype;

	switch (txrate) {
	case 1:
		subtype = IFM_IEEE80211_DS1;
		break;
	case 2:
		subtype = IFM_IEEE80211_DS2;
		break;
	case 3:
		subtype = IFM_AUTO;
		break;
	case 5:
		subtype = IFM_IEEE80211_DS5;
		break;
	case 11:
		subtype = IFM_IEEE80211_DS11;
		break;
	default:
		subtype = IFM_MANUAL;		/* Unable to represent */
		break;
	}

	options &= ~IFM_OMASK;
	switch (ptype) {
	case WI_PORTTYPE_BSS:
		/* default port type */
		break;
	case WI_PORTTYPE_ADHOC:
		options |= IFM_IEEE80211_ADHOC;
		break;
	case WI_PORTTYPE_HOSTAP:
		options |= IFM_IEEE80211_HOSTAP;
		break;
	case WI_PORTTYPE_IBSS:
		if (sc->wi_create_ibss)
			options |= IFM_IEEE80211_IBSSMASTER;
		else
			options |= IFM_IEEE80211_IBSS;
		break;
	default:
		subtype = IFM_MANUAL;		/* Unable to represent */
		break;
	}
	media = IFM_MAKEWORD(IFM_TYPE(media), subtype, options,
	IFM_INST(media));
	if (ifmedia_match(&sc->sc_media, media, sc->sc_media.ifm_mask) == NULL)
		return (EINVAL);
	ifmedia_set(&sc->sc_media, media);
	sc->wi_ptype = ptype;
	sc->wi_tx_rate = txrate;
	return (0);
}

STATIC int
wi_media_change(ifp)
	struct ifnet *ifp;
{
	struct wi_softc *sc = ifp->if_softc;
	int otype = sc->wi_ptype;
	int orate = sc->wi_tx_rate;
	int ocreate_ibss = sc->wi_create_ibss;

	if ((sc->sc_media.ifm_cur->ifm_media & IFM_IEEE80211_HOSTAP) &&
	    sc->sc_firmware_type != WI_INTERSIL)
		return (EINVAL);

	sc->wi_create_ibss = 0;

	switch (sc->sc_media.ifm_cur->ifm_media & IFM_OMASK) {
	case 0:
		sc->wi_ptype = WI_PORTTYPE_BSS;
		break;
	case IFM_IEEE80211_ADHOC:
		sc->wi_ptype = WI_PORTTYPE_ADHOC;
		break;
	case IFM_IEEE80211_HOSTAP:
		sc->wi_ptype = WI_PORTTYPE_HOSTAP;
		break;
	case IFM_IEEE80211_IBSSMASTER:
	case IFM_IEEE80211_IBSSMASTER|IFM_IEEE80211_IBSS:
		if (!(sc->wi_flags & WI_FLAGS_HAS_CREATE_IBSS))
			return (EINVAL);
		sc->wi_create_ibss = 1;
		/* FALLTHROUGH */
	case IFM_IEEE80211_IBSS:
		sc->wi_ptype = WI_PORTTYPE_IBSS;
		break;
	default:
		/* Invalid combination. */
		return (EINVAL);
	}

	switch (IFM_SUBTYPE(sc->sc_media.ifm_cur->ifm_media)) {
	case IFM_IEEE80211_DS1:
		sc->wi_tx_rate = 1;
		break;
	case IFM_IEEE80211_DS2:
		sc->wi_tx_rate = 2;
		break;
	case IFM_AUTO:
		sc->wi_tx_rate = 3;
		break;
	case IFM_IEEE80211_DS5:
		sc->wi_tx_rate = 5;
		break;
	case IFM_IEEE80211_DS11:
		sc->wi_tx_rate = 11;
		break;
	}

	if (sc->sc_arpcom.ac_if.if_flags & IFF_UP) {
		if (otype != sc->wi_ptype || orate != sc->wi_tx_rate ||
		    ocreate_ibss != sc->wi_create_ibss)
			wi_init(sc);
	}

	ifp->if_baudrate = ifmedia_baudrate(sc->sc_media.ifm_cur->ifm_media);

	return (0);
}

STATIC void
wi_media_status(ifp, imr)
	struct ifnet *ifp;
	struct ifmediareq *imr;
{
	struct wi_softc *sc = ifp->if_softc;

	if (!(sc->sc_arpcom.ac_if.if_flags & IFF_UP)) {
		imr->ifm_active = IFM_IEEE80211|IFM_NONE;
		imr->ifm_status = 0;
		return;
	}

	imr->ifm_active = sc->sc_media.ifm_cur->ifm_media;
	imr->ifm_status = IFM_AVALID|IFM_ACTIVE;
}

STATIC int
wi_set_nwkey(sc, nwkey)
	struct wi_softc *sc;
	struct ieee80211_nwkey *nwkey;
{
	int i, len, error;
	struct wi_req wreq;
	struct wi_ltv_keys *wk = (struct wi_ltv_keys *)&wreq;

	if (!(sc->wi_flags & WI_FLAGS_HAS_WEP))
		return ENODEV;
	if (nwkey->i_defkid <= 0 || nwkey->i_defkid > IEEE80211_WEP_NKID)
		return EINVAL;
	memcpy(wk, &sc->wi_keys, sizeof(*wk));
	for (i = 0; i < IEEE80211_WEP_NKID; i++) {
		if (nwkey->i_key[i].i_keydat == NULL)
			continue;
		len = nwkey->i_key[i].i_keylen;
		if (len > sizeof(wk->wi_keys[i].wi_keydat))
			return EINVAL;
		error = copyin(nwkey->i_key[i].i_keydat,
		    wk->wi_keys[i].wi_keydat, len);
		if (error)
			return error;
		wk->wi_keys[i].wi_keylen = htole16(len);
	}

	wk->wi_len = (sizeof(*wk) / 2) + 1;
	wk->wi_type = WI_RID_DEFLT_CRYPT_KEYS;
	if (sc->sc_arpcom.ac_if.if_flags & IFF_UP) {
		error = wi_write_record(sc, (struct wi_ltv_gen *)&wreq);
		if (error)
			return error;
	}
	if ((error = wi_setdef(sc, &wreq)))
		return (error);

	wreq.wi_len = 2;
	wreq.wi_type = WI_RID_TX_CRYPT_KEY;
	wreq.wi_val[0] = htole16(nwkey->i_defkid - 1);
	if (sc->sc_arpcom.ac_if.if_flags & IFF_UP) {
		error = wi_write_record(sc, (struct wi_ltv_gen *)&wreq);
		if (error)
			return error;
	}
	if ((error = wi_setdef(sc, &wreq)))
		return (error);

	wreq.wi_type = WI_RID_ENCRYPTION;
	wreq.wi_val[0] = htole16(nwkey->i_wepon);
	if (sc->sc_arpcom.ac_if.if_flags & IFF_UP) {
		error = wi_write_record(sc, (struct wi_ltv_gen *)&wreq);
		if (error)
			return error;
	}
	if ((error = wi_setdef(sc, &wreq)))
		return (error);

	if (sc->sc_arpcom.ac_if.if_flags & IFF_UP)
		wi_init(sc);
	return 0;
}

STATIC int
wi_get_nwkey(sc, nwkey)
	struct wi_softc *sc;
	struct ieee80211_nwkey *nwkey;
{
	int i, len, error;
	struct wi_ltv_keys *wk = &sc->wi_keys;

	if (!(sc->wi_flags & WI_FLAGS_HAS_WEP))
		return ENODEV;
	nwkey->i_wepon = sc->wi_use_wep;
	nwkey->i_defkid = sc->wi_tx_key + 1;

	/* do not show any keys to non-root user */
	error = suser(curproc->p_ucred, &curproc->p_acflag);
	for (i = 0; i < IEEE80211_WEP_NKID; i++) {
		if (nwkey->i_key[i].i_keydat == NULL)
			continue;
		/* error holds results of suser() for the first time */
		if (error)
			return error;
		len = letoh16(wk->wi_keys[i].wi_keylen);
		if (nwkey->i_key[i].i_keylen < len)
			return ENOSPC;
		nwkey->i_key[i].i_keylen = len;
		error = copyout(wk->wi_keys[i].wi_keydat,
		    nwkey->i_key[i].i_keydat, len);
		if (error)
			return error;
	}
	return 0;
}

STATIC int
wi_set_pm(struct wi_softc *sc, struct ieee80211_power *power)
{

	sc->wi_pm_enabled = power->i_enabled;
	sc->wi_max_sleep = power->i_maxsleep;

	if (sc->sc_arpcom.ac_if.if_flags & IFF_UP)
		wi_init(sc);

	return (0);
}

STATIC int
wi_get_pm(struct wi_softc *sc, struct ieee80211_power *power)
{

	power->i_enabled = sc->wi_pm_enabled;
	power->i_maxsleep = sc->wi_max_sleep;

	return (0);
}

STATIC int
wi_set_ssid(ws, id, len)
	struct ieee80211_nwid *ws;
	u_int8_t *id;
	int len;
{

	if (len > IEEE80211_NWID_LEN)
		return (EINVAL);
	ws->i_len = len;
	memcpy(ws->i_nwid, id, len);
	return (0);
}

STATIC int
wi_get_debug(sc, wreq)
	struct wi_softc		*sc;
	struct wi_req		*wreq;
{
	int			error = 0;

	wreq->wi_len = 1;

	switch (wreq->wi_type) {
	case WI_DEBUG_SLEEP:
		wreq->wi_len++;
		wreq->wi_val[0] = htole16(sc->wi_debug.wi_sleep);
		break;
	case WI_DEBUG_DELAYSUPP:
		wreq->wi_len++;
		wreq->wi_val[0] = htole16(sc->wi_debug.wi_delaysupp);
		break;
	case WI_DEBUG_TXSUPP:
		wreq->wi_len++;
		wreq->wi_val[0] = htole16(sc->wi_debug.wi_txsupp);
		break;
	case WI_DEBUG_MONITOR:
		wreq->wi_len++;
		wreq->wi_val[0] = htole16(sc->wi_debug.wi_monitor);
		break;
	case WI_DEBUG_LEDTEST:
		wreq->wi_len += 3;
		wreq->wi_val[0] = htole16(sc->wi_debug.wi_ledtest);
		wreq->wi_val[1] = htole16(sc->wi_debug.wi_ledtest_param0);
		wreq->wi_val[2] = htole16(sc->wi_debug.wi_ledtest_param1);
		break;
	case WI_DEBUG_CONTTX:
		wreq->wi_len += 2;
		wreq->wi_val[0] = htole16(sc->wi_debug.wi_conttx);
		wreq->wi_val[1] = htole16(sc->wi_debug.wi_conttx_param0);
		break;
	case WI_DEBUG_CONTRX:
		wreq->wi_len++;
		wreq->wi_val[0] = htole16(sc->wi_debug.wi_contrx);
		break;
	case WI_DEBUG_SIGSTATE:
		wreq->wi_len += 2;
		wreq->wi_val[0] = htole16(sc->wi_debug.wi_sigstate);
		wreq->wi_val[1] = htole16(sc->wi_debug.wi_sigstate_param0);
		break;
	case WI_DEBUG_CONFBITS:
		wreq->wi_len += 2;
		wreq->wi_val[0] = htole16(sc->wi_debug.wi_confbits);
		wreq->wi_val[1] = htole16(sc->wi_debug.wi_confbits_param0);
		break;
	default:
		error = EIO;
		break;
	}

	return (error);
}

STATIC int
wi_set_debug(sc, wreq)
	struct wi_softc		*sc;
	struct wi_req		*wreq;
{
	int				error = 0;
	u_int16_t			cmd, param0 = 0, param1 = 0;

	switch (wreq->wi_type) {
	case WI_DEBUG_RESET:
	case WI_DEBUG_INIT:
	case WI_DEBUG_CALENABLE:
		break;
	case WI_DEBUG_SLEEP:
		sc->wi_debug.wi_sleep = 1;
		break;
	case WI_DEBUG_WAKE:
		sc->wi_debug.wi_sleep = 0;
		break;
	case WI_DEBUG_CHAN:
		param0 = letoh16(wreq->wi_val[0]);
		break;
	case WI_DEBUG_DELAYSUPP:
		sc->wi_debug.wi_delaysupp = 1;
		break;
	case WI_DEBUG_TXSUPP:
		sc->wi_debug.wi_txsupp = 1;
		break;
	case WI_DEBUG_MONITOR:
		sc->wi_debug.wi_monitor = 1;
		break;
	case WI_DEBUG_LEDTEST:
		param0 = letoh16(wreq->wi_val[0]);
		param1 = letoh16(wreq->wi_val[1]);
		sc->wi_debug.wi_ledtest = 1;
		sc->wi_debug.wi_ledtest_param0 = param0;
		sc->wi_debug.wi_ledtest_param1 = param1;
		break;
	case WI_DEBUG_CONTTX:
		param0 = letoh16(wreq->wi_val[0]);
		sc->wi_debug.wi_conttx = 1;
		sc->wi_debug.wi_conttx_param0 = param0;
		break;
	case WI_DEBUG_STOPTEST:
		sc->wi_debug.wi_delaysupp = 0;
		sc->wi_debug.wi_txsupp = 0;
		sc->wi_debug.wi_monitor = 0;
		sc->wi_debug.wi_ledtest = 0;
		sc->wi_debug.wi_ledtest_param0 = 0;
		sc->wi_debug.wi_ledtest_param1 = 0;
		sc->wi_debug.wi_conttx = 0;
		sc->wi_debug.wi_conttx_param0 = 0;
		sc->wi_debug.wi_contrx = 0;
		sc->wi_debug.wi_sigstate = 0;
		sc->wi_debug.wi_sigstate_param0 = 0;
		break;
	case WI_DEBUG_CONTRX:
		sc->wi_debug.wi_contrx = 1;
		break;
	case WI_DEBUG_SIGSTATE:
		param0 = letoh16(wreq->wi_val[0]);
		sc->wi_debug.wi_sigstate = 1;
		sc->wi_debug.wi_sigstate_param0 = param0;
		break;
	case WI_DEBUG_CONFBITS:
		param0 = letoh16(wreq->wi_val[0]);
		param1 = letoh16(wreq->wi_val[1]);
		sc->wi_debug.wi_confbits = param0;
		sc->wi_debug.wi_confbits_param0 = param1;
		break;
	default:
		error = EIO;
		break;
	}

	if (error)
		return (error);

	cmd = WI_CMD_DEBUG | (wreq->wi_type << 8);
	error = wi_cmd(sc, cmd, param0, param1, 0);

	return (error);
}
