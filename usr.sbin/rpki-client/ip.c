/*	$OpenBSD: ip.c,v 1.12 2020/04/16 14:39:44 claudio Exp $ */
/*
 * Copyright (c) 2019 Kristaps Dzonsons <kristaps@bsd.lv>
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

#include <sys/socket.h>
#include <arpa/inet.h>

#include <assert.h>
#include <err.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openssl/ssl.h>

#include "extern.h"

#define   PREFIX_SIZE(x)  (((x) + 7) / 8)

/*
 * Parse an IP address family.
 * This is defined in different places in the ROA/X509 standards, but
 * it's the same thing.
 * We prohibit all but IPv4 and IPv6, without SAFI.
 * Return zero on failure, non-zero on success.
 */
int
ip_addr_afi_parse(const char *fn, const ASN1_OCTET_STRING *p, enum afi *afi)
{
	uint16_t v;

	if (p->length == 0 || p->length > 3) {
		warnx("%s: invalid field length, want 1--3, have %d",
		    fn, p->length);
		return 0;
	}

	memcpy(&v, p->data, sizeof(v));
	v = ntohs(v);

	/* Only accept IPv4 and IPv6 AFIs. */

	if (v != AFI_IPV4 && v != AFI_IPV6) {
		warnx("%s: only AFI for IPV4 (1) and IPV6 (2) allowed: "
		    "have %hd", fn, v);
		return 0;
	}

	/* Disallow the optional SAFI. */

	if (p->length == 3) {
		warnx("%s: SAFI not allowed", fn);
		return 0;
	}

	*afi = v;
	return 1;
}

/*
 * See if a given IP prefix is covered by the IP prefixes or ranges
 * specified in the "ips" array.
 * This means that the IP prefix must be strictly within the ranges or
 * singletons given in the array.
 * Return 0 if we're inheriting from the parent, >0 if we're covered,
 * or <0 if we're not covered.
 */
int
ip_addr_check_covered(enum afi afi,
    const unsigned char *min, const unsigned char *max,
    const struct cert_ip *ips, size_t ipsz)
{
	size_t	 i, sz = AFI_IPV4 == afi ? 4 : 16;

	for (i = 0; i < ipsz; i++) {
		if (ips[i].afi != afi)
			continue;
		if (ips[i].type == CERT_IP_INHERIT)
			return 0;
		if (memcmp(ips[i].min, min, sz) <= 0 &&
		    memcmp(ips[i].max, max, sz) >= 0)
			return 1;
	}

	return -1;
}

/*
 * Given a newly-parsed IP address or range "ip", make sure that "ip"
 * does not overlap with any addresses or ranges in the "ips" array.
 * This is defined by RFC 3779 section 2.2.3.6.
 * Returns zero on failure, non-zero on success.
 */
int
ip_addr_check_overlap(const struct cert_ip *ip, const char *fn,
    const struct cert_ip *ips, size_t ipsz)
{
	size_t	 i, sz = ip->afi == AFI_IPV4 ? 4 : 16;
	int	 inherit_v4 = 0, inherit_v6 = 0;
	int	 has_v4 = 0, has_v6 = 0, socktype;
	char	 buf[64];

	/*
	 * FIXME: cache this by having a flag on the cert_ip, else we're
	 * going to need to do a lot of scanning for big allocations.
	 */

	for (i = 0; i < ipsz; i++)
		if (ips[i].type == CERT_IP_INHERIT) {
			if (ips[i].afi == AFI_IPV4)
				inherit_v4 = 1;
			else
				inherit_v6 = 1;
		} else {
			if (ips[i].afi == AFI_IPV4)
				has_v4 = 1;
			else
				has_v6 = 1;
		}

	/* Disallow multiple inheritence per type. */

	if ((inherit_v4 && ip->afi == AFI_IPV4) ||
	    (inherit_v6 && ip->afi == AFI_IPV6) ||
	    (has_v4 && ip->afi == AFI_IPV4 &&
	     ip->type == CERT_IP_INHERIT) ||
	    (has_v6 && ip->afi == AFI_IPV6 &&
	     ip->type == CERT_IP_INHERIT)) {
		warnx("%s: RFC 3779 section 2.2.3.5: "
		    "cannot have multiple inheritence or inheritence and "
		    "addresses of the same class", fn);
		return 0;
	}

	/* Check our ranges. */

	for (i = 0; i < ipsz; i++) {
		if (ips[i].afi != ip->afi)
			continue;
		if (memcmp(ips[i].max, ip->min, sz) <= 0 ||
		    memcmp(ips[i].min, ip->max, sz) >= 0)
			continue;
		socktype = (ips[i].afi == AFI_IPV4) ? AF_INET : AF_INET6,
		warnx("%s: RFC 3779 section 2.2.3.5: "
		    "cannot have overlapping IP addresses", fn);
		ip_addr_print(&ip->ip, ip->afi, buf, sizeof(buf));
		warnx("%s: certificate IP: %s", fn, buf);
		inet_ntop(socktype, ip->min, buf, sizeof(buf));
		warnx("%s: certificate IP minimum: %s", fn, buf);
		inet_ntop(socktype, ip->max, buf, sizeof(buf));
		warnx("%s: certificate IP maximum: %s", fn, buf);
		inet_ntop(socktype, ips[i].min, buf, sizeof(buf));
		warnx("%s: offending IP minimum: %s", fn, buf);
		inet_ntop(socktype, ips[i].max, buf, sizeof(buf));
		warnx("%s: offending IP maximum: %s", fn, buf);
		return 0;
	}

	return 1;
}

/*
 * Parse an IP address, RFC 3779, 2.2.3.8.
 * Return zero on failure, non-zero on success.
 */
int
ip_addr_parse(const ASN1_BIT_STRING *p,
    enum afi afi, const char *fn, struct ip_addr *addr)
{
	long	 unused = 0;

	/* Weird OpenSSL-ism to get unused bit count. */

	if ((p->flags & ASN1_STRING_FLAG_BITS_LEFT))
		unused = p->flags & ~ASN1_STRING_FLAG_BITS_LEFT;

	if (unused < 0) {
		warnx("%s: RFC 3779 section 2.2.3.8: "
		    "unused bit count must be non-negative", fn);
		return 0;
	} else if (unused >= 8) {
		warnx("%s: RFC 3779 section 2.2.3.8: "
		    "unused bit count must mask an unsigned char", fn);
		return 0;
	} else if (p->length == 0 && unused != 0) {
		warnx("%s: RFC 3779 section 2.2.3.8: "
		    "unused bit count must be zero if length is zero", fn);
		return 0;
	}

	/*
	 * Check that the unused bits are set to zero.
	 * If we don't do this, stray bits will corrupt our composition
	 * of the [minimum] address ranges.
	 */

	if (p->length != 0 &&
	    (p->data[p->length - 1] & ((1 << unused) - 1))) {
		warnx("%s: RFC 3779 section 2.2.3.8: "
		    "unused bits must be set to zero", fn);
		return 0;
	}

	/* Limit possible sizes of addresses. */

	if ((afi == AFI_IPV4 && p->length > 4) ||
	    (afi == AFI_IPV6 && p->length > 16)) {
		warnx("%s: RFC 3779 section 2.2.3.8: "
		    "IP address too long", fn);
		return 0;
	}

	memset (addr, 0, sizeof(struct ip_addr));
	addr->prefixlen = p->length * 8 - unused;
	memcpy(addr->addr, p->data, p->length);
	return 1;
}

/*
 * Convert the IPv4 address into CIDR notation conforming to RFC 4632.
 * Buffer should be able to hold xxx.yyy.zzz.www/nn.
 */
static void
ip4_addr2str(const struct ip_addr *addr, char *b, size_t bsz)
{
	char buf[16];

	snprintf(b, bsz, "%s/%hhu", inet_ntop(AF_INET, addr->addr, buf,
	    sizeof(buf)), addr->prefixlen);
}

/*
 * Convert the IPv6 address into CIDR notation conforming to RFC 4291.
 * See also RFC 5952.
 * Must hold 0000:0000:0000:0000:0000:0000:0000:0000/nn.
 */
static void
ip6_addr2str(const struct ip_addr *addr, char *b, size_t bsz)
{
	char	 buf[44];

	snprintf(b, bsz, "%s/%hhu", inet_ntop(AF_INET6, addr->addr, buf,
	    sizeof(buf)), addr->prefixlen);
}

/*
 * Convert a ip_addr into a NUL-terminated CIDR notation string
 * conforming to RFC 4632 or 4291.
 * The size of the buffer must be at least 64 (inclusive).
 */
void
ip_addr_print(const struct ip_addr *addr,
    enum afi afi, char *buf, size_t bufsz)
{

	if (afi == AFI_IPV4)
		ip4_addr2str(addr, buf, bufsz);
	else
		ip6_addr2str(addr, buf, bufsz);
}

/*
 * Serialise an ip_addr for sending over the wire.
 * Matched with ip_addr_read().
 */
void
ip_addr_buffer(char **b, size_t *bsz, size_t *bmax, const struct ip_addr *p)
{
	size_t sz = PREFIX_SIZE(p->prefixlen);

	assert(sz <= 16);
	io_simple_buffer(b, bsz, bmax, &p->prefixlen, sizeof(unsigned char));
	io_simple_buffer(b, bsz, bmax, p->addr, sz);
}

/*
 * Serialise an ip_addr_range for sending over the wire.
 * Matched with ip_addr_range_read().
 */
void
ip_addr_range_buffer(char **b, size_t *bsz, size_t *bmax,
    const struct ip_addr_range *p)
{

	ip_addr_buffer(b, bsz, bmax, &p->min);
	ip_addr_buffer(b, bsz, bmax, &p->max);
}

/*
 * Read an ip_addr from the wire.
 * Matched with ip_addr_buffer().
 */
void
ip_addr_read(int fd, struct ip_addr *p)
{
	size_t sz;

	io_simple_read(fd, &p->prefixlen, sizeof(unsigned char));
	sz = PREFIX_SIZE(p->prefixlen);
	assert(sz <= 16);
	io_simple_read(fd, p->addr, sz);
}

/*
 * Read an ip_addr_range from the wire.
 * Matched with ip_addr_range_buffer().
 */
void
ip_addr_range_read(int fd, struct ip_addr_range *p)
{

	ip_addr_read(fd, &p->min);
	ip_addr_read(fd, &p->max);
}

/*
 * Given the addresses (range or IP) in cert_ip, fill in the "min" and
 * "max" fields with the minimum and maximum possible IP addresses given
 * those ranges (or singleton prefixed range).
 * This does nothing if CERT_IP_INHERIT.
 * Returns zero on failure (misordered ranges), non-zero on success.
 */
int
ip_cert_compose_ranges(struct cert_ip *p)
{
	size_t sz;

	switch (p->type) {
	case CERT_IP_ADDR:
		sz = PREFIX_SIZE(p->ip.prefixlen);
		memset(p->min, 0x0, sizeof(p->min));
		memcpy(p->min, p->ip.addr, sz);
		memset(p->max, 0xff, sizeof(p->max));
		memcpy(p->max, p->ip.addr, sz);
		if (sz > 0 && p->ip.prefixlen % 8 != 0)
			p->max[sz - 1] |= (1 << (8 - p->ip.prefixlen % 8)) - 1;
		break;
	case CERT_IP_RANGE:
		memset(p->min, 0x0, sizeof(p->min));
		sz = PREFIX_SIZE(p->range.min.prefixlen);
		memcpy(p->min, p->range.min.addr, sz);
		memset(p->max, 0xff, sizeof(p->max));
		sz = PREFIX_SIZE(p->range.max.prefixlen);
		memcpy(p->max, p->range.max.addr, sz);
		if (sz > 0 && p->range.max.prefixlen % 8 != 0)
			p->max[sz - 1] |=
			    (1 << (8 - p->range.max.prefixlen % 8)) - 1;
		break;
	default:
		return 1;
	}

	sz = AFI_IPV4 == p->afi ? 4 : 16;
	return memcmp(p->min, p->max, sz) <= 0;
}

/*
 * Given the ROA's acceptable prefix, compute the minimum and maximum
 * address accepted by the prefix.
 */
void
ip_roa_compose_ranges(struct roa_ip *p)
{
	size_t sz = PREFIX_SIZE(p->addr.prefixlen);

	memset(p->min, 0x0, sizeof(p->min));
	memcpy(p->min, p->addr.addr, sz);
	memset(p->max, 0xff, sizeof(p->max));
	memcpy(p->max, p->addr.addr, sz);
	if (sz > 0 && p->addr.prefixlen % 8 != 0)
		p->max[sz - 1] |= (1 << (8 - p->addr.prefixlen % 8)) - 1;
}
