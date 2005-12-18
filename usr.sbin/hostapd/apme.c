/*	$OpenBSD: apme.c,v 1.10 2005/12/18 17:54:12 reyk Exp $	*/

/*
 * Copyright (c) 2004, 2005 Reyk Floeter <reyk@openbsd.org>
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

#include <net80211/ieee80211_radiotap.h>

#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "hostapd.h"
#include "iapp.h"

void	 hostapd_apme_frame(struct hostapd_apme *, u_int8_t *, u_int);

int
hostapd_apme_add(struct hostapd_config *cfg, const char *name)
{
	struct hostapd_apme *apme;

	if (hostapd_apme_lookup(cfg, name) != NULL)
		return (EEXIST);
	if ((apme = (struct hostapd_apme *)
	    calloc(1, sizeof(struct hostapd_apme))) == NULL)
		return (ENOMEM);

	strlcpy(apme->a_iface, name, sizeof(apme->a_iface));
	apme->a_cfg = cfg;

	TAILQ_INSERT_TAIL(&cfg->c_apmes, apme, a_entries);

	hostapd_log(HOSTAPD_LOG_DEBUG,
	    "%s: Host AP interface added\n", apme->a_iface);

	return (0);
}

int
hostapd_apme_deauth(struct hostapd_apme *apme)
{
	struct hostapd_config *cfg = (struct hostapd_config *)apme->a_cfg;
	struct hostapd_iapp *iapp = &cfg->c_iapp;
	u_int8_t buf[sizeof(struct ieee80211_frame) + sizeof(u_int16_t)];
	struct ieee80211_frame *wh;

	bzero(&buf, sizeof(buf));
	wh = (struct ieee80211_frame *)&buf[0];
	wh->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_MGT |
	    IEEE80211_FC0_SUBTYPE_DEAUTH;
	wh->i_fc[1] = IEEE80211_FC1_DIR_NODS;
	memset(&wh->i_addr1, 0xff, IEEE80211_ADDR_LEN);
	bcopy(apme->a_bssid, wh->i_addr2, IEEE80211_ADDR_LEN);
	bcopy(apme->a_bssid, wh->i_addr3, IEEE80211_ADDR_LEN);
	*(u_int16_t *)(wh + 1) = htole16(IEEE80211_REASON_AUTH_EXPIRE);

	if (write(apme->a_raw, buf, sizeof(buf)) == -1) {
		hostapd_log(HOSTAPD_LOG_VERBOSE,
		    "%s/%s: failed to deauthenticate all stations: %s\n",
		    iapp->i_iface, apme->a_iface,
		    strerror(errno));
		return (EIO);
	}

	hostapd_log(HOSTAPD_LOG_VERBOSE,
	    "%s/%s: deauthenticated all stations\n",
	    apme->a_iface, iapp->i_iface);

	return (0);
}

struct hostapd_apme *
hostapd_apme_lookup(struct hostapd_config *cfg, const char *name)
{
	struct hostapd_apme *apme;

	TAILQ_FOREACH(apme, &cfg->c_apmes, a_entries) {
		if (strcmp(name, apme->a_iface) == 0)
			return (apme);
	}

	return (NULL);
}

void
hostapd_apme_term(struct hostapd_apme *apme)
{
	struct hostapd_config *cfg = (struct hostapd_config *)apme->a_cfg;

	/* Kick a specified Host AP interface */
	event_del(&apme->a_ev);
	close(apme->a_raw);

	TAILQ_REMOVE(&cfg->c_apmes, apme, a_entries);

	hostapd_log(HOSTAPD_LOG_DEBUG,
	    "%s: Host AP interface removed\n", apme->a_iface);

	free(apme);
}

void
hostapd_apme_input(int fd, short sig, void *arg)
{
	struct hostapd_apme *apme = (struct hostapd_apme *)arg;
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
		register u_int caplen, hdrlen;
		caplen = bph->bh_caplen;
		hdrlen = bph->bh_hdrlen;

		/* Process frame */
		hostapd_apme_frame(apme, bp + hdrlen, caplen);

		bp += BPF_WORDALIGN(caplen + hdrlen);
	}
}

int
hostapd_apme_output(struct hostapd_apme *apme,
    struct hostapd_ieee80211_frame *frame)
{
	struct iovec iov[2];
	int iovcnt;
	struct ieee80211_frame wh;

	bzero(&wh, sizeof(wh));

	switch (frame->i_fc[1] & IEEE80211_FC1_DIR_MASK) {
	case IEEE80211_FC1_DIR_NODS:
		bcopy(frame->i_from, wh.i_addr2, IEEE80211_ADDR_LEN);
		bcopy(frame->i_to, wh.i_addr1, IEEE80211_ADDR_LEN);
		bcopy(frame->i_bssid, wh.i_addr3, IEEE80211_ADDR_LEN);
		break;
	case IEEE80211_FC1_DIR_TODS:
		bcopy(frame->i_from, wh.i_addr2, IEEE80211_ADDR_LEN);
		bcopy(frame->i_to, wh.i_addr3, IEEE80211_ADDR_LEN);
		bcopy(frame->i_bssid, wh.i_addr1, IEEE80211_ADDR_LEN);
		break;
	case IEEE80211_FC1_DIR_FROMDS:
		bcopy(frame->i_from, wh.i_addr3, IEEE80211_ADDR_LEN);
		bcopy(frame->i_to, wh.i_addr1, IEEE80211_ADDR_LEN);
		bcopy(frame->i_bssid, wh.i_addr2, IEEE80211_ADDR_LEN);
		break;
	default:
	case IEEE80211_FC1_DIR_DSTODS:
		return (EINVAL);
	}

	wh.i_fc[0] = IEEE80211_FC0_VERSION_0 | frame->i_fc[0];
	wh.i_fc[1] = frame->i_fc[1];
	bcopy(frame->i_dur, wh.i_dur, sizeof(wh.i_dur));
	bcopy(frame->i_seq, wh.i_seq, sizeof(wh.i_seq));

	iovcnt = 1;
	iov[0].iov_base = &wh;
	iov[0].iov_len = sizeof(struct ieee80211_frame);

	if (frame->i_data != NULL && frame->i_data_len > 0) {
		iovcnt = 2;
		iov[1].iov_base = frame->i_data;
		iov[1].iov_len = frame->i_data_len;
	}

	if (writev(apme->a_raw, iov, iovcnt) == -1)
		return (errno);

	return (0);
}

int
hostapd_apme_offset(struct hostapd_apme *apme,
    u_int8_t *buf, const u_int len)
{
	struct hostapd_config *cfg = (struct hostapd_config *)apme->a_cfg;
	struct ieee80211_radiotap_header *rh;
	u_int rh_len;

	if (cfg->c_apme_dlt == DLT_IEEE802_11)
		return (0);
	else if (cfg->c_apme_dlt != DLT_IEEE802_11_RADIO)
		return (-1);

	if (len < sizeof(struct ieee80211_radiotap_header))
		return (-1);

	rh = (struct ieee80211_radiotap_header*)buf;
	rh_len = letoh16(rh->it_len);

	if (rh->it_version != 0)
		return (-1);
	if (len <= rh_len)
		return (-1);

	return ((int)rh_len);
}

void
hostapd_apme_frame(struct hostapd_apme *apme, u_int8_t *buf, u_int len)
{
	struct hostapd_config *cfg = (struct hostapd_config *)apme->a_cfg;
	struct hostapd_apme *other_apme;
	struct hostapd_node node;
	struct ieee80211_frame *wh;
	int offset;

	if ((offset = hostapd_apme_offset(apme, buf, len)) < 0)
		return;
	wh = (struct ieee80211_frame *)(buf + offset);

	/* Ignore short frames or fragments */
	if (len < sizeof(struct ieee80211_frame))
		return;

	/* Handle received frames */
	if ((hostapd_handle_input(apme, buf, len) ==
	    (HOSTAPD_FRAME_F_RET_SKIP >> HOSTAPD_FRAME_F_RET_S)) ||
	    cfg->c_flags & HOSTAPD_CFG_F_IAPP_PASSIVE)
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
	if (bcmp(wh->i_addr2, apme->a_bssid, IEEE80211_ADDR_LEN) != 0 ||
	    bcmp(wh->i_addr3, apme->a_bssid, IEEE80211_ADDR_LEN) != 0)
		return;

	cfg->c_stats.cn_rx_apme++;

	/*
	 * Double-check if the station got associated to our Host AP
	 */
	bcopy(wh->i_addr1, node.ni_macaddr, IEEE80211_ADDR_LEN);
	if (hostapd_priv_apme_getnode(apme, &node) != 0) {
		hostapd_log(HOSTAPD_LOG_DEBUG,
		    "%s: invalid association from %s on the Host AP\n",
		    apme->a_iface, etheraddr_string(wh->i_addr1));
		return;
	}
	cfg->c_stats.cn_tx_apme++;

	/*
	 * Delete node on other attached Host APs
	 */
	TAILQ_FOREACH(other_apme, &cfg->c_apmes, a_entries) {
		if (apme == other_apme)
			continue;
		if (hostapd_apme_delnode(other_apme, &node) == 0)
			cfg->c_stats.cn_tx_apme++;
	}

	hostapd_iapp_add_notify(apme, &node);

}

void
hostapd_apme_init(struct hostapd_apme *apme)
{
	struct hostapd_config *cfg = (struct hostapd_config *)apme->a_cfg;
	u_int i, dlt;
	struct ifreq ifr;

	apme->a_raw = hostapd_bpf_open(O_RDWR);

	apme->a_rawlen = IAPP_MAXSIZE;
	if (ioctl(apme->a_raw, BIOCSBLEN, &apme->a_rawlen) == -1)
		hostapd_fatal("failed to set BPF buffer len \"%s\": %s\n",
		    apme->a_iface, strerror(errno));

	i = 1;
	if (ioctl(apme->a_raw, BIOCIMMEDIATE, &i) == -1)
		hostapd_fatal("failed to set BPF immediate mode on \"%s\": "
		    "%s\n", apme->a_iface, strerror(errno));

	bzero(&ifr, sizeof(struct ifreq));
	strlcpy(ifr.ifr_name, apme->a_iface, sizeof(ifr.ifr_name));

	/* This may fail, ignore it */
	ioctl(apme->a_raw, BIOCPROMISC, NULL);

	/* Associate the wireless network interface to the BPF descriptor */
	if (ioctl(apme->a_raw, BIOCSETIF, &ifr) == -1)
		hostapd_fatal("failed to set BPF interface \"%s\": %s\n",
		    apme->a_iface, strerror(errno));

	dlt = cfg->c_apme_dlt;
	if (ioctl(apme->a_raw, BIOCSDLT, &dlt) == -1)
		hostapd_fatal("failed to set BPF link type on \"%s\": %s\n",
		    apme->a_iface, strerror(errno));

	/* Lock the BPF descriptor, no further configuration */
	if (ioctl(apme->a_raw, BIOCLOCK, NULL) == -1)
		hostapd_fatal("failed to lock BPF interface on \"%s\": %s\n",
		    apme->a_iface, strerror(errno));
}

int
hostapd_apme_addnode(struct hostapd_apme *apme, struct hostapd_node *node)
{
	return (hostapd_priv_apme_setnode(apme, node, 1));
}

int
hostapd_apme_delnode(struct hostapd_apme *apme, struct hostapd_node *node)
{
	return (hostapd_priv_apme_setnode(apme, node, 0));
}
