/*	$OpenBSD: in_gif.c,v 1.38 2010/05/11 09:36:07 claudio Exp $	*/
/*	$KAME: in_gif.c,v 1.50 2001/01/22 07:27:16 itojun Exp $	*/

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

#include "pf.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/mbuf.h>

#include <net/if.h>
#include <net/route.h>
#include <net/if_gif.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/in_gif.h>
#include <netinet/ip_ipsp.h>

#ifdef INET6
#include <netinet/ip6.h>
#endif

#include "gif.h"
#include "bridge.h"
#if NBRIDGE > 0
#include <netinet/ip_ether.h>
#endif

#if NPF > 0
#include <net/pfvar.h>
#endif

int
in_gif_output(struct ifnet *ifp, int family, struct mbuf **m0)
{
	struct gif_softc *sc = (struct gif_softc*)ifp;
	struct sockaddr_in *sin_src = (struct sockaddr_in *)sc->gif_psrc;
	struct sockaddr_in *sin_dst = (struct sockaddr_in *)sc->gif_pdst;
	struct tdb tdb;
	struct xformsw xfs;
	int error;
	struct mbuf *m = *m0;

	if (sin_src == NULL || sin_dst == NULL ||
	    sin_src->sin_family != AF_INET ||
	    sin_dst->sin_family != AF_INET) {
		m_freem(m);
		return EAFNOSUPPORT;
	}

#ifdef DIAGNOSTIC
	if (ifp->if_rdomain != rtable_l2(m->m_pkthdr.rdomain)) {
		printf("%s: trying to send packet on wrong domain. "
		    "if %d vs. mbuf %d, AF %d\n", ifp->if_xname,
		    ifp->if_rdomain, rtable_l2(m->m_pkthdr.rdomain));
	}
#endif

	/* setup dummy tdb.  it highly depends on ipip_output() code. */
	bzero(&tdb, sizeof(tdb));
	bzero(&xfs, sizeof(xfs));
	tdb.tdb_src.sin.sin_family = AF_INET;
	tdb.tdb_src.sin.sin_len = sizeof(struct sockaddr_in);
	tdb.tdb_src.sin.sin_addr = sin_src->sin_addr;
	tdb.tdb_dst.sin.sin_family = AF_INET;
	tdb.tdb_dst.sin.sin_len = sizeof(struct sockaddr_in);
	tdb.tdb_dst.sin.sin_addr = sin_dst->sin_addr;
	tdb.tdb_xform = &xfs;
	xfs.xf_type = -1;	/* not XF_IP4 */

	switch (family) {
	case AF_INET:
		break;
#ifdef INET6
	case AF_INET6:
		break;
#endif
#if NBRIDGE > 0
	case AF_LINK:
		break;
#endif
#if MPLS
	case AF_MPLS:
		break;
#endif
	default:
#ifdef DEBUG
	        printf("in_gif_output: warning: unknown family %d passed\n",
			family);
#endif
		m_freem(m);
		return EAFNOSUPPORT;
	}

	/* encapsulate into IPv4 packet */
	*m0 = NULL;
#if NBRIDGE > 0
	if (family == AF_LINK)
		error = etherip_output(m, &tdb, m0, IPPROTO_ETHERIP);
	else
#endif /* NBRIDGE */
#ifdef MPLS
	if (family == AF_MPLS)
		error = etherip_output(m, &tdb, m0, IPPROTO_MPLS);
	else
#endif
	error = ipip_output(m, &tdb, m0, 0, 0);
	if (error)
		return error;
	else if (*m0 == NULL)
		return EFAULT;

	m = *m0;

	m->m_pkthdr.rdomain = sc->gif_rtableid;
#if NPF > 0
	pf_pkt_addr_changed(m);
#endif
	return 0;
}

void
in_gif_input(struct mbuf *m, ...)
{
	int off;
	struct gif_softc *sc;
	struct ifnet *gifp = NULL;
	struct ip *ip;
	va_list ap;

	va_start(ap, m);
	off = va_arg(ap, int);
	va_end(ap);

	/* IP-in-IP header is caused by tunnel mode, so skip gif lookup */
	if (m->m_flags & M_TUNNEL) {
		m->m_flags &= ~M_TUNNEL;
		goto inject;
	}

	ip = mtod(m, struct ip *);

	/* this code will be soon improved. */
	LIST_FOREACH(sc, &gif_softc_list, gif_list) {
		if (sc->gif_psrc == NULL || sc->gif_pdst == NULL ||
		    sc->gif_psrc->sa_family != AF_INET ||
		    sc->gif_pdst->sa_family != AF_INET ||
		    rtable_l2(sc->gif_rtableid) !=
		    rtable_l2(m->m_pkthdr.rdomain)) {
			continue;
		}

		if ((sc->gif_if.if_flags & IFF_UP) == 0)
			continue;

		if (in_hosteq(satosin(sc->gif_psrc)->sin_addr, ip->ip_dst) &&
		    in_hosteq(satosin(sc->gif_pdst)->sin_addr, ip->ip_src)) {
			gifp = &sc->gif_if;
			break;
		}
	}

	if (gifp) {
		m->m_pkthdr.rcvif = gifp;
		m->m_pkthdr.rdomain = gifp->if_rdomain;
		gifp->if_ipackets++;
		gifp->if_ibytes += m->m_pkthdr.len;
		/* We have a configured GIF */
		ipip_input(m, off, gifp, ip->ip_p);
		return;
	}

inject:
	ip4_input(m, off); /* No GIF interface was configured */
	return;
}
