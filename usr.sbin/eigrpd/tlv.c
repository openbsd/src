/*	$OpenBSD: tlv.c,v 1.3 2015/10/04 23:08:57 renato Exp $ */

/*
 * Copyright (c) 2015 Renato Westphal <renato@openbsd.org>
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

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <sys/uio.h>
#include <sys/utsname.h>

#include "eigrpd.h"
#include "eigrp.h"
#include "log.h"
#include "eigrpe.h"

int
gen_parameter_tlv(struct ibuf *buf, struct eigrp_iface *ei)
{
	struct tlv_parameter	 tp;

	tp.type = htons(TLV_TYPE_PARAMETER);
	tp.length = htons(TLV_TYPE_PARAMETER_LEN);
	memcpy(tp.kvalues, ei->eigrp->kvalues, 6);
	tp.holdtime = htons(ei->hello_holdtime);

	return (ibuf_add(buf, &tp, sizeof(tp)));
}

int
gen_sequence_tlv(struct ibuf *buf, struct seq_addr_head *seq_addr_list)
{
	struct tlv		 tlv, *tlvp;
	struct seq_addr_entry	*sa;
	uint8_t			 alen;
	uint16_t		 len = TLV_HDR_LEN;
	size_t			 original_size = ibuf_size(buf);

	tlv.type = htons(TLV_TYPE_SEQ);
	if (ibuf_add(buf, &tlv, sizeof(tlv))) {
		log_warn("%s: ibuf_add failed", __func__);
		return (-1);
	}

	TAILQ_FOREACH(sa, seq_addr_list, entry) {
		switch (sa->af) {
		case AF_INET:
			alen = INADDRSZ;
			if (ibuf_add(buf, &alen, sizeof(alen)))
				return (-1);
			if (ibuf_add(buf, &sa->addr.v4, sizeof(sa->addr.v4))) {
				log_warn("%s: ibuf_add failed", __func__);
				return (-1);
			}
			break;
		case AF_INET6:
			alen = IN6ADDRSZ;
			if (ibuf_add(buf, &alen, sizeof(alen)))
				return (-1);
			if (ibuf_add(buf, &sa->addr.v6, sizeof(sa->addr.v6))) {
				log_warn("%s: ibuf_add failed", __func__);
				return (-1);
			}
			break;
		default:
			log_debug("%s: unkown address family", __func__);
			return (-1);
		}
		len += (sizeof(alen) + alen);
	}

	/* adjust tlv length */
	if ((tlvp = ibuf_seek(buf, original_size, sizeof(*tlvp))) == NULL)
                fatalx("gen_sequence_tlv: buf_seek failed");
	tlvp->length = htons(len);

	return (0);
}

int
gen_sw_version_tlv(struct ibuf *buf)
{
	struct tlv_sw_version	 ts;
	struct utsname		 u;
	unsigned int		 vendor_os_major;
	unsigned int		 vendor_os_minor;

	memset(&ts, 0, sizeof(ts));
	ts.type = htons(TLV_TYPE_SW_VERSION);
	ts.length = htons(TLV_TYPE_SW_VERSION_LEN);
	if (uname(&u) == 0) {
		if (sscanf(u.release, "%u.%u", &vendor_os_major,
		    &vendor_os_minor) == 2) {
			ts.vendor_os_major = (uint8_t) vendor_os_major;
			ts.vendor_os_minor = (uint8_t) vendor_os_minor;
		}
	}
	ts.eigrp_major = EIGRP_VERSION_MAJOR;
	ts.eigrp_minor = EIGRP_VERSION_MINOR;

	return (ibuf_add(buf, &ts, sizeof(ts)));
}

int
gen_mcast_seq_tlv(struct ibuf *buf, uint32_t seq)
{
	struct tlv_mcast_seq	 tm;

	tm.type = htons(TLV_TYPE_MCAST_SEQ);
	tm.length = htons(TLV_TYPE_MCAST_SEQ_LEN);
	tm.seq = htonl(seq);

	return (ibuf_add(buf, &tm, sizeof(tm)));
}

uint16_t
len_route_tlv(struct rinfo *ri)
{
	uint16_t		 len = TLV_HDR_LEN;

	switch (ri->af) {
	case AF_INET:
		len += sizeof(ri->nexthop.v4);
		len += PREFIX_SIZE4(ri->prefixlen);
		break;
	case AF_INET6:
		len += sizeof(ri->nexthop.v6);
		len += PREFIX_SIZE6(ri->prefixlen);
		break;
	default:
		break;
	}

	len += sizeof(ri->metric);
	if (ri->type == EIGRP_ROUTE_EXTERNAL)
		len += sizeof(ri->emetric);

	len += sizeof(ri->prefixlen);

	return (len);
}

int
gen_route_tlv(struct ibuf *buf, struct rinfo *ri)
{
	struct tlv		 tlv, *tlvp;
	struct in_addr		 addr;
	struct classic_metric	 metric;
	struct classic_emetric	 emetric;
	uint16_t		 tlvlen;
	uint8_t			 pflen;
	size_t			 original_size = ibuf_size(buf);

	switch (ri->af) {
	case AF_INET:
		switch (ri->type) {
		case EIGRP_ROUTE_INTERNAL:
			tlv.type = htons(TLV_TYPE_IPV4_INTERNAL);
			break;
		case EIGRP_ROUTE_EXTERNAL:
			tlv.type = htons(TLV_TYPE_IPV4_EXTERNAL);
			break;
		}
		break;
	case AF_INET6:
		switch (ri->type) {
		case EIGRP_ROUTE_INTERNAL:
			tlv.type = htons(TLV_TYPE_IPV6_INTERNAL);
			break;
		case EIGRP_ROUTE_EXTERNAL:
			tlv.type = htons(TLV_TYPE_IPV6_EXTERNAL);
			break;
		}
		break;
	default:
		fatalx("gen_route_tlv: unknown af");
	}
	if (ibuf_add(buf, &tlv, sizeof(tlv)))
		return (-1);
	tlvlen = TLV_HDR_LEN;

	/* nexthop */
	switch (ri->af) {
	case AF_INET:
		addr.s_addr = htonl(ri->nexthop.v4.s_addr);
		if (ibuf_add(buf, &addr, sizeof(addr)))
			return (-1);
		tlvlen += sizeof(ri->nexthop.v4);
		break;
	case AF_INET6:
		if (ibuf_add(buf, &ri->nexthop.v6, sizeof(ri->nexthop.v6)))
			return (-1);
		tlvlen += sizeof(ri->nexthop.v6);
		break;
	default:
		fatalx("gen_route_tlv: unknown af");
	}

	/* exterior metric */
	if (ri->type == EIGRP_ROUTE_EXTERNAL) {
		memcpy(&emetric, &ri->emetric, sizeof(emetric));
		emetric.routerid = htonl(emetric.routerid);
		emetric.as = htonl(emetric.as);
		emetric.tag = htonl(emetric.tag);
		emetric.metric = htonl(emetric.metric);
		emetric.reserved = htons(emetric.reserved);
		if (ibuf_add(buf, &emetric, sizeof(emetric)))
			return (-1);
		tlvlen += sizeof(emetric);
	}

	/* metric */
	memcpy(&metric, &ri->metric, sizeof(metric));
	metric.delay = htonl(metric.delay);
	metric.bandwidth = htonl(metric.bandwidth);
	if (ibuf_add(buf, &metric, sizeof(metric)))
		return (-1);
	tlvlen += sizeof(metric);

	/* destination */
	if (ibuf_add(buf, &ri->prefixlen, sizeof(ri->prefixlen)))
		return (-1);
	switch (ri->af) {
	case AF_INET:
		pflen = PREFIX_SIZE4(ri->prefixlen);
		if (ibuf_add(buf, &ri->prefix.v4, pflen))
			return (-1);
		break;
	case AF_INET6:
		pflen = PREFIX_SIZE6(ri->prefixlen);
		if (ibuf_add(buf, &ri->prefix.v6, pflen))
			return (-1);
		break;
	default:
		fatalx("gen_route_tlv: unknown af");
	}
	tlvlen += sizeof(pflen) + pflen;

	/* adjust tlv length */
	if ((tlvp = ibuf_seek(buf, original_size, sizeof(*tlvp))) == NULL)
                fatalx("gen_ipv4_internal_tlv: buf_seek failed");
	tlvp->length = htons(tlvlen);

	return (0);
}

struct tlv_parameter *
tlv_decode_parameter(struct tlv *tlv, char *buf)
{
	struct tlv_parameter	*tp;

	if (ntohs(tlv->length) != TLV_TYPE_PARAMETER_LEN) {
		log_debug("%s: malformed tlv (bad length)", __func__);
		return (NULL);
	}
	tp = (struct tlv_parameter *)buf;
	return (tp);
}

int
tlv_decode_seq(int af, struct tlv *tlv, char *buf,
    struct seq_addr_head *seq_addr_list)
{
	uint16_t		 len;
	uint8_t			 alen;
	struct seq_addr_entry	*sa;

	len = ntohs(tlv->length);
	if (len < TLV_HDR_LEN) {
		log_debug("%s: malformed tlv (bad length)", __func__);
		return (-1);
	}
	buf += TLV_HDR_LEN;
	len -= TLV_HDR_LEN;

	while (len > 0) {
		memcpy(&alen, buf, sizeof(alen));
		buf += sizeof(alen);
		len -= sizeof(alen);
		if (alen > len) {
			log_debug("%s: malformed tlv (bad length)", __func__);
			return (-1);
		}

		if ((sa = calloc(1, sizeof(*sa))) == NULL)
			fatal("tlv_decode_seq");
		sa->af = af;
		switch (af) {
		case AF_INET:
			if (alen != INADDRSZ) {
				log_debug("%s: invalid address length");
				free(sa);
				return (-1);
			}
			memcpy(&sa->addr.v4, buf, sizeof(struct in_addr));
			break;
		case AF_INET6:
			if (alen != IN6ADDRSZ) {
				log_debug("%s: invalid address length");
				free(sa);
				return (-1);
			}
			memcpy(&sa->addr.v6, buf, sizeof(struct in6_addr));
			break;
		default:
			free(sa);
			fatalx("tlv_decode_seq: unknown af");
		}
		buf += alen;
		len -= alen;
		TAILQ_INSERT_TAIL(seq_addr_list, sa, entry);
	}

	return (0);
}

struct tlv_sw_version *
tlv_decode_sw_version(struct tlv *tlv, char *buf)
{
	struct tlv_sw_version	*tv;

	if (ntohs(tlv->length) != TLV_TYPE_SW_VERSION_LEN) {
		log_debug("%s: malformed tlv (bad length)", __func__);
		return (NULL);
	}
	tv = (struct tlv_sw_version *)buf;
	return (tv);
}

struct tlv_mcast_seq *
tlv_decode_mcast_seq(struct tlv *tlv, char *buf)
{
	struct tlv_mcast_seq	*tm;

	if (ntohs(tlv->length) != TLV_TYPE_MCAST_SEQ_LEN) {
		log_debug("%s: malformed tlv (bad length)", __func__);
		return (NULL);
	}
	tm = (struct tlv_mcast_seq *)buf;
	return (tm);
}

int
tlv_decode_route(int af, enum route_type type, struct tlv *tlv, char *buf,
    struct rinfo *ri)
{
	int		 tlv_len, min_len, plen, offset;
	in_addr_t	 ipv4;

	tlv_len = ntohs(tlv->length);
	switch (af) {
	case AF_INET:
		min_len = TLV_TYPE_IPV4_INT_MIN_LEN;
		break;
	case AF_INET6:
		min_len = TLV_TYPE_IPV6_INT_MIN_LEN;
		break;
	default:
		fatalx("tlv_decode_route: unknown af");
	}
	if (type == EIGRP_ROUTE_EXTERNAL)
		min_len += sizeof(struct classic_emetric);

	if (tlv_len < min_len) {
		log_debug("%s: malformed tlv (bad length)", __func__);
		return (-1);
	}

	ri->af = af;
	ri->type = type;

	/* nexthop */
	offset = TLV_HDR_LEN;
	switch (af) {
	case AF_INET:
		memcpy(&ri->nexthop.v4, buf + offset, sizeof(ri->nexthop.v4));
		offset += sizeof(ri->nexthop.v4);
		break;
	case AF_INET6:
		memcpy(&ri->nexthop.v6, buf + offset, sizeof(ri->nexthop.v6));
		offset += sizeof(ri->nexthop.v6);
		break;
	default:
		fatalx("tlv_decode_route: unknown af");
	}

	/* exterior metric */
	if (type == EIGRP_ROUTE_EXTERNAL) {
		memcpy(&ri->emetric, buf + offset, sizeof(ri->emetric));
		ri->emetric.routerid = ntohl(ri->emetric.routerid);
		ri->emetric.as = ntohl(ri->emetric.as);
		ri->emetric.tag = ntohl(ri->emetric.tag);
		ri->emetric.metric = ntohl(ri->emetric.metric);
		ri->emetric.reserved = ntohs(ri->emetric.reserved);
		offset += sizeof(ri->emetric);
	}

	/* metric */
	memcpy(&ri->metric, buf + offset, sizeof(ri->metric));
	ri->metric.delay = ntohl(ri->metric.delay);
	ri->metric.bandwidth = ntohl(ri->metric.bandwidth);
	offset += sizeof(ri->metric);

	/* prefixlen */
	memcpy(&ri->prefixlen, buf + offset, sizeof(ri->prefixlen));
	offset += sizeof(ri->prefixlen);

	switch (af) {
	case AF_INET:
		plen = PREFIX_SIZE4(ri->prefixlen);
		break;
	case AF_INET6:
		plen = PREFIX_SIZE6(ri->prefixlen);
		break;
	default:
		fatalx("tlv_decode_route: unknown af");
	}

	/* safety check */
	if (plen != (tlv_len - min_len)) {
		log_debug("%s: malformed tlv (invalid prefix length)",
		    __func__);
		return (-1);
	}

	/* destination */
	switch (af) {
	case AF_INET:
		memset(&ri->prefix.v4, 0, sizeof(ri->prefix.v4));
		memcpy(&ri->prefix.v4, buf + offset, plen);

		/* check if the network is valid */
		ipv4 = ntohl(ri->prefix.v4.s_addr);
		if (((ipv4 >> IN_CLASSA_NSHIFT) == IN_LOOPBACKNET) ||
		    IN_MULTICAST(ipv4) || IN_BADCLASS(ipv4)) {
			log_debug("%s: malformed tlv (invalid ipv4 prefix)",
			    __func__);
			return (-1);
		}
		break;
	case AF_INET6:
		memset(&ri->prefix.v6, 0, sizeof(ri->prefix.v6));
		memcpy(&ri->prefix.v6, buf + offset, plen);

		/* check if the network is valid */
		if (IN6_IS_ADDR_LOOPBACK(&ri->prefix.v6) ||
		    IN6_IS_ADDR_MULTICAST(&ri->prefix.v6)) {
			log_debug("%s: malformed tlv (invalid ipv6 prefix)",
			    __func__);
			return (-1);
		}
		break;
	default:
		fatalx("tlv_decode_route: unknown af");
	}

	/* just in case... */
	eigrp_applymask(af, &ri->prefix, &ri->prefix, ri->prefixlen);

	return (0);
}

void
metric_encode_mtu(uint8_t *dst, int mtu)
{
	dst[0] = (mtu & 0x00FF0000) >> 16;
	dst[1] = (mtu & 0x0000FF00) >> 8;
	dst[2] = (mtu & 0x000000FF);
}

int
metric_decode_mtu(uint8_t *mtu)
{
	return ((mtu[0] << 16) + (mtu[1] << 8) + mtu[2]);
}
