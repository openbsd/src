/*	$OpenBSD: ip6_forward.c,v 1.47 2010/06/29 21:28:38 reyk Exp $	*/
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
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/if_enc.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/ip_var.h>
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
#include <net/pfkeyv2.h>
#endif

struct	route_in6 ip6_forward_rt;

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
ip6_forward(struct mbuf *m, int srcrt)
{
	struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);
	struct sockaddr_in6 *dst;
	struct rtentry *rt;
	int error = 0, type = 0, code = 0;
	struct mbuf *mcopy = NULL;
	struct ifnet *origifp;	/* maybe unnecessary */
#ifdef IPSEC
	u_int8_t sproto = 0;
	struct m_tag *mtag;
	union sockaddr_union sdst;
	struct tdb_ident *tdbi;
	u_int32_t sspi;
	struct tdb *tdb;
	int s;
#if NPF > 0
	struct ifnet *encif;
#endif
#endif /* IPSEC */
	u_int rtableid = 0;

	/*
	 * Do not forward packets to multicast destination (should be handled
	 * by ip6_mforward().
	 * Do not forward packets with unspecified source.  It was discussed
	 * in July 2000, on ipngwg mailing list.
	 */
	if ((m->m_flags & (M_BCAST|M_MCAST)) != 0 ||
	    IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst) ||
	    IN6_IS_ADDR_UNSPECIFIED(&ip6->ip6_src)) {
		ip6stat.ip6s_cantforward++;
		/* XXX in6_ifstat_inc(rt->rt_ifp, ifs6_in_discard) */
		if (ip6_log_time + ip6_log_interval < time_second) {
			ip6_log_time = time_second;
			log(LOG_DEBUG,
			    "cannot forward "
			    "from %s to %s nxt %d received on %s\n",
			    ip6_sprintf(&ip6->ip6_src),
			    ip6_sprintf(&ip6->ip6_dst),
			    ip6->ip6_nxt,
			    m->m_pkthdr.rcvif->if_xname);
		}
		m_freem(m);
		return;
	}

	if (ip6->ip6_hlim <= IPV6_HLIMDEC) {
		/* XXX in6_ifstat_inc(rt->rt_ifp, ifs6_in_discard) */
		icmp6_error(m, ICMP6_TIME_EXCEEDED,
				ICMP6_TIME_EXCEED_TRANSIT, 0);
		return;
	}
	ip6->ip6_hlim -= IPV6_HLIMDEC;

#if NPF > 0
reroute:
#endif

#ifdef IPSEC
	if (!ipsec_in_use)
		goto done_spd;

	s = splnet();

	/*
	 * Check if there was an outgoing SA bound to the flow
	 * from a transport protocol.
	 */

	/* Do we have any pending SAs to apply ? */
	mtag = m_tag_find(m, PACKET_TAG_IPSEC_PENDING_TDB, NULL);
	if (mtag != NULL) {
#ifdef DIAGNOSTIC
		if (mtag->m_tag_len != sizeof (struct tdb_ident))
			panic("ip6_forward: tag of length %d (should be %d",
			    mtag->m_tag_len, sizeof (struct tdb_ident));
#endif
		tdbi = (struct tdb_ident *)(mtag + 1);
		tdb = gettdb(tdbi->spi, &tdbi->dst, tdbi->proto);
		if (tdb == NULL)
			error = -EINVAL;
		m_tag_delete(m, mtag);
	} else
		tdb = ipsp_spd_lookup(m, AF_INET6, sizeof(struct ip6_hdr),
		    &error, IPSP_DIRECTION_OUT, NULL, NULL);

	if (tdb == NULL) {
	        splx(s);

		if (error == 0) {
		        /*
			 * No IPsec processing required, we'll just send the
			 * packet out.
			 */
		        sproto = 0;

			/* Fall through to routing/multicast handling */
		} else {
		        /*
			 * -EINVAL is used to indicate that the packet should
			 * be silently dropped, typically because we've asked
			 * key management for an SA.
			 */
		        if (error == -EINVAL) /* Should silently drop packet */
				error = 0;

			goto freecopy;
		}
	} else {
		/* Loop detection */
		for (mtag = m_tag_first(m); mtag != NULL;
		    mtag = m_tag_next(m, mtag)) {
			if (mtag->m_tag_id != PACKET_TAG_IPSEC_OUT_DONE &&
			    mtag->m_tag_id !=
			    PACKET_TAG_IPSEC_OUT_CRYPTO_NEEDED)
				continue;
			tdbi = (struct tdb_ident *)(mtag + 1);
			if (tdbi->spi == tdb->tdb_spi &&
			    tdbi->proto == tdb->tdb_sproto &&
			    !bcmp(&tdbi->dst, &tdb->tdb_dst,
			    sizeof(union sockaddr_union))) {
				splx(s);
				sproto = 0; /* mark as no-IPsec-needed */
				goto done_spd;
			}
		}

	        /* We need to do IPsec */
	        bcopy(&tdb->tdb_dst, &sdst, sizeof(sdst));
		sspi = tdb->tdb_spi;
		sproto = tdb->tdb_sproto;
	        splx(s);
	}

	/* Fall through to the routing/multicast handling code */
 done_spd:
#endif /* IPSEC */

#if NPF > 0
	rtableid = m->m_pkthdr.rdomain;
#endif

	/*
	 * Save at most ICMPV6_PLD_MAXLEN (= the min IPv6 MTU -
	 * size of IPv6 + ICMPv6 headers) bytes of the packet in case
	 * we need to generate an ICMP6 message to the src.
	 * Thanks to M_EXT, in most cases copy will not occur.
	 *
	 * It is important to save it before IPsec processing as IPsec
	 * processing may modify the mbuf.
	 */
	mcopy = m_copy(m, 0, imin(m->m_pkthdr.len, ICMPV6_PLD_MAXLEN));

	dst = &ip6_forward_rt.ro_dst;
	if (!srcrt) {
		/*
		 * ip6_forward_rt.ro_dst.sin6_addr is equal to ip6->ip6_dst
		 */
		if (ip6_forward_rt.ro_rt == 0 ||
		    (ip6_forward_rt.ro_rt->rt_flags & RTF_UP) == 0 ||
		    ip6_forward_rt.ro_tableid != rtableid) {
			if (ip6_forward_rt.ro_rt) {
				RTFREE(ip6_forward_rt.ro_rt);
				ip6_forward_rt.ro_rt = 0;
			}
			/* this probably fails but give it a try again */
			ip6_forward_rt.ro_tableid = rtableid;
			rtalloc_mpath((struct route *)&ip6_forward_rt,
			    &ip6->ip6_src.s6_addr32[0]);
		}

		if (ip6_forward_rt.ro_rt == 0) {
			ip6stat.ip6s_noroute++;
			/* XXX in6_ifstat_inc(rt->rt_ifp, ifs6_in_noroute) */
			if (mcopy) {
				icmp6_error(mcopy, ICMP6_DST_UNREACH,
					    ICMP6_DST_UNREACH_NOROUTE, 0);
			}
			m_freem(m);
			return;
		}
	} else if (ip6_forward_rt.ro_rt == 0 ||
	   (ip6_forward_rt.ro_rt->rt_flags & RTF_UP) == 0 ||
	   !IN6_ARE_ADDR_EQUAL(&ip6->ip6_dst, &dst->sin6_addr) ||
	   ip6_forward_rt.ro_tableid != rtableid) {
		if (ip6_forward_rt.ro_rt) {
			RTFREE(ip6_forward_rt.ro_rt);
			ip6_forward_rt.ro_rt = 0;
		}
		bzero(dst, sizeof(*dst));
		dst->sin6_len = sizeof(struct sockaddr_in6);
		dst->sin6_family = AF_INET6;
		dst->sin6_addr = ip6->ip6_dst;
		ip6_forward_rt.ro_tableid = rtableid;

		rtalloc_mpath((struct route *)&ip6_forward_rt,
		    &ip6->ip6_src.s6_addr32[0]);

		if (ip6_forward_rt.ro_rt == 0) {
			ip6stat.ip6s_noroute++;
			/* XXX in6_ifstat_inc(rt->rt_ifp, ifs6_in_noroute) */
			if (mcopy) {
				icmp6_error(mcopy, ICMP6_DST_UNREACH,
					    ICMP6_DST_UNREACH_NOROUTE, 0);
			}
			m_freem(m);
			return;
		}
	}
	rt = ip6_forward_rt.ro_rt;

	/*
	 * Scope check: if a packet can't be delivered to its destination
	 * for the reason that the destination is beyond the scope of the
	 * source address, discard the packet and return an icmp6 destination
	 * unreachable error with Code 2 (beyond scope of source address).
	 * [draft-ietf-ipngwg-icmp-v3-00.txt, Section 3.1]
	 */
	if (in6_addr2scopeid(m->m_pkthdr.rcvif, &ip6->ip6_src) !=
	    in6_addr2scopeid(rt->rt_ifp, &ip6->ip6_src)) {
		ip6stat.ip6s_cantforward++;
		ip6stat.ip6s_badscope++;
		in6_ifstat_inc(rt->rt_ifp, ifs6_in_discard);

		if (ip6_log_time + ip6_log_interval < time_second) {
			ip6_log_time = time_second;
			log(LOG_DEBUG,
			    "cannot forward "
			    "src %s, dst %s, nxt %d, rcvif %s, outif %s\n",
			    ip6_sprintf(&ip6->ip6_src),
			    ip6_sprintf(&ip6->ip6_dst),
			    ip6->ip6_nxt,
			    m->m_pkthdr.rcvif->if_xname, rt->rt_ifp->if_xname);
		}
		if (mcopy)
			icmp6_error(mcopy, ICMP6_DST_UNREACH,
				    ICMP6_DST_UNREACH_BEYONDSCOPE, 0);
		m_freem(m);
		goto freert;
	}

#ifdef IPSEC
	/*
	 * Check if the packet needs encapsulation.
	 * ipsp_process_packet will never come back to here.
	 * XXX ipsp_process_packet() calls ip6_output(), and there'll be no
	 * PMTU notification.  is it okay?
	 */
	if (sproto != 0) {
		s = splnet();

#if NPF > 0
		if ((encif = enc_getif(0)) == NULL ||
		    pf_test6(PF_OUT, encif, &m, NULL) != PF_PASS) {
			splx(s);
			error = EHOSTUNREACH;
			m_freem(m);
			goto senderr;
		}
		if (m == NULL) {
			splx(s);
			goto senderr;
		}
		ip6 = mtod(m, struct ip6_hdr *);
		/*
		 * PF_TAG_REROUTE handling or not...
		 * Packet is entering IPsec so the routing is
		 * already overruled by the IPsec policy.
		 * Until now the change was not reconsidered.
		 * What's the behaviour?
		 */
#endif
		tdb = gettdb(sspi, &sdst, sproto);
		if (tdb == NULL) {
			splx(s);
			error = EHOSTUNREACH;
			m_freem(m);
			goto senderr;	/*XXX*/
		}

		m->m_flags &= ~(M_BCAST | M_MCAST);	/* just in case */

		/* Callee frees mbuf */
		error = ipsp_process_packet(m, tdb, AF_INET6, 0);
		splx(s);
		m_freem(mcopy);
		goto freert;
	}
#endif /* IPSEC */

	if (m->m_pkthdr.len > IN6_LINKMTU(rt->rt_ifp)) {
		in6_ifstat_inc(rt->rt_ifp, ifs6_in_toobig);
		if (mcopy) {
			u_long mtu;

			mtu = IN6_LINKMTU(rt->rt_ifp);

			icmp6_error(mcopy, ICMP6_PACKET_TOO_BIG, 0, mtu);
		}
		m_freem(m);
		goto freert;
	}

	if (rt->rt_flags & RTF_GATEWAY)
		dst = (struct sockaddr_in6 *)rt->rt_gateway;

	/*
	 * If we are to forward the packet using the same interface
	 * as one we got the packet from, perhaps we should send a redirect
	 * to sender to shortcut a hop.
	 * Only send redirect if source is sending directly to us,
	 * and if packet was not source routed (or has any options).
	 * Also, don't send redirect if forwarding using a route
	 * modified by a redirect.
	 */
	if (rt->rt_ifp == m->m_pkthdr.rcvif && !srcrt && ip6_sendredirects &&
	    (rt->rt_flags & (RTF_DYNAMIC|RTF_MODIFIED)) == 0) {
		if ((rt->rt_ifp->if_flags & IFF_POINTOPOINT) &&
		    nd6_is_addr_neighbor((struct sockaddr_in6 *)&ip6_forward_rt.ro_dst, rt->rt_ifp)) {
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
			icmp6_error(mcopy, ICMP6_DST_UNREACH,
				    ICMP6_DST_UNREACH_ADDR, 0);
			m_freem(m);
			goto freert;
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
	if ((rt->rt_ifp->if_flags & IFF_LOOPBACK) != 0) {
		/*
		 * See corresponding comments in ip6_output.
		 * XXX: but is it possible that ip6_forward() sends a packet
		 *      to a loopback interface? I don't think so, and thus
		 *      I bark here. (jinmei@kame.net)
		 * XXX: it is common to route invalid packets to loopback.
		 *	also, the codepath will be visited on use of ::1 in
		 *	rthdr. (itojun)
		 */
#if 1
		if (0)
#else
		if ((rt->rt_flags & (RTF_BLACKHOLE|RTF_REJECT)) == 0)
#endif
		{
			printf("ip6_forward: outgoing interface is loopback. "
			       "src %s, dst %s, nxt %d, rcvif %s, outif %s\n",
			       ip6_sprintf(&ip6->ip6_src),
			       ip6_sprintf(&ip6->ip6_dst),
			       ip6->ip6_nxt, m->m_pkthdr.rcvif->if_xname,
			       rt->rt_ifp->if_xname);
		}

		/* we can just use rcvif in forwarding. */
		origifp = m->m_pkthdr.rcvif;
	}
	else
		origifp = rt->rt_ifp;
	if (IN6_IS_SCOPE_EMBED(&ip6->ip6_src))
		ip6->ip6_src.s6_addr16[1] = 0;
	if (IN6_IS_SCOPE_EMBED(&ip6->ip6_dst))
		ip6->ip6_dst.s6_addr16[1] = 0;

#if NPF > 0 
	if (pf_test6(PF_OUT, rt->rt_ifp, &m, NULL) != PF_PASS) {
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
		goto reroute;
	}
#endif 

	error = nd6_output(rt->rt_ifp, origifp, m, dst, rt);
	if (error) {
		in6_ifstat_inc(rt->rt_ifp, ifs6_out_discard);
		ip6stat.ip6s_cantforward++;
	} else {
		ip6stat.ip6s_forward++;
		in6_ifstat_inc(rt->rt_ifp, ifs6_out_forward);
		if (type)
			ip6stat.ip6s_redirectsent++;
		else {
			if (mcopy)
				goto freecopy;
		}
	}

#if NPF > 0 || defined(IPSEC)
senderr:
#endif
	if (mcopy == NULL)
		goto freert;
	switch (error) {
	case 0:
		if (type == ND_REDIRECT) {
			icmp6_redirect_output(mcopy, rt);
			goto freert;
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
	goto freert;

 freecopy:
	m_freem(mcopy);
 freert:
#ifndef SMALL_KERNEL
	if (ip6_multipath && ip6_forward_rt.ro_rt &&
	    (ip6_forward_rt.ro_rt->rt_flags & RTF_MPATH)) {
		RTFREE(ip6_forward_rt.ro_rt);
		ip6_forward_rt.ro_rt = 0;
	}
#endif
	return;
}
