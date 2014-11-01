/*	$OpenBSD: if_ethersubr.c,v 1.176 2014/11/01 21:40:38 mpi Exp $	*/
/*	$NetBSD: if_ethersubr.c,v 1.19 1996/05/07 02:40:30 thorpej Exp $	*/

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
 * Copyright (c) 1982, 1989, 1993
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
 *	@(#)if_ethersubr.c	8.1 (Berkeley) 6/10/93
 */

/*
%%% portions-copyright-nrl-95
Portions of this software are Copyright 1995-1998 by Randall Atkinson,
Ronald Lee, Daniel McDonald, Bao Phan, and Chris Winters. All Rights
Reserved. All rights under this copyright have been assigned to the US
Naval Research Laboratory (NRL). The NRL Copyright Notice and License
Agreement Version 1.1 (January 17, 1995) applies to these portions of the
software.
You should have received a copy of the license with this software. If you
didn't get a copy, you may request one from <license@ipv6.nrl.navy.mil>.
*/

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/timeout.h>


#include <net/if.h>
#include <net/netisr.h>
#include <net/route.h>
#include <net/if_llc.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/ip_ipsp.h>

#include <dev/rndvar.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include "bridge.h"
#if NBRIDGE > 0
#include <net/if_bridge.h>
#endif

#include "vlan.h"
#if NVLAN > 0
#include <net/if_vlan_var.h>
#endif /* NVLAN > 0 */

#include "carp.h"
#if NCARP > 0
#include <netinet/ip_carp.h>
#endif

#include "pppoe.h"
#if NPPPOE > 0
#include <net/if_pppoe.h>
#endif

#include "trunk.h"
#if NTRUNK > 0
#include <net/if_trunk.h>
#endif

#ifdef INET6
#ifndef INET
#include <netinet/in.h>
#endif
#include <netinet6/in6_var.h>
#include <netinet6/nd6.h>
#endif

#ifdef PIPEX
#include <net/pipex.h>
#endif

#ifdef MPLS
#include <netmpls/mpls.h>
#endif /* MPLS */

u_char etherbroadcastaddr[ETHER_ADDR_LEN] =
    { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
#define senderr(e) { error = (e); goto bad;}

static inline int	ether_addheader(struct mbuf **, struct ifnet *,
			   u_int16_t, u_char *, u_char *);

int
ether_ioctl(struct ifnet *ifp, struct arpcom *arp, u_long cmd, caddr_t data)
{
	struct ifreq *ifr = (struct ifreq *)data;
	int error = 0;

	switch (cmd) {
	case SIOCSIFADDR:
		break;

	case SIOCSIFMTU:
		if (ifr->ifr_mtu < ETHERMIN || ifr->ifr_mtu > ifp->if_hardmtu)
			error = EINVAL;
		else
			ifp->if_mtu = ifr->ifr_mtu;
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (ifp->if_flags & IFF_MULTICAST) {
			error = (cmd == SIOCADDMULTI) ?
			    ether_addmulti(ifr, arp) :
			    ether_delmulti(ifr, arp);
		} else
			error = ENOTTY;
		break;

	default:
		error = ENOTTY;
	}

	return (error);
}

static inline int
ether_addheader(struct mbuf **m, struct ifnet *ifp, u_int16_t etype,
    u_char *esrc, u_char *edst)
{
	struct ether_header *eh;

#if NVLAN > 0
	if ((*m)->m_flags & M_VLANTAG) {
		struct ifvlan	*ifv = ifp->if_softc;
		struct ifnet	*p = ifv->ifv_p;

		/* should we use the tx tagging hw offload at all? */
		if ((p->if_capabilities & IFCAP_VLAN_HWTAGGING) &&
		    (ifv->ifv_type == ETHERTYPE_VLAN)) {
			(*m)->m_pkthdr.ether_vtag = ifv->ifv_tag +
			    ((*m)->m_pkthdr.pf.prio << EVL_PRIO_BITS);
			/* don't return, need to add regular ethernet header */
		} else {
			struct ether_vlan_header	*evh;

			M_PREPEND(*m, sizeof(*evh), M_DONTWAIT);
			if (*m == NULL)
				return (-1);
			evh = mtod(*m, struct ether_vlan_header *);
			memcpy(evh->evl_dhost, edst, sizeof(evh->evl_dhost));
			memcpy(evh->evl_shost, esrc, sizeof(evh->evl_shost));
			evh->evl_proto = etype;
			evh->evl_encap_proto = htons(ifv->ifv_type);
			evh->evl_tag = htons(ifv->ifv_tag +
			    ((*m)->m_pkthdr.pf.prio << EVL_PRIO_BITS));
			(*m)->m_flags &= ~M_VLANTAG;
			return (0);
		}
	}
#endif /* NVLAN > 0 */
	M_PREPEND(*m, ETHER_HDR_LEN, M_DONTWAIT);
	if (*m == NULL)
		return (-1);
	eh = mtod(*m, struct ether_header *);
	eh->ether_type = etype;
	memcpy(eh->ether_dhost, edst, sizeof(eh->ether_dhost));
	memcpy(eh->ether_shost, esrc, sizeof(eh->ether_shost));
	return (0);
}

/*
 * Ethernet output routine.
 * Encapsulate a packet of type family for the local net.
 * Assumes that ifp is actually pointer to arpcom structure.
 */
int
ether_output(struct ifnet *ifp0, struct mbuf *m0, struct sockaddr *dst,
    struct rtentry *rt0)
{
	u_int16_t etype;
	int s, len, error = 0;
	u_char edst[ETHER_ADDR_LEN];
	u_char *esrc;
	struct mbuf *m = m0;
	struct rtentry *rt;
	struct mbuf *mcopy = NULL;
	struct ether_header *eh;
	struct arpcom *ac = (struct arpcom *)ifp0;
	short mflags;
	struct ifnet *ifp = ifp0;

#ifdef DIAGNOSTIC
	if (ifp->if_rdomain != rtable_l2(m->m_pkthdr.ph_rtableid)) {
		printf("%s: trying to send packet on wrong domain. "
		    "if %d vs. mbuf %d, AF %d\n", ifp->if_xname,
		    ifp->if_rdomain, rtable_l2(m->m_pkthdr.ph_rtableid),
		    dst->sa_family);
	}
#endif

#if NTRUNK > 0
	/* restrict transmission on trunk members to bpf only */
	if (ifp->if_type == IFT_IEEE8023ADLAG &&
	    (m_tag_find(m, PACKET_TAG_DLT, NULL) == NULL))
		senderr(EBUSY);
#endif

#if NCARP > 0
	if (ifp->if_type == IFT_CARP) {
		ifp = ifp->if_carpdev;
		ac = (struct arpcom *)ifp;

		if ((ifp0->if_flags & (IFF_UP|IFF_RUNNING)) !=
		    (IFF_UP|IFF_RUNNING))
			senderr(ENETDOWN);
	}
#endif /* NCARP > 0 */

	if ((ifp->if_flags & (IFF_UP|IFF_RUNNING)) != (IFF_UP|IFF_RUNNING))
		senderr(ENETDOWN);
	if ((rt = rt0) != NULL) {
		if ((rt->rt_flags & RTF_UP) == 0) {
			if ((rt0 = rt = rtalloc(dst, RT_REPORT|RT_RESOLVE,
			    m->m_pkthdr.ph_rtableid)) != NULL)
				rt->rt_refcnt--;
			else
				senderr(EHOSTUNREACH);
		}

		if (rt->rt_flags & RTF_GATEWAY) {
			if (rt->rt_gwroute == NULL)
				goto lookup;
			if (((rt = rt->rt_gwroute)->rt_flags & RTF_UP) == 0) {
				rtfree(rt);
				rt = rt0;
			lookup:
				rt->rt_gwroute = rtalloc(rt->rt_gateway,
				    RT_REPORT|RT_RESOLVE, ifp->if_rdomain);
				if ((rt = rt->rt_gwroute) == NULL)
					senderr(EHOSTUNREACH);
			}
		}
		if (rt->rt_flags & RTF_REJECT)
			if (rt->rt_rmx.rmx_expire == 0 ||
			    time_second < rt->rt_rmx.rmx_expire)
				senderr(rt == rt0 ? EHOSTDOWN : EHOSTUNREACH);
	}
	esrc = ac->ac_enaddr;
	switch (dst->sa_family) {

#ifdef INET
	case AF_INET:
		if (!arpresolve(ac, rt, m, dst, edst))
			return (0);	/* if not yet resolved */
		/* If broadcasting on a simplex interface, loopback a copy */
		if ((m->m_flags & M_BCAST) && (ifp->if_flags & IFF_SIMPLEX) &&
		    !m->m_pkthdr.pf.routed)
			mcopy = m_copy(m, 0, (int)M_COPYALL);
		etype = htons(ETHERTYPE_IP);
		break;
#endif
#ifdef INET6
	case AF_INET6:
		if (!nd6_storelladdr(ifp, rt, m, dst, (u_char *)edst))
			return (0); /* it must be impossible, but... */
		etype = htons(ETHERTYPE_IPV6);
		break;
#endif
#ifdef MPLS
       case AF_MPLS:
		if (rt)
			dst = rt_key(rt);
		else
			senderr(EHOSTUNREACH);

		if (!ISSET(ifp->if_xflags, IFXF_MPLS))
			senderr(ENETUNREACH);

		switch (dst->sa_family) {
			case AF_LINK:
				if (((struct sockaddr_dl *)dst)->sdl_alen <
				    sizeof(edst))
					senderr(EHOSTUNREACH);
				memcpy(edst, LLADDR((struct sockaddr_dl *)dst),
				    sizeof(edst));
				break;
			case AF_INET:
				if (!arpresolve(ac, rt, m, dst, edst))
					return (0); /* if not yet resolved */
				break;
			default:
				senderr(EHOSTUNREACH);
		}
		/* XXX handling for simplex devices in case of M/BCAST ?? */
		if (m->m_flags & (M_BCAST | M_MCAST))
			etype = htons(ETHERTYPE_MPLS_MCAST);
		else
			etype = htons(ETHERTYPE_MPLS);
		break;
#endif /* MPLS */
	case pseudo_AF_HDRCMPLT:
		eh = (struct ether_header *)dst->sa_data;
		esrc = eh->ether_shost;
		/* FALLTHROUGH */

	case AF_UNSPEC:
		eh = (struct ether_header *)dst->sa_data;
		memcpy(edst, eh->ether_dhost, sizeof(edst));
		/* AF_UNSPEC doesn't swap the byte order of the ether_type. */
		etype = eh->ether_type;
		break;

	default:
		printf("%s: can't handle af%d\n", ifp->if_xname,
			dst->sa_family);
		senderr(EAFNOSUPPORT);
	}

	/* XXX Should we feed-back an unencrypted IPsec packet ? */
	if (mcopy)
		(void) looutput(ifp, mcopy, dst, rt);

#if NCARP > 0
	if (ifp0 != ifp && ifp0->if_type == IFT_CARP)
		esrc = carp_get_srclladdr(ifp0, esrc);
#endif

	if (ether_addheader(&m, ifp, etype, esrc, edst) == -1)
		senderr(ENOBUFS);

#if NBRIDGE > 0
	/*
	 * Interfaces that are bridgeports need special handling for output.
	 */
	if (ifp->if_bridgeport) {
		struct m_tag *mtag;

		/*
		 * Check if this packet has already been sent out through
		 * this bridgeport, in which case we simply send it out
		 * without further bridge processing.
		 */
		for (mtag = m_tag_find(m, PACKET_TAG_BRIDGE, NULL); mtag;
		    mtag = m_tag_find(m, PACKET_TAG_BRIDGE, mtag)) {
#ifdef DEBUG
			/* Check that the information is there */
			if (mtag->m_tag_len != sizeof(caddr_t)) {
				error = EINVAL;
				goto bad;
			}
#endif
			if (!memcmp(&ifp->if_bridgeport, mtag + 1,
			    sizeof(caddr_t)))
				break;
		}
		if (mtag == NULL) {
			/* Attach a tag so we can detect loops */
			mtag = m_tag_get(PACKET_TAG_BRIDGE, sizeof(caddr_t),
			    M_NOWAIT);
			if (mtag == NULL) {
				error = ENOBUFS;
				goto bad;
			}
			memcpy(mtag + 1, &ifp->if_bridgeport, sizeof(caddr_t));
			m_tag_prepend(m, mtag);
			error = bridge_output(ifp, m, NULL, NULL);
			return (error);
		}
	}
#endif
	mflags = m->m_flags;
	len = m->m_pkthdr.len;
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
	ifp->if_obytes += len;
#if NCARP > 0
	if (ifp != ifp0)
		ifp0->if_obytes += len;
#endif /* NCARP > 0 */
	if (mflags & M_MCAST)
		ifp->if_omcasts++;
	if_start(ifp);
	splx(s);
	return (error);

bad:
	if (m)
		m_freem(m);
	return (error);
}

/*
 * Process a received Ethernet packet;
 * the packet is in the mbuf chain m without
 * the ether header, which is provided separately.
 */
void
ether_input(struct ifnet *ifp0, struct ether_header *eh, struct mbuf *m)
{
	struct ifqueue *inq;
	u_int16_t etype;
	int s, llcfound = 0;
	struct llc *l;
	struct arpcom *ac;
	struct ifnet *ifp = ifp0;
#if NTRUNK > 0
	int i = 0;
#endif
#if NPPPOE > 0
	struct ether_header *eh_tmp;
#endif

	/* mark incoming routing table */
	m->m_pkthdr.ph_rtableid = ifp->if_rdomain;

	if (eh == NULL) {
		eh = mtod(m, struct ether_header *);
		m_adj(m, ETHER_HDR_LEN);
	}

#if NTRUNK > 0
	/* Handle input from a trunk port */
	while (ifp->if_type == IFT_IEEE8023ADLAG) {
		if (++i > TRUNK_MAX_STACKING) {
			m_freem(m);
			return;
		}
		if (trunk_input(ifp, eh, m) != 0)
			return;

		/* Has been set to the trunk interface */
		ifp = m->m_pkthdr.rcvif;
	}
#endif

	if ((ifp->if_flags & IFF_UP) == 0) {
		m_freem(m);
		return;
	}
	if (ETHER_IS_MULTICAST(eh->ether_dhost)) {
		/*
		 * If this is not a simplex interface, drop the packet
		 * if it came from us.
		 */
		if ((ifp->if_flags & IFF_SIMPLEX) == 0) {
			if (memcmp(LLADDR(ifp->if_sadl), eh->ether_shost,
			    ETHER_ADDR_LEN) == 0) {
				m_freem(m);
				return;
			}
		}

		if (memcmp(etherbroadcastaddr, eh->ether_dhost,
		    sizeof(etherbroadcastaddr)) == 0)
			m->m_flags |= M_BCAST;
		else
			m->m_flags |= M_MCAST;
		ifp->if_imcasts++;
#if NTRUNK > 0
		if (ifp != ifp0)
			ifp0->if_imcasts++;
#endif
	}

	ifp->if_ibytes += m->m_pkthdr.len + sizeof(*eh);
#if NTRUNK > 0
	if (ifp != ifp0)
		ifp0->if_ibytes += m->m_pkthdr.len + sizeof(*eh);
#endif

	etype = ntohs(eh->ether_type);

	if (!(netisr & (1 << NETISR_RND_DONE))) {
		add_net_randomness(etype);
		atomic_setbits_int(&netisr, (1 << NETISR_RND_DONE));
	}

#if NVLAN > 0
	if (((m->m_flags & M_VLANTAG) || etype == ETHERTYPE_VLAN ||
	    etype == ETHERTYPE_QINQ) && (vlan_input(eh, m) == 0))
		return;
#endif

#if NBRIDGE > 0
	/*
	 * Tap the packet off here for a bridge, if configured and
	 * active for this interface.  bridge_input returns
	 * NULL if it has consumed the packet, otherwise, it
	 * gets processed as normal.
	 */
	if (ifp->if_bridgeport) {
		if (m->m_flags & M_PROTO1)
			m->m_flags &= ~M_PROTO1;
		else {
			m = bridge_input(ifp, eh, m);
			if (m == NULL)
				return;
			/* The bridge has determined it's for us. */
			ifp = m->m_pkthdr.rcvif;
		}
	}
#endif

#if NVLAN > 0
	if ((m->m_flags & M_VLANTAG) || etype == ETHERTYPE_VLAN ||
	    etype == ETHERTYPE_QINQ) {
		/* The bridge did not want the vlan frame either, drop it. */
		ifp->if_noproto++;
		m_freem(m);
		return;
	}
#endif /* NVLAN > 0 */

#if NCARP > 0
	if (ifp->if_carp) {
		if (ifp->if_type != IFT_CARP && (carp_input(ifp, eh, m) == 0))
			return;
		/* clear mcast if received on a carp IP balanced address */
		else if (ifp->if_type == IFT_CARP &&
		    m->m_flags & (M_BCAST|M_MCAST) &&
		    carp_our_mcastaddr(ifp, (u_int8_t *)&eh->ether_dhost))
			m->m_flags &= ~(M_BCAST|M_MCAST);
	}
#endif /* NCARP > 0 */

	ac = (struct arpcom *)ifp;

	/*
	 * If packet has been filtered by the bpf listener, drop it now
	 */
	if (m->m_flags & M_FILDROP) {
		m_freem(m);
		return;
	}

	/*
	 * If packet is unicast and we're in promiscuous mode, make sure it
	 * is for us.  Drop otherwise.
	 */
	if ((m->m_flags & (M_BCAST|M_MCAST)) == 0 &&
	    ((ifp->if_flags & IFF_PROMISC) || (ifp0->if_flags & IFF_PROMISC))) {
		if (memcmp(ac->ac_enaddr, eh->ether_dhost, ETHER_ADDR_LEN)) {
			m_freem(m);
			return;
		}
	}

	/*
	 * Schedule softnet interrupt and enqueue packet within the same spl.
	 */
	s = splnet();
decapsulate:

	switch (etype) {
#ifdef INET
	case ETHERTYPE_IP:
		schednetisr(NETISR_IP);
		inq = &ipintrq;
		break;

	case ETHERTYPE_ARP:
		if (ifp->if_flags & IFF_NOARP)
			goto dropanyway;
		schednetisr(NETISR_ARP);
		inq = &arpintrq;
		break;

	case ETHERTYPE_REVARP:
		if (ifp->if_flags & IFF_NOARP)
			goto dropanyway;
		revarpinput(m);	/* XXX queue? */
		goto done;

#endif
#ifdef INET6
	/*
	 * Schedule IPv6 software interrupt for incoming IPv6 packet.
	 */
	case ETHERTYPE_IPV6:
		schednetisr(NETISR_IPV6);
		inq = &ip6intrq;
		break;
#endif /* INET6 */
#if NPPPOE > 0 || defined(PIPEX)
	case ETHERTYPE_PPPOEDISC:
	case ETHERTYPE_PPPOE:
#ifndef PPPOE_SERVER
		if (m->m_flags & (M_MCAST | M_BCAST)) {
			m_freem(m);
			goto done;
		}
#endif
		M_PREPEND(m, sizeof(*eh), M_DONTWAIT);
		if (m == NULL)
			goto done;

		eh_tmp = mtod(m, struct ether_header *);
		memcpy(eh_tmp, eh, sizeof(struct ether_header));
#ifdef PIPEX
		if (pipex_enable) {
			struct pipex_session *session;

			if ((session = pipex_pppoe_lookup_session(m)) != NULL) {
				pipex_pppoe_input(m, session);
				goto done;
			}
		}
#endif
		if (etype == ETHERTYPE_PPPOEDISC)
			inq = &pppoediscinq;
		else
			inq = &pppoeinq;

		schednetisr(NETISR_PPPOE);
		break;
#endif
#ifdef MPLS
	case ETHERTYPE_MPLS:
	case ETHERTYPE_MPLS_MCAST:
		inq = &mplsintrq;
		schednetisr(NETISR_MPLS);
		break;
#endif
	default:
		if (llcfound || etype > ETHERMTU)
			goto dropanyway;
		llcfound = 1;
		l = mtod(m, struct llc *);
		switch (l->llc_dsap) {
		case LLC_SNAP_LSAP:
			if (l->llc_control == LLC_UI &&
			    l->llc_dsap == LLC_SNAP_LSAP &&
			    l->llc_ssap == LLC_SNAP_LSAP) {
				/* SNAP */
				if (m->m_pkthdr.len > etype)
					m_adj(m, etype - m->m_pkthdr.len);
				m_adj(m, 6);
				M_PREPEND(m, sizeof(*eh), M_DONTWAIT);
				if (m == NULL)
					goto done;
				*mtod(m, struct ether_header *) = *eh;
				goto decapsulate;
			}
			goto dropanyway;
		dropanyway:
		default:
			m_freem(m);
			goto done;
		}
	}

	IF_INPUT_ENQUEUE(inq, m);
done:
	splx(s);
}

/*
 * Convert Ethernet address to printable (loggable) representation.
 */
static char digits[] = "0123456789abcdef";
char *
ether_sprintf(u_char *ap)
{
	int i;
	static char etherbuf[ETHER_ADDR_LEN * 3];
	char *cp = etherbuf;

	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		*cp++ = digits[*ap >> 4];
		*cp++ = digits[*ap++ & 0xf];
		*cp++ = ':';
	}
	*--cp = 0;
	return (etherbuf);
}

/*
 * Generate a (hopefully) acceptable MAC address, if asked.
 */
void
ether_fakeaddr(struct ifnet *ifp)
{
	static int unit;
	int rng = arc4random();

	/* Non-multicast; locally administered address */
	((struct arpcom *)ifp)->ac_enaddr[0] = 0xfe;
	((struct arpcom *)ifp)->ac_enaddr[1] = 0xe1;
	((struct arpcom *)ifp)->ac_enaddr[2] = 0xba;
	((struct arpcom *)ifp)->ac_enaddr[3] = 0xd0 | (unit++ & 0xf);
	((struct arpcom *)ifp)->ac_enaddr[4] = rng;
	((struct arpcom *)ifp)->ac_enaddr[5] = rng >> 8;
}

/*
 * Perform common duties while attaching to interface list
 */
void
ether_ifattach(struct ifnet *ifp)
{
	/*
	 * Any interface which provides a MAC address which is obviously
	 * invalid gets whacked, so that users will notice.
	 */
	if (ETHER_IS_MULTICAST(((struct arpcom *)ifp)->ac_enaddr))
		ether_fakeaddr(ifp);

	ifp->if_type = IFT_ETHER;
	ifp->if_addrlen = ETHER_ADDR_LEN;
	ifp->if_hdrlen = ETHER_HDR_LEN;
	ifp->if_mtu = ETHERMTU;
	ifp->if_output = ether_output;

	if (ifp->if_hardmtu == 0)
		ifp->if_hardmtu = ETHERMTU;

	if_alloc_sadl(ifp);
	memcpy(LLADDR(ifp->if_sadl), ((struct arpcom *)ifp)->ac_enaddr,
	    ifp->if_addrlen);
	LIST_INIT(&((struct arpcom *)ifp)->ac_multiaddrs);
#if NBPFILTER > 0
	bpfattach(&ifp->if_bpf, ifp, DLT_EN10MB, ETHER_HDR_LEN);
#endif
}

void
ether_ifdetach(struct ifnet *ifp)
{
	struct arpcom *ac = (struct arpcom *)ifp;
	struct ether_multi *enm;

	for (enm = LIST_FIRST(&ac->ac_multiaddrs);
	    enm != NULL;
	    enm = LIST_FIRST(&ac->ac_multiaddrs)) {
		LIST_REMOVE(enm, enm_list);
		free(enm, M_IFMADDR, 0);
	}

#if 0
	/* moved to if_detach() */
	if_free_sadl(ifp);
#endif
}

#if 0
/*
 * This is for reference.  We have table-driven versions of the
 * crc32 generators, which are faster than the double-loop.
 */
u_int32_t __pure
ether_crc32_le_update(u_int_32_t crc, const u_int8_t *buf, size_t len)
{
	u_int32_t c, carry;
	size_t i, j;

	for (i = 0; i < len; i++) {
		c = buf[i];
		for (j = 0; j < 8; j++) {
			carry = ((crc & 0x01) ? 1 : 0) ^ (c & 0x01);
			crc >>= 1;
			c >>= 1;
			if (carry)
				crc = (crc ^ ETHER_CRC_POLY_LE);
		}
	}

	return (crc);
}

u_int32_t __pure
ether_crc32_be_update(u_int_32_t crc, const u_int8_t *buf, size_t len)
{
	u_int32_t c, carry;
	size_t i, j;

	for (i = 0; i < len; i++) {
		c = buf[i];
		for (j = 0; j < 8; j++) {
			carry = ((crc & 0x80000000U) ? 1 : 0) ^ (c & 0x01);
			crc <<= 1;
			c >>= 1;
			if (carry)
				crc = (crc ^ ETHER_CRC_POLY_BE) | carry;
		}
	}

	return (crc);
}
#else
u_int32_t __pure
ether_crc32_le_update(u_int32_t crc, const u_int8_t *buf, size_t len)
{
	static const u_int32_t crctab[] = {
		0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
		0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
		0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
		0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
	};
	size_t i;

	for (i = 0; i < len; i++) {
		crc ^= buf[i];
		crc = (crc >> 4) ^ crctab[crc & 0xf];
		crc = (crc >> 4) ^ crctab[crc & 0xf];
	}

	return (crc);
}

u_int32_t __pure
ether_crc32_be_update(u_int32_t crc, const u_int8_t *buf, size_t len)
{
	static const u_int8_t rev[] = {
		0x0, 0x8, 0x4, 0xc, 0x2, 0xa, 0x6, 0xe,
		0x1, 0x9, 0x5, 0xd, 0x3, 0xb, 0x7, 0xf
	};
	static const u_int32_t crctab[] = {
		0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9,
		0x130476dc, 0x17c56b6b, 0x1a864db2, 0x1e475005,
		0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
		0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd
	};
	size_t i;
	u_int8_t data;

	for (i = 0; i < len; i++) {
		data = buf[i];
		crc = (crc << 4) ^ crctab[(crc >> 28) ^ rev[data & 0xf]];
		crc = (crc << 4) ^ crctab[(crc >> 28) ^ rev[data >> 4]];
	}

	return (crc);
}
#endif

u_int32_t
ether_crc32_le(const u_int8_t *buf, size_t len)
{
	return ether_crc32_le_update(0xffffffff, buf, len);
}

u_int32_t
ether_crc32_be(const u_int8_t *buf, size_t len)
{
	return ether_crc32_be_update(0xffffffff, buf, len);
}

#ifdef INET
u_char	ether_ipmulticast_min[ETHER_ADDR_LEN] =
    { 0x01, 0x00, 0x5e, 0x00, 0x00, 0x00 };
u_char	ether_ipmulticast_max[ETHER_ADDR_LEN] =
    { 0x01, 0x00, 0x5e, 0x7f, 0xff, 0xff };
#endif

#ifdef INET6
u_char	ether_ip6multicast_min[ETHER_ADDR_LEN] =
    { 0x33, 0x33, 0x00, 0x00, 0x00, 0x00 };
u_char	ether_ip6multicast_max[ETHER_ADDR_LEN] =
    { 0x33, 0x33, 0xff, 0xff, 0xff, 0xff };
#endif

/*
 * Convert a sockaddr into an Ethernet address or range of Ethernet
 * addresses.
 */
int
ether_multiaddr(struct sockaddr *sa, u_int8_t addrlo[ETHER_ADDR_LEN],
    u_int8_t addrhi[ETHER_ADDR_LEN])
{
#ifdef INET
	struct sockaddr_in *sin;
#endif /* INET */
#ifdef INET6
	struct sockaddr_in6 *sin6;
#endif /* INET6 */

	switch (sa->sa_family) {

	case AF_UNSPEC:
		memcpy(addrlo, sa->sa_data, ETHER_ADDR_LEN);
		memcpy(addrhi, addrlo, ETHER_ADDR_LEN);
		break;

#ifdef INET
	case AF_INET:
		sin = satosin(sa);
		if (sin->sin_addr.s_addr == INADDR_ANY) {
			/*
			 * An IP address of INADDR_ANY means listen to
			 * or stop listening to all of the Ethernet
			 * multicast addresses used for IP.
			 * (This is for the sake of IP multicast routers.)
			 */
			memcpy(addrlo, ether_ipmulticast_min, ETHER_ADDR_LEN);
			memcpy(addrhi, ether_ipmulticast_max, ETHER_ADDR_LEN);
		} else {
			ETHER_MAP_IP_MULTICAST(&sin->sin_addr, addrlo);
			memcpy(addrhi, addrlo, ETHER_ADDR_LEN);
		}
		break;
#endif
#ifdef INET6
	case AF_INET6:
		sin6 = satosin6(sa);
		if (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr)) {
			/*
			 * An IP6 address of 0 means listen to or stop
			 * listening to all of the Ethernet multicast
			 * address used for IP6.
			 *
			 * (This might not be healthy, given IPv6's reliance on
			 * multicast for things like neighbor discovery.
			 * Perhaps initializing all-nodes, solicited nodes, and
			 * possibly all-routers for this interface afterwards
			 * is not a bad idea.)
			 */

			memcpy(addrlo, ether_ip6multicast_min, ETHER_ADDR_LEN);
			memcpy(addrhi, ether_ip6multicast_max, ETHER_ADDR_LEN);
		} else {
			ETHER_MAP_IPV6_MULTICAST(&sin6->sin6_addr, addrlo);
			memcpy(addrhi, addrlo, ETHER_ADDR_LEN);
		}
		break;
#endif

	default:
		return (EAFNOSUPPORT);
	}
	return (0);
}

/*
 * Add an Ethernet multicast address or range of addresses to the list for a
 * given interface.
 */
int
ether_addmulti(struct ifreq *ifr, struct arpcom *ac)
{
	struct ether_multi *enm;
	u_char addrlo[ETHER_ADDR_LEN];
	u_char addrhi[ETHER_ADDR_LEN];
	int s = splnet(), error;

	error = ether_multiaddr(&ifr->ifr_addr, addrlo, addrhi);
	if (error != 0) {
		splx(s);
		return (error);
	}

	/*
	 * Verify that we have valid Ethernet multicast addresses.
	 */
	if ((addrlo[0] & 0x01) != 1 || (addrhi[0] & 0x01) != 1) {
		splx(s);
		return (EINVAL);
	}
	/*
	 * See if the address range is already in the list.
	 */
	ETHER_LOOKUP_MULTI(addrlo, addrhi, ac, enm);
	if (enm != NULL) {
		/*
		 * Found it; just increment the reference count.
		 */
		++enm->enm_refcount;
		splx(s);
		return (0);
	}
	/*
	 * New address or range; malloc a new multicast record
	 * and link it into the interface's multicast list.
	 */
	enm = malloc(sizeof(*enm), M_IFMADDR, M_NOWAIT);
	if (enm == NULL) {
		splx(s);
		return (ENOBUFS);
	}
	memcpy(enm->enm_addrlo, addrlo, ETHER_ADDR_LEN);
	memcpy(enm->enm_addrhi, addrhi, ETHER_ADDR_LEN);
	enm->enm_ac = ac;
	enm->enm_refcount = 1;
	LIST_INSERT_HEAD(&ac->ac_multiaddrs, enm, enm_list);
	ac->ac_multicnt++;
	if (memcmp(addrlo, addrhi, ETHER_ADDR_LEN) != 0)
		ac->ac_multirangecnt++;
	splx(s);
	/*
	 * Return ENETRESET to inform the driver that the list has changed
	 * and its reception filter should be adjusted accordingly.
	 */
	return (ENETRESET);
}

/*
 * Delete a multicast address record.
 */
int
ether_delmulti(struct ifreq *ifr, struct arpcom *ac)
{
	struct ether_multi *enm;
	u_char addrlo[ETHER_ADDR_LEN];
	u_char addrhi[ETHER_ADDR_LEN];
	int s = splnet(), error;

	error = ether_multiaddr(&ifr->ifr_addr, addrlo, addrhi);
	if (error != 0) {
		splx(s);
		return (error);
	}

	/*
	 * Look up the address in our list.
	 */
	ETHER_LOOKUP_MULTI(addrlo, addrhi, ac, enm);
	if (enm == NULL) {
		splx(s);
		return (ENXIO);
	}
	if (--enm->enm_refcount != 0) {
		/*
		 * Still some claims to this record.
		 */
		splx(s);
		return (0);
	}
	/*
	 * No remaining claims to this record; unlink and free it.
	 */
	LIST_REMOVE(enm, enm_list);
	free(enm, M_IFMADDR, 0);
	ac->ac_multicnt--;
	if (memcmp(addrlo, addrhi, ETHER_ADDR_LEN) != 0)
		ac->ac_multirangecnt--;
	splx(s);
	/*
	 * Return ENETRESET to inform the driver that the list has changed
	 * and its reception filter should be adjusted accordingly.
	 */
	return (ENETRESET);
}
