/*	$OpenBSD: print-netbios.c,v 1.1 1996/11/12 07:54:55 mickey Exp $	*/

/*
 * Copyright (c) 1994, 1995, 1996
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

/*
 * Format and print NETBIOS packets.
 * Contributed by Brad Parker (brad@fcr.com).
 */
#ifndef lint
static  char rcsid[] =
    "@(#)Header: print-netbios.c,v 1.5 96/06/03 02:53:36 leres Exp";
#endif

#ifdef __STDC__
#include <stdlib.h>
#endif
#include <stdio.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/tcp.h>
#include <netinet/tcpip.h>

#include <string.h>

#include "interface.h"
#include "addrtoname.h"
#include "netbios.h"
#include "extract.h"

/*
 * Print NETBIOS packets.
 */
void
netbios_print(const u_char *p, int length)
{
	struct p8022Hdr *nb = (struct p8022Hdr *)p;

	if (length < p8022Size) {
		(void)printf(" truncated-netbios %d", length);
		return;
	}

	if (nb->flags == UI) {
	    (void)printf("802.1 UI ");
	} else {
	    (void)printf("802.1 CONN ");
	}

	if ((u_char *)(nb + 1) > snapend) {
		printf(" [|netbios]");
		return;
	}

/*
	netbios_decode(nb, (u_char *)nb + p8022Size, length - p8022Size);
*/
}

#ifdef never
	(void)printf("%s.%d > ",
		     ipxaddr_string(EXTRACT_LONG(ipx->srcNet), ipx->srcNode),
		     EXTRACT_SHORT(ipx->srcSkt));

	(void)printf("%s.%d:",
		     ipxaddr_string(EXTRACT_LONG(ipx->dstNet), ipx->dstNode),
		     EXTRACT_SHORT(ipx->dstSkt));

	if ((u_char *)(ipx + 1) > snapend) {
		printf(" [|ipx]");
		return;
	}

	/* take length from ipx header */
	length = EXTRACT_SHORT(&ipx->length);

	ipx_decode(ipx, (u_char *)ipx + ipxSize, length - ipxSize);
#endif

