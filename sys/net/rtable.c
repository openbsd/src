/*	$OpenBSD: rtable.c,v 1.23 2015/11/09 12:15:29 mpi Exp $ */

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

#ifndef _KERNEL
#include "kern_compat.h"
#else
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/queue.h>
#include <sys/domain.h>
#endif

#include <net/rtable.h>
#include <net/route.h>

uint8_t		   af2idx[AF_MAX+1];	/* To only allocate supported AF */
uint8_t		   af2idx_max;

union rtmap {
	void		**tbl;
	unsigned int	 *dom;
};

union rtmap	  *rtmap;		/* Array of per domain routing table */
unsigned int	   rtables_id_max;

void		   rtable_init_backend(unsigned int);
void		  *rtable_alloc(unsigned int, sa_family_t, unsigned int);
void		   rtable_free(unsigned int);
void		   rtable_grow(unsigned int, sa_family_t);
void		  *rtable_get(unsigned int, sa_family_t);
void		   rtable_put(void *);

void
rtable_init(void)
{
	struct domain	*dp;
	unsigned int	 keylen = 0;
	int		 i;

	/* We use index 0 for the rtable/rdomain map. */
	af2idx_max = 1;
	memset(af2idx, 0, sizeof(af2idx));

	/*
	 * Compute the maximum supported key length in case the routing
	 * table backend needs it.
	 */
	for (i = 0; (dp = domains[i]) != NULL; i++) {
		if (dp->dom_rtoffset)
			af2idx[dp->dom_family] = af2idx_max++;
		if (dp->dom_rtkeylen > keylen)
			keylen = dp->dom_rtkeylen;

	}

	rtables_id_max = 0;
	rtmap = mallocarray(af2idx_max + 1, sizeof(*rtmap), M_RTABLE, M_WAITOK);

	/* Start with a single table for every domain that requires it. */
	for (i = 0; i < af2idx_max + 1; i++) {
		rtmap[i].tbl = mallocarray(1, sizeof(rtmap[0].tbl),
		    M_RTABLE, M_WAITOK|M_ZERO);
	}

	rtable_init_backend(keylen);
}

void
rtable_grow(unsigned int id, sa_family_t af)
{
	void		**tbl, **ntbl;
	int		  i;

	KASSERT(id > rtables_id_max);

	KERNEL_ASSERT_LOCKED();

	tbl = rtmap[af2idx[af]].tbl;
	ntbl = mallocarray(id + 1, sizeof(rtmap[0].tbl), M_RTABLE, M_WAITOK);

	for (i = 0; i < rtables_id_max + 1; i++)
		ntbl[i] = tbl[i];

	while (i < id + 1) {
		ntbl[i] = NULL;
		i++;
	}

	rtmap[af2idx[af]].tbl = ntbl;
	free(tbl, M_RTABLE, (rtables_id_max + 1) * sizeof(rtmap[0].tbl));
}

int
rtable_add(unsigned int id)
{
	struct domain	 *dp;
	void		 *rtbl;
	sa_family_t	  af;
	unsigned int	  off;
	int		  i;

	if (id > RT_TABLEID_MAX)
		return (EINVAL);

	if (rtable_exists(id))
		return (EEXIST);

	for (i = 0; (dp = domains[i]) != NULL; i++) {
		if (dp->dom_rtoffset == 0)
			continue;

		af = dp->dom_family;
		off = dp->dom_rtoffset;

		if (id > rtables_id_max)
			rtable_grow(id, af);

		rtbl = rtable_alloc(id, af, off);
		if (rtbl == NULL)
			return (ENOMEM);

		rtmap[af2idx[af]].tbl[id] = rtbl;
	}

	/* Reflect possible growth. */
	if (id > rtables_id_max) {
		rtable_grow(id, 0);
		rtables_id_max = id;
	}

	/* Use main rtable/rdomain by default. */
	rtmap[0].dom[id] = 0;


	return (0);
}

void
rtable_del(unsigned int id)
{
	struct domain	 *dp;
	sa_family_t	  af;
	int		  i;

	if (id > rtables_id_max || !rtable_exists(id))
		return;

	for (i = 0; (dp = domains[i]) != NULL; i++) {
		if (dp->dom_rtoffset == 0)
			continue;

		af = dp->dom_family;

		rtable_free(id);
		rtmap[af2idx[af]].tbl[id] = NULL;
	}
}

void *
rtable_get(unsigned int rtableid, sa_family_t af)
{
	if (af >= nitems(af2idx) || rtableid > rtables_id_max)
		return (NULL);

	if (af2idx[af] == 0 || rtmap[af2idx[af]].tbl == NULL)
		return (NULL);

	return (rtmap[af2idx[af]].tbl[rtableid]);
}

void
rtable_put(void *tbl)
{
}

int
rtable_exists(unsigned int rtableid)
{
	struct domain	*dp;
	void		*tbl;
	int		 i, exist = 0;

	if (rtableid > rtables_id_max)
		return (0);

	for (i = 0; (dp = domains[i]) != NULL; i++) {
		if (dp->dom_rtoffset == 0)
			continue;

		tbl = rtable_get(rtableid, dp->dom_family);
		if (tbl != NULL)
			exist = 1;
		rtable_put(tbl);

		if (exist)
			break;
	}

	return (exist);
}

unsigned int
rtable_l2(unsigned int rtableid)
{
	if (rtableid > rtables_id_max)
		return (0);

	return (rtmap[0].dom[rtableid]);
}

void
rtable_l2set(unsigned int rtableid, unsigned int parent)
{
	if (!rtable_exists(rtableid) || !rtable_exists(parent))
		return;

	rtmap[0].dom[rtableid] = parent;
}

#ifndef ART
void
rtable_init_backend(unsigned int keylen)
{
	rn_init(keylen); /* initialize all zeroes, all ones, mask table */
}

void *
rtable_alloc(unsigned int rtableid, sa_family_t af, unsigned int off)
{
	struct radix_node_head *rnh = NULL;

	if (rn_inithead((void **)&rnh, off)) {
#ifndef SMALL_KERNEL
		rnh->rnh_multipath = 1;
#endif /* SMALL_KERNEL */
		rnh->rnh_rtableid = rtableid;
	}

	return (rnh);
}

void
rtable_free(unsigned int rtableid)
{
}

struct rtentry *
rtable_lookup(unsigned int rtableid, struct sockaddr *dst,
    struct sockaddr *mask, struct sockaddr *gateway, uint8_t prio)
{
	struct radix_node_head	*rnh;
	struct radix_node	*rn;
	struct rtentry		*rt = NULL;

	rnh = rtable_get(rtableid, dst->sa_family);
	if (rnh == NULL)
		return (NULL);

	rn = rn_lookup(dst, mask, rnh);
	if (rn == NULL || (rn->rn_flags & RNF_ROOT) != 0)
		goto out;

	rt = ((struct rtentry *)rn);

#ifndef SMALL_KERNEL
	if (rnh->rnh_multipath) {
		rt = rt_mpath_matchgate(rt, gateway, prio);
		if (rt == NULL)
			goto out;
	}
#endif /* !SMALL_KERNEL */

	rtref(rt);

out:
	rtable_put(rnh);
	return (rt);
}

struct rtentry *
rtable_match(unsigned int rtableid, struct sockaddr *dst)
{
	struct radix_node_head	*rnh;
	struct radix_node	*rn;
	struct rtentry		*rt = NULL;

	rnh = rtable_get(rtableid, dst->sa_family);
	if (rnh == NULL)
		return (NULL);

	rn = rn_match(dst, rnh);
	if (rn == NULL || (rn->rn_flags & RNF_ROOT) != 0)
		goto out;

	rt = ((struct rtentry *)rn);
	rtref(rt);

out:
	rtable_put(rnh);
	return (rt);
}

int
rtable_insert(unsigned int rtableid, struct sockaddr *dst,
    struct sockaddr *mask, struct sockaddr *gateway, uint8_t prio,
    struct rtentry *rt)
{
	struct radix_node_head	*rnh;
	struct radix_node	*rn = (struct radix_node *)rt;
	int			 error = 0;

	rnh = rtable_get(rtableid, dst->sa_family);
	if (rnh == NULL)
		return (EAFNOSUPPORT);

#ifndef SMALL_KERNEL
	if (rnh->rnh_multipath) {
		/* Do not permit exactly the same dst/mask/gw pair. */
		if (rt_mpath_conflict(rnh, dst, mask, gateway, prio,
	    	    ISSET(rt->rt_flags, RTF_MPATH))) {
			error = EEXIST;
			goto out;
		}
	}
#endif

	rn = rn_addroute(dst, mask, rnh, rn, prio);
	if (rn == NULL) {
		error = ESRCH;
		goto out;
	}

	rt = ((struct rtentry *)rn);
	rtref(rt);

out:
	rtable_put(rnh);
	return (error);
}

int
rtable_delete(unsigned int rtableid, struct sockaddr *dst,
    struct sockaddr *mask, uint8_t prio, struct rtentry *rt)
{
	struct radix_node_head	*rnh;
	struct radix_node	*rn = (struct radix_node *)rt;
	int			 error = 0;

	rnh = rtable_get(rtableid, dst->sa_family);
	if (rnh == NULL)
		return (EAFNOSUPPORT);

	rn = rn_delete(dst, mask, rnh, rn);
	if (rn == NULL) {
		error = ESRCH;
		goto out;
	}

	if (rn->rn_flags & (RNF_ACTIVE | RNF_ROOT))
		panic("active node flags=%x", rn->rn_flags);

	rt = ((struct rtentry *)rn);
	rtfree(rt);

out:
	rtable_put(rnh);
	return (error);
}

int
rtable_walk(unsigned int rtableid, sa_family_t af,
    int (*func)(struct rtentry *, void *, unsigned int), void *arg)
{
	struct radix_node_head	*rnh;
	int (*f)(struct radix_node *, void *, unsigned int) = (void *)func;
	int error;

	rnh = rtable_get(rtableid, af);
	if (rnh == NULL)
		return (EAFNOSUPPORT);

	error = rn_walktree(rnh, f, arg);

	rtable_put(rnh);
	return (error);
}

#ifndef SMALL_KERNEL
int
rtable_mpath_capable(unsigned int rtableid, sa_family_t af)
{
	struct radix_node_head	*rnh;
	int mpath;

	rnh = rtable_get(rtableid, af);
	if (rnh == NULL)
		return (0);

	mpath = rnh->rnh_multipath;

	rtable_put(rnh);
	return (mpath);
}

/* Gateway selection by Hash-Threshold (RFC 2992) */
struct rtentry *
rtable_mpath_select(struct rtentry *rt, uint32_t hash)
{
	struct rtentry *mrt = rt;
	int npaths, threshold;

	npaths = 1;
	while ((mrt = rtable_mpath_next(mrt)) != NULL)
		npaths++;

	threshold = (0xffff / npaths) + 1;

	mrt = rt;
	while (hash > threshold && mrt != NULL) {
		/* stay within the multipath routes */
		mrt = rtable_mpath_next(mrt);
		hash -= threshold;
	}

	/* if gw selection fails, use the first match (default) */
	if (mrt != NULL) {
		rtref(mrt);
		rtfree(rt);
		rt = mrt;
	}

	return (rt);
}

void
rtable_mpath_reprio(struct rtentry *rt, uint8_t newprio)
{
	struct radix_node	*rn = (struct radix_node *)rt;

	rn_mpath_reprio(rn, newprio);
}

struct rtentry *
rtable_mpath_next(struct rtentry *rt)
{
	struct radix_node *rn = (struct radix_node *)rt;

	return ((struct rtentry *)rn_mpath_next(rn, RMP_MODE_ACTIVE));
}
#endif /* SMALL_KERNEL */

#else /* ART */

struct pool		an_pool;	/* pool for ART node structures */

static inline int	 satoplen(struct art_root *, struct sockaddr *);
static inline uint8_t	*satoaddr(struct art_root *, struct sockaddr *);

void
rtable_init_backend(unsigned int keylen)
{
	pool_init(&an_pool, sizeof(struct art_node), 0, 0, 0, "art node", NULL);
}

void *
rtable_alloc(unsigned int rtableid, sa_family_t af, unsigned int off)
{
	return (art_alloc(rtableid, off));
}

void
rtable_free(unsigned int rtableid)
{
}

struct rtentry *
rtable_lookup(unsigned int rtableid, struct sockaddr *dst,
    struct sockaddr *mask, struct sockaddr *gateway, uint8_t prio)
{
	struct art_root			*ar;
	struct art_node			*an;
	struct rtentry			*rt = NULL;
	uint8_t				*addr;
	int				 plen;

	ar = rtable_get(rtableid, dst->sa_family);
	if (ar == NULL)
		return (NULL);

	addr = satoaddr(ar, dst);

	/* No need for a perfect match. */
	if (mask == NULL) {
		an = art_match(ar, addr);
		if (an == NULL)
			goto out;
	} else {
		plen = satoplen(ar, mask);
		if (plen == -1)
			goto out;

		an = art_lookup(ar, addr, plen);
		/* Make sure we've got a perfect match. */
		if (an == NULL || an->an_plen != plen ||
		    memcmp(an->an_dst, dst, dst->sa_len))
			goto out;
	}

#ifdef SMALL_KERNEL
	rt = SLIST_FIRST(&an->an_rtlist);
#else
	SLIST_FOREACH(rt, &an->an_rtlist, rt_next) {
		if (prio != RTP_ANY &&
		    (rt->rt_priority & RTP_MASK) != (prio & RTP_MASK))
			continue;

		if (gateway == NULL)
			break;

		if (rt->rt_gateway->sa_len == gateway->sa_len &&
		    memcmp(rt->rt_gateway, gateway, gateway->sa_len) == 0)
			break;
	}
	if (rt == NULL)
		goto out;
#endif /* SMALL_KERNEL */

	rtref(rt);

out:
	rtable_put(ar);
	return (rt);
}

struct rtentry *
rtable_match(unsigned int rtableid, struct sockaddr *dst)
{
	struct art_root			*ar;
	struct art_node			*an;
	struct rtentry			*rt = NULL;
	uint8_t				*addr;

	ar = rtable_get(rtableid, dst->sa_family);
	if (ar == NULL)
		return (NULL);

	addr = satoaddr(ar, dst);
	an = art_match(ar, addr);
	if (an == NULL)
		goto out;

	rt = SLIST_FIRST(&an->an_rtlist);
	rtref(rt);

out:
	rtable_put(ar);
	return (rt);
}

int
rtable_insert(unsigned int rtableid, struct sockaddr *dst,
    struct sockaddr *mask, struct sockaddr *gateway, uint8_t prio,
    struct rtentry *rt)
{
#ifndef SMALL_KERNEL
	struct rtentry			*mrt;
#endif
	struct art_root			*ar;
	struct art_node			*an, *prev;
	uint8_t				*addr;
	int				 plen, error = 0;

	ar = rtable_get(rtableid, dst->sa_family);
	if (ar == NULL)
		return (EAFNOSUPPORT);

	addr = satoaddr(ar, dst);
	plen = satoplen(ar, mask);
	if (plen == -1) {
		error = EINVAL;
		goto out;
	}

#ifndef SMALL_KERNEL
	/* Do not permit exactly the same dst/mask/gw pair. */
	an = art_lookup(ar, addr, plen);
	if (an != NULL && an->an_plen == plen &&
	    !memcmp(an->an_dst, dst, dst->sa_len)) {
	    	struct rtentry	*mrt;
		int		 mpathok = ISSET(rt->rt_flags, RTF_MPATH);

		SLIST_FOREACH(mrt, &an->an_rtlist, rt_next) {
			if (prio != RTP_ANY &&
			    (mrt->rt_priority & RTP_MASK) != (prio & RTP_MASK))
				continue;

			if (!mpathok ||
			    (mrt->rt_gateway->sa_len == gateway->sa_len &&
			    !memcmp(mrt->rt_gateway, gateway, gateway->sa_len))){
			    	error = EEXIST;
			    	goto out;
			}
		}
	}
#endif

	/*
	 * XXX Allocating a sockaddr for the mask per node wastes a lot
	 * of memory, thankfully we'll get rid of that when rt_mask()
	 * will be no more.
	 */
	if (mask != NULL) {
		struct sockaddr		*msk;

		msk = malloc(dst->sa_len, M_RTABLE, M_NOWAIT | M_ZERO);
		if (msk == NULL) {
			error = ENOMEM;
			goto out;
		}
		memcpy(msk, mask, dst->sa_len);
		rt->rt_mask = msk;
	}

	an = pool_get(&an_pool, PR_NOWAIT | PR_ZERO);
	if (an == NULL) {
		error = ENOBUFS;
		goto out;
	}

	an->an_dst = dst;
	an->an_plen = plen;

	prev = art_insert(ar, an, addr, plen);
	if (prev == NULL) {
		free(rt->rt_mask, M_RTABLE, 0);
		rt->rt_mask = NULL;
		pool_put(&an_pool, an);
		error = ESRCH;
		goto out;
	}

	if (prev == an) {
		rt->rt_flags &= ~RTF_MPATH;
	} else {
		pool_put(&an_pool, an);
#ifndef SMALL_KERNEL
		an = prev;

		mrt = SLIST_FIRST(&an->an_rtlist);
		KASSERT(mrt != NULL);
		KASSERT((rt->rt_flags & RTF_MPATH) || mrt->rt_priority != prio);

		/*
		 * An ART node with the same destination/netmask already
		 * exists, MPATH conflict must have been already checked.
		 */
		if (rt->rt_flags & RTF_MPATH) {
			/*
			 * Only keep the RTF_MPATH flag if two routes have
			 * the same gateway.
			 */
			rt->rt_flags &= ~RTF_MPATH;
			SLIST_FOREACH(mrt, &an->an_rtlist, rt_next) {
				if (mrt->rt_priority == prio) {
					mrt->rt_flags |= RTF_MPATH;
					rt->rt_flags |= RTF_MPATH;
				}
			}
		}
#else
		error = EEXIST;
		goto out;
#endif /* SMALL_KERNEL */
	}

	rt->rt_node = an;
	rt->rt_dest = dst;
	rtref(rt);
	SLIST_INSERT_HEAD(&an->an_rtlist, rt, rt_next);

#ifndef SMALL_KERNEL
	/* Put newly inserted entry at the right place. */
	rtable_mpath_reprio(rt, rt->rt_priority);
#endif /* SMALL_KERNEL */

out:
	rtable_put(ar);
	return (error);
}

int
rtable_delete(unsigned int rtableid, struct sockaddr *dst,
    struct sockaddr *mask, uint8_t prio, struct rtentry *rt)
{
	struct art_root			*ar;
	struct art_node			*an = rt->rt_node;
	uint8_t				*addr;
	int				 plen, error = 0;
#ifndef SMALL_KERNEL
	struct rtentry			*mrt;
	int				 npaths = 0;

	/*
	 * If other multipath route entries are still attached to
	 * this ART node we only have to unlink it.
	 */
	SLIST_FOREACH(mrt, &an->an_rtlist, rt_next)
		npaths++;

	if (npaths > 1) {
		free(rt->rt_mask, M_RTABLE, 0);
		rt->rt_mask = NULL;
		rt->rt_node = NULL;
		SLIST_REMOVE(&an->an_rtlist, rt, rtentry, rt_next);
		KASSERT(rt->rt_refcnt >= 1);
		rtfree(rt);

		mrt = SLIST_FIRST(&an->an_rtlist);
		an->an_dst = mrt->rt_dest;
		if (npaths == 2)
			mrt->rt_flags &= ~RTF_MPATH;
		return (0);
	}
#endif /* SMALL_KERNEL */

	ar = rtable_get(rtableid, dst->sa_family);
	if (ar == NULL)
		return (EAFNOSUPPORT);

#ifdef DIAGNOSTIC
	if (memcmp(dst, an->an_dst, dst->sa_len))
		panic("destination do not match");
	if (mask != NULL && an->an_plen != satoplen(ar, mask))
		panic("mask do not match");
#endif

	addr = satoaddr(ar, an->an_dst);
	plen = an->an_plen;

	if (art_delete(ar, an, addr, plen) == NULL) {
		error = ESRCH;
		goto out;
	}

	/*
	 * XXX Is it safe to free the mask now?  Are we sure rt_mask()
	 * is only used when entries are in the table?
	 */
	free(rt->rt_mask, M_RTABLE, 0);
	rt->rt_node = NULL;
	rt->rt_mask = NULL;
	SLIST_REMOVE(&an->an_rtlist, rt, rtentry, rt_next);
	KASSERT(rt->rt_refcnt >= 1);
	rtfree(rt);

	pool_put(&an_pool, an);
out:
	rtable_put(ar);
	return (error);
}

struct rtable_walk_cookie {
	int		(*rwc_func)(struct rtentry *, void *, unsigned int);
	void		 *rwc_arg;
	unsigned int	  rwc_rid;
};

/*
 * Helper for rtable_walk to keep the ART code free from any "struct rtentry".
 */
int
rtable_walk_helper(struct art_node *an, void *xrwc)
{
	struct rtable_walk_cookie	*rwc = xrwc;
	struct rtentry			*rt, *nrt;
	int				 error = 0;

	SLIST_FOREACH_SAFE(rt, &an->an_rtlist, rt_next, nrt) {
		if ((error = (*rwc->rwc_func)(rt, rwc->rwc_arg, rwc->rwc_rid)))
			break;
	}

	return (error);
}

int
rtable_walk(unsigned int rtableid, sa_family_t af,
    int (*func)(struct rtentry *, void *, unsigned int), void *arg)
{
	struct art_root			*ar;
	struct rtable_walk_cookie	 rwc;
	int				 error;

	ar = rtable_get(rtableid, af);
	if (ar == NULL)
		return (EAFNOSUPPORT);

	rwc.rwc_func = func;
	rwc.rwc_arg = arg;
	rwc.rwc_rid = rtableid;

	error = art_walk(ar, rtable_walk_helper, &rwc);

	rtable_put(ar);
	return (error);
}

#ifndef SMALL_KERNEL
int
rtable_mpath_capable(unsigned int rtableid, sa_family_t af)
{
	return (1);
}

/* Gateway selection by Hash-Threshold (RFC 2992) */
struct rtentry *
rtable_mpath_select(struct rtentry *rt, uint32_t hash)
{
	struct art_node			*an = rt->rt_node;
	struct rtentry			*mrt;
	int				 npaths, threshold;

	npaths = 0;
	SLIST_FOREACH(mrt, &an->an_rtlist, rt_next) {
		/* Only count nexthops with the same priority. */
		if (mrt->rt_priority == rt->rt_priority)
			npaths++;
	}

	threshold = (0xffff / npaths) + 1;

	mrt = SLIST_FIRST(&an->an_rtlist);
	while (hash > threshold && mrt != NULL) {
		if (mrt->rt_priority == rt->rt_priority)
			hash -= threshold;
		mrt = SLIST_NEXT(mrt, rt_next);
	}

	if (mrt != NULL) {
		rtref(mrt);
		rtfree(rt);
		rt = mrt;
	}

	return (rt);
}

void
rtable_mpath_reprio(struct rtentry *rt, uint8_t prio)
{
	struct art_node			*an = rt->rt_node;
	struct rtentry			*mrt, *prt;

	SLIST_REMOVE(&an->an_rtlist, rt, rtentry, rt_next);
	rt->rt_priority = prio;

	if ((mrt = SLIST_FIRST(&an->an_rtlist)) != NULL) {
		/*
		 * Select the order of the MPATH routes.
		 */
		while (SLIST_NEXT(mrt, rt_next) != NULL) {
			if (mrt->rt_priority > prio)
				break;
		    	prt = mrt;
			mrt = SLIST_NEXT(mrt, rt_next);
		}

		if (mrt->rt_priority > prio) {
			/* prt -> rt -> mrt */
			SLIST_INSERT_AFTER(prt, rt, rt_next);
		} else {
			/* prt -> mrt -> rt */
			SLIST_INSERT_AFTER(mrt, rt, rt_next);
		}
	} else {
		SLIST_INSERT_HEAD(&an->an_rtlist, rt, rt_next);
	}
}

struct rtentry *
rtable_mpath_next(struct rtentry *rt)
{
	return (SLIST_NEXT(rt, rt_next));
}
#endif /* SMALL_KERNEL */

/*
 * Return a pointer to the address (key).  This is an heritage from the
 * BSD radix tree needed to skip the non-address fields from the flavor
 * of "struct sockaddr" used by this routing table.
 */
static inline uint8_t *
satoaddr(struct art_root *at, struct sockaddr *sa)
{
	return (((uint8_t *)sa) + at->ar_off);
}

/*
 * Return the prefix length of a mask.
 */
static inline int
satoplen(struct art_root *ar, struct sockaddr *mask)
{
	uint8_t				*ap, *ep;
	int				 skip, mlen, plen = 0;

	/* Host route */
	if (mask == NULL)
		return (ar->ar_alen);

	mlen = mask->sa_len;

	/* Default route */
	if (mlen == 0)
		return (0);

	skip = ar->ar_off;

	ap = (uint8_t *)((uint8_t *)mask) + skip;
	ep = (uint8_t *)((uint8_t *)mask) + mlen;
	if (ap > ep)
		return (-1);

	if (ap == ep)
		return (0);

	/* "Beauty" adapted from sbin/route/show.c ... */
	while (ap < ep) {
		switch (*ap) {
		case 0xff:
			plen += 8;
			ap++;
			break;
		case 0xfe:
			plen += 7;
			ap++;
			goto out;
		case 0xfc:
			plen += 6;
			ap++;
			goto out;
		case 0xf8:
			plen += 5;
			ap++;
			goto out;
		case 0xf0:
			plen += 4;
			ap++;
			goto out;
		case 0xe0:
			plen += 3;
			ap++;
			goto out;
		case 0xc0:
			plen += 2;
			ap++;
			goto out;
		case 0x80:
			plen += 1;
			ap++;
			goto out;
		case 0x00:
			goto out;
		default:
			/* Non contiguous mask. */
			return (-1);
		}

	}

out:
#ifdef DIAGNOSTIC
	for (; ap < ep; ap++) {
		if (*ap != 0x00)
			return (-1);
	}
#endif

	return (plen);
}
#endif /* ART */
