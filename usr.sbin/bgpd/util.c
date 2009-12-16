/*	$OpenBSD: util.c,v 1.9 2009/12/16 15:40:55 claudio Exp $ */

/*
 * Copyright (c) 2006 Claudio Jeker <claudio@openbsd.org>
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bgpd.h"
#include "rde.h"

const char *
log_addr(const struct bgpd_addr *addr)
{
	static char	buf[48];

	if (inet_ntop(aid2af(addr->aid), &addr->ba, buf, sizeof(buf)) == NULL)
		return ("?");
	else
		return (buf);
}

const char *
log_in6addr(const struct in6_addr *addr)
{
	struct sockaddr_in6	sa_in6;
	u_int16_t		tmp16;

	bzero(&sa_in6, sizeof(sa_in6));
	sa_in6.sin6_len = sizeof(sa_in6);
	sa_in6.sin6_family = AF_INET6;
	memcpy(&sa_in6.sin6_addr, addr, sizeof(sa_in6.sin6_addr));

	/* XXX thanks, KAME, for this ugliness... adopted from route/show.c */
	if (IN6_IS_ADDR_LINKLOCAL(&sa_in6.sin6_addr) ||
	    IN6_IS_ADDR_MC_LINKLOCAL(&sa_in6.sin6_addr)) {
		memcpy(&tmp16, &sa_in6.sin6_addr.s6_addr[2], sizeof(tmp16));
		sa_in6.sin6_scope_id = ntohs(tmp16);
		sa_in6.sin6_addr.s6_addr[2] = 0;
		sa_in6.sin6_addr.s6_addr[3] = 0;
	}

	return (log_sockaddr((struct sockaddr *)&sa_in6));
}

const char *
log_sockaddr(struct sockaddr *sa)
{
	static char	buf[NI_MAXHOST];

	if (getnameinfo(sa, sa->sa_len, buf, sizeof(buf), NULL, 0,
	    NI_NUMERICHOST))
		return ("(unknown)");
	else
		return (buf);
}

const char *
log_as(u_int32_t as)
{
	static char	buf[12];	/* "65000.65000\0" */

	if (as <= USHRT_MAX) {
		if (snprintf(buf, sizeof(buf), "%u", as) == -1)
			return ("?");
	} else {
		if (snprintf(buf, sizeof(buf), "%u.%u", as >> 16,
		    as & 0xffff) == -1)
			return ("?");
	}
	return (buf);
}

/* XXX this function does not check if the type/subtype combo is
 * actually valid. */
const char *
log_ext_subtype(u_int8_t subtype)
{
	static char etype[6];

	switch (subtype) {
	case EXT_COMMUNITY_ROUTE_TGT:
		return ("rt");	/* route target */
	case EXT_CUMMUNITY_ROUTE_ORIG:
		return ("soo");	/* source of origin */
	case EXT_COMMUNITY_OSPF_DOM_ID:
		return ("odi");	/* ospf domain id */
	case EXT_COMMUNITY_OSPF_RTR_TYPE:
		return ("ort");	/* ospf route type */
	case EXT_COMMUNITY_OSPF_RTR_ID:
		return ("ori");	/* ospf router id */
	case EXT_COMMUNITY_BGP_COLLECT:
		return ("bdc");	/* bgp data collection */
	default:
		snprintf(etype, sizeof(etype), "[%i]", (int)subtype);
		return (etype);
	}
}

int
aspath_snprint(char *buf, size_t size, void *data, u_int16_t len)
{
#define UPDATE()				\
	do {					\
		if (r == -1)			\
			return (-1);		\
		total_size += r;		\
		if ((unsigned int)r < size) {	\
			size -= r;		\
			buf += r;		\
		} else {			\
			buf += size;		\
			size = 0;		\
		}				\
	} while (0)
	u_int8_t	*seg;
	int		 r, total_size;
	u_int16_t	 seg_size;
	u_int8_t	 i, seg_type, seg_len;

	total_size = 0;
	seg = data;
	for (; len > 0; len -= seg_size, seg += seg_size) {
		seg_type = seg[0];
		seg_len = seg[1];
		seg_size = 2 + sizeof(u_int32_t) * seg_len;

		if (seg_type == AS_SET) {
			if (total_size != 0)
				r = snprintf(buf, size, " { ");
			else
				r = snprintf(buf, size, "{ ");
			UPDATE();
		} else if (total_size != 0) {
			r = snprintf(buf, size, " ");
			UPDATE();
		}

		for (i = 0; i < seg_len; i++) {
			r = snprintf(buf, size, "%s",
			    log_as(aspath_extract(seg, i)));
			UPDATE();
			if (i + 1 < seg_len) {
				r = snprintf(buf, size, " ");
				UPDATE();
			}
		}
		if (seg_type == AS_SET) {
			r = snprintf(buf, size, " }");
			UPDATE();
		}
	}
	/* ensure that we have a valid C-string especially for empty as path */
	if (size > 0)
		*buf = '\0';

	return (total_size);
#undef UPDATE
}

int
aspath_asprint(char **ret, void *data, u_int16_t len)
{
	size_t	slen;
	int	plen;

	slen = aspath_strlen(data, len) + 1;
	*ret = malloc(slen);
	if (*ret == NULL)
		return (-1);

	plen = aspath_snprint(*ret, slen, data, len);
	if (plen == -1) {
		free(*ret);
		*ret = NULL;
		return (-1);
	}

	return (0);
}

size_t
aspath_strlen(void *data, u_int16_t len)
{
	u_int8_t	*seg;
	int		 total_size;
	u_int32_t	 as;
	u_int16_t	 seg_size;
	u_int8_t	 i, seg_type, seg_len;

	total_size = 0;
	seg = data;
	for (; len > 0; len -= seg_size, seg += seg_size) {
		seg_type = seg[0];
		seg_len = seg[1];
		seg_size = 2 + sizeof(u_int32_t) * seg_len;

		if (seg_type == AS_SET)
			if (total_size != 0)
				total_size += 3;
			else
				total_size += 2;
		else if (total_size != 0)
			total_size += 1;

		for (i = 0; i < seg_len; i++) {
			as = aspath_extract(seg, i);
			if (as > USHRT_MAX) {
				u_int32_t	a = as >> 16;

				if (a >= 10000)
					total_size += 5;
				else if (a >= 1000)
					total_size += 4;
				else if (a >= 100)
					total_size += 3;
				else if (a >= 10)
					total_size += 2;
				else
					total_size += 1;
				total_size += 1; /* dot between hi & lo */
				as &= 0xffff;
			}
			if (as >= 10000)
				total_size += 5;
			else if (as >= 1000)
				total_size += 4;
			else if (as >= 100)
				total_size += 3;
			else if (as >= 10)
				total_size += 2;
			else
				total_size += 1;

			if (i + 1 < seg_len)
				total_size += 1;
		}

		if (seg_type == AS_SET)
			total_size += 2;
	}
	return (total_size);
}

/*
 * Extract the asnum out of the as segment at the specified position.
 * Direct access is not possible because of non-aligned reads.
 * ATTENTION: no bounds checks are done.
 */
u_int32_t
aspath_extract(const void *seg, int pos)
{
	const u_char	*ptr = seg;
	u_int32_t	 as;

	ptr += 2 + sizeof(u_int32_t) * pos;
	memcpy(&as, ptr, sizeof(u_int32_t));
	return (ntohl(as));
}

in_addr_t
prefixlen2mask(u_int8_t prefixlen)
{
	if (prefixlen == 0)
		return (0);

	return (0xffffffff << (32 - prefixlen));
}

void
inet6applymask(struct in6_addr *dest, const struct in6_addr *src, int prefixlen)
{
	struct in6_addr	mask;
	int		i;

	bzero(&mask, sizeof(mask));
	for (i = 0; i < prefixlen / 8; i++)
		mask.s6_addr[i] = 0xff;
	i = prefixlen % 8;
	if (i)
		mask.s6_addr[prefixlen / 8] = 0xff00 >> i;

	for (i = 0; i < 16; i++)
		dest->s6_addr[i] = src->s6_addr[i] & mask.s6_addr[i];
}

/* address family translation functions */
const struct aid aid_vals[AID_MAX] = AID_VALS;

const char *
aid2str(u_int8_t aid)
{
	if (aid < AID_MAX)
		return (aid_vals[aid].name);
	return ("unknown AID");
}

int
aid2afi(u_int8_t aid, u_int16_t *afi, u_int8_t *safi)
{
	if (aid < AID_MAX) {
		*afi = aid_vals[aid].afi;
		*safi = aid_vals[aid].safi;
		return (0);
	}
	return (-1);
}

int
afi2aid(u_int16_t afi, u_int8_t safi, u_int8_t *aid)
{
	u_int8_t i;

	for (i = 0; i < AID_MAX; i++)
		if (aid_vals[i].afi == afi && aid_vals[i].safi == safi) {
			*aid = i;
			return (0);
		}

	return (-1);
}

sa_family_t
aid2af(u_int8_t aid)
{
	if (aid < AID_MAX)
		return (aid_vals[aid].af);
	return (AF_UNSPEC);
}

int
af2aid(sa_family_t af, u_int8_t safi, u_int8_t *aid)
{
	u_int8_t i;

	if (safi == 0) /* default to unicast subclass */
		safi = SAFI_UNICAST;

	for (i = 0; i < AID_MAX; i++)
		if (aid_vals[i].af == af && aid_vals[i].safi == safi) {
			*aid = i;
			return (0);
		}

	return (-1);
}

struct sockaddr *
addr2sa(struct bgpd_addr *addr, u_int16_t port)
{
	static struct sockaddr_storage	 ss;
	struct sockaddr_in		*sa_in = (struct sockaddr_in *)&ss;
	struct sockaddr_in6		*sa_in6 = (struct sockaddr_in6 *)&ss;

	if (addr->aid == AID_UNSPEC)
		return (NULL);

	bzero(&ss, sizeof(ss));
	switch (addr->aid) {
	case AID_INET:
		sa_in->sin_family = AF_INET;
		sa_in->sin_len = sizeof(struct sockaddr_in);
		sa_in->sin_addr.s_addr = addr->v4.s_addr;
		sa_in->sin_port = htons(port);
		break;
	case AID_INET6:
		sa_in6->sin6_family = AF_INET6;
		sa_in6->sin6_len = sizeof(struct sockaddr_in6);
		memcpy(&sa_in6->sin6_addr, &addr->v6,
		    sizeof(sa_in6->sin6_addr));
		sa_in6->sin6_port = htons(port);
		sa_in6->sin6_scope_id = addr->scope_id;
		break;
	}

	return ((struct sockaddr *)&ss);
}

void
sa2addr(struct sockaddr *sa, struct bgpd_addr *addr)
{
	struct sockaddr_in		*sa_in = (struct sockaddr_in *)sa;
	struct sockaddr_in6		*sa_in6 = (struct sockaddr_in6 *)sa;

	bzero(addr, sizeof(*addr));
	switch (sa->sa_family) {
	case AF_INET:
		addr->aid = AID_INET;
		memcpy(&addr->v4, &sa_in->sin_addr, sizeof(addr->v4));
		break;
	case AF_INET6:
		addr->aid = AID_INET6;
		memcpy(&addr->v6, &sa_in6->sin6_addr, sizeof(addr->v6));
		addr->scope_id = sa_in6->sin6_scope_id; /* I hate v6 */
		break;
	}
}
