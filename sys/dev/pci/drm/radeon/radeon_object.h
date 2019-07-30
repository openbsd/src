/*	$OpenBSD: radeon_object.h,v 1.3 2017/06/04 15:06:22 kettenis Exp $	*/
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
#ifndef __RADEON_OBJECT_H__
#define __RADEON_OBJECT_H__

#include <dev/pci/drm/radeon_drm.h>
#include "radeon.h"

/**
 * radeon_mem_type_to_domain - return domain corresponding to mem_type
 * @mem_type:	ttm memory type
 *
 * Returns corresponding domain of the ttm mem_type
 */
static inline unsigned radeon_mem_type_to_domain(u32 mem_type)
{
	switch (mem_type) {
	case TTM_PL_VRAM:
		return RADEON_GEM_DOMAIN_VRAM;
	case TTM_PL_TT:
		return RADEON_GEM_DOMAIN_GTT;
	case TTM_PL_SYSTEM:
		return RADEON_GEM_DOMAIN_CPU;
	default:
		break;
	}
	return 0;
}

int radeon_bo_reserve(struct radeon_bo *bo, bool no_intr);

static inline void radeon_bo_unreserve(struct radeon_bo *bo)
{
	ttm_bo_unreserve(&bo->tbo);
}

/**
 * radeon_bo_gpu_offset - return GPU offset of bo
 * @bo:	radeon object for which we query the offset
 *
 * Returns current GPU offset of the object.
 *
 * Note: object should either be pinned or reserved when calling this
 * function, it might be useful to add check for this for debugging.
 */
static inline u64 radeon_bo_gpu_offset(struct radeon_bo *bo)
{
	return bo->tbo.offset;
}

static inline unsigned long radeon_bo_size(struct radeon_bo *bo)
{
	return bo->tbo.num_pages << PAGE_SHIFT;
}

static inline bool radeon_bo_is_reserved(struct radeon_bo *bo)
{
	return ttm_bo_is_reserved(&bo->tbo);
}

static inline unsigned radeon_bo_ngpu_pages(struct radeon_bo *bo)
{
	return (bo->tbo.num_pages << PAGE_SHIFT) / RADEON_GPU_PAGE_SIZE;
}

static inline unsigned radeon_bo_gpu_page_alignment(struct radeon_bo *bo)
{
	return (bo->tbo.mem.page_alignment << PAGE_SHIFT) / RADEON_GPU_PAGE_SIZE;
}

/**
 * radeon_bo_mmap_offset - return mmap offset of bo
 * @bo:	radeon object for which we query the offset
 *
 * Returns mmap offset of the object.
 */
static inline u64 radeon_bo_mmap_offset(struct radeon_bo *bo)
{
	return drm_vma_node_offset_addr(&bo->tbo.vma_node);
}

extern int radeon_bo_wait(struct radeon_bo *bo, u32 *mem_type,
			  bool no_wait);

extern int radeon_bo_create(struct radeon_device *rdev,
			    unsigned long size, int byte_align,
			    bool kernel, u32 domain,
			    struct sg_table *sg,
			    struct radeon_bo **bo_ptr);
extern int radeon_bo_kmap(struct radeon_bo *bo, void **ptr);
extern void radeon_bo_kunmap(struct radeon_bo *bo);
extern void radeon_bo_unref(struct radeon_bo **bo);
extern int radeon_bo_pin(struct radeon_bo *bo, u32 domain, u64 *gpu_addr);
extern int radeon_bo_pin_restricted(struct radeon_bo *bo, u32 domain,
				    u64 max_offset, u64 *gpu_addr);
extern int radeon_bo_unpin(struct radeon_bo *bo);
extern int radeon_bo_evict_vram(struct radeon_device *rdev);
extern void radeon_bo_force_delete(struct radeon_device *rdev);
extern int radeon_bo_init(struct radeon_device *rdev);
extern void radeon_bo_fini(struct radeon_device *rdev);
extern void radeon_bo_list_add_object(struct radeon_bo_list *lobj,
				struct list_head *head);
extern int radeon_bo_list_validate(struct list_head *head);
#ifdef notyet
extern int radeon_bo_fbdev_mmap(struct radeon_bo *bo,
				struct vm_area_struct *vma);
#endif
extern int radeon_bo_set_tiling_flags(struct radeon_bo *bo,
				u32 tiling_flags, u32 pitch);
extern void radeon_bo_get_tiling_flags(struct radeon_bo *bo,
				u32 *tiling_flags, u32 *pitch);
extern int radeon_bo_check_tiling(struct radeon_bo *bo, bool has_moved,
				bool force_drop);
extern void radeon_bo_move_notify(struct ttm_buffer_object *bo,
					struct ttm_mem_reg *mem);
extern int radeon_bo_fault_reserve_notify(struct ttm_buffer_object *bo);
extern int radeon_bo_get_surface_reg(struct radeon_bo *bo);

/*
 * sub allocation
 */

static inline uint64_t radeon_sa_bo_gpu_addr(struct radeon_sa_bo *sa_bo)
{
	return sa_bo->manager->gpu_addr + sa_bo->soffset;
}

static inline void * radeon_sa_bo_cpu_addr(struct radeon_sa_bo *sa_bo)
{
	return sa_bo->manager->cpu_ptr + sa_bo->soffset;
}

extern int radeon_sa_bo_manager_init(struct radeon_device *rdev,
				     struct radeon_sa_manager *sa_manager,
				     unsigned size, u32 align, u32 domain);
extern void radeon_sa_bo_manager_fini(struct radeon_device *rdev,
				      struct radeon_sa_manager *sa_manager);
extern int radeon_sa_bo_manager_start(struct radeon_device *rdev,
				      struct radeon_sa_manager *sa_manager);
extern int radeon_sa_bo_manager_suspend(struct radeon_device *rdev,
					struct radeon_sa_manager *sa_manager);
extern int radeon_sa_bo_new(struct radeon_device *rdev,
			    struct radeon_sa_manager *sa_manager,
			    struct radeon_sa_bo **sa_bo,
			    unsigned size, unsigned align, bool block);
extern void radeon_sa_bo_free(struct radeon_device *rdev,
			      struct radeon_sa_bo **sa_bo,
			      struct radeon_fence *fence);
#if defined(CONFIG_DEBUG_FS)
extern void radeon_sa_bo_dump_debug_info(struct radeon_sa_manager *sa_manager,
					 struct seq_file *m);
#endif


#endif
