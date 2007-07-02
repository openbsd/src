/*	$OpenBSD: ieee80211_node.c,v 1.25 2007/07/02 16:46:44 damien Exp $	*/
/*	$NetBSD: ieee80211_node.c,v 1.14 2004/05/09 09:18:47 dyoung Exp $	*/

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
#include "bridge.h"

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
#include <sys/tree.h>

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

#if NBRIDGE > 0
#include <net/if_bridge.h>
#endif

#include <net80211/ieee80211_var.h>

#include <dev/rndvar.h>

struct ieee80211_node *ieee80211_node_alloc(struct ieee80211com *);
void ieee80211_node_free(struct ieee80211com *, struct ieee80211_node *);
void ieee80211_node_copy(struct ieee80211com *, struct ieee80211_node *,
    const struct ieee80211_node *);
u_int8_t ieee80211_node_getrssi(struct ieee80211com *,
    const struct ieee80211_node *);
void ieee80211_setup_node(struct ieee80211com *, struct ieee80211_node *,
    const u_int8_t *);
void ieee80211_free_node(struct ieee80211com *, struct ieee80211_node *);
struct ieee80211_node *ieee80211_alloc_node_helper(struct ieee80211com *);
void ieee80211_node_cleanup(struct ieee80211com *, struct ieee80211_node *);
void ieee80211_node_join_11g(struct ieee80211com *, struct ieee80211_node *);
void ieee80211_node_leave_11g(struct ieee80211com *, struct ieee80211_node *);

#define M_80211_NODE	M_DEVBUF

void
ieee80211_node_attach(struct ifnet *ifp)
{
	struct ieee80211com *ic = (void *)ifp;
	int size;

	RB_INIT(&ic->ic_tree);
	ic->ic_node_alloc = ieee80211_node_alloc;
	ic->ic_node_free = ieee80211_node_free;
	ic->ic_node_copy = ieee80211_node_copy;
	ic->ic_node_getrssi = ieee80211_node_getrssi;
	ic->ic_scangen = 1;
	ic->ic_max_nnodes = ieee80211_cache_size;

	if (ic->ic_max_aid == 0)
		ic->ic_max_aid = IEEE80211_AID_DEF;
	else if (ic->ic_max_aid > IEEE80211_AID_MAX)
		ic->ic_max_aid = IEEE80211_AID_MAX;
	size = howmany(ic->ic_max_aid, 32) * sizeof(u_int32_t);
	MALLOC(ic->ic_aid_bitmap, u_int32_t *, size, M_DEVBUF, M_NOWAIT);
	if (ic->ic_aid_bitmap == NULL) {
		/* XXX no way to recover */
		printf("%s: no memory for AID bitmap!\n", __func__);
		ic->ic_max_aid = 0;
	} else
		memset(ic->ic_aid_bitmap, 0, size);

	if (ic->ic_caps & (IEEE80211_C_HOSTAP | IEEE80211_C_IBSS)) {
		ic->ic_tim_len = howmany(ic->ic_max_aid, 8);
		MALLOC(ic->ic_tim_bitmap, u_int8_t *, ic->ic_tim_len, M_DEVBUF,
		    M_NOWAIT);
		if (ic->ic_tim_bitmap == NULL) {
			printf("%s: no memory for TIM bitmap!\n", __func__);
			ic->ic_tim_len = 0;
		} else
			memset(ic->ic_tim_bitmap, 0, ic->ic_tim_len);
	}
}

struct ieee80211_node *
ieee80211_alloc_node_helper(struct ieee80211com *ic)
{
	struct ieee80211_node *ni;
	if (ic->ic_nnodes >= ic->ic_max_nnodes)
		ieee80211_clean_nodes(ic);
	if (ic->ic_nnodes >= ic->ic_max_nnodes)
		return NULL;
	ni = (*ic->ic_node_alloc)(ic);
	if (ni != NULL)
		ic->ic_nnodes++;
	return ni;
}

void
ieee80211_node_lateattach(struct ifnet *ifp)
{
	struct ieee80211com *ic = (void *)ifp;
	struct ieee80211_node *ni;

	ni = ieee80211_alloc_node_helper(ic);
	if (ni == NULL)
		panic("unable to setup inital BSS node");
	ni->ni_chan = IEEE80211_CHAN_ANYC;
	ic->ic_bss = ieee80211_ref_node(ni);
	ic->ic_txpower = IEEE80211_TXPOWER_MAX;
}

void
ieee80211_node_detach(struct ifnet *ifp)
{
	struct ieee80211com *ic = (void *)ifp;

	if (ic->ic_bss != NULL) {
		(*ic->ic_node_free)(ic, ic->ic_bss);
		ic->ic_bss = NULL;
	}
	ieee80211_free_allnodes(ic);
	if (ic->ic_aid_bitmap != NULL)
		FREE(ic->ic_aid_bitmap, M_DEVBUF);
	if (ic->ic_tim_bitmap != NULL)
		FREE(ic->ic_tim_bitmap, M_DEVBUF);
}

/*
 * AP scanning support.
 */

/*
 * Initialize the active channel set based on the set
 * of available channels and the current PHY mode.
 */
void
ieee80211_reset_scan(struct ifnet *ifp)
{
	struct ieee80211com *ic = (void *)ifp;

	memcpy(ic->ic_chan_scan, ic->ic_chan_active,
		sizeof(ic->ic_chan_active));
	/* NB: hack, setup so next_scan starts with the first channel */
	if (ic->ic_bss != NULL && ic->ic_bss->ni_chan == IEEE80211_CHAN_ANYC)
		ic->ic_bss->ni_chan = &ic->ic_channels[IEEE80211_CHAN_MAX];
}

/*
 * Begin an active scan.
 */
void
ieee80211_begin_scan(struct ifnet *ifp)
{
	struct ieee80211com *ic = (void *)ifp;

	if (ic->ic_scan_lock & IEEE80211_SCAN_LOCKED)
		return;
	ic->ic_scan_lock |= IEEE80211_SCAN_LOCKED;

	/*
	 * In all but hostap mode scanning starts off in
	 * an active mode before switching to passive.
	 */
	if (ic->ic_opmode != IEEE80211_M_HOSTAP) {
		ic->ic_flags |= IEEE80211_F_ASCAN;
		ic->ic_stats.is_scan_active++;
	} else
		ic->ic_stats.is_scan_passive++;
	if (ifp->if_flags & IFF_DEBUG)
		printf("%s: begin %s scan\n", ifp->if_xname,
			(ic->ic_flags & IEEE80211_F_ASCAN) ?
				"active" : "passive");

	/*
	 * Flush any previously seen AP's. Note that the latter 
	 * assumes we don't act as both an AP and a station,
	 * otherwise we'll potentially flush state of stations
	 * associated with us.
	 */
	ieee80211_free_allnodes(ic);

	/*
	 * Reset the current mode. Setting the current mode will also
	 * reset scan state.
	 */
	if (IFM_MODE(ic->ic_media.ifm_cur->ifm_media) == IFM_AUTO)
		ic->ic_curmode = IEEE80211_MODE_AUTO;
	ieee80211_setmode(ic, ic->ic_curmode);

	ic->ic_scan_count = 0;

	/* Scan the next channel. */
	ieee80211_next_scan(ifp);
}

/*
 * Switch to the next channel marked for scanning.
 */
void
ieee80211_next_scan(struct ifnet *ifp)
{
	struct ieee80211com *ic = (void *)ifp;
	struct ieee80211_channel *chan;

	chan = ic->ic_bss->ni_chan;
	for (;;) {
		if (++chan > &ic->ic_channels[IEEE80211_CHAN_MAX])
			chan = &ic->ic_channels[0];
		if (isset(ic->ic_chan_scan, ieee80211_chan2ieee(ic, chan))) {
			/*
			 * Ignore channels marked passive-only
			 * during an active scan.
			 */
			if ((ic->ic_flags & IEEE80211_F_ASCAN) == 0 ||
			    (chan->ic_flags & IEEE80211_CHAN_PASSIVE) == 0)
				break;
		}
		if (chan == ic->ic_bss->ni_chan) {
			ieee80211_end_scan(ifp);
			return;
		}
	}
	clrbit(ic->ic_chan_scan, ieee80211_chan2ieee(ic, chan));
	IEEE80211_DPRINTF(("%s: chan %d->%d\n", __func__,
	    ieee80211_chan2ieee(ic, ic->ic_bss->ni_chan),
	    ieee80211_chan2ieee(ic, chan)));
	ic->ic_bss->ni_chan = chan;
	ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);
}

void
ieee80211_create_ibss(struct ieee80211com* ic, struct ieee80211_channel *chan)
{
	struct ieee80211_node *ni;
	struct ifnet *ifp = &ic->ic_if;

	ni = ic->ic_bss;
	if (ifp->if_flags & IFF_DEBUG)
		printf("%s: creating ibss\n", ifp->if_xname);
	ic->ic_flags |= IEEE80211_F_SIBSS;
	ni->ni_chan = chan;
	ni->ni_rates = ic->ic_sup_rates[ieee80211_chan2mode(ic, ni->ni_chan)];
	IEEE80211_ADDR_COPY(ni->ni_macaddr, ic->ic_myaddr);
	IEEE80211_ADDR_COPY(ni->ni_bssid, ic->ic_myaddr);
	if (ic->ic_opmode == IEEE80211_M_IBSS) {
		if ((ic->ic_flags & IEEE80211_F_DESBSSID) != 0)
			IEEE80211_ADDR_COPY(ni->ni_bssid, ic->ic_des_bssid);
		else
			ni->ni_bssid[0] |= 0x02;	/* local bit for IBSS */
	}
	ni->ni_esslen = ic->ic_des_esslen;
	memcpy(ni->ni_essid, ic->ic_des_essid, ni->ni_esslen);
	ni->ni_rssi = 0;
	ni->ni_rstamp = 0;
	memset(ni->ni_tstamp, 0, sizeof(ni->ni_tstamp));
	ni->ni_intval = ic->ic_lintval;
	ni->ni_capinfo = IEEE80211_CAPINFO_IBSS;
	if (ic->ic_flags & IEEE80211_F_WEPON)
		ni->ni_capinfo |= IEEE80211_CAPINFO_PRIVACY;
	if (ic->ic_phytype == IEEE80211_T_FH) {
		ni->ni_fhdwell = 200;	/* XXX */
		ni->ni_fhindex = 1;
	}
	ieee80211_new_state(ic, IEEE80211_S_RUN, -1);
}

int
ieee80211_match_bss(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	u_int8_t rate;
	int fail;

	fail = 0;
	if (isclr(ic->ic_chan_active, ieee80211_chan2ieee(ic, ni->ni_chan)))
		fail |= 0x01;
	if (ic->ic_des_chan != IEEE80211_CHAN_ANYC &&
	    ni->ni_chan != ic->ic_des_chan)
		fail |= 0x01;
	if (ic->ic_opmode == IEEE80211_M_IBSS) {
		if ((ni->ni_capinfo & IEEE80211_CAPINFO_IBSS) == 0)
			fail |= 0x02;
	} else {
		if ((ni->ni_capinfo & IEEE80211_CAPINFO_ESS) == 0)
			fail |= 0x02;
	}
	if (ic->ic_flags & IEEE80211_F_WEPON) {
		if ((ni->ni_capinfo & IEEE80211_CAPINFO_PRIVACY) == 0)
			fail |= 0x04;
	} else {
		/* XXX does this mean privacy is supported or required? */
		if (ni->ni_capinfo & IEEE80211_CAPINFO_PRIVACY)
			fail |= 0x04;
	}
	rate = ieee80211_fix_rate(ic, ni, IEEE80211_F_DONEGO);
	if (rate & IEEE80211_RATE_BASIC)
		fail |= 0x08;
	if (ic->ic_des_esslen != 0 &&
	    (ni->ni_esslen != ic->ic_des_esslen ||
	     memcmp(ni->ni_essid, ic->ic_des_essid, ic->ic_des_esslen) != 0))
		fail |= 0x10;
	if ((ic->ic_flags & IEEE80211_F_DESBSSID) &&
	    !IEEE80211_ADDR_EQ(ic->ic_des_bssid, ni->ni_bssid))
		fail |= 0x20;
#ifdef IEEE80211_DEBUG
	if (ic->ic_if.if_flags & IFF_DEBUG) {
		printf(" %c %s", fail ? '-' : '+',
		    ether_sprintf(ni->ni_macaddr));
		printf(" %s%c", ether_sprintf(ni->ni_bssid),
		    fail & 0x20 ? '!' : ' ');
		printf(" %3d%c", ieee80211_chan2ieee(ic, ni->ni_chan),
			fail & 0x01 ? '!' : ' ');
		printf(" %+4d", ni->ni_rssi);
		printf(" %2dM%c", (rate & IEEE80211_RATE_VAL) / 2,
		    fail & 0x08 ? '!' : ' ');
		printf(" %4s%c",
		    (ni->ni_capinfo & IEEE80211_CAPINFO_ESS) ? "ess" :
		    (ni->ni_capinfo & IEEE80211_CAPINFO_IBSS) ? "ibss" :
		    "????",
		    fail & 0x02 ? '!' : ' ');
		printf(" %3s%c ",
		    (ni->ni_capinfo & IEEE80211_CAPINFO_PRIVACY) ?
		    "wep" : "no",
		    fail & 0x04 ? '!' : ' ');
		ieee80211_print_essid(ni->ni_essid, ni->ni_esslen);
		printf("%s\n", fail & 0x10 ? "!" : "");
	}
#endif
	return fail;
}

/*
 * Complete a scan of potential channels.
 */
void
ieee80211_end_scan(struct ifnet *ifp)
{
	struct ieee80211com *ic = (void *)ifp;
	struct ieee80211_node *ni, *nextbs, *selbs;
	int i, fail;

	if (ifp->if_flags & IFF_DEBUG)
		printf("%s: end %s scan\n", ifp->if_xname,
			(ic->ic_flags & IEEE80211_F_ASCAN) ?
				"active" : "passive");

	if (ic->ic_scan_count)
		ic->ic_flags &= ~IEEE80211_F_ASCAN;

	ni = RB_MIN(ieee80211_tree, &ic->ic_tree);

	if (ic->ic_opmode == IEEE80211_M_HOSTAP) {
		/* XXX off stack? */
		u_char occupied[howmany(IEEE80211_CHAN_MAX, NBBY)];
		/*
		 * The passive scan to look for existing AP's completed,
		 * select a channel to camp on.  Identify the channels
		 * that already have one or more AP's and try to locate
		 * an unnoccupied one.  If that fails, pick a random
		 * channel from the active set.
		 */
		RB_FOREACH(ni, ieee80211_tree, &ic->ic_tree)
			setbit(occupied, ieee80211_chan2ieee(ic, ni->ni_chan));
		for (i = 0; i < IEEE80211_CHAN_MAX; i++)
			if (isset(ic->ic_chan_active, i) && isclr(occupied, i))
				break;
		if (i == IEEE80211_CHAN_MAX) {
			fail = arc4random() & 3;	/* random 0-3 */
			for (i = 0; i < IEEE80211_CHAN_MAX; i++)
				if (isset(ic->ic_chan_active, i) && fail-- == 0)
					break;
		}
		ieee80211_create_ibss(ic, &ic->ic_channels[i]);
		goto wakeup;
	}
	if (ni == NULL) {
		IEEE80211_DPRINTF(("%s: no scan candidate\n", __func__));
 notfound:
		if (ic->ic_opmode == IEEE80211_M_IBSS &&
		    (ic->ic_flags & IEEE80211_F_IBSSON) &&
		    ic->ic_des_esslen != 0) {
			ieee80211_create_ibss(ic, ic->ic_ibss_chan);
			goto wakeup;
		}

		/*
		 * Scan the next mode if nothing has been found. This
		 * is necessary if the device supports different
		 * incompatible modes in the same channel range, like
		 * like 11b and "pure" 11G mode. This will loop
		 * forever except for user-initiated scans.
		 */
		if (ieee80211_next_mode(ifp) == IEEE80211_MODE_AUTO) {
			if (ic->ic_scan_lock & IEEE80211_SCAN_REQUEST &&
			    ic->ic_scan_lock & IEEE80211_SCAN_RESUME) {
				ic->ic_scan_lock = IEEE80211_SCAN_LOCKED;
				/* Return from an user-initiated scan */
				wakeup(&ic->ic_scan_lock);
			} else if (ic->ic_scan_lock & IEEE80211_SCAN_REQUEST)
				goto wakeup;
			ic->ic_scan_count++;
		}

		/*
		 * Reset the list of channels to scan and start again.
		 */
		ieee80211_next_scan(ifp);
		return;
	}
	selbs = NULL;

	for (; ni != NULL; ni = nextbs) {
		nextbs = RB_NEXT(ieee80211_tree, &ic->ic_tree, ni);
		if (ni->ni_fails) {
			/*
			 * The configuration of the access points may change
			 * during my scan.  So delete the entry for the AP
			 * and retry to associate if there is another beacon.
			 */
			if (ni->ni_fails++ > 2)
				ieee80211_free_node(ic, ni);
			continue;
		}
		if (ieee80211_match_bss(ic, ni) == 0) {
			if (selbs == NULL)
				selbs = ni;
			else if (ni->ni_rssi > selbs->ni_rssi)
				selbs = ni;
		}
	}
	if (selbs == NULL)
		goto notfound;
	(*ic->ic_node_copy)(ic, ic->ic_bss, selbs);

	/*
	 * Set the erp state (mostly the slot time) to deal with
	 * the auto-select case; this should be redundant if the
	 * mode is locked.
	 */
	ic->ic_curmode = ieee80211_chan2mode(ic, selbs->ni_chan);
	ieee80211_reset_erp(ic);

	ieee80211_node_newstate(selbs, IEEE80211_STA_BSS);
	if (ic->ic_opmode == IEEE80211_M_IBSS) {
		ieee80211_fix_rate(ic, ic->ic_bss, IEEE80211_F_DOFRATE |
		    IEEE80211_F_DONEGO | IEEE80211_F_DODEL);
		if (ic->ic_bss->ni_rates.rs_nrates == 0)
			goto notfound;
		ieee80211_new_state(ic, IEEE80211_S_RUN, -1);
	} else {
		ieee80211_new_state(ic, IEEE80211_S_AUTH, -1);
	}

 wakeup:
	if (ic->ic_scan_lock & IEEE80211_SCAN_REQUEST) {
		/* Return from an user-initiated scan */
		wakeup(&ic->ic_scan_lock);
	}

	ic->ic_scan_lock = IEEE80211_SCAN_UNLOCKED;
}

int
ieee80211_get_rate(struct ieee80211com *ic)
{
	u_int8_t (*rates)[IEEE80211_RATE_MAXSIZE];
	int rate;

	rates = &ic->ic_bss->ni_rates.rs_rates;

	if (ic->ic_fixed_rate != -1)
		rate = (*rates)[ic->ic_fixed_rate];
	else if (ic->ic_state == IEEE80211_S_RUN)
		rate = (*rates)[ic->ic_bss->ni_txrate];
	else
		rate = 0;

	return rate & IEEE80211_RATE_VAL;
}

struct ieee80211_node *
ieee80211_node_alloc(struct ieee80211com *ic)
{
	struct ieee80211_node *ni;
	MALLOC(ni, struct ieee80211_node *, sizeof(struct ieee80211_node),
	    M_80211_NODE, M_NOWAIT);
	if (ni != NULL)
		memset(ni, 0, sizeof(struct ieee80211_node));
	return ni;
}

void
ieee80211_node_cleanup(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	if (ni->ni_challenge != NULL) {
		FREE(ni->ni_challenge, M_DEVBUF);
		ni->ni_challenge = NULL;
	}
}

void
ieee80211_node_free(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	ieee80211_node_cleanup(ic, ni);
	FREE(ni, M_80211_NODE);
}

void
ieee80211_node_copy(struct ieee80211com *ic,
	struct ieee80211_node *dst, const struct ieee80211_node *src)
{
	ieee80211_node_cleanup(ic, dst);
	*dst = *src;
	dst->ni_challenge = NULL;
}

u_int8_t
ieee80211_node_getrssi(struct ieee80211com *ic,
    const struct ieee80211_node *ni)
{
	return ni->ni_rssi;
}

void
ieee80211_setup_node(struct ieee80211com *ic,
	struct ieee80211_node *ni, const u_int8_t *macaddr)
{
	int s;

	IEEE80211_DPRINTF(("%s %s\n", __func__,
	    ether_sprintf((u_int8_t *)macaddr)));
	IEEE80211_ADDR_COPY(ni->ni_macaddr, macaddr);
	ieee80211_node_newstate(ni, IEEE80211_STA_CACHE);

	/* 
	 * Note we don't enable the inactive timer when acting
	 * as a station.  Nodes created in this mode represent
	 * AP's identified while scanning.  If we time them out
	 * then several things happen: we can't return the data
	 * to users to show the list of AP's we encountered, and
	 * more importantly, we'll incorrectly deauthenticate
	 * ourself because the inactivity timer will kick us off.
	 */
	s = splnet();
	if (ic->ic_opmode != IEEE80211_M_STA &&
	    RB_EMPTY(&ic->ic_tree))
		ic->ic_inact_timer = IEEE80211_INACT_WAIT;
	RB_INSERT(ieee80211_tree, &ic->ic_tree, ni);
	splx(s);
}

struct ieee80211_node *
ieee80211_alloc_node(struct ieee80211com *ic, const u_int8_t *macaddr)
{
	struct ieee80211_node *ni = ieee80211_alloc_node_helper(ic);
	if (ni != NULL)
		ieee80211_setup_node(ic, ni, macaddr);
	else
		ic->ic_stats.is_rx_nodealloc++;
	return ni;
}

struct ieee80211_node *
ieee80211_dup_bss(struct ieee80211com *ic, const u_int8_t *macaddr)
{
	struct ieee80211_node *ni = ieee80211_alloc_node_helper(ic);
	if (ni != NULL) {
		ieee80211_setup_node(ic, ni, macaddr);
		/*
		 * Inherit from ic_bss.
		 */
		IEEE80211_ADDR_COPY(ni->ni_bssid, ic->ic_bss->ni_bssid);
		ni->ni_chan = ic->ic_bss->ni_chan;
	} else
		ic->ic_stats.is_rx_nodealloc++;
	return ni;
}

struct ieee80211_node *
ieee80211_find_node(struct ieee80211com *ic, const u_int8_t *macaddr)
{
	struct ieee80211_node ni;

	IEEE80211_ADDR_COPY(ni.ni_macaddr, macaddr);
	return (RB_FIND(ieee80211_tree, &ic->ic_tree, &ni));
}

/*
 * Return a reference to the appropriate node for sending
 * a data frame.  This handles node discovery in adhoc networks.
 *
 * Drivers will call this, so increase the reference count before
 * returning the node.
 */
struct ieee80211_node *
ieee80211_find_txnode(struct ieee80211com *ic, const u_int8_t *macaddr)
{
	struct ieee80211_node *ni;
	int s;

	/*
	 * The destination address should be in the node table
	 * unless we are operating in station mode or this is a
	 * multicast/broadcast frame.
	 */
	if (ic->ic_opmode == IEEE80211_M_STA || IEEE80211_IS_MULTICAST(macaddr))
		return ieee80211_ref_node(ic->ic_bss);

	s = splnet();
	ni = ieee80211_find_node(ic, macaddr);
	splx(s);
	if (ni == NULL) {
		if (ic->ic_opmode != IEEE80211_M_IBSS &&
		    ic->ic_opmode != IEEE80211_M_AHDEMO)
			return NULL;

		/*
		 * Fake up a node; this handles node discovery in
		 * adhoc mode.  Note that for the driver's benefit
		 * we we treat this like an association so the driver
		 * has an opportunity to setup its private state.
		 *
		 * XXX need better way to handle this; issue probe
		 *     request so we can deduce rate set, etc.
		 */
		if ((ni = ieee80211_dup_bss(ic, macaddr)) == NULL)
			return NULL;
		/* XXX no rate negotiation; just dup */
		ni->ni_rates = ic->ic_bss->ni_rates;
		if (ic->ic_newassoc)
			(*ic->ic_newassoc)(ic, ni, 1);
	}
	return ieee80211_ref_node(ni);
}

/*
 * It is usually desirable to process a Rx packet using its sender's
 * node-record instead of the BSS record.
 *
 * - AP mode: keep a node-record for every authenticated/associated
 *   station *in the BSS*. For future use, we also track neighboring
 *   APs, since they might belong to the same ESS.  APs in the same
 *   ESS may bridge packets to each other, forming a Wireless
 *   Distribution System (WDS).
 *
 * - IBSS mode: keep a node-record for every station *in the BSS*.
 *   Also track neighboring stations by their beacons/probe responses.
 *
 * - monitor mode: keep a node-record for every sender, regardless
 *   of BSS.
 *
 * - STA mode: the only available node-record is the BSS record,
 *   ic->ic_bss.
 *
 * Of all the 802.11 Control packets, only the node-records for
 * RTS packets node-record can be looked up.
 *
 * Return non-zero if the packet's node-record is kept, zero
 * otherwise.
 */
static __inline int
ieee80211_needs_rxnode(struct ieee80211com *ic,
    const struct ieee80211_frame *wh, const u_int8_t **bssid)
{
	struct ieee80211_node *bss = ic->ic_bss;
	int monitor, rc = 0;

	monitor = (ic->ic_opmode == IEEE80211_M_MONITOR);

	*bssid = NULL;

	switch (wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) {
	case IEEE80211_FC0_TYPE_CTL:
		if (!monitor)
			break;
		return (wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) ==
		    IEEE80211_FC0_SUBTYPE_RTS;
	case IEEE80211_FC0_TYPE_MGT:
		*bssid = wh->i_addr3;
		switch (wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) {
		case IEEE80211_FC0_SUBTYPE_BEACON:
		case IEEE80211_FC0_SUBTYPE_PROBE_RESP:
			rc = 1;
			break;
		default:
			if (ic->ic_opmode == IEEE80211_M_STA)
				break;
			rc = IEEE80211_ADDR_EQ(*bssid, bss->ni_bssid) ||
			     IEEE80211_ADDR_EQ(*bssid, etherbroadcastaddr);
			break;
		}
		break;
	case IEEE80211_FC0_TYPE_DATA:
		switch (wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) {
		case IEEE80211_FC1_DIR_NODS:
			*bssid = wh->i_addr3;
			if (ic->ic_opmode == IEEE80211_M_IBSS ||
			    ic->ic_opmode == IEEE80211_M_AHDEMO)
				rc = IEEE80211_ADDR_EQ(*bssid, bss->ni_bssid);
			break;
		case IEEE80211_FC1_DIR_TODS:
			*bssid = wh->i_addr1;
			if (ic->ic_opmode == IEEE80211_M_HOSTAP)
				rc = IEEE80211_ADDR_EQ(*bssid, bss->ni_bssid);
			break;
		case IEEE80211_FC1_DIR_FROMDS:
		case IEEE80211_FC1_DIR_DSTODS:
			*bssid = wh->i_addr2;
			rc = (ic->ic_opmode == IEEE80211_M_HOSTAP);
			break;
		}
		break;
	}
	return monitor || rc;
}

/* 
 * Drivers call this, so increase the reference count before returning
 * the node.
 */
struct ieee80211_node *
ieee80211_find_rxnode(struct ieee80211com *ic,
    const struct ieee80211_frame *wh)
{
	struct ieee80211_node *ni;
	const static u_int8_t zero[IEEE80211_ADDR_LEN];
	const u_int8_t *bssid;
	int s;

	if (!ieee80211_needs_rxnode(ic, wh, &bssid))
		return ieee80211_ref_node(ic->ic_bss);

	s = splnet();
	ni = ieee80211_find_node(ic, wh->i_addr2);
	splx(s);

	if (ni != NULL)
		return ieee80211_ref_node(ni);
	if (ic->ic_opmode == IEEE80211_M_HOSTAP)
		return ieee80211_ref_node(ic->ic_bss);

	/* XXX see remarks in ieee80211_find_txnode */
	/* XXX no rate negotiation; just dup */
	if ((ni = ieee80211_dup_bss(ic, wh->i_addr2)) == NULL)
		return ieee80211_ref_node(ic->ic_bss);

	IEEE80211_ADDR_COPY(ni->ni_bssid, (bssid != NULL) ? bssid : zero);

	ni->ni_rates = ic->ic_bss->ni_rates;
	if (ic->ic_newassoc)
		(*ic->ic_newassoc)(ic, ni, 1);

	IEEE80211_DPRINTF(("%s: faked-up node %p for %s\n", __func__, ni,
	    ether_sprintf((u_int8_t *)wh->i_addr2)));

	return ieee80211_ref_node(ni);
}

struct ieee80211_node *
ieee80211_find_node_for_beacon(struct ieee80211com *ic,
    const u_int8_t *macaddr, const struct ieee80211_channel *chan,
    const char *ssid, u_int8_t rssi)
{
	struct ieee80211_node *ni, *keep = NULL;
	int s, score = 0;

	if ((ni = ieee80211_find_node(ic, macaddr)) != NULL) {
		s = splnet();

		if (ni->ni_chan != chan && ni->ni_rssi >= rssi)
			score++;
		if (ssid[1] == 0 && ni->ni_esslen != 0)
			score++;
		if (score > 0)
			keep = ni;

		splx(s);
	}

	return (keep);
}

void
ieee80211_free_node(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	if (ni == ic->ic_bss)
		panic("freeing bss node");

	IEEE80211_DPRINTF(("%s %s\n", __func__, ether_sprintf(ni->ni_macaddr)));
	IEEE80211_AID_CLR(ni->ni_associd, ic->ic_aid_bitmap);
	RB_REMOVE(ieee80211_tree, &ic->ic_tree, ni);
	ic->ic_nnodes--;
	if (!IF_IS_EMPTY(&ni->ni_savedq)) {
		IF_PURGE(&ni->ni_savedq);
		if (ic->ic_set_tim)
			(*ic->ic_set_tim)(ic, ni->ni_associd, 0);
	}
	if (RB_EMPTY(&ic->ic_tree))
		ic->ic_inact_timer = 0;
	(*ic->ic_node_free)(ic, ni);
	/* TBD indicate to drivers that a new node can be allocated */
}

void
ieee80211_release_node(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	int s;

	IEEE80211_DPRINTF(("%s %s refcnt %d\n", __func__,
	    ether_sprintf(ni->ni_macaddr), ni->ni_refcnt));
	if (ieee80211_node_decref(ni) == 0 &&
	    ni->ni_state == IEEE80211_STA_COLLECT) {
		s = splnet();
		ieee80211_free_node(ic, ni);
		splx(s);
	}
}

void
ieee80211_free_allnodes(struct ieee80211com *ic)
{
	struct ieee80211_node *ni;
	int s;

	IEEE80211_DPRINTF(("%s\n", __func__));
	s = splnet();
	while ((ni = RB_MIN(ieee80211_tree, &ic->ic_tree)) != NULL)
		ieee80211_free_node(ic, ni);
	splx(s);

	if (ic->ic_bss != NULL)
		ieee80211_node_cleanup(ic, ic->ic_bss);	/* for station mode */
}

/*
 * Timeout inactive nodes.
 */
void
ieee80211_clean_nodes(struct ieee80211com *ic)
{
	struct ieee80211_node *ni, *next_ni;
	u_int gen = ic->ic_scangen++;		/* NB: ok 'cuz single-threaded*/
	int s;

	s = splnet();
	for (ni = RB_MIN(ieee80211_tree, &ic->ic_tree);
	    ni != NULL; ni = next_ni) {
		next_ni = RB_NEXT(ieee80211_tree, &ic->ic_tree, ni);
		if (ic->ic_nnodes <= ic->ic_max_nnodes)
			break;
		if (ni->ni_scangen == gen)	/* previously handled */
			continue;
		ni->ni_scangen = gen;
		if (ni->ni_refcnt > 0)
			continue;
		IEEE80211_DPRINTF(("station %s purged from LRU cache\n",
		    ether_sprintf(ni->ni_macaddr)));
		/*
		 * Send a deauthenticate frame.
		 */
		if (ic->ic_opmode == IEEE80211_M_HOSTAP) {
			splx(s);
			IEEE80211_SEND_MGMT(ic, ni,
			    IEEE80211_FC0_SUBTYPE_DEAUTH,
			    IEEE80211_REASON_AUTH_EXPIRE);
			s = splnet();
			ieee80211_node_leave(ic, ni);
		} else
			ieee80211_free_node(ic, ni);
		ic->ic_stats.is_node_timeout++;
	}
	splx(s);
}

void
ieee80211_iterate_nodes(struct ieee80211com *ic, ieee80211_iter_func *f,
    void *arg)
{
	struct ieee80211_node *ni;
	int s;

	s = splnet();
	RB_FOREACH(ni, ieee80211_tree, &ic->ic_tree)
		(*f)(arg, ni);
	splx(s);
}

/*
 * Check if the specified node supports ERP.
 */
int
ieee80211_iserp_sta(const struct ieee80211_node *ni)
{
#define N(a)	(sizeof (a) / sizeof (a)[0])
	static const u_int8_t rates[] = { 2, 4, 11, 22, 12, 24, 48 };
	const struct ieee80211_rateset *rs = &ni->ni_rates;
	int i, j;

	/*
	 * A STA supports ERP operation if it includes all the Clause 19
	 * mandatory rates in its supported rate set.
	 */
	for (i = 0; i < N(rates); i++) {
		for (j = 0; j < rs->rs_nrates; j++) {
			if ((rs->rs_rates[j] & IEEE80211_RATE_VAL) == rates[i])
				break;
		}
		if (j == rs->rs_nrates)
			return 0;
	}
	return 1;
#undef N
}

/*
 * Handle a station joining an 11g network.
 */
void
ieee80211_node_join_11g(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	if (!(ni->ni_capinfo & IEEE80211_CAPINFO_SHORT_SLOTTIME)) {
		/*
		 * Joining STA doesn't support short slot time.  We must
		 * disable the use of short slot time for all other associated
		 * STAs and give the driver a chance to reconfigure the
		 * hardware.
		 */
		if (++ic->ic_longslotsta == 1) {
			if (ic->ic_caps & IEEE80211_C_SHSLOT)
				ieee80211_set_shortslottime(ic, 0);
		}
		IEEE80211_DPRINTF(("[%s] station needs long slot time, "
		    "count %d\n", ether_sprintf(ni->ni_macaddr),
		    ic->ic_longslotsta));
	}

	if (!ieee80211_iserp_sta(ni)) {
		/*
		 * Joining STA is non-ERP.
		 */
		ic->ic_nonerpsta++;

		IEEE80211_DPRINTF(("[%s] station is non-ERP, %d non-ERP "
		    "stations associated\n", ether_sprintf(ni->ni_macaddr),
		    ic->ic_nonerpsta));

		/* must enable the use of protection */
		if (ic->ic_protmode != IEEE80211_PROT_NONE) {
			IEEE80211_DPRINTF(("%s: enable use of protection\n",
			    __func__));
			ic->ic_flags |= IEEE80211_F_USEPROT;
		}

		if (!(ni->ni_capinfo & IEEE80211_CAPINFO_SHORT_PREAMBLE))
			ic->ic_flags &= ~IEEE80211_F_SHPREAMBLE;
	} else
		ni->ni_flags |= IEEE80211_NODE_ERP;
}

void
ieee80211_node_join(struct ieee80211com *ic, struct ieee80211_node *ni,
    int resp)
{
	int newassoc;

	if (ni->ni_associd == 0) {
		u_int16_t aid;

		/*
		 * It would be clever to search the bitmap
		 * more efficiently, but this will do for now.
		 */
		for (aid = 1; aid < ic->ic_max_aid; aid++) {
			if (!IEEE80211_AID_ISSET(aid,
			    ic->ic_aid_bitmap))
				break;
		}
		if (aid >= ic->ic_max_aid) {
			IEEE80211_SEND_MGMT(ic, ni, resp,
			    IEEE80211_REASON_ASSOC_TOOMANY);
			ieee80211_node_leave(ic, ni);
			return;
		}
		ni->ni_associd = aid | 0xc000;
		IEEE80211_AID_SET(ni->ni_associd, ic->ic_aid_bitmap);
		newassoc = 1;
		if (ic->ic_curmode == IEEE80211_MODE_11G)
			ieee80211_node_join_11g(ic, ni);
	} else
		newassoc = 0;

	IEEE80211_DPRINTF(("station %s %s associated at aid %d\n",
	    ether_sprintf(ni->ni_macaddr),
	    (newassoc ? "newly" : "already"),
	    ni->ni_associd & ~0xc000));

	/* give driver a chance to setup state like ni_txrate */
	if (ic->ic_newassoc)
		(*ic->ic_newassoc)(ic, ni, newassoc);
	IEEE80211_SEND_MGMT(ic, ni, resp, IEEE80211_STATUS_SUCCESS);
	ieee80211_node_newstate(ni, IEEE80211_STA_ASSOC);

#if NBRIDGE > 0
	/*
	 * If the parent interface belongs to a bridge, learn
	 * the node's address dynamically on this interface.
	 */
	if (ic->ic_if.if_bridge != NULL)
		bridge_update(&ic->ic_if,
		    (struct ether_addr *)ni->ni_macaddr, 0);
#endif
}

/*
 * Handle a station leaving an 11g network.
 */
void
ieee80211_node_leave_11g(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	if (!(ni->ni_capinfo & IEEE80211_CAPINFO_SHORT_SLOTTIME)) {
#ifdef DIAGNOSTIC
		if (ic->ic_longslotsta == 0) {
			panic("bogus long slot station count %d",
			    ic->ic_longslotsta);
		}
#endif
		/* leaving STA did not support short slot time */
		if (--ic->ic_longslotsta == 0) {
			/*
			 * All associated STAs now support short slot time, so
			 * enable this feature and give the driver a chance to
			 * reconfigure the hardware. Notice that IBSS always
			 * use a long slot time.
			 */
			if ((ic->ic_caps & IEEE80211_C_SHSLOT) &&
			    ic->ic_opmode != IEEE80211_M_IBSS)
				ieee80211_set_shortslottime(ic, 1);
		}
		IEEE80211_DPRINTF(("[%s] long slot time station leaves, "
		    "count now %d\n", ether_sprintf(ni->ni_macaddr),
		    ic->ic_longslotsta));
	}

	if (!(ni->ni_flags & IEEE80211_NODE_ERP)) {
#ifdef DIAGNOSTIC
		if (ic->ic_nonerpsta == 0) {
			panic("bogus non-ERP station count %d\n",
			    ic->ic_nonerpsta);
		}
#endif
		/* leaving STA was non-ERP */
		if (--ic->ic_nonerpsta == 0) {
			/*
			 * All associated STAs are now ERP capable, disable use
			 * of protection and re-enable short preamble support.
			 */
			ic->ic_flags &= ~IEEE80211_F_USEPROT;
			if (ic->ic_caps & IEEE80211_C_SHPREAMBLE)
				ic->ic_flags |= IEEE80211_F_SHPREAMBLE;
		}
		IEEE80211_DPRINTF(("[%s] non-ERP station leaves, "
		    "count now %d\n", ether_sprintf(ni->ni_macaddr),
		    ic->ic_nonerpsta));
	}
}

/*
 * Handle bookkeeping for station deauthentication/disassociation
 * when operating as an ap.
 */
void
ieee80211_node_leave(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	if (ic->ic_opmode != IEEE80211_M_HOSTAP)
		panic("not in ap mode, mode %u", ic->ic_opmode);
	/*
	 * If node wasn't previously associated all
	 * we need to do is reclaim the reference.
	 */
	if (ni->ni_associd == 0)
		return;
	IEEE80211_AID_CLR(ni->ni_associd, ic->ic_aid_bitmap);
	ni->ni_associd = 0;

	if (ic->ic_curmode == IEEE80211_MODE_11G)
		ieee80211_node_leave_11g(ic, ni);

	ieee80211_node_newstate(ni, IEEE80211_STA_COLLECT);

#if NBRIDGE > 0
	/*
	 * If the parent interface belongs to a bridge, delete
	 * any dynamically learned address for this node.
	 */
	if (ic->ic_if.if_bridge != NULL)
		bridge_update(&ic->ic_if,
		    (struct ether_addr *)ni->ni_macaddr, 1);
#endif
}

/*
 * Compare nodes in the tree by lladdr
 */
int
ieee80211_node_cmp(const struct ieee80211_node *b1,
    const struct ieee80211_node *b2)
{
	return (memcmp(b1->ni_macaddr, b2->ni_macaddr, IEEE80211_ADDR_LEN));
}

/*
 * Generate red-black tree function logic
 */
RB_GENERATE(ieee80211_tree, ieee80211_node, ni_node, ieee80211_node_cmp);
