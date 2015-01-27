/*	$OpenBSD: if_loop.c,v 1.63 2015/01/27 10:20:31 mpi Exp $	*/
/*	$NetBSD: if_loop.c,v 1.15 1996/05/07 02:40:33 thorpej Exp $	*/

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
 *
 *	@(#)if_loop.c	8.1 (Berkeley) 6/10/93
 */

/*
 *	@(#)COPYRIGHT	1.1 (NRL) 17 January 1995
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

/*
 * Loopback interface driver for protocol testing and timing.
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/time.h>


#include <net/if.h>
#include <net/if_var.h>
#include <net/if_types.h>
#include <net/netisr.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/ip.h>

#ifdef INET6
#include <netinet/ip6.h>
#endif

#ifdef MPLS
#include <netmpls/mpls.h>
#endif

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#define	LOMTU	32768

int	loop_clone_create(struct if_clone *, int);
int	loop_clone_destroy(struct ifnet *);

struct if_clone loop_cloner =
    IF_CLONE_INITIALIZER("lo", loop_clone_create, loop_clone_destroy);

/* ARGSUSED */
void
loopattach(int n)
{
	if (loop_clone_create(&loop_cloner, 0))
		panic("unable to create lo0");

	if_clone_attach(&loop_cloner);
}

int
loop_clone_create(struct if_clone *ifc, int unit)
{
	struct ifnet *ifp;

	ifp = malloc(sizeof(*ifp), M_DEVBUF, M_NOWAIT|M_ZERO);
	if (ifp == NULL)
		return (ENOMEM);

	snprintf(ifp->if_xname, sizeof ifp->if_xname, "lo%d", unit);
	ifp->if_softc = NULL;
	ifp->if_mtu = LOMTU;
	ifp->if_flags = IFF_LOOPBACK | IFF_MULTICAST;
	ifp->if_ioctl = loioctl;
	ifp->if_output = looutput;
	ifp->if_type = IFT_LOOP;
	ifp->if_hdrlen = sizeof(u_int32_t);
	ifp->if_addrlen = 0;
	IFQ_SET_READY(&ifp->if_snd);
	if (unit == 0) {
		lo0ifp = ifp;
		if_attachhead(ifp);
		if_addgroup(lo0ifp, ifc->ifc_name);
	} else
		if_attach(ifp);
	if_alloc_sadl(ifp);
#if NBPFILTER > 0
	bpfattach(&ifp->if_bpf, ifp, DLT_LOOP, sizeof(u_int32_t));
#endif
	return (0);
}

int
loop_clone_destroy(struct ifnet *ifp)
{
	if (ifp == lo0ifp)
		return (EPERM);

	if_detach(ifp);

	free(ifp, M_DEVBUF, sizeof(*ifp));
	return (0);
}

int
looutput(struct ifnet *ifp, struct mbuf *m, struct sockaddr *dst,
    struct rtentry *rt)
{
	int s, isr;
	struct ifqueue *ifq = 0;

	if ((m->m_flags & M_PKTHDR) == 0)
		panic("looutput: no header mbuf");
#if NBPFILTER > 0
	/*
	 * only send packets to bpf if they are real loopback packets;
	 * looutput() is also called for SIMPLEX interfaces to duplicate
	 * packets for local use. But don't dup them to bpf.
	 */
	if (ifp->if_bpf && (ifp->if_flags & IFF_LOOPBACK))
		bpf_mtap_af(ifp->if_bpf, dst->sa_family, m, BPF_DIRECTION_OUT);
#endif
	m->m_pkthdr.rcvif = ifp;

	if (rt && rt->rt_flags & (RTF_REJECT|RTF_BLACKHOLE)) {
		m_freem(m);
		return (rt->rt_flags & RTF_BLACKHOLE ? 0 :
			rt->rt_flags & RTF_HOST ? EHOSTUNREACH : ENETUNREACH);
	}

	ifp->if_opackets++;
	ifp->if_obytes += m->m_pkthdr.len;
	switch (dst->sa_family) {

	case AF_INET:
		ifq = &ipintrq;
		isr = NETISR_IP;
		break;
#ifdef INET6
	case AF_INET6:
		ifq = &ip6intrq;
		isr = NETISR_IPV6;
		break;
#endif /* INET6 */
#ifdef MPLS
	case AF_MPLS:
		ifq = &mplsintrq;
		isr = NETISR_MPLS;
		break;
#endif /* MPLS */
	default:
		printf("%s: can't handle af%d\n", ifp->if_xname,
			dst->sa_family);
		m_freem(m);
		return (EAFNOSUPPORT);
	}
	s = splnet();
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
void
lortrequest(int cmd, struct rtentry *rt)
{
	if (rt && rt->rt_rmx.rmx_mtu == 0)
		rt->rt_rmx.rmx_mtu = LOMTU;
}

/*
 * Process an ioctl request.
 */
/* ARGSUSED */
int
loioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct ifaddr *ifa;
	struct ifreq *ifr;
	int error = 0;

	switch (cmd) {

	case SIOCSIFADDR:
		ifp->if_flags |= IFF_RUNNING;
		if_up(ifp);		/* send up RTM_IFINFO */

		ifa = (struct ifaddr *)data;
		if (ifa != 0)
			ifa->ifa_rtrequest = lortrequest;
		/*
		 * Everything else is done at a higher level.
		 */
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		break;

	case SIOCSIFMTU:
		ifr = (struct ifreq *)data;
		ifp->if_mtu = ifr->ifr_mtu;
		break;

	default:
		error = ENOTTY;
	}
	return (error);
}
