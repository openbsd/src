/* $OpenBSD: if_mpe.c,v 1.64 2018/01/09 15:24:24 bluhm Exp $ */

/*
 * Copyright (c) 2008 Pierre-Yves Ritschard <pyr@spootnik.org>
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
#include "mpe.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/ioctl.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_var.h>
#include <net/if_types.h>
#include <net/netisr.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/ip.h>

#ifdef INET6
#include <netinet/ip6.h>
#endif /* INET6 */

#include "bpfilter.h"
#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <netmpls/mpls.h>

#ifdef MPLS_DEBUG
#define DPRINTF(x)    do { if (mpedebug) printf x ; } while (0)
#else
#define DPRINTF(x)
#endif

void	mpeattach(int);
int	mpeoutput(struct ifnet *, struct mbuf *, struct sockaddr *,
		       struct rtentry *);
int	mpeioctl(struct ifnet *, u_long, caddr_t);
void	mpestart(struct ifnet *);
int	mpe_clone_create(struct if_clone *, int);
int	mpe_clone_destroy(struct ifnet *);

LIST_HEAD(, mpe_softc)	mpeif_list;
struct if_clone	mpe_cloner =
    IF_CLONE_INITIALIZER("mpe", mpe_clone_create, mpe_clone_destroy);

extern int	mpls_mapttl_ip;
#ifdef INET6
extern int	mpls_mapttl_ip6;
#endif

void
mpeattach(int nmpe)
{
	LIST_INIT(&mpeif_list);
	if_clone_attach(&mpe_cloner);
}

int
mpe_clone_create(struct if_clone *ifc, int unit)
{
	struct ifnet		*ifp;
	struct mpe_softc	*mpeif;

	mpeif = malloc(sizeof(*mpeif), M_DEVBUF, M_WAITOK|M_ZERO);
	mpeif->sc_unit = unit;
	ifp = &mpeif->sc_if;
	snprintf(ifp->if_xname, sizeof ifp->if_xname, "mpe%d", unit);
	ifp->if_flags = IFF_POINTOPOINT;
	ifp->if_xflags = IFXF_CLONED;
	ifp->if_softc = mpeif;
	ifp->if_mtu = MPE_MTU;
	ifp->if_ioctl = mpeioctl;
	ifp->if_output = mpeoutput;
	ifp->if_start = mpestart;
	ifp->if_type = IFT_MPLS;
	ifp->if_hdrlen = MPE_HDRLEN;
	IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);
	if_attach(ifp);
	if_alloc_sadl(ifp);
#if NBPFILTER > 0
	bpfattach(&ifp->if_bpf, ifp, DLT_LOOP, sizeof(u_int32_t));
#endif

	mpeif->sc_ifa.ifa_ifp = ifp;
	mpeif->sc_ifa.ifa_addr = sdltosa(ifp->if_sadl);
	mpeif->sc_smpls.smpls_len = sizeof(mpeif->sc_smpls);
	mpeif->sc_smpls.smpls_family = AF_MPLS;

	LIST_INSERT_HEAD(&mpeif_list, mpeif, sc_list);

	return (0);
}

int
mpe_clone_destroy(struct ifnet *ifp)
{
	struct mpe_softc	*mpeif = ifp->if_softc;

	LIST_REMOVE(mpeif, sc_list);

	if (mpeif->sc_smpls.smpls_label) {
		rt_ifa_del(&mpeif->sc_ifa, RTF_MPLS,
		    smplstosa(&mpeif->sc_smpls));
	}

	if_detach(ifp);
	free(mpeif, M_DEVBUF, sizeof *mpeif);
	return (0);
}

struct sockaddr_storage	 mpedst;
/*
 * Start output on the mpe interface.
 */
void
mpestart(struct ifnet *ifp0)
{
	struct mbuf		*m;
	struct sockaddr		*sa = sstosa(&mpedst);
	sa_family_t		 af;
	struct rtentry		*rt;
	struct ifnet		*ifp;

	for (;;) {
		IFQ_DEQUEUE(&ifp0->if_snd, m);
		if (m == NULL)
			return;

		af = *mtod(m, sa_family_t *);
		m_adj(m, sizeof(af));
		switch (af) {
		case AF_INET:
			bzero(sa, sizeof(struct sockaddr_in));
			satosin(sa)->sin_family = af;
			satosin(sa)->sin_len = sizeof(struct sockaddr_in);
			bcopy(mtod(m, caddr_t), &satosin(sa)->sin_addr,
			    sizeof(in_addr_t));
			m_adj(m, sizeof(in_addr_t));
			break;
		default:
			m_freem(m);
			continue;
		}

		rt = rtalloc(sa, RT_RESOLVE, 0);
		if (!rtisvalid(rt)) {
			m_freem(m);
			rtfree(rt);
			continue;
		}

		ifp = if_get(rt->rt_ifidx);
		if (ifp == NULL) {
			m_freem(m);
			rtfree(rt);
			continue;
		}
#if NBPFILTER > 0
		if (ifp0->if_bpf) {
			/* remove MPLS label before passing packet to bpf */
			m->m_data += sizeof(struct shim_hdr);
			m->m_len -= sizeof(struct shim_hdr);
			m->m_pkthdr.len -= sizeof(struct shim_hdr);
			bpf_mtap_af(ifp0->if_bpf, af, m, BPF_DIRECTION_OUT);
			m->m_data -= sizeof(struct shim_hdr);
			m->m_len += sizeof(struct shim_hdr);
			m->m_pkthdr.len += sizeof(struct shim_hdr);
		}
#endif
		/* XXX lie, but mpls_output looks only at sa_family */
		sa->sa_family = AF_MPLS;

		mpls_output(ifp, m, sa, rt);
		if_put(ifp);
		rtfree(rt);
	}
}

int
mpeoutput(struct ifnet *ifp, struct mbuf *m, struct sockaddr *dst,
	struct rtentry *rt)
{
	struct shim_hdr	shim;
	int		error;
	int		off;
	in_addr_t	addr;
	u_int8_t	op = 0;

#ifdef DIAGNOSTIC
	if (ifp->if_rdomain != rtable_l2(m->m_pkthdr.ph_rtableid)) {
		printf("%s: trying to send packet on wrong domain. "
		    "if %d vs. mbuf %d\n", ifp->if_xname,
		    ifp->if_rdomain, rtable_l2(m->m_pkthdr.ph_rtableid));
	}
#endif
	m->m_pkthdr.ph_ifidx = ifp->if_index;
	/* XXX assumes MPLS is always in rdomain 0 */
	m->m_pkthdr.ph_rtableid = 0;

	error = 0;
	switch (dst->sa_family) {
	case AF_INET:
		if (!rt || !(rt->rt_flags & RTF_MPLS)) {
			m_freem(m);
			error = ENETUNREACH;
			goto out;
		}
		shim.shim_label =
		    ((struct rt_mpls *)rt->rt_llinfo)->mpls_label;
		shim.shim_label |= MPLS_BOS_MASK;
		op =  ((struct rt_mpls *)rt->rt_llinfo)->mpls_operation;
		if (op != MPLS_OP_PUSH) {
			m_freem(m);
			error = ENETUNREACH;
			goto out;
		}
		if (mpls_mapttl_ip) {
			struct ip	*ip;
			ip = mtod(m, struct ip *);
			shim.shim_label |= htonl(ip->ip_ttl) & MPLS_TTL_MASK;
		} else
			shim.shim_label |= htonl(mpls_defttl) & MPLS_TTL_MASK;
		off = sizeof(sa_family_t) + sizeof(in_addr_t);
		M_PREPEND(m, sizeof(shim) + off, M_DONTWAIT);
		if (m == NULL) {
			error = ENOBUFS;
			goto out;
		}
		*mtod(m, sa_family_t *) = AF_INET;
		addr = satosin(rt->rt_gateway)->sin_addr.s_addr;
		m_copyback(m, sizeof(sa_family_t), sizeof(in_addr_t),
		    &addr, M_NOWAIT);
		break;
	default:
		m_freem(m);
		error = EPFNOSUPPORT;
		goto out;
	}

	m_copyback(m, off, sizeof(shim), (caddr_t)&shim, M_NOWAIT);

	error = if_enqueue(ifp, m);
out:
	if (error)
		ifp->if_oerrors++;
	return (error);
}

int
mpeioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct mpe_softc	*ifm;
	struct ifreq		*ifr;
	struct shim_hdr		 shim;
	int			 error = 0;

	ifr = (struct ifreq *)data;
	switch (cmd) {
	case SIOCSIFADDR:
		if (!ISSET(ifp->if_flags, IFF_UP))
			if_up(ifp);
		break;
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP)
			ifp->if_flags |= IFF_RUNNING;
		else
			ifp->if_flags &= ~IFF_RUNNING;
		break;
	case SIOCSIFMTU:
		if (ifr->ifr_mtu < MPE_MTU_MIN ||
		    ifr->ifr_mtu > MPE_MTU_MAX)
			error = EINVAL;
		else
			ifp->if_mtu = ifr->ifr_mtu;
		break;
	case SIOCGETLABEL:
		ifm = ifp->if_softc;
		shim.shim_label =
		    ((ntohl(ifm->sc_smpls.smpls_label & MPLS_LABEL_MASK)) >>
		    MPLS_LABEL_OFFSET);
		error = copyout(&shim, ifr->ifr_data, sizeof(shim));
		break;
	case SIOCSETLABEL:
		ifm = ifp->if_softc;
		if ((error = copyin(ifr->ifr_data, &shim, sizeof(shim))))
			break;
		if (shim.shim_label > MPLS_LABEL_MAX ||
		    shim.shim_label <= MPLS_LABEL_RESERVED_MAX) {
			error = EINVAL;
			break;
		}
		shim.shim_label = htonl(shim.shim_label << MPLS_LABEL_OFFSET);
		if (ifm->sc_smpls.smpls_label == shim.shim_label)
			break;
		LIST_FOREACH(ifm, &mpeif_list, sc_list) {
			if (ifm != ifp->if_softc &&
			    ifm->sc_smpls.smpls_label == shim.shim_label) {
				error = EEXIST;
				break;
			}
		}
		if (error)
			break;
		/*
		 * force interface up for now,
		 * linkstate of MPLS route is not tracked
		 */
		if (!ISSET(ifp->if_flags, IFF_UP))
			if_up(ifp);
		ifm = ifp->if_softc;
		if (ifm->sc_smpls.smpls_label) {
			/* remove old MPLS route */
			rt_ifa_del(&ifm->sc_ifa, RTF_MPLS,
			    smplstosa(&ifm->sc_smpls));
		}
		/* add new MPLS route */
		ifm->sc_smpls.smpls_label = shim.shim_label;
		error = rt_ifa_add(&ifm->sc_ifa, RTF_MPLS,
		    smplstosa(&ifm->sc_smpls));
		if (error) {
			ifm->sc_smpls.smpls_label = 0;
			break;
		}
		break;
	case SIOCSIFRDOMAIN:
		/* must readd the MPLS "route" for our label */
		/* XXX does not make sense, the MPLS route is on rtable 0 */
		ifm = ifp->if_softc;
		if (ifr->ifr_rdomainid != ifp->if_rdomain) {
			if (ifm->sc_smpls.smpls_label) {
				rt_ifa_add(&ifm->sc_ifa, RTF_MPLS,
				    smplstosa(&ifm->sc_smpls));
			}
		}
		/* return with ENOTTY so that the parent handler finishes */
		return (ENOTTY);
	default:
		return (ENOTTY);
	}

	return (error);
}

void
mpe_input(struct mbuf *m, struct ifnet *ifp, struct sockaddr_mpls *smpls,
    u_int8_t ttl)
{
	struct ip	*ip;
	int		 hlen;

	/* label -> AF lookup */

	if (mpls_mapttl_ip) {
		if (m->m_len < sizeof (struct ip) &&
		    (m = m_pullup(m, sizeof(struct ip))) == NULL)
			return;
		ip = mtod(m, struct ip *);
		hlen = ip->ip_hl << 2;
		if (m->m_len < hlen) {
			if ((m = m_pullup(m, hlen)) == NULL)
				return;
			ip = mtod(m, struct ip *);
		}

		if (in_cksum(m, hlen) != 0) {
			m_freem(m);
			return;
		}

		/* set IP ttl from MPLS ttl */
		ip->ip_ttl = ttl;

		/* recalculate checksum */
		ip->ip_sum = 0;
		ip->ip_sum = in_cksum(m, hlen);
	}

	/* new receive if and move into correct rtable */
	m->m_pkthdr.ph_ifidx = ifp->if_index;
	m->m_pkthdr.ph_rtableid = ifp->if_rdomain;

#if NBPFILTER > 0
	if (ifp->if_bpf)
		bpf_mtap_af(ifp->if_bpf, AF_INET, m, BPF_DIRECTION_IN);
#endif

	ipv4_input(ifp, m);
}

#ifdef INET6
void
mpe_input6(struct mbuf *m, struct ifnet *ifp, struct sockaddr_mpls *smpls,
    u_int8_t ttl)
{
	struct ip6_hdr *ip6hdr;

	/* label -> AF lookup */

	if (mpls_mapttl_ip6) {
		if (m->m_len < sizeof (struct ip6_hdr) &&
		    (m = m_pullup(m, sizeof(struct ip6_hdr))) == NULL)
			return;

		ip6hdr = mtod(m, struct ip6_hdr *);

		/* set IPv6 ttl from MPLS ttl */
		ip6hdr->ip6_hlim = ttl;
	}

	/* new receive if and move into correct rtable */
	m->m_pkthdr.ph_ifidx = ifp->if_index;
	m->m_pkthdr.ph_rtableid = ifp->if_rdomain;

#if NBPFILTER > 0
	if (ifp->if_bpf)
		bpf_mtap_af(ifp->if_bpf, AF_INET6, m, BPF_DIRECTION_IN);
#endif

	ipv6_input(ifp, m);
}
#endif	/* INET6 */
