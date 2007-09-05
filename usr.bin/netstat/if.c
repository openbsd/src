/*	$OpenBSD: if.c,v 1.52 2007/09/05 20:27:04 claudio Exp $	*/
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
 */

#ifndef lint
#if 0
static char sccsid[] = "from: @(#)if.c	8.2 (Berkeley) 2/21/94";
#else
static char *rcsid = "$OpenBSD: if.c,v 1.52 2007/09/05 20:27:04 claudio Exp $";
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
#include <arpa/inet.h>

#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "netstat.h"

#define	YES	1
#define	NO	0

static void sidewaysintpr(u_int, u_long);
static void catchalarm(int);

/*
 * Print a description of the network interfaces.
 * NOTE: ifnetaddr is the location of the kernel global "ifnet",
 * which is a TAILQ_HEAD.
 */
void
intpr(int interval, u_long ifnetaddr)
{
	struct ifnet ifnet;
	union {
		struct ifaddr ifa;
		struct in_ifaddr in;
#ifdef INET6
		struct in6_ifaddr in6;
#endif
	} ifaddr;
	u_int64_t total;
	u_long ifaddraddr;
	struct sockaddr *sa;
	struct ifnet_head ifhead;	/* TAILQ_HEAD */
	char name[IFNAMSIZ];

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
	if (kread(ifnetaddr, &ifhead, sizeof ifhead))
		return;
	ifnetaddr = (u_long)TAILQ_FIRST(&ifhead);

	printf("%-7.7s %-5.5s %-11.11s %-17.17s ",
	    "Name", "Mtu", "Network", "Address");
	if (bflag)
		printf("%10.10s %10.10s", "Ibytes", "Obytes");
	else
		printf("%8.8s %5.5s %8.8s %5.5s %5.5s",
		    "Ipkts", "Ierrs", "Opkts", "Oerrs", "Colls");
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
		char *cp;
		int n, m;

		if (ifaddraddr == 0) {
			if (kread(ifnetaddr, &ifnet, sizeof ifnet))
				return;
			bcopy(ifnet.if_xname, name, IFNAMSIZ);
			name[IFNAMSIZ - 1] = '\0';	/* sanity */
			ifnetaddr = (u_long)TAILQ_NEXT(&ifnet, if_list);
			if (interface != 0 && strcmp(name, interface) != 0)
				continue;
			cp = strchr(name, '\0');
			if ((ifnet.if_flags & IFF_UP) == 0)
				*cp++ = '*';
			*cp = '\0';
			ifaddraddr = (u_long)TAILQ_FIRST(&ifnet.if_addrlist);
		}

		if (qflag) {
			total = ifnet.if_ibytes + ifnet.if_obytes +
			    ifnet.if_ipackets + ifnet.if_ierrors +
			    ifnet.if_opackets + ifnet.if_oerrors +
			    ifnet.if_collisions;
			if (tflag)
				total += ifnet.if_timer;
			if (dflag)
				total += ifnet.if_snd.ifq_drops;
			if (total == 0) {
				ifaddraddr = 0;
				continue;
			}
		}

		printf("%-7s %-5ld ", name, ifnet.if_mtu);
		if (ifaddraddr == 0) {
			printf("%-11.11s ", "none");
			printf("%-17.17s ", "none");
		} else {
			if (kread(ifaddraddr, &ifaddr, sizeof ifaddr)) {
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
				cp = netname4(in.s_addr,
				    ifaddr.in.ia_subnetmask);
#else
				cp = netname4(ifaddr.in.ia_subnet,
				    ifaddr.in.ia_subnetmask);
#endif
				if (vflag)
					n = strlen(cp) < 11 ? 11 : strlen(cp);
				else
					n = 11;
				printf("%-*.*s ", n, n, cp);
				cp = routename4(sin->sin_addr.s_addr);
				if (vflag)
					n = strlen(cp) < 17 ? 17 : strlen(cp);
				else
					n = 17;
				printf("%-*.*s ", n, n, cp);

				if (aflag) {
					u_long multiaddr;
					struct in_multi inm;

					multiaddr = (u_long)LIST_FIRST(&ifaddr.in.ia_multiaddrs);
					while (multiaddr != 0) {
						kread(multiaddr, &inm, sizeof inm);
						printf("\n%25s %-17.17s ", "",
						    routename4(inm.inm_addr.s_addr));
						multiaddr = (u_long)LIST_NEXT(&inm, inm_list);
					}
				}
				break;
#ifdef INET6
			case AF_INET6:
				sin6 = (struct sockaddr_in6 *)sa;
#ifdef __KAME__
				if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr)) {
					sin6->sin6_scope_id =
					    ntohs(*(u_int16_t *)
					    &sin6->sin6_addr.s6_addr[2]);
					sin6->sin6_addr.s6_addr[2] = 0;
					sin6->sin6_addr.s6_addr[3] = 0;
				}
#endif
				cp = netname6(&ifaddr.in6.ia_addr,
				    &ifaddr.in6.ia_prefixmask);
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
					struct sockaddr_in6 m6;

					multiaddr = (u_long)LIST_FIRST(&ifaddr.in6.ia6_multiaddrs);
					while (multiaddr != 0) {
						kread(multiaddr, &inm, sizeof inm);
						memset(&m6, 0, sizeof(m6));
						m6.sin6_len = sizeof(struct sockaddr_in6);
						m6.sin6_family = AF_INET6;
						m6.sin6_addr = inm.in6m_addr;
#ifdef __KAME__
						if (IN6_IS_ADDR_MC_LINKLOCAL(&m6.sin6_addr) ||
						    IN6_IS_ADDR_MC_INTFACELOCAL(&m6.sin6_addr)) {
							m6.sin6_scope_id =
							    ntohs(*(u_int16_t *)
							    &m6.sin6_addr.s6_addr[2]);
							m6.sin6_addr.s6_addr[2] = 0;
							m6.sin6_addr.s6_addr[3] = 0;
						}
#endif
						cp = routename6(&m6);
						if (vflag)
							n = strlen(cp) < 17 ? 17 : strlen(cp);
						else
							n = 17;
						printf("\n%25s %-*.*s ", "",
						    n, n, cp);
						multiaddr = (u_long)LIST_NEXT(&inm, in6m_entry);
					}
				}
				break;
#endif
			case AF_APPLETALK:
				printf("atlk:%-12s",atalk_print(sa,0x10) );
				printf("%-12s ",atalk_print(sa,0x0b) );
				break;
			case AF_LINK:
				{
				struct sockaddr_dl *sdl =
					(struct sockaddr_dl *)sa;
				m = printf("%-11.11s ", "<Link>");
				if (sdl->sdl_type == IFT_ETHER ||
				    sdl->sdl_type == IFT_CARP ||
				    sdl->sdl_type == IFT_FDDI ||
				    sdl->sdl_type == IFT_ISO88025)
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
			ifaddraddr = (u_long)TAILQ_NEXT(&ifaddr.ifa, ifa_list);
		}
		if (bflag)
			printf("%10llu %10llu",
			    ifnet.if_ibytes, ifnet.if_obytes);
		else
			printf("%8llu %5llu %8llu %5llu %5llu",
			    ifnet.if_ipackets, ifnet.if_ierrors,
			    ifnet.if_opackets, ifnet.if_oerrors,
			    ifnet.if_collisions);
		if (tflag)
			printf(" %4d", ifnet.if_timer);
		if (dflag)
			printf(" %4d", ifnet.if_snd.ifq_drops);
		putchar('\n');
	}
}

#define	MAXIF	100
struct	iftot {
	char	ift_name[IFNAMSIZ];	/* interface name */
	u_long	ift_ip;			/* input packets */
	u_long	ift_ib;			/* input bytes */
	u_long	ift_ie;			/* input errors */
	u_long	ift_op;			/* output packets */
	u_long	ift_ob;			/* output bytes */
	u_long	ift_oe;			/* output errors */
	u_long	ift_co;			/* collisions */
	u_long	ift_dr;			/* drops */
} iftot[MAXIF];

volatile sig_atomic_t signalled;	/* set if alarm goes off "early" */

/*
 * Print a running summary of interface statistics.
 * Repeat display every interval seconds, showing statistics
 * collected over that interval.  Assumes that interval is non-zero.
 * First line printed at top of screen is always cumulative.
 */
static void
sidewaysintpr(unsigned int interval, u_long off)
{
	struct ifnet ifnet;
	u_long firstifnet;
	struct iftot *ip, *total;
	int line;
	struct iftot *lastif, *sum, *interesting;
	struct ifnet_head ifhead;	/* TAILQ_HEAD */
	sigset_t emptyset;

	/*
	 * Find the pointer to the first ifnet structure.  Replace
	 * the pointer to the TAILQ_HEAD with the actual pointer
	 * to the first list element.
	 */
	if (kread(off, &ifhead, sizeof ifhead))
		return;
	firstifnet = (u_long)TAILQ_FIRST(&ifhead);

	lastif = iftot;
	sum = iftot + MAXIF - 1;
	total = sum - 1;
	interesting = (interface == NULL) ? iftot : NULL;
	for (off = firstifnet, ip = iftot; off;) {
		if (kread(off, &ifnet, sizeof ifnet))
			break;
		bzero(ip->ift_name, sizeof(ip->ift_name));
		snprintf(ip->ift_name, IFNAMSIZ, "%s", ifnet.if_xname);
		if (interface && strcmp(ifnet.if_xname, interface) == 0)
			interesting = ip;
		ip++;
		if (ip >= iftot + MAXIF - 2)
			break;
		off = (u_long)TAILQ_NEXT(&ifnet, if_list);
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
	if (bflag)
		printf("%7.7s in %8.8s %6.6s out %5.5s",
		    interesting->ift_name, " ",
		    interesting->ift_name, " ");
	else
		printf("%5.5s in %5.5s%5.5s out %5.5s %5.5s",
		    interesting->ift_name, " ",
		    interesting->ift_name, " ", " ");
	if (dflag)
		printf(" %5.5s", " ");
	if (lastif - iftot > 0) {
		if (bflag)
			printf("  %7.7s in %8.8s %6.6s out %5.5s",
			    "total", " ", "total", " ");
		else
			printf("  %5.5s in %5.5s%5.5s out %5.5s %5.5s",
			    "total", " ", "total", " ", " ");
		if (dflag)
			printf(" %5.5s", " ");
	}
	for (ip = iftot; ip < iftot + MAXIF; ip++) {
		ip->ift_ip = 0;
		ip->ift_ib = 0;
		ip->ift_ie = 0;
		ip->ift_op = 0;
		ip->ift_ob = 0;
		ip->ift_oe = 0;
		ip->ift_co = 0;
		ip->ift_dr = 0;
	}
	putchar('\n');
	if (bflag)
		printf("%10.10s %8.8s %10.10s %5.5s",
		    "bytes", " ", "bytes", " ");
	else
		printf("%8.8s %5.5s %8.8s %5.5s %5.5s",
		    "packets", "errs", "packets", "errs", "colls");
	if (dflag)
		printf(" %5.5s", "drops");
	if (lastif - iftot > 0) {
		if (bflag)
			printf("  %10.10s %8.8s %10.10s %5.5s",
			    "bytes", " ", "bytes", " ");
		else
			printf("  %8.8s %5.5s %8.8s %5.5s %5.5s",
			    "packets", "errs", "packets", "errs", "colls");
		if (dflag)
			printf(" %5.5s", "drops");
	}
	putchar('\n');
	fflush(stdout);
	line = 0;
loop:
	sum->ift_ip = 0;
	sum->ift_ib = 0;
	sum->ift_ie = 0;
	sum->ift_op = 0;
	sum->ift_ob = 0;
	sum->ift_oe = 0;
	sum->ift_co = 0;
	sum->ift_dr = 0;
	for (off = firstifnet, ip = iftot; off && ip < lastif; ip++) {
		if (kread(off, &ifnet, sizeof ifnet)) {
			off = 0;
			continue;
		}
		if (ip == interesting) {
			if (bflag)
				printf("%10lu %8.8s %10lu %5.5s",
				    ifnet.if_ibytes - ip->ift_ib, " ",
				    ifnet.if_obytes - ip->ift_ob, " ");
			else
				printf("%8lu %5lu %8lu %5lu %5lu",
				    ifnet.if_ipackets - ip->ift_ip,
				    ifnet.if_ierrors - ip->ift_ie,
				    ifnet.if_opackets - ip->ift_op,
				    ifnet.if_oerrors - ip->ift_oe,
				    ifnet.if_collisions - ip->ift_co);
			if (dflag)
				printf(" %5lu",
				    ifnet.if_snd.ifq_drops - ip->ift_dr);
		}
		ip->ift_ip = ifnet.if_ipackets;
		ip->ift_ib = ifnet.if_ibytes;
		ip->ift_ie = ifnet.if_ierrors;
		ip->ift_op = ifnet.if_opackets;
		ip->ift_ob = ifnet.if_obytes;
		ip->ift_oe = ifnet.if_oerrors;
		ip->ift_co = ifnet.if_collisions;
		ip->ift_dr = ifnet.if_snd.ifq_drops;
		sum->ift_ip += ip->ift_ip;
		sum->ift_ib += ip->ift_ib;
		sum->ift_ie += ip->ift_ie;
		sum->ift_op += ip->ift_op;
		sum->ift_ob += ip->ift_ob;
		sum->ift_oe += ip->ift_oe;
		sum->ift_co += ip->ift_co;
		sum->ift_dr += ip->ift_dr;
		off = (u_long)TAILQ_NEXT(&ifnet, if_list);
	}
	if (lastif - iftot > 0) {
		if (bflag)
			printf("  %10lu %8.8s %10lu %5.5s",
			    sum->ift_ib - total->ift_ib, " ",
			    sum->ift_ob - total->ift_ob, " ");
		else
			printf("  %8lu %5lu %8lu %5lu %5lu",
			    sum->ift_ip - total->ift_ip,
			    sum->ift_ie - total->ift_ie,
			    sum->ift_op - total->ift_op,
			    sum->ift_oe - total->ift_oe,
			    sum->ift_co - total->ift_co);
		if (dflag)
			printf(" %5lu", sum->ift_dr - total->ift_dr);
	}
	*total = *sum;
	putchar('\n');
	fflush(stdout);
	line++;
	sigemptyset(&emptyset);
	if (!signalled)
		sigsuspend(&emptyset);
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
/* ARGSUSED */
static void
catchalarm(int signo)
{
	signalled = YES;
}
