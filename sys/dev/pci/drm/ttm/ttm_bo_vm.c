/*	$OpenBSD: ttm_bo_vm.c,v 1.13 2018/04/20 16:09:37 deraadt Exp $	*/
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

#include <dev/pci/drm/ttm/ttm_module.h>
#include <dev/pci/drm/ttm/ttm_bo_driver.h>
#include <dev/pci/drm/ttm/ttm_placement.h>
#include <dev/pci/drm/drm_vma_manager.h>

#define TTM_BO_VM_NUM_PREFAULT 16

ssize_t	 ttm_bo_fbdev_io(struct ttm_buffer_object *, const char __user *,
	     char __user *, size_t, off_t *, bool);

int ttm_bo_vm_fault(struct uvm_faultinfo *, vaddr_t, vm_page_t *,
        int, int, vm_fault_t, vm_prot_t, int);
void ttm_bo_vm_reference(struct uvm_object *);
void ttm_bo_vm_detach(struct uvm_object *);

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
	boolean_t locked = TRUE;
	int ret;
	int i;
	unsigned long address = (unsigned long)vaddr;
	int retval = VM_PAGER_OK;
	struct ttm_mem_type_manager *man =
		&bdev->man[bo->mem.mem_type];

	/*
	 * Work around locking order reversal in fault / nopfn
	 * between mmap_sem and bo_reserve: Perform a trylock operation
	 * for reserve, and if it fails, retry the fault after scheduling.
	 */

	ret = ttm_bo_reserve(bo, true, true, false, 0);
	if (unlikely(ret != 0)) {
		uvmfault_unlockall(ufi, NULL, uobj, NULL);
		ret = ttm_bo_reserve(bo, true, false, false, 0);
		locked = uvmfault_relock(ufi);
		if (!locked)
			return VM_PAGER_REFAULT;
	}

	if (bdev->driver->fault_reserve_notify) {
		ret = bdev->driver->fault_reserve_notify(bo);
		switch (ret) {
		case 0:
			break;
		case -EBUSY:
#if 0
			set_need_resched();
#else
			printf("resched?\n");
#endif
		case -ERESTARTSYS:
			retval = VM_PAGER_REFAULT;
			goto out_unlock;
		default:
			retval = VM_PAGER_ERROR;
			goto out_unlock;
		}
	}

	/*
	 * Wait for buffer data in transit, due to a pipelined
	 * move.
	 */

	spin_lock(&bdev->fence_lock);
	if (test_bit(TTM_BO_PRIV_FLAG_MOVING, &bo->priv_flags)) {
		ret = ttm_bo_wait(bo, false, true, false);
		spin_unlock(&bdev->fence_lock);
		if (unlikely(ret != 0)) {
			retval = (ret != -ERESTARTSYS) ?
			    VM_PAGER_ERROR : VM_PAGER_REFAULT;
			goto out_unlock;
		}
	} else
		spin_unlock(&bdev->fence_lock);

	ret = ttm_mem_io_lock(man, true);
	if (unlikely(ret != 0)) {
		retval = VM_PAGER_REFAULT;
		goto out_unlock;
	}
	ret = ttm_mem_io_reserve_vm(bo);
	if (unlikely(ret != 0)) {
		retval = VM_PAGER_ERROR;
		goto out_io_unlock;
	}

	page_offset = ((address - ufi->entry->start) >> PAGE_SHIFT) +
	    drm_vma_node_start(&bo->vma_node) - (ufi->entry->offset >> PAGE_SHIFT);
	page_last = ((ufi->entry->end - ufi->entry->start) >> PAGE_SHIFT) +
	    drm_vma_node_start(&bo->vma_node) - (ufi->entry->offset >> PAGE_SHIFT);

	if (unlikely(page_offset >= bo->num_pages)) {
		retval = VM_PAGER_ERROR;
		goto out_io_unlock;
	}

	/*
	 * Strictly, we're not allowed to modify vma->vm_page_prot here,
	 * since the mmap_sem is only held in read mode. However, we
	 * modify only the caching bits of vma->vm_page_prot and
	 * consider those bits protected by
	 * the bo->rwlock, as we should be the only writers.
	 * There shouldn't really be any readers of these bits except
	 * within vm_insert_mixed()? fork?
	 *
	 * TODO: Add a list of vmas to the bo, and change the
	 * vma->vm_page_prot when the object changes caching policy, with
	 * the correct locks held.
	 */
	mapprot = ufi->entry->protection;
	if (bo->mem.bus.is_iomem) {
		pmap_flags = ttm_io_prot(bo->mem.placement, 0);
	} else {
		ttm = bo->ttm;
		pmap_flags = (bo->mem.placement & TTM_PL_FLAG_CACHED) ?
		    0 : ttm_io_prot(bo->mem.placement, 0);

		/* Allocate all page at once, most common usage */
		if (ttm->bdev->driver->ttm_tt_populate(ttm)) {
			retval = VM_PAGER_ERROR;
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
				retval = VM_PAGER_ERROR;
				goto out_io_unlock;
			} else if (unlikely(!page)) {
				break;
			}
			paddr = VM_PAGE_TO_PHYS(page);
		}

		ret = pmap_enter(ufi->orig_map->pmap, vaddr,
		    paddr | pmap_flags, mapprot, PMAP_CANFAIL | mapprot);

		/*
		 * Somebody beat us to this PTE or prefaulting to
		 * an already populated PTE, or prefaulting error.
		 */

		if (ret != 0 && i > 0)
			break;
		else if (unlikely(ret != 0)) {
			uvmfault_unlockall(ufi, ufi->entry->aref.ar_amap,
			    NULL, NULL);
			uvm_wait("ttmflt");
			return VM_PAGER_REFAULT;
		}

		address += PAGE_SIZE;
		vaddr += PAGE_SIZE;
		if (unlikely(++page_offset >= page_last))
			break;
	}
	pmap_update(ufi->orig_map->pmap);
out_io_unlock:
	ttm_mem_io_unlock(man);
out_unlock:
	uvmfault_unlockall(ufi, ufi->entry->aref.ar_amap, NULL, NULL);
	ttm_bo_unreserve(bo);
	return retval;
}

void
ttm_bo_vm_reference(struct uvm_object *uobj)
{
	struct ttm_buffer_object *bo =
	    (struct ttm_buffer_object *)uobj;

	(void)ttm_bo_reference(bo);
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

struct uvm_object *
ttm_bo_mmap(voff_t off, vsize_t size, struct ttm_bo_device *bdev)
{
	struct ttm_bo_driver *driver;
	struct ttm_buffer_object *bo;
	int ret;

	bo = ttm_bo_vm_lookup(bdev, off >> PAGE_SHIFT, size >> PAGE_SHIFT);
	if (unlikely(!bo)) {
 		ret = -EINVAL;
		return NULL;
	}

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

#if 0
	/*
	 * Note: We're transferring the bo reference to
	 * vma->vm_private_data here.
	 */

	vma->vm_private_data = bo;
	vma->vm_flags |= VM_IO | VM_MIXEDMAP | VM_DONTEXPAND | VM_DONTDUMP;
#else
	bo->uobj.uo_refs++;
#endif
	return &bo->uobj;
out_unref:
	ttm_bo_unref(&bo);
	return NULL;
}
EXPORT_SYMBOL(ttm_bo_mmap);

#ifdef notyet
int ttm_fbdev_mmap(struct vm_area_struct *vma, struct ttm_buffer_object *bo)
{
	if (vma->vm_pgoff != 0)
		return -EACCES;

	vma->vm_ops = &ttm_bo_vm_ops;
	vma->vm_private_data = ttm_bo_reference(bo);
	vma->vm_flags |= VM_IO | VM_MIXEDMAP | VM_DONTEXPAND;
	return 0;
}
#endif
EXPORT_SYMBOL(ttm_fbdev_mmap);


ssize_t ttm_bo_io(struct ttm_bo_device *bdev, struct file *filp,
		  const char __user *wbuf, char __user *rbuf, size_t count,
		  off_t *f_pos, bool write)
{
	struct ttm_buffer_object *bo;
	struct ttm_bo_driver *driver;
	struct ttm_bo_kmap_obj map;
	unsigned long dev_offset = (*f_pos >> PAGE_SHIFT);
	unsigned long kmap_offset;
	unsigned long kmap_end;
	unsigned long kmap_num;
	size_t io_size;
	unsigned int page_offset;
	char *virtual;
	int ret;
	bool no_wait = false;
	bool dummy;

	bo = ttm_bo_vm_lookup(bdev, dev_offset, 1);
	if (unlikely(bo == NULL))
		return -EFAULT;

	driver = bo->bdev->driver;
	if (unlikely(!driver->verify_access)) {
		ret = -EPERM;
		goto out_unref;
	}

	ret = driver->verify_access(bo, filp);
	if (unlikely(ret != 0))
		goto out_unref;

	kmap_offset = dev_offset - drm_vma_node_start(&bo->vma_node);
	if (unlikely(kmap_offset >= bo->num_pages)) {
		ret = -EFBIG;
		goto out_unref;
	}

	page_offset = *f_pos & PAGE_MASK;
	io_size = bo->num_pages - kmap_offset;
	io_size = (io_size << PAGE_SHIFT) - page_offset;
	if (count < io_size)
		io_size = count;

	kmap_end = (*f_pos + count - 1) >> PAGE_SHIFT;
	kmap_num = kmap_end - kmap_offset + 1;

	ret = ttm_bo_reserve(bo, true, no_wait, false, 0);

	switch (ret) {
	case 0:
		break;
	case -EBUSY:
		ret = -EAGAIN;
		goto out_unref;
	default:
		goto out_unref;
	}

	ret = ttm_bo_kmap(bo, kmap_offset, kmap_num, &map);
	if (unlikely(ret != 0)) {
		ttm_bo_unreserve(bo);
		goto out_unref;
	}

	virtual = ttm_kmap_obj_virtual(&map, &dummy);
	virtual += page_offset;

	if (write)
		ret = copy_from_user(virtual, wbuf, io_size);
	else
		ret = copy_to_user(rbuf, virtual, io_size);

	ttm_bo_kunmap(&map);
	ttm_bo_unreserve(bo);
	ttm_bo_unref(&bo);

	if (unlikely(ret != 0))
		return -EFBIG;

	*f_pos += io_size;

	return io_size;
out_unref:
	ttm_bo_unref(&bo);
	return ret;
}

ssize_t ttm_bo_fbdev_io(struct ttm_buffer_object *bo, const char __user *wbuf,
			char __user *rbuf, size_t count, off_t *f_pos,
			bool write)
{
	struct ttm_bo_kmap_obj map;
	unsigned long kmap_offset;
	unsigned long kmap_end;
	unsigned long kmap_num;
	size_t io_size;
	unsigned int page_offset;
	char *virtual;
	int ret;
	bool no_wait = false;
	bool dummy;

	kmap_offset = (*f_pos >> PAGE_SHIFT);
	if (unlikely(kmap_offset >= bo->num_pages))
		return -EFBIG;

	page_offset = *f_pos & PAGE_MASK;
	io_size = bo->num_pages - kmap_offset;
	io_size = (io_size << PAGE_SHIFT) - page_offset;
	if (count < io_size)
		io_size = count;

	kmap_end = (*f_pos + count - 1) >> PAGE_SHIFT;
	kmap_num = kmap_end - kmap_offset + 1;

	ret = ttm_bo_reserve(bo, true, no_wait, false, 0);

	switch (ret) {
	case 0:
		break;
	case -EBUSY:
		return -EAGAIN;
	default:
		return ret;
	}

	ret = ttm_bo_kmap(bo, kmap_offset, kmap_num, &map);
	if (unlikely(ret != 0)) {
		ttm_bo_unreserve(bo);
		return ret;
	}

	virtual = ttm_kmap_obj_virtual(&map, &dummy);
	virtual += page_offset;

	if (write)
		ret = copy_from_user(virtual, wbuf, io_size);
	else
		ret = copy_to_user(rbuf, virtual, io_size);

	ttm_bo_kunmap(&map);
	ttm_bo_unreserve(bo);
	ttm_bo_unref(&bo);

	if (unlikely(ret != 0))
		return ret;

	*f_pos += io_size;

	return io_size;
}
