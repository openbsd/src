/*	$OpenBSD: rde_filter.c,v 1.21 2004/10/08 16:36:42 claudio Exp $ */

/*
 * Copyright (c) 2004 Claudio Jeker <claudio@openbsd.org>
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
#include <sys/queue.h>

#include <string.h>

#include "bgpd.h"
#include "rde.h"

extern struct filter_head	*rules_l;	/* XXX ugly */

int	rde_filter_match(struct filter_rule *, struct rde_aspath *,
	    struct bgpd_addr *, u_int8_t);

enum filter_actions
rde_filter(struct rde_peer *peer, struct rde_aspath *asp,
    struct bgpd_addr *prefix, u_int8_t prefixlen, enum directions dir)
{
	struct filter_rule	*f;
	enum filter_actions	 action = ACTION_ALLOW; /* default allow */

	TAILQ_FOREACH(f, rules_l, entry) {
		if (dir != f->dir)
			continue;
		if (f->peer.groupid != 0 &&
		    f->peer.groupid != peer->conf.groupid)
			continue;
		if (f->peer.peerid != 0 &&
		    f->peer.peerid != peer->conf.id)
			continue;
		if (rde_filter_match(f, asp, prefix, prefixlen)) {
			if (asp != NULL)
				rde_apply_set(asp, &f->set, prefix->af,
				    asp->peer, dir);
			if (f->action != ACTION_NONE)
				action = f->action;
			if (f->quick)
				return (action);
		}
	}
	return (action);
}

void
rde_apply_set(struct rde_aspath *asp, struct filter_set *set, sa_family_t af,
    struct rde_peer *peer, enum directions dir)
{
	struct aspath	*new;
	u_int16_t	 as;
	u_int8_t	 prepend;

	if (asp == NULL)
		return;

	if (set->flags & SET_PREPEND_SELF && dir != DIR_DEFAULT_IN) {
		/* don't apply if this is a incoming default override */
		as = rde_local_as();
		prepend = set->prepend_self;
		new = aspath_prepend(asp->aspath, as, prepend);
		aspath_put(asp->aspath);
		asp->aspath = new;
	}

	if (dir == DIR_DEFAULT_OUT)
		/*
		 * default outgoing overrides are only allowed to
		 * set prepend-self
		 */
		return;

	if (set->flags & SET_PREPEND_PEER) {
		as = peer->conf.remote_as;
		prepend = set->prepend_peer;
		new = aspath_prepend(asp->aspath, as, prepend);
		aspath_put(asp->aspath);
		asp->aspath = new;
	}

	if (set->flags & SET_LOCALPREF)
		asp->lpref = set->localpref;
	if (set->flags & SET_MED) {
		asp->flags |= F_ATTR_MED | F_ATTR_MED_ANNOUNCE;
		asp->med = set->med;
	}

	nexthop_modify(asp, &set->nexthop, set->flags, af);

	if (set->flags & SET_PFTABLE)
		strlcpy(asp->pftable, set->pftable, sizeof(asp->pftable));
	if (set->flags & SET_COMMUNITY) {
		struct attr *a;

		if ((a = attr_optget(asp, ATTR_COMMUNITIES)) == NULL) {
			attr_optadd(asp, ATTR_OPTIONAL|ATTR_TRANSITIVE,
			    ATTR_COMMUNITIES, NULL, 0);
			if ((a = attr_optget(asp, ATTR_COMMUNITIES)) == NULL)
				fatalx("internal community bug");
		}
		community_set(a, set->community.as, set->community.type);
	}
}

int
rde_filter_match(struct filter_rule *f, struct rde_aspath *asp,
    struct bgpd_addr *prefix, u_int8_t plen)
{

	if (asp != NULL && f->match.as.type != AS_NONE)
		if (aspath_match(asp->aspath, f->match.as.type,
		    f->match.as.as) == 0)
			return (0);

	if (asp != NULL && f->match.community.as != 0)
		if (rde_filter_community(asp, f->match.community.as,
		    f->match.community.type) == 0)
			return (0);

	if (f->match.prefix.addr.af != 0 &&
	    f->match.prefix.addr.af == prefix->af) {
		if (prefix_compare(prefix, &f->match.prefix.addr,
		    f->match.prefix.len))
			return (0);

		/* test prefixlen stuff too */
		switch (f->match.prefixlen.op) {
		case OP_NONE:
			/* perfect match */
			return (plen == f->match.prefix.len);
		case OP_RANGE:
			return ((plen >= f->match.prefixlen.len_min) &&
			    (plen <= f->match.prefixlen.len_max));
		case OP_XRANGE:
			return ((plen < f->match.prefixlen.len_min) ||
			    (plen > f->match.prefixlen.len_max));
		case OP_EQ:
			return (plen == f->match.prefixlen.len_min);
		case OP_NE:
			return (plen != f->match.prefixlen.len_min);
		case OP_LE:
			return (plen <= f->match.prefixlen.len_min);
		case OP_LT:
			return (plen < f->match.prefixlen.len_min);
		case OP_GE:
			return (plen >= f->match.prefixlen.len_min);
		case OP_GT:
			return (plen > f->match.prefixlen.len_min);
		}
		/* NOTREACHED */
	} else if (f->match.prefixlen.op != OP_NONE) {
		/* only prefixlen without a prefix */

		if (f->match.prefixlen.af != prefix->af)
			/* don't use IPv4 rules for IPv6 and vice versa */
			return (0);

		switch (f->match.prefixlen.op) {
		case OP_NONE:
			fatalx("internal filter bug");
		case OP_RANGE:
			return ((plen >= f->match.prefixlen.len_min) &&
			    (plen <= f->match.prefixlen.len_max));
		case OP_XRANGE:
			return ((plen < f->match.prefixlen.len_min) ||
			    (plen > f->match.prefixlen.len_max));
		case OP_EQ:
			return (plen == f->match.prefixlen.len_min);
		case OP_NE:
			return (plen != f->match.prefixlen.len_min);
		case OP_LE:
			return (plen <= f->match.prefixlen.len_min);
		case OP_LT:
			return (plen < f->match.prefixlen.len_min);
		case OP_GE:
			return (plen >= f->match.prefixlen.len_min);
		case OP_GT:
			return (plen > f->match.prefixlen.len_min);
		}
		/* NOTREACHED */
	}

	/* matched somewhen or is anymatch rule  */
	return (1);
}

int
rde_filter_community(struct rde_aspath *asp, int as, int type)
{
	struct attr	*a;

	a = attr_optget(asp, ATTR_COMMUNITIES);
	if (a == NULL)
		/* no communities, no match */
		return (0);

	return (community_match(a->data, a->len, as, type));
}
