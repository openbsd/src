/*	$OpenBSD: ospfd.h,v 1.7 2005/02/09 17:41:16 claudio Exp $ */

/*
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

#ifndef _OSPFD_H_
#define _OSPFD_H_

#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/tree.h>
#include <net/if.h>
#include <netinet/in.h>
#include <event.h>
#include <stdbool.h>

#define CONF_FILE			"/etc/ospfd.conf"
#define	OSPFD_SOCKET			"/var/run/ospfd.sock"
#define OSPFD_USER			"_ospfd"

#define NBR_HASHSIZE		128
#define LSA_HASHSIZE		512

#define	READ_BUF_SIZE		65535
#define	RT_BUF_SIZE		16384

#define	OSPFD_FLAG_NO_FIB_UPDATE	0x0001

#define	F_OSPFD_INSERTED	0x0001
#define	F_KERNEL		0x0002
#define	F_CONNECTED		0x0004
#define	F_NEXTHOP		0x0008
#define	F_DOWN			0x0010
#define	F_STATIC		0x0020
#define	F_LONGER		0x0040
#define	F_REJECT		0x0080
#define	F_BLACKHOLE		0x0100

/* buffer */
struct buf {
	TAILQ_ENTRY(buf)	 entry;
	u_char			*buf;
	size_t			 size;
	size_t			 max;
	size_t			 wpos;
	size_t			 rpos;
	int			 fd;
};

struct msgbuf {
	u_int32_t		 queued;
	int			 fd;
	TAILQ_HEAD(, buf)	 bufs;
};

#define	IMSG_HEADER_SIZE	sizeof(struct imsg_hdr)
#define	MAX_IMSGSIZE		8192

struct buf_read {
	u_char			 buf[READ_BUF_SIZE];
	u_char			*rptr;
	ssize_t			 wpos;
};

struct imsgbuf {
	TAILQ_HEAD(, imsg_fd)	fds;
	struct buf_read		r;
	struct msgbuf		w;
	struct event		ev;
	void			(*handler)(int, short, void *);
	int			fd;
	pid_t			pid;
	short			events;
};

enum imsg_type {
	IMSG_NONE,
	IMSG_CTL_RELOAD,
	IMSG_CTL_SHOW_INTERFACE,
	IMSG_CTL_SHOW_DATABASE,
	IMSG_CTL_SHOW_NBR,
	IMSG_CTL_FIB_COUPLE,
	IMSG_CTL_FIB_DECOUPLE,
	IMSG_CTL_AREA,
	IMSG_CTL_END,
	IMSG_NEXTHOP_ADD,
	IMSG_NEXTHOP_REMOVE,
	IMSG_NEXTHOP_UPDATE,
	IMSG_IFINFO,
	IMSG_NEIGHBOR_UP,
	IMSG_NEIGHBOR_DOWN,
	IMSG_NEIGHBOR_CHANGE,
	IMSG_DD,
	IMSG_DB_SNAPSHOT,
	IMSG_DB_END,
	IMSG_LS_REQ,
	IMSG_LS_UPD,
	IMSG_LS_ACK,
	IMSG_LS_FLOOD,
	IMSG_LS_BADREQ,
	IMSG_LS_MAXAGE
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

struct imsg_fd {
	TAILQ_ENTRY(imsg_fd)	entry;
	int			fd;
};

/* area */
struct vertex;
struct rde_nbr;
RB_HEAD(lsa_tree, vertex);

struct area {
	LIST_ENTRY(area)		 entry;
	struct in_addr			 id;

	struct lsa_tree			 lsa_tree;
	LIST_HEAD(, iface)		 iface_list;
	LIST_HEAD(, rde_nbr)		 nbr_list;
/*	list				 addr_range_list; */
	u_int32_t			 stub_default_cost;

	u_int32_t	dead_interval;
	u_int16_t	transfer_delay;
	u_int16_t	hello_interval;
	u_int16_t	rxmt_interval;
	u_int16_t	metric;
	u_int8_t	priority;

	bool				 transit;
	bool				 stub;
};

/* interface states */
#define	IF_STA_DOWN		0x01
#define	IF_STA_LOOPBACK		0x02
#define	IF_STA_WAITING		0x04
#define	IF_STA_POINTTOPOINT	0x08
#define	IF_STA_DROTHER		0x10
#define	IF_STA_BACKUP		0x20
#define	IF_STA_DR		0x40
#define IF_STA_DRORBDR		(IF_STA_DR | IF_STA_BACKUP)
#define	IF_STA_MULTI		(IF_STA_DROTHER | IF_STA_BACKUP | IF_STA_DR)
#define	IF_STA_ANY		0x7f

/* interface events */
enum iface_event {
	IF_EVT_NOTHING,
	IF_EVT_UP,
	IF_EVT_WTIMER,
	IF_EVT_BACKUP_SEEN,
	IF_EVT_NBR_CHNG,
	IF_EVT_LOOP,
	IF_EVT_UNLOOP,
	IF_EVT_DOWN
};

static const char * const if_event_names[] = {
	"NOTHING",
	"UP",
	"WAITTIMER",
	"BACKUPSEEN",
	"NEIGHBORCHANGE",
	"LOOP",
	"UNLOOP",
	"DOWN"
};

/* interface actions */
enum iface_action {
	IF_ACT_NOTHING,
	IF_ACT_STRT,
	IF_ACT_ELECT,
	IF_ACT_RST
};

/* interface types */
enum iface_type {
	IF_TYPE_POINTOPOINT,
	IF_TYPE_BROADCAST,
	IF_TYPE_NBMA,
	IF_TYPE_POINTOMULTIPOINT,
	IF_TYPE_VIRTUALLINK
};

/* auth types */
enum auth_type {
	AUTH_NONE,
	AUTH_SIMPLE,
	AUTH_CRYPT
};

/* lsa list used in RDE and OE */
TAILQ_HEAD(lsa_head, lsa_entry);

struct iface {
	LIST_ENTRY(iface)	 entry;
	struct event		 hello_timer;
	struct event		 wait_timer;
	struct event		 lsack_tx_timer;

	LIST_HEAD(, nbr)	 nbr_list;
	struct lsa_head		 ls_ack_list;

	char			 name[IF_NAMESIZE];
	struct in_addr		 addr;
	struct in_addr		 mask;
	struct in_addr		 rtr_id;
	char			*auth_key;
	struct nbr		*dr;	/* designated router */
	struct nbr		*bdr;	/* backup designated router */
	struct nbr		*self;
	struct area		*area;

	u_int32_t		 baudrate;
	u_int32_t		 dead_interval;
	u_int32_t		 ls_ack_cnt;
	unsigned int		 ifindex;
	int			 fd;
	int			 state;
	int			 mtu;
	u_int16_t		 flags;
	u_int16_t		 transfer_delay;
	u_int16_t		 hello_interval;
	u_int16_t		 rxmt_interval;
	u_int16_t		 metric;
	enum iface_type		 type;
	enum auth_type		 auth_type;
	u_int8_t		 linkstate;
	u_int8_t		 priority;

	bool			 passive;
};

/* ospf_conf */
enum {
	PROC_MAIN,
	PROC_OSPF_ENGINE,
	PROC_RDE_ENGINE
} ospfd_process;

struct ospfd_conf {
	struct event		ev;
	struct in_addr		rtr_id;
	u_int32_t		opts;
#define OSPFD_OPT_VERBOSE	0x00000001
#define OSPFD_OPT_VERBOSE2	0x00000002
#define OSPFD_OPT_NOACTION	0x00000004
	int			maxdepth;
	LIST_HEAD(, area)	area_list;

	struct lsa_tree		lsa_tree;
	int			ospf_socket;
	int			flags;
	int			options; /* OSPF options */
};

/* kroute */
struct kroute {
	struct in_addr	prefix;
	u_int8_t	prefixlen;
	struct in_addr	nexthop;
	u_int16_t	flags;
	u_short		ifindex;
};

struct kroute_nexthop {
	struct in_addr		nexthop;
	u_int8_t		valid;
	u_int8_t		connected;
	struct in_addr		gateway;
	struct kroute		kr;
};

struct kif {
	u_short			 ifindex;
	int			 flags;
	char			 ifname[IF_NAMESIZE];
	u_int8_t		 media_type;
	u_int8_t		 link_state;
	u_long			 baudrate;
	u_int8_t		 nh_reachable;	/* for nexthop verification */
};

/* control data structures */
struct ctl_iface {
	char			 name[IF_NAMESIZE];
	struct in_addr		 addr;
	struct in_addr		 mask;
	struct in_addr		 area;
	struct in_addr		 rtr_id;
	struct in_addr		 dr_id;
	struct in_addr		 dr_addr;
	struct in_addr		 bdr_id;
	struct in_addr		 bdr_addr;
	time_t			 hello_timer;
	u_int32_t		 baudrate;
	u_int32_t		 dead_interval;
	unsigned int		 ifindex;
	int			 state;
	int			 mtu;
	int			 nbr_cnt;
	int			 adj_cnt;
	u_int16_t		 transfer_delay;
	u_int16_t		 hello_interval;
	u_int16_t		 flags;
	u_int16_t		 metric;
	u_int16_t		 rxmt_interval;
	enum iface_type		 type;
	u_int8_t		 linkstate;
	u_int8_t		 priority;
	bool			 passive;
};

struct ctl_nbr {
	char			 name[IF_NAMESIZE];
	struct in_addr		 id;
	struct in_addr		 addr;
	struct in_addr		 dr;
	struct in_addr		 bdr;
	struct in_addr		 area;
	time_t			 dead_timer;
	u_int32_t		 db_sum_lst_cnt;
	u_int32_t		 ls_req_lst_cnt;
	u_int32_t		 ls_retrans_lst_cnt;
	u_int32_t		 state_chng_cnt;
	int			 nbr_state;
	int			 iface_state;
	u_int8_t		 priority;
	u_int8_t		 options;
};

void		 show_config(struct ospfd_conf *xconf);

/* area.c */
struct area	*area_new(void);
int		 area_del(struct area *);
struct area	*area_find(struct ospfd_conf *, struct in_addr);

/* buffer.c */
struct buf	*buf_open(size_t);
struct buf	*buf_dynamic(size_t, size_t);
int		 buf_add(struct buf *, void *, size_t);
void		*buf_reserve(struct buf *, size_t);
void		*buf_seek(struct buf *, size_t, size_t);
int		 buf_close(struct msgbuf *, struct buf *);
int		 buf_write(int, struct buf *);
void		 buf_free(struct buf *);
void		 msgbuf_init(struct msgbuf *);
void		 msgbuf_clear(struct msgbuf *);
int		 msgbuf_write(struct msgbuf *);
int		 msgbuf_writebound(struct msgbuf *);
int		 msgbuf_unbounded(struct msgbuf *msgbuf);

/* parse.y */
struct ospfd_conf	*parse_config(char *, int);
int			 cmdline_symset(char *);

/* imsg.c */
void	 imsg_init(struct imsgbuf *, int, void (*)(int, short, void *));
int	 imsg_read(struct imsgbuf *);
int	 imsg_get(struct imsgbuf *, struct imsg *);
int	 imsg_compose(struct imsgbuf *, enum imsg_type, u_int32_t, pid_t, int,
	    void *, u_int16_t);
struct buf	*imsg_create(struct imsgbuf *, enum imsg_type, u_int32_t, pid_t,
		    u_int16_t);
int	 imsg_add(struct buf *, void *, u_int16_t);
int	 imsg_close(struct imsgbuf *, struct buf *);
void	 imsg_free(struct imsg *);
int	 imsg_get_fd(struct imsgbuf *);
void	 imsg_event_add(struct imsgbuf *); /* needs to be provided externally */

/* in_cksum.c */
int		 in_cksum(void *, int);

/* iso_cksum.c */
u_int16_t	 iso_cksum(void *, u_int16_t, u_int16_t);

/* kroute.c */
int	kr_init(int);
int	kr_change(struct kroute *);
int	kr_delete(struct kroute *);
void	kr_shutdown(void);
void	kr_fib_couple(void);
void	kr_fib_decouple(void);
void	kr_dispatch_msg(int, short, void *);
int	kr_nexthop_add(struct in_addr);
void	kr_nexthop_delete(struct in_addr);
void	kr_show_route(struct imsg *);
void	kr_ifinfo(char *);
void	send_nexthop_update(struct kroute_nexthop *);

/* ospfd.c */
void	 send_nexthop_update(struct kroute_nexthop *);

#endif	/* _OSPFD_H_ */
