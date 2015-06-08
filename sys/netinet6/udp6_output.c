/*	$OpenBSD: udp6_output.c,v 1.35 2015/06/08 22:19:28 krw Exp $	*/
/*	$KAME: udp6_output.c,v 1.21 2001/02/07 11:51:54 itojun Exp $	*/

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
 * Copyright (c) 1982, 1986, 1989, 1993
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

#include "pf.h"

#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/systm.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/route.h>
#include <net/if_types.h>

#include <netinet/in.h>
#include <netinet6/in6_var.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/in_pcb.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet/icmp6.h>
#include <netinet6/ip6protosw.h>

/*
 * UDP protocol inplementation.
 * Per RFC 768, August, 1980.
 */
int
udp6_output(struct inpcb *in6p, struct mbuf *m, struct mbuf *addr6,
	struct mbuf *control)
{
	u_int32_t ulen = m->m_pkthdr.len;
	u_int32_t plen = sizeof(struct udphdr) + ulen;
	int error = 0, priv = 0, hlen, flags;
	struct ip6_hdr *ip6;
	struct udphdr *udp6;
	struct in6_addr *laddr, *faddr;
	struct ip6_pktopts *optp, opt;
	struct sockaddr_in6 tmp;
	struct proc *p = curproc;	/* XXX */
	struct ifnet *ifp;
	u_short fport;

	if ((in6p->inp_socket->so_state & SS_PRIV) != 0)
		priv = 1;
	if (control) {
		if ((error = ip6_setpktopts(control, &opt,
		    in6p->inp_outputopts6, priv, IPPROTO_UDP)) != 0)
			goto release;
		optp = &opt;
	} else
		optp = in6p->inp_outputopts6;

	if (addr6) {
		struct sockaddr_in6 *sin6 = mtod(addr6, struct sockaddr_in6 *);

		if (addr6->m_len != sizeof(*sin6)) {
			error = EINVAL;
			goto release;
		}
		if (sin6->sin6_family != AF_INET6) {
			error = EAFNOSUPPORT;
			goto release;
		}
		if (sin6->sin6_port == 0) {
			error = EADDRNOTAVAIL;
			goto release;
		}
		if (IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr)) {
			error = EINVAL;
			goto release;
		}

		if (!IN6_IS_ADDR_UNSPECIFIED(&in6p->inp_faddr6)) {
			error = EISCONN;
			goto release;
		}

		/* protect *sin6 from overwrites */
		tmp = *sin6;
		sin6 = &tmp;

		faddr = &sin6->sin6_addr;
		fport = sin6->sin6_port; /* allow 0 port */

		/* KAME hack: embed scopeid */
		if (in6_embedscope(&sin6->sin6_addr, sin6, in6p, NULL) != 0) {
			error = EINVAL;
			goto release;
		}

		error = in6_selectsrc(&laddr, sin6, optp,
		    in6p->inp_moptions6, &in6p->inp_route6,
		    &in6p->inp_laddr6, in6p->inp_rtableid);
		if (error)
			goto release;

		if (in6p->inp_lport == 0 &&
		    (error = in6_pcbsetport(laddr, in6p, p)) != 0)
			goto release;
	} else {
		if (IN6_IS_ADDR_UNSPECIFIED(&in6p->inp_faddr6)) {
			error = ENOTCONN;
			goto release;
		}
		laddr = &in6p->inp_laddr6;
		faddr = &in6p->inp_faddr6;
		fport = in6p->inp_fport;
	}

	hlen = sizeof(struct ip6_hdr);

	/*
	 * Calculate data length and get a mbuf
	 * for UDP and IP6 headers.
	 */
	M_PREPEND(m, hlen + sizeof(struct udphdr), M_DONTWAIT);
	if (m == NULL) {
		error = ENOBUFS;
		goto releaseopt;
	}

	/*
	 * Stuff checksum and output datagram.
	 */
	udp6 = (struct udphdr *)(mtod(m, caddr_t) + hlen);
	udp6->uh_sport = in6p->inp_lport; /* lport is always set in the PCB */
	udp6->uh_dport = fport;
	if (plen <= 0xffff)
		udp6->uh_ulen = htons((u_short)plen);
	else
		udp6->uh_ulen = 0;
	udp6->uh_sum = 0;

	ip6 = mtod(m, struct ip6_hdr *);
	ip6->ip6_flow	= in6p->inp_flowinfo & IPV6_FLOWINFO_MASK;
	ip6->ip6_vfc	&= ~IPV6_VERSION_MASK;
	ip6->ip6_vfc	|= IPV6_VERSION;
#if 0	/* ip6_plen will be filled in ip6_output. */
	ip6->ip6_plen	= htons((u_short)plen);
#endif
	ip6->ip6_nxt	= IPPROTO_UDP;
	ifp = NULL;
	if (in6p->inp_route6.ro_rt &&
	    in6p->inp_route6.ro_rt->rt_flags & RTF_UP)
		ifp = in6p->inp_route6.ro_rt->rt_ifp;
	ip6->ip6_hlim	= in6_selecthlim(in6p, ifp);
	ip6->ip6_src	= *laddr;
	ip6->ip6_dst	= *faddr;

	m->m_pkthdr.csum_flags |= M_UDP_CSUM_OUT;

	flags = 0;
	if (in6p->inp_flags & IN6P_MINMTU)
		flags |= IPV6_MINMTU;

	udpstat.udps_opackets++;

	/* force routing table */
	m->m_pkthdr.ph_rtableid = in6p->inp_rtableid;

#if NPF > 0
	if (in6p->inp_socket->so_state & SS_ISCONNECTED)
		m->m_pkthdr.pf.inp = in6p;
#endif

	error = ip6_output(m, optp, &in6p->inp_route6,
	    flags, in6p->inp_moptions6, NULL, in6p);
	goto releaseopt;

release:
	m_freem(m);

releaseopt:
	if (control) {
		ip6_clearpktopts(&opt, -1);
		m_freem(control);
	}
	return (error);
}
