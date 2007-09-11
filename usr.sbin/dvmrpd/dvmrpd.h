/*	$OpenBSD: dvmrpd.h,v 1.8 2007/09/11 18:23:05 claudio Exp $ */

/*
 * Copyright (c) 2004, 2005, 2006 Esben Norby <norby@openbsd.org>
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

#ifndef _DVMRPD_H_
#define _DVMRPD_H_

#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/tree.h>
#include <net/if.h>
#include <netinet/in.h>
#include <event.h>

#define CONF_FILE		"/etc/dvmrpd.conf"
#define	DVMRPD_SOCKET		"/var/run/dvmrpd.sock"
#define DVMRPD_USER		"_dvmrpd"

#define NBR_HASHSIZE		128

#define NBR_IDSELF		1
#define NBR_CNTSTART		(NBR_IDSELF + 1)

#define	READ_BUF_SIZE		65535
#define	RT_BUF_SIZE		16384

#define	DVMRPD_FLAG_NO_FIB_UPDATE	0x0001

#define	F_DVMRPD_INSERTED	0x0001
#define	F_KERNEL		0x0002
#define	F_CONNECTED		0x0004
#define	F_DOWN			0x0010
#define	F_STATIC		0x0020
#define	F_LONGER		0x0040

#define MAXVIFS			32	/* XXX */

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
	IMSG_CTL_SHOW_IFACE,
	IMSG_CTL_SHOW_IGMP,
	IMSG_CTL_SHOW_NBR,
	IMSG_CTL_SHOW_RIB,
	IMSG_CTL_SHOW_MFC,
	IMSG_CTL_MFC_COUPLE,
	IMSG_CTL_MFC_DECOUPLE,
	IMSG_CTL_KROUTE,
	IMSG_CTL_KROUTE_ADDR,
	IMSG_CTL_IFINFO,
	IMSG_CTL_SHOW_SUM,
	IMSG_CTL_END,
	IMSG_IFINFO,
	IMSG_ROUTE_REPORT,
	IMSG_FULL_ROUTE_REPORT,
	IMSG_FULL_ROUTE_REPORT_END,
	IMSG_MFC_ADD,
	IMSG_MFC_DEL
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

/* interface states */
#define	IF_STA_DOWN		0x01
#define IF_STA_QUERIER		0x02
#define	IF_STA_NONQUERIER	0x04
#define	IF_STA_ANY		0x07
#define	IF_STA_ACTIVE		(~IF_STA_DOWN)

/* interface events */
enum iface_event {
	IF_EVT_NOTHING,
	IF_EVT_UP,
	IF_EVT_QTMOUT,		/* query timer expired */
	IF_EVT_QRECVD,		/* query received, check for lower IP */
	IF_EVT_QPRSNTTMOUT,	/* other querier present timeout */
	IF_EVT_DOWN
};

/* interface actions */
enum iface_action {
	IF_ACT_NOTHING,
	IF_ACT_STRT,
	IF_ACT_QPRSNT,
	IF_ACT_RST
};

/* interface types */
enum iface_type {
	IF_TYPE_POINTOPOINT,
	IF_TYPE_BROADCAST
};

/* neighbor states */
#define	NBR_STA_DOWN		0x01
#define	NBR_STA_1_WAY		0x02
#define	NBR_STA_2_WAY		0x04
#define NBR_STA_ACTIVE		(~NBR_STA_DOWN)
#define NBR_STA_ANY		0xff

struct group {
	TAILQ_ENTRY(group)	 entry;
	struct event		 dead_timer;
	struct event		 v1_host_timer;
	struct event		 retrans_timer;

	struct in_addr		 addr;

	struct iface		*iface;

	time_t			 uptime;
	int			 state;
};

struct mfc {
	struct in_addr		 origin;
	struct in_addr		 group;
	u_short			 ifindex;
	u_int8_t		 ttls[MAXVIFS];
};

TAILQ_HEAD(rr_head, rr_entry);
RB_HEAD(src_head, src_node);

struct iface {
	LIST_ENTRY(iface)	 entry;
	struct event		 probe_timer;
	struct event		 query_timer;
	struct event		 querier_present_timer;
	time_t			 uptime;
	LIST_HEAD(, nbr)	 nbr_list;
	TAILQ_HEAD(, group)	 group_list;
	struct rr_head		 rr_list;

	char			 name[IF_NAMESIZE];
	struct in_addr		 addr;
	struct in_addr		 dst;
	struct in_addr		 mask;
	struct in_addr		 querier;	/* designated querier */

	u_int64_t		 baudrate;
	u_int32_t		 gen_id;
	u_int32_t		 group_cnt;
	u_int32_t		 probe_interval;

	u_int32_t		 query_interval;
	u_int32_t		 query_resp_interval;
	u_int32_t		 recv_query_resp_interval;
	u_int32_t		 group_member_interval;
	u_int32_t		 querier_present_interval;
	u_int32_t		 startup_query_interval;
	u_int32_t		 startup_query_cnt;
	u_int32_t		 last_member_query_interval;
	u_int32_t		 last_member_query_cnt;
	u_int32_t		 last_member_query_time;
	u_int32_t		 v1_querier_present_tmout;
	u_int32_t		 v1_host_present_interval;
	u_int32_t		 startup_query_counter; /* actual counter */
	u_int32_t		 dead_interval;

	unsigned int		 ifindex;		/* ifindex and vif */
	int			 fd;
	int			 state;
	int			 mtu;
	int			 nbr_cnt;
	int			 adj_cnt;

	u_int16_t		 flags;
	u_int16_t		 metric;
	enum iface_type		 type;

	u_int8_t		 robustness;
	u_int8_t		 linkstate;
	u_int8_t		 media_type;
	u_int8_t		 passive;
	u_int8_t		 igmp_version;
};

/* dvmrp_conf */
enum {
	PROC_MAIN,
	PROC_DVMRP_ENGINE,
	PROC_RDE_ENGINE
} dvmrpd_process;

struct dvmrpd_conf {
	struct event		 ev;
	struct event		 report_timer;
	u_int32_t		 gen_id;
	u_int32_t		 opts;
#define DVMRPD_OPT_VERBOSE	 0x00000001
#define DVMRPD_OPT_VERBOSE2	 0x00000002
#define DVMRPD_OPT_NOACTION	 0x00000004
	int			 maxdepth;
	LIST_HEAD(, iface)	 iface_list;
	struct src_head		 src_list;
	int			 dvmrp_socket;
	int			 mroute_socket;
	int			 flags;
};

/* kroute */
struct kroute {
	struct in_addr		 prefix;
	struct in_addr		 nexthop;
	u_int16_t		 flags;
	u_short			 ifindex;
	u_int8_t		 prefixlen;
};

struct kif {
	char			 ifname[IF_NAMESIZE];
	u_int64_t		 baudrate;
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
	struct in_addr		 querier;

	time_t			 probe_timer;
	time_t			 query_timer;
	time_t			 querier_present_timer;
	time_t			 uptime;

	u_int64_t		 baudrate;
	u_int32_t		 gen_id;
	u_int32_t		 group_cnt;
	u_int32_t		 probe_interval;
	u_int32_t		 query_interval;
	u_int32_t		 query_resp_interval;
	u_int32_t		 recv_query_resp_interval;
	u_int32_t		 group_member_interval;
	u_int32_t		 querier_present_interval;
	u_int32_t		 startup_query_interval;
	u_int32_t		 startup_query_cnt;
	u_int32_t		 last_member_query_interval;
	u_int32_t		 last_member_query_cnt;
	u_int32_t		 last_member_query_time;
	u_int32_t		 v1_querier_present_tmout;
	u_int32_t		 v1_host_present_interval;
	u_int32_t		 dead_interval;

	unsigned int		 ifindex;
	int			 state;
	int			 mtu;
	int			 nbr_cnt;
	int			 adj_cnt;

	u_int16_t		 flags;
	u_int16_t		 metric;
	enum iface_type		 type;
	u_int8_t		 robustness;
	u_int8_t		 linkstate;
	u_int8_t		 mediatype;
	u_int8_t		 passive;
	u_int8_t		 igmp_version;
};

struct ctl_group {
	time_t			 dead_timer;
	time_t			 v1_host_timer;
	time_t			 retrans_timer;
	time_t			 uptime;
	struct in_addr		 addr;
	int			 state;
};

struct ctl_nbr {
	char			 name[IF_NAMESIZE];
	struct in_addr		 id;
	struct in_addr		 addr;
	time_t			 dead_timer;
	time_t			 uptime;
	int			 state;
};

struct ctl_rt {
	struct in_addr		 prefix;
	struct in_addr		 nexthop;
	struct in_addr		 area;
	struct in_addr		 adv_rtr;
	time_t			 uptime;
	time_t			 expire;
	u_int32_t		 cost;
	u_int8_t		 flags;
	u_int8_t		 prefixlen;
};

struct ctl_mfc {
	u_int8_t		 ttls[MAXVIFS];	/* outgoing vif(s) */
	struct in_addr		 origin;
	struct in_addr		 group;
	time_t			 uptime;
	time_t			 expire;
	u_short			 ifindex;	/* incoming vif */
};

struct ctl_sum {
	struct in_addr		 rtr_id;
	u_int32_t		 hold_time;
};

/* buffer.c */
struct buf	*buf_open(size_t);
struct buf	*buf_dynamic(size_t, size_t);
int		 buf_add(struct buf *, void *, size_t);
void		*buf_reserve(struct buf *, size_t);
void		*buf_seek(struct buf *, size_t, size_t);
int		 buf_close(struct msgbuf *, struct buf *);
void		 buf_free(struct buf *);
void		 msgbuf_init(struct msgbuf *);
void		 msgbuf_clear(struct msgbuf *);
int		 msgbuf_write(struct msgbuf *);

/* dvmrpd.c */
void main_imsg_compose_dvmrpe(int, pid_t, void *, u_int16_t);

/* parse.y */
struct dvmrpd_conf	*parse_config(char *, int);
int			 cmdline_symset(char *);

/* imsg.c */
void	 imsg_init(struct imsgbuf *, int, void (*)(int, short, void *));
ssize_t	 imsg_read(struct imsgbuf *);
ssize_t	 imsg_get(struct imsgbuf *, struct imsg *);
int	 imsg_compose(struct imsgbuf *, enum imsg_type, u_int32_t, pid_t,
	    void *, u_int16_t);
struct buf	*imsg_create(struct imsgbuf *, enum imsg_type, u_int32_t, pid_t,
		    u_int16_t);
int	 imsg_add(struct buf *, void *, u_int16_t);
int	 imsg_close(struct imsgbuf *, struct buf *);
void	 imsg_free(struct imsg *);
void	 imsg_event_add(struct imsgbuf *); /* needs to be provided externally */

/* in_cksum.c */
u_int16_t	 in_cksum(void *, size_t);

/* kroute.c */
int		 kif_init(void);
void		 kif_clear(void);
int		 kr_init(void);
void		 kr_shutdown(void);
void		 kr_dispatch_msg(int, short, void *);
void		 kr_ifinfo(char *);
struct kif	*kif_findname(char *);

u_int8_t	 prefixlen_classful(in_addr_t);
u_int8_t	 mask2prefixlen(in_addr_t);
in_addr_t	 prefixlen2mask(u_int8_t);

/* kmroute.c */
int		 kmr_init(int);
void		 kmr_shutdown(void);
void		 kmr_recv_msg(int, short, void *);
void		 kmr_mfc_couple(void);
void		 kmr_mfc_decouple(void);
void		 kmroute_clear(void);
int		 mrt_init(int);
int		 mrt_done(int);
int		 mrt_add_vif(int, struct iface *);
void		 mrt_del_vif(int, struct iface *);
int		 mrt_add_mfc(int, struct mfc *);
int		 mrt_del_mfc(int, struct mfc *);

/* log.h */
const char	*nbr_state_name(int);
const char	*if_state_name(int);
const char	*if_type_name(enum iface_type);
const char	*group_state_name(int);

/* printconf.c */
void		 print_config(struct dvmrpd_conf *);

/* interface.c */
struct iface	*if_find_index(u_short);

#define	PREFIX_SIZE(x)	(((x) + 7) / 8)

#endif	/* _DVMRPD_H_ */
