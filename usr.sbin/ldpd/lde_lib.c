/*	$OpenBSD: lde_lib.c,v 1.22 2010/06/30 01:47:11 claudio Exp $ */

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


void
rt_dump(pid_t pid)
{
	struct fec		*f;
	struct rt_node		*r;
	static struct ctl_rt	 rtctl;

	RB_FOREACH(f, fec_tree, &rt) {
		r = (struct rt_node *)f;
		rtctl.prefix.s_addr = r->fec.prefix.s_addr;
		rtctl.prefixlen = r->fec.prefixlen;
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
	struct fec	*f;
	struct rt_node	*r;
	struct map	 map;

	bzero(&map, sizeof(map));
	RB_FOREACH(f, fec_tree, &rt) {
		r = (struct rt_node *)f;
		map.prefix = r->fec.prefix;
		map.prefixlen = r->fec.prefixlen;
		map.label = r->local_label;

		lde_imsg_compose_ldpe(IMSG_MAPPING_ADD, peerid, 0, &map,
		    sizeof(map));
	}
}

void
rt_clear(void)
{
	fec_clear(&rt, free);
}

void
lde_kernel_insert(struct kroute *kr)
{
	struct rt_node		*rn;
	struct lde_nbr_address	*addr;
	struct lde_map		*map;

	log_debug("kernel add route %s/%u", inet_ntoa(kr->prefix),
	    kr->prefixlen);

	rn = (struct rt_node *)fec_find_prefix(&rt, kr->prefix.s_addr,
	    kr->prefixlen);
	if (rn == NULL) {
		rn = calloc(1, sizeof(*rn));
		if (rn == NULL)
			fatal("lde_insert");

		rn->fec.prefix.s_addr = kr->prefix.s_addr;
		rn->fec.prefixlen = kr->prefixlen;
		rn->remote_label = NO_LABEL;
		rn->local_label = NO_LABEL;
		LIST_INIT(&rn->upstream);
		LIST_INIT(&rn->downstream);

		if (fec_insert(&rt, &rn->fec))
			log_warnx("failed to add %s/%u to rt tree",
			    inet_ntoa(rn->fec.prefix), rn->fec.prefixlen);
	}

	if (rn->present) {
		if (kr->nexthop.s_addr == rn->nexthop.s_addr)
			return;

		/* The nexthop has changed, change also the label associated
		   with prefix */
		rn->remote_label = NO_LABEL;
		rn->nexthop.s_addr = kr->nexthop.s_addr;

		if ((ldeconf->mode & MODE_RET_LIBERAL) == 0) {
			/* XXX: we support just liberal retention for now */
			log_warnx("lde_kernel_insert: missing mode");
			return;
		}

		LIST_FOREACH(map, &rn->downstream, entry) {
			addr = lde_address_find(map->nexthop, &rn->nexthop);
			if (addr != NULL) {
				rn->remote_label = map->label;
				break;
			}
		}

		log_debug("lde_kernel_insert: prefix %s%u, "
		    "changing label to %u", inet_ntoa(rn->fec.prefix),
		    rn->fec.prefixlen, map ? map->label : 0);

		lde_send_change_klabel(rn);
		return;
	}

	rn->present = 1;
	rn->nexthop.s_addr = kr->nexthop.s_addr;

	/* There is static assigned label for this route, record it in lib */
	if (kr->local_label != NO_LABEL) {
		rn->local_label = kr->local_label;
		return;
	}

	LIST_FOREACH(map, &rn->downstream, entry) {
		addr = lde_address_find(map->nexthop, &rn->nexthop);
		if (addr != NULL) {
			rn->remote_label = map->label;
			break;
		}
	}

	if (rn->local_label == NO_LABEL) {
		/* Directly connected route */
		if (kr->nexthop.s_addr == INADDR_ANY) {
			rn->local_label = MPLS_LABEL_IMPLNULL;
			rn->nexthop.s_addr = htonl(INADDR_LOOPBACK);
		} else
			rn->local_label = lde_assign_label();
	}

	lde_send_insert_klabel(rn);

	/* Redistribute the current mapping to every nbr */
	lde_nbr_do_mappings(rn);
}

void
lde_kernel_remove(struct kroute *kr)
{
	struct rt_node		*rn;
	struct lde_map		*map;
	struct lde_nbr		*ln;

	log_debug("kernel remove route %s/%u", inet_ntoa(kr->prefix),
	    kr->prefixlen);

	rn = (struct rt_node *)fec_find_prefix(&rt, kr->prefix.s_addr,
	    kr->prefixlen);
	if (rn == NULL)
		return;

	if (ldeconf->mode & MODE_RET_LIBERAL) {
		ln = lde_find_address(rn->nexthop);
		if (ln) {
			map = calloc(1, sizeof(*map));
			if (map == NULL)
				fatal("lde_kernel_remove");

			map->label = rn->remote_label;
			map->fec = rn->fec;
			map->nexthop = ln;
			LIST_INSERT_HEAD(&rn->downstream, map, entry);
			if (fec_insert(&ln->recv_map, &map->fec))
				log_warnx("failed to add %s/%u to recv map (1)",
				    inet_ntoa(map->fec.prefix),
				    map->fec.prefixlen);
		}
	}

	rn->remote_label = NO_LABEL;
	rn->nexthop.s_addr = INADDR_ANY;
	rn->present = 0;
}

void
lde_check_mapping(struct map *map, struct lde_nbr *ln)
{
	struct rt_node		*rn;
	struct lde_nbr_address	*addr;
	struct lde_map		*me;

	log_debug("label mapping from nbr %s, FEC %s/%u, label %u",
	    inet_ntoa(ln->id), log_fec(map), map->label);

	rn = (struct rt_node *)fec_find_prefix(&rt, map->prefix.s_addr,
	    map->prefixlen);
	if (rn == NULL) {
		/* The route is not yet in fib. If we are in liberal mode
		 *  create a route and record the label */
		if (ldeconf->mode & MODE_RET_CONSERVATIVE)
			return;

		rn = calloc(1, sizeof(*rn));
		if (rn == NULL)
			fatal("lde_check_mapping");

		rn->fec.prefix = map->prefix;
		rn->fec.prefixlen = map->prefixlen;
		rn->local_label = lde_assign_label();
		rn->remote_label = NO_LABEL;
		rn->present = 0;

		LIST_INIT(&rn->upstream);
		LIST_INIT(&rn->downstream);

		if (fec_insert(&rt, &rn->fec))
			log_warnx("failed to add %s/%u to rt tree",
			    inet_ntoa(rn->fec.prefix), rn->fec.prefixlen);
	}

	LIST_FOREACH(me, &rn->downstream, entry) {
		if (ln == me->nexthop) {
			if (me->label == map->label) {
				/* Duplicate: RFC says to send back a release,
				 * even though we did not release the actual
				 * mapping. This is confusing.
				 */
				lde_send_labelrelease(ln->peerid, map);
				return;
			}
			/* old mapping that is now changed */
			break;
		}
	}

	addr = lde_address_find(ln, &rn->nexthop);
	if (addr == NULL || !rn->present) {
		/* route not yet available */
		if (ldeconf->mode & MODE_RET_CONSERVATIVE) {
			lde_send_labelrelease(ln->peerid, map);
			return;
		}
		/* in liberal mode just note the mapping */
		if (me == NULL) {
			me = calloc(1, sizeof(*me));
			if (me == NULL)
				fatal("lde_check_mapping");
			me->fec = rn->fec;
			me->nexthop = ln;

			LIST_INSERT_HEAD(&rn->downstream, me, entry);
			if (fec_insert(&ln->recv_map, &me->fec))
				log_warnx("failed to add %s/%u to recv map (2)",
				    inet_ntoa(me->fec.prefix),
				    me->fec.prefixlen);
		}
		me->label = map->label;

		return;
	}

	rn->remote_label = map->label;

	/* If we are ingress for this LSP install the label */
	if (rn->nexthop.s_addr == INADDR_ANY)
		lde_send_change_klabel(rn);

	/* Record the mapping from this peer */	
	if (me == NULL) {
		me = calloc(1, sizeof(*me));
		if (me == NULL)
			fatal("lde_check_mapping");

		me->fec = rn->fec;
		me->nexthop = ln;
		LIST_INSERT_HEAD(&rn->downstream, me, entry);
		if (fec_insert(&ln->recv_map, &me->fec))
			log_warnx("failed to add %s/%u to recv map (3)",
			    inet_ntoa(me->fec.prefix), me->fec.prefixlen);
	}
	me->label = map->label;

	lde_send_change_klabel(rn);

	/* Redistribute the current mapping to every nbr */
	lde_nbr_do_mappings(rn);
}

void
lde_check_request(struct map *map, struct lde_nbr *ln)
{
	struct lde_req	*lre;
	struct rt_node	*rn;
	struct lde_nbr	*lnn;
	struct map	 localmap;

	log_debug("label request from nbr %s, FEC %s",
	    inet_ntoa(ln->id), log_fec(map));

	rn = (struct rt_node *)fec_find_prefix(&rt, map->prefix.s_addr,
	    map->prefixlen);
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

	lre = (struct lde_req *)fec_find(&ln->recv_req, &rn->fec);
	if (lre != NULL)
		return;

	if (rn->nexthop.s_addr == INADDR_ANY ||
	    rn->remote_label != NO_LABEL) {
		bzero(&localmap, sizeof(localmap));
		localmap.prefix = map->prefix;
		localmap.prefixlen = map->prefixlen;
		localmap.label = rn->local_label;

		lde_send_labelmapping(ln->peerid, &localmap);
	} else {
		lnn = lde_find_address(rn->nexthop);
		if (lnn == NULL)
			/* XXX this feels wrong.... */
			return;

		lde_send_labelrequest(lnn->peerid, map);

		lre = calloc(1, sizeof(*lre));
		if (lre == NULL)
			fatal("lde_check_request");

		lre->fec = rn->fec;
		lre->msgid = map->messageid;

		if (fec_insert(&ln->recv_req, &lre->fec))
			log_warnx("failed to add %s/%u to recv req",
			    inet_ntoa(lre->fec.prefix), lre->fec.prefixlen);
	}
}

void
lde_check_release(struct map *map, struct lde_nbr *ln)
{
	log_debug("label mapping from nbr %s, FEC %s",
	    inet_ntoa(ln->id), log_fec(map));

	/* check withdraw list */
	/* check sent map list */
}
