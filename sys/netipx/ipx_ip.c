/*	$OpenBSD: ipx_ip.c,v 1.19 2004/09/20 23:10:47 drahn Exp $	*/

/*-
 *
 * Copyright (c) 1996 Michael Shalayeff
 * Copyright (c) 1995, Mike Mitchell
 * Copyright (c) 1984, 1985, 1986, 1987, 1993
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
 *	@(#)ipx_ip.c
 *
 * from FreeBSD Id: ipx_ip.c,v 1.7 1996/03/11 15:13:50 davidg Exp
 */

/*
 * Software interface driver for encapsulating IPX in IP.
 */

#ifdef IPXIP
#ifndef INET
#error The option IPXIP requires option INET.
#endif

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/protosw.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/netisr.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>

#include <netipx/ipx.h>
#include <netipx/ipx_if.h>
#include <netipx/ipx_ip.h>

#include <sys/stdarg.h>

struct ifnet ipxipif;
struct ifnet_en *ipxip_list; /* list of all hosts and gateways or broadcast addrs */

void
ipxipprotoinit(void)
{
	(void) ipxipattach();
}

struct ifnet_en *
ipxipattach(void)
{
	struct ifnet_en *m;
	struct ifnet *ifp;

	if (ipxipif.if_mtu == 0) {
		ifp = &ipxipif;
		snprintf(ifp->if_xname, sizeof ifp->if_xname, "ipx0");
		ifp->if_mtu = LOMTU;
		ifp->if_ioctl = ipxipioctl;
		ifp->if_output = ipxipoutput;
		ifp->if_start = ipxipstart;
		ifp->if_flags = IFF_POINTOPOINT;
		ifp->if_type = IFT_NSIP;
	}

	MALLOC((m), struct ifnet_en *, sizeof(*m), M_PCB, M_NOWAIT);
	if (m == NULL)
		return (NULL);
	bzero(m, sizeof(*m));
	m->ifen_next = ipxip_list;
	ipxip_list = m;
	ifp = &m->ifen_ifnet;

	snprintf(ifp->if_xname, sizeof ifp->if_xname, "ipx0");
	ifp->if_mtu = LOMTU;
	ifp->if_ioctl = ipxipioctl;
	ifp->if_output = ipxipoutput;
	ifp->if_start = ipxipstart;
	ifp->if_flags = IFF_POINTOPOINT;
	if_attach(ifp);

	return (m);
}


/*
 * Process an ioctl request.
 */
/* ARGSUSED */
int
ipxipioctl(ifp, cmd, data)
	struct ifnet *ifp;
	u_long cmd;
	caddr_t data;
{
	int error = 0;
	struct ifreq *ifr;

	switch (cmd) {

	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		/* FALLTHROUGH */

	case SIOCSIFDSTADDR:
		/*
		 * Everything else is done at a higher level.
		 */
		break;

	case SIOCSIFFLAGS:
		ifr = (struct ifreq *)data;
		if ((ifr->ifr_flags & IFF_UP) == 0)
			error = ipxip_free(ifp);

	default:
		error = EINVAL;
	}
	return (error);
}

struct mbuf *ipxip_badlen;

void
ipxip_input( struct mbuf *m, ...)
{
	struct ifnet *ifp;
	struct ip *ip;
	struct ipx *ipx;
	struct ifqueue *ifq = &ipxintrq;
	int len, s;
	va_list	ap;

	va_start(ap, m);
	ifp = va_arg(ap, struct ifnet *);
	va_end(ap);

	/*
	 * Get IP and IPX header together in first mbuf.
	 */
	ipxipif.if_ipackets++;
	s = sizeof(struct ip) + sizeof(struct ipx);
	if (((m->m_flags & M_EXT) || m->m_len < s) &&
	    (m = m_pullup(m, s)) == NULL) {
		ipxipif.if_ierrors++;
		return;
	}
	ip = mtod(m, struct ip *);
	if (ip->ip_hl > (sizeof(struct ip) >> 2)) {
		ip_stripoptions(m, (struct mbuf *)0);
		if (m->m_len < s) {
			if ((m = m_pullup(m, s)) == NULL) {
				ipxipif.if_ierrors++;
				return;
			}
			ip = mtod(m, struct ip *);
		}
	}

	/*
	 * Make mbuf data length reflect IPX length.
	 * If not enough data to reflect IPX length, drop.
	 */
	m->m_data += sizeof(struct ip);
	m->m_len -= sizeof(struct ip);
	m->m_pkthdr.len -= sizeof(struct ip);
	ipx = mtod(m, struct ipx *);
	len = ntohs(ipx->ipx_len);
	if (len & 1)
		len++;		/* Preserve Garbage Byte */
	if (ntohs(ip->ip_len) - (ip->ip_hl << 2) != len) {
		if (len > ntohs(ip->ip_len) - (ip->ip_hl << 2)) {
			ipxipif.if_ierrors++;
			if (ipxip_badlen)
				m_freem(ipxip_badlen);
			ipxip_badlen = m;
			return;
		}
		/* Any extra will be trimmed off by the IPX routines */
	}

	/*
	 * Place interface pointer before the data
	 * for the receiving protocol.
	 */
	m->m_pkthdr.rcvif = ifp;
	/*
	 * Deliver to IPX
	 */
	s = splimp();
	if (IF_QFULL(ifq)) {
		IF_DROP(ifq);
		m_freem(m);
		splx(s);
		return;
	}
	IF_ENQUEUE(ifq, m);
	schednetisr(NETISR_IPX);
	splx(s);
	return;
}

/* ARGSUSED */
int
ipxipoutput(ifp, m, dst, rt)
	struct ifnet *ifp;
	struct mbuf *m;
	struct sockaddr *dst;
	struct rtentry *rt;
{
	struct ifnet_en *ifn = (struct ifnet_en *)ifp;
	struct ip *ip;
	struct route *ro = &(ifn->ifen_route);
	int len = 0;
	struct ipx *ipx = mtod(m, struct ipx *);
	int error;

	ifn->ifen_ifnet.if_opackets++;
	ipxipif.if_opackets++;

	/*
	 * Calculate data length and make space
	 * for IP header.
	 */
	len =  ntohs(ipx->ipx_len);
	if (len & 1)
		len++;		/* Preserve Garbage Byte */
	/* following clause not necessary on vax */
	if (3 & (long)m->m_data) {
		/* force longword alignment of ip hdr */
		struct mbuf *m0 = m_gethdr(M_DONTWAIT, MT_HEADER);
		if (m0 == NULL) {
			m_freem(m);
			return (ENOBUFS);
		}
		MH_ALIGN(m0, sizeof(struct ip));
		m0->m_flags = m->m_flags & M_COPYFLAGS;
		m0->m_next = m;
		m0->m_len = sizeof(struct ip);
		m0->m_pkthdr.len = m0->m_len + m->m_len;
		m0->m_pkthdr.tags = m->m_pkthdr.tags;
		m->m_flags &= ~M_PKTHDR;
		m_tag_init(m);
	} else {
		M_PREPEND(m, sizeof(struct ip), M_DONTWAIT);
		if (m == NULL)
			return (ENOBUFS);
	}
	/*
	 * Fill in IP header.
	 */
	ip = mtod(m, struct ip *);
	*(long *)ip = 0;
	ip->ip_p = IPPROTO_IDP;
	ip->ip_src = ifn->ifen_src;
	ip->ip_dst = ifn->ifen_dst;
	if (len + sizeof(struct ip) > IP_MAXPACKET) {
		m_freem(m);
		return EMSGSIZE;
	}
	ip->ip_len = htons(len + sizeof(struct ip));
	ip->ip_ttl = MAXTTL;

	/*
	 * Output final datagram.
	 */
	error = ip_output(m, NULL, ro, SO_BROADCAST, NULL, NULL, NULL);
	if (error) {
		ifn->ifen_ifnet.if_oerrors++;
		ifn->ifen_ifnet.if_ierrors = error;
	}
	return (error);
}

void
ipxipstart(ifp)
	struct ifnet *ifp;
{
	panic("ipxip_start called");
}

struct ifreq ifr_ipxip = {"ipx0"};

int
ipxip_route(m)
	struct mbuf *m;
{
	struct ipxip_req *rq = mtod(m, struct ipxip_req *);
	struct sockaddr_ipx *ipx_dst = (struct sockaddr_ipx *)&rq->rq_ipx;
	struct sockaddr_in *ip_dst = (struct sockaddr_in *)&rq->rq_ip;
	struct route ro;
	struct ifnet_en *ifn;
	struct sockaddr_in *src;

	/*
	 * First, make sure we already have an IPX address.
	 */
	if (ipx_ifaddr.tqh_first == NULL)
		return (EADDRNOTAVAIL);
	/*
	 * Now, determine if we can get to the destination
	 */
	bzero((caddr_t)&ro, sizeof(ro));
	ro.ro_dst = *(struct sockaddr *)ip_dst;
	rtalloc(&ro);
	if (ro.ro_rt == NULL || ro.ro_rt->rt_ifp == NULL) {
		return (ENETUNREACH);
	}

	/*
	 * And see how he's going to get back to us:
	 * i.e., what return ip address do we use?
	 */
	{
		struct in_ifaddr *ia;
		struct ifnet *ifp = ro.ro_rt->rt_ifp;

		for (ia = in_ifaddr.tqh_first; ia; ia = ia->ia_list.tqe_next)
			if (ia->ia_ifp == ifp)
				break;
		if (ia == NULL)
			ia = in_ifaddr.tqh_first;
		if (ia == NULL) {
			RTFREE(ro.ro_rt);
			return (EADDRNOTAVAIL);
		}
		src = (struct sockaddr_in *)&ia->ia_addr;
	}

	/*
	 * Is there a free (pseudo-)interface or space?
	 */
	for (ifn = ipxip_list; ifn; ifn = ifn->ifen_next) {
		if ((ifn->ifen_ifnet.if_flags & IFF_UP) == 0)
			break;
	}
	if (ifn == NULL)
		ifn = ipxipattach();
	if (ifn == NULL) {
		RTFREE(ro.ro_rt);
		return (ENOBUFS);
	}
	ifn->ifen_route = ro;
	ifn->ifen_dst =  ip_dst->sin_addr;
	ifn->ifen_src = src->sin_addr;

	/*
	 * now configure this as a point to point link
	 */
	ifr_ipxip.ifr_dstaddr = * (struct sockaddr *) ipx_dst;
	ipx_control((struct socket *)0, (int)SIOCSIFDSTADDR,
		(caddr_t)&ifr_ipxip, (struct ifnet *)ifn);

	satoipx_addr(ifr_ipxip.ifr_addr).ipx_host =
	    ipx_ifaddr.tqh_first->ia_addr.sipx_addr.ipx_host;

	return (ipx_control((struct socket *)0, (int)SIOCSIFADDR,
			(caddr_t)&ifr_ipxip, (struct ifnet *)ifn));
}

int
ipxip_free(ifp)
	struct ifnet *ifp;
{
	struct ifnet_en *ifn = (struct ifnet_en *)ifp;
	struct route *ro = & ifn->ifen_route;

	if (ro->ro_rt) {
		RTFREE(ro->ro_rt);
		ro->ro_rt = NULL;
	}
	ifp->if_flags &= ~IFF_UP;
	return (0);
}

void *
ipxip_ctlinput(cmd, sa, dummy)
	int cmd;
	struct sockaddr *sa;
	void *dummy;
{
	struct sockaddr_in *sin;

	if ((unsigned)cmd >= PRC_NCMDS)
		return NULL;
	if (sa->sa_family != AF_INET && sa->sa_family != AF_IMPLINK)
		return NULL;
	sin = (struct sockaddr_in *)sa;
	if (sin->sin_addr.s_addr == INADDR_ANY)
		return NULL;

	switch (cmd) {

	case PRC_ROUTEDEAD:
	case PRC_REDIRECT_NET:
	case PRC_REDIRECT_HOST:
	case PRC_REDIRECT_TOSNET:
	case PRC_REDIRECT_TOSHOST:
		ipxip_rtchange(&sin->sin_addr);
		break;
	}
	return NULL;
}

void
ipxip_rtchange(dst)
	struct in_addr *dst;
{
	struct ifnet_en *ifn;

	for (ifn = ipxip_list; ifn; ifn = ifn->ifen_next) {
		if (ifn->ifen_dst.s_addr == dst->s_addr &&
		    ifn->ifen_route.ro_rt) {
			RTFREE(ifn->ifen_route.ro_rt);
			ifn->ifen_route.ro_rt = NULL;
		}
	}
}
#endif /* IPXIP */
