/*	$OpenBSD: lde_lib.c,v 1.37 2015/07/19 20:54:16 renato Exp $ */

/*
 * Copyright (c) 2009 Michele Marchetto <michele@openbsd.org>
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

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_types.h>
#include <netinet/in.h>
#include <netmpls/mpls.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <event.h>

#include "ldpd.h"
#include "ldp.h"
#include "log.h"
#include "lde.h"

static int fec_compare(struct fec *, struct fec *);

void		 rt_free(void *);
struct rt_node	*rt_add(struct in_addr, u_int8_t);
struct rt_lsp	*rt_lsp_find(struct rt_node *, struct in_addr);
struct rt_lsp	*rt_lsp_add(struct rt_node *, struct in_addr);
void		 rt_lsp_del(struct rt_lsp *);
int		 lde_nbr_is_nexthop(struct rt_node *, struct lde_nbr *);

RB_GENERATE(fec_tree, fec, entry, fec_compare)

extern struct nbr_tree	lde_nbrs;
RB_PROTOTYPE(nbr_tree, lde_nbr, entry, lde_nbr_compare)

extern struct ldpd_conf		*ldeconf;

struct fec_tree	rt = RB_INITIALIZER(&rt);

/* FEC tree functions */
void
fec_init(struct fec_tree *fh)
{
	RB_INIT(fh);
}

static int
fec_compare(struct fec *a, struct fec *b)
{
	if (ntohl(a->prefix.s_addr) < ntohl(b->prefix.s_addr))
		return (-1);
	if (ntohl(a->prefix.s_addr) > ntohl(b->prefix.s_addr))
		return (1);
	if (a->prefixlen < b->prefixlen)
		return (-1);
	if (a->prefixlen > b->prefixlen)
		return (1);

	return (0);
}

struct fec *
fec_find_prefix(struct fec_tree *fh, in_addr_t prefix, u_int8_t prefixlen)
{
	struct fec	 s;

	s.prefix.s_addr = prefix;
	s.prefixlen = prefixlen;

	return (fec_find(fh, &s));
}

struct fec *
fec_find(struct fec_tree *fh, struct fec *f)
{
	return (RB_FIND(fec_tree, fh, f));
}

int
fec_insert(struct fec_tree *fh, struct fec *f)
{
	if (RB_INSERT(fec_tree, fh, f) != NULL)
		return (-1);
	return (0);
}

int
fec_remove(struct fec_tree *fh, struct fec *f)
{
	if (RB_REMOVE(fec_tree, fh, f) == NULL) {
		log_warnx("fec_remove failed for %s/%u",
		    inet_ntoa(f->prefix), f->prefixlen);
		return (-1);
	}
	return (0);
}

void
fec_clear(struct fec_tree *fh, void (*free_cb)(void *))
{
	struct fec	*f;

	while ((f = RB_ROOT(fh)) != NULL) {
		fec_remove(fh, f);
		free_cb(f);
	}
}

/* routing table functions */
int
lde_nbr_is_nexthop(struct rt_node *rn, struct lde_nbr *ln)
{
	struct rt_lsp		*rl;

	LIST_FOREACH(rl, &rn->lsp, entry)
		if (lde_address_find(ln, &rl->nexthop))
			return (1);

	return (0);
}

void
rt_dump(pid_t pid)
{
	struct fec		*f;
	struct rt_node		*rr;
	struct lde_map		*me;
	static struct ctl_rt	 rtctl;

	RB_FOREACH(f, fec_tree, &rt) {
		rr = (struct rt_node *)f;
		if (rr->local_label == NO_LABEL &&
		    LIST_EMPTY(&rr->downstream))
			continue;

		rtctl.prefix = rr->fec.prefix;
		rtctl.prefixlen = rr->fec.prefixlen;
		rtctl.flags = rr->flags;
		rtctl.local_label = rr->local_label;

		LIST_FOREACH(me, &rr->downstream, entry) {
			rtctl.in_use = lde_nbr_is_nexthop(rr, me->nexthop);
			rtctl.nexthop = me->nexthop->id;
			rtctl.remote_label = me->label;

			lde_imsg_compose_ldpe(IMSG_CTL_SHOW_LIB, 0, pid,
			    &rtctl, sizeof(rtctl));
		}
		if (LIST_EMPTY(&rr->downstream)) {
			rtctl.in_use = 0;
			rtctl.nexthop.s_addr = INADDR_ANY;
			rtctl.remote_label = NO_LABEL;

			lde_imsg_compose_ldpe(IMSG_CTL_SHOW_LIB, 0, pid,
			    &rtctl, sizeof(rtctl));
		}
	}
}

void
rt_snap(struct lde_nbr *ln)
{
	struct fec	*f;
	struct rt_node	*rn;
	int		 count = 0;

	RB_FOREACH(f, fec_tree, &rt) {
		rn = (struct rt_node *)f;
		if (rn->local_label == NO_LABEL)
			continue;

		lde_send_labelmapping(ln, rn);
		count++;
	}
	if (count > 0)
		lde_imsg_compose_ldpe(IMSG_MAPPING_ADD_END,
		    ln->peerid, 0, NULL, 0);
}

void
rt_free(void *arg)
{
	struct rt_node	*rr = arg;
	struct rt_lsp	*rl;

	while ((rl = LIST_FIRST(&rr->lsp)))
		rt_lsp_del(rl);
	if (!LIST_EMPTY(&rr->downstream))
		log_warnx("rt_free: fec %s/%u downstream list not empty",
		    inet_ntoa(rr->fec.prefix), rr->fec.prefixlen);
	if (!LIST_EMPTY(&rr->upstream))
		log_warnx("rt_free: fec %s/%u upstream list not empty",
		    inet_ntoa(rr->fec.prefix), rr->fec.prefixlen);

	free(rr);
}

void
rt_clear(void)
{
	fec_clear(&rt, rt_free);
}

struct rt_node *
rt_add(struct in_addr prefix, u_int8_t prefixlen)
{
	struct rt_node	*rn;

	rn = calloc(1, sizeof(*rn));
	if (rn == NULL)
		fatal("rt_add");

	rn->fec.prefix.s_addr = prefix.s_addr;
	rn->fec.prefixlen = prefixlen;
	rn->local_label = NO_LABEL;
	LIST_INIT(&rn->upstream);
	LIST_INIT(&rn->downstream);
	LIST_INIT(&rn->lsp);

	if (fec_insert(&rt, &rn->fec))
		log_warnx("failed to add %s/%u to rt tree",
		    inet_ntoa(rn->fec.prefix), rn->fec.prefixlen);

	return (rn);
}

struct rt_lsp *
rt_lsp_find(struct rt_node *rn, struct in_addr nexthop)
{
	struct rt_lsp	*rl;

	LIST_FOREACH(rl, &rn->lsp, entry)
		if (rl->nexthop.s_addr == nexthop.s_addr)
			return (rl);
	return (NULL);
}

struct rt_lsp *
rt_lsp_add(struct rt_node *rn, struct in_addr nexthop)
{
	struct rt_lsp	*rl;

	rl = calloc(1, sizeof(*rl));
	if (rl == NULL)
		fatal("rt_lsp_add");

	rl->nexthop.s_addr = nexthop.s_addr;
	rl->remote_label = NO_LABEL;
	LIST_INSERT_HEAD(&rn->lsp, rl, entry);

	return (rl);
}

void
rt_lsp_del(struct rt_lsp *rl)
{
	LIST_REMOVE(rl, entry);
	free(rl);
}

void
lde_kernel_insert(struct kroute *kr)
{
	struct rt_node		*rn;
	struct rt_lsp		*rl;
	struct lde_map		*me;
	struct lde_nbr		*ln;
	char			 buf[16];

	log_debug("kernel add route %s/%u nexthop %s",
	    inet_ntoa(kr->prefix), kr->prefixlen,
	    inet_ntop(AF_INET, &kr->nexthop, buf, sizeof(buf)));

	rn = (struct rt_node *)fec_find_prefix(&rt, kr->prefix.s_addr,
	    kr->prefixlen);
	if (rn == NULL)
		rn = rt_add(kr->prefix, kr->prefixlen);

	if (rt_lsp_find(rn, kr->nexthop) != NULL)
		return;

	if (LIST_EMPTY(&rn->lsp)) {
		if (rn->local_label == NO_LABEL) {
			if (kr->flags & F_CONNECTED)
				rn->local_label = MPLS_LABEL_IMPLNULL;
			else
				rn->local_label = lde_assign_label();
		} else {
			/* Handle local label changes */
			if ((kr->flags & F_CONNECTED) &&
			    rn->local_label != MPLS_LABEL_IMPLNULL) {
				/* explicit withdraw of the previous label */
				RB_FOREACH(ln, nbr_tree, &lde_nbrs)
					lde_send_labelwithdraw(ln, rn);
				rn->local_label = MPLS_LABEL_IMPLNULL;
			}

			if (!(kr->flags & F_CONNECTED) &&
			    rn->local_label == MPLS_LABEL_IMPLNULL) {
				/* explicit withdraw of the previous label */
				RB_FOREACH(ln, nbr_tree, &lde_nbrs)
					lde_send_labelwithdraw(ln, rn);
				rn->local_label = lde_assign_label();
			}
		}

		/* FEC.1: perform lsr label distribution procedure */
		RB_FOREACH(ln, nbr_tree, &lde_nbrs) {
			lde_send_labelmapping(ln, rn);
			lde_imsg_compose_ldpe(IMSG_MAPPING_ADD_END,
			    ln->peerid, 0, NULL, 0);
		}
	}

	rl = rt_lsp_add(rn, kr->nexthop);
	lde_send_change_klabel(rn, rl);

	ln = lde_find_address(rl->nexthop);
	if (ln) {
		/* FEC.2  */
		me = (struct lde_map *)fec_find(&ln->recv_map, &rn->fec);
		if (me) {
			struct map	map;

			bzero(&map, sizeof(map));
			map.prefix.s_addr = me->fec.prefix.s_addr;
			map.prefixlen = me->fec.prefixlen;
			map.label = me->label;

			/* FEC.5 */
			lde_check_mapping(&map, ln);
		}
	}
}

void
lde_kernel_remove(struct kroute *kr)
{
	struct rt_node		*rn;
	struct rt_lsp		*rl;
	struct lde_nbr		*ln;
	char			 buf[16];

	log_debug("kernel remove route %s/%u nexthop %s",
	    inet_ntoa(kr->prefix), kr->prefixlen,
	    inet_ntop(AF_INET, &kr->nexthop, buf, sizeof(buf)));

	rn = (struct rt_node *)fec_find_prefix(&rt, kr->prefix.s_addr,
	    kr->prefixlen);
	if (rn == NULL)
		/* route lost */
		return;

	rl = rt_lsp_find(rn, kr->nexthop);
	if (rl == NULL)
		/* route lost */
		return;

	rt_lsp_del(rl);
	if (LIST_EMPTY(&rn->lsp))
		RB_FOREACH(ln, nbr_tree, &lde_nbrs)
			lde_send_labelwithdraw(ln, rn);
}

void
lde_check_mapping(struct map *map, struct lde_nbr *ln)
{
	struct rt_node		*rn;
	struct rt_lsp		*rl;
	struct lde_req		*lre;
	struct lde_nbr_address	*addr;
	struct lde_map		*me;
	int			 msgsource = 0;

	rn = (struct rt_node *)fec_find_prefix(&rt, map->prefix.s_addr,
	    map->prefixlen);
	if (rn == NULL)
		rn = rt_add(map->prefix, map->prefixlen);

	/* LMp.1: first check if we have a pending request running */
	lre = (struct lde_req *)fec_find(&ln->sent_req, &rn->fec);
	if (lre)
		/* LMp.2: delete record of outstanding label request */
		lde_req_del(ln, lre, 1);

	/*
	 * LMp.3 - LMp.8: Loop detection LMp.3 - unecessary for frame-mode
	 * mpls networks
	 */

	/* LMp.9 */
	me = (struct lde_map *)fec_find(&ln->recv_map, &rn->fec);
	if (me) {
		/* LMp.10 */
		if (me->label != map->label && lre == NULL) {
			/* LMp.10a */
			lde_send_labelrelease(ln, rn, me->label);

			LIST_FOREACH(rl, &rn->lsp, entry)
				TAILQ_FOREACH(addr, &ln->addr_list, entry)
					if (rl->nexthop.s_addr ==
					    addr->addr.s_addr) {
						lde_send_delete_klabel(rn, rl);
						rl->remote_label = NO_LABEL;
					}
		}
	}

	/*
	 * LMp.11 - 12: consider multiple nexthops in order to
	 * support multipath
	 */
	LIST_FOREACH(rl, &rn->lsp, entry) {
		if (lde_address_find(ln, &rl->nexthop)) {
			msgsource = 1;

			/* LMp.15: install FEC in FIB */
			rl->remote_label = map->label;
			lde_send_change_klabel(rn, rl);
		}
	}
	if (msgsource == 0) {
		/* LMp.13: perform lsr label release procedure */
		if (me == NULL)
			me = lde_map_add(ln, rn, 0);
		me->label = map->label;
		return;
	}

	/* LMp.16: Record the mapping from this peer */
	if (me == NULL)
		me = lde_map_add(ln, rn, 0);
	me->label = map->label;


	/*
	 * LMp.17 - LMp.27 are unnecessary since we don't need to implement
	 * loop detection. LMp.28 - LMp.30 are unnecessary because we are
	 * merging capable.
	 */
}

void
lde_check_request(struct map *map, struct lde_nbr *ln)
{
	struct lde_req	*lre;
	struct rt_node	*rn;
	struct rt_lsp	*rl;

	/* TODO LRq.1: loop detection */

	/* LRq.2: is there a next hop for fec? */
	rn = (struct rt_node *)fec_find_prefix(&rt, map->prefix.s_addr,
	    map->prefixlen);
	if (rn == NULL || LIST_EMPTY(&rn->lsp)) {
		lde_send_notification(ln->peerid, S_NO_ROUTE, map->messageid,
		    MSG_TYPE_LABELREQUEST);
		return;
	}

	/* LRq.3: is MsgSource the next hop? */
	LIST_FOREACH(rl, &rn->lsp, entry) {
		if (lde_address_find(ln, &rl->nexthop)) {
			lde_send_notification(ln->peerid, S_LOOP_DETECTED,
			    map->messageid, MSG_TYPE_LABELREQUEST);
			return;
		}
	}

	/* LRq.6: first check if we have a pending request running */
	lre = (struct lde_req *)fec_find(&ln->recv_req, &rn->fec);
	if (lre != NULL)
		/* LRq.7: duplicate request */
		return;

	/* LRq.8: record label request */
	lre = lde_req_add(ln, &rn->fec, 0);
	if (lre != NULL)
		lre->msgid = map->messageid;

	/* LRq.9: perform LSR label distribution */
	lde_send_labelmapping(ln, rn);
	lde_imsg_compose_ldpe(IMSG_MAPPING_ADD_END, ln->peerid, 0, NULL, 0);

	/*
	 * LRq.10: do nothing (Request Never) since we use liberal
	 * label retention.
	 * LRq.11 - 12 are unnecessary since we are merging capable.
	 */
}

void
lde_check_release(struct map *map, struct lde_nbr *ln)
{
	struct rt_node		*rn;
	struct lde_wdraw	*lw;
	struct lde_map		*me;

	rn = (struct rt_node *)fec_find_prefix(&rt, map->prefix.s_addr,
	    map->prefixlen);
	/* LRl.1: does FEC match a known FEC? */
	if (rn == NULL)
		return;

	/* LRl.3: first check if we have a pending withdraw running */
	lw = (struct lde_wdraw *)fec_find(&ln->sent_wdraw, &rn->fec);
	if (lw && (map->label == NO_LABEL ||
	    (lw->label != NO_LABEL && map->label == lw->label))) {
		/* LRl.4: delete record of outstanding label withdraw */
		lde_wdraw_del(ln, lw);
	}

	/* LRl.6: check sent map list and remove it if available */
	me = (struct lde_map *)fec_find(&ln->sent_map, &rn->fec);
	if (me && (map->label == NO_LABEL || map->label == me->label))
		lde_map_del(ln, me, 1);

	/*
	 * LRl.11 - 13 are unnecessary since we remove the label from
	 * forwarding/switching as soon as the FEC is unreachable.
	 */
}

void
lde_check_release_wcard(struct map *map, struct lde_nbr *ln)
{
	struct fec		*f;
	struct rt_node		*rn;
	struct lde_wdraw	*lw;
	struct lde_map		*me;

	RB_FOREACH(f, fec_tree, &rt) {
		rn = (struct rt_node *)f;

		/* LRl.3: first check if we have a pending withdraw running */
		lw = (struct lde_wdraw *)fec_find(&ln->sent_wdraw, &rn->fec);
		if (lw && (map->label == NO_LABEL ||
		    (lw->label != NO_LABEL && map->label == lw->label))) {
			/* LRl.4: delete record of outstanding lbl withdraw */
			lde_wdraw_del(ln, lw);
		}

		/* LRl.6: check sent map list and remove it if available */
		me = (struct lde_map *)fec_find(&ln->sent_map, &rn->fec);
		if (me &&
		    (map->label == NO_LABEL || map->label == me->label)) {
			lde_map_del(ln, me, 1);
		}

		/*
		 * LRl.11 - 13 are unnecessary since we remove the label from
		 * forwarding/switching as soon as the FEC is unreachable.
		 */
	}
}

void
lde_check_withdraw(struct map *map, struct lde_nbr *ln)
{
	struct rt_node	*rn;
	struct rt_lsp	*rl;
	struct lde_map	*me;

	rn = (struct rt_node *)fec_find_prefix(&rt, map->prefix.s_addr,
	    map->prefixlen);
	if (rn == NULL)
		rn = rt_add(map->prefix, map->prefixlen);

	/* LWd.1: remove label from forwarding/switching use */
	LIST_FOREACH(rl, &rn->lsp, entry) {
		if (lde_address_find(ln, &rl->nexthop)) {
			lde_send_delete_klabel(rn, rl);
			rl->remote_label = NO_LABEL;
		}
	}

	/* LWd.2: send label release */
	lde_send_labelrelease(ln, rn, map->label);

	/* LWd.3: check previously received label mapping */
	me = (struct lde_map *)fec_find(&ln->recv_map, &rn->fec);
	if (me && (map->label == NO_LABEL || map->label == me->label))
		/* LWd.4: remove record of previously received lbl mapping */
		lde_map_del(ln, me, 0);
}

void
lde_check_withdraw_wcard(struct map *map, struct lde_nbr *ln)
{
	struct fec	*f;
	struct rt_node	*rn;
	struct rt_lsp	*rl;
	struct lde_map	*me;

	/* LWd.2: send label release */
	lde_send_labelrelease(ln, NULL, map->label);

	RB_FOREACH(f, fec_tree, &rt) {
		rn = (struct rt_node *)f;

		/* LWd.1: remove label from forwarding/switching use */
		LIST_FOREACH(rl, &rn->lsp, entry) {
			if (lde_address_find(ln, &rl->nexthop)) {
				lde_send_delete_klabel(rn, rl);
				rl->remote_label = NO_LABEL;
			}
		}

		/* LWd.3: check previously received label mapping */
		me = (struct lde_map *)fec_find(&ln->recv_map, &rn->fec);
		if (me && (map->label == NO_LABEL || map->label == me->label))
			/*
			 * LWd.4: remove record of previously received
			 * label mapping
			 */
			lde_map_del(ln, me, 0);
	}
}
