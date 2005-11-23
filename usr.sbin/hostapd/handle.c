/*	$OpenBSD: handle.c,v 1.6 2005/11/23 20:40:38 reyk Exp $	*/

/*
 * Copyright (c) 2005 Reyk Floeter <reyk@vantronix.net>
 *
 * Permission to use, copy, modify, and distribute this software for any
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

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_arp.h>
#include <net/if_llc.h>
#include <net/bpf.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <arpa/inet.h>

#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "hostapd.h"

int	 hostapd_handle_frame(struct hostapd_apme *, struct hostapd_frame *,
	    u_int8_t *, const u_int);
int	 hostapd_handle_action(struct hostapd_apme *, struct hostapd_frame *,
	    u_int8_t *, u_int8_t *, u_int8_t *, u_int8_t *, const u_int);
void	 hostapd_handle_addr(const u_int32_t, u_int32_t *, u_int8_t *,
	    u_int8_t *, struct hostapd_table *);
void	 hostapd_handle_ref(u_int, u_int, u_int8_t *, u_int8_t *, u_int8_t *,
	    u_int8_t *);

int
hostapd_handle_input(struct hostapd_apme *apme, u_int8_t *buf, u_int len)
{
	struct hostapd_config *cfg = (struct hostapd_config *)apme->a_cfg;
	struct hostapd_frame *frame;
	int ret;

	TAILQ_FOREACH(frame, &cfg->c_frames, f_entries) {
		if ((ret = hostapd_handle_frame(apme, frame, buf, len)) != 0)
			return (ret);
	}

	return (0);
}

void
hostapd_handle_addr(const u_int32_t mask, u_int32_t *flags, u_int8_t *addr,
    u_int8_t *maddr, struct hostapd_table *table)
{
	int ret = 0;

	if ((*flags & mask) & HOSTAPD_FRAME_TABLE) {
		if (hostapd_entry_lookup(table, addr) == NULL)
			ret = 1;
	} else if (bcmp(addr, maddr, IEEE80211_ADDR_LEN) != 0)
			ret = 1;

	if ((ret == 1 && (*flags & mask) & HOSTAPD_FRAME_N) ||
	    (ret == 0 && ((*flags & mask) & HOSTAPD_FRAME_N) == 0))
		*flags &= ~mask;
}

void
hostapd_handle_ref(u_int flags, u_int shift, u_int8_t *wfrom, u_int8_t *wto,
    u_int8_t *wbssid, u_int8_t *addr)
{
	if (flags & (HOSTAPD_ACTION_F_REF_FROM << shift))
		bcopy(wfrom, addr, IEEE80211_ADDR_LEN);
	else if (flags & (HOSTAPD_ACTION_F_REF_TO << shift))
		bcopy(wto, addr, IEEE80211_ADDR_LEN);
	else if (flags & (HOSTAPD_ACTION_F_REF_BSSID << shift))
		bcopy(wbssid, addr, IEEE80211_ADDR_LEN);
	else if (flags & (HOSTAPD_ACTION_F_REF_RANDOM << shift)) {
		hostapd_randval(addr, IEEE80211_ADDR_LEN);
		/* Avoid multicast/broadcast addresses */
		addr[0] &= ~0x1;
	}
}

int
hostapd_handle_frame(struct hostapd_apme *apme, struct hostapd_frame *frame,
    u_int8_t *buf, const u_int len)
{
	struct ieee80211_frame *wh;
	struct hostapd_ieee80211_frame *mh;
	u_int8_t *wfrom, *wto, *wbssid;
	struct timeval t_now;
	u_int32_t flags;
	int offset, min_rate = 0;

	if ((offset = hostapd_apme_offset(apme, buf, len)) < 0)
		return (0);
	wh = (struct ieee80211_frame *)(buf + offset);

	mh = &frame->f_frame;
	flags = frame->f_flags;

	/* Get timestamp */
	gettimeofday(&t_now, NULL);

	/* Handle optional limit */
	if (frame->f_limit.tv_sec || frame->f_limit.tv_usec) {
		if (timercmp(&t_now, &frame->f_then, <))
			return (0);
		timeradd(&t_now, &frame->f_limit, &frame->f_then);
	}

	switch (wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) {
	case IEEE80211_FC1_DIR_NODS:
		wfrom = wh->i_addr2;
		wto = wh->i_addr1;
		wbssid = wh->i_addr3;
		break;
	case IEEE80211_FC1_DIR_TODS:
		wfrom = wh->i_addr2;
		wto = wh->i_addr3;
		wbssid = wh->i_addr1;
		break;
	case IEEE80211_FC1_DIR_FROMDS:
		wfrom = wh->i_addr3;
		wto = wh->i_addr1;
		wbssid = wh->i_addr2;
		break;
	default:
	case IEEE80211_FC1_DIR_DSTODS:
		return (0);
	}

	if (flags & HOSTAPD_FRAME_F_APME_M) {
		if (frame->f_apme == NULL)
			return (0);
		/* Match hostap interface */
		if ((flags & HOSTAPD_FRAME_F_APME &&
		    apme == frame->f_apme) ||
		    (flags & HOSTAPD_FRAME_F_APME_N &&
		    apme != frame->f_apme))
			flags &= ~HOSTAPD_FRAME_F_APME_M;
	}

	if (flags & HOSTAPD_FRAME_F_TYPE) {
		/* type $type */
		if ((wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) ==
		    (mh->i_fc[0] & IEEE80211_FC0_TYPE_MASK))
			flags &= ~HOSTAPD_FRAME_F_TYPE;
	} else if (flags & HOSTAPD_FRAME_F_TYPE_N) {
		/* type !$type */
		if ((wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) !=
		    (mh->i_fc[0] & IEEE80211_FC0_TYPE_MASK))
			flags &= ~HOSTAPD_FRAME_F_TYPE_N;
	}

	if (flags & HOSTAPD_FRAME_F_SUBTYPE) {
		/* subtype $subtype */
		if ((wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) ==
		    (mh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK))
			flags &= ~HOSTAPD_FRAME_F_SUBTYPE;
	} else if (flags & HOSTAPD_FRAME_F_SUBTYPE_N) {
		/* subtype !$subtype */
		if ((wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) !=
		    (mh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK))
			flags &= ~HOSTAPD_FRAME_F_SUBTYPE_N;
	}

	if (flags & HOSTAPD_FRAME_F_DIR) {
		/* dir $dir */
		if ((wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) ==
		    (mh->i_fc[1] & IEEE80211_FC1_DIR_MASK))
			flags &= ~HOSTAPD_FRAME_F_DIR;
	} else if (flags & HOSTAPD_FRAME_F_DIR_N) {
		/* dir !$dir */
		if ((wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) !=
		    (mh->i_fc[1] & IEEE80211_FC1_DIR_MASK))
			flags &= ~HOSTAPD_FRAME_F_DIR_N;
	}

	/* from/to/bssid [!]$addr/<table> */
	hostapd_handle_addr(HOSTAPD_FRAME_F_FROM_M, &flags, wfrom,
	    mh->i_from, frame->f_from);
	hostapd_handle_addr(HOSTAPD_FRAME_F_TO_M, &flags, wto,
	    mh->i_to, frame->f_to);
	hostapd_handle_addr(HOSTAPD_FRAME_F_BSSID_M, &flags, wbssid,
	    mh->i_bssid, frame->f_bssid);

	/* Handle if frame matches */
	if ((flags & HOSTAPD_FRAME_F_M) != 0)
		return (0);

	/* Handle optional minimal rate */
	if (frame->f_rate && frame->f_rate_intval) {
		frame->f_rate_delay = t_now.tv_sec - frame->f_last.tv_sec;
		if (frame->f_rate_delay < frame->f_rate_intval) {
			frame->f_rate_cnt++;
			if (frame->f_rate_cnt < frame->f_rate)
				min_rate = 1;
		} else {
			min_rate = 1;
			frame->f_rate_cnt = 0;
		}
	}

	/* Update timestamp for the last match of this event */
	if (frame->f_rate_cnt == 0 || min_rate == 0)
		bcopy(&t_now, &frame->f_last, sizeof(struct timeval));

	/* Return if the minimal rate is not reached, yet */
	if (min_rate)
		return (0);

	if (hostapd_handle_action(apme, frame, wfrom, wto, wbssid, buf,
	    len) != 0)
		return (0);

	/* Reset minimal rate counter after successfully handled the frame */
	frame->f_rate_cnt = 0;

	return ((frame->f_flags & HOSTAPD_FRAME_F_RET_M) >>
	    HOSTAPD_FRAME_F_RET_S);
}

int
hostapd_handle_action(struct hostapd_apme *apme, struct hostapd_frame *frame,
    u_int8_t *wfrom, u_int8_t *wto, u_int8_t *wbssid, u_int8_t *buf,
    const u_int len)
{
	struct hostapd_config *cfg = (struct hostapd_config *)apme->a_cfg;
	struct hostapd_action_data *action = &frame->f_action_data;
	struct hostapd_node node;
	u_int8_t *lladdr = NULL;
	int ret = 0, offset;

	switch (frame->f_action) {
	case HOSTAPD_ACTION_RADIOTAP:
		/* Send IAPP frame with radiotap/pcap payload */
		if ((ret = hostapd_iapp_radiotap(apme, buf, len)) != 0)
			return (ret);

		if ((frame->f_action_flags & HOSTAPD_ACTION_VERBOSE) == 0)
			return (0);

		hostapd_log(HOSTAPD_LOG,
		    "%s: sent IAPP frame HOSTAPD_%s (%u bytes)\n",
		    cfg->c_iapp_iface, cfg->c_apme_dlt ==
		    DLT_IEEE802_11_RADIO ? "RADIOTAP" : "PCAP", len);
		break;

	case HOSTAPD_ACTION_LOG:
		/* Log frame to syslog/stderr */
		if (frame->f_rate && frame->f_rate_intval) {
			hostapd_printf("%s: (rate: %ld/%ld sec) ",
			    apme->a_iface, frame->f_rate_cnt,
			    frame->f_rate_delay + 1);
		} else
			hostapd_printf("%s: ", apme->a_iface);

		hostapd_print_ieee80211(cfg->c_apme_dlt, frame->f_action_flags &
		    HOSTAPD_ACTION_VERBOSE, buf, len);

		/* Flush output buffer */
		hostapd_printf(NULL);
		break;

	case HOSTAPD_ACTION_DELNODE:
	case HOSTAPD_ACTION_ADDNODE:
		bzero(&node, sizeof(node));

		if (action->a_flags & HOSTAPD_ACTION_F_REF_FROM)
			lladdr = wfrom;
		else if (action->a_flags & HOSTAPD_ACTION_F_REF_TO)
			lladdr = wto;
		else if (action->a_flags & HOSTAPD_ACTION_F_REF_BSSID)
			lladdr = wbssid;
		else
			lladdr = action->a_lladdr;

		bcopy(lladdr, &node.ni_macaddr, IEEE80211_ADDR_LEN);

		if (frame->f_action == HOSTAPD_ACTION_DELNODE)
			ret = hostapd_apme_delnode(apme, &node);
		else
			ret = hostapd_apme_addnode(apme, &node);

		if (ret != 0)  {
			hostapd_log(HOSTAPD_LOG_DEBUG,
			    "%s: node add/delete %s failed: %s\n",
			    apme->a_iface, etheraddr_string(lladdr),
			    strerror(ret));
		}
		break;

	case HOSTAPD_ACTION_NONE:
		/* Nothing */
		break;

	case HOSTAPD_ACTION_RESEND:
		/* Resend received raw IEEE 802.11 frame */
		if ((offset = hostapd_apme_offset(apme, buf, len)) < 0)
			return (EINVAL);
		if (write(apme->a_raw, buf + offset, len - offset) == -1)
			ret = errno;
		break;

	case HOSTAPD_ACTION_FRAME:
		if (action->a_flags & HOSTAPD_ACTION_F_REF_M) {
			hostapd_handle_ref(action->a_flags &
			    HOSTAPD_ACTION_F_REF_FROM_M,
			    HOSTAPD_ACTION_F_REF_FROM_S, wfrom, wto, wbssid,
			    action->a_frame.i_from);
			hostapd_handle_ref(action->a_flags &
			    HOSTAPD_ACTION_F_REF_TO_M,
			    HOSTAPD_ACTION_F_REF_TO_S, wfrom, wto, wbssid,
			    action->a_frame.i_to);
			hostapd_handle_ref(action->a_flags &
			    HOSTAPD_ACTION_F_REF_BSSID_M,
			    HOSTAPD_ACTION_F_REF_BSSID_S, wfrom, wto, wbssid,
			    action->a_frame.i_bssid);
		}

		/* Send a raw IEEE 802.11 frame */
		return (hostapd_apme_output(apme, &action->a_frame));

	default:
		return (0);
	}

	return (ret);
}
