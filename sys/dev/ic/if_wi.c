/*	$OpenBSD: if_wi.c,v 1.18 2001/12/21 15:48:19 mickey Exp $	*/

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

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <machine/bus.h>

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
	"$OpenBSD: if_wi.c,v 1.18 2001/12/21 15:48:19 mickey Exp $";
#endif	/* lint */

#ifdef foo
static u_int8_t	wi_mcast_addr[6] = { 0x01, 0x60, 0x1D, 0x00, 0x01, 0x00 };
#endif

STATIC void wi_reset		__P((struct wi_softc *));
STATIC int wi_ioctl		__P((struct ifnet *, u_long, caddr_t));
STATIC void wi_start		__P((struct ifnet *));
STATIC void wi_watchdog		__P((struct ifnet *));
STATIC void wi_shutdown		__P((void *));
STATIC void wi_rxeof		__P((struct wi_softc *));
STATIC void wi_txeof		__P((struct wi_softc *, int));
STATIC void wi_update_stats	__P((struct wi_softc *));
STATIC void wi_setmulti		__P((struct wi_softc *));

STATIC int wi_cmd		__P((struct wi_softc *, int, int));
STATIC int wi_read_record	__P((struct wi_softc *, struct wi_ltv_gen *));
STATIC int wi_write_record	__P((struct wi_softc *, struct wi_ltv_gen *));
STATIC int wi_read_data		__P((struct wi_softc *, int,
					int, caddr_t, int));
STATIC int wi_write_data	__P((struct wi_softc *, int,
					int, caddr_t, int));
STATIC int wi_seek		__P((struct wi_softc *, int, int, int));
STATIC int wi_alloc_nicmem	__P((struct wi_softc *, int, int *));
STATIC void wi_inquire		__P((void *));
STATIC void wi_setdef		__P((struct wi_softc *, struct wi_req *));
STATIC int wi_mgmt_xmit		__P((struct wi_softc *, caddr_t, int));
STATIC void wi_get_id		__P((struct wi_softc *, int));

int	wi_intr			__P((void *));
int	wi_attach		__P((struct wi_softc *, int));
void	wi_init			__P((void *));
void	wi_stop			__P((struct wi_softc *));

/* Autoconfig definition of driver back-end */
struct cfdriver wi_cd = {
	NULL, "wi", DV_IFNET
};

int
wi_attach(sc, print_cis)
	struct wi_softc *sc;
	int print_cis;
{
	struct wi_ltv_macaddr	mac;
	struct wi_ltv_gen	gen;
	struct ifnet		*ifp;
	int			error;

	sc->wi_gone = 0;

	wi_reset(sc);

	/* Read the station address. */
	mac.wi_type = WI_RID_MAC_NODE;
	mac.wi_len = 4;
	error = wi_read_record(sc, (struct wi_ltv_gen *)&mac);
	if (error) {
		printf(": unable to read station address\n");
		return (error);
	}
	bcopy((char *)&mac.wi_mac_addr, (char *)&sc->arpcom.ac_enaddr,
	    ETHER_ADDR_LEN);

	wi_get_id(sc, print_cis);
	printf("address %s", ether_sprintf(sc->arpcom.ac_enaddr));

	ifp = &sc->arpcom.ac_if;
	bcopy(sc->sc_dev.dv_xname, ifp->if_xname, IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = wi_ioctl;
	ifp->if_start = wi_start;
	ifp->if_watchdog = wi_watchdog;
	ifp->if_baudrate = 10000000;
	IFQ_SET_READY(&ifp->if_snd);

	bzero(sc->wi_node_name, sizeof(sc->wi_node_name));
	bcopy(WI_DEFAULT_NODENAME, sc->wi_node_name,
	    sizeof(WI_DEFAULT_NODENAME) - 1);

	bzero(sc->wi_net_name, sizeof(sc->wi_net_name));
	bcopy(WI_DEFAULT_NETNAME, sc->wi_net_name,
	    sizeof(WI_DEFAULT_NETNAME) - 1);

	bzero(sc->wi_ibss_name, sizeof(sc->wi_ibss_name));
	bcopy(WI_DEFAULT_IBSS, sc->wi_ibss_name,
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
	 * Find out if we support WEP on this card.
	 */
	gen.wi_type = WI_RID_WEP_AVAIL;
	gen.wi_len = 2;
	if (wi_read_record(sc, &gen) == 0)
		sc->wi_has_wep = letoh16(gen.wi_val);
	timeout_set(&sc->sc_timo, wi_inquire, sc);

	bzero((char *)&sc->wi_stats, sizeof(sc->wi_stats));

	/*
	 * Call MI attach routines.
	 */
	if_attach(ifp);
	ether_ifattach(ifp);
	printf("\n");

#if NBPFILTER > 0
	BPFATTACH(&sc->arpcom.ac_if.if_bpf, ifp, DLT_EN10MB,
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

	ifp = &sc->arpcom.ac_if;

	if (!(ifp->if_flags & IFF_UP)) {
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
	struct wi_frame		rx_frame;
	struct mbuf		*m;
	int			id;

	ifp = &sc->arpcom.ac_if;

	id = CSR_READ_2(sc, WI_RX_FID);

	/* First read in the frame header */
	if (wi_read_data(sc, id, 0, (caddr_t)&rx_frame, sizeof(rx_frame))) {
		ifp->if_ierrors++;
		return;
	}

	if (rx_frame.wi_status & htole16(WI_STAT_ERRSTAT)) {
		ifp->if_ierrors++;
		return;
	}

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

	eh = mtod(m, struct ether_header *);
	m->m_pkthdr.rcvif = ifp;

	if (rx_frame.wi_status == htole16(WI_STAT_1042) ||
	    rx_frame.wi_status == htole16(WI_STAT_TUNNEL) ||
	    rx_frame.wi_status == htole16(WI_STAT_WMP_MSG)) {
		if ((letoh16(rx_frame.wi_dat_len) + WI_SNAPHDR_LEN) > MCLBYTES) {
			printf(WI_PRT_FMT ": oversized packet received "
			    "(wi_dat_len=%d, wi_status=0x%x)\n",
			    WI_PRT_ARG(sc), letoh16(rx_frame.wi_dat_len),
			    letoh16(rx_frame.wi_status));
			m_freem(m);
			ifp->if_ierrors++;
			return;
		}
		m->m_pkthdr.len = m->m_len =
		    letoh16(rx_frame.wi_dat_len) + WI_SNAPHDR_LEN;

		bcopy((char *)&rx_frame.wi_addr1,
		    (char *)&eh->ether_dhost, ETHER_ADDR_LEN);
		bcopy((char *)&rx_frame.wi_addr2,
		    (char *)&eh->ether_shost, ETHER_ADDR_LEN);
		bcopy((char *)&rx_frame.wi_type,
		    (char *)&eh->ether_type, sizeof(u_int16_t));

		if (wi_read_data(sc, id, WI_802_11_OFFSET,
		    mtod(m, caddr_t) + sizeof(struct ether_header),
		    m->m_len + 2)) {
			m_freem(m);
			ifp->if_ierrors++;
			return;
		}
	} else {
		if ((letoh16(rx_frame.wi_dat_len) +
		    sizeof(struct ether_header)) > MCLBYTES) {
			printf(WI_PRT_FMT ": oversized packet received "
			    "(wi_dat_len=%d, wi_status=0x%x)\n",
			    WI_PRT_ARG(sc), letoh16(rx_frame.wi_dat_len),
			    letoh16(rx_frame.wi_status));
			m_freem(m);
			ifp->if_ierrors++;
			return;
		}
		m->m_pkthdr.len = m->m_len =
		    letoh16(rx_frame.wi_dat_len) + sizeof(struct ether_header);

		if (wi_read_data(sc, id, WI_802_3_OFFSET,
		    mtod(m, caddr_t), m->m_len + 2)) {
			m_freem(m);
			ifp->if_ierrors++;
			return;
		}
	}

	ifp->if_ipackets++;

#if NBPFILTER > 0
	/* Handle BPF listeners. */
	if (ifp->if_bpf)
		BPF_MTAP(ifp, m);
#endif

	/* Receive packet. */
	ether_input_mbuf(ifp, m);

	return;
}

STATIC void
wi_txeof(sc, status)
	struct wi_softc		*sc;
	int			status;
{
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;

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
	ifp = &sc->arpcom.ac_if;

	timeout_add(&sc->sc_timo, hz * 60);

	/* Don't do this while we're transmitting */
	if (ifp->if_flags & IFF_OACTIVE)
		return;

	s = splnet();
	rv = wi_cmd(sc, WI_CMD_INQUIRE, WI_INFO_COUNTERS);
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

	ifp = &sc->arpcom.ac_if;

	id = CSR_READ_2(sc, WI_INFO_FID);

	wi_read_data(sc, id, 0, (char *)&gen, 4);

	if (gen.wi_type != WI_INFO_COUNTERS)
		return;

	/* Some card versions have a larger stats structure */
	len = (gen.wi_len - 1 < sizeof(sc->wi_stats) / 4) ?
	    gen.wi_len - 1 : sizeof(sc->wi_stats) / 4;

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
wi_cmd(sc, cmd, val)
	struct wi_softc		*sc;
	int			cmd;
	int			val;
{
	int			i, s = 0;

	/* Wait for the busy bit to clear. */
	for (i = 0; i < WI_TIMEOUT; i++) {
		if (!(CSR_READ_2(sc, WI_COMMAND) & WI_CMD_BUSY))
			break;
		DELAY(10);
	}

	CSR_WRITE_2(sc, WI_PARAM0, val);
	CSR_WRITE_2(sc, WI_PARAM1, 0);
	CSR_WRITE_2(sc, WI_PARAM2, 0);
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

	if (wi_cmd(sc, WI_CMD_INI, 0))
		printf(WI_PRT_FMT ": init failed\n", WI_PRT_ARG(sc));
	CSR_WRITE_2(sc, WI_INT_EN, 0);
	CSR_WRITE_2(sc, WI_EVENT_ACK, 0xFFFF);

	/* Calibrate timer. */
	WI_SETVAL(WI_RID_TICK_TIME, 8);

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

	if (sc->sc_prism2) {
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
	if (wi_cmd(sc, WI_CMD_ACCESS|WI_ACCESS_READ, ltv->wi_type))
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

	if (sc->sc_prism2) {
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
			oltv->wi_len = 2;
			oltv->wi_val = ltv->wi_val;
			break;
		case WI_RID_AUTH_CNTL:
			oltv->wi_len = 2;
			if (ltv->wi_val & htole16(0x01))
				oltv->wi_val = htole16(1);
			else if (ltv->wi_val & htole16(0x02))
				oltv->wi_val = htole16(2);
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
	int			i;
	struct wi_ltv_gen	p2ltv;

	if (sc->sc_prism2) {
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
			if (ltv->wi_val & htole16(0x01))
				p2ltv.wi_val = htole16(0x03);
			else
				p2ltv.wi_val = htole16(0x90);
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
				struct wi_ltv_str ws;
				struct wi_ltv_keys *wk = (struct wi_ltv_keys *)ltv;
				for (i = 0; i < 4; i++) {
					ws.wi_len = 4;
					ws.wi_type = WI_RID_P2_CRYPT_KEY0 + i;
					bcopy(&wk->wi_keys[i].wi_keydat,
					    ws.wi_str, 5);
					ws.wi_str[5] = '\0';
					error = wi_write_record(sc,
					    (struct wi_ltv_gen *)&ws);
					if (error)
						return (error);
				}
			}
			return (0);
		case WI_RID_AUTH_CNTL:
			p2ltv.wi_type = WI_RID_AUTH_CNTL;
			p2ltv.wi_len = 2;
			if (ltv->wi_val == htole16(1))
				p2ltv.wi_val = htole16(0x01);
			else if (ltv->wi_val == htole16(2))
				p2ltv.wi_val = htole16(0x02);
			ltv = &p2ltv;
			break;
		}
	}

	if (wi_seek(sc, ltv->wi_type, 0, WI_BAP1))
		return(EIO);

	CSR_WRITE_2(sc, WI_DATA1, ltv->wi_len);
	CSR_WRITE_2(sc, WI_DATA1, ltv->wi_type);

	ptr = (u_int8_t *)&ltv->wi_val;
	if (ltv->wi_len > 1)
		CSR_WRITE_RAW_2(sc, WI_DATA1, ptr, (ltv->wi_len-1) *2);

	if (wi_cmd(sc, WI_CMD_ACCESS|WI_ACCESS_WRITE, ltv->wi_type))
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

	if (wi_cmd(sc, WI_CMD_ALLOC_MEM, len)) {
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

	CSR_WRITE_2(sc, WI_EVENT_ACK, WI_EV_ALLOC);
	*id = CSR_READ_2(sc, WI_ALLOC_FID);

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

	ifp = &sc->arpcom.ac_if;

	bzero((char *)&mcast, sizeof(mcast));

	mcast.wi_type = WI_RID_MCAST_LIST;
	mcast.wi_len = ((ETHER_ADDR_LEN / 2) * 16) + 1;

	if (ifp->if_flags & IFF_ALLMULTI || ifp->if_flags & IFF_PROMISC) {
		wi_write_record(sc, (struct wi_ltv_gen *)&mcast);
		return;
	}

	ETHER_FIRST_MULTI(step, &sc->arpcom, enm);
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

STATIC void
wi_setdef(sc, wreq)
	struct wi_softc		*sc;
	struct wi_req		*wreq;
{
	struct sockaddr_dl	*sdl;
	struct ifaddr		*ifa;
	struct ifnet		*ifp;
	extern struct ifaddr	**ifnet_addrs;

	ifp = &sc->arpcom.ac_if;

	switch(wreq->wi_type) {
	case WI_RID_MAC_NODE:
		ifa = ifnet_addrs[ifp->if_index];
		sdl = (struct sockaddr_dl *)ifa->ifa_addr;
		bcopy((char *)&wreq->wi_val, LLADDR(sdl), ETHER_ADDR_LEN);
		bcopy((char *)&wreq->wi_val, (char *)&sc->arpcom.ac_enaddr,
		    ETHER_ADDR_LEN);
		break;
	case WI_RID_PORTTYPE:
		sc->wi_ptype = letoh16(wreq->wi_val[0]);
		break;
	case WI_RID_TX_RATE:
		sc->wi_tx_rate = letoh16(wreq->wi_val[0]);
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
		break;
	case WI_RID_OWN_CHNL:
		sc->wi_channel = letoh16(wreq->wi_val[0]);
		break;
	case WI_RID_NODENAME:
		bzero(sc->wi_node_name, sizeof(sc->wi_node_name));
		bcopy((char *)&wreq->wi_val[1], sc->wi_node_name, 30);
		break;
	case WI_RID_DESIRED_SSID:
		bzero(sc->wi_net_name, sizeof(sc->wi_net_name));
		bcopy((char *)&wreq->wi_val[1], sc->wi_net_name, 30);
		break;
	case WI_RID_OWN_SSID:
		bzero(sc->wi_ibss_name, sizeof(sc->wi_ibss_name));
		bcopy((char *)&wreq->wi_val[1], sc->wi_ibss_name, 30);
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
	case WI_RID_AUTH_CNTL:
		sc->wi_authtype = letoh16(wreq->wi_val[0]);
		break;
	case WI_RID_ROAMING_MODE:
		sc->wi_roaming = letoh16(wreq->wi_val[0]);
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
		break;
	}

	/* Reinitialize WaveLAN. */
	wi_init(sc);

	return;
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

	s = splimp();

	sc = ifp->if_softc;
	ifr = (struct ifreq *)data;

	if (sc->wi_gone) {
		splx(s);
		return(ENODEV);
	}

	DPRINTF (WID_IOCTL, ("wi_ioctl: command %lu data %p\n",
	    command, data));

	if ((error = ether_ioctl(ifp, &sc->arpcom, command, data)) > 0) {
		splx(s);
		return error;
	}

	switch(command) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			wi_init(sc);
			arp_ifinit(&sc->arpcom, ifa);
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
				WI_SETVAL(WI_RID_PROMISC, 1);
			} else if (ifp->if_flags & IFF_RUNNING &&
			    !(ifp->if_flags & IFF_PROMISC) &&
			    sc->wi_if_flags & IFF_PROMISC) {
				WI_SETVAL(WI_RID_PROMISC, 0);
			}
			wi_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING) {
				wi_stop(sc);
			}
		}
		sc->wi_if_flags = ifp->if_flags;
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		/* Update our multicast list. */
		error = (command == SIOCADDMULTI) ?
		    ether_addmulti(ifr, &sc->arpcom) :
		    ether_delmulti(ifr, &sc->arpcom);
		if (error == ENETRESET) {
			/*
			 * Multicast list has changed; set the hardware filter
			 * accordingly.
			 */
			wi_setmulti(sc);
			error = 0;
		}
		break;
	case SIOCGWAVELAN:
		error = copyin(ifr->ifr_data, &wreq, sizeof(wreq));
		if (error)
			break;
		if (wreq.wi_type == WI_RID_IFACE_STATS) {
			/* XXX native byte order */
			bcopy((char *)&sc->wi_stats, (char *)&wreq.wi_val,
			    sizeof(sc->wi_stats));
			wreq.wi_len = (sizeof(sc->wi_stats) / 2) + 1;
		} else if (wreq.wi_type == WI_RID_DEFLT_CRYPT_KEYS) {
			/* For non-root user, return all-zeroes keys */
			if (suser(p->p_ucred, &p->p_acflag))
				bzero((char *)&wreq,
					sizeof(struct wi_ltv_keys));
			else
				bcopy((char *)&sc->wi_keys, (char *)&wreq,
					sizeof(struct wi_ltv_keys));
		} else {
			if (wi_read_record(sc, (struct wi_ltv_gen *)&wreq)) {
				error = EINVAL;
				break;
			}
		}
		error = copyout(&wreq, ifr->ifr_data, sizeof(wreq));
		break;
	case SIOCSWAVELAN:
		error = suser(p->p_ucred, &p->p_acflag);
		if (error)
			break;
		error = copyin(ifr->ifr_data, &wreq, sizeof(wreq));
		if (error)
			break;
		if (wreq.wi_type == WI_RID_IFACE_STATS) {
			error = EINVAL;
			break;
		} else if (wreq.wi_type == WI_RID_MGMT_XMIT) {
			error = wi_mgmt_xmit(sc, (caddr_t)&wreq.wi_val,
			    wreq.wi_len);
		} else {
			error = wi_write_record(sc, (struct wi_ltv_gen *)&wreq);
			if (!error)
				wi_setdef(sc, &wreq);
		}
		break;
	default:
		error = EINVAL;
		break;
	}

	splx(s);
	return(error);
}

STATIC void
wi_init(xsc)
	void			*xsc;
{
	struct wi_softc		*sc = xsc;
	struct ifnet		*ifp = &sc->arpcom.ac_if;
	int			s;
	struct wi_ltv_macaddr	mac;
	int			id = 0;

	if (sc->wi_gone)
		return;

	DPRINTF(WID_INIT, ("wi_init: sc %p\n", sc));

	s = splimp();

	if (ifp->if_flags & IFF_RUNNING)
		wi_stop(sc);

	wi_reset(sc);

	/* Program max data length. */
	WI_SETVAL(WI_RID_MAX_DATALEN, sc->wi_max_data_len);

	/* Enable/disable IBSS creation. */
	WI_SETVAL(WI_RID_CREATE_IBSS, sc->wi_create_ibss);

	/* Set the port type. */
	WI_SETVAL(WI_RID_PORTTYPE, sc->wi_ptype);

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

	/* Roaming type */
	WI_SETVAL(WI_RID_ROAMING_MODE, sc->wi_roaming);

	/* Specify the IBSS name */
	WI_SETSTR(WI_RID_OWN_SSID, sc->wi_ibss_name);

	/* Specify the network name */
	WI_SETSTR(WI_RID_DESIRED_SSID, sc->wi_net_name);

	/* Specify the frequency to use */
	WI_SETVAL(WI_RID_OWN_CHNL, sc->wi_channel);

	/* Program the nodename. */
	WI_SETSTR(WI_RID_NODENAME, sc->wi_node_name);

	/* Set our MAC address. */
	mac.wi_len = 4;
	mac.wi_type = WI_RID_MAC_NODE;
	bcopy((char *)&sc->arpcom.ac_enaddr,
	   (char *)&mac.wi_mac_addr, ETHER_ADDR_LEN);
	wi_write_record(sc, (struct wi_ltv_gen *)&mac);

	/* Initialize promisc mode. */
	if (ifp->if_flags & IFF_PROMISC) {
		WI_SETVAL(WI_RID_PROMISC, 1);
	} else {
		WI_SETVAL(WI_RID_PROMISC, 0);
	}

	/* Configure WEP. */
	if (sc->wi_has_wep) {
		WI_SETVAL(WI_RID_ENCRYPTION, sc->wi_use_wep);
		WI_SETVAL(WI_RID_TX_CRYPT_KEY, sc->wi_tx_key);
		sc->wi_keys.wi_len = (sizeof(struct wi_ltv_keys) / 2) + 1;
		sc->wi_keys.wi_type = WI_RID_DEFLT_CRYPT_KEYS;
		wi_write_record(sc, (struct wi_ltv_gen *)&sc->wi_keys);
		if (sc->sc_prism2 && sc->wi_use_wep) {
			/*
			 * For Prism2 Firmware version less than 0.8 variant3.
			 * If promiscuous mode is disabled, the Prism2 chip
			 * does not work with WEP .
			 * I'm currently investigating the details of this.
			 * (ichiro@netbsd.org)
			 */
			 if (sc->sc_prism2_ver < 83 ) {
				/* firm ver < 0.8 variant 3 */
				WI_SETVAL(WI_RID_PROMISC, 1);
			 }
			 WI_SETVAL(WI_RID_AUTH_CNTL, sc->wi_authtype);
		}
	}

	/* Set multicast filter. */
	wi_setmulti(sc);

	/* Enable desired port */
	wi_cmd(sc, WI_CMD_ENABLE | sc->wi_portnum, 0);

	if (wi_alloc_nicmem(sc, 1518 + sizeof(struct wi_frame) + 8, &id))
		printf(WI_PRT_FMT ": tx buffer allocation failed\n",
		    WI_PRT_ARG(sc));
	sc->wi_tx_data_id = id;

	if (wi_alloc_nicmem(sc, 1518 + sizeof(struct wi_frame) + 8, &id))
		printf(WI_PRT_FMT ": mgmt. buffer allocation failed\n",
		    WI_PRT_ARG(sc));
	sc->wi_tx_mgmt_id = id;

	/* enable interrupts */
	CSR_WRITE_2(sc, WI_INT_EN, WI_INTRS);

	splx(s);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	timeout_add(&sc->sc_timo, hz * 60);

	return;
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

	if (sc->wi_gone)
		return;

	if (ifp->if_flags & IFF_OACTIVE)
		return;

	IFQ_DEQUEUE(&ifp->if_snd, m0);
	if (m0 == NULL)
		return;

	bzero((char *)&tx_frame, sizeof(tx_frame));
	id = sc->wi_tx_data_id;
	eh = mtod(m0, struct ether_header *);

	/*
	 * Use RFC1042 encoding for IP and ARP datagrams,
	 * 802.3 for anything else.
	 */
	if (ntohs(eh->ether_type) == ETHERTYPE_IP ||
	    ntohs(eh->ether_type) == ETHERTYPE_ARP ||
	    ntohs(eh->ether_type) == ETHERTYPE_REVARP ||
	    ntohs(eh->ether_type) == ETHERTYPE_IPV6) {
		bcopy((char *)&eh->ether_dhost,
		    (char *)&tx_frame.wi_addr1, ETHER_ADDR_LEN);
		bcopy((char *)&eh->ether_shost,
		    (char *)&tx_frame.wi_addr2, ETHER_ADDR_LEN);
		bcopy((char *)&eh->ether_dhost,
		    (char *)&tx_frame.wi_dst_addr, ETHER_ADDR_LEN);
		bcopy((char *)&eh->ether_shost,
		    (char *)&tx_frame.wi_src_addr, ETHER_ADDR_LEN);

		tx_frame.wi_dat_len = htole16(m0->m_pkthdr.len - WI_SNAPHDR_LEN);
		tx_frame.wi_frame_ctl = htole16(WI_FTYPE_DATA);
		tx_frame.wi_dat[0] = htons(WI_SNAP_WORD0);
		tx_frame.wi_dat[1] = htons(WI_SNAP_WORD1);
		tx_frame.wi_len = htons(m0->m_pkthdr.len - WI_SNAPHDR_LEN);
		tx_frame.wi_type = eh->ether_type;

		m_copydata(m0, sizeof(struct ether_header),
		    m0->m_pkthdr.len - sizeof(struct ether_header),
		    (caddr_t)&sc->wi_txbuf);

		wi_write_data(sc, id, 0, (caddr_t)&tx_frame,
		    sizeof(struct wi_frame));
		wi_write_data(sc, id, WI_802_11_OFFSET, (caddr_t)&sc->wi_txbuf,
		    (m0->m_pkthdr.len - sizeof(struct ether_header)) + 2);
	} else {
		tx_frame.wi_dat_len = htole16(m0->m_pkthdr.len);

		m_copydata(m0, 0, m0->m_pkthdr.len, (caddr_t)&sc->wi_txbuf);

		wi_write_data(sc, id, 0, (caddr_t)&tx_frame,
		    sizeof(struct wi_frame));
		wi_write_data(sc, id, WI_802_3_OFFSET, (caddr_t)&sc->wi_txbuf,
		    m0->m_pkthdr.len + 2);
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

	if (wi_cmd(sc, WI_CMD_TX|WI_RECLAIM, id))
		printf(WI_PRT_FMT ": xmit failed\n", WI_PRT_ARG(sc));

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

	if (sc->wi_gone)
		return(ENODEV);

	hdr = (struct wi_80211_hdr *)data;
	dptr = data + sizeof(struct wi_80211_hdr);

	bzero((char *)&tx_frame, sizeof(tx_frame));
	id = sc->wi_tx_mgmt_id;

	bcopy((char *)hdr, (char *)&tx_frame.wi_frame_ctl,
	   sizeof(struct wi_80211_hdr));

	tx_frame.wi_dat_len = htole16(len - WI_SNAPHDR_LEN);
	tx_frame.wi_len = htons(len - WI_SNAPHDR_LEN);

	wi_write_data(sc, id, 0, (caddr_t)&tx_frame, sizeof(struct wi_frame));
	wi_write_data(sc, id, WI_802_11_OFFSET_RAW, dptr,
	    (len - sizeof(struct wi_80211_hdr)) + 2);

	if (wi_cmd(sc, WI_CMD_TX|WI_RECLAIM, id)) {
		printf(WI_PRT_FMT ": xmit failed\n", WI_PRT_ARG(sc));
		return(EIO);
	}

	return(0);
}

STATIC void
wi_stop(sc)
	struct wi_softc		*sc;
{
	struct ifnet		*ifp;

	if (sc->wi_gone)
		return;

	DPRINTF(WID_STOP, ("wi_stop: sc %p\n", sc));

	ifp = &sc->arpcom.ac_if;

	CSR_WRITE_2(sc, WI_INT_EN, 0);
	wi_cmd(sc, WI_CMD_DISABLE|sc->wi_portnum, 0);

	timeout_del(&sc->sc_timo);

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
wi_get_id(sc, print_cis)
	struct wi_softc *sc;
	int print_cis;
{
	struct wi_ltv_ver       ver;
	struct wi_ltv_cis	cis;
	const char		*p;

	if (print_cis) {
		/*
		 * For PCI attachments the CIS strings won't have been printed
		 * so print them here.
		 */
		cis.wi_type = WI_RID_CIS;
		cis.wi_len = sizeof(cis.wi_cis);
		if (wi_read_record(sc, (struct wi_ltv_gen *)&cis) == 0) {
			char *cis_strings[3];

			cis_strings[0] = (char *)&cis.wi_cis[11];
			cis_strings[1] = cis_strings[0] +
			    strlen(cis_strings[0]) + 1;
			cis_strings[2] = cis_strings[1] +
			    strlen(cis_strings[1]) + 1;
			printf("\n%s: \"%s, %s, %s\"", WI_PRT_ARG(sc),
			    cis_strings[0], cis_strings[1], cis_strings[2]);
		}
	}

	/* get chip identity */
	bzero(&ver, sizeof(ver));
	ver.wi_type = WI_RID_CARD_ID;
	ver.wi_len = 5;
	wi_read_record(sc, (struct wi_ltv_gen *)&ver);
	switch (letoh16(ver.wi_ver[0])) {
		case WI_NIC_EVB2:
			p = "PRISM I HFA3841(EVB2)";
			sc->sc_prism2 = 1;
			break;
		case WI_NIC_HWB3763:
			p = "PRISM II HWB3763 rev.B";
			sc->sc_prism2 = 1;
			break;
		case WI_NIC_HWB3163:
			p = "PRISM II HWB3163 rev.A";
			sc->sc_prism2 = 1;
			break;
		case WI_NIC_HWB3163B:
			p = "PRISM II HWB3163 rev.B";
			sc->sc_prism2 = 1;
			break;
		case WI_NIC_EVB3:
			p = "PRISM II  HFA3842(EVB3)";
			sc->sc_prism2 = 1;
			break;
		case WI_NIC_HWB1153:
			p = "PRISM I HFA1153";
			sc->sc_prism2 = 1;
			break;
		case WI_NIC_P2_SST:
			p = "PRISM II HWB3163 SST-flash";
			sc->sc_prism2 = 1;
			break;
		case WI_NIC_PRISM2_5:
			p = "PRISM 2.5 ISL3873";
			sc->sc_prism2 = 1;
			break;
		case WI_NIC_3874A:
			p = "PRISM 2.5 ISL3874A(PCI)";
			sc->sc_prism2 = 1;
			break;
		default:
			p = "Lucent chip or unknown chip";
			sc->sc_prism2 = 0;
			break;
	}

	/* get firmware version */
	bzero(&ver, sizeof(ver));
	ver.wi_type = WI_RID_STA_IDENTITY;
	ver.wi_len = 5;
	wi_read_record(sc, (struct wi_ltv_gen *)&ver);
	ver.wi_ver[1] = letoh16(ver.wi_ver[1]);
	ver.wi_ver[2] = letoh16(ver.wi_ver[2]);
	ver.wi_ver[3] = letoh16(ver.wi_ver[3]);
	if (sc->sc_prism2) {
		printf("\n%s: %s, Firmware %i.%i variant %i, ",
		    WI_PRT_ARG(sc), p, ver.wi_ver[2],
		    ver.wi_ver[3], ver.wi_ver[1]);
		sc->sc_prism2_ver = ver.wi_ver[2] * 100 +
				    ver.wi_ver[3] *  10 + ver.wi_ver[1];
	} else {
		printf("\n%s: Firmware %i.%i variant %i, ", WI_PRT_ARG(sc),
		    ver.wi_ver[2], ver.wi_ver[3], ver.wi_ver[1]);
	}

	return;
}
