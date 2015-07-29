/*      $OpenBSD: ip_gre.c,v 1.57 2015/07/29 00:04:03 rzalamena Exp $ */
/*	$NetBSD: ip_gre.c,v 1.9 1999/10/25 19:18:11 drochner Exp $ */

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
 * decapsulate tunneled packets and send them on
 * output half is in net/if_gre.[ch]
 * This currently handles IPPROTO_GRE, IPPROTO_MOBILE
 */


#include "gre.h"
#if NGRE > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <net/if.h>
#include <net/netisr.h>
#include <net/route.h>
#include <net/bpf.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip_gre.h>
#include <netinet/if_ether.h>
#include <netinet/in_pcb.h>

#ifdef MPLS
#include <netmpls/mpls.h>
#endif

#include "bpfilter.h"
#include "pf.h"

#if NPF > 0
#include <net/pfvar.h>
#endif

#ifdef PIPEX
#include <net/pipex.h>
#endif

/* Needs IP headers. */
#include <net/if_gre.h>

struct gre_softc *gre_lookup(struct mbuf *, u_int8_t);
int gre_input2(struct mbuf *, int, u_char);

/*
 * Decapsulate.
 * Does the real work and is called from gre_input() (above)
 * returns 0 if packet is not yet processed
 * and 1 if it needs no further processing
 * proto is the protocol number of the "calling" foo_input()
 * routine.
 */

int
gre_input2(struct mbuf *m, int hlen, u_char proto)
{
	struct greip *gip;
	struct niqueue *ifq;
	struct gre_softc *sc;
	u_short flags;
	u_int af;

	if ((sc = gre_lookup(m, proto)) == NULL) {
		/* No matching tunnel or tunnel is down. */
		return (0);
	}

	if (m->m_len < sizeof(*gip)) {
		m = m_pullup(m, sizeof(*gip));
		if (m == NULL)
			return (ENOBUFS);
	}
	gip = mtod(m, struct greip *);

	m->m_pkthdr.ph_ifidx = sc->sc_if.if_index;
	m->m_pkthdr.ph_rtableid = sc->sc_if.if_rdomain;

	sc->sc_if.if_ipackets++;
	sc->sc_if.if_ibytes += m->m_pkthdr.len;

	switch (proto) {
	case IPPROTO_GRE:
		hlen += sizeof (struct gre_h);

		/* process GRE flags as packet can be of variable len */
		flags = ntohs(gip->gi_flags);

		/* Checksum & Offset are present */
		if ((flags & GRE_CP) | (flags & GRE_RP))
			hlen += 4;

		/* We don't support routing fields (variable length) */
		if (flags & GRE_RP)
			return (0);

		if (flags & GRE_KP)
			hlen += 4;

		if (flags & GRE_SP)
			hlen += 4;

		switch (ntohs(gip->gi_ptype)) { /* ethertypes */
		case GREPROTO_WCCP:
			/* WCCP/GRE:
			 *   So far as I can see (and test) it seems that Cisco's WCCP
			 *   GRE tunnel is precisely a IP-in-GRE tunnel that differs
			 *   only in its protocol number.  At least, it works for me.
			 *
			 *   The Internet Drafts can be found if you look for
			 *   the following:
			 *     draft-forster-wrec-wccp-v1-00.txt
			 *     draft-wilson-wrec-wccp-v2-01.txt
			 *
			 *   So yes, we're doing a fall-through (unless, of course,
			 *   net.inet.gre.wccp is 0).
			 */
			if (!gre_wccp)
				return (0);
			/*
			 * For WCCPv2, additionally skip the 4 byte
			 * redirect header.
			 */
			if (gre_wccp == 2) 
				hlen += 4;
		case ETHERTYPE_IP: /* shouldn't need a schednetisr(), as */
			ifq = &ipintrq;          /* we are in ip_input */
			af = AF_INET;
			break;
#ifdef INET6
		case ETHERTYPE_IPV6:
		        ifq = &ip6intrq;
			af = AF_INET6;
			break;
#endif
		case 0:
			/* keepalive reply, retrigger hold timer */
			gre_recv_keepalive(sc);
			m_freem(m);
			return (1);
#ifdef MPLS
		case ETHERTYPE_MPLS:
		case ETHERTYPE_MPLS_MCAST:
			mpls_input(&sc->sc_if, m);
			return (1);
#endif
		default:	   /* others not yet supported */
			return (0);
		}
		break;
	default:
		/* others not yet supported */
		return (0);
	}

	if (hlen > m->m_pkthdr.len) {
		m_freem(m);
		return (EINVAL);
	}
	m_adj(m, hlen);

#if NBPFILTER > 0
        if (sc->sc_if.if_bpf)
		bpf_mtap_af(sc->sc_if.if_bpf, af, m, BPF_DIRECTION_IN);
#endif

#if NPF > 0
	pf_pkt_addr_changed(m);
#endif

	niq_enqueue(ifq, m);

	return (1);	/* packet is done, no further processing needed */
}

/*
 * Decapsulate a packet and feed it back through ip_input (this
 * routine is called whenever IP gets a packet with proto type
 * IPPROTO_GRE and a local destination address).
 */
void
gre_input(struct mbuf *m, ...)
{
	int hlen, ret;
	va_list ap;

	va_start(ap, m);
	hlen = va_arg(ap, int);
	va_end(ap);

	if (!gre_allow) {
	        m_freem(m);
		return;
	}

#ifdef PIPEX
	if (pipex_enable) {
		struct pipex_session *session;

		if ((session = pipex_pptp_lookup_session(m)) != NULL) {
			if (pipex_pptp_input(m, session) == NULL)
				return;
		}
	}
#endif

	ret = gre_input2(m, hlen, IPPROTO_GRE);
	/*
	 * ret == 0: packet not processed, but input from here
	 * means no matching tunnel that is up is found.
	 * we inject it to raw ip socket to see if anyone picks it up.
	 * possible that we received a WCCPv1-style GRE packet
	 * but we're not set to accept them.
	 */
	if (!ret)
		rip_input(m, hlen, IPPROTO_GRE);
}

/*
 * Input routine for IPPROTO_MOBILE.
 * This is a little bit different from the other modes, as the
 * encapsulating header was not prepended, but instead inserted
 * between IP header and payload.
 */

void
gre_mobile_input(struct mbuf *m, ...)
{
	struct ip *ip;
	struct mobip_h *mip;
	struct gre_softc *sc;
	int hlen;
	va_list ap;
	u_char osrc = 0;
	int msiz;

	va_start(ap, m);
	hlen = va_arg(ap, int);
	va_end(ap);

	if (!ip_mobile_allow) {
	        m_freem(m);
		return;
	}

	if ((sc = gre_lookup(m, IPPROTO_MOBILE)) == NULL) {
		/* No matching tunnel or tunnel is down. */
		m_freem(m);
		return;
	}

	if (m->m_len < sizeof(*mip)) {
		m = m_pullup(m, sizeof(*mip));
		if (m == NULL)
			return;
	}
	ip = mtod(m, struct ip *);
	mip = mtod(m, struct mobip_h *);

	m->m_pkthdr.ph_ifidx = sc->sc_if.if_index;

	sc->sc_if.if_ipackets++;
	sc->sc_if.if_ibytes += m->m_pkthdr.len;

	if (ntohs(mip->mh.proto) & MOB_H_SBIT) {
		osrc = 1;
		msiz = MOB_H_SIZ_L;
		mip->mi.ip_src.s_addr = mip->mh.osrc;
	} else
		msiz = MOB_H_SIZ_S;

	if (m->m_len < (ip->ip_hl << 2) + msiz) {
		m = m_pullup(m, (ip->ip_hl << 2) + msiz);
		if (m == NULL)
			return;
		ip = mtod(m, struct ip *);
		mip = mtod(m, struct mobip_h *);
	}

	mip->mi.ip_dst.s_addr = mip->mh.odst;
	mip->mi.ip_p = (ntohs(mip->mh.proto) >> 8);

	if (gre_in_cksum((u_short *) &mip->mh, msiz) != 0) {
		m_freem(m);
		return;
	}

	memmove(ip + (ip->ip_hl << 2), ip + (ip->ip_hl << 2) + msiz, 
	      m->m_len - msiz - (ip->ip_hl << 2));

	m->m_len -= msiz;
	ip->ip_len = htons(ntohs(ip->ip_len) - msiz);
	m->m_pkthdr.len -= msiz;

	ip->ip_sum = 0;
	ip->ip_sum = in_cksum(m,(ip->ip_hl << 2));

#if NBPFILTER > 0
        if (sc->sc_if.if_bpf)
		bpf_mtap_af(sc->sc_if.if_bpf, AF_INET, m, BPF_DIRECTION_IN);
#endif

	niq_enqueue(&ipintrq, m);
}

/*
 * Find the gre interface associated with our src/dst/proto set.
 */
struct gre_softc *
gre_lookup(struct mbuf *m, u_int8_t proto)
{
	struct ip *ip = mtod(m, struct ip *);
	struct gre_softc *sc;

	LIST_FOREACH(sc, &gre_softc_list, sc_list) {
		if ((sc->g_dst.s_addr == ip->ip_src.s_addr) &&
		    (sc->g_src.s_addr == ip->ip_dst.s_addr) &&
		    (sc->g_proto == proto) &&
		    (rtable_l2(sc->g_rtableid) ==
		    rtable_l2(m->m_pkthdr.ph_rtableid)) &&
		    ((sc->sc_if.if_flags & IFF_UP) != 0))
			return (sc);
	}

	return (NULL);
}

int
gre_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp, void *newp,
    size_t newlen)
{
        /* All sysctl names at this level are terminal. */
        if (namelen != 1)
                return (ENOTDIR);

        switch (name[0]) {
        case GRECTL_ALLOW:
                return (sysctl_int(oldp, oldlenp, newp, newlen, &gre_allow));
        case GRECTL_WCCP:
                return (sysctl_int(oldp, oldlenp, newp, newlen, &gre_wccp));
        default:
                return (ENOPROTOOPT);
        }
        /* NOTREACHED */
}

int
ipmobile_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp,
    void *newp, size_t newlen)
{
        /* All sysctl names at this level are terminal. */
        if (namelen != 1)
                return (ENOTDIR);

        switch (name[0]) {
        case MOBILEIPCTL_ALLOW:
                return (sysctl_int(oldp, oldlenp, newp, newlen,
				   &ip_mobile_allow));
        default:
                return (ENOPROTOOPT);
        }
        /* NOTREACHED */
}

int
gre_usrreq(struct socket *so, int req, struct mbuf *m, struct mbuf *nam,
    struct mbuf *control, struct proc *p)
{
#ifdef  PIPEX 
	struct inpcb *inp = sotoinpcb(so);

	if (inp != NULL && inp->inp_pipex && req == PRU_SEND) {
		int s;
		struct sockaddr_in *sin4;
		struct in_addr *ina_dst;
		struct pipex_session *session;

		s = splsoftnet();
		ina_dst = NULL;
		if ((so->so_state & SS_ISCONNECTED) != 0) {
			inp = sotoinpcb(so);
			if (inp)
				ina_dst = &inp->inp_laddr;
		} else if (nam) {
			sin4 = mtod(nam, struct sockaddr_in *);
			if (nam->m_len == sizeof(struct sockaddr_in) &&
			    sin4->sin_family == AF_INET)
				ina_dst = &sin4->sin_addr;
		}
		if (ina_dst != NULL &&
		    (session = pipex_pptp_userland_lookup_session_ipv4(m,
			    *ina_dst)))
			m = pipex_pptp_userland_output(m, session);
		splx(s);

		if (m == NULL)
			return (ENOMEM);
	}
#endif
	return rip_usrreq(so, req, m, nam, control, p);
}
#endif /* if NGRE > 0 */
