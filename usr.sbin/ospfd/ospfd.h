/*	$OpenBSD: ospfd.h,v 1.42 2006/02/01 18:31:47 norby Exp $ */

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
#include <md5.h>
#include <net/if.h>
#include <netinet/in.h>
#include <event.h>

#define CONF_FILE		"/etc/ospfd.conf"
#define	OSPFD_SOCKET		"/var/run/ospfd.sock"
#define OSPFD_USER		"_ospfd"

#define NBR_HASHSIZE		128
#define LSA_HASHSIZE		512

#define NBR_IDSELF		1
#define NBR_CNTSTART		(NBR_IDSELF + 1)

#define	READ_BUF_SIZE		65535
#define	PKG_DEF_SIZE		512	/* compromise */
#define	RT_BUF_SIZE		16384
#define	MAX_RTSOCK_BUF		128 * 1024

#define	OSPFD_FLAG_NO_FIB_UPDATE	0x0001

#define	F_OSPFD_INSERTED	0x0001
#define	F_KERNEL		0x0002
#define	F_CONNECTED		0x0004
#define	F_DOWN			0x0010
#define	F_STATIC		0x0020
#define	F_DYNAMIC		0x0040
#define	F_LONGER		0x0080
#define	F_REDISTRIBUTED		0x0100

#define REDISTRIBUTE_STATIC	0x01
#define REDISTRIBUTE_CONNECTED	0x02
#define REDISTRIBUTE_DEFAULT	0x04

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
	TAILQ_HEAD(, buf)	 bufs;
	u_int32_t		 queued;
	int			 fd;
};

#define	IMSG_HEADER_SIZE	sizeof(struct imsg_hdr)
#define	MAX_IMSGSIZE		8192

struct buf_read {
	u_char			 buf[READ_BUF_SIZE];
	u_char			*rptr;
	size_t			 wpos;
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
	IMSG_CTL_SHOW_DB_EXT,
	IMSG_CTL_SHOW_DB_NET,
	IMSG_CTL_SHOW_DB_RTR,
	IMSG_CTL_SHOW_DB_SELF,
	IMSG_CTL_SHOW_DB_SUM,
	IMSG_CTL_SHOW_DB_ASBR,
	IMSG_CTL_SHOW_NBR,
	IMSG_CTL_SHOW_RIB,
	IMSG_CTL_SHOW_SUM,
	IMSG_CTL_SHOW_SUM_AREA,
	IMSG_CTL_FIB_COUPLE,
	IMSG_CTL_FIB_DECOUPLE,
	IMSG_CTL_AREA,
	IMSG_CTL_KROUTE,
	IMSG_CTL_KROUTE_ADDR,
	IMSG_CTL_IFINFO,
	IMSG_CTL_END,
	IMSG_KROUTE_CHANGE,
	IMSG_KROUTE_DELETE,
	IMSG_IFINFO,
	IMSG_NEIGHBOR_UP,
	IMSG_NEIGHBOR_DOWN,
	IMSG_NEIGHBOR_CHANGE,
	IMSG_NETWORK_ADD,
	IMSG_NETWORK_DEL,
	IMSG_DD,
	IMSG_DD_END,
	IMSG_DB_SNAPSHOT,
	IMSG_DB_END,
	IMSG_LS_REQ,
	IMSG_LS_UPD,
	IMSG_LS_ACK,
	IMSG_LS_FLOOD,
	IMSG_LS_BADREQ,
	IMSG_LS_MAXAGE,
	IMSG_ABR_UP,
	IMSG_ABR_DOWN
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
	LIST_ENTRY(area)	 entry;
	struct in_addr		 id;
	struct lsa_tree		 lsa_tree;

	LIST_HEAD(, iface)	 iface_list;
	LIST_HEAD(, rde_nbr)	 nbr_list;
/*	list			 addr_range_list; */
	u_int32_t		 stub_default_cost;
	u_int32_t		 num_spf_calc;
	u_int32_t		 dead_interval;
	int			 active;
	u_int16_t		 transmit_delay;
	u_int16_t		 hello_interval;
	u_int16_t		 rxmt_interval;
	u_int16_t		 metric;
	u_int8_t		 priority;
	u_int8_t		 transit;
	u_int8_t		 stub;
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

/* spf states */
enum spf_state {
	SPF_IDLE,
	SPF_DELAY,
	SPF_HOLD,
	SPF_HOLDQUEUE
};

enum dst_type {
	DT_NET,
	DT_RTR
};

static const char * const dst_type_names[] = {
	"Network",
	"Router"
};

enum path_type {
	PT_INTRA_AREA,
	PT_INTER_AREA,
	PT_TYPE1_EXT,
	PT_TYPE2_EXT
};

enum rib_type {
	RIB_NET = 1,
	RIB_RTR,
	RIB_EXT
};

static const char * const path_type_names[] = {
	"Intra-Area",
	"Inter-Area",
	"Type 1 ext",
	"Type 2 ext"
};

struct auth_md {
	TAILQ_ENTRY(auth_md)	 entry;
	char			 key[MD5_DIGEST_LENGTH];
	u_int8_t		 keyid;
};

/* lsa list used in RDE and OE */
TAILQ_HEAD(lsa_head, lsa_entry);

struct iface {
	LIST_ENTRY(iface)	 entry;
	struct event		 hello_timer;
	struct event		 wait_timer;
	struct event		 lsack_tx_timer;

	LIST_HEAD(, nbr)	 nbr_list;
	TAILQ_HEAD(, auth_md)	 auth_md_list;
	struct lsa_head		 ls_ack_list;

	char			 name[IF_NAMESIZE];
	struct in_addr		 addr;
	struct in_addr		 dst;
	struct in_addr		 mask;
	struct in_addr		 abr_id;
	char			*auth_key;
	struct nbr		*dr;	/* designated router */
	struct nbr		*bdr;	/* backup designated router */
	struct nbr		*self;
	struct area		*area;

	u_int32_t		 baudrate;
	u_int32_t		 dead_interval;
	u_int32_t		 ls_ack_cnt;
	u_int32_t		 crypt_seq_num;
	unsigned int		 ifindex;
	int			 fd;
	int			 state;
	int			 mtu;
	u_int16_t		 flags;
	u_int16_t		 transmit_delay;
	u_int16_t		 hello_interval;
	u_int16_t		 rxmt_interval;
	u_int16_t		 metric;
	enum iface_type		 type;
	enum auth_type		 auth_type;
	u_int8_t		 media_type;
	u_int8_t		 auth_keyid;
	u_int8_t		 linkstate;
	u_int8_t		 priority;
	u_int8_t		 passive;
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
	struct lsa_tree		lsa_tree;
	LIST_HEAD(, area)	area_list;
	LIST_HEAD(, vertex)	cand_list;

	u_int32_t		opts;
#define OSPFD_OPT_VERBOSE	0x00000001
#define OSPFD_OPT_VERBOSE2	0x00000002
#define OSPFD_OPT_NOACTION	0x00000004
	u_int32_t		spf_delay;
	u_int32_t		spf_hold_time;
	int			spf_state;
	int			ospf_socket;
	int			flags;
	int			redistribute_flags;
	int			options; /* OSPF options */
	u_int8_t		rfc1583compat;
	u_int8_t		border;
};

/* kroute */
struct kroute {
	struct in_addr	prefix;
	struct in_addr	nexthop;
	u_int16_t	flags;
	u_short		ifindex;
	u_int8_t	prefixlen;
};

struct kif {
	char			 ifname[IF_NAMESIZE];
	u_long			 baudrate;
	int			 flags;
	int			 mtu;
	u_short			 ifindex;
	u_int8_t		 media_type;
	u_int8_t		 link_state;
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
	u_int16_t		 transmit_delay;
	u_int16_t		 hello_interval;
	u_int16_t		 flags;
	u_int16_t		 metric;
	u_int16_t		 rxmt_interval;
	enum iface_type		 type;
	u_int8_t		 linkstate;
	u_int8_t		 priority;
	u_int8_t		 passive;
	enum auth_type		 auth_type;
	u_int8_t		 auth_keyid;
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

struct ctl_rt {
	struct in_addr		 prefix;
	struct in_addr		 nexthop;
	struct in_addr		 area;
	struct in_addr		 adv_rtr;
	u_int32_t		 cost;
	u_int32_t		 cost2;
	enum path_type		 p_type;
	enum dst_type		 d_type;
	u_int8_t		 flags;
	u_int8_t		 prefixlen;
};

struct ctl_sum {
	struct in_addr		 rtr_id;
	u_int32_t		 spf_delay;
	u_int32_t		 spf_hold_time;
	u_int32_t		 num_ext_lsa;
	u_int32_t		 num_area;
	u_int8_t		 rfc1583compat;
};

struct ctl_sum_area {
	struct in_addr		 area;
	u_int32_t		 num_iface;
	u_int32_t		 num_adj_nbr;
	u_int32_t		 num_spf_calc;
	u_int32_t		 num_lsa;
};

/* area.c */
struct area	*area_new(void);
int		 area_del(struct area *);
struct area	*area_find(struct ospfd_conf *, struct in_addr);
void		 area_track(struct area *, int);
int		 area_border_router(struct ospfd_conf *);

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
void	 imsg_event_add(struct imsgbuf *); /* needs to be provided externally */

/* in_cksum.c */
int		 in_cksum(void *, int);

/* iso_cksum.c */
u_int16_t	 iso_cksum(void *, u_int16_t, u_int16_t);

/* kroute.c */
int		 kif_init(void);
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

u_int8_t	mask2prefixlen(in_addr_t);
in_addr_t	prefixlen2mask(u_int8_t);

/* ospfd.c */
void	main_imsg_compose_ospfe(int, pid_t, void *, u_int16_t);
void	main_imsg_compose_rde(int, pid_t, void *, u_int16_t);
int	ospf_redistribute(struct kroute *kr);

/* printconf.c */
void	print_config(struct ospfd_conf *);

#endif	/* _OSPFD_H_ */
