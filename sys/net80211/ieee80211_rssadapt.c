/*	$OpenBSD: ieee80211_rssadapt.c,v 1.1 2004/06/22 22:53:52 millert Exp $	*/
/*	$NetBSD: ieee80211_rssadapt.c,v 1.7 2004/05/25 04:33:59 dyoung Exp $	*/

/*-
 * Copyright (c) 2003, 2004 David Young.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 * 3. The name of David Young may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY David Young ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL David
 * Young BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_media.h>
#ifdef __FreeBSD__
#include <net/ethernet.h>
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
#include <net80211/ieee80211.h>
#include <net80211/ieee80211_compat.h>
#include <net80211/ieee80211_rssadapt.h>

#ifdef interpolate
#undef interpolate
#endif
#define interpolate(parm, old, new) ((parm##_old * (old) + \
                                     (parm##_denom - parm##_old) * (new)) / \
				    parm##_denom)

#ifdef IEEE80211_DEBUG
static	struct timeval lastrateadapt;	/* time of last rate adaptation msg */
static	int currssadaptps = 0;		/* rate-adaptation msgs this second */
static	int ieee80211_adaptrate = 4;	/* rate-adaptation max msgs/sec */

#define RSSADAPT_DO_PRINT() \
	((ieee80211_rssadapt_debug > 0) && \
	 ppsratecheck(&lastrateadapt, &currssadaptps, ieee80211_adaptrate))
#define	RSSADAPT_PRINTF(X) \
	if (RSSADAPT_DO_PRINT()) \
		printf X

int ieee80211_rssadapt_debug = 0;

#else
#define	RSSADAPT_DO_PRINT() (0)
#define	RSSADAPT_PRINTF(X)
#endif

static struct ieee80211_rssadapt_expavgctl master_expavgctl = {
	rc_decay_denom : 16,
	rc_decay_old : 15,
	rc_thresh_denom : 8,
	rc_thresh_old : 4,
	rc_avgrssi_denom : 8,
	rc_avgrssi_old : 4
};

#ifdef __NetBSD__
#ifdef IEEE80211_DEBUG
/* TBD factor with sysctl_ath_verify, sysctl_ieee80211_verify. */
static int
sysctl_ieee80211_rssadapt_debug(SYSCTLFN_ARGS)
{
	int error, t;
	struct sysctlnode node;

	node = *rnode;
	t = *(int*)rnode->sysctl_data;
	node.sysctl_data = &t;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return (error);

	IEEE80211_DPRINTF(("%s: t = %d, nodenum = %d, rnodenum = %d\n",
	    __func__, t, node.sysctl_num, rnode->sysctl_num));

	if (t < 0 || t > 2)
		return (EINVAL);
	*(int*)rnode->sysctl_data = t;

	return (0);
}
#endif /* IEEE80211_DEBUG */

/* TBD factor with sysctl_ath_verify, sysctl_ieee80211_verify. */
static int
sysctl_ieee80211_rssadapt_expavgctl(SYSCTLFN_ARGS)
{
	struct ieee80211_rssadapt_expavgctl rc;
	int error;
	struct sysctlnode node;

	node = *rnode;
	rc = *(struct ieee80211_rssadapt_expavgctl *)rnode->sysctl_data;
	node.sysctl_data = &rc;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return (error);

	IEEE80211_DPRINTF(("%s: decay = %d/%d, thresh = %d/%d, "
	    "avgrssi = %d/%d, nodenum = %d, rnodenum = %d\n",
	    __func__, rc.rc_decay_old, rc.rc_decay_denom,
	    rc.rc_thresh_old, rc.rc_thresh_denom,
	    rc.rc_avgrssi_old, rc.rc_avgrssi_denom,
	    node.sysctl_num, rnode->sysctl_num));

	if (rc.rc_decay_old < 0 ||
	    rc.rc_decay_denom < rc.rc_decay_old)
		return (EINVAL);

	if (rc.rc_thresh_old < 0 ||
	    rc.rc_thresh_denom < rc.rc_thresh_old)
		return (EINVAL);

	if (rc.rc_avgrssi_old < 0 ||
	    rc.rc_avgrssi_denom < rc.rc_avgrssi_old)
		return (EINVAL);

	*(struct ieee80211_rssadapt_expavgctl *)rnode->sysctl_data = rc;

	return (0);
}

/*
 * Setup sysctl(3) MIB, net.ieee80211.*
 *
 * TBD condition CTLFLAG_PERMANENT on being an LKM or not
 */
SYSCTL_SETUP(sysctl_ieee80211_rssadapt,
    "sysctl ieee80211 rssadapt subtree setup")
{
	int rc;
	struct sysctlnode *node;

	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT, CTLTYPE_NODE, "net", NULL,
	    NULL, 0, NULL, 0, CTL_NET, CTL_EOL)) != 0)
		goto err;

	if ((rc = sysctl_createv(clog, 0, &node, &node,
	    CTLFLAG_PERMANENT, CTLTYPE_NODE, "link", NULL,
	    NULL, 0, NULL, 0, PF_LINK, CTL_EOL)) != 0)
		goto err;

	if ((rc = sysctl_createv(clog, 0, &node, &node,
	    CTLFLAG_PERMANENT, CTLTYPE_NODE, "ieee80211", NULL,
	    NULL, 0, NULL, 0, CTL_CREATE, CTL_EOL)) != 0)
		goto err;

	if ((rc = sysctl_createv(clog, 0, &node, &node,
	    CTLFLAG_PERMANENT, CTLTYPE_NODE, "rssadapt",
	    SYSCTL_DESCR("Received Signal Strength adaptation controls"),
	    NULL, 0, NULL, 0, CTL_CREATE, CTL_EOL)) != 0)
		goto err;

#ifdef IEEE80211_DEBUG
	/* control debugging printfs */
	if ((rc = sysctl_createv(clog, 0, &node, NULL,
	    CTLFLAG_PERMANENT|CTLFLAG_READWRITE, CTLTYPE_INT, "debug",
	    SYSCTL_DESCR("Enable RSS adaptation debugging output"),
	    sysctl_ieee80211_rssadapt_debug, 0, &ieee80211_rssadapt_debug, 0,
	    CTL_CREATE, CTL_EOL)) != 0)
		goto err;
#endif /* IEEE80211_DEBUG */

	/* control rate of decay for exponential averages */
	if ((rc = sysctl_createv(clog, 0, &node, NULL,
	    CTLFLAG_PERMANENT|CTLFLAG_READWRITE, CTLTYPE_STRUCT,
	    "expavgctl", SYSCTL_DESCR("RSS exponential averaging control"),
	    sysctl_ieee80211_rssadapt_expavgctl, 0,
	    &master_expavgctl, sizeof(master_expavgctl), CTL_CREATE,
	    CTL_EOL)) != 0)
		goto err;

	return;
err:
	printf("%s: sysctl_createv failed (rc = %d)\n", __func__, rc);
}
#endif /* __NetBSD__ */

int
ieee80211_rssadapt_choose(struct ieee80211_rssadapt *ra,
    struct ieee80211_rateset *rs, struct ieee80211_frame *wh, u_int len,
    int fixed_rate, const char *dvname, int do_not_adapt)
{
	u_int16_t (*thrs)[IEEE80211_RATE_SIZE];
	int flags = 0, i, rateidx = 0, thridx, top;

	if ((wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) == IEEE80211_FC0_TYPE_CTL)
		flags |= IEEE80211_RATE_BASIC;

	for (i = 0, top = IEEE80211_RSSADAPT_BKT0;
	     i < IEEE80211_RSSADAPT_BKTS;
	     i++, top <<= IEEE80211_RSSADAPT_BKTPOWER) {
		thridx = i;
		if (len <= top)
			break;
	}

	thrs = &ra->ra_rate_thresh[thridx];

	if (fixed_rate != -1) {
		if ((rs->rs_rates[fixed_rate] & flags) == flags) {
			rateidx = fixed_rate;
			goto out;
		}
		flags |= IEEE80211_RATE_BASIC;
		i = fixed_rate;
	} else
		i = rs->rs_nrates;

	while (--i >= 0) {
		rateidx = i;
		if ((rs->rs_rates[i] & flags) != flags)
			continue;
		if (do_not_adapt)
			break;
		if ((*thrs)[i] < ra->ra_avg_rssi)
			break;
	}

out:
#ifdef IEEE80211_DEBUG
	if (ieee80211_rssadapt_debug && dvname != NULL) {
		printf("%s: dst %s threshold[%d, %d.%d] %d < %d\n",
		    dvname, ether_sprintf(wh->i_addr1), len,
		    (rs->rs_rates[rateidx] & IEEE80211_RATE_VAL) / 2,
		    (rs->rs_rates[rateidx] & IEEE80211_RATE_VAL) * 5 % 10,
		    (*thrs)[rateidx], ra->ra_avg_rssi);
	}
#endif /* IEEE80211_DEBUG */
	return rateidx;
}

void
ieee80211_rssadapt_updatestats(struct ieee80211_rssadapt *ra)
{
	long interval;

	ra->ra_pktrate =
	    (ra->ra_pktrate + 10 * (ra->ra_nfail + ra->ra_nok)) / 2;
	ra->ra_nfail = ra->ra_nok = 0;

	/* a node is eligible for its rate to be raised every 1/10 to 10
	 * seconds, more eligible in proportion to recent packet rates.
	 */
	interval = MAX(100000, 10000000 / MAX(1, 10 * ra->ra_pktrate));
	ra->ra_raise_interval.tv_sec = interval / (1000 * 1000);
	ra->ra_raise_interval.tv_usec = interval % (1000 * 1000);
}

void
ieee80211_rssadapt_input(struct ieee80211com *ic, struct ieee80211_node *ni,
    struct ieee80211_rssadapt *ra, int rssi)
{
#ifdef IEEE80211_DEBUG
	int last_avg_rssi = ra->ra_avg_rssi;
#endif

	ra->ra_avg_rssi = interpolate(master_expavgctl.rc_avgrssi,
	                              ra->ra_avg_rssi, (rssi << 8));

	RSSADAPT_PRINTF(("%s: src %s rssi %d avg %d -> %d\n",
	    ic->ic_if.if_xname, ether_sprintf(ni->ni_macaddr),
	    rssi, last_avg_rssi, ra->ra_avg_rssi));
}

/*
 * Adapt the data rate to suit the conditions.  When a transmitted
 * packet is dropped after IEEE80211_RSSADAPT_RETRY_LIMIT retransmissions,
 * raise the RSS threshold for transmitting packets of similar length at
 * the same data rate.
 */
void
ieee80211_rssadapt_lower_rate(struct ieee80211com *ic,
    struct ieee80211_node *ni, struct ieee80211_rssadapt *ra,
    struct ieee80211_rssdesc *id)
{
	struct ieee80211_rateset *rs = &ni->ni_rates;
	u_int16_t last_thr;
	u_int i, thridx, top;

	ra->ra_nfail++;

	if (id->id_rateidx >= rs->rs_nrates) {
		RSSADAPT_PRINTF(("ieee80211_rssadapt_lower_rate: "
		    "%s rate #%d > #%d out of bounds\n",
		    ether_sprintf(ni->ni_macaddr), id->id_rateidx,
		        rs->rs_nrates - 1));
		return;
	}

	for (i = 0, top = IEEE80211_RSSADAPT_BKT0;
	     i < IEEE80211_RSSADAPT_BKTS;
	     i++, top <<= IEEE80211_RSSADAPT_BKTPOWER) {
		thridx = i;
		if (id->id_len <= top)
			break;
	}

	last_thr = ra->ra_rate_thresh[thridx][id->id_rateidx];
	ra->ra_rate_thresh[thridx][id->id_rateidx] =
	    interpolate(master_expavgctl.rc_thresh, last_thr,
	                (id->id_rssi << 8));

	RSSADAPT_PRINTF(("%s: dst %s rssi %d threshold[%d, %d.%d] %d -> %d\n",
	    ic->ic_if.if_xname, ether_sprintf(ni->ni_macaddr),
	    id->id_rssi, id->id_len,
	    (rs->rs_rates[id->id_rateidx] & IEEE80211_RATE_VAL) / 2,
	    (rs->rs_rates[id->id_rateidx] & IEEE80211_RATE_VAL) * 5 % 10,
	    last_thr, ra->ra_rate_thresh[thridx][id->id_rateidx]));
}

void
ieee80211_rssadapt_raise_rate(struct ieee80211com *ic,
    struct ieee80211_rssadapt *ra, struct ieee80211_rssdesc *id)
{
	u_int16_t (*thrs)[IEEE80211_RATE_SIZE], newthr, oldthr;
	struct ieee80211_node *ni = id->id_node;
	struct ieee80211_rateset *rs = &ni->ni_rates;
	int i, rate, top;
#ifdef IEEE80211_DEBUG
	int j;
#endif

	ra->ra_nok++;

	if (!ratecheck(&ra->ra_last_raise, &ra->ra_raise_interval))
		return;

	for (i = 0, top = IEEE80211_RSSADAPT_BKT0;
	     i < IEEE80211_RSSADAPT_BKTS;
	     i++, top <<= IEEE80211_RSSADAPT_BKTPOWER) {
		thrs = &ra->ra_rate_thresh[i];
		if (id->id_len <= top)
			break;
	}

	if (id->id_rateidx + 1 < rs->rs_nrates &&
	    (*thrs)[id->id_rateidx + 1] > (*thrs)[id->id_rateidx]) {
		rate = (rs->rs_rates[id->id_rateidx + 1] & IEEE80211_RATE_VAL);

		RSSADAPT_PRINTF(("%s: threshold[%d, %d.%d] decay %d ",
		    ic->ic_if.if_xname,
		    IEEE80211_RSSADAPT_BKT0 << (IEEE80211_RSSADAPT_BKTPOWER* i),
		    rate / 2, rate * 5 % 10, (*thrs)[id->id_rateidx + 1]));
		oldthr = (*thrs)[id->id_rateidx + 1];
		if ((*thrs)[id->id_rateidx] == 0)
			newthr = ra->ra_avg_rssi;
		else
			newthr = (*thrs)[id->id_rateidx];
		(*thrs)[id->id_rateidx + 1] =
		    interpolate(master_expavgctl.rc_decay, oldthr, newthr);

		RSSADAPT_PRINTF(("-> %d\n", (*thrs)[id->id_rateidx + 1]));
	}

#ifdef IEEE80211_DEBUG
	if (RSSADAPT_DO_PRINT()) {
		printf("%s: dst %s thresholds\n", ic->ic_if.if_xname,
		    ether_sprintf(ni->ni_macaddr));
		for (i = 0; i < IEEE80211_RSSADAPT_BKTS; i++) {
			printf("%d-byte", IEEE80211_RSSADAPT_BKT0 << (IEEE80211_RSSADAPT_BKTPOWER * i));
			for (j = 0; j < rs->rs_nrates; j++) {
				rate = (rs->rs_rates[j] & IEEE80211_RATE_VAL);
				printf(", T[%d.%d] = %d", rate / 2,
				    rate * 5 % 10, ra->ra_rate_thresh[i][j]);
			}
			printf("\n");
		}
	}
#endif /* IEEE80211_DEBUG */
}
