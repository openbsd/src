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
 * Format and print ipsec (esp/ah) packets.
 *      By Tero Kivinen <kivinen@ssh.fi>, Tero Mononen <tmo@ssh.fi>,  
 *         Tatu Ylonen <ylo@ssh.fi> and Timo J. Rinne <tri@ssh.fi>
 *         in co-operation with SSH Communications Security, Espoo, Finland    
 */

#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /home/cvs/src/usr.sbin/tcpdump/print-ipsec.c,v 1.1 1999/07/28 20:41:36 jakob Exp $ (XXX)";
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

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "addrtoname.h"
#include "interface.h"
#include "extract.h"		    /* must come after interface.h */

/*
 * IPSec/ESP header
 */
struct esp_hdr {
	u_int esp_spi;
	u_int esp_seq;
};

void esp_print(register const u_char *bp, register u_int len,
	       register const u_char *bp2)
{
	const struct ip *ip;
	const struct esp_hdr *esp;

	ip = (const struct ip *)bp2;
	esp = (const struct esp_hdr *)bp;

	(void)printf("esp %s > %s spi 0x%08X seq %d",
		     ipaddr_string(&ip->ip_src),
		     ipaddr_string(&ip->ip_dst),
		     ntohl(esp->esp_spi), ntohl(esp->esp_seq));

}

/*
 * IPSec/AH header
 */
struct ah_hdr {
	u_int ah_dummy;
	u_int ah_spi;
	u_int ah_seq;
};

ah_print(register const u_char *bp, register u_int len,
	 register const u_char *bp2)
{
	const struct ip *ip;
	const struct ah_hdr *ah;

	ip = (const struct ip *)bp2;
	ah = (const struct ah_hdr *)bp;

	(void)printf("ah %s > %s spi 0x%08X seq %d",
		     ipaddr_string(&ip->ip_src),
		     ipaddr_string(&ip->ip_dst),
		     ntohl(ah->ah_spi), ntohl(ah->ah_seq));

}
