/*	$OpenBSD: route.c,v 1.17 2000/03/22 16:50:24 itojun Exp $	*/
/*	$NetBSD: route.c,v 1.14 1996/02/13 22:00:46 christos Exp $	*/

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
 * Copyright (c) 1980, 1986, 1991, 1993
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
 *	@(#)route.c	8.2 (Berkeley) 11/15/93
 */

/*
%%% portions-copyright-nrl-95
Portions of this software are Copyright 1995-1998 by Randall Atkinson,
Ronald Lee, Daniel McDonald, Bao Phan, and Chris Winters. All Rights
Reserved. All rights under this copyright have been assigned to the US
Naval Research Laboratory (NRL). The NRL Copyright Notice and License
Agreement Version 1.1 (January 17, 1995) applies to these portions of the
software.
You should have received a copy of the license with this software. If you
didn't get a copy, you may request one from <license@ipv6.nrl.navy.mil>.
*/

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>

#include <net/if.h>
#include <net/route.h>
#include <net/raw_cb.h>

#include <netinet/in.h>
#include <netinet/in_var.h>

#ifdef NS
#include <netns/ns.h>
#endif

#ifdef IPSEC
#include <netinet/ip_ipsp.h>

extern struct ifnet encif; 
#endif

#define	SA(p) ((struct sockaddr *)(p))

int	rttrash;		/* routes not in table but not freed */
struct	sockaddr wildcard;	/* zero valued cookie for wildcard searches */

static int okaytoclone __P((u_int, int));

#ifdef IPSEC

static struct ifaddr *
encap_findgwifa(struct sockaddr *gw)
{
	return encif.if_addrlist.tqh_first;
}

#endif

void
rtable_init(table)
	void **table;
{
	struct domain *dom;
	for (dom = domains; dom; dom = dom->dom_next)
		if (dom->dom_rtattach)
			dom->dom_rtattach(&table[dom->dom_family],
			    dom->dom_rtoffset);
}

void
route_init()
{
	rn_init();	/* initialize all zeroes, all ones, mask table */
	rtable_init((void **)rt_tables);
}

void
rtalloc_noclone(ro, howstrict)
	register struct route *ro;
	int howstrict;
{
	if (ro->ro_rt && ro->ro_rt->rt_ifp && (ro->ro_rt->rt_flags & RTF_UP))
		return;		/* XXX */
	ro->ro_rt = rtalloc2(&ro->ro_dst, 1, howstrict);
}

static int
okaytoclone(flags, howstrict)
	u_int flags;
	int howstrict;
{
	if (howstrict == ALL_CLONING)
		return 1;
	if (howstrict == ONNET_CLONING && !(flags & (RTF_GATEWAY|RTF_TUNNEL)))
		return 1;
	return 0;
}

struct rtentry *
rtalloc2(dst, report,howstrict)
	register struct sockaddr *dst;
	int report,howstrict;
{
	register struct radix_node_head *rnh = rt_tables[dst->sa_family];
	register struct rtentry *rt;
	register struct radix_node *rn;
	struct rtentry *newrt = 0;
	struct rt_addrinfo info;
	int  s = splnet(), err = 0, msgtype = RTM_MISS;

	if (rnh && (rn = rnh->rnh_matchaddr((caddr_t)dst, rnh)) &&
	    ((rn->rn_flags & RNF_ROOT) == 0)) {
		newrt = rt = (struct rtentry *)rn;
		if (report && (rt->rt_flags & RTF_CLONING) &&
		    okaytoclone(rt->rt_flags, howstrict)) {
			err = rtrequest(RTM_RESOLVE, dst, SA(0), SA(0), 0,
			    &newrt);
			if (err) {
				newrt = rt;
				rt->rt_refcnt++;
				goto miss;
			}
			if ((rt = newrt) && (rt->rt_flags & RTF_XRESOLVE)) {
				msgtype = RTM_RESOLVE;
				goto miss;
			}
		} else
			rt->rt_refcnt++;
	} else {
		rtstat.rts_unreach++;
miss:		if (report) {
			bzero((caddr_t)&info, sizeof(info));
			info.rti_info[RTAX_DST] = dst;
			rt_missmsg(msgtype, &info, 0, err);
		}
	}
	splx(s);
	return (newrt);
}

/*
 * Packet routing routines.
 */
void
rtalloc(ro)
	register struct route *ro;
{
	if (ro->ro_rt && ro->ro_rt->rt_ifp && (ro->ro_rt->rt_flags & RTF_UP))
		return;				 /* XXX */
	ro->ro_rt = rtalloc1(&ro->ro_dst, 1);
}

struct rtentry *
rtalloc1(dst, report)
	register struct sockaddr *dst;
	int report;
{
	register struct radix_node_head *rnh = rt_tables[dst->sa_family];
	register struct rtentry *rt;
	register struct radix_node *rn;
	struct rtentry *newrt = 0;
	struct rt_addrinfo info;
	int  s = splsoftnet(), err = 0, msgtype = RTM_MISS;

	if (rnh && (rn = rnh->rnh_matchaddr((caddr_t)dst, rnh)) &&
	    ((rn->rn_flags & RNF_ROOT) == 0)) {
		newrt = rt = (struct rtentry *)rn;
		if (report && (rt->rt_flags & RTF_CLONING)) {
			err = rtrequest(RTM_RESOLVE, dst, SA(NULL),
			    SA(NULL), 0, &newrt);
			if (err) {
				newrt = rt;
				rt->rt_refcnt++;
				goto miss;
			}
			if ((rt = newrt) && (rt->rt_flags & RTF_XRESOLVE)) {
				msgtype = RTM_RESOLVE;
				goto miss;
			}
		} else
			rt->rt_refcnt++;
	} else {
		if (dst->sa_family != PF_KEY)
		        rtstat.rts_unreach++;
	/*
	 * IP encapsulation does lots of lookups where we don't need nor want
	 * the RTM_MISSes that would be generated.  It causes RTM_MISS storms
	 * sent upward breaking user-level routing queries.
	 */
	miss:	if (report && dst->sa_family != PF_KEY) {
			bzero((caddr_t)&info, sizeof(info));
			info.rti_info[RTAX_DST] = dst;
			rt_missmsg(msgtype, &info, 0, err);
		}
	}
	splx(s);
	return (newrt);
}

void
rtfree(rt)
	register struct rtentry *rt;
{
	register struct ifaddr *ifa;

	if (rt == NULL)
		panic("rtfree");
	rt->rt_refcnt--;
	if (rt->rt_refcnt <= 0 && (rt->rt_flags & RTF_UP) == 0) {
		if (rt->rt_nodes->rn_flags & (RNF_ACTIVE | RNF_ROOT))
			panic ("rtfree 2");
		rttrash--;
		if (rt->rt_refcnt < 0) {
			printf("rtfree: %p not freed (neg refs)\n", rt);
			return;
		}
		rt_timer_remove_all(rt);
		ifa = rt->rt_ifa;
		if (ifa)
			IFAFREE(ifa);
		Free(rt_key(rt));
		Free(rt);
	}
}

void
ifafree(ifa)
	register struct ifaddr *ifa;
{
	if (ifa == NULL)
		panic("ifafree");
	if (ifa->ifa_refcnt == 0)
		free(ifa, M_IFADDR);
	else
		ifa->ifa_refcnt--;
}

/*
 * Force a routing table entry to the specified
 * destination to go through the given gateway.
 * Normally called as a result of a routing redirect
 * message from the network layer.
 *
 * N.B.: must be called at splsoftnet
 */
void
rtredirect(dst, gateway, netmask, flags, src, rtp)
	struct sockaddr *dst, *gateway, *netmask, *src;
	int flags;
	struct rtentry **rtp;
{
	register struct rtentry *rt;
	int error = 0;
	u_int32_t *stat = NULL;
	struct rt_addrinfo info;
	struct ifaddr *ifa;

	/* verify the gateway is directly reachable */
	if ((ifa = ifa_ifwithnet(gateway)) == NULL) {
		error = ENETUNREACH;
		goto out;
	}
	rt = rtalloc1(dst, 0);
	/*
	 * If the redirect isn't from our current router for this dst,
	 * it's either old or wrong.  If it redirects us to ourselves,
	 * we have a routing loop, perhaps as a result of an interface
	 * going down recently.
	 */
#define	equal(a1, a2) (bcmp((caddr_t)(a1), (caddr_t)(a2), (a1)->sa_len) == 0)
	if (!(flags & RTF_DONE) && rt &&
	     (!equal(src, rt->rt_gateway) || rt->rt_ifa != ifa))
		error = EINVAL;
	else if (ifa_ifwithaddr(gateway) != NULL)
		error = EHOSTUNREACH;
	if (error)
		goto done;
	/*
	 * Create a new entry if we just got back a wildcard entry
	 * or the the lookup failed.  This is necessary for hosts
	 * which use routing redirects generated by smart gateways
	 * to dynamically build the routing tables.
	 */
	if ((rt == NULL) || (rt_mask(rt) && rt_mask(rt)->sa_len < 2))
		goto create;
	/*
	 * Don't listen to the redirect if it's
	 * for a route to an interface. 
	 */
	if (rt->rt_flags & RTF_GATEWAY) {
		if (((rt->rt_flags & RTF_HOST) == 0) && (flags & RTF_HOST)) {
			/*
			 * Changing from route to net => route to host.
			 * Create new route, rather than smashing route to net.
			 */
		create:
			flags |=  RTF_GATEWAY | RTF_DYNAMIC;
			error = rtrequest((int)RTM_ADD, dst, gateway,
				    netmask, flags,
				    (struct rtentry **)0);
			stat = &rtstat.rts_dynamic;
		} else {
			/*
			 * Smash the current notion of the gateway to
			 * this destination.  Should check about netmask!!!
			 */
			rt->rt_flags |= RTF_MODIFIED;
			flags |= RTF_MODIFIED;
			stat = &rtstat.rts_newgateway;
			rt_setgate(rt, rt_key(rt), gateway);
		}
	} else
		error = EHOSTUNREACH;
done:
	if (rt) {
		if (rtp && !error)
			*rtp = rt;
		else
			rtfree(rt);
	}
out:
	if (error)
		rtstat.rts_badredirect++;
	else if (stat != NULL)
		(*stat)++;
	bzero((caddr_t)&info, sizeof(info));
	info.rti_info[RTAX_DST] = dst;
	info.rti_info[RTAX_GATEWAY] = gateway;
	info.rti_info[RTAX_NETMASK] = netmask;
	info.rti_info[RTAX_AUTHOR] = src;
	rt_missmsg(RTM_REDIRECT, &info, flags, error);
}

/*
* Routing table ioctl interface.
*/
int
rtioctl(req, data, p)
	u_long req;
	caddr_t data;
	struct proc *p;
{
	return (EOPNOTSUPP);
}

struct ifaddr *
ifa_ifwithroute(flags, dst, gateway)
	int flags;
	struct sockaddr	*dst, *gateway;
{
	register struct ifaddr *ifa;

#ifdef IPSEC
	/*
	 * If the destination is a PF_KEY address, we'll look
	 * for the existence of a encap interface number or address
	 * in the options list of the gateway. By default, we'll return
	 * enc0.
	 */
	if (dst && (dst->sa_family == PF_KEY))
		return encap_findgwifa(gateway);
#endif

	if ((flags & RTF_GATEWAY) == 0) {
		/*
		 * If we are adding a route to an interface,
		 * and the interface is a pt to pt link
		 * we should search for the destination
		 * as our clue to the interface.  Otherwise
		 * we can use the local address.
		 */
		ifa = NULL;
		if (flags & RTF_HOST) 
			ifa = ifa_ifwithdstaddr(dst);
		if (ifa == NULL)
			ifa = ifa_ifwithaddr(gateway);
	} else {
		/*
		 * If we are adding a route to a remote net
		 * or host, the gateway may still be on the
		 * other end of a pt to pt link.
		 */
		ifa = ifa_ifwithdstaddr(gateway);
	}
	if (ifa == NULL)
		ifa = ifa_ifwithnet(gateway);
	if (ifa == NULL) {
		struct rtentry *rt = rtalloc1(gateway, 0);
		if (rt == NULL)
			return (NULL);
		rt->rt_refcnt--;
		/* The gateway must be local if the same address family. */
		if (!(flags & RTF_TUNNEL) && (rt->rt_flags & RTF_GATEWAY) && 
		    rt_key(rt)->sa_family == dst->sa_family)
			return (0);
		if ((ifa = rt->rt_ifa) == NULL)
			return (NULL);
	}
	if (ifa->ifa_addr->sa_family != dst->sa_family) {
		struct ifaddr *oifa = ifa;
		ifa = ifaof_ifpforaddr(dst, ifa->ifa_ifp);
		if (ifa == NULL)
			ifa = oifa;
	}
	return (ifa);
}

#define ROUNDUP(a) (a>0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))

int
rtrequest(req, dst, gateway, netmask, flags, ret_nrt)
	int req, flags;
	struct sockaddr *dst, *gateway, *netmask;
	struct rtentry **ret_nrt;
{
	int s = splsoftnet(); int error = 0;
	register struct rtentry *rt;
	register struct radix_node *rn;
	register struct radix_node_head *rnh;
	struct ifaddr *ifa;
	struct sockaddr *ndst;
#define senderr(x) { error = x ; goto bad; }

	if ((rnh = rt_tables[dst->sa_family]) == 0)
		senderr(EAFNOSUPPORT);
	if (flags & RTF_HOST)
		netmask = 0;
	switch (req) {
	case RTM_DELETE:
		if ((rn = rnh->rnh_deladdr(dst, netmask, rnh)) == NULL)
			senderr(ESRCH);
		if (rn->rn_flags & (RNF_ACTIVE | RNF_ROOT))
			panic ("rtrequest delete");
		rt = (struct rtentry *)rn;
		rt->rt_flags &= ~RTF_UP;
		if (rt->rt_gwroute) {
			rt = rt->rt_gwroute; RTFREE(rt);
			(rt = (struct rtentry *)rn)->rt_gwroute = NULL;
		}
		if ((ifa = rt->rt_ifa) && ifa->ifa_rtrequest)
			ifa->ifa_rtrequest(RTM_DELETE, rt, SA(NULL));
		rttrash++;
		if (ret_nrt)
			*ret_nrt = rt;
		else if (rt->rt_refcnt <= 0) {
			rt->rt_refcnt++;
			rtfree(rt);
		}
		break;

	case RTM_RESOLVE:
		if (ret_nrt == NULL || (rt = *ret_nrt) == NULL)
			senderr(EINVAL);
		ifa = rt->rt_ifa;
		flags = rt->rt_flags & ~RTF_CLONING;
		gateway = rt->rt_gateway;
		if ((netmask = rt->rt_genmask) == NULL)
			flags |= RTF_HOST;
		goto makeroute;

	case RTM_ADD:
		if ((ifa = ifa_ifwithroute(flags, dst, gateway)) == NULL)
			senderr(ENETUNREACH);
	makeroute:
		R_Malloc(rt, struct rtentry *, sizeof(*rt));
		if (rt == NULL)
			senderr(ENOBUFS);
		Bzero(rt, sizeof(*rt));
		rt->rt_flags = RTF_UP | flags;
		LIST_INIT(&rt->rt_timer);
		if (rt_setgate(rt, dst, gateway)) {
			Free(rt);
			senderr(ENOBUFS);
		}
		ndst = rt_key(rt);
		if (netmask) {
			rt_maskedcopy(dst, ndst, netmask);
		} else
			Bcopy(dst, ndst, dst->sa_len);
if (!rt->rt_rmx.rmx_mtu && !(rt->rt_rmx.rmx_locks & RTV_MTU)) { /* XXX */
  rt->rt_rmx.rmx_mtu = ifa->ifa_ifp->if_mtu;
}
		rn = rnh->rnh_addaddr((caddr_t)ndst, (caddr_t)netmask,
					rnh, rt->rt_nodes);
		if (rn == NULL) {
			if (rt->rt_gwroute)
				rtfree(rt->rt_gwroute);
			Free(rt_key(rt));
			Free(rt);
			senderr(EEXIST);
		}
		ifa->ifa_refcnt++;
		rt->rt_ifa = ifa;
		rt->rt_ifp = ifa->ifa_ifp;
		if (req == RTM_RESOLVE) {
			/*
			 * Copy both metrics and a back pointer to the cloned
			 * route's parent.
			 */
			rt->rt_rmx = (*ret_nrt)->rt_rmx; /* copy metrics */
			rt->rt_parent = *ret_nrt;	 /* Back ptr. to parent. */
		}
		if (ifa->ifa_rtrequest)
			ifa->ifa_rtrequest(req, rt, SA(ret_nrt ? *ret_nrt : NULL));
		if (ret_nrt) {
			*ret_nrt = rt;
			rt->rt_refcnt++;
		}
#ifdef INET6
		/* If we have a v4_in_v4 or a v4_in_v6 tunnel route
		 * then do some tunnel state (e.g. security state)
		 * initialization.
		 *
		 * Since IPV6 packets flow down this path, we don't
		 * want it using ipv4_tunnelsetup(rt) (since they
		 * have their own ipv6_tunnel_parent/child()
		 * routines which are called ipv6_rtrequest().)
		 *
		 * Thus, we check to see if the packet is to a v4
		 * destination.
		 */
		if (dst->sa_family == AF_INET && (rt->rt_flags & RTF_TUNNEL))
			ipv4_tunnelsetup(rt);
#endif /* INET6 */
		break;
	}
bad:
	splx(s);
	return (error);
}

/*
 * Set up any tunnel states (e.g. security) information
 * for v4_in_v4 or v4_in_v6 tunnel routes.
 */
void
ipv4_tunnelsetup(rt)
	register struct rtentry *rt;
{
	/* XXX */
}

int
rt_setgate(rt0, dst, gate)
	struct rtentry *rt0;
	struct sockaddr *dst, *gate;
{
	caddr_t new, old;
	int dlen = ROUNDUP(dst->sa_len), glen = ROUNDUP(gate->sa_len);
	register struct rtentry *rt = rt0;

	if (rt->rt_gateway == NULL || glen > ROUNDUP(rt->rt_gateway->sa_len)) {
		old = (caddr_t)rt_key(rt);
		R_Malloc(new, caddr_t, dlen + glen);
		if (new == NULL)
			return 1;
		rt->rt_nodes->rn_key = new;
	} else {
		new = rt->rt_nodes->rn_key;
		old = NULL;
	}
	Bcopy(gate, (rt->rt_gateway = (struct sockaddr *)(new + dlen)), glen);
	if (old) {
		Bcopy(dst, new, dlen);
		Free(old);
	}
	if (rt->rt_gwroute != NULL) {
		rt = rt->rt_gwroute;
		RTFREE(rt);
		rt = rt0;
		rt->rt_gwroute = NULL;
	}
	if (rt->rt_flags & RTF_GATEWAY) {
		rt->rt_gwroute = rtalloc1(gate, 1);
	}
	return 0;
}

void
rt_maskedcopy(src, dst, netmask)
	struct sockaddr *src, *dst, *netmask;
{
	register u_char *cp1 = (u_char *)src;
	register u_char *cp2 = (u_char *)dst;
	register u_char *cp3 = (u_char *)netmask;
	u_char *cplim = cp2 + *cp3;
	u_char *cplim2 = cp2 + *cp1;

	*cp2++ = *cp1++; *cp2++ = *cp1++; /* copies sa_len & sa_family */
	cp3 += 2;
	if (cplim > cplim2)
		cplim = cplim2;
	while (cp2 < cplim)
		*cp2++ = *cp1++ & *cp3++;
	if (cp2 < cplim2)
		bzero((caddr_t)cp2, (unsigned)(cplim2 - cp2));
}

/*
 * Set up a routing table entry, normally
 * for an interface.
 */
int
rtinit(ifa, cmd, flags)
	register struct ifaddr *ifa;
	int cmd, flags;
{
	register struct rtentry *rt;
	register struct sockaddr *dst;
	register struct sockaddr *deldst;
	struct mbuf *m = NULL;
	struct rtentry *nrt = NULL;
	int error;

	dst = flags & RTF_HOST ? ifa->ifa_dstaddr : ifa->ifa_addr;
	if (cmd == RTM_DELETE) {
		if ((flags & RTF_HOST) == 0 && ifa->ifa_netmask) {
			m = m_get(M_DONTWAIT, MT_SONAME);
			if (m == NULL)
				return(ENOBUFS);
			deldst = mtod(m, struct sockaddr *);
			rt_maskedcopy(dst, deldst, ifa->ifa_netmask);
			dst = deldst;
		}
		if ((rt = rtalloc1(dst, 0)) != NULL) {
			rt->rt_refcnt--;
			if (rt->rt_ifa != ifa) {
				if (m != NULL)
					(void) m_free(m);
				return (flags & RTF_HOST ? EHOSTUNREACH
							: ENETUNREACH);
			}
		}
	}
	error = rtrequest(cmd, dst, ifa->ifa_addr, ifa->ifa_netmask,
			flags | ifa->ifa_flags, &nrt);
	if (m != NULL)
		(void) m_free(m);
	if (cmd == RTM_DELETE && error == 0 && (rt = nrt) != NULL) {
		rt_newaddrmsg(cmd, ifa, error, nrt);
		if (rt->rt_refcnt <= 0) {
			rt->rt_refcnt++;
			rtfree(rt);
		}
	}
	if (cmd == RTM_ADD && error == 0 && (rt = nrt) != NULL) {
		rt->rt_refcnt--;
#ifdef INET6
		/* Initialize Path MTU for IPv6 interface route */
		if (ifa->ifa_addr->sa_family == AF_INET6 &&
		    !rt->rt_rmx.rmx_mtu)
			rt->rt_rmx.rmx_mtu = ifa->ifa_ifp->if_mtu;
#endif /* INET6 */
		if (rt->rt_ifa != ifa) {
			printf("rtinit: wrong ifa (%p) was (%p)\n",
			       ifa, rt->rt_ifa);
			if (rt->rt_ifa->ifa_rtrequest)
				rt->rt_ifa->ifa_rtrequest(RTM_DELETE, rt,
				    SA(NULL));
			IFAFREE(rt->rt_ifa);
			rt->rt_ifa = ifa;
			rt->rt_ifp = ifa->ifa_ifp;
			rt->rt_rmx.rmx_mtu = ifa->ifa_ifp->if_mtu;	/*XXX*/
			ifa->ifa_refcnt++;
			if (ifa->ifa_rtrequest)
				ifa->ifa_rtrequest(RTM_ADD, rt, SA(NULL));
		}
		rt_newaddrmsg(cmd, ifa, error, nrt);
	}
	return (error);
}

/*
 * Route timer routines.  These routes allow functions to be called
 * for various routes at any time.  This is useful in supporting
 * path MTU discovery and redirect route deletion.
 *
 * This is similar to some BSDI internal functions, but it provides
 * for multiple queues for efficiency's sake...
 */

LIST_HEAD(, rttimer_queue) rttimer_queue_head;
static int rt_init_done = 0;

#define RTTIMER_CALLOUT(r)	{				\
	if (r->rtt_func != NULL) {				\
		(*r->rtt_func)(r->rtt_rt, r);			\
	} else {						\
		rtrequest((int) RTM_DELETE,			\
			  (struct sockaddr *)rt_key(r->rtt_rt),	\
			  0, 0, 0, 0);				\
	}							\
}

/* 
 * Some subtle order problems with domain initialization mean that
 * we cannot count on this being run from rt_init before various
 * protocol initializations are done.  Therefore, we make sure
 * that this is run when the first queue is added...
 */

void	 
rt_timer_init()
{
	assert(rt_init_done == 0);

#if 0
	pool_init(&rttimer_pool, sizeof(struct rttimer), 0, 0, 0, "rttmrpl",
	    0, NULL, NULL, M_RTABLE);
#endif

	LIST_INIT(&rttimer_queue_head);
	timeout(rt_timer_timer, NULL, hz);  /* every second */
	rt_init_done = 1;
}

struct rttimer_queue *
rt_timer_queue_create(timeout)
	u_int	timeout;
{
	struct rttimer_queue *rtq;

	if (rt_init_done == 0)
		rt_timer_init();

	R_Malloc(rtq, struct rttimer_queue *, sizeof *rtq);
	if (rtq == NULL)
		return (NULL);		

	rtq->rtq_timeout = timeout;
	TAILQ_INIT(&rtq->rtq_head);
	LIST_INSERT_HEAD(&rttimer_queue_head, rtq, rtq_link);

	return (rtq);
}

void
rt_timer_queue_change(rtq, timeout)
	struct rttimer_queue *rtq;
	long timeout;
{

	rtq->rtq_timeout = timeout;
}


void
rt_timer_queue_destroy(rtq, destroy)
	struct rttimer_queue *rtq;
	int destroy;
{
	struct rttimer *r;

	while ((r = TAILQ_FIRST(&rtq->rtq_head)) != NULL) {
		LIST_REMOVE(r, rtt_link);
		TAILQ_REMOVE(&rtq->rtq_head, r, rtt_next);
		if (destroy)
			RTTIMER_CALLOUT(r);
#if 0
		pool_put(&rttimer_pool, r);
#else
		free(r, M_RTABLE);
#endif
	}

	LIST_REMOVE(rtq, rtq_link);

	/*
	 * Caller is responsible for freeing the rttimer_queue structure.
	 */
}

void     
rt_timer_remove_all(rt)
	struct rtentry *rt;
{
	struct rttimer *r;

	while ((r = LIST_FIRST(&rt->rt_timer)) != NULL) {
		LIST_REMOVE(r, rtt_link);
		TAILQ_REMOVE(&r->rtt_queue->rtq_head, r, rtt_next);
#if 0
		pool_put(&rttimer_pool, r);
#else
		free(r, M_RTABLE);
#endif
	}
}

int      
rt_timer_add(rt, func, queue)
	struct rtentry *rt;
	void(*func) __P((struct rtentry *, struct rttimer *));
	struct rttimer_queue *queue;
{
	struct rttimer *r;
	long current_time;
	int s;

	s = splclock();
	current_time = mono_time.tv_sec;
	splx(s);

	/*
	 * If there's already a timer with this action, destroy it before
	 * we add a new one.
	 */
	for (r = LIST_FIRST(&rt->rt_timer); r != NULL;
	     r = LIST_NEXT(r, rtt_link)) {
		if (r->rtt_func == func) {
			LIST_REMOVE(r, rtt_link);
			TAILQ_REMOVE(&r->rtt_queue->rtq_head, r, rtt_next);
#if 0
			pool_put(&rttimer_pool, r);
#else
			free(r, M_RTABLE);
#endif
			break;  /* only one per list, so we can quit... */
		}
	}

#if 0
	r = pool_get(&rttimer_pool, PR_NOWAIT);
#else
	r = (struct rttimer *)malloc(sizeof(*r), M_RTABLE, M_NOWAIT);
#endif
	if (r == NULL)
		return (ENOBUFS);

	r->rtt_rt = rt;
	r->rtt_time = current_time;
	r->rtt_func = func;
	r->rtt_queue = queue;
	LIST_INSERT_HEAD(&rt->rt_timer, r, rtt_link);
	TAILQ_INSERT_TAIL(&queue->rtq_head, r, rtt_next);
	
	return (0);
}

/* ARGSUSED */
void
rt_timer_timer(arg)
	void *arg;
{
	struct rttimer_queue *rtq;
	struct rttimer *r;
	long current_time;
	int s;

	s = splclock();
	current_time = mono_time.tv_sec;
	splx(s);

	s = splsoftnet();
	for (rtq = LIST_FIRST(&rttimer_queue_head); rtq != NULL; 
	     rtq = LIST_NEXT(rtq, rtq_link)) {
		while ((r = TAILQ_FIRST(&rtq->rtq_head)) != NULL &&
		    (r->rtt_time + rtq->rtq_timeout) < current_time) {
			LIST_REMOVE(r, rtt_link);
			TAILQ_REMOVE(&rtq->rtq_head, r, rtt_next);
			RTTIMER_CALLOUT(r);
#if 0
			pool_put(&rttimer_pool, r);
#else
			free(r, M_RTABLE);
#endif
		}
	}
	splx(s);

	timeout(rt_timer_timer, NULL, hz);  /* every second */
}
