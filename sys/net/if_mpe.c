/* $OpenBSD: if_mpe.c,v 1.17 2009/12/26 12:34:41 claudio Exp $ */

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
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/ioctl.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/netisr.h>
#include <net/route.h>

#ifdef	INET
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#endif

#ifdef INET6
#include <netinet/ip6.h>
#ifndef INET
#include <netinet/in.h>
#endif
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
extern int	mpls_mapttl_ip6;

void
mpeattach(int nmpe)
{
	LIST_INIT(&mpeif_list);
	if_clone_attach(&mpe_cloner);
}

int
mpe_clone_create(struct if_clone *ifc, int unit)
{
	struct ifnet 		*ifp;
	struct mpe_softc	*mpeif;
	int 			 s;

	if ((mpeif = malloc(sizeof(*mpeif),
	    M_DEVBUF, M_NOWAIT|M_ZERO)) == NULL)
		return (ENOMEM);

	mpeif->sc_shim.shim_label = MPLS_BOS_MASK | htonl(mpls_defttl);
	mpeif->sc_unit = unit;
	ifp = &mpeif->sc_if;
	snprintf(ifp->if_xname, sizeof ifp->if_xname, "mpe%d", unit);
	ifp->if_flags = IFF_POINTOPOINT;
	ifp->if_softc = mpeif;
	ifp->if_mtu = MPE_MTU;
	ifp->if_ioctl = mpeioctl;
	ifp->if_output = mpeoutput;
	ifp->if_start = mpestart;
	ifp->if_type = IFT_MPLS;
	ifp->if_hdrlen = MPE_HDRLEN;
	IFQ_SET_MAXLEN(&ifp->if_snd, ifqmaxlen);
	IFQ_SET_READY(&ifp->if_snd);
	if_attach(ifp);
	if_alloc_sadl(ifp);
#if NBPFILTER > 0
	bpfattach(&ifp->if_bpf, ifp, DLT_LOOP, MPE_HDRLEN);
#endif

	s = splnet();
	LIST_INSERT_HEAD(&mpeif_list, mpeif, sc_list);
	splx(s);

	return (0);
}

int
mpe_clone_destroy(struct ifnet *ifp)
{
	struct mpe_softc	*mpeif = ifp->if_softc;
	int			 s;

	s = splnet();
	LIST_REMOVE(mpeif, sc_list);
	splx(s);

#if NBPFILTER > 0
	bpfdetach(ifp);
#endif
	if_detach(ifp);
	free(mpeif, M_DEVBUF);
	return (0);
}

/*
 * Start output on the mpe interface.
 */
void
mpestart(struct ifnet *ifp)
{
	struct mbuf 		*m;
	struct mpe_softc	*ifm;
	struct shim_hdr		 shim;
	int			 s;

	for (;;) {
		s = splnet();
		IFQ_DEQUEUE(&ifp->if_snd, m);
		splx(s);

		if (m == NULL)
			return;

#ifdef DIAGNOSTIC
		if (ifp->if_rdomain != rtable_l2(m->m_pkthdr.rdomain)) {
			printf("%s: trying to send packet on wrong domain. "
			    "if %d vs. mbuf %d\n", ifp->if_xname,
			    ifp->if_rdomain, rtable_l2(m->m_pkthdr.rdomain));
		}
#endif

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap_af(ifp->if_bpf, AF_INET, m, BPF_DIRECTION_OUT);
#endif
		ifm = ifp->if_softc;
		shim.shim_label = ifm->sc_shim.shim_label;
		M_PREPEND(m, sizeof(shim), M_DONTWAIT);
		m_copyback(m, 0, sizeof(shim), (caddr_t)&shim);
		if (m == NULL) {
			ifp->if_ierrors++;
			continue;
		}
		m->m_pkthdr.rcvif = ifp;
		/* XXX assumes MPLS is always in rdomain 0 */
		m->m_pkthdr.rdomain = 0;
		mpls_output(m, NULL);
	}
}

int
mpeoutput(struct ifnet *ifp, struct mbuf *m, struct sockaddr *dst,
	struct rtentry *rt)
{
	int	s;
	int	error;

	error = 0;
	switch (dst->sa_family) {
	case AF_INET:
		break;
	case AF_MPLS:
		/*
		 * drop MPLS packets entering here. This is a hack to prevent
		 * loops because of misconfiguration.
		 */
		m_freem(m);
		error = ENETUNREACH;
		return (error);
	default:
		error = ENETDOWN;
		goto out;
	}
	s = splnet();
	IFQ_ENQUEUE(&ifp->if_snd, m, NULL, error);
	if (error) {
		/* mbuf is already freed */
		splx(s);
		return (error);
	}
	if_start(ifp);
	splx(s);
out:
	if (error)
		ifp->if_oerrors++;
	return (error);
}

/* ARGSUSED */
int
mpeioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	int			 error;
	struct mpe_softc	*ifm;
	struct ifreq		*ifr;
	struct shim_hdr		 shim;
	u_int32_t		 ttl = htonl(mpls_defttl);

	ifr = (struct ifreq *)data;
	error = 0;
	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
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
		    ((ntohl(ifm->sc_shim.shim_label & MPLS_LABEL_MASK)) >>
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
		shim.shim_label = (htonl(shim.shim_label << MPLS_LABEL_OFFSET))
		    | MPLS_BOS_MASK | ttl;
		if (ifm->sc_shim.shim_label == shim.shim_label)
			break;
		LIST_FOREACH(ifm, &mpeif_list, sc_list) {
			if (ifm != ifp->if_softc &&
			    ifm->sc_shim.shim_label == shim.shim_label) {
				error = EEXIST;
				break;
			}
		}
		if (error)
			break;
		ifm = ifp->if_softc;
		ifm->sc_shim.shim_label = shim.shim_label;
		break;
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
	int		 s, hlen;

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
			m_free(m);
			return;
		}

		/* set IP ttl from MPLS ttl */
		ip->ip_ttl = ttl;

		/* recalculate checksum */
		ip->ip_sum = 0;
		ip->ip_sum = in_cksum(m, hlen);
	}
	
	/* new receive if and move into correct rdomain */
	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.rdomain = ifp->if_rdomain;

#if NBPFILTER > 0
	if (ifp && ifp->if_bpf)
		bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_OUT);
#endif
	s = splnet();
	IF_ENQUEUE(&ipintrq, m);
	schednetisr(NETISR_IP);
	splx(s);
}

void
mpe_input6(struct mbuf *m, struct ifnet *ifp, struct sockaddr_mpls *smpls,
    u_int8_t ttl)
{
	struct ip6_hdr *ip6hdr;
	int s;

	/* label -> AF lookup */

	if (mpls_mapttl_ip6) {
		if (m->m_len < sizeof (struct ip6_hdr) &&
		    (m = m_pullup(m, sizeof(struct ip6_hdr))) == NULL)
			return;

		ip6hdr = mtod(m, struct ip6_hdr *);

		/* set IPv6 ttl from MPLS ttl */
		ip6hdr->ip6_hlim = ttl;
	}

	/* new receive if and move into correct rdomain */
	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.rdomain = ifp->if_rdomain;

#if NBPFILTER > 0
	if (ifp && ifp->if_bpf)
		bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_OUT);
#endif
	s = splnet();
	IF_ENQUEUE(&ip6intrq, m);
	schednetisr(NETISR_IPV6);
	splx(s);
}
