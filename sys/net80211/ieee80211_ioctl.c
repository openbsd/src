/*	$OpenBSD: ieee80211_ioctl.c,v 1.6 2005/02/17 18:28:05 reyk Exp $	*/
/*	$NetBSD: ieee80211_ioctl.c,v 1.15 2004/05/06 02:58:16 dyoung Exp $	*/

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
__FBSDID("$FreeBSD: src/sys/net80211/ieee80211_ioctl.c,v 1.13 2004/03/30 22:57:57 sam Exp $");
#elif defined(__NetBSD__)
__KERNEL_RCSID(0, "$NetBSD: ieee80211_ioctl.c,v 1.15 2004/05/06 02:58:16 dyoung Exp $");
#endif

/*
 * IEEE 802.11 ioctl support (FreeBSD-specific)
 */

#ifdef __NetBSD__
#include "opt_inet.h"
#endif

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/systm.h>
#include <sys/endian.h>
#include <sys/proc.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_media.h>
#if defined(__FreeBSD__)
#include <net/ethernet.h>
#elif defined(__NetBSD__)
#include <net/if_ether.h>
#endif

#ifdef INET
#include <netinet/in.h>
#if defined(__FreeBSD__) || defined(__OpenBSD__)
#include <netinet/if_ether.h>
#endif /* __FreeBSD__ */
#ifndef __OpenBSD__
#include <netinet/if_inarp.h>
#endif
#endif

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_ioctl.h>

#if defined(__FreeBSD__)
#include <dev/wi/if_wavelan_ieee.h>
#elif defined(__OpenBSD__)
#include <dev/ic/if_wi_ieee.h>
#include <dev/ic/if_wi_hostap.h>
#else
#include <dev/ic/wi_ieee.h>
#endif

#if defined(__OpenBSD__)
void	 node2sta(struct ieee80211com *, struct ieee80211_node *,
	    struct hostap_sta *);
#endif

/*
 * XXX
 * Wireless LAN specific configuration interface, which is compatible
 * with wicontrol(8).
 */

int
ieee80211_cfgget(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct ieee80211com *ic = (void *)ifp;
	int i, j, error;
	struct ifreq *ifr = (struct ifreq *)data;
	struct wi_req wreq;
	struct wi_ltv_keys *keys;
	struct wi_apinfo *ap;
	struct ieee80211_node *ni;
	struct ieee80211_rateset *rs;
#ifdef WICACHE
	struct wi_sigcache wsc;
#endif /* WICACHE */
	struct wi_scan_p2_hdr *p2;
	struct wi_scan_res *res;

	error = copyin(ifr->ifr_data, &wreq, sizeof(wreq));
	if (error)
		return error;
	wreq.wi_len = 0;
	switch (wreq.wi_type) {
	case WI_RID_SERIALNO:
	case WI_RID_STA_IDENTITY:
	case WI_RID_CARD_ID:
	case WI_RID_PROCFRAME:
	case WI_RID_SYMBOL_DIVERSITY:
	case WI_RID_ENH_SECURITY:
	case WI_RID_AUTH_STATION:
		/* nothing appropriate */
		break;
	case WI_RID_NODENAME:
		strlcpy((char *)&wreq.wi_val[1], hostname,
		    sizeof(wreq.wi_val) - sizeof(wreq.wi_val[0]));
		wreq.wi_val[0] = htole16(strlen(hostname));
		wreq.wi_len = (1 + strlen(hostname) + 1) / 2;
		break;
	case WI_RID_CURRENT_SSID:
		if (ic->ic_state != IEEE80211_S_RUN) {
			wreq.wi_val[0] = 0;
			wreq.wi_len = 1;
			break;
		}
		wreq.wi_val[0] = htole16(ic->ic_bss->ni_esslen);
		memcpy(&wreq.wi_val[1], ic->ic_bss->ni_essid,
		    ic->ic_bss->ni_esslen);
		wreq.wi_len = (1 + ic->ic_bss->ni_esslen + 1) / 2;
		break;
	case WI_RID_OWN_SSID:
	case WI_RID_DESIRED_SSID:
		wreq.wi_val[0] = htole16(ic->ic_des_esslen);
		memcpy(&wreq.wi_val[1], ic->ic_des_essid, ic->ic_des_esslen);
		wreq.wi_len = (1 + ic->ic_des_esslen + 1) / 2;
		break;
	case WI_RID_CURRENT_BSSID:
		if (ic->ic_state == IEEE80211_S_RUN)
			IEEE80211_ADDR_COPY(wreq.wi_val, ic->ic_bss->ni_bssid);
		else
			memset(wreq.wi_val, 0, IEEE80211_ADDR_LEN);
		wreq.wi_len = IEEE80211_ADDR_LEN / 2;
		break;
	case WI_RID_CHANNEL_LIST:
		memset(wreq.wi_val, 0, sizeof(wreq.wi_val));
		/*
		 * Since channel 0 is not available for DS, channel 1
		 * is assigned to LSB on WaveLAN.
		 */
		if (ic->ic_phytype == IEEE80211_T_DS)
			i = 1;
		else
			i = 0;
		for (j = 0; i <= IEEE80211_CHAN_MAX; i++, j++)
			if (isset(ic->ic_chan_active, i)) {
				setbit((u_int8_t *)wreq.wi_val, j);
				wreq.wi_len = j / 16 + 1;
			}
		break;
	case WI_RID_OWN_CHNL:
		wreq.wi_val[0] = htole16(
			ieee80211_chan2ieee(ic, ic->ic_ibss_chan));
		wreq.wi_len = 1;
		break;
	case WI_RID_CURRENT_CHAN:
		wreq.wi_val[0] = htole16(
			ieee80211_chan2ieee(ic, ic->ic_bss->ni_chan));
		wreq.wi_len = 1;
		break;
	case WI_RID_COMMS_QUALITY:
		wreq.wi_val[0] = 0;				/* quality */
		wreq.wi_val[1] =
			htole16((*ic->ic_node_getrssi)(ic, ic->ic_bss));
		wreq.wi_val[2] = 0;				/* noise */
		wreq.wi_len = 3;
		break;
	case WI_RID_PROMISC:
		wreq.wi_val[0] = htole16((ifp->if_flags & IFF_PROMISC) ? 1 : 0);
		wreq.wi_len = 1;
		break;
	case WI_RID_PORTTYPE:
		wreq.wi_val[0] = htole16(ic->ic_opmode);
		wreq.wi_len = 1;
		break;
	case WI_RID_MAC_NODE:
		IEEE80211_ADDR_COPY(wreq.wi_val, ic->ic_myaddr);
		wreq.wi_len = IEEE80211_ADDR_LEN / 2;
		break;
	case WI_RID_TX_RATE:
		if (ic->ic_fixed_rate == -1)
			wreq.wi_val[0] = 0;	/* auto */
		else
			wreq.wi_val[0] = htole16(
			    (ic->ic_sup_rates[ic->ic_curmode].rs_rates[ic->ic_fixed_rate] &
			    IEEE80211_RATE_VAL) / 2);
		wreq.wi_len = 1;
		break;
	case WI_RID_CUR_TX_RATE:
		wreq.wi_val[0] = htole16(
		    (ic->ic_bss->ni_rates.rs_rates[ic->ic_bss->ni_txrate] &
		    IEEE80211_RATE_VAL) / 2);
		wreq.wi_len = 1;
		break;
	case WI_RID_FRAG_THRESH:
		wreq.wi_val[0] = htole16(ic->ic_fragthreshold);
		wreq.wi_len = 1;
		break;
	case WI_RID_RTS_THRESH:
		wreq.wi_val[0] = htole16(ic->ic_rtsthreshold);
		wreq.wi_len = 1;
		break;
	case WI_RID_CREATE_IBSS:
		wreq.wi_val[0] =
		    htole16((ic->ic_flags & IEEE80211_F_IBSSON) ? 1 : 0);
		wreq.wi_len = 1;
		break;
	case WI_RID_MICROWAVE_OVEN:
		wreq.wi_val[0] = 0;	/* no ... not supported */
		wreq.wi_len = 1;
		break;
	case WI_RID_ROAMING_MODE:
		wreq.wi_val[0] = htole16(1);	/* enabled ... not supported */
		wreq.wi_len = 1;
		break;
	case WI_RID_SYSTEM_SCALE:
		wreq.wi_val[0] = htole16(1);	/* low density ... not supp */
		wreq.wi_len = 1;
		break;
	case WI_RID_PM_ENABLED:
		wreq.wi_val[0] =
		    htole16((ic->ic_flags & IEEE80211_F_PMGTON) ? 1 : 0);
		wreq.wi_len = 1;
		break;
	case WI_RID_MAX_SLEEP:
		wreq.wi_val[0] = htole16(ic->ic_lintval);
		wreq.wi_len = 1;
		break;
	case WI_RID_CUR_BEACON_INT:
		wreq.wi_val[0] = htole16(ic->ic_bss->ni_intval);
		wreq.wi_len = 1;
		break;
	case WI_RID_WEP_AVAIL:
		wreq.wi_val[0] =
		    htole16((ic->ic_caps & IEEE80211_C_WEP) ? 1 : 0);
		wreq.wi_len = 1;
		break;
	case WI_RID_CNFAUTHMODE:
		wreq.wi_val[0] = htole16(1);	/* TODO: open system only */
		wreq.wi_len = 1;
		break;
	case WI_RID_ENCRYPTION:
		wreq.wi_val[0] =
		    htole16((ic->ic_flags & IEEE80211_F_WEPON) ? 1 : 0);
		wreq.wi_len = 1;
		break;
	case WI_RID_TX_CRYPT_KEY:
		wreq.wi_val[0] = htole16(ic->ic_wep_txkey);
		wreq.wi_len = 1;
		break;
	case WI_RID_DEFLT_CRYPT_KEYS:
		keys = (struct wi_ltv_keys *)&wreq;
		memset(keys, 0, sizeof(*keys));
		/* do not show keys to non-root user */
		error = suser(curproc, 0);
		if (error) {
			error = 0;
			break;
		}
		for (i = 0; i < IEEE80211_WEP_NKID; i++) {
			keys->wi_keys[i].wi_keylen =
			    htole16(ic->ic_nw_keys[i].wk_len);
			memcpy(keys->wi_keys[i].wi_keydat,
			    ic->ic_nw_keys[i].wk_key, ic->ic_nw_keys[i].wk_len);
		}
		wreq.wi_len = sizeof(*keys) / 2;
		break;
	case WI_RID_MAX_DATALEN:
		wreq.wi_val[0] = htole16(IEEE80211_MAX_LEN);	/* TODO: frag */
		wreq.wi_len = 1;
		break;
	case WI_RID_DBM_ADJUST:
		/* not supported, we just pass rssi value from driver. */
		break;
	case WI_RID_IFACE_STATS:
		/* XXX: should be implemented in lower drivers */
		break;
	case WI_RID_READ_APS:
		if (ic->ic_opmode != IEEE80211_M_HOSTAP) {
			/*
			 * Don't return results until active scan completes.
			 */
			if (ic->ic_state == IEEE80211_S_SCAN &&
			    (ic->ic_flags & IEEE80211_F_ASCAN)) {
				error = EINPROGRESS;
				break;
			}
		}
		i = 0;
		ap = (void *)((char *)wreq.wi_val + sizeof(i));
		TAILQ_FOREACH(ni, &ic->ic_node, ni_list) {
			if ((caddr_t)(ap + 1) > (caddr_t)(&wreq + 1))
				break;
			memset(ap, 0, sizeof(*ap));
			if (ic->ic_opmode == IEEE80211_M_HOSTAP) {
				IEEE80211_ADDR_COPY(ap->bssid, ni->ni_macaddr);
				ap->namelen = ic->ic_des_esslen;
				if (ic->ic_des_esslen)
					memcpy(ap->name, ic->ic_des_essid,
					    ic->ic_des_esslen);
			} else {
				IEEE80211_ADDR_COPY(ap->bssid, ni->ni_bssid);
				ap->namelen = ni->ni_esslen;
				if (ni->ni_esslen)
					memcpy(ap->name, ni->ni_essid,
					    ni->ni_esslen);
			}
			ap->channel = ieee80211_chan2ieee(ic, ni->ni_chan);
			ap->signal = (*ic->ic_node_getrssi)(ic, ni);
			ap->capinfo = ni->ni_capinfo;
			ap->interval = ni->ni_intval;
			rs = &ni->ni_rates;
			for (j = 0; j < rs->rs_nrates; j++) {
				if (rs->rs_rates[j] & IEEE80211_RATE_BASIC) {
					ap->rate = (rs->rs_rates[j] &
					    IEEE80211_RATE_VAL) * 5; /* XXX */
				}
			}
			i++;
			ap++;
		}
		memcpy(wreq.wi_val, &i, sizeof(i));
		wreq.wi_len = (sizeof(int) + sizeof(*ap) * i) / 2;
		break;
	case WI_RID_PRISM2:
		wreq.wi_val[0] = 1;	/* XXX lie so SCAN_RES can give rates */
		wreq.wi_len = sizeof(u_int16_t) / 2;
		break;
	case WI_RID_SCAN_RES:			/* compatibility interface */
		if (ic->ic_opmode != IEEE80211_M_HOSTAP &&
		    ic->ic_state == IEEE80211_S_SCAN &&
		    (ic->ic_flags & IEEE80211_F_ASCAN)) {
			error = EINPROGRESS;
			break;
		}
		/* NB: we use the Prism2 format so we can return rate info */
		p2 = (struct wi_scan_p2_hdr *)wreq.wi_val;
		res = (void *)&p2[1];
		i = 0;
		TAILQ_FOREACH(ni, &ic->ic_node, ni_list) {
			if ((caddr_t)(res + 1) > (caddr_t)(&wreq + 1))
				break;
			res->wi_chan = ieee80211_chan2ieee(ic, ni->ni_chan);
			res->wi_noise = 0;
			res->wi_signal = (*ic->ic_node_getrssi)(ic, ni);
			IEEE80211_ADDR_COPY(res->wi_bssid, ni->ni_bssid);
			res->wi_interval = ni->ni_intval;
			res->wi_capinfo = ni->ni_capinfo;
			res->wi_ssid_len = ni->ni_esslen;
			memcpy(res->wi_ssid, ni->ni_essid, IEEE80211_NWID_LEN);
			/* NB: assumes wi_srates holds <= ni->ni_rates */
			memcpy(res->wi_srates, ni->ni_rates.rs_rates,
				sizeof(res->wi_srates));
			if (ni->ni_rates.rs_nrates < 10)
				res->wi_srates[ni->ni_rates.rs_nrates] = 0;
			res->wi_rate = ni->ni_rates.rs_rates[ni->ni_txrate];
			res->wi_rsvd = 0;
			res++, i++;
		}
		p2->wi_rsvd = 0;
		p2->wi_reason = i;
		wreq.wi_len = (sizeof(*p2) + sizeof(*res) * i) / 2;
		break;
#ifdef WICACHE
	case WI_RID_READ_CACHE:
		i = 0;
		TAILQ_FOREACH(ni, &ic->ic_node, ni_list) {
			if (i == (WI_MAX_DATALEN/sizeof(struct wi_sigcache))-1)
				break;
			IEEE80211_ADDR_COPY(wsc.macsrc, ni->ni_macaddr);
			memset(&wsc.ipsrc, 0, sizeof(wsc.ipsrc));
			wsc.signal = (*ic->ic_node_getrssi)(ic, ni);
			wsc.noise = 0;
			wsc.quality = 0;
			memcpy((caddr_t)wreq.wi_val + sizeof(wsc) * i,
			    &wsc, sizeof(wsc));
			i++;
		}
		wreq.wi_len = sizeof(wsc) * i / 2;
		break;
#endif /* WICACHE */
	case WI_RID_SCAN_APS:
		error = EINVAL;
		break;
	default:
		error = EINVAL;
		break;
	}
	if (error == 0) {
		wreq.wi_len++;
		error = copyout(&wreq, ifr->ifr_data, sizeof(wreq));
	}
	return error;
}

static int
findrate(struct ieee80211com *ic, enum ieee80211_phymode mode, int rate)
{
#define	IEEERATE(_ic,_m,_i) \
	((_ic)->ic_sup_rates[_m].rs_rates[_i] & IEEE80211_RATE_VAL)
	int i, nrates = ic->ic_sup_rates[mode].rs_nrates;
	for (i = 0; i < nrates; i++)
		if (IEEERATE(ic, mode, i) == rate)
			return i;
	return -1;
#undef IEEERATE
}

/*
 * Prepare to do a user-initiated scan for AP's.  If no
 * current/default channel is setup or the current channel
 * is invalid then pick the first available channel from
 * the active list as the place to start the scan.
 */
static int
ieee80211_setupscan(struct ieee80211com *ic)
{
	u_char *chanlist = ic->ic_chan_active;
	int i;

	if (ic->ic_ibss_chan == NULL ||
	    isclr(chanlist, ieee80211_chan2ieee(ic, ic->ic_ibss_chan))) {
		for (i = 0; i <= IEEE80211_CHAN_MAX; i++)
			if (isset(chanlist, i)) {
				ic->ic_ibss_chan = &ic->ic_channels[i];
				goto found;
			}
		return EINVAL;			/* no active channels */
found:
		;
	}
	if (ic->ic_bss->ni_chan == IEEE80211_CHAN_ANYC ||
	    isclr(chanlist, ieee80211_chan2ieee(ic, ic->ic_bss->ni_chan)))
		ic->ic_bss->ni_chan = ic->ic_ibss_chan;
	/*
	 * XXX don't permit a scan to be started unless we
	 * know the device is ready.  For the moment this means
	 * the device is marked up as this is the required to
	 * initialize the hardware.  It would be better to permit
	 * scanning prior to being up but that'll require some
	 * changes to the infrastructure.
	 */
	return (ic->ic_if.if_flags & IFF_UP) ? 0 : ENETRESET;
}

int
ieee80211_cfgset(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct ieee80211com *ic = (void *)ifp;
	int i, j, len, error, rate;
	struct ifreq *ifr = (struct ifreq *)data;
	struct wi_ltv_keys *keys;
	struct wi_req wreq;
	u_char chanlist[roundup(IEEE80211_CHAN_MAX, NBBY)];

	error = copyin(ifr->ifr_data, &wreq, sizeof(wreq));
	if (error)
		return error;
	len = wreq.wi_len ? (wreq.wi_len - 1) * 2 : 0;
	switch (wreq.wi_type) {
	case WI_RID_SERIALNO:
	case WI_RID_NODENAME:
		return EPERM;
	case WI_RID_CURRENT_SSID:
		return EPERM;
	case WI_RID_OWN_SSID:
	case WI_RID_DESIRED_SSID:
		if (letoh16(wreq.wi_val[0]) * 2 > len ||
		    letoh16(wreq.wi_val[0]) > IEEE80211_NWID_LEN) {
			error = ENOSPC;
			break;
		}
		memset(ic->ic_des_essid, 0, sizeof(ic->ic_des_essid));
		ic->ic_des_esslen = letoh16(wreq.wi_val[0]) * 2;
		memcpy(ic->ic_des_essid, &wreq.wi_val[1], ic->ic_des_esslen);
		error = ENETRESET;
		break;
	case WI_RID_CURRENT_BSSID:
		return EPERM;
	case WI_RID_OWN_CHNL:
		if (len != 2)
			return EINVAL;
		i = letoh16(wreq.wi_val[0]);
		if (i < 0 ||
		    i > IEEE80211_CHAN_MAX ||
		    isclr(ic->ic_chan_active, i))
			return EINVAL;
		ic->ic_ibss_chan = &ic->ic_channels[i];
		if (ic->ic_flags & IEEE80211_F_SIBSS)
			error = ENETRESET;
		break;
	case WI_RID_CURRENT_CHAN:
		return EPERM;
	case WI_RID_COMMS_QUALITY:
		return EPERM;
	case WI_RID_PROMISC:
		if (len != 2)
			return EINVAL;
		if (ifp->if_flags & IFF_PROMISC) {
			if (wreq.wi_val[0] == 0) {
				ifp->if_flags &= ~IFF_PROMISC;
				error = ENETRESET;
			}
		} else {
			if (wreq.wi_val[0] != 0) {
				ifp->if_flags |= IFF_PROMISC;
				error = ENETRESET;
			}
		}
		break;
	case WI_RID_PORTTYPE:
		if (len != 2)
			return EINVAL;
		switch (letoh16(wreq.wi_val[0])) {
		case IEEE80211_M_STA:
			break;
		case IEEE80211_M_IBSS:
			if (!(ic->ic_caps & IEEE80211_C_IBSS))
				return EINVAL;
			break;
		case IEEE80211_M_AHDEMO:
			if (ic->ic_phytype != IEEE80211_T_DS ||
			    !(ic->ic_caps & IEEE80211_C_AHDEMO))
				return EINVAL;
			break;
		case IEEE80211_M_HOSTAP:
			if (!(ic->ic_caps & IEEE80211_C_HOSTAP))
				return EINVAL;
			break;
		default:
			return EINVAL;
		}
		if (letoh16(wreq.wi_val[0]) != ic->ic_opmode) {
			ic->ic_opmode = letoh16(wreq.wi_val[0]);
			error = ENETRESET;
		}
		break;
#if 0
	case WI_RID_MAC_NODE:
		if (len != IEEE80211_ADDR_LEN)
			return EINVAL;
		IEEE80211_ADDR_COPY(LLADDR(ifp->if_sadl), wreq.wi_val);
		/* if_init will copy lladdr into ic_myaddr */
		error = ENETRESET;
		break;
#endif
	case WI_RID_TX_RATE:
		if (len != 2)
			return EINVAL;
		if (wreq.wi_val[0] == 0) {
			/* auto */
			ic->ic_fixed_rate = -1;
			break;
		}
		rate = 2 * letoh16(wreq.wi_val[0]);
		if (ic->ic_curmode == IEEE80211_MODE_AUTO) {
			/*
			 * In autoselect mode search for the rate.  We take
			 * the first instance which may not be right, but we
			 * are limited by the interface.  Note that we also
			 * lock the mode to insure the rate is meaningful
			 * when it is used.
			 */
			for (j = IEEE80211_MODE_11A;
			     j < IEEE80211_MODE_MAX; j++) {
				if ((ic->ic_modecaps & (1<<j)) == 0)
					continue;
				i = findrate(ic, j, rate);
				if (i != -1) {
					/* lock mode too */
					ic->ic_curmode = j;
					goto setrate;
				}
			}
		} else {
			i = findrate(ic, ic->ic_curmode, rate);
			if (i != -1)
				goto setrate;
		}
		return EINVAL;
	setrate:
		ic->ic_fixed_rate = i;
		error = ENETRESET;
		break;
	case WI_RID_CUR_TX_RATE:
		return EPERM;
	case WI_RID_FRAG_THRESH:
		if (len != 2)
			return EINVAL;
		ic->ic_fragthreshold = letoh16(wreq.wi_val[0]);
		error = ENETRESET;
		break;
	case WI_RID_RTS_THRESH:
		if (len != 2)
			return EINVAL;
		ic->ic_rtsthreshold = letoh16(wreq.wi_val[0]);
		error = ENETRESET;
		break;
	case WI_RID_CREATE_IBSS:
		if (len != 2)
			return EINVAL;
		if (wreq.wi_val[0] != 0) {
			if ((ic->ic_caps & IEEE80211_C_IBSS) == 0)
				return EINVAL;
			if ((ic->ic_flags & IEEE80211_F_IBSSON) == 0) {
				ic->ic_flags |= IEEE80211_F_IBSSON;
				if (ic->ic_opmode == IEEE80211_M_IBSS &&
				    ic->ic_state == IEEE80211_S_SCAN)
					error = ENETRESET;
			}
		} else {
			if (ic->ic_flags & IEEE80211_F_IBSSON) {
				ic->ic_flags &= ~IEEE80211_F_IBSSON;
				if (ic->ic_flags & IEEE80211_F_SIBSS) {
					ic->ic_flags &= ~IEEE80211_F_SIBSS;
					error = ENETRESET;
				}
			}
		}
		break;
	case WI_RID_MICROWAVE_OVEN:
		if (len != 2)
			return EINVAL;
		if (wreq.wi_val[0] != 0)
			return EINVAL;		/* not supported */
		break;
	case WI_RID_ROAMING_MODE:
		if (len != 2)
			return EINVAL;
		if (letoh16(wreq.wi_val[0]) != 1)
			return EINVAL;		/* not supported */
		break;
	case WI_RID_SYSTEM_SCALE:
		if (len != 2)
			return EINVAL;
		if (letoh16(wreq.wi_val[0]) != 1)
			return EINVAL;		/* not supported */
		break;
	case WI_RID_PM_ENABLED:
		if (len != 2)
			return EINVAL;
		if (wreq.wi_val[0] != 0) {
			if ((ic->ic_caps & IEEE80211_C_PMGT) == 0)
				return EINVAL;
			if ((ic->ic_flags & IEEE80211_F_PMGTON) == 0) {
				ic->ic_flags |= IEEE80211_F_PMGTON;
				error = ENETRESET;
			}
		} else {
			if (ic->ic_flags & IEEE80211_F_PMGTON) {
				ic->ic_flags &= ~IEEE80211_F_PMGTON;
				error = ENETRESET;
			}
		}
		break;
	case WI_RID_MAX_SLEEP:
		if (len != 2)
			return EINVAL;
		ic->ic_lintval = letoh16(wreq.wi_val[0]);
		if (ic->ic_flags & IEEE80211_F_PMGTON)
			error = ENETRESET;
		break;
	case WI_RID_CUR_BEACON_INT:
		return EPERM;
	case WI_RID_WEP_AVAIL:
		return EPERM;
	case WI_RID_CNFAUTHMODE:
		if (len != 2)
			return EINVAL;
		if (letoh16(wreq.wi_val[0]) != 1)
			return EINVAL;		/* TODO: shared key auth */
		break;
	case WI_RID_ENCRYPTION:
		if (len != 2)
			return EINVAL;
		if (wreq.wi_val[0] != 0) {
			if ((ic->ic_caps & IEEE80211_C_WEP) == 0)
				return EINVAL;
			if ((ic->ic_flags & IEEE80211_F_WEPON) == 0) {
				ic->ic_flags |= IEEE80211_F_WEPON;
				error = ENETRESET;
			}
		} else {
			if (ic->ic_flags & IEEE80211_F_WEPON) {
				ic->ic_flags &= ~IEEE80211_F_WEPON;
				error = ENETRESET;
			}
		}
		break;
	case WI_RID_TX_CRYPT_KEY:
		if (len != 2)
			return EINVAL;
		i = letoh16(wreq.wi_val[0]);
		if (i >= IEEE80211_WEP_NKID)
			return EINVAL;
		ic->ic_wep_txkey = i;
		break;
	case WI_RID_DEFLT_CRYPT_KEYS:
		if (len != sizeof(struct wi_ltv_keys))
			return EINVAL;
		keys = (struct wi_ltv_keys *)&wreq;
		for (i = 0; i < IEEE80211_WEP_NKID; i++) {
			len = letoh16(keys->wi_keys[i].wi_keylen);
			if (len < 0 || len > sizeof(ic->ic_nw_keys[i].wk_key))
				return EINVAL;
		}
		memset(ic->ic_nw_keys, 0, sizeof(ic->ic_nw_keys));
		for (i = 0; i < IEEE80211_WEP_NKID; i++) {
			len = letoh16(keys->wi_keys[i].wi_keylen);
			ic->ic_nw_keys[i].wk_len = len;
			memcpy(ic->ic_nw_keys[i].wk_key,
			    keys->wi_keys[i].wi_keydat, len);
		}
		error = ENETRESET;
		break;
	case WI_RID_MAX_DATALEN:
		if (len != 2)
			return EINVAL;
		len = letoh16(wreq.wi_val[0]);
		if (len < 350 /* ? */ || len > IEEE80211_MAX_LEN)
			return EINVAL;
		if (len != IEEE80211_MAX_LEN)
			return EINVAL;		/* TODO: fragment */
		ic->ic_fragthreshold = len;
		error = ENETRESET;
		break;
	case WI_RID_IFACE_STATS:
		error = EPERM;
		break;
	case WI_RID_SCAN_REQ:			/* XXX wicontrol */
		if (ic->ic_opmode == IEEE80211_M_HOSTAP)
			break;
		error = ieee80211_setupscan(ic);
		if (error == 0)
			error = ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);
		break;
	case WI_RID_SCAN_APS:
		if (ic->ic_opmode == IEEE80211_M_HOSTAP)
			break;
		len--;			/* XXX: tx rate? */
		/* FALLTHRU */
	case WI_RID_CHANNEL_LIST:
		memset(chanlist, 0, sizeof(chanlist));
		/*
		 * Since channel 0 is not available for DS, channel 1
		 * is assigned to LSB on WaveLAN.
		 */
		if (ic->ic_phytype == IEEE80211_T_DS)
			i = 1;
		else
			i = 0;
		for (j = 0; i <= IEEE80211_CHAN_MAX; i++, j++) {
			if ((j / 8) >= len)
				break;
			if (isclr((u_int8_t *)wreq.wi_val, j))
				continue;
			if (isclr(ic->ic_chan_active, i)) {
				if (wreq.wi_type != WI_RID_CHANNEL_LIST)
					continue;
				if (isclr(ic->ic_chan_avail, i))
					return EPERM;
			}
			setbit(chanlist, i);
		}
		memcpy(ic->ic_chan_active, chanlist,
		    sizeof(ic->ic_chan_active));
		error = ieee80211_setupscan(ic);
		if (wreq.wi_type == WI_RID_CHANNEL_LIST) {
			/* NB: ignore error from ieee80211_setupscan */
			error = ENETRESET;
		} else if (error == 0)
			error = ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);
		break;
	default:
		error = EINVAL;
		break;
	}
	return error;
}

#ifdef __FreeBSD__
int
ieee80211_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct ieee80211com *ic = (void *)ifp;
	int error = 0;
	u_int kid, len;
	struct ieee80211req *ireq;
	struct ifreq *ifr;
	u_int8_t tmpkey[IEEE80211_KEYBUF_SIZE];
	char tmpssid[IEEE80211_NWID_LEN];
	struct ieee80211_channel *chan;
	struct ifaddr *ifa;			/* XXX */

	switch (cmd) {
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, (struct ifreq *) data,
				&ic->ic_media, cmd);
		break;
	case SIOCG80211:
		ireq = (struct ieee80211req *) data;
		switch (ireq->i_type) {
		case IEEE80211_IOC_SSID:
			switch (ic->ic_state) {
			case IEEE80211_S_INIT:
			case IEEE80211_S_SCAN:
				ireq->i_len = ic->ic_des_esslen;
				memcpy(tmpssid, ic->ic_des_essid, ireq->i_len);
				break;
			default:
				ireq->i_len = ic->ic_bss->ni_esslen;
				memcpy(tmpssid, ic->ic_bss->ni_essid,
					ireq->i_len);
				break;
			}
			error = copyout(tmpssid, ireq->i_data, ireq->i_len);
			break;
		case IEEE80211_IOC_NUMSSIDS:
			ireq->i_val = 1;
			break;
		case IEEE80211_IOC_WEP:
			if ((ic->ic_caps & IEEE80211_C_WEP) == 0) {
				ireq->i_val = IEEE80211_WEP_NOSUP;
			} else {
				if (ic->ic_flags & IEEE80211_F_WEPON) {
					ireq->i_val =
					    IEEE80211_WEP_MIXED;
				} else {
					ireq->i_val =
					    IEEE80211_WEP_OFF;
				}
			}
			break;
		case IEEE80211_IOC_WEPKEY:
			if ((ic->ic_caps & IEEE80211_C_WEP) == 0) {
				error = EINVAL;
				break;
			}
			kid = (u_int) ireq->i_val;
			if (kid >= IEEE80211_WEP_NKID) {
				error = EINVAL;
				break;
			}
			len = (u_int) ic->ic_nw_keys[kid].wk_len;
			/* NB: only root can read WEP keys */
			if (suser(curthread) == 0) {
				bcopy(ic->ic_nw_keys[kid].wk_key, tmpkey, len);
			} else {
				bzero(tmpkey, len);
			}
			ireq->i_len = len;
			error = copyout(tmpkey, ireq->i_data, len);
			break;
		case IEEE80211_IOC_NUMWEPKEYS:
			if ((ic->ic_caps & IEEE80211_C_WEP) == 0)
				error = EINVAL;
			else
				ireq->i_val = IEEE80211_WEP_NKID;
			break;
		case IEEE80211_IOC_WEPTXKEY:
			if ((ic->ic_caps & IEEE80211_C_WEP) == 0)
				error = EINVAL;
			else
				ireq->i_val = ic->ic_wep_txkey;
			break;
		case IEEE80211_IOC_AUTHMODE:
			ireq->i_val = IEEE80211_AUTH_OPEN;
			break;
		case IEEE80211_IOC_CHANNEL:
			switch (ic->ic_state) {
			case IEEE80211_S_INIT:
			case IEEE80211_S_SCAN:
				if (ic->ic_opmode == IEEE80211_M_STA)
					chan = ic->ic_des_chan;
				else
					chan = ic->ic_ibss_chan;
				break;
			default:
				chan = ic->ic_bss->ni_chan;
				break;
			}
			ireq->i_val = ieee80211_chan2ieee(ic, chan);
			break;
		case IEEE80211_IOC_POWERSAVE:
			if (ic->ic_flags & IEEE80211_F_PMGTON)
				ireq->i_val = IEEE80211_POWERSAVE_ON;
			else
				ireq->i_val = IEEE80211_POWERSAVE_OFF;
			break;
		case IEEE80211_IOC_POWERSAVESLEEP:
			ireq->i_val = ic->ic_lintval;
			break;
		case IEEE80211_IOC_RTSTHRESHOLD:
			ireq->i_val = ic->ic_rtsthreshold;
			break;
		case IEEE80211_IOC_PROTMODE:
			ireq->i_val = ic->ic_protmode;
			break;
		case IEEE80211_IOC_TXPOWER:
			if ((ic->ic_caps & IEEE80211_C_TXPMGT) == 0)
				error = EINVAL;
			else
				ireq->i_val = ic->ic_txpower;
			break;
		default:
			error = EINVAL;
			break;
		}
		break;
	case SIOCS80211:
		error = suser(curproc, 0);
		if (error)
			break;
		ireq = (struct ieee80211req *) data;
		switch (ireq->i_type) {
		case IEEE80211_IOC_SSID:
			if (ireq->i_val != 0 ||
			    ireq->i_len > IEEE80211_NWID_LEN) {
				error = EINVAL;
				break;
			}
			error = copyin(ireq->i_data, tmpssid, ireq->i_len);
			if (error)
				break;
			memset(ic->ic_des_essid, 0, IEEE80211_NWID_LEN);
			ic->ic_des_esslen = ireq->i_len;
			memcpy(ic->ic_des_essid, tmpssid, ireq->i_len);
			error = ENETRESET;
			break;
		case IEEE80211_IOC_WEP:
			/*
			 * These cards only support one mode so
			 * we just turn wep on if what ever is
			 * passed in is not OFF.
			 */
			if (ireq->i_val == IEEE80211_WEP_OFF) {
				ic->ic_flags &= ~IEEE80211_F_WEPON;
			} else {
				ic->ic_flags |= IEEE80211_F_WEPON;
			}
			error = ENETRESET;
			break;
		case IEEE80211_IOC_WEPKEY:
			if ((ic->ic_caps & IEEE80211_C_WEP) == 0) {
				error = EINVAL;
				break;
			}
			kid = (u_int) ireq->i_val;
			if (kid >= IEEE80211_WEP_NKID) {
				error = EINVAL;
				break;
			}
			if (ireq->i_len > sizeof(tmpkey)) {
				error = EINVAL;
				break;
			}
			memset(tmpkey, 0, sizeof(tmpkey));
			error = copyin(ireq->i_data, tmpkey, ireq->i_len);
			if (error)
				break;
			memcpy(ic->ic_nw_keys[kid].wk_key, tmpkey,
				sizeof(tmpkey));
			ic->ic_nw_keys[kid].wk_len = ireq->i_len;
			error = ENETRESET;
			break;
		case IEEE80211_IOC_WEPTXKEY:
			kid = (u_int) ireq->i_val;
			if (kid >= IEEE80211_WEP_NKID) {
				error = EINVAL;
				break;
			}
			ic->ic_wep_txkey = kid;
			error = ENETRESET;
			break;
#if 0
		case IEEE80211_IOC_AUTHMODE:
			sc->wi_authmode = ireq->i_val;
			break;
#endif
		case IEEE80211_IOC_CHANNEL:
			/* XXX 0xffff overflows 16-bit signed */
			if (ireq->i_val == 0 ||
			    ireq->i_val == (int16_t) IEEE80211_CHAN_ANY)
				ic->ic_des_chan = IEEE80211_CHAN_ANYC;
			else if ((u_int) ireq->i_val > IEEE80211_CHAN_MAX ||
			    isclr(ic->ic_chan_active, ireq->i_val)) {
				error = EINVAL;
				break;
			} else
				ic->ic_ibss_chan = ic->ic_des_chan =
					&ic->ic_channels[ireq->i_val];
			switch (ic->ic_state) {
			case IEEE80211_S_INIT:
			case IEEE80211_S_SCAN:
				error = ENETRESET;
				break;
			default:
				if (ic->ic_opmode == IEEE80211_M_STA) {
					if (ic->ic_des_chan != IEEE80211_CHAN_ANYC &&
					    ic->ic_bss->ni_chan != ic->ic_des_chan)
						error = ENETRESET;
				} else {
					if (ic->ic_bss->ni_chan != ic->ic_ibss_chan)
						error = ENETRESET;
				}
				break;
			}
			break;
		case IEEE80211_IOC_POWERSAVE:
			switch (ireq->i_val) {
			case IEEE80211_POWERSAVE_OFF:
				if (ic->ic_flags & IEEE80211_F_PMGTON) {
					ic->ic_flags &= ~IEEE80211_F_PMGTON;
					error = ENETRESET;
				}
				break;
			case IEEE80211_POWERSAVE_ON:
				if ((ic->ic_caps & IEEE80211_C_PMGT) == 0)
					error = EINVAL;
				else if ((ic->ic_flags & IEEE80211_F_PMGTON) == 0) {
					ic->ic_flags |= IEEE80211_F_PMGTON;
					error = ENETRESET;
				}
				break;
			default:
				error = EINVAL;
				break;
			}
			break;
		case IEEE80211_IOC_POWERSAVESLEEP:
			if (ireq->i_val < 0) {
				error = EINVAL;
				break;
			}
			ic->ic_lintval = ireq->i_val;
			error = ENETRESET;
			break;
		case IEEE80211_IOC_RTSTHRESHOLD:
			if (!(IEEE80211_RTS_MIN < ireq->i_val &&
			      ireq->i_val <= IEEE80211_RTS_MAX + 1)) {
				error = EINVAL;
				break;
			}
			ic->ic_rtsthreshold = ireq->i_val;
			error = ENETRESET;
			break;
		case IEEE80211_IOC_PROTMODE:
			if (ireq->i_val > IEEE80211_PROT_RTSCTS) {
				error = EINVAL;
				break;
			}
			ic->ic_protmode = ireq->i_val;
			/* NB: if not operating in 11g this can wait */
			if (ic->ic_curmode == IEEE80211_MODE_11G)
				error = ENETRESET;
			break;
		case IEEE80211_IOC_TXPOWER:
			if ((ic->ic_caps & IEEE80211_C_TXPMGT) == 0) {
				error = EINVAL;
				break;
			}
			if (!(IEEE80211_TXPOWER_MIN < ireq->i_val &&
			      ireq->i_val < IEEE80211_TXPOWER_MAX)) {
				error = EINVAL;
				break;
			}
			ic->ic_txpower = ireq->i_val;
			error = ENETRESET;
			break;
		default:
			error = EINVAL;
			break;
		}
		break;
	case SIOCGIFGENERIC:
		error = ieee80211_cfgget(ifp, cmd, data);
		break;
	case SIOCSIFGENERIC:
		error = suser(curproc, 0);
		if (error)
			break;
		error = ieee80211_cfgset(ifp, cmd, data);
		break;
	default:
		error = ether_ioctl(ifp, cmd, data);
		break;
	}
	return error;
}

#else /* !__FreeBSD__ */

int
ieee80211_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct ieee80211com *ic = (void *)ifp;
	struct ifreq *ifr = (struct ifreq *)data;
	int i, error = 0;
	struct ieee80211_nwid nwid;
	struct ieee80211_nwkey *nwkey;
	struct ieee80211_power *power;
	struct ieee80211_bssid *bssid;
	struct ieee80211chanreq *chanreq;
	struct ieee80211_channel *chan;
	struct ieee80211_txpower *txpower;
	struct ieee80211_wepkey keys[IEEE80211_WEP_NKID];
	static const u_int8_t empty_macaddr[IEEE80211_ADDR_LEN] = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
#if defined(__OpenBSD__)
	struct ieee80211_node *ni;
	struct hostap_getall reqall;
	struct hostap_sta stabuf;
#endif

	switch (cmd) {
	case SIOCSIFADDR:
	case SIOCGIFADDR:
#ifdef __OpenBSD__
		error = ether_ioctl(ifp, &ic->ic_ac, cmd, data);
#else
		error = ether_ioctl(ifp, cmd, data);
#endif
		break;
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &ic->ic_media, cmd);
		break;
	case SIOCS80211NWID:
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
		switch (ic->ic_state) {
		case IEEE80211_S_INIT:
		case IEEE80211_S_SCAN:
			nwid.i_len = ic->ic_des_esslen;
			memcpy(nwid.i_nwid, ic->ic_des_essid, nwid.i_len);
			break;
		default:
			nwid.i_len = ic->ic_bss->ni_esslen;
			memcpy(nwid.i_nwid, ic->ic_bss->ni_essid, nwid.i_len);
			break;
		}
		error = copyout(&nwid, ifr->ifr_data, sizeof(nwid));
		break;
	case SIOCS80211NWKEY:
		nwkey = (struct ieee80211_nwkey *)data;
		if ((ic->ic_caps & IEEE80211_C_WEP) == 0 &&
		    nwkey->i_wepon != IEEE80211_NWKEY_OPEN) {
			error = EINVAL;
			break;
		}
		/* check and copy keys */
		memset(keys, 0, sizeof(keys));
		for (i = 0; i < IEEE80211_WEP_NKID; i++) {
			keys[i].wk_len = nwkey->i_key[i].i_keylen;
			if (keys[i].wk_len > sizeof(keys[i].wk_key)) {
				error = EINVAL;
				break;
			}
			if (keys[i].wk_len <= 0)
				continue;
			if ((error = copyin(nwkey->i_key[i].i_keydat,
			    keys[i].wk_key, keys[i].wk_len)) != 0)
				break;
		}
		if (error)
			break;
		i = nwkey->i_defkid - 1;
		if (i < 0 || i >= IEEE80211_WEP_NKID ||
		    keys[i].wk_len == 0 ||
		    (keys[i].wk_len == -1 && ic->ic_nw_keys[i].wk_len == 0)) {
			if (nwkey->i_wepon != IEEE80211_NWKEY_OPEN) {
				error = EINVAL;
				break;
			}
		} else
			ic->ic_wep_txkey = i;
		/* save the key */
		if (nwkey->i_wepon == IEEE80211_NWKEY_OPEN)
			ic->ic_flags &= ~IEEE80211_F_WEPON;
		else
			ic->ic_flags |= IEEE80211_F_WEPON;
		for (i = 0; i < IEEE80211_WEP_NKID; i++) {
			if (keys[i].wk_len < 0)
				continue;
			ic->ic_nw_keys[i].wk_len = keys[i].wk_len;
			memcpy(ic->ic_nw_keys[i].wk_key, keys[i].wk_key,
			    sizeof(keys[i].wk_key));
		}
		error = ENETRESET;
		break;
	case SIOCG80211NWKEY:
		nwkey = (struct ieee80211_nwkey *)data;
		if (ic->ic_flags & IEEE80211_F_WEPON)
			nwkey->i_wepon = IEEE80211_NWKEY_WEP;
		else
			nwkey->i_wepon = IEEE80211_NWKEY_OPEN;
		nwkey->i_defkid = ic->ic_wep_txkey + 1;
		for (i = 0; i < IEEE80211_WEP_NKID; i++) {
			if (nwkey->i_key[i].i_keydat == NULL)
				continue;
			/* do not show any keys to non-root user */
			if ((error = suser(curproc, 0)) != 0)
				break;
			nwkey->i_key[i].i_keylen = ic->ic_nw_keys[i].wk_len;
			if ((error = copyout(ic->ic_nw_keys[i].wk_key,
			    nwkey->i_key[i].i_keydat,
			    ic->ic_nw_keys[i].wk_len)) != 0)
				break;
		}
		break;
	case SIOCS80211POWER:
		power = (struct ieee80211_power *)data;
		ic->ic_lintval = power->i_maxsleep;
		if (power->i_enabled != 0) {
			if ((ic->ic_caps & IEEE80211_C_PMGT) == 0)
				error = EINVAL;
			else if ((ic->ic_flags & IEEE80211_F_PMGTON) == 0) {
				ic->ic_flags |= IEEE80211_F_PMGTON;
				error = ENETRESET;
			}
		} else {
			if (ic->ic_flags & IEEE80211_F_PMGTON) {
				ic->ic_flags &= ~IEEE80211_F_PMGTON;
				error = ENETRESET;
			}
		}
		break;
	case SIOCG80211POWER:
		power = (struct ieee80211_power *)data;
		power->i_enabled = (ic->ic_flags & IEEE80211_F_PMGTON) ? 1 : 0;
		power->i_maxsleep = ic->ic_lintval;
		break;
	case SIOCS80211BSSID:
		bssid = (struct ieee80211_bssid *)data;
		if (IEEE80211_ADDR_EQ(bssid->i_bssid, empty_macaddr))
			ic->ic_flags &= ~IEEE80211_F_DESBSSID;
		else {
			ic->ic_flags |= IEEE80211_F_DESBSSID;
			IEEE80211_ADDR_COPY(ic->ic_des_bssid, bssid->i_bssid);
		}
		if (ic->ic_opmode == IEEE80211_M_HOSTAP)
			break;
		switch (ic->ic_state) {
		case IEEE80211_S_INIT:
		case IEEE80211_S_SCAN:
			error = ENETRESET;
			break;
		default:
			if ((ic->ic_flags & IEEE80211_F_DESBSSID) &&
			    !IEEE80211_ADDR_EQ(ic->ic_des_bssid,
			    ic->ic_bss->ni_bssid))
				error = ENETRESET;
			break;
		}
		break;
	case SIOCG80211BSSID:
		bssid = (struct ieee80211_bssid *)data;
		switch (ic->ic_state) {
		case IEEE80211_S_INIT:
		case IEEE80211_S_SCAN:
			if (ic->ic_opmode == IEEE80211_M_HOSTAP)
				IEEE80211_ADDR_COPY(bssid->i_bssid,
				    ic->ic_myaddr);
			else if (ic->ic_flags & IEEE80211_F_DESBSSID)
				IEEE80211_ADDR_COPY(bssid->i_bssid,
				    ic->ic_des_bssid);
			else
				memset(bssid->i_bssid, 0, IEEE80211_ADDR_LEN);
			break;
		default:
			IEEE80211_ADDR_COPY(bssid->i_bssid,
			    ic->ic_bss->ni_bssid);
			break;
		}
		break;
	case SIOCS80211CHANNEL:
		chanreq = (struct ieee80211chanreq *)data;
		if (chanreq->i_channel == IEEE80211_CHAN_ANY)
			ic->ic_des_chan = IEEE80211_CHAN_ANYC;
		else if (chanreq->i_channel > IEEE80211_CHAN_MAX ||
		    isclr(ic->ic_chan_active, chanreq->i_channel)) {
			error = EINVAL;
			break;
		} else
			ic->ic_ibss_chan = ic->ic_des_chan =
			    &ic->ic_channels[chanreq->i_channel];
		switch (ic->ic_state) {
		case IEEE80211_S_INIT:
		case IEEE80211_S_SCAN:
			error = ENETRESET;
			break;
		default:
			if (ic->ic_opmode == IEEE80211_M_STA) {
				if (ic->ic_des_chan != IEEE80211_CHAN_ANYC &&
				    ic->ic_bss->ni_chan != ic->ic_des_chan)
					error = ENETRESET;
			} else {
				if (ic->ic_bss->ni_chan != ic->ic_ibss_chan)
					error = ENETRESET;
			}
			break;
		}
		break;
	case SIOCG80211CHANNEL:
		chanreq = (struct ieee80211chanreq *)data;
		switch (ic->ic_state) {
		case IEEE80211_S_INIT:
		case IEEE80211_S_SCAN:
			if (ic->ic_opmode == IEEE80211_M_STA)
				chan = ic->ic_des_chan;
			else
				chan = ic->ic_ibss_chan;
			break;
		default:
			chan = ic->ic_bss->ni_chan;
			break;
		}
		chanreq->i_channel = ieee80211_chan2ieee(ic, chan);
		break;
	case SIOCGIFGENERIC:
		error = ieee80211_cfgget(ifp, cmd, data);
		break;
	case SIOCSIFGENERIC:
		error = suser(curproc, 0);
		if (error)
			break;
		error = ieee80211_cfgset(ifp, cmd, data);
		break;
#if 0
	case SIOCG80211ZSTATS:
#endif
	case SIOCG80211STATS:
		ifr = (struct ifreq *)data;
		copyout(&ic->ic_stats, ifr->ifr_data, sizeof (ic->ic_stats));
#if 0
		if (cmd == SIOCG80211ZSTATS)
			memset(&ic->ic_stats, 0, sizeof(ic->ic_stats));
#endif
		break;
	case SIOCS80211TXPOWER:
		txpower = (struct ieee80211_txpower *)data;
		if ((error = suser(curproc, 0)))
			break;
		if ((ic->ic_caps & IEEE80211_C_TXPMGT) == 0) {
			error = EINVAL;
			break;
		}
		if (!(IEEE80211_TXPOWER_MIN < txpower->i_val &&
			txpower->i_val < IEEE80211_TXPOWER_MAX)) {
			error = EINVAL;
			break;
		}
		ic->ic_txpower = txpower->i_val;
		error = ENETRESET;
		break;
	case SIOCG80211TXPOWER:
		txpower = (struct ieee80211_txpower *)data;
		if ((ic->ic_caps & IEEE80211_C_TXPMGT) == 0)
			error = EINVAL;
		else
			txpower->i_val = ic->ic_txpower;
		break;
	case SIOCSIFMTU:
		ifr = (struct ifreq *)data;
		if (!(IEEE80211_MTU_MIN <= ifr->ifr_mtu &&
		    ifr->ifr_mtu <= IEEE80211_MTU_MAX))
			error = EINVAL;
		else
			ifp->if_mtu = ifr->ifr_mtu;
		break;
#if defined(__OpenBSD__)
	case SIOCHOSTAP_DEL:
		if ((error = suser(curproc, 0)))
			break;
		if ((error = copyin(ifr->ifr_data, &stabuf, sizeof(stabuf))))
			break;
		ni = ieee80211_find_node(ic, stabuf.addr);
		if (ni == NULL)
			error = ENOENT;
		else {
			if (ni->ni_state == IEEE80211_STA_COLLECT)
				break;

			/* Disassociate station. */
			if (ni->ni_state == IEEE80211_STA_ASSOC)
				IEEE80211_SEND_MGMT(ic, ni,
				    IEEE80211_FC0_SUBTYPE_DISASSOC,
				    IEEE80211_REASON_ASSOC_LEAVE);

			/* Deauth station. */
			if (ni->ni_state >= IEEE80211_STA_AUTH)
				IEEE80211_SEND_MGMT(ic, ni,
				    IEEE80211_FC0_SUBTYPE_DEAUTH,
				    IEEE80211_REASON_AUTH_LEAVE);

			ieee80211_release_node(ic, ni);
		}
		break;
	case SIOCHOSTAP_GET:
		if ((error = copyin(ifr->ifr_data, &stabuf, sizeof(stabuf))))
			break;
		ni = ieee80211_find_node(ic, stabuf.addr);
		if (ni == NULL)
			error = ENOENT;
		else {
			node2sta(ic, ni, &stabuf);
			error = copyout(&stabuf, ifr->ifr_data,
			    sizeof(stabuf));
		}
		break;
	case SIOCHOSTAP_GETALL:
		if ((error = copyin(ifr->ifr_data, &reqall, sizeof(reqall))))
			break;

		reqall.nstations = i = 0;
		ni = TAILQ_FIRST(&ic->ic_node);
		while(ni && reqall.size >= i + sizeof(struct hostap_sta)) {
			node2sta(ic, ni, &stabuf);
			error = copyout(&stabuf, (caddr_t) reqall.addr + i,
			    sizeof(struct hostap_sta));
			if (error)
				break;
			i += sizeof(struct hostap_sta);
			reqall.nstations++;
			ni = TAILQ_NEXT(ni, ni_list);
		}

		if (!error)
			error = copyout(&reqall, ifr->ifr_data,
			    sizeof(reqall));
		break;
	case SIOCHOSTAP_ADD:
	case SIOCHOSTAP_GFLAGS:
	case SIOCHOSTAP_SFLAGS:
#endif
	default:
		error = EINVAL;
		break;
	}
	return error;
}
#endif /* !__FreeBSD__ */

#if defined(__OpenBSD__)
void
node2sta(struct ieee80211com *ic, struct ieee80211_node *ni,
    struct hostap_sta *sta)
{
	bcopy(ni->ni_macaddr, sta->addr, IEEE80211_ADDR_LEN);
	sta->flags = 0;
	if (ni->ni_state >= IEEE80211_STA_AUTH)
		sta->flags |= HOSTAP_FLAGS_AUTHEN;
	if (ni->ni_state >= IEEE80211_STA_ASSOC)
		sta->flags |= HOSTAP_FLAGS_ASSOC;
	sta->asid = ni->ni_associd;
	sta->capinfo = ni->ni_capinfo;
	sta->sig_info = (*ic->ic_node_getrssi)(ic, ni);
	sta->rates = 0x00;	/* not compatible */
}
#endif
