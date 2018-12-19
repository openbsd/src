/*	$OpenBSD: rde_filter.c,v 1.116 2018/12/19 15:26:42 claudio Exp $ */

/*
 * Copyright (c) 2004 Claudio Jeker <claudio@openbsd.org>
 * Copyright (c) 2016 Job Snijders <job@instituut.net>
 * Copyright (c) 2016 Peter Hessler <phessler@openbsd.org>
 * Copyright (c) 2018 Sebastian Benoit <benno@openbsd.org>
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

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "bgpd.h"
#include "rde.h"
#include "log.h"

int	rde_filter_match(struct filter_rule *, struct rde_peer *,
	    struct filterstate *, struct prefix *);
int	rde_prefix_match(struct filter_prefix *, struct prefix *);
int	filterset_equal(struct filter_set_head *, struct filter_set_head *);

void
rde_apply_set(struct filter_set_head *sh, struct filterstate *state,
    u_int8_t aid, struct rde_peer *from, struct rde_peer *peer)
{
	struct filter_set	*set;
	u_char			*np;
	u_int32_t		 prep_as;
	u_int16_t		 nl;
	u_int8_t		 prepend;

	if (state == NULL)
		return;

	TAILQ_FOREACH(set, sh, entry) {
		switch (set->type) {
		case ACTION_SET_LOCALPREF:
			state->aspath.lpref = set->action.metric;
			break;
		case ACTION_SET_RELATIVE_LOCALPREF:
			if (set->action.relative > 0) {
				if (set->action.relative + state->aspath.lpref <
				    state->aspath.lpref)
					state->aspath.lpref = UINT_MAX;
				else
					state->aspath.lpref +=
					    set->action.relative;
			} else {
				if ((u_int32_t)-set->action.relative >
				    state->aspath.lpref)
					state->aspath.lpref = 0;
				else
					state->aspath.lpref +=
					    set->action.relative;
			}
			break;
		case ACTION_SET_MED:
			state->aspath.flags |= F_ATTR_MED | F_ATTR_MED_ANNOUNCE;
			state->aspath.med = set->action.metric;
			break;
		case ACTION_SET_RELATIVE_MED:
			state->aspath.flags |= F_ATTR_MED | F_ATTR_MED_ANNOUNCE;
			if (set->action.relative > 0) {
				if (set->action.relative + state->aspath.med <
				    state->aspath.med)
					state->aspath.med = UINT_MAX;
				else
					state->aspath.med +=
					    set->action.relative;
			} else {
				if ((u_int32_t)-set->action.relative >
				    state->aspath.med)
					state->aspath.med = 0;
				else
					state->aspath.med +=
					    set->action.relative;
			}
			break;
		case ACTION_SET_WEIGHT:
			state->aspath.weight = set->action.metric;
			break;
		case ACTION_SET_RELATIVE_WEIGHT:
			if (set->action.relative > 0) {
				if (set->action.relative + state->aspath.weight
				    < state->aspath.weight)
					state->aspath.weight = UINT_MAX;
				else
					state->aspath.weight +=
					    set->action.relative;
			} else {
				if ((u_int32_t)-set->action.relative >
				    state->aspath.weight)
					state->aspath.weight = 0;
				else
					state->aspath.weight +=
					    set->action.relative;
			}
			break;
		case ACTION_SET_PREPEND_SELF:
			prep_as = peer->conf.local_as;
			prepend = set->action.prepend;
			np = aspath_prepend(state->aspath.aspath, prep_as,
			    prepend, &nl);
			aspath_put(state->aspath.aspath);
			state->aspath.aspath = aspath_get(np, nl);
			free(np);
			break;
		case ACTION_SET_PREPEND_PEER:
			if (from == NULL)
				break;
			prep_as = from->conf.remote_as;
			prepend = set->action.prepend;
			np = aspath_prepend(state->aspath.aspath, prep_as,
			    prepend, &nl);
			aspath_put(state->aspath.aspath);
			state->aspath.aspath = aspath_get(np, nl);
			free(np);
			break;
		case ACTION_SET_NEXTHOP:
		case ACTION_SET_NEXTHOP_REJECT:
		case ACTION_SET_NEXTHOP_BLACKHOLE:
		case ACTION_SET_NEXTHOP_NOMODIFY:
		case ACTION_SET_NEXTHOP_SELF:
			nexthop_modify(set->action.nh, set->type, aid,
			    &state->nexthop, &state->nhflags);
			break;
		case ACTION_SET_COMMUNITY:
			switch (set->action.community.type) {
			case COMMUNITY_TYPE_BASIC:
				community_set(&state->aspath,
				    &set->action.community, peer);
				break;
			case COMMUNITY_TYPE_LARGE:
				community_large_set(&state->aspath,
				    &set->action.community, peer);
				break;
			case COMMUNITY_TYPE_EXT:
				community_ext_set(&state->aspath,
				    &set->action.community, peer);
				break;
			}
			break;
		case ACTION_DEL_COMMUNITY:
			switch (set->action.community.type) {
			case COMMUNITY_TYPE_BASIC:
				community_delete(&state->aspath,
				    &set->action.community, peer);
				break;
			case COMMUNITY_TYPE_LARGE:
				community_large_delete(&state->aspath,
				    &set->action.community, peer);
				break;
			case COMMUNITY_TYPE_EXT:
				community_ext_delete(&state->aspath,
				    &set->action.community, peer);
			}
			break;
		case ACTION_PFTABLE:
			/* convert pftable name to an id */
			set->action.id = pftable_name2id(set->action.pftable);
			set->type = ACTION_PFTABLE_ID;
			/* FALLTHROUGH */
		case ACTION_PFTABLE_ID:
			pftable_unref(state->aspath.pftableid);
			state->aspath.pftableid = pftable_ref(set->action.id);
			break;
		case ACTION_RTLABEL:
			/* convert the route label to an id for faster access */
			set->action.id = rtlabel_name2id(set->action.rtlabel);
			set->type = ACTION_RTLABEL_ID;
			/* FALLTHROUGH */
		case ACTION_RTLABEL_ID:
			rtlabel_unref(state->aspath.rtlabelid);
			state->aspath.rtlabelid = rtlabel_ref(set->action.id);
			break;
		case ACTION_SET_ORIGIN:
			state->aspath.origin = set->action.origin;
			break;
		}
	}
}

int
rde_filter_match(struct filter_rule *f, struct rde_peer *peer,
    struct filterstate *state, struct prefix *p)
{
	struct rde_aspath *asp = NULL;
	int i;

	if (state != NULL)
		asp = &state->aspath;

	if (f->peer.ebgp && !peer->conf.ebgp)
		return (0);
	if (f->peer.ibgp && peer->conf.ebgp)
		return (0);

	if (f->match.ovs.is_set) {
		if (prefix_vstate(p) != f->match.ovs.validity)
			return (0);
	}

	if (asp != NULL && f->match.as.type != AS_UNDEF) {
		if (aspath_match(asp->aspath, &f->match.as,
		    peer->conf.remote_as) == 0)
			return (0);
	}

	if (asp != NULL && f->match.aslen.type != ASLEN_NONE)
		if (aspath_lenmatch(asp->aspath, f->match.aslen.type,
		    f->match.aslen.aslen) == 0)
			return (0);

	for (i = 0; asp != NULL && i < MAX_COMM_MATCH; i++) {
		switch (f->match.community[i].type) {
		case COMMUNITY_TYPE_NONE:
			i = MAX_COMM_MATCH;
			break;
		case COMMUNITY_TYPE_BASIC:
			if (community_match(asp, &f->match.community[i],
			    peer) == 0)
				return (0);
			break;
		case COMMUNITY_TYPE_LARGE:
			if (community_large_match(asp, &f->match.community[i],
			    peer) == 0)
				return (0);
			break;
		case COMMUNITY_TYPE_EXT:
			if (community_ext_match(asp, &f->match.community[i],
			    peer) == 0)
				return (0);
		}
	}

	if (state != NULL && f->match.nexthop.flags != 0) {
		struct bgpd_addr *nexthop, *cmpaddr;
		if (state->nexthop == NULL)
			/* no nexthop, skip */
			return (0);
		nexthop = &state->nexthop->exit_nexthop;
		if (f->match.nexthop.flags == FILTER_NEXTHOP_ADDR)
			cmpaddr = &f->match.nexthop.addr;
		else
			cmpaddr = &prefix_peer(p)->remote_addr;
		if (cmpaddr->aid != nexthop->aid)
			/* don't use IPv4 rules for IPv6 and vice versa */
			return (0);

		switch (cmpaddr->aid) {
		case AID_INET:
			if (cmpaddr->v4.s_addr != nexthop->v4.s_addr)
				return (0);
			break;
		case AID_INET6:
			if (memcmp(&cmpaddr->v6, &nexthop->v6,
			    sizeof(struct in6_addr)))
				return (0);
			break;
		default:
			fatalx("King Bula lost in address space");
		}
	}

	/* origin-set lookups match only on ROA_VALID */
	if (asp != NULL && f->match.originset.ps != NULL) {
		struct bgpd_addr addr, *prefix = &addr;
		u_int8_t plen;

		pt_getaddr(p->re->prefix, prefix);
		plen = p->re->prefix->prefixlen;
		if (trie_roa_check(&f->match.originset.ps->th, prefix, plen,
		    aspath_origin(asp->aspath)) != ROA_VALID)
			return (0);
	}

	/*
	 * prefixset and prefix filter rules are mutual exclusive
	 */
	if (f->match.prefixset.flags != 0) {
		struct bgpd_addr addr, *prefix = &addr;
		u_int8_t plen;

		pt_getaddr(p->re->prefix, prefix);
		plen = p->re->prefix->prefixlen;
		if (f->match.prefixset.ps == NULL ||
		    !trie_match(&f->match.prefixset.ps->th, prefix, plen,
		    (f->match.prefixset.flags & PREFIXSET_FLAG_LONGER)))
			return (0);
	} else if (f->match.prefix.addr.aid != 0)
		return (rde_prefix_match(&f->match.prefix, p));

	/* matched somewhen or is anymatch rule  */
	return (1);
}

/* return 1 when prefix matches filter_prefix, 0 if not */
int
rde_prefix_match(struct filter_prefix *fp, struct prefix *p)
{
	struct bgpd_addr addr, *prefix = &addr;
	u_int8_t plen;

	pt_getaddr(p->re->prefix, prefix);
	plen = p->re->prefix->prefixlen;

	if (fp->addr.aid != prefix->aid)
		/* don't use IPv4 rules for IPv6 and vice versa */
		return (0);

	if (prefix_compare(prefix, &fp->addr, fp->len))
		return (0);

	/* test prefixlen stuff too */
	switch (fp->op) {
	case OP_NONE: /* perfect match */
		return (plen == fp->len);
	case OP_EQ:
		return (plen == fp->len_min);
	case OP_NE:
		return (plen != fp->len_min);
	case OP_RANGE:
		return ((plen >= fp->len_min) &&
		    (plen <= fp->len_max));
	case OP_XRANGE:
		return ((plen < fp->len_min) ||
		    (plen > fp->len_max));
	default:
		log_warnx("%s: unsupported prefix operation", __func__);
		return (0);
	}
}

/* return true when the rule f can never match for this peer */
static int
rde_filter_skip_rule(struct rde_peer *peer, struct filter_rule *f)
{
	/* if any of the two is unset then rule can't be skipped */
	if (peer == NULL || f == NULL)
		return (0);

	if (f->peer.groupid != 0 &&
	    f->peer.groupid != peer->conf.groupid)
		return (1);

	if (f->peer.peerid != 0 &&
	    f->peer.peerid != peer->conf.id)
		return (1);

	if (f->peer.remote_as != 0 &&
	    f->peer.remote_as != peer->conf.remote_as)
		return (1);

	if (f->peer.ebgp != 0 &&
	    f->peer.ebgp != peer->conf.ebgp)
		return (1);

	if (f->peer.ibgp != 0 &&
	    f->peer.ibgp != !peer->conf.ebgp)
		return (1);

	return (0);
}

int
rde_filter_equal(struct filter_head *a, struct filter_head *b,
    struct rde_peer *peer)
{
	struct filter_rule	*fa, *fb;
	struct rde_prefixset	*psa, *psb, *osa, *osb;
	struct as_set		*asa, *asb;
	int			 r;

	fa = a ? TAILQ_FIRST(a) : NULL;
	fb = b ? TAILQ_FIRST(b) : NULL;

	while (fa != NULL || fb != NULL) {
		/* skip all rules with wrong peer */
		if (rde_filter_skip_rule(peer, fa)) {
			fa = TAILQ_NEXT(fa, entry);
			continue;
		}
		if (rde_filter_skip_rule(peer, fb)) {
			fb = TAILQ_NEXT(fb, entry);
			continue;
		}

		/* compare the two rules */
		if ((fa == NULL && fb != NULL) || (fa != NULL && fb == NULL))
			/* new rule added or removed */
			return (0);

		if (fa->action != fb->action || fa->quick != fb->quick)
			return (0);
		if (memcmp(&fa->peer, &fb->peer, sizeof(fa->peer)))
			return (0);

		/* compare filter_rule.match without the prefixset pointer */
		psa = fa->match.prefixset.ps;
		psb = fb->match.prefixset.ps;
		osa = fa->match.originset.ps;
		osb = fb->match.originset.ps;
		asa = fa->match.as.aset;
		asb = fb->match.as.aset;
		fa->match.prefixset.ps = fb->match.prefixset.ps = NULL;
		fa->match.originset.ps = fb->match.originset.ps = NULL;
		fa->match.as.aset = fb->match.as.aset = NULL;
		r = memcmp(&fa->match, &fb->match, sizeof(fa->match));
		/* fixup the struct again */
		fa->match.prefixset.ps = psa;
		fb->match.prefixset.ps = psb;
		fa->match.originset.ps = osa;
		fb->match.originset.ps = osb;
		fa->match.as.aset = asa;
		fb->match.as.aset = asb;
		if (r != 0)
			return (0);
		if (fa->match.prefixset.ps != NULL &&
		    fa->match.prefixset.ps->dirty) {
			log_debug("%s: prefixset %s has changed",
			    __func__, fa->match.prefixset.name);
			return (0);
		}
		if (fa->match.originset.ps != NULL &&
		    fa->match.originset.ps->dirty) {
			log_debug("%s: originset %s has changed",
			    __func__, fa->match.originset.name);
			return (0);
		}
		if ((fa->match.as.flags & AS_FLAG_AS_SET) &&
		    fa->match.as.aset->dirty) {
			log_debug("%s: as-set %s has changed",
			    __func__, fa->match.as.name);
			return (0);
		}

		if (!filterset_equal(&fa->set, &fb->set))
			return (0);

		fa = TAILQ_NEXT(fa, entry);
		fb = TAILQ_NEXT(fb, entry);
	}
	return (1);
}

void
rde_filterstate_prep(struct filterstate *state, struct rde_aspath *asp,
    struct nexthop *nh, u_int8_t nhflags)
{
	memset(state, 0, sizeof(*state));

	path_prep(&state->aspath);
	if (asp)
		path_copy(&state->aspath, asp);
	state->nexthop = nexthop_ref(nh);
	state->nhflags = nhflags;
}

void
rde_filterstate_clean(struct filterstate *state)
{
	path_clean(&state->aspath);
	nexthop_put(state->nexthop);
	state->nexthop = NULL;
}

void
filterlist_free(struct filter_head *fh)
{
	struct filter_rule	*r;

	if (fh == NULL)
		return;

	while ((r = TAILQ_FIRST(fh)) != NULL) {
		TAILQ_REMOVE(fh, r, entry);
		filterset_free(&r->set);
		free(r);
	}
	free(fh);
}

/* free a filterset and take care of possible name2id references */
void
filterset_free(struct filter_set_head *sh)
{
	struct filter_set	*s;

	if (sh == NULL)
		return;

	while ((s = TAILQ_FIRST(sh)) != NULL) {
		TAILQ_REMOVE(sh, s, entry);
		if (s->type == ACTION_RTLABEL_ID)
			rtlabel_unref(s->action.id);
		else if (s->type == ACTION_PFTABLE_ID)
			pftable_unref(s->action.id);
		else if (s->type == ACTION_SET_NEXTHOP &&
		    bgpd_process == PROC_RDE)
			nexthop_put(s->action.nh);
		free(s);
	}
}

/*
 * this function is a bit more complicated than a memcmp() because there are
 * types that need to be considered equal e.g. ACTION_SET_MED and
 * ACTION_SET_RELATIVE_MED. Also ACTION_SET_COMMUNITY and ACTION_SET_NEXTHOP
 * need some special care. It only checks the types and not the values so
 * it does not do a real compare.
 */
int
filterset_cmp(struct filter_set *a, struct filter_set *b)
{
	if (strcmp(filterset_name(a->type), filterset_name(b->type)))
		return (a->type - b->type);

	if (a->type == ACTION_SET_COMMUNITY ||
	    a->type == ACTION_DEL_COMMUNITY) {	/* a->type == b->type */
		return (memcmp(&a->action.community, &b->action.community,
		    sizeof(a->action.community)));
	}

	if (a->type == ACTION_SET_NEXTHOP && b->type == ACTION_SET_NEXTHOP) {
		/*
		 * This is the only interesting case, all others are considered
		 * equal. It does not make sense to e.g. set a nexthop and
		 * reject it at the same time. Allow one IPv4 and one IPv6
		 * per filter set or only one of the other nexthop modifiers.
		 */
		return (a->action.nexthop.aid - b->action.nexthop.aid);
	}

	/* equal */
	return (0);
}

void
filterset_move(struct filter_set_head *source, struct filter_set_head *dest)
{
	TAILQ_INIT(dest);
	if (source == NULL)
		return;
	TAILQ_CONCAT(dest, source, entry);
}

int
filterset_equal(struct filter_set_head *ah, struct filter_set_head *bh)
{
	struct filter_set	*a, *b;
	const char		*as, *bs;

	for (a = TAILQ_FIRST(ah), b = TAILQ_FIRST(bh);
	    a != NULL && b != NULL;
	    a = TAILQ_NEXT(a, entry), b = TAILQ_NEXT(b, entry)) {
		switch (a->type) {
		case ACTION_SET_PREPEND_SELF:
		case ACTION_SET_PREPEND_PEER:
			if (a->type == b->type &&
			    a->action.prepend == b->action.prepend)
				continue;
			break;
		case ACTION_SET_LOCALPREF:
		case ACTION_SET_MED:
		case ACTION_SET_WEIGHT:
			if (a->type == b->type &&
			    a->action.metric == b->action.metric)
				continue;
			break;
		case ACTION_SET_RELATIVE_LOCALPREF:
		case ACTION_SET_RELATIVE_MED:
		case ACTION_SET_RELATIVE_WEIGHT:
			if (a->type == b->type &&
			    a->action.relative == b->action.relative)
				continue;
			break;
		case ACTION_SET_NEXTHOP:
			if (a->type == b->type &&
			    memcmp(&a->action.nexthop, &b->action.nexthop,
			    sizeof(a->action.nexthop)) == 0)
				continue;
			break;
		case ACTION_SET_NEXTHOP_BLACKHOLE:
		case ACTION_SET_NEXTHOP_REJECT:
		case ACTION_SET_NEXTHOP_NOMODIFY:
		case ACTION_SET_NEXTHOP_SELF:
			if (a->type == b->type)
				continue;
			break;
		case ACTION_DEL_COMMUNITY:
		case ACTION_SET_COMMUNITY:
			if (a->type == b->type &&
			    memcmp(&a->action.community, &b->action.community,
			    sizeof(a->action.community)) == 0)
				continue;
			break;
		case ACTION_PFTABLE:
		case ACTION_PFTABLE_ID:
			if (b->type == ACTION_PFTABLE)
				bs = b->action.pftable;
			else if (b->type == ACTION_PFTABLE_ID)
				bs = pftable_id2name(b->action.id);
			else
				break;

			if (a->type == ACTION_PFTABLE)
				as = a->action.pftable;
			else
				as = pftable_id2name(a->action.id);

			if (strcmp(as, bs) == 0)
				continue;
			break;
		case ACTION_RTLABEL:
		case ACTION_RTLABEL_ID:
			if (b->type == ACTION_RTLABEL)
				bs = b->action.rtlabel;
			else if (b->type == ACTION_RTLABEL_ID)
				bs = rtlabel_id2name(b->action.id);
			else
				break;

			if (a->type == ACTION_RTLABEL)
				as = a->action.rtlabel;
			else
				as = rtlabel_id2name(a->action.id);

			if (strcmp(as, bs) == 0)
				continue;
			break;
		case ACTION_SET_ORIGIN:
			if (a->type == b->type &&
			    a->action.origin == b->action.origin)
				continue;
			break;
		}
		/* compare failed */
		return (0);
	}
	if (a != NULL || b != NULL)
		return (0);
	return (1);
}

const char *
filterset_name(enum action_types type)
{
	switch (type) {
	case ACTION_SET_LOCALPREF:
	case ACTION_SET_RELATIVE_LOCALPREF:
		return ("localpref");
	case ACTION_SET_MED:
	case ACTION_SET_RELATIVE_MED:
		return ("metric");
	case ACTION_SET_WEIGHT:
	case ACTION_SET_RELATIVE_WEIGHT:
		return ("weight");
	case ACTION_SET_PREPEND_SELF:
		return ("prepend-self");
	case ACTION_SET_PREPEND_PEER:
		return ("prepend-peer");
	case ACTION_SET_NEXTHOP:
	case ACTION_SET_NEXTHOP_REJECT:
	case ACTION_SET_NEXTHOP_BLACKHOLE:
	case ACTION_SET_NEXTHOP_NOMODIFY:
	case ACTION_SET_NEXTHOP_SELF:
		return ("nexthop");
	case ACTION_SET_COMMUNITY:
		return ("community");
	case ACTION_DEL_COMMUNITY:
		return ("community delete");
	case ACTION_PFTABLE:
	case ACTION_PFTABLE_ID:
		return ("pftable");
	case ACTION_RTLABEL:
	case ACTION_RTLABEL_ID:
		return ("rtlabel");
	case ACTION_SET_ORIGIN:
		return ("origin");
	}

	fatalx("filterset_name: got lost");
}

/*
 * Copyright (c) 2001 Daniel Hartmeier
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    - Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    - Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Effort sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F30602-01-2-0537.
 *
 */

#define RDE_FILTER_SET_SKIP_STEPS(i)				\
	do {							\
		while (head[i] != cur) {			\
			head[i]->skip[i] = cur;			\
			head[i] = TAILQ_NEXT(head[i], entry);	\
		}						\
	} while (0)

void
rde_filter_calc_skip_steps(struct filter_head *rules)
{
	struct filter_rule *cur, *prev, *head[RDE_FILTER_SKIP_COUNT];
	int i;

	if (rules == NULL)
		return;

	cur = TAILQ_FIRST(rules);

	prev = cur;
	for (i = 0; i < RDE_FILTER_SKIP_COUNT; ++i)
		head[i] = cur;
	while (cur != NULL) {
		if (cur->peer.groupid != prev->peer.groupid)
			RDE_FILTER_SET_SKIP_STEPS(RDE_FILTER_SKIP_GROUPID);
		if (cur->peer.remote_as != prev->peer.remote_as)
			RDE_FILTER_SET_SKIP_STEPS(RDE_FILTER_SKIP_REMOTE_AS);
		if (cur->peer.peerid != prev->peer.peerid)
			RDE_FILTER_SET_SKIP_STEPS(RDE_FILTER_SKIP_PEERID);
		prev = cur;
		cur = TAILQ_NEXT(cur, entry);
	}
	for (i = 0; i < RDE_FILTER_SKIP_COUNT; ++i)
		RDE_FILTER_SET_SKIP_STEPS(i);

}

#define RDE_FILTER_TEST_ATTRIB(t, a)				\
	do {							\
		if (t) {					\
			f = a;					\
			goto nextrule;				\
		}						\
	} while (0)

enum filter_actions
rde_filter(struct filter_head *rules, struct rde_peer *peer,
    struct prefix *p, struct filterstate *state)
{
	struct filter_rule	*f;
	enum filter_actions	 action = ACTION_DENY; /* default deny */

	if (state == NULL) /* withdraw should be accepted by default */
		action = ACTION_ALLOW;

	if (state && state->aspath.flags & F_ATTR_PARSE_ERR)
		/*
		 * don't try to filter bad updates just deny them
		 * so they act as implicit withdraws
		 */
		return (ACTION_DENY);

	if (rules == NULL)
		return (action);

	f = TAILQ_FIRST(rules);
	while (f != NULL) {
		RDE_FILTER_TEST_ATTRIB(
		    (f->peer.groupid &&
		     f->peer.groupid != peer->conf.groupid),
		     f->skip[RDE_FILTER_SKIP_GROUPID]);
		RDE_FILTER_TEST_ATTRIB(
		    (f->peer.remote_as &&
		     f->peer.remote_as != peer->conf.remote_as),
		     f->skip[RDE_FILTER_SKIP_REMOTE_AS]);
		RDE_FILTER_TEST_ATTRIB(
		    (f->peer.peerid &&
		     f->peer.peerid != peer->conf.id),
		     f->skip[RDE_FILTER_SKIP_PEERID]);

		if (rde_filter_match(f, peer, state, p)) {
			if (state != NULL) {
				rde_apply_set(&f->set, state,
				    p->re->prefix->aid, prefix_peer(p), peer);
			}
			if (f->action != ACTION_NONE)
				action = f->action;
			if (f->quick)
				return (action);
		}
		f = TAILQ_NEXT(f, entry);
 nextrule: ;
	}
	return (action);
}
