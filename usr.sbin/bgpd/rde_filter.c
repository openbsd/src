/*	$OpenBSD: rde_filter.c,v 1.35 2005/11/01 10:58:29 claudio Exp $ */

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

#include <stdlib.h>
#include <string.h>

#include "bgpd.h"
#include "rde.h"

extern struct filter_head	*rules_l;	/* XXX ugly */

int	rde_filter_match(struct filter_rule *, struct rde_aspath *,
	    struct bgpd_addr *, u_int8_t);

enum filter_actions
rde_filter(struct rde_peer *peer, struct rde_aspath *asp,
    struct bgpd_addr *prefix, u_int8_t prefixlen, struct rde_peer *from,
    enum directions dir)
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
				    from, dir);
			if (f->action != ACTION_NONE)
				action = f->action;
			if (f->quick)
				return (action);
		}
	}
	return (action);
}

void
rde_apply_set(struct rde_aspath *asp, struct filter_set_head *sh,
    sa_family_t af, struct rde_peer *from, enum directions dir)
{
	struct filter_set	*set;
	struct aspath		*new;
	struct attr		*a;
	u_int16_t		 as;
	u_int8_t		 prepend;

	if (asp == NULL)
		return;

	TAILQ_FOREACH(set, sh, entry) {
		switch (set->type) {
		case ACTION_SET_LOCALPREF:
			asp->lpref = set->action.metric;
			break;
		case ACTION_SET_RELATIVE_LOCALPREF:
			if (set->action.relative > 0) {
				if (set->action.relative + asp->lpref <
				    asp->lpref)
					asp->lpref = UINT_MAX;
				else
					asp->lpref += set->action.relative;
			} else {
				if ((u_int32_t)-set->action.relative >
				    asp->lpref)
					asp->lpref = 0;
				else
					asp->lpref += set->action.relative;
			}
			break;
		case ACTION_SET_MED:
			asp->flags |= F_ATTR_MED | F_ATTR_MED_ANNOUNCE;
			asp->med = set->action.metric;
			break;
		case ACTION_SET_RELATIVE_MED:
			asp->flags |= F_ATTR_MED | F_ATTR_MED_ANNOUNCE;
			if (set->action.relative > 0) {
				if (set->action.relative + asp->med <
				    asp->med)
					asp->med = UINT_MAX;
				else
					asp->med += set->action.relative;
			} else {
				if ((u_int32_t)-set->action.relative >
				    asp->med)
					asp->med = 0;
				else
					asp->med += set->action.relative;
			}
			break;
		case ACTION_SET_WEIGHT:
			asp->weight = set->action.metric;
			break;
		case ACTION_SET_RELATIVE_WEIGHT:
			if (set->action.relative > 0) {
				if (set->action.relative + asp->weight <
				    asp->weight)
					asp->weight = UINT_MAX;
				else
					asp->weight += set->action.relative;
			} else {
				if ((u_int32_t)-set->action.relative >
				    asp->weight)
					asp->weight = 0;
				else
					asp->weight += set->action.relative;
			}
			break;
		case ACTION_SET_PREPEND_SELF:
			as = rde_local_as();
			prepend = set->action.prepend;
			new = aspath_prepend(asp->aspath, as, prepend);
			aspath_put(asp->aspath);
			asp->aspath = new;
			break;
		case ACTION_SET_PREPEND_PEER:
			if (from == NULL)
				break;
			as = from->conf.remote_as;
			prepend = set->action.prepend;
			new = aspath_prepend(asp->aspath, as, prepend);
			aspath_put(asp->aspath);
			asp->aspath = new;
			break;
		case ACTION_SET_NEXTHOP:
		case ACTION_SET_NEXTHOP_REJECT:
		case ACTION_SET_NEXTHOP_BLACKHOLE:
		case ACTION_SET_NEXTHOP_NOMODIFY:
			nexthop_modify(asp, &set->action.nexthop, set->type,
			    af);
			break;
		case ACTION_SET_COMMUNITY:
			if ((a = attr_optget(asp, ATTR_COMMUNITIES)) == NULL) {
				attr_optadd(asp, ATTR_OPTIONAL|ATTR_TRANSITIVE,
				    ATTR_COMMUNITIES, NULL, 0);
				if ((a = attr_optget(asp,
				    ATTR_COMMUNITIES)) == NULL)
					fatalx("internal community bug");
			}
			community_set(a, set->action.community.as,
			    set->action.community.type);
			break;
		case ACTION_PFTABLE:
			/* convert pftable name to an id */
			set->action.id = pftable_name2id(set->action.pftable);
			set->type = ACTION_PFTABLE_ID;
			/* FALLTHROUGH */
		case ACTION_PFTABLE_ID:
			pftable_unref(asp->pftableid);
			asp->pftableid = set->action.id;
			pftable_ref(asp->pftableid);
			break;
		case ACTION_RTLABEL:
			/* convert the route label to an id for faster access */
			set->action.id = rtlabel_name2id(set->action.rtlabel);
			set->type = ACTION_RTLABEL_ID;
			/* FALLTHROUGH */
		case ACTION_RTLABEL_ID:
			rtlabel_unref(asp->rtlabelid);
			asp->rtlabelid = set->action.id;
			rtlabel_ref(asp->rtlabelid);
			break;
		}
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

/* free a filterset and take care of possible name2id references */
void
filterset_free(struct filter_set_head *sh)
{
	struct filter_set	*s;

	while ((s = TAILQ_FIRST(sh)) != NULL) {
		TAILQ_REMOVE(sh, s, entry);
		if (s->type == ACTION_RTLABEL_ID)
			rtlabel_unref(s->action.id);
		else if (s->type == ACTION_PFTABLE_ID)
			pftable_unref(s->action.id);
		free(s);
	}
}

/*
 * this function is a bit more complicated than a memcmp() because there are
 * types that need to be considered equal e.g. ACTION_SET_MED and
 * ACTION_SET_RELATIVE_MED. Also ACTION_SET_COMMUNITY and ACTION_SET_NEXTHOP
 * need some special care.
 */
int
filterset_cmp(struct filter_set *a, struct filter_set *b)
{
	if (strcmp(filterset_names[a->type], filterset_names[b->type]))
		return (a->type - b->type);

	if (a->type == ACTION_SET_COMMUNITY) {	/* a->type == b->type */
		/* compare community */
		if (a->action.community.as - b->action.community.as != 0)
			return (a->action.community.as -
			    b->action.community.as);
		return (a->action.community.type - b->action.community.type);
	}

	if (a->type == ACTION_SET_NEXTHOP && b->type == ACTION_SET_NEXTHOP) {
		/*
		 * This is the only intresting case, all others are considered
		 * equal. It does not make sense to e.g. set a nexthop and
		 * reject it at the same time. Allow one IPv4 and one IPv6
		 * per filter set or only one of the other nexthop modifiers.
		 */
		return (a->action.nexthop.af - b->action.nexthop.af);
	}

	/* equal */
	return (0);
}


