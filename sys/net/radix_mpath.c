/*	$OpenBSD: radix_mpath.c,v 1.25 2014/11/01 21:40:38 mpi Exp $	*/
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
#include <sys/domain.h>
#include <sys/syslog.h>
#include <net/radix.h>
#include <net/radix_mpath.h>
#include <net/route.h>
#include <dev/rndvar.h>

#include <netinet/in.h>
#include <netinet/ip_var.h>

#ifdef INET6
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#endif

u_int32_t rn_mpath_hash(struct sockaddr *, u_int32_t *);

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
rn_mpath_next(struct radix_node *rn, int mode)
{
	struct radix_node	*next;
	struct rtentry		*nt, *rt = (struct rtentry *)rn;

	if (!rn->rn_dupedkey)
		return NULL;
	next = rn->rn_dupedkey;

	if (rn->rn_mask != next->rn_mask)
		return NULL;

	switch (mode) {
	case RMP_MODE_BYPRIO:
		/* scan all of the list until we find a match */
		while (next) {
			if (rn->rn_mask != next->rn_mask)
				return NULL;
			nt = (struct rtentry *)next;
			if ((rt->rt_priority & RTP_MASK) ==
			    (nt->rt_priority & RTP_MASK))
				return next;
			rn = next;
			next = next->rn_dupedkey;
		}
		return NULL;
	case RMP_MODE_ACTIVE:
		nt = (struct rtentry *)next;
		if (rt->rt_priority != nt->rt_priority)
			return NULL;
		break;
	case RMP_MODE_ALL:
		break;
	}
	return next;
}

struct rtentry *
rt_mpath_next(struct rtentry *rt)
{
	struct radix_node *rn = (struct radix_node *)rt;

	return ((struct rtentry *)rn_mpath_next(rn, RMP_MODE_ACTIVE));
}

/*
 * return first route matching prio or the node just before.
 */
struct radix_node *
rn_mpath_prio(struct radix_node *rn, u_int8_t prio)
{
	struct radix_node	*hit = rn;
	struct rtentry		*rt;

	if (prio == RTP_ANY)
		return (hit);

	do {
		rt = (struct rtentry *)rn;
		if (rt->rt_priority == prio)
			/* perfect match */
			return (rn);

		/* list is sorted remember last more prefered (smaller) entry */
		if (rt->rt_priority < prio)
			hit = rn;
	} while ((rn = rn_mpath_next(rn, RMP_MODE_ALL)) != NULL);

	return (hit);
}

/*
 * Adjust the RTF_MPATH flag for the part of the rn_dupedkey chain
 * that matches the prio.
 */
void
rn_mpath_adj_mpflag(struct radix_node *rn, u_int8_t prio)
{
	struct rtentry *rt = (struct rtentry *)rn;

	prio &= RTP_MASK;
	rt = rt_mpath_matchgate(rt, NULL, prio);
	rn = (struct radix_node *)rt;
	
	if (!rn)
		return;
	if (rn_mpath_next(rn, RMP_MODE_BYPRIO)) {
		while (rn != NULL) {
			((struct rtentry *)rn)->rt_flags |= RTF_MPATH;
			rn = rn_mpath_next(rn, RMP_MODE_BYPRIO);
		}
	} else
		rt->rt_flags &= ~RTF_MPATH;
}

int
rn_mpath_active_count(struct radix_node *rn)
{
	int i;

	i = 1;
	while ((rn = rn_mpath_next(rn, RMP_MODE_ACTIVE)) != NULL)
		i++;
	return i;
}

/*
 * return best matching route based on gateway and prio. If both are
 * specified it acts as a lookup function to get the actual rt.
 * If gate is NULL the first node matching the prio will be returned.
 */
struct rtentry *
rt_mpath_matchgate(struct rtentry *rt, struct sockaddr *gate, u_int8_t prio)
{
	struct radix_node *rn = (struct radix_node *)rt;

	do {
		rt = (struct rtentry *)rn;

		/* first find routes with correct priority */
		if (prio != RTP_ANY &&
		    (rt->rt_priority & RTP_MASK) != (prio & RTP_MASK))
			continue;
		/* if no gate is set we found a match */
		if (!gate)
			return rt;
		if (rt->rt_gateway->sa_len == gate->sa_len &&
		    !memcmp(rt->rt_gateway, gate, gate->sa_len))
			break;
	} while ((rn = rn_mpath_next(rn, RMP_MODE_ALL)) != NULL);

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
	if (!mpathok && rt1->rt_priority == rt->rt_priority)
		return EEXIST;

	/* key/mask were the same.  compare gateway for all multipaths */
	if (rt_mpath_matchgate(rt1, rt->rt_gateway, rt->rt_priority))
		/* all key/mask/gateway are the same.  conflicting entry. */
		return EEXIST;

 different:
	return 0;
}

void
rn_mpath_reprio(struct radix_node *rn, int newprio)
{
	struct radix_node	*prev = rn->rn_p;
	struct radix_node	*next = rn->rn_dupedkey;
	struct radix_node	*t, *tt, *saved_tt, *head;
	struct rtentry		*rt = (struct rtentry *)rn;
	int			 mid, oldprio, prioinv = 0;

	oldprio = rt->rt_priority;
	rt->rt_priority = newprio;

	/* same prio, no change needed */
	if (oldprio == newprio)
		return;
	if (rn_mpath_next(rn, RMP_MODE_ALL) == NULL) {
		/* no need to move node, route is alone */
		if (prev->rn_mask != rn->rn_mask)
			return;
		/* ... or route is last and prio gets bigger */
		if (oldprio < newprio)
			return;
	}

	/* remove node from dupedkey list and reinsert at correct place */
	if (prev->rn_dupedkey == rn) {
		prev->rn_dupedkey = next;
		if (next)
			next->rn_p = prev;
		else
			next = prev;
	} else {
		if (next == NULL)
			panic("next == NULL");
		next->rn_p = prev;
		if (prev->rn_l == rn)
			prev->rn_l = next;
		else
			prev->rn_r = next;
	}

	/* re-insert rn at the right spot, so first rewind to the head */
	for (tt = next; tt->rn_p->rn_dupedkey == tt; tt = tt->rn_p)
		;
	saved_tt = tt;

	/*
	 * Stolen from radix.c rn_addroute().
	 * This is nasty code with a certain amount of magic and dragons.
	 * t is the element where the re-priorized rn is inserted -- before
	 * or after depending on prioinv. saved_tt points to the head of the
	 * dupedkey chain and tt is a bit of a helper
	 *
	 * First we skip with tt to the start of the mpath group then we
	 * search the right spot to enter our node.
	 */
	for (; tt; tt = tt->rn_dupedkey)
		if (rn->rn_mask == tt->rn_mask)
			break;
	head = tt; /* store current head entry for rn_mklist check */

	tt = rn_mpath_prio(tt, newprio);
	if (((struct rtentry *)tt)->rt_priority != newprio) {
		if (((struct rtentry *)tt)->rt_priority > newprio)
			prioinv = 1;
		t = tt;
	} else {
		mid = rn_mpath_active_count(tt) / 2;
		do {
			t = tt;
			tt = rn_mpath_next(tt, RMP_MODE_ACTIVE);
		} while (tt && --mid > 0);
	}

	/* insert rn before or after t depending on prioinv */
	rn_link_dupedkey(rn, t, prioinv);

	/* the rn_mklist needs to be fixed if the best route changed */
	if (rn->rn_mklist && rn->rn_flags & RNF_NORMAL) {
		if (rn->rn_mklist->rm_leaf != rn) {
			if (rn->rn_mklist->rm_leaf->rn_p == rn)
				/* changed route is now best */
				rn->rn_mklist->rm_leaf = rn;
		} else if (rn->rn_dupedkey != head) {
			/* rn moved behind head, so head is new head */
			rn->rn_mklist->rm_leaf = head;
		}
	}
}

/*
 * allocate a route, potentially using multipath to select the peer.
 */
struct rtentry *
rtalloc_mpath(struct sockaddr *dst, u_int32_t *srcaddrp, u_int rtableid)
{
	struct rtentry *rt;
#if defined(INET) || defined(INET6)
	struct radix_node *rn;
	int hash, npaths, threshold;
#endif

	rt = rtalloc(dst, RT_REPORT|RT_RESOLVE, rtableid);

	/* if the route does not exist or it is not multipath, don't care */
	if (rt == NULL || !ISSET(rt->rt_flags, RTF_MPATH))
		return (rt);

	/* check if multipath routing is enabled for the specified protocol */
	if (!(0
#ifdef INET
	    || (ipmultipath && dst->sa_family == AF_INET)
#endif
#ifdef INET6
	    || (ip6_multipath && dst->sa_family == AF_INET6)
#endif
	    ))
		return (rt);

#if defined(INET) || defined(INET6)
	/* gw selection by Hash-Threshold (RFC 2992) */
	rn = (struct radix_node *)rt;
	npaths = rn_mpath_active_count(rn);
	hash = rn_mpath_hash(dst, srcaddrp) & 0xffff;
	threshold = 1 + (0xffff / npaths);
	while (hash > threshold && rn) {
		/* stay within the multipath routes */
		rn = rn_mpath_next(rn, RMP_MODE_ACTIVE);
		hash -= threshold;
	}

	/* if gw selection fails, use the first match (default) */
	if (rn != NULL) {
		rtfree(rt);
		rt = (struct rtentry *)rn;
		rt->rt_refcnt++;
	}
#endif

	return (rt);
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
rn_mpath_hash(struct sockaddr *dst, u_int32_t *srcaddrp)
{
	u_int32_t a, b, c;

	a = b = 0x9e3779b9;
	c = hashjitter;

	switch (dst->sa_family) {
#ifdef INET
	case AF_INET:
	    {
		struct sockaddr_in *sin_dst;

		sin_dst = (struct sockaddr_in *)dst;
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

		sin6_dst = (struct sockaddr_in6 *)dst;
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
