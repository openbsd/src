/*	$OpenBSD: print-ipsec.c,v 1.7 2003/02/20 23:39:20 jason Exp $	*/

/*
 * Copyright (c) 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999
 *      The Regents of the University of California.  All rights reserved.
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
 *
 * Format and print IPsec (ESP/AH) packets.
 *      By Tero Kivinen <kivinen@ssh.fi>, Tero Mononen <tmo@ssh.fi>,  
 *         Tatu Ylonen <ylo@ssh.fi> and Timo J. Rinne <tri@ssh.fi>
 *         in co-operation with SSH Communications Security, Espoo, Finland    
 */

#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /home/cvs/src/usr.sbin/tcpdump/print-ipsec.c,v 1.7 2003/02/20 23:39:20 jason Exp $ (XXX)";
#endif

#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/tcp.h>
#include <netinet/tcpip.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "addrtoname.h"
#include "interface.h"
#include "extract.h"		    /* must come after interface.h */

/*
 * IPsec/ESP header
 */
struct esp_hdr {
	u_int esp_spi;
	u_int esp_seq;
};

void 
esp_print (register const u_char *bp, register u_int len,
	   register const u_char *bp2)
{
	const struct ip *ip;
	const struct esp_hdr *esp;
	u_int plen = len;
 
	ip = (const struct ip *)bp2;

	printf("esp %s > %s",
	    ipaddr_string(&ip->ip_src), ipaddr_string(&ip->ip_dst));

	if (plen < sizeof(struct esp_hdr)) {
		printf("[|esp]");
		return;
	}
	esp = (const struct esp_hdr *)bp;

	printf(" spi 0x%08X seq %d len %d",
	    ntohl(esp->esp_spi), ntohl(esp->esp_seq), len);
}

/*
 * IPsec/AH header
 */
struct ah_hdr {
	u_char  ah_nxt_hdr;
	u_char  ah_pl_len;
	u_short ah_reserved;
	u_int   ah_spi;
	u_int   ah_seq;
};

void
ah_print (register const u_char *bp, register u_int len,
	  register const u_char *bp2)
{
	const struct ip *ip;
	const struct ah_hdr *ah;
	u_int pl_len = len;

	ip = (const struct ip *)bp2;

	printf("ah %s > %s",
	    ipaddr_string(&ip->ip_src), ipaddr_string(&ip->ip_dst));

	if (pl_len < sizeof(struct ah_hdr)) {
		printf("[|esp]");
		return;
	}
	ah = (const struct ah_hdr *)bp;

	printf(" spi 0x%08X seq %d len %d",
	    ntohl(ah->ah_spi), ntohl(ah->ah_seq), len);

	if (vflag) {
	        (void)printf("\n\t[ ");

	        pl_len = (ah->ah_pl_len + 2) << 2; /* RFC2402, sec 2.2 */

		if (len - pl_len <= 0) {
		        (void)printf("truncated");
			goto out;
		}
		
		switch (ah->ah_nxt_hdr) { 

		case IPPROTO_IPIP: /* Tunnel Mode, IP-in-IP */
		        ip_print(bp + pl_len, len - pl_len); 
			break;

	        case IPPROTO_ICMP: /* From here and down; Transport mode */
		        icmp_print(bp + pl_len, (const u_char *) ip);
			break;

	        case IPPROTO_TCP:
		        tcp_print(bp + pl_len, len - pl_len, 
				  (const u_char *) ip);
			break;

	        case IPPROTO_UDP:
		        udp_print(bp + pl_len, len - pl_len, 
				  (const u_char *) ip);
			break;

		case IPPROTO_ESP:
		        esp_print(bp + pl_len, len - pl_len, 
				  (const u_char *) ip);
			break;

		case IPPROTO_AH:
		        ah_print(bp + pl_len, len - pl_len, 
				 (const u_char *) ip);
			break;

		default:
		        (void)printf("ip-proto-%d len %d", ah->ah_nxt_hdr,
				     len - pl_len);
		}
out:
		(void)printf(" ]");
	}

}

struct ipcomp_hdr {
	u_char  ipcomp_nxt_hdr;
	u_char	ipcomp_flags;
	u_short	ipcomp_cpi;
};

void
ipcomp_print (register const u_char *bp, register u_int len,
	  register const u_char *bp2)
{
	const struct ip *ip;
	const struct ipcomp_hdr *ipc;
	u_int plen = len;
 
	ip = (const struct ip *)bp2;

	printf("ipcomp %s > %s",
	    ipaddr_string(&ip->ip_src), ipaddr_string(&ip->ip_dst));

	if (plen < sizeof(struct ipcomp_hdr)) {
		printf("[|ipcomp]");
		return;
	}
	ipc = (const struct ipcomp_hdr *)bp;

	printf(" cpi 0x%04X flags %x next %x",
	    ntohs(ipc->ipcomp_cpi), ipc->ipcomp_flags, ipc->ipcomp_nxt_hdr);
}
