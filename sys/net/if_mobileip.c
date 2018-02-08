/*	$OpenBSD: if_mobileip.c,v 1.4 2018/02/08 21:55:34 dlg Exp $ */

/*
 * Copyright (c) 2016 David Gwynne <dlg@openbsd.org>
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

#include "mobileip.h"

#include "bpfilter.h"
#include "pf.h"

#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/timeout.h>
#include <sys/tree.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_var.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#if NPF > 0
#include <net/pfvar.h>
#endif

#include <net/if_mobileip.h>

struct mobileip_softc {
	struct ifnet		sc_if;

	RBT_ENTRY(mobileip_softc)
				sc_entry;

	unsigned int		sc_rtableid;
	uint32_t		sc_src;
	uint32_t		sc_dst;
};

static int	mobileip_clone_create(struct if_clone *, int);
static int	mobileip_clone_destroy(struct ifnet *);

static struct if_clone mobileip_cloner = IF_CLONE_INITIALIZER("mobileip",
    mobileip_clone_create, mobileip_clone_destroy);

RBT_HEAD(mobileip_tree, mobileip_softc);

static inline int
		mobileip_cmp(const struct mobileip_softc *,
		    const struct mobileip_softc *);

RBT_PROTOTYPE(mobileip_tree, mobileip_softc, sc_entry, mobileip_cmp);

struct mobileip_tree mobileip_softcs = RBT_INITIALIZER();

#define MOBILEIPMTU	(1500 - (sizeof(struct mobileip_header) +	\
			    sizeof(struct mobileip_h_src)))		\

static int	mobileip_ioctl(struct ifnet *, u_long, caddr_t);
static int	mobileip_up(struct mobileip_softc *);
static int	mobileip_down(struct mobileip_softc *);
static int	mobileip_set_tunnel(struct mobileip_softc *,
		    struct if_laddrreq *);
static int	mobileip_get_tunnel(struct mobileip_softc *,
		    struct if_laddrreq *);
static int	mobileip_del_tunnel(struct mobileip_softc *);

static int	mobileip_output(struct ifnet *, struct mbuf *,
		    struct sockaddr *, struct rtentry *);
static void	mobileip_start(struct ifnet *);
static int	mobileip_encap(struct mobileip_softc *, struct mbuf *);

/*
 * let's begin
 */

int	mobileip_allow = 0;

void
mobileipattach(int n)
{
	if_clone_attach(&mobileip_cloner);
}

int
mobileip_clone_create(struct if_clone *ifc, int unit)
{
	struct mobileip_softc *sc;

	sc = malloc(sizeof(*sc), M_DEVBUF, M_NOWAIT|M_ZERO);
	if (!sc)
		return (ENOMEM);

	sc->sc_rtableid = 0;
	sc->sc_src = INADDR_ANY;
	sc->sc_dst = INADDR_ANY;

	snprintf(sc->sc_if.if_xname, sizeof sc->sc_if.if_xname, "%s%d",
	    ifc->ifc_name, unit);
	sc->sc_if.if_softc = sc;
	sc->sc_if.if_type = IFT_TUNNEL;
	sc->sc_if.if_addrlen = 0;
	sc->sc_if.if_mtu = MOBILEIPMTU;
	sc->sc_if.if_flags = IFF_POINTOPOINT|IFF_MULTICAST;
	sc->sc_if.if_output = mobileip_output;
	sc->sc_if.if_start = mobileip_start;
	sc->sc_if.if_ioctl = mobileip_ioctl;
	sc->sc_if.if_rtrequest = p2p_rtrequest;

	if_attach(&sc->sc_if);
	if_alloc_sadl(&sc->sc_if);

#if NBPFILTER > 0
	bpfattach(&sc->sc_if.if_bpf, &sc->sc_if, DLT_LOOP, sizeof(uint32_t));
#endif

	return (0);
}

int
mobileip_clone_destroy(struct ifnet *ifp)
{
	struct mobileip_softc *sc = ifp->if_softc;

	if_detach(ifp);

	NET_LOCK();
	if (ISSET(ifp->if_flags, IFF_RUNNING))
		mobileip_down(sc);
	NET_UNLOCK();

	free(sc, M_DEVBUF, sizeof(*sc));

	return (0);
}

/*
 * do a checksum of a header.
 *
 * assumes len is aligned correctly, and not an odd number of bytes.
 */
static inline uint16_t
mobileip_cksum(const void *buf, size_t len)
{
	const uint16_t *p = buf;
	uint32_t sum = 0;

	do {
		sum += bemtoh16(p++);
	} while (len -= 2);

	/* end-around-carry */
	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);
	return (~sum);
}

static inline int
mobileip_cmp(const struct mobileip_softc *a, const struct mobileip_softc *b)
{
	if (a->sc_src > b->sc_src)
		return (1);
	if (a->sc_src < b->sc_src)
		return (-1);

	if (a->sc_dst > b->sc_dst)
		return (1);
	if (a->sc_dst < b->sc_dst)
		return (-1);

	if (a->sc_rtableid > b->sc_rtableid)
		return (1);
	if (a->sc_rtableid < b->sc_rtableid)
		return (-1);

	return (0);
}

static int
mobileip_output(struct ifnet *ifp, struct mbuf *m, struct sockaddr *dst,
    struct rtentry *rt)
{
	struct m_tag *mtag;
	int error = 0;

	if (!mobileip_allow) {
		m_freem(m);
		error = EACCES;
		goto end;
	}

	if (!ISSET(ifp->if_flags, IFF_RUNNING)) {
		m_freem(m);
		error = ENETDOWN;
		goto end;
	}

	if (dst->sa_family != AF_INET) {
		m_freem(m);
		error = EAFNOSUPPORT;
		goto end;
	}

	/* Try to limit infinite recursion through misconfiguration. */
	for (mtag = m_tag_find(m, PACKET_TAG_GRE, NULL); mtag;
	     mtag = m_tag_find(m, PACKET_TAG_GRE, mtag)) {
		if (memcmp(mtag + 1, &ifp->if_index,
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
	memcpy(mtag + 1, &ifp->if_index, sizeof(ifp->if_index));
	m_tag_prepend(m, mtag);

	error = if_enqueue(ifp, m);
  end:
	if (error)
		ifp->if_oerrors++;
	return (error);
}

static void
mobileip_start(struct ifnet *ifp)
{
	struct mobileip_softc *sc = ifp->if_softc;
	struct mbuf *m;

	while ((m = ifq_dequeue(&ifp->if_snd)) != NULL) {
#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap_af(ifp->if_bpf, AF_INET, m, BPF_DIRECTION_OUT);
#endif

		if (mobileip_encap(sc, m) != 0)
			ifp->if_oerrors++;
	}
}

static int
mobileip_encap(struct mobileip_softc *sc, struct mbuf *m)
{
	struct ip *ip;
	struct mobileip_header *mh;
	struct mobileip_h_src *msh;
	caddr_t hdr;
	int iphlen, hlen;

	/* look at the current IP header */
	m = m_pullup(m, sizeof(*ip));
	if (m == NULL)
		return (ENOBUFS);

	/* figure out how long it is */
	ip = mtod(m, struct ip *);
	iphlen = ip->ip_hl << 2;

	/* figure out how much extra space we'll need */
	hlen = sizeof(*mh);
	if (ip->ip_src.s_addr != sc->sc_src)
		hlen += sizeof(*msh);

	/* add the space */
	m = m_prepend(m, hlen, M_DONTWAIT);
	if (m == NULL)
		return (ENOBUFS);

	/* make the IP and mobileip headers contig */
	m = m_pullup(m, iphlen + hlen);
	if (m == NULL)
		return (ENOBUFS);

	/* move the IP header to the front */
	hdr = mtod(m, caddr_t);
	memmove(hdr, hdr + hlen, iphlen);

	/* fill in the headers */
	ip = (struct ip *)hdr;
	mh = (struct mobileip_header *)(hdr + iphlen);
	mh->mip_proto = ip->ip_p;
	mh->mip_flags = 0;
	mh->mip_hcrc = 0;
	mh->mip_dst = ip->ip_dst.s_addr;

	if (ip->ip_src.s_addr != sc->sc_src) {
		mh->mip_flags |= MOBILEIP_SP;

		msh = (struct mobileip_h_src *)(mh + 1);
		msh->mip_src = ip->ip_src.s_addr;

		ip->ip_src.s_addr = sc->sc_src;
	}

	htobem16(&mh->mip_hcrc, mobileip_cksum(mh, hlen));

	ip->ip_p = IPPROTO_MOBILE;
	htobem16(&ip->ip_len, bemtoh16(&ip->ip_len) + hlen);
	ip->ip_dst.s_addr = sc->sc_dst;

	m->m_flags &= ~(M_BCAST|M_MCAST);
	m->m_pkthdr.ph_rtableid = sc->sc_rtableid;

#if NPF > 0
	pf_pkt_addr_changed(m);
#endif

	ip_send(m);

	return (0);
}

int
mobileip_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct mobileip_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	int error = 0;

	switch(cmd) {
	case SIOCSIFADDR:
		/* XXX restrict to AF_INET */
		ifp->if_flags |= IFF_UP;
		/* FALLTHROUGH */
	case SIOCSIFFLAGS:
		if (ISSET(ifp->if_flags, IFF_UP)) {
			if (!ISSET(ifp->if_flags, IFF_RUNNING))
				error = mobileip_up(sc);
			else
				error = 0;
		} else {
			if (ISSET(ifp->if_flags, IFF_RUNNING))
				error = mobileip_down(sc);
		}
		break;
	case SIOCSIFDSTADDR:
		break;
	case SIOCSIFMTU:
		if (ifr->ifr_mtu < 576) {
			error = EINVAL;
			break;
		}
		ifp->if_mtu = ifr->ifr_mtu;
		break;
	case SIOCGIFMTU:
		ifr->ifr_mtu = sc->sc_if.if_mtu;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		break;

	case SIOCSLIFPHYADDR:
		error = mobileip_set_tunnel(sc, (struct if_laddrreq *)data);
		break;
	case SIOCGLIFPHYADDR:
		error = mobileip_get_tunnel(sc, (struct if_laddrreq *)data);
		break;
	case SIOCDIFPHYADDR:
		error = mobileip_del_tunnel(sc);
		break;

	case SIOCGLIFPHYTTL:
		ifr->ifr_ttl = -1;
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
		sc->sc_rtableid = ifr->ifr_rdomainid;
		break;
	case SIOCGLIFPHYRTABLE:
		ifr->ifr_rdomainid = sc->sc_rtableid;
		break;

	default:
		error = ENOTTY;
		break;
	}

	return (error);
}

static int
mobileip_up(struct mobileip_softc *sc)
{
	struct mobileip_softc *osc;

	if (sc->sc_dst == INADDR_ANY)
		return (EDESTADDRREQ);

	NET_ASSERT_LOCKED();
	osc = RBT_INSERT(mobileip_tree, &mobileip_softcs, sc);
	if (osc != NULL)
		return (EADDRINUSE);

	SET(sc->sc_if.if_flags, IFF_RUNNING);

	return (0);
}

static int
mobileip_down(struct mobileip_softc *sc)
{
	NET_ASSERT_LOCKED();
	RBT_REMOVE(mobileip_tree, &mobileip_softcs, sc);

	CLR(sc->sc_if.if_flags, IFF_RUNNING);

	ifq_barrier(&sc->sc_if.if_snd);

	return (0);
}

static int
mobileip_set_tunnel(struct mobileip_softc *sc, struct if_laddrreq *req)
{
	struct sockaddr_in *src = (struct sockaddr_in *)&req->addr;
	struct sockaddr_in *dst = (struct sockaddr_in *)&req->dstaddr;

	if (ISSET(sc->sc_if.if_flags, IFF_RUNNING))
		return (EBUSY);

	/* sa_family and sa_len must be equal */
	if (src->sin_family != dst->sin_family || src->sin_len != dst->sin_len)
		return (EINVAL);

	if (dst->sin_family != AF_INET)
		return (EAFNOSUPPORT);
	if (dst->sin_len != sizeof(*dst))
		return (EINVAL);

	if (in_nullhost(src->sin_addr) ||
	    IN_MULTICAST(src->sin_addr.s_addr) ||
	    in_nullhost(dst->sin_addr) ||
	    IN_MULTICAST(dst->sin_addr.s_addr))
		return (EINVAL);

	/* commit */
	sc->sc_src = src->sin_addr.s_addr;
	sc->sc_dst = dst->sin_addr.s_addr;

	return (0);
}

static int
mobileip_get_tunnel(struct mobileip_softc *sc, struct if_laddrreq *req)
{
	struct sockaddr_in *src = (struct sockaddr_in *)&req->addr;
	struct sockaddr_in *dst = (struct sockaddr_in *)&req->dstaddr;

	if (sc->sc_dst == INADDR_ANY)
		return (EADDRNOTAVAIL);

	memset(src, 0, sizeof(*src));
	src->sin_family = AF_INET;
	src->sin_len = sizeof(*src);
	src->sin_addr.s_addr = sc->sc_src;

	memset(dst, 0, sizeof(*dst));
	dst->sin_family = AF_INET;
	dst->sin_len = sizeof(*dst);
	dst->sin_addr.s_addr = sc->sc_dst;

	return (0);
}

static int
mobileip_del_tunnel(struct mobileip_softc *sc)
{
	if (ISSET(sc->sc_if.if_flags, IFF_RUNNING))
		return (EBUSY);

	/* commit */
	sc->sc_src = INADDR_ANY;
	sc->sc_dst = INADDR_ANY;

	return (0);
}

int
mobileip_input(struct mbuf **mp, int *offp, int type, int af)
{
	struct mobileip_softc key;
	struct mbuf *m = *mp;
	struct ifnet *ifp;
	struct mobileip_softc *sc;
	caddr_t hdr;
	struct ip *ip;
	struct mobileip_header *mh;
	struct mobileip_h_src *msh;
	int iphlen = 0;
	int hlen;

	if (!mobileip_allow)
		goto drop;

	ip = mtod(m, struct ip *);

	key.sc_rtableid = m->m_pkthdr.ph_rtableid;
	key.sc_src = ip->ip_dst.s_addr;
	key.sc_dst = ip->ip_src.s_addr;

	/* NET_ASSERT_READ_LOCKED() */
	sc = RBT_FIND(mobileip_tree, &mobileip_softcs, &key);
	if (sc == NULL)
		goto drop;

	/* it's ours now, we can do what we want */

	iphlen = ip->ip_hl << 2;
	hlen = sizeof(*mh);
	m = m_pullup(m, iphlen + hlen);
	if (m == NULL)
		return (IPPROTO_DONE);

	hdr = mtod(m, caddr_t);
	ip = (struct ip *)hdr;
	mh = (struct mobileip_header *)(hdr + iphlen);

	if (mh->mip_flags & ~MOBILEIP_SP)
		goto drop;

	if (ISSET(mh->mip_flags, MOBILEIP_SP)) {
		hlen += sizeof(*msh);
		m = m_pullup(m, iphlen + hlen);
		if (m == NULL)
			return (IPPROTO_DONE);

		hdr = mtod(m, caddr_t);
		ip = (struct ip *)hdr;
		mh = (struct mobileip_header *)(hdr + iphlen);
		msh = (struct mobileip_h_src *)(mh + 1);

		ip->ip_src.s_addr = msh->mip_src;
	}

	if (mobileip_cksum(mh, hlen) != 0)
		goto drop;

	ip->ip_p = mh->mip_proto;
	htobem16(&ip->ip_len, bemtoh16(&ip->ip_len) - hlen);
	ip->ip_dst.s_addr = mh->mip_dst;

	memmove(hdr + hlen, hdr, iphlen);
	m_adj(m, hlen);

	ifp = &sc->sc_if;

	CLR(m->m_flags, M_MCAST|M_BCAST);
	SET(m->m_pkthdr.csum_flags, M_IPV4_CSUM_IN_OK);
	m->m_pkthdr.ph_ifidx = ifp->if_index;
	m->m_pkthdr.ph_rtableid = ifp->if_rdomain;

#if NPF > 0
	pf_pkt_addr_changed(m);
#endif

	ifp->if_ipackets++;
	ifp->if_ibytes += m->m_pkthdr.len;

#if NBPFILTER > 0
	if (ifp->if_bpf)
		bpf_mtap_af(ifp->if_bpf, AF_INET, m, BPF_DIRECTION_IN);
#endif

	ipv4_input(ifp, m);

	return (IPPROTO_DONE);

drop:
	m_freem(m);
	return (IPPROTO_DONE);
}

#include <sys/sysctl.h>
#include <netinet/ip_gre.h>

int
mobileip_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp,
    void *newp, size_t newlen)
{
	int allow;
	int error;

	/* All sysctl names at this level are terminal. */
	if (namelen != 1)
		return (ENOTDIR);

	switch (name[0]) {
	case MOBILEIPCTL_ALLOW:
		allow = mobileip_allow;

		error = sysctl_int(oldp, oldlenp, newp, newlen,
		    &allow);
		if (error != 0)
			return (error);

		mobileip_allow = allow;
		break;
	default:
		return (ENOPROTOOPT);
	}

	return (0);
}

RBT_GENERATE(mobileip_tree, mobileip_softc, sc_entry, mobileip_cmp);
