/*	$OpenBSD: apme.c,v 1.1 2005/04/13 18:12:23 reyk Exp $	*/

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

void	 hostapd_apme_frame(struct hostapd_config *, u_int8_t *, u_int);

void
hostapd_apme_input(int fd, short sig, void *arg)
{
	struct hostapd_config *cfg = (struct hostapd_config *)arg;
	u_int8_t buf[IAPP_MAXSIZE], *bp, *ep;
	struct bpf_hdr *bph;
	ssize_t len;

	/* Ignore invalid signals */
	if (sig != EV_READ)
		return;

	bzero(&buf, sizeof(buf));

	if ((len = read(fd, buf, sizeof(buf))) <
	    (ssize_t)sizeof(struct ieee80211_frame))
		return;

	/*
	 * Loop through each frame.
	 */

	bp = (u_int8_t *)&buf;
	bph = (struct bpf_hdr *)bp;
	ep = bp + len;

	while (bp < ep) {
		register int caplen, hdrlen;
		caplen = bph->bh_caplen;
		hdrlen = bph->bh_hdrlen;

		/* Process frame */
		hostapd_apme_frame(cfg, bp + hdrlen, caplen);

		bp += BPF_WORDALIGN(caplen + hdrlen);
	}
}

void
hostapd_apme_frame(struct hostapd_config *cfg, u_int8_t *buf, u_int len)
{
	struct hostapd_node node;
	struct ieee80211_frame *wh = (struct ieee80211_frame *)buf;

	/* Ignore short frames or fragments */
	if (len < sizeof(struct ieee80211_frame))
		return;

	/*
	 * Only accept local association response frames, ...
	 */
	if (!((wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) ==
	    IEEE80211_FC1_DIR_NODS &&
	    (wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) ==
	    IEEE80211_FC0_TYPE_MGT &&
	    (wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) ==
	    IEEE80211_FC0_SUBTYPE_ASSOC_RESP))
		return;

	/* 
	 * ...sent by the Host AP (addr2) to our BSSID (addr3)
	 */
	if (bcmp(wh->i_addr2, cfg->c_apme_bssid, IEEE80211_ADDR_LEN) != 0 ||
	    bcmp(wh->i_addr3, cfg->c_apme_bssid, IEEE80211_ADDR_LEN) != 0)
		return;

	cfg->c_stats.cn_rx_apme++;

	/*
	 * Double-check if the station got associated to our Host AP
	 */
	bcopy(wh->i_addr1, node.ni_macaddr, IEEE80211_ADDR_LEN);
	if (hostapd_priv_apme_getnode(cfg, &node) != 0) {
		hostapd_log(HOSTAPD_LOG_DEBUG,
		    "%s/%s: invalid association from %s on the Host AP\n",
		    cfg->c_apme_iface, cfg->c_iapp_iface,
		    ether_ntoa((struct ether_addr*)wh->i_addr1));
		return;
	}

	/* Call ADD.notify handler */
	hostapd_iapp_add_notify(cfg, &node);
}

void
hostapd_apme_init(struct hostapd_config *cfg)
{
	u_int i, dlt;
	struct ifreq ifr;

	cfg->c_apme_raw = hostapd_bpf_open(O_RDONLY);

	cfg->c_apme_rawlen = IAPP_MAXSIZE;
	if (ioctl(cfg->c_apme_raw, BIOCSBLEN, &cfg->c_apme_rawlen) == -1)
		hostapd_fatal("failed to set BPF buffer len \"%s\": %s\n",
		    cfg->c_apme_iface, strerror(errno));

	i = 1;
	if (ioctl(cfg->c_apme_raw, BIOCIMMEDIATE, &i) == -1)
		hostapd_fatal("failed to set BPF immediate mode on \"%s\": %s\n",
		    cfg->c_apme_iface, strerror(errno));

	bzero(&ifr, sizeof(struct ifreq));
	strlcpy(ifr.ifr_name, cfg->c_apme_iface, sizeof(ifr.ifr_name));

	/* This may fail, ignore it */
	ioctl(cfg->c_apme_raw, BIOCPROMISC, NULL);

	/* Associate the wireless network interface to the BPF descriptor */
	if (ioctl(cfg->c_apme_raw, BIOCSETIF, &ifr) == -1)
		hostapd_fatal("failed to set BPF interface \"%s\": %s\n",
		    cfg->c_apme_iface, strerror(errno));

	dlt = IAPP_DLT;
	if (ioctl(cfg->c_apme_raw, BIOCSDLT, &dlt) == -1)
		hostapd_fatal("failed to set BPF link type on \"%s\": %s\n",
		    cfg->c_apme_iface, strerror(errno));

	/* Lock the BPF descriptor, no further configuration */
	if (ioctl(cfg->c_apme_raw, BIOCLOCK, NULL) == -1)
		hostapd_fatal("failed to lock BPF interface on \"%s\": %s\n",
		    cfg->c_apme_iface, strerror(errno));
}
