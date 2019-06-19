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

#include <drm/ttm/ttm_module.h>
#include <drm/ttm/ttm_bo_driver.h>
#include <drm/ttm/ttm_placement.h>
#include <drm/drm_vma_manager.h>
#include <linux/mm.h>
#include <linux/pfn_t.h>
#include <linux/rbtree.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/mem_encrypt.h>

#define TTM_BO_VM_NUM_PREFAULT 16

#ifdef __linux__

static vm_fault_t ttm_bo_vm_fault_idle(struct ttm_buffer_object *bo,
				struct vm_fault *vmf)
{
	vm_fault_t ret = 0;
	int err = 0;

	if (likely(!bo->moving))
		goto out_unlock;

	/*
	 * Quick non-stalling check for idle.
	 */
	if (dma_fence_is_signaled(bo->moving))
		goto out_clear;

	/*
	 * If possible, avoid waiting for GPU with mmap_sem
	 * held.
	 */
	if (vmf->flags & FAULT_FLAG_ALLOW_RETRY) {
		ret = VM_FAULT_RETRY;
		if (vmf->flags & FAULT_FLAG_RETRY_NOWAIT)
			goto out_unlock;

		ttm_bo_get(bo);
		up_read(&vmf->vma->vm_mm->mmap_sem);
		(void) dma_fence_wait(bo->moving, true);
		ttm_bo_unreserve(bo);
		ttm_bo_put(bo);
		goto out_unlock;
	}

	/*
	 * Ordinary wait.
	 */
	err = dma_fence_wait(bo->moving, true);
	if (unlikely(err != 0)) {
		ret = (err != -ERESTARTSYS) ? VM_FAULT_SIGBUS :
			VM_FAULT_NOPAGE;
		goto out_unlock;
	}

out_clear:
	dma_fence_put(bo->moving);
	bo->moving = NULL;

out_unlock:
	return ret;
}

static unsigned long ttm_bo_io_mem_pfn(struct ttm_buffer_object *bo,
				       unsigned long page_offset)
{
	struct ttm_bo_device *bdev = bo->bdev;

	if (bdev->driver->io_mem_pfn)
		return bdev->driver->io_mem_pfn(bo, page_offset);

	return ((bo->mem.bus.base + bo->mem.bus.offset) >> PAGE_SHIFT)
		+ page_offset;
}

static vm_fault_t ttm_bo_vm_fault(struct vm_fault *vmf)
{
	struct vm_area_struct *vma = vmf->vma;
	struct ttm_buffer_object *bo = (struct ttm_buffer_object *)
	    vma->vm_private_data;
	struct ttm_bo_device *bdev = bo->bdev;
	unsigned long page_offset;
	unsigned long page_last;
	unsigned long pfn;
	struct ttm_tt *ttm = NULL;
	struct page *page;
	int err;
	int i;
	vm_fault_t ret = VM_FAULT_NOPAGE;
	unsigned long address = vmf->address;
	struct ttm_mem_type_manager *man =
		&bdev->man[bo->mem.mem_type];
	struct vm_area_struct cvma;

	/*
	 * Work around locking order reversal in fault / nopfn
	 * between mmap_sem and bo_reserve: Perform a trylock operation
	 * for reserve, and if it fails, retry the fault after waiting
	 * for the buffer to become unreserved.
	 */
	err = ttm_bo_reserve(bo, true, true, NULL);
	if (unlikely(err != 0)) {
		if (err != -EBUSY)
			return VM_FAULT_NOPAGE;

		if (vmf->flags & FAULT_FLAG_ALLOW_RETRY) {
			if (!(vmf->flags & FAULT_FLAG_RETRY_NOWAIT)) {
				ttm_bo_get(bo);
				up_read(&vmf->vma->vm_mm->mmap_sem);
				(void) ttm_bo_wait_unreserved(bo);
				ttm_bo_put(bo);
			}

			return VM_FAULT_RETRY;
		}

		/*
		 * If we'd want to change locking order to
		 * mmap_sem -> bo::reserve, we'd use a blocking reserve here
		 * instead of retrying the fault...
		 */
		return VM_FAULT_NOPAGE;
	}

	/*
	 * Refuse to fault imported pages. This should be handled
	 * (if at all) by redirecting mmap to the exporter.
	 */
	if (bo->ttm && (bo->ttm->page_flags & TTM_PAGE_FLAG_SG)) {
		ret = VM_FAULT_SIGBUS;
		goto out_unlock;
	}

	if (bdev->driver->fault_reserve_notify) {
		err = bdev->driver->fault_reserve_notify(bo);
		switch (err) {
		case 0:
			break;
		case -EBUSY:
		case -ERESTARTSYS:
			ret = VM_FAULT_NOPAGE;
			goto out_unlock;
		default:
			ret = VM_FAULT_SIGBUS;
			goto out_unlock;
		}
	}

	/*
	 * Wait for buffer data in transit, due to a pipelined
	 * move.
	 */
	ret = ttm_bo_vm_fault_idle(bo, vmf);
	if (unlikely(ret != 0)) {
		if (ret == VM_FAULT_RETRY &&
		    !(vmf->flags & FAULT_FLAG_RETRY_NOWAIT)) {
			/* The BO has already been unreserved. */
			return ret;
		}

		goto out_unlock;
	}

	err = ttm_mem_io_lock(man, true);
	if (unlikely(err != 0)) {
		ret = VM_FAULT_NOPAGE;
		goto out_unlock;
	}
	err = ttm_mem_io_reserve_vm(bo);
	if (unlikely(err != 0)) {
		ret = VM_FAULT_SIGBUS;
		goto out_io_unlock;
	}

	page_offset = ((address - vma->vm_start) >> PAGE_SHIFT) +
		vma->vm_pgoff - drm_vma_node_start(&bo->vma_node);
	page_last = vma_pages(vma) + vma->vm_pgoff -
		drm_vma_node_start(&bo->vma_node);

	if (unlikely(page_offset >= bo->num_pages)) {
		ret = VM_FAULT_SIGBUS;
		goto out_io_unlock;
	}

	/*
	 * Make a local vma copy to modify the page_prot member
	 * and vm_flags if necessary. The vma parameter is protected
	 * by mmap_sem in write mode.
	 */
	cvma = *vma;
	cvma.vm_page_prot = vm_get_page_prot(cvma.vm_flags);

	if (bo->mem.bus.is_iomem) {
		cvma.vm_page_prot = ttm_io_prot(bo->mem.placement,
						cvma.vm_page_prot);
	} else {
		struct ttm_operation_ctx ctx = {
			.interruptible = false,
			.no_wait_gpu = false,
			.flags = TTM_OPT_FLAG_FORCE_ALLOC

		};

		ttm = bo->ttm;
		cvma.vm_page_prot = ttm_io_prot(bo->mem.placement,
						cvma.vm_page_prot);

		/* Allocate all page at once, most common usage */
		if (ttm_tt_populate(ttm, &ctx)) {
			ret = VM_FAULT_OOM;
			goto out_io_unlock;
		}
	}

	/*
	 * Speculatively prefault a number of pages. Only error on
	 * first page.
	 */
	for (i = 0; i < TTM_BO_VM_NUM_PREFAULT; ++i) {
		if (bo->mem.bus.is_iomem) {
			/* Iomem should not be marked encrypted */
			cvma.vm_page_prot = pgprot_decrypted(cvma.vm_page_prot);
			pfn = ttm_bo_io_mem_pfn(bo, page_offset);
		} else {
			page = ttm->pages[page_offset];
			if (unlikely(!page && i == 0)) {
				ret = VM_FAULT_OOM;
				goto out_io_unlock;
			} else if (unlikely(!page)) {
				break;
			}
			page->index = drm_vma_node_start(&bo->vma_node) +
				page_offset;
			pfn = page_to_pfn(page);
		}

		if (vma->vm_flags & VM_MIXEDMAP)
			ret = vmf_insert_mixed(&cvma, address,
					__pfn_to_pfn_t(pfn, PFN_DEV));
		else
			ret = vmf_insert_pfn(&cvma, address, pfn);

		/*
		 * Somebody beat us to this PTE or prefaulting to
		 * an already populated PTE, or prefaulting error.
		 */

		if (unlikely((ret == VM_FAULT_NOPAGE && i > 0)))
			break;
		else if (unlikely(ret & VM_FAULT_ERROR))
			goto out_io_unlock;

		address += PAGE_SIZE;
		if (unlikely(++page_offset >= page_last))
			break;
	}
	ret = VM_FAULT_NOPAGE;
out_io_unlock:
	ttm_mem_io_unlock(man);
out_unlock:
	ttm_bo_unreserve(bo);
	return ret;
}

#else

static vm_fault_t ttm_bo_vm_fault_idle(struct ttm_buffer_object *bo,
    struct uvm_faultinfo *ufi)
{
	vm_fault_t ret = 0;

	if (likely(!bo->moving))
		goto out_unlock;

	/*
	 * Quick non-stalling check for idle.
	 */
	if (dma_fence_is_signaled(bo->moving))
		goto out_clear;

	/*
	 * If possible, avoid waiting for GPU with mmap_sem
	 * held.
	 */
	ret = VM_PAGER_REFAULT;

	ttm_bo_get(bo);
	uvmfault_unlockall(ufi, NULL, NULL, NULL);
	(void) dma_fence_wait(bo->moving, true);
	ttm_bo_unreserve(bo);
	ttm_bo_put(bo);
	goto out_unlock;

out_clear:
	dma_fence_put(bo->moving);
	bo->moving = NULL;

out_unlock:
	return ret;
}

int
ttm_bo_vm_fault(struct uvm_faultinfo *ufi, vaddr_t vaddr, vm_page_t *pps,
    int npages, int centeridx, vm_fault_t fault_type,
    vm_prot_t access_type, int flags)
{
	struct uvm_object *uobj = ufi->entry->object.uvm_obj;
	struct ttm_buffer_object *bo = (struct ttm_buffer_object *)uobj;
	struct ttm_bo_device *bdev = bo->bdev;
	unsigned long page_offset;
	unsigned long page_last;
	struct ttm_tt *ttm = NULL;
	struct vm_page *page;
	bus_addr_t addr;
	paddr_t paddr;
	vm_prot_t mapprot;
	int pmap_flags;
	int err;
	int i;
	int ret = VM_PAGER_OK;
	unsigned long address = (unsigned long)vaddr;
	struct ttm_mem_type_manager *man =
		&bdev->man[bo->mem.mem_type];

	/*
	 * Work around locking order reversal in fault / nopfn
	 * between mmap_sem and bo_reserve: Perform a trylock operation
	 * for reserve, and if it fails, retry the fault after waiting
	 * for the buffer to become unreserved.
	 */
	err = ttm_bo_reserve(bo, true, true, NULL);
	if (unlikely(err != 0)) {
		if (err != -EBUSY) {
			uvmfault_unlockall(ufi, NULL, NULL, NULL);
			return VM_PAGER_OK;
		}

		ttm_bo_get(bo);
		uvmfault_unlockall(ufi, NULL, NULL, NULL);
		(void) ttm_bo_wait_unreserved(bo);
		ttm_bo_put(bo);
		return VM_PAGER_REFAULT;
	}

	/*
	 * Refuse to fault imported pages. This should be handled
	 * (if at all) by redirecting mmap to the exporter.
	 */
	if (bo->ttm && (bo->ttm->page_flags & TTM_PAGE_FLAG_SG)) {
		ret = VM_PAGER_ERROR;
		goto out_unlock;
	}

	if (bdev->driver->fault_reserve_notify) {
		err = bdev->driver->fault_reserve_notify(bo);
		switch (err) {
		case 0:
			break;
		case -EBUSY:
		case -ERESTARTSYS:
			ret = VM_PAGER_OK;
			goto out_unlock;
		default:
			ret = VM_PAGER_ERROR;
			goto out_unlock;
		}
	}

	/*
	 * Wait for buffer data in transit, due to a pipelined
	 * move.
	 */
	ret = ttm_bo_vm_fault_idle(bo, ufi);
	if (unlikely(ret != 0)) {
		if (ret == VM_PAGER_REFAULT) {
			/* The BO has already been unreserved. */
			return ret;
		}

		goto out_unlock;
	}

	err = ttm_mem_io_lock(man, true);
	if (unlikely(err != 0)) {
		ret = VM_PAGER_OK;
		goto out_unlock;
	}
	err = ttm_mem_io_reserve_vm(bo);
	if (unlikely(err != 0)) {
		ret = VM_PAGER_ERROR;
		goto out_io_unlock;
	}

	page_offset = ((address - ufi->entry->start) >> PAGE_SHIFT) +
	    drm_vma_node_start(&bo->vma_node) - (ufi->entry->offset >> PAGE_SHIFT);
	page_last = ((ufi->entry->end - ufi->entry->start) >> PAGE_SHIFT) +
	    drm_vma_node_start(&bo->vma_node) - (ufi->entry->offset >> PAGE_SHIFT);

	if (unlikely(page_offset >= bo->num_pages)) {
		ret = VM_PAGER_ERROR;
		goto out_io_unlock;
	}

	/*
	 * Make a local vma copy to modify the page_prot member
	 * and vm_flags if necessary. The vma parameter is protected
	 * by mmap_sem in write mode.
	 */
	mapprot = ufi->entry->protection;
	if (bo->mem.bus.is_iomem) {
		pmap_flags = ttm_io_prot(bo->mem.placement, 0);
	} else {
		struct ttm_operation_ctx ctx = {
			.interruptible = false,
			.no_wait_gpu = false,
			.flags = TTM_OPT_FLAG_FORCE_ALLOC

		};

		ttm = bo->ttm;
		pmap_flags = ttm_io_prot(bo->mem.placement, 0);

		/* Allocate all page at once, most common usage */
		if (ttm_tt_populate(ttm, &ctx)) {
			ret = VM_PAGER_ERROR;
			goto out_io_unlock;
		}
	}

	/*
	 * Speculatively prefault a number of pages. Only error on
	 * first page.
	 */
	for (i = 0; i < TTM_BO_VM_NUM_PREFAULT; ++i) {
		if (bo->mem.bus.is_iomem) {
			addr = bo->mem.bus.base + bo->mem.bus.offset;
			paddr = bus_space_mmap(bdev->memt, addr,
					       page_offset << PAGE_SHIFT,
					       mapprot, 0);
		} else {
			page = ttm->pages[page_offset];
			if (unlikely(!page && i == 0)) {
				ret = VM_PAGER_ERROR;
				goto out_io_unlock;
			} else if (unlikely(!page)) {
				break;
			}
			paddr = VM_PAGE_TO_PHYS(page);
		}

		err = pmap_enter(ufi->orig_map->pmap, vaddr,
		    paddr | pmap_flags, mapprot, PMAP_CANFAIL | mapprot);

		/*
		 * Somebody beat us to this PTE or prefaulting to
		 * an already populated PTE, or prefaulting error.
		 */

		if (unlikely((err != 0 && i > 0)))
			break;
		else if (unlikely(err != 0)) {
			uvmfault_unlockall(ufi, NULL, NULL, NULL);
			uvm_wait("ttmflt");
			return VM_PAGER_REFAULT;
		}

		address += PAGE_SIZE;
		vaddr += PAGE_SIZE;
		if (unlikely(++page_offset >= page_last))
			break;
	}
	pmap_update(ufi->orig_map->pmap);
	ret = VM_PAGER_OK;
out_io_unlock:
	ttm_mem_io_unlock(man);
out_unlock:
	uvmfault_unlockall(ufi, NULL, NULL, NULL);
	ttm_bo_unreserve(bo);
	return ret;
}

#endif

#ifdef notyet
static void ttm_bo_vm_open(struct vm_area_struct *vma)
{
	struct ttm_buffer_object *bo =
	    (struct ttm_buffer_object *)vma->vm_private_data;

	WARN_ON(bo->bdev->dev_mapping != vma->vm_file->f_mapping);

	ttm_bo_get(bo);
}

static void ttm_bo_vm_close(struct vm_area_struct *vma)
{
	struct ttm_buffer_object *bo = (struct ttm_buffer_object *)vma->vm_private_data;

	ttm_bo_put(bo);
	vma->vm_private_data = NULL;
}

static int ttm_bo_vm_access_kmap(struct ttm_buffer_object *bo,
				 unsigned long offset,
				 uint8_t *buf, int len, int write)
{
	unsigned long page = offset >> PAGE_SHIFT;
	unsigned long bytes_left = len;
	int ret;

	/* Copy a page at a time, that way no extra virtual address
	 * mapping is needed
	 */
	offset -= page << PAGE_SHIFT;
	do {
		unsigned long bytes = min(bytes_left, PAGE_SIZE - offset);
		struct ttm_bo_kmap_obj map;
		void *ptr;
		bool is_iomem;

		ret = ttm_bo_kmap(bo, page, 1, &map);
		if (ret)
			return ret;

		ptr = (uint8_t *)ttm_kmap_obj_virtual(&map, &is_iomem) + offset;
		WARN_ON_ONCE(is_iomem);
		if (write)
			memcpy(ptr, buf, bytes);
		else
			memcpy(buf, ptr, bytes);
		ttm_bo_kunmap(&map);

		page++;
		buf += bytes;
		bytes_left -= bytes;
		offset = 0;
	} while (bytes_left);

	return len;
}

static int ttm_bo_vm_access(struct vm_area_struct *vma, unsigned long addr,
			    void *buf, int len, int write)
{
	unsigned long offset = (addr) - vma->vm_start;
	struct ttm_buffer_object *bo = vma->vm_private_data;
	int ret;

	if (len < 1 || (offset + len) >> PAGE_SHIFT > bo->num_pages)
		return -EIO;

	ret = ttm_bo_reserve(bo, true, false, NULL);
	if (ret)
		return ret;

	switch (bo->mem.mem_type) {
	case TTM_PL_SYSTEM:
		if (unlikely(bo->ttm->page_flags & TTM_PAGE_FLAG_SWAPPED)) {
			ret = ttm_tt_swapin(bo->ttm);
			if (unlikely(ret != 0))
				return ret;
		}
		/* fall through */
	case TTM_PL_TT:
		ret = ttm_bo_vm_access_kmap(bo, offset, buf, len, write);
		break;
	default:
		if (bo->bdev->driver->access_memory)
			ret = bo->bdev->driver->access_memory(
				bo, offset, buf, len, write);
		else
			ret = -EIO;
	}

	ttm_bo_unreserve(bo);

	return ret;
}

static const struct vm_operations_struct ttm_bo_vm_ops = {
	.fault = ttm_bo_vm_fault,
	.open = ttm_bo_vm_open,
	.close = ttm_bo_vm_close,
	.access = ttm_bo_vm_access
};
#endif

void
ttm_bo_vm_reference(struct uvm_object *uobj)
{
	struct ttm_buffer_object *bo =
	    (struct ttm_buffer_object *)uobj;

	ttm_bo_get(bo);
	uobj->uo_refs++;
}

void
ttm_bo_vm_detach(struct uvm_object *uobj)
{
	struct ttm_buffer_object *bo = (struct ttm_buffer_object *)uobj;

	uobj->uo_refs--;
	ttm_bo_unref(&bo);
}

struct uvm_pagerops ttm_bo_vm_ops = {
	.pgo_fault = ttm_bo_vm_fault,
	.pgo_reference = ttm_bo_vm_reference,
	.pgo_detach = ttm_bo_vm_detach
};

static struct ttm_buffer_object *ttm_bo_vm_lookup(struct ttm_bo_device *bdev,
						  unsigned long offset,
						  unsigned long pages)
{
	struct drm_vma_offset_node *node;
	struct ttm_buffer_object *bo = NULL;

	drm_vma_offset_lock_lookup(&bdev->vma_manager);

	node = drm_vma_offset_lookup_locked(&bdev->vma_manager, offset, pages);
	if (likely(node)) {
		bo = container_of(node, struct ttm_buffer_object, vma_node);
		if (!kref_get_unless_zero(&bo->kref))
			bo = NULL;
	}

	drm_vma_offset_unlock_lookup(&bdev->vma_manager);

	if (!bo)
		pr_err("Could not find buffer object to map\n");

	return bo;
}

#ifdef __linux__
int ttm_bo_mmap(struct file *filp, struct vm_area_struct *vma,
		struct ttm_bo_device *bdev)
{
	struct ttm_bo_driver *driver;
	struct ttm_buffer_object *bo;
	int ret;

	bo = ttm_bo_vm_lookup(bdev, vma->vm_pgoff, vma_pages(vma));
	if (unlikely(!bo))
		return -EINVAL;

	driver = bo->bdev->driver;
	if (unlikely(!driver->verify_access)) {
		ret = -EPERM;
		goto out_unref;
	}
	ret = driver->verify_access(bo, filp);
	if (unlikely(ret != 0))
		goto out_unref;

	vma->vm_ops = &ttm_bo_vm_ops;

	/*
	 * Note: We're transferring the bo reference to
	 * vma->vm_private_data here.
	 */

	vma->vm_private_data = bo;

	/*
	 * We'd like to use VM_PFNMAP on shared mappings, where
	 * (vma->vm_flags & VM_SHARED) != 0, for performance reasons,
	 * but for some reason VM_PFNMAP + x86 PAT + write-combine is very
	 * bad for performance. Until that has been sorted out, use
	 * VM_MIXEDMAP on all mappings. See freedesktop.org bug #75719
	 */
	vma->vm_flags |= VM_MIXEDMAP;
	vma->vm_flags |= VM_IO | VM_DONTEXPAND | VM_DONTDUMP;
	return 0;
out_unref:
	ttm_bo_put(bo);
	return ret;
}
#else
struct uvm_object *
ttm_bo_mmap(voff_t off, vsize_t size, struct ttm_bo_device *bdev)
{
	struct ttm_bo_driver *driver;
	struct ttm_buffer_object *bo;
	int ret;

	bo = ttm_bo_vm_lookup(bdev, off >> PAGE_SHIFT, size >> PAGE_SHIFT);
	if (unlikely(!bo))
		return NULL;

	driver = bo->bdev->driver;
	if (unlikely(!driver->verify_access)) {
		ret = -EPERM;
		goto out_unref;
	}
#ifdef notyet
	ret = driver->verify_access(bo, filp);
	if (unlikely(ret != 0))
		goto out_unref;
#endif

	bo->uobj.pgops = &ttm_bo_vm_ops;
	bo->uobj.uo_refs++;
	return &bo->uobj;
out_unref:
	ttm_bo_put(bo);
	return NULL;
}
#endif
EXPORT_SYMBOL(ttm_bo_mmap);

#ifdef notyet
int ttm_fbdev_mmap(struct vm_area_struct *vma, struct ttm_buffer_object *bo)
{
	if (vma->vm_pgoff != 0)
		return -EACCES;

	ttm_bo_get(bo);

	vma->vm_ops = &ttm_bo_vm_ops;
	vma->vm_private_data = bo;
	vma->vm_flags |= VM_MIXEDMAP;
	vma->vm_flags |= VM_IO | VM_DONTEXPAND;
	return 0;
}
EXPORT_SYMBOL(ttm_fbdev_mmap);
#endif
