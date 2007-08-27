/*	$OpenBSD: ieee80211_output.c,v 1.57 2007/08/27 18:53:27 damien Exp $	*/
/*	$NetBSD: ieee80211_output.c,v 1.13 2004/05/31 11:02:55 dyoung Exp $	*/

/*-
 * Copyright (c) 2001 Atsushi Onoe
 * Copyright (c) 2002, 2003 Sam Leffler, Errno Consulting
 * Copyright (c) 2007 Damien Bergamini
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "bpfilter.h"
#include "vlan.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/endian.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_arp.h>
#include <net/if_llc.h>
#include <net/bpf.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#endif

#if NVLAN > 0
#include <net/if_types.h>
#include <net/if_vlan_var.h>
#endif

#include <net80211/ieee80211_var.h>

#include <dev/rndvar.h>

enum	ieee80211_edca_ac ieee80211_up_to_ac(struct ieee80211com *, int);
int	ieee80211_classify(struct ieee80211com *, struct mbuf *);
int	ieee80211_mgmt_output(struct ifnet *, struct ieee80211_node *,
	    struct mbuf *, int);
u_int8_t *ieee80211_add_rsn_body(u_int8_t *, struct ieee80211com *,
	    const struct ieee80211_node *, int);
struct	mbuf *ieee80211_getmbuf(int, int, u_int);
struct	mbuf *ieee80211_get_probe_req(struct ieee80211com *,
	    struct ieee80211_node *);
struct	mbuf *ieee80211_get_probe_resp(struct ieee80211com *,
	    struct ieee80211_node *);
struct	mbuf *ieee80211_get_auth(struct ieee80211com *,
	    struct ieee80211_node *, u_int16_t, u_int16_t);
struct	mbuf *ieee80211_get_deauth(struct ieee80211com *,
	    struct ieee80211_node *, u_int16_t);
struct	mbuf *ieee80211_get_assoc_req(struct ieee80211com *,
	    struct ieee80211_node *, int);
struct	mbuf *ieee80211_get_assoc_resp(struct ieee80211com *,
	    struct ieee80211_node *, u_int16_t);
struct	mbuf *ieee80211_get_disassoc(struct ieee80211com *,
	    struct ieee80211_node *, u_int16_t);
int	ieee80211_send_eapol_key(struct ieee80211com *, struct mbuf *,
	    struct ieee80211_node *);
u_int8_t *ieee80211_add_gtk_kde(u_int8_t *, const struct ieee80211_key *);
u_int8_t *ieee80211_add_pmkid_kde(u_int8_t *, const u_int8_t *);
struct	mbuf *ieee80211_get_eapol_key(int, int, u_int);


/*
 * IEEE 802.11 output routine. Normally this will directly call the
 * Ethernet output routine because 802.11 encapsulation is called
 * later by the driver. This function can be used to send raw frames
 * if the mbuf has been tagged with a 802.11 data link type.
 */
int
ieee80211_output(struct ifnet *ifp, struct mbuf *m, struct sockaddr *dst,
    struct rtentry *rt)
{
	u_int dlt = 0;
	int s, error = 0;
	struct m_tag *mtag;

	/* Interface has to be up and running */
	if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) !=
	    (IFF_UP | IFF_RUNNING)) {
		error = ENETDOWN;
		goto bad;
	}

	/* Try to get the DLT from a mbuf tag */
	if ((mtag = m_tag_find(m, PACKET_TAG_DLT, NULL)) != NULL) {
		dlt = *(u_int *)(mtag + 1);

		/* Fallback to ethernet for non-802.11 linktypes */
		if (!(dlt == DLT_IEEE802_11 || dlt == DLT_IEEE802_11_RADIO))
			goto fallback;

		/*
		 * Queue message on interface without adding any
		 * further headers, and start output if interface not
		 * yet active.
		 */
		s = splnet();
		IFQ_ENQUEUE(&ifp->if_snd, m, NULL, error);
		if (error) {
			/* mbuf is already freed */
			splx(s);
			printf("%s: failed to queue raw tx frame\n",
			    ifp->if_xname);
			return (error);
		}
		ifp->if_obytes += m->m_pkthdr.len;
		if (m->m_flags & M_MCAST)
			ifp->if_omcasts++;
		if ((ifp->if_flags & IFF_OACTIVE) == 0)
			(*ifp->if_start)(ifp);
		splx(s);

		return (error);
	}

 fallback:
	return (ether_output(ifp, m, dst, rt));

 bad:
	if (m)
		m_freem(m);
	return (error);
}

/*
 * Send a management frame to the specified node.  The node pointer
 * must have a reference as the pointer will be passed to the driver
 * and potentially held for a long time.  If the frame is successfully
 * dispatched to the driver, then it is responsible for freeing the
 * reference (and potentially free'ing up any associated storage).
 */
int
ieee80211_mgmt_output(struct ifnet *ifp, struct ieee80211_node *ni,
    struct mbuf *m, int type)
{
	struct ieee80211com *ic = (void *)ifp;
	struct ieee80211_frame *wh;

	if (ni == NULL)
		panic("null node");
	ni->ni_inact = 0;

	/*
	 * Yech, hack alert!  We want to pass the node down to the
	 * driver's start routine.  We could stick this in an m_tag
	 * and tack that on to the mbuf.  However that's rather
	 * expensive to do for every frame so instead we stuff it in
	 * the rcvif field since outbound frames do not (presently)
	 * use this.
	 */
	M_PREPEND(m, sizeof(struct ieee80211_frame), M_DONTWAIT);
	if (m == NULL)
		return ENOMEM;
	m->m_pkthdr.rcvif = (void *)ni;

	wh = mtod(m, struct ieee80211_frame *);
	wh->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_MGT | type;
	wh->i_fc[1] = IEEE80211_FC1_DIR_NODS;
	*(u_int16_t *)&wh->i_dur[0] = 0;
	*(u_int16_t *)&wh->i_seq[0] =
	    htole16(ni->ni_txseq << IEEE80211_SEQ_SEQ_SHIFT);
	ni->ni_txseq++;
	IEEE80211_ADDR_COPY(wh->i_addr1, ni->ni_macaddr);
	IEEE80211_ADDR_COPY(wh->i_addr2, ic->ic_myaddr);
	IEEE80211_ADDR_COPY(wh->i_addr3, ni->ni_bssid);

	if (ifp->if_flags & IFF_DEBUG) {
		/* avoid to print too many frames */
		if (ic->ic_opmode == IEEE80211_M_IBSS ||
#ifdef IEEE80211_DEBUG
		    ieee80211_debug > 1 ||
#endif
		    (type & IEEE80211_FC0_SUBTYPE_MASK) !=
		    IEEE80211_FC0_SUBTYPE_PROBE_RESP)
			printf("%s: sending %s to %s on channel %u mode %s\n",
			    ifp->if_xname,
			    ieee80211_mgt_subtype_name[
			    (type & IEEE80211_FC0_SUBTYPE_MASK)
			    >> IEEE80211_FC0_SUBTYPE_SHIFT],
			    ether_sprintf(ni->ni_macaddr),
			    ieee80211_chan2ieee(ic, ni->ni_chan),
			    ieee80211_phymode_name[
			    ieee80211_chan2mode(ic, ni->ni_chan)]);
	}

	IF_ENQUEUE(&ic->ic_mgtq, m);
	ifp->if_timer = 1;
	(*ifp->if_start)(ifp);
	return 0;
}

/*-
 * EDCA tables are computed using the following formulas:
 *
 * 1) EDCATable (non-AP QSTA)
 *
 * AC     CWmin 	   CWmax	   AIFSN  TXOP limit(ms)
 * -------------------------------------------------------------
 * AC_BK  aCWmin	   aCWmax	   7	  0
 * AC_BE  aCWmin	   aCWmax	   3	  0
 * AC_VI  (aCWmin+1)/2-1   aCWmin	   2	  agn=3.008 b=6.016 others=0
 * AC_VO  (aCWmin+1)/4-1   (aCWmin+1)/2-1  2	  agn=1.504 b=3.264 others=0
 *
 * 2) QAPEDCATable (QAP)
 *
 * AC     CWmin 	   CWmax	   AIFSN  TXOP limit(ms)
 * -------------------------------------------------------------
 * AC_BK  aCWmin	   aCWmax	   7	  0
 * AC_BE  aCWmin	   4*(aCWmin+1)-1  3	  0
 * AC_VI  (aCWmin+1)/2-1   aCWmin	   1	  agn=3.008 b=6.016 others=0
 * AC_VO  (aCWmin+1)/4-1   (aCWmin+1)/2-1  1	  agn=1.504 b=3.264 others=0
 *
 * and the following aCWmin/aCWmax values:
 *
 * PHY		aCWmin	aCWmax
 * ---------------------------
 * 11A		15	1023
 * 11B  	31	1023
 * 11G		15*	1023	(*) aCWmin(1)
 * FH		15	1023
 * Turbo A/G	7	1023	(Atheros proprietary mode)
 */
static const struct ieee80211_edca_ac_params
    ieee80211_edca_table[IEEE80211_MODE_MAX][EDCA_NUM_AC] = {
	[IEEE80211_MODE_FH] = {
		[EDCA_AC_BK] = { 4, 10, 7,   0 },
		[EDCA_AC_BE] = { 4, 10, 3,   0 },
		[EDCA_AC_VI] = { 3,  4, 2,   0 },
		[EDCA_AC_VO] = { 2,  3, 2,   0 }
	},
	[IEEE80211_MODE_11B] = {
		[EDCA_AC_BK] = { 5, 10, 7,   0 },
		[EDCA_AC_BE] = { 5, 10, 3,   0 },
		[EDCA_AC_VI] = { 4,  5, 2, 188 },
		[EDCA_AC_VO] = { 3,  4, 2, 102 }
	},
	[IEEE80211_MODE_11A] = {
		[EDCA_AC_BK] = { 4, 10, 7,   0 },
		[EDCA_AC_BE] = { 4, 10, 3,   0 },
		[EDCA_AC_VI] = { 3,  4, 2,  94 },
		[EDCA_AC_VO] = { 2,  3, 2,  47 }
	},
	[IEEE80211_MODE_11G] = {
		[EDCA_AC_BK] = { 4, 10, 7,   0 },
		[EDCA_AC_BE] = { 4, 10, 3,   0 },
		[EDCA_AC_VI] = { 3,  4, 2,  94 },
		[EDCA_AC_VO] = { 2,  3, 2,  47 }
	},
	[IEEE80211_MODE_TURBO] = {
		[EDCA_AC_BK] = { 3, 10, 7,   0 },
		[EDCA_AC_BE] = { 3, 10, 2,   0 },
		[EDCA_AC_VI] = { 2,  3, 2,  94 },
		[EDCA_AC_VO] = { 2,  2, 1,  47 }
	}
};

static const struct ieee80211_edca_ac_params
    ieee80211_qap_edca_table[IEEE80211_MODE_MAX][EDCA_NUM_AC] = {
	[IEEE80211_MODE_FH] = {
		[EDCA_AC_BK] = { 4, 10, 7,   0 },
		[EDCA_AC_BE] = { 4,  6, 3,   0 },
		[EDCA_AC_VI] = { 3,  4, 1,   0 },
		[EDCA_AC_VO] = { 2,  3, 1,   0 }
	},
	[IEEE80211_MODE_11B] = {
		[EDCA_AC_BK] = { 5, 10, 7,   0 },
		[EDCA_AC_BE] = { 5,  7, 3,   0 },
		[EDCA_AC_VI] = { 4,  5, 1, 188 },
		[EDCA_AC_VO] = { 3,  4, 1, 102 }
	},
	[IEEE80211_MODE_11A] = {
		[EDCA_AC_BK] = { 4, 10, 7,   0 },
		[EDCA_AC_BE] = { 4,  6, 3,   0 },
		[EDCA_AC_VI] = { 3,  4, 1,  94 },
		[EDCA_AC_VO] = { 2,  3, 1,  47 }
	},
	[IEEE80211_MODE_11G] = {
		[EDCA_AC_BK] = { 4, 10, 7,   0 },
		[EDCA_AC_BE] = { 4,  6, 3,   0 },
		[EDCA_AC_VI] = { 3,  4, 1,  94 },
		[EDCA_AC_VO] = { 2,  3, 1,  47 }
	},
	[IEEE80211_MODE_TURBO] = {
		[EDCA_AC_BK] = { 3, 10, 7,   0 },
		[EDCA_AC_BE] = { 3,  5, 2,   0 },
		[EDCA_AC_VI] = { 2,  3, 1,  94 },
		[EDCA_AC_VO] = { 2,  2, 1,  47 }
	}
};

/*
 * Return the EDCA Access Category to be used for transmitting a frame with
 * user-priority `up'.
 */
enum ieee80211_edca_ac
ieee80211_up_to_ac(struct ieee80211com *ic, int up)
{
	/* IEEE Std 802.11e-2005, table 20i */
	static const enum ieee80211_edca_ac up_to_ac[] = {
		EDCA_AC_BE,	/* BE */
		EDCA_AC_BK,	/* BK */
		EDCA_AC_BK,	/* -- */
		EDCA_AC_BE,	/* EE */
		EDCA_AC_VI,	/* CL */
		EDCA_AC_VI,	/* VI */
		EDCA_AC_VO,	/* VO */
		EDCA_AC_VO	/* NC */
	};
	enum ieee80211_edca_ac ac;

	ac = (up <= 7) ? up_to_ac[up] : EDCA_AC_BE;

	if (ic->ic_opmode == IEEE80211_M_HOSTAP)
		return ac;

	/*
	 * We do not support the admission control procedure defined in
	 * IEEE Std 802.11e-2005 section 9.9.3.1.2.  The spec says that
	 * non-AP QSTAs that don't support this procedure shall use EDCA
	 * parameters of a lower priority AC that does not require
	 * admission control.
	 */
	while (ac != EDCA_AC_BK && ic->ic_edca_ac[ac].ac_acm) {
		switch (ac) {
		case EDCA_AC_BK:
			/* can't get there */
			break;
		case EDCA_AC_BE:
			/* BE shouldn't require admission control */
			ac = EDCA_AC_BK;
			break;
		case EDCA_AC_VI:
			ac = EDCA_AC_BE;
			break;
		case EDCA_AC_VO:
			ac = EDCA_AC_VI;
			break;
		}
	}
	return ac;
}

/*
 * Get mbuf's user-priority: if mbuf is not VLAN tagged, select user-priority
 * based on the DSCP (Differentiated Services Codepoint) field.
 */
int
ieee80211_classify(struct ieee80211com *ic, struct mbuf *m)
{
#ifdef INET
	const struct ether_header *eh;
#endif
#if NVLAN > 0
	if ((m->m_flags & M_PROTO1) == M_PROTO1 && m->m_pkthdr.rcvif != NULL) {
		const struct ifvlan *ifv = m->m_pkthdr.rcvif->if_softc;

		/* use VLAN 802.1D user-priority */
		if (ifv->ifv_prio <= 7)
			return ifv->ifv_prio;
	}
#endif
#ifdef INET
	eh = mtod(m, struct ether_header *);
	if (eh->ether_type == htons(ETHERTYPE_IP)) {
		const struct ip *ip = (const struct ip *)(eh + 1);
		/*
		 * Map Differentiated Services Codepoint field (see RFC2474).
		 * Preserves backward compatibility with IP Precedence field.
		 */
		switch (ip->ip_tos & 0xfc) {
		case IPTOS_PREC_PRIORITY:
			return 2;
		case IPTOS_PREC_IMMEDIATE:
			return 1;
		case IPTOS_PREC_FLASH:
			return 3;
		case IPTOS_PREC_FLASHOVERRIDE:
			return 4;
		case IPTOS_PREC_CRITIC_ECP:
			return 5;
		case IPTOS_PREC_INTERNETCONTROL:
			return 6;
		case IPTOS_PREC_NETCONTROL:
			return 7;
		}
	}
#endif
	return 0;	/* default to Best-Effort */
}

/*
 * Encapsulate an outbound data frame.  The mbuf chain is updated and
 * a reference to the destination node is returned.  If an error is
 * encountered NULL is returned and the node reference will also be NULL.
 *
 * NB: The caller is responsible for free'ing a returned node reference.
 *     The convention is ic_bss is not reference counted; the caller must
 *     maintain that.
 */
struct mbuf *
ieee80211_encap(struct ifnet *ifp, struct mbuf *m, struct ieee80211_node **pni)
{
	struct ieee80211com *ic = (void *)ifp;
	struct ether_header eh;
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni = NULL;
	struct llc *llc;
	struct m_tag *mtag;
	u_int8_t *addr;
	u_int dlt, hdrlen;
	int addqos, tid;

	/* Handle raw frames if mbuf is tagged as 802.11 */
	if ((mtag = m_tag_find(m, PACKET_TAG_DLT, NULL)) != NULL) {
		dlt = *(u_int *)(mtag + 1);

		if (!(dlt == DLT_IEEE802_11 || dlt == DLT_IEEE802_11_RADIO))
			goto fallback;

		wh = mtod(m, struct ieee80211_frame *);

		if (m->m_pkthdr.len < sizeof(struct ieee80211_frame_min))
			goto bad;

		if ((wh->i_fc[0] & IEEE80211_FC0_VERSION_MASK) !=
		    IEEE80211_FC0_VERSION_0)
			goto bad;

		switch (wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) {
		case IEEE80211_FC1_DIR_NODS:
		case IEEE80211_FC1_DIR_FROMDS:
			addr = wh->i_addr1;
			break;
		case IEEE80211_FC1_DIR_DSTODS:
		case IEEE80211_FC1_DIR_TODS:
			addr = wh->i_addr3;
			break;
		default:
			goto bad;
		}

		ni = ieee80211_find_txnode(ic, addr);
		if (ni == NULL)
			ni = ieee80211_ref_node(ic->ic_bss);
		if (ni == NULL) {
			printf("%s: no node for dst %s, "
			    "discard raw tx frame\n", ifp->if_xname,
			    ether_sprintf(addr));
			ic->ic_stats.is_tx_nonode++;
			goto bad;
		}
		ni->ni_inact = 0;

		*pni = ni;
		return (m);
	}

 fallback:
	if (m->m_len < sizeof(struct ether_header)) {
		m = m_pullup(m, sizeof(struct ether_header));
		if (m == NULL) {
			ic->ic_stats.is_tx_nombuf++;
			goto bad;
		}
	}
	memcpy(&eh, mtod(m, caddr_t), sizeof(struct ether_header));

	ni = ieee80211_find_txnode(ic, eh.ether_dhost);
	if (ni == NULL) {
		IEEE80211_DPRINTF(("%s: no node for dst %s, discard frame\n",
		    __func__, ether_sprintf(eh.ether_dhost)));
		ic->ic_stats.is_tx_nonode++;
		goto bad;
	}
#if 0
	if (!ni->ni_port_valid && eh.ether_type != htons(ETHERTYPE_PAE)) {
		IEEE80211_DPRINTF(("%s: port not valid: %s\n",
		    __func__, ether_sprintf(eh.ether_dhost)));
		ic->ic_stats.is_tx_noauth++;
		goto bad;
	}
#endif
	ni->ni_inact = 0;

	if ((ic->ic_flags & IEEE80211_F_QOS) &&
	    (ni->ni_flags & IEEE80211_NODE_QOS) &&
	    /* do not QoS-encapsulate EAPOL frames */
	    eh.ether_type != htons(ETHERTYPE_PAE)) {
		tid = ieee80211_classify(ic, m);
		hdrlen = sizeof(struct ieee80211_qosframe);
		addqos = 1;
	} else {
		hdrlen = sizeof(struct ieee80211_frame);
		addqos = 0;
	}
	m_adj(m, sizeof(struct ether_header) - sizeof(struct llc));
	llc = mtod(m, struct llc *);
	llc->llc_dsap = llc->llc_ssap = LLC_SNAP_LSAP;
	llc->llc_control = LLC_UI;
	llc->llc_snap.org_code[0] = 0;
	llc->llc_snap.org_code[1] = 0;
	llc->llc_snap.org_code[2] = 0;
	llc->llc_snap.ether_type = eh.ether_type;
	M_PREPEND(m, hdrlen, M_DONTWAIT);
	if (m == NULL) {
		ic->ic_stats.is_tx_nombuf++;
		goto bad;
	}
	wh = mtod(m, struct ieee80211_frame *);
	wh->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_DATA;
	*(u_int16_t *)&wh->i_dur[0] = 0;
	if (addqos) {
		struct ieee80211_qosframe *qwh =
		    (struct ieee80211_qosframe *)wh;
		qwh->i_fc[0] |= IEEE80211_FC0_SUBTYPE_QOS;
		qwh->i_qos[0] = tid & IEEE80211_QOS_TID;
		qwh->i_qos[1] = 0;	/* no TXOP requested */
		*(u_int16_t *)&qwh->i_seq[0] =
		    htole16(ni->ni_qos_txseqs[tid] << IEEE80211_SEQ_SEQ_SHIFT);
		ni->ni_qos_txseqs[tid]++;
	} else {
		*(u_int16_t *)&wh->i_seq[0] =
		    htole16(ni->ni_txseq << IEEE80211_SEQ_SEQ_SHIFT);
		ni->ni_txseq++;
	}
	switch (ic->ic_opmode) {
	case IEEE80211_M_STA:
		wh->i_fc[1] = IEEE80211_FC1_DIR_TODS;
		IEEE80211_ADDR_COPY(wh->i_addr1, ni->ni_bssid);
		IEEE80211_ADDR_COPY(wh->i_addr2, eh.ether_shost);
		IEEE80211_ADDR_COPY(wh->i_addr3, eh.ether_dhost);
		break;
	case IEEE80211_M_IBSS:
	case IEEE80211_M_AHDEMO:
		wh->i_fc[1] = IEEE80211_FC1_DIR_NODS;
		IEEE80211_ADDR_COPY(wh->i_addr1, eh.ether_dhost);
		IEEE80211_ADDR_COPY(wh->i_addr2, eh.ether_shost);
		IEEE80211_ADDR_COPY(wh->i_addr3, ic->ic_bss->ni_bssid);
		break;
	case IEEE80211_M_HOSTAP:
		wh->i_fc[1] = IEEE80211_FC1_DIR_FROMDS;
		IEEE80211_ADDR_COPY(wh->i_addr1, eh.ether_dhost);
		IEEE80211_ADDR_COPY(wh->i_addr2, ni->ni_bssid);
		IEEE80211_ADDR_COPY(wh->i_addr3, eh.ether_shost);
		break;
	case IEEE80211_M_MONITOR:
		goto bad;
	}
	if (ic->ic_flags & IEEE80211_F_WEPON)
		wh->i_fc[1] |= IEEE80211_FC1_WEP;
	*pni = ni;
	return m;
bad:
	if (m != NULL)
		m_freem(m);
	if (ni != NULL)
		ieee80211_release_node(ic, ni);
	*pni = NULL;
	return NULL;
}

/* unaligned little endian access */
#define LE_WRITE_2(p, v) do {			\
	((u_int8_t *)(p))[0] = (v) & 0xff;	\
	((u_int8_t *)(p))[1] = (v) >> 8;	\
} while (0)

/*
 * Add a Capability Information field to a frame (see 7.3.1.4).
 */
u_int8_t *
ieee80211_add_capinfo(u_int8_t *frm, struct ieee80211com *ic,
    const struct ieee80211_node *ni)
{
	u_int16_t capinfo;

	if (ic->ic_opmode == IEEE80211_M_IBSS)
		capinfo = IEEE80211_CAPINFO_IBSS;
	else if (ic->ic_opmode == IEEE80211_M_HOSTAP)
		capinfo = IEEE80211_CAPINFO_ESS;
	else
		capinfo = 0;
	if (ic->ic_flags & IEEE80211_F_WEPON)
		capinfo |= IEEE80211_CAPINFO_PRIVACY;
	/* NB: some 11a AP's reject the request when short preamble is set */
	if ((ic->ic_flags & IEEE80211_F_SHPREAMBLE) &&
	    IEEE80211_IS_CHAN_2GHZ(ni->ni_chan))
		capinfo |= IEEE80211_CAPINFO_SHORT_PREAMBLE;
	if (ic->ic_flags & IEEE80211_F_SHSLOT)
		capinfo |= IEEE80211_CAPINFO_SHORT_SLOTTIME;
	LE_WRITE_2(frm, capinfo);
	return frm + 2;
}

/*
 * Add an SSID element to a frame (see 7.3.2.1).
 */
u_int8_t *
ieee80211_add_ssid(u_int8_t *frm, const u_int8_t *ssid, u_int len)
{
	*frm++ = IEEE80211_ELEMID_SSID;
	*frm++ = len;
	memcpy(frm, ssid, len);
	return frm + len;
}

/*
 * Add a supported rates element to a frame (see 7.3.2.2).
 */
u_int8_t *
ieee80211_add_rates(u_int8_t *frm, const struct ieee80211_rateset *rs)
{
	int nrates;

	*frm++ = IEEE80211_ELEMID_RATES;
	nrates = min(rs->rs_nrates, IEEE80211_RATE_SIZE);
	*frm++ = nrates;
	memcpy(frm, rs->rs_rates, nrates);
	return frm + nrates;
}

/*
 * Add a FH Parameter Set element to a frame (see 7.3.2.3).
 */
u_int8_t *
ieee80211_add_fh_params(u_int8_t *frm, struct ieee80211com *ic,
    const struct ieee80211_node *ni)
{
	u_int chan = ieee80211_chan2ieee(ic, ni->ni_chan);

	*frm++ = IEEE80211_ELEMID_FHPARMS;
	*frm++ = 5;
	LE_WRITE_2(frm, ni->ni_fhdwell); frm += 2;
	*frm++ = IEEE80211_FH_CHANSET(chan);
	*frm++ = IEEE80211_FH_CHANPAT(chan);
	*frm++ = ni->ni_fhindex;
	return frm;
}

/*
 * Add a DS Parameter Set element to a frame (see 7.3.2.4).
 */
u_int8_t *
ieee80211_add_ds_params(u_int8_t *frm, struct ieee80211com *ic,
    const struct ieee80211_node *ni)
{
	*frm++ = IEEE80211_ELEMID_DSPARMS;
	*frm++ = 1;
	*frm++ = ieee80211_chan2ieee(ic, ni->ni_chan);
	return frm;
}

/*
 * Add a TIM element to a frame (see 7.3.2.6 and Annex L).
 */
u_int8_t *
ieee80211_add_tim(u_int8_t *frm, struct ieee80211com *ic)
{
	u_int i, offset = 0, len;

	/* find first non-zero octet in the virtual bit map */
	for (i = 0; i < ic->ic_tim_len && ic->ic_tim_bitmap[i] == 0; i++);

	/* clear the lsb as it is reserved for the broadcast indication bit */
	if (i < ic->ic_tim_len)
		offset = i & ~1;

	/* find last non-zero octet in the virtual bit map */
	for (i = ic->ic_tim_len - 1; i > 0 && ic->ic_tim_bitmap[i] == 0; i--);

	len = i - offset + 1;

	*frm++ = IEEE80211_ELEMID_TIM;
	*frm++ = len + 3;		/* length */
	*frm++ = ic->ic_dtim_count;	/* DTIM count */
	*frm++ = ic->ic_dtim_period;	/* DTIM period */

	/* Bitmap Control */
	*frm = offset;
	/* set broadcast/multicast indication bit if necessary */
	if (ic->ic_dtim_count == 0 && ic->ic_tim_mcast)
		*frm |= 0x01;
	frm++;

	/* Partial Virtual Bitmap */
	memcpy(frm, &ic->ic_tim_bitmap[offset], len);
	return frm + len;
}

/*
 * Add an IBSS Parameter Set element to a frame (see 7.3.2.7).
 */
u_int8_t *
ieee80211_add_ibss_params(u_int8_t *frm, const struct ieee80211_node *ni)
{
	*frm++ = IEEE80211_ELEMID_IBSSPARMS;
	*frm++ = 2;
	LE_WRITE_2(frm, 0);	/* TODO: ATIM window */
	return frm + 2;
}

/*
 * Add an EDCA Parameter Set element to a frame (see 7.3.2.29).
 */
u_int8_t *
ieee80211_add_edca_params(u_int8_t *frm, struct ieee80211com *ic)
{
	const struct ieee80211_edca_ac_params *edca;
	int aci;

	*frm++ = IEEE80211_ELEMID_EDCAPARMS;
	*frm++ = 18;	/* length */
	*frm++ = 0;	/* QoS Info */
	*frm++ = 0;	/* reserved */

	/* setup AC Parameter Records */
	edca = ieee80211_qap_edca_table[ic->ic_curmode];
	for (aci = 0; aci < EDCA_NUM_AC; aci++) {
		const struct ieee80211_edca_ac_params *ac = &edca[aci];

		*frm++ = (aci << 5) | ((ac->ac_acm & 0x1) << 4) |
			 (ac->ac_aifsn & 0xf);
		*frm++ = (ac->ac_ecwmax << 4) |
			 (ac->ac_ecwmin & 0xf);
		LE_WRITE_2(frm, ac->ac_txoplimit); frm += 2;
	}
	return frm;
}

/*
 * Add an ERP element to a frame (see 7.3.2.13).
 */
u_int8_t *
ieee80211_add_erp(u_int8_t *frm, struct ieee80211com *ic)
{
	u_int8_t erp;

	*frm++ = IEEE80211_ELEMID_ERP;
	*frm++ = 1;
	erp = 0;
	/*
	 * The NonERP_Present bit shall be set to 1 when a NonERP STA
	 * is associated with the BSS.
	 */
	if (ic->ic_nonerpsta != 0)
		erp |= IEEE80211_ERP_NON_ERP_PRESENT;
	/*
	 * If one or more NonERP STAs are associated in the BSS, the
	 * Use_Protection bit shall be set to 1 in transmitted ERP
	 * Information Elements.
	 */
	if (ic->ic_flags & IEEE80211_F_USEPROT)
		erp |= IEEE80211_ERP_USE_PROTECTION;
	/*
	 * The Barker_Preamble_Mode bit shall be set to 1 by the ERP
	 * Information Element sender if one or more associated NonERP
	 * STAs are not short preamble capable.
	 */
	if (!(ic->ic_flags & IEEE80211_F_SHPREAMBLE))
		erp |= IEEE80211_ERP_BARKER_MODE;
	*frm++ = erp;
	return frm;
}

/*
 * Add a QoS Capability element to a frame (see 7.3.2.35).
 */
u_int8_t *
ieee80211_add_qos_capability(u_int8_t *frm, struct ieee80211com *ic)
{
	*frm++ = IEEE80211_ELEMID_QOS_CAP;
	*frm++ = 1;
	*frm++ = 0;	/* QoS Info */
	return frm;
}

/*
 * Add an RSN element to a frame (see 7.3.2.25).
 */
u_int8_t *
ieee80211_add_rsn_body(u_int8_t *frm, struct ieee80211com *ic,
    const struct ieee80211_node *ni, int wpa1)
{
	const u_int8_t *oui = wpa1 ? MICROSOFT_OUI : IEEE80211_OUI;
	u_int8_t *pcount;
	u_int16_t count;

	/* write Version field */
	LE_WRITE_2(frm, 1); frm += 2;

	/* write Group Cipher Suite field (see Table 20da) */
	memcpy(frm, oui, 3); frm += 3;
	switch (ni->ni_group_cipher) {
	case IEEE80211_CIPHER_USEGROUP:
		/* can't get there */
		panic("invalid group cipher!");
		break;
	case IEEE80211_CIPHER_WEP40:
		*frm++ = 1;
		break;
	case IEEE80211_CIPHER_TKIP:
		*frm++ = 2;
		break;
	case IEEE80211_CIPHER_CCMP:
		*frm++ = 4;
		break;
	case IEEE80211_CIPHER_WEP104:
		*frm++ = 5;
		break;
	}

	pcount = frm; frm += 2;
	count = 0;
	/* write Pairwise Cipher Suite List */
	if (ni->ni_pairwise_cipherset & IEEE80211_CIPHER_USEGROUP) {
		memcpy(frm, oui, 3); frm += 3;
		*frm++ = 0;
		count++;
	}
	if (ni->ni_pairwise_cipherset & IEEE80211_CIPHER_TKIP) {
		memcpy(frm, oui, 3); frm += 3;
		*frm++ = 2;
		count++;
	}
	if (ni->ni_pairwise_cipherset & IEEE80211_CIPHER_CCMP) {
		memcpy(frm, oui, 3); frm += 3;
		*frm++ = 4;
		count++;
	}
	/* write Pairwise Cipher Suite Count field */
	LE_WRITE_2(pcount, count);

	pcount = frm; frm += 2;
	count = 0;
	/* write AKM Suite List (see Table 20dc) */
	if (ni->ni_akmset & IEEE80211_AKM_IEEE8021X) {
		memcpy(frm, oui, 3); frm += 3;
		*frm++ = 1;
		count++;
	}
	if (ni->ni_akmset & IEEE80211_AKM_PSK) {
		memcpy(frm, oui, 3); frm += 3;
		*frm++ = 2;
		count++;
	}
	/* write AKM Suite List Count field */
	LE_WRITE_2(pcount, count);

	/* write RSN Capabilities field */
	LE_WRITE_2(frm, ni->ni_rsncaps); frm += 2;

	/* no PMKID List for now */

	return frm;
}

u_int8_t *
ieee80211_add_rsn(u_int8_t *frm, struct ieee80211com *ic,
    const struct ieee80211_node *ni)
{
	u_int8_t *plen;

	*frm++ = IEEE80211_ELEMID_RSN;
	plen = frm++;	/* length filled in later */
	frm = ieee80211_add_rsn_body(frm, ic, ni, 0);

	/* write length field */
	*plen = frm - plen - 1;
	return frm;
}

/*
 * Add a vendor specific WPA1 element to a frame.
 * This is required for compatibility with Wi-Fi Alliance WPA1/WPA1+WPA2.
 */
u_int8_t *
ieee80211_add_wpa1(u_int8_t *frm, struct ieee80211com *ic,
    const struct ieee80211_node *ni)
{
	u_int8_t *plen;

	*frm++ = IEEE80211_ELEMID_VENDOR;
	plen = frm++;	/* length filled in later */
	memcpy(frm, MICROSOFT_OUI, 3); frm += 3;
	*frm++ = 1;	/* WPA1 */
	frm = ieee80211_add_rsn_body(frm, ic, ni, 1);

	/* write length field */
	*plen = frm - plen - 1;
	return frm;
}

/*
 * Add an extended supported rates element to a frame (see 7.3.2.14).
 */
u_int8_t *
ieee80211_add_xrates(u_int8_t *frm, const struct ieee80211_rateset *rs)
{
	int nrates;

	KASSERT(rs->rs_nrates > IEEE80211_RATE_SIZE);

	*frm++ = IEEE80211_ELEMID_XRATES;
	nrates = rs->rs_nrates - IEEE80211_RATE_SIZE;
	*frm++ = nrates;
	memcpy(frm, rs->rs_rates + IEEE80211_RATE_SIZE, nrates);
	return frm + nrates;
}

struct mbuf *
ieee80211_getmbuf(int flags, int type, u_int pktlen)
{
	struct mbuf *m;

	/* account for 802.11 header */
	pktlen += sizeof(struct ieee80211_frame);

	if (pktlen > MCLBYTES)
		panic("802.11 packet too large: %u", pktlen);
	MGETHDR(m, flags, type);
	if (m != NULL && pktlen > MHLEN) {
		MCLGET(m, flags);
		if (!(m->m_flags & M_EXT))
			m = m_free(m);
	}
	return m;
}

/*-
 * Probe request frame format:
 * [tlv] SSID
 * [tlv] Supported rates
 * [tlv] Extended Supported Rates (802.11g)
 */
struct mbuf *
ieee80211_get_probe_req(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	const struct ieee80211_rateset *rs =
	    &ic->ic_sup_rates[ieee80211_chan2mode(ic, ni->ni_chan)];
	struct mbuf *m;
	u_int8_t *frm;

	m = ieee80211_getmbuf(M_DONTWAIT, MT_DATA,
	    2 + ic->ic_des_esslen +
	    2 + min(rs->rs_nrates, IEEE80211_RATE_SIZE) +
	    ((rs->rs_nrates > IEEE80211_RATE_SIZE) ?
		2 + rs->rs_nrates - IEEE80211_RATE_SIZE : 0));
	if (m == NULL)
		return NULL;

	m->m_data += sizeof(struct ieee80211_frame);

	frm = mtod(m, u_int8_t *);
	frm = ieee80211_add_ssid(frm, ic->ic_des_essid, ic->ic_des_esslen);
	frm = ieee80211_add_rates(frm, rs);
	if (rs->rs_nrates > IEEE80211_RATE_SIZE)
		frm = ieee80211_add_xrates(frm, rs);

	m->m_pkthdr.len = m->m_len = frm - mtod(m, u_int8_t *);

	return m;
}

/*-
 * Probe response frame format:
 * [8]    Timestamp
 * [2]    Beacon interval
 * [2]    Capability
 * [tlv]  Service Set Identifier (SSID)
 * [tlv]  Supported rates
 * [tlv*] Frequency-Hopping (FH) Parameter Set
 * [tlv*] DS Parameter Set (802.11g)
 * [tlv]  ERP Information (802.11g)
 * [tlv]  Extended Supported Rates (802.11g)
 * [tlv]  RSN (802.11i)
 * [tlv]  EDCA Parameter Set (802.11e)
 */
struct mbuf *
ieee80211_get_probe_resp(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	const struct ieee80211_rateset *rs = &ic->ic_bss->ni_rates;
	struct mbuf *m;
	u_int8_t *frm;

	m = ieee80211_getmbuf(M_DONTWAIT, MT_DATA,
	    8 + 2 + 2 +
	    2 + ni->ni_esslen +
	    2 + min(rs->rs_nrates, IEEE80211_RATE_SIZE) +
	    2 + ((ic->ic_phytype == IEEE80211_T_FH) ? 5 : 1) +
	    ((ic->ic_opmode == IEEE80211_M_IBSS) ? 2 + 2 : 0) +
	    ((ic->ic_curmode == IEEE80211_MODE_11G) ? 2 + 1 : 0) +
	    ((rs->rs_nrates > IEEE80211_RATE_SIZE) ?
		2 + rs->rs_nrates - IEEE80211_RATE_SIZE : 0) +
	    ((ic->ic_flags & IEEE80211_F_RSN) ? 2 + 44 : 0) +
	    ((ic->ic_flags & IEEE80211_F_QOS) ? 2 + 18 : 0) +
	    ((ic->ic_flags & IEEE80211_F_WPA1) ? 2 + 48 : 0));
	if (m == NULL)
		return NULL;

	m->m_data += sizeof(struct ieee80211_frame);

	frm = mtod(m, u_int8_t *);
	memset(frm, 0, 8); frm += 8;	/* timestamp is set by hardware */
	LE_WRITE_2(frm, ic->ic_bss->ni_intval); frm += 2;
	frm = ieee80211_add_capinfo(frm, ic, ni);
	frm = ieee80211_add_ssid(frm, ic->ic_bss->ni_essid,
	    ic->ic_bss->ni_esslen);
	frm = ieee80211_add_rates(frm, rs);
	if (ic->ic_phytype == IEEE80211_T_FH)
		frm = ieee80211_add_fh_params(frm, ic, ni);
	else
		frm = ieee80211_add_ds_params(frm, ic, ni);
	if (ic->ic_opmode == IEEE80211_M_IBSS)
		frm = ieee80211_add_ibss_params(frm, ni);
	if (ic->ic_curmode == IEEE80211_MODE_11G)
		frm = ieee80211_add_erp(frm, ic);
	if (rs->rs_nrates > IEEE80211_RATE_SIZE)
		frm = ieee80211_add_xrates(frm, rs);
	if (ic->ic_flags & IEEE80211_F_RSN)
		frm = ieee80211_add_rsn(frm, ic, ic->ic_bss);
	if (ic->ic_flags & IEEE80211_F_QOS)
		frm = ieee80211_add_edca_params(frm, ic);
	if (ic->ic_flags & IEEE80211_F_WPA1)
		frm = ieee80211_add_wpa1(frm, ic, ic->ic_bss);

	m->m_pkthdr.len = m->m_len = frm - mtod(m, u_int8_t *);

	return m;
}

/*-
 * Authentication frame format:
 * [2] Authentication algorithm number
 * [2] Authentication transaction sequence number
 * [2] Status code
 */
struct mbuf *
ieee80211_get_auth(struct ieee80211com *ic, struct ieee80211_node *ni,
    u_int16_t status, u_int16_t seq)
{
	struct mbuf *m;
	u_int8_t *frm;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return NULL;
	MH_ALIGN(m, 2 * 3);
	m->m_pkthdr.len = m->m_len = 2 * 3;

	frm = mtod(m, u_int8_t *);
	LE_WRITE_2(frm, IEEE80211_AUTH_ALG_OPEN); frm += 2;
	LE_WRITE_2(frm, seq); frm += 2;
	LE_WRITE_2(frm, status);

	return m;
}

/*-
 * Deauthentication frame format:
 * [2] Reason code
 */
struct mbuf *
ieee80211_get_deauth(struct ieee80211com *ic, struct ieee80211_node *ni,
    u_int16_t reason)
{
	struct mbuf *m;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return NULL;
	MH_ALIGN(m, 2);

	m->m_pkthdr.len = m->m_len = 2;
	*mtod(m, u_int16_t *) = htole16(reason);

	return m;
}

/*-
 * (Re)Association request frame format:
 * [2]   Capability information
 * [2]   Listen interval
 * [6*]  Current AP address (Reassociation only)
 * [tlv] SSID
 * [tlv] Supported rates
 * [tlv] Extended Supported Rates (802.11g)
 * [tlv] RSN (802.11i)
 * [tlv] QoS Capability (802.11e)
 */
struct mbuf *
ieee80211_get_assoc_req(struct ieee80211com *ic, struct ieee80211_node *ni,
    int reassoc)
{
	const struct ieee80211_rateset *rs = &ni->ni_rates;
	struct mbuf *m;
	u_int8_t *frm;
	u_int16_t capinfo;

	m = ieee80211_getmbuf(M_DONTWAIT, MT_DATA,
	    2 +	2 +
	    ((reassoc == IEEE80211_FC0_SUBTYPE_REASSOC_REQ) ?
		IEEE80211_ADDR_LEN : 0) +
	    2 + ni->ni_esslen +
	    2 + min(rs->rs_nrates, IEEE80211_RATE_SIZE) +
	    ((rs->rs_nrates > IEEE80211_RATE_SIZE) ?
		2 + rs->rs_nrates - IEEE80211_RATE_SIZE : 0) +
	    ((ic->ic_flags & IEEE80211_F_RSN) ? 2 + 44 : 0) +
	    ((ic->ic_flags & IEEE80211_F_QOS) ? 2 + 1 : 0) +
	    ((ic->ic_flags & IEEE80211_F_WPA1) ? 2 + 48 : 0));
	if (m == NULL)
		return NULL;

	m->m_data += sizeof(struct ieee80211_frame);

	frm = mtod(m, u_int8_t *);
	capinfo = IEEE80211_CAPINFO_ESS;
	if (ic->ic_flags & IEEE80211_F_WEPON)
		capinfo |= IEEE80211_CAPINFO_PRIVACY;
	if ((ic->ic_flags & IEEE80211_F_SHPREAMBLE) &&
	    IEEE80211_IS_CHAN_2GHZ(ni->ni_chan))
		capinfo |= IEEE80211_CAPINFO_SHORT_PREAMBLE;
	if ((ni->ni_capinfo & IEEE80211_CAPINFO_SHORT_SLOTTIME) &&
	    (ic->ic_flags & IEEE80211_F_SHSLOT))
		capinfo |= IEEE80211_CAPINFO_SHORT_SLOTTIME;
	LE_WRITE_2(frm, capinfo); frm += 2;
	LE_WRITE_2(frm, ic->ic_lintval); frm += 2;
	if (reassoc == IEEE80211_FC0_SUBTYPE_REASSOC_REQ) {
		IEEE80211_ADDR_COPY(frm, ic->ic_bss->ni_bssid);
		frm += IEEE80211_ADDR_LEN;
	}
	frm = ieee80211_add_ssid(frm, ni->ni_essid, ni->ni_esslen);
	frm = ieee80211_add_rates(frm, rs);
	if (rs->rs_nrates > IEEE80211_RATE_SIZE)
		frm = ieee80211_add_xrates(frm, rs);
	if (ic->ic_flags & IEEE80211_F_RSN)
		frm = ieee80211_add_rsn(frm, ic, ic->ic_bss);
	if ((ic->ic_flags & IEEE80211_F_QOS) &&
	    (ni->ni_flags & IEEE80211_NODE_QOS))
		frm = ieee80211_add_qos_capability(frm, ic);
	if (ic->ic_flags & IEEE80211_F_WPA1)
		frm = ieee80211_add_wpa1(frm, ic, ic->ic_bss);

	m->m_pkthdr.len = m->m_len = frm - mtod(m, u_int8_t *);

	return m;
}

/*-
 * (Re)Association response frame format:
 * [2]   Capability information
 * [2]   Status code
 * [2]   Association ID (AID)
 * [tlv] Supported rates
 * [tlv] Extended Supported Rates (802.11g)
 * [tlv] EDCA Parameter Set (802.11e)
 */
struct mbuf *
ieee80211_get_assoc_resp(struct ieee80211com *ic, struct ieee80211_node *ni,
    u_int16_t status)
{
	const struct ieee80211_rateset *rs = &ni->ni_rates;
	struct mbuf *m;
	u_int8_t *frm;

	m = ieee80211_getmbuf(M_DONTWAIT, MT_DATA,
	    2 +	2 + 2 +
	    2 + min(rs->rs_nrates, IEEE80211_RATE_SIZE) +
	    ((rs->rs_nrates > IEEE80211_RATE_SIZE) ?
		2 + rs->rs_nrates - IEEE80211_RATE_SIZE : 0) +
	    ((ic->ic_flags & IEEE80211_F_QOS) ? 2 + 18 : 0));
	if (m == NULL)
		return NULL;

	m->m_data += sizeof(struct ieee80211_frame);

	frm = mtod(m, u_int8_t *);
	frm = ieee80211_add_capinfo(frm, ic, ni);
	LE_WRITE_2(frm, status); frm += 2;
	if (status == IEEE80211_STATUS_SUCCESS)
		LE_WRITE_2(frm, ni->ni_associd);
	else
		LE_WRITE_2(frm, 0);
	frm += 2;
	frm = ieee80211_add_rates(frm, rs);
	if (rs->rs_nrates > IEEE80211_RATE_SIZE)
		frm = ieee80211_add_xrates(frm, rs);
	if ((ic->ic_flags & IEEE80211_F_QOS) &&
	    (ni->ni_flags & IEEE80211_NODE_QOS))
		frm = ieee80211_add_edca_params(frm, ic);

	m->m_pkthdr.len = m->m_len = frm - mtod(m, u_int8_t *);

	return m;
}

/*-
 * Disassociation frame format:
 * [2] Reason code
 */
struct mbuf *
ieee80211_get_disassoc(struct ieee80211com *ic, struct ieee80211_node *ni,
    u_int16_t reason)
{
	struct mbuf *m;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return NULL;
	MH_ALIGN(m, 2);

	m->m_pkthdr.len = m->m_len = 2;
	*mtod(m, u_int16_t *) = htole16(reason);

	return m;
}

/*
 * Send a management frame.  The node is for the destination (or ic_bss
 * when in station mode).  Nodes other than ic_bss have their reference
 * count bumped to reflect our use for an indeterminant time.
 */
int
ieee80211_send_mgmt(struct ieee80211com *ic, struct ieee80211_node *ni,
    int type, int arg)
{
#define	senderr(_x, _v)	do { ic->ic_stats._v++; ret = _x; goto bad; } while (0)
	struct ifnet *ifp = &ic->ic_if;
	struct mbuf *m;
	int ret, timer;

	if (ni == NULL)
		panic("null node");

	/*
	 * Hold a reference on the node so it doesn't go away until after
	 * the xmit is complete all the way in the driver.  On error we
	 * will remove our reference.
	 */
	ieee80211_ref_node(ni);
	timer = 0;
	switch (type) {
	case IEEE80211_FC0_SUBTYPE_PROBE_REQ:
		if ((m = ieee80211_get_probe_req(ic, ni)) == NULL)
			senderr(ENOMEM, is_tx_nombuf);

		timer = IEEE80211_TRANS_WAIT;
		break;

	case IEEE80211_FC0_SUBTYPE_PROBE_RESP:
		if ((m = ieee80211_get_probe_resp(ic, ni)) == NULL)
			senderr(ENOMEM, is_tx_nombuf);
		break;

	case IEEE80211_FC0_SUBTYPE_AUTH:
		m = ieee80211_get_auth(ic, ni, arg >> 16, arg & 0xffff);
		if (m == NULL)
			senderr(ENOMEM, is_tx_nombuf);

		if (ic->ic_opmode == IEEE80211_M_STA)
			timer = IEEE80211_TRANS_WAIT;
		break;

	case IEEE80211_FC0_SUBTYPE_DEAUTH:
		if ((m = ieee80211_get_deauth(ic, ni, arg)) == NULL)
			senderr(ENOMEM, is_tx_nombuf);

		if (ifp->if_flags & IFF_DEBUG) {
			printf("%s: station %s deauthenticate (reason %d)\n",
			    ifp->if_xname, ether_sprintf(ni->ni_macaddr), arg);
		}
		break;

	case IEEE80211_FC0_SUBTYPE_ASSOC_REQ:
	case IEEE80211_FC0_SUBTYPE_REASSOC_REQ:
		if ((m = ieee80211_get_assoc_req(ic, ni, type)) == NULL)
			senderr(ENOMEM, is_tx_nombuf);

		timer = IEEE80211_TRANS_WAIT;
		break;

	case IEEE80211_FC0_SUBTYPE_ASSOC_RESP:
	case IEEE80211_FC0_SUBTYPE_REASSOC_RESP:
		if ((m = ieee80211_get_assoc_resp(ic, ni, arg)) == NULL)
			senderr(ENOMEM, is_tx_nombuf);
		break;

	case IEEE80211_FC0_SUBTYPE_DISASSOC:
		if ((m = ieee80211_get_disassoc(ic, ni, arg)) == NULL)
			senderr(ENOMEM, is_tx_nombuf);

		if (ifp->if_flags & IFF_DEBUG) {
			printf("%s: station %s disassociate (reason %d)\n",
			    ifp->if_xname, ether_sprintf(ni->ni_macaddr), arg);
		}
		break;

	default:
		IEEE80211_DPRINTF(("%s: invalid mgmt frame type %u\n",
		    __func__, type));
		senderr(EINVAL, is_tx_unknownmgt);
		/* NOTREACHED */
	}

	ret = ieee80211_mgmt_output(ifp, ni, m, type);
	if (ret == 0) {
		if (timer)
			ic->ic_mgt_timer = timer;
	} else {
bad:
		ieee80211_release_node(ic, ni);
	}
	return ret;
#undef senderr
}

/*
 * Build a RTS (Request To Send) control frame (see 7.2.1.1).
 */
struct mbuf *
ieee80211_get_rts(struct ieee80211com *ic, const struct ieee80211_frame *wh,
    u_int16_t dur)
{
	struct ieee80211_frame_rts *rts;
	struct mbuf *m;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return NULL;

	m->m_pkthdr.len = m->m_len = sizeof (struct ieee80211_frame_rts);

	rts = mtod(m, struct ieee80211_frame_rts *);
	rts->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_CTL |
	    IEEE80211_FC0_SUBTYPE_RTS;
	rts->i_fc[1] = IEEE80211_FC1_DIR_NODS;
	*(u_int16_t *)rts->i_dur = htole16(dur);
	IEEE80211_ADDR_COPY(rts->i_ra, wh->i_addr1);
	IEEE80211_ADDR_COPY(rts->i_ta, wh->i_addr2);

	return m;
}

/*
 * Build a CTS-to-self (Clear To Send) control frame (see 7.2.1.2).
 */
struct mbuf *
ieee80211_get_cts_to_self(struct ieee80211com *ic, u_int16_t dur)
{
	struct ieee80211_frame_cts *cts;
	struct mbuf *m;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return NULL;

	m->m_pkthdr.len = m->m_len = sizeof (struct ieee80211_frame_cts);

	cts = mtod(m, struct ieee80211_frame_cts *);
	cts->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_CTL |
	    IEEE80211_FC0_SUBTYPE_CTS;
	cts->i_fc[1] = IEEE80211_FC1_DIR_NODS;
	*(u_int16_t *)cts->i_dur = htole16(dur);
	IEEE80211_ADDR_COPY(cts->i_ra, ic->ic_myaddr);

	return m;
}

/*-
 * Beacon frame format:
 * [8]    Timestamp
 * [2]    Beacon interval
 * [2]    Capability
 * [tlv]  Service Set Identifier (SSID)
 * [tlv]  Supported rates
 * [tlv*] Frequency-Hopping (FH) Parameter Set
 * [tlv*] DS Parameter Set (802.11g)
 * [tlv*] IBSS Parameter Set
 * [tlv]  Traffic Indication Map (TIM)
 * [tlv]  ERP Information (802.11g)
 * [tlv]  Extended Supported Rates (802.11g)
 * [tlv]  RSN (802.11i)
 * [tlv]  EDCA Parameter Set (802.11e)
 */
struct mbuf *
ieee80211_beacon_alloc(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	const struct ieee80211_rateset *rs = &ni->ni_rates;
	struct ieee80211_frame *wh;
	struct mbuf *m;
	u_int8_t *frm;

	m = ieee80211_getmbuf(M_DONTWAIT, MT_DATA,
	    8 +	2 + 2 +
	    2 + ((ic->ic_flags & IEEE80211_F_HIDENWID) ? 0 : ni->ni_esslen) +
	    2 + min(rs->rs_nrates, IEEE80211_RATE_SIZE) +
	    2 + ((ic->ic_phytype == IEEE80211_T_FH) ? 5 : 1) +
	    2 + ((ic->ic_opmode == IEEE80211_M_IBSS) ? 2 : 254) +
	    ((ic->ic_curmode == IEEE80211_MODE_11G) ? 2 + 1 : 0) +
	    ((rs->rs_nrates > IEEE80211_RATE_SIZE) ?
		2 + rs->rs_nrates - IEEE80211_RATE_SIZE : 0) +
	    ((ic->ic_flags & IEEE80211_F_RSN) ? 2 + 44 : 0) +
	    ((ic->ic_flags & IEEE80211_F_QOS) ? 2 + 18 : 0) +
	    ((ic->ic_flags & IEEE80211_F_WPA1) ? 2 + 48 : 0));
	if (m == NULL)
		return NULL;

	wh = mtod(m, struct ieee80211_frame *);
	wh->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_MGT |
	    IEEE80211_FC0_SUBTYPE_BEACON;
	wh->i_fc[1] = IEEE80211_FC1_DIR_NODS;
	*(u_int16_t *)wh->i_dur = 0;
	IEEE80211_ADDR_COPY(wh->i_addr1, etherbroadcastaddr);
	IEEE80211_ADDR_COPY(wh->i_addr2, ic->ic_myaddr);
	IEEE80211_ADDR_COPY(wh->i_addr3, ni->ni_bssid);
	*(u_int16_t *)wh->i_seq = 0;

	frm = (u_int8_t *)&wh[1];
	memset(frm, 0, 8); frm += 8;	/* timestamp is set by hardware */
	LE_WRITE_2(frm, ni->ni_intval); frm += 2;
	frm = ieee80211_add_capinfo(frm, ic, ni);
	if (ic->ic_flags & IEEE80211_F_HIDENWID)
		frm = ieee80211_add_ssid(frm, NULL, 0);
	else
		frm = ieee80211_add_ssid(frm, ni->ni_essid, ni->ni_esslen);
	frm = ieee80211_add_rates(frm, rs);
	if (ic->ic_phytype == IEEE80211_T_FH)
		frm = ieee80211_add_fh_params(frm, ic, ni);
	else
		frm = ieee80211_add_ds_params(frm, ic, ni);
	if (ic->ic_opmode == IEEE80211_M_IBSS)
		frm = ieee80211_add_ibss_params(frm, ni);
	else
		frm = ieee80211_add_tim(frm, ic);
	if (ic->ic_curmode == IEEE80211_MODE_11G)
		frm = ieee80211_add_erp(frm, ic);
	if (rs->rs_nrates > IEEE80211_RATE_SIZE)
		frm = ieee80211_add_xrates(frm, rs);
	if (ic->ic_flags & IEEE80211_F_RSN)
		frm = ieee80211_add_rsn(frm, ic, ni);
	if (ic->ic_flags & IEEE80211_F_QOS)
		frm = ieee80211_add_edca_params(frm, ic);
	if (ic->ic_flags & IEEE80211_F_WPA1)
		frm = ieee80211_add_wpa1(frm, ic, ni);

	m->m_pkthdr.len = m->m_len = frm - mtod(m, u_int8_t *);
	m->m_pkthdr.rcvif = (void *)ni;

	return m;
}

/* unaligned big endian access */
#define BE_READ_2(p)				\
	((u_int16_t)(p)[0] << 8 | (u_int16_t)(p)[1])

#define BE_WRITE_2(p, v) do {			\
	(p)[0] = (v) >>  8; (p)[1] = (v);	\
} while (0)

#define BE_WRITE_8(p, v) do {			\
	(p)[0] = (v) >> 56; (p)[1] = (v) >> 48;	\
	(p)[2] = (v) >> 40; (p)[3] = (v) >> 32;	\
	(p)[4] = (v) >> 24; (p)[5] = (v) >> 16;	\
	(p)[6] = (v) >>  8; (p)[7] = (v);	\
} while (0)

/* unaligned little endian access */
#define LE_WRITE_8(p, v) do {			\
	(p)[7] = (v) >> 56; (p)[6] = (v) >> 48;	\
	(p)[5] = (v) >> 40; (p)[4] = (v) >> 32;	\
	(p)[3] = (v) >> 24; (p)[2] = (v) >> 16;	\
	(p)[1] = (v) >>  8; (p)[0] = (v);	\
} while (0)

int
ieee80211_send_eapol_key(struct ieee80211com *ic, struct mbuf *m,
    struct ieee80211_node *ni)
{
	struct ifnet *ifp = &ic->ic_if;
	struct ether_header *eh;
	struct ieee80211_eapol_key *key;
	u_int16_t len, info;
	int s, error;

	M_PREPEND(m, sizeof(struct ether_header), M_DONTWAIT);
	if (m == NULL)
		return ENOMEM;
	eh = mtod(m, struct ether_header *);
	eh->ether_type = htons(ETHERTYPE_PAE);
	IEEE80211_ADDR_COPY(eh->ether_shost, ic->ic_myaddr);
	IEEE80211_ADDR_COPY(eh->ether_dhost, ni->ni_macaddr);

	key = (struct ieee80211_eapol_key *)&eh[1];
	key->version = EAPOL_VERSION;
	key->type = EAPOL_KEY;
	key->desc = ni->ni_eapol_desc;

	info = BE_READ_2(key->info);
	/* use V2 descriptor iff pairwise cipher is CCMP */
	info |= (ni->ni_pairwise_cipher != IEEE80211_CIPHER_CCMP) ?
	    EAPOL_KEY_DESC_V1 : EAPOL_KEY_DESC_V2;
	BE_WRITE_2(key->info, info);

	len = m->m_len - sizeof(struct ether_header);
	BE_WRITE_2(key->paylen, len - sizeof(*key));
	BE_WRITE_2(key->len, len - 4);

	if (info & EAPOL_KEY_ENCRYPTED)
		ieee80211_eapol_key_encrypt(ic, key, ni->ni_ptk.kek);

	if (info & EAPOL_KEY_KEYMIC)
		ieee80211_eapol_key_mic(key, ni->ni_ptk.kck);

	s = splnet();
	IFQ_ENQUEUE(&ifp->if_snd, m, NULL, error);
	if (error) {
		splx(s);
		return error;
	}
	ifp->if_obytes += m->m_pkthdr.len;
	if ((ifp->if_flags & IFF_OACTIVE) == 0)
		(*ifp->if_start)(ifp);
	splx(s);

	return 0;
}

/*
 * Add a GTK KDE to an EAPOL-Key frame (see Figure 144).
 */
u_int8_t *
ieee80211_add_gtk_kde(u_int8_t *frm, const struct ieee80211_key *k)
{
	KASSERT(k->k_flags & IEEE80211_KEY_GROUP);

	*frm++ = IEEE80211_ELEMID_VENDOR;
	*frm++ = 6 + k->k_len;
	memcpy(frm, IEEE80211_OUI, 3); frm += 3;
	*frm++ = IEEE80211_KDE_GTK;
	*frm = k->k_id & 3;
	if (k->k_flags & IEEE80211_KEY_TX)
		*frm |= 1 << 2;	/* set the Tx bit */
	frm++;
	*frm++ = 0;	/* reserved */
	memcpy(frm, k->k_key, k->k_len);
	return frm + k->k_len;
}

/*
 * Add a PMKID KDE to an EAPOL-Key frame (see Figure 146).
 */
u_int8_t *
ieee80211_add_pmkid_kde(u_int8_t *frm, const u_int8_t *pmkid)
{
	*frm++ = IEEE80211_ELEMID_VENDOR;
	*frm++ = 20;
	memcpy(frm, IEEE80211_OUI, 3); frm += 3;
	*frm++ = IEEE80211_KDE_PMKID;
	memcpy(frm, pmkid, IEEE80211_PMKID_LEN);
	return frm + IEEE80211_PMKID_LEN;
}

struct mbuf *
ieee80211_get_eapol_key(int flags, int type, u_int pktlen)
{
	struct mbuf *m;

	pktlen += sizeof(struct ether_header) +
	    sizeof(struct ieee80211_eapol_key);

	if (pktlen > MCLBYTES)
		panic("EAPOL-Key frame too large: %u", pktlen);
	MGETHDR(m, flags, type);
	if (m != NULL && pktlen > MHLEN) {
		MCLGET(m, flags);
		if (!(m->m_flags & M_EXT))
			m = m_free(m);
	}
	m->m_data += sizeof(struct ether_header);
	return m;
}

/*
 * 4-Way Handshake Message 1 is sent by the authenticator to the supplicant
 * (see 8.5.3.1).
 */
int
ieee80211_send_4way_msg1(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	struct ieee80211_eapol_key *key;
	struct mbuf *m;
	u_int16_t info, keylen;
	u_int8_t *pmkid;
	u_int8_t *frm;

	m = ieee80211_get_eapol_key(M_DONTWAIT, MT_DATA,
	    (ni->ni_eapol_desc == EAPOL_KEY_DESC_IEEE80211) ? 2 + 20 : 0);
	if (m == NULL)
		return ENOMEM;
	key = mtod(m, struct ieee80211_eapol_key *);
	memset(key, 0, sizeof(*key));

	info = EAPOL_KEY_PAIRWISE | EAPOL_KEY_KEYACK;
	BE_WRITE_2(key->info, info);

	/* generate a new nonce ANonce */
	get_random_bytes(ni->ni_nonce, EAPOL_KEY_NONCE_LEN);
	memcpy(key->nonce, ni->ni_nonce, EAPOL_KEY_NONCE_LEN);

	keylen = ieee80211_cipher_keylen(ni->ni_pairwise_cipher);
	BE_WRITE_2(key->keylen, keylen);

	frm = (u_int8_t *)&key[1];
	/* WPA1 does not have PMKID KDE */
	if (ni->ni_eapol_desc == EAPOL_KEY_DESC_IEEE80211) {
		/* XXX retrieve PMKID from the PMKSA cache */
		frm = ieee80211_add_pmkid_kde(frm, pmkid);
	}

	m->m_pkthdr.len = m->m_len = frm - (u_int8_t *)key;

	if (ic->ic_if.if_flags & IFF_DEBUG)
		printf("%s: sending msg %d/%d of the %s handshake to %s\n",
		    ic->ic_if.if_xname, 1, 4, "4-way",
		    ether_sprintf(ni->ni_macaddr));

	return ieee80211_send_eapol_key(ic, m, ni);
}

/*
 * 4-Way Handshake Message 2 is sent by the supplicant to the authenticator
 * (see 8.5.3.2).
 */
int
ieee80211_send_4way_msg2(struct ieee80211com *ic, struct ieee80211_node *ni,
    const u_int8_t *snonce)
{
	struct ieee80211_eapol_key *key;
	struct mbuf *m;
	u_int16_t info;
	u_int8_t *frm;

	m = ieee80211_get_eapol_key(M_DONTWAIT, MT_DATA,
	    2 + 48);
	if (m == NULL)
		return ENOMEM;
	key = mtod(m, struct ieee80211_eapol_key *);
	memset(key, 0, sizeof(*key));

	info = EAPOL_KEY_PAIRWISE | EAPOL_KEY_KEYMIC;
	BE_WRITE_2(key->info, info);

	/* copy key replay counter from authenticator */
	BE_WRITE_8(key->replaycnt, ni->ni_replaycnt);

	/* copy the supplicant's nonce (SNonce) */
	memcpy(key->nonce, snonce, EAPOL_KEY_NONCE_LEN);

	frm = (u_int8_t *)&key[1];
	/* add the WPA/RSN IE used in the (Re)Association Request */
	if (ni->ni_eapol_desc == EAPOL_KEY_DESC_WPA1) {
		u_int16_t keylen;
		frm = ieee80211_add_wpa1(frm, ic, ni);
		/* WPA1 sets the key length field here */
		keylen = ieee80211_cipher_keylen(ni->ni_pairwise_cipher);
		BE_WRITE_2(key->keylen, keylen);
	} else	/* RSN */
		frm = ieee80211_add_rsn(frm, ic, ni);

	m->m_pkthdr.len = m->m_len = frm - (u_int8_t *)key;

	if (ic->ic_if.if_flags & IFF_DEBUG)
		printf("%s: sending msg %d/%d of the %s handshake to %s\n",
		    ic->ic_if.if_xname, 2, 4, "4-way",
		    ether_sprintf(ni->ni_macaddr));

	return ieee80211_send_eapol_key(ic, m, ni);
}

/*
 * 4-Way Handshake Message 3 is sent by the authenticator to the supplicant
 * (see 8.5.3.3).
 */
int
ieee80211_send_4way_msg3(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	struct ieee80211_eapol_key *key;
	struct ieee80211_key *gtk;
	struct mbuf *m;
	u_int16_t info, keylen;
	u_int8_t *frm;

	m = ieee80211_get_eapol_key(M_DONTWAIT, MT_DATA,
	    2 + 48 +
	    ((ni->ni_eapol_desc == EAPOL_KEY_DESC_IEEE80211) ?
		2 + 6 + gtk->k_len : 0) +
	    8);
	if (m == NULL)
		return ENOMEM;
	key = mtod(m, struct ieee80211_eapol_key *);
	memset(key, 0, sizeof(*key));

	info = EAPOL_KEY_PAIRWISE | EAPOL_KEY_INSTALL | EAPOL_KEY_KEYACK |
	    EAPOL_KEY_KEYMIC;

	BE_WRITE_8(key->replaycnt, ni->ni_replaycnt);
	/* use same nonce as in Message 1 */
	memcpy(key->nonce, ni->ni_nonce, EAPOL_KEY_NONCE_LEN);

	keylen = ieee80211_cipher_keylen(ni->ni_pairwise_cipher);
	BE_WRITE_2(key->keylen, keylen);

	frm = (u_int8_t *)&key[1];
	/* add the WPA/RSN IE included in Beacon/Probe Response */
	if (ni->ni_eapol_desc == EAPOL_KEY_DESC_IEEE80211) {
		frm = ieee80211_add_rsn(frm, ic, ic->ic_bss);
		/* RSN: encapsulate the GTK and ask for encryption */
		frm = ieee80211_add_gtk_kde(frm, gtk);
		LE_WRITE_8(key->rsc, gtk->k_rsc);
		info |= EAPOL_KEY_ENCRYPTED | EAPOL_KEY_SECURE;
	} else	/* WPA1 */
		frm = ieee80211_add_wpa1(frm, ic, ic->ic_bss);

	/* write the key info field */
	BE_WRITE_2(key->info, info);

	m->m_pkthdr.len = m->m_len = frm - (u_int8_t *)key;

	if (ic->ic_if.if_flags & IFF_DEBUG)
		printf("%s: sending msg %d/%d of the %s handshake to %s\n",
		    ic->ic_if.if_xname, 3, 4, "4-way",
		    ether_sprintf(ni->ni_macaddr));

	return ieee80211_send_eapol_key(ic, m, ni);
}

/*
 * 4-Way Handshake Message 4 is sent by the supplicant to the authenticator
 * (see 8.5.3.4).
 */
int
ieee80211_send_4way_msg4(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	struct ieee80211_eapol_key *key;
	struct mbuf *m;
	u_int16_t info;

	m = ieee80211_get_eapol_key(M_DONTWAIT, MT_DATA, 0);
	if (m == NULL)
		return ENOMEM;
	key = mtod(m, struct ieee80211_eapol_key *);
	memset(key, 0, sizeof(*key));

	info = EAPOL_KEY_PAIRWISE | EAPOL_KEY_KEYMIC;

	/* copy key replay counter from authenticator */
	BE_WRITE_8(key->replaycnt, ni->ni_replaycnt);

	if (ni->ni_eapol_desc == EAPOL_KEY_DESC_WPA1) {
		u_int16_t keylen;
		/* WPA1 sets the key length field here */
		keylen = ieee80211_cipher_keylen(ni->ni_pairwise_cipher);
		BE_WRITE_2(key->keylen, keylen);
	} else
		info |= EAPOL_KEY_SECURE;

	/* write the key info field */
	BE_WRITE_2(key->info, info);

	/* empty key data field */
	m->m_pkthdr.len = m->m_len = sizeof(*key);

	if (ic->ic_if.if_flags & IFF_DEBUG)
		printf("%s: sending msg %d/%d of the %s handshake to %s\n",
		    ic->ic_if.if_xname, 4, 4, "4-way",
		    ether_sprintf(ni->ni_macaddr));

	return ieee80211_send_eapol_key(ic, m, ni);
}

/*
 * Group Key Handshake Message 1 is sent by the authenticator to the
 * supplicant (see 8.5.4.1).
 */
int
ieee80211_send_group_msg1(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	struct ieee80211_eapol_key *key;
	struct ieee80211_key *gtk;
	struct mbuf *m;
	u_int16_t info;
	u_int8_t *frm;

	m = ieee80211_get_eapol_key(M_DONTWAIT, MT_DATA,
	    ((ni->ni_eapol_desc == EAPOL_KEY_DESC_WPA1) ?
		gtk->k_len : 2 + 6 + gtk->k_len) +
	    8);
	if (m == NULL)
		return ENOMEM;
	key = mtod(m, struct ieee80211_eapol_key *);
	memset(key, 0, sizeof(*key));

	info = EAPOL_KEY_KEYACK | EAPOL_KEY_KEYMIC | EAPOL_KEY_SECURE |
	    EAPOL_KEY_ENCRYPTED;

	BE_WRITE_8(key->replaycnt, ni->ni_replaycnt);
#if 0
	/* use global counter as GNonce */
	ieee80211_derive_gtk(ic->ic_gmk, IEEE80211_PMK_LEN, ic->ic_myaddr,
	    ic->ic_globalcnt, &gtk, sizeof gtk);
	/* XXX increment global counter */
#endif
	frm = (u_int8_t *)&key[1];
	if (ni->ni_eapol_desc == EAPOL_KEY_DESC_WPA1) {
		/* WPA1 does not have GTK KDE */
		BE_WRITE_2(key->keylen, gtk->k_len);
		memcpy(frm, gtk->k_key, gtk->k_len);
		frm += gtk->k_len;
		info |= gtk->k_id << EAPOL_KEY_WPA_KID_SHIFT;
		if (gtk->k_flags & IEEE80211_KEY_TX)
			info |= EAPOL_KEY_WPA_TX;
	} else	/* RSN */
		frm = ieee80211_add_gtk_kde(frm, gtk);

	LE_WRITE_8(key->rsc, gtk->k_rsc);

	/* write the key info field */
	BE_WRITE_2(key->info, info);

	m->m_pkthdr.len = m->m_len = frm - (u_int8_t *)key;

	if (ic->ic_if.if_flags & IFF_DEBUG)
		printf("%s: sending msg %d/%d of the %s handshake to %s\n",
		    ic->ic_if.if_xname, 1, 2, "group key",
		    ether_sprintf(ni->ni_macaddr));

	return ieee80211_send_eapol_key(ic, m, ni);
}

/*
 * Group Key Handshake Message 2 is sent by the supplicant to the
 * authenticator (see 8.5.4.2).
 */
int
ieee80211_send_group_msg2(struct ieee80211com *ic, struct ieee80211_node *ni,
    const struct ieee80211_key *gtk)
{
	struct ieee80211_eapol_key *key;
	u_int16_t info;
	struct mbuf *m;

	m = ieee80211_get_eapol_key(M_DONTWAIT, MT_DATA, 0);
	if (m == NULL)
		return ENOMEM;
	key = mtod(m, struct ieee80211_eapol_key *);
	memset(key, 0, sizeof(*key));

	info = EAPOL_KEY_KEYMIC | EAPOL_KEY_SECURE;

	/* copy key replay counter from authenticator */
	BE_WRITE_8(key->replaycnt, ni->ni_replaycnt);

	if (ni->ni_eapol_desc == EAPOL_KEY_DESC_WPA1) {
		/* WPA1 sets the key length and key id fields here */
		BE_WRITE_2(key->keylen, gtk->k_len);
		info |= (gtk->k_id & 3) << EAPOL_KEY_WPA_KID_SHIFT;
	}

	/* write the key info field */
	BE_WRITE_2(key->info, info);

	/* empty key data field */
	m->m_pkthdr.len = m->m_len = sizeof(*key);

	if (ic->ic_if.if_flags & IFF_DEBUG)
		printf("%s: sending msg %d/%d of the %s handshake to %s\n",
		    ic->ic_if.if_xname, 2, 2, "group key",
		    ether_sprintf(ni->ni_macaddr));

	return ieee80211_send_eapol_key(ic, m, ni);
}

/*
 * EAPOL-Key Request frames are sent by the supplicant to request that the
 * authenticator initiate either a 4-Way Handshake or Group Key Handshake
 * and to report a MIC failure in a TKIP MSDU.
 */
int
ieee80211_send_eapol_key_req(struct ieee80211com *ic,
    struct ieee80211_node *ni, u_int16_t info, u_int64_t tsc)
{
	struct ieee80211_eapol_key *key;
	struct mbuf *m;

	m = ieee80211_get_eapol_key(M_DONTWAIT, MT_DATA, 0);
	if (m == NULL)
		return ENOMEM;
	key = mtod(m, struct ieee80211_eapol_key *);
	memset(key, 0, sizeof(*key));

	BE_WRITE_2(key->info, info);

	/* in case of TKIP MIC failure, fill the RSC field */
	if (info & EAPOL_KEY_ERROR)
		LE_WRITE_8(key->rsc, tsc);

	/* use our separate key replay counter for key requests */
	BE_WRITE_8(key->replaycnt, ic->ic_keyreplaycnt);
	ic->ic_keyreplaycnt++;

	if (ic->ic_if.if_flags & IFF_DEBUG)
		printf("%s: sending EAPOL-Key request to %s\n",
		    ic->ic_if.if_xname, ether_sprintf(ni->ni_macaddr));

	return ieee80211_send_eapol_key(ic, m, ni);
}

void
ieee80211_pwrsave(struct ieee80211com *ic, struct ieee80211_node *ni,
    struct mbuf *m)
{
	/* store the new packet on our queue, changing the TIM if necessary */
	if (IF_IS_EMPTY(&ni->ni_savedq))
		(*ic->ic_set_tim)(ic, ni->ni_associd, 1);

	if (ni->ni_savedq.ifq_len >= IEEE80211_PS_MAX_QUEUE) {
		IF_DROP(&ni->ni_savedq);
		m_freem(m);
		if (ic->ic_if.if_flags & IFF_DEBUG)
			printf("%s: station %s power save queue overflow"
			    " of size %d drops %d\n",
			    ic->ic_if.if_xname,
			    ether_sprintf(ni->ni_macaddr),
			    IEEE80211_PS_MAX_QUEUE,
			    ni->ni_savedq.ifq_drops);
	} else {
		/*
		 * Similar to ieee80211_mgmt_output, store the node in
		 * the rcvif field.
		 */
		IF_ENQUEUE(&ni->ni_savedq, m);
		m->m_pkthdr.rcvif = (void *)ni;
	}
}
