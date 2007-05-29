/*	$OpenBSD: ip_output.c,v 1.186 2007/05/29 17:46:24 henning Exp $	*/
/*	$NetBSD: ip_output.c,v 1.28 1996/02/13 23:43:07 christos Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1988, 1990, 1993
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
 *	@(#)ip_output.c	8.3 (Berkeley) 1/21/94
 */

#include "pf.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/proc.h>
#include <sys/kernel.h>

#include <net/if.h>
#include <net/if_enc.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>
#include <netinet/ip_var.h>
#include <netinet/ip_icmp.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/udp_var.h>

#if NPF > 0
#include <net/pfvar.h>
#endif

#ifdef IPSEC
#ifdef ENCDEBUG
#define DPRINTF(x)    do { if (encdebug) printf x ; } while (0)
#else
#define DPRINTF(x)
#endif

extern u_int8_t get_sa_require(struct inpcb *);

extern int ipsec_auth_default_level;
extern int ipsec_esp_trans_default_level;
extern int ipsec_esp_network_default_level;
extern int ipsec_ipcomp_default_level;
extern int ipforwarding;
extern int ipsec_in_use;
#endif /* IPSEC */

#ifdef MROUTING
extern int ipmforwarding;
#endif

struct mbuf *ip_insertoptions(struct mbuf *, struct mbuf *, int *);
void ip_mloopback(struct ifnet *, struct mbuf *, struct sockaddr_in *);

/*
 * IP output.  The packet in mbuf chain m contains a skeletal IP
 * header (with len, off, ttl, proto, tos, src, dst).
 * The mbuf chain containing the packet will be freed.
 * The mbuf opt, if present, will not be freed.
 */
int
ip_output(struct mbuf *m0, ...)
{
	struct ip *ip;
	struct ifnet *ifp;
	struct mbuf *m = m0;
	int hlen = sizeof (struct ip);
	int len, error = 0;
	struct route iproute;
	struct sockaddr_in *dst;
	struct in_ifaddr *ia;
	struct mbuf *opt;
	struct route *ro;
	int flags;
	struct ip_moptions *imo;
	va_list ap;
	u_int8_t sproto = 0, donerouting = 0;
	u_long mtu;
#ifdef IPSEC
	u_int32_t icmp_mtu = 0;
	union sockaddr_union sdst;
	u_int32_t sspi;
	struct m_tag *mtag;
	struct tdb_ident *tdbi;

	struct inpcb *inp;
	struct tdb *tdb;
	int s;
#endif /* IPSEC */

	va_start(ap, m0);
	opt = va_arg(ap, struct mbuf *);
	ro = va_arg(ap, struct route *);
	flags = va_arg(ap, int);
	imo = va_arg(ap, struct ip_moptions *);
#ifdef IPSEC
	inp = va_arg(ap, struct inpcb *);
	if (inp && (inp->inp_flags & INP_IPV6) != 0)
		panic("ip_output: IPv6 pcb is passed");
#endif /* IPSEC */
	va_end(ap);

#ifdef	DIAGNOSTIC
	if ((m->m_flags & M_PKTHDR) == 0)
		panic("ip_output no HDR");
#endif
	if (opt) {
		m = ip_insertoptions(m, opt, &len);
		hlen = len;
	}

	ip = mtod(m, struct ip *);

	/*
	 * Fill in IP header.
	 */
	if ((flags & (IP_FORWARDING|IP_RAWOUTPUT)) == 0) {
		ip->ip_v = IPVERSION;
		ip->ip_off &= htons(IP_DF);
		ip->ip_id = htons(ip_randomid());
		ip->ip_hl = hlen >> 2;
		ipstat.ips_localout++;
	} else {
		hlen = ip->ip_hl << 2;
	}

	/*
	 * If we're missing the IP source address, do a route lookup. We'll
	 * remember this result, in case we don't need to do any IPsec
	 * processing on the packet. We need the source address so we can
	 * do an SPD lookup in IPsec; for most packets, the source address
	 * is set at a higher level protocol. ICMPs and other packets
	 * though (e.g., traceroute) have a source address of zeroes.
	 */
	if (ip->ip_src.s_addr == INADDR_ANY) {
		if (flags & IP_ROUTETOETHER) {
			error = EINVAL;
			goto bad;
		}
		donerouting = 1;

		if (ro == 0) {
			ro = &iproute;
			bzero((caddr_t)ro, sizeof (*ro));
		}

		dst = satosin(&ro->ro_dst);

		/*
		 * If there is a cached route, check that it is to the same
		 * destination and is still up.  If not, free it and try again.
		 */
		if (ro->ro_rt && ((ro->ro_rt->rt_flags & RTF_UP) == 0 ||
				  dst->sin_addr.s_addr != ip->ip_dst.s_addr)) {
			RTFREE(ro->ro_rt);
			ro->ro_rt = (struct rtentry *)0;
		}

		if (ro->ro_rt == 0) {
			dst->sin_family = AF_INET;
			dst->sin_len = sizeof(*dst);
			dst->sin_addr = ip->ip_dst;
		}

		/*
		 * If routing to interface only, short-circuit routing lookup.
		 */
		if (flags & IP_ROUTETOIF) {
			if ((ia = ifatoia(ifa_ifwithdstaddr(sintosa(dst)))) == 0 &&
			    (ia = ifatoia(ifa_ifwithnet(sintosa(dst)))) == 0) {
			    ipstat.ips_noroute++;
			    error = ENETUNREACH;
			    goto bad;
			}

			ifp = ia->ia_ifp;
			mtu = ifp->if_mtu;
			ip->ip_ttl = 1;
		} else if ((IN_MULTICAST(ip->ip_dst.s_addr) ||
		    (ip->ip_dst.s_addr == INADDR_BROADCAST)) &&
		    imo != NULL && imo->imo_multicast_ifp != NULL) {
			ifp = imo->imo_multicast_ifp;
			mtu = ifp->if_mtu;
			IFP_TO_IA(ifp, ia);
		} else {
			if (ro->ro_rt == 0)
				rtalloc_mpath(ro, NULL, 0);

			if (ro->ro_rt == 0) {
				ipstat.ips_noroute++;
				error = EHOSTUNREACH;
				goto bad;
			}

			ia = ifatoia(ro->ro_rt->rt_ifa);
			ifp = ro->ro_rt->rt_ifp;
			if ((mtu = ro->ro_rt->rt_rmx.rmx_mtu) == 0)
				mtu = ifp->if_mtu;
			ro->ro_rt->rt_use++;

			if (ro->ro_rt->rt_flags & RTF_GATEWAY)
				dst = satosin(ro->ro_rt->rt_gateway);
		}

		/* Set the source IP address */
		if (!IN_MULTICAST(ip->ip_dst.s_addr))
			ip->ip_src = ia->ia_addr.sin_addr;
	}

#ifdef IPSEC
	if (!ipsec_in_use && inp == NULL)
		goto done_spd;

	/*
	 * splnet is chosen over spltdb because we are not allowed to
	 * lower the level, and udp_output calls us in splnet().
	 */
	s = splnet();

	/* Do we have any pending SAs to apply ? */
	mtag = m_tag_find(m, PACKET_TAG_IPSEC_PENDING_TDB, NULL);
	if (mtag != NULL) {
#ifdef DIAGNOSTIC
		if (mtag->m_tag_len != sizeof (struct tdb_ident))
			panic("ip_output: tag of length %d (should be %d",
			    mtag->m_tag_len, sizeof (struct tdb_ident));
#endif
		tdbi = (struct tdb_ident *)(mtag + 1);
		tdb = gettdb(tdbi->spi, &tdbi->dst, tdbi->proto);
		if (tdb == NULL)
			error = -EINVAL;
		m_tag_delete(m, mtag);
	}
	else
		tdb = ipsp_spd_lookup(m, AF_INET, hlen, &error,
		    IPSP_DIRECTION_OUT, NULL, inp);

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

			m_freem(m);
			goto done;
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

		/*
		 * If it needs TCP/UDP hardware-checksumming, do the
		 * computation now.
		 */
		if (m->m_pkthdr.csum_flags & (M_TCPV4_CSUM_OUT | M_UDPV4_CSUM_OUT)) {
			in_delayed_cksum(m);
			m->m_pkthdr.csum_flags &=
			    ~(M_UDPV4_CSUM_OUT | M_TCPV4_CSUM_OUT);
		}

		/* If it's not a multicast packet, try to fast-path */
		if (!IN_MULTICAST(ip->ip_dst.s_addr)) {
			goto sendit;
		}
	}

	/* Fall through to the routing/multicast handling code */
 done_spd:
#endif /* IPSEC */

	if (flags & IP_ROUTETOETHER) {
		dst = satosin(&ro->ro_dst);
		ifp = ro->ro_rt->rt_ifp;
		mtu = ifp->if_mtu;
		ro->ro_rt = NULL;
	} else if (donerouting == 0) {
		if (ro == 0) {
			ro = &iproute;
			bzero((caddr_t)ro, sizeof (*ro));
		}

		dst = satosin(&ro->ro_dst);

		/*
		 * If there is a cached route, check that it is to the same
		 * destination and is still up.  If not, free it and try again.
		 */
		if (ro->ro_rt && ((ro->ro_rt->rt_flags & RTF_UP) == 0 ||
				  dst->sin_addr.s_addr != ip->ip_dst.s_addr)) {
			RTFREE(ro->ro_rt);
			ro->ro_rt = (struct rtentry *)0;
		}

		if (ro->ro_rt == 0) {
			dst->sin_family = AF_INET;
			dst->sin_len = sizeof(*dst);
			dst->sin_addr = ip->ip_dst;
		}

		/*
		 * If routing to interface only, short-circuit routing lookup.
		 */
		if (flags & IP_ROUTETOIF) {
			if ((ia = ifatoia(ifa_ifwithdstaddr(sintosa(dst)))) == 0 &&
			    (ia = ifatoia(ifa_ifwithnet(sintosa(dst)))) == 0) {
			    ipstat.ips_noroute++;
			    error = ENETUNREACH;
			    goto bad;
			}

			ifp = ia->ia_ifp;
			mtu = ifp->if_mtu;
			ip->ip_ttl = 1;
		} else if ((IN_MULTICAST(ip->ip_dst.s_addr) ||
		    (ip->ip_dst.s_addr == INADDR_BROADCAST)) &&
		    imo != NULL && imo->imo_multicast_ifp != NULL) {
			ifp = imo->imo_multicast_ifp;
			mtu = ifp->if_mtu;
			IFP_TO_IA(ifp, ia);
		} else {
			if (ro->ro_rt == 0)
				rtalloc_mpath(ro, &ip->ip_src.s_addr, 0);

			if (ro->ro_rt == 0) {
				ipstat.ips_noroute++;
				error = EHOSTUNREACH;
				goto bad;
			}

			ia = ifatoia(ro->ro_rt->rt_ifa);
			ifp = ro->ro_rt->rt_ifp;
			if ((mtu = ro->ro_rt->rt_rmx.rmx_mtu) == 0)
				mtu = ifp->if_mtu;
			ro->ro_rt->rt_use++;

			if (ro->ro_rt->rt_flags & RTF_GATEWAY)
				dst = satosin(ro->ro_rt->rt_gateway);
		}

		/* Set the source IP address */
		if (ip->ip_src.s_addr == INADDR_ANY)
			ip->ip_src = ia->ia_addr.sin_addr;
	}

	if (IN_MULTICAST(ip->ip_dst.s_addr) ||
	    (ip->ip_dst.s_addr == INADDR_BROADCAST)) {
		struct in_multi *inm;

		m->m_flags |= (ip->ip_dst.s_addr == INADDR_BROADCAST) ?
			M_BCAST : M_MCAST;

		/*
		 * IP destination address is multicast.  Make sure "dst"
		 * still points to the address in "ro".  (It may have been
		 * changed to point to a gateway address, above.)
		 */
		dst = satosin(&ro->ro_dst);

		/*
		 * See if the caller provided any multicast options
		 */
		if (imo != NULL)
			ip->ip_ttl = imo->imo_multicast_ttl;
		else
			ip->ip_ttl = IP_DEFAULT_MULTICAST_TTL;

		/*
		 * if we don't know the outgoing ifp yet, we can't generate
		 * output
		 */
		if (!ifp) {
			ipstat.ips_noroute++;
			error = EHOSTUNREACH;
			goto bad;
		}

		/*
		 * Confirm that the outgoing interface supports multicast,
		 * but only if the packet actually is going out on that
		 * interface (i.e., no IPsec is applied).
		 */
		if ((((m->m_flags & M_MCAST) &&
		      (ifp->if_flags & IFF_MULTICAST) == 0) ||
		     ((m->m_flags & M_BCAST) &&
		      (ifp->if_flags & IFF_BROADCAST) == 0)) && (sproto == 0)) {
			ipstat.ips_noroute++;
			error = ENETUNREACH;
			goto bad;
		}

		/*
		 * If source address not specified yet, use address
		 * of outgoing interface.
		 */
		if (ip->ip_src.s_addr == INADDR_ANY) {
			struct in_ifaddr *ia;

			TAILQ_FOREACH(ia, &in_ifaddr, ia_list)
				if (ia->ia_ifp == ifp) {
					ip->ip_src = ia->ia_addr.sin_addr;
					break;
				}
		}

		IN_LOOKUP_MULTI(ip->ip_dst, ifp, inm);
		if (inm != NULL &&
		   (imo == NULL || imo->imo_multicast_loop)) {
			/*
			 * If we belong to the destination multicast group
			 * on the outgoing interface, and the caller did not
			 * forbid loopback, loop back a copy.
			 * Can't defer TCP/UDP checksumming, do the
			 * computation now.
			 */
			if (m->m_pkthdr.csum_flags &
			    (M_TCPV4_CSUM_OUT | M_UDPV4_CSUM_OUT)) {
				in_delayed_cksum(m);
				m->m_pkthdr.csum_flags &=
				    ~(M_UDPV4_CSUM_OUT | M_TCPV4_CSUM_OUT);
			}
			ip_mloopback(ifp, m, dst);
		}
#ifdef MROUTING
		else {
			/*
			 * If we are acting as a multicast router, perform
			 * multicast forwarding as if the packet had just
			 * arrived on the interface to which we are about
			 * to send.  The multicast forwarding function
			 * recursively calls this function, using the
			 * IP_FORWARDING flag to prevent infinite recursion.
			 *
			 * Multicasts that are looped back by ip_mloopback(),
			 * above, will be forwarded by the ip_input() routine,
			 * if necessary.
			 */
			extern struct socket *ip_mrouter;

			if (ipmforwarding && ip_mrouter &&
			    (flags & IP_FORWARDING) == 0) {
				if (ip_mforward(m, ifp) != 0) {
					m_freem(m);
					goto done;
				}
			}
		}
#endif
		/*
		 * Multicasts with a time-to-live of zero may be looped-
		 * back, above, but must not be transmitted on a network.
		 * Also, multicasts addressed to the loopback interface
		 * are not sent -- the above call to ip_mloopback() will
		 * loop back a copy if this host actually belongs to the
		 * destination group on the loopback interface.
		 */
		if (ip->ip_ttl == 0 || (ifp->if_flags & IFF_LOOPBACK) != 0) {
			m_freem(m);
			goto done;
		}

		goto sendit;
	}

	/*
	 * Look for broadcast address and and verify user is allowed to send
	 * such a packet; if the packet is going in an IPsec tunnel, skip
	 * this check.
	 */
	if ((sproto == 0) && (in_broadcast(dst->sin_addr, ifp))) {
		if ((ifp->if_flags & IFF_BROADCAST) == 0) {
			error = EADDRNOTAVAIL;
			goto bad;
		}
		if ((flags & IP_ALLOWBROADCAST) == 0) {
			error = EACCES;
			goto bad;
		}

		/* Don't allow broadcast messages to be fragmented */
		if (ntohs(ip->ip_len) > ifp->if_mtu) {
			error = EMSGSIZE;
			goto bad;
		}
		m->m_flags |= M_BCAST;
	} else
		m->m_flags &= ~M_BCAST;

sendit:
	/*
	 * If we're doing Path MTU discovery, we need to set DF unless
	 * the route's MTU is locked.
	 */
	if ((flags & IP_MTUDISC) && ro && ro->ro_rt &&
	    (ro->ro_rt->rt_rmx.rmx_locks & RTV_MTU) == 0)
		ip->ip_off |= htons(IP_DF);

#ifdef IPSEC
	/*
	 * Check if the packet needs encapsulation.
	 */
	if (sproto != 0) {
		s = splnet();

		/*
		 * Packet filter
		 */
#if NPF > 0

		if (pf_test(PF_OUT, &encif[0].sc_if, &m, NULL) != PF_PASS) {
			error = EHOSTUNREACH;
			splx(s);
			m_freem(m);
			goto done;
		}
		if (m == NULL) {
			splx(s);
			goto done;
		}
		ip = mtod(m, struct ip *);
		hlen = ip->ip_hl << 2;
#endif

		tdb = gettdb(sspi, &sdst, sproto);
		if (tdb == NULL) {
			DPRINTF(("ip_output: unknown TDB"));
			error = EHOSTUNREACH;
			splx(s);
			m_freem(m);
			goto done;
		}

		/* Check if we are allowed to fragment */
		if (ip_mtudisc && (ip->ip_off & htons(IP_DF)) && tdb->tdb_mtu &&
		    ntohs(ip->ip_len) > tdb->tdb_mtu &&
		    tdb->tdb_mtutimeout > time_second) {
			struct rtentry *rt = NULL;
			int rt_mtucloned = 0;
			int transportmode = 0;

			transportmode = (tdb->tdb_dst.sa.sa_family == AF_INET) &&
			    (tdb->tdb_dst.sin.sin_addr.s_addr ==
			    ip->ip_dst.s_addr);
			icmp_mtu = tdb->tdb_mtu;
			splx(s);

			/* Find a host route to store the mtu in */
			if (ro != NULL)
				rt = ro->ro_rt;
			/* but don't add a PMTU route for transport mode SAs */
			if (transportmode)
				rt = NULL;
			else if (rt == NULL || (rt->rt_flags & RTF_HOST) == 0) {
				struct sockaddr_in dst = {
					sizeof(struct sockaddr_in), AF_INET};
				dst.sin_addr = ip->ip_dst;
				rt = icmp_mtudisc_clone((struct sockaddr *)&dst);
				rt_mtucloned = 1;
			}
			DPRINTF(("ip_output: spi %08x mtu %d rt %p cloned %d\n",
			    ntohl(tdb->tdb_spi), icmp_mtu, rt, rt_mtucloned));
			if (rt != NULL) {
				rt->rt_rmx.rmx_mtu = icmp_mtu;
				if (ro && ro->ro_rt != NULL) {
					RTFREE(ro->ro_rt);
					ro->ro_rt = (struct rtentry *) 0;
					rtalloc(ro);
				}
				if (rt_mtucloned)
					rtfree(rt);
			}
			error = EMSGSIZE;
			goto bad;
		}

		/*
		 * Clear these -- they'll be set in the recursive invocation
		 * as needed.
		 */
		m->m_flags &= ~(M_MCAST | M_BCAST);

		/* Callee frees mbuf */
		error = ipsp_process_packet(m, tdb, AF_INET, 0);
		splx(s);
		return error;  /* Nothing more to be done */
	}

	/*
	 * If deferred crypto processing is needed, check that the
	 * interface supports it.
	 */
	if (ipsec_in_use && (mtag = m_tag_find(m,
	    PACKET_TAG_IPSEC_OUT_CRYPTO_NEEDED, NULL)) != NULL &&
	    (ifp->if_capabilities & IFCAP_IPSEC) == 0) {
		/* Notify IPsec to do its own crypto. */
		ipsp_skipcrypto_unmark((struct tdb_ident *)(mtag + 1));
		m_freem(m);
		error = EHOSTUNREACH;
		goto done;
	}
#endif /* IPSEC */

	/* Catch routing changes wrt. hardware checksumming for TCP or UDP. */
	if (m->m_pkthdr.csum_flags & M_TCPV4_CSUM_OUT) {
		if (!(ifp->if_capabilities & IFCAP_CSUM_TCPv4) ||
		    ifp->if_bridge != NULL) {
			in_delayed_cksum(m);
			m->m_pkthdr.csum_flags &= ~M_TCPV4_CSUM_OUT; /* Clear */
		}
	} else if (m->m_pkthdr.csum_flags & M_UDPV4_CSUM_OUT) {
		if (!(ifp->if_capabilities & IFCAP_CSUM_UDPv4) ||
		    ifp->if_bridge != NULL) {
			in_delayed_cksum(m);
			m->m_pkthdr.csum_flags &= ~M_UDPV4_CSUM_OUT; /* Clear */
		}
	}

	/*
	 * Packet filter
	 */
#if NPF > 0
	if (pf_test(PF_OUT, ifp, &m, NULL) != PF_PASS) {
		error = EHOSTUNREACH;
		m_freem(m);
		goto done;
	}
	if (m == NULL)
		goto done;

	ip = mtod(m, struct ip *);
	hlen = ip->ip_hl << 2;
#endif

#ifdef IPSEC
	if (ipsec_in_use && (flags & IP_FORWARDING) && (ipforwarding == 2) &&
	    (m_tag_find(m, PACKET_TAG_IPSEC_IN_DONE, NULL) == NULL)) {
		error = EHOSTUNREACH;
		m_freem(m);
		goto done;
	}
#endif

	/* XXX
	 * Try to use jumbograms based on socket option, or the route
	 * or... for other reasons later on. 
	 */
	if ((flags & IP_JUMBO) && ro->ro_rt && (ro->ro_rt->rt_flags & RTF_JUMBO) &&
	    ro->ro_rt->rt_ifp)
		mtu = ro->ro_rt->rt_ifp->if_hardmtu;

	/*
	 * If small enough for interface, can just send directly.
	 */
	if (ntohs(ip->ip_len) <= mtu) {
		if ((ifp->if_capabilities & IFCAP_CSUM_IPv4) &&
		    ifp->if_bridge == NULL) {
			m->m_pkthdr.csum_flags |= M_IPV4_CSUM_OUT;
			ipstat.ips_outhwcsum++;
		} else {
			ip->ip_sum = 0;
			ip->ip_sum = in_cksum(m, hlen);
		}
		/* Update relevant hardware checksum stats for TCP/UDP */
		if (m->m_pkthdr.csum_flags & M_TCPV4_CSUM_OUT)
			tcpstat.tcps_outhwcsum++;
		else if (m->m_pkthdr.csum_flags & M_UDPV4_CSUM_OUT)
			udpstat.udps_outhwcsum++;
		error = (*ifp->if_output)(ifp, m, sintosa(dst), ro->ro_rt);
		goto done;
	}

	/*
	 * Too large for interface; fragment if possible.
	 * Must be able to put at least 8 bytes per fragment.
	 */
	if (ip->ip_off & htons(IP_DF)) {
#ifdef IPSEC
		icmp_mtu = ifp->if_mtu;
#endif
		error = EMSGSIZE;
		/*
		 * This case can happen if the user changed the MTU
		 * of an interface after enabling IP on it.  Because
		 * most netifs don't keep track of routes pointing to
		 * them, there is no way for one to update all its
		 * routes when the MTU is changed.
		 */
		if ((ro->ro_rt->rt_flags & (RTF_UP | RTF_HOST)) &&
		    !(ro->ro_rt->rt_rmx.rmx_locks & RTV_MTU) &&
		    (ro->ro_rt->rt_rmx.rmx_mtu > ifp->if_mtu)) {
			ro->ro_rt->rt_rmx.rmx_mtu = ifp->if_mtu;
		}
		ipstat.ips_cantfrag++;
		goto bad;
	}

	error = ip_fragment(m, ifp, mtu);
	if (error) {
		m = m0 = NULL;
		goto bad;
	}

	for (; m; m = m0) {
		m0 = m->m_nextpkt;
		m->m_nextpkt = 0;
		if (error == 0)
			error = (*ifp->if_output)(ifp, m, sintosa(dst),
			    ro->ro_rt);
		else
			m_freem(m);
	}

	if (error == 0)
		ipstat.ips_fragmented++;

done:
	if (ro == &iproute && (flags & IP_ROUTETOIF) == 0 && ro->ro_rt)
		RTFREE(ro->ro_rt);
	return (error);
bad:
#ifdef IPSEC
	if (error == EMSGSIZE && ip_mtudisc && icmp_mtu != 0 && m != NULL)
		ipsec_adjust_mtu(m, icmp_mtu);
#endif
	m_freem(m0);
	goto done;
}

int
ip_fragment(struct mbuf *m, struct ifnet *ifp, u_long mtu)
{
	struct ip *ip, *mhip;
	struct mbuf *m0;
	int len, hlen, off;
	int mhlen, firstlen;
	struct mbuf **mnext;
	int fragments = 0;
	int s;
	int error = 0;

	ip = mtod(m, struct ip *);
	hlen = ip->ip_hl << 2;

	len = (mtu - hlen) &~ 7;
	if (len < 8) {
		m_freem(m);
		return (EMSGSIZE);
	}

	/*
	 * If we are doing fragmentation, we can't defer TCP/UDP
	 * checksumming; compute the checksum and clear the flag.
	 */
	if (m->m_pkthdr.csum_flags & (M_TCPV4_CSUM_OUT | M_UDPV4_CSUM_OUT)) {
		in_delayed_cksum(m);
		m->m_pkthdr.csum_flags &= ~(M_UDPV4_CSUM_OUT | M_TCPV4_CSUM_OUT);
	}

	firstlen = len;
	mnext = &m->m_nextpkt;

	/*
	 * Loop through length of segment after first fragment,
	 * make new header and copy data of each part and link onto chain.
	 */
	m0 = m;
	mhlen = sizeof (struct ip);
	for (off = hlen + len; off < ntohs(ip->ip_len); off += len) {
		MGETHDR(m, M_DONTWAIT, MT_HEADER);
		if (m == 0) {
			ipstat.ips_odropped++;
			error = ENOBUFS;
			goto sendorfree;
		}
		*mnext = m;
		mnext = &m->m_nextpkt;
		m->m_data += max_linkhdr;
		mhip = mtod(m, struct ip *);
		*mhip = *ip;
		/* we must inherit MCAST and BCAST flags */
		m->m_flags |= m0->m_flags & (M_MCAST|M_BCAST);
		if (hlen > sizeof (struct ip)) {
			mhlen = ip_optcopy(ip, mhip) + sizeof (struct ip);
			mhip->ip_hl = mhlen >> 2;
		}
		m->m_len = mhlen;
		mhip->ip_off = ((off - hlen) >> 3) +
		    (ntohs(ip->ip_off) & ~IP_MF);
		if (ip->ip_off & htons(IP_MF))
			mhip->ip_off |= IP_MF;
		if (off + len >= ntohs(ip->ip_len))
			len = ntohs(ip->ip_len) - off;
		else
			mhip->ip_off |= IP_MF;
		mhip->ip_len = htons((u_int16_t)(len + mhlen));
		m->m_next = m_copy(m0, off, len);
		if (m->m_next == 0) {
			ipstat.ips_odropped++;
			error = ENOBUFS;
			goto sendorfree;
		}
		m->m_pkthdr.len = mhlen + len;
		m->m_pkthdr.rcvif = (struct ifnet *)0;
		mhip->ip_off = htons((u_int16_t)mhip->ip_off);
		if ((ifp != NULL) &&
		    (ifp->if_capabilities & IFCAP_CSUM_IPv4) &&
		    ifp->if_bridge == NULL) {
			m->m_pkthdr.csum_flags |= M_IPV4_CSUM_OUT;
			ipstat.ips_outhwcsum++;
		} else {
			mhip->ip_sum = 0;
			mhip->ip_sum = in_cksum(m, mhlen);
		}
		ipstat.ips_ofragments++;
		fragments++;
	}
	/*
	 * Update first fragment by trimming what's been copied out
	 * and updating header, then send each fragment (in order).
	 */
	m = m0;
	m_adj(m, hlen + firstlen - ntohs(ip->ip_len));
	m->m_pkthdr.len = hlen + firstlen;
	ip->ip_len = htons((u_int16_t)m->m_pkthdr.len);
	ip->ip_off |= htons(IP_MF);
	if ((ifp != NULL) &&
	    (ifp->if_capabilities & IFCAP_CSUM_IPv4) &&
	    ifp->if_bridge == NULL) {
		m->m_pkthdr.csum_flags |= M_IPV4_CSUM_OUT;
		ipstat.ips_outhwcsum++;
	} else {
		ip->ip_sum = 0;
		ip->ip_sum = in_cksum(m, hlen);
	}
sendorfree:
	/*
	 * If there is no room for all the fragments, don't queue
	 * any of them.
	 */
	if (ifp != NULL) {
		s = splnet();
		if (ifp->if_snd.ifq_maxlen - ifp->if_snd.ifq_len < fragments &&
		    error == 0) {
			error = ENOBUFS;
			ipstat.ips_odropped++;
			IFQ_INC_DROPS(&ifp->if_snd);
		}
		splx(s);
	}
	if (error) {
		for (m = m0; m; m = m0) {
			m0 = m->m_nextpkt;
			m->m_nextpkt = NULL;
			m_freem(m);
		}
	}

	return (error);
}

/*
 * Insert IP options into preformed packet.
 * Adjust IP destination as required for IP source routing,
 * as indicated by a non-zero in_addr at the start of the options.
 */
struct mbuf *
ip_insertoptions(m, opt, phlen)
	struct mbuf *m;
	struct mbuf *opt;
	int *phlen;
{
	struct ipoption *p = mtod(opt, struct ipoption *);
	struct mbuf *n;
	struct ip *ip = mtod(m, struct ip *);
	unsigned optlen;

	optlen = opt->m_len - sizeof(p->ipopt_dst);
	if (optlen + ntohs(ip->ip_len) > IP_MAXPACKET)
		return (m);		/* XXX should fail */
	if (p->ipopt_dst.s_addr)
		ip->ip_dst = p->ipopt_dst;
	if (m->m_flags & M_EXT || m->m_data - optlen < m->m_pktdat) {
		MGETHDR(n, M_DONTWAIT, MT_HEADER);
		if (n == 0)
			return (m);
		M_MOVE_HDR(n, m);
		n->m_pkthdr.len += optlen;
		m->m_len -= sizeof(struct ip);
		m->m_data += sizeof(struct ip);
		n->m_next = m;
		m = n;
		m->m_len = optlen + sizeof(struct ip);
		m->m_data += max_linkhdr;
		bcopy((caddr_t)ip, mtod(m, caddr_t), sizeof(struct ip));
	} else {
		m->m_data -= optlen;
		m->m_len += optlen;
		m->m_pkthdr.len += optlen;
		ovbcopy((caddr_t)ip, mtod(m, caddr_t), sizeof(struct ip));
	}
	ip = mtod(m, struct ip *);
	bcopy((caddr_t)p->ipopt_list, (caddr_t)(ip + 1), (unsigned)optlen);
	*phlen = sizeof(struct ip) + optlen;
	ip->ip_len = htons(ntohs(ip->ip_len) + optlen);
	return (m);
}

/*
 * Copy options from ip to jp,
 * omitting those not copied during fragmentation.
 */
int
ip_optcopy(ip, jp)
	struct ip *ip, *jp;
{
	u_char *cp, *dp;
	int opt, optlen, cnt;

	cp = (u_char *)(ip + 1);
	dp = (u_char *)(jp + 1);
	cnt = (ip->ip_hl << 2) - sizeof (struct ip);
	for (; cnt > 0; cnt -= optlen, cp += optlen) {
		opt = cp[0];
		if (opt == IPOPT_EOL)
			break;
		if (opt == IPOPT_NOP) {
			/* Preserve for IP mcast tunnel's LSRR alignment. */
			*dp++ = IPOPT_NOP;
			optlen = 1;
			continue;
		}
#ifdef DIAGNOSTIC
		if (cnt < IPOPT_OLEN + sizeof(*cp))
			panic("malformed IPv4 option passed to ip_optcopy");
#endif
		optlen = cp[IPOPT_OLEN];
#ifdef DIAGNOSTIC
		if (optlen < IPOPT_OLEN + sizeof(*cp) || optlen > cnt)
			panic("malformed IPv4 option passed to ip_optcopy");
#endif
		/* bogus lengths should have been caught by ip_dooptions */
		if (optlen > cnt)
			optlen = cnt;
		if (IPOPT_COPIED(opt)) {
			bcopy((caddr_t)cp, (caddr_t)dp, (unsigned)optlen);
			dp += optlen;
		}
	}
	for (optlen = dp - (u_char *)(jp+1); optlen & 0x3; optlen++)
		*dp++ = IPOPT_EOL;
	return (optlen);
}

/*
 * IP socket option processing.
 */
int
ip_ctloutput(op, so, level, optname, mp)
	int op;
	struct socket *so;
	int level, optname;
	struct mbuf **mp;
{
	struct inpcb *inp = sotoinpcb(so);
	struct mbuf *m = *mp;
	int optval = 0;
#ifdef IPSEC
	struct proc *p = curproc; /* XXX */
	struct ipsec_ref *ipr;
	u_int16_t opt16val;
#endif
	int error = 0;

	if (level != IPPROTO_IP) {
		error = EINVAL;
		if (op == PRCO_SETOPT && *mp)
			(void) m_free(*mp);
	} else switch (op) {
	case PRCO_SETOPT:
		switch (optname) {
		case IP_OPTIONS:
#ifdef notyet
		case IP_RETOPTS:
			return (ip_pcbopts(optname, &inp->inp_options, m));
#else
			return (ip_pcbopts(&inp->inp_options, m));
#endif

		case IP_TOS:
		case IP_TTL:
		case IP_MINTTL:
		case IP_RECVOPTS:
		case IP_RECVRETOPTS:
		case IP_RECVDSTADDR:
		case IP_RECVIF:
		case IP_RECVTTL:
			if (m == NULL || m->m_len != sizeof(int))
				error = EINVAL;
			else {
				optval = *mtod(m, int *);
				switch (optname) {

				case IP_TOS:
					inp->inp_ip.ip_tos = optval;
					break;

				case IP_TTL:
					if (optval > 0 && optval <= MAXTTL)
						inp->inp_ip.ip_ttl = optval;
					else
						error = EINVAL;
					break;

				case IP_MINTTL:
					if (optval > 0 && optval <= MAXTTL)
						inp->inp_ip_minttl = optval;
					else
						error = EINVAL;
					break;
#define	OPTSET(bit) \
	if (optval) \
		inp->inp_flags |= bit; \
	else \
		inp->inp_flags &= ~bit;

				case IP_RECVOPTS:
					OPTSET(INP_RECVOPTS);
					break;

				case IP_RECVRETOPTS:
					OPTSET(INP_RECVRETOPTS);
					break;

				case IP_RECVDSTADDR:
					OPTSET(INP_RECVDSTADDR);
					break;
				case IP_RECVIF:
					OPTSET(INP_RECVIF);
					break;
				case IP_RECVTTL:
					OPTSET(INP_RECVTTL);
					break;
				}
			}
			break;
#undef OPTSET

		case IP_MULTICAST_IF:
		case IP_MULTICAST_TTL:
		case IP_MULTICAST_LOOP:
		case IP_ADD_MEMBERSHIP:
		case IP_DROP_MEMBERSHIP:
			error = ip_setmoptions(optname, &inp->inp_moptions, m);
			break;

		case IP_PORTRANGE:
			if (m == 0 || m->m_len != sizeof(int))
				error = EINVAL;
			else {
				optval = *mtod(m, int *);

				switch (optval) {

				case IP_PORTRANGE_DEFAULT:
					inp->inp_flags &= ~(INP_LOWPORT);
					inp->inp_flags &= ~(INP_HIGHPORT);
					break;

				case IP_PORTRANGE_HIGH:
					inp->inp_flags &= ~(INP_LOWPORT);
					inp->inp_flags |= INP_HIGHPORT;
					break;

				case IP_PORTRANGE_LOW:
					inp->inp_flags &= ~(INP_HIGHPORT);
					inp->inp_flags |= INP_LOWPORT;
					break;

				default:

					error = EINVAL;
					break;
				}
			}
			break;
		case IP_AUTH_LEVEL:
		case IP_ESP_TRANS_LEVEL:
		case IP_ESP_NETWORK_LEVEL:
		case IP_IPCOMP_LEVEL:
#ifndef IPSEC
			error = EOPNOTSUPP;
#else
			if (m == 0 || m->m_len != sizeof(int)) {
				error = EINVAL;
				break;
			}
			optval = *mtod(m, int *);

			if (optval < IPSEC_LEVEL_BYPASS ||
			    optval > IPSEC_LEVEL_UNIQUE) {
				error = EINVAL;
				break;
			}

			/* Unlink cached output TDB to force a re-search */
			if (inp->inp_tdb_out) {
				int s = spltdb();
				TAILQ_REMOVE(&inp->inp_tdb_out->tdb_inp_out,
				    inp, inp_tdb_out_next);
				splx(s);
			}

			if (inp->inp_tdb_in) {
				int s = spltdb();
				TAILQ_REMOVE(&inp->inp_tdb_in->tdb_inp_in,
				    inp, inp_tdb_in_next);
				splx(s);
			}

			switch (optname) {
			case IP_AUTH_LEVEL:
				if (optval < ipsec_auth_default_level &&
				    suser(p, 0)) {
					error = EACCES;
					break;
				}
				inp->inp_seclevel[SL_AUTH] = optval;
				break;

			case IP_ESP_TRANS_LEVEL:
				if (optval < ipsec_esp_trans_default_level &&
				    suser(p, 0)) {
					error = EACCES;
					break;
				}
				inp->inp_seclevel[SL_ESP_TRANS] = optval;
				break;

			case IP_ESP_NETWORK_LEVEL:
				if (optval < ipsec_esp_network_default_level &&
				    suser(p, 0)) {
					error = EACCES;
					break;
				}
				inp->inp_seclevel[SL_ESP_NETWORK] = optval;
				break;
			case IP_IPCOMP_LEVEL:
				if (optval < ipsec_ipcomp_default_level &&
				    suser(p, 0)) {
					error = EACCES;
					break;
				}
				inp->inp_seclevel[SL_IPCOMP] = optval;
				break;
			}
			if (!error)
				inp->inp_secrequire = get_sa_require(inp);
#endif
			break;

		case IP_IPSEC_REMOTE_CRED:
		case IP_IPSEC_REMOTE_AUTH:
			/* Can't set the remote credential or key */
			error = EOPNOTSUPP;
			break;

		case IP_IPSEC_LOCAL_ID:
		case IP_IPSEC_REMOTE_ID:
		case IP_IPSEC_LOCAL_CRED:
		case IP_IPSEC_LOCAL_AUTH:
#ifndef IPSEC
			error = EOPNOTSUPP;
#else
			if (m->m_len < 2) {
				error = EINVAL;
				break;
			}

			m_copydata(m, 0, 2, (caddr_t) &opt16val);

			/* If the type is 0, then we cleanup and return */
			if (opt16val == 0) {
				switch (optname) {
				case IP_IPSEC_LOCAL_ID:
					if (inp->inp_ipo != NULL &&
					    inp->inp_ipo->ipo_srcid != NULL) {
						ipsp_reffree(inp->inp_ipo->ipo_srcid);
						inp->inp_ipo->ipo_srcid = NULL;
					}
					break;

				case IP_IPSEC_REMOTE_ID:
					if (inp->inp_ipo != NULL &&
					    inp->inp_ipo->ipo_dstid != NULL) {
						ipsp_reffree(inp->inp_ipo->ipo_dstid);
						inp->inp_ipo->ipo_dstid = NULL;
					}
					break;

				case IP_IPSEC_LOCAL_CRED:
					if (inp->inp_ipo != NULL &&
					    inp->inp_ipo->ipo_local_cred != NULL) {
						ipsp_reffree(inp->inp_ipo->ipo_local_cred);
						inp->inp_ipo->ipo_local_cred = NULL;
					}
					break;

				case IP_IPSEC_LOCAL_AUTH:
					if (inp->inp_ipo != NULL &&
					    inp->inp_ipo->ipo_local_auth != NULL) {
						ipsp_reffree(inp->inp_ipo->ipo_local_auth);
						inp->inp_ipo->ipo_local_auth = NULL;
					}
					break;
				}

				error = 0;
				break;
			}

			/* Can't have an empty payload */
			if (m->m_len == 2) {
				error = EINVAL;
				break;
			}

			/* Allocate if needed */
			if (inp->inp_ipo == NULL) {
				inp->inp_ipo = ipsec_add_policy(inp,
				    AF_INET, IPSP_DIRECTION_OUT);
				if (inp->inp_ipo == NULL) {
					error = ENOBUFS;
					break;
				}
			}

			MALLOC(ipr, struct ipsec_ref *,
			       sizeof(struct ipsec_ref) + m->m_len - 2,
			       M_CREDENTIALS, M_NOWAIT);
			if (ipr == NULL) {
				error = ENOBUFS;
				break;
			}

			ipr->ref_count = 1;
			ipr->ref_malloctype = M_CREDENTIALS;
			ipr->ref_len = m->m_len - 2;
			ipr->ref_type = opt16val;
			m_copydata(m, 2, m->m_len - 2, (caddr_t)(ipr + 1));

			switch (optname) {
			case IP_IPSEC_LOCAL_ID:
				/* Check valid types and NUL-termination */
				if (ipr->ref_type < IPSP_IDENTITY_PREFIX ||
				    ipr->ref_type > IPSP_IDENTITY_CONNECTION ||
				    ((char *)(ipr + 1))[ipr->ref_len - 1]) {
					FREE(ipr, M_CREDENTIALS);
					error = EINVAL;
				} else {
					if (inp->inp_ipo->ipo_srcid != NULL)
						ipsp_reffree(inp->inp_ipo->ipo_srcid);
					inp->inp_ipo->ipo_srcid = ipr;
				}
				break;
			case IP_IPSEC_REMOTE_ID:
				/* Check valid types and NUL-termination */
				if (ipr->ref_type < IPSP_IDENTITY_PREFIX ||
				    ipr->ref_type > IPSP_IDENTITY_CONNECTION ||
				    ((char *)(ipr + 1))[ipr->ref_len - 1]) {
					FREE(ipr, M_CREDENTIALS);
					error = EINVAL;
				} else {
					if (inp->inp_ipo->ipo_dstid != NULL)
						ipsp_reffree(inp->inp_ipo->ipo_dstid);
					inp->inp_ipo->ipo_dstid = ipr;
				}
				break;
			case IP_IPSEC_LOCAL_CRED:
				if (ipr->ref_type < IPSP_CRED_KEYNOTE ||
				    ipr->ref_type > IPSP_CRED_X509) {
					FREE(ipr, M_CREDENTIALS);
					error = EINVAL;
				} else {
					if (inp->inp_ipo->ipo_local_cred != NULL)
						ipsp_reffree(inp->inp_ipo->ipo_local_cred);
					inp->inp_ipo->ipo_local_cred = ipr;
				}
				break;
			case IP_IPSEC_LOCAL_AUTH:
				if (ipr->ref_type < IPSP_AUTH_PASSPHRASE ||
				    ipr->ref_type > IPSP_AUTH_RSA) {
					FREE(ipr, M_CREDENTIALS);
					error = EINVAL;
				} else {
					if (inp->inp_ipo->ipo_local_auth != NULL)
						ipsp_reffree(inp->inp_ipo->ipo_local_auth);
					inp->inp_ipo->ipo_local_auth = ipr;
				}
				break;
			}

			/* Unlink cached output TDB to force a re-search */
			if (inp->inp_tdb_out) {
				int s = spltdb();
				TAILQ_REMOVE(&inp->inp_tdb_out->tdb_inp_out,
				    inp, inp_tdb_out_next);
				splx(s);
			}

			if (inp->inp_tdb_in) {
				int s = spltdb();
				TAILQ_REMOVE(&inp->inp_tdb_in->tdb_inp_in,
				    inp, inp_tdb_in_next);
				splx(s);
			}
#endif
			break;
		default:
			error = ENOPROTOOPT;
			break;
		}
		if (m)
			(void)m_free(m);
		break;

	case PRCO_GETOPT:
		switch (optname) {
		case IP_OPTIONS:
		case IP_RETOPTS:
			*mp = m = m_get(M_WAIT, MT_SOOPTS);
			if (inp->inp_options) {
				m->m_len = inp->inp_options->m_len;
				bcopy(mtod(inp->inp_options, caddr_t),
				    mtod(m, caddr_t), (unsigned)m->m_len);
			} else
				m->m_len = 0;
			break;

		case IP_TOS:
		case IP_TTL:
		case IP_MINTTL:
		case IP_RECVOPTS:
		case IP_RECVRETOPTS:
		case IP_RECVDSTADDR:
		case IP_RECVIF:
		case IP_RECVTTL:
			*mp = m = m_get(M_WAIT, MT_SOOPTS);
			m->m_len = sizeof(int);
			switch (optname) {

			case IP_TOS:
				optval = inp->inp_ip.ip_tos;
				break;

			case IP_TTL:
				optval = inp->inp_ip.ip_ttl;
				break;

			case IP_MINTTL:
				optval = inp->inp_ip_minttl;
				break;

#define	OPTBIT(bit)	(inp->inp_flags & bit ? 1 : 0)

			case IP_RECVOPTS:
				optval = OPTBIT(INP_RECVOPTS);
				break;

			case IP_RECVRETOPTS:
				optval = OPTBIT(INP_RECVRETOPTS);
				break;

			case IP_RECVDSTADDR:
				optval = OPTBIT(INP_RECVDSTADDR);
				break;
			case IP_RECVIF:
				optval = OPTBIT(INP_RECVIF);
				break;
			case IP_RECVTTL:
				optval = OPTBIT(INP_RECVTTL);
				break;
			}
			*mtod(m, int *) = optval;
			break;

		case IP_MULTICAST_IF:
		case IP_MULTICAST_TTL:
		case IP_MULTICAST_LOOP:
		case IP_ADD_MEMBERSHIP:
		case IP_DROP_MEMBERSHIP:
			error = ip_getmoptions(optname, inp->inp_moptions, mp);
			break;

		case IP_PORTRANGE:
			*mp = m = m_get(M_WAIT, MT_SOOPTS);
			m->m_len = sizeof(int);

			if (inp->inp_flags & INP_HIGHPORT)
				optval = IP_PORTRANGE_HIGH;
			else if (inp->inp_flags & INP_LOWPORT)
				optval = IP_PORTRANGE_LOW;
			else
				optval = 0;

			*mtod(m, int *) = optval;
			break;

		case IP_AUTH_LEVEL:
		case IP_ESP_TRANS_LEVEL:
		case IP_ESP_NETWORK_LEVEL:
		case IP_IPCOMP_LEVEL:
#ifndef IPSEC
			m->m_len = sizeof(int);
			*mtod(m, int *) = IPSEC_LEVEL_NONE;
#else
			m->m_len = sizeof(int);
			switch (optname) {
			case IP_AUTH_LEVEL:
				optval = inp->inp_seclevel[SL_AUTH];
				break;

			case IP_ESP_TRANS_LEVEL:
				optval = inp->inp_seclevel[SL_ESP_TRANS];
				break;

			case IP_ESP_NETWORK_LEVEL:
				optval = inp->inp_seclevel[SL_ESP_NETWORK];
				break;
			case IP_IPCOMP_LEVEL:
				optval = inp->inp_seclevel[SL_IPCOMP];
				break;
			}
			*mtod(m, int *) = optval;
#endif
			break;
		case IP_IPSEC_LOCAL_ID:
		case IP_IPSEC_REMOTE_ID:
		case IP_IPSEC_LOCAL_CRED:
		case IP_IPSEC_REMOTE_CRED:
		case IP_IPSEC_LOCAL_AUTH:
		case IP_IPSEC_REMOTE_AUTH:
#ifndef IPSEC
			error = EOPNOTSUPP;
#else
			*mp = m = m_get(M_WAIT, MT_SOOPTS);
			m->m_len = sizeof(u_int16_t);
			ipr = NULL;
			switch (optname) {
			case IP_IPSEC_LOCAL_ID:
				if (inp->inp_ipo != NULL)
					ipr = inp->inp_ipo->ipo_srcid;
				opt16val = IPSP_IDENTITY_NONE;
				break;
			case IP_IPSEC_REMOTE_ID:
				if (inp->inp_ipo != NULL)
					ipr = inp->inp_ipo->ipo_dstid;
				opt16val = IPSP_IDENTITY_NONE;
				break;
			case IP_IPSEC_LOCAL_CRED:
				if (inp->inp_ipo != NULL)
					ipr = inp->inp_ipo->ipo_local_cred;
				opt16val = IPSP_CRED_NONE;
				break;
			case IP_IPSEC_REMOTE_CRED:
				ipr = inp->inp_ipsec_remotecred;
				opt16val = IPSP_CRED_NONE;
				break;
			case IP_IPSEC_LOCAL_AUTH:
				if (inp->inp_ipo != NULL)
					ipr = inp->inp_ipo->ipo_local_auth;
				break;
			case IP_IPSEC_REMOTE_AUTH:
				ipr = inp->inp_ipsec_remoteauth;
				break;
			}
			if (ipr == NULL)
				*mtod(m, u_int16_t *) = opt16val;
			else {
				size_t len;

				len = m->m_len + ipr->ref_len;
				if (len > MCLBYTES) {
					 m_free(m);
					 error = EINVAL;
					 break;
				}
				/* allocate mbuf cluster for larger option */
				if (len > MLEN) {
					 MCLGET(m, M_WAITOK);
					 if ((m->m_flags & M_EXT) == 0) {
						 m_free(m);
						 error = ENOBUFS;
						 break;
					 }

				}
				m->m_len = len;
				*mtod(m, u_int16_t *) = ipr->ref_type;
				m_copyback(m, sizeof(u_int16_t), ipr->ref_len,
				    ipr + 1);
			}
#endif
			break;
		default:
			error = ENOPROTOOPT;
			break;
		}
		break;
	}
	return (error);
}

/*
 * Set up IP options in pcb for insertion in output packets.
 * Store in mbuf with pointer in pcbopt, adding pseudo-option
 * with destination address if source routed.
 */
int
#ifdef notyet
ip_pcbopts(optname, pcbopt, m)
	int optname;
#else
ip_pcbopts(pcbopt, m)
#endif
	struct mbuf **pcbopt;
	struct mbuf *m;
{
	int cnt, optlen;
	u_char *cp;
	u_char opt;

	/* turn off any old options */
	if (*pcbopt)
		(void)m_free(*pcbopt);
	*pcbopt = 0;
	if (m == (struct mbuf *)0 || m->m_len == 0) {
		/*
		 * Only turning off any previous options.
		 */
		if (m)
			(void)m_free(m);
		return (0);
	}

	if (m->m_len % sizeof(int32_t))
		goto bad;

	/*
	 * IP first-hop destination address will be stored before
	 * actual options; move other options back
	 * and clear it when none present.
	 */
	if (m->m_data + m->m_len + sizeof(struct in_addr) >= &m->m_dat[MLEN])
		goto bad;
	cnt = m->m_len;
	m->m_len += sizeof(struct in_addr);
	cp = mtod(m, u_char *) + sizeof(struct in_addr);
	ovbcopy(mtod(m, caddr_t), (caddr_t)cp, (unsigned)cnt);
	bzero(mtod(m, caddr_t), sizeof(struct in_addr));

	for (; cnt > 0; cnt -= optlen, cp += optlen) {
		opt = cp[IPOPT_OPTVAL];
		if (opt == IPOPT_EOL)
			break;
		if (opt == IPOPT_NOP)
			optlen = 1;
		else {
			if (cnt < IPOPT_OLEN + sizeof(*cp))
				goto bad;
			optlen = cp[IPOPT_OLEN];
			if (optlen < IPOPT_OLEN  + sizeof(*cp) || optlen > cnt)
				goto bad;
		}
		switch (opt) {

		default:
			break;

		case IPOPT_LSRR:
		case IPOPT_SSRR:
			/*
			 * user process specifies route as:
			 *	->A->B->C->D
			 * D must be our final destination (but we can't
			 * check that since we may not have connected yet).
			 * A is first hop destination, which doesn't appear in
			 * actual IP option, but is stored before the options.
			 */
			if (optlen < IPOPT_MINOFF - 1 + sizeof(struct in_addr))
				goto bad;
			m->m_len -= sizeof(struct in_addr);
			cnt -= sizeof(struct in_addr);
			optlen -= sizeof(struct in_addr);
			cp[IPOPT_OLEN] = optlen;
			/*
			 * Move first hop before start of options.
			 */
			bcopy((caddr_t)&cp[IPOPT_OFFSET+1], mtod(m, caddr_t),
			    sizeof(struct in_addr));
			/*
			 * Then copy rest of options back
			 * to close up the deleted entry.
			 */
			ovbcopy((caddr_t)(&cp[IPOPT_OFFSET+1] +
			    sizeof(struct in_addr)),
			    (caddr_t)&cp[IPOPT_OFFSET+1],
			    (unsigned)cnt - (IPOPT_OFFSET+1));
			break;
		}
	}
	if (m->m_len > MAX_IPOPTLEN + sizeof(struct in_addr))
		goto bad;
	*pcbopt = m;
	return (0);

bad:
	(void)m_free(m);
	return (EINVAL);
}

/*
 * Set the IP multicast options in response to user setsockopt().
 */
int
ip_setmoptions(optname, imop, m)
	int optname;
	struct ip_moptions **imop;
	struct mbuf *m;
{
	int error = 0;
	u_char loop;
	int i;
	struct in_addr addr;
	struct ip_mreq *mreq;
	struct ifnet *ifp;
	struct ip_moptions *imo = *imop;
	struct route ro;
	struct sockaddr_in *dst;

	if (imo == NULL) {
		/*
		 * No multicast option buffer attached to the pcb;
		 * allocate one and initialize to default values.
		 */
		imo = (struct ip_moptions *)malloc(sizeof(*imo), M_IPMOPTS,
		    M_WAITOK);

		*imop = imo;
		imo->imo_multicast_ifp = NULL;
		imo->imo_multicast_ttl = IP_DEFAULT_MULTICAST_TTL;
		imo->imo_multicast_loop = IP_DEFAULT_MULTICAST_LOOP;
		imo->imo_num_memberships = 0;
	}

	switch (optname) {

	case IP_MULTICAST_IF:
		/*
		 * Select the interface for outgoing multicast packets.
		 */
		if (m == NULL || m->m_len != sizeof(struct in_addr)) {
			error = EINVAL;
			break;
		}
		addr = *(mtod(m, struct in_addr *));
		/*
		 * INADDR_ANY is used to remove a previous selection.
		 * When no interface is selected, a default one is
		 * chosen every time a multicast packet is sent.
		 */
		if (addr.s_addr == INADDR_ANY) {
			imo->imo_multicast_ifp = NULL;
			break;
		}
		/*
		 * The selected interface is identified by its local
		 * IP address.  Find the interface and confirm that
		 * it supports multicasting.
		 */
		INADDR_TO_IFP(addr, ifp);
		if (ifp == NULL || (ifp->if_flags & IFF_MULTICAST) == 0) {
			error = EADDRNOTAVAIL;
			break;
		}
		imo->imo_multicast_ifp = ifp;
		break;

	case IP_MULTICAST_TTL:
		/*
		 * Set the IP time-to-live for outgoing multicast packets.
		 */
		if (m == NULL || m->m_len != 1) {
			error = EINVAL;
			break;
		}
		imo->imo_multicast_ttl = *(mtod(m, u_char *));
		break;

	case IP_MULTICAST_LOOP:
		/*
		 * Set the loopback flag for outgoing multicast packets.
		 * Must be zero or one.
		 */
		if (m == NULL || m->m_len != 1 ||
		   (loop = *(mtod(m, u_char *))) > 1) {
			error = EINVAL;
			break;
		}
		imo->imo_multicast_loop = loop;
		break;

	case IP_ADD_MEMBERSHIP:
		/*
		 * Add a multicast group membership.
		 * Group must be a valid IP multicast address.
		 */
		if (m == NULL || m->m_len != sizeof(struct ip_mreq)) {
			error = EINVAL;
			break;
		}
		mreq = mtod(m, struct ip_mreq *);
		if (!IN_MULTICAST(mreq->imr_multiaddr.s_addr)) {
			error = EINVAL;
			break;
		}
		/*
		 * If no interface address was provided, use the interface of
		 * the route to the given multicast address.
		 */
		if (mreq->imr_interface.s_addr == INADDR_ANY) {
			ro.ro_rt = NULL;
			dst = satosin(&ro.ro_dst);
			dst->sin_len = sizeof(*dst);
			dst->sin_family = AF_INET;
			dst->sin_addr = mreq->imr_multiaddr;
			rtalloc(&ro);
			if (ro.ro_rt == NULL) {
				error = EADDRNOTAVAIL;
				break;
			}
			ifp = ro.ro_rt->rt_ifp;
			rtfree(ro.ro_rt);
		} else {
			INADDR_TO_IFP(mreq->imr_interface, ifp);
		}
		/*
		 * See if we found an interface, and confirm that it
		 * supports multicast.
		 */
		if (ifp == NULL || (ifp->if_flags & IFF_MULTICAST) == 0) {
			error = EADDRNOTAVAIL;
			break;
		}
		/*
		 * See if the membership already exists or if all the
		 * membership slots are full.
		 */
		for (i = 0; i < imo->imo_num_memberships; ++i) {
			if (imo->imo_membership[i]->inm_ifp == ifp &&
			    imo->imo_membership[i]->inm_addr.s_addr
						== mreq->imr_multiaddr.s_addr)
				break;
		}
		if (i < imo->imo_num_memberships) {
			error = EADDRINUSE;
			break;
		}
		if (i == IP_MAX_MEMBERSHIPS) {
			error = ETOOMANYREFS;
			break;
		}
		/*
		 * Everything looks good; add a new record to the multicast
		 * address list for the given interface.
		 */
		if ((imo->imo_membership[i] =
		    in_addmulti(&mreq->imr_multiaddr, ifp)) == NULL) {
			error = ENOBUFS;
			break;
		}
		++imo->imo_num_memberships;
		break;

	case IP_DROP_MEMBERSHIP:
		/*
		 * Drop a multicast group membership.
		 * Group must be a valid IP multicast address.
		 */
		if (m == NULL || m->m_len != sizeof(struct ip_mreq)) {
			error = EINVAL;
			break;
		}
		mreq = mtod(m, struct ip_mreq *);
		if (!IN_MULTICAST(mreq->imr_multiaddr.s_addr)) {
			error = EINVAL;
			break;
		}
		/*
		 * If an interface address was specified, get a pointer
		 * to its ifnet structure.
		 */
		if (mreq->imr_interface.s_addr == INADDR_ANY)
			ifp = NULL;
		else {
			INADDR_TO_IFP(mreq->imr_interface, ifp);
			if (ifp == NULL) {
				error = EADDRNOTAVAIL;
				break;
			}
		}
		/*
		 * Find the membership in the membership array.
		 */
		for (i = 0; i < imo->imo_num_memberships; ++i) {
			if ((ifp == NULL ||
			     imo->imo_membership[i]->inm_ifp == ifp) &&
			     imo->imo_membership[i]->inm_addr.s_addr ==
			     mreq->imr_multiaddr.s_addr)
				break;
		}
		if (i == imo->imo_num_memberships) {
			error = EADDRNOTAVAIL;
			break;
		}
		/*
		 * Give up the multicast address record to which the
		 * membership points.
		 */
		in_delmulti(imo->imo_membership[i]);
		/*
		 * Remove the gap in the membership array.
		 */
		for (++i; i < imo->imo_num_memberships; ++i)
			imo->imo_membership[i-1] = imo->imo_membership[i];
		--imo->imo_num_memberships;
		break;

	default:
		error = EOPNOTSUPP;
		break;
	}

	/*
	 * If all options have default values, no need to keep the mbuf.
	 */
	if (imo->imo_multicast_ifp == NULL &&
	    imo->imo_multicast_ttl == IP_DEFAULT_MULTICAST_TTL &&
	    imo->imo_multicast_loop == IP_DEFAULT_MULTICAST_LOOP &&
	    imo->imo_num_memberships == 0) {
		free(*imop, M_IPMOPTS);
		*imop = NULL;
	}

	return (error);
}

/*
 * Return the IP multicast options in response to user getsockopt().
 */
int
ip_getmoptions(optname, imo, mp)
	int optname;
	struct ip_moptions *imo;
	struct mbuf **mp;
{
	u_char *ttl;
	u_char *loop;
	struct in_addr *addr;
	struct in_ifaddr *ia;

	*mp = m_get(M_WAIT, MT_SOOPTS);

	switch (optname) {

	case IP_MULTICAST_IF:
		addr = mtod(*mp, struct in_addr *);
		(*mp)->m_len = sizeof(struct in_addr);
		if (imo == NULL || imo->imo_multicast_ifp == NULL)
			addr->s_addr = INADDR_ANY;
		else {
			IFP_TO_IA(imo->imo_multicast_ifp, ia);
			addr->s_addr = (ia == NULL) ? INADDR_ANY
					: ia->ia_addr.sin_addr.s_addr;
		}
		return (0);

	case IP_MULTICAST_TTL:
		ttl = mtod(*mp, u_char *);
		(*mp)->m_len = 1;
		*ttl = (imo == NULL) ? IP_DEFAULT_MULTICAST_TTL
				     : imo->imo_multicast_ttl;
		return (0);

	case IP_MULTICAST_LOOP:
		loop = mtod(*mp, u_char *);
		(*mp)->m_len = 1;
		*loop = (imo == NULL) ? IP_DEFAULT_MULTICAST_LOOP
				      : imo->imo_multicast_loop;
		return (0);

	default:
		return (EOPNOTSUPP);
	}
}

/*
 * Discard the IP multicast options.
 */
void
ip_freemoptions(imo)
	struct ip_moptions *imo;
{
	int i;

	if (imo != NULL) {
		for (i = 0; i < imo->imo_num_memberships; ++i)
			in_delmulti(imo->imo_membership[i]);
		free(imo, M_IPMOPTS);
	}
}

/*
 * Routine called from ip_output() to loop back a copy of an IP multicast
 * packet to the input queue of a specified interface.  Note that this
 * calls the output routine of the loopback "driver", but with an interface
 * pointer that might NOT be &loif -- easier than replicating that code here.
 */
void
ip_mloopback(ifp, m, dst)
	struct ifnet *ifp;
	struct mbuf *m;
	struct sockaddr_in *dst;
{
	struct ip *ip;
	struct mbuf *copym;

	copym = m_copym2(m, 0, M_COPYALL, M_DONTWAIT);
	if (copym != NULL) {
		/*
		 * We don't bother to fragment if the IP length is greater
		 * than the interface's MTU.  Can this possibly matter?
		 */
		ip = mtod(copym, struct ip *);
		ip->ip_sum = 0;
		ip->ip_sum = in_cksum(copym, ip->ip_hl << 2);
		(void) looutput(ifp, copym, sintosa(dst), NULL);
	}
}

/*
 * Process a delayed payload checksum calculation.
 */
void
in_delayed_cksum(struct mbuf *m)
{
	struct ip *ip;
	u_int16_t csum, offset;

	ip = mtod(m, struct ip *);
	offset = ip->ip_hl << 2;
	csum = in4_cksum(m, 0, offset, m->m_pkthdr.len - offset);
	if (csum == 0 && ip->ip_p == IPPROTO_UDP)
		csum = 0xffff;

	switch (ip->ip_p) {
	case IPPROTO_TCP:
		offset += offsetof(struct tcphdr, th_sum);
		break;

	case IPPROTO_UDP:
		offset += offsetof(struct udphdr, uh_sum);
		break;

	default:
		return;
	}

	if ((offset + sizeof(u_int16_t)) > m->m_len)
		m_copyback(m, offset, sizeof(csum), &csum);
	else
		*(u_int16_t *)(mtod(m, caddr_t) + offset) = csum;
}
