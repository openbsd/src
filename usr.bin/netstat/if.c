/*	$OpenBSD: if.c,v 1.20 2000/02/05 18:46:50 itojun Exp $	*/
/*	$NetBSD: if.c,v 1.16.4.2 1996/06/07 21:46:46 thorpej Exp $	*/

/*
 * Copyright (c) 1983, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 */

#ifndef lint
#if 0
static char sccsid[] = "from: @(#)if.c	8.2 (Berkeley) 2/21/94";
#else
static char *rcsid = "$OpenBSD: if.c,v 1.20 2000/02/05 18:46:50 itojun Exp $";
#endif
#endif /* not lint */

#include <sys/types.h>
#include <sys/protosw.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/if_ether.h>
#include <netns/ns.h>
#include <netns/ns_if.h>
#include <netipx/ipx.h>
#include <netipx/ipx_if.h>
#include <netiso/iso.h>
#include <netiso/iso_var.h>
#include <arpa/inet.h>

#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "netstat.h"

#define	YES	1
#define	NO	0

static void sidewaysintpr __P((u_int, u_long));
static void catchalarm __P((int));

/*
 * Print a description of the network interfaces.
 * NOTE: ifnetaddr is the location of the kernel global "ifnet",
 * which is a TAILQ_HEAD.
 */
void
intpr(interval, ifnetaddr)
	int interval;
	u_long ifnetaddr;
{
	struct ifnet ifnet;
	union {
		struct ifaddr ifa;
		struct in_ifaddr in;
#ifdef INET6
		struct in6_ifaddr in6;
#endif
		struct ns_ifaddr ns;
		struct ipx_ifaddr ipx;
		struct iso_ifaddr iso;
	} ifaddr;
	u_long ifaddraddr;
	struct sockaddr *sa;
	struct ifnet_head ifhead;	/* TAILQ_HEAD */
	char name[IFNAMSIZ];
	int n;

	if (ifnetaddr == 0) {
		printf("ifnet: symbol not defined\n");
		return;
	}
	if (interval) {
		sidewaysintpr((unsigned)interval, ifnetaddr);
		return;
	}

	/*
	 * Find the pointer to the first ifnet structure.  Replace
	 * the pointer to the TAILQ_HEAD with the actual pointer
	 * to the first list element.
	 */
	if (kread(ifnetaddr, (char *)&ifhead, sizeof ifhead))
		return;
	ifnetaddr = (u_long)ifhead.tqh_first;

	printf("%-7.7s %-5.5s %-11.11s %-17.17s %8.8s %5.5s %8.8s %5.5s",
		"Name", "Mtu", "Network", "Address", "Ipkts", "Ierrs",
		"Opkts", "Oerrs");
	printf(" %5s", "Coll");
	if (tflag)
		printf(" %s", "Time");
	if (dflag)
		printf(" %s", "Drop");
	putchar('\n');
	ifaddraddr = 0;
	while (ifnetaddr || ifaddraddr) {
		struct sockaddr_in *sin;
#ifdef INET6
		struct sockaddr_in6 *sin6;
#endif
		register char *cp;
		int n, m;

		if (ifaddraddr == 0) {
			if (kread(ifnetaddr, (char *)&ifnet, sizeof ifnet))
				return;
			bcopy(ifnet.if_xname, name, IFNAMSIZ);
			name[IFNAMSIZ - 1] = '\0';	/* sanity */
			ifnetaddr = (u_long)ifnet.if_list.tqe_next;
			if (interface != 0 && strcmp(name, interface) != 0)
				continue;
			cp = strchr(name, '\0');
			if ((ifnet.if_flags & IFF_UP) == 0)
				*cp++ = '*';
			*cp = '\0';
			ifaddraddr = (u_long)ifnet.if_addrlist.tqh_first;
		}
		printf("%-7.7s %-5ld ", name, ifnet.if_mtu);
		if (ifaddraddr == 0) {
			printf("%-11.11s ", "none");
			printf("%-15.15s ", "none");
		} else {
			if (kread(ifaddraddr, (char *)&ifaddr, sizeof ifaddr)) {
				ifaddraddr = 0;
				continue;
			}
#define CP(x) ((char *)(x))
			cp = (CP(ifaddr.ifa.ifa_addr) - CP(ifaddraddr)) +
				CP(&ifaddr); sa = (struct sockaddr *)cp;
			switch (sa->sa_family) {
			case AF_UNSPEC:
				printf("%-11.11s ", "none");
				printf("%-17.17s ", "none");
				break;
			case AF_INET:
				sin = (struct sockaddr_in *)sa;
#ifdef notdef
				/* can't use inet_makeaddr because kernel
				 * keeps nets unshifted.
				 */
				in = inet_makeaddr(ifaddr.in.ia_subnet,
					INADDR_ANY);
				cp = netname(in.s_addr,
			    	    ifaddr.in.ia_subnetmask);
#else
				cp = netname(ifaddr.in.ia_subnet,
				    ifaddr.in.ia_subnetmask);
#endif
				if (vflag)
					n = strlen(cp) < 11 ? 11 : strlen(cp);
				else
					n = 11;
				printf("%-*.*s ", n, n, cp);
				cp = routename(sin->sin_addr.s_addr);
				if (vflag)
					n = strlen(cp) < 17 ? 17 : strlen(cp);
				else
					n = 17;
				printf("%-*.*s ", n, n, cp);

				if (aflag) {
					u_long multiaddr;
					struct in_multi inm;
		
					multiaddr = (u_long)ifaddr.in.ia_multiaddrs.lh_first;
					while (multiaddr != 0) {
						kread(multiaddr, (char *)&inm,
						    sizeof inm);
						printf("\n%25s %-17.17s ", "",
						    routename(inm.inm_addr.s_addr));
						multiaddr = (u_long)inm.inm_list.le_next;
					}
				}
				break;
#ifdef INET6
			case AF_INET6:
				sin6 = (struct sockaddr_in6 *)sa;
#ifdef KAME_SCOPEID
				if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr)) {
					sin6->sin6_scope_id =
						ntohs(*(u_int16_t *)
						  &sin6->sin6_addr.s6_addr[2]);
					/* too little width */
					if (!vflag)
						sin6->sin6_scope_id = 0;
					sin6->sin6_addr.s6_addr[2] = 0;
					sin6->sin6_addr.s6_addr[3] = 0;
				}
#endif
				cp = netname6(&ifaddr.in6.ia_addr,
					&ifaddr.in6.ia_prefixmask.sin6_addr);
				if (vflag)
					n = strlen(cp) < 11 ? 11 : strlen(cp);
				else
					n = 11;
				printf("%-*.*s ", n, n, cp);
				cp = routename6(sin6);
				if (vflag)
					n = strlen(cp) < 17 ? 17 : strlen(cp);
				else
					n = 17;
				printf("%-*.*s ", n, n, cp);
				if (aflag) {
					u_long multiaddr;
					struct in6_multi inm;
					char hbuf[INET6_ADDRSTRLEN];
		
					multiaddr = (u_long)ifaddr.in6.ia6_multiaddrs.lh_first;
					while (multiaddr != 0) {
						kread(multiaddr, (char *)&inm,
						    sizeof inm);
						inet_ntop(AF_INET6, &inm.in6m_addr,
							hbuf, sizeof(hbuf));
						if (vflag)
							n = strlen(hbuf) < 17 ? 17 : strlen(hbuf);
						else
							n = 17;
						printf("\n%25s %-*.*s ", "",
						    n, n, hbuf);
						multiaddr = (u_long)inm.in6m_entry.le_next;
					}
				}
				break;
#endif
			case AF_IPX:
				{
				struct sockaddr_ipx *sipx =
					(struct sockaddr_ipx *)sa;
				u_long net;
				char netnum[8];

				*(union ipx_net *)&net = sipx->sipx_addr.ipx_net;
				snprintf(netnum, sizeof netnum, "%xH",
				    ntohl(net));
				upHex(netnum);
				printf("ipx:%-8s", netnum);
				printf("%-17s ",
				    ipx_phost((struct sockaddr *)sipx));
				}
				break;
			case AF_APPLETALK:
				printf("atlk:%-12s",atalk_print(sa,0x10) );
				printf("%-12s ",atalk_print(sa,0x0b) );
				break;
			case AF_NS:
				{
				struct sockaddr_ns *sns =
					(struct sockaddr_ns *)sa;
				u_long net;
				char netnum[8];

				*(union ns_net *)&net = sns->sns_addr.x_net;
				snprintf(netnum, sizeof netnum, "%xH",
				    ntohl(net));
				upHex(netnum);
				printf("ns:%-8s ", netnum);
				printf("%-17s ",
				    ns_phost((struct sockaddr *)sns));
				}
				break;
			case AF_LINK:
				{
				struct sockaddr_dl *sdl =
					(struct sockaddr_dl *)sa;
				m = printf("%-11.11s ", "<Link>");
				if (sdl->sdl_type == IFT_ETHER ||
				    sdl->sdl_type == IFT_FDDI)
					printf("%-17.17s ",
					    ether_ntoa((struct ether_addr *)LLADDR(sdl)));
				else {
					cp = (char *)LLADDR(sdl);
					n = sdl->sdl_alen;
					goto hexprint;
				}
				}
				break;
			default:
				m = printf("(%d)", sa->sa_family);
				for (cp = sa->sa_len + (char *)sa;
					--cp > sa->sa_data && (*cp == 0);) {}
				n = cp - sa->sa_data + 1;
				cp = sa->sa_data;
			hexprint:
				while (--n >= 0)
					m += printf("%x%c", *cp++ & 0xff,
						    n > 0 ? '.' : ' ');
				m = 30 - m;
				while (m-- > 0)
					putchar(' ');
				break;
			}
			ifaddraddr = (u_long)ifaddr.ifa.ifa_list.tqe_next;
		}
		printf("%8ld %5ld %8ld %5ld %5ld",
		    ifnet.if_ipackets, ifnet.if_ierrors,
		    ifnet.if_opackets, ifnet.if_oerrors,
		    ifnet.if_collisions);
		if (tflag)
			printf(" %3d", ifnet.if_timer);
		if (dflag)
			printf(" %3d", ifnet.if_snd.ifq_drops);
		putchar('\n');
	}
}

#define	MAXIF	100
struct	iftot {
	char	ift_name[IFNAMSIZ];	/* interface name */
	int	ift_ip;			/* input packets */
	int	ift_ie;			/* input errors */
	int	ift_op;			/* output packets */
	int	ift_oe;			/* output errors */
	int	ift_co;			/* collisions */
	int	ift_dr;			/* drops */
} iftot[MAXIF];

u_char	signalled;			/* set if alarm goes off "early" */

/*
 * Print a running summary of interface statistics.
 * Repeat display every interval seconds, showing statistics
 * collected over that interval.  Assumes that interval is non-zero.
 * First line printed at top of screen is always cumulative.
 */
static void
sidewaysintpr(interval, off)
	unsigned interval;
	u_long off;
{
	struct ifnet ifnet;
	u_long firstifnet;
	register struct iftot *ip, *total;
	register int line;
	struct iftot *lastif, *sum, *interesting;
	struct ifnet_head ifhead;	/* TAILQ_HEAD */
	int oldmask;

	/*
	 * Find the pointer to the first ifnet structure.  Replace
	 * the pointer to the TAILQ_HEAD with the actual pointer
	 * to the first list element.
	 */
	if (kread(off, (char *)&ifhead, sizeof ifhead))
		return;
	firstifnet = (u_long)ifhead.tqh_first;

	lastif = iftot;
	sum = iftot + MAXIF - 1;
	total = sum - 1;
	interesting = (interface == NULL) ? iftot : NULL;
	for (off = firstifnet, ip = iftot; off;) {
		if (kread(off, (char *)&ifnet, sizeof ifnet))
			break;
		bzero(ip->ift_name, sizeof(ip->ift_name));
		snprintf(ip->ift_name, IFNAMSIZ, "(%s)", ifnet.if_xname);
		if (interface && strcmp(ifnet.if_xname, interface) == 0)
			interesting = ip;
		ip++;
		if (ip >= iftot + MAXIF - 2)
			break;
		off = (u_long)ifnet.if_list.tqe_next;
	}
	if (interesting == NULL) {
		fprintf(stderr, "%s: %s: unknown interface\n",
		    __progname, interface);
		exit(1);
	}
	lastif = ip;

	(void)signal(SIGALRM, catchalarm);
	signalled = NO;
	(void)alarm(interval);
banner:
	printf("   input    %-6.6s    output       ", interesting->ift_name);
	if (lastif - iftot > 0) {
		if (dflag)
			printf("      ");
		printf("     input   (Total)    output");
	}
	for (ip = iftot; ip < iftot + MAXIF; ip++) {
		ip->ift_ip = 0;
		ip->ift_ie = 0;
		ip->ift_op = 0;
		ip->ift_oe = 0;
		ip->ift_co = 0;
		ip->ift_dr = 0;
	}
	putchar('\n');
	printf("%8.8s %5.5s %8.8s %5.5s %5.5s ",
		"packets", "errs", "packets", "errs", "colls");
	if (dflag)
		printf("%5.5s ", "drops");
	if (lastif - iftot > 0)
		printf(" %8.8s %5.5s %8.8s %5.5s %5.5s",
			"packets", "errs", "packets", "errs", "colls");
	if (dflag)
		printf(" %5.5s", "drops");
	putchar('\n');
	fflush(stdout);
	line = 0;
loop:
	sum->ift_ip = 0;
	sum->ift_ie = 0;
	sum->ift_op = 0;
	sum->ift_oe = 0;
	sum->ift_co = 0;
	sum->ift_dr = 0;
	for (off = firstifnet, ip = iftot; off && ip < lastif; ip++) {
		if (kread(off, (char *)&ifnet, sizeof ifnet)) {
			off = 0;
			continue;
		}
		if (ip == interesting) {
			printf("%8ld %5ld %8ld %5ld %5ld",
				ifnet.if_ipackets - ip->ift_ip,
				ifnet.if_ierrors - ip->ift_ie,
				ifnet.if_opackets - ip->ift_op,
				ifnet.if_oerrors - ip->ift_oe,
				ifnet.if_collisions - ip->ift_co);
			if (dflag)
				printf(" %5d",
				    ifnet.if_snd.ifq_drops - ip->ift_dr);
		}
		ip->ift_ip = ifnet.if_ipackets;
		ip->ift_ie = ifnet.if_ierrors;
		ip->ift_op = ifnet.if_opackets;
		ip->ift_oe = ifnet.if_oerrors;
		ip->ift_co = ifnet.if_collisions;
		ip->ift_dr = ifnet.if_snd.ifq_drops;
		sum->ift_ip += ip->ift_ip;
		sum->ift_ie += ip->ift_ie;
		sum->ift_op += ip->ift_op;
		sum->ift_oe += ip->ift_oe;
		sum->ift_co += ip->ift_co;
		sum->ift_dr += ip->ift_dr;
		off = (u_long)ifnet.if_list.tqe_next;
	}
	if (lastif - iftot > 0) {
		printf("  %8d %5d %8d %5d %5d",
			sum->ift_ip - total->ift_ip,
			sum->ift_ie - total->ift_ie,
			sum->ift_op - total->ift_op,
			sum->ift_oe - total->ift_oe,
			sum->ift_co - total->ift_co);
		if (dflag)
			printf(" %5d", sum->ift_dr - total->ift_dr);
	}
	*total = *sum;
	putchar('\n');
	fflush(stdout);
	line++;
	oldmask = sigblock(sigmask(SIGALRM));
	if (! signalled) {
		sigpause(0);
	}
	sigsetmask(oldmask);
	signalled = NO;
	(void)alarm(interval);
	if (line == 21)
		goto banner;
	goto loop;
	/*NOTREACHED*/
}

/*
 * Called if an interval expires before sidewaysintpr has completed a loop.
 * Sets a flag to not wait for the alarm.
 */
static void
catchalarm(signo)
	int signo;
{
	signalled = YES;
}
