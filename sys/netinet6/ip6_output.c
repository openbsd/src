/*	$OpenBSD: ip6_output.c,v 1.70 2002/09/11 03:15:36 itojun Exp $	*/
/*	$KAME: ip6_output.c,v 1.172 2001/03/25 09:55:56 itojun Exp $	*/

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/errno.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/systm.h>
#include <sys/proc.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>

#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/nd6.h>

#if NPF > 0
#include <net/pfvar.h>
#endif

#ifdef IPSEC
#include <netinet/ip_ah.h>
#include <netinet/ip_esp.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <net/pfkeyv2.h>

extern u_int8_t get_sa_require(struct inpcb *);

extern int ipsec_auth_default_level;
extern int ipsec_esp_trans_default_level;
extern int ipsec_esp_network_default_level;
extern int ipsec_ipcomp_default_level;
#endif /* IPSEC */

struct ip6_exthdrs {
	struct mbuf *ip6e_ip6;
	struct mbuf *ip6e_hbh;
	struct mbuf *ip6e_dest1;
	struct mbuf *ip6e_rthdr;
	struct mbuf *ip6e_dest2;
};

static int ip6_pcbopts(struct ip6_pktopts **, struct mbuf *, struct socket *);
static int ip6_setmoptions(int, struct ip6_moptions **, struct mbuf *);
static int ip6_getmoptions(int, struct ip6_moptions *, struct mbuf **);
static int ip6_copyexthdr(struct mbuf **, caddr_t, int);
static int ip6_insertfraghdr(struct mbuf *, struct mbuf *, int,
	struct ip6_frag **);
static int ip6_insert_jumboopt(struct ip6_exthdrs *, u_int32_t);
static int ip6_splithdr(struct mbuf *, struct ip6_exthdrs *);
static int ip6_getpmtu(struct route_in6 *, struct route_in6 *,
	struct ifnet *, struct in6_addr *, u_long *);

/*
 * IP6 output. The packet in mbuf chain m contains a skeletal IP6
 * header (with pri, len, nxt, hlim, src, dst).
 * This function may modify ver and hlim only.
 * The mbuf chain containing the packet will be freed.
 * The mbuf opt, if present, will not be freed.
 *
 * type of "mtu": rt_rmx.rmx_mtu is u_long, ifnet.ifr_mtu is int, and
 * nd_ifinfo.linkmtu is u_int32_t.  so we use u_long to hold largest one,
 * which is rt_rmx.rmx_mtu.
 */
int
ip6_output(m0, opt, ro, flags, im6o, ifpp)
	struct mbuf *m0;
	struct ip6_pktopts *opt;
	struct route_in6 *ro;
	int flags;
	struct ip6_moptions *im6o;
	struct ifnet **ifpp;		/* XXX: just for statistics */
{
	struct ip6_hdr *ip6, *mhip6;
	struct ifnet *ifp, *origifp;
	struct mbuf *m = m0;
	int hlen, tlen, len, off;
	struct route_in6 ip6route;
	struct sockaddr_in6 *dst;
	int error = 0;
	struct in6_ifaddr *ia;
	u_long mtu;
	u_int32_t optlen = 0, plen = 0, unfragpartlen = 0;
	struct ip6_exthdrs exthdrs;
	struct in6_addr finaldst;
	struct route_in6 *ro_pmtu = NULL;
	int hdrsplit = 0;
	u_int8_t sproto = 0;
#ifdef IPSEC
	struct m_tag *mtag;
	union sockaddr_union sdst;
	struct tdb_ident *tdbi;
	u_int32_t sspi;
	struct inpcb *inp;
	struct tdb *tdb;
	int s;
#endif /* IPSEC */

#ifdef IPSEC
	inp = NULL;	/*XXX*/
	if (inp && (inp->inp_flags & INP_IPV6) == 0)
		panic("ip6_output: IPv4 pcb is passed");
#endif /* IPSEC */

#define MAKE_EXTHDR(hp, mp)						\
    do {								\
	if (hp) {							\
		struct ip6_ext *eh = (struct ip6_ext *)(hp);		\
		error = ip6_copyexthdr((mp), (caddr_t)(hp), 		\
		    ((eh)->ip6e_len + 1) << 3);				\
		if (error)						\
			goto freehdrs;					\
	}								\
    } while (0)

	bzero(&exthdrs, sizeof(exthdrs));
	if (opt) {
		/* Hop-by-Hop options header */
		MAKE_EXTHDR(opt->ip6po_hbh, &exthdrs.ip6e_hbh);
		/* Destination options header(1st part) */
		MAKE_EXTHDR(opt->ip6po_dest1, &exthdrs.ip6e_dest1);
		/* Routing header */
		MAKE_EXTHDR(opt->ip6po_rthdr, &exthdrs.ip6e_rthdr);
		/* Destination options header(2nd part) */
		MAKE_EXTHDR(opt->ip6po_dest2, &exthdrs.ip6e_dest2);
	}

#ifdef IPSEC
	/*
	 * splnet is chosen over spltdb because we are not allowed to
	 * lower the level, and udp6_output calls us in splnet(). XXX check
	 */
	s = splnet();

	/*
	 * Check if there was an outgoing SA bound to the flow
	 * from a transport protocol.
	 */
	ip6 = mtod(m, struct ip6_hdr *);

	/* Do we have any pending SAs to apply ? */
	mtag = m_tag_find(m, PACKET_TAG_IPSEC_PENDING_TDB, NULL);
	if (mtag != NULL) {
#ifdef DIAGNOSTIC
		if (mtag->m_tag_len != sizeof (struct tdb_ident))
			panic("ip6_output: tag of length %d (should be %d",
			    mtag->m_tag_len, sizeof (struct tdb_ident));
#endif
		tdbi = (struct tdb_ident *)(mtag + 1);
		tdb = gettdb(tdbi->spi, &tdbi->dst, tdbi->proto);
		if (tdb == NULL)
			error = -EINVAL;
		m_tag_delete(m, mtag);
	} else
		tdb = ipsp_spd_lookup(m, AF_INET6, sizeof(struct ip6_hdr),
		    &error, IPSP_DIRECTION_OUT, NULL, inp);

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

			goto freehdrs;
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

#if 1 /* XXX */
		/* if we have any extension header, we cannot perform IPsec */
		if (exthdrs.ip6e_hbh || exthdrs.ip6e_dest1 ||
		    exthdrs.ip6e_rthdr || exthdrs.ip6e_dest2) {
			error = EHOSTUNREACH;
			goto freehdrs;
		}
#endif
	}

	/* Fall through to the routing/multicast handling code */
 done_spd:
#endif /* IPSEC */

	/*
	 * Calculate the total length of the extension header chain.
	 * Keep the length of the unfragmentable part for fragmentation.
	 */
	optlen = 0;
	if (exthdrs.ip6e_hbh) optlen += exthdrs.ip6e_hbh->m_len;
	if (exthdrs.ip6e_dest1) optlen += exthdrs.ip6e_dest1->m_len;
	if (exthdrs.ip6e_rthdr) optlen += exthdrs.ip6e_rthdr->m_len;
	unfragpartlen = optlen + sizeof(struct ip6_hdr);
	/* NOTE: we don't add AH/ESP length here. do that later. */
	if (exthdrs.ip6e_dest2) optlen += exthdrs.ip6e_dest2->m_len;

	/*
	 * If we need IPsec, or there is at least one extension header,
	 * separate IP6 header from the payload.
	 */
	if ((sproto || optlen) && !hdrsplit) {
		if ((error = ip6_splithdr(m, &exthdrs)) != 0) {
			m = NULL;
			goto freehdrs;
		}
		m = exthdrs.ip6e_ip6;
		hdrsplit++;
	}

	/* adjust pointer */
	ip6 = mtod(m, struct ip6_hdr *);

	/* adjust mbuf packet header length */
	m->m_pkthdr.len += optlen;
	plen = m->m_pkthdr.len - sizeof(*ip6);

	/* If this is a jumbo payload, insert a jumbo payload option. */
	if (plen > IPV6_MAXPACKET) {
		if (!hdrsplit) {
			if ((error = ip6_splithdr(m, &exthdrs)) != 0) {
				m = NULL;
				goto freehdrs;
			}
			m = exthdrs.ip6e_ip6;
			hdrsplit++;
		}
		/* adjust pointer */
		ip6 = mtod(m, struct ip6_hdr *);
		if ((error = ip6_insert_jumboopt(&exthdrs, plen)) != 0)
			goto freehdrs;
		ip6->ip6_plen = 0;
	} else
		ip6->ip6_plen = htons(plen);

	/*
	 * Concatenate headers and fill in next header fields.
	 * Here we have, on "m"
	 *	IPv6 payload
	 * and we insert headers accordingly.  Finally, we should be getting:
	 *	IPv6 hbh dest1 rthdr ah* [esp* dest2 payload]
	 *
	 * during the header composing process, "m" points to IPv6 header.
	 * "mprev" points to an extension header prior to esp.
	 */
	{
		u_char *nexthdrp = &ip6->ip6_nxt;
		struct mbuf *mprev = m;

		/*
		 * we treat dest2 specially.  this makes IPsec processing
		 * much easier.
		 *
		 * result: IPv6 dest2 payload
		 * m and mprev will point to IPv6 header.
		 */
		if (exthdrs.ip6e_dest2) {
			if (!hdrsplit)
				panic("assumption failed: hdr not split");
			exthdrs.ip6e_dest2->m_next = m->m_next;
			m->m_next = exthdrs.ip6e_dest2;
			*mtod(exthdrs.ip6e_dest2, u_char *) = ip6->ip6_nxt;
			ip6->ip6_nxt = IPPROTO_DSTOPTS;
		}

#define MAKE_CHAIN(m, mp, p, i)\
    do {\
	if (m) {\
		if (!hdrsplit) \
			panic("assumption failed: hdr not split"); \
		*mtod((m), u_char *) = *(p);\
		*(p) = (i);\
		p = mtod((m), u_char *);\
		(m)->m_next = (mp)->m_next;\
		(mp)->m_next = (m);\
		(mp) = (m);\
	}\
    } while (0)
		/*
		 * result: IPv6 hbh dest1 rthdr dest2 payload
		 * m will point to IPv6 header.  mprev will point to the
		 * extension header prior to dest2 (rthdr in the above case).
		 */
		MAKE_CHAIN(exthdrs.ip6e_hbh, mprev, nexthdrp, IPPROTO_HOPOPTS);
		MAKE_CHAIN(exthdrs.ip6e_dest1, mprev, nexthdrp,
		    IPPROTO_DSTOPTS);
		MAKE_CHAIN(exthdrs.ip6e_rthdr, mprev, nexthdrp,
		    IPPROTO_ROUTING);
	}

	/*
	 * If there is a routing header, replace destination address field
	 * with the first hop of the routing header.
	 */
	if (exthdrs.ip6e_rthdr) {
		struct ip6_rthdr *rh;
		struct ip6_rthdr0 *rh0;

		rh = (struct ip6_rthdr *)(mtod(exthdrs.ip6e_rthdr,
		    struct ip6_rthdr *));
		finaldst = ip6->ip6_dst;
		switch (rh->ip6r_type) {
		case IPV6_RTHDR_TYPE_0:
			 rh0 = (struct ip6_rthdr0 *)rh;
			 ip6->ip6_dst = rh0->ip6r0_addr[0];
			 bcopy((caddr_t)&rh0->ip6r0_addr[1],
			     (caddr_t)&rh0->ip6r0_addr[0],
			     sizeof(struct in6_addr) * (rh0->ip6r0_segleft - 1));
			 rh0->ip6r0_addr[rh0->ip6r0_segleft - 1] = finaldst;
			 break;
		default:	/* is it possible? */
			 error = EINVAL;
			 goto bad;
		}
	}

	/* Source address validation */
	if (IN6_IS_ADDR_UNSPECIFIED(&ip6->ip6_src) &&
	    (flags & IPV6_UNSPECSRC) == 0) {
		error = EOPNOTSUPP;
		ip6stat.ip6s_badscope++;
		goto bad;
	}
	if (IN6_IS_ADDR_MULTICAST(&ip6->ip6_src)) {
		error = EOPNOTSUPP;
		ip6stat.ip6s_badscope++;
		goto bad;
	}

	ip6stat.ip6s_localout++;

	/*
	 * Route packet.
	 */
	if (ro == 0) {
		ro = &ip6route;
		bzero((caddr_t)ro, sizeof(*ro));
	}
	ro_pmtu = ro;
	if (opt && opt->ip6po_rthdr)
		ro = &opt->ip6po_route;
	dst = (struct sockaddr_in6 *)&ro->ro_dst;
	/*
	 * If there is a cached route,
	 * check that it is to the same destination
	 * and is still up. If not, free it and try again.
	 */
	if (ro->ro_rt && ((ro->ro_rt->rt_flags & RTF_UP) == 0 ||
	    dst->sin6_family != AF_INET6 ||
	    !IN6_ARE_ADDR_EQUAL(&dst->sin6_addr, &ip6->ip6_dst))) {
		RTFREE(ro->ro_rt);
		ro->ro_rt = (struct rtentry *)0;
	}
	if (ro->ro_rt == 0) {
		bzero(dst, sizeof(*dst));
		dst->sin6_family = AF_INET6;
		dst->sin6_len = sizeof(struct sockaddr_in6);
		dst->sin6_addr = ip6->ip6_dst;
	}
#ifdef IPSEC
	/*
	 * Check if the packet needs encapsulation.
	 * ipsp_process_packet will never come back to here.
	 */
	if (sproto != 0) {
	        s = splnet();

		/* fill in IPv6 header which would be filled later */
		if (!IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst)) {
			if (opt && opt->ip6po_hlim != -1)
				ip6->ip6_hlim = opt->ip6po_hlim & 0xff;
		} else {
			if (im6o != NULL)
				ip6->ip6_hlim = im6o->im6o_multicast_hlim;
			else
				ip6->ip6_hlim = ip6_defmcasthlim;
			if (opt && opt->ip6po_hlim != -1)
				ip6->ip6_hlim = opt->ip6po_hlim & 0xff;

			/*
			 * XXX what should we do if ip6_hlim == 0 and the
			 * packet gets tunnelled?
			 */
		}

		tdb = gettdb(sspi, &sdst, sproto);
		if (tdb == NULL) {
			splx(s);
			error = EHOSTUNREACH;
			m_freem(m);
			goto done;
		}

		/* Latch to PCB */
		if (inp)
			tdb_add_inp(tdb, inp, 0);

		m->m_flags &= ~(M_BCAST | M_MCAST);	/* just in case */

		/* Callee frees mbuf */
		error = ipsp_process_packet(m, tdb, AF_INET6, 0);
		splx(s);
		return error;  /* Nothing more to be done */
	}
#endif /* IPSEC */

	if (!IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst)) {
		/* Unicast */

#define ifatoia6(ifa)	((struct in6_ifaddr *)(ifa))
#define sin6tosa(sin6)	((struct sockaddr *)(sin6))
		/* xxx
		 * interface selection comes here
		 * if an interface is specified from an upper layer,
		 * ifp must point it.
		 */
		if (ro->ro_rt == 0) {
			/*
			 * non-bsdi always clone routes, if parent is
			 * PRF_CLONING.
			 */
			rtalloc((struct route *)ro);
		}
		if (ro->ro_rt == 0) {
			ip6stat.ip6s_noroute++;
			error = EHOSTUNREACH;
			/* XXX in6_ifstat_inc(ifp, ifs6_out_discard); */
			goto bad;
		}
		ia = ifatoia6(ro->ro_rt->rt_ifa);
		ifp = ro->ro_rt->rt_ifp;
		ro->ro_rt->rt_use++;
		if (ro->ro_rt->rt_flags & RTF_GATEWAY)
			dst = (struct sockaddr_in6 *)ro->ro_rt->rt_gateway;
		m->m_flags &= ~(M_BCAST | M_MCAST);	/* just in case */

		in6_ifstat_inc(ifp, ifs6_out_request);

		/*
		 * Check if the outgoing interface conflicts with
		 * the interface specified by ifi6_ifindex (if specified).
		 * Note that loopback interface is always okay.
		 * (this may happen when we are sending a packet to one of
		 *  our own addresses.)
		 */
		if (opt && opt->ip6po_pktinfo
		 && opt->ip6po_pktinfo->ipi6_ifindex) {
			if (!(ifp->if_flags & IFF_LOOPBACK)
			 && ifp->if_index != opt->ip6po_pktinfo->ipi6_ifindex) {
				ip6stat.ip6s_noroute++;
				in6_ifstat_inc(ifp, ifs6_out_discard);
				error = EHOSTUNREACH;
				goto bad;
			}
		}

		if (opt && opt->ip6po_hlim != -1)
			ip6->ip6_hlim = opt->ip6po_hlim & 0xff;
	} else {
		/* Multicast */
		struct	in6_multi *in6m;

		m->m_flags = (m->m_flags & ~M_BCAST) | M_MCAST;

		/*
		 * See if the caller provided any multicast options
		 */
		ifp = NULL;
		if (im6o != NULL) {
			ip6->ip6_hlim = im6o->im6o_multicast_hlim;
			if (im6o->im6o_multicast_ifp != NULL)
				ifp = im6o->im6o_multicast_ifp;
		} else
			ip6->ip6_hlim = ip6_defmcasthlim;

		/*
		 * See if the caller provided the outgoing interface
		 * as an ancillary data.
		 * Boundary check for ifindex is assumed to be already done.
		 */
		if (opt && opt->ip6po_pktinfo && opt->ip6po_pktinfo->ipi6_ifindex)
			ifp = ifindex2ifnet[opt->ip6po_pktinfo->ipi6_ifindex];

		/*
		 * If the destination is a node-local scope multicast,
		 * the packet should be loop-backed only.
		 */
		if (IN6_IS_ADDR_MC_NODELOCAL(&ip6->ip6_dst)) {
			/*
			 * If the outgoing interface is already specified,
			 * it should be a loopback interface.
			 */
			if (ifp && (ifp->if_flags & IFF_LOOPBACK) == 0) {
				ip6stat.ip6s_badscope++;
				error = ENETUNREACH; /* XXX: better error? */
				/* XXX correct ifp? */
				in6_ifstat_inc(ifp, ifs6_out_discard);
				goto bad;
			} else {
				ifp = lo0ifp;
			}
		}

		if (opt && opt->ip6po_hlim != -1)
			ip6->ip6_hlim = opt->ip6po_hlim & 0xff;

		/*
		 * If caller did not provide an interface lookup a
		 * default in the routing table.  This is either a
		 * default for the speicfied group (i.e. a host
		 * route), or a multicast default (a route for the
		 * ``net'' ff00::/8).
		 */
		if (ifp == NULL) {
			if (ro->ro_rt == 0) {
				ro->ro_rt = rtalloc1((struct sockaddr *)
				    &ro->ro_dst, 0);
			}
			if (ro->ro_rt == 0) {
				ip6stat.ip6s_noroute++;
				error = EHOSTUNREACH;
				/* XXX in6_ifstat_inc(ifp, ifs6_out_discard) */
				goto bad;
			}
			ia = ifatoia6(ro->ro_rt->rt_ifa);
			ifp = ro->ro_rt->rt_ifp;
			ro->ro_rt->rt_use++;
		}

		if ((flags & IPV6_FORWARDING) == 0)
			in6_ifstat_inc(ifp, ifs6_out_request);
		in6_ifstat_inc(ifp, ifs6_out_mcast);

		/*
		 * Confirm that the outgoing interface supports multicast.
		 */
		if ((ifp->if_flags & IFF_MULTICAST) == 0) {
			ip6stat.ip6s_noroute++;
			in6_ifstat_inc(ifp, ifs6_out_discard);
			error = ENETUNREACH;
			goto bad;
		}
		IN6_LOOKUP_MULTI(ip6->ip6_dst, ifp, in6m);
		if (in6m != NULL &&
		   (im6o == NULL || im6o->im6o_multicast_loop)) {
			/*
			 * If we belong to the destination multicast group
			 * on the outgoing interface, and the caller did not
			 * forbid loopback, loop back a copy.
			 */
			ip6_mloopback(ifp, m, dst);
		} else {
			/*
			 * If we are acting as a multicast router, perform
			 * multicast forwarding as if the packet had just
			 * arrived on the interface to which we are about
			 * to send.  The multicast forwarding function
			 * recursively calls this function, using the
			 * IPV6_FORWARDING flag to prevent infinite recursion.
			 *
			 * Multicasts that are looped back by ip6_mloopback(),
			 * above, will be forwarded by the ip6_input() routine,
			 * if necessary.
			 */
			if (ip6_mrouter && (flags & IPV6_FORWARDING) == 0) {
				if (ip6_mforward(ip6, ifp, m) != 0) {
					m_freem(m);
					goto done;
				}
			}
		}
		/*
		 * Multicasts with a hoplimit of zero may be looped back,
		 * above, but must not be transmitted on a network.
		 * Also, multicasts addressed to the loopback interface
		 * are not sent -- the above call to ip6_mloopback() will
		 * loop back a copy if this host actually belongs to the
		 * destination group on the loopback interface.
		 */
		if (ip6->ip6_hlim == 0 || (ifp->if_flags & IFF_LOOPBACK)) {
			m_freem(m);
			goto done;
		}
	}

	/*
	 * Fill the outgoing inteface to tell the upper layer
	 * to increment per-interface statistics.
	 */
	if (ifpp)
		*ifpp = ifp;

	/* Determine path MTU. */
	if ((error = ip6_getpmtu(ro_pmtu, ro, ifp, &finaldst, &mtu)) != 0)
		goto bad;

	/*
	 * The caller of this function may specify to use the minimum MTU
	 * in some cases.
	 */
	if (mtu > IPV6_MMTU) {
		if ((flags & IPV6_MINMTU))
			mtu = IPV6_MMTU;
	}

	/* Fake scoped addresses */
	if ((ifp->if_flags & IFF_LOOPBACK) != 0) {
		/*
		 * If source or destination address is a scoped address, and
		 * the packet is going to be sent to a loopback interface,
		 * we should keep the original interface.
		 */

		/*
		 * XXX: this is a very experimental and temporary solution.
		 * We eventually have sockaddr_in6 and use the sin6_scope_id
		 * field of the structure here.
		 * We rely on the consistency between two scope zone ids
		 * of source add destination, which should already be assured
		 * Larger scopes than link will be supported in the near
		 * future.
		 */
		origifp = NULL;
		if (IN6_IS_SCOPE_LINKLOCAL(&ip6->ip6_src))
			origifp = ifindex2ifnet[ntohs(ip6->ip6_src.s6_addr16[1])];
		else if (IN6_IS_SCOPE_LINKLOCAL(&ip6->ip6_dst))
			origifp = ifindex2ifnet[ntohs(ip6->ip6_dst.s6_addr16[1])];
		/*
		 * XXX: origifp can be NULL even in those two cases above.
		 * For example, if we remove the (only) link-local address
		 * from the loopback interface, and try to send a link-local
		 * address without link-id information.  Then the source
		 * address is ::1, and the destination address is the
		 * link-local address with its s6_addr16[1] being zero.
		 * What is worse, if the packet goes to the loopback interface
		 * by a default rejected route, the null pointer would be
		 * passed to looutput, and the kernel would hang.
		 * The following last resort would prevent such disaster.
		 */
		if (origifp == NULL)
			origifp = ifp;
	} else
		origifp = ifp;
	if (IN6_IS_SCOPE_LINKLOCAL(&ip6->ip6_src))
		ip6->ip6_src.s6_addr16[1] = 0;
	if (IN6_IS_SCOPE_LINKLOCAL(&ip6->ip6_dst))
		ip6->ip6_dst.s6_addr16[1] = 0;

	/*
	 * If the outgoing packet contains a hop-by-hop options header,
	 * it must be examined and processed even by the source node.
	 * (RFC 2460, section 4.)
	 */
	if (exthdrs.ip6e_hbh) {
		struct ip6_hbh *hbh = mtod(exthdrs.ip6e_hbh, struct ip6_hbh *);
		u_int32_t dummy1; /* XXX unused */
		u_int32_t dummy2; /* XXX unused */

		/*
		 *  XXX: if we have to send an ICMPv6 error to the sender,
		 *       we need the M_LOOP flag since icmp6_error() expects
		 *       the IPv6 and the hop-by-hop options header are
		 *       continuous unless the flag is set.
		 */
		m->m_flags |= M_LOOP;
		m->m_pkthdr.rcvif = ifp;
		if (ip6_process_hopopts(m, (u_int8_t *)(hbh + 1),
		    ((hbh->ip6h_len + 1) << 3) - sizeof(struct ip6_hbh),
		    &dummy1, &dummy2) < 0) {
			/* m was already freed at this point */
			error = EINVAL;/* better error? */
			goto done;
		}
		m->m_flags &= ~M_LOOP; /* XXX */
		m->m_pkthdr.rcvif = NULL;
	}

#if NPF > 0
	if (pf_test6(PF_OUT, ifp, &m) != PF_PASS) {
		error = EHOSTUNREACH;
		m_freem(m);
		goto done;
	}
	if (m == NULL)
		goto done;
	ip6 = mtod(m, struct ip6_hdr *);
#endif

	/*
	 * Send the packet to the outgoing interface.
	 * If necessary, do IPv6 fragmentation before sending.
	 */
	tlen = m->m_pkthdr.len;
	if (tlen <= mtu) {
		error = nd6_output(ifp, origifp, m, dst, ro->ro_rt);
		goto done;
	} else if (mtu < IPV6_MMTU) {
		/*
		 * note that path MTU is never less than IPV6_MMTU
		 * (see icmp6_input).
		 */
		error = EMSGSIZE;
		in6_ifstat_inc(ifp, ifs6_out_fragfail);
		goto bad;
	} else if (ip6->ip6_plen == 0) { /* jumbo payload cannot be fragmented */
		error = EMSGSIZE;
		in6_ifstat_inc(ifp, ifs6_out_fragfail);
		goto bad;
	} else {
		struct mbuf **mnext, *m_frgpart;
		struct ip6_frag *ip6f;
		u_int32_t id = htonl(ip6_id++);
		u_char nextproto;

		/*
		 * Too large for the destination or interface;
		 * fragment if possible.
		 * Must be able to put at least 8 bytes per fragment.
		 */
		hlen = unfragpartlen;
		if (mtu > IPV6_MAXPACKET)
			mtu = IPV6_MAXPACKET;
		len = (mtu - hlen - sizeof(struct ip6_frag)) & ~7;
		if (len < 8) {
			error = EMSGSIZE;
			in6_ifstat_inc(ifp, ifs6_out_fragfail);
			goto bad;
		}

		mnext = &m->m_nextpkt;

		/*
		 * Change the next header field of the last header in the
		 * unfragmentable part.
		 */
		if (exthdrs.ip6e_rthdr) {
			nextproto = *mtod(exthdrs.ip6e_rthdr, u_char *);
			*mtod(exthdrs.ip6e_rthdr, u_char *) = IPPROTO_FRAGMENT;
		} else if (exthdrs.ip6e_dest1) {
			nextproto = *mtod(exthdrs.ip6e_dest1, u_char *);
			*mtod(exthdrs.ip6e_dest1, u_char *) = IPPROTO_FRAGMENT;
		} else if (exthdrs.ip6e_hbh) {
			nextproto = *mtod(exthdrs.ip6e_hbh, u_char *);
			*mtod(exthdrs.ip6e_hbh, u_char *) = IPPROTO_FRAGMENT;
		} else {
			nextproto = ip6->ip6_nxt;
			ip6->ip6_nxt = IPPROTO_FRAGMENT;
		}

		/*
		 * Loop through length of segment after first fragment,
		 * make new header and copy data of each part and link onto
		 * chain.
		 */
		m0 = m;
		for (off = hlen; off < tlen; off += len) {
			MGETHDR(m, M_DONTWAIT, MT_HEADER);
			if (!m) {
				error = ENOBUFS;
				ip6stat.ip6s_odropped++;
				goto sendorfree;
			}
			m->m_flags = m0->m_flags & M_COPYFLAGS;
			*mnext = m;
			mnext = &m->m_nextpkt;
			m->m_data += max_linkhdr;
			mhip6 = mtod(m, struct ip6_hdr *);
			*mhip6 = *ip6;
			m->m_len = sizeof(*mhip6);
			error = ip6_insertfraghdr(m0, m, hlen, &ip6f);
			if (error) {
				ip6stat.ip6s_odropped++;
				goto sendorfree;
			}
			ip6f->ip6f_offlg = htons((u_short)((off - hlen) & ~7));
			if (off + len >= tlen)
				len = tlen - off;
			else
				ip6f->ip6f_offlg |= IP6F_MORE_FRAG;
			mhip6->ip6_plen = htons((u_short)(len + hlen +
			    sizeof(*ip6f) - sizeof(struct ip6_hdr)));
			if ((m_frgpart = m_copy(m0, off, len)) == 0) {
				error = ENOBUFS;
				ip6stat.ip6s_odropped++;
				goto sendorfree;
			}
			m_cat(m, m_frgpart);
			m->m_pkthdr.len = len + hlen + sizeof(*ip6f);
			m->m_pkthdr.rcvif = (struct ifnet *)0;
			ip6f->ip6f_reserved = 0;
			ip6f->ip6f_ident = id;
			ip6f->ip6f_nxt = nextproto;
			ip6stat.ip6s_ofragments++;
			in6_ifstat_inc(ifp, ifs6_out_fragcreat);
		}

		in6_ifstat_inc(ifp, ifs6_out_fragok);
	}

	/*
	 * Remove leading garbages.
	 */
sendorfree:
	m = m0->m_nextpkt;
	m0->m_nextpkt = 0;
	m_freem(m0);
	for (m0 = m; m; m = m0) {
		m0 = m->m_nextpkt;
		m->m_nextpkt = 0;
		if (error == 0) {
			error = nd6_output(ifp, origifp, m, dst, ro->ro_rt);
		} else
			m_freem(m);
	}

	if (error == 0)
		ip6stat.ip6s_fragmented++;

done:
	if (ro == &ip6route && ro->ro_rt) { /* brace necessary for RTFREE */
		RTFREE(ro->ro_rt);
	} else if (ro_pmtu == &ip6route && ro_pmtu->ro_rt) {
		RTFREE(ro_pmtu->ro_rt);
	}

	return (error);

freehdrs:
	m_freem(exthdrs.ip6e_hbh);	/* m_freem will check if mbuf is 0 */
	m_freem(exthdrs.ip6e_dest1);
	m_freem(exthdrs.ip6e_rthdr);
	m_freem(exthdrs.ip6e_dest2);
	/* FALLTHROUGH */
bad:
	m_freem(m);
	goto done;
}

static int
ip6_copyexthdr(mp, hdr, hlen)
	struct mbuf **mp;
	caddr_t hdr;
	int hlen;
{
	struct mbuf *m;

	if (hlen > MCLBYTES)
		return (ENOBUFS); /* XXX */

	MGET(m, M_DONTWAIT, MT_DATA);
	if (!m)
		return (ENOBUFS);

	if (hlen > MLEN) {
		MCLGET(m, M_DONTWAIT);
		if ((m->m_flags & M_EXT) == 0) {
			m_free(m);
			return (ENOBUFS);
		}
	}
	m->m_len = hlen;
	if (hdr)
		bcopy(hdr, mtod(m, caddr_t), hlen);

	*mp = m;
	return (0);
}

/*
 * Insert jumbo payload option.
 */
static int
ip6_insert_jumboopt(exthdrs, plen)
	struct ip6_exthdrs *exthdrs;
	u_int32_t plen;
{
	struct mbuf *mopt;
	u_char *optbuf;
	u_int32_t v;

#define JUMBOOPTLEN	8	/* length of jumbo payload option and padding */

	/*
	 * If there is no hop-by-hop options header, allocate new one.
	 * If there is one but it doesn't have enough space to store the
	 * jumbo payload option, allocate a cluster to store the whole options.
	 * Otherwise, use it to store the options.
	 */
	if (exthdrs->ip6e_hbh == 0) {
		MGET(mopt, M_DONTWAIT, MT_DATA);
		if (mopt == 0)
			return (ENOBUFS);
		mopt->m_len = JUMBOOPTLEN;
		optbuf = mtod(mopt, u_char *);
		optbuf[1] = 0;	/* = ((JUMBOOPTLEN) >> 3) - 1 */
		exthdrs->ip6e_hbh = mopt;
	} else {
		struct ip6_hbh *hbh;

		mopt = exthdrs->ip6e_hbh;
		if (M_TRAILINGSPACE(mopt) < JUMBOOPTLEN) {
			/*
			 * XXX assumption:
			 * - exthdrs->ip6e_hbh is not referenced from places
			 *   other than exthdrs.
			 * - exthdrs->ip6e_hbh is not an mbuf chain.
			 */
			int oldoptlen = mopt->m_len;
			struct mbuf *n;

			/*
			 * XXX: give up if the whole (new) hbh header does
			 * not fit even in an mbuf cluster.
			 */
			if (oldoptlen + JUMBOOPTLEN > MCLBYTES)
				return (ENOBUFS);

			/*
			 * As a consequence, we must always prepare a cluster
			 * at this point.
			 */
			MGET(n, M_DONTWAIT, MT_DATA);
			if (n) {
				MCLGET(n, M_DONTWAIT);
				if ((n->m_flags & M_EXT) == 0) {
					m_freem(n);
					n = NULL;
				}
			}
			if (!n)
				return (ENOBUFS);
			n->m_len = oldoptlen + JUMBOOPTLEN;
			bcopy(mtod(mopt, caddr_t), mtod(n, caddr_t),
			      oldoptlen);
			optbuf = mtod(n, caddr_t) + oldoptlen;
			m_freem(mopt);
			mopt = exthdrs->ip6e_hbh = n;
		} else {
			optbuf = mtod(mopt, u_char *) + mopt->m_len;
			mopt->m_len += JUMBOOPTLEN;
		}
		optbuf[0] = IP6OPT_PADN;
		optbuf[1] = 1;

		/*
		 * Adjust the header length according to the pad and
		 * the jumbo payload option.
		 */
		hbh = mtod(mopt, struct ip6_hbh *);
		hbh->ip6h_len += (JUMBOOPTLEN >> 3);
	}

	/* fill in the option. */
	optbuf[2] = IP6OPT_JUMBO;
	optbuf[3] = 4;
	v = (u_int32_t)htonl(plen + JUMBOOPTLEN);
	bcopy(&v, &optbuf[4], sizeof(u_int32_t));

	/* finally, adjust the packet header length */
	exthdrs->ip6e_ip6->m_pkthdr.len += JUMBOOPTLEN;

	return (0);
#undef JUMBOOPTLEN
}

/*
 * Insert fragment header and copy unfragmentable header portions.
 */
static int
ip6_insertfraghdr(m0, m, hlen, frghdrp)
	struct mbuf *m0, *m;
	int hlen;
	struct ip6_frag **frghdrp;
{
	struct mbuf *n, *mlast;

	if (hlen > sizeof(struct ip6_hdr)) {
		n = m_copym(m0, sizeof(struct ip6_hdr),
		    hlen - sizeof(struct ip6_hdr), M_DONTWAIT);
		if (n == 0)
			return (ENOBUFS);
		m->m_next = n;
	} else
		n = m;

	/* Search for the last mbuf of unfragmentable part. */
	for (mlast = n; mlast->m_next; mlast = mlast->m_next)
		;

	if ((mlast->m_flags & M_EXT) == 0 &&
	    M_TRAILINGSPACE(mlast) >= sizeof(struct ip6_frag)) {
		/* use the trailing space of the last mbuf for the fragment hdr */
		*frghdrp = (struct ip6_frag *)(mtod(mlast, caddr_t) +
		    mlast->m_len);
		mlast->m_len += sizeof(struct ip6_frag);
		m->m_pkthdr.len += sizeof(struct ip6_frag);
	} else {
		/* allocate a new mbuf for the fragment header */
		struct mbuf *mfrg;

		MGET(mfrg, M_DONTWAIT, MT_DATA);
		if (mfrg == 0)
			return (ENOBUFS);
		mfrg->m_len = sizeof(struct ip6_frag);
		*frghdrp = mtod(mfrg, struct ip6_frag *);
		mlast->m_next = mfrg;
	}

	return (0);
}

static int
ip6_getpmtu(ro_pmtu, ro, ifp, dst, mtup)
	struct route_in6 *ro_pmtu, *ro;
	struct ifnet *ifp;
	struct in6_addr *dst;
	u_long *mtup;
{
	u_int32_t mtu = 0;
	int error = 0;

	if (ro_pmtu != ro) {
		/* The first hop and the final destination may differ. */
		struct sockaddr_in6 *sa6_dst =
		    (struct sockaddr_in6 *)&ro_pmtu->ro_dst;
		if (ro_pmtu->ro_rt &&
		    ((ro_pmtu->ro_rt->rt_flags & RTF_UP) == 0 ||
		     !IN6_ARE_ADDR_EQUAL(&sa6_dst->sin6_addr, dst))) {
			RTFREE(ro_pmtu->ro_rt);
			ro_pmtu->ro_rt = (struct rtentry *)0;
		}
		if (ro_pmtu->ro_rt == 0) {
			bzero(sa6_dst, sizeof(*sa6_dst));
			sa6_dst->sin6_family = AF_INET6;
			sa6_dst->sin6_len = sizeof(struct sockaddr_in6);
			sa6_dst->sin6_addr = *dst;

			rtalloc((struct route *)ro_pmtu);
		}
	}
	if (ro_pmtu->ro_rt) {
		u_int32_t ifmtu;

		if (ifp == NULL)
			ifp = ro_pmtu->ro_rt->rt_ifp;
		ifmtu = IN6_LINKMTU(ifp);
		mtu = ro_pmtu->ro_rt->rt_rmx.rmx_mtu;
		if (mtu == 0)
			mtu = ifmtu;
		else if (mtu > ifmtu) {
			/*
			 * The MTU on the route is larger than the MTU on
			 * the interface!  This shouldn't happen, unless the
			 * MTU of the interface has been changed after the
			 * interface was brought up.  Change the MTU in the
			 * route to match the interface MTU (as long as the
			 * field isn't locked).
			 *
			 * if MTU on the route is 0, we need to fix the MTU.
			 * this case happens with path MTU discovery timeouts.
			 */
			mtu = ifmtu;
			if (!(ro_pmtu->ro_rt->rt_rmx.rmx_locks & RTV_MTU))
				ro_pmtu->ro_rt->rt_rmx.rmx_mtu = mtu;
		}
	} else if (ifp) {
		mtu = IN6_LINKMTU(ifp);
	} else
		error = EHOSTUNREACH; /* XXX */

	*mtup = mtu;
	return (error);
}

/*
 * IP6 socket option processing.
 */
int
ip6_ctloutput(op, so, level, optname, mp)
	int op;
	struct socket *so;
	int level, optname;
	struct mbuf **mp;
{
	int privileged;
	struct inpcb *inp = sotoinpcb(so);
	struct mbuf *m = *mp;
	int error, optval;
	int optlen;
#ifdef IPSEC
	struct proc *p = curproc; /* XXX */
	struct tdb *tdb;
	struct tdb_ident *tdbip, tdbi;
	int s;
#endif

	optlen = m ? m->m_len : 0;
	error = optval = 0;

	privileged = (inp->inp_socket->so_state & SS_PRIV);

	if (level == IPPROTO_IPV6) {
		switch (op) {
		case PRCO_SETOPT:
			switch (optname) {
			case IPV6_PKTOPTIONS:
				/* m is freed in ip6_pcbopts */
				return (ip6_pcbopts(&inp->inp_outputopts6,
				    m, so));
			case IPV6_HOPOPTS:
			case IPV6_DSTOPTS:
				if (!privileged) {
					error = EPERM;
					break;
				}
				/* FALLTHROUGH */
			case IPV6_UNICAST_HOPS:
			case IPV6_RECVOPTS:
			case IPV6_RECVRETOPTS:
			case IPV6_RECVDSTADDR:
			case IPV6_PKTINFO:
			case IPV6_HOPLIMIT:
			case IPV6_RTHDR:
			case IPV6_FAITH:
			case IPV6_V6ONLY:
				if (optlen != sizeof(int)) {
					error = EINVAL;
					break;
				}
				optval = *mtod(m, int *);
				switch (optname) {

				case IPV6_UNICAST_HOPS:
					if (optval < -1 || optval >= 256)
						error = EINVAL;
					else {
						/* -1 = kernel default */
						inp->inp_hops = optval;
					}
					break;
#define OPTSET(bit) \
do { \
	if (optval) \
		inp->inp_flags |= (bit); \
	else \
		inp->inp_flags &= ~(bit); \
} while (0)
				case IPV6_RECVOPTS:
					OPTSET(IN6P_RECVOPTS);
					break;

				case IPV6_RECVRETOPTS:
					OPTSET(IN6P_RECVRETOPTS);
					break;

				case IPV6_RECVDSTADDR:
					OPTSET(IN6P_RECVDSTADDR);
					break;

				case IPV6_PKTINFO:
					OPTSET(IN6P_PKTINFO);
					break;

				case IPV6_HOPLIMIT:
					OPTSET(IN6P_HOPLIMIT);
					break;

				case IPV6_HOPOPTS:
					OPTSET(IN6P_HOPOPTS);
					break;

				case IPV6_DSTOPTS:
					OPTSET(IN6P_DSTOPTS);
					break;

				case IPV6_RTHDR:
					OPTSET(IN6P_RTHDR);
					break;

				case IPV6_FAITH:
					OPTSET(IN6P_FAITH);
					break;

				case IPV6_V6ONLY:
					if (!optval)
						error = EINVAL;
					break;
				}
				break;
#undef OPTSET

			case IPV6_MULTICAST_IF:
			case IPV6_MULTICAST_HOPS:
			case IPV6_MULTICAST_LOOP:
			case IPV6_JOIN_GROUP:
			case IPV6_LEAVE_GROUP:
				error =	ip6_setmoptions(optname,
					&inp->inp_moptions6, m);
				break;

			case IPV6_PORTRANGE:
				optval = *mtod(m, int *);

				switch (optval) {
				case IPV6_PORTRANGE_DEFAULT:
					inp->inp_flags &= ~(IN6P_LOWPORT);
					inp->inp_flags &= ~(IN6P_HIGHPORT);
					break;

				case IPV6_PORTRANGE_HIGH:
					inp->inp_flags &= ~(IN6P_LOWPORT);
					inp->inp_flags |= IN6P_HIGHPORT;
					break;

				case IPV6_PORTRANGE_LOW:
					inp->inp_flags &= ~(IN6P_HIGHPORT);
					inp->inp_flags |= IN6P_LOWPORT;
					break;

				default:
					error = EINVAL;
					break;
				}
				break;

			case IPSEC6_OUTSA:
#ifndef IPSEC
				error = EINVAL;
#else
				s = spltdb();
				if (m == 0 || m->m_len != sizeof(struct tdb_ident)) {
					error = EINVAL;
				} else {
					tdbip = mtod(m, struct tdb_ident *);
					tdb = gettdb(tdbip->spi, &tdbip->dst,
						     tdbip->proto);
					if (tdb == NULL)
						error = ESRCH;
					else
						tdb_add_inp(tdb, inp, 0);
				}
				splx(s);
#endif /* IPSEC */
				break;

			case IPV6_AUTH_LEVEL:
			case IPV6_ESP_TRANS_LEVEL:
			case IPV6_ESP_NETWORK_LEVEL:
			case IPV6_IPCOMP_LEVEL:
#ifndef IPSEC
				error = EINVAL;
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

				switch (optname) {
				case IPV6_AUTH_LEVEL:
				        if (optval < ipsec_auth_default_level &&
					    suser(p->p_ucred, &p->p_acflag)) {
						error = EACCES;
						break;
					}
					inp->inp_seclevel[SL_AUTH] = optval;
					break;

				case IPV6_ESP_TRANS_LEVEL:
				        if (optval < ipsec_esp_trans_default_level &&
					    suser(p->p_ucred, &p->p_acflag)) {
						error = EACCES;
						break;
					}
					inp->inp_seclevel[SL_ESP_TRANS] = optval;
					break;

				case IPV6_ESP_NETWORK_LEVEL:
				        if (optval < ipsec_esp_network_default_level &&
					    suser(p->p_ucred, &p->p_acflag)) {
						error = EACCES;
						break;
					}
					inp->inp_seclevel[SL_ESP_NETWORK] = optval;
					break;

				case IPV6_IPCOMP_LEVEL:
				        if (optval < ipsec_ipcomp_default_level &&
					    suser(p->p_ucred, &p->p_acflag)) {
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


			default:
				error = ENOPROTOOPT;
				break;
			}
			if (m)
				(void)m_free(m);
			break;

		case PRCO_GETOPT:
			switch (optname) {

			case IPV6_OPTIONS:
			case IPV6_RETOPTS:
				error = ENOPROTOOPT;
				break;

			case IPV6_PKTOPTIONS:
				if (inp->inp_options) {
					*mp = m_copym(inp->inp_options, 0,
					    M_COPYALL, M_WAIT);
				} else {
					*mp = m_get(M_WAIT, MT_SOOPTS);
					(*mp)->m_len = 0;
				}
				break;

			case IPV6_HOPOPTS:
			case IPV6_DSTOPTS:
				if (!privileged) {
					error = EPERM;
					break;
				}
				/* FALLTHROUGH */
			case IPV6_UNICAST_HOPS:
			case IPV6_RECVOPTS:
			case IPV6_RECVRETOPTS:
			case IPV6_RECVDSTADDR:
			case IPV6_PKTINFO:
			case IPV6_HOPLIMIT:
			case IPV6_RTHDR:
			case IPV6_FAITH:
			case IPV6_V6ONLY:
			case IPV6_PORTRANGE:
				switch (optname) {

				case IPV6_UNICAST_HOPS:
					optval = inp->inp_hops;
					break;

#define OPTBIT(bit) (inp->inp_flags & bit ? 1 : 0)

				case IPV6_RECVOPTS:
					optval = OPTBIT(IN6P_RECVOPTS);
					break;

				case IPV6_RECVRETOPTS:
					optval = OPTBIT(IN6P_RECVRETOPTS);
					break;

				case IPV6_RECVDSTADDR:
					optval = OPTBIT(IN6P_RECVDSTADDR);
					break;

				case IPV6_PKTINFO:
					optval = OPTBIT(IN6P_PKTINFO);
					break;

				case IPV6_HOPLIMIT:
					optval = OPTBIT(IN6P_HOPLIMIT);
					break;

				case IPV6_HOPOPTS:
					optval = OPTBIT(IN6P_HOPOPTS);
					break;

				case IPV6_DSTOPTS:
					optval = OPTBIT(IN6P_DSTOPTS);
					break;

				case IPV6_RTHDR:
					optval = OPTBIT(IN6P_RTHDR);
					break;

				case IPV6_FAITH:
					optval = OPTBIT(IN6P_FAITH);
					break;

				case IPV6_V6ONLY:
					optval = (ip6_v6only != 0); /* XXX */
					break;

				case IPV6_PORTRANGE:
				    {
					int flags;

					flags = inp->inp_flags;
					if (flags & IN6P_HIGHPORT)
						optval = IPV6_PORTRANGE_HIGH;
					else if (flags & IN6P_LOWPORT)
						optval = IPV6_PORTRANGE_LOW;
					else
						optval = 0;
					break;
				    }
				}
				*mp = m = m_get(M_WAIT, MT_SOOPTS);
				m->m_len = sizeof(int);
				*mtod(m, int *) = optval;
				break;

			case IPV6_MULTICAST_IF:
			case IPV6_MULTICAST_HOPS:
			case IPV6_MULTICAST_LOOP:
			case IPV6_JOIN_GROUP:
			case IPV6_LEAVE_GROUP:
				error = ip6_getmoptions(optname, inp->inp_moptions6, mp);
				break;

			case IPSEC6_OUTSA:
#ifndef IPSEC
				error = EINVAL;
#else
				s = spltdb();
				if (inp->inp_tdb_out == NULL) {
					error = ENOENT;
				} else {
					tdbi.spi = inp->inp_tdb_out->tdb_spi;
					tdbi.dst = inp->inp_tdb_out->tdb_dst;
					tdbi.proto = inp->inp_tdb_out->tdb_sproto;
					*mp = m = m_get(M_WAIT, MT_SOOPTS);
					m->m_len = sizeof(tdbi);
					bcopy((caddr_t)&tdbi, mtod(m, caddr_t),
					    (unsigned)m->m_len);
				}
				splx(s);
#endif /* IPSEC */
				break;

			case IPV6_AUTH_LEVEL:
			case IPV6_ESP_TRANS_LEVEL:
			case IPV6_ESP_NETWORK_LEVEL:
			case IPV6_IPCOMP_LEVEL:
#ifndef IPSEC
				m->m_len = sizeof(int);
				*mtod(m, int *) = IPSEC_LEVEL_NONE;
#else
				m->m_len = sizeof(int);
				switch (optname) {
				case IPV6_AUTH_LEVEL:
					optval = inp->inp_seclevel[SL_AUTH];
					break;

				case IPV6_ESP_TRANS_LEVEL:
					optval =
					    inp->inp_seclevel[SL_ESP_TRANS];
					break;

				case IPV6_ESP_NETWORK_LEVEL:
					optval =
					    inp->inp_seclevel[SL_ESP_NETWORK];
					break;

				case IPV6_IPCOMP_LEVEL:
					optval = inp->inp_seclevel[SL_IPCOMP];
					break;
				}
				*mtod(m, int *) = optval;
#endif
				break;

			default:
				error = ENOPROTOOPT;
				break;
			}
			break;
		}
	} else {
		error = EINVAL;
		if (op == PRCO_SETOPT && *mp)
			(void)m_free(*mp);
	}
	return (error);
}

int
ip6_raw_ctloutput(op, so, level, optname, mp)
	int op;
	struct socket *so;
	int level, optname;
	struct mbuf **mp;
{
	int error = 0, optval, optlen;
	const int icmp6off = offsetof(struct icmp6_hdr, icmp6_cksum);
	struct inpcb *inp = sotoinpcb(so);
	struct mbuf *m = *mp;

	optlen = m ? m->m_len : 0;

	if (level != IPPROTO_IPV6) {
		if (op == PRCO_SETOPT && *mp)
			(void)m_free(*mp);
		return (EINVAL);
	}

	switch (optname) {
	case IPV6_CHECKSUM:
		/*
		 * For ICMPv6 sockets, no modification allowed for checksum
		 * offset, permit "no change" values to help existing apps.
		 *
		 * XXX 2292bis says: "An attempt to set IPV6_CHECKSUM
		 * for an ICMPv6 socket will fail."
		 * The current behavior does not meet 2292bis.
		 */
		switch (op) {
		case PRCO_SETOPT:
			if (optlen != sizeof(int)) {
				error = EINVAL;
				break;
			}
			optval = *mtod(m, int *);
			if ((optval % 2) != 0) {
				/* the API assumes even offset values */
				error = EINVAL;
			} else if (so->so_proto->pr_protocol ==
			    IPPROTO_ICMPV6) {
				if (optval != icmp6off)
					error = EINVAL;
			} else
				inp->in6p_cksum = optval;
			break;

		case PRCO_GETOPT:
			if (so->so_proto->pr_protocol == IPPROTO_ICMPV6)
				optval = icmp6off;
			else
				optval = inp->in6p_cksum;

			*mp = m = m_get(M_WAIT, MT_SOOPTS);
			m->m_len = sizeof(int);
			*mtod(m, int *) = optval;
			break;

		default:
			error = EINVAL;
			break;
		}
		break;

	default:
		error = ENOPROTOOPT;
		break;
	}

	if (op == PRCO_SETOPT && m)
		(void)m_free(m);

	return (error);
}

/*
 * Set up IP6 options in pcb for insertion in output packets.
 * Store in mbuf with pointer in pcbopt, adding pseudo-option
 * with destination address if source routed.
 */
static int
ip6_pcbopts(pktopt, m, so)
	struct ip6_pktopts **pktopt;
	struct mbuf *m;
	struct socket *so;
{
	struct ip6_pktopts *opt = *pktopt;
	int error = 0;
	struct proc *p = curproc;	/* XXX */
	int priv = 0;

	/* turn off any old options. */
	if (opt) {
		if (opt->ip6po_m)
			(void)m_free(opt->ip6po_m);
	} else
		opt = malloc(sizeof(*opt), M_IP6OPT, M_WAITOK);
	*pktopt = 0;

	if (!m || m->m_len == 0) {
		/*
		 * Only turning off any previous options.
		 */
		if (opt)
			free(opt, M_IP6OPT);
		if (m)
			(void)m_free(m);
		return (0);
	}

	/*  set options specified by user. */
	if (p && !suser(p->p_ucred, &p->p_acflag))
		priv = 1;
	if ((error = ip6_setpktoptions(m, opt, priv)) != 0) {
		(void)m_free(m);
		return (error);
	}
	*pktopt = opt;
	return (0);
}

/*
 * Set the IP6 multicast options in response to user setsockopt().
 */
static int
ip6_setmoptions(optname, im6op, m)
	int optname;
	struct ip6_moptions **im6op;
	struct mbuf *m;
{
	int error = 0;
	u_int loop, ifindex;
	struct ipv6_mreq *mreq;
	struct ifnet *ifp;
	struct ip6_moptions *im6o = *im6op;
	struct route_in6 ro;
	struct sockaddr_in6 *dst;
	struct in6_multi_mship *imm;
	struct proc *p = curproc;	/* XXX */

	if (im6o == NULL) {
		/*
		 * No multicast option buffer attached to the pcb;
		 * allocate one and initialize to default values.
		 */
		im6o = (struct ip6_moptions *)
			malloc(sizeof(*im6o), M_IPMOPTS, M_WAITOK);

		if (im6o == NULL)
			return (ENOBUFS);
		*im6op = im6o;
		im6o->im6o_multicast_ifp = NULL;
		im6o->im6o_multicast_hlim = ip6_defmcasthlim;
		im6o->im6o_multicast_loop = IPV6_DEFAULT_MULTICAST_LOOP;
		LIST_INIT(&im6o->im6o_memberships);
	}

	switch (optname) {

	case IPV6_MULTICAST_IF:
		/*
		 * Select the interface for outgoing multicast packets.
		 */
		if (m == NULL || m->m_len != sizeof(u_int)) {
			error = EINVAL;
			break;
		}
		bcopy(mtod(m, u_int *), &ifindex, sizeof(ifindex));
		if (ifindex < 0 || if_index < ifindex) {
			error = ENXIO;	/* XXX EINVAL? */
			break;
		}
		ifp = ifindex2ifnet[ifindex];
		if (ifp == NULL || (ifp->if_flags & IFF_MULTICAST) == 0) {
			error = EADDRNOTAVAIL;
			break;
		}
		im6o->im6o_multicast_ifp = ifp;
		break;

	case IPV6_MULTICAST_HOPS:
	    {
		/*
		 * Set the IP6 hoplimit for outgoing multicast packets.
		 */
		int optval;
		if (m == NULL || m->m_len != sizeof(int)) {
			error = EINVAL;
			break;
		}
		bcopy(mtod(m, u_int *), &optval, sizeof(optval));
		if (optval < -1 || optval >= 256)
			error = EINVAL;
		else if (optval == -1)
			im6o->im6o_multicast_hlim = ip6_defmcasthlim;
		else
			im6o->im6o_multicast_hlim = optval;
		break;
	    }

	case IPV6_MULTICAST_LOOP:
		/*
		 * Set the loopback flag for outgoing multicast packets.
		 * Must be zero or one.
		 */
		if (m == NULL || m->m_len != sizeof(u_int)) {
			error = EINVAL;
			break;
		}
		bcopy(mtod(m, u_int *), &loop, sizeof(loop));
		if (loop > 1) {
			error = EINVAL;
			break;
		}
		im6o->im6o_multicast_loop = loop;
		break;

	case IPV6_JOIN_GROUP:
		/*
		 * Add a multicast group membership.
		 * Group must be a valid IP6 multicast address.
		 */
		if (m == NULL || m->m_len != sizeof(struct ipv6_mreq)) {
			error = EINVAL;
			break;
		}
		mreq = mtod(m, struct ipv6_mreq *);
		if (IN6_IS_ADDR_UNSPECIFIED(&mreq->ipv6mr_multiaddr)) {
			/*
			 * We use the unspecified address to specify to accept
			 * all multicast addresses. Only super user is allowed
			 * to do this.
			 */
			if (suser(p->p_ucred, &p->p_acflag))
			{
				error = EACCES;
				break;
			}
		} else if (!IN6_IS_ADDR_MULTICAST(&mreq->ipv6mr_multiaddr)) {
			error = EINVAL;
			break;
		}

		/*
		 * If the interface is specified, validate it.
		 */
		if (mreq->ipv6mr_interface < 0
		 || if_index < mreq->ipv6mr_interface) {
			error = ENXIO;	/* XXX EINVAL? */
			break;
		}
		/*
		 * If no interface was explicitly specified, choose an
		 * appropriate one according to the given multicast address.
		 */
		if (mreq->ipv6mr_interface == 0) {
			/*
			 * If the multicast address is in node-local scope,
			 * the interface should be a loopback interface.
			 * Otherwise, look up the routing table for the
			 * address, and choose the outgoing interface.
			 *   XXX: is it a good approach?
			 */
			if (IN6_IS_ADDR_MC_NODELOCAL(&mreq->ipv6mr_multiaddr)) {
				ifp = lo0ifp;
			} else {
				ro.ro_rt = NULL;
				dst = (struct sockaddr_in6 *)&ro.ro_dst;
				bzero(dst, sizeof(*dst));
				dst->sin6_len = sizeof(struct sockaddr_in6);
				dst->sin6_family = AF_INET6;
				dst->sin6_addr = mreq->ipv6mr_multiaddr;
				rtalloc((struct route *)&ro);
				if (ro.ro_rt == NULL) {
					error = EADDRNOTAVAIL;
					break;
				}
				ifp = ro.ro_rt->rt_ifp;
				rtfree(ro.ro_rt);
			}
		} else
			ifp = ifindex2ifnet[mreq->ipv6mr_interface];

		/*
		 * See if we found an interface, and confirm that it
		 * supports multicast
		 */
		if (ifp == NULL || (ifp->if_flags & IFF_MULTICAST) == 0) {
			error = EADDRNOTAVAIL;
			break;
		}
		/*
		 * Put interface index into the multicast address,
		 * if the address has link-local scope.
		 */
		if (IN6_IS_ADDR_MC_LINKLOCAL(&mreq->ipv6mr_multiaddr)) {
			mreq->ipv6mr_multiaddr.s6_addr16[1] =
			    htons(mreq->ipv6mr_interface);
		}
		/*
		 * See if the membership already exists.
		 */
		for (imm = im6o->im6o_memberships.lh_first;
		     imm != NULL; imm = imm->i6mm_chain.le_next)
			if (imm->i6mm_maddr->in6m_ifp == ifp &&
			    IN6_ARE_ADDR_EQUAL(&imm->i6mm_maddr->in6m_addr,
			    &mreq->ipv6mr_multiaddr))
				break;
		if (imm != NULL) {
			error = EADDRINUSE;
			break;
		}
		/*
		 * Everything looks good; add a new record to the multicast
		 * address list for the given interface.
		 */
		imm = in6_joingroup(ifp, &mreq->ipv6mr_multiaddr, &error);
		if (!imm)
			break;
		LIST_INSERT_HEAD(&im6o->im6o_memberships, imm, i6mm_chain);
		break;

	case IPV6_LEAVE_GROUP:
		/*
		 * Drop a multicast group membership.
		 * Group must be a valid IP6 multicast address.
		 */
		if (m == NULL || m->m_len != sizeof(struct ipv6_mreq)) {
			error = EINVAL;
			break;
		}
		mreq = mtod(m, struct ipv6_mreq *);
		if (IN6_IS_ADDR_UNSPECIFIED(&mreq->ipv6mr_multiaddr)) {
			if (suser(p->p_ucred, &p->p_acflag))
			{
				error = EACCES;
				break;
			}
		} else if (!IN6_IS_ADDR_MULTICAST(&mreq->ipv6mr_multiaddr)) {
			error = EINVAL;
			break;
		}
		/*
		 * If an interface address was specified, get a pointer
		 * to its ifnet structure.
		 */
		if (mreq->ipv6mr_interface < 0
		 || if_index < mreq->ipv6mr_interface) {
			error = ENXIO;	/* XXX EINVAL? */
			break;
		}
		ifp = ifindex2ifnet[mreq->ipv6mr_interface];
		/*
		 * Put interface index into the multicast address,
		 * if the address has link-local scope.
		 */
		if (IN6_IS_ADDR_MC_LINKLOCAL(&mreq->ipv6mr_multiaddr)) {
			mreq->ipv6mr_multiaddr.s6_addr16[1] =
			    htons(mreq->ipv6mr_interface);
		}
		/*
		 * Find the membership in the membership list.
		 */
		for (imm = im6o->im6o_memberships.lh_first;
		     imm != NULL; imm = imm->i6mm_chain.le_next) {
			if ((ifp == NULL || imm->i6mm_maddr->in6m_ifp == ifp) &&
			    IN6_ARE_ADDR_EQUAL(&imm->i6mm_maddr->in6m_addr,
			    &mreq->ipv6mr_multiaddr))
				break;
		}
		if (imm == NULL) {
			/* Unable to resolve interface */
			error = EADDRNOTAVAIL;
			break;
		}
		/*
		 * Give up the multicast address record to which the
		 * membership points.
		 */
		LIST_REMOVE(imm, i6mm_chain);
		in6_leavegroup(imm);
		break;

	default:
		error = EOPNOTSUPP;
		break;
	}

	/*
	 * If all options have default values, no need to keep the mbuf.
	 */
	if (im6o->im6o_multicast_ifp == NULL &&
	    im6o->im6o_multicast_hlim == ip6_defmcasthlim &&
	    im6o->im6o_multicast_loop == IPV6_DEFAULT_MULTICAST_LOOP &&
	    im6o->im6o_memberships.lh_first == NULL) {
		free(*im6op, M_IPMOPTS);
		*im6op = NULL;
	}

	return (error);
}

/*
 * Return the IP6 multicast options in response to user getsockopt().
 */
static int
ip6_getmoptions(optname, im6o, mp)
	int optname;
	struct ip6_moptions *im6o;
	struct mbuf **mp;
{
	u_int *hlim, *loop, *ifindex;

	*mp = m_get(M_WAIT, MT_SOOPTS);

	switch (optname) {

	case IPV6_MULTICAST_IF:
		ifindex = mtod(*mp, u_int *);
		(*mp)->m_len = sizeof(u_int);
		if (im6o == NULL || im6o->im6o_multicast_ifp == NULL)
			*ifindex = 0;
		else
			*ifindex = im6o->im6o_multicast_ifp->if_index;
		return (0);

	case IPV6_MULTICAST_HOPS:
		hlim = mtod(*mp, u_int *);
		(*mp)->m_len = sizeof(u_int);
		if (im6o == NULL)
			*hlim = ip6_defmcasthlim;
		else
			*hlim = im6o->im6o_multicast_hlim;
		return (0);

	case IPV6_MULTICAST_LOOP:
		loop = mtod(*mp, u_int *);
		(*mp)->m_len = sizeof(u_int);
		if (im6o == NULL)
			*loop = ip6_defmcasthlim;
		else
			*loop = im6o->im6o_multicast_loop;
		return (0);

	default:
		return (EOPNOTSUPP);
	}
}

/*
 * Discard the IP6 multicast options.
 */
void
ip6_freemoptions(im6o)
	struct ip6_moptions *im6o;
{
	struct in6_multi_mship *imm;

	if (im6o == NULL)
		return;

	while ((imm = im6o->im6o_memberships.lh_first) != NULL) {
		LIST_REMOVE(imm, i6mm_chain);
		in6_leavegroup(imm);
	}
	free(im6o, M_IPMOPTS);
}

/*
 * Set IPv6 outgoing packet options based on advanced API.
 */
int
ip6_setpktoptions(control, opt, priv)
	struct mbuf *control;
	struct ip6_pktopts *opt;
	int priv;
{
	struct cmsghdr *cm = 0;

	if (control == 0 || opt == 0)
		return (EINVAL);

	bzero(opt, sizeof(*opt));
	opt->ip6po_hlim = -1; /* -1 means to use default hop limit */

	/*
	 * XXX: Currently, we assume all the optional information is stored
	 * in a single mbuf.
	 */
	if (control->m_next)
		return (EINVAL);

	opt->ip6po_m = control;

	for (; control->m_len; control->m_data += CMSG_ALIGN(cm->cmsg_len),
	    control->m_len -= CMSG_ALIGN(cm->cmsg_len)) {
		cm = mtod(control, struct cmsghdr *);
		if (cm->cmsg_len == 0 || cm->cmsg_len > control->m_len)
			return (EINVAL);
		if (cm->cmsg_level != IPPROTO_IPV6)
			continue;

		switch (cm->cmsg_type) {
		case IPV6_PKTINFO:
			if (cm->cmsg_len != CMSG_LEN(sizeof(struct in6_pktinfo)))
				return (EINVAL);
			opt->ip6po_pktinfo = (struct in6_pktinfo *)CMSG_DATA(cm);
			if (opt->ip6po_pktinfo->ipi6_ifindex &&
			    IN6_IS_ADDR_LINKLOCAL(&opt->ip6po_pktinfo->ipi6_addr))
				opt->ip6po_pktinfo->ipi6_addr.s6_addr16[1] =
					htons(opt->ip6po_pktinfo->ipi6_ifindex);

			if (opt->ip6po_pktinfo->ipi6_ifindex > if_index ||
			    opt->ip6po_pktinfo->ipi6_ifindex < 0) {
				return (ENXIO);
			}

			/*
			 * Check if the requested source address is indeed a
			 * unicast address assigned to the node, and can be
			 * used as the packet's source address.
			 */
			if (!IN6_IS_ADDR_UNSPECIFIED(&opt->ip6po_pktinfo->ipi6_addr)) {
				struct ifaddr *ia;
				struct in6_ifaddr *ia6;
				struct sockaddr_in6 sin6;

				bzero(&sin6, sizeof(sin6));
				sin6.sin6_len = sizeof(sin6);
				sin6.sin6_family = AF_INET6;
				sin6.sin6_addr =
					opt->ip6po_pktinfo->ipi6_addr;
				ia = ifa_ifwithaddr(sin6tosa(&sin6));
				if (ia == NULL ||
				    (opt->ip6po_pktinfo->ipi6_ifindex &&
				     (ia->ifa_ifp->if_index !=
				      opt->ip6po_pktinfo->ipi6_ifindex))) {
					return (EADDRNOTAVAIL);
				}
				ia6 = (struct in6_ifaddr *)ia;
				if ((ia6->ia6_flags & (IN6_IFF_ANYCAST|IN6_IFF_NOTREADY)) != 0) {
					return (EADDRNOTAVAIL);
				}

				/*
				 * Check if the requested source address is
				 * indeed a unicast address assigned to the
				 * node.
				 */
				if (IN6_IS_ADDR_MULTICAST(&opt->ip6po_pktinfo->ipi6_addr))
					return (EADDRNOTAVAIL);
			}
			break;

		case IPV6_HOPLIMIT:
			if (cm->cmsg_len != CMSG_LEN(sizeof(int)))
				return (EINVAL);

			bcopy(CMSG_DATA(cm), &opt->ip6po_hlim,
			    sizeof(opt->ip6po_hlim));
			if (opt->ip6po_hlim < -1 || opt->ip6po_hlim > 255)
				return (EINVAL);
			break;

		case IPV6_NEXTHOP:
			if (!priv)
				return (EPERM);

			/* check if cmsg_len is large enough for sa_len */
			if (cm->cmsg_len < sizeof(u_char) ||
			    cm->cmsg_len < CMSG_LEN(*CMSG_DATA(cm)))
				return (EINVAL);

			opt->ip6po_nexthop = (struct sockaddr *)CMSG_DATA(cm);

			break;

		case IPV6_HOPOPTS:
			if (cm->cmsg_len < CMSG_LEN(sizeof(struct ip6_hbh)))
				return (EINVAL);
			opt->ip6po_hbh = (struct ip6_hbh *)CMSG_DATA(cm);
			if (cm->cmsg_len !=
			    CMSG_LEN((opt->ip6po_hbh->ip6h_len + 1) << 3))
				return (EINVAL);
			break;

		case IPV6_DSTOPTS:
			if (cm->cmsg_len < CMSG_LEN(sizeof(struct ip6_dest)))
				return (EINVAL);

			/*
			 * If there is no routing header yet, the destination
			 * options header should be put on the 1st part.
			 * Otherwise, the header should be on the 2nd part.
			 * (See RFC 2460, section 4.1)
			 */
			if (opt->ip6po_rthdr == NULL) {
				opt->ip6po_dest1 =
				    (struct ip6_dest *)CMSG_DATA(cm);
				if (cm->cmsg_len !=
				    CMSG_LEN((opt->ip6po_dest1->ip6d_len + 1) << 3));
					return (EINVAL);
			}
			else {
				opt->ip6po_dest2 =
				    (struct ip6_dest *)CMSG_DATA(cm);
				if (cm->cmsg_len !=
				    CMSG_LEN((opt->ip6po_dest2->ip6d_len + 1) << 3))
					return (EINVAL);
			}
			break;

		case IPV6_RTHDR:
			if (cm->cmsg_len < CMSG_LEN(sizeof(struct ip6_rthdr)))
				return (EINVAL);
			opt->ip6po_rthdr = (struct ip6_rthdr *)CMSG_DATA(cm);
			if (cm->cmsg_len !=
			    CMSG_LEN((opt->ip6po_rthdr->ip6r_len + 1) << 3))
				return (EINVAL);
			switch (opt->ip6po_rthdr->ip6r_type) {
			case IPV6_RTHDR_TYPE_0:
				if (opt->ip6po_rthdr->ip6r_segleft == 0)
					return (EINVAL);
				break;
			default:
				return (EINVAL);
			}
			break;

		default:
			return (ENOPROTOOPT);
		}
	}

	return (0);
}

/*
 * Routine called from ip6_output() to loop back a copy of an IP6 multicast
 * packet to the input queue of a specified interface.  Note that this
 * calls the output routine of the loopback "driver", but with an interface
 * pointer that might NOT be lo0ifp -- easier than replicating that code here.
 */
void
ip6_mloopback(ifp, m, dst)
	struct ifnet *ifp;
	struct mbuf *m;
	struct sockaddr_in6 *dst;
{
	struct mbuf *copym;
	struct ip6_hdr *ip6;

	copym = m_copy(m, 0, M_COPYALL);
	if (copym == NULL)
		return;

	/*
	 * Make sure to deep-copy IPv6 header portion in case the data
	 * is in an mbuf cluster, so that we can safely override the IPv6
	 * header portion later.
	 */
	if ((copym->m_flags & M_EXT) != 0 ||
	    copym->m_len < sizeof(struct ip6_hdr)) {
		copym = m_pullup(copym, sizeof(struct ip6_hdr));
		if (copym == NULL)
			return;
	}

#ifdef DIAGNOSTIC
	if (copym->m_len < sizeof(*ip6)) {
		m_freem(copym);
		return;
	}
#endif

	ip6 = mtod(copym, struct ip6_hdr *);
	if (IN6_IS_SCOPE_LINKLOCAL(&ip6->ip6_src))
		ip6->ip6_src.s6_addr16[1] = 0;
	if (IN6_IS_SCOPE_LINKLOCAL(&ip6->ip6_dst))
		ip6->ip6_dst.s6_addr16[1] = 0;

	(void)looutput(ifp, copym, (struct sockaddr *)dst, NULL);
}

/*
 * Chop IPv6 header off from the payload.
 */
static int
ip6_splithdr(m, exthdrs)
	struct mbuf *m;
	struct ip6_exthdrs *exthdrs;
{
	struct mbuf *mh;
	struct ip6_hdr *ip6;

	ip6 = mtod(m, struct ip6_hdr *);
	if (m->m_len > sizeof(*ip6)) {
		MGETHDR(mh, M_DONTWAIT, MT_HEADER);
		if (mh == 0) {
			m_freem(m);
			return ENOBUFS;
		}
		M_MOVE_PKTHDR(mh, m);
		MH_ALIGN(mh, sizeof(*ip6));
		m->m_len -= sizeof(*ip6);
		m->m_data += sizeof(*ip6);
		mh->m_next = m;
		m = mh;
		m->m_len = sizeof(*ip6);
		bcopy((caddr_t)ip6, mtod(m, caddr_t), sizeof(*ip6));
	}
	exthdrs->ip6e_ip6 = m;
	return 0;
}

/*
 * Compute IPv6 extension header length.
 */
int
ip6_optlen(inp)
	struct inpcb *inp;
{
	int len;

	if (!inp->inp_outputopts6)
		return 0;

	len = 0;
#define elen(x) \
    (((struct ip6_ext *)(x)) ? (((struct ip6_ext *)(x))->ip6e_len + 1) << 3 : 0)

	len += elen(inp->inp_outputopts6->ip6po_hbh);
	len += elen(inp->inp_outputopts6->ip6po_dest1);
	len += elen(inp->inp_outputopts6->ip6po_rthdr);
	len += elen(inp->inp_outputopts6->ip6po_dest2);
	return len;
#undef elen
}
