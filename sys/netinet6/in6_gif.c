/*	$OpenBSD: in6_gif.c,v 1.4 2000/01/12 06:35:04 angelos Exp $	*/

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
 * in6_gif.c
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/protosw.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip_ipsp.h>

#ifdef INET
#include <netinet/ip.h>
#endif

#include <netinet6/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/in6_gif.h>

#ifdef INET6
#include <netinet6/ip6.h>
#endif

#include <netinet/ip_ecn.h>

#include <net/if_gif.h>

#include <net/net_osdep.h>

#ifndef offsetof
#define offsetof(s, e) ((int)&((s *)0)->e)
#endif

int
in6_gif_output(ifp, family, m, rt)
	struct ifnet *ifp;
	int family; /* family of the packet to be encapsulate. */
	struct mbuf *m;
	struct rtentry *rt;
{
	struct gif_softc *sc = (struct gif_softc*)ifp;
        struct sockaddr_in6 *dst = (struct sockaddr_in6 *)&sc->gif_ro6.ro_dst;
	struct sockaddr_in6 *sin6_src = (struct sockaddr_in6 *)sc->gif_psrc;
	struct sockaddr_in6 *sin6_dst = (struct sockaddr_in6 *)sc->gif_pdst;
	struct tdb tdb;
	struct xformsw xfs;
	int error;
	int hlen, poff;
	struct mbuf *mp;

	if (sin6_src == NULL || sin6_dst == NULL ||
	    sin6_src->sin6_family != AF_INET6 ||
	    sin6_dst->sin6_family != AF_INET6) {
		m_freem(m);
		return EAFNOSUPPORT;
	}

	/* multi-destination mode is not supported */
	if (ifp->if_flags & IFF_LINK0) {
		m_freem(m);
		return ENETUNREACH;
	}

	/* setup dummy tdb.  it highly depends on ipe4_output() code. */
	bzero(&tdb, sizeof(tdb));
	bzero(&xfs, sizeof(xfs));
	tdb.tdb_src.sin6.sin6_family = AF_INET6;
	tdb.tdb_src.sin6.sin6_len = sizeof(struct sockaddr_in6);
	tdb.tdb_src.sin6.sin6_addr = sin6_src->sin6_addr;
	tdb.tdb_dst.sin6.sin6_family = AF_INET6;
	tdb.tdb_dst.sin6.sin6_len = sizeof(struct sockaddr_in6);
	tdb.tdb_dst.sin6.sin6_addr = sin6_dst->sin6_addr;
	tdb.tdb_xform = &xfs;
	xfs.xf_type = -1;	/* not XF_IP4 */

	switch (family) {
#ifdef INET
	case AF_INET:
	    {
		if (m->m_len < sizeof(struct ip)) {
			m = m_pullup(m, sizeof(struct ip));
			if (!m)
				return ENOBUFS;
		}
		hlen = (mtod(m, struct ip *)->ip_hl) << 2;
		poff = offsetof(struct ip, ip_p);
		break;
	    }
#endif
#ifdef INET6
	case AF_INET6:
	    {
		hlen = sizeof(struct ip6_hdr);
		poff = offsetof(struct ip6_hdr, ip6_nxt);
		break;
	    }
#endif
	default:
#ifdef DIAGNOSTIC
		printf("in6_gif_output: warning: unknown family %d passed\n",
			family);
#endif
		m_freem(m);
		return EAFNOSUPPORT;
	}
	
	/* encapsulate into IPv6 packet */
	mp = NULL;
	error = ipe4_output(m, &tdb, &mp, hlen, poff);
	if (error)
	        return error;
	else if (mp == NULL)
	        return EFAULT;

	m = mp;

	/* See if out cached route remains the same */
	if (dst->sin6_family != sin6_dst->sin6_family ||
	     !IN6_ARE_ADDR_EQUAL(&dst->sin6_addr, &sin6_dst->sin6_addr)) {
		/* cache route doesn't match */
		bzero(dst, sizeof(*dst));
		dst->sin6_family = sin6_dst->sin6_family;
		dst->sin6_len = sizeof(struct sockaddr_in6);
		dst->sin6_addr = sin6_dst->sin6_addr;
		if (sc->gif_ro6.ro_rt) {
			RTFREE(sc->gif_ro6.ro_rt);
			sc->gif_ro6.ro_rt = NULL;
		}
	}

	if (sc->gif_ro6.ro_rt == NULL) {
		rtalloc((struct route *)&sc->gif_ro6);
		if (sc->gif_ro6.ro_rt == NULL) {
			m_freem(m);
			return ENETUNREACH;
		}
	}
	
	return(ip6_output(m, 0, &sc->gif_ro6, 0, 0, NULL));
}

int in6_gif_input(mp, offp, proto)
	struct mbuf **mp;
	int *offp, proto;
{
	struct mbuf *m = *mp;
	struct gif_softc *sc;
	struct ifnet *gifp = NULL;
	struct ip6_hdr *ip6;
	int i;

	/* XXX What if we run transport-mode IPsec to protect gif tunnel ? */
	if (m->m_flags & (M_AUTH | M_CONF))
	        goto inject;

	ip6 = mtod(m, struct ip6_hdr *);

#define satoin6(sa)	(((struct sockaddr_in6 *)(sa))->sin6_addr)
	for (i = 0, sc = gif; i < ngif; i++, sc++) {
		if (sc->gif_psrc == NULL ||
		    sc->gif_pdst == NULL ||
		    sc->gif_psrc->sa_family != AF_INET6 ||
		    sc->gif_pdst->sa_family != AF_INET6) {
			continue;
		}

		if ((sc->gif_if.if_flags & IFF_UP) == 0)
			continue;

		if ((sc->gif_if.if_flags & IFF_LINK0) &&
		    IN6_ARE_ADDR_EQUAL(&satoin6(sc->gif_psrc),
				       &ip6->ip6_dst) &&
		    IN6_IS_ADDR_UNSPECIFIED(&satoin6(sc->gif_pdst))) {
			gifp = &sc->gif_if;
			continue;
		}

		if (IN6_ARE_ADDR_EQUAL(&satoin6(sc->gif_psrc),
				       &ip6->ip6_dst) &&
		    IN6_ARE_ADDR_EQUAL(&satoin6(sc->gif_pdst),
				       &ip6->ip6_src)) {
			gifp = &sc->gif_if;
			break;
		}
	}

	if (gifp && (m->m_flags & (M_AUTH | M_CONF)) == 0)
	        m->m_pkthdr.rcvif = gifp;

inject:
	ip4_input(m, *offp);
	return IPPROTO_DONE;
}
