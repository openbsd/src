/*
 * (C)opyright 1995 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 */
#ifndef	lint
static	char	sccsid[] = "@(#)ip_nat.c	1.3 1/12/96 (C) 1995 Darren Reed";
#endif

#include <stdio.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#if !defined(__SVR4) && !defined(__svr4__)
# include <sys/dir.h>
# include <sys/mbuf.h>
#else
# include <sys/byteorder.h>
# include <sys/dditypes.h>
# include <sys/stream.h>
# include <sys/kmem.h>
#endif

#include <net/if.h>
#ifdef sun
#include <net/af.h>
#endif
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/tcpip.h>
#include <netinet/ip_icmp.h>
#include <syslog.h>
#include "ip_fil.h"
#include "ip_nat.h"
#ifndef	MIN
#define	MIN(a,b)	(((a)<(b))?(a):(b))
#endif

nat_t	*nat_table[2][NAT_SIZE];
ipnat_t	*nat_list = NULL;
u_long	nat_inuse = 0;
natstat_t nat_stats;
#if	SOLARIS
# ifndef	_KERNEL
#define	bcmp(a,b,c)	memcpy(a,b,c)
#define	bcopy(a,b,c)	memmove(b,a,c)
# else
extern	kmutex_t	ipf_nat;
# endif
#endif


/*
 * How the NAT is organised and works.
 *
 * Inside (interface y) NAT       Outside (interface x)
 * -------------------- -+- -------------------------------------
 * Packet going          |   out, processsed by ip_natout() for x
 * ------------>         |   ------------>
 * src=10.1.1.1          |   src=192.1.1.1
 *                       |
 *                       |   in, processed by ip_natin() for x
 * <------------         |   <------------
 * dst=10.1.1.1          |   dst=192.1.1.1
 * -------------------- -+- -------------------------------------
 * ip_natout() - changes ip_src and if required, sport
 *             - creates a new mapping, if required.
 * ip_natin()  - changes ip_dst and if required, dport
 *
 * In the NAT table, internal source is recorded as "in" and externally
 * seen as "out".
 */

/*
 * Handle ioctls which manipulate the NAT.
 */
int nat_ioctl(data, cmd)
caddr_t data;
int cmd;
{
	register ipnat_t *nat, *n, **np;

	/*
	 * For add/delete, look to see if the NAT entry is already present
	 */
	MUTEX_ENTER(&ipf_nat);
	if ((cmd == SIOCADNAT) || (cmd == SIOCRMNAT)) {
		nat = (ipnat_t *)data;
		for (np = &nat_list; (n = *np); np = &n->in_next)
			if (!bcmp((char *)&nat->in_port, (char *)&n->in_port,
				  IPN_CMPSIZ))
				break;
	}

	switch (cmd)
	{
	case SIOCADNAT :
		if (n) {
			MUTEX_EXIT(&ipf_nat);
			return EEXIST;
		}
		if (!(n = (ipnat_t *)KMALLOC(sizeof(*n)))) {
			MUTEX_EXIT(&ipf_nat);
			return ENOMEM;
		}
		IRCOPY((char *)data, (char *)np, sizeof(*np));
		bcopy((char *)nat, (char *)n, sizeof(*n));
		n->in_ifp = (void *)GETUNIT(n->in_ifname);
		n->in_next = *np;
		n->in_space = ~(0xffffffff & ntohl(n->in_outmsk));
		n->in_space--;	/* lose 1 for broadcast address */
		n->in_nip = ntohl(n->in_outip) + 1;
		n->in_pnext = ntohs(n->in_pmin);
		*np = n;
		break;
	case SIOCRMNAT :
		if (!n) {
			MUTEX_EXIT(&ipf_nat);
			return ESRCH;
		}
		*np = n->in_next;
		KFREE(n);
		break;
	case SIOCGNATS :
		nat_stats.ns_table = (nat_t ***)nat_table;
		nat_stats.ns_list = nat_list;
		nat_stats.ns_inuse = nat_inuse;
		IWCOPY((char *)&nat_stats, (char *)data, sizeof(nat_stats));
		break;
	}
	MUTEX_EXIT(&ipf_nat);
	return 0;
}


/*
 * Create a new NAT table entry.
 */
nat_t *nat_new(ip, hlen, flags)
ip_t *ip;
int hlen;
u_short flags;
{
	u_short port = 0, sport = 0;
	struct in_addr in;
	tcphdr_t *tcp;
	ipnat_t *np;
	nat_t *nat, **natp;

	if (flags) {
		tcp = (tcphdr_t *)((char *)ip + hlen);
		sport = tcp->th_sport;
	}

	MUTEX_ENTER(&ipf_nat);
	/*
	 * Search the current table for a match.
	 */
	do {
		in.s_addr = np->in_nip;
		if (np->in_flags & IPN_TCPUDP) {
			port = htons(np->in_pnext++);
			if (np->in_pnext >= ntohs(np->in_pmax)) {
				np->in_pnext = ntohs(np->in_pmin);
				np->in_nip++;
				np->in_space--;
			}
		} else {
			np->in_space--;
			np->in_nip++;
		}
		if ((np->in_nip & ntohl(np->in_outmsk)) > ntohl(np->in_outip))
			np->in_nip = ntohl(np->in_outip) + 1;
	} while (nat_lookupinip(in, sport));

	if (!(nat = (nat_t *)KMALLOC(sizeof(*nat)))) {
		MUTEX_EXIT(&ipf_nat);
		return NULL;
	}
	nat->nat_use = 0;
	in.s_addr = htonl(in.s_addr);
	nat->nat_inip = ip->ip_src;
	nat->nat_outip = in;
	nat->nat_sumd = (ntohl(ip->ip_src.s_addr) & 0xffff) +
			(ntohl(ip->ip_src.s_addr) >> 16);
	nat->nat_sumd -= ((ntohl(in.s_addr) & 0xffff) +
			  (ntohl(in.s_addr) >> 16));
	if (sport) {
		nat->nat_inport = sport;
		nat->nat_outport = port;
		nat->nat_sumd += (ntohs(sport) - ntohs(port));
	} else {
		nat->nat_inport = 0;
		nat->nat_outport = 0;
	}
	natp = &nat_table[0][nat->nat_inip.s_addr % NAT_SIZE];
	nat->nat_next = *natp;
	*natp = nat;
	nat->nat_use++;
	natp = &nat_table[1][nat->nat_outip.s_addr % NAT_SIZE];
	nat->nat_next = *natp;
	*natp = nat;
	nat->nat_use++;
	ip->ip_src = in;
	if (flags)
		tcp->th_sport = htons(port);
	nat_stats.ns_added++;
	nat_inuse++;
	MUTEX_EXIT(&ipf_nat);
	return nat;
}


nat_t *nat_lookupoutip(ipaddr, sport)
struct in_addr ipaddr;
u_short sport;
{
	nat_t *nat;

	nat = nat_table[1][ipaddr.s_addr % NAT_SIZE];

	MUTEX_ENTER(&ipf_nat);
	for (; nat; nat = nat->nat_next)
		if (nat->nat_outip.s_addr == ipaddr.s_addr) {
			if (nat->nat_outport && (sport != nat->nat_outport))
				continue;
			return nat;
		}
	MUTEX_EXIT(&ipf_nat);
	return NULL;
}


/*
 * Packets going out on the external interface go through this.
 * Here, the source address requires alteration, if anything.
 */
void ip_natout(ifp, ip, hlen)
struct ifnet *ifp;
ip_t *ip;
int hlen;
{
	register ipnat_t *np;
	register u_long ipa;
	register u_long sum1, sum2;
	tcphdr_t *tcp;
	nat_t *nat;
	u_short nflags = 0, sport = 0;

	if (ip->ip_p == IPPROTO_TCP)
		nflags = IPN_TCP;
	else if (ip->ip_p == IPPROTO_UDP)
		nflags = IPN_UDP;
	if (nflags) {
		tcp = (tcphdr_t *)((char *)ip + hlen);
		sport = tcp->th_sport;
	}

	ipa = ip->ip_src.s_addr;

	MUTEX_ENTER(&ipf_nat);
	for (np = nat_list; np; np = np->in_next)
		if ((np->in_ifp == ifp) && np->in_space &&
		    (!np->in_flags || (np->in_flags & nflags)) &&
		    ((ipa & np->in_inmsk) == np->in_inip)) {
			/*
			 * If there is no current entry in the nat table for
			 * this IP#, create one for it.
			 */
			if (!(nat = nat_lookupinip(ip->ip_src, sport))) {
				if (!(nat = nat_new(ip, hlen,
						    nflags & np->in_flags))) {
					MUTEX_EXIT(&ipf_nat);
					return;
				}
			} else
				ip->ip_src = nat->nat_outip;

			nat->nat_age = 1200;	/* 5 mins */

			/*
			 * Fix up checksums, not by recalculating them, but
			 * simply computing adjustments.
			 */
			if (nflags) {
				if (nat->nat_outport) {
					sum1 += sport;
					tcp->th_sport = nat->nat_outport;
					sum2 += tcp->th_sport;
				}

				sum2 = nat->nat_sumd;

				if (ip->ip_p == IPPROTO_TCP) {
					sum2 += ntohs(tcp->th_sum);
					sum2 = (sum2 >> 16) + (sum2 & 0xffff);
					sum2 += (sum2 >> 16);
					tcp->th_sum = htons(sum2);
				} else if (ip->ip_p == IPPROTO_UDP) {
					udphdr_t *udp = (udphdr_t *)tcp;

					udp->uh_sum = 0;
				}
			}
			nat_stats.ns_mapped[1]++;
			MUTEX_EXIT(&ipf_nat);
			return;
		}
	MUTEX_EXIT(&ipf_nat);
	return;
}

nat_t *nat_lookupinip(ipaddr, sport)
struct in_addr ipaddr;
u_short sport;
{
	nat_t *nat;

	nat = nat_table[0][ipaddr.s_addr % NAT_SIZE];

	MUTEX_ENTER(&ipf_nat);
	for (; nat; nat = nat->nat_next)
		if (nat->nat_inip.s_addr == ipaddr.s_addr) {
			if (nat->nat_inport && (sport != nat->nat_inport))
				continue;
			return nat;
		}
	MUTEX_EXIT(&ipf_nat);
	return NULL;
}


/*
 * Packets coming in from the external interface go through this.
 * Here, the destination address requires alteration, if anything.
 */
void ip_natin(ifp, ip, hlen)
struct ifnet *ifp;
ip_t *ip;
int hlen;
{
	register ipnat_t *np;
	register struct in_addr in;
	register u_long sum1, sum2;
	tcphdr_t *tcp;
	u_short port = 0, nflags;
	nat_t *nat;

	if (ip->ip_p == IPPROTO_TCP)
		nflags = IPN_TCP;
	else if (ip->ip_p == IPPROTO_UDP)
		nflags = IPN_UDP;
	if (nflags) {
		tcp = (tcphdr_t *)((char *)ip + hlen);
		port = tcp->th_dport;
	}

	in = ip->ip_dst;

	MUTEX_ENTER(&ipf_nat);
	for (np = nat_list; np; np = np->in_next)
		if ((np->in_ifp == ifp) &&
		    (!np->in_flags || (nflags & np->in_flags)) &&
		    ((in.s_addr & np->in_outmsk) == np->in_outip)) {
			if (!(nat = nat_lookupoutip(in, port)))
				continue;
			nat->nat_age = 1200;
			ip->ip_dst = nat->nat_inip;

			/*
			 * Fix up checksums, not by recalculating them, but
			 * simply computing adjustments.
			 */

			if (nflags) {
				u_short *sp = NULL;

				if (nat->nat_inport) {
					sum1 += port;
					tcp->th_dport = nat->nat_inport;
					sum2 += tcp->th_dport;
				}

				sum2 = nat->nat_sumd;

				if (ip->ip_p == IPPROTO_TCP) {
					sp = &tcp->th_sum;
					if (ntohs(*sp) > sum2)
						sum2--;
					sum2 -= ntohs(*sp);
					sum2 = (sum2 >> 16) + (sum2 & 0xffff);
					sum2 += (sum2 >> 16);
					*sp = htons(~sum2);
				} else if (ip->ip_p == IPPROTO_UDP) {
					udphdr_t *udp = (udphdr_t *)tcp;

					udp->uh_sum = 0;
				}
			}
			nat_stats.ns_mapped[0]++;
			MUTEX_EXIT(&ipf_nat);
			return;
		}
	MUTEX_EXIT(&ipf_nat);
	return;
}


/*
 * Free all memory used by NAT structures allocated at runtime.
 */
void ip_natunload()
{
	register struct nat *nat, **natp;
	register struct ipnat *ipn, **ipnp;
	register int i;

	MUTEX_ENTER(&ipf_nat);
	for (i = 0; i < NAT_SIZE; i++)
		for (natp = &nat_table[0][i]; (nat = *natp); ) {
			*natp = nat->nat_next;
			if (!--nat->nat_use)
				KFREE(nat);
		}
	for (i = 0; i < NAT_SIZE; i++)
		for (natp = &nat_table[1][i]; (nat = *natp); ) {
			*natp = nat->nat_next;
			if (!--nat->nat_use)
				KFREE(nat);
		}

	for (ipnp = &nat_list; (ipn = *ipnp); ) {
		*ipnp = ipn->in_next;
		KFREE(ipn);
	}
	MUTEX_EXIT(&ipf_nat);
}


/*
 * Slowly expire held state for NAT entries.  Timeouts are set in
 * expectation of this being called twice per second.
 */
void ip_natexpire()
{
	register struct nat *nat, **natp;
	register int i;

	MUTEX_ENTER(&ipf_nat);
	for (i = 0; i < NAT_SIZE; i++)
		for (natp = &nat_table[0][i]; (nat = *natp); ) {
			if (nat->nat_age > 0)
				nat->nat_age--;
			if (!nat->nat_use || !nat->nat_age) {
				*natp = nat->nat_next;
				if (nat->nat_use)
					nat->nat_use--;
				if (!nat->nat_use) {
					KFREE(nat);
					nat_stats.ns_expire++;
					nat_inuse--;
				}
			} else
				natp = &nat->nat_next;
		}

	for (i = 0; i < NAT_SIZE; i++)
		for (natp = &nat_table[1][i]; (nat = *natp); ) {
			if (nat->nat_age > 0)
				nat->nat_age--;
			if (!nat->nat_use || !nat->nat_age) {
				*natp = nat->nat_next;
				if (nat->nat_use)
					nat->nat_use--;
				if (!nat->nat_use) {
					KFREE(nat);
					nat_stats.ns_expire++;
					nat_inuse--;
				}
			} else
				natp = &nat->nat_next;
		}
	MUTEX_EXIT(&ipf_nat);
}
