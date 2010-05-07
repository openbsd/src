/*	$OpenBSD: in6_src.c,v 1.25 2010/05/07 13:33:17 claudio Exp $	*/
/*	$KAME: in6_src.c,v 1.36 2001/02/06 04:08:17 itojun Exp $	*/

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
 * Copyright (c) 1982, 1986, 1991, 1993
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
 *	@(#)in_pcb.c	8.2 (Berkeley) 1/4/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/proc.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet6/in6_var.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/nd6.h>

int selectroute(struct sockaddr_in6 *, struct ip6_pktopts *,
    struct ip6_moptions *, struct route_in6 *, struct ifnet **,
    struct rtentry **, int);

/*
 * Return an IPv6 address, which is the most appropriate for a given
 * destination and user specified options.
 * If necessary, this function lookups the routing table and returns
 * an entry to the caller for later use.
 */
struct in6_addr *
in6_selectsrc(struct sockaddr_in6 *dstsock, struct ip6_pktopts *opts,
    struct ip6_moptions *mopts, struct route_in6 *ro, struct in6_addr *laddr,
    int *errorp)
{
	struct in6_addr *dst;
	struct in6_ifaddr *ia6 = 0;
	struct in6_pktinfo *pi = NULL;

	dst = &dstsock->sin6_addr;
	*errorp = 0;

	/*
	 * If the source address is explicitly specified by the caller,
	 * use it.
	 */
	if (opts && (pi = opts->ip6po_pktinfo) &&
	    !IN6_IS_ADDR_UNSPECIFIED(&pi->ipi6_addr))
		return (&pi->ipi6_addr);

	/*
	 * If the source address is not specified but the socket(if any)
	 * is already bound, use the bound address.
	 */
	if (laddr && !IN6_IS_ADDR_UNSPECIFIED(laddr))
		return (laddr);

	/*
	 * If the caller doesn't specify the source address but
	 * the outgoing interface, use an address associated with
	 * the interface.
	 */
	if (pi && pi->ipi6_ifindex) {
		/* XXX boundary check is assumed to be already done. */
		ia6 = in6_ifawithscope(ifindex2ifnet[pi->ipi6_ifindex],
				       dst);
		if (ia6 == 0) {
			*errorp = EADDRNOTAVAIL;
			return (0);
		}
		return (&satosin6(&ia6->ia_addr)->sin6_addr);
	}

	/*
	 * If the destination address is a link-local unicast address or
	 * a link/interface-local multicast address, and if the outgoing
	 * interface is specified by the sin6_scope_id filed, use an address
	 * associated with the interface.
	 * XXX: We're now trying to define more specific semantics of
	 *      sin6_scope_id field, so this part will be rewritten in
	 *      the near future.
	 */
	if ((IN6_IS_ADDR_LINKLOCAL(dst) || IN6_IS_ADDR_MC_LINKLOCAL(dst) ||
	     IN6_IS_ADDR_MC_INTFACELOCAL(dst)) && dstsock->sin6_scope_id) {
		/*
		 * I'm not sure if boundary check for scope_id is done
		 * somewhere...
		 */
		if (dstsock->sin6_scope_id < 0 ||
		    if_indexlim <= dstsock->sin6_scope_id ||
		    !ifindex2ifnet[dstsock->sin6_scope_id]) {
			*errorp = ENXIO; /* XXX: better error? */
			return (0);
		}
		ia6 = in6_ifawithscope(ifindex2ifnet[dstsock->sin6_scope_id],
				       dst);
		if (ia6 == 0) {
			*errorp = EADDRNOTAVAIL;
			return (0);
		}
		return (&satosin6(&ia6->ia_addr)->sin6_addr);
	}

	/*
	 * If the destination address is a multicast address and
	 * the outgoing interface for the address is specified
	 * by the caller, use an address associated with the interface.
	 * Even if the outgoing interface is not specified, we also
	 * choose a loopback interface as the outgoing interface.
	 */
	if (IN6_IS_ADDR_MULTICAST(dst)) {
		struct ifnet *ifp = mopts ? mopts->im6o_multicast_ifp : NULL;

		if (!ifp && dstsock->sin6_scope_id)
			ifp = ifindex2ifnet[htons(dstsock->sin6_scope_id)];

		if (ifp) {
			ia6 = in6_ifawithscope(ifp, dst);
			if (ia6 == 0) {
				*errorp = EADDRNOTAVAIL;
				return (0);
			}
			return (&satosin6(&ia6->ia_addr)->sin6_addr);
		}
	}

	/*
	 * If the next hop address for the packet is specified
	 * by caller, use an address associated with the route
	 * to the next hop.
	 */
	{
		struct sockaddr_in6 *sin6_next;
		struct rtentry *rt;

		if (opts && opts->ip6po_nexthop) {
			sin6_next = satosin6(opts->ip6po_nexthop);
			rt = nd6_lookup(&sin6_next->sin6_addr, 1, NULL);
			if (rt) {
				ia6 = in6_ifawithscope(rt->rt_ifp, dst);
				if (ia6 == 0)
					ia6 = ifatoia6(rt->rt_ifa);
			}
			if (ia6 == 0) {
				*errorp = EADDRNOTAVAIL;
				return (0);
			}
			return (&satosin6(&ia6->ia_addr)->sin6_addr);
		}
	}

	/*
	 * If route is known or can be allocated now,
	 * our src addr is taken from the i/f, else punt.
	 */
	if (ro) {
		if (ro->ro_rt && ((ro->ro_rt->rt_flags & RTF_UP) == 0 ||
		    !IN6_ARE_ADDR_EQUAL(&satosin6(&ro->ro_dst)->sin6_addr, dst))) {
			RTFREE(ro->ro_rt);
			ro->ro_rt = (struct rtentry *)0;
		}
		if (ro->ro_rt == (struct rtentry *)0 ||
		    ro->ro_rt->rt_ifp == (struct ifnet *)0) {
			struct sockaddr_in6 *sa6;

			/* No route yet, so try to acquire one */
			bzero(&ro->ro_dst, sizeof(struct sockaddr_in6));
			sa6 = (struct sockaddr_in6 *)&ro->ro_dst;
			sa6->sin6_family = AF_INET6;
			sa6->sin6_len = sizeof(struct sockaddr_in6);
			sa6->sin6_addr = *dst;
			sa6->sin6_scope_id = dstsock->sin6_scope_id;
			if (IN6_IS_ADDR_MULTICAST(dst)) {
				rtalloc((struct route *)ro);
			} else {
				rtalloc_mpath((struct route *)ro, NULL);
			}
		}

		/*
		 * in_pcbconnect() checks out IFF_LOOPBACK to skip using
		 * the address. But we don't know why it does so.
		 * It is necessary to ensure the scope even for lo0
		 * so doesn't check out IFF_LOOPBACK.
		 */

		if (ro->ro_rt) {
			ia6 = in6_ifawithscope(ro->ro_rt->rt_ifa->ifa_ifp, dst);
			if (ia6 == 0) /* xxx scope error ?*/
				ia6 = ifatoia6(ro->ro_rt->rt_ifa);
		}
#if 0
		/*
		 * xxx The followings are necessary? (kazu)
		 * I don't think so.
		 * It's for SO_DONTROUTE option in IPv4.(jinmei)
		 */
		if (ia6 == 0) {
			struct sockaddr_in6 sin6 = {sizeof(sin6), AF_INET6, 0};

			sin6->sin6_addr = *dst;

			ia6 = ifatoia6(ifa_ifwithdstaddr(sin6tosa(&sin6)));
			if (ia6 == 0)
				ia6 = ifatoia6(ifa_ifwithnet(sin6tosa(&sin6)));
			if (ia6 == 0)
				return (0);
			return (&satosin6(&ia6->ia_addr)->sin6_addr);
		}
#endif /* 0 */
		if (ia6 == 0) {
			*errorp = EHOSTUNREACH;	/* no route */
			return (0);
		}
		return (&satosin6(&ia6->ia_addr)->sin6_addr);
	}

	*errorp = EADDRNOTAVAIL;
	return (0);
}

int
selectroute(struct sockaddr_in6 *dstsock, struct ip6_pktopts *opts,
    struct ip6_moptions *mopts, struct route_in6 *ro, struct ifnet **retifp,
    struct rtentry **retrt, int norouteok)
{
	int error = 0;
	struct ifnet *ifp = NULL;
	struct rtentry *rt = NULL;
	struct sockaddr_in6 *sin6_next;
	struct in6_pktinfo *pi = NULL;
	struct in6_addr *dst;

	dst = &dstsock->sin6_addr;

#if 0
	if (dstsock->sin6_addr.s6_addr32[0] == 0 &&
	    dstsock->sin6_addr.s6_addr32[1] == 0 &&
	    !IN6_IS_ADDR_LOOPBACK(&dstsock->sin6_addr)) {
		printf("in6_selectroute: strange destination %s\n",
		       ip6_sprintf(&dstsock->sin6_addr));
	} else {
		printf("in6_selectroute: destination = %s%%%d\n",
		       ip6_sprintf(&dstsock->sin6_addr),
		       dstsock->sin6_scope_id); /* for debug */
	}
#endif

	/* If the caller specify the outgoing interface explicitly, use it. */
	if (opts && (pi = opts->ip6po_pktinfo) != NULL && pi->ipi6_ifindex) {
		/* XXX boundary check is assumed to be already done. */
		ifp = ifindex2ifnet[pi->ipi6_ifindex];
		if (ifp != NULL &&
		    (norouteok || retrt == NULL ||
		     IN6_IS_ADDR_MULTICAST(dst))) {
			/*
			 * we do not have to check or get the route for
			 * multicast.
			 */
			goto done;
		} else
			goto getroute;
	}

	/*
	 * If the destination address is a multicast address and the outgoing
	 * interface for the address is specified by the caller, use it.
	 */
	if (IN6_IS_ADDR_MULTICAST(dst) &&
	    mopts != NULL && (ifp = mopts->im6o_multicast_ifp) != NULL) {
		goto done; /* we do not need a route for multicast. */
	}

  getroute:
	/*
	 * If the next hop address for the packet is specified by the caller,
	 * use it as the gateway.
	 */
	if (opts && opts->ip6po_nexthop) {
		struct route_in6 *ron;

		sin6_next = satosin6(opts->ip6po_nexthop);

		/* at this moment, we only support AF_INET6 next hops */
		if (sin6_next->sin6_family != AF_INET6) {
			error = EAFNOSUPPORT; /* or should we proceed? */
			goto done;
		}

		/*
		 * If the next hop is an IPv6 address, then the node identified
		 * by that address must be a neighbor of the sending host.
		 */
		ron = &opts->ip6po_nextroute;
		if ((ron->ro_rt &&
		    (ron->ro_rt->rt_flags & (RTF_UP | RTF_GATEWAY)) !=
		    RTF_UP) ||
		    !IN6_ARE_ADDR_EQUAL(&satosin6(&ron->ro_dst)->sin6_addr,
		    &sin6_next->sin6_addr)) {
			if (ron->ro_rt) {
				RTFREE(ron->ro_rt);
				ron->ro_rt = NULL;
			}
			*satosin6(&ron->ro_dst) = *sin6_next;
			ron->ro_tableid = 0;	/* XXX rtableid */
		}
		if (ron->ro_rt == NULL) {
			rtalloc((struct route *)ron); /* multi path case? */
			if (ron->ro_rt == NULL ||
			    (ron->ro_rt->rt_flags & RTF_GATEWAY)) {
				if (ron->ro_rt) {
					RTFREE(ron->ro_rt);
					ron->ro_rt = NULL;
				}
				error = EHOSTUNREACH;
				goto done;
			}
		}
		if (!nd6_is_addr_neighbor(sin6_next, ron->ro_rt->rt_ifp)) {
			RTFREE(ron->ro_rt);
			ron->ro_rt = NULL;
			error = EHOSTUNREACH;
			goto done;
		}
		rt = ron->ro_rt;
		ifp = rt->rt_ifp;

		/*
		 * When cloning is required, try to allocate a route to the
		 * destination so that the caller can store path MTU
		 * information.
		 */
		goto done;
	}

	/*
	 * Use a cached route if it exists and is valid, else try to allocate
	 * a new one.  Note that we should check the address family of the
	 * cached destination, in case of sharing the cache with IPv4.
	 */
	if (ro) {
		if (ro->ro_rt &&
		    (!(ro->ro_rt->rt_flags & RTF_UP) ||
		     ((struct sockaddr *)(&ro->ro_dst))->sa_family != AF_INET6 ||
		     !IN6_ARE_ADDR_EQUAL(&satosin6(&ro->ro_dst)->sin6_addr,
		     dst))) {
			RTFREE(ro->ro_rt);
			ro->ro_rt = (struct rtentry *)NULL;
		}
		if (ro->ro_rt == (struct rtentry *)NULL) {
			struct sockaddr_in6 *sa6;

			/* No route yet, so try to acquire one */
			bzero(&ro->ro_dst, sizeof(struct sockaddr_in6));
			sa6 = (struct sockaddr_in6 *)&ro->ro_dst;
			*sa6 = *dstsock;
			sa6->sin6_scope_id = 0;
			rtalloc_mpath((struct route *)ro, NULL);
		}

		/*
		 * do not care about the result if we have the nexthop
		 * explicitly specified.
		 */
		if (opts && opts->ip6po_nexthop)
			goto done;

		if (ro->ro_rt) {
			ifp = ro->ro_rt->rt_ifp;

			if (ifp == NULL) { /* can this really happen? */
				RTFREE(ro->ro_rt);
				ro->ro_rt = NULL;
			}
		}
		if (ro->ro_rt == NULL)
			error = EHOSTUNREACH;
		rt = ro->ro_rt;

		/*
		 * Check if the outgoing interface conflicts with
		 * the interface specified by ipi6_ifindex (if specified).
		 * Note that loopback interface is always okay.
		 * (this may happen when we are sending a packet to one of
		 *  our own addresses.)
		 */
		if (opts && opts->ip6po_pktinfo &&
		    opts->ip6po_pktinfo->ipi6_ifindex) {
			if (!(ifp->if_flags & IFF_LOOPBACK) &&
			    ifp->if_index !=
			    opts->ip6po_pktinfo->ipi6_ifindex) {
				error = EHOSTUNREACH;
				goto done;
			}
		}
	}

  done:
	if (ifp == NULL && rt == NULL) {
		/*
		 * This can happen if the caller did not pass a cached route
		 * nor any other hints.  We treat this case an error.
		 */
		error = EHOSTUNREACH;
	}
	if (error == EHOSTUNREACH)
		ip6stat.ip6s_noroute++;

	if (retifp != NULL)
		*retifp = ifp;
	if (retrt != NULL)
		*retrt = rt;	/* rt may be NULL */

	return (error);
}

int
in6_selectroute(struct sockaddr_in6 *dstsock, struct ip6_pktopts *opts,
    struct ip6_moptions *mopts, struct route_in6 *ro, struct ifnet **retifp,
    struct rtentry **retrt)
{

	return (selectroute(dstsock, opts, mopts, ro, retifp, retrt, 0));
}

/*
 * Default hop limit selection. The precedence is as follows:
 * 1. Hoplimit value specified via ioctl.
 * 2. (If the outgoing interface is detected) the current
 *     hop limit of the interface specified by router advertisement.
 * 3. The system default hoplimit.
*/
#define in6pcb		inpcb
#define in6p_hops	inp_hops	
int
in6_selecthlim(struct in6pcb *in6p, struct ifnet *ifp)
{
	if (in6p && in6p->in6p_hops >= 0)
		return (in6p->in6p_hops);
	else if (ifp)
		return (ND_IFINFO(ifp)->chlim);
	else
		return (ip6_defhlim);
}
#undef in6pcb
#undef in6p_hops

/*
 * generate kernel-internal form (scopeid embedded into s6_addr16[1]).
 * If the address scope of is link-local, embed the interface index in the
 * address.  The routine determines our precedence
 * between advanced API scope/interface specification and basic API
 * specification.
 *
 * this function should be nuked in the future, when we get rid of
 * embedded scopeid thing.
 *
 * XXX actually, it is over-specification to return ifp against sin6_scope_id.
 * there can be multiple interfaces that belong to a particular scope zone
 * (in specification, we have 1:N mapping between a scope zone and interfaces).
 * we may want to change the function to return something other than ifp.
 */
int
in6_embedscope(in6, sin6, in6p, ifpp)
	struct in6_addr *in6;
	const struct sockaddr_in6 *sin6;
	struct inpcb *in6p;
#define in6p_outputopts	inp_outputopts6
#define in6p_moptions	inp_moptions6
	struct ifnet **ifpp;
{
	struct ifnet *ifp = NULL;
	u_int32_t scopeid;

	*in6 = sin6->sin6_addr;
	scopeid = sin6->sin6_scope_id;
	if (ifpp)
		*ifpp = NULL;

	/*
	 * don't try to read sin6->sin6_addr beyond here, since the caller may
	 * ask us to overwrite existing sockaddr_in6
	 */

	if (IN6_IS_SCOPE_EMBED(in6)) {
		struct in6_pktinfo *pi;

		/*
		 * KAME assumption: link id == interface id
		 */

		if (in6p && in6p->in6p_outputopts &&
		    (pi = in6p->in6p_outputopts->ip6po_pktinfo) &&
		    pi->ipi6_ifindex) {
			ifp = ifindex2ifnet[pi->ipi6_ifindex];
			in6->s6_addr16[1] = htons(pi->ipi6_ifindex);
		} else if (in6p && IN6_IS_ADDR_MULTICAST(in6) &&
			   in6p->in6p_moptions &&
			   in6p->in6p_moptions->im6o_multicast_ifp) {
			ifp = in6p->in6p_moptions->im6o_multicast_ifp;
			in6->s6_addr16[1] = htons(ifp->if_index);
		} else if (scopeid) {
			/* boundary check */
			if (scopeid < 0 || if_indexlim <= scopeid ||
			    !ifindex2ifnet[scopeid])
				return ENXIO;  /* XXX EINVAL? */
			ifp = ifindex2ifnet[scopeid];
			/*XXX assignment to 16bit from 32bit variable */
			in6->s6_addr16[1] = htons(scopeid & 0xffff);
		}

		if (ifpp)
			*ifpp = ifp;
	}

	return 0;
}
#undef in6p_outputopts
#undef in6p_moptions

/*
 * generate standard sockaddr_in6 from embedded form.
 * touches sin6_addr and sin6_scope_id only.
 *
 * this function should be nuked in the future, when we get rid of
 * embedded scopeid thing.
 */
int
in6_recoverscope(struct sockaddr_in6 *sin6, const struct in6_addr *in6,
    struct ifnet *ifp)
{
	u_int32_t scopeid;

	sin6->sin6_addr = *in6;

	/*
	 * don't try to read *in6 beyond here, since the caller may
	 * ask us to overwrite existing sockaddr_in6
	 */

	sin6->sin6_scope_id = 0;
	if (IN6_IS_SCOPE_EMBED(in6)) {
		/*
		 * KAME assumption: link id == interface id
		 */
		scopeid = ntohs(sin6->sin6_addr.s6_addr16[1]);
		if (scopeid) {
			/* sanity check */
			if (scopeid < 0 || if_indexlim <= scopeid ||
			    !ifindex2ifnet[scopeid])
				return ENXIO;
			if (ifp && ifp->if_index != scopeid)
				return ENXIO;
			sin6->sin6_addr.s6_addr16[1] = 0;
			sin6->sin6_scope_id = scopeid;
		}
	}

	return 0;
}

/*
 * just clear the embedded scope identifer.
 */
void
in6_clearscope(struct in6_addr *addr)
{
	if (IN6_IS_SCOPE_EMBED(addr))
		addr->s6_addr16[1] = 0;
}
