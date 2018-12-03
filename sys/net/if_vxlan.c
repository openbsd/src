/*	$OpenBSD: if_vxlan.c,v 1.70 2018/12/03 17:25:22 claudio Exp $	*/

/*
 * Copyright (c) 2013 Reyk Floeter <reyk@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "bpfilter.h"
#include "vxlan.h"
#include "vlan.h"
#include "pf.h"
#include "bridge.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/ioctl.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_media.h>
#include <net/route.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/in_pcb.h>

#if NPF > 0
#include <net/pfvar.h>
#endif

#if NBRIDGE > 0
#include <net/if_bridge.h>
#endif

#include <net/if_vxlan.h>

struct vxlan_softc {
	struct arpcom		 sc_ac;
	struct ifmedia		 sc_media;

	struct ip_moptions	 sc_imo;
	void			*sc_ahcookie;
	void			*sc_lhcookie;
	void			*sc_dhcookie;

	struct sockaddr_storage	 sc_src;
	struct sockaddr_storage	 sc_dst;
	in_port_t		 sc_dstport;
	u_int			 sc_rdomain;
	int64_t			 sc_vnetid;
	uint16_t		 sc_df;
	u_int8_t		 sc_ttl;
	int			 sc_txhprio;

	struct task		 sc_sendtask;

	LIST_ENTRY(vxlan_softc)	 sc_entry;
};

void	 vxlanattach(int);
int	 vxlanioctl(struct ifnet *, u_long, caddr_t);
void	 vxlanstart(struct ifnet *);
int	 vxlan_clone_create(struct if_clone *, int);
int	 vxlan_clone_destroy(struct ifnet *);
void	 vxlan_multicast_cleanup(struct ifnet *);
int	 vxlan_multicast_join(struct ifnet *, struct sockaddr *,
	    struct sockaddr *);
int	 vxlan_media_change(struct ifnet *);
void	 vxlan_media_status(struct ifnet *, struct ifmediareq *);
int	 vxlan_config(struct ifnet *, struct sockaddr *, struct sockaddr *);
int	 vxlan_output(struct ifnet *, struct mbuf *);
void	 vxlan_addr_change(void *);
void	 vxlan_if_change(void *);
void	 vxlan_link_change(void *);
void	 vxlan_send_dispatch(void *);

int	 vxlan_sockaddr_cmp(struct sockaddr *, struct sockaddr *);
uint16_t vxlan_sockaddr_port(struct sockaddr *);

struct if_clone	vxlan_cloner =
    IF_CLONE_INITIALIZER("vxlan", vxlan_clone_create, vxlan_clone_destroy);

int	 vxlan_enable = 0;
u_long	 vxlan_tagmask;

#define VXLAN_TAGHASHSIZE		 32
#define VXLAN_TAGHASH(tag)		 ((unsigned int)tag & vxlan_tagmask)
LIST_HEAD(vxlan_taghash, vxlan_softc)	*vxlan_tagh, vxlan_any;

void
vxlanattach(int count)
{
	/* Regular vxlan interfaces with a VNI */
	if ((vxlan_tagh = hashinit(VXLAN_TAGHASHSIZE, M_DEVBUF, M_NOWAIT,
	    &vxlan_tagmask)) == NULL)
		panic("vxlanattach: hashinit");

	/* multipoint-to-multipoint interfaces that accept any VNI */
	LIST_INIT(&vxlan_any);

	if_clone_attach(&vxlan_cloner);
}

int
vxlan_clone_create(struct if_clone *ifc, int unit)
{
	struct ifnet		*ifp;
	struct vxlan_softc	*sc;

	sc = malloc(sizeof(*sc), M_DEVBUF, M_WAITOK|M_ZERO);
	sc->sc_imo.imo_membership = malloc(
	    (sizeof(struct in_multi *) * IP_MIN_MEMBERSHIPS), M_IPMOPTS,
	    M_WAITOK|M_ZERO);
	sc->sc_imo.imo_max_memberships = IP_MIN_MEMBERSHIPS;
	sc->sc_dstport = htons(VXLAN_PORT);
	sc->sc_vnetid = VXLAN_VNI_UNSET;
	sc->sc_txhprio = IFQ_TOS2PRIO(IPTOS_PREC_ROUTINE); /* 0 */
	sc->sc_df = htons(0);
	task_set(&sc->sc_sendtask, vxlan_send_dispatch, sc);

	ifp = &sc->sc_ac.ac_if;
	snprintf(ifp->if_xname, sizeof ifp->if_xname, "vxlan%d", unit);
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ether_fakeaddr(ifp);

	ifp->if_softc = sc;
	ifp->if_ioctl = vxlanioctl;
	ifp->if_start = vxlanstart;
	IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);

	ifp->if_hardmtu = ETHER_MAX_HARDMTU_LEN;
	ifp->if_capabilities = IFCAP_VLAN_MTU;

	ifmedia_init(&sc->sc_media, 0, vxlan_media_change,
	    vxlan_media_status);
	ifmedia_add(&sc->sc_media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(&sc->sc_media, IFM_ETHER | IFM_AUTO);

	if_attach(ifp);
	ether_ifattach(ifp);

#if 0
	/*
	 * Instead of using a decreased MTU of 1450 bytes, prefer
	 * to use the default Ethernet-size MTU of 1500 bytes and to
	 * increase the MTU of the outer transport interfaces to
	 * at least 1550 bytes. The following is disabled by default.
	 */
	ifp->if_mtu = ETHERMTU - sizeof(struct ether_header);
	ifp->if_mtu -= sizeof(struct vxlanudphdr) + sizeof(struct ipovly);
#endif

	LIST_INSERT_HEAD(&vxlan_tagh[VXLAN_TAGHASH(0)], sc, sc_entry);
	vxlan_enable++;

	return (0);
}

int
vxlan_clone_destroy(struct ifnet *ifp)
{
	struct vxlan_softc	*sc = ifp->if_softc;

	NET_LOCK();
	vxlan_multicast_cleanup(ifp);
	NET_UNLOCK();

	vxlan_enable--;
	LIST_REMOVE(sc, sc_entry);

	ifmedia_delete_instance(&sc->sc_media, IFM_INST_ANY);
	ether_ifdetach(ifp);
	if_detach(ifp);

	if (!task_del(net_tq(ifp->if_index), &sc->sc_sendtask))
		taskq_barrier(net_tq(ifp->if_index));

	free(sc->sc_imo.imo_membership, M_IPMOPTS, 0);
	free(sc, M_DEVBUF, sizeof(*sc));

	return (0);
}

void
vxlan_multicast_cleanup(struct ifnet *ifp)
{
	struct vxlan_softc	*sc = (struct vxlan_softc *)ifp->if_softc;
	struct ip_moptions	*imo = &sc->sc_imo;
	struct ifnet		*mifp;

	mifp = if_get(imo->imo_ifidx);
	if (mifp != NULL) {
		if (sc->sc_ahcookie != NULL) {
			hook_disestablish(mifp->if_addrhooks, sc->sc_ahcookie);
			sc->sc_ahcookie = NULL;
		}
		if (sc->sc_lhcookie != NULL) {
			hook_disestablish(mifp->if_linkstatehooks,
			    sc->sc_lhcookie);
			sc->sc_lhcookie = NULL;
		}
		if (sc->sc_dhcookie != NULL) {
			hook_disestablish(mifp->if_detachhooks,
			    sc->sc_dhcookie);
			sc->sc_dhcookie = NULL;
		}

		if_put(mifp);
	}

	if (imo->imo_num_memberships > 0) {
		in_delmulti(imo->imo_membership[--imo->imo_num_memberships]);
		imo->imo_ifidx = 0;
	}
}

int
vxlan_multicast_join(struct ifnet *ifp, struct sockaddr *src,
    struct sockaddr *dst)
{
	struct vxlan_softc	*sc = ifp->if_softc;
	struct ip_moptions	*imo = &sc->sc_imo;
	struct sockaddr_in	*src4, *dst4;
#ifdef INET6
	struct sockaddr_in6	*dst6;
#endif /* INET6 */
	struct ifaddr		*ifa;
	struct ifnet		*mifp;

	switch (dst->sa_family) {
	case AF_INET:
		dst4 = satosin(dst);
		if (!IN_MULTICAST(dst4->sin_addr.s_addr))
			return (0);
		break;
#ifdef INET6
	case AF_INET6:
		dst6 = satosin6(dst);
		if (!IN6_IS_ADDR_MULTICAST(&dst6->sin6_addr))
			return (0);

		/* Multicast mode is currently not supported for IPv6 */
		return (EAFNOSUPPORT);
#endif /* INET6 */
	default:
		return (EAFNOSUPPORT);
	}

	src4 = satosin(src);
	dst4 = satosin(dst);

	if (src4->sin_addr.s_addr == INADDR_ANY ||
	    IN_MULTICAST(src4->sin_addr.s_addr))
		return (EINVAL);
	if ((ifa = ifa_ifwithaddr(src, sc->sc_rdomain)) == NULL ||
	    (mifp = ifa->ifa_ifp) == NULL ||
	    (mifp->if_flags & IFF_MULTICAST) == 0)
		return (EADDRNOTAVAIL);

	if ((imo->imo_membership[0] =
	    in_addmulti(&dst4->sin_addr, mifp)) == NULL)
		return (ENOBUFS);

	imo->imo_num_memberships++;
	imo->imo_ifidx = mifp->if_index;
	if (sc->sc_ttl > 0)
		imo->imo_ttl = sc->sc_ttl;
	else
		imo->imo_ttl = IP_DEFAULT_MULTICAST_TTL;
	imo->imo_loop = 0;

	/*
	 * Use interface hooks to track any changes on the interface
	 * that is used to send out the tunnel traffic as multicast.
	 */
	if ((sc->sc_ahcookie = hook_establish(mifp->if_addrhooks,
	    0, vxlan_addr_change, sc)) == NULL ||
	    (sc->sc_lhcookie = hook_establish(mifp->if_linkstatehooks,
	    0, vxlan_link_change, sc)) == NULL ||
	    (sc->sc_dhcookie = hook_establish(mifp->if_detachhooks,
	    0, vxlan_if_change, sc)) == NULL)
		panic("%s: cannot allocate interface hook",
		    mifp->if_xname);

	return (0);
}

void
vxlanstart(struct ifnet *ifp)
{
	struct vxlan_softc	*sc = (struct vxlan_softc *)ifp->if_softc;

	task_add(net_tq(ifp->if_index), &sc->sc_sendtask);
}

void
vxlan_send_dispatch(void *xsc)
{
	struct vxlan_softc	*sc = xsc;
	struct ifnet		*ifp = &sc->sc_ac.ac_if;
	struct mbuf		*m;
	struct mbuf_list	 ml;

	ml_init(&ml);
	for (;;) {
		IFQ_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL)
			break;

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_OUT);
#endif

		ml_enqueue(&ml, m);
	}

	if (ml_empty(&ml))
		return;

	NET_RLOCK();
	while ((m = ml_dequeue(&ml)) != NULL) {
		vxlan_output(ifp, m);
	}
	NET_RUNLOCK();
}


int
vxlan_config(struct ifnet *ifp, struct sockaddr *src, struct sockaddr *dst)
{
	struct vxlan_softc	*sc = (struct vxlan_softc *)ifp->if_softc;
	int			 reset = 0, error, af;
	socklen_t		 slen;
	in_port_t		 port;
	struct vxlan_taghash	*tagh;

	if (src != NULL && dst != NULL) {
		if ((af = src->sa_family) != dst->sa_family)
			return (EAFNOSUPPORT);
	} else {
		/* Reset current configuration */
		af = sc->sc_src.ss_family;
		src = sstosa(&sc->sc_src);
		dst = sstosa(&sc->sc_dst);
		reset = 1;
	}

	switch (af) {
	case AF_INET:
		slen = sizeof(struct sockaddr_in);
		break;
#ifdef INET6
	case AF_INET6:
		slen = sizeof(struct sockaddr_in6);
		break;
#endif /* INET6 */
	default:
		return (EAFNOSUPPORT);
	}

	if (src->sa_len != slen || dst->sa_len != slen)
		return (EINVAL);

	vxlan_multicast_cleanup(ifp);

	/* returns without error if multicast is not configured */
	if ((error = vxlan_multicast_join(ifp, src, dst)) != 0)
		return (error);

	if ((port = vxlan_sockaddr_port(dst)) != 0)
		sc->sc_dstport = port;

	if (!reset) {
		bzero(&sc->sc_src, sizeof(sc->sc_src));
		bzero(&sc->sc_dst, sizeof(sc->sc_dst));
		memcpy(&sc->sc_src, src, src->sa_len);
		memcpy(&sc->sc_dst, dst, dst->sa_len);
	}

	if (sc->sc_vnetid == VXLAN_VNI_ANY) {
		/*
		 * If the interface accepts any VNI, put it into a separate
		 * list that is not part of the main hash.
		 */
		tagh = &vxlan_any;
	} else
		tagh = &vxlan_tagh[VXLAN_TAGHASH(sc->sc_vnetid)];

	LIST_REMOVE(sc, sc_entry);
	LIST_INSERT_HEAD(tagh, sc, sc_entry);

	return (0);
}

int
vxlanioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct vxlan_softc	*sc = (struct vxlan_softc *)ifp->if_softc;
	struct ifreq		*ifr = (struct ifreq *)data;
	struct if_laddrreq	*lifr = (struct if_laddrreq *)data;
	int			 error = 0;

	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		/* FALLTHROUGH */

	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			ifp->if_flags |= IFF_RUNNING;
		} else {
			ifp->if_flags &= ~IFF_RUNNING;
		}
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		break;

	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_media, cmd);
		break;

	case SIOCSLIFPHYADDR:
		error = vxlan_config(ifp,
		    sstosa(&lifr->addr),
		    sstosa(&lifr->dstaddr));
		break;

	case SIOCDIFPHYADDR:
		vxlan_multicast_cleanup(ifp);
		bzero(&sc->sc_src, sizeof(sc->sc_src));
		bzero(&sc->sc_dst, sizeof(sc->sc_dst));
		sc->sc_dstport = htons(VXLAN_PORT);
		break;

	case SIOCGLIFPHYADDR:
		if (sc->sc_dst.ss_family == AF_UNSPEC) {
			error = EADDRNOTAVAIL;
			break;
		}
		bzero(&lifr->addr, sizeof(lifr->addr));
		bzero(&lifr->dstaddr, sizeof(lifr->dstaddr));
		memcpy(&lifr->addr, &sc->sc_src, sc->sc_src.ss_len);
		memcpy(&lifr->dstaddr, &sc->sc_dst, sc->sc_dst.ss_len);
		break;

	case SIOCSLIFPHYRTABLE:
		if (ifr->ifr_rdomainid < 0 ||
		    ifr->ifr_rdomainid > RT_TABLEID_MAX ||
		    !rtable_exists(ifr->ifr_rdomainid)) {
			error = EINVAL;
			break;
		}
		sc->sc_rdomain = ifr->ifr_rdomainid;
		(void)vxlan_config(ifp, NULL, NULL);
		break;

	case SIOCGLIFPHYRTABLE:
		ifr->ifr_rdomainid = sc->sc_rdomain;
		break;

	case SIOCSLIFPHYTTL:
		if (ifr->ifr_ttl < 0 || ifr->ifr_ttl > 0xff) {
			error = EINVAL;
			break;
		}
		if (sc->sc_ttl == (u_int8_t)ifr->ifr_ttl)
			break;
		sc->sc_ttl = (u_int8_t)(ifr->ifr_ttl);
		(void)vxlan_config(ifp, NULL, NULL);
		break;

	case SIOCGLIFPHYTTL:
		ifr->ifr_ttl = (int)sc->sc_ttl;
		break;

	case SIOCSLIFPHYDF:
		/* commit */
		sc->sc_df = ifr->ifr_df ? htons(IP_DF) : htons(0);
		break;
	case SIOCGLIFPHYDF:
		ifr->ifr_df = sc->sc_df ? 1 : 0;
		break;

	case SIOCSTXHPRIO:
		if (ifr->ifr_hdrprio == IF_HDRPRIO_PACKET)
			; /* fall through */
		else if (ifr->ifr_hdrprio < IF_HDRPRIO_MIN ||
		    ifr->ifr_hdrprio > IF_HDRPRIO_MAX) {
			error = EINVAL;
			break;
		}

		sc->sc_txhprio = ifr->ifr_hdrprio;
		break;
	case SIOCGTXHPRIO:
		ifr->ifr_hdrprio = sc->sc_txhprio;
		break;

	case SIOCSVNETID:
		if (sc->sc_vnetid == ifr->ifr_vnetid)
			break;

		if ((ifr->ifr_vnetid != VXLAN_VNI_ANY) &&
		    (ifr->ifr_vnetid > VXLAN_VNI_MAX ||
		     ifr->ifr_vnetid < VXLAN_VNI_MIN)) {
			error = EINVAL;
			break;
		}

		sc->sc_vnetid = (int)ifr->ifr_vnetid;
		(void)vxlan_config(ifp, NULL, NULL);
		break;

	case SIOCGVNETID:
		if ((sc->sc_vnetid != VXLAN_VNI_ANY) &&
		    (sc->sc_vnetid > VXLAN_VNI_MAX ||
		     sc->sc_vnetid < VXLAN_VNI_MIN)) {
			error = EADDRNOTAVAIL;
			break;
		}

		ifr->ifr_vnetid = sc->sc_vnetid;
		break;

	case SIOCDVNETID:
		sc->sc_vnetid = VXLAN_VNI_UNSET;
		(void)vxlan_config(ifp, NULL, NULL);
		break;

	default:
		error = ether_ioctl(ifp, &sc->sc_ac, cmd, data);
		break;
	}

	return (error);
}

int
vxlan_media_change(struct ifnet *ifp)
{
	return (0);
}

void
vxlan_media_status(struct ifnet *ifp, struct ifmediareq *imr)
{
	imr->ifm_status = IFM_AVALID | IFM_ACTIVE;
}

int
vxlan_sockaddr_cmp(struct sockaddr *srcsa, struct sockaddr *dstsa)
{
	struct sockaddr_in	*src4, *dst4;
#ifdef INET6
	struct sockaddr_in6	*src6, *dst6;
#endif /* INET6 */

	if (srcsa->sa_family != dstsa->sa_family)
		return (1);

	switch (dstsa->sa_family) {
	case AF_INET:
		src4 = satosin(srcsa);
		dst4 = satosin(dstsa);
		if (src4->sin_addr.s_addr == dst4->sin_addr.s_addr)
			return (0);
		break;
#ifdef INET6
	case AF_INET6:
		src6 = satosin6(srcsa);
		dst6 = satosin6(dstsa);
		if (IN6_ARE_ADDR_EQUAL(&src6->sin6_addr, &dst6->sin6_addr) &&
		    src6->sin6_scope_id == dst6->sin6_scope_id)
			return (0);
		break;
#endif /* INET6 */
	}

	return (1);
}

uint16_t
vxlan_sockaddr_port(struct sockaddr *sa)
{
	struct sockaddr_in	*sin4;
#ifdef INET6
	struct sockaddr_in6	*sin6;
#endif /* INET6 */

	switch (sa->sa_family) {
	case AF_INET:
		sin4 = satosin(sa);
		return (sin4->sin_port);
#ifdef INET6
	case AF_INET6:
		sin6 = satosin6(sa);
		return (sin6->sin6_port);
#endif /* INET6 */
	default:
		break;
	}

	return (0);
}

int
vxlan_lookup(struct mbuf *m, struct udphdr *uh, int iphlen,
    struct sockaddr *srcsa, struct sockaddr *dstsa)
{
	struct mbuf_list	 ml = MBUF_LIST_INITIALIZER();
	struct vxlan_softc	*sc = NULL, *sc_cand = NULL;
	struct vxlan_header	 v;
	int			 vni;
	struct ifnet		*ifp;
	int			 skip;
#if NBRIDGE > 0
	struct bridge_tunneltag	*brtag;
#endif
	struct mbuf		*n;
	int			 off;

	/* XXX Should verify the UDP port first before copying the packet */
	skip = iphlen + sizeof(*uh);
	if (m->m_pkthdr.len - skip < sizeof(v))
		return (0);
	m_copydata(m, skip, sizeof(v), (caddr_t)&v);
	skip += sizeof(v);

	if (v.vxlan_flags & htonl(VXLAN_RESERVED1) ||
	    v.vxlan_id & htonl(VXLAN_RESERVED2))
		return (0);

	vni = ntohl(v.vxlan_id) >> VXLAN_VNI_S;
	if ((v.vxlan_flags & htonl(VXLAN_FLAGS_VNI)) == 0) {
		if (vni != 0)
			return (0);

		vni = VXLAN_VNI_UNSET;
	}

	NET_ASSERT_LOCKED();
	/* First search for a vxlan(4) interface with the packet's VNI */
	LIST_FOREACH(sc, &vxlan_tagh[VXLAN_TAGHASH(vni)], sc_entry) {
		if ((uh->uh_dport == sc->sc_dstport) &&
		    vni == sc->sc_vnetid &&
		    sc->sc_rdomain == rtable_l2(m->m_pkthdr.ph_rtableid)) {
			sc_cand = sc;
			if (vxlan_sockaddr_cmp(srcsa, sstosa(&sc->sc_dst)) == 0)
				goto found;
		}
	}

	/*
	 * Now loop through all the vxlan(4) interfaces that are configured
	 * to accept any VNI and operating in multipoint-to-multipoint mode
	 * that is used in combination with bridge(4) or switch(4).
	 * If a vxlan(4) interface has been found for the packet's VNI, this
	 * code is not reached as the other interface is more specific.
	 */
	LIST_FOREACH(sc, &vxlan_any, sc_entry) {
		if ((uh->uh_dport == sc->sc_dstport) &&
		    (sc->sc_rdomain == rtable_l2(m->m_pkthdr.ph_rtableid))) {
			sc_cand = sc;
			goto found;
		}
	}

	if (sc_cand) {
		sc = sc_cand;
		goto found;
	}

	/* not found */
	return (0);

 found:
	if (m->m_pkthdr.len < skip + sizeof(struct ether_header)) {
		m_freem(m);
		return (EINVAL);
	}

	m_adj(m, skip);
	ifp = &sc->sc_ac.ac_if;

#if NBRIDGE > 0
	/* Store the tunnel src/dst IP and vni for the bridge or switch */
	if ((ifp->if_bridgeport != NULL || ifp->if_switchport != NULL) &&
	    srcsa->sa_family != AF_UNSPEC &&
	    ((brtag = bridge_tunneltag(m)) != NULL)) {
		memcpy(&brtag->brtag_peer.sa, srcsa, srcsa->sa_len);
		memcpy(&brtag->brtag_local.sa, dstsa, dstsa->sa_len);
		brtag->brtag_id = vni;
	}
#endif

	m->m_flags &= ~(M_BCAST|M_MCAST);

#if NPF > 0
	pf_pkt_addr_changed(m);
#endif
	if ((m->m_len < sizeof(struct ether_header)) &&
	    (m = m_pullup(m, sizeof(struct ether_header))) == NULL)
		return (ENOBUFS);

	n = m_getptr(m, sizeof(struct ether_header), &off);
	if (n == NULL) {
		m_freem(m);
		return (EINVAL);
	}
	if (!ALIGNED_POINTER(mtod(n, caddr_t) + off, uint32_t)) {
		n = m_dup_pkt(m, ETHER_ALIGN, M_NOWAIT);
		/* Dispose of the original mbuf chain */
		m_freem(m);
		if (n == NULL)
			return (ENOBUFS);
		m = n;
	}

	ml_enqueue(&ml, m);
	if_input(ifp, &ml);

	/* success */
	return (1);
}

struct mbuf *
vxlan_encap4(struct ifnet *ifp, struct mbuf *m,
    struct sockaddr *src, struct sockaddr *dst)
{
	struct vxlan_softc	*sc = (struct vxlan_softc *)ifp->if_softc;
	struct ip		*ip;

	/*
	 * Remove multicast and broadcast flags or encapsulated packet
	 * ends up as multicast or broadcast packet.
	 */
	m->m_flags &= ~(M_BCAST|M_MCAST);

	M_PREPEND(m, sizeof(*ip), M_DONTWAIT);
	if (m == NULL)
		return (NULL);

	ip = mtod(m, struct ip *);
	ip->ip_v = IPVERSION;
	ip->ip_hl = sizeof(struct ip) >> 2;
	ip->ip_id = htons(ip_randomid());
	ip->ip_off = sc->sc_df;
	ip->ip_p = IPPROTO_UDP;
	ip->ip_tos = IFQ_PRIO2TOS(sc->sc_txhprio == IF_HDRPRIO_PACKET ?
	    m->m_pkthdr.pf.prio : sc->sc_txhprio);
	ip->ip_len = htons(m->m_pkthdr.len);

	ip->ip_src = satosin(src)->sin_addr;
	ip->ip_dst = satosin(dst)->sin_addr;

	if (sc->sc_ttl > 0)
		ip->ip_ttl = sc->sc_ttl;
	else
		ip->ip_ttl = IPDEFTTL;

	return (m);
}

#ifdef INET6
struct mbuf *
vxlan_encap6(struct ifnet *ifp, struct mbuf *m,
    struct sockaddr *src, struct sockaddr *dst)
{
	struct vxlan_softc	*sc = (struct vxlan_softc *)ifp->if_softc;
	struct ip6_hdr		*ip6;
	struct in6_addr		*in6a;
	uint32_t		 flow;

	/*
	 * Remove multicast and broadcast flags or encapsulated packet
	 * ends up as multicast or broadcast packet.
	 */
	m->m_flags &= ~(M_BCAST|M_MCAST);

	M_PREPEND(m, sizeof(struct ip6_hdr), M_DONTWAIT);
	if (m == NULL)
		return (NULL);

	flow = (uint32_t)IFQ_PRIO2TOS(sc->sc_txhprio == IF_HDRPRIO_PACKET ?
	    m->m_pkthdr.pf.prio : sc->sc_txhprio) << 20;

	ip6 = mtod(m, struct ip6_hdr *);
	ip6->ip6_flow = htonl(flow);
	ip6->ip6_vfc &= ~IPV6_VERSION_MASK;
	ip6->ip6_vfc |= IPV6_VERSION;
	ip6->ip6_nxt = IPPROTO_UDP;
	ip6->ip6_plen = htons(m->m_pkthdr.len - sizeof(struct ip6_hdr));
	if (in6_embedscope(&ip6->ip6_src, satosin6(src), NULL) != 0)
		goto drop;
	if (in6_embedscope(&ip6->ip6_dst, satosin6(dst), NULL) != 0)
		goto drop;

	if (sc->sc_ttl > 0)
		ip6->ip6_hlim = sc->sc_ttl;
	else
		ip6->ip6_hlim = ip6_defhlim;

	if (IN6_IS_ADDR_UNSPECIFIED(&satosin6(src)->sin6_addr)) {
		if (in6_selectsrc(&in6a, satosin6(dst), NULL,
		    sc->sc_rdomain) != 0)
			goto drop;

		ip6->ip6_src = *in6a;
	}

	if (sc->sc_df)
		SET(m->m_pkthdr.csum_flags, M_IPV6_DF_OUT);

	/*
	 * The UDP checksum of VXLAN packets should be set to zero,
	 * but the IPv6 UDP checksum is not optional.  There is an RFC 6539
	 * to relax the IPv6 UDP checksum requirement for tunnels, but it
	 * is currently not supported by most implementations.
	 */
	m->m_pkthdr.csum_flags |= M_UDP_CSUM_OUT;

	return (m);

drop:
	m_freem(m);
	return (NULL);
}
#endif /* INET6 */

int
vxlan_output(struct ifnet *ifp, struct mbuf *m)
{
	struct vxlan_softc	*sc = (struct vxlan_softc *)ifp->if_softc;
	struct vxlanudphdr	*vu;
	struct sockaddr		*src, *dst;
#if NBRIDGE > 0
	struct bridge_tunneltag	*brtag;
#endif
	int			 error, af;
	uint32_t		 tag;
	struct mbuf		*m0;

	/* VXLAN header, needs new mbuf because of alignment issues */
	MGET(m0, M_DONTWAIT, m->m_type);
	if (m0 == NULL) {
		ifp->if_oerrors++;
		return (ENOBUFS);
	}
	M_MOVE_PKTHDR(m0, m);
	m0->m_next = m;
	m = m0;
	m_align(m, sizeof(*vu));
	m->m_len = sizeof(*vu);
	m->m_pkthdr.len += sizeof(*vu);

	src = sstosa(&sc->sc_src);
	dst = sstosa(&sc->sc_dst);
	af = src->sa_family;

	vu = mtod(m, struct vxlanudphdr *);
	vu->vu_u.uh_sport = sc->sc_dstport;
	vu->vu_u.uh_dport = sc->sc_dstport;
	vu->vu_u.uh_ulen = htons(m->m_pkthdr.len);
	vu->vu_u.uh_sum = 0;
	tag = sc->sc_vnetid;

#if NBRIDGE > 0
	if ((brtag = bridge_tunnel(m)) != NULL) {
		dst = &brtag->brtag_peer.sa;

		/* If accepting any VNI, source ip address is from brtag */
		if (sc->sc_vnetid == VXLAN_VNI_ANY) {
			src = &brtag->brtag_local.sa;
			tag = (uint32_t)brtag->brtag_id;
			af = src->sa_family;
		}

		if (dst->sa_family != af) {
			ifp->if_oerrors++;
			m_freem(m);
			return (EINVAL);
		}
	} else
#endif
	if (sc->sc_vnetid == VXLAN_VNI_ANY) {
		/*
		 * If accepting any VNI, build the vxlan header only by
		 * bridge_tunneltag or drop packet if the tag does not exist.
		 */
		ifp->if_oerrors++;
		m_freem(m);
		return (ENETUNREACH);
	}

	if (sc->sc_vnetid != VXLAN_VNI_UNSET) {
		vu->vu_v.vxlan_flags = htonl(VXLAN_FLAGS_VNI);
		vu->vu_v.vxlan_id = htonl(tag << VXLAN_VNI_S);
	} else {
		vu->vu_v.vxlan_flags = htonl(0);
		vu->vu_v.vxlan_id = htonl(0);
	}

	switch (af) {
	case AF_INET:
		m = vxlan_encap4(ifp, m, src, dst);
		break;
#ifdef INET6
	case AF_INET6:
		m = vxlan_encap6(ifp, m, src, dst);
		break;
#endif /* INET6 */
	default:
		m_freem(m);
		m = NULL;
	}

	if (m == NULL) {
		ifp->if_oerrors++;
		return (ENOBUFS);
	}

#if NBRIDGE > 0
	if (brtag != NULL)
		bridge_tunneluntag(m);
#endif

	m->m_pkthdr.ph_rtableid = sc->sc_rdomain;

#if NPF > 0
	pf_pkt_addr_changed(m);
#endif

	switch (af) {
	case AF_INET:
		error = ip_output(m, NULL, NULL, IP_RAWOUTPUT,
		    &sc->sc_imo, NULL, 0);
		break;
#ifdef INET6
	case AF_INET6:
		error = ip6_output(m, 0, NULL, IPV6_MINMTU, 0, NULL);
		break;
#endif /* INET6 */
	default:
		m_freem(m);
		error = EAFNOSUPPORT;
	}

	if (error)
		ifp->if_oerrors++;

	return (error);
}

void
vxlan_addr_change(void *arg)
{
	struct vxlan_softc	*sc = arg;
	struct ifnet		*ifp = &sc->sc_ac.ac_if;
	int			 error;

	/*
	 * Reset the configuration after resume or any possible address
	 * configuration changes.
	 */
	if ((error = vxlan_config(ifp, NULL, NULL))) {
		/*
		 * The source address of the tunnel can temporarily disappear,
		 * after a link state change when running the DHCP client,
		 * so keep it configured.
		 */
	}
}

void
vxlan_if_change(void *arg)
{
	struct vxlan_softc	*sc = arg;
	struct ifnet		*ifp = &sc->sc_ac.ac_if;

	/*
	 * Reset the configuration after the parent interface disappeared.
	 */
	vxlan_multicast_cleanup(ifp);
	memset(&sc->sc_src, 0, sizeof(sc->sc_src));
	memset(&sc->sc_dst, 0, sizeof(sc->sc_dst));
	sc->sc_dstport = htons(VXLAN_PORT);
}

void
vxlan_link_change(void *arg)
{
	struct vxlan_softc	*sc = arg;
	struct ifnet		*ifp = &sc->sc_ac.ac_if;

	/*
	 * The machine might have lost its multicast associations after
	 * link state changes.  This fixes a problem with VMware after
	 * suspend/resume of the host or guest.
	 */
	(void)vxlan_config(ifp, NULL, NULL);
}
