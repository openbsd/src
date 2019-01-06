/*	$OpenBSD: ip_output.c,v 1.351 2019/01/06 11:05:09 claudio Exp $	*/
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
#include <net/if_var.h>
#include <net/if_enc.h>
#include <net/route.h>

#include <netinet/in.h>
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
#endif /* IPSEC */

int ip_pcbopts(struct mbuf **, struct mbuf *);
int ip_setmoptions(int, struct ip_moptions **, struct mbuf *, u_int);
void ip_mloopback(struct ifnet *, struct mbuf *, struct sockaddr_in *);
static __inline u_int16_t __attribute__((__unused__))
    in_cksum_phdr(u_int32_t, u_int32_t, u_int32_t);
void in_delayed_cksum(struct mbuf *);

#ifdef IPSEC
struct tdb *
ip_output_ipsec_lookup(struct mbuf *m, int hlen, int *error, struct inpcb *inp,
    int ipsecflowinfo);
int
ip_output_ipsec_send(struct tdb *, struct mbuf *, struct route *, int);
#endif /* IPSEC */

/*
 * IP output.  The packet in mbuf chain m contains a skeletal IP
 * header (with len, off, ttl, proto, tos, src, dst).
 * The mbuf chain containing the packet will be freed.
 * The mbuf opt, if present, will not be freed.
 */
int
ip_output(struct mbuf *m0, struct mbuf *opt, struct route *ro, int flags,
    struct ip_moptions *imo, struct inpcb *inp, u_int32_t ipsecflowinfo)
{
	struct ip *ip;
	struct ifnet *ifp = NULL;
	struct mbuf *m = m0;
	int hlen = sizeof (struct ip);
	int len, error = 0;
	struct route iproute;
	struct sockaddr_in *dst;
	struct tdb *tdb = NULL;
	u_long mtu;
#if defined(MROUTING)
	int rv;
#endif

	NET_ASSERT_LOCKED();

#ifdef IPSEC
	if (inp && (inp->inp_flags & INP_IPV6) != 0)
		panic("ip_output: IPv6 pcb is passed");
#endif /* IPSEC */

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
		ipstat_inc(ips_localout);
	} else {
		hlen = ip->ip_hl << 2;
	}

	/*
	 * We should not send traffic to 0/8 say both Stevens and RFCs
	 * 5735 section 3 and 1122 sections 3.2.1.3 and 3.3.6.
	 */
	if ((ntohl(ip->ip_dst.s_addr) >> IN_CLASSA_NSHIFT) == 0) {
		error = ENETUNREACH;
		goto bad;
	}

#if NPF > 0
reroute:
#endif

	/*
	 * Do a route lookup now in case we need the source address to
	 * do an SPD lookup in IPsec; for most packets, the source address
	 * is set at a higher level protocol. ICMPs and other packets
	 * though (e.g., traceroute) have a source address of zeroes.
	 */
	if (ro == NULL) {
		ro = &iproute;
		memset(ro, 0, sizeof(*ro));
	}

	dst = satosin(&ro->ro_dst);

	/*
	 * If there is a cached route, check that it is to the same
	 * destination and is still up.  If not, free it and try again.
	 */
	if (!rtisvalid(ro->ro_rt) ||
	    dst->sin_addr.s_addr != ip->ip_dst.s_addr ||
	    ro->ro_tableid != m->m_pkthdr.ph_rtableid) {
		rtfree(ro->ro_rt);
		ro->ro_rt = NULL;
	}

	if (ro->ro_rt == NULL) {
		dst->sin_family = AF_INET;
		dst->sin_len = sizeof(*dst);
		dst->sin_addr = ip->ip_dst;
		ro->ro_tableid = m->m_pkthdr.ph_rtableid;
	}

	if ((IN_MULTICAST(ip->ip_dst.s_addr) ||
	    (ip->ip_dst.s_addr == INADDR_BROADCAST)) &&
	    imo != NULL && (ifp = if_get(imo->imo_ifidx)) != NULL) {

		mtu = ifp->if_mtu;
		if (ip->ip_src.s_addr == INADDR_ANY) {
			struct in_ifaddr *ia;

			IFP_TO_IA(ifp, ia);
			if (ia != NULL)
				ip->ip_src = ia->ia_addr.sin_addr;
		}
	} else {
		struct in_ifaddr *ia;

		if (ro->ro_rt == NULL)
			ro->ro_rt = rtalloc_mpath(&ro->ro_dst,
			    &ip->ip_src.s_addr, ro->ro_tableid);

		if (ro->ro_rt == NULL) {
			ipstat_inc(ips_noroute);
			error = EHOSTUNREACH;
			goto bad;
		}

		ia = ifatoia(ro->ro_rt->rt_ifa);
		if (ISSET(ro->ro_rt->rt_flags, RTF_LOCAL))
			ifp = if_get(rtable_loindex(m->m_pkthdr.ph_rtableid));
		else
			ifp = if_get(ro->ro_rt->rt_ifidx);
		/*
		 * We aren't using rtisvalid() here because the UP/DOWN state
		 * machine is broken with some Ethernet drivers like em(4).
		 * As a result we might try to use an invalid cached route
		 * entry while an interface is being detached.
		 */
		if (ifp == NULL) {
			ipstat_inc(ips_noroute);
			error = EHOSTUNREACH;
			goto bad;
		}
		if ((mtu = ro->ro_rt->rt_mtu) == 0)
			mtu = ifp->if_mtu;

		if (ro->ro_rt->rt_flags & RTF_GATEWAY)
			dst = satosin(ro->ro_rt->rt_gateway);

		/* Set the source IP address */
		if (ip->ip_src.s_addr == INADDR_ANY && ia)
			ip->ip_src = ia->ia_addr.sin_addr;
	}

#ifdef IPSEC
	if (ipsec_in_use || inp != NULL) {
		/* Do we have any pending SAs to apply ? */
		tdb = ip_output_ipsec_lookup(m, hlen, &error, inp,
		    ipsecflowinfo);
		if (error != 0) {
			/* Should silently drop packet */
			if (error == -EINVAL)
				error = 0;
			m_freem(m);
			goto done;
		}
		if (tdb != NULL) {
			/*
			 * If it needs TCP/UDP hardware-checksumming, do the
			 * computation now.
			 */
			in_proto_cksum_out(m, NULL);
		}
	}
#endif /* IPSEC */

	if (IN_MULTICAST(ip->ip_dst.s_addr) ||
	    (ip->ip_dst.s_addr == INADDR_BROADCAST)) {

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
			ip->ip_ttl = imo->imo_ttl;
		else
			ip->ip_ttl = IP_DEFAULT_MULTICAST_TTL;

		/*
		 * if we don't know the outgoing ifp yet, we can't generate
		 * output
		 */
		if (!ifp) {
			ipstat_inc(ips_noroute);
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
		      (ifp->if_flags & IFF_BROADCAST) == 0)) && (tdb == NULL)) {
			ipstat_inc(ips_noroute);
			error = ENETUNREACH;
			goto bad;
		}

		/*
		 * If source address not specified yet, use address
		 * of outgoing interface.
		 */
		if (ip->ip_src.s_addr == INADDR_ANY) {
			struct in_ifaddr *ia;

			IFP_TO_IA(ifp, ia);
			if (ia != NULL)
				ip->ip_src = ia->ia_addr.sin_addr;
		}

		if ((imo == NULL || imo->imo_loop) &&
		    in_hasmulti(&ip->ip_dst, ifp)) {
			/*
			 * If we belong to the destination multicast group
			 * on the outgoing interface, and the caller did not
			 * forbid loopback, loop back a copy.
			 * Can't defer TCP/UDP checksumming, do the
			 * computation now.
			 */
			in_proto_cksum_out(m, NULL);
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
			if (ipmforwarding && ip_mrouter[ifp->if_rdomain] &&
			    (flags & IP_FORWARDING) == 0) {
				KERNEL_LOCK();
				rv = ip_mforward(m, ifp);
				KERNEL_UNLOCK();
				if (rv != 0) {
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
	 * Look for broadcast address and verify user is allowed to send
	 * such a packet; if the packet is going in an IPsec tunnel, skip
	 * this check.
	 */
	if ((tdb == NULL) && ((dst->sin_addr.s_addr == INADDR_BROADCAST) ||
	    (ro && ro->ro_rt && ISSET(ro->ro_rt->rt_flags, RTF_BROADCAST)))) {
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
	    (ro->ro_rt->rt_locks & RTV_MTU) == 0)
		ip->ip_off |= htons(IP_DF);

#ifdef IPSEC
	/*
	 * Check if the packet needs encapsulation.
	 */
	if (tdb != NULL) {
		/* Callee frees mbuf */
		error = ip_output_ipsec_send(tdb, m, ro,
		    (flags & IP_FORWARDING) ? 1 : 0);
		goto done;
	}
#endif /* IPSEC */

	/*
	 * Packet filter
	 */
#if NPF > 0
	if (pf_test(AF_INET, (flags & IP_FORWARDING) ? PF_FWD : PF_OUT,
	    ifp, &m) != PF_PASS) {
		error = EACCES;
		m_freem(m);
		goto done;
	}
	if (m == NULL)
		goto done;
	ip = mtod(m, struct ip *);
	hlen = ip->ip_hl << 2;
	if ((m->m_pkthdr.pf.flags & (PF_TAG_REROUTE | PF_TAG_GENERATED)) ==
	    (PF_TAG_REROUTE | PF_TAG_GENERATED))
		/* already rerun the route lookup, go on */
		m->m_pkthdr.pf.flags &= ~(PF_TAG_GENERATED | PF_TAG_REROUTE);
	else if (m->m_pkthdr.pf.flags & PF_TAG_REROUTE) {
		/* tag as generated to skip over pf_test on rerun */
		m->m_pkthdr.pf.flags |= PF_TAG_GENERATED;
		ro = NULL;
		if_put(ifp); /* drop reference since target changed */
		ifp = NULL;
		goto reroute;
	}
#endif
	in_proto_cksum_out(m, ifp);

#ifdef IPSEC
	if (ipsec_in_use && (flags & IP_FORWARDING) && (ipforwarding == 2) &&
	    (m_tag_find(m, PACKET_TAG_IPSEC_IN_DONE, NULL) == NULL)) {
		error = EHOSTUNREACH;
		m_freem(m);
		goto done;
	}
#endif

	/*
	 * If small enough for interface, can just send directly.
	 */
	if (ntohs(ip->ip_len) <= mtu) {
		ip->ip_sum = 0;
		if ((ifp->if_capabilities & IFCAP_CSUM_IPv4) &&
		    (ifp->if_bridgeport == NULL))
			m->m_pkthdr.csum_flags |= M_IPV4_CSUM_OUT;
		else {
			ipstat_inc(ips_outswcsum);
			ip->ip_sum = in_cksum(m, hlen);
		}

		error = ifp->if_output(ifp, m, sintosa(dst), ro->ro_rt);
		goto done;
	}

	/*
	 * Too large for interface; fragment if possible.
	 * Must be able to put at least 8 bytes per fragment.
	 */
	if (ip->ip_off & htons(IP_DF)) {
#ifdef IPSEC
		if (ip_mtudisc)
			ipsec_adjust_mtu(m, ifp->if_mtu);
#endif
		error = EMSGSIZE;
		/*
		 * This case can happen if the user changed the MTU
		 * of an interface after enabling IP on it.  Because
		 * most netifs don't keep track of routes pointing to
		 * them, there is no way for one to update all its
		 * routes when the MTU is changed.
		 */
		if (rtisvalid(ro->ro_rt) &&
		    ISSET(ro->ro_rt->rt_flags, RTF_HOST) &&
		    !(ro->ro_rt->rt_locks & RTV_MTU) &&
		    (ro->ro_rt->rt_mtu > ifp->if_mtu)) {
			ro->ro_rt->rt_mtu = ifp->if_mtu;
		}
		ipstat_inc(ips_cantfrag);
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
			error = ifp->if_output(ifp, m, sintosa(dst), ro->ro_rt);
		else
			m_freem(m);
	}

	if (error == 0)
		ipstat_inc(ips_fragmented);

done:
	if (ro == &iproute && ro->ro_rt)
		rtfree(ro->ro_rt);
	if_put(ifp);
	return (error);
bad:
	m_freem(m0);
	goto done;
}

#ifdef IPSEC
struct tdb *
ip_output_ipsec_lookup(struct mbuf *m, int hlen, int *error, struct inpcb *inp,
    int ipsecflowinfo)
{
	struct m_tag *mtag;
	struct tdb_ident *tdbi;
	struct tdb *tdb;

	/* Do we have any pending SAs to apply ? */
	tdb = ipsp_spd_lookup(m, AF_INET, hlen, error, IPSP_DIRECTION_OUT,
	    NULL, inp, ipsecflowinfo);
	if (tdb == NULL)
		return NULL;
	/* Loop detection */
	for (mtag = m_tag_first(m); mtag != NULL; mtag = m_tag_next(m, mtag)) {
		if (mtag->m_tag_id != PACKET_TAG_IPSEC_OUT_DONE)
			continue;
		tdbi = (struct tdb_ident *)(mtag + 1);
		if (tdbi->spi == tdb->tdb_spi &&
		    tdbi->proto == tdb->tdb_sproto &&
		    tdbi->rdomain == tdb->tdb_rdomain &&
		    !memcmp(&tdbi->dst, &tdb->tdb_dst,
		    sizeof(union sockaddr_union))) {
			/* no IPsec needed */
			return NULL;
		}
	}
	return tdb;
}

int
ip_output_ipsec_send(struct tdb *tdb, struct mbuf *m, struct route *ro, int fwd)
{
#if NPF > 0
	struct ifnet *encif;
#endif
	struct ip *ip;
	int error;

#if NPF > 0
	/*
	 * Packet filter
	 */
	if ((encif = enc_getif(tdb->tdb_rdomain, tdb->tdb_tap)) == NULL ||
	    pf_test(AF_INET, fwd ? PF_FWD : PF_OUT, encif, &m) != PF_PASS) {
		m_freem(m);
		return EACCES;
	}
	if (m == NULL)
		return 0;
	/*
	 * PF_TAG_REROUTE handling or not...
	 * Packet is entering IPsec so the routing is
	 * already overruled by the IPsec policy.
	 * Until now the change was not reconsidered.
	 * What's the behaviour?
	 */
	in_proto_cksum_out(m, encif);
#endif

	/* Check if we are allowed to fragment */
	ip = mtod(m, struct ip *);
	if (ip_mtudisc && (ip->ip_off & htons(IP_DF)) && tdb->tdb_mtu &&
	    ntohs(ip->ip_len) > tdb->tdb_mtu &&
	    tdb->tdb_mtutimeout > time_second) {
		struct rtentry *rt = NULL;
		int rt_mtucloned = 0;
		int transportmode = 0;

		transportmode = (tdb->tdb_dst.sa.sa_family == AF_INET) &&
		    (tdb->tdb_dst.sin.sin_addr.s_addr == ip->ip_dst.s_addr);

		/* Find a host route to store the mtu in */
		if (ro != NULL)
			rt = ro->ro_rt;
		/* but don't add a PMTU route for transport mode SAs */
		if (transportmode)
			rt = NULL;
		else if (rt == NULL || (rt->rt_flags & RTF_HOST) == 0) {
			rt = icmp_mtudisc_clone(ip->ip_dst,
			    m->m_pkthdr.ph_rtableid);
			rt_mtucloned = 1;
		}
		DPRINTF(("%s: spi %08x mtu %d rt %p cloned %d\n", __func__,
		    ntohl(tdb->tdb_spi), tdb->tdb_mtu, rt, rt_mtucloned));
		if (rt != NULL) {
			rt->rt_mtu = tdb->tdb_mtu;
			if (ro && ro->ro_rt != NULL) {
				rtfree(ro->ro_rt);
				ro->ro_rt = rtalloc(&ro->ro_dst, RT_RESOLVE,
				    m->m_pkthdr.ph_rtableid);
			}
			if (rt_mtucloned)
				rtfree(rt);
		}
		ipsec_adjust_mtu(m, tdb->tdb_mtu);
		m_freem(m);
		return EMSGSIZE;
	}

	/*
	 * Clear these -- they'll be set in the recursive invocation
	 * as needed.
	 */
	m->m_flags &= ~(M_MCAST | M_BCAST);

	/* Callee frees mbuf */
	error = ipsp_process_packet(m, tdb, AF_INET, 0);
	if (error) {
		ipsecstat_inc(ipsec_odrops);
		tdb->tdb_odrops++;
	}
	return error;
}
#endif /* IPSEC */

int
ip_fragment(struct mbuf *m, struct ifnet *ifp, u_long mtu)
{
	struct ip *ip, *mhip;
	struct mbuf *m0;
	int len, hlen, off;
	int mhlen, firstlen;
	struct mbuf **mnext;
	int fragments = 0;
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
	in_proto_cksum_out(m, NULL);
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
		if (m == NULL) {
			ipstat_inc(ips_odropped);
			error = ENOBUFS;
			goto sendorfree;
		}
		*mnext = m;
		mnext = &m->m_nextpkt;
		m->m_data += max_linkhdr;
		mhip = mtod(m, struct ip *);
		*mhip = *ip;
		/* we must inherit MCAST/BCAST flags, routing table and prio */
		m->m_flags |= m0->m_flags & (M_MCAST|M_BCAST);
		m->m_pkthdr.ph_rtableid = m0->m_pkthdr.ph_rtableid;
		m->m_pkthdr.pf.prio = m0->m_pkthdr.pf.prio;
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
		m->m_next = m_copym(m0, off, len, M_NOWAIT);
		if (m->m_next == 0) {
			ipstat_inc(ips_odropped);
			error = ENOBUFS;
			goto sendorfree;
		}
		m->m_pkthdr.len = mhlen + len;
		m->m_pkthdr.ph_ifidx = 0;
		mhip->ip_off = htons((u_int16_t)mhip->ip_off);
		mhip->ip_sum = 0;
		if ((ifp != NULL) &&
		    (ifp->if_capabilities & IFCAP_CSUM_IPv4) &&
		    (ifp->if_bridgeport == NULL))
			m->m_pkthdr.csum_flags |= M_IPV4_CSUM_OUT;
		else {
			ipstat_inc(ips_outswcsum);
			mhip->ip_sum = in_cksum(m, mhlen);
		}
		ipstat_inc(ips_ofragments);
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
	ip->ip_sum = 0;
	if ((ifp != NULL) &&
	    (ifp->if_capabilities & IFCAP_CSUM_IPv4) &&
	    (ifp->if_bridgeport == NULL))
		m->m_pkthdr.csum_flags |= M_IPV4_CSUM_OUT;
	else {
		ipstat_inc(ips_outswcsum);
		ip->ip_sum = in_cksum(m, hlen);
	}
sendorfree:
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
ip_insertoptions(struct mbuf *m, struct mbuf *opt, int *phlen)
{
	struct ipoption *p = mtod(opt, struct ipoption *);
	struct mbuf *n;
	struct ip *ip = mtod(m, struct ip *);
	unsigned int optlen;

	optlen = opt->m_len - sizeof(p->ipopt_dst);
	if (optlen + ntohs(ip->ip_len) > IP_MAXPACKET)
		return (m);		/* XXX should fail */
	if (p->ipopt_dst.s_addr)
		ip->ip_dst = p->ipopt_dst;
	if (m->m_flags & M_EXT || m->m_data - optlen < m->m_pktdat) {
		MGETHDR(n, M_DONTWAIT, MT_HEADER);
		if (n == NULL)
			return (m);
		M_MOVE_HDR(n, m);
		n->m_pkthdr.len += optlen;
		m->m_len -= sizeof(struct ip);
		m->m_data += sizeof(struct ip);
		n->m_next = m;
		m = n;
		m->m_len = optlen + sizeof(struct ip);
		m->m_data += max_linkhdr;
		memcpy(mtod(m, caddr_t), ip, sizeof(struct ip));
	} else {
		m->m_data -= optlen;
		m->m_len += optlen;
		m->m_pkthdr.len += optlen;
		memmove(mtod(m, caddr_t), (caddr_t)ip, sizeof(struct ip));
	}
	ip = mtod(m, struct ip *);
	memcpy(ip + 1, p->ipopt_list, optlen);
	*phlen = sizeof(struct ip) + optlen;
	ip->ip_len = htons(ntohs(ip->ip_len) + optlen);
	return (m);
}

/*
 * Copy options from ip to jp,
 * omitting those not copied during fragmentation.
 */
int
ip_optcopy(struct ip *ip, struct ip *jp)
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
			memcpy(dp, cp, optlen);
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
ip_ctloutput(int op, struct socket *so, int level, int optname,
    struct mbuf *m)
{
	struct inpcb *inp = sotoinpcb(so);
	int optval = 0;
	struct proc *p = curproc; /* XXX */
	int error = 0;
	u_int rtid = 0;

	if (level != IPPROTO_IP)
		return (EINVAL);

	switch (op) {
	case PRCO_SETOPT:
		switch (optname) {
		case IP_OPTIONS:
			return (ip_pcbopts(&inp->inp_options, m));

		case IP_TOS:
		case IP_TTL:
		case IP_MINTTL:
		case IP_RECVOPTS:
		case IP_RECVRETOPTS:
		case IP_RECVDSTADDR:
		case IP_RECVIF:
		case IP_RECVTTL:
		case IP_RECVDSTPORT:
		case IP_RECVRTABLE:
		case IP_IPSECFLOWINFO:
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
					else if (optval == -1)
						inp->inp_ip.ip_ttl = ip_defttl;
					else
						error = EINVAL;
					break;

				case IP_MINTTL:
					if (optval >= 0 && optval <= MAXTTL)
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
				case IP_RECVDSTPORT:
					OPTSET(INP_RECVDSTPORT);
					break;
				case IP_RECVRTABLE:
					OPTSET(INP_RECVRTABLE);
					break;
				case IP_IPSECFLOWINFO:
					OPTSET(INP_IPSECFLOWINFO);
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
			error = ip_setmoptions(optname, &inp->inp_moptions, m,
			    inp->inp_rtableid);
			break;

		case IP_PORTRANGE:
			if (m == NULL || m->m_len != sizeof(int))
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
			if (m == NULL || m->m_len != sizeof(int)) {
				error = EINVAL;
				break;
			}
			optval = *mtod(m, int *);

			if (optval < IPSEC_LEVEL_BYPASS ||
			    optval > IPSEC_LEVEL_UNIQUE) {
				error = EINVAL;
				break;
			}

			switch (optname) {
			case IP_AUTH_LEVEL:
				if (optval < IPSEC_AUTH_LEVEL_DEFAULT &&
				    suser(p)) {
					error = EACCES;
					break;
				}
				inp->inp_seclevel[SL_AUTH] = optval;
				break;

			case IP_ESP_TRANS_LEVEL:
				if (optval < IPSEC_ESP_TRANS_LEVEL_DEFAULT &&
				    suser(p)) {
					error = EACCES;
					break;
				}
				inp->inp_seclevel[SL_ESP_TRANS] = optval;
				break;

			case IP_ESP_NETWORK_LEVEL:
				if (optval < IPSEC_ESP_NETWORK_LEVEL_DEFAULT &&
				    suser(p)) {
					error = EACCES;
					break;
				}
				inp->inp_seclevel[SL_ESP_NETWORK] = optval;
				break;
			case IP_IPCOMP_LEVEL:
				if (optval < IPSEC_IPCOMP_LEVEL_DEFAULT &&
				    suser(p)) {
					error = EACCES;
					break;
				}
				inp->inp_seclevel[SL_IPCOMP] = optval;
				break;
			}
#endif
			break;

		case IP_IPSEC_LOCAL_ID:
		case IP_IPSEC_REMOTE_ID:
			error = EOPNOTSUPP;
			break;
		case SO_RTABLE:
			if (m == NULL || m->m_len < sizeof(u_int)) {
				error = EINVAL;
				break;
			}
			rtid = *mtod(m, u_int *);
			if (inp->inp_rtableid == rtid)
				break;
			/* needs privileges to switch when already set */
			if (p->p_p->ps_rtableid != rtid &&
			    p->p_p->ps_rtableid != 0 &&
			    (error = suser(p)) != 0)
				break;
			/* table must exist */
			if (!rtable_exists(rtid)) {
				error = EINVAL;
				break;
			}
			if (inp->inp_lport) {
				error = EBUSY;
				break;
			}
			inp->inp_rtableid = rtid;
			in_pcbrehash(inp);
			break;
		case IP_PIPEX:
			if (m != NULL && m->m_len == sizeof(int))
				inp->inp_pipex = *mtod(m, int *);
			else
				error = EINVAL;
			break;

		default:
			error = ENOPROTOOPT;
			break;
		}
		break;

	case PRCO_GETOPT:
		switch (optname) {
		case IP_OPTIONS:
		case IP_RETOPTS:
			if (inp->inp_options) {
				m->m_len = inp->inp_options->m_len;
				memcpy(mtod(m, caddr_t),
				    mtod(inp->inp_options, caddr_t), m->m_len);
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
		case IP_RECVDSTPORT:
		case IP_RECVRTABLE:
		case IP_IPSECFLOWINFO:
		case IP_IPDEFTTL:
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

			case IP_IPDEFTTL:
				optval = ip_defttl;
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
			case IP_RECVDSTPORT:
				optval = OPTBIT(INP_RECVDSTPORT);
				break;
			case IP_RECVRTABLE:
				optval = OPTBIT(INP_RECVRTABLE);
				break;
			case IP_IPSECFLOWINFO:
				optval = OPTBIT(INP_IPSECFLOWINFO);
				break;
			}
			*mtod(m, int *) = optval;
			break;

		case IP_MULTICAST_IF:
		case IP_MULTICAST_TTL:
		case IP_MULTICAST_LOOP:
		case IP_ADD_MEMBERSHIP:
		case IP_DROP_MEMBERSHIP:
			error = ip_getmoptions(optname, inp->inp_moptions, m);
			break;

		case IP_PORTRANGE:
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
			error = EOPNOTSUPP;
			break;
		case SO_RTABLE:
			m->m_len = sizeof(u_int);
			*mtod(m, u_int *) = inp->inp_rtableid;
			break;
		case IP_PIPEX:
			m->m_len = sizeof(int);
			*mtod(m, int *) = inp->inp_pipex;
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
ip_pcbopts(struct mbuf **pcbopt, struct mbuf *m)
{
	struct mbuf *n;
	struct ipoption *p;
	int cnt, off, optlen;
	u_char *cp;
	u_char opt;

	/* turn off any old options */
	m_free(*pcbopt);
	*pcbopt = NULL;
	if (m == NULL || m->m_len == 0) {
		/*
		 * Only turning off any previous options.
		 */
		return (0);
	}

	if (m->m_len % sizeof(int32_t) ||
	    m->m_len > MAX_IPOPTLEN + sizeof(struct in_addr))
		return (EINVAL);

	/* Don't sleep because NET_LOCK() is hold. */
	if ((n = m_get(M_NOWAIT, MT_SOOPTS)) == NULL)
		return (ENOBUFS);
	p = mtod(n, struct ipoption *);
	p->ipopt_dst.s_addr = 0;
	n->m_len = sizeof(struct in_addr);

	off = 0;
	cnt = m->m_len;
	cp = mtod(m, u_char *) + sizeof(struct in_addr);

	while (cnt > 0) {
		opt = cp[IPOPT_OPTVAL];

		if (opt == IPOPT_NOP || opt == IPOPT_EOL)
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
			memcpy(p->ipopt_list + off, cp, optlen);
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

			/*
			 * Optlen is smaller because first address is popped.
			 * Cnt and cp will be adjusted a bit later to reflect
			 * this.
			 */
			optlen -= sizeof(struct in_addr);
			p->ipopt_list[off + IPOPT_OPTVAL] = opt;
			p->ipopt_list[off + IPOPT_OLEN] = optlen;

			/*
			 * Move first hop before start of options.
			 */
			memcpy(&p->ipopt_dst, cp + IPOPT_OFFSET,
			    sizeof(struct in_addr));
			cp += sizeof(struct in_addr);
			cnt -= sizeof(struct in_addr);
			/*
			 * Then copy rest of options
			 */
			memcpy(p->ipopt_list + off + IPOPT_OFFSET,
			    cp + IPOPT_OFFSET, optlen - IPOPT_OFFSET);
			break;
		}
		off += optlen;
		cp += optlen;
		cnt -= optlen;

		if (opt == IPOPT_EOL)
			break;
	}
	n->m_len += off;
	if (n->m_len > sizeof(*p)) {
 bad:
		m_freem(n);
		return (EINVAL);
	}

	*pcbopt = n;
	return (0);
}

/*
 * Set the IP multicast options in response to user setsockopt().
 */
int
ip_setmoptions(int optname, struct ip_moptions **imop, struct mbuf *m,
    u_int rtableid)
{
	struct in_addr addr;
	struct in_ifaddr *ia;
	struct ip_mreq *mreq;
	struct ifnet *ifp = NULL;
	struct ip_moptions *imo = *imop;
	struct in_multi **immp;
	struct rtentry *rt;
	struct sockaddr_in sin;
	int i, error = 0;
	u_char loop;

	if (imo == NULL) {
		/*
		 * No multicast option buffer attached to the pcb;
		 * allocate one and initialize to default values.
		 */
		imo = malloc(sizeof(*imo), M_IPMOPTS, M_WAITOK|M_ZERO);
		immp = (struct in_multi **)malloc(
		    (sizeof(*immp) * IP_MIN_MEMBERSHIPS), M_IPMOPTS,
		    M_WAITOK|M_ZERO);
		*imop = imo;
		imo->imo_ifidx = 0;
		imo->imo_ttl = IP_DEFAULT_MULTICAST_TTL;
		imo->imo_loop = IP_DEFAULT_MULTICAST_LOOP;
		imo->imo_num_memberships = 0;
		imo->imo_max_memberships = IP_MIN_MEMBERSHIPS;
		imo->imo_membership = immp;
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
			imo->imo_ifidx = 0;
			break;
		}
		/*
		 * The selected interface is identified by its local
		 * IP address.  Find the interface and confirm that
		 * it supports multicasting.
		 */
		memset(&sin, 0, sizeof(sin));
		sin.sin_len = sizeof(sin);
		sin.sin_family = AF_INET;
		sin.sin_addr = addr;
		ia = ifatoia(ifa_ifwithaddr(sintosa(&sin), rtableid));
		if (ia == NULL ||
		    (ia->ia_ifp->if_flags & IFF_MULTICAST) == 0) {
			error = EADDRNOTAVAIL;
			break;
		}
		imo->imo_ifidx = ia->ia_ifp->if_index;
		break;

	case IP_MULTICAST_TTL:
		/*
		 * Set the IP time-to-live for outgoing multicast packets.
		 */
		if (m == NULL || m->m_len != 1) {
			error = EINVAL;
			break;
		}
		imo->imo_ttl = *(mtod(m, u_char *));
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
		imo->imo_loop = loop;
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
			memset(&sin, 0, sizeof(sin));
			sin.sin_len = sizeof(sin);
			sin.sin_family = AF_INET;
			sin.sin_addr = mreq->imr_multiaddr;
			rt = rtalloc(sintosa(&sin), RT_RESOLVE, rtableid);
			if (!rtisvalid(rt)) {
				rtfree(rt);
				error = EADDRNOTAVAIL;
				break;
			}
		} else {
			memset(&sin, 0, sizeof(sin));
			sin.sin_len = sizeof(sin);
			sin.sin_family = AF_INET;
			sin.sin_addr = mreq->imr_interface;
			rt = rtalloc(sintosa(&sin), 0, rtableid);
			if (!rtisvalid(rt) || !ISSET(rt->rt_flags, RTF_LOCAL)) {
				rtfree(rt);
				error = EADDRNOTAVAIL;
				break;
			}
		}
		ifp = if_get(rt->rt_ifidx);
		rtfree(rt);

		/*
		 * See if we found an interface, and confirm that it
		 * supports multicast.
		 */
		if (ifp == NULL || (ifp->if_flags & IFF_MULTICAST) == 0) {
			error = EADDRNOTAVAIL;
			if_put(ifp);
			break;
		}
		/*
		 * See if the membership already exists or if all the
		 * membership slots are full.
		 */
		for (i = 0; i < imo->imo_num_memberships; ++i) {
			if (imo->imo_membership[i]->inm_ifidx
						== ifp->if_index &&
			    imo->imo_membership[i]->inm_addr.s_addr
						== mreq->imr_multiaddr.s_addr)
				break;
		}
		if (i < imo->imo_num_memberships) {
			error = EADDRINUSE;
			if_put(ifp);
			break;
		}
		if (imo->imo_num_memberships == imo->imo_max_memberships) {
			struct in_multi **nmships, **omships;
			size_t newmax;
			/*
			 * Resize the vector to next power-of-two minus 1. If the
			 * size would exceed the maximum then we know we've really
			 * run out of entries. Otherwise, we reallocate the vector.
			 */
			nmships = NULL;
			omships = imo->imo_membership;
			newmax = ((imo->imo_max_memberships + 1) * 2) - 1;
			if (newmax <= IP_MAX_MEMBERSHIPS) {
				nmships = (struct in_multi **)mallocarray(
				    newmax, sizeof(*nmships), M_IPMOPTS,
				    M_NOWAIT|M_ZERO);
				if (nmships != NULL) {
					memcpy(nmships, omships,
					    sizeof(*omships) *
					    imo->imo_max_memberships);
					free(omships, M_IPMOPTS,
					    sizeof(*omships) *
					    imo->imo_max_memberships);
					imo->imo_membership = nmships;
					imo->imo_max_memberships = newmax;
				}
			}
			if (nmships == NULL) {
				error = ENOBUFS;
				if_put(ifp);
				break;
			}
		}
		/*
		 * Everything looks good; add a new record to the multicast
		 * address list for the given interface.
		 */
		if ((imo->imo_membership[i] =
		    in_addmulti(&mreq->imr_multiaddr, ifp)) == NULL) {
			error = ENOBUFS;
			if_put(ifp);
			break;
		}
		++imo->imo_num_memberships;
		if_put(ifp);
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
			memset(&sin, 0, sizeof(sin));
			sin.sin_len = sizeof(sin);
			sin.sin_family = AF_INET;
			sin.sin_addr = mreq->imr_interface;
			ia = ifatoia(ifa_ifwithaddr(sintosa(&sin), rtableid));
			if (ia == NULL) {
				error = EADDRNOTAVAIL;
				break;
			}
			ifp = ia->ia_ifp;
		}
		/*
		 * Find the membership in the membership array.
		 */
		for (i = 0; i < imo->imo_num_memberships; ++i) {
			if ((ifp == NULL ||
			    imo->imo_membership[i]->inm_ifidx ==
			        ifp->if_index) &&
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
	 * If all options have default values, no need to keep the data.
	 */
	if (imo->imo_ifidx == 0 &&
	    imo->imo_ttl == IP_DEFAULT_MULTICAST_TTL &&
	    imo->imo_loop == IP_DEFAULT_MULTICAST_LOOP &&
	    imo->imo_num_memberships == 0) {
		free(imo->imo_membership , M_IPMOPTS, 0);
		free(*imop, M_IPMOPTS, sizeof(**imop));
		*imop = NULL;
	}

	return (error);
}

/*
 * Return the IP multicast options in response to user getsockopt().
 */
int
ip_getmoptions(int optname, struct ip_moptions *imo, struct mbuf *m)
{
	u_char *ttl;
	u_char *loop;
	struct in_addr *addr;
	struct in_ifaddr *ia;
	struct ifnet *ifp;

	switch (optname) {

	case IP_MULTICAST_IF:
		addr = mtod(m, struct in_addr *);
		m->m_len = sizeof(struct in_addr);
		if (imo == NULL || (ifp = if_get(imo->imo_ifidx)) == NULL)
			addr->s_addr = INADDR_ANY;
		else {
			IFP_TO_IA(ifp, ia);
			if_put(ifp);
			addr->s_addr = (ia == NULL) ? INADDR_ANY
					: ia->ia_addr.sin_addr.s_addr;
		}
		return (0);

	case IP_MULTICAST_TTL:
		ttl = mtod(m, u_char *);
		m->m_len = 1;
		*ttl = (imo == NULL) ? IP_DEFAULT_MULTICAST_TTL
				     : imo->imo_ttl;
		return (0);

	case IP_MULTICAST_LOOP:
		loop = mtod(m, u_char *);
		m->m_len = 1;
		*loop = (imo == NULL) ? IP_DEFAULT_MULTICAST_LOOP
				      : imo->imo_loop;
		return (0);

	default:
		return (EOPNOTSUPP);
	}
}

/*
 * Discard the IP multicast options.
 */
void
ip_freemoptions(struct ip_moptions *imo)
{
	int i;

	if (imo != NULL) {
		for (i = 0; i < imo->imo_num_memberships; ++i)
			in_delmulti(imo->imo_membership[i]);
		free(imo->imo_membership, M_IPMOPTS, 0);
		free(imo, M_IPMOPTS, sizeof(*imo));
	}
}

/*
 * Routine called from ip_output() to loop back a copy of an IP multicast
 * packet to the input queue of a specified interface.
 */
void
ip_mloopback(struct ifnet *ifp, struct mbuf *m, struct sockaddr_in *dst)
{
	struct ip *ip;
	struct mbuf *copym;

	copym = m_dup_pkt(m, max_linkhdr, M_DONTWAIT);
	if (copym != NULL) {
		/*
		 * We don't bother to fragment if the IP length is greater
		 * than the interface's MTU.  Can this possibly matter?
		 */
		ip = mtod(copym, struct ip *);
		ip->ip_sum = 0;
		ip->ip_sum = in_cksum(copym, ip->ip_hl << 2);
		if_input_local(ifp, copym, dst->sin_family);
	}
}

/*
 *	Compute significant parts of the IPv4 checksum pseudo-header
 *	for use in a delayed TCP/UDP checksum calculation.
 */
static __inline u_int16_t __attribute__((__unused__))
in_cksum_phdr(u_int32_t src, u_int32_t dst, u_int32_t lenproto)
{
	u_int32_t sum;

	sum = lenproto +
	      (u_int16_t)(src >> 16) +
	      (u_int16_t)(src /*& 0xffff*/) +
	      (u_int16_t)(dst >> 16) +
	      (u_int16_t)(dst /*& 0xffff*/);

	sum = (u_int16_t)(sum >> 16) + (u_int16_t)(sum /*& 0xffff*/);

	if (sum > 0xffff)
		sum -= 0xffff;

	return (sum);
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

	case IPPROTO_ICMP:
		offset += offsetof(struct icmp, icmp_cksum);
		break;

	default:
		return;
	}

	if ((offset + sizeof(u_int16_t)) > m->m_len)
		m_copyback(m, offset, sizeof(csum), &csum, M_NOWAIT);
	else
		*(u_int16_t *)(mtod(m, caddr_t) + offset) = csum;
}

void
in_proto_cksum_out(struct mbuf *m, struct ifnet *ifp)
{
	struct ip *ip = mtod(m, struct ip *);

	/* some hw and in_delayed_cksum need the pseudo header cksum */
	if (m->m_pkthdr.csum_flags &
	    (M_TCP_CSUM_OUT|M_UDP_CSUM_OUT|M_ICMP_CSUM_OUT)) {
		u_int16_t csum = 0, offset;

		offset = ip->ip_hl << 2;
		if (m->m_pkthdr.csum_flags & (M_TCP_CSUM_OUT|M_UDP_CSUM_OUT))
			csum = in_cksum_phdr(ip->ip_src.s_addr,
			    ip->ip_dst.s_addr, htonl(ntohs(ip->ip_len) -
			    offset + ip->ip_p));
		if (ip->ip_p == IPPROTO_TCP)
			offset += offsetof(struct tcphdr, th_sum);
		else if (ip->ip_p == IPPROTO_UDP)
			offset += offsetof(struct udphdr, uh_sum);
		else if (ip->ip_p == IPPROTO_ICMP)
			offset += offsetof(struct icmp, icmp_cksum);
		if ((offset + sizeof(u_int16_t)) > m->m_len)
			m_copyback(m, offset, sizeof(csum), &csum, M_NOWAIT);
		else
			*(u_int16_t *)(mtod(m, caddr_t) + offset) = csum;
	}

	if (m->m_pkthdr.csum_flags & M_TCP_CSUM_OUT) {
		if (!ifp || !(ifp->if_capabilities & IFCAP_CSUM_TCPv4) ||
		    ip->ip_hl != 5 || ifp->if_bridgeport != NULL) {
			tcpstat_inc(tcps_outswcsum);
			in_delayed_cksum(m);
			m->m_pkthdr.csum_flags &= ~M_TCP_CSUM_OUT; /* Clear */
		}
	} else if (m->m_pkthdr.csum_flags & M_UDP_CSUM_OUT) {
		if (!ifp || !(ifp->if_capabilities & IFCAP_CSUM_UDPv4) ||
		    ip->ip_hl != 5 || ifp->if_bridgeport != NULL) {
			udpstat_inc(udps_outswcsum);
			in_delayed_cksum(m);
			m->m_pkthdr.csum_flags &= ~M_UDP_CSUM_OUT; /* Clear */
		}
	} else if (m->m_pkthdr.csum_flags & M_ICMP_CSUM_OUT) {
		in_delayed_cksum(m);
		m->m_pkthdr.csum_flags &= ~M_ICMP_CSUM_OUT; /* Clear */
	}
}
