/*	$OpenBSD: rde.h,v 1.1 2015/10/02 04:26:47 renato Exp $ */

/*
 * Copyright (c) 2015 Renato Westphal <renato@openbsd.org>
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

#ifndef _RDE_H_
#define _RDE_H_

#include <sys/types.h>
#include <sys/time.h>
#include <sys/tree.h>
#include <sys/queue.h>
#include <event.h>
#include <limits.h>

#define min(x,y) ((x) <= (y) ? (x) : (y))

/* just the info RDE needs */
struct rde_nbr {
	RB_ENTRY(rde_nbr)	 entry;
	uint32_t		 peerid;
	uint32_t		 ifaceid;
	union eigrpd_addr	 addr;
	struct eigrp_iface	*ei;
	struct eigrp		*eigrp;
	TAILQ_HEAD(,reply_node)	 rijk;		/* outstanding replies */
	uint8_t			 flags;
};
/*
 * We have one "self" neighbor for each interface on which EIGRP is configured.
 * This way we can inject local routes into the DUAL FSM just like any other
 * route received from a remote neighbor. For each instance, we also have two
 * additional special neighbors used to inject redistributed and summarized
 * routes.
 */
#define F_RDE_NBR_SELF		0x01
#define F_RDE_NBR_REDIST	0x02
#define F_RDE_NBR_SUMMARY	0x04

struct reply_node {
	TAILQ_ENTRY(reply_node)	 rn_entry;
	TAILQ_ENTRY(reply_node)	 nbr_entry;
	struct event		 ev_active_timeout;
	struct event		 ev_sia_timeout;
	int			 siaquery_sent;
	int			 siareply_recv;
	struct rt_node		*rn;
	struct rde_nbr		*nbr;
};

struct eigrp_route {
	TAILQ_ENTRY(eigrp_route) entry;
	struct rde_nbr		*nbr;		/* advertising nbr */
	enum route_type		 type;
	union eigrpd_addr	 nexthop;
	uint32_t		 distance;	/* local distance */
	uint32_t		 rdistance;	/* reported distance */
	struct classic_metric	 metric;	/* metric */
	struct classic_emetric	 emetric;	/* external metric */
	uint8_t			 flags;
};
#define F_EIGRP_ROUTE_INSTALLED	0x01
#define F_EIGRP_ROUTE_M_CHANGED	0x02

struct rt_node {
	RB_ENTRY(rt_node)	 entry;
	struct eigrp		*eigrp;
	union eigrpd_addr	 prefix;
	uint8_t			 prefixlen;
	int			 state;
	TAILQ_HEAD(,eigrp_route) routes;
	TAILQ_HEAD(,reply_node)	 rijk;		/* outstanding replies */

	struct {
		struct rde_nbr		*nbr;
		enum route_type		 type;
		uint32_t		 fdistance;
		uint32_t		 rdistance;
		struct classic_metric	 metric;
		struct classic_emetric	 emetric;
	} successor;
};

/* DUAL states */
#define	DUAL_STA_PASSIVE	0x0001
#define	DUAL_STA_ACTIVE0	0x0002
#define	DUAL_STA_ACTIVE1	0x0004
#define	DUAL_STA_ACTIVE2	0x0008
#define	DUAL_STA_ACTIVE3	0x0010
#define	DUAL_STA_ACTIVE_ALL	(DUAL_STA_ACTIVE0 | DUAL_STA_ACTIVE1 | \
				DUAL_STA_ACTIVE2 | DUAL_STA_ACTIVE3)

enum dual_event {
	DUAL_EVT_1,
	DUAL_EVT_2,
	DUAL_EVT_3,
	DUAL_EVT_4,
	DUAL_EVT_5,
	DUAL_EVT_6,
	DUAL_EVT_7,
	DUAL_EVT_8,
	DUAL_EVT_9,
	DUAL_EVT_10,
	DUAL_EVT_11,
	DUAL_EVT_12,
	DUAL_EVT_13,
	DUAL_EVT_14,
	DUAL_EVT_15,
	DUAL_EVT_16
};

/* rde.c */
pid_t		 rde(struct eigrpd_conf *, int [2], int [2], int [2]);
int		 rde_imsg_compose_parent(int, pid_t, void *, uint16_t);
int		 rde_imsg_compose_eigrpe(int, uint32_t, pid_t, void *,
    uint16_t);

void		 rde_instance_init(struct eigrp *);
void		 rde_instance_del(struct eigrp *);
void		 rde_send_change_kroute(struct rt_node *, struct eigrp_route *);
void		 rde_send_delete_kroute(struct rt_node *, struct eigrp_route *);
void		 rt_redist_set(struct kroute *, int);
void		 rt_snap(struct rde_nbr *);
struct ctl_rt	*rt_to_ctl(struct rt_node *, struct eigrp_route *);
void		 rt_dump(struct ctl_show_topology_req *, pid_t);

/* rde_nbr.c */
struct rde_nbr		*rde_nbr_find(uint32_t);
struct rde_nbr		*rde_nbr_new(uint32_t, struct rde_nbr *);
void			 rde_nbr_del(struct rde_nbr *);

/* rde_dual.c */
int			 dual_fsm(struct rt_node *, enum dual_event);
struct rt_node		*rt_find(struct eigrp *, struct rinfo *);
struct rt_node		*rt_new(struct eigrp *, struct rinfo *);
void			 rt_del(struct rt_node *);
struct eigrp_route	*route_find(struct rde_nbr *, struct rt_node *);
struct eigrp_route	*route_new(struct rt_node *, struct rde_nbr *,
    struct rinfo *);
void			 route_del(struct rt_node *, struct eigrp_route *);
uint32_t		 safe_sum_uint32(uint32_t, uint32_t);
uint32_t		 eigrp_composite_delay(uint32_t);
uint32_t		 eigrp_real_delay(uint32_t);
uint32_t		 eigrp_composite_bandwidth(uint32_t);
uint32_t		 eigrp_real_bandwidth(uint32_t);
void			 route_update_metrics(struct eigrp_route *,
    struct rinfo *);
void			 reply_outstanding_add(struct rt_node *,
    struct rde_nbr *);
struct reply_node	*reply_outstanding_find(struct rt_node *,
    struct rde_nbr *);
void			 reply_outstanding_remove(struct reply_node *);
void			 rinfo_fill_successor(struct rt_node *, struct rinfo *);
void			 rinfo_fill_infinite(struct rt_node *, enum route_type,
    struct rinfo *);
void			 rt_update_fib(struct rt_node *);
void			 rt_set_successor(struct rt_node *,
    struct eigrp_route *);
struct eigrp_route	*rt_get_successor_fc(struct rt_node *);

void			 rde_send_ack(struct rde_nbr *);
void			 rde_send_update(struct eigrp_iface *, struct rinfo *);
void			 rde_send_update_all(struct rt_node *, struct rinfo *);
void			 rde_send_query(struct eigrp_iface *, struct rinfo *,
    int);
void			 rde_send_siaquery(struct rde_nbr *, struct rinfo *);
void			 rde_send_query_all(struct eigrp *, struct rt_node *,
    int);
void			 rde_flush_queries(void);
void			 rde_send_reply(struct rde_nbr *, struct rinfo *, int);
void			 rde_check_update(struct rde_nbr *, struct rinfo *);
void			 rde_check_query(struct rde_nbr *, struct rinfo *, int);
void			 rde_last_reply(struct rt_node *);
void			 rde_check_reply(struct rde_nbr *, struct rinfo *, int);
void			 rde_check_link_down_rn(struct rde_nbr *,
    struct rt_node *, struct eigrp_route *);
void			 rde_check_link_down_nbr(struct rde_nbr *);
void			 rde_check_link_down(unsigned int);
struct eigrp_interface;
void			 rde_check_link_cost_change(struct rde_nbr *,
    struct eigrp_interface *);

#endif	/* _RDE_H_ */
