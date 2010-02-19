/*	$OpenBSD: lde.h,v 1.6 2010/02/19 12:49:21 claudio Exp $ */

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

#ifndef _LDE_H_
#define _LDE_H_

#include <sys/types.h>
#include <sys/time.h>
#include <sys/tree.h>
#include <sys/queue.h>
#include <event.h>
#include <limits.h>

/* Label mapping request pending */
struct lde_req_entry {
	TAILQ_ENTRY(lde_req_entry)	entry;
	struct in_addr			prefix;
	u_int8_t			prefixlen;
};

/* Label mapping message sent */
struct lde_map_entry {
	TAILQ_ENTRY(lde_map_entry)	entry;
	struct in_addr			prefix;
	u_int8_t			prefixlen;
	u_int32_t			label;
};

/* Addresses belonging to neighbor */
struct lde_nbr_address {
	TAILQ_ENTRY(lde_nbr_address)	entry;
	struct in_addr			addr;
};

/* just the info LDE needs */
struct lde_nbr {
	LIST_ENTRY(lde_nbr)		 hash, entry;
	struct in_addr			 id;

	TAILQ_HEAD(, lde_req_entry)	 req_list;
	TAILQ_HEAD(, lde_map_entry)	 recv_map_list;
	TAILQ_HEAD(, lde_map_entry)	 sent_map_list;
	TAILQ_HEAD(, lde_nbr_address)	 addr_list;
	TAILQ_HEAD(, rt_label)		 labels_list;

	u_int32_t			 peerid;
	unsigned int			 ifindex;
	int				 self;
	int				 state;

	u_int16_t			 lspace;
};

struct rt_node;

struct rt_label {
	TAILQ_ENTRY(rt_label)	 node_l, nbr_l;
	struct lde_nbr		*nexthop;
	struct rt_node		*node;
	u_int32_t		 label;
};

struct rt_node {
	RB_ENTRY(rt_node)	entry;
	TAILQ_HEAD(, rt_label)	labels_list;
	struct in_addr		prefix;
	struct in_addr		nexthop;

	u_int32_t		local_label;
	u_int32_t		remote_label;

	u_int32_t		ext_tag;
	u_int16_t		lspace;
	u_int8_t		flags;
	u_int8_t		prefixlen;
	u_int8_t		invalid;
	u_int8_t		present;	/* Is it present in fib? */
};

/* lde.c */
pid_t		 lde(struct ldpd_conf *, int [2], int [2], int [2]);
int		 lde_imsg_compose_ldpe(int, u_int32_t, pid_t, void *,
		     u_int16_t);
u_int32_t	 lde_router_id(void);
void		 lde_send_insert_klabel(struct rt_node *);
void		 lde_send_change_klabel(struct rt_node *);
void		 lde_send_delete_klabel(struct rt_node *);
void		 lde_send_labelmapping(u_int32_t, struct map *);
void		 lde_send_labelrequest(u_int32_t, struct map *);
void		 lde_send_labelrelease(u_int32_t, struct map *);
void		 lde_send_notification(u_int32_t, u_int32_t, u_int32_t,
		    u_int32_t);

void		 lde_nbr_del(struct lde_nbr *);
struct lde_nbr *lde_find_address(struct in_addr);


int			 lde_address_add(struct lde_nbr *, struct in_addr *);
struct lde_nbr_address	*lde_address_find(struct lde_nbr *, struct in_addr *);
int			 lde_address_del(struct lde_nbr *, struct in_addr *);

/* lde_lib.c */
void		 rt_init(void);
int		 rt_compare(struct rt_node *, struct rt_node *);
int		 rt_insert(struct rt_node *);
int		 rt_remove(struct rt_node *);
struct rt_node	*rt_find(in_addr_t, u_int8_t);
void		 rt_clear(void);
void		 route_reset_timers(struct rt_node *);
int		 route_start_timeout(struct rt_node *);
void		 route_start_garbage(struct rt_node *);
void		 rt_dump(pid_t);
void		 rt_snap(u_int32_t);
void		 lde_kernel_insert(struct kroute *);
void		 lde_kernel_remove(struct kroute *);
void		 lde_check_mapping(struct map *, struct lde_nbr *);
void		 lde_check_request(struct map *, struct lde_nbr *);
void		 lde_label_list_free(struct lde_nbr *);

#endif	/* _LDE_H_ */
