/*	$OpenBSD: lde_lib.c,v 1.10 2010/02/19 12:49:21 claudio Exp $ */

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
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <net/if_types.h>
#include <netmpls/mpls.h>
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

extern struct ldpd_conf		*ldeconf;
RB_HEAD(rt_tree, rt_node)	 rt;
RB_PROTOTYPE(rt_tree, rt_node, entry, rt_compare)
RB_GENERATE(rt_tree, rt_node, entry, rt_compare)

u_int32_t	lde_assign_label(void);

/* route table */
void
rt_init(void)
{
	RB_INIT(&rt);
}

int
rt_compare(struct rt_node *a, struct rt_node *b)
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

struct rt_node *
rt_find(in_addr_t prefix, u_int8_t prefixlen)
{
	struct rt_node	 s;

	s.prefix.s_addr = prefix;
	s.prefixlen = prefixlen;

	return (RB_FIND(rt_tree, &rt, &s));
}

int
rt_insert(struct rt_node *r)
{
	if (RB_INSERT(rt_tree, &rt, r) != NULL) {
		log_warnx("rt_insert failed for %s/%u",
		    inet_ntoa(r->prefix), r->prefixlen);
		free(r);
		return (-1);
	}

	return (0);
}

int
rt_remove(struct rt_node *r)
{
	if (RB_REMOVE(rt_tree, &rt, r) == NULL) {
		log_warnx("rt_remove failed for %s/%u",
		    inet_ntoa(r->prefix), r->prefixlen);
		return (-1);
	}

	free(r);
	return (0);
}

void
rt_dump(pid_t pid)
{
	struct rt_node		*r;
	static struct ctl_rt	 rtctl;

	RB_FOREACH(r, rt_tree, &rt) {
		rtctl.prefix.s_addr = r->prefix.s_addr;
		rtctl.prefixlen = r->prefixlen;
		rtctl.nexthop.s_addr = r->nexthop.s_addr;
		rtctl.flags = r->flags;
		rtctl.local_label = r->local_label;
		rtctl.remote_label = r->remote_label;

		if (!r->present)
			rtctl.in_use = 0;
		else
			rtctl.in_use = 1;

		if (rtctl.nexthop.s_addr == htonl(INADDR_LOOPBACK))
			rtctl.connected = 1;
		else
			rtctl.connected = 0;

		lde_imsg_compose_ldpe(IMSG_CTL_SHOW_LIB, 0, pid, &rtctl,
		    sizeof(rtctl));
	}
}

void
rt_snap(u_int32_t peerid)
{
	struct rt_node	*r;
	struct map	 map;

	bzero(&map, sizeof(map));

	RB_FOREACH(r, rt_tree, &rt) {
		map.prefix = r->prefix.s_addr;
		map.prefixlen = r->prefixlen;
		map.label = (ntohl(r->local_label) & MPLS_LABEL_MASK) >>
		    MPLS_LABEL_OFFSET;

		lde_imsg_compose_ldpe(IMSG_MAPPING_ADD, peerid, 0, &map,
		    sizeof(map));
	}
}

void
rt_clear(void)
{
	struct rt_node	*r;

	while ((r = RB_MIN(rt_tree, &rt)) != NULL)
		rt_remove(r);
}

u_int32_t
lde_assign_label()
{
	static u_int32_t label = MPLS_LABEL_RESERVED_MAX;

	/* XXX some checks needed */
	label++;
	return (htonl(label << MPLS_LABEL_OFFSET));
}

void
lde_kernel_insert(struct kroute *kr)
{
	struct rt_node		*rn;
	struct rt_label		*rl;
	struct iface		*iface;
	struct lde_nbr		*ln;
	struct lde_nbr_address	*addr;
	struct map		 localmap;

	rn = rt_find(kr->prefix.s_addr, kr->prefixlen);
	if (rn == NULL) {
		rn = calloc(1, sizeof(*rn));
		if (rn == NULL)
			fatal("lde_insert");

		rn->prefix.s_addr = kr->prefix.s_addr;
		rn->prefixlen = kr->prefixlen;
		TAILQ_INIT(&rn->labels_list);

		rt_insert(rn);
	}

	if (rn->present) {
		if (kr->nexthop.s_addr == rn->nexthop.s_addr)
			return;

		/* The nexthop has changed, change also the label associated
		   with prefix */
		rn->remote_label = 0;
		rn->nexthop.s_addr = kr->nexthop.s_addr;

		if ((ldeconf->mode & MODE_RET_LIBERAL) == 0) {
			/* XXX: we support just liberal retention for now */
			log_warnx("lde_kernel_insert: missing mode");
			return;
		}

		TAILQ_FOREACH(rl, &rn->labels_list, node_l) {
			addr = lde_address_find(rl->nexthop, &rn->nexthop);
			if (addr != NULL) {
				rn->remote_label =
				    htonl(rl->label << MPLS_LABEL_OFFSET);
				break;
			}
		}

		log_debug("lde_kernel_insert: prefix %s, changing label to %u",
		    inet_ntoa(rn->prefix), rl ? rl->label : 0);

		lde_send_change_klabel(rn);
		return;
	}

	rn->present = 1;
	rn->nexthop.s_addr = kr->nexthop.s_addr;

	/* There is static assigned label for this route, record it in lib */
	if (kr->local_label) {
		rn->local_label = (htonl(kr->local_label) << MPLS_LABEL_OFFSET);
		return;
	}

	TAILQ_FOREACH(rl, &rn->labels_list, node_l) {
		addr = lde_address_find(rl->nexthop, &rn->nexthop);
		if (addr != NULL) {
			rn->remote_label =
			    htonl(rl->label << MPLS_LABEL_OFFSET);
			break;
		}
	}

	if (!rn->local_label) {
		/* Directly connected route */
		if (kr->nexthop.s_addr == INADDR_ANY) {
			rn->local_label =
			    htonl(MPLS_LABEL_IMPLNULL << MPLS_LABEL_OFFSET);
			rn->nexthop.s_addr = htonl(INADDR_LOOPBACK);
		} else
			rn->local_label = lde_assign_label();
	}

	lde_send_insert_klabel(rn);

	/* Redistribute the current mapping to every nbr */
	localmap.label = (ntohl(rn->local_label) & MPLS_LABEL_MASK) >>
	   MPLS_LABEL_OFFSET;
	localmap.prefix = rn->prefix.s_addr;
	localmap.prefixlen = rn->prefixlen;

	LIST_FOREACH(iface, &ldeconf->iface_list, entry) {
	       LIST_FOREACH(ln, &iface->lde_nbr_list, entry) {
		       if (ln->self)
			       continue;

		       if (ldeconf->mode & MODE_ADV_UNSOLICITED &&
			   ldeconf->mode & MODE_DIST_INDEPENDENT)
			       lde_send_labelmapping(ln->peerid, &localmap);

		       if (ldeconf->mode & MODE_ADV_UNSOLICITED &&
			   ldeconf->mode & MODE_DIST_ORDERED) {
			       /* XXX */
			       if (rn->nexthop.s_addr == INADDR_ANY ||
				   rn->remote_label != 0)
				       lde_send_labelmapping(ln->peerid,
					   &localmap);
		       }
	       }
	}
}

void
lde_kernel_remove(struct kroute *kr)
{
	struct rt_node		*rn;
	struct rt_label		*rl;
	struct lde_nbr		*ln;

	rn = rt_find(kr->prefix.s_addr, kr->prefixlen);
	if (rn == NULL)
		return;

	if (ldeconf->mode & MODE_RET_LIBERAL) {
		ln = lde_find_address(rn->nexthop);
		if (ln) {
			rl = calloc(1, sizeof(*rl));
			if (rl == NULL)
				fatal("lde_kernel_remove");

			rl->label = rn->remote_label;
			rl->node = rn;
			rl->nexthop = ln;
			TAILQ_INSERT_TAIL(&rn->labels_list, rl, node_l);
			TAILQ_INSERT_TAIL(&ln->labels_list, rl, nbr_l);
		}
	}

	rn->remote_label = 0;
	rn->nexthop.s_addr = INADDR_ANY;
	rn->present = 0;
}

void
lde_check_mapping(struct map *map, struct lde_nbr *ln)
{
	struct rt_node		*rn;
	struct rt_label		*rl;
	struct lde_nbr_address	*addr;
	struct lde_map_entry	*me, *menew;
	struct lde_req_entry	*req;
	struct iface		*iface;
	struct map		 localmap;

	/* The route is not yet in fib. If we are in liberal mode create a
	   route and record the label */
	rn = rt_find(map->prefix, map->prefixlen);
	if (rn == NULL) {
		if (ldeconf->mode & MODE_RET_CONSERVATIVE)
			return;

		rn = calloc(1, sizeof(*rn));
		if (rn == NULL)
			fatal("lde_check_mapping");

		rn->prefix.s_addr = map->prefix;
		rn->prefixlen = map->prefixlen;
		rn->local_label = lde_assign_label();
		rn->present = 0;

		TAILQ_INIT(&rn->labels_list);

		rt_insert(rn);
	}

	TAILQ_FOREACH(me, &ln->recv_map_list, entry) {
		if (me->prefix.s_addr == map->prefix &&
		    me->prefixlen == map->prefixlen) {
			if (me->label == map->label) {
				lde_send_labelrelease(ln->peerid, map);
				return;
			}
		}
	}

	addr = lde_address_find(ln, &rn->nexthop);
	if (addr == NULL || !rn->present) {
		if (ldeconf->mode & MODE_RET_CONSERVATIVE) {
			lde_send_labelrelease(ln->peerid, map);
			return;
		}

		rl = calloc(1, sizeof(*rl));
		if (rl == NULL)
			fatal("lde_check_mapping");

		rl->label = map->label;
		rl->node = rn;
		rl->nexthop = ln;

		TAILQ_INSERT_TAIL(&rn->labels_list, rl, node_l);
		TAILQ_INSERT_TAIL(&ln->labels_list, rl, nbr_l);
		return;
	}

	rn->remote_label = htonl(map->label << MPLS_LABEL_OFFSET);

	/* If we are ingress for this LSP install the label */
	if (rn->nexthop.s_addr == INADDR_ANY)
		lde_send_change_klabel(rn);

	/* Record the mapping from this peer */	
	menew = calloc(1, sizeof(*menew));
	if (menew == NULL)
		fatal("lde_check_mapping");

	menew->prefix.s_addr = map->prefix;
	menew->prefixlen = map->prefixlen;
	menew->label = map->label;

	TAILQ_INSERT_HEAD(&ln->recv_map_list, menew, entry);

	/* Redistribute the current mapping to every nbr */
	localmap.label = rn->local_label;
	localmap.prefix = rn->prefix.s_addr;
	localmap.prefixlen = rn->prefixlen;

	LIST_FOREACH(iface, &ldeconf->iface_list, entry) {
		LIST_FOREACH(ln, &iface->lde_nbr_list, entry) {
			/* Did we already send a mapping to this peer? */
			TAILQ_FOREACH(me, &ln->sent_map_list, entry) {
				if (me->prefix.s_addr == rn->prefix.s_addr &&
				    me->prefixlen == rn->prefixlen)
					break;
			}
			if (me != NULL) {
				/* XXX: check RAttributes */
				continue;
			}

			if (ldeconf->mode & MODE_ADV_UNSOLICITED &&
			    ldeconf->mode & MODE_DIST_ORDERED) {
				lde_send_labelmapping(ln->peerid, &localmap);

				menew = calloc(1, sizeof(*menew));
				if (menew == NULL)
					fatal("lde_check_mapping");

				menew->prefix.s_addr = map->prefix;
				menew->prefixlen = map->prefixlen;
				menew->label = map->label;

				TAILQ_INSERT_HEAD(&ln->sent_map_list, menew,
				    entry);
			}

			TAILQ_FOREACH(req, &ln->req_list, entry) {
				if (req->prefix.s_addr == rn->prefix.s_addr &&
				    req->prefixlen == rn->prefixlen)
					break;
			}
			if (req != NULL) {
				lde_send_labelmapping(ln->peerid, &localmap);

				menew = calloc(1, sizeof(*menew));
				if (menew == NULL)
					fatal("lde_check_mapping");

				menew->prefix.s_addr = map->prefix;
				menew->prefixlen = map->prefixlen;
				menew->label = map->label;

				TAILQ_INSERT_HEAD(&ln->sent_map_list, menew,
				    entry);

				TAILQ_REMOVE(&ln->req_list, req, entry);
				free(req);
			}
		}
	}

	lde_send_change_klabel(rn);
}

void
lde_check_request(struct map *map, struct lde_nbr *ln)
{
	struct lde_req_entry	*lre, *newlre;
	struct rt_node		*rn;
	struct lde_nbr		*lnn;
	struct map		 localmap;

	bzero(&newlre, sizeof(newlre));

	rn = rt_find(map->prefix, map->prefixlen);
	if (rn == NULL || rn->remote_label == NO_LABEL) {
		lde_send_notification(ln->peerid, S_NO_ROUTE, map->messageid,
		    MSG_TYPE_LABELREQUEST);
		return;
	}

	if (lde_address_find(ln, &rn->nexthop)) {
		lde_send_notification(ln->peerid, S_LOOP_DETECTED,
		    map->messageid, MSG_TYPE_LABELREQUEST);
		return;
	}

	TAILQ_FOREACH(lre, &ln->req_list, entry) {
		if (lre->prefix.s_addr == map->prefix &&
		    lre->prefixlen == map->prefixlen)
			return;
	}

	/* XXX: if we are egress ? */
	if (rn->remote_label != NO_LABEL) {
		bzero(&localmap, sizeof(localmap));
		localmap.prefix = map->prefix;
		localmap.prefixlen = map->prefixlen;
		localmap.label = rn->local_label;

		lde_send_labelmapping(ln->peerid, &localmap);
	} else {
		lnn = lde_find_address(rn->nexthop);
		if (lnn == NULL)
			return;

		lde_send_labelrequest(lnn->peerid, map);

		newlre = calloc(1, sizeof(*newlre));
		if (newlre == NULL)
			fatal("lde_check_request");

		newlre->prefix.s_addr = map->prefix;
		newlre->prefixlen = map->prefixlen;

		TAILQ_INSERT_HEAD(&ln->req_list, newlre, entry);
	}
}

void
lde_label_list_free(struct lde_nbr *nbr)
{
	struct rt_label	*rl;

	while ((rl = TAILQ_FIRST(&nbr->labels_list)) != NULL) {
		TAILQ_REMOVE(&nbr->labels_list, rl, nbr_l);
		TAILQ_REMOVE(&nbr->labels_list, rl, node_l);
		if (TAILQ_EMPTY(&rl->node->labels_list))
			rt_remove(rl->node);
		free(rl);
	}
}
