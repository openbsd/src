/*	$OpenBSD: rtable.c,v 1.31 2015/12/02 16:49:58 bluhm Exp $ */

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
#include <sys/srp.h>
#endif

#include <net/rtable.h>
#include <net/route.h>

/*
 * Structures used by rtable_get() to retrieve the corresponding
 * routing table for a given pair of ``af'' and ``rtableid''.
 *
 * Note that once allocated routing table heads are never freed.
 * This way we do not need to reference count them.
 *
 *	afmap		    rtmap/dommp
 *   -----------          ---------     -----
 *   |   0     |--------> | 0 | 0 | ... | 0 |	Array mapping rtableid (=index)
 *   -----------          ---------     -----   to rdomain (=value).
 *   | AF_INET |.
 *   ----------- `.       .---------.     .---------.
 *       ...	   `----> | rtable0 | ... | rtableN |	Array of pointers for
 *   -----------          '---------'     '---------'	IPv4 routing tables
 *   | AF_MPLS |            			 	indexed by ``rtableid''.
 *   -----------
 */
struct srp	  *afmap;
uint8_t		   af2idx[AF_MAX+1];	/* To only allocate supported AF */
uint8_t		   af2idx_max;

/* Array of routing table pointers. */
struct rtmap {
	unsigned int	   limit;
	void		 **tbl;
};

/* Array of rtableid -> rdomain mapping. */
struct dommp {
	unsigned int	   limit;
	unsigned int	  *dom;
};

unsigned int	   rtmap_limit = 0;

void		   rtmap_init(void);
void		   rtmap_grow(unsigned int, sa_family_t);
void		   rtmap_dtor(void *, void *);

struct srp_gc	   rtmap_gc = SRP_GC_INITIALIZER(rtmap_dtor, NULL);

struct rtentry	  *rtable_mpath_select(struct rtentry *, uint32_t *);
void		   rtable_init_backend(unsigned int);
void		  *rtable_alloc(unsigned int, sa_family_t, unsigned int);
void		  *rtable_get(unsigned int, sa_family_t);

void
rtmap_init(void)
{
	struct domain	*dp;
	int		 i;

	/* Start with a single table for every domain that requires it. */
	for (i = 0; (dp = domains[i]) != NULL; i++) {
		if (dp->dom_rtoffset == 0)
			continue;

		rtmap_grow(1, dp->dom_family);
	}

	/* Initialize the rtableid->rdomain mapping table. */
	rtmap_grow(1, 0);

	rtmap_limit = 1;
}

/*
 * Grow the size of the array of routing table for AF ``af'' to ``nlimit''.
 */
void
rtmap_grow(unsigned int nlimit, sa_family_t af)
{
	struct rtmap	*map, *nmap;
	int		 i;

	KERNEL_ASSERT_LOCKED();

	KASSERT(nlimit > rtmap_limit);

	nmap = malloc(sizeof(*nmap), M_RTABLE, M_WAITOK);
	nmap->limit = nlimit;
	nmap->tbl = mallocarray(nlimit, sizeof(*nmap[0].tbl), M_RTABLE,
	    M_WAITOK|M_ZERO);

	map = srp_get_locked(&afmap[af2idx[af]]);
	if (map != NULL) {
		KASSERT(map->limit == rtmap_limit);

		for (i = 0; i < map->limit; i++)
			nmap->tbl[i] = map->tbl[i];
	}

	srp_update_locked(&rtmap_gc, &afmap[af2idx[af]], nmap);
}

void
rtmap_dtor(void *null, void *xmap)
{
	struct rtmap	*map = xmap;

	/*
	 * doesnt need to be serialized since this is the last reference
	 * to this map. there's nothing to race against.
	 */
	free(map->tbl, M_RTABLE, map->limit * sizeof(*map[0].tbl));
	free(map, M_RTABLE, sizeof(*map));
}

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
		if (dp->dom_rtoffset == 0)
			continue;

		af2idx[dp->dom_family] = af2idx_max++;
		if (dp->dom_rtkeylen > keylen)
			keylen = dp->dom_rtkeylen;

	}
	rtable_init_backend(keylen);

	/*
	 * Allocate AF-to-id table now that we now how many AFs this
	 * kernel supports.
	 */
	afmap = mallocarray(af2idx_max + 1, sizeof(*afmap), M_RTABLE,
	    M_WAITOK|M_ZERO);

	rtmap_init();
}

int
rtable_add(unsigned int id)
{
	struct domain	*dp;
	void		*tbl;
	struct rtmap	*map;
	struct dommp	*dmm;
	sa_family_t	 af;
	unsigned int	 off;
	int		 i;

	KERNEL_ASSERT_LOCKED();

	if (id > RT_TABLEID_MAX)
		return (EINVAL);

	if (rtable_exists(id))
		return (EEXIST);

	for (i = 0; (dp = domains[i]) != NULL; i++) {
		if (dp->dom_rtoffset == 0)
			continue;

		af = dp->dom_family;
		off = dp->dom_rtoffset;

		if (id >= rtmap_limit)
			rtmap_grow(id + 1, af);

		tbl = rtable_alloc(id, af, off);
		if (tbl == NULL)
			return (ENOMEM);

		map = srp_get_locked(&afmap[af2idx[af]]);
		map->tbl[id] = tbl;
	}

	/* Reflect possible growth. */
	if (id >= rtmap_limit) {
		rtmap_grow(id + 1, 0);
		rtmap_limit = id + 1;
	}

	/* Use main rtable/rdomain by default. */
	dmm = srp_get_locked(&afmap[0]);
	dmm->dom[id] = 0;

	return (0);
}

void *
rtable_get(unsigned int rtableid, sa_family_t af)
{
	struct rtmap	*map;
	void		*tbl = NULL;

	if (af >= nitems(af2idx) || af2idx[af] == 0)
		return (NULL);

	map = srp_enter(&afmap[af2idx[af]]);
	if (rtableid < map->limit)
		tbl = map->tbl[rtableid];
	srp_leave(&afmap[af2idx[af]], map);

	return (tbl);
}

int
rtable_exists(unsigned int rtableid)
{
	struct domain	*dp;
	void		*tbl;
	int		 i;

	for (i = 0; (dp = domains[i]) != NULL; i++) {
		if (dp->dom_rtoffset == 0)
			continue;

		tbl = rtable_get(rtableid, dp->dom_family);
		if (tbl != NULL)
			return (1);
	}

	return (0);
}

unsigned int
rtable_l2(unsigned int rtableid)
{
	struct dommp	*dmm;
	unsigned int	 rdomain = 0;

	dmm = srp_enter(&afmap[0]);
	if (rtableid < dmm->limit)
		rdomain = dmm->dom[rtableid];
	srp_leave(&afmap[0], dmm);

	return (rdomain);
}

void
rtable_l2set(unsigned int rtableid, unsigned int rdomain)
{
	struct dommp	*dmm;

	KERNEL_ASSERT_LOCKED();

	if (!rtable_exists(rtableid) || !rtable_exists(rdomain))
		return;

	dmm = srp_get_locked(&afmap[0]);
	dmm->dom[rtableid] = rdomain;
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

struct rtentry *
rtable_lookup(unsigned int rtableid, struct sockaddr *dst,
    struct sockaddr *mask, struct sockaddr *gateway, uint8_t prio)
{
	struct radix_node_head	*rnh;
	struct radix_node	*rn;
	struct rtentry		*rt;

	rnh = rtable_get(rtableid, dst->sa_family);
	if (rnh == NULL)
		return (NULL);

	rn = rn_lookup(dst, mask, rnh);
	if (rn == NULL || (rn->rn_flags & RNF_ROOT) != 0)
		return (NULL);

	rt = ((struct rtentry *)rn);

#ifndef SMALL_KERNEL
	if (rnh->rnh_multipath) {
		rt = rt_mpath_matchgate(rt, gateway, prio);
		if (rt == NULL)
			return (NULL);
	}
#endif /* !SMALL_KERNEL */

	rtref(rt);
	return (rt);
}

struct rtentry *
rtable_match(unsigned int rtableid, struct sockaddr *dst, uint32_t *src)
{
	struct radix_node_head	*rnh;
	struct radix_node	*rn;
	struct rtentry		*rt;

	rnh = rtable_get(rtableid, dst->sa_family);
	if (rnh == NULL)
		return (NULL);

	rn = rn_match(dst, rnh);
	if (rn == NULL || (rn->rn_flags & RNF_ROOT) != 0)
		return (NULL);

	rt = ((struct rtentry *)rn);
	rtref(rt);

#ifndef SMALL_KERNEL
	rt = rtable_mpath_select(rt, src);
#endif /* SMALL_KERNEL */

	return (rt);
}

int
rtable_insert(unsigned int rtableid, struct sockaddr *dst,
    struct sockaddr *mask, struct sockaddr *gateway, uint8_t prio,
    struct rtentry *rt)
{
	struct radix_node_head	*rnh;
	struct radix_node	*rn = (struct radix_node *)rt;

	rnh = rtable_get(rtableid, dst->sa_family);
	if (rnh == NULL)
		return (EAFNOSUPPORT);

#ifndef SMALL_KERNEL
	if (rnh->rnh_multipath) {
		/* Do not permit exactly the same dst/mask/gw pair. */
		if (rt_mpath_conflict(rnh, dst, mask, gateway, prio,
	    	    ISSET(rt->rt_flags, RTF_MPATH))) {
			return (EEXIST);
		}
	}
#endif /* SMALL_KERNEL */

	rn = rn_addroute(dst, mask, rnh, rn, prio);
	if (rn == NULL)
		return (ESRCH);

	rt = ((struct rtentry *)rn);
	rtref(rt);

	return (0);
}

int
rtable_delete(unsigned int rtableid, struct sockaddr *dst,
    struct sockaddr *mask, struct rtentry *rt)
{
	struct radix_node_head	*rnh;
	struct radix_node	*rn = (struct radix_node *)rt;

	rnh = rtable_get(rtableid, dst->sa_family);
	if (rnh == NULL)
		return (EAFNOSUPPORT);

	rn = rn_delete(dst, mask, rnh, rn);
	if (rn == NULL)
		return (ESRCH);

	if (rn->rn_flags & (RNF_ACTIVE | RNF_ROOT))
		panic("active node flags=%x", rn->rn_flags);

	rt = ((struct rtentry *)rn);
	rtfree(rt);

	return (0);
}

int
rtable_walk(unsigned int rtableid, sa_family_t af,
    int (*func)(struct rtentry *, void *, unsigned int), void *arg)
{
	struct radix_node_head	*rnh;
	int (*f)(struct radix_node *, void *, unsigned int) = (void *)func;

	rnh = rtable_get(rtableid, af);
	if (rnh == NULL)
		return (EAFNOSUPPORT);

	return (rn_walktree(rnh, f, arg));
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
	return (mpath);
}

/* Gateway selection by Hash-Threshold (RFC 2992) */
struct rtentry *
rtable_mpath_select(struct rtentry *rt, uint32_t *src)
{
	struct rtentry *mrt = rt;
	int npaths, threshold, hash;

	if ((hash = rt_hash(rt, src)) == -1)
		return (rt);

	KASSERT(hash <= 0xffff);

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

struct pool		 an_pool;	/* pool for ART node structures */

static inline int	 satoplen(struct art_root *, struct sockaddr *);
static inline uint8_t	*satoaddr(struct art_root *, struct sockaddr *);

void	rtentry_ref(void *, void *);
void	rtentry_unref(void *, void *);

struct srpl_rc rt_rc = SRPL_RC_INITIALIZER(rtentry_ref, rtentry_unref, NULL);

void
rtable_init_backend(unsigned int keylen)
{
	art_init();
	pool_init(&an_pool, sizeof(struct art_node), 0, 0, 0, "art_node", NULL);
}

void *
rtable_alloc(unsigned int rtableid, sa_family_t af, unsigned int off)
{
	return (art_alloc(rtableid, off));
}

struct rtentry *
rtable_lookup(unsigned int rtableid, struct sockaddr *dst,
    struct sockaddr *mask, struct sockaddr *gateway, uint8_t prio)
{
	struct art_root			*ar;
	struct art_node			*an;
	struct rtentry			*rt;
	struct srpl_iter		 i;
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
			return (NULL);
	} else {
		plen = satoplen(ar, mask);
		if (plen == -1)
			return (NULL);

		an = art_lookup(ar, addr, plen);
		/* Make sure we've got a perfect match. */
		if (an == NULL || an->an_plen != plen ||
		    memcmp(an->an_dst, dst, dst->sa_len))
			return (NULL);
	}

#ifdef SMALL_KERNEL
	rt = SRPL_ENTER(&an->an_rtlist, &i);
#else
	SRPL_FOREACH(rt, &an->an_rtlist, &i, rt_next) {
		if (prio != RTP_ANY &&
		    (rt->rt_priority & RTP_MASK) != (prio & RTP_MASK))
			continue;

		if (gateway == NULL)
			break;

		if (rt->rt_gateway->sa_len == gateway->sa_len &&
		    memcmp(rt->rt_gateway, gateway, gateway->sa_len) == 0)
			break;
	}
	if (rt == NULL) {
		SRPL_LEAVE(&i, rt);
		return (NULL);
	}
#endif /* SMALL_KERNEL */

	rtref(rt);
	SRPL_LEAVE(&i, rt);

	return (rt);
}

struct rtentry *
rtable_match(unsigned int rtableid, struct sockaddr *dst, uint32_t *src)
{
	struct art_root			*ar;
	struct art_node			*an;
	struct rtentry			*rt;
	struct srpl_iter		 i;
	uint8_t				*addr;

	ar = rtable_get(rtableid, dst->sa_family);
	if (ar == NULL)
		return (NULL);

	addr = satoaddr(ar, dst);
	an = art_match(ar, addr);
	if (an == NULL)
		return (NULL);

	rt = SRPL_ENTER(&an->an_rtlist, &i);
	rtref(rt);
	SRPL_LEAVE(&i, rt);

#ifndef SMALL_KERNEL
	rt = rtable_mpath_select(rt, src);
#endif /* SMALL_KERNEL */

	return (rt);
}

int
rtable_insert(unsigned int rtableid, struct sockaddr *dst,
    struct sockaddr *mask, struct sockaddr *gateway, uint8_t prio,
    struct rtentry *rt)
{
#ifndef SMALL_KERNEL
	struct rtentry			*mrt;
#endif /* SMALL_KERNEL */
	struct art_root			*ar;
	struct art_node			*an, *prev;
	uint8_t				*addr;
	int				 plen;

	KERNEL_ASSERT_LOCKED();

	ar = rtable_get(rtableid, dst->sa_family);
	if (ar == NULL)
		return (EAFNOSUPPORT);

	addr = satoaddr(ar, dst);
	plen = satoplen(ar, mask);
	if (plen == -1)
		return (EINVAL);

#ifndef SMALL_KERNEL
	/* Do not permit exactly the same dst/mask/gw pair. */
	an = art_lookup(ar, addr, plen);
	if (an != NULL && an->an_plen == plen &&
	    !memcmp(an->an_dst, dst, dst->sa_len)) {
	    	struct rtentry	*mrt;
		int		 mpathok = ISSET(rt->rt_flags, RTF_MPATH);

		SRPL_FOREACH_LOCKED(mrt, &an->an_rtlist, rt_next) {
			if (prio != RTP_ANY &&
			    (mrt->rt_priority & RTP_MASK) != (prio & RTP_MASK))
				continue;

			if (!mpathok ||
			    (mrt->rt_gateway->sa_len == gateway->sa_len &&
			    !memcmp(mrt->rt_gateway, gateway, gateway->sa_len))){
			    	return (EEXIST);
			}
		}
	}
#endif /* SMALL_KERNEL */

	/*
	 * XXX Allocating a sockaddr for the mask per node wastes a lot
	 * of memory, thankfully we'll get rid of that when rt_mask()
	 * will be no more.
	 */
	if (mask != NULL) {
		struct sockaddr		*msk;

		msk = malloc(dst->sa_len, M_RTABLE, M_NOWAIT | M_ZERO);
		if (msk == NULL)
			return (ENOMEM);
		memcpy(msk, mask, dst->sa_len);
		rt->rt_mask = msk;
	}

	an = pool_get(&an_pool, PR_NOWAIT | PR_ZERO);
	if (an == NULL)
		return (ENOBUFS);

	an->an_dst = dst;
	an->an_plen = plen;
	SRPL_INIT(&an->an_rtlist);

	prev = art_insert(ar, an, addr, plen);
	if (prev == NULL) {
		free(rt->rt_mask, M_RTABLE, 0);
		rt->rt_mask = NULL;
		pool_put(&an_pool, an);
		return (ESRCH);
	}

	if (prev == an) {
		rt->rt_flags &= ~RTF_MPATH;
	} else {
		pool_put(&an_pool, an);
#ifndef SMALL_KERNEL
		an = prev;

		mrt = SRPL_FIRST_LOCKED(&an->an_rtlist);
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
			SRPL_FOREACH_LOCKED(mrt, &an->an_rtlist, rt_next) {
				if (mrt->rt_priority == prio) {
					mrt->rt_flags |= RTF_MPATH;
					rt->rt_flags |= RTF_MPATH;
				}
			}
		}
#else
		return (EEXIST);
#endif /* SMALL_KERNEL */
	}

	rt->rt_node = an;
	rt->rt_dest = dst;
	rtref(rt);
	SRPL_INSERT_HEAD_LOCKED(&rt_rc, &an->an_rtlist, rt, rt_next);

#ifndef SMALL_KERNEL
	/* Put newly inserted entry at the right place. */
	rtable_mpath_reprio(rt, rt->rt_priority);
#endif /* SMALL_KERNEL */

	return (0);
}

int
rtable_delete(unsigned int rtableid, struct sockaddr *dst,
    struct sockaddr *mask, struct rtentry *rt)
{
	struct art_root			*ar;
	struct art_node			*an = rt->rt_node;
	uint8_t				*addr;
	int				 plen;
#ifndef SMALL_KERNEL
	struct rtentry			*mrt;
	int				 npaths = 0;

	KERNEL_ASSERT_LOCKED();

	/*
	 * If other multipath route entries are still attached to
	 * this ART node we only have to unlink it.
	 */
	SRPL_FOREACH_LOCKED(mrt, &an->an_rtlist, rt_next)
		npaths++;

	if (npaths > 1) {
		free(rt->rt_mask, M_RTABLE, 0);
		rt->rt_mask = NULL;
		rt->rt_node = NULL;
		KASSERT(rt->rt_refcnt >= 2);
		SRPL_REMOVE_LOCKED(&rt_rc, &an->an_rtlist, rt, rtentry,
		    rt_next);
		rtfree(rt);

		mrt = SRPL_FIRST_LOCKED(&an->an_rtlist);
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
#endif /* DIAGNOSTIC */

	addr = satoaddr(ar, an->an_dst);
	plen = an->an_plen;

	if (art_delete(ar, an, addr, plen) == NULL)
		return (ESRCH);

	/*
	 * XXX Is it safe to free the mask now?  Are we sure rt_mask()
	 * is only used when entries are in the table?
	 */
	free(rt->rt_mask, M_RTABLE, 0);
	rt->rt_node = NULL;
	rt->rt_mask = NULL;
	KASSERT(rt->rt_refcnt >= 2);
	SRPL_REMOVE_LOCKED(&rt_rc, &an->an_rtlist, rt, rtentry, rt_next);
	rtfree(rt);

	pool_put(&an_pool, an);
	return (0);
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

	KERNEL_ASSERT_LOCKED();

	SRPL_FOREACH_SAFE_LOCKED(rt, &an->an_rtlist, rt_next, nrt) {
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

	ar = rtable_get(rtableid, af);
	if (ar == NULL)
		return (EAFNOSUPPORT);

	rwc.rwc_func = func;
	rwc.rwc_arg = arg;
	rwc.rwc_rid = rtableid;

	return (art_walk(ar, rtable_walk_helper, &rwc));
}

#ifndef SMALL_KERNEL
int
rtable_mpath_capable(unsigned int rtableid, sa_family_t af)
{
	return (1);
}

/* Gateway selection by Hash-Threshold (RFC 2992) */
struct rtentry *
rtable_mpath_select(struct rtentry *rt, uint32_t *src)
{
	struct art_node			*an = rt->rt_node;
	struct rtentry			*mrt;
	struct srpl_iter		 i;
	int				 npaths, threshold, hash;

	if ((hash = rt_hash(rt, src)) == -1)
		return (rt);

	KASSERT(hash <= 0xffff);

	npaths = 0;
	SRPL_FOREACH(mrt, &an->an_rtlist, &i, rt_next) {
		/* Only count nexthops with the same priority. */
		if (mrt->rt_priority == rt->rt_priority)
			npaths++;
	}
	SRPL_LEAVE(&i, mrt);

	threshold = (0xffff / npaths) + 1;

	mrt = SRPL_ENTER(&an->an_rtlist, &i);
	while (hash > threshold && mrt != NULL) {
		if (mrt->rt_priority == rt->rt_priority)
			hash -= threshold;
		mrt = SRPL_NEXT(&i, mrt, rt_next);
	}

	if (mrt != NULL) {
		rtref(mrt);
		rtfree(rt);
		rt = mrt;
	}
	SRPL_LEAVE(&i, mrt);

	return (rt);
}

void
rtable_mpath_reprio(struct rtentry *rt, uint8_t prio)
{
	struct art_node			*an = rt->rt_node;
	struct rtentry			*mrt, *prt = NULL;

	KERNEL_ASSERT_LOCKED();

	SRPL_REMOVE_LOCKED(&rt_rc, &an->an_rtlist, rt, rtentry, rt_next);
	rt->rt_priority = prio;

	if ((mrt = SRPL_FIRST_LOCKED(&an->an_rtlist)) != NULL) {
		/*
		 * Select the order of the MPATH routes.
		 */
		while (SRPL_NEXT_LOCKED(mrt, rt_next) != NULL) {
			if (mrt->rt_priority > prio)
				break;
		    	prt = mrt;
			mrt = SRPL_NEXT_LOCKED(mrt, rt_next);
		}

		if (mrt->rt_priority > prio) {
			/*
			 * ``rt'' has a higher (smaller) priority than
			 * ``mrt'' so put it before in the list.
			 */
			if (prt != NULL) {
				SRPL_INSERT_AFTER_LOCKED(&rt_rc, prt, rt,
				    rt_next);
			} else {
				SRPL_INSERT_HEAD_LOCKED(&rt_rc, &an->an_rtlist,
				    rt, rt_next);
			}
		} else {
			SRPL_INSERT_AFTER_LOCKED(&rt_rc, mrt, rt, rt_next);
		}
	} else {
		SRPL_INSERT_HEAD_LOCKED(&rt_rc, &an->an_rtlist, rt, rt_next);
	}
}

struct rtentry *
rtable_mpath_next(struct rtentry *rt)
{
	KERNEL_ASSERT_LOCKED();
	return (SRPL_NEXT_LOCKED(rt, rt_next));
}
#endif /* SMALL_KERNEL */

void
rtentry_ref(void *null, void *xrt)
{
	struct rtentry *rt = xrt;

	rtref(rt);
}

void
rtentry_unref(void *null, void *xrt)
{
	struct rtentry *rt = xrt;

	rtfree(rt);
}

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
#endif /* DIAGNOSTIC */

	return (plen);
}
#endif /* ART */
