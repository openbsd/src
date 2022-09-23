/*	$OpenBSD: rde_update.c,v 1.148 2022/09/23 15:49:20 claudio Exp $ */

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
#include <sys/tree.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "bgpd.h"
#include "rde.h"
#include "log.h"

static struct community	comm_no_advertise = {
	.flags = COMMUNITY_TYPE_BASIC,
	.data1 = COMMUNITY_WELLKNOWN,
	.data2 = COMMUNITY_NO_ADVERTISE
};
static struct community	comm_no_export = {
	.flags = COMMUNITY_TYPE_BASIC,
	.data1 = COMMUNITY_WELLKNOWN,
	.data2 = COMMUNITY_NO_EXPORT
};
static struct community	comm_no_expsubconfed = {
	.flags = COMMUNITY_TYPE_BASIC,
	.data1 = COMMUNITY_WELLKNOWN,
	.data2 = COMMUNITY_NO_EXPSUBCONFED
};

static void up_prep_adjout(struct rde_peer *, struct filterstate *, uint8_t);

static int
up_test_update(struct rde_peer *peer, struct prefix *p)
{
	struct rde_aspath	*asp;
	struct rde_community	*comm;
	struct rde_peer		*frompeer;

	frompeer = prefix_peer(p);
	asp = prefix_aspath(p);
	comm = prefix_communities(p);

	if (asp == NULL || asp->flags & F_ATTR_PARSE_ERR)
		fatalx("try to send out a botched path");
	if (asp->flags & F_ATTR_LOOP)
		fatalx("try to send out a looped path");

	if (peer == frompeer)
		/* Do not send routes back to sender */
		return (0);

	if (!frompeer->conf.ebgp && !peer->conf.ebgp) {
		/*
		 * route reflector redistribution rules:
		 * 1. if announce is set                -> announce
		 * 2. from non-client, to non-client    -> no
		 * 3. from client, to non-client        -> yes
		 * 4. from non-client, to client        -> yes
		 * 5. from client, to client            -> yes
		 */
		if (frompeer->conf.reflector_client == 0 &&
		    peer->conf.reflector_client == 0 &&
		    (asp->flags & F_PREFIX_ANNOUNCED) == 0)
			/* Do not redistribute updates to ibgp peers */
			return (0);
	}

	/* well known communities */
	if (community_match(comm, &comm_no_advertise, NULL))
		return (0);
	if (peer->conf.ebgp) {
		if (community_match(comm, &comm_no_export, NULL))
			return (0);
		if (community_match(comm, &comm_no_expsubconfed, NULL))
			return (0);
	}

	return (1);
}

/* RFC9234 open policy handling */
static int
up_enforce_open_policy(struct rde_peer *peer, struct filterstate *state)
{
	uint8_t role;

	if (!peer_has_open_policy(peer, &role))
		return 0;

	/*
	 * do not propagate (consider it filtered) if OTC is present and
	 * neighbor role is peer, provider or rs.
	 */
	if (role == CAPA_ROLE_PEER || role == CAPA_ROLE_PROVIDER ||
	    role == CAPA_ROLE_RS)
		if (state->aspath.flags & F_ATTR_OTC)
			return (1);

	/*
	 * add OTC attribute if not present for peers, customers and rs-clients.
	 */
	if (role == CAPA_ROLE_PEER || role == CAPA_ROLE_CUSTOMER ||
	    role == CAPA_ROLE_RS_CLIENT)
		if ((state->aspath.flags & F_ATTR_OTC) == 0) {
			uint32_t tmp;

			tmp = htonl(peer->conf.local_as);
			if (attr_optadd(&state->aspath,
			    ATTR_OPTIONAL|ATTR_TRANSITIVE, ATTR_OTC,
			    &tmp, sizeof(tmp)) == -1)
				log_peer_warnx(&peer->conf,
				    "failed to add OTC attribute");
			state->aspath.flags |= F_ATTR_OTC;
		}

	return 0;
}

void
up_generate_updates(struct filter_head *rules, struct rde_peer *peer,
    struct prefix *new, struct prefix *old)
{
	struct filterstate	state;
	struct bgpd_addr	addr;
	struct prefix		*p;
	int			need_withdraw;
	uint8_t			prefixlen;

	if (new == NULL) {
		pt_getaddr(old->pt, &addr);
		prefixlen = old->pt->prefixlen;
	} else {
		pt_getaddr(new->pt, &addr);
		prefixlen = new->pt->prefixlen;
	}

	p = prefix_adjout_lookup(peer, &addr, prefixlen);

	while (new != NULL) {
		need_withdraw = 0;
		/*
		 * up_test_update() needs to run before the output filters
		 * else the well known communities won't work properly.
		 * The output filters would not be able to add well known
		 * communities.
		 */
		if (!up_test_update(peer, new))
			need_withdraw = 1;

		/*
		 * if 'rde evaluate all' is set for this peer then
		 * delay the the withdraw because of up_test_update().
		 * The filters may actually skip this prefix and so this
		 * decision needs to be delayed.
		 * For the default mode we can just give up here and
		 * skip the filters.
		 */
		if (need_withdraw &&
		    !(peer->flags & PEERFLAG_EVALUATE_ALL))
			break;

		rde_filterstate_prep(&state, prefix_aspath(new),
		    prefix_communities(new), prefix_nexthop(new),
		    prefix_nhflags(new));
		if (rde_filter(rules, peer, prefix_peer(new), &addr,
		    prefixlen, prefix_vstate(new), &state) == ACTION_DENY) {
			rde_filterstate_clean(&state);
			if (peer->flags & PEERFLAG_EVALUATE_ALL) {
				new = TAILQ_NEXT(new, entry.list.rib);
				if (new != NULL && prefix_eligible(new))
					continue;
			}
			break;
		}

		if (up_enforce_open_policy(peer, &state)) {
			rde_filterstate_clean(&state);
			if (peer->flags & PEERFLAG_EVALUATE_ALL) {
				new = TAILQ_NEXT(new, entry.list.rib);
				if (new != NULL && prefix_eligible(new))
					continue;
			}
			break;
		}

		/* check if this was actually a withdraw */
		if (need_withdraw)
			break;

		/* from here on we know this is an update */

		up_prep_adjout(peer, &state, addr.aid);
		prefix_adjout_update(p, peer, &state, &addr,
		    new->pt->prefixlen, new->path_id_tx, prefix_vstate(new));
		rde_filterstate_clean(&state);

		/* max prefix checker outbound */
		if (peer->conf.max_out_prefix &&
		    peer->prefix_out_cnt > peer->conf.max_out_prefix) {
			log_peer_warnx(&peer->conf,
			    "outbound prefix limit reached (>%u/%u)",
			    peer->prefix_out_cnt, peer->conf.max_out_prefix);
			rde_update_err(peer, ERR_CEASE,
			    ERR_CEASE_MAX_SENT_PREFIX, NULL, 0);
		}

		return;
	}

	/* withdraw prefix */
	if (p != NULL)
		prefix_adjout_withdraw(p);
}

/*
 * Generate updates for the add-path send case. Depending on the
 * peer eval settings prefixes are selected and distributed.
 * This highly depends on the Adj-RIB-Out to handle prefixes with no
 * changes gracefully. It may be possible to improve the API so that
 * less churn is needed.
 */
void
up_generate_addpath(struct filter_head *rules, struct rde_peer *peer,
    struct prefix *new, struct prefix *old)
{
	struct filterstate	state;
	struct bgpd_addr	addr;
	struct prefix		*head, *p;
	uint8_t			prefixlen;
	int			maxpaths = 0, extrapaths = 0, extra;
	int			checkmode = 1;

	if (new == NULL) {
		pt_getaddr(old->pt, &addr);
		prefixlen = old->pt->prefixlen;
	} else {
		pt_getaddr(new->pt, &addr);
		prefixlen = new->pt->prefixlen;
	}

	head = prefix_adjout_lookup(peer, &addr, prefixlen);

	/* mark all paths as stale */
	for (p = head; p != NULL; p = prefix_adjout_next(peer, p))
		p->flags |= PREFIX_FLAG_STALE;

	/* update paths */
	for ( ; new != NULL; new = TAILQ_NEXT(new, entry.list.rib)) {
		/* since list is sorted, stop at first invalid prefix */
		if (!prefix_eligible(new))
			break;

		/* check limits and stop when a limit is reached */
		if (peer->eval.maxpaths != 0 &&
		    maxpaths >= peer->eval.maxpaths)
			break;
		if (peer->eval.extrapaths != 0 &&
		    extrapaths >= peer->eval.extrapaths)
			break;

		extra = 1;
		if (checkmode) {
			switch (peer->eval.mode) {
			case ADDPATH_EVAL_BEST:
				if (new->dmetric == PREFIX_DMETRIC_BEST)
					extra = 0;
				else
					checkmode = 0;
				break;
			case ADDPATH_EVAL_ECMP:
				if (new->dmetric == PREFIX_DMETRIC_BEST ||
				    new->dmetric == PREFIX_DMETRIC_ECMP)
					extra = 0;
				else
					checkmode = 0;
				break;
			case ADDPATH_EVAL_AS_WIDE:
				if (new->dmetric == PREFIX_DMETRIC_BEST ||
				    new->dmetric == PREFIX_DMETRIC_ECMP ||
				    new->dmetric == PREFIX_DMETRIC_AS_WIDE)
					extra = 0;
				else
					checkmode = 0;
				break;
			case ADDPATH_EVAL_ALL:
				/* nothing to check */
				checkmode = 0;
				break;
			default:
				fatalx("unknown add-path eval mode");
			}
		}

		/*
		 * up_test_update() needs to run before the output filters
		 * else the well known communities won't work properly.
		 * The output filters would not be able to add well known
		 * communities.
		 */
		if (!up_test_update(peer, new))
			continue;

		rde_filterstate_prep(&state, prefix_aspath(new),
		    prefix_communities(new), prefix_nexthop(new),
		    prefix_nhflags(new));
		if (rde_filter(rules, peer, prefix_peer(new), &addr,
		    prefixlen, prefix_vstate(new), &state) == ACTION_DENY) {
			rde_filterstate_clean(&state);
			continue;
		}

		if (up_enforce_open_policy(peer, &state)) {
			rde_filterstate_clean(&state);
			continue;
		}

		/* from here on we know this is an update */
		maxpaths++;
		extrapaths += extra;

		p = prefix_adjout_get(peer, new->path_id_tx, &addr,
		    new->pt->prefixlen);

		up_prep_adjout(peer, &state, addr.aid);
		prefix_adjout_update(p, peer, &state, &addr,
		    new->pt->prefixlen, new->path_id_tx, prefix_vstate(new));
		rde_filterstate_clean(&state);

		/* max prefix checker outbound */
		if (peer->conf.max_out_prefix &&
		    peer->prefix_out_cnt > peer->conf.max_out_prefix) {
			log_peer_warnx(&peer->conf,
			    "outbound prefix limit reached (>%u/%u)",
			    peer->prefix_out_cnt, peer->conf.max_out_prefix);
			rde_update_err(peer, ERR_CEASE,
			    ERR_CEASE_MAX_SENT_PREFIX, NULL, 0);
		}
	}

	/* withdraw stale paths */
	for (p = head; p != NULL; p = prefix_adjout_next(peer, p)) {
		if (p->flags & PREFIX_FLAG_STALE)
			prefix_adjout_withdraw(p);
	}
}

/*
 * Generate updates for the add-path send all case. Since all prefixes
 * are distributed just remove old and add new.
 */ 
void
up_generate_addpath_all(struct filter_head *rules, struct rde_peer *peer,
    struct prefix *best, struct prefix *new, struct prefix *old)
{
	struct filterstate	state;
	struct bgpd_addr	addr;
	struct prefix		*p, *next, *head = NULL;
	uint8_t			prefixlen;
	int			all = 0;

	/*
	 * if old and new are NULL then insert all prefixes from best,
	 * clearing old routes in the process
	 */
	if (old == NULL && new == NULL) {
		/* mark all paths as stale */
		pt_getaddr(best->pt, &addr);
		prefixlen = best->pt->prefixlen;

		head = prefix_adjout_lookup(peer, &addr, prefixlen);
		for (p = head; p != NULL; p = prefix_adjout_next(peer, p))
			p->flags |= PREFIX_FLAG_STALE;

		new = best;
		all = 1;
	}

	if (old != NULL) {
		/* withdraw stale paths */
		pt_getaddr(old->pt, &addr);
		p = prefix_adjout_get(peer, old->path_id_tx, &addr,
		    old->pt->prefixlen);
		if (p != NULL)
			prefix_adjout_withdraw(p);
	}

	if (new != NULL) {
		pt_getaddr(new->pt, &addr);
		prefixlen = new->pt->prefixlen;
	}

	/* add new path (or multiple if all is set) */
	for (; new != NULL; new = next) {
		if (all)
			next = TAILQ_NEXT(new, entry.list.rib);
		else
			next = NULL;

		/* only allow valid prefixes */
		if (!prefix_eligible(new))
			break;

		/*
		 * up_test_update() needs to run before the output filters
		 * else the well known communities won't work properly.
		 * The output filters would not be able to add well known
		 * communities.
		 */
		if (!up_test_update(peer, new))
			continue;

		rde_filterstate_prep(&state, prefix_aspath(new),
		    prefix_communities(new), prefix_nexthop(new),
		    prefix_nhflags(new));
		if (rde_filter(rules, peer, prefix_peer(new), &addr,
		    prefixlen, prefix_vstate(new), &state) == ACTION_DENY) {
			rde_filterstate_clean(&state);
			continue;
		}

		if (up_enforce_open_policy(peer, &state)) {
			rde_filterstate_clean(&state);
			continue;
		}

		/* from here on we know this is an update */
		p = prefix_adjout_get(peer, new->path_id_tx, &addr, prefixlen);

		up_prep_adjout(peer, &state, addr.aid);
		prefix_adjout_update(p, peer, &state, &addr,
		    prefixlen, new->path_id_tx, prefix_vstate(new));
		rde_filterstate_clean(&state);

		/* max prefix checker outbound */
		if (peer->conf.max_out_prefix &&
		    peer->prefix_out_cnt > peer->conf.max_out_prefix) {
			log_peer_warnx(&peer->conf,
			    "outbound prefix limit reached (>%u/%u)",
			    peer->prefix_out_cnt, peer->conf.max_out_prefix);
			rde_update_err(peer, ERR_CEASE,
			    ERR_CEASE_MAX_SENT_PREFIX, NULL, 0);
		}
	}

	if (all) {
		/* withdraw stale paths */
		for (p = head; p != NULL; p = prefix_adjout_next(peer, p)) {
			if (p->flags & PREFIX_FLAG_STALE)
				prefix_adjout_withdraw(p);
		}
	}
}

struct rib_entry *rib_add(struct rib *, struct bgpd_addr *, int);
void rib_remove(struct rib_entry *);
int rib_empty(struct rib_entry *);

/* send a default route to the specified peer */
void
up_generate_default(struct filter_head *rules, struct rde_peer *peer,
    uint8_t aid)
{
	extern struct rde_peer	*peerself;
	struct filterstate	 state;
	struct rde_aspath	*asp;
	struct prefix		*p;
	struct bgpd_addr	 addr;

	if (peer->capa.mp[aid] == 0)
		return;

	rde_filterstate_prep(&state, NULL, NULL, NULL, 0);
	asp = &state.aspath;
	asp->aspath = aspath_get(NULL, 0);
	asp->origin = ORIGIN_IGP;
	/* the other default values are OK, nexthop is once again NULL */

	/*
	 * XXX apply default overrides. Not yet possible, mainly a parse.y
	 * problem.
	 */
	/* rde_apply_set(asp, peerself, peerself, set, af); */

	memset(&addr, 0, sizeof(addr));
	addr.aid = aid;
	p = prefix_adjout_lookup(peer, &addr, 0);

	/* outbound filter as usual */
	if (rde_filter(rules, peer, peerself, &addr, 0, ROA_NOTFOUND,
	    &state) == ACTION_DENY) {
		rde_filterstate_clean(&state);
		return;
	}

	up_prep_adjout(peer, &state, addr.aid);
	prefix_adjout_update(p, peer, &state, &addr, 0, 0, ROA_NOTFOUND);
	rde_filterstate_clean(&state);

	/* max prefix checker outbound */
	if (peer->conf.max_out_prefix &&
	    peer->prefix_out_cnt > peer->conf.max_out_prefix) {
		log_peer_warnx(&peer->conf,
		    "outbound prefix limit reached (>%u/%u)",
		    peer->prefix_out_cnt, peer->conf.max_out_prefix);
		rde_update_err(peer, ERR_CEASE,
		    ERR_CEASE_MAX_SENT_PREFIX, NULL, 0);
	}
}

static struct bgpd_addr *
up_get_nexthop(struct rde_peer *peer, struct filterstate *state, uint8_t aid)
{
	struct bgpd_addr *peer_local;

	switch (aid) {
	case AID_INET:
	case AID_VPN_IPv4:
		peer_local = &peer->local_v4_addr;
		break;
	case AID_INET6:
	case AID_VPN_IPv6:
		peer_local = &peer->local_v6_addr;
		break;
	default:
		fatalx("%s, bad AID %s", __func__, aid2str(aid));
	}

	if (state->nhflags & NEXTHOP_SELF) {
		/*
		 * Forcing the nexthop to self is always possible
		 * and has precedence over other flags.
		 */
		return (peer_local);
	} else if (!peer->conf.ebgp) {
		/*
		 * in the ibgp case the nexthop is normally not
		 * modified unless it points at the peer itself.
		 */
		if (state->nexthop == NULL) {
			/* announced networks without explicit nexthop set */
			return (peer_local);
		}
		/*
		 * per RFC: if remote peer address is equal to the nexthop set
		 * the nexthop to our local address. This reduces the risk of
		 * routing loops. This overrides NEXTHOP_NOMODIFY.
		 */
		if (memcmp(&state->nexthop->exit_nexthop,
		    &peer->remote_addr, sizeof(peer->remote_addr)) == 0) {
			return (peer_local);
		}
		return (&state->nexthop->exit_nexthop);
	} else if (peer->conf.distance == 1) {
		/*
		 * In the ebgp directly connected case never send
		 * out a nexthop that is outside of the connected
		 * network of the peer. No matter what flags are
		 * set. This follows section 5.1.3 of RFC 4271.
		 * So just check if the nexthop is in the same net
		 * is enough here.
		 */
		if (state->nexthop != NULL &&
		    state->nexthop->flags & NEXTHOP_CONNECTED &&
		    prefix_compare(&peer->remote_addr,
		    &state->nexthop->nexthop_net,
		    state->nexthop->nexthop_netlen) == 0) {
			/* nexthop and peer are in the same net */
			return (&state->nexthop->exit_nexthop);
		}
		return (peer_local);
	} else {
		/*
		 * For ebgp multihop make it possible to overrule
		 * the sent nexthop by setting NEXTHOP_NOMODIFY.
		 * Similar to the ibgp case there is no same net check
		 * needed but still ensure that the nexthop is not
		 * pointing to the peer itself.
		 */
		if (state->nhflags & NEXTHOP_NOMODIFY &&
		    state->nexthop != NULL &&
		    memcmp(&state->nexthop->exit_nexthop,
		    &peer->remote_addr, sizeof(peer->remote_addr)) != 0) {
			/* no modify flag set and nexthop not peer addr */
			return (&state->nexthop->exit_nexthop);
		}
		return (peer_local);
	}
}

static void
up_prep_adjout(struct rde_peer *peer, struct filterstate *state, uint8_t aid)
{
	struct bgpd_addr *nexthop;
	struct nexthop *nh;
	u_char *np;
	uint16_t nl;

	/* prepend local AS number for eBGP sessions. */
	if (peer->conf.ebgp && (peer->flags & PEERFLAG_TRANS_AS) == 0) {
		uint32_t prep_as = peer->conf.local_as;
		np = aspath_prepend(state->aspath.aspath, prep_as, 1, &nl);
		aspath_put(state->aspath.aspath);
		state->aspath.aspath = aspath_get(np, nl);
		free(np);
	}

	/* update nexthop */
	nexthop = up_get_nexthop(peer, state, aid);
	nh = nexthop_get(nexthop);
	nexthop_unref(state->nexthop);
	state->nexthop = nh;
	state->nhflags = 0;
}


static int
up_generate_attr(u_char *buf, int len, struct rde_peer *peer,
    struct filterstate *state, uint8_t aid)
{
	struct rde_aspath *asp = &state->aspath;
	struct rde_community *comm = &state->communities;
	struct attr	*oa = NULL, *newaggr = NULL;
	u_char		*pdata;
	uint32_t	 tmp32;
	struct bgpd_addr *nexthop;
	int		 flags, r, neednewpath = 0;
	uint16_t	 wlen = 0, plen;
	uint8_t		 oalen = 0, type;

	if (asp->others_len > 0)
		oa = asp->others[oalen++];

	/* dump attributes in ascending order */
	for (type = ATTR_ORIGIN; type < 255; type++) {
		r = 0;

		while (oa && oa->type < type) {
			if (oalen < asp->others_len)
				oa = asp->others[oalen++];
			else
				oa = NULL;
		}

		switch (type) {
		/*
		 * Attributes stored in rde_aspath
		 */
		case ATTR_ORIGIN:
			if ((r = attr_write(buf + wlen, len, ATTR_WELL_KNOWN,
			    ATTR_ORIGIN, &asp->origin, 1)) == -1)
				return (-1);
			break;
		case ATTR_ASPATH:
			plen = aspath_length(asp->aspath);
			pdata = aspath_dump(asp->aspath);

			if (!peer_has_as4byte(peer))
				pdata = aspath_deflate(pdata, &plen,
				    &neednewpath);

			if ((r = attr_write(buf + wlen, len, ATTR_WELL_KNOWN,
			    ATTR_ASPATH, pdata, plen)) == -1)
				return (-1);
			if (!peer_has_as4byte(peer))
				free(pdata);
			break;
		case ATTR_NEXTHOP:
			switch (aid) {
			case AID_INET:
				nexthop = &state->nexthop->exit_nexthop;
				if ((r = attr_write(buf + wlen, len,
				    ATTR_WELL_KNOWN, ATTR_NEXTHOP,
				    &nexthop->v4.s_addr, 4)) == -1)
					return (-1);
				break;
			default:
				break;
			}
			break;
		case ATTR_MED:
			/*
			 * The old MED from other peers MUST not be announced
			 * to others unless the MED is originating from us or
			 * the peer is an IBGP one. Only exception are routers
			 * with "transparent-as yes" set.
			 */
			if (asp->flags & F_ATTR_MED && (!peer->conf.ebgp ||
			    asp->flags & F_ATTR_MED_ANNOUNCE ||
			    peer->flags & PEERFLAG_TRANS_AS)) {
				tmp32 = htonl(asp->med);
				if ((r = attr_write(buf + wlen, len,
				    ATTR_OPTIONAL, ATTR_MED, &tmp32, 4)) == -1)
					return (-1);
			}
			break;
		case ATTR_LOCALPREF:
			if (!peer->conf.ebgp) {
				/* local preference, only valid for ibgp */
				tmp32 = htonl(asp->lpref);
				if ((r = attr_write(buf + wlen, len,
				    ATTR_WELL_KNOWN, ATTR_LOCALPREF, &tmp32,
				    4)) == -1)
					return (-1);
			}
			break;
		/*
		 * Communities are stored in struct rde_community
		 */
		case ATTR_COMMUNITIES:
			if ((r = community_write(comm, buf + wlen, len)) == -1)
				return (-1);
			break;
		case ATTR_EXT_COMMUNITIES:
			if ((r = community_ext_write(comm, peer->conf.ebgp,
			    buf + wlen, len)) == -1)
				return (-1);
			break;
		case ATTR_LARGE_COMMUNITIES:
			if ((r = community_large_write(comm, buf + wlen,
			    len)) == -1)
				return (-1);
			break;
		/*
		 * NEW to OLD conversion when sending stuff to a 2byte AS peer
		 */
		case ATTR_AS4_PATH:
			if (neednewpath) {
				plen = aspath_length(asp->aspath);
				pdata = aspath_dump(asp->aspath);

				flags = ATTR_OPTIONAL|ATTR_TRANSITIVE;
				if (!(asp->flags & F_PREFIX_ANNOUNCED))
					flags |= ATTR_PARTIAL;
				if (plen == 0)
					r = 0;
				else if ((r = attr_write(buf + wlen, len, flags,
				    ATTR_AS4_PATH, pdata, plen)) == -1)
					return (-1);
			}
			break;
		case ATTR_AS4_AGGREGATOR:
			if (newaggr) {
				flags = ATTR_OPTIONAL|ATTR_TRANSITIVE;
				if (!(asp->flags & F_PREFIX_ANNOUNCED))
					flags |= ATTR_PARTIAL;
				if ((r = attr_write(buf + wlen, len, flags,
				    ATTR_AS4_AGGREGATOR, newaggr->data,
				    newaggr->len)) == -1)
					return (-1);
			}
			break;
		/*
		 * multiprotocol attributes are handled elsewhere
		 */
		case ATTR_MP_REACH_NLRI:
		case ATTR_MP_UNREACH_NLRI:
			break;
		/*
		 * dump all other path attributes. Following rules apply:
		 *  1. well-known attrs: ATTR_ATOMIC_AGGREGATE and
		 *     ATTR_AGGREGATOR pass unmodified (enforce flags
		 *     to correct values). Actually ATTR_AGGREGATOR may be
		 *     deflated for OLD 2-byte peers.
		 *  2. non-transitive attrs: don't re-announce to ebgp peers
		 *  3. transitive known attrs: announce unmodified
		 *  4. transitive unknown attrs: set partial bit and re-announce
		 */
		case ATTR_ATOMIC_AGGREGATE:
			if (oa == NULL || oa->type != type)
				break;
			if ((r = attr_write(buf + wlen, len,
			    ATTR_WELL_KNOWN, ATTR_ATOMIC_AGGREGATE,
			    NULL, 0)) == -1)
				return (-1);
			break;
		case ATTR_AGGREGATOR:
			if (oa == NULL || oa->type != type)
				break;
			if (!peer_has_as4byte(peer)) {
				/* need to deflate the aggregator */
				uint8_t	t[6];
				uint16_t	tas;

				if ((!(oa->flags & ATTR_TRANSITIVE)) &&
				    peer->conf.ebgp) {
					r = 0;
					break;
				}

				memcpy(&tmp32, oa->data, sizeof(tmp32));
				if (ntohl(tmp32) > USHRT_MAX) {
					tas = htons(AS_TRANS);
					newaggr = oa;
				} else
					tas = htons(ntohl(tmp32));

				memcpy(t, &tas, sizeof(tas));
				memcpy(t + sizeof(tas),
				    oa->data + sizeof(tmp32),
				    oa->len - sizeof(tmp32));
				if ((r = attr_write(buf + wlen, len,
				    oa->flags, oa->type, &t, sizeof(t))) == -1)
					return (-1);
				break;
			}
			/* FALLTHROUGH */
		case ATTR_ORIGINATOR_ID:
		case ATTR_CLUSTER_LIST:
		case ATTR_OTC:
			if (oa == NULL || oa->type != type)
				break;
			if ((!(oa->flags & ATTR_TRANSITIVE)) &&
			    peer->conf.ebgp) {
				r = 0;
				break;
			}
			if ((r = attr_write(buf + wlen, len,
			    oa->flags, oa->type, oa->data, oa->len)) == -1)
				return (-1);
			break;
		default:
			if (oa == NULL && type >= ATTR_FIRST_UNKNOWN)
				/* there is no attribute left to dump */
				goto done;

			if (oa == NULL || oa->type != type)
				break;
			/* unknown attribute */
			if (!(oa->flags & ATTR_TRANSITIVE)) {
				/*
				 * RFC 1771:
				 * Unrecognized non-transitive optional
				 * attributes must be quietly ignored and
				 * not passed along to other BGP peers.
				 */
				break;
			}
			if ((r = attr_write(buf + wlen, len,
			    oa->flags | ATTR_PARTIAL, oa->type,
			    oa->data, oa->len)) == -1)
				return (-1);
		}
		wlen += r;
		len -= r;
	}
done:
	return (wlen);
}

/*
 * Check if the pending element is a EoR marker. If so remove it from the
 * tree and return 1.
 */
int
up_is_eor(struct rde_peer *peer, uint8_t aid)
{
	struct prefix *p;

	p = RB_MIN(prefix_tree, &peer->updates[aid]);
	if (p != NULL && (p->flags & PREFIX_FLAG_EOR)) {
		/*
		 * Need to remove eor from update tree because
		 * prefix_adjout_destroy() can't handle that.
		 */
		RB_REMOVE(prefix_tree, &peer->updates[aid], p);
		p->flags &= ~PREFIX_FLAG_UPDATE;
		prefix_adjout_destroy(p);
		return 1;
	}
	return 0;
}

/* minimal buffer size > withdraw len + attr len + attr hdr + afi/safi */
#define MIN_UPDATE_LEN	16

/*
 * Write prefixes to buffer until either there is no more space or
 * the next prefix has no longer the same ASPATH attributes.
 */
static int
up_dump_prefix(u_char *buf, int len, struct prefix_tree *prefix_head,
    struct rde_peer *peer, int withdraw)
{
	struct prefix	*p, *np;
	struct bgpd_addr addr;
	uint32_t	 pathid;
	int		 r, wpos = 0, done = 0;

	RB_FOREACH_SAFE(p, prefix_tree, prefix_head, np) {
		if (peer_has_add_path(peer, p->pt->aid, CAPA_AP_SEND)) {
			if (len <= wpos + (int)sizeof(pathid))
				break;
			pathid = htonl(p->path_id_tx);
			memcpy(buf + wpos, &pathid, sizeof(pathid));
			wpos += sizeof(pathid);
		}
		pt_getaddr(p->pt, &addr);
		if ((r = prefix_write(buf + wpos, len - wpos,
		    &addr, p->pt->prefixlen, withdraw)) == -1) {
			if (peer_has_add_path(peer, p->pt->aid, CAPA_AP_SEND))
				wpos -= sizeof(pathid);
			break;
		}
		wpos += r;

		/* make sure we only dump prefixes which belong together */
		if (np == NULL ||
		    np->aspath != p->aspath ||
		    np->communities != p->communities ||
		    np->nexthop != p->nexthop ||
		    np->nhflags != p->nhflags ||
		    (np->flags & PREFIX_FLAG_EOR))
			done = 1;

		if (withdraw) {
			/* prefix no longer needed, remove it */
			prefix_adjout_destroy(p);
			peer->prefix_sent_withdraw++;
		} else {
			/* prefix still in Adj-RIB-Out, keep it */
			RB_REMOVE(prefix_tree, prefix_head, p);
			p->flags &= ~PREFIX_FLAG_UPDATE;
			peer->up_nlricnt--;
			peer->prefix_sent_update++;
		}

		if (done)
			break;
	}
	return (wpos);
}

int
up_dump_withdraws(u_char *buf, int len, struct rde_peer *peer, uint8_t aid)
{
	uint16_t wpos, wd_len;
	int r;

	if (len < MIN_UPDATE_LEN)
		return (-1);

	/* reserve space for the length field */
	wpos = 2;
	r = up_dump_prefix(buf + wpos, len - wpos, &peer->withdraws[aid],
	    peer, 1);
	wd_len = htons(r);
	memcpy(buf, &wd_len, 2);

	return (wpos + r);
}

int
up_dump_mp_unreach(u_char *buf, int len, struct rde_peer *peer, uint8_t aid)
{
	u_char		*attrbuf;
	int		 wpos, r;
	uint16_t	 attr_len, tmp;

	if (len < MIN_UPDATE_LEN || RB_EMPTY(&peer->withdraws[aid]))
		return (-1);

	/* reserve space for withdraw len, attr len */
	wpos = 2 + 2;
	attrbuf = buf + wpos;

	/* attribute header, defaulting to extended length one */
	attrbuf[0] = ATTR_OPTIONAL | ATTR_EXTLEN;
	attrbuf[1] = ATTR_MP_UNREACH_NLRI;
	wpos += 4;

	/* afi & safi */
	if (aid2afi(aid, &tmp, buf + wpos + 2))
		fatalx("up_dump_mp_unreach: bad AID");
	tmp = htons(tmp);
	memcpy(buf + wpos, &tmp, sizeof(uint16_t));
	wpos += 3;

	r = up_dump_prefix(buf + wpos, len - wpos, &peer->withdraws[aid],
	    peer, 1);
	if (r == 0)
		return (-1);
	wpos += r;
	attr_len = r + 3;	/* prefixes + afi & safi */

	/* attribute length */
	attr_len = htons(attr_len);
	memcpy(attrbuf + 2, &attr_len, sizeof(attr_len));

	/* write length fields */
	memset(buf, 0, sizeof(uint16_t));	/* withdrawn routes len */
	attr_len = htons(wpos - 4);
	memcpy(buf + 2, &attr_len, sizeof(attr_len));

	return (wpos);
}

int
up_dump_attrnlri(u_char *buf, int len, struct rde_peer *peer)
{
	struct filterstate	 state;
	struct prefix		*p;
	int			 r, wpos;
	uint16_t		 attr_len;

	if (len < 2)
		fatalx("up_dump_attrnlri: buffer way too small");
	if (len < MIN_UPDATE_LEN)
		goto done;

	p = RB_MIN(prefix_tree, &peer->updates[AID_INET]);
	if (p == NULL)
		goto done;

	rde_filterstate_prep(&state, prefix_aspath(p), prefix_communities(p),
	    prefix_nexthop(p), prefix_nhflags(p));

	r = up_generate_attr(buf + 2, len - 2, peer, &state, AID_INET);
	rde_filterstate_clean(&state);
	if (r == -1) {
		/*
		 * either no packet or not enough space.
		 * The length field needs to be set to zero else it would be
		 * an invalid bgp update.
		 */
done:
		memset(buf, 0, 2);
		return (2);
	}

	/* first dump the 2-byte path attribute length */
	attr_len = htons(r);
	memcpy(buf, &attr_len, 2);
	wpos = 2;
	/* then skip over the already dumped path attributes themselves */
	wpos += r;

	/* last but not least dump the nlri */
	r = up_dump_prefix(buf + wpos, len - wpos, &peer->updates[AID_INET],
	    peer, 0);
	wpos += r;

	return (wpos);
}

static int
up_generate_mp_reach(u_char *buf, int len, struct rde_peer *peer,
    struct filterstate *state, uint8_t aid)
{
	struct bgpd_addr	*nexthop;
	u_char			*attrbuf;
	int			 r, wpos, attrlen;
	uint16_t		 tmp;

	if (len < 4)
		return (-1);
	/* attribute header, defaulting to extended length one */
	buf[0] = ATTR_OPTIONAL | ATTR_EXTLEN;
	buf[1] = ATTR_MP_REACH_NLRI;
	wpos = 4;
	attrbuf = buf + wpos;

	switch (aid) {
	case AID_INET6:
		attrlen = 21; /* AFI + SAFI + NH LEN + NH + Reserved */
		if (len < wpos + attrlen)
			return (-1);
		wpos += attrlen;
		if (aid2afi(aid, &tmp, &attrbuf[2]))
			fatalx("up_generate_mp_reach: bad AID");
		tmp = htons(tmp);
		memcpy(attrbuf, &tmp, sizeof(tmp));
		attrbuf[3] = sizeof(struct in6_addr);
		attrbuf[20] = 0; /* Reserved must be 0 */

		/* write nexthop */
		attrbuf += 4;
		nexthop = &state->nexthop->exit_nexthop;
		memcpy(attrbuf, &nexthop->v6, sizeof(struct in6_addr));
		break;
	case AID_VPN_IPv4:
		attrlen = 17; /* AFI + SAFI + NH LEN + NH + Reserved */
		if (len < wpos + attrlen)
			return (-1);
		wpos += attrlen;
		if (aid2afi(aid, &tmp, &attrbuf[2]))
			fatalx("up_generate_mp_reachi: bad AID");
		tmp = htons(tmp);
		memcpy(attrbuf, &tmp, sizeof(tmp));
		attrbuf[3] = sizeof(uint64_t) + sizeof(struct in_addr);
		memset(attrbuf + 4, 0, sizeof(uint64_t));
		attrbuf[16] = 0; /* Reserved must be 0 */

		/* write nexthop */
		attrbuf += 12;
		nexthop = &state->nexthop->exit_nexthop;
		memcpy(attrbuf, &nexthop->v4, sizeof(struct in_addr));
		break;
	case AID_VPN_IPv6:
		attrlen = 29; /* AFI + SAFI + NH LEN + NH + Reserved */
		if (len < wpos + attrlen)
			return (-1);
		wpos += attrlen;
		if (aid2afi(aid, &tmp, &attrbuf[2]))
			fatalx("up_generate_mp_reachi: bad AID");
		tmp = htons(tmp);
		memcpy(attrbuf, &tmp, sizeof(tmp));
		attrbuf[3] = sizeof(uint64_t) + sizeof(struct in6_addr);
		memset(attrbuf + 4, 0, sizeof(uint64_t));
		attrbuf[28] = 0; /* Reserved must be 0 */

		/* write nexthop */
		attrbuf += 12;
		nexthop = &state->nexthop->exit_nexthop;
		memcpy(attrbuf, &nexthop->v6, sizeof(struct in6_addr));
		break;
	default:
		fatalx("up_generate_mp_reach: unknown AID");
	}

	r = up_dump_prefix(buf + wpos, len - wpos, &peer->updates[aid],
	    peer, 0);
	if (r == 0) {
		/* no prefixes written ... */
		return (-1);
	}
	attrlen += r;
	wpos += r;
	/* update attribute length field */
	tmp = htons(attrlen);
	memcpy(buf + 2, &tmp, sizeof(tmp));

	return (wpos);
}

int
up_dump_mp_reach(u_char *buf, int len, struct rde_peer *peer, uint8_t aid)
{
	struct filterstate	 state;
	struct prefix		*p;
	int			r, wpos;
	uint16_t		attr_len;

	if (len < MIN_UPDATE_LEN)
		return 0;

	/* get starting point */
	p = RB_MIN(prefix_tree, &peer->updates[aid]);
	if (p == NULL)
		return 0;

	wpos = 4;	/* reserve space for length fields */

	rde_filterstate_prep(&state, prefix_aspath(p), prefix_communities(p),
	    prefix_nexthop(p), prefix_nhflags(p));

	/* write regular path attributes */
	r = up_generate_attr(buf + wpos, len - wpos, peer, &state, aid);
	if (r == -1) {
		rde_filterstate_clean(&state);
		return 0;
	}
	wpos += r;

	/* write mp attribute */
	r = up_generate_mp_reach(buf + wpos, len - wpos, peer, &state, aid);
	rde_filterstate_clean(&state);
	if (r == -1)
		return 0;
	wpos += r;

	/* write length fields */
	memset(buf, 0, sizeof(uint16_t));	/* withdrawn routes len */
	attr_len = htons(wpos - 4);
	memcpy(buf + 2, &attr_len, sizeof(attr_len));

	return (wpos);
}
