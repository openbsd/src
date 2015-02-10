/*	$OpenBSD: ttm_page_alloc.c,v 1.7 2015/02/10 10:50:49 jsg Exp $	*/
/*
 * Copyright (c) Red Hat Inc.

 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors: Dave Airlie <airlied@redhat.com>
 *          Jerome Glisse <jglisse@redhat.com>
 *          Pauli Nieminen <suokkos@gmail.com>
 */

/* simple list based uncached page pool
 * - Pool collects resently freed pages for reuse
 * - Use page->lru to keep a free list
 * - doesn't track currently in use pages
 */

#define pr_fmt(fmt) "[TTM] " fmt

#include <dev/pci/drm/drmP.h>
#include <dev/pci/drm/refcount.h>
#include <dev/pci/drm/ttm/ttm_bo_driver.h>
#include <dev/pci/drm/ttm/ttm_page_alloc.h>

#ifdef TTM_HAS_AGP
#include <dev/pci/agpvar.h>
#endif

#include <uvm/uvm_extern.h>

#define NUM_PAGES_TO_ALLOC		(PAGE_SIZE/sizeof(struct vm_page *))
#define SMALL_ALLOCATION		16
#define FREE_ALL_PAGES			(~0U)
/* times are in msecs */
#define PAGE_FREE_INTERVAL		1000

/**
 * struct ttm_page_pool - Pool to reuse recently allocated uc/wc pages.
 *
 * @lock: Protects the shared pool from concurrnet access. Must be used with
 * irqsave/irqrestore variants because pool allocator maybe called from
 * delayed work.
 * @fill_lock: Prevent concurrent calls to fill.
 * @list: Pool of free uc/wc pages for fast reuse.
 * @gfp_flags: Flags to pass for alloc_page.
 * @npages: Number of pages in pool.
 */
struct ttm_page_pool {
	struct mutex		lock;
	bool			fill_lock;
	struct pglist		list;
	int			ttm_page_alloc_flags;
	unsigned		npages;
	char			*name;
	unsigned long		nfrees;
	unsigned long		nrefills;
};

/**
 * Limits for the pool. They are handled without locks because only place where
 * they may change is in sysfs store. They won't have immediate effect anyway
 * so forcing serialization to access them is pointless.
 */

struct ttm_pool_opts {
	unsigned	alloc_size;
	unsigned	max_size;
	unsigned	small;
};

#define NUM_POOLS 4

/**
 * struct ttm_pool_manager - Holds memory pools for fst allocation
 *
 * Manager is read only object for pool code so it doesn't need locking.
 *
 * @free_interval: minimum number of jiffies between freeing pages from pool.
 * @page_alloc_inited: reference counting for pool allocation.
 * @work: Work that is used to shrink the pool. Work is only run when there is
 * some pages to free.
 * @small_allocation: Limit in number of pages what is small allocation.
 *
 * @pools: All pool objects in use.
 **/
struct ttm_pool_manager {
	unsigned int		kobj_ref;
#ifdef notyet
	struct shrinker		mm_shrink;
#endif
	struct ttm_pool_opts	options;

	union {
		struct ttm_page_pool	pools[NUM_POOLS];
		struct {
			struct ttm_page_pool	wc_pool;
			struct ttm_page_pool	uc_pool;
			struct ttm_page_pool	wc_pool_dma32;
			struct ttm_page_pool	uc_pool_dma32;
		} ;
	};
};

#ifdef notyet
static struct attribute ttm_page_pool_max = {
	.name = "pool_max_size",
	.mode = S_IRUGO | S_IWUSR
};
static struct attribute ttm_page_pool_small = {
	.name = "pool_small_allocation",
	.mode = S_IRUGO | S_IWUSR
};
static struct attribute ttm_page_pool_alloc_size = {
	.name = "pool_allocation_size",
	.mode = S_IRUGO | S_IWUSR
};

static struct attribute *ttm_pool_attrs[] = {
	&ttm_page_pool_max,
	&ttm_page_pool_small,
	&ttm_page_pool_alloc_size,
	NULL
};
#endif

struct vm_page *ttm_uvm_alloc_page(void);
void	 ttm_uvm_free_page(struct vm_page *);

struct vm_page *
ttm_uvm_alloc_page(void)
{
	struct pglist mlist;
	int error;

	TAILQ_INIT(&mlist);
	error = uvm_pglistalloc(PAGE_SIZE, dma_constraint.ucr_low,
				dma_constraint.ucr_high, 0, 0, &mlist,
				1, UVM_PLA_WAITOK | UVM_PLA_ZERO);
	if (error)
		return NULL;

	return TAILQ_FIRST(&mlist);
}

void
ttm_uvm_free_page(struct vm_page *m)
{
#ifdef notyet
	KASSERT(m->uobject == NULL);
	KASSERT(m->wire_count == 1);
	KASSERT((m->pg_flags & PG_FAKE) != 0);
#endif
	uvm_pagefree(m);
}

static void ttm_pool_kobj_release(struct ttm_pool_manager *m)
{
	kfree(m);
}

#ifdef notyet
static ssize_t ttm_pool_store(struct kobject *kobj,
		struct attribute *attr, const char *buffer, size_t size)
{
	struct ttm_pool_manager *m =
		container_of(kobj, struct ttm_pool_manager, kobj);
	int chars;
	unsigned val;
	chars = sscanf(buffer, "%u", &val);
	if (chars == 0)
		return size;

	/* Convert kb to number of pages */
	val = val / (PAGE_SIZE >> 10);

	if (attr == &ttm_page_pool_max)
		m->options.max_size = val;
	else if (attr == &ttm_page_pool_small)
		m->options.small = val;
	else if (attr == &ttm_page_pool_alloc_size) {
		if (val > NUM_PAGES_TO_ALLOC*8) {
			pr_err("Setting allocation size to %lu is not allowed. Recommended size is %lu\n",
			       NUM_PAGES_TO_ALLOC*(PAGE_SIZE >> 7),
			       NUM_PAGES_TO_ALLOC*(PAGE_SIZE >> 10));
			return size;
		} else if (val > NUM_PAGES_TO_ALLOC) {
			pr_warn("Setting allocation size to larger than %lu is not recommended\n",
				NUM_PAGES_TO_ALLOC*(PAGE_SIZE >> 10));
		}
		m->options.alloc_size = val;
	}

	return size;
}

static ssize_t ttm_pool_show(struct kobject *kobj,
		struct attribute *attr, char *buffer)
{
	struct ttm_pool_manager *m =
		container_of(kobj, struct ttm_pool_manager, kobj);
	unsigned val = 0;

	if (attr == &ttm_page_pool_max)
		val = m->options.max_size;
	else if (attr == &ttm_page_pool_small)
		val = m->options.small;
	else if (attr == &ttm_page_pool_alloc_size)
		val = m->options.alloc_size;

	val = val * (PAGE_SIZE >> 10);

	return snprintf(buffer, PAGE_SIZE, "%u\n", val);
}

static const struct sysfs_ops ttm_pool_sysfs_ops = {
	.show = &ttm_pool_show,
	.store = &ttm_pool_store,
};

static struct kobj_type ttm_pool_kobj_type = {
	.release = &ttm_pool_kobj_release,
	.sysfs_ops = &ttm_pool_sysfs_ops,
	.default_attrs = ttm_pool_attrs,
};
#endif // notyet

static struct ttm_pool_manager *_manager;

static int set_pages_array_wb(struct vm_page **pages, int addrinarray)
{
#ifdef TTM_HAS_AGP
#if defined(__amd64__) || defined(__i386__)
	int i;

	for (i = 0; i < addrinarray; i++)
		atomic_clearbits_int(&pages[i]->pg_flags, PG_PMAP_WC);
#else
	printf("%s stub\n", __func__);
	return -ENOSYS;
#endif
#endif
	return 0;
}

static int set_pages_array_wc(struct vm_page **pages, int addrinarray)
{
#ifdef TTM_HAS_AGP
#if defined(__amd64__) || defined(__i386__)
	int i;

	for (i = 0; i < addrinarray; i++)
		atomic_setbits_int(&pages[i]->pg_flags, PG_PMAP_WC);
#else
	printf("%s stub\n", __func__);
	return -ENOSYS;
#endif
#endif
	return 0;
}

static int set_pages_array_uc(struct vm_page **pages, int addrinarray)
{
#ifdef TTM_HAS_AGP
	printf("%s stub\n", __func__);
	return -ENOSYS;
#ifdef notyet
	int i;

	for (i = 0; i < addrinarray; i++)
		map_page_into_agp(pages[i]);
#endif
#endif
	return 0;
}

/**
 * Select the right pool or requested caching state and ttm flags. */
static struct ttm_page_pool *ttm_get_pool(int flags,
		enum ttm_caching_state cstate)
{
	int pool_index;

	if (cstate == tt_cached)
		return NULL;

	if (cstate == tt_wc)
		pool_index = 0x0;
	else
		pool_index = 0x1;

	if (flags & TTM_PAGE_FLAG_DMA32)
		pool_index |= 0x2;

	return &_manager->pools[pool_index];
}

/* set memory back to wb and free the pages. */
static void ttm_pages_put(struct vm_page *pages[], unsigned npages)
{
	unsigned i;
	if (set_pages_array_wb(pages, npages))
		printf("Failed to set %d pages to wb!\n", npages);
	for (i = 0; i < npages; ++i)
		ttm_uvm_free_page(pages[i]);
}

static void ttm_pool_update_free_locked(struct ttm_page_pool *pool,
		unsigned freed_pages)
{
	pool->npages -= freed_pages;
	pool->nfrees += freed_pages;
}

/**
 * Free pages from pool.
 *
 * To prevent hogging the ttm_swap process we only free NUM_PAGES_TO_ALLOC
 * number of pages in one go.
 *
 * @pool: to free the pages from
 * @free_all: If set to true will free all pages in pool
 **/
static int ttm_page_pool_free(struct ttm_page_pool *pool, unsigned nr_free)
{
	unsigned long irq_flags;
	struct vm_page *p, *p1;
	struct vm_page **pages_to_free;
	unsigned freed_pages = 0,
		 npages_to_free = nr_free;
	unsigned i;

	if (NUM_PAGES_TO_ALLOC < nr_free)
		npages_to_free = NUM_PAGES_TO_ALLOC;

	pages_to_free = kmalloc(npages_to_free * sizeof(struct vm_page *),
			GFP_KERNEL);
	if (!pages_to_free) {
		printf("Failed to allocate memory for pool free operation\n");
		return 0;
	}

restart:
	spin_lock_irqsave(&pool->lock, irq_flags);

	TAILQ_FOREACH_REVERSE_SAFE(p, &pool->list, pglist, pageq, p1) {
		if (freed_pages >= npages_to_free)
			break;

		pages_to_free[freed_pages++] = p;
		/* We can only remove NUM_PAGES_TO_ALLOC at a time. */
		if (freed_pages >= NUM_PAGES_TO_ALLOC) {
			/* remove range of pages from the pool */
			for (i = 0; i < freed_pages; i++)
				TAILQ_REMOVE(&pool->list, pages_to_free[i], pageq);

			ttm_pool_update_free_locked(pool, freed_pages);
			/**
			 * Because changing page caching is costly
			 * we unlock the pool to prevent stalling.
			 */
			spin_unlock_irqrestore(&pool->lock, irq_flags);

			ttm_pages_put(pages_to_free, freed_pages);
			if (likely(nr_free != FREE_ALL_PAGES))
				nr_free -= freed_pages;

			if (NUM_PAGES_TO_ALLOC >= nr_free)
				npages_to_free = nr_free;
			else
				npages_to_free = NUM_PAGES_TO_ALLOC;

			freed_pages = 0;

			/* free all so restart the processing */
			if (nr_free)
				goto restart;

			/* Not allowed to fall through or break because
			 * following context is inside spinlock while we are
			 * outside here.
			 */
			goto out;

		}
	}

	/* remove range of pages from the pool */
	if (freed_pages) {
		for (i = 0; i < freed_pages; i++)
			TAILQ_REMOVE(&pool->list, pages_to_free[i], pageq);

		ttm_pool_update_free_locked(pool, freed_pages);
		nr_free -= freed_pages;
	}

	spin_unlock_irqrestore(&pool->lock, irq_flags);

	if (freed_pages)
		ttm_pages_put(pages_to_free, freed_pages);
out:
	kfree(pages_to_free);
	return nr_free;
}

#ifdef notyet
/* Get good estimation how many pages are free in pools */
static int ttm_pool_get_num_unused_pages(void)
{
	unsigned i;
	int total = 0;
	for (i = 0; i < NUM_POOLS; ++i)
		total += _manager->pools[i].npages;

	return total;
}
#endif

/**
 * Callback for mm to request pool to reduce number of page held.
 */
#ifdef notyet
static int ttm_pool_mm_shrink(struct shrinker *shrink,
			      struct shrink_control *sc)
{
	static atomic_t start_pool = ATOMIC_INIT(0);
	unsigned i;
	unsigned pool_offset = atomic_add_return(1, &start_pool);
	struct ttm_page_pool *pool;
	int shrink_pages = sc->nr_to_scan;

	pool_offset = pool_offset % NUM_POOLS;
	/* select start pool in round robin fashion */
	for (i = 0; i < NUM_POOLS; ++i) {
		unsigned nr_free = shrink_pages;
		if (shrink_pages == 0)
			break;
		pool = &_manager->pools[(i + pool_offset)%NUM_POOLS];
		shrink_pages = ttm_page_pool_free(pool, nr_free);
	}
	/* return estimated number of unused pages in pool */
	return ttm_pool_get_num_unused_pages();
}
#endif

static void ttm_pool_mm_shrink_init(struct ttm_pool_manager *manager)
{
#ifdef notyet
	manager->mm_shrink.shrink = &ttm_pool_mm_shrink;
	manager->mm_shrink.seeks = 1;
	register_shrinker(&manager->mm_shrink);
#endif
}

static void ttm_pool_mm_shrink_fini(struct ttm_pool_manager *manager)
{
	printf("%s stub\n", __func__);
#ifdef notyet
	unregister_shrinker(&manager->mm_shrink);
#endif
}

static int ttm_set_pages_caching(struct vm_page **pages,
		enum ttm_caching_state cstate, unsigned cpages)
{
	int r = 0;
	/* Set page caching */
	switch (cstate) {
	case tt_uncached:
		r = set_pages_array_uc(pages, cpages);
		if (r)
			printf("Failed to set %d pages to uc!\n", cpages);
		break;
	case tt_wc:
		r = set_pages_array_wc(pages, cpages);
		if (r)
			printf("Failed to set %d pages to wc!\n", cpages);
		break;
	default:
		break;
	}
	return r;
}

/**
 * Free pages the pages that failed to change the caching state. If there is
 * any pages that have changed their caching state already put them to the
 * pool.
 */
static void ttm_handle_caching_state_failure(struct pglist *pages,
		int ttm_flags, enum ttm_caching_state cstate,
		struct vm_page **failed_pages, unsigned cpages)
{
	unsigned i;
	/* Failed pages have to be freed */
	for (i = 0; i < cpages; ++i) {
		TAILQ_REMOVE(pages, failed_pages[i], pageq);
		ttm_uvm_free_page(failed_pages[i]);
	}
}

/**
 * Allocate new pages with correct caching.
 *
 * This function is reentrant if caller updates count depending on number of
 * pages returned in pages array.
 */
static int ttm_alloc_new_pages(struct pglist *pages, int gfp_flags,
		int ttm_flags, enum ttm_caching_state cstate, unsigned count)
{
	struct vm_page **caching_array;
	struct vm_page *p;
	int r = 0;
	unsigned i, cpages;
	unsigned max_cpages = min(count,
			(unsigned)(PAGE_SIZE/sizeof(struct vm_page *)));

	/* allocate array for page caching change */
	caching_array = kmalloc(max_cpages*sizeof(struct vm_page *), GFP_KERNEL);

	if (!caching_array) {
		printf("Unable to allocate table for new pages\n");
		return -ENOMEM;
	}

	for (i = 0, cpages = 0; i < count; ++i) {
		p = ttm_uvm_alloc_page();

		if (!p) {
			printf("Unable to get page %u\n", i);

			/* store already allocated pages in the pool after
			 * setting the caching state */
			if (cpages) {
				r = ttm_set_pages_caching(caching_array,
							  cstate, cpages);
				if (r)
					ttm_handle_caching_state_failure(pages,
						ttm_flags, cstate,
						caching_array, cpages);
			}
			r = -ENOMEM;
			goto out;
		}

#ifdef CONFIG_HIGHMEM
		/* gfp flags of highmem page should never be dma32 so we
		 * we should be fine in such case
		 */
		if (!PageHighMem(p))
#endif
		{
			caching_array[cpages++] = p;
			if (cpages == max_cpages) {

				r = ttm_set_pages_caching(caching_array,
						cstate, cpages);
				if (r) {
					ttm_handle_caching_state_failure(pages,
						ttm_flags, cstate,
						caching_array, cpages);
					goto out;
				}
				cpages = 0;
			}
		}

		TAILQ_INSERT_HEAD(pages, p, pageq);
	}

	if (cpages) {
		r = ttm_set_pages_caching(caching_array, cstate, cpages);
		if (r)
			ttm_handle_caching_state_failure(pages,
					ttm_flags, cstate,
					caching_array, cpages);
	}
out:
	kfree(caching_array);

	return r;
}

/**
 * Fill the given pool if there aren't enough pages and the requested number of
 * pages is small.
 */
static void ttm_page_pool_fill_locked(struct ttm_page_pool *pool,
		int ttm_flags, enum ttm_caching_state cstate, unsigned count,
		unsigned long *irq_flags)
{
	struct vm_page *p;
	int r;
	unsigned cpages = 0;
	/**
	 * Only allow one pool fill operation at a time.
	 * If pool doesn't have enough pages for the allocation new pages are
	 * allocated from outside of pool.
	 */
	if (pool->fill_lock)
		return;

	pool->fill_lock = true;

	/* If allocation request is small and there are not enough
	 * pages in a pool we fill the pool up first. */
	if (count < _manager->options.small
		&& count > pool->npages) {
		struct pglist new_pages;
		unsigned alloc_size = _manager->options.alloc_size;

		/**
		 * Can't change page caching if in irqsave context. We have to
		 * drop the pool->lock.
		 */
		spin_unlock_irqrestore(&pool->lock, *irq_flags);

		TAILQ_INIT(&new_pages);
		r = ttm_alloc_new_pages(&new_pages, pool->ttm_page_alloc_flags,
		    ttm_flags, cstate, alloc_size);
		spin_lock_irqsave(&pool->lock, *irq_flags);

		if (!r) {
			TAILQ_CONCAT(&pool->list, &new_pages, pageq);
			++pool->nrefills;
			pool->npages += alloc_size;
		} else {
			DRM_ERROR("Failed to fill pool (%p)\n", pool);
			/* If we have any pages left put them to the pool. */
			TAILQ_FOREACH(p, &pool->list, pageq) {
				++cpages;
			}
			TAILQ_CONCAT(&pool->list, &new_pages, pageq);
			pool->npages += cpages;
		}

	}
	pool->fill_lock = false;
}

/**
 * Cut 'count' number of pages from the pool and put them on the return list.
 *
 * @return count of pages still required to fulfill the request.
 */
static unsigned ttm_page_pool_get_pages(struct ttm_page_pool *pool,
					struct pglist *pages,
					int ttm_flags,
					enum ttm_caching_state cstate,
					unsigned count)
{
	unsigned long irq_flags;
	vm_page_t p;
	unsigned i;

	spin_lock_irqsave(&pool->lock, irq_flags);
	ttm_page_pool_fill_locked(pool, ttm_flags, cstate, count, &irq_flags);

	if (count >= pool->npages) {
		/* take all pages from the pool */
		TAILQ_CONCAT(pages, &pool->list, pageq);
		count -= pool->npages;
		pool->npages = 0;
		goto out;
	}
	for (i = 0; i < count; i++) {
		p = TAILQ_FIRST(&pool->list);
		TAILQ_REMOVE(&pool->list, p, pageq);
		TAILQ_INSERT_TAIL(pages, p, pageq);
	}
	pool->npages -= count;
	count = 0;
out:
	spin_unlock_irqrestore(&pool->lock, irq_flags);
	return count;
}

/* Put all pages in pages list to correct pool to wait for reuse */
static void ttm_put_pages(struct vm_page **pages, unsigned npages, int flags,
			  enum ttm_caching_state cstate)
{
	unsigned long irq_flags;
	struct ttm_page_pool *pool = ttm_get_pool(flags, cstate);
	unsigned i;

	if (pool == NULL) {
		/* No pool for this memory type so free the pages */
		for (i = 0; i < npages; i++) {
			if (pages[i]) {
				ttm_uvm_free_page(pages[i]);
				pages[i] = NULL;
			}
		}
		return;
	}

	spin_lock_irqsave(&pool->lock, irq_flags);
	for (i = 0; i < npages; i++) {
		if (pages[i]) {
			TAILQ_INSERT_TAIL(&pool->list, pages[i], pageq);
			pages[i] = NULL;
			pool->npages++;
		}
	}
	/* Check that we don't go over the pool limit */
	npages = 0;
	if (pool->npages > _manager->options.max_size) {
		npages = pool->npages - _manager->options.max_size;
		/* free at least NUM_PAGES_TO_ALLOC number of pages
		 * to reduce calls to set_memory_wb */
		if (npages < NUM_PAGES_TO_ALLOC)
			npages = NUM_PAGES_TO_ALLOC;
	}
	spin_unlock_irqrestore(&pool->lock, irq_flags);
	if (npages)
		ttm_page_pool_free(pool, npages);
}

/*
 * On success pages list will hold count number of correctly
 * cached pages.
 */
static int ttm_get_pages(struct vm_page **pages, unsigned npages, int flags,
			 enum ttm_caching_state cstate)
{
	struct ttm_page_pool *pool = ttm_get_pool(flags, cstate);
	struct pglist plist;
	struct vm_page *p = NULL;
	const struct kmem_pa_mode *kp;
	int gfp_flags = 0;
	unsigned count;
	int r;

	/* No pool for cached pages */
	if (pool == NULL) {

		if (flags & TTM_PAGE_FLAG_ZERO_ALLOC) {
			if (flags & TTM_PAGE_FLAG_DMA32)
				kp = &kp_dma_zero;
			else
				kp = &kp_zero;
		} else if (flags & TTM_PAGE_FLAG_DMA32) {
			kp = &kp_dma;
		} else {
			kp = &kp_dirty;
		}

		for (r = 0; r < npages; ++r) {
//			p = km_alloc(PAGE_SIZE, &kv_any, kp, &kd_waitok);
			p = ttm_uvm_alloc_page();
			if (!p) {

				printf("ttm: Unable to allocate page\n");
				return -ENOMEM;
			}

			pages[r] = p;
		}
		return 0;
	}

	/* combine zero flag to pool flags */
	gfp_flags |= pool->ttm_page_alloc_flags;

	/* First we take pages from the pool */
	TAILQ_INIT(&plist);
	npages = ttm_page_pool_get_pages(pool, &plist, flags, cstate, npages);
	count = 0;
	TAILQ_FOREACH(p, &plist, pageq) {
		pages[count++] = p;
	}

	/* clear the pages coming from the pool if requested */
	if (flags & TTM_PAGE_FLAG_ZERO_ALLOC) {
		TAILQ_FOREACH(p, &plist, pageq) {
			pmap_zero_page(p);
		}
	}

	/* If pool didn't have enough pages allocate new one. */
	if (npages > 0) {
		/* ttm_alloc_new_pages doesn't reference pool so we can run
		 * multiple requests in parallel.
		 **/
		TAILQ_INIT(&plist);
		r = ttm_alloc_new_pages(&plist, gfp_flags, flags, cstate, npages);
		TAILQ_FOREACH(p, &plist, pageq) {
			pages[count++] = p;
		}
		if (r) {
			/* If there is any pages in the list put them back to
			 * the pool. */
			printf("ttm: Failed to allocate extra pages for large request\n");
			ttm_put_pages(pages, count, flags, cstate);
			return r;
		}
	}

	return 0;
}

static void ttm_page_pool_init_locked(struct ttm_page_pool *pool, int flags,
		char *name)
{
	mtx_init(&pool->lock, IPL_TTY);
	pool->fill_lock = false;
	TAILQ_INIT(&pool->list);
	pool->npages = pool->nfrees = 0;
	pool->ttm_page_alloc_flags = flags;
	pool->name = name;
}

int ttm_page_alloc_init(struct ttm_mem_global *glob, unsigned max_pages)
{
	WARN_ON(_manager);

	DRM_DEBUG("Initializing pool allocator\n");

	_manager = kzalloc(sizeof(*_manager), GFP_KERNEL);

	ttm_page_pool_init_locked(&_manager->wc_pool, 0, "wc");

	ttm_page_pool_init_locked(&_manager->uc_pool, 0, "uc");

	ttm_page_pool_init_locked(&_manager->wc_pool_dma32,
	    TTM_PAGE_FLAG_DMA32, "wc dma");

	ttm_page_pool_init_locked(&_manager->uc_pool_dma32,
	    TTM_PAGE_FLAG_DMA32, "uc dma");

	_manager->options.max_size = max_pages;
	_manager->options.small = SMALL_ALLOCATION;
	_manager->options.alloc_size = NUM_PAGES_TO_ALLOC;

	refcount_init(&_manager->kobj_ref, 1);
	ttm_pool_mm_shrink_init(_manager);

	return 0;
}

void ttm_page_alloc_fini(void)
{
	int i;

	printf("Finalizing pool allocator\n");
	ttm_pool_mm_shrink_fini(_manager);

	for (i = 0; i < NUM_POOLS; ++i)
		ttm_page_pool_free(&_manager->pools[i], FREE_ALL_PAGES);

	if (refcount_release(&_manager->kobj_ref))
		ttm_pool_kobj_release(_manager);
	_manager = NULL;
}

int ttm_pool_populate(struct ttm_tt *ttm)
{
	struct ttm_mem_global *mem_glob = ttm->glob->mem_glob;
	unsigned i;
	int ret;

	if (ttm->state != tt_unpopulated)
		return 0;

	for (i = 0; i < ttm->num_pages; ++i) {
		ret = ttm_get_pages(&ttm->pages[i], 1,
				    ttm->page_flags,
				    ttm->caching_state);
		if (ret != 0) {
			ttm_pool_unpopulate(ttm);
			return -ENOMEM;
		}

		ret = ttm_mem_global_alloc_page(mem_glob, ttm->pages[i],
						false, false);
		if (unlikely(ret != 0)) {
			ttm_pool_unpopulate(ttm);
			return -ENOMEM;
		}
	}

	if (unlikely(ttm->page_flags & TTM_PAGE_FLAG_SWAPPED)) {
		ret = ttm_tt_swapin(ttm);
		if (unlikely(ret != 0)) {
			ttm_pool_unpopulate(ttm);
			return ret;
		}
	}

	ttm->state = tt_unbound;
	return 0;
}
EXPORT_SYMBOL(ttm_pool_populate);

void ttm_pool_unpopulate(struct ttm_tt *ttm)
{
	unsigned i;

	for (i = 0; i < ttm->num_pages; ++i) {
		if (ttm->pages[i]) {
			ttm_mem_global_free_page(ttm->glob->mem_glob,
						 ttm->pages[i]);
			ttm_put_pages(&ttm->pages[i], 1,
				      ttm->page_flags,
				      ttm->caching_state);
		}
	}
	ttm->state = tt_unpopulated;
}
EXPORT_SYMBOL(ttm_pool_unpopulate);

#ifdef notyet
int ttm_page_alloc_debugfs(struct seq_file *m, void *data)
{
	struct ttm_page_pool *p;
	unsigned i;
	char *h[] = {"pool", "refills", "pages freed", "size"};
	if (!_manager) {
		seq_printf(m, "No pool allocator running.\n");
		return 0;
	}
	seq_printf(m, "%6s %12s %13s %8s\n",
			h[0], h[1], h[2], h[3]);
	for (i = 0; i < NUM_POOLS; ++i) {
		p = &_manager->pools[i];

		seq_printf(m, "%6s %12ld %13ld %8d\n",
				p->name, p->nrefills,
				p->nfrees, p->npages);
	}
	return 0;
}
#endif
EXPORT_SYMBOL(ttm_page_alloc_debugfs);
