/*	$OpenBSD: ip6_forward.c,v 1.97 2017/11/28 15:32:51 mpi Exp $	*/
/*	$KAME: ip6_forward.c,v 1.75 2001/06/29 12:42:13 jinmei Exp $	*/

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
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_enc.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/ip_var.h>
#include <netinet6/in6_var.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet/icmp6.h>
#include <netinet6/nd6.h>

#if NPF > 0
#include <net/pfvar.h>
#endif

#ifdef IPSEC
#include <netinet/ip_ipsp.h>
#include <netinet/ip_ah.h>
#include <netinet/ip_esp.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#endif

/*
 * Forward a packet.  If some error occurs return the sender
 * an icmp packet.  Note we can't always generate a meaningful
 * icmp message because icmp doesn't have a large enough repertoire
 * of codes and types.
 *
 * If not forwarding, just drop the packet.  This could be confusing
 * if ipforwarding was zero but some routing protocol was advancing
 * us as a gateway to somewhere.  However, we must let the routing
 * protocol deal with that.
 *
 */

void
ip6_forward(struct mbuf *m, struct rtentry *rt, int srcrt)
{
	struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);
	struct sockaddr_in6 *dst, sin6;
	struct ifnet *ifp = NULL;
	int error = 0, type = 0, code = 0;
	struct mbuf *mcopy = NULL;
#ifdef IPSEC
	struct tdb *tdb = NULL;
#endif /* IPSEC */
	char src6[INET6_ADDRSTRLEN], dst6[INET6_ADDRSTRLEN];

	/*
	 * Do not forward packets to multicast destination (should be handled
	 * by ip6_mforward().
	 * Do not forward packets with unspecified source.  It was discussed
	 * in July 2000, on ipngwg mailing list.
	 */
	if ((m->m_flags & (M_BCAST|M_MCAST)) != 0 ||
	    IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst) ||
	    IN6_IS_ADDR_UNSPECIFIED(&ip6->ip6_src)) {
		ip6stat_inc(ip6s_cantforward);
		if (ip6_log_time + ip6_log_interval < time_uptime) {
			ip6_log_time = time_uptime;
			inet_ntop(AF_INET6, &ip6->ip6_src, src6, sizeof(src6));
			inet_ntop(AF_INET6, &ip6->ip6_dst, dst6, sizeof(dst6));
			log(LOG_DEBUG,
			    "cannot forward "
			    "from %s to %s nxt %d received on inteface %u\n",
			    src6, dst6,
			    ip6->ip6_nxt,
			    m->m_pkthdr.ph_ifidx);
		}
		m_freem(m);
		goto out;
	}

	if (ip6->ip6_hlim <= IPV6_HLIMDEC) {
		icmp6_error(m, ICMP6_TIME_EXCEEDED,
				ICMP6_TIME_EXCEED_TRANSIT, 0);
		goto out;
	}
	ip6->ip6_hlim -= IPV6_HLIMDEC;

	/*
	 * Save at most ICMPV6_PLD_MAXLEN (= the min IPv6 MTU -
	 * size of IPv6 + ICMPv6 headers) bytes of the packet in case
	 * we need to generate an ICMP6 message to the src.
	 * Thanks to M_EXT, in most cases copy will not occur.
	 *
	 * It is important to save it before IPsec processing as IPsec
	 * processing may modify the mbuf.
	 */
	mcopy = m_copym(m, 0, imin(m->m_pkthdr.len, ICMPV6_PLD_MAXLEN),
	    M_NOWAIT);

#if NPF > 0
reroute:
#endif

#ifdef IPSEC
	if (ipsec_in_use) {
		tdb = ip6_output_ipsec_lookup(m, &error, NULL);
		if (error != 0) {
			/*
			 * -EINVAL is used to indicate that the packet should
			 * be silently dropped, typically because we've asked
			 * key management for an SA.
			 */
			if (error == -EINVAL) /* Should silently drop packet */
				error = 0;

			m_freem(m);
			goto freecopy;
		}
	}
#endif /* IPSEC */

	dst = &sin6;
	memset(dst, 0, sizeof(*dst));
	dst->sin6_len = sizeof(struct sockaddr_in6);
	dst->sin6_family = AF_INET6;
	dst->sin6_addr = ip6->ip6_dst;

	if (!rtisvalid(rt)) {
		rtfree(rt);
		rt = rtalloc_mpath(sin6tosa(dst), &ip6->ip6_src.s6_addr32[0],
		    m->m_pkthdr.ph_rtableid);
		if (rt == NULL) {
			ip6stat_inc(ip6s_noroute);
			if (mcopy) {
				icmp6_error(mcopy, ICMP6_DST_UNREACH,
					    ICMP6_DST_UNREACH_NOROUTE, 0);
			}
			m_freem(m);
			goto out;
		}
	}

	/*
	 * Scope check: if a packet can't be delivered to its destination
	 * for the reason that the destination is beyond the scope of the
	 * source address, discard the packet and return an icmp6 destination
	 * unreachable error with Code 2 (beyond scope of source address).
	 * [draft-ietf-ipngwg-icmp-v3-00.txt, Section 3.1]
	 */
	if (in6_addr2scopeid(m->m_pkthdr.ph_ifidx, &ip6->ip6_src) !=
	    in6_addr2scopeid(rt->rt_ifidx, &ip6->ip6_src)) {
		ip6stat_inc(ip6s_cantforward);
		ip6stat_inc(ip6s_badscope);

		if (ip6_log_time + ip6_log_interval < time_uptime) {
			ip6_log_time = time_uptime;
			inet_ntop(AF_INET6, &ip6->ip6_src, src6, sizeof(src6));
			inet_ntop(AF_INET6, &ip6->ip6_dst, dst6, sizeof(dst6));
			log(LOG_DEBUG,
			    "cannot forward "
			    "src %s, dst %s, nxt %d, rcvif %u, outif %u\n",
			    src6, dst6,
			    ip6->ip6_nxt,
			    m->m_pkthdr.ph_ifidx, rt->rt_ifidx);
		}
		if (mcopy)
			icmp6_error(mcopy, ICMP6_DST_UNREACH,
				    ICMP6_DST_UNREACH_BEYONDSCOPE, 0);
		m_freem(m);
		goto out;
	}

#ifdef IPSEC
	/*
	 * Check if the packet needs encapsulation.
	 * ipsp_process_packet will never come back to here.
	 * XXX ipsp_process_packet() calls ip6_output(), and there'll be no
	 * PMTU notification.  is it okay?
	 */
	if (tdb != NULL) {
		/* Callee frees mbuf */
		error = ip6_output_ipsec_send(tdb, m, 0, 1);
		if (error)
			goto senderr;
		goto freecopy;
	}
#endif /* IPSEC */

	if (rt->rt_flags & RTF_GATEWAY)
		dst = satosin6(rt->rt_gateway);

	/*
	 * If we are to forward the packet using the same interface
	 * as one we got the packet from, perhaps we should send a redirect
	 * to sender to shortcut a hop.
	 * Only send redirect if source is sending directly to us,
	 * and if packet was not source routed (or has any options).
	 * Also, don't send redirect if forwarding using a route
	 * modified by a redirect.
	 */
	ifp = if_get(rt->rt_ifidx);
	if (ifp == NULL) {
		m_freem(m);
		goto freecopy;
	}
	if (rt->rt_ifidx == m->m_pkthdr.ph_ifidx && !srcrt &&
	    ip6_sendredirects &&
	    (rt->rt_flags & (RTF_DYNAMIC|RTF_MODIFIED)) == 0) {
		if ((ifp->if_flags & IFF_POINTOPOINT) &&
		    nd6_is_addr_neighbor(&sin6, ifp)) {
			/*
			 * If the incoming interface is equal to the outgoing
			 * one, the link attached to the interface is
			 * point-to-point, and the IPv6 destination is
			 * regarded as on-link on the link, then it will be
			 * highly probable that the destination address does
			 * not exist on the link and that the packet is going
			 * to loop.  Thus, we immediately drop the packet and
			 * send an ICMPv6 error message.
			 * For other routing loops, we dare to let the packet
			 * go to the loop, so that a remote diagnosing host
			 * can detect the loop by traceroute.
			 * type/code is based on suggestion by Rich Draves.
			 * not sure if it is the best pick.
			 */
			if (mcopy)
				icmp6_error(mcopy, ICMP6_DST_UNREACH,
				    ICMP6_DST_UNREACH_ADDR, 0);
			m_freem(m);
			goto out;
		}
		type = ND_REDIRECT;
	}

	/*
	 * Fake scoped addresses. Note that even link-local source or
	 * destinaion can appear, if the originating node just sends the
	 * packet to us (without address resolution for the destination).
	 * Since both icmp6_error and icmp6_redirect_output fill the embedded
	 * link identifiers, we can do this stuff after making a copy for
	 * returning an error.
	 */
	if (IN6_IS_SCOPE_EMBED(&ip6->ip6_src))
		ip6->ip6_src.s6_addr16[1] = 0;
	if (IN6_IS_SCOPE_EMBED(&ip6->ip6_dst))
		ip6->ip6_dst.s6_addr16[1] = 0;

#if NPF > 0
	if (pf_test(AF_INET6, PF_FWD, ifp, &m) != PF_PASS) {
		m_freem(m);
		goto senderr;
	}
	if (m == NULL)
		goto senderr;
	ip6 = mtod(m, struct ip6_hdr *);
	if ((m->m_pkthdr.pf.flags & (PF_TAG_REROUTE | PF_TAG_GENERATED)) ==
	    (PF_TAG_REROUTE | PF_TAG_GENERATED)) {
		/* already rerun the route lookup, go on */
		m->m_pkthdr.pf.flags &= ~(PF_TAG_GENERATED | PF_TAG_REROUTE);
	} else if (m->m_pkthdr.pf.flags & PF_TAG_REROUTE) {
		/* tag as generated to skip over pf_test on rerun */
		m->m_pkthdr.pf.flags |= PF_TAG_GENERATED;
		srcrt = 1;
		rtfree(rt);
		rt = NULL;
		if_put(ifp);
		ifp = NULL;
		goto reroute;
	}
#endif
	in6_proto_cksum_out(m, ifp);

	/* Check the size after pf_test to give pf a chance to refragment. */
	if (m->m_pkthdr.len > ifp->if_mtu) {
		if (mcopy)
			icmp6_error(mcopy, ICMP6_PACKET_TOO_BIG, 0,
			    ifp->if_mtu);
		m_freem(m);
		goto out;
	}

	error = ifp->if_output(ifp, m, sin6tosa(dst), rt);
	if (error) {
		ip6stat_inc(ip6s_cantforward);
	} else {
		ip6stat_inc(ip6s_forward);
		if (type)
			ip6stat_inc(ip6s_redirectsent);
		else {
			if (mcopy)
				goto freecopy;
		}
	}

#if NPF > 0 || defined(IPSEC)
senderr:
#endif
	if (mcopy == NULL)
		goto out;
	switch (error) {
	case 0:
		if (type == ND_REDIRECT) {
			icmp6_redirect_output(mcopy, rt);
			goto out;
		}
		goto freecopy;

	case EMSGSIZE:
		/* xxx MTU is constant in PPP? */
		goto freecopy;

	case ENOBUFS:
		/* Tell source to slow down like source quench in IP? */
		goto freecopy;

	case ENETUNREACH:	/* shouldn't happen, checked above */
	case EHOSTUNREACH:
	case ENETDOWN:
	case EHOSTDOWN:
	default:
		type = ICMP6_DST_UNREACH;
		code = ICMP6_DST_UNREACH_ADDR;
		break;
	}
	icmp6_error(mcopy, type, code, 0);
	goto out;

freecopy:
	m_freem(mcopy);
out:
	rtfree(rt);
	if_put(ifp);
}
