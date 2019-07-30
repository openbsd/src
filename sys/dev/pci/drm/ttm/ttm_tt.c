/*	$OpenBSD: ttm_tt.c,v 1.4 2015/02/11 07:01:37 jsg Exp $	*/
/**************************************************************************
 *
 * Copyright (c) 2006-2009 VMware, Inc., Palo Alto, CA., USA
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/
/*
 * Authors: Thomas Hellstrom <thellstrom-at-vmware-dot-com>
 */

#define pr_fmt(fmt) "[TTM] " fmt

#include <dev/pci/drm/drmP.h>
#include <dev/pci/drm/drm_cache.h>
#include <dev/pci/drm/drm_mem_util.h>
#include <dev/pci/drm/ttm/ttm_module.h>
#include <dev/pci/drm/ttm/ttm_bo_driver.h>
#include <dev/pci/drm/ttm/ttm_placement.h>
#include <dev/pci/drm/ttm/ttm_page_alloc.h>

/**
 * Allocates storage for pointers to the pages that back the ttm.
 */
static void ttm_tt_alloc_page_directory(struct ttm_tt *ttm)
{
	ttm->pages = drm_calloc_large(ttm->num_pages, sizeof(void*));
}

static void ttm_dma_tt_alloc_page_directory(struct ttm_dma_tt *ttm)
{
	ttm->ttm.pages = drm_calloc_large(ttm->ttm.num_pages, sizeof(void*));
	ttm->dma_address = drm_calloc_large(ttm->ttm.num_pages,
					    sizeof(*ttm->dma_address));
}

#ifdef CONFIG_X86
static inline int ttm_tt_set_page_caching(struct vm_page *p,
					  enum ttm_caching_state c_old,
					  enum ttm_caching_state c_new)
{
	int ret = 0;

	if (PageHighMem(p))
		return 0;

	if (c_old != tt_cached) {
		/* p isn't in the default caching state, set it to
		 * writeback first to free its current memtype. */

		ret = set_pages_wb(p, 1);
		if (ret)
			return ret;
	}

	if (c_new == tt_wc)
		ret = set_memory_wc((unsigned long) page_address(p), 1);
	else if (c_new == tt_uncached)
		ret = set_pages_uc(p, 1);

	return ret;
}
#else /* CONFIG_X86 */
static inline int ttm_tt_set_page_caching(struct vm_page *p,
					  enum ttm_caching_state c_old,
					  enum ttm_caching_state c_new)
{
	return 0;
}
#endif /* CONFIG_X86 */

/*
 * Change caching policy for the linear kernel map
 * for range of pages in a ttm.
 */

static int ttm_tt_set_caching(struct ttm_tt *ttm,
			      enum ttm_caching_state c_state)
{
	int i, j;
	struct vm_page *cur_page;
	int ret;

	if (ttm->caching_state == c_state)
		return 0;

	if (ttm->state == tt_unpopulated) {
		/* Change caching but don't populate */
		ttm->caching_state = c_state;
		return 0;
	}

	if (ttm->caching_state == tt_cached)
#ifdef notyet
		drm_clflush_pages(ttm->pages, ttm->num_pages);
#else
		printf("%s partial stub\n", __func__);
#endif

	for (i = 0; i < ttm->num_pages; ++i) {
		cur_page = ttm->pages[i];
		if (likely(cur_page != NULL)) {
			ret = ttm_tt_set_page_caching(cur_page,
						      ttm->caching_state,
						      c_state);
			if (unlikely(ret != 0))
				goto out_err;
		}
	}

	ttm->caching_state = c_state;

	return 0;

out_err:
	for (j = 0; j < i; ++j) {
		cur_page = ttm->pages[j];
		if (likely(cur_page != NULL)) {
			(void)ttm_tt_set_page_caching(cur_page, c_state,
						      ttm->caching_state);
		}
	}

	return ret;
}

int ttm_tt_set_placement_caching(struct ttm_tt *ttm, uint32_t placement)
{
	enum ttm_caching_state state;

	if (placement & TTM_PL_FLAG_WC)
		state = tt_wc;
	else if (placement & TTM_PL_FLAG_UNCACHED)
		state = tt_uncached;
	else
		state = tt_cached;

	return ttm_tt_set_caching(ttm, state);
}
EXPORT_SYMBOL(ttm_tt_set_placement_caching);

void ttm_tt_destroy(struct ttm_tt *ttm)
{
	if (unlikely(ttm == NULL))
		return;

	if (ttm->state == tt_bound) {
		ttm_tt_unbind(ttm);
	}

	if (ttm->state == tt_unbound) {
		ttm->bdev->driver->ttm_tt_unpopulate(ttm);
	}

	if (!(ttm->page_flags & TTM_PAGE_FLAG_PERSISTENT_SWAP) &&
	    ttm->swap_storage)
		uao_detach(ttm->swap_storage);

	ttm->swap_storage = NULL;
	ttm->func->destroy(ttm);
}

int ttm_tt_init(struct ttm_tt *ttm, struct ttm_bo_device *bdev,
		unsigned long size, uint32_t page_flags,
		struct vm_page *dummy_read_page)
{
	ttm->bdev = bdev;
	ttm->glob = bdev->glob;
	ttm->num_pages = (size + PAGE_SIZE - 1) >> PAGE_SHIFT;
	ttm->caching_state = tt_cached;
	ttm->page_flags = page_flags;
	ttm->dummy_read_page = dummy_read_page;
	ttm->state = tt_unpopulated;
	ttm->swap_storage = NULL;

	ttm_tt_alloc_page_directory(ttm);
	if (!ttm->pages) {
		ttm_tt_destroy(ttm);
		pr_err("Failed allocating page table\n");
		return -ENOMEM;
	}
	return 0;
}
EXPORT_SYMBOL(ttm_tt_init);

void ttm_tt_fini(struct ttm_tt *ttm)
{
	drm_free_large(ttm->pages);
	ttm->pages = NULL;
}
EXPORT_SYMBOL(ttm_tt_fini);

int ttm_dma_tt_init(struct ttm_dma_tt *ttm_dma, struct ttm_bo_device *bdev,
		unsigned long size, uint32_t page_flags,
		struct vm_page *dummy_read_page)
{
	struct ttm_tt *ttm = &ttm_dma->ttm;

	ttm->bdev = bdev;
	ttm->glob = bdev->glob;
	ttm->num_pages = (size + PAGE_SIZE - 1) >> PAGE_SHIFT;
	ttm->caching_state = tt_cached;
	ttm->page_flags = page_flags;
	ttm->dummy_read_page = dummy_read_page;
	ttm->state = tt_unpopulated;
	ttm->swap_storage = NULL;

	INIT_LIST_HEAD(&ttm_dma->pages_list);
	ttm_dma_tt_alloc_page_directory(ttm_dma);
	if (!ttm->pages || !ttm_dma->dma_address) {
		ttm_tt_destroy(ttm);
		pr_err("Failed allocating page table\n");
		return -ENOMEM;
	}
	return 0;
}
EXPORT_SYMBOL(ttm_dma_tt_init);

void ttm_dma_tt_fini(struct ttm_dma_tt *ttm_dma)
{
	struct ttm_tt *ttm = &ttm_dma->ttm;

	drm_free_large(ttm->pages);
	ttm->pages = NULL;
	drm_free_large(ttm_dma->dma_address);
	ttm_dma->dma_address = NULL;
}
EXPORT_SYMBOL(ttm_dma_tt_fini);

void ttm_tt_unbind(struct ttm_tt *ttm)
{
	int ret;

	if (ttm->state == tt_bound) {
		ret = ttm->func->unbind(ttm);
		BUG_ON(ret);
		ttm->state = tt_unbound;
	}
}

int ttm_tt_bind(struct ttm_tt *ttm, struct ttm_mem_reg *bo_mem)
{
	int ret = 0;

	if (!ttm)
		return -EINVAL;

	if (ttm->state == tt_bound)
		return 0;

	ret = ttm->bdev->driver->ttm_tt_populate(ttm);
	if (ret)
		return ret;

	ret = ttm->func->bind(ttm, bo_mem);
	if (unlikely(ret != 0))
		return ret;

	ttm->state = tt_bound;

	return 0;
}
EXPORT_SYMBOL(ttm_tt_bind);

int ttm_tt_swapin(struct ttm_tt *ttm)
{
	struct uvm_object *swap_storage;
	struct vm_page *from_page;
	struct vm_page *to_page;
	struct pglist plist;
	int i;
	int ret = -ENOMEM;

	swap_storage = ttm->swap_storage;
	BUG_ON(swap_storage == NULL);

	TAILQ_INIT(&plist);
	if (uvm_objwire(swap_storage, 0, ttm->num_pages << PAGE_SHIFT, &plist))
		goto out_err;

	from_page = TAILQ_FIRST(&plist);
	for (i = 0; i < ttm->num_pages; ++i) {
		to_page = ttm->pages[i];
		if (unlikely(to_page == NULL))
			goto out_err;

		uvm_pagecopy(from_page, to_page);
		from_page = TAILQ_NEXT(from_page, pageq);
	}

	uvm_objunwire(swap_storage, 0, ttm->num_pages << PAGE_SHIFT);

	if (!(ttm->page_flags & TTM_PAGE_FLAG_PERSISTENT_SWAP))
		uao_detach(swap_storage);
	ttm->swap_storage = NULL;
	ttm->page_flags &= ~TTM_PAGE_FLAG_SWAPPED;

	return 0;
out_err:
	return ret;
}

int ttm_tt_swapout(struct ttm_tt *ttm, struct uvm_object *persistent_swap_storage)
{
	struct uvm_object *swap_storage;
	struct vm_page *from_page;
	struct vm_page *to_page;
	struct pglist plist;
	int i;
	int ret = -ENOMEM;

	BUG_ON(ttm->state != tt_unbound && ttm->state != tt_unpopulated);
	BUG_ON(ttm->caching_state != tt_cached);

	if (!persistent_swap_storage) {
		swap_storage = uao_create(ttm->num_pages << PAGE_SHIFT, 0);
#ifdef notyet
		if (unlikely(IS_ERR(swap_storage))) {
			pr_err("Failed allocating swap storage\n");
			return PTR_ERR(swap_storage);
		}
#endif
	} else
		swap_storage = persistent_swap_storage;

	TAILQ_INIT(&plist);
	if (uvm_objwire(swap_storage, 0, ttm->num_pages << PAGE_SHIFT, &plist))
		goto out_err;

	to_page = TAILQ_FIRST(&plist);
	for (i = 0; i < ttm->num_pages; ++i) {
		from_page = ttm->pages[i];
		if (unlikely(from_page == NULL))
			continue;
		uvm_pagecopy(from_page, to_page);
#ifdef notyet
		set_page_dirty(to_page);
		mark_page_accessed(to_page);
#endif
		to_page = TAILQ_NEXT(to_page, pageq);
	}

	uvm_objunwire(swap_storage, 0, ttm->num_pages << PAGE_SHIFT);

	ttm->bdev->driver->ttm_tt_unpopulate(ttm);
	ttm->swap_storage = swap_storage;
	ttm->page_flags |= TTM_PAGE_FLAG_SWAPPED;
	if (persistent_swap_storage)
		ttm->page_flags |= TTM_PAGE_FLAG_PERSISTENT_SWAP;

	return 0;
out_err:
	if (!persistent_swap_storage)
		uao_detach(swap_storage);

	return ret;
}
