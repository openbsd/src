/*	$OpenBSD: ieee80211_output.c,v 1.5 2005/02/17 18:28:05 reyk Exp $	*/
/*	$NetBSD: ieee80211_output.c,v 1.13 2004/05/31 11:02:55 dyoung Exp $	*/

/*-
 * Copyright (c) 2001 Atsushi Onoe
 * Copyright (c) 2002, 2003 Sam Leffler, Errno Consulting
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
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
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

#include <sys/cdefs.h>
#if defined(__FreeBSD__)
__FBSDID("$FreeBSD: src/sys/net80211/ieee80211_output.c,v 1.10 2004/04/02 23:25:39 sam Exp $");
#elif defined(__NetBSD__)
__KERNEL_RCSID(0, "$NetBSD: ieee80211_output.c,v 1.13 2004/05/31 11:02:55 dyoung Exp $");
#endif

#ifdef __NetBSD__
#include "opt_inet.h"
#endif

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/endian.h>
#include <sys/errno.h>
#ifdef __FreeBSD__
#include <sys/bus.h>
#endif
#include <sys/proc.h>
#include <sys/sysctl.h>

#ifdef __FreeBSD__
#include <machine/atomic.h>
#endif

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_arp.h>
#if defined(__FreeBSD__)
#include <net/ethernet.h>
#elif defined(__NetBSD__)
#include <net/if_ether.h>
#endif
#include <net/if_llc.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#ifdef INET
#include <netinet/in.h>
#if defined(__FreeBSD__) || defined(__OpenBSD__)
#include <netinet/if_ether.h>
#else
#include <net/if_ether.h>
#endif
#endif

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_compat.h>

/*
 * Send a management frame to the specified node.  The node pointer
 * must have a reference as the pointer will be passed to the driver
 * and potentially held for a long time.  If the frame is successfully
 * dispatched to the driver, then it is responsible for freeing the
 * reference (and potentially free'ing up any associated storage).
 */
static int
ieee80211_mgmt_output(struct ifnet *ifp, struct ieee80211_node *ni,
    struct mbuf *m, int type)
{
	struct ieee80211com *ic = (void *)ifp;
	struct ieee80211_frame *wh;

	IASSERT(ni != NULL, ("null node"));
	ni->ni_inact = 0;

	/*
	 * Yech, hack alert!  We want to pass the node down to the
	 * driver's start routine.  If we don't do so then the start
	 * routine must immediately look it up again and that can
	 * cause a lock order reversal if, for example, this frame
	 * is being sent because the station is being timedout and
	 * the frame being sent is a DEAUTH message.  We could stick
	 * this in an m_tag and tack that on to the mbuf.  However
	 * that's rather expensive to do for every frame so instead
	 * we stuff it in the rcvif field since outbound frames do
	 * not (presently) use this.
	 */
	M_PREPEND(m, sizeof(struct ieee80211_frame), M_DONTWAIT);
	if (m == NULL)
		return ENOMEM;
#ifdef __FreeBSD__
	KASSERT(m->m_pkthdr.rcvif == NULL, ("rcvif not null"));
#endif
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

	if ((m->m_flags & M_LINK0) != 0 && ni->ni_challenge != NULL) {
		m->m_flags &= ~M_LINK0;
		IEEE80211_DPRINTF(("%s: encrypting frame for %s\n", __func__,
		    ether_sprintf(wh->i_addr1)));
		wh->i_fc[1] |= IEEE80211_FC1_WEP;
	}

	if (ifp->if_flags & IFF_DEBUG) {
		/* avoid to print too many frames */
		if (ic->ic_opmode == IEEE80211_M_IBSS ||
#ifdef IEEE80211_DEBUG
		    ieee80211_debug > 1 ||
#endif
		    (type & IEEE80211_FC0_SUBTYPE_MASK) !=
		    IEEE80211_FC0_SUBTYPE_PROBE_RESP)
			if_printf(ifp, "sending %s to %s on channel %u\n",
			    ieee80211_mgt_subtype_name[
			    (type & IEEE80211_FC0_SUBTYPE_MASK)
			    >> IEEE80211_FC0_SUBTYPE_SHIFT],
			    ether_sprintf(ni->ni_macaddr),
			    ieee80211_chan2ieee(ic, ni->ni_chan));
	}

	IF_ENQUEUE(&ic->ic_mgtq, m);
	ifp->if_timer = 1;
	(*ifp->if_start)(ifp);
	return 0;
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
	ni->ni_inact = 0;

	m_adj(m, sizeof(struct ether_header) - sizeof(struct llc));
	llc = mtod(m, struct llc *);
	llc->llc_dsap = llc->llc_ssap = LLC_SNAP_LSAP;
	llc->llc_control = LLC_UI;
	llc->llc_snap.org_code[0] = 0;
	llc->llc_snap.org_code[1] = 0;
	llc->llc_snap.org_code[2] = 0;
	llc->llc_snap.ether_type = eh.ether_type;
	M_PREPEND(m, sizeof(struct ieee80211_frame), M_DONTWAIT);
	if (m == NULL) {
		ic->ic_stats.is_tx_nombuf++;
		goto bad;
	}
	wh = mtod(m, struct ieee80211_frame *);
	wh->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_DATA;
	*(u_int16_t *)&wh->i_dur[0] = 0;
	*(u_int16_t *)&wh->i_seq[0] =
	    htole16(ni->ni_txseq << IEEE80211_SEQ_SEQ_SHIFT);
	ni->ni_txseq++;
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
	ieee80211_release_node(ic, ni);
	*pni = NULL;
	return NULL;
}

/*
 * Arguments in:
 *
 * paylen:  payload length (no FCS, no WEP header)
 *
 * hdrlen:  header length
 *
 * rate:    MSDU speed, units 500kb/s
 *
 * flags:   IEEE80211_F_SHPREAMBLE (use short preamble),
 *          IEEE80211_F_SHSLOT (use short slot length)
 *
 * Arguments out:
 *
 * d:       802.11 Duration field for RTS,
 *          802.11 Duration field for data frame,
 *          PLCP Length for data frame,
 *          residual octets at end of data slot
 */
static int
ieee80211_compute_duration1(int len, int use_ack, uint32_t flags, int rate,
    struct ieee80211_duration *d)
{
	int pre, ctsrate;
	int ack, bitlen, data_dur, remainder;

	/* RTS reserves medium for SIFS | CTS | SIFS | (DATA) | SIFS | ACK
	 * DATA reserves medium for SIFS | ACK
	 *
	 * XXXMYC: no ACK on multicast/broadcast or control packets
	 */

	bitlen = len * 8;

	pre = IEEE80211_DUR_DS_SIFS;
	if ((flags & IEEE80211_F_SHPREAMBLE) != 0)
		pre += IEEE80211_DUR_DS_SHORT_PREAMBLE + IEEE80211_DUR_DS_FAST_PLCPHDR;
	else
		pre += IEEE80211_DUR_DS_LONG_PREAMBLE + IEEE80211_DUR_DS_SLOW_PLCPHDR;

	d->d_residue = 0;
	data_dur = (bitlen * 2) / rate;
	remainder = (bitlen * 2) % rate;
	if (remainder != 0) {
		d->d_residue = (rate - remainder) / 16;
		data_dur++;
	}

	switch (rate) {
	case 2:		/* 1 Mb/s */
	case 4:		/* 2 Mb/s */
		/* 1 - 2 Mb/s WLAN: send ACK/CTS at 1 Mb/s */
		ctsrate = 2;
		break;
	case 11:	/* 5.5 Mb/s */
	case 22:	/* 11  Mb/s */
	case 44:	/* 22  Mb/s */
		/* 5.5 - 11 Mb/s WLAN: send ACK/CTS at 2 Mb/s */
		ctsrate = 4;
		break;
	default:
		/* TBD */
		return -1;
	}

	d->d_plcp_len = data_dur;

	ack = (use_ack) ? pre + (IEEE80211_DUR_DS_SLOW_ACK * 2) / ctsrate : 0;

	d->d_rts_dur =
	    pre + (IEEE80211_DUR_DS_SLOW_CTS * 2) / ctsrate +
	    pre + data_dur +
	    ack;

	d->d_data_dur = ack;

	return 0;
}

/*
 * Arguments in:
 *
 * wh:      802.11 header
 *
 * len: packet length 
 *
 * rate:    MSDU speed, units 500kb/s
 *
 * fraglen: fragment length, set to maximum (or higher) for no
 *          fragmentation
 *
 * flags:   IEEE80211_F_WEPON (hardware adds WEP),
 *          IEEE80211_F_SHPREAMBLE (use short preamble),
 *          IEEE80211_F_SHSLOT (use short slot length)
 *
 * Arguments out:
 *
 * d0: 802.11 Duration fields (RTS/Data), PLCP Length, Service fields
 *     of first/only fragment
 *
 * dn: 802.11 Duration fields (RTS/Data), PLCP Length, Service fields
 *     of first/only fragment
 */
int
ieee80211_compute_duration(struct ieee80211_frame *wh, int len,
    uint32_t flags, int fraglen, int rate, struct ieee80211_duration *d0,
    struct ieee80211_duration *dn, int *npktp, int debug)
{
	int ack, rc;
	int firstlen, hdrlen, lastlen, lastlen0, npkt, overlen, paylen;

	if ((wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) == IEEE80211_FC1_DIR_DSTODS)
		hdrlen = sizeof(struct ieee80211_frame_addr4);
	else
		hdrlen = sizeof(struct ieee80211_frame);

	paylen = len - hdrlen;

	if ((flags & IEEE80211_F_WEPON) != 0)
		overlen = IEEE80211_WEP_TOTLEN + IEEE80211_CRC_LEN;
	else
		overlen = IEEE80211_CRC_LEN;

	npkt = paylen / fraglen;
	lastlen0 = paylen % fraglen;

	if (npkt == 0)			/* no fragments */
		lastlen = paylen + overlen;
	else if (lastlen0 != 0) {	/* a short "tail" fragment */
		lastlen = lastlen0 + overlen;
		npkt++;
	} else				/* full-length "tail" fragment */
		lastlen = fraglen + overlen;

	if (npktp != NULL)
		*npktp = npkt;

	if (npkt > 1)
		firstlen = fraglen + overlen;
	else
		firstlen = paylen + overlen;

	if (debug) {
		printf("%s: npkt %d firstlen %d lastlen0 %d lastlen %d "
		    "fraglen %d overlen %d len %d rate %d flags %08x\n",
		    __func__, npkt, firstlen, lastlen0, lastlen, fraglen,
		    overlen, len, rate, flags);
	}

	ack = !IEEE80211_IS_MULTICAST(wh->i_addr1) &&
	    (wh->i_fc[1] & IEEE80211_FC0_TYPE_MASK) != IEEE80211_FC0_TYPE_CTL;

	rc = ieee80211_compute_duration1(firstlen + hdrlen,
	    ack, flags, rate, d0);
	if (rc == -1)
		return rc;

	if (npkt <= 1) {
		*dn = *d0;
		return 0;
	}
	return ieee80211_compute_duration1(lastlen + hdrlen, ack, flags, rate,
	    dn);
}

/*
 * Add a supported rates element id to a frame.
 */
u_int8_t *
ieee80211_add_rates(u_int8_t *frm, const struct ieee80211_rateset *rs)
{
	int nrates;

	*frm++ = IEEE80211_ELEMID_RATES;
	nrates = rs->rs_nrates;
	if (nrates > IEEE80211_RATE_SIZE)
		nrates = IEEE80211_RATE_SIZE;
	*frm++ = nrates;
	memcpy(frm, rs->rs_rates, nrates);
	return frm + nrates;
}

/*
 * Add an extended supported rates element id to a frame.
 */
u_int8_t *
ieee80211_add_xrates(u_int8_t *frm, const struct ieee80211_rateset *rs)
{
	/*
	 * Add an extended supported rates element if operating in 11g mode.
	 */
	if (rs->rs_nrates > IEEE80211_RATE_SIZE) {
		int nrates = rs->rs_nrates - IEEE80211_RATE_SIZE;
		*frm++ = IEEE80211_ELEMID_XRATES;
		*frm++ = nrates;
		memcpy(frm, rs->rs_rates + IEEE80211_RATE_SIZE, nrates);
		frm += nrates;
	}
	return frm;
}

/*
 * Add an ssid elemet to a frame.
 */
static u_int8_t *
ieee80211_add_ssid(u_int8_t *frm, const u_int8_t *ssid, u_int len)
{
	*frm++ = IEEE80211_ELEMID_SSID;
	*frm++ = len;
	memcpy(frm, ssid, len);
	return frm + len;
}

static struct mbuf *
ieee80211_getmbuf(int flags, int type, u_int pktlen)
{
	struct mbuf *m;

	IASSERT(pktlen <= MCLBYTES, ("802.11 packet too large: %u", pktlen));
#ifdef __FreeBSD__
	if (pktlen <= MHLEN)
		MGETHDR(m, flags, type);
	else
		m = m_getcl(flags, type, M_PKTHDR);
#else
	MGETHDR(m, flags, type);
	if (m != NULL && pktlen > MHLEN)
		MCLGET(m, flags);
#endif
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
	u_int8_t *frm;
	enum ieee80211_phymode mode;
	u_int16_t capinfo;
	int has_challenge, is_shared_key, ret, timer;

	IASSERT(ni != NULL, ("null node"));

	/*
	 * Hold a reference on the node so it doesn't go away until after
	 * the xmit is complete all the way in the driver.  On error we
	 * will remove our reference.
	 */
	ieee80211_ref_node(ni);
	timer = 0;
	switch (type) {
	case IEEE80211_FC0_SUBTYPE_PROBE_REQ:
		/*
		 * prreq frame format
		 *	[tlv] ssid
		 *	[tlv] supported rates
		 *	[tlv] extended supported rates
		 */
		m = ieee80211_getmbuf(M_DONTWAIT, MT_DATA,
			 2 + ic->ic_des_esslen
		       + 2 + IEEE80211_RATE_SIZE
		       + 2 + (IEEE80211_RATE_MAXSIZE - IEEE80211_RATE_SIZE));
		if (m == NULL)
			senderr(ENOMEM, is_tx_nombuf);
		m->m_data += sizeof(struct ieee80211_frame);
		frm = mtod(m, u_int8_t *);
		frm = ieee80211_add_ssid(frm, ic->ic_des_essid, ic->ic_des_esslen);
		mode = ieee80211_chan2mode(ic, ni->ni_chan);
		frm = ieee80211_add_rates(frm, &ic->ic_sup_rates[mode]);
		frm = ieee80211_add_xrates(frm, &ic->ic_sup_rates[mode]);
		m->m_pkthdr.len = m->m_len = frm - mtod(m, u_int8_t *);

		timer = IEEE80211_TRANS_WAIT;
		break;

	case IEEE80211_FC0_SUBTYPE_PROBE_RESP:
		/*
		 * probe response frame format
		 *	[8] time stamp
		 *	[2] beacon interval
		 *	[2] cabability information
		 *	[tlv] ssid
		 *	[tlv] supported rates
		 *	[tlv] parameter set (FH/DS)
		 *	[tlv] parameter set (IBSS)
		 *	[tlv] extended supported rates
		 */
		m = ieee80211_getmbuf(M_DONTWAIT, MT_DATA,
			 8 + 2 + 2 + 2
		       + 2 + ni->ni_esslen
		       + 2 + IEEE80211_RATE_SIZE
		       + (ic->ic_phytype == IEEE80211_T_FH ? 7 : 3)
		       + 6
		       + 2 + (IEEE80211_RATE_MAXSIZE - IEEE80211_RATE_SIZE));
		if (m == NULL)
			senderr(ENOMEM, is_tx_nombuf);
		m->m_data += sizeof(struct ieee80211_frame);
		frm = mtod(m, u_int8_t *);

		memset(frm, 0, 8);	/* timestamp should be filled later */
		frm += 8;
		*(u_int16_t *)frm = htole16(ic->ic_bss->ni_intval);
		frm += 2;
		if (ic->ic_opmode == IEEE80211_M_IBSS)
			capinfo = IEEE80211_CAPINFO_IBSS;
		else
			capinfo = IEEE80211_CAPINFO_ESS;
		if (ic->ic_flags & IEEE80211_F_WEPON)
			capinfo |= IEEE80211_CAPINFO_PRIVACY;
		if ((ic->ic_flags & IEEE80211_F_SHPREAMBLE) &&
		    IEEE80211_IS_CHAN_2GHZ(ni->ni_chan))
			capinfo |= IEEE80211_CAPINFO_SHORT_PREAMBLE;
		*(u_int16_t *)frm = htole16(capinfo);
		frm += 2;

		frm = ieee80211_add_ssid(frm, ic->ic_bss->ni_essid,
				ic->ic_bss->ni_esslen);
		frm = ieee80211_add_rates(frm, &ic->ic_bss->ni_rates);

		if (ic->ic_phytype == IEEE80211_T_FH) {
                        *frm++ = IEEE80211_ELEMID_FHPARMS;
                        *frm++ = 5;
                        *frm++ = ni->ni_fhdwell & 0x00ff;
                        *frm++ = (ni->ni_fhdwell >> 8) & 0x00ff;
                        *frm++ = IEEE80211_FH_CHANSET(
			    ieee80211_chan2ieee(ic, ni->ni_chan));
                        *frm++ = IEEE80211_FH_CHANPAT(
			    ieee80211_chan2ieee(ic, ni->ni_chan));
                        *frm++ = ni->ni_fhindex;
		} else {
			*frm++ = IEEE80211_ELEMID_DSPARMS;
			*frm++ = 1;
			*frm++ = ieee80211_chan2ieee(ic, ni->ni_chan);
		}

		if (ic->ic_opmode == IEEE80211_M_IBSS) {
			*frm++ = IEEE80211_ELEMID_IBSSPARMS;
			*frm++ = 2;
			*frm++ = 0; *frm++ = 0;		/* TODO: ATIM window */
		} else {	/* IEEE80211_M_HOSTAP */
			/* TODO: TIM */
			*frm++ = IEEE80211_ELEMID_TIM;
			*frm++ = 4;	/* length */
			*frm++ = 0;	/* DTIM count */
			*frm++ = 1;	/* DTIM period */
			*frm++ = 0;	/* bitmap control */
			*frm++ = 0;	/* Partial Virtual Bitmap (variable length) */
		}
		frm = ieee80211_add_xrates(frm, &ic->ic_bss->ni_rates);
		m->m_pkthdr.len = m->m_len = frm - mtod(m, u_int8_t *);
		break;

	case IEEE80211_FC0_SUBTYPE_AUTH:
		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if (m == NULL)
			senderr(ENOMEM, is_tx_nombuf);

		has_challenge = ((arg == IEEE80211_AUTH_SHARED_CHALLENGE ||
		    arg == IEEE80211_AUTH_SHARED_RESPONSE) &&
		    ni->ni_challenge != NULL);

		is_shared_key = has_challenge || (ni->ni_challenge != NULL &&
		    arg == IEEE80211_AUTH_SHARED_PASS);

		if (has_challenge) {
			MH_ALIGN(m, 2 * 3 + 2 + IEEE80211_CHALLENGE_LEN);
			m->m_pkthdr.len = m->m_len =
			    2 * 3 + 2 + IEEE80211_CHALLENGE_LEN;
		} else {
			MH_ALIGN(m, 2 * 3);
			m->m_pkthdr.len = m->m_len = 2 * 3;
		}
		frm = mtod(m, u_int8_t *);
		((u_int16_t *)frm)[0] =
		    (is_shared_key) ? htole16(IEEE80211_AUTH_ALG_SHARED)
		                    : htole16(IEEE80211_AUTH_ALG_OPEN);
		((u_int16_t *)frm)[1] = htole16(arg);	/* sequence number */
		((u_int16_t *)frm)[2] = 0;		/* status */

		if (has_challenge) {
			((u_int16_t *)frm)[3] =
			    htole16((IEEE80211_CHALLENGE_LEN << 8) |
			    IEEE80211_ELEMID_CHALLENGE);
			memcpy(&((u_int16_t *)frm)[4], ni->ni_challenge,
			    IEEE80211_CHALLENGE_LEN);
			if (arg == IEEE80211_AUTH_SHARED_RESPONSE) {
				IEEE80211_DPRINTF((
				    "%s: request encrypt frame\n", __func__));
				m->m_flags |= M_LINK0; /* WEP-encrypt, please */
			}
		}
		if (ic->ic_opmode == IEEE80211_M_STA)
			timer = IEEE80211_TRANS_WAIT;
		break;

	case IEEE80211_FC0_SUBTYPE_DEAUTH:
		if (ifp->if_flags & IFF_DEBUG)
			if_printf(ifp, "station %s deauthenticate (reason %d)\n",
			    ether_sprintf(ni->ni_macaddr), arg);
		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if (m == NULL)
			senderr(ENOMEM, is_tx_nombuf);
		MH_ALIGN(m, 2);
		m->m_pkthdr.len = m->m_len = 2;
		*mtod(m, u_int16_t *) = htole16(arg);	/* reason */
		break;

	case IEEE80211_FC0_SUBTYPE_ASSOC_REQ:
	case IEEE80211_FC0_SUBTYPE_REASSOC_REQ:
		/*
		 * asreq frame format
		 *	[2] capability information
		 *	[2] listen interval
		 *	[6*] current AP address (reassoc only)
		 *	[tlv] ssid
		 *	[tlv] supported rates
		 *	[tlv] extended supported rates
		 */
		m = ieee80211_getmbuf(M_DONTWAIT, MT_DATA,
			 sizeof(capinfo)
		       + sizeof(u_int16_t)
		       + IEEE80211_ADDR_LEN
		       + 2 + ni->ni_esslen
		       + 2 + IEEE80211_RATE_SIZE
		       + 2 + (IEEE80211_RATE_MAXSIZE - IEEE80211_RATE_SIZE));
		if (m == NULL)
			senderr(ENOMEM, is_tx_nombuf);
		m->m_data += sizeof(struct ieee80211_frame);
		frm = mtod(m, u_int8_t *);

		capinfo = 0;
		if (ic->ic_opmode == IEEE80211_M_IBSS)
			capinfo |= IEEE80211_CAPINFO_IBSS;
		else		/* IEEE80211_M_STA */
			capinfo |= IEEE80211_CAPINFO_ESS;
		if (ic->ic_flags & IEEE80211_F_WEPON)
			capinfo |= IEEE80211_CAPINFO_PRIVACY;
		/*
		 * NB: Some 11a AP's reject the request when
		 *     short premable is set.
		 */
		if ((ic->ic_flags & IEEE80211_F_SHPREAMBLE) &&
		    IEEE80211_IS_CHAN_2GHZ(ni->ni_chan))
			capinfo |= IEEE80211_CAPINFO_SHORT_PREAMBLE;
		if (ic->ic_flags & IEEE80211_F_SHSLOT)
			capinfo |= IEEE80211_CAPINFO_SHORT_SLOTTIME;
		*(u_int16_t *)frm = htole16(capinfo);
		frm += 2;

		*(u_int16_t *)frm = htole16(ic->ic_lintval);
		frm += 2;

		if (type == IEEE80211_FC0_SUBTYPE_REASSOC_REQ) {
			IEEE80211_ADDR_COPY(frm, ic->ic_bss->ni_bssid);
			frm += IEEE80211_ADDR_LEN;
		}

		frm = ieee80211_add_ssid(frm, ni->ni_essid, ni->ni_esslen);
		frm = ieee80211_add_rates(frm, &ni->ni_rates);
		frm = ieee80211_add_xrates(frm, &ni->ni_rates);
		m->m_pkthdr.len = m->m_len = frm - mtod(m, u_int8_t *);

		timer = IEEE80211_TRANS_WAIT;
		break;

	case IEEE80211_FC0_SUBTYPE_ASSOC_RESP:
	case IEEE80211_FC0_SUBTYPE_REASSOC_RESP:
		/*
		 * asreq frame format
		 *	[2] capability information
		 *	[2] status
		 *	[2] association ID
		 *	[tlv] supported rates
		 *	[tlv] extended supported rates
		 */
		m = ieee80211_getmbuf(M_DONTWAIT, MT_DATA,
			 sizeof(capinfo)
		       + sizeof(u_int16_t)
		       + sizeof(u_int16_t)
		       + 2 + IEEE80211_RATE_SIZE
		       + 2 + (IEEE80211_RATE_MAXSIZE - IEEE80211_RATE_SIZE));
		if (m == NULL)
			senderr(ENOMEM, is_tx_nombuf);
		m->m_data += sizeof(struct ieee80211_frame);
		frm = mtod(m, u_int8_t *);

		capinfo = IEEE80211_CAPINFO_ESS;
		if (ic->ic_flags & IEEE80211_F_WEPON)
			capinfo |= IEEE80211_CAPINFO_PRIVACY;
		if ((ic->ic_flags & IEEE80211_F_SHPREAMBLE) &&
		    IEEE80211_IS_CHAN_2GHZ(ni->ni_chan))
			capinfo |= IEEE80211_CAPINFO_SHORT_PREAMBLE;
		*(u_int16_t *)frm = htole16(capinfo);
		frm += 2;

		*(u_int16_t *)frm = htole16(arg);	/* status */
		frm += 2;

		if (arg == IEEE80211_STATUS_SUCCESS)
			*(u_int16_t *)frm = htole16(ni->ni_associd);
		frm += 2;

		frm = ieee80211_add_rates(frm, &ni->ni_rates);
		frm = ieee80211_add_xrates(frm, &ni->ni_rates);
		m->m_pkthdr.len = m->m_len = frm - mtod(m, u_int8_t *);
		break;

	case IEEE80211_FC0_SUBTYPE_DISASSOC:
		if (ifp->if_flags & IFF_DEBUG)
			if_printf(ifp, "station %s disassociate (reason %d)\n",
			    ether_sprintf(ni->ni_macaddr), arg);
		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if (m == NULL)
			senderr(ENOMEM, is_tx_nombuf);
		MH_ALIGN(m, 2);
		m->m_pkthdr.len = m->m_len = 2;
		*mtod(m, u_int16_t *) = htole16(arg);	/* reason */
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

void
ieee80211_pwrsave(struct ieee80211com *ic, struct ieee80211_node *ni,
		  struct mbuf *m)
{
	/* Store the new packet on our queue, changing the TIM if necessary */

	if (IF_IS_EMPTY(&ni->ni_savedq)) {
		ic->ic_set_tim(ic, ni->ni_associd, 1);
	}
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
		/* Similar to ieee80211_mgmt_output, store the node in
		 * the rcvif field.
		 */
		IF_ENQUEUE(&ni->ni_savedq, m);
		m->m_pkthdr.rcvif = (void *)ni;
	}
}
