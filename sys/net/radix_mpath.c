/*	$OpenBSD: radix_mpath.c,v 1.8 2007/12/04 19:43:04 claudio Exp $	*/
/*	$KAME: radix_mpath.c,v 1.13 2002/10/28 21:05:59 itojun Exp $	*/

/*
 * Copyright (C) 2001 WIDE Project.
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
 * THE AUTHORS DO NOT GUARANTEE THAT THIS SOFTWARE DOES NOT INFRINGE
 * ANY OTHERS' INTELLECTUAL PROPERTIES. IN NO EVENT SHALL THE AUTHORS
 * BE LIABLE FOR ANY INFRINGEMENT OF ANY OTHERS' INTELLECTUAL
 * PROPERTIES.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#define	M_DONTWAIT M_NOWAIT
#include <sys/domain.h>
#include <sys/syslog.h>
#include <net/radix.h>
#include <net/radix_mpath.h>
#include <net/route.h>
#include <dev/rndvar.h>

#include <netinet/in.h>

extern int ipmultipath;
extern int ip6_multipath;

u_int32_t rn_mpath_hash(struct route *, u_int32_t *);

/*
 * give some jitter to hash, to avoid synchronization between routers
 */
static u_int32_t hashjitter;

int
rn_mpath_capable(struct radix_node_head *rnh)
{
	return rnh->rnh_multipath;
}

struct radix_node *
rn_mpath_next(struct radix_node *rn)
{
	struct radix_node *next;

	if (!rn->rn_dupedkey)
		return NULL;
	next = rn->rn_dupedkey;
	if (rn->rn_mask == next->rn_mask)
		return next;
	else
		return NULL;
}

int
rn_mpath_count(struct radix_node *rn)
{
	int i;

	i = 1;
	while ((rn = rn_mpath_next(rn)) != NULL)
		i++;
	return i;
}

struct rtentry *
rt_mpath_matchgate(struct rtentry *rt, struct sockaddr *gate)
{
	struct radix_node *rn = (struct radix_node *)rt;

	/*
	 * if gate is set it must be compared, if not set the route must be
	 * a non-multipath one.
	 */
	if (!gate && !rn_mpath_next(rn))
		return rt;
	if (!gate)
		return NULL;

	do {
		rt = (struct rtentry *)rn;
		if (rt->rt_gateway->sa_len == gate->sa_len &&
		    !memcmp(rt->rt_gateway, gate, gate->sa_len))
			break;
	} while ((rn = rn_mpath_next(rn)) != NULL);

	return (struct rtentry *)rn;
}

/*
 * check if we have the same key/mask/gateway on the table already.
 */
int
rt_mpath_conflict(struct radix_node_head *rnh, struct rtentry *rt,
		   struct sockaddr *netmask, int mpathok)
{
	struct radix_node *rn, *rn1;
	struct rtentry *rt1;
	char *p, *q, *eq;
	int same, l, skip;

	rn = (struct radix_node *)rt;
	rn1 = rnh->rnh_lookup(rt_key(rt), netmask, rnh);
	if (!rn1 || rn1->rn_flags & RNF_ROOT)
		return 0;

	/*
	 * unlike other functions we have in this file, we have to check
	 * all key/mask/gateway as rnh_lookup can match less specific entry.
	 */
	rt1 = (struct rtentry *)rn1;

	/* compare key. */
	if (rt_key(rt1)->sa_len != rt_key(rt)->sa_len ||
	    bcmp(rt_key(rt1), rt_key(rt), rt_key(rt1)->sa_len))
		goto different;

	/* key was the same.  compare netmask.  hairy... */
	if (rt_mask(rt1) && netmask) {
		skip = rnh->rnh_treetop->rn_off;
		if (rt_mask(rt1)->sa_len > netmask->sa_len) {
			/*
			 * as rt_mask(rt1) is made optimal by radix.c,
			 * there must be some 1-bits on rt_mask(rt1)
			 * after netmask->sa_len.  therefore, in
			 * this case, the entries are different.
			 */
			if (rt_mask(rt1)->sa_len > skip)
				goto different;
			else {
				/* no bits to compare, i.e. same*/
				goto maskmatched;
			}
		}

		l = rt_mask(rt1)->sa_len;
		if (skip > l) {
			/* no bits to compare, i.e. same */
			goto maskmatched;
		}
		p = (char *)rt_mask(rt1);
		q = (char *)netmask;
		if (bcmp(p + skip, q + skip, l - skip))
			goto different;
		/*
		 * need to go through all the bit, as netmask is not
		 * optimal and can contain trailing 0s
		 */
		eq = (char *)netmask + netmask->sa_len;
		q += l;
		same = 1;
		while (eq > q)
			if (*q++) {
				same = 0;
				break;
			}
		if (!same)
			goto different;
	} else if (!rt_mask(rt1) && !netmask)
		; /* no mask to compare, i.e. same */
	else {
		/* one has mask and the other does not, different */
		goto different;
	}

 maskmatched:
	if (!mpathok)
		return EEXIST;

	/* key/mask were the same.  compare gateway for all multipaths */
	do {
		rt1 = (struct rtentry *)rn1;

		/* sanity: no use in comparing the same thing */
		if (rn1 == rn)
			continue;

		if (rt1->rt_gateway->sa_len != rt->rt_gateway->sa_len ||
		    bcmp(rt1->rt_gateway, rt->rt_gateway,
		    rt1->rt_gateway->sa_len))
			continue;

		/* all key/mask/gateway are the same.  conflicting entry. */
		return EEXIST;
	} while ((rn1 = rn_mpath_next(rn1)) != NULL);

 different:
	return 0;
}

/*
 * allocate a route, potentially using multipath to select the peer.
 */
void
rtalloc_mpath(struct route *ro, u_int32_t *srcaddrp, u_int tableid)
{
#if defined(INET) || defined(INET6)
	struct radix_node *rn;
	int hash, npaths, threshold;
#endif

	/*
	 * return a cached entry if it is still valid, otherwise we increase
	 * the risk of disrupting local flows.
	 */
	if (ro->ro_rt && ro->ro_rt->rt_ifp && (ro->ro_rt->rt_flags & RTF_UP))
		return;
	ro->ro_rt = rtalloc1(&ro->ro_dst, 1, tableid);

	/* if the route does not exist or it is not multipath, don't care */
	if (!ro->ro_rt || !(ro->ro_rt->rt_flags & RTF_MPATH))
		return;

	/* check if multipath routing is enabled for the specified protocol */
	if (!(0
#ifdef INET
	    || (ipmultipath && ro->ro_dst.sa_family == AF_INET)
#endif
#ifdef INET6
	    || (ip6_multipath && ro->ro_dst.sa_family == AF_INET6)
#endif
	    ))
		return;

#if defined(INET) || defined(INET6)
	/* gw selection by Hash-Threshold (RFC 2992) */
	rn = (struct radix_node *)ro->ro_rt;
	npaths = rn_mpath_count(rn);
	hash = rn_mpath_hash(ro, srcaddrp) & 0xffff;
	threshold = 1 + (0xffff / npaths);
	while (hash > threshold && rn) {
		/* stay within the multipath routes */
		if (rn->rn_dupedkey && rn->rn_mask != rn->rn_dupedkey->rn_mask)
			break;
		rn = rn->rn_dupedkey;
		hash -= threshold;
	}

	/* XXX try filling rt_gwroute and avoid unreachable gw  */

	/* if gw selection fails, use the first match (default) */
	if (!rn)
		return;

	rtfree(ro->ro_rt);
	ro->ro_rt = (struct rtentry *)rn;
	ro->ro_rt->rt_refcnt++;
#endif
}

int
rn_mpath_inithead(void **head, int off)
{
	struct radix_node_head *rnh;

	while (hashjitter == 0)
		hashjitter = arc4random();
	if (rn_inithead(head, off) == 1) {
		rnh = (struct radix_node_head *)*head;
		rnh->rnh_multipath = 1;
		return 1;
	} else
		return 0;
}

/*
 * hash function based on pf_hash in pf.c
 */
#define mix(a,b,c) \
	do {					\
		a -= b; a -= c; a ^= (c >> 13);	\
		b -= c; b -= a; b ^= (a << 8);	\
		c -= a; c -= b; c ^= (b >> 13);	\
		a -= b; a -= c; a ^= (c >> 12);	\
		b -= c; b -= a; b ^= (a << 16);	\
		c -= a; c -= b; c ^= (b >> 5);	\
		a -= b; a -= c; a ^= (c >> 3);	\
		b -= c; b -= a; b ^= (a << 10);	\
		c -= a; c -= b; c ^= (b >> 15);	\
	} while (0)

u_int32_t
rn_mpath_hash(struct route *ro, u_int32_t *srcaddrp)
{
	u_int32_t a, b, c;

	a = b = 0x9e3779b9;
	c = hashjitter;

	switch (ro->ro_dst.sa_family) {
#ifdef INET
	case AF_INET:
	    {
		struct sockaddr_in *sin_dst;

		sin_dst = (struct sockaddr_in *)&ro->ro_dst;
		a += sin_dst->sin_addr.s_addr;
		b += srcaddrp ? srcaddrp[0] : 0;
		mix(a, b, c);
		break;
	    }
#endif /* INET */
#ifdef INET6
	case AF_INET6:
	    {
		struct sockaddr_in6 *sin6_dst;

		sin6_dst = (struct sockaddr_in6 *)&ro->ro_dst;
		a += sin6_dst->sin6_addr.s6_addr32[0];
		b += sin6_dst->sin6_addr.s6_addr32[2];
		c += srcaddrp ? srcaddrp[0] : 0;
		mix(a, b, c);
		a += sin6_dst->sin6_addr.s6_addr32[1];
		b += sin6_dst->sin6_addr.s6_addr32[3];
		c += srcaddrp ? srcaddrp[1] : 0;
		mix(a, b, c);
		a += sin6_dst->sin6_addr.s6_addr32[2];
		b += sin6_dst->sin6_addr.s6_addr32[1];
		c += srcaddrp ? srcaddrp[2] : 0;
		mix(a, b, c);
		a += sin6_dst->sin6_addr.s6_addr32[3];
		b += sin6_dst->sin6_addr.s6_addr32[0];
		c += srcaddrp ? srcaddrp[3] : 0;
		mix(a, b, c);
		break;
	    }
#endif /* INET6 */
	}

	return c;
}
