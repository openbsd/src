/*
 * Copyright 2008 Advanced Micro Devices, Inc.
 * Copyright 2008 Red Hat Inc.
 * Copyright 2009 Jerome Glisse.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors: Dave Airlie
 *          Alex Deucher
 *          Jerome Glisse
 */
#include <linux/ktime.h>
#include <linux/pagemap.h>
#include <drm/drmP.h>
#include <drm/amdgpu_drm.h>
#include "amdgpu.h"
#include "amdgpu_display.h"

void amdgpu_gem_object_free(struct drm_gem_object *gobj)
{
	struct amdgpu_bo *robj = gem_to_amdgpu_bo(gobj);

	if (robj) {
		amdgpu_mn_unregister(robj);
		amdgpu_bo_unref(&robj);
	}
}

int amdgpu_gem_object_create(struct amdgpu_device *adev, unsigned long size,
			     int alignment, u32 initial_domain,
			     u64 flags, enum ttm_bo_type type,
			     struct reservation_object *resv,
			     struct drm_gem_object **obj)
{
	struct amdgpu_bo *bo;
	struct amdgpu_bo_param bp;
	int r;

	memset(&bp, 0, sizeof(bp));
	*obj = NULL;
	/* At least align on page size */
	if (alignment < PAGE_SIZE) {
		alignment = PAGE_SIZE;
	}

	bp.size = size;
	bp.byte_align = alignment;
	bp.type = type;
	bp.resv = resv;
	bp.preferred_domain = initial_domain;
retry:
	bp.flags = flags;
	bp.domain = initial_domain;
	r = amdgpu_bo_create(adev, &bp, &bo);
	if (r) {
		if (r != -ERESTARTSYS) {
			if (flags & AMDGPU_GEM_CREATE_CPU_ACCESS_REQUIRED) {
				flags &= ~AMDGPU_GEM_CREATE_CPU_ACCESS_REQUIRED;
				goto retry;
			}

			if (initial_domain == AMDGPU_GEM_DOMAIN_VRAM) {
				initial_domain |= AMDGPU_GEM_DOMAIN_GTT;
				goto retry;
			}
			DRM_DEBUG("Failed to allocate GEM object (%ld, %d, %u, %d)\n",
				  size, initial_domain, alignment, r);
		}
		return r;
	}
	*obj = &bo->gem_base;

	return 0;
}

void amdgpu_gem_force_release(struct amdgpu_device *adev)
{
	STUB();
#if 0
	struct drm_device *ddev = adev->ddev;
	struct drm_file *file;

	mutex_lock(&ddev->filelist_mutex);

	list_for_each_entry(file, &ddev->filelist, lhead) {
		struct drm_gem_object *gobj;
		int handle;

		WARN_ONCE(1, "Still active user space clients!\n");
		spin_lock(&file->table_lock);
		idr_for_each_entry(&file->object_idr, gobj, handle) {
			WARN_ONCE(1, "And also active allocations!\n");
			drm_gem_object_put_unlocked(gobj);
		}
		idr_destroy(&file->object_idr);
		spin_unlock(&file->table_lock);
	}

	mutex_unlock(&ddev->filelist_mutex);
#endif
}

/*
 * Call from drm_gem_handle_create which appear in both new and open ioctl
 * case.
 */
int amdgpu_gem_object_open(struct drm_gem_object *obj,
			   struct drm_file *file_priv)
{
	struct amdgpu_bo *abo = gem_to_amdgpu_bo(obj);
	struct amdgpu_device *adev = amdgpu_ttm_adev(abo->tbo.bdev);
	struct amdgpu_fpriv *fpriv = file_priv->driver_priv;
	struct amdgpu_vm *vm = &fpriv->vm;
	struct amdgpu_bo_va *bo_va;
#ifdef notyet
	struct mm_struct *mm;
#endif
	int r;

#ifdef notyet
	mm = amdgpu_ttm_tt_get_usermm(abo->tbo.ttm);
	if (mm && mm != current->mm)
		return -EPERM;
#endif

	if (abo->flags & AMDGPU_GEM_CREATE_VM_ALWAYS_VALID &&
	    abo->tbo.resv != vm->root.base.bo->tbo.resv)
		return -EPERM;

	r = amdgpu_bo_reserve(abo, false);
	if (r)
		return r;

	bo_va = amdgpu_vm_bo_find(vm, abo);
	if (!bo_va) {
		bo_va = amdgpu_vm_bo_add(adev, vm, abo);
	} else {
		++bo_va->ref_count;
	}
	amdgpu_bo_unreserve(abo);
	return 0;
}

void amdgpu_gem_object_close(struct drm_gem_object *obj,
			     struct drm_file *file_priv)
{
	struct amdgpu_bo *bo = gem_to_amdgpu_bo(obj);
	struct amdgpu_device *adev = amdgpu_ttm_adev(bo->tbo.bdev);
	struct amdgpu_fpriv *fpriv = file_priv->driver_priv;
	struct amdgpu_vm *vm = &fpriv->vm;

	struct amdgpu_bo_list_entry vm_pd;
	struct list_head list, duplicates;
	struct ttm_validate_buffer tv;
	struct ww_acquire_ctx ticket;
	struct amdgpu_bo_va *bo_va;
	int r;

	INIT_LIST_HEAD(&list);
	INIT_LIST_HEAD(&duplicates);

	tv.bo = &bo->tbo;
	tv.shared = true;
	list_add(&tv.head, &list);

	amdgpu_vm_get_pd_bo(vm, &list, &vm_pd);

	r = ttm_eu_reserve_buffers(&ticket, &list, false, &duplicates);
	if (r) {
		dev_err(adev->dev, "leaking bo va because "
			"we fail to reserve bo (%d)\n", r);
		return;
	}
	bo_va = amdgpu_vm_bo_find(vm, bo);
	if (bo_va && --bo_va->ref_count == 0) {
		amdgpu_vm_bo_rmv(adev, bo_va);

		if (amdgpu_vm_ready(vm)) {
			struct dma_fence *fence = NULL;

			r = amdgpu_vm_clear_freed(adev, vm, &fence);
			if (unlikely(r)) {
				dev_err(adev->dev, "failed to clear page "
					"tables on GEM object close (%d)\n", r);
			}

			if (fence) {
				amdgpu_bo_fence(bo, fence, true);
				dma_fence_put(fence);
			}
		}
	}
	ttm_eu_backoff_reservation(&ticket, &list);
}

/*
 * GEM ioctls.
 */
int amdgpu_gem_create_ioctl(struct drm_device *dev, void *data,
			    struct drm_file *filp)
{
	struct amdgpu_device *adev = dev->dev_private;
	struct amdgpu_fpriv *fpriv = filp->driver_priv;
	struct amdgpu_vm *vm = &fpriv->vm;
	union drm_amdgpu_gem_create *args = data;
	uint64_t flags = args->in.domain_flags;
	uint64_t size = args->in.bo_size;
	struct reservation_object *resv = NULL;
	struct drm_gem_object *gobj;
	uint32_t handle;
	int r;

	/* reject invalid gem flags */
	if (flags & ~(AMDGPU_GEM_CREATE_CPU_ACCESS_REQUIRED |
		      AMDGPU_GEM_CREATE_NO_CPU_ACCESS |
		      AMDGPU_GEM_CREATE_CPU_GTT_USWC |
		      AMDGPU_GEM_CREATE_VRAM_CLEARED |
		      AMDGPU_GEM_CREATE_VM_ALWAYS_VALID |
		      AMDGPU_GEM_CREATE_EXPLICIT_SYNC))

		return -EINVAL;

	/* reject invalid gem domains */
	if (args->in.domains & ~AMDGPU_GEM_DOMAIN_MASK)
		return -EINVAL;

	/* create a gem object to contain this object in */
	if (args->in.domains & (AMDGPU_GEM_DOMAIN_GDS |
	    AMDGPU_GEM_DOMAIN_GWS | AMDGPU_GEM_DOMAIN_OA)) {
		if (flags & AMDGPU_GEM_CREATE_VM_ALWAYS_VALID) {
			/* if gds bo is created from user space, it must be
			 * passed to bo list
			 */
			DRM_ERROR("GDS bo cannot be per-vm-bo\n");
			return -EINVAL;
		}
		flags |= AMDGPU_GEM_CREATE_NO_CPU_ACCESS;
		if (args->in.domains == AMDGPU_GEM_DOMAIN_GDS)
			size = size << AMDGPU_GDS_SHIFT;
		else if (args->in.domains == AMDGPU_GEM_DOMAIN_GWS)
			size = size << AMDGPU_GWS_SHIFT;
		else if (args->in.domains == AMDGPU_GEM_DOMAIN_OA)
			size = size << AMDGPU_OA_SHIFT;
		else
			return -EINVAL;
	}
	size = roundup(size, PAGE_SIZE);

	if (flags & AMDGPU_GEM_CREATE_VM_ALWAYS_VALID) {
		r = amdgpu_bo_reserve(vm->root.base.bo, false);
		if (r)
			return r;

		resv = vm->root.base.bo->tbo.resv;
	}

	r = amdgpu_gem_object_create(adev, size, args->in.alignment,
				     (u32)(0xffffffff & args->in.domains),
				     flags, ttm_bo_type_device, resv, &gobj);
	if (flags & AMDGPU_GEM_CREATE_VM_ALWAYS_VALID) {
		if (!r) {
			struct amdgpu_bo *abo = gem_to_amdgpu_bo(gobj);

			abo->parent = amdgpu_bo_ref(vm->root.base.bo);
		}
		amdgpu_bo_unreserve(vm->root.base.bo);
	}
	if (r)
		return r;

	r = drm_gem_handle_create(filp, gobj, &handle);
	/* drop reference from allocate - handle holds it now */
	drm_gem_object_put_unlocked(gobj);
	if (r)
		return r;

	memset(args, 0, sizeof(*args));
	args->out.handle = handle;
	return 0;
}

int amdgpu_gem_userptr_ioctl(struct drm_device *dev, void *data,
			     struct drm_file *filp)
{
	STUB();
	return -ENOSYS;
#if 0
	struct ttm_operation_ctx ctx = { true, false };
	struct amdgpu_device *adev = dev->dev_private;
	struct drm_amdgpu_gem_userptr *args = data;
	struct drm_gem_object *gobj;
	struct amdgpu_bo *bo;
	uint32_t handle;
	int r;

	if (offset_in_page(args->addr | args->size))
		return -EINVAL;

	/* reject unknown flag values */
	if (args->flags & ~(AMDGPU_GEM_USERPTR_READONLY |
	    AMDGPU_GEM_USERPTR_ANONONLY | AMDGPU_GEM_USERPTR_VALIDATE |
	    AMDGPU_GEM_USERPTR_REGISTER))
		return -EINVAL;

	if (!(args->flags & AMDGPU_GEM_USERPTR_READONLY) &&
	     !(args->flags & AMDGPU_GEM_USERPTR_REGISTER)) {

		/* if we want to write to it we must install a MMU notifier */
		return -EACCES;
	}

	/* create a gem object to contain this object in */
	r = amdgpu_gem_object_create(adev, args->size, 0, AMDGPU_GEM_DOMAIN_CPU,
				     0, ttm_bo_type_device, NULL, &gobj);
	if (r)
		return r;

	bo = gem_to_amdgpu_bo(gobj);
	bo->preferred_domains = AMDGPU_GEM_DOMAIN_GTT;
	bo->allowed_domains = AMDGPU_GEM_DOMAIN_GTT;
	r = amdgpu_ttm_tt_set_userptr(bo->tbo.ttm, args->addr, args->flags);
	if (r)
		goto release_object;

	if (args->flags & AMDGPU_GEM_USERPTR_REGISTER) {
		r = amdgpu_mn_register(bo, args->addr);
		if (r)
			goto release_object;
	}

	if (args->flags & AMDGPU_GEM_USERPTR_VALIDATE) {
		r = amdgpu_ttm_tt_get_user_pages(bo->tbo.ttm,
						 bo->tbo.ttm->pages);
		if (r)
			goto release_object;

		r = amdgpu_bo_reserve(bo, true);
		if (r)
			goto free_pages;

		amdgpu_bo_placement_from_domain(bo, AMDGPU_GEM_DOMAIN_GTT);
		r = ttm_bo_validate(&bo->tbo, &bo->placement, &ctx);
		amdgpu_bo_unreserve(bo);
		if (r)
			goto free_pages;
	}

	r = drm_gem_handle_create(filp, gobj, &handle);
	/* drop reference from allocate - handle holds it now */
	drm_gem_object_put_unlocked(gobj);
	if (r)
		return r;

	args->handle = handle;
	return 0;

free_pages:
	release_pages(bo->tbo.ttm->pages, bo->tbo.ttm->num_pages);

release_object:
	drm_gem_object_put_unlocked(gobj);

	return r;
#endif
}

int amdgpu_mode_dumb_mmap(struct drm_file *filp,
			  struct drm_device *dev,
			  uint32_t handle, uint64_t *offset_p)
{
	struct drm_gem_object *gobj;
	struct amdgpu_bo *robj;

	gobj = drm_gem_object_lookup(filp, handle);
	if (gobj == NULL) {
		return -ENOENT;
	}
	robj = gem_to_amdgpu_bo(gobj);
	if (amdgpu_ttm_tt_get_usermm(robj->tbo.ttm) ||
	    (robj->flags & AMDGPU_GEM_CREATE_NO_CPU_ACCESS)) {
		drm_gem_object_put_unlocked(gobj);
		return -EPERM;
	}
	*offset_p = amdgpu_bo_mmap_offset(robj);
	drm_gem_object_put_unlocked(gobj);
	return 0;
}

int amdgpu_gem_mmap_ioctl(struct drm_device *dev, void *data,
			  struct drm_file *filp)
{
	union drm_amdgpu_gem_mmap *args = data;
	uint32_t handle = args->in.handle;
	memset(args, 0, sizeof(*args));
	return amdgpu_mode_dumb_mmap(filp, dev, handle, &args->out.addr_ptr);
}

/**
 * amdgpu_gem_timeout - calculate jiffies timeout from absolute value
 *
 * @timeout_ns: timeout in ns
 *
 * Calculate the timeout in jiffies from an absolute timeout in ns.
 */
unsigned long amdgpu_gem_timeout(uint64_t timeout_ns)
{
	unsigned long timeout_jiffies;
	ktime_t timeout;

	/* clamp timeout if it's to large */
	if (((int64_t)timeout_ns) < 0)
		return MAX_SCHEDULE_TIMEOUT;

	timeout = ktime_sub(ns_to_ktime(timeout_ns), ktime_get());
	if (ktime_to_ns(timeout) < 0)
		return 0;

	timeout_jiffies = nsecs_to_jiffies(ktime_to_ns(timeout));
	/*  clamp timeout to avoid unsigned-> signed overflow */
	if (timeout_jiffies > MAX_SCHEDULE_TIMEOUT )
		return MAX_SCHEDULE_TIMEOUT - 1;

	return timeout_jiffies;
}

int amdgpu_gem_wait_idle_ioctl(struct drm_device *dev, void *data,
			      struct drm_file *filp)
{
	union drm_amdgpu_gem_wait_idle *args = data;
	struct drm_gem_object *gobj;
	struct amdgpu_bo *robj;
	uint32_t handle = args->in.handle;
	unsigned long timeout = amdgpu_gem_timeout(args->in.timeout);
	int r = 0;
	long ret;

	gobj = drm_gem_object_lookup(filp, handle);
	if (gobj == NULL) {
		return -ENOENT;
	}
	robj = gem_to_amdgpu_bo(gobj);
	ret = reservation_object_wait_timeout_rcu(robj->tbo.resv, true, true,
						  timeout);

	/* ret == 0 means not signaled,
	 * ret > 0 means signaled
	 * ret < 0 means interrupted before timeout
	 */
	if (ret >= 0) {
		memset(args, 0, sizeof(*args));
		args->out.status = (ret == 0);
	} else
		r = ret;

	drm_gem_object_put_unlocked(gobj);
	return r;
}

int amdgpu_gem_metadata_ioctl(struct drm_device *dev, void *data,
				struct drm_file *filp)
{
	struct drm_amdgpu_gem_metadata *args = data;
	struct drm_gem_object *gobj;
	struct amdgpu_bo *robj;
	int r = -1;

	DRM_DEBUG("%d \n", args->handle);
	gobj = drm_gem_object_lookup(filp, args->handle);
	if (gobj == NULL)
		return -ENOENT;
	robj = gem_to_amdgpu_bo(gobj);

	r = amdgpu_bo_reserve(robj, false);
	if (unlikely(r != 0))
		goto out;

	if (args->op == AMDGPU_GEM_METADATA_OP_GET_METADATA) {
		amdgpu_bo_get_tiling_flags(robj, &args->data.tiling_info);
		r = amdgpu_bo_get_metadata(robj, args->data.data,
					   sizeof(args->data.data),
					   &args->data.data_size_bytes,
					   &args->data.flags);
	} else if (args->op == AMDGPU_GEM_METADATA_OP_SET_METADATA) {
		if (args->data.data_size_bytes > sizeof(args->data.data)) {
			r = -EINVAL;
			goto unreserve;
		}
		r = amdgpu_bo_set_tiling_flags(robj, args->data.tiling_info);
		if (!r)
			r = amdgpu_bo_set_metadata(robj, args->data.data,
						   args->data.data_size_bytes,
						   args->data.flags);
	}

unreserve:
	amdgpu_bo_unreserve(robj);
out:
	drm_gem_object_put_unlocked(gobj);
	return r;
}

/**
 * amdgpu_gem_va_update_vm -update the bo_va in its VM
 *
 * @adev: amdgpu_device pointer
 * @vm: vm to update
 * @bo_va: bo_va to update
 * @operation: map, unmap or clear
 *
 * Update the bo_va directly after setting its address. Errors are not
 * vital here, so they are not reported back to userspace.
 */
static void amdgpu_gem_va_update_vm(struct amdgpu_device *adev,
				    struct amdgpu_vm *vm,
				    struct amdgpu_bo_va *bo_va,
				    uint32_t operation)
{
	int r;

	if (!amdgpu_vm_ready(vm))
		return;

	r = amdgpu_vm_clear_freed(adev, vm, NULL);
	if (r)
		goto error;

	if (operation == AMDGPU_VA_OP_MAP ||
	    operation == AMDGPU_VA_OP_REPLACE) {
		r = amdgpu_vm_bo_update(adev, bo_va, false);
		if (r)
			goto error;
	}

	r = amdgpu_vm_update_directories(adev, vm);

error:
	if (r && r != -ERESTARTSYS)
		DRM_ERROR("Couldn't update BO_VA (%d)\n", r);
}

int amdgpu_gem_va_ioctl(struct drm_device *dev, void *data,
			  struct drm_file *filp)
{
	const uint32_t valid_flags = AMDGPU_VM_DELAY_UPDATE |
		AMDGPU_VM_PAGE_READABLE | AMDGPU_VM_PAGE_WRITEABLE |
		AMDGPU_VM_PAGE_EXECUTABLE | AMDGPU_VM_MTYPE_MASK;
	const uint32_t prt_flags = AMDGPU_VM_DELAY_UPDATE |
		AMDGPU_VM_PAGE_PRT;

	struct drm_amdgpu_gem_va *args = data;
	struct drm_gem_object *gobj;
	struct amdgpu_device *adev = dev->dev_private;
	struct amdgpu_fpriv *fpriv = filp->driver_priv;
	struct amdgpu_bo *abo;
	struct amdgpu_bo_va *bo_va;
	struct amdgpu_bo_list_entry vm_pd;
	struct ttm_validate_buffer tv;
	struct ww_acquire_ctx ticket;
	struct list_head list, duplicates;
	uint64_t va_flags;
	int r = 0;

	if (args->va_address < AMDGPU_VA_RESERVED_SIZE) {
		dev_dbg(&dev->pdev->dev,
			"va_address 0x%LX is in reserved area 0x%LX\n",
			args->va_address, AMDGPU_VA_RESERVED_SIZE);
		return -EINVAL;
	}

	if (args->va_address >= AMDGPU_VA_HOLE_START &&
	    args->va_address < AMDGPU_VA_HOLE_END) {
		dev_dbg(&dev->pdev->dev,
			"va_address 0x%LX is in VA hole 0x%LX-0x%LX\n",
			args->va_address, AMDGPU_VA_HOLE_START,
			AMDGPU_VA_HOLE_END);
		return -EINVAL;
	}

	args->va_address &= AMDGPU_VA_HOLE_MASK;

	if ((args->flags & ~valid_flags) && (args->flags & ~prt_flags)) {
		dev_dbg(&dev->pdev->dev, "invalid flags combination 0x%08X\n",
			args->flags);
		return -EINVAL;
	}

	switch (args->operation) {
	case AMDGPU_VA_OP_MAP:
	case AMDGPU_VA_OP_UNMAP:
	case AMDGPU_VA_OP_CLEAR:
	case AMDGPU_VA_OP_REPLACE:
		break;
	default:
		dev_dbg(&dev->pdev->dev, "unsupported operation %d\n",
			args->operation);
		return -EINVAL;
	}

	INIT_LIST_HEAD(&list);
	INIT_LIST_HEAD(&duplicates);
	if ((args->operation != AMDGPU_VA_OP_CLEAR) &&
	    !(args->flags & AMDGPU_VM_PAGE_PRT)) {
		gobj = drm_gem_object_lookup(filp, args->handle);
		if (gobj == NULL)
			return -ENOENT;
		abo = gem_to_amdgpu_bo(gobj);
		tv.bo = &abo->tbo;
		tv.shared = !!(abo->flags & AMDGPU_GEM_CREATE_VM_ALWAYS_VALID);
		list_add(&tv.head, &list);
	} else {
		gobj = NULL;
		abo = NULL;
	}

	amdgpu_vm_get_pd_bo(&fpriv->vm, &list, &vm_pd);

	r = ttm_eu_reserve_buffers(&ticket, &list, true, &duplicates);
	if (r)
		goto error_unref;

	if (abo) {
		bo_va = amdgpu_vm_bo_find(&fpriv->vm, abo);
		if (!bo_va) {
			r = -ENOENT;
			goto error_backoff;
		}
	} else if (args->operation != AMDGPU_VA_OP_CLEAR) {
		bo_va = fpriv->prt_va;
	} else {
		bo_va = NULL;
	}

	switch (args->operation) {
	case AMDGPU_VA_OP_MAP:
		r = amdgpu_vm_alloc_pts(adev, bo_va->base.vm, args->va_address,
					args->map_size);
		if (r)
			goto error_backoff;

		va_flags = amdgpu_gmc_get_pte_flags(adev, args->flags);
		r = amdgpu_vm_bo_map(adev, bo_va, args->va_address,
				     args->offset_in_bo, args->map_size,
				     va_flags);
		break;
	case AMDGPU_VA_OP_UNMAP:
		r = amdgpu_vm_bo_unmap(adev, bo_va, args->va_address);
		break;

	case AMDGPU_VA_OP_CLEAR:
		r = amdgpu_vm_bo_clear_mappings(adev, &fpriv->vm,
						args->va_address,
						args->map_size);
		break;
	case AMDGPU_VA_OP_REPLACE:
		r = amdgpu_vm_alloc_pts(adev, bo_va->base.vm, args->va_address,
					args->map_size);
		if (r)
			goto error_backoff;

		va_flags = amdgpu_gmc_get_pte_flags(adev, args->flags);
		r = amdgpu_vm_bo_replace_map(adev, bo_va, args->va_address,
					     args->offset_in_bo, args->map_size,
					     va_flags);
		break;
	default:
		break;
	}
	if (!r && !(args->flags & AMDGPU_VM_DELAY_UPDATE) && !amdgpu_vm_debug)
		amdgpu_gem_va_update_vm(adev, &fpriv->vm, bo_va,
					args->operation);

error_backoff:
	ttm_eu_backoff_reservation(&ticket, &list);

error_unref:
	drm_gem_object_put_unlocked(gobj);
	return r;
}

int amdgpu_gem_op_ioctl(struct drm_device *dev, void *data,
			struct drm_file *filp)
{
	struct amdgpu_device *adev = dev->dev_private;
	struct drm_amdgpu_gem_op *args = data;
	struct drm_gem_object *gobj;
	struct amdgpu_bo *robj;
	int r;

	gobj = drm_gem_object_lookup(filp, args->handle);
	if (gobj == NULL) {
		return -ENOENT;
	}
	robj = gem_to_amdgpu_bo(gobj);

	r = amdgpu_bo_reserve(robj, false);
	if (unlikely(r))
		goto out;

	switch (args->op) {
	case AMDGPU_GEM_OP_GET_GEM_CREATE_INFO: {
		struct drm_amdgpu_gem_create_in info;
		void __user *out = u64_to_user_ptr(args->value);

		info.bo_size = robj->gem_base.size;
		info.alignment = robj->tbo.mem.page_alignment << PAGE_SHIFT;
		info.domains = robj->preferred_domains;
		info.domain_flags = robj->flags;
		amdgpu_bo_unreserve(robj);
		if (copy_to_user(out, &info, sizeof(info)))
			r = -EFAULT;
		break;
	}
	case AMDGPU_GEM_OP_SET_PLACEMENT:
		if (robj->prime_shared_count && (args->value & AMDGPU_GEM_DOMAIN_VRAM)) {
			r = -EINVAL;
			amdgpu_bo_unreserve(robj);
			break;
		}
		if (amdgpu_ttm_tt_get_usermm(robj->tbo.ttm)) {
			r = -EPERM;
			amdgpu_bo_unreserve(robj);
			break;
		}
		robj->preferred_domains = args->value & (AMDGPU_GEM_DOMAIN_VRAM |
							AMDGPU_GEM_DOMAIN_GTT |
							AMDGPU_GEM_DOMAIN_CPU);
		robj->allowed_domains = robj->preferred_domains;
		if (robj->allowed_domains == AMDGPU_GEM_DOMAIN_VRAM)
			robj->allowed_domains |= AMDGPU_GEM_DOMAIN_GTT;

		if (robj->flags & AMDGPU_GEM_CREATE_VM_ALWAYS_VALID)
			amdgpu_vm_bo_invalidate(adev, robj, true);

		amdgpu_bo_unreserve(robj);
		break;
	default:
		amdgpu_bo_unreserve(robj);
		r = -EINVAL;
	}

out:
	drm_gem_object_put_unlocked(gobj);
	return r;
}

int amdgpu_mode_dumb_create(struct drm_file *file_priv,
			    struct drm_device *dev,
			    struct drm_mode_create_dumb *args)
{
	struct amdgpu_device *adev = dev->dev_private;
	struct drm_gem_object *gobj;
	uint32_t handle;
	u32 domain;
	int r;

	args->pitch = amdgpu_align_pitch(adev, args->width,
					 DIV_ROUND_UP(args->bpp, 8), 0);
	args->size = (u64)args->pitch * args->height;
	args->size = roundup2(args->size, PAGE_SIZE);
	domain = amdgpu_bo_get_preferred_pin_domain(adev,
				amdgpu_display_supported_domains(adev));
	r = amdgpu_gem_object_create(adev, args->size, 0, domain,
				     AMDGPU_GEM_CREATE_CPU_ACCESS_REQUIRED,
				     ttm_bo_type_device, NULL, &gobj);
	if (r)
		return -ENOMEM;

	r = drm_gem_handle_create(file_priv, gobj, &handle);
	/* drop reference from allocate - handle holds it now */
	drm_gem_object_put_unlocked(gobj);
	if (r) {
		return r;
	}
	args->handle = handle;
	return 0;
}

#if defined(CONFIG_DEBUG_FS)

#define amdgpu_debugfs_gem_bo_print_flag(m, bo, flag)	\
	if (bo->flags & (AMDGPU_GEM_CREATE_ ## flag)) {	\
		seq_printf((m), " " #flag);		\
	}

static int amdgpu_debugfs_gem_bo_info(int id, void *ptr, void *data)
{
	struct drm_gem_object *gobj = ptr;
	struct amdgpu_bo *bo = gem_to_amdgpu_bo(gobj);
	struct seq_file *m = data;

	struct dma_buf_attachment *attachment;
	struct dma_buf *dma_buf;
	unsigned domain;
	const char *placement;
	unsigned pin_count;

	domain = amdgpu_mem_type_to_domain(bo->tbo.mem.mem_type);
	switch (domain) {
	case AMDGPU_GEM_DOMAIN_VRAM:
		placement = "VRAM";
		break;
	case AMDGPU_GEM_DOMAIN_GTT:
		placement = " GTT";
		break;
	case AMDGPU_GEM_DOMAIN_CPU:
	default:
		placement = " CPU";
		break;
	}
	seq_printf(m, "\t0x%08x: %12ld byte %s",
		   id, amdgpu_bo_size(bo), placement);

	pin_count = READ_ONCE(bo->pin_count);
	if (pin_count)
		seq_printf(m, " pin count %d", pin_count);

	dma_buf = READ_ONCE(bo->gem_base.dma_buf);
	attachment = READ_ONCE(bo->gem_base.import_attach);

	if (attachment)
		seq_printf(m, " imported from %p", dma_buf);
	else if (dma_buf)
		seq_printf(m, " exported as %p", dma_buf);

	amdgpu_debugfs_gem_bo_print_flag(m, bo, CPU_ACCESS_REQUIRED);
	amdgpu_debugfs_gem_bo_print_flag(m, bo, NO_CPU_ACCESS);
	amdgpu_debugfs_gem_bo_print_flag(m, bo, CPU_GTT_USWC);
	amdgpu_debugfs_gem_bo_print_flag(m, bo, VRAM_CLEARED);
	amdgpu_debugfs_gem_bo_print_flag(m, bo, SHADOW);
	amdgpu_debugfs_gem_bo_print_flag(m, bo, VRAM_CONTIGUOUS);
	amdgpu_debugfs_gem_bo_print_flag(m, bo, VM_ALWAYS_VALID);
	amdgpu_debugfs_gem_bo_print_flag(m, bo, EXPLICIT_SYNC);

	seq_printf(m, "\n");

	return 0;
}

static int amdgpu_debugfs_gem_info(struct seq_file *m, void *data)
{
	struct drm_info_node *node = (struct drm_info_node *)m->private;
	struct drm_device *dev = node->minor->dev;
	struct drm_file *file;
	int r;

	r = mutex_lock_interruptible(&dev->filelist_mutex);
	if (r)
		return r;

	list_for_each_entry(file, &dev->filelist, lhead) {
		struct task_struct *task;

		/*
		 * Although we have a valid reference on file->pid, that does
		 * not guarantee that the task_struct who called get_pid() is
		 * still alive (e.g. get_pid(current) => fork() => exit()).
		 * Therefore, we need to protect this ->comm access using RCU.
		 */
		rcu_read_lock();
		task = pid_task(file->pid, PIDTYPE_PID);
		seq_printf(m, "pid %8d command %s:\n", pid_nr(file->pid),
			   task ? task->comm : "<unknown>");
		rcu_read_unlock();

		spin_lock(&file->table_lock);
		idr_for_each(&file->object_idr, amdgpu_debugfs_gem_bo_info, m);
		spin_unlock(&file->table_lock);
	}

	mutex_unlock(&dev->filelist_mutex);
	return 0;
}

static const struct drm_info_list amdgpu_debugfs_gem_list[] = {
	{"amdgpu_gem_info", &amdgpu_debugfs_gem_info, 0, NULL},
};
#endif

int amdgpu_debugfs_gem_init(struct amdgpu_device *adev)
{
#if defined(CONFIG_DEBUG_FS)
	return amdgpu_debugfs_add_files(adev, amdgpu_debugfs_gem_list, 1);
#endif
	return 0;
}
