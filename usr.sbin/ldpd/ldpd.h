/*	$OpenBSD: ldpd.h,v 1.66 2016/05/23 18:25:30 renato Exp $ */

/*
 * Copyright (c) 2009 Michele Marchetto <michele@openbsd.org>
 * Copyright (c) 2004 Esben Norby <norby@openbsd.org>
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

#ifndef _LDPD_H_
#define _LDPD_H_

#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/tree.h>
#include <md5.h>
#include <net/if.h>
#include <netinet/in.h>
#include <event.h>

#include <imsg.h>
#include "ldp.h"

#define CONF_FILE		"/etc/ldpd.conf"
#define	LDPD_SOCKET		"/var/run/ldpd.sock"
#define LDPD_USER		"_ldpd"

#define LDPD_OPT_VERBOSE	0x00000001
#define LDPD_OPT_VERBOSE2	0x00000002
#define LDPD_OPT_NOACTION	0x00000004

#define TCP_MD5_KEY_LEN		80
#define L2VPN_NAME_LEN		32

#define	RT_BUF_SIZE		16384
#define	MAX_RTSOCK_BUF		128 * 1024
#define	LDP_BACKLOG		128

#define	F_LDPD_INSERTED		0x0001
#define	F_CONNECTED		0x0002
#define	F_STATIC		0x0004
#define	F_DYNAMIC		0x0008
#define	F_REJECT		0x0010
#define	F_BLACKHOLE		0x0020
#define	F_REDISTRIBUTED		0x0040

struct evbuf {
	struct msgbuf		wbuf;
	struct event		ev;
};

struct imsgev {
	struct imsgbuf		 ibuf;
	void			(*handler)(int, short, void *);
	struct event		 ev;
	short			 events;
};

enum imsg_type {
	IMSG_NONE,
	IMSG_CTL_RELOAD,
	IMSG_CTL_SHOW_INTERFACE,
	IMSG_CTL_SHOW_DISCOVERY,
	IMSG_CTL_SHOW_NBR,
	IMSG_CTL_SHOW_LIB,
	IMSG_CTL_SHOW_L2VPN_PW,
	IMSG_CTL_SHOW_L2VPN_BINDING,
	IMSG_CTL_FIB_COUPLE,
	IMSG_CTL_FIB_DECOUPLE,
	IMSG_CTL_KROUTE,
	IMSG_CTL_KROUTE_ADDR,
	IMSG_CTL_IFINFO,
	IMSG_CTL_END,
	IMSG_CTL_LOG_VERBOSE,
	IMSG_KLABEL_CHANGE,
	IMSG_KLABEL_DELETE,
	IMSG_KPWLABEL_CHANGE,
	IMSG_KPWLABEL_DELETE,
	IMSG_IFSTATUS,
	IMSG_NEWADDR,
	IMSG_DELADDR,
	IMSG_LABEL_MAPPING,
	IMSG_LABEL_MAPPING_FULL,
	IMSG_LABEL_REQUEST,
	IMSG_LABEL_RELEASE,
	IMSG_LABEL_WITHDRAW,
	IMSG_LABEL_ABORT,
	IMSG_REQUEST_ADD,
	IMSG_REQUEST_ADD_END,
	IMSG_MAPPING_ADD,
	IMSG_MAPPING_ADD_END,
	IMSG_RELEASE_ADD,
	IMSG_RELEASE_ADD_END,
	IMSG_WITHDRAW_ADD,
	IMSG_WITHDRAW_ADD_END,
	IMSG_ADDRESS_ADD,
	IMSG_ADDRESS_DEL,
	IMSG_NOTIFICATION,
	IMSG_NOTIFICATION_SEND,
	IMSG_NEIGHBOR_UP,
	IMSG_NEIGHBOR_DOWN,
	IMSG_NETWORK_ADD,
	IMSG_NETWORK_DEL,
	IMSG_RECONF_CONF,
	IMSG_RECONF_IFACE,
	IMSG_RECONF_TNBR,
	IMSG_RECONF_NBRP,
	IMSG_RECONF_L2VPN,
	IMSG_RECONF_L2VPN_IF,
	IMSG_RECONF_L2VPN_PW,
	IMSG_RECONF_END
};

/* interface states */
#define	IF_STA_DOWN		0x01
#define	IF_STA_ACTIVE		0x02

/* interface types */
enum iface_type {
	IF_TYPE_POINTOPOINT,
	IF_TYPE_BROADCAST
};

/* neighbor states */
#define	NBR_STA_PRESENT		0x0001
#define	NBR_STA_INITIAL		0x0002
#define	NBR_STA_OPENREC		0x0004
#define	NBR_STA_OPENSENT	0x0008
#define	NBR_STA_OPER		0x0010
#define	NBR_STA_SESSION		(NBR_STA_INITIAL | NBR_STA_OPENREC | \
				NBR_STA_OPENSENT | NBR_STA_OPER)

/* neighbor events */
enum nbr_event {
	NBR_EVT_NOTHING,
	NBR_EVT_MATCH_ADJ,
	NBR_EVT_CONNECT_UP,
	NBR_EVT_CLOSE_SESSION,
	NBR_EVT_INIT_RCVD,
	NBR_EVT_KEEPALIVE_RCVD,
	NBR_EVT_PDU_RCVD,
	NBR_EVT_PDU_SENT,
	NBR_EVT_INIT_SENT
};

/* neighbor actions */
enum nbr_action {
	NBR_ACT_NOTHING,
	NBR_ACT_RST_KTIMEOUT,
	NBR_ACT_SESSION_EST,
	NBR_ACT_RST_KTIMER,
	NBR_ACT_CONNECT_SETUP,
	NBR_ACT_PASSIVE_INIT,
	NBR_ACT_KEEPALIVE_SEND,
	NBR_ACT_CLOSE_SESSION
};

TAILQ_HEAD(mapping_head, mapping_entry);

struct map {
	uint8_t		type;
	uint32_t	messageid;
	union map_fec {
		struct {
			struct in_addr	prefix;
			uint8_t		prefixlen;
		} ipv4;
		struct {
			uint16_t	type;
			uint32_t	pwid;
			uint32_t	group_id;
			uint16_t	ifmtu;
		} pwid;
	} fec;
	uint32_t	label;
	uint32_t	requestid;
	uint32_t	pw_status;
	uint8_t		flags;
};
#define F_MAP_REQ_ID	0x01	/* optional request message id present */
#define F_MAP_PW_CWORD	0x02	/* pseudowire control word */
#define F_MAP_PW_ID	0x04	/* pseudowire connection id */
#define F_MAP_PW_IFMTU	0x08	/* pseudowire interface parameter */
#define F_MAP_PW_STATUS	0x10	/* pseudowire status */

struct notify_msg {
	uint32_t	status;
	uint32_t	messageid;	/* network byte order */
	uint16_t	type;		/* network byte order */
	uint32_t	pw_status;
	struct map	fec;
	uint8_t		flags;
};
#define F_NOTIF_PW_STATUS	0x01	/* pseudowire status tlv present */
#define F_NOTIF_FEC		0x02	/* fec tlv present */

struct if_addr {
	LIST_ENTRY(if_addr)	 entry;
	struct in_addr		 addr;
	struct in_addr		 mask;
	struct in_addr		 dstbrd;
};
LIST_HEAD(if_addr_head, if_addr);

struct iface {
	LIST_ENTRY(iface)	 entry;
	struct event		 hello_timer;

	char			 name[IF_NAMESIZE];
	struct if_addr_head	 addr_list;
	LIST_HEAD(, adj)	 adj_list;

	time_t			 uptime;
	unsigned int		 ifindex;
	int			 state;
	uint16_t		 hello_holdtime;
	uint16_t		 hello_interval;
	uint16_t		 flags;
	enum iface_type		 type;
	uint8_t			 if_type;
	uint8_t			 linkstate;
};

/* source of targeted hellos */
struct tnbr {
	LIST_ENTRY(tnbr)	 entry;
	struct event		 hello_timer;
	struct adj		*adj;
	struct in_addr		 addr;

	uint16_t		 hello_holdtime;
	uint16_t		 hello_interval;
	uint16_t		 pw_count;
	uint8_t			 flags;
};
#define F_TNBR_CONFIGURED	 0x01
#define F_TNBR_DYNAMIC		 0x02

enum auth_method {
	AUTH_NONE,
	AUTH_MD5SIG
};

/* neighbor specific parameters */
struct nbr_params {
	LIST_ENTRY(nbr_params)	 entry;
	struct in_addr		 lsr_id;
	uint16_t		 keepalive;
	struct {
		enum auth_method	 method;
		char			 md5key[TCP_MD5_KEY_LEN];
		uint8_t			 md5key_len;
	} auth;
	uint8_t			 flags;
};
#define F_NBRP_KEEPALIVE	 0x01

struct l2vpn_if {
	LIST_ENTRY(l2vpn_if)	 entry;
	struct l2vpn		*l2vpn;
	char			 ifname[IF_NAMESIZE];
	unsigned int		 ifindex;
	uint16_t		 flags;
	uint8_t			 link_state;
};

struct l2vpn_pw {
	LIST_ENTRY(l2vpn_pw)	 entry;
	struct l2vpn		*l2vpn;
	struct in_addr		 lsr_id;
	uint32_t		 pwid;
	char			 ifname[IF_NAMESIZE];
	unsigned int		 ifindex;
	uint32_t		 remote_group;
	uint16_t		 remote_mtu;
	uint32_t		 remote_status;
	uint8_t			 flags;
};
#define F_PW_STATUSTLV_CONF	0x01	/* status tlv configured */
#define F_PW_STATUSTLV		0x02	/* status tlv negotiated */
#define F_PW_CWORD_CONF		0x04	/* control word configured */
#define F_PW_CWORD		0x08	/* control word negotiated */
#define F_PW_STATUS_UP		0x10	/* pseudowire is operational */

struct l2vpn {
	LIST_ENTRY(l2vpn)	 entry;
	char			 name[L2VPN_NAME_LEN];
	int			 type;
	int			 pw_type;
	int			 mtu;
	char			 br_ifname[IF_NAMESIZE];
	unsigned int		 br_ifindex;
	LIST_HEAD(, l2vpn_if)	 if_list;
	LIST_HEAD(, l2vpn_pw)	 pw_list;
};
#define L2VPN_TYPE_VPWS		1
#define L2VPN_TYPE_VPLS		2

/* ldp_conf */
enum {
	PROC_MAIN,
	PROC_LDP_ENGINE,
	PROC_LDE_ENGINE
} ldpd_process;

enum socket_type {
	LDP_SOCKET_DISC,
	LDP_SOCKET_EDISC,
	LDP_SOCKET_SESSION
};

enum hello_type {
	HELLO_LINK,
	HELLO_TARGETED
};

struct ldpd_conf {
	struct in_addr		 rtr_id;
	struct in_addr		 trans_addr;
	LIST_HEAD(, iface)	 iface_list;
	LIST_HEAD(, tnbr)	 tnbr_list;
	LIST_HEAD(, nbr_params)	 nbrp_list;
	LIST_HEAD(, l2vpn)	 l2vpn_list;
	uint16_t		 keepalive;
	uint16_t		 thello_holdtime;
	uint16_t		 thello_interval;
	int			 flags;
};
#define	F_LDPD_NO_FIB_UPDATE	0x0001
#define	F_LDPD_TH_ACCEPT	0x0002
#define	F_LDPD_EXPNULL		0x0004

struct ldpd_global {
	int			 cmd_opts;
	time_t			 uptime;
	int			 pfkeysock;
	int			 ldp_disc_socket;
	int			 ldp_edisc_socket;
	int			 ldp_session_socket;
	struct if_addr_head	 addr_list;
	TAILQ_HEAD(, pending_conn) pending_conns;
};

extern struct ldpd_global global;

/* kroute */
struct kroute {
	struct in_addr		 prefix;
	struct in_addr		 nexthop;
	uint32_t		 local_label;
	uint32_t		 remote_label;
	uint16_t		 flags;
	unsigned short		 ifindex;
	uint8_t			 prefixlen;
	uint8_t			 priority;
};

struct kpw {
	unsigned short		 ifindex;
	int			 pw_type;
	struct in_addr		 nexthop;
	uint32_t		 local_label;
	uint32_t		 remote_label;
	uint8_t			 flags;
};

struct kaddr {
	unsigned short		 ifindex;
	struct in_addr		 addr;
	struct in_addr		 mask;
	struct in_addr		 dstbrd;
};

struct kif {
	char			 ifname[IF_NAMESIZE];
	uint64_t		 baudrate;
	int			 flags;
	int			 mtu;
	unsigned short		 ifindex;
	uint8_t			 if_type;
	uint8_t			 link_state;
};

/* control data structures */
struct ctl_iface {
	char			 name[IF_NAMESIZE];
	time_t			 uptime;
	unsigned int		 ifindex;
	int			 state;
	uint16_t		 adj_cnt;
	uint16_t		 flags;
	uint16_t		 hello_holdtime;
	uint16_t		 hello_interval;
	enum iface_type		 type;
	uint8_t			 linkstate;
	uint8_t			 if_type;
};

struct ctl_adj {
	struct in_addr		 id;
	enum hello_type		 type;
	char			 ifname[IF_NAMESIZE];
	struct in_addr		 src_addr;
	uint16_t		 holdtime;
};

struct ctl_nbr {
	struct in_addr		 id;
	struct in_addr		 addr;
	time_t			 uptime;
	int			 nbr_state;
};

struct ctl_rt {
	struct in_addr		 prefix;
	uint8_t			 prefixlen;
	struct in_addr		 nexthop;
	uint32_t		 local_label;
	uint32_t		 remote_label;
	uint8_t			 flags;
	uint8_t			 in_use;
};

struct ctl_pw {
	uint16_t		 type;
	char			 ifname[IF_NAMESIZE];
	uint32_t		 pwid;
	struct in_addr		 lsr_id;
	uint32_t		 local_label;
	uint32_t		 local_gid;
	uint16_t		 local_ifmtu;
	uint32_t		 remote_label;
	uint32_t		 remote_gid;
	uint16_t		 remote_ifmtu;
	uint32_t		 status;
};

/* parse.y */
struct ldpd_conf	*parse_config(char *);
int			 cmdline_symset(char *);

/* kroute.c */
int		 kif_init(void);
void		 kif_redistribute(void);
int		 kr_init(int);
int		 kr_change(struct kroute *);
int		 kr_delete(struct kroute *);
void		 kif_clear(void);
void		 kr_shutdown(void);
void		 kr_fib_couple(void);
void		 kr_fib_decouple(void);
void		 kr_change_egress_label(int);
void		 kr_dispatch_msg(int, short, void *);
void		 kr_show_route(struct imsg *);
void		 kr_ifinfo(char *, pid_t);
struct kif	*kif_findname(char *);
uint8_t		 mask2prefixlen(in_addr_t);
in_addr_t	 prefixlen2mask(uint8_t);
void		 kmpw_set(struct kpw *);
void		 kmpw_unset(struct kpw *);
void		 kmpw_install(const char *, struct kpw *);
void		 kmpw_uninstall(const char *, struct kpw *);

/* log.h */
const char	*nbr_state_name(int);
const char	*if_state_name(int);
const char	*if_type_name(enum iface_type);
const char	*notification_name(uint32_t);

/* util.c */
int		 bad_ip_addr(struct in_addr);

/* ldpd.c */
void	main_imsg_compose_ldpe(int, pid_t, void *, uint16_t);
void	main_imsg_compose_lde(int, pid_t, void *, uint16_t);
void	merge_config(struct ldpd_conf *, struct ldpd_conf *);
void	config_clear(struct ldpd_conf *);
int	imsg_compose_event(struct imsgev *, uint16_t, uint32_t, pid_t,
	    int, void *, uint16_t);
void	imsg_event_add(struct imsgev *);
void	evbuf_enqueue(struct evbuf *, struct ibuf *);
void	evbuf_event_add(struct evbuf *);
void	evbuf_init(struct evbuf *, int, void (*)(int, short, void *), void *);
void	evbuf_clear(struct evbuf *);

/* socket.c */
int		 ldp_create_socket(enum socket_type);
void		 sock_set_recvbuf(int);
int		 sock_set_reuse(int, int);
int		 sock_set_ipv4_mcast_ttl(int, uint8_t);
int		 sock_set_ipv4_tos(int, int);
int		 sock_set_ipv4_recvif(int, int);
int		 sock_set_ipv4_mcast(struct iface *);
int		 sock_set_ipv4_mcast_loop(int);

/* printconf.c */
void	print_config(struct ldpd_conf *);

#endif	/* _LDPD_H_ */
