/*	$OpenBSD: if_ethersubr.c,v 1.68 2003/02/16 21:30:13 deraadt Exp $	*/
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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

#include <machine/cpu.h>

#include <net/if.h>
#include <net/netisr.h>
#include <net/route.h>
#include <net/if_llc.h>
#include <net/if_dl.h>
#include <net/if_types.h>

#include <netinet/in.h>
#ifdef INET
#include <netinet/in_var.h>
#endif
#include <netinet/if_ether.h>
#include <netinet/ip_ipsp.h>

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

#ifdef INET6
#ifndef INET
#include <netinet/in.h>
#endif
#include <netinet6/in6_var.h>
#include <netinet6/nd6.h>
#endif

#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#ifdef IPX
#include <netipx/ipx.h>
#include <netipx/ipx_if.h>
#endif

#ifdef ISO
#include <netiso/argo_debug.h>
#include <netiso/iso.h>
#include <netiso/iso_var.h>
#include <netiso/iso_snpac.h>
#endif

#include <netccitt/x25.h>
#include <netccitt/pk.h>
#include <netccitt/pk_extern.h>
#include <netccitt/dll.h>
#include <netccitt/llc_var.h>

#ifdef NETATALK
#include <netatalk/at.h>
#include <netatalk/at_var.h>
#include <netatalk/at_extern.h>

extern u_char	at_org_code[ 3 ];
extern u_char	aarp_org_code[ 3 ];
#endif /* NETATALK */

#if defined(CCITT)
#include <sys/socketvar.h>
#endif

u_char etherbroadcastaddr[ETHER_ADDR_LEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
#define senderr(e) { error = (e); goto bad;}


int
ether_ioctl(ifp, arp, cmd, data)
	register struct ifnet *ifp;
	struct arpcom *arp;
	u_long cmd;
	caddr_t data;
{
	struct ifaddr *ifa = (struct ifaddr *)data;
	int	error = 0;

	switch (cmd) {

#if defined(CCITT)
	case SIOCSIFCONF_X25:
		ifp->if_flags |= IFF_UP;
		ifa->ifa_rtrequest = cons_rtrequest;
		error = x25_llcglue(PRC_IFUP, ifa->ifa_addr);
		break;
#endif /* CCITT */
	case SIOCSIFADDR:
		switch (ifa->ifa_addr->sa_family) {
#ifdef IPX
		case AF_IPX:
		    {
			struct ipx_addr *ina = &IA_SIPX(ifa)->sipx_addr;

			if (ipx_nullhost(*ina))
				ina->ipx_host =
				    *(union ipx_host *)(arp->ac_enaddr);
			else
				bcopy(ina->ipx_host.c_host,
				    arp->ac_enaddr, sizeof(arp->ac_enaddr));
			break;
		    }
#endif /* IPX */
#ifdef NETATALK
		case AF_APPLETALK:
			/* Nothing to do. */
			break;
#endif /* NETATALK */
#ifdef NS
		/* XXX - This code is probably wrong. */
		case AF_NS:
		    {
			struct ns_addr *ina = &IA_SNS(ifa)->sns_addr;

			if (ns_nullhost(*ina))
				ina->x_host =
				    *(union ns_host *)(arp->ac_enaddr);
			else
				bcopy(ina->x_host.c_host,
				    arp->ac_enaddr, sizeof(arp->ac_enaddr));
			break;
		    }
#endif /* NS */
		}
		break;
	default:
		break;
	}

	return error;
}

/*
 * Ethernet output routine.
 * Encapsulate a packet of type family for the local net.
 * Assumes that ifp is actually pointer to arpcom structure.
 */
int
ether_output(ifp, m0, dst, rt0)
	register struct ifnet *ifp;
	struct mbuf *m0;
	struct sockaddr *dst;
	struct rtentry *rt0;
{
	u_int16_t etype;
	int s, len, error = 0, hdrcmplt = 0;
 	u_char edst[ETHER_ADDR_LEN], esrc[ETHER_ADDR_LEN];
	register struct mbuf *m = m0;
	register struct rtentry *rt;
	struct mbuf *mcopy = (struct mbuf *)0;
	register struct ether_header *eh;
	struct arpcom *ac = (struct arpcom *)ifp;
	short mflags;

	if ((ifp->if_flags & (IFF_UP|IFF_RUNNING)) != (IFF_UP|IFF_RUNNING))
		senderr(ENETDOWN);
	if ((rt = rt0) != NULL) {
		if ((rt->rt_flags & RTF_UP) == 0) {
			if ((rt0 = rt = rtalloc1(dst, 1)) != NULL)
				rt->rt_refcnt--;
			else
				senderr(EHOSTUNREACH);
		}
		if (rt->rt_flags & RTF_GATEWAY) {
			if (rt->rt_gwroute == 0)
				goto lookup;
			if (((rt = rt->rt_gwroute)->rt_flags & RTF_UP) == 0) {
				rtfree(rt); rt = rt0;
			lookup: rt->rt_gwroute = rtalloc1(rt->rt_gateway, 1);
				if ((rt = rt->rt_gwroute) == 0)
					senderr(EHOSTUNREACH);
			}
		}
		if (rt->rt_flags & RTF_REJECT)
			if (rt->rt_rmx.rmx_expire == 0 ||
			    time.tv_sec < rt->rt_rmx.rmx_expire)
				senderr(rt == rt0 ? EHOSTDOWN : EHOSTUNREACH);
	}

	switch (dst->sa_family) {

#ifdef INET
	case AF_INET:
		if (!arpresolve(ac, rt, m, dst, edst))
			return (0);	/* if not yet resolved */
		/* If broadcasting on a simplex interface, loopback a copy */
		if ((m->m_flags & M_BCAST) && (ifp->if_flags & IFF_SIMPLEX))
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
#ifdef NS
	case AF_NS:
		etype = htons(ETHERTYPE_NS);
 		bcopy((caddr_t)&(((struct sockaddr_ns *)dst)->sns_addr.x_host),
		    (caddr_t)edst, sizeof (edst));
		if (!bcmp((caddr_t)edst, (caddr_t)&ns_thishost, sizeof(edst)))
			return (looutput(ifp, m, dst, rt));
		/* If broadcasting on a simplex interface, loopback a copy */
		if ((m->m_flags & M_BCAST) && (ifp->if_flags & IFF_SIMPLEX))
			mcopy = m_copy(m, 0, (int)M_COPYALL);
		break;
#endif
#ifdef IPX
	case AF_IPX:
		etype = htons(ETHERTYPE_IPX);
 		bcopy((caddr_t)&satosipx(dst)->sipx_addr.ipx_host,
		    (caddr_t)edst, sizeof (edst));
		/* If broadcasting on a simplex interface, loopback a copy */
		if ((m->m_flags & M_BCAST) && (ifp->if_flags & IFF_SIMPLEX))
			mcopy = m_copy(m, 0, (int)M_COPYALL);
		break;
#endif
#if 0	/*NRL INET6*/
	case AF_INET6:
		/*
		 * The bottom line here is to either queue the outgoing packet
		 * in the discovery engine, or fill in edst with something
		 * that'll work.
		 */
		if (m->m_flags & M_MCAST) {
			/*
			 * If multicast dest., then use IPv6 -> Ethernet
			 * mcast mapping.  Really simple.
			 */
			ETHER_MAP_IPV6_MULTICAST(&((struct sockaddr_in6 *)dst)->sin6_addr,
			    edst);
		} else {
			/* Do unicast neighbor discovery stuff. */
			if (!ipv6_discov_resolve(ifp, rt, m, dst, edst))
				return 0;
		}
		etype = htons(ETHERTYPE_IPV6);
		break;
#endif /* INET6 */
#ifdef NETATALK
	case AF_APPLETALK: {
		struct at_ifaddr *aa;

		if (!aarpresolve(ac, m, (struct sockaddr_at *)dst, edst)) {
#ifdef NETATALKDEBUG
			extern char *prsockaddr(struct sockaddr *);
			printf("aarpresolv: failed for %s\n", prsockaddr(dst));
#endif /* NETATALKDEBUG */
			return (0);
		}

		/*
		 * ifaddr is the first thing in at_ifaddr
		 */
		aa = (struct at_ifaddr *)at_ifawithnet(
			(struct sockaddr_at *)dst,
			TAILQ_FIRST(&ifp->if_addrlist));
		if (aa == 0)
			goto bad;

		/*
		 * In the phase 2 case, we need to prepend an mbuf for the llc
		 * header. Since we must preserve the value of m, which is
		 * passed to us by value, we m_copy() the first mbuf,
		 * and use it for our llc header.
		 */
		if (aa->aa_flags & AFA_PHASE2) {
			struct llc llc;

			M_PREPEND(m, AT_LLC_SIZE, M_DONTWAIT);
			if (m == NULL)
				return (0);
			/*
			 * FreeBSD doesn't count the LLC len in
			 * ifp->obytes, so they increment a length
			 * field here. We don't do this.
			 */
			llc.llc_dsap = llc.llc_ssap = LLC_SNAP_LSAP;
			llc.llc_control = LLC_UI;
			bcopy(at_org_code, llc.llc_snap.org_code,
				sizeof(at_org_code));
			llc.llc_snap.ether_type = htons( ETHERTYPE_AT );
			bcopy(&llc, mtod(m, caddr_t), AT_LLC_SIZE);
			etype = htons(m->m_pkthdr.len);
		} else {
			etype = htons(ETHERTYPE_AT);
		}
		} break;
#endif /* NETATALK */
#ifdef	ISO
	case AF_ISO: {
		int	snpalen;
		struct	llc *l;
		register struct sockaddr_dl *sdl;

		if (rt && (sdl = (struct sockaddr_dl *)rt->rt_gateway) &&
		    sdl->sdl_family == AF_LINK && sdl->sdl_alen > 0) {
			bcopy(LLADDR(sdl), (caddr_t)edst, sizeof(edst));
		} else {
			error = iso_snparesolve(ifp, (struct sockaddr_iso *)dst,
						(char *)edst, &snpalen);
			if (error)
				goto bad; /* Not Resolved */
		}
		/* If broadcasting on a simplex interface, loopback a copy */
		if (*edst & 1)
			m->m_flags |= (M_BCAST|M_MCAST);
		if ((m->m_flags & M_BCAST) && (ifp->if_flags & IFF_SIMPLEX) &&
		    (mcopy = m_copy(m, 0, (int)M_COPYALL))) {
			M_PREPEND(mcopy, sizeof (*eh), M_DONTWAIT);
			if (mcopy) {
				eh = mtod(mcopy, struct ether_header *);
				bcopy(edst, eh->ether_dhost, sizeof (edst));
				bcopy(ac->ac_enaddr, eh->ether_shost,
				    sizeof (edst));
			}
		}
		M_PREPEND(m, 3, M_DONTWAIT);
		if (m == NULL)
			return (0);
		etype = htons(m->m_pkthdr.len);
		l = mtod(m, struct llc *);
		l->llc_dsap = l->llc_ssap = LLC_ISO_LSAP;
		l->llc_control = LLC_UI;
#ifdef ARGO_DEBUG
		if (argo_debug[D_ETHER]) {
			int i;
			printf("unoutput: sending pkt to: ");
			for (i=0; i < ETHER_ADDR_LEN; i++)
				printf("%x ", edst[i] & 0xff);
			printf("\n");
		}
#endif
		} break;
#endif /* ISO */
/*	case AF_NSAP: */
	case AF_CCITT: {
		register struct sockaddr_dl *sdl =
			(struct sockaddr_dl *) rt -> rt_gateway;

		if (sdl && sdl->sdl_family == AF_LINK
		    && sdl->sdl_alen > 0) {
			bcopy(LLADDR(sdl), (char *)edst,
				sizeof(edst));
		} else goto bad; /* Not a link interface ? Funny ... */
		if ((ifp->if_flags & IFF_SIMPLEX) && (*edst & 1) &&
		    (mcopy = m_copy(m, 0, (int)M_COPYALL))) {
			M_PREPEND(mcopy, sizeof (*eh), M_DONTWAIT);
			if (mcopy) {
				eh = mtod(mcopy, struct ether_header *);
				bcopy(edst, eh->ether_dhost, sizeof (edst));
				bcopy(ac->ac_enaddr, eh->ether_shost,
				    sizeof (edst));
			}
		}
		etype = htons(m->m_pkthdr.len);
#ifdef LLC_DEBUG
		{
			int i;
			register struct llc *l = mtod(m, struct llc *);

			printf("ether_output: sending LLC2 pkt to: ");
			for (i=0; i < ETHER_ADDR_LEN; i++)
				printf("%x ", edst[i] & 0xff);
			printf(" len 0x%x dsap 0x%x ssap 0x%x control 0x%x\n",
			    m->m_pkthdr.len, l->llc_dsap & 0xff, l->llc_ssap &0xff,
			    l->llc_control & 0xff);

		}
#endif /* LLC_DEBUG */
		} break;

	case pseudo_AF_HDRCMPLT:
		hdrcmplt = 1;
		eh = (struct ether_header *)dst->sa_data;
		bcopy((caddr_t)eh->ether_shost, (caddr_t)esrc, sizeof (esrc));
		/* FALLTHROUGH */

	case AF_UNSPEC:
		eh = (struct ether_header *)dst->sa_data;
 		bcopy((caddr_t)eh->ether_dhost, (caddr_t)edst, sizeof (edst));
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

	/*
	 * Add local net header.  If no space in first mbuf,
	 * allocate another.
	 */
	M_PREPEND(m, sizeof (struct ether_header), M_DONTWAIT);
	if (m == 0)
		senderr(ENOBUFS);
	eh = mtod(m, struct ether_header *);
	bcopy((caddr_t)&etype,(caddr_t)&eh->ether_type,
		sizeof(eh->ether_type));
 	bcopy((caddr_t)edst, (caddr_t)eh->ether_dhost, sizeof (edst));
	if (hdrcmplt)
	 	bcopy((caddr_t)esrc, (caddr_t)eh->ether_shost,
		    sizeof(eh->ether_shost));
	else
	 	bcopy((caddr_t)ac->ac_enaddr, (caddr_t)eh->ether_shost,
		    sizeof(eh->ether_shost));

#if NBRIDGE > 0
	/*
	 * Interfaces that are bridge members need special handling
	 * for output.
	 */
	if (ifp->if_bridge) {
		struct m_tag *mtag;

		/*
		 * Check if this packet has already been sent out through
		 * this bridge, in which case we simply send it out
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
			if (!bcmp(&ifp->if_bridge, mtag + 1, sizeof(caddr_t)))
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
			bcopy(&ifp->if_bridge, mtag + 1, sizeof(caddr_t));
			m_tag_prepend(m, mtag);
			bridge_output(ifp, m, NULL, NULL);
			return (error);
		}
	}
#endif

	mflags = m->m_flags;
	len = m->m_pkthdr.len;
	s = splimp();
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
	ifp->if_obytes += len + sizeof (struct ether_header);
	if (mflags & M_MCAST)
		ifp->if_omcasts++;
	if ((ifp->if_flags & IFF_OACTIVE) == 0)
		(*ifp->if_start)(ifp);
	splx(s);
	return (error);

bad:
	if (m)
		m_freem(m);
	return (error);
}

/*
 * Temporary function to migrate while
 * removing ether_header * from ether_input().
 */
void
ether_input_mbuf(ifp, m)
	struct ifnet *ifp;
	struct mbuf *m;
{
	struct ether_header *eh;

	eh = mtod(m, struct ether_header *);
	m_adj(m, ETHER_HDR_LEN);
	ether_input(ifp, eh, m);
}

/*
 * Process a received Ethernet packet;
 * the packet is in the mbuf chain m without
 * the ether header, which is provided separately.
 */
void
ether_input(ifp, eh, m)
	struct ifnet *ifp;
	register struct ether_header *eh;
	struct mbuf *m;
{
	register struct ifqueue *inq;
	u_int16_t etype;
	int s, llcfound = 0;
	register struct llc *l;
	struct arpcom *ac;

	if ((ifp->if_flags & IFF_UP) == 0) {
		m_freem(m);
		return;
	}
	if (ETHER_IS_MULTICAST(eh->ether_dhost)) {
		if ((ifp->if_flags & IFF_SIMPLEX) == 0) {
			struct ifaddr *ifa;
			struct sockaddr_dl *sdl = NULL;

			TAILQ_FOREACH(ifa, &ifp->if_addrlist, ifa_list) {
				if ((sdl = (struct sockaddr_dl *)ifa->ifa_addr) &&
				    sdl->sdl_family == AF_LINK)
					break;
			}
			/*
			 * If this is not a simplex interface, drop the packet
			 * if it came from us.
			 */
			if (sdl && bcmp(LLADDR(sdl), eh->ether_shost,
			    ETHER_ADDR_LEN) == 0) {
				m_freem(m);
				return;
			}
		}

		if (bcmp((caddr_t)etherbroadcastaddr, (caddr_t)eh->ether_dhost,
		    sizeof(etherbroadcastaddr)) == 0)
			m->m_flags |= M_BCAST;
		else
			m->m_flags |= M_MCAST;
		ifp->if_imcasts++;
	}

	ifp->if_ibytes += m->m_pkthdr.len + sizeof (*eh);

	etype = ntohs(eh->ether_type);

#if NBRIDGE > 0
	/*
	 * Tap the packet off here for a bridge, if configured and
	 * active for this interface.  bridge_input returns
	 * NULL if it has consumed the packet, otherwise, it
	 * gets processed as normal.
	 */
	if (ifp->if_bridge) {
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
	if (etype == ETHERTYPE_8021Q) {
		if (vlan_input(eh, m) < 0)
			ifp->if_noproto++;
		return;
       }
#endif /* NVLAN > 0 */

	ac = (struct arpcom *)ifp;

	/*
	 * If packet is unicast and we're in promiscuous mode, make sure it
	 * is for us.  Drop otherwise.
	 */
	if ((m->m_flags & (M_BCAST|M_MCAST)) == 0 &&
	    (ifp->if_flags & IFF_PROMISC)) {
		if (bcmp(ac->ac_enaddr, (caddr_t)eh->ether_dhost,
		    ETHER_ADDR_LEN)) {
			m_freem(m);
			return;
		}
	}

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
		return;

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
#ifdef IPX
	case ETHERTYPE_IPX:
		schednetisr(NETISR_IPX);
		inq = &ipxintrq;
		break;
#endif
#ifdef NS
	case ETHERTYPE_NS:
		schednetisr(NETISR_NS);
		inq = &nsintrq;
		break;
#endif
#ifdef NETATALK
	case ETHERTYPE_AT:
		schednetisr(NETISR_ATALK);
		inq = &atintrq1;
		break;
	case ETHERTYPE_AARP:
		/* probably this should be done with a NETISR as well */
		/* XXX queue this */
		aarpinput((struct arpcom *)ifp, m);
		return;
#endif
	default:
		if (llcfound || etype > ETHERMTU)
			goto dropanyway;
		llcfound = 1;
		l = mtod(m, struct llc *);
		switch (l->llc_dsap) {
		case LLC_SNAP_LSAP:
#ifdef NETATALK
			/*
			 * Some protocols (like Appletalk) need special
			 * handling depending on if they are type II
			 * or SNAP encapsulated. Everything else
			 * gets handled by stripping off the SNAP header
			 * and going back up to decapsulate.
			 */
			if (l->llc_control == LLC_UI &&
			    l->llc_ssap == LLC_SNAP_LSAP &&
			    Bcmp(&(l->llc_snap.org_code)[0],
			    at_org_code, sizeof(at_org_code)) == 0 &&
			    ntohs(l->llc_snap.ether_type) == ETHERTYPE_AT) {
				inq = &atintrq2;
				m_adj(m, AT_LLC_SIZE);
				schednetisr(NETISR_ATALK);
				break;
			}

			if (l->llc_control == LLC_UI &&
			    l->llc_ssap == LLC_SNAP_LSAP &&
			    Bcmp(&(l->llc_snap.org_code)[0],
			    aarp_org_code, sizeof(aarp_org_code)) == 0 &&
			    ntohs(l->llc_snap.ether_type) == ETHERTYPE_AARP) {
				m_adj(m, AT_LLC_SIZE);
				/* XXX Really this should use netisr too */
				aarpinput((struct arpcom *)ifp, m);
				return;
			}
#endif /* NETATALK */
			if (l->llc_control == LLC_UI &&
			    l->llc_dsap == LLC_SNAP_LSAP &&
			    l->llc_ssap == LLC_SNAP_LSAP) {
				/* SNAP */
				if (m->m_pkthdr.len > etype)
					m_adj(m, etype - m->m_pkthdr.len);
				m->m_data += 6;		/* XXX */
				m->m_len -= 6;		/* XXX */
				m->m_pkthdr.len -= 6;	/* XXX */
				M_PREPEND(m, sizeof *eh, M_DONTWAIT);
				if (m == 0)
					return;
				*mtod(m, struct ether_header *) = *eh;
				goto decapsulate;
			}
			goto dropanyway;
#ifdef	ISO
		case LLC_ISO_LSAP:
			switch (l->llc_control) {
			case LLC_UI:
				/* LLC_UI_P forbidden in class 1 service */
				if ((l->llc_dsap == LLC_ISO_LSAP) &&
				    (l->llc_ssap == LLC_ISO_LSAP)) {
					/* LSAP for ISO */
					if (m->m_pkthdr.len > etype)
						m_adj(m, etype - m->m_pkthdr.len);
					m->m_data += 3;		/* XXX */
					m->m_len -= 3;		/* XXX */
					m->m_pkthdr.len -= 3;	/* XXX */
					M_PREPEND(m, sizeof *eh, M_DONTWAIT);
					if (m == 0)
						return;
					*mtod(m, struct ether_header *) = *eh;
#ifdef ARGO_DEBUG
					if (argo_debug[D_ETHER])
						printf("clnp packet");
#endif
					schednetisr(NETISR_ISO);
					inq = &clnlintrq;
					break;
				}
				goto dropanyway;

			case LLC_XID:
			case LLC_XID_P:
				if (m->m_len < ETHER_ADDR_LEN)
					goto dropanyway;
				l->llc_window = 0;
				l->llc_fid = 9;
				l->llc_class = 1;
				l->llc_dsap = l->llc_ssap = 0;
				/* Fall through to */
			case LLC_TEST:
			case LLC_TEST_P:
			{
				struct sockaddr sa;
				register struct ether_header *eh2;
				int i;
				u_char c = l->llc_dsap;

				l->llc_dsap = l->llc_ssap;
				l->llc_ssap = c;
				if (m->m_flags & (M_BCAST | M_MCAST))
					bcopy(ac->ac_enaddr,
					    eh->ether_dhost, ETHER_ADDR_LEN);
				sa.sa_family = AF_UNSPEC;
				sa.sa_len = sizeof(sa);
				eh2 = (struct ether_header *)sa.sa_data;
				for (i = 0; i < ETHER_ADDR_LEN; i++) {
					eh2->ether_shost[i] = c = eh->ether_dhost[i];
					eh2->ether_dhost[i] =
						eh->ether_dhost[i] = eh->ether_shost[i];
					eh->ether_shost[i] = c;
				}
				ifp->if_output(ifp, m, &sa, NULL);
				return;
			}
			break;
			}
#endif /* ISO */
#ifdef CCITT
		case LLC_X25_LSAP:
			if (m->m_pkthdr.len > etype)
				m_adj(m, etype - m->m_pkthdr.len);
			M_PREPEND(m, sizeof(struct sdl_hdr) , M_DONTWAIT);
			if (m == 0)
				return;
			if (!sdl_sethdrif(ifp, eh->ether_shost, LLC_X25_LSAP,
			    eh->ether_dhost, LLC_X25_LSAP, ETHER_ADDR_LEN,
			    mtod(m, struct sdl_hdr *)))
				panic("ETHER cons addr failure");
			mtod(m, struct sdl_hdr *)->sdlhdr_len = etype;
#ifdef LLC_DEBUG
			printf("llc packet\n");
#endif /* LLC_DEBUG */
			schednetisr(NETISR_CCITT);
			inq = &llcintrq;
			break;
#endif /* CCITT */
		dropanyway:
		default:
			m_freem(m);
			return;
		}
	}

	s = splimp();
	if (IF_QFULL(inq)) {
		IF_DROP(inq);
		m_freem(m);
	} else
		IF_ENQUEUE(inq, m);
	splx(s);
}

/*
 * Convert Ethernet address to printable (loggable) representation.
 */
static char digits[] = "0123456789abcdef";
char *
ether_sprintf(ap)
	register u_char *ap;
{
	register int i;
	static char etherbuf[18];
	register char *cp = etherbuf;

	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		*cp++ = digits[*ap >> 4];
		*cp++ = digits[*ap++ & 0xf];
		*cp++ = ':';
	}
	*--cp = 0;
	return (etherbuf);
}

/*
 * Perform common duties while attaching to interface list
 */
void
ether_ifattach(ifp)
	register struct ifnet *ifp;
{

	/*
	 * Any interface which provides a MAC address which is obviously
	 * invalid gets whacked, so that users will notice.
	 */
	if (ETHER_IS_MULTICAST(((struct arpcom *)ifp)->ac_enaddr)) {
		((struct arpcom *)ifp)->ac_enaddr[0] = 0x00;
		((struct arpcom *)ifp)->ac_enaddr[1] = 0xfe;
		((struct arpcom *)ifp)->ac_enaddr[2] = 0xe1;
		((struct arpcom *)ifp)->ac_enaddr[3] = 0xba;
		((struct arpcom *)ifp)->ac_enaddr[4] = 0xd0;
		/*
		 * XXX use of random() by anything except the scheduler is
		 * normally invalid, but this is boot time, so pre-scheduler,
		 * and the random subsystem is not alive yet
		 */
		((struct arpcom *)ifp)->ac_enaddr[5] = (u_char)random() & 0xff;
	}
		
	ifp->if_type = IFT_ETHER;
	ifp->if_addrlen = ETHER_ADDR_LEN;
	ifp->if_hdrlen = ETHER_HDR_LEN;
	ifp->if_mtu = ETHERMTU;
	ifp->if_output = ether_output;

	if_alloc_sadl(ifp);
	bcopy((caddr_t)((struct arpcom *)ifp)->ac_enaddr,
	    LLADDR(ifp->if_sadl), ifp->if_addrlen);
	LIST_INIT(&((struct arpcom *)ifp)->ac_multiaddrs);
#if NBPFILTER > 0
	bpfattach(&ifp->if_bpf, ifp, DLT_EN10MB, sizeof(struct ether_header));
#endif
}

void
ether_ifdetach(ifp)
	struct ifnet *ifp;
{
	struct arpcom *ac = (struct arpcom *)ifp;
	struct ether_multi *enm;

	for (enm = LIST_FIRST(&ac->ac_multiaddrs);
	    enm != LIST_END(&ac->ac_multiaddrs);
	    enm = LIST_FIRST(&ac->ac_multiaddrs)) {
		LIST_REMOVE(enm, enm_list);
		free(enm, M_IFMADDR);
	}

	if_free_sadl(ifp);
}

#if 0
/*
 * This is for reference.  We have a table-driven version
 * of the little-endian crc32 generator, which is faster
 * than the double-loop.
 */
u_int32_t
ether_crc32_le(const u_int8_t *buf, size_t len)
{
	u_int32_t c, crc, carry;
	size_t i, j;

	crc = 0xffffffffU;	/* initial value */

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
#else
u_int32_t
ether_crc32_le(const u_int8_t *buf, size_t len)
{
	static const u_int32_t crctab[] = {
		0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
		0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
		0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
		0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
	};
	u_int32_t crc;
	int i;

	crc = 0xffffffffU;	/* initial value */

	for (i = 0; i < len; i++) {
		crc ^= buf[i];
		crc = (crc >> 4) ^ crctab[crc & 0xf];
		crc = (crc >> 4) ^ crctab[crc & 0xf];
	}

	return (crc);
}
#endif

u_int32_t
ether_crc32_be(const u_int8_t *buf, size_t len)
{
	u_int32_t c, crc, carry;
	size_t i, j;

	crc = 0xffffffffU;	/* initial value */

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
		bcopy(sa->sa_data, addrlo, ETHER_ADDR_LEN);
		bcopy(addrlo, addrhi, ETHER_ADDR_LEN);
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
			bcopy(ether_ipmulticast_min, addrlo, ETHER_ADDR_LEN);
			bcopy(ether_ipmulticast_max, addrhi, ETHER_ADDR_LEN);
		} else {
			ETHER_MAP_IP_MULTICAST(&sin->sin_addr, addrlo);
			bcopy(addrlo, addrhi, ETHER_ADDR_LEN);
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

			bcopy(ether_ip6multicast_min, addrlo, ETHER_ADDR_LEN);
			bcopy(ether_ip6multicast_max, addrhi, ETHER_ADDR_LEN);
		} else {
			ETHER_MAP_IPV6_MULTICAST(&sin6->sin6_addr, addrlo);
			bcopy(addrlo, addrhi, ETHER_ADDR_LEN);
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
ether_addmulti(ifr, ac)
	struct ifreq *ifr;
	register struct arpcom *ac;
{
	register struct ether_multi *enm;
	u_char addrlo[ETHER_ADDR_LEN];
	u_char addrhi[ETHER_ADDR_LEN];
	int s = splimp(), error;

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
	enm = (struct ether_multi *)malloc(sizeof(*enm), M_IFMADDR, M_NOWAIT);
	if (enm == NULL) {
		splx(s);
		return (ENOBUFS);
	}
	bcopy(addrlo, enm->enm_addrlo, ETHER_ADDR_LEN);
	bcopy(addrhi, enm->enm_addrhi, ETHER_ADDR_LEN);
	enm->enm_ac = ac;
	enm->enm_refcount = 1;
	LIST_INSERT_HEAD(&ac->ac_multiaddrs, enm, enm_list);
	ac->ac_multicnt++;
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
ether_delmulti(ifr, ac)
	struct ifreq *ifr;
	register struct arpcom *ac;
{
	register struct ether_multi *enm;
	u_char addrlo[ETHER_ADDR_LEN];
	u_char addrhi[ETHER_ADDR_LEN];
	int s = splimp(), error;

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
	free(enm, M_IFMADDR);
	ac->ac_multicnt--;
	splx(s);
	/*
	 * Return ENETRESET to inform the driver that the list has changed
	 * and its reception filter should be adjusted accordingly.
	 */
	return (ENETRESET);
}
