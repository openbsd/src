/*	$OpenBSD: print-pflog.c,v 1.11 2003/01/01 16:55:16 mcbride Exp $	*/

/*
 * Copyright (c) 1990, 1991, 1993, 1994, 1995, 1996
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
static const char rcsid[] =
    "@(#) $Header: /home/cvs/src/usr.sbin/tcpdump/print-pflog.c,v 1.11 2003/01/01 16:55:16 mcbride Exp $ (LBL)";
#endif

#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/mbuf.h>

struct rtentry;
#include <net/if.h>
#include <net/if_pflog.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>

#include <net/pfvar.h>

#include <ctype.h>
#include <netdb.h>
#include <pcap.h>
#include <signal.h>
#include <stdio.h>

#include "interface.h"
#include "addrtoname.h"

char *pf_reasons[PFRES_MAX+2] = PFRES_NAMES;

void
pflog_if_print(u_char *user, const struct pcap_pkthdr *h,
     register const u_char *p)
{
	u_int length = h->len;
	u_int caplen = h->caplen;
	const struct ip *ip;
	const struct ip6_hdr *ip6;
	const struct pfloghdr *hdr;
	u_short res;
	char reason[128], *why;
	u_int8_t af;

	ts_print(&h->ts);

	if (caplen < PFLOG_HDRLEN) {
		printf("[|pflog]");
		goto out;
	}

	/*
	 * Some printers want to get back at the link level addresses,
	 * and/or check that they're not walking off the end of the packet.
	 * Rather than pass them all the way down, we set these globals.
	 */
	packetp = p;
	snapend = p + caplen;

	hdr = (struct pfloghdr *)p;
	if (eflag) {
		res = ntohs(hdr->reason);
		why = (res < PFRES_MAX) ? pf_reasons[res] : "unkn";

		snprintf(reason, sizeof(reason), "%d(%s)", res, why);

		printf("rule %d/%s: ",
		    (short)ntohs(hdr->rnr), reason);
		switch (hdr->action) {
		case PF_SCRUB:
			printf("scrub");
			break;
		case PF_PASS:
			printf("pass");
			break;
		case PF_DROP:
			printf("block");
			break;
		case PF_NAT:
		case PF_NONAT:
			printf("nat");
			break;
		case PF_BINAT:
		case PF_NOBINAT:
			printf("binat");
			break;
		case PF_RDR:
		case PF_NORDR:
			printf("rdr");
			break;
		}
		printf(" %s on %s: ",
		    ntohs(hdr->dir) == PF_OUT ? "out" : "in",
		    hdr->ifname);
	}
	af = ntohl(hdr->af);
	length -= PFLOG_HDRLEN;
	if (af == AF_INET) {
		ip = (struct ip *)(p + PFLOG_HDRLEN);
		ip_print((const u_char *)ip, length);
		if (xflag)
			default_print((const u_char *)ip,
			    caplen - PFLOG_HDRLEN);
	} else {
		ip6 = (struct ip6_hdr *)(p + PFLOG_HDRLEN);
		ip6_print((const u_char *)ip6, length);
		if (xflag)
			default_print((const u_char *)ip6,
			    caplen - PFLOG_HDRLEN);
	}

out:
	putchar('\n');
}
