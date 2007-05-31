/*	$OpenBSD: route6.c,v 1.16 2007/05/31 23:17:38 mcbride Exp $	*/
/*	$KAME: route6.c,v 1.22 2000/12/03 00:54:00 itojun Exp $	*/

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

#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/systm.h>

#include <net/if.h>

#include <netinet/in.h>
#include <netinet6/in6_var.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>

#include <netinet/icmp6.h>

#if 0
static int ip6_rthdr0(struct mbuf *, struct ip6_hdr *, struct ip6_rthdr0 *);
#endif

int
route6_input(mp, offp, proto)
	struct mbuf **mp;
	int *offp, proto;	/* proto is unused */
{
	struct ip6_hdr *ip6;
	struct mbuf *m = *mp;
	struct ip6_rthdr *rh;
	int off = *offp, rhlen;

	ip6 = mtod(m, struct ip6_hdr *);
	IP6_EXTHDR_GET(rh, struct ip6_rthdr *, m, off, sizeof(*rh));
	if (rh == NULL) {
		ip6stat.ip6s_tooshort++;
		return IPPROTO_DONE;
	}

	switch (rh->ip6r_type) {
#if 0
	/*
	 * See http://www.secdev.org/conf/IPv6_RH_security-csw07.pdf
	 * for why IPV6_RTHDR_TYPE_0 is banned here.
	 *
	 * We return ICMPv6 parameter problem so that innocent people
	 * (not an attacker) would notice about the use of IPV6_RTHDR_TYPE_0.
	 * Since there's no amplification, and ICMPv6 error will be rate-
	 * controlled, it shouldn't cause any problem.
	 * If you are concerned about this, you may want to use the following
	 * code fragment:
	 *
	 * case IPV6_RTHDR_TYPE_0:
	 *	m_freem(m);
	 *	return (IPPROTO_DONE);
	 */
	case IPV6_RTHDR_TYPE_0:
		rhlen = (rh->ip6r_len + 1) << 3;
		if (rh->ip6r_segleft == 0)
			break;	/* Final dst. Just ignore the header. */
		/*
		 * note on option length:
		 * maximum rhlen: 2048
		 * max mbuf m_pulldown can handle: MCLBYTES == usually 2048
		 * so, here we are assuming that m_pulldown can handle
		 * rhlen == 2048 case.  this may not be a good thing to
		 * assume - we may want to avoid pulling it up altogether.
		 */
		IP6_EXTHDR_GET(rh, struct ip6_rthdr *, m, off, rhlen);
		if (rh == NULL) {
			ip6stat.ip6s_tooshort++;
			return IPPROTO_DONE;
		}
		if (ip6_rthdr0(m, ip6, (struct ip6_rthdr0 *)rh))
			return (IPPROTO_DONE);
		break;
#endif
	default:
		/* unknown routing type */
		if (rh->ip6r_segleft == 0) {
			rhlen = (rh->ip6r_len + 1) << 3;
			break;	/* Final dst. Just ignore the header. */
		}
		ip6stat.ip6s_badoptions++;
		icmp6_error(m, ICMP6_PARAM_PROB, ICMP6_PARAMPROB_HEADER,
			    (caddr_t)&rh->ip6r_type - (caddr_t)ip6);
		return (IPPROTO_DONE);
	}

	*offp += rhlen;
	return (rh->ip6r_nxt);
}

#if 0
/*
 * Type0 routing header processing
 *
 * RFC2292 backward compatibility warning: no support for strict/loose bitmap,
 * as it was dropped between RFC1883 and RFC2460.
 */
static int
ip6_rthdr0(m, ip6, rh0)
	struct mbuf *m;
	struct ip6_hdr *ip6;
	struct ip6_rthdr0 *rh0;
{
	int addrs, index;
	struct in6_addr *nextaddr, tmpaddr;

	if (rh0->ip6r0_segleft == 0)
		return (0);

	if (rh0->ip6r0_len % 2) {
		/*
		 * Type 0 routing header can't contain more than 23 addresses.
		 * RFC 2460: this limitation was removed since strict/loose
		 * bitmap field was deleted.
		 */
		ip6stat.ip6s_badoptions++;
		icmp6_error(m, ICMP6_PARAM_PROB, ICMP6_PARAMPROB_HEADER,
			    (caddr_t)&rh0->ip6r0_len - (caddr_t)ip6);
		return (-1);
	}

	if ((addrs = rh0->ip6r0_len / 2) < rh0->ip6r0_segleft) {
		ip6stat.ip6s_badoptions++;
		icmp6_error(m, ICMP6_PARAM_PROB, ICMP6_PARAMPROB_HEADER,
			    (caddr_t)&rh0->ip6r0_segleft - (caddr_t)ip6);
		return (-1);
	}

	index = addrs - rh0->ip6r0_segleft;
	rh0->ip6r0_segleft--;
	nextaddr = ((struct in6_addr *)(rh0 + 1)) + index;

	/*
	 * reject invalid addresses.  be proactive about malicious use of
	 * IPv4 mapped/compat address.
	 * XXX need more checks?
	 */
	if (IN6_IS_ADDR_MULTICAST(nextaddr) ||
	    IN6_IS_ADDR_UNSPECIFIED(nextaddr) ||
	    IN6_IS_ADDR_V4MAPPED(nextaddr) ||
	    IN6_IS_ADDR_V4COMPAT(nextaddr)) {
		ip6stat.ip6s_badoptions++;
		goto bad;
	}
	if (IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst) ||
	    IN6_IS_ADDR_UNSPECIFIED(&ip6->ip6_dst) ||
	    IN6_IS_ADDR_V4MAPPED(&ip6->ip6_dst) ||
	    IN6_IS_ADDR_V4COMPAT(&ip6->ip6_dst)) {
		ip6stat.ip6s_badoptions++;
		goto bad;
	}

	/*
	 * Swap the IPv6 destination address and nextaddr. Forward the packet.
	 */
	tmpaddr = *nextaddr;
	*nextaddr = ip6->ip6_dst;
	if (IN6_IS_ADDR_LINKLOCAL(nextaddr))
		nextaddr->s6_addr16[1] = 0;
	ip6->ip6_dst = tmpaddr;
	if (IN6_IS_ADDR_LINKLOCAL(&ip6->ip6_dst))
		ip6->ip6_dst.s6_addr16[1] = htons(m->m_pkthdr.rcvif->if_index);

	ip6_forward(m, 1);

	return (-1);			/* m would be freed in ip6_forward() */

  bad:
	m_freem(m);
	return (-1);
}
#endif
