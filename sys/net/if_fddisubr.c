/*	$OpenBSD: if_fddisubr.c,v 1.42 2004/12/10 22:35:17 mcbride Exp $	*/
/*	$NetBSD: if_fddisubr.c,v 1.5 1996/05/07 23:20:21 christos Exp $	*/

/*
 * Copyright (c) 1995
 *	Matt Thomas.  All rights reserved.
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
 *	@(#)if_fddisubr.c	8.1 (Berkeley) 6/10/93
 */

/*
 *	@(#)COPYRIGHT	1.1 (NRL) January 1997
 * 
 * NRL grants permission for redistribution and use in source and binary
 * forms, with or without modification, of the software and documentation
 * created at NRL provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgements:
 * 	This product includes software developed by the University of
 * 	California, Berkeley and its contributors.
 * 	This product includes software developed at the Information
 * 	Technology Division, US Naval Research Laboratory.
 * 4. Neither the name of the NRL nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THE SOFTWARE PROVIDED BY NRL IS PROVIDED BY NRL AND CONTRIBUTORS ``AS
 * IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL NRL OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * The views and conclusions contained in the software and documentation
 * are those of the authors and should not be interpreted as representing
 * official policies, either expressed or implied, of the US Naval
 * Research Laboratory (NRL).
 */

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

#include <machine/cpu.h>

#include <net/if.h>
#include <net/netisr.h>
#include <net/route.h>
#include <net/if_llc.h>
#include <net/if_dl.h>
#include <net/if_types.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_var.h>
#endif
#include <netinet/if_ether.h>
#include <net/if_fddi.h>

#ifdef IPX
#include <netipx/ipx.h>
#include <netipx/ipx_if.h>
#endif

#ifdef INET6
#ifndef INET
#include <netinet/in.h>
#include <netinet/in_var.h>
#endif
#include <netinet6/nd6.h>
#endif

#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#ifdef DECNET
#include <netdnet/dn.h>
#endif

#include "bpfilter.h"

#include <netccitt/dll.h>
#include <netccitt/llc_var.h>

#if defined(CCITT)
extern struct ifqueue pkintrq;
#endif

#include "carp.h"
#if NCARP > 0
#include <netinet/ip_carp.h>
#endif

#define senderr(e) { error = (e); goto bad;}

/*
 * This really should be defined in if_llc.h but in case it isn't.
 */
#ifndef llc_snap
#define	llc_snap	llc_un.type_snap
#endif

/*
 * FDDI output routine.
 * Encapsulate a packet of type family for the local net.
 * Assumes that ifp is actually pointer to arpcom structure.
 */
int
fddi_output(ifp, m0, dst, rt0)
	struct ifnet *ifp;
	struct mbuf *m0;
	struct sockaddr *dst;
	struct rtentry *rt0;
{
	u_int16_t type;
	int s, len, error = 0, hdrcmplt = 0;
 	u_char edst[6], esrc[6];
	struct mbuf *m = m0;
	struct rtentry *rt;
	struct mbuf *mcopy = (struct mbuf *)0;
	struct fddi_header *fh;
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
			    time_second < rt->rt_rmx.rmx_expire)
				senderr(rt == rt0 ? EHOSTDOWN : EHOSTUNREACH);
	}

	switch (dst->sa_family) {

#ifdef INET
	case AF_INET:
		if (!arpresolve(ac, rt, m, dst, edst))
			return (0);	/* if not yet resolved */
		/* If broadcasting on a simplex interface, loopback a copy */
		if ((m->m_flags & M_BCAST) && (ifp->if_flags & IFF_SIMPLEX) &&
		    m_tag_find(m, PACKET_TAG_PF_ROUTED, NULL) == NULL)
			mcopy = m_copy(m, 0, (int)M_COPYALL);
		type = htons(ETHERTYPE_IP);
		break;
#endif
#ifdef INET6
	case AF_INET6:
		if (!nd6_storelladdr(ifp, rt, m, dst, (u_char *)edst))
			return (0);	/* if not yet resolved */
		type = htons(ETHERTYPE_IPV6);
		break;
#endif
#if 0	/*NRL IPv6*/
#ifdef INET6
	case AF_INET6:
		/*
		 * The bottom line here is to either queue the outgoing packet
		 * in the discovery engine, or fill in edst with something
		 * that'll work.
		 */
		if (m->m_flags & M_MCAST) {
			/*
			 * If multicast dest., then use IPv6 -> Ethernet mcast
			 * mapping.  Really simple.
			 */
			ETHER_MAP_IN6_MULTICAST(((struct sockaddr_in6 *)dst)->sin6_addr,
			    edst);
		} else {
			/* Do unicast neighbor discovery stuff. */
			if (!ipv6_discov_resolve(ifp, rt, m, dst, edst))
 				return 0;
		}
		type = htons(ETHERTYPE_IPV6);
		break;
#endif /* INET6 */
#endif
#ifdef IPX
	case AF_IPX:
		type = htons(ETHERTYPE_IPX);
 		bcopy((caddr_t)&(((struct sockaddr_ipx*)dst)->sipx_addr.ipx_host),
		    (caddr_t)edst, sizeof (edst));
		/* If broadcasting on a simplex interface, loopback a copy */
		if ((m->m_flags & M_BCAST) && (ifp->if_flags & IFF_SIMPLEX))
			mcopy = m_copy(m, 0, (int)M_COPYALL);
		break;
#endif
#ifdef NS
	case AF_NS:
		type = htons(ETHERTYPE_NS);
 		bcopy((caddr_t)&(((struct sockaddr_ns *)dst)->sns_addr.x_host),
		    (caddr_t)edst, sizeof (edst));
		if (!bcmp((caddr_t)edst, (caddr_t)&ns_thishost, sizeof(edst)))
			return (looutput(ifp, m, dst, rt));
		/* If broadcasting on a simplex interface, loopback a copy */
		if ((m->m_flags & M_BCAST) && (ifp->if_flags & IFF_SIMPLEX))
			mcopy = m_copy(m, 0, (int)M_COPYALL);
		break;
#endif
#ifdef	CCITT
/*	case AF_NSAP: */
	case AF_CCITT: {
		struct sockaddr_dl *sdl = 
			(struct sockaddr_dl *) rt->rt_gateway;

		if (sdl && sdl->sdl_family == AF_LINK
		    && sdl->sdl_alen > 0) {
			bcopy(LLADDR(sdl), (char *)edst,
				sizeof(edst));
		} else goto bad; /* Not a link interface ? Funny ... */
		if ((ifp->if_flags & IFF_SIMPLEX) && (*edst & 1) &&
		    (mcopy = m_copy(m, 0, (int)M_COPYALL))) {
			M_PREPEND(mcopy, sizeof (*fh), M_DONTWAIT);
			if (mcopy) {
				fh = mtod(mcopy, struct fddi_header *);
				bcopy((caddr_t)edst,
				      (caddr_t)fh->fddi_dhost, sizeof (edst));
				bcopy((caddr_t)ac->ac_enaddr,
				      (caddr_t)fh->fddi_shost, sizeof (edst));
				fh->fddi_fc = FDDIFC_LLC_ASYNC|FDDIFC_LLC_PRIO4;
			}
		}
		type = 0;
#ifdef LLC_DEBUG
		{
			int i;
			struct llc *l = mtod(m, struct llc *);

			printf("fddi_output: sending LLC2 pkt to: ");
			for (i=0; i<6; i++)
				printf("%x ", edst[i] & 0xff);
			printf(" len 0x%x dsap 0x%x ssap 0x%x control 0x%x\n", 
			    m->m_pkthdr.len, l->llc_dsap & 0xff, l->llc_ssap &0xff,
			    l->llc_control & 0xff);

		}
#endif /* LLC_DEBUG */
		} break;
#endif /* CCITT */	

	case pseudo_AF_HDRCMPLT:
	{
		struct fddi_header *fh = (struct fddi_header *)dst->sa_data;
		hdrcmplt = 1;
		bcopy((caddr_t)fh->fddi_shost, (caddr_t)esrc, sizeof (esrc));
		/* FALLTHROUGH */
	}

	case AF_UNSPEC:
	{
		struct ether_header *eh;
		eh = (struct ether_header *)dst->sa_data;
 		bcopy((caddr_t)eh->ether_dhost, (caddr_t)edst, sizeof (edst));
		if (*edst & 1)
			m->m_flags |= (M_BCAST|M_MCAST);
		type = eh->ether_type;
		break;
	}

#if NBPFILTER > 0
	case AF_IMPLINK:
	{
		fh = mtod(m, struct fddi_header *);
		error = EPROTONOSUPPORT;
		switch (fh->fddi_fc & (FDDIFC_C|FDDIFC_L|FDDIFC_F)) {
			case FDDIFC_LLC_ASYNC: {
				/* legal priorities are 0 through 7 */
				if ((fh->fddi_fc & FDDIFC_Z) > 7)
			        	goto bad;
				break;
			}
			case FDDIFC_LLC_SYNC: {
				/* FDDIFC_Z bits reserved, must be zero */
				if (fh->fddi_fc & FDDIFC_Z)
					goto bad;
				break;
			}
			case FDDIFC_SMT: {
				/* FDDIFC_Z bits must be non zero */
				if ((fh->fddi_fc & FDDIFC_Z) == 0)
					goto bad;
				break;
			}
			default: {
				/* anything else is too dangerous */
               	 		goto bad;
			}
		}
		error = 0;
		if (fh->fddi_dhost[0] & 1)
			m->m_flags |= (M_BCAST|M_MCAST);
		goto queue_it;
	}
#endif
	default:
		printf("%s: can't handle af%d\n", ifp->if_xname,
			dst->sa_family);
		senderr(EAFNOSUPPORT);
	}

	if (mcopy)
		(void) looutput(ifp, mcopy, dst, rt);

	if (type != 0) {
		struct llc *l;
		M_PREPEND(m, sizeof (struct llc), M_DONTWAIT);
		if (m == 0)
			senderr(ENOBUFS);
		l = mtod(m, struct llc *);
		l->llc_control = LLC_UI;
		l->llc_dsap = l->llc_ssap = LLC_SNAP_LSAP;
		l->llc_snap.org_code[0] = l->llc_snap.org_code[1] = l->llc_snap.org_code[2] = 0;
		bcopy((caddr_t) &type, (caddr_t) &l->llc_snap.ether_type,
			sizeof(u_short));
	}
	/*
	 * Add local net header.  If no space in first mbuf,
	 * allocate another.
	 */
	M_PREPEND(m, sizeof (struct fddi_header), M_DONTWAIT);
	if (m == 0)
		senderr(ENOBUFS);
	fh = mtod(m, struct fddi_header *);
	fh->fddi_fc = FDDIFC_LLC_ASYNC|FDDIFC_LLC_PRIO4;
 	bcopy((caddr_t)edst, (caddr_t)fh->fddi_dhost, sizeof (edst));
#if NBPFILTER > 0
  queue_it:
#endif
	if (hdrcmplt)
		bcopy((caddr_t)esrc, (caddr_t)fh->fddi_shost,
		    sizeof(fh->fddi_shost));
	else
 		bcopy((caddr_t)ac->ac_enaddr, (caddr_t)fh->fddi_shost,
		    sizeof(fh->fddi_shost));
#if NCARP > 0
	if (ifp->if_carp) { 
		error = carp_fix_lladdr(ifp, m, NULL, NULL);
		if (error)
			goto bad;
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
	ifp->if_obytes += len;
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
 * Process a received FDDI packet;
 * the packet is in the mbuf chain m without
 * the fddi header, which is provided separately.
 */
void
fddi_input(ifp, fh, m)
	struct ifnet *ifp;
	struct fddi_header *fh;
	struct mbuf *m;
{
	struct ifqueue *inq;
	struct llc *l;
	int s;

	if ((ifp->if_flags & IFF_UP) == 0) {
		m_freem(m);
		return;
	}
	ifp->if_ibytes += m->m_pkthdr.len + sizeof (*fh);
	if (bcmp((caddr_t)fddibroadcastaddr, (caddr_t)fh->fddi_dhost,
	    sizeof(fddibroadcastaddr)) == 0)
		m->m_flags |= M_BCAST;
	else if (fh->fddi_dhost[0] & 1)
		m->m_flags |= M_MCAST;
	if (m->m_flags & (M_BCAST|M_MCAST))
		ifp->if_imcasts++;

	l = mtod(m, struct llc *);
	switch (l->llc_dsap) {
#if defined(INET) || defined(IPX) || defined(NS) || defined(DECNET) || defined(INET6)
	case LLC_SNAP_LSAP:
	{
		u_int16_t etype;
		if (l->llc_control != LLC_UI || l->llc_ssap != LLC_SNAP_LSAP)
			goto dropanyway;
		if (l->llc_snap.org_code[0] != 0 || l->llc_snap.org_code[1] != 0|| l->llc_snap.org_code[2] != 0)
			goto dropanyway;
		etype = ntohs(l->llc_snap.ether_type);
		m_adj(m, LLC_SNAPFRAMELEN);
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
#endif
#ifdef INET6
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
#ifdef DECNET
		case ETHERTYPE_DECNET:
			schednetisr(NETISR_DECNET);
			inq = &decnetintrq;
			break;
#endif
		default:
			/* printf("fddi_input: unknown protocol 0x%x\n", etype); */
			ifp->if_noproto++;
			goto dropanyway;
		}
		break;
	}
#endif /* INET || IPX || NS || DECNET */
#ifdef CCITT
	case LLC_X25_LSAP:
	{
		M_PREPEND(m, sizeof(struct sdl_hdr) , M_DONTWAIT);
		if (m == 0)
			return;
		if ( !sdl_sethdrif(ifp, fh->fddi_shost, LLC_X25_LSAP,
				    fh->fddi_dhost, LLC_X25_LSAP, 6, 
				    mtod(m, struct sdl_hdr *)))
			panic("ETHER cons addr failure");
		mtod(m, struct sdl_hdr *)->sdlhdr_len = m->m_pkthdr.len - sizeof(struct sdl_hdr);
#ifdef LLC_DEBUG
		printf("llc packet\n");
#endif /* LLC_DEBUG */
		schednetisr(NETISR_CCITT);
		inq = &llcintrq;
		break;
	}
#endif /* CCITT */
		
	default:
		/* printf("fddi_input: unknown dsap 0x%x\n", l->llc_dsap); */
		ifp->if_noproto++;
	dropanyway:
		m_freem(m);
		return;
	}

	s = splimp();
	IF_INPUT_ENQUEUE(inq, m);
	splx(s);
}
/*
 * Perform common duties while attaching to interface list
 */
void
fddi_ifattach(ifp)
	struct ifnet *ifp;
{

	ifp->if_type = IFT_FDDI;
	ifp->if_addrlen = 6;
	ifp->if_hdrlen = 21;
	ifp->if_mtu = FDDIMTU;
	ifp->if_output = fddi_output;
	if_alloc_sadl(ifp);
	bcopy((caddr_t)((struct arpcom *)ifp)->ac_enaddr,
	      LLADDR(ifp->if_sadl), ifp->if_addrlen);
}
