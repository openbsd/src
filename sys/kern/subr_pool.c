/*	$OpenBSD: subr_pool.c,v 1.1 1999/02/26 03:13:30 art Exp $	*/
/*	$NetBSD: subr_pool.c,v 1.17 1998/12/27 21:13:43 thorpej Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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
#include <sys/lock.h>
#include <sys/pool.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>

#if defined(UVM)
#include <uvm/uvm.h>
#endif  

/*
 * Pool resource management utility.
 *
 * Memory is allocated in pages which are split into pieces according
 * to the pool item size. Each page is kept on a list headed by `pr_pagelist'
 * in the pool structure and the individual pool items are on a linked list
 * headed by `ph_itemlist' in each page header. The memory for building
 * the page list is either taken from the allocated pages themselves (for
 * small pool items) or taken from an internal pool of page headers (`phpool').
 * 
 */

/* List of all pools */
TAILQ_HEAD(,pool) pool_head = { NULL, &(pool_head).tqh_first };

/* Private pool for page header structures */
static struct pool phpool;

/* # of seconds to retain page after last use */
int pool_inactive_time = 10;

/* Next candidate for drainage (see pool_drain()) */
static struct pool	*drainpp = NULL;

struct pool_item_header {
	/* Page headers */
	TAILQ_ENTRY(pool_item_header)
				ph_pagelist;	/* pool page list */
	TAILQ_HEAD(,pool_item)	ph_itemlist;	/* chunk list for this page */
	LIST_ENTRY(pool_item_header)
				ph_hashlist;	/* Off-page page headers */
	int			ph_nmissing;	/* # of chunks in use */
	caddr_t			ph_page;	/* this page's address */
	struct timeval		ph_time;	/* last referenced */
};

struct pool_item {
#ifdef DIAGNOSTIC
	int pi_magic;
#define PI_MAGIC 0xdeadbeef
#endif
	/* Other entries use only this list entry */
	TAILQ_ENTRY(pool_item)	pi_list;
};


#define PR_HASH_INDEX(pp,addr) \
	(((u_long)(addr) >> (pp)->pr_pageshift) & (PR_HASHTABSIZE - 1))



static struct pool_item_header
		*pr_find_pagehead __P((struct pool *, caddr_t));
static void	pr_rmpage __P((struct pool *, struct pool_item_header *));
static int	pool_prime_page __P((struct pool *, caddr_t));
static void	*pool_page_alloc __P((unsigned long, int, int));
static void	pool_page_free __P((void *, unsigned long, int));


#ifdef POOL_DIAGNOSTIC
/*
 * Pool log entry. An array of these is allocated in pool_create().
 */
struct pool_log {
	const char	*pl_file;
	long		pl_line;
	int		pl_action;
#define PRLOG_GET	1
#define PRLOG_PUT	2
	void		*pl_addr;
};

/* Number of entries in pool log buffers */
#ifndef POOL_LOGSIZE
#define	POOL_LOGSIZE	10
#endif

int pool_logsize = POOL_LOGSIZE;

static void	pr_log __P((struct pool *, void *, int, const char *, long));
static void	pr_printlog __P((struct pool *));

static __inline__ void
pr_log(pp, v, action, file, line)
	struct pool	*pp;
	void		*v;
	int		action;
	const char	*file;
	long		line;
{
	int n = pp->pr_curlogentry;
	struct pool_log *pl;

	if ((pp->pr_flags & PR_LOGGING) == 0)
		return;

	/*
	 * Fill in the current entry. Wrap around and overwrite
	 * the oldest entry if necessary.
	 */
	pl = &pp->pr_log[n];
	pl->pl_file = file;
	pl->pl_line = line;
	pl->pl_action = action;
	pl->pl_addr = v;
	if (++n >= pp->pr_logsize)
		n = 0;
	pp->pr_curlogentry = n;
}

static void
pr_printlog(pp)
	struct pool *pp;
{
	int i = pp->pr_logsize;
	int n = pp->pr_curlogentry;

	if ((pp->pr_flags & PR_LOGGING) == 0)
		return;

	pool_print(pp, "printlog");

	/*
	 * Print all entries in this pool's log.
	 */
	while (i-- > 0) {
		struct pool_log *pl = &pp->pr_log[n];
		if (pl->pl_action != 0) {
			printf("log entry %d:\n", i);
			printf("\taction = %s, addr = %p\n",
				pl->pl_action == PRLOG_GET ? "get" : "put",
				pl->pl_addr);
			printf("\tfile: %s at line %lu\n",
				pl->pl_file, pl->pl_line);
		}
		if (++n >= pp->pr_logsize)
			n = 0;
	}
}
#else
#define pr_log(pp, v, action, file, line)
#define pr_printlog(pp)
#endif


/*
 * Return the pool page header based on page address.
 */
static __inline__ struct pool_item_header *
pr_find_pagehead(pp, page)
	struct pool *pp;
	caddr_t page;
{
	struct pool_item_header *ph;

	if ((pp->pr_flags & PR_PHINPAGE) != 0)
		return ((struct pool_item_header *)(page + pp->pr_phoffset));

	for (ph = LIST_FIRST(&pp->pr_hashtab[PR_HASH_INDEX(pp, page)]);
	     ph != NULL;
	     ph = LIST_NEXT(ph, ph_hashlist)) {
		if (ph->ph_page == page)
			return (ph);
	}
	return (NULL);
}

/*
 * Remove a page from the pool.
 */
static __inline__ void
pr_rmpage(pp, ph)
	struct pool *pp;
	struct pool_item_header *ph;
{

	/*
	 * If the page was idle, decrement the idle page count.
	 */
	if (ph->ph_nmissing == 0) {
#ifdef DIAGNOSTIC
		if (pp->pr_nidle == 0)
			panic("pr_rmpage: nidle inconsistent");
#endif
		pp->pr_nidle--;
	}

	/*
	 * Unlink a page from the pool and release it.
	 */
	TAILQ_REMOVE(&pp->pr_pagelist, ph, ph_pagelist);
	(*pp->pr_free)(ph->ph_page, pp->pr_pagesz, pp->pr_mtype);
	pp->pr_npages--;
	pp->pr_npagefree++;

	if ((pp->pr_flags & PR_PHINPAGE) == 0) {
		LIST_REMOVE(ph, ph_hashlist);
		pool_put(&phpool, ph);
	}

	if (pp->pr_curpage == ph) {
		/*
		 * Find a new non-empty page header, if any.
		 * Start search from the page head, to increase the
		 * chance for "high water" pages to be freed.
		 */
		for (ph = TAILQ_FIRST(&pp->pr_pagelist); ph != NULL;
		     ph = TAILQ_NEXT(ph, ph_pagelist))
			if (TAILQ_FIRST(&ph->ph_itemlist) != NULL)
				break;

		pp->pr_curpage = ph;
	}
}

/*
 * Allocate and initialize a pool.
 */
struct pool *
pool_create(size, align, ioff, nitems, wchan, pagesz, alloc, release, mtype)
	size_t	size;
	u_int	align;
	u_int	ioff;
	int	nitems;
	char	*wchan;
	size_t	pagesz;
	void	*(*alloc) __P((unsigned long, int, int));
	void	(*release) __P((void *, unsigned long, int));
	int	mtype;
{
	struct pool *pp;
	int flags;

	pp = (struct pool *)malloc(sizeof(*pp), M_POOL, M_NOWAIT);
	if (pp == NULL)
		return (NULL);

	flags = PR_FREEHEADER;
#ifdef POOL_DIAGNOSTIC
	if (pool_logsize != 0)
		flags |= PR_LOGGING;
#endif

	pool_init(pp, size, align, ioff, flags, wchan, pagesz,
		  alloc, release, mtype);

	if (nitems != 0) {
		if (pool_prime(pp, nitems, NULL) != 0) {
			pool_destroy(pp);
			return (NULL);
		}
	}

	return (pp);
}

/*
 * Initialize the given pool resource structure.
 *
 * We export this routine to allow other kernel parts to declare
 * static pools that must be initialized before malloc() is available.
 */
void
pool_init(pp, size, align, ioff, flags, wchan, pagesz, alloc, release, mtype)
	struct pool	*pp;
	size_t		size;
	u_int		align;
	u_int		ioff;
	int		flags;
	char		*wchan;
	size_t		pagesz;
	void		*(*alloc) __P((unsigned long, int, int));
	void		(*release) __P((void *, unsigned long, int));
	int		mtype;
{
	int off, slack, i;

	/*
	 * Check arguments and construct default values.
	 */
	if (!powerof2(pagesz) || pagesz > PAGE_SIZE)
		panic("pool_init: page size invalid (%lx)\n", (u_long)pagesz);

	if (alloc == NULL && release == NULL) {
		alloc = pool_page_alloc;
		release = pool_page_free;
		pagesz = PAGE_SIZE;	/* Rounds to PAGE_SIZE anyhow. */
	} else if ((alloc != NULL && release != NULL) == 0) {
		/* If you specifiy one, must specify both. */
		panic("pool_init: must specify alloc and release together");
	}
			
	if (pagesz == 0)
		pagesz = PAGE_SIZE;

	if (align == 0)
		align = ALIGN(1);

	if (size < sizeof(struct pool_item))
		size = sizeof(struct pool_item);

	/*
	 * Initialize the pool structure.
	 */
	TAILQ_INSERT_TAIL(&pool_head, pp, pr_poollist);
	TAILQ_INIT(&pp->pr_pagelist);
	pp->pr_curpage = NULL;
	pp->pr_npages = 0;
	pp->pr_minitems = 0;
	pp->pr_minpages = 0;
	pp->pr_maxpages = UINT_MAX;
	pp->pr_flags = flags;
	pp->pr_size = ALIGN(size);
	pp->pr_align = align;
	pp->pr_wchan = wchan;
	pp->pr_mtype = mtype;
	pp->pr_alloc = alloc;
	pp->pr_free = release;
	pp->pr_pagesz = pagesz;
	pp->pr_pagemask = ~(pagesz - 1);
	pp->pr_pageshift = ffs(pagesz) - 1;

	/*
	 * Decide whether to put the page header off page to avoid
	 * wasting too large a part of the page. Off-page page headers
	 * go on a hash table, so we can match a returned item
	 * with its header based on the page address.
	 * We use 1/16 of the page size as the threshold (XXX: tune)
	 */
	if (pp->pr_size < pagesz/16) {
		/* Use the end of the page for the page header */
		pp->pr_flags |= PR_PHINPAGE;
		pp->pr_phoffset = off =
			pagesz - ALIGN(sizeof(struct pool_item_header));
	} else {
		/* The page header will be taken from our page header pool */
		pp->pr_phoffset = 0;
		off = pagesz;
		for (i = 0; i < PR_HASHTABSIZE; i++) {
			LIST_INIT(&pp->pr_hashtab[i]);
		}
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

#ifdef POOL_DIAGNOSTIC
	if ((flags & PR_LOGGING) != 0) {
		pp->pr_log = malloc(pool_logsize * sizeof(struct pool_log),
				    M_TEMP, M_NOWAIT);
		if (pp->pr_log == NULL)
			pp->pr_flags &= ~PR_LOGGING;
		pp->pr_curlogentry = 0;
		pp->pr_logsize = pool_logsize;
	}
#endif

	simple_lock_init(&pp->pr_lock);
	lockinit(&pp->pr_resourcelock, PSWP, wchan, 0, 0);

	/*
	 * Initialize private page header pool if we haven't done so yet.
	 */
	if (phpool.pr_size == 0) {
		pool_init(&phpool, sizeof(struct pool_item_header), 0, 0,
			  0, "phpool", 0, 0, 0, 0);
	}

	return;
}

/*
 * De-commision a pool resource.
 */
void
pool_destroy(pp)
	struct pool *pp;
{
	struct pool_item_header *ph;

#ifdef DIAGNOSTIC
	if (pp->pr_nget - pp->pr_nput != 0) {
		pr_printlog(pp);
		panic("pool_destroy: pool busy: still out: %lu\n",
		      pp->pr_nget - pp->pr_nput);
	}
#endif

	/* Remove all pages */
	if ((pp->pr_flags & PR_STATIC) == 0)
		while ((ph = pp->pr_pagelist.tqh_first) != NULL)
			pr_rmpage(pp, ph);

	/* Remove from global pool list */
	TAILQ_REMOVE(&pool_head, pp, pr_poollist);
	drainpp = NULL;

#ifdef POOL_DIAGNOSTIC
	if ((pp->pr_flags & PR_LOGGING) != 0)
		free(pp->pr_log, M_TEMP);
#endif

	if (pp->pr_flags & PR_FREEHEADER)
		free(pp, M_POOL);
}


/*
 * Grab an item from the pool; must be called at appropriate spl level
 */
#ifdef POOL_DIAGNOSTIC
void *
_pool_get(pp, flags, file, line)
	struct pool *pp;
	int flags;
	const char *file;
	long line;
#else
void *
pool_get(pp, flags)
	struct pool *pp;
	int flags;
#endif
{
	void *v;
	struct pool_item *pi;
	struct pool_item_header *ph;

#ifdef DIAGNOSTIC
	if ((pp->pr_flags & PR_STATIC) && (flags & PR_MALLOCOK)) {
		pr_printlog(pp);
		panic("pool_get: static");
	}
#endif

	simple_lock(&pp->pr_lock);
	if (curproc == NULL && (flags & PR_WAITOK) != 0)
		panic("pool_get: must have NOWAIT");

	/*
	 * The convention we use is that if `curpage' is not NULL, then
	 * it points at a non-empty bucket. In particular, `curpage'
	 * never points at a page header which has PR_PHINPAGE set and
	 * has no items in its bucket.
	 */
	while ((ph = pp->pr_curpage) == NULL) {
		void *v;
		int lkflags = LK_EXCLUSIVE | LK_INTERLOCK |
			      ((flags & PR_WAITOK) == 0 ? LK_NOWAIT : 0);

		/* Get long-term lock on pool */
		if (lockmgr(&pp->pr_resourcelock, lkflags, &pp->pr_lock,
			    curproc /*XXX*/) != 0)
			return (NULL);

		/* Check if pool became non-empty while we slept */
		if ((ph = pp->pr_curpage) != NULL)
			goto again;

		/* Call the page back-end allocator for more memory */
		v = (*pp->pr_alloc)(pp->pr_pagesz, flags, pp->pr_mtype);
		if (v == NULL) {
			if (flags & PR_URGENT)
				panic("pool_get: urgent");
			if ((flags & PR_WAITOK) == 0) {
				pp->pr_nfail++;
				lockmgr(&pp->pr_resourcelock, LK_RELEASE, NULL,
					curproc/*XXX*/);
				return (NULL);
			}

			/*
			 * Wait for items to be returned to this pool.
			 * XXX: we actually want to wait just until
			 * the page allocator has memory again. Depending
			 * on this pool's usage, we might get stuck here
			 * for a long time.
			 */
			pp->pr_flags |= PR_WANTED;
			lockmgr(&pp->pr_resourcelock, LK_RELEASE, NULL,
				curproc/*XXX*/);
			tsleep((caddr_t)pp, PSWP, pp->pr_wchan, 0);
			simple_lock(&pp->pr_lock);
			continue;
		}

		/* We have more memory; add it to the pool */
		pp->pr_npagealloc++;
		pool_prime_page(pp, v);

again:
		/* Re-acquire pool interlock */
		simple_lock(&pp->pr_lock);
		lockmgr(&pp->pr_resourcelock, LK_RELEASE, NULL, curproc/*XXX*/);
	}

	if ((v = pi = TAILQ_FIRST(&ph->ph_itemlist)) == NULL)
		panic("pool_get: %s: page empty", pp->pr_wchan);

	pr_log(pp, v, PRLOG_GET, file, line);

#ifdef DIAGNOSTIC
	if (pi->pi_magic != PI_MAGIC) {
		pr_printlog(pp);
		panic("pool_get(%s): free list modified: magic=%x; page %p;"
		       " item addr %p\n",
			pp->pr_wchan, pi->pi_magic, ph->ph_page, pi);
	}
#endif

	/*
	 * Remove from item list.
	 */
	TAILQ_REMOVE(&ph->ph_itemlist, pi, pi_list);
	if (ph->ph_nmissing == 0) {
#ifdef DIAGNOSTIC
		if (pp->pr_nidle == 0)
			panic("pool_get: nidle inconsistent");
#endif
		pp->pr_nidle--;
	}
	ph->ph_nmissing++;
	if (TAILQ_FIRST(&ph->ph_itemlist) == NULL) {
		/*
		 * Find a new non-empty page header, if any.
		 * Start search from the page head, to increase
		 * the chance for "high water" pages to be freed.
		 *
		 * First, move the now empty page to the head of
		 * the page list.
		 */
		TAILQ_REMOVE(&pp->pr_pagelist, ph, ph_pagelist);
		TAILQ_INSERT_HEAD(&pp->pr_pagelist, ph, ph_pagelist);
		while ((ph = TAILQ_NEXT(ph, ph_pagelist)) != NULL)
			if (TAILQ_FIRST(&ph->ph_itemlist) != NULL)
				break;

		pp->pr_curpage = ph;
	}

	pp->pr_nget++;
	simple_unlock(&pp->pr_lock);
	return (v);
}

/*
 * Return resource to the pool; must be called at appropriate spl level
 */
#ifdef POOL_DIAGNOSTIC
void
_pool_put(pp, v, file, line)
	struct pool *pp;
	void *v;
	const char *file;
	long line;
#else
void
pool_put(pp, v)
	struct pool *pp;
	void *v;
#endif
{
	struct pool_item *pi = v;
	struct pool_item_header *ph;
	caddr_t page;

	page = (caddr_t)((u_long)v & pp->pr_pagemask);

	simple_lock(&pp->pr_lock);

	pr_log(pp, v, PRLOG_PUT, file, line);

	if ((ph = pr_find_pagehead(pp, page)) == NULL) {
		pr_printlog(pp);
		panic("pool_put: %s: page header missing", pp->pr_wchan);
	}

	/*
	 * Return to item list.
	 */
#ifdef DIAGNOSTIC
	pi->pi_magic = PI_MAGIC;
#endif
	TAILQ_INSERT_HEAD(&ph->ph_itemlist, pi, pi_list);
	ph->ph_nmissing--;
	pp->pr_nput++;

	/* Cancel "pool empty" condition if it exists */
	if (pp->pr_curpage == NULL)
		pp->pr_curpage = ph;

	if (pp->pr_flags & PR_WANTED) {
		pp->pr_flags &= ~PR_WANTED;
		if (ph->ph_nmissing == 0)
			pp->pr_nidle++;
		wakeup((caddr_t)pp);
		simple_unlock(&pp->pr_lock);
		return;
	}

	/*
	 * If this page is now complete, move it to the end of the pagelist.
	 * If this page has just become un-empty, move it the head.
	 */
	if (ph->ph_nmissing == 0) {
		pp->pr_nidle++;
		if (pp->pr_npages > pp->pr_maxpages) {
#if 0
			timeout(pool_drain, 0, pool_inactive_time*hz);
#else
			pr_rmpage(pp, ph);
#endif
		} else {
			TAILQ_REMOVE(&pp->pr_pagelist, ph, ph_pagelist);
			TAILQ_INSERT_TAIL(&pp->pr_pagelist, ph, ph_pagelist);
			ph->ph_time = time;

			/* XXX - update curpage */
			for (ph = TAILQ_FIRST(&pp->pr_pagelist); ph != NULL;
			     ph = TAILQ_NEXT(ph, ph_pagelist))
				if (TAILQ_FIRST(&ph->ph_itemlist) != NULL)
					break;

			pp->pr_curpage = ph;
		}
	}

	simple_unlock(&pp->pr_lock);
}

/*
 * Add N items to the pool.
 */
int
pool_prime(pp, n, storage)
	struct pool *pp;
	int n;
	caddr_t storage;
{
	caddr_t cp;
	int newnitems, newpages;

#ifdef DIAGNOSTIC
	if (storage && !(pp->pr_flags & PR_STATIC))
		panic("pool_prime: static");
	/* !storage && static caught below */
#endif

	(void)lockmgr(&pp->pr_resourcelock, LK_EXCLUSIVE, NULL, curproc/*XXX*/);
	newnitems = pp->pr_minitems + n;
	newpages =
		roundup(pp->pr_itemsperpage,newnitems) / pp->pr_itemsperpage
		- pp->pr_minpages;

	while (newpages-- > 0) {

		if (pp->pr_flags & PR_STATIC) {
			cp = storage;
			storage += pp->pr_pagesz;
		} else {
			cp = (*pp->pr_alloc)(pp->pr_pagesz, 0, pp->pr_mtype);
		}

		if (cp == NULL) {
			(void)lockmgr(&pp->pr_resourcelock, LK_RELEASE, NULL,
				      curproc/*XXX*/);
			return (ENOMEM);
		}

		pool_prime_page(pp, cp);
		pp->pr_minpages++;
	}

	pp->pr_minitems = newnitems;

	if (pp->pr_minpages >= pp->pr_maxpages)
		pp->pr_maxpages = pp->pr_minpages + 1;	/* XXX */

	(void)lockmgr(&pp->pr_resourcelock, LK_RELEASE, NULL, curproc/*XXX*/);
	return (0);
}

/*
 * Add a page worth of items to the pool.
 */
int
pool_prime_page(pp, storage)
	struct pool *pp;
	caddr_t storage;
{
	struct pool_item *pi;
	struct pool_item_header *ph;
	caddr_t cp = storage;
	unsigned int align = pp->pr_align;
	unsigned int ioff = pp->pr_itemoffset;
	int n;

	simple_lock(&pp->pr_lock);

	if ((pp->pr_flags & PR_PHINPAGE) != 0) {
		ph = (struct pool_item_header *)(cp + pp->pr_phoffset);
	} else {
		ph = pool_get(&phpool, PR_URGENT);
		LIST_INSERT_HEAD(&pp->pr_hashtab[PR_HASH_INDEX(pp, cp)],
				 ph, ph_hashlist);
	}

	/*
	 * Insert page header.
	 */
	TAILQ_INSERT_HEAD(&pp->pr_pagelist, ph, ph_pagelist);
	TAILQ_INIT(&ph->ph_itemlist);
	ph->ph_page = storage;
	ph->ph_nmissing = 0;
	ph->ph_time.tv_sec = ph->ph_time.tv_usec = 0;

	pp->pr_nidle++;

	/*
	 * Color this page.
	 */
	cp = (caddr_t)(cp + pp->pr_curcolor);
	if ((pp->pr_curcolor += align) > pp->pr_maxcolor)
		pp->pr_curcolor = 0;

	/*
	 * Adjust storage to apply aligment to `pr_itemoffset' in each item.
	 */
	if (ioff != 0)
		cp = (caddr_t)(cp + (align - ioff));

	/*
	 * Insert remaining chunks on the bucket list.
	 */
	n = pp->pr_itemsperpage;

	while (n--) {
		pi = (struct pool_item *)cp;

		/* Insert on page list */
		TAILQ_INSERT_TAIL(&ph->ph_itemlist, pi, pi_list);
#ifdef DIAGNOSTIC
		pi->pi_magic = PI_MAGIC;
#endif
		cp = (caddr_t)(cp + pp->pr_size);
	}

	/*
	 * If the pool was depleted, point at the new page.
	 */
	if (pp->pr_curpage == NULL)
		pp->pr_curpage = ph;

	if (++pp->pr_npages > pp->pr_hiwat)
		pp->pr_hiwat = pp->pr_npages;

	simple_unlock(&pp->pr_lock);
	return (0);
}

void
pool_setlowat(pp, n)
	pool_handle_t	pp;
	int n;
{

	(void)lockmgr(&pp->pr_resourcelock, LK_EXCLUSIVE, NULL, curproc/*XXX*/);
	pp->pr_minitems = n;
	pp->pr_minpages = (n == 0)
		? 0
		: roundup(pp->pr_itemsperpage,n) / pp->pr_itemsperpage;
	(void)lockmgr(&pp->pr_resourcelock, LK_RELEASE, NULL, curproc/*XXX*/);
}

void
pool_sethiwat(pp, n)
	pool_handle_t	pp;
	int n;
{

	(void)lockmgr(&pp->pr_resourcelock, LK_EXCLUSIVE, NULL, curproc/*XXX*/);
	pp->pr_maxpages = (n == 0)
		? 0
		: roundup(pp->pr_itemsperpage,n) / pp->pr_itemsperpage;
	(void)lockmgr(&pp->pr_resourcelock, LK_RELEASE, NULL, curproc/*XXX*/);
}


/*
 * Default page allocator.
 */
static void *
pool_page_alloc(sz, flags, mtype)
	unsigned long sz;
	int flags;
	int mtype;
{
	boolean_t waitok = (flags & PR_WAITOK) ? TRUE : FALSE;

#if defined(UVM)
	return ((void *)uvm_km_alloc_poolpage(waitok));
#else
	return ((void *)kmem_alloc_poolpage(waitok));
#endif
}

static void
pool_page_free(v, sz, mtype)
	void *v;
	unsigned long sz;
	int mtype;
{

#if defined(UVM)
	uvm_km_free_poolpage((vaddr_t)v);
#else
	kmem_free_poolpage((vaddr_t)v);
#endif  
}

/*
 * Alternate pool page allocator for pools that know they will
 * never be accessed in interrupt context.
 */
void *
pool_page_alloc_nointr(sz, flags, mtype)
	unsigned long sz;
	int flags;
	int mtype;
{
#if defined(UVM)
	boolean_t waitok = (flags & PR_WAITOK) ? TRUE : FALSE;

	/*
	 * With UVM, we can use the kernel_map.
	 */
	return ((void *)uvm_km_alloc_poolpage1(kernel_map, uvm.kernel_object,
	    waitok));
#else
	/*
	 * Can't do anything so cool with Mach VM.
	 */
	return (pool_page_alloc(sz, flags, mtype));
#endif
}

void
pool_page_free_nointr(v, sz, mtype)
	void *v;
	unsigned long sz;
	int mtype;
{

#if defined(UVM)
	uvm_km_free_poolpage1(kernel_map, (vaddr_t)v);
#else
	pool_page_free(v, sz, mtype);
#endif  
}


/*
 * Release all complete pages that have not been used recently.
 */
void
pool_reclaim (pp)
	pool_handle_t pp;
{
	struct pool_item_header *ph, *phnext;
	struct timeval curtime = time;

	if (pp->pr_flags & PR_STATIC)
		return;

	if (simple_lock_try(&pp->pr_lock) == 0)
		return;

	for (ph = TAILQ_FIRST(&pp->pr_pagelist); ph != NULL; ph = phnext) {
		phnext = TAILQ_NEXT(ph, ph_pagelist);

		/* Check our minimum page claim */
		if (pp->pr_npages <= pp->pr_minpages)
			break;

		if (ph->ph_nmissing == 0) {
			struct timeval diff;
			timersub(&curtime, &ph->ph_time, &diff);
			if (diff.tv_sec < pool_inactive_time)
				continue;
			pr_rmpage(pp, ph);
		}
	}

	simple_unlock(&pp->pr_lock);
}


/*
 * Drain pools, one at a time.
 */
void
pool_drain(arg)
	void *arg;
{
	struct pool *pp;
	int s = splimp();

	/* XXX:lock pool head */
	if (drainpp == NULL && (drainpp = TAILQ_FIRST(&pool_head)) == NULL) {
		splx(s);
		return;
	}

	pp = drainpp;
	drainpp = TAILQ_NEXT(pp, pr_poollist);
	/* XXX:unlock pool head */

	pool_reclaim(pp);
	splx(s);
}


#if defined(POOL_DIAGNOSTIC) || defined(DEBUG)
/*
 * Diagnostic helpers.
 */
void
pool_print(pp, label)
	struct pool *pp;
	char *label;
{

	if (label != NULL)
		printf("%s: ", label);

	printf("pool %s: nalloc %lu nfree %lu npagealloc %lu npagefree %lu\n"
	       "         npages %u minitems %u itemsperpage %u itemoffset %u\n"
	       "         nidle %lu\n",
		pp->pr_wchan,
		pp->pr_nget,
		pp->pr_nput,
		pp->pr_npagealloc,
		pp->pr_npagefree,
		pp->pr_npages,
		pp->pr_minitems,
		pp->pr_itemsperpage,
		pp->pr_itemoffset,
		pp->pr_nidle);
}

int
pool_chk(pp, label)
	struct pool *pp;
	char *label;
{
	struct pool_item_header *ph;
	int r = 0;

	simple_lock(&pp->pr_lock);

	for (ph = TAILQ_FIRST(&pp->pr_pagelist); ph != NULL;
	     ph = TAILQ_NEXT(ph, ph_pagelist)) {

		struct pool_item *pi;
		int n;
		caddr_t page;

		page = (caddr_t)((u_long)ph & pp->pr_pagemask);
		if (page != ph->ph_page && (pp->pr_flags & PR_PHINPAGE) != 0) {
			if (label != NULL)
				printf("%s: ", label);
			printf("pool(%p:%s): page inconsistency: page %p;"
			       " at page head addr %p (p %p)\n", pp,
				pp->pr_wchan, ph->ph_page,
				ph, page);
			r++;
			goto out;
		}

		for (pi = TAILQ_FIRST(&ph->ph_itemlist), n = 0;
		     pi != NULL;
		     pi = TAILQ_NEXT(pi,pi_list), n++) {

#ifdef DIAGNOSTIC
			if (pi->pi_magic != PI_MAGIC) {
				if (label != NULL)
					printf("%s: ", label);
				printf("pool(%s): free list modified: magic=%x;"
				       " page %p; item ordinal %d;"
				       " addr %p (p %p)\n",
					pp->pr_wchan, pi->pi_magic, ph->ph_page,
					n, pi, page);
				panic("pool");
			}
#endif
			page = (caddr_t)((u_long)pi & pp->pr_pagemask);
			if (page == ph->ph_page)
				continue;

			if (label != NULL)
				printf("%s: ", label);
			printf("pool(%p:%s): page inconsistency: page %p;"
			       " item ordinal %d; addr %p (p %p)\n", pp,
				pp->pr_wchan, ph->ph_page,
				n, pi, page);
			r++;
			goto out;
		}
	}
out:
	simple_unlock(&pp->pr_lock);
	return (r);
}
#endif /* POOL_DIAGNOSTIC || DEBUG */
