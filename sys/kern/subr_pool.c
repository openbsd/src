/*	$OpenBSD: subr_pool.c,v 1.109 2011/09/23 07:27:09 dlg Exp $	*/
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
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/syslog.h>
#include <sys/sysctl.h>

#include <uvm/uvm.h>
#include <dev/rndvar.h>

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
TAILQ_HEAD(,pool) pool_head = TAILQ_HEAD_INITIALIZER(pool_head);

/* Private pool for page header structures */
struct pool phpool;

struct pool_item_header {
	/* Page headers */
	LIST_ENTRY(pool_item_header)
				ph_pagelist;	/* pool page list */
	TAILQ_HEAD(,pool_item)	ph_itemlist;	/* chunk list for this page */
	RB_ENTRY(pool_item_header)
				ph_node;	/* Off-page page headers */
	int			ph_nmissing;	/* # of chunks in use */
	caddr_t			ph_page;	/* this page's address */
	caddr_t			ph_colored;	/* page's colored address */
	int			ph_pagesize;
	int			ph_magic;
};

struct pool_item {
#ifdef DIAGNOSTIC
	u_int32_t pi_magic;
#endif
	/* Other entries use only this list entry */
	TAILQ_ENTRY(pool_item)	pi_list;
};

#ifdef DEADBEEF1
#define	PI_MAGIC DEADBEEF1
#else
#define	PI_MAGIC 0xdeafbeef
#endif

#ifdef POOL_DEBUG
int	pool_debug = 1;
#else
int	pool_debug = 0;
#endif

#define	POOL_NEEDS_CATCHUP(pp)						\
	((pp)->pr_nitems < (pp)->pr_minitems)

/*
 * Every pool gets a unique serial number assigned to it. If this counter
 * wraps, we're screwed, but we shouldn't create so many pools anyway.
 */
unsigned int pool_serial;

int	 pool_catchup(struct pool *);
void	 pool_prime_page(struct pool *, caddr_t, struct pool_item_header *);
void	 pool_update_curpage(struct pool *);
void	*pool_do_get(struct pool *, int);
void	 pool_do_put(struct pool *, void *);
void	 pr_rmpage(struct pool *, struct pool_item_header *,
	    struct pool_pagelist *);
int	pool_chk_page(struct pool *, struct pool_item_header *, int);
struct pool_item_header *pool_alloc_item_header(struct pool *, caddr_t , int);

void	*pool_allocator_alloc(struct pool *, int, int *);
void	 pool_allocator_free(struct pool *, void *);

/*
 * XXX - quick hack. For pools with large items we want to use a special
 *       allocator. For now, instead of having the allocator figure out
 *       the allocation size from the pool (which can be done trivially
 *       with round_page(pr_itemsperpage * pr_size)) which would require
 *	 lots of changes everywhere, we just create allocators for each
 *	 size. We limit those to 128 pages.
 */
#define POOL_LARGE_MAXPAGES 128
struct pool_allocator pool_allocator_large[POOL_LARGE_MAXPAGES];
struct pool_allocator pool_allocator_large_ni[POOL_LARGE_MAXPAGES];
void	*pool_large_alloc(struct pool *, int, int *);
void	pool_large_free(struct pool *, void *);
void	*pool_large_alloc_ni(struct pool *, int, int *);
void	pool_large_free_ni(struct pool *, void *);


#ifdef DDB
void	 pool_print_pagelist(struct pool_pagelist *,
	    int (*)(const char *, ...));
void	 pool_print1(struct pool *, const char *, int (*)(const char *, ...));
#endif

#define pool_sleep(pl) msleep(pl, &pl->pr_mtx, PSWP, pl->pr_wchan, 0)

static __inline int
phtree_compare(struct pool_item_header *a, struct pool_item_header *b)
{
	long diff = (vaddr_t)a->ph_page - (vaddr_t)b->ph_page;
	if (diff < 0)
		return -(-diff >= a->ph_pagesize);
	else if (diff > 0)
		return (diff >= b->ph_pagesize);
	else
		return (0);
}

RB_PROTOTYPE(phtree, pool_item_header, ph_node, phtree_compare);
RB_GENERATE(phtree, pool_item_header, ph_node, phtree_compare);

/*
 * Return the pool page header based on page address.
 */
static __inline struct pool_item_header *
pr_find_pagehead(struct pool *pp, void *v)
{
	struct pool_item_header *ph, tmp;

	if ((pp->pr_roflags & PR_PHINPAGE) != 0) {
		caddr_t page;

		page = (caddr_t)((vaddr_t)v & pp->pr_alloc->pa_pagemask);

		return ((struct pool_item_header *)(page + pp->pr_phoffset));
	}

	/*
	 * The trick we're using in the tree compare function is to compare
	 * two elements equal when they overlap. We want to return the
	 * page header that belongs to the element just before this address.
	 * We don't want this element to compare equal to the next element,
	 * so the compare function takes the pagesize from the lower element.
	 * If this header is the lower, its pagesize is zero, so it can't
	 * overlap with the next header. But if the header we're looking for
	 * is lower, we'll use its pagesize and it will overlap and return
	 * equal.
	 */
	tmp.ph_page = v;
	tmp.ph_pagesize = 0;
	ph = RB_FIND(phtree, &pp->pr_phtree, &tmp);

	if (ph) {
		KASSERT(ph->ph_page <= (caddr_t)v);
		KASSERT(ph->ph_page + ph->ph_pagesize > (caddr_t)v);
	}
	return ph;
}

/*
 * Remove a page from the pool.
 */
void
pr_rmpage(struct pool *pp, struct pool_item_header *ph,
    struct pool_pagelist *pq)
{

	/*
	 * If the page was idle, decrement the idle page count.
	 */
	if (ph->ph_nmissing == 0) {
#ifdef DIAGNOSTIC
		if (pp->pr_nidle == 0)
			panic("pr_rmpage: nidle inconsistent");
		if (pp->pr_nitems < pp->pr_itemsperpage)
			panic("pr_rmpage: nitems inconsistent");
#endif
		pp->pr_nidle--;
	}

	pp->pr_nitems -= pp->pr_itemsperpage;

	/*
	 * Unlink a page from the pool and release it (or queue it for release).
	 */
	LIST_REMOVE(ph, ph_pagelist);
	if ((pp->pr_roflags & PR_PHINPAGE) == 0)
		RB_REMOVE(phtree, &pp->pr_phtree, ph);
	pp->pr_npages--;
	pp->pr_npagefree++;
	pool_update_curpage(pp);

	if (pq) {
		LIST_INSERT_HEAD(pq, ph, ph_pagelist);
	} else {
		pool_allocator_free(pp, ph->ph_page);
		if ((pp->pr_roflags & PR_PHINPAGE) == 0)
			pool_put(&phpool, ph);
	}
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
	int off, slack;

#ifdef MALLOC_DEBUG
	if ((flags & PR_DEBUG) && (ioff != 0 || align != 0))
		flags &= ~PR_DEBUG;
#endif
	/*
	 * Check arguments and construct default values.
	 */
	if (palloc == NULL) {
		if (size > PAGE_SIZE) {
			int psize;

			/*
			 * XXX - should take align into account as well.
			 */
			if (size == round_page(size))
				psize = size / PAGE_SIZE;
			else
				psize = PAGE_SIZE / roundup(size % PAGE_SIZE,
				    1024);
			if (psize > POOL_LARGE_MAXPAGES)
				psize = POOL_LARGE_MAXPAGES;
			if (flags & PR_WAITOK)
				palloc = &pool_allocator_large_ni[psize-1];
			else
				palloc = &pool_allocator_large[psize-1];
			if (palloc->pa_pagesz == 0) {
				palloc->pa_pagesz = psize * PAGE_SIZE;
				if (flags & PR_WAITOK) {
					palloc->pa_alloc = pool_large_alloc_ni;
					palloc->pa_free = pool_large_free_ni;
				} else {
					palloc->pa_alloc = pool_large_alloc;
					palloc->pa_free = pool_large_free;
				}
			}
		} else {
			palloc = &pool_allocator_nointr;
		}
	}
	if (palloc->pa_pagesz == 0) {
		palloc->pa_pagesz = PAGE_SIZE;
	}
	if (palloc->pa_pagemask == 0) {
		palloc->pa_pagemask = ~(palloc->pa_pagesz - 1);
		palloc->pa_pageshift = ffs(palloc->pa_pagesz) - 1;
	}

	if (align == 0)
		align = ALIGN(1);

	if (size < sizeof(struct pool_item))
		size = sizeof(struct pool_item);

	size = roundup(size, align);
#ifdef DIAGNOSTIC
	if (size > palloc->pa_pagesz)
		panic("pool_init: pool item size (%lu) too large",
		    (u_long)size);
#endif

	/*
	 * Initialize the pool structure.
	 */
	LIST_INIT(&pp->pr_emptypages);
	LIST_INIT(&pp->pr_fullpages);
	LIST_INIT(&pp->pr_partpages);
	pp->pr_curpage = NULL;
	pp->pr_npages = 0;
	pp->pr_minitems = 0;
	pp->pr_minpages = 0;
	pp->pr_maxpages = 8;
	pp->pr_roflags = flags;
	pp->pr_flags = 0;
	pp->pr_size = size;
	pp->pr_align = align;
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
	pp->pr_serial = ++pool_serial;
	if (pool_serial == 0)
		panic("pool_init: too much uptime");

        /* constructor, destructor, and arg */
	pp->pr_ctor = NULL;
	pp->pr_dtor = NULL;
	pp->pr_arg = NULL;

	/*
	 * Decide whether to put the page header off page to avoid
	 * wasting too large a part of the page. Off-page page headers
	 * go into an RB tree, so we can match a returned item with
	 * its header based on the page address.
	 * We use 1/16 of the page size as the threshold (XXX: tune)
	 */
	if (pp->pr_size < palloc->pa_pagesz/16 && pp->pr_size < PAGE_SIZE) {
		/* Use the end of the page for the page header */
		pp->pr_roflags |= PR_PHINPAGE;
		pp->pr_phoffset = off = palloc->pa_pagesz -
		    ALIGN(sizeof(struct pool_item_header));
	} else {
		/* The page header will be taken from our page header pool */
		pp->pr_phoffset = 0;
		off = palloc->pa_pagesz;
		RB_INIT(&pp->pr_phtree);
	}

	/*
	 * Alignment is to take place at `ioff' within the item. This means
	 * we must reserve up to `align - 1' bytes on the page to allow
	 * appropriate positioning of each item.
	 *
	 * Silently enforce `0 <= ioff < align'.
	 */
	pp->pr_itemoffset = ioff = ioff % align;
	pp->pr_itemsperpage = (off - ((align - ioff) % align)) / pp->pr_size;
	KASSERT(pp->pr_itemsperpage != 0);

	/*
	 * Use the slack between the chunks and the page header
	 * for "cache coloring".
	 */
	slack = off - pp->pr_itemsperpage * pp->pr_size;
	pp->pr_maxcolor = (slack / align) * align;
	pp->pr_curcolor = 0;

	pp->pr_nget = 0;
	pp->pr_nfail = 0;
	pp->pr_nput = 0;
	pp->pr_npagealloc = 0;
	pp->pr_npagefree = 0;
	pp->pr_hiwat = 0;
	pp->pr_nidle = 0;

	pp->pr_ipl = -1;
	mtx_init(&pp->pr_mtx, IPL_NONE);

	if (phpool.pr_size == 0) {
		pool_init(&phpool, sizeof(struct pool_item_header), 0, 0,
		    0, "phpool", NULL);
		pool_setipl(&phpool, IPL_HIGH);
	}

	/* pglistalloc/constraint parameters */
	pp->pr_crange = &kp_dirty;

	/* Insert this into the list of all pools. */
	TAILQ_INSERT_HEAD(&pool_head, pp, pr_poollist);
}

void
pool_setipl(struct pool *pp, int ipl)
{
	pp->pr_ipl = ipl;
	mtx_init(&pp->pr_mtx, ipl);
}

/*
 * Decommission a pool resource.
 */
void
pool_destroy(struct pool *pp)
{
	struct pool_item_header *ph;

#ifdef DIAGNOSTIC
	if (pp->pr_nout != 0)
		panic("pool_destroy: pool busy: still out: %u", pp->pr_nout);
#endif

	/* Remove all pages */
	while ((ph = LIST_FIRST(&pp->pr_emptypages)) != NULL)
		pr_rmpage(pp, ph, NULL);
	KASSERT(LIST_EMPTY(&pp->pr_fullpages));
	KASSERT(LIST_EMPTY(&pp->pr_partpages));

	/* Remove from global pool list */
	TAILQ_REMOVE(&pool_head, pp, pr_poollist);
}

struct pool_item_header *
pool_alloc_item_header(struct pool *pp, caddr_t storage, int flags)
{
	struct pool_item_header *ph;

	if ((pp->pr_roflags & PR_PHINPAGE) != 0)
		ph = (struct pool_item_header *)(storage + pp->pr_phoffset);
	else
		ph = pool_get(&phpool, (flags & ~(PR_WAITOK | PR_ZERO)) |
		    PR_NOWAIT);
	if (pool_debug && ph != NULL)
		ph->ph_magic = PI_MAGIC;
	return (ph);
}

/*
 * Grab an item from the pool; must be called at appropriate spl level
 */
void *
pool_get(struct pool *pp, int flags)
{
	void *v;

	KASSERT(flags & (PR_WAITOK | PR_NOWAIT));

#ifdef DIAGNOSTIC
	if ((flags & PR_WAITOK) != 0)
		assertwaitok();
#endif /* DIAGNOSTIC */

	mtx_enter(&pp->pr_mtx);
#ifdef POOL_DEBUG
	if (pp->pr_roflags & PR_DEBUGCHK) {
		if (pool_chk(pp))
			panic("before pool_get");
	}
#endif
	v = pool_do_get(pp, flags);
#ifdef POOL_DEBUG
	if (pp->pr_roflags & PR_DEBUGCHK) {
		if (pool_chk(pp))
			panic("after pool_get");
	}
#endif
	mtx_leave(&pp->pr_mtx);
	if (v == NULL)
		return (v);

	if (pp->pr_ctor) {
		if (flags & PR_ZERO)
			panic("pool_get: PR_ZERO when ctor set");
		if (pp->pr_ctor(pp->pr_arg, v, flags)) {
			mtx_enter(&pp->pr_mtx);
			pool_do_put(pp, v);
			mtx_leave(&pp->pr_mtx);
			v = NULL;
		}
	} else {
		if (flags & PR_ZERO)
			memset(v, 0, pp->pr_size);
	}
	if (v != NULL)
		pp->pr_nget++;
	return (v);
}

void *
pool_do_get(struct pool *pp, int flags)
{
	struct pool_item *pi;
	struct pool_item_header *ph;
	void *v;
	int slowdown = 0;
#if defined(DIAGNOSTIC) && defined(POOL_DEBUG)
	int i, *ip;
#endif

#ifdef MALLOC_DEBUG
	if (pp->pr_roflags & PR_DEBUG) {
		void *addr;

		addr = NULL;
		debug_malloc(pp->pr_size, M_DEBUG,
		    (flags & PR_WAITOK) ? M_WAITOK : M_NOWAIT, &addr);
		return (addr);
	}
#endif

startover:
	/*
	 * Check to see if we've reached the hard limit.  If we have,
	 * and we can wait, then wait until an item has been returned to
	 * the pool.
	 */
#ifdef DIAGNOSTIC
	if (pp->pr_nout > pp->pr_hardlimit)
		panic("pool_do_get: %s: crossed hard limit", pp->pr_wchan);
#endif
	if (pp->pr_nout == pp->pr_hardlimit) {
		if ((flags & PR_WAITOK) && !(flags & PR_LIMITFAIL)) {
			/*
			 * XXX: A warning isn't logged in this case.  Should
			 * it be?
			 */
			pp->pr_flags |= PR_WANTED;
			pool_sleep(pp);
			goto startover;
		}

		/*
		 * Log a message that the hard limit has been hit.
		 */
		if (pp->pr_hardlimit_warning != NULL &&
		    ratecheck(&pp->pr_hardlimit_warning_last,
		    &pp->pr_hardlimit_ratecap))
			log(LOG_ERR, "%s\n", pp->pr_hardlimit_warning);

		pp->pr_nfail++;
		return (NULL);
	}

	/*
	 * The convention we use is that if `curpage' is not NULL, then
	 * it points at a non-empty bucket. In particular, `curpage'
	 * never points at a page header which has PR_PHINPAGE set and
	 * has no items in its bucket.
	 */
	if ((ph = pp->pr_curpage) == NULL) {
#ifdef DIAGNOSTIC
		if (pp->pr_nitems != 0) {
			printf("pool_do_get: %s: curpage NULL, nitems %u\n",
			    pp->pr_wchan, pp->pr_nitems);
			panic("pool_do_get: nitems inconsistent");
		}
#endif

		/*
		 * Call the back-end page allocator for more memory.
		 */
		v = pool_allocator_alloc(pp, flags, &slowdown);
		if (v != NULL)
			ph = pool_alloc_item_header(pp, v, flags);

		if (v == NULL || ph == NULL) {
			if (v != NULL)
				pool_allocator_free(pp, v);

			if ((flags & PR_WAITOK) == 0) {
				pp->pr_nfail++;
				return (NULL);
			}

			/*
			 * Wait for items to be returned to this pool.
			 *
			 * XXX: maybe we should wake up once a second and
			 * try again?
			 */
			pp->pr_flags |= PR_WANTED;
			pool_sleep(pp);
			goto startover;
		}

		/* We have more memory; add it to the pool */
		pool_prime_page(pp, v, ph);
		pp->pr_npagealloc++;

		if (slowdown && (flags & PR_WAITOK)) {
			mtx_leave(&pp->pr_mtx);
			yield();
			mtx_enter(&pp->pr_mtx);
		}

		/* Start the allocation process over. */
		goto startover;
	}
	if ((v = pi = TAILQ_FIRST(&ph->ph_itemlist)) == NULL) {
		panic("pool_do_get: %s: page empty", pp->pr_wchan);
	}
#ifdef DIAGNOSTIC
	if (pp->pr_nitems == 0) {
		printf("pool_do_get: %s: items on itemlist, nitems %u\n",
		    pp->pr_wchan, pp->pr_nitems);
		panic("pool_do_get: nitems inconsistent");
	}
#endif

#ifdef DIAGNOSTIC
	if (pi->pi_magic != PI_MAGIC)
		panic("pool_do_get(%s): free list modified: "
		    "page %p; item addr %p; offset 0x%x=0x%x",
		    pp->pr_wchan, ph->ph_page, pi, 0, pi->pi_magic);
#ifdef POOL_DEBUG
	if (pool_debug && ph->ph_magic) {
		for (ip = (int *)pi, i = sizeof(*pi) / sizeof(int);
		    i < pp->pr_size / sizeof(int); i++) {
			if (ip[i] != ph->ph_magic) {
				panic("pool_do_get(%s): free list modified: "
				    "page %p; item addr %p; offset 0x%x=0x%x",
				    pp->pr_wchan, ph->ph_page, pi,
				    i * sizeof(int), ip[i]);
			}
		}
	}
#endif /* POOL_DEBUG */
#endif /* DIAGNOSTIC */

	/*
	 * Remove from item list.
	 */
	TAILQ_REMOVE(&ph->ph_itemlist, pi, pi_list);
	pp->pr_nitems--;
	pp->pr_nout++;
	if (ph->ph_nmissing == 0) {
#ifdef DIAGNOSTIC
		if (pp->pr_nidle == 0)
			panic("pool_do_get: nidle inconsistent");
#endif
		pp->pr_nidle--;

		/*
		 * This page was previously empty.  Move it to the list of
		 * partially-full pages.  This page is already curpage.
		 */
		LIST_REMOVE(ph, ph_pagelist);
		LIST_INSERT_HEAD(&pp->pr_partpages, ph, ph_pagelist);
	}
	ph->ph_nmissing++;
	if (TAILQ_EMPTY(&ph->ph_itemlist)) {
#ifdef DIAGNOSTIC
		if (ph->ph_nmissing != pp->pr_itemsperpage) {
			panic("pool_do_get: %s: nmissing inconsistent",
			    pp->pr_wchan);
		}
#endif
		/*
		 * This page is now full.  Move it to the full list
		 * and select a new current page.
		 */
		LIST_REMOVE(ph, ph_pagelist);
		LIST_INSERT_HEAD(&pp->pr_fullpages, ph, ph_pagelist);
		pool_update_curpage(pp);
	}

	/*
	 * If we have a low water mark and we are now below that low
	 * water mark, add more items to the pool.
	 */
	if (POOL_NEEDS_CATCHUP(pp) && pool_catchup(pp) != 0) {
		/*
		 * XXX: Should we log a warning?  Should we set up a timeout
		 * to try again in a second or so?  The latter could break
		 * a caller's assumptions about interrupt protection, etc.
		 */
	}
	return (v);
}

/*
 * Return resource to the pool; must be called at appropriate spl level
 */
void
pool_put(struct pool *pp, void *v)
{
	if (pp->pr_dtor)
		pp->pr_dtor(pp->pr_arg, v);
	mtx_enter(&pp->pr_mtx);
#ifdef POOL_DEBUG
	if (pp->pr_roflags & PR_DEBUGCHK) {
		if (pool_chk(pp))
			panic("before pool_put");
	}
#endif
	pool_do_put(pp, v);
#ifdef POOL_DEBUG
	if (pp->pr_roflags & PR_DEBUGCHK) {
		if (pool_chk(pp))
			panic("after pool_put");
	}
#endif
	mtx_leave(&pp->pr_mtx);
	pp->pr_nput++;
}

/*
 * Internal version of pool_put().
 */
void
pool_do_put(struct pool *pp, void *v)
{
	struct pool_item *pi = v;
	struct pool_item_header *ph;
#if defined(DIAGNOSTIC) && defined(POOL_DEBUG)
	int i, *ip;
#endif

	if (v == NULL)
		panic("pool_put of NULL");

#ifdef MALLOC_DEBUG
	if (pp->pr_roflags & PR_DEBUG) {
		debug_free(v, M_DEBUG);
		return;
	}
#endif

#ifdef DIAGNOSTIC
	if (pp->pr_ipl != -1)
		splassert(pp->pr_ipl);

	if (pp->pr_nout == 0) {
		printf("pool %s: putting with none out\n",
		    pp->pr_wchan);
		panic("pool_do_put");
	}
#endif

	if ((ph = pr_find_pagehead(pp, v)) == NULL) {
		panic("pool_do_put: %s: page header missing", pp->pr_wchan);
	}

	/*
	 * Return to item list.
	 */
#ifdef DIAGNOSTIC
	pi->pi_magic = PI_MAGIC;
#ifdef POOL_DEBUG
	if (ph->ph_magic) {
		for (ip = (int *)pi, i = sizeof(*pi)/sizeof(int);
		    i < pp->pr_size / sizeof(int); i++)
			ip[i] = ph->ph_magic;
	}
#endif /* POOL_DEBUG */
#endif /* DIAGNOSTIC */

	TAILQ_INSERT_HEAD(&ph->ph_itemlist, pi, pi_list);
	ph->ph_nmissing--;
	pp->pr_nitems++;
	pp->pr_nout--;

	/* Cancel "pool empty" condition if it exists */
	if (pp->pr_curpage == NULL)
		pp->pr_curpage = ph;

	if (pp->pr_flags & PR_WANTED) {
		pp->pr_flags &= ~PR_WANTED;
		wakeup(pp);
	}

	/*
	 * If this page is now empty, do one of two things:
	 *
	 *	(1) If we have more pages than the page high water mark,
	 *	    free the page back to the system.
	 *
	 *	(2) Otherwise, move the page to the empty page list.
	 *
	 * Either way, select a new current page (so we use a partially-full
	 * page if one is available).
	 */
	if (ph->ph_nmissing == 0) {
		pp->pr_nidle++;
		if (pp->pr_nidle > pp->pr_maxpages) {
			pr_rmpage(pp, ph, NULL);
		} else {
			LIST_REMOVE(ph, ph_pagelist);
			LIST_INSERT_HEAD(&pp->pr_emptypages, ph, ph_pagelist);
			pool_update_curpage(pp);
		}
	}

	/*
	 * If the page was previously completely full, move it to the
	 * partially-full list and make it the current page.  The next
	 * allocation will get the item from this page, instead of
	 * further fragmenting the pool.
	 */
	else if (ph->ph_nmissing == (pp->pr_itemsperpage - 1)) {
		LIST_REMOVE(ph, ph_pagelist);
		LIST_INSERT_HEAD(&pp->pr_partpages, ph, ph_pagelist);
		pp->pr_curpage = ph;
	}
}

/*
 * Add N items to the pool.
 */
int
pool_prime(struct pool *pp, int n)
{
	struct pool_item_header *ph;
	caddr_t cp;
	int newpages;
	int slowdown;

	mtx_enter(&pp->pr_mtx);
	newpages = roundup(n, pp->pr_itemsperpage) / pp->pr_itemsperpage;

	while (newpages-- > 0) {
		cp = pool_allocator_alloc(pp, PR_NOWAIT, &slowdown);
		if (cp != NULL)
			ph = pool_alloc_item_header(pp, cp, PR_NOWAIT);
		if (cp == NULL || ph == NULL) {
			if (cp != NULL)
				pool_allocator_free(pp, cp);
			break;
		}

		pool_prime_page(pp, cp, ph);
		pp->pr_npagealloc++;
		pp->pr_minpages++;
	}

	if (pp->pr_minpages >= pp->pr_maxpages)
		pp->pr_maxpages = pp->pr_minpages + 1;	/* XXX */

	mtx_leave(&pp->pr_mtx);
	return (0);
}

/*
 * Add a page worth of items to the pool.
 *
 * Note, we must be called with the pool descriptor LOCKED.
 */
void
pool_prime_page(struct pool *pp, caddr_t storage, struct pool_item_header *ph)
{
	struct pool_item *pi;
	caddr_t cp = storage;
	unsigned int align = pp->pr_align;
	unsigned int ioff = pp->pr_itemoffset;
	int n;
#if defined(DIAGNOSTIC) && defined(POOL_DEBUG)
	int i, *ip;
#endif

	/*
	 * Insert page header.
	 */
	LIST_INSERT_HEAD(&pp->pr_emptypages, ph, ph_pagelist);
	TAILQ_INIT(&ph->ph_itemlist);
	ph->ph_page = storage;
	ph->ph_pagesize = pp->pr_alloc->pa_pagesz;
	ph->ph_nmissing = 0;
	if ((pp->pr_roflags & PR_PHINPAGE) == 0)
		RB_INSERT(phtree, &pp->pr_phtree, ph);

	pp->pr_nidle++;

	/*
	 * Color this page.
	 */
	cp = (caddr_t)(cp + pp->pr_curcolor);
	if ((pp->pr_curcolor += align) > pp->pr_maxcolor)
		pp->pr_curcolor = 0;

	/*
	 * Adjust storage to apply alignment to `pr_itemoffset' in each item.
	 */
	if (ioff != 0)
		cp = (caddr_t)(cp + (align - ioff));
	ph->ph_colored = cp;

	/*
	 * Insert remaining chunks on the bucket list.
	 */
	n = pp->pr_itemsperpage;
	pp->pr_nitems += n;

	while (n--) {
		pi = (struct pool_item *)cp;

		KASSERT(((((vaddr_t)pi) + ioff) & (align - 1)) == 0);

		/* Insert on page list */
		TAILQ_INSERT_TAIL(&ph->ph_itemlist, pi, pi_list);

#ifdef DIAGNOSTIC
		pi->pi_magic = PI_MAGIC;
#ifdef POOL_DEBUG
		if (ph->ph_magic) {
			for (ip = (int *)pi, i = sizeof(*pi)/sizeof(int);
			    i < pp->pr_size / sizeof(int); i++)
				ip[i] = ph->ph_magic;
		}
#endif /* POOL_DEBUG */
#endif /* DIAGNOSTIC */
		cp = (caddr_t)(cp + pp->pr_size);
	}

	/*
	 * If the pool was depleted, point at the new page.
	 */
	if (pp->pr_curpage == NULL)
		pp->pr_curpage = ph;

	if (++pp->pr_npages > pp->pr_hiwat)
		pp->pr_hiwat = pp->pr_npages;
}

/*
 * Used by pool_get() when nitems drops below the low water mark.  This
 * is used to catch up pr_nitems with the low water mark.
 *
 * Note we never wait for memory here, we let the caller decide what to do.
 */
int
pool_catchup(struct pool *pp)
{
	struct pool_item_header *ph;
	caddr_t cp;
	int error = 0;
	int slowdown;

	while (POOL_NEEDS_CATCHUP(pp)) {
		/*
		 * Call the page back-end allocator for more memory.
		 */
		cp = pool_allocator_alloc(pp, PR_NOWAIT, &slowdown);
		if (cp != NULL)
			ph = pool_alloc_item_header(pp, cp, PR_NOWAIT);
		if (cp == NULL || ph == NULL) {
			if (cp != NULL)
				pool_allocator_free(pp, cp);
			error = ENOMEM;
			break;
		}
		pool_prime_page(pp, cp, ph);
		pp->pr_npagealloc++;
	}

	return (error);
}

void
pool_update_curpage(struct pool *pp)
{

	pp->pr_curpage = LIST_FIRST(&pp->pr_partpages);
	if (pp->pr_curpage == NULL) {
		pp->pr_curpage = LIST_FIRST(&pp->pr_emptypages);
	}
}

void
pool_setlowat(struct pool *pp, int n)
{

	pp->pr_minitems = n;
	pp->pr_minpages = (n == 0)
		? 0
		: roundup(n, pp->pr_itemsperpage) / pp->pr_itemsperpage;

	mtx_enter(&pp->pr_mtx);
	/* Make sure we're caught up with the newly-set low water mark. */
	if (POOL_NEEDS_CATCHUP(pp) && pool_catchup(pp) != 0) {
		/*
		 * XXX: Should we log a warning?  Should we set up a timeout
		 * to try again in a second or so?  The latter could break
		 * a caller's assumptions about interrupt protection, etc.
		 */
	}
	mtx_leave(&pp->pr_mtx);
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

void
pool_set_ctordtor(struct pool *pp, int (*ctor)(void *, void *, int),
    void (*dtor)(void *, void *), void *arg)
{
	pp->pr_ctor = ctor;
	pp->pr_dtor = dtor;
	pp->pr_arg = arg;
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
	struct pool_pagelist pq;

	LIST_INIT(&pq);

	mtx_enter(&pp->pr_mtx);
	for (ph = LIST_FIRST(&pp->pr_emptypages); ph != NULL; ph = phnext) {
		phnext = LIST_NEXT(ph, ph_pagelist);

		/* Check our minimum page claim */
		if (pp->pr_npages <= pp->pr_minpages)
			break;

		KASSERT(ph->ph_nmissing == 0);

		/*
		 * If freeing this page would put us below
		 * the low water mark, stop now.
		 */
		if ((pp->pr_nitems - pp->pr_itemsperpage) <
		    pp->pr_minitems)
			break;

		pr_rmpage(pp, ph, &pq);
	}
	mtx_leave(&pp->pr_mtx);

	if (LIST_EMPTY(&pq))
		return (0);
	while ((ph = LIST_FIRST(&pq)) != NULL) {
		LIST_REMOVE(ph, ph_pagelist);
		pool_allocator_free(pp, ph->ph_page);
		if (pp->pr_roflags & PR_PHINPAGE)
			continue;
		pool_put(&phpool, ph);
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
	TAILQ_FOREACH(pp, &pool_head, pr_poollist)
		pool_reclaim(pp);
}

#ifdef DDB
#include <machine/db_machdep.h>
#include <ddb/db_interface.h>
#include <ddb/db_output.h>

/*
 * Diagnostic helpers.
 */
void
pool_printit(struct pool *pp, const char *modif, int (*pr)(const char *, ...))
{
	pool_print1(pp, modif, pr);
}

void
pool_print_pagelist(struct pool_pagelist *pl, int (*pr)(const char *, ...))
{
	struct pool_item_header *ph;
#ifdef DIAGNOSTIC
	struct pool_item *pi;
#endif

	LIST_FOREACH(ph, pl, ph_pagelist) {
		(*pr)("\t\tpage %p, nmissing %d\n",
		    ph->ph_page, ph->ph_nmissing);
#ifdef DIAGNOSTIC
		TAILQ_FOREACH(pi, &ph->ph_itemlist, pi_list) {
			if (pi->pi_magic != PI_MAGIC) {
				(*pr)("\t\t\titem %p, magic 0x%x\n",
				    pi, pi->pi_magic);
			}
		}
#endif
	}
}

void
pool_print1(struct pool *pp, const char *modif, int (*pr)(const char *, ...))
{
	struct pool_item_header *ph;
	int print_pagelist = 0;
	char c;

	while ((c = *modif++) != '\0') {
		if (c == 'p')
			print_pagelist = 1;
		modif++;
	}

	(*pr)("POOL %s: size %u, align %u, ioff %u, roflags 0x%08x\n",
	    pp->pr_wchan, pp->pr_size, pp->pr_align, pp->pr_itemoffset,
	    pp->pr_roflags);
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

	if ((ph = LIST_FIRST(&pp->pr_emptypages)) != NULL)
		(*pr)("\n\tempty page list:\n");
	pool_print_pagelist(&pp->pr_emptypages, pr);
	if ((ph = LIST_FIRST(&pp->pr_fullpages)) != NULL)
		(*pr)("\n\tfull page list:\n");
	pool_print_pagelist(&pp->pr_fullpages, pr);
	if ((ph = LIST_FIRST(&pp->pr_partpages)) != NULL)
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
		db_printf("%-10s %18s %18s\n",
		    "Name", "Address", "Allocator");

	TAILQ_FOREACH(pp, &pool_head, pr_poollist) {
		if (mode == 'a') {
			db_printf("%-10s %18p %18p\n", pp->pr_wchan, pp,
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

int
pool_chk_page(struct pool *pp, struct pool_item_header *ph, int expected)
{
	struct pool_item *pi;
	caddr_t page;
	int n;
#if defined(DIAGNOSTIC) && defined(POOL_DEBUG)
	int i, *ip;
#endif
	const char *label = pp->pr_wchan;

	page = (caddr_t)((u_long)ph & pp->pr_alloc->pa_pagemask);
	if (page != ph->ph_page &&
	    (pp->pr_roflags & PR_PHINPAGE) != 0) {
		printf("%s: ", label);
		printf("pool(%p:%s): page inconsistency: page %p; "
		    "at page head addr %p (p %p)\n",
		    pp, pp->pr_wchan, ph->ph_page, ph, page);
		return 1;
	}

	for (pi = TAILQ_FIRST(&ph->ph_itemlist), n = 0;
	     pi != NULL;
	     pi = TAILQ_NEXT(pi,pi_list), n++) {

#ifdef DIAGNOSTIC
		if (pi->pi_magic != PI_MAGIC) {
			printf("%s: ", label);
			printf("pool(%s): free list modified: "
			    "page %p; item ordinal %d; addr %p "
			    "(p %p); offset 0x%x=0x%x\n",
			    pp->pr_wchan, ph->ph_page, n, pi, page,
			    0, pi->pi_magic);
		}
#ifdef POOL_DEBUG
		if (pool_debug && ph->ph_magic) {
			for (ip = (int *)pi, i = sizeof(*pi) / sizeof(int);
			    i < pp->pr_size / sizeof(int); i++) {
				if (ip[i] != ph->ph_magic) {
					printf("pool(%s): free list modified: "
					    "page %p; item ordinal %d; addr %p "
					    "(p %p); offset 0x%x=0x%x\n",
					    pp->pr_wchan, ph->ph_page, n, pi,
					    page, i * sizeof(int), ip[i]);
				}
			}
		}

#endif /* POOL_DEBUG */
#endif /* DIAGNOSTIC */
		page =
		    (caddr_t)((u_long)pi & pp->pr_alloc->pa_pagemask);
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

	LIST_FOREACH(ph, &pp->pr_emptypages, ph_pagelist)
		r += pool_chk_page(pp, ph, pp->pr_itemsperpage);
	LIST_FOREACH(ph, &pp->pr_fullpages, ph_pagelist)
		r += pool_chk_page(pp, ph, 0);
	LIST_FOREACH(ph, &pp->pr_partpages, ph_pagelist)
		r += pool_chk_page(pp, ph, -1);

	return (r);
}

void
pool_walk(struct pool *pp, int full, int (*pr)(const char *, ...),
    void (*func)(void *, int, int (*)(const char *, ...)))
{
	struct pool_item_header *ph;
	struct pool_item *pi;
	caddr_t cp;
	int n;

	LIST_FOREACH(ph, &pp->pr_fullpages, ph_pagelist) {
		cp = ph->ph_colored;
		n = ph->ph_nmissing;

		while (n--) {
			func(cp, full, pr);
			cp += pp->pr_size;
		}
	}

	LIST_FOREACH(ph, &pp->pr_partpages, ph_pagelist) {
		cp = ph->ph_colored;
		n = ph->ph_nmissing;

		do {
			TAILQ_FOREACH(pi, &ph->ph_itemlist, pi_list) {
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
sysctl_dopool(int *name, u_int namelen, char *where, size_t *sizep)
{
	struct pool *pp, *foundpool = NULL;
	size_t buflen = where != NULL ? *sizep : 0;
	int npools = 0, s;
	unsigned int lookfor;
	size_t len;

	switch (*name) {
	case KERN_POOL_NPOOLS:
		if (namelen != 1 || buflen != sizeof(int))
			return (EINVAL);
		lookfor = 0;
		break;
	case KERN_POOL_NAME:
		if (namelen != 2 || buflen < 1)
			return (EINVAL);
		lookfor = name[1];
		break;
	case KERN_POOL_POOL:
		if (namelen != 2 || buflen != sizeof(struct pool))
			return (EINVAL);
		lookfor = name[1];
		break;
	default:
		return (EINVAL);
	}

	s = splvm();

	TAILQ_FOREACH(pp, &pool_head, pr_poollist) {
		npools++;
		if (lookfor == pp->pr_serial) {
			foundpool = pp;
			break;
		}
	}

	splx(s);

	if (*name != KERN_POOL_NPOOLS && foundpool == NULL)
		return (ENOENT);

	switch (*name) {
	case KERN_POOL_NPOOLS:
		return copyout(&npools, where, buflen);
	case KERN_POOL_NAME:
		len = strlen(foundpool->pr_wchan) + 1;
		if (*sizep < len)
			return (ENOMEM);
		*sizep = len;
		return copyout(foundpool->pr_wchan, where, len);
	case KERN_POOL_POOL:
		return copyout(foundpool, where, buflen);
	}
	/* NOTREACHED */
	return (0); /* XXX - Stupid gcc */
}

/*
 * Pool backend allocators.
 *
 * Each pool has a backend allocator that handles allocation, deallocation
 */
void	*pool_page_alloc(struct pool *, int, int *);
void	pool_page_free(struct pool *, void *);

/*
 * safe for interrupts, name preserved for compat this is the default
 * allocator
 */
struct pool_allocator pool_allocator_nointr = {
	pool_page_alloc, pool_page_free, 0,
};

/*
 * XXX - we have at least three different resources for the same allocation
 *  and each resource can be depleted. First we have the ready elements in
 *  the pool. Then we have the resource (typically a vm_map) for this
 *  allocator, then we have physical memory. Waiting for any of these can
 *  be unnecessary when any other is freed, but the kernel doesn't support
 *  sleeping on multiple addresses, so we have to fake. The caller sleeps on
 *  the pool (so that we can be awakened when an item is returned to the pool),
 *  but we set PA_WANT on the allocator. When a page is returned to
 *  the allocator and PA_WANT is set pool_allocator_free will wakeup all
 *  sleeping pools belonging to this allocator. (XXX - thundering herd).
 *  We also wake up the allocator in case someone without a pool (malloc)
 *  is sleeping waiting for this allocator.
 */

void *
pool_allocator_alloc(struct pool *pp, int flags, int *slowdown)
{
	boolean_t waitok = (flags & PR_WAITOK) ? TRUE : FALSE;
	void *v;

	if (waitok)
		mtx_leave(&pp->pr_mtx);
	v = pp->pr_alloc->pa_alloc(pp, flags, slowdown);
	if (waitok)
		mtx_enter(&pp->pr_mtx);

	return (v);
}

void
pool_allocator_free(struct pool *pp, void *v)
{
	struct pool_allocator *pa = pp->pr_alloc;

	(*pa->pa_free)(pp, v);
}

void *
pool_page_alloc(struct pool *pp, int flags, int *slowdown)
{
	struct kmem_dyn_mode kd = KMEM_DYN_INITIALIZER;

	kd.kd_waitok = (flags & PR_WAITOK);
	kd.kd_slowdown = slowdown;

	return (km_alloc(PAGE_SIZE, &kv_page, pp->pr_crange, &kd));
}

void
pool_page_free(struct pool *pp, void *v)
{
	km_free(v, PAGE_SIZE, &kv_page, pp->pr_crange);
}

void *
pool_large_alloc(struct pool *pp, int flags, int *slowdown)
{
	struct kmem_dyn_mode kd = KMEM_DYN_INITIALIZER;
	void *v;
	int s;

	kd.kd_waitok = (flags & PR_WAITOK);
	kd.kd_slowdown = slowdown;

	s = splvm();
	v = km_alloc(pp->pr_alloc->pa_pagesz, &kv_intrsafe, pp->pr_crange,
	    &kd);
	splx(s);

	return (v);
}

void
pool_large_free(struct pool *pp, void *v)
{
	int s;

	s = splvm();
	km_free(v, pp->pr_alloc->pa_pagesz, &kv_intrsafe, pp->pr_crange);
	splx(s);
}

void *
pool_large_alloc_ni(struct pool *pp, int flags, int *slowdown)
{
	struct kmem_dyn_mode kd = KMEM_DYN_INITIALIZER;

	kd.kd_waitok = (flags & PR_WAITOK);
	kd.kd_slowdown = slowdown;

	return (km_alloc(pp->pr_alloc->pa_pagesz, &kv_any, pp->pr_crange, &kd));
}

void
pool_large_free_ni(struct pool *pp, void *v)
{
	km_free(v, pp->pr_alloc->pa_pagesz, &kv_any, pp->pr_crange);
}
