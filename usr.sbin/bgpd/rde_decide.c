/*	$OpenBSD: rde_decide.c,v 1.86 2021/08/09 08:15:34 claudio Exp $ */

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
#include "log.h"

int	prefix_cmp(struct prefix *, struct prefix *, int *);
void	prefix_insert(struct prefix *, struct prefix *, struct rib_entry *);
void	prefix_remove(struct prefix *, struct rib_entry *);
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
 * The filtering is done first. The filtering calculates the preference and
 * stores it in LOCAL_PREF (Phase 1).
 * Ineligible routes are flagged as ineligible via nexthop_add().
 * Phase 3 is done together with Phase 2.
 * In following cases a prefix needs to be reevaluated:
 *  - update of a prefix (prefix_update)
 *  - withdraw of a prefix (prefix_withdraw)
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
prefix_cmp(struct prefix *p1, struct prefix *p2, int *testall)
{
	struct rde_aspath	*asp1, *asp2;
	struct rde_peer		*peer1, *peer2;
	struct attr		*a;
	u_int32_t		 p1id, p2id;
	int			 p1cnt, p2cnt, i;

	/*
	 * If a match happens before the MED check then the list is
	 * correctly sorted. If a match happens after MED then further
	 * elements may need to be checked to ensure that all paths
	 * which could affect this path were considered. This only
	 * matters for strict MED evaluation and in that case testall
	 * is set to 1. If the check happens to be on the MED check
	 * itself testall is set to 2.
	 */
	*testall = 0;

	if (p1 == NULL)
		return -1;
	if (p2 == NULL)
		return 1;

	asp1 = prefix_aspath(p1);
	asp2 = prefix_aspath(p2);
	peer1 = prefix_peer(p1);
	peer2 = prefix_peer(p2);

	/* pathes with errors are not eligible */
	if (asp1 == NULL || asp1->flags & F_ATTR_PARSE_ERR)
		return -1;
	if (asp2 == NULL || asp2->flags & F_ATTR_PARSE_ERR)
		return 1;

	/* only loop free pathes are eligible */
	if (asp1->flags & F_ATTR_LOOP)
		return -1;
	if (asp2->flags & F_ATTR_LOOP)
		return 1;

	/*
	 * 1. check if prefix is eligible a.k.a reachable
	 *    A NULL nexthop is eligible since it is used for locally
	 *    announced networks.
	 */
	if (prefix_nexthop(p2) != NULL &&
	    prefix_nexthop(p2)->state != NEXTHOP_REACH)
		return 1;
	if (prefix_nexthop(p1) != NULL &&
	    prefix_nexthop(p1)->state != NEXTHOP_REACH)
		return -1;

	/* 2. local preference of prefix, bigger is better */
	if (asp1->lpref > asp2->lpref)
		return 1;
	if (asp1->lpref < asp2->lpref)
		return -1;

	/* 3. aspath count, the shorter the better */
	if ((asp2->aspath->ascnt - asp1->aspath->ascnt) != 0)
		return (asp2->aspath->ascnt - asp1->aspath->ascnt);

	/* 4. origin, the lower the better */
	if ((asp2->origin - asp1->origin) != 0)
		return (asp2->origin - asp1->origin);

	/*
	 * 5. MED decision
	 * Only comparable between the same neighboring AS or if
	 * 'rde med compare always' is set. In the first case
	 * set the testall flag since further elements need to be
	 * evaluated as well.
	 */
	if ((rde_decisionflags() & BGPD_FLAG_DECISION_MED_ALWAYS) ||
	    aspath_neighbor(asp1->aspath) == aspath_neighbor(asp2->aspath)) {
		if (!(rde_decisionflags() & BGPD_FLAG_DECISION_MED_ALWAYS))
			*testall = 2;
		/* lowest value wins */
		if (asp1->med < asp2->med)
			return 1;
		if (asp1->med > asp2->med)
			return -1;
	}

	if (!(rde_decisionflags() & BGPD_FLAG_DECISION_MED_ALWAYS))
		*testall = 1;

	/*
	 * 6. EBGP is cooler than IBGP
	 * It is absolutely important that the ebgp value in peer_config.ebgp
	 * is bigger than all other ones (IBGP, confederations)
	 */
	if (peer1->conf.ebgp != peer2->conf.ebgp) {
		if (peer1->conf.ebgp) /* peer1 is EBGP other is lower */
			return 1;
		else if (peer2->conf.ebgp) /* peer2 is EBGP */
			return -1;
	}

	/*
	 * 7. local tie-breaker, this weight is here to tip equal long AS
	 * paths in one or the other direction. It happens more and more
	 * that AS paths are equally long and so traffic engineering needs
	 * a metric that weights a prefix at a very late stage in the
	 * decision process.
	 */
	if (asp1->weight > asp2->weight)
		return 1;
	if (asp1->weight < asp2->weight)
		return -1;

	/* 8. nexthop costs. NOT YET -> IGNORE */

	/*
	 * 9. older route (more stable) wins but only if route-age
	 * evaluation is enabled.
	 */
	if (rde_decisionflags() & BGPD_FLAG_DECISION_ROUTEAGE) {
		if (p1->lastchange < p2->lastchange) /* p1 is older */
			return 1;
		if (p1->lastchange > p2->lastchange)
			return -1;
	}

	/* 10. lowest BGP Id wins, use ORIGINATOR_ID if present */
	if ((a = attr_optget(asp1, ATTR_ORIGINATOR_ID)) != NULL) {
		memcpy(&p1id, a->data, sizeof(p1id));
		p1id = ntohl(p1id);
	} else
		p1id = peer1->remote_bgpid;
	if ((a = attr_optget(asp2, ATTR_ORIGINATOR_ID)) != NULL) {
		memcpy(&p2id, a->data, sizeof(p2id));
		p2id = ntohl(p2id);
	} else
		p2id = peer2->remote_bgpid;
	if (p1id < p2id)
		return 1;
	if (p1id > p2id)
		return -1;

	/* 11. compare CLUSTER_LIST length, shorter is better */
	p1cnt = p2cnt = 0;
	if ((a = attr_optget(asp1, ATTR_CLUSTER_LIST)) != NULL)
		p1cnt = a->len / sizeof(u_int32_t);
	if ((a = attr_optget(asp2, ATTR_CLUSTER_LIST)) != NULL)
		p2cnt = a->len / sizeof(u_int32_t);
	if ((p2cnt - p1cnt) != 0)
		return (p2cnt - p1cnt);

	/* 12. lowest peer address wins (IPv4 is better than IPv6) */
	if (peer1->remote_addr.aid < peer2->remote_addr.aid)
		return 1;
	if (peer1->remote_addr.aid > peer2->remote_addr.aid)
		return -1;
	switch (peer1->remote_addr.aid) {
	case AID_INET:
		i = memcmp(&peer1->remote_addr.v4, &peer2->remote_addr.v4,
		    sizeof(struct in_addr));
		break;
	case AID_INET6:
		i = memcmp(&peer1->remote_addr.v6, &peer2->remote_addr.v6,
		    sizeof(struct in6_addr));
		break;
	default:
		fatalx("%s: unknown af", __func__);
	}
	if (i < 0)
		return 1;
	if (i > 0)
		return -1;

	/* XXX RFC7911 does not specify this but it is needed. */
	/* 13. lowest path identifier wins */
	if (p1->path_id < p2->path_id)
		return 1;
	if (p1->path_id > p2->path_id)
		return -1;

	fatalx("Uh, oh a politician in the decision process");
}

/*
 * Insert a prefix keeping the total order of the list. For routes
 * that may depend on a MED selection the set is scanned until the
 * condition is cleared. If a MED inversion is detected the respective
 * prefix is taken of the rib list and put onto a redo queue. All
 * prefixes on the redo queue are re-inserted at the end.
 */
void
prefix_insert(struct prefix *new, struct prefix *ep, struct rib_entry *re)
{
	struct prefix_list redo = LIST_HEAD_INITIALIZER(redo);
	struct prefix *xp, *np, *tailp = NULL, *insertp = ep;
	int testall, selected = 0;

	/* start scan at the entry point (ep) or if the head if ep == NULL */
	if (ep == NULL)
		ep = LIST_FIRST(&re->prefix_h);

	for (xp = ep; xp != NULL; xp = np) {
		np = LIST_NEXT(xp, entry.list.rib);

		if (prefix_cmp(new, xp, &testall) > 0) {
			/* new is preferred over xp */
			if (testall == 0)
				break;		/* we're done */
			else if (testall == 2) {
				/*
				 * MED inversion, take out prefix and
				 * put it onto redo queue.
				 */
				LIST_REMOVE(xp, entry.list.rib);
				if (tailp == NULL)
					LIST_INSERT_HEAD(&redo, xp,
					    entry.list.rib);
				else
					LIST_INSERT_AFTER(tailp, xp,
					    entry.list.rib);
				tailp = xp;
			} else {
				/*
				 * lock insertion point and
				 * continue on with scan
				 */
				selected = 1;
				continue;
			}
		} else {
			/*
			 * p is less preferred, remember insertion point
			 * If p got selected during a testall traverse 
			 * do not alter the insertion point unless this
			 * happened on an actual MED check.
			 */
			if (testall == 2)
				selected = 0;
			if (!selected)
				insertp = xp;
		}
	}

	if (insertp == NULL)
		LIST_INSERT_HEAD(&re->prefix_h, new, entry.list.rib);
	else
		LIST_INSERT_AFTER(insertp, new, entry.list.rib);

	/* Fixup MED order again. All elements are < new */
	while (!LIST_EMPTY(&redo)) {
		xp = LIST_FIRST(&redo);
		LIST_REMOVE(xp, entry.list.rib);

		prefix_insert(xp, new, re);
	}
}

/*
 * Remove a prefix from the RIB list ensuring that the total order of the
 * list remains intact. All routes that differ in the MED are taken of the
 * list and put on the redo list. To figure out if a route could cause a
 * resort because of a MED check the next prefix of the to-remove prefix
 * is compared with the old prefix. A full scan is only done if the next
 * route differs because of the MED or later checks.
 * Again at the end all routes on the redo queue are reinserted.
 */
void
prefix_remove(struct prefix *old, struct rib_entry *re)
{
	struct prefix_list redo = LIST_HEAD_INITIALIZER(redo);
	struct prefix *xp, *np, *tailp = NULL;
	int testall;

	xp = LIST_NEXT(old, entry.list.rib);
	LIST_REMOVE(old, entry.list.rib);
	/* check if a MED inversion could be possible */
	prefix_cmp(old, xp, &testall);
	if (testall > 0) {
		/* maybe MED route, scan tail for other possible routes */
		for (; xp != NULL; xp = np) {
			np = LIST_NEXT(xp, entry.list.rib);

			/* only interested in the testall result */
			prefix_cmp(old, xp, &testall);
			if (testall == 0)
				break;		/* we're done */
			else if (testall == 2) {
				/*
				 * possible MED inversion, take out prefix and
				 * put it onto redo queue.
				 */
				LIST_REMOVE(xp, entry.list.rib);
				if (tailp == NULL)
					LIST_INSERT_HEAD(&redo, xp,
					    entry.list.rib);
				else
					LIST_INSERT_AFTER(tailp, xp,
					    entry.list.rib);
				tailp = xp;
			}
		}
	}

	/* Fixup MED order again, reinsert prefixes from the start */
	while (!LIST_EMPTY(&redo)) {
		xp = LIST_FIRST(&redo);
		LIST_REMOVE(xp, entry.list.rib);

		prefix_insert(xp, NULL, re);
	}
}

/* helper function to check if a prefix is valid to be selected */
int
prefix_eligible(struct prefix *p)
{
	struct rde_aspath *asp = prefix_aspath(p);
	struct nexthop *nh = prefix_nexthop(p);

	/* The aspath needs to be loop and error free */
	if (asp == NULL || asp->flags & (F_ATTR_LOOP|F_ATTR_PARSE_ERR))
		return 0;
	/*
	 * If the nexthop exists it must be reachable.
	 * It is OK if the nexthop does not exist (local announcement).
	 */
	if (nh != NULL && nh->state != NEXTHOP_REACH)
		return 0;

	return 1;
}

/*
 * Find the correct place to insert the prefix in the prefix list.
 * If the active prefix has changed we need to send an update also special
 * treatment is needed if 'rde evaluate all' is used on some peers.
 * To re-evaluate a prefix just call prefix_evaluate with old and new pointing
 * to the same prefix.
 */
void
prefix_evaluate(struct rib_entry *re, struct prefix *new, struct prefix *old)
{
	struct prefix	*xp;

	if (re_rib(re)->flags & F_RIB_NOEVALUATE) {
		/* decision process is turned off */
		if (old != NULL)
			LIST_REMOVE(old, entry.list.rib);
		if (new != NULL)
			LIST_INSERT_HEAD(&re->prefix_h, new, entry.list.rib);
		if (re->active) {
			/*
			 * During reloads it is possible that the decision
			 * process is turned off but prefixes are still
			 * active. Clean up now to ensure that the RIB
			 * is consistant.
			 */
			rde_generate_updates(re_rib(re), NULL, re->active, 0);
			re->active = NULL;
		}
		return;
	}

	if (old != NULL)
		prefix_remove(old, re);

	if (new != NULL)
		prefix_insert(new, NULL, re);

	xp = LIST_FIRST(&re->prefix_h);
	if (xp != NULL && !prefix_eligible(xp))
		xp = NULL;

	/*
	 * If the active prefix changed or the active prefix was removed
	 * and added again then generate an update.
	 */
	if (re->active != xp || (old != NULL && xp == old)) {
		/*
		 * Send update withdrawing re->active and adding xp
		 * but remember that xp may be NULL aka ineligible.
		 * Additional decision may be made by the called functions.
		 */
		rde_generate_updates(re_rib(re), xp, re->active, 0);
		re->active = xp;
		return;
	}

	/*
	 * If there are peers with 'rde evaluate all' every update needs
	 * to be passed on (not only a change of the best prefix).
	 * rde_generate_updates() will then take care of distribution.
	 */
	if (rde_evaluate_all())
		if ((new != NULL && prefix_eligible(new)) || old != NULL)
			rde_generate_updates(re_rib(re), re->active, NULL, 1);
}
