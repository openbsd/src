/*	$OpenBSD: nametoaddr.c,v 1.2 1996/08/22 00:33:40 deraadt Exp $	*/
/*	From NetBSD: nametoaddr.c,v 1.3 1995/04/29 05:42:23 cgd Exp */

/*
 * Copyright (c) 1990, 1991, 1992, 1993, 1994
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
 *
 * Name to id translation routines used by the scanner.
 * These functions are not time critical.
 */

#ifndef lint
#if 0
from: static char rcsid[] =
    "@(#) Header: nametoaddr.c,v 1.21 94/06/20 19:07:54 leres Exp (LBL)";
#else
static char rcsid[] = "$OpenBSD: nametoaddr.c,v 1.2 1996/08/22 00:33:40 deraadt Exp $";
#endif
#endif

#include <stdio.h>
#if define(__NetBSD__) || defined(__OpenBSD__)
#include <stdlib.h>
#include <string.h>
#endif
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/ppp_defs.h>

#include "bpf_compile.h"
#include "gencode.h"

#ifndef __GNUC__
#define inline
#endif

#ifndef NTOHL
#define NTOHL(x) (x) = ntohl(x)
#define NTOHS(x) (x) = ntohs(x)
#endif

/*
 *  Convert host name to internet address.
 *  Return 0 upon failure.
 */
u_long **
pcap_nametoaddr(const char *name)
{
#ifndef h_addr
	static u_long *hlist[2];
#endif
	u_long **p;
	struct hostent *hp;

	if ((hp = gethostbyname(name)) != NULL) {
#ifndef h_addr
		hlist[0] = (u_long *)hp->h_addr;
		NTOHL(hp->h_addr);
		return hlist;
#else
		for (p = (u_long **)hp->h_addr_list; *p; ++p)
			NTOHL(**p);
		return (u_long **)hp->h_addr_list;
#endif
	}
	else
		return 0;
}

/*
 *  Convert net name to internet address.
 *  Return 0 upon failure.
 */
u_long
pcap_nametonetaddr(const char *name)
{
	struct netent *np;

	if ((np = getnetbyname(name)) != NULL)
		return np->n_net;
	else
		return 0;
}

/*
 * Convert a port name to its port and protocol numbers.
 * We assume only TCP or UDP.
 * Return 0 upon failure.
 */
int
pcap_nametoport(const char *name, int *port, int *proto)
{
	struct servent *sp;
	char *other;

	sp = getservbyname(name, (char *)0);
	if (sp != NULL) {
		NTOHS(sp->s_port);
		*port = sp->s_port;
		*proto = pcap_nametoproto(sp->s_proto);
		/*
		 * We need to check /etc/services for ambiguous entries.
		 * If we find the ambiguous entry, and it has the
		 * same port number, change the proto to PROTO_UNDEF
		 * so both TCP and UDP will be checked.
		 */
		if (*proto == IPPROTO_TCP)
			other = "udp";
		else
			other = "tcp";

		sp = getservbyname(name, other);
		if (sp != 0) {
			NTOHS(sp->s_port);
			if (*port != sp->s_port)
				/* Can't handle ambiguous names that refer
				   to different port numbers. */
#ifdef notdef
				warning("ambiguous port %s in /etc/services",
					name);
#else
			;
#endif
			*proto = PROTO_UNDEF;
		}
		return 1;
	}
#if defined(ultrix) || defined(__osf__)
	/* Special hack in case NFS isn't in /etc/services */
	if (strcmp(name, "nfs") == 0) {
		*port = 2049;
		*proto = PROTO_UNDEF;
		return 1;
	}
#endif
	return 0;
}

int
pcap_nametoproto(const char *str)
{
	struct protoent *p;

	p = getprotobyname(str);
	if (p != 0)
		return p->p_proto;
	else
		return PROTO_UNDEF;
}

u_long
__pcap_atoin(const char *s)
{
	u_long addr = 0;
	u_int n;

	while (1) {
		n = 0;
		while (*s && *s != '.')
			n = n * 10 + *s++ - '0';
		addr <<= 8;
		addr |= n & 0xff;
		if (*s == '\0')
			return addr;
		++s;
	}
	/* NOTREACHED */
}

struct pppproto {
	char *s;
	u_short p;
};

/* Static data base of PPP protocol types. */
struct pppproto pppproto_db[] = {
	{ "ip", PPP_IP },
	{ (char *)0, 0 }
};

int
pcap_nametopppproto(const char *s)
{
	struct pppproto *p = pppproto_db;

	while (p->s != 0) {
		if (strcmp(p->s, s) == 0)
			return p->p;
		p += 1;
	}
	return PROTO_UNDEF;
}
