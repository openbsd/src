/*	$OpenBSD: if_gif.c,v 1.45 2007/05/26 17:13:30 jason Exp $	*/
/*	$KAME: if_gif.c,v 1.43 2001/02/20 08:51:07 itojun Exp $	*/

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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/route.h>
#include <net/bpf.h>

#ifdef	INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/in_gif.h>
#include <netinet/ip.h>
#endif	/* INET */

#ifdef INET6
#ifndef INET
#include <netinet/in.h>
#endif
#include <netinet/ip6.h>
#include <netinet6/in6_gif.h>
#endif /* INET6 */

#include <net/if_gif.h>

#include "bpfilter.h"
#include "bridge.h"

void	gifattach(int);
int	gif_clone_create(struct if_clone *, int);
int	gif_clone_destroy(struct ifnet *);

/*
 * gif global variable definitions
 */
struct gif_softc_head gif_softc_list;
struct if_clone gif_cloner =
    IF_CLONE_INITIALIZER("gif", gif_clone_create, gif_clone_destroy);

/* ARGSUSED */
void
gifattach(count)
	int count;
{
	LIST_INIT(&gif_softc_list);
	if_clone_attach(&gif_cloner);
}

int
gif_clone_create(ifc, unit)
	struct if_clone *ifc;
	int unit;
{
	struct gif_softc *sc;
	int s;

	sc = malloc(sizeof(*sc), M_DEVBUF, M_NOWAIT);
	if (!sc)
		return (ENOMEM);
	bzero(sc, sizeof(*sc));

	snprintf(sc->gif_if.if_xname, sizeof sc->gif_if.if_xname,
	     "%s%d", ifc->ifc_name, unit);
	sc->gif_if.if_mtu    = GIF_MTU;
	sc->gif_if.if_flags  = IFF_POINTOPOINT | IFF_MULTICAST;
	sc->gif_if.if_ioctl  = gif_ioctl;
	sc->gif_if.if_start  = gif_start;
	sc->gif_if.if_output = gif_output;
	sc->gif_if.if_type   = IFT_GIF;
	IFQ_SET_MAXLEN(&sc->gif_if.if_snd, ifqmaxlen);
	IFQ_SET_READY(&sc->gif_if.if_snd);
	sc->gif_if.if_softc = sc;
	if_attach(&sc->gif_if);
	if_alloc_sadl(&sc->gif_if);

#if NBPFILTER > 0
	bpfattach(&sc->gif_if.if_bpf, &sc->gif_if, DLT_NULL,
	    sizeof(u_int));
#endif
	s = splnet();
	LIST_INSERT_HEAD(&gif_softc_list, sc, gif_list);
	splx(s);

	return (0);
}

int
gif_clone_destroy(ifp)
	struct ifnet *ifp;
{
	struct gif_softc *sc = ifp->if_softc;
	int s;

	s = splnet();
	LIST_REMOVE(sc, gif_list);
	splx(s);

	if_detach(ifp);

	if (sc->gif_psrc)
		free((caddr_t)sc->gif_psrc, M_IFADDR);
	sc->gif_psrc = NULL;
	if (sc->gif_pdst)
		free((caddr_t)sc->gif_pdst, M_IFADDR);
	sc->gif_pdst = NULL;
	free(sc, M_DEVBUF);
	return (0);
}

void
gif_start(ifp)
	struct ifnet *ifp;
{
	struct gif_softc *sc = (struct gif_softc*)ifp;
	struct mbuf *m;
	struct m_tag *mtag;
	int family;
	int s;
	u_int8_t tp;

	/* is interface up and running? */
	if ((ifp->if_flags & (IFF_OACTIVE | IFF_UP)) != IFF_UP ||
	    sc->gif_psrc == NULL || sc->gif_pdst == NULL)
		return;

	/* are the tunnel endpoints valid? */
#ifdef INET
	if (sc->gif_psrc->sa_family != AF_INET)
#endif
#ifdef INET6
		if (sc->gif_psrc->sa_family != AF_INET6)
#endif
			return;

	s = splnet();
	ifp->if_flags |= IFF_OACTIVE;
	splx(s);

	while (1) {
		s = splnet();
		IFQ_DEQUEUE(&ifp->if_snd, m);
		splx(s);

		if (m == NULL)
			break;

		/*
		 * gif may cause infinite recursion calls when misconfigured.
		 * We'll prevent this by detecting loops.
		 */
		for (mtag = m_tag_find(m, PACKET_TAG_GIF, NULL); mtag;
		    mtag = m_tag_find(m, PACKET_TAG_GIF, mtag)) {
			if (!bcmp((caddr_t)(mtag + 1), &ifp,
			    sizeof(struct ifnet *))) {
				IF_DROP(&ifp->if_snd);
				log(LOG_NOTICE, "gif_output: "
				    "recursively called too many times\n");
				m_freem(m);
				break;
			}
		}
		if (mtag)
			continue;

		mtag = m_tag_get(PACKET_TAG_GIF, sizeof(caddr_t), M_NOWAIT);
		if (mtag == NULL) {
			m_freem(m);
			break;
		}
		bcopy(&ifp, mtag + 1, sizeof(caddr_t));
		m_tag_prepend(m, mtag);

		/*
		 * remove multicast and broadcast flags or encapsulated paket
		 * ends up as multicast or broadcast packet.
		 */
		m->m_flags &= ~(M_BCAST|M_MCAST);

		/* extract address family */
		family = AF_UNSPEC;
		tp = *mtod(m, u_int8_t *);
		tp = (tp >> 4) & 0xff;  /* Get the IP version number. */
#ifdef INET
		if (tp == IPVERSION)
			family = AF_INET;
#endif
#ifdef INET6
		if (tp == (IPV6_VERSION >> 4))
			family = AF_INET6;
#endif

#if NBRIDGE > 0
		/*
		 * Check if the packet is comming via bridge and needs
		 * etherip encapsulation or not.
		 */
		if (ifp->if_bridge && (m->m_flags & M_PROTO1)) {
			m->m_flags &= ~M_PROTO1;
			family = AF_LINK;
		}
#endif

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap_af(ifp->if_bpf, family, m, BPF_DIRECTION_OUT);
#endif
		ifp->if_opackets++;
		ifp->if_obytes += m->m_pkthdr.len;

		switch (sc->gif_psrc->sa_family) {
#ifdef INET
		case AF_INET:
			in_gif_output(ifp, family, m);
			break;
#endif
#ifdef INET6
		case AF_INET6:
			in6_gif_output(ifp, family, m);
			break;
#endif
		default:
			m_freem(m);
			break;
		}
	}

	ifp->if_flags &= ~IFF_OACTIVE;
}

int
gif_output(ifp, m, dst, rt)
	struct ifnet *ifp;
	struct mbuf *m;
	struct sockaddr *dst;
	struct rtentry *rt;	/* added in net2 */
{
	struct gif_softc *sc = (struct gif_softc*)ifp;
	int error = 0;
	int s;

	if (!(ifp->if_flags & IFF_UP) ||
	    sc->gif_psrc == NULL || sc->gif_pdst == NULL) {
		m_freem(m);
		error = ENETDOWN;
		goto end;
	}

	switch (sc->gif_psrc->sa_family) {
#ifdef INET
	case AF_INET:
		break;
#endif
#ifdef INET6
	case AF_INET6:
		break;
#endif
	default:
		m_freem(m);
		error = ENETDOWN;
		goto end;
	}

	s = splnet();
	/*
	 * Queue message on interface, and start output if interface
	 * not yet active.
	 */
	IFQ_ENQUEUE(&ifp->if_snd, m, NULL, error);
	if (error) {
		/* mbuf is already freed */
		splx(s);
		return (error);
	}
	if ((ifp->if_flags & IFF_OACTIVE) == 0)
		(*ifp->if_start)(ifp);
	splx(s);
	return (error);

end:
	if (error)
		ifp->if_oerrors++;
	return (error);
}

int
gif_ioctl(ifp, cmd, data)
	struct ifnet *ifp;
	u_long cmd;
	caddr_t data;
{
	struct gif_softc *sc  = (struct gif_softc*)ifp;
	struct ifreq     *ifr = (struct ifreq*)data;
	int error = 0, size;
	struct sockaddr *dst, *src;
	struct sockaddr *sa;
	int s;
	struct gif_softc *sc2;

	switch (cmd) {
	case SIOCSIFADDR:
		break;

	case SIOCSIFDSTADDR:
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		switch (ifr->ifr_addr.sa_family) {
#ifdef INET
		case AF_INET:	/* IP supports Multicast */
			break;
#endif /* INET */
#ifdef INET6
		case AF_INET6:	/* IP6 supports Multicast */
			break;
#endif /* INET6 */
		default:  /* Other protocols doesn't support Multicast */
			error = EAFNOSUPPORT;
			break;
		}
		break;

	case SIOCSIFPHYADDR:
#ifdef INET6
	case SIOCSIFPHYADDR_IN6:
#endif /* INET6 */
	case SIOCSLIFPHYADDR:
		switch (cmd) {
#ifdef INET
		case SIOCSIFPHYADDR:
			src = (struct sockaddr *)
				&(((struct in_aliasreq *)data)->ifra_addr);
			dst = (struct sockaddr *)
				&(((struct in_aliasreq *)data)->ifra_dstaddr);
			break;
#endif
#ifdef INET6
		case SIOCSIFPHYADDR_IN6:
			src = (struct sockaddr *)
				&(((struct in6_aliasreq *)data)->ifra_addr);
			dst = (struct sockaddr *)
				&(((struct in6_aliasreq *)data)->ifra_dstaddr);
			break;
#endif
		case SIOCSLIFPHYADDR:
			src = (struct sockaddr *)
				&(((struct if_laddrreq *)data)->addr);
			dst = (struct sockaddr *)
				&(((struct if_laddrreq *)data)->dstaddr);
			break;
		default:
			return (EINVAL);
		}

		/* sa_family must be equal */
		if (src->sa_family != dst->sa_family)
			return (EINVAL);

		/* validate sa_len */
		switch (src->sa_family) {
#ifdef INET
		case AF_INET:
			if (src->sa_len != sizeof(struct sockaddr_in))
				return (EINVAL);
			break;
#endif
#ifdef INET6
		case AF_INET6:
			if (src->sa_len != sizeof(struct sockaddr_in6))
				return (EINVAL);
			break;
#endif
		default:
			return (EAFNOSUPPORT);
		}
		switch (dst->sa_family) {
#ifdef INET
		case AF_INET:
			if (dst->sa_len != sizeof(struct sockaddr_in))
				return (EINVAL);
			break;
#endif
#ifdef INET6
		case AF_INET6:
			if (dst->sa_len != sizeof(struct sockaddr_in6))
				return (EINVAL);
			break;
#endif
		default:
			return (EAFNOSUPPORT);
		}

		/* check sa_family looks sane for the cmd */
		switch (cmd) {
		case SIOCSIFPHYADDR:
			if (src->sa_family == AF_INET)
				break;
			return (EAFNOSUPPORT);
#ifdef INET6
		case SIOCSIFPHYADDR_IN6:
			if (src->sa_family == AF_INET6)
				break;
			return (EAFNOSUPPORT);
#endif /* INET6 */
		case SIOCSLIFPHYADDR:
			/* checks done in the above */
			break;
		}

		LIST_FOREACH(sc2, &gif_softc_list, gif_list) {
			if (sc2 == sc)
				continue;
			if (!sc2->gif_pdst || !sc2->gif_psrc)
				continue;
			if (sc2->gif_pdst->sa_family != dst->sa_family ||
			    sc2->gif_pdst->sa_len != dst->sa_len ||
			    sc2->gif_psrc->sa_family != src->sa_family ||
			    sc2->gif_psrc->sa_len != src->sa_len)
				continue;
			/* can't configure same pair of address onto two gifs */
			if (bcmp(sc2->gif_pdst, dst, dst->sa_len) == 0 &&
			    bcmp(sc2->gif_psrc, src, src->sa_len) == 0) {
				error = EADDRNOTAVAIL;
				goto bad;
			}

			/* can't configure multiple multi-dest interfaces */
#define multidest(x) \
	(((struct sockaddr_in *)(x))->sin_addr.s_addr == INADDR_ANY)
#ifdef INET6
#define multidest6(x) \
	(IN6_IS_ADDR_UNSPECIFIED(&((struct sockaddr_in6 *)(x))->sin6_addr))
#endif
			if (dst->sa_family == AF_INET &&
			    multidest(dst) && multidest(sc2->gif_pdst)) {
				error = EADDRNOTAVAIL;
				goto bad;
			}
#ifdef INET6
			if (dst->sa_family == AF_INET6 &&
			    multidest6(dst) && multidest6(sc2->gif_pdst)) {
				error = EADDRNOTAVAIL;
				goto bad;
			}
#endif
		}

		if (sc->gif_psrc)
			free((caddr_t)sc->gif_psrc, M_IFADDR);
		sa = (struct sockaddr *)malloc(src->sa_len, M_IFADDR, M_WAITOK);
		bcopy((caddr_t)src, (caddr_t)sa, src->sa_len);
		sc->gif_psrc = sa;

		if (sc->gif_pdst)
			free((caddr_t)sc->gif_pdst, M_IFADDR);
		sa = (struct sockaddr *)malloc(dst->sa_len, M_IFADDR, M_WAITOK);
		bcopy((caddr_t)dst, (caddr_t)sa, dst->sa_len);
		sc->gif_pdst = sa;

		s = splnet();
		ifp->if_flags |= IFF_RUNNING;
		if_up(ifp);		/* send up RTM_IFINFO */
		splx(s);

		error = 0;
		break;

#ifdef SIOCDIFPHYADDR
	case SIOCDIFPHYADDR:
		if (sc->gif_psrc) {
			free((caddr_t)sc->gif_psrc, M_IFADDR);
			sc->gif_psrc = NULL;
		}
		if (sc->gif_pdst) {
			free((caddr_t)sc->gif_pdst, M_IFADDR);
			sc->gif_pdst = NULL;
		}
		/* change the IFF_{UP, RUNNING} flag as well? */
		break;
#endif

	case SIOCGIFPSRCADDR:
#ifdef INET6
	case SIOCGIFPSRCADDR_IN6:
#endif /* INET6 */
		if (sc->gif_psrc == NULL) {
			error = EADDRNOTAVAIL;
			goto bad;
		}
		src = sc->gif_psrc;
		switch (cmd) {
#ifdef INET
		case SIOCGIFPSRCADDR:
			dst = &ifr->ifr_addr;
			size = sizeof(ifr->ifr_addr);
			break;
#endif /* INET */
#ifdef INET6
		case SIOCGIFPSRCADDR_IN6:
			dst = (struct sockaddr *)
				&(((struct in6_ifreq *)data)->ifr_addr);
			size = sizeof(((struct in6_ifreq *)data)->ifr_addr);
			break;
#endif /* INET6 */
		default:
			error = EADDRNOTAVAIL;
			goto bad;
		}
		if (src->sa_len > size)
			return (EINVAL);
		bcopy((caddr_t)src, (caddr_t)dst, src->sa_len);
		break;

	case SIOCGIFPDSTADDR:
#ifdef INET6
	case SIOCGIFPDSTADDR_IN6:
#endif /* INET6 */
		if (sc->gif_pdst == NULL) {
			error = EADDRNOTAVAIL;
			goto bad;
		}
		src = sc->gif_pdst;
		switch (cmd) {
#ifdef INET
		case SIOCGIFPDSTADDR:
			dst = &ifr->ifr_addr;
			size = sizeof(ifr->ifr_addr);
			break;
#endif /* INET */
#ifdef INET6
		case SIOCGIFPDSTADDR_IN6:
			dst = (struct sockaddr *)
				&(((struct in6_ifreq *)data)->ifr_addr);
			size = sizeof(((struct in6_ifreq *)data)->ifr_addr);
			break;
#endif /* INET6 */
		default:
			error = EADDRNOTAVAIL;
			goto bad;
		}
		if (src->sa_len > size)
			return (EINVAL);
		bcopy((caddr_t)src, (caddr_t)dst, src->sa_len);
		break;

	case SIOCGLIFPHYADDR:
		if (sc->gif_psrc == NULL || sc->gif_pdst == NULL) {
			error = EADDRNOTAVAIL;
			goto bad;
		}

		/* copy src */
		src = sc->gif_psrc;
		dst = (struct sockaddr *)
			&(((struct if_laddrreq *)data)->addr);
		size = sizeof(((struct if_laddrreq *)data)->addr);
		if (src->sa_len > size)
			return (EINVAL);
		bcopy((caddr_t)src, (caddr_t)dst, src->sa_len);

		/* copy dst */
		src = sc->gif_pdst;
		dst = (struct sockaddr *)
			&(((struct if_laddrreq *)data)->dstaddr);
		size = sizeof(((struct if_laddrreq *)data)->dstaddr);
		if (src->sa_len > size)
			return (EINVAL);
		bcopy((caddr_t)src, (caddr_t)dst, src->sa_len);
		break;

	case SIOCSIFFLAGS:
		/* if_ioctl() takes care of it */
		break;

	case SIOCSIFMTU:
		if (ifr->ifr_mtu < GIF_MTU_MIN || ifr->ifr_mtu > GIF_MTU_MAX)
			error = EINVAL;
		else
			ifp->if_mtu = ifr->ifr_mtu;
		break;

	default:
		error = EINVAL;
		break;
	}
 bad:
	return (error);
}
