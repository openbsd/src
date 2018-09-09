/*	$OpenBSD: if_loop.c,v 1.88 2018/09/09 10:11:41 henning Exp $	*/
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 *	This product includes software developed at the Information
 *	Technology Division, US Naval Research Laboratory.
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
#include <net/rtable.h>
#include <net/route.h>

#include <netinet/in.h>

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

int	loioctl(struct ifnet *, u_long, caddr_t);
void	loopattach(int);
void	lortrequest(struct ifnet *, int, struct rtentry *);
int	loinput(struct ifnet *, struct mbuf *, void *);
int	looutput(struct ifnet *,
	    struct mbuf *, struct sockaddr *, struct rtentry *);

int	loop_clone_create(struct if_clone *, int);
int	loop_clone_destroy(struct ifnet *);

struct if_clone loop_cloner =
    IF_CLONE_INITIALIZER("lo", loop_clone_create, loop_clone_destroy);

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

	ifp = malloc(sizeof(*ifp), M_DEVBUF, M_WAITOK|M_ZERO);
	snprintf(ifp->if_xname, sizeof ifp->if_xname, "lo%d", unit);
	ifp->if_softc = NULL;
	ifp->if_mtu = LOMTU;
	ifp->if_flags = IFF_LOOPBACK | IFF_MULTICAST;
	ifp->if_xflags = IFXF_CLONED;
	ifp->if_rtrequest = lortrequest;
	ifp->if_ioctl = loioctl;
	ifp->if_output = looutput;
	ifp->if_type = IFT_LOOP;
	ifp->if_hdrlen = sizeof(u_int32_t);
	if (unit == 0) {
		if_attachhead(ifp);
		if_addgroup(ifp, ifc->ifc_name);
		rtable_l2set(0, 0, ifp->if_index);
	} else
		if_attach(ifp);
	if_alloc_sadl(ifp);
#if NBPFILTER > 0
	bpfattach(&ifp->if_bpf, ifp, DLT_LOOP, sizeof(u_int32_t));
#endif
	if_ih_insert(ifp, loinput, NULL);
	return (0);
}

int
loop_clone_destroy(struct ifnet *ifp)
{
	struct ifnet	*p;

	if (ifp->if_index == rtable_loindex(ifp->if_rdomain)) {
		/* rdomain 0 always needs a loopback */
		if (ifp->if_rdomain == 0)
			return (EPERM);

		/* if there is any other interface in this rdomain, deny */
		NET_LOCK();
		TAILQ_FOREACH(p, &ifnet, if_list) {
			if (p->if_rdomain != ifp->if_rdomain)
				continue;
			if (p->if_index == ifp->if_index)
				continue;
			NET_UNLOCK();
			return (EBUSY);
		}
		NET_UNLOCK();

		rtable_l2set(ifp->if_rdomain, 0, 0);
	}

	if_ih_remove(ifp, loinput, NULL);
	if_detach(ifp);

	free(ifp, M_DEVBUF, sizeof(*ifp));
	return (0);
}

int
loinput(struct ifnet *ifp, struct mbuf *m, void *cookie)
{
	int error;

	if ((m->m_flags & M_PKTHDR) == 0)
		panic("%s: no header mbuf", __func__);

	error = if_input_local(ifp, m, m->m_pkthdr.ph_family);
	if (error)
		ifp->if_ierrors++;

	return (1);
}

int
looutput(struct ifnet *ifp, struct mbuf *m, struct sockaddr *dst,
    struct rtentry *rt)
{
	if ((m->m_flags & M_PKTHDR) == 0)
		panic("%s: no header mbuf", __func__);

	if (rt && rt->rt_flags & (RTF_REJECT|RTF_BLACKHOLE)) {
		m_freem(m);
		return (rt->rt_flags & RTF_BLACKHOLE ? 0 :
			rt->rt_flags & RTF_HOST ? EHOSTUNREACH : ENETUNREACH);
	}

	/* Use the quick path only once to avoid stack overflow. */
	if ((m->m_flags & M_LOOP) == 0)
		return (if_input_local(ifp, m, dst->sa_family));

	return (if_output_local(ifp, m, dst->sa_family));
}

void
lortrequest(struct ifnet *ifp, int cmd, struct rtentry *rt)
{
	if (rt && rt->rt_mtu == 0)
		rt->rt_mtu = LOMTU;
}

/*
 * Process an ioctl request.
 */
int
loioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct ifreq *ifr;
	int error = 0;

	switch (cmd) {
	case SIOCSIFFLAGS:
		break;

	case SIOCSIFADDR:
		ifp->if_flags |= IFF_RUNNING;
		if_up(ifp);		/* send up RTM_IFINFO */
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
