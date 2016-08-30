/*	$OpenBSD: rtable.c,v 1.51 2016/08/30 07:42:57 jmatthew Exp $ */

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

void		   rtable_init_backend(unsigned int);
void		  *rtable_alloc(unsigned int, unsigned int, unsigned int);
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
	unsigned int	 off, alen;
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
		alen = dp->dom_maxplen;

		if (id >= rtmap_limit)
			rtmap_grow(id + 1, af);

		tbl = rtable_alloc(id, alen, off);
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
	struct srp_ref	 sr;

	if (af >= nitems(af2idx) || af2idx[af] == 0)
		return (NULL);

	map = srp_enter(&sr, &afmap[af2idx[af]]);
	if (rtableid < map->limit)
		tbl = map->tbl[rtableid];
	srp_leave(&sr);

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
	struct srp_ref	 sr;

	dmm = srp_enter(&sr, &afmap[0]);
	if (rtableid < dmm->limit)
		rdomain = dmm->dom[rtableid];
	srp_leave(&sr);

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
rtable_alloc(unsigned int rtableid, unsigned int alen, unsigned int off)
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
	struct rtentry		*rt = NULL;
#ifndef SMALL_KERNEL
	int			 hash;
#endif /* SMALL_KERNEL */

	rnh = rtable_get(rtableid, dst->sa_family);
	if (rnh == NULL)
		return (NULL);

	KERNEL_LOCK();
	rn = rn_match(dst, rnh);
	if (rn == NULL || (rn->rn_flags & RNF_ROOT) != 0)
		goto out;

	rt = ((struct rtentry *)rn);
	rtref(rt);

#ifndef SMALL_KERNEL
	/* Gateway selection by Hash-Threshold (RFC 2992) */
	if ((hash = rt_hash(rt, dst, src)) != -1) {
		struct rtentry		*mrt = rt;
		int			 threshold, npaths = 1;

		KASSERT(hash <= 0xffff);

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
	}
#endif /* SMALL_KERNEL */
out:
	KERNEL_UNLOCK();
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
	int error;

	rnh = rtable_get(rtableid, af);
	if (rnh == NULL)
		return (EAFNOSUPPORT);

	while ((error = rn_walktree(rnh, f, arg)) == EAGAIN)
		continue;

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
	return (mpath);
}

int
rtable_mpath_reprio(unsigned int rtableid, struct sockaddr *dst,
    struct sockaddr *mask, uint8_t prio, struct rtentry *rt)
{
	struct radix_node	*rn = (struct radix_node *)rt;

	rn_mpath_reprio(rn, prio);

	return (0);
}

struct rtentry *
rtable_mpath_next(struct rtentry *rt)
{
	struct radix_node *rn = (struct radix_node *)rt;

	return ((struct rtentry *)rn_mpath_next(rn, RMP_MODE_ACTIVE));
}
#endif /* SMALL_KERNEL */

#else /* ART */

static inline uint8_t	*satoaddr(struct art_root *, struct sockaddr *);

void	rtentry_ref(void *, void *);
void	rtentry_unref(void *, void *);

#ifndef SMALL_KERNEL
void	rtable_mpath_insert(struct art_node *, struct rtentry *);
#endif

struct srpl_rc rt_rc = SRPL_RC_INITIALIZER(rtentry_ref, rtentry_unref, NULL);

void
rtable_init_backend(unsigned int keylen)
{
	art_init();
}

void *
rtable_alloc(unsigned int rtableid, unsigned int alen, unsigned int off)
{
	return (art_alloc(rtableid, alen, off));
}

struct rtentry *
rtable_lookup(unsigned int rtableid, struct sockaddr *dst,
    struct sockaddr *mask, struct sockaddr *gateway, uint8_t prio)
{
	struct art_root			*ar;
	struct art_node			*an;
	struct rtentry			*rt = NULL;
	struct srp_ref			 sr, nsr;
	uint8_t				*addr;
	int				 plen;

	ar = rtable_get(rtableid, dst->sa_family);
	if (ar == NULL)
		return (NULL);

	addr = satoaddr(ar, dst);

	/* No need for a perfect match. */
	if (mask == NULL) {
		an = art_match(ar, addr, &nsr);
		if (an == NULL)
			goto out;
	} else {
		plen = rtable_satoplen(dst->sa_family, mask);
		if (plen == -1)
			return (NULL);

		an = art_lookup(ar, addr, plen, &nsr);

		/* Make sure we've got a perfect match. */
		if (an == NULL || an->an_plen != plen ||
		    memcmp(an->an_dst, dst, dst->sa_len))
			goto out;
	}

#ifdef SMALL_KERNEL
	rt = SRPL_ENTER(&sr, &an->an_rtlist);
#else
	SRPL_FOREACH(rt, &sr, &an->an_rtlist, rt_next) {
		if (prio != RTP_ANY &&
		    (rt->rt_priority & RTP_MASK) != (prio & RTP_MASK))
			continue;

		if (gateway == NULL)
			break;

		if (rt->rt_gateway->sa_len == gateway->sa_len &&
		    memcmp(rt->rt_gateway, gateway, gateway->sa_len) == 0)
			break;
	}
#endif /* SMALL_KERNEL */
	if (rt != NULL)
		rtref(rt);

	SRPL_LEAVE(&sr);
out:
	srp_leave(&nsr);

	return (rt);
}

struct rtentry *
rtable_match(unsigned int rtableid, struct sockaddr *dst, uint32_t *src)
{
	struct art_root			*ar;
	struct art_node			*an;
	struct rtentry			*rt = NULL;
	struct srp_ref			 sr, nsr;
	uint8_t				*addr;
#ifndef SMALL_KERNEL
	int				 hash;
#endif /* SMALL_KERNEL */

	ar = rtable_get(rtableid, dst->sa_family);
	if (ar == NULL)
		return (NULL);

	addr = satoaddr(ar, dst);

	an = art_match(ar, addr, &nsr);
	if (an == NULL)
		goto out;

	rt = SRPL_ENTER(&sr, &an->an_rtlist);
	rtref(rt);
	SRPL_LEAVE(&sr);

#ifndef SMALL_KERNEL
	/* Gateway selection by Hash-Threshold (RFC 2992) */
	if ((hash = rt_hash(rt, dst, src)) != -1) {
		struct rtentry		*mrt;
		int			 threshold, npaths = 0;

		KASSERT(hash <= 0xffff);

		SRPL_FOREACH(mrt, &sr, &an->an_rtlist, rt_next) {
			/* Only count nexthops with the same priority. */
			if (mrt->rt_priority == rt->rt_priority)
				npaths++;
		}
		SRPL_LEAVE(&sr);

		threshold = (0xffff / npaths) + 1;

		/*
		 * we have no protection against concurrent modification of the
		 * route list attached to the node, so we won't necessarily
		 * have the same number of routes.  for most modifications,
		 * we'll pick a route that we wouldn't have if we only saw the
		 * list before or after the change.  if we were going to use
		 * the last available route, but it got removed, we'll hit
		 * the end of the list and then pick the first route.
		 */

		mrt = SRPL_ENTER(&sr, &an->an_rtlist);
		while (hash > threshold && mrt != NULL) {
			if (mrt->rt_priority == rt->rt_priority)
				hash -= threshold;
			mrt = SRPL_NEXT(&sr, mrt, rt_next);
		}

		if (mrt != NULL) {
			rtref(mrt);
			rtfree(rt);
			rt = mrt;
		}
		SRPL_LEAVE(&sr);
	}
#endif /* SMALL_KERNEL */
out:
	srp_leave(&nsr);
	return (rt);
}

int
rtable_insert(unsigned int rtableid, struct sockaddr *dst,
    struct sockaddr *mask, struct sockaddr *gateway, uint8_t prio,
    struct rtentry *rt)
{
#ifndef SMALL_KERNEL
	struct rtentry			*mrt;
	struct srp_ref			 sr;
#endif /* SMALL_KERNEL */
	struct art_root			*ar;
	struct art_node			*an, *prev;
	uint8_t				*addr;
	int				 plen;
	unsigned int			 rt_flags;
	int				 error = 0;

	ar = rtable_get(rtableid, dst->sa_family);
	if (ar == NULL)
		return (EAFNOSUPPORT);

	addr = satoaddr(ar, dst);
	plen = rtable_satoplen(dst->sa_family, mask);
	if (plen == -1)
		return (EINVAL);

	rtref(rt); /* guarantee rtfree won't do anything during insert */
	rw_enter_write(&ar->ar_lock);

#ifndef SMALL_KERNEL
	/* Do not permit exactly the same dst/mask/gw pair. */
	an = art_lookup(ar, addr, plen, &sr);
	srp_leave(&sr); /* an can't go away while we have the lock */
	if (an != NULL && an->an_plen == plen &&
	    !memcmp(an->an_dst, dst, dst->sa_len)) {
		struct rtentry  *mrt;
		int		 mpathok = ISSET(rt->rt_flags, RTF_MPATH);

		SRPL_FOREACH_LOCKED(mrt, &an->an_rtlist, rt_next) {
			if (prio != RTP_ANY &&
			    (mrt->rt_priority & RTP_MASK) != (prio & RTP_MASK))
				continue;

			if (!mpathok ||
			    (mrt->rt_gateway->sa_len == gateway->sa_len &&
			    !memcmp(mrt->rt_gateway, gateway, gateway->sa_len))){
				error = EEXIST;
				goto leave;
			}
		}
	}
#endif /* SMALL_KERNEL */

	an = art_get(dst, plen);
	if (an == NULL) {
		error = ENOBUFS;
		goto leave;
	}

	/* prepare for immediate operation if insert succeeds */
	rt_flags = rt->rt_flags;
	rt->rt_flags &= ~RTF_MPATH;
	rt->rt_dest = dst;
	rt->rt_plen = plen;
	SRPL_INSERT_HEAD_LOCKED(&rt_rc, &an->an_rtlist, rt, rt_next);

	prev = art_insert(ar, an, addr, plen);
	if (prev != an) {
		SRPL_REMOVE_LOCKED(&rt_rc, &an->an_rtlist, rt, rtentry,
		    rt_next);
		rt->rt_flags = rt_flags;
		art_put(an);

		if (prev == NULL) {
			error = ESRCH;
			goto leave;
		}

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

		/* Put newly inserted entry at the right place. */
		rtable_mpath_insert(an, rt);
#else
		error = EEXIST;
#endif /* SMALL_KERNEL */
	}
leave:
	rw_exit_write(&ar->ar_lock);
	rtfree(rt);
	return (error);
}

int
rtable_delete(unsigned int rtableid, struct sockaddr *dst,
    struct sockaddr *mask, struct rtentry *rt)
{
	struct art_root			*ar;
	struct art_node			*an;
	struct srp_ref			 sr;
	uint8_t				*addr;
	int				 plen;
#ifndef SMALL_KERNEL
	struct rtentry			*mrt;
	int				 npaths = 0;
#endif /* SMALL_KERNEL */
	int				 error = 0;

	ar = rtable_get(rtableid, dst->sa_family);
	if (ar == NULL)
		return (EAFNOSUPPORT);

	addr = satoaddr(ar, dst);
	plen = rtable_satoplen(dst->sa_family, mask);

	rtref(rt); /* guarantee rtfree won't do anything under ar_lock */
	rw_enter_write(&ar->ar_lock);
	an = art_lookup(ar, addr, plen, &sr);
	srp_leave(&sr); /* an can't go away while we have the lock */

	/* Make sure we've got a perfect match. */
	if (an == NULL || an->an_plen != plen ||
	    memcmp(an->an_dst, dst, dst->sa_len)) {
		error = ESRCH;
		goto leave;
	}

#ifndef SMALL_KERNEL
	/*
	 * If other multipath route entries are still attached to
	 * this ART node we only have to unlink it.
	 */
	SRPL_FOREACH_LOCKED(mrt, &an->an_rtlist, rt_next)
		npaths++;

	if (npaths > 1) {
		KASSERT(rt->rt_refcnt >= 1);
		SRPL_REMOVE_LOCKED(&rt_rc, &an->an_rtlist, rt, rtentry,
		    rt_next);

		mrt = SRPL_FIRST_LOCKED(&an->an_rtlist);
		an->an_dst = mrt->rt_dest;
		if (npaths == 2)
			mrt->rt_flags &= ~RTF_MPATH;

		goto leave;
	}
#endif /* SMALL_KERNEL */

	if (art_delete(ar, an, addr, plen) == NULL)
		panic("art_delete failed to find node %p", an);

	KASSERT(rt->rt_refcnt >= 1);
	SRPL_REMOVE_LOCKED(&rt_rc, &an->an_rtlist, rt, rtentry, rt_next);
	art_put(an);

leave:
	rw_exit_write(&ar->ar_lock);
	rtfree(rt);

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
	struct srp_ref			 sr;
	struct rtable_walk_cookie	*rwc = xrwc;
	struct rtentry			*rt;
	int				 error = 0;

	SRPL_FOREACH(rt, &sr, &an->an_rtlist, rt_next) {
		if ((error = (*rwc->rwc_func)(rt, rwc->rwc_arg, rwc->rwc_rid)))
			break;
	}
	SRPL_LEAVE(&sr);

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

	while ((error = art_walk(ar, rtable_walk_helper, &rwc)) == EAGAIN)
		continue;

	return (error);
}

#ifndef SMALL_KERNEL
int
rtable_mpath_capable(unsigned int rtableid, sa_family_t af)
{
	return (1);
}

int
rtable_mpath_reprio(unsigned int rtableid, struct sockaddr *dst,
    struct sockaddr *mask, uint8_t prio, struct rtentry *rt)
{
	struct art_root			*ar;
	struct art_node			*an;
	struct srp_ref			 sr;
	uint8_t				*addr;
	int				 plen;
	int				 error = 0;

	ar = rtable_get(rtableid, dst->sa_family);
	if (ar == NULL)
		return (EAFNOSUPPORT);

	addr = satoaddr(ar, dst);
	plen = rtable_satoplen(dst->sa_family, mask);

	rw_enter_write(&ar->ar_lock);
	an = art_lookup(ar, addr, plen, &sr);
	srp_leave(&sr); /* an can't go away while we have the lock */

	/* Make sure we've got a perfect match. */
	if (an == NULL || an->an_plen != plen ||
	    memcmp(an->an_dst, dst, dst->sa_len))
		error = ESRCH;
	else {
		rtref(rt); /* keep rt alive in between remove and insert */
		SRPL_REMOVE_LOCKED(&rt_rc, &an->an_rtlist,
		    rt, rtentry, rt_next);
		rt->rt_priority = prio;
		rtable_mpath_insert(an, rt);
		rtfree(rt);
	}
	rw_exit_write(&ar->ar_lock);

	return (error);
}

void
rtable_mpath_insert(struct art_node *an, struct rtentry *rt)
{
	struct rtentry			*mrt, *prt = NULL;
	uint8_t				 prio = rt->rt_priority;

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
#endif /* ART */

/*
 * Return the prefix length of a mask.
 */
int
rtable_satoplen(sa_family_t af, struct sockaddr *mask)
{
	struct domain	*dp;
	uint8_t		*ap, *ep;
	int		 mlen, plen = 0;
	int		 i;

	for (i = 0; (dp = domains[i]) != NULL; i++) {
		if (dp->dom_rtoffset == 0)
			continue;

		if (af == dp->dom_family)
			break;
	}
	if (dp == NULL)
		return (-1);

	/* Host route */
	if (mask == NULL)
		return (dp->dom_maxplen);

	mlen = mask->sa_len;

	/* Default route */
	if (mlen == 0)
		return (0);

	ap = (uint8_t *)((uint8_t *)mask) + dp->dom_rtoffset;
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
