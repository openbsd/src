/*	$OpenBSD: rde_decide.c,v 1.60 2010/05/03 13:09:38 claudio Exp $ */

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

#include <string.h>

#include "bgpd.h"
#include "rde.h"

int	prefix_cmp(struct prefix *, struct prefix *);
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
 * Ineligible routes are flagged as ineligible via nexthop_add().
 * Phase 3 is done together with Phase 2.
 * In following cases a prefix needs to be reevaluated:
 *  - update of a prefix (path_update)
 *  - withdraw of a prefix (prefix_remove)
 *  - state change of the nexthop (nexthop-{in}validate)
 *  - state change of session (session down)
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

	if (p1 == NULL)
		return (-1);
	if (p2 == NULL)
		return (1);

	asp1 = p1->aspath;
	asp2 = p2->aspath;

	/* pathes with errors are not eligible */
	if (asp1->flags & F_ATTR_PARSE_ERR)
		return (-1);
	if (asp2->flags & F_ATTR_PARSE_ERR)
		return (1);

	/* only loop free pathes are eligible */
	if (asp1->flags & F_ATTR_LOOP)
		return (-1);
	if (asp2->flags & F_ATTR_LOOP)
		return (1);

	/* 1. check if prefix is eligible a.k.a reachable */
	if (asp2->nexthop != NULL && asp2->nexthop->state != NEXTHOP_REACH)
		return (1);
	if (asp1->nexthop != NULL && asp1->nexthop->state != NEXTHOP_REACH)
		return (-1);

	/* 2. preference of prefix, bigger is better */
	if ((asp1->lpref - asp2->lpref) != 0)
		return (asp1->lpref - asp2->lpref);

	/* 3. aspath count, the shorter the better */
	if ((asp2->aspath->ascnt - asp1->aspath->ascnt) != 0)
		return (asp2->aspath->ascnt - asp1->aspath->ascnt);

	/* 4. origin, the lower the better */
	if ((asp2->origin - asp1->origin) != 0)
		return (asp2->origin - asp1->origin);

	/* 5. MED decision, only comparable between the same neighboring AS */
	if (rde_decisionflags() & BGPD_FLAG_DECISION_MED_ALWAYS ||
	    aspath_neighbor(asp1->aspath) == aspath_neighbor(asp2->aspath))
		/* lowest value wins */
		if ((asp2->med - asp1->med) != 0)
			return (asp2->med - asp1->med);

	/*
	 * 6. EBGP is cooler than IBGP
	 * It is absolutely important that the ebgp value in peer_config.ebgp
	 * is bigger than all other ones (IBGP, confederations)
	 */
	if ((asp1->peer->conf.ebgp - asp2->peer->conf.ebgp) != 0) {
		if (asp1->peer->conf.ebgp == 1) /* p1 is EBGP other is lower */
			return 1;
		else if (asp2->peer->conf.ebgp == 1) /* p2 is EBGP */
			return -1;
	}

	/*
	 * 7. local tie-breaker, this weight is here to tip equal long AS
	 * paths in one or the other direction. It happens more and more
	 * that AS paths are equally long and so traffic engineering needs
	 * a metric that weights a prefix at a very late stage in the
	 * decision process.
	 */
	if ((asp1->weight - asp2->weight) != 0)
		return (asp1->weight - asp2->weight);

	/* 8. nexthop costs. NOT YET -> IGNORE */

	/*
	 * 9. older route (more stable) wins but only if route-age
	 * evaluation is enabled.
	 */
	if (rde_decisionflags() & BGPD_FLAG_DECISION_ROUTEAGE)
		if ((p2->lastchange - p1->lastchange) != 0)
			return (p2->lastchange - p1->lastchange);

	/* 10. lowest BGP Id wins */
	if ((p2->aspath->peer->remote_bgpid -
	    p1->aspath->peer->remote_bgpid) != 0)
		return (p2->aspath->peer->remote_bgpid -
		    p1->aspath->peer->remote_bgpid);

	/* 11. lowest peer address wins (IPv4 is better than IPv6) */
	if (memcmp(&p1->aspath->peer->remote_addr,
	    &p2->aspath->peer->remote_addr,
	    sizeof(p1->aspath->peer->remote_addr)) != 0)
		return (-memcmp(&p1->aspath->peer->remote_addr,
		    &p2->aspath->peer->remote_addr,
		    sizeof(p1->aspath->peer->remote_addr)));

	/* 12. for announced prefixes prefer dynamic routes */
	if ((asp1->flags & F_ANN_DYNAMIC) != (asp2->flags & F_ANN_DYNAMIC)) {
		if (asp1->flags & F_ANN_DYNAMIC)
			return (1);
		else
			return (-1);
	}

	fatalx("Uh, oh a politician in the decision process");
	/* NOTREACHED */
}

/*
 * Find the correct place to insert the prefix in the prefix list.
 * If the active prefix has changed we need to send an update.
 * The to evaluate prefix must not be in the prefix list.
 */
void
prefix_evaluate(struct prefix *p, struct rib_entry *re)
{
	struct prefix	*xp;

	if (re->flags & F_RIB_NOEVALUATE || rde_noevaluate()) {
		/* decision process is turned off */
		if (p != NULL)
			LIST_INSERT_HEAD(&re->prefix_h, p, rib_l);
		if (re->active != NULL) {
			re->active->aspath->active_cnt--;
			re->active = NULL;
		}
		return;
	}

	if (p != NULL) {
		if (LIST_EMPTY(&re->prefix_h))
			LIST_INSERT_HEAD(&re->prefix_h, p, rib_l);
		else {
			LIST_FOREACH(xp, &re->prefix_h, rib_l)
				if (prefix_cmp(p, xp) > 0) {
					LIST_INSERT_BEFORE(xp, p, rib_l);
					break;
				} else if (LIST_NEXT(xp, rib_l) == NULL) {
					/* if xp last element ... */
					LIST_INSERT_AFTER(xp, p, rib_l);
					break;
				}
		}
	}

	xp = LIST_FIRST(&re->prefix_h);
	if (xp == NULL || xp->aspath->flags & (F_ATTR_LOOP|F_ATTR_PARSE_ERR) ||
	    (xp->aspath->nexthop != NULL &&
	    xp->aspath->nexthop->state != NEXTHOP_REACH))
		/* xp is ineligible */
		xp = NULL;

	if (re->active != xp) {
		/* need to generate an update */
		if (re->active != NULL)
			re->active->aspath->active_cnt--;

		/*
		 * Send update with remove for re->active and add for xp
		 * but remember that xp may be NULL aka ineligible.
		 * Additional decision may be made by the called functions.
		 */
		rde_generate_updates(re->ribid, xp, re->active);
		if ((re->flags & F_RIB_NOFIB) == 0)
			rde_send_kroute(xp, re->active, re->ribid);

		re->active = xp;
		if (xp != NULL)
			xp->aspath->active_cnt++;
	}
}
