/*	$OpenBSD: hostapd.h,v 1.1 2005/04/13 18:12:23 reyk Exp $	*/

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

#ifndef _HOSTAPD_H
#define _HOSTAPD_H

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <errno.h>
#include <event.h>
#include <syslog.h>

#include <net80211/ieee80211.h>
#include <net80211/ieee80211_ioctl.h>

#define IEEE80211_IAPP_VERSION	0

/*
 * hostapd (IAPP) <-> Host AP (APME)
 */

#define SIOCS80211IAPP	0
#define SIOCG80211IAPP	1

struct hostapd_node {
	u_int8_t	ni_macaddr[IEEE80211_ADDR_LEN];
	u_int8_t	ni_bssid[IEEE80211_ADDR_LEN];
	u_int32_t	ni_associd;
	u_int16_t	ni_capinfo;
	u_int16_t	ni_flags;
	u_int16_t	ni_rxseq;
	u_int16_t	ni_rssi;
};

/*
 * IAPP <-> IAPP
 */

struct ieee80211_iapp_frame {
	u_int8_t	i_version;
	u_int8_t	i_command;
	u_int16_t	i_identifier;
	u_int16_t	i_length;
} __packed;

enum ieee80211_iapp_frame_type {
	IEEE80211_IAPP_FRAME_ADD_NOTIFY			= 0,
	IEEE80211_IAPP_FRAME_MOVE_NOTIFY		= 1,
	IEEE80211_IAPP_FRAME_MOVE_RESPONSE		= 2,
	IEEE80211_IAPP_FRAME_SEND_SECURITY_BLOCK	= 3,
	IEEE80211_IAPP_FRAME_ACK_SECURITY_BLOCK		= 4,
	IEEE80211_IAPP_FRAME_CACHE_NOTIFY		= 5,
	IEEE80211_IAPP_FRAME_CACHE_RESPONSE		= 6
};

struct ieee80211_iapp_add_notify {
	u_int8_t	a_length;
	u_int8_t	a_reserved;
	u_int8_t	a_macaddr[IEEE80211_ADDR_LEN];
	u_int16_t	a_seqnum;
} __packed;

/*
 * IAPP -> switches (LLC)
 */

struct hostapd_llc {
	struct ether_header x_hdr;
	struct llc x_llc;
} __packed;

#define IAPP_LLC	LLC_XID
#define IAPP_LLC_XID	0x81
#define IAPP_LLC_CLASS	1
#define IAPP_LLC_WINDOW	1 << 1

/*
 * hostapd configuration
 */

struct hostapd_counter {
	u_int64_t	cn_tx_llc;	/* sent LLC messages */
	u_int64_t	cn_rx_iapp;	/* received IAPP messages */
	u_int64_t	cn_tx_iapp;	/* sent IAPP messages */
	u_int64_t	cn_rx_apme;	/* received Host AP messages */
	u_int64_t	cn_tx_apme;	/* sent Host AP messages */
};

struct hostapd_config {
	int			c_apme;
	int			c_apme_raw;
	u_int			c_apme_rawlen;
	struct event		c_apme_ev;
	char			c_apme_iface[IFNAMSIZ];
	int			c_apme_n;
	u_int8_t		c_apme_bssid[IEEE80211_ADDR_LEN];

	u_int16_t		c_iapp;
	int			c_iapp_raw;
	char			c_iapp_iface[IFNAMSIZ];
	int			c_iapp_udp;
	struct event		c_iapp_udp_ev;
	u_int16_t		c_iapp_udp_port;
	struct sockaddr_in	c_iapp_addr;
	struct sockaddr_in	c_iapp_broadcast;
	struct sockaddr_in	c_iapp_multicast;

	u_int8_t		c_flags;

#define HOSTAPD_CFG_F_APME	0x01
#define HOSTAPD_CFG_F_IAPP	0x02
#define HOSTAPD_CFG_F_RAW		0x04
#define HOSTAPD_CFG_F_UDP		0x08
#define HOSTAPD_CFG_F_BRDCAST	0x10
#define HOSTAPD_CFG_F_PRIV	0x20

	struct event		c_priv_ev;

	char			c_config[MAXPATHLEN];
	
	u_int			c_verbose;
	u_int			c_debug;

	struct hostapd_counter	c_stats;
};

#define IAPP_PORT	3517	/* XXX this should be added to /etc/services */
#define IAPP_MCASTADDR	"224.0.1.178"
#define IAPP_DLT	DLT_IEEE802_11
#define IAPP_MAXSIZE	512

#define	HOSTAPD_USER	"_hostapd"

#define HOSTAPD_CONFIG	"/etc/hostapd.conf"

#define HOSTAPD_LOG		0
#define HOSTAPD_LOG_VERBOSE	1
#define HOSTAPD_LOG_DEBUG		2

__BEGIN_DECLS

void	 hostapd_log(u_int, const char *, ...);
void	 hostapd_fatal(const char *, ...);
int	 hostapd_bpf_open(u_int);
void	 hostapd_cleanup(struct hostapd_config *);
int	 hostapd_check_file_secrecy(int, const char *);

int	 hostapd_parse_file(struct hostapd_config *);
int	 hostapd_parse_symset(char *);

void	 hostapd_priv_init(struct hostapd_config *);
int	 hostapd_priv_llc_xid(struct hostapd_config *, struct hostapd_node *);
void	 hostapd_priv_apme_bssid(struct hostapd_config *);
int	 hostapd_priv_apme_getnode(struct hostapd_config *, struct hostapd_node *);
int	 hostapd_priv_apme_delnode(struct hostapd_config *, struct hostapd_node *);

void	 hostapd_apme_init(struct hostapd_config *);
void	 hostapd_apme_input(int, short, void *);

void	 hostapd_iapp_init(struct hostapd_config *);
void	 hostapd_iapp_term(struct hostapd_config *);
int	 hostapd_iapp_add_notify(struct hostapd_config *, struct hostapd_node *);
void	 hostapd_iapp_input(int, short, void *);

void	 hostapd_llc_init(struct hostapd_config *);
int	 hostapd_llc_send_xid(struct hostapd_config *, struct hostapd_node *);

__END_DECLS

#endif /* _HOSTAPD_H */
