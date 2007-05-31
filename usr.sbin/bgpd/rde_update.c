/*	$OpenBSD: rde_update.c,v 1.60 2007/05/31 04:27:00 claudio Exp $ */

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
#include <sys/hash.h>

#include <stdlib.h>
#include <string.h>

#include "bgpd.h"
#include "rde.h"

in_addr_t	up_get_nexthop(struct rde_peer *, struct rde_aspath *);
int		up_generate_mp_reach(struct rde_peer *, struct update_attr *,
		    struct rde_aspath *, sa_family_t);
int		up_generate_attr(struct rde_peer *, struct update_attr *,
		    struct rde_aspath *, sa_family_t);

/* update stuff. */
struct update_prefix {
	TAILQ_ENTRY(update_prefix)	 prefix_l;
	RB_ENTRY(update_prefix)		 entry;
	struct uplist_prefix		*prefix_h;
	struct bgpd_addr		 prefix;
	int				 prefixlen;
};

struct update_attr {
	TAILQ_ENTRY(update_attr)	 attr_l;
	RB_ENTRY(update_attr)		 entry;
	struct uplist_prefix		 prefix_h;
	u_char				*attr;
	u_char				*mpattr;
	u_int32_t			 attr_hash;
	u_int16_t			 attr_len;
	u_int16_t			 mpattr_len;
};

void	up_clear(struct uplist_attr *, struct uplist_prefix *);
int	up_prefix_cmp(struct update_prefix *, struct update_prefix *);
int	up_attr_cmp(struct update_attr *, struct update_attr *);
int	up_add(struct rde_peer *, struct update_prefix *, struct update_attr *);

RB_PROTOTYPE(uptree_prefix, update_prefix, entry, up_prefix_cmp)
RB_GENERATE(uptree_prefix, update_prefix, entry, up_prefix_cmp)

RB_PROTOTYPE(uptree_attr, update_attr, entry, up_attr_cmp)
RB_GENERATE(uptree_attr, update_attr, entry, up_attr_cmp)

void
up_init(struct rde_peer *peer)
{
	TAILQ_INIT(&peer->updates);
	TAILQ_INIT(&peer->withdraws);
	TAILQ_INIT(&peer->updates6);
	TAILQ_INIT(&peer->withdraws6);
	RB_INIT(&peer->up_prefix);
	RB_INIT(&peer->up_attrs);
	peer->up_pcnt = 0;
	peer->up_acnt = 0;
	peer->up_nlricnt = 0;
	peer->up_wcnt = 0;
}

void
up_clear(struct uplist_attr *updates, struct uplist_prefix *withdraws)
{
	struct update_attr	*ua;
	struct update_prefix	*up;

	while ((ua = TAILQ_FIRST(updates)) != NULL) {
		TAILQ_REMOVE(updates, ua, attr_l);
		while ((up = TAILQ_FIRST(&ua->prefix_h)) != NULL) {
			TAILQ_REMOVE(&ua->prefix_h, up, prefix_l);
			free(up);
		}
		free(ua->attr);
		free(ua->mpattr);
		free(ua);
	}

	while ((up = TAILQ_FIRST(withdraws)) != NULL) {
		TAILQ_REMOVE(withdraws, up, prefix_l);
		free(up);
	}
}

void
up_down(struct rde_peer *peer)
{
	up_clear(&peer->updates, &peer->withdraws);
	up_clear(&peer->updates6, &peer->withdraws6);

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
	int	i;

	if (a->prefix.af < b->prefix.af)
		return (-1);
	if (a->prefix.af > b->prefix.af)
		return (1);

	switch (a->prefix.af) {
	case AF_INET:
		if (ntohl(a->prefix.v4.s_addr) < ntohl(b->prefix.v4.s_addr))
			return (-1);
		if (ntohl(a->prefix.v4.s_addr) > ntohl(b->prefix.v4.s_addr))
			return (1);
		break;
	case AF_INET6:
		i = memcmp(&a->prefix.v6, &b->prefix.v6,
		    sizeof(struct in6_addr));
		if (i > 0)
			return (1);
		if (i < 0)
			return (-1);
		break;
	default:
		fatalx("pt_prefix_cmp: unknown af");
	}
	if (a->prefixlen < b->prefixlen)
		return (-1);
	if (a->prefixlen > b->prefixlen)
		return (1);
	return (0);
}

int
up_attr_cmp(struct update_attr *a, struct update_attr *b)
{
	int	r;

	if ((r = a->attr_hash - b->attr_hash) != 0)
		return (r);
	if ((r = a->attr_len - b->attr_len) != 0)
		return (r);
	if ((r = a->mpattr_len - b->mpattr_len) != 0)
		return (r);
	if ((r = memcmp(a->mpattr, b->mpattr, a->mpattr_len)) != 0)
		return (r);
	return (memcmp(a->attr, b->attr, a->attr_len));
}

int
up_add(struct rde_peer *peer, struct update_prefix *p, struct update_attr *a)
{
	struct update_attr	*na = NULL;
	struct update_prefix	*np;
	struct uplist_attr	*upl = NULL;
	struct uplist_prefix	*wdl = NULL;

	switch (p->prefix.af) {
	case AF_INET:
		upl = &peer->updates;
		wdl = &peer->withdraws;
		break;
	case AF_INET6:
		upl = &peer->updates6;
		wdl = &peer->withdraws6;
		break;
	default:
		fatalx("up_add: unknown AF");
	}

	/* 1. search for attr */
	if (a != NULL && (na = RB_FIND(uptree_attr, &peer->up_attrs, a)) ==
	    NULL) {
		/* 1.1 if not found -> add */
		TAILQ_INIT(&a->prefix_h);
		if (RB_INSERT(uptree_attr, &peer->up_attrs, a) != NULL) {
			log_warnx("uptree_attr insert failed");
			/* cleanup */
			free(a->attr);
			free(a->mpattr);
			free(a);
			free(p);
			return (-1);
		}
		TAILQ_INSERT_TAIL(upl, a, attr_l);
		peer->up_acnt++;
	} else {
		/* 1.2 if found -> use that, free a */
		if (a != NULL) {
			free(a->attr);
			free(a->mpattr);
			free(a);
			a = na;
			/* move to end of update queue */
			TAILQ_REMOVE(upl, a, attr_l);
			TAILQ_INSERT_TAIL(upl, a, attr_l);
		}
	}

	/* 2. search for prefix */
	if ((np = RB_FIND(uptree_prefix, &peer->up_prefix, p)) == NULL) {
		/* 2.1 if not found -> add */
		if (RB_INSERT(uptree_prefix, &peer->up_prefix, p) != NULL) {
			log_warnx("uptree_prefix insert failed");
			/*
			 * cleanup. But do not free a because it is already
			 * linked or NULL. up_dump_attrnlri() will remove and
			 * free the empty attribute later.
			 */
			free(p);
			return (-1);
		}
		peer->up_pcnt++;
	} else {
		/* 2.2 if found -> use that and free p */
		TAILQ_REMOVE(np->prefix_h, np, prefix_l);
		free(p);
		p = np;
		if (p->prefix_h == wdl)
			peer->up_wcnt--;
		else
			peer->up_nlricnt--;
	}
	/* 3. link prefix to attr */
	if (a == NULL) {
		TAILQ_INSERT_TAIL(wdl, p, prefix_l);
		p->prefix_h = wdl;
		peer->up_wcnt++;
	} else {
		TAILQ_INSERT_TAIL(&a->prefix_h, p, prefix_l);
		p->prefix_h = &a->prefix_h;
		peer->up_nlricnt++;
	}
	return (0);
}

int
up_test_update(struct rde_peer *peer, struct prefix *p)
{
	struct bgpd_addr	 addr;
	struct attr		*attr;

	if (peer->state != PEER_UP)
		return (-1);

	if (p == NULL)
		/* no prefix available */
		return (0);

	if (peer == p->aspath->peer)
		/* Do not send routes back to sender */
		return (0);

	pt_getaddr(p->prefix, &addr);
	switch (addr.af) {
	case AF_INET:
		if (peer->capa_announced.mp_v4 == SAFI_NONE &&
		    peer->capa_received.mp_v6 != SAFI_NONE)
			return (-1);
		break;
	case AF_INET6:
		if (peer->capa_announced.mp_v6 == SAFI_NONE)
			return (-1);
		break;
	}

	if (p->aspath->peer->conf.ebgp == 0 && peer->conf.ebgp == 0) {
		/*
		 * route reflector redistribution rules:
		 * 1. if announce is set                -> announce
		 * 2. old non-client, new non-client    -> no
		 * 3. old client, new non-client        -> yes
		 * 4. old non-client, new client        -> yes
		 * 5. old client, new client            -> yes
		 */
		if (p->aspath->peer->conf.reflector_client == 0 &&
		    peer->conf.reflector_client == 0 &&
		    (p->aspath->flags & F_PREFIX_ANNOUNCED) == 0)
			/* Do not redistribute updates to ibgp peers */
			return (0);
	}

	/* announce type handling */
	switch (peer->conf.announce_type) {
	case ANNOUNCE_UNDEF:
	case ANNOUNCE_NONE:
	case ANNOUNCE_DEFAULT_ROUTE:
		/*
		 * no need to withdraw old prefix as this will be
		 * filtered out as well.
		 */
		return (-1);
	case ANNOUNCE_ALL:
		break;
	case ANNOUNCE_SELF:
		/*
		 * pass only prefix that have an aspath count
		 * of zero this is equal to the ^$ regex.
		 */
		if (p->aspath->aspath->ascnt != 0)
			return (0);
		break;
	}

	/* well known communities */
	if (rde_filter_community(p->aspath,
	    COMMUNITY_WELLKNOWN, COMMUNITY_NO_ADVERTISE))
		return (0);
	if (peer->conf.ebgp && rde_filter_community(p->aspath,
	    COMMUNITY_WELLKNOWN, COMMUNITY_NO_EXPORT))
		return (0);
	if (peer->conf.ebgp && rde_filter_community(p->aspath,
	    COMMUNITY_WELLKNOWN, COMMUNITY_NO_EXPSUBCONFED))
		return (0);

	/*
	 * Don't send messages back to originator
	 * this is not specified in the RFC but seems logical.
	 */
	if ((attr = attr_optget(p->aspath, ATTR_ORIGINATOR_ID)) != NULL) {
		if (memcmp(attr->data, &peer->remote_bgpid,
		    sizeof(peer->remote_bgpid)) == 0) {
			/* would cause loop don't send */
			return (-1);
		}
	}

	return (1);
}

int
up_generate(struct rde_peer *peer, struct rde_aspath *asp,
    struct bgpd_addr *addr, u_int8_t prefixlen)
{
	struct update_attr		*ua = NULL;
	struct update_prefix		*up;

	if (asp) {
		ua = calloc(1, sizeof(struct update_attr));
		if (ua == NULL)
			fatal("up_generate_updates");

		if (up_generate_attr(peer, ua, asp, addr->af) == -1) {
			log_warnx("generation of bgp path attributes failed");
			free(ua);
			return (-1);
		}
		/*
		 * use aspath_hash as attr_hash, this may be unoptimal
		 * but currently I don't care.
		 */
		ua->attr_hash = hash32_buf(ua->attr, ua->attr_len, HASHINIT);
		if (ua->mpattr)
			ua->attr_hash = hash32_buf(ua->mpattr, ua->mpattr_len,
			    ua->attr_hash);
	}

	up = calloc(1, sizeof(struct update_prefix));
	if (up == NULL)
		fatal("up_generate_updates");
	up->prefix = *addr;
	up->prefixlen = prefixlen;

	if (up_add(peer, up, ua) == -1)
		return (-1);

	return (0);
}

void
up_generate_updates(struct filter_head *rules, struct rde_peer *peer,
    struct prefix *new, struct prefix *old)
{
	struct rde_aspath		*asp;
	struct bgpd_addr		 addr;

	if (peer->state != PEER_UP)
		return;

	if (new == NULL) {
		if (up_test_update(peer, old) != 1)
			return;

		pt_getaddr(old->prefix, &addr);
		if (rde_filter(NULL, rules, peer, old->aspath, &addr,
		    old->prefix->prefixlen, old->aspath->peer, DIR_OUT) ==
		    ACTION_DENY)
			return;

		/* withdraw prefix */
		up_generate(peer, NULL, &addr, old->prefix->prefixlen);
	} else {
		switch (up_test_update(peer, new)) {
		case 1:
			break;
		case 0:
			up_generate_updates(rules, peer, NULL, old);
			return;
		case -1:
			return;
		}

		pt_getaddr(new->prefix, &addr);
		if (rde_filter(&asp, rules, peer, new->aspath, &addr,
		    new->prefix->prefixlen, new->aspath->peer, DIR_OUT) ==
		    ACTION_DENY) {
			path_put(asp);
			up_generate_updates(rules, peer, NULL, old);
			return;
		}

		/* generate update */
		if (asp != NULL) {
			up_generate(peer, asp, &addr, new->prefix->prefixlen);
			path_put(asp);
		} else
			up_generate(peer, new->aspath, &addr,
			    new->prefix->prefixlen);
	}
}

/* send a default route to the specified peer */
void
up_generate_default(struct filter_head *rules, struct rde_peer *peer,
    sa_family_t af)
{
	struct rde_aspath	*asp, *fasp;
	struct bgpd_addr	 addr;

	if (peer->capa_received.mp_v4 == SAFI_NONE &&
	    peer->capa_received.mp_v6 != SAFI_NONE &&
	    af == AF_INET)
		return;

	if (peer->capa_received.mp_v6 == SAFI_NONE &&
	    af == AF_INET6)
		return;

	asp = path_get();
	asp->aspath = aspath_get(NULL, 0);
	asp->origin = ORIGIN_IGP;
	/* the other default values are OK, nexthop is once again NULL */

	/*
	 * XXX apply default overrides. Not yet possible, mainly a parse.y
	 * problem.
	 */
	/* rde_apply_set(asp, set, af, NULL ???, DIR_IN); */

	/* filter as usual */
	bzero(&addr, sizeof(addr));
	addr.af = af;

	if (rde_filter(&fasp, rules, peer, asp, &addr, 0, NULL, DIR_OUT) ==
	    ACTION_DENY) {
		path_put(fasp);
		path_put(asp);
		return;
	}

	/* generate update */
	if (fasp != NULL)
		up_generate(peer, fasp, &addr, 0);
	else
		up_generate(peer, asp, &addr, 0);

	/* no longer needed */
	path_put(fasp);
	path_put(asp);
}

u_char	up_attr_buf[4096];

/* only for IPv4 */
in_addr_t
up_get_nexthop(struct rde_peer *peer, struct rde_aspath *a)
{
	in_addr_t	mask;

	/* nexthop, already network byte order */
	if (a->flags & F_NEXTHOP_NOMODIFY) {
		/* no modify flag set */
		if (a->nexthop == NULL)
			return (peer->local_v4_addr.v4.s_addr);
		else
			return (a->nexthop->exit_nexthop.v4.s_addr);
	} else if (a->flags & F_NEXTHOP_SELF)
		return (peer->local_v4_addr.v4.s_addr);
	else if (!peer->conf.ebgp) {
		/*
		 * If directly connected use peer->local_v4_addr
		 * this is only true for announced networks.
		 */
		if (a->nexthop == NULL)
			return (peer->local_v4_addr.v4.s_addr);
		else if (a->nexthop->exit_nexthop.v4.s_addr ==
		    peer->remote_addr.v4.s_addr)
			/*
			 * per RFC: if remote peer address is equal to
			 * the nexthop set the nexthop to our local address.
			 * This reduces the risk of routing loops.
			 */
			return (peer->local_v4_addr.v4.s_addr);
		else
			return (a->nexthop->exit_nexthop.v4.s_addr);
	} else if (peer->conf.distance == 1) {
		/* ebgp directly connected */
		if (a->nexthop != NULL &&
		    a->nexthop->flags & NEXTHOP_CONNECTED) {
			mask = htonl(
			    prefixlen2mask(a->nexthop->nexthop_netlen));
			if ((peer->remote_addr.v4.s_addr & mask) ==
			    (a->nexthop->nexthop_net.v4.s_addr & mask))
				/* nexthop and peer are in the same net */
				return (a->nexthop->exit_nexthop.v4.s_addr);
			else
				return (peer->local_v4_addr.v4.s_addr);
		} else
			return (peer->local_v4_addr.v4.s_addr);
	} else
		/* ebgp multihop */
		/*
		 * For ebgp multihop nh->flags should never have
		 * NEXTHOP_CONNECTED set so it should be possible to unify the
		 * two ebgp cases. But this is safe and RFC compliant.
		 */
		return (peer->local_v4_addr.v4.s_addr);
}

int
up_generate_mp_reach(struct rde_peer *peer, struct update_attr *upa,
    struct rde_aspath *a, sa_family_t af)
{
	u_int16_t	tmp;

	switch (af) {
	case AF_INET6:
		upa->mpattr_len = 21; /* AFI + SAFI + NH LEN + NH + Reserved */
		upa->mpattr = malloc(upa->mpattr_len);
		if (upa->mpattr == NULL)
			fatal("up_generate_mp_reach");
		tmp = htons(AFI_IPv6);
		memcpy(upa->mpattr, &tmp, sizeof(tmp));
		upa->mpattr[2] = SAFI_UNICAST;
		upa->mpattr[3] = sizeof(struct in6_addr);
		upa->mpattr[20] = 0; /* Reserved must be 0 */

		/* nexthop dance see also up_get_nexthop() */
		if (peer->conf.ebgp == 0) {
			/* ibgp */
			if (a->nexthop == NULL ||
			    (a->nexthop->exit_nexthop.af == AF_INET6 &&
			    memcmp(&a->nexthop->exit_nexthop.v6,
			    &peer->remote_addr.v6, sizeof(struct in6_addr))))
				memcpy(&upa->mpattr[4], &peer->local_v6_addr.v6,
				    sizeof(struct in6_addr));
			else
				memcpy(&upa->mpattr[4],
				    &a->nexthop->exit_nexthop.v6,
				    sizeof(struct in6_addr));
		} else if (peer->conf.distance == 1) {
			/* ebgp directly connected */
			if (a->nexthop != NULL &&
			    a->nexthop->flags & NEXTHOP_CONNECTED)
				if (prefix_compare(&peer->remote_addr,
				    &a->nexthop->nexthop_net,
				    a->nexthop->nexthop_netlen) == 0) {
					/*
					 * nexthop and peer are in the same
					 * subnet
					 */
					memcpy(&upa->mpattr[4],
					    &a->nexthop->exit_nexthop.v6,
					    sizeof(struct in6_addr));
					return (0);
				}
			memcpy(&upa->mpattr[4], &peer->local_v6_addr.v6,
			    sizeof(struct in6_addr));
		} else
			/* ebgp multihop */
			memcpy(&upa->mpattr[4], &peer->local_v6_addr.v6,
			    sizeof(struct in6_addr));
		return (0);
	default:
		break;
	}
	return (-1);
}

int
up_generate_attr(struct rde_peer *peer, struct update_attr *upa,
    struct rde_aspath *a, sa_family_t af)
{
	struct attr	*oa, *newaggr = NULL;
	u_char		*pdata;
	u_int32_t	 tmp32;
	in_addr_t	 nexthop;
	int		 r, ismp = 0, neednewpath = 0;
	u_int16_t	 len = sizeof(up_attr_buf), wlen = 0, plen;
	u_int8_t	 l;

	/* origin */
	if ((r = attr_write(up_attr_buf + wlen, len, ATTR_WELL_KNOWN,
	    ATTR_ORIGIN, &a->origin, 1)) == -1)
		return (-1);
	wlen += r; len -= r;

	/* aspath */
	if (!peer->conf.ebgp ||
	    rde_decisionflags() & BGPD_FLAG_DECISION_TRANS_AS)
		pdata = aspath_prepend(a->aspath, rde_local_as(), 0, &plen);
	else
		pdata = aspath_prepend(a->aspath, rde_local_as(), 1, &plen);

	if (!rde_as4byte(peer))
		pdata = aspath_deflate(pdata, &plen, &neednewpath);

	if ((r = attr_write(up_attr_buf + wlen, len, ATTR_WELL_KNOWN,
	    ATTR_ASPATH, pdata, plen)) == -1)
		return (-1);
	wlen += r; len -= r;
	free(pdata);

	switch (af) {
	case AF_INET:
		nexthop = up_get_nexthop(peer, a);
		if ((r = attr_write(up_attr_buf + wlen, len, ATTR_WELL_KNOWN,
		    ATTR_NEXTHOP, &nexthop, 4)) == -1)
			return (-1);
		wlen += r; len -= r;
		break;
	default:
		ismp = 1;
		break;
	}

	/*
	 * The old MED from other peers MUST not be announced to others
	 * unless the MED is originating from us or the peer is an IBGP one.
	 */
	if (a->flags & F_ATTR_MED && (peer->conf.ebgp == 0 ||
	    a->flags & F_ATTR_MED_ANNOUNCE)) {
		tmp32 = htonl(a->med);
		if ((r = attr_write(up_attr_buf + wlen, len, ATTR_OPTIONAL,
		    ATTR_MED, &tmp32, 4)) == -1)
			return (-1);
		wlen += r; len -= r;
	}

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
	 *     Actually ATTR_AGGREGATOR may be deflated for OLD 2-byte peers.
	 *  2. non-transitive attrs: don't re-announce to ebgp peers
	 *  3. transitive known attrs: announce unmodified
	 *  4. transitive unknown attrs: set partial bit and re-announce
	 */
	for (l = 0; l < a->others_len; l++) {
		if ((oa = a->others[l]) == NULL)
			break;
		switch (oa->type) {
		case ATTR_ATOMIC_AGGREGATE:
			if ((r = attr_write(up_attr_buf + wlen, len,
			    ATTR_WELL_KNOWN, ATTR_ATOMIC_AGGREGATE,
			    NULL, 0)) == -1)
				return (-1);
			break;
		case ATTR_AGGREGATOR:
			if (!rde_as4byte(peer)) {
				/* need to deflate the aggregator */
				u_int8_t	t[6];
				u_int16_t	tas;

				if ((!(oa->flags & ATTR_TRANSITIVE)) &&
				    peer->conf.ebgp != 0) {
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
				if ((r = attr_write(up_attr_buf + wlen, len,
				    oa->flags, oa->type, &t, sizeof(t))) == -1)
					return (-1);
				break;
			}
			/* FALLTHROUGH */
		case ATTR_COMMUNITIES:
		case ATTR_ORIGINATOR_ID:
		case ATTR_CLUSTER_LIST:
			if ((!(oa->flags & ATTR_TRANSITIVE)) &&
			    peer->conf.ebgp != 0) {
				r = 0;
				break;
			}
			if ((r = attr_write(up_attr_buf + wlen, len,
			    oa->flags, oa->type, oa->data, oa->len)) == -1)
				return (-1);
			break;
		default:
			/* unknown attribute */
			if (!(oa->flags & ATTR_TRANSITIVE)) {
				/*
				 * RFC 1771:
				 * Unrecognized non-transitive optional
				 * attributes must be quietly ignored and
				 * not passed along to other BGP peers.
				 */
				r = 0;
				break;
			}
			if ((r = attr_write(up_attr_buf + wlen, len,
			    oa->flags | ATTR_PARTIAL, oa->type,
			    oa->data, oa->len)) == -1)
				return (-1);
			break;
		}
		wlen += r; len -= r;
	}

	/* NEW to OLD conversion when going sending stuff to a 2byte AS peer */
	if (neednewpath) {
		if (!peer->conf.ebgp ||
		    rde_decisionflags() & BGPD_FLAG_DECISION_TRANS_AS)
			pdata = aspath_prepend(a->aspath, rde_local_as(), 0,
			    &plen);
		else
			pdata = aspath_prepend(a->aspath, rde_local_as(), 1,
			    &plen);
		if (plen == 0)
			r = 0;
		else if ((r = attr_write(up_attr_buf + wlen, len,
		    ATTR_OPTIONAL|ATTR_TRANSITIVE, ATTR_NEW_ASPATH,
		    pdata, plen)) == -1)
			return (-1);
		wlen += r; len -= r;
		free(pdata);
	}
	if (newaggr) {
		if ((r = attr_write(up_attr_buf + wlen, len,
		    ATTR_OPTIONAL|ATTR_TRANSITIVE, ATTR_NEW_AGGREGATOR,
		    newaggr->data, newaggr->len)) == -1)
			return (-1);
		wlen += r; len -= r;
	}

	/* write mp attribute to different buffer */
	if (ismp)
		if (up_generate_mp_reach(peer, upa, a, AF_INET6) == -1)
			return (-1);

	/* the bgp path attributes are now stored in the global buf */
	upa->attr = malloc(wlen);
	if (upa->attr == NULL)
		fatal("up_generate_attr");
	memcpy(upa->attr, up_attr_buf, wlen);
	upa->attr_len = wlen;
	return (wlen);
}

#define MIN_PREFIX_LEN	5	/* 1 byte prefix length + 4 bytes addr */
int
up_dump_prefix(u_char *buf, int len, struct uplist_prefix *prefix_head,
    struct rde_peer *peer)
{
	struct update_prefix	*upp;
	int			 r, wpos = 0;

	while ((upp = TAILQ_FIRST(prefix_head)) != NULL) {
		if ((r = prefix_write(buf + wpos, len - wpos,
		    &upp->prefix, upp->prefixlen)) == -1)
			break;
		wpos += r;
		if (RB_REMOVE(uptree_prefix, &peer->up_prefix, upp) == NULL)
			log_warnx("dequeuing update failed.");
		TAILQ_REMOVE(upp->prefix_h, upp, prefix_l);
		peer->up_pcnt--;
		if (upp->prefix_h == &peer->withdraws ||
		    upp->prefix_h == &peer->withdraws6) {
			peer->up_wcnt--;
			peer->prefix_sent_withdraw++;
		} else {
			peer->up_nlricnt--;
			peer->prefix_sent_update++;
		}
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

	/*
	 * It is possible that a queued path attribute has no nlri prefix.
	 * Ignore and remove those path attributes.
	 */
	while ((upa = TAILQ_FIRST(&peer->updates)) != NULL)
		if (TAILQ_EMPTY(&upa->prefix_h)) {
			if (RB_REMOVE(uptree_attr, &peer->up_attrs,
			    upa) == NULL)
				log_warnx("dequeuing update failed.");
			TAILQ_REMOVE(&peer->updates, upa, attr_l);
			free(upa->attr);
			free(upa->mpattr);
			free(upa);
			peer->up_acnt--;
		} else
			break;

	if (upa == NULL || upa->attr_len + MIN_PREFIX_LEN > len) {
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

	/* then the path attributes themselves */
	memcpy(buf + wpos, upa->attr, upa->attr_len);
	wpos += upa->attr_len;

	/* last but not least dump the nlri */
	r = up_dump_prefix(buf + wpos, len - wpos, &upa->prefix_h, peer);
	wpos += r;

	/* now check if all prefixes were written */
	if (TAILQ_EMPTY(&upa->prefix_h)) {
		if (RB_REMOVE(uptree_attr, &peer->up_attrs, upa) == NULL)
			log_warnx("dequeuing update failed.");
		TAILQ_REMOVE(&peer->updates, upa, attr_l);
		free(upa->attr);
		free(upa->mpattr);
		free(upa);
		peer->up_acnt--;
	}

	return (wpos);
}

u_char *
up_dump_mp_unreach(u_char *buf, u_int16_t *len, struct rde_peer *peer)
{
	int		wpos;
	u_int16_t	datalen, tmp;
	u_int16_t	attrlen = 2;	/* attribute header (without len) */
	u_int8_t	flags = ATTR_OPTIONAL;

	/*
	 * reserve space for withdraw len, attr len, the attribute header
	 * and the mp attribute header
	 */
	wpos = 2 + 2 + 4 + 3;

	if (*len < wpos)
		return (NULL);

	datalen = up_dump_prefix(buf + wpos, *len - wpos,
	    &peer->withdraws6, peer);
	if (datalen == 0)
		return (NULL);

	datalen += 3;	/* afi + safi */
	if (datalen > 255) {
		attrlen += 2 + datalen;
		flags |= ATTR_EXTLEN;
	} else {
		attrlen += 1 + datalen;
		buf++;
	}
	/* prepend header, need to do it reverse */
	/* safi & afi */
	buf[--wpos] = SAFI_UNICAST;
	wpos -= sizeof(u_int16_t);
	tmp = htons(AFI_IPv6);
	memcpy(buf + wpos, &tmp, sizeof(u_int16_t));

	/* attribute length */
	if (datalen > 255) {
		wpos -= sizeof(u_int16_t);
		tmp = htons(datalen);
		memcpy(buf + wpos, &tmp, sizeof(u_int16_t));
	} else
		buf[--wpos] = (u_char)datalen;

	/* mp attribute */
	buf[--wpos] = (u_char)ATTR_MP_UNREACH_NLRI;
	buf[--wpos] = flags;

	/* attribute length */
	wpos -= sizeof(u_int16_t);
	tmp = htons(attrlen);
	memcpy(buf + wpos, &tmp, sizeof(u_int16_t));

	/* no IPv4 withdraws */
	wpos -= sizeof(u_int16_t);
	bzero(buf + wpos, sizeof(u_int16_t));

	if (wpos < 0)
		fatalx("up_dump_mp_unreach: buffer underflow");

	/* total length includes the two 2-bytes length fields. */
	*len = attrlen + 2 * sizeof(u_int16_t);

	return (buf);
}

u_char *
up_dump_mp_reach(u_char *buf, u_int16_t *len, struct rde_peer *peer)
{
	struct update_attr	*upa;
	int			wpos;
	u_int16_t		datalen, tmp;
	u_int8_t		flags = ATTR_OPTIONAL;

	/*
	 * It is possible that a queued path attribute has no nlri prefix.
	 * Ignore and remove those path attributes.
	 */
	while ((upa = TAILQ_FIRST(&peer->updates6)) != NULL)
		if (TAILQ_EMPTY(&upa->prefix_h)) {
			if (RB_REMOVE(uptree_attr, &peer->up_attrs,
			    upa) == NULL)
				log_warnx("dequeuing update failed.");
			TAILQ_REMOVE(&peer->updates6, upa, attr_l);
			free(upa->attr);
			free(upa->mpattr);
			free(upa);
			peer->up_acnt--;
		} else
			break;

	if (upa == NULL)
		return (NULL);

	/*
	 * reserve space for attr len, the attributes, the
	 * mp attribute and the attribute header
	 */
	wpos = 2 + 2 + upa->attr_len + 4 + upa->mpattr_len;
	if (*len < wpos)
		return (NULL);

	datalen = up_dump_prefix(buf + wpos, *len - wpos,
	    &upa->prefix_h, peer);
	if (datalen == 0)
		return (NULL);

	if (upa->mpattr_len == 0 || upa->mpattr == NULL)
		fatalx("mulitprotocol update without MP attrs");

	datalen += upa->mpattr_len;
	wpos -= upa->mpattr_len;
	memcpy(buf + wpos, upa->mpattr, upa->mpattr_len);

	if (datalen > 255) {
		wpos -= 2;
		tmp = htons(datalen);
		memcpy(buf + wpos, &tmp, sizeof(tmp));
		datalen += 4;
		flags |= ATTR_EXTLEN;
	} else {
		buf[--wpos] = (u_char)datalen;
		datalen += 3;
	}
	buf[--wpos] = (u_char)ATTR_MP_REACH_NLRI;
	buf[--wpos] = flags;

	datalen += upa->attr_len;
	wpos -= upa->attr_len;
	memcpy(buf + wpos, upa->attr, upa->attr_len);

	if (wpos < 4)
		fatalx("Grrr, mp_reach buffer fucked up");

	wpos -= 2;
	tmp = htons(datalen);
	memcpy(buf + wpos, &tmp, sizeof(tmp));

	wpos -= 2;
	bzero(buf + wpos, 2);

	/* now check if all prefixes were written */
	if (TAILQ_EMPTY(&upa->prefix_h)) {
		if (RB_REMOVE(uptree_attr, &peer->up_attrs, upa) == NULL)
			log_warnx("dequeuing update failed.");
		TAILQ_REMOVE(&peer->updates6, upa, attr_l);
		free(upa->attr);
		free(upa->mpattr);
		free(upa);
		peer->up_acnt--;
	}

	*len = datalen + 4;
	return (buf + wpos);
}

