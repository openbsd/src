/*	$OpenBSD: rde.h,v 1.1 2006/06/01 14:12:20 norby Exp $ */

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

struct rt_node {
	RB_ENTRY(rt_node)	 entry;
	struct event		 expiration_timer;
	struct in_addr		 prefix;
	struct in_addr		 nexthop;
	struct in_addr		 adv_rtr;
	u_int32_t		 cost;
	u_short			 ifindex;	/* learned from this iface */
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

/* rde.c */
pid_t	rde(struct dvmrpd_conf *, int [2], int [2], int [2]);
int	rde_imsg_compose_parent(int, pid_t, void *, u_int16_t);
int	rde_imsg_compose_dvmrpe(int, u_int32_t, pid_t, void *, u_int16_t);

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

/* rde_srt.c */
void		 rt_init(void);
int		 rt_compare(struct rt_node *, struct rt_node *);
struct rt_node	*rt_find(in_addr_t, u_int8_t);
int		 rt_insert(struct rt_node *);
int		 rt_remove(struct rt_node *);
void		 rt_clear(void);
void		 rt_snap(u_int32_t);
void		 rt_dump(pid_t);
void		 rt_update(struct in_addr, u_int8_t, struct in_addr,
		     u_int32_t, struct in_addr, u_short, u_int8_t, u_int8_t);

#endif	/* _RDE_H_ */
