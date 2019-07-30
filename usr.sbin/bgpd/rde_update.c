/*	$OpenBSD: rde_update.c,v 1.89 2018/02/10 05:54:31 claudio Exp $ */

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

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <siphash.h>

#include "bgpd.h"
#include "rde.h"
#include "log.h"

in_addr_t	up_get_nexthop(struct rde_peer *, struct rde_aspath *);
int		up_generate_mp_reach(struct rde_peer *, struct update_attr *,
		    struct rde_aspath *, u_int8_t);
int		up_generate_attr(struct rde_peer *, struct update_attr *,
		    struct rde_aspath *, u_int8_t);

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

SIPHASH_KEY uptree_key;

void
up_init(struct rde_peer *peer)
{
	u_int8_t	i;

	for (i = 0; i < AID_MAX; i++) {
		TAILQ_INIT(&peer->updates[i]);
		TAILQ_INIT(&peer->withdraws[i]);
	}
	RB_INIT(&peer->up_prefix);
	RB_INIT(&peer->up_attrs);
	peer->up_pcnt = 0;
	peer->up_acnt = 0;
	peer->up_nlricnt = 0;
	peer->up_wcnt = 0;
	arc4random_buf(&uptree_key, sizeof(uptree_key));
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
	u_int8_t	i;

	for (i = 0; i < AID_MAX; i++)
		up_clear(&peer->updates[i], &peer->withdraws[i]);

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

	if (a->prefix.aid < b->prefix.aid)
		return (-1);
	if (a->prefix.aid > b->prefix.aid)
		return (1);

	switch (a->prefix.aid) {
	case AID_INET:
		if (ntohl(a->prefix.v4.s_addr) < ntohl(b->prefix.v4.s_addr))
			return (-1);
		if (ntohl(a->prefix.v4.s_addr) > ntohl(b->prefix.v4.s_addr))
			return (1);
		break;
	case AID_INET6:
		i = memcmp(&a->prefix.v6, &b->prefix.v6,
		    sizeof(struct in6_addr));
		if (i > 0)
			return (1);
		if (i < 0)
			return (-1);
		break;
	case AID_VPN_IPv4:
		if (betoh64(a->prefix.vpn4.rd) < betoh64(b->prefix.vpn4.rd))
			return (-1);
		if (betoh64(a->prefix.vpn4.rd) > betoh64(b->prefix.vpn4.rd))
			return (1);
		if (ntohl(a->prefix.v4.s_addr) < ntohl(b->prefix.v4.s_addr))
			return (-1);
		if (ntohl(a->prefix.v4.s_addr) > ntohl(b->prefix.v4.s_addr))
			return (1);
		if (a->prefixlen < b->prefixlen)
			return (-1);
		if (a->prefixlen > b->prefixlen)
			return (1);
		if (a->prefix.vpn4.labellen < b->prefix.vpn4.labellen)
			return (-1);
		if (a->prefix.vpn4.labellen > b->prefix.vpn4.labellen)
			return (1);
		return (memcmp(a->prefix.vpn4.labelstack,
		    b->prefix.vpn4.labelstack, a->prefix.vpn4.labellen));
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

	upl = &peer->updates[p->prefix.aid];
	wdl = &peer->withdraws[p->prefix.aid];

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
	struct rde_aspath	*asp;
	struct rde_peer		*prefp;
	struct attr		*attr;

	if (peer->state != PEER_UP)
		return (-1);

	if (p == NULL)
		/* no prefix available */
		return (0);

	prefp = prefix_peer(p);
	asp = prefix_aspath(p);

	if (peer == prefp)
		/* Do not send routes back to sender */
		return (0);

	if (asp->flags & F_ATTR_PARSE_ERR)
		fatalx("try to send out a botched path");
	if (asp->flags & F_ATTR_LOOP)
		fatalx("try to send out a looped path");

	pt_getaddr(p->re->prefix, &addr);
	if (peer->capa.mp[addr.aid] == 0)
		return (-1);

	if (!prefp->conf.ebgp && !peer->conf.ebgp) {
		/*
		 * route reflector redistribution rules:
		 * 1. if announce is set                -> announce
		 * 2. old non-client, new non-client    -> no
		 * 3. old client, new non-client        -> yes
		 * 4. old non-client, new client        -> yes
		 * 5. old client, new client            -> yes
		 */
		if (prefp->conf.reflector_client == 0 &&
		    peer->conf.reflector_client == 0 &&
		    (asp->flags & F_PREFIX_ANNOUNCED) == 0)
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
		if (asp->aspath->ascnt != 0)
			return (0);
		break;
	}

	/* well known communities */
	if (community_match(asp, COMMUNITY_WELLKNOWN, COMMUNITY_NO_ADVERTISE))
		return (0);
	if (peer->conf.ebgp && community_match(asp, COMMUNITY_WELLKNOWN,
	    COMMUNITY_NO_EXPORT))
		return (0);
	if (peer->conf.ebgp && community_match(asp, COMMUNITY_WELLKNOWN,
	    COMMUNITY_NO_EXPSUBCONFED))
		return (0);

	/*
	 * Don't send messages back to originator
	 * this is not specified in the RFC but seems logical.
	 */
	if ((attr = attr_optget(asp, ATTR_ORIGINATOR_ID)) != NULL) {
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
	SIPHASH_CTX			ctx;

	if (asp) {
		ua = calloc(1, sizeof(struct update_attr));
		if (ua == NULL)
			fatal("up_generate");

		if (up_generate_attr(peer, ua, asp, addr->aid) == -1) {
			log_warnx("generation of bgp path attributes failed");
			free(ua);
			return (-1);
		}
		/*
		 * use aspath_hash as attr_hash, this may be unoptimal
		 * but currently I don't care.
		 */
		SipHash24_Init(&ctx, &uptree_key);
		SipHash24_Update(&ctx, ua->attr, ua->attr_len);
		if (ua->mpattr)
			SipHash24_Update(&ctx, ua->mpattr, ua->mpattr_len);
		ua->attr_hash = SipHash24_End(&ctx);
	}

	up = calloc(1, sizeof(struct update_prefix));
	if (up == NULL)
		fatal("up_generate");
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
	struct rde_aspath		*asp, *fasp;
	struct bgpd_addr		 addr;

	if (peer->state != PEER_UP)
		return;

	if (new == NULL) {
withdraw:
		if (up_test_update(peer, old) != 1)
			return;

		asp = prefix_aspath(old);
		pt_getaddr(old->re->prefix, &addr);
		if (rde_filter(rules, NULL, peer, asp, &addr,
		    old->re->prefix->prefixlen, asp->peer) == ACTION_DENY)
			return;

		/* withdraw prefix */
		up_generate(peer, NULL, &addr, old->re->prefix->prefixlen);
	} else {
		switch (up_test_update(peer, new)) {
		case 1:
			break;
		case 0:
			goto withdraw;
		case -1:
			return;
		}

		asp = prefix_aspath(new);
		pt_getaddr(new->re->prefix, &addr);
		if (rde_filter(rules, &fasp, peer, asp, &addr,
		    new->re->prefix->prefixlen, asp->peer) == ACTION_DENY) {
			path_put(fasp);
			goto withdraw;
		}
		if (fasp == NULL)
			fasp = asp;

		up_generate(peer, fasp, &addr, new->re->prefix->prefixlen);

		/* free modified aspath */
		if (fasp != asp)
			path_put(fasp);
	}
}

/* send a default route to the specified peer */
void
up_generate_default(struct filter_head *rules, struct rde_peer *peer,
    u_int8_t aid)
{
	struct rde_aspath	*asp, *fasp;
	struct bgpd_addr	 addr;

	if (peer->capa.mp[aid] == 0)
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
	addr.aid = aid;

	if (rde_filter(rules, &fasp, peer, asp, &addr, 0, NULL) ==
	    ACTION_DENY) {
		path_put(fasp);
		path_put(asp);
		return;
	}

	if (fasp == NULL)
		fasp = asp;

	up_generate(peer, fasp, &addr, 0);

	/* no longer needed */
	if (fasp != asp)
		path_put(fasp);
	path_put(asp);
}

/* generate a EoR marker in the update list. This is a horrible hack. */
int
up_generate_marker(struct rde_peer *peer, u_int8_t aid)
{
	struct update_attr	*ua;
	struct update_attr	*na = NULL;
	struct uplist_attr	*upl = NULL;

	ua = calloc(1, sizeof(struct update_attr));
	if (ua == NULL)
		fatal("up_generate_marker");

	upl = &peer->updates[aid];

	/* 1. search for attr */
	if ((na = RB_FIND(uptree_attr, &peer->up_attrs, ua)) == NULL) {
		/* 1.1 if not found -> add */
		TAILQ_INIT(&ua->prefix_h);
		if (RB_INSERT(uptree_attr, &peer->up_attrs, ua) != NULL) {
			log_warnx("uptree_attr insert failed");
			/* cleanup */
			free(ua);
			return (-1);
		}
		TAILQ_INSERT_TAIL(upl, ua, attr_l);
		peer->up_acnt++;
	} else {
		/* 1.2 if found -> use that, free ua */
		free(ua);
		ua = na;
		/* move to end of update queue */
		TAILQ_REMOVE(upl, ua, attr_l);
		TAILQ_INSERT_TAIL(upl, ua, attr_l);
	}
	return (0);
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
    struct rde_aspath *a, u_int8_t aid)
{
	u_int16_t	tmp;

	switch (aid) {
	case AID_INET6:
		upa->mpattr_len = 21; /* AFI + SAFI + NH LEN + NH + Reserved */
		upa->mpattr = malloc(upa->mpattr_len);
		if (upa->mpattr == NULL)
			fatal("up_generate_mp_reach");
		if (aid2afi(aid, &tmp, &upa->mpattr[2]))
			fatalx("up_generate_mp_reachi: bad AID");
		tmp = htons(tmp);
		memcpy(upa->mpattr, &tmp, sizeof(tmp));
		upa->mpattr[3] = sizeof(struct in6_addr);
		upa->mpattr[20] = 0; /* Reserved must be 0 */

		/* nexthop dance see also up_get_nexthop() */
		if (a->flags & F_NEXTHOP_NOMODIFY) {
			/* no modify flag set */
			if (a->nexthop == NULL)
				memcpy(&upa->mpattr[4], &peer->local_v6_addr.v6,
				    sizeof(struct in6_addr));
			else
				memcpy(&upa->mpattr[4],
				    &a->nexthop->exit_nexthop.v6,
				    sizeof(struct in6_addr));
		} else if (a->flags & F_NEXTHOP_SELF)
			memcpy(&upa->mpattr[4], &peer->local_v6_addr.v6,
			    sizeof(struct in6_addr));
		else if (!peer->conf.ebgp) {
			/* ibgp */
			if (a->nexthop == NULL ||
			    (a->nexthop->exit_nexthop.aid == AID_INET6 &&
			    !memcmp(&a->nexthop->exit_nexthop.v6,
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
	case AID_VPN_IPv4:
		upa->mpattr_len = 17; /* AFI + SAFI + NH LEN + NH + Reserved */
		upa->mpattr = calloc(upa->mpattr_len, 1);
		if (upa->mpattr == NULL)
			fatal("up_generate_mp_reach");
		if (aid2afi(aid, &tmp, &upa->mpattr[2]))
			fatalx("up_generate_mp_reachi: bad AID");
		tmp = htons(tmp);
		memcpy(upa->mpattr, &tmp, sizeof(tmp));
		upa->mpattr[3] = sizeof(u_int64_t) + sizeof(struct in_addr);

		/* nexthop dance see also up_get_nexthop() */
		if (a->flags & F_NEXTHOP_NOMODIFY) {
			/* no modify flag set */
			if (a->nexthop == NULL)
				memcpy(&upa->mpattr[12],
				    &peer->local_v4_addr.v4,
				    sizeof(struct in_addr));
			else
				/* nexthops are stored as IPv4 addrs */
				memcpy(&upa->mpattr[12],
				    &a->nexthop->exit_nexthop.v4,
				    sizeof(struct in_addr));
		} else if (a->flags & F_NEXTHOP_SELF)
			memcpy(&upa->mpattr[12], &peer->local_v4_addr.v4,
			    sizeof(struct in_addr));
		else if (!peer->conf.ebgp) {
			/* ibgp */
			if (a->nexthop == NULL ||
			    (a->nexthop->exit_nexthop.aid == AID_INET &&
			    !memcmp(&a->nexthop->exit_nexthop.v4,
			    &peer->remote_addr.v4, sizeof(struct in_addr))))
				memcpy(&upa->mpattr[12],
				    &peer->local_v4_addr.v4,
				    sizeof(struct in_addr));
			else
				memcpy(&upa->mpattr[12],
				    &a->nexthop->exit_nexthop.v4,
				    sizeof(struct in_addr));
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
					memcpy(&upa->mpattr[12],
					    &a->nexthop->exit_nexthop.v4,
					    sizeof(struct in_addr));
					return (0);
				}
			memcpy(&upa->mpattr[12], &peer->local_v4_addr.v4,
			    sizeof(struct in_addr));
		} else
			/* ebgp multihop */
			memcpy(&upa->mpattr[12], &peer->local_v4_addr.v4,
			    sizeof(struct in_addr));
		return (0);
	default:
		break;
	}
	return (-1);
}

int
up_generate_attr(struct rde_peer *peer, struct update_attr *upa,
    struct rde_aspath *a, u_int8_t aid)
{
	struct attr	*oa, *newaggr = NULL;
	u_char		*pdata;
	u_int32_t	 tmp32;
	in_addr_t	 nexthop;
	int		 flags, r, ismp = 0, neednewpath = 0;
	u_int16_t	 len = sizeof(up_attr_buf), wlen = 0, plen;
	u_int8_t	 l;
	u_int16_t	 nlen = 0;
	u_char		*ndata = NULL;

	/* origin */
	if ((r = attr_write(up_attr_buf + wlen, len, ATTR_WELL_KNOWN,
	    ATTR_ORIGIN, &a->origin, 1)) == -1)
		return (-1);
	wlen += r; len -= r;

	/* aspath */
	if (!peer->conf.ebgp ||
	    peer->conf.flags & PEERFLAG_TRANS_AS)
		pdata = aspath_prepend(a->aspath, peer->conf.local_as, 0,
		    &plen);
	else
		pdata = aspath_prepend(a->aspath, peer->conf.local_as, 1,
		    &plen);

	if (!rde_as4byte(peer))
		pdata = aspath_deflate(pdata, &plen, &neednewpath);

	if ((r = attr_write(up_attr_buf + wlen, len, ATTR_WELL_KNOWN,
	    ATTR_ASPATH, pdata, plen)) == -1)
		return (-1);
	wlen += r; len -= r;
	free(pdata);

	switch (aid) {
	case AID_INET:
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
	 * Only exception are routers with "transparent-as yes" set.
	 */
	if (a->flags & F_ATTR_MED && (!peer->conf.ebgp ||
	    a->flags & F_ATTR_MED_ANNOUNCE ||
	    peer->conf.flags & PEERFLAG_TRANS_AS)) {
		tmp32 = htonl(a->med);
		if ((r = attr_write(up_attr_buf + wlen, len, ATTR_OPTIONAL,
		    ATTR_MED, &tmp32, 4)) == -1)
			return (-1);
		wlen += r; len -= r;
	}

	if (!peer->conf.ebgp) {
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
				if ((r = attr_write(up_attr_buf + wlen, len,
				    oa->flags, oa->type, &t, sizeof(t))) == -1)
					return (-1);
				break;
			}
			/* FALLTHROUGH */
		case ATTR_COMMUNITIES:
		case ATTR_ORIGINATOR_ID:
		case ATTR_CLUSTER_LIST:
		case ATTR_LARGE_COMMUNITIES:
			if ((!(oa->flags & ATTR_TRANSITIVE)) &&
			    peer->conf.ebgp) {
				r = 0;
				break;
			}
			if ((r = attr_write(up_attr_buf + wlen, len,
			    oa->flags, oa->type, oa->data, oa->len)) == -1)
				return (-1);
			break;
		case ATTR_EXT_COMMUNITIES:
			/* handle (non-)transitive extended communities */
			if (peer->conf.ebgp) {
				ndata = community_ext_delete_non_trans(oa->data,
				    oa->len, &nlen);

				if (nlen > 0) {
					if ((r = attr_write(up_attr_buf + wlen,
					    len, oa->flags, oa->type, ndata,
					    nlen)) == -1) {
						free(ndata);
						return (-1);
					}
				} else
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
		    peer->conf.flags & PEERFLAG_TRANS_AS)
			pdata = aspath_prepend(a->aspath, peer->conf.local_as,
			    0, &plen);
		else
			pdata = aspath_prepend(a->aspath, peer->conf.local_as,
			    1, &plen);
		flags = ATTR_OPTIONAL|ATTR_TRANSITIVE;
		if (!(a->flags & F_PREFIX_ANNOUNCED))
			flags |= ATTR_PARTIAL;
		if (plen == 0)
			r = 0;
		else if ((r = attr_write(up_attr_buf + wlen, len, flags,
		    ATTR_AS4_PATH, pdata, plen)) == -1)
			return (-1);
		wlen += r; len -= r;
		free(pdata);
	}
	if (newaggr) {
		flags = ATTR_OPTIONAL|ATTR_TRANSITIVE;
		if (!(a->flags & F_PREFIX_ANNOUNCED))
			flags |= ATTR_PARTIAL;
		if ((r = attr_write(up_attr_buf + wlen, len, flags,
		    ATTR_AS4_AGGREGATOR, newaggr->data, newaggr->len)) == -1)
			return (-1);
		wlen += r; len -= r;
	}

	/* write mp attribute to different buffer */
	if (ismp)
		if (up_generate_mp_reach(peer, upa, a, aid) == -1)
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
    struct rde_peer *peer, int withdraw)
{
	struct update_prefix	*upp;
	int			 r, wpos = 0;

	while ((upp = TAILQ_FIRST(prefix_head)) != NULL) {
		if ((r = prefix_write(buf + wpos, len - wpos,
		    &upp->prefix, upp->prefixlen, withdraw)) == -1)
			break;
		wpos += r;
		if (RB_REMOVE(uptree_prefix, &peer->up_prefix, upp) == NULL)
			log_warnx("dequeuing update failed.");
		TAILQ_REMOVE(upp->prefix_h, upp, prefix_l);
		peer->up_pcnt--;
		if (withdraw) {
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
	while ((upa = TAILQ_FIRST(&peer->updates[AID_INET])) != NULL)
		if (TAILQ_EMPTY(&upa->prefix_h)) {
			attr_len = upa->attr_len;
			if (RB_REMOVE(uptree_attr, &peer->up_attrs,
			    upa) == NULL)
				log_warnx("dequeuing update failed.");
			TAILQ_REMOVE(&peer->updates[AID_INET], upa, attr_l);
			free(upa->attr);
			free(upa->mpattr);
			free(upa);
			peer->up_acnt--;
			/* XXX horrible hack,
			 * if attr_len is 0, it is a EoR marker */
			if (attr_len == 0)
				return (-1);
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
	r = up_dump_prefix(buf + wpos, len - wpos, &upa->prefix_h, peer, 0);
	wpos += r;

	/* now check if all prefixes were written */
	if (TAILQ_EMPTY(&upa->prefix_h)) {
		if (RB_REMOVE(uptree_attr, &peer->up_attrs, upa) == NULL)
			log_warnx("dequeuing update failed.");
		TAILQ_REMOVE(&peer->updates[AID_INET], upa, attr_l);
		free(upa->attr);
		free(upa->mpattr);
		free(upa);
		peer->up_acnt--;
	}

	return (wpos);
}

u_char *
up_dump_mp_unreach(u_char *buf, u_int16_t *len, struct rde_peer *peer,
    u_int8_t aid)
{
	int		wpos;
	u_int16_t	datalen, tmp;
	u_int16_t	attrlen = 2;	/* attribute header (without len) */
	u_int8_t	flags = ATTR_OPTIONAL, safi;

	/*
	 * reserve space for withdraw len, attr len, the attribute header
	 * and the mp attribute header
	 */
	wpos = 2 + 2 + 4 + 3;

	if (*len < wpos)
		return (NULL);

	datalen = up_dump_prefix(buf + wpos, *len - wpos,
	    &peer->withdraws[aid], peer, 1);
	if (datalen == 0)
		return (NULL);

	datalen += 3;	/* afi + safi */

	/* prepend header, need to do it reverse */
	/* safi & afi */
	if (aid2afi(aid, &tmp, &safi))
		fatalx("up_dump_mp_unreach: bad AID");
	buf[--wpos] = safi;
	wpos -= sizeof(u_int16_t);
	tmp = htons(tmp);
	memcpy(buf + wpos, &tmp, sizeof(u_int16_t));

	/* attribute length */
	if (datalen > 255) {
		attrlen += 2 + datalen;
		flags |= ATTR_EXTLEN;
		wpos -= sizeof(u_int16_t);
		tmp = htons(datalen);
		memcpy(buf + wpos, &tmp, sizeof(u_int16_t));
	} else {
		attrlen += 1 + datalen;
		buf[--wpos] = (u_char)datalen;
	}

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

	return (buf + wpos);
}

int
up_dump_mp_reach(u_char *buf, u_int16_t *len, struct rde_peer *peer,
    u_int8_t aid)
{
	struct update_attr	*upa;
	int			wpos;
	u_int16_t		attr_len, datalen, tmp;
	u_int8_t		flags = ATTR_OPTIONAL;

	/*
	 * It is possible that a queued path attribute has no nlri prefix.
	 * Ignore and remove those path attributes.
	 */
	while ((upa = TAILQ_FIRST(&peer->updates[aid])) != NULL)
		if (TAILQ_EMPTY(&upa->prefix_h)) {
			attr_len = upa->attr_len;
			if (RB_REMOVE(uptree_attr, &peer->up_attrs,
			    upa) == NULL)
				log_warnx("dequeuing update failed.");
			TAILQ_REMOVE(&peer->updates[aid], upa, attr_l);
			free(upa->attr);
			free(upa->mpattr);
			free(upa);
			peer->up_acnt--;
			/* XXX horrible hack,
			 * if attr_len is 0, it is a EoR marker */
			if (attr_len == 0)
				return (-1);
		} else
			break;

	if (upa == NULL)
		return (-2);

	/*
	 * reserve space for attr len, the attributes, the
	 * mp attribute and the attribute header
	 */
	wpos = 2 + 2 + upa->attr_len + 4 + upa->mpattr_len;
	if (*len < wpos)
		return (-2);

	datalen = up_dump_prefix(buf + wpos, *len - wpos,
	    &upa->prefix_h, peer, 0);
	if (datalen == 0)
		return (-2);

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
		TAILQ_REMOVE(&peer->updates[aid], upa, attr_l);
		free(upa->attr);
		free(upa->mpattr);
		free(upa);
		peer->up_acnt--;
	}

	*len = datalen + 4;
	return (wpos);
}
