/*	$OpenBSD: print-icmp.c,v 1.4 1996/07/13 11:01:23 mickey Exp $	*/

/*
 * Copyright (c) 1988, 1989, 1990, 1991, 1993, 1994, 1995, 1996
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

#ifndef lint
static char rcsid[] =
    "@(#) Header: print-icmp.c,v 1.27 96/06/24 22:14:23 leres Exp (LBL)";
#endif

#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

#if __STDC__
struct mbuf;
struct rtentry;
#endif
#include <net/if.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/tcp.h>
#include <netinet/tcpip.h>

#include <stdio.h>

#include "interface.h"
#include "addrtoname.h"

/* Compatibility */
#ifndef ICMP_UNREACH_NET_UNKNOWN
#define ICMP_UNREACH_NET_UNKNOWN	6	/* destination net unknown */
#endif
#ifndef ICMP_UNREACH_HOST_UNKNOWN
#define ICMP_UNREACH_HOST_UNKNOWN	7	/* destination host unknown */
#endif
#ifndef ICMP_UNREACH_ISOLATED
#define ICMP_UNREACH_ISOLATED		8	/* source host isolated */
#endif
#ifndef ICMP_UNREACH_NET_PROHIB
#define ICMP_UNREACH_NET_PROHIB		9	/* admin prohibited net */
#endif
#ifndef ICMP_UNREACH_HOST_PROHIB
#define ICMP_UNREACH_HOST_PROHIB	10	/* admin prohibited host */
#endif
#ifndef ICMP_UNREACH_TOSNET
#define ICMP_UNREACH_TOSNET		11	/* tos prohibited net */
#endif
#ifndef ICMP_UNREACH_TOSHOST
#define ICMP_UNREACH_TOSHOST		12	/* tos prohibited host */
#endif

/* Most of the icmp types */
static struct tok icmp2str[] = {
	{ ICMP_ECHOREPLY,		"echo reply" },
	{ ICMP_SOURCEQUENCH,		"source quench" },
	{ ICMP_ECHO,			"echo request" },
	{ ICMP_TSTAMP,			"time stamp request" },
	{ ICMP_TSTAMPREPLY,		"time stamp reply" },
	{ ICMP_IREQ,			"information request" },
	{ ICMP_IREQREPLY,		"information reply" },
	{ ICMP_MASKREQ,			"address mask request" },
	{ 0,				NULL }
};

/* Formats for most of the ICMP_UNREACH codes */
static struct tok unreach2str[] = {
	{ ICMP_UNREACH_NET,		"net %s unreachable" },
	{ ICMP_UNREACH_HOST,		"host %s unreachable" },
	{ ICMP_UNREACH_NEEDFRAG,	"%s unreachable - need to frag" },
	{ ICMP_UNREACH_SRCFAIL,
	    "%s unreachable - source route failed" },
	{ ICMP_UNREACH_NET_UNKNOWN,	"net %s unreachable - unknown" },
	{ ICMP_UNREACH_HOST_UNKNOWN,	"host %s unreachable - unknown" },
	{ ICMP_UNREACH_ISOLATED,
	    "%s unreachable - source host isolated" },
	{ ICMP_UNREACH_NET_PROHIB,
	    "net %s unreachable - admin prohibited" },
	{ ICMP_UNREACH_HOST_PROHIB,
	    "host %s unreachable - admin prohibited" },
	{ ICMP_UNREACH_TOSNET,
	    "net %s unreachable - tos prohibited" },
	{ ICMP_UNREACH_TOSHOST,
	    "host %s unreachable - tos prohibited" },
	{ 0,				NULL }
};

/* Formats for the ICMP_REDIRECT codes */
static struct tok type2str[] = {
	{ ICMP_REDIRECT_NET,		"redirect %s to net %s" },
	{ ICMP_REDIRECT_HOST,		"redirect %s to host %s" },
	{ ICMP_REDIRECT_TOSNET,		"redirect-tos %s to net %s" },
	{ ICMP_REDIRECT_TOSHOST,	"redirect-tos %s to net %s" },
	{ 0,				NULL }
};

void
icmp_print(register const u_char *bp, register const u_char *bp2)
{
	register const struct icmp *dp;
	register const struct ip *ip;
	register const char *str, *fmt;
	register const struct ip *oip;
	register const struct udphdr *ouh;
	register int hlen, dport;
	char buf[256];

#define TCHECK(var, l) if ((u_char *)&(var) > snapend - l) goto trunc

	dp = (struct icmp *)bp;
	ip = (struct ip *)bp2;
	str = buf;

        (void)printf("%s > %s: ",
		ipaddr_string(&ip->ip_src),
		ipaddr_string(&ip->ip_dst));

	TCHECK(dp->icmp_code, sizeof(dp->icmp_code));
	switch (dp->icmp_type) {

	case ICMP_UNREACH:
		TCHECK(dp->icmp_ip.ip_dst, sizeof(dp->icmp_ip.ip_dst));
		switch (dp->icmp_code) {

		case ICMP_UNREACH_PROTOCOL:
			TCHECK(dp->icmp_ip.ip_p, sizeof(dp->icmp_ip.ip_p));
			(void)sprintf(buf, "%s protocol %d unreachable",
				       ipaddr_string(&dp->icmp_ip.ip_dst),
				       dp->icmp_ip.ip_p);
			break;

		case ICMP_UNREACH_PORT:
			TCHECK(dp->icmp_ip.ip_p, sizeof(dp->icmp_ip.ip_p));
			oip = &dp->icmp_ip;
			hlen = oip->ip_hl * 4;
			ouh = (struct udphdr *)(((u_char *)oip) + hlen);
			dport = ntohs(ouh->uh_dport);
			switch (oip->ip_p) {

			case IPPROTO_TCP:
				(void)sprintf(buf,
					"%s tcp port %s unreachable",
					ipaddr_string(&oip->ip_dst),
					tcpport_string(dport));
				break;

			case IPPROTO_UDP:
				(void)sprintf(buf,
					"%s udp port %s unreachable",
					ipaddr_string(&oip->ip_dst),
					udpport_string(dport));
				break;

			default:
				(void)sprintf(buf,
					"%s protocol %d port %d unreachable",
					ipaddr_string(&oip->ip_dst),
					oip->ip_p, dport);
				break;
			}
			break;

		default:
			fmt = tok2str(unreach2str, "#%d %%s unreachable",
			    dp->icmp_code);
			(void)sprintf(buf, fmt,
			    ipaddr_string(&dp->icmp_ip.ip_dst));
			break;
		}
		break;

	case ICMP_REDIRECT:
		TCHECK(dp->icmp_ip.ip_dst, sizeof(dp->icmp_ip.ip_dst));
		fmt = tok2str(type2str, "redirect-#%d %%s to net %%s",
		    dp->icmp_code);
		(void)sprintf(buf, fmt,
		    ipaddr_string(&dp->icmp_ip.ip_dst),
		    ipaddr_string(&dp->icmp_gwaddr));
		break;

	case ICMP_TIMXCEED:
		TCHECK(dp->icmp_ip.ip_dst, sizeof(dp->icmp_ip.ip_dst));
		switch (dp->icmp_code) {

		case ICMP_TIMXCEED_INTRANS:
			str = "time exceeded in-transit";
			break;

		case ICMP_TIMXCEED_REASS:
			str = "ip reassembly time exceeded";
			break;

		default:
			(void)sprintf(buf, "time exceeded-#%d", dp->icmp_code);
			break;
		}
		break;

	case ICMP_PARAMPROB:
		if (dp->icmp_code)
			(void)sprintf(buf, "parameter problem - code %d",
					dp->icmp_code);
		else {
			TCHECK(dp->icmp_pptr, sizeof(dp->icmp_pptr));
			(void)sprintf(buf, "parameter problem - octet %d",
					dp->icmp_pptr);
		}
		break;

	case ICMP_MASKREPLY:
		TCHECK(dp->icmp_mask, sizeof(dp->icmp_mask));
		(void)sprintf(buf, "address mask is 0x%08x",
		    (u_int32_t)ntohl(dp->icmp_mask));
		break;

	default:
		str = tok2str(icmp2str, "type-#%d", dp->icmp_type);
		break;
	}
        (void)printf("icmp: %s", str);
	return;
trunc:
	fputs("[|icmp]", stdout);
#undef TCHECK
}
