/*	$OpenBSD: rde_update.c,v 1.116 2019/06/17 12:02:44 claudio Exp $ */

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
#include <siphash.h>

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

static int
up_test_update(struct rde_peer *peer, struct prefix *p)
{
	struct bgpd_addr	 addr;
	struct rde_aspath	*asp;
	struct rde_community	*comm;
	struct rde_peer		*prefp;
	struct attr		*attr;

	if (p == NULL)
		/* no prefix available */
		return (0);

	prefp = prefix_peer(p);
	asp = prefix_aspath(p);
	comm = prefix_communities(p);

	if (peer == prefp)
		/* Do not send routes back to sender */
		return (0);

	if (asp == NULL || asp->flags & F_ATTR_PARSE_ERR)
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

	/* export type handling */
	if (peer->conf.export_type == EXPORT_NONE ||
	    peer->conf.export_type == EXPORT_DEFAULT_ROUTE) {
		/*
		 * no need to withdraw old prefix as this will be
		 * filtered out as well.
		 */
		return (-1);
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

void
up_generate_updates(struct filter_head *rules, struct rde_peer *peer,
    struct prefix *new, struct prefix *old)
{
	struct filterstate		state;
	struct bgpd_addr		addr;

	if (peer->state != PEER_UP)
		return;

	if (new == NULL) {
withdraw:
		if (old == NULL)
			/* no prefix to withdraw */
			return;

		/* withdraw prefix */
		pt_getaddr(old->re->prefix, &addr);
		if (prefix_withdraw(&ribs[RIB_ADJ_OUT].rib, peer, &addr,
		    old->re->prefix->prefixlen) == 1)
			peer->up_wcnt++;
	} else {
		switch (up_test_update(peer, new)) {
		case 1:
			break;
		case 0:
			goto withdraw;
		case -1:
			return;
		}

		rde_filterstate_prep(&state, prefix_aspath(new),
		    prefix_communities(new), prefix_nexthop(new),
		    prefix_nhflags(new));
		if (rde_filter(rules, peer, new, &state) == ACTION_DENY) {
			rde_filterstate_clean(&state);
			goto withdraw;
		}

		pt_getaddr(new->re->prefix, &addr);
		if (path_update(&ribs[RIB_ADJ_OUT].rib, peer, &state, &addr,
		    new->re->prefix->prefixlen, prefix_vstate(new)) != 2) {
			/* only send update if path changed */
			prefix_update(&ribs[RIB_ADJ_OUT].rib, peer, &addr,
			    new->re->prefix->prefixlen);
			peer->up_nlricnt++;
		}

		rde_filterstate_clean(&state);
	}
}

struct rib_entry *rib_add(struct rib *, struct bgpd_addr *, int);
void rib_remove(struct rib_entry *);
int rib_empty(struct rib_entry *);

/* send a default route to the specified peer */
void
up_generate_default(struct filter_head *rules, struct rde_peer *peer,
    u_int8_t aid)
{
	struct filterstate	 state;
	struct rde_aspath	*asp;
	struct prefix		 p;
	struct rib_entry	*re;
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
	/* rde_apply_set(asp, set, af, &state, DIR_IN); */

	/*
	 * XXX this is ugly because we need to have a prefix for rde_filter()
	 * but it will be added after filtering. So fake it till we make it.
	 */
	bzero(&p, sizeof(p));
	bzero(&addr, sizeof(addr));
	addr.aid = aid;
	re = rib_get(rib_byid(peer->loc_rib_id), &addr, 0);
	if (re == NULL)
		re = rib_add(rib_byid(peer->loc_rib_id), &addr, 0);
	p.re = re;
	p.aspath = asp;
	p.peer = peer; /* XXX should be peerself */

	/* filter as usual */
	if (rde_filter(rules, peer, &p, &state) == ACTION_DENY) {
		rde_filterstate_clean(&state);
		return;
	}

	if (path_update(&ribs[RIB_ADJ_OUT].rib, peer, &state, &addr, 0,
	    ROA_NOTFOUND) != 2) {
		prefix_update(&ribs[RIB_ADJ_OUT].rib, peer, &addr, 0);
		peer->up_nlricnt++;
	}

	/* no longer needed */
	rde_filterstate_clean(&state);

	if (rib_empty(re))
		rib_remove(re);
}

/* only for IPv4 */
static struct bgpd_addr *
up_get_nexthop(struct rde_peer *peer, struct filterstate *state, u_int8_t aid)
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

static int
up_generate_attr(u_char *buf, int len, struct rde_peer *peer,
    struct filterstate *state, u_int8_t aid)
{
	struct rde_aspath *asp = &state->aspath;
	struct rde_community *comm = &state->communities;
	struct attr	*oa = NULL, *newaggr = NULL;
	u_char		*pdata;
	u_int32_t	 tmp32;
	in_addr_t	 nexthop;
	int		 flags, r, neednewpath = 0;
	u_int16_t	 wlen = 0, plen;
	u_int8_t	 oalen = 0, type;

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
			if (!peer->conf.ebgp ||
			    peer->conf.flags & PEERFLAG_TRANS_AS)
				pdata = aspath_prepend(asp->aspath,
				    peer->conf.local_as, 0, &plen);
			else
				pdata = aspath_prepend(asp->aspath,
				    peer->conf.local_as, 1, &plen);

			if (!rde_as4byte(peer))
				pdata = aspath_deflate(pdata, &plen,
				    &neednewpath);

			if ((r = attr_write(buf + wlen, len, ATTR_WELL_KNOWN,
			    ATTR_ASPATH, pdata, plen)) == -1)
				return (-1);
			free(pdata);
			break;
		case ATTR_NEXTHOP:
			switch (aid) {
			case AID_INET:
				nexthop =
				    up_get_nexthop(peer, state, aid)->v4.s_addr;
				if ((r = attr_write(buf + wlen, len,
				    ATTR_WELL_KNOWN, ATTR_NEXTHOP, &nexthop,
				    4)) == -1)
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
			    peer->conf.flags & PEERFLAG_TRANS_AS)) {
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
				if (!peer->conf.ebgp ||
				    peer->conf.flags & PEERFLAG_TRANS_AS)
					pdata = aspath_prepend(asp->aspath,
					peer->conf.local_as, 0, &plen);
				else
					pdata = aspath_prepend(asp->aspath,
					    peer->conf.local_as, 1, &plen);
				flags = ATTR_OPTIONAL|ATTR_TRANSITIVE;
				if (!(asp->flags & F_PREFIX_ANNOUNCED))
					flags |= ATTR_PARTIAL;
				if (plen == 0)
					r = 0;
				else if ((r = attr_write(buf + wlen, len, flags,
				    ATTR_AS4_PATH, pdata, plen)) == -1)
					return (-1);
				free(pdata);
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
				if ((r = attr_write(buf + wlen, len,
				    oa->flags, oa->type, &t, sizeof(t))) == -1)
					return (-1);
				break;
			}
			/* FALLTHROUGH */
		case ATTR_ORIGINATOR_ID:
		case ATTR_CLUSTER_LIST:
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
up_is_eor(struct rde_peer *peer, u_int8_t aid)
{
	struct prefix *p;

	p = RB_MIN(prefix_tree, &peer->updates[aid]);
	if (p != NULL && p->eor) {
		RB_REMOVE(prefix_tree, &peer->updates[aid], p);
		prefix_destroy(p);
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
	int		 r, wpos = 0, done = 0;

	RB_FOREACH_SAFE(p, prefix_tree, prefix_head, np) {
		pt_getaddr(p->re->prefix, &addr);
		if ((r = prefix_write(buf + wpos, len - wpos,
		    &addr, p->re->prefix->prefixlen, withdraw)) == -1)
			break;
		wpos += r;

		/* make sure we only dump prefixes which belong together */
		if (np == NULL || np->aspath != p->aspath ||
		    np->nexthop != p->nexthop || np->nhflags != p->nhflags ||
		    np->eor)
			done = 1;

		/* prefix sent, remove from list and clear flag */
		RB_REMOVE(prefix_tree, prefix_head, p);
		p->flags = 0;

		if (withdraw) {
			/* prefix no longer needed, remove it */
			prefix_destroy(p);
			peer->up_wcnt--;
			peer->prefix_sent_withdraw++;
		} else {
			/* prefix still in Adj-RIB-Out, keep it */
			peer->up_nlricnt--;
			peer->prefix_sent_update++;
		}
		if (done)
			break;
	}
	return (wpos);
}

int
up_dump_withdraws(u_char *buf, int len, struct rde_peer *peer, u_int8_t aid)
{
	u_int16_t wpos, wd_len;
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
up_dump_mp_unreach(u_char *buf, int len, struct rde_peer *peer, u_int8_t aid)
{
	u_char		*attrbuf;
	int		 wpos, r;
	u_int16_t	 attr_len, tmp;

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
	memcpy(buf + wpos, &tmp, sizeof(u_int16_t));
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
	bzero(buf, sizeof(u_int16_t));	/* withdrawn routes len */
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
	u_int16_t		 attr_len;

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
		bzero(buf, 2);
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
    struct filterstate *state, u_int8_t aid)
{
	struct bgpd_addr	*nexthop;
	u_char			*attrbuf;
	int			 r, wpos, attrlen;
	u_int16_t		 tmp;

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
		nexthop = up_get_nexthop(peer, state, aid);
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
		attrbuf[3] = sizeof(u_int64_t) + sizeof(struct in_addr);
		bzero(attrbuf + 4, sizeof(u_int64_t));
		attrbuf[16] = 0; /* Reserved must be 0 */

		/* write nexthop */
		attrbuf += 12;
		nexthop = up_get_nexthop(peer, state, aid);
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
		attrbuf[3] = sizeof(u_int64_t) + sizeof(struct in6_addr);
		bzero(attrbuf + 4, sizeof(u_int64_t));
		attrbuf[28] = 0; /* Reserved must be 0 */

		/* write nexthop */
		attrbuf += 12;
		nexthop = up_get_nexthop(peer, state, aid);
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
up_dump_mp_reach(u_char *buf, int len, struct rde_peer *peer, u_int8_t aid)
{
	struct filterstate	 state;
	struct prefix		*p;
	int			r, wpos;
	u_int16_t		attr_len;

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
	bzero(buf, sizeof(u_int16_t));	/* withdrawn routes len */
	attr_len = htons(wpos - 4);
	memcpy(buf + 2, &attr_len, sizeof(attr_len));

	return (wpos);
}
