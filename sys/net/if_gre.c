/*      $OpenBSD: if_gre.c,v 1.3 2000/01/07 23:25:21 angelos Exp $ */
/*	$NetBSD: if_gre.c,v 1.9 1999/10/25 19:18:11 drochner Exp $ */

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Heiko W.Rupp <hwr@pilhuhn.de>
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

#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
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

#define GREMTU 1450	/* XXX this is below the standard MTU of
                         1500 Bytes, allowing for headers, 
                         but we should possibly do path mtu discovery
                         before changing if state to up to find the 
                         correct value */

#define LINK_MASK (IFF_LINK0|IFF_LINK1|IFF_LINK2)

struct gre_softc gre_softc[NGRE];

/*
 * We can control the acceptance of GRE and MobileIP packets by
 * altering the sysctl net.inet.gre.allow and net.inet.mobileip.allow values
 * respectively.  Zero means drop them, all else is acceptance.
 */
int gre_allow = 0;
int ip_mobile_allow = 0;

static void gre_compute_route(struct gre_softc *sc);

void
greattach(void)
{
	struct gre_softc *sc;
	int i;

	for (i = 0, sc = gre_softc ; i < NGRE ; sc++ ) {
		snprintf(sc->sc_if.if_xname, sizeof(sc->sc_if.if_xname),
			 "gre%d", i++);
		sc->sc_if.if_softc = sc;
		sc->sc_if.if_type =  IFT_OTHER;
		sc->sc_if.if_addrlen = 4;
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

		if_attach(&sc->sc_if);

#if NBPFILTER > 0
		bpfattach(&sc->sc_if.if_bpf, &sc->sc_if, DLT_RAW,
			  sizeof(u_int32_t) );
#endif
	}
}

/* 
 * The output routine. Takes a packet and encapsulates it in the protocol
 * given by sc->g_proto. See also RFC 1701 and RFC 2004
 */

int
gre_output(struct ifnet *ifp, struct mbuf *m, struct sockaddr *dst,
	   struct rtentry *rt)
{
	int error = 0;
	struct gre_softc *sc = (struct gre_softc *) (ifp->if_softc);
	struct greip *gh = NULL;
	struct ip *inp = NULL;
	u_char ttl = 255;
	u_short etype = 0;
	struct mobile_h mob_h;

#if NBPFILTER >0
	if (ifp->if_bpf) {
                /*
                 * We need to prepend the address family as
                 * a four byte field.  Cons up a fake header
                 * to pacify bpf.  This is safe because bpf
                 * will only read from the mbuf (i.e., it won't
                 * try to free it or keep a pointer a to it).
                 */
		struct mbuf m0;
		u_int af = dst->sa_family;

		m0.m_next = m;
		m0.m_len = 4;
		m0.m_data = (char *) &af;
		
		bpf_mtap(ifp->if_bpf, &m0);
	}
#endif

	if (sc->g_proto == IPPROTO_MOBILE) {
	        if (ip_mobile_allow == 0) {
		        IF_DROP(&ifp->if_snd);
			m_freem(m);
			return (EACCES);
		}

		if (dst->sa_family == AF_INET) {
			struct mbuf *m0;
			int msiz;

			/*
			 * Make sure the complete IP header (with options)
			 * is in the first mbuf.
			 */
			if (m->m_len < sizeof(struct ip))
			{
			        m = m_pullup(m, sizeof(struct ip));
			        if (m == 0) {
					IF_DROP(&ifp->if_snd);
					return (ENOBUFS);
				}
				else
				        inp = mtod(m, struct ip *);

				if (m->m_len < inp->ip_hl << 2) {
				        m = m_pullup(m,
						     sizeof(inp->ip_hl << 2));
					if (m == 0) {
					        IF_DROP(&ifp->if_snd);
						return (ENOBUFS);
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
			mob_h.hcrc = gre_in_cksum((u_short *) &mob_h, msiz);

			/* Squeeze in the mobility header */
			if ((m->m_data - msiz) < m->m_pktdat) {
				/* Need new mbuf */
				MGETHDR(m0, M_DONTWAIT, MT_HEADER);
				if (m0 == NULL) {
					IF_DROP(&ifp->if_snd);
					m_freem(m);
					return (ENOBUFS);
				}

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
			NTOHS(inp->ip_len);
			inp->ip_len += msiz;
		} else {  /* AF_INET */
			IF_DROP(&ifp->if_snd);
			m_freem(m);
			return (EINVAL);
		}
	} else if (sc->g_proto == IPPROTO_GRE) {
	        if (gre_allow == 0) {
		        IF_DROP(&ifp->if_snd);
			m_freem(m);
			return (EACCES);
		}

		switch(dst->sa_family) {
		case AF_INET:
		        if (m->m_len < sizeof(struct ip)) {
			        m = m_pullup(m, sizeof(struct ip));
				if (m == 0) {
				        IF_DROP(&ifp->if_snd);
					return (ENOBUFS);
				}
			}

			inp = mtod(m, struct ip *);
			ttl = inp->ip_ttl;
			etype = ETHERTYPE_IP;
			break;
#ifdef NETATALK
		case AF_APPLETALK:
			etype = ETHERTYPE_ATALK;
			break;
#endif
#ifdef NS
		case AF_NS:
			etype = ETHERTYPE_NS;
			break;
#endif
		default:
			IF_DROP(&ifp->if_snd);
			m_freem(m);
			return (EAFNOSUPPORT);
		}

		M_PREPEND(m, sizeof(struct greip), M_DONTWAIT);
	} else {
		error = EINVAL;
		IF_DROP(&ifp->if_snd);
		m_freem(m);
		return (error);
	}
			
	if (m == NULL) {
		IF_DROP(&ifp->if_snd);
		return (ENOBUFS);
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
		((struct ip *) gh)->ip_ttl = ttl;
		((struct ip *) gh)->ip_tos = inp->ip_tos;
		gh->gi_len = m->m_pkthdr.len;
	}

	ifp->if_opackets++;
	ifp->if_obytes += m->m_pkthdr.len;

	/* Send it off */
	error = ip_output(m, NULL, &sc->route, 0, NULL, NULL);
	if (error)
		ifp->if_oerrors++;
	return (error);
}

int
gre_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{

	struct ifaddr *ifa = (struct ifaddr *) data;
	struct ifreq *ifr = (struct ifreq *) data;
	struct in_ifaddr *ia = (struct in_ifaddr *) data;
	struct gre_softc *sc = ifp->if_softc;
	int s;
	struct sockaddr_in si;
	struct sockaddr *sa = NULL;
	int error = 0;
	struct proc *prc = curproc;             /* XXX */

	s = splimp();
	switch(cmd) {
	case SIOCSIFADDR:		
	case SIOCSIFDSTADDR: 	
		/* 
                 * set tunnel endpoints in case that we "only"
                 * have ip over ip encapsulation. This allows to
                 * set tunnel endpoints with ifconfig.
                 */
		if (ifa->ifa_addr->sa_family == AF_INET) {
			sa = ifa->ifa_addr;
			sc->g_src = (satosin(sa))->sin_addr;
			sc->g_dst = ia->ia_dstaddr.sin_addr;
			if ((sc->g_src.s_addr != INADDR_ANY) &&
			    (sc->g_dst.s_addr != INADDR_ANY)) {
				if (sc->route.ro_rt != 0) /* free old route */
					RTFREE(sc->route.ro_rt);
				gre_compute_route(sc);
				ifp->if_flags |= IFF_UP;
			}
		}
		break;
	case SIOCSIFFLAGS:
		if ((sc->g_dst.s_addr == INADDR_ANY) || 
		    (sc->g_src.s_addr == INADDR_ANY))
			ifp->if_flags &= ~IFF_UP;

		switch(ifr->ifr_flags & LINK_MASK) {
			case IFF_LINK0:
				sc->g_proto = IPPROTO_GRE;
				ifp->if_flags |= IFF_LINK0;
				ifp->if_flags &= ~(IFF_LINK1|IFF_LINK2);
				break;
			case IFF_LINK2:
				sc->g_proto = IPPROTO_MOBILE;
				ifp->if_flags |= IFF_LINK2;
				ifp->if_flags &= ~(IFF_LINK0|IFF_LINK1);
				break;
		}
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
		default:
			error = EAFNOSUPPORT;
			break;
		}
		break;
	case GRESPROTO:
	        /* Check for superuser */
	        if ((error = suser(prc->p_ucred, &prc->p_acflag)) != 0)
		        break;

		sc->g_proto = ifr->ifr_flags;
		switch (sc->g_proto) {
		case IPPROTO_GRE :
			ifp->if_flags |= IFF_LINK0;
			ifp->if_flags &= ~(IFF_LINK1|IFF_LINK2);
			break;
		case IPPROTO_MOBILE :
			ifp->if_flags |= IFF_LINK2;
			ifp->if_flags &= ~(IFF_LINK1|IFF_LINK2);
			break;
		default:
			ifp->if_flags &= ~(IFF_LINK0|IFF_LINK1|IFF_LINK2);
		}
		break;
	case GREGPROTO:
		ifr->ifr_flags = sc->g_proto;
		break;
	case GRESADDRS:
	case GRESADDRD:
	        /* Check for superuser */
	        if ((error = suser(prc->p_ucred, &prc->p_acflag)) != 0)
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
		if ((sc->g_src.s_addr != INADDR_ANY) &&
		    (sc->g_dst.s_addr != INADDR_ANY)) {
			if (sc->route.ro_rt != 0) /* free old route */
				RTFREE(sc->route.ro_rt);
			gre_compute_route(sc);
			ifp->if_flags |= IFF_UP;
		}
		break;
	case GREGADDRS:
		si.sin_addr.s_addr = sc->g_src.s_addr;
		sa = sintosa(&si);
		ifr->ifr_addr = *sa;
		break;
	case GREGADDRD:
		si.sin_addr.s_addr = sc->g_dst.s_addr;
		sa = sintosa(&si);
		ifr->ifr_addr = *sa;
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
 * gets src=a, dst=b tacked on and would from ip_ouput() sent back to 
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
		((struct sockaddr_in *) &ro->ro_dst)->sin_addr.s_addr
			= htonl(a);
	}

	rtalloc(ro);

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

u_short
gre_in_cksum(u_short *p, u_int len)
{
	u_int sum = 0; 
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
