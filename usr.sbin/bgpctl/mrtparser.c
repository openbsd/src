/*	$OpenBSD: mrtparser.c,v 1.6 2015/01/09 08:09:39 henning Exp $ */
/*
 * Copyright (c) 2011 Claudio Jeker <claudio@openbsd.org>
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "mrt.h"
#include "mrtparser.h"

void	*mrt_read_msg(int, struct mrt_hdr *);
size_t	 mrt_read_buf(int, void *, size_t);

struct mrt_peer	*mrt_parse_v2_peer(struct mrt_hdr *, void *);
struct mrt_rib	*mrt_parse_v2_rib(struct mrt_hdr *, void *);
int	mrt_parse_dump(struct mrt_hdr *, void *, struct mrt_peer **,
	    struct mrt_rib **);
int	mrt_parse_dump_mp(struct mrt_hdr *, void *, struct mrt_peer **,
	    struct mrt_rib **);
int	mrt_extract_attr(struct mrt_rib_entry *, u_char *, int, sa_family_t,
	    int);

void	mrt_free_peers(struct mrt_peer *);
void	mrt_free_rib(struct mrt_rib *);
void	mrt_free_bgp_state(struct mrt_bgp_state *);
void	mrt_free_bgp_msg(struct mrt_bgp_msg *);

u_char *mrt_aspath_inflate(void *, u_int16_t, u_int16_t *);
int	mrt_extract_addr(void *, u_int, union mrt_addr *, sa_family_t);

void *
mrt_read_msg(int fd, struct mrt_hdr *hdr)
{
	void *buf;

	bzero(hdr, sizeof(*hdr));
	if (mrt_read_buf(fd, hdr, sizeof(*hdr)) != sizeof(*hdr))
		return (NULL);

	if ((buf = malloc(ntohl(hdr->length))) == NULL)
		err(1, "malloc(%d)", hdr->length);

	if (mrt_read_buf(fd, buf, ntohl(hdr->length)) != ntohl(hdr->length)) {
		free(buf);
		return (NULL);
	}
	return (buf);
}

size_t
mrt_read_buf(int fd, void *buf, size_t len)
{
	char *b = buf;
	ssize_t n;

	while (len > 0) {
		if ((n = read(fd, b, len)) == -1) {
			if (errno == EINTR)
				continue;
			err(1, "read");
		}
		if (n == 0)
			break;
		b += n;
		len -= n;
	}

	return (b - (char *)buf);
}

void
mrt_parse(int fd, struct mrt_parser *p, int verbose)
{
	struct mrt_hdr	h;
	struct mrt_peer	*pctx = NULL;
	struct mrt_rib	*r;
	void		*msg;

	while ((msg = mrt_read_msg(fd, &h))) {
		switch (ntohs(h.type)) {
		case MSG_NULL:
		case MSG_START:
		case MSG_DIE:
		case MSG_I_AM_DEAD:
		case MSG_PEER_DOWN:
		case MSG_PROTOCOL_BGP:
		case MSG_PROTOCOL_IDRP:
		case MSG_PROTOCOL_BGP4PLUS:
		case MSG_PROTOCOL_BGP4PLUS1:
			if (verbose)
				printf("deprecated MRT type %d\n",
				    ntohs(h.type));
			break;
		case MSG_PROTOCOL_RIP:
		case MSG_PROTOCOL_RIPNG:
		case MSG_PROTOCOL_OSPF:
		case MSG_PROTOCOL_ISIS_ET:
		case MSG_PROTOCOL_ISIS:
		case MSG_PROTOCOL_OSPFV3_ET:
		case MSG_PROTOCOL_OSPFV3:
			if (verbose)
				printf("unsuported MRT type %d\n",
				    ntohs(h.type));
			break;
		case MSG_TABLE_DUMP:
			switch (ntohs(h.subtype)) {
			case MRT_DUMP_AFI_IP:
			case MRT_DUMP_AFI_IPv6:
				if (p->dump == NULL)
					break;
				if (mrt_parse_dump(&h, msg, &pctx, &r) == 0) {
					p->dump(r, pctx, p->arg);
					mrt_free_rib(r);
				}
				break;
			default:
				if (verbose)
					printf("unknown AFI %d in table dump\n",
					    ntohs(h.subtype));
				break;
			}
			break;
		case MSG_TABLE_DUMP_V2:
			switch (ntohs(h.subtype)) {
			case MRT_DUMP_V2_PEER_INDEX_TABLE:
				if (p->dump == NULL)
					break;
				if (pctx)
					mrt_free_peers(pctx);
				pctx = mrt_parse_v2_peer(&h, msg);
				break;
			case MRT_DUMP_V2_RIB_IPV4_UNICAST:
			case MRT_DUMP_V2_RIB_IPV4_MULTICAST:
			case MRT_DUMP_V2_RIB_IPV6_UNICAST:
			case MRT_DUMP_V2_RIB_IPV6_MULTICAST:
			case MRT_DUMP_V2_RIB_GENERIC:
				if (p->dump == NULL)
					break;
				r = mrt_parse_v2_rib(&h, msg);
				if (r) {
					p->dump(r, pctx, p->arg);
					mrt_free_rib(r);
				}
				break;
			default:
				if (verbose)
					printf("unhandled BGP4MP subtype %d\n",
					    ntohs(h.subtype));
				break;
			}
			break;
		case MSG_PROTOCOL_BGP4MP_ET:
		case MSG_PROTOCOL_BGP4MP:
			switch (ntohs(h.subtype)) {
			case BGP4MP_STATE_CHANGE:
			case BGP4MP_STATE_CHANGE_AS4:
				/* XXX p->state(s, p->arg); */
				errx(1, "BGP4MP subtype not yet implemented");
				break;
			case BGP4MP_MESSAGE:
			case BGP4MP_MESSAGE_AS4:
			case BGP4MP_MESSAGE_LOCAL:
			case BGP4MP_MESSAGE_AS4_LOCAL:
				/* XXX p->message(m, p->arg); */
				errx(1, "BGP4MP subtype not yet implemented");
				break;
			case BGP4MP_ENTRY:
				if (p->dump == NULL)
					break;
				if (mrt_parse_dump_mp(&h, msg, &pctx, &r) ==
				    0) {
					p->dump(r, pctx, p->arg);
					mrt_free_rib(r);
				}
				break;
			default:
				if (verbose)
					printf("unhandled BGP4MP subtype %d\n",
					    ntohs(h.subtype));
				break;
			}
			break;
		default:
			if (verbose)
				printf("unknown MRT type %d\n", ntohs(h.type));
			break;
		}
		free(msg);
	}
	if (pctx)
		mrt_free_peers(pctx);
}

struct mrt_peer *
mrt_parse_v2_peer(struct mrt_hdr *hdr, void *msg)
{
	struct mrt_peer_entry	*peers = NULL;
	struct mrt_peer	*p;
	u_int8_t	*b = msg;
	u_int32_t	bid, as4;
	u_int16_t	cnt, i, as2;
	u_int		len = ntohl(hdr->length);

	if (len < 8)	/* min msg size */
		return NULL;

	p = calloc(1, sizeof(struct mrt_peer));
	if (p == NULL)
		err(1, "calloc");

	/* collector bgp id */
	memcpy(&bid, b, sizeof(bid));
	b += sizeof(bid);
	len -= sizeof(bid);
	p->bgp_id = ntohl(bid);

	/* view name length */
	memcpy(&cnt, b, sizeof(cnt));
	b += sizeof(cnt);
	len -= sizeof(cnt);
	cnt = ntohs(cnt);

	/* view name */
	if (cnt > len)
		goto fail;
	if (cnt != 0) {
		if ((p->view = malloc(cnt + 1)) == NULL)
			err(1, "malloc");
		memcpy(p->view, b, cnt);
		p->view[cnt] = 0;
	} else
		if ((p->view = strdup("")) == NULL)
			err(1, "strdup");
	b += cnt;
	len -= cnt;

	/* peer_count */
	if (len < sizeof(cnt))
		goto fail;
	memcpy(&cnt, b, sizeof(cnt));
	b += sizeof(cnt);
	len -= sizeof(cnt);
	cnt = ntohs(cnt);

	/* peer entries */
	if ((peers = calloc(cnt, sizeof(struct mrt_peer_entry))) == NULL)
		err(1, "calloc");
	for (i = 0; i < cnt; i++) {
		u_int8_t type;

		if (len < sizeof(u_int8_t) + sizeof(u_int32_t))
			goto fail;
		type = *b++;
		len -= 1;
		memcpy(&bid, b, sizeof(bid));
		b += sizeof(bid);
		len -= sizeof(bid);
		peers[i].bgp_id = ntohl(bid);

		if (type & MRT_DUMP_V2_PEER_BIT_I) {
			if (mrt_extract_addr(b, len, &peers[i].addr,
			    AF_INET6) == -1)
				goto fail;
			b += sizeof(struct in6_addr);
			len -= sizeof(struct in6_addr);
		} else {
			if (mrt_extract_addr(b, len, &peers[i].addr,
			    AF_INET) == -1)
				goto fail;
			b += sizeof(struct in_addr);
			len -= sizeof(struct in_addr);
		}

		if (type & MRT_DUMP_V2_PEER_BIT_A) {
			memcpy(&as4, b, sizeof(as4));
			b += sizeof(as4);
			len -= sizeof(as4);
			as4 = ntohl(as4);
		} else {
			memcpy(&as2, b, sizeof(as2));
			b += sizeof(as2);
			len -= sizeof(as2);
			as4 = ntohs(as2);
		}
		peers[i].asnum = as4;
	}
	p->peers = peers;
	p->npeers = cnt;
	return (p);
fail:
	mrt_free_peers(p);
	free(peers);
	return (NULL);
}

struct mrt_rib *
mrt_parse_v2_rib(struct mrt_hdr *hdr, void *msg)
{
	struct mrt_rib_entry *entries = NULL;
	struct mrt_rib	*r;
	u_int8_t	*b = msg;
	u_int		len = ntohl(hdr->length);
	u_int32_t	snum;
	u_int16_t	cnt, i;
	u_int8_t	plen;

	if (len < sizeof(snum) + 1)
		return NULL;

	r = calloc(1, sizeof(struct mrt_rib));
	if (r == NULL)
		err(1, "calloc");

	/* seq_num */
	memcpy(&snum, b, sizeof(snum));
	b += sizeof(snum);
	len -= sizeof(snum);
	r->seqnum = ntohl(snum);

	switch (ntohs(hdr->subtype)) {
	case MRT_DUMP_V2_RIB_IPV4_UNICAST:
	case MRT_DUMP_V2_RIB_IPV4_MULTICAST:
		plen = *b++;
		len -= 1;
		if (len < MRT_PREFIX_LEN(plen))
			goto fail;
		r->prefix.sin.sin_family = AF_INET;
		r->prefix.sin.sin_len = sizeof(struct sockaddr_in);
		memcpy(&r->prefix.sin.sin_addr, b, MRT_PREFIX_LEN(plen));
		b += MRT_PREFIX_LEN(plen);
		len -= MRT_PREFIX_LEN(plen);
		r->prefixlen = plen;
		break;
	case MRT_DUMP_V2_RIB_IPV6_UNICAST:
	case MRT_DUMP_V2_RIB_IPV6_MULTICAST:
		plen = *b++;
		len -= 1;
		if (len < MRT_PREFIX_LEN(plen))
			goto fail;
		r->prefix.sin6.sin6_family = AF_INET6;
		r->prefix.sin6.sin6_len = sizeof(struct sockaddr_in6);
		memcpy(&r->prefix.sin6.sin6_addr, b, MRT_PREFIX_LEN(plen));
		b += MRT_PREFIX_LEN(plen);
		len -= MRT_PREFIX_LEN(plen);
		r->prefixlen = plen;
		break;
	case MRT_DUMP_V2_RIB_GENERIC:
		/* XXX unhandled */
		errx(1, "MRT_DUMP_V2_RIB_GENERIC subtype not yet implemented");
		goto fail;
	}

	/* entries count */
	if (len < sizeof(cnt))
		goto fail;
	memcpy(&cnt, b, sizeof(cnt));
	b += sizeof(cnt);
	len -= sizeof(cnt);
	cnt = ntohs(cnt);
	r->nentries = cnt;

	/* entries */
	if ((entries = calloc(cnt, sizeof(struct mrt_rib_entry))) == NULL)
		err(1, "calloc");
	for (i = 0; i < cnt; i++) {
		u_int32_t	otm;
		u_int16_t	pix, alen;
		if (len < 2 * sizeof(u_int16_t) + sizeof(u_int32_t))
			goto fail;
		/* peer index */
		memcpy(&pix, b, sizeof(pix));
		b += sizeof(pix);
		len -= sizeof(pix);
		entries[i].peer_idx = ntohs(pix);

		/* originated */
		memcpy(&otm, b, sizeof(otm));
		b += sizeof(otm);
		len -= sizeof(otm);
		entries[i].originated = ntohl(otm);

		/* attr_len */
		memcpy(&alen, b, sizeof(alen));
		b += sizeof(alen);
		len -= sizeof(alen);
		alen = ntohs(alen);

		/* attr */
		if (len < alen)
			goto fail;
		if (mrt_extract_attr(&entries[i], b, alen,
		    r->prefix.sa.sa_family, 1) == -1)
			goto fail;
		b += alen;
		len -= alen;
	}
	r->entries = entries;
	return (r);
fail:
	mrt_free_rib(r);
	free(entries);
	return (NULL);
}

int
mrt_parse_dump(struct mrt_hdr *hdr, void *msg, struct mrt_peer **pp,
    struct mrt_rib **rp)
{
	struct mrt_peer		*p;
	struct mrt_rib		*r;
	struct mrt_rib_entry	*re;
	u_int8_t		*b = msg;
	u_int			 len = ntohl(hdr->length);
	u_int16_t		 asnum, alen;

	if (*pp == NULL) {
		*pp = calloc(1, sizeof(struct mrt_peer));
		if (*pp == NULL)
			err(1, "calloc");
		(*pp)->peers = calloc(1, sizeof(struct mrt_peer_entry));
		if ((*pp)->peers == NULL)
			err(1, "calloc");
		(*pp)->npeers = 1;
	}
	p = *pp;

	*rp = r = calloc(1, sizeof(struct mrt_rib));
	if (r == NULL)
		err(1, "calloc");
	re = calloc(1, sizeof(struct mrt_rib_entry));
	if (re == NULL)
		err(1, "calloc");
	r->nentries = 1;
	r->entries = re;
	
	if (len < 2 * sizeof(u_int16_t))
		goto fail;
	/* view */
	b += sizeof(u_int16_t);
	len -= sizeof(u_int16_t);
	/* seqnum */
	memcpy(&r->seqnum, b, sizeof(u_int16_t));
	b += sizeof(u_int16_t);
	len -= sizeof(u_int16_t);
	r->seqnum = ntohs(r->seqnum);

	switch (ntohs(hdr->subtype)) {
	case MRT_DUMP_AFI_IP:
		if (mrt_extract_addr(b, len, &r->prefix, AF_INET) == -1)
			goto fail;
		b += sizeof(struct in_addr);
		len -= sizeof(struct in_addr);
		break;
	case MRT_DUMP_AFI_IPv6:
		if (mrt_extract_addr(b, len, &r->prefix, AF_INET6) == -1)
			goto fail;
		b += sizeof(struct in6_addr);
		len -= sizeof(struct in6_addr);
		break;
	}
	if (len < 2 * sizeof(u_int32_t) + 2 * sizeof(u_int16_t) + 2)
		goto fail;
	r->prefixlen = *b++;
	len -= 1;
	/* status */
	b += 1;
	len -= 1;
	/* originated */
	memcpy(&re->originated, b, sizeof(u_int32_t));
	b += sizeof(u_int32_t);
	len -= sizeof(u_int32_t);
	re->originated = ntohl(re->originated);
	/* peer ip */
	switch (ntohs(hdr->subtype)) {
	case MRT_DUMP_AFI_IP:
		if (mrt_extract_addr(b, len, &p->peers->addr, AF_INET) == -1)
			goto fail;
		b += sizeof(struct in_addr);
		len -= sizeof(struct in_addr);
		break;
	case MRT_DUMP_AFI_IPv6:
		if (mrt_extract_addr(b, len, &p->peers->addr, AF_INET6) == -1)
			goto fail;
		b += sizeof(struct in6_addr);
		len -= sizeof(struct in6_addr);
		break;
	}
	memcpy(&asnum, b, sizeof(asnum));
	b += sizeof(asnum);
	len -= sizeof(asnum);
	p->peers->asnum = ntohs(asnum);

	memcpy(&alen, b, sizeof(alen));
	b += sizeof(alen);
	len -= sizeof(alen);
	alen = ntohs(alen);

	/* attr */
	if (len < alen)
		goto fail;
	if (mrt_extract_attr(re, b, alen, r->prefix.sa.sa_family, 0) == -1)
		goto fail;
	b += alen;
	len -= alen;

	return (0);
fail:
	mrt_free_rib(r);
	return (-1);
}

int
mrt_parse_dump_mp(struct mrt_hdr *hdr, void *msg, struct mrt_peer **pp,
    struct mrt_rib **rp)
{
	struct mrt_peer		*p;
	struct mrt_rib		*r;
	struct mrt_rib_entry	*re;
	u_int8_t		*b = msg;
	u_int			 len = ntohl(hdr->length);
	u_int16_t		 asnum, alen, afi;
	u_int8_t		 safi, nhlen;
	sa_family_t		 af;

	/* just ignore the microsec field for _ET header for now */
	if (ntohs(hdr->type) == MSG_PROTOCOL_BGP4MP_ET) {
		b = (char *)b + sizeof(u_int32_t);
		len -= sizeof(u_int32_t);
	}

	if (*pp == NULL) {
		*pp = calloc(1, sizeof(struct mrt_peer));
		if (*pp == NULL)
			err(1, "calloc");
		(*pp)->peers = calloc(1, sizeof(struct mrt_peer_entry));
		if ((*pp)->peers == NULL)
			err(1, "calloc");
		(*pp)->npeers = 1;
	}
	p = *pp;

	*rp = r = calloc(1, sizeof(struct mrt_rib));
	if (r == NULL)
		err(1, "calloc");
	re = calloc(1, sizeof(struct mrt_rib_entry));
	if (re == NULL)
		err(1, "calloc");
	r->nentries = 1;
	r->entries = re;
	
	if (len < 4 * sizeof(u_int16_t))
		goto fail;
	/* source AS */
	b += sizeof(u_int16_t);
	len -= sizeof(u_int16_t);
	/* dest AS */
	memcpy(&asnum, b, sizeof(asnum));
	b += sizeof(asnum);
	len -= sizeof(asnum);
	p->peers->asnum = ntohs(asnum);
	/* iface index */
	b += sizeof(u_int16_t);
	len -= sizeof(u_int16_t);
	/* afi */
	memcpy(&afi, b, sizeof(afi));
	b += sizeof(afi);
	len -= sizeof(afi);
	afi = ntohs(afi);

	/* source + dest ip */
	switch (afi) {
	case MRT_DUMP_AFI_IP:
		if (len < 2 * sizeof(struct in_addr))
			goto fail;
		/* source IP */
		b += sizeof(struct in_addr);
		len -= sizeof(struct in_addr);
		/* dest IP */
		if (mrt_extract_addr(b, len, &p->peers->addr, AF_INET) == -1)
			goto fail;
		b += sizeof(struct in_addr);
		len -= sizeof(struct in_addr);
		break;
	case MRT_DUMP_AFI_IPv6:
		if (len < 2 * sizeof(struct in6_addr))
			goto fail;
		/* source IP */
		b += sizeof(struct in6_addr);
		len -= sizeof(struct in6_addr);
		/* dest IP */
		if (mrt_extract_addr(b, len, &p->peers->addr, AF_INET6) == -1)
			goto fail;
		b += sizeof(struct in6_addr);
		len -= sizeof(struct in6_addr);
		break;
	}
	
	if (len < 2 * sizeof(u_int16_t) + 2 * sizeof(u_int32_t))
		goto fail;
	/* view + status */
	b += 2 * sizeof(u_int16_t);
	len -= 2 * sizeof(u_int16_t);
	/* originated */
	memcpy(&re->originated, b, sizeof(u_int32_t));
	b += sizeof(u_int32_t);
	len -= sizeof(u_int32_t);
	re->originated = ntohl(re->originated);

	/* afi */
	memcpy(&afi, b, sizeof(afi));
	b += sizeof(afi);
	len -= sizeof(afi);
	afi = ntohs(afi);
	
	/* safi */
	safi = *b++;
	len -= 1;

	switch (afi) {
	case MRT_DUMP_AFI_IP:
		if (safi == 1 || safi == 2) {
			af = AF_INET;
			break;
		} else if (safi == 128) {
			af = AF_VPNv4;
			break;
		}
		goto fail;
	case MRT_DUMP_AFI_IPv6:
		if (safi != 1 && safi != 2)
			goto fail;
		af = AF_INET6;
		break;
	default:
		goto fail;
	}

	/* nhlen */
	nhlen = *b++;
	len -= 1;

	/* nexthop */
	if (mrt_extract_addr(b, len, &re->nexthop, af) == -1)
		goto fail;
	if (len < nhlen)
		goto fail;
	b += nhlen;
	len -= nhlen;

	if (len < 1)
		goto fail;
	r->prefixlen = *b++;
	len -= 1;

	/* prefix */
	switch (af) {
	case AF_INET:
		if (len < MRT_PREFIX_LEN(r->prefixlen))
			goto fail;
		r->prefix.sin.sin_family = AF_INET;
		r->prefix.sin.sin_len = sizeof(struct sockaddr_in);
		memcpy(&r->prefix.sin.sin_addr, b,
		    MRT_PREFIX_LEN(r->prefixlen));
		b += MRT_PREFIX_LEN(r->prefixlen);
		len -= MRT_PREFIX_LEN(r->prefixlen);
		break;
	case AF_INET6:
		if (len < MRT_PREFIX_LEN(r->prefixlen))
			goto fail;
		r->prefix.sin6.sin6_family = AF_INET6;
		r->prefix.sin6.sin6_len = sizeof(struct sockaddr_in6);
		memcpy(&r->prefix.sin6.sin6_addr, b,
		    MRT_PREFIX_LEN(r->prefixlen));
		b += MRT_PREFIX_LEN(r->prefixlen);
		len -= MRT_PREFIX_LEN(r->prefixlen);
		break;
	case AF_VPNv4:
		if (len < MRT_PREFIX_LEN(r->prefixlen))
			goto fail;
		errx(1, "AF_VPNv4 handling not yet implemented");
		goto fail;
	}

	memcpy(&alen, b, sizeof(alen));
	b += sizeof(alen);
	len -= sizeof(alen);
	alen = ntohs(alen);

	/* attr */
	if (len < alen)
		goto fail;
	if (mrt_extract_attr(re, b, alen, r->prefix.sa.sa_family, 0) == -1)
		goto fail;
	b += alen;
	len -= alen;

	return (0);
fail:
	mrt_free_rib(r);
	return (-1);
}

int
mrt_extract_attr(struct mrt_rib_entry *re, u_char *a, int alen, sa_family_t af,
    int as4)
{
	struct mrt_attr	*ap;
	u_int32_t	tmp;
	u_int16_t	attr_len;
	u_int8_t	type, flags, *attr;

	do {
		if (alen < 3)
			return (-1);
		attr = a;
		flags = *a++;
		alen -= 1;
		type = *a++;
		alen -= 1;
		
		if (flags & MRT_ATTR_EXTLEN) {
			if (alen < 2)
				return (-1);
			memcpy(&attr_len, a, sizeof(attr_len));
			attr_len = ntohs(attr_len);
			a += sizeof(attr_len);
			alen -= sizeof(attr_len);
		} else {
			attr_len = *a++;
			alen -= 1;
		}
		switch (type) {
		case MRT_ATTR_ORIGIN:
			if (attr_len != 1)
				return (-1);
			re->origin = *a;
			break;
		case MRT_ATTR_ASPATH:
			if (as4) {
				re->aspath_len = attr_len;
				if ((re->aspath = malloc(attr_len)) == NULL)
					err(1, "malloc");
				memcpy(re->aspath, a, attr_len);
			} else {
				re->aspath = mrt_aspath_inflate(a, attr_len,
				    &re->aspath_len);
				if (re->aspath == NULL)
					return (-1);
			}
			break;
		case MRT_ATTR_NEXTHOP:
			if (attr_len != 4)
				return (-1);
			if (af != AF_INET)
				break;
			memcpy(&tmp, a, sizeof(tmp));
			re->nexthop.sin.sin_len = sizeof(struct sockaddr_in);
			re->nexthop.sin.sin_family = AF_INET;
			re->nexthop.sin.sin_addr.s_addr = tmp;
			break;
		case MRT_ATTR_MED:
			if (attr_len != 4)
				return (-1);
			memcpy(&tmp, a, sizeof(tmp));
			re->med = ntohl(tmp);
			break;
		case MRT_ATTR_LOCALPREF:
			if (attr_len != 4)
				return (-1);
			memcpy(&tmp, a, sizeof(tmp));
			re->local_pref = ntohl(tmp);
			break;
		case MRT_ATTR_MP_REACH_NLRI:
			/*
			 * XXX horrible hack:
			 * Once again IETF and the real world differ in the
			 * implementation. In short the abbreviated MP_NLRI
			 * hack in the standard is not used in real life.
			 * Detect the two cases by looking at the first byte
			 * of the payload (either the nexthop addr length (RFC)
			 * or the high byte of the AFI (old form)). If the
			 * first byte matches the expected nexthop length it
			 * is expected to be the RFC 6396 encoding.
			 */
			if (*a != attr_len - 1) {
				a += 3;
				alen -= 3;
				attr_len -= 3;
			}
			switch (af) {
			case AF_INET6:
				if (attr_len < sizeof(struct in6_addr) + 1)
					return (-1);
				re->nexthop.sin6.sin6_len =
				    sizeof(struct sockaddr_in6);
				re->nexthop.sin6.sin6_family = AF_INET6;
				memcpy(&re->nexthop.sin6.sin6_addr, a + 1,
				    sizeof(struct in6_addr));
				break;
			case AF_VPNv4:
				if (attr_len < sizeof(u_int64_t) +
				    sizeof(struct in_addr))
					return (-1);
				re->nexthop.svpn4.sv_len =
				    sizeof(struct sockaddr_vpn4);
				re->nexthop.svpn4.sv_family = AF_VPNv4;
				memcpy(&tmp, a + 1 + sizeof(u_int64_t),
				    sizeof(tmp));
				re->nexthop.svpn4.sv_addr.s_addr = tmp;
				break;
			}
			break;
		case MRT_ATTR_AS4PATH:
			if (!as4) {
				if (re->aspath)
					free(re->aspath);
				re->aspath_len = attr_len;
				if ((re->aspath = malloc(attr_len)) == NULL)
					err(1, "malloc");
				memcpy(re->aspath, a, attr_len);
				break;
			}
			/* FALLTHROUGH */
		default:
			re->nattrs++;
			if (re->nattrs >= UCHAR_MAX)
				err(1, "too many attributes");
			ap = reallocarray(re->attrs,
			    re->nattrs, sizeof(struct mrt_attr));
			if (ap == NULL)
				err(1, "realloc");
			re->attrs = ap;
			ap = re->attrs + re->nattrs - 1;
			ap->attr_len = a + attr_len - attr;
			if ((ap->attr = malloc(ap->attr_len)) == NULL)
				err(1, "malloc");
			memcpy(ap->attr, attr, ap->attr_len);
			break;
		}
		a += attr_len;
		alen -= attr_len;
	} while (alen > 0);

	return (0);
}

void
mrt_free_peers(struct mrt_peer *p)
{
	free(p->peers);
	free(p->view);
	free(p);
}

void
mrt_free_rib(struct mrt_rib *r)
{
	u_int16_t	i, j;

	for (i = 0; i < r->nentries && r->entries; i++) {
		for (j = 0; j < r->entries[i].nattrs; j++)
			 free(r->entries[i].attrs[j].attr);
		free(r->entries[i].attrs);
		free(r->entries[i].aspath);
	}

	free(r->entries);
	free(r);
}

void
mrt_free_bgp_state(struct mrt_bgp_state *s)
{
	free(s);
}

void
mrt_free_bgp_msg(struct mrt_bgp_msg *m)
{
	free(m->msg);
	free(m);
}

u_char *
mrt_aspath_inflate(void *data, u_int16_t len, u_int16_t *newlen)
{
	u_int8_t	*seg, *nseg, *ndata;
	u_int16_t	 seg_size, olen, nlen;
	u_int8_t	 seg_len;

	/* first calculate the length of the aspath */
	seg = data;
	nlen = 0;
	for (olen = len; olen > 0; olen -= seg_size, seg += seg_size) {
		seg_len = seg[1];
		seg_size = 2 + sizeof(u_int16_t) * seg_len;
		nlen += 2 + sizeof(u_int32_t) * seg_len;

		if (seg_size > olen)
			return NULL;
	}

	*newlen = nlen;
	if ((ndata = malloc(nlen)) == NULL)
		err(1, "malloc");

	/* then copy the aspath */
	seg = data;
	for (nseg = ndata; nseg < ndata + nlen; ) {
		*nseg++ = *seg++;
		*nseg++ = seg_len = *seg++;
		for (; seg_len > 0; seg_len--) {
			*nseg++ = 0;
			*nseg++ = 0;
			*nseg++ = *seg++;
			*nseg++ = *seg++;
		}
	}

	return (ndata);
}

int
mrt_extract_addr(void *msg, u_int len, union mrt_addr *addr, sa_family_t af)
{
	u_int8_t	*b = msg;

	switch (af) {
	case AF_INET:
		if (len < sizeof(struct in_addr))
			return (-1);
		addr->sin.sin_family = AF_INET;
		addr->sin.sin_len = sizeof(struct sockaddr_in);
		memcpy(&addr->sin.sin_addr, b, sizeof(struct in_addr));
		return sizeof(struct in_addr);
	case AF_INET6:
		if (len < sizeof(struct in6_addr))
			return (-1);
		addr->sin6.sin6_family = AF_INET6;
		addr->sin6.sin6_len = sizeof(struct sockaddr_in6);
		memcpy(&addr->sin6.sin6_addr, b, sizeof(struct in6_addr));
		return sizeof(struct in6_addr);
	case AF_VPNv4:
		if (len < sizeof(u_int64_t) + sizeof(struct in_addr))
			return (-1);
		addr->svpn4.sv_len = sizeof(struct sockaddr_vpn4);
		addr->svpn4.sv_family = AF_VPNv4;
		memcpy(&addr->svpn4.sv_addr, b + sizeof(u_int64_t),
		    sizeof(struct in_addr));
		return (sizeof(u_int64_t) + sizeof(struct in_addr));
	default:
		return (-1);
	}
}
