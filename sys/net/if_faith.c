/*	$OpenBSD: if_faith.c,v 1.17 2003/12/16 20:33:25 markus Exp $	*/
/*
 * Copyright (c) 1982, 1986, 1993
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
 */
/*
 * derived from 
 *	@(#)if_loop.c	8.1 (Berkeley) 6/10/93
 * Id: if_loop.c,v 1.22 1996/06/19 16:24:10 wollman Exp
 */

/*
 * Loopback interface driver for protocol testing and timing.
 */
#include "faith.h"

#if NFAITH > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/netisr.h>
#include <net/route.h>
#include <net/bpf.h>

#ifdef	INET
#include <netinet/in.h>
#include <netinet/in_var.h>
#endif

#ifdef INET6
#ifndef INET
#include <netinet/in.h>
#endif
#include <netinet6/in6_var.h>
#endif

#include "bpfilter.h"

static int faithioctl(struct ifnet *, u_long, caddr_t);
int faithoutput(struct ifnet *, struct mbuf *, struct sockaddr *,
	struct rtentry *);
static void faithrtrequest(int, struct rtentry *, struct rt_addrinfo *);

void	faithattach(int);
int	faith_clone_create(struct if_clone *, int);
int	faith_clone_destroy(struct ifnet *ifp);

struct if_clone faith_cloner =
    IF_CLONE_INITIALIZER("faith", faith_clone_create, faith_clone_destroy);

#define	FAITHMTU	1500

/* ARGSUSED */
void
faithattach(faith)
	int faith;
{
	if_clone_attach(&faith_cloner);
}

int
faith_clone_create(ifc, unit)
	struct if_clone *ifc;
	int unit;
{
	struct ifnet *ifp;

	ifp = malloc(sizeof(*ifp), M_DEVBUF, M_NOWAIT);
	if (!ifp)
		return (ENOMEM);
	bzero(ifp, sizeof(*ifp));
	snprintf(ifp->if_xname, sizeof ifp->if_xname, "%s%d", ifc->ifc_name, 
	    unit);
	ifp->if_mtu = FAITHMTU;
	/* Change to BROADCAST experimentaly to announce its prefix. */
	ifp->if_flags = /* IFF_LOOPBACK */ IFF_BROADCAST | IFF_MULTICAST;
	ifp->if_ioctl = faithioctl;
	ifp->if_output = faithoutput;
	ifp->if_type = IFT_FAITH;
	ifp->if_hdrlen = 0;
	ifp->if_addrlen = 0;
	if_attach(ifp);
	if_alloc_sadl(ifp);
#if NBPFILTER > 0
	bpfattach(&ifp->if_bpf, ifp, DLT_NULL, sizeof(u_int));
#endif
	return (0);
}

int
faith_clone_destroy(ifp)
	struct ifnet *ifp;
{
#if NBPFILTER > 0
	bpfdetach(ifp);
#endif  
	if_detach(ifp);

	free(ifp, M_DEVBUF);
	return (0);
}

int
faithoutput(ifp, m, dst, rt)
	struct ifnet *ifp;
	struct mbuf *m;
	struct sockaddr *dst;
	struct rtentry *rt;
{
	int s, isr;
	struct ifqueue *ifq = 0;

	if ((m->m_flags & M_PKTHDR) == 0)
		panic("faithoutput no HDR");
#if NBPFILTER > 0
	/* BPF write needs to be handled specially */
	if (dst->sa_family == AF_UNSPEC) {
		dst->sa_family = *(mtod(m, int *));
		m->m_len -= sizeof(int);
		m->m_pkthdr.len -= sizeof(int);
		m->m_data += sizeof(int);
	}

	if (ifp->if_bpf) {
		/*
		 * We need to prepend the address family as
		 * a four byte field.  Cons up a faith header
		 * to pacify bpf.  This is safe because bpf
		 * will only read from the mbuf (i.e., it won't
		 * try to free it or keep a pointer a to it).
		 */
		struct mbuf m0;
		u_int32_t af = dst->sa_family;

		m0.m_flags = 0;
		m0.m_next = m;
		m0.m_len = 4;
		m0.m_data = (char *)&af;

		bpf_mtap(ifp->if_bpf, &m0);
	}
#endif

	if (rt && rt->rt_flags & (RTF_REJECT|RTF_BLACKHOLE)) {
		m_freem(m);
		return (rt->rt_flags & RTF_BLACKHOLE ? 0 :
		        rt->rt_flags & RTF_HOST ? EHOSTUNREACH : ENETUNREACH);
	}
	ifp->if_opackets++;
	ifp->if_obytes += m->m_pkthdr.len;
	switch (dst->sa_family) {
#ifdef INET
	case AF_INET:
		ifq = &ipintrq;
		isr = NETISR_IP;
		break;
#endif
#ifdef INET6
	case AF_INET6:
		ifq = &ip6intrq;
		isr = NETISR_IPV6;
		break;
#endif
	default:
		m_freem(m);
		return EAFNOSUPPORT;
	}

	/* XXX do we need more sanity checks? */

	m->m_pkthdr.rcvif = ifp;
	s = splimp();
	if (IF_QFULL(ifq)) {
		IF_DROP(ifq);
		m_freem(m);
		splx(s);
		return (ENOBUFS);
	}
	IF_ENQUEUE(ifq, m);
	schednetisr(isr);
	ifp->if_ipackets++;
	ifp->if_ibytes += m->m_pkthdr.len;
	splx(s);
	return (0);
}

/* ARGSUSED */
static void
faithrtrequest(cmd, rt, info)
	int cmd;
	struct rtentry *rt;
	struct rt_addrinfo *info;
{
	if (rt)
		rt->rt_rmx.rmx_mtu = rt->rt_ifp->if_mtu; /* for ISO */
}

/*
 * Process an ioctl request.
 */
/* ARGSUSED */
static int
faithioctl(ifp, cmd, data)
	struct ifnet *ifp;
	u_long cmd;
	caddr_t data;
{
	struct ifaddr *ifa;
	struct ifreq *ifr = (struct ifreq *)data;
	int error = 0;

	switch (cmd) {

	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP | IFF_RUNNING;
		ifa = (struct ifaddr *)data;
		ifa->ifa_rtrequest = faithrtrequest;
		/*
		 * Everything else is done at a higher level.
		 */
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (ifr == 0) {
			error = EAFNOSUPPORT;		/* XXX */
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

	case SIOCSIFFLAGS:
		break;

	default:
		error = EINVAL;
	}
	return (error);
}
#endif /* NFAITH > 0 */
