/*	$OpenBSD: bgpd.h,v 1.94 2004/02/10 23:10:23 henning Exp $ */

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
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>

#include <poll.h>
#include <stdarg.h>

#define	BGP_VERSION			4
#define	BGP_PORT			179
#define	CONFFILE			"/etc/bgpd.conf"
#define	BGPD_USER			"_bgpd"
#define	PEER_DESCR_LEN			32
#define	TCP_MD5_KEY_LEN			80

#define	MAX_PKTSIZE			4096
#define	MIN_HOLDTIME			3
#define	READ_BUF_SIZE			65535
#define	RT_BUF_SIZE			16384

#define	BGPD_OPT_VERBOSE		0x0001
#define	BGPD_OPT_VERBOSE2		0x0002
#define	BGPD_OPT_NOACTION		0x0004

#define	BGPD_FLAG_NO_FIB_UPDATE		0x0001

#define BGPD_LOG_UPDATES		0x0001

#define	SOCKET_NAME			"/var/run/bgpd.sock"

#define	F_BGPD_INSERTED		0x01
#define	F_KERNEL		0x02
#define	F_CONNECTED		0x04
#define	F_NEXTHOP		0x08
#define	F_DOWN			0x10
#define	F_STATIC		0x20

enum {
	PROC_MAIN,
	PROC_SE,
	PROC_RDE
} bgpd_process;

enum reconf_action {
	RECONF_NONE,
	RECONF_KEEP,
	RECONF_REINIT,
	RECONF_DELETE
};

struct buf {
	TAILQ_ENTRY(buf)	 entries;
	u_char			*buf;
	ssize_t			 size;
	ssize_t			 wpos;
	ssize_t			 rpos;
};

struct msgbuf {
	u_int32_t		 queued;
	int			 sock;
	TAILQ_HEAD(bufs, buf)	 bufs;
};

struct bgpd_addr {
	sa_family_t	af;
	union {
		struct in_addr		v4;
		struct in6_addr		v6;
		u_int8_t		addr8[16];
		u_int16_t		addr16[8];
		u_int32_t		addr32[4];
	} ba;		    /* 128-bit address */
#define v4	ba.v4
#define v6	ba.v6
#define addr8	ba.addr8
#define addr16	ba.addr16
#define addr32	ba.addr32
};

struct bgpd_config {
	int			 opts;
	u_int16_t		 as;
	u_int32_t		 bgpid;
	u_int16_t		 holdtime;
	u_int16_t		 min_holdtime;
	int			 flags;
	int			 log;
	struct sockaddr_in	 listen_addr;
};

struct buf_read {
	u_char			 buf[READ_BUF_SIZE];
	u_char			*rptr;
	ssize_t			 wpos;
};

enum announce_type {
	ANNOUNCE_UNDEF,
	ANNOUNCE_SELF,
	ANNOUNCE_NONE,
	ANNOUNCE_ALL
};

struct peer_config {
	u_int32_t		 id;
	u_int32_t		 groupid;
	char			 group[PEER_DESCR_LEN];
	char			 descr[PEER_DESCR_LEN];
	struct bgpd_addr	 remote_addr;
	struct bgpd_addr	 local_addr;
	u_int32_t		 max_prefix;
	u_int16_t		 remote_as;
	u_int8_t		 ebgp;		/* 1 = ebgp, 0 = ibgp */
	u_int8_t		 distance;	/* 1 = direct, >1 = multihop */
	u_int8_t		 passive;
	u_int16_t		 holdtime;
	u_int16_t		 min_holdtime;
	enum announce_type	 announce_type;
	char			 tcp_md5_key[TCP_MD5_KEY_LEN];
	enum reconf_action	 reconf_action;
};

struct network_config {
	struct bgpd_addr	prefix;
	u_int8_t		prefixlen;
};

TAILQ_HEAD(network_head, network);

struct network {
	struct network_config	net;
	TAILQ_ENTRY(network)	network_l;
};

/* ipc messages */

#define	IMSG_HEADER_SIZE	sizeof(struct imsg_hdr)
#define	MAX_IMSGSIZE		8192

struct imsgbuf {
	int			sock;
	pid_t			pid;
	struct buf_read		r;
	struct msgbuf		w;
};

enum imsg_type {
	IMSG_NONE,
	IMSG_RECONF_CONF,
	IMSG_RECONF_PEER,
	IMSG_RECONF_NETWORK,
	IMSG_RECONF_FILTER,
	IMSG_RECONF_DONE,
	IMSG_UPDATE,
	IMSG_UPDATE_ERR,
	IMSG_SESSION_UP,
	IMSG_SESSION_DOWN,
	IMSG_MRT_REQ,
	IMSG_MRT_MSG,
	IMSG_MRT_END,
	IMSG_KROUTE_CHANGE,
	IMSG_KROUTE_DELETE,
	IMSG_NEXTHOP_ADD,
	IMSG_NEXTHOP_REMOVE,
	IMSG_NEXTHOP_UPDATE,
	IMSG_CTL_SHOW_NEIGHBOR,
	IMSG_CTL_END,
	IMSG_CTL_RELOAD,
	IMSG_CTL_FIB_COUPLE,
	IMSG_CTL_FIB_DECOUPLE,
	IMSG_CTL_NEIGHBOR_UP,
	IMSG_CTL_NEIGHBOR_DOWN,
	IMSG_CTL_KROUTE,
	IMSG_CTL_KROUTE_ADDR,
	IMSG_CTL_SHOW_NEXTHOP,
	IMSG_CTL_SHOW_INTERFACE
};

struct imsg_hdr {
	enum imsg_type	type;
	u_int16_t	len;
	u_int32_t	peerid;
	pid_t		pid;
};

struct imsg {
	struct imsg_hdr	 hdr;
	void		*data;
};

/* needed for session.h parse prototype */
LIST_HEAD(mrt_head, mrt);

/* error subcode for UPDATE; needed in SE and RDE */
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

struct kroute {
	struct in_addr	prefix;
	u_int8_t	prefixlen;
	struct in_addr	nexthop;
	u_int8_t	flags;
	u_short		ifindex;
};

struct kroute_nexthop {
	struct bgpd_addr	nexthop;
	u_int8_t		valid;
	u_int8_t		connected;
	struct bgpd_addr	gateway;
	struct kroute		kr;
};

struct kif {
	u_short			 ifindex;
	int			 flags;
	char			 ifname[IFNAMSIZ];
	u_int8_t		 media_type;
	u_int8_t		 link_state;
	u_long			 baudrate;
	u_int8_t		 nh_reachable;	/* for nexthop verification */
};

struct session_up {
	u_int32_t		remote_bgpid;
	struct bgpd_addr	local_addr;
	struct bgpd_addr	remote_addr;
};

struct ctl_show_nexthop {
	struct bgpd_addr	addr;
	u_int8_t		valid;
};

enum filter_actions {
	ACTION_NONE,
	ACTION_ALLOW,
	ACTION_DENY
};

enum directions {
	DIR_IN=1,
	DIR_OUT
};

enum from_spec {
	FROM_ALL,
	FROM_ADDRESS,
	FROM_DESCR,
	FROM_GROUP
};

enum as_spec {
	AS_NONE,
	AS_ALL,
	AS_SOURCE,
	AS_TRANSIT
};

enum comp_ops {
	OP_NONE,
	OP_RANGE,
	OP_EQ,
	OP_NE,
	OP_LE,
	OP_LT,
	OP_GE,
	OP_GT
};

/* set flags */
#define	SET_LOCALPREF	0x01
#define	SET_MED		0x02
#define	SET_NEXTHOP	0x04
#define	SET_NEXTHOP6	0x08
#define	SET_PREPEND	0x10

struct filter_peers {
	u_int32_t	peerid;
	u_int32_t	groupid;
};

struct filter_match {
	struct {
		struct bgpd_addr	addr;
		u_int8_t		len;
	} prefix;
	struct {
		enum comp_ops		op;
		u_int8_t		len_min;
		u_int8_t		len_max;
	} prefixlen;
	struct {
		u_int16_t		as;
		enum as_spec		type;
	} as;
};

struct filter_set {
	u_int8_t	flags;
	u_int32_t	localpref;
	u_int32_t	med;
	struct in_addr	nexthop;
	struct in6_addr	nexthop6;
	u_int8_t	prepend;
};

TAILQ_HEAD(filter_head, filter_rule);

struct filter_rule {
	TAILQ_ENTRY(filter_rule)	entries;
	enum filter_actions		action;
	enum directions			dir;
	u_int8_t			quick;
	struct filter_peers		peer;
	struct filter_match		match;
	struct filter_set		set;
};

/* prototypes */
/* bgpd.c */
void		 send_nexthop_update(struct kroute_nexthop *);
void		 send_imsg_session(int, pid_t, void *, u_int16_t);

/* buffer.c */
struct buf	*buf_open(ssize_t);
int		 buf_add(struct buf *, void *, ssize_t);
void		*buf_reserve(struct buf *, ssize_t);
int		 buf_close(struct msgbuf *, struct buf *);
void		 buf_free(struct buf *);
void		 msgbuf_init(struct msgbuf *);
void		 msgbuf_clear(struct msgbuf *);
int		 msgbuf_write(struct msgbuf *);
int		 msgbuf_writebound(struct msgbuf *);
int		 msgbuf_unbounded(struct msgbuf *msgbuf);

/* log.c */
void		 log_init(int);
void		 vlog(int, const char *, va_list);
void		 log_peer_warn(const struct peer_config *, const char *, ...);
void		 log_peer_warnx(const struct peer_config *, const char *, ...);
void		 log_warn(const char *, ...);
void		 log_warnx(const char *, ...);
void		 log_info(const char *, ...);
void		 log_debug(const char *, ...);
void		 fatal(const char *);
void		 fatalx(const char *);
void		 fatal_ensure(const char *, int, const char *);
const char	*log_addr(const struct bgpd_addr *);

/* parse.y */
int	 cmdline_symset(char *);

/* config.c */
int	 check_file_secrecy(int fd, const char *fname);

/* imsg.c */
void	 imsg_init(struct imsgbuf *, int);
int	 imsg_read(struct imsgbuf *);
int	 imsg_get(struct imsgbuf *, struct imsg *);
int	 imsg_compose(struct imsgbuf *, int, u_int32_t, void *, u_int16_t);
int	 imsg_compose_pid(struct imsgbuf *, int, pid_t, void *, u_int16_t);
void	 imsg_free(struct imsg *);

/* kroute.c */
int	kr_init(int);
int	kr_change(struct kroute *);
int	kr_delete(struct kroute *);
void	kr_shutdown(void);
void	kr_fib_couple(void);
void	kr_fib_decouple(void);
int	kr_dispatch_msg(void);
int	kr_nexthop_add(struct bgpd_addr *);
void	kr_nexthop_delete(struct bgpd_addr *);
void	kr_show_route(struct imsg *);

/* control.c */
int	control_init(void);
void	control_cleanup(void);
int	control_imsg_relay(struct imsg *);

#endif /* __BGPD_H__ */
