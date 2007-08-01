/*	$NetBSD: ieee80211_input.c,v 1.24 2004/05/31 11:12:24 dyoung Exp $	*/
/*	$OpenBSD: ieee80211_input.c,v 1.50 2007/08/01 12:32:26 damien Exp $	*/
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


int	ieee80211_setup_rates(struct ieee80211com *, struct ieee80211_node *,
	    const u_int8_t *, const u_int8_t *, int);
void	ieee80211_auth_open(struct ieee80211com *,
	    const struct ieee80211_frame *, struct ieee80211_node *, int,
	    u_int32_t, u_int16_t, u_int16_t);
int	ieee80211_parse_edca_params_body(struct ieee80211com *,
	    const u_int8_t *);
int	ieee80211_parse_edca_params(struct ieee80211com *, const u_int8_t *);
int	ieee80211_parse_wmm_params(struct ieee80211com *, const u_int8_t *);
enum	ieee80211_cipher ieee80211_parse_rsn_cipher(const u_int8_t[]);
enum	ieee80211_akm ieee80211_parse_rsn_akm(const u_int8_t[]);
int	ieee80211_parse_rsn_body(struct ieee80211com *,
	    struct ieee80211_node *, const u_int8_t *, u_int);
int	ieee80211_parse_rsn(struct ieee80211com *, struct ieee80211_node *,
	    const u_int8_t *);
int	ieee80211_parse_wpa1(struct ieee80211com *, struct ieee80211_node *,
	    const u_int8_t *);
void	ieee80211_recv_pspoll(struct ieee80211com *, struct mbuf *, int,
	    u_int32_t);
int	ieee80211_do_slow_print(struct ieee80211com *, int *);
void	ieee80211_recv_probe_resp(struct ieee80211com *, struct mbuf *,
	    struct ieee80211_node *, int, u_int32_t);
void	ieee80211_recv_probe_req(struct ieee80211com *, struct mbuf *,
	    struct ieee80211_node *, int, u_int32_t);
void	ieee80211_recv_auth(struct ieee80211com *, struct mbuf *,
	    struct ieee80211_node *, int, u_int32_t);
void	ieee80211_recv_assoc_req(struct ieee80211com *, struct mbuf *,
	    struct ieee80211_node *, int, u_int32_t);
void	ieee80211_recv_assoc_resp(struct ieee80211com *, struct mbuf *,
	    struct ieee80211_node *, int, u_int32_t);
void	ieee80211_recv_deauth(struct ieee80211com *, struct mbuf *,
	    struct ieee80211_node *, int, u_int32_t);
void	ieee80211_recv_disassoc(struct ieee80211com *, struct mbuf *,
	    struct ieee80211_node *, int, u_int32_t);
void	ieee80211_recv_4way_msg1(struct ieee80211com *,
	    const struct ieee80211_eapol_key *, struct ieee80211_node *);
void	ieee80211_recv_4way_msg2(struct ieee80211com *,
	    const struct ieee80211_eapol_key *, struct ieee80211_node *);
void	ieee80211_recv_4way_msg3(struct ieee80211com *,
	    const struct ieee80211_eapol_key *, struct ieee80211_node *);
void	ieee80211_recv_4way_msg4(struct ieee80211com *,
	    const struct ieee80211_eapol_key *, struct ieee80211_node *);
void	ieee80211_recv_group_msg1(struct ieee80211com *,
	    const struct ieee80211_eapol_key *, struct ieee80211_node *);
void	ieee80211_recv_group_msg2(struct ieee80211com *,
	    const struct ieee80211_eapol_key *, struct ieee80211_node *);
void	ieee80211_recv_eapol_key_req(struct ieee80211com *,
	    const struct ieee80211_eapol_key *, struct ieee80211_node *);

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
		panic("null mode");

	/* trim CRC here so WEP can find its own CRC at the end of packet. */
	if (m->m_flags & M_HASFCS) {
		m_adj(m, -IEEE80211_CRC_LEN);
		m->m_flags &= ~M_HASFCS;
	}

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

	if (ic->ic_set_tim != NULL &&
	    (wh->i_fc[1] & IEEE80211_FC1_PWR_MGT) &&
	    ni->ni_pwrsave == 0) {
		/* turn on power save mode */

		if (ifp->if_flags & IFF_DEBUG)
			printf("%s: power save mode on for %s\n",
			    ifp->if_xname, ether_sprintf(wh->i_addr2));

		ni->ni_pwrsave = IEEE80211_PS_SLEEP;
	}
	if (ic->ic_set_tim != NULL &&
	    !(wh->i_fc[1] & IEEE80211_FC1_PWR_MGT) &&
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
		if (wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_QOS)
			hdrlen = sizeof(struct ieee80211_qosframe);
		else
			hdrlen = sizeof(struct ieee80211_frame);
		if (wh->i_fc[1] & IEEE80211_FC1_WEP) {
			if (ic->ic_flags & IEEE80211_F_WEPON) {
				m = ieee80211_wep_crypt(ifp, m, 0);
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
#if 0
		if (!ni->ni_port_valid &&
		    eh->ether_type != htons(ETHERTYPE_PAE)) {
			IEEE80211_DPRINTF(("%s: port not valid: %s\n",
			    __func__, ether_sprintf(wh->i_addr2)));
			ic->ic_stats.is_rx_unauth++;
			goto err;
		}
#endif
		ifp->if_ipackets++;

		/* perform as a bridge within the AP */
		m1 = NULL;
		if (ic->ic_opmode == IEEE80211_M_HOSTAP &&
		    (ic->ic_flags & IEEE80211_F_NOBRIDGE) == 0) {
			if (ETHER_IS_MULTICAST(eh->ether_dhost)) {
				m1 = m_copym(m, 0, M_COPYALL, M_DONTWAIT);
				if (m1 == NULL)
					ifp->if_oerrors++;
				else
					m1->m_flags |= M_MCAST;
			} else {
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
			ieee80211_recv_pspoll(ic, m, rssi, rstamp);
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

/*
 * Install received rate set information in the node's state block.
 */
int
ieee80211_setup_rates(struct ieee80211com *ic, struct ieee80211_node *ni,
    const u_int8_t *rates, const u_int8_t *xrates, int flags)
{
	struct ieee80211_rateset *rs = &ni->ni_rates;

	memset(rs, 0, sizeof(*rs));
	rs->rs_nrates = rates[1];
	memcpy(rs->rs_rates, rates + 2, rs->rs_nrates);
	if (xrates != NULL) {
		u_int8_t nxrates;
		/*
		 * Tack on 11g extended supported rate element.
		 */
		nxrates = xrates[1];
		if (rs->rs_nrates + nxrates > IEEE80211_RATE_MAXSIZE) {
			nxrates = IEEE80211_RATE_MAXSIZE - rs->rs_nrates;
			IEEE80211_DPRINTF(("%s: extended rate set too large;"
				" only using %u of %u rates\n",
				__func__, nxrates, xrates[1]));
			ic->ic_stats.is_rx_rstoobig++;
		}
		memcpy(rs->rs_rates + rs->rs_nrates, xrates+2, nxrates);
		rs->rs_nrates += nxrates;
	}
	return ieee80211_fix_rate(ic, ni, flags);
}

/* Verify the existence and length of __elem or get out. */
#define IEEE80211_VERIFY_ELEMENT(__elem, __maxlen) do {			\
	if ((__elem) == NULL) {						\
		IEEE80211_DPRINTF(("%s: no " #__elem "in %s frame\n",	\
			__func__, ieee80211_mgt_subtype_name[		\
				(wh->i_fc[0] &				\
				    IEEE80211_FC0_SUBTYPE_MASK) >>	\
				IEEE80211_FC0_SUBTYPE_SHIFT]));		\
		ic->ic_stats.is_rx_elem_missing++;			\
		return;							\
	}								\
	if ((__elem)[1] > (__maxlen)) {					\
		IEEE80211_DPRINTF(("%s: bad " #__elem " len %d in %s "	\
			"frame from %s\n", __func__, (__elem)[1],	\
			ieee80211_mgt_subtype_name[(wh->i_fc[0] & 	\
			        IEEE80211_FC0_SUBTYPE_MASK) >>		\
			    IEEE80211_FC0_SUBTYPE_SHIFT],		\
			ether_sprintf((u_int8_t *)wh->i_addr2)));	\
		ic->ic_stats.is_rx_elem_toobig++;			\
		return;							\
	}								\
} while (0)

#define	IEEE80211_VERIFY_LENGTH(_len, _minlen) do {			\
	if ((_len) < (_minlen)) {					\
		IEEE80211_DPRINTF(("%s: %s frame too short from %s\n",	\
			__func__,					\
			ieee80211_mgt_subtype_name[(wh->i_fc[0] & 	\
			        IEEE80211_FC0_SUBTYPE_MASK) >>		\
			    IEEE80211_FC0_SUBTYPE_SHIFT],		\
			ether_sprintf((u_int8_t *)wh->i_addr2)));	\
		ic->ic_stats.is_rx_elem_toosmall++;			\
		return;							\
	}								\
} while (0)

#ifdef IEEE80211_DEBUG
void
ieee80211_ssid_mismatch(struct ieee80211com *, const char *,
    const u_int8_t[IEEE80211_ADDR_LEN], const u_int8_t *);
void
ieee80211_ssid_mismatch(struct ieee80211com *ic, const char *tag,
    const u_int8_t mac[IEEE80211_ADDR_LEN], const u_int8_t *ssid)
{
	printf("[%s] %s req ssid mismatch: ",
	    ether_sprintf((u_int8_t *)mac), tag);
	ieee80211_print_essid(ssid + 2, ssid[1]);
	printf("\n");
}

#define IEEE80211_VERIFY_SSID(_ni, _ssid, _packet_type) do {		\
	if ((_ssid)[1] != 0 &&						\
	    ((_ssid)[1] != (_ni)->ni_esslen ||				\
	    memcmp((_ssid) + 2, (_ni)->ni_essid, (_ssid)[1]) != 0)) {   \
		if (ieee80211_debug)					\
			ieee80211_ssid_mismatch(ic, _packet_type,       \
				wh->i_addr2, _ssid);			\
		ic->ic_stats.is_rx_ssidmismatch++;			\
		return;							\
	}								\
} while (0)
#else /* !IEEE80211_DEBUG */
#define IEEE80211_VERIFY_SSID(_ni, _ssid, _packet_type) do {		\
	if ((_ssid)[1] != 0 &&						\
	    ((_ssid)[1] != (_ni)->ni_esslen ||				\
	    memcmp((_ssid) + 2, (_ni)->ni_essid, (_ssid)[1]) != 0)) {   \
		ic->ic_stats.is_rx_ssidmismatch++;			\
		return;							\
	}								\
} while (0)
#endif /* !IEEE80211_DEBUG */

void
ieee80211_auth_open(struct ieee80211com *ic, const struct ieee80211_frame *wh,
    struct ieee80211_node *ni, int rssi, u_int32_t rstamp, u_int16_t seq,
    u_int16_t status)
{
	struct ifnet *ifp = &ic->ic_if;
	switch (ic->ic_opmode) {
	case IEEE80211_M_IBSS:
		if (ic->ic_state != IEEE80211_S_RUN ||
		    seq != IEEE80211_AUTH_OPEN_REQUEST) {
			IEEE80211_DPRINTF(("%s: discard auth from %s; "
			    "state %u, seq %u\n", __func__,
			    ether_sprintf((u_int8_t *)wh->i_addr2),
			    ic->ic_state, seq));
			ic->ic_stats.is_rx_bad_auth++;
			return;
		}
		ieee80211_new_state(ic, IEEE80211_S_AUTH,
		    wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK);
		break;

	case IEEE80211_M_AHDEMO:
		/* should not come here */
		break;

	case IEEE80211_M_HOSTAP:
		if (ic->ic_state != IEEE80211_S_RUN ||
		    seq != IEEE80211_AUTH_OPEN_REQUEST) {
			IEEE80211_DPRINTF(("%s: discard auth from %s; "
			    "state %u, seq %u\n", __func__,
			    ether_sprintf((u_int8_t *)wh->i_addr2),
			    ic->ic_state, seq));
			ic->ic_stats.is_rx_bad_auth++;
			return;
		}
		if (ni == ic->ic_bss) {
			ni = ieee80211_alloc_node(ic, wh->i_addr2);
			if (ni == NULL) {
				ic->ic_stats.is_rx_nodealloc++;
				return;
			}
			IEEE80211_ADDR_COPY(ni->ni_bssid, ic->ic_bss->ni_bssid);
			ni->ni_rssi = rssi;
			ni->ni_rstamp = rstamp;
			ni->ni_chan = ic->ic_bss->ni_chan;
		}
		IEEE80211_SEND_MGMT(ic, ni,
			IEEE80211_FC0_SUBTYPE_AUTH, seq + 1);
		if (ifp->if_flags & IFF_DEBUG)
			printf("%s: station %s %s authenticated (open)\n",
			    ifp->if_xname,
			    ether_sprintf((u_int8_t *)ni->ni_macaddr),
			    ni->ni_state != IEEE80211_STA_CACHE ?
			    "newly" : "already");
		ieee80211_node_newstate(ni, IEEE80211_STA_AUTH);
		break;

	case IEEE80211_M_STA:
		if (ic->ic_state != IEEE80211_S_AUTH ||
		    seq != IEEE80211_AUTH_OPEN_RESPONSE) {
			ic->ic_stats.is_rx_bad_auth++;
			IEEE80211_DPRINTF(("%s: discard auth from %s; "
			    "state %u, seq %u\n", __func__,
			    ether_sprintf((u_int8_t *)wh->i_addr2),
			    ic->ic_state, seq));
			return;
		}
		if (status != 0) {
			if (ifp->if_flags & IFF_DEBUG)
				printf("%s: open authentication failed "
				    "(reason %d) for %s\n", ifp->if_xname,
				    status,
				    ether_sprintf((u_int8_t *)wh->i_addr3));
			if (ni != ic->ic_bss)
				ni->ni_fails++;
			ic->ic_stats.is_rx_auth_fail++;
			return;
		}
		ieee80211_new_state(ic, IEEE80211_S_ASSOC,
		    wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK);
		break;
	case IEEE80211_M_MONITOR:
		break;
	}
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
	if (memcmp(selector, MICROSOFT_OUI, 3) == 0 ||	/* WPA1 */
	    memcmp(selector, IEEE80211_OUI, 3) == 0) {	/* RSN (aka WPA2) */
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
	if (memcmp(selector, MICROSOFT_OUI, 3) == 0 ||	/* WPA1 */
	    memcmp(selector, IEEE80211_OUI, 3) == 0) {	/* RSN (aka WPA2) */
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
ieee80211_parse_rsn_body(struct ieee80211com *ic, struct ieee80211_node *ni,
    const u_int8_t *frm, u_int len)
{
	const u_int8_t *efrm;
	u_int16_t m, n, s;
	u_int16_t rsncaps;
	enum ieee80211_cipher group_cipher;
	u_int akmset, pairwise_cipherset;

	efrm = frm + len;

	/* check Version field */
	if (LE_READ_2(frm) != 1)
		return IEEE80211_REASON_RSN_IE_VER_UNSUP;
	frm += 2;

	/* all fields after the Version field are optional */

	/* if Cipher Suite missing, default to CCMP */
	ni->ni_group_cipher = IEEE80211_CIPHER_CCMP;
	ni->ni_pairwise_cipherset = IEEE80211_CIPHER_CCMP;
	/* if AKM Suite missing, default to 802.1X */
	ni->ni_akmset = IEEE80211_AKM_IEEE8021X;

	/* read Group Cipher Suite field */
	if (frm + 4 > efrm)
		return 0;
	group_cipher = ieee80211_parse_rsn_cipher(frm);
	if (group_cipher == IEEE80211_CIPHER_USEGROUP)
		return IEEE80211_REASON_BAD_GROUP_CIPHER;
	frm += 4;

	/* read Pairwise Cipher Suite Count field */
	if (frm + 2 > efrm)
		return 0;
	m = LE_READ_2(frm);
	frm += 2;

	/* read Pairwise Cipher Suite List */
	if (frm + m * 4 > efrm)
		return IEEE80211_REASON_IE_INVALID;
	pairwise_cipherset = IEEE80211_CIPHER_NONE;
	while (m-- > 0) {
		pairwise_cipherset |= ieee80211_parse_rsn_cipher(frm);
		frm += 4;
	}
	if (pairwise_cipherset & IEEE80211_CIPHER_USEGROUP) {
		if (pairwise_cipherset != IEEE80211_CIPHER_USEGROUP)
			return IEEE80211_REASON_BAD_PAIRWISE_CIPHER;
		if (group_cipher == IEEE80211_CIPHER_CCMP)
			return IEEE80211_REASON_BAD_PAIRWISE_CIPHER;
	}

	/* read AKM Suite List Count field */
	if (frm + 2 > efrm)
		return 0;
	n = LE_READ_2(frm);
	frm += 2;

	/* read AKM Suite List */
	if (frm + n * 4 > efrm)
		return IEEE80211_REASON_IE_INVALID;
	akmset = IEEE80211_AKM_NONE;
	while (n-- > 0) {
		akmset |= ieee80211_parse_rsn_akm(frm);
		frm += 4;
	}

	/* read RSN Capabilities field */
	if (frm + 2 > efrm)
		return 0;
	rsncaps = LE_READ_2(frm);
	frm += 2;

	/* read PMKID Count field */
	if (frm + 2 > efrm)
		return 0;
	s = LE_READ_2(frm);
	frm += 2;

	/* read PMKID List */
	if (frm + s * IEEE80211_PMKID_LEN > efrm)
		return IEEE80211_REASON_IE_INVALID;
	while (s-- > 0) {
		/* ignore PMKIDs for now */
		frm += IEEE80211_PMKID_LEN;
	}

	ni->ni_group_cipher = group_cipher;
	ni->ni_pairwise_cipherset = pairwise_cipherset;
	ni->ni_akmset = akmset;
	ni->ni_rsncaps = rsncaps;

	return 0;
}

int
ieee80211_parse_rsn(struct ieee80211com *ic, struct ieee80211_node *ni,
    const u_int8_t *frm)
{
	/* check IE length */
	if (frm[1] < 2) {
		IEEE80211_DPRINTF(("%s: invalid RSN/WPA2 IE;"
		    " length %u, expecting at least 2\n", __func__, frm[1]));
		ic->ic_stats.is_rx_elem_toosmall++;
		return IEEE80211_REASON_IE_INVALID;
	}
	return ieee80211_parse_rsn_body(ic, ni, frm + 2, frm[1] - 2);
}

int
ieee80211_parse_wpa1(struct ieee80211com *ic, struct ieee80211_node *ni,
    const u_int8_t *frm)
{
	/* check IE length */
	if (frm[1] < 6) {
		IEEE80211_DPRINTF(("%s: invalid WPA1 IE;"
		    " length %u, expecting at least 6\n", __func__, frm[1]));
		ic->ic_stats.is_rx_elem_toosmall++;
		return IEEE80211_REASON_IE_INVALID;
	}
	return ieee80211_parse_rsn_body(ic, ni, frm + 6, frm[1] - 4);
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
    struct ieee80211_node *ni, int rssi, u_int32_t rstamp)
{
#define	ISPROBE(_wh)	(((_wh)->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) == \
	IEEE80211_FC0_SUBTYPE_PROBE_RESP)

	const struct ieee80211_frame *wh;
	const u_int8_t *frm, *efrm;
	const u_int8_t *tstamp, *ssid, *rates, *xrates, *edca, *wmm;
	const u_int8_t *rsn, *wpa;
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

	IEEE80211_VERIFY_LENGTH(efrm - frm, 12);
	tstamp  = frm; frm += 8;
	bintval = LE_READ_2(frm); frm += 2;
	capinfo = LE_READ_2(frm); frm += 2;
	ssid = rates = xrates = edca = wmm = rsn = wpa = NULL;
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
			rsn = frm;
			break;
		case IEEE80211_ELEMID_EDCAPARMS:
			edca = frm;
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
					wpa = frm;
				else if (frm[1] >= 5 &&
				    frm[5] == 2 && frm[6] == 1)
					wmm = frm;
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
	IEEE80211_VERIFY_ELEMENT(rates, IEEE80211_RATE_MAXSIZE);
	IEEE80211_VERIFY_ELEMENT(ssid, IEEE80211_NWID_LEN);

	if (
#if IEEE80211_CHAN_MAX < 255
	    chan > IEEE80211_CHAN_MAX ||
#endif
	    isclr(ic->ic_chan_active, chan)) {
		IEEE80211_DPRINTF(("%s: ignore %s with invalid channel "
		    "%u\n", __func__, ISPROBE(wh) ?
		    "probe response" : "beacon", chan));
		ic->ic_stats.is_rx_badchan++;
		return;
	}
	if (!(ic->ic_caps & IEEE80211_C_SCANALL) &&
	    (chan != bchan && ic->ic_phytype != IEEE80211_T_FH)) {
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
		    "for channel %u\n", __func__, ISPROBE(wh) ?
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
		    ISPROBE(wh) ? "probe response" : "beacon",
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
	    ic->ic_state == IEEE80211_S_ASSOC &&
	    ni->ni_state == IEEE80211_STA_BSS) {
		/*
		 * Check if protection mode has changed since last
		 * beacon.
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
	if (ni->ni_flags & IEEE80211_NODE_QOS) {
		if (edca != NULL)
			ieee80211_parse_edca_params(ic, edca);
		else if (wmm != NULL)
			ieee80211_parse_wmm_params(ic, edca);
	}
	if (ssid[1] != 0 && ni->ni_esslen == 0) {
		/*
		 * Update ESSID at probe response to adopt hidden AP by
		 * Lucent/Cisco, which announces null ESSID in beacon.
		 */
		ni->ni_esslen = ssid[1];
		memset(ni->ni_essid, 0, sizeof(ni->ni_essid));
		memcpy(ni->ni_essid, ssid + 2, ssid[1]);
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
	if (ic->ic_opmode == IEEE80211_M_IBSS || (is_new && ISPROBE(wh))) {
		/*
		 * Fake an association so the driver can setup it's
		 * private state.  The rate set has been setup above;
		 * there is no handshake as in ap/station operation.
		 */
		if (ic->ic_newassoc)
			(*ic->ic_newassoc)(ic, ni, 1);
	}
#undef ISPROBE
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
	IEEE80211_VERIFY_ELEMENT(rates, IEEE80211_RATE_MAXSIZE);
	IEEE80211_VERIFY_ELEMENT(ssid, IEEE80211_NWID_LEN);
	IEEE80211_VERIFY_SSID(ic->ic_bss, ssid, "probe");
	if ((ic->ic_flags & IEEE80211_F_HIDENWID) && ssid[1] == 0) {
		IEEE80211_DPRINTF(("%s: no ssid "
		    "with ssid suppression enabled", __func__));
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
	} else {
		IEEE80211_SEND_MGMT(ic, ni,
			IEEE80211_FC0_SUBTYPE_PROBE_RESP, 0);
	}
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

	IEEE80211_VERIFY_LENGTH(efrm - frm, 6);
	algo   = LE_READ_2(frm); frm += 2;
	seq    = LE_READ_2(frm); frm += 2;
	status = LE_READ_2(frm); frm += 2;
	IEEE80211_DPRINTF(("%s: auth %d seq %d from %s\n",
	    __func__, algo, seq, ether_sprintf((u_int8_t *)wh->i_addr2)));

	if (algo == IEEE80211_AUTH_ALG_OPEN)
		ieee80211_auth_open(ic, wh, ni, rssi, rstamp, seq, status);
	else {
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
	}
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
    struct ieee80211_node *ni, int rssi, u_int32_t rstamp)
{
#define ISREASSOC(_wh)	(((_wh)->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) == \
	IEEE80211_FC0_SUBTYPE_REASSOC_REQ)

	const struct ieee80211_frame *wh;
	const u_int8_t *frm, *efrm;
	const u_int8_t *ssid, *rates, *xrates, *rsn, *wpa;
	u_int16_t capinfo, bintval;
	int reassoc, resp, reason = 0;
	u_int8_t rate;

	if (ic->ic_opmode != IEEE80211_M_HOSTAP ||
	    ic->ic_state != IEEE80211_S_RUN)
		return;

	wh = mtod(m0, struct ieee80211_frame *);
	frm = (const u_int8_t *)&wh[1];
	efrm = mtod(m0, u_int8_t *) + m0->m_len;

	if (ISREASSOC(wh)) {
		reassoc = 1;
		resp = IEEE80211_FC0_SUBTYPE_REASSOC_RESP;
	} else {
		reassoc = 0;
		resp = IEEE80211_FC0_SUBTYPE_ASSOC_RESP;
	}

	IEEE80211_VERIFY_LENGTH(efrm - frm, (reassoc ? 10 : 4));
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
	ssid = rates = xrates = rsn = wpa = NULL;
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
			rsn = frm;
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
					wpa = frm;
			}
			break;
		}
		frm += 2 + frm[1];
	}
	IEEE80211_VERIFY_ELEMENT(rates, IEEE80211_RATE_MAXSIZE);
	IEEE80211_VERIFY_ELEMENT(ssid, IEEE80211_NWID_LEN);
	IEEE80211_VERIFY_SSID(ic->ic_bss, ssid,
		reassoc ? "reassoc" : "assoc");
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
	if (rsn != NULL)
		reason = ieee80211_parse_rsn(ic, ni, rsn);
	else if (wpa != NULL)
		reason = ieee80211_parse_wpa1(ic, ni, wpa);
	if (reason != 0) {
		IEEE80211_DPRINTF(("%s: invalid RSN IE for %s\n",
		    __func__, ether_sprintf((u_int8_t *)wh->i_addr2)));
		IEEE80211_SEND_MGMT(ic, ni, IEEE80211_FC0_SUBTYPE_DEAUTH,
		    reason);
		ieee80211_node_leave(ic, ni);
		ic->ic_stats.is_rx_assoc_badrsnie++;
		return;
	}
	if (!(capinfo & IEEE80211_CAPINFO_ESS)) {
		IEEE80211_DPRINTF(("%s: capinfo mismatch for %s\n",
		    __func__, ether_sprintf((u_int8_t *)wh->i_addr2)));
		IEEE80211_SEND_MGMT(ic, ni, resp, IEEE80211_STATUS_CAPINFO);
		ieee80211_node_leave(ic, ni);
		ic->ic_stats.is_rx_assoc_capmismatch++;
		return;
	}
	rate = ieee80211_setup_rates(ic, ni, rates, xrates,
	    IEEE80211_F_DOSORT | IEEE80211_F_DOFRATE | IEEE80211_F_DONEGO |
	    IEEE80211_F_DODEL);
	if (rate & IEEE80211_RATE_BASIC) {
		IEEE80211_DPRINTF(("%s: rate mismatch for %s\n",
		    __func__, ether_sprintf((u_int8_t *)wh->i_addr2)));
		IEEE80211_SEND_MGMT(ic, ni, resp, IEEE80211_STATUS_BASIC_RATE);
		ieee80211_node_leave(ic, ni);
		ic->ic_stats.is_rx_assoc_norate++;
		return;
	}
	ni->ni_rssi = rssi;
	ni->ni_rstamp = rstamp;
	ni->ni_intval = bintval;
	ni->ni_capinfo = capinfo;
	ni->ni_chan = ic->ic_bss->ni_chan;
	ni->ni_fhdwell = ic->ic_bss->ni_fhdwell;
	ni->ni_fhindex = ic->ic_bss->ni_fhindex;

	ieee80211_node_join(ic, ni, resp);
#undef ISREASSOC
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
    struct ieee80211_node *ni, int rssi, u_int32_t rstamp)
{
#define ISREASSOC(_wh)	(((_wh)->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) == \
	IEEE80211_FC0_SUBTYPE_REASSOC_RESP)

	struct ifnet *ifp = &ic->ic_if;
	const struct ieee80211_frame *wh;
	const u_int8_t *frm, *efrm;
	const u_int8_t *rates, *xrates, *edca, *wmm;
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

	IEEE80211_VERIFY_LENGTH(efrm - frm, 6);
	ni = ic->ic_bss;
	capinfo = LE_READ_2(frm); frm += 2;
	status =  LE_READ_2(frm); frm += 2;
	if (status != 0) {
		if (ifp->if_flags & IFF_DEBUG)
			printf("%s: %sassociation failed (reason %d)"
			    " for %s\n", ifp->if_xname,
			    ISREASSOC(wh) ?  "re" : "",
			    status, ether_sprintf((u_int8_t *)wh->i_addr3));
		if (ni != ic->ic_bss)
			ni->ni_fails++;
		ic->ic_stats.is_rx_auth_fail++;
		return;
	}
	associd = LE_READ_2(frm); frm += 2;

	rates = xrates = edca = wmm = NULL;
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
			edca = frm;
			break;
		case IEEE80211_ELEMID_VENDOR:
			if (frm[1] < 4) {
				ic->ic_stats.is_rx_elem_toosmall++;
				break;
			}
			if (memcmp(frm + 2, MICROSOFT_OUI, 3) == 0) {
				if (frm[1] >= 5 && frm[5] == 2 && frm[6] == 1)
					wmm = frm;
			}
			break;
		}
		frm += 2 + frm[1];
	}

	IEEE80211_VERIFY_ELEMENT(rates, IEEE80211_RATE_MAXSIZE);
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
	if (edca != NULL || wmm != NULL) {
		/* force update of EDCA parameters */
		ic->ic_edca_updtcount = -1;

		if ((edca != NULL &&
		     ieee80211_parse_edca_params(ic, edca) == 0) ||
		    (wmm != NULL &&
		     ieee80211_parse_wmm_params(ic, wmm) == 0))
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

	ieee80211_new_state(ic, IEEE80211_S_RUN,
		wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK);
#undef ISREASSOC
}

/*-
 * Deauthentication frame format:
 * [2] Reason code
 */
void
ieee80211_recv_deauth(struct ieee80211com *ic, struct mbuf *m0,
    struct ieee80211_node *ni, int rssi, u_int32_t rstamp)
{
	struct ifnet *ifp = &ic->ic_if;
	const struct ieee80211_frame *wh;
	const u_int8_t *frm, *efrm;
	u_int16_t reason;

	wh = mtod(m0, struct ieee80211_frame *);
	frm = (const u_int8_t *)&wh[1];
	efrm = mtod(m0, u_int8_t *) + m0->m_len;

	IEEE80211_VERIFY_LENGTH(efrm - frm, 2);
	reason = LE_READ_2(frm);
	ic->ic_stats.is_rx_deauth++;
	switch (ic->ic_opmode) {
	case IEEE80211_M_STA:
		ieee80211_new_state(ic, IEEE80211_S_AUTH,
		    wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK);
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
    struct ieee80211_node *ni, int rssi, u_int32_t rstamp)
{
	struct ifnet *ifp = &ic->ic_if;
	const struct ieee80211_frame *wh;
	const u_int8_t *frm, *efrm;
	u_int16_t reason;

	wh = mtod(m0, struct ieee80211_frame *);
	frm = (const u_int8_t *)&wh[1];
	efrm = mtod(m0, u_int8_t *) + m0->m_len;

	IEEE80211_VERIFY_LENGTH(efrm - frm, 2);
	reason = LE_READ_2(frm);
	ic->ic_stats.is_rx_disassoc++;
	switch (ic->ic_opmode) {
	case IEEE80211_M_STA:
		ieee80211_new_state(ic, IEEE80211_S_ASSOC,
		    wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK);
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

void
ieee80211_recv_mgmt(struct ieee80211com *ic, struct mbuf *m0,
    struct ieee80211_node *ni, int subtype, int rssi, u_int32_t rstamp)
{
	switch (subtype) {
	case IEEE80211_FC0_SUBTYPE_PROBE_RESP:
	case IEEE80211_FC0_SUBTYPE_BEACON:
		ieee80211_recv_probe_resp(ic, m0, ni, rssi, rstamp);
		break;
	case IEEE80211_FC0_SUBTYPE_PROBE_REQ:
		ieee80211_recv_probe_req(ic, m0, ni, rssi, rstamp);
		break;
	case IEEE80211_FC0_SUBTYPE_AUTH:
		ieee80211_recv_auth(ic, m0, ni, rssi, rstamp);
		break;
	case IEEE80211_FC0_SUBTYPE_ASSOC_REQ:
	case IEEE80211_FC0_SUBTYPE_REASSOC_REQ:
		ieee80211_recv_assoc_req(ic, m0, ni, rssi, rstamp);
		break;
	case IEEE80211_FC0_SUBTYPE_ASSOC_RESP:
	case IEEE80211_FC0_SUBTYPE_REASSOC_RESP:
		ieee80211_recv_assoc_resp(ic, m0, ni, rssi, rstamp);
		break;
	case IEEE80211_FC0_SUBTYPE_DEAUTH:
		ieee80211_recv_deauth(ic, m0, ni, rssi, rstamp);
		break;
	case IEEE80211_FC0_SUBTYPE_DISASSOC:
		ieee80211_recv_disassoc(ic, m0, ni, rssi, rstamp);
		break;
	default:
		IEEE80211_DPRINTF(("%s: mgmt frame with subtype 0x%x not "
		    "handled\n", __func__, subtype));
		ic->ic_stats.is_rx_badsubtype++;
		break;
	}
}
#undef IEEE80211_VERIFY_LENGTH
#undef IEEE80211_VERIFY_ELEMENT
#undef IEEE80211_VERIFY_SSID

/* unaligned big endian access */
#define BE_READ_2(p)						\
	((u_int16_t)(p)[0] << 8 | (u_int16_t)(p)[1])

#define BE_READ_8(p)						\
	((u_int64_t)(p)[0] << 56 | (u_int64_t)(p)[1] << 48 |	\
	 (u_int64_t)(p)[2] << 40 | (u_int64_t)(p)[3] << 32 |	\
	 (u_int64_t)(p)[4] << 24 | (u_int64_t)(p)[5] << 16 |	\
	 (u_int64_t)(p)[6] <<  8 | (u_int64_t)(p)[7])

#define BE_WRITE_2(p, v) do {	\
	(p)[0] = (v) >> 8;	\
	(p)[1] = (v) & 0xff;	\
} while (0)

/* unaligned little endian access */
#define LE_READ_8(p)						\
	((u_int64_t)(p)[7] << 56 | (u_int64_t)(p)[6] << 48 |	\
	 (u_int64_t)(p)[5] << 40 | (u_int64_t)(p)[4] << 32 |	\
	 (u_int64_t)(p)[3] << 24 | (u_int64_t)(p)[2] << 16 |	\
	 (u_int64_t)(p)[1] <<  8 | (u_int64_t)(p)[0])

/*
 * 4-Way Handshake Message 1 is sent by the authenticator to the supplicant
 * (see 8.5.3.1).
 */
void
ieee80211_recv_4way_msg1(struct ieee80211com *ic,
    const struct ieee80211_eapol_key *key, struct ieee80211_node *ni)
{
	u_int8_t snonce[EAPOL_KEY_NONCE_LEN];
	const u_int8_t *frm, *efrm;
	const u_int8_t *pmkid;
	const u_int8_t *pmk;
	size_t pmk_len;

	if (ic->ic_opmode != IEEE80211_M_STA &&
	    ic->ic_opmode != IEEE80211_M_IBSS)
		return;

	if (ni->ni_replaycnt && BE_READ_8(key->replaycnt) <= ni->ni_replaycnt)
		return;

	/* save authenticator's nonce (ANonce) */
	memcpy(ni->ni_nonce, key->nonce, EAPOL_KEY_NONCE_LEN);

	/* parse key data field (shall contain an encapsulated PMKID) */
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
	/* check that the PMKID KDE is valid */
	if (pmkid != NULL && pmkid[1] - 4 < 16)
		return;

	/* update the last seen value of the key replay counter field */
	ni->ni_replaycnt = BE_READ_8(key->replaycnt);
	/* do not set ni_replaycnt_ok since the frame contains no MIC */

	/* generate a new nonce (SNonce) */
	get_random_bytes(snonce, EAPOL_KEY_NONCE_LEN);

	if (ni->ni_akm == IEEE80211_AKM_IEEE8021X) {
		/* XXX find the PMK in the PMKSA cache using the PMKID */
	} else {
		/* XXX the PMK is the PSK */
	}

	/* derive PTK from PMK */
	ieee80211_derive_ptk(pmk, pmk_len, ni->ni_macaddr, ic->ic_myaddr,
	    ni->ni_nonce, snonce, (u_int8_t *)&ni->ni_ptk, sizeof(ni->ni_ptk));
	ni->ni_ptk_ok = 1;

	if (ic->ic_if.if_flags & IFF_DEBUG)
		printf("%s: received msg %d/%d of the %s handshake from %s\n",
		    ic->ic_if.if_xname, 1, 4, "4-way",
		    ether_sprintf(ni->ni_macaddr));

	/* send message 2 to authenticator */
	ieee80211_send_4way_msg2(ic, ni);
}

/*
 * 4-Way Handshake Message 2 is sent by the supplicant to the authenticator
 * (see 8.5.3.2).
 */
void
ieee80211_recv_4way_msg2(struct ieee80211com *ic,
    const struct ieee80211_eapol_key *key, struct ieee80211_node *ni)
{
	const u_int8_t *frm, *efrm;
	const u_int8_t *rsn;
	const u_int8_t *pmk;
	size_t pmk_len;

	if (ic->ic_opmode != IEEE80211_M_HOSTAP &&
	    ic->ic_opmode != IEEE80211_M_IBSS)
		return;

	if (BE_READ_8(key->replaycnt) != ni->ni_replaycnt)
		return;

	/* derive PTK from PMK */
	ieee80211_derive_ptk(pmk, pmk_len, ic->ic_myaddr, ni->ni_macaddr,
	    ni->ni_nonce, key->nonce, (u_int8_t *)&ni->ni_ptk,
	    sizeof(ni->ni_ptk));
	ni->ni_ptk_ok = 1;

	/* parse key data field (shall contain an RSN IE) */
	frm = (const u_int8_t *)&key[1];
	efrm = frm + BE_READ_2(key->paylen);

	rsn = NULL;
	while (frm + 2 <= efrm) {
		if (frm + 2 + frm[1] > efrm)
			break;
		switch (frm[0]) {
		case IEEE80211_ELEMID_RSN:
			rsn = frm;
			break;
		}
		frm += 2 + frm[1];
	}
	if (rsn == NULL)
		return;

	/*
	 * The RSN IE must match bit-wise with what the STA included in its
	 * (Re)Association Request.
	 */
	if (rsn[1] != ni->ni_rsnie[1] ||
	    memcmp(rsn, ni->ni_rsnie, 2 + rsn[1]) != 0)
		return;

	if (ic->ic_if.if_flags & IFF_DEBUG)
		printf("%s: received msg %d/%d of the %s handshake from %s\n",
		    ic->ic_if.if_xname, 2, 4, "4-way",
		    ether_sprintf(ni->ni_macaddr));

	/* send message 3 to supplicant */
	ieee80211_send_4way_msg3(ic, ni);
}

/*
 * 4-Way Handshake Message 3 is sent by the authenticator to the supplicant
 * (see 8.5.3.3).
 */
void
ieee80211_recv_4way_msg3(struct ieee80211com *ic,
    const struct ieee80211_eapol_key *key, struct ieee80211_node *ni)
{
	struct ieee80211_key k;
	const u_int8_t *frm, *efrm;
	const u_int8_t *rsn1, *rsn2, *gtk;

	if (ic->ic_opmode != IEEE80211_M_STA &&
	    ic->ic_opmode != IEEE80211_M_IBSS)
		return;

	if (BE_READ_8(key->replaycnt) <= ni->ni_replaycnt)
		return;

	/* check that ANonce matches the one received in message 1 */
	if (memcmp(key->nonce, ni->ni_nonce, EAPOL_KEY_NONCE_LEN) != 0)
		return;

	/* parse key data field */
	frm = (const u_int8_t *)&key[1];
	efrm = frm + BE_READ_2(key->paylen);

	rsn1 = rsn2 = gtk = NULL;
	while (frm + 2 <= efrm) {
		if (frm + 2 + frm[1] > efrm)
			break;
		switch (frm[0]) {
		case IEEE80211_ELEMID_RSN:
			if (rsn1 == NULL)
				rsn1 = frm;
			else if (rsn2 == NULL)
				rsn2 = frm;
			/* ignore if more than two RSN IEs */
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
			}
			break;
		}
		frm += 2 + frm[1];
	}
	/* first RSN IE is mandatory */
	if (rsn1 == NULL)
		return;
	/* key data must be encrypted if GTK is included */
	if (gtk != NULL && !(BE_READ_2(key->info) & EAPOL_KEY_ENCRYPTED))
		return;

	/*
	 * Check that first RSN IE is identical to the one received in
	 * the beacon or probe response frame.
	 */
	if (rsn1[1] != ni->ni_rsnie[1] ||
	    memcmp(rsn1, ni->ni_rsnie, 2 + rsn1[1]) != 0)
		return;

	/* update the last seen value of the key replay counter field */
	ni->ni_replaycnt = BE_READ_8(key->replaycnt);
	ni->ni_replaycnt_ok = 1;

	/*
	 * If a second RSN information element is present, use its pairwise
	 * cipher suite or deauthenticate.
	 */
	if (rsn2 != NULL) {
		/* XXX ieee80211_parse_rsn(rsn2); */
	}

	/* install the PTK */
	memset(&k, 0, sizeof k);
	k.k_cipher = ni->ni_pairwise_cipher;
	k.k_flags = IEEE80211_KEY_TX;
	k.k_len = ieee80211_cipher_keylen(k.k_cipher);
	memcpy(k.k_key, ni->ni_ptk.tk, k.k_len);
	if ((*ic->ic_set_key)(ic, ni, &k) != 0)
		return;

	if (gtk != NULL) {
		/* check that the GTK KDE is valid */
		if (gtk[1] - 4 < 2)
			return;
		/* install the GTK */
		memset(&k, 0, sizeof k);
		k.k_id = gtk[6] & 3;
		k.k_cipher = ni->ni_group_cipher;
		k.k_flags = IEEE80211_KEY_GROUP;
		if (gtk[6] & (1 << 2))	/* Tx bit */
			k.k_flags |= IEEE80211_KEY_TX;
		k.k_len = gtk[1] - 6;
		/* check that key length matches group cipher */
		if (k.k_len != ieee80211_cipher_keylen(k.k_cipher))
			return;	/* XXX PTK already installed! */
		memcpy(k.k_key, &gtk[7], k.k_len);
		k.k_rsc = LE_READ_8(key->rsc);
		if ((*ic->ic_set_key)(ic, ni, &k) != 0)
			return;
	}

	/* mark the PAE port as valid */
	ni->ni_port_valid = 1;

	if (ic->ic_if.if_flags & IFF_DEBUG)
		printf("%s: received msg %d/%d of the %s handshake from %s\n",
		    ic->ic_if.if_xname, 3, 4, "4-way",
		    ether_sprintf(ni->ni_macaddr));

	/* send message 4 to authenticator */
	ieee80211_send_4way_msg4(ic, ni);
}

/*
 * 4-Way Handshake Message 4 is sent by the supplicant to the authenticator
 * (see 8.5.3.4).
 */
void
ieee80211_recv_4way_msg4(struct ieee80211com *ic,
    const struct ieee80211_eapol_key *key, struct ieee80211_node *ni)
{
	struct ieee80211_key k;

	if (ic->ic_opmode != IEEE80211_M_HOSTAP &&
	    ic->ic_opmode != IEEE80211_M_IBSS)
		return;

	if (BE_READ_8(key->replaycnt) != ni->ni_replaycnt)
		return;

	/* empty key data field */

	/* install the PTK */
	memset(&k, 0, sizeof k);
	k.k_cipher = ni->ni_pairwise_cipher;
	k.k_flags = IEEE80211_KEY_TX;
	k.k_len = ieee80211_cipher_keylen(k.k_cipher);
	memcpy(k.k_key, ni->ni_ptk.tk, k.k_len);
	if ((*ic->ic_set_key)(ic, ni, &k) != 0)
		return;

	if (ic->ic_opmode == IEEE80211_M_IBSS) {
		if (++ni->ni_key_count == 2)
			ni->ni_port_valid = 1;
	} else
		ni->ni_port_valid = 1;

	/* increment the 64-bit Key Replay Counter */
	ni->ni_replaycnt++;

	if (ic->ic_if.if_flags & IFF_DEBUG)
		printf("%s: received msg %d/%d of the %s handshake from %s\n",
		    ic->ic_if.if_xname, 4, 4, "4-way",
		    ether_sprintf(ni->ni_macaddr));
}

/*
 * Group Key Handshake Message 1 is sent by the authenticator to the
 * supplicant (see 8.5.4.1).
 */
void
ieee80211_recv_group_msg1(struct ieee80211com *ic,
    const struct ieee80211_eapol_key *key, struct ieee80211_node *ni)
{
	struct ieee80211_key k;
	const u_int8_t *frm, *efrm;
	const u_int8_t *gtk;

	if (ic->ic_opmode != IEEE80211_M_STA &&
	    ic->ic_opmode != IEEE80211_M_IBSS)
		return;

	if (BE_READ_8(key->replaycnt) <= ni->ni_replaycnt)
		return;

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
	if (gtk == NULL || !(BE_READ_2(key->info) & EAPOL_KEY_ENCRYPTED))
		return;

	/* check that the GTK KDE is valid */
	if (gtk[1] - 4 < 2)
		return;
	/* install the GTK */
	memset(&k, 0, sizeof k);
	k.k_id = gtk[6] & 3;
	k.k_cipher = ni->ni_group_cipher;
	k.k_flags = IEEE80211_KEY_GROUP;
	if (gtk[6] & (1 << 2))	/* Tx bit */
		k.k_flags |= IEEE80211_KEY_TX;
	k.k_len = gtk[1] - 6;
	/* check that key length matches group cipher */
	if (k.k_len != ieee80211_cipher_keylen(k.k_cipher))
		return;
	memcpy(k.k_key, &gtk[7], k.k_len);
	k.k_rsc = LE_READ_8(key->rsc);
	if ((*ic->ic_set_key)(ic, ni, &k) != 0)
		return;

	/* update the last seen value of the key replay counter field */
	ni->ni_replaycnt = BE_READ_8(key->replaycnt);

	if (ic->ic_if.if_flags & IFF_DEBUG)
		printf("%s: received msg %d/%d of the %s handshake from %s\n",
		    ic->ic_if.if_xname, 1, 2, "group key",
		    ether_sprintf(ni->ni_macaddr));

	/* send message 2 to authenticator */
	ieee80211_send_group_msg2(ic, ni);
}

/*
 * Group Key Handshake Message 2 is sent by the supplicant to the
 * authenticator (see 8.5.4.2).
 */
void
ieee80211_recv_group_msg2(struct ieee80211com *ic,
    const struct ieee80211_eapol_key *key, struct ieee80211_node *ni)
{
	if (ic->ic_opmode != IEEE80211_M_HOSTAP &&
	    ic->ic_opmode != IEEE80211_M_IBSS)
		return;

	if (BE_READ_8(key->replaycnt) != ni->ni_replaycnt)
		return;

	/* empty key data field */

#ifdef notyet
	if (--ic->ic_keydone_sta == 0) {
		/* install GTK */
	}
#endif
	if (ic->ic_if.if_flags & IFF_DEBUG)
		printf("%s: received msg %d/%d of the %s handshake from %s\n",
		    ic->ic_if.if_xname, 2, 2, "group key",
		    ether_sprintf(ni->ni_macaddr));
}

/*
 * EAPOL-Key Request frames are sent by the supplicant to request that the
 * authenticator initiate either a 4-Way Handshake or Group Key Handshake
 * and to report a MIC failure in a TKIP MSDU.
 */
void
ieee80211_recv_eapol_key_req(struct ieee80211com *ic,
    const struct ieee80211_eapol_key *key, struct ieee80211_node *ni)
{
	u_int16_t info;

	if (ic->ic_opmode != IEEE80211_M_HOSTAP &&
	    ic->ic_opmode != IEEE80211_M_IBSS)
		return;

	info = BE_READ_2(key->info);

	if (info & EAPOL_KEY_ERROR) {
		/* TKIP MIC failure */

	} else if (info & EAPOL_KEY_PAIRWISE) {
		/* initiate the 4-Way Handshake */

	} else {
		/*
		 * Should change the GTK, initiate the 4-Way Handshake and
		 * then execute a Group Key Handshake with all supplicants.
		 */
	}
}

#ifdef IEEE80211_DEBUG
static void
ieee80211_print_eapol_key(struct ieee80211com *ic,
    const struct ieee80211_eapol_key *key, const struct ieee80211_node *ni)
{
	int i;
	printf("%s: received EAPOL-Key frame from %s\n",
	    ic->ic_if.if_xname, ether_sprintf((u_int8_t *)ni->ni_macaddr));
	printf("version=0x%02x type=0x%02x desc=0x%02x body length=%d "
	    "data length=%d\n", key->version, key->type, key->desc,
	    BE_READ_2(key->len), BE_READ_2(key->paylen));
	printf("info=%b\n", BE_READ_2(key->info),
	    "\20\x03PAIRWISE\x06INSTALL\x07KEYACK\x08KEYMIC\x09SECURE"
	    "\x0aERROR\x0bREQUEST\x0cENCRYPTED\x0dSMK");
	printf("Key Replay Counter=0x");
	for (i = 0; i < 8; i++)
		printf("%02x", key->replaycnt[i]);
	printf("\n");
	printf("Key Nonce=0x");
	for (i = 0; i < EAPOL_KEY_NONCE_LEN; i++)
		printf("%02x", key->nonce[i]);
	printf("\n");
	printf("EAPOL-Key IV=0x");
	for (i = 0; i < EAPOL_KEY_IV_LEN; i++)
		printf("%02x", key->iv[i]);
	printf("\n");
	printf("Key RSC=0x");
	for (i = 0; i < 8; i++)
		printf("%02x", key->replaycnt[i]);
	printf("\n");
	printf("Key MIC=0x");
	for (i = 0; i < EAPOL_KEY_MIC_LEN; i++)
		printf("%02x", key->mic[i]);
	printf("\n");
}
#endif

/*
 * Process an incoming EAPOL frame.  Notice that we are only interested in
 * EAPOL-Key frames with an IEEE 802.11 descriptor type.
 */
void
ieee80211_recv_eapol(struct ieee80211com *ic, struct mbuf *m0,
    struct ieee80211_node *ni)
{
	struct ieee80211_eapol_key *key;
	u_int16_t info;

	if (m0->m_len < sizeof(*key))
		goto out;

	key = mtod(m0, struct ieee80211_eapol_key *);

	if (key->type != EAPOL_KEY ||
	    (key->desc != EAPOL_KEY_DESC_IEEE80211 &&
	     key->desc != EAPOL_KEY_DESC_WPA1))
		goto out;

	/* check packet body length */
	if (m0->m_len < 4 + BE_READ_2(key->len))
		goto out;

	/* check key data length */
	if (m0->m_len < sizeof(*key) + BE_READ_2(key->paylen))
		goto out;

#ifdef IEEE80211_DEBUG
	if (ieee80211_debug > 0)
		ieee80211_print_eapol_key(ic, key, ni);
#endif
	info = BE_READ_2(key->info);

	if (info & (EAPOL_KEY_KEYMIC | EAPOL_KEY_ENCRYPTED)) {
		/* check that we have a valid PTK or TPTK */
		if (!ni->ni_ptk_ok)
			goto out;

		if (ni->ni_pairwise_cipher == IEEE80211_CIPHER_CCMP &&
		    (info & EAPOL_KEY_VERSION_MASK) != EAPOL_KEY_DESC_V2)
			goto out;

		/* check Key MIC field using KCK */
		if ((info & EAPOL_KEY_KEYMIC) &&
		    ieee80211_eapol_key_check_mic(key, ni->ni_ptk.kck) != 0)
			goto out;

		/* decrypt Key Data field using KEK */
		if ((info & EAPOL_KEY_ENCRYPTED) &&
		    ieee80211_eapol_key_decrypt(key, ni->ni_ptk.kek) != 0)
			goto out;
	}

	/* determine message type (see 8.5.3.7) */
	if (info & EAPOL_KEY_REQUEST) {
		/* EAPOL-Key Request */
		ieee80211_recv_eapol_key_req(ic, key, ni);

	} else if (info & EAPOL_KEY_PAIRWISE) {
		/* 4-Way Handshake */
		if (info & EAPOL_KEY_KEYMIC) {
			if (!(info & EAPOL_KEY_KEYACK)) {
				if (info & EAPOL_KEY_SECURE)
					ieee80211_recv_4way_msg4(ic, key, ni);
				else
					ieee80211_recv_4way_msg2(ic, key, ni);
			} else
				ieee80211_recv_4way_msg3(ic, key, ni);
		} else
			ieee80211_recv_4way_msg1(ic, key, ni);
	} else {
		/* Group Key Handshake */
		if (info & EAPOL_KEY_KEYACK)
			ieee80211_recv_group_msg1(ic, key, ni);
		else
			ieee80211_recv_group_msg2(ic, key, ni);
	}
 out:
	m_freem(m0);
}

void
ieee80211_recv_pspoll(struct ieee80211com *ic, struct mbuf *m0, int rssi,
    u_int32_t rstamp)
{
	struct ifnet *ifp = &ic->ic_if;
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni;
	struct mbuf *m;
	u_int16_t aid;

	if (ic->ic_set_tim == NULL)  /* no powersaving functionality */
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

int
ieee80211_do_slow_print(struct ieee80211com *ic, int *did_print)
{
	static const struct timeval merge_print_intvl = {
		.tv_sec = 1, .tv_usec = 0
	};
	if ((ic->ic_if.if_flags & IFF_LINK0) == 0)
		return 0;
	if (!*did_print && (ic->ic_if.if_flags & IFF_DEBUG) == 0 &&
	    !ratecheck(&ic->ic_last_merge_print, &merge_print_intvl))
		return 0;

	*did_print = 1;
	return 1;
}

/* ieee80211_ibss_merge helps merge 802.11 ad hoc networks.  The
 * convention, set by the Wireless Ethernet Compatibility Alliance
 * (WECA), is that an 802.11 station will change its BSSID to match
 * the "oldest" 802.11 ad hoc network, on the same channel, that
 * has the station's desired SSID.  The "oldest" 802.11 network
 * sends beacons with the greatest TSF timestamp.
 *
 * Return ENETRESET if the BSSID changed, 0 otherwise.
 *
 * XXX Perhaps we should compensate for the time that elapses
 * between the MAC receiving the beacon and the host processing it
 * in ieee80211_ibss_merge.
 */
int
ieee80211_ibss_merge(struct ieee80211com *ic, struct ieee80211_node *ni,
    u_int64_t local_tsft)
{
	u_int64_t beacon_tsft;
	int did_print = 0, sign;
	union {
		u_int64_t	word;
		u_int8_t	tstamp[8];
	} u;

	/* ensure alignment */
	(void)memcpy(&u, &ni->ni_tstamp[0], sizeof(u));
	beacon_tsft = letoh64(u.word);

	/* we are faster, let the other guy catch up */
	if (beacon_tsft < local_tsft)
		sign = -1;
	else
		sign = 1;

	if (IEEE80211_ADDR_EQ(ni->ni_bssid, ic->ic_bss->ni_bssid)) {
		if (!ieee80211_do_slow_print(ic, &did_print))
			return 0;
		printf("%s: tsft offset %s%llu\n", ic->ic_if.if_xname,
		    (sign < 0) ? "-" : "",
		    (sign < 0)
			? (local_tsft - beacon_tsft)
			: (beacon_tsft - local_tsft));
		return 0;
	}

	if (sign < 0)
		return 0;

	if (ieee80211_match_bss(ic, ni) != 0)
		return 0;

	if (ieee80211_do_slow_print(ic, &did_print)) {
		printf("%s: ieee80211_ibss_merge: bssid mismatch %s\n",
		    ic->ic_if.if_xname, ether_sprintf(ni->ni_bssid));
		printf("%s: my tsft %llu beacon tsft %llu\n",
		    ic->ic_if.if_xname, local_tsft, beacon_tsft);
		printf("%s: sync TSF with %s\n",
		    ic->ic_if.if_xname, ether_sprintf(ni->ni_macaddr));
	}

	ic->ic_flags &= ~IEEE80211_F_SIBSS;

	/* negotiate rates with new IBSS */
	ieee80211_fix_rate(ic, ni, IEEE80211_F_DOFRATE |
	    IEEE80211_F_DONEGO | IEEE80211_F_DODEL);
	if (ni->ni_rates.rs_nrates == 0) {
		if (ieee80211_do_slow_print(ic, &did_print)) {
			printf("%s: rates mismatch, BSSID %s\n",
			    ic->ic_if.if_xname, ether_sprintf(ni->ni_bssid));
		}
		return 0;
	}

	if (ieee80211_do_slow_print(ic, &did_print)) {
		printf("%s: sync BSSID %s -> ",
		    ic->ic_if.if_xname, ether_sprintf(ic->ic_bss->ni_bssid));
		printf("%s ", ether_sprintf(ni->ni_bssid));
		printf("(from %s)\n", ether_sprintf(ni->ni_macaddr));
	}

	ieee80211_node_newstate(ni, IEEE80211_STA_BSS);
	(*ic->ic_node_copy)(ic, ic->ic_bss, ni);

	return ENETRESET;
}
