/*	$OpenBSD: mrt.c,v 1.88 2018/12/30 13:53:07 denis Exp $ */

/*
 * Copyright (c) 2003, 2004 Claudio Jeker <claudio@openbsd.org>
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
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "bgpd.h"
#include "rde.h"
#include "session.h"

#include "mrt.h"
#include "log.h"

int mrt_attr_dump(struct ibuf *, struct rde_aspath *, struct bgpd_addr *, int);
int mrt_dump_entry_mp(struct mrt *, struct prefix *, u_int16_t,
    struct rde_peer*);
int mrt_dump_entry(struct mrt *, struct prefix *, u_int16_t, struct rde_peer*);
int mrt_dump_entry_v2(struct mrt *, struct rib_entry *, u_int32_t);
int mrt_dump_peer(struct ibuf *, struct rde_peer *);
int mrt_dump_hdr_se(struct ibuf **, struct peer *, u_int16_t, u_int16_t,
    u_int32_t, int);
int mrt_dump_hdr_rde(struct ibuf **, u_int16_t type, u_int16_t, u_int32_t);
int mrt_open(struct mrt *, time_t);

#define DUMP_BYTE(x, b)							\
	do {								\
		u_char		t = (b);				\
		if (ibuf_add((x), &t, sizeof(t)) == -1) {		\
			log_warn("mrt_dump1: ibuf_add error");		\
			goto fail;					\
		}							\
	} while (0)

#define DUMP_SHORT(x, s)						\
	do {								\
		u_int16_t	t;					\
		t = htons((s));						\
		if (ibuf_add((x), &t, sizeof(t)) == -1) {		\
			log_warn("mrt_dump2: ibuf_add error");		\
			goto fail;					\
		}							\
	} while (0)

#define DUMP_LONG(x, l)							\
	do {								\
		u_int32_t	t;					\
		t = htonl((l));						\
		if (ibuf_add((x), &t, sizeof(t)) == -1) {		\
			log_warn("mrt_dump3: ibuf_add error");		\
			goto fail;					\
		}							\
	} while (0)

#define DUMP_NLONG(x, l)						\
	do {								\
		u_int32_t	t = (l);				\
		if (ibuf_add((x), &t, sizeof(t)) == -1) {		\
			log_warn("mrt_dump4: ibuf_add error");		\
			goto fail;					\
		}							\
	} while (0)

#define RDEIDX		0
#define SEIDX		1
#define TYPE2IDX(x)	((x == MRT_TABLE_DUMP ||			\
			    x == MRT_TABLE_DUMP_MP ||			\
			    x == MRT_TABLE_DUMP_V2) ? RDEIDX : SEIDX	\
			)

void
mrt_dump_bgp_msg(struct mrt *mrt, void *pkg, u_int16_t pkglen,
    struct peer *peer)
{
	struct ibuf	*buf;
	int		 incoming = 0;
	u_int16_t	 subtype = BGP4MP_MESSAGE;

	if (peer->capa.neg.as4byte)
		subtype = BGP4MP_MESSAGE_AS4;

	/* get the direction of the message to swap address and AS fields */
	if (mrt->type == MRT_ALL_IN || mrt->type == MRT_UPDATE_IN)
		incoming = 1;

	if (mrt_dump_hdr_se(&buf, peer, MSG_PROTOCOL_BGP4MP_ET, subtype,
	    pkglen, incoming) == -1)
		return;

	if (ibuf_add(buf, pkg, pkglen) == -1) {
		log_warn("mrt_dump_bgp_msg: ibuf_add error");
		ibuf_free(buf);
		return;
	}

	ibuf_close(&mrt->wbuf, buf);
}

void
mrt_dump_state(struct mrt *mrt, u_int16_t old_state, u_int16_t new_state,
    struct peer *peer)
{
	struct ibuf	*buf;
	u_int16_t	 subtype = BGP4MP_STATE_CHANGE;

	if (peer->capa.neg.as4byte)
		subtype = BGP4MP_STATE_CHANGE_AS4;

	if (mrt_dump_hdr_se(&buf, peer, MSG_PROTOCOL_BGP4MP_ET, subtype,
	    2 * sizeof(short), 0) == -1)
		return;

	DUMP_SHORT(buf, old_state);
	DUMP_SHORT(buf, new_state);

	ibuf_close(&mrt->wbuf, buf);
	return;

fail:
	ibuf_free(buf);
}

int
mrt_attr_dump(struct ibuf *buf, struct rde_aspath *a, struct bgpd_addr *nexthop,
    int v2)
{
	struct attr	*oa;
	u_char		*pdata;
	u_int32_t	 tmp;
	int		 neednewpath = 0;
	u_int16_t	 plen, afi;
	u_int8_t	 l, safi;

	/* origin */
	if (attr_writebuf(buf, ATTR_WELL_KNOWN, ATTR_ORIGIN,
	    &a->origin, 1) == -1)
		return (-1);

	/* aspath */
	pdata = aspath_prepend(a->aspath, rde_local_as(), 0, &plen);
	if (!v2)
		pdata = aspath_deflate(pdata, &plen, &neednewpath);
	if (attr_writebuf(buf, ATTR_WELL_KNOWN, ATTR_ASPATH, pdata,
	    plen) == -1) {
		free(pdata);
		return (-1);
	}
	free(pdata);

	if (nexthop && nexthop->aid == AID_INET) {
		/* nexthop, already network byte order */
		if (attr_writebuf(buf, ATTR_WELL_KNOWN, ATTR_NEXTHOP,
		    &nexthop->v4.s_addr, 4) ==	-1)
			return (-1);
	}

	/* MED, non transitive */
	if (a->med != 0) {
		tmp = htonl(a->med);
		if (attr_writebuf(buf, ATTR_OPTIONAL, ATTR_MED, &tmp, 4) == -1)
			return (-1);
	}

	/* local preference */
	tmp = htonl(a->lpref);
	if (attr_writebuf(buf, ATTR_WELL_KNOWN, ATTR_LOCALPREF, &tmp, 4) == -1)
		return (-1);

	/* dump all other path attributes without modification */
	for (l = 0; l < a->others_len; l++) {
		if ((oa = a->others[l]) == NULL)
			break;
		if (attr_writebuf(buf, oa->flags, oa->type,
		    oa->data, oa->len) == -1)
			return (-1);
	}

	if (nexthop && nexthop->aid != AID_INET) {
		struct ibuf *nhbuf;

		if ((nhbuf = ibuf_dynamic(0, UCHAR_MAX)) == NULL)
			return (-1);
		if (!v2) {
			if (aid2afi(nexthop->aid, &afi, &safi))
				return (-1);
			DUMP_SHORT(nhbuf, afi);
			DUMP_BYTE(nhbuf, safi);
		}
		switch (nexthop->aid) {
		case AID_INET6:
			DUMP_BYTE(nhbuf, sizeof(struct in6_addr));
			if (ibuf_add(nhbuf, &nexthop->v6,
			    sizeof(struct in6_addr)) == -1) {
			}
			break;
		case AID_VPN_IPv4:
			DUMP_BYTE(nhbuf, sizeof(u_int64_t) +
			    sizeof(struct in_addr));
			DUMP_NLONG(nhbuf, 0);	/* set RD to 0 */
			DUMP_NLONG(nhbuf, 0);
			DUMP_NLONG(nhbuf, nexthop->v4.s_addr);
			break;
		case AID_VPN_IPv6:
			DUMP_BYTE(nhbuf, sizeof(u_int64_t) +
			    sizeof(struct in6_addr));
			DUMP_NLONG(nhbuf, 0);	/* set RD to 0 */
			DUMP_NLONG(nhbuf, 0);
			if (ibuf_add(nhbuf, &nexthop->v6,
			    sizeof(struct in6_addr)) == -1) {
			}
			break;
		}
		if (!v2)
			DUMP_BYTE(nhbuf, 0);
		if (attr_writebuf(buf, ATTR_OPTIONAL, ATTR_MP_REACH_NLRI,
		    nhbuf->buf, ibuf_size(nhbuf)) == -1) {
fail:
			ibuf_free(nhbuf);
			return (-1);
		}
		ibuf_free(nhbuf);
	}

	if (neednewpath) {
		pdata = aspath_prepend(a->aspath, rde_local_as(), 0, &plen);
		if (plen != 0)
			if (attr_writebuf(buf, ATTR_OPTIONAL|ATTR_TRANSITIVE,
			    ATTR_AS4_PATH, pdata, plen) == -1) {
				free(pdata);
				return (-1);
			}
		free(pdata);
	}

	return (0);
}

int
mrt_dump_entry_mp(struct mrt *mrt, struct prefix *p, u_int16_t snum,
    struct rde_peer *peer)
{
	struct ibuf	*buf, *hbuf = NULL, *h2buf = NULL;
	struct nexthop	*n;
	struct bgpd_addr addr, nexthop, *nh;
	u_int16_t	 len;
	u_int8_t	 aid;

	if ((buf = ibuf_dynamic(0, MAX_PKTSIZE)) == NULL) {
		log_warn("mrt_dump_entry_mp: ibuf_dynamic");
		return (-1);
	}

	if (mrt_attr_dump(buf, prefix_aspath(p), NULL, 0) == -1) {
		log_warnx("mrt_dump_entry_mp: mrt_attr_dump error");
		goto fail;
	}
	len = ibuf_size(buf);

	if ((h2buf = ibuf_dynamic(MRT_BGP4MP_IPv4_HEADER_SIZE +
	    MRT_BGP4MP_IPv4_ENTRY_SIZE, MRT_BGP4MP_IPv6_HEADER_SIZE +
	    MRT_BGP4MP_IPv6_ENTRY_SIZE + MRT_BGP4MP_MAX_PREFIXLEN)) == NULL) {
		log_warn("mrt_dump_entry_mp: ibuf_dynamic");
		goto fail;
	}

	DUMP_SHORT(h2buf, peer->conf.local_short_as);
	DUMP_SHORT(h2buf, peer->short_as);
	DUMP_SHORT(h2buf, /* ifindex */ 0);

	/* XXX is this for peer self? */
	aid = peer->remote_addr.aid == AID_UNSPEC ? p->re->prefix->aid :
	     peer->remote_addr.aid;
	switch (aid) {
	case AID_INET:
		DUMP_SHORT(h2buf, AFI_IPv4);
		DUMP_NLONG(h2buf, peer->local_v4_addr.v4.s_addr);
		DUMP_NLONG(h2buf, peer->remote_addr.v4.s_addr);
		break;
	case AID_INET6:
		DUMP_SHORT(h2buf, AFI_IPv6);
		if (ibuf_add(h2buf, &peer->local_v6_addr.v6,
		    sizeof(struct in6_addr)) == -1 ||
		    ibuf_add(h2buf, &peer->remote_addr.v6,
		    sizeof(struct in6_addr)) == -1) {
			log_warn("mrt_dump_entry_mp: ibuf_add error");
			goto fail;
		}
		break;
	default:
		log_warnx("king bula found new AF in mrt_dump_entry_mp");
		goto fail;
	}

	DUMP_SHORT(h2buf, 0);		/* view */
	DUMP_SHORT(h2buf, 1);		/* status */
	DUMP_LONG(h2buf, p->lastchange);	/* originated */

	pt_getaddr(p->re->prefix, &addr);

	n = prefix_nexthop(p);
	if (n == NULL) {
		bzero(&nexthop, sizeof(struct bgpd_addr));
		nexthop.aid = addr.aid;
		nh = &nexthop;
	} else
		nh = &n->exit_nexthop;

	switch (addr.aid) {
	case AID_INET:
		DUMP_SHORT(h2buf, AFI_IPv4);	/* afi */
		DUMP_BYTE(h2buf, SAFI_UNICAST);	/* safi */
		DUMP_BYTE(h2buf, 4);		/* nhlen */
		DUMP_NLONG(h2buf, nh->v4.s_addr);	/* nexthop */
		break;
	case AID_INET6:
		DUMP_SHORT(h2buf, AFI_IPv6);	/* afi */
		DUMP_BYTE(h2buf, SAFI_UNICAST);	/* safi */
		DUMP_BYTE(h2buf, 16);		/* nhlen */
		if (ibuf_add(h2buf, &nh->v6, sizeof(struct in6_addr)) == -1) {
			log_warn("mrt_dump_entry_mp: ibuf_add error");
			goto fail;
		}
		break;
	default:
		log_warnx("king bula found new AF in mrt_dump_entry_mp");
		goto fail;
	}

	if (prefix_writebuf(h2buf, &addr, p->re->prefix->prefixlen) == -1) {
		log_warn("mrt_dump_entry_mp: prefix_writebuf error");
		goto fail;
	}

	DUMP_SHORT(h2buf, len);
	len += ibuf_size(h2buf);

	if (mrt_dump_hdr_rde(&hbuf, MSG_PROTOCOL_BGP4MP, BGP4MP_ENTRY,
	    len) == -1)
		goto fail;

	ibuf_close(&mrt->wbuf, hbuf);
	ibuf_close(&mrt->wbuf, h2buf);
	ibuf_close(&mrt->wbuf, buf);

	return (len + MRT_HEADER_SIZE);

fail:
	ibuf_free(hbuf);
	ibuf_free(h2buf);
	ibuf_free(buf);
	return (-1);
}

int
mrt_dump_entry(struct mrt *mrt, struct prefix *p, u_int16_t snum,
    struct rde_peer *peer)
{
	struct ibuf	*buf, *hbuf;
	struct nexthop	*nexthop;
	struct bgpd_addr addr, *nh;
	size_t		 len;
	u_int16_t	 subtype;
	u_int8_t	 dummy;

	if (p->re->prefix->aid != peer->remote_addr.aid &&
	    p->re->prefix->aid != AID_INET && p->re->prefix->aid != AID_INET6)
		/* only able to dump pure IPv4/IPv6 */
		return (0);

	if ((buf = ibuf_dynamic(0, MAX_PKTSIZE)) == NULL) {
		log_warn("mrt_dump_entry: ibuf_dynamic");
		return (-1);
	}

	nexthop = prefix_nexthop(p);
	if (nexthop == NULL) {
		bzero(&addr, sizeof(struct bgpd_addr));
		addr.aid = p->re->prefix->aid;
		nh = &addr;
	} else
		nh = &nexthop->exit_nexthop;
	if (mrt_attr_dump(buf, prefix_aspath(p), nh, 0) == -1) {
		log_warnx("mrt_dump_entry: mrt_attr_dump error");
		ibuf_free(buf);
		return (-1);
	}
	len = ibuf_size(buf);
	aid2afi(p->re->prefix->aid, &subtype, &dummy);
	if (mrt_dump_hdr_rde(&hbuf, MSG_TABLE_DUMP, subtype, len) == -1) {
		ibuf_free(buf);
		return (-1);
	}

	DUMP_SHORT(hbuf, 0);
	DUMP_SHORT(hbuf, snum);

	pt_getaddr(p->re->prefix, &addr);
	switch (p->re->prefix->aid) {
	case AID_INET:
		DUMP_NLONG(hbuf, addr.v4.s_addr);
		break;
	case AID_INET6:
		if (ibuf_add(hbuf, &addr.v6, sizeof(struct in6_addr)) == -1) {
			log_warn("mrt_dump_entry: ibuf_add error");
			goto fail;
		}
		break;
	}
	DUMP_BYTE(hbuf, p->re->prefix->prefixlen);

	DUMP_BYTE(hbuf, 1);		/* state */
	DUMP_LONG(hbuf, p->lastchange);	/* originated */
	switch (p->re->prefix->aid) {
	case AID_INET:
		DUMP_NLONG(hbuf, peer->remote_addr.v4.s_addr);
		break;
	case AID_INET6:
		if (ibuf_add(hbuf, &peer->remote_addr.v6,
		    sizeof(struct in6_addr)) == -1) {
			log_warn("mrt_dump_entry: ibuf_add error");
			goto fail;
		}
		break;
	}
	DUMP_SHORT(hbuf, peer->short_as);
	DUMP_SHORT(hbuf, len);

	ibuf_close(&mrt->wbuf, hbuf);
	ibuf_close(&mrt->wbuf, buf);

	return (len + MRT_HEADER_SIZE);

fail:
	ibuf_free(hbuf);
	ibuf_free(buf);
	return (-1);
}

int
mrt_dump_entry_v2(struct mrt *mrt, struct rib_entry *re, u_int32_t snum)
{
	struct ibuf	*buf, *hbuf = NULL;
	struct prefix	*p;
	struct bgpd_addr addr;
	size_t		 len, off;
	u_int16_t	 subtype, nump;

	switch (re->prefix->aid) {
	case AID_INET:
		subtype = MRT_DUMP_V2_RIB_IPV4_UNICAST;
		break;
	case AID_INET6:
		subtype = MRT_DUMP_V2_RIB_IPV6_UNICAST;
		break;
	default:
		subtype = MRT_DUMP_V2_RIB_GENERIC;
		break;
	}

	if ((buf = ibuf_dynamic(0, UINT_MAX)) == NULL) {
		log_warn("%s: ibuf_dynamic", __func__);
		return (-1);
	}

	DUMP_LONG(buf, snum);
	pt_getaddr(re->prefix, &addr);
	if (subtype == MRT_DUMP_V2_RIB_GENERIC) {
		u_int16_t afi;
		u_int8_t safi;

		aid2afi(re->prefix->aid, &afi, &safi);
		DUMP_SHORT(buf, afi);
		DUMP_BYTE(buf, safi);
	}
	if (prefix_writebuf(buf, &addr, re->prefix->prefixlen) == -1) {
		log_warn("%s: prefix_writebuf error", __func__);
		goto fail;
	}

	off = ibuf_size(buf);
	if (ibuf_reserve(buf, sizeof(nump)) == NULL) {
		log_warn("%s: ibuf_reserve error", __func__);
		goto fail;
	}
	nump = 0;
	LIST_FOREACH(p, &re->prefix_h, rib_l) {
		struct nexthop		*nexthop;
		struct bgpd_addr	*nh;
		struct ibuf		*tbuf;

		nexthop = prefix_nexthop(p);
		if (nexthop == NULL) {
			bzero(&addr, sizeof(struct bgpd_addr));
			addr.aid = re->prefix->aid;
			nh = &addr;
		} else
			nh = &nexthop->exit_nexthop;

		DUMP_SHORT(buf, prefix_peer(p)->mrt_idx);
		DUMP_LONG(buf, p->lastchange); /* originated */

		if ((tbuf = ibuf_dynamic(0, MAX_PKTSIZE)) == NULL) {
			log_warn("%s: ibuf_dynamic", __func__);
			return (-1);
		}
		if (mrt_attr_dump(tbuf, prefix_aspath(p), nh, 1) == -1) {
			log_warnx("%s: mrt_attr_dump error", __func__);
			ibuf_free(buf);
			return (-1);
		}
		len = ibuf_size(tbuf);
		DUMP_SHORT(buf, (u_int16_t)len);
		if (ibuf_add(buf, tbuf->buf, ibuf_size(tbuf)) == -1) {
			log_warn("%s: ibuf_add error", __func__);
			ibuf_free(tbuf);
			return (-1);
		}
		ibuf_free(tbuf);
		nump++;
	}
	nump = htons(nump);
	memcpy(ibuf_seek(buf, off, sizeof(nump)), &nump, sizeof(nump));

	len = ibuf_size(buf);
	if (mrt_dump_hdr_rde(&hbuf, MSG_TABLE_DUMP_V2, subtype, len) == -1) {
		ibuf_free(buf);
		return (-1);
	}

	ibuf_close(&mrt->wbuf, hbuf);
	ibuf_close(&mrt->wbuf, buf);

	return (0);
fail:
	ibuf_free(hbuf);
	ibuf_free(buf);
	return (-1);
}

int
mrt_dump_v2_hdr(struct mrt *mrt, struct bgpd_config *conf,
    struct rde_peer_head *ph)
{
	struct rde_peer	*peer;
	struct ibuf	*buf, *hbuf = NULL;
	size_t		 len, off;
	u_int16_t	 nlen, nump;

	if ((buf = ibuf_dynamic(0, UINT_MAX)) == NULL) {
		log_warn("%s: ibuf_dynamic", __func__);
		return (-1);
	}

	DUMP_NLONG(buf, conf->bgpid);
	nlen = strlen(mrt->rib);
	if (nlen > 0)
		nlen += 1;
	DUMP_SHORT(buf, nlen);
	if (ibuf_add(buf, mrt->rib, nlen) == -1) {
		log_warn("%s: ibuf_add error", __func__);
		goto fail;
	}

	off = ibuf_size(buf);
	if (ibuf_reserve(buf, sizeof(nump)) == NULL) {
		log_warn("%s: ibuf_reserve error", __func__);
		goto fail;
	}
	nump = 0;
	LIST_FOREACH(peer, ph, peer_l) {
		peer->mrt_idx = nump;
		if (mrt_dump_peer(buf, peer) == -1)
			goto fail;
		nump++;
	}
	nump = htons(nump);
	memcpy(ibuf_seek(buf, off, sizeof(nump)), &nump, sizeof(nump));

	len = ibuf_size(buf);
	if (mrt_dump_hdr_rde(&hbuf, MSG_TABLE_DUMP_V2,
	    MRT_DUMP_V2_PEER_INDEX_TABLE, len) == -1)
		goto fail;

	ibuf_close(&mrt->wbuf, hbuf);
	ibuf_close(&mrt->wbuf, buf);

	return (0);
fail:
	ibuf_free(hbuf);
	ibuf_free(buf);
	return (-1);
}

int
mrt_dump_peer(struct ibuf *buf, struct rde_peer *peer)
{
	u_int8_t	type = 0;

	if (peer->capa.as4byte)
		type |= MRT_DUMP_V2_PEER_BIT_A;
	if (peer->remote_addr.aid == AID_INET6)
		type |= MRT_DUMP_V2_PEER_BIT_I;

	DUMP_BYTE(buf, type);
	DUMP_LONG(buf, peer->remote_bgpid);

	switch (peer->remote_addr.aid) {
	case AID_INET:
		DUMP_NLONG(buf, peer->remote_addr.v4.s_addr);
		break;
	case AID_INET6:
		if (ibuf_add(buf, &peer->remote_addr.v6,
		    sizeof(struct in6_addr)) == -1) {
			log_warn("mrt_dump_peer: ibuf_add error");
			goto fail;
		}
		break;
	case AID_UNSPEC: /* XXX special handling for peer_self? */
		DUMP_NLONG(buf, 0);
		break;
	default:
		log_warnx("king bula found new AF in mrt_dump_entry_mp");
		goto fail;
	}

	if (peer->capa.as4byte)
		DUMP_LONG(buf, peer->conf.remote_as);
	else
		DUMP_SHORT(buf, peer->short_as);

	return (0);
fail:
	return (-1);
}

void
mrt_dump_upcall(struct rib_entry *re, void *ptr)
{
	struct mrt		*mrtbuf = ptr;
	struct prefix		*p;

	if (mrtbuf->type == MRT_TABLE_DUMP_V2) {
		mrt_dump_entry_v2(mrtbuf, re, mrtbuf->seqnum++);
		return;
	}

	/*
	 * dump all prefixes even the inactive ones. That is the way zebra
	 * dumps the table so we do the same. If only the active route should
	 * be dumped p should be set to p = pt->active.
	 */
	LIST_FOREACH(p, &re->prefix_h, rib_l) {
		if (mrtbuf->type == MRT_TABLE_DUMP)
			mrt_dump_entry(mrtbuf, p, mrtbuf->seqnum++,
			    prefix_peer(p));
		else
			mrt_dump_entry_mp(mrtbuf, p, mrtbuf->seqnum++,
			    prefix_peer(p));
	}
}

void
mrt_done(struct mrt *mrtbuf)
{
	mrtbuf->state = MRT_STATE_REMOVE;
}

int
mrt_dump_hdr_se(struct ibuf ** bp, struct peer *peer, u_int16_t type,
    u_int16_t subtype, u_int32_t len, int swap)
{
	struct timespec	time;

	if ((*bp = ibuf_dynamic(MRT_ET_HEADER_SIZE, MRT_ET_HEADER_SIZE +
	    MRT_BGP4MP_AS4_IPv6_HEADER_SIZE + len)) == NULL) {
		log_warn("mrt_dump_hdr_se: ibuf_dynamic error");
		return (-1);
	}

	clock_gettime(CLOCK_REALTIME, &time);

	DUMP_LONG(*bp, time.tv_sec);
	DUMP_SHORT(*bp, type);
	DUMP_SHORT(*bp, subtype);

	switch (peer->sa_local.ss_family) {
	case AF_INET:
		if (subtype == BGP4MP_STATE_CHANGE_AS4 ||
		    subtype == BGP4MP_MESSAGE_AS4)
			len += MRT_BGP4MP_ET_AS4_IPv4_HEADER_SIZE;
		else
			len += MRT_BGP4MP_ET_IPv4_HEADER_SIZE;
		break;
	case AF_INET6:
		if (subtype == BGP4MP_STATE_CHANGE_AS4 ||
		    subtype == BGP4MP_MESSAGE_AS4)
			len += MRT_BGP4MP_ET_AS4_IPv6_HEADER_SIZE;
		else
			len += MRT_BGP4MP_ET_IPv6_HEADER_SIZE;
		break;
	case 0:
		goto fail;
	default:
		log_warnx("king bula found new AF in mrt_dump_hdr_se");
		goto fail;
	}

	DUMP_LONG(*bp, len);
	/* milisecond field use by the _ET format */
	DUMP_LONG(*bp, time.tv_nsec / 1000);

	if (subtype == BGP4MP_STATE_CHANGE_AS4 ||
	    subtype == BGP4MP_MESSAGE_AS4) {
		if (!swap)
			DUMP_LONG(*bp, peer->conf.local_as);
		DUMP_LONG(*bp, peer->conf.remote_as);
		if (swap)
			DUMP_LONG(*bp, peer->conf.local_as);
	} else {
		if (!swap)
			DUMP_SHORT(*bp, peer->conf.local_short_as);
		DUMP_SHORT(*bp, peer->short_as);
		if (swap)
			DUMP_SHORT(*bp, peer->conf.local_short_as);
	}

	DUMP_SHORT(*bp, /* ifindex */ 0);

	switch (peer->sa_local.ss_family) {
	case AF_INET:
		DUMP_SHORT(*bp, AFI_IPv4);
		if (!swap)
			DUMP_NLONG(*bp, ((struct sockaddr_in *)
			    &peer->sa_local)->sin_addr.s_addr);
		DUMP_NLONG(*bp,
		    ((struct sockaddr_in *)&peer->sa_remote)->sin_addr.s_addr);
		if (swap)
			DUMP_NLONG(*bp, ((struct sockaddr_in *)
			    &peer->sa_local)->sin_addr.s_addr);
		break;
	case AF_INET6:
		DUMP_SHORT(*bp, AFI_IPv6);
		if (!swap)
			if (ibuf_add(*bp, &((struct sockaddr_in6 *)
			    &peer->sa_local)->sin6_addr,
			    sizeof(struct in6_addr)) == -1) {
				log_warn("mrt_dump_hdr_se: ibuf_add error");
				goto fail;
			}
		if (ibuf_add(*bp,
		    &((struct sockaddr_in6 *)&peer->sa_remote)->sin6_addr,
		    sizeof(struct in6_addr)) == -1) {
			log_warn("mrt_dump_hdr_se: ibuf_add error");
			goto fail;
		}
		if (swap)
			if (ibuf_add(*bp, &((struct sockaddr_in6 *)
			    &peer->sa_local)->sin6_addr,
			    sizeof(struct in6_addr)) == -1) {
				log_warn("mrt_dump_hdr_se: ibuf_add error");
				goto fail;
			}
		break;
	}

	return (0);

fail:
	ibuf_free(*bp);
	return (-1);
}

int
mrt_dump_hdr_rde(struct ibuf **bp, u_int16_t type, u_int16_t subtype,
    u_int32_t len)
{
	time_t		 now;

	if ((*bp = ibuf_dynamic(MRT_HEADER_SIZE, MRT_HEADER_SIZE +
	    MRT_BGP4MP_AS4_IPv6_HEADER_SIZE + MRT_BGP4MP_IPv6_ENTRY_SIZE)) ==
	    NULL) {
		log_warn("mrt_dump_hdr_rde: ibuf_dynamic error");
		return (-1);
	}

	now = time(NULL);
	DUMP_LONG(*bp, now);
	DUMP_SHORT(*bp, type);
	DUMP_SHORT(*bp, subtype);

	switch (type) {
	case MSG_TABLE_DUMP:
		switch (subtype) {
		case AFI_IPv4:
			len += MRT_DUMP_HEADER_SIZE;
			break;
		case AFI_IPv6:
			len += MRT_DUMP_HEADER_SIZE_V6;
			break;
		}
		DUMP_LONG(*bp, len);
		break;
	case MSG_PROTOCOL_BGP4MP:
	case MSG_TABLE_DUMP_V2:
		DUMP_LONG(*bp, len);
		break;
	default:
		log_warnx("mrt_dump_hdr_rde: unsupported type");
		goto fail;
	}
	return (0);

fail:
	ibuf_free(*bp);
	return (-1);
}

void
mrt_write(struct mrt *mrt)
{
	int	r;

	if ((r = ibuf_write(&mrt->wbuf)) < 0 && errno != EAGAIN) {
		log_warn("mrt dump aborted, mrt_write");
		mrt_clean(mrt);
		mrt_done(mrt);
	}
}

void
mrt_clean(struct mrt *mrt)
{
	struct ibuf	*b;

	close(mrt->wbuf.fd);
	while ((b = TAILQ_FIRST(&mrt->wbuf.bufs))) {
		TAILQ_REMOVE(&mrt->wbuf.bufs, b, entry);
		ibuf_free(b);
	}
	mrt->wbuf.queued = 0;
}

static struct imsgbuf	*mrt_imsgbuf[2];

void
mrt_init(struct imsgbuf *rde, struct imsgbuf *se)
{
	mrt_imsgbuf[RDEIDX] = rde;
	mrt_imsgbuf[SEIDX] = se;
}

int
mrt_open(struct mrt *mrt, time_t now)
{
	enum imsg_type	type;
	int		fd;

	if (strftime(MRT2MC(mrt)->file, sizeof(MRT2MC(mrt)->file),
	    MRT2MC(mrt)->name, localtime(&now)) == 0) {
		log_warnx("mrt_open: strftime conversion failed");
		return (-1);
	}

	fd = open(MRT2MC(mrt)->file,
	    O_WRONLY|O_NONBLOCK|O_CREAT|O_TRUNC, 0644);
	if (fd == -1) {
		log_warn("mrt_open %s", MRT2MC(mrt)->file);
		return (1);
	}

	if (mrt->state == MRT_STATE_OPEN)
		type = IMSG_MRT_OPEN;
	else
		type = IMSG_MRT_REOPEN;

	if (imsg_compose(mrt_imsgbuf[TYPE2IDX(mrt->type)], type, 0, 0, fd,
	    mrt, sizeof(struct mrt)) == -1)
		log_warn("mrt_open");

	return (1);
}

int
mrt_timeout(struct mrt_head *mrt)
{
	struct mrt	*m;
	time_t		 now;
	int		 timeout = MRT_MAX_TIMEOUT;

	now = time(NULL);
	LIST_FOREACH(m, mrt, entry) {
		if (m->state == MRT_STATE_RUNNING &&
		    MRT2MC(m)->ReopenTimerInterval != 0) {
			if (MRT2MC(m)->ReopenTimer <= now) {
				mrt_open(m, now);
				MRT2MC(m)->ReopenTimer =
				    now + MRT2MC(m)->ReopenTimerInterval;
			}
			if (MRT2MC(m)->ReopenTimer - now < timeout)
				timeout = MRT2MC(m)->ReopenTimer - now;
		}
	}
	return (timeout > 0 ? timeout : 0);
}

void
mrt_reconfigure(struct mrt_head *mrt)
{
	struct mrt	*m, *xm;
	time_t		 now;

	now = time(NULL);
	for (m = LIST_FIRST(mrt); m != NULL; m = xm) {
		xm = LIST_NEXT(m, entry);
		if (m->state == MRT_STATE_OPEN ||
		    m->state == MRT_STATE_REOPEN) {
			if (mrt_open(m, now) == -1)
				continue;
			if (MRT2MC(m)->ReopenTimerInterval != 0)
				MRT2MC(m)->ReopenTimer =
				    now + MRT2MC(m)->ReopenTimerInterval;
			m->state = MRT_STATE_RUNNING;
		}
		if (m->state == MRT_STATE_REMOVE) {
			if (imsg_compose(mrt_imsgbuf[TYPE2IDX(m->type)],
			    IMSG_MRT_CLOSE, 0, 0, -1, m, sizeof(struct mrt)) ==
			    -1)
				log_warn("mrt_reconfigure");
			LIST_REMOVE(m, entry);
			free(m);
			continue;
		}
	}
}

void
mrt_handler(struct mrt_head *mrt)
{
	struct mrt	*m;
	time_t		 now;

	now = time(NULL);
	LIST_FOREACH(m, mrt, entry) {
		if (m->state == MRT_STATE_RUNNING &&
		    (MRT2MC(m)->ReopenTimerInterval != 0 ||
		     m->type == MRT_TABLE_DUMP ||
		     m->type == MRT_TABLE_DUMP_MP ||
		     m->type == MRT_TABLE_DUMP_V2)) {
			if (mrt_open(m, now) == -1)
				continue;
			MRT2MC(m)->ReopenTimer =
			    now + MRT2MC(m)->ReopenTimerInterval;
		}
	}
}

struct mrt *
mrt_get(struct mrt_head *c, struct mrt *m)
{
	struct mrt	*t;

	LIST_FOREACH(t, c, entry) {
		if (t->type != m->type)
			continue;
		if (strcmp(t->rib, m->rib))
			continue;
		if (t->peer_id == m->peer_id &&
		    t->group_id == m->group_id)
			return (t);
	}
	return (NULL);
}

int
mrt_mergeconfig(struct mrt_head *xconf, struct mrt_head *nconf)
{
	struct mrt	*m, *xm;

	/* both lists here are actually struct mrt_conifg nodes */
	LIST_FOREACH(m, nconf, entry) {
		if ((xm = mrt_get(xconf, m)) == NULL) {
			/* NEW */
			if ((xm = malloc(sizeof(struct mrt_config))) == NULL)
				fatal("mrt_mergeconfig");
			memcpy(xm, m, sizeof(struct mrt_config));
			xm->state = MRT_STATE_OPEN;
			LIST_INSERT_HEAD(xconf, xm, entry);
		} else {
			/* MERGE */
			if (strlcpy(MRT2MC(xm)->name, MRT2MC(m)->name,
			    sizeof(MRT2MC(xm)->name)) >=
			    sizeof(MRT2MC(xm)->name))
				fatalx("mrt_mergeconfig: strlcpy");
			MRT2MC(xm)->ReopenTimerInterval =
			    MRT2MC(m)->ReopenTimerInterval;
			xm->state = MRT_STATE_REOPEN;
		}
	}

	LIST_FOREACH(xm, xconf, entry)
		if (mrt_get(nconf, xm) == NULL)
			/* REMOVE */
			xm->state = MRT_STATE_REMOVE;

	/* free config */
	while ((m = LIST_FIRST(nconf)) != NULL) {
		LIST_REMOVE(m, entry);
		free(m);
	}

	return (0);
}
