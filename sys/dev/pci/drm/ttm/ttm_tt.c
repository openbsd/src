/* SPDX-License-Identifier: GPL-2.0 OR MIT */
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

#include <linux/sched.h>
#include <linux/pagemap.h>
#include <linux/shmem_fs.h>
#include <linux/file.h>
#include <linux/export.h>
#include <drm/drm_cache.h>
#include <drm/ttm/ttm_bo_driver.h>
#include <drm/ttm/ttm_page_alloc.h>
#include <drm/ttm/ttm_set_memory.h>

/**
 * Allocates a ttm structure for the given BO.
 */
int ttm_tt_create(struct ttm_buffer_object *bo, bool zero_alloc)
{
	struct ttm_bo_device *bdev = bo->bdev;
	uint32_t page_flags = 0;

	reservation_object_assert_held(bo->resv);

	if (bdev->need_dma32)
		page_flags |= TTM_PAGE_FLAG_DMA32;

	if (bdev->no_retry)
		page_flags |= TTM_PAGE_FLAG_NO_RETRY;

	switch (bo->type) {
	case ttm_bo_type_device:
		if (zero_alloc)
			page_flags |= TTM_PAGE_FLAG_ZERO_ALLOC;
		break;
	case ttm_bo_type_kernel:
		break;
	case ttm_bo_type_sg:
		page_flags |= TTM_PAGE_FLAG_SG;
		break;
	default:
		bo->ttm = NULL;
		pr_err("Illegal buffer object type\n");
		return -EINVAL;
	}

	bo->ttm = bdev->driver->ttm_tt_create(bo, page_flags);
	if (unlikely(bo->ttm == NULL))
		return -ENOMEM;

	return 0;
}

/**
 * Allocates storage for pointers to the pages that back the ttm.
 */
static int ttm_tt_alloc_page_directory(struct ttm_tt *ttm)
{
	ttm->pages = kvmalloc_array(ttm->num_pages, sizeof(void*),
			GFP_KERNEL | __GFP_ZERO);
	if (!ttm->pages)
		return -ENOMEM;
	return 0;
}

static int ttm_dma_tt_alloc_page_directory(struct ttm_dma_tt *ttm)
{
	ttm->ttm.pages = kvmalloc_array(ttm->ttm.num_pages,
					  sizeof(*ttm->ttm.pages) +
					  sizeof(*ttm->dma_address),
					  GFP_KERNEL | __GFP_ZERO);
	if (!ttm->ttm.pages)
		return -ENOMEM;
	ttm->dma_address = (void *) (ttm->ttm.pages + ttm->ttm.num_pages);
	return 0;
}

static int ttm_sg_tt_alloc_page_directory(struct ttm_dma_tt *ttm)
{
	ttm->dma_address = kvmalloc_array(ttm->ttm.num_pages,
					  sizeof(*ttm->dma_address),
					  GFP_KERNEL | __GFP_ZERO);
	if (!ttm->dma_address)
		return -ENOMEM;
	return 0;
}

static int ttm_tt_set_page_caching(struct vm_page *p,
				   enum ttm_caching_state c_old,
				   enum ttm_caching_state c_new)
{
	int ret = 0;

	if (PageHighMem(p))
		return 0;

	if (c_old != tt_cached) {
		/* p isn't in the default caching state, set it to
		 * writeback first to free its current memtype. */

		ret = ttm_set_pages_wb(p, 1);
		if (ret)
			return ret;
	}

	if (c_new == tt_wc)
		ret = ttm_set_pages_wc(p, 1);
	else if (c_new == tt_uncached)
		ret = ttm_set_pages_uc(p, 1);

	return ret;
}

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
		drm_clflush_pages(ttm->pages, ttm->num_pages);

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
	if (ttm == NULL)
		return;

	ttm_tt_unbind(ttm);

	if (ttm->state == tt_unbound)
		ttm_tt_unpopulate(ttm);

	if (!(ttm->page_flags & TTM_PAGE_FLAG_PERSISTENT_SWAP) &&
	    ttm->swap_storage)
		uao_detach(ttm->swap_storage);

	ttm->swap_storage = NULL;
	ttm->func->destroy(ttm);
}

void ttm_tt_init_fields(struct ttm_tt *ttm, struct ttm_buffer_object *bo,
			uint32_t page_flags)
{
	ttm->bdev = bo->bdev;
	ttm->num_pages = bo->num_pages;
	ttm->caching_state = tt_cached;
	ttm->page_flags = page_flags;
	ttm->state = tt_unpopulated;
	ttm->swap_storage = NULL;
	ttm->sg = bo->sg;
}

int ttm_tt_init(struct ttm_tt *ttm, struct ttm_buffer_object *bo,
		uint32_t page_flags)
{
	ttm_tt_init_fields(ttm, bo, page_flags);

	if (ttm_tt_alloc_page_directory(ttm)) {
		ttm_tt_destroy(ttm);
		pr_err("Failed allocating page table\n");
		return -ENOMEM;
	}
	return 0;
}
EXPORT_SYMBOL(ttm_tt_init);

void ttm_tt_fini(struct ttm_tt *ttm)
{
	kvfree(ttm->pages);
	ttm->pages = NULL;
}
EXPORT_SYMBOL(ttm_tt_fini);

int ttm_dma_tt_init(struct ttm_dma_tt *ttm_dma, struct ttm_buffer_object *bo,
		    uint32_t page_flags)
{
	struct ttm_tt *ttm = &ttm_dma->ttm;
	int flags = BUS_DMA_WAITOK;

	ttm_tt_init_fields(ttm, bo, page_flags);

	INIT_LIST_HEAD(&ttm_dma->pages_list);
	if (ttm_dma_tt_alloc_page_directory(ttm_dma)) {
		ttm_tt_destroy(ttm);
		pr_err("Failed allocating page table\n");
		return -ENOMEM;
	}

	ttm_dma->segs = km_alloc(round_page(ttm->num_pages *
	    sizeof(bus_dma_segment_t)), &kv_any, &kp_zero, &kd_waitok);

	ttm_dma->dmat = bo->bdev->dmat;

	if ((page_flags & TTM_PAGE_FLAG_DMA32) == 0)
		flags |= BUS_DMA_64BIT;
	if (bus_dmamap_create(ttm_dma->dmat, ttm->num_pages << PAGE_SHIFT,
	    ttm->num_pages, ttm->num_pages << PAGE_SHIFT, 0, flags,
	    &ttm_dma->map)) {
		km_free(ttm_dma->segs, round_page(ttm->num_pages *
		    sizeof(bus_dma_segment_t)), &kv_any, &kp_zero);
		ttm_tt_destroy(ttm);
		pr_err("Failed allocating page table\n");
		return -ENOMEM;
	}

	return 0;
}
EXPORT_SYMBOL(ttm_dma_tt_init);

int ttm_sg_tt_init(struct ttm_dma_tt *ttm_dma, struct ttm_buffer_object *bo,
		   uint32_t page_flags)
{
	struct ttm_tt *ttm = &ttm_dma->ttm;
	int flags = BUS_DMA_WAITOK;
	int ret;

	ttm_tt_init_fields(ttm, bo, page_flags);

	INIT_LIST_HEAD(&ttm_dma->pages_list);
	if (page_flags & TTM_PAGE_FLAG_SG)
		ret = ttm_sg_tt_alloc_page_directory(ttm_dma);
	else
		ret = ttm_dma_tt_alloc_page_directory(ttm_dma);
	if (ret) {
		ttm_tt_destroy(ttm);
		pr_err("Failed allocating page table\n");
		return -ENOMEM;
	}

	ttm_dma->segs = km_alloc(round_page(ttm->num_pages *
	    sizeof(bus_dma_segment_t)), &kv_any, &kp_zero, &kd_waitok);

	ttm_dma->dmat = bo->bdev->dmat;

	if ((page_flags & TTM_PAGE_FLAG_DMA32) == 0)
		flags |= BUS_DMA_64BIT;
	if (bus_dmamap_create(ttm_dma->dmat, ttm->num_pages << PAGE_SHIFT,
	    ttm->num_pages, ttm->num_pages << PAGE_SHIFT, 0, flags,
	    &ttm_dma->map)) {
		km_free(ttm_dma->segs, round_page(ttm->num_pages *
		    sizeof(bus_dma_segment_t)), &kv_any, &kp_zero);
		ttm_tt_destroy(ttm);
		pr_err("Failed allocating page table\n");
		return -ENOMEM;
	}

	return 0;
}
EXPORT_SYMBOL(ttm_sg_tt_init);

void ttm_dma_tt_fini(struct ttm_dma_tt *ttm_dma)
{
	struct ttm_tt *ttm = &ttm_dma->ttm;

	if (ttm->pages)
		kvfree(ttm->pages);
	else
		kvfree(ttm_dma->dma_address);
	ttm->pages = NULL;
	ttm_dma->dma_address = NULL;

	bus_dmamap_destroy(ttm_dma->dmat, ttm_dma->map);
	km_free(ttm_dma->segs, round_page(ttm->num_pages *
	    sizeof(bus_dma_segment_t)), &kv_any, &kp_zero);
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

int ttm_tt_bind(struct ttm_tt *ttm, struct ttm_mem_reg *bo_mem,
		struct ttm_operation_ctx *ctx)
{
	int ret = 0;

	if (!ttm)
		return -EINVAL;

	if (ttm->state == tt_bound)
		return 0;

	ret = ttm_tt_populate(ttm, ctx);
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
		if (IS_ERR(swap_storage)) {
			pr_err("Failed allocating swap storage\n");
			return PTR_ERR(swap_storage);
		}
#endif
	} else {
		swap_storage = persistent_swap_storage;
	}

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

static void ttm_tt_add_mapping(struct ttm_tt *ttm)
{
#ifdef __linux__
	pgoff_t i;

	if (ttm->page_flags & TTM_PAGE_FLAG_SG)
		return;

	for (i = 0; i < ttm->num_pages; ++i)
		ttm->pages[i]->mapping = ttm->bdev->dev_mapping;
#endif
}

int ttm_tt_populate(struct ttm_tt *ttm, struct ttm_operation_ctx *ctx)
{
	int ret;

	if (ttm->state != tt_unpopulated)
		return 0;

	if (ttm->bdev->driver->ttm_tt_populate)
		ret = ttm->bdev->driver->ttm_tt_populate(ttm, ctx);
	else
		ret = ttm_pool_populate(ttm, ctx);
	if (!ret)
		ttm_tt_add_mapping(ttm);
	return ret;
}

static void ttm_tt_clear_mapping(struct ttm_tt *ttm)
{
	int i;
	struct vm_page *page;

	if (ttm->page_flags & TTM_PAGE_FLAG_SG)
		return;

	for (i = 0; i < ttm->num_pages; ++i) {
		page = ttm->pages[i];
		if (unlikely(page == NULL))
			continue;
		pmap_page_protect(page, PROT_NONE);
	}
}

void ttm_tt_unpopulate(struct ttm_tt *ttm)
{
	if (ttm->state == tt_unpopulated)
		return;

	ttm_tt_clear_mapping(ttm);
	if (ttm->bdev->driver->ttm_tt_unpopulate)
		ttm->bdev->driver->ttm_tt_unpopulate(ttm);
	else
		ttm_pool_unpopulate(ttm);
}
