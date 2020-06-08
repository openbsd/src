/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright © 2014-2016 Intel Corporation
 */

#include <linux/highmem.h>
#include <linux/shmem_fs.h>
#include <linux/swap.h>

#include <drm/drm.h> /* for drm_legacy.h! */
#include <drm/drm_cache.h>
#include <drm/drm_legacy.h> /* for drm_pci.h! */
#include <drm/drm_pci.h>

#include "gt/intel_gt.h"
#include "i915_drv.h"
#include "i915_gem_object.h"
#include "i915_gem_region.h"
#include "i915_scatterlist.h"

static int i915_gem_object_get_pages_phys(struct drm_i915_gem_object *obj)
{
#ifdef __linux__
	struct address_space *mapping = obj->base.filp->f_mapping;
#else
	struct drm_dma_handle *phys;
#endif
	struct scatterlist *sg;
	struct sg_table *st;
	dma_addr_t dma;
	void *vaddr;
	void *dst;
	int i;

	if (WARN_ON(i915_gem_object_needs_bit17_swizzle(obj)))
		return -EINVAL;

	/*
	 * Always aligning to the object size, allows a single allocation
	 * to handle all possible callers, and given typical object sizes,
	 * the alignment of the buddy allocation will naturally match.
	 */
#ifdef __linux__
	vaddr = dma_alloc_coherent(&obj->base.dev->pdev->dev,
				   roundup_pow_of_two(obj->base.size),
				   &dma, GFP_KERNEL);
	if (!vaddr)
		return -ENOMEM;
#else
	phys = drm_pci_alloc(obj->base.dev,
			     roundup_pow_of_two(obj->base.size),
			     roundup_pow_of_two(obj->base.size));
	if (!phys)
		return -ENOMEM;
	vaddr = phys->vaddr;
	dma = phys->busaddr;
#endif

	st = kmalloc(sizeof(*st), GFP_KERNEL);
	if (!st)
		goto err_pci;

	if (sg_alloc_table(st, 1, GFP_KERNEL))
		goto err_st;

	sg = st->sgl;
	sg->offset = 0;
	sg->length = obj->base.size;

#ifdef __linux__
	sg_assign_page(sg, (struct page *)vaddr);
#else
	sg_assign_page(sg, (struct vm_page *)phys);
#endif
	sg_dma_address(sg) = dma;
	sg_dma_len(sg) = obj->base.size;

	dst = vaddr;
	for (i = 0; i < obj->base.size / PAGE_SIZE; i++) {
		struct vm_page *page;
		void *src;

#ifdef  __linux__
		page = shmem_read_mapping_page(mapping, i);
		if (IS_ERR(page))
			goto err_st;
#else
		struct pglist plist;
		TAILQ_INIT(&plist);
		if (uvm_objwire(obj->base.uao, i * PAGE_SIZE,
				(i + 1) * PAGE_SIZE, &plist))
			goto err_st;
		page = TAILQ_FIRST(&plist);
#endif

		src = kmap_atomic(page);
		memcpy(dst, src, PAGE_SIZE);
		drm_clflush_virt_range(dst, PAGE_SIZE);
		kunmap_atomic(src);

#ifdef __linux__
		put_page(page);
#else
		uvm_objunwire(obj->base.uao, i * PAGE_SIZE,
			      (i + 1) * PAGE_SIZE);
#endif
		dst += PAGE_SIZE;
	}

	intel_gt_chipset_flush(&to_i915(obj->base.dev)->gt);

	__i915_gem_object_set_pages(obj, st, sg->length);

	return 0;

err_st:
	kfree(st);
err_pci:
#ifdef __linux__
	dma_free_coherent(&obj->base.dev->pdev->dev,
			  roundup_pow_of_two(obj->base.size),
			  vaddr, dma);
#else
	drm_pci_free(obj->base.dev, phys);
#endif
	return -ENOMEM;
}

static void
i915_gem_object_put_pages_phys(struct drm_i915_gem_object *obj,
			       struct sg_table *pages)
{
	dma_addr_t dma = sg_dma_address(pages->sgl);
#ifdef __linux__
	void *vaddr = sg_page(pages->sgl);
#else
	struct drm_dma_handle *phys = (void *)sg_page(pages->sgl);
	void *vaddr = phys->vaddr;
#endif

	__i915_gem_object_release_shmem(obj, pages, false);

	if (obj->mm.dirty) {
#ifdef __linux__
		struct address_space *mapping = obj->base.filp->f_mapping;
#endif
		void *src = vaddr;
		int i;

		for (i = 0; i < obj->base.size / PAGE_SIZE; i++) {
			struct vm_page *page;
			char *dst;

#ifdef __linux__
			page = shmem_read_mapping_page(mapping, i);
			if (IS_ERR(page))
				continue;
#else
			struct pglist plist;
			TAILQ_INIT(&plist);
			if (uvm_objwire(obj->base.uao, i * PAGE_SIZE,
					(i + 1) * PAGE_SIZE, &plist))
				continue;
			page = TAILQ_FIRST(&plist);
#endif

			dst = kmap_atomic(page);
			drm_clflush_virt_range(src, PAGE_SIZE);
			memcpy(dst, src, PAGE_SIZE);
			kunmap_atomic(dst);

			set_page_dirty(page);
#ifdef __linux__
			if (obj->mm.madv == I915_MADV_WILLNEED)
				mark_page_accessed(page);
			put_page(page);
#else
			uvm_objunwire(obj->base.uao, i * PAGE_SIZE,
				      (i + 1) * PAGE_SIZE);
#endif

			src += PAGE_SIZE;
		}
		obj->mm.dirty = false;
	}

	sg_free_table(pages);
	kfree(pages);

#ifdef __linux__
	dma_free_coherent(&obj->base.dev->pdev->dev,
			  roundup_pow_of_two(obj->base.size),
			  vaddr, dma);
#else
	drm_pci_free(obj->base.dev, phys);
#endif
}

static void phys_release(struct drm_i915_gem_object *obj)
{
	fput(obj->base.filp);
}

static const struct drm_i915_gem_object_ops i915_gem_phys_ops = {
	.get_pages = i915_gem_object_get_pages_phys,
	.put_pages = i915_gem_object_put_pages_phys,

	.release = phys_release,
};

int i915_gem_object_attach_phys(struct drm_i915_gem_object *obj, int align)
{
	struct sg_table *pages;
	int err;

	if (align > obj->base.size)
		return -EINVAL;

	if (obj->ops == &i915_gem_phys_ops)
		return 0;

	if (obj->ops != &i915_gem_shmem_ops)
		return -EINVAL;

	err = i915_gem_object_unbind(obj, I915_GEM_OBJECT_UNBIND_ACTIVE);
	if (err)
		return err;

	mutex_lock_nested(&obj->mm.lock, I915_MM_GET_PAGES);

	if (obj->mm.madv != I915_MADV_WILLNEED) {
		err = -EFAULT;
		goto err_unlock;
	}

	if (obj->mm.quirked) {
		err = -EFAULT;
		goto err_unlock;
	}

	if (obj->mm.mapping) {
		err = -EBUSY;
		goto err_unlock;
	}

	pages = __i915_gem_object_unset_pages(obj);

	obj->ops = &i915_gem_phys_ops;

	err = ____i915_gem_object_get_pages(obj);
	if (err)
		goto err_xfer;

	/* Perma-pin (until release) the physical set of pages */
	__i915_gem_object_pin_pages(obj);

	if (!IS_ERR_OR_NULL(pages))
		i915_gem_shmem_ops.put_pages(obj, pages);

	i915_gem_object_release_memory_region(obj);

	mutex_unlock(&obj->mm.lock);
	return 0;

err_xfer:
	obj->ops = &i915_gem_shmem_ops;
	if (!IS_ERR_OR_NULL(pages)) {
		unsigned int sg_page_sizes = i915_sg_page_sizes(pages->sgl);

		__i915_gem_object_set_pages(obj, pages, sg_page_sizes);
	}
err_unlock:
	mutex_unlock(&obj->mm.lock);
	return err;
}

#if IS_ENABLED(CONFIG_DRM_I915_SELFTEST)
#include "selftests/i915_gem_phys.c"
#endif
