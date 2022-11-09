/*	$OpenBSD: util.c,v 1.73 2022/11/09 14:23:53 claudio Exp $ */

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
#include <endian.h>
#include <errno.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <vis.h>

#include "bgpd.h"
#include "rde.h"
#include "log.h"

const char	*aspath_delim(uint8_t, int);

const char *
log_addr(const struct bgpd_addr *addr)
{
	static char	buf[74];
	struct sockaddr *sa;
	socklen_t	len;

	sa = addr2sa(addr, 0, &len);
	switch (addr->aid) {
	case AID_INET:
	case AID_INET6:
		return log_sockaddr(sa, len);
	case AID_VPN_IPv4:
	case AID_VPN_IPv6:
		snprintf(buf, sizeof(buf), "%s %s", log_rd(addr->rd),
		    log_sockaddr(sa, len));
		return (buf);
	}
	return ("???");
}

const char *
log_in6addr(const struct in6_addr *addr)
{
	struct sockaddr_in6	sa_in6;

	memset(&sa_in6, 0, sizeof(sa_in6));
	sa_in6.sin6_family = AF_INET6;
	memcpy(&sa_in6.sin6_addr, addr, sizeof(sa_in6.sin6_addr));

#ifdef __KAME__
	/* XXX thanks, KAME, for this ugliness... adopted from route/show.c */
	if ((IN6_IS_ADDR_LINKLOCAL(&sa_in6.sin6_addr) ||
	    IN6_IS_ADDR_MC_LINKLOCAL(&sa_in6.sin6_addr) ||
	    IN6_IS_ADDR_MC_NODELOCAL(&sa_in6.sin6_addr)) &&
	    sa_in6.sin6_scope_id == 0) {
		uint16_t tmp16;
		memcpy(&tmp16, &sa_in6.sin6_addr.s6_addr[2], sizeof(tmp16));
		sa_in6.sin6_scope_id = ntohs(tmp16);
		sa_in6.sin6_addr.s6_addr[2] = 0;
		sa_in6.sin6_addr.s6_addr[3] = 0;
	}
#endif

	return (log_sockaddr((struct sockaddr *)&sa_in6, sizeof(sa_in6)));
}

const char *
log_sockaddr(struct sockaddr *sa, socklen_t len)
{
	static char	buf[NI_MAXHOST];

	if (sa == NULL || getnameinfo(sa, len, buf, sizeof(buf), NULL, 0,
	    NI_NUMERICHOST))
		return ("(unknown)");
	else
		return (buf);
}

const char *
log_as(uint32_t as)
{
	static char	buf[11];	/* "4294967294\0" */

	if (snprintf(buf, sizeof(buf), "%u", as) < 0)
		return ("?");

	return (buf);
}

const char *
log_rd(uint64_t rd)
{
	static char	buf[32];
	struct in_addr	addr;
	uint32_t	u32;
	uint16_t	u16;

	rd = be64toh(rd);
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
		snprintf(buf, sizeof(buf), "rd #%016llx",
		    (unsigned long long)rd);
		break;
	}
	return (buf);
}

const struct ext_comm_pairs iana_ext_comms[] = IANA_EXT_COMMUNITIES;

/* NOTE: this function does not check if the type/subtype combo is
 * actually valid. */
const char *
log_ext_subtype(int type, uint8_t subtype)
{
	static char etype[6];
	const struct ext_comm_pairs *cp;

	for (cp = iana_ext_comms; cp->subname != NULL; cp++) {
		if ((type == cp->type || type == -1) && subtype == cp->subtype)
			return (cp->subname);
	}
	snprintf(etype, sizeof(etype), "[%u]", subtype);
	return (etype);
}

const char *
log_reason(const char *communication) {
	static char buf[(REASON_LEN - 1) * 4 + 1];

	strnvis(buf, communication, sizeof(buf), VIS_NL | VIS_OCTAL);

	return buf;
}

const char *
log_rtr_error(enum rtr_error err)
{
	static char buf[20];

	switch (err) {
	case NO_ERROR:
		return "No Error";
	case CORRUPT_DATA:
		return "Corrupt Data";
	case INTERNAL_ERROR:
		return "Internal Error";
	case NO_DATA_AVAILABLE:
		return "No Data Available";
	case INVALID_REQUEST:
		return "Invalid Request";
	case UNSUPP_PROTOCOL_VERS:
		return "Unsupported Protocol Version";
	case UNSUPP_PDU_TYPE:
		return "Unsupported PDU Type";
	case UNK_REC_WDRAWL:
		return "Withdrawal of Unknown Record";
	case DUP_REC_RECV:
		return "Duplicate Announcement Received";
	case UNEXP_PROTOCOL_VERS:
		return "Unexpected Protocol Version";
	default:
		snprintf(buf, sizeof(buf), "unknown %u", err);
		return buf;
	}
}

const char *
log_policy(uint8_t role)
{
	switch (role) {
	case CAPA_ROLE_PROVIDER:
		return "provider";
	case CAPA_ROLE_RS:
		return "rs";
	case CAPA_ROLE_RS_CLIENT:
		return "rs-client";
	case CAPA_ROLE_CUSTOMER:
		return "customer";
	case CAPA_ROLE_PEER:
		return "peer";
	default:
		return "unknown";
	}
}

const char *
aspath_delim(uint8_t seg_type, int closing)
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
aspath_snprint(char *buf, size_t size, void *data, uint16_t len)
{
#define UPDATE()				\
	do {					\
		if (r < 0)			\
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
	uint8_t		*seg;
	int		 r, total_size;
	uint16_t	 seg_size;
	uint8_t		 i, seg_type, seg_len;

	total_size = 0;
	seg = data;
	for (; len > 0; len -= seg_size, seg += seg_size) {
		seg_type = seg[0];
		seg_len = seg[1];
		seg_size = 2 + sizeof(uint32_t) * seg_len;

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
aspath_asprint(char **ret, void *data, uint16_t len)
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
aspath_strlen(void *data, uint16_t len)
{
	uint8_t		*seg;
	int		 total_size;
	uint32_t	 as;
	uint16_t	 seg_size;
	uint8_t		 i, seg_type, seg_len;

	total_size = 0;
	seg = data;
	for (; len > 0; len -= seg_size, seg += seg_size) {
		seg_type = seg[0];
		seg_len = seg[1];
		seg_size = 2 + sizeof(uint32_t) * seg_len;

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
 * Only works on verified 4-byte AS paths.
 */
uint32_t
aspath_extract(const void *seg, int pos)
{
	const u_char	*ptr = seg;
	uint32_t	 as;

	/* minimal pos check, return 0 since that is an invalid ASN */
	if (pos < 0 || pos >= ptr[1])
		return (0);
	ptr += 2 + sizeof(uint32_t) * pos;
	memcpy(&as, ptr, sizeof(uint32_t));
	return (ntohl(as));
}

/*
 * Verify that the aspath is correctly encoded.
 */
int
aspath_verify(void *data, uint16_t len, int as4byte, int noset)
{
	uint8_t		*seg = data;
	uint16_t	 seg_size, as_size = 2;
	uint8_t		 seg_len, seg_type;
	int		 error = 0;

	if (len & 1)
		/* odd length aspath are invalid */
		return (AS_ERR_BAD);

	if (as4byte)
		as_size = 4;

	for (; len > 0; len -= seg_size, seg += seg_size) {
		const uint8_t	*ptr;
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
		/*
		 * If AS_SET filtering (RFC6472) is on, error out on AS_SET
		 * as well.
		 */
		if (noset && seg_type == AS_SET)
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
			uint32_t as;

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
aspath_inflate(void *data, uint16_t len, uint16_t *newlen)
{
	uint8_t		*seg, *nseg, *ndata;
	uint16_t	 seg_size, olen, nlen;
	uint8_t		 seg_len;

	/* first calculate the length of the aspath */
	seg = data;
	nlen = 0;
	for (olen = len; olen > 0; olen -= seg_size, seg += seg_size) {
		seg_len = seg[1];
		seg_size = 2 + sizeof(uint16_t) * seg_len;
		nlen += 2 + sizeof(uint32_t) * seg_len;

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
extract_prefix(u_char *p, uint16_t len, void *va,
    uint8_t pfxlen, uint8_t max)
{
	static u_char	 addrmask[] = {
	    0x00, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff };
	u_char		*a = va;
	int		 i;
	uint16_t	 plen = 0;

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
nlri_get_prefix(u_char *p, uint16_t len, struct bgpd_addr *prefix,
    uint8_t *prefixlen)
{
	int	 plen;
	uint8_t	 pfxlen;

	if (len < 1)
		return (-1);

	pfxlen = *p++;
	len--;

	memset(prefix, 0, sizeof(struct bgpd_addr));
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
nlri_get_prefix6(u_char *p, uint16_t len, struct bgpd_addr *prefix,
    uint8_t *prefixlen)
{
	int	plen;
	uint8_t	pfxlen;

	if (len < 1)
		return (-1);

	pfxlen = *p++;
	len--;

	memset(prefix, 0, sizeof(struct bgpd_addr));
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
nlri_get_vpn4(u_char *p, uint16_t len, struct bgpd_addr *prefix,
    uint8_t *prefixlen, int withdraw)
{
	int		 rv, done = 0;
	uint16_t	 plen;
	uint8_t		 pfxlen;

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
		if (prefix->labellen + 3U >
		    sizeof(prefix->labelstack))
			return (-1);
		if (withdraw) {
			/* on withdraw ignore the labelstack all together */
			p += 3;
			plen += 3;
			pfxlen -= 3 * 8;
			break;
		}
		prefix->labelstack[prefix->labellen++] = *p++;
		prefix->labelstack[prefix->labellen++] = *p++;
		prefix->labelstack[prefix->labellen] = *p++;
		if (prefix->labelstack[prefix->labellen] &
		    BGP_MPLS_BOS)
			done = 1;
		prefix->labellen++;
		plen += 3;
		pfxlen -= 3 * 8;
	} while (!done);

	/* RD */
	if (len - plen < (int)sizeof(uint64_t) ||
	    pfxlen < sizeof(uint64_t) * 8)
		return (-1);
	memcpy(&prefix->rd, p, sizeof(uint64_t));
	pfxlen -= sizeof(uint64_t) * 8;
	p += sizeof(uint64_t);
	plen += sizeof(uint64_t);

	/* prefix */
	prefix->aid = AID_VPN_IPv4;
	*prefixlen = pfxlen;

	if (pfxlen > 32)
		return (-1);
	if ((rv = extract_prefix(p, len, &prefix->v4,
	    pfxlen, sizeof(prefix->v4))) == -1)
		return (-1);

	return (plen + rv);
}

int
nlri_get_vpn6(u_char *p, uint16_t len, struct bgpd_addr *prefix,
    uint8_t *prefixlen, int withdraw)
{
	int		rv, done = 0;
	uint16_t	plen;
	uint8_t		pfxlen;

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
		if (prefix->labellen + 3U >
		    sizeof(prefix->labelstack))
			return (-1);
		if (withdraw) {
			/* on withdraw ignore the labelstack all together */
			p += 3;
			plen += 3;
			pfxlen -= 3 * 8;
			break;
		}

		prefix->labelstack[prefix->labellen++] = *p++;
		prefix->labelstack[prefix->labellen++] = *p++;
		prefix->labelstack[prefix->labellen] = *p++;
		if (prefix->labelstack[prefix->labellen] &
		    BGP_MPLS_BOS)
			done = 1;
		prefix->labellen++;
		plen += 3;
		pfxlen -= 3 * 8;
	} while (!done);

	/* RD */
	if (len - plen < (int)sizeof(uint64_t) ||
	    pfxlen < sizeof(uint64_t) * 8)
		return (-1);

	memcpy(&prefix->rd, p, sizeof(uint64_t));
	pfxlen -= sizeof(uint64_t) * 8;
	p += sizeof(uint64_t);
	plen += sizeof(uint64_t);

	/* prefix */
	prefix->aid = AID_VPN_IPv6;
	*prefixlen = pfxlen;

	if (pfxlen > 128)
		return (-1);

	if ((rv = extract_prefix(p, len, &prefix->v6,
	    pfxlen, sizeof(prefix->v6))) == -1)
		return (-1);

	return (plen + rv);
}

static in_addr_t
prefixlen2mask(uint8_t prefixlen)
{
	if (prefixlen == 0)
		return (0);

	return (0xffffffff << (32 - prefixlen));
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
	uint8_t		m;

	if (a->aid != b->aid)
		return (a->aid - b->aid);

	switch (a->aid) {
	case AID_VPN_IPv4:
		if (be64toh(a->rd) > be64toh(b->rd))
			return (1);
		if (be64toh(a->rd) < be64toh(b->rd))
			return (-1);
		/* FALLTHROUGH */
	case AID_INET:
		if (prefixlen == 0)
			return (0);
		if (prefixlen > 32)
			return (-1);
		mask = htonl(prefixlen2mask(prefixlen));
		aa = ntohl(a->v4.s_addr & mask);
		ba = ntohl(b->v4.s_addr & mask);
		if (aa > ba)
			return (1);
		if (aa < ba)
			return (-1);
		break;
	case AID_VPN_IPv6:
		if (be64toh(a->rd) > be64toh(b->rd))
			return (1);
		if (be64toh(a->rd) < be64toh(b->rd))
			return (-1);
		/* FALLTHROUGH */
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
		break;
	default:
		return (-1);
	}

	if (a->aid == AID_VPN_IPv4 || a->aid == AID_VPN_IPv6) {
		if (a->labellen > b->labellen)
			return (1);
		if (a->labellen < b->labellen)
			return (-1);
		return (memcmp(a->labelstack, b->labelstack, a->labellen));
	}
	return (0);

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

	memset(&mask, 0, sizeof(mask));
	for (i = 0; i < prefixlen / 8; i++)
		mask.s6_addr[i] = 0xff;
	i = prefixlen % 8;
	if (i)
		mask.s6_addr[prefixlen / 8] = 0xff00 >> i;

	for (i = 0; i < 16; i++)
		dest->s6_addr[i] = src->s6_addr[i] & mask.s6_addr[i];
}

void
applymask(struct bgpd_addr *dest, const struct bgpd_addr *src, int prefixlen)
{
	*dest = *src;
	switch (src->aid) {
	case AID_INET:
	case AID_VPN_IPv4:
		inet4applymask(&dest->v4, &src->v4, prefixlen);
		break;
	case AID_INET6:
	case AID_VPN_IPv6:
		inet6applymask(&dest->v6, &src->v6, prefixlen);
		break;
	}
}

/* address family translation functions */
const struct aid aid_vals[AID_MAX] = AID_VALS;

const char *
aid2str(uint8_t aid)
{
	if (aid < AID_MAX)
		return (aid_vals[aid].name);
	return ("unknown AID");
}

int
aid2afi(uint8_t aid, uint16_t *afi, uint8_t *safi)
{
	if (aid < AID_MAX) {
		*afi = aid_vals[aid].afi;
		*safi = aid_vals[aid].safi;
		return (0);
	}
	return (-1);
}

int
afi2aid(uint16_t afi, uint8_t safi, uint8_t *aid)
{
	uint8_t i;

	for (i = 0; i < AID_MAX; i++)
		if (aid_vals[i].afi == afi && aid_vals[i].safi == safi) {
			*aid = i;
			return (0);
		}

	return (-1);
}

sa_family_t
aid2af(uint8_t aid)
{
	if (aid < AID_MAX)
		return (aid_vals[aid].af);
	return (AF_UNSPEC);
}

int
af2aid(sa_family_t af, uint8_t safi, uint8_t *aid)
{
	uint8_t i;

	if (safi == 0) /* default to unicast subclass */
		safi = SAFI_UNICAST;

	for (i = 0; i < AID_MAX; i++)
		if (aid_vals[i].af == af && aid_vals[i].safi == safi) {
			*aid = i;
			return (0);
		}

	return (-1);
}

/*
 * Convert a struct bgpd_addr into a struct sockaddr. For VPN addresses
 * the included label stack is ignored and needs to be handled by the caller.
 */
struct sockaddr *
addr2sa(const struct bgpd_addr *addr, uint16_t port, socklen_t *len)
{
	static struct sockaddr_storage	 ss;
	struct sockaddr_in		*sa_in = (struct sockaddr_in *)&ss;
	struct sockaddr_in6		*sa_in6 = (struct sockaddr_in6 *)&ss;

	if (addr == NULL || addr->aid == AID_UNSPEC)
		return (NULL);

	memset(&ss, 0, sizeof(ss));
	switch (addr->aid) {
	case AID_INET:
	case AID_VPN_IPv4:
		sa_in->sin_family = AF_INET;
		sa_in->sin_addr.s_addr = addr->v4.s_addr;
		sa_in->sin_port = htons(port);
		*len = sizeof(struct sockaddr_in);
		break;
	case AID_INET6:
	case AID_VPN_IPv6:
		sa_in6->sin6_family = AF_INET6;
		memcpy(&sa_in6->sin6_addr, &addr->v6,
		    sizeof(sa_in6->sin6_addr));
		sa_in6->sin6_port = htons(port);
		sa_in6->sin6_scope_id = addr->scope_id;
		*len = sizeof(struct sockaddr_in6);
		break;
	}

	return ((struct sockaddr *)&ss);
}

void
sa2addr(struct sockaddr *sa, struct bgpd_addr *addr, uint16_t *port)
{
	struct sockaddr_in		*sa_in = (struct sockaddr_in *)sa;
	struct sockaddr_in6		*sa_in6 = (struct sockaddr_in6 *)sa;

	memset(addr, 0, sizeof(*addr));
	switch (sa->sa_family) {
	case AF_INET:
		addr->aid = AID_INET;
		memcpy(&addr->v4, &sa_in->sin_addr, sizeof(addr->v4));
		if (port)
			*port = ntohs(sa_in->sin_port);
		break;
	case AF_INET6:
		addr->aid = AID_INET6;
#ifdef __KAME__
		/*
		 * XXX thanks, KAME, for this ugliness...
		 * adopted from route/show.c
		 */
		if ((IN6_IS_ADDR_LINKLOCAL(&sa_in6->sin6_addr) ||
		    IN6_IS_ADDR_MC_LINKLOCAL(&sa_in6->sin6_addr) ||
		    IN6_IS_ADDR_MC_NODELOCAL(&sa_in6->sin6_addr)) &&
		    sa_in6->sin6_scope_id == 0) {
			uint16_t tmp16;
			memcpy(&tmp16, &sa_in6->sin6_addr.s6_addr[2],
			    sizeof(tmp16));
			sa_in6->sin6_scope_id = ntohs(tmp16);
			sa_in6->sin6_addr.s6_addr[2] = 0;
			sa_in6->sin6_addr.s6_addr[3] = 0;
		}
#endif
		memcpy(&addr->v6, &sa_in6->sin6_addr, sizeof(addr->v6));
		addr->scope_id = sa_in6->sin6_scope_id; /* I hate v6 */
		if (port)
			*port = ntohs(sa_in6->sin6_port);
		break;
	}
}

const char *
get_baudrate(unsigned long long baudrate, char *unit)
{
	static char bbuf[16];
	const unsigned long long kilo = 1000;
	const unsigned long long mega = 1000ULL * kilo;
	const unsigned long long giga = 1000ULL * mega;

	if (baudrate > giga)
		snprintf(bbuf, sizeof(bbuf), "%llu G%s",
		    baudrate / giga, unit);
	else if (baudrate > mega)
		snprintf(bbuf, sizeof(bbuf), "%llu M%s",
		    baudrate / mega, unit);
	else if (baudrate > kilo)
		snprintf(bbuf, sizeof(bbuf), "%llu K%s",
		    baudrate / kilo, unit);
	else
		snprintf(bbuf, sizeof(bbuf), "%llu %s",
		    baudrate, unit);

	return (bbuf);
}
