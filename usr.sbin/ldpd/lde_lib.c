/*	$OpenBSD: lde_lib.c,v 1.32 2015/04/04 15:15:44 renato Exp $ */

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
struct rt_lsp	*rt_lsp_find(struct rt_node *, struct in_addr, u_int8_t);
struct rt_lsp	*rt_lsp_add(struct rt_node *, struct in_addr, u_int8_t);
void		 rt_lsp_del(struct rt_lsp *);
int		 lde_nbr_is_nexthop(struct rt_node *, struct lde_nbr *);

RB_PROTOTYPE(fec_tree, fec, entry, fec_compare)
RB_GENERATE(fec_tree, fec, entry, fec_compare)

extern struct ldpd_conf		*ldeconf;

struct fec_tree	rt = RB_INITIALIZER(&rt);

/* FEC tree fucntions */
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
	struct rt_node	*r;
	struct lde_map	*me;
	struct map	 map;

	bzero(&map, sizeof(map));
	RB_FOREACH(f, fec_tree, &rt) {
		r = (struct rt_node *)f;
		map.prefix = r->fec.prefix;
		map.prefixlen = r->fec.prefixlen;
		map.label = r->local_label;

		me = lde_map_add(ln, r, 1);
		me->label = r->local_label;

		lde_imsg_compose_ldpe(IMSG_MAPPING_ADD, ln->peerid, 0, &map,
		    sizeof(map));
	}
}

void
rt_free(void *arg)
{
	struct rt_node	*rr = arg;
	struct rt_lsp	*rl;

	while ((rl = LIST_FIRST(&rr->lsp))) {
		LIST_REMOVE(rl, entry);
		free(rl);
	}

	if (!LIST_EMPTY(&rr->downstream))
		log_warnx("rt_free: fec %s/%u downstream list not empty",
		    inet_ntoa(rr->fec.prefix), rr->fec.prefixlen);
	if (!LIST_EMPTY(&rr->upstream))
		log_warnx("rt_free: fec %s/%u upstream list not empty",
		    inet_ntoa(rr->fec.prefix), rr->fec.prefixlen);

	free(rl);
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
rt_lsp_find(struct rt_node *rn, struct in_addr nexthop, u_int8_t prio)
{
	struct rt_lsp	*rl;

	LIST_FOREACH(rl, &rn->lsp, entry)
		if (rl->nexthop.s_addr == nexthop.s_addr &&
		    rl->priority == prio)
			return (rl);
	return (NULL);
}

struct rt_lsp *
rt_lsp_add(struct rt_node *rn, struct in_addr nexthop, u_int8_t prio)
{
	struct rt_lsp	*rl, *nrl;

	rl = calloc(1, sizeof(*rl));
	if (rl == NULL)
		fatal("rt_lsp_add");

	rl->nexthop.s_addr = nexthop.s_addr;
	rl->remote_label = NO_LABEL;
	rl->priority = prio;

	/* keep LSP list sorted by priority because only the best routes
	 * can be used in a LSP. */
	if (LIST_EMPTY(&rn->lsp))
		LIST_INSERT_HEAD(&rn->lsp, rl, entry);
	else {
		LIST_FOREACH(nrl, &rn->lsp, entry) {
			if (prio < nrl->priority) {
				LIST_INSERT_BEFORE(nrl, rl, entry);
				break;
			}
			if (LIST_NEXT(nrl, entry) == NULL) {
				LIST_INSERT_AFTER(nrl, rl, entry);
				break;
			}
		}
	}
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
	struct lde_nbr_address	*addr;
	struct lde_map		*map;

	log_debug("kernel add route %s/%u", inet_ntoa(kr->prefix),
	    kr->prefixlen);

	rn = (struct rt_node *)fec_find_prefix(&rt, kr->prefix.s_addr,
	    kr->prefixlen);
	if (rn == NULL)
		rn = rt_add(kr->prefix, kr->prefixlen);

	rl = rt_lsp_find(rn, kr->nexthop, kr->priority);
	if (rl == NULL)
		rl = rt_lsp_add(rn, kr->nexthop, kr->priority);

	/* There is static assigned label for this route, record it in lib */
	if (kr->local_label != NO_LABEL) {
		rn->local_label = kr->local_label;
		return;
	}

	if (rn->local_label == NO_LABEL) {
		if (kr->flags & F_CONNECTED)
			/* Directly connected route */
			rn->local_label = MPLS_LABEL_IMPLNULL;
		else
			rn->local_label = lde_assign_label();
	}

	LIST_FOREACH(map, &rn->downstream, entry) {
		addr = lde_address_find(map->nexthop, &rl->nexthop);
		if (addr != NULL) {
			rl->remote_label = map->label;
			break;
		}
	}

	lde_send_change_klabel(rn, rl);

	/* Redistribute the current mapping to every nbr */
	lde_nbr_do_mappings(rn);
}

void
lde_kernel_remove(struct kroute *kr)
{
	struct rt_node		*rn;
	struct rt_lsp		*rl;

	log_debug("kernel remove route %s/%u", inet_ntoa(kr->prefix),
	    kr->prefixlen);

	rn = (struct rt_node *)fec_find_prefix(&rt, kr->prefix.s_addr,
	    kr->prefixlen);
	if (rn == NULL)
		/* route lost */
		return;

	rl = rt_lsp_find(rn, kr->nexthop, kr->priority);
	if (rl != NULL)
		rt_lsp_del(rl);

	/* XXX handling of total loss of route, withdraw mappings, etc */

	/* Redistribute the current mapping to every nbr */
	lde_nbr_do_mappings(rn);
}

void
lde_check_mapping(struct map *map, struct lde_nbr *ln)
{
	struct rt_node		*rn;
	struct rt_lsp		*rl;
	struct lde_req		*lre;
	struct lde_nbr_address	*addr = NULL;
	struct lde_map		*me;

	log_debug("label mapping from nbr %s, FEC %s, label %u",
	    inet_ntoa(ln->id), log_fec(map), map->label);

	rn = (struct rt_node *)fec_find_prefix(&rt, map->prefix.s_addr,
	    map->prefixlen);
	if (rn == NULL) {
		/* The route is not yet in fib. If we are in liberal mode
		 *  create a route and record the label */
		if (ldeconf->mode & MODE_RET_CONSERVATIVE)
			return;

		rn = rt_add(map->prefix, map->prefixlen);
		rn->local_label = lde_assign_label();
	}

	/* first check if we have a pending request running */
	lre = (struct lde_req *)fec_find(&ln->sent_req, &rn->fec);
	if (lre)
		lde_req_del(ln, lre, 1);

	/* TODO Loop detection LMp.3 - LMp.8 */

	LIST_FOREACH(me, &rn->downstream, entry) {
		if (ln != me->nexthop)				/* LMp.9 */
			continue;
		if (lre)
			/* LMp.10 Note 6: req. mappings are always new */
			break;
		if (me->label != map->label) {			/* LMp.10 */
			/*
			 * This is, according to the RFC, a try to install a
			 * multipath LSP which is not supported by the RFC.
			 * So instead release the old label and install the
			 * new one.
			 */
			log_debug("possible multipath FEC %s, "
			    "label %u, old label %u",
			    log_fec(map), map->label, me->label);
			lde_send_labelrelease(ln, rn, me->label);
		}
		/* there can only be one mapping */
		break;
	}

	/* LMp.11: get nexthop */
	LIST_FOREACH(rl, &rn->lsp, entry) {
		addr = lde_address_find(ln, &rl->nexthop);
		if (addr)
			break;
	}
	if (addr == NULL) {
		/* route not yet available LMp.13 */
		if (ldeconf->mode & MODE_RET_CONSERVATIVE) {
			log_debug("FEC %s: conservative ret but no route",
			    log_fec(map));
			lde_send_labelrelease(ln, rn, map->label);
			return;
		}
		/* in liberal mode just note the mapping */
		if (me == NULL)
			me = lde_map_add(ln, rn, 0);
		me->label = map->label;

		return;
	}

	/* LMp.14 do we actually need this FEC for now this is always true */
	rl->remote_label = map->label;

	/* LMp.15 install FEC in FIB */
	lde_send_change_klabel(rn, rl);

	/* Record the mapping from this peer LMp.16 */
	if (me == NULL)
		me = lde_map_add(ln, rn, 0);
	me->label = map->label;

	/* Redistribute the current mapping to every nbr LMp.17-31 */
	lde_nbr_do_mappings(rn);
}

void
lde_check_request(struct map *map, struct lde_nbr *ln)
{
	struct lde_req	*lre;
	struct rt_node	*rn;
	struct rt_lsp	*rl;
	struct lde_nbr	*lnn;
	u_int8_t	 prio = 0;

	log_debug("label request from nbr %s, FEC %s",
	    inet_ntoa(ln->id), log_fec(map));

	rn = (struct rt_node *)fec_find_prefix(&rt, map->prefix.s_addr,
	    map->prefixlen);
	if (rn == NULL) {
		lde_send_notification(ln->peerid, S_NO_ROUTE, map->messageid,
		    MSG_TYPE_LABELREQUEST);
		return;
	}

	LIST_FOREACH(rl, &rn->lsp, entry) {
		/* only consider pathes with highest priority */
		if (prio == 0)
			prio = rl->priority;
		if (prio < rl->priority)
			break;
		if (lde_address_find(ln, &rl->nexthop)) {
			lde_send_notification(ln->peerid, S_LOOP_DETECTED,
			    map->messageid, MSG_TYPE_LABELREQUEST);
			return;
		}

		if (rl->remote_label != NO_LABEL)
			break;
	}

	/* first check if we have a pending request running */
	lre = (struct lde_req *)fec_find(&ln->recv_req, &rn->fec);
	if (lre != NULL)
		return;
	/* else record label request */
	lre = lde_req_add(ln, &rn->fec, 0);
	if (lre != NULL)
		lre->msgid = map->messageid;

	/* there is a valid mapping available */
	if (rl != NULL) {
		/* TODO loop protection handling (LRq.9) */
		lde_send_labelmapping(ln, rn);
		return;
	}

	/* no mapping available, try to request */
	/* XXX depending on the request behaviour we could return here */
	LIST_FOREACH(rl, &rn->lsp, entry) {
		/* only consider pathes with highest priority */
		if (prio == 0)
			prio = rl->priority;
		if (prio < rl->priority)
			break;
		lnn = lde_find_address(rl->nexthop);
		if (lnn == NULL)
			continue;
		lde_send_labelrequest(lnn, rn);
	}
}

void
lde_check_release(struct map *map, struct lde_nbr *ln)
{
	struct rt_node	*rn;
	struct lde_req	*lre;
	struct lde_map	*me;

	log_debug("label release from nbr %s, FEC %s",
	    inet_ntoa(ln->id), log_fec(map));

	rn = (struct rt_node *)fec_find_prefix(&rt, map->prefix.s_addr,
	    map->prefixlen);
	if (rn == NULL)
		return;

	/* first check if we have a pending withdraw running */
	lre = (struct lde_req *)fec_find(&ln->sent_wdraw, &rn->fec);
	if (lre) {
		fec_remove(&ln->sent_wdraw, &lre->fec);
		free(lre);
	}

	/* check sent map list and remove it if available */
	me = (struct lde_map *)fec_find(&ln->sent_map, &rn->fec);
	if (me)
		lde_map_del(ln, me, 1);

	/* remove FEC if not in use anymore */
	/* XXX what about outstanding label requests? */
	if (!LIST_EMPTY(&rn->upstream))
		return;

	/* XXX if originated here free all resources */
	/* else decide if a label release should be forwarded. */
	/* Since we do liberal retention we can keep the path mapped. */
}

void
lde_check_withdraw(struct map *map, struct lde_nbr *ln)
{
	struct rt_node	*rn;
	struct rt_lsp	*rl;
	struct lde_map	*me;

	log_debug("label withdraw from nbr %s, FEC %s",
	    inet_ntoa(ln->id), log_fec(map));

	rn = (struct rt_node *)fec_find_prefix(&rt, map->prefix.s_addr,
	    map->prefixlen);

	lde_send_labelrelease(ln, rn, map->label);

	if (rn == NULL)
		/* LSP not available, nothing to do */
		return;

	/* remove LSP from kernel */
	LIST_FOREACH(rl, &rn->lsp, entry) {
		if (lde_address_find(ln, &rl->nexthop))
			break;
	}
	if (rl) {
		rl->remote_label = NO_LABEL;
		lde_send_delete_klabel(rn, rl);
	}

	/* check recv map list and remove it if available */
	me = (struct lde_map *)fec_find(&ln->recv_map, &rn->fec);
	if (me)
		lde_map_del(ln, me, 0);

	/* if ordered distribution */
	/* walk over upstream list and send withdraws for LSP that depend on
	 * the removed LSP */

	/* if independent distribution and adv on demand */
	/* Generate Event: Recognize New FEC for FEC. */
}
