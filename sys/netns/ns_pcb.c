/*	$OpenBSD: ns_pcb.c,v 1.6 2003/12/10 07:22:44 itojun Exp $	*/
/*	$NetBSD: ns_pcb.c,v 1.10 1996/03/27 14:44:14 christos Exp $	*/

/*
 * Copyright (c) 1984, 1985, 1986, 1987, 1993
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
 *
 *	@(#)ns_pcb.c	8.1 (Berkeley) 6/10/93
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>

#include <net/if.h>
#include <net/route.h>

#include <netns/ns.h>
#include <netns/ns_if.h>
#include <netns/ns_pcb.h>
#include <netns/ns_var.h>

struct	ns_addr zerons_addr;

int
ns_pcballoc(so, head)
	struct socket *so;
	struct nspcb *head;
{
	struct nspcb *nsp;

	nsp = malloc(sizeof(*nsp), M_PCB, M_NOWAIT);
	if (nsp == 0)
		return (ENOBUFS);
	bzero((caddr_t)nsp, sizeof(*nsp));
	nsp->nsp_socket = so;
	insque(nsp, head);
	so->so_pcb = nsp;
	return (0);
}
	
int
ns_pcbbind(nsp, nam)
	struct nspcb *nsp;
	struct mbuf *nam;
{
	struct sockaddr_ns *sns;
	u_short lport = 0;

	if (nsp->nsp_lport || !ns_nullhost(nsp->nsp_laddr))
		return (EINVAL);
	if (nam == 0)
		goto noname;
	sns = mtod(nam, struct sockaddr_ns *);
	if (nam->m_len != sizeof (*sns))
		return (EINVAL);
	if (!ns_nullhost(sns->sns_addr)) {
		int tport = sns->sns_port;

		sns->sns_port = 0;		/* yech... */
		if (ifa_ifwithaddr(snstosa(sns)) == 0)
			return (EADDRNOTAVAIL);
		sns->sns_port = tport;
	}
	lport = sns->sns_port;
	if (lport) {
		u_short aport = ntohs(lport);

		if (aport < NSPORT_RESERVED &&
		    (nsp->nsp_socket->so_state & SS_PRIV) == 0)
			return (EACCES);
		if (ns_pcblookup(&zerons_addr, lport, 0))
			return (EADDRINUSE);
	}
	nsp->nsp_laddr = sns->sns_addr;
noname:
	if (lport == 0)
		do {
			if (nspcb.nsp_lport++ < NSPORT_RESERVED)
				nspcb.nsp_lport = NSPORT_RESERVED;
			lport = htons(nspcb.nsp_lport);
		} while (ns_pcblookup(&zerons_addr, lport, 0));
	nsp->nsp_lport = lport;
	return (0);
}

/*
 * Connect from a socket to a specified address.
 * Both address and port must be specified in argument sns.
 * If don't have a local address for this socket yet,
 * then pick one.
 */
int
ns_pcbconnect(nsp, nam)
	struct nspcb *nsp;
	struct mbuf *nam;
{
	struct ns_ifaddr *ia;
	struct sockaddr_ns *sns = mtod(nam, struct sockaddr_ns *);
	struct ns_addr *dst;
	struct route *ro;
	struct ifnet *ifp;

	if (nam->m_len != sizeof (*sns))
		return (EINVAL);
	if (sns->sns_family != AF_NS)
		return (EAFNOSUPPORT);
	if (sns->sns_port==0 || ns_nullhost(sns->sns_addr))
		return (EADDRNOTAVAIL);
	/*
	 * If we haven't bound which network number to use as ours,
	 * we will use the number of the outgoing interface.
	 * This depends on having done a routing lookup, which
	 * we will probably have to do anyway, so we might
	 * as well do it now.  On the other hand if we are
	 * sending to multiple destinations we may have already
	 * done the lookup, so see if we can use the route
	 * from before.  In any case, we only
	 * chose a port number once, even if sending to multiple
	 * destinations.
	 */
	ro = &nsp->nsp_route;
	dst = &satons_addr(ro->ro_dst);
	if (nsp->nsp_socket->so_options & SO_DONTROUTE)
		goto flush;
	if (!ns_neteq(nsp->nsp_lastdst, sns->sns_addr))
		goto flush;
	if (!ns_hosteq(nsp->nsp_lastdst, sns->sns_addr)) {
		if (ro->ro_rt && ! (ro->ro_rt->rt_flags & RTF_HOST)) {
			/* can patch route to avoid rtalloc */
			*dst = sns->sns_addr;
		} else {
	flush:
			if (ro->ro_rt)
				RTFREE(ro->ro_rt);
			ro->ro_rt = (struct rtentry *)0;
			nsp->nsp_laddr.x_net = ns_zeronet;
		}
	}/* else cached route is ok; do nothing */
	nsp->nsp_lastdst = sns->sns_addr;
	if ((nsp->nsp_socket->so_options & SO_DONTROUTE) == 0 && /*XXX*/
	    (ro->ro_rt == (struct rtentry *)0 ||
	     ro->ro_rt->rt_ifp == (struct ifnet *)0)) {
		    /* No route yet, so try to acquire one */
		    ro->ro_dst.sa_family = AF_NS;
		    ro->ro_dst.sa_len = sizeof(ro->ro_dst);
		    *dst = sns->sns_addr;
		    dst->x_port = 0;
		    rtalloc(ro);
	}
	if (ns_neteqnn(nsp->nsp_laddr.x_net, ns_zeronet)) {
		/* 
		 * If route is known or can be allocated now,
		 * our src addr is taken from the i/f, else punt.
		 */

		ia = 0;
		/*
		 * If we found a route, use the address
		 * corresponding to the outgoing interface
		 */
		if (ro->ro_rt && (ifp = ro->ro_rt->rt_ifp))
			for (ia = ns_ifaddr.tqh_first; ia != 0;
			    ia = ia->ia_list.tqe_next)
				if (ia->ia_ifp == ifp)
					break;
		if (ia == 0) {
			u_short fport = sns->sns_addr.x_port;
			sns->sns_addr.x_port = 0;
			ia = (struct ns_ifaddr *)
				ifa_ifwithdstaddr(snstosa(sns));
			sns->sns_addr.x_port = fport;
			if (ia == 0)
				ia = ns_iaonnetof(&sns->sns_addr);
			if (ia == 0)
				ia = ns_ifaddr.tqh_first;
			if (ia == 0)
				return (EADDRNOTAVAIL);
		}
		nsp->nsp_laddr.x_net = satons_addr(ia->ia_addr).x_net;
	}
	if (ns_pcblookup(&sns->sns_addr, nsp->nsp_lport, 0))
		return (EADDRINUSE);
	if (ns_nullhost(nsp->nsp_laddr)) {
		if (nsp->nsp_lport == 0)
			(void) ns_pcbbind(nsp, (struct mbuf *)0);
		nsp->nsp_laddr.x_host = ns_thishost;
	}
	nsp->nsp_faddr = sns->sns_addr;
	/* Includes nsp->nsp_fport = sns->sns_port; */
	return (0);
}

void
ns_pcbdisconnect(nsp)
	struct nspcb *nsp;
{

	nsp->nsp_faddr = zerons_addr;
	if (nsp->nsp_socket->so_state & SS_NOFDREF)
		ns_pcbdetach(nsp);
}

void
ns_pcbdetach(nsp)
	struct nspcb *nsp;
{
	struct socket *so = nsp->nsp_socket;

	so->so_pcb = 0;
	sofree(so);
	if (nsp->nsp_route.ro_rt)
		rtfree(nsp->nsp_route.ro_rt);
	remque(nsp);
	free(nsp, M_PCB);
}

void
ns_setsockaddr(nsp, nam)
	struct nspcb *nsp;
	struct mbuf *nam;
{
	struct sockaddr_ns *sns = mtod(nam, struct sockaddr_ns *);
	
	nam->m_len = sizeof (*sns);
	sns = mtod(nam, struct sockaddr_ns *);
	bzero((caddr_t)sns, sizeof (*sns));
	sns->sns_len = sizeof(*sns);
	sns->sns_family = AF_NS;
	sns->sns_addr = nsp->nsp_laddr;
}

void
ns_setpeeraddr(nsp, nam)
	struct nspcb *nsp;
	struct mbuf *nam;
{
	struct sockaddr_ns *sns = mtod(nam, struct sockaddr_ns *);
	
	nam->m_len = sizeof (*sns);
	sns = mtod(nam, struct sockaddr_ns *);
	bzero((caddr_t)sns, sizeof (*sns));
	sns->sns_len = sizeof(*sns);
	sns->sns_family = AF_NS;
	sns->sns_addr  = nsp->nsp_faddr;
}

/*
 * Pass some notification to all connections of a protocol
 * associated with address dst.  Call the
 * protocol specific routine to handle each connection.
 * Also pass an extra paramter via the nspcb. (which may in fact
 * be a parameter list!)
 */
void
ns_pcbnotify(dst, errno, notify, param)
	struct ns_addr *dst;
	long param;
	int errno;
	void (*notify)(struct nspcb *);
{
	struct nspcb *nsp, *oinp;
	int s = splimp();

	for (nsp = (&nspcb)->nsp_next; nsp != (&nspcb);) {
		if (!ns_hosteq(*dst,nsp->nsp_faddr)) {
	next:
			nsp = nsp->nsp_next;
			continue;
		}
		if (nsp->nsp_socket == 0)
			goto next;
		if (errno) 
			nsp->nsp_socket->so_error = errno;
		oinp = nsp;
		nsp = nsp->nsp_next;
		oinp->nsp_notify_param = param;
		(*notify)(oinp);
	}
	splx(s);
}

/*
 * After a routing change, flush old routing
 * and allocate a (hopefully) better one.
 */
void
ns_rtchange(nsp)
	struct nspcb *nsp;
{
	if (nsp->nsp_route.ro_rt) {
		rtfree(nsp->nsp_route.ro_rt);
		nsp->nsp_route.ro_rt = 0;
		/*
		 * A new route can be allocated the next time
		 * output is attempted.
		 */
	}
	/* SHOULD NOTIFY HIGHER-LEVEL PROTOCOLS */
}

struct nspcb *
ns_pcblookup(faddr, lport, wildp)
	struct ns_addr *faddr;
	u_short lport;
	int wildp;
{
	struct nspcb *nsp, *match = 0;
	int matchwild = 3, wildcard;
	u_short fport;

	fport = faddr->x_port;
	for (nsp = (&nspcb)->nsp_next; nsp != (&nspcb); nsp = nsp->nsp_next) {
		if (nsp->nsp_lport != lport)
			continue;
		wildcard = 0;
		if (ns_nullhost(nsp->nsp_faddr)) {
			if (!ns_nullhost(*faddr))
				wildcard++;
		} else {
			if (ns_nullhost(*faddr))
				wildcard++;
			else {
				if (!ns_hosteq(nsp->nsp_faddr, *faddr))
					continue;
				if (nsp->nsp_fport != fport) {
					if (nsp->nsp_fport != 0)
						continue;
					else
						wildcard++;
				}
			}
		}
		if (wildcard && wildp==0)
			continue;
		if (wildcard < matchwild) {
			match = nsp;
			matchwild = wildcard;
			if (wildcard == 0)
				break;
		}
	}
	return (match);
}
