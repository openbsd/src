/*      $OpenBSD: if_gre.c,v 1.91 2018/02/07 22:30:59 dlg Exp $ */
/*	$NetBSD: if_gre.c,v 1.9 1999/10/25 19:18:11 drochner Exp $ */

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Heiko W.Rupp <hwr@pilhuhn.de>
 *
 * IPv6-over-GRE contributed by Gert Doering <gert@greenie.muc.de>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Encapsulate L3 protocols into IP, per RFC 1701 and 1702.
 * See gre(4) for more details.
 * Also supported: IP in IP encapsulation (proto 55) per RFC 2004.
 */

#include "bpfilter.h"
#include "pf.h"

#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/tree.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_media.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/if_ether.h>

#ifdef INET6
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#endif

#ifdef PIPEX
#include <net/pipex.h>
#endif

#ifdef MPLS
#include <netmpls/mpls.h>
#endif /* MPLS */

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#if NPF > 0
#include <net/pfvar.h>
#endif

#include <net/if_gre.h>

#include <netinet/ip_gre.h>
#include <sys/sysctl.h>

/*
 * packet formats
 */
struct gre_header {
	uint16_t		gre_flags;
#define GRE_CP				0x8000  /* Checksum Present */
#define GRE_KP				0x2000  /* Key Present */
#define GRE_SP				0x1000  /* Sequence Present */

#define GRE_VERS_MASK			0x0007
#define GRE_VERS_0			0x0000
#define GRE_VERS_1			0x0001

	uint16_t		gre_proto;
} __packed __aligned(4);

struct gre_h_cksum {
	uint16_t		gre_cksum;
	uint16_t		gre_reserved1;
} __packed __aligned(4);

struct gre_h_key {
	uint32_t		gre_key;
} __packed __aligned(4);

struct gre_h_seq {
	uint32_t		gre_seq;
} __packed __aligned(4);


/*
 * GRE tunnel metadata
 */

struct gre_tunnel {
	RBT_ENTRY(gre_entry)	t_entry;

	uint32_t		t_key_mask;
#define GRE_KEY_NONE			htonl(0x00000000U)
#define GRE_KEY_ENTROPY			htonl(0xffffff00U)
#define GRE_KEY_MASK			htonl(0xffffffffU)
	uint32_t		t_key;

	u_int			t_rtableid;
	int			t_af;
	uint32_t		t_src[4];
	uint32_t		t_dst[4];

	uint8_t			t_ttl;
};

RBT_HEAD(gre_tree, gre_tunnel);

static inline int
		gre_cmp(const struct gre_tunnel *, const struct gre_tunnel *);

RBT_PROTOTYPE(gre_tree, gre_tunnel, t_entry, gre_cmp);

static int	gre_set_tunnel(struct gre_tunnel *, struct if_laddrreq *);
static int	gre_get_tunnel(struct gre_tunnel *, struct if_laddrreq *);
static int	gre_del_tunnel(struct gre_tunnel *);

static int	gre_set_vnetid(struct gre_tunnel *, struct ifreq *);
static int	gre_get_vnetid(struct gre_tunnel *, struct ifreq *);
static int	gre_del_vnetid(struct gre_tunnel *);

static int	gre_ip_output(const struct gre_tunnel *, struct mbuf *,
		    uint8_t);
/*
 * layer 3 GRE tunnels
 */

struct gre_softc {
	struct gre_tunnel	sc_tunnel; /* must be first */
	struct ifnet		sc_if;
};

static int	gre_clone_create(struct if_clone *, int);
static int	gre_clone_destroy(struct ifnet *);

struct if_clone gre_cloner =
    IF_CLONE_INITIALIZER("gre", gre_clone_create, gre_clone_destroy);

struct gre_tree gre_softcs = RBT_INITIALIZER();

static int	gre_output(struct ifnet *, struct mbuf *, struct sockaddr *,
		    struct rtentry *);
static void	gre_start(struct ifnet *);
static int	gre_ioctl(struct ifnet *, u_long, caddr_t);

static int	gre_up(struct gre_softc *);
static int	gre_down(struct gre_softc *);

static int	gre_input_key(struct mbuf **, int *, int, int,
		    struct gre_tunnel *);

static struct mbuf *
		gre_encap(struct gre_softc *, struct mbuf *, uint8_t *);

/*
 * It is not easy to calculate the right value for a GRE MTU.
 * We leave this task to the admin and use the same default that
 * other vendors use.
 */
#define GREMTU 1476

/*
 * We can control the acceptance of GRE and MobileIP packets by
 * altering the sysctl net.inet.gre.allow values
 * respectively. Zero means drop them, all else is acceptance.  We can also
 * control acceptance of WCCPv1-style GRE packets through the
 * net.inet.gre.wccp value, but be aware it depends upon normal GRE being
 * allowed as well.
 *
 */
int gre_allow = 0;
int gre_wccp = 0;

void
greattach(int n)
{
	if_clone_attach(&gre_cloner);
}

static int
gre_clone_create(struct if_clone *ifc, int unit)
{
	struct gre_softc *sc;
	struct ifnet *ifp;

	sc = malloc(sizeof(*sc), M_DEVBUF, M_WAITOK|M_ZERO);
	snprintf(sc->sc_if.if_xname, sizeof sc->sc_if.if_xname, "%s%d",
	    ifc->ifc_name, unit);

	ifp = &sc->sc_if;
	ifp->if_softc = sc;
	ifp->if_type = IFT_TUNNEL;
	ifp->if_hdrlen = 24; /* IP + GRE */
	ifp->if_mtu = GREMTU;
	ifp->if_flags = IFF_POINTOPOINT|IFF_MULTICAST;
	ifp->if_xflags = IFXF_CLONED;
	ifp->if_output = gre_output;
	ifp->if_start = gre_start;
	ifp->if_ioctl = gre_ioctl;
	ifp->if_rtrequest = p2p_rtrequest;

	if_attach(ifp);
	if_alloc_sadl(ifp);

#if NBPFILTER > 0
	bpfattach(&ifp->if_bpf, ifp, DLT_LOOP, sizeof(uint32_t));
#endif

	return (0);
}

static int
gre_clone_destroy(struct ifnet *ifp)
{
	struct gre_softc *sc = ifp->if_softc;

	NET_LOCK();
	if (ISSET(ifp->if_flags, IFF_RUNNING))
		gre_down(sc);
	NET_UNLOCK();

	if_detach(ifp);

	free(sc, M_DEVBUF, sizeof(*sc));

	return (0);
}

int
gre_input(struct mbuf **mp, int *offp, int type, int af)
{
	struct mbuf *m = *mp;
	struct gre_tunnel key;
	struct ip *ip;

	ip = mtod(m, struct ip *);

	key.t_af = AF_INET;
	key.t_src[0] = ip->ip_dst.s_addr;
	key.t_dst[0] = ip->ip_src.s_addr;

	if (gre_input_key(mp, offp, type, af, &key) == -1)
		return (rip_input(mp, offp, type, af));

	return (IPPROTO_DONE);
}

#ifdef INET6
int
gre_input6(struct mbuf **mp, int *offp, int type, int af)
{
	struct mbuf *m = *mp;
	struct gre_tunnel key;
	struct ip6_hdr *ip6;

	ip6 = mtod(m, struct ip6_hdr *);

	key.t_af = AF_INET6;
	memcpy(key.t_src, &ip6->ip6_dst, sizeof(key.t_src));
	memcpy(key.t_dst, &ip6->ip6_src, sizeof(key.t_dst));

	if (gre_input_key(mp, offp, type, af, &key) == -1)
		return (rip6_input(mp, offp, type, af));

	return (IPPROTO_DONE);
}
#endif /* INET6 */

static int
gre_input_key(struct mbuf **mp, int *offp, int type, int af,
    struct gre_tunnel *key)
{
	struct mbuf *m = *mp;
	int iphlen = *offp, hlen;
	struct gre_softc *sc;
	struct ifnet *ifp;
	caddr_t buf;
	struct gre_header *gh;
	struct gre_h_key *gkh;
	void (*input)(struct ifnet *, struct mbuf *);
	int bpf_af = AF_UNSPEC; /* bpf */

	if (!gre_allow)
		goto decline;

	hlen = iphlen + sizeof(*gh);
	if (m->m_pkthdr.len < hlen)
		goto decline;

	m = m_pullup(m, hlen);
	if (m == NULL)
		return (IPPROTO_DONE);

	buf = mtod(m, caddr_t);
	gh = (struct gre_header *)(buf + iphlen);

	/* check the version */
	switch (gh->gre_flags & htons(GRE_VERS_MASK)) {
	case htons(GRE_VERS_0):
		break;

	case htons(GRE_VERS_1):
#ifdef PIPEX
		if (pipex_enable) {
			struct pipex_session *session;

			session = pipex_pptp_lookup_session(m);
			if (session != NULL &&
			    pipex_pptp_input(m, session) == NULL)
				return (IPPROTO_DONE);
		}
#endif
		/* FALLTHROUGH */
	default:
		goto decline;
	}

	/* the only optional bit in the header is K flag */
	if ((gh->gre_flags & htons(~(GRE_KP|GRE_VERS_MASK))) != htons(0))
		goto decline;

	if (gh->gre_flags & htons(GRE_KP)) {
		hlen += sizeof(*gkh);
		if (m->m_pkthdr.len < hlen)
			goto decline;

		m = m_pullup(m, hlen);
		if (m == NULL)
			return (IPPROTO_DONE);

		buf = mtod(m, caddr_t);
		gh = (struct gre_header *)(buf + iphlen);
		gkh = (struct gre_h_key *)(gh + 1);

		key->t_key_mask = GRE_KEY_MASK;
		key->t_key = gkh->gre_key;
	} else
		key->t_key_mask = GRE_KEY_NONE;

	key->t_rtableid = m->m_pkthdr.ph_rtableid;

	switch (gh->gre_proto) {
	case htons(ETHERTYPE_IP):
#if NBPFILTER > 0
		bpf_af = AF_INET;
#endif
		input = ipv4_input;
		break;
#ifdef INET6
	case htons(ETHERTYPE_IPV6):
#if NBPFILTER > 0
		bpf_af = AF_INET6;
#endif
		input = ipv6_input;
		break;
#endif
#ifdef MPLS
	case htons(ETHERTYPE_MPLS):
	case htons(ETHERTYPE_MPLS_MCAST):
#if NBPFILTER > 0
		bpf_af = AF_MPLS;
#endif
		input = mpls_input;
		break;
#endif

	case htons(ETHERTYPE_TRANSETHER): /* not yet */
	default:
		goto decline;
	}

	sc = (struct gre_softc *)RBT_FIND(gre_tree, &gre_softcs, key);
	if (sc == NULL)
		goto decline;

	ifp = &sc->sc_if;

	m_adj(m, hlen);

	m->m_flags &= ~(M_MCAST|M_BCAST);
	m->m_pkthdr.ph_ifidx = ifp->if_index;
	m->m_pkthdr.ph_rtableid = ifp->if_rdomain;

#if NPF > 0
	pf_pkt_addr_changed(m);
#endif

	ifp->if_ipackets++;
	ifp->if_ibytes += m->m_pkthdr.len;

#if NBPFILTER > 0
	if (ifp->if_bpf)
		bpf_mtap_af(ifp->if_bpf, bpf_af, m, BPF_DIRECTION_IN);
#endif

	(*input)(ifp, m);
	return (IPPROTO_DONE);
decline:
	mp = &m;
	return (-1);
}

static int
gre_output(struct ifnet *ifp, struct mbuf *m, struct sockaddr *dst,
    struct rtentry *rt)
{
	struct m_tag *mtag;
	int error = 0;

	if (!gre_allow) {
		error = EACCES;
		goto drop;
	}

	if (!ISSET(ifp->if_flags, IFF_RUNNING)) {
		error = ENETDOWN;
		goto drop;
	}

	switch (dst->sa_family) {
	case AF_INET:
#ifdef INET6
	case AF_INET6:
#endif
#ifdef MPLS
	case AF_MPLS:
#endif
		break;
	default:
		error = EAFNOSUPPORT;
		goto drop;
	}

	/* Try to limit infinite recursion through misconfiguration. */
	for (mtag = m_tag_find(m, PACKET_TAG_GRE, NULL); mtag;
	     mtag = m_tag_find(m, PACKET_TAG_GRE, mtag)) {
		if (memcmp((caddr_t)(mtag + 1), &ifp->if_index,
		    sizeof(ifp->if_index)) == 0) {
			m_freem(m);
			error = EIO;
			goto end;
		}
	}

	mtag = m_tag_get(PACKET_TAG_GRE, sizeof(ifp->if_index), M_NOWAIT);
	if (mtag == NULL) {
		m_freem(m);
		error = ENOBUFS;
		goto end;
	}
	memcpy((caddr_t)(mtag + 1), &ifp->if_index, sizeof(ifp->if_index));
	m_tag_prepend(m, mtag);

	m->m_pkthdr.ph_family = dst->sa_family;

	error = if_enqueue(ifp, m);
end:
	if (error)
		ifp->if_oerrors++;
	return (error);

drop:
	m_freem(m);
	return (error);
}

void
gre_start(struct ifnet *ifp)
{
	struct gre_softc *sc = ifp->if_softc;
	struct mbuf *m;
	uint8_t tos;
#if NBPFILTER > 0
	caddr_t if_bpf;
#endif

	while ((m = ifq_dequeue(&ifp->if_snd)) != NULL) {
#if NBPFILTER > 0
		if_bpf = ifp->if_bpf;
		if (if_bpf) {
			int af = m->m_pkthdr.ph_family;
			bpf_mtap_af(if_bpf, af, m, BPF_DIRECTION_OUT);
		}
#endif

		m = gre_encap(sc, m, &tos);
		if (m == NULL || gre_ip_output(&sc->sc_tunnel, m, tos) != 0)
			ifp->if_oerrors++;
	}
}

static struct mbuf *
gre_encap(struct gre_softc *sc, struct mbuf *m, uint8_t *tos)
{
	struct gre_header *gh;
	struct gre_h_key *gkh;
	uint16_t proto;
	int hlen;

	*tos = 0;
	switch (m->m_pkthdr.ph_family) {
	case AF_INET: {
		proto = htons(ETHERTYPE_IP);

		struct ip *ip = mtod(m, struct ip *);
		*tos = ip->ip_tos;
		break;
	}
#ifdef INET6
	case AF_INET6:
		proto = htons(ETHERTYPE_IPV6);
		break;
#endif
#ifdef MPLS
	case AF_MPLS:
		if (m->m_flags & (M_BCAST | M_MCAST))
			proto = htons(ETHERTYPE_MPLS_MCAST);
		else
			proto = htons(ETHERTYPE_MPLS);
		break;
#endif
	default:
		unhandled_af(m->m_pkthdr.ph_family);
	}

	hlen = sizeof(*gh);
	if (sc->sc_tunnel.t_key_mask != GRE_KEY_NONE)
		hlen += sizeof(*gkh);

	m = m_prepend(m, hlen, M_DONTWAIT);
	if (m == NULL)
		return (NULL);

	gh = mtod(m, struct gre_header *);
	gh->gre_flags = GRE_VERS_0;
	gh->gre_proto = proto;
	if (sc->sc_tunnel.t_key_mask != GRE_KEY_NONE) {
		gh->gre_flags |= htons(GRE_KP);

		gkh = (struct gre_h_key *)(gh + 1);
		gkh->gre_key = sc->sc_tunnel.t_key;
	}

	return (m);
}

static int
gre_ip_output(const struct gre_tunnel *tunnel, struct mbuf *m, uint8_t tos)
{
	m->m_flags &= ~(M_BCAST|M_MCAST);
	m->m_pkthdr.ph_rtableid = tunnel->t_rtableid;

#if NPF > 0
	pf_pkt_addr_changed(m);
#endif

	switch (tunnel->t_af) {
	case AF_INET: {
		struct ip *ip;

		m = m_prepend(m, sizeof(*ip), M_DONTWAIT);
		if (m == NULL)
			return (ENOMEM);

		ip = mtod(m, struct ip *);
		ip->ip_tos = tos;
		ip->ip_len = htons(m->m_pkthdr.len);
		ip->ip_ttl = tunnel->t_ttl;
		ip->ip_p = IPPROTO_GRE;
		ip->ip_src.s_addr = tunnel->t_src[0];
		ip->ip_dst.s_addr = tunnel->t_dst[0];

		ip_send(m);
		break;
	}
#ifdef INET6
	case AF_INET6: {
		struct ip6_hdr *ip6;
		int len = m->m_pkthdr.len;

		m = m_prepend(m, sizeof(*ip6), M_DONTWAIT);
		if (m == NULL)
			return (ENOMEM);

		ip6 = mtod(m, struct ip6_hdr *);
		ip6->ip6_flow = ISSET(m->m_pkthdr.ph_flowid, M_FLOWID_VALID) ?
		    htonl(m->m_pkthdr.ph_flowid & M_FLOWID_MASK) : 0;
		ip6->ip6_vfc |= IPV6_VERSION;
		ip6->ip6_plen = htons(len);
		ip6->ip6_nxt = IPPROTO_GRE;
		ip6->ip6_hlim = tunnel->t_ttl;
		memcpy(&ip6->ip6_src, tunnel->t_src, sizeof(ip6->ip6_src));
		memcpy(&ip6->ip6_dst, tunnel->t_dst, sizeof(ip6->ip6_dst));

		ip6_send(m);
		break;
	}
#endif /* INET6 */
	default:
		panic("%s: unsupported af %d in %p", __func__, tunnel->t_af,
		    tunnel);
	}

	return (0);
}

static int
gre_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct ifreq *ifr = (struct ifreq *)data;
	struct gre_softc *sc = ifp->if_softc;
	int error = 0;

	switch(cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		/* FALLTHROUGH */
	case SIOCSIFFLAGS:
		if (ISSET(ifp->if_flags, IFF_UP)) {
			if (!ISSET(ifp->if_flags, IFF_RUNNING))
				error = gre_up(sc);
			else
				error = 0;
		} else {
			if (ISSET(ifp->if_flags, IFF_RUNNING))
				error = gre_down(sc);
		}
		break;
	case SIOCSIFMTU:
		if (ifr->ifr_mtu < 576) {
			error = EINVAL;
			break;
		}
		ifp->if_mtu = ifr->ifr_mtu;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		break;

	case SIOCSVNETID:
		if (ISSET(ifp->if_flags, IFF_RUNNING)) {
			error = EBUSY;
			break;
		}
		error = gre_set_vnetid(&sc->sc_tunnel, ifr);
		break;

	case SIOCGVNETID:
		error = gre_get_vnetid(&sc->sc_tunnel, ifr);
		break;
	case SIOCDVNETID:
		if (ISSET(ifp->if_flags, IFF_RUNNING)) {
			error = EBUSY;
			break;
		}
		error = gre_del_vnetid(&sc->sc_tunnel);
		break;

	case SIOCSLIFPHYADDR:
		if (ISSET(ifp->if_flags, IFF_RUNNING)) {
			error = EBUSY;
			break;
		}
		error = gre_set_tunnel(&sc->sc_tunnel,
		    (struct if_laddrreq *)data);
		break;
	case SIOCGLIFPHYADDR:
		error = gre_get_tunnel(&sc->sc_tunnel,
		    (struct if_laddrreq *)data);
		break;
	case SIOCDIFPHYADDR:
		if (ISSET(ifp->if_flags, IFF_RUNNING)) {
			error = EBUSY;
			break;
		}

		error = gre_del_tunnel(&sc->sc_tunnel);
		break;

	case SIOCSLIFPHYRTABLE:
		if (ISSET(ifp->if_flags, IFF_RUNNING)) {
			error = EBUSY;
			break;
		}

		if (ifr->ifr_rdomainid < 0 ||
		    ifr->ifr_rdomainid > RT_TABLEID_MAX ||
		    !rtable_exists(ifr->ifr_rdomainid)) {
			error = EINVAL;
			break;
		}
		sc->sc_tunnel.t_rtableid = ifr->ifr_rdomainid;
		break;
	case SIOCGLIFPHYRTABLE:
		ifr->ifr_rdomainid = sc->sc_tunnel.t_rtableid;
		break;

	case SIOCSLIFPHYTTL:
		if (ifr->ifr_ttl < 0 || ifr->ifr_ttl > 0xff) {
			error = EINVAL;
			break;
		}

		/* commit */
		sc->sc_tunnel.t_ttl = (uint8_t)ifr->ifr_ttl;
		break;

	case SIOCGLIFPHYTTL:
		ifr->ifr_ttl = (int)sc->sc_tunnel.t_ttl;
		break;

	default:
		error = ENOTTY;
	}

	return (error);
}

static int
gre_up(struct gre_softc *sc)
{
	int error = 0;
 
	if (sc->sc_tunnel.t_af == AF_UNSPEC)
		return (ENXIO);

	NET_ASSERT_LOCKED(); 
	if (RBT_INSERT(gre_tree, &gre_softcs, &sc->sc_tunnel) != NULL)
		return (EBUSY);

	SET(sc->sc_if.if_flags, IFF_RUNNING);

	return (error);
}

static int
gre_down(struct gre_softc *sc)
{
	NET_ASSERT_LOCKED();
	RBT_REMOVE(gre_tree, &gre_softcs, &sc->sc_tunnel);

	CLR(sc->sc_if.if_flags, IFF_RUNNING);

	return (0);
}

static int
gre_set_tunnel(struct gre_tunnel *tunnel, struct if_laddrreq *req)
{
	struct sockaddr *src = (struct sockaddr *)&req->addr;
	struct sockaddr *dst = (struct sockaddr *)&req->dstaddr;
	struct sockaddr_in *src4, *dst4;
#ifdef INET6
	struct sockaddr_in6 *src6, *dst6;
	int error;
#endif

	/* sa_family and sa_len must be equal */
	if (src->sa_family != dst->sa_family || src->sa_len != dst->sa_len)
		return (EINVAL);

	/* validate */
	switch (dst->sa_family) {
	case AF_INET:
		if (dst->sa_len != sizeof(*dst4))
			return (EINVAL);

		src4 = (struct sockaddr_in *)src;
		if (in_nullhost(src4->sin_addr) ||
		    IN_MULTICAST(src4->sin_addr.s_addr))
			return (EINVAL);

		dst4 = (struct sockaddr_in *)dst;
		if (in_nullhost(dst4->sin_addr) ||
		    IN_MULTICAST(dst4->sin_addr.s_addr))
			return (EINVAL);

		tunnel->t_src[0] = src4->sin_addr.s_addr;
		tunnel->t_dst[0] = dst4->sin_addr.s_addr;

		break;
#ifdef INET6
	case AF_INET6:
		if (dst->sa_len != sizeof(*dst6))
			return (EINVAL);

		src6 = (struct sockaddr_in6 *)src;
		if (IN6_IS_ADDR_UNSPECIFIED(&src6->sin6_addr) ||
		    IN6_IS_ADDR_MULTICAST(&src6->sin6_addr))
			return (EINVAL);

		dst6 = (struct sockaddr_in6 *)dst;
		if (IN6_IS_ADDR_UNSPECIFIED(&dst6->sin6_addr) ||
		    IN6_IS_ADDR_MULTICAST(&dst6->sin6_addr))
			return (EINVAL);

		error = in6_embedscope((struct in6_addr *)tunnel->t_src,
		    src6, NULL);
		if (error != 0)
			return (error);

		error = in6_embedscope((struct in6_addr *)tunnel->t_dst,
		    dst6, NULL);
		if (error != 0)
			return (error);

		break;
#endif
	default:
		return (EAFNOSUPPORT);
	}

	/* commit */
	tunnel->t_af = dst->sa_family;

	return (0);
}

static int
gre_get_tunnel(struct gre_tunnel *tunnel, struct if_laddrreq *req)
{
	struct sockaddr *src = (struct sockaddr *)&req->addr;
	struct sockaddr *dst = (struct sockaddr *)&req->dstaddr;
	struct sockaddr_in *sin;
#ifdef INET6 /* ifconfig already embeds the scopeid */
	struct sockaddr_in6 *sin6;
#endif

	switch (tunnel->t_af) {
	case AF_UNSPEC:
		return (EADDRNOTAVAIL);
	case AF_INET:
		sin = (struct sockaddr_in *)src;
		memset(sin, 0, sizeof(*sin));
		sin->sin_family = AF_INET;
		sin->sin_len = sizeof(*sin);
		sin->sin_addr.s_addr = tunnel->t_src[0];

		sin = (struct sockaddr_in *)dst;
		memset(sin, 0, sizeof(*sin));
		sin->sin_family = AF_INET;
		sin->sin_len = sizeof(*sin);
		sin->sin_addr.s_addr = tunnel->t_dst[0];

		break;

#ifdef INET6
	case AF_INET6:
		sin6 = (struct sockaddr_in6 *)src;
		memset(sin6, 0, sizeof(*sin6));
		sin6->sin6_family = AF_INET6;
		sin6->sin6_len = sizeof(*sin6);
		in6_recoverscope(sin6, (struct in6_addr *)tunnel->t_src);

		sin6 = (struct sockaddr_in6 *)dst;
		memset(sin6, 0, sizeof(*sin6));
		sin6->sin6_family = AF_INET6;
		sin6->sin6_len = sizeof(*sin6);
		in6_recoverscope(sin6, (struct in6_addr *)tunnel->t_dst);

		break;
#endif
	default:
		return (EAFNOSUPPORT);
	}

	return (0);
}

static int
gre_del_tunnel(struct gre_tunnel *tunnel)
{
	/* commit */
	tunnel->t_af = AF_UNSPEC;

	return (0);
}

static int
gre_set_vnetid(struct gre_tunnel *tunnel, struct ifreq *ifr)
{
	uint32_t key;

	if (ifr->ifr_vnetid < 0 || ifr->ifr_vnetid > 0xffffffff)
		return EINVAL;

	key = htonl(ifr->ifr_vnetid);

	if (tunnel->t_key_mask == GRE_KEY_MASK && tunnel->t_key == key)
		return (0);

	/* commit */
	tunnel->t_key_mask = GRE_KEY_MASK;
	tunnel->t_key = key;

	return (0);
}

static int
gre_get_vnetid(struct gre_tunnel *tunnel, struct ifreq *ifr)
{
	if (tunnel->t_key_mask == GRE_KEY_NONE)
		return (EADDRNOTAVAIL);

	ifr->ifr_vnetid = (int64_t)ntohl(tunnel->t_key);

	return (0);
}

static int
gre_del_vnetid(struct gre_tunnel *tunnel)
{
	tunnel->t_key_mask = GRE_KEY_NONE;

	return (0);
}

int
gre_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp, void *newp,
    size_t newlen)
{
	int error;

	/* All sysctl names at this level are terminal. */
	if (namelen != 1)
		return (ENOTDIR);

	switch (name[0]) {
	case GRECTL_ALLOW:
		NET_LOCK();
		error = sysctl_int(oldp, oldlenp, newp, newlen, &gre_allow);
		NET_UNLOCK();
		return (error);
	case GRECTL_WCCP:
		NET_LOCK();
		error = sysctl_int(oldp, oldlenp, newp, newlen, &gre_wccp);
		NET_UNLOCK();
		return (error);
	default:
		return (ENOPROTOOPT);
        }
	/* NOTREACHED */
}

static inline int
gre_ip_cmp(int af, const uint32_t *a, const uint32_t *b)
{
	switch (af) {
#ifdef INET6
	case AF_INET6:
		if (a[3] > b[3])
			return (1);
		if (a[3] < b[3])
			return (-1);

		if (a[2] > b[2])
			return (1);
		if (a[2] < b[2])
			return (-1);

		if (a[1] > b[1])
			return (1);
		if (a[1] < b[1])
			return (-1);

		/* FALLTHROUGH */
#endif /* INET6 */
	case AF_INET:
		if (a[0] > b[0])
			return (1);
		if (a[0] < b[0])
			return (-1);
		break;
	default:
		panic("%s: unsupported af %d\n", __func__, af);
	}

	return (0);
}

static inline int
gre_cmp(const struct gre_tunnel *a, const struct gre_tunnel *b)
{
	uint32_t ka, kb;
	uint32_t mask;
	int rv;

	/* sort by routing table */
	if (a->t_rtableid > b->t_rtableid)
		return (1);
	if (a->t_rtableid < b->t_rtableid)
		return (-1);

	/* sort by address */
	if (a->t_af > b->t_af)
		return (1);
	if (a->t_af < b->t_af)
		return (-1);

	rv = gre_ip_cmp(a->t_af, a->t_dst, b->t_dst);
	if (rv != 0)
		return (rv);

	rv = gre_ip_cmp(a->t_af, a->t_src, b->t_src);
	if (rv != 0)
		return (rv);

	/* is K set at all? */
	ka = a->t_key_mask & GRE_KEY_ENTROPY;
	kb = b->t_key_mask & GRE_KEY_ENTROPY;

	/* sort by whether K is set */
	if (ka > kb)
		return (1);
	if (ka < kb)
		return (-1);

	/* is K set on both? */
	if (ka != GRE_KEY_NONE) {
		/* get common prefix */
		mask = a->t_key_mask & b->t_key_mask;

		ka = a->t_key & mask;
		kb = b->t_key & mask;

		/* sort by common prefix */
		if (ka > kb)
			return (1);
		if (ka < kb)
			return (-1);
	}

	return (0);
}

RBT_GENERATE(gre_tree, gre_tunnel, t_entry, gre_cmp);

