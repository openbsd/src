/*	$OpenBSD: nd6_rtr.c,v 1.164 2017/08/09 06:21:04 florian Exp $	*/
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
#include <net/if_dl.h>
#include <net/if_var.h>
#include <net/route.h>
#include <net/rtable.h>

#include <netinet/in.h>
#include <netinet6/in6_var.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/nd6.h>
#include <netinet/icmp6.h>

int rt6_deleteroute(struct rtentry *, void *, unsigned int);

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
	struct ifnet *ifp;
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
	if (!ip6_forwarding)
		goto freeit;

	/* Sanity checks */
	if (ip6->ip6_hlim != 255) {
		nd6log((LOG_ERR,
		    "nd6_rs_input: invalid hlim (%d) from %s to %s on %u\n",
		    ip6->ip6_hlim,
		    inet_ntop(AF_INET6, &ip6->ip6_src, src, sizeof(src)),
		    inet_ntop(AF_INET6, &ip6->ip6_dst, dst, sizeof(dst)),
		    m->m_pkthdr.ph_ifidx));
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
		icmp6stat_inc(icp6s_tooshort);
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

	ifp = if_get(m->m_pkthdr.ph_ifidx);
	if (ifp == NULL)
		goto freeit;

	if (lladdr && ((ifp->if_addrlen + 2 + 7) & ~7) != lladdrlen) {
		nd6log((LOG_INFO,
		    "nd6_rs_input: lladdrlen mismatch for %s "
		    "(if %d, RS packet %d)\n",
		    inet_ntop(AF_INET6, &saddr6, src, sizeof(src)),
		    ifp->if_addrlen, lladdrlen - 2));
		if_put(ifp);
		goto bad;
	}

	nd6_cache_lladdr(ifp, &saddr6, lladdr, lladdrlen, ND_ROUTER_SOLICIT, 0);
	if_put(ifp);

 freeit:
	m_freem(m);
	return;

 bad:
	icmp6stat_inc(icp6s_badrs);
	m_freem(m);
}

/*
 * Receive Router Advertisement Message.
 *
 * Based on RFC 2461
 */
void
nd6_ra_input(struct mbuf *m, int off, int icmp6len)
{
	struct ifnet *ifp;
	struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);
	struct nd_router_advert *nd_ra;
	struct in6_addr saddr6 = ip6->ip6_src;
	char *lladdr = NULL;
	int lladdrlen = 0;
	union nd_opts ndopts;
	char src[INET6_ADDRSTRLEN], dst[INET6_ADDRSTRLEN];

	/* Sanity checks */
	if (ip6->ip6_hlim != 255) {
		nd6log((LOG_ERR,
		    "nd6_ra_input: invalid hlim (%d) from %s to %s on %u\n",
		    ip6->ip6_hlim,
		    inet_ntop(AF_INET6, &ip6->ip6_src, src, sizeof(src)),
		    inet_ntop(AF_INET6, &ip6->ip6_dst, dst, sizeof(dst)),
		    m->m_pkthdr.ph_ifidx));
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
		icmp6stat_inc(icp6s_tooshort);
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

	if (ndopts.nd_opts_src_lladdr) {
		lladdr = (char *)(ndopts.nd_opts_src_lladdr + 1);
		lladdrlen = ndopts.nd_opts_src_lladdr->nd_opt_len << 3;
	}

	ifp = if_get(m->m_pkthdr.ph_ifidx);
	if (ifp == NULL)
		goto freeit;

	if (lladdr && ((ifp->if_addrlen + 2 + 7) & ~7) != lladdrlen) {
		nd6log((LOG_INFO,
		    "nd6_ra_input: lladdrlen mismatch for %s "
		    "(if %d, RA packet %d)\n",
		    inet_ntop(AF_INET6, &saddr6, src, sizeof(src)),
		    ifp->if_addrlen, lladdrlen - 2));
		if_put(ifp);
		goto bad;
	}

	nd6_cache_lladdr(ifp, &saddr6, lladdr, lladdrlen, ND_ROUTER_ADVERT, 0);
	if_put(ifp);

 freeit:
	m_freem(m);
	return;

 bad:
	icmp6stat_inc(icp6s_badra);
	m_freem(m);
}

/*
 * Delete all the routing table entries that use the specified gateway.
 * XXX: this function causes search through all entries of routing table, so
 * it shouldn't be called when acting as a router.
 */
void
rt6_flush(struct in6_addr *gateway, struct ifnet *ifp)
{
	NET_ASSERT_LOCKED();

	/* We'll care only link-local addresses */
	if (!IN6_IS_ADDR_LINKLOCAL(gateway))
		return;

	/* XXX: hack for KAME's link-local address kludge */
	gateway->s6_addr16[1] = htons(ifp->if_index);

	rtable_walk(ifp->if_rdomain, AF_INET6, rt6_deleteroute, gateway);
}

int
rt6_deleteroute(struct rtentry *rt, void *arg, unsigned int id)
{
	struct ifnet *ifp;
	struct rt_addrinfo info;
	struct in6_addr *gate = (struct in6_addr *)arg;
	struct sockaddr_in6 sa_mask;
	int error = 0;

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

	ifp = if_get(rt->rt_ifidx);
	if (ifp != NULL) {
		bzero(&info, sizeof(info));
		info.rti_flags =  rt->rt_flags;
		info.rti_info[RTAX_DST] = rt_key(rt);
		info.rti_info[RTAX_GATEWAY] = rt->rt_gateway;
		info.rti_info[RTAX_NETMASK] = rt_plen2mask(rt, &sa_mask);
		error = rtrequest_delete(&info, RTP_ANY, ifp, NULL, id);
	}
	if_put(ifp);

	return (error != 0 ? error : EAGAIN);
}
