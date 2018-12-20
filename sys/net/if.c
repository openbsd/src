/*	$OpenBSD: if.c,v 1.570 2018/12/20 10:26:36 claudio Exp $	*/
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
#include "ether.h"
#include "pf.h"
#include "pfsync.h"
#include "ppp.h"
#include "pppoe.h"
#include "switch.h"
#include "trunk.h"

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
#include <sys/task.h>
#include <sys/atomic.h>
#include <sys/percpu.h>
#include <sys/proc.h>

#include <dev/rndvar.h>

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

#include <sys/device.h>

void	if_attachsetup(struct ifnet *);
void	if_attachdomain(struct ifnet *);
void	if_attach_common(struct ifnet *);
int	if_createrdomain(int, struct ifnet *);
int	if_setrdomain(struct ifnet *, int);
void	if_slowtimo(void *);

void	if_detached_qstart(struct ifqueue *);
int	if_detached_ioctl(struct ifnet *, u_long, caddr_t);

int	ifioctl_get(u_long, caddr_t);
int	ifconf(caddr_t);

int	if_getgroup(caddr_t, struct ifnet *);
int	if_getgroupmembers(caddr_t);
int	if_getgroupattribs(caddr_t);
int	if_setgroupattribs(caddr_t);
int	if_getgrouplist(caddr_t);

void	if_linkstate(struct ifnet *);
void	if_linkstate_task(void *);

int	if_clone_list(struct if_clonereq *);
struct if_clone	*if_clone_lookup(const char *, int *);

int	if_group_egress_build(void);

void	if_watchdog_task(void *);

void	if_netisr(void *);

#ifdef DDB
void	ifa_print_all(void);
#endif

void	if_qstart_compat(struct ifqueue *);

/*
 * interface index map
 *
 * the kernel maintains a mapping of interface indexes to struct ifnet
 * pointers.
 *
 * the map is an array of struct ifnet pointers prefixed by an if_map
 * structure. the if_map structure stores the length of its array.
 *
 * as interfaces are attached to the system, the map is grown on demand
 * up to USHRT_MAX entries.
 *
 * interface index 0 is reserved and represents no interface. this
 * supports the use of the interface index as the scope for IPv6 link
 * local addresses, where scope 0 means no scope has been specified.
 * it also supports the use of interface index as the unique identifier
 * for network interfaces in SNMP applications as per RFC2863. therefore
 * if_get(0) returns NULL.
 */

void if_ifp_dtor(void *, void *);
void if_map_dtor(void *, void *);
struct ifnet *if_ref(struct ifnet *);

/*
 * struct if_map
 *
 * bounded array of ifnet srp pointers used to fetch references of live
 * interfaces with if_get().
 */

struct if_map {
	unsigned long		 limit;
	/* followed by limit ifnet srp pointers */
};

/*
 * struct if_idxmap
 *
 * infrastructure to manage updates and accesses to the current if_map.
 */

struct if_idxmap {
	unsigned int		 serial;
	unsigned int		 count;
	struct srp		 map;
};

void	if_idxmap_init(unsigned int);
void	if_idxmap_insert(struct ifnet *);
void	if_idxmap_remove(struct ifnet *);

TAILQ_HEAD(, ifg_group) ifg_head = TAILQ_HEAD_INITIALIZER(ifg_head);

LIST_HEAD(, if_clone) if_cloners = LIST_HEAD_INITIALIZER(if_cloners);
int if_cloners_count;

struct timeout net_tick_to;
void	net_tick(void *);
int	net_livelocked(void);
int	ifq_congestion;

int		 netisr;

#define	NET_TASKQ	1
struct taskq	*nettqmp[NET_TASKQ];

struct task if_input_task_locked = TASK_INITIALIZER(if_netisr, NULL);

/*
 * Serialize socket operations to ensure no new sleeping points
 * are introduced in IP output paths.
 */
struct rwlock netlock = RWLOCK_INITIALIZER("netlock");

/*
 * Network interface utility routines.
 */
void
ifinit(void)
{
	unsigned int	i;

	/*
	 * most machines boot with 4 or 5 interfaces, so size the initial map
	 * to accomodate this
	 */
	if_idxmap_init(8);

	timeout_set(&net_tick_to, net_tick, &net_tick_to);

	for (i = 0; i < NET_TASKQ; i++) {
		nettqmp[i] = taskq_create("softnet", 1, IPL_NET, TASKQ_MPSAFE);
		if (nettqmp[i] == NULL)
			panic("unable to create network taskq %d", i);
	}

	net_tick(&net_tick_to);
}

static struct if_idxmap if_idxmap = {
	0,
	0,
	SRP_INITIALIZER()
};

struct srp_gc if_ifp_gc = SRP_GC_INITIALIZER(if_ifp_dtor, NULL);
struct srp_gc if_map_gc = SRP_GC_INITIALIZER(if_map_dtor, NULL);

struct ifnet_head ifnet = TAILQ_HEAD_INITIALIZER(ifnet);

void
if_idxmap_init(unsigned int limit)
{
	struct if_map *if_map;
	struct srp *map;
	unsigned int i;

	if_idxmap.serial = 1; /* skip ifidx 0 so it can return NULL */

	if_map = malloc(sizeof(*if_map) + limit * sizeof(*map),
	    M_IFADDR, M_WAITOK);

	if_map->limit = limit;
	map = (struct srp *)(if_map + 1);
	for (i = 0; i < limit; i++)
		srp_init(&map[i]);

	/* this is called early so there's nothing to race with */
	srp_update_locked(&if_map_gc, &if_idxmap.map, if_map);
}

void
if_idxmap_insert(struct ifnet *ifp)
{
	struct if_map *if_map;
	struct srp *map;
	unsigned int index, i;

	refcnt_init(&ifp->if_refcnt);

	/* the kernel lock guarantees serialised modifications to if_idxmap */
	KERNEL_ASSERT_LOCKED();

	if (++if_idxmap.count > USHRT_MAX)
		panic("too many interfaces");

	if_map = srp_get_locked(&if_idxmap.map);
	map = (struct srp *)(if_map + 1);

	index = if_idxmap.serial++ & USHRT_MAX;

	if (index >= if_map->limit) {
		struct if_map *nif_map;
		struct srp *nmap;
		unsigned int nlimit;
		struct ifnet *nifp;

		nlimit = if_map->limit * 2;
		nif_map = malloc(sizeof(*nif_map) + nlimit * sizeof(*nmap),
		    M_IFADDR, M_WAITOK);
		nmap = (struct srp *)(nif_map + 1);

		nif_map->limit = nlimit;
		for (i = 0; i < if_map->limit; i++) {
			srp_init(&nmap[i]);
			nifp = srp_get_locked(&map[i]);
			if (nifp != NULL) {
				srp_update_locked(&if_ifp_gc, &nmap[i],
				    if_ref(nifp));
			}
		}

		while (i < nlimit) {
			srp_init(&nmap[i]);
			i++;
		}

		srp_update_locked(&if_map_gc, &if_idxmap.map, nif_map);
		if_map = nif_map;
		map = nmap;
	}

	/* pick the next free index */
	for (i = 0; i < USHRT_MAX; i++) {
		if (index != 0 && srp_get_locked(&map[index]) == NULL)
			break;

		index = if_idxmap.serial++ & USHRT_MAX;
	}

	/* commit */
	ifp->if_index = index;
	srp_update_locked(&if_ifp_gc, &map[index], if_ref(ifp));
}

void
if_idxmap_remove(struct ifnet *ifp)
{
	struct if_map *if_map;
	struct srp *map;
	unsigned int index;

	index = ifp->if_index;

	/* the kernel lock guarantees serialised modifications to if_idxmap */
	KERNEL_ASSERT_LOCKED();

	if_map = srp_get_locked(&if_idxmap.map);
	KASSERT(index < if_map->limit);

	map = (struct srp *)(if_map + 1);
	KASSERT(ifp == (struct ifnet *)srp_get_locked(&map[index]));

	srp_update_locked(&if_ifp_gc, &map[index], NULL);
	if_idxmap.count--;
	/* end of if_idxmap modifications */

	/* sleep until the last reference is released */
	refcnt_finalize(&ifp->if_refcnt, "ifidxrm");
}

void
if_ifp_dtor(void *null, void *ifp)
{
	if_put(ifp);
}

void
if_map_dtor(void *null, void *m)
{
	struct if_map *if_map = m;
	struct srp *map = (struct srp *)(if_map + 1);
	unsigned int i;

	/*
	 * dont need to serialize the use of update_locked since this is
	 * the last reference to this map. there's nothing to race against.
	 */
	for (i = 0; i < if_map->limit; i++)
		srp_update_locked(&if_ifp_gc, &map[i], NULL);

	free(if_map, M_IFADDR, sizeof(*if_map) + if_map->limit * sizeof(*map));
}

/*
 * Attach an interface to the
 * list of "active" interfaces.
 */
void
if_attachsetup(struct ifnet *ifp)
{
	unsigned long ifidx;

	NET_ASSERT_LOCKED();

	TAILQ_INIT(&ifp->if_groups);

	if_addgroup(ifp, IFG_ALL);

	if_attachdomain(ifp);
#if NPF > 0
	pfi_attach_ifnet(ifp);
#endif

	timeout_set(&ifp->if_slowtimo, if_slowtimo, ifp);
	if_slowtimo(ifp);

	if_idxmap_insert(ifp);
	KASSERT(if_get(0) == NULL);

	ifidx = ifp->if_index;

	task_set(&ifp->if_watchdogtask, if_watchdog_task, (void *)ifidx);
	task_set(&ifp->if_linkstatetask, if_linkstate_task, (void *)ifidx);

	/* Announce the interface. */
	rtm_ifannounce(ifp, IFAN_ARRIVAL);
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
if_attachdomain(struct ifnet *ifp)
{
	struct domain *dp;
	int i, s;

	s = splnet();

	/* address family dependent data region */
	bzero(ifp->if_afdata, sizeof(ifp->if_afdata));
	for (i = 0; (dp = domains[i]) != NULL; i++) {
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
	NET_LOCK();
	TAILQ_INSERT_HEAD(&ifnet, ifp, if_list);
	if_attachsetup(ifp);
	NET_UNLOCK();
}

void
if_attach(struct ifnet *ifp)
{
	if_attach_common(ifp);
	NET_LOCK();
	TAILQ_INSERT_TAIL(&ifnet, ifp, if_list);
	if_attachsetup(ifp);
	NET_UNLOCK();
}

void
if_attach_queues(struct ifnet *ifp, unsigned int nqs)
{
	struct ifqueue **map;
	struct ifqueue *ifq;
	int i;

	KASSERT(ifp->if_ifqs == ifp->if_snd.ifq_ifqs);
	KASSERT(nqs != 0);

	map = mallocarray(sizeof(*map), nqs, M_DEVBUF, M_WAITOK);

	ifp->if_snd.ifq_softc = NULL;
	map[0] = &ifp->if_snd;

	for (i = 1; i < nqs; i++) {
		ifq = malloc(sizeof(*ifq), M_DEVBUF, M_WAITOK|M_ZERO);
		ifq_set_maxlen(ifq, ifp->if_snd.ifq_maxlen);
		ifq_init(ifq, ifp, i);
		map[i] = ifq;
	}

	ifp->if_ifqs = map;
	ifp->if_nifqs = nqs;
}

void
if_attach_iqueues(struct ifnet *ifp, unsigned int niqs)
{
	struct ifiqueue **map;
	struct ifiqueue *ifiq;
	unsigned int i;

	KASSERT(niqs != 0);

	map = mallocarray(niqs, sizeof(*map), M_DEVBUF, M_WAITOK);

	ifp->if_rcv.ifiq_softc = NULL;
	map[0] = &ifp->if_rcv;

	for (i = 1; i < niqs; i++) {
		ifiq = malloc(sizeof(*ifiq), M_DEVBUF, M_WAITOK|M_ZERO);
		ifiq_init(ifiq, ifp, i);
		map[i] = ifiq;
	}

	ifp->if_iqs = map;
	ifp->if_niqs = niqs;
}

void
if_attach_common(struct ifnet *ifp)
{
	KASSERT(ifp->if_ioctl != NULL);

	TAILQ_INIT(&ifp->if_addrlist);
	TAILQ_INIT(&ifp->if_maddrlist);

	if (!ISSET(ifp->if_xflags, IFXF_MPSAFE)) {
		KASSERTMSG(ifp->if_qstart == NULL,
		    "%s: if_qstart set without MPSAFE set", ifp->if_xname);
		ifp->if_qstart = if_qstart_compat;
	} else {
		KASSERTMSG(ifp->if_start == NULL,
		    "%s: if_start set with MPSAFE set", ifp->if_xname);
		KASSERTMSG(ifp->if_qstart != NULL,
		    "%s: if_qstart not set with MPSAFE set", ifp->if_xname);
	}

	ifq_init(&ifp->if_snd, ifp, 0);

	ifp->if_snd.ifq_ifqs[0] = &ifp->if_snd;
	ifp->if_ifqs = ifp->if_snd.ifq_ifqs;
	ifp->if_nifqs = 1;

	ifiq_init(&ifp->if_rcv, ifp, 0);

	ifp->if_rcv.ifiq_ifiqs[0] = &ifp->if_rcv;
	ifp->if_iqs = ifp->if_rcv.ifiq_ifiqs;
	ifp->if_niqs = 1;

	ifp->if_addrhooks = malloc(sizeof(*ifp->if_addrhooks),
	    M_TEMP, M_WAITOK);
	TAILQ_INIT(ifp->if_addrhooks);
	ifp->if_linkstatehooks = malloc(sizeof(*ifp->if_linkstatehooks),
	    M_TEMP, M_WAITOK);
	TAILQ_INIT(ifp->if_linkstatehooks);
	ifp->if_detachhooks = malloc(sizeof(*ifp->if_detachhooks),
	    M_TEMP, M_WAITOK);
	TAILQ_INIT(ifp->if_detachhooks);

	if (ifp->if_rtrequest == NULL)
		ifp->if_rtrequest = if_rtrequest_dummy;
	ifp->if_llprio = IFQ_DEFPRIO;

	SRPL_INIT(&ifp->if_inputs);
}

void
if_attach_ifq(struct ifnet *ifp, const struct ifq_ops *newops, void *args)
{
	/*
	 * only switch the ifq_ops on the first ifq on an interface.
	 *
	 * the only ifq_ops we provide priq and hfsc, and hfsc only
	 * works on a single ifq. because the code uses the ifq_ops
	 * on the first ifq (if_snd) to select a queue for an mbuf,
	 * by switching only the first one we change both the algorithm
	 * and force the routing of all new packets to it.
	 */
	ifq_attach(&ifp->if_snd, newops, args);
}

void
if_start(struct ifnet *ifp)
{
	KASSERT(ifp->if_qstart == if_qstart_compat);
	if_qstart_compat(&ifp->if_snd);
}
void
if_qstart_compat(struct ifqueue *ifq)
{
	struct ifnet *ifp = ifq->ifq_if;
	int s;

	/*
	 * the stack assumes that an interface can have multiple
	 * transmit rings, but a lot of drivers are still written
	 * so that interfaces and send rings have a 1:1 mapping.
	 * this provides compatability between the stack and the older
	 * drivers by translating from the only queue they have
	 * (ifp->if_snd) back to the interface and calling if_start.
 	 */

	KERNEL_LOCK();
	s = splnet();
	(*ifp->if_start)(ifp);
	splx(s);
	KERNEL_UNLOCK();
}

int
if_enqueue(struct ifnet *ifp, struct mbuf *m)
{
	unsigned int idx;
	struct ifqueue *ifq;
	int error;

#if NPF > 0
	if (m->m_pkthdr.pf.delay > 0)
		return (pf_delay_pkt(m, ifp->if_index));
#endif

#if NBRIDGE > 0
	if (ifp->if_bridgeport && (m->m_flags & M_PROTO1) == 0) {
		KERNEL_LOCK();
		error = bridge_output(ifp, m, NULL, NULL);
		KERNEL_UNLOCK();
		return (error);
	}
#endif

#if NPF > 0
	pf_pkt_addr_changed(m);
#endif	/* NPF > 0 */

	/*
	 * use the operations on the first ifq to pick which of the array
	 * gets this mbuf.
	 */
	idx = ifq_idx(&ifp->if_snd, ifp->if_nifqs, m);
	ifq = ifp->if_ifqs[idx];

	error = ifq_enqueue(ifq, m);
	if (error)
		return (error);

	ifq_start(ifq);

	return (0);
}

void
if_input(struct ifnet *ifp, struct mbuf_list *ml)
{
	ifiq_input(&ifp->if_rcv, ml, 2048);
}

int
if_input_local(struct ifnet *ifp, struct mbuf *m, sa_family_t af)
{
#if NBPFILTER > 0
	/*
	 * Only send packets to bpf if they are destinated to local
	 * addresses.
	 *
	 * if_input_local() is also called for SIMPLEX interfaces to
	 * duplicate packets for local use.  But don't dup them to bpf.
	 */
	if (ifp->if_flags & IFF_LOOPBACK) {
		caddr_t if_bpf = ifp->if_bpf;

		if (if_bpf)
			bpf_mtap_af(if_bpf, af, m, BPF_DIRECTION_OUT);
	}
#endif
	m_resethdr(m);
	m->m_flags |= M_LOOP;
	m->m_pkthdr.ph_ifidx = ifp->if_index;
	m->m_pkthdr.ph_rtableid = ifp->if_rdomain;

	ifp->if_opackets++;
	ifp->if_obytes += m->m_pkthdr.len;

	ifp->if_ipackets++;
	ifp->if_ibytes += m->m_pkthdr.len;

	switch (af) {
	case AF_INET:
		ipv4_input(ifp, m);
		break;
#ifdef INET6
	case AF_INET6:
		ipv6_input(ifp, m);
		break;
#endif /* INET6 */
#ifdef MPLS
	case AF_MPLS:
		mpls_input(ifp, m);
		break;
#endif /* MPLS */
	default:
		printf("%s: can't handle af%d\n", ifp->if_xname, af);
		m_freem(m);
		return (EAFNOSUPPORT);
	}

	return (0);
}

int
if_output_local(struct ifnet *ifp, struct mbuf *m, sa_family_t af)
{
	struct ifiqueue *ifiq;
	unsigned int flow = 0;

	m->m_pkthdr.ph_family = af;
	m->m_pkthdr.ph_ifidx = ifp->if_index;
	m->m_pkthdr.ph_rtableid = ifp->if_rdomain;

	if (ISSET(m->m_pkthdr.ph_flowid, M_FLOWID_VALID))
		flow = m->m_pkthdr.ph_flowid & M_FLOWID_MASK;

	ifiq = ifp->if_iqs[flow % ifp->if_niqs];

	return (ifiq_enqueue(ifiq, m) == 0 ? 0 : ENOBUFS);
}

struct ifih {
	SRPL_ENTRY(ifih)	  ifih_next;
	int			(*ifih_input)(struct ifnet *, struct mbuf *,
				      void *);
	void			 *ifih_cookie;
	int			  ifih_refcnt;
	struct refcnt		  ifih_srpcnt;
};

void	if_ih_ref(void *, void *);
void	if_ih_unref(void *, void *);

struct srpl_rc ifih_rc = SRPL_RC_INITIALIZER(if_ih_ref, if_ih_unref, NULL);

void
if_ih_insert(struct ifnet *ifp, int (*input)(struct ifnet *, struct mbuf *,
    void *), void *cookie)
{
	struct ifih *ifih;

	/* the kernel lock guarantees serialised modifications to if_inputs */
	KERNEL_ASSERT_LOCKED();

	SRPL_FOREACH_LOCKED(ifih, &ifp->if_inputs, ifih_next) {
		if (ifih->ifih_input == input && ifih->ifih_cookie == cookie) {
			ifih->ifih_refcnt++;
			break;
		}
	}

	if (ifih == NULL) {
		ifih = malloc(sizeof(*ifih), M_DEVBUF, M_WAITOK);

		ifih->ifih_input = input;
		ifih->ifih_cookie = cookie;
		ifih->ifih_refcnt = 1;
		refcnt_init(&ifih->ifih_srpcnt);
		SRPL_INSERT_HEAD_LOCKED(&ifih_rc, &ifp->if_inputs,
		    ifih, ifih_next);
	}
}

void
if_ih_ref(void *null, void *i)
{
	struct ifih *ifih = i;

	refcnt_take(&ifih->ifih_srpcnt);
}

void
if_ih_unref(void *null, void *i)
{
	struct ifih *ifih = i;

	refcnt_rele_wake(&ifih->ifih_srpcnt);
}

void
if_ih_remove(struct ifnet *ifp, int (*input)(struct ifnet *, struct mbuf *,
    void *), void *cookie)
{
	struct ifih *ifih;

	/* the kernel lock guarantees serialised modifications to if_inputs */
	KERNEL_ASSERT_LOCKED();

	SRPL_FOREACH_LOCKED(ifih, &ifp->if_inputs, ifih_next) {
		if (ifih->ifih_input == input && ifih->ifih_cookie == cookie)
			break;
	}

	KASSERT(ifih != NULL);

	if (--ifih->ifih_refcnt == 0) {
		SRPL_REMOVE_LOCKED(&ifih_rc, &ifp->if_inputs, ifih,
		    ifih, ifih_next);

		refcnt_finalize(&ifih->ifih_srpcnt, "ifihrm");
		free(ifih, M_DEVBUF, sizeof(*ifih));
	}
}

void
if_input_process(struct ifnet *ifp, struct mbuf_list *ml)
{
	struct mbuf *m;
	struct ifih *ifih;
	struct srp_ref sr;
	int s;

	if (ml_empty(ml))
		return;

	if (!ISSET(ifp->if_xflags, IFXF_CLONED))
		enqueue_randomness(ml_len(ml));

	/*
	 * We grab the NET_LOCK() before processing any packet to
	 * ensure there's no contention on the routing table lock.
	 *
	 * Without it we could race with a userland thread to insert
	 * a L2 entry in ip{6,}_output().  Such race would result in
	 * one of the threads sleeping *inside* the IP output path.
	 *
	 * Since we have a NET_LOCK() we also use it to serialize access
	 * to PF globals, pipex globals, unicast and multicast addresses
	 * lists.
	 */
	NET_RLOCK();
	s = splnet();
	while ((m = ml_dequeue(ml)) != NULL) {
		/*
		 * Pass this mbuf to all input handlers of its
		 * interface until it is consumed.
		 */
		SRPL_FOREACH(ifih, &sr, &ifp->if_inputs, ifih_next) {
			if ((*ifih->ifih_input)(ifp, m, ifih->ifih_cookie))
				break;
		}
		SRPL_LEAVE(&sr);

		if (ifih == NULL)
			m_freem(m);
	}
	splx(s);
	NET_RUNLOCK();
}

void
if_netisr(void *unused)
{
	int n, t = 0;

	NET_LOCK();

	while ((n = netisr) != 0) {
		/* Like sched_pause() but with a rwlock dance. */
		if (curcpu()->ci_schedstate.spc_schedflags & SPCF_SHOULDYIELD) {
			NET_UNLOCK();
			yield();
			NET_LOCK();
		}

		atomic_clearbits_int(&netisr, n);

#if NETHER > 0
		if (n & (1 << NETISR_ARP)) {
			KERNEL_LOCK();
			arpintr();
			KERNEL_UNLOCK();
		}
#endif
		if (n & (1 << NETISR_IP))
			ipintr();
#ifdef INET6
		if (n & (1 << NETISR_IPV6))
			ip6intr();
#endif
#if NPPP > 0
		if (n & (1 << NETISR_PPP)) {
			KERNEL_LOCK();
			pppintr();
			KERNEL_UNLOCK();
		}
#endif
#if NBRIDGE > 0
		if (n & (1 << NETISR_BRIDGE))
			bridgeintr();
#endif
#if NSWITCH > 0
		if (n & (1 << NETISR_SWITCH)) {
			KERNEL_LOCK();
			switchintr();
			KERNEL_UNLOCK();
		}
#endif
#if NPPPOE > 0
		if (n & (1 << NETISR_PPPOE)) {
			KERNEL_LOCK();
			pppoeintr();
			KERNEL_UNLOCK();
		}
#endif
#ifdef PIPEX
		if (n & (1 << NETISR_PIPEX)) {
			KERNEL_LOCK();
			pipexintr();
			KERNEL_UNLOCK();
		}
#endif
		t |= n;
	}

#if NPFSYNC > 0
	if (t & (1 << NETISR_PFSYNC)) {
		KERNEL_LOCK();
		pfsyncintr();
		KERNEL_UNLOCK();
	}
#endif

	NET_UNLOCK();
}

void
if_deactivate(struct ifnet *ifp)
{
	NET_LOCK();
	/*
	 * Call detach hooks from head to tail.  To make sure detach
	 * hooks are executed in the reverse order they were added, all
	 * the hooks have to be added to the head!
	 */
	dohooks(ifp->if_detachhooks, HOOK_REMOVE | HOOK_FREE);

	NET_UNLOCK();
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
	struct domain *dp;
	int i, s;

	/* Undo pseudo-driver changes. */
	if_deactivate(ifp);

	ifq_clr_oactive(&ifp->if_snd);

	/* Other CPUs must not have a reference before we start destroying. */
	if_idxmap_remove(ifp);

#if NBPFILTER > 0
	bpfdetach(ifp);
#endif

	NET_LOCK();
	s = splnet();
	ifp->if_qstart = if_detached_qstart;
	ifp->if_ioctl = if_detached_ioctl;
	ifp->if_watchdog = NULL;

	/* Remove the watchdog timeout & task */
	timeout_del(&ifp->if_slowtimo);
	task_del(net_tq(ifp->if_index), &ifp->if_watchdogtask);

	/* Remove the link state task */
	task_del(net_tq(ifp->if_index), &ifp->if_linkstatetask);

	rti_delete(ifp);
#if NETHER > 0 && defined(NFSCLIENT)
	if (ifp->if_index == revarp_ifidx)
		revarp_ifidx = 0;
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

	/* Remove the interface from the list of all interfaces.  */
	TAILQ_REMOVE(&ifnet, ifp, if_list);

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

	for (i = 0; (dp = domains[i]) != NULL; i++) {
		if (dp->dom_ifdetach && ifp->if_afdata[dp->dom_family])
			(*dp->dom_ifdetach)(ifp,
			    ifp->if_afdata[dp->dom_family]);
	}

	/* Announce that the interface is gone. */
	rtm_ifannounce(ifp, IFAN_DEPARTURE);
	splx(s);
	NET_UNLOCK();

	if (ifp->if_counters != NULL)
		if_counters_free(ifp);

	for (i = 0; i < ifp->if_nifqs; i++)
		ifq_destroy(ifp->if_ifqs[i]);
	if (ifp->if_ifqs != ifp->if_snd.ifq_ifqs) {
		for (i = 1; i < ifp->if_nifqs; i++) {
			free(ifp->if_ifqs[i], M_DEVBUF,
			    sizeof(struct ifqueue));
		}
		free(ifp->if_ifqs, M_DEVBUF,
		    sizeof(struct ifqueue *) * ifp->if_nifqs);
	}

	for (i = 0; i < ifp->if_niqs; i++)
		ifiq_destroy(ifp->if_iqs[i]);
	if (ifp->if_iqs != ifp->if_rcv.ifiq_ifiqs) {
		for (i = 1; i < ifp->if_niqs; i++) {
			free(ifp->if_iqs[i], M_DEVBUF,
			    sizeof(struct ifiqueue));
		}
		free(ifp->if_iqs, M_DEVBUF,
		    sizeof(struct ifiqueue *) * ifp->if_niqs);
	}
}

/*
 * Returns true if ``ifp0'' is connected to the interface with index ``ifidx''.
 */
int
if_isconnected(const struct ifnet *ifp0, unsigned int ifidx)
{
	struct ifnet *ifp;
	int connected = 0;

	ifp = if_get(ifidx);
	if (ifp == NULL)
		return (0);

	if (ifp0->if_index == ifp->if_index)
		connected = 1;

#if NBRIDGE > 0
	if (SAME_BRIDGE(ifp0->if_bridgeport, ifp->if_bridgeport))
		connected = 1;
#endif
#if NCARP > 0
	if ((ifp0->if_type == IFT_CARP && ifp0->if_carpdev == ifp) ||
	    (ifp->if_type == IFT_CARP && ifp->if_carpdev == ifp0))
		connected = 1;
#endif

	if_put(ifp);
	return (connected);
}

/*
 * Create a clone network interface.
 */
int
if_clone_create(const char *name, int rdomain)
{
	struct if_clone *ifc;
	struct ifnet *ifp;
	int unit, ret;

	NET_ASSERT_LOCKED();

	ifc = if_clone_lookup(name, &unit);
	if (ifc == NULL)
		return (EINVAL);

	if (ifunit(name) != NULL)
		return (EEXIST);

	/* XXXSMP breaks atomicity */
	NET_UNLOCK();
	ret = (*ifc->ifc_create)(ifc, unit);
	NET_LOCK();

	if (ret != 0 || (ifp = ifunit(name)) == NULL)
		return (ret);

	if_addgroup(ifp, ifc->ifc_name);
	if (rdomain != 0)
		if_setrdomain(ifp, rdomain);

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
	int ret;

	NET_ASSERT_LOCKED();

	ifc = if_clone_lookup(name, NULL);
	if (ifc == NULL)
		return (EINVAL);

	ifp = ifunit(name);
	if (ifp == NULL)
		return (ENXIO);

	if (ifc->ifc_destroy == NULL)
		return (EOPNOTSUPP);

	if (ifp->if_flags & IFF_UP) {
		int s;
		s = splnet();
		if_down(ifp);
		splx(s);
	}

	/* XXXSMP breaks atomicity */
	NET_UNLOCK();
	ret = (*ifc->ifc_destroy)(ifp);
	NET_LOCK();

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
	/*
	 * we are called at kernel boot by main(), when pseudo devices are
	 * being attached. The main() is the only guy which may alter the
	 * if_cloners. While system is running and main() is done with
	 * initialization, the if_cloners becomes immutable. 
	 */
	KASSERT(pdevinit_done == 0);
	LIST_INSERT_HEAD(&if_cloners, ifc, ifc_list);
	if_cloners_count++;
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

	if ((dst = ifcr->ifcr_buffer) == NULL) {
		/* Just asking how many there are. */
		ifcr->ifcr_total = if_cloners_count;
		return (0);
	}

	if (ifcr->ifcr_count < 0)
		return (EINVAL);

	ifcr->ifcr_total = if_cloners_count;
	count = MIN(if_cloners_count, ifcr->ifcr_count);

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
	int diff;

	diff = ticks - ifq_congestion;
	if (diff < 0) {
		ifq_congestion = ticks - hz;
		return (0);
	}

	return (diff <= (hz / 100));
}

#define	equal(a1, a2)	\
	(bcmp((caddr_t)(a1), (caddr_t)(a2),	\
	(a1)->sa_len) == 0)

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
	KERNEL_LOCK();
	TAILQ_FOREACH(ifp, &ifnet, if_list) {
		if (ifp->if_rdomain != rdomain)
			continue;

		TAILQ_FOREACH(ifa, &ifp->if_addrlist, ifa_list) {
			if (ifa->ifa_addr->sa_family != addr->sa_family)
				continue;

			if (equal(addr, ifa->ifa_addr)) {
				KERNEL_UNLOCK();
				return (ifa);
			}
		}
	}
	KERNEL_UNLOCK();
	return (NULL);
}

/*
 * Locate the point to point interface with a given destination address.
 */
struct ifaddr *
ifa_ifwithdstaddr(struct sockaddr *addr, u_int rdomain)
{
	struct ifnet *ifp;
	struct ifaddr *ifa;

	rdomain = rtable_l2(rdomain);
	KERNEL_LOCK();
	TAILQ_FOREACH(ifp, &ifnet, if_list) {
		if (ifp->if_rdomain != rdomain)
			continue;
		if (ifp->if_flags & IFF_POINTOPOINT) {
			TAILQ_FOREACH(ifa, &ifp->if_addrlist, ifa_list) {
				if (ifa->ifa_addr->sa_family !=
				    addr->sa_family || ifa->ifa_dstaddr == NULL)
					continue;
				if (equal(addr, ifa->ifa_dstaddr)) {
					KERNEL_UNLOCK();
					return (ifa);
				}
			}
		}
	}
	KERNEL_UNLOCK();
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

void
if_rtrequest_dummy(struct ifnet *ifp, int req, struct rtentry *rt)
{
}

/*
 * Default action when installing a local route on a point-to-point
 * interface.
 */
void
p2p_rtrequest(struct ifnet *ifp, int req, struct rtentry *rt)
{
	struct ifnet *lo0ifp;
	struct ifaddr *ifa, *lo0ifa;

	switch (req) {
	case RTM_ADD:
		if (!ISSET(rt->rt_flags, RTF_LOCAL))
			break;

		TAILQ_FOREACH(ifa, &ifp->if_addrlist, ifa_list) {
			if (memcmp(rt_key(rt), ifa->ifa_addr,
			    rt_key(rt)->sa_len) == 0)
				break;
		}

		if (ifa == NULL)
			break;

		KASSERT(ifa == rt->rt_ifa);

		lo0ifp = if_get(rtable_loindex(ifp->if_rdomain));
		KASSERT(lo0ifp != NULL);
		TAILQ_FOREACH(lo0ifa, &lo0ifp->if_addrlist, ifa_list) {
			if (lo0ifa->ifa_addr->sa_family ==
			    ifa->ifa_addr->sa_family)
				break;
		}
		if_put(lo0ifp);

		if (lo0ifa == NULL)
			break;

		rt->rt_flags &= ~RTF_LLINFO;
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

	NET_LOCK();
	TAILQ_FOREACH(ifp, &ifnet, if_list) {
		if ((ifp->if_flags & IFF_UP) == 0)
			continue;
		if_down(ifp);
		ifrq.ifr_flags = ifp->if_flags;
		(*ifp->if_ioctl)(ifp, SIOCSIFFLAGS, (caddr_t)&ifrq);
	}
	NET_UNLOCK();
}

/*
 * Mark an interface down and notify protocols of
 * the transition.
 */
void
if_down(struct ifnet *ifp)
{
	NET_ASSERT_LOCKED();

	ifp->if_flags &= ~IFF_UP;
	getmicrotime(&ifp->if_lastchange);
	IFQ_PURGE(&ifp->if_snd);

	if_linkstate(ifp);
}

/*
 * Mark an interface up and notify protocols of
 * the transition.
 */
void
if_up(struct ifnet *ifp)
{
	NET_ASSERT_LOCKED();

	ifp->if_flags |= IFF_UP;
	getmicrotime(&ifp->if_lastchange);

#ifdef INET6
	/* Userland expects the kernel to set ::1 on default lo(4). */
	if (ifp->if_index == rtable_loindex(ifp->if_rdomain))
		in6_ifattach(ifp);
#endif

	if_linkstate(ifp);
}

/*
 * Notify userland, the routing table and hooks owner of
 * a link-state transition.
 */
void
if_linkstate_task(void *xifidx)
{
	unsigned int ifidx = (unsigned long)xifidx;
	struct ifnet *ifp;

	KERNEL_LOCK();
	NET_LOCK();

	ifp = if_get(ifidx);
	if (ifp != NULL)
		if_linkstate(ifp);
	if_put(ifp);

	NET_UNLOCK();
	KERNEL_UNLOCK();
}

void
if_linkstate(struct ifnet *ifp)
{
	NET_ASSERT_LOCKED();

	rtm_ifchg(ifp);
	rt_if_track(ifp);
	dohooks(ifp->if_linkstatehooks, 0);
}

/*
 * Schedule a link state change task.
 */
void
if_link_state_change(struct ifnet *ifp)
{
	task_add(net_tq(ifp->if_index), &ifp->if_linkstatetask);
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
			task_add(net_tq(ifp->if_index), &ifp->if_watchdogtask);
		timeout_add_sec(&ifp->if_slowtimo, IFNET_SLOWTIMO);
	}
	splx(s);
}

void
if_watchdog_task(void *xifidx)
{
	unsigned int ifidx = (unsigned long)xifidx;
	struct ifnet *ifp;
	int s;

	ifp = if_get(ifidx);
	if (ifp == NULL)
		return;

	KERNEL_LOCK();
	s = splnet();
	if (ifp->if_watchdog)
		(*ifp->if_watchdog)(ifp);
	splx(s);
	KERNEL_UNLOCK();

	if_put(ifp);
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
	struct srp_ref sr;
	struct if_map *if_map;
	struct srp *map;
	struct ifnet *ifp = NULL;

	if_map = srp_enter(&sr, &if_idxmap.map);
	if (index < if_map->limit) {
		map = (struct srp *)(if_map + 1);

		ifp = srp_follow(&sr, &map[index]);
		if (ifp != NULL) {
			KASSERT(ifp->if_index == index);
			if_ref(ifp);
		}
	}
	srp_leave(&sr);

	return (ifp);
}

struct ifnet *
if_ref(struct ifnet *ifp)
{
	refcnt_take(&ifp->if_refcnt);

	return (ifp);
}

void
if_put(struct ifnet *ifp)
{
	if (ifp == NULL)
		return;

	refcnt_rele_wake(&ifp->if_refcnt);
}

int
if_setlladdr(struct ifnet *ifp, const uint8_t *lladdr)
{
	if (ifp->if_sadl == NULL)
		return (EINVAL);

	memcpy(((struct arpcom *)ifp)->ac_enaddr, lladdr, ETHER_ADDR_LEN);
	memcpy(LLADDR(ifp->if_sadl), lladdr, ETHER_ADDR_LEN);

	return (0);
}

int
if_createrdomain(int rdomain, struct ifnet *ifp)
{
	int error;
	struct ifnet *loifp;
	char loifname[IFNAMSIZ];
	unsigned int unit = rdomain;

	if (!rtable_exists(rdomain) && (error = rtable_add(rdomain)) != 0)
		return (error);
	if (!rtable_empty(rdomain))
		return (EEXIST);

	/* Create rdomain including its loopback if with unit == rdomain */
	snprintf(loifname, sizeof(loifname), "lo%u", unit);
	error = if_clone_create(loifname, 0);
	if ((loifp = ifunit(loifname)) == NULL)
		return (ENXIO);
	if (error && (ifp != loifp || error != EEXIST))
		return (error);

	rtable_l2set(rdomain, rdomain, loifp->if_index);
	loifp->if_rdomain = rdomain;

	return (0);
}

int
if_setrdomain(struct ifnet *ifp, int rdomain)
{
	struct ifreq ifr;
	int error, up = 0, s;

	if (rdomain < 0 || rdomain > RT_TABLEID_MAX)
		return (EINVAL);

	if (rdomain != ifp->if_rdomain &&
	    (ifp->if_flags & IFF_LOOPBACK) &&
	    (ifp->if_index == rtable_loindex(ifp->if_rdomain)))
		return (EPERM);

	if (!rtable_exists(rdomain))
		return (ESRCH);

	/* make sure that the routing table is a real rdomain */
	if (rdomain != rtable_l2(rdomain))
		return (EINVAL);

	if (rdomain != ifp->if_rdomain) {
		s = splnet();
		/*
		 * We are tearing down the world.
		 * Take down the IF so:
		 * 1. everything that cares gets a message
		 * 2. the automagic IPv6 bits are recreated
		 */
		if (ifp->if_flags & IFF_UP) {
			up = 1;
			if_down(ifp);
		}
		rti_delete(ifp);
#ifdef MROUTING
		vif_delete(ifp);
#endif
		in_ifdetach(ifp);
#ifdef INET6
		in6_ifdetach(ifp);
#endif
		splx(s);
	}

	/* Let devices like enc(4) or mpe(4) know about the change */
	ifr.ifr_rdomainid = rdomain;
	if ((error = (*ifp->if_ioctl)(ifp, SIOCSIFRDOMAIN,
	    (caddr_t)&ifr)) != ENOTTY)
		return (error);
	error = 0;

	/* Add interface to the specified rdomain */
	ifp->if_rdomain = rdomain;

	/* If we took down the IF, bring it back */
	if (up) {
		s = splnet();
		if_up(ifp);
		splx(s);
	}

	return (0);
}

/*
 * Interface ioctls.
 */
int
ifioctl(struct socket *so, u_long cmd, caddr_t data, struct proc *p)
{
	struct ifnet *ifp;
	struct ifreq *ifr = (struct ifreq *)data;
	struct ifgroupreq *ifgr = (struct ifgroupreq *)data;
	struct if_afreq *ifar = (struct if_afreq *)data;
	char ifdescrbuf[IFDESCRSIZE];
	char ifrtlabelbuf[RTLABEL_LEN];
	int s, error = 0, oif_xflags;
	size_t bytesdone;
	unsigned short oif_flags;

	switch (cmd) {
	case SIOCIFCREATE:
		if ((error = suser(p)) != 0)
			return (error);
		NET_LOCK();
		error = if_clone_create(ifr->ifr_name, 0);
		NET_UNLOCK();
		return (error);
	case SIOCIFDESTROY:
		if ((error = suser(p)) != 0)
			return (error);
		NET_LOCK();
		error = if_clone_destroy(ifr->ifr_name);
		NET_UNLOCK();
		return (error);
	case SIOCSIFGATTR:
		if ((error = suser(p)) != 0)
			return (error);
		NET_LOCK();
		error = if_setgroupattribs(data);
		NET_UNLOCK();
		return (error);
	case SIOCGIFCONF:
	case SIOCIFGCLONERS:
	case SIOCGIFGMEMB:
	case SIOCGIFGATTR:
	case SIOCGIFGLIST:
	case SIOCGIFFLAGS:
	case SIOCGIFXFLAGS:
	case SIOCGIFMETRIC:
	case SIOCGIFMTU:
	case SIOCGIFHARDMTU:
	case SIOCGIFDATA:
	case SIOCGIFDESCR:
	case SIOCGIFRTLABEL:
	case SIOCGIFPRIORITY:
	case SIOCGIFRDOMAIN:
	case SIOCGIFGROUP:
	case SIOCGIFLLPRIO:
		return (ifioctl_get(cmd, data));
	}

	ifp = ifunit(ifr->ifr_name);
	if (ifp == NULL)
		return (ENXIO);
	oif_flags = ifp->if_flags;
	oif_xflags = ifp->if_xflags;

	switch (cmd) {
	case SIOCIFAFATTACH:
	case SIOCIFAFDETACH:
		if ((error = suser(p)) != 0)
			break;
		NET_LOCK();
		switch (ifar->ifar_af) {
		case AF_INET:
			/* attach is a noop for AF_INET */
			if (cmd == SIOCIFAFDETACH)
				in_ifdetach(ifp);
			break;
#ifdef INET6
		case AF_INET6:
			if (cmd == SIOCIFAFATTACH)
				error = in6_ifattach(ifp);
			else
				in6_ifdetach(ifp);
			break;
#endif /* INET6 */
		default:
			error = EAFNOSUPPORT;
		}
		NET_UNLOCK();
		break;

	case SIOCSIFFLAGS:
		if ((error = suser(p)) != 0)
			break;

		NET_LOCK();
		ifp->if_flags = (ifp->if_flags & IFF_CANTCHANGE) |
			(ifr->ifr_flags & ~IFF_CANTCHANGE);

		error = (*ifp->if_ioctl)(ifp, cmd, data);
		if (error != 0) {
			ifp->if_flags = oif_flags;
		} else if (ISSET(oif_flags ^ ifp->if_flags, IFF_UP)) {
			s = splnet();
			if (ISSET(ifp->if_flags, IFF_UP))
				if_up(ifp);
			else
				if_down(ifp);
			splx(s);
		}
		NET_UNLOCK();
		break;

	case SIOCSIFXFLAGS:
		if ((error = suser(p)) != 0)
			break;

		NET_LOCK();
#ifdef INET6
		if (ISSET(ifr->ifr_flags, IFXF_AUTOCONF6)) {
			error = in6_ifattach(ifp);
			if (error != 0) {
				NET_UNLOCK();
				break;
			}
		}

		if (ISSET(ifr->ifr_flags, IFXF_INET6_NOSOII) &&
		    !ISSET(ifp->if_xflags, IFXF_INET6_NOSOII)) {
			ifp->if_xflags |= IFXF_INET6_NOSOII;
			in6_soiiupdate(ifp);
		}

		if (!ISSET(ifr->ifr_flags, IFXF_INET6_NOSOII) &&
		    ISSET(ifp->if_xflags, IFXF_INET6_NOSOII)) {
			ifp->if_xflags &= ~IFXF_INET6_NOSOII;
			in6_soiiupdate(ifp);
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
			}
			if (ISSET(ifp->if_xflags, IFXF_WOL) &&
			    !ISSET(ifr->ifr_flags, IFXF_WOL)) {
				s = splnet();
				ifp->if_xflags &= ~IFXF_WOL;
				error = ifp->if_wol(ifp, 0);
				splx(s);
			}
		} else if (ISSET(ifr->ifr_flags, IFXF_WOL)) {
			ifr->ifr_flags &= ~IFXF_WOL;
			error = ENOTSUP;
		}
#endif

		if (error == 0)
			ifp->if_xflags = (ifp->if_xflags & IFXF_CANTCHANGE) |
				(ifr->ifr_flags & ~IFXF_CANTCHANGE);
		NET_UNLOCK();
		break;

	case SIOCSIFMETRIC:
		if ((error = suser(p)) != 0)
			break;
		NET_LOCK();
		ifp->if_metric = ifr->ifr_metric;
		NET_UNLOCK();
		break;

	case SIOCSIFMTU:
		if ((error = suser(p)) != 0)
			break;
		NET_LOCK();
		error = (*ifp->if_ioctl)(ifp, cmd, data);
		NET_UNLOCK();
		if (!error)
			rtm_ifchg(ifp);
		break;

	case SIOCSIFDESCR:
		if ((error = suser(p)) != 0)
			break;
		error = copyinstr(ifr->ifr_data, ifdescrbuf,
		    IFDESCRSIZE, &bytesdone);
		if (error == 0) {
			(void)memset(ifp->if_description, 0, IFDESCRSIZE);
			strlcpy(ifp->if_description, ifdescrbuf, IFDESCRSIZE);
		}
		break;

	case SIOCSIFRTLABEL:
		if ((error = suser(p)) != 0)
			break;
		error = copyinstr(ifr->ifr_data, ifrtlabelbuf,
		    RTLABEL_LEN, &bytesdone);
		if (error == 0) {
			rtlabel_unref(ifp->if_rtlabelid);
			ifp->if_rtlabelid = rtlabel_name2id(ifrtlabelbuf);
		}
		break;

	case SIOCSIFPRIORITY:
		if ((error = suser(p)) != 0)
			break;
		if (ifr->ifr_metric < 0 || ifr->ifr_metric > 15) {
			error = EINVAL;
			break;
		}
		ifp->if_priority = ifr->ifr_metric;
		break;

	case SIOCSIFRDOMAIN:
		if ((error = suser(p)) != 0)
			break;
		NET_LOCK();
		error = if_createrdomain(ifr->ifr_rdomainid, ifp);
		if (!error || error == EEXIST)
			error = if_setrdomain(ifp, ifr->ifr_rdomainid);
		NET_UNLOCK();
		break;

	case SIOCAIFGROUP:
		if ((error = suser(p)))
			break;
		NET_LOCK();
		error = if_addgroup(ifp, ifgr->ifgr_group);
		if (error == 0) {
			error = (*ifp->if_ioctl)(ifp, cmd, data);
			if (error == ENOTTY)
				error = 0;
		}
		NET_UNLOCK();
		break;

	case SIOCDIFGROUP:
		if ((error = suser(p)))
			break;
		NET_LOCK();
		error = (*ifp->if_ioctl)(ifp, cmd, data);
		if (error == ENOTTY)
			error = 0;
		if (error == 0)
			error = if_delgroup(ifp, ifgr->ifgr_group);
		NET_UNLOCK();
		break;

	case SIOCSIFLLADDR:
		if ((error = suser(p)))
			break;
		if ((ifp->if_sadl == NULL) ||
		    (ifr->ifr_addr.sa_len != ETHER_ADDR_LEN) ||
		    (ETHER_IS_MULTICAST(ifr->ifr_addr.sa_data))) {
			error = EINVAL;
			break;
		}
		NET_LOCK();
		switch (ifp->if_type) {
		case IFT_ETHER:
		case IFT_CARP:
		case IFT_XETHER:
		case IFT_ISO88025:
			error = (*ifp->if_ioctl)(ifp, cmd, data);
			if (error == ENOTTY)
				error = 0;
			if (error == 0)
				error = if_setlladdr(ifp,
				    ifr->ifr_addr.sa_data);
			break;
		default:
			error = ENODEV;
		}

		if (error == 0)
			ifnewlladdr(ifp);
		NET_UNLOCK();
		break;

	case SIOCSIFLLPRIO:
		if ((error = suser(p)))
			break;
		if (ifr->ifr_llprio < IFQ_MINPRIO ||
		    ifr->ifr_llprio > IFQ_MAXPRIO) {
			error = EINVAL;
			break;
		}
		NET_LOCK();
		ifp->if_llprio = ifr->ifr_llprio;
		NET_UNLOCK();
		break;

	case SIOCSETKALIVE:
	case SIOCDIFPHYADDR:
	case SIOCSLIFPHYADDR:
	case SIOCSLIFPHYRTABLE:
	case SIOCSLIFPHYTTL:
	case SIOCSLIFPHYDF:
	case SIOCSLIFPHYECN:
	case SIOCADDMULTI:
	case SIOCDELMULTI:
	case SIOCSIFMEDIA:
	case SIOCSVNETID:
	case SIOCSVNETFLOWID:
	case SIOCSTXHPRIO:
	case SIOCSIFPAIR:
	case SIOCSIFPARENT:
	case SIOCDIFPARENT:
		if ((error = suser(p)) != 0)
			break;
		/* FALLTHROUGH */
	default:
		error = ((*so->so_proto->pr_usrreq)(so, PRU_CONTROL,
			(struct mbuf *) cmd, (struct mbuf *) data,
			(struct mbuf *) ifp, p));
		if (error == EOPNOTSUPP) {
			NET_LOCK();
			error = ((*ifp->if_ioctl)(ifp, cmd, data));
			NET_UNLOCK();
		}
		break;
	}

	if (oif_flags != ifp->if_flags || oif_xflags != ifp->if_xflags)
		rtm_ifchg(ifp);

	if (((oif_flags ^ ifp->if_flags) & IFF_UP) != 0)
		getmicrotime(&ifp->if_lastchange);

	return (error);
}

int
ifioctl_get(u_long cmd, caddr_t data)
{
	struct ifnet *ifp;
	struct ifreq *ifr = (struct ifreq *)data;
	char ifdescrbuf[IFDESCRSIZE];
	char ifrtlabelbuf[RTLABEL_LEN];
	int error = 0;
	size_t bytesdone;
	const char *label;

	switch(cmd) {
	case SIOCGIFCONF:
		NET_RLOCK();
		error = ifconf(data);
		NET_RUNLOCK();
		return (error);
	case SIOCIFGCLONERS:
		error = if_clone_list((struct if_clonereq *)data);
		return (error);
	case SIOCGIFGMEMB:
		NET_RLOCK();
		error = if_getgroupmembers(data);
		NET_RUNLOCK();
		return (error);
	case SIOCGIFGATTR:
		NET_RLOCK();
		error = if_getgroupattribs(data);
		NET_RUNLOCK();
		return (error);
	case SIOCGIFGLIST:
		NET_RLOCK();
		error = if_getgrouplist(data);
		NET_RUNLOCK();
		return (error);
	}

	ifp = ifunit(ifr->ifr_name);
	if (ifp == NULL)
		return (ENXIO);

	NET_RLOCK();

	switch(cmd) {
	case SIOCGIFFLAGS:
		ifr->ifr_flags = ifp->if_flags;
		if (ifq_is_oactive(&ifp->if_snd))
			ifr->ifr_flags |= IFF_OACTIVE;
		break;

	case SIOCGIFXFLAGS:
		ifr->ifr_flags = ifp->if_xflags & ~(IFXF_MPSAFE|IFXF_CLONED);
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

	case SIOCGIFDATA: {
		struct if_data ifdata;
		if_getdata(ifp, &ifdata);
		error = copyout(&ifdata, ifr->ifr_data, sizeof(ifdata));
		break;
	}

	case SIOCGIFDESCR:
		strlcpy(ifdescrbuf, ifp->if_description, IFDESCRSIZE);
		error = copyoutstr(ifdescrbuf, ifr->ifr_data, IFDESCRSIZE,
		    &bytesdone);
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

	case SIOCGIFPRIORITY:
		ifr->ifr_metric = ifp->if_priority;
		break;

	case SIOCGIFRDOMAIN:
		ifr->ifr_rdomainid = ifp->if_rdomain;
		break;

	case SIOCGIFGROUP:
		error = if_getgroup(data, ifp);
		break;

	case SIOCGIFLLPRIO:
		ifr->ifr_llprio = ifp->if_llprio;
		break;

	default:
		panic("invalid ioctl %lu", cmd);
	}

	NET_RUNLOCK();

	return (error);
}

/*
 * Return interface configuration
 * of system.  List may be used
 * in later ioctl's (above) to get
 * other information.
 */
int
ifconf(caddr_t data)
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

void
if_counters_alloc(struct ifnet *ifp)
{
	KASSERT(ifp->if_counters == NULL);

	ifp->if_counters = counters_alloc(ifc_ncounters);
}

void
if_counters_free(struct ifnet *ifp)
{
	KASSERT(ifp->if_counters != NULL);

	counters_free(ifp->if_counters, ifc_ncounters);
	ifp->if_counters = NULL;
}

void
if_getdata(struct ifnet *ifp, struct if_data *data)
{
	unsigned int i;

	*data = ifp->if_data;

	if (ifp->if_counters != NULL) {
		uint64_t counters[ifc_ncounters];

		counters_read(ifp->if_counters, counters, nitems(counters));

		data->ifi_ipackets += counters[ifc_ipackets];
		data->ifi_ierrors += counters[ifc_ierrors];
		data->ifi_opackets += counters[ifc_opackets];
		data->ifi_oerrors += counters[ifc_oerrors];
		data->ifi_collisions += counters[ifc_collisions];
		data->ifi_ibytes += counters[ifc_ibytes];
		data->ifi_obytes += counters[ifc_obytes];
		data->ifi_imcasts += counters[ifc_imcasts];
		data->ifi_omcasts += counters[ifc_omcasts];
		data->ifi_iqdrops += counters[ifc_iqdrops];
		data->ifi_oqdrops += counters[ifc_oqdrops];
		data->ifi_noproto += counters[ifc_noproto];
	}

	for (i = 0; i < ifp->if_nifqs; i++) {
		struct ifqueue *ifq = ifp->if_ifqs[i];

		ifq_add_data(ifq, data);
	}

	for (i = 0; i < ifp->if_niqs; i++) {
		struct ifiqueue *ifiq = ifp->if_iqs[i];

		ifiq_add_data(ifiq, data);
	}
}

/*
 * Dummy functions replaced in ifnet during detach (if protocols decide to
 * fiddle with the if during detach.
 */
void
if_detached_qstart(struct ifqueue *ifq)
{
	ifq_purge(ifq);
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
	pfi_group_addmember(groupname, ifp);
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
		ifgm->ifgm_ifp->if_ioctl(ifgm->ifgm_ifp, SIOCSIFGATTR, data);

	return (0);
}

/*
 * Stores all groups in memory pointed to by data
 */
int
if_getgrouplist(caddr_t data)
{
	struct ifgroupreq	*ifgr = (struct ifgroupreq *)data;
	struct ifg_group	*ifg;
	struct ifg_req		 ifgrq, *ifgp;
	int			 len, error;

	if (ifgr->ifgr_len == 0) {
		TAILQ_FOREACH(ifg, &ifg_head, ifg_next)
			ifgr->ifgr_len += sizeof(ifgrq);
		return (0);
	}

	len = ifgr->ifgr_len;
	ifgp = ifgr->ifgr_groups;
	TAILQ_FOREACH(ifg, &ifg_head, ifg_next) {
		if (len < sizeof(ifgrq))
			return (EINVAL);
		bzero(&ifgrq, sizeof ifgrq);
		strlcpy(ifgrq.ifgrq_group, ifg->ifg_group,
                    sizeof(ifgrq.ifgrq_group));
		if ((error = copyout((caddr_t)&ifgrq, (caddr_t)ifgp,
                    sizeof(struct ifg_req))))
			return (error);
		len -= sizeof(ifgrq);
		ifgp++;
	}

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
	struct ifnet		*ifp;
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
	rt = rtable_lookup(0, sintosa(&sa_in), sintosa(&sa_in), NULL, RTP_ANY);
	while (rt != NULL) {
		ifp = if_get(rt->rt_ifidx);
		if (ifp != NULL) {
			if_addgroup(ifp, IFG_EGRESS);
			if_put(ifp);
		}
		rt = rtable_iterate(rt);
	}

#ifdef INET6
	bcopy(&sa6_any, &sa_in6, sizeof(sa_in6));
	rt = rtable_lookup(0, sin6tosa(&sa_in6), sin6tosa(&sa_in6), NULL,
	    RTP_ANY);
	while (rt != NULL) {
		ifp = if_get(rt->rt_ifidx);
		if (ifp != NULL) {
			if_addgroup(ifp, IFG_EGRESS);
			if_put(ifp);
		}
		rt = rtable_iterate(rt);
	}
#endif /* INET6 */

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
	unsigned short oif_flags;
	int oif_pcount, error;

	oif_flags = ifp->if_flags;
	oif_pcount = ifp->if_pcount;
	if (pswitch) {
		if (ifp->if_pcount++ != 0)
			return (0);
		ifp->if_flags |= IFF_PROMISC;
	} else {
		if (--ifp->if_pcount > 0)
			return (0);
		ifp->if_flags &= ~IFF_PROMISC;
	}

	if ((ifp->if_flags & IFF_UP) == 0)
		return (0);

	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_flags = ifp->if_flags;
	error = ((*ifp->if_ioctl)(ifp, SIOCSIFFLAGS, (caddr_t)&ifr));
	if (error) {
		ifp->if_flags = oif_flags;
		ifp->if_pcount = oif_pcount;
	}

	return (error);
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
			}
			printf(" on %s\n", ifp->if_xname);
		}
	}
}
#endif /* DDB */

void
ifnewlladdr(struct ifnet *ifp)
{
#ifdef INET6
	struct ifaddr *ifa;
#endif
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
net_livelocked(void)
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

void
if_rxr_livelocked(struct if_rxring *rxr)
{
	extern int ticks;

	if (ticks - rxr->rxr_adjusted >= 1) {
		if (rxr->rxr_cwm > rxr->rxr_lwm)
			rxr->rxr_cwm--;

		rxr->rxr_adjusted = ticks;
	}
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

__dead void
unhandled_af(int af)
{
	panic("unhandled af %d", af);
}

/*
 * XXXSMP This tunable is here to work around the fact that IPsec
 * globals aren't ready to be accessed by multiple threads in
 * parallel.
 */
int		 nettaskqs = NET_TASKQ;

struct taskq *
net_tq(unsigned int ifindex)
{
	struct taskq *t = NULL;

	t = nettqmp[ifindex % nettaskqs];

	return (t);
}
