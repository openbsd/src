/*	$OpenBSD: lde.h,v 1.25 2015/07/19 18:34:32 renato Exp $ */

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

RB_HEAD(fec_tree, fec);

struct fec {
	RB_ENTRY(fec)		entry;
	struct in_addr		prefix;
	u_int8_t		prefixlen;
};
RB_PROTOTYPE(fec_tree, fec, entry, fec_compare)
extern struct fec_tree rt;

/*
 * fec tree of pending label request
 * Note: currently only one outstanding request per FEC can be tracked but
 *       should not be a problem since ldpd does not support multipath for now.
 */
struct lde_req {
	struct fec		fec;
	u_int32_t		msgid;
};

/* mapping entries */
struct lde_map {
	struct fec		 fec;
	LIST_ENTRY(lde_map)	 entry;
	struct lde_nbr		*nexthop;
	u_int32_t		 label;
};

/* Addresses belonging to neighbor */
struct lde_nbr_address {
	TAILQ_ENTRY(lde_nbr_address)	entry;
	struct in_addr			addr;
};

/* just the info LDE needs */
struct lde_nbr {
	RB_ENTRY(lde_nbr)		 entry;
	struct in_addr			 id;

	struct fec_tree			 recv_req;
	struct fec_tree			 sent_req;
	struct fec_tree			 recv_map;
	struct fec_tree			 sent_map;
	struct fec_tree			 sent_wdraw;
	TAILQ_HEAD(, lde_nbr_address)	 addr_list;

	u_int32_t			 peerid;
};

struct rt_lsp {
	LIST_ENTRY(rt_lsp)	entry;

	struct in_addr		nexthop;
	u_int32_t		remote_label;
};

struct rt_node {
	struct fec		fec;

	LIST_HEAD(, rt_lsp)	lsp;		/* label switching pathes */
	LIST_HEAD(, lde_map)	downstream;	/* recv mappings */
	LIST_HEAD(, lde_map)	upstream;	/* sent mappings */

	u_int32_t		local_label;
	u_int8_t		flags;
};

/* lde.c */
pid_t		lde(struct ldpd_conf *, int [2], int [2], int [2]);
int		lde_imsg_compose_parent(int, pid_t, void *, u_int16_t);
int		lde_imsg_compose_ldpe(int, u_int32_t, pid_t, void *, u_int16_t);
u_int32_t	lde_assign_label(void);

void	lde_send_change_klabel(struct rt_node *, struct rt_lsp *);
void	lde_send_delete_klabel(struct rt_node *, struct rt_lsp *);
void	lde_send_labelmapping(struct lde_nbr *, struct rt_node *);
void	lde_send_labelrequest(struct lde_nbr *, struct rt_node *);
void	lde_send_labelrelease(struct lde_nbr *, struct rt_node *, u_int32_t);
void	lde_send_notification(u_int32_t, u_int32_t, u_int32_t, u_int32_t);

void		lde_nbr_do_mappings(struct rt_node *);
struct lde_map *lde_map_add(struct lde_nbr *, struct rt_node *, int);
void		lde_map_del(struct lde_nbr *, struct lde_map *, int);
struct lde_req *lde_req_add(struct lde_nbr *, struct fec *, int);
void		lde_req_del(struct lde_nbr *, struct lde_req *, int);
struct lde_nbr *lde_find_address(struct in_addr);

int			 lde_address_add(struct lde_nbr *, struct in_addr *);
struct lde_nbr_address	*lde_address_find(struct lde_nbr *, struct in_addr *);
int			 lde_address_del(struct lde_nbr *, struct in_addr *);

/* lde_lib.c */
void		 fec_init(struct fec_tree *);
int		 fec_insert(struct fec_tree *, struct fec *);
int		 fec_remove(struct fec_tree *, struct fec *);
struct fec	*fec_find_prefix(struct fec_tree *, in_addr_t, u_int8_t);
struct fec	*fec_find(struct fec_tree *, struct fec *);
void		 fec_clear(struct fec_tree *, void (*)(void *));

void		 rt_dump(pid_t);
void		 rt_snap(struct lde_nbr *);
void		 rt_clear(void);

void		 lde_kernel_insert(struct kroute *);
void		 lde_kernel_remove(struct kroute *);
void		 lde_check_mapping(struct map *, struct lde_nbr *);
void		 lde_check_request(struct map *, struct lde_nbr *);
void		 lde_check_release(struct map *, struct lde_nbr *);
void		 lde_check_withdraw(struct map *, struct lde_nbr *);
void		 lde_label_list_free(struct lde_nbr *);

#endif	/* _LDE_H_ */
