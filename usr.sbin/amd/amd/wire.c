/*
 * Copyright (c) 1990 Jan-Simon Pendry
 * Copyright (c) 1990 Imperial College of Science, Technology & Medicine
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry at Imperial College, London.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)wire.c	8.1 (Berkeley) 6/6/93
 *	$Id: wire.c,v 1.12 2003/06/17 18:00:24 millert Exp $
 */

/*
 * This function returns the subnet (address&netmask) for the primary network
 * interface.  If the resulting address has an entry in the hosts file, the
 * corresponding name is retuned, otherwise the address is returned in
 * standard internet format.
 * As a side-effect, a list of local IP/net address is recorded for use
 * by the islocalnet() function.
 *
 * Derived from original by Paul Anderson (23/4/90)
 * Updates from Dirk Grunwald (11/11/91)
 * Modified to use getifaddrs() by Todd C. Miller (6/14/2003)
 */

#include "am.h"

#include <ifaddrs.h>
#include <netdb.h>
#include <net/if.h>

#define NO_SUBNET "notknown"

/*
 * List of locally connected networks
 */
typedef struct addrlist addrlist;
struct addrlist {
	addrlist *ip_next;
	in_addr_t ip_addr;
	in_addr_t ip_mask;
};
static addrlist *localnets = 0;

char *
getwire(void)
{
	struct ifaddrs *ifa, *ifaddrs;
	struct hostent *hp;
	struct netent *np;
	addrlist *al;
	char *s, *netname = NULL;

	if (getifaddrs(&ifaddrs))
		return strdup(NO_SUBNET);

	for (ifa = ifaddrs; ifa != NULL; ifa = ifa -> ifa_next) {
		/*
		 * Ignore non-AF_INET interfaces as well as any that
		 * are down or loopback.
		 */
		if (ifa->ifa_addr == NULL ||
		    ifa->ifa_addr->sa_family != AF_INET ||
		    !(ifa->ifa_flags & IFF_UP) ||
		    (ifa->ifa_flags & IFF_LOOPBACK))
			continue;
		
		/*
		 * Add interface to local network list
		 */
		al = ALLOC(addrlist);
		al->ip_addr =
		    ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr.s_addr;
		al->ip_mask =
		    ((struct sockaddr_in *)ifa->ifa_netmask)->sin_addr.s_addr;
		al->ip_next = localnets;
		localnets = al;

		if (netname == NULL) {
			in_addr_t net;
			in_addr_t mask;
			in_addr_t subnet;
			in_addr_t subnetshift;
			char dq[20];

			/*
			 * Figure out the subnet's network address
			 */
			subnet = al->ip_addr & al->ip_mask;

#ifdef IN_CLASSA
			subnet = ntohl(subnet);

			if (IN_CLASSA(subnet)) {
				mask = IN_CLASSA_NET;
				subnetshift = 8;
			} else if (IN_CLASSB(subnet)) {
				mask = IN_CLASSB_NET;
				subnetshift = 8;
			} else {
				mask = IN_CLASSC_NET;
				subnetshift = 4;
			}

			/*
			 * If there are more bits than the standard mask
			 * would suggest, subnets must be in use.
			 * Guess at the subnet mask, assuming reasonable
			 * width subnet fields.
			 * XXX: Or-in at least 1 byte's worth of 1s to make
			 * sure the top bits remain set.
			 */
			while (subnet &~ mask)
				mask = (mask >> subnetshift) | 0xff000000;

			net = subnet & mask;
			while ((mask & 1) == 0)
				mask >>= 1, net >>= 1;

			/*
			 * Now get a usable name.
			 * First use the network database,
			 * then the host database,
			 * and finally just make a dotted quad.
			 */
			np = getnetbyaddr(net, AF_INET);
#else
			/* This is probably very wrong. */
			np = getnetbyaddr(subnet, AF_INET);
#endif /* IN_CLASSA */
			if (np)
				s = np->n_name;
			else {
				subnet = al->ip_addr & al->ip_mask;
				hp = gethostbyaddr((char *) &subnet, 4, AF_INET);
				if (hp)
					s = hp->h_name;
				else
					s = inet_dquad(dq, sizeof(dq), subnet);
			}
			netname = strdup(s);
		}
	}
	freeifaddrs(ifaddrs);
	return (netname ? netname : strdup(NO_SUBNET));
}

/*
 * Determine whether a network is on a local network
 * (addr) is in network byte order.
 */
int
islocalnet(in_addr_t addr)
{
	addrlist *al;

	for (al = localnets; al; al = al->ip_next)
		if (((addr ^ al->ip_addr) & al->ip_mask) == 0)
			return TRUE;

#ifdef DEBUG
	{ char buf[16];
	plog(XLOG_INFO, "%s is on a remote network", inet_dquad(buf, sizeof(buf), addr));
	}
#endif
	return FALSE;
}
