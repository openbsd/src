/*	$NetBSD: ieee80211_input.c,v 1.24 2004/05/31 11:12:24 dyoung Exp $	*/
/*	$OpenBSD: ieee80211_input.c,v 1.79 2008/04/26 19:57:49 damien Exp $	*/

/*-
 * Copyright (c) 2001 Atsushi Onoe
 * Copyright (c) 2002, 2003 Sam Leffler, Errno Consulting
 * Copyright (c) 2007, 2008 Damien Bergamini
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/endian.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/endian.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_arp.h>
#include <net/if_llc.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#include <net80211/ieee80211_var.h>

#include <dev/rndvar.h>

int	ieee80211_parse_edca_params_body(struct ieee80211com *,
	    const u_int8_t *);
int	ieee80211_parse_edca_params(struct ieee80211com *, const u_int8_t *);
int	ieee80211_parse_wmm_params(struct ieee80211com *, const u_int8_t *);
enum	ieee80211_cipher ieee80211_parse_rsn_cipher(const u_int8_t[]);
enum	ieee80211_akm ieee80211_parse_rsn_akm(const u_int8_t[]);
int	ieee80211_parse_rsn_body(struct ieee80211com *, const u_int8_t *,
	    u_int, struct ieee80211_rsnparams *);
int	ieee80211_parse_rsn(struct ieee80211com *, const u_int8_t *,
	    struct ieee80211_rsnparams *);
int	ieee80211_parse_wpa(struct ieee80211com *, const u_int8_t *,
	    struct ieee80211_rsnparams *);
int	ieee80211_save_ie(const u_int8_t *, u_int8_t **);
void	ieee80211_recv_pspoll(struct ieee80211com *, struct mbuf *);
void	ieee80211_recv_probe_resp(struct ieee80211com *, struct mbuf *,
	    struct ieee80211_node *, int, u_int32_t, int);
void	ieee80211_recv_probe_req(struct ieee80211com *, struct mbuf *,
	    struct ieee80211_node *, int, u_int32_t);
void	ieee80211_recv_auth(struct ieee80211com *, struct mbuf *,
	    struct ieee80211_node *, int, u_int32_t);
void	ieee80211_recv_assoc_req(struct ieee80211com *, struct mbuf *,
	    struct ieee80211_node *, int, u_int32_t, int);
void	ieee80211_recv_assoc_resp(struct ieee80211com *, struct mbuf *,
	    struct ieee80211_node *, int);
void	ieee80211_recv_deauth(struct ieee80211com *, struct mbuf *,
	    struct ieee80211_node *);
void	ieee80211_recv_disassoc(struct ieee80211com *, struct mbuf *,
	    struct ieee80211_node *);
void	ieee80211_recv_action(struct ieee80211com *, struct mbuf *,
	    struct ieee80211_node *);
void	ieee80211_recv_4way_msg1(struct ieee80211com *,
	    struct ieee80211_eapol_key *, struct ieee80211_node *);
void	ieee80211_recv_4way_msg2(struct ieee80211com *,
	    struct ieee80211_eapol_key *, struct ieee80211_node *,
	    const u_int8_t *);
void	ieee80211_recv_4way_msg3(struct ieee80211com *,
	    struct ieee80211_eapol_key *, struct ieee80211_node *);
void	ieee80211_recv_4way_msg4(struct ieee80211com *,
	    struct ieee80211_eapol_key *, struct ieee80211_node *);
void	ieee80211_recv_4way_msg2or4(struct ieee80211com *,
	    struct ieee80211_eapol_key *, struct ieee80211_node *);
void	ieee80211_recv_rsn_group_msg1(struct ieee80211com *,
	    struct ieee80211_eapol_key *, struct ieee80211_node *);
void	ieee80211_recv_wpa_group_msg1(struct ieee80211com *,
	    struct ieee80211_eapol_key *, struct ieee80211_node *);
void	ieee80211_recv_group_msg2(struct ieee80211com *,
	    struct ieee80211_eapol_key *, struct ieee80211_node *);
void	ieee80211_recv_eapol_key_req(struct ieee80211com *,
	    struct ieee80211_eapol_key *, struct ieee80211_node *);

/*
 * Retrieve the length in bytes of a 802.11 header.
 */
u_int
ieee80211_get_hdrlen(const void *data)
{
	const u_int8_t *fc = data;
	u_int size = sizeof(struct ieee80211_frame);

	/* NB: doesn't work with control frames */
	KASSERT((fc[0] & IEEE80211_FC0_TYPE_MASK) != IEEE80211_FC0_TYPE_CTL);

	if ((fc[1] & IEEE80211_FC1_DIR_MASK) == IEEE80211_FC1_DIR_DSTODS)
		size += IEEE80211_ADDR_LEN;		/* i_addr4 */
	if ((fc[0] & (IEEE80211_FC0_TYPE_MASK | IEEE80211_FC0_SUBTYPE_QOS)) ==
	    (IEEE80211_FC0_TYPE_DATA | IEEE80211_FC0_SUBTYPE_QOS)) {
		size += sizeof(u_int16_t);		/* i_qos */
		if (fc[1] & IEEE80211_FC1_ORDER)
			size += sizeof(u_int32_t);	/* i_ht */
	} else if ((fc[0] & IEEE80211_FC0_TYPE_MASK) ==
	     IEEE80211_FC0_TYPE_MGT && (fc[1] & IEEE80211_FC1_ORDER))
		size += sizeof(u_int32_t);		/* i_ht */
	return size;
}

/*
 * Process a received frame.  The node associated with the sender
 * should be supplied.  If nothing was found in the node table then
 * the caller is assumed to supply a reference to ic_bss instead.
 * The RSSI and a timestamp are also supplied.  The RSSI data is used
 * during AP scanning to select a AP to associate with; it can have
 * any units so long as values have consistent units and higher values
 * mean ``better signal''.  The receive timestamp is currently not used
 * by the 802.11 layer.
 */
void
ieee80211_input(struct ifnet *ifp, struct mbuf *m, struct ieee80211_node *ni,
    int rssi, u_int32_t rstamp)
{
	struct ieee80211com *ic = (void *)ifp;
	struct ieee80211_frame *wh;
	struct ether_header *eh;
	struct mbuf *m1;
	int error, hdrlen, len;
	u_int8_t dir, type, subtype;
	u_int16_t orxseq, nrxseq;

	if (ni == NULL)
		panic("null node");

	/*
	 * In monitor mode, send everything directly to bpf.
	 * XXX may want to include the CRC
	 */
	if (ic->ic_opmode == IEEE80211_M_MONITOR)
		goto out;

	/* do not process frames w/o i_addr2 any further */
	if (m->m_pkthdr.len < sizeof(struct ieee80211_frame_min)) {
		IEEE80211_DPRINTF2(("%s: frame too short (1), len %u\n",
		    __func__, m->m_pkthdr.len));
		ic->ic_stats.is_rx_tooshort++;
		goto out;
	}

	wh = mtod(m, struct ieee80211_frame *);
	if ((wh->i_fc[0] & IEEE80211_FC0_VERSION_MASK) !=
	    IEEE80211_FC0_VERSION_0) {
		IEEE80211_DPRINTF(("%s: packet with wrong version: %x\n",
		    __func__, wh->i_fc[0]));
		ic->ic_stats.is_rx_badversion++;
		goto err;
	}

	dir = wh->i_fc[1] & IEEE80211_FC1_DIR_MASK;
	type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;

	/*
	 * NB: We are not yet prepared to handle control frames,
	 *     but permitting drivers to send them to us allows
	 *     them to go through bpf tapping at the 802.11 layer.
	 */
	if (m->m_pkthdr.len < sizeof(struct ieee80211_frame)) {
		IEEE80211_DPRINTF2(("%s: frame too short (2), len %u\n",
		    __func__, m->m_pkthdr.len));
		ic->ic_stats.is_rx_tooshort++;
		goto out;
	}
	if (ic->ic_state != IEEE80211_S_SCAN) {
		ni->ni_rssi = rssi;
		ni->ni_rstamp = rstamp;
		if (type == IEEE80211_FC0_TYPE_DATA &&
		    (wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_QOS)) {
			struct ieee80211_qosframe *qwh =
			    (struct ieee80211_qosframe *)wh;
			int tid = qwh->i_qos[0] & IEEE80211_QOS_TID;
			orxseq = ni->ni_qos_rxseqs[tid];
			nrxseq = ni->ni_qos_rxseqs[tid] =
			    letoh16(*(u_int16_t *)qwh->i_seq) >>
				IEEE80211_SEQ_SEQ_SHIFT;
		} else {
			orxseq = ni->ni_rxseq;
			nrxseq = ni->ni_rxseq =
			    letoh16(*(u_int16_t *)wh->i_seq) >>
				IEEE80211_SEQ_SEQ_SHIFT;
		}
		/* TODO: fragment */
		if ((wh->i_fc[1] & IEEE80211_FC1_RETRY) &&
		    orxseq == nrxseq) {
			/* duplicate, silently discarded */
			ic->ic_stats.is_rx_dup++; /* XXX per-station stat */
			goto out;
		}
		ni->ni_inact = 0;
	}

	if ((ic->ic_opmode == IEEE80211_M_HOSTAP ||
	     ic->ic_opmode == IEEE80211_M_IBSS) && ic->ic_set_tim != NULL) {
		if ((wh->i_fc[1] & IEEE80211_FC1_PWR_MGT) &&
		    ni->ni_pwrsave == 0) {
			/* turn on power save mode */

			if (ifp->if_flags & IFF_DEBUG)
				printf("%s: power save mode on for %s\n",
				    ifp->if_xname, ether_sprintf(wh->i_addr2));

			ni->ni_pwrsave = IEEE80211_PS_SLEEP;
		}
		if (!(wh->i_fc[1] & IEEE80211_FC1_PWR_MGT) &&
		    ni->ni_pwrsave != 0) {
			/* turn off power save mode, dequeue stored packets */

			ni->ni_pwrsave = 0;
			(*ic->ic_set_tim)(ic, ni->ni_associd, 0);

			if (ifp->if_flags & IFF_DEBUG)
				printf("%s: power save mode off for %s\n",
				    ifp->if_xname, ether_sprintf(wh->i_addr2));

			while (!IF_IS_EMPTY(&ni->ni_savedq)) {
				struct mbuf *m;
				IF_DEQUEUE(&ni->ni_savedq, m);
				IF_ENQUEUE(&ic->ic_pwrsaveq, m);
				(*ifp->if_start)(ifp);
			}
		}
	}

	switch (type) {
	case IEEE80211_FC0_TYPE_DATA:
		switch (ic->ic_opmode) {
		case IEEE80211_M_STA:
			if (dir != IEEE80211_FC1_DIR_FROMDS) {
				ic->ic_stats.is_rx_wrongdir++;
				goto out;
			}
			if (ic->ic_state != IEEE80211_S_SCAN &&
			    !IEEE80211_ADDR_EQ(wh->i_addr2, ni->ni_bssid)) {
				/* Source address is not our BSS. */
				IEEE80211_DPRINTF(
				    ("%s: discard frame from SA %s\n",
				    __func__, ether_sprintf(wh->i_addr2)));
				ic->ic_stats.is_rx_wrongbss++;
				goto out;
			}
			if ((ifp->if_flags & IFF_SIMPLEX) &&
			    IEEE80211_IS_MULTICAST(wh->i_addr1) &&
			    IEEE80211_ADDR_EQ(wh->i_addr3, ic->ic_myaddr)) {
				/*
				 * In IEEE802.11 network, multicast packet
				 * sent from me is broadcasted from AP.
				 * It should be silently discarded for
				 * SIMPLEX interface.
				 */
				ic->ic_stats.is_rx_mcastecho++;
				goto out;
			}
			break;
		case IEEE80211_M_IBSS:
		case IEEE80211_M_AHDEMO:
			if (dir != IEEE80211_FC1_DIR_NODS) {
				ic->ic_stats.is_rx_wrongdir++;
				goto out;
			}
			if (ic->ic_state != IEEE80211_S_SCAN &&
			    !IEEE80211_ADDR_EQ(wh->i_addr3,
				ic->ic_bss->ni_bssid) &&
			    !IEEE80211_ADDR_EQ(wh->i_addr3,
				etherbroadcastaddr)) {
				/* Destination is not our BSS or broadcast. */
				IEEE80211_DPRINTF2(
				    ("%s: discard data frame to DA %s\n",
				    __func__, ether_sprintf(wh->i_addr3)));
				ic->ic_stats.is_rx_wrongbss++;
				goto out;
			}
			break;
		case IEEE80211_M_HOSTAP:
			if (dir != IEEE80211_FC1_DIR_TODS) {
				ic->ic_stats.is_rx_wrongdir++;
				goto out;
			}
			if (ic->ic_state != IEEE80211_S_SCAN &&
			    !IEEE80211_ADDR_EQ(wh->i_addr1,
				ic->ic_bss->ni_bssid) &&
			    !IEEE80211_ADDR_EQ(wh->i_addr1,
				etherbroadcastaddr)) {
				/* BSS is not us or broadcast. */
				IEEE80211_DPRINTF2(
				    ("%s: discard data frame to BSS %s\n",
				    __func__, ether_sprintf(wh->i_addr1)));
				ic->ic_stats.is_rx_wrongbss++;
				goto out;
			}
			/* check if source STA is associated */
			if (ni == ic->ic_bss) {
				IEEE80211_DPRINTF(("%s: "
				    "data from unknown src %s\n", __func__,
				    ether_sprintf(wh->i_addr2)));
				/* NB: caller deals with reference */
				ni = ieee80211_dup_bss(ic, wh->i_addr2);
				if (ni != NULL) {
					IEEE80211_SEND_MGMT(ic, ni,
					    IEEE80211_FC0_SUBTYPE_DEAUTH,
					    IEEE80211_REASON_NOT_AUTHED);
				}
				ic->ic_stats.is_rx_notassoc++;
				goto err;
			}
			if (ni->ni_associd == 0) {
				IEEE80211_DPRINTF(("%s: "
				    "data from unassoc src %s\n", __func__,
				    ether_sprintf(wh->i_addr2)));
				IEEE80211_SEND_MGMT(ic, ni,
				    IEEE80211_FC0_SUBTYPE_DISASSOC,
				    IEEE80211_REASON_NOT_ASSOCED);
				ic->ic_stats.is_rx_notassoc++;
				goto err;
			}
			break;
		case IEEE80211_M_MONITOR:
			/* can't get there */
			goto out;
		}

		hdrlen = ieee80211_get_hdrlen(wh);

		if (wh->i_fc[1] & IEEE80211_FC1_PROTECTED) {
			if (ic->ic_flags &
			    (IEEE80211_F_WEPON | IEEE80211_F_RSNON)) {
				m = ieee80211_decrypt(ic, m, ni);
				if (m == NULL) {
					ic->ic_stats.is_rx_wepfail++;
					goto err;
				}
				wh = mtod(m, struct ieee80211_frame *);
			} else {
				ic->ic_stats.is_rx_nowep++;
				goto out;
			}
		}
		/*
		 * XXX else: drivers should pass a flag to indicate if the
		 * frame was successfully decrypted or not.
		 */
#if NBPFILTER > 0
		/* copy to listener after decrypt */
		if (ic->ic_rawbpf)
			bpf_mtap(ic->ic_rawbpf, m, BPF_DIRECTION_IN);
#endif
		m = ieee80211_decap(ifp, m, hdrlen);
		if (m == NULL) {
			IEEE80211_DPRINTF(("%s: "
			    "decapsulation error for src %s\n",
			    __func__, ether_sprintf(wh->i_addr2)));
			ic->ic_stats.is_rx_decap++;
			goto err;
		}
		eh = mtod(m, struct ether_header *);

		if ((ic->ic_flags & IEEE80211_F_RSNON) && !ni->ni_port_valid &&
		    eh->ether_type != htons(ETHERTYPE_PAE)) {
			IEEE80211_DPRINTF(("%s: port not valid: %s\n",
			    __func__, ether_sprintf(wh->i_addr2)));
			ic->ic_stats.is_rx_unauth++;
			goto err;
		}
		ifp->if_ipackets++;

		/*
		 * Perform as a bridge within the AP.  XXX we do not bridge
		 * 802.1X frames as suggested in C.1.1 of IEEE Std 802.1X.
		 */
		m1 = NULL;
		if (ic->ic_opmode == IEEE80211_M_HOSTAP &&
		    !(ic->ic_flags & IEEE80211_F_NOBRIDGE) &&
		    eh->ether_type != htons(ETHERTYPE_PAE)) {
			if (ETHER_IS_MULTICAST(eh->ether_dhost)) {
				m1 = m_copym(m, 0, M_COPYALL, M_DONTWAIT);
				if (m1 == NULL)
					ifp->if_oerrors++;
				else
					m1->m_flags |= M_MCAST;
			} else {
				struct ieee80211_node *ni;
				ni = ieee80211_find_node(ic, eh->ether_dhost);
				if (ni != NULL) {
					if (ni->ni_associd != 0) {
						m1 = m;
						m = NULL;
					}
				}
			}
			if (m1 != NULL) {
				len = m1->m_pkthdr.len;
				IFQ_ENQUEUE(&ifp->if_snd, m1, NULL, error);
				if (error)
					ifp->if_oerrors++;
				else {
					if (m != NULL)
						ifp->if_omcasts++;
					ifp->if_obytes += len;
				}
			}
		}
		if (m != NULL) {
#if NBPFILTER > 0
			/*
			 * If we forward packet into transmitter of the AP,
			 * we don't need to duplicate for DLT_EN10MB.
			 */
			if (ifp->if_bpf && m1 == NULL)
				bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_IN);
#endif
			if ((ic->ic_flags & IEEE80211_F_RSNON) &&
			    eh->ether_type == htons(ETHERTYPE_PAE)) {
				(*ic->ic_recv_eapol)(ic, m, ni);
				m_freem(m);
			} else
				ether_input_mbuf(ifp, m);
		}
		return;

	case IEEE80211_FC0_TYPE_MGT:
		if (dir != IEEE80211_FC1_DIR_NODS) {
			ic->ic_stats.is_rx_wrongdir++;
			goto err;
		}
		if (ic->ic_opmode == IEEE80211_M_AHDEMO) {
			ic->ic_stats.is_rx_ahdemo_mgt++;
			goto out;
		}
		subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;

		/* drop frames without interest */
		if (ic->ic_state == IEEE80211_S_SCAN) {
			if (subtype != IEEE80211_FC0_SUBTYPE_BEACON &&
			    subtype != IEEE80211_FC0_SUBTYPE_PROBE_RESP) {
				ic->ic_stats.is_rx_mgtdiscard++;
				goto out;
			}
		}

		if (ifp->if_flags & IFF_DEBUG) {
			/* avoid to print too many frames */
			int doprint = 0;

			switch (subtype) {
			case IEEE80211_FC0_SUBTYPE_BEACON:
				if (ic->ic_state == IEEE80211_S_SCAN)
					doprint = 1;
				break;
			case IEEE80211_FC0_SUBTYPE_PROBE_REQ:
				if (ic->ic_opmode == IEEE80211_M_IBSS)
					doprint = 1;
				break;
			default:
				doprint = 1;
				break;
			}
#ifdef IEEE80211_DEBUG
			doprint += ieee80211_debug;
#endif
			if (doprint)
				printf("%s: received %s from %s rssi %d mode %s\n",
				    ifp->if_xname,
				    ieee80211_mgt_subtype_name[subtype
				    >> IEEE80211_FC0_SUBTYPE_SHIFT],
				    ether_sprintf(wh->i_addr2), rssi,
				    ieee80211_phymode_name[ieee80211_chan2mode(ic,
				    ic->ic_bss->ni_chan)]);
		}
#if NBPFILTER > 0
		if (ic->ic_rawbpf)
			bpf_mtap(ic->ic_rawbpf, m, BPF_DIRECTION_IN);
		/*
		 * Drop mbuf if it was filtered by bpf. Normally, this is
		 * done in ether_input() but IEEE 802.11 management frames
		 * are a special case.
		 */
		if (m->m_flags & M_FILDROP) {
			m_freem(m);
			return;
		}
#endif
		(*ic->ic_recv_mgmt)(ic, m, ni, subtype, rssi, rstamp);
		m_freem(m);
		return;

	case IEEE80211_FC0_TYPE_CTL:
		ic->ic_stats.is_rx_ctl++;
		if (ic->ic_opmode != IEEE80211_M_HOSTAP)
			goto out;
		subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;
		if (subtype == IEEE80211_FC0_SUBTYPE_PS_POLL) {
			/* XXX statistic */
			/* Dump out a single packet from the host */
			if (ifp->if_flags & IFF_DEBUG)
				printf("%s: got power save probe from %s\n",
				    ifp->if_xname,
				    ether_sprintf(wh->i_addr2));
			ieee80211_recv_pspoll(ic, m);
		}
		goto out;

	default:
		IEEE80211_DPRINTF(("%s: bad packet type %x\n", __func__, type));
		/* should not come here */
		break;
	}
 err:
	ifp->if_ierrors++;
 out:
	if (m != NULL) {
#if NBPFILTER > 0
		if (ic->ic_rawbpf)
			bpf_mtap(ic->ic_rawbpf, m, BPF_DIRECTION_IN);
#endif
		m_freem(m);
	}
}

struct mbuf *
ieee80211_decap(struct ifnet *ifp, struct mbuf *m, int hdrlen)
{
	struct ieee80211_frame wh;
	struct ether_header *eh;
	struct llc *llc;

	if (m->m_len < hdrlen + sizeof(*llc)) {
		m = m_pullup(m, hdrlen + sizeof(*llc));
		if (m == NULL)
			return NULL;
	}
	memcpy(&wh, mtod(m, caddr_t), sizeof(wh));
	llc = (struct llc *)(mtod(m, caddr_t) + hdrlen);
	if (llc->llc_dsap == LLC_SNAP_LSAP &&
	    llc->llc_ssap == LLC_SNAP_LSAP &&
	    llc->llc_control == LLC_UI &&
	    llc->llc_snap.org_code[0] == 0 &&
	    llc->llc_snap.org_code[1] == 0 &&
	    llc->llc_snap.org_code[2] == 0) {
		m_adj(m, hdrlen + sizeof(struct llc) - sizeof(*eh));
		llc = NULL;
	} else {
		m_adj(m, hdrlen - sizeof(*eh));
	}
	eh = mtod(m, struct ether_header *);
	switch (wh.i_fc[1] & IEEE80211_FC1_DIR_MASK) {
	case IEEE80211_FC1_DIR_NODS:
		IEEE80211_ADDR_COPY(eh->ether_dhost, wh.i_addr1);
		IEEE80211_ADDR_COPY(eh->ether_shost, wh.i_addr2);
		break;
	case IEEE80211_FC1_DIR_TODS:
		IEEE80211_ADDR_COPY(eh->ether_dhost, wh.i_addr3);
		IEEE80211_ADDR_COPY(eh->ether_shost, wh.i_addr2);
		break;
	case IEEE80211_FC1_DIR_FROMDS:
		IEEE80211_ADDR_COPY(eh->ether_dhost, wh.i_addr1);
		IEEE80211_ADDR_COPY(eh->ether_shost, wh.i_addr3);
		break;
	case IEEE80211_FC1_DIR_DSTODS:
		/* not yet supported */
		IEEE80211_DPRINTF(("%s: discard DS to DS frame\n", __func__));
		m_freem(m);
		return NULL;
	}
	if (!ALIGNED_POINTER(mtod(m, caddr_t) + sizeof(*eh), u_int32_t)) {
		struct mbuf *n, *n0, **np;
		caddr_t newdata;
		int off, pktlen;

		n0 = NULL;
		np = &n0;
		off = 0;
		pktlen = m->m_pkthdr.len;
		while (pktlen > off) {
			if (n0 == NULL) {
				MGETHDR(n, M_DONTWAIT, MT_DATA);
				if (n == NULL) {
					m_freem(m);
					return NULL;
				}
				M_DUP_PKTHDR(n, m);
				n->m_len = MHLEN;
			} else {
				MGET(n, M_DONTWAIT, MT_DATA);
				if (n == NULL) {
					m_freem(m);
					m_freem(n0);
					return NULL;
				}
				n->m_len = MLEN;
			}
			if (pktlen - off >= MINCLSIZE) {
				MCLGET(n, M_DONTWAIT);
				if (n->m_flags & M_EXT)
					n->m_len = n->m_ext.ext_size;
			}
			if (n0 == NULL) {
				newdata =
				    (caddr_t)ALIGN(n->m_data + sizeof(*eh)) -
				    sizeof(*eh);
				n->m_len -= newdata - n->m_data;
				n->m_data = newdata;
			}
			if (n->m_len > pktlen - off)
				n->m_len = pktlen - off;
			m_copydata(m, off, n->m_len, mtod(n, caddr_t));
			off += n->m_len;
			*np = n;
			np = &n->m_next;
		}
		m_freem(m);
		m = n0;
	}
	if (llc != NULL) {
		eh = mtod(m, struct ether_header *);
		eh->ether_type = htons(m->m_pkthdr.len - sizeof(*eh));
	}
	return m;
}

/* unaligned little endian access */
#define LE_READ_2(p)					\
	((u_int16_t)					\
	 ((((const u_int8_t *)(p))[0]) |		\
	  (((const u_int8_t *)(p))[1] <<  8)))
#define LE_READ_4(p)					\
	((u_int32_t)					\
	 ((((const u_int8_t *)(p))[0])       |		\
	  (((const u_int8_t *)(p))[1] <<  8) |		\
	  (((const u_int8_t *)(p))[2] << 16) |		\
	  (((const u_int8_t *)(p))[3] << 24)))

/*
 * Parse an EDCA Parameter Set element (see 7.3.2.27).
 */
int
ieee80211_parse_edca_params_body(struct ieee80211com *ic, const u_int8_t *frm)
{
	u_int updtcount;
	int aci;

	/*
	 * Check if EDCA parameters have changed XXX if we miss more than
	 * 15 consecutive beacons, we might not detect changes to EDCA
	 * parameters due to wraparound of the 4-bit Update Count field.
	 */
	updtcount = frm[0] & 0xf;
	if (updtcount == ic->ic_edca_updtcount)
		return 0;	/* no changes to EDCA parameters, ignore */
	ic->ic_edca_updtcount = updtcount;

	frm += 2;	/* skip QoS Info & Reserved fields */

	/* parse AC Parameter Records */
	for (aci = 0; aci < EDCA_NUM_AC; aci++) {
		struct ieee80211_edca_ac_params *ac = &ic->ic_edca_ac[aci];

		ac->ac_acm       = (frm[0] >> 4) & 0x1;
		ac->ac_aifsn     = frm[0] & 0xf;
		ac->ac_ecwmin    = frm[1] & 0xf;
		ac->ac_ecwmax    = frm[1] >> 4;
		ac->ac_txoplimit = LE_READ_2(frm + 2);
		frm += 4;
	}
	/* give drivers a chance to update their settings */
	if ((ic->ic_flags & IEEE80211_F_QOS) && ic->ic_updateedca != NULL)
		(*ic->ic_updateedca)(ic);

	return 0;
}

int
ieee80211_parse_edca_params(struct ieee80211com *ic, const u_int8_t *frm)
{
	/* check IE length */
	if (frm[1] < 18) {
		IEEE80211_DPRINTF(("%s: invalid EDCA parameter set IE;"
		    " length %u, expecting 18\n", __func__, frm[1]));
		ic->ic_stats.is_rx_elem_toosmall++;
		return IEEE80211_REASON_IE_INVALID;
	}
	return ieee80211_parse_edca_params_body(ic, frm + 2);
}

int
ieee80211_parse_wmm_params(struct ieee80211com *ic, const u_int8_t *frm)
{
	/* check IE length */
	if (frm[1] < 24) {
		IEEE80211_DPRINTF(("%s: invalid WMM parameter set IE;"
		    " length %u, expecting 24\n", __func__, frm[1]));
		ic->ic_stats.is_rx_elem_toosmall++;
		return IEEE80211_REASON_IE_INVALID;
	}
	return ieee80211_parse_edca_params_body(ic, frm + 8);
}

enum ieee80211_cipher
ieee80211_parse_rsn_cipher(const u_int8_t selector[4])
{
	/* from IEEE Std 802.11i-2004 - Table 20da */
	if (memcmp(selector, MICROSOFT_OUI, 3) == 0 ||	/* WPA */
	    memcmp(selector, IEEE80211_OUI, 3) == 0) {	/* RSN */
		switch (selector[3]) {
		case 0:	/* use group cipher suite */
			return IEEE80211_CIPHER_USEGROUP;
		case 1:	/* WEP-40 */
			return IEEE80211_CIPHER_WEP40;
		case 2:	/* TKIP */
			return IEEE80211_CIPHER_TKIP;
		case 4:	/* CCMP (RSNA default) */
			return IEEE80211_CIPHER_CCMP;
		case 5:	/* WEP-104 */
			return IEEE80211_CIPHER_WEP104;
		}
	}
	return IEEE80211_CIPHER_NONE;	/* ignore unknown ciphers */
}

enum ieee80211_akm
ieee80211_parse_rsn_akm(const u_int8_t selector[4])
{
	/* from IEEE Std 802.11i-2004 - Table 20dc */
	if (memcmp(selector, MICROSOFT_OUI, 3) == 0 ||	/* WPA */
	    memcmp(selector, IEEE80211_OUI, 3) == 0) {	/* RSN */
		switch (selector[3]) {
		case 1:	/* IEEE 802.1X (RSNA default) */
			return IEEE80211_AKM_IEEE8021X;
		case 2:	/* PSK */
			return IEEE80211_AKM_PSK;
		}
	}
	return IEEE80211_AKM_NONE;	/* ignore unknown AKMs */
}

/*
 * Parse an RSN element (see 7.3.2.25).
 */
int
ieee80211_parse_rsn_body(struct ieee80211com *ic, const u_int8_t *frm,
    u_int len, struct ieee80211_rsnparams *rsn)
{
	const u_int8_t *efrm;
	u_int16_t m, n, s;

	efrm = frm + len;

	/* check Version field */
	if (LE_READ_2(frm) != 1)
		return IEEE80211_STATUS_RSN_IE_VER_UNSUP;
	frm += 2;

	/* all fields after the Version field are optional */

	/* if Cipher Suite missing, default to CCMP */
	rsn->rsn_groupcipher = IEEE80211_CIPHER_CCMP;
	rsn->rsn_nciphers = 1;
	rsn->rsn_ciphers = IEEE80211_CIPHER_CCMP;
	/* if AKM Suite missing, default to 802.1X */
	rsn->rsn_nakms = 1;
	rsn->rsn_akms = IEEE80211_AKM_IEEE8021X;
	/* if RSN capabilities missing, default to 0 */
	rsn->rsn_caps = 0;

	/* read Group Cipher Suite field */
	if (frm + 4 > efrm)
		return 0;
	rsn->rsn_groupcipher = ieee80211_parse_rsn_cipher(frm);
	if (rsn->rsn_groupcipher == IEEE80211_CIPHER_USEGROUP)
		return IEEE80211_STATUS_BAD_GROUP_CIPHER;
	frm += 4;

	/* read Pairwise Cipher Suite Count field */
	if (frm + 2 > efrm)
		return 0;
	m = rsn->rsn_nciphers = LE_READ_2(frm);
	frm += 2;

	/* read Pairwise Cipher Suite List */
	if (frm + m * 4 > efrm)
		return IEEE80211_STATUS_IE_INVALID;
	rsn->rsn_ciphers = IEEE80211_CIPHER_NONE;
	while (m-- > 0) {
		rsn->rsn_ciphers |= ieee80211_parse_rsn_cipher(frm);
		frm += 4;
	}
	if (rsn->rsn_ciphers & IEEE80211_CIPHER_USEGROUP) {
		if (rsn->rsn_ciphers != IEEE80211_CIPHER_USEGROUP)
			return IEEE80211_STATUS_BAD_PAIRWISE_CIPHER;
		if (rsn->rsn_groupcipher == IEEE80211_CIPHER_CCMP)
			return IEEE80211_STATUS_BAD_PAIRWISE_CIPHER;
	}

	/* read AKM Suite List Count field */
	if (frm + 2 > efrm)
		return 0;
	n = rsn->rsn_nakms = LE_READ_2(frm);
	frm += 2;

	/* read AKM Suite List */
	if (frm + n * 4 > efrm)
		return IEEE80211_STATUS_IE_INVALID;
	rsn->rsn_akms = IEEE80211_AKM_NONE;
	while (n-- > 0) {
		rsn->rsn_akms |= ieee80211_parse_rsn_akm(frm);
		frm += 4;
	}

	/* read RSN Capabilities field */
	if (frm + 2 > efrm)
		return 0;
	rsn->rsn_caps = LE_READ_2(frm);
	frm += 2;

	/* read PMKID Count field */
	if (frm + 2 > efrm)
		return 0;
	s = LE_READ_2(frm);
	frm += 2;

	/* read PMKID List */
	if (frm + s * IEEE80211_PMKID_LEN > efrm)
		return IEEE80211_STATUS_IE_INVALID;
	while (s-- > 0) {
		/* ignore PMKIDs for now */
		frm += IEEE80211_PMKID_LEN;
	}

	return IEEE80211_STATUS_SUCCESS;
}

int
ieee80211_parse_rsn(struct ieee80211com *ic, const u_int8_t *frm,
    struct ieee80211_rsnparams *rsn)
{
	/* check IE length */
	if (frm[1] < 2) {
		IEEE80211_DPRINTF(("%s: invalid RSN/WPA2 IE;"
		    " length %u, expecting at least 2\n", __func__, frm[1]));
		ic->ic_stats.is_rx_elem_toosmall++;
		return IEEE80211_STATUS_IE_INVALID;
	}
	return ieee80211_parse_rsn_body(ic, frm + 2, frm[1], rsn);
}

int
ieee80211_parse_wpa(struct ieee80211com *ic, const u_int8_t *frm,
    struct ieee80211_rsnparams *rsn)
{
	/* check IE length */
	if (frm[1] < 6) {
		IEEE80211_DPRINTF(("%s: invalid WPA IE;"
		    " length %u, expecting at least 6\n", __func__, frm[1]));
		ic->ic_stats.is_rx_elem_toosmall++;
		return IEEE80211_STATUS_IE_INVALID;
	}
	return ieee80211_parse_rsn_body(ic, frm + 6, frm[1] - 4, rsn);
}

/*
 * Create (or update) a copy of an information element.
 */
int
ieee80211_save_ie(const u_int8_t *frm, u_int8_t **ie)
{
	if (*ie == NULL || (*ie)[1] != frm[1]) {
		if (*ie != NULL)
			free(*ie, M_DEVBUF);
		*ie = malloc(2 + frm[1], M_DEVBUF, M_NOWAIT);
		if (*ie == NULL)
			return ENOMEM;
	}
	memcpy(*ie, frm, 2 + frm[1]);
	return 0;
}

/*-
 * Beacon/Probe response frame format:
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
 * [tlv]  QoS Capability (Beacon only, 802.11e)
 */
void
ieee80211_recv_probe_resp(struct ieee80211com *ic, struct mbuf *m0,
    struct ieee80211_node *ni, int rssi, u_int32_t rstamp, int isprobe)
{
	const struct ieee80211_frame *wh;
	const u_int8_t *frm, *efrm;
	const u_int8_t *tstamp, *ssid, *rates, *xrates, *edcaie, *wmmie;
	const u_int8_t *rsnie, *wpaie;
	u_int16_t capinfo, bintval, fhdwell;
	u_int8_t chan, bchan, fhindex, erp;
	int is_new;

	/*
	 * We process beacon/probe response frames for:
	 *    o station mode: to collect state
	 *      updates such as 802.11g slot time and for passive
	 *      scanning of APs
	 *    o adhoc mode: to discover neighbors
	 *    o hostap mode: for passive scanning of neighbor APs
	 *    o when scanning
	 * In other words, in all modes other than monitor (which
	 * does not process incoming packets) and adhoc-demo (which
	 * does not use management frames at all).
	 */
#ifdef DIAGNOSTIC
	if (ic->ic_opmode != IEEE80211_M_STA &&
	    ic->ic_opmode != IEEE80211_M_IBSS &&
	    ic->ic_opmode != IEEE80211_M_HOSTAP &&
	    ic->ic_state != IEEE80211_S_SCAN) {
		panic("%s: impossible operating mode", __func__);
	}
#endif
	wh = mtod(m0, struct ieee80211_frame *);
	frm = (const u_int8_t *)&wh[1];
	efrm = mtod(m0, u_int8_t *) + m0->m_len;

	/* make sure all mandatory fixed fields are present */
	if (efrm - frm < 12) {
		IEEE80211_DPRINTF(("%s: frame too short\n", __func__));
		return;
	}
	tstamp  = frm; frm += 8;
	bintval = LE_READ_2(frm); frm += 2;
	capinfo = LE_READ_2(frm); frm += 2;

	ssid = rates = xrates = edcaie = wmmie = rsnie = wpaie = NULL;
	bchan = ieee80211_chan2ieee(ic, ic->ic_bss->ni_chan);
	chan = bchan;
	fhdwell = 0;
	fhindex = 0;
	erp = 0;
	while (frm + 2 <= efrm) {
		if (frm + 2 + frm[1] > efrm) {
			ic->ic_stats.is_rx_elem_toosmall++;
			return;
		}
		switch (frm[0]) {
		case IEEE80211_ELEMID_SSID:
			ssid = frm;
			break;
		case IEEE80211_ELEMID_RATES:
			rates = frm;
			break;
		case IEEE80211_ELEMID_FHPARMS:
			if (ic->ic_phytype != IEEE80211_T_FH)
				break;
			if (frm[1] < 5) {
				ic->ic_stats.is_rx_elem_toosmall++;
				break;
			}
			fhdwell = LE_READ_2(frm + 2);
			chan = IEEE80211_FH_CHAN(frm[4], frm[5]);
			fhindex = frm[6];
			break;
		case IEEE80211_ELEMID_DSPARMS:
			if (ic->ic_phytype == IEEE80211_T_FH)
				break;
			if (frm[1] < 1) {
				ic->ic_stats.is_rx_elem_toosmall++;
				break;
			}
			chan = frm[2];
			break;
		case IEEE80211_ELEMID_TIM:
			break;
		case IEEE80211_ELEMID_IBSSPARMS:
			break;
		case IEEE80211_ELEMID_XRATES:
			xrates = frm;
			break;
		case IEEE80211_ELEMID_ERP:
			if (frm[1] < 1) {
				ic->ic_stats.is_rx_elem_toosmall++;
				break;
			}
			erp = frm[2];
			break;
		case IEEE80211_ELEMID_RSN:
			rsnie = frm;
			break;
		case IEEE80211_ELEMID_EDCAPARMS:
			edcaie = frm;
			break;
		case IEEE80211_ELEMID_QOS_CAP:
			break;
		case IEEE80211_ELEMID_VENDOR:
			if (frm[1] < 4) {
				ic->ic_stats.is_rx_elem_toosmall++;
				break;
			}
			if (memcmp(frm + 2, MICROSOFT_OUI, 3) == 0) {
				if (frm[5] == 1)
					wpaie = frm;
				else if (frm[1] >= 5 &&
				    frm[5] == 2 && frm[6] == 1)
					wmmie = frm;
			}
			break;
		default:
			IEEE80211_DPRINTF2(("%s: element id %u/len %u "
			    "ignored\n", __func__, *frm, frm[1]));
			ic->ic_stats.is_rx_elem_unknown++;
			break;
		}
		frm += 2 + frm[1];
	}
	/* supported rates element is mandatory */
	if (rates == NULL || rates[1] > IEEE80211_RATE_MAXSIZE) {
		IEEE80211_DPRINTF(("%s: invalid supported rates element\n",
		    __func__));
		return;
	}
	/* SSID element is mandatory */
	if (ssid == NULL || ssid[1] > IEEE80211_NWID_LEN) {
		IEEE80211_DPRINTF(("%s: invalid SSID element\n", __func__));
		return;
	}
	if (
#if IEEE80211_CHAN_MAX < 255
	    chan > IEEE80211_CHAN_MAX ||
#endif
	    isclr(ic->ic_chan_active, chan)) {
		IEEE80211_DPRINTF(("%s: ignore %s with invalid channel "
		    "%u\n", __func__, isprobe ?
		    "probe response" : "beacon", chan));
		ic->ic_stats.is_rx_badchan++;
		return;
	}
	if ((ic->ic_state != IEEE80211_S_SCAN ||
	     !(ic->ic_caps & IEEE80211_C_SCANALL)) &&
	    chan != bchan && ic->ic_phytype != IEEE80211_T_FH) {
		/*
		 * Frame was received on a channel different from the
		 * one indicated in the DS params element id;
		 * silently discard it.
		 *
		 * NB: this can happen due to signal leakage.
		 *     But we should take it for FH phy because
		 *     the rssi value should be correct even for
		 *     different hop pattern in FH.
		 */
		IEEE80211_DPRINTF(("%s: ignore %s on channel %u marked "
		    "for channel %u\n", __func__, isprobe ?
		    "probe response" : "beacon", bchan, chan));
		ic->ic_stats.is_rx_chanmismatch++;
		return;
	}
	/*
	 * Use mac, channel and rssi so we collect only the
	 * best potential AP with the equal bssid while scanning.
	 * Collecting all potential APs may result in bloat of
	 * the node tree. This call will return NULL if the node
	 * for this APs does not exist or if the new node is the
	 * potential better one.
	 */
	if ((ni = ieee80211_find_node_for_beacon(ic, wh->i_addr2,
	    &ic->ic_channels[chan], ssid, rssi)) != NULL)
		return;

#ifdef IEEE80211_DEBUG
	if (ieee80211_debug &&
	    (ni == NULL || ic->ic_state == IEEE80211_S_SCAN)) {
		printf("%s: %s%s on chan %u (bss chan %u) ",
		    __func__, (ni == NULL ? "new " : ""),
		    isprobe ? "probe response" : "beacon",
		    chan, bchan);
		ieee80211_print_essid(ssid + 2, ssid[1]);
		printf(" from %s\n", ether_sprintf((u_int8_t *)wh->i_addr2));
		printf("%s: caps 0x%x bintval %u erp 0x%x\n",
			__func__, capinfo, bintval, erp);
	}
#endif

	if ((ni = ieee80211_find_node(ic, wh->i_addr2)) == NULL) {
		ni = ieee80211_alloc_node(ic, wh->i_addr2);
		if (ni == NULL)
			return;
		is_new = 1;
	} else
		is_new = 0;

	/*
	 * When operating in station mode, check for state updates
	 * while we're associated. We consider only 11g stuff right
	 * now.
	 */
	if (ic->ic_opmode == IEEE80211_M_STA &&
	    ic->ic_state == IEEE80211_S_RUN &&
	    ni->ni_state == IEEE80211_STA_BSS) {
		/*
		 * Check if protection mode has changed since last beacon.
		 */
		if (ni->ni_erp != erp) {
			IEEE80211_DPRINTF((
			    "[%s] erp change: was 0x%x, now 0x%x\n",
			    ether_sprintf((u_int8_t *)wh->i_addr2),
			    ni->ni_erp, erp));
			if (ic->ic_curmode == IEEE80211_MODE_11G &&
			    (erp & IEEE80211_ERP_USE_PROTECTION))
				ic->ic_flags |= IEEE80211_F_USEPROT;
			else
				ic->ic_flags &= ~IEEE80211_F_USEPROT;
			ic->ic_bss->ni_erp = erp;
		}
		/*
		 * Check if AP short slot time setting has changed
		 * since last beacon and give the driver a chance to
		 * update the hardware.
		 */
		if ((ni->ni_capinfo ^ capinfo) &
		    IEEE80211_CAPINFO_SHORT_SLOTTIME) {
			ieee80211_set_shortslottime(ic,
			    ic->ic_curmode == IEEE80211_MODE_11A ||
			    (capinfo & IEEE80211_CAPINFO_SHORT_SLOTTIME));
		}
	}
	/*
	 * We do not try to update EDCA parameters if QoS was not negotiated
	 * with the AP at association time.
	 */
	if (ni->ni_flags & IEEE80211_NODE_QOS) {
		/* always prefer EDCA IE over Wi-Fi Alliance WMM IE */
		if (edcaie != NULL)
			ieee80211_parse_edca_params(ic, edcaie);
		else if (wmmie != NULL)
			ieee80211_parse_wmm_params(ic, wmmie);
	}

	if (ic->ic_state == IEEE80211_S_SCAN &&
	    ic->ic_opmode != IEEE80211_M_HOSTAP &&
	    (ic->ic_flags & IEEE80211_F_RSNON)) {
		struct ieee80211_rsnparams rsn;
		const u_int8_t *saveie = NULL;
		/*
		 * If the AP advertises both RSN and WPA IEs (WPA1+WPA2),
		 * we only store the parameters of the highest protocol
		 * version we support.
		 */
		if (rsnie != NULL &&
		    (ic->ic_rsnprotos & IEEE80211_PROTO_RSN)) {
			if (ieee80211_parse_rsn(ic, rsnie, &rsn) == 0) {
				ni->ni_rsnprotos = IEEE80211_PROTO_RSN;
				saveie = rsnie;
			}
		} else if (wpaie != NULL &&
		    (ic->ic_rsnprotos & IEEE80211_PROTO_WPA)) {
			if (ieee80211_parse_wpa(ic, wpaie, &rsn) == 0) {
				ni->ni_rsnprotos = IEEE80211_PROTO_WPA;
				saveie = wpaie;
			}
		}
		if (saveie != NULL &&
		    ieee80211_save_ie(saveie, &ni->ni_rsnie) == 0) {
			ni->ni_rsnakms = rsn.rsn_akms;
			ni->ni_rsnciphers = rsn.rsn_ciphers;
			ni->ni_rsngroupcipher = rsn.rsn_groupcipher;
			ni->ni_rsncaps = rsn.rsn_caps;
		} else
			ni->ni_rsnprotos = IEEE80211_PROTO_NONE;
	} else if (ic->ic_state == IEEE80211_S_SCAN)
		ni->ni_rsnprotos = IEEE80211_PROTO_NONE;

	if (ssid[1] != 0 && ni->ni_esslen == 0) {
		ni->ni_esslen = ssid[1];
		memset(ni->ni_essid, 0, sizeof(ni->ni_essid));
		/* we know that ssid[1] <= IEEE80211_NWID_LEN */
		memcpy(ni->ni_essid, &ssid[2], ssid[1]);
	}
	IEEE80211_ADDR_COPY(ni->ni_bssid, wh->i_addr3);
	ni->ni_rssi = rssi;
	ni->ni_rstamp = rstamp;
	memcpy(ni->ni_tstamp, tstamp, sizeof(ni->ni_tstamp));
	ni->ni_intval = bintval;
	ni->ni_capinfo = capinfo;
	/* XXX validate channel # */
	ni->ni_chan = &ic->ic_channels[chan];
	ni->ni_fhdwell = fhdwell;
	ni->ni_fhindex = fhindex;
	ni->ni_erp = erp;
	/* NB: must be after ni_chan is setup */
	ieee80211_setup_rates(ic, ni, rates, xrates, IEEE80211_F_DOSORT);

	/*
	 * When scanning we record results (nodes) with a zero
	 * refcnt.  Otherwise we want to hold the reference for
	 * ibss neighbors so the nodes don't get released prematurely.
	 * Anything else can be discarded (XXX and should be handled
	 * above so we don't do so much work).
	 */
	if (ic->ic_opmode == IEEE80211_M_IBSS || (is_new && isprobe)) {
		/*
		 * Fake an association so the driver can setup it's
		 * private state.  The rate set has been setup above;
		 * there is no handshake as in ap/station operation.
		 */
		if (ic->ic_newassoc)
			(*ic->ic_newassoc)(ic, ni, 1);
	}
}

/*-
 * Probe request frame format:
 * [tlv] SSID
 * [tlv] Supported rates
 * [tlv] Extended Supported Rates (802.11g)
 */
void
ieee80211_recv_probe_req(struct ieee80211com *ic, struct mbuf *m0,
    struct ieee80211_node *ni, int rssi, u_int32_t rstamp)
{
	const struct ieee80211_frame *wh;
	const u_int8_t *frm, *efrm;
	const u_int8_t *ssid, *rates, *xrates;
	u_int8_t rate;

	if (ic->ic_opmode == IEEE80211_M_STA ||
	    ic->ic_state != IEEE80211_S_RUN)
		return;

	wh = mtod(m0, struct ieee80211_frame *);
	frm = (const u_int8_t *)&wh[1];
	efrm = mtod(m0, u_int8_t *) + m0->m_len;

	ssid = rates = xrates = NULL;
	while (frm + 2 <= efrm) {
		if (frm + 2 + frm[1] > efrm) {
			ic->ic_stats.is_rx_elem_toosmall++;
			return;
		}
		switch (frm[0]) {
		case IEEE80211_ELEMID_SSID:
			ssid = frm;
			break;
		case IEEE80211_ELEMID_RATES:
			rates = frm;
			break;
		case IEEE80211_ELEMID_XRATES:
			xrates = frm;
			break;
		}
		frm += 2 + frm[1];
	}
	/* supported rates element is mandatory */
	if (rates == NULL || rates[1] > IEEE80211_RATE_MAXSIZE) {
		IEEE80211_DPRINTF(("%s: invalid supported rates element\n",
		    __func__));
		return;
	}
	/* SSID element is mandatory */
	if (ssid == NULL || ssid[1] > IEEE80211_NWID_LEN) {
		IEEE80211_DPRINTF(("%s: invalid SSID element\n", __func__));
		return;
	}
	/* check that the specified SSID (if not wildcard) matches ours */
	if (ssid[1] != 0 && (ssid[1] != ic->ic_bss->ni_esslen ||
	    memcmp(&ssid[2], ic->ic_bss->ni_essid, ic->ic_bss->ni_esslen))) {
		IEEE80211_DPRINTF(("%s: SSID mismatch\n", __func__));
		ic->ic_stats.is_rx_ssidmismatch++;
		return;
	}
	/* refuse wildcard SSID if we're hiding our SSID in beacons */
	if (ssid[1] == 0 && (ic->ic_flags & IEEE80211_F_HIDENWID)) {
		IEEE80211_DPRINTF(("%s: wildcard SSID rejected", __func__));
		ic->ic_stats.is_rx_ssidmismatch++;
		return;
	}

	if (ni == ic->ic_bss) {
		ni = ieee80211_dup_bss(ic, wh->i_addr2);
		if (ni == NULL)
			return;
		IEEE80211_DPRINTF(("%s: new probe req from %s\n",
		    __func__, ether_sprintf((u_int8_t *)wh->i_addr2)));
	}
	ni->ni_rssi = rssi;
	ni->ni_rstamp = rstamp;
	rate = ieee80211_setup_rates(ic, ni, rates, xrates,
	    IEEE80211_F_DOSORT | IEEE80211_F_DOFRATE | IEEE80211_F_DONEGO |
	    IEEE80211_F_DODEL);
	if (rate & IEEE80211_RATE_BASIC) {
		IEEE80211_DPRINTF(("%s: rate mismatch for %s\n",
		    __func__, ether_sprintf((u_int8_t *)wh->i_addr2)));
		return;
	}
	IEEE80211_SEND_MGMT(ic, ni, IEEE80211_FC0_SUBTYPE_PROBE_RESP, 0);
}

/*-
 * Authentication frame format:
 * [2] Authentication algorithm number
 * [2] Authentication transaction sequence number
 * [2] Status code
 */
void
ieee80211_recv_auth(struct ieee80211com *ic, struct mbuf *m0,
    struct ieee80211_node *ni, int rssi, u_int32_t rstamp)
{
	const struct ieee80211_frame *wh;
	const u_int8_t *frm, *efrm;
	u_int16_t algo, seq, status;

	wh = mtod(m0, struct ieee80211_frame *);
	frm = (const u_int8_t *)&wh[1];
	efrm = mtod(m0, u_int8_t *) + m0->m_len;

	/* make sure all mandatory fixed fields are present */
	if (efrm - frm < 6) {
		IEEE80211_DPRINTF(("%s: frame too short\n", __func__));
		return;
	}
	algo   = LE_READ_2(frm); frm += 2;
	seq    = LE_READ_2(frm); frm += 2;
	status = LE_READ_2(frm); frm += 2;
	IEEE80211_DPRINTF(("%s: auth %d seq %d from %s\n",
	    __func__, algo, seq, ether_sprintf((u_int8_t *)wh->i_addr2)));

	/* only "open" auth mode is supported */
	if (algo != IEEE80211_AUTH_ALG_OPEN) {
		IEEE80211_DPRINTF(("%s: unsupported authentication "
		    "algorithm %d from %s\n",
		    __func__, algo, ether_sprintf((u_int8_t *)wh->i_addr2)));
		ic->ic_stats.is_rx_auth_unsupported++;
		if (ic->ic_opmode == IEEE80211_M_HOSTAP) {
			/* XXX hack to workaround calling convention */
			IEEE80211_SEND_MGMT(ic, ni,
				IEEE80211_FC0_SUBTYPE_AUTH,
				(seq+1) | (IEEE80211_STATUS_ALG<<16));
		}
		return;
	}
	ieee80211_auth_open(ic, wh, ni, rssi, rstamp, seq, status);
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
void
ieee80211_recv_assoc_req(struct ieee80211com *ic, struct mbuf *m0,
    struct ieee80211_node *ni, int rssi, u_int32_t rstamp, int reassoc)
{
	const struct ieee80211_frame *wh;
	const u_int8_t *frm, *efrm;
	const u_int8_t *ssid, *rates, *xrates, *rsnie, *wpaie;
	u_int16_t capinfo, bintval;
	int resp, status = 0;
	struct ieee80211_rsnparams rsn;
	u_int8_t rate;

	if (ic->ic_opmode != IEEE80211_M_HOSTAP ||
	    ic->ic_state != IEEE80211_S_RUN)
		return;

	wh = mtod(m0, struct ieee80211_frame *);
	frm = (const u_int8_t *)&wh[1];
	efrm = mtod(m0, u_int8_t *) + m0->m_len;

	resp = reassoc ? IEEE80211_FC0_SUBTYPE_REASSOC_RESP :
	    IEEE80211_FC0_SUBTYPE_ASSOC_RESP;

	/* make sure all mandatory fixed fields are present */
	if (efrm - frm < (reassoc ? 10 : 4)) {
		IEEE80211_DPRINTF(("%s: frame too short\n", __func__));
		return;
	}
	if (!IEEE80211_ADDR_EQ(wh->i_addr3, ic->ic_bss->ni_bssid)) {
		IEEE80211_DPRINTF(("%s: ignore other bss from %s\n",
		    __func__, ether_sprintf((u_int8_t *)wh->i_addr2)));
		ic->ic_stats.is_rx_assoc_bss++;
		return;
	}
	capinfo = LE_READ_2(frm); frm += 2;
	bintval = LE_READ_2(frm); frm += 2;
	if (reassoc)
		frm += IEEE80211_ADDR_LEN;	/* skip current AP address */

	ssid = rates = xrates = rsnie = wpaie = NULL;
	while (frm + 2 <= efrm) {
		if (frm + 2 + frm[1] > efrm) {
			ic->ic_stats.is_rx_elem_toosmall++;
			return;
		}
		switch (frm[0]) {
		case IEEE80211_ELEMID_SSID:
			ssid = frm;
			break;
		case IEEE80211_ELEMID_RATES:
			rates = frm;
			break;
		case IEEE80211_ELEMID_XRATES:
			xrates = frm;
			break;
		case IEEE80211_ELEMID_RSN:
			rsnie = frm;
			break;
		case IEEE80211_ELEMID_QOS_CAP:
			break;
		case IEEE80211_ELEMID_VENDOR:
			if (frm[1] < 4) {
				ic->ic_stats.is_rx_elem_toosmall++;
				break;
			}
			if (memcmp(frm + 2, MICROSOFT_OUI, 3) == 0) {
				if (frm[5] == 1)
					wpaie = frm;
			}
			break;
		}
		frm += 2 + frm[1];
	}
	/* supported rates element is mandatory */
	if (rates == NULL || rates[1] > IEEE80211_RATE_MAXSIZE) {
		IEEE80211_DPRINTF(("%s: invalid supported rates element\n",
		    __func__));
		return;
	}
	/* SSID element is mandatory */
	if (ssid == NULL || ssid[1] > IEEE80211_NWID_LEN) {
		IEEE80211_DPRINTF(("%s: invalid SSID element\n", __func__));
		return;
	}
	/* check that the specified SSID matches ours */
	if (ssid[1] != ic->ic_bss->ni_esslen ||
	    memcmp(&ssid[2], ic->ic_bss->ni_essid, ic->ic_bss->ni_esslen)) {
		IEEE80211_DPRINTF(("%s: SSID mismatch\n", __func__));
		ic->ic_stats.is_rx_ssidmismatch++;
		return;
	}

	if (ni->ni_state != IEEE80211_STA_AUTH &&
	    ni->ni_state != IEEE80211_STA_ASSOC) {
		IEEE80211_DPRINTF(
		    ("%s: deny %sassoc from %s, not authenticated\n",
		    __func__, reassoc ? "re" : "",
		    ether_sprintf((u_int8_t *)wh->i_addr2)));
		ni = ieee80211_dup_bss(ic, wh->i_addr2);
		if (ni != NULL) {
			IEEE80211_SEND_MGMT(ic, ni,
			    IEEE80211_FC0_SUBTYPE_DEAUTH,
			    IEEE80211_REASON_ASSOC_NOT_AUTHED);
		}
		ic->ic_stats.is_rx_assoc_notauth++;
		return;
	}

	if (!(capinfo & IEEE80211_CAPINFO_ESS)) {
		ic->ic_stats.is_rx_assoc_capmismatch++;
		status = IEEE80211_STATUS_CAPINFO;
		goto end;
	}
	rate = ieee80211_setup_rates(ic, ni, rates, xrates,
	    IEEE80211_F_DOSORT | IEEE80211_F_DOFRATE | IEEE80211_F_DONEGO |
	    IEEE80211_F_DODEL);
	if (rate & IEEE80211_RATE_BASIC) {
		ic->ic_stats.is_rx_assoc_norate++;
		status = IEEE80211_STATUS_BASIC_RATE;
		goto end;
	}

	if (ic->ic_flags & IEEE80211_F_RSNON) {
		const u_int8_t *saveie;
		/*
		 * A station should never include both a WPA and an RSN IE
		 * in its (Re)Association Requests, but if it does, we only
		 * consider the IE of the highest version of the protocol
		 * that is allowed (ie RSN over WPA).
		 */
		if (rsnie != NULL &&
		    (ic->ic_rsnprotos & IEEE80211_PROTO_RSN)) {
			status = ieee80211_parse_rsn(ic, rsnie, &rsn);
			if (status != 0)
				goto end;
			ni->ni_rsnprotos = IEEE80211_PROTO_RSN;
			saveie = rsnie;
		} else if (wpaie != NULL &&
		    (ic->ic_rsnprotos & IEEE80211_PROTO_WPA)) {
			status = ieee80211_parse_wpa(ic, wpaie, &rsn);
			if (status != 0)
				goto end;
			ni->ni_rsnprotos = IEEE80211_PROTO_WPA;
			saveie = wpaie;
		} else {
			/*
			 * In an RSN, an AP shall not associate with STAs
			 * that fail to include the RSN IE in the
			 * (Re)Association Request.
			 */
			status = IEEE80211_STATUS_IE_INVALID;
			goto end;
		}
		/*
		 * The initiating STA's RSN IE shall include one authentication
		 * and pairwise cipher suite among those advertised by the
		 * targeted AP.  It shall also specify the group cipher suite
		 * specified by the targeted AP.
		 */
		if (rsn.rsn_nakms != 1 ||
		    !(rsn.rsn_akms & ic->ic_bss->ni_rsnakms)) {
			status = IEEE80211_STATUS_BAD_AKMP;
			goto end;
		}
		if (rsn.rsn_nciphers != 1 ||
		    !(rsn.rsn_ciphers & ic->ic_bss->ni_rsnciphers)) {
			status = IEEE80211_STATUS_BAD_PAIRWISE_CIPHER;
			goto end;
		}
		if (rsn.rsn_groupcipher != ic->ic_bss->ni_rsngroupcipher) {
			status = IEEE80211_STATUS_BAD_GROUP_CIPHER;
			goto end;
		}
		/*
		 * Disallow new associations using TKIP if countermeasures
		 * are active.
		 */
		if ((ic->ic_flags & IEEE80211_F_COUNTERM) &&
		    (rsn.rsn_ciphers == IEEE80211_CIPHER_TKIP ||
		     rsn.rsn_groupcipher == IEEE80211_CIPHER_TKIP)) {
			status = IEEE80211_STATUS_CIPHER_REJ_POLICY;
			goto end;
		}

		/* everything looks fine, save IE and parameters */
		if (ieee80211_save_ie(saveie, &ni->ni_rsnie) != 0) {
			status = IEEE80211_STATUS_TOOMANY;
			goto end;
		}
		ni->ni_rsnakms = rsn.rsn_akms;
		ni->ni_rsnciphers = rsn.rsn_ciphers;
		ni->ni_rsngroupcipher = ic->ic_bss->ni_rsngroupcipher;
		ni->ni_rsncaps = rsn.rsn_caps;
	} else
		ni->ni_rsnprotos = IEEE80211_PROTO_NONE;

	ni->ni_rssi = rssi;
	ni->ni_rstamp = rstamp;
	ni->ni_intval = bintval;
	ni->ni_capinfo = capinfo;
	ni->ni_chan = ic->ic_bss->ni_chan;
	ni->ni_fhdwell = ic->ic_bss->ni_fhdwell;
	ni->ni_fhindex = ic->ic_bss->ni_fhindex;
 end:
	if (status != 0) {
		IEEE80211_SEND_MGMT(ic, ni, resp, status);
		ieee80211_node_leave(ic, ni);
	} else
		ieee80211_node_join(ic, ni, resp);
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
void
ieee80211_recv_assoc_resp(struct ieee80211com *ic, struct mbuf *m0,
    struct ieee80211_node *ni, int reassoc)
{
	struct ifnet *ifp = &ic->ic_if;
	const struct ieee80211_frame *wh;
	const u_int8_t *frm, *efrm;
	const u_int8_t *rates, *xrates, *edcaie, *wmmie;
	u_int16_t capinfo, status, associd;
	u_int8_t rate;

	if (ic->ic_opmode != IEEE80211_M_STA ||
	    ic->ic_state != IEEE80211_S_ASSOC) {
		ic->ic_stats.is_rx_mgtdiscard++;
		return;
	}

	wh = mtod(m0, struct ieee80211_frame *);
	frm = (const u_int8_t *)&wh[1];
	efrm = mtod(m0, u_int8_t *) + m0->m_len;

	/* make sure all mandatory fixed fields are present */
	if (efrm - frm < 6) {
		IEEE80211_DPRINTF(("%s: frame too short\n", __func__));
		return;
	}
	capinfo = LE_READ_2(frm); frm += 2;
	status =  LE_READ_2(frm); frm += 2;
	if (status != IEEE80211_STATUS_SUCCESS) {
		if (ifp->if_flags & IFF_DEBUG)
			printf("%s: %sassociation failed (reason %d)"
			    " for %s\n", ifp->if_xname,
			    reassoc ?  "re" : "",
			    status, ether_sprintf((u_int8_t *)wh->i_addr3));
		if (ni != ic->ic_bss)
			ni->ni_fails++;
		ic->ic_stats.is_rx_auth_fail++;
		return;
	}
	associd = LE_READ_2(frm); frm += 2;

	rates = xrates = edcaie = wmmie = NULL;
	while (frm + 2 <= efrm) {
		if (frm + 2 + frm[1] > efrm) {
			ic->ic_stats.is_rx_elem_toosmall++;
			return;
		}
		switch (frm[0]) {
		case IEEE80211_ELEMID_RATES:
			rates = frm;
			break;
		case IEEE80211_ELEMID_XRATES:
			xrates = frm;
			break;
		case IEEE80211_ELEMID_EDCAPARMS:
			edcaie = frm;
			break;
		case IEEE80211_ELEMID_VENDOR:
			if (frm[1] < 4) {
				ic->ic_stats.is_rx_elem_toosmall++;
				break;
			}
			if (memcmp(frm + 2, MICROSOFT_OUI, 3) == 0) {
				if (frm[1] >= 5 && frm[5] == 2 && frm[6] == 1)
					wmmie = frm;
			}
			break;
		}
		frm += 2 + frm[1];
	}
	/* supported rates element is mandatory */
	if (rates == NULL || rates[1] > IEEE80211_RATE_MAXSIZE) {
		IEEE80211_DPRINTF(("%s: invalid supported rates element\n",
		    __func__));
		return;
	}
	rate = ieee80211_setup_rates(ic, ni, rates, xrates,
	    IEEE80211_F_DOSORT | IEEE80211_F_DOFRATE | IEEE80211_F_DONEGO |
	    IEEE80211_F_DODEL);
	if (rate & IEEE80211_RATE_BASIC) {
		IEEE80211_DPRINTF(("%s: rate mismatch for %s\n",
		    __func__, ether_sprintf((u_int8_t *)wh->i_addr2)));
		ic->ic_stats.is_rx_assoc_norate++;
		return;
	}
	ni->ni_capinfo = capinfo;
	ni->ni_associd = associd;
	if (edcaie != NULL || wmmie != NULL) {
		/* force update of EDCA parameters */
		ic->ic_edca_updtcount = -1;

		if ((edcaie != NULL &&
		     ieee80211_parse_edca_params(ic, edcaie) == 0) ||
		    (wmmie != NULL &&
		     ieee80211_parse_wmm_params(ic, wmmie) == 0))
			ni->ni_flags |= IEEE80211_NODE_QOS;
		else	/* for Reassociation */
			ni->ni_flags &= ~IEEE80211_NODE_QOS;
	}
	/*
	 * Configure state now that we are associated.
	 */
	if (ic->ic_curmode == IEEE80211_MODE_11A ||
	    (ni->ni_capinfo & IEEE80211_CAPINFO_SHORT_PREAMBLE))
		ic->ic_flags |= IEEE80211_F_SHPREAMBLE;
	else
		ic->ic_flags &= ~IEEE80211_F_SHPREAMBLE;

	ieee80211_set_shortslottime(ic,
	    ic->ic_curmode == IEEE80211_MODE_11A ||
	    (ni->ni_capinfo & IEEE80211_CAPINFO_SHORT_SLOTTIME));
	/*
	 * Honor ERP protection.
	 */
	if (ic->ic_curmode == IEEE80211_MODE_11G &&
	    (ni->ni_erp & IEEE80211_ERP_USE_PROTECTION))
		ic->ic_flags |= IEEE80211_F_USEPROT;
	else
		ic->ic_flags &= ~IEEE80211_F_USEPROT;
	/*
	 * If not an RSNA, mark the port as valid, otherwise wait for
	 * 802.1X authentication and 4-way handshake to complete..
	 */
	if (ic->ic_flags & IEEE80211_F_RSNON) {
		/* XXX ic->ic_mgt_timer = 5; */
	}
	ieee80211_new_state(ic, IEEE80211_S_RUN,
	    IEEE80211_FC0_SUBTYPE_ASSOC_RESP);
}

/*-
 * Deauthentication frame format:
 * [2] Reason code
 */
void
ieee80211_recv_deauth(struct ieee80211com *ic, struct mbuf *m0,
    struct ieee80211_node *ni)
{
	struct ifnet *ifp = &ic->ic_if;
	const struct ieee80211_frame *wh;
	const u_int8_t *frm, *efrm;
	u_int16_t reason;

	wh = mtod(m0, struct ieee80211_frame *);
	frm = (const u_int8_t *)&wh[1];
	efrm = mtod(m0, u_int8_t *) + m0->m_len;

	/* make sure all mandatory fixed fields are present */
	if (efrm - frm < 2) {
		IEEE80211_DPRINTF(("%s: frame too short\n", __func__));
		return;
	}
	reason = LE_READ_2(frm);

	ic->ic_stats.is_rx_deauth++;
	switch (ic->ic_opmode) {
	case IEEE80211_M_STA:
		ieee80211_new_state(ic, IEEE80211_S_AUTH,
		    IEEE80211_FC0_SUBTYPE_DEAUTH);
		break;
	case IEEE80211_M_HOSTAP:
		if (ni != ic->ic_bss) {
			if (ifp->if_flags & IFF_DEBUG)
				printf("%s: station %s deauthenticated "
				    "by peer (reason %d)\n",
				    ifp->if_xname,
				    ether_sprintf(ni->ni_macaddr),
				    reason);
			ieee80211_node_leave(ic, ni);
		}
		break;
	default:
		break;
	}
}

/*-
 * Disassociation frame format:
 * [2] Reason code
 */
void
ieee80211_recv_disassoc(struct ieee80211com *ic, struct mbuf *m0,
    struct ieee80211_node *ni)
{
	struct ifnet *ifp = &ic->ic_if;
	const struct ieee80211_frame *wh;
	const u_int8_t *frm, *efrm;
	u_int16_t reason;

	wh = mtod(m0, struct ieee80211_frame *);
	frm = (const u_int8_t *)&wh[1];
	efrm = mtod(m0, u_int8_t *) + m0->m_len;

	/* make sure all mandatory fixed fields are present */
	if (efrm - frm < 2) {
		IEEE80211_DPRINTF(("%s: frame too short\n", __func__));
		return;
	}
	reason = LE_READ_2(frm);

	ic->ic_stats.is_rx_disassoc++;
	switch (ic->ic_opmode) {
	case IEEE80211_M_STA:
		ieee80211_new_state(ic, IEEE80211_S_ASSOC,
		    IEEE80211_FC0_SUBTYPE_DISASSOC);
		break;
	case IEEE80211_M_HOSTAP:
		if (ni != ic->ic_bss) {
			if (ifp->if_flags & IFF_DEBUG)
				printf("%s: station %s disassociated "
				    "by peer (reason %d)\n",
				    ifp->if_xname,
				    ether_sprintf(ni->ni_macaddr),
				    reason);
			ieee80211_node_leave(ic, ni);
		}
		break;
	default:
		break;
	}
}

/*-
 * Action frame format:
 * [1] Action
 */
void
ieee80211_recv_action(struct ieee80211com *ic, struct mbuf *m0,
    struct ieee80211_node *ni)
{
	/* TBD */
}

void
ieee80211_recv_mgmt(struct ieee80211com *ic, struct mbuf *m0,
    struct ieee80211_node *ni, int subtype, int rssi, u_int32_t rstamp)
{
	switch (subtype) {
	case IEEE80211_FC0_SUBTYPE_BEACON:
		ieee80211_recv_probe_resp(ic, m0, ni, rssi, rstamp, 0);
		break;
	case IEEE80211_FC0_SUBTYPE_PROBE_RESP:
		ieee80211_recv_probe_resp(ic, m0, ni, rssi, rstamp, 1);
		break;
	case IEEE80211_FC0_SUBTYPE_PROBE_REQ:
		ieee80211_recv_probe_req(ic, m0, ni, rssi, rstamp);
		break;
	case IEEE80211_FC0_SUBTYPE_AUTH:
		ieee80211_recv_auth(ic, m0, ni, rssi, rstamp);
		break;
	case IEEE80211_FC0_SUBTYPE_ASSOC_REQ:
		ieee80211_recv_assoc_req(ic, m0, ni, rssi, rstamp, 0);
		break;
	case IEEE80211_FC0_SUBTYPE_REASSOC_REQ:
		ieee80211_recv_assoc_req(ic, m0, ni, rssi, rstamp, 1);
		break;
	case IEEE80211_FC0_SUBTYPE_ASSOC_RESP:
		ieee80211_recv_assoc_resp(ic, m0, ni, 0);
		break;
	case IEEE80211_FC0_SUBTYPE_REASSOC_RESP:
		ieee80211_recv_assoc_resp(ic, m0, ni, 1);
		break;
	case IEEE80211_FC0_SUBTYPE_DEAUTH:
		ieee80211_recv_deauth(ic, m0, ni);
		break;
	case IEEE80211_FC0_SUBTYPE_DISASSOC:
		ieee80211_recv_disassoc(ic, m0, ni);
		break;
	case IEEE80211_FC0_SUBTYPE_ACTION:
		ieee80211_recv_action(ic, m0, ni);
		break;
	default:
		IEEE80211_DPRINTF(("%s: mgmt frame with subtype 0x%x not "
		    "handled\n", __func__, subtype));
		ic->ic_stats.is_rx_badsubtype++;
		break;
	}
}

/* unaligned big endian access */
#define BE_READ_2(p)						\
	((u_int16_t)(p)[0] << 8 | (u_int16_t)(p)[1])

#define BE_READ_8(p)						\
	((u_int64_t)(p)[0] << 56 | (u_int64_t)(p)[1] << 48 |	\
	 (u_int64_t)(p)[2] << 40 | (u_int64_t)(p)[3] << 32 |	\
	 (u_int64_t)(p)[4] << 24 | (u_int64_t)(p)[5] << 16 |	\
	 (u_int64_t)(p)[6] <<  8 | (u_int64_t)(p)[7])

/* unaligned little endian access */
#define LE_READ_6(p)						\
	((u_int64_t)(p)[5] << 40 | (u_int64_t)(p)[4] << 32 |	\
	 (u_int64_t)(p)[3] << 24 | (u_int64_t)(p)[2] << 16 |	\
	 (u_int64_t)(p)[1] <<  8 | (u_int64_t)(p)[0])

/*
 * 4-Way Handshake Message 1 is sent by the authenticator to the supplicant
 * (see 8.5.3.1).
 */
void
ieee80211_recv_4way_msg1(struct ieee80211com *ic,
    struct ieee80211_eapol_key *key, struct ieee80211_node *ni)
{
	struct ieee80211_ptk tptk;
	const u_int8_t *frm, *efrm;
	const u_int8_t *pmkid;
	const u_int8_t *pmk;

	if (ic->ic_opmode != IEEE80211_M_STA &&
	    ic->ic_opmode != IEEE80211_M_IBSS)
		return;

	if (ni->ni_replaycnt_ok &&
	    BE_READ_8(key->replaycnt) <= ni->ni_replaycnt) {
		ic->ic_stats.is_rx_eapol_replay++;
		return;
	}
	/* save authenticator's nonce (ANonce) */
	memcpy(ni->ni_nonce, key->nonce, EAPOL_KEY_NONCE_LEN);

	/* parse key data field (may contain an encapsulated PMKID) */
	frm = (const u_int8_t *)&key[1];
	efrm = frm + BE_READ_2(key->paylen);

	pmkid = NULL;
	while (frm + 2 <= efrm) {
		if (frm + 2 + frm[1] > efrm)
			break;
		switch (frm[0]) {
		case IEEE80211_ELEMID_VENDOR:
			if (frm[1] < 4)
				break;
			if (memcmp(&frm[2], IEEE80211_OUI, 3) == 0) {
				switch (frm[5]) {
				case IEEE80211_KDE_PMKID:
					pmkid = frm;
					break;
				}
			}
			break;
		}
		frm += 2 + frm[1];
	}
	/* check that the PMKID KDE is valid (if present) */
	if (pmkid != NULL && pmkid[1] < 4 + 16)
		return;

	/* generate a new supplicant's nonce (SNonce) */
	arc4random_bytes(ic->ic_nonce, EAPOL_KEY_NONCE_LEN);

	/* retrieve PMK and derive TPTK */
	if ((pmk = ieee80211_get_pmk(ic, ni, pmkid)) == NULL) {
		/* no PMK configured for this STA/PMKID */
		return;
	}
	ieee80211_derive_ptk(pmk, IEEE80211_PMK_LEN, ni->ni_macaddr,
	    ic->ic_myaddr, key->nonce, ic->ic_nonce, (u_int8_t *)&tptk,
	    sizeof(tptk));

	if (ic->ic_if.if_flags & IFF_DEBUG)
		printf("%s: received msg %d/%d of the %s handshake from %s\n",
		    ic->ic_if.if_xname, 1, 4, "4-way",
		    ether_sprintf(ni->ni_macaddr));

	/* send message 2 to authenticator using TPTK */
	(void)ieee80211_send_4way_msg2(ic, ni, key->replaycnt, &tptk);
}

/*
 * 4-Way Handshake Message 2 is sent by the supplicant to the authenticator
 * (see 8.5.3.2).
 */
void
ieee80211_recv_4way_msg2(struct ieee80211com *ic,
    struct ieee80211_eapol_key *key, struct ieee80211_node *ni,
    const u_int8_t *rsnie)
{
	struct ieee80211_ptk tptk;
	const u_int8_t *pmk;

	if (ic->ic_opmode != IEEE80211_M_HOSTAP &&
	    ic->ic_opmode != IEEE80211_M_IBSS)
		return;

	/* discard if we're not expecting this message */
	if (ni->ni_rsn_state != RSNA_PTKSTART &&
	    ni->ni_rsn_state != RSNA_PTKCALCNEGOTIATING) {
		IEEE80211_DPRINTF(("%s: unexpected in state: %d\n",
		    __func__, ni->ni_rsn_state));
		return;
	}
	ni->ni_rsn_state = RSNA_PTKCALCNEGOTIATING;

	/* replay counter has already been verified by caller */

	/* retrieve PMK and derive TPTK */
	if ((pmk = ieee80211_get_pmk(ic, ni, NULL)) == NULL) {
		/* no PMK configured for this STA */
		return;	/* will timeout.. */
	}
	ieee80211_derive_ptk(pmk, IEEE80211_PMK_LEN, ic->ic_myaddr,
	    ni->ni_macaddr, ni->ni_nonce, key->nonce, (u_int8_t *)&tptk,
	    sizeof(tptk));

	/* check Key MIC field using KCK */
	if (ieee80211_eapol_key_check_mic(key, tptk.kck) != 0) {
		IEEE80211_DPRINTF(("%s: key MIC failed\n", __func__));
		ic->ic_stats.is_rx_eapol_badmic++;
		return;	/* will timeout.. */
	}

	timeout_del(&ni->ni_rsn_timeout);
	ni->ni_rsn_state = RSNA_PTKCALCNEGOTIATING_2;
	ni->ni_rsn_retries = 0;

	/* install TPTK as PTK now that MIC is verified */
	memcpy(&ni->ni_ptk, &tptk, sizeof(tptk));

	/*
	 * The RSN IE must match bit-wise with what the STA included in its
	 * (Re)Association Request.
	 */
	if (ni->ni_rsnie == NULL || rsnie[1] != ni->ni_rsnie[1] ||
	    memcmp(rsnie, ni->ni_rsnie, 2 + rsnie[1]) != 0) {
		IEEE80211_SEND_MGMT(ic, ni, IEEE80211_FC0_SUBTYPE_DEAUTH,
		    IEEE80211_REASON_RSN_DIFFERENT_IE);
		ieee80211_node_leave(ic, ni);
		return;
	}

	if (ic->ic_if.if_flags & IFF_DEBUG)
		printf("%s: received msg %d/%d of the %s handshake from %s\n",
		    ic->ic_if.if_xname, 2, 4, "4-way",
		    ether_sprintf(ni->ni_macaddr));

	/* send message 3 to supplicant */
	(void)ieee80211_send_4way_msg3(ic, ni);
}

/*
 * 4-Way Handshake Message 3 is sent by the authenticator to the supplicant
 * (see 8.5.3.3).
 */
void
ieee80211_recv_4way_msg3(struct ieee80211com *ic,
    struct ieee80211_eapol_key *key, struct ieee80211_node *ni)
{
	struct ieee80211_ptk tptk;
	struct ieee80211_key *k;
	const u_int8_t *frm, *efrm;
	const u_int8_t *rsnie1, *rsnie2, *gtk;
	const u_int8_t *pmk;
	u_int16_t info, reason = 0;

	if (ic->ic_opmode != IEEE80211_M_STA &&
	    ic->ic_opmode != IEEE80211_M_IBSS)
		return;

	if (ni->ni_replaycnt_ok &&
	    BE_READ_8(key->replaycnt) <= ni->ni_replaycnt) {
		ic->ic_stats.is_rx_eapol_replay++;
		return;
	}

	/* check that ANonce matches that of message 1 */
	if (memcmp(key->nonce, ni->ni_nonce, EAPOL_KEY_NONCE_LEN) != 0) {
		IEEE80211_DPRINTF(("%s: ANonce does not match msg 1/4\n",
		    __func__));
		return;
	}
	/* retrieve PMK and derive TPTK */
	if ((pmk = ieee80211_get_pmk(ic, ni, NULL)) == NULL) {
		/* no PMK configured for this STA */
		return;
	}
	ieee80211_derive_ptk(pmk, IEEE80211_PMK_LEN, ni->ni_macaddr,
	    ic->ic_myaddr, key->nonce, ic->ic_nonce, (u_int8_t *)&tptk,
	    sizeof(tptk));

	info = BE_READ_2(key->info);

	/* check Key MIC field using KCK */
	if (ieee80211_eapol_key_check_mic(key, tptk.kck) != 0) {
		IEEE80211_DPRINTF(("%s: key MIC failed\n", __func__));
		ic->ic_stats.is_rx_eapol_badmic++;
		return;
	}
	/* install TPTK as PTK now that MIC is verified */
	memcpy(&ni->ni_ptk, &tptk, sizeof(tptk));

	/* if encrypted, decrypt Key Data field using KEK */
	if ((info & EAPOL_KEY_ENCRYPTED) &&
	    ieee80211_eapol_key_decrypt(key, ni->ni_ptk.kek) != 0) {
		IEEE80211_DPRINTF(("%s: decryption failed\n", __func__));
		return;
	}

	/* parse key data field */
	frm = (const u_int8_t *)&key[1];
	efrm = frm + BE_READ_2(key->paylen);

	/*
	 * Some WPA1+WPA2 APs (like hostapd) appear to include both WPA and
	 * RSN IEs in message 3/4.  We only take into account the IE of the
	 * version of the protocol we negotiated at association time.
	 */
	rsnie1 = rsnie2 = gtk = NULL;
	while (frm + 2 <= efrm) {
		if (frm + 2 + frm[1] > efrm)
			break;
		switch (frm[0]) {
		case IEEE80211_ELEMID_RSN:
			if (ni->ni_rsnprotos != IEEE80211_PROTO_RSN)
				break;
			if (rsnie1 == NULL)
				rsnie1 = frm;
			else if (rsnie2 == NULL)
				rsnie2 = frm;
			/* ignore others if more than two RSN IEs */
			break;
		case IEEE80211_ELEMID_VENDOR:
			if (frm[1] < 4)
				break;
			if (memcmp(&frm[2], IEEE80211_OUI, 3) == 0) {
				switch (frm[5]) {
				case IEEE80211_KDE_GTK:
					gtk = frm;
					break;
				}
			} else if (memcmp(&frm[2], MICROSOFT_OUI, 3) == 0) {
				switch (frm[5]) {
				case 1:	/* WPA */
					if (ni->ni_rsnprotos !=
					    IEEE80211_PROTO_WPA)
						break;
					rsnie1 = frm;
					break;
				}
			}
			break;
		}
		frm += 2 + frm[1];
	}
	/* first WPA/RSN IE is mandatory */
	if (rsnie1 == NULL) {
		IEEE80211_DPRINTF(("%s: missing RSN IE\n", __func__));
		return;
	}
	/* key data must be encrypted if GTK is included */
	if (gtk != NULL && !(info & EAPOL_KEY_ENCRYPTED)) {
		IEEE80211_DPRINTF(("%s: GTK not encrypted\n", __func__));
		return;
	}
	/*
	 * Check that first WPA/RSN IE is identical to the one received in
	 * the beacon or probe response frame.
	 */
	if (ni->ni_rsnie == NULL || rsnie1[1] != ni->ni_rsnie[1] ||
	    memcmp(rsnie1, ni->ni_rsnie, 2 + rsnie1[1]) != 0) {
		reason = IEEE80211_REASON_RSN_DIFFERENT_IE;
		goto deauth;
	}

	/*
	 * If a second RSN information element is present, use its pairwise
	 * cipher suite or deauthenticate.
	 */
	if (rsnie2 != NULL) {
		struct ieee80211_rsnparams rsn;

		if (ieee80211_parse_rsn(ic, rsnie2, &rsn) == 0) {
			if (rsn.rsn_akms != ni->ni_rsnakms ||
			    rsn.rsn_groupcipher != ni->ni_rsngroupcipher ||
			    rsn.rsn_nciphers != 1 ||
			    !(rsn.rsn_ciphers & ic->ic_rsnciphers)) {
				reason = IEEE80211_REASON_BAD_PAIRWISE_CIPHER;
				goto deauth;
			}
			/* use pairwise cipher suite of second RSN IE */
			ni->ni_rsnciphers = rsn.rsn_ciphers;
			ni->ni_rsncipher = ni->ni_rsnciphers;
		}
	}

	/* update the last seen value of the key replay counter field */
	ni->ni_replaycnt = BE_READ_8(key->replaycnt);
	ni->ni_replaycnt_ok = 1;

	if (ic->ic_if.if_flags & IFF_DEBUG)
		printf("%s: received msg %d/%d of the %s handshake from %s\n",
		    ic->ic_if.if_xname, 3, 4, "4-way",
		    ether_sprintf(ni->ni_macaddr));

	/* send message 4 to authenticator */
	if (ieee80211_send_4way_msg4(ic, ni) != 0)
		return;	/* ..authenticator will retry */

	if (info & EAPOL_KEY_INSTALL) {
		u_int64_t prsc;

		/* check that key length matches that of pairwise cipher */
		if (BE_READ_2(key->keylen) !=
		    ieee80211_cipher_keylen(ni->ni_rsncipher)) {
			reason = IEEE80211_REASON_AUTH_LEAVE;
			goto deauth;
		}
		/* install the PTK */
		prsc = (gtk == NULL) ? LE_READ_6(key->rsc) : 0;
		k = &ni->ni_pairwise_key;
		ieee80211_map_ptk(&ni->ni_ptk, ni->ni_rsncipher, prsc, k);
		if ((*ic->ic_set_key)(ic, ni, k) != 0) {
			reason = IEEE80211_REASON_AUTH_LEAVE;
			goto deauth;
		}
	}
	if (gtk != NULL) {
		u_int64_t rsc;
		u_int8_t kid;

		/* check that the GTK KDE is valid */
		if (gtk[1] < 4 + 2) {
			reason = IEEE80211_REASON_AUTH_LEAVE;
			goto deauth;
		}
		/* check that key length matches that of group cipher */
		if (gtk[1] - 6 !=
		    ieee80211_cipher_keylen(ni->ni_rsngroupcipher)) {
			reason = IEEE80211_REASON_AUTH_LEAVE;
			goto deauth;
		}
		/* install the GTK */
		kid = gtk[6] & 3;
		rsc = LE_READ_6(key->rsc);
		k = &ic->ic_nw_keys[kid];
		ieee80211_map_gtk(&gtk[8], ni->ni_rsngroupcipher, kid,
		    gtk[6] & (1 << 2), rsc, k);
		if ((*ic->ic_set_key)(ic, ni, k) != 0) {
			reason = IEEE80211_REASON_AUTH_LEAVE;
			goto deauth;
		}
	}
	if (info & EAPOL_KEY_SECURE) {
		if (ic->ic_opmode != IEEE80211_M_IBSS ||
		    ++ni->ni_key_count == 2) {
			IEEE80211_DPRINTF(("%s: marking port %s valid\n",
			    __func__, ether_sprintf(ni->ni_macaddr)));
			ni->ni_port_valid = 1;
		}
	}
 deauth:
	if (reason != 0) {
		IEEE80211_SEND_MGMT(ic, ni, IEEE80211_FC0_SUBTYPE_DEAUTH,
		    reason);
		ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);
	}
}

/*
 * 4-Way Handshake Message 4 is sent by the supplicant to the authenticator
 * (see 8.5.3.4).
 */
void
ieee80211_recv_4way_msg4(struct ieee80211com *ic,
    struct ieee80211_eapol_key *key, struct ieee80211_node *ni)
{
	if (ic->ic_opmode != IEEE80211_M_HOSTAP &&
	    ic->ic_opmode != IEEE80211_M_IBSS)
		return;

	/* discard if we're not expecting this message */
	if (ni->ni_rsn_state != RSNA_PTKINITNEGOTIATING) {
		IEEE80211_DPRINTF(("%s: unexpected in state: %d\n",
		    __func__, ni->ni_rsn_state));
		return;
	}

	/* replay counter has already been verified by caller */

	/* check Key MIC field using KCK */
	if (ieee80211_eapol_key_check_mic(key, ni->ni_ptk.kck) != 0) {
		IEEE80211_DPRINTF(("%s: key MIC failed\n", __func__));
		ic->ic_stats.is_rx_eapol_badmic++;
		return;	/* will timeout.. */
	}

	timeout_del(&ni->ni_rsn_timeout);
	ni->ni_rsn_state = RSNA_PTKINITDONE;
	ni->ni_rsn_retries = 0;

	if (ni->ni_rsncipher != IEEE80211_CIPHER_USEGROUP) {
		/* install the PTK */
		struct ieee80211_key *k = &ni->ni_pairwise_key;
		ieee80211_map_ptk(&ni->ni_ptk, ni->ni_rsncipher, 0, k);
		if ((*ic->ic_set_key)(ic, ni, k) != 0) {
			IEEE80211_SEND_MGMT(ic, ni,
			    IEEE80211_FC0_SUBTYPE_DEAUTH,
			    IEEE80211_REASON_ASSOC_TOOMANY);
			ieee80211_node_leave(ic, ni);
			return;
		}
	}
	if (ic->ic_opmode != IEEE80211_M_IBSS || ++ni->ni_key_count == 2) {
		IEEE80211_DPRINTF(("%s: marking port %s valid\n", __func__,
		    ether_sprintf(ni->ni_macaddr)));
		ni->ni_port_valid = 1;
	}

	if (ic->ic_if.if_flags & IFF_DEBUG)
		printf("%s: received msg %d/%d of the %s handshake from %s\n",
		    ic->ic_if.if_xname, 4, 4, "4-way",
		    ether_sprintf(ni->ni_macaddr));

	/* initiate a group key handshake for WPA */
	if (ni->ni_rsnprotos == IEEE80211_PROTO_WPA)
		(void)ieee80211_send_group_msg1(ic, ni);
	else
		ni->ni_rsn_gstate = RSNA_IDLE;
}

/*
 * Differentiate Message 2 from Message 4 of the 4-Way Handshake based on
 * the presence of an RSN or WPA Information Element.
 */
void
ieee80211_recv_4way_msg2or4(struct ieee80211com *ic,
    struct ieee80211_eapol_key *key, struct ieee80211_node *ni)
{
	const u_int8_t *frm, *efrm;
	const u_int8_t *rsnie;

	if (BE_READ_8(key->replaycnt) != ni->ni_replaycnt) {
		ic->ic_stats.is_rx_eapol_replay++;
		return;
	}

	/* parse key data field (check if an RSN IE is present) */
	frm = (const u_int8_t *)&key[1];
	efrm = frm + BE_READ_2(key->paylen);

	rsnie = NULL;
	while (frm + 2 <= efrm) {
		if (frm + 2 + frm[1] > efrm)
			break;
		switch (frm[0]) {
		case IEEE80211_ELEMID_RSN:
			rsnie = frm;
			break;
		case IEEE80211_ELEMID_VENDOR:
			if (frm[1] < 4)
				break;
			if (memcmp(&frm[2], MICROSOFT_OUI, 3) == 0) {
				switch (frm[5]) {
				case 1:	/* WPA */
					rsnie = frm;
					break;
				}
			}
		}
		frm += 2 + frm[1];
	}
	if (rsnie != NULL)
		ieee80211_recv_4way_msg2(ic, key, ni, rsnie);
	else
		ieee80211_recv_4way_msg4(ic, key, ni);
}

/*
 * Group Key Handshake Message 1 is sent by the authenticator to the
 * supplicant (see 8.5.4.1).
 */
void
ieee80211_recv_rsn_group_msg1(struct ieee80211com *ic,
    struct ieee80211_eapol_key *key, struct ieee80211_node *ni)
{
	struct ieee80211_key *k;
	const u_int8_t *frm, *efrm;
	const u_int8_t *gtk;
	u_int64_t rsc;
	u_int16_t info;
	u_int8_t kid;

	if (ic->ic_opmode != IEEE80211_M_STA &&
	    ic->ic_opmode != IEEE80211_M_IBSS)
		return;

	if (BE_READ_8(key->replaycnt) <= ni->ni_replaycnt) {
		ic->ic_stats.is_rx_eapol_replay++;
		return;
	}
	/* check Key MIC field using KCK */
	if (ieee80211_eapol_key_check_mic(key, ni->ni_ptk.kck) != 0) {
		IEEE80211_DPRINTF(("%s: key MIC failed\n", __func__));
		ic->ic_stats.is_rx_eapol_badmic++;
		return;
	}
	info = BE_READ_2(key->info);

	/* check that encrypted and decrypt Key Data field using KEK */
	if (!(info & EAPOL_KEY_ENCRYPTED) ||
	    ieee80211_eapol_key_decrypt(key, ni->ni_ptk.kek) != 0) {
		IEEE80211_DPRINTF(("%s: decryption failed\n", __func__));
		return;
	}

	/* parse key data field (shall contain a GTK KDE) */
	frm = (const u_int8_t *)&key[1];
	efrm = frm + BE_READ_2(key->paylen);

	gtk = NULL;
	while (frm + 2 <= efrm) {
		if (frm + 2 + frm[1] > efrm)
			break;
		switch (frm[0]) {
		case IEEE80211_ELEMID_VENDOR:
			if (frm[1] < 4)
				break;
			if (memcmp(&frm[2], IEEE80211_OUI, 3) == 0) {
				switch (frm[5]) {
				case IEEE80211_KDE_GTK:
					gtk = frm;
					break;
				}
			}
			break;
		}
		frm += 2 + frm[1];
	}
	/* check that the GTK KDE is present and valid */
	if (gtk == NULL || gtk[1] < 4 + 2) {
		IEEE80211_DPRINTF(("%s: missing or invalid GTK KDE\n",
		    __func__));
		return;
	}

	/* check that key length matches that of group cipher */
	if (gtk[1] - 6 != ieee80211_cipher_keylen(ni->ni_rsngroupcipher))
		return;

	/* install the GTK */
	kid = gtk[6] & 3;
	rsc = LE_READ_6(key->rsc);
	k = &ic->ic_nw_keys[kid];
	ieee80211_map_gtk(&gtk[8], ni->ni_rsngroupcipher, kid,
	    gtk[6] & (1 << 2), rsc, k);
	if ((*ic->ic_set_key)(ic, ni, k) != 0) {
		IEEE80211_SEND_MGMT(ic, ni, IEEE80211_FC0_SUBTYPE_DEAUTH,
		    IEEE80211_REASON_AUTH_LEAVE);
		ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);
		return;
	}
	if (info & EAPOL_KEY_SECURE) {
		if (ic->ic_opmode != IEEE80211_M_IBSS ||
		    ++ni->ni_key_count == 2) {
			IEEE80211_DPRINTF(("%s: marking port %s valid\n",
			    __func__, ether_sprintf(ni->ni_macaddr)));
			ni->ni_port_valid = 1;
		}
	}
	/* update the last seen value of the key replay counter field */
	ni->ni_replaycnt = BE_READ_8(key->replaycnt);

	if (ic->ic_if.if_flags & IFF_DEBUG)
		printf("%s: received msg %d/%d of the %s handshake from %s\n",
		    ic->ic_if.if_xname, 1, 2, "group key",
		    ether_sprintf(ni->ni_macaddr));

	/* send message 2 to authenticator */
	(void)ieee80211_send_group_msg2(ic, ni, k);
}

void
ieee80211_recv_wpa_group_msg1(struct ieee80211com *ic,
    struct ieee80211_eapol_key *key, struct ieee80211_node *ni)
{
	struct ieee80211_key *k;
	const u_int8_t *frm;
	u_int64_t rsc;
	u_int16_t info;
	u_int8_t kid;
	int keylen;

	if (ic->ic_opmode != IEEE80211_M_STA &&
	    ic->ic_opmode != IEEE80211_M_IBSS)
		return;

	if (BE_READ_8(key->replaycnt) <= ni->ni_replaycnt) {
		ic->ic_stats.is_rx_eapol_replay++;
		return;
	}
	/* check Key MIC field using KCK */
	if (ieee80211_eapol_key_check_mic(key, ni->ni_ptk.kck) != 0) {
		IEEE80211_DPRINTF(("%s: key MIC failed\n", __func__));
		ic->ic_stats.is_rx_eapol_badmic++;
		return;
	}
	/*
	 * EAPOL-Key data field is encrypted even though WPA doesn't set
	 * the ENCRYPTED bit in the info field.
	 */
	if (ieee80211_eapol_key_decrypt(key, ni->ni_ptk.kek) != 0) {
		IEEE80211_DPRINTF(("%s: decryption failed\n", __func__));
		return;
	}
	info = BE_READ_2(key->info);
	keylen = ieee80211_cipher_keylen(ni->ni_rsngroupcipher);

	/* check that key length matches that of group cipher */
	if (BE_READ_2(key->keylen) != keylen)
		return;

	/* check that the data length is large enough to hold the key */
	if (BE_READ_2(key->paylen) < keylen)
		return;

	/* key data field contains the GTK */
	frm = (const u_int8_t *)&key[1];

	/* install the GTK */
	kid = (info >> EAPOL_KEY_WPA_KID_SHIFT) & 3;
	rsc = LE_READ_6(key->rsc);
	k = &ic->ic_nw_keys[kid];
	ieee80211_map_gtk(frm, ni->ni_rsngroupcipher, kid,
	    info & EAPOL_KEY_WPA_TX, rsc, k);
	if ((*ic->ic_set_key)(ic, ni, k) != 0) {
		IEEE80211_SEND_MGMT(ic, ni, IEEE80211_FC0_SUBTYPE_DEAUTH,
		    IEEE80211_REASON_AUTH_LEAVE);
		ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);
		return;
	}
	if (info & EAPOL_KEY_SECURE) {
		if (ic->ic_opmode != IEEE80211_M_IBSS ||
		    ++ni->ni_key_count == 2) {
			IEEE80211_DPRINTF(("%s: marking port %s valid\n",
			    __func__, ether_sprintf(ni->ni_macaddr)));
			ni->ni_port_valid = 1;
		}
	}
	/* update the last seen value of the key replay counter field */
	ni->ni_replaycnt = BE_READ_8(key->replaycnt);

	if (ic->ic_if.if_flags & IFF_DEBUG)
		printf("%s: received msg %d/%d of the %s handshake from %s\n",
		    ic->ic_if.if_xname, 1, 2, "group key",
		    ether_sprintf(ni->ni_macaddr));

	/* send message 2 to authenticator */
	(void)ieee80211_send_group_msg2(ic, ni, k);
}

/*
 * Group Key Handshake Message 2 is sent by the supplicant to the
 * authenticator (see 8.5.4.2).
 */
void
ieee80211_recv_group_msg2(struct ieee80211com *ic,
    struct ieee80211_eapol_key *key, struct ieee80211_node *ni)
{
	if (ic->ic_opmode != IEEE80211_M_HOSTAP &&
	    ic->ic_opmode != IEEE80211_M_IBSS)
		return;

	/* discard if we're not expecting this message */
	if (ni->ni_rsn_gstate != RSNA_REKEYNEGOTIATING) {
		IEEE80211_DPRINTF(("%s: unexpected in state: %d\n",
		    __func__, ni->ni_rsn_state));
		return;
	}
	if (BE_READ_8(key->replaycnt) != ni->ni_replaycnt) {
		ic->ic_stats.is_rx_eapol_replay++;
		return;
	}
	/* check Key MIC field using KCK */
	if (ieee80211_eapol_key_check_mic(key, ni->ni_ptk.kck) != 0) {
		IEEE80211_DPRINTF(("%s: key MIC failed\n", __func__));
		ic->ic_stats.is_rx_eapol_badmic++;
		return;
	}

	timeout_del(&ni->ni_rsn_timeout);
	ni->ni_rsn_gstate = RSNA_REKEYESTABLISHED;

	if ((ni->ni_flags & IEEE80211_NODE_REKEY) &&
	    --ic->ic_rsn_keydonesta == 0)
		ieee80211_setkeysdone(ic);
	ni->ni_flags &= ~IEEE80211_NODE_REKEY;

	ni->ni_rsn_gstate = RSNA_IDLE;
	ni->ni_rsn_retries = 0;

	if (ic->ic_if.if_flags & IFF_DEBUG)
		printf("%s: received msg %d/%d of the %s handshake from %s\n",
		    ic->ic_if.if_xname, 2, 2, "group key",
		    ether_sprintf(ni->ni_macaddr));
}

/*
 * EAPOL-Key Request frames are sent by the supplicant to request that the
 * authenticator initiates either a 4-Way Handshake or Group Key Handshake,
 * or to report a MIC failure in a TKIP MSDU.
 */
void
ieee80211_recv_eapol_key_req(struct ieee80211com *ic,
    struct ieee80211_eapol_key *key, struct ieee80211_node *ni)
{
	u_int16_t info;

	if (ic->ic_opmode != IEEE80211_M_HOSTAP &&
	    ic->ic_opmode != IEEE80211_M_IBSS)
		return;

	info = BE_READ_2(key->info);

	/* enforce monotonicity of key request replay counter */
	if (ni->ni_reqreplaycnt_ok &&
	    BE_READ_8(key->replaycnt) <= ni->ni_reqreplaycnt) {
		ic->ic_stats.is_rx_eapol_replay++;
		return;
	}
	if (!(info & EAPOL_KEY_KEYMIC) ||
	    ieee80211_eapol_key_check_mic(key, ni->ni_ptk.kck) != 0) {
		IEEE80211_DPRINTF(("%s: key MIC failed\n", __func__));
		ic->ic_stats.is_rx_eapol_badmic++;
		return;
	}
	/* update key request replay counter now that MIC is verified */
	ni->ni_reqreplaycnt = BE_READ_8(key->replaycnt);
	ni->ni_reqreplaycnt_ok = 1;

	if (info & EAPOL_KEY_ERROR) {	/* TKIP MIC failure */
		/* ignore reports from STAs not using TKIP */
		if (ic->ic_bss->ni_rsngroupcipher != IEEE80211_CIPHER_TKIP &&
		    ni->ni_rsncipher != IEEE80211_CIPHER_TKIP) {
			IEEE80211_DPRINTF(("%s: MIC failure report from "
			    "STA not using TKIP: %s\n", __func__,
			    ether_sprintf(ni->ni_macaddr)));
			return;
		}
		ic->ic_stats.is_rx_remmicfail++;
		ieee80211_michael_mic_failure(ic, LE_READ_6(key->rsc));

	} else if (info & EAPOL_KEY_PAIRWISE) {
		/* initiate a 4-Way Handshake */

	} else {
		/*
		 * Should change the GTK, initiate the 4-Way Handshake and
		 * then execute a Group Key Handshake with all supplicants.
		 */
	}
}

/*
 * Process an incoming EAPOL frame.  Notice that we are only interested in
 * EAPOL-Key frames with an IEEE 802.11 or WPA descriptor type.
 */
void
ieee80211_recv_eapol(struct ieee80211com *ic, struct mbuf *m0,
    struct ieee80211_node *ni)
{
	struct ifnet *ifp = &ic->ic_if;
	struct ether_header *eh;
	struct ieee80211_eapol_key *key;
	u_int16_t info, desc;

	ifp->if_ibytes += m0->m_pkthdr.len;

	if (m0->m_len < sizeof(*eh) + sizeof(*key))
		return;
	eh = mtod(m0, struct ether_header *);
	if (IEEE80211_IS_MULTICAST(eh->ether_dhost)) {
		ifp->if_imcasts++;
		return;
	}
	m_adj(m0, sizeof(*eh));
	key = mtod(m0, struct ieee80211_eapol_key *);

	if (key->type != EAPOL_KEY)
		return;
	ic->ic_stats.is_rx_eapol_key++;

	if ((ni->ni_rsnprotos == IEEE80211_PROTO_RSN &&
	     key->desc != EAPOL_KEY_DESC_IEEE80211) ||
	    (ni->ni_rsnprotos == IEEE80211_PROTO_WPA &&
	     key->desc != EAPOL_KEY_DESC_WPA))
		return;

	/* check packet body length */
	if (m0->m_len < 4 + BE_READ_2(key->len))
		return;

	/* check key data length */
	if (m0->m_len < sizeof(*key) + BE_READ_2(key->paylen))
		return;

	info = BE_READ_2(key->info);

	/* discard EAPOL-Key frames with an unknown descriptor version */
	desc = info & EAPOL_KEY_VERSION_MASK;
	if (desc != EAPOL_KEY_DESC_V1 && desc != EAPOL_KEY_DESC_V2)
		return;

	if ((ni->ni_rsncipher == IEEE80211_CIPHER_CCMP ||
	     ni->ni_rsngroupcipher == IEEE80211_CIPHER_CCMP) &&
	    desc != EAPOL_KEY_DESC_V2)
		return;

	/* determine message type (see 8.5.3.7) */
	if (info & EAPOL_KEY_REQUEST) {
		/* EAPOL-Key Request frame */
		ieee80211_recv_eapol_key_req(ic, key, ni);

	} else if (info & EAPOL_KEY_PAIRWISE) {
		/* 4-Way Handshake */
		if (info & EAPOL_KEY_KEYMIC) {
			if (info & EAPOL_KEY_KEYACK)
				ieee80211_recv_4way_msg3(ic, key, ni);
			else
				ieee80211_recv_4way_msg2or4(ic, key, ni);
		} else if (info & EAPOL_KEY_KEYACK)
			ieee80211_recv_4way_msg1(ic, key, ni);
	} else {
		/* Group Key Handshake */
		if (!(info & EAPOL_KEY_KEYMIC))
			return;
		if (info & EAPOL_KEY_KEYACK) {
			if (key->desc == EAPOL_KEY_DESC_WPA)
				ieee80211_recv_wpa_group_msg1(ic, key, ni);
			else
				ieee80211_recv_rsn_group_msg1(ic, key, ni);
		} else
			ieee80211_recv_group_msg2(ic, key, ni);
	}
}

void
ieee80211_recv_pspoll(struct ieee80211com *ic, struct mbuf *m0)
{
	struct ifnet *ifp = &ic->ic_if;
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni;
	struct mbuf *m;
	u_int16_t aid;

	if (ic->ic_set_tim == NULL)	/* no powersaving functionality */
		return;

	wh = mtod(m0, struct ieee80211_frame *);

	if ((ni = ieee80211_find_node(ic, wh->i_addr2)) == NULL) {
		if (ifp->if_flags & IFF_DEBUG)
			printf("%s: station %s sent bogus power save poll\n",
			    ifp->if_xname, ether_sprintf(wh->i_addr2));
		return;
	}

	memcpy(&aid, wh->i_dur, sizeof(wh->i_dur));
	if ((aid & 0xc000) != 0xc000) {
		if (ifp->if_flags & IFF_DEBUG)
			printf("%s: station %s sent bogus aid %x\n",
			    ifp->if_xname, ether_sprintf(wh->i_addr2), aid);
		return;
	}

	if (aid != ni->ni_associd) {
		if (ifp->if_flags & IFF_DEBUG)
			printf("%s: station %s aid %x doesn't match pspoll "
			    "aid %x\n", ifp->if_xname,
			    ether_sprintf(wh->i_addr2), ni->ni_associd, aid);
		return;
	}

	/* Okay, take the first queued packet and put it out... */

	IF_DEQUEUE(&ni->ni_savedq, m);
	if (m == NULL) {
		if (ifp->if_flags & IFF_DEBUG)
			printf("%s: station %s sent pspoll, "
			    "but no packets are saved\n",
			    ifp->if_xname, ether_sprintf(wh->i_addr2));
		return;
	}
	wh = mtod(m, struct ieee80211_frame *);

	/*
	 * If this is the last packet, turn off the TIM fields.
	 * If there are more packets, set the more packets bit.
	 */

	if (IF_IS_EMPTY(&ni->ni_savedq))
		(*ic->ic_set_tim)(ic, ni->ni_associd, 0);
	else
		wh->i_fc[1] |= IEEE80211_FC1_MORE_DATA;

	if (ifp->if_flags & IFF_DEBUG)
		printf("%s: enqueued power saving packet for station %s\n",
		    ifp->if_xname, ether_sprintf(ni->ni_macaddr));

	IF_ENQUEUE(&ic->ic_pwrsaveq, m);
	(*ifp->if_start)(ifp);
}
