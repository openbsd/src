/*	$OpenBSD: if.c,v 1.327 2015/04/07 10:46:20 mpi Exp $	*/
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
#include "trunk.h"
#include "ether.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/timeout.h>
#include <sys/protosw.h>
#include <sys/kernel.h>
#include <sys/ioctl.h>
#include <sys/domain.h>
#include <sys/sysctl.h>
#include <sys/task.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/route.h>
#include <net/netisr.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/igmp.h>
#ifdef MROUTING
#include <netinet/ip_mroute.h>
#endif

#ifdef INET6
#include <netinet6/in6_var.h>
#include <netinet6/in6_ifattach.h>
#include <netinet6/nd6.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#endif

#ifdef MPLS
#include <netmpls/mpls.h>
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
void	if_attach_common(struct ifnet *);

void	if_detach_queues(struct ifnet *, struct ifqueue *);
void	if_detached_start(struct ifnet *);
int	if_detached_ioctl(struct ifnet *, u_long, caddr_t);

int	if_getgroup(caddr_t, struct ifnet *);
int	if_getgroupmembers(caddr_t);
int	if_getgroupattribs(caddr_t);
int	if_setgroupattribs(caddr_t);

int	if_clone_list(struct if_clonereq *);
struct if_clone	*if_clone_lookup(const char *, int *);

int	if_group_egress_build(void);

void	if_link_state_change_task(void *);

void	if_input_process(void *);

#ifdef DDB
void	ifa_print_all(void);
#endif

TAILQ_HEAD(, ifg_group) ifg_head = TAILQ_HEAD_INITIALIZER(ifg_head);
LIST_HEAD(, if_clone) if_cloners = LIST_HEAD_INITIALIZER(if_cloners);
int if_cloners_count;

struct timeout net_tick_to;
void	net_tick(void *);
int	net_livelocked(void);
int	ifq_congestion;

struct taskq *softnettq;

/*
 * Network interface utility routines.
 *
 * Routines with ifa_ifwith* names take sockaddr *'s as
 * parameters.
 */
void
ifinit()
{
	timeout_set(&net_tick_to, net_tick, &net_tick_to);

	softnettq = taskq_create("softnet", 1, IPL_NET,
	    TASKQ_MPSAFE | TASKQ_CANTSLEEP);
	if (softnettq == NULL)
		panic("unable to create softnet taskq");

	net_tick(&net_tick_to);
}

static unsigned int if_index = 0;
static unsigned int if_indexlim = 0;
struct ifnet **ifindex2ifnet = NULL;
struct ifnet_head ifnet = TAILQ_HEAD_INITIALIZER(ifnet);
struct ifnet_head iftxlist = TAILQ_HEAD_INITIALIZER(iftxlist);
struct ifnet *lo0ifp;

/*
 * Attach an interface to the
 * list of "active" interfaces.
 */
void
if_attachsetup(struct ifnet *ifp)
{
	int wrapped = 0;

	/*
	 * Always increment the index to avoid races.
	 */
	if_index++;

	/*
	 * If we hit USHRT_MAX, we skip back to 1 since there are a
	 * number of places where the value of ifp->if_index or
	 * if_index itself is compared to or stored in an unsigned
	 * short.  By jumping back, we won't botch those assignments
	 * or comparisons.
	 */
	if (if_index == USHRT_MAX) {
		if_index = 1;
		wrapped++;
	}

	while (if_index < if_indexlim && ifindex2ifnet[if_index] != NULL) {
		if_index++;

		if (if_index == USHRT_MAX) {
			/*
			 * If we have to jump back to 1 twice without
			 * finding an empty slot then there are too many
			 * interfaces.
			 */
			if (wrapped)
				panic("too many interfaces");

			if_index = 1;
			wrapped++;
		}
	}
	ifp->if_index = if_index;

	/*
	 * We have some arrays that should be indexed by if_index.
	 * since if_index will grow dynamically, they should grow too.
	 *	struct ifnet **ifindex2ifnet
	 */
	if (ifindex2ifnet == NULL || if_index >= if_indexlim) {
		size_t m, n, oldlim;
		caddr_t q;

		oldlim = if_indexlim;
		if (if_indexlim == 0)
			if_indexlim = 8;
		while (if_index >= if_indexlim)
			if_indexlim <<= 1;

		/* grow ifindex2ifnet */
		m = oldlim * sizeof(struct ifnet *);
		n = if_indexlim * sizeof(struct ifnet *);
		q = (caddr_t)malloc(n, M_IFADDR, M_WAITOK|M_ZERO);
		if (ifindex2ifnet) {
			bcopy((caddr_t)ifindex2ifnet, q, m);
			free((caddr_t)ifindex2ifnet, M_IFADDR, 0);
		}
		ifindex2ifnet = (struct ifnet **)q;
	}

	TAILQ_INIT(&ifp->if_groups);

	if_addgroup(ifp, IFG_ALL);

	ifindex2ifnet[if_index] = ifp;

	if (ifp->if_snd.ifq_maxlen == 0)
		IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);

	if (domains)
		if_attachdomain1(ifp);
#if NPF > 0
	pfi_attach_ifnet(ifp);
#endif

	timeout_set(ifp->if_slowtimo, if_slowtimo, ifp);
	if_slowtimo(ifp);

	task_set(ifp->if_linkstatetask, if_link_state_change_task, ifp);

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
	unsigned int socksize;
	int namelen, masklen;
	struct sockaddr_dl *sdl;

	/*
	 * If the interface already has a link name, release it
	 * now.  This is useful for interfaces that can change
	 * link types, and thus switch link names often.
	 */
	if (ifp->if_sadl != NULL)
		if_free_sadl(ifp);

	namelen = strlen(ifp->if_xname);
	masklen = offsetof(struct sockaddr_dl, sdl_data[0]) + namelen;
	socksize = masklen + ifp->if_addrlen;
#define ROUNDUP(a) (1 + (((a) - 1) | (sizeof(long) - 1)))
	if (socksize < sizeof(*sdl))
		socksize = sizeof(*sdl);
	socksize = ROUNDUP(socksize);
	sdl = malloc(socksize, M_IFADDR, M_WAITOK|M_ZERO);
	sdl->sdl_len = socksize;
	sdl->sdl_family = AF_LINK;
	bcopy(ifp->if_xname, sdl->sdl_data, namelen);
	sdl->sdl_nlen = namelen;
	sdl->sdl_alen = ifp->if_addrlen;
	sdl->sdl_index = ifp->if_index;
	sdl->sdl_type = ifp->if_type;
	ifp->if_sadl = sdl;
}

/*
 * Free the link level name for the specified interface.  This is
 * a detach helper.  This is called from if_detach() or from
 * link layer type specific detach functions.
 */
void
if_free_sadl(struct ifnet *ifp)
{
	free(ifp->if_sadl, M_IFADDR, 0);
	ifp->if_sadl = NULL;
}

void
if_attachdomain()
{
	struct ifnet *ifp;
	int s;

	s = splnet();
	TAILQ_FOREACH(ifp, &ifnet, if_list)
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
	if_attach_common(ifp);
	TAILQ_INSERT_HEAD(&ifnet, ifp, if_list);
	if_attachsetup(ifp);
}

void
if_attach(struct ifnet *ifp)
{
#if NCARP > 0
	struct ifnet *before = NULL;
#endif

	if_attach_common(ifp);

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

void
if_attach_common(struct ifnet *ifp)
{
	TAILQ_INIT(&ifp->if_addrlist);
	TAILQ_INIT(&ifp->if_maddrlist);

	ifp->if_addrhooks = malloc(sizeof(*ifp->if_addrhooks),
	    M_TEMP, M_WAITOK);
	TAILQ_INIT(ifp->if_addrhooks);
	ifp->if_linkstatehooks = malloc(sizeof(*ifp->if_linkstatehooks),
	    M_TEMP, M_WAITOK);
	TAILQ_INIT(ifp->if_linkstatehooks);
	ifp->if_detachhooks = malloc(sizeof(*ifp->if_detachhooks),
	    M_TEMP, M_WAITOK);
	TAILQ_INIT(ifp->if_detachhooks);

	ifp->if_slowtimo = malloc(sizeof(*ifp->if_slowtimo), M_TEMP,
	    M_WAITOK|M_ZERO);
	ifp->if_linkstatetask = malloc(sizeof(*ifp->if_linkstatetask),
	    M_TEMP, M_WAITOK|M_ZERO);

	SLIST_INIT(&ifp->if_inputs);
}

void
if_start(struct ifnet *ifp)
{

	splassert(IPL_NET);

	if (ifp->if_snd.ifq_len >= min(8, ifp->if_snd.ifq_maxlen) &&
	    !ISSET(ifp->if_flags, IFF_OACTIVE)) {
		if (ISSET(ifp->if_xflags, IFXF_TXREADY)) {
			TAILQ_REMOVE(&iftxlist, ifp, if_txlist);
			CLR(ifp->if_xflags, IFXF_TXREADY);
		}
		ifp->if_start(ifp);
		return;
	}

	if (!ISSET(ifp->if_xflags, IFXF_TXREADY)) {
		SET(ifp->if_xflags, IFXF_TXREADY);
		TAILQ_INSERT_TAIL(&iftxlist, ifp, if_txlist);
		schednetisr(NETISR_TX);
	}
}

struct mbuf_queue if_input_queue = MBUF_QUEUE_INITIALIZER(8192, IPL_NET);
struct task if_input_task = TASK_INITIALIZER(if_input_process, &if_input_queue);

void
if_input(struct ifnet *ifp, struct mbuf_list *ml)
{
	struct mbuf *m;

	splassert(IPL_NET);

	MBUF_LIST_FOREACH(ml, m) {
		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.ph_rtableid = ifp->if_rdomain;
	}

#if NBPFILTER > 0
	if (ifp->if_bpf) {
		KERNEL_LOCK();
		MBUF_LIST_FOREACH(ml, m)
			bpf_mtap_ether(ifp->if_bpf, m, BPF_DIRECTION_IN);
		KERNEL_UNLOCK();
	}
#endif

	mq_enlist(&if_input_queue, ml);
	task_add(softnettq, &if_input_task);
}

void
ether_input_mbuf(struct ifnet *ifp, struct mbuf *m)
{
	mq_enqueue(&if_input_queue, m);
	task_add(softnettq, &if_input_task);
}

void
if_input_process(void *xmq)
{
	struct mbuf_queue *mq = xmq;
	struct mbuf_list ml;
	struct mbuf *m;
	struct ifnet *ifp;
	struct ifih *ifih;
	int mit = 0;
	int s;

	mq_delist(mq, &ml);
	if (ml_empty(&ml))
		return;

	KERNEL_LOCK();
	s = splnet();
	while ((m = ml_dequeue(&ml)) != NULL) {
		if ((++mit & 0x1f) == 0)
			yield();

		ifp = m->m_pkthdr.rcvif;
		SLIST_FOREACH(ifih, &ifp->if_inputs, ifih_next) {
			if ((*ifih->ifih_input)(m, NULL))
				break;
		}
	}
	splx(s);
	KERNEL_UNLOCK();
}

void
nettxintr(void)
{
	struct ifnet *ifp;
	int s;

	s = splnet();
	while ((ifp = TAILQ_FIRST(&iftxlist)) != NULL) {
		TAILQ_REMOVE(&iftxlist, ifp, if_txlist);
		CLR(ifp->if_xflags, IFXF_TXREADY);
		ifp->if_start(ifp);
	}
	splx(s);
}

/*
 * Detach an interface from everything in the kernel.  Also deallocate
 * private resources.
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
	ifp->if_watchdog = NULL;

	/*
	 * Call detach hooks from head to tail.  To make sure detach
	 * hooks are executed in the reverse order they were added, all
	 * the hooks have to be added to the head!
	 */
	dohooks(ifp->if_detachhooks, HOOK_REMOVE | HOOK_FREE);

	/* Remove the watchdog timeout */
	timeout_del(ifp->if_slowtimo);

	/* Remove the link state task */
	task_del(systq, ifp->if_linkstatetask);

#if NBRIDGE > 0
	/* Remove the interface from any bridge it is part of.  */
	if (ifp->if_bridgeport)
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
	rt_if_remove(ifp);
	rti_delete(ifp);
#if NETHER > 0 && defined(NFSCLIENT) 
	if (ifp == revarp_ifp)
		revarp_ifp = NULL;
#endif
#ifdef MROUTING
	vif_delete(ifp);
#endif
	in_ifdetach(ifp);
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
	IF_DETACH_QUEUES(arpintrq);
	IF_DETACH_QUEUES(ipintrq);
#ifdef INET6
	IF_DETACH_QUEUES(ip6intrq);
#endif
#undef IF_DETACH_QUEUES

	/* Remove the interface from the list of all interfaces.  */
	TAILQ_REMOVE(&ifnet, ifp, if_list);
	if (ISSET(ifp->if_xflags, IFXF_TXREADY))
		TAILQ_REMOVE(&iftxlist, ifp, if_txlist);

	while ((ifg = TAILQ_FIRST(&ifp->if_groups)) != NULL)
		if_delgroup(ifp, ifg->ifgl_group->ifg_group);

	if_free_sadl(ifp);

	/* We should not have any address left at this point. */
	if (!TAILQ_EMPTY(&ifp->if_addrlist)) {
#ifdef DIAGNOSTIC
		printf("%s: address list non empty\n", ifp->if_xname);
#endif
		while ((ifa = TAILQ_FIRST(&ifp->if_addrlist)) != NULL) {
			ifa_del(ifp, ifa);
			ifa->ifa_ifp = NULL;
			ifafree(ifa);
		}
	}

	free(ifp->if_addrhooks, M_TEMP, 0);
	free(ifp->if_linkstatehooks, M_TEMP, 0);
	free(ifp->if_detachhooks, M_TEMP, 0);

	free(ifp->if_slowtimo, M_TEMP, sizeof(*ifp->if_slowtimo));
	free(ifp->if_linkstatetask, M_TEMP, sizeof(*ifp->if_linkstatetask));

	for (dp = domains; dp; dp = dp->dom_next) {
		if (dp->dom_ifdetach && ifp->if_afdata[dp->dom_family])
			(*dp->dom_ifdetach)(ifp,
			    ifp->if_afdata[dp->dom_family]);
	}

	/* Announce that the interface is gone. */
	rt_ifannouncemsg(ifp, IFAN_DEPARTURE);

	ifindex2ifnet[ifp->if_index] = NULL;
	splx(s);
}

void
if_detach_queues(struct ifnet *ifp, struct ifqueue *q)
{
	struct mbuf *m, *prev = NULL, *next;
	int prio;

	for (prio = 0; prio <= IFQ_MAXPRIO; prio++) {
		for (m = q->ifq_q[prio].head; m; m = next) {
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
				q->ifq_q[prio].head = m->m_nextpkt;
			if (q->ifq_q[prio].tail == m)
				q->ifq_q[prio].tail = prev;
			q->ifq_len--;

			m->m_nextpkt = NULL;
			m_freem(m);
			IF_DROP(q);
		}
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
	int s;

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

	return ((*ifc->ifc_destroy)(ifp));
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

	LIST_FOREACH(ifc, &if_cloners, ifc_list) {
		if (count == 0)
			break;
		bzero(outbuf, sizeof outbuf);
		strlcpy(outbuf, ifc->ifc_name, IFNAMSIZ);
		error = copyout(outbuf, dst, IFNAMSIZ);
		if (error)
			break;
		count--;
		dst += IFNAMSIZ;
	}

	return (error);
}

/*
 * set queue congestion marker
 */
void
if_congestion(void)
{
	extern int ticks;

	ifq_congestion = ticks;
}

int
if_congested(void)
{
	extern int ticks;

	return (ticks - ifq_congestion <= (hz / 100));
}

#define	equal(a1, a2)	\
	(bcmp((caddr_t)(a1), (caddr_t)(a2),	\
	((struct sockaddr *)(a1))->sa_len) == 0)

/*
 * Locate an interface based on a complete address.
 */
struct ifaddr *
ifa_ifwithaddr(struct sockaddr *addr, u_int rtableid)
{
	struct ifnet *ifp;
	struct ifaddr *ifa;
	u_int rdomain;

	rdomain = rtable_l2(rtableid);
	TAILQ_FOREACH(ifp, &ifnet, if_list) {
		if (ifp->if_rdomain != rdomain)
			continue;

		TAILQ_FOREACH(ifa, &ifp->if_addrlist, ifa_list) {
			if (ifa->ifa_addr->sa_family != addr->sa_family)
				continue;

			if (equal(addr, ifa->ifa_addr))
				return (ifa);

			/* IPv6 doesn't have broadcast */
			if ((ifp->if_flags & IFF_BROADCAST) &&
			    ifa->ifa_broadaddr &&
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
ifa_ifwithdstaddr(struct sockaddr *addr, u_int rdomain)
{
	struct ifnet *ifp;
	struct ifaddr *ifa;

	rdomain = rtable_l2(rdomain);
	TAILQ_FOREACH(ifp, &ifnet, if_list) {
		if (ifp->if_rdomain != rdomain)
			continue;
		if (ifp->if_flags & IFF_POINTOPOINT)
			TAILQ_FOREACH(ifa, &ifp->if_addrlist, ifa_list) {
				if (ifa->ifa_addr->sa_family !=
				    addr->sa_family || ifa->ifa_dstaddr == NULL)
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
ifa_ifwithnet(struct sockaddr *sa, u_int rtableid)
{
	struct ifnet *ifp;
	struct ifaddr *ifa, *ifa_maybe = NULL;
	char *cplim, *addr_data = sa->sa_data;
	u_int rdomain;

	rdomain = rtable_l2(rtableid);
	TAILQ_FOREACH(ifp, &ifnet, if_list) {
		if (ifp->if_rdomain != rdomain)
			continue;
		TAILQ_FOREACH(ifa, &ifp->if_addrlist, ifa_list) {
			char *cp, *cp2, *cp3;

			if (ifa->ifa_addr->sa_family != sa->sa_family ||
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
link_rtrequest(int cmd, struct rtentry *rt)
{
	struct ifaddr *ifa;
	struct sockaddr *dst;
	struct ifnet *ifp;

	if (cmd != RTM_ADD || ((ifa = rt->rt_ifa) == 0) ||
	    ((ifp = ifa->ifa_ifp) == 0) || ((dst = rt_key(rt)) == 0))
		return;
	if ((ifa = ifaof_ifpforaddr(dst, ifp)) != NULL) {
		ifa->ifa_refcnt++;
		ifafree(rt->rt_ifa);
		rt->rt_ifa = ifa;
		if (ifa->ifa_rtrequest && ifa->ifa_rtrequest != link_rtrequest)
			ifa->ifa_rtrequest(cmd, rt);
	}
}

/*
 * Default action when installing a local route on a point-to-point
 * interface.
 */
void
p2p_rtrequest(int req, struct rtentry *rt)
{
	struct ifnet *ifp = rt->rt_ifp;
	struct ifaddr *ifa, *lo0ifa;

	switch (req) {
	case RTM_ADD:
		if ((rt->rt_flags & RTF_LOCAL) == 0)
			break;

		TAILQ_FOREACH(ifa, &ifp->if_addrlist, ifa_list) {
			if (memcmp(rt_key(rt), ifa->ifa_addr,
			    rt_key(rt)->sa_len) == 0)
				break;
		}

		if (ifa == NULL)
			break;

		/*
		 * XXX Since lo0 is in the default rdomain we should not
		 * (ab)use it for any route related to an interface of a
		 * different rdomain.
		 */
		TAILQ_FOREACH(lo0ifa, &lo0ifp->if_addrlist, ifa_list)
			if (lo0ifa->ifa_addr->sa_family ==
			    ifa->ifa_addr->sa_family)
				break;

		if (lo0ifa == NULL)
			break;

		rt->rt_ifp = lo0ifp;
		rt->rt_flags &= ~RTF_LLINFO;

		/*
		 * make sure to set rt->rt_ifa to the interface
		 * address we are using, otherwise we will have trouble
		 * with source address selection.
		 */
		if (ifa != rt->rt_ifa) {
			ifafree(rt->rt_ifa);
			ifa->ifa_refcnt++;
			rt->rt_ifa = ifa;
		}
		break;
	case RTM_DELETE:
	case RTM_RESOLVE:
	default:
		break;
	}
}


/*
 * Bring down all interfaces
 */
void
if_downall(void)
{
	struct ifreq ifrq;	/* XXX only partly built */
	struct ifnet *ifp;
	int s;

	s = splnet();
	TAILQ_FOREACH(ifp, &ifnet, if_list) {
		if ((ifp->if_flags & IFF_UP) == 0)
			continue;
		if_down(ifp);
		ifp->if_flags &= ~IFF_UP;

		if (ifp->if_ioctl) {
			ifrq.ifr_flags = ifp->if_flags;
			(void) (*ifp->if_ioctl)(ifp, SIOCSIFFLAGS,
			    (caddr_t)&ifrq);
		}
	}
	splx(s);
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

	splsoftassert(IPL_SOFTNET);

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
	if (ifp->if_bridgeport)
		bstp_ifstate(ifp);
#endif
	rt_ifmsg(ifp);
#ifndef SMALL_KERNEL
	rt_if_track(ifp);
#endif
}

/*
 * Mark an interface up and notify protocols of
 * the transition.
 * NOTE: must be called at splsoftnet or equivalent.
 */
void
if_up(struct ifnet *ifp)
{
	splsoftassert(IPL_SOFTNET);

	ifp->if_flags |= IFF_UP;
	microtime(&ifp->if_lastchange);
#if NCARP > 0
	if (ifp->if_carp)
		carp_carpdev_state(ifp);
#endif
#if NBRIDGE > 0
	if (ifp->if_bridgeport)
		bstp_ifstate(ifp);
#endif
	rt_ifmsg(ifp);
#ifdef INET6
	/* Userland expects the kernel to set ::1 on lo0. */
	if (ifp == lo0ifp)
		in6_ifattach(ifp);
#endif
#ifndef SMALL_KERNEL
	rt_if_track(ifp);
#endif
}

/*
 * Schedule a link state change task.
 */
void
if_link_state_change(struct ifnet *ifp)
{
	/* put the routing table update task on systq */
	task_add(systq, ifp->if_linkstatetask);
}

/*
 * Process a link state change.
 */
void
if_link_state_change_task(void *arg)
{
	struct ifnet *ifp = arg;
	int s;

	s = splsoftnet();
	rt_ifmsg(ifp);
#ifndef SMALL_KERNEL
	rt_if_track(ifp);
#endif
	dohooks(ifp->if_linkstatehooks, 0);
	splx(s);
}

/*
 * Handle interface watchdog timer routine.  Called
 * from softclock, we decrement timer (if set) and
 * call the appropriate interface routine on expiration.
 */
void
if_slowtimo(void *arg)
{
	struct ifnet *ifp = arg;
	int s = splnet();

	if (ifp->if_watchdog) {
		if (ifp->if_timer > 0 && --ifp->if_timer == 0)
			(*ifp->if_watchdog)(ifp);
		timeout_add(ifp->if_slowtimo, hz / IFNET_SLOWHZ);
	}
	splx(s);
}

/*
 * Map interface name to interface structure pointer.
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
 * Map interface index to interface structure pointer.
 */
struct ifnet *
if_get(unsigned int index)
{
	struct ifnet *ifp = NULL;

	if (index < if_indexlim)
		ifp = ifindex2ifnet[index];

	return (ifp);
}

/*
 * Interface ioctls.
 */
int
ifioctl(struct socket *so, u_long cmd, caddr_t data, struct proc *p)
{
	struct ifnet *ifp;
	struct ifreq *ifr;
	struct sockaddr_dl *sdl;
	struct ifgroupreq *ifgr;
	struct if_afreq *ifar;
	char ifdescrbuf[IFDESCRSIZE];
	char ifrtlabelbuf[RTLABEL_LEN];
	int s, error = 0;
	size_t bytesdone;
	short oif_flags;
	const char *label;
	short up = 0;

	switch (cmd) {

	case SIOCGIFCONF:
#ifdef COMPAT_LINUX
	case OSIOCGIFCONF:
#endif
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
	case SIOCIFAFATTACH:
	case SIOCIFAFDETACH:
		if ((error = suser(p, 0)) != 0)
			return (error);
		ifar = (struct if_afreq *)data;
		if ((ifp = ifunit(ifar->ifar_name)) == NULL)
			return (ENXIO);
		switch (ifar->ifar_af) {
		case AF_INET:
			/* attach is a noop for AF_INET */
			if (cmd == SIOCIFAFDETACH) {
				s = splsoftnet();
				in_ifdetach(ifp);
				splx(s);
			}
			return (0);
#ifdef INET6
		case AF_INET6:
			s = splsoftnet();
			if (cmd == SIOCIFAFATTACH)
				in6_ifattach(ifp);
			else
				in6_ifdetach(ifp);
			splx(s);
			return (0);
#endif /* INET6 */
		default:
			return (EAFNOSUPPORT);
		}
	}

	ifp = ifunit(ifr->ifr_name);
	if (ifp == 0)
		return (ENXIO);
	oif_flags = ifp->if_flags;
	switch (cmd) {

	case SIOCGIFFLAGS:
		ifr->ifr_flags = ifp->if_flags;
		break;

	case SIOCGIFXFLAGS:
		ifr->ifr_flags = ifp->if_xflags;
		break;

	case SIOCGIFMETRIC:
		ifr->ifr_metric = ifp->if_metric;
		break;

	case SIOCGIFMTU:
		ifr->ifr_mtu = ifp->if_mtu;
		break;

	case SIOCGIFHARDMTU:
		ifr->ifr_hardmtu = ifp->if_hardmtu;
		break;

	case SIOCGIFDATA:
		error = copyout((caddr_t)&ifp->if_data, ifr->ifr_data,
		    sizeof(ifp->if_data));
		break;

	case SIOCSIFFLAGS:
		if ((error = suser(p, 0)) != 0)
			return (error);
		if (ifp->if_flags & IFF_UP && (ifr->ifr_flags & IFF_UP) == 0) {
			s = splnet();
			if_down(ifp);
			splx(s);
		}
		if (ifr->ifr_flags & IFF_UP && (ifp->if_flags & IFF_UP) == 0) {
			s = splnet();
			if_up(ifp);
			splx(s);
		}
		ifp->if_flags = (ifp->if_flags & IFF_CANTCHANGE) |
			(ifr->ifr_flags & ~IFF_CANTCHANGE);
		if (ifp->if_ioctl)
			(void) (*ifp->if_ioctl)(ifp, cmd, data);
		break;

	case SIOCSIFXFLAGS:
		if ((error = suser(p, 0)) != 0)
			return (error);

#ifdef INET6
		if (ISSET(ifr->ifr_flags, IFXF_AUTOCONF6)) {
			s = splsoftnet();
			in6_ifattach(ifp);
			splx(s);
		}

		if ((ifr->ifr_flags & IFXF_AUTOCONF6) &&
		    !(ifp->if_xflags & IFXF_AUTOCONF6)) {
			nd6_rs_timeout_count++;
			RS_LHCOOKIE(ifp) = hook_establish(
			    ifp->if_linkstatehooks, 1, nd6_rs_dev_state, ifp);
			if (!timeout_pending(&nd6_rs_output_timer))
				nd6_rs_output_set_timo(
				    ND6_RS_OUTPUT_QUICK_INTERVAL);
		}

		if ((ifp->if_xflags & IFXF_AUTOCONF6) &&
		    !(ifr->ifr_flags & IFXF_AUTOCONF6)) {
			hook_disestablish(ifp->if_linkstatehooks,
			    RS_LHCOOKIE(ifp));
			nd6_rs_timeout_count--;
			if (nd6_rs_timeout_count == 0)
				timeout_del(&nd6_rs_output_timer);
		}
#endif	/* INET6 */

#ifdef MPLS
		if (ISSET(ifr->ifr_flags, IFXF_MPLS) &&
		    !ISSET(ifp->if_xflags, IFXF_MPLS)) {
			s = splnet();
			ifp->if_xflags |= IFXF_MPLS;
			ifp->if_ll_output = ifp->if_output; 
			ifp->if_output = mpls_output;
			splx(s);
		}
		if (ISSET(ifp->if_xflags, IFXF_MPLS) &&
		    !ISSET(ifr->ifr_flags, IFXF_MPLS)) {
			s = splnet();
			ifp->if_xflags &= ~IFXF_MPLS;
			ifp->if_output = ifp->if_ll_output; 
			ifp->if_ll_output = NULL;
			splx(s);
		}
#endif	/* MPLS */

#ifndef SMALL_KERNEL
		if (ifp->if_capabilities & IFCAP_WOL) {
			if (ISSET(ifr->ifr_flags, IFXF_WOL) &&
			    !ISSET(ifp->if_xflags, IFXF_WOL)) {
				s = splnet();
				ifp->if_xflags |= IFXF_WOL;
				error = ifp->if_wol(ifp, 1);
				splx(s);
				if (error)
					return (error);
			}
			if (ISSET(ifp->if_xflags, IFXF_WOL) &&
			    !ISSET(ifr->ifr_flags, IFXF_WOL)) {
				s = splnet();
				ifp->if_xflags &= ~IFXF_WOL;
				error = ifp->if_wol(ifp, 0);
				splx(s);
				if (error)
					return (error);
			}
		} else if (ISSET(ifr->ifr_flags, IFXF_WOL)) {
			ifr->ifr_flags &= ~IFXF_WOL;
			error = ENOTSUP;
		}
#endif

		ifp->if_xflags = (ifp->if_xflags & IFXF_CANTCHANGE) |
			(ifr->ifr_flags & ~IFXF_CANTCHANGE);
		rt_ifmsg(ifp);
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
	case SIOCSLIFPHYRTABLE:
	case SIOCADDMULTI:
	case SIOCDELMULTI:
	case SIOCSIFMEDIA:
		if ((error = suser(p, 0)) != 0)
			return (error);
		/* FALLTHROUGH */
	case SIOCGIFPSRCADDR:
	case SIOCGIFPDSTADDR:
	case SIOCGLIFPHYADDR:
	case SIOCGLIFPHYRTABLE:
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

	case SIOCGIFRTLABEL:
		if (ifp->if_rtlabelid &&
		    (label = rtlabel_id2name(ifp->if_rtlabelid)) != NULL) {
			strlcpy(ifrtlabelbuf, label, RTLABEL_LEN);
			error = copyoutstr(ifrtlabelbuf, ifr->ifr_data,
			    RTLABEL_LEN, &bytesdone);
		} else
			error = ENOENT;
		break;

	case SIOCSIFRTLABEL:
		if ((error = suser(p, 0)) != 0)
			return (error);
		error = copyinstr(ifr->ifr_data, ifrtlabelbuf,
		    RTLABEL_LEN, &bytesdone);
		if (error == 0) {
			rtlabel_unref(ifp->if_rtlabelid);
			ifp->if_rtlabelid = rtlabel_name2id(ifrtlabelbuf);
		}
		break;

	case SIOCGIFPRIORITY:
		ifr->ifr_metric = ifp->if_priority;
		break;

	case SIOCSIFPRIORITY:
		if ((error = suser(p, 0)) != 0)
			return (error);
		if (ifr->ifr_metric < 0 || ifr->ifr_metric > 15)
			return (EINVAL);
		ifp->if_priority = ifr->ifr_metric;
		break;

	case SIOCGIFRDOMAIN:
		ifr->ifr_rdomainid = ifp->if_rdomain;
		break;

	case SIOCSIFRDOMAIN:
		if ((error = suser(p, 0)) != 0)
			return (error);
		if (ifr->ifr_rdomainid < 0 ||
		    ifr->ifr_rdomainid > RT_TABLEID_MAX)
			return (EINVAL);

		/* make sure that the routing table exists */
		if (!rtable_exists(ifr->ifr_rdomainid)) {
			s = splsoftnet();
			if ((error = rtable_add(ifr->ifr_rdomainid)) == 0)
				rtable_l2set(ifr->ifr_rdomainid, ifr->ifr_rdomainid);
			splx(s);
			if (error)
				return (error);
		}

		/* make sure that the routing table is a real rdomain */
		if (ifr->ifr_rdomainid != rtable_l2(ifr->ifr_rdomainid))
			return (EINVAL);

		/* remove all routing entries when switching domains */
		/* XXX hell this is ugly */
		if (ifr->ifr_rdomainid != ifp->if_rdomain) {
			s = splnet();
			if (ifp->if_flags & IFF_UP)
				up = 1;
			/*
			 * We are tearing down the world.
			 * Take down the IF so:
			 * 1. everything that cares gets a message
			 * 2. the automagic IPv6 bits are recreated
			 */
			if (up)
				if_down(ifp);
			rt_if_remove(ifp);
			rti_delete(ifp);
#ifdef MROUTING
			vif_delete(ifp);
#endif
#ifdef INET6
			in6_ifdetach(ifp);
#endif
			in_ifdetach(ifp);
			splx(s);
		}

		/* Let devices like enc(4) or mpe(4) know about the change */
		if ((error = (*ifp->if_ioctl)(ifp, cmd, data)) != ENOTTY)
			return (error);
		error = 0;

		/* Add interface to the specified rdomain */
		ifp->if_rdomain = ifr->ifr_rdomainid;
		break;

	case SIOCAIFGROUP:
		if ((error = suser(p, 0)))
			return (error);
		ifgr = (struct ifgroupreq *)data;
		if ((error = if_addgroup(ifp, ifgr->ifgr_group)))
			return (error);
		(*ifp->if_ioctl)(ifp, cmd, data); /* XXX error check */
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
		sdl = (struct sockaddr_dl *)ifp->if_sadl;
		if (sdl == NULL)
			return (EINVAL);
		if (ifr->ifr_addr.sa_len != ETHER_ADDR_LEN)
			return (EINVAL);
		if (ETHER_IS_MULTICAST(ifr->ifr_addr.sa_data))
			return (EINVAL);
		switch (ifp->if_type) {
		case IFT_ETHER:
		case IFT_CARP:
		case IFT_XETHER:
		case IFT_ISO88025:
			bcopy((caddr_t)ifr->ifr_addr.sa_data,
			    (caddr_t)((struct arpcom *)ifp)->ac_enaddr,
			    ETHER_ADDR_LEN);
			bcopy((caddr_t)ifr->ifr_addr.sa_data,
			    LLADDR(sdl), ETHER_ADDR_LEN);
			error = (*ifp->if_ioctl)(ifp, cmd, data);
			if (error == ENOTTY)
				error = 0;
			break;
		default:
			return (ENODEV);
		}

		ifnewlladdr(ifp);
		break;

	default:
		if (so->so_proto == 0)
			return (EOPNOTSUPP);
		error = ((*so->so_proto->pr_usrreq)(so, PRU_CONTROL,
			(struct mbuf *) cmd, (struct mbuf *) data,
			(struct mbuf *) ifp, p));
		break;
	}

	if (((oif_flags ^ ifp->if_flags) & IFF_UP) != 0)
		microtime(&ifp->if_lastchange);

	/* If we took down the IF, bring it back */
	if (up) {
		s = splnet();
		if_up(ifp);
		splx(s);
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
#ifdef COMPAT_LINUX
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
	TAILQ_FOREACH(ifp, &ifnet, if_list) {
		if (space < sizeof(ifr))
			break;
		bcopy(ifp->if_xname, ifr.ifr_name, IFNAMSIZ);
		if (TAILQ_EMPTY(&ifp->if_addrlist)) {
			bzero((caddr_t)&ifr.ifr_addr, sizeof(ifr.ifr_addr));
			error = copyout((caddr_t)&ifr, (caddr_t)ifrp,
			    sizeof(ifr));
			if (error)
				break;
			space -= sizeof (ifr), ifrp++;
		} else
			TAILQ_FOREACH(ifa, &ifp->if_addrlist, ifa_list) {
				struct sockaddr *sa = ifa->ifa_addr;

				if (space < sizeof(ifr))
					break;
#ifdef COMPAT_LINUX
				if (cmd == OSIOCGIFCONF) {
					ifr.ifr_addr = *sa;
					*(u_int16_t *)&ifr.ifr_addr =
					    sa->sa_family;
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
	IFQ_PURGE(&ifp->if_snd);
}

int
if_detached_ioctl(struct ifnet *ifp, u_long a, caddr_t b)
{
	return ENODEV;
}

/*
 * Create interface group without members
 */
struct ifg_group *
if_creategroup(const char *groupname)
{
	struct ifg_group	*ifg;

	if ((ifg = malloc(sizeof(*ifg), M_TEMP, M_NOWAIT)) == NULL)
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

	if ((ifgl = malloc(sizeof(*ifgl), M_TEMP, M_NOWAIT)) == NULL)
		return (ENOMEM);

	if ((ifgm = malloc(sizeof(*ifgm), M_TEMP, M_NOWAIT)) == NULL) {
		free(ifgl, M_TEMP, sizeof(*ifgl));
		return (ENOMEM);
	}

	TAILQ_FOREACH(ifg, &ifg_head, ifg_next)
		if (!strcmp(ifg->ifg_group, groupname))
			break;

	if (ifg == NULL && (ifg = if_creategroup(groupname)) == NULL) {
		free(ifgl, M_TEMP, sizeof(*ifgl));
		free(ifgm, M_TEMP, sizeof(*ifgm));
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
		free(ifgm, M_TEMP, sizeof(*ifgm));
	}

	if (--ifgl->ifgl_group->ifg_refcnt == 0) {
		TAILQ_REMOVE(&ifg_head, ifgl->ifgl_group, ifg_next);
#if NPF > 0
		pfi_detach_ifgroup(ifgl->ifgl_group);
#endif
		free(ifgl->ifgl_group, M_TEMP, 0);
	}

	free(ifgl, M_TEMP, sizeof(*ifgl));

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
		return (EINVAL);

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
		if (satosin(dst)->sin_addr.s_addr == INADDR_ANY &&
		    mask && (mask->sa_len == 0 ||
		    satosin(mask)->sin_addr.s_addr == INADDR_ANY))
			if_group_egress_build();
		break;
#ifdef INET6
	case AF_INET6:
		if (IN6_ARE_ADDR_EQUAL(&(satosin6(dst))->sin6_addr,
		    &in6addr_any) && mask && (mask->sa_len == 0 ||
		    IN6_ARE_ADDR_EQUAL(&(satosin6(mask))->sin6_addr,
		    &in6addr_any)))
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
	struct rtentry		*rt;

	TAILQ_FOREACH(ifg, &ifg_head, ifg_next)
		if (!strcmp(ifg->ifg_group, IFG_EGRESS))
			break;

	if (ifg != NULL)
		TAILQ_FOREACH_SAFE(ifgm, &ifg->ifg_members, ifgm_next, next)
			if_delgroup(ifgm->ifgm_ifp, IFG_EGRESS);

	bzero(&sa_in, sizeof(sa_in));
	sa_in.sin_len = sizeof(sa_in);
	sa_in.sin_family = AF_INET;
	if ((rt = rt_lookup(sintosa(&sa_in), sintosa(&sa_in), 0)) != NULL) {
		do {
			if (rt->rt_ifp)
				if_addgroup(rt->rt_ifp, IFG_EGRESS);
#ifndef SMALL_KERNEL
			rt = rt_mpath_next(rt);
#else
			rt = NULL;
#endif
		} while (rt != NULL);
	}

#ifdef INET6
	bcopy(&sa6_any, &sa_in6, sizeof(sa_in6));
	if ((rt = rt_lookup(sin6tosa(&sa_in6), sin6tosa(&sa_in6), 0)) != NULL) {
		do {
			if (rt->rt_ifp)
				if_addgroup(rt->rt_ifp, IFG_EGRESS);
#ifndef SMALL_KERNEL
			rt = rt_mpath_next(rt);
#else
			rt = NULL;
#endif
		} while (rt != NULL);
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

int
sysctl_niq(int *name, u_int namelen, void *oldp, size_t *oldlenp,
    void *newp, size_t newlen, struct niqueue *niq)
{
	/* All sysctl names at this level are terminal. */
	if (namelen != 1)
		return (ENOTDIR);

	switch (name[0]) {
	case IFQCTL_LEN:
		return (sysctl_rdint(oldp, oldlenp, newp, niq_len(niq)));
	case IFQCTL_MAXLEN:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &niq->ni_q.mq_maxlen)); /* XXX */
	case IFQCTL_DROPS:
		return (sysctl_rdint(oldp, oldlenp, newp, niq_drops(niq)));
	default:
		return (EOPNOTSUPP);
	}
	/* NOTREACHED */
}

void
ifa_add(struct ifnet *ifp, struct ifaddr *ifa)
{
	TAILQ_INSERT_TAIL(&ifp->if_addrlist, ifa, ifa_list);
}

void
ifa_del(struct ifnet *ifp, struct ifaddr *ifa)
{
	TAILQ_REMOVE(&ifp->if_addrlist, ifa, ifa_list);
}

void
ifa_update_broadaddr(struct ifnet *ifp, struct ifaddr *ifa, struct sockaddr *sa)
{
	if (ifa->ifa_broadaddr->sa_len != sa->sa_len)
		panic("ifa_update_broadaddr does not support dynamic length");
	bcopy(sa, ifa->ifa_broadaddr, sa->sa_len);
}

#ifdef DDB
/* debug function, can be called from ddb> */
void
ifa_print_all(void)
{
	struct ifnet *ifp;
	struct ifaddr *ifa;

	TAILQ_FOREACH(ifp, &ifnet, if_list) {
		TAILQ_FOREACH(ifa, &ifp->if_addrlist, ifa_list) {
			char addr[INET6_ADDRSTRLEN];

			switch (ifa->ifa_addr->sa_family) {
			case AF_INET:
				printf("%s", inet_ntop(AF_INET,
				    &satosin(ifa->ifa_addr)->sin_addr,
				    addr, sizeof(addr)));
				break;
#ifdef INET6
			case AF_INET6:
				printf("%s", inet_ntop(AF_INET6,
				    &(satosin6(ifa->ifa_addr))->sin6_addr,
				    addr, sizeof(addr)));
				break;
#endif
			case AF_LINK:
				printf("%s",
				    ether_sprintf(ifa->ifa_addr->sa_data));
				break;
			}
			printf(" on %s\n", ifa->ifa_ifp->if_xname);
		}
	}
}
#endif /* SMALL_KERNEL */

void
ifnewlladdr(struct ifnet *ifp)
{
	struct ifaddr *ifa;
	struct ifreq ifrq;
	short up;
	int s;

	s = splnet();
	up = ifp->if_flags & IFF_UP;

	if (up) {
		/* go down for a moment... */
		ifp->if_flags &= ~IFF_UP;
		ifrq.ifr_flags = ifp->if_flags;
		(*ifp->if_ioctl)(ifp, SIOCSIFFLAGS, (caddr_t)&ifrq);
	}

	ifp->if_flags |= IFF_UP;
	ifrq.ifr_flags = ifp->if_flags;
	(*ifp->if_ioctl)(ifp, SIOCSIFFLAGS, (caddr_t)&ifrq);

	TAILQ_FOREACH(ifa, &ifp->if_addrlist, ifa_list) {
		if (ifa->ifa_addr != NULL &&
		    ifa->ifa_addr->sa_family == AF_INET)
			arp_ifinit((struct arpcom *)ifp, ifa);
	}
#ifdef INET6
	/*
	 * Update the link-local address.  Don't do it if we're
	 * a router to avoid confusing hosts on the network.
	 */
	if (!ip6_forwarding) {
		ifa = &in6ifa_ifpforlinklocal(ifp, 0)->ia_ifa;
		if (ifa) {
			in6_purgeaddr(ifa);
			dohooks(ifp->if_addrhooks, 0);
			in6_ifattach(ifp);
		}
	}
#endif
	if (!up) {
		/* go back down */
		ifp->if_flags &= ~IFF_UP;
		ifrq.ifr_flags = ifp->if_flags;
		(*ifp->if_ioctl)(ifp, SIOCSIFFLAGS, (caddr_t)&ifrq);
	}
	splx(s);
}

int net_ticks;
u_int net_livelocks;

void
net_tick(void *null)
{
	extern int ticks;

	if (ticks - net_ticks > 1)
		net_livelocks++;

	net_ticks = ticks;

	timeout_add(&net_tick_to, 1);
}

int
net_livelocked()
{
	extern int ticks;

	return (ticks - net_ticks > 1);
}

void
if_rxr_init(struct if_rxring *rxr, u_int lwm, u_int hwm)
{
	extern int ticks;

	memset(rxr, 0, sizeof(*rxr));

	rxr->rxr_adjusted = ticks;
	rxr->rxr_cwm = rxr->rxr_lwm = lwm;
	rxr->rxr_hwm = hwm;
}

static inline void
if_rxr_adjust_cwm(struct if_rxring *rxr)
{
	extern int ticks;

	if (net_livelocked()) {
		if (rxr->rxr_cwm > rxr->rxr_lwm)
			rxr->rxr_cwm--;
		else
			return;
	} else if (rxr->rxr_alive >= rxr->rxr_lwm)
		return;
	else if (rxr->rxr_cwm < rxr->rxr_hwm)
		rxr->rxr_cwm++;

	rxr->rxr_adjusted = ticks;
}

u_int
if_rxr_get(struct if_rxring *rxr, u_int max)
{
	extern int ticks;
	u_int diff;

	if (ticks - rxr->rxr_adjusted >= 1) {
		/* we're free to try for an adjustment */
		if_rxr_adjust_cwm(rxr);
	}

	if (rxr->rxr_alive >= rxr->rxr_cwm)
		return (0);

	diff = min(rxr->rxr_cwm - rxr->rxr_alive, max);
	rxr->rxr_alive += diff;

	return (diff);
}

int
if_rxr_info_ioctl(struct if_rxrinfo *uifri, u_int t, struct if_rxring_info *e)
{
	struct if_rxrinfo kifri;
	int error;
	u_int n;

	error = copyin(uifri, &kifri, sizeof(kifri));
	if (error)
		return (error);

	n = min(t, kifri.ifri_total);
	kifri.ifri_total = t;

	if (n > 0) {
		error = copyout(e, kifri.ifri_entries, sizeof(*e) * n);
		if (error)
			return (error);
	}

	return (copyout(&kifri, uifri, sizeof(kifri)));
}

int
if_rxr_ioctl(struct if_rxrinfo *ifri, const char *name, u_int size,
    struct if_rxring *rxr)
{
	struct if_rxring_info ifr;

	memset(&ifr, 0, sizeof(ifr));

	if (name != NULL)
		strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));

	ifr.ifr_size = size;
	ifr.ifr_info = *rxr;

	return (if_rxr_info_ioctl(ifri, 1, &ifr));
}

/*
 * Network stack input queues.
 */

void
niq_init(struct niqueue *niq, u_int maxlen, u_int isr)
{
	mq_init(&niq->ni_q, maxlen, IPL_NET);
	niq->ni_isr = isr;
}

int
niq_enqueue(struct niqueue *niq, struct mbuf *m)
{
	int rv;

	rv = mq_enqueue(&niq->ni_q, m);
	if (rv == 0)
		schednetisr(niq->ni_isr);
	else
		if_congestion();

	return (rv);
}

int
niq_enlist(struct niqueue *niq, struct mbuf_list *ml)
{
	int rv;

	rv = mq_enlist(&niq->ni_q, ml);
	if (rv == 0)
		schednetisr(niq->ni_isr);
	else
		if_congestion();

	return (rv);
}
