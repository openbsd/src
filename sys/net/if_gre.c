/*      $OpenBSD: if_gre.c,v 1.42 2007/09/15 16:43:51 henning Exp $ */
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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

#include "gre.h"
#if NGRE > 0

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/kernel.h>
#include <sys/systm.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/netisr.h>
#include <net/route.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/if_ether.h>
#else
#error "if_gre used without inet"
#endif

#ifdef NETATALK
#include <netatalk/at.h>
#include <netatalk/at_var.h>
#include <netatalk/at_extern.h>
#endif

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <net/if_gre.h>

#ifndef GRE_RECURSION_LIMIT
#define GRE_RECURSION_LIMIT	3   /* How many levels of recursion allowed */
#endif /* GRE_RECURSION_LIMIT */

/*
 * It is not easy to calculate the right value for a GRE MTU.
 * We leave this task to the admin and use the same default that
 * other vendors use.
 */
#define GREMTU 1476

int	gre_clone_create(struct if_clone *, int);
int	gre_clone_destroy(struct ifnet *);

struct gre_softc_head gre_softc_list;
struct if_clone gre_cloner =
    IF_CLONE_INITIALIZER("gre", gre_clone_create, gre_clone_destroy);

/*
 * We can control the acceptance of GRE and MobileIP packets by
 * altering the sysctl net.inet.gre.allow and net.inet.mobileip.allow values
 * respectively. Zero means drop them, all else is acceptance.  We can also
 * control acceptance of WCCPv1-style GRE packets through the
 * net.inet.gre.wccp value, but be aware it depends upon normal GRE being
 * allowed as well.
 * 
 */
int gre_allow = 0;
int gre_wccp = 0;
int ip_mobile_allow = 0;

static void gre_compute_route(struct gre_softc *sc);

void
greattach(int n)
{
	LIST_INIT(&gre_softc_list);
	if_clone_attach(&gre_cloner);
}

int
gre_clone_create(struct if_clone *ifc, int unit)
{
	struct gre_softc *sc;
	int s;

	sc = malloc(sizeof(*sc), M_DEVBUF, M_NOWAIT|M_ZERO);
	if (!sc)
		return (ENOMEM);
	snprintf(sc->sc_if.if_xname, sizeof sc->sc_if.if_xname, "%s%d",
	    ifc->ifc_name, unit);
	sc->sc_if.if_softc = sc;
	sc->sc_if.if_type = IFT_TUNNEL;
	sc->sc_if.if_addrlen = 0;
	sc->sc_if.if_hdrlen = 24; /* IP + GRE */
	sc->sc_if.if_mtu = GREMTU;
	sc->sc_if.if_flags = IFF_POINTOPOINT|IFF_MULTICAST;
	sc->sc_if.if_output = gre_output;
	sc->sc_if.if_ioctl = gre_ioctl;
	sc->sc_if.if_collisions = 0;
	sc->sc_if.if_ierrors = 0;
	sc->sc_if.if_oerrors = 0;
	sc->sc_if.if_ipackets = 0;
	sc->sc_if.if_opackets = 0;
	sc->g_dst.s_addr = sc->g_src.s_addr = INADDR_ANY;
	sc->g_proto = IPPROTO_GRE;
	sc->sc_if.if_flags |= IFF_LINK0;

	if_attach(&sc->sc_if);
	if_alloc_sadl(&sc->sc_if);

#if NBPFILTER > 0
	bpfattach(&sc->sc_if.if_bpf, &sc->sc_if, DLT_NULL,
	    sizeof(u_int32_t));
#endif
	s = splnet();
	LIST_INSERT_HEAD(&gre_softc_list, sc, sc_list);
	splx(s);

	return (0);
}

int
gre_clone_destroy(struct ifnet *ifp)
{
	struct gre_softc *sc = ifp->if_softc;
	int s;

	s = splnet();
	LIST_REMOVE(sc, sc_list);
	splx(s);

	if_detach(ifp);

	free(sc, M_DEVBUF);
	return (0);
}

/*
 * The output routine. Takes a packet and encapsulates it in the protocol
 * given by sc->g_proto. See also RFC 1701 and RFC 2004.
 */

int
gre_output(struct ifnet *ifp, struct mbuf *m, struct sockaddr *dst,
	   struct rtentry *rt)
{
	int error = 0;
	struct gre_softc *sc = (struct gre_softc *) (ifp->if_softc);
	struct greip *gh = NULL;
	struct ip *inp = NULL;
	u_int8_t ip_tos = 0;
	u_int16_t etype = 0;
	struct mobile_h mob_h;
	struct m_tag *mtag;

	if ((ifp->if_flags & IFF_UP) == 0 ||
	    sc->g_src.s_addr == INADDR_ANY || sc->g_dst.s_addr == INADDR_ANY) {
		m_freem(m);
		error = ENETDOWN;
		goto end;
	}

	/* Try to limit infinite recursion through misconfiguration. */
	for (mtag = m_tag_find(m, PACKET_TAG_GRE, NULL); mtag;
	     mtag = m_tag_find(m, PACKET_TAG_GRE, mtag)) {
		if (!bcmp((caddr_t)(mtag + 1), &ifp, sizeof(struct ifnet *))) {
			IF_DROP(&ifp->if_snd);
			m_freem(m);
			error = EIO;
			goto end;
		}
	}

	mtag = m_tag_get(PACKET_TAG_GRE, sizeof(struct ifnet *), M_NOWAIT);
	if (mtag == NULL) {
		IF_DROP(&ifp->if_snd);
		m_freem(m);
		error = ENOBUFS;
		goto end;
	}
	bcopy(&ifp, (caddr_t)(mtag + 1), sizeof(struct ifnet *));
	m_tag_prepend(m, mtag);

	m->m_flags &= ~(M_BCAST|M_MCAST);

#if NBPFILTER >0
	if (ifp->if_bpf)
		bpf_mtap_af(ifp->if_bpf, dst->sa_family, m, BPF_DIRECTION_OUT);
#endif

	if (sc->g_proto == IPPROTO_MOBILE) {
		if (ip_mobile_allow == 0) {
			IF_DROP(&ifp->if_snd);
			m_freem(m);
			error = EACCES;
			goto end;
		}

		if (dst->sa_family == AF_INET) {
			struct mbuf *m0;
			int msiz;

			/*
			 * Make sure the complete IP header (with options)
			 * is in the first mbuf.
			 */
			if (m->m_len < sizeof(struct ip)) {
				m = m_pullup(m, sizeof(struct ip));
				if (m == NULL) {
					IF_DROP(&ifp->if_snd);
					error = ENOBUFS;
					goto end;
				} else
					inp = mtod(m, struct ip *);

				if (m->m_len < inp->ip_hl << 2) {
					m = m_pullup(m, inp->ip_hl << 2);
					if (m == NULL) {
						IF_DROP(&ifp->if_snd);
						error = ENOBUFS;
						goto end;
					}
				}
			}

			inp = mtod(m, struct ip *);

			bzero(&mob_h, MOB_H_SIZ_L);
			mob_h.proto = (inp->ip_p) << 8;
			mob_h.odst = inp->ip_dst.s_addr;
			inp->ip_dst.s_addr = sc->g_dst.s_addr;

			/*
			 * If the packet comes from our host, we only change
			 * the destination address in the IP header.
			 * Otherwise we need to save and change the source.
			 */
			if (inp->ip_src.s_addr == sc->g_src.s_addr) {
				msiz = MOB_H_SIZ_S;
			} else {
				mob_h.proto |= MOB_H_SBIT;
				mob_h.osrc = inp->ip_src.s_addr;
				inp->ip_src.s_addr = sc->g_src.s_addr;
				msiz = MOB_H_SIZ_L;
			}

			HTONS(mob_h.proto);
			mob_h.hcrc = gre_in_cksum((u_int16_t *) &mob_h, msiz);

			/* Squeeze in the mobility header */
			if ((m->m_data - msiz) < m->m_pktdat) {
				/* Need new mbuf */
				MGETHDR(m0, M_DONTWAIT, MT_HEADER);
				if (m0 == NULL) {
					IF_DROP(&ifp->if_snd);
					m_freem(m);
					error = ENOBUFS;
					goto end;
				}
				M_MOVE_HDR(m0, m);

				m0->m_len = msiz + (inp->ip_hl << 2);
				m0->m_data += max_linkhdr;
				m0->m_pkthdr.len = m->m_pkthdr.len + msiz;
				m->m_data += inp->ip_hl << 2;
				m->m_len -= inp->ip_hl << 2;

				bcopy((caddr_t) inp, mtod(m0, caddr_t),
				    sizeof(struct ip));

				m0->m_next = m;
				m = m0;
			} else {  /* we have some space left in the old one */
				m->m_data -= msiz;
				m->m_len += msiz;
				m->m_pkthdr.len += msiz;
				bcopy(inp, mtod(m, caddr_t),
				    inp->ip_hl << 2);
			}

			/* Copy Mobility header */
			inp = mtod(m, struct ip *);
			bcopy(&mob_h, (caddr_t)(inp + 1), (unsigned) msiz);
			inp->ip_len = htons(ntohs(inp->ip_len) + msiz);
		} else {  /* AF_INET */
			IF_DROP(&ifp->if_snd);
			m_freem(m);
			error = EINVAL;
			goto end;
		}
	} else if (sc->g_proto == IPPROTO_GRE) {
		if (gre_allow == 0) {
			IF_DROP(&ifp->if_snd);
			m_freem(m);
			error = EACCES;
			goto end;
		}

		switch(dst->sa_family) {
		case AF_INET:
			if (m->m_len < sizeof(struct ip)) {
				m = m_pullup(m, sizeof(struct ip));
				if (m == NULL) {
					IF_DROP(&ifp->if_snd);
					error = ENOBUFS;
					goto end;
				}
			}

			inp = mtod(m, struct ip *);
			ip_tos = inp->ip_tos;
			etype = ETHERTYPE_IP;
			break;
#ifdef NETATALK
		case AF_APPLETALK:
			etype = ETHERTYPE_AT;
			break;
#endif
#ifdef INET6
		case AF_INET6:
			etype = ETHERTYPE_IPV6;
			break;
#endif
		default:
			IF_DROP(&ifp->if_snd);
			m_freem(m);
			error = EAFNOSUPPORT;
			goto end;
		}

		M_PREPEND(m, sizeof(struct greip), M_DONTWAIT);
	} else {
		IF_DROP(&ifp->if_snd);
		m_freem(m);
		error = EINVAL;
		goto end;
	}

	if (m == NULL) {
		IF_DROP(&ifp->if_snd);
		error = ENOBUFS;
		goto end;
	}

	gh = mtod(m, struct greip *);
	if (sc->g_proto == IPPROTO_GRE) {
		/* We don't support any GRE flags for now */

		bzero((void *) &gh->gi_g, sizeof(struct gre_h));
		gh->gi_ptype = htons(etype);
	}

	gh->gi_pr = sc->g_proto;
	if (sc->g_proto != IPPROTO_MOBILE) {
		gh->gi_src = sc->g_src;
		gh->gi_dst = sc->g_dst;
		((struct ip *) gh)->ip_hl = (sizeof(struct ip)) >> 2;
		((struct ip *) gh)->ip_ttl = ip_defttl;
		((struct ip *) gh)->ip_tos = ip_tos;
		gh->gi_len = htons(m->m_pkthdr.len);
	}

	ifp->if_opackets++;
	ifp->if_obytes += m->m_pkthdr.len;

	/* Send it off */
	error = ip_output(m, (void *)NULL, &sc->route, 0, (void *)NULL, (void *)NULL);
  end:
	if (error)
		ifp->if_oerrors++;
	return (error);
}

int
gre_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{

	struct ifreq *ifr = (struct ifreq *) data;
	struct if_laddrreq *lifr = (struct if_laddrreq *)data;
	struct gre_softc *sc = ifp->if_softc;
	int s;
	struct sockaddr_in si;
	struct sockaddr *sa = NULL;
	int error = 0;
	struct proc *prc = curproc;		/* XXX */

	s = splnet();
	switch(cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		break;
	case SIOCSIFDSTADDR:
		break;
	case SIOCSIFFLAGS:
		if ((ifr->ifr_flags & IFF_LINK0) != 0)
			sc->g_proto = IPPROTO_GRE;
		else
			sc->g_proto = IPPROTO_MOBILE;
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
		if (ifr == 0) {
			error = EAFNOSUPPORT;
			break;
		}
		switch (ifr->ifr_addr.sa_family) {
#ifdef INET
		case AF_INET:
			break;
#endif
#ifdef INET6
		case AF_INET6:
			break;
#endif
		default:
			error = EAFNOSUPPORT;
			break;
		}
		break;
	case GRESPROTO:
		/* Check for superuser */
		if ((error = suser(prc, 0)) != 0)
			break;

		sc->g_proto = ifr->ifr_flags;
		switch (sc->g_proto) {
		case IPPROTO_GRE:
			ifp->if_flags |= IFF_LINK0;
			break;
		case IPPROTO_MOBILE:
			ifp->if_flags &= ~IFF_LINK0;
			break;
		default:
			error = EPROTONOSUPPORT;
			break;
		}
		break;
	case GREGPROTO:
		ifr->ifr_flags = sc->g_proto;
		break;
	case GRESADDRS:
	case GRESADDRD:
		/* Check for superuser */
		if ((error = suser(prc, 0)) != 0)
			break;

		/*
		 * set tunnel endpoints, compute a less specific route
		 * to the remote end and mark if as up
		 */
		sa = &ifr->ifr_addr;
		if (cmd == GRESADDRS )
			sc->g_src = (satosin(sa))->sin_addr;
		if (cmd == GRESADDRD )
			sc->g_dst = (satosin(sa))->sin_addr;
	recompute:
		if ((sc->g_src.s_addr != INADDR_ANY) &&
		    (sc->g_dst.s_addr != INADDR_ANY)) {
			if (sc->route.ro_rt != 0) {
				/* free old route */
				RTFREE(sc->route.ro_rt);
				sc->route.ro_rt = (struct rtentry *) 0;
			}
			gre_compute_route(sc);
			ifp->if_flags |= IFF_UP;
		}
		break;
	case GREGADDRS:
		bzero(&si, sizeof(si));
		si.sin_family = AF_INET;
		si.sin_len = sizeof(struct sockaddr_in);
		si.sin_addr.s_addr = sc->g_src.s_addr;
		sa = sintosa(&si);
		ifr->ifr_addr = *sa;
		break;
	case GREGADDRD:
		bzero(&si, sizeof(si));
		si.sin_family = AF_INET;
		si.sin_len = sizeof(struct sockaddr_in);
		si.sin_addr.s_addr = sc->g_dst.s_addr;
		sa = sintosa(&si);
		ifr->ifr_addr = *sa;
		break;
	case SIOCSLIFPHYADDR:
		if ((error = suser(prc, 0)) != 0)
			break;
		if (lifr->addr.ss_family != AF_INET ||
		    lifr->dstaddr.ss_family != AF_INET) {
			error = EAFNOSUPPORT;
			break;
		}
		if (lifr->addr.ss_len != sizeof(si) ||
		    lifr->dstaddr.ss_len != sizeof(si)) {
			error = EINVAL;
			break;
		}
		sc->g_src = (satosin((struct sockadrr *)&lifr->addr))->sin_addr;
		sc->g_dst =
		    (satosin((struct sockadrr *)&lifr->dstaddr))->sin_addr;
		goto recompute;
	case SIOCDIFPHYADDR:
		if ((error = suser(prc, 0)) != 0)
			break;
		sc->g_src.s_addr = INADDR_ANY;
		sc->g_dst.s_addr = INADDR_ANY;
		break;
	case SIOCGLIFPHYADDR:
		if (sc->g_src.s_addr == INADDR_ANY ||
		    sc->g_dst.s_addr == INADDR_ANY) {
			error = EADDRNOTAVAIL;
			break;
		}
		bzero(&si, sizeof(si));
		si.sin_family = AF_INET;
		si.sin_len = sizeof(struct sockaddr_in);
		si.sin_addr.s_addr = sc->g_src.s_addr;
		memcpy(&lifr->addr, &si, sizeof(si));
		si.sin_addr.s_addr = sc->g_dst.s_addr;
		memcpy(&lifr->dstaddr, &si, sizeof(si));
		break;
	default:
		error = EINVAL;
	}

	splx(s);
	return (error);
}

/*
 * computes a route to our destination that is not the one
 * which would be taken by ip_output(), as this one will loop back to
 * us. If the interface is p2p as  a--->b, then a routing entry exists
 * If we now send a packet to b (e.g. ping b), this will come down here
 * gets src=a, dst=b tacked on and would from ip_output() sent back to
 * if_gre.
 * Goal here is to compute a route to b that is less specific than
 * a-->b. We know that this one exists as in normal operation we have
 * at least a default route which matches.
 */

static void
gre_compute_route(struct gre_softc *sc)
{
	struct route *ro;
	u_int32_t a, b, c;

	ro = &sc->route;

	bzero(ro, sizeof(struct route));
	((struct sockaddr_in *) &ro->ro_dst)->sin_addr = sc->g_dst;
	ro->ro_dst.sa_family = AF_INET;
	ro->ro_dst.sa_len = sizeof(ro->ro_dst);

	/*
	 * toggle last bit, so our interface is not found, but a less
	 * specific route. I'd rather like to specify a shorter mask,
 	 * but this is not possible. Should work though. XXX
	 * there is a simpler way ...
	 */
	if ((sc->sc_if.if_flags & IFF_LINK1) == 0) {
		a = ntohl(sc->g_dst.s_addr);
		b = a & 0x01;
		c = a & 0xfffffffe;
		b = b ^ 0x01;
		a = b | c;
		((struct sockaddr_in *) &ro->ro_dst)->sin_addr.s_addr = htonl(a);
	}

	rtalloc(ro);
	if (ro->ro_rt == 0)
		return;

	/*
	 * Check whether we just created a loop. An even more paranoid
	 * check would be against all GRE interfaces, but that would
	 * not allow people to link GRE tunnels.
	 */
	if (ro->ro_rt->rt_ifp == &sc->sc_if) {
		RTFREE(ro->ro_rt);
		ro->ro_rt = (struct rtentry *) 0;
		return;
	}

	/*
	 * now change it back - else ip_output will just drop
	 * the route and search one to this interface ...
	 */
	if ((sc->sc_if.if_flags & IFF_LINK1) == 0)
		((struct sockaddr_in *) &ro->ro_dst)->sin_addr = sc->g_dst;
}

/*
 * do a checksum of a buffer - much like in_cksum, which operates on
 * mbufs.
 */
u_int16_t
gre_in_cksum(u_int16_t *p, u_int len)
{
	u_int32_t sum = 0;
	int nwords = len >> 1;

	while (nwords-- != 0)
		sum += *p++;

		if (len & 1) {
			union {
				u_short w;
				u_char c[2];
			} u;
			u.c[0] = *(u_char *) p;
			u.c[1] = 0;
			sum += u.w;
		}

		/* end-around-carry */
		sum = (sum >> 16) + (sum & 0xffff);
		sum += (sum >> 16);
		return (~sum);
}
#endif
