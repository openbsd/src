/*	$OpenBSD: ns.c,v 1.5 2003/12/10 07:22:44 itojun Exp $	*/
/*	$NetBSD: ns.c,v 1.10 1996/02/13 22:13:49 christos Exp $	*/

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
 *	@(#)ns.c	8.2 (Berkeley) 11/15/93
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/ioctl.h>
#include <sys/protosw.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/socketvar.h>

#include <net/if.h>
#include <net/route.h>

#include <netns/ns.h>
#include <netns/ns_if.h>
#include <netns/ns_var.h>

#ifdef NS

struct ns_ifaddrhead ns_ifaddr;
int ns_interfaces;
extern struct sockaddr_ns ns_netmask, ns_hostmask;

/*
 * Generic internet control operations (ioctl's).
 */
/* ARGSUSED */
int
ns_control(so, cmd, data, ifp)
	struct socket *so;
	u_long cmd;
	caddr_t data;
	struct ifnet *ifp;
{
	struct ifreq *ifr = (struct ifreq *)data;
	struct ns_aliasreq *ifra = (struct ns_aliasreq *)data;
	struct ns_ifaddr *ia;
	int dstIsNew, hostIsNew, error = 0;

	/*
	 * Find address for this interface, if it exists.
	 */
	if (ifp == 0)
		return (EADDRNOTAVAIL);
	for (ia = ns_ifaddr.tqh_first; ia != 0; ia = ia->ia_list.tqe_next)
		if (ia->ia_ifp == ifp)
			break;

	switch (cmd) {

	case SIOCGIFADDR:
		if (ia == 0)
			return (EADDRNOTAVAIL);
		*satosns(&ifr->ifr_addr) = ia->ia_addr;
		return (0);


	case SIOCGIFBRDADDR:
		if (ia == 0)
			return (EADDRNOTAVAIL);
		if ((ifp->if_flags & IFF_BROADCAST) == 0)
			return (EINVAL);
		*satosns(&ifr->ifr_dstaddr) = ia->ia_broadaddr;
		return (0);

	case SIOCGIFDSTADDR:
		if (ia == 0)
			return (EADDRNOTAVAIL);
		if ((ifp->if_flags & IFF_POINTOPOINT) == 0)
			return (EINVAL);
		*satosns(&ifr->ifr_dstaddr) = ia->ia_dstaddr;
		return (0);
	}

	if ((so->so_state & SS_PRIV) == 0)
		return (EPERM);

	switch (cmd) {
	case SIOCAIFADDR:
	case SIOCDIFADDR:
		if (ifra->ifra_addr.sns_family == AF_NS)
		    for (; ia; ia = ia->ia_list.tqe_next) {
			if (ia->ia_ifp == ifp  &&
			    ns_neteq(ia->ia_addr.sns_addr,
				  ifra->ifra_addr.sns_addr))
			    break;
		    }
		if (cmd == SIOCDIFADDR && ia == 0)
			return (EADDRNOTAVAIL);
		/* FALLTHROUGH */

	case SIOCSIFADDR:
	case SIOCSIFDSTADDR:
		if (ia == 0) {
			ia = (struct ns_ifaddr *)
				malloc(sizeof *ia, M_IFADDR, M_WAITOK);
			bzero((caddr_t)ia, sizeof(*ia));
			TAILQ_INSERT_TAIL(&ns_ifaddr, ia, ia_list);
			TAILQ_INSERT_TAIL(&ifp->if_addrlist, (struct ifaddr *)ia,
			    ifa_list);
			ia->ia_ifa.ifa_addr = snstosa(&ia->ia_addr);
			ia->ia_ifa.ifa_netmask = snstosa(&ns_netmask);
			ia->ia_ifa.ifa_dstaddr = snstosa(&ia->ia_dstaddr);
			ia->ia_ifp = ifp;
			if (ifp->if_flags & IFF_BROADCAST) {
				ia->ia_broadaddr.sns_family = AF_NS;
				ia->ia_broadaddr.sns_len = sizeof(ia->ia_addr);
				ia->ia_broadaddr.sns_addr.x_host = ns_broadhost;
			}
			ns_interfaces++;
		}
	}

	switch (cmd) {
	case SIOCSIFDSTADDR:
		if ((ifp->if_flags & IFF_POINTOPOINT) == 0)
			return (EINVAL);
		if (ia->ia_flags & IFA_ROUTE) {
			rtinit(&(ia->ia_ifa), (int)RTM_DELETE, RTF_HOST);
			ia->ia_flags &= ~IFA_ROUTE;
		}
		if (ifp->if_ioctl) {
			error = (*ifp->if_ioctl)(ifp, SIOCSIFDSTADDR,
						 (caddr_t)ia);
			if (error)
				return (error);
		}
		*snstosa(&ia->ia_dstaddr) = ifr->ifr_dstaddr;
		return (0);

	case SIOCSIFADDR:
		return (ns_ifinit(ifp, ia, satosns(&ifr->ifr_addr), 1));

	case SIOCDIFADDR:
		ns_ifscrub(ifp, ia);
		TAILQ_REMOVE(&ifp->if_addrlist, (struct ifaddr *)ia, ifa_list);
		TAILQ_REMOVE(&ns_ifaddr, ia, ia_list);
		IFAFREE((&ia->ia_ifa));
		if (0 == --ns_interfaces) {
			/*
			 * We reset to virginity and start all over again
			 */
			ns_thishost = ns_zerohost;
		}
		return (0);
	
	case SIOCAIFADDR:
		dstIsNew = 0; hostIsNew = 1;
		if (ia->ia_addr.sns_family == AF_NS) {
			if (ifra->ifra_addr.sns_len == 0) {
				ifra->ifra_addr = ia->ia_addr;
				hostIsNew = 0;
			} else if (ns_neteq(ifra->ifra_addr.sns_addr,
					 ia->ia_addr.sns_addr))
				hostIsNew = 0;
		}
		if ((ifp->if_flags & IFF_POINTOPOINT) &&
		    (ifra->ifra_dstaddr.sns_family == AF_NS)) {
			if (hostIsNew == 0)
				ns_ifscrub(ifp, ia);
			ia->ia_dstaddr = ifra->ifra_dstaddr;
			dstIsNew  = 1;
		}
		if (ifra->ifra_addr.sns_family == AF_NS &&
					    (hostIsNew || dstIsNew))
			error = ns_ifinit(ifp, ia, &ifra->ifra_addr, 0);
		else
			error = 0;
		return (error);

	default:
		if (ifp->if_ioctl == 0)
			return (EOPNOTSUPP);
		return ((*ifp->if_ioctl)(ifp, cmd, data));
	}
}

/*
* Delete any previous route for an old address.
*/
void
ns_ifscrub(ifp, ia)
	struct ifnet *ifp;
	struct ns_ifaddr *ia; 
{
	if (ia->ia_flags & IFA_ROUTE) {
		if (ifp->if_flags & IFF_POINTOPOINT) {
			rtinit(&(ia->ia_ifa), (int)RTM_DELETE, RTF_HOST);
		} else
			rtinit(&(ia->ia_ifa), (int)RTM_DELETE, 0);
		ia->ia_flags &= ~IFA_ROUTE;
	}
}
/*
 * Initialize an interface's internet address
 * and routing table entry.
 */
int
ns_ifinit(ifp, ia, sns, scrub)
	struct ifnet *ifp;
	struct ns_ifaddr *ia;
	struct sockaddr_ns *sns;
	int scrub;
{
	struct sockaddr_ns oldaddr;
	union ns_host *h = &ia->ia_addr.sns_addr.x_host;
	int s = splimp(), error;

	/*
	 * Set up new addresses.
	 */
	oldaddr = ia->ia_addr;
	ia->ia_addr = *sns;
	/*
	 * The convention we shall adopt for naming is that
	 * a supplied address of zero means that "we don't care".
	 * if there is a single interface, use the address of that
	 * interface as our 6 byte host address.
	 * if there are multiple interfaces, use any address already
	 * used.
	 *
	 * Give the interface a chance to initialize
	 * if this is its first address,
	 * and to validate the address if necessary.
	 */
	if (ns_hosteqnh(ns_thishost, ns_zerohost)) {
		if (ifp->if_ioctl &&
		     (error = (*ifp->if_ioctl)(ifp, SIOCSIFADDR,
		    (caddr_t)ia)))
			goto bad;
		ns_thishost = *h;
	} else if (ns_hosteqnh(sns->sns_addr.x_host, ns_zerohost)
	    || ns_hosteqnh(sns->sns_addr.x_host, ns_thishost)) {
		*h = ns_thishost;
		if (ifp->if_ioctl &&
		     (error = (*ifp->if_ioctl)(ifp, SIOCSIFADDR,
		    (caddr_t)ia)))
			goto bad;
		if (!ns_hosteqnh(ns_thishost, *h)) {
			error = EINVAL;
			goto bad;
		}
	} else {
		error = EINVAL;
		goto bad;
	}
	ia->ia_ifa.ifa_metric = ifp->if_metric;
	/*
	 * Add route for the network.
	 */
	if (scrub) {
		ia->ia_ifa.ifa_addr = snstosa(&oldaddr);
		ns_ifscrub(ifp, ia);
		ia->ia_ifa.ifa_addr = snstosa(&ia->ia_addr);
	}
	if (ifp->if_flags & IFF_POINTOPOINT)
		rtinit(&(ia->ia_ifa), (int)RTM_ADD, RTF_HOST|RTF_UP);
	else {
		ia->ia_broadaddr.sns_addr.x_net = ia->ia_addr.sns_addr.x_net;
		rtinit(&(ia->ia_ifa), (int)RTM_ADD, RTF_UP);
	}
	ia->ia_flags |= IFA_ROUTE;
	splx(s);
	return (0);
bad:
	ia->ia_addr = oldaddr;
	splx(s);
	return (error);
}

/*
 * Return address info for specified internet network.
 */
struct ns_ifaddr *
ns_iaonnetof(dst)
	struct ns_addr *dst;
{
	struct ns_ifaddr *ia;
	struct ns_addr *compare;
	struct ifnet *ifp;
	struct ns_ifaddr *ia_maybe = 0;
	union ns_net net = dst->x_net;

	for (ia = ns_ifaddr.tqh_first; ia != 0; ia = ia->ia_list.tqe_next) {
		if ((ifp = ia->ia_ifp) != NULL) {
			if (ifp->if_flags & IFF_POINTOPOINT) {
				compare = &satons_addr(ia->ia_dstaddr);
				if (ns_hosteq(*dst, *compare))
					return (ia);
				if (ns_neteqnn(net, ia->ia_addr.sns_addr.x_net))
					ia_maybe = ia;
			} else {
				if (ns_neteqnn(net, ia->ia_addr.sns_addr.x_net))
					return (ia);
			}
		}
	}
	return (ia_maybe);
}
#endif
