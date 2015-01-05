/*	$OpenBSD: subr_pool.c,v 1.177 2015/01/05 23:54:18 dlg Exp $	*/
/*	$NetBSD: subr_pool.c,v 1.61 2001/09/26 07:14:56 chs Exp $	*/

/*-
 * Copyright (c) 1997, 1999, 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg; by Jason R. Thorpe of the Numerical Aerospace
 * Simulation Facility, NASA Ames Research Center.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/syslog.h>
#include <sys/rwlock.h>
#include <sys/sysctl.h>

#include <uvm/uvm_extern.h>

/*
 * Pool resource management utility.
 *
 * Memory is allocated in pages which are split into pieces according to
 * the pool item size. Each page is kept on one of three lists in the
 * pool structure: `pr_emptypages', `pr_fullpages' and `pr_partpages',
 * for empty, full and partially-full pages respectively. The individual
 * pool items are on a linked list headed by `ph_itemlist' in each page
 * header. The memory for building the page list is either taken from
 * the allocated pages themselves (for small pool items) or taken from
 * an internal pool of page headers (`phpool').
 */

/* List of all pools */
SIMPLEQ_HEAD(,pool) pool_head = SIMPLEQ_HEAD_INITIALIZER(pool_head);

/*
 * Every pool gets a unique serial number assigned to it. If this counter
 * wraps, we're screwed, but we shouldn't create so many pools anyway.
 */
unsigned int pool_serial;
unsigned int pool_count;

/* Lock the previous variables making up the global pool state */
struct rwlock pool_lock = RWLOCK_INITIALIZER("pools");

/* Private pool for page header structures */
struct pool phpool;

struct pool_item_header {
	/* Page headers */
	TAILQ_ENTRY(pool_item_header)
				ph_pagelist;	/* pool page list */
	XSIMPLEQ_HEAD(,pool_item) ph_itemlist;	/* chunk list for this page */
	RB_ENTRY(pool_item_header)
				ph_node;	/* Off-page page headers */
	int			ph_nmissing;	/* # of chunks in use */
	caddr_t			ph_page;	/* this page's address */
	u_long			ph_magic;
	int			ph_tick;
};
#define POOL_MAGICBIT (1 << 3) /* keep away from perturbed low bits */
#define POOL_PHPOISON(ph) ISSET((ph)->ph_magic, POOL_MAGICBIT)

struct pool_item {
	u_long				pi_magic;
	XSIMPLEQ_ENTRY(pool_item)	pi_list;
};
#define POOL_IMAGIC(ph, pi) ((u_long)(pi) ^ (ph)->ph_magic)

#ifdef POOL_DEBUG
int	pool_debug = 1;
#else
int	pool_debug = 0;
#endif

#define	POOL_NEEDS_CATCHUP(pp)						\
	((pp)->pr_nitems < (pp)->pr_minitems)

#define POOL_INPGHDR(pp) ((pp)->pr_phoffset != 0)

struct pool_item_header *
	 pool_p_alloc(struct pool *, int, int *);
void	 pool_p_insert(struct pool *, struct pool_item_header *);
void	 pool_p_remove(struct pool *, struct pool_item_header *);
void	 pool_p_free(struct pool *, struct pool_item_header *);

void	 pool_update_curpage(struct pool *);
void	*pool_do_get(struct pool *, int, int *);
int	 pool_chk_page(struct pool *, struct pool_item_header *, int);
int	 pool_chk(struct pool *);
void	 pool_get_done(void *, void *);
void	 pool_runqueue(struct pool *, int);

void	*pool_allocator_alloc(struct pool *, int, int *);
void	 pool_allocator_free(struct pool *, void *);

/*
 * The default pool allocator.
 */
void	*pool_page_alloc(struct pool *, int, int *);
void	pool_page_free(struct pool *, void *);

/*
 * safe for interrupts, name preserved for compat this is the default
 * allocator
 */
struct pool_allocator pool_allocator_nointr = {
	pool_page_alloc,
	pool_page_free
};

void	*pool_large_alloc(struct pool *, int, int *);
void	pool_large_free(struct pool *, void *);

struct pool_allocator pool_allocator_large = {
	pool_large_alloc,
	pool_large_free
};

void	*pool_large_alloc_ni(struct pool *, int, int *);
void	pool_large_free_ni(struct pool *, void *);

struct pool_allocator pool_allocator_large_ni = {
	pool_large_alloc_ni,
	pool_large_free_ni
};

#ifdef DDB
void	 pool_print_pagelist(struct pool_pagelist *, int (*)(const char *, ...)
	     __attribute__((__format__(__kprintf__,1,2))));
void	 pool_print1(struct pool *, const char *, int (*)(const char *, ...)
	     __attribute__((__format__(__kprintf__,1,2))));
#endif

#define pool_sleep(pl) msleep(pl, &pl->pr_mtx, PSWP, pl->pr_wchan, 0)

static inline int
phtree_compare(struct pool_item_header *a, struct pool_item_header *b)
{
	vaddr_t va = (vaddr_t)a->ph_page;
	vaddr_t vb = (vaddr_t)b->ph_page;

	/* the compares in this order are important for the NFIND to work */
	if (vb < va)
		return (-1);
	if (vb > va)
		return (1);

	return (0);
}

RB_PROTOTYPE(phtree, pool_item_header, ph_node, phtree_compare);
RB_GENERATE(phtree, pool_item_header, ph_node, phtree_compare);

/*
 * Return the pool page header based on page address.
 */
static inline struct pool_item_header *
pr_find_pagehead(struct pool *pp, void *v)
{
	struct pool_item_header *ph, key;

	if (POOL_INPGHDR(pp)) {
		caddr_t page;

		page = (caddr_t)((vaddr_t)v & pp->pr_pgmask);

		return ((struct pool_item_header *)(page + pp->pr_phoffset));
	}

	key.ph_page = v;
	ph = RB_NFIND(phtree, &pp->pr_phtree, &key);
	if (ph == NULL)
		panic("%s: %s: page header missing", __func__, pp->pr_wchan);

	KASSERT(ph->ph_page <= (caddr_t)v);
	if (ph->ph_page + pp->pr_pgsize <= (caddr_t)v)
		panic("%s: %s: incorrect page", __func__, pp->pr_wchan);

	return (ph);
}

/*
 * Initialize the given pool resource structure.
 *
 * We export this routine to allow other kernel parts to declare
 * static pools that must be initialized before malloc() is available.
 */
void
pool_init(struct pool *pp, size_t size, u_int align, u_int ioff, int flags,
    const char *wchan, struct pool_allocator *palloc)
{
	int off = 0;
	unsigned int pgsize = PAGE_SIZE, items;
#ifdef DIAGNOSTIC
	struct pool *iter;
	KASSERT(ioff == 0);
#endif

	if (align == 0)
		align = ALIGN(1);

	if (size < sizeof(struct pool_item))
		size = sizeof(struct pool_item);

	size = roundup(size, align);

	if (palloc == NULL) {
		while (size > pgsize)
			pgsize <<= 1;

		if (pgsize > PAGE_SIZE) {
			palloc = ISSET(flags, PR_WAITOK) ?
			    &pool_allocator_large_ni : &pool_allocator_large;
		} else
			palloc = &pool_allocator_nointr;
	} else
		pgsize = palloc->pa_pagesz ? palloc->pa_pagesz : PAGE_SIZE;

	items = pgsize / size;

	/*
	 * Decide whether to put the page header off page to avoid
	 * wasting too large a part of the page. Off-page page headers
	 * go into an RB tree, so we can match a returned item with
	 * its header based on the page address.
	 */
	if (pgsize - (size * items) > sizeof(struct pool_item_header)) {
		off = pgsize - sizeof(struct pool_item_header);
	} else if (sizeof(struct pool_item_header) * 2 >= size) {
		off = pgsize - sizeof(struct pool_item_header);
		items = off / size;
	}

	KASSERT(items > 0);

	/*
	 * Initialize the pool structure.
	 */
	memset(pp, 0, sizeof(*pp));
	TAILQ_INIT(&pp->pr_emptypages);
	TAILQ_INIT(&pp->pr_fullpages);
	TAILQ_INIT(&pp->pr_partpages);
	pp->pr_curpage = NULL;
	pp->pr_npages = 0;
	pp->pr_minitems = 0;
	pp->pr_minpages = 0;
	pp->pr_maxpages = 8;
	pp->pr_size = size;
	pp->pr_pgsize = pgsize;
	pp->pr_pgmask = ~0UL ^ (pgsize - 1);
	pp->pr_phoffset = off;
	pp->pr_itemsperpage = items;
	pp->pr_wchan = wchan;
	pp->pr_alloc = palloc;
	pp->pr_nitems = 0;
	pp->pr_nout = 0;
	pp->pr_hardlimit = UINT_MAX;
	pp->pr_hardlimit_warning = NULL;
	pp->pr_hardlimit_ratecap.tv_sec = 0;
	pp->pr_hardlimit_ratecap.tv_usec = 0;
	pp->pr_hardlimit_warning_last.tv_sec = 0;
	pp->pr_hardlimit_warning_last.tv_usec = 0;
	RB_INIT(&pp->pr_phtree);

	pp->pr_nget = 0;
	pp->pr_nfail = 0;
	pp->pr_nput = 0;
	pp->pr_npagealloc = 0;
	pp->pr_npagefree = 0;
	pp->pr_hiwat = 0;
	pp->pr_nidle = 0;

	pp->pr_ipl = -1;
	mtx_init(&pp->pr_mtx, IPL_NONE);
	mtx_init(&pp->pr_requests_mtx, IPL_NONE);
	TAILQ_INIT(&pp->pr_requests);

	if (phpool.pr_size == 0) {
		pool_init(&phpool, sizeof(struct pool_item_header), 0, 0,
		    0, "phpool", NULL);
		pool_setipl(&phpool, IPL_HIGH);

		/* make sure phpool wont "recurse" */
		KASSERT(POOL_INPGHDR(&phpool));
	}

	/* pglistalloc/constraint parameters */
	pp->pr_crange = &kp_dirty;

	/* Insert this into the list of all pools. */
	rw_enter_write(&pool_lock);
#ifdef DIAGNOSTIC
	SIMPLEQ_FOREACH(iter, &pool_head, pr_poollist) {
		if (iter == pp)
			panic("%s: pool %s already on list", __func__, wchan);
	}
#endif

	pp->pr_serial = ++pool_serial;
	if (pool_serial == 0)
		panic("%s: too much uptime", __func__);

	SIMPLEQ_INSERT_HEAD(&pool_head, pp, pr_poollist);
	pool_count++;
	rw_exit_write(&pool_lock);
}

void
pool_setipl(struct pool *pp, int ipl)
{
	pp->pr_ipl = ipl;
	mtx_init(&pp->pr_mtx, ipl);
	mtx_init(&pp->pr_requests_mtx, ipl);
}

/*
 * Decommission a pool resource.
 */
void
pool_destroy(struct pool *pp)
{
	struct pool_item_header *ph;
	struct pool *prev, *iter;

#ifdef DIAGNOSTIC
	if (pp->pr_nout != 0)
		panic("%s: pool busy: still out: %u", __func__, pp->pr_nout);
#endif

	/* Remove from global pool list */
	rw_enter_write(&pool_lock);
	pool_count--;
	if (pp == SIMPLEQ_FIRST(&pool_head))
		SIMPLEQ_REMOVE_HEAD(&pool_head, pr_poollist);
	else {
		prev = SIMPLEQ_FIRST(&pool_head);
		SIMPLEQ_FOREACH(iter, &pool_head, pr_poollist) {
			if (iter == pp) {
				SIMPLEQ_REMOVE_AFTER(&pool_head, prev,
				    pr_poollist);
				break;
			}
			prev = iter;
		}
	}
	rw_exit_write(&pool_lock);

	/* Remove all pages */
	while ((ph = TAILQ_FIRST(&pp->pr_emptypages)) != NULL) {
		mtx_enter(&pp->pr_mtx);
		pool_p_remove(pp, ph);
		mtx_leave(&pp->pr_mtx);
		pool_p_free(pp, ph);
	}
	KASSERT(TAILQ_EMPTY(&pp->pr_fullpages));
	KASSERT(TAILQ_EMPTY(&pp->pr_partpages));
}

void
pool_request_init(struct pool_request *pr,
    void (*handler)(void *, void *), void *cookie)
{
	pr->pr_handler = handler;
	pr->pr_cookie = cookie;
	pr->pr_item = NULL;
}

void
pool_request(struct pool *pp, struct pool_request *pr)
{
	mtx_enter(&pp->pr_requests_mtx);
	TAILQ_INSERT_TAIL(&pp->pr_requests, pr, pr_entry);
	pool_runqueue(pp, PR_NOWAIT);
	mtx_leave(&pp->pr_requests_mtx);
}

struct pool_get_memory {
	struct mutex mtx;
	void * volatile v;
};

/*
 * Grab an item from the pool.
 */
void *
pool_get(struct pool *pp, int flags)
{
	void *v = NULL;
	int slowdown = 0;

	KASSERT(flags & (PR_WAITOK | PR_NOWAIT));


	mtx_enter(&pp->pr_mtx);
	if (pp->pr_nout >= pp->pr_hardlimit) {
		if (ISSET(flags, PR_NOWAIT|PR_LIMITFAIL))
			goto fail;
	} else if ((v = pool_do_get(pp, flags, &slowdown)) == NULL) {
		if (ISSET(flags, PR_NOWAIT))
			goto fail;
	}
	mtx_leave(&pp->pr_mtx);

	if (slowdown && ISSET(flags, PR_WAITOK))
		yield();

	if (v == NULL) {
		struct pool_get_memory mem = {
		    MUTEX_INITIALIZER((pp->pr_ipl == -1) ?
		    IPL_NONE : pp->pr_ipl), NULL };
		struct pool_request pr;

		pool_request_init(&pr, pool_get_done, &mem);
		pool_request(pp, &pr);

		mtx_enter(&mem.mtx);
		while (mem.v == NULL)
			msleep(&mem, &mem.mtx, PSWP, pp->pr_wchan, 0);
		mtx_leave(&mem.mtx);

		v = mem.v;
	}

	if (ISSET(flags, PR_ZERO))
		memset(v, 0, pp->pr_size);

	return (v);

fail:
	pp->pr_nfail++;
	mtx_leave(&pp->pr_mtx);
	return (NULL);
}

void
pool_get_done(void *xmem, void *v)
{
	struct pool_get_memory *mem = xmem;

	mtx_enter(&mem->mtx);
	mem->v = v;
	mtx_leave(&mem->mtx);

	wakeup_one(mem);
}

void
pool_runqueue(struct pool *pp, int flags)
{
	struct pool_requests prl = TAILQ_HEAD_INITIALIZER(prl);
	struct pool_request *pr;

	MUTEX_ASSERT_UNLOCKED(&pp->pr_mtx);
	MUTEX_ASSERT_LOCKED(&pp->pr_requests_mtx);

	if (pp->pr_requesting++)
		return;

	do {
		pp->pr_requesting = 1;

		/* no TAILQ_JOIN? :( */
		while ((pr = TAILQ_FIRST(&pp->pr_requests)) != NULL) {
			TAILQ_REMOVE(&pp->pr_requests, pr, pr_entry);
			TAILQ_INSERT_TAIL(&prl, pr, pr_entry);
		}
		if (TAILQ_EMPTY(&prl))
			continue;

		mtx_leave(&pp->pr_requests_mtx);

		mtx_enter(&pp->pr_mtx);
		pr = TAILQ_FIRST(&prl);
		while (pr != NULL) {
			int slowdown = 0;

			if (pp->pr_nout >= pp->pr_hardlimit)
				break;

			pr->pr_item = pool_do_get(pp, flags, &slowdown);
			if (pr->pr_item == NULL) /* || slowdown ? */
				break;

			pr = TAILQ_NEXT(pr, pr_entry);
		}
		mtx_leave(&pp->pr_mtx);

		while ((pr = TAILQ_FIRST(&prl)) != NULL &&
		    pr->pr_item != NULL) {
			TAILQ_REMOVE(&prl, pr, pr_entry);
			(*pr->pr_handler)(pr->pr_cookie, pr->pr_item);
		}

		mtx_enter(&pp->pr_requests_mtx);
	} while (--pp->pr_requesting);

	/* no TAILQ_JOIN :( */
	while ((pr = TAILQ_FIRST(&prl)) != NULL) {
		TAILQ_REMOVE(&prl, pr, pr_entry);
		TAILQ_INSERT_TAIL(&pp->pr_requests, pr, pr_entry);
	}
}

void *
pool_do_get(struct pool *pp, int flags, int *slowdown)
{
	struct pool_item *pi;
	struct pool_item_header *ph;

	MUTEX_ASSERT_LOCKED(&pp->pr_mtx);

	if (pp->pr_ipl != -1)
		splassert(pp->pr_ipl);

	/*
	 * Account for this item now to avoid races if we need to give up
	 * pr_mtx to allocate a page.
	 */
	pp->pr_nout++;

	if (pp->pr_curpage == NULL) {
		mtx_leave(&pp->pr_mtx);
		ph = pool_p_alloc(pp, flags, slowdown);
		mtx_enter(&pp->pr_mtx);

		if (ph == NULL) {
			pp->pr_nout--;
			return (NULL);
		}

		pool_p_insert(pp, ph);
	}

	ph = pp->pr_curpage;
	pi = XSIMPLEQ_FIRST(&ph->ph_itemlist);
	if (__predict_false(pi == NULL))
		panic("%s: %s: page empty", __func__, pp->pr_wchan);

	if (__predict_false(pi->pi_magic != POOL_IMAGIC(ph, pi))) {
		panic("%s: %s free list modified: "
		    "page %p; item addr %p; offset 0x%x=0x%lx != 0x%lx",
		    __func__, pp->pr_wchan, ph->ph_page, pi,
		    0, pi->pi_magic, POOL_IMAGIC(ph, pi));
	}

	XSIMPLEQ_REMOVE_HEAD(&ph->ph_itemlist, pi_list);

#ifdef DIAGNOSTIC
	if (pool_debug && POOL_PHPOISON(ph)) {
		size_t pidx;
		uint32_t pval;
		if (poison_check(pi + 1, pp->pr_size - sizeof(*pi),
		    &pidx, &pval)) {
			int *ip = (int *)(pi + 1);
			panic("%s: %s free list modified: "
			    "page %p; item addr %p; offset 0x%zx=0x%x",
			    __func__, pp->pr_wchan, ph->ph_page, pi,
			    pidx * sizeof(int), ip[pidx]);
		}
	}
#endif /* DIAGNOSTIC */

	if (ph->ph_nmissing++ == 0) {
		/*
		 * This page was previously empty.  Move it to the list of
		 * partially-full pages.  This page is already curpage.
		 */
		TAILQ_REMOVE(&pp->pr_emptypages, ph, ph_pagelist);
		TAILQ_INSERT_TAIL(&pp->pr_partpages, ph, ph_pagelist);

		pp->pr_nidle--;
	}

	if (ph->ph_nmissing == pp->pr_itemsperpage) {
		/*
		 * This page is now full.  Move it to the full list
		 * and select a new current page.
		 */
		TAILQ_REMOVE(&pp->pr_partpages, ph, ph_pagelist);
		TAILQ_INSERT_TAIL(&pp->pr_fullpages, ph, ph_pagelist);
		pool_update_curpage(pp);
	}

	pp->pr_nget++;

	return (pi);
}

/*
 * Return resource to the pool.
 */
void
pool_put(struct pool *pp, void *v)
{
	struct pool_item *pi = v;
	struct pool_item_header *ph, *freeph = NULL;
	extern int ticks;

#ifdef DIAGNOSTIC
	if (v == NULL)
		panic("%s: NULL item", __func__);
#endif

	mtx_enter(&pp->pr_mtx);

	if (pp->pr_ipl != -1)
		splassert(pp->pr_ipl);

	ph = pr_find_pagehead(pp, v);

#ifdef DIAGNOSTIC
	if (pool_debug) {
		struct pool_item *qi;
		XSIMPLEQ_FOREACH(qi, &ph->ph_itemlist, pi_list) {
			if (pi == qi) {
				panic("%s: %s: double pool_put: %p", __func__,
				    pp->pr_wchan, pi);
			}
		}
	}
#endif /* DIAGNOSTIC */

	pi->pi_magic = POOL_IMAGIC(ph, pi);
	XSIMPLEQ_INSERT_HEAD(&ph->ph_itemlist, pi, pi_list);
#ifdef DIAGNOSTIC
	if (POOL_PHPOISON(ph))
		poison_mem(pi + 1, pp->pr_size - sizeof(*pi));
#endif /* DIAGNOSTIC */

	if (ph->ph_nmissing-- == pp->pr_itemsperpage) {
		/*
		 * The page was previously completely full, move it to the
		 * partially-full list.
		 */
		TAILQ_REMOVE(&pp->pr_fullpages, ph, ph_pagelist);
		TAILQ_INSERT_TAIL(&pp->pr_partpages, ph, ph_pagelist);
	}

	if (ph->ph_nmissing == 0) {
		/*
		 * The page is now empty, so move it to the empty page list.
	 	 */
		pp->pr_nidle++;

		ph->ph_tick = ticks;
		TAILQ_REMOVE(&pp->pr_partpages, ph, ph_pagelist);
		TAILQ_INSERT_TAIL(&pp->pr_emptypages, ph, ph_pagelist);
		pool_update_curpage(pp);
	}

	pp->pr_nout--;
	pp->pr_nput++;

	/* is it time to free a page? */
	if (pp->pr_nidle > pp->pr_maxpages &&
	    (ph = TAILQ_FIRST(&pp->pr_emptypages)) != NULL &&
	    (ticks - ph->ph_tick) > hz) {
		freeph = ph;
		pool_p_remove(pp, freeph);
	}
	mtx_leave(&pp->pr_mtx);

	if (freeph != NULL)
		pool_p_free(pp, freeph);

	mtx_enter(&pp->pr_requests_mtx);
	pool_runqueue(pp, PR_NOWAIT);
	mtx_leave(&pp->pr_requests_mtx);
}

/*
 * Add N items to the pool.
 */
int
pool_prime(struct pool *pp, int n)
{
	struct pool_pagelist pl = TAILQ_HEAD_INITIALIZER(pl);
	struct pool_item_header *ph;
	int newpages;

	newpages = roundup(n, pp->pr_itemsperpage) / pp->pr_itemsperpage;

	while (newpages-- > 0) {
		int slowdown = 0;

		ph = pool_p_alloc(pp, PR_NOWAIT, &slowdown);
		if (ph == NULL) /* or slowdown? */
			break;

		TAILQ_INSERT_TAIL(&pl, ph, ph_pagelist);
	}

	mtx_enter(&pp->pr_mtx);
	while ((ph = TAILQ_FIRST(&pl)) != NULL) {
		TAILQ_REMOVE(&pl, ph, ph_pagelist);
		pool_p_insert(pp, ph);
	}
	mtx_leave(&pp->pr_mtx);

	return (0);
}

struct pool_item_header *
pool_p_alloc(struct pool *pp, int flags, int *slowdown)
{
	struct pool_item_header *ph;
	struct pool_item *pi;
	caddr_t addr;
	int n; 

	MUTEX_ASSERT_UNLOCKED(&pp->pr_mtx);
	KASSERT(pp->pr_size >= sizeof(*pi));

	addr = pool_allocator_alloc(pp, flags, slowdown);
	if (addr == NULL)
		return (NULL);

	if (POOL_INPGHDR(pp))
		ph = (struct pool_item_header *)(addr + pp->pr_phoffset);
	else {
		ph = pool_get(&phpool, flags);
		if (ph == NULL) {
			pool_allocator_free(pp, addr);
			return (NULL);
		}
	}

	XSIMPLEQ_INIT(&ph->ph_itemlist);
	ph->ph_page = addr;
	ph->ph_nmissing = 0;
	arc4random_buf(&ph->ph_magic, sizeof(ph->ph_magic));
#ifdef DIAGNOSTIC
	/* use a bit in ph_magic to record if we poison page items */
	if (pool_debug)
		SET(ph->ph_magic, POOL_MAGICBIT);
	else
		CLR(ph->ph_magic, POOL_MAGICBIT);
#endif /* DIAGNOSTIC */

	n = pp->pr_itemsperpage;
	while (n--) {
		pi = (struct pool_item *)addr;
		pi->pi_magic = POOL_IMAGIC(ph, pi);
		XSIMPLEQ_INSERT_TAIL(&ph->ph_itemlist, pi, pi_list);

#ifdef DIAGNOSTIC
		if (POOL_PHPOISON(ph))
			poison_mem(pi + 1, pp->pr_size - sizeof(*pi));
#endif /* DIAGNOSTIC */

		addr += pp->pr_size;
	}

	return (ph);
}

void
pool_p_free(struct pool *pp, struct pool_item_header *ph)
{
	struct pool_item *pi;

        MUTEX_ASSERT_UNLOCKED(&pp->pr_mtx);
        KASSERT(ph->ph_nmissing == 0);

	XSIMPLEQ_FOREACH(pi, &ph->ph_itemlist, pi_list) {
		if (__predict_false(pi->pi_magic != POOL_IMAGIC(ph, pi))) {
			panic("%s: %s free list modified: "
			    "page %p; item addr %p; offset 0x%x=0x%lx",
			    __func__, pp->pr_wchan, ph->ph_page, pi,
			    0, pi->pi_magic);
		}

#ifdef DIAGNOSTIC
		if (POOL_PHPOISON(ph)) {
			size_t pidx;
			uint32_t pval;
			if (poison_check(pi + 1, pp->pr_size - sizeof(*pi),
			    &pidx, &pval)) {
				int *ip = (int *)(pi + 1);
				panic("%s: %s free list modified: "
				    "page %p; item addr %p; offset 0x%zx=0x%x",
				    __func__, pp->pr_wchan, ph->ph_page, pi,
				    pidx * sizeof(int), ip[pidx]);
			}
		}
#endif
        }

        pool_allocator_free(pp, ph->ph_page);

	if (!POOL_INPGHDR(pp))
		pool_put(&phpool, ph);
}

void
pool_p_insert(struct pool *pp, struct pool_item_header *ph)
{
        MUTEX_ASSERT_LOCKED(&pp->pr_mtx);

	/* If the pool was depleted, point at the new page */
	if (pp->pr_curpage == NULL)
		pp->pr_curpage = ph;

	TAILQ_INSERT_TAIL(&pp->pr_emptypages, ph, ph_pagelist);
	if (!POOL_INPGHDR(pp))
		RB_INSERT(phtree, &pp->pr_phtree, ph);

	pp->pr_nitems += pp->pr_itemsperpage;
	pp->pr_nidle++;

	pp->pr_npagealloc++;
	if (++pp->pr_npages > pp->pr_hiwat)
		pp->pr_hiwat = pp->pr_npages;
}

void
pool_p_remove(struct pool *pp, struct pool_item_header *ph)
{
	MUTEX_ASSERT_LOCKED(&pp->pr_mtx);

	pp->pr_npagefree++;
	pp->pr_npages--;
	pp->pr_nidle--;
	pp->pr_nitems -= pp->pr_itemsperpage;

	if (!POOL_INPGHDR(pp))
		RB_REMOVE(phtree, &pp->pr_phtree, ph);
	TAILQ_REMOVE(&pp->pr_emptypages, ph, ph_pagelist);

	pool_update_curpage(pp);
}

void
pool_update_curpage(struct pool *pp)
{
	pp->pr_curpage = TAILQ_LAST(&pp->pr_partpages, pool_pagelist);
	if (pp->pr_curpage == NULL) {
		pp->pr_curpage = TAILQ_LAST(&pp->pr_emptypages, pool_pagelist);
	}
}

void
pool_setlowat(struct pool *pp, int n)
{
	int prime = 0;

	mtx_enter(&pp->pr_mtx);
	pp->pr_minitems = n;
	pp->pr_minpages = (n == 0)
		? 0
		: roundup(n, pp->pr_itemsperpage) / pp->pr_itemsperpage;

	if (pp->pr_nitems < n)
		prime = n - pp->pr_nitems;
	mtx_leave(&pp->pr_mtx);

	if (prime > 0)
		pool_prime(pp, prime);
}

void
pool_sethiwat(struct pool *pp, int n)
{
	pp->pr_maxpages = (n == 0)
		? 0
		: roundup(n, pp->pr_itemsperpage) / pp->pr_itemsperpage;
}

int
pool_sethardlimit(struct pool *pp, u_int n, const char *warnmsg, int ratecap)
{
	int error = 0;

	if (n < pp->pr_nout) {
		error = EINVAL;
		goto done;
	}

	pp->pr_hardlimit = n;
	pp->pr_hardlimit_warning = warnmsg;
	pp->pr_hardlimit_ratecap.tv_sec = ratecap;
	pp->pr_hardlimit_warning_last.tv_sec = 0;
	pp->pr_hardlimit_warning_last.tv_usec = 0;

done:
	return (error);
}

void
pool_set_constraints(struct pool *pp, const struct kmem_pa_mode *mode)
{
	pp->pr_crange = mode;
}

/*
 * Release all complete pages that have not been used recently.
 *
 * Returns non-zero if any pages have been reclaimed.
 */
int
pool_reclaim(struct pool *pp)
{
	struct pool_item_header *ph, *phnext;
	struct pool_pagelist pl = TAILQ_HEAD_INITIALIZER(pl);

	mtx_enter(&pp->pr_mtx);
	for (ph = TAILQ_FIRST(&pp->pr_emptypages); ph != NULL; ph = phnext) {
		phnext = TAILQ_NEXT(ph, ph_pagelist);

		/* Check our minimum page claim */
		if (pp->pr_npages <= pp->pr_minpages)
			break;

		/*
		 * If freeing this page would put us below
		 * the low water mark, stop now.
		 */
		if ((pp->pr_nitems - pp->pr_itemsperpage) <
		    pp->pr_minitems)
			break;

		pool_p_remove(pp, ph);
		TAILQ_INSERT_TAIL(&pl, ph, ph_pagelist);
	}
	mtx_leave(&pp->pr_mtx);

	if (TAILQ_EMPTY(&pl))
		return (0);

	while ((ph = TAILQ_FIRST(&pl)) != NULL) {
		TAILQ_REMOVE(&pl, ph, ph_pagelist);
		pool_p_free(pp, ph);
	}

	return (1);
}

/*
 * Release all complete pages that have not been used recently
 * from all pools.
 */
void
pool_reclaim_all(void)
{
	struct pool	*pp;

	rw_enter_read(&pool_lock);
	SIMPLEQ_FOREACH(pp, &pool_head, pr_poollist)
		pool_reclaim(pp);
	rw_exit_read(&pool_lock);
}

#ifdef DDB
#include <machine/db_machdep.h>
#include <ddb/db_interface.h>
#include <ddb/db_output.h>

/*
 * Diagnostic helpers.
 */
void
pool_printit(struct pool *pp, const char *modif,
    int (*pr)(const char *, ...) __attribute__((__format__(__kprintf__,1,2))))
{
	pool_print1(pp, modif, pr);
}

void
pool_print_pagelist(struct pool_pagelist *pl,
    int (*pr)(const char *, ...) __attribute__((__format__(__kprintf__,1,2))))
{
	struct pool_item_header *ph;
	struct pool_item *pi;

	TAILQ_FOREACH(ph, pl, ph_pagelist) {
		(*pr)("\t\tpage %p, nmissing %d\n",
		    ph->ph_page, ph->ph_nmissing);
		XSIMPLEQ_FOREACH(pi, &ph->ph_itemlist, pi_list) {
			if (pi->pi_magic != POOL_IMAGIC(ph, pi)) {
				(*pr)("\t\t\titem %p, magic 0x%lx\n",
				    pi, pi->pi_magic);
			}
		}
	}
}

void
pool_print1(struct pool *pp, const char *modif,
    int (*pr)(const char *, ...) __attribute__((__format__(__kprintf__,1,2))))
{
	struct pool_item_header *ph;
	int print_pagelist = 0;
	char c;

	while ((c = *modif++) != '\0') {
		if (c == 'p')
			print_pagelist = 1;
		modif++;
	}

	(*pr)("POOL %s: size %u\n", pp->pr_wchan, pp->pr_size);
	(*pr)("\talloc %p\n", pp->pr_alloc);
	(*pr)("\tminitems %u, minpages %u, maxpages %u, npages %u\n",
	    pp->pr_minitems, pp->pr_minpages, pp->pr_maxpages, pp->pr_npages);
	(*pr)("\titemsperpage %u, nitems %u, nout %u, hardlimit %u\n",
	    pp->pr_itemsperpage, pp->pr_nitems, pp->pr_nout, pp->pr_hardlimit);

	(*pr)("\n\tnget %lu, nfail %lu, nput %lu\n",
	    pp->pr_nget, pp->pr_nfail, pp->pr_nput);
	(*pr)("\tnpagealloc %lu, npagefree %lu, hiwat %u, nidle %lu\n",
	    pp->pr_npagealloc, pp->pr_npagefree, pp->pr_hiwat, pp->pr_nidle);

	if (print_pagelist == 0)
		return;

	if ((ph = TAILQ_FIRST(&pp->pr_emptypages)) != NULL)
		(*pr)("\n\tempty page list:\n");
	pool_print_pagelist(&pp->pr_emptypages, pr);
	if ((ph = TAILQ_FIRST(&pp->pr_fullpages)) != NULL)
		(*pr)("\n\tfull page list:\n");
	pool_print_pagelist(&pp->pr_fullpages, pr);
	if ((ph = TAILQ_FIRST(&pp->pr_partpages)) != NULL)
		(*pr)("\n\tpartial-page list:\n");
	pool_print_pagelist(&pp->pr_partpages, pr);

	if (pp->pr_curpage == NULL)
		(*pr)("\tno current page\n");
	else
		(*pr)("\tcurpage %p\n", pp->pr_curpage->ph_page);
}

void
db_show_all_pools(db_expr_t expr, int haddr, db_expr_t count, char *modif)
{
	struct pool *pp;
	char maxp[16];
	int ovflw;
	char mode;

	mode = modif[0];
	if (mode != '\0' && mode != 'a') {
		db_printf("usage: show all pools [/a]\n");
		return;
	}

	if (mode == '\0')
		db_printf("%-10s%4s%9s%5s%9s%6s%6s%6s%6s%6s%6s%5s\n",
		    "Name",
		    "Size",
		    "Requests",
		    "Fail",
		    "Releases",
		    "Pgreq",
		    "Pgrel",
		    "Npage",
		    "Hiwat",
		    "Minpg",
		    "Maxpg",
		    "Idle");
	else
		db_printf("%-12s %18s %18s\n",
		    "Name", "Address", "Allocator");

	SIMPLEQ_FOREACH(pp, &pool_head, pr_poollist) {
		if (mode == 'a') {
			db_printf("%-12s %18p %18p\n", pp->pr_wchan, pp,
			    pp->pr_alloc);
			continue;
		}

		if (!pp->pr_nget)
			continue;

		if (pp->pr_maxpages == UINT_MAX)
			snprintf(maxp, sizeof maxp, "inf");
		else
			snprintf(maxp, sizeof maxp, "%u", pp->pr_maxpages);

#define PRWORD(ovflw, fmt, width, fixed, val) do {	\
	(ovflw) += db_printf((fmt),			\
	    (width) - (fixed) - (ovflw) > 0 ?		\
	    (width) - (fixed) - (ovflw) : 0,		\
	    (val)) - (width);				\
	if ((ovflw) < 0)				\
		(ovflw) = 0;				\
} while (/* CONSTCOND */0)

		ovflw = 0;
		PRWORD(ovflw, "%-*s", 10, 0, pp->pr_wchan);
		PRWORD(ovflw, " %*u", 4, 1, pp->pr_size);
		PRWORD(ovflw, " %*lu", 9, 1, pp->pr_nget);
		PRWORD(ovflw, " %*lu", 5, 1, pp->pr_nfail);
		PRWORD(ovflw, " %*lu", 9, 1, pp->pr_nput);
		PRWORD(ovflw, " %*lu", 6, 1, pp->pr_npagealloc);
		PRWORD(ovflw, " %*lu", 6, 1, pp->pr_npagefree);
		PRWORD(ovflw, " %*d", 6, 1, pp->pr_npages);
		PRWORD(ovflw, " %*d", 6, 1, pp->pr_hiwat);
		PRWORD(ovflw, " %*d", 6, 1, pp->pr_minpages);
		PRWORD(ovflw, " %*s", 6, 1, maxp);
		PRWORD(ovflw, " %*lu\n", 5, 1, pp->pr_nidle);

		pool_chk(pp);
	}
}
#endif /* DDB */

#if defined(POOL_DEBUG) || defined(DDB)
int
pool_chk_page(struct pool *pp, struct pool_item_header *ph, int expected)
{
	struct pool_item *pi;
	caddr_t page;
	int n;
	const char *label = pp->pr_wchan;

	page = (caddr_t)((u_long)ph & pp->pr_pgmask);
	if (page != ph->ph_page && POOL_INPGHDR(pp)) {
		printf("%s: ", label);
		printf("pool(%p:%s): page inconsistency: page %p; "
		    "at page head addr %p (p %p)\n",
		    pp, pp->pr_wchan, ph->ph_page, ph, page);
		return 1;
	}

	for (pi = XSIMPLEQ_FIRST(&ph->ph_itemlist), n = 0;
	     pi != NULL;
	     pi = XSIMPLEQ_NEXT(&ph->ph_itemlist, pi, pi_list), n++) {
		if (pi->pi_magic != POOL_IMAGIC(ph, pi)) {
			printf("%s: ", label);
			printf("pool(%p:%s): free list modified: "
			    "page %p; item ordinal %d; addr %p "
			    "(p %p); offset 0x%x=0x%lx\n",
			    pp, pp->pr_wchan, ph->ph_page, n, pi, page,
			    0, pi->pi_magic);
		}

#ifdef DIAGNOSTIC
		if (POOL_PHPOISON(ph)) {
			size_t pidx;
			uint32_t pval;
			if (poison_check(pi + 1, pp->pr_size - sizeof(*pi),
			    &pidx, &pval)) {
				int *ip = (int *)(pi + 1);
				printf("pool(%s): free list modified: "
				    "page %p; item ordinal %d; addr %p "
				    "(p %p); offset 0x%zx=0x%x\n",
				    pp->pr_wchan, ph->ph_page, n, pi,
				    page, pidx * sizeof(int), ip[pidx]);
			}
		}
#endif /* DIAGNOSTIC */

		page = (caddr_t)((u_long)pi & pp->pr_pgmask);
		if (page == ph->ph_page)
			continue;

		printf("%s: ", label);
		printf("pool(%p:%s): page inconsistency: page %p;"
		    " item ordinal %d; addr %p (p %p)\n", pp,
		    pp->pr_wchan, ph->ph_page, n, pi, page);
		return 1;
	}
	if (n + ph->ph_nmissing != pp->pr_itemsperpage) {
		printf("pool(%p:%s): page inconsistency: page %p;"
		    " %d on list, %d missing, %d items per page\n", pp,
		    pp->pr_wchan, ph->ph_page, n, ph->ph_nmissing,
		    pp->pr_itemsperpage);
		return 1;
	}
	if (expected >= 0 && n != expected) {
		printf("pool(%p:%s): page inconsistency: page %p;"
		    " %d on list, %d missing, %d expected\n", pp,
		    pp->pr_wchan, ph->ph_page, n, ph->ph_nmissing,
		    expected);
		return 1;
	}
	return 0;
}

int
pool_chk(struct pool *pp)
{
	struct pool_item_header *ph;
	int r = 0;

	TAILQ_FOREACH(ph, &pp->pr_emptypages, ph_pagelist)
		r += pool_chk_page(pp, ph, pp->pr_itemsperpage);
	TAILQ_FOREACH(ph, &pp->pr_fullpages, ph_pagelist)
		r += pool_chk_page(pp, ph, 0);
	TAILQ_FOREACH(ph, &pp->pr_partpages, ph_pagelist)
		r += pool_chk_page(pp, ph, -1);

	return (r);
}
#endif /* defined(POOL_DEBUG) || defined(DDB) */

#ifdef DDB
void
pool_walk(struct pool *pp, int full,
    int (*pr)(const char *, ...) __attribute__((__format__(__kprintf__,1,2))),
    void (*func)(void *, int, int (*)(const char *, ...)
	    __attribute__((__format__(__kprintf__,1,2)))))
{
	struct pool_item_header *ph;
	struct pool_item *pi;
	caddr_t cp;
	int n;

	TAILQ_FOREACH(ph, &pp->pr_fullpages, ph_pagelist) {
		cp = ph->ph_page;
		n = ph->ph_nmissing;

		while (n--) {
			func(cp, full, pr);
			cp += pp->pr_size;
		}
	}

	TAILQ_FOREACH(ph, &pp->pr_partpages, ph_pagelist) {
		cp = ph->ph_page;
		n = ph->ph_nmissing;

		do {
			XSIMPLEQ_FOREACH(pi, &ph->ph_itemlist, pi_list) {
				if (cp == (caddr_t)pi)
					break;
			}
			if (cp != (caddr_t)pi) {
				func(cp, full, pr);
				n--;
			}

			cp += pp->pr_size;
		} while (n > 0);
	}
}
#endif

/*
 * We have three different sysctls.
 * kern.pool.npools - the number of pools.
 * kern.pool.pool.<pool#> - the pool struct for the pool#.
 * kern.pool.name.<pool#> - the name for pool#.
 */
int
sysctl_dopool(int *name, u_int namelen, char *oldp, size_t *oldlenp)
{
	struct kinfo_pool pi;
	struct pool *pp;
	int rv = ENOENT;

	switch (name[0]) {
	case KERN_POOL_NPOOLS:
		if (namelen != 1)
			return (ENOTDIR);
		return (sysctl_rdint(oldp, oldlenp, NULL, pool_count));

	case KERN_POOL_NAME:
	case KERN_POOL_POOL:
		break;
	default:
		return (EOPNOTSUPP);
	}

	if (namelen != 2)
		return (ENOTDIR);

	rw_enter_read(&pool_lock);

	SIMPLEQ_FOREACH(pp, &pool_head, pr_poollist) {
		if (name[1] == pp->pr_serial)
			break;
	}

	if (pp == NULL)
		goto done;

	switch (name[0]) {
	case KERN_POOL_NAME:
		rv = sysctl_rdstring(oldp, oldlenp, NULL, pp->pr_wchan);
		break;
	case KERN_POOL_POOL:
		memset(&pi, 0, sizeof(pi));

		if (pp->pr_ipl != -1)
			mtx_enter(&pp->pr_mtx);
		pi.pr_size = pp->pr_size;
		pi.pr_pgsize = pp->pr_pgsize;
		pi.pr_itemsperpage = pp->pr_itemsperpage;
		pi.pr_npages = pp->pr_npages;
		pi.pr_minpages = pp->pr_minpages;
		pi.pr_maxpages = pp->pr_maxpages;
		pi.pr_hardlimit = pp->pr_hardlimit;
		pi.pr_nout = pp->pr_nout;
		pi.pr_nitems = pp->pr_nitems;
		pi.pr_nget = pp->pr_nget;
		pi.pr_nput = pp->pr_nput;
		pi.pr_nfail = pp->pr_nfail;
		pi.pr_npagealloc = pp->pr_npagealloc;
		pi.pr_npagefree = pp->pr_npagefree;
		pi.pr_hiwat = pp->pr_hiwat;
		pi.pr_nidle = pp->pr_nidle;
		if (pp->pr_ipl != -1)
			mtx_leave(&pp->pr_mtx);

		rv = sysctl_rdstruct(oldp, oldlenp, NULL, &pi, sizeof(pi));
		break;
	}

done:
	rw_exit_read(&pool_lock);

	return (rv);
}

/*
 * Pool backend allocators.
 */

void *
pool_allocator_alloc(struct pool *pp, int flags, int *slowdown)
{
	void *v;

	KERNEL_LOCK();
	v = (*pp->pr_alloc->pa_alloc)(pp, flags, slowdown);
	KERNEL_UNLOCK();

#ifdef DIAGNOSTIC
	if (v != NULL && POOL_INPGHDR(pp)) {
		vaddr_t addr = (vaddr_t)v;
		if ((addr & pp->pr_pgmask) != addr) {
			panic("%s: %s page address %p isnt aligned to %u",
			    __func__, pp->pr_wchan, v, pp->pr_pgsize);
		}
	}
#endif

	return (v);
}

void
pool_allocator_free(struct pool *pp, void *v)
{
	struct pool_allocator *pa = pp->pr_alloc;

	KERNEL_LOCK();
	(*pa->pa_free)(pp, v);
	KERNEL_UNLOCK();
}

void *
pool_page_alloc(struct pool *pp, int flags, int *slowdown)
{
	struct kmem_dyn_mode kd = KMEM_DYN_INITIALIZER;

	kd.kd_waitok = ISSET(flags, PR_WAITOK);
	kd.kd_slowdown = slowdown;

	return (km_alloc(pp->pr_pgsize, &kv_page, pp->pr_crange, &kd));
}

void
pool_page_free(struct pool *pp, void *v)
{
	km_free(v, pp->pr_pgsize, &kv_page, pp->pr_crange);
}

void *
pool_large_alloc(struct pool *pp, int flags, int *slowdown)
{
	struct kmem_va_mode kv = kv_intrsafe;
	struct kmem_dyn_mode kd = KMEM_DYN_INITIALIZER;
	void *v;
	int s;

	if (POOL_INPGHDR(pp))
		kv.kv_align = pp->pr_pgsize;

	kd.kd_waitok = ISSET(flags, PR_WAITOK);
	kd.kd_slowdown = slowdown;

	s = splvm();
	v = km_alloc(pp->pr_pgsize, &kv, pp->pr_crange, &kd);
	splx(s);

	return (v);
}

void
pool_large_free(struct pool *pp, void *v)
{
	struct kmem_va_mode kv = kv_intrsafe;
	int s;

	if (POOL_INPGHDR(pp))
		kv.kv_align = pp->pr_pgsize;

	s = splvm();
	km_free(v, pp->pr_pgsize, &kv, pp->pr_crange);
	splx(s);
}

void *
pool_large_alloc_ni(struct pool *pp, int flags, int *slowdown)
{
	struct kmem_va_mode kv = kv_any;
	struct kmem_dyn_mode kd = KMEM_DYN_INITIALIZER;

	if (POOL_INPGHDR(pp))
		kv.kv_align = pp->pr_pgsize;

	kd.kd_waitok = ISSET(flags, PR_WAITOK);
	kd.kd_slowdown = slowdown;

	return (km_alloc(pp->pr_pgsize, &kv, pp->pr_crange, &kd));
}

void
pool_large_free_ni(struct pool *pp, void *v)
{
	struct kmem_va_mode kv = kv_any;

	if (POOL_INPGHDR(pp))
		kv.kv_align = pp->pr_pgsize;

	km_free(v, pp->pr_pgsize, &kv, pp->pr_crange);
}
