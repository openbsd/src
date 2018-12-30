/*	$OpenBSD: bgpd.h,v 1.361 2018/12/30 13:53:07 denis Exp $ */

/*
 * Copyright (c) 2003, 2004 Henning Brauer <henning@openbsd.org>
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
#ifndef __BGPD_H__
#define	__BGPD_H__

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/tree.h>
#include <net/route.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <net/pfkeyv2.h>

#include <poll.h>
#include <stdarg.h>

#include <imsg.h>

#define	BGP_VERSION			4
#define	BGP_PORT			179
#define	CONFFILE			"/etc/bgpd.conf"
#define	BGPD_USER			"_bgpd"
#define	PEER_DESCR_LEN			32
#define	SHUT_COMM_LEN			129
#define	PFTABLE_LEN			32
#define	TCP_MD5_KEY_LEN			80
#define	IPSEC_ENC_KEY_LEN		32
#define	IPSEC_AUTH_KEY_LEN		20
#define	SET_NAME_LEN			64

#define	MAX_PKTSIZE			4096
#define	MIN_HOLDTIME			3
#define	READ_BUF_SIZE			65535
#define	RT_BUF_SIZE			16384
#define	MAX_RTSOCK_BUF			(2 * 1024 * 1024)
#define	MAX_COMM_MATCH			3

#define	BGPD_OPT_VERBOSE		0x0001
#define	BGPD_OPT_VERBOSE2		0x0002
#define	BGPD_OPT_NOACTION		0x0004
#define	BGPD_OPT_FORCE_DEMOTE		0x0008

#define	BGPD_FLAG_NO_EVALUATE		0x0002
#define	BGPD_FLAG_REFLECTOR		0x0004
#define	BGPD_FLAG_NEXTHOP_BGP		0x0080
#define	BGPD_FLAG_NEXTHOP_DEFAULT	0x1000
#define	BGPD_FLAG_DECISION_MASK		0x0f00
#define	BGPD_FLAG_DECISION_ROUTEAGE	0x0100
#define	BGPD_FLAG_DECISION_TRANS_AS	0x0200
#define	BGPD_FLAG_DECISION_MED_ALWAYS	0x0400

#define	BGPD_LOG_UPDATES		0x0001

#define	SOCKET_NAME			"/var/run/bgpd.sock"

#define	F_BGPD_INSERTED		0x0001
#define	F_KERNEL		0x0002
#define	F_CONNECTED		0x0004
#define	F_NEXTHOP		0x0008
#define	F_DOWN			0x0010
#define	F_STATIC		0x0020
#define	F_DYNAMIC		0x0040
#define	F_REJECT		0x0080
#define	F_BLACKHOLE		0x0100
#define	F_LONGER		0x0200
#define	F_MPLS			0x0400
#define	F_REDISTRIBUTED		0x0800
#define	F_CTL_DETAIL		0x1000	/* only used by bgpctl */
#define	F_CTL_ADJ_IN		0x2000
#define	F_CTL_ADJ_OUT		0x4000
#define	F_CTL_ACTIVE		0x8000
#define	F_RTLABEL		0x10000
#define	F_CTL_SSV		0x20000	/* only used by bgpctl */
#define	F_CTL_INVALID		0x40000 /* only used by bgpctl */
#define	F_CTL_OVS_VALID		0x80000
#define	F_CTL_OVS_INVALID	0x100000
#define	F_CTL_OVS_NOTFOUND	0x200000

/*
 * Note that these numeric assignments differ from the numbers commonly
 * used in route origin validation context.
 */
#define	ROA_NOTFOUND		0x0	/* default */
#define	ROA_INVALID		0x1
#define	ROA_VALID		0x2
#define	ROA_MASK		0x3

/*
 * Limit the number of messages queued in the session engine.
 * The SE will send an IMSG_XOFF messages to the RDE if the high water mark
 * is reached. The RDE should then throttle this peer or control connection.
 * Once the message queue in the SE drops below the low water mark an
 * IMSG_XON message will be sent and the RDE will produce more messages again.
 */
#define RDE_RUNNER_ROUNDS	100
#define SESS_MSG_HIGH_MARK	300
#define SESS_MSG_LOW_MARK	50
#define CTL_MSG_HIGH_MARK	500
#define CTL_MSG_LOW_MARK	100

enum bgpd_process {
	PROC_MAIN,
	PROC_SE,
	PROC_RDE
} bgpd_process;

enum reconf_action {
	RECONF_NONE,
	RECONF_KEEP,
	RECONF_REINIT,
	RECONF_RELOAD,
	RECONF_DELETE
};

/* Address Family Numbers as per RFC 1700 */
#define	AFI_UNSPEC	0
#define	AFI_IPv4	1
#define	AFI_IPv6	2

/* Subsequent Address Family Identifier as per RFC 4760 */
#define	SAFI_NONE	0
#define	SAFI_UNICAST	1
#define	SAFI_MULTICAST	2
#define	SAFI_MPLS	4
#define	SAFI_MPLSVPN	128

struct aid {
	u_int16_t	 afi;
	sa_family_t	 af;
	u_int8_t	 safi;
	char		*name;
};

extern const struct aid aid_vals[];

#define	AID_UNSPEC	0
#define	AID_INET	1
#define	AID_INET6	2
#define	AID_VPN_IPv4	3
#define	AID_VPN_IPv6	4
#define	AID_MAX		5
#define	AID_MIN		1	/* skip AID_UNSPEC since that is a dummy */

#define AID_VALS	{					\
	/* afi, af, safii, name */				\
	{ AFI_UNSPEC, AF_UNSPEC, SAFI_NONE, "unspec"},		\
	{ AFI_IPv4, AF_INET, SAFI_UNICAST, "IPv4 unicast" },	\
	{ AFI_IPv6, AF_INET6, SAFI_UNICAST, "IPv6 unicast" },	\
	{ AFI_IPv4, AF_INET, SAFI_MPLSVPN, "IPv4 vpn" },	\
	{ AFI_IPv6, AF_INET6, SAFI_MPLSVPN, "IPv6 vpn" }	\
}

#define AID_PTSIZE	{				\
	0,						\
	sizeof(struct pt_entry4),			\
	sizeof(struct pt_entry6),			\
	sizeof(struct pt_entry_vpn4),			\
	sizeof(struct pt_entry_vpn6)			\
}

struct vpn4_addr {
	u_int64_t	rd;
	struct in_addr	addr;
	u_int8_t	labelstack[21];	/* max that makes sense */
	u_int8_t	labellen;
	u_int8_t	pad1;
	u_int8_t	pad2;
};

struct vpn6_addr {
	u_int64_t	rd;
	struct in6_addr	addr;
	u_int8_t	labelstack[21];	/* max that makes sense */
	u_int8_t	labellen;
	u_int8_t	pad1;
	u_int8_t	pad2;
};

#define BGP_MPLS_BOS	0x01

struct bgpd_addr {
	union {
		struct in_addr		v4;
		struct in6_addr		v6;
		struct vpn4_addr	vpn4;
		struct vpn6_addr	vpn6;
		/* maximum size for a prefix is 256 bits */
		u_int8_t		addr8[32];
		u_int16_t		addr16[16];
		u_int32_t		addr32[8];
	} ba;		    /* 128-bit address */
	u_int32_t	scope_id;	/* iface scope id for v6 */
	u_int8_t	aid;
#define	v4	ba.v4
#define	v6	ba.v6
#define	vpn4	ba.vpn4
#define	vpn6	ba.vpn6
#define	addr8	ba.addr8
#define	addr16	ba.addr16
#define	addr32	ba.addr32
};

#define	DEFAULT_LISTENER	0x01
#define	LISTENER_LISTENING	0x02

struct listen_addr {
	TAILQ_ENTRY(listen_addr)	 entry;
	struct sockaddr_storage		 sa;
	int				 fd;
	enum reconf_action		 reconf;
	u_int8_t			 flags;
};

TAILQ_HEAD(listen_addrs, listen_addr);
TAILQ_HEAD(filter_set_head, filter_set);

struct rdomain;
SIMPLEQ_HEAD(rdomain_head, rdomain);

struct network;
TAILQ_HEAD(network_head, network);

struct prefixset;
SIMPLEQ_HEAD(prefixset_head, prefixset);
struct prefixset_item;
RB_HEAD(prefixset_tree, prefixset_item);

struct tentry_v4;
struct tentry_v6;
struct trie_head {
	struct tentry_v4	*root_v4;
	struct tentry_v6	*root_v6;
	int			 match_default_v4;
	int			 match_default_v6;
};

struct rde_prefixset {
	char				name[SET_NAME_LEN];
	struct trie_head		th;
	SIMPLEQ_ENTRY(rde_prefixset)	entry;
	int				dirty;
};
SIMPLEQ_HEAD(rde_prefixset_head, rde_prefixset);

struct set_table;
struct as_set;
SIMPLEQ_HEAD(as_set_head, as_set);

struct filter_rule;
TAILQ_HEAD(filter_head, filter_rule);

struct bgpd_config {
	struct rdomain_head			 rdomains;
	struct network_head			 networks;
	struct filter_head			*filters;
	struct listen_addrs			*listen_addrs;
	struct mrt_head				*mrt;
	struct prefixset_head			 prefixsets;
	struct prefixset_head			 originsets;
	struct prefixset_tree			 roa;
	struct rde_prefixset_head		 rde_prefixsets;
	struct rde_prefixset_head		 rde_originsets;
	struct rde_prefixset			 rde_roa;
	struct as_set_head			*as_sets;
	char					*csock;
	char					*rcsock;
	int					 flags;
	int					 log;
	u_int					 default_tableid;
	u_int32_t				 bgpid;
	u_int32_t				 clusterid;
	u_int32_t				 as;
	u_int16_t				 short_as;
	u_int16_t				 holdtime;
	u_int16_t				 min_holdtime;
	u_int16_t				 connectretry;
	u_int8_t				 fib_priority;
};

extern int cmd_opts;

enum export_type {
	EXPORT_UNSET,
	EXPORT_NONE,
	EXPORT_DEFAULT_ROUTE
};

enum enforce_as {
	ENFORCE_AS_UNDEF,
	ENFORCE_AS_OFF,
	ENFORCE_AS_ON
};

enum auth_method {
	AUTH_NONE,
	AUTH_MD5SIG,
	AUTH_IPSEC_MANUAL_ESP,
	AUTH_IPSEC_MANUAL_AH,
	AUTH_IPSEC_IKE_ESP,
	AUTH_IPSEC_IKE_AH
};

struct peer_auth {
	char			md5key[TCP_MD5_KEY_LEN];
	char			auth_key_in[IPSEC_AUTH_KEY_LEN];
	char			auth_key_out[IPSEC_AUTH_KEY_LEN];
	char			enc_key_in[IPSEC_ENC_KEY_LEN];
	char			enc_key_out[IPSEC_ENC_KEY_LEN];
	u_int32_t		spi_in;
	u_int32_t		spi_out;
	enum auth_method	method;
	u_int8_t		md5key_len;
	u_int8_t		auth_alg_in;
	u_int8_t		auth_alg_out;
	u_int8_t		auth_keylen_in;
	u_int8_t		auth_keylen_out;
	u_int8_t		enc_alg_in;
	u_int8_t		enc_alg_out;
	u_int8_t		enc_keylen_in;
	u_int8_t		enc_keylen_out;
};

struct capabilities {
	struct {
		int16_t	timeout;	/* graceful restart timeout */
		int8_t	flags[AID_MAX];	/* graceful restart per AID flags */
		int8_t	restart;	/* graceful restart, RFC 4724 */
	}	grestart;
	int8_t	mp[AID_MAX];		/* multiprotocol extensions, RFC 4760 */
	int8_t	refresh;		/* route refresh, RFC 2918 */
	int8_t	as4byte;		/* 4-byte ASnum, RFC 4893 */
};

#define	CAPA_GR_PRESENT		0x01
#define	CAPA_GR_RESTART		0x02
#define	CAPA_GR_FORWARD		0x04
#define	CAPA_GR_RESTARTING	0x08

#define	CAPA_GR_TIMEMASK	0x0fff
#define	CAPA_GR_R_FLAG		0x8000
#define	CAPA_GR_F_FLAG		0x80

struct peer_config {
	struct bgpd_addr	 remote_addr;
	struct bgpd_addr	 local_addr;
	struct peer_auth	 auth;
	struct capabilities	 capabilities;
	char			 group[PEER_DESCR_LEN];
	char			 descr[PEER_DESCR_LEN];
	char			 shutcomm[SHUT_COMM_LEN];
	char			 rib[PEER_DESCR_LEN];
	char			 if_depend[IFNAMSIZ];
	char			 demote_group[IFNAMSIZ];
	u_int32_t		 id;
	u_int32_t		 groupid;
	u_int32_t		 remote_as;
	u_int32_t		 local_as;
	u_int32_t		 max_prefix;
	enum export_type	 export_type;
	enum enforce_as		 enforce_as;
	enum enforce_as		 enforce_local_as;
	enum reconf_action	 reconf_action;
	u_int16_t		 max_prefix_restart;
	u_int16_t		 holdtime;
	u_int16_t		 min_holdtime;
	u_int16_t		 local_short_as;
	u_int8_t		 template;
	u_int8_t		 remote_masklen;
	u_int8_t		 ebgp;		/* 0 = ibgp else ebgp */
	u_int8_t		 distance;	/* 1 = direct, >1 = multihop */
	u_int8_t		 passive;
	u_int8_t		 down;
	u_int8_t		 announce_capa;
	u_int8_t		 reflector_client;
	u_int8_t		 ttlsec;	/* TTL security hack */
	u_int8_t		 flags;
};

#define PEERFLAG_TRANS_AS	0x01
#define PEERFLAG_LOG_UPDATES	0x02

enum network_type {
	NETWORK_DEFAULT,	/* from network statements */
	NETWORK_STATIC,
	NETWORK_CONNECTED,
	NETWORK_RTLABEL,
	NETWORK_MRTCLONE,
	NETWORK_PRIORITY,
	NETWORK_PREFIXSET,
};

struct network_config {
	struct bgpd_addr	 prefix;
	struct filter_set_head	 attrset;
	struct rde_aspath	*asp;
	char			 psname[SET_NAME_LEN];
	u_int			 rtableid;
	u_int16_t		 rtlabel;
	enum network_type	 type;
	u_int8_t		 prefixlen;
	u_int8_t		 priority;
	u_int8_t		 old;	/* used for reloading */
};

struct network {
	struct network_config		net;
	TAILQ_ENTRY(network)		entry;
};

enum imsg_type {
	IMSG_NONE,
	IMSG_CTL_END,
	IMSG_CTL_RELOAD,
	IMSG_CTL_FIB_COUPLE,
	IMSG_CTL_FIB_DECOUPLE,
	IMSG_CTL_NEIGHBOR_UP,
	IMSG_CTL_NEIGHBOR_DOWN,
	IMSG_CTL_NEIGHBOR_CLEAR,
	IMSG_CTL_NEIGHBOR_RREFRESH,
	IMSG_CTL_NEIGHBOR_DESTROY,
	IMSG_CTL_KROUTE,
	IMSG_CTL_KROUTE_ADDR,
	IMSG_CTL_RESULT,
	IMSG_CTL_SHOW_NEIGHBOR,
	IMSG_CTL_SHOW_NEXTHOP,
	IMSG_CTL_SHOW_INTERFACE,
	IMSG_CTL_SHOW_RIB,
	IMSG_CTL_SHOW_RIB_PREFIX,
	IMSG_CTL_SHOW_RIB_ATTR,
	IMSG_CTL_SHOW_NETWORK,
	IMSG_CTL_SHOW_RIB_MEM,
	IMSG_CTL_SHOW_RIB_HASH,
	IMSG_CTL_SHOW_TERSE,
	IMSG_CTL_SHOW_TIMER,
	IMSG_CTL_LOG_VERBOSE,
	IMSG_CTL_SHOW_FIB_TABLES,
	IMSG_CTL_TERMINATE,
	IMSG_NETWORK_ADD,
	IMSG_NETWORK_ASPATH,
	IMSG_NETWORK_ATTR,
	IMSG_NETWORK_REMOVE,
	IMSG_NETWORK_FLUSH,
	IMSG_NETWORK_DONE,
	IMSG_FILTER_SET,
	IMSG_SOCKET_CONN,
	IMSG_SOCKET_CONN_CTL,
	IMSG_RECONF_CONF,
	IMSG_RECONF_RIB,
	IMSG_RECONF_PEER,
	IMSG_RECONF_FILTER,
	IMSG_RECONF_LISTENER,
	IMSG_RECONF_CTRL,
	IMSG_RECONF_RDOMAIN,
	IMSG_RECONF_RDOMAIN_EXPORT,
	IMSG_RECONF_RDOMAIN_IMPORT,
	IMSG_RECONF_RDOMAIN_DONE,
	IMSG_RECONF_PREFIX_SET,
	IMSG_RECONF_PREFIX_SET_ITEM,
	IMSG_RECONF_AS_SET,
	IMSG_RECONF_AS_SET_ITEMS,
	IMSG_RECONF_AS_SET_DONE,
	IMSG_RECONF_ORIGIN_SET,
	IMSG_RECONF_ROA_SET,
	IMSG_RECONF_ROA_SET_ITEMS,
	IMSG_RECONF_DRAIN,
	IMSG_RECONF_DONE,
	IMSG_UPDATE,
	IMSG_UPDATE_ERR,
	IMSG_SESSION_ADD,
	IMSG_SESSION_UP,
	IMSG_SESSION_DOWN,
	IMSG_SESSION_STALE,
	IMSG_SESSION_FLUSH,
	IMSG_SESSION_RESTARTED,
	IMSG_MRT_OPEN,
	IMSG_MRT_REOPEN,
	IMSG_MRT_CLOSE,
	IMSG_KROUTE_CHANGE,
	IMSG_KROUTE_DELETE,
	IMSG_NEXTHOP_ADD,
	IMSG_NEXTHOP_REMOVE,
	IMSG_NEXTHOP_UPDATE,
	IMSG_PFTABLE_ADD,
	IMSG_PFTABLE_REMOVE,
	IMSG_PFTABLE_COMMIT,
	IMSG_REFRESH,
	IMSG_IFINFO,
	IMSG_DEMOTE,
	IMSG_XON,
	IMSG_XOFF
};

struct demote_msg {
	char		 demote_group[IFNAMSIZ];
	int		 level;
};

enum ctl_results {
	CTL_RES_OK,
	CTL_RES_NOSUCHPEER,
	CTL_RES_DENIED,
	CTL_RES_NOCAP,
	CTL_RES_PARSE_ERROR,
	CTL_RES_PENDING,
	CTL_RES_NOMEM,
	CTL_RES_BADPEER,
	CTL_RES_BADSTATE,
	CTL_RES_NOSUCHRIB
};

/* needed for session.h parse prototype */
LIST_HEAD(mrt_head, mrt);

/* error codes and subcodes needed in SE and RDE */
enum err_codes {
	ERR_HEADER = 1,
	ERR_OPEN,
	ERR_UPDATE,
	ERR_HOLDTIMEREXPIRED,
	ERR_FSM,
	ERR_CEASE
};

enum suberr_update {
	ERR_UPD_UNSPECIFIC,
	ERR_UPD_ATTRLIST,
	ERR_UPD_UNKNWN_WK_ATTR,
	ERR_UPD_MISSNG_WK_ATTR,
	ERR_UPD_ATTRFLAGS,
	ERR_UPD_ATTRLEN,
	ERR_UPD_ORIGIN,
	ERR_UPD_LOOP,
	ERR_UPD_NEXTHOP,
	ERR_UPD_OPTATTR,
	ERR_UPD_NETWORK,
	ERR_UPD_ASPATH
};

enum suberr_cease {
	ERR_CEASE_MAX_PREFIX = 1,
	ERR_CEASE_ADMIN_DOWN,
	ERR_CEASE_PEER_UNCONF,
	ERR_CEASE_ADMIN_RESET,
	ERR_CEASE_CONN_REJECT,
	ERR_CEASE_OTHER_CHANGE,
	ERR_CEASE_COLLISION,
	ERR_CEASE_RSRC_EXHAUST
};

struct kroute_node;
struct kroute6_node;
struct knexthop_node;
RB_HEAD(kroute_tree, kroute_node);
RB_HEAD(kroute6_tree, kroute6_node);
RB_HEAD(knexthop_tree, knexthop_node);

struct ktable {
	char			 descr[PEER_DESCR_LEN];
	char			 ifmpe[IFNAMSIZ];
	struct kroute_tree	 krt;
	struct kroute6_tree	 krt6;
	struct knexthop_tree	 knt;
	struct network_head	 krn;
	u_int			 rtableid;
	u_int			 nhtableid; /* rdomain id for nexthop lookup */
	u_int			 ifindex;   /* ifindex of ifmpe */
	int			 nhrefcnt;  /* refcnt for nexthop table */
	enum reconf_action	 state;
	u_int8_t		 fib_conf;  /* configured FIB sync flag */
	u_int8_t		 fib_sync;  /* is FIB synced with kernel? */
};

struct kroute_full {
	struct bgpd_addr	prefix;
	struct bgpd_addr	nexthop;
	char			label[RTLABEL_LEN];
	u_int16_t		labelid;
	u_int16_t		flags;
	u_short			ifindex;
	u_int8_t		prefixlen;
	u_int8_t		priority;
};

struct kroute {
	struct in_addr	prefix;
	struct in_addr	nexthop;
	u_int32_t	mplslabel;
	u_int16_t	flags;
	u_int16_t	labelid;
	u_short		ifindex;
	u_int8_t	prefixlen;
	u_int8_t	priority;
};

struct kroute6 {
	struct in6_addr	prefix;
	struct in6_addr	nexthop;
	u_int32_t	mplslabel;
	u_int16_t	flags;
	u_int16_t	labelid;
	u_short		ifindex;
	u_int8_t	prefixlen;
	u_int8_t	priority;
};

struct kroute_nexthop {
	struct bgpd_addr	nexthop;
	struct bgpd_addr	gateway;
	struct bgpd_addr	net;
	u_int8_t		valid;
	u_int8_t		connected;
	u_int8_t		netlen;
};

struct kif {
	char			 ifname[IFNAMSIZ];
	u_int64_t		 baudrate;
	u_int			 rdomain;
	int			 flags;
	u_short			 ifindex;
	u_int8_t		 if_type;
	u_int8_t		 link_state;
	u_int8_t		 nh_reachable;	/* for nexthop verification */
};

struct session_up {
	struct bgpd_addr	local_addr;
	struct bgpd_addr	remote_addr;
	struct capabilities	capa;
	u_int32_t		remote_bgpid;
	u_int16_t		short_as;
};

struct pftable_msg {
	struct bgpd_addr	addr;
	char			pftable[PFTABLE_LEN];
	u_int8_t		len;
};

struct ctl_show_nexthop {
	struct bgpd_addr	addr;
	struct kif		kif;
	union {
		struct kroute		kr4;
		struct kroute6		kr6;
	} kr;
	u_int8_t		valid;
	u_int8_t		krvalid;
};

struct ctl_neighbor {
	struct bgpd_addr	addr;
	char			descr[PEER_DESCR_LEN];
	char			shutcomm[SHUT_COMM_LEN];
	int			show_timers;
};

#define	F_PREF_ELIGIBLE	0x01
#define	F_PREF_ACTIVE	0x02
#define	F_PREF_INTERNAL	0x04
#define	F_PREF_ANNOUNCE	0x08
#define	F_PREF_STALE	0x10
#define	F_PREF_INVALID	0x20

struct ctl_show_rib {
	struct bgpd_addr	true_nexthop;
	struct bgpd_addr	exit_nexthop;
	struct bgpd_addr	prefix;
	struct bgpd_addr	remote_addr;
	char			descr[PEER_DESCR_LEN];
	time_t			lastchange;
	u_int32_t		remote_id;
	u_int32_t		local_pref;
	u_int32_t		med;
	u_int32_t		weight;
	u_int32_t		flags;
	u_int16_t		aspath_len;
	u_int8_t		prefixlen;
	u_int8_t		origin;
	u_int8_t		validation_state;
	/* plus a aspath_len bytes long aspath */
};

enum as_spec {
	AS_UNDEF,
	AS_ALL,
	AS_SOURCE,
	AS_TRANSIT,
	AS_PEER,
	AS_EMPTY
};

enum aslen_spec {
	ASLEN_NONE,
	ASLEN_MAX,
	ASLEN_SEQ
};

#define AS_FLAG_NEIGHBORAS	0x01
#define AS_FLAG_AS_SET_NAME	0x02
#define AS_FLAG_AS_SET		0x04

struct filter_as {
	char		 name[SET_NAME_LEN];
	struct as_set	*aset;
	u_int32_t	 as_min;
	u_int32_t	 as_max;
	enum as_spec	 type;
	u_int8_t	 flags;
	u_int8_t	 op;
};

struct filter_aslen {
	u_int		aslen;
	enum aslen_spec	type;
};

#define PREFIXSET_FLAG_FILTER	0x01
#define PREFIXSET_FLAG_DIRTY	0x02	/* prefix-set changed at reload */
#define PREFIXSET_FLAG_OPS	0x04	/* indiv. prefixes have prefixlenops */
#define PREFIXSET_FLAG_LONGER	0x08	/* filter all prefixes with or-longer */

struct filter_prefixset {
	int			 flags;
	char			 name[SET_NAME_LEN];
	struct rde_prefixset	*ps;
};

struct filter_originset {
	char			 name[SET_NAME_LEN];
	struct rde_prefixset	*ps;
};

struct filter_ovs {
	u_int8_t		 validity;
	u_int8_t		 is_set;
};

struct filter_community {
	u_int8_t	type;
	u_int8_t	dflag1;	/* one of set, any, local-as, neighbor-as */
	u_int8_t	dflag2;
	u_int8_t	dflag3;
	union {
		struct basic {
			u_int32_t	data1;
			u_int32_t	data2;
		} b;
		struct large {
			u_int32_t	data1;
			u_int32_t	data2;
			u_int32_t	data3;
		} l;
		struct ext {
			u_int32_t	data1;
			u_int64_t	data2;
			u_int8_t	type;
			u_int8_t	subtype;	/* if extended type */
		} e;
	}		c;
};

struct ctl_show_rib_request {
	char			rib[PEER_DESCR_LEN];
	struct ctl_neighbor	neighbor;
	struct bgpd_addr	prefix;
	struct filter_as	as;
	struct filter_community community;
	u_int32_t		peerid;
	u_int32_t		flags;
	u_int8_t		validation_state;
	pid_t			pid;
	enum imsg_type		type;
	u_int8_t		prefixlen;
	u_int8_t		aid;
};

enum filter_actions {
	ACTION_NONE,
	ACTION_ALLOW,
	ACTION_DENY
};

enum directions {
	DIR_IN = 1,
	DIR_OUT
};

enum from_spec {
	FROM_ALL,
	FROM_ADDRESS,
	FROM_DESCR,
	FROM_GROUP
};

enum comp_ops {
	OP_NONE,
	OP_RANGE,
	OP_XRANGE,
	OP_EQ,
	OP_NE,
	OP_LE,
	OP_LT,
	OP_GE,
	OP_GT
};

struct filter_peers {
	u_int32_t	peerid;
	u_int32_t	groupid;
	u_int32_t	remote_as;
	u_int16_t	ribid;
	u_int8_t	ebgp;
	u_int8_t	ibgp;
};

/* special community type */
#define	COMMUNITY_TYPE_NONE		0
#define	COMMUNITY_TYPE_BASIC		1
#define	COMMUNITY_TYPE_EXT		2
#define	COMMUNITY_TYPE_LARGE		3

#define	COMMUNITY_ANY			1
#define	COMMUNITY_NEIGHBOR_AS		2
#define	COMMUNITY_LOCAL_AS		3

/* wellknown community definitions */
#define	COMMUNITY_WELLKNOWN		0xffff
#define	COMMUNITY_GRACEFUL_SHUTDOWN	0x0000  /* RFC 8326 */
#define	COMMUNITY_BLACKHOLE		0x029A	/* RFC 7999 */
#define	COMMUNITY_NO_EXPORT		0xff01
#define	COMMUNITY_NO_ADVERTISE		0xff02
#define	COMMUNITY_NO_EXPSUBCONFED	0xff03
#define	COMMUNITY_NO_PEER		0xff04	/* RFC 3765 */

/* extended community definitions */
#define EXT_COMMUNITY_IANA		0x80
#define EXT_COMMUNITY_TRANSITIVE	0x40
#define EXT_COMMUNITY_VALUE		0x3f
/* extended types transitive */
#define EXT_COMMUNITY_TRANS_TWO_AS	0x00	/* 2 octet AS specific */
#define EXT_COMMUNITY_TRANS_IPV4	0x01	/* IPv4 specific */
#define EXT_COMMUNITY_TRANS_FOUR_AS	0x02	/* 4 octet AS specific */
#define EXT_COMMUNITY_TRANS_OPAQUE	0x03	/* opaque ext community */
#define EXT_COMMUNITY_TRANS_EVPN	0x06	/* EVPN RFC7432 */
/* extended types non-transitive */
#define EXT_COMMUNITY_NON_TRANS_TWO_AS	0x40	/* 2 octet AS specific */
#define EXT_COMMUNITY_NON_TRANS_IPV4	0x41	/* IPv4 specific */
#define EXT_COMMUNITY_NON_TRANS_FOUR_AS	0x42	/* 4 octet AS specific */
#define EXT_COMMUNITY_NON_TRANS_OPAQUE	0x43	/* opaque ext community */

/* BGP Origin Validation State Extended Community RFC8097 */
#define EXT_COMMUNITY_OVS_VALID		0
#define EXT_COMMUNITY_OVS_NOTFOUND	1
#define EXT_COMMUNITY_OVS_INVALID	2

/* other handy defines */
#define EXT_COMMUNITY_OPAQUE_MAX	0xffffffffffffULL
#define EXT_COMMUNITY_FLAG_VALID	0x01

struct ext_comm_pairs {
	u_int8_t	type;
	u_int8_t	subtype;
	const char	*subname;
};

#define IANA_EXT_COMMUNITIES	{				\
	{ EXT_COMMUNITY_TRANS_TWO_AS, 0x02, "rt" },		\
	{ EXT_COMMUNITY_TRANS_TWO_AS, 0x03, "soo" },		\
	{ EXT_COMMUNITY_TRANS_TWO_AS, 0x05, "odi" },		\
	{ EXT_COMMUNITY_TRANS_TWO_AS, 0x08, "bdc" },		\
	{ EXT_COMMUNITY_TRANS_TWO_AS, 0x09, "srcas" },		\
	{ EXT_COMMUNITY_TRANS_TWO_AS, 0x0a, "l2vid" },		\
								\
	{ EXT_COMMUNITY_TRANS_FOUR_AS, 0x02, "rt" },		\
	{ EXT_COMMUNITY_TRANS_FOUR_AS, 0x03, "soo" },		\
	{ EXT_COMMUNITY_TRANS_FOUR_AS, 0x05, "odi" },		\
	{ EXT_COMMUNITY_TRANS_FOUR_AS, 0x08, "bdc" },		\
	{ EXT_COMMUNITY_TRANS_FOUR_AS, 0x09, "srcas" },		\
								\
	{ EXT_COMMUNITY_TRANS_IPV4, 0x02, "rt" },		\
	{ EXT_COMMUNITY_TRANS_IPV4, 0x03, "soo" },		\
	{ EXT_COMMUNITY_TRANS_IPV4, 0x05, "odi" },		\
	{ EXT_COMMUNITY_TRANS_IPV4, 0x07, "ori" },		\
	{ EXT_COMMUNITY_TRANS_IPV4, 0x0a, "l2vid" },		\
	{ EXT_COMMUNITY_TRANS_IPV4, 0x0b, "vrfri" },		\
								\
	{ EXT_COMMUNITY_TRANS_OPAQUE, 0x06, "ort" },		\
	{ EXT_COMMUNITY_TRANS_OPAQUE, 0x0d, "defgw" },		\
								\
	{ EXT_COMMUNITY_NON_TRANS_OPAQUE, 0x00, "ovs" },	\
								\
	{ EXT_COMMUNITY_TRANS_EVPN, 0x00, "mac-mob" },		\
	{ EXT_COMMUNITY_TRANS_EVPN, 0x01, "esi-lab" },		\
	{ EXT_COMMUNITY_TRANS_EVPN, 0x02, "esi-rt" },		\
								\
	{ 0 }							\
}

extern const struct ext_comm_pairs iana_ext_comms[];

struct filter_prefix {
	struct bgpd_addr	addr;
	u_int8_t		op;
	u_int8_t		len;
	u_int8_t		len_min;
	u_int8_t		len_max;
};

struct filter_nexthop {
	struct bgpd_addr	addr;
	u_int8_t		flags;
#define FILTER_NEXTHOP_ADDR	1
#define FILTER_NEXTHOP_NEIGHBOR	2
};

struct filter_match {
	struct filter_prefix		prefix;
	struct filter_nexthop		nexthop;
	struct filter_as		as;
	struct filter_aslen		aslen;
	struct filter_community		community[MAX_COMM_MATCH];
	struct filter_prefixset		prefixset;
	struct filter_originset		originset;
	struct filter_ovs		ovs;
};

struct filter_rule {
	TAILQ_ENTRY(filter_rule)	entry;
	char				rib[PEER_DESCR_LEN];
	struct filter_peers		peer;
	struct filter_match		match;
	struct filter_set_head		set;
#define RDE_FILTER_SKIP_DIR		0
#define RDE_FILTER_SKIP_GROUPID		1
#define RDE_FILTER_SKIP_REMOTE_AS	2
#define RDE_FILTER_SKIP_PEERID		3
#define RDE_FILTER_SKIP_COUNT		4
	struct filter_rule		*skip[RDE_FILTER_SKIP_COUNT];
	enum filter_actions		action;
	enum directions			dir;
	u_int8_t			quick;
};

enum action_types {
	ACTION_SET_LOCALPREF,
	ACTION_SET_RELATIVE_LOCALPREF,
	ACTION_SET_MED,
	ACTION_SET_RELATIVE_MED,
	ACTION_SET_WEIGHT,
	ACTION_SET_RELATIVE_WEIGHT,
	ACTION_SET_PREPEND_SELF,
	ACTION_SET_PREPEND_PEER,
	ACTION_SET_NEXTHOP,
	ACTION_SET_NEXTHOP_REJECT,
	ACTION_SET_NEXTHOP_BLACKHOLE,
	ACTION_SET_NEXTHOP_NOMODIFY,
	ACTION_SET_NEXTHOP_SELF,
	ACTION_DEL_COMMUNITY,
	ACTION_SET_COMMUNITY,
	ACTION_PFTABLE,
	ACTION_PFTABLE_ID,
	ACTION_RTLABEL,
	ACTION_RTLABEL_ID,
	ACTION_SET_ORIGIN
};

struct nexthop;
struct filter_set {
	TAILQ_ENTRY(filter_set)		entry;
	union {
		u_int8_t			 prepend;
		u_int16_t			 id;
		u_int32_t			 metric;
		int32_t				 relative;
		struct bgpd_addr		 nexthop;
		struct nexthop			*nh;
		struct filter_community		 community;
		char				 pftable[PFTABLE_LEN];
		char				 rtlabel[RTLABEL_LEN];
		u_int8_t			 origin;
	} action;
	enum action_types		type;
};

struct roa_set {
	u_int32_t	as;	/* must be first */
	u_int32_t	maxlen;	/* change type for better struct layout */
};

struct prefixset_item {
	struct filter_prefix		p;
	RB_ENTRY(prefixset_item)	entry;
	struct set_table		*set;
};

struct prefixset {
	int				 sflags;
	char				 name[SET_NAME_LEN];
	struct prefixset_tree		 psitems;
	SIMPLEQ_ENTRY(prefixset)	 entry;
};

struct as_set {
	char				 name[SET_NAME_LEN];
	SIMPLEQ_ENTRY(as_set)		 entry;
	struct set_table		*set;
	int				 dirty;
};

struct rdomain {
	SIMPLEQ_ENTRY(rdomain)		entry;
	char				descr[PEER_DESCR_LEN];
	char				ifmpe[IFNAMSIZ];
	struct filter_set_head		import;
	struct filter_set_head		export;
	struct network_head		net_l;
	u_int64_t			rd;
	u_int				rtableid;
	u_int				label;
	int				flags;
};

struct rde_rib {
	SIMPLEQ_ENTRY(rde_rib)	entry;
	char			name[PEER_DESCR_LEN];
	u_int			rtableid;
	u_int16_t		id;
	u_int16_t		flags;
};
SIMPLEQ_HEAD(rib_names, rde_rib);
extern struct rib_names ribnames;

/* rde_rib flags */
#define F_RIB_LOCAL		0x0001
#define F_RIB_NOEVALUATE	0x0002
#define F_RIB_NOFIB		0x0004
#define F_RIB_NOFIBSYNC		0x0008
#define F_RIB_HASNOFIB		(F_RIB_NOFIB | F_RIB_NOEVALUATE)

/* 4-byte magic AS number */
#define AS_TRANS	23456
/* AS_NONE for origin validation */
#define AS_NONE		0

struct rde_memstats {
	int64_t		path_cnt;
	int64_t		path_refs;
	int64_t		prefix_cnt;
	int64_t		rib_cnt;
	int64_t		pt_cnt[AID_MAX];
	int64_t		nexthop_cnt;
	int64_t		aspath_cnt;
	int64_t		aspath_size;
	int64_t		aspath_refs;
	int64_t		attr_cnt;
	int64_t		attr_refs;
	int64_t		attr_data;
	int64_t		attr_dcnt;
	int64_t		aset_cnt;
	int64_t		aset_size;
	int64_t		aset_nmemb;
	int64_t		pset_cnt;
	int64_t		pset_size;
};

struct rde_hashstats {
	char		name[16];
	int64_t		num;
	int64_t		min;
	int64_t		max;
	int64_t		sum;
	int64_t		sumq;
};

#define	MRT_FILE_LEN	512
#define	MRT2MC(x)	((struct mrt_config *)(x))
#define	MRT_MAX_TIMEOUT	7200

enum mrt_type {
	MRT_NONE,
	MRT_TABLE_DUMP,
	MRT_TABLE_DUMP_MP,
	MRT_TABLE_DUMP_V2,
	MRT_ALL_IN,
	MRT_ALL_OUT,
	MRT_UPDATE_IN,
	MRT_UPDATE_OUT
};

enum mrt_state {
	MRT_STATE_RUNNING,
	MRT_STATE_OPEN,
	MRT_STATE_REOPEN,
	MRT_STATE_REMOVE
};

struct mrt {
	char			rib[PEER_DESCR_LEN];
	struct msgbuf		wbuf;
	LIST_ENTRY(mrt)		entry;
	u_int32_t		peer_id;
	u_int32_t		group_id;
	enum mrt_type		type;
	enum mrt_state		state;
	u_int16_t		seqnum;
};

struct mrt_config {
	struct mrt		conf;
	char			name[MRT_FILE_LEN];	/* base file name */
	char			file[MRT_FILE_LEN];	/* actual file name */
	time_t			ReopenTimer;
	int			ReopenTimerInterval;
};

/* prototypes */
/* bgpd.c */
void		 send_nexthop_update(struct kroute_nexthop *);
void		 send_imsg_session(int, pid_t, void *, u_int16_t);
int		 send_network(int, struct network_config *,
		     struct filter_set_head *);
int		 bgpd_filternexthop(struct kroute *, struct kroute6 *);
void		 set_pollfd(struct pollfd *, struct imsgbuf *);
int		 handle_pollfd(struct pollfd *, struct imsgbuf *);

/* control.c */
int	control_imsg_relay(struct imsg *);

/* config.c */
struct bgpd_config	*new_config(void);
void			 free_config(struct bgpd_config *);
void	free_prefixsets(struct prefixset_head *);
void	free_prefixtree(struct prefixset_tree *);
void	filterlist_free(struct filter_head *);
int	host(const char *, struct bgpd_addr *, u_int8_t *);
void	copy_filterset(struct filter_set_head *, struct filter_set_head *);
void	expand_networks(struct bgpd_config *);
int	prefixset_cmp(struct prefixset_item *, struct prefixset_item *);
RB_PROTOTYPE(prefixset_tree, prefixset_item, entry, prefixset_cmp);

/* kroute.c */
int		 kr_init(void);
int		 ktable_update(u_int, char *, char *, int, u_int8_t);
void		 ktable_preload(void);
void		 ktable_postload(u_int8_t);
int		 ktable_exists(u_int, u_int *);
int		 kr_change(u_int, struct kroute_full *,  u_int8_t);
int		 kr_delete(u_int, struct kroute_full *, u_int8_t);
void		 kr_shutdown(u_int8_t, u_int);
void		 kr_fib_couple(u_int, u_int8_t);
void		 kr_fib_couple_all(u_int8_t);
void		 kr_fib_decouple(u_int, u_int8_t);
void		 kr_fib_decouple_all(u_int8_t);
void		 kr_fib_update_prio_all(u_int8_t);
int		 kr_dispatch_msg(u_int rdomain);
int		 kr_nexthop_add(u_int32_t, struct bgpd_addr *,
		    struct bgpd_config *);
void		 kr_nexthop_delete(u_int32_t, struct bgpd_addr *,
		    struct bgpd_config *);
void		 kr_show_route(struct imsg *);
void		 kr_ifinfo(char *);
int		 kr_net_reload(u_int, struct network_head *);
int		 kr_reload(void);
struct in6_addr	*prefixlen2mask6(u_int8_t prefixlen);

/* log.c */
void		 log_peer_info(const struct peer_config *, const char *, ...)
			__attribute__((__format__ (printf, 2, 3)));
void		 log_peer_warn(const struct peer_config *, const char *, ...)
			__attribute__((__format__ (printf, 2, 3)));
void		 log_peer_warnx(const struct peer_config *, const char *, ...)
			__attribute__((__format__ (printf, 2, 3)));

/* mrt.c */
void		 mrt_clear_seq(void);
void		 mrt_write(struct mrt *);
void		 mrt_clean(struct mrt *);
void		 mrt_init(struct imsgbuf *, struct imsgbuf *);
int		 mrt_timeout(struct mrt_head *);
void		 mrt_reconfigure(struct mrt_head *);
void		 mrt_handler(struct mrt_head *);
struct mrt	*mrt_get(struct mrt_head *, struct mrt *);
int		 mrt_mergeconfig(struct mrt_head *, struct mrt_head *);

/* name2id.c */
u_int16_t	 rib_name2id(const char *);
const char	*rib_id2name(u_int16_t);
void		 rib_unref(u_int16_t);
void		 rib_ref(u_int16_t);
u_int16_t	 rtlabel_name2id(const char *);
const char	*rtlabel_id2name(u_int16_t);
void		 rtlabel_unref(u_int16_t);
u_int16_t	 rtlabel_ref(u_int16_t);
u_int16_t	 pftable_name2id(const char *);
const char	*pftable_id2name(u_int16_t);
void		 pftable_unref(u_int16_t);
u_int16_t	 pftable_ref(u_int16_t);

/* parse.y */
int		 cmdline_symset(char *);
struct prefixset *find_prefixset(char *, struct prefixset_head *);

/* pftable.c */
int	pftable_exists(const char *);
int	pftable_add(const char *);
int	pftable_clear_all(void);
int	pftable_addr_add(struct pftable_msg *);
int	pftable_addr_remove(struct pftable_msg *);
int	pftable_commit(void);

/* rde_filter.c */
void		 filterset_free(struct filter_set_head *);
int		 filterset_cmp(struct filter_set *, struct filter_set *);
void		 filterset_move(struct filter_set_head *,
		    struct filter_set_head *);
const char	*filterset_name(enum action_types);

/* rde_sets.c */
struct as_set	*as_sets_lookup(struct as_set_head *, const char *);
struct as_set	*as_sets_new(struct as_set_head *, const char *, size_t,
		    size_t);
void		 as_sets_free(struct as_set_head *);
void		 as_sets_mark_dirty(struct as_set_head *, struct as_set_head *);
int		 as_set_match(const struct as_set *, u_int32_t);

struct set_table	*set_new(size_t, size_t);
void		 	 set_free(struct set_table *);
int			 set_add(struct set_table *, void *, size_t);
void			*set_get(struct set_table *, size_t *);
void			 set_prep(struct set_table *);
void			*set_match(const struct set_table *, u_int32_t);
int			 set_equal(const struct set_table *,
			    const struct set_table *);

/* util.c */
const char	*log_addr(const struct bgpd_addr *);
const char	*log_in6addr(const struct in6_addr *);
const char	*log_sockaddr(struct sockaddr *);
const char	*log_as(u_int32_t);
const char	*log_rd(u_int64_t);
const char	*log_ext_subtype(u_int8_t, u_int8_t);
const char	*log_shutcomm(const char *);
int		 aspath_snprint(char *, size_t, void *, u_int16_t);
int		 aspath_asprint(char **, void *, u_int16_t);
size_t		 aspath_strlen(void *, u_int16_t);
u_int32_t	 aspath_extract(const void *, int);
int		 aspath_verify(void *, u_int16_t, int);
#define		 AS_ERR_LEN	-1
#define		 AS_ERR_TYPE	-2
#define		 AS_ERR_BAD	-3
#define		 AS_ERR_SOFT	-4
u_char		*aspath_inflate(void *, u_int16_t, u_int16_t *);
int		 nlri_get_prefix(u_char *, u_int16_t, struct bgpd_addr *,
		     u_int8_t *);
int		 nlri_get_prefix6(u_char *, u_int16_t, struct bgpd_addr *,
		     u_int8_t *);
int		 nlri_get_vpn4(u_char *, u_int16_t, struct bgpd_addr *,
		     u_int8_t *, int);
int		 nlri_get_vpn6(u_char *, u_int16_t, struct bgpd_addr *,
		     u_int8_t *, int);
int		 prefix_compare(const struct bgpd_addr *,
		    const struct bgpd_addr *, int);
in_addr_t	 prefixlen2mask(u_int8_t);
void		 inet4applymask(struct in_addr *, const struct in_addr *, int);
void		 inet6applymask(struct in6_addr *, const struct in6_addr *,
		    int);
const char	*aid2str(u_int8_t);
int		 aid2afi(u_int8_t, u_int16_t *, u_int8_t *);
int		 afi2aid(u_int16_t, u_int8_t, u_int8_t *);
sa_family_t	 aid2af(u_int8_t);
int		 af2aid(sa_family_t, u_int8_t, u_int8_t *);
struct sockaddr	*addr2sa(struct bgpd_addr *, u_int16_t);
void		 sa2addr(struct sockaddr *, struct bgpd_addr *);
uint64_t	 ift2ifm(uint8_t);
const char *	 get_media_descr(uint64_t);
const char *	 get_linkstate(uint8_t, int);
const char *	 get_baudrate(u_int64_t, char *);

static const char * const log_procnames[] = {
	"parent",
	"SE",
	"RDE"
};

/* logmsg.c and needed by bgpctl */
static const char * const statenames[] = {
	"None",
	"Idle",
	"Connect",
	"Active",
	"OpenSent",
	"OpenConfirm",
	"Established"
};

static const char * const msgtypenames[] = {
	"NONE",
	"OPEN",
	"UPDATE",
	"NOTIFICATION",
	"KEEPALIVE",
	"RREFRESH"
};

static const char * const eventnames[] = {
	"None",
	"Start",
	"Stop",
	"Connection opened",
	"Connection closed",
	"Connection open failed",
	"Fatal error",
	"ConnectRetryTimer expired",
	"HoldTimer expired",
	"KeepaliveTimer expired",
	"OPEN message received",
	"KEEPALIVE message received",
	"UPDATE message received",
	"NOTIFICATION received"
};

static const char * const errnames[] = {
	"none",
	"Header error",
	"error in OPEN message",
	"error in UPDATE message",
	"HoldTimer expired",
	"Finite State Machine error",
	"Cease"
};

static const char * const suberr_header_names[] = {
	"none",
	"synchronization error",
	"wrong length",
	"unknown message type"
};

static const char * const suberr_open_names[] = {
	"none",
	"version mismatch",
	"AS unacceptable",
	"BGPID invalid",
	"optional parameter error",
	"authentication error",
	"unacceptable holdtime",
	"unsupported capability",
	"group membership conflict",	/* draft-ietf-idr-bgp-multisession-07 */
	"group membership required"	/* draft-ietf-idr-bgp-multisession-07 */
};

static const char * const suberr_fsm_names[] = {
	"unspecified error",
	"received unexpected message in OpenSent",
	"received unexpected message in OpenConfirm",
	"received unexpected message in Established"
};

static const char * const suberr_update_names[] = {
	"none",
	"attribute list error",
	"unknown well-known attribute",
	"well-known attribute missing",
	"attribute flags error",
	"attribute length wrong",
	"origin unacceptable",
	"loop detected",
	"nexthop unacceptable",
	"optional attribute error",
	"network unacceptable",
	"AS-Path unacceptable"
};

static const char * const suberr_cease_names[] = {
	"none",
	"max-prefix exceeded",
	"administratively down",
	"peer unconfigured",
	"administrative reset",
	"connection rejected",
	"other config change",
	"collision",
	"resource exhaustion"
};

static const char * const ctl_res_strerror[] = {
	"no error",
	"no such neighbor",
	"permission denied",
	"neighbor does not have this capability",
	"config file has errors, reload failed",
	"previous reload still running",
	"out of memory",
	"not a cloned peer",
	"peer still active, down peer first",
	"no such RIB"
};

static const char * const timernames[] = {
	"None",
	"ConnectRetryTimer",
	"KeepaliveTimer",
	"HoldTimer",
	"IdleHoldTimer",
	"IdleHoldResetTimer",
	"CarpUndemoteTimer",
	""
};

#endif /* __BGPD_H__ */
