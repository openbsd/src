/*	$OpenBSD: rtable.c,v 1.1 2015/07/18 15:51:16 mpi Exp $ */

/*
 * Copyright (c) 2014-2015 Martin Pieuchot
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>

#include <net/rtable.h>
#include <net/route.h>

void
rtable_init(void)
{
	rn_init();
}

int
rtable_attach(void **head, int off)
{
	int rv;

#ifndef SMALL_KERNEL
	rv = rn_mpath_inithead(head, off);
#else
	rv = rn_inithead(head, off);
#endif

	return (rv);
}

struct rtentry *
rtable_lookup(unsigned int rtableid, struct sockaddr *dst,
    struct sockaddr *mask)
{
	struct radix_node_head	*rnh;
	struct radix_node	*rn;

	rnh = rtable_get(rtableid, dst->sa_family);
	if (rnh == NULL)
		return (NULL);

	rn = rnh->rnh_lookup(dst, mask, rnh);
	if (rn == NULL || (rn->rn_flags & RNF_ROOT) != 0)
		return (NULL);

	return ((struct rtentry *)rn);
}

struct rtentry *
rtable_match(unsigned int rtableid, struct sockaddr *dst)
{
	struct radix_node_head	*rnh;
	struct radix_node	*rn;

	rnh = rtable_get(rtableid, dst->sa_family);
	if (rnh == NULL)
		return (NULL);

	rn = rnh->rnh_matchaddr(dst, rnh);
	if (rn == NULL || (rn->rn_flags & RNF_ROOT) != 0)
		return (NULL);

	return ((struct rtentry *)rn);
}

int
rtable_insert(unsigned int rtableid, struct sockaddr *dst,
    struct sockaddr *mask, uint8_t prio, struct rtentry *rt)
{
	struct radix_node_head	*rnh;
	struct radix_node	*rn = (struct radix_node *)rt;

	rnh = rtable_get(rtableid, dst->sa_family);
	if (rnh == NULL)
		return (EAFNOSUPPORT);

	rn = rnh->rnh_addaddr(dst, mask, rnh, rn, prio);
	if (rn == NULL)
		return (ESRCH);

	return (0);
}

int
rtable_delete(unsigned int rtableid, struct sockaddr *dst,
    struct sockaddr *mask, uint8_t prio, struct rtentry *rt)
{
	struct radix_node_head	*rnh;
	struct radix_node	*rn = (struct radix_node *)rt;

	rnh = rtable_get(rtableid, dst->sa_family);
	if (rnh == NULL)
		return (EAFNOSUPPORT);

	rn = rnh->rnh_deladdr(dst, mask, rnh, rn);
	if (rn == NULL)
		return (ESRCH);

	if (rn->rn_flags & (RNF_ACTIVE | RNF_ROOT))
		panic("active node flags=%x", rn->rn_flags);

	return (0);
}

int
rtable_setid(void **p, unsigned int rtableid, sa_family_t af)
{
	struct radix_node_head **rnh = (struct radix_node_head **)p;

	if (rnh == NULL || rnh[af] == NULL)
		return (EINVAL);

	rnh[af]->rnh_rtableid = rtableid;

	return (0);
}

int
rtable_walk(unsigned int rtableid, sa_family_t af,
    int (*func)(struct rtentry *, void *, unsigned int), void *arg)
{
	struct radix_node_head	*rnh;
	int (*f)(struct radix_node *, void *, u_int) = (void *)func;

	rnh = rtable_get(rtableid, af);
	if (rnh == NULL)
		return (EAFNOSUPPORT);

	return (*rnh->rnh_walktree)(rnh, f, arg);
}

#ifndef SMALL_KERNEL
int
rtable_mpath_capable(unsigned int rtableid, sa_family_t af)
{
	struct radix_node_head	*rnh;

	rnh = rtable_get(rtableid, af);
	if (rnh == NULL)
		return (0);

	return (rnh->rnh_multipath);
}

struct rtentry *
rtable_mpath_match(unsigned int rtableid, struct rtentry *rt,
    struct sockaddr *gateway, uint8_t prio)
{
	struct radix_node_head	*rnh;

	rnh = rtable_get(rtableid, rt_key(rt)->sa_family);
	if (rnh == NULL || rnh->rnh_multipath == 0)
		return (rt);

	rt = rt_mpath_matchgate(rt, gateway, prio);

	return (rt);
}

int
rtable_mpath_conflict(unsigned int rtableid, struct sockaddr *dst,
    struct sockaddr *mask, struct sockaddr *gateway, uint8_t prio, int mpathok)
{
	struct radix_node_head	*rnh;

	rnh = rtable_get(rtableid, dst->sa_family);
	if (rnh == NULL)
		return (EAFNOSUPPORT);

	if (rnh->rnh_multipath == 0)
		return (0);

	return (rt_mpath_conflict(rnh, dst, mask, gateway, prio, mpathok));
}

struct rtentry *
rtable_mpath_select(struct rtentry *rt, uint32_t *src)
{
	return (rn_mpath_select(rt, src));
}

void
rtable_mpath_reprio(struct rtentry *rt, uint8_t newprio)
{
	struct radix_node	*rn = (struct radix_node *)rt;

	rn_mpath_reprio(rn, newprio);
}
#endif
