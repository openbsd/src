/*	$OpenBSD: nd6_rtr.c,v 1.95 2014/12/22 11:05:53 mpi Exp $	*/
/*	$KAME: nd6_rtr.c,v 1.97 2001/02/07 11:09:13 itojun Exp $	*/

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
#include <sys/systm.h>
#include <sys/timeout.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/queue.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <net/radix.h>

#include <netinet/in.h>
#include <netinet6/in6_var.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/nd6.h>
#include <netinet/icmp6.h>

#define SDL(s)	((struct sockaddr_dl *)s)

int rtpref(struct nd_defrouter *);
struct nd_defrouter *defrtrlist_update(struct nd_defrouter *);
struct in6_ifaddr *in6_ifadd(struct nd_prefix *, int);
struct nd_pfxrouter *pfxrtr_lookup(struct nd_prefix *, struct nd_defrouter *);
void pfxrtr_add(struct nd_prefix *, struct nd_defrouter *);
void pfxrtr_del(struct nd_pfxrouter *);
struct nd_pfxrouter *find_pfxlist_reachable_router(struct nd_prefix *);
void defrouter_delreq(struct nd_defrouter *);
void purge_detached(struct ifnet *);

void in6_init_address_ltimes(struct nd_prefix *, struct in6_addrlifetime *);

int rt6_deleteroute(struct radix_node *, void *, u_int);

void nd6_addr_add(void *, void *);

extern int nd6_recalc_reachtm_interval;

/*
 * Receive Router Solicitation Message - just for routers.
 * Router solicitation/advertisement is mostly managed by userland program
 * (rtadvd) so here we have no function like nd6_ra_output().
 *
 * Based on RFC 2461
 */
void
nd6_rs_input(struct mbuf *m, int off, int icmp6len)
{
	struct ifnet *ifp = m->m_pkthdr.rcvif;
	struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);
	struct nd_router_solicit *nd_rs;
	struct in6_addr saddr6 = ip6->ip6_src;
#if 0
	struct in6_addr daddr6 = ip6->ip6_dst;
#endif
	char *lladdr = NULL;
	int lladdrlen = 0;
#if 0
	struct sockaddr_dl *sdl = NULL;
	struct llinfo_nd6 *ln = NULL;
	struct rtentry *rt = NULL;
	int is_newentry;
#endif
	union nd_opts ndopts;
	char src[INET6_ADDRSTRLEN], dst[INET6_ADDRSTRLEN];

	/* If I'm not a router, ignore it. XXX - too restrictive? */
	if (!ip6_forwarding || (ifp->if_xflags & IFXF_AUTOCONF6))
		goto freeit;

	/* Sanity checks */
	if (ip6->ip6_hlim != 255) {
		nd6log((LOG_ERR,
		    "nd6_rs_input: invalid hlim (%d) from %s to %s on %s\n",
		    ip6->ip6_hlim,
		    inet_ntop(AF_INET6, &ip6->ip6_src, src, sizeof(src)),
		    inet_ntop(AF_INET6, &ip6->ip6_dst, dst, sizeof(dst)),
		    ifp->if_xname));
		goto bad;
	}

	/*
	 * Don't update the neighbor cache, if src = ::.
	 * This indicates that the src has no IP address assigned yet.
	 */
	if (IN6_IS_ADDR_UNSPECIFIED(&saddr6))
		goto freeit;

	IP6_EXTHDR_GET(nd_rs, struct nd_router_solicit *, m, off, icmp6len);
	if (nd_rs == NULL) {
		icmp6stat.icp6s_tooshort++;
		return;
	}

	icmp6len -= sizeof(*nd_rs);
	nd6_option_init(nd_rs + 1, icmp6len, &ndopts);
	if (nd6_options(&ndopts) < 0) {
		nd6log((LOG_INFO,
		    "nd6_rs_input: invalid ND option, ignored\n"));
		/* nd6_options have incremented stats */
		goto freeit;
	}

	if (ndopts.nd_opts_src_lladdr) {
		lladdr = (char *)(ndopts.nd_opts_src_lladdr + 1);
		lladdrlen = ndopts.nd_opts_src_lladdr->nd_opt_len << 3;
	}

	if (lladdr && ((ifp->if_addrlen + 2 + 7) & ~7) != lladdrlen) {
		nd6log((LOG_INFO,
		    "nd6_rs_input: lladdrlen mismatch for %s "
		    "(if %d, RS packet %d)\n",
		    inet_ntop(AF_INET6, &saddr6, src, sizeof(src)),
		    ifp->if_addrlen, lladdrlen - 2));
		goto bad;
	}

	nd6_cache_lladdr(ifp, &saddr6, lladdr, lladdrlen, ND_ROUTER_SOLICIT, 0);

 freeit:
	m_freem(m);
	return;

 bad:
	icmp6stat.icp6s_badrs++;
	m_freem(m);
}

void
nd6_rs_output(struct ifnet* ifp, struct in6_ifaddr *ia6)
{
	struct mbuf *m;
	struct ip6_hdr *ip6;
	struct nd_router_solicit *rs;
	struct ip6_moptions im6o;
	caddr_t mac;
	int icmp6len, maxlen, s;

	KASSERT(ia6 != NULL);
	KASSERT(ifp->if_flags & IFF_RUNNING);
	KASSERT(ifp->if_xflags & IFXF_AUTOCONF6);
	KASSERT(!(ia6->ia6_flags & IN6_IFF_TENTATIVE));

	maxlen = sizeof(*ip6) + sizeof(*rs);
	maxlen += (sizeof(struct nd_opt_hdr) + ifp->if_addrlen + 7) & ~7;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m && max_linkhdr + maxlen >= MHLEN) {
		MCLGET(m, M_DONTWAIT);
		if ((m->m_flags & M_EXT) == 0) {
			m_free(m);
			m = NULL;
		}
	}
	if (m == NULL)
		return;

	m->m_pkthdr.rcvif = NULL;
	m->m_pkthdr.ph_rtableid = ifp->if_rdomain;
	m->m_flags |= M_MCAST;
	m->m_pkthdr.csum_flags |= M_ICMP_CSUM_OUT;

	im6o.im6o_ifidx = ifp->if_index;
	im6o.im6o_hlim = 255;
	im6o.im6o_loop = 0;

	icmp6len = sizeof(*rs);
	m->m_pkthdr.len = m->m_len = sizeof(*ip6) + icmp6len;
	m->m_data += max_linkhdr;	/* or MH_ALIGN() equivalent? */

	/* fill neighbor solicitation packet */
	ip6 = mtod(m, struct ip6_hdr *);
	ip6->ip6_flow = 0;
	ip6->ip6_vfc &= ~IPV6_VERSION_MASK;
	ip6->ip6_vfc |= IPV6_VERSION;
	/* ip6->ip6_plen will be set later */
	ip6->ip6_nxt = IPPROTO_ICMPV6;
	ip6->ip6_hlim = 255;
	
	ip6->ip6_dst = in6addr_linklocal_allrouters;

	ip6->ip6_src = ia6->ia_addr.sin6_addr;

	rs = (struct nd_router_solicit *)(ip6 + 1);
	rs->nd_rs_type = ND_ROUTER_SOLICIT;
	rs->nd_rs_code = 0;
	rs->nd_rs_cksum = 0;
	rs->nd_rs_reserved = 0;

	if ((mac = nd6_ifptomac(ifp))) {
		int optlen = sizeof(struct nd_opt_hdr) + ifp->if_addrlen;
		struct nd_opt_hdr *nd_opt = (struct nd_opt_hdr *)(rs + 1);
		/* 8 byte alignments... */
		optlen = (optlen + 7) & ~7;

		m->m_pkthdr.len += optlen;
		m->m_len += optlen;
		icmp6len += optlen;
		bzero((caddr_t)nd_opt, optlen);
		nd_opt->nd_opt_type = ND_OPT_SOURCE_LINKADDR;
		nd_opt->nd_opt_len = optlen >> 3;
		bcopy(mac, (caddr_t)(nd_opt + 1), ifp->if_addrlen);
	}

	ip6->ip6_plen = htons((u_short)icmp6len);

	s = splsoftnet();
	ip6_output(m, NULL, NULL, 0, &im6o, NULL, NULL);
	splx(s);

	icmp6_ifstat_inc(ifp, ifs6_out_msg);
	icmp6_ifstat_inc(ifp, ifs6_out_routersolicit);
	icmp6stat.icp6s_outhist[ND_ROUTER_SOLICIT]++;
}

void
nd6_rs_dev_state(void *arg)
{
	struct ifnet *ifp;

	ifp = (struct ifnet *) arg;

	if (LINK_STATE_IS_UP(ifp->if_link_state) &&
	    ifp->if_flags & IFF_RUNNING)
		/* start quick timer, will exponentially back off */
		nd6_rs_output_set_timo(ND6_RS_OUTPUT_QUICK_INTERVAL);
}

/*
 * Receive Router Advertisement Message.
 *
 * Based on RFC 2461
 * TODO: on-link bit on prefix information
 * TODO: ND_RA_FLAG_{OTHER,MANAGED} processing
 */
void
nd6_ra_input(struct mbuf *m, int off, int icmp6len)
{
	struct ifnet *ifp = m->m_pkthdr.rcvif;
	struct nd_ifinfo *ndi = ND_IFINFO(ifp);
	struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);
	struct nd_router_advert *nd_ra;
	struct in6_addr saddr6 = ip6->ip6_src;
#if 0
	struct in6_addr daddr6 = ip6->ip6_dst;
	int flags; /* = nd_ra->nd_ra_flags_reserved; */
	int is_managed = ((flags & ND_RA_FLAG_MANAGED) != 0);
	int is_other = ((flags & ND_RA_FLAG_OTHER) != 0);
#endif
	union nd_opts ndopts;
	struct nd_defrouter *dr;
	char src[INET6_ADDRSTRLEN], dst[INET6_ADDRSTRLEN];

	/* We accept RAs only if inet6 autoconf is enabled  */
	if (!(ifp->if_xflags & IFXF_AUTOCONF6))
		goto freeit;
	if (!(ndi->flags & ND6_IFF_ACCEPT_RTADV))
		goto freeit;

	if (nd6_rs_output_timeout != ND6_RS_OUTPUT_INTERVAL)
		/* we saw a RA, stop quick timer */
		nd6_rs_output_set_timo(ND6_RS_OUTPUT_INTERVAL);

	if (ip6->ip6_hlim != 255) {
		nd6log((LOG_ERR,
		    "nd6_ra_input: invalid hlim (%d) from %s to %s on %s\n",
		    ip6->ip6_hlim,
		    inet_ntop(AF_INET6, &ip6->ip6_src, src, sizeof(src)),
		    inet_ntop(AF_INET6, &ip6->ip6_dst, dst, sizeof(dst)),
		    ifp->if_xname));
		goto bad;
	}

	if (!IN6_IS_ADDR_LINKLOCAL(&saddr6)) {
		nd6log((LOG_ERR,
		    "nd6_ra_input: src %s is not link-local\n",
		    inet_ntop(AF_INET6, &saddr6, src, sizeof(src))));
		goto bad;
	}

	IP6_EXTHDR_GET(nd_ra, struct nd_router_advert *, m, off, icmp6len);
	if (nd_ra == NULL) {
		icmp6stat.icp6s_tooshort++;
		return;
	}

	icmp6len -= sizeof(*nd_ra);
	nd6_option_init(nd_ra + 1, icmp6len, &ndopts);
	if (nd6_options(&ndopts) < 0) {
		nd6log((LOG_INFO,
		    "nd6_ra_input: invalid ND option, ignored\n"));
		/* nd6_options have incremented stats */
		goto freeit;
	}

    {
	struct nd_defrouter dr0;
	u_int32_t advreachable = nd_ra->nd_ra_reachable;

	memset(&dr0, 0, sizeof(dr0));
	dr0.rtaddr = saddr6;
	dr0.flags  = nd_ra->nd_ra_flags_reserved;
	dr0.rtlifetime = ntohs(nd_ra->nd_ra_router_lifetime);
	dr0.expire = time_second + dr0.rtlifetime;
	dr0.ifp = ifp;
	/* unspecified or not? (RFC 2461 6.3.4) */
	if (advreachable) {
		NTOHL(advreachable);
		if (advreachable <= MAX_REACHABLE_TIME &&
		    ndi->basereachable != advreachable) {
			ndi->basereachable = advreachable;
			ndi->reachable = ND_COMPUTE_RTIME(ndi->basereachable);
			ndi->recalctm = nd6_recalc_reachtm_interval; /* reset */
		}
	}
	if (nd_ra->nd_ra_retransmit)
		ndi->retrans = ntohl(nd_ra->nd_ra_retransmit);
	if (nd_ra->nd_ra_curhoplimit)
		ndi->chlim = nd_ra->nd_ra_curhoplimit;
	dr = defrtrlist_update(&dr0);
    }

	/*
	 * prefix
	 */
	if (ndopts.nd_opts_pi) {
		struct nd_opt_hdr *pt;
		struct nd_opt_prefix_info *pi = NULL;
		struct nd_prefix pr;

		for (pt = (struct nd_opt_hdr *)ndopts.nd_opts_pi;
		     pt <= (struct nd_opt_hdr *)ndopts.nd_opts_pi_end;
		     pt = (struct nd_opt_hdr *)((caddr_t)pt +
						(pt->nd_opt_len << 3))) {
			if (pt->nd_opt_type != ND_OPT_PREFIX_INFORMATION)
				continue;
			pi = (struct nd_opt_prefix_info *)pt;

			if (pi->nd_opt_pi_len != 4) {
				nd6log((LOG_INFO,
				    "nd6_ra_input: invalid option "
				    "len %d for prefix information option, "
				    "ignored\n", pi->nd_opt_pi_len));
				continue;
			}

			if (128 < pi->nd_opt_pi_prefix_len) {
				nd6log((LOG_INFO,
				    "nd6_ra_input: invalid prefix "
				    "len %d for prefix information option, "
				    "ignored\n", pi->nd_opt_pi_prefix_len));
				continue;
			}

			if (IN6_IS_ADDR_MULTICAST(&pi->nd_opt_pi_prefix)
			 || IN6_IS_ADDR_LINKLOCAL(&pi->nd_opt_pi_prefix)) {
				nd6log((LOG_INFO,
				    "nd6_ra_input: invalid prefix "
				    "%s, ignored\n",
				    inet_ntop(AF_INET6, &pi->nd_opt_pi_prefix,
					src, sizeof(src))));
				continue;
			}

			/* aggregatable unicast address, rfc2374 */
			if ((pi->nd_opt_pi_prefix.s6_addr8[0] & 0xe0) == 0x20
			 && pi->nd_opt_pi_prefix_len != 64) {
				nd6log((LOG_INFO,
				    "nd6_ra_input: invalid prefixlen "
				    "%d for rfc2374 prefix %s, ignored\n",
				    pi->nd_opt_pi_prefix_len,
				    inet_ntop(AF_INET6, &pi->nd_opt_pi_prefix,
					src, sizeof(src))));
				continue;
			}

			bzero(&pr, sizeof(pr));
			pr.ndpr_prefix.sin6_family = AF_INET6;
			pr.ndpr_prefix.sin6_len = sizeof(pr.ndpr_prefix);
			pr.ndpr_prefix.sin6_addr = pi->nd_opt_pi_prefix;
			pr.ndpr_ifp = (struct ifnet *)m->m_pkthdr.rcvif;

			pr.ndpr_raf_onlink = (pi->nd_opt_pi_flags_reserved &
			     ND_OPT_PI_FLAG_ONLINK) ? 1 : 0;
			pr.ndpr_raf_auto = (pi->nd_opt_pi_flags_reserved &
			     ND_OPT_PI_FLAG_AUTO) ? 1 : 0;
			pr.ndpr_plen = pi->nd_opt_pi_prefix_len;
			pr.ndpr_vltime = ntohl(pi->nd_opt_pi_valid_time);
			pr.ndpr_pltime = ntohl(pi->nd_opt_pi_preferred_time);
			pr.ndpr_lastupdate = time_second;

			if (in6_init_prefix_ltimes(&pr))
				continue; /* prefix lifetime init failed */

			(void)prelist_update(&pr, dr, m);
		}
	}

	/*
	 * MTU
	 */
	if (ndopts.nd_opts_mtu && ndopts.nd_opts_mtu->nd_opt_mtu_len == 1) {
		u_long mtu;
		u_long maxmtu;

		mtu = ntohl(ndopts.nd_opts_mtu->nd_opt_mtu_mtu);

		/* lower bound */
		if (mtu < IPV6_MMTU) {
			nd6log((LOG_INFO, "nd6_ra_input: bogus mtu option "
			    "mtu=%lu sent from %s, ignoring\n",
			    mtu,
			    inet_ntop(AF_INET6, &ip6->ip6_src,
				src, sizeof(src))));
			goto skip;
		}

		/* upper bound */
		maxmtu = (ndi->maxmtu && ndi->maxmtu < ifp->if_mtu)
		    ? ndi->maxmtu : ifp->if_mtu;
		if (mtu <= maxmtu) {
			ndi->linkmtu = mtu;
		} else {
			nd6log((LOG_INFO, "nd6_ra_input: bogus mtu "
			    "mtu=%lu sent from %s; "
			    "exceeds maxmtu %lu, ignoring\n",
			    mtu,
			    inet_ntop(AF_INET6, &ip6->ip6_src,
				src, sizeof(src)),
			    maxmtu));
		}
	}

 skip:

	/*
	 * Source link layer address
	 */
    {
	char *lladdr = NULL;
	int lladdrlen = 0;

	if (ndopts.nd_opts_src_lladdr) {
		lladdr = (char *)(ndopts.nd_opts_src_lladdr + 1);
		lladdrlen = ndopts.nd_opts_src_lladdr->nd_opt_len << 3;
	}

	if (lladdr && ((ifp->if_addrlen + 2 + 7) & ~7) != lladdrlen) {
		nd6log((LOG_INFO,
		    "nd6_ra_input: lladdrlen mismatch for %s "
		    "(if %d, RA packet %d)\n",
		    inet_ntop(AF_INET6, &saddr6, src, sizeof(src)),
		    ifp->if_addrlen, lladdrlen - 2));
		goto bad;
	}

	nd6_cache_lladdr(ifp, &saddr6, lladdr, lladdrlen, ND_ROUTER_ADVERT, 0);

	/*
	 * Installing a link-layer address might change the state of the
	 * router's neighbor cache, which might also affect our on-link
	 * detection of adveritsed prefixes.
	 */
	pfxlist_onlink_check();
    }

 freeit:
	m_freem(m);
	return;

 bad:
	icmp6stat.icp6s_badra++;
	m_freem(m);
}

/*
 * default router list processing sub routines
 */
void
defrouter_addreq(struct nd_defrouter *new)
{
	struct rt_addrinfo info;
	struct sockaddr_in6 def, mask, gate;
	struct rtentry *newrt = NULL;
	int s;
	int error;

	memset(&def, 0, sizeof(def));
	memset(&mask, 0, sizeof(mask));
	memset(&gate, 0, sizeof(gate)); /* for safety */
	memset(&info, 0, sizeof(info));

	def.sin6_len = mask.sin6_len = gate.sin6_len =
	    sizeof(struct sockaddr_in6);
	def.sin6_family = mask.sin6_family = gate.sin6_family = AF_INET6;
	gate.sin6_addr = new->rtaddr;
	gate.sin6_scope_id = 0;	/* XXX */

	info.rti_flags = RTF_GATEWAY;
	info.rti_info[RTAX_DST] = sin6tosa(&def);
	info.rti_info[RTAX_GATEWAY] = sin6tosa(&gate);
	info.rti_info[RTAX_NETMASK] = sin6tosa(&mask);

	s = splsoftnet();
	error = rtrequest1(RTM_ADD, &info, RTP_DEFAULT, &newrt,
	    new->ifp->if_rdomain);
	if (newrt) {
		rt_sendmsg(newrt, RTM_ADD, new->ifp->if_rdomain);
		newrt->rt_refcnt--;
	}
	if (error == 0)
		new->installed = 1;
	splx(s);
	return;
}

struct nd_defrouter *
defrouter_lookup(struct in6_addr *addr, struct ifnet *ifp)
{
	struct nd_defrouter *dr;

	TAILQ_FOREACH(dr, &nd_defrouter, dr_entry)
		if (dr->ifp == ifp && IN6_ARE_ADDR_EQUAL(addr, &dr->rtaddr))
			return (dr);

	return (NULL);		/* search failed */
}

void
defrtrlist_del(struct nd_defrouter *dr)
{
	struct nd_defrouter *deldr = NULL;
	struct in6_ifextra *ext = dr->ifp->if_afdata[AF_INET6];
	struct nd_prefix *pr;

	/*
	 * Flush all the routing table entries that use the router
	 * as a next hop.
	 */
	/* XXX: better condition? */
	if (!ip6_forwarding && (dr->ifp->if_xflags & IFXF_AUTOCONF6))
		rt6_flush(&dr->rtaddr, dr->ifp);

	if (dr->installed) {
		deldr = dr;
		defrouter_delreq(dr);
	}
	TAILQ_REMOVE(&nd_defrouter, dr, dr_entry);

	/*
	 * Also delete all the pointers to the router in each prefix lists.
	 */
	LIST_FOREACH(pr, &nd_prefix, ndpr_entry) {
		struct nd_pfxrouter *pfxrtr;
		if ((pfxrtr = pfxrtr_lookup(pr, dr)) != NULL)
			pfxrtr_del(pfxrtr);
	}
	pfxlist_onlink_check();

	/*
	 * If the router is the primary one, choose a new one.
	 * Note that defrouter_select() will remove the current gateway
	 * from the routing table.
	 */
	if (deldr)
		defrouter_select();

	ext->ndefrouters--;
	if (ext->ndefrouters < 0) {
		log(LOG_WARNING, "defrtrlist_del: negative count on %s\n",
		    dr->ifp->if_xname);
	}

	free(dr, M_IP6NDP, 0);
}

/*
 * Remove the default route for a given router.
 * This is just a subroutine function for defrouter_select(), and should
 * not be called from anywhere else.
 */
void
defrouter_delreq(struct nd_defrouter *dr)
{
	struct rt_addrinfo info;
	struct sockaddr_in6 def, mask, gw;
	struct rtentry *oldrt = NULL;

#ifdef DIAGNOSTIC
	if (!dr)
		panic("dr == NULL in defrouter_delreq");
#endif

	memset(&info, 0, sizeof(info));
	memset(&def, 0, sizeof(def));
	memset(&mask, 0, sizeof(mask));
	memset(&gw, 0, sizeof(gw));	/* for safety */

	def.sin6_len = mask.sin6_len = gw.sin6_len =
	    sizeof(struct sockaddr_in6);
	def.sin6_family = mask.sin6_family = gw.sin6_family = AF_INET6;
	gw.sin6_addr = dr->rtaddr;
	gw.sin6_scope_id = 0;	/* XXX */

	info.rti_flags = RTF_GATEWAY;
	info.rti_info[RTAX_DST] = sin6tosa(&def);
	info.rti_info[RTAX_GATEWAY] = sin6tosa(&gw);
	info.rti_info[RTAX_NETMASK] = sin6tosa(&mask);

	rtrequest1(RTM_DELETE, &info, RTP_DEFAULT, &oldrt,
	    dr->ifp->if_rdomain);
	if (oldrt) {
		rt_sendmsg(oldrt, RTM_DELETE, dr->ifp->if_rdomain);
		if (oldrt->rt_refcnt <= 0) {
			/*
			 * XXX: borrowed from the RTM_DELETE case of
			 * rtrequest1().
			 */
			oldrt->rt_refcnt++;
			rtfree(oldrt);
		}
	}

	dr->installed = 0;
}

/*
 * remove all default routes from default router list
 */
void
defrouter_reset(void)
{
	struct nd_defrouter *dr;

	TAILQ_FOREACH(dr, &nd_defrouter, dr_entry)
		defrouter_delreq(dr);

	/*
	 * XXX should we also nuke any default routers in the kernel, by
	 * going through them by rtalloc()?
	 */
}

/*
 * Default Router Selection according to Section 6.3.6 of RFC 2461 and
 * draft-ietf-ipngwg-router-selection:
 * 1) Routers that are reachable or probably reachable should be preferred.
 *    If we have more than one (probably) reachable router, prefer ones
 *    with the highest router preference.
 * 2) When no routers on the list are known to be reachable or
 *    probably reachable, routers SHOULD be selected in a round-robin
 *    fashion, regardless of router preference values.
 * 3) If the Default Router List is empty, assume that all
 *    destinations are on-link.
 *
 * We assume nd_defrouter is sorted by router preference value.
 * Since the code below covers both with and without router preference cases,
 * we do not need to classify the cases by ifdef.
 *
 * At this moment, we do not try to install more than one default router,
 * even when the multipath routing is available, because we're not sure about
 * the benefits for stub hosts comparing to the risk of making the code
 * complicated and the possibility of introducing bugs.
 */
void
defrouter_select(void)
{
	int s = splsoftnet();
	struct nd_defrouter *dr, *selected_dr = NULL, *installed_dr = NULL;
	struct rtentry *rt = NULL;
	struct llinfo_nd6 *ln = NULL;

	/*
	 * This function should be called only when acting as an autoconfigured
	 * host.  Although the remaining part of this function is not effective
	 * if the node is not an autoconfigured host, we explicitly exclude
	 * such cases here for safety.
	 */
	/* XXX too strict? */
	if (ip6_forwarding) {
		nd6log((LOG_WARNING,
		    "defrouter_select: called unexpectedly (forwarding=%d)\n",
		    ip6_forwarding));
		splx(s);
		return;
	}

	/*
	 * Let's handle easy case (3) first:
	 * If default router list is empty, there's nothing to be done.
	 */
	if (TAILQ_EMPTY(&nd_defrouter)) {
		splx(s);
		return;
	}

	/*
	 * Search for a (probably) reachable router from the list.
	 * We just pick up the first reachable one (if any), assuming that
	 * the ordering rule of the list described in defrtrlist_update().
	 */
	TAILQ_FOREACH(dr, &nd_defrouter, dr_entry) {
		if (!(dr->ifp->if_xflags & IFXF_AUTOCONF6))
			continue;
		if (!selected_dr &&
		    (rt = nd6_lookup(&dr->rtaddr, 0, dr->ifp,
		     dr->ifp->if_rdomain)) &&
		    (ln = (struct llinfo_nd6 *)rt->rt_llinfo) &&
		    ND6_IS_LLINFO_PROBREACH(ln)) {
			selected_dr = dr;
		}

		if (dr->installed && !installed_dr)
			installed_dr = dr;
		else if (dr->installed && installed_dr) {
			/* this should not happen.  warn for diagnosis. */
			log(LOG_ERR, "defrouter_select: more than one router"
			    " is installed\n");
		}
	}
	/*
	 * If none of the default routers was found to be reachable,
	 * round-robin the list regardless of preference.
	 * Otherwise, if we have an installed router, check if the selected
	 * (reachable) router should really be preferred to the installed one.
	 * We only prefer the new router when the old one is not reachable
	 * or when the new one has a really higher preference value.
	 */
	if (!selected_dr) {
		if (!installed_dr || !TAILQ_NEXT(installed_dr, dr_entry))
			selected_dr = TAILQ_FIRST(&nd_defrouter);
		else
			selected_dr = TAILQ_NEXT(installed_dr, dr_entry);
	} else if (installed_dr &&
	    (rt = nd6_lookup(&installed_dr->rtaddr, 0, installed_dr->ifp,
	     installed_dr->ifp->if_rdomain)) &&
	    (ln = (struct llinfo_nd6 *)rt->rt_llinfo) &&
	    ND6_IS_LLINFO_PROBREACH(ln) &&
	    rtpref(selected_dr) <= rtpref(installed_dr)) {
		selected_dr = installed_dr;
	}

	/*
	 * If the selected router is different than the installed one,
	 * remove the installed router and install the selected one.
	 * Note that the selected router is never NULL here.
	 */
	if (installed_dr != selected_dr) {
		if (installed_dr)
			defrouter_delreq(installed_dr);
		defrouter_addreq(selected_dr);
	}

	splx(s);
	return;
}

/*
 * for default router selection
 * regards router-preference field as a 2-bit signed integer
 */
int
rtpref(struct nd_defrouter *dr)
{
#ifdef RTPREF
	switch (dr->flags & ND_RA_FLAG_RTPREF_MASK) {
	case ND_RA_FLAG_RTPREF_HIGH:
		return RTPREF_HIGH;
	case ND_RA_FLAG_RTPREF_MEDIUM:
	case ND_RA_FLAG_RTPREF_RSV:
		return RTPREF_MEDIUM;
	case ND_RA_FLAG_RTPREF_LOW:
		return RTPREF_LOW;
	default:
		/*
		 * This case should never happen.  If it did, it would mean a
		 * serious bug of kernel internal.  We thus always bark here.
		 * Or, can we even panic?
		 */
		log(LOG_ERR, "rtpref: impossible RA flag %x", dr->flags);
		return RTPREF_INVALID;
	}
	/* NOTREACHED */
#else
	return 0;
#endif
}

struct nd_defrouter *
defrtrlist_update(struct nd_defrouter *new)
{
	struct nd_defrouter *dr, *n;
	struct in6_ifextra *ext = new->ifp->if_afdata[AF_INET6];
	int s = splsoftnet();

	if ((dr = defrouter_lookup(&new->rtaddr, new->ifp)) != NULL) {
		/* entry exists */
		if (new->rtlifetime == 0) {
			defrtrlist_del(dr);
			dr = NULL;
		} else {
			int oldpref = rtpref(dr);

			/* override */
			dr->flags = new->flags; /* xxx flag check */
			dr->rtlifetime = new->rtlifetime;
			dr->expire = new->expire;

			if (!dr->installed)
				defrouter_select();

			/*
			 * If the preference does not change, there's no need
			 * to sort the entries.
			 */
			if (rtpref(new) == oldpref) {
				splx(s);
				return (dr);
			}

			/*
			 * preferred router may be changed, so relocate
			 * this router.
			 * XXX: calling TAILQ_REMOVE directly is a bad manner.
			 * However, since defrtrlist_del() has many side
			 * effects, we intentionally do so here.
			 * defrouter_select() below will handle routing
			 * changes later.
			 */
			TAILQ_REMOVE(&nd_defrouter, dr, dr_entry);
			n = dr;
			goto insert;
		}
		splx(s);
		return (dr);
	}

	/* entry does not exist */
	if (new->rtlifetime == 0) {
		/* flush all possible redirects */
		if (!ip6_forwarding && (new->ifp->if_xflags & IFXF_AUTOCONF6))
			rt6_flush(&new->rtaddr, new->ifp);
		splx(s);
		return (NULL);
	}

	if (ip6_maxifdefrouters >= 0 &&
	    ext->ndefrouters >= ip6_maxifdefrouters) {
		splx(s);
		return (NULL);
	}

	n = malloc(sizeof(*n), M_IP6NDP, M_NOWAIT | M_ZERO);
	if (n == NULL) {
		splx(s);
		return (NULL);
	}
	*n = *new;

insert:
	/*
	 * Insert the new router in the Default Router List;
	 * The Default Router List should be in the descending order
	 * of router-preference.  Routers with the same preference are
	 * sorted in the arriving time order.
	 */

	/* insert at the end of the group */
	TAILQ_FOREACH(dr, &nd_defrouter, dr_entry)
		if (rtpref(n) > rtpref(dr))
			break;
	if (dr)
		TAILQ_INSERT_BEFORE(dr, n, dr_entry);
	else
		TAILQ_INSERT_TAIL(&nd_defrouter, n, dr_entry);

	defrouter_select();

	ext->ndefrouters++;

	splx(s);

	return (n);
}

struct nd_pfxrouter *
pfxrtr_lookup(struct nd_prefix *pr, struct nd_defrouter *dr)
{
	struct nd_pfxrouter *search;

	LIST_FOREACH(search, &pr->ndpr_advrtrs, pfr_entry) {
		if (search->router == dr)
			break;
	}

	return (search);
}

void
pfxrtr_add(struct nd_prefix *pr, struct nd_defrouter *dr)
{
	struct nd_pfxrouter *new;

	new = malloc(sizeof(*new), M_IP6NDP, M_NOWAIT | M_ZERO);
	if (new == NULL)
		return;
	new->router = dr;

	LIST_INSERT_HEAD(&pr->ndpr_advrtrs, new, pfr_entry);

	pfxlist_onlink_check();
}

void
pfxrtr_del(struct nd_pfxrouter *pfr)
{
	LIST_REMOVE(pfr, pfr_entry);
	free(pfr, M_IP6NDP, 0);
}

struct nd_prefix *
nd6_prefix_lookup(struct nd_prefix *pr)
{
	struct nd_prefix *search;

	LIST_FOREACH(search, &nd_prefix, ndpr_entry) {
		if (pr->ndpr_ifp == search->ndpr_ifp &&
		    pr->ndpr_plen == search->ndpr_plen &&
		    in6_are_prefix_equal(&pr->ndpr_prefix.sin6_addr,
		    &search->ndpr_prefix.sin6_addr, pr->ndpr_plen)) {
			break;
		}
	}

	return (search);
}

void
purge_detached(struct ifnet *ifp)
{
	struct nd_prefix *pr, *pr_next;
	struct in6_ifaddr *ia6;
	struct ifaddr *ifa, *ifa_next;

	splsoftassert(IPL_SOFTNET);

	LIST_FOREACH_SAFE(pr, &nd_prefix, ndpr_entry, pr_next) {
		/*
		 * This function is called when we need to make more room for
		 * new prefixes rather than keeping old, possibly stale ones.
		 * Detached prefixes would be a good candidate; if all routers
		 * that advertised the prefix expired, the prefix is also
		 * probably stale.
		 */
		if (pr->ndpr_ifp != ifp ||
		    IN6_IS_ADDR_LINKLOCAL(&pr->ndpr_prefix.sin6_addr) ||
		    ((pr->ndpr_stateflags & NDPRF_DETACHED) == 0 &&
		    !LIST_EMPTY(&pr->ndpr_advrtrs)))
			continue;

		TAILQ_FOREACH_SAFE(ifa, &ifp->if_addrlist, ifa_list, ifa_next) {
			if (ifa->ifa_addr->sa_family != AF_INET6)
				continue;
			ia6 = ifatoia6(ifa);
			if ((ia6->ia6_flags & IN6_IFF_AUTOCONF) ==
			    IN6_IFF_AUTOCONF && ia6->ia6_ndpr == pr) {
				in6_purgeaddr(ifa);
			}
		}
	}
}

int
nd6_prelist_add(struct nd_prefix *pr, struct nd_defrouter *dr, 
    struct nd_prefix **newp)
{
	struct nd_prefix *new = NULL;
	int i, s;
	struct in6_ifextra *ext = pr->ndpr_ifp->if_afdata[AF_INET6];

	if (ip6_maxifprefixes >= 0) {
		if (ext->nprefixes >= ip6_maxifprefixes / 2) {
			s = splsoftnet();
			purge_detached(pr->ndpr_ifp);
			splx(s);
		}
		if (ext->nprefixes >= ip6_maxifprefixes)
			return(ENOMEM);
	}

	new = malloc(sizeof(*new), M_IP6NDP, M_NOWAIT | M_ZERO);
	if (new == NULL)
		return ENOMEM;
	*new = *pr;
	if (newp != NULL)
		*newp = new;

	/* initialization */
	LIST_INIT(&new->ndpr_advrtrs);
	in6_prefixlen2mask(&new->ndpr_mask, new->ndpr_plen);
	/* make prefix in the canonical form */
	for (i = 0; i < 4; i++)
		new->ndpr_prefix.sin6_addr.s6_addr32[i] &=
		    new->ndpr_mask.s6_addr32[i];

	task_set(&new->ndpr_task, nd6_addr_add, new, NULL);

	s = splsoftnet();
	/* link ndpr_entry to nd_prefix list */
	LIST_INSERT_HEAD(&nd_prefix, new, ndpr_entry);
	splx(s);

	/* ND_OPT_PI_FLAG_ONLINK processing */
	if (new->ndpr_raf_onlink) {
		char addr[INET6_ADDRSTRLEN];
		int e;

		if ((e = nd6_prefix_onlink(new)) != 0) {
			nd6log((LOG_ERR, "nd6_prelist_add: failed to make "
			    "the prefix %s/%d on-link on %s (errno=%d)\n",
			    inet_ntop(AF_INET6, &pr->ndpr_prefix.sin6_addr,
				addr, sizeof(addr)),
			    pr->ndpr_plen, pr->ndpr_ifp->if_xname, e));
			/* proceed anyway. XXX: is it correct? */
		}
	}

	if (dr)
		pfxrtr_add(new, dr);

	ext->nprefixes++;

	return 0;
}

void
prelist_remove(struct nd_prefix *pr)
{
	struct nd_pfxrouter *pfr, *next;
	int e, s;
	struct in6_ifextra *ext = pr->ndpr_ifp->if_afdata[AF_INET6];

	/* make sure to invalidate the prefix until it is really freed. */
	pr->ndpr_vltime = 0;
	pr->ndpr_pltime = 0;
#if 0
	/*
	 * Though these flags are now meaningless, we'd rather keep the value
	 * not to confuse users when executing "ndp -p".
	 */
	pr->ndpr_raf_onlink = 0;
	pr->ndpr_raf_auto = 0;
#endif
	if ((pr->ndpr_stateflags & NDPRF_ONLINK) != 0 &&
	    (e = nd6_prefix_offlink(pr)) != 0) {
		char addr[INET6_ADDRSTRLEN];
		nd6log((LOG_ERR, "prelist_remove: failed to make %s/%d offlink "
		    "on %s, errno=%d\n",
		    inet_ntop(AF_INET6, &pr->ndpr_prefix.sin6_addr,
			addr, sizeof(addr)),
		    pr->ndpr_plen, pr->ndpr_ifp->if_xname, e));
		/* what should we do? */
	}

	if (pr->ndpr_refcnt > 0)
		return;		/* notice here? */

	s = splsoftnet();

	/* unlink ndpr_entry from nd_prefix list */
	LIST_REMOVE(pr, ndpr_entry);

	/* free list of routers that adversed the prefix */
	LIST_FOREACH_SAFE(pfr, &pr->ndpr_advrtrs, pfr_entry, next)
		free(pfr, M_IP6NDP, 0);

	ext->nprefixes--;
	if (ext->nprefixes < 0) {
		log(LOG_WARNING, "prelist_remove: negative count on %s\n",
		    pr->ndpr_ifp->if_xname);
	}

	free(pr, M_IP6NDP, 0);

	pfxlist_onlink_check();
	splx(s);
}

/*
 * dr - may be NULL
 */

int
prelist_update(struct nd_prefix *new, struct nd_defrouter *dr, struct mbuf *m)
{
	struct in6_ifaddr *ia6_match = NULL;
	struct ifaddr *ifa;
	struct ifnet *ifp = new->ndpr_ifp;
	struct nd_prefix *pr;
	int s = splsoftnet();
	int error = 0;
	int tempaddr_preferred = 0, autoconf = 0, statique = 0;
	int auth;
	struct in6_addrlifetime lt6_tmp;
	char addr[INET6_ADDRSTRLEN];

	auth = 0;
	if (m) {
		/*
		 * Authenticity for NA consists authentication for
		 * both IP header and IP datagrams, doesn't it ?
		 */
		auth = (m->m_flags & M_AUTH);
	}

	if ((pr = nd6_prefix_lookup(new)) != NULL) {
		/*
		 * nd6_prefix_lookup() ensures that pr and new have the same
		 * prefix on a same interface.
		 */

		/*
		 * Update prefix information.  Note that the on-link (L) bit
		 * and the autonomous (A) bit should NOT be changed from 1
		 * to 0.
		 */
		if (new->ndpr_raf_onlink == 1)
			pr->ndpr_raf_onlink = 1;
		if (new->ndpr_raf_auto == 1)
			pr->ndpr_raf_auto = 1;
		if (new->ndpr_raf_onlink) {
			pr->ndpr_vltime = new->ndpr_vltime;
			pr->ndpr_pltime = new->ndpr_pltime;
			pr->ndpr_preferred = new->ndpr_preferred;
			pr->ndpr_expire = new->ndpr_expire;
			pr->ndpr_lastupdate = new->ndpr_lastupdate;
		}

		if (new->ndpr_raf_onlink &&
		    (pr->ndpr_stateflags & NDPRF_ONLINK) == 0) {
			int e;

			if ((e = nd6_prefix_onlink(pr)) != 0) {
				nd6log((LOG_ERR,
				    "prelist_update: failed to make "
				    "the prefix %s/%d on-link on %s "
				    "(errno=%d)\n",
				    inet_ntop(AF_INET6,
					&pr->ndpr_prefix.sin6_addr,
					addr, sizeof(addr)),
				    pr->ndpr_plen, pr->ndpr_ifp->if_xname, e));
				/* proceed anyway. XXX: is it correct? */
			}
		}

		if (dr && pfxrtr_lookup(pr, dr) == NULL)
			pfxrtr_add(pr, dr);
	} else {
		struct nd_prefix *newpr = NULL;

		if (new->ndpr_vltime == 0)
			goto end;
		if (new->ndpr_raf_onlink == 0 && new->ndpr_raf_auto == 0)
			goto end;

		error = nd6_prelist_add(new, dr, &newpr);
		if (error != 0 || newpr == NULL) {
			nd6log((LOG_NOTICE, "prelist_update: "
			    "nd6_prelist_add failed for %s/%d on %s "
			    "errno=%d, returnpr=%p\n",
			    inet_ntop(AF_INET6, &new->ndpr_prefix.sin6_addr,
				addr, sizeof(addr)),
			    new->ndpr_plen, new->ndpr_ifp->if_xname,
			    error, newpr));
			goto end; /* we should just give up in this case. */
		}

		/*
		 * XXX: from the ND point of view, we can ignore a prefix
		 * with the on-link bit being zero.  However, we need a
		 * prefix structure for references from autoconfigured
		 * addresses.  Thus, we explicitly make sure that the prefix
		 * itself expires now.
		 */
		if (newpr->ndpr_raf_onlink == 0) {
			newpr->ndpr_vltime = 0;
			newpr->ndpr_pltime = 0;
			in6_init_prefix_ltimes(newpr);
		}

		pr = newpr;
	}

	/*
	 * Address autoconfiguration based on Section 5.5.3 of RFC 2462.
	 * Note that pr must be non NULL at this point.
	 */

	/* 5.5.3 (a). Ignore the prefix without the A bit set. */
	if (!new->ndpr_raf_auto)
		goto end;

	/*
	 * 5.5.3 (b). the link-local prefix should have been ignored in
	 * nd6_ra_input.
	 */

	/*
	 * 5.5.3 (c). Consistency check on lifetimes: pltime <= vltime.
	 * This should have been done in nd6_ra_input.
	 */

	/*
	 * 5.5.3 (d). If the prefix advertised does not match the prefix of an
	 * address already in the list, and the Valid Lifetime is not 0,
	 * form an address.  Note that even a manually configured address
	 * should reject autoconfiguration of a new address.
	 */
	TAILQ_FOREACH(ifa, &ifp->if_addrlist, ifa_list) {
		struct in6_ifaddr *ia6;
		int ifa_plen;
		u_int32_t storedlifetime;

		if (ifa->ifa_addr->sa_family != AF_INET6)
			continue;

		ia6 = ifatoia6(ifa);

		/*
		 * Spec is not clear here, but I believe we should concentrate
		 * on unicast (i.e. not anycast) addresses.
		 * XXX: other ia6_flags? detached or duplicated?
		 */
		if ((ia6->ia6_flags & IN6_IFF_ANYCAST) != 0)
			continue;

		ifa_plen = in6_mask2len(&ia6->ia_prefixmask.sin6_addr, NULL);
		if (ifa_plen != new->ndpr_plen ||
		    !in6_are_prefix_equal(&ia6->ia_addr.sin6_addr,
		    &new->ndpr_prefix.sin6_addr, ifa_plen))
			continue;

		if (ia6_match == NULL) /* remember the first one */
			ia6_match = ia6;

		if ((ia6->ia6_flags & IN6_IFF_AUTOCONF) == 0) {
			statique = 1;
			continue;
		}

		/*
		 * An already autoconfigured address matched.  Now that we
		 * are sure there is at least one matched address, we can
		 * proceed to 5.5.3. (e): update the lifetimes according to the
		 * "two hours" rule and the privacy extension.
		 */
#define TWOHOUR		(120*60)
		/*
		 * RFC2462 introduces the notion of StoredLifetime to the
		 * "two hours" rule as follows:
		 *   the Lifetime associated with the previously autoconfigured
		 *   address.
		 * Our interpretation of this definition is "the remaining
		 * lifetime to expiration at the evaluation time".  One might
		 * be wondering if this interpretation is really conform to the
		 * RFC, because the text can read that "Lifetimes" are never
		 * decreased, and our definition of the "storedlifetime" below
		 * essentially reduces the "Valid Lifetime" advertised in the
		 * previous RA.  But, this is due to the wording of the text,
		 * and our interpretation is the same as an author's intention.
		 * See the discussion in the IETF ipngwg ML in August 2001,
		 * with the Subject "StoredLifetime in RFC 2462".
		 */
		lt6_tmp = ia6->ia6_lifetime;

		/* RFC 4941 temporary addresses (privacy extension). */
		if (ia6->ia6_flags & IN6_IFF_PRIVACY) {
			/* Do we still have a non-deprecated address? */
			if ((ia6->ia6_flags & IN6_IFF_DEPRECATED) == 0)
				tempaddr_preferred = 1;
			/* Don't extend lifetime for temporary addresses. */
			if (new->ndpr_vltime >= lt6_tmp.ia6t_vltime)
				continue;
			if (new->ndpr_pltime >= lt6_tmp.ia6t_pltime)
				continue;
		} else if ((ia6->ia6_flags & IN6_IFF_DEPRECATED) == 0)
			/* We have a regular SLAAC address. */
			autoconf = 1;

		if (lt6_tmp.ia6t_vltime == ND6_INFINITE_LIFETIME)
			storedlifetime = ND6_INFINITE_LIFETIME;
		else if (time_second - ia6->ia6_updatetime >
			 lt6_tmp.ia6t_vltime) {
			/*
			 * The case of "invalid" address.  We should usually
			 * not see this case.
			 */
			storedlifetime = 0;
		} else
			storedlifetime = lt6_tmp.ia6t_vltime -
				(time_second - ia6->ia6_updatetime);
		if (TWOHOUR < new->ndpr_vltime ||
		    storedlifetime < new->ndpr_vltime) {
			lt6_tmp.ia6t_vltime = new->ndpr_vltime;
		} else if (storedlifetime <= TWOHOUR
#if 0
			   /*
			    * This condition is logically redundant, so we just
			    * omit it.
			    * See IPng 6712, 6717, and 6721.
			    */
			   && new->ndpr_vltime <= storedlifetime
#endif
			) {
			if (auth) {
				lt6_tmp.ia6t_vltime = new->ndpr_vltime;
			}
		} else {
			/*
			 * new->ndpr_vltime <= TWOHOUR &&
			 * TWOHOUR < storedlifetime
			 */
			lt6_tmp.ia6t_vltime = TWOHOUR;
		}

		/* The 2 hour rule is not imposed for preferred lifetime. */
		lt6_tmp.ia6t_pltime = new->ndpr_pltime;

		in6_init_address_ltimes(pr, &lt6_tmp);

		ia6->ia6_lifetime = lt6_tmp;
		ia6->ia6_updatetime = time_second;
	}

	if ((!autoconf || ((ifp->if_xflags & IFXF_INET6_NOPRIVACY) == 0 &&
	    !tempaddr_preferred)) && new->ndpr_vltime != 0 &&
	    !((ifp->if_xflags & IFXF_INET6_NOPRIVACY) && statique)) {
		/*
		 * There is no SLAAC address and/or there is no preferred RFC
		 * 4941 temporary address. And the valid prefix lifetime is
		 * non-zero. And there is no static address in the same prefix.
		 * Create new addresses in process context.
		 * Increment prefix refcount to ensure the prefix is not
		 * removed before the task is done.
		 */
		pr->ndpr_refcnt++;
		if (task_add(systq, &pr->ndpr_task) == 0)
			pr->ndpr_refcnt--;
	}

 end:
	splx(s);
	return error;
}

void
nd6_addr_add(void *prptr, void *arg2)
{
	struct nd_prefix *pr = (struct nd_prefix *)prptr;
	struct in6_ifaddr *ia6;
	struct ifaddr *ifa;
	int ifa_plen, autoconf, privacy, s;

	s = splsoftnet();

	autoconf = 1;
	privacy = (pr->ndpr_ifp->if_xflags & IFXF_INET6_NOPRIVACY) == 0;

	/* 
	 * Check again if a non-deprecated address has already
	 * been autoconfigured for this prefix.
	 */
	TAILQ_FOREACH(ifa, &pr->ndpr_ifp->if_addrlist, ifa_list) {
		if (ifa->ifa_addr->sa_family != AF_INET6)
			continue;

		ia6 = ifatoia6(ifa);

		/*
		 * Spec is not clear here, but I believe we should concentrate
		 * on unicast (i.e. not anycast) addresses.
		 * XXX: other ia6_flags? detached or duplicated?
		 */
		if ((ia6->ia6_flags & IN6_IFF_ANYCAST) != 0)
			continue;

		if ((ia6->ia6_flags & IN6_IFF_AUTOCONF) == 0)
			continue;

		if ((ia6->ia6_flags & IN6_IFF_DEPRECATED) != 0)
			continue;

		ifa_plen = in6_mask2len(&ia6->ia_prefixmask.sin6_addr, NULL);
		if (ifa_plen == pr->ndpr_plen &&
		    in6_are_prefix_equal(&ia6->ia_addr.sin6_addr,
		    &pr->ndpr_prefix.sin6_addr, ifa_plen)) {
			if ((ia6->ia6_flags & IN6_IFF_PRIVACY) == 0)
				autoconf = 0;
			else
				privacy = 0;
			if (!autoconf && !privacy)
				break;
		}
	}

	if (autoconf && (ia6 = in6_ifadd(pr, 0)) != NULL) {
		ia6->ia6_ndpr = pr;
		pr->ndpr_refcnt++;
	} else
		autoconf = 0;

	if (privacy && (ia6 = in6_ifadd(pr, 1)) != NULL) {
		ia6->ia6_ndpr = pr;
		pr->ndpr_refcnt++;
	} else
		privacy = 0;

	/*
	 * A newly added address might affect the status
	 * of other addresses, so we check and update it.
	 * XXX: what if address duplication happens?
	 */
	if (autoconf || privacy)
		pfxlist_onlink_check();

	/* Decrement prefix refcount now that the task is done. */
	pr->ndpr_refcnt--;

	splx(s);
}

/*
 * A supplement function used in the on-link detection below;
 * detect if a given prefix has a (probably) reachable advertising router.
 * XXX: lengthy function name...
 */
struct nd_pfxrouter *
find_pfxlist_reachable_router(struct nd_prefix *pr)
{
	struct nd_pfxrouter *pfxrtr;
	struct rtentry *rt;
	struct llinfo_nd6 *ln;

	LIST_FOREACH(pfxrtr, &pr->ndpr_advrtrs, pfr_entry) {
		if ((rt = nd6_lookup(&pfxrtr->router->rtaddr, 0,
		    pfxrtr->router->ifp, pfxrtr->router->ifp->if_rdomain)) &&
		    (ln = (struct llinfo_nd6 *)rt->rt_llinfo) &&
		    ND6_IS_LLINFO_PROBREACH(ln))
			break;	/* found */
	}

	return (pfxrtr);
}

/*
 * Check if each prefix in the prefix list has at least one available router
 * that advertised the prefix (a router is "available" if its neighbor cache
 * entry is reachable or probably reachable).
 * If the check fails, the prefix may be off-link, because, for example,
 * we have moved from the network but the lifetime of the prefix has not
 * expired yet.  So we should not use the prefix if there is another prefix
 * that has an available router.
 * But, if there is no prefix that has an available router, we still regards
 * all the prefixes as on-link.  This is because we can't tell if all the
 * routers are simply dead or if we really moved from the network and there
 * is no router around us.
 */
void
pfxlist_onlink_check(void)
{
	struct nd_prefix *pr;
	struct in6_ifaddr *ia6;
	char addr[INET6_ADDRSTRLEN];

	/*
	 * Check if there is a prefix that has a reachable advertising
	 * router.
	 */
	LIST_FOREACH(pr, &nd_prefix, ndpr_entry) {
		if (pr->ndpr_raf_onlink && find_pfxlist_reachable_router(pr))
			break;
	}
	if (pr != NULL || !TAILQ_EMPTY(&nd_defrouter)) {
		/*
		 * There is at least one prefix that has a reachable router,
		 * or at least a router which probably does not advertise
		 * any prefixes.  The latter would be the case when we move
		 * to a new link where we have a router that does not provide
		 * prefixes and we configure an address by hand.
		 * Detach prefixes which have no reachable advertising
		 * router, and attach other prefixes.
		 */
		LIST_FOREACH(pr, &nd_prefix, ndpr_entry) {
			/* XXX: a link-local prefix should never be detached */
			if (IN6_IS_ADDR_LINKLOCAL(&pr->ndpr_prefix.sin6_addr))
				continue;

			/*
			 * we aren't interested in prefixes without the L bit
			 * set.
			 */
			if (pr->ndpr_raf_onlink == 0)
				continue;

			if ((pr->ndpr_stateflags & NDPRF_DETACHED) == 0 &&
			    find_pfxlist_reachable_router(pr) == NULL)
				pr->ndpr_stateflags |= NDPRF_DETACHED;
			if ((pr->ndpr_stateflags & NDPRF_DETACHED) != 0 &&
			    find_pfxlist_reachable_router(pr) != 0)
				pr->ndpr_stateflags &= ~NDPRF_DETACHED;
		}
	} else {
		/* there is no prefix that has a reachable router */
		LIST_FOREACH(pr, &nd_prefix, ndpr_entry) {
			if (IN6_IS_ADDR_LINKLOCAL(&pr->ndpr_prefix.sin6_addr))
				continue;

			if (pr->ndpr_raf_onlink == 0)
				continue;

			if ((pr->ndpr_stateflags & NDPRF_DETACHED) != 0)
				pr->ndpr_stateflags &= ~NDPRF_DETACHED;
		}
	}

	/*
	 * Remove each interface route associated with a (just) detached
	 * prefix, and reinstall the interface route for a (just) attached
	 * prefix.  Note that all attempt of reinstallation does not
	 * necessarily success, when a same prefix is shared among multiple
	 * interfaces.  Such cases will be handled in nd6_prefix_onlink,
	 * so we don't have to care about them.
	 */
	LIST_FOREACH(pr, &nd_prefix, ndpr_entry) {	
		int e;

		if (IN6_IS_ADDR_LINKLOCAL(&pr->ndpr_prefix.sin6_addr))
			continue;

		if (pr->ndpr_raf_onlink == 0)
			continue;

		if ((pr->ndpr_stateflags & NDPRF_DETACHED) != 0 &&
		    (pr->ndpr_stateflags & NDPRF_ONLINK) != 0) {
			if ((e = nd6_prefix_offlink(pr)) != 0) {
				nd6log((LOG_ERR,
				    "pfxlist_onlink_check: failed to "
				    "make %s/%d offlink, errno=%d\n",
				    inet_ntop(AF_INET6,
					&pr->ndpr_prefix.sin6_addr,
					addr, sizeof(addr)),
				    pr->ndpr_plen, e));
			}
		}
		if ((pr->ndpr_stateflags & NDPRF_DETACHED) == 0 &&
		    (pr->ndpr_stateflags & NDPRF_ONLINK) == 0 &&
		    pr->ndpr_raf_onlink) {
			if ((e = nd6_prefix_onlink(pr)) != 0) {
				nd6log((LOG_ERR,
				    "pfxlist_onlink_check: failed to "
				    "make %s/%d offlink, errno=%d\n",
				    inet_ntop(AF_INET6,
					&pr->ndpr_prefix.sin6_addr,
					addr, sizeof(addr)),
				    pr->ndpr_plen, e));
			}
		}
	}

	/*
	 * Changes on the prefix status might affect address status as well.
	 * Make sure that all addresses derived from an attached prefix are
	 * attached, and that all addresses derived from a detached prefix are
	 * detached.  Note, however, that a manually configured address should
	 * always be attached.
	 * The precise detection logic is same as the one for prefixes.
	 */
	TAILQ_FOREACH(ia6, &in6_ifaddr, ia_list) {
		if (!(ia6->ia6_flags & IN6_IFF_AUTOCONF))
			continue;

		if (ia6->ia6_ndpr == NULL) {
			/*
			 * This can happen when we first configure the address
			 * (i.e. the address exists, but the prefix does not).
			 * XXX: complicated relationships...
			 */
			continue;
		}

		if (find_pfxlist_reachable_router(ia6->ia6_ndpr))
			break;
	}
	if (ia6) {
		TAILQ_FOREACH(ia6, &in6_ifaddr, ia_list) {
			if ((ia6->ia6_flags & IN6_IFF_AUTOCONF) == 0)
				continue;

			if (ia6->ia6_ndpr == NULL) /* XXX: see above. */
				continue;

			if (find_pfxlist_reachable_router(ia6->ia6_ndpr))
				ia6->ia6_flags &= ~IN6_IFF_DETACHED;
			else
				ia6->ia6_flags |= IN6_IFF_DETACHED;
		}
	}
	else {
		TAILQ_FOREACH(ia6, &in6_ifaddr, ia_list) {
			if ((ia6->ia6_flags & IN6_IFF_AUTOCONF) == 0)
				continue;

			ia6->ia6_flags &= ~IN6_IFF_DETACHED;
		}
	}
}

int
nd6_prefix_onlink(struct nd_prefix *pr)
{
	struct rt_addrinfo info;
	struct ifaddr *ifa;
	struct ifnet *ifp = pr->ndpr_ifp;
	struct sockaddr_in6 mask6;
	struct nd_prefix *opr;
	u_long rtflags;
	int error = 0;
	struct rtentry *rt = NULL;
	char addr[INET6_ADDRSTRLEN];

	/* sanity check */
	if ((pr->ndpr_stateflags & NDPRF_ONLINK) != 0) {
		nd6log((LOG_ERR,
		    "nd6_prefix_onlink: %s/%d is already on-link\n",
		    inet_ntop(AF_INET6,	&pr->ndpr_prefix.sin6_addr,
			addr, sizeof(addr)),
		    pr->ndpr_plen));
		return (EEXIST);
	}

	/*
	 * Add the interface route associated with the prefix.  Before
	 * installing the route, check if there's the same prefix on another
	 * interface, and the prefix has already installed the interface route.
	 * Although such a configuration is expected to be rare, we explicitly
	 * allow it.
	 */
	LIST_FOREACH(opr, &nd_prefix, ndpr_entry) {
		if (opr == pr)
			continue;

		if ((opr->ndpr_stateflags & NDPRF_ONLINK) == 0)
			continue;

		if (opr->ndpr_plen == pr->ndpr_plen &&
		    in6_are_prefix_equal(&pr->ndpr_prefix.sin6_addr,
		    &opr->ndpr_prefix.sin6_addr, pr->ndpr_plen))
			return (0);
	}

	/*
	 * We prefer link-local addresses as the associated interface address.
	 */
	/* search for a link-local addr */
	ifa = &in6ifa_ifpforlinklocal(ifp,
	    IN6_IFF_NOTREADY | IN6_IFF_ANYCAST)->ia_ifa;
	if (ifa == NULL) {
		TAILQ_FOREACH(ifa, &ifp->if_addrlist, ifa_list) {
			if (ifa->ifa_addr->sa_family == AF_INET6)
				break;
		}
		/* should we care about ia6_flags? */
	}
	if (ifa == NULL) {
		/*
		 * This can still happen, when, for example, we receive an RA
		 * containing a prefix with the L bit set and the A bit clear,
		 * after removing all IPv6 addresses on the receiving
		 * interface.  This should, of course, be rare though.
		 */
		nd6log((LOG_NOTICE,
		    "nd6_prefix_onlink: failed to find any ifaddr"
		    " to add route for a prefix(%s/%d) on %s\n",
		    inet_ntop(AF_INET6,	&pr->ndpr_prefix.sin6_addr,
			addr, sizeof(addr)),
		    pr->ndpr_plen, ifp->if_xname));
		return (0);
	}

	/*
	 * in6_ifinit() sets nd6_rtrequest to ifa_rtrequest for all ifaddrs.
	 * ifa->ifa_rtrequest = nd6_rtrequest;
	 */
	bzero(&mask6, sizeof(mask6));
	mask6.sin6_len = sizeof(mask6);
	mask6.sin6_addr = pr->ndpr_mask;
	/* rtrequest1() will probably set RTF_UP, but we're not sure. */
	rtflags = RTF_UP;
	if (nd6_need_cache(ifp))
		rtflags |= RTF_CLONING;
	else
		rtflags &= ~RTF_CLONING;

	bzero(&info, sizeof(info));
	info.rti_flags = rtflags;
	info.rti_info[RTAX_DST] = sin6tosa(&pr->ndpr_prefix);
	info.rti_info[RTAX_GATEWAY] = ifa->ifa_addr;
	info.rti_info[RTAX_NETMASK] = sin6tosa(&mask6);

	error = rtrequest1(RTM_ADD, &info, RTP_CONNECTED, &rt, ifp->if_rdomain);
	if (error == 0) {
		if (rt != NULL) /* this should be non NULL, though */
			rt_sendmsg(rt, RTM_ADD, ifp->if_rdomain);
		pr->ndpr_stateflags |= NDPRF_ONLINK;
	} else {
		char gw[INET6_ADDRSTRLEN], mask[INET6_ADDRSTRLEN];
		nd6log((LOG_ERR, "nd6_prefix_onlink: failed to add route for a"
		    " prefix (%s/%d) on %s, gw=%s, mask=%s, flags=%lx "
		    "errno = %d\n",
		    inet_ntop(AF_INET6,	&pr->ndpr_prefix.sin6_addr,
			addr, sizeof(addr)),
		    pr->ndpr_plen, ifp->if_xname,
		    inet_ntop(AF_INET6, &satosin6(ifa->ifa_addr)->sin6_addr,
			gw, sizeof(gw)),
		    inet_ntop(AF_INET6, &mask6.sin6_addr, mask, sizeof(mask)),
		    rtflags, error));
	}

	if (rt != NULL)
		rt->rt_refcnt--;

	return (error);
}

int
nd6_prefix_offlink(struct nd_prefix *pr)
{
	struct rt_addrinfo info;
	int error = 0;
	struct ifnet *ifp = pr->ndpr_ifp;
	struct nd_prefix *opr;
	struct sockaddr_in6 sa6, mask6;
	struct rtentry *rt = NULL;
	char addr[INET6_ADDRSTRLEN];

	/* sanity check */
	if ((pr->ndpr_stateflags & NDPRF_ONLINK) == 0) {
		nd6log((LOG_ERR,
		    "nd6_prefix_offlink: %s/%d is already off-link\n",
		    inet_ntop(AF_INET6, &pr->ndpr_prefix.sin6_addr,
			addr, sizeof(addr)),
		    pr->ndpr_plen));
		return (EEXIST);
	}

	bzero(&sa6, sizeof(sa6));
	sa6.sin6_family = AF_INET6;
	sa6.sin6_len = sizeof(sa6);
	bcopy(&pr->ndpr_prefix.sin6_addr, &sa6.sin6_addr,
	    sizeof(struct in6_addr));
	bzero(&mask6, sizeof(mask6));
	mask6.sin6_family = AF_INET6;
	mask6.sin6_len = sizeof(sa6);
	bcopy(&pr->ndpr_mask, &mask6.sin6_addr, sizeof(struct in6_addr));
	bzero(&info, sizeof(info));
	info.rti_info[RTAX_DST] = sin6tosa(&sa6);
	info.rti_info[RTAX_NETMASK] = sin6tosa(&mask6);
	error = rtrequest1(RTM_DELETE, &info, RTP_CONNECTED, &rt,
	    ifp->if_rdomain);
	if (error == 0) {
		pr->ndpr_stateflags &= ~NDPRF_ONLINK;

		/* report the route deletion to the routing socket. */
		if (rt != NULL)
			rt_sendmsg(rt, RTM_DELETE, ifp->if_rdomain);

		/*
		 * There might be the same prefix on another interface,
		 * the prefix which could not be on-link just because we have
		 * the interface route (see comments in nd6_prefix_onlink).
		 * If there's one, try to make the prefix on-link on the
		 * interface.
		 */
		LIST_FOREACH(opr, &nd_prefix, ndpr_entry) {
			if (opr == pr)
				continue;

			if ((opr->ndpr_stateflags & NDPRF_ONLINK) != 0)
				continue;

			/*
			 * KAME specific: detached prefixes should not be
			 * on-link.
			 */
			if ((opr->ndpr_stateflags & NDPRF_DETACHED) != 0)
				continue;

			if (opr->ndpr_plen == pr->ndpr_plen &&
			    in6_are_prefix_equal(&pr->ndpr_prefix.sin6_addr,
			    &opr->ndpr_prefix.sin6_addr, pr->ndpr_plen)) {
				int e;

				if ((e = nd6_prefix_onlink(opr)) != 0) {
					nd6log((LOG_ERR,
					    "nd6_prefix_offlink: failed to "
					    "recover a prefix %s/%d from %s "
					    "to %s (errno = %d)\n",
					    inet_ntop(AF_INET6,
						&pr->ndpr_prefix.sin6_addr,
						addr, sizeof(addr)),
					    opr->ndpr_plen, ifp->if_xname,
					    opr->ndpr_ifp->if_xname, e));
				}
			}
		}
	} else {
		/* XXX: can we still set the NDPRF_ONLINK flag? */
		nd6log((LOG_ERR,
		    "nd6_prefix_offlink: failed to delete route: "
		    "%s/%d on %s (errno = %d)\n",
		    inet_ntop(AF_INET6,	&sa6.sin6_addr, addr, sizeof(addr)),
		    pr->ndpr_plen, ifp->if_xname, error));
	}

	if (rt != NULL) {
		if (rt->rt_refcnt <= 0) {
			/* XXX: we should free the entry ourselves. */
			rt->rt_refcnt++;
			rtfree(rt);
		}
	}

	return (error);
}

struct in6_ifaddr *
in6_ifadd(struct nd_prefix *pr, int privacy)
{
	struct ifnet *ifp = pr->ndpr_ifp;
	struct ifaddr *ifa;
	struct in6_aliasreq ifra;
	struct in6_ifaddr *ia6;
	int error, s, plen0;
	struct in6_addr mask, rand_ifid;
	int prefixlen = pr->ndpr_plen;

	in6_prefixlen2mask(&mask, prefixlen);

	/*
	 * find a link-local address (will be interface ID).
	 * Is it really mandatory? Theoretically, a global or a site-local
	 * address can be configured without a link-local address, if we
	 * have a unique interface identifier...
	 *
	 * it is not mandatory to have a link-local address, we can generate
	 * interface identifier on the fly.  we do this because:
	 * (1) it should be the easiest way to find interface identifier.
	 * (2) RFC2462 5.4 suggesting the use of the same interface identifier
	 * for multiple addresses on a single interface, and possible shortcut
	 * of DAD.  we omitted DAD for this reason in the past.
	 * (3) a user can prevent autoconfiguration of global address
	 * by removing link-local address by hand (this is partly because we
	 * don't have other way to control the use of IPv6 on a interface.
	 * this has been our design choice - cf. NRL's "ifconfig auto").
	 * (4) it is easier to manage when an interface has addresses
	 * with the same interface identifier, than to have multiple addresses
	 * with different interface identifiers.
	 */
	ifa = &in6ifa_ifpforlinklocal(ifp, 0)->ia_ifa; /* 0 is OK? */
	if (ifa)
		ia6 = ifatoia6(ifa);
	else
		return NULL;

#if 0 /* don't care link local addr state, and always do DAD */
	/* if link-local address is not eligible, do not autoconfigure. */
	if (ifatoia6(ifa)->ia6_flags & IN6_IFF_NOTREADY) {
		printf("in6_ifadd: link-local address not ready\n");
		return NULL;
	}
#endif

	/* prefixlen + ifidlen must be equal to 128 */
	plen0 = in6_mask2len(&ia6->ia_prefixmask.sin6_addr, NULL);
	if (prefixlen != plen0) {
		nd6log((LOG_INFO, "in6_ifadd: wrong prefixlen for %s "
		    "(prefix=%d ifid=%d)\n",
		    ifp->if_xname, prefixlen, 128 - plen0));
		return NULL;
	}

	/* make ifaddr */

	bzero(&ifra, sizeof(ifra));
	/*
	 * in6_update_ifa() does not use ifra_name, but we accurately set it
	 * for safety.
	 */
	strncpy(ifra.ifra_name, ifp->if_xname, sizeof(ifra.ifra_name));
	ifra.ifra_addr.sin6_family = AF_INET6;
	ifra.ifra_addr.sin6_len = sizeof(struct sockaddr_in6);
	/* prefix */
	bcopy(&pr->ndpr_prefix.sin6_addr, &ifra.ifra_addr.sin6_addr,
	    sizeof(ifra.ifra_addr.sin6_addr));
	ifra.ifra_addr.sin6_addr.s6_addr32[0] &= mask.s6_addr32[0];
	ifra.ifra_addr.sin6_addr.s6_addr32[1] &= mask.s6_addr32[1];
	ifra.ifra_addr.sin6_addr.s6_addr32[2] &= mask.s6_addr32[2];
	ifra.ifra_addr.sin6_addr.s6_addr32[3] &= mask.s6_addr32[3];

	/* interface ID */
	if (privacy) {
		ifra.ifra_flags |= IN6_IFF_PRIVACY;
		bcopy(&pr->ndpr_prefix.sin6_addr, &rand_ifid,
		    sizeof(rand_ifid));
		in6_get_rand_ifid(ifp, &rand_ifid);
		ifra.ifra_addr.sin6_addr.s6_addr32[0] |=
		    (rand_ifid.s6_addr32[0] & ~mask.s6_addr32[0]);
		ifra.ifra_addr.sin6_addr.s6_addr32[1] |=
		    (rand_ifid.s6_addr32[1] & ~mask.s6_addr32[1]);
		ifra.ifra_addr.sin6_addr.s6_addr32[2] |=
		    (rand_ifid.s6_addr32[2] & ~mask.s6_addr32[2]);
		ifra.ifra_addr.sin6_addr.s6_addr32[3] |=
		    (rand_ifid.s6_addr32[3] & ~mask.s6_addr32[3]);
	} else {
		ifra.ifra_addr.sin6_addr.s6_addr32[0] |=
		    (ia6->ia_addr.sin6_addr.s6_addr32[0] & ~mask.s6_addr32[0]);
		ifra.ifra_addr.sin6_addr.s6_addr32[1] |=
		    (ia6->ia_addr.sin6_addr.s6_addr32[1] & ~mask.s6_addr32[1]);
		ifra.ifra_addr.sin6_addr.s6_addr32[2] |=
		    (ia6->ia_addr.sin6_addr.s6_addr32[2] & ~mask.s6_addr32[2]);
		ifra.ifra_addr.sin6_addr.s6_addr32[3] |=
		    (ia6->ia_addr.sin6_addr.s6_addr32[3] & ~mask.s6_addr32[3]);
	}

	/* new prefix mask. */
	ifra.ifra_prefixmask.sin6_len = sizeof(struct sockaddr_in6);
	ifra.ifra_prefixmask.sin6_family = AF_INET6;
	bcopy(&mask, &ifra.ifra_prefixmask.sin6_addr,
	    sizeof(ifra.ifra_prefixmask.sin6_addr));

	/*
	 * lifetime.
	 * XXX: in6_init_address_ltimes would override these values later.
	 * We should reconsider this logic.
	 */
	ifra.ifra_lifetime.ia6t_vltime = pr->ndpr_vltime;
	ifra.ifra_lifetime.ia6t_pltime = pr->ndpr_pltime;

	if (privacy) {
	    if (ifra.ifra_lifetime.ia6t_vltime > ND6_PRIV_VALID_LIFETIME)
		ifra.ifra_lifetime.ia6t_vltime = ND6_PRIV_VALID_LIFETIME;
	    if (ifra.ifra_lifetime.ia6t_pltime > ND6_PRIV_PREFERRED_LIFETIME)
		ifra.ifra_lifetime.ia6t_pltime = ND6_PRIV_PREFERRED_LIFETIME
			- (arc4random() % ND6_PRIV_MAX_DESYNC_FACTOR);
	}

	/* XXX: scope zone ID? */

	ifra.ifra_flags |= IN6_IFF_AUTOCONF; /* obey autoconf */

	/* allocate ifaddr structure, link into chain, etc. */
	s = splsoftnet();
	error = in6_update_ifa(ifp, &ifra, NULL);
	splx(s);

	if (error != 0) {
		char addr[INET6_ADDRSTRLEN];

		nd6log((LOG_ERR,
		    "in6_ifadd: failed to make ifaddr %s on %s (errno=%d)\n",
		    inet_ntop(AF_INET6,	&ifra.ifra_addr.sin6_addr,
			addr, sizeof(addr)),
		    ifp->if_xname, error));
		return (NULL);	/* ifaddr must not have been allocated. */
	}

	/* this is always non-NULL */
	return (in6ifa_ifpwithaddr(ifp, &ifra.ifra_addr.sin6_addr));
}

int
in6_init_prefix_ltimes(struct nd_prefix *ndpr)
{

	/* check if preferred lifetime > valid lifetime.  RFC2462 5.5.3 (c) */
	if (ndpr->ndpr_pltime > ndpr->ndpr_vltime) {
		nd6log((LOG_INFO, "in6_init_prefix_ltimes: preferred lifetime"
		    "(%d) is greater than valid lifetime(%d)\n",
		    (u_int)ndpr->ndpr_pltime, (u_int)ndpr->ndpr_vltime));
		return (EINVAL);
	}
	if (ndpr->ndpr_pltime == ND6_INFINITE_LIFETIME)
		ndpr->ndpr_preferred = 0;
	else
		ndpr->ndpr_preferred = time_second + ndpr->ndpr_pltime;
	if (ndpr->ndpr_vltime == ND6_INFINITE_LIFETIME)
		ndpr->ndpr_expire = 0;
	else
		ndpr->ndpr_expire = time_second + ndpr->ndpr_vltime;

	return 0;
}

void
in6_init_address_ltimes(struct nd_prefix *new, struct in6_addrlifetime *lt6)
{

	/* Valid lifetime must not be updated unless explicitly specified. */
	/* init ia6t_expire */
	if (lt6->ia6t_vltime == ND6_INFINITE_LIFETIME)
		lt6->ia6t_expire = 0;
	else {
		lt6->ia6t_expire = time_second;
		lt6->ia6t_expire += lt6->ia6t_vltime;
	}

	/* init ia6t_preferred */
	if (lt6->ia6t_pltime == ND6_INFINITE_LIFETIME)
		lt6->ia6t_preferred = 0;
	else {
		lt6->ia6t_preferred = time_second;
		lt6->ia6t_preferred += lt6->ia6t_pltime;
	}
}

/*
 * Delete all the routing table entries that use the specified gateway.
 * XXX: this function causes search through all entries of routing table, so
 * it shouldn't be called when acting as a router.
 */
void
rt6_flush(struct in6_addr *gateway, struct ifnet *ifp)
{
	struct radix_node_head *rnh = rtable_get(ifp->if_rdomain, AF_INET6);
	int s = splsoftnet();

	/* We'll care only link-local addresses */
	if (!IN6_IS_ADDR_LINKLOCAL(gateway)) {
		splx(s);
		return;
	}
	/* XXX: hack for KAME's link-local address kludge */
	gateway->s6_addr16[1] = htons(ifp->if_index);

	rnh->rnh_walktree(rnh, rt6_deleteroute, (void *)gateway);
	splx(s);
}

int
rt6_deleteroute(struct radix_node *rn, void *arg, u_int id)
{
	struct rt_addrinfo info;
	struct rtentry *rt = (struct rtentry *)rn;
	struct in6_addr *gate = (struct in6_addr *)arg;

	if (rt->rt_gateway == NULL || rt->rt_gateway->sa_family != AF_INET6)
		return (0);

	if (!IN6_ARE_ADDR_EQUAL(gate, &satosin6(rt->rt_gateway)->sin6_addr))
		return (0);

	/*
	 * Do not delete a static route.
	 * XXX: this seems to be a bit ad-hoc. Should we consider the
	 * 'cloned' bit instead?
	 */
	if ((rt->rt_flags & RTF_STATIC) != 0)
		return (0);

	/*
	 * We delete only host route. This means, in particular, we don't
	 * delete default route.
	 */
	if ((rt->rt_flags & RTF_HOST) == 0)
		return (0);

	bzero(&info, sizeof(info));
	info.rti_flags =  rt->rt_flags;
	info.rti_info[RTAX_DST] = rt_key(rt);
	info.rti_info[RTAX_GATEWAY] = rt->rt_gateway;
	info.rti_info[RTAX_NETMASK] = rt_mask(rt);
	return (rtrequest1(RTM_DELETE, &info, RTP_ANY, NULL, id));
}
