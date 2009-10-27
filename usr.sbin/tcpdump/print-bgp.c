/*	$OpenBSD: print-bgp.c,v 1.14 2009/10/27 23:59:55 deraadt Exp $	*/

/*
 * Copyright (C) 1999 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>

#include "interface.h"
#include "addrtoname.h"
#include "extract.h"
#include "afnum.h"

struct bgp {
	u_int8_t bgp_marker[16];
	u_int16_t bgp_len;
	u_int8_t bgp_type;
};
#define BGP_SIZE		19	/* unaligned */

#define BGP_OPEN		1
#define BGP_UPDATE		2
#define BGP_NOTIFICATION	3
#define BGP_KEEPALIVE		4
#define BGP_ROUTE_REFRESH	5

struct bgp_open {
	u_int8_t bgpo_marker[16];
	u_int16_t bgpo_len;
	u_int8_t bgpo_type;
	u_int8_t bgpo_version;
	u_int16_t bgpo_myas;
	u_int16_t bgpo_holdtime;
	u_int32_t bgpo_id;
	u_int8_t bgpo_optlen;
	/* options should follow */
};
#define BGP_OPEN_SIZE		29	/* unaligned */

struct bgp_opt {
	u_int8_t bgpopt_type;
	u_int8_t bgpopt_len;
	/* variable length */
};
#define BGP_OPT_CAP		2
#define BGP_OPT_SIZE		2	/* some compilers may pad to 4 bytes */

#define BGP_UPDATE_MINSIZE	23

struct bgp_notification {
	u_int8_t bgpn_marker[16];
	u_int16_t bgpn_len;
	u_int8_t bgpn_type;
	u_int8_t bgpn_major;
	u_int8_t bgpn_minor;
	/* data should follow */
};
#define BGP_NOTIFICATION_SIZE		21	/* unaligned */

struct bgp_route_refresh {
	u_int8_t bgp_marker[16];
	u_int16_t len;
	u_int8_t type;
	u_int8_t afi[2]; /* unaligned; should be u_int16_t */
	u_int8_t res;
	u_int8_t safi;
};
#define BGP_ROUTE_REFRESH_SIZE          23

struct bgp_attr {
	u_int8_t bgpa_flags;
	u_int8_t bgpa_type;
	union {
		u_int8_t len;
		u_int16_t elen;
	} bgpa_len;
#define bgp_attr_len(p) \
	(((p)->bgpa_flags & 0x10) ? \
		ntohs((p)->bgpa_len.elen) : (p)->bgpa_len.len)
#define bgp_attr_off(p) \
	(((p)->bgpa_flags & 0x10) ? 4 : 3)
};

#define BGPTYPE_ORIGIN			1
#define BGPTYPE_AS_PATH			2
#define BGPTYPE_NEXT_HOP		3
#define BGPTYPE_MULTI_EXIT_DISC		4
#define BGPTYPE_LOCAL_PREF		5
#define BGPTYPE_ATOMIC_AGGREGATE	6
#define BGPTYPE_AGGREGATOR		7
#define	BGPTYPE_COMMUNITIES		8	/* RFC1997 */
#define	BGPTYPE_ORIGINATOR_ID		9	/* RFC1998 */
#define	BGPTYPE_CLUSTER_LIST		10	/* RFC1998 */
#define	BGPTYPE_DPA			11	/* draft-ietf-idr-bgp-dpa */
#define	BGPTYPE_ADVERTISERS		12	/* RFC1863 */
#define	BGPTYPE_RCID_PATH		13	/* RFC1863 */
#define BGPTYPE_MP_REACH_NLRI		14	/* RFC2283 */
#define BGPTYPE_MP_UNREACH_NLRI		15	/* RFC2283 */
#define BGPTYPE_EXTD_COMMUNITIES	16	/* RFC4360 */
#define BGPTYPE_AS4_PATH		17	/* RFC4893 */
#define BGPTYPE_AGGREGATOR4		18	/* RFC4893 */

#define BGP_AS_SET             1
#define BGP_AS_SEQUENCE        2
#define BGP_CONFED_AS_SEQUENCE 3 /* draft-ietf-idr-rfc3065bis-01 */
#define BGP_CONFED_AS_SET      4 /* draft-ietf-idr-rfc3065bis-01  */

static struct tok bgp_as_path_segment_open_values[] = {
	{ BGP_AS_SET,			" {" },
	{ BGP_AS_SEQUENCE,		" " },
	{ BGP_CONFED_AS_SEQUENCE,	" (" },
	{ BGP_CONFED_AS_SET,		" ({" },
	{ 0, NULL},
};

static struct tok bgp_as_path_segment_close_values[] = {
	{ BGP_AS_SET,			"}" },
	{ BGP_AS_SEQUENCE,		"" },
	{ BGP_CONFED_AS_SEQUENCE,	")" },
	{ BGP_CONFED_AS_SET,		"})" },
	{ 0, NULL},
};

#define BGP_MP_NLRI_MINSIZE		3

static const char *bgptype[] = {
	NULL, "OPEN", "UPDATE", "NOTIFICATION", "KEEPALIVE", "ROUTE-REFRESH",
};
#define bgp_type(x) num_or_str(bgptype, sizeof(bgptype)/sizeof(bgptype[0]), (x))

static const char *bgpopt_type[] = {
	NULL, "Authentication Information", "Capabilities Advertisement",
};
#define bgp_opttype(x) \
	num_or_str(bgpopt_type, sizeof(bgpopt_type)/sizeof(bgpopt_type[0]), (x))

#define BGP_CAPCODE_MP			1
#define BGP_CAPCODE_REFRESH		2
#define BGP_CAPCODE_RESTART		64 /* draft-ietf-idr-restart-05  */
#define BGP_CAPCODE_AS4			65 /* RFC4893 */

static const char *bgp_capcode[] = {
	NULL, "MULTI_PROTOCOL", "ROUTE_REFRESH",
	/* 3: RFC5291 */ "OUTBOUND_ROUTE_FILTERING",
	/* 4: RFC3107 */ "MULTIPLE_ROUTES",
	/* 5: RFC5549 */ "EXTENDED_NEXTHOP_ENCODING",
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	/* 64: RFC4724 */ "GRACEFUL_RESTART",
	/* 65: RFC4893 */ "AS4", 0,
	/* 67: [Chen] */ "DYNAMIC_CAPABILITY",
	/* 68: [Appanna] */ "MULTISESSION",
	/* 69: [draft-ietf-idr-add-paths] */ "ADD-PATH",
};

#define bgp_capcode(x) \
	num_or_str(bgp_capcode, sizeof(bgp_capcode)/sizeof(bgp_capcode[0]), (x))

#define BGP_NOTIFY_MAJOR_CEASE		6
static const char *bgpnotify_major[] = {
	NULL, "Message Header Error",
	"OPEN Message Error", "UPDATE Message Error",
	"Hold Timer Expired", "Finite State Machine Error",
	"Cease", "Capability Message Error",
};
#define bgp_notify_major(x) \
	num_or_str(bgpnotify_major, \
		sizeof(bgpnotify_major)/sizeof(bgpnotify_major[0]), (x))

static const char *bgpnotify_minor_msg[] = {
	NULL, "Connection Not Synchronized",
	"Bad Message Length", "Bad Message Type",
};

static const char *bgpnotify_minor_open[] = {
	NULL, "Unsupported Version Number",
	"Bad Peer AS", "Bad BGP Identifier",
	"Unsupported Optional Parameter", "Authentication Failure",
	"Unacceptable Hold Time", "Unsupported Capability",
};

static const char *bgpnotify_minor_update[] = {
	NULL, "Malformed Attribute List",
	"Unrecognized Well-known Attribute", "Missing Well-known Attribute",
	"Attribute Flags Error", "Attribute Length Error",
	"Invalid ORIGIN Attribute", "AS Routing Loop",
	"Invalid NEXT_HOP Attribute", "Optional Attribute Error",
	"Invalid Network Field", "Malformed AS_PATH",
};

/* RFC 4486 */
#define BGP_NOTIFY_MINOR_CEASE_MAXPRFX  1
static const char *bgpnotify_minor_cease[] = {
	NULL, "Maximum Number of Prefixes Reached", "Administratively Shutdown",
	"Peer De-configured", "Administratively Reset", "Connection Rejected",
	"Other Configuration Change", "Connection Collision Resolution",
	"Out of Resources",
};

static const char *bgpnotify_minor_cap[] = {
	NULL, "Invalid Action Value", "Invalid Capability Length",
	"Malformed Capability Value", "Unsupported Capability Code",
};

static const char **bgpnotify_minor[] = {
	NULL, bgpnotify_minor_msg, bgpnotify_minor_open, bgpnotify_minor_update,
};
static const int bgpnotify_minor_siz[] = {
	0,
	sizeof(bgpnotify_minor_msg)/sizeof(bgpnotify_minor_msg[0]),
	sizeof(bgpnotify_minor_open)/sizeof(bgpnotify_minor_open[0]),
	sizeof(bgpnotify_minor_update)/sizeof(bgpnotify_minor_update[0]),
	0,
	0,
	sizeof(bgpnotify_minor_cease)/sizeof(bgpnotify_minor_cease[0]),
	sizeof(bgpnotify_minor_cap)/sizeof(bgpnotify_minor_cap[0]),
};

static const char *bgpattr_origin[] = {
	"IGP", "EGP", "INCOMPLETE",
};
#define bgp_attr_origin(x) \
	num_or_str(bgpattr_origin, \
		sizeof(bgpattr_origin)/sizeof(bgpattr_origin[0]), (x))

static const char *bgpattr_type[] = {
	NULL, "ORIGIN", "AS_PATH", "NEXT_HOP",
	"MULTI_EXIT_DISC", "LOCAL_PREF", "ATOMIC_AGGREGATE", "AGGREGATOR",
	"COMMUNITIES", "ORIGINATOR_ID", "CLUSTER_LIST", "DPA",
	"ADVERTISERS", "RCID_PATH", "MP_REACH_NLRI", "MP_UNREACH_NLRI",
	"EXTD_COMMUNITIES", "AS4_PATH", "AGGREGATOR4",
};
#define bgp_attr_type(x) \
	num_or_str(bgpattr_type, \
		sizeof(bgpattr_type)/sizeof(bgpattr_type[0]), (x))

/* Subsequent address family identifier, RFC2283 section 7 */
static const char *bgpattr_nlri_safi[] = {
	"Reserved", "Unicast", "Multicast", "Unicast+Multicast",
	"labeled Unicast", /* MPLS BGP RFC3107 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	/* 64-66: MPLS BGP RFC3107 */
	"Tunnel", "VPLS", "MDT",
};
#define bgp_attr_nlri_safi(x) \
	num_or_str(bgpattr_nlri_safi, \
		sizeof(bgpattr_nlri_safi)/sizeof(bgpattr_nlri_safi[0]), (x))

/* well-known community */
#define BGP_COMMUNITY_NO_EXPORT			0xffffff01
#define BGP_COMMUNITY_NO_ADVERT			0xffffff02
#define BGP_COMMUNITY_NO_EXPORT_SUBCONFED	0xffffff03
#define BGP_COMMUNITY_NO_PEER			0xffffff04

static const char *afnumber[] = AFNUM_NAME_STR;
#define af_name(x) \
	(((x) == 65535) ? afnumber[0] : \
		num_or_str(afnumber, \
			sizeof(afnumber)/sizeof(afnumber[0]), (x)))


static const char *
num_or_str(const char **table, size_t siz, int value)
{
	static char buf[20];
	if (value < 0 || siz <= value || table[value] == NULL) {
		snprintf(buf, sizeof(buf), "#%d", value);
		return buf;
	} else
		return table[value];
}

static const char *
bgp_notify_minor(int major, int minor)
{
	static const char **table;
	int siz;
	static char buf[20];
	const char *p;

	if (0 <= major
	 && major < sizeof(bgpnotify_minor)/sizeof(bgpnotify_minor[0])
	 && bgpnotify_minor[major]) {
		table = bgpnotify_minor[major];
		siz = bgpnotify_minor_siz[major];
		if (0 <= minor && minor < siz && table[minor])
			p = table[minor];
		else
			p = NULL;
	} else
		p = NULL;
	if (p == NULL) {
		snprintf(buf, sizeof(buf), "#%d", minor);
		return buf;
	} else
		return p;
}

static int
decode_prefix4(const u_char *pd, char *buf, u_int buflen)
{
	struct in_addr addr;
	u_int plen;
	int n;

	TCHECK(pd[0]);
	plen = pd[0]; /*
		       * prefix length is in bits; packet only contains
		       * enough bytes of address to contain this many bits
		       */
	plen = pd[0];
	if (plen < 0 || 32 < plen)
		return -1;
	memset(&addr, 0, sizeof(addr));
	TCHECK2(pd[1], (plen + 7) / 8);
	memcpy(&addr, &pd[1], (plen + 7) / 8);
	if (plen % 8) {
		((u_char *)&addr)[(plen + 7) / 8 - 1] &=
			((0xff00 >> (plen % 8)) & 0xff);
	}
	n = snprintf(buf, buflen, "%s/%d", getname((u_char *)&addr), plen);
	if (n == -1 || n >= buflen)
		return -1;

	return 1 + (plen + 7) / 8;

trunc:
	return -2;
}

#ifdef INET6
static int
decode_prefix6(const u_char *pd, char *buf, u_int buflen)
{
	struct in6_addr addr;
	u_int plen;
	int n;

	TCHECK(pd[0]);
	plen = pd[0];
	if (plen < 0 || 128 < plen)
		return -1;

	memset(&addr, 0, sizeof(addr));
	TCHECK2(pd[1], (plen + 7) / 8);
	memcpy(&addr, &pd[1], (plen + 7) / 8);
	if (plen % 8) {
		addr.s6_addr[(plen + 7) / 8 - 1] &=
			((0xff00 >> (plen % 8)) & 0xff);
	}

	n = snprintf(buf, buflen, "%s/%d", getname6((u_char *)&addr), plen);
	if (n == -1 || n >= buflen)
		return -1;

	return 1 + (plen + 7) / 8;

trunc:
	return -2;
}
#endif

static int
bgp_attr_print(const struct bgp_attr *attr, const u_char *dat, int len)
{
	int i;
	u_int16_t af;
	u_int8_t safi, snpa;
	int advance;
	int tlen, asn_bytes;
	const u_char *p;
	char buf[MAXHOSTNAMELEN + 100];

	p = dat;
	tlen = len;
	asn_bytes = 0;

	switch (attr->bgpa_type) {
	case BGPTYPE_ORIGIN:
		if (len != 1)
			printf(" invalid len");
		else {
			TCHECK(p[0]);
			printf(" %s", bgp_attr_origin(p[0]));
		}
		break;
	case BGPTYPE_AS4_PATH:
		asn_bytes = 4;
		/* FALLTHROUGH */
	case BGPTYPE_AS_PATH:
	/*
	 * 2-byte speakers will receive AS4_PATH as well AS_PATH (2-byte).
	 * 4-byte speakers will only receive AS_PATH but it will be 4-byte.
	 * To identify which is the case, compare the length of the path
	 * segment value in bytes, with the path segment length from the
	 * message (counted in # of AS)
	 */
	 
		if (len % 2) {
			printf(" invalid len");
			break;
		}
		if (!len) {
			printf(" empty");
			break;
		}
		while (p < dat + len) {
			TCHECK(p[0]);
			if (asn_bytes == 0) {
				asn_bytes = (len-2)/p[1];
			}
			printf("%s",
			    tok2str(bgp_as_path_segment_open_values,
			    "?", p[0]));

			for (i = 0; i < p[1] * asn_bytes; i += asn_bytes) {
				TCHECK2(p[2 + i], asn_bytes);
				printf("%s", i == 0 ? "" : " ");
				if (asn_bytes == 2 || EXTRACT_16BITS(&p[2 + i]))
					printf("%u%s",
					    EXTRACT_16BITS(&p[2 + i]),
					    asn_bytes == 4 ? "." : "");
				if (asn_bytes == 4)
					printf("%u",
					    EXTRACT_16BITS(&p[2 + i + 2]));
			}
			TCHECK(p[0]);
			printf("%s",
			    tok2str(bgp_as_path_segment_close_values,
			    "?", p[0]));
			TCHECK(p[1]);
			p += 2 + p[1] * asn_bytes;
		}
		break;
	case BGPTYPE_NEXT_HOP:
		if (len != 4)
			printf(" invalid len");
		else {
			TCHECK2(p[0], 4);
			printf(" %s", getname(p));
		}
		break;
	case BGPTYPE_MULTI_EXIT_DISC:
	case BGPTYPE_LOCAL_PREF:
		if (len != 4)
			printf(" invalid len");
		else {
			TCHECK2(p[0], 4);
			printf(" %u", EXTRACT_32BITS(p));
		}
		break;
	case BGPTYPE_ATOMIC_AGGREGATE:
		if (len != 0)
			printf(" invalid len");
		break;
	case BGPTYPE_AGGREGATOR4:
	case BGPTYPE_AGGREGATOR:
	/*
	 * like AS_PATH/AS4_PATH, AGGREGATOR can contain
	 * either 2-byte or 4-byte ASN, and AGGREGATOR4
	 * always contains 4-byte ASN.
	 */
		if (len != 6 && len != 8) {
			printf(" invalid len");
			break;
		}
		TCHECK2(p[0], len);
		printf(" AS #");
		if (len == 6 || EXTRACT_16BITS(p))
			printf("%u%s", EXTRACT_16BITS(p), len == 8 ? "." : "");
		if (len == 8)
			printf("%u", EXTRACT_16BITS(p+2));
		printf(", origin %s", getname(p+len-4));
		break;
	case BGPTYPE_COMMUNITIES:
		if (len % 4) {
			printf(" invalid len");
			break;
		}
		while (tlen>0) {
			u_int32_t comm;
			TCHECK2(p[0], 4);
			comm = EXTRACT_32BITS(p);
			switch (comm) {
			case BGP_COMMUNITY_NO_EXPORT:
				printf(" NO_EXPORT");
				break;
			case BGP_COMMUNITY_NO_ADVERT:
				printf(" NO_ADVERTISE");
				break;
			case BGP_COMMUNITY_NO_EXPORT_SUBCONFED:
				printf(" NO_EXPORT_SUBCONFED");
				break;
			case BGP_COMMUNITY_NO_PEER:
				printf(" NO_PEER");
				break;
			default:
				printf(" %d:%d",
					(comm >> 16) & 0xffff, comm & 0xffff);
				break;
			}
			tlen -= 4;
			p += 4;
		}
		break;
	case BGPTYPE_ORIGINATOR_ID:
		if (len != 4) {
			printf(" invalid len");
			break;
                }
		TCHECK2(p[0], 4);
		printf("%s",getname(p));
		break;
	case BGPTYPE_CLUSTER_LIST:
		if (len % 4) {
			printf(" invalid len");
			break;
		}
		while (tlen>0) {
			TCHECK2(p[0], 4);
			printf(" %s%s",
			    getname(p),
			    (tlen>4) ? ", " : "");
			tlen -=4;
			p +=4;
		}
		break;
	case BGPTYPE_MP_REACH_NLRI:
		TCHECK2(p[0], BGP_MP_NLRI_MINSIZE);
		af = EXTRACT_16BITS(p);
		safi = p[2];
		if (safi >= 128)
			printf(" %s vendor specific %u,", af_name(af), safi);
		else {
			printf(" %s %s,", af_name(af),
				bgp_attr_nlri_safi(safi));
		}
		p += 3;

		if (af == AFNUM_INET)
			;
#ifdef INET6
		else if (af == AFNUM_INET6)
			;
#endif
		else
			break;

		TCHECK(p[0]);
		tlen = p[0];
		if (tlen) {
			printf(" nexthop");
			i = 0;
			while (i < tlen) {
				switch (af) {
				case AFNUM_INET:
					TCHECK2(p[1+i], sizeof(struct in_addr));
					printf(" %s", getname(p + 1 + i));
					i += sizeof(struct in_addr);
					break;
#ifdef INET6
				case AFNUM_INET6:
					TCHECK2(p[1+i], sizeof(struct in6_addr));
					printf(" %s", getname6(p + 1 + i));
					i += sizeof(struct in6_addr);
					break;
#endif
				default:
					printf(" (unknown af)");
					i = tlen;	/*exit loop*/
					break;
				}
			}
			printf(",");
		}
		p += 1 + tlen;

		TCHECK(p[0]);
		snpa = p[0];
		p++;
		if (snpa) {
			printf(" %u snpa", snpa);
			for (/*nothing*/; snpa > 0; snpa--) {
				TCHECK(p[0]);
				printf("(%d bytes)", p[0]);
				p += p[0] + 1;
			}
			printf(",");
		}

		printf(" NLRI");
		while (len - (p - dat) > 0) {
			switch (af) {
			case AFNUM_INET:
				advance = decode_prefix4(p, buf, sizeof(buf));
				break;
#ifdef INET6
			case AFNUM_INET6:
				advance = decode_prefix6(p, buf, sizeof(buf));
				break;
#endif
			default:
				printf(" (unknown af)");
				advance = 0;
				p = dat + len;
				break;
			}

			if (advance <= 0)
				break;

			printf(" %s", buf);
			p += advance;
		}

		break;

	case BGPTYPE_MP_UNREACH_NLRI:
		TCHECK2(p[0], BGP_MP_NLRI_MINSIZE);
		af = EXTRACT_16BITS(p);
		safi = p[2];
		if (safi >= 128)
			printf(" %s vendor specific %u,", af_name(af), safi);
		else {
			printf(" %s %s,", af_name(af),
				bgp_attr_nlri_safi(safi));
		}
		p += 3;

		printf(" Withdraw");
		while (len - (p - dat) > 0) {
			switch (af) {
			case AFNUM_INET:
				advance = decode_prefix4(p, buf, sizeof(buf));
				break;
#ifdef INET6
			case AFNUM_INET6:
				advance = decode_prefix6(p, buf, sizeof(buf));
				break;
#endif
			default:
				printf(" (unknown af)");
				advance = 0;
				p = dat + len;
				break;
			}

			if (advance <= 0)
				break;

			printf(" %s", buf);
			p += advance;
		}
		break;
	default:
		break;
	}
	return 1;

trunc:
	return 0;
}

static void
bgp_open_capa_print(const u_char *opt, int length)
{
	int i,cap_type,cap_len,tcap_len,cap_offset;

	i = 0;
	while (i < length) {
		TCHECK2(opt[i], 2);

		cap_type=opt[i];
		cap_len=opt[i+1];
		printf("%sCAP %s", i == 0 ? "(" : " ", 		/* ) */
		    bgp_capcode(cap_type));

		/* can we print the capability? */
		TCHECK2(opt[i+2],cap_len);
		i += 2;

		switch(cap_type) {
		case BGP_CAPCODE_MP:
			if (cap_len != 4) {
				printf(" BAD ENCODING");
				break;
			}
			printf(" [%s %s]",
			    af_name(EXTRACT_16BITS(opt+i)),
			    bgp_attr_nlri_safi(opt[i+3]));
			break;
		case BGP_CAPCODE_REFRESH:
			if (cap_len != 0) {
				printf(" BAD ENCODING");
				break;
			}
			break;
		case BGP_CAPCODE_RESTART:
			if (cap_len < 2 || (cap_len - 2) % 4) {
				printf(" BAD ENCODING");
				break;
			}
			printf(" [%s], Time %us",
			    ((opt[i])&0x80) ? "R" : "none",
			    EXTRACT_16BITS(opt+i)&0xfff);
			tcap_len=cap_len - 2;
			cap_offset=2;
			while(tcap_len>=4) {
				printf(" (%s %s)%s",
				    af_name(EXTRACT_16BITS(opt+i+cap_offset)),
				    bgp_attr_nlri_safi(opt[i+cap_offset+2]),
				    ((opt[i+cap_offset+3])&0x80) ?
					" forwarding state preserved" : "" );
				tcap_len-=4;
				cap_offset+=4;
			}
			break;
		case BGP_CAPCODE_AS4:
			if (cap_len != 4) {
				printf(" BAD ENCODING");
				break;
			}
			printf(" #");
			if (EXTRACT_16BITS(opt+i))
				printf("%u.",
				    EXTRACT_16BITS(opt+i));
			printf("%u",
			    EXTRACT_16BITS(opt+i+2));
			break;
		default:
			printf(" len %d", cap_len);
			break;
		}
		i += cap_len;
		if (i + cap_len < length)
			printf(",");
	}
	/* ( */
	printf(")");
	return;
trunc:
	printf("[|BGP]");
}

static void
bgp_open_print(const u_char *dat, int length)
{
	struct bgp_open bgpo;
	struct bgp_opt bgpopt;
	const u_char *opt;
	int i;

	TCHECK2(dat[0], BGP_OPEN_SIZE);
	memcpy(&bgpo, dat, BGP_OPEN_SIZE);

	printf(": Version %d,", bgpo.bgpo_version);
	printf(" AS #%u,", ntohs(bgpo.bgpo_myas));
	printf(" Holdtime %u,", ntohs(bgpo.bgpo_holdtime));
	printf(" ID %s,", getname((u_char *)&bgpo.bgpo_id));
	printf(" Option length %u", bgpo.bgpo_optlen);

	/* sanity checking */
	if ((length < bgpo.bgpo_optlen+BGP_OPEN_SIZE) || (!bgpo.bgpo_optlen))
		return;

	/* ugly! */
	opt = &((const struct bgp_open *)dat)->bgpo_optlen;
	opt++;

	i = 0;
	while (i < bgpo.bgpo_optlen) {
		TCHECK2(opt[i], BGP_OPT_SIZE);
		memcpy(&bgpopt, &opt[i], BGP_OPT_SIZE);
		if (i + 2 + bgpopt.bgpopt_len > bgpo.bgpo_optlen) {
			printf(" [|opt %d %d]", bgpopt.bgpopt_len, bgpopt.bgpopt_type);
			break;
		}

		if (i == 0)
			printf(" (");		/* ) */
		else
			printf(" ");

		switch(bgpopt.bgpopt_type) {
		case BGP_OPT_CAP:
			bgp_open_capa_print(opt + i + BGP_OPT_SIZE,
			    bgpopt.bgpopt_len);
			break;
		default:
			printf(" (option %s, len=%u)",
			    bgp_opttype(bgpopt.bgpopt_type),
			    bgpopt.bgpopt_len);
			break;
		}

		i += BGP_OPT_SIZE + bgpopt.bgpopt_len;
	}
	/* ( */
	printf(")");	
	return;
trunc:
	printf("[|BGP]");
}

static void
bgp_update_print(const u_char *dat, int length)
{
	struct bgp bgp;
	struct bgp_attr bgpa;
	const u_char *p;
	int len;
	int i;
	int newline;

	TCHECK2(dat[0], BGP_SIZE);
	memcpy(&bgp, dat, BGP_SIZE);
	p = dat + BGP_SIZE;	/*XXX*/
	printf(":");

	/* Unfeasible routes */
	len = EXTRACT_16BITS(p);
	if (len) {
		/*
		 * Without keeping state from the original NLRI message,
		 * it's not possible to tell if this a v4 or v6 route,
		 * so only try to decode it if we're not v6 enabled.
	         */
#ifdef INET6
		printf(" (Withdrawn routes: %d bytes)", len);
#else	
		char buf[MAXHOSTNAMELEN + 100];
		int wpfx;

		TCHECK2(p[2], len);
 		i = 2;

		printf(" (Withdrawn routes:");
			
		while(i < 2 + len) {
			wpfx = decode_prefix4(&p[i], buf, sizeof(buf));
			if (wpfx = -1) {
				printf(" (illegal prefix length)");
				break;
			} else if (wpfx == -2)
				goto trunc;
			i += wpfx;
			printf(" %s", buf);
		}
		printf(")");
#endif
	}
	p += 2 + len;

	TCHECK2(p[0], 2);
	len = EXTRACT_16BITS(p);

	if (len == 0 && length == BGP_UPDATE_MINSIZE) {
		printf(" End-of-Rib Marker (empty NLRI)");
		return;
	}

	if (len) {
		/* do something more useful!*/
		i = 2;
		printf(" (Path attributes:");	/* ) */
		newline = 0;
		while (i < 2 + len) {
			int alen, aoff;

			TCHECK2(p[i], sizeof(bgpa));
			memcpy(&bgpa, &p[i], sizeof(bgpa));
			alen = bgp_attr_len(&bgpa);
			aoff = bgp_attr_off(&bgpa);

			if (vflag && newline)
				printf("\n\t\t");
			else
				printf(" ");
			printf("(");		/* ) */
			printf("%s", bgp_attr_type(bgpa.bgpa_type));
			if (bgpa.bgpa_flags) {
				printf("[%s%s%s%s",
					bgpa.bgpa_flags & 0x80 ? "O" : "",
					bgpa.bgpa_flags & 0x40 ? "T" : "",
					bgpa.bgpa_flags & 0x20 ? "P" : "",
					bgpa.bgpa_flags & 0x10 ? "E" : "");
				if (bgpa.bgpa_flags & 0xf)
					printf("+%x", bgpa.bgpa_flags & 0xf);
				printf("]");
			}

			if (!bgp_attr_print(&bgpa, &p[i + aoff], alen))
				goto trunc;
			newline = 1;

			/* ( */
			printf(")");	

			i += aoff + alen;
		}

		/* ( */
		printf(")");
	}
	p += 2 + len;

	if (len && dat + length > p)
		printf("\n\t\t");
	if (dat + length > p) {
		printf("(NLRI:");	/* ) */
		while (dat + length > p) {
			char buf[MAXHOSTNAMELEN + 100];
			i = decode_prefix4(p, buf, sizeof(buf));
			if (i == -1) {
				printf(" (illegal prefix length)");
				break;
			} else if (i == -2)
				goto trunc;
			printf(" %s", buf);
			p += i;
		}

		/* ( */
		printf(")");
	}
	return;
trunc:
	printf("[|BGP]");
}

static void
bgp_notification_print(const u_char *dat, int length)
{
	struct bgp_notification bgpn;
	u_int16_t af;
	u_int8_t safi;
	const u_char *p;

	TCHECK2(dat[0], BGP_NOTIFICATION_SIZE);
	memcpy(&bgpn, dat, BGP_NOTIFICATION_SIZE);

	/* sanity checking */
	if (length<BGP_NOTIFICATION_SIZE)
		return;

	printf(": error %s,", bgp_notify_major(bgpn.bgpn_major));
	printf(" subcode %s",
		bgp_notify_minor(bgpn.bgpn_major, bgpn.bgpn_minor));

	if (bgpn.bgpn_major == BGP_NOTIFY_MAJOR_CEASE) {
		/*
		 * RFC 4486: optional maxprefix subtype of 7 bytes
		 * may contain AFI, SAFI and MAXPREFIXES
		 */
		if(bgpn.bgpn_minor == BGP_NOTIFY_MINOR_CEASE_MAXPRFX && 
		    length >= BGP_NOTIFICATION_SIZE + 7) {

			p = dat + BGP_NOTIFICATION_SIZE;
			TCHECK2(*p, 7);

			af = EXTRACT_16BITS(p);
			safi = p[2];
			printf(" %s %s,", af_name(af),
			    bgp_attr_nlri_safi(safi));

			printf(" Max Prefixes: %u", EXTRACT_32BITS(p+3));
		}
	}

	return;
trunc:
	printf("[|BGP]");
}

static void
bgp_route_refresh_print(const u_char *dat, int length)
{
	const struct bgp_route_refresh *bgp_route_refresh_header;

	TCHECK2(dat[0], BGP_ROUTE_REFRESH_SIZE);

	/* sanity checking */
	if (length<BGP_ROUTE_REFRESH_SIZE)
		return;

	bgp_route_refresh_header = (const struct bgp_route_refresh *)dat;

	printf(" (%s %s)",
	    af_name(EXTRACT_16BITS(&bgp_route_refresh_header->afi)),
	    bgp_attr_nlri_safi(bgp_route_refresh_header->safi));

	return;
trunc:
	printf("[|BGP]");
}

static int
bgp_header_print(const u_char *dat, int length)
{
	struct bgp bgp;

	TCHECK2(dat[0], BGP_SIZE);
	memcpy(&bgp, dat, BGP_SIZE);
	printf("(%s", bgp_type(bgp.bgp_type));		/* ) */

	switch (bgp.bgp_type) {
	case BGP_OPEN:
		bgp_open_print(dat, length);
		break;
	case BGP_UPDATE:
		bgp_update_print(dat, length);
		break;
	case BGP_NOTIFICATION:
		bgp_notification_print(dat, length);
		break;
	case BGP_KEEPALIVE:
		break;
	case BGP_ROUTE_REFRESH:
		bgp_route_refresh_print(dat, length);
	default:
		TCHECK2(*dat, length);
		break;
	}

	/* ( */
	printf(")");
	return 1;
trunc:
	printf("[|BGP]");
	return 0;
}

void
bgp_print(const u_char *dat, int length)
{
	const u_char *p;
	const u_char *ep;
	const u_char *start;
	const u_char marker[] = {
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	};
	struct bgp bgp;
	u_int16_t hlen;
	int newline;

	ep = dat + length;
	if (snapend < dat + length)
		ep = snapend;

	printf(": BGP");

	p = dat;
	newline = 0;
	start = p;
	while (p < ep) {
		if (!TTEST2(p[0], 1))
			break;
		if (p[0] != 0xff) {
			p++;
			continue;
		}

		if (!TTEST2(p[0], sizeof(marker)))
			break;
		if (memcmp(p, marker, sizeof(marker)) != 0) {
			p++;
			continue;
		}

		/* found BGP header */
		TCHECK2(p[0], BGP_SIZE);	/*XXX*/
		memcpy(&bgp, p, BGP_SIZE);

		if (start != p)
			printf(" [|BGP]");

		hlen = ntohs(bgp.bgp_len);
		if (vflag && newline)
			printf("\n\t");
		else
			printf(" ");
		if (hlen < BGP_SIZE) {
			printf("\n[|BGP Bogus header length %u < %u]",
			    hlen, BGP_SIZE);
			break;
		}
		if (TTEST2(p[0], hlen)) {
			if (!bgp_header_print(p, hlen))
				return;
			newline = 1;
			p += hlen;
			start = p;
		} else {
			printf("[|BGP %s]", bgp_type(bgp.bgp_type));
			break;
		}
	}

	return;

trunc:
	printf(" [|BGP]");
}
