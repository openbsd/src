/*	$OpenBSD: rde_update.c,v 1.3 2004/02/17 15:44:02 claudio Exp $ */

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
#include "ensure.h"
#include "rde.h"

int	up_generate_attr(struct rde_peer *, struct update_attr *,
	    struct attr_flags *, struct nexthop *);
int	up_set_prefix(u_char *, int, struct bgpd_addr *, u_int8_t);


/* update stuff. */
struct update_prefix {
	struct bgpd_addr		 prefix;
	int				 prefixlen;
	struct uplist_prefix		*prefix_h;
	TAILQ_ENTRY(update_prefix)	 prefix_l;
	RB_ENTRY(update_prefix)		 entry;
};

struct update_attr {
	u_int32_t			 attr_hash;
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
	struct update_attr	*na = NULL;
	struct update_prefix	*np;

	ENSURE(p != NULL);

	/* 1. search for attr */
	if (a != NULL && (na = RB_FIND(uptree_attr, &peer->up_attrs, a)) ==
	    NULL) {
		/* 1.1 if not found -> add */
		TAILQ_INIT(&a->prefix_h);
		if (RB_INSERT(uptree_attr, &peer->up_attrs, a) != NULL) {
			log_warnx("uptree_attr insert failed");
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
			log_warnx("uptree_prefix insert failed");
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
		case ANNOUNCE_UNDEF:
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
			log_warnx("queuing update failed.");
	} else {
		if (peer == new->peer)
			/* Do not send routes back to sender */
			return;

		if (peer->conf.ebgp == 0 && new->peer->conf.ebgp == 0)
			/* Do not redistribute updates to ibgp peers */
			return;

		/* announce type handling */
		switch (peer->conf.announce_type) {
		case ANNOUNCE_UNDEF:
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
			log_warnx("generation of bgp path attributes failed");

		/*
		 * use aspath_hash as attr_hash, this may be unoptimal
		 * but currently I don't care.
		 */
		a->attr_hash = aspath_hash(new->aspath->flags.aspath);
		p->prefix = new->prefix->prefix;
		p->prefixlen = new->prefix->prefixlen;

		if (up_add(peer, p, a) == -1)
			log_warnx("queuing update failed.");
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
			log_warnx("dequeuing update failed.");
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
			log_warnx("dequeuing update failed.");
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
