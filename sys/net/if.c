/*	$OpenBSD: if.c,v 1.93 2004/10/14 21:28:15 mickey Exp $	*/
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
 *	@(#)if.c	8.3 (Berkeley) 1/4/94
 */

#include "bpfilter.h"
#include "bridge.h"
#include "carp.h"
#include "pf.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/kernel.h>
#include <sys/ioctl.h>
#include <sys/domain.h>

#include <net/if.h>
#include <net/if_dl.h>
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
#include <netinet6/in6_ifattach.h>
#include <netinet6/nd6.h>
#endif

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#if NBRIDGE > 0
#include <net/if_bridge.h>
#endif

#if NCARP > 0
#include <netinet/ip_carp.h>
#endif

#if NPF > 0
#include <net/pfvar.h>
#endif

void	if_attachsetup(struct ifnet *);
void	if_attachdomain1(struct ifnet *);
int	if_detach_rtdelete(struct radix_node *, void *);

int	ifqmaxlen = IFQ_MAXLEN;

void	if_detach_queues(struct ifnet *, struct ifqueue *);
void	if_detached_start(struct ifnet *);
int	if_detached_ioctl(struct ifnet *, u_long, caddr_t);
int	if_detached_init(struct ifnet *);
void	if_detached_watchdog(struct ifnet *);

int	if_clone_list(struct if_clonereq *);
struct if_clone	*if_clone_lookup(const char *, int *);

void	if_congestion_clear(void *);

TAILQ_HEAD(, ifg_group) ifg_head;
LIST_HEAD(, if_clone) if_cloners = LIST_HEAD_INITIALIZER(if_cloners);
int if_cloners_count;

/*
 * Network interface utility routines.
 *
 * Routines with ifa_ifwith* names take sockaddr *'s as
 * parameters.
 */
void
ifinit()
{
	static struct timeout if_slowtim;

	timeout_set(&if_slowtim, if_slowtimo, &if_slowtim);

	if_slowtimo(&if_slowtim);
}

static int if_index = 0;
int if_indexlim = 0;
struct ifaddr **ifnet_addrs = NULL;
struct ifnet **ifindex2ifnet = NULL;
struct ifnet_head ifnet;
struct ifnet *lo0ifp;

/*
 * Attach an interface to the
 * list of "active" interfaces.
 */
void
if_attachsetup(ifp)
	struct ifnet *ifp;
{
	struct ifaddr *ifa;
	int n;
	int wrapped = 0;
	char ifgroup[IFNAMSIZ];

	if (ifindex2ifnet == 0)
		if_index = 1;
	else {
		while (if_index < if_indexlim &&
		    ifindex2ifnet[if_index] != NULL) {
			if_index++;
			/*
			 * If we hit USHRT_MAX, we skip back to 1 since
			 * there are a number of places where the value
			 * of ifp->if_index or if_index itself is compared
			 * to or stored in an unsigned short.  By
			 * jumping back, we won't botch those assignments
			 * or comparisons.
			 */
			if (if_index == USHRT_MAX) {
				if_index = 1;
				/*
				 * However, if we have to jump back to 1
				 * *twice* without finding an empty
				 * slot in ifindex2ifnet[], then there
				 * there are too many (>65535) interfaces.
				 */
				if (wrapped++)
					panic("too many interfaces");
			}
		}
	}
	ifp->if_index = if_index;

	/*
	 * We have some arrays that should be indexed by if_index.
	 * since if_index will grow dynamically, they should grow too.
	 *	struct ifadd **ifnet_addrs
	 *	struct ifnet **ifindex2ifnet
	 */
	if (ifnet_addrs == 0 || ifindex2ifnet == 0 || if_index >= if_indexlim) {
		size_t m, n, oldlim;
		caddr_t q;
		
		oldlim = if_indexlim;
		if (if_indexlim == 0)
			if_indexlim = 8;
		while (if_index >= if_indexlim)
			if_indexlim <<= 1;

		/* grow ifnet_addrs */
		m = oldlim * sizeof(ifa);
		n = if_indexlim * sizeof(ifa);
		q = (caddr_t)malloc(n, M_IFADDR, M_WAITOK);
		bzero(q, n);
		if (ifnet_addrs) {
			bcopy((caddr_t)ifnet_addrs, q, m);
			free((caddr_t)ifnet_addrs, M_IFADDR);
		}
		ifnet_addrs = (struct ifaddr **)q;

		/* grow ifindex2ifnet */
		m = oldlim * sizeof(struct ifnet *);
		n = if_indexlim * sizeof(struct ifnet *);
		q = (caddr_t)malloc(n, M_IFADDR, M_WAITOK);
		bzero(q, n);
		if (ifindex2ifnet) {
			bcopy((caddr_t)ifindex2ifnet, q, m);
			free((caddr_t)ifindex2ifnet, M_IFADDR);
		}
		ifindex2ifnet = (struct ifnet **)q;
	}

	TAILQ_INIT(&ifp->if_groups);

	/* add the interface family group */
	for (n = 0; ifp->if_xname[n] < '0' || ifp->if_xname[n] > '9'; n++)
		continue;
	strlcpy(ifgroup, ifp->if_xname, n + 1);
	if_addgroup(ifp, ifgroup);

	ifindex2ifnet[if_index] = ifp;

	if (ifp->if_snd.ifq_maxlen == 0)
		ifp->if_snd.ifq_maxlen = ifqmaxlen;
#ifdef ALTQ
	ifp->if_snd.altq_type = 0;
	ifp->if_snd.altq_disc = NULL;
	ifp->if_snd.altq_flags &= ALTQF_CANTCHANGE;
	ifp->if_snd.altq_tbr  = NULL;
	ifp->if_snd.altq_ifp  = ifp;
#endif

	if (domains)
		if_attachdomain1(ifp);
#if NPF > 0
	pfi_attach_ifnet(ifp);
#endif

	/* Announce the interface. */
	rt_ifannouncemsg(ifp, IFAN_ARRIVAL);
}

/*
 * Allocate the link level name for the specified interface.  This
 * is an attachment helper.  It must be called after ifp->if_addrlen
 * is initialized, which may not be the case when if_attach() is
 * called.
 */
void
if_alloc_sadl(ifp)
	struct ifnet *ifp;
{
	unsigned socksize, ifasize;
	int namelen, masklen;
	struct sockaddr_dl *sdl;
	struct ifaddr *ifa;

	/*
	 * If the interface already has a link name, release it
	 * now.  This is useful for interfaces that can change
	 * link types, and thus switch link names often.
	 */
	if (ifp->if_sadl != NULL)
		if_free_sadl(ifp);

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
	sdl->sdl_alen = ifp->if_addrlen;
	sdl->sdl_index = ifp->if_index;
	sdl->sdl_type = ifp->if_type;
	ifnet_addrs[ifp->if_index] = ifa;
	ifa->ifa_ifp = ifp;
	ifa->ifa_rtrequest = link_rtrequest;
	TAILQ_INSERT_HEAD(&ifp->if_addrlist, ifa, ifa_list);
	ifa->ifa_addr = (struct sockaddr *)sdl;
	ifp->if_sadl = sdl;
	sdl = (struct sockaddr_dl *)(socksize + (caddr_t)sdl);
	ifa->ifa_netmask = (struct sockaddr *)sdl;
	sdl->sdl_len = masklen;
	while (namelen != 0)
		sdl->sdl_data[--namelen] = 0xff;
}

/*
 * Free the link level name for the specified interface.  This is
 * a detach helper.  This is called from if_detach() or from
 * link layer type specific detach functions.
 */
void
if_free_sadl(ifp)
	struct ifnet *ifp;
{
	struct ifaddr *ifa;
	int s;

	ifa = ifnet_addrs[ifp->if_index];
	if (ifa == NULL)
		return;

	s = splnet();
	rtinit(ifa, RTM_DELETE, 0);
#if 0
	TAILQ_REMOVE(&ifp->if_addrlist, ifa, ifa_list);
	ifnet_addrs[ifp->if_index] = NULL;
#endif
	ifp->if_sadl = NULL;

	splx(s);
}

void
if_attachdomain()
{
	struct ifnet *ifp;
	int s;

	s = splnet();
	for (ifp = TAILQ_FIRST(&ifnet); ifp; ifp = TAILQ_NEXT(ifp, if_list))
		if_attachdomain1(ifp);
	splx(s);
}

void
if_attachdomain1(ifp)
	struct ifnet *ifp;
{
	struct domain *dp;
	int s;

	s = splnet();

	/* address family dependent data region */
	bzero(ifp->if_afdata, sizeof(ifp->if_afdata));
	for (dp = domains; dp; dp = dp->dom_next) {
		if (dp->dom_ifattach)
			ifp->if_afdata[dp->dom_family] =
			    (*dp->dom_ifattach)(ifp);
	}

	splx(s);
}

void
if_attachhead(ifp)
	struct ifnet *ifp;
{
	if (if_index == 0) {
		TAILQ_INIT(&ifnet);
		TAILQ_INIT(&ifg_head);
	}
	TAILQ_INIT(&ifp->if_addrlist);
	ifp->if_addrhooks = malloc(sizeof(*ifp->if_addrhooks), M_TEMP, M_NOWAIT);
	if (ifp->if_addrhooks == NULL)
		panic("if_attachhead: malloc");
	TAILQ_INIT(ifp->if_addrhooks);
	TAILQ_INSERT_HEAD(&ifnet, ifp, if_list);
	if_attachsetup(ifp);
}

void
if_attach(ifp)
	struct ifnet *ifp;
{
	if (if_index == 0) {
		TAILQ_INIT(&ifnet);
		TAILQ_INIT(&ifg_head);
	}
	TAILQ_INIT(&ifp->if_addrlist);
	ifp->if_addrhooks = malloc(sizeof(*ifp->if_addrhooks), M_TEMP, M_NOWAIT);
	if (ifp->if_addrhooks == NULL)
		panic("if_attach: malloc");
	TAILQ_INIT(ifp->if_addrhooks);
	TAILQ_INSERT_TAIL(&ifnet, ifp, if_list);
	if_attachsetup(ifp);
}

/*
 * Delete a route if it has a specific interface for output.
 * This function complies to the rn_walktree callback API.
 *
 * Note that deleting a RTF_CLONING route can trigger the
 * deletion of more entries, so we need to cancel the walk
 * and return EAGAIN.  The caller should restart the walk
 * as long as EAGAIN is returned.
 */
int
if_detach_rtdelete(rn, vifp)
	struct radix_node *rn;
	void *vifp;
{
	struct ifnet *ifp = vifp;
	struct rtentry *rt = (struct rtentry *)rn;

	if (rt->rt_ifp == ifp) {
		int cloning = (rt->rt_flags & RTF_CLONING);

		if (rtrequest(RTM_DELETE, rt_key(rt), rt->rt_gateway,
		    rt_mask(rt), 0, NULL) == 0 && cloning)
			return (EAGAIN);
	}

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
	struct ifg_list *ifg;
	int i, s = splimp();
	struct radix_node_head *rnh;
	struct domain *dp;

	ifp->if_flags &= ~IFF_OACTIVE;
	ifp->if_start = if_detached_start;
	ifp->if_ioctl = if_detached_ioctl;
	ifp->if_init = if_detached_init;
	ifp->if_watchdog = if_detached_watchdog;

#if NPF > 0
	pfi_detach_ifnet(ifp);
#endif

#if NBRIDGE > 0
	/* Remove the interface from any bridge it is part of.  */
	if (ifp->if_bridge)
		bridge_ifdetach(ifp);
#endif

#if NCARP > 0
	/* Remove the interface from any carp group it is a part of.  */
	if (ifp->if_carp)
		carp_ifdetach(ifp);
#endif

#if NBPFILTER > 0
	/* If there is a bpf device attached, detach from it.  */
	if (ifp->if_bpf)
		bpfdetach(ifp);
#endif
#ifdef ALTQ
	if (ALTQ_IS_ENABLED(&ifp->if_snd))
		altq_disable(&ifp->if_snd);
	if (ALTQ_IS_ATTACHED(&ifp->if_snd))
		altq_detach(&ifp->if_snd);
#endif

	/*
	 * Find and remove all routes which is using this interface.
	 * XXX Factor out into a route.c function?
	 */
	for (i = 1; i <= AF_MAX; i++) {
		rnh = rt_tables[i];
		if (rnh)
			while ((*rnh->rnh_walktree)(rnh,
			    if_detach_rtdelete, ifp) == EAGAIN)
				;
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
#ifdef INET6
	in6_ifdetach(ifp);
#endif

	/*
	 * remove packets came from ifp, from software interrupt queues.
	 * net/netisr_dispatch.h is not usable, as some of them use
	 * strange queue names.
	 */
#define IF_DETACH_QUEUES(x) \
do { \
	extern struct ifqueue x; \
	if_detach_queues(ifp, & x); \
} while (0)
#ifdef INET
	IF_DETACH_QUEUES(arpintrq);
	IF_DETACH_QUEUES(ipintrq);
#endif
#ifdef INET6
	IF_DETACH_QUEUES(ip6intrq);
#endif
#ifdef IPX
	IF_DETACH_QUEUES(ipxintrq);
#endif
#ifdef NS
	IF_DETACH_QUEUES(nsintrq);
#endif
#ifdef NETATALK
	IF_DETACH_QUEUES(atintrq1);
	IF_DETACH_QUEUES(atintrq2);
#endif
#ifdef CCITT
	IF_DETACH_QUEUES(llcintrq);
#endif
#ifdef NATM
	IF_DETACH_QUEUES(natmintrq);
#endif
#ifdef DECNET
	IF_DETACH_QUEUES(decnetintrq);
#endif
#undef IF_DETACH_QUEUES

	/*
	 * XXX transient ifp refs?  inpcb.ip_moptions.imo_multicast_ifp?
	 * Other network stacks than INET?
	 */

	/* Remove the interface from the list of all interfaces.  */
	TAILQ_REMOVE(&ifnet, ifp, if_list);

	/*
	 * Deallocate private resources.
	 * XXX should consult refcnt and use IFAFREE
	 */
	for (ifa = TAILQ_FIRST(&ifp->if_addrlist); ifa;
	    ifa = TAILQ_FIRST(&ifp->if_addrlist)) {
		TAILQ_REMOVE(&ifp->if_addrlist, ifa, ifa_list);
#ifdef INET
		if (ifa->ifa_addr->sa_family == AF_INET)
			TAILQ_REMOVE(&in_ifaddr, (struct in_ifaddr *)ifa,
			    ia_list);
#endif
		/* XXX if_free_sadl needs this */
		if (ifa == ifnet_addrs[ifp->if_index])
			continue;

		free(ifa, M_IFADDR);
	}

	for (ifg = TAILQ_FIRST(&ifp->if_groups); ifg;
	    ifg = TAILQ_FIRST(&ifp->if_groups))
		if_delgroup(ifp, ifg->ifgl_group->ifg_group);

	if_free_sadl(ifp);

	free(ifnet_addrs[ifp->if_index], M_IFADDR);
	ifnet_addrs[ifp->if_index] = NULL;

	free(ifp->if_addrhooks, M_TEMP);

	for (dp = domains; dp; dp = dp->dom_next) {
		if (dp->dom_ifdetach && ifp->if_afdata[dp->dom_family])
			(*dp->dom_ifdetach)(ifp,
			    ifp->if_afdata[dp->dom_family]);
	}

	/* Announce that the interface is gone. */
	rt_ifannouncemsg(ifp, IFAN_DEPARTURE);

	splx(s);
}

void
if_detach_queues(ifp, q)
	struct ifnet *ifp;
	struct ifqueue *q;
{
	struct mbuf *m, *prev, *next;

	prev = NULL;
	for (m = q->ifq_head; m; m = next) {
		next = m->m_nextpkt;
#ifdef DIAGNOSTIC
		if ((m->m_flags & M_PKTHDR) == 0) {
			prev = m;
			continue;
		}
#endif
		if (m->m_pkthdr.rcvif != ifp) {
			prev = m;
			continue;
		}

		if (prev)
			prev->m_nextpkt = m->m_nextpkt;
		else
			q->ifq_head = m->m_nextpkt;
		if (q->ifq_tail == m)
			q->ifq_tail = prev;
		q->ifq_len--;

		m->m_nextpkt = NULL;
		m_freem(m);
		IF_DROP(q);
	}
} 

/*
 * Create a clone network interface.
 */
int
if_clone_create(name)
	const char *name;
{
	struct if_clone *ifc;
	int unit;

	ifc = if_clone_lookup(name, &unit);
	if (ifc == NULL)
		return (EINVAL);

	if (ifunit(name) != NULL)
		return (EEXIST);

	return ((*ifc->ifc_create)(ifc, unit));
}

/*
 * Destroy a clone network interface.
 */
int
if_clone_destroy(name)
	const char *name;
{
	struct if_clone *ifc;
	struct ifnet *ifp;

	ifc = if_clone_lookup(name, NULL);
	if (ifc == NULL)
		return (EINVAL);

	ifp = ifunit(name);
	if (ifp == NULL)
		return (ENXIO);

	if (ifc->ifc_destroy == NULL)
		return (EOPNOTSUPP);

	return ((*ifc->ifc_destroy)(ifp));
}

/*
 * Look up a network interface cloner.
 */
struct if_clone *
if_clone_lookup(name, unitp)
	const char *name;
	int *unitp;
{
	struct if_clone *ifc;
	const char *cp;
	int unit;

	/* separate interface name from unit */
	for (cp = name;
	    cp - name < IFNAMSIZ && *cp && (*cp < '0' || *cp > '9');
	    cp++)
		continue;

	if (cp == name || cp - name == IFNAMSIZ || !*cp)
		return (NULL);	/* No name or unit number */

	if (cp - name < IFNAMSIZ-1 && *cp == '0' && cp[1] != '\0')
		return (NULL);	/* unit number 0 padded */

	LIST_FOREACH(ifc, &if_cloners, ifc_list) {
		if (strlen(ifc->ifc_name) == cp - name &&
		    !strncmp(name, ifc->ifc_name, cp - name))
			break;
	}

	if (ifc == NULL)
		return (NULL);

	unit = 0;
	while (cp - name < IFNAMSIZ && *cp) {
		if (*cp < '0' || *cp > '9' ||
		    unit > (INT_MAX - (*cp - '0')) / 10) {
			/* Bogus unit number. */
			return (NULL);
		}
		unit = (unit * 10) + (*cp++ - '0');
	}

	if (unitp != NULL)
		*unitp = unit;
	return (ifc);
}

/*
 * Register a network interface cloner.
 */
void
if_clone_attach(ifc)
	struct if_clone *ifc;
{
	LIST_INSERT_HEAD(&if_cloners, ifc, ifc_list);
	if_cloners_count++;
#if NPF > 0
	pfi_attach_clone(ifc);
#endif
}

/*
 * Unregister a network interface cloner.
 */
void
if_clone_detach(ifc)
	struct if_clone *ifc;
{

	LIST_REMOVE(ifc, ifc_list);
	if_cloners_count--;
}

/*
 * Provide list of interface cloners to userspace.
 */
int
if_clone_list(ifcr)
	struct if_clonereq *ifcr;
{
	char outbuf[IFNAMSIZ], *dst;
	struct if_clone *ifc;
	int count, error = 0;

	ifcr->ifcr_total = if_cloners_count;
	if ((dst = ifcr->ifcr_buffer) == NULL) {
		/* Just asking how many there are. */
		return (0);
	}

	if (ifcr->ifcr_count < 0)
		return (EINVAL);

	count = (if_cloners_count < ifcr->ifcr_count) ?
	    if_cloners_count : ifcr->ifcr_count;

	for (ifc = LIST_FIRST(&if_cloners); ifc != NULL && count != 0;
	     ifc = LIST_NEXT(ifc, ifc_list), count--, dst += IFNAMSIZ) {
		strlcpy(outbuf, ifc->ifc_name, IFNAMSIZ);
		error = copyout(outbuf, dst, IFNAMSIZ);
		if (error)
			break;
	}

	return (error);
}

/*
 * set queue congestion marker and register timeout to clear it
 */
void
if_congestion(struct ifqueue *ifq)
{
	static struct timeout	to;

	ifq->ifq_congestion = 1;
	bzero(&to, sizeof(to));
	timeout_set(&to, if_congestion_clear, ifq);
	timeout_add(&to, hz / 100);
}

/*
 * clear the congestion flag
 */
void
if_congestion_clear(void *ifq)
{
	((struct ifqueue *)ifq)->ifq_congestion = 0;
}

/*
 * Locate an interface based on a complete address.
 */
/*ARGSUSED*/
struct ifaddr *
ifa_ifwithaddr(addr)
	struct sockaddr *addr;
{
	struct ifnet *ifp;
	struct ifaddr *ifa;

#define	equal(a1, a2) \
  (bcmp((caddr_t)(a1), (caddr_t)(a2), ((struct sockaddr *)(a1))->sa_len) == 0)
	TAILQ_FOREACH(ifp, &ifnet, if_list) {
	    TAILQ_FOREACH(ifa, &ifp->if_addrlist, ifa_list) {
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
	}
	return (NULL);
}
/*
 * Locate the point to point interface with a given destination address.
 */
/*ARGSUSED*/
struct ifaddr *
ifa_ifwithdstaddr(addr)
	struct sockaddr *addr;
{
	struct ifnet *ifp;
	struct ifaddr *ifa;

	TAILQ_FOREACH(ifp, &ifnet, if_list) {
	    if (ifp->if_flags & IFF_POINTOPOINT)
	        TAILQ_FOREACH(ifa, &ifp->if_addrlist, ifa_list) {
			if (ifa->ifa_addr->sa_family != addr->sa_family ||
			    ifa->ifa_dstaddr == NULL)
				continue;
			if (equal(addr, ifa->ifa_dstaddr))
				return (ifa);
		}
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
	struct ifnet *ifp;
	struct ifaddr *ifa;
	struct ifaddr *ifa_maybe = 0;
	u_int af = addr->sa_family;
	char *addr_data = addr->sa_data, *cplim;

	if (af == AF_LINK) {
		struct sockaddr_dl *sdl = (struct sockaddr_dl *)addr;
		if (sdl->sdl_index && sdl->sdl_index < if_indexlim &&
		    ifindex2ifnet[sdl->sdl_index])
			return (ifnet_addrs[sdl->sdl_index]);
	}
	TAILQ_FOREACH(ifp, &ifnet, if_list) {
		TAILQ_FOREACH(ifa, &ifp->if_addrlist, ifa_list) {
			char *cp, *cp2, *cp3;

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
	}
	return (ifa_maybe);
}

/*
 * Find an interface using a specific address family
 */
struct ifaddr *
ifa_ifwithaf(af)
	int af;
{
	struct ifnet *ifp;
	struct ifaddr *ifa;

	TAILQ_FOREACH(ifp, &ifnet, if_list) {
		TAILQ_FOREACH(ifa, &ifp->if_addrlist, ifa_list) {
			if (ifa->ifa_addr->sa_family == af)
				return (ifa);
		}
	}
	return (NULL);
}

/*
 * Find an interface address specific to an interface best matching
 * a given address.
 */
struct ifaddr *
ifaof_ifpforaddr(addr, ifp)
	struct sockaddr *addr;
	struct ifnet *ifp;
{
	struct ifaddr *ifa;
	char *cp, *cp2, *cp3;
	char *cplim;
	struct ifaddr *ifa_maybe = 0;
	u_int af = addr->sa_family;

	if (af >= AF_MAX)
		return (NULL);
	TAILQ_FOREACH(ifa, &ifp->if_addrlist, ifa_list) {
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
link_rtrequest(cmd, rt, info)
	int cmd;
	struct rtentry *rt;
	struct rt_addrinfo *info;
{
	struct ifaddr *ifa;
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
			ifa->ifa_rtrequest(cmd, rt, info);
	}
}

/*
 * Mark an interface down and notify protocols of
 * the transition.
 * NOTE: must be called at splsoftnet or equivalent.
 */
void
if_down(struct ifnet *ifp)
{
	struct ifaddr *ifa;

	splassert(IPL_SOFTNET);

	ifp->if_flags &= ~IFF_UP;
	microtime(&ifp->if_lastchange);
	TAILQ_FOREACH(ifa, &ifp->if_addrlist, ifa_list) {
		pfctlinput(PRC_IFDOWN, ifa->ifa_addr);
	}
	IFQ_PURGE(&ifp->if_snd);
#if NCARP > 0
	if (ifp->if_carp)
		carp_carpdev_state(ifp->if_carp);
#endif
	rt_ifmsg(ifp);
}

/*
 * Mark an interface up and notify protocols of
 * the transition.
 * NOTE: must be called at splsoftnet or equivalent.
 */
void
if_up(struct ifnet *ifp)
{
#ifdef notyet
	struct ifaddr *ifa;
#endif

	splassert(IPL_SOFTNET);

	ifp->if_flags |= IFF_UP;
	microtime(&ifp->if_lastchange);
#ifdef notyet
	/* this has no effect on IP, and will kill all ISO connections XXX */
	TAILQ_FOREACH(ifa, &ifp->if_addrlist, ifa_list) {
		pfctlinput(PRC_IFUP, ifa->ifa_addr);
	}
#endif
#if NCARP > 0
	if (ifp->if_carp)
		carp_carpdev_state(ifp->if_carp);
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
	struct ifqueue *ifq;
{
	struct mbuf *m, *n;

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
	struct timeout *to = (struct timeout *)arg;
	struct ifnet *ifp;
	int s = splimp();

	TAILQ_FOREACH(ifp, &ifnet, if_list) {
		if (ifp->if_timer == 0 || --ifp->if_timer)
			continue;
		if (ifp->if_watchdog)
			(*ifp->if_watchdog)(ifp);
	}
	splx(s);
	timeout_add(to, hz / IFNET_SLOWHZ);
}

/*
 * Map interface name to
 * interface structure pointer.
 */
struct ifnet *
ifunit(name)
	const char *name;
{
	struct ifnet *ifp;

	TAILQ_FOREACH(ifp, &ifnet, if_list) {
		if (strcmp(ifp->if_xname, name) == 0)
			return (ifp);
	}
	return (NULL);
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
	struct ifnet *ifp;
	struct ifreq *ifr;
	struct ifgroupreq *ifgr;
	char ifdescrbuf[IFDESCRSIZE];
	int error = 0;
	size_t bytesdone;
	short oif_flags;

	switch (cmd) {

	case SIOCGIFCONF:
	case OSIOCGIFCONF:
		return (ifconf(cmd, data));
	}
	ifr = (struct ifreq *)data;

	switch (cmd) {
	case SIOCIFCREATE:
	case SIOCIFDESTROY:
		if ((error = suser(p, 0)) != 0)
			return (error);
		return ((cmd == SIOCIFCREATE) ?
		    if_clone_create(ifr->ifr_name) :
		    if_clone_destroy(ifr->ifr_name));
	case SIOCIFGCLONERS:
		return (if_clone_list((struct if_clonereq *)data));
	}

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
		if ((error = suser(p, 0)) != 0)
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
		if ((error = suser(p, 0)) != 0)
			return (error);
		ifp->if_metric = ifr->ifr_metric;
		break;

	case SIOCSIFMTU:
	{
#ifdef INET6
		int oldmtu = ifp->if_mtu;
#endif

		if ((error = suser(p, 0)) != 0)
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

	case SIOCSIFPHYADDR:
	case SIOCDIFPHYADDR:
#ifdef INET6
	case SIOCSIFPHYADDR_IN6:
#endif
	case SIOCSLIFPHYADDR:
	case SIOCADDMULTI:
	case SIOCDELMULTI:
	case SIOCSIFMEDIA:
		if ((error = suser(p, 0)) != 0)
			return (error);
		/* FALLTHROUGH */
	case SIOCGIFPSRCADDR:
	case SIOCGIFPDSTADDR:
	case SIOCGLIFPHYADDR:
	case SIOCGIFMEDIA:
		if (ifp->if_ioctl == 0)
			return (EOPNOTSUPP);
		error = (*ifp->if_ioctl)(ifp, cmd, data);
		break;

	case SIOCGIFDESCR:
		strlcpy(ifdescrbuf, ifp->if_description, IFDESCRSIZE);
		error = copyoutstr(ifdescrbuf, ifr->ifr_data, IFDESCRSIZE,
		    &bytesdone);
		break;

	case SIOCSIFDESCR:
		if ((error = suser(p, 0)) != 0)
			return (error);
		error = copyinstr(ifr->ifr_data, ifdescrbuf,
		    IFDESCRSIZE, &bytesdone);
		if (error == 0) {
			(void)memset(ifp->if_description, 0, IFDESCRSIZE);
			strlcpy(ifp->if_description, ifdescrbuf, IFDESCRSIZE);
		}
		break;

	case OSIOCAIFGROUP:
	case OSIOCGIFGROUP:
	case OSIOCDIFGROUP:
		return (EINVAL);
		break;

	case SIOCAIFGROUP:
		if ((error = suser(p, 0)))
			return (error);
		ifgr = (struct ifgroupreq *)data;
		if ((error = if_addgroup(ifp, ifgr->ifgr_group)))
			return (error);
		break;

	case SIOCGIFGROUP:
		if ((error = if_getgroup(data, ifp)))
			return (error);
		break;

	case SIOCDIFGROUP:
		if ((error = suser(p, 0)))
			return (error);
		ifgr = (struct ifgroupreq *)data;
		if ((error = if_delgroup(ifp, ifgr->ifgr_group)))
			return (error);
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
		    (struct mbuf *) cmd, (struct mbuf *) data,
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
			int s = splnet();
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
	struct ifconf *ifc = (struct ifconf *)data;
	struct ifnet *ifp;
	struct ifaddr *ifa;
	struct ifreq ifr, *ifrp;
	int space = ifc->ifc_len, error = 0;

	/* If ifc->ifc_len is 0, fill it in with the needed size and return. */
	if (space == 0) {
		TAILQ_FOREACH(ifp, &ifnet, if_list) {
			struct sockaddr *sa;

			if (TAILQ_EMPTY(&ifp->if_addrlist))
				space += sizeof (ifr);
			else 
				TAILQ_FOREACH(ifa, &ifp->if_addrlist, ifa_list) {
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
		return (0);
	}

	ifrp = ifc->ifc_req;
	for (ifp = TAILQ_FIRST(&ifnet); space >= sizeof(ifr) &&
	    ifp != TAILQ_END(&ifnet); ifp = TAILQ_NEXT(ifp, if_list)) {
		bcopy(ifp->if_xname, ifr.ifr_name, IFNAMSIZ);
		if (TAILQ_EMPTY(&ifp->if_addrlist)) {
			bzero((caddr_t)&ifr.ifr_addr, sizeof(ifr.ifr_addr));
			error = copyout((caddr_t)&ifr, (caddr_t)ifrp,
			    sizeof(ifr));
			if (error)
				break;
			space -= sizeof (ifr), ifrp++;
		} else 
			for (ifa = TAILQ_FIRST(&ifp->if_addrlist);
			    space >= sizeof (ifr) &&
			    ifa != TAILQ_END(&ifp->if_addrlist);
			    ifa = TAILQ_NEXT(ifa, ifa_list)) {
				struct sockaddr *sa = ifa->ifa_addr;
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

/*
 * Dummy functions replaced in ifnet during detach (if protocols decide to
 * fiddle with the if during detach.
 */
void
if_detached_start(struct ifnet *ifp)
{
	struct mbuf *m;

	while (1) {
		IF_DEQUEUE(&ifp->if_snd, m);

		if (m == NULL)
			return;
		m_freem(m);
	}
}

int
if_detached_ioctl(struct ifnet *ifp, u_long a, caddr_t b)
{
	return ENODEV;
}

int
if_detached_init(struct ifnet *ifp)
{
	return (ENXIO);
}

void
if_detached_watchdog(struct ifnet *ifp)
{
	/* nothing */
}

/*
 * Add a group to an interface
 */
int
if_addgroup(struct ifnet *ifp, char *groupname)
{
	struct ifg_list		*ifgl;
	struct ifg_group	*ifg = NULL;

	TAILQ_FOREACH(ifgl, &ifp->if_groups, ifgl_next)
		if (!strcmp(ifgl->ifgl_group->ifg_group, groupname))
			return (EEXIST);

	if ((ifgl = (struct ifg_list *)malloc(sizeof(struct ifg_list), M_TEMP,
	    M_NOWAIT)) == NULL)
		return (ENOMEM);

	TAILQ_FOREACH(ifg, &ifg_head, ifg_next)
		if (!strcmp(ifg->ifg_group, groupname))
			break;

	if (ifg == NULL) {
		if ((ifg = (struct ifg_group *)malloc(sizeof(struct ifg_group),
		    M_TEMP, M_NOWAIT)) == NULL) {
			free(ifgl, M_TEMP);
			return (ENOMEM);
		}
		strlcpy(ifg->ifg_group, groupname, sizeof(ifg->ifg_group));
		ifg->ifg_refcnt = 0;
		TAILQ_INSERT_TAIL(&ifg_head, ifg, ifg_next);
	}

	ifg->ifg_refcnt++;
	ifgl->ifgl_group = ifg;

	TAILQ_INSERT_TAIL(&ifp->if_groups, ifgl, ifgl_next);

	return (0);
}

/*
 * Remove a group from an interface
 */
int
if_delgroup(struct ifnet *ifp, char *groupname)
{
	struct ifg_list		*ifgl;

	TAILQ_FOREACH(ifgl, &ifp->if_groups, ifgl_next)
		if (!strcmp(ifgl->ifgl_group->ifg_group, groupname))
			break;
	if (ifgl == NULL)
		return (ENOENT);

	TAILQ_REMOVE(&ifp->if_groups, ifgl, ifgl_next);

	if (--ifgl->ifgl_group->ifg_refcnt == 0) {
		TAILQ_REMOVE(&ifg_head, ifgl->ifgl_group, ifg_next);
		free(ifgl->ifgl_group, M_TEMP);
	}

	free(ifgl, M_TEMP);

	return (0);
}

/*
 * Stores all groups from an interface in memory pointed
 * to by data
 */
int
if_getgroup(caddr_t data, struct ifnet *ifp)
{
	int			 len, error;
	struct ifg_list		*ifgl;
	struct ifg_req		 ifgrq, *ifgp;
	struct ifgroupreq	*ifgr = (struct ifgroupreq *)data;

	if (ifgr->ifgr_len == 0) {
		TAILQ_FOREACH(ifgl, &ifp->if_groups, ifgl_next)
			ifgr->ifgr_len += sizeof(struct ifg_req);
		return (0);
	}

	len = ifgr->ifgr_len;
	ifgp = ifgr->ifgr_groups;
	TAILQ_FOREACH(ifgl, &ifp->if_groups, ifgl_next) {
		if (len < sizeof(ifgrq))
			return (EINVAL);
		strlcpy(ifgrq.ifgrq_group, ifgl->ifgl_group->ifg_group,
		    sizeof(ifgrq.ifgrq_group));
		if ((error = copyout((caddr_t)&ifgrq, (caddr_t)ifgp, 
		    sizeof(struct ifg_req))))
			return (error);
		len -= sizeof(ifgrq);
		ifgp++;
	}

	return (0);
}

		
/*
 * Set/clear promiscuous mode on interface ifp based on the truth value
 * of pswitch.  The calls are reference counted so that only the first
 * "on" request actually has an effect, as does the final "off" request.
 * Results are undefined if the "off" and "on" requests are not matched.
 */
int
ifpromisc(ifp, pswitch)
	struct ifnet *ifp;
	int pswitch;
{
	struct ifreq ifr;

	if (pswitch) {
		/*
		 * If the device is not configured up, we cannot put it in
		 * promiscuous mode.
		 */
		if ((ifp->if_flags & IFF_UP) == 0)
			return (ENETDOWN);
		if (ifp->if_pcount++ != 0)
			return (0);
		ifp->if_flags |= IFF_PROMISC;
	} else {
		if (--ifp->if_pcount > 0)
			return (0);
		ifp->if_flags &= ~IFF_PROMISC;
		/*
		 * If the device is not configured up, we should not need to
		 * turn off promiscuous mode (device should have turned it
		 * off when interface went down; and will look at IFF_PROMISC
		 * again next time interface comes up).
		 */
		if ((ifp->if_flags & IFF_UP) == 0)
			return (0);
	}
	ifr.ifr_flags = ifp->if_flags;
	return ((*ifp->if_ioctl)(ifp, SIOCSIFFLAGS, (caddr_t)&ifr));
}
