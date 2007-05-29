/*	$OpenBSD: if.c,v 1.159 2007/05/29 02:21:18 claudio Exp $	*/
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

#include "bluetooth.h"
#include "bpfilter.h"
#include "bridge.h"
#include "carp.h"
#include "pf.h"
#include "trunk.h"

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
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/route.h>
#include <net/netisr.h>

#include <dev/rndvar.h>

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

#if NTRUNK > 0
#include <net/if_trunk.h>
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

int	ifqmaxlen = IFQ_MAXLEN;

void	if_detach_queues(struct ifnet *, struct ifqueue *);
void	if_detached_start(struct ifnet *);
int	if_detached_ioctl(struct ifnet *, u_long, caddr_t);
int	if_detached_init(struct ifnet *);
void	if_detached_watchdog(struct ifnet *);

int	if_getgroup(caddr_t, struct ifnet *);
int	if_getgroupmembers(caddr_t);
int	if_getgroupattribs(caddr_t);
int	if_setgroupattribs(caddr_t);

int	if_clone_list(struct if_clonereq *);
struct if_clone	*if_clone_lookup(const char *, int *);

void	if_congestion_clear(void *);
int	if_group_egress_build(void);

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
if_attachsetup(struct ifnet *ifp)
{
	struct ifaddr *ifa;
	int wrapped = 0;

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
	 *	struct ifaddr **ifnet_addrs
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

	if_addgroup(ifp, IFG_ALL);

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
if_alloc_sadl(struct ifnet *ifp)
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
if_free_sadl(struct ifnet *ifp)
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
if_attachdomain1(struct ifnet *ifp)
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
if_attachhead(struct ifnet *ifp)
{
	if (if_index == 0) {
		TAILQ_INIT(&ifnet);
		TAILQ_INIT(&ifg_head);
	}
	TAILQ_INIT(&ifp->if_addrlist);
	ifp->if_addrhooks = malloc(sizeof(*ifp->if_addrhooks),
	    M_TEMP, M_NOWAIT);
	if (ifp->if_addrhooks == NULL)
		panic("if_attachhead: malloc");
	TAILQ_INIT(ifp->if_addrhooks);
	ifp->if_linkstatehooks = malloc(sizeof(*ifp->if_linkstatehooks),
	    M_TEMP, M_NOWAIT);
	if (ifp->if_linkstatehooks == NULL)
		panic("if_attachhead: malloc");
	TAILQ_INIT(ifp->if_linkstatehooks);
	ifp->if_detachhooks = malloc(sizeof(*ifp->if_detachhooks),
	    M_TEMP, M_NOWAIT);
	if (ifp->if_detachhooks == NULL)
		panic("if_attachhead: malloc");
	TAILQ_INIT(ifp->if_detachhooks);
	TAILQ_INSERT_HEAD(&ifnet, ifp, if_list);
	if_attachsetup(ifp);
}

void
if_attach(struct ifnet *ifp)
{
#if NCARP > 0
	struct ifnet *before = NULL;
#endif

	if (if_index == 0) {
		TAILQ_INIT(&ifnet);
		TAILQ_INIT(&ifg_head);
	}
	TAILQ_INIT(&ifp->if_addrlist);
	ifp->if_addrhooks = malloc(sizeof(*ifp->if_addrhooks),
	    M_TEMP, M_NOWAIT);
	if (ifp->if_addrhooks == NULL)
		panic("if_attach: malloc");
	TAILQ_INIT(ifp->if_addrhooks);
	ifp->if_linkstatehooks = malloc(sizeof(*ifp->if_linkstatehooks),
	    M_TEMP, M_NOWAIT);
	if (ifp->if_linkstatehooks == NULL)
		panic("if_attach: malloc");
	TAILQ_INIT(ifp->if_linkstatehooks);
	ifp->if_detachhooks = malloc(sizeof(*ifp->if_detachhooks),
	    M_TEMP, M_NOWAIT);
	if (ifp->if_detachhooks == NULL)
		panic("if_attach: malloc");
	TAILQ_INIT(ifp->if_detachhooks);

#if NCARP > 0
	if (ifp->if_type != IFT_CARP)
		TAILQ_FOREACH(before, &ifnet, if_list)
			if (before->if_type == IFT_CARP)
				break;
	if (before == NULL)
		TAILQ_INSERT_TAIL(&ifnet, ifp, if_list);
	else
		TAILQ_INSERT_BEFORE(before, ifp, if_list);
#else
	TAILQ_INSERT_TAIL(&ifnet, ifp, if_list);
#endif

	if_attachsetup(ifp);
}

/*
 * Detach an interface from everything in the kernel.  Also deallocate
 * private resources.
 * XXX So far only the INET protocol family has been looked over
 * wrt resource usage that needs to be decoupled.
 */
void
if_detach(struct ifnet *ifp)
{
	struct ifaddr *ifa;
	struct ifg_list *ifg;
	int s = splnet();
	struct domain *dp;

	ifp->if_flags &= ~IFF_OACTIVE;
	ifp->if_start = if_detached_start;
	ifp->if_ioctl = if_detached_ioctl;
	ifp->if_init = if_detached_init;
	ifp->if_watchdog = if_detached_watchdog;

	/* Call detach hooks, ie. to remove vlan interfaces */
	dohooks(ifp->if_detachhooks, HOOK_REMOVE | HOOK_FREE);

#if NTRUNK > 0
	if (ifp->if_type == IFT_IEEE8023ADLAG)
		trunk_port_ifdetach(ifp);
#endif

#if NBRIDGE > 0
	/* Remove the interface from any bridge it is part of.  */
	if (ifp->if_bridge)
		bridge_ifdetach(ifp);
#endif

#if NCARP > 0
	/* Remove the interface from any carp group it is a part of.  */
	if (ifp->if_carp && ifp->if_type != IFT_CARP)
		carp_ifdetach(ifp);
#endif

#if NBPFILTER > 0
	bpfdetach(ifp);
#endif
#ifdef ALTQ
	if (ALTQ_IS_ENABLED(&ifp->if_snd))
		altq_disable(&ifp->if_snd);
	if (ALTQ_IS_ATTACHED(&ifp->if_snd))
		altq_detach(&ifp->if_snd);
#endif
	rt_if_remove(ifp);
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

#if NPF > 0
	pfi_detach_ifnet(ifp);
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
#ifdef NETATALK
	IF_DETACH_QUEUES(atintrq1);
	IF_DETACH_QUEUES(atintrq2);
#endif
#ifdef NATM
	IF_DETACH_QUEUES(natmintrq);
#endif
#if NBLUETOOTH > 0
	IF_DETACH_QUEUES(btintrq);
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
	 */
	while ((ifa = TAILQ_FIRST(&ifp->if_addrlist)) != NULL) {
		TAILQ_REMOVE(&ifp->if_addrlist, ifa, ifa_list);
#ifdef INET
		if (ifa->ifa_addr->sa_family == AF_INET)
			TAILQ_REMOVE(&in_ifaddr, (struct in_ifaddr *)ifa,
			    ia_list);
#endif
		/* XXX if_free_sadl needs this */
		if (ifa == ifnet_addrs[ifp->if_index])
			continue;

		IFAFREE(ifa);
	}

	for (ifg = TAILQ_FIRST(&ifp->if_groups); ifg;
	    ifg = TAILQ_FIRST(&ifp->if_groups))
		if_delgroup(ifp, ifg->ifgl_group->ifg_group);

	if_free_sadl(ifp);

	IFAFREE(ifnet_addrs[ifp->if_index]);
	ifnet_addrs[ifp->if_index] = NULL;

	free(ifp->if_addrhooks, M_TEMP);
	free(ifp->if_linkstatehooks, M_TEMP);
	free(ifp->if_detachhooks, M_TEMP);

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
if_detach_queues(struct ifnet *ifp, struct ifqueue *q)
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
if_clone_create(const char *name)
{
	struct if_clone *ifc;
	struct ifnet *ifp;
	int unit, ret;

	ifc = if_clone_lookup(name, &unit);
	if (ifc == NULL)
		return (EINVAL);

	if (ifunit(name) != NULL)
		return (EEXIST);

	if ((ret = (*ifc->ifc_create)(ifc, unit)) == 0 &&
	    (ifp = ifunit(name)) != NULL)
		if_addgroup(ifp, ifc->ifc_name);

	return (ret);
}

/*
 * Destroy a clone network interface.
 */
int
if_clone_destroy(const char *name)
{
	struct if_clone *ifc;
	struct ifnet *ifp;
	int s, ret;

	ifc = if_clone_lookup(name, NULL);
	if (ifc == NULL)
		return (EINVAL);

	ifp = ifunit(name);
	if (ifp == NULL)
		return (ENXIO);

	if (ifc->ifc_destroy == NULL)
		return (EOPNOTSUPP);

	if (ifp->if_flags & IFF_UP) {
		s = splnet();
		if_down(ifp);
		splx(s);
	}

	if_delgroup(ifp, ifc->ifc_name);

	if ((ret = (*ifc->ifc_destroy)(ifp)) != 0)
		if_addgroup(ifp, ifc->ifc_name);

	return (ret);
}

/*
 * Look up a network interface cloner.
 */
struct if_clone *
if_clone_lookup(const char *name, int *unitp)
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
if_clone_attach(struct if_clone *ifc)
{
	LIST_INSERT_HEAD(&if_cloners, ifc, ifc_list);
	if_cloners_count++;
}

/*
 * Unregister a network interface cloner.
 */
void
if_clone_detach(struct if_clone *ifc)
{

	LIST_REMOVE(ifc, ifc_list);
	if_cloners_count--;
}

/*
 * Provide list of interface cloners to userspace.
 */
int
if_clone_list(struct if_clonereq *ifcr)
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
		bzero(outbuf, sizeof outbuf);
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
	/* Not currently needed, all callers check this */
	if (ifq->ifq_congestion)
		return;

	ifq->ifq_congestion = malloc(sizeof(struct timeout), M_TEMP, M_NOWAIT);
	if (ifq->ifq_congestion == NULL)
		return;
	timeout_set(ifq->ifq_congestion, if_congestion_clear, ifq);
	timeout_add(ifq->ifq_congestion, hz / 100);
}

/*
 * clear the congestion flag
 */
void
if_congestion_clear(void *arg)
{
	struct ifqueue *ifq = arg;
	struct timeout *to = ifq->ifq_congestion;

	ifq->ifq_congestion = NULL;
	free(to, M_TEMP);
}

/*
 * Locate an interface based on a complete address.
 */
/*ARGSUSED*/
struct ifaddr *
ifa_ifwithaddr(struct sockaddr *addr)
{
	struct ifnet *ifp;
	struct ifaddr *ifa;

#define	equal(a1, a2)	\
	(bcmp((caddr_t)(a1), (caddr_t)(a2),	\
	((struct sockaddr *)(a1))->sa_len) == 0)
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
ifa_ifwithdstaddr(struct sockaddr *addr)
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
ifa_ifwithnet(struct sockaddr *addr)
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
ifa_ifwithaf(int af)
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
ifaof_ifpforaddr(struct sockaddr *addr, struct ifnet *ifp)
{
	struct ifaddr *ifa;
	char *cp, *cp2, *cp3;
	char *cplim;
	struct ifaddr *ifa_maybe = NULL;
	u_int af = addr->sa_family;

	if (af >= AF_MAX)
		return (NULL);
	TAILQ_FOREACH(ifa, &ifp->if_addrlist, ifa_list) {
		if (ifa->ifa_addr->sa_family != af)
			continue;
		if (ifa_maybe == NULL)
			ifa_maybe = ifa;
		if (ifa->ifa_netmask == 0 || ifp->if_flags & IFF_POINTOPOINT) {
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
link_rtrequest(int cmd, struct rtentry *rt, struct rt_addrinfo *info)
{
	struct ifaddr *ifa;
	struct sockaddr *dst;
	struct ifnet *ifp;

	if (cmd != RTM_ADD || ((ifa = rt->rt_ifa) == 0) ||
	    ((ifp = ifa->ifa_ifp) == 0) || ((dst = rt_key(rt)) == 0))
		return;
	if ((ifa = ifaof_ifpforaddr(dst, ifp)) != NULL) {
		ifa->ifa_refcnt++;
		IFAFREE(rt->rt_ifa);
		rt->rt_ifa = ifa;
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
		carp_carpdev_state(ifp);
#endif
#if NBRIDGE > 0
	if (ifp->if_bridge)
		bstp_ifstate(ifp);
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
		carp_carpdev_state(ifp);
#endif
#if NBRIDGE > 0
	if (ifp->if_bridge)
		bstp_ifstate(ifp);
#endif
	rt_ifmsg(ifp);
#ifdef INET6
	in6_if_up(ifp);
#endif
}

/*
 * Process a link state change.
 * NOTE: must be called at splsoftnet or equivalent.
 */
void
if_link_state_change(struct ifnet *ifp)
{
	rt_ifmsg(ifp);
	dohooks(ifp->if_linkstatehooks, 0);
}

/*
 * Flush an interface queue.
 */
void
if_qflush(struct ifqueue *ifq)
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
if_slowtimo(void *arg)
{
	struct timeout *to = (struct timeout *)arg;
	struct ifnet *ifp;
	int s = splnet();

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
ifunit(const char *name)
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
ifioctl(struct socket *so, u_long cmd, caddr_t data, struct proc *p)
{
	struct ifnet *ifp;
	struct ifreq *ifr;
	struct ifaddr *ifa;
	struct sockaddr_dl *sdl;
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
	case SIOCGIFGMEMB:
		return (if_getgroupmembers(data));
	case SIOCGIFGATTR:
		return (if_getgroupattribs(data));
	case SIOCSIFGATTR:
		if ((error = suser(p, 0)) != 0)
			return (error);
		return (if_setgroupattribs(data));
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
			int s = splnet();
			if_down(ifp);
			splx(s);
		}
		if (ifr->ifr_flags & IFF_UP && (ifp->if_flags & IFF_UP) == 0) {
			int s = splnet();
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

	case SIOCAIFGROUP:
		if ((error = suser(p, 0)))
			return (error);
		(*ifp->if_ioctl)(ifp, cmd, data); /* XXX error check */
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
		(*ifp->if_ioctl)(ifp, cmd, data); /* XXX error check */
		ifgr = (struct ifgroupreq *)data;
		if ((error = if_delgroup(ifp, ifgr->ifgr_group)))
			return (error);
		break;

	case SIOCSIFLLADDR:
		if ((error = suser(p, 0)))
			return (error);
		ifa = ifnet_addrs[ifp->if_index];
		if (ifa == NULL)
			return (EINVAL);
		sdl = (struct sockaddr_dl *)ifa->ifa_addr;
		if (sdl == NULL)
			return (EINVAL);
		if (ifr->ifr_addr.sa_len != ETHER_ADDR_LEN)
			return (EINVAL);
		if (ETHER_IS_MULTICAST(ifr->ifr_addr.sa_data))
			return (EINVAL);
		switch (ifp->if_type) {
		case IFT_ETHER:
		case IFT_CARP:
		case IFT_FDDI:
		case IFT_XETHER:
		case IFT_ISO88025:
		case IFT_L2VLAN:
			bcopy((caddr_t)ifr->ifr_addr.sa_data,
			    (caddr_t)((struct arpcom *)ifp)->ac_enaddr,
			    ETHER_ADDR_LEN);
			/* FALLTHROUGH */
		case IFT_ARCNET:
			bcopy((caddr_t)ifr->ifr_addr.sa_data,
			    LLADDR(sdl), ETHER_ADDR_LEN);
			break;
		default:
			return (ENODEV);
		}
		if (ifp->if_flags & IFF_UP) {
			struct ifreq ifrq;
			int s = splnet();
			ifp->if_flags &= ~IFF_UP;
			ifrq.ifr_flags = ifp->if_flags;
			(*ifp->if_ioctl)(ifp, SIOCSIFFLAGS, (caddr_t)&ifrq);
			ifp->if_flags |= IFF_UP;
			ifrq.ifr_flags = ifp->if_flags;
			(*ifp->if_ioctl)(ifp, SIOCSIFFLAGS, (caddr_t)&ifrq);
			splx(s);
			TAILQ_FOREACH(ifa, &ifp->if_addrlist, ifa_list) {
				if (ifa->ifa_addr != NULL &&
				    ifa->ifa_addr->sa_family == AF_INET)
					arp_ifinit((struct arpcom *)ifp, ifa);
			}
		}
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
		microtime(&ifp->if_lastchange);
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
ifconf(u_long cmd, caddr_t data)
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
				TAILQ_FOREACH(ifa,
				    &ifp->if_addrlist, ifa_list) {
					sa = ifa->ifa_addr;
#if defined(COMPAT_43) || defined(COMPAT_LINUX) || defined(COMPAT_SVR4)
					if (cmd != OSIOCGIFCONF)
#endif
					if (sa->sa_len > sizeof(*sa))
						space += sa->sa_len -
						    sizeof(*sa);
					space += sizeof(ifr);
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
					error = copyout((caddr_t)&ifr,
					    (caddr_t)ifrp, sizeof (ifr));
					ifrp++;
				} else
#endif
				if (sa->sa_len <= sizeof(*sa)) {
					ifr.ifr_addr = *sa;
					error = copyout((caddr_t)&ifr,
					    (caddr_t)ifrp, sizeof (ifr));
					ifrp++;
				} else {
					space -= sa->sa_len - sizeof(*sa);
					if (space < sizeof (ifr))
						break;
					error = copyout((caddr_t)&ifr,
					    (caddr_t)ifrp,
					    sizeof(ifr.ifr_name));
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
 * Create interface group without members
 */
struct ifg_group *
if_creategroup(const char *groupname)
{
	struct ifg_group	*ifg = NULL;

	if ((ifg = (struct ifg_group *)malloc(sizeof(struct ifg_group),
	    M_TEMP, M_NOWAIT)) == NULL)
		return (NULL);

	strlcpy(ifg->ifg_group, groupname, sizeof(ifg->ifg_group));
	ifg->ifg_refcnt = 0;
	ifg->ifg_carp_demoted = 0;
	TAILQ_INIT(&ifg->ifg_members);
#if NPF > 0
	pfi_attach_ifgroup(ifg);
#endif
	TAILQ_INSERT_TAIL(&ifg_head, ifg, ifg_next);

	return (ifg);
}

/*
 * Add a group to an interface
 */
int
if_addgroup(struct ifnet *ifp, const char *groupname)
{
	struct ifg_list		*ifgl;
	struct ifg_group	*ifg = NULL;
	struct ifg_member	*ifgm;

	if (groupname[0] && groupname[strlen(groupname) - 1] >= '0' &&
	    groupname[strlen(groupname) - 1] <= '9')
		return (EINVAL);

	TAILQ_FOREACH(ifgl, &ifp->if_groups, ifgl_next)
		if (!strcmp(ifgl->ifgl_group->ifg_group, groupname))
			return (EEXIST);

	if ((ifgl = (struct ifg_list *)malloc(sizeof(struct ifg_list), M_TEMP,
	    M_NOWAIT)) == NULL)
		return (ENOMEM);

	if ((ifgm = (struct ifg_member *)malloc(sizeof(struct ifg_member),
	    M_TEMP, M_NOWAIT)) == NULL) {
		free(ifgl, M_TEMP);
		return (ENOMEM);
	}

	TAILQ_FOREACH(ifg, &ifg_head, ifg_next)
		if (!strcmp(ifg->ifg_group, groupname))
			break;

	if (ifg == NULL && (ifg = if_creategroup(groupname)) == NULL) {
		free(ifgl, M_TEMP);
		free(ifgm, M_TEMP);
		return (ENOMEM);
	}

	ifg->ifg_refcnt++;
	ifgl->ifgl_group = ifg;
	ifgm->ifgm_ifp = ifp;

	TAILQ_INSERT_TAIL(&ifg->ifg_members, ifgm, ifgm_next);
	TAILQ_INSERT_TAIL(&ifp->if_groups, ifgl, ifgl_next);

#if NPF > 0
	pfi_group_change(groupname);
#endif

	return (0);
}

/*
 * Remove a group from an interface
 */
int
if_delgroup(struct ifnet *ifp, const char *groupname)
{
	struct ifg_list		*ifgl;
	struct ifg_member	*ifgm;

	TAILQ_FOREACH(ifgl, &ifp->if_groups, ifgl_next)
		if (!strcmp(ifgl->ifgl_group->ifg_group, groupname))
			break;
	if (ifgl == NULL)
		return (ENOENT);

	TAILQ_REMOVE(&ifp->if_groups, ifgl, ifgl_next);

	TAILQ_FOREACH(ifgm, &ifgl->ifgl_group->ifg_members, ifgm_next)
		if (ifgm->ifgm_ifp == ifp)
			break;

	if (ifgm != NULL) {
		TAILQ_REMOVE(&ifgl->ifgl_group->ifg_members, ifgm, ifgm_next);
		free(ifgm, M_TEMP);
	}

	if (--ifgl->ifgl_group->ifg_refcnt == 0) {
		TAILQ_REMOVE(&ifg_head, ifgl->ifgl_group, ifg_next);
#if NPF > 0
		pfi_detach_ifgroup(ifgl->ifgl_group);
#endif
		free(ifgl->ifgl_group, M_TEMP);
	}

	free(ifgl, M_TEMP);

#if NPF > 0
	pfi_group_change(groupname);
#endif

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
		bzero(&ifgrq, sizeof ifgrq);
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
 * Stores all members of a group in memory pointed to by data
 */
int
if_getgroupmembers(caddr_t data)
{
	struct ifgroupreq	*ifgr = (struct ifgroupreq *)data;
	struct ifg_group	*ifg;
	struct ifg_member	*ifgm;
	struct ifg_req		 ifgrq, *ifgp;
	int			 len, error;

	TAILQ_FOREACH(ifg, &ifg_head, ifg_next)
		if (!strcmp(ifg->ifg_group, ifgr->ifgr_name))
			break;
	if (ifg == NULL)
		return (ENOENT);

	if (ifgr->ifgr_len == 0) {
		TAILQ_FOREACH(ifgm, &ifg->ifg_members, ifgm_next)
			ifgr->ifgr_len += sizeof(ifgrq);
		return (0);
	}

	len = ifgr->ifgr_len;
	ifgp = ifgr->ifgr_groups;
	TAILQ_FOREACH(ifgm, &ifg->ifg_members, ifgm_next) {
		if (len < sizeof(ifgrq))
			return (EINVAL);
		bzero(&ifgrq, sizeof ifgrq);
		strlcpy(ifgrq.ifgrq_member, ifgm->ifgm_ifp->if_xname,
		    sizeof(ifgrq.ifgrq_member));
		if ((error = copyout((caddr_t)&ifgrq, (caddr_t)ifgp,
		    sizeof(struct ifg_req))))
			return (error);
		len -= sizeof(ifgrq);
		ifgp++;
	}

	return (0);
}

int
if_getgroupattribs(caddr_t data)
{
	struct ifgroupreq	*ifgr = (struct ifgroupreq *)data;
	struct ifg_group	*ifg;

	TAILQ_FOREACH(ifg, &ifg_head, ifg_next)
		if (!strcmp(ifg->ifg_group, ifgr->ifgr_name))
			break;
	if (ifg == NULL)
		return (ENOENT);

	ifgr->ifgr_attrib.ifg_carp_demoted = ifg->ifg_carp_demoted;

	return (0);
}

int
if_setgroupattribs(caddr_t data)
{
	struct ifgroupreq	*ifgr = (struct ifgroupreq *)data;
	struct ifg_group	*ifg;
	struct ifg_member	*ifgm;
	int			 demote;

	TAILQ_FOREACH(ifg, &ifg_head, ifg_next)
		if (!strcmp(ifg->ifg_group, ifgr->ifgr_name))
			break;
	if (ifg == NULL)
		return (ENOENT);

	demote = ifgr->ifgr_attrib.ifg_carp_demoted;
	if (demote + ifg->ifg_carp_demoted > 0xff ||
	    demote + ifg->ifg_carp_demoted < 0)
		return (ERANGE);

	ifg->ifg_carp_demoted += demote;

	TAILQ_FOREACH(ifgm, &ifg->ifg_members, ifgm_next)
		if (ifgm->ifgm_ifp->if_ioctl)
			ifgm->ifgm_ifp->if_ioctl(ifgm->ifgm_ifp,
			    SIOCSIFGATTR, data);
	return (0);
}

void
if_group_routechange(struct sockaddr *dst, struct sockaddr *mask)
{
	switch (dst->sa_family) {
	case AF_INET:
		if (satosin(dst)->sin_addr.s_addr == INADDR_ANY)
			if_group_egress_build();
		break;
#ifdef INET6
	case AF_INET6:
		if (IN6_ARE_ADDR_EQUAL(&(satosin6(dst))->sin6_addr,
		    &in6addr_any) &&
		    mask && IN6_ARE_ADDR_EQUAL(&(satosin6(mask))->sin6_addr,
		    &in6addr_any))
			if_group_egress_build();
		break;
#endif
	}
}

int
if_group_egress_build(void)
{
	struct ifg_group	*ifg;
	struct ifg_member	*ifgm, *next;
	struct sockaddr_in	 sa_in;
#ifdef INET6
	struct sockaddr_in6	 sa_in6;
#endif
	struct radix_node	*rn;
	struct rtentry		*rt;

	TAILQ_FOREACH(ifg, &ifg_head, ifg_next)
		if (!strcmp(ifg->ifg_group, IFG_EGRESS))
			break;

	if (ifg != NULL)
		for (ifgm = TAILQ_FIRST(&ifg->ifg_members); ifgm; ifgm = next) {
			next = TAILQ_NEXT(ifgm, ifgm_next);
			if_delgroup(ifgm->ifgm_ifp, IFG_EGRESS);
		}

	bzero(&sa_in, sizeof(sa_in));
	sa_in.sin_len = sizeof(sa_in);
	sa_in.sin_family = AF_INET;
	if ((rn = rt_lookup(sintosa(&sa_in), sintosa(&sa_in), 0)) != NULL) {
		do {
			rt = (struct rtentry *)rn;
			if (rt->rt_ifp)
				if_addgroup(rt->rt_ifp, IFG_EGRESS);
#ifndef SMALL_KERNEL
			rn = rn_mpath_next(rn);
#else
			rn = NULL;
#endif
		} while (rn != NULL);
	}

#ifdef INET6
	bcopy(&sa6_any, &sa_in6, sizeof(sa_in6));
	if ((rn = rt_lookup(sin6tosa(&sa_in6), sin6tosa(&sa_in6), 0)) != NULL) {
		do {
			rt = (struct rtentry *)rn;
			if (rt->rt_ifp)
				if_addgroup(rt->rt_ifp, IFG_EGRESS);
#ifndef SMALL_KERNEL
			rn = rn_mpath_next(rn);
#else
			rn = NULL;
#endif
		} while (rn != NULL);
	}
#endif

	return (0);
}

/*
 * Set/clear promiscuous mode on interface ifp based on the truth value
 * of pswitch.  The calls are reference counted so that only the first
 * "on" request actually has an effect, as does the final "off" request.
 * Results are undefined if the "off" and "on" requests are not matched.
 */
int
ifpromisc(struct ifnet *ifp, int pswitch)
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

int
sysctl_ifq(int *name, u_int namelen, void *oldp, size_t *oldlenp,
    void *newp, size_t newlen, struct ifqueue *ifq)
{
	/* All sysctl names at this level are terminal. */
	if (namelen != 1)
		return (ENOTDIR);

	switch (name[0]) {
	case IFQCTL_LEN:
		return (sysctl_rdint(oldp, oldlenp, newp, ifq->ifq_len));
	case IFQCTL_MAXLEN:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &ifq->ifq_maxlen));
	case IFQCTL_DROPS:
		return (sysctl_rdint(oldp, oldlenp, newp, ifq->ifq_drops));
	default:
		return (EOPNOTSUPP);
	}
	/* NOTREACHED */
}

void
netrndintr(void)
{
	add_net_randomness(0);
}
