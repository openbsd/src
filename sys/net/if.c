/*	$OpenBSD: if.c,v 1.29 2000/03/21 23:31:26 mickey Exp $	*/
/*	$NetBSD: if.c,v 1.35 1996/05/07 05:26:04 thorpej Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Copyright (c) 1980, 1986, 1993
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
 *
 *	@(#)if.c	8.3 (Berkeley) 1/4/94
 */

#include "bpfilter.h"
#include "bridge.h"

#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/kernel.h>
#include <sys/ioctl.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/radix.h>

#include <net/route.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/if_ether.h>
#include <netinet/igmp.h>
#ifdef MROUTING
#include <netinet/ip_mroute.h>
#endif
#endif

#ifdef INET6
#ifndef INET
#include <netinet/in.h>
#endif
#endif

#ifdef IPFILTER
#include <netinet/ip_fil_compat.h>
#include <netinet/ip_fil.h>
#include <netinet/ip_nat.h>
#endif

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#if NBRIDGE > 0
#include <net/if_bridge.h>
#endif

void	if_attachsetup __P((struct ifnet *));
int	if_detach_rtdelete __P((struct radix_node *, void *));

int	ifqmaxlen = IFQ_MAXLEN;
void	if_slowtimo __P((void *arg));

#ifdef INET6
/*
 * XXX: declare here to avoid to include many inet6 related files..
 * should be more generalized?
 */
extern void nd6_setmtu __P((struct ifnet *));
#endif 

/*
 * Network interface utility routines.
 *
 * Routines with ifa_ifwith* names take sockaddr *'s as
 * parameters.
 */
void
ifinit()
{
	register struct ifnet *ifp;

	for (ifp = ifnet.tqh_first; ifp != 0; ifp = ifp->if_list.tqe_next)
		if (ifp->if_snd.ifq_maxlen == 0)
			ifp->if_snd.ifq_maxlen = ifqmaxlen;
	if_slowtimo(NULL);
}

int if_index = 0;
struct ifaddr **ifnet_addrs = NULL;
struct ifnet **ifindex2ifnet = NULL;

/*
 * Attach an interface to the
 * list of "active" interfaces.
 */
void
if_attachsetup(ifp)
	struct ifnet *ifp;
{
	unsigned int socksize, ifasize;
	int namelen, masklen;
	register struct sockaddr_dl *sdl;
	register struct ifaddr *ifa;
	static int if_indexlim = 8;

	ifp->if_index = ++if_index;

	/*
	 * We have some arrays that should be indexed by if_index.
	 * since if_index will grow dynamically, they should grow too.
	 *	struct ifadd **ifnet_addrs
	 *	struct ifnet **ifindex2ifnet
	 */
	if (ifnet_addrs == 0 || ifindex2ifnet == 0 || if_index >= if_indexlim) {
		size_t n;
		caddr_t q;
		
		while (if_index >= if_indexlim)
			if_indexlim <<= 1;

		/* grow ifnet_addrs */
		n = if_indexlim * sizeof(ifa);
		q = (caddr_t)malloc(n, M_IFADDR, M_WAITOK);
		bzero(q, n);
		if (ifnet_addrs) {
			bcopy((caddr_t)ifnet_addrs, q, n/2);
			free((caddr_t)ifnet_addrs, M_IFADDR);
		}
		ifnet_addrs = (struct ifaddr **)q;

		/* grow ifindex2ifnet */
		n = if_indexlim * sizeof(struct ifnet *);
		q = (caddr_t)malloc(n, M_IFADDR, M_WAITOK);
		bzero(q, n);
		if (ifindex2ifnet) {
			bcopy((caddr_t)ifindex2ifnet, q, n/2);
			free((caddr_t)ifindex2ifnet, M_IFADDR);
		}
		ifindex2ifnet = (struct ifnet **)q;
	}

	ifindex2ifnet[if_index] = ifp;

	/*
	 * create a Link Level name for this device
	 */
	namelen = strlen(ifp->if_xname);
#define _offsetof(t, m) ((int)((caddr_t)&((t *)0)->m))
	masklen = _offsetof(struct sockaddr_dl, sdl_data[0]) + namelen;
	socksize = masklen + ifp->if_addrlen;
#define ROUNDUP(a) (1 + (((a) - 1) | (sizeof(long) - 1)))
	if (socksize < sizeof(*sdl))
		socksize = sizeof(*sdl);
	socksize = ROUNDUP(socksize);
	ifasize = sizeof(*ifa) + 2 * socksize;
	ifa = (struct ifaddr *)malloc(ifasize, M_IFADDR, M_WAITOK);
	bzero((caddr_t)ifa, ifasize);
	sdl = (struct sockaddr_dl *)(ifa + 1);
	sdl->sdl_len = socksize;
	sdl->sdl_family = AF_LINK;
	bcopy(ifp->if_xname, sdl->sdl_data, namelen);
	sdl->sdl_nlen = namelen;
	sdl->sdl_index = ifp->if_index;
	sdl->sdl_type = ifp->if_type;
	ifnet_addrs[if_index] = ifa;
	ifa->ifa_ifp = ifp;
	ifa->ifa_rtrequest = link_rtrequest;
	TAILQ_INSERT_HEAD(&ifp->if_addrlist, ifa, ifa_list);
	ifa->ifa_addr = (struct sockaddr *)sdl;
	sdl = (struct sockaddr_dl *)(socksize + (caddr_t)sdl);
	ifa->ifa_netmask = (struct sockaddr *)sdl;
	sdl->sdl_len = masklen;
	while (namelen != 0)
		sdl->sdl_data[--namelen] = 0xff;
}

void
if_attachhead(ifp)
	struct ifnet *ifp;
{
	if (if_index == 0)
		TAILQ_INIT(&ifnet);
	TAILQ_INIT(&ifp->if_addrlist);
	TAILQ_INSERT_HEAD(&ifnet, ifp, if_list);
	if_attachsetup(ifp);
}

void
if_attach(ifp)
	struct ifnet *ifp;
{
	if (if_index == 0)
		TAILQ_INIT(&ifnet);
	TAILQ_INIT(&ifp->if_addrlist);
	TAILQ_INSERT_TAIL(&ifnet, ifp, if_list);
	if_attachsetup(ifp);
}

/*
 * Delete a route if it has a specific interface for output.
 * This function complies to the rn_walktree callback API.
 */
int
if_detach_rtdelete(rn, vifp)
	struct radix_node *rn;
	void *vifp;
{
	struct ifnet *ifp = vifp;
	struct rtentry *rt = (struct rtentry *)rn;

	if (rt->rt_ifp == ifp)
		rtrequest(RTM_DELETE, rt_key(rt), rt->rt_gateway, rt_mask(rt),
		    0, NULL);

	/*
	 * XXX There should be no need to check for rt_ifa belonging to this
	 * interface, because then rt_ifp is set, right?
	 */

	return (0);
}

/*
 * Detach an interface from everything in the kernel.  Also deallocate
 * private resources.
 * XXX So far only the INET protocol family has been looked over
 * wrt resource usage that needs to be decoupled.
 */
void
if_detach(ifp)
	struct ifnet *ifp;
{
	struct ifaddr *ifa;
	int i, s = splimp();
	struct radix_node_head *rnh;

#if NBRIDGE > 0
	/* Remove the interface from any bridge it is part of.  */
	if (ifp->if_bridge)
		bridge_ifdetach(ifp);
#endif

#if NBPFILTER > 0
	/* If there is a bpf device attached, detach from it.  */
	if (ifp->if_bpf)
		bpfdetach(ifp);
#endif

	/*
	 * Find and remove all routes which is using this interface.
	 * XXX Factor out into a route.c function?
	 */
	for (i = 1; i <= AF_MAX; i++) {
		rnh = rt_tables[i];
		if (rnh)
			(*rnh->rnh_walktree)(rnh, if_detach_rtdelete, ifp);
	}

#ifdef INET
	rti_delete(ifp);
#if NETHER > 0
	myip_ifp = NULL;
#endif
#ifdef MROUTING
	vif_delete(ifp);
#endif
#endif
	/*
	 * XXX transient ifp refs?  inpcb.ip_moptions.imo_multicast_ifp?
	 * Other network stacks than INET?
	 */

	/* Remove the interface from the list of all interfaces.  */
	TAILQ_REMOVE(&ifnet, ifp, if_list);

#ifdef IPFILTER
	/* XXX More ipf & ipnat cleanup needed.  */
	nat_ifdetach(ifp);
#endif

	/* Deallocate private resources.  */
	for (ifa = TAILQ_FIRST(&ifp->if_addrlist); ifa;
	    ifa = TAILQ_FIRST(&ifp->if_addrlist)) {
		TAILQ_REMOVE(&ifp->if_addrlist, ifa, ifa_list);
#ifdef INET
		if (ifa->ifa_addr->sa_family == AF_INET)
			TAILQ_REMOVE(&in_ifaddr, (struct in_ifaddr *)ifa,
			    ia_list);
#endif
		free(ifa, M_IFADDR);
	}
	splx(s);
}

/*
 * Locate an interface based on a complete address.
 */
/*ARGSUSED*/
struct ifaddr *
ifa_ifwithaddr(addr)
	register struct sockaddr *addr;
{
	register struct ifnet *ifp;
	register struct ifaddr *ifa;

#define	equal(a1, a2) \
  (bcmp((caddr_t)(a1), (caddr_t)(a2), ((struct sockaddr *)(a1))->sa_len) == 0)
	for (ifp = ifnet.tqh_first; ifp != 0; ifp = ifp->if_list.tqe_next)
	    for (ifa = ifp->if_addrlist.tqh_first; ifa != 0; ifa = ifa->ifa_list.tqe_next) {
		if (ifa->ifa_addr->sa_family != addr->sa_family)
			continue;
		if (equal(addr, ifa->ifa_addr))
			return (ifa);
		if ((ifp->if_flags & IFF_BROADCAST) && ifa->ifa_broadaddr &&
		    /* IP6 doesn't have broadcast */
		    ifa->ifa_broadaddr->sa_len != 0 &&
		    equal(ifa->ifa_broadaddr, addr))
			return (ifa);
	}
	return (NULL);
}
/*
 * Locate the point to point interface with a given destination address.
 */
/*ARGSUSED*/
struct ifaddr *
ifa_ifwithdstaddr(addr)
	register struct sockaddr *addr;
{
	register struct ifnet *ifp;
	register struct ifaddr *ifa;

	for (ifp = ifnet.tqh_first; ifp != 0; ifp = ifp->if_list.tqe_next)
	    if (ifp->if_flags & IFF_POINTOPOINT)
		for (ifa = ifp->if_addrlist.tqh_first; ifa != 0; ifa = ifa->ifa_list.tqe_next) {
			if (ifa->ifa_addr->sa_family != addr->sa_family ||
			    ifa->ifa_dstaddr == NULL)
				continue;
			if (equal(addr, ifa->ifa_dstaddr))
				return (ifa);
	}
	return (NULL);
}

/*
 * Find an interface on a specific network.  If many, choice
 * is most specific found.
 */
struct ifaddr *
ifa_ifwithnet(addr)
	struct sockaddr *addr;
{
	register struct ifnet *ifp;
	register struct ifaddr *ifa;
	struct ifaddr *ifa_maybe = 0;
	u_int af = addr->sa_family;
	char *addr_data = addr->sa_data, *cplim;

	if (af == AF_LINK) {
	    register struct sockaddr_dl *sdl = (struct sockaddr_dl *)addr;
	    if (sdl->sdl_index && sdl->sdl_index <= if_index)
		return (ifnet_addrs[sdl->sdl_index]);
	}
	for (ifp = ifnet.tqh_first; ifp != 0; ifp = ifp->if_list.tqe_next)
		for (ifa = ifp->if_addrlist.tqh_first; ifa != 0; ifa = ifa->ifa_list.tqe_next) {
			register char *cp, *cp2, *cp3;

			if (ifa->ifa_addr->sa_family != af ||
			    ifa->ifa_netmask == 0)
				next: continue;
			cp = addr_data;
			cp2 = ifa->ifa_addr->sa_data;
			cp3 = ifa->ifa_netmask->sa_data;
			cplim = (char *)ifa->ifa_netmask +
				ifa->ifa_netmask->sa_len;
			while (cp3 < cplim)
				if ((*cp++ ^ *cp2++) & *cp3++)
				    /* want to continue for() loop */
					goto next;
			if (ifa_maybe == 0 ||
			    rn_refines((caddr_t)ifa->ifa_netmask,
			    (caddr_t)ifa_maybe->ifa_netmask))
				ifa_maybe = ifa;
		}
	return (ifa_maybe);
}

/*
 * Find an interface using a specific address family
 */
struct ifaddr *
ifa_ifwithaf(af)
	register int af;
{
	register struct ifnet *ifp;
	register struct ifaddr *ifa;

	for (ifp = ifnet.tqh_first; ifp != 0; ifp = ifp->if_list.tqe_next)
		for (ifa = ifp->if_addrlist.tqh_first; ifa != 0; ifa = ifa->ifa_list.tqe_next)
			if (ifa->ifa_addr->sa_family == af)
				return (ifa);
	return (NULL);
}

/*
 * Find an interface address specific to an interface best matching
 * a given address.
 */
struct ifaddr *
ifaof_ifpforaddr(addr, ifp)
	struct sockaddr *addr;
	register struct ifnet *ifp;
{
	register struct ifaddr *ifa;
	register char *cp, *cp2, *cp3;
	register char *cplim;
	struct ifaddr *ifa_maybe = 0;
	u_int af = addr->sa_family;

	if (af >= AF_MAX)
		return (NULL);
	for (ifa = ifp->if_addrlist.tqh_first; ifa != 0; ifa = ifa->ifa_list.tqe_next) {
		if (ifa->ifa_addr->sa_family != af)
			continue;
		ifa_maybe = ifa;
		if (ifa->ifa_netmask == 0) {
			if (equal(addr, ifa->ifa_addr) ||
			    (ifa->ifa_dstaddr && equal(addr, ifa->ifa_dstaddr)))
				return (ifa);
			continue;
		}
		cp = addr->sa_data;
		cp2 = ifa->ifa_addr->sa_data;
		cp3 = ifa->ifa_netmask->sa_data;
		cplim = ifa->ifa_netmask->sa_len + (char *)ifa->ifa_netmask;
		for (; cp3 < cplim; cp3++)
			if ((*cp++ ^ *cp2++) & *cp3)
				break;
		if (cp3 == cplim)
			return (ifa);
	}
	return (ifa_maybe);
}

/*
 * Default action when installing a route with a Link Level gateway.
 * Lookup an appropriate real ifa to point to.
 * This should be moved to /sys/net/link.c eventually.
 */
void
link_rtrequest(cmd, rt, sa)
	int cmd;
	register struct rtentry *rt;
	struct sockaddr *sa;
{
	register struct ifaddr *ifa;
	struct sockaddr *dst;
	struct ifnet *ifp;

	if (cmd != RTM_ADD || ((ifa = rt->rt_ifa) == 0) ||
	    ((ifp = ifa->ifa_ifp) == 0) || ((dst = rt_key(rt)) == 0))
		return;
	if ((ifa = ifaof_ifpforaddr(dst, ifp)) != NULL) {
		IFAFREE(rt->rt_ifa);
		rt->rt_ifa = ifa;
		ifa->ifa_refcnt++;
		if (ifa->ifa_rtrequest && ifa->ifa_rtrequest != link_rtrequest)
			ifa->ifa_rtrequest(cmd, rt, sa);
	}
}

/*
 * Mark an interface down and notify protocols of
 * the transition.
 * NOTE: must be called at splsoftnet or equivalent.
 */
void
if_down(ifp)
	register struct ifnet *ifp;
{
	register struct ifaddr *ifa;

	ifp->if_flags &= ~IFF_UP;
	for (ifa = ifp->if_addrlist.tqh_first; ifa != 0; ifa = ifa->ifa_list.tqe_next)
		pfctlinput(PRC_IFDOWN, ifa->ifa_addr);
	if_qflush(&ifp->if_snd);
	rt_ifmsg(ifp);
}

/*
 * Mark an interface up and notify protocols of
 * the transition.
 * NOTE: must be called at splsoftnet or equivalent.
 */
void
if_up(ifp)
	register struct ifnet *ifp;
{
#ifdef notyet
	register struct ifaddr *ifa;
#endif

	ifp->if_flags |= IFF_UP;
#ifdef notyet
	/* this has no effect on IP, and will kill all ISO connections XXX */
	for (ifa = ifp->if_addrlist.tqh_first; ifa != 0;
	     ifa = ifa->ifa_list.tqe_next)
		pfctlinput(PRC_IFUP, ifa->ifa_addr);
#endif
	rt_ifmsg(ifp);
#ifdef INET6
	in6_if_up(ifp);
#endif
}

/*
 * Flush an interface queue.
 */
void
if_qflush(ifq)
	register struct ifqueue *ifq;
{
	register struct mbuf *m, *n;

	n = ifq->ifq_head;
	while ((m = n) != NULL) {
		n = m->m_act;
		m_freem(m);
	}
	ifq->ifq_head = 0;
	ifq->ifq_tail = 0;
	ifq->ifq_len = 0;
}

/*
 * Handle interface watchdog timer routines.  Called
 * from softclock, we decrement timers (if set) and
 * call the appropriate interface routine on expiration.
 */
void
if_slowtimo(arg)
	void *arg;
{
	register struct ifnet *ifp;
	int s = splimp();

	for (ifp = ifnet.tqh_first; ifp != 0; ifp = ifp->if_list.tqe_next) {
		if (ifp->if_timer == 0 || --ifp->if_timer)
			continue;
		if (ifp->if_watchdog)
			(*ifp->if_watchdog)(ifp);
	}
	splx(s);
	timeout(if_slowtimo, NULL, hz / IFNET_SLOWHZ);
}

/*
 * Map interface name to
 * interface structure pointer.
 */
struct ifnet *
ifunit(name)
	register char *name;
{
	register struct ifnet *ifp;

	for (ifp = ifnet.tqh_first; ifp != 0; ifp = ifp->if_list.tqe_next)
		if (strcmp(ifp->if_xname, name) == 0)
			return (ifp);

	return (NULL);
}


/*
 * Map interface name in a sockaddr_dl to
 * interface structure pointer.
 */
struct ifnet *
if_withname(sa)
	struct sockaddr *sa;
{
	char ifname[IFNAMSIZ+1];
	struct sockaddr_dl *sdl = (struct sockaddr_dl *)sa;

	if ( (sa->sa_family != AF_LINK) || (sdl->sdl_nlen == 0) ||
	     (sdl->sdl_nlen > IFNAMSIZ) )
		return NULL;

	/*
	 * ifunit wants a null-terminated name.  It may not be null-terminated
	 * in the sockaddr.  We don't want to change the caller's sockaddr,
	 * and there might not be room to put the trailing null anyway, so we
	 * make a local copy that we know we can null terminate safely.
	 */

	bcopy(sdl->sdl_data, ifname, sdl->sdl_nlen);
	ifname[sdl->sdl_nlen] = '\0';
	return ifunit(ifname);
}


/*
 * Interface ioctls.
 */
int
ifioctl(so, cmd, data, p)
	struct socket *so;
	u_long cmd;
	caddr_t data;
	struct proc *p;
{
	register struct ifnet *ifp;
	register struct ifreq *ifr;
	int error = 0;
	short oif_flags;

	switch (cmd) {

	case SIOCGIFCONF:
	case OSIOCGIFCONF:
		return (ifconf(cmd, data));
	}
	ifr = (struct ifreq *)data;
	ifp = ifunit(ifr->ifr_name);
	if (ifp == 0)
		return (ENXIO);
	oif_flags = ifp->if_flags;
	switch (cmd) {

	case SIOCGIFFLAGS:
		ifr->ifr_flags = ifp->if_flags;
		break;

	case SIOCGIFMETRIC:
		ifr->ifr_metric = ifp->if_metric;
		break;

	case SIOCGIFMTU:
		ifr->ifr_mtu = ifp->if_mtu;
		break;

	case SIOCGIFDATA:
		error = copyout((caddr_t)&ifp->if_data, ifr->ifr_data,
		    sizeof(ifp->if_data));
		break;

	case SIOCSIFFLAGS:
		if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
			return (error);
		if (ifp->if_flags & IFF_UP && (ifr->ifr_flags & IFF_UP) == 0) {
			int s = splimp();
			if_down(ifp);
			splx(s);
		}
		if (ifr->ifr_flags & IFF_UP && (ifp->if_flags & IFF_UP) == 0) {
			int s = splimp();
			if_up(ifp);
			splx(s);
		}
		ifp->if_flags = (ifp->if_flags & IFF_CANTCHANGE) |
			(ifr->ifr_flags &~ IFF_CANTCHANGE);
		if (ifp->if_ioctl)
			(void) (*ifp->if_ioctl)(ifp, cmd, data);
		break;

	case SIOCSIFMETRIC:
		if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
			return (error);
		ifp->if_metric = ifr->ifr_metric;
		break;

	case SIOCSIFMTU:
	{
#ifdef INET6
		int oldmtu = ifp->if_mtu;
#endif

		if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
			return (error);
		if (ifp->if_ioctl == NULL)
			return (EOPNOTSUPP);
		error = (*ifp->if_ioctl)(ifp, cmd, data);

		/*
		 * If the link MTU changed, do network layer specific procedure.
		 */
#ifdef INET6
		if (ifp->if_mtu != oldmtu)
			nd6_setmtu(ifp);
#endif
		break;
	}

	case SIOCADDMULTI:
	case SIOCDELMULTI:
	case SIOCSIFMEDIA:
		if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
			return (error);
		/* FALLTHROUGH */
	case SIOCGIFMEDIA:
		if (ifp->if_ioctl == 0)
			return (EOPNOTSUPP);
		error = (*ifp->if_ioctl)(ifp, cmd, data);
		break;

	default:
		if (so->so_proto == 0)
			return (EOPNOTSUPP);
#if !defined(COMPAT_43) && !defined(COMPAT_LINUX) && !defined(COMPAT_SVR4)
		error = ((*so->so_proto->pr_usrreq)(so, PRU_CONTROL,
			(struct mbuf *) cmd, (struct mbuf *) data,
			(struct mbuf *) ifp));
#else
	    {
		u_long ocmd = cmd;

		switch (cmd) {

		case SIOCSIFADDR:
		case SIOCSIFDSTADDR:
		case SIOCSIFBRDADDR:
		case SIOCSIFNETMASK:
#if BYTE_ORDER != BIG_ENDIAN
			if (ifr->ifr_addr.sa_family == 0 &&
			    ifr->ifr_addr.sa_len < 16) {
				ifr->ifr_addr.sa_family = ifr->ifr_addr.sa_len;
				ifr->ifr_addr.sa_len = 16;
			}
#else
			if (ifr->ifr_addr.sa_len == 0)
				ifr->ifr_addr.sa_len = 16;
#endif
			break;

		case OSIOCGIFADDR:
			cmd = SIOCGIFADDR;
			break;

		case OSIOCGIFDSTADDR:
			cmd = SIOCGIFDSTADDR;
			break;

		case OSIOCGIFBRDADDR:
			cmd = SIOCGIFBRDADDR;
			break;

		case OSIOCGIFNETMASK:
			cmd = SIOCGIFNETMASK;
		}
		error = ((*so->so_proto->pr_usrreq)(so, PRU_CONTROL,
						    (struct mbuf *) cmd,
						    (struct mbuf *) data,
						    (struct mbuf *) ifp));
		switch (ocmd) {

		case OSIOCGIFADDR:
		case OSIOCGIFDSTADDR:
		case OSIOCGIFBRDADDR:
		case OSIOCGIFNETMASK:
			*(u_int16_t *)&ifr->ifr_addr = ifr->ifr_addr.sa_family;
		}

	    }
#endif
		break;
	}

	if (((oif_flags ^ ifp->if_flags) & IFF_UP) != 0) {
#ifdef INET6
		if ((ifp->if_flags & IFF_UP) != 0) {
			int s = splsoftnet();
			in6_if_up(ifp);
			splx(s);
		}
#endif
  	}
	return (error);
}

/*
 * Return interface configuration
 * of system.  List may be used
 * in later ioctl's (above) to get
 * other information.
 */
/*ARGSUSED*/
int
ifconf(cmd, data)
	u_long cmd;
	caddr_t data;
{
	register struct ifconf *ifc = (struct ifconf *)data;
	register struct ifnet *ifp;
	register struct ifaddr *ifa;
	struct ifreq ifr, *ifrp;
	int space = ifc->ifc_len, error = 0;

	/* If ifc->ifc_len is 0, fill it in with the needed size and return. */
	if (space == 0) {
		for (ifp = ifnet.tqh_first; ifp; ifp = ifp->if_list.tqe_next) {
			register struct sockaddr *sa;

			if ((ifa = ifp->if_addrlist.tqh_first) == 0)
				space += sizeof (ifr);
			else 
				for (; ifa != 0; ifa = ifa->ifa_list.tqe_next) {
					sa = ifa->ifa_addr;
#if defined(COMPAT_43) || defined(COMPAT_LINUX) || defined(COMPAT_SVR4)
					if (cmd != OSIOCGIFCONF)
#endif
					if (sa->sa_len > sizeof(*sa))
						space += sa->sa_len -
						    sizeof (*sa);
					space += sizeof (ifr);
				}
		}
		ifc->ifc_len = space;
		return(0);
	}

	ifrp = ifc->ifc_req;
	for (ifp = ifnet.tqh_first; space >= sizeof (ifr) && ifp != 0;
	    ifp = ifp->if_list.tqe_next) {
		bcopy(ifp->if_xname, ifr.ifr_name, IFNAMSIZ);
		if ((ifa = ifp->if_addrlist.tqh_first) == 0) {
			bzero((caddr_t)&ifr.ifr_addr, sizeof(ifr.ifr_addr));
			error = copyout((caddr_t)&ifr, (caddr_t)ifrp,
			    sizeof(ifr));
			if (error)
				break;
			space -= sizeof (ifr), ifrp++;
		} else 
			for (; space >= sizeof (ifr) && ifa != 0;
			    ifa = ifa->ifa_list.tqe_next) {
				register struct sockaddr *sa = ifa->ifa_addr;
#if defined(COMPAT_43) || defined(COMPAT_LINUX) || defined(COMPAT_SVR4)
				if (cmd == OSIOCGIFCONF) {
					struct osockaddr *osa =
					    (struct osockaddr *)&ifr.ifr_addr;
					ifr.ifr_addr = *sa;
					osa->sa_family = sa->sa_family;
					error = copyout((caddr_t)&ifr, (caddr_t)ifrp,
					    sizeof (ifr));
					ifrp++;
				} else
#endif
				if (sa->sa_len <= sizeof(*sa)) {
					ifr.ifr_addr = *sa;
					error = copyout((caddr_t)&ifr, (caddr_t)ifrp,
					    sizeof (ifr));
					ifrp++;
				} else {
					space -= sa->sa_len - sizeof(*sa);
					if (space < sizeof (ifr))
						break;
					error = copyout((caddr_t)&ifr, (caddr_t)ifrp,
					    sizeof (ifr.ifr_name));
					if (error == 0)
						error = copyout((caddr_t)sa,
						    (caddr_t)&ifrp->ifr_addr,
						    sa->sa_len);
					ifrp = (struct ifreq *)(sa->sa_len +
					    (caddr_t)&ifrp->ifr_addr);
				}
				if (error)
					break;
				space -= sizeof (ifr);
			}
	}
	ifc->ifc_len -= space;
	return (error);
}
