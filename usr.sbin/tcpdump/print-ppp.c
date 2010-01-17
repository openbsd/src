/*	$OpenBSD: print-ppp.c,v 1.25 2010/01/17 19:56:58 naddy Exp $	*/

/*
 * Copyright (c) 1990, 1991, 1993, 1994, 1995, 1996, 1997
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifdef PPP
#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/ioctl.h>

struct mbuf;
struct rtentry;
#include <net/if.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>

#include <ctype.h>
#include <netdb.h>
#include <pcap.h>
#include <signal.h>
#include <stdio.h>

#include <netinet/if_ether.h>
#include "ethertype.h"

#include <net/ppp_defs.h>
#include "interface.h"
#include "addrtoname.h"
#include "extract.h"

struct protonames {
	u_short protocol;
	char *name;
};

static struct protonames protonames[] = {
	/*
	 * Protocol field values.
	 */
	{ PPP_IP,	"IP" },		/* Internet Protocol */
	{ PPP_XNS,	"XNS" },	/* Xerox NS */
	{ PPP_IPX,	"IPX" },	/* IPX Datagram (RFC1552) */
	{ PPP_VJC_COMP,	"VJC_UNCOMP" },	/* VJ compressed TCP */
	{ PPP_VJC_UNCOMP,"VJC_UNCOMP" },/* VJ uncompressed TCP */
	{ PPP_COMP,	"COMP" },	/* compressed packet */
	{ PPP_IPCP,	"IPCP" },	/* IP Control Protocol */
	{ PPP_IPXCP,	"IPXCP" },	/* IPX Control Protocol (RFC1552) */
	{ PPP_IPV6CP,	"IPV6CP" },	/* IPv6 Control Protocol */
	{ PPP_CCP,	"CCP" },	/* Compression Control Protocol */
	{ PPP_LCP,	"LCP" },	/* Link Control Protocol */
	{ PPP_PAP,	"PAP" },	/* Password Authentication Protocol */
	{ PPP_LQR,	"LQR" },	/* Link Quality Report protocol */
	{ PPP_CHAP,	"CHAP" },	/* Cryptographic Handshake Auth. Proto */
	{ PPP_IPV6,	"IPV6" },	/* Internet Protocol v6 */
};

/* LCP */

#define LCP_CONF_REQ	1
#define LCP_CONF_ACK	2
#define LCP_CONF_NAK	3
#define LCP_CONF_REJ	4
#define LCP_TERM_REQ	5
#define LCP_TERM_ACK	6
#define LCP_CODE_REJ	7
#define LCP_PROT_REJ	8
#define LCP_ECHO_REQ	9
#define LCP_ECHO_RPL	10
#define LCP_DISC_REQ	11

#define LCP_MIN	LCP_CONF_REQ
#define LCP_MAX LCP_DISC_REQ

static char *lcpcodes[] = {
	/*
	 * LCP code values (RFC1661, pp26)
	 */
	"Configure-Request",
	"Configure-Ack",
	"Configure-Nak",
	"Configure-Reject",
	"Terminate-Request",
	"Terminate-Ack",
 	"Code-Reject",
	"Protocol-Reject",
	"Echo-Request",
	"Echo-Reply",
	"Discard-Request",
};

#define LCPOPT_VEXT	0
#define LCPOPT_MRU	1
#define LCPOPT_ACCM	2
#define LCPOPT_AP	3
#define LCPOPT_QP	4
#define LCPOPT_MN	5
#define LCPOPT_PFC	7
#define LCPOPT_ACFC	8

#define LCPOPT_MIN 0
#define LCPOPT_MAX 24

static char *lcpconfopts[] = {
	"Vendor-Ext",
	"Max-Rx-Unit",
	"Async-Ctrl-Char-Map",
	"Auth-Prot",
	"Quality-Prot",
	"Magic-Number",
	"unassigned (6)",	
	"Prot-Field-Compr",
	"Add-Ctrl-Field-Compr",
	"FCS-Alternatives",
	"Self-Describing-Pad",
	"Numbered-Mode",
	"Multi-Link-Procedure",
	"Call-Back",
	"Connect-Time"
	"Compund-Frames",
	"Nominal-Data-Encap",
	"Multilink-MRRU",
	"Multilink-SSNHF",
	"Multilink-ED",
	"Proprietary",
	"DCE-Identifier",
	"Multilink-Plus-Proc",
	"Link-Discriminator",
	"LCP-Auth-Option",
};

/* CHAP */

#define CHAP_CHAL	1
#define CHAP_RESP	2
#define CHAP_SUCC	3
#define CHAP_FAIL	4

#define CHAP_CODEMIN 1
#define CHAP_CODEMAX 4

static char *chapcode[] = {
	"Challenge",
	"Response",
	"Success",
	"Failure",	
};

/* PAP */

#define PAP_AREQ	1
#define PAP_AACK	2
#define PAP_ANAK	3

#define PAP_CODEMIN	1
#define PAP_CODEMAX	3

static char *papcode[] = {
	"Authenticate-Request",
	"Authenticate-Ack",
	"Authenticate-Nak",
};

/* IPCP */

#define IPCP_CODE_CFG_REQ	1
#define IPCP_CODE_CFG_ACK	2
#define IPCP_CODE_CFG_NAK	3
#define IPCP_CODE_CFG_REJ	4
#define IPCP_CODE_TRM_REQ	5
#define IPCP_CODE_TRM_ACK	6
#define IPCP_CODE_COD_REJ	7

#define IPCP_CODE_MIN IPCP_CODE_CFG_REQ
#define IPCP_CODE_MAX IPCP_CODE_COD_REJ

#define IPCP_2ADDR	1
#define IPCP_CP		2
#define IPCP_ADDR	3

/* IPV6CP */

#define IPV6CP_CODE_CFG_REQ	1
#define IPV6CP_CODE_CFG_ACK	2
#define IPV6CP_CODE_CFG_NAK	3
#define IPV6CP_CODE_CFG_REJ	4
#define IPV6CP_CODE_TRM_REQ	5
#define IPV6CP_CODE_TRM_ACK	6
#define IPV6CP_CODE_COD_REJ	7

#define IPV6CP_CODE_MIN IPV6CP_CODE_CFG_REQ
#define IPV6CP_CODE_MAX IPV6CP_CODE_COD_REJ

#define IPV6CP_IFID	1

static int print_lcp_config_options(u_char *p);
static void handle_lcp(const u_char *p, int length);
static void handle_chap(const u_char *p, int length);
static void handle_ipcp(const u_char *p, int length);
static void handle_ipv6cp(const u_char *p, int length);
static void handle_pap(const u_char *p, int length);

struct pppoe_header {
	u_int8_t vertype;	/* PPPoE version/type */
	u_int8_t code;		/* PPPoE code (packet type) */
	u_int16_t sessionid;	/* PPPoE session id */
	u_int16_t len;		/* PPPoE payload length */
};
#define	PPPOE_CODE_SESSION	0x00	/* Session */
#define	PPPOE_CODE_PADO		0x07	/* Active Discovery Offer */
#define	PPPOE_CODE_PADI		0x09	/* Active Discovery Initiation */
#define	PPPOE_CODE_PADR		0x19	/* Active Discovery Request */
#define	PPPOE_CODE_PADS		0x65	/* Active Discovery Session-Confirm */
#define	PPPOE_CODE_PADT		0xa7	/* Active Discovery Terminate */
#define	PPPOE_TAG_END_OF_LIST		0x0000	/* End Of List */
#define	PPPOE_TAG_SERVICE_NAME		0x0101	/* Service Name */
#define	PPPOE_TAG_AC_NAME		0x0102	/* Access Concentrator Name */
#define	PPPOE_TAG_HOST_UNIQ		0x0103	/* Host Uniq */
#define	PPPOE_TAG_AC_COOKIE		0x0104	/* Access Concentratr Cookie */
#define	PPPOE_TAG_VENDOR_SPEC		0x0105	/* Vendor Specific */
#define	PPPOE_TAG_RELAY_SESSION		0x0110	/* Relay Session Id */
#define	PPPOE_TAG_SERVICE_NAME_ERROR	0x0201	/* Service Name Error */
#define	PPPOE_TAG_AC_SYSTEM_ERROR	0x0202	/* Acc. Concentrator Error */
#define	PPPOE_TAG_GENERIC_ERROR		0x0203	/* Generic Error */

void
ppp_hdlc_print(p, length)
	const u_char *p;
	int length;
{
	int proto = PPP_PROTOCOL(p);
	int i;

	for (i = sizeof(protonames) / sizeof(protonames[0]) - 1; i >= 0; i--) {
		if (proto == protonames[i].protocol) {
			printf("%s: ", protonames[i].name);

			switch(proto) {

			case PPP_LCP:
				handle_lcp(p, length);
				break;
			case PPP_CHAP:
				handle_chap(p, length);
				break;
			case PPP_PAP:
				handle_pap(p, length);
				break;
			case PPP_IPCP:
				handle_ipcp(p, length);
				break;
			case PPP_IPV6CP:
				handle_ipv6cp(p, length);
				break;
			}
			break;
		}
	}
	if (i < 0)
		printf("%04x: ", proto);
}

/* print LCP frame */

static void
handle_lcp(p, length)
	const u_char *p;
	int length;
{
	int x, j;
	u_char *ptr;

	TCHECK(*(p + 4));
	x = *(p + 4);

	if ((x >= LCP_MIN) && (x <= LCP_MAX))
		printf("%s", lcpcodes[x-1]);
	else {
		printf("0x%02x", x);
		return;
	}

	length -= 4;
	
	switch(x) {

	case LCP_CONF_REQ:
	case LCP_CONF_ACK:
	case LCP_CONF_NAK:
	case LCP_CONF_REJ:
		x = length;
		ptr = (u_char *)p+8;
		do {
			if((j = print_lcp_config_options(ptr)) == 0)
				break;
			x -= j;
			ptr += j;
		}
		while(x > 0);
		break;

	case LCP_ECHO_REQ:
	case LCP_ECHO_RPL:
		TCHECK2(*(p + 8), 4);
		printf(", Magic-Number=%d", ((*(p+ 8) << 24) + (*(p+9) << 16) +
					     (*(p+10) <<  8) + (*(p+11))));
		break;
	case LCP_TERM_REQ:
	case LCP_TERM_ACK:
	case LCP_CODE_REJ:
	case LCP_PROT_REJ:
	case LCP_DISC_REQ:
	default:
		break;
	}
	return;

trunc:
	printf("[|lcp]");
}

/* LCP config options */

static int
print_lcp_config_options(p)
	u_char *p;
{
	int len, opt;

	TCHECK2(*p, 2);
	len = *(p+1);
	opt = *p;

	if((opt >= LCPOPT_MIN) && (opt <= LCPOPT_MAX))
		printf(", %s", lcpconfopts[opt]);

	switch(opt) {
	case LCPOPT_MRU:
		if(len == 4) {
			TCHECK2(*(p + 2), 2);
			printf("=%d", (*(p+2) << 8) + *(p+3));
		}
		break;
	case LCPOPT_AP:
		if(len >= 4) {
			TCHECK2(*(p + 2), 2);
			if(*(p+2) == 0xc0 && *(p+3) == 0x23)
				printf(" PAP");
			else if(*(p+2) == 0xc2 && *(p+3) == 0x23) {
				printf(" CHAP/");
				TCHECK(*(p+4));
				switch(*(p+4)) {
				default:
					printf("unknown-algorithm-%d", *(p+4));
					break;
				case 5:
					printf("MD5");
					break;
				case 0x80:
					printf("Microsoft");
					break;
				}
			} else if(*(p+2) == 0xc2 && *(p+3) == 0x27)
					printf(" EAP");
			else if(*(p+2) == 0xc0 && *(p+3) == 0x27)
				printf(" SPAP");
			else if(*(p+2) == 0xc1 && *(p+3) == 0x23)
				printf(" Old-SPAP");
			else
				printf("unknown");
		}
		break;
	case LCPOPT_QP:
		if(len >= 4) {
			TCHECK2(*(p + 2), 2);
			if(*(p+2) == 0xc0 && *(p+3) == 0x25)
				printf(" LQR");
			else
				printf(" unknown");
		}
		break;
	case LCPOPT_MN:
		if(len == 6) {
			TCHECK2(*(p + 2), 4);
			printf("=%d", ((*(p+2) << 24) + (*(p+3) << 16) +
				       (*(p+4) <<  8) + (*(p+5))));
		}
		break;
	case LCPOPT_PFC:
		printf(" PFC");
		break;
	case LCPOPT_ACFC:
		printf(" ACFC");
		break;
	}
	return(len);

trunc:
	printf("[|lcp]");
	return 0;
}

/* CHAP */

static void
handle_chap(p, length)
	const u_char *p;
	int length;
{
	int x;
	u_char *ptr;

	TCHECK(*(p+4));
	x = *(p+4);

	if((x >= CHAP_CODEMIN) && (x <= CHAP_CODEMAX))
		printf("%s", chapcode[x-1]);
	else {
		printf("0x%02x", x);
		return;
	}

	length -= 4;
	
	switch(x) {
	case CHAP_CHAL:
	case CHAP_RESP:
		printf(", Value=");
		TCHECK(*(p+8));
		x = *(p+8);	/* value size */
		ptr = (u_char *)p+9;
		while(--x >= 0) {
			TCHECK(*ptr);
			printf("%02x", *ptr++);
		}
		x = length - *(p+8) - 1;
		printf(", Name=");
		while(--x >= 0) {
			TCHECK(*ptr);
			safeputchar(*ptr++);
		}
		break;
	}
	return;

trunc:
	printf("[|chap]");
}

/* PAP */

static void
handle_pap(p, length)
	const u_char *p;
	int length;
{
	int x;
	u_char *ptr;

	TCHECK(*(p+4));
	x = *(p+4);

	if((x >= PAP_CODEMIN) && (x <= PAP_CODEMAX))
		printf("%s", papcode[x-1]);
	else {
		printf("0x%02x", x);
		return;
	}

	length -= 4;

	switch(x) {
	case PAP_AREQ:
		printf(", Peer-Id=");
		TCHECK(*(p+8));
		x = *(p+8);	/* peerid size */
		ptr = (u_char *)p+9;
		while(--x >= 0) {
			TCHECK(*ptr);
			safeputchar(*ptr++);
		}
		TCHECK(*ptr);
		x = *ptr++;
		printf(", Passwd=");
		while(--x >= 0) {
			TCHECK(*ptr);
			safeputchar(*ptr++);
		}
		break;
	case PAP_AACK:
	case PAP_ANAK:		
		break;			
	}
	return;

trunc:
	printf("[|pap]");
}

/* IPCP */

static void
handle_ipcp(p, length)
	const u_char *p;
	int length;
{
	int x;

	TCHECK(*(p+4));
	x = *(p+4);

	if((x >= IPCP_CODE_MIN) && (x <= IPCP_CODE_MAX))
		printf("%s", lcpcodes[x-1]);	/* share table with LCP */
	else {
		printf("0x%02x", x);
		return;
	}

	length -= 4;

	TCHECK(*(p+8));	
	switch(*(p+8)) {
	case IPCP_2ADDR:
		printf(", IP-Addresses");
		TCHECK2(*(p+10), 8);
		printf(", Src=%d.%d.%d.%d",
		       *(p+10), *(p+11), *(p+12), *(p+13));
		printf(", Dst=%d.%d.%d.%d",
		       *(p+14), *(p+15), *(p+16), *(p+17));
		break;

	case IPCP_CP:
		printf(", IP-Compression-Protocol");
		break;

	case IPCP_ADDR:
		TCHECK2(*(p+10), 4);
		printf(", IP-Address=%d.%d.%d.%d",
		       *(p+10), *(p+11), *(p+12), *(p+13));
		break;
	default:
		printf(", Unknown IPCP code 0x%x", *(p+8));
		break;
	}
	return;

trunc:
	printf("[|ipcp]");
}

/* IPV6CP */

static void
handle_ipv6cp(p, length)
	const u_char *p;
	int length;
{
	int x;

	TCHECK(*(p+4));
	x = *(p+4);

	if((x >= IPV6CP_CODE_MIN) && (x <= IPV6CP_CODE_MAX))
		printf("%s", lcpcodes[x-1]);    /* share table with LCP */
	else {
		printf("0x%02x", x);
		return;
	}

	TCHECK(*(p+8));
	switch(*(p+8)) {
	case IPV6CP_IFID:
		TCHECK2(*(p + 10), 8);
		printf(", Interface-ID=%04x:%04x:%04x:%04x",
			EXTRACT_16BITS(p + 10),
			EXTRACT_16BITS(p + 12),
			EXTRACT_16BITS(p + 14),
			EXTRACT_16BITS(p + 16));
		break;

	default:
		printf(", Unknown IPV6CP code 0x%x", *(p+8));
		break;
	}
	return;

trunc:
	printf("[|ipv6cp]");
}

void
ppp_if_print(user, h, p)
	u_char *user;
	const struct pcap_pkthdr *h;
	register const u_char *p;
{
	register u_int length = h->len;
	register u_int caplen = h->caplen;

	ts_print(&h->ts);

	if (caplen < PPP_HDRLEN) {
		printf("[|ppp]");
		goto out;
	}

	/*
	 * Some printers want to get back at the link level addresses,
	 * and/or check that they're not walking off the end of the packet.
	 * Rather than pass them all the way down, we set these globals.
	 */
	packetp = p;
	snapend = p + caplen;

	if (eflag)
		ppp_hdlc_print(p, length);

	length -= PPP_HDRLEN;

	switch(PPP_PROTOCOL(p)) {
	case PPP_IP:
	case ETHERTYPE_IP:
		ip_print((const u_char *)(p + PPP_HDRLEN), length);
		break;
	case PPP_IPV6:
	case ETHERTYPE_IPV6:
		ip6_print((const u_char *)(p + PPP_HDRLEN), length);
		break;
	case PPP_IPX:
	case ETHERTYPE_IPX:
		ipx_print((const u_char *)(p + PPP_HDRLEN), length);
		break;

#ifndef	PPP_MPLS
#define	PPP_MPLS	0x0281
#endif
	case PPP_MPLS:
		mpls_print((const u_char *)(p + PPP_HDRLEN), length);
		break;

	default:
		if(!eflag)
			ppp_hdlc_print(p, length);
		if(!xflag)
			default_print((const u_char *)(p + PPP_HDRLEN),
				      caplen - PPP_HDRLEN);
	}

	if (xflag)
		default_print((const u_char *)(p + PPP_HDRLEN),
			      caplen - PPP_HDRLEN);
out:
	putchar('\n');
}

void
ppp_ether_if_print(user, h, p)
	u_char *user;
	const struct pcap_pkthdr *h;
	register const u_char *p;
{
	u_int16_t pppoe_sid, pppoe_len;
	u_int caplen = h->caplen;
	u_int16_t length = h->len;
	u_int16_t proto;
	int i;

	ts_print(&h->ts);

	packetp = p;
	snapend = p + caplen;

	if (eflag)
		printf("PPPoE ");

	if (caplen < sizeof(struct pppoe_header)) {
		printf("[|pppoe]");
		return;
	}

	if(eflag)
	{
		printf("\n\tcode ");
		switch (p[1]) {
		case PPPOE_CODE_PADI:
			printf("Initiation");
			break;
		case PPPOE_CODE_PADO:
			printf("Offer");
			break;
		case PPPOE_CODE_PADR:
			printf("Request");
			break;
		case PPPOE_CODE_PADS:
			printf("Confirm");
			break;
		case PPPOE_CODE_PADT:
			printf("Terminate");
			break;
		case PPPOE_CODE_SESSION:
			printf("Session");
			break;
		default:
			printf("Unknown(0x%02x)", p[1]);
			break;
		}
	}

	pppoe_sid = EXTRACT_16BITS(p + 2);
	pppoe_len = EXTRACT_16BITS(p + 4);

	if(eflag)
	    printf(", version %d, type %d, id 0x%04x, length %d",
		    (p[0] & 0xf), (p[0] & 0xf0) >> 4, pppoe_sid, pppoe_len);

	length -= sizeof(struct pppoe_header);
	caplen -= sizeof(struct pppoe_header);
	p += sizeof(struct pppoe_header);

	if (pppoe_len > caplen)
		pppoe_len = caplen;

	if (pppoe_len < 2) {
		printf("[|pppoe]");
		return;
	}
	proto = EXTRACT_16BITS(p);

	for (i = sizeof(protonames)/sizeof(protonames[0]) - 1; i >= 0; i--) {
		if (proto == protonames[i].protocol) {
			if (eflag)
				printf("\n\t%s: ", protonames[i].name);
			switch (proto) {
			case PPP_LCP:
				handle_lcp(p - 2, length + 2);
				break;
			case PPP_CHAP:
				handle_chap(p - 2, length + 2);
				break;
			case PPP_PAP:
				handle_pap(p - 2, length + 2);
				break;
			case PPP_IPCP:
				handle_ipcp(p - 2, length + 2);
				break;
			case PPP_IPV6CP:
				handle_ipv6cp(p - 2, length + 2);
				break;
			case PPP_IP:
				ip_print(p + 2, length - 2);
				break;
			case PPP_IPV6:
				ip6_print(p + 2, length - 2);
				break;
			case PPP_IPX:
				ipx_print(p + 2, length - 2);
			}
			break;
		}
	}
	if (i < 0)
		printf("\n\t%04x: ", proto);

	if (xflag)
	    default_print(p + 2, caplen - 2);
	putchar('\n');
}

int
pppoe_if_print(ethertype, p, length, caplen)
	u_short ethertype;
	const u_char *p;
	u_int length, caplen;
{
	u_int16_t pppoe_sid, pppoe_len;

	if (ethertype == ETHERTYPE_PPPOEDISC)
		printf("PPPoE-Discovery");
	else
		printf("PPPoE-Session");

	if (caplen < sizeof(struct pppoe_header)) {
		printf("[|pppoe]");
		return (1);
	}

	printf("\n\tcode ");
	switch (p[1]) {
	case PPPOE_CODE_PADI:
		printf("Initiation");
		break;
	case PPPOE_CODE_PADO:
		printf("Offer");
		break;
	case PPPOE_CODE_PADR:
		printf("Request");
		break;
	case PPPOE_CODE_PADS:
		printf("Confirm");
		break;
	case PPPOE_CODE_PADT:
		printf("Terminate");
		break;
	case PPPOE_CODE_SESSION:
		printf("Session");
		break;
	default:
		printf("Unknown(0x%02x)", p[1]);
		break;
	}

	pppoe_sid = EXTRACT_16BITS(p + 2);
	pppoe_len = EXTRACT_16BITS(p + 4);
	printf(", version %d, type %d, id 0x%04x, length %d",
	    (p[0] & 0xf), (p[0] & 0xf0) >> 4, pppoe_sid, pppoe_len);

	length -= sizeof(struct pppoe_header);
	caplen -= sizeof(struct pppoe_header);
	p += sizeof(struct pppoe_header);

	if (pppoe_len > caplen)
		pppoe_len = caplen;

	if (ethertype == ETHERTYPE_PPPOEDISC) {
		while (pppoe_len > 0) {
			u_int16_t t_type, t_len;

			if (pppoe_len < 4) {
				printf("\n\t[|pppoe]");
				break;
			}
			t_type = EXTRACT_16BITS(p);
			t_len = EXTRACT_16BITS(p + 2);

			pppoe_len -= 4;
			p += 4;

			if (pppoe_len < t_len) {
				printf("\n\t[|pppoe]");
				break;
			}

			printf("\n\ttag ");
			switch (t_type) {
			case PPPOE_TAG_END_OF_LIST:
				printf("End-Of-List");
				break;
			case PPPOE_TAG_SERVICE_NAME:
				printf("Service-Name");
				break;
			case PPPOE_TAG_AC_NAME:
				printf("AC-Name");
				break;
			case PPPOE_TAG_HOST_UNIQ:
				printf("Host-Uniq");
				break;
			case PPPOE_TAG_AC_COOKIE:
				printf("AC-Cookie");
				break;
			case PPPOE_TAG_VENDOR_SPEC:
				printf("Vendor-Specific");
				break;
			case PPPOE_TAG_RELAY_SESSION:
				printf("Relay-Session");
				break;
			case PPPOE_TAG_SERVICE_NAME_ERROR:
				printf("Service-Name-Error");
				break;
			case PPPOE_TAG_AC_SYSTEM_ERROR:
				printf("AC-System-Error");
				break;
			case PPPOE_TAG_GENERIC_ERROR:
				printf("Generic-Error");
				break;
			default:
				printf("Unknown(0x%04x)", t_type);
			}
			printf(", length %u%s", t_len, t_len ? " " : "");

			if (t_len) {
				for (t_type = 0; t_type < t_len; t_type++) {
					if (isprint(p[t_type]))
						printf("%c", p[t_type]);
					else
						printf("\\%03o", p[t_type]);
				}
			}
			pppoe_len -= t_len;
			p += t_len;
		}
	}
	else if (ethertype == ETHERTYPE_PPPOE) {
		u_int16_t proto;
		int i;

		if (pppoe_len < 2) {
			printf("[|pppoe]");
			return (1);
		}
		proto = EXTRACT_16BITS(p);

		for (i = sizeof(protonames)/sizeof(protonames[0]) - 1; i >= 0;
		     i--) {
			if (proto == protonames[i].protocol) {
				printf("\n\t%s: ", protonames[i].name);
				switch (proto) {
				case PPP_LCP:
					handle_lcp(p - 2, length + 2);
					break;
				case PPP_CHAP:
					handle_chap(p - 2, length + 2);
					break;
				case PPP_PAP:
					handle_pap(p - 2, length + 2);
					break;
				case PPP_IPCP:
					handle_ipcp(p - 2, length + 2);
					break;
				case PPP_IPV6CP:
					handle_ipv6cp(p - 2, length + 2);
					break;
				case PPP_IP:
					ip_print(p + 2, length - 2);
					break;
				case PPP_IPV6:
					ip6_print(p + 2, length - 2);
					break;
				case PPP_IPX:
					ipx_print(p + 2, length - 2);
				}
				break;
			}
		}
		if (i < 0)
			printf("\n\t%04x: ", proto);
	}

	return (1);
}

#else

#include <sys/types.h>
#include <sys/time.h>

#include <stdio.h>

#include "interface.h"
void
ppp_if_print(user, h, p)
	u_char *user;
	const struct pcap_pkthdr *h;
	const u_char *p;
{
	error("not configured for ppp");
	/* NOTREACHED */
}
#endif
