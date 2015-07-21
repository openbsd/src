/*	$OpenBSD: lde.h,v 1.29 2015/07/21 04:52:29 renato Exp $ */

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

enum fec_type {
	FEC_TYPE_IPV4,
	FEC_TYPE_PWID
};

struct fec {
	RB_ENTRY(fec)		entry;
	enum fec_type		type;
	union {
		struct {
			struct in_addr	prefix;
			u_int8_t	prefixlen;
		} ipv4;
		struct {
			u_int16_t	type;
			u_int32_t	pwid;
			struct in_addr	nexthop;
		} pwid;
	} u;
};
RB_PROTOTYPE(fec_tree, fec, entry, fec_compare)
extern struct fec_tree ft;

/* request entries */
struct lde_req {
	struct fec		fec;
	u_int32_t		msgid;
};

/* mapping entries */
struct lde_map {
	struct fec		 fec;
	LIST_ENTRY(lde_map)	 entry;
	struct lde_nbr		*nexthop;
	struct map		 map;
};

/* withdraw entries */
struct lde_wdraw {
	struct fec		 fec;
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

struct fec_nh {
	LIST_ENTRY(fec_nh)	entry;

	struct in_addr		nexthop;
	u_int32_t		remote_label;
	void			*data;		/* fec specific data */
};

struct fec_node {
	struct fec		fec;

	LIST_HEAD(, fec_nh)	nexthops;	/* fib nexthops */
	LIST_HEAD(, lde_map)	downstream;	/* recv mappings */
	LIST_HEAD(, lde_map)	upstream;	/* sent mappings */

	u_int32_t		local_label;
};

/* lde.c */
pid_t		lde(struct ldpd_conf *, int [2], int [2], int [2]);
int		lde_imsg_compose_parent(int, pid_t, void *, u_int16_t);
int		lde_imsg_compose_ldpe(int, u_int32_t, pid_t, void *, u_int16_t);
u_int32_t	lde_assign_label(void);
void		lde_fec2map(struct fec *, struct map *);
void		lde_map2fec(struct map *, struct in_addr, struct fec *);

void	lde_send_change_klabel(struct fec_node *, struct fec_nh *);
void	lde_send_delete_klabel(struct fec_node *, struct fec_nh *);
void	lde_send_labelmapping(struct lde_nbr *, struct fec_node *, int);
void	lde_send_labelwithdraw(struct lde_nbr *, struct fec_node *);
void	lde_send_labelrelease(struct lde_nbr *, struct fec_node *, u_int32_t);
void	lde_send_notification(u_int32_t, u_int32_t, u_int32_t, u_int32_t);

struct lde_map *lde_map_add(struct lde_nbr *, struct fec_node *, int);
void		lde_map_del(struct lde_nbr *, struct lde_map *, int);
struct lde_req *lde_req_add(struct lde_nbr *, struct fec *, int);
void		lde_req_del(struct lde_nbr *, struct lde_req *, int);
struct lde_wdraw *lde_wdraw_add(struct lde_nbr *, struct fec_node *);
void		  lde_wdraw_del(struct lde_nbr *, struct lde_wdraw *);
struct lde_nbr *lde_find_address(struct in_addr);

int			 lde_address_add(struct lde_nbr *, struct in_addr *);
struct lde_nbr_address	*lde_address_find(struct lde_nbr *, struct in_addr *);
int			 lde_address_del(struct lde_nbr *, struct in_addr *);

/* lde_lib.c */
void		 fec_init(struct fec_tree *);
int		 fec_insert(struct fec_tree *, struct fec *);
int		 fec_remove(struct fec_tree *, struct fec *);
struct fec	*fec_find(struct fec_tree *, struct fec *);
void		 fec_clear(struct fec_tree *, void (*)(void *));

void		 rt_dump(pid_t);
void		 fec_snap(struct lde_nbr *);
void		 fec_tree_clear(void);

struct fec_nh	*fec_nh_find(struct fec_node *, struct in_addr);
void		 lde_kernel_insert(struct fec *, struct in_addr, int, void *);
void		 lde_kernel_remove(struct fec *, struct in_addr);
void		 lde_check_mapping(struct map *, struct lde_nbr *);
void		 lde_check_request(struct map *, struct lde_nbr *);
void		 lde_check_release(struct map *, struct lde_nbr *);
void		 lde_check_release_wcard(struct map *, struct lde_nbr *);
void		 lde_check_withdraw(struct map *, struct lde_nbr *);
void		 lde_check_withdraw_wcard(struct map *, struct lde_nbr *);
void		 lde_label_list_free(struct lde_nbr *);

/* l2vpn.c */
struct l2vpn	*l2vpn_new(const char *);
struct l2vpn	*l2vpn_find(struct ldpd_conf *, char *);
void		 l2vpn_del(struct l2vpn *);
void		 l2vpn_init(struct l2vpn *);
struct l2vpn_if	*l2vpn_if_new(struct l2vpn *, struct kif *);
struct l2vpn_if	*l2vpn_if_find(struct l2vpn *, unsigned int);
void		 l2vpn_if_del(struct l2vpn_if *l);
struct l2vpn_pw	*l2vpn_pw_new(struct l2vpn *, struct kif *);
struct l2vpn_pw *l2vpn_pw_find(struct l2vpn *, unsigned int);
void		 l2vpn_pw_del(struct l2vpn_pw *);
void		 l2vpn_pw_init(struct l2vpn_pw *);
void		 l2vpn_pw_fec(struct l2vpn_pw *, struct fec *);
void		 l2vpn_pw_reset(struct l2vpn_pw *);
int		 l2vpn_pw_ok(struct l2vpn_pw *, struct fec_nh *);
int		 l2vpn_pw_negotiate(struct lde_nbr *, struct fec_node *,
    struct map *);
void		 l2vpn_send_pw_status(u_int32_t, u_int32_t, struct fec *);
void		 l2vpn_recv_pw_status(struct lde_nbr *, struct notify_msg *);
void		 l2vpn_sync_pws(struct in_addr);
void		 l2vpn_pw_ctl(pid_t);
void		 l2vpn_binding_ctl(pid_t);

#endif	/* _LDE_H_ */
