/*	$OpenBSD: rtable.c,v 1.96 2026/11/04 08:51:52 claudio Exp $ */

/*
 * Copyright (c) 2014-2016 Martin Pieuchot
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
#include <sys/smr.h>
#include <sys/srp.h>
#include <sys/smr.h>
#endif

#include <net/rtable.h>
#include <net/route.h>
#include <net/art.h>

/*
 * Maximum number of alternate routing tables
 */
int rt_tableid_max = 255;

struct rtable_map *rtables;		/* [S] write side: rtable_lock */
size_t		  rtable_size;		/* [I] size of rtable entry */
struct rwlock	  rtable_lock = RWLOCK_INITIALIZER("rtable");

uint8_t		  af2idx[AF_MAX+1];	/* [I] to only allocate supported AF */
uint8_t		  af2idx_max;		/* [I] number of tables needed */

void		  rtable_grow(unsigned int);
struct rtable	 *rtable_entry(unsigned int);
struct rtidx	 *rtable_get(unsigned int, sa_family_t);

/*
 * One past the highest rtableid that can currently exist.  Monotonic,
 * so a caller walking every table may read it once for the whole walk.
 */
unsigned int
rtable_limit(void)
{
	struct rtable_map	*map;
	unsigned int		 limit;

	smr_read_enter();
	map = SMR_PTR_GET(&rtables);
	limit = (map == NULL) ? 0 : map->m_limit;
	smr_read_leave();

	return (limit);
}

/*
 * Grow the array of routing tables to at least ``nlimit'' entries.
 */
void
rtable_grow(unsigned int nlimit)
{
	struct rtable_map	*omap, *nmap;
	unsigned int		 olimit;

	rw_assert_wrlock(&rtable_lock);

	nlimit = roundup(nlimit, 16);

	omap = SMR_PTR_GET_LOCKED(&rtables);
	olimit = (omap == NULL) ? 0 : omap->m_limit;

	KASSERT(nlimit > olimit);

	nmap = malloc(RTABLE_MAP_SIZE(nlimit), M_RTABLE, M_WAITOK|M_ZERO);
	nmap->m_limit = nlimit;
	if (omap != NULL)
		memcpy(nmap->m_tbl, omap->m_tbl,
		    olimit * sizeof(omap->m_tbl[0]));

	SMR_PTR_SET_LOCKED(&rtables, nmap);

	if (omap != NULL) {
		smr_barrier();
		free(omap, M_RTABLE, RTABLE_MAP_SIZE(olimit));
	}
}

void
rtable_init(void)
{
	const struct domain	*dp;
	int			 i;

	/*
	 * Compute the maximum supported key length in case the routing
	 * table backend needs it.
	 */
	for (i = 0; (dp = domains[i]) != NULL; i++) {
		if (dp->dom_rtoffset == 0)
			continue;

		af2idx[dp->dom_family] = ++af2idx_max;
	}

	rtable_size = sizeof(struct rtable) +
	    af2idx_max * sizeof(struct rtidx);

	art_boot();

	if (rtable_add(0) != 0)
		panic("unable to create default routing table");

	rt_timer_init();
}

int
rtable_add(unsigned int id)
{
	struct rtable		*rt = NULL;
	struct rtable_map	*map;
	const struct domain	*dp;
	sa_family_t		 af;
	unsigned int		 off, alen;
	int			 i, error = 0;

	if (id >= USHRT_MAX)
		return (EINVAL);
	if (id > rt_tableid_max)
		return (EINVAL);

	rw_enter_write(&rtable_lock);

	if (rtable_exists(id))
		goto out;

	rt = malloc(rtable_size, M_RTABLE, M_WAITOK|M_ZERO);

	for (i = 0; (dp = domains[i]) != NULL; i++) {
		struct rtidx *ri;
		if (dp->dom_rtoffset == 0)
			continue;

		af = dp->dom_family;
		off = dp->dom_rtoffset;
		alen = dp->dom_maxplen;

		ri = &rt->rt_idx[af2idx[af] - 1];

		ri->r_art = art_alloc(alen);
		if (ri->r_art == NULL) {
			error = ENOMEM;
			goto out;
		}
		rw_init(&ri->r_lock, "rtable");
		ri->r_off = off;
	}

	/* Reflect possible growth. */
	map = SMR_PTR_GET_LOCKED(&rtables);
	if (map == NULL || id >= map->m_limit) {
		rtable_grow(id + 1);
		map = SMR_PTR_GET_LOCKED(&rtables);
	}

	/* Use primary rdomain by default. */
	rt->rt_rdomain = 0;

	SMR_PTR_SET_LOCKED(&map->m_tbl[id], rt);
	rt = NULL;
out:
	rw_exit_write(&rtable_lock);
	if (rt != NULL) {
		for (i = 0; i < af2idx_max; i++) {
			struct rtidx *ri;

			ri = &rt->rt_idx[i];
			art_free(ri->r_art);
		}
		free(rt, M_RTABLE, rtable_size);
	}
	return (error);
}

struct rtable *
rtable_entry(unsigned int rtableid)
{
	struct rtable_map *map;

	SMR_ASSERT_CRITICAL();

	map = SMR_PTR_GET(&rtables);
	if (map == NULL || rtableid >= map->m_limit)
		return (NULL);

	return (SMR_PTR_GET(&map->m_tbl[rtableid]));
}

/*
 * rtables are immortal so it is save to return the rtidx pointer
 * outside of the smr read critical section.
 */
struct rtidx *
rtable_get(unsigned int rtableid, sa_family_t af)
{
	struct rtable	*rt;
	struct rtidx	*ri = NULL;

	if (af >= nitems(af2idx) || af2idx[af] == 0)
		return (NULL);
	smr_read_enter();
	if ((rt = rtable_entry(rtableid)) != NULL)
		ri = &rt->rt_idx[af2idx[af] - 1];
	smr_read_leave();
	return (ri);
}

int
rtable_exists(unsigned int rtableid)
{
	int rv;

	smr_read_enter();
	rv = (rtable_entry(rtableid) != NULL);
	smr_read_leave();

	return (rv);
}

int
rtable_empty(unsigned int rtableid)
{
	struct rtable	*rt;
	struct rtidx	*ri;
	int		 i, rv = 0;

	smr_read_enter();
	if ((rt = rtable_entry(rtableid)) != NULL) {
		for (i = 0; i < af2idx_max; i++) {
			ri = &rt->rt_idx[i];
			if (!art_is_empty(ri->r_art))
				break;
		}
		if (i == af2idx_max)
			rv = 1;
	}
	smr_read_leave();

	return (rv);
}

unsigned int
rtable_l2(unsigned int rtableid)
{
	struct rtable *rt;
	unsigned int rdomain = 0;

	smr_read_enter();
	if ((rt = rtable_entry(rtableid)) != NULL)
		rdomain = rt->rt_rdomain;
	smr_read_leave();
	return (rdomain);
}

unsigned int
rtable_loindex(unsigned int rtableid)
{
	struct rtable *rt;
	unsigned int loifidx = 0;

	smr_read_enter();
	if ((rt = rtable_entry(rtableid)) != NULL)
		loifidx = rt->rt_loifidx;
	smr_read_leave();
	return (loifidx);
}

struct ip_mrouter *
rtable_get_mrouter(unsigned int rtableid)
{
	struct rtable *rt;

	SMR_ASSERT_CRITICAL();

	if ((rt = rtable_entry(rtableid)) == NULL)
		return (NULL);
	return (SMR_PTR_GET(&rt->rt_mrouter));
}

struct ip6_mrouter *
rtable_get_mrouter6(unsigned int rtableid)
{
	struct rtable *rt;

	SMR_ASSERT_CRITICAL();

	if ((rt = rtable_entry(rtableid)) == NULL)
		return (NULL);
	return (SMR_PTR_GET(&rt->rt_mrouter6));
}

struct ip_mrouter *
rtable_set_mrouter(unsigned int rtableid, struct ip_mrouter *new)
{
	struct ip_mrouter *old;
	struct rtable *rt;

	smr_read_enter();
	rt = rtable_entry(rtableid);
	smr_read_leave();
	if (rt == NULL)
		return (NULL);

	old = SMR_PTR_GET_LOCKED(&rt->rt_mrouter);
	SMR_PTR_SET_LOCKED(&rt->rt_mrouter, new);

	return (old);
}

struct ip6_mrouter *
rtable_set_mrouter6(unsigned int rtableid, struct ip6_mrouter *new)
{
	struct ip6_mrouter *old;
	struct rtable *rt;

	smr_read_enter();
	rt = rtable_entry(rtableid);
	smr_read_leave();
	if (rt == NULL)
		return (NULL);

	old = SMR_PTR_GET_LOCKED(&rt->rt_mrouter6);
	SMR_PTR_SET_LOCKED(&rt->rt_mrouter6, new);

	return (old);
}

void
rtable_l2set(unsigned int rtableid, unsigned int rdomain, unsigned int loifidx)
{
	struct rtable *rt;

	KERNEL_ASSERT_LOCKED();

	if (!rtable_exists(rdomain))
		panic("rdomain %d does not exist", rdomain);
	if (rtableid != rdomain && rtable_l2(rdomain) != rdomain)
		panic("routing table %d isn't a rdomain", rdomain);
	if (!rtable_empty(rtableid))
		panic("rtable %d is not empty", rtableid);

	smr_read_enter();
	rt = rtable_entry(rtableid);
	smr_read_leave();
	if (rt == NULL)
		return;

	rt->rt_rdomain = rdomain;
	rt->rt_loifidx = loifidx;
}


static inline const uint8_t *satoaddr(struct rtidx *,
    const struct sockaddr *);

void	rtable_mpath_insert(struct art_node *, struct rtentry *);

int
rtable_setsource(unsigned int rtableid, int af, struct sockaddr *src)
{
	struct rtidx *ri;

	NET_ASSERT_LOCKED_EXCLUSIVE();

	ri = rtable_get(rtableid, af);
	if (ri == NULL)
		return (EAFNOSUPPORT);

	ri->r_source = src;

	return (0);
}

struct sockaddr *
rtable_getsource(unsigned int rtableid, int af)
{
	struct rtidx *ri;

	NET_ASSERT_LOCKED();

	ri = rtable_get(rtableid, af);
	if (ri == NULL)
		return (NULL);

	return (ri->r_source);
}

void
rtable_clearsource(unsigned int rtableid, struct sockaddr *src)
{
	struct sockaddr	*addr;

	addr = rtable_getsource(rtableid, src->sa_family);
	if (addr && (addr->sa_len == src->sa_len)) {
		if (memcmp(src, addr, addr->sa_len) == 0) {
			rtable_setsource(rtableid, src->sa_family, NULL);
		}
	}
}

struct rtentry *
rtable_lookup(unsigned int rtableid, const struct sockaddr *dst,
    const struct sockaddr *mask, const struct sockaddr *gateway, uint8_t prio)
{
	struct rtidx			*ri;
	struct art_node			*an;
	struct rtentry			*rt = NULL;
	const uint8_t			*addr;
	int				 plen;

	ri = rtable_get(rtableid, dst->sa_family);
	if (ri == NULL)
		return (NULL);

	addr = satoaddr(ri, dst);

	smr_read_enter();
	if (mask == NULL) {
		/* No need for a perfect match. */
		an = art_match(ri->r_art, addr);
	} else {
		plen = rtable_satoplen(dst->sa_family, mask);
		if (plen == -1)
			goto out;

		an = art_lookup(ri->r_art, addr, plen);
	}
	if (an == NULL)
		goto out;

	for (rt = SMR_PTR_GET(&an->an_value); rt != NULL;
	    rt = SMR_PTR_GET(&rt->rt_next)) {
		if (prio != RTP_ANY &&
		    (rt->rt_priority & RTP_MASK) != (prio & RTP_MASK))
			continue;

		if (gateway == NULL)
			break;

		if (rt->rt_gateway->sa_len == gateway->sa_len &&
		    memcmp(rt->rt_gateway, gateway, gateway->sa_len) == 0)
			break;
	}
	if (rt != NULL)
		rtref(rt);

out:
	smr_read_leave();

	return (rt);
}

struct rtentry *
rtable_match(unsigned int rtableid, const struct sockaddr *dst, uint32_t *src)
{
	struct rtidx			*ri;
	struct art_node			*an;
	struct rtentry			*rt = NULL;
	const uint8_t			*addr;
	int				 hash;
	uint8_t				 prio;

	ri = rtable_get(rtableid, dst->sa_family);
	if (ri == NULL)
		return (NULL);

	addr = satoaddr(ri, dst);

	smr_read_enter();
	an = art_match(ri->r_art, addr);
	if (an == NULL)
		goto out;

	rt = SMR_PTR_GET(&an->an_value);
	KASSERT(rt != NULL);
	prio = rt->rt_priority;

	/* Gateway selection by Hash-Threshold (RFC 2992) */
	if ((hash = rt_hash(rt, dst, src)) != -1) {
		struct rtentry		*mrt;
		int			 threshold, npaths = 1;

		KASSERT(hash <= 0xffff);

		/* Only count nexthops with the same priority. */
		mrt = rt;
		while ((mrt = SMR_PTR_GET(&mrt->rt_next)) != NULL) {
			if (mrt->rt_priority == prio)
				npaths++;
		}

		threshold = (0xffff / npaths) + 1;

		/*
		 * we have no protection against concurrent modification of the
		 * route list attached to the node, so we won't necessarily
		 * have the same number of routes.  for most modifications,
		 * we'll pick a route that we wouldn't have if we only saw the
		 * list before or after the change.
		 */
		mrt = rt;
		while (hash > threshold) {
			if (mrt->rt_priority == prio) {
				rt = mrt;
				hash -= threshold;
			}
			mrt = SMR_PTR_GET(&mrt->rt_next);
			if (mrt == NULL)
				break;
		}
	}
	rtref(rt);
out:
	smr_read_leave();
	return (rt);
}

int
rtable_insert(unsigned int rtableid, struct sockaddr *dst,
    const struct sockaddr *mask, const struct sockaddr *gateway, uint8_t prio,
    struct rtentry *rt)
{
	struct rtidx			*ri;
	struct art_node			*an, *prev;
	const uint8_t			*addr;
	int				 plen;
	unsigned int			 rt_flags;
	int				 error = 0;

	ri = rtable_get(rtableid, dst->sa_family);
	if (ri == NULL)
		return (EAFNOSUPPORT);

	addr = satoaddr(ri, dst);
	plen = rtable_satoplen(dst->sa_family, mask);
	if (plen == -1)
		return (EINVAL);

	an = art_get(addr, plen);
	if (an == NULL)
		return (ENOMEM);

	/* prepare for immediate operation if insert succeeds */
	rt_flags = rt->rt_flags;
	rt->rt_flags &= ~RTF_MPATH;
	rt->rt_dest = dst;
	rt->rt_plen = plen;
	rt->rt_next = NULL;

	rtref(rt); /* take a ref for the table */
	an->an_value = rt;

	rw_enter_write(&ri->r_lock);
	prev = art_insert(ri->r_art, an);
	if (prev == NULL) {
		error = ENOMEM;
		goto put;
	}

	if (prev != an) {
		struct rtentry *mrt;
		int mpathok = ISSET(rt_flags, RTF_MPATH);
		int mpath = 0;

		/*
		 * An ART node with the same destination/netmask already
		 * exists.
		 */
		art_put(an);
		an = prev;

		/* Do not permit exactly the same dst/mask/gw pair. */
		for (mrt = SMR_PTR_GET_LOCKED(&an->an_value);
		     mrt != NULL;
		     mrt = SMR_PTR_GET_LOCKED(&mrt->rt_next)) {
			if (prio != RTP_ANY &&
			    (mrt->rt_priority & RTP_MASK) != (prio & RTP_MASK))
				continue;

			if (!mpathok ||
			    (mrt->rt_gateway->sa_len == gateway->sa_len &&
			    memcmp(mrt->rt_gateway, gateway,
			    gateway->sa_len) == 0)) {
				error = EEXIST;
				goto leave;
			}
			mpath = RTF_MPATH;
		}

		/* The new route can be added to the list. */
		if (mpath) {
			SET(rt->rt_flags, RTF_MPATH);

			for (mrt = SMR_PTR_GET_LOCKED(&an->an_value);
			     mrt != NULL;
			     mrt = SMR_PTR_GET_LOCKED(&mrt->rt_next)) {
				if ((mrt->rt_priority & RTP_MASK) !=
				    (prio & RTP_MASK))
					continue;

				SET(mrt->rt_flags, RTF_MPATH);
			}
		}

		/* Put newly inserted entry at the right place. */
		rtable_mpath_insert(an, rt);
	}
	rw_exit_write(&ri->r_lock);
	return (error);

put:
	art_put(an);
leave:
	rw_exit_write(&ri->r_lock);
	rtfree(rt);
	return (error);
}

int
rtable_delete(unsigned int rtableid, const struct sockaddr *dst,
    const struct sockaddr *mask, struct rtentry *rt)
{
	struct rtidx			*ri;
	struct art_node			*an;
	const uint8_t			*addr;
	int				 plen;
	struct rtentry			*mrt;

	ri = rtable_get(rtableid, dst->sa_family);
	if (ri == NULL)
		return (EAFNOSUPPORT);

	addr = satoaddr(ri, dst);
	plen = rtable_satoplen(dst->sa_family, mask);
	if (plen == -1)
		return (EINVAL);

	rw_enter_write(&ri->r_lock);
	smr_read_enter();
	an = art_lookup(ri->r_art, addr, plen);
	smr_read_leave();
	if (an == NULL) {
		rw_exit_write(&ri->r_lock);
		return (ESRCH);
	}

	/* If this is the only route in the list then we can delete the node */
	if (SMR_PTR_GET_LOCKED(&an->an_value) == rt &&
	    SMR_PTR_GET_LOCKED(&rt->rt_next) == NULL) {
		struct art_node *oan;
		oan = art_delete(ri->r_art, addr, plen);
		if (oan != an)
			panic("art %p changed shape during delete", ri->r_art);
		art_put(an);
		/*
		 * XXX an and the rt ref could still be alive on other cpus.
		 * this currently works because of the NET_LOCK/KERNEL_LOCK
		 * but should be fixed if we want to do route lookups outside
		 * these locks. - dlg@
		 */
	} else {
		struct rtentry **prt;
		struct rtentry *nrt;
		unsigned int found = 0;
		unsigned int npaths = 0;

		/*
		 * If other multipath route entries are still attached to
		 * this ART node we only have to unlink it.
		 */
 		prt = (struct rtentry **)&an->an_value;
		while ((mrt = SMR_PTR_GET_LOCKED(prt)) != NULL) {
			if (mrt == rt) {
				found = 1;
				SMR_PTR_SET_LOCKED(prt,
				    SMR_PTR_GET_LOCKED(&mrt->rt_next));
			} else if ((mrt->rt_priority & RTP_MASK) ==
			    (rt->rt_priority & RTP_MASK)) {
				npaths++;
				nrt = mrt;
			}
			prt = &mrt->rt_next;
		}
		if (!found)
			panic("removing non-existent route");
		if (npaths == 1)
			CLR(nrt->rt_flags, RTF_MPATH);
	}
	KASSERT(refcnt_read(&rt->rt_refcnt) >= 1);
	rw_exit_write(&ri->r_lock);
	rtfree(rt);

	return (0);
}

int
rtable_walk(unsigned int rtableid, sa_family_t af, struct rtentry **prt,
    int (*func)(struct rtentry *, void *, unsigned int), void *arg)
{
	struct rtidx			*ri;
	struct art_iter			 ai;
	struct art_node			*an;
	int				 error = 0;

	ri = rtable_get(rtableid, af);
	if (ri == NULL)
		return (EAFNOSUPPORT);

	rw_enter_write(&ri->r_lock);
	ART_FOREACH(an, ri->r_art, &ai) {
		/*
		 * ART nodes have a list of rtentries.
		 *
		 * art_iter holds references to the topology
		 * so it won't change, but not the an_node or rtentries.
		 */
		struct rtentry *rt = SMR_PTR_GET_LOCKED(&an->an_value);
		rtref(rt);

		rw_exit_write(&ri->r_lock);
		do {
			struct rtentry *nrt;

			smr_read_enter();
			/* Get ready for the next entry. */
			nrt = SMR_PTR_GET(&rt->rt_next);
			if (nrt != NULL)
				rtref(nrt);
			smr_read_leave();

			error = func(rt, arg, rtableid);
			if (error != 0) {
				if (prt != NULL)
					*prt = rt;
				else
					rtfree(rt);

				if (nrt != NULL)
					rtfree(nrt);

				rw_enter_write(&ri->r_lock);
				art_iter_close(&ai);
				rw_exit_write(&ri->r_lock);
				return (error);
			}

			rtfree(rt);
			rt = nrt;
		} while (rt != NULL);
		rw_enter_write(&ri->r_lock);
	}
	rw_exit_write(&ri->r_lock);

	return (error);
}

int
rtable_read(unsigned int rtableid, sa_family_t af,
    int (*func)(const struct rtentry *, void *, unsigned int), void *arg)
{
	struct rtidx			*ri;
	struct art_iter			 ai;
	struct art_node			*an;
	int				 error = 0;

	ri = rtable_get(rtableid, af);
	if (ri == NULL)
		return (EAFNOSUPPORT);

	rw_enter_write(&ri->r_lock);
	ART_FOREACH(an, ri->r_art, &ai) {
		struct rtentry *rt;
		for (rt = SMR_PTR_GET_LOCKED(&an->an_value); rt != NULL;
		    rt = SMR_PTR_GET_LOCKED(&rt->rt_next)) {
			error = func(rt, arg, rtableid);
			if (error != 0) {
				art_iter_close(&ai);
				goto leave;
			}
		}
	}
leave:
	rw_exit_write(&ri->r_lock);

	return (error);
}

struct rtentry *
rtable_iterate(struct rtentry *rt0)
{
	struct rtentry *rt = NULL;

	smr_read_enter();
	rt = SMR_PTR_GET(&rt0->rt_next);
	if (rt != NULL)
		rtref(rt);
	smr_read_leave();
	rtfree(rt0);
	return (rt);
}

int
rtable_mpath_capable(unsigned int rtableid, sa_family_t af)
{
	if (af == AF_MPLS)
		return (0);
	return (1);
}

int
rtable_mpath_reprio(unsigned int rtableid, struct sockaddr *dst,
    int plen, uint8_t prio, struct rtentry *rt)
{
	struct rtidx			*ri;
	struct art_node			*an;
	const uint8_t			*addr;
	int				 error = 0;

	ri = rtable_get(rtableid, dst->sa_family);
	if (ri == NULL)
		return (EAFNOSUPPORT);

	addr = satoaddr(ri, dst);

	rw_enter_write(&ri->r_lock);
	smr_read_enter();
	an = art_lookup(ri->r_art, addr, plen);
	smr_read_leave();
	if (an == NULL) {
		error = ESRCH;
	} else if (SMR_PTR_GET_LOCKED(&an->an_value) == rt &&
	    SMR_PTR_GET_LOCKED(&rt->rt_next) == NULL) {
		/*
		 * If there's only one entry on the list do not go
		 * through an insert/remove cycle.  This is done to
		 * guarantee that ``an->an_rtlist''  is never empty
		 * when a node is in the tree.
		 */
		rt->rt_priority = prio;
	} else {
		struct rtentry **prt;
		struct rtentry *mrt;

 		prt = (struct rtentry **)&an->an_value;
		while ((mrt = SMR_PTR_GET_LOCKED(prt)) != NULL) {
			if (mrt == rt)
				break;
			prt = &mrt->rt_next;
		}
		KASSERT(mrt != NULL);

		SMR_PTR_SET_LOCKED(prt, SMR_PTR_GET_LOCKED(&rt->rt_next));
		rt->rt_priority = prio;
		rtable_mpath_insert(an, rt);
		error = EAGAIN;
	}
	rw_exit_write(&ri->r_lock);

	return (error);
}

void
rtable_mpath_insert(struct art_node *an, struct rtentry *rt)
{
	struct rtentry			*mrt, **prt;
	uint8_t				 prio = rt->rt_priority;

	/* Iterate until we find the route to be placed after ``rt''. */

	prt = (struct rtentry **)&an->an_value;
	while ((mrt = SMR_PTR_GET_LOCKED(prt)) != NULL) {
		if (mrt->rt_priority > prio)
			break;

		prt = &mrt->rt_next;
	}

	SMR_PTR_SET_LOCKED(&rt->rt_next, mrt);
	SMR_PTR_SET_LOCKED(prt, rt);
}

/*
 * Return a pointer to the address (key).  This is an heritage from the
 * BSD radix tree needed to skip the non-address fields from the flavor
 * of "struct sockaddr" used by this routing table.
 */
static inline const uint8_t *
satoaddr(struct rtidx *ri, const struct sockaddr *sa)
{
	return (((const uint8_t *)sa) + ri->r_off);
}

/*
 * Return the prefix length of a mask.
 */
int
rtable_satoplen(sa_family_t af, const struct sockaddr *mask)
{
	const struct domain	*dp;
	uint8_t			*ap, *ep;
	int			 mlen, plen = 0;
	int			 i;

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

	/* Trim trailing zeroes. */
	while (ap < ep && ep[-1] == 0)
		ep--;

	if (ap == ep)
		return (0);

	/* "Beauty" adapted from sbin/route/show.c ... */
	while (ap < ep) {
		switch (*ap++) {
		case 0xff:
			plen += 8;
			break;
		case 0xfe:
			plen += 7;
			goto out;
		case 0xfc:
			plen += 6;
			goto out;
		case 0xf8:
			plen += 5;
			goto out;
		case 0xf0:
			plen += 4;
			goto out;
		case 0xe0:
			plen += 3;
			goto out;
		case 0xc0:
			plen += 2;
			goto out;
		case 0x80:
			plen += 1;
			goto out;
		default:
			/* Non contiguous mask. */
			return (-1);
		}
	}

out:
	if (plen > dp->dom_maxplen || ap != ep)
		return -1;

	return (plen);
}
