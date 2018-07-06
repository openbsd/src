/*	$OpenBSD: print-gre.c,v 1.21 2018/07/06 07:13:21 dlg Exp $	*/

/*
 * Copyright (c) 2002 Jason L. Wright (jason@thought.net)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * tcpdump filter for GRE - Generic Routing Encapsulation
 * RFC1701 (GRE), RFC1702 (GRE IPv4), and RFC2637 (Enhanced GRE)
 */

#include <sys/time.h>
#include <sys/uio.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

#include <net/ethertypes.h>

#include <stdio.h>
#include <string.h>

#include "interface.h"
#include "addrtoname.h"
#include "extract.h"

#define	GRE_CP		0x8000		/* checksum present */
#define	GRE_RP		0x4000		/* routing present */
#define	GRE_KP		0x2000		/* key present */
#define	GRE_SP		0x1000		/* sequence# present */
#define	GRE_sP		0x0800		/* source routing */
#define	GRE_RECRS	0x0700		/* recursion count */
#define	GRE_AP		0x0080		/* acknowledgment# present */
#define	GRE_VERS	0x0007		/* protocol version */

/* source route entry types */
#define	GRESRE_IP	0x0800		/* IP */
#define	GRESRE_ASN	0xfffe		/* ASN */

#define NVGRE_VSID_MASK		0xffffff00U
#define NVGRE_VSID_SHIFT	8
#define NVGRE_FLOWID_MASK	0x000000ffU
#define NVGRE_FLOWID_SHIFT	0

#define GRE_WCCP	0x883e

struct wccp_redirect {
	uint8_t		flags;
#define WCCP_D			(1 << 7)
#define WCCP_A			(1 << 6)
	uint8_t		ServiceId;
	uint8_t		AltBucket;
	uint8_t		PriBucket;
};

void gre_print_0(const u_char *, u_int);
void gre_print_1(const u_char *, u_int);
void gre_print_pptp(const u_char *, u_int, uint16_t);
void gre_print_eoip(const u_char *, u_int, uint16_t);
void gre_sre_print(u_int16_t, u_int8_t, u_int8_t, const u_char *, u_int);
void gre_sre_ip_print(u_int8_t, u_int8_t, const u_char *, u_int);
void gre_sre_asn_print(u_int8_t, u_int8_t, const u_char *, u_int);

void
gre_print(const u_char *p, u_int length)
{
	uint16_t vers;
	int l;

	l = snapend - p;

	if (l < sizeof(vers)) {
		printf("[|gre]");
		return;
	}
	vers = EXTRACT_16BITS(p) & GRE_VERS;

	switch (vers) {
	case 0:
		gre_print_0(p, length);
		break;
	case 1:
		gre_print_1(p, length);
		break;
	default:
		printf("gre-unknown-version=%u", vers);
		break;
	}
}

void
gre_print_0(const u_char *p, u_int length)
{
	uint16_t flags, proto;
	u_int l;

	l = snapend - p;

	flags = EXTRACT_16BITS(p);
	p += sizeof(flags);
	l -= sizeof(flags);
	length -= sizeof(flags);

	printf("gre");

	if (vflag) {
		printf(" [%s%s%s%s%s]",
		    (flags & GRE_CP) ? "C" : "",
		    (flags & GRE_RP) ? "R" : "",
		    (flags & GRE_KP) ? "K" : "",
		    (flags & GRE_SP) ? "S" : "",
		    (flags & GRE_sP) ? "s" : "");
	}

	if (l < sizeof(proto))
		goto trunc;
	proto = EXTRACT_16BITS(p);
	p += sizeof(proto);
	l -= sizeof(proto);
	length -= sizeof(proto);

	if ((flags & GRE_CP) | (flags & GRE_RP)) {
		if (l < 2)
			goto trunc;
		if ((flags & GRE_CP) && vflag)
			printf(" sum 0x%x", EXTRACT_16BITS(p));
		p += 2;
		l -= 2;
		length -= 2;

		if (l < 2)
			goto trunc;
		if (flags & GRE_RP)
			printf(" off 0x%x", EXTRACT_16BITS(p));
		p += 2;
		l -= 2;
		length -= 2;
	}

	if (flags & GRE_KP) {
		uint32_t key, vsid;

		if (l < sizeof(key))
			goto trunc;
		key = EXTRACT_32BITS(p);
		p += sizeof(key);
		l -= sizeof(key);
		length -= sizeof(key);

		/* maybe NVGRE, or key entropy? */
		vsid = (key & NVGRE_VSID_MASK) >> NVGRE_VSID_SHIFT;
		printf(" key=%u|%u+%02x", key, vsid,
		    (key & NVGRE_FLOWID_MASK) >> NVGRE_FLOWID_SHIFT);
	}

	if (flags & GRE_SP) {
		if (l < 4)
			goto trunc;
		printf(" seq %u", EXTRACT_32BITS(p));
		p += 4;
		l -= 4;
		length -= 4;
	}

	if (flags & GRE_RP) {
		for (;;) {
			u_int16_t af;
			u_int8_t sreoff;
			u_int8_t srelen;

			if (l < 4)
				goto trunc;
			af = EXTRACT_16BITS(p);
			sreoff = *(p + 2);
			srelen = *(p + 3);
			p += 4;
			l -= 4;
			length -= 4;

			if (af == 0 && srelen == 0)
				break;

			gre_sre_print(af, sreoff, srelen, p, l);

			if (l < srelen)
				goto trunc;
			p += srelen;
			l -= srelen;
			length -= srelen;
		}
	}

	printf(" ");

	switch (proto) {
	case 0:
		printf("keep-alive");
		break;
	case GRE_WCCP: {
		printf("wccp ");

		if (l == 0)
			return;

		if (*p >> 4 != 4) {
			struct wccp_redirect *wccp;

			if (l < sizeof(*wccp)) {
				printf("[|wccp]");
				return;
			}

			wccp = (struct wccp_redirect *)p;

			printf("D:%c A:%c SId:%u Alt:%u Pri:%u",
			    (wccp->flags & WCCP_D) ? '1' : '0',
			    (wccp->flags & WCCP_A) ? '1' : '0',
			    wccp->ServiceId, wccp->AltBucket, wccp->PriBucket);

			p += sizeof(*wccp);
			l -= sizeof(*wccp);

			printf(": ");
		}

		/* FALLTHROUGH */
	}
	case ETHERTYPE_IP:
		ip_print(p, length);
		break;
	case ETHERTYPE_IPV6:
		ip6_print(p, length);
		break;
	case ETHERTYPE_MPLS:
		mpls_print(p, length);
		break;
	case ETHERTYPE_TRANSETHER:
		ether_tryprint(p, length, 0);
		break;
	default:
		printf("unknown-proto-%04x", proto);
	}
	return;

trunc:
	printf("[|gre]");
}

void
gre_print_1(const u_char *p, u_int length)
{
	uint16_t flags, proto;
	int l;

	l = snapend - p;

	flags = EXTRACT_16BITS(p);
	p += sizeof(flags);
	l -= sizeof(flags);
	length -= sizeof(flags);

	if (l < sizeof(proto))
		goto trunc;

	proto = EXTRACT_16BITS(p);
	p += sizeof(proto);
	l -= sizeof(proto);
	length -= sizeof(proto);

	switch (proto) {
	case ETHERTYPE_PPP:
		gre_print_pptp(p, length, flags);
		break;
	case 0x6400:
		/* MikroTik RouterBoard Ethernet over IP (EoIP) */
		gre_print_eoip(p, length, flags);
		break;
	default:
		printf("unknown-gre1-proto-%04x", proto);
		break;
	}

	return;

trunc:
	printf("[|gre1]");
}

void
gre_print_pptp(const u_char *p, u_int length, uint16_t flags)
{
	uint16_t len;
	int l;

	l = snapend - p;

	printf("pptp");

	if (vflag) {
		printf(" [%s%s%s%s%s%s]",
		    (flags & GRE_CP) ? "C" : "",
		    (flags & GRE_RP) ? "R" : "",
		    (flags & GRE_KP) ? "K" : "",
		    (flags & GRE_SP) ? "S" : "",
		    (flags & GRE_sP) ? "s" : "",
		    (flags & GRE_AP) ? "A" : "");
	}

	if (flags & GRE_CP) {
		printf(" cpset!");
		return;
	}
	if (flags & GRE_RP) {
		printf(" rpset!");
		return;
	}
	if ((flags & GRE_KP) == 0) {
		printf(" kpunset!");
		return;
	}
	if (flags & GRE_sP) {
		printf(" spset!");
		return;
	}

	/* GRE_KP */
	if (l < sizeof(len))
		goto trunc;
	len = EXTRACT_16BITS(p);
	p += sizeof(len);
	l -= sizeof(len);
	length -= sizeof(len);

	if (vflag)
		printf(" len %u", EXTRACT_16BITS(p));

	if (l < 2)
		goto trunc;
	printf(" callid %u", EXTRACT_16BITS(p));
	p += 2;
	l -= 2;
	length -= 2;

	if (flags & GRE_SP) {
		if (l < 4)
			goto trunc;
		printf(" seq %u", EXTRACT_32BITS(p));
		p += 4;
		l -= 4;
		length -= 4;
	}

	if (flags & GRE_AP) {
		if (l < 4)
			goto trunc;
		printf(" ack %u", EXTRACT_32BITS(p));
		p += 4;
		l -= 4;
		length -= 4;
	}

	if ((flags & GRE_SP) == 0)
		return;

        if (length < len) {
                (void)printf(" truncated-pptp - %d bytes missing!",
		    len - length);
		len = length;
	}

	printf(": ");

	ppp_hdlc_print(p, len);
	return;

trunc:
	printf("[|pptp]");
}

void
gre_print_eoip(const u_char *p, u_int length, uint16_t flags)
{
	uint16_t len, id;
	int l;

	l = snapend - p;

	printf("eoip");

	flags &= ~GRE_VERS;
	if (flags != GRE_KP) {
		printf(" unknown-eoip-flags-%04x!", flags);
		return;
	}

	if (l < sizeof(len))
		goto trunc;

	len = EXTRACT_16BITS(p);
	p += sizeof(len);
	l -= sizeof(len);
	length -= sizeof(len);

	if (l < sizeof(id))
		goto trunc;

	id = EXTRACT_LE_16BITS(p);
	p += sizeof(id);
	l -= sizeof(id);
	length -= sizeof(id);

	if (vflag)
		printf(" len=%u tunnel-id=%u", len, id);
	else
		printf(" %u", id);

        if (length < len) {
                (void)printf(" truncated-eoip - %d bytes missing!",
		    len - length);
		len = length;
	}

	printf(": ");

	if (len == 0)
		printf("keepalive");
	else
		ether_tryprint(p, len, 0);

	return;

trunc:
	printf("[|eoip]");
}

void
gre_sre_print(u_int16_t af, u_int8_t sreoff, u_int8_t srelen,
    const u_char *bp, u_int len)
{
	switch (af) {
	case GRESRE_IP:
		printf(" (rtaf=ip");
		gre_sre_ip_print(sreoff, srelen, bp, len);
		printf(")");
		break;
	case GRESRE_ASN:
		printf(" (rtaf=asn");
		gre_sre_asn_print(sreoff, srelen, bp, len);
		printf(")");
		break;
	default:
		printf(" (rtaf=0x%x)", af);
	}
}
void
gre_sre_ip_print(u_int8_t sreoff, u_int8_t srelen, const u_char *bp, u_int len)
{
	struct in_addr a;
	const u_char *up = bp;

	if (sreoff & 3) {
		printf(" badoffset=%u", sreoff);
		return;
	}
	if (srelen & 3) {
		printf(" badlength=%u", srelen);
		return;
	}
	if (sreoff >= srelen) {
		printf(" badoff/len=%u/%u", sreoff, srelen);
		return;
	}

	for (;;) {
		if (len < 4 || srelen == 0)
			return;

		memcpy(&a, bp, sizeof(a));
		printf(" %s%s",
		    ((bp - up) == sreoff) ? "*" : "",
		    inet_ntoa(a));

		bp += 4;
		len -= 4;
		srelen -= 4;
	}
}

void
gre_sre_asn_print(u_int8_t sreoff, u_int8_t srelen, const u_char *bp, u_int len)
{
	const u_char *up = bp;

	if (sreoff & 1) {
		printf(" badoffset=%u", sreoff);
		return;
	}
	if (srelen & 1) {
		printf(" badlength=%u", srelen);
		return;
	}
	if (sreoff >= srelen) {
		printf(" badoff/len=%u/%u", sreoff, srelen);
		return;
	}

	for (;;) {
		if (len < 2 || srelen == 0)
			return;

		printf(" %s%x",
		    ((bp - up) == sreoff) ? "*" : "",
		    EXTRACT_16BITS(bp));

		bp += 2;
		len -= 2;
		srelen -= 2;
	}
}

struct vxlan_header {
	uint16_t	flags;
#define VXLAN_I			0x0800
	uint16_t	proto;
	uint32_t	vni;
#define VXLAN_VNI_SHIFT		8
#define VXLAN_VNI_MASK		(0xffffffU << VXLAN_VNI_SHIFT)
#define VXLAN_VNI_RESERVED	(~VXLAN_VNI_MASK)
};

void
vxlan_print(const u_char *p, u_int length)
{
	const struct vxlan_header *vh;
	uint16_t flags, proto;
	uint32_t vni;
	size_t l;

	l = snapend - p;
	if (l < sizeof(*vh)) {
		printf("[|vxlan]");
		return;
	}
	vh = (const struct vxlan_header *)p;

	flags = ntohs(vh->flags);
	if (flags & ~VXLAN_I) {
		printf("vxlan-invalid-flags %04x", flags);
		return;
	}

	proto = ntohs(vh->proto);
	if (proto != 0) {
		printf("vxlan-invalid-proto %04x", proto);
		return;
	}

	vni = ntohl(vh->vni);
	if (flags & VXLAN_I) {
		if (vni & VXLAN_VNI_RESERVED) {
			printf("vxlan-vni-reserved %02x",
			    vni & VXLAN_VNI_RESERVED);
			return;
		}

		printf("vxlan %u: ", vni >> VXLAN_VNI_SHIFT);
	} else {
		if (vh->vni != 0) {
			printf("vxlan-invalid-vni %08x\n", vni);
			return;
		}

		printf("vxlan: ");
	}

	p += sizeof(*vh);
	length -= sizeof(*vh);

	ether_tryprint(p, length, 0);
}
