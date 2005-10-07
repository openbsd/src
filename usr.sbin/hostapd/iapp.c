/*	$OpenBSD: iapp.c,v 1.8 2005/10/07 22:32:52 reyk Exp $	*/

/*
 * Copyright (c) 2004, 2005 Reyk Floeter <reyk@vantronix.net>
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
#include <sys/uio.h>

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
#include <string.h>
#include <unistd.h>

#include "hostapd.h"
#include "iapp.h"

void
hostapd_iapp_init(struct hostapd_config *cfg)
{
	if ((cfg->c_flags & HOSTAPD_CFG_F_APME) == 0)
		return;

	/* Get Host AP's BSSID */
	hostapd_priv_apme_bssid(cfg);

	hostapd_log(HOSTAPD_LOG_VERBOSE,
	    "%s: attaching to Host AP %s with BSSID \"%s\"\n",
	    cfg->c_iapp_iface, cfg->c_apme_iface,
	    etheraddr_string(cfg->c_apme_bssid));
}

void
hostapd_iapp_term(struct hostapd_config *cfg)
{
	if ((cfg->c_flags & HOSTAPD_CFG_F_APME) == 0)
		return;

	/* XXX not yet used but inspired by the APME TERMINATE action */
	hostapd_log(HOSTAPD_LOG_VERBOSE, "%s: detaching from Host AP %s\n",
	    cfg->c_iapp_iface, cfg->c_apme_iface);
}

int
hostapd_iapp_add_notify(struct hostapd_config *cfg, struct hostapd_node *node)
{
	struct sockaddr_in *addr;
	struct {
		struct ieee80211_iapp_frame hdr;
		struct ieee80211_iapp_add_notify add;
	} __packed frame;

	/*
	 * Send an ADD.notify message to other accesspoints to notify
	 * about a new association on our Host AP.
	 */
	bzero(&frame, sizeof(frame));

	frame.hdr.i_version = IEEE80211_IAPP_VERSION;
	frame.hdr.i_command = IEEE80211_IAPP_FRAME_ADD_NOTIFY;
	frame.hdr.i_identifier = htons(cfg->c_iapp++);
	frame.hdr.i_length = sizeof(struct ieee80211_iapp_add_notify);

	frame.add.a_length = IEEE80211_ADDR_LEN;
	frame.add.a_seqnum = htons(node->ni_rxseq);
	bcopy(node->ni_macaddr, frame.add.a_macaddr, IEEE80211_ADDR_LEN);

	if (cfg->c_flags & HOSTAPD_CFG_F_BRDCAST)
		addr = &cfg->c_iapp_broadcast;
	else
		addr = &cfg->c_iapp_multicast;

	if (sendto(cfg->c_iapp_udp, &frame, sizeof(frame),
	    0, (struct sockaddr *)addr, sizeof(struct sockaddr_in)) == -1) {
		hostapd_log(HOSTAPD_LOG,
		    "%s: failed to send ADD notification: %s\n",
		    cfg->c_iapp_iface, strerror(errno));
		return (errno);
	}

	hostapd_log(HOSTAPD_LOG, "%s: sent ADD notification for %s\n",
	    cfg->c_iapp_iface, etheraddr_string(frame.add.a_macaddr));

	/* Send a LLC XID frame, see llc.c for details */
	return (hostapd_priv_llc_xid(cfg, node));
}

int
hostapd_iapp_radiotap(struct hostapd_config *cfg, u_int8_t *buf,
    const u_int len)
{
	struct sockaddr_in *addr;
	struct ieee80211_iapp_frame hdr;
	struct msghdr msg;
	struct iovec iov[2];

	/*
	 * Send an HOSTAPD.pcap/radiotap message to other accesspoints with
	 * with an appended network dump. This is an hostapd extension to
	 * IAPP.
	 */
	bzero(&hdr, sizeof(hdr));

	hdr.i_version = IEEE80211_IAPP_VERSION;
	if (cfg->c_apme_dlt == DLT_IEEE802_11_RADIO)
		hdr.i_command = IEEE80211_IAPP_FRAME_HOSTAPD_RADIOTAP;
	else if (cfg->c_apme_dlt == DLT_IEEE802_11)
		hdr.i_command = IEEE80211_IAPP_FRAME_HOSTAPD_PCAP;
	else
		return (EINVAL);
	hdr.i_identifier = htons(cfg->c_iapp++);
	hdr.i_length = len;

	if (cfg->c_flags & HOSTAPD_CFG_F_BRDCAST)
		addr = &cfg->c_iapp_broadcast;
	else
		addr = &cfg->c_iapp_multicast;

	iov[0].iov_base = &hdr;
	iov[0].iov_len = sizeof(hdr);
	iov[1].iov_base = buf;
	iov[1].iov_len = len;
	msg.msg_name = (caddr_t)addr;
	msg.msg_namelen = sizeof(struct sockaddr_in);
	msg.msg_iov = iov;
	msg.msg_iovlen = 2;
	msg.msg_control = 0;
	msg.msg_controllen = 0;
	msg.msg_flags = 0;

	if (sendmsg(cfg->c_iapp_udp, &msg, 0) == -1) {
		hostapd_log(HOSTAPD_LOG,
		    "%s: failed to send HOSTAPD %s: %s\n",
		    cfg->c_iapp_iface, cfg->c_apme_dlt ==
		    DLT_IEEE802_11_RADIO ? "radiotap" : "pcap",
		    strerror(errno));
		return (errno);
	}

	return (0);
}

void
hostapd_iapp_input(int fd, short sig, void *arg)
{
	struct hostapd_config *cfg = (struct hostapd_config *)arg;
	struct sockaddr_in addr;
	socklen_t addr_len;
	ssize_t len;
	u_int8_t buf[IAPP_MAXSIZE];
	struct hostapd_node node;
	struct ieee80211_iapp_recv {
		struct ieee80211_iapp_frame hdr;
		union {
			struct ieee80211_iapp_add_notify add;
			u_int8_t buf[1];
		} u;
	} __packed *frame;
	u_int dlt;
	int ret = 0;

	/* Ignore invalid signals */
	if (sig != EV_READ)
		return;

	/*
	 * Listen to possible messages from other IAPP
	 */
	bzero(buf, sizeof(buf));

	if ((len = recvfrom(fd, buf, sizeof(buf), 0,
	    (struct sockaddr*)&addr, &addr_len)) < 1)
		return;

	if (bcmp(&cfg->c_iapp_addr.sin_addr, &addr.sin_addr,
	    sizeof(addr.sin_addr)) == 0)
		return;

	frame = (struct ieee80211_iapp_recv*)buf;

	/* Validate the IAPP version */
	if (len < (ssize_t)sizeof(struct ieee80211_iapp_frame) ||
	    frame->hdr.i_version != IEEE80211_IAPP_VERSION ||
	    addr_len < sizeof(struct sockaddr_in))
		return;

	cfg->c_stats.cn_rx_iapp++;

	/*
	 * Process the IAPP frame
	 */
	switch (frame->hdr.i_command) {
	case IEEE80211_IAPP_FRAME_ADD_NOTIFY:
		/* Short frame */
		if (len < (ssize_t)(sizeof(struct ieee80211_iapp_frame) +
		    sizeof(struct ieee80211_iapp_add_notify)))
			return;

		/* Don't support non-48bit MAC addresses, yet */
		if (frame->u.add.a_length != IEEE80211_ADDR_LEN)
			return;

		node.ni_rxseq = frame->u.add.a_seqnum;
		bcopy(frame->u.add.a_macaddr, node.ni_macaddr,
		    IEEE80211_ADDR_LEN);

		/*
		 * Try to remove a node from our Host AP and to free
		 * any allocated resources. Otherwise the received
		 * ADD.notify message will be ignored.
		 */
		if (cfg->c_flags & HOSTAPD_CFG_F_APME) {
			if ((ret = hostapd_apme_delnode(cfg, &node)) == 0)
				cfg->c_stats.cn_tx_apme++;
		} else
			ret = 0;

		hostapd_log(HOSTAPD_LOG, "%s: %s ADD notification "
		    "for %s at %s\n",
		    cfg->c_apme_iface, ret == 0 ?
		    "received" : "ignored",
		    etheraddr_string(node.ni_macaddr),
		    inet_ntoa(addr.sin_addr));
		break;

	case IEEE80211_IAPP_FRAME_HOSTAPD_PCAP:
	case IEEE80211_IAPP_FRAME_HOSTAPD_RADIOTAP:
		/* Short frame */
		if (len <= (ssize_t)sizeof(struct ieee80211_iapp_frame) ||
		    frame->hdr.i_length < sizeof(struct ieee80211_frame))
			return;

		dlt = frame->hdr.i_command ==
		    IEEE80211_IAPP_FRAME_HOSTAPD_PCAP ?
		    DLT_IEEE802_11 : DLT_IEEE802_11_RADIO;

		hostapd_print_ieee80211(dlt, 1, (u_int8_t *)frame->u.buf,
		    len - sizeof(struct ieee80211_iapp_frame));
		return;

	case IEEE80211_IAPP_FRAME_MOVE_NOTIFY:
	case IEEE80211_IAPP_FRAME_MOVE_RESPONSE:
	case IEEE80211_IAPP_FRAME_SEND_SECURITY_BLOCK:
	case IEEE80211_IAPP_FRAME_ACK_SECURITY_BLOCK:
	case IEEE80211_IAPP_FRAME_CACHE_NOTIFY:
	case IEEE80211_IAPP_FRAME_CACHE_RESPONSE:

		/*
		 * XXX TODO
		 */

		hostapd_log(HOSTAPD_LOG_VERBOSE,
		    "%s: received unsupported IAPP message %d\n",
		    cfg->c_iapp_iface, frame->hdr.i_command);
		return;

	default:
		return;
	}
}
