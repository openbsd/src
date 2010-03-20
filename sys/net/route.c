/*	$OpenBSD: route.c,v 1.116 2010/03/20 10:43:11 blambert Exp $	*/
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
 *	@(#)route.c	8.2 (Berkeley) 11/15/93
 */

/*
 *	@(#)COPYRIGHT	1.1 (NRL) 17 January 1995
 * 
 * NRL grants permission for redistribution and use in source and binary
 * forms, with or without modification, of the software and documentation
 * created at NRL provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgements:
 * 	This product includes software developed by the University of
 * 	California, Berkeley and its contributors.
 * 	This product includes software developed at the Information
 * 	Technology Division, US Naval Research Laboratory.
 * 4. Neither the name of the NRL nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THE SOFTWARE PROVIDED BY NRL IS PROVIDED BY NRL AND CONTRIBUTORS ``AS
 * IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL NRL OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * The views and conclusions contained in the software and documentation
 * are those of the authors and should not be interpreted as representing
 * official policies, either expressed or implied, of the US Naval
 * Research Laboratory (NRL).
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
#include <sys/queue.h>
#include <sys/pool.h>

#include <net/if.h>
#include <net/route.h>
#include <net/raw_cb.h>

#include <netinet/in.h>
#include <netinet/in_var.h>

#ifdef MPLS
#include <netmpls/mpls.h>
#endif

#ifdef IPSEC
#include <netinet/ip_ipsp.h>
#include <net/if_enc.h>

struct ifaddr	*encap_findgwifa(struct sockaddr *);
#endif

#define	SA(p) ((struct sockaddr *)(p))

struct	route_cb	   route_cb;
struct	rtstat		   rtstat;
struct	radix_node_head	***rt_tables;
u_int8_t		   af2rtafidx[AF_MAX+1];
u_int8_t		   rtafidx_max;
u_int			   rtbl_id_max = 0;
u_int			  *rt_tab2dom;	/* rt table to domain lookup table */

int			rttrash;	/* routes not in table but not freed */

struct pool		rtentry_pool;	/* pool for rtentry structures */
struct pool		rttimer_pool;	/* pool for rttimer structures */

int	rtable_init(struct radix_node_head ***);
int	okaytoclone(u_int, int);
int	rtflushclone1(struct radix_node *, void *);
void	rtflushclone(struct radix_node_head *, struct rtentry *);
int	rt_if_remove_rtdelete(struct radix_node *, void *);

#define	LABELID_MAX	50000

struct rt_label {
	TAILQ_ENTRY(rt_label)	rtl_entry;
	char			rtl_name[RTLABEL_LEN];
	u_int16_t		rtl_id;
	int			rtl_ref;
};

TAILQ_HEAD(rt_labels, rt_label)	rt_labels = TAILQ_HEAD_INITIALIZER(rt_labels);

#ifdef IPSEC
struct ifaddr *
encap_findgwifa(struct sockaddr *gw)
{
	return (TAILQ_FIRST(&encif[0].sc_if.if_addrlist));
}
#endif

int
rtable_init(struct radix_node_head ***table)
{
	void		**p;
	struct domain	 *dom;

	if ((p = malloc(sizeof(void *) * (rtafidx_max + 1), M_RTABLE,
	    M_NOWAIT|M_ZERO)) == NULL)
		return (-1);

	/* 2nd pass: attach */
	for (dom = domains; dom != NULL; dom = dom->dom_next)
		if (dom->dom_rtattach)
			dom->dom_rtattach(&p[af2rtafidx[dom->dom_family]],
			    dom->dom_rtoffset);

	*table = (struct radix_node_head **)p;
	return (0);
}

void
route_init()
{
	struct domain	 *dom;

	pool_init(&rtentry_pool, sizeof(struct rtentry), 0, 0, 0, "rtentpl",
	    NULL);
	rn_init();	/* initialize all zeroes, all ones, mask table */

	bzero(af2rtafidx, sizeof(af2rtafidx));
	rtafidx_max = 1;	/* must have NULL at index 0, so start at 1 */

	/* find out how many tables to allocate */
	for (dom = domains; dom != NULL; dom = dom->dom_next)
		if (dom->dom_rtattach)
			af2rtafidx[dom->dom_family] = rtafidx_max++;

	if (rtable_add(0) == -1)
		panic("route_init rtable_add");
}

int
rtable_add(u_int id)	/* must be called at splsoftnet */
{
	void	*p, *q;

	if (id > RT_TABLEID_MAX)
		return (-1);

	if (id == 0 || id > rtbl_id_max) {
		size_t	newlen = sizeof(void *) * (id+1);
		size_t	newlen2 = sizeof(u_int) * (id+1);

		if ((p = malloc(newlen, M_RTABLE, M_NOWAIT|M_ZERO)) == NULL)
			return (-1);
		if ((q = malloc(newlen2, M_RTABLE, M_NOWAIT|M_ZERO)) == NULL) {
			free(p, M_RTABLE);
			return (-1);
		}
		if (rt_tables) {
			bcopy(rt_tables, p, sizeof(void *) * (rtbl_id_max+1));
			bcopy(rt_tab2dom, q, sizeof(u_int) * (rtbl_id_max+1));
			free(rt_tables, M_RTABLE);
			free(rt_tab2dom, M_RTABLE);
		}
		rt_tables = p;
		rt_tab2dom = q;
		rtbl_id_max = id;
	}

	if (rt_tables[id] != NULL)	/* already exists */
		return (-1);

	rt_tab2dom[id] = 0;	/* use main table/domain by default */
	return (rtable_init(&rt_tables[id]));
}

u_int
rtable_l2(u_int id)
{
	if (id > rtbl_id_max)
		return (0);
	return (rt_tab2dom[id]);
}

void
rtable_l2set(u_int id, u_int parent)
{
	if (!rtable_exists(id) || !rtable_exists(parent))
		return;
	rt_tab2dom[id] = parent;
}

int
rtable_exists(u_int id)	/* verify table with that ID exists */
{
	if (id > RT_TABLEID_MAX)
		return (0);

	if (id > rtbl_id_max)
		return (0);

	if (rt_tables[id] == NULL)
		return (0);

	return (1);
}

#include "pf.h"
#if NPF > 0
void
rtalloc_noclone(struct route *ro, int howstrict)
{
	if (ro->ro_rt && ro->ro_rt->rt_ifp && (ro->ro_rt->rt_flags & RTF_UP))
		return;		/* XXX */
	ro->ro_rt = rtalloc2(&ro->ro_dst, 1, howstrict);
}

int
okaytoclone(u_int flags, int howstrict)
{
	if (howstrict == ALL_CLONING)
		return (1);
	if (howstrict == ONNET_CLONING && !(flags & RTF_GATEWAY))
		return (1);
	return (0);
}

struct rtentry *
rtalloc2(struct sockaddr *dst, int report, int howstrict)
{
	struct radix_node_head	*rnh;
	struct rtentry		*rt;
	struct radix_node	*rn;
	struct rtentry		*newrt = 0;
	struct rt_addrinfo	 info;
	int			 s = splnet(), err = 0, msgtype = RTM_MISS;

	bzero(&info, sizeof(info));
	info.rti_info[RTAX_DST] = dst;

	rnh = rt_gettable(dst->sa_family, 0);
	if (rnh && (rn = rnh->rnh_matchaddr((caddr_t)dst, rnh)) &&
	    ((rn->rn_flags & RNF_ROOT) == 0)) {
		newrt = rt = (struct rtentry *)rn;
		if (report && (rt->rt_flags & RTF_CLONING) &&
		    okaytoclone(rt->rt_flags, howstrict)) {
			err = rtrequest1(RTM_RESOLVE, &info, RTP_DEFAULT,
			    &newrt, 0);
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
miss:
		if (report) {
			rt_missmsg(msgtype, &info, 0, NULL, err, 0);
		}
	}
	splx(s);
	return (newrt);
}
#endif /* NPF > 0 */

/*
 * Packet routing routines.
 */
void
rtalloc(struct route *ro)
{
	if (ro->ro_rt && ro->ro_rt->rt_ifp && (ro->ro_rt->rt_flags & RTF_UP))
		return;				 /* XXX */
	ro->ro_rt = rtalloc1(&ro->ro_dst, 1, 0);
}

struct rtentry *
rtalloc1(struct sockaddr *dst, int report, u_int tableid)
{
	struct radix_node_head	*rnh;
	struct rtentry		*rt;
	struct radix_node	*rn;
	struct rtentry		*newrt = 0;
	struct rt_addrinfo	 info;
	int			 s = splsoftnet(), err = 0, msgtype = RTM_MISS;

	bzero(&info, sizeof(info));
	info.rti_info[RTAX_DST] = dst;

	rnh = rt_gettable(dst->sa_family, tableid);
	if (rnh && (rn = rnh->rnh_matchaddr((caddr_t)dst, rnh)) &&
	    ((rn->rn_flags & RNF_ROOT) == 0)) {
		newrt = rt = (struct rtentry *)rn;
		if (report && (rt->rt_flags & RTF_CLONING)) {
			err = rtrequest1(RTM_RESOLVE, &info, RTP_DEFAULT,
			    &newrt, tableid);
			if (err) {
				newrt = rt;
				rt->rt_refcnt++;
				goto miss;
			}
			if ((rt = newrt) && (rt->rt_flags & RTF_XRESOLVE)) {
				msgtype = RTM_RESOLVE;
				goto miss;
			}
			/* Inform listeners of the new route */
			bzero(&info, sizeof(info));
			info.rti_info[RTAX_DST] = rt_key(rt);
			info.rti_info[RTAX_NETMASK] = rt_mask(rt);
			info.rti_info[RTAX_GATEWAY] = rt->rt_gateway;
			if (rt->rt_ifp != NULL) {
				info.rti_info[RTAX_IFP] =
				    TAILQ_FIRST(&rt->rt_ifp->if_addrlist)->ifa_addr;
				info.rti_info[RTAX_IFA] = rt->rt_ifa->ifa_addr;
			}
			rt_missmsg(RTM_ADD, &info, rt->rt_flags,
			    rt->rt_ifp, 0, tableid);
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
miss:
		if (report && dst->sa_family != PF_KEY) {
			bzero((caddr_t)&info, sizeof(info));
			info.rti_info[RTAX_DST] = dst;
			rt_missmsg(msgtype, &info, 0, NULL, err, tableid);
		}
	}
	splx(s);
	return (newrt);
}

void
rtfree(struct rtentry *rt)
{
	struct ifaddr	*ifa;

	if (rt == NULL)
		panic("rtfree");

	rt->rt_refcnt--;

	if (rt->rt_refcnt <= 0 && (rt->rt_flags & RTF_UP) == 0) {
		if (rt->rt_refcnt == 0 && (rt->rt_nodes->rn_flags & RNF_ACTIVE))
			return; /* route still active but currently down */
		if (rt->rt_nodes->rn_flags & (RNF_ACTIVE | RNF_ROOT))
			panic("rtfree 2");
		rttrash--;
		if (rt->rt_refcnt < 0) {
			printf("rtfree: %p not freed (neg refs)\n", rt);
			return;
		}
		rt_timer_remove_all(rt);
		ifa = rt->rt_ifa;
		if (ifa)
			IFAFREE(ifa);
		rtlabel_unref(rt->rt_labelid);
#ifdef MPLS
		if (rt->rt_flags & RTF_MPLS)
			free(rt->rt_llinfo, M_TEMP);
#endif
		Free(rt_key(rt));
		pool_put(&rtentry_pool, rt);
	}
}

void
ifafree(struct ifaddr *ifa)
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
rtredirect(struct sockaddr *dst, struct sockaddr *gateway,
    struct sockaddr *netmask, int flags, struct sockaddr *src,
    struct rtentry **rtp, u_int rdomain)
{
	struct rtentry		*rt;
	int			 error = 0;
	u_int32_t		*stat = NULL;
	struct rt_addrinfo	 info;
	struct ifaddr		*ifa;
	struct ifnet		*ifp = NULL;

	splsoftassert(IPL_SOFTNET);

	/* verify the gateway is directly reachable */
	if ((ifa = ifa_ifwithnet(gateway, rdomain)) == NULL) {
		error = ENETUNREACH;
		goto out;
	}
	ifp = ifa->ifa_ifp;
	rt = rtalloc1(dst, 0, rdomain);
	/*
	 * If the redirect isn't from our current router for this dst,
	 * it's either old or wrong.  If it redirects us to ourselves,
	 * we have a routing loop, perhaps as a result of an interface
	 * going down recently.
	 */
#define	equal(a1, a2) \
	((a1)->sa_len == (a2)->sa_len && \
	 bcmp((caddr_t)(a1), (caddr_t)(a2), (a1)->sa_len) == 0)
	if (!(flags & RTF_DONE) && rt &&
	     (!equal(src, rt->rt_gateway) || rt->rt_ifa != ifa))
		error = EINVAL;
	else if (ifa_ifwithaddr(gateway, rdomain) != NULL)
		error = EHOSTUNREACH;
	if (error)
		goto done;
	/*
	 * Create a new entry if we just got back a wildcard entry
	 * or the lookup failed.  This is necessary for hosts
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
			if (rt)
				rtfree(rt);
			flags |= RTF_GATEWAY | RTF_DYNAMIC;
			bzero(&info, sizeof(info));
			info.rti_info[RTAX_DST] = dst;
			info.rti_info[RTAX_GATEWAY] = gateway;
			info.rti_info[RTAX_NETMASK] = netmask;
			info.rti_ifa = ifa;
			info.rti_flags = flags;
			rt = NULL;
			error = rtrequest1(RTM_ADD, &info, RTP_DEFAULT, &rt,
			    rdomain);
			if (rt != NULL)
				flags = rt->rt_flags;
			stat = &rtstat.rts_dynamic;
		} else {
			/*
			 * Smash the current notion of the gateway to
			 * this destination.  Should check about netmask!!!
			 */
			rt->rt_flags |= RTF_MODIFIED;
			flags |= RTF_MODIFIED;
			stat = &rtstat.rts_newgateway;
			rt_setgate(rt, rt_key(rt), gateway, rdomain);
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
	rt_missmsg(RTM_REDIRECT, &info, flags, ifp, error, rdomain);
}

/*
 * Delete a route and generate a message
 */
int
rtdeletemsg(struct rtentry *rt, u_int tableid)
{
	int			error;
	struct rt_addrinfo	info;
	struct ifnet		*ifp;

	/*
	 * Request the new route so that the entry is not actually
	 * deleted.  That will allow the information being reported to
	 * be accurate (and consistent with route_output()).
	 */
	bzero((caddr_t)&info, sizeof(info));
	info.rti_info[RTAX_DST] = rt_key(rt);
	info.rti_info[RTAX_NETMASK] = rt_mask(rt);
	info.rti_info[RTAX_GATEWAY] = rt->rt_gateway;
	info.rti_flags = rt->rt_flags;
	ifp = rt->rt_ifp;
	error = rtrequest1(RTM_DELETE, &info, rt->rt_priority, &rt, tableid);

	rt_missmsg(RTM_DELETE, &info, info.rti_flags, ifp, error, tableid);

	/* Adjust the refcount */
	if (error == 0 && rt->rt_refcnt <= 0) {
		rt->rt_refcnt++;
		rtfree(rt);
	}
	return (error);
}

int
rtflushclone1(struct radix_node *rn, void *arg)
{
	struct rtentry	*rt, *parent;

	rt = (struct rtentry *)rn;
	parent = (struct rtentry *)arg;
	if ((rt->rt_flags & RTF_CLONED) != 0 && rt->rt_parent == parent)
		rtdeletemsg(rt, 0);
	return 0;
}

void
rtflushclone(struct radix_node_head *rnh, struct rtentry *parent)
{

#ifdef DIAGNOSTIC
	if (!parent || (parent->rt_flags & RTF_CLONING) == 0)
		panic("rtflushclone: called with a non-cloning route");
	if (!rnh->rnh_walktree)
		panic("rtflushclone: no rnh_walktree");
#endif
	rnh->rnh_walktree(rnh, rtflushclone1, (void *)parent);
}

int
rtioctl(u_long req, caddr_t data, struct proc *p)
{
	return (EOPNOTSUPP);
}

struct ifaddr *
ifa_ifwithroute(int flags, struct sockaddr *dst, struct sockaddr *gateway,
    u_int rtableid)
{
	struct ifaddr	*ifa;

#ifdef IPSEC
	/*
	 * If the destination is a PF_KEY address, we'll look
	 * for the existence of a encap interface number or address
	 * in the options list of the gateway. By default, we'll return
	 * enc0.
	 */
	if (dst && (dst->sa_family == PF_KEY))
		return (encap_findgwifa(gateway));
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
			ifa = ifa_ifwithdstaddr(dst, rtableid);
		if (ifa == NULL)
			ifa = ifa_ifwithaddr(gateway, rtableid);
	} else {
		/*
		 * If we are adding a route to a remote net
		 * or host, the gateway may still be on the
		 * other end of a pt to pt link.
		 */
		ifa = ifa_ifwithdstaddr(gateway, rtableid);
	}
	if (ifa == NULL)
		ifa = ifa_ifwithnet(gateway, rtableid);
	if (ifa == NULL) {
		struct rtentry	*rt = rtalloc1(gateway, 0, rtable_l2(rtableid));
		if (rt == NULL)
			return (NULL);
		rt->rt_refcnt--;
		/* The gateway must be local if the same address family. */
		if ((rt->rt_flags & RTF_GATEWAY) &&
		    rt_key(rt)->sa_family == dst->sa_family)
			return (0);
		if ((ifa = rt->rt_ifa) == NULL)
			return (NULL);
	}
	if (ifa->ifa_addr->sa_family != dst->sa_family) {
		struct ifaddr	*oifa = ifa;
		ifa = ifaof_ifpforaddr(dst, ifa->ifa_ifp);
		if (ifa == NULL)
			ifa = oifa;
	}
	return (ifa);
}

#define ROUNDUP(a) (a>0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))

int
rt_getifa(struct rt_addrinfo *info, u_int rtid)
{
	struct ifaddr	*ifa;
	int		 error = 0;

	/*
	 * ifp may be specified by sockaddr_dl when protocol address
	 * is ambiguous
	 */
	if (info->rti_ifp == NULL && info->rti_info[RTAX_IFP] != NULL
	    && info->rti_info[RTAX_IFP]->sa_family == AF_LINK &&
	    (ifa = ifa_ifwithnet((struct sockaddr *)info->rti_info[RTAX_IFP],
	    rtid)) != NULL)
		info->rti_ifp = ifa->ifa_ifp;

	if (info->rti_ifa == NULL && info->rti_info[RTAX_IFA] != NULL)
		info->rti_ifa = ifa_ifwithaddr(info->rti_info[RTAX_IFA], rtid);

	if (info->rti_ifa == NULL) {
		struct sockaddr	*sa;

		if ((sa = info->rti_info[RTAX_IFA]) == NULL)
			if ((sa = info->rti_info[RTAX_GATEWAY]) == NULL)
				sa = info->rti_info[RTAX_DST];

		if (sa != NULL && info->rti_ifp != NULL)
			info->rti_ifa = ifaof_ifpforaddr(sa, info->rti_ifp);
		else if (info->rti_info[RTAX_DST] != NULL &&
		    info->rti_info[RTAX_GATEWAY] != NULL)
			info->rti_ifa = ifa_ifwithroute(info->rti_flags,
			    info->rti_info[RTAX_DST],
			    info->rti_info[RTAX_GATEWAY],
			    rtid);
		else if (sa != NULL)
			info->rti_ifa = ifa_ifwithroute(info->rti_flags,
			    sa, sa, rtid);
	}
	if ((ifa = info->rti_ifa) != NULL) {
		if (info->rti_ifp == NULL)
			info->rti_ifp = ifa->ifa_ifp;
	} else
		error = ENETUNREACH;
	return (error);
}

int
rtrequest1(int req, struct rt_addrinfo *info, u_int8_t prio,
    struct rtentry **ret_nrt, u_int tableid)
{
	int			 s = splsoftnet(); int error = 0;
	struct rtentry		*rt, *crt;
	struct radix_node	*rn;
	struct radix_node_head	*rnh;
	struct ifaddr		*ifa;
	struct sockaddr		*ndst;
	struct sockaddr_rtlabel	*sa_rl, sa_rl2;
#ifdef MPLS
	struct sockaddr_mpls	*sa_mpls;
#endif
#define senderr(x) { error = x ; goto bad; }

	if ((rnh = rt_gettable(info->rti_info[RTAX_DST]->sa_family, tableid)) ==
	    NULL)
		senderr(EAFNOSUPPORT);
	if (info->rti_flags & RTF_HOST)
		info->rti_info[RTAX_NETMASK] = NULL;
	switch (req) {
	case RTM_DELETE:
		if ((rn = rnh->rnh_lookup(info->rti_info[RTAX_DST],
		    info->rti_info[RTAX_NETMASK], rnh)) == NULL)
			senderr(ESRCH);
		rt = (struct rtentry *)rn;
#ifndef SMALL_KERNEL
		/*
		 * if we got multipath routes, we require users to specify
		 * a matching RTAX_GATEWAY.
		 */
		if (rn_mpath_capable(rnh)) {
			rt = rt_mpath_matchgate(rt,
			    info->rti_info[RTAX_GATEWAY], prio);
			rn = (struct radix_node *)rt;
			if (!rt)
				senderr(ESRCH);
		}
#endif
		if ((rn = rnh->rnh_deladdr(info->rti_info[RTAX_DST],
		    info->rti_info[RTAX_NETMASK], rnh, rn)) == NULL)
			senderr(ESRCH);
		rt = (struct rtentry *)rn;

		/* clean up any cloned children */
		if ((rt->rt_flags & RTF_CLONING) != 0)
			rtflushclone(rnh, rt);

		if (rn->rn_flags & (RNF_ACTIVE | RNF_ROOT))
			panic ("rtrequest delete");

		if (rt->rt_gwroute) {
			rt = rt->rt_gwroute; RTFREE(rt);
			(rt = (struct rtentry *)rn)->rt_gwroute = NULL;
		}

		if (rt->rt_parent) {
			rt->rt_parent->rt_refcnt--;
			rt->rt_parent = NULL;
		}

#ifndef SMALL_KERNEL
		if (rn_mpath_capable(rnh)) {
			if ((rn = rnh->rnh_lookup(info->rti_info[RTAX_DST],
			    info->rti_info[RTAX_NETMASK], rnh)) != NULL &&
			    rn_mpath_next(rn, 0) == NULL)
				((struct rtentry *)rn)->rt_flags &= ~RTF_MPATH;
		}
#endif

		rt->rt_flags &= ~RTF_UP;
		if ((ifa = rt->rt_ifa) && ifa->ifa_rtrequest)
			ifa->ifa_rtrequest(RTM_DELETE, rt, info);
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
		if ((rt->rt_flags & RTF_CLONING) == 0)
			senderr(EINVAL);
		ifa = rt->rt_ifa;
		info->rti_flags = rt->rt_flags & ~(RTF_CLONING | RTF_STATIC);
		info->rti_flags |= RTF_CLONED;
		info->rti_info[RTAX_GATEWAY] = rt->rt_gateway;
		if ((info->rti_info[RTAX_NETMASK] = rt->rt_genmask) == NULL)
			info->rti_flags |= RTF_HOST;
		info->rti_info[RTAX_LABEL] =
		    rtlabel_id2sa(rt->rt_labelid, &sa_rl2);
		goto makeroute;

	case RTM_ADD:
		if (info->rti_ifa == 0 && (error = rt_getifa(info, tableid)))
			senderr(error);
		ifa = info->rti_ifa;
makeroute:
		rt = pool_get(&rtentry_pool, PR_NOWAIT | PR_ZERO);
		if (rt == NULL)
			senderr(ENOBUFS);

		rt->rt_flags = info->rti_flags;

		if (prio == 0)
			prio = ifa->ifa_ifp->if_priority + RTP_STATIC;
		rt->rt_priority = prio;	/* init routing priority */
		if ((LINK_STATE_IS_UP(ifa->ifa_ifp->if_link_state) ||
		    ifa->ifa_ifp->if_link_state == LINK_STATE_UNKNOWN) &&
		    ifa->ifa_ifp->if_flags & IFF_UP)
			rt->rt_flags |= RTF_UP;
		else {
			rt->rt_flags &= ~RTF_UP;
			rt->rt_priority |= RTP_DOWN;
		}
		LIST_INIT(&rt->rt_timer);
		if (rt_setgate(rt, info->rti_info[RTAX_DST],
		    info->rti_info[RTAX_GATEWAY], tableid)) {
			pool_put(&rtentry_pool, rt);
			senderr(ENOBUFS);
		}
		ndst = rt_key(rt);
		if (info->rti_info[RTAX_NETMASK] != NULL) {
			rt_maskedcopy(info->rti_info[RTAX_DST], ndst,
			    info->rti_info[RTAX_NETMASK]);
		} else
			Bcopy(info->rti_info[RTAX_DST], ndst,
			    info->rti_info[RTAX_DST]->sa_len);
#ifndef SMALL_KERNEL
		/* do not permit exactly the same dst/mask/gw pair */
		if (rn_mpath_capable(rnh) &&
		    rt_mpath_conflict(rnh, rt, info->rti_info[RTAX_NETMASK],
		    info->rti_flags & RTF_MPATH)) {
			if (rt->rt_gwroute)
				rtfree(rt->rt_gwroute);
			Free(rt_key(rt));
			pool_put(&rtentry_pool, rt);
			senderr(EEXIST);
		}
#endif

		if (info->rti_info[RTAX_LABEL] != NULL) {
			sa_rl = (struct sockaddr_rtlabel *)
			    info->rti_info[RTAX_LABEL];
			rt->rt_labelid = rtlabel_name2id(sa_rl->sr_label);
		}

#ifdef MPLS
		/* We have to allocate additional space for MPLS infos */ 
		if (info->rti_info[RTAX_SRC] != NULL ||
		    info->rti_info[RTAX_DST]->sa_family == AF_MPLS) {
			struct rt_mpls *rt_mpls;

			sa_mpls = (struct sockaddr_mpls *)
			    info->rti_info[RTAX_SRC];

			rt->rt_llinfo = (caddr_t)malloc(sizeof(struct rt_mpls),
			    M_TEMP, M_NOWAIT|M_ZERO);

			if (rt->rt_llinfo == NULL) {
				if (rt->rt_gwroute)
					rtfree(rt->rt_gwroute);
				Free(rt_key(rt));
				pool_put(&rtentry_pool, rt);
				senderr(ENOMEM);
			}

			rt_mpls = (struct rt_mpls *)rt->rt_llinfo;

			if (sa_mpls != NULL)
				rt_mpls->mpls_label = sa_mpls->smpls_label;

			rt_mpls->mpls_operation = info->rti_mpls;

			/* XXX: set experimental bits */

			rt->rt_flags |= RTF_MPLS;
		}
#endif

		ifa->ifa_refcnt++;
		rt->rt_ifa = ifa;
		rt->rt_ifp = ifa->ifa_ifp;
		if (req == RTM_RESOLVE) {
			/*
			 * Copy both metrics and a back pointer to the cloned
			 * route's parent.
			 */
			rt->rt_rmx = (*ret_nrt)->rt_rmx; /* copy metrics */
			rt->rt_priority = (*ret_nrt)->rt_priority;
			rt->rt_parent = *ret_nrt;	 /* Back ptr. to parent. */
			rt->rt_parent->rt_refcnt++;
		}
		rn = rnh->rnh_addaddr((caddr_t)ndst,
		    (caddr_t)info->rti_info[RTAX_NETMASK], rnh, rt->rt_nodes,
		    rt->rt_priority);
		if (rn == NULL && (crt = rtalloc1(ndst, 0, tableid)) != NULL) {
			/* overwrite cloned route */
			if ((crt->rt_flags & RTF_CLONED) != 0) {
				rtdeletemsg(crt, tableid);
				rn = rnh->rnh_addaddr((caddr_t)ndst,
				    (caddr_t)info->rti_info[RTAX_NETMASK],
				    rnh, rt->rt_nodes, rt->rt_priority);
			}
			RTFREE(crt);
		}
		if (rn == 0) {
			IFAFREE(ifa);
			if ((rt->rt_flags & RTF_CLONED) != 0 && rt->rt_parent)
				rtfree(rt->rt_parent);
			if (rt->rt_gwroute)
				rtfree(rt->rt_gwroute);
			Free(rt_key(rt));
			pool_put(&rtentry_pool, rt);
			senderr(EEXIST);
		}

#ifndef SMALL_KERNEL
		if (rn_mpath_capable(rnh) &&
		    (rn = rnh->rnh_lookup(info->rti_info[RTAX_DST],
		    info->rti_info[RTAX_NETMASK], rnh)) != NULL &&
		    (rn = rn_mpath_prio(rn, prio)) != NULL) {
			if (rn_mpath_next(rn, 0) == NULL)
				((struct rtentry *)rn)->rt_flags &= ~RTF_MPATH;
			else
				((struct rtentry *)rn)->rt_flags |= RTF_MPATH;
		}
#endif

		if (ifa->ifa_rtrequest)
			ifa->ifa_rtrequest(req, rt, info);
		if (ret_nrt) {
			*ret_nrt = rt;
			rt->rt_refcnt++;
		}
		if ((rt->rt_flags & RTF_CLONING) != 0) {
			/* clean up any cloned children */
			rtflushclone(rnh, rt);
		}

		if_group_routechange(info->rti_info[RTAX_DST],
			info->rti_info[RTAX_NETMASK]);
		break;
	}
bad:
	splx(s);
	return (error);
}

int
rt_setgate(struct rtentry *rt0, struct sockaddr *dst, struct sockaddr *gate,
    u_int tableid)
{
	caddr_t	new, old;
	int	dlen = ROUNDUP(dst->sa_len), glen = ROUNDUP(gate->sa_len);
	struct rtentry	*rt = rt0;

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
		/* XXX is this actually valid to cross tables here? */
		rt->rt_gwroute = rtalloc1(gate, 1, rtable_l2(tableid));
		/*
		 * If we switched gateways, grab the MTU from the new
		 * gateway route if the current MTU is 0 or greater
		 * than the MTU of gateway.
		 * Note that, if the MTU of gateway is 0, we will reset the
		 * MTU of the route to run PMTUD again from scratch. XXX
		 */
		if (rt->rt_gwroute && !(rt->rt_rmx.rmx_locks & RTV_MTU) &&
		    rt->rt_rmx.rmx_mtu &&
		    rt->rt_rmx.rmx_mtu > rt->rt_gwroute->rt_rmx.rmx_mtu) {
			rt->rt_rmx.rmx_mtu = rt->rt_gwroute->rt_rmx.rmx_mtu;
		}
	}
	return (0);
}

void
rt_maskedcopy(struct sockaddr *src, struct sockaddr *dst,
    struct sockaddr *netmask)
{
	u_char	*cp1 = (u_char *)src;
	u_char	*cp2 = (u_char *)dst;
	u_char	*cp3 = (u_char *)netmask;
	u_char	*cplim = cp2 + *cp3;
	u_char	*cplim2 = cp2 + *cp1;

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
rtinit(struct ifaddr *ifa, int cmd, int flags)
{
	struct rtentry		*rt;
	struct sockaddr		*dst, *deldst;
	struct mbuf		*m = NULL;
	struct rtentry		*nrt = NULL;
	int			 error;
	struct rt_addrinfo	 info;
	struct sockaddr_rtlabel	 sa_rl;
	u_short			 rtableid = ifa->ifa_ifp->if_rdomain;

	dst = flags & RTF_HOST ? ifa->ifa_dstaddr : ifa->ifa_addr;
	if (cmd == RTM_DELETE) {
		if ((flags & RTF_HOST) == 0 && ifa->ifa_netmask) {
			m = m_get(M_DONTWAIT, MT_SONAME);
			if (m == NULL)
				return (ENOBUFS);
			deldst = mtod(m, struct sockaddr *);
			rt_maskedcopy(dst, deldst, ifa->ifa_netmask);
			dst = deldst;
		}
		if ((rt = rtalloc1(dst, 0, rtableid)) != NULL) {
			rt->rt_refcnt--;
			/* try to find the right route */
			while (rt && rt->rt_ifa != ifa)
				rt = (struct rtentry *)
				    ((struct radix_node *)rt)->rn_dupedkey;
			if (!rt) {
				if (m != NULL)
					(void) m_free(m);
				return (flags & RTF_HOST ? EHOSTUNREACH
							: ENETUNREACH);
			}
		}
	}
	bzero(&info, sizeof(info));
	info.rti_ifa = ifa;
	info.rti_flags = flags | ifa->ifa_flags;
	info.rti_info[RTAX_DST] = dst;
	if (cmd == RTM_ADD)
		info.rti_info[RTAX_GATEWAY] = ifa->ifa_addr;
	info.rti_info[RTAX_LABEL] =
	    rtlabel_id2sa(ifa->ifa_ifp->if_rtlabelid, &sa_rl);

	/*
	 * XXX here, it seems that we are assuming that ifa_netmask is NULL
	 * for RTF_HOST.  bsdi4 passes NULL explicitly (via intermediate
	 * variable) when RTF_HOST is 1.  still not sure if i can safely
	 * change it to meet bsdi4 behavior.
	 */
	info.rti_info[RTAX_NETMASK] = ifa->ifa_netmask;
	error = rtrequest1(cmd, &info, RTP_CONNECTED, &nrt, rtableid);
	if (cmd == RTM_DELETE) {
		if (error == 0 && (rt = nrt) != NULL) {
			rt_newaddrmsg(cmd, ifa, error, nrt);
			if (rt->rt_refcnt <= 0) {
				rt->rt_refcnt++;
				rtfree(rt);
			}
		}
		if (m != NULL)
			(void) m_free(m);
	}
	if (cmd == RTM_ADD && error == 0 && (rt = nrt) != NULL) {
		rt->rt_refcnt--;
		if (rt->rt_ifa != ifa) {
			printf("rtinit: wrong ifa (%p) was (%p)\n",
			    ifa, rt->rt_ifa);
			if (rt->rt_ifa->ifa_rtrequest)
				rt->rt_ifa->ifa_rtrequest(RTM_DELETE, rt, NULL);
			IFAFREE(rt->rt_ifa);
			rt->rt_ifa = ifa;
			rt->rt_ifp = ifa->ifa_ifp;
			ifa->ifa_refcnt++;
			if (ifa->ifa_rtrequest)
				ifa->ifa_rtrequest(RTM_ADD, rt, NULL);
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

LIST_HEAD(, rttimer_queue)	rttimer_queue_head;
static int			rt_init_done = 0;

#define RTTIMER_CALLOUT(r)	{				\
	if (r->rtt_func != NULL) {				\
		(*r->rtt_func)(r->rtt_rt, r);			\
	} else {						\
		struct rt_addrinfo info;			\
		bzero(&info, sizeof(info));			\
		info.rti_info[RTAX_DST] = rt_key(r->rtt_rt);	\
		rtrequest1(RTM_DELETE, &info,			\
		    r->rtt_rt->rt_priority, NULL, 0 /* XXX */);	\
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
	static struct timeout	rt_timer_timeout;

	if (rt_init_done)
		panic("rt_timer_init: already initialized");

	pool_init(&rttimer_pool, sizeof(struct rttimer), 0, 0, 0, "rttmrpl",
	    NULL);

	LIST_INIT(&rttimer_queue_head);
	timeout_set(&rt_timer_timeout, rt_timer_timer, &rt_timer_timeout);
	timeout_add_sec(&rt_timer_timeout, 1);
	rt_init_done = 1;
}

struct rttimer_queue *
rt_timer_queue_create(u_int timeout)
{
	struct rttimer_queue	*rtq;

	if (rt_init_done == 0)
		rt_timer_init();

	R_Malloc(rtq, struct rttimer_queue *, sizeof *rtq);
	if (rtq == NULL)
		return (NULL);
	Bzero(rtq, sizeof *rtq);

	rtq->rtq_timeout = timeout;
	rtq->rtq_count = 0;
	TAILQ_INIT(&rtq->rtq_head);
	LIST_INSERT_HEAD(&rttimer_queue_head, rtq, rtq_link);

	return (rtq);
}

void
rt_timer_queue_change(struct rttimer_queue *rtq, long timeout)
{
	rtq->rtq_timeout = timeout;
}

void
rt_timer_queue_destroy(struct rttimer_queue *rtq, int destroy)
{
	struct rttimer	*r;

	while ((r = TAILQ_FIRST(&rtq->rtq_head)) != NULL) {
		LIST_REMOVE(r, rtt_link);
		TAILQ_REMOVE(&rtq->rtq_head, r, rtt_next);
		if (destroy)
			RTTIMER_CALLOUT(r);
		pool_put(&rttimer_pool, r);
		if (rtq->rtq_count > 0)
			rtq->rtq_count--;
		else
			printf("rt_timer_queue_destroy: rtq_count reached 0\n");
	}

	LIST_REMOVE(rtq, rtq_link);

	/*
	 * Caller is responsible for freeing the rttimer_queue structure.
	 */
}

unsigned long
rt_timer_count(struct rttimer_queue *rtq)
{
	return (rtq->rtq_count);
}

void
rt_timer_remove_all(struct rtentry *rt)
{
	struct rttimer	*r;

	while ((r = LIST_FIRST(&rt->rt_timer)) != NULL) {
		LIST_REMOVE(r, rtt_link);
		TAILQ_REMOVE(&r->rtt_queue->rtq_head, r, rtt_next);
		if (r->rtt_queue->rtq_count > 0)
			r->rtt_queue->rtq_count--;
		else
			printf("rt_timer_remove_all: rtq_count reached 0\n");
		pool_put(&rttimer_pool, r);
	}
}

int
rt_timer_add(struct rtentry *rt, void (*func)(struct rtentry *,
    struct rttimer *), struct rttimer_queue *queue)
{
	struct rttimer	*r;
	long		 current_time;

	current_time = time_uptime;
	rt->rt_rmx.rmx_expire = time_second + queue->rtq_timeout;

	/*
	 * If there's already a timer with this action, destroy it before
	 * we add a new one.
	 */
	for (r = LIST_FIRST(&rt->rt_timer); r != NULL;
	     r = LIST_NEXT(r, rtt_link)) {
		if (r->rtt_func == func) {
			LIST_REMOVE(r, rtt_link);
			TAILQ_REMOVE(&r->rtt_queue->rtq_head, r, rtt_next);
			if (r->rtt_queue->rtq_count > 0)
				r->rtt_queue->rtq_count--;
			else
				printf("rt_timer_add: rtq_count reached 0\n");
			pool_put(&rttimer_pool, r);
			break;  /* only one per list, so we can quit... */
		}
	}

	r = pool_get(&rttimer_pool, PR_NOWAIT | PR_ZERO);
	if (r == NULL)
		return (ENOBUFS);

	r->rtt_rt = rt;
	r->rtt_time = current_time;
	r->rtt_func = func;
	r->rtt_queue = queue;
	LIST_INSERT_HEAD(&rt->rt_timer, r, rtt_link);
	TAILQ_INSERT_TAIL(&queue->rtq_head, r, rtt_next);
	r->rtt_queue->rtq_count++;

	return (0);
}

struct radix_node_head *
rt_gettable(sa_family_t af, u_int id)
{
	if (id > rtbl_id_max)
		return (NULL);
	return (rt_tables[id] ? rt_tables[id][af2rtafidx[af]] : NULL);
}

struct radix_node *
rt_lookup(struct sockaddr *dst, struct sockaddr *mask, u_int tableid)
{
	struct radix_node_head	*rnh;

	if ((rnh = rt_gettable(dst->sa_family, tableid)) == NULL)
		return (NULL);

	return (rnh->rnh_lookup(dst, mask, rnh));
}

/* ARGSUSED */
void
rt_timer_timer(void *arg)
{
	struct timeout		*to = (struct timeout *)arg;
	struct rttimer_queue	*rtq;
	struct rttimer		*r;
	long			 current_time;
	int			 s;

	current_time = time_uptime;

	s = splsoftnet();
	for (rtq = LIST_FIRST(&rttimer_queue_head); rtq != NULL;
	     rtq = LIST_NEXT(rtq, rtq_link)) {
		while ((r = TAILQ_FIRST(&rtq->rtq_head)) != NULL &&
		    (r->rtt_time + rtq->rtq_timeout) < current_time) {
			LIST_REMOVE(r, rtt_link);
			TAILQ_REMOVE(&rtq->rtq_head, r, rtt_next);
			RTTIMER_CALLOUT(r);
			pool_put(&rttimer_pool, r);
			if (rtq->rtq_count > 0)
				rtq->rtq_count--;
			else
				printf("rt_timer_timer: rtq_count reached 0\n");
		}
	}
	splx(s);

	timeout_add_sec(to, 1);
}

u_int16_t
rtlabel_name2id(char *name)
{
	struct rt_label		*label, *p = NULL;
	u_int16_t		 new_id = 1;

	if (!name[0])
		return (0);

	TAILQ_FOREACH(label, &rt_labels, rtl_entry)
		if (strcmp(name, label->rtl_name) == 0) {
			label->rtl_ref++;
			return (label->rtl_id);
		}

	/*
	 * to avoid fragmentation, we do a linear search from the beginning
	 * and take the first free slot we find. if there is none or the list
	 * is empty, append a new entry at the end.
	 */

	if (!TAILQ_EMPTY(&rt_labels))
		for (p = TAILQ_FIRST(&rt_labels); p != NULL &&
		    p->rtl_id == new_id; p = TAILQ_NEXT(p, rtl_entry))
			new_id = p->rtl_id + 1;

	if (new_id > LABELID_MAX)
		return (0);

	label = malloc(sizeof(*label), M_TEMP, M_NOWAIT|M_ZERO);
	if (label == NULL)
		return (0);
	strlcpy(label->rtl_name, name, sizeof(label->rtl_name));
	label->rtl_id = new_id;
	label->rtl_ref++;

	if (p != NULL)	/* insert new entry before p */
		TAILQ_INSERT_BEFORE(p, label, rtl_entry);
	else		/* either list empty or no free slot in between */
		TAILQ_INSERT_TAIL(&rt_labels, label, rtl_entry);

	return (label->rtl_id);
}

const char *
rtlabel_id2name(u_int16_t id)
{
	struct rt_label	*label;

	TAILQ_FOREACH(label, &rt_labels, rtl_entry)
		if (label->rtl_id == id)
			return (label->rtl_name);

	return (NULL);
}

struct sockaddr *
rtlabel_id2sa(u_int16_t labelid, struct sockaddr_rtlabel *sa_rl)
{
	const char	*label;

	if (labelid == 0 || (label = rtlabel_id2name(labelid)) == NULL)
		return (NULL);

	bzero(sa_rl, sizeof(*sa_rl));
	sa_rl->sr_len = sizeof(*sa_rl);
	sa_rl->sr_family = AF_UNSPEC;
	strlcpy(sa_rl->sr_label, label, sizeof(sa_rl->sr_label));

	return ((struct sockaddr *)sa_rl);
}

void
rtlabel_unref(u_int16_t id)
{
	struct rt_label	*p, *next;

	if (id == 0)
		return;

	for (p = TAILQ_FIRST(&rt_labels); p != NULL; p = next) {
		next = TAILQ_NEXT(p, rtl_entry);
		if (id == p->rtl_id) {
			if (--p->rtl_ref == 0) {
				TAILQ_REMOVE(&rt_labels, p, rtl_entry);
				free(p, M_TEMP);
			}
			break;
		}
	}
}

void
rt_if_remove(struct ifnet *ifp)
{
	int			 i;
	u_int			 tid;
	struct radix_node_head	*rnh;

	for (tid = 0; tid <= rtbl_id_max; tid++) {
		for (i = 1; i <= AF_MAX; i++) {
			if ((rnh = rt_gettable(i, tid)) != NULL)
				while ((*rnh->rnh_walktree)(rnh,
				    rt_if_remove_rtdelete, ifp) == EAGAIN)
					;	/* nothing */
		}
	}
}

/*
 * Note that deleting a RTF_CLONING route can trigger the
 * deletion of more entries, so we need to cancel the walk
 * and return EAGAIN.  The caller should restart the walk
 * as long as EAGAIN is returned.
 */
int
rt_if_remove_rtdelete(struct radix_node *rn, void *vifp)
{
	struct ifnet	*ifp = vifp;
	struct rtentry	*rt = (struct rtentry *)rn;

	if (rt->rt_ifp == ifp) {
		int	cloning = (rt->rt_flags & RTF_CLONING);

		if (rtdeletemsg(rt, ifp->if_rdomain /* XXX wrong */) == 0 && cloning)
			return (EAGAIN);
	}

	/*
	 * XXX There should be no need to check for rt_ifa belonging to this
	 * interface, because then rt_ifp is set, right?
	 */

	return (0);
}

#ifndef SMALL_KERNEL
void
rt_if_track(struct ifnet *ifp)
{
	struct radix_node_head *rnh;
	int i;
	u_int tid;

	if (rt_tables == NULL)
		return;

	for (tid = 0; tid <= rtbl_id_max; tid++) {
		for (i = 1; i <= AF_MAX; i++) {
			if ((rnh = rt_gettable(i, tid)) != NULL) {
				if (!rn_mpath_capable(rnh))
					continue;
				while ((*rnh->rnh_walktree)(rnh,
				    rt_if_linkstate_change, ifp) == EAGAIN)
					;	/* nothing */
			}
		}
	}
}

int
rt_if_linkstate_change(struct radix_node *rn, void *arg)
{
	struct ifnet *ifp = arg;
	struct rtentry *rt = (struct rtentry *)rn;

	if (rt->rt_ifp == ifp) {
		if ((LINK_STATE_IS_UP(ifp->if_link_state) ||
		    ifp->if_link_state == LINK_STATE_UNKNOWN) &&
		    ifp->if_flags & IFF_UP) {
			if (!(rt->rt_flags & RTF_UP)) {
				/* bring route up */
				rt->rt_flags |= RTF_UP;
				rn_mpath_reprio(rn, rt->rt_priority & RTP_MASK);
			}
		} else {
			if (rt->rt_flags & RTF_UP) {
				/* take route done */
				rt->rt_flags &= ~RTF_UP;
				rn_mpath_reprio(rn, rt->rt_priority | RTP_DOWN);
			}
		}
		if_group_routechange(rt_key(rt), rt_mask(rt));
	}

	return (0);
}
#endif
