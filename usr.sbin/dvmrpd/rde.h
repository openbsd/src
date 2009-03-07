/*	$OpenBSD: rde.h,v 1.12 2009/03/07 12:47:17 michele Exp $ */

/*
 * Copyright (c) 2005, 2006 Esben Norby <norby@openbsd.org>
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

struct adv_rtr {
	struct in_addr		 addr;
	u_int32_t		 metric;
};

struct rt_node {
	RB_ENTRY(rt_node)	 entry;
	struct event		 expiration_timer;
	struct event		 holddown_timer;
	u_int8_t		 ttls[MAXVIFS];	/* downstream vif(s) */
	struct in_addr		 prefix;
	struct in_addr		 nexthop;
	u_int32_t		 cost;
	u_int32_t		 old_cost;	/* used when in hold-down */
	u_short			 ifindex;	/* learned from this iface */
	struct adv_rtr		 adv_rtr[MAXVIFS];
	u_int16_t		 ds_cnt[MAXVIFS];
	LIST_HEAD(, ds_nbr)	 ds_list;
	time_t			 uptime;
	u_int8_t		 flags;
	u_int8_t		 prefixlen;
	u_int8_t		 invalid;
	u_int8_t		 connected;
};

struct mfc_node {
	RB_ENTRY(mfc_node)	 entry;
	struct event		 expiration_timer;
	u_int8_t		 ttls[MAXVIFS];	/* outgoing vif(s) */
	struct in_addr		 origin;
	struct in_addr		 group;
	u_short			 ifindex;	/* incoming vif */
	time_t			 uptime;
};

/* downstream neighbor per source */
struct ds_nbr {
	LIST_ENTRY(ds_nbr)	 entry;
	struct in_addr		 addr;
};

/* rde.c */
pid_t	rde(struct dvmrpd_conf *, int [2], int [2], int [2]);
int	rde_imsg_compose_parent(int, pid_t, void *, u_int16_t);
int	rde_imsg_compose_dvmrpe(int, u_int32_t, pid_t, void *, u_int16_t);

void	rde_group_list_add(struct iface *, struct in_addr);
int	rde_group_list_find(struct iface *, struct in_addr);
void	rde_group_list_remove(struct iface *, struct in_addr);

/* rde_mfc.c */
void		 mfc_init(void);
int		 mfc_compare(struct mfc_node *, struct mfc_node *);
struct mfc_node *mfc_find(in_addr_t, in_addr_t);
int		 mfc_insert(struct mfc_node *);
int		 mfc_remove(struct mfc_node *);
void		 mfc_clear(void);
void		 mfc_dump(pid_t);
void		 mfc_update(struct mfc *);
void		 mfc_delete(struct mfc *);
void		 mfc_update_source(struct rt_node *);
int		 mfc_check_members(struct rt_node *, struct iface *);

/* rde_srt.c */
void		 rt_init(void);
int		 rt_compare(struct rt_node *, struct rt_node *);
struct rt_node	*rt_find(in_addr_t, u_int8_t);
struct rt_node	*rr_new_rt(struct route_report *, u_int32_t, int);
int		 rt_insert(struct rt_node *);
void		 rt_update(struct rt_node *);
int		 rt_remove(struct rt_node *);
void		 rt_clear(void);
void		 rt_snap(u_int32_t);
void		 rt_dump(pid_t);

int		 srt_check_route(struct route_report *, int);
int		 src_compare(struct src_node *, struct src_node *);

void		 srt_expire_nbr(struct in_addr, struct iface *);

RB_PROTOTYPE(src_head, src_node, entry, src_compare);

#endif	/* _RDE_H_ */
