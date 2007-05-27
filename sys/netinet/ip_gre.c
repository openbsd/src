/*      $OpenBSD: ip_gre.c,v 1.31 2007/05/27 21:20:52 claudio Exp $ */
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <net/if.h>
#include <net/netisr.h>
#include <net/route.h>
#include <net/bpf.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip_gre.h>
#include <netinet/if_ether.h>
#else
#error "ip_gre used without inet"
#endif

#ifdef NETATALK
#include <netatalk/at.h>
#include <netatalk/at_var.h>
#include <netatalk/at_extern.h>
#endif

#include "bpfilter.h"

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
gre_input2(m , hlen, proto)
        struct mbuf *m;
	int hlen;
	u_char proto;
{
	struct greip *gip;
	int s;
	struct ifqueue *ifq;
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

	m->m_pkthdr.rcvif = &sc->sc_if;

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
			 *   The Internet Draft can be found if you look for
			 *     draft-forster-wrec-wccp-v1-00.txt
			 *
			 *   So yes, we're doing a fall-through (unless, of course,
			 *   net.inet.gre.wccp is 0).
			 */
			if (!gre_wccp)
				return (0);
		case ETHERTYPE_IP: /* shouldn't need a schednetisr(), as */
			ifq = &ipintrq;          /* we are in ip_input */
			af = AF_INET;
			break;
#ifdef NETATALK
		case ETHERTYPE_AT:
			ifq = &atintrq1;
			schednetisr(NETISR_ATALK);
			af = AF_APPLETALK;
			break;
#endif
#ifdef INET6
		case ETHERTYPE_IPV6:
		        ifq = &ip6intrq;
			schednetisr(NETISR_IPV6);
			af = AF_INET6;
			break;
#endif /* INET6 */
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

	s = splnet();		/* possible */
	IF_INPUT_ENQUEUE(ifq, m);
	splx(s);

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
 * Input routine for IPPRPOTO_MOBILE.
 * This is a little bit diffrent from the other modes, as the
 * encapsulating header was not prepended, but instead inserted
 * between IP header and payload.
 */

void
gre_mobile_input(struct mbuf *m, ...)
{
	struct ip *ip;
	struct mobip_h *mip;
	struct ifqueue *ifq;
	struct gre_softc *sc;
	int hlen, s;
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

	m->m_pkthdr.rcvif = &sc->sc_if;

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

	bcopy(ip + (ip->ip_hl << 2) + msiz, ip + (ip->ip_hl << 2),
	      m->m_len - msiz - (ip->ip_hl << 2));

	m->m_len -= msiz;
	ip->ip_len = htons(ntohs(ip->ip_len) - msiz);
	m->m_pkthdr.len -= msiz;

	ip->ip_sum = 0;
	ip->ip_sum = in_cksum(m,(ip->ip_hl << 2));

	ifq = &ipintrq;

#if NBPFILTER > 0
        if (sc->sc_if.if_bpf)
		bpf_mtap_af(sc->sc_if.if_bpf, AF_INET, m, BPF_DIRECTION_IN);
#endif

	s = splnet();       /* possible */
	IF_INPUT_ENQUEUE(ifq, m);
	splx(s);
}

/*
 * Find the gre interface associated with our src/dst/proto set.
 */
struct gre_softc *
gre_lookup(m, proto)
	struct mbuf *m;
	u_int8_t proto;
{
	struct ip *ip = mtod(m, struct ip *);
	struct gre_softc *sc;

	LIST_FOREACH(sc, &gre_softc_list, sc_list) {
		if ((sc->g_dst.s_addr == ip->ip_src.s_addr) &&
		    (sc->g_src.s_addr == ip->ip_dst.s_addr) &&
		    (sc->g_proto == proto) &&
		    ((sc->sc_if.if_flags & IFF_UP) != 0))
			return (sc);
	}

	return (NULL);
}

int
gre_sysctl(name, namelen, oldp, oldlenp, newp, newlen)
        int *name;
        u_int namelen;
        void *oldp;
        size_t *oldlenp;
        void *newp;
        size_t newlen;
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
ipmobile_sysctl(name, namelen, oldp, oldlenp, newp, newlen)
        int *name;
        u_int namelen;
        void *oldp;
        size_t *oldlenp;
        void *newp;
        size_t newlen;
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
#endif /* if NGRE > 0 */
