/*
 * (C)opyright 1995 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 *
 *  Added redirect stuff and a LOT of bug fixes. (mcn@EnGarde.com)
 *
 * Things still screwed:
 *  1) You can't specify a mapping to a class D address. By default, it
 *     always adds 1 to that address. As a result, when a packet comes back,
 *     the rule won't be matched. (e.g. outgoing address = 199.165.219.2,
 *     whereas the rule says outgoing address = 199.165.219.1/32. Because
 *     ADNATS always adds one, and there really isn't any provision for
 *     only using 1 address (the in_space stuff is broke), there isn't any
 *     easy solution)
 *  2) There needs to be a way to flush the NATs table completely. Either
 *     an ioctl, or an easy way of doing it from ipnat.c.
 *
 * Missing from RFC 1631: ICMP header checksum recalculations.
 *
 */
#if 0
#ifndef	lint
static	char	sccsid[] = "@(#)ip_nat.c	1.11 6/5/96 (C) 1995 Darren Reed";
static	char	rcsid[] = "$OpenBSD: ip_nat.c,v 1.5 1996/10/08 07:33:28 niklas Exp $";
#endif
#endif

#if !defined(_KERNEL) && !defined(KERNEL)
# include <stdio.h>
# include <string.h>
# include <stdlib.h>
#endif
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/param.h>
#if defined(_KERNEL) || defined(KERNEL)
#include <sys/systm.h>
#endif
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
#include "ip_fil_compat.h"
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

int	flush_nattable __P((void));
int	clear_natlist __P((void));
nat_t	*nat_new __P((ipnat_t *, ip_t *, int, u_short, int));

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
int
nat_ioctl(data, cmd, mode)
	caddr_t data;
	int cmd, mode;
{
	register ipnat_t *nat, *n, **np;
	ipnat_t natd;
	int error = 0, ret;

	/*
	 * For add/delete, look to see if the NAT entry is already present
	 */
	MUTEX_ENTER(&ipf_nat);
	if ((cmd == SIOCADNAT) || (cmd == SIOCRMNAT)) {
		IRCOPY(data, &natd, sizeof(natd));
		nat = &natd;
		for (np = &nat_list; (n = *np); np = &n->in_next)
			if (!bcmp((char *)&nat->in_port, (char *)&n->in_port,
					IPN_CMPSIZ))
				break;
	}

	switch (cmd)
	{
	case SIOCADNAT :
		if (!(mode & FWRITE)) {
			error = EPERM;
			break;
		}
		if (n) {
			error = EEXIST;
			break;
		}
		if (!(n = (ipnat_t *)KMALLOC(sizeof(*n)))) {
			error = ENOMEM;
			break;
		}
		IRCOPY((char *)data, (char *)n, sizeof(*n));
		n->in_ifp = (void *)GETUNIT(n->in_ifname);
		n->in_next = *np;
		n->in_space = ~(0xffffffff & ntohl(n->in_outmsk));
		n->in_space -= 2; /* lose 2: broadcast + network address */
		if (n->in_inmsk != 0xffffffff)
			n->in_nip = ntohl(n->in_outip) + 1;
		else
			n->in_nip = ntohl(n->in_outip);
		if (n->in_redir == NAT_MAP)
			n->in_pnext = ntohs(n->in_pmin);
		/* Otherwise, these fields are preset */
		*np = n;
		break;
	case SIOCRMNAT :
		if (!(mode & FWRITE)) {
			error = EPERM;
			break;
		}
		if (!n) {
			error = ESRCH;
			break;
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
	case SIOCGNATL :
	    {
		natlookup_t nl;
		nat_t	*na;

		IRCOPY((char *)data, (char *)&nl, sizeof(nl));
		if ((na = nat_lookupredir(&nl))) {
			nl.nl_inip = na->nat_outip;
			nl.nl_inport = na->nat_outport;
			IWCOPY((char *)&nl, (char *)data, sizeof(nl));
		} else
			error = ESRCH;
		break;
	    }
	case SIOCFLNAT :
		if (!(mode & FWRITE)) {
			error = EPERM;
			break;
		}
		ret = flush_nattable();
		IWCOPY((caddr_t)&ret, data, sizeof(ret));
		break;
	case SIOCCNATL :
		if (!(mode & FWRITE)) {
			error = EPERM;
			break;
		}
		ret = clear_natlist();
		IWCOPY((caddr_t)&ret, data, sizeof(ret));
		break;
	}
	MUTEX_EXIT(&ipf_nat);
	return error;
}


/*
 * flush_nattable - clear the NAT table of all mapping entries.
 */
int
flush_nattable()
{
	nat_t *nat, **natp;
	int i, j = 0;

	for (natp = &nat_table[0][0], i = NAT_SIZE - 1; i >= 0; i--, natp++)
		while ((nat = *natp)) {
			*natp = nat->nat_next;
			KFREE((caddr_t)nat);
			j++;
		}

	for (natp = &nat_table[1][0], i = NAT_SIZE - 1; i >= 0; i--, natp++)
		while ((nat = *natp)) {
			*natp = nat->nat_next;
			KFREE((caddr_t)nat);
			j++;
		}
	return j;
}


/*
 * clear_natlist - delete all entries in the active NAT mapping list.
 */
int
clear_natlist()
{
	register ipnat_t *n, **np;
	int i = 0;

	for (np = &nat_list; (n = *np); i++) {
		*np = n->in_next;
		KFREE(n);
	}
	return i;
}


/*
 * Create a new NAT table entry.
 */
nat_t *
nat_new(np, ip, hlen, flags, direction)
	ipnat_t *np;
	ip_t *ip;
	int hlen;
	u_short flags;
	int direction;
{
	register u_long sum1, sum2, sumd;
	u_short port = 0, sport = 0, dport = 0, nport = 0;
	struct in_addr in;
	tcphdr_t *tcp;
	nat_t *nat, **natp;

	if (flags) {
		tcp = (tcphdr_t *)((char *)ip + hlen);
		sport = tcp->th_sport;
		dport = tcp->th_dport;
	}

	/* Give me a new nat */
	if (!(nat = (nat_t *)KMALLOC(sizeof(*nat))))
		return NULL;

	/*
	 * Search the current table for a match.
	 */
	if (direction == NAT_OUTBOUND) {
		/*
		 * If it's an outbound packet which doesn't match any existing
		 * record, then create a new port
		 */
		do {
			in.s_addr = np->in_nip;
			if (np->in_flags & IPN_TCPUDP) {
				port = htons(np->in_pnext++);
				if (np->in_pnext >= ntohs(np->in_pmax)) {
					np->in_pnext = ntohs(np->in_pmin);
					np->in_space--;
					if (np->in_outmsk != 0xffffffff)
						np->in_nip++;
				}
			} else {
				np->in_space--;
				if (np->in_outmsk != 0xffffffff)
					np->in_nip++;
			}
			if ((np->in_nip & ntohl(np->in_outmsk)) >
					ntohl(np->in_outip))
				np->in_nip = ntohl(np->in_outip) + 1;
		} while (nat_lookupinip(in, sport));

		/* Setup the NAT table */
		nat->nat_use = 0;
		nat->nat_inip = ip->ip_src;
		nat->nat_outip.s_addr = htonl(in.s_addr);

		sum1 = (ntohl(ip->ip_src.s_addr) & 0xffff) +
			(ntohl(ip->ip_src.s_addr) >> 16) + ntohs(sport);

		/* Do it twice */
		sum1 = (sum1 & 0xffff) + (sum1 >> 16);
		sum1 = (sum1 & 0xffff) + (sum1 >> 16);

		sum2 = (in.s_addr & 0xffff) + (in.s_addr >> 16) + ntohs(port);

		/* Do it twice */
		sum2 = (sum2 & 0xffff) + (sum2 >> 16);
		sum2 = (sum2 & 0xffff) + (sum2 >> 16);

		if (sum1 > sum2)
			sum2--; /* Because ~1 == -2, We really need ~1 == -1 */
		sumd = sum2 - sum1;
		sumd = (sumd & 0xffff) + (sumd >> 16);
		nat->nat_sumd = (sumd & 0xffff) + (sumd >> 16);

		if (sport) {
			nat->nat_inport = sport;
			nat->nat_outport = port;
		} else {
			nat->nat_inport = 0;
			nat->nat_outport = 0;
		}
	} else {

		/*
		 * Otherwise, it's an inbound packet. Most likely, we don't
		 * want to rewrite source ports and source addresses. Instead,
		 * we want to rewrite to a fixed internal address and fixed
		 * internal port.
		 */
		in.s_addr = ntohl(np->in_inip);
		nport = np->in_pnext;

		nat->nat_use = 0;
		nat->nat_inip.s_addr = htonl(in.s_addr);
		nat->nat_outip = ip->ip_dst;
		nat->nat_oip = ip->ip_src;

		sum1 = (ntohl(ip->ip_dst.s_addr) & 0xffff) +
			(ntohl(ip->ip_dst.s_addr) >> 16) + ntohs(dport);

		/* Do it twice */
		sum1 = (sum1 & 0xffff) + (sum1 >> 16);
		sum1 = (sum1 & 0xffff) + (sum1 >> 16);

		sum2 = (in.s_addr & 0xffff) + (in.s_addr >> 16) + ntohs(nport);

		/* Do it twice */
		sum2 = (sum2 & 0xffff) + (sum2 >> 16);
		sum2 = (sum2 & 0xffff) + (sum2 >> 16);

		if (sum2 > sum1)
			sum1--; /* Because ~1 == -2, We really need ~1 == -1 */
		sumd = (sum1 - sum2);
		sumd = (sumd & 0xffff) + (sumd >> 16);
		nat->nat_sumd = (sumd & 0xffff) + (sumd >> 16);

		if (dport) {
			nat->nat_inport = nport;
			nat->nat_outport = dport;
			nat->nat_oport = sport;
		} else {
			nat->nat_inport = 0;
			nat->nat_outport = 0;
		}
	}

	in.s_addr = htonl(in.s_addr);
	natp = &nat_table[0][nat->nat_inip.s_addr % NAT_SIZE];
	nat->nat_next = *natp;
	*natp = nat;
	nat->nat_use++;
	natp = &nat_table[1][nat->nat_outip.s_addr % NAT_SIZE];
	nat->nat_next = *natp;
	*natp = nat;
	nat->nat_use++;
	if (direction == NAT_REDIRECT) {
		ip->ip_src = in;
		if (flags)
			tcp->th_sport = htons(port);
	} else {
		ip->ip_dst = in;
		if (flags)
			tcp->th_dport = htons(nport);
	}

	nat_stats.ns_added++;
	nat_inuse++;
	return nat;
}


/*
 * NB: these lookups don't lock access to the list, it assume it has already
 * been done!
 */
nat_t *
nat_lookupredir(np)
	natlookup_t *np;
{
	nat_t *nat;

	nat = nat_table[0][np->nl_inip.s_addr % NAT_SIZE];
	for (; nat; nat = nat->nat_next)
		if ((nat->nat_inip.s_addr == np->nl_inip.s_addr) &&
		    (nat->nat_oip.s_addr == np->nl_outip.s_addr) &&
		    (np->nl_inport == nat->nat_inport) &&
		    (np->nl_outport == nat->nat_oport))
			return nat;
	return NULL;
}


nat_t *
nat_lookupinip(ipaddr, sport)
	struct in_addr ipaddr;
	u_short sport;
{
	nat_t *nat;

	nat = nat_table[0][ipaddr.s_addr % NAT_SIZE];

	for (; nat; nat = nat->nat_next)
		if (nat->nat_inip.s_addr == ipaddr.s_addr) {
			if (nat->nat_inport && (sport != nat->nat_inport))
				continue;
			return nat;
		}
	return NULL;
}


nat_t *
nat_lookupoutip(np, ip, tcp)
	register ipnat_t *np;
	ip_t *ip;
	tcphdr_t *tcp;
{
	struct in_addr ipaddr;
	u_short	port = tcp->th_dport;
	nat_t *nat;

	ipaddr.s_addr = ip->ip_dst.s_addr;
	nat = nat_table[1][ipaddr.s_addr % NAT_SIZE];

	if (np->in_redir == NAT_MAP) {
		for (; nat; nat = nat->nat_next)
			if (nat->nat_outip.s_addr == ipaddr.s_addr &&
			    (!nat->nat_outport || (port == nat->nat_outport)))
				return nat;
	} else
		for (; nat; nat = nat->nat_next)
			if (nat->nat_outip.s_addr == ipaddr.s_addr &&
			    nat->nat_oip.s_addr == ip->ip_src.s_addr &&
			    port == nat->nat_outport &&
			    tcp->th_sport == nat->nat_oport)
				return nat;
	return NULL;
}


/*
 * Packets going out on the external interface go through this.
 * Here, the source address requires alteration, if anything.
 */
void
ip_natout(ip, hlen, fin)
	ip_t *ip;
	int hlen;
	fr_info_t *fin;
{
	register ipnat_t *np;
	register u_long ipa;
	register u_long sum1;
	tcphdr_t *tcp;
	nat_t *nat;
	u_short nflags = 0, sport = 0;
	struct ifnet *ifp = fin->fin_ifp;

	if (!(ip->ip_off & 0x1fff) && !(fin->fin_fi.fi_fl & FI_SHORT)) {
		if (ip->ip_p == IPPROTO_TCP)
			nflags = IPN_TCP;
		else if (ip->ip_p == IPPROTO_UDP)
			nflags = IPN_UDP;
	}
	if (nflags) {
		tcp = (tcphdr_t *)fin->fin_dp;
		sport = tcp->th_sport;
	}

	ipa = ip->ip_src.s_addr;

	MUTEX_ENTER(&ipf_nat);
	for (np = nat_list; np; np = np->in_next)
		if ((np->in_ifp == ifp) && np->in_space &&
		    (!np->in_flags || (np->in_flags & nflags)) &&
		    ((ipa & np->in_inmsk) == np->in_inip) &&
		    (np->in_redir == NAT_MAP ||
		     np->in_pnext == sport)) {
			/*
			 * If there is no current entry in the nat table for
			 * this IP#, create one for it.
			 */
			if (!(nat = nat_lookupinip(ip->ip_src, sport))) {
				if (np->in_redir == NAT_REDIRECT)
					continue;
				/*
				 * if it's a redirection, then we don't want
				 * to create new outgoing port stuff.
				 * Redirections are only for incoming
				 * connections.
				 */
				if (!(nat = nat_new(np, ip, hlen,
						    nflags & np->in_flags,
						    NAT_OUTBOUND)))
					break;
			} else
				ip->ip_src = nat->nat_outip;

			nat->nat_age = 1200;	/* 5 mins */

			/*
			 * Fix up checksums, not by recalculating them, but
			 * simply computing adjustments.
			 */
			if (nflags && !(ip->ip_off & 0x1fff) &&
			    !(fin->fin_fi.fi_fl & FI_SHORT)) {
				u_short *sp;
				u_short sumshort;

				if (nat->nat_outport)
					tcp->th_sport = nat->nat_outport;

				if (ip->ip_p == IPPROTO_TCP) {
					sp = &tcp->th_sum;

					sum1 = (~ntohs(*sp)) & 0xffff;

					sum1 += nat->nat_sumd;

					sum1 = (sum1 >> 16) + (sum1 & 0xffff);
					/* Again */
					sum1 = (sum1 >> 16) + (sum1 & 0xffff);
					sumshort = ~(u_short)sum1;
					*sp = htons(sumshort);

				} else if (ip->ip_p == IPPROTO_UDP) {
					udphdr_t *udp = (udphdr_t *)tcp;

					sp = &udp->uh_sum;

					if (udp->uh_sum) {
						sum1 = (~ntohs(*sp)) & 0xffff;
						sum1 += nat->nat_sumd;
						sum1 = (sum1 >> 16) +
						       (sum1 & 0xffff);
						/* Again */
						sum1 = (sum1 >> 16) +
						       (sum1 & 0xffff);
						sumshort = ~(u_short)sum1;
						*sp = htons(sumshort);
					}
				}
			}
			nat_stats.ns_mapped[1]++;
			MUTEX_EXIT(&ipf_nat);
			return;
		}
	MUTEX_EXIT(&ipf_nat);
	return;
}


/*
 * Packets coming in from the external interface go through this.
 * Here, the destination address requires alteration, if anything.
 */
void
ip_natin(ip, hlen, fin)
	ip_t *ip;
	int hlen;
	fr_info_t *fin;
{
	register ipnat_t *np;
	register struct in_addr in;
	register u_long sum1;
	struct ifnet *ifp = fin->fin_ifp;
	tcphdr_t *tcp;
	u_short port = 0, nflags;
	nat_t *nat;

	if (!(ip->ip_off & 0x1fff) && !(fin->fin_fi.fi_fl & FI_SHORT)) {
		if (ip->ip_p == IPPROTO_TCP)
			nflags = IPN_TCP;
		else if (ip->ip_p == IPPROTO_UDP)
			nflags = IPN_UDP;
	}
	if (nflags) {
		tcp = (tcphdr_t *)((char *)ip + hlen);
		port = tcp->th_dport;
	}

	in = ip->ip_dst;

	MUTEX_ENTER(&ipf_nat);
	for (np = nat_list; np; np = np->in_next)
		if ((np->in_ifp == ifp) &&
		    (!np->in_flags || (nflags & np->in_flags)) &&
		    ((in.s_addr & np->in_outmsk) == np->in_outip) &&
		    (np->in_redir == NAT_MAP || np->in_pmin == port)) {
			if (!(nat = nat_lookupoutip(np, ip, tcp))) {
				if (np->in_redir == NAT_MAP)
					continue;
				else {
					/*
					 * If this rule (np) is a redirection,
					 * rather than a mapping, then do a
					 * nat_new. Otherwise, if it's just a
					 * mapping, do a continue;
					 */
					nflags &= np->in_flags;
					if (!(nat = nat_new(np, ip, hlen,
							    nflags,
							    NAT_INBOUND)))
						break;
				}
			}
			nat->nat_age = 1200;

			ip->ip_dst = nat->nat_inip;

			/*
			 * Fix up checksums, not by recalculating them, but
			 * simply computing adjustments.
			 */
			if (nflags && !(ip->ip_off & 0x1fff) &&
			    !(fin->fin_fi.fi_fl & FI_SHORT)) {
				u_short	*sp;
				u_short sumshort;

				if (nat->nat_inport)
					tcp->th_dport = nat->nat_inport;

				if (ip->ip_p == IPPROTO_TCP) {
					sp = &tcp->th_sum;

					sum1 = (~ntohs(*sp)) & 0xffff;
					sum1 += ~nat->nat_sumd & 0xffff;
					sum1 = (sum1 >> 16) + (sum1 & 0xffff);
					/* Again */
					sum1 = (sum1 >> 16) + (sum1 & 0xffff);
					sumshort = ~(u_short)sum1;
					*sp = htons(sumshort);
				} else if (ip->ip_p == IPPROTO_UDP) {
					udphdr_t *udp = (udphdr_t *)tcp;

					sp = &udp->uh_sum;

					if (udp->uh_sum) {
						sum1 = (~ntohs(*sp)) & 0xffff;
						sum1+= ~nat->nat_sumd & 0xffff;
						sum1 = (sum1 >> 16) +
						       (sum1 & 0xffff);
						/* Again */
						sum1 = (sum1 >> 16) +
						       (sum1 & 0xffff);
						sumshort = ~(u_short)sum1;
						*sp = htons(sumshort);
					}
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
void
ip_natunload()
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
void
ip_natexpire()
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
