/*	$OpenBSD: rde.h,v 1.5 2005/02/27 08:21:15 norby Exp $ */

/*
 * Copyright (c) 2004, 2005 Esben Norby <norby@openbsd.org>
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

struct vertex {
	RB_ENTRY(vertex)	 entry;
	TAILQ_ENTRY(vertex)	 cand;
	struct event		 ev;
	struct in_addr		 nexthop;
	struct vertex		*prev;
	struct rde_nbr		*nbr;
	struct lsa		*lsa;
	time_t			 changed;
	time_t			 stamp;
	u_int32_t		 cost;
	u_int32_t		 ls_id;
	u_int32_t		 adv_rtr;
	u_int8_t		 type;
	u_int8_t		 flooded;
};

/* just the info RDE needs */
struct rde_nbr {
	LIST_ENTRY(rde_nbr)	 entry, hash;
	struct in_addr		 id;
	struct in_addr		 area_id;
	struct lsa_head		 ls_req_list;
	struct area		*area;
	u_int32_t		 peerid;	/* unique ID in DB */
	int			 state;
	int			 self;
};

struct rt_node {
	RB_ENTRY(rt_node)	 entry;
	struct in_addr		 prefix;
	u_int8_t		 prefixlen;
	struct in_addr		 nexthop;
	enum path_type		 p_type;
	u_int32_t		 cost;
};

/* rde.c */
pid_t		 rde(struct ospfd_conf *, int [2], int [2], int [2]);
int		 rde_imsg_compose_parent(int, pid_t, void *, u_int16_t);
int		 rde_imsg_compose_ospfe(int, u_int32_t, pid_t, void *,
		     u_int16_t);
u_int32_t	 rde_router_id(void);
void		 rde_send_kroute(struct rt_node *);
void		 rde_nbr_del(struct rde_nbr *);
int		 rde_nbr_loading(struct area *);
struct rde_nbr	*rde_nbr_self(struct area *);

/* rde_lsdb.c */
void		 lsa_init(struct lsa_tree *);
int		 lsa_compare(struct vertex *, struct vertex *);
void		 vertex_free(struct vertex *);
int		 lsa_newer(struct lsa_hdr *, struct lsa_hdr *);
int		 lsa_check(struct rde_nbr *, struct lsa *, u_int16_t);
int		 lsa_self(struct rde_nbr *, struct lsa *, struct vertex *);
void		 lsa_add(struct rde_nbr *, struct lsa *);
void		 lsa_del(struct rde_nbr *, struct lsa_hdr *);
struct vertex	*lsa_find(struct area *, u_int8_t, u_int32_t, u_int32_t);
struct vertex	*lsa_find_net(struct area *area, u_int32_t);
int		 lsa_num_links(struct vertex *);
void		 lsa_snap(struct area *, u_int32_t);
void		 lsa_dump(struct lsa_tree *, pid_t);
void		 lsa_merge(struct rde_nbr *, struct lsa *, struct vertex *);

/* rde_spf.c */
void		 spf_calc(struct area *);
void		 spf_tree_clr(struct area *);

void		 cand_list_init(void);
void		 cand_list_add(struct vertex *);
struct vertex	*cand_list_pop(void);
bool		 cand_list_present(struct vertex *);
void		 cand_list_clr(void);
bool		 cand_list_empty(void);

void		 spf_timer(int, short, void *);
int		 start_spf_timer(struct ospfd_conf *);
int		 stop_spf_timer(struct ospfd_conf *);
int		 start_spf_holdtimer(struct ospfd_conf *);

void		 rt_init(void);
int		 rt_compare(struct rt_node *, struct rt_node *);
struct rt_node	*rt_find(in_addr_t, u_int8_t);
int		 rt_insert(struct rt_node *);
int		 rt_remove(struct rt_node *);
void		 rt_clear(void);

struct lsa_rtr_link	*get_rtr_link(struct vertex *, int);
struct lsa_net_link	*get_net_link(struct vertex *, int);

RB_PROTOTYPE(lsa_tree, vertex, entry, lsa_compare)

#endif	/* _RDE_H_ */
