/*	$OpenBSD: util.c,v 1.42 2018/12/30 13:53:07 denis Exp $ */

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
#include <net/if.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <vis.h>

#include "bgpd.h"
#include "rde.h"
#include "log.h"

const char	*aspath_delim(u_int8_t, int);

const char *
log_addr(const struct bgpd_addr *addr)
{
	static char	buf[74];
	char		tbuf[40];

	switch (addr->aid) {
	case AID_INET:
	case AID_INET6:
		if (inet_ntop(aid2af(addr->aid), &addr->ba, buf,
		    sizeof(buf)) == NULL)
			return ("?");
		return (buf);
	case AID_VPN_IPv4:
		if (inet_ntop(AF_INET, &addr->vpn4.addr, tbuf,
		    sizeof(tbuf)) == NULL)
			return ("?");
		snprintf(buf, sizeof(buf), "%s %s", log_rd(addr->vpn4.rd),
		   tbuf);
		return (buf);
	case AID_VPN_IPv6:
		if (inet_ntop(aid2af(addr->aid), &addr->vpn6.addr, tbuf,
		    sizeof(tbuf)) == NULL)
			return ("?");
		snprintf(buf, sizeof(buf), "%s %s", log_rd(addr->vpn6.rd),
		    tbuf);
		return (buf);
	}
	return ("???");
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
	static char	buf[11];	/* "4294967294\0" */

	if (snprintf(buf, sizeof(buf), "%u", as) == -1)
		return ("?");

	return (buf);
}

const char *
log_rd(u_int64_t rd)
{
	static char	buf[32];
	struct in_addr	addr;
	u_int32_t	u32;
	u_int16_t	u16;

	rd = betoh64(rd);
	switch (rd >> 48) {
	case EXT_COMMUNITY_TRANS_TWO_AS:
		u32 = rd & 0xffffffff;
		u16 = (rd >> 32) & 0xffff;
		snprintf(buf, sizeof(buf), "rd %hu:%u", u16, u32);
		break;
	case EXT_COMMUNITY_TRANS_FOUR_AS:
		u32 = (rd >> 16) & 0xffffffff;
		u16 = rd & 0xffff;
		snprintf(buf, sizeof(buf), "rd %s:%hu", log_as(u32), u16);
		break;
	case EXT_COMMUNITY_TRANS_IPV4:
		u32 = (rd >> 16) & 0xffffffff;
		u16 = rd & 0xffff;
		addr.s_addr = htonl(u32);
		snprintf(buf, sizeof(buf), "rd %s:%hu", inet_ntoa(addr), u16);
		break;
	default:
		return ("rd ?");
	}
	return (buf);
}

const struct ext_comm_pairs iana_ext_comms[] = IANA_EXT_COMMUNITIES;

/* NOTE: this function does not check if the type/subtype combo is
 * actually valid. */
const char *
log_ext_subtype(u_int8_t type, u_int8_t subtype)
{
	static char etype[6];
	const struct ext_comm_pairs *cp;

	for (cp = iana_ext_comms; cp->subname != NULL; cp++) {
		if (type == cp->type && subtype == cp->subtype)
			return (cp->subname);
	}
	snprintf(etype, sizeof(etype), "[%u]", subtype);
	return (etype);
}

const char *
log_shutcomm(const char *communication) {
	static char buf[(SHUT_COMM_LEN - 1) * 4 + 1];

	strnvis(buf, communication, sizeof(buf), VIS_NL | VIS_OCTAL);

	return buf;
}

const char *
aspath_delim(u_int8_t seg_type, int closing)
{
	static char db[8];

	switch (seg_type) {
	case AS_SET:
		if (!closing)
			return ("{ ");
		else
			return (" }");
	case AS_SEQUENCE:
		return ("");
	case AS_CONFED_SEQUENCE:
		if (!closing)
			return ("( ");
		else
			return (" )");
	case AS_CONFED_SET:
		if (!closing)
			return ("[ ");
		else
			return (" ]");
	default:
		if (!closing)
			snprintf(db, sizeof(db), "!%u ", seg_type);
		else
			snprintf(db, sizeof(db), " !%u", seg_type);
		return (db);
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

		r = snprintf(buf, size, "%s%s",
		    total_size != 0 ? " " : "",
		    aspath_delim(seg_type, 0));
		UPDATE();

		for (i = 0; i < seg_len; i++) {
			r = snprintf(buf, size, "%s",
			    log_as(aspath_extract(seg, i)));
			UPDATE();
			if (i + 1 < seg_len) {
				r = snprintf(buf, size, " ");
				UPDATE();
			}
		}
		r = snprintf(buf, size, "%s", aspath_delim(seg_type, 1));
		UPDATE();
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

			do {
				total_size++;
			} while ((as = as / 10) != 0);

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

/*
 * Verify that the aspath is correctly encoded.
 */
int
aspath_verify(void *data, u_int16_t len, int as4byte)
{
	u_int8_t	*seg = data;
	u_int16_t	 seg_size, as_size = 2;
	u_int8_t	 seg_len, seg_type;
	int		 error = 0;

	if (len & 1)
		/* odd length aspath are invalid */
		return (AS_ERR_BAD);

	if (as4byte)
		as_size = 4;

	for (; len > 0; len -= seg_size, seg += seg_size) {
		const u_int8_t	*ptr;
		int		 pos;

		if (len < 2)	/* header length check */
			return (AS_ERR_BAD);
		seg_type = seg[0];
		seg_len = seg[1];

		if (seg_len == 0)
			/* empty aspath segments are not allowed */
			return (AS_ERR_BAD);

		/*
		 * BGP confederations should not show up but consider them
		 * as a soft error which invalidates the path but keeps the
		 * bgp session running.
		 */
		if (seg_type == AS_CONFED_SEQUENCE || seg_type == AS_CONFED_SET)
			error = AS_ERR_SOFT;
		if (seg_type != AS_SET && seg_type != AS_SEQUENCE &&
		    seg_type != AS_CONFED_SEQUENCE && seg_type != AS_CONFED_SET)
			return (AS_ERR_TYPE);

		seg_size = 2 + as_size * seg_len;

		if (seg_size > len)
			return (AS_ERR_LEN);

		/* RFC 7607 - AS 0 is considered malformed */
		ptr = seg + 2;
		for (pos = 0; pos < seg_len; pos++) {
			u_int32_t as;

			memcpy(&as, ptr, as_size);
			if (as == 0)
				error = AS_ERR_SOFT;
			ptr += as_size;
		}
	}
	return (error);	/* aspath is valid but probably not loop free */
}

/*
 * convert a 2 byte aspath to a 4 byte one.
 */
u_char *
aspath_inflate(void *data, u_int16_t len, u_int16_t *newlen)
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

		if (seg_size > olen) {
			errno = ERANGE;
			return (NULL);
		}
	}

	*newlen = nlen;
	if ((ndata = malloc(nlen)) == NULL)
		return (NULL);

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

/* NLRI functions to extract prefixes from the NLRI blobs */
static int
extract_prefix(u_char *p, u_int16_t len, void *va,
    u_int8_t pfxlen, u_int8_t max)
{
	static u_char addrmask[] = {
	    0x00, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff };
	u_char		*a = va;
	int		 i;
	u_int16_t	 plen = 0;

	for (i = 0; pfxlen && i < max; i++) {
		if (len <= plen)
			return (-1);
		if (pfxlen < 8) {
			a[i] = *p++ & addrmask[pfxlen];
			plen++;
			break;
		} else {
			a[i] = *p++;
			plen++;
			pfxlen -= 8;
		}
	}
	return (plen);
}

int
nlri_get_prefix(u_char *p, u_int16_t len, struct bgpd_addr *prefix,
    u_int8_t *prefixlen)
{
	u_int8_t	 pfxlen;
	int		 plen;

	if (len < 1)
		return (-1);

	pfxlen = *p++;
	len--;

	bzero(prefix, sizeof(struct bgpd_addr));
	prefix->aid = AID_INET;
	*prefixlen = pfxlen;

	if (pfxlen > 32)
		return (-1);
	if ((plen = extract_prefix(p, len, &prefix->v4, pfxlen,
	    sizeof(prefix->v4))) == -1)
		return (-1);

	return (plen + 1);	/* pfxlen needs to be added */
}

int
nlri_get_prefix6(u_char *p, u_int16_t len, struct bgpd_addr *prefix,
    u_int8_t *prefixlen)
{
	int		plen;
	u_int8_t	pfxlen;

	if (len < 1)
		return (-1);

	pfxlen = *p++;
	len--;

	bzero(prefix, sizeof(struct bgpd_addr));
	prefix->aid = AID_INET6;
	*prefixlen = pfxlen;

	if (pfxlen > 128)
		return (-1);
	if ((plen = extract_prefix(p, len, &prefix->v6, pfxlen,
	    sizeof(prefix->v6))) == -1)
		return (-1);

	return (plen + 1);	/* pfxlen needs to be added */
}

int
nlri_get_vpn4(u_char *p, u_int16_t len, struct bgpd_addr *prefix,
    u_int8_t *prefixlen, int withdraw)
{
	int		 rv, done = 0;
	u_int8_t	 pfxlen;
	u_int16_t	 plen;

	if (len < 1)
		return (-1);

	memcpy(&pfxlen, p, 1);
	p += 1;
	plen = 1;

	bzero(prefix, sizeof(struct bgpd_addr));

	/* label stack */
	do {
		if (len - plen < 3 || pfxlen < 3 * 8)
			return (-1);
		if (prefix->vpn4.labellen + 3U >
		    sizeof(prefix->vpn4.labelstack))
			return (-1);
		if (withdraw) {
			/* on withdraw ignore the labelstack all together */
			plen += 3;
			pfxlen -= 3 * 8;
			break;
		}
		prefix->vpn4.labelstack[prefix->vpn4.labellen++] = *p++;
		prefix->vpn4.labelstack[prefix->vpn4.labellen++] = *p++;
		prefix->vpn4.labelstack[prefix->vpn4.labellen] = *p++;
		if (prefix->vpn4.labelstack[prefix->vpn4.labellen] &
		    BGP_MPLS_BOS)
			done = 1;
		prefix->vpn4.labellen++;
		plen += 3;
		pfxlen -= 3 * 8;
	} while (!done);

	/* RD */
	if (len - plen < (int)sizeof(u_int64_t) ||
	    pfxlen < sizeof(u_int64_t) * 8)
		return (-1);
	memcpy(&prefix->vpn4.rd, p, sizeof(u_int64_t));
	pfxlen -= sizeof(u_int64_t) * 8;
	p += sizeof(u_int64_t);
	plen += sizeof(u_int64_t);

	/* prefix */
	prefix->aid = AID_VPN_IPv4;
	*prefixlen = pfxlen;

	if (pfxlen > 32)
		return (-1);
	if ((rv = extract_prefix(p, len, &prefix->vpn4.addr,
	    pfxlen, sizeof(prefix->vpn4.addr))) == -1)
		return (-1);

	return (plen + rv);
}

int
nlri_get_vpn6(u_char *p, u_int16_t len, struct bgpd_addr *prefix,
    u_int8_t *prefixlen, int withdraw)
{
	int		rv, done = 0;
	u_int8_t	pfxlen;
	u_int16_t	plen;

	if (len < 1)
		return (-1);

	memcpy(&pfxlen, p, 1);
	p += 1;
	plen = 1;

	memset(prefix, 0, sizeof(struct bgpd_addr));

	/* label stack */
	do {
		if (len - plen < 3 || pfxlen < 3 * 8)
			return (-1);
		if (prefix->vpn6.labellen + 3U >
		    sizeof(prefix->vpn6.labelstack))
			return (-1);
		if (withdraw) {
			/* on withdraw ignore the labelstack all together */
			plen += 3;
			pfxlen -= 3 * 8;
			break;
		}

		prefix->vpn6.labelstack[prefix->vpn6.labellen++] = *p++;
		prefix->vpn6.labelstack[prefix->vpn6.labellen++] = *p++;
		prefix->vpn6.labelstack[prefix->vpn6.labellen] = *p++;
		if (prefix->vpn6.labelstack[prefix->vpn6.labellen] &
		    BGP_MPLS_BOS)
			done = 1;
		prefix->vpn6.labellen++;
		plen += 3;
		pfxlen -= 3 * 8;
	} while (!done);

	/* RD */
	if (len - plen < (int)sizeof(u_int64_t) ||
	    pfxlen < sizeof(u_int64_t) * 8)
		return (-1);

	memcpy(&prefix->vpn6.rd, p, sizeof(u_int64_t));
	pfxlen -= sizeof(u_int64_t) * 8;
	p += sizeof(u_int64_t);
	plen += sizeof(u_int64_t);

	/* prefix */
	prefix->aid = AID_VPN_IPv6;
	*prefixlen = pfxlen;

	if (pfxlen > 128)
		return (-1);

	if ((rv = extract_prefix(p, len, &prefix->vpn6.addr,
	    pfxlen, sizeof(prefix->vpn6.addr))) == -1)
		return (-1);

	return (plen + rv);
}



/*
 * This function will have undefined behaviour if the passed in prefixlen is
 * too large for the respective bgpd_addr address family.
 */
int
prefix_compare(const struct bgpd_addr *a, const struct bgpd_addr *b,
    int prefixlen)
{
	in_addr_t	mask, aa, ba;
	int		i;
	u_int8_t	m;

	if (a->aid != b->aid)
		return (a->aid - b->aid);

	switch (a->aid) {
	case AID_INET:
		if (prefixlen == 0)
			return (0);
		if (prefixlen > 32)
			return (-1);
		mask = htonl(prefixlen2mask(prefixlen));
		aa = ntohl(a->v4.s_addr & mask);
		ba = ntohl(b->v4.s_addr & mask);
		if (aa != ba)
			return (aa - ba);
		return (0);
	case AID_INET6:
		if (prefixlen == 0)
			return (0);
		if (prefixlen > 128)
			return (-1);
		for (i = 0; i < prefixlen / 8; i++)
			if (a->v6.s6_addr[i] != b->v6.s6_addr[i])
				return (a->v6.s6_addr[i] - b->v6.s6_addr[i]);
		i = prefixlen % 8;
		if (i) {
			m = 0xff00 >> i;
			if ((a->v6.s6_addr[prefixlen / 8] & m) !=
			    (b->v6.s6_addr[prefixlen / 8] & m))
				return ((a->v6.s6_addr[prefixlen / 8] & m) -
				    (b->v6.s6_addr[prefixlen / 8] & m));
		}
		return (0);
	case AID_VPN_IPv4:
		if (prefixlen > 32)
			return (-1);
		if (betoh64(a->vpn4.rd) > betoh64(b->vpn4.rd))
			return (1);
		if (betoh64(a->vpn4.rd) < betoh64(b->vpn4.rd))
			return (-1);
		mask = htonl(prefixlen2mask(prefixlen));
		aa = ntohl(a->vpn4.addr.s_addr & mask);
		ba = ntohl(b->vpn4.addr.s_addr & mask);
		if (aa != ba)
			return (aa - ba);
		if (a->vpn4.labellen > b->vpn4.labellen)
			return (1);
		if (a->vpn4.labellen < b->vpn4.labellen)
			return (-1);
		return (memcmp(a->vpn4.labelstack, b->vpn4.labelstack,
		    a->vpn4.labellen));
	case AID_VPN_IPv6:
		if (prefixlen > 128)
			return (-1);
		if (betoh64(a->vpn6.rd) > betoh64(b->vpn6.rd))
			return (1);
		if (betoh64(a->vpn6.rd) < betoh64(b->vpn6.rd))
			return (-1);
		for (i = 0; i < prefixlen / 8; i++)
			if (a->vpn6.addr.s6_addr[i] != b->vpn6.addr.s6_addr[i])
				return (a->vpn6.addr.s6_addr[i] -
				    b->vpn6.addr.s6_addr[i]);
		i = prefixlen % 8;
		if (i) {
			m = 0xff00 >> i;
			if ((a->vpn6.addr.s6_addr[prefixlen / 8] & m) !=
			    (b->vpn6.addr.s6_addr[prefixlen / 8] & m))
				return ((a->vpn6.addr.s6_addr[prefixlen / 8] &
				    m) - (b->vpn6.addr.s6_addr[prefixlen / 8] &
				    m));
		}
		if (a->vpn6.labellen > b->vpn6.labellen)
			return (1);
		if (a->vpn6.labellen < b->vpn6.labellen)
			return (-1);
		return (memcmp(a->vpn6.labelstack, b->vpn6.labelstack,
		    a->vpn6.labellen));
	}
	return (-1);
}

in_addr_t
prefixlen2mask(u_int8_t prefixlen)
{
	if (prefixlen == 0)
		return (0);

	return (0xffffffff << (32 - prefixlen));
}

void
inet4applymask(struct in_addr *dest, const struct in_addr *src, int prefixlen)
{
	struct in_addr mask;

	mask.s_addr = htonl(prefixlen2mask(prefixlen));
	dest->s_addr = src->s_addr & mask.s_addr;
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

const struct if_status_description
		if_status_descriptions[] = LINK_STATE_DESCRIPTIONS;
const struct ifmedia_description
		ifm_type_descriptions[] = IFM_TYPE_DESCRIPTIONS;

uint64_t
ift2ifm(uint8_t if_type)
{
	switch (if_type) {
	case IFT_ETHER:
		return (IFM_ETHER);
	case IFT_FDDI:
		return (IFM_FDDI);
	case IFT_CARP:
		return (IFM_CARP);
	case IFT_IEEE80211:
		return (IFM_IEEE80211);
	default:
		return (0);
	}
}

const char *
get_media_descr(uint64_t media_type)
{
	const struct ifmedia_description	*p;

	for (p = ifm_type_descriptions; p->ifmt_string != NULL; p++)
		if (media_type == p->ifmt_word)
			return (p->ifmt_string);

	return ("unknown media");
}

const char *
get_linkstate(uint8_t if_type, int link_state)
{
	const struct if_status_description *p;
	static char buf[8];

	for (p = if_status_descriptions; p->ifs_string != NULL; p++) {
		if (LINK_STATE_DESC_MATCH(p, if_type, link_state))
			return (p->ifs_string);
	}
	snprintf(buf, sizeof(buf), "[#%d]", link_state);
	return (buf);
}

const char *
get_baudrate(u_int64_t baudrate, char *unit)
{
	static char bbuf[16];

	if (baudrate > IF_Gbps(1))
		snprintf(bbuf, sizeof(bbuf), "%llu G%s",
		    baudrate / IF_Gbps(1), unit);
	else if (baudrate > IF_Mbps(1))
		snprintf(bbuf, sizeof(bbuf), "%llu M%s",
		    baudrate / IF_Mbps(1), unit);
	else if (baudrate > IF_Kbps(1))
		snprintf(bbuf, sizeof(bbuf), "%llu K%s",
		    baudrate / IF_Kbps(1), unit);
	else
		snprintf(bbuf, sizeof(bbuf), "%llu %s",
		    baudrate, unit);

	return (bbuf);
}
