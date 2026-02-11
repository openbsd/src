/*	$OpenBSD: rde_filter.c,v 1.143 2026/02/11 10:24:57 claudio Exp $ */

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

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <siphash.h>

#include "bgpd.h"
#include "rde.h"
#include "log.h"
#include "chash.h"

static int	rde_filterset_equal(const struct rde_filter_set *,
	    const struct rde_filter_set *);
static void	rde_filterset_ref(struct rde_filter_set *);

struct rde_filter_set_elm {
	enum action_types	type;
	union {
		uint8_t				 prepend;
		uint8_t				 origin;
		uint16_t			 id;
		uint32_t			 metric;
		int32_t				 relative;
		struct nexthop			*nh_ref;
		struct community		 community;
	}			action;
};

struct rde_filter_set {
	uint64_t			hash;
	size_t				len;
	int				refcnt;
	struct rde_filter_set_elm	set[0];
};

void
rde_apply_set(const struct rde_filter_set *rfs, struct rde_peer *peer,
    struct rde_peer *from, struct filterstate *state, uint8_t aid)
{
	u_char			*np;
	size_t			 i;
	uint32_t		 prep_as;
	uint16_t		 nl;
	uint8_t			 prepend;

	if (rfs == NULL)
		return;
	for (i = 0; i < rfs->len; i++) {
		const struct rde_filter_set_elm *set = &rfs->set[i];

		switch (set->type) {
		case ACTION_SET_LOCALPREF:
			state->aspath.lpref = set->action.metric;
			break;
		case ACTION_SET_RELATIVE_LOCALPREF:
			if (set->action.relative > 0) {
				if (state->aspath.lpref >
				    UINT_MAX - set->action.relative)
					state->aspath.lpref = UINT_MAX;
				else
					state->aspath.lpref +=
					    set->action.relative;
			} else {
				if (state->aspath.lpref <
				    0U - set->action.relative)
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
				if (state->aspath.med >
				    UINT_MAX - set->action.relative)
					state->aspath.med = UINT_MAX;
				else
					state->aspath.med +=
					    set->action.relative;
			} else {
				if (state->aspath.med <
				    0U - set->action.relative)
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
				if (state->aspath.weight >
				    UINT_MAX - set->action.relative)
					state->aspath.weight = UINT_MAX;
				else
					state->aspath.weight +=
					    set->action.relative;
			} else {
				if (state->aspath.weight <
				    0U - set->action.relative)
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
		case ACTION_SET_AS_OVERRIDE:
			if (from == NULL)
				break;
			np = aspath_override(state->aspath.aspath,
			    from->conf.remote_as, from->conf.local_as, &nl);
			aspath_put(state->aspath.aspath);
			state->aspath.aspath = aspath_get(np, nl);
			free(np);
			break;
		case ACTION_SET_NEXTHOP:
		case ACTION_SET_NEXTHOP_REJECT:
		case ACTION_SET_NEXTHOP_BLACKHOLE:
		case ACTION_SET_NEXTHOP_NOMODIFY:
		case ACTION_SET_NEXTHOP_SELF:
			nexthop_modify(set->action.nh_ref, set->type, aid,
			    &state->nexthop, &state->nhflags);
			break;
		case ACTION_SET_COMMUNITY:
			community_set(&state->communities,
			    &set->action.community, peer);
			break;
		case ACTION_DEL_COMMUNITY:
			community_delete(&state->communities,
			    &set->action.community, peer);
			break;
		case ACTION_PFTABLE:
			pftable_unref(state->aspath.pftableid);
			state->aspath.pftableid = pftable_ref(set->action.id);
			break;
		case ACTION_RTLABEL:
			rtlabel_unref(state->aspath.rtlabelid);
			state->aspath.rtlabelid = rtlabel_ref(set->action.id);
			break;
		case ACTION_SET_ORIGIN:
			state->aspath.origin = set->action.origin;
			break;
		}
	}
}

/* use to match the import filters for vpn imports */
int
rde_l3vpn_import(struct rde_community *comm, struct l3vpn *rd)
{
	size_t i;

	if (rd->rde_import == NULL)
		return (0);
	for (i = 0; i < rd->rde_import->len; i++) {
		struct rde_filter_set_elm *s = &rd->rde_import->set[i];
		if (community_match(comm, &s->action.community, 0))
			return (1);
	}
	return (0);
}

/* return 1 when prefix matches filter_prefix, 0 if not */
static int
rde_prefix_match(struct filter_prefix *fp, struct bgpd_addr *prefix,
    uint8_t plen)
{
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

static int
rde_filter_match(struct filter_match *match, struct rde_peer *peer,
    struct rde_peer *from, struct filterstate *state,
    struct bgpd_addr *prefix, uint8_t plen)
{
	struct rde_aspath *asp = &state->aspath;
	int i;

	if (match->ovs.is_set) {
		if ((state->vstate & ROA_MASK) != match->ovs.validity)
			return (0);
	}

	if (match->avs.is_set) {
		if (((state->vstate >> 4) & ASPA_MASK) != match->avs.validity)
			return (0);
	}

	if (asp != NULL && match->as.type != AS_UNDEF) {
		if (aspath_match(asp->aspath, &match->as,
		    peer->conf.remote_as) == 0)
			return (0);
	}

	if (asp != NULL && match->aslen.type != ASLEN_NONE)
		if (aspath_lenmatch(asp->aspath, match->aslen.type,
		    match->aslen.aslen) == 0)
			return (0);

	for (i = 0; i < MAX_COMM_MATCH; i++) {
		if (match->community[i].flags == 0)
			break;
		if (community_match(&state->communities,
		    &match->community[i], peer) == 0)
			return (0);
	}

	if (match->maxcomm != 0) {
		if (match->maxcomm >
		    community_count(&state->communities, COMMUNITY_TYPE_BASIC))
			return (0);
	}
	if (match->maxextcomm != 0) {
		if (match->maxextcomm >
		    community_count(&state->communities, COMMUNITY_TYPE_EXT))
			return (0);
	}
	if (match->maxlargecomm != 0) {
		if (match->maxlargecomm >
		    community_count(&state->communities, COMMUNITY_TYPE_LARGE))
			return (0);
	}

	if (match->nexthop.flags != 0) {
		struct bgpd_addr *nexthop, *cmpaddr;
		if (state->nexthop == NULL)
			/* no nexthop, skip */
			return (0);
		nexthop = &state->nexthop->exit_nexthop;
		if (match->nexthop.flags == FILTER_NEXTHOP_ADDR)
			cmpaddr = &match->nexthop.addr;
		else
			cmpaddr = &from->remote_addr;
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
	if (asp != NULL && match->originset.ps != NULL) {
		if (trie_roa_check(&match->originset.ps->th, prefix, plen,
		    aspath_origin(asp->aspath)) != ROA_VALID)
			return (0);
	}

	/*
	 * prefixset and prefix filter rules are mutual exclusive
	 */
	if (match->prefixset.flags != 0) {
		if (match->prefixset.ps == NULL ||
		    !trie_match(&match->prefixset.ps->th, prefix, plen,
		    (match->prefixset.flags & PREFIXSET_FLAG_LONGER)))
			return (0);
	} else if (match->prefix.addr.aid != 0)
		return (rde_prefix_match(&match->prefix, prefix, plen));

	/* matched somewhen or is anymatch rule  */
	return (1);
}

/* return true when the rule f can never match for this peer */
int
rde_filter_skip_rule(struct rde_peer *peer, struct filter_rule *f)
{
	/* if any of the two is unset then rule can't be skipped */
	if (peer == NULL || f == NULL)
		return (0);

	if (f->peer.peerid != 0 &&
	    f->peer.peerid != peer->conf.id)
		return (1);

	if (f->peer.groupid != 0 &&
	    f->peer.groupid != peer->conf.groupid)
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
rde_filter_equal(struct filter_head *a, struct filter_head *b)
{
	struct filter_rule	*fa, *fb;
	struct rde_prefixset	*psa, *psb, *osa, *osb;
	struct as_set		*asa, *asb;
	int			 r;

	fa = a ? TAILQ_FIRST(a) : NULL;
	fb = b ? TAILQ_FIRST(b) : NULL;

	while (fa != NULL || fb != NULL) {
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

		if (!rde_filterset_equal(fa->rde_set, fb->rde_set))
			return (0);

		fa = TAILQ_NEXT(fa, entry);
		fb = TAILQ_NEXT(fb, entry);
	}
	return (1);
}

struct filter_rule *
rde_filter_dup(const struct filter_rule *fr)
{
	struct filter_rule *new;

	if ((new = malloc(sizeof(*new))) == NULL)
		fatal(NULL);
	*new = *fr;
	/* XXX think about skip table */
	rde_filterset_ref(new->rde_set);
	return new;
}

void
rde_filterstate_init(struct filterstate *state)
{
	memset(state, 0, sizeof(*state));
	path_prep(&state->aspath);
}

static void
rde_filterstate_set(struct filterstate *state, struct rde_aspath *asp,
    struct rde_community *communities, struct nexthop *nh, uint8_t nhflags,
    uint8_t vstate)
{
	rde_filterstate_init(state);

	if (asp)
		path_copy(&state->aspath, asp);
	if (communities)
		communities_copy(&state->communities, communities);
	state->nexthop = nexthop_ref(nh);
	state->nhflags = nhflags;
	state->vstate = vstate;
}

/*
 * Build a filterstate based on the prefix p.
 */
void
rde_filterstate_prep(struct filterstate *state, struct prefix *p)
{
	rde_filterstate_set(state, prefix_aspath(p), prefix_communities(p),
	    prefix_nexthop(p), prefix_nhflags(p), p->validation_state);
}

/*
 * Copy a filterstate to a new filterstate.
 */
void
rde_filterstate_copy(struct filterstate *state, struct filterstate *src)
{
	rde_filterstate_set(state, &src->aspath, &src->communities,
	    src->nexthop, src->nhflags, src->vstate);
}

/*
 * Set the vstate based on the aspa_state and the supplied roa vstate.
 * This function must be called after rde_filterstate_init().
 * rde_filterstate_prep() and rde_filterstate_copy() set the right vstate.
 */
void
rde_filterstate_set_vstate(struct filterstate *state, uint8_t roa_vstate,
    uint8_t aspa_state)
{
	state->vstate = aspa_state << 4;
	state->vstate |= roa_vstate & ROA_MASK;
}

void
rde_filterstate_clean(struct filterstate *state)
{
	path_clean(&state->aspath);
	communities_clean(&state->communities);
	nexthop_unref(state->nexthop);
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
		if (r->rde_set != NULL)
			rde_filterset_unref(r->rde_set);
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
		free(s);
	}
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
	case ACTION_SET_AS_OVERRIDE:
		return ("as-override");
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
		return ("pftable");
	case ACTION_RTLABEL:
		return ("rtlabel");
	case ACTION_SET_ORIGIN:
		return ("origin");
	}

	fatalx("filterset_name: got lost");
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

/*
 * move filterset from source to dest. dest will be initialized first.
 * After the move source is an empty list.
 */
void
filterset_move(struct filter_set_head *source, struct filter_set_head *dest)
{
	TAILQ_INIT(dest);
	if (source == NULL)
		return;
	TAILQ_CONCAT(dest, source, entry);
}

/*
 * copy filterset from source to dest. dest will be initialized first.
 */
void
filterset_copy(const struct filter_set_head *source,
    struct filter_set_head *dest)
{
	struct filter_set	*s, *t;

	TAILQ_INIT(dest);
	if (source == NULL)
		return;

	TAILQ_FOREACH(s, source, entry) {
		if ((t = malloc(sizeof(struct filter_set))) == NULL)
			fatal(NULL);
		*t = *s;
		TAILQ_INSERT_TAIL(dest, t, entry);
	}
}

static int
rde_filterset_equal(const struct rde_filter_set *afs,
    const struct rde_filter_set *bfs)
{
	const struct rde_filter_set_elm *a, *b;
	size_t i;

	if (afs->len != bfs->len)
		return 0;

	a = afs->set;
	b = bfs->set;
	for (i = 0; i < afs->len; i++, a++, b++) {
		if (a->type != b->type)
			return 0;

		switch (a->type) {
		case ACTION_SET_PREPEND_SELF:
		case ACTION_SET_PREPEND_PEER:
			if (a->action.prepend == b->action.prepend)
				continue;
			break;
		case ACTION_SET_AS_OVERRIDE:
			continue;
		case ACTION_SET_LOCALPREF:
		case ACTION_SET_MED:
		case ACTION_SET_WEIGHT:
			if (a->action.metric == b->action.metric)
				continue;
			break;
		case ACTION_SET_RELATIVE_LOCALPREF:
		case ACTION_SET_RELATIVE_MED:
		case ACTION_SET_RELATIVE_WEIGHT:
			if (a->action.relative == b->action.relative)
				continue;
			break;
		case ACTION_SET_NEXTHOP:
			if (a->action.nh_ref == b->action.nh_ref)
				continue;
			break;
		case ACTION_SET_NEXTHOP_BLACKHOLE:
		case ACTION_SET_NEXTHOP_REJECT:
		case ACTION_SET_NEXTHOP_NOMODIFY:
		case ACTION_SET_NEXTHOP_SELF:
			continue;
		case ACTION_DEL_COMMUNITY:
		case ACTION_SET_COMMUNITY:
			if (memcmp(&a->action.community, &b->action.community,
			    sizeof(a->action.community)) == 0)
				continue;
			break;
		case ACTION_RTLABEL:
		case ACTION_PFTABLE:
			if (a->action.id == b->action.id)
				continue;
			break;
		case ACTION_SET_ORIGIN:
			if (a->action.origin == b->action.origin)
				continue;
			break;
		}
		/* compare failed */
		return 0;
	}
	return 1;
}

static SIPHASH_KEY	fskey;

static inline uint64_t
rde_filterset_hash(const struct rde_filter_set *rfs)
{
	return rfs->hash;
}

uint64_t
rde_filterset_calc_hash(const struct rde_filter_set *rfs)
{
	return SipHash24(&fskey, rfs->set, rfs->len * sizeof(*rfs->set));
}

CH_HEAD(rde_filterset, rde_filter_set);
CH_PROTOTYPE(rde_filterset, rde_filter_set, rde_filterset_hash);

static struct rde_filterset filterset = CH_INITIALIZER(&filterset);

static void
rde_filterset_free(struct rde_filter_set *rfs)
{
	struct rde_filter_set_elm *rfse;
	size_t i;

	if (rfs == NULL)
		return;

	rdemem.filter_set_size -= sizeof(*rfs) + rfs->len * sizeof(*rfse);
	rdemem.filter_set_cnt--;

	rfse = rfs->set;
	for (i = 0; i < rfs->len; i++, rfse++) {
		if (rfse->type == ACTION_RTLABEL)
			rtlabel_unref(rfse->action.id);
		else if (rfse->type == ACTION_PFTABLE)
			pftable_unref(rfse->action.id);
		else if (rfse->type == ACTION_SET_NEXTHOP)
			nexthop_unref(rfse->action.nh_ref);
	}
	free(rfs);
}

static void
rde_filterset_ref(struct rde_filter_set *rfs)
{
	rfs->refcnt++;
	rdemem.filter_set_refs++;
}

void
rde_filterset_unref(struct rde_filter_set *rfs)
{
	rfs->refcnt--;
	rdemem.filter_set_refs--;
	if (rfs->refcnt <= 0) {
		CH_REMOVE(rde_filterset, &filterset, rfs);
		rde_filterset_free(rfs);
	}
}

static void
rde_filterset_conv(const struct filter_set *set,
    struct rde_filter_set_elm *rfse)
{
	rfse->type = set->type;
	switch (set->type) {
	case ACTION_SET_PREPEND_SELF:
	case ACTION_SET_PREPEND_PEER:
		rfse->action.prepend = set->action.prepend;
		break;
	case ACTION_SET_AS_OVERRIDE:
		break;
	case ACTION_SET_LOCALPREF:
	case ACTION_SET_MED:
	case ACTION_SET_WEIGHT:
		rfse->action.metric = set->action.metric;
		break;
	case ACTION_SET_RELATIVE_LOCALPREF:
	case ACTION_SET_RELATIVE_MED:
	case ACTION_SET_RELATIVE_WEIGHT:
		rfse->action.relative = set->action.relative;
		break;
	case ACTION_SET_NEXTHOP_BLACKHOLE:
	case ACTION_SET_NEXTHOP_REJECT:
	case ACTION_SET_NEXTHOP_NOMODIFY:
	case ACTION_SET_NEXTHOP_SELF:
		break;
	case ACTION_DEL_COMMUNITY:
	case ACTION_SET_COMMUNITY:
		rfse->action.community = set->action.community;
		break;
	case ACTION_SET_ORIGIN:
		rfse->action.origin = set->action.origin;
		break;
	case ACTION_SET_NEXTHOP:
		rfse->action.nh_ref = nexthop_get(&set->action.nexthop);
		break;
	case ACTION_RTLABEL:
		rfse->action.id = rtlabel_name2id(set->action.rtlabel);
		break;
	case ACTION_PFTABLE:
		rfse->action.id = pftable_name2id(set->action.pftable);
		break;
	}
}

struct rde_filter_set *
rde_filterset_imsg_recv(struct imsg *imsg)
{
	struct ibuf ibuf;
	struct rde_filter_set *rfs = NULL, *nrfs;
	struct rde_filter_set_elm *rfse;
	uint16_t count, i;

	if (imsg_get_ibuf(imsg, &ibuf) == -1)
		goto fail;

	if (ibuf_recv_filterset_count(&ibuf, &count) == -1)
		goto fail;

	if ((rfs = calloc(1, sizeof(*rfs) + count * sizeof(*rfse))) == NULL)
		goto fail;

	rdemem.filter_set_size += sizeof(*rfs) + count * sizeof(*rfse);
	rdemem.filter_set_cnt++;

	rfs->len = count;
	rfse = rfs->set;

	for (i = 0; i < count; i++, rfse++) {
		struct filter_set set;
		if (ibuf_recv_one_filterset(&ibuf, &set) == -1)
			goto fail;
		rde_filterset_conv(&set, rfse);
	}

	if (ibuf_size(&ibuf) != 0) {
		errno = EBADMSG;
		goto fail;
	}

	rfs->hash = rde_filterset_calc_hash(rfs);

	if ((nrfs = CH_FIND(rde_filterset, &filterset, rfs)) == NULL) {
		if (CH_INSERT(rde_filterset, &filterset, rfs, NULL) != 1)
			fatalx("%s: already present set", __func__);
	} else {
		rde_filterset_free(rfs);
		rfs = nrfs;
	}
	rde_filterset_ref(rfs);
	return rfs;

 fail:
	log_warn("filter set receive");
	rde_filterset_free(rfs);
	return NULL;
}

CH_GENERATE(rde_filterset, rde_filter_set, rde_filterset_equal,
    rde_filterset_hash);

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
		if (cur->peer.peerid != prev->peer.peerid)
			RDE_FILTER_SET_SKIP_STEPS(RDE_FILTER_SKIP_PEERID);
		if (cur->peer.groupid != prev->peer.groupid)
			RDE_FILTER_SET_SKIP_STEPS(RDE_FILTER_SKIP_GROUPID);
		if (cur->peer.remote_as != prev->peer.remote_as)
			RDE_FILTER_SET_SKIP_STEPS(RDE_FILTER_SKIP_REMOTE_AS);
		prev = cur;
		cur = TAILQ_NEXT(cur, entry);
	}
	for (i = 0; i < RDE_FILTER_SKIP_COUNT; ++i)
		RDE_FILTER_SET_SKIP_STEPS(i);

}

enum filter_actions
rde_filter(struct filter_head *rules, struct rde_peer *peer,
    struct rde_peer *from, struct bgpd_addr *prefix, uint8_t plen,
    struct filterstate *state)
{
	struct filter_rule	*f;
	enum filter_actions	 action = ACTION_DENY; /* default deny */

	if (state->aspath.flags & F_ATTR_PARSE_ERR)
		/*
		 * don't try to filter bad updates just deny them
		 * so they act as implicit withdraws
		 */
		return (ACTION_DENY);

	if (rules == NULL)
		return (action);

	if (prefix->aid == AID_FLOWSPECv4 || prefix->aid == AID_FLOWSPECv6)
		return (ACTION_ALLOW);

	f = TAILQ_FIRST(rules);
	while (f != NULL) {
		if (f->peer.peerid && f->peer.peerid != peer->conf.id) {
			f = f->skip[RDE_FILTER_SKIP_PEERID];
			continue;
		}
		if (f->peer.groupid && f->peer.groupid != peer->conf.groupid) {
			f = f->skip[RDE_FILTER_SKIP_GROUPID];
			continue;
		}
		if (f->peer.remote_as &&
		    f->peer.remote_as != peer->conf.remote_as) {
			f = f->skip[RDE_FILTER_SKIP_REMOTE_AS];
			continue;
		}
		if (f->peer.ebgp && !peer->conf.ebgp) {
			f = TAILQ_NEXT(f, entry);
			continue;
		}
			f = TAILQ_NEXT(f, entry);
			continue;
		}

		if (rde_filter_match(&f->match, peer, from, state,
		    prefix, plen)) {
			rde_apply_set(f->rde_set, peer, from, state,
			    prefix->aid);
			if (f->action != ACTION_NONE)
				action = f->action;
			if (f->quick)
				return (action);
		}
		f = TAILQ_NEXT(f, entry);
	}
	return (action);
}

enum filter_actions
rde_filter_out(struct filter_head *rules, struct rde_peer *peer,
    struct rde_peer *from, struct bgpd_addr *prefix, uint8_t plen,
    struct filterstate *state)
{
	struct filter_rule	*f;
	enum filter_actions	 action = ACTION_DENY; /* default deny */

	if (state->aspath.flags & F_ATTR_PARSE_ERR)
		/*
		 * don't try to filter bad updates just deny them
		 * so they act as implicit withdraws
		 */
		return (ACTION_DENY);

	if (rules == NULL)
		return (action);

	if (prefix->aid == AID_FLOWSPECv4 || prefix->aid == AID_FLOWSPECv6)
		return (ACTION_ALLOW);

	f = TAILQ_FIRST(rules);
	while (f != NULL) {
		if (rde_filter_match(&f->match, peer, from, state,
		    prefix, plen)) {
			rde_apply_set(f->rde_set, peer, from, state,
			    prefix->aid);
			if (f->action != ACTION_NONE)
				action = f->action;
			if (f->quick)
				return (action);
		}
		f = TAILQ_NEXT(f, entry);
	}
	return (action);
}
