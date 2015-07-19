/*	$OpenBSD: ldpd.h,v 1.49 2015/07/19 21:01:56 renato Exp $ */

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

#define TCP_MD5_KEY_LEN		80

#define NBR_IDSELF		1
#define NBR_CNTSTART		(NBR_IDSELF + 1)

#define	RT_BUF_SIZE		16384
#define	MAX_RTSOCK_BUF		128 * 1024
#define	LDP_BACKLOG		128

#define	LDPD_FLAG_NO_FIB_UPDATE	0x0001
#define	LDPD_FLAG_TH_ACCEPT	0x0002

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
	void			*data;
	short			 events;
};

enum imsg_type {
	IMSG_NONE,
	IMSG_CTL_RELOAD,
	IMSG_CTL_SHOW_INTERFACE,
	IMSG_CTL_SHOW_DISCOVERY,
	IMSG_CTL_SHOW_NBR,
	IMSG_CTL_SHOW_LIB,
	IMSG_CTL_FIB_COUPLE,
	IMSG_CTL_FIB_DECOUPLE,
	IMSG_CTL_KROUTE,
	IMSG_CTL_KROUTE_ADDR,
	IMSG_CTL_IFINFO,
	IMSG_CTL_END,
	IMSG_CTL_LOG_VERBOSE,
	IMSG_KLABEL_CHANGE,
	IMSG_KLABEL_DELETE,
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
	IMSG_NOTIFICATION_SEND,
	IMSG_NEIGHBOR_UP,
	IMSG_NEIGHBOR_DOWN,
	IMSG_NETWORK_ADD,
	IMSG_NETWORK_DEL,
	IMSG_RECONF_CONF,
	IMSG_RECONF_IFACE,
	IMSG_RECONF_END
};

/* interface states */
#define	IF_STA_NEW		0x00	/* dummy state for reload */
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
	NBR_EVT_INIT_SENT,
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
	struct in_addr	prefix;
	u_int8_t	prefixlen;
	u_int32_t	label;
	u_int32_t	messageid;
	u_int32_t	requestid;
	u_int8_t	flags;
};
#define F_MAP_WILDCARD	0x01	/* wildcard FEC */
#define F_MAP_REQ_ID	0x02	/* optional request message id present */

struct notify_msg {
	u_int32_t	messageid;
	u_int32_t	status;
	u_int32_t	type;
};

struct if_addr {
	LIST_ENTRY(if_addr)	 global_entry;
	LIST_ENTRY(if_addr)	 iface_entry;
	struct in_addr		 addr;
	struct in_addr		 mask;
	struct in_addr		 dstbrd;
};

struct iface {
	LIST_ENTRY(iface)	 entry;
	struct event		 hello_timer;

	char			 name[IF_NAMESIZE];
	LIST_HEAD(, if_addr)	 addr_list;
	LIST_HEAD(, adj)	 adj_list;

	time_t			 uptime;
	unsigned int		 ifindex;
	int			 discovery_fd;
	int			 state;
	u_int16_t		 hello_holdtime;
	u_int16_t		 hello_interval;
	u_int16_t		 flags;
	enum iface_type		 type;
	u_int8_t		 media_type;
	u_int8_t		 linkstate;
};

/* source of targeted hellos */
struct tnbr {
	LIST_ENTRY(tnbr)	 entry;
	struct event		 hello_timer;
	int			 discovery_fd;
	struct adj		*adj;
	struct in_addr		 addr;

	u_int16_t		 hello_holdtime;
	u_int16_t		 hello_interval;
	u_int8_t		 flags;
};
#define F_TNBR_CONFIGURED	 0x01

enum auth_method {
	AUTH_NONE,
	AUTH_MD5SIG,
};

/* neighbor specific parameters */
struct nbr_params {
	LIST_ENTRY(nbr_params)	 entry;
	struct in_addr		 addr;
	struct {
		enum auth_method	 method;
		char			 md5key[TCP_MD5_KEY_LEN];
		u_int8_t		 md5key_len;
	} auth;
};

/* ldp_conf */
enum {
	PROC_MAIN,
	PROC_LDP_ENGINE,
	PROC_LDE_ENGINE
} ldpd_process;

enum hello_type {
	HELLO_LINK,
	HELLO_TARGETED
};

struct ldpd_conf {
	struct event		disc_ev;
	struct event		edisc_ev;
	struct in_addr		rtr_id;
	LIST_HEAD(, iface)	iface_list;
	LIST_HEAD(, if_addr)	addr_list;
	LIST_HEAD(, tnbr)	tnbr_list;
	LIST_HEAD(, nbr_params)	nbrp_list;

	u_int32_t		opts;
#define LDPD_OPT_VERBOSE	0x00000001
#define LDPD_OPT_VERBOSE2	0x00000002
#define LDPD_OPT_NOACTION	0x00000004
	time_t			uptime;
	int			ldp_discovery_socket;
	int			ldp_ediscovery_socket;
	int			ldp_session_socket;
	int			flags;
	u_int16_t		keepalive;
	u_int16_t		thello_holdtime;
	u_int16_t		thello_interval;
};

/* kroute */
struct kroute {
	struct in_addr	prefix;
	struct in_addr	nexthop;
	u_int32_t	local_label;
	u_int32_t	remote_label;
	u_int16_t	flags;
	u_short		ifindex;
	u_int8_t	prefixlen;
	u_int8_t	priority;
};

struct kaddr {
	u_short			 ifindex;
	struct in_addr		 addr;
	struct in_addr		 mask;
	struct in_addr		 dstbrd;
};

struct kif {
	char			 ifname[IF_NAMESIZE];
	u_int64_t		 baudrate;
	int			 flags;
	int			 mtu;
	u_short			 ifindex;
	u_int8_t		 media_type;
	u_int8_t		 link_state;
};

/* control data structures */
struct ctl_iface {
	char			 name[IF_NAMESIZE];
	time_t			 uptime;
	unsigned int		 ifindex;
	int			 state;
	u_int16_t		 adj_cnt;
	u_int16_t		 flags;
	u_int16_t		 hello_holdtime;
	u_int16_t		 hello_interval;
	enum iface_type		 type;
	u_int8_t		 linkstate;
	u_int8_t		 mediatype;
};

struct ctl_adj {
	struct in_addr		 id;
	enum hello_type		 type;
	char			 ifname[IF_NAMESIZE];
	struct in_addr		 src_addr;
	u_int16_t		 holdtime;
};

struct ctl_nbr {
	struct in_addr		 id;
	struct in_addr		 addr;
	time_t			 uptime;
	int			 nbr_state;
};

struct ctl_rt {
	struct in_addr		 prefix;
	u_int8_t		 prefixlen;
	struct in_addr		 nexthop;
	u_int32_t		 local_label;
	u_int32_t		 remote_label;
	u_int8_t		 flags;
	u_int8_t		 in_use;
};

/* parse.y */
struct ldpd_conf	*parse_config(char *, int);
int			 cmdline_symset(char *);

/* kroute.c */
int		 kif_init(void);
void		 kif_redistribute(void);
int		 kr_init(int);
int		 kr_change(struct kroute *);
int		 kr_delete(struct kroute *);
void		 kr_shutdown(void);
void		 kr_fib_couple(void);
void		 kr_fib_decouple(void);
void		 kr_dispatch_msg(int, short, void *);
void		 kr_show_route(struct imsg *);
void		 kr_ifinfo(char *, pid_t);
struct kif	*kif_findname(char *);
void		 kr_reload(void);
u_int8_t	 mask2prefixlen(in_addr_t);
in_addr_t	 prefixlen2mask(u_int8_t);

/* log.h */
const char	*nbr_state_name(int);
const char	*if_state_name(int);
const char	*if_type_name(enum iface_type);
const char	*notification_name(u_int32_t);

/* ldpd.c */
void	main_imsg_compose_ldpe(int, pid_t, void *, u_int16_t);
void	main_imsg_compose_lde(int, pid_t, void *, u_int16_t);
void	merge_config(struct ldpd_conf *, struct ldpd_conf *);
int	imsg_compose_event(struct imsgev *, u_int16_t, u_int32_t, pid_t,
	    int, void *, u_int16_t);
void	imsg_event_add(struct imsgev *);
void	evbuf_enqueue(struct evbuf *, struct ibuf *);
void	evbuf_event_add(struct evbuf *);
void	evbuf_init(struct evbuf *, int, void (*)(int, short, void *), void *);
void	evbuf_clear(struct evbuf *);

/* printconf.c */
void	print_config(struct ldpd_conf *);

#endif	/* _LDPD_H_ */
