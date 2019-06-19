/*
 * Copyright 2016 Advanced Micro Devices, Inc.
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
 */

#ifndef __AMDGPU_TTM_H__
#define __AMDGPU_TTM_H__

#include "amdgpu.h"
#include <drm/gpu_scheduler.h>

#define AMDGPU_PL_GDS		(TTM_PL_PRIV + 0)
#define AMDGPU_PL_GWS		(TTM_PL_PRIV + 1)
#define AMDGPU_PL_OA		(TTM_PL_PRIV + 2)

#define AMDGPU_PL_FLAG_GDS		(TTM_PL_FLAG_PRIV << 0)
#define AMDGPU_PL_FLAG_GWS		(TTM_PL_FLAG_PRIV << 1)
#define AMDGPU_PL_FLAG_OA		(TTM_PL_FLAG_PRIV << 2)

#define AMDGPU_GTT_MAX_TRANSFER_SIZE	512
#define AMDGPU_GTT_NUM_TRANSFER_WINDOWS	2

struct amdgpu_mman {
	struct ttm_bo_global_ref        bo_global_ref;
	struct drm_global_reference	mem_global_ref;
	struct ttm_bo_device		bdev;
	bool				mem_global_referenced;
	bool				initialized;
	void __iomem			*aper_base_kaddr;

#if defined(CONFIG_DEBUG_FS)
	struct dentry			*debugfs_entries[8];
#endif

	/* buffer handling */
	const struct amdgpu_buffer_funcs	*buffer_funcs;
	struct amdgpu_ring			*buffer_funcs_ring;
	bool					buffer_funcs_enabled;

	struct rwlock				gtt_window_lock;
	/* Scheduler entity for buffer moves */
	struct drm_sched_entity			entity;
};

struct amdgpu_copy_mem {
	struct ttm_buffer_object	*bo;
	struct ttm_mem_reg		*mem;
	unsigned long			offset;
};

extern const struct ttm_mem_type_manager_func amdgpu_gtt_mgr_func;
extern const struct ttm_mem_type_manager_func amdgpu_vram_mgr_func;

bool amdgpu_gtt_mgr_has_gart_addr(struct ttm_mem_reg *mem);
uint64_t amdgpu_gtt_mgr_usage(struct ttm_mem_type_manager *man);
int amdgpu_gtt_mgr_recover(struct ttm_mem_type_manager *man);

u64 amdgpu_vram_mgr_bo_visible_size(struct amdgpu_bo *bo);
uint64_t amdgpu_vram_mgr_usage(struct ttm_mem_type_manager *man);
uint64_t amdgpu_vram_mgr_vis_usage(struct ttm_mem_type_manager *man);

int amdgpu_ttm_init(struct amdgpu_device *adev);
void amdgpu_ttm_late_init(struct amdgpu_device *adev);
void amdgpu_ttm_fini(struct amdgpu_device *adev);
void amdgpu_ttm_set_buffer_funcs_status(struct amdgpu_device *adev,
					bool enable);

int amdgpu_copy_buffer(struct amdgpu_ring *ring, uint64_t src_offset,
		       uint64_t dst_offset, uint32_t byte_count,
		       struct reservation_object *resv,
		       struct dma_fence **fence, bool direct_submit,
		       bool vm_needs_flush);
int amdgpu_ttm_copy_mem_to_mem(struct amdgpu_device *adev,
			       struct amdgpu_copy_mem *src,
			       struct amdgpu_copy_mem *dst,
			       uint64_t size,
			       struct reservation_object *resv,
			       struct dma_fence **f);
int amdgpu_fill_buffer(struct amdgpu_bo *bo,
			uint32_t src_data,
			struct reservation_object *resv,
			struct dma_fence **fence);

#ifdef __linux__
int amdgpu_mmap(struct file *filp, struct vm_area_struct *vma);
#else
struct uvm_object *amdgpu_mmap(struct drm_device *, voff_t, vsize_t);
#endif
int amdgpu_ttm_alloc_gart(struct ttm_buffer_object *bo);
int amdgpu_ttm_recover_gart(struct ttm_buffer_object *tbo);

int amdgpu_ttm_tt_get_user_pages(struct ttm_tt *ttm, struct vm_page **pages);
void amdgpu_ttm_tt_set_user_pages(struct ttm_tt *ttm, struct vm_page **pages);
void amdgpu_ttm_tt_mark_user_pages(struct ttm_tt *ttm);
int amdgpu_ttm_tt_set_userptr(struct ttm_tt *ttm, uint64_t addr,
				     uint32_t flags);
bool amdgpu_ttm_tt_has_userptr(struct ttm_tt *ttm);
struct mm_struct *amdgpu_ttm_tt_get_usermm(struct ttm_tt *ttm);
bool amdgpu_ttm_tt_affect_userptr(struct ttm_tt *ttm, unsigned long start,
				  unsigned long end);
bool amdgpu_ttm_tt_userptr_invalidated(struct ttm_tt *ttm,
				       int *last_invalidated);
bool amdgpu_ttm_tt_userptr_needs_pages(struct ttm_tt *ttm);
bool amdgpu_ttm_tt_is_readonly(struct ttm_tt *ttm);
uint64_t amdgpu_ttm_tt_pte_flags(struct amdgpu_device *adev, struct ttm_tt *ttm,
				 struct ttm_mem_reg *mem);

#endif
