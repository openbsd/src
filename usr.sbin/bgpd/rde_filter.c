/*	$OpenBSD: rde_filter.c,v 1.8 2004/05/07 10:06:15 djm Exp $ */

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

int	rde_filter_match(struct filter_rule *, struct attr_flags *,
	    struct bgpd_addr *, u_int8_t);

enum filter_actions
rde_filter(struct rde_peer *peer, struct attr_flags *attrs,
    struct bgpd_addr *prefix, u_int8_t prefixlen, enum directions dir)
{
	struct filter_rule	*f;
	enum filter_actions	 action = ACTION_ALLOW; /* default allow */

	TAILQ_FOREACH(f, rules_l, entries) {
		if (dir != f->dir)
			continue;
		if (f->peer.groupid != 0 &&
		    f->peer.groupid != peer->conf.groupid)
			continue;
		if (f->peer.peerid != 0 &&
		    f->peer.peerid != peer->conf.id)
			continue;
		if (rde_filter_match(f, attrs, prefix, prefixlen)) {
			rde_apply_set(attrs, &f->set);
			if (f->action != ACTION_NONE)
				action = f->action;
			if (f->quick)
				return (action);
		}
	}
	return (action);
}

void
rde_apply_set(struct attr_flags *attrs, struct filter_set *set)
{
	if (attrs == NULL)
		return;

	if (set->flags & SET_LOCALPREF)
		attrs->lpref = set->localpref;
	if (set->flags & SET_MED)
		attrs->med = set->med;
	if (set->flags & SET_NEXTHOP)
		attrs->nexthop = set->nexthop;
	if (set->flags & SET_PREPEND) {
		/*
		 * The actual prepending is done afterwards because
		 * This could overflow but somebody that uses that many
		 * prepends is loony and needs professional help.
		 */
		attrs->aspath->hdr.prepend += set->prepend;
		attrs->aspath->hdr.as_cnt += set->prepend;
	}
	if (set->flags & SET_PFTABLE)
		strlcpy(attrs->pftable, set->pftable, sizeof(attrs->pftable));
}

int
rde_filter_match(struct filter_rule *f, struct attr_flags *attrs,
    struct bgpd_addr *prefix, u_int8_t plen)
{
	in_addr_t	mask;

	if (attrs != NULL && f->match.as.type != AS_NONE)
		if (aspath_match(attrs->aspath, f->match.as.type,
		    f->match.as.as) == 0)
			return (0);

	if (attrs != NULL && f->match.community.as != 0)
		if (rde_filter_community(attrs, f->match.community.as,
		    f->match.community.type) == 0)
			return (0);

	if (f->match.prefix.addr.af != 0 &&
	    f->match.prefix.addr.af == prefix->af) {
		switch (f->match.prefix.addr.af) {
		case AF_INET:
			mask = htonl(0xffffffff << (32 - f->match.prefix.len));
			if ((prefix->v4.s_addr & mask) !=
			    (f->match.prefix.addr.v4.s_addr & mask))
				return (0);
			break;
		default:
			fatalx("rde_filter_match: unsupported address family");
		}

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
rde_filter_community(struct attr_flags *attr, int as, int type)
{
	struct attr	*a;

	a = attr_optget(attr, ATTR_COMMUNITIES);
	if (a == NULL)
		/* no communities, no match */
		return (0);

	return (community_match(a->data, a->len, as, type));
}
