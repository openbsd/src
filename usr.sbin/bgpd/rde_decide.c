/*	$OpenBSD: rde_decide.c,v 1.22 2004/01/17 19:35:36 claudio Exp $ */

/*
 * Copyright (c) 2003, 2004 Claudio Jeker <claudio@openbsd.org>
 * Copyright (c) 2003, 2004 Henning Brauer <henning@openbsd.org>
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
#include "ensure.h"
#include "rde.h"
#include "session.h"

int	prefix_cmp(struct prefix *, struct prefix *);
int	up_generate_attr(struct rde_peer *, struct update_attr *,
	    struct attr_flags *, struct nexthop *);
int	up_set_prefix(u_char *, int, struct bgpd_addr *, u_int8_t);

/*
 * Decision Engine RFC implementation:
 *  Phase 1:
 *   - calculate LOCAL_PREF if needed -- EBGP or IGP learnt routes
 *   - IBGP routes may either use LOCAL_PREF or the local system computes
 *     the degree of preference
 *   - If the route is ineligible, the route MAY NOT serve as an input to
 *     the next phase of route selection
 *   - if the route is eligible the computed value MUST be used as the
 *     LOCAL_PREF value in any IBGP readvertisement
 *
 *  Phase 2:
 *   - If the NEXT_HOP attribute of a BGP route depicts an address that is
 *     not resolvable the BGP route MUST be excluded from the Phase 2 decision
 *     function.
 *   - If the AS_PATH attribute of a BGP route contains an AS loop, the BGP
 *     route should be excluded from the Phase 2 decision function.
 *   - The local BGP speaker identifies the route that has:
 *     a) the highest degree of preference of any route to the same set
 *        of destinations
 *     b) is the only route to that destination
 *     c) is selected as a result of the Phase 2 tie breaking rules
 *   - The local speaker MUST determine the immediate next-hop address from
 *     the NEXT_HOP attribute of the selected route.
 *   - If either the immediate next hop or the IGP cost to the NEXT_HOP changes,
 *     Phase 2 Route Selection MUST be performed again.
 *
 *  Route Resolvability Condition
 *   - A route Rte1, referencing only the intermediate network address, is
 *     considered resolvable if the Routing Table contains at least one
 *     resolvable route Rte2 that matches Rte1's intermediate network address
 *     and is not recursively resolved through Rte1.
 *   - Routes referencing interfaces are considered resolvable if the state of
 *     the referenced interface is up and IP processing is enabled.
 *
 *  Breaking Ties (Phase 2)
 *   1. Remove from consideration all routes which are not tied for having the
 *      smallest number of AS numbers present in their AS_PATH attributes.
 *      Note, that when counting this number, an AS_SET counts as 1
 *   2. Remove from consideration all routes which are not tied for having the
 *      lowest Origin number in their Origin attribute.
 *   3. Remove from consideration routes with less-preferred MULTI_EXIT_DISC
 *      attributes. MULTI_EXIT_DISC is only comparable between routes learned
 *      from the same neighboring AS.
 *   4. If at least one of the candidate routes was received via EBGP,
 *      remove from consideration all routes which were received via IBGP.
 *   5. Remove from consideration any routes with less-preferred interior cost.
 *      If the NEXT_HOP hop for a route is reachable, but no cost can be
 *      determined, then this step should be skipped.
 *   6. Remove from consideration all routes other than the route that was
 *      advertised by the BGP speaker whose BGP Identifier has the lowest value.
 *   7. Prefer the route received from the lowest peer address.
 *
 * Phase 3: Route Dissemination
 *   - All routes in the Loc-RIB are processed into Adj-RIBs-Out according
 *     to configured policy. A route SHALL NOT be installed in the Adj-Rib-Out
 *     unless the destination and NEXT_HOP described by this route may be
 *     forwarded appropriately by the Routing Table.
 */

/*
 * Decision Engine OUR implementation:
 * Our implementation has only one RIB. The filtering is done first. The
 * filtering calculates the preference and stores it in LOCAL_PREF (Phase 1).
 * Ineligible routes are flagged as ineligible via nexthop_add(). The flags
 * are inherited from the nexthop descriptor.
 * Phase 3 is done together with Phase 2 -- the output filtering is done in
 * the session engine.
 * In following cases a prefix needs to be reevaluated:
 *  - update of a prefix (path_update)
 *  - withdraw of a prefix (prefix_remove)
 *  - state change of the nexthop (nexthop-{in}validate)
 *  - state change of session (session down)
 *
 */

/*
 * Compare two prefixes with equal pt_entry. Returns an integer greater than or
 * less than 0, according to whether the prefix p1 is more or less preferred
 * than the prefix p2. p1 should be used for the new prefix and p2 for a
 * already added prefix.
 */
int
prefix_cmp(struct prefix *p1, struct prefix *p2)
{
	struct rde_aspath	*asp1, *asp2;

	ENSURE(p1 != NULL);

	/* p2 is allowed to be NULL */
	if (p2 == NULL)
		return (1);

	asp1 = p1->aspath;
	asp2 = p2->aspath;
	/* 1. check if prefix is eligible a.k.a reachable */
	if (asp2->nexthop == NULL)
		return (1);
	if (asp1->nexthop == NULL)
		return (-1);
	if (asp2->nexthop->state != NEXTHOP_REACH)
		return (1);
	if (asp1->nexthop->state != NEXTHOP_REACH)
		return (-1);

	/* 2. preference of prefix, bigger is better */
	if ((asp1->flags.lpref - asp2->flags.lpref) != 0)
		return (asp1->flags.lpref - asp2->flags.lpref);

	/* 3. aspath count, the shorter the better */
	if ((asp2->flags.aspath->hdr.as_cnt -
	    asp1->flags.aspath->hdr.as_cnt) != 0)
		return (asp2->flags.aspath->hdr.as_cnt -
		    asp1->flags.aspath->hdr.as_cnt);

	/* 4. origin, the lower the better */
	if ((asp2->flags.origin - asp1->flags.origin) != 0)
		return (asp2->flags.origin - asp1->flags.origin);

	/* 5. MED decision, only comparable between the same neighboring AS */
	if (aspath_neighbour(asp1->flags.aspath) ==
	    aspath_neighbour(asp2->flags.aspath))
		/* the bigger, the better */
		if ((asp1->flags.med - asp2->flags.med) != 0)
			return (asp1->flags.med - asp2->flags.med);

	/*
	 * 6. EBGP is cooler than IBGP 
	 * It is absolutely important that the ebgp value in peer_config.ebgp
	 * is bigger than all other ones (IBGP, confederations)
	 */
	if ((p1->peer->conf.ebgp - p2->peer->conf.ebgp) != 0) {
		if (p1->peer->conf.ebgp == 1) /* p1 is EBGP other is lower */
			return 1;
		else if (p2->peer->conf.ebgp == 1) /* p2 is EBGP */
			return -1;
	}

	/* 7. nexthop costs. NOT YET -> IGNORE */

	/* 8. lowest BGP Id wins */
	if ((p2->peer->remote_bgpid - p1->peer->remote_bgpid) != 0)
		return (p2->peer->remote_bgpid - p1->peer->remote_bgpid);

	/* 9. lowest peer address wins */
	if ((p2->peer->conf.remote_addr.sin_addr.s_addr -
	    p1->peer->conf.remote_addr.sin_addr.s_addr) != 0)
		return (p2->peer->conf.remote_addr.sin_addr.s_addr -
		    p1->peer->conf.remote_addr.sin_addr.s_addr);

	fatalx("Uh, oh a politician in the decision process");
	/* NOTREACHED */
	return 0;
}

/*
 * Find the correct place to insert the prefix in the prefix list.
 * If the active prefix has changed we need to send an update.
 * The to evaluate prefix must not be in the prefix list.
 */
void
prefix_evaluate(struct prefix *p, struct pt_entry *pte)
{
	struct prefix	*xp;

	if (p != NULL) {
		if (LIST_EMPTY(&pte->prefix_h))
			LIST_INSERT_HEAD(&pte->prefix_h, p, prefix_l);
		else {
			LIST_FOREACH(xp, &pte->prefix_h, prefix_l)
				if (prefix_cmp(p, xp) > 0) {
					LIST_INSERT_BEFORE(xp, p, prefix_l);
					break;
				}
		}
	}
	xp = LIST_FIRST(&pte->prefix_h);
	if (pte->active != xp) {
		/* need to generate an update */
		if (pte->active != NULL) {
			ENSURE(pte->active->aspath != NULL);
			ENSURE(pte->active->aspath->active_cnt > 0);
			pte->active->aspath->active_cnt--;
		}

		/*
		 * Send update with remove for pte->active and add for xp
		 * but remember that xp may be ineligible or NULL.
		 * Do not send an update if the only available path
		 * has an unreachable nexthop. This decision has to be made
		 * by the called functions.
		 */
		rde_generate_updates(xp, pte->active);
		rde_send_kroute(xp, pte->active);

		if (xp == NULL || xp->aspath->nexthop == NULL ||
		    xp->aspath->nexthop->state != NEXTHOP_REACH)
			pte->active = NULL;
		else {
			pte->active = xp;
			pte->active->aspath->active_cnt++;
			ENSURE(pte->active->aspath->active_cnt <=
			    pte->active->aspath->prefix_cnt);
		}
	}
}


/* update stuff. */
struct update_prefix {
	struct bgpd_addr		 prefix;
	int				 prefixlen;
	struct uplist_prefix		*prefix_h;
	TAILQ_ENTRY(update_prefix)	 prefix_l;
	RB_ENTRY(update_prefix)		 entry;
};

struct update_attr {
	u_long				 attr_hash;
	u_char				*attr;
	u_int16_t			 attr_len;
	struct uplist_prefix		 prefix_h;
	TAILQ_ENTRY(update_attr)	 attr_l;
	RB_ENTRY(update_attr)		 entry;
};

int	up_prefix_cmp(struct update_prefix *, struct update_prefix *);
int	up_attr_cmp(struct update_attr *, struct update_attr *);
int	up_add(struct rde_peer *, struct update_prefix *, struct update_attr *);

RB_PROTOTYPE(uptree_prefix, update_prefix, entry, up_prefix_cmp);
RB_GENERATE(uptree_prefix, update_prefix, entry, up_prefix_cmp);

RB_PROTOTYPE(uptree_attr, update_attr, entry, up_attr_cmp);
RB_GENERATE(uptree_attr, update_attr, entry, up_attr_cmp);

void
up_init(struct rde_peer *peer)
{
	TAILQ_INIT(&peer->updates);
	TAILQ_INIT(&peer->withdraws);
	RB_INIT(&peer->up_prefix);
	RB_INIT(&peer->up_attrs);
	peer->up_pcnt = 0;
	peer->up_acnt = 0;
	peer->up_nlricnt = 0;
	peer->up_wcnt = 0;
}

void
up_down(struct rde_peer *peer)
{
	struct update_attr	*ua, *xua;
	struct update_prefix	*up, *xup;

	for (ua = TAILQ_FIRST(&peer->updates); ua != TAILQ_END(&peer->updates);
	    ua = xua) {
		xua = TAILQ_NEXT(ua, attr_l);
		for (up = TAILQ_FIRST(&ua->prefix_h);
		    up != TAILQ_END(&ua->prefix_h); up = xup) {
			xup = TAILQ_NEXT(up, prefix_l);
			free(up);
		}
		free(ua);
	}

	for (up = TAILQ_FIRST(&peer->withdraws);
	    up != TAILQ_END(&peer->withdraws); up = xup) {
		xup = TAILQ_NEXT(up, prefix_l);
		free(up);
	}

	TAILQ_INIT(&peer->updates);
	TAILQ_INIT(&peer->withdraws);
	RB_INIT(&peer->up_prefix);
	RB_INIT(&peer->up_attrs);

	peer->up_pcnt = 0;
	peer->up_acnt = 0;
	peer->up_nlricnt = 0;
	peer->up_wcnt = 0;
}

int
up_prefix_cmp(struct update_prefix *a, struct update_prefix *b)
{
	ENSURE(a->prefix.af == AF_INET);

	if (a->prefix.v4.s_addr < b->prefix.v4.s_addr)
		return (-1);
	if (a->prefix.v4.s_addr > b->prefix.v4.s_addr)
		return (1);
	if (a->prefixlen < b->prefixlen)
		return (-1);
	if (a->prefixlen > b->prefixlen)
		return (1);
	return (0);
}

int
up_attr_cmp(struct update_attr *a, struct update_attr *b)
{
	if (a->attr_hash < b->attr_hash)
		return (-1);
	if (a->attr_hash > b->attr_hash)
		return (1);
	if (a->attr_len < b->attr_len)
		return (-1);
	if (a->attr_len > b->attr_len)
		return (1);
	return memcmp(a->attr, b->attr, a->attr_len);
}

int
up_add(struct rde_peer *peer, struct update_prefix *p, struct update_attr *a)
{
	struct update_attr	*na;
	struct update_prefix	*np;

	ENSURE(p != NULL);

	/* 1. search for attr */
	if (a != NULL && (na = RB_FIND(uptree_attr, &peer->up_attrs, a)) ==
	    NULL) {
		/* 1.1 if not found -> add */
		TAILQ_INIT(&a->prefix_h);
		if (RB_INSERT(uptree_attr, &peer->up_attrs, a) != NULL) {
			logit(LOG_CRIT, "uptree_attr insert failed");
			return (-1);
		}
		TAILQ_INSERT_TAIL(&peer->updates, a, attr_l);
		peer->up_acnt++;
	} else {
		/* 1.2 if found -> use that, free a */
		if (a != NULL) {
			free(a);
			a = na;
			/* move to end of update queue */
			TAILQ_REMOVE(&peer->updates, a, attr_l);
			TAILQ_INSERT_TAIL(&peer->updates, a, attr_l);
		}
	}

	/* 2. search for prefix */
	if ((np = RB_FIND(uptree_prefix, &peer->up_prefix, p)) == NULL) {
		/* 2.1 if not found -> add */
		if (RB_INSERT(uptree_prefix, &peer->up_prefix, p) != NULL) {
			logit(LOG_CRIT, "uptree_prefix insert failed");
			return (-1);
		}
		peer->up_pcnt++;
	} else {
		/* 2.2 if found -> use that and free p */
		TAILQ_REMOVE(np->prefix_h, np, prefix_l);
		free(p);
		p = np;
		if (p->prefix_h == &peer->withdraws)
			peer->up_wcnt--;
		else
			peer->up_nlricnt--;
	}
	/* 3. link prefix to attr */
	if (a == NULL) {
		TAILQ_INSERT_TAIL(&peer->withdraws, p, prefix_l);
		p->prefix_h = &peer->withdraws;
		peer->up_wcnt++;
	} else {
		TAILQ_INSERT_TAIL(&a->prefix_h, p, prefix_l);
		p->prefix_h = &a->prefix_h;
		peer->up_nlricnt++;
	}
	return (0);
}

void
up_generate_updates(struct rde_peer *peer,
    struct prefix *new, struct prefix *old)
{
	struct update_attr		*a;
	struct update_prefix		*p;

	ENSURE(peer->state == PEER_UP);
	/*
	 * Filtering should be hooked up here.
	 * With filtering the decision if withdraw, update or nothing
	 * needs to be done on a per peer basis -- acctually per filter
	 * set.
	 */

	if (new == NULL || new->aspath->nexthop == NULL ||
	    new->aspath->nexthop->state != NEXTHOP_REACH) {
		if (peer == old->peer)
			/* Do not send routes back to sender */
			return;

		if (peer->conf.ebgp == 0 && old->peer->conf.ebgp == 0)
			/* Do not redistribute updates to ibgp peers */
			return;

		/* announce type handling */
		switch (peer->conf.announce_type) {
		case ANNOUNCE_NONE:
			return;
		case ANNOUNCE_ALL:
			break;
		case ANNOUNCE_SELF:
			/*
			 * pass only prefix that have a aspath count
			 * of zero this is equal to the ^$ regex.
			 */
			if (old->aspath->flags.aspath->hdr.as_cnt != 0)
				return;
			break;
		}

		/* withdraw prefix */
		p = calloc(1, sizeof(struct update_prefix));
		if (p == NULL)
			fatal("up_queue_update");

		p->prefix = old->prefix->prefix;
		p->prefixlen = old->prefix->prefixlen;
		if (up_add(peer, p, NULL) == -1)
			logit(LOG_CRIT, "queuing update failed.");
	} else {
		if (peer == new->peer)
			/* Do not send routes back to sender */
			return;

		if (peer->conf.ebgp == 0 && new->peer->conf.ebgp == 0)
			/* Do not redistribute updates to ibgp peers */
			return;

		/* announce type handling */
		switch (peer->conf.announce_type) {
		case ANNOUNCE_NONE:
			return;
		case ANNOUNCE_ALL:
			break;
		case ANNOUNCE_SELF:
			/*
			 * pass only prefix that have a aspath count
			 * of zero this is equal to the ^$ regex.
			 */
			if (new->aspath->flags.aspath->hdr.as_cnt != 0)
				return;
			break;
		}

		/* generate update */
		p = calloc(1, sizeof(struct update_prefix));
		if (p == NULL)
			fatal("up_queue_update");

		a = calloc(1, sizeof(struct update_attr));
		if (a == NULL)
			fatal("up_queue_update");

		if (up_generate_attr(peer, a, &new->aspath->flags,
		    new->aspath->nexthop) == -1)
			logit(LOG_CRIT,
			    "generation of bgp path attributes failed");

		/*
		 * use aspath_hash as attr_hash, this may be unoptimal
		 * but currently I don't care.
		 */
		a->attr_hash = aspath_hash(new->aspath->flags.aspath);
		p->prefix = new->prefix->prefix;
		p->prefixlen = new->prefix->prefixlen;

		if (up_add(peer, p, a) == -1)
			logit(LOG_CRIT, "queuing update failed.");
	}
}

u_char	up_attr_buf[4096];

int
up_generate_attr(struct rde_peer *peer, struct update_attr *upa,
    struct attr_flags *a, struct nexthop *nh)
{
	struct attr	*oa;
	u_int32_t	 tmp32;
	in_addr_t	 nexthop, mask;
	int		 r;
	u_int16_t	 len = sizeof(up_attr_buf), wlen = 0;

	/* origin */
	if ((r = attr_write(up_attr_buf + wlen, len, ATTR_WELL_KNOWN,
	    ATTR_ORIGIN, &a->origin, 1)) == -1)
		return (-1);
	wlen += r; len -= r;

	/* aspath */
	if ((r = aspath_write(up_attr_buf + wlen, len, a->aspath,
	    rde_local_as(), peer->conf.ebgp == 0 ? 0 : 1)) == -1)
		return (-1);
	wlen += r; len -= r;

	/* nexthop, already network byte order */
	if (peer->conf.ebgp == 0) {
		/*
		 * if directly connected use peer->local_addr
		 */
		if (nh->flags & NEXTHOP_ANNOUNCE)
			nexthop = peer->local_addr.v4.s_addr;
		else if (a->nexthop == peer->remote_addr.v4.s_addr)
			/*
			 * per rfc: if remote peer address is equal to
			 * the nexthop set the nexthop to our local address.
			 * This reduces the risk of routing loops.
			 */
			nexthop = peer->local_addr.v4.s_addr;
		else
			nexthop = nh->exit_nexthop.v4.s_addr;
	} else if (peer->conf.distance == 1) {
		/* ebgp directly connected */
		if (nh->flags & NEXTHOP_CONNECTED) {
			mask = 0xffffffff << (32 - nh->nexthop_netlen);
			mask = htonl(mask);
			if ((peer->remote_addr.v4.s_addr & mask) ==
			    (nh->nexthop_net.v4.s_addr & mask))
				/* nexthop and peer are in the same net */
				nexthop = nh->exit_nexthop.v4.s_addr;
			else
				nexthop = peer->local_addr.v4.s_addr;
		} else
			nexthop = peer->local_addr.v4.s_addr;
	} else
		/* ebgp multihop */
		/*
		 * XXX for ebgp multihop nh->flags should never have
		 * NEXTHOP_CONNECTED set so it should be possible to unify the
		 * two ebgp cases.
		 */
		nexthop = peer->local_addr.v4.s_addr;

	if ((r = attr_write(up_attr_buf + wlen, len, ATTR_WELL_KNOWN,
	    ATTR_NEXTHOP, &nexthop, 4)) == -1)
		return (-1);
	wlen += r; len -= r;

	/*
	 * The MED of other peers MUST not be announced to others.
	 * Currently we just dump it. Possibilities are setting the MED via
	 * a filter or set it to local-pref. struct attr_flags probably needs
	 * a med_in and a med_out field.
	 */

	if (peer->conf.ebgp == 0) {
		/* local preference, only valid for ibgp */
		tmp32 = htonl(a->lpref);
		if ((r = attr_write(up_attr_buf + wlen, len, ATTR_WELL_KNOWN,
		    ATTR_LOCALPREF, &tmp32, 4)) == -1)
			return (-1);
		wlen += r; len -= r;
	}

	/*
	 * dump all other path attributes. Following rules apply:
	 *  1. well-known attrs: ATTR_ATOMIC_AGGREGATE and ATTR_AGGREGATOR
	 *     pass unmodified (enforce flags to correct values)
	 *  2. non-transitive attrs: don't re-announce
	 *  3. transitive known attrs: announce unmodified
	 *  4. transitive unknown attrs: set partial bit and re-announce
	 */ 
	TAILQ_FOREACH(oa, &a->others, attr_l) {
		switch (oa->type) {
		case ATTR_ATOMIC_AGGREGATE:
			if ((r = attr_write(up_attr_buf + wlen, len,
			    ATTR_WELL_KNOWN, ATTR_ATOMIC_AGGREGATE,
			    NULL, 0)) == -1)
				return (-1);
			break;
		case ATTR_AGGREGATOR:
			if ((r = attr_write(up_attr_buf + wlen, len,
			    ATTR_OPTIONAL | ATTR_TRANSITIVE, ATTR_AGGREGATOR,
			    oa->data, oa->len)) == -1)
				return (-1);
			break;
		/*
		 * currently there are no non-transitive or transitive known
		 * attributes.
		 */
		default:
			/* unknown attribute */
			if (!(oa->flags & ATTR_OPTIONAL))
				/* somehow a non-transitive slipped through */
				break;
			if ((r = attr_write(up_attr_buf + wlen, len,
			    oa->flags | ATTR_PARTIAL, oa->type,
			    oa->data, oa->len)) == -1)
				return (-1);
			break;
		}
		wlen += r; len -= r;
	}

	/* the bgp path attributes are now stored in the global buf */
	upa->attr = malloc(wlen);
	if (upa->attr == NULL)
		fatal("up_generate_attr");
	memcpy(upa->attr, up_attr_buf, wlen);
	upa->attr_len = wlen;
	return (wlen);
}

int
up_set_prefix(u_char *buf, int len, struct bgpd_addr *prefix, u_int8_t plen)
{
	int	totlen;

	ENSURE(prefix->af == AF_INET);
	ENSURE(plen <= 32);
	totlen = (plen + 7) / 8 + 1;

	if (totlen > len)
		return (-1);
	*buf++ = plen;
	memcpy(buf, &prefix->v4.s_addr, totlen - 1);
	return (totlen);
}

int
up_dump_prefix(u_char *buf, int len, struct uplist_prefix *prefix_head,
    struct rde_peer *peer)
{
	struct update_prefix	*upp, *xupp;
	int			 r, wpos = 0;

	for (upp = TAILQ_FIRST(prefix_head);
	    upp != TAILQ_END(prefix_head); upp = xupp) {
		xupp = TAILQ_NEXT(upp, prefix_l);
		if ((r = up_set_prefix(buf + wpos, len - wpos,
		    &upp->prefix, upp->prefixlen)) == -1)
			break;
		wpos += r;
		if (RB_REMOVE(uptree_prefix, &peer->up_prefix, upp) == NULL)
			logit(LOG_CRIT, "dequeuing update failed.");
		TAILQ_REMOVE(upp->prefix_h, upp, prefix_l);
		peer->up_pcnt--;
		if (upp->prefix_h == &peer->withdraws)
			peer->up_wcnt--;
		else
			peer->up_nlricnt--;
		free(upp);
	}
	return (wpos);
}

int
up_dump_attrnlri(u_char *buf, int len, struct rde_peer *peer)
{
	struct update_attr	*upa;
	int			 r, wpos;
	u_int16_t		 attr_len;

	upa = TAILQ_FIRST(&peer->updates);
	if (upa == NULL || upa->attr_len + 5 > len) {
		/*
		 * either no packet or not enough space.
		 * The length field needs to be set to zero else it would be
		 * an invalid bgp update.
		 */
		bzero(buf, 2);
		return (2);
	}

	/* first dump the 2-byte path attribute length */
	attr_len = htons(upa->attr_len);
	memcpy(buf, &attr_len, 2);
	wpos = 2;

	/* then the path attributes them self */
	memcpy(buf + wpos, upa->attr, upa->attr_len);
	wpos += upa->attr_len;

	/* last but not least dump the nlri */
	r = up_dump_prefix(buf + wpos, len - wpos, &upa->prefix_h, peer);
	wpos += r;

	/* now check if all prefixes where written */
	if (TAILQ_EMPTY(&upa->prefix_h)) {
		if (RB_REMOVE(uptree_attr, &peer->up_attrs, upa) == NULL)
			logit(LOG_CRIT, "dequeuing update failed.");
		TAILQ_REMOVE(&peer->updates, upa, attr_l);
		free(upa);
		peer->up_acnt--;
	}

	return (wpos);
}

void
up_dump_upcall(struct pt_entry *pt, void *ptr)
{
	struct rde_peer	*peer = ptr;

	if (pt->active == NULL)
		return;
	up_generate_updates(peer, pt->active, NULL);
}
