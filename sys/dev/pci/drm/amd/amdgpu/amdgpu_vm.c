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
#include <linux/dma-fence-array.h>
#include <linux/interval_tree_generic.h>
#include <linux/idr.h>
#include <drm/drmP.h>
#include <drm/amdgpu_drm.h>
#include "amdgpu.h"
#include "amdgpu_trace.h"
#include "amdgpu_amdkfd.h"
#include "amdgpu_gmc.h"

/**
 * DOC: GPUVM
 *
 * GPUVM is similar to the legacy gart on older asics, however
 * rather than there being a single global gart table
 * for the entire GPU, there are multiple VM page tables active
 * at any given time.  The VM page tables can contain a mix
 * vram pages and system memory pages and system memory pages
 * can be mapped as snooped (cached system pages) or unsnooped
 * (uncached system pages).
 * Each VM has an ID associated with it and there is a page table
 * associated with each VMID.  When execting a command buffer,
 * the kernel tells the the ring what VMID to use for that command
 * buffer.  VMIDs are allocated dynamically as commands are submitted.
 * The userspace drivers maintain their own address space and the kernel
 * sets up their pages tables accordingly when they submit their
 * command buffers and a VMID is assigned.
 * Cayman/Trinity support up to 8 active VMs at any given time;
 * SI supports 16.
 */

#define START(node) ((node)->start)
#define LAST(node) ((node)->last)

#ifdef __linux__
INTERVAL_TREE_DEFINE(struct amdgpu_bo_va_mapping, rb, uint64_t, __subtree_last,
		     START, LAST, static, amdgpu_vm_it)
#else
static struct amdgpu_bo_va_mapping *
amdgpu_vm_it_iter_first(struct rb_root_cached *root, uint64_t start,
    uint64_t last)
{
	struct amdgpu_bo_va_mapping *node;
	struct rb_node *rb;

	for (rb = rb_first(root); rb; rb = rb_next(rb)) {
		node = rb_entry(rb, typeof(*node), rb);
		if (LAST(node) >= start && START(node) <= last)
			return node;
	}
	return NULL;
}

static struct amdgpu_bo_va_mapping *
amdgpu_vm_it_iter_next(struct amdgpu_bo_va_mapping *node, uint64_t start,
    uint64_t last)
{
	STUB();
	struct rb_node *rb = &node->rb;

	for (rb = rb_next(rb); rb; rb = rb_next(rb)) {
		node = rb_entry(rb, typeof(*node), rb);
		if (LAST(node) >= start && START(node) <= last)
			return node;
	}
	return NULL;
}

static void
amdgpu_vm_it_remove(struct amdgpu_bo_va_mapping *node,
    struct rb_root_cached *root) 
{
	rb_erase(&node->rb, root);
}

static void
amdgpu_vm_it_insert(struct amdgpu_bo_va_mapping *node,
    struct rb_root_cached *root)
{
	struct rb_node **iter = &root->rb_node;
	struct rb_node *parent = NULL;
	struct amdgpu_bo_va_mapping *iter_node;

	while (*iter) {
		parent = *iter;
		iter_node = rb_entry(*iter, struct amdgpu_bo_va_mapping, rb);

		if (node->start < iter_node->start)
			iter = &(*iter)->rb_left;
		else
			iter = &(*iter)->rb_right;
	}

	rb_link_node(&node->rb, parent, iter);
	rb_insert_color(&node->rb, root);
}
#endif

#undef START
#undef LAST

/**
 * struct amdgpu_pte_update_params - Local structure
 *
 * Encapsulate some VM table update parameters to reduce
 * the number of function parameters
 *
 */
struct amdgpu_pte_update_params {

	/**
	 * @adev: amdgpu device we do this update for
	 */
	struct amdgpu_device *adev;

	/**
	 * @vm: optional amdgpu_vm we do this update for
	 */
	struct amdgpu_vm *vm;

	/**
	 * @src: address where to copy page table entries from
	 */
	uint64_t src;

	/**
	 * @ib: indirect buffer to fill with commands
	 */
	struct amdgpu_ib *ib;

	/**
	 * @func: Function which actually does the update
	 */
	void (*func)(struct amdgpu_pte_update_params *params,
		     struct amdgpu_bo *bo, uint64_t pe,
		     uint64_t addr, unsigned count, uint32_t incr,
		     uint64_t flags);
	/**
	 * @pages_addr:
	 *
	 * DMA addresses to use for mapping, used during VM update by CPU
	 */
	dma_addr_t *pages_addr;

	/**
	 * @kptr:
	 *
	 * Kernel pointer of PD/PT BO that needs to be updated,
	 * used during VM update by CPU
	 */
	void *kptr;
};

/**
 * struct amdgpu_prt_cb - Helper to disable partial resident texture feature from a fence callback
 */
struct amdgpu_prt_cb {

	/**
	 * @adev: amdgpu device
	 */
	struct amdgpu_device *adev;

	/**
	 * @cb: callback
	 */
	struct dma_fence_cb cb;
};

/**
 * amdgpu_vm_bo_base_init - Adds bo to the list of bos associated with the vm
 *
 * @base: base structure for tracking BO usage in a VM
 * @vm: vm to which bo is to be added
 * @bo: amdgpu buffer object
 *
 * Initialize a bo_va_base structure and add it to the appropriate lists
 *
 */
static void amdgpu_vm_bo_base_init(struct amdgpu_vm_bo_base *base,
				   struct amdgpu_vm *vm,
				   struct amdgpu_bo *bo)
{
	base->vm = vm;
	base->bo = bo;
	INIT_LIST_HEAD(&base->bo_list);
	INIT_LIST_HEAD(&base->vm_status);

	if (!bo)
		return;
	list_add_tail(&base->bo_list, &bo->va);

	if (bo->tbo.type == ttm_bo_type_kernel)
		list_move(&base->vm_status, &vm->relocated);

	if (bo->tbo.resv != vm->root.base.bo->tbo.resv)
		return;

	if (bo->preferred_domains &
	    amdgpu_mem_type_to_domain(bo->tbo.mem.mem_type))
		return;

	/*
	 * we checked all the prerequisites, but it looks like this per vm bo
	 * is currently evicted. add the bo to the evicted list to make sure it
	 * is validated on next vm use to avoid fault.
	 * */
	list_move_tail(&base->vm_status, &vm->evicted);
	base->moved = true;
}

/**
 * amdgpu_vm_level_shift - return the addr shift for each level
 *
 * @adev: amdgpu_device pointer
 * @level: VMPT level
 *
 * Returns:
 * The number of bits the pfn needs to be right shifted for a level.
 */
static unsigned amdgpu_vm_level_shift(struct amdgpu_device *adev,
				      unsigned level)
{
	unsigned shift = 0xff;

	switch (level) {
	case AMDGPU_VM_PDB2:
	case AMDGPU_VM_PDB1:
	case AMDGPU_VM_PDB0:
		shift = 9 * (AMDGPU_VM_PDB0 - level) +
			adev->vm_manager.block_size;
		break;
	case AMDGPU_VM_PTB:
		shift = 0;
		break;
	default:
		dev_err(adev->dev, "the level%d isn't supported.\n", level);
	}

	return shift;
}

/**
 * amdgpu_vm_num_entries - return the number of entries in a PD/PT
 *
 * @adev: amdgpu_device pointer
 * @level: VMPT level
 *
 * Returns:
 * The number of entries in a page directory or page table.
 */
static unsigned amdgpu_vm_num_entries(struct amdgpu_device *adev,
				      unsigned level)
{
	unsigned shift = amdgpu_vm_level_shift(adev,
					       adev->vm_manager.root_level);

	if (level == adev->vm_manager.root_level)
		/* For the root directory */
		return round_up(adev->vm_manager.max_pfn, 1 << shift) >> shift;
	else if (level != AMDGPU_VM_PTB)
		/* Everything in between */
		return 512;
	else
		/* For the page tables on the leaves */
		return AMDGPU_VM_PTE_COUNT(adev);
}

/**
 * amdgpu_vm_bo_size - returns the size of the BOs in bytes
 *
 * @adev: amdgpu_device pointer
 * @level: VMPT level
 *
 * Returns:
 * The size of the BO for a page directory or page table in bytes.
 */
static unsigned amdgpu_vm_bo_size(struct amdgpu_device *adev, unsigned level)
{
	return AMDGPU_GPU_PAGE_ALIGN(amdgpu_vm_num_entries(adev, level) * 8);
}

/**
 * amdgpu_vm_get_pd_bo - add the VM PD to a validation list
 *
 * @vm: vm providing the BOs
 * @validated: head of validation list
 * @entry: entry to add
 *
 * Add the page directory to the list of BOs to
 * validate for command submission.
 */
void amdgpu_vm_get_pd_bo(struct amdgpu_vm *vm,
			 struct list_head *validated,
			 struct amdgpu_bo_list_entry *entry)
{
	entry->robj = vm->root.base.bo;
	entry->priority = 0;
	entry->tv.bo = &entry->robj->tbo;
	entry->tv.shared = true;
	entry->user_pages = NULL;
	list_add(&entry->tv.head, validated);
}

/**
 * amdgpu_vm_validate_pt_bos - validate the page table BOs
 *
 * @adev: amdgpu device pointer
 * @vm: vm providing the BOs
 * @validate: callback to do the validation
 * @param: parameter for the validation callback
 *
 * Validate the page table BOs on command submission if neccessary.
 *
 * Returns:
 * Validation result.
 */
int amdgpu_vm_validate_pt_bos(struct amdgpu_device *adev, struct amdgpu_vm *vm,
			      int (*validate)(void *p, struct amdgpu_bo *bo),
			      void *param)
{
	struct ttm_bo_global *glob = adev->mman.bdev.glob;
	struct amdgpu_vm_bo_base *bo_base, *tmp;
	int r = 0;

	list_for_each_entry_safe(bo_base, tmp, &vm->evicted, vm_status) {
		struct amdgpu_bo *bo = bo_base->bo;

		if (bo->parent) {
			r = validate(param, bo);
			if (r)
				break;

			spin_lock(&glob->lru_lock);
			ttm_bo_move_to_lru_tail(&bo->tbo);
			if (bo->shadow)
				ttm_bo_move_to_lru_tail(&bo->shadow->tbo);
			spin_unlock(&glob->lru_lock);
		}

		if (bo->tbo.type != ttm_bo_type_kernel) {
			spin_lock(&vm->moved_lock);
			list_move(&bo_base->vm_status, &vm->moved);
			spin_unlock(&vm->moved_lock);
		} else {
			list_move(&bo_base->vm_status, &vm->relocated);
		}
	}

	spin_lock(&glob->lru_lock);
	list_for_each_entry(bo_base, &vm->idle, vm_status) {
		struct amdgpu_bo *bo = bo_base->bo;

		if (!bo->parent)
			continue;

		ttm_bo_move_to_lru_tail(&bo->tbo);
		if (bo->shadow)
			ttm_bo_move_to_lru_tail(&bo->shadow->tbo);
	}
	spin_unlock(&glob->lru_lock);

	return r;
}

/**
 * amdgpu_vm_ready - check VM is ready for updates
 *
 * @vm: VM to check
 *
 * Check if all VM PDs/PTs are ready for updates
 *
 * Returns:
 * True if eviction list is empty.
 */
bool amdgpu_vm_ready(struct amdgpu_vm *vm)
{
	return list_empty(&vm->evicted);
}

/**
 * amdgpu_vm_clear_bo - initially clear the PDs/PTs
 *
 * @adev: amdgpu_device pointer
 * @vm: VM to clear BO from
 * @bo: BO to clear
 * @level: level this BO is at
 * @pte_support_ats: indicate ATS support from PTE
 *
 * Root PD needs to be reserved when calling this.
 *
 * Returns:
 * 0 on success, errno otherwise.
 */
static int amdgpu_vm_clear_bo(struct amdgpu_device *adev,
			      struct amdgpu_vm *vm, struct amdgpu_bo *bo,
			      unsigned level, bool pte_support_ats)
{
	struct ttm_operation_ctx ctx = { true, false };
	struct dma_fence *fence = NULL;
	unsigned entries, ats_entries;
	struct amdgpu_ring *ring;
	struct amdgpu_job *job;
	uint64_t addr;
	int r;

	entries = amdgpu_bo_size(bo) / 8;

	if (pte_support_ats) {
		if (level == adev->vm_manager.root_level) {
			ats_entries = amdgpu_vm_level_shift(adev, level);
			ats_entries += AMDGPU_GPU_PAGE_SHIFT;
			ats_entries = AMDGPU_VA_HOLE_START >> ats_entries;
			ats_entries = min(ats_entries, entries);
			entries -= ats_entries;
		} else {
			ats_entries = entries;
			entries = 0;
		}
	} else {
		ats_entries = 0;
	}

	ring = container_of(vm->entity.rq->sched, struct amdgpu_ring, sched);

	r = reservation_object_reserve_shared(bo->tbo.resv);
	if (r)
		return r;

	r = ttm_bo_validate(&bo->tbo, &bo->placement, &ctx);
	if (r)
		goto error;

	r = amdgpu_job_alloc_with_ib(adev, 64, &job);
	if (r)
		goto error;

	addr = amdgpu_bo_gpu_offset(bo);
	if (ats_entries) {
		uint64_t ats_value;

		ats_value = AMDGPU_PTE_DEFAULT_ATC;
		if (level != AMDGPU_VM_PTB)
			ats_value |= AMDGPU_PDE_PTE;

		amdgpu_vm_set_pte_pde(adev, &job->ibs[0], addr, 0,
				      ats_entries, 0, ats_value);
		addr += ats_entries * 8;
	}

	if (entries)
		amdgpu_vm_set_pte_pde(adev, &job->ibs[0], addr, 0,
				      entries, 0, 0);

	amdgpu_ring_pad_ib(ring, &job->ibs[0]);

	WARN_ON(job->ibs[0].length_dw > 64);
	r = amdgpu_sync_resv(adev, &job->sync, bo->tbo.resv,
			     AMDGPU_FENCE_OWNER_UNDEFINED, false);
	if (r)
		goto error_free;

	r = amdgpu_job_submit(job, &vm->entity, AMDGPU_FENCE_OWNER_UNDEFINED,
			      &fence);
	if (r)
		goto error_free;

	amdgpu_bo_fence(bo, fence, true);
	dma_fence_put(fence);

	if (bo->shadow)
		return amdgpu_vm_clear_bo(adev, vm, bo->shadow,
					  level, pte_support_ats);

	return 0;

error_free:
	amdgpu_job_free(job);

error:
	return r;
}

/**
 * amdgpu_vm_alloc_levels - allocate the PD/PT levels
 *
 * @adev: amdgpu_device pointer
 * @vm: requested vm
 * @parent: parent PT
 * @saddr: start of the address range
 * @eaddr: end of the address range
 * @level: VMPT level
 * @ats: indicate ATS support from PTE
 *
 * Make sure the page directories and page tables are allocated
 *
 * Returns:
 * 0 on success, errno otherwise.
 */
static int amdgpu_vm_alloc_levels(struct amdgpu_device *adev,
				  struct amdgpu_vm *vm,
				  struct amdgpu_vm_pt *parent,
				  uint64_t saddr, uint64_t eaddr,
				  unsigned level, bool ats)
{
	unsigned shift = amdgpu_vm_level_shift(adev, level);
	unsigned pt_idx, from, to;
	u64 flags;
	int r;

	if (!parent->entries) {
		unsigned num_entries = amdgpu_vm_num_entries(adev, level);

		parent->entries = kvmalloc_array(num_entries,
						   sizeof(struct amdgpu_vm_pt),
						   GFP_KERNEL | __GFP_ZERO);
		if (!parent->entries)
			return -ENOMEM;
		memset(parent->entries, 0 , sizeof(struct amdgpu_vm_pt));
	}

	from = saddr >> shift;
	to = eaddr >> shift;
	if (from >= amdgpu_vm_num_entries(adev, level) ||
	    to >= amdgpu_vm_num_entries(adev, level))
		return -EINVAL;

	++level;
	saddr = saddr & ((1 << shift) - 1);
	eaddr = eaddr & ((1 << shift) - 1);

	flags = AMDGPU_GEM_CREATE_VRAM_CONTIGUOUS;
	if (vm->root.base.bo->shadow)
		flags |= AMDGPU_GEM_CREATE_SHADOW;
	if (vm->use_cpu_for_update)
		flags |= AMDGPU_GEM_CREATE_CPU_ACCESS_REQUIRED;
	else
		flags |= AMDGPU_GEM_CREATE_NO_CPU_ACCESS;

	/* walk over the address space and allocate the page tables */
	for (pt_idx = from; pt_idx <= to; ++pt_idx) {
		struct reservation_object *resv = vm->root.base.bo->tbo.resv;
		struct amdgpu_vm_pt *entry = &parent->entries[pt_idx];
		struct amdgpu_bo *pt;

		if (!entry->base.bo) {
			struct amdgpu_bo_param bp;

			memset(&bp, 0, sizeof(bp));
			bp.size = amdgpu_vm_bo_size(adev, level);
			bp.byte_align = AMDGPU_GPU_PAGE_SIZE;
			bp.domain = AMDGPU_GEM_DOMAIN_VRAM;
			bp.flags = flags;
			bp.type = ttm_bo_type_kernel;
			bp.resv = resv;
			r = amdgpu_bo_create(adev, &bp, &pt);
			if (r)
				return r;

			r = amdgpu_vm_clear_bo(adev, vm, pt, level, ats);
			if (r) {
				amdgpu_bo_unref(&pt->shadow);
				amdgpu_bo_unref(&pt);
				return r;
			}

			if (vm->use_cpu_for_update) {
				r = amdgpu_bo_kmap(pt, NULL);
				if (r) {
					amdgpu_bo_unref(&pt->shadow);
					amdgpu_bo_unref(&pt);
					return r;
				}
			}

			/* Keep a reference to the root directory to avoid
			* freeing them up in the wrong order.
			*/
			pt->parent = amdgpu_bo_ref(parent->base.bo);

			amdgpu_vm_bo_base_init(&entry->base, vm, pt);
		}

		if (level < AMDGPU_VM_PTB) {
			uint64_t sub_saddr = (pt_idx == from) ? saddr : 0;
			uint64_t sub_eaddr = (pt_idx == to) ? eaddr :
				((1 << shift) - 1);
			r = amdgpu_vm_alloc_levels(adev, vm, entry, sub_saddr,
						   sub_eaddr, level, ats);
			if (r)
				return r;
		}
	}

	return 0;
}

/**
 * amdgpu_vm_alloc_pts - Allocate page tables.
 *
 * @adev: amdgpu_device pointer
 * @vm: VM to allocate page tables for
 * @saddr: Start address which needs to be allocated
 * @size: Size from start address we need.
 *
 * Make sure the page tables are allocated.
 *
 * Returns:
 * 0 on success, errno otherwise.
 */
int amdgpu_vm_alloc_pts(struct amdgpu_device *adev,
			struct amdgpu_vm *vm,
			uint64_t saddr, uint64_t size)
{
	uint64_t eaddr;
	bool ats = false;

	/* validate the parameters */
	if (saddr & AMDGPU_GPU_PAGE_MASK || size & AMDGPU_GPU_PAGE_MASK)
		return -EINVAL;

	eaddr = saddr + size - 1;

	if (vm->pte_support_ats)
		ats = saddr < AMDGPU_VA_HOLE_START;

	saddr /= AMDGPU_GPU_PAGE_SIZE;
	eaddr /= AMDGPU_GPU_PAGE_SIZE;

	if (eaddr >= adev->vm_manager.max_pfn) {
		dev_err(adev->dev, "va above limit (0x%08llX >= 0x%08llX)\n",
			eaddr, adev->vm_manager.max_pfn);
		return -EINVAL;
	}

	return amdgpu_vm_alloc_levels(adev, vm, &vm->root, saddr, eaddr,
				      adev->vm_manager.root_level, ats);
}

/**
 * amdgpu_vm_check_compute_bug - check whether asic has compute vm bug
 *
 * @adev: amdgpu_device pointer
 */
void amdgpu_vm_check_compute_bug(struct amdgpu_device *adev)
{
	const struct amdgpu_ip_block *ip_block;
	bool has_compute_vm_bug;
	struct amdgpu_ring *ring;
	int i;

	has_compute_vm_bug = false;

	ip_block = amdgpu_device_ip_get_ip_block(adev, AMD_IP_BLOCK_TYPE_GFX);
	if (ip_block) {
		/* Compute has a VM bug for GFX version < 7.
		   Compute has a VM bug for GFX 8 MEC firmware version < 673.*/
		if (ip_block->version->major <= 7)
			has_compute_vm_bug = true;
		else if (ip_block->version->major == 8)
			if (adev->gfx.mec_fw_version < 673)
				has_compute_vm_bug = true;
	}

	for (i = 0; i < adev->num_rings; i++) {
		ring = adev->rings[i];
		if (ring->funcs->type == AMDGPU_RING_TYPE_COMPUTE)
			/* only compute rings */
			ring->has_compute_vm_bug = has_compute_vm_bug;
		else
			ring->has_compute_vm_bug = false;
	}
}

/**
 * amdgpu_vm_need_pipeline_sync - Check if pipe sync is needed for job.
 *
 * @ring: ring on which the job will be submitted
 * @job: job to submit
 *
 * Returns:
 * True if sync is needed.
 */
bool amdgpu_vm_need_pipeline_sync(struct amdgpu_ring *ring,
				  struct amdgpu_job *job)
{
	struct amdgpu_device *adev = ring->adev;
	unsigned vmhub = ring->funcs->vmhub;
	struct amdgpu_vmid_mgr *id_mgr = &adev->vm_manager.id_mgr[vmhub];
	struct amdgpu_vmid *id;
	bool gds_switch_needed;
	bool vm_flush_needed = job->vm_needs_flush || ring->has_compute_vm_bug;

	if (job->vmid == 0)
		return false;
	id = &id_mgr->ids[job->vmid];
	gds_switch_needed = ring->funcs->emit_gds_switch && (
		id->gds_base != job->gds_base ||
		id->gds_size != job->gds_size ||
		id->gws_base != job->gws_base ||
		id->gws_size != job->gws_size ||
		id->oa_base != job->oa_base ||
		id->oa_size != job->oa_size);

	if (amdgpu_vmid_had_gpu_reset(adev, id))
		return true;

	return vm_flush_needed || gds_switch_needed;
}

/**
 * amdgpu_vm_flush - hardware flush the vm
 *
 * @ring: ring to use for flush
 * @job:  related job
 * @need_pipe_sync: is pipe sync needed
 *
 * Emit a VM flush when it is necessary.
 *
 * Returns:
 * 0 on success, errno otherwise.
 */
int amdgpu_vm_flush(struct amdgpu_ring *ring, struct amdgpu_job *job, bool need_pipe_sync)
{
	struct amdgpu_device *adev = ring->adev;
	unsigned vmhub = ring->funcs->vmhub;
	struct amdgpu_vmid_mgr *id_mgr = &adev->vm_manager.id_mgr[vmhub];
	struct amdgpu_vmid *id = &id_mgr->ids[job->vmid];
	bool gds_switch_needed = ring->funcs->emit_gds_switch && (
		id->gds_base != job->gds_base ||
		id->gds_size != job->gds_size ||
		id->gws_base != job->gws_base ||
		id->gws_size != job->gws_size ||
		id->oa_base != job->oa_base ||
		id->oa_size != job->oa_size);
	bool vm_flush_needed = job->vm_needs_flush;
	bool pasid_mapping_needed = id->pasid != job->pasid ||
		!id->pasid_mapping ||
		!dma_fence_is_signaled(id->pasid_mapping);
	struct dma_fence *fence = NULL;
	unsigned patch_offset = 0;
	int r;

	if (amdgpu_vmid_had_gpu_reset(adev, id)) {
		gds_switch_needed = true;
		vm_flush_needed = true;
		pasid_mapping_needed = true;
	}

	gds_switch_needed &= !!ring->funcs->emit_gds_switch;
	vm_flush_needed &= !!ring->funcs->emit_vm_flush  &&
			job->vm_pd_addr != AMDGPU_BO_INVALID_OFFSET;
	pasid_mapping_needed &= adev->gmc.gmc_funcs->emit_pasid_mapping &&
		ring->funcs->emit_wreg;

	if (!vm_flush_needed && !gds_switch_needed && !need_pipe_sync)
		return 0;

	if (ring->funcs->init_cond_exec)
		patch_offset = amdgpu_ring_init_cond_exec(ring);

	if (need_pipe_sync)
		amdgpu_ring_emit_pipeline_sync(ring);

	if (vm_flush_needed) {
		trace_amdgpu_vm_flush(ring, job->vmid, job->vm_pd_addr);
		amdgpu_ring_emit_vm_flush(ring, job->vmid, job->vm_pd_addr);
	}

	if (pasid_mapping_needed)
		amdgpu_gmc_emit_pasid_mapping(ring, job->vmid, job->pasid);

	if (vm_flush_needed || pasid_mapping_needed) {
		r = amdgpu_fence_emit(ring, &fence, 0);
		if (r)
			return r;
	}

	if (vm_flush_needed) {
		mutex_lock(&id_mgr->lock);
		dma_fence_put(id->last_flush);
		id->last_flush = dma_fence_get(fence);
		id->current_gpu_reset_count =
			atomic_read(&adev->gpu_reset_counter);
		mutex_unlock(&id_mgr->lock);
	}

	if (pasid_mapping_needed) {
		id->pasid = job->pasid;
		dma_fence_put(id->pasid_mapping);
		id->pasid_mapping = dma_fence_get(fence);
	}
	dma_fence_put(fence);

	if (ring->funcs->emit_gds_switch && gds_switch_needed) {
		id->gds_base = job->gds_base;
		id->gds_size = job->gds_size;
		id->gws_base = job->gws_base;
		id->gws_size = job->gws_size;
		id->oa_base = job->oa_base;
		id->oa_size = job->oa_size;
		amdgpu_ring_emit_gds_switch(ring, job->vmid, job->gds_base,
					    job->gds_size, job->gws_base,
					    job->gws_size, job->oa_base,
					    job->oa_size);
	}

	if (ring->funcs->patch_cond_exec)
		amdgpu_ring_patch_cond_exec(ring, patch_offset);

	/* the double SWITCH_BUFFER here *cannot* be skipped by COND_EXEC */
	if (ring->funcs->emit_switch_buffer) {
		amdgpu_ring_emit_switch_buffer(ring);
		amdgpu_ring_emit_switch_buffer(ring);
	}
	return 0;
}

/**
 * amdgpu_vm_bo_find - find the bo_va for a specific vm & bo
 *
 * @vm: requested vm
 * @bo: requested buffer object
 *
 * Find @bo inside the requested vm.
 * Search inside the @bos vm list for the requested vm
 * Returns the found bo_va or NULL if none is found
 *
 * Object has to be reserved!
 *
 * Returns:
 * Found bo_va or NULL.
 */
struct amdgpu_bo_va *amdgpu_vm_bo_find(struct amdgpu_vm *vm,
				       struct amdgpu_bo *bo)
{
	struct amdgpu_bo_va *bo_va;

	list_for_each_entry(bo_va, &bo->va, base.bo_list) {
		if (bo_va->base.vm == vm) {
			return bo_va;
		}
	}
	return NULL;
}

/**
 * amdgpu_vm_do_set_ptes - helper to call the right asic function
 *
 * @params: see amdgpu_pte_update_params definition
 * @bo: PD/PT to update
 * @pe: addr of the page entry
 * @addr: dst addr to write into pe
 * @count: number of page entries to update
 * @incr: increase next addr by incr bytes
 * @flags: hw access flags
 *
 * Traces the parameters and calls the right asic functions
 * to setup the page table using the DMA.
 */
static void amdgpu_vm_do_set_ptes(struct amdgpu_pte_update_params *params,
				  struct amdgpu_bo *bo,
				  uint64_t pe, uint64_t addr,
				  unsigned count, uint32_t incr,
				  uint64_t flags)
{
	pe += amdgpu_bo_gpu_offset(bo);
	trace_amdgpu_vm_set_ptes(pe, addr, count, incr, flags);

	if (count < 3) {
		amdgpu_vm_write_pte(params->adev, params->ib, pe,
				    addr | flags, count, incr);

	} else {
		amdgpu_vm_set_pte_pde(params->adev, params->ib, pe, addr,
				      count, incr, flags);
	}
}

/**
 * amdgpu_vm_do_copy_ptes - copy the PTEs from the GART
 *
 * @params: see amdgpu_pte_update_params definition
 * @bo: PD/PT to update
 * @pe: addr of the page entry
 * @addr: dst addr to write into pe
 * @count: number of page entries to update
 * @incr: increase next addr by incr bytes
 * @flags: hw access flags
 *
 * Traces the parameters and calls the DMA function to copy the PTEs.
 */
static void amdgpu_vm_do_copy_ptes(struct amdgpu_pte_update_params *params,
				   struct amdgpu_bo *bo,
				   uint64_t pe, uint64_t addr,
				   unsigned count, uint32_t incr,
				   uint64_t flags)
{
	uint64_t src = (params->src + (addr >> 12) * 8);

	pe += amdgpu_bo_gpu_offset(bo);
	trace_amdgpu_vm_copy_ptes(pe, src, count);

	amdgpu_vm_copy_pte(params->adev, params->ib, pe, src, count);
}

/**
 * amdgpu_vm_map_gart - Resolve gart mapping of addr
 *
 * @pages_addr: optional DMA address to use for lookup
 * @addr: the unmapped addr
 *
 * Look up the physical address of the page that the pte resolves
 * to.
 *
 * Returns:
 * The pointer for the page table entry.
 */
static uint64_t amdgpu_vm_map_gart(const dma_addr_t *pages_addr, uint64_t addr)
{
	uint64_t result;

	/* page table offset */
	result = pages_addr[addr >> PAGE_SHIFT];

	/* in case cpu page size != gpu page size*/
	result |= addr & (PAGE_MASK);

	result &= 0xFFFFFFFFFFFFF000ULL;

	return result;
}

/**
 * amdgpu_vm_cpu_set_ptes - helper to update page tables via CPU
 *
 * @params: see amdgpu_pte_update_params definition
 * @bo: PD/PT to update
 * @pe: kmap addr of the page entry
 * @addr: dst addr to write into pe
 * @count: number of page entries to update
 * @incr: increase next addr by incr bytes
 * @flags: hw access flags
 *
 * Write count number of PT/PD entries directly.
 */
static void amdgpu_vm_cpu_set_ptes(struct amdgpu_pte_update_params *params,
				   struct amdgpu_bo *bo,
				   uint64_t pe, uint64_t addr,
				   unsigned count, uint32_t incr,
				   uint64_t flags)
{
	unsigned int i;
	uint64_t value;

	pe += (unsigned long)amdgpu_bo_kptr(bo);

	trace_amdgpu_vm_set_ptes(pe, addr, count, incr, flags);

	for (i = 0; i < count; i++) {
		value = params->pages_addr ?
			amdgpu_vm_map_gart(params->pages_addr, addr) :
			addr;
		amdgpu_gmc_set_pte_pde(params->adev, (void *)(uintptr_t)pe,
				       i, value, flags);
		addr += incr;
	}
}


/**
 * amdgpu_vm_wait_pd - Wait for PT BOs to be free.
 *
 * @adev: amdgpu_device pointer
 * @vm: related vm
 * @owner: fence owner
 *
 * Returns:
 * 0 on success, errno otherwise.
 */
static int amdgpu_vm_wait_pd(struct amdgpu_device *adev, struct amdgpu_vm *vm,
			     void *owner)
{
	struct amdgpu_sync sync;
	int r;

	amdgpu_sync_create(&sync);
	amdgpu_sync_resv(adev, &sync, vm->root.base.bo->tbo.resv, owner, false);
	r = amdgpu_sync_wait(&sync, true);
	amdgpu_sync_free(&sync);

	return r;
}

/*
 * amdgpu_vm_update_pde - update a single level in the hierarchy
 *
 * @param: parameters for the update
 * @vm: requested vm
 * @parent: parent directory
 * @entry: entry to update
 *
 * Makes sure the requested entry in parent is up to date.
 */
static void amdgpu_vm_update_pde(struct amdgpu_pte_update_params *params,
				 struct amdgpu_vm *vm,
				 struct amdgpu_vm_pt *parent,
				 struct amdgpu_vm_pt *entry)
{
	struct amdgpu_bo *bo = parent->base.bo, *pbo;
	uint64_t pde, pt, flags;
	unsigned level;

	/* Don't update huge pages here */
	if (entry->huge)
		return;

	for (level = 0, pbo = bo->parent; pbo; ++level)
		pbo = pbo->parent;

	level += params->adev->vm_manager.root_level;
	pt = amdgpu_bo_gpu_offset(entry->base.bo);
	flags = AMDGPU_PTE_VALID;
	amdgpu_gmc_get_vm_pde(params->adev, level, &pt, &flags);
	pde = (entry - parent->entries) * 8;
	if (bo->shadow)
		params->func(params, bo->shadow, pde, pt, 1, 0, flags);
	params->func(params, bo, pde, pt, 1, 0, flags);
}

/*
 * amdgpu_vm_invalidate_level - mark all PD levels as invalid
 *
 * @adev: amdgpu_device pointer
 * @vm: related vm
 * @parent: parent PD
 * @level: VMPT level
 *
 * Mark all PD level as invalid after an error.
 */
static void amdgpu_vm_invalidate_level(struct amdgpu_device *adev,
				       struct amdgpu_vm *vm,
				       struct amdgpu_vm_pt *parent,
				       unsigned level)
{
	unsigned pt_idx, num_entries;

	/*
	 * Recurse into the subdirectories. This recursion is harmless because
	 * we only have a maximum of 5 layers.
	 */
	num_entries = amdgpu_vm_num_entries(adev, level);
	for (pt_idx = 0; pt_idx < num_entries; ++pt_idx) {
		struct amdgpu_vm_pt *entry = &parent->entries[pt_idx];

		if (!entry->base.bo)
			continue;

		if (!entry->base.moved)
			list_move(&entry->base.vm_status, &vm->relocated);
		amdgpu_vm_invalidate_level(adev, vm, entry, level + 1);
	}
}

/*
 * amdgpu_vm_update_directories - make sure that all directories are valid
 *
 * @adev: amdgpu_device pointer
 * @vm: requested vm
 *
 * Makes sure all directories are up to date.
 *
 * Returns:
 * 0 for success, error for failure.
 */
int amdgpu_vm_update_directories(struct amdgpu_device *adev,
				 struct amdgpu_vm *vm)
{
	struct amdgpu_pte_update_params params;
	struct amdgpu_job *job;
	unsigned ndw = 0;
	int r = 0;

	if (list_empty(&vm->relocated))
		return 0;

restart:
	memset(&params, 0, sizeof(params));
	params.adev = adev;

	if (vm->use_cpu_for_update) {
		struct amdgpu_vm_bo_base *bo_base;

		list_for_each_entry(bo_base, &vm->relocated, vm_status) {
			r = amdgpu_bo_kmap(bo_base->bo, NULL);
			if (unlikely(r))
				return r;
		}

		r = amdgpu_vm_wait_pd(adev, vm, AMDGPU_FENCE_OWNER_VM);
		if (unlikely(r))
			return r;

		params.func = amdgpu_vm_cpu_set_ptes;
	} else {
		ndw = 512 * 8;
		r = amdgpu_job_alloc_with_ib(adev, ndw * 4, &job);
		if (r)
			return r;

		params.ib = &job->ibs[0];
		params.func = amdgpu_vm_do_set_ptes;
	}

	while (!list_empty(&vm->relocated)) {
		struct amdgpu_vm_bo_base *bo_base, *parent;
		struct amdgpu_vm_pt *pt, *entry;
		struct amdgpu_bo *bo;

		bo_base = list_first_entry(&vm->relocated,
					   struct amdgpu_vm_bo_base,
					   vm_status);
		bo_base->moved = false;
		list_del_init(&bo_base->vm_status);

		bo = bo_base->bo->parent;
		if (!bo)
			continue;

		parent = list_first_entry(&bo->va, struct amdgpu_vm_bo_base,
					  bo_list);
		pt = container_of(parent, struct amdgpu_vm_pt, base);
		entry = container_of(bo_base, struct amdgpu_vm_pt, base);

		amdgpu_vm_update_pde(&params, vm, pt, entry);

		if (!vm->use_cpu_for_update &&
		    (ndw - params.ib->length_dw) < 32)
			break;
	}

	if (vm->use_cpu_for_update) {
		/* Flush HDP */
		mb();
		amdgpu_asic_flush_hdp(adev, NULL);
	} else if (params.ib->length_dw == 0) {
		amdgpu_job_free(job);
	} else {
		struct amdgpu_bo *root = vm->root.base.bo;
		struct amdgpu_ring *ring;
		struct dma_fence *fence;

		ring = container_of(vm->entity.rq->sched, struct amdgpu_ring,
				    sched);

		amdgpu_ring_pad_ib(ring, params.ib);
		amdgpu_sync_resv(adev, &job->sync, root->tbo.resv,
				 AMDGPU_FENCE_OWNER_VM, false);
		WARN_ON(params.ib->length_dw > ndw);
		r = amdgpu_job_submit(job, &vm->entity, AMDGPU_FENCE_OWNER_VM,
				      &fence);
		if (r)
			goto error;

		amdgpu_bo_fence(root, fence, true);
		dma_fence_put(vm->last_update);
		vm->last_update = fence;
	}

	if (!list_empty(&vm->relocated))
		goto restart;

	return 0;

error:
	amdgpu_vm_invalidate_level(adev, vm, &vm->root,
				   adev->vm_manager.root_level);
	amdgpu_job_free(job);
	return r;
}

/**
 * amdgpu_vm_find_entry - find the entry for an address
 *
 * @p: see amdgpu_pte_update_params definition
 * @addr: virtual address in question
 * @entry: resulting entry or NULL
 * @parent: parent entry
 *
 * Find the vm_pt entry and it's parent for the given address.
 */
void amdgpu_vm_get_entry(struct amdgpu_pte_update_params *p, uint64_t addr,
			 struct amdgpu_vm_pt **entry,
			 struct amdgpu_vm_pt **parent)
{
	unsigned level = p->adev->vm_manager.root_level;

	*parent = NULL;
	*entry = &p->vm->root;
	while ((*entry)->entries) {
		unsigned shift = amdgpu_vm_level_shift(p->adev, level++);

		*parent = *entry;
		*entry = &(*entry)->entries[addr >> shift];
		addr &= (1ULL << shift) - 1;
	}

	if (level != AMDGPU_VM_PTB)
		*entry = NULL;
}

/**
 * amdgpu_vm_handle_huge_pages - handle updating the PD with huge pages
 *
 * @p: see amdgpu_pte_update_params definition
 * @entry: vm_pt entry to check
 * @parent: parent entry
 * @nptes: number of PTEs updated with this operation
 * @dst: destination address where the PTEs should point to
 * @flags: access flags fro the PTEs
 *
 * Check if we can update the PD with a huge page.
 */
static void amdgpu_vm_handle_huge_pages(struct amdgpu_pte_update_params *p,
					struct amdgpu_vm_pt *entry,
					struct amdgpu_vm_pt *parent,
					unsigned nptes, uint64_t dst,
					uint64_t flags)
{
	uint64_t pde;

	/* In the case of a mixed PT the PDE must point to it*/
	if (p->adev->asic_type >= CHIP_VEGA10 && !p->src &&
	    nptes == AMDGPU_VM_PTE_COUNT(p->adev)) {
		/* Set the huge page flag to stop scanning at this PDE */
		flags |= AMDGPU_PDE_PTE;
	}

	if (!(flags & AMDGPU_PDE_PTE)) {
		if (entry->huge) {
			/* Add the entry to the relocated list to update it. */
			entry->huge = false;
			list_move(&entry->base.vm_status, &p->vm->relocated);
		}
		return;
	}

	entry->huge = true;
	amdgpu_gmc_get_vm_pde(p->adev, AMDGPU_VM_PDB0, &dst, &flags);

	pde = (entry - parent->entries) * 8;
	if (parent->base.bo->shadow)
		p->func(p, parent->base.bo->shadow, pde, dst, 1, 0, flags);
	p->func(p, parent->base.bo, pde, dst, 1, 0, flags);
}

/**
 * amdgpu_vm_update_ptes - make sure that page tables are valid
 *
 * @params: see amdgpu_pte_update_params definition
 * @start: start of GPU address range
 * @end: end of GPU address range
 * @dst: destination address to map to, the next dst inside the function
 * @flags: mapping flags
 *
 * Update the page tables in the range @start - @end.
 *
 * Returns:
 * 0 for success, -EINVAL for failure.
 */
static int amdgpu_vm_update_ptes(struct amdgpu_pte_update_params *params,
				  uint64_t start, uint64_t end,
				  uint64_t dst, uint64_t flags)
{
	struct amdgpu_device *adev = params->adev;
	const uint64_t mask = AMDGPU_VM_PTE_COUNT(adev) - 1;

	uint64_t addr, pe_start;
	struct amdgpu_bo *pt;
	unsigned nptes;

	/* walk over the address space and update the page tables */
	for (addr = start; addr < end; addr += nptes,
	     dst += nptes * AMDGPU_GPU_PAGE_SIZE) {
		struct amdgpu_vm_pt *entry, *parent;

		amdgpu_vm_get_entry(params, addr, &entry, &parent);
		if (!entry)
			return -ENOENT;

		if ((addr & ~mask) == (end & ~mask))
			nptes = end - addr;
		else
			nptes = AMDGPU_VM_PTE_COUNT(adev) - (addr & mask);

		amdgpu_vm_handle_huge_pages(params, entry, parent,
					    nptes, dst, flags);
		/* We don't need to update PTEs for huge pages */
		if (entry->huge)
			continue;

		pt = entry->base.bo;
		pe_start = (addr & mask) * 8;
		if (pt->shadow)
			params->func(params, pt->shadow, pe_start, dst, nptes,
				     AMDGPU_GPU_PAGE_SIZE, flags);
		params->func(params, pt, pe_start, dst, nptes,
			     AMDGPU_GPU_PAGE_SIZE, flags);
	}

	return 0;
}

/*
 * amdgpu_vm_frag_ptes - add fragment information to PTEs
 *
 * @params: see amdgpu_pte_update_params definition
 * @vm: requested vm
 * @start: first PTE to handle
 * @end: last PTE to handle
 * @dst: addr those PTEs should point to
 * @flags: hw mapping flags
 *
 * Returns:
 * 0 for success, -EINVAL for failure.
 */
static int amdgpu_vm_frag_ptes(struct amdgpu_pte_update_params	*params,
				uint64_t start, uint64_t end,
				uint64_t dst, uint64_t flags)
{
	/**
	 * The MC L1 TLB supports variable sized pages, based on a fragment
	 * field in the PTE. When this field is set to a non-zero value, page
	 * granularity is increased from 4KB to (1 << (12 + frag)). The PTE
	 * flags are considered valid for all PTEs within the fragment range
	 * and corresponding mappings are assumed to be physically contiguous.
	 *
	 * The L1 TLB can store a single PTE for the whole fragment,
	 * significantly increasing the space available for translation
	 * caching. This leads to large improvements in throughput when the
	 * TLB is under pressure.
	 *
	 * The L2 TLB distributes small and large fragments into two
	 * asymmetric partitions. The large fragment cache is significantly
	 * larger. Thus, we try to use large fragments wherever possible.
	 * Userspace can support this by aligning virtual base address and
	 * allocation size to the fragment size.
	 */
	unsigned max_frag = params->adev->vm_manager.fragment_size;
	int r;

	/* system pages are non continuously */
	if (params->src || !(flags & AMDGPU_PTE_VALID))
		return amdgpu_vm_update_ptes(params, start, end, dst, flags);

	while (start != end) {
		uint64_t frag_flags, frag_end;
		unsigned frag;

		/* This intentionally wraps around if no bit is set */
		frag = min((unsigned)ffs(start) - 1,
			   (unsigned)fls64(end - start) - 1);
		if (frag >= max_frag) {
			frag_flags = AMDGPU_PTE_FRAG(max_frag);
			frag_end = end & ~((1ULL << max_frag) - 1);
		} else {
			frag_flags = AMDGPU_PTE_FRAG(frag);
			frag_end = start + (1 << frag);
		}

		r = amdgpu_vm_update_ptes(params, start, frag_end, dst,
					  flags | frag_flags);
		if (r)
			return r;

		dst += (frag_end - start) * AMDGPU_GPU_PAGE_SIZE;
		start = frag_end;
	}

	return 0;
}

/**
 * amdgpu_vm_bo_update_mapping - update a mapping in the vm page table
 *
 * @adev: amdgpu_device pointer
 * @exclusive: fence we need to sync to
 * @pages_addr: DMA addresses to use for mapping
 * @vm: requested vm
 * @start: start of mapped range
 * @last: last mapped entry
 * @flags: flags for the entries
 * @addr: addr to set the area to
 * @fence: optional resulting fence
 *
 * Fill in the page table entries between @start and @last.
 *
 * Returns:
 * 0 for success, -EINVAL for failure.
 */
static int amdgpu_vm_bo_update_mapping(struct amdgpu_device *adev,
				       struct dma_fence *exclusive,
				       dma_addr_t *pages_addr,
				       struct amdgpu_vm *vm,
				       uint64_t start, uint64_t last,
				       uint64_t flags, uint64_t addr,
				       struct dma_fence **fence)
{
	struct amdgpu_ring *ring;
	void *owner = AMDGPU_FENCE_OWNER_VM;
	unsigned nptes, ncmds, ndw;
	struct amdgpu_job *job;
	struct amdgpu_pte_update_params params;
	struct dma_fence *f = NULL;
	int r;

	memset(&params, 0, sizeof(params));
	params.adev = adev;
	params.vm = vm;

	/* sync to everything on unmapping */
	if (!(flags & AMDGPU_PTE_VALID))
		owner = AMDGPU_FENCE_OWNER_UNDEFINED;

	if (vm->use_cpu_for_update) {
		/* params.src is used as flag to indicate system Memory */
		if (pages_addr)
			params.src = ~0;

		/* Wait for PT BOs to be free. PTs share the same resv. object
		 * as the root PD BO
		 */
		r = amdgpu_vm_wait_pd(adev, vm, owner);
		if (unlikely(r))
			return r;

		params.func = amdgpu_vm_cpu_set_ptes;
		params.pages_addr = pages_addr;
		return amdgpu_vm_frag_ptes(&params, start, last + 1,
					   addr, flags);
	}

	ring = container_of(vm->entity.rq->sched, struct amdgpu_ring, sched);

	nptes = last - start + 1;

	/*
	 * reserve space for two commands every (1 << BLOCK_SIZE)
	 *  entries or 2k dwords (whatever is smaller)
         *
         * The second command is for the shadow pagetables.
	 */
	if (vm->root.base.bo->shadow)
		ncmds = ((nptes >> min(adev->vm_manager.block_size, 11u)) + 1) * 2;
	else
		ncmds = ((nptes >> min(adev->vm_manager.block_size, 11u)) + 1);

	/* padding, etc. */
	ndw = 64;

	if (pages_addr) {
		/* copy commands needed */
		ndw += ncmds * adev->vm_manager.vm_pte_funcs->copy_pte_num_dw;

		/* and also PTEs */
		ndw += nptes * 2;

		params.func = amdgpu_vm_do_copy_ptes;

	} else {
		/* set page commands needed */
		ndw += ncmds * 10;

		/* extra commands for begin/end fragments */
		if (vm->root.base.bo->shadow)
		        ndw += 2 * 10 * adev->vm_manager.fragment_size * 2;
		else
		        ndw += 2 * 10 * adev->vm_manager.fragment_size;

		params.func = amdgpu_vm_do_set_ptes;
	}

	r = amdgpu_job_alloc_with_ib(adev, ndw * 4, &job);
	if (r)
		return r;

	params.ib = &job->ibs[0];

	if (pages_addr) {
		uint64_t *pte;
		unsigned i;

		/* Put the PTEs at the end of the IB. */
		i = ndw - nptes * 2;
		pte= (uint64_t *)&(job->ibs->ptr[i]);
		params.src = job->ibs->gpu_addr + i * 4;

		for (i = 0; i < nptes; ++i) {
			pte[i] = amdgpu_vm_map_gart(pages_addr, addr + i *
						    AMDGPU_GPU_PAGE_SIZE);
			pte[i] |= flags;
		}
		addr = 0;
	}

	r = amdgpu_sync_fence(adev, &job->sync, exclusive, false);
	if (r)
		goto error_free;

	r = amdgpu_sync_resv(adev, &job->sync, vm->root.base.bo->tbo.resv,
			     owner, false);
	if (r)
		goto error_free;

	r = reservation_object_reserve_shared(vm->root.base.bo->tbo.resv);
	if (r)
		goto error_free;

	r = amdgpu_vm_frag_ptes(&params, start, last + 1, addr, flags);
	if (r)
		goto error_free;

	amdgpu_ring_pad_ib(ring, params.ib);
	WARN_ON(params.ib->length_dw > ndw);
	r = amdgpu_job_submit(job, &vm->entity, AMDGPU_FENCE_OWNER_VM, &f);
	if (r)
		goto error_free;

	amdgpu_bo_fence(vm->root.base.bo, f, true);
	dma_fence_put(*fence);
	*fence = f;
	return 0;

error_free:
	amdgpu_job_free(job);
	return r;
}

/**
 * amdgpu_vm_bo_split_mapping - split a mapping into smaller chunks
 *
 * @adev: amdgpu_device pointer
 * @exclusive: fence we need to sync to
 * @pages_addr: DMA addresses to use for mapping
 * @vm: requested vm
 * @mapping: mapped range and flags to use for the update
 * @flags: HW flags for the mapping
 * @nodes: array of drm_mm_nodes with the MC addresses
 * @fence: optional resulting fence
 *
 * Split the mapping into smaller chunks so that each update fits
 * into a SDMA IB.
 *
 * Returns:
 * 0 for success, -EINVAL for failure.
 */
static int amdgpu_vm_bo_split_mapping(struct amdgpu_device *adev,
				      struct dma_fence *exclusive,
				      dma_addr_t *pages_addr,
				      struct amdgpu_vm *vm,
				      struct amdgpu_bo_va_mapping *mapping,
				      uint64_t flags,
				      struct drm_mm_node *nodes,
				      struct dma_fence **fence)
{
	unsigned min_linear_pages = 1 << adev->vm_manager.fragment_size;
	uint64_t pfn, start = mapping->start;
	int r;

	/* normally,bo_va->flags only contians READABLE and WIRTEABLE bit go here
	 * but in case of something, we filter the flags in first place
	 */
	if (!(mapping->flags & AMDGPU_PTE_READABLE))
		flags &= ~AMDGPU_PTE_READABLE;
	if (!(mapping->flags & AMDGPU_PTE_WRITEABLE))
		flags &= ~AMDGPU_PTE_WRITEABLE;

	flags &= ~AMDGPU_PTE_EXECUTABLE;
	flags |= mapping->flags & AMDGPU_PTE_EXECUTABLE;

	flags &= ~AMDGPU_PTE_MTYPE_MASK;
	flags |= (mapping->flags & AMDGPU_PTE_MTYPE_MASK);

	if ((mapping->flags & AMDGPU_PTE_PRT) &&
	    (adev->asic_type >= CHIP_VEGA10)) {
		flags |= AMDGPU_PTE_PRT;
		flags &= ~AMDGPU_PTE_VALID;
	}

	trace_amdgpu_vm_bo_update(mapping);

	pfn = mapping->offset >> PAGE_SHIFT;
	if (nodes) {
		while (pfn >= nodes->size) {
			pfn -= nodes->size;
			++nodes;
		}
	}

	do {
		dma_addr_t *dma_addr = NULL;
		uint64_t max_entries;
		uint64_t addr, last;

		if (nodes) {
			addr = nodes->start << PAGE_SHIFT;
			max_entries = (nodes->size - pfn) *
				AMDGPU_GPU_PAGES_IN_CPU_PAGE;
		} else {
			addr = 0;
			max_entries = S64_MAX;
		}

		if (pages_addr) {
			uint64_t count;

			max_entries = min(max_entries, 16ull * 1024ull);
			for (count = 1;
			     count < max_entries / AMDGPU_GPU_PAGES_IN_CPU_PAGE;
			     ++count) {
				uint64_t idx = pfn + count;

				if (pages_addr[idx] !=
				    (pages_addr[idx - 1] + PAGE_SIZE))
					break;
			}

			if (count < min_linear_pages) {
				addr = pfn << PAGE_SHIFT;
				dma_addr = pages_addr;
			} else {
				addr = pages_addr[pfn];
				max_entries = count * AMDGPU_GPU_PAGES_IN_CPU_PAGE;
			}

		} else if (flags & AMDGPU_PTE_VALID) {
			addr += adev->vm_manager.vram_base_offset;
			addr += pfn << PAGE_SHIFT;
		}

		last = min((uint64_t)mapping->last, start + max_entries - 1);
		r = amdgpu_vm_bo_update_mapping(adev, exclusive, dma_addr, vm,
						start, last, flags, addr,
						fence);
		if (r)
			return r;

		pfn += (last - start + 1) / AMDGPU_GPU_PAGES_IN_CPU_PAGE;
		if (nodes && nodes->size == pfn) {
			pfn = 0;
			++nodes;
		}
		start = last + 1;

	} while (unlikely(start != mapping->last + 1));

	return 0;
}

/**
 * amdgpu_vm_bo_update - update all BO mappings in the vm page table
 *
 * @adev: amdgpu_device pointer
 * @bo_va: requested BO and VM object
 * @clear: if true clear the entries
 *
 * Fill in the page table entries for @bo_va.
 *
 * Returns:
 * 0 for success, -EINVAL for failure.
 */
int amdgpu_vm_bo_update(struct amdgpu_device *adev,
			struct amdgpu_bo_va *bo_va,
			bool clear)
{
	struct amdgpu_bo *bo = bo_va->base.bo;
	struct amdgpu_vm *vm = bo_va->base.vm;
	struct amdgpu_bo_va_mapping *mapping;
	dma_addr_t *pages_addr = NULL;
	struct ttm_mem_reg *mem;
	struct drm_mm_node *nodes;
	struct dma_fence *exclusive, **last_update;
	uint64_t flags;
	int r;

	if (clear || !bo) {
		mem = NULL;
		nodes = NULL;
		exclusive = NULL;
	} else {
		struct ttm_dma_tt *ttm;

		mem = &bo->tbo.mem;
		nodes = mem->mm_node;
		if (mem->mem_type == TTM_PL_TT) {
			ttm = container_of(bo->tbo.ttm, struct ttm_dma_tt, ttm);
			pages_addr = ttm->dma_address;
		}
		exclusive = reservation_object_get_excl(bo->tbo.resv);
	}

	if (bo)
		flags = amdgpu_ttm_tt_pte_flags(adev, bo->tbo.ttm, mem);
	else
		flags = 0x0;

	if (clear || (bo && bo->tbo.resv == vm->root.base.bo->tbo.resv))
		last_update = &vm->last_update;
	else
		last_update = &bo_va->last_pt_update;

	if (!clear && bo_va->base.moved) {
		bo_va->base.moved = false;
		list_splice_init(&bo_va->valids, &bo_va->invalids);

	} else if (bo_va->cleared != clear) {
		list_splice_init(&bo_va->valids, &bo_va->invalids);
	}

	list_for_each_entry(mapping, &bo_va->invalids, list) {
		r = amdgpu_vm_bo_split_mapping(adev, exclusive, pages_addr, vm,
					       mapping, flags, nodes,
					       last_update);
		if (r)
			return r;
	}

	if (vm->use_cpu_for_update) {
		/* Flush HDP */
		mb();
		amdgpu_asic_flush_hdp(adev, NULL);
	}

	spin_lock(&vm->moved_lock);
	list_del_init(&bo_va->base.vm_status);
	spin_unlock(&vm->moved_lock);

	/* If the BO is not in its preferred location add it back to
	 * the evicted list so that it gets validated again on the
	 * next command submission.
	 */
	if (bo && bo->tbo.resv == vm->root.base.bo->tbo.resv) {
		uint32_t mem_type = bo->tbo.mem.mem_type;

		if (!(bo->preferred_domains & amdgpu_mem_type_to_domain(mem_type)))
			list_add_tail(&bo_va->base.vm_status, &vm->evicted);
		else
			list_add(&bo_va->base.vm_status, &vm->idle);
	}

	list_splice_init(&bo_va->invalids, &bo_va->valids);
	bo_va->cleared = clear;

	if (trace_amdgpu_vm_bo_mapping_enabled()) {
		list_for_each_entry(mapping, &bo_va->valids, list)
			trace_amdgpu_vm_bo_mapping(mapping);
	}

	return 0;
}

/**
 * amdgpu_vm_update_prt_state - update the global PRT state
 *
 * @adev: amdgpu_device pointer
 */
static void amdgpu_vm_update_prt_state(struct amdgpu_device *adev)
{
	unsigned long flags;
	bool enable;

	spin_lock_irqsave(&adev->vm_manager.prt_lock, flags);
	enable = !!atomic_read(&adev->vm_manager.num_prt_users);
	adev->gmc.gmc_funcs->set_prt(adev, enable);
	spin_unlock_irqrestore(&adev->vm_manager.prt_lock, flags);
}

/**
 * amdgpu_vm_prt_get - add a PRT user
 *
 * @adev: amdgpu_device pointer
 */
static void amdgpu_vm_prt_get(struct amdgpu_device *adev)
{
	if (!adev->gmc.gmc_funcs->set_prt)
		return;

	if (atomic_inc_return(&adev->vm_manager.num_prt_users) == 1)
		amdgpu_vm_update_prt_state(adev);
}

/**
 * amdgpu_vm_prt_put - drop a PRT user
 *
 * @adev: amdgpu_device pointer
 */
static void amdgpu_vm_prt_put(struct amdgpu_device *adev)
{
	if (atomic_dec_return(&adev->vm_manager.num_prt_users) == 0)
		amdgpu_vm_update_prt_state(adev);
}

/**
 * amdgpu_vm_prt_cb - callback for updating the PRT status
 *
 * @fence: fence for the callback
 * @_cb: the callback function
 */
static void amdgpu_vm_prt_cb(struct dma_fence *fence, struct dma_fence_cb *_cb)
{
	struct amdgpu_prt_cb *cb = container_of(_cb, struct amdgpu_prt_cb, cb);

	amdgpu_vm_prt_put(cb->adev);
	kfree(cb);
}

/**
 * amdgpu_vm_add_prt_cb - add callback for updating the PRT status
 *
 * @adev: amdgpu_device pointer
 * @fence: fence for the callback
 */
static void amdgpu_vm_add_prt_cb(struct amdgpu_device *adev,
				 struct dma_fence *fence)
{
	struct amdgpu_prt_cb *cb;

	if (!adev->gmc.gmc_funcs->set_prt)
		return;

	cb = kmalloc(sizeof(struct amdgpu_prt_cb), GFP_KERNEL);
	if (!cb) {
		/* Last resort when we are OOM */
		if (fence)
			dma_fence_wait(fence, false);

		amdgpu_vm_prt_put(adev);
	} else {
		cb->adev = adev;
		if (!fence || dma_fence_add_callback(fence, &cb->cb,
						     amdgpu_vm_prt_cb))
			amdgpu_vm_prt_cb(fence, &cb->cb);
	}
}

/**
 * amdgpu_vm_free_mapping - free a mapping
 *
 * @adev: amdgpu_device pointer
 * @vm: requested vm
 * @mapping: mapping to be freed
 * @fence: fence of the unmap operation
 *
 * Free a mapping and make sure we decrease the PRT usage count if applicable.
 */
static void amdgpu_vm_free_mapping(struct amdgpu_device *adev,
				   struct amdgpu_vm *vm,
				   struct amdgpu_bo_va_mapping *mapping,
				   struct dma_fence *fence)
{
	if (mapping->flags & AMDGPU_PTE_PRT)
		amdgpu_vm_add_prt_cb(adev, fence);
	kfree(mapping);
}

/**
 * amdgpu_vm_prt_fini - finish all prt mappings
 *
 * @adev: amdgpu_device pointer
 * @vm: requested vm
 *
 * Register a cleanup callback to disable PRT support after VM dies.
 */
static void amdgpu_vm_prt_fini(struct amdgpu_device *adev, struct amdgpu_vm *vm)
{
	struct reservation_object *resv = vm->root.base.bo->tbo.resv;
	struct dma_fence *excl, **shared;
	unsigned i, shared_count;
	int r;

	r = reservation_object_get_fences_rcu(resv, &excl,
					      &shared_count, &shared);
	if (r) {
		/* Not enough memory to grab the fence list, as last resort
		 * block for all the fences to complete.
		 */
		reservation_object_wait_timeout_rcu(resv, true, false,
						    MAX_SCHEDULE_TIMEOUT);
		return;
	}

	/* Add a callback for each fence in the reservation object */
	amdgpu_vm_prt_get(adev);
	amdgpu_vm_add_prt_cb(adev, excl);

	for (i = 0; i < shared_count; ++i) {
		amdgpu_vm_prt_get(adev);
		amdgpu_vm_add_prt_cb(adev, shared[i]);
	}

	kfree(shared);
}

/**
 * amdgpu_vm_clear_freed - clear freed BOs in the PT
 *
 * @adev: amdgpu_device pointer
 * @vm: requested vm
 * @fence: optional resulting fence (unchanged if no work needed to be done
 * or if an error occurred)
 *
 * Make sure all freed BOs are cleared in the PT.
 * PTs have to be reserved and mutex must be locked!
 *
 * Returns:
 * 0 for success.
 *
 */
int amdgpu_vm_clear_freed(struct amdgpu_device *adev,
			  struct amdgpu_vm *vm,
			  struct dma_fence **fence)
{
	struct amdgpu_bo_va_mapping *mapping;
	uint64_t init_pte_value = 0;
	struct dma_fence *f = NULL;
	int r;

	while (!list_empty(&vm->freed)) {
		mapping = list_first_entry(&vm->freed,
			struct amdgpu_bo_va_mapping, list);
		list_del(&mapping->list);

		if (vm->pte_support_ats && mapping->start < AMDGPU_VA_HOLE_START)
			init_pte_value = AMDGPU_PTE_DEFAULT_ATC;

		r = amdgpu_vm_bo_update_mapping(adev, NULL, NULL, vm,
						mapping->start, mapping->last,
						init_pte_value, 0, &f);
		amdgpu_vm_free_mapping(adev, vm, mapping, f);
		if (r) {
			dma_fence_put(f);
			return r;
		}
	}

	if (fence && f) {
		dma_fence_put(*fence);
		*fence = f;
	} else {
		dma_fence_put(f);
	}

	return 0;

}

/**
 * amdgpu_vm_handle_moved - handle moved BOs in the PT
 *
 * @adev: amdgpu_device pointer
 * @vm: requested vm
 *
 * Make sure all BOs which are moved are updated in the PTs.
 *
 * Returns:
 * 0 for success.
 *
 * PTs have to be reserved!
 */
int amdgpu_vm_handle_moved(struct amdgpu_device *adev,
			   struct amdgpu_vm *vm)
{
	struct amdgpu_bo_va *bo_va, *tmp;
	struct list_head moved;
	bool clear;
	int r;

	INIT_LIST_HEAD(&moved);
	spin_lock(&vm->moved_lock);
	list_splice_init(&vm->moved, &moved);
	spin_unlock(&vm->moved_lock);

	list_for_each_entry_safe(bo_va, tmp, &moved, base.vm_status) {
		struct reservation_object *resv = bo_va->base.bo->tbo.resv;

		/* Per VM BOs never need to bo cleared in the page tables */
		if (resv == vm->root.base.bo->tbo.resv)
			clear = false;
		/* Try to reserve the BO to avoid clearing its ptes */
		else if (!amdgpu_vm_debug && reservation_object_trylock(resv))
			clear = false;
		/* Somebody else is using the BO right now */
		else
			clear = true;

		r = amdgpu_vm_bo_update(adev, bo_va, clear);
		if (r) {
			spin_lock(&vm->moved_lock);
			list_splice(&moved, &vm->moved);
			spin_unlock(&vm->moved_lock);
			return r;
		}

		if (!clear && resv != vm->root.base.bo->tbo.resv)
			reservation_object_unlock(resv);

	}

	return 0;
}

/**
 * amdgpu_vm_bo_add - add a bo to a specific vm
 *
 * @adev: amdgpu_device pointer
 * @vm: requested vm
 * @bo: amdgpu buffer object
 *
 * Add @bo into the requested vm.
 * Add @bo to the list of bos associated with the vm
 *
 * Returns:
 * Newly added bo_va or NULL for failure
 *
 * Object has to be reserved!
 */
struct amdgpu_bo_va *amdgpu_vm_bo_add(struct amdgpu_device *adev,
				      struct amdgpu_vm *vm,
				      struct amdgpu_bo *bo)
{
	struct amdgpu_bo_va *bo_va;

	bo_va = kzalloc(sizeof(struct amdgpu_bo_va), GFP_KERNEL);
	if (bo_va == NULL) {
		return NULL;
	}
	amdgpu_vm_bo_base_init(&bo_va->base, vm, bo);

	bo_va->ref_count = 1;
	INIT_LIST_HEAD(&bo_va->valids);
	INIT_LIST_HEAD(&bo_va->invalids);

	return bo_va;
}


/**
 * amdgpu_vm_bo_insert_mapping - insert a new mapping
 *
 * @adev: amdgpu_device pointer
 * @bo_va: bo_va to store the address
 * @mapping: the mapping to insert
 *
 * Insert a new mapping into all structures.
 */
static void amdgpu_vm_bo_insert_map(struct amdgpu_device *adev,
				    struct amdgpu_bo_va *bo_va,
				    struct amdgpu_bo_va_mapping *mapping)
{
	struct amdgpu_vm *vm = bo_va->base.vm;
	struct amdgpu_bo *bo = bo_va->base.bo;

	mapping->bo_va = bo_va;
	list_add(&mapping->list, &bo_va->invalids);
	amdgpu_vm_it_insert(mapping, &vm->va);

	if (mapping->flags & AMDGPU_PTE_PRT)
		amdgpu_vm_prt_get(adev);

	if (bo && bo->tbo.resv == vm->root.base.bo->tbo.resv &&
	    !bo_va->base.moved) {
		spin_lock(&vm->moved_lock);
		list_move(&bo_va->base.vm_status, &vm->moved);
		spin_unlock(&vm->moved_lock);
	}
	trace_amdgpu_vm_bo_map(bo_va, mapping);
}

/**
 * amdgpu_vm_bo_map - map bo inside a vm
 *
 * @adev: amdgpu_device pointer
 * @bo_va: bo_va to store the address
 * @saddr: where to map the BO
 * @offset: requested offset in the BO
 * @size: BO size in bytes
 * @flags: attributes of pages (read/write/valid/etc.)
 *
 * Add a mapping of the BO at the specefied addr into the VM.
 *
 * Returns:
 * 0 for success, error for failure.
 *
 * Object has to be reserved and unreserved outside!
 */
int amdgpu_vm_bo_map(struct amdgpu_device *adev,
		     struct amdgpu_bo_va *bo_va,
		     uint64_t saddr, uint64_t offset,
		     uint64_t size, uint64_t flags)
{
	struct amdgpu_bo_va_mapping *mapping, *tmp;
	struct amdgpu_bo *bo = bo_va->base.bo;
	struct amdgpu_vm *vm = bo_va->base.vm;
	uint64_t eaddr;

	/* validate the parameters */
	if (saddr & AMDGPU_GPU_PAGE_MASK || offset & AMDGPU_GPU_PAGE_MASK ||
	    size == 0 || size & AMDGPU_GPU_PAGE_MASK)
		return -EINVAL;

	/* make sure object fit at this offset */
	eaddr = saddr + size - 1;
	if (saddr >= eaddr ||
	    (bo && offset + size > amdgpu_bo_size(bo)))
		return -EINVAL;

	saddr /= AMDGPU_GPU_PAGE_SIZE;
	eaddr /= AMDGPU_GPU_PAGE_SIZE;

	tmp = amdgpu_vm_it_iter_first(&vm->va, saddr, eaddr);
	if (tmp) {
		/* bo and tmp overlap, invalid addr */
		dev_err(adev->dev, "bo %p va 0x%010Lx-0x%010Lx conflict with "
			"0x%010Lx-0x%010Lx\n", bo, saddr, eaddr,
			tmp->start, tmp->last + 1);
		return -EINVAL;
	}

	mapping = kmalloc(sizeof(*mapping), GFP_KERNEL);
	if (!mapping)
		return -ENOMEM;

	mapping->start = saddr;
	mapping->last = eaddr;
	mapping->offset = offset;
	mapping->flags = flags;

	amdgpu_vm_bo_insert_map(adev, bo_va, mapping);

	return 0;
}

/**
 * amdgpu_vm_bo_replace_map - map bo inside a vm, replacing existing mappings
 *
 * @adev: amdgpu_device pointer
 * @bo_va: bo_va to store the address
 * @saddr: where to map the BO
 * @offset: requested offset in the BO
 * @size: BO size in bytes
 * @flags: attributes of pages (read/write/valid/etc.)
 *
 * Add a mapping of the BO at the specefied addr into the VM. Replace existing
 * mappings as we do so.
 *
 * Returns:
 * 0 for success, error for failure.
 *
 * Object has to be reserved and unreserved outside!
 */
int amdgpu_vm_bo_replace_map(struct amdgpu_device *adev,
			     struct amdgpu_bo_va *bo_va,
			     uint64_t saddr, uint64_t offset,
			     uint64_t size, uint64_t flags)
{
	struct amdgpu_bo_va_mapping *mapping;
	struct amdgpu_bo *bo = bo_va->base.bo;
	uint64_t eaddr;
	int r;

	/* validate the parameters */
	if (saddr & AMDGPU_GPU_PAGE_MASK || offset & AMDGPU_GPU_PAGE_MASK ||
	    size == 0 || size & AMDGPU_GPU_PAGE_MASK)
		return -EINVAL;

	/* make sure object fit at this offset */
	eaddr = saddr + size - 1;
	if (saddr >= eaddr ||
	    (bo && offset + size > amdgpu_bo_size(bo)))
		return -EINVAL;

	/* Allocate all the needed memory */
	mapping = kmalloc(sizeof(*mapping), GFP_KERNEL);
	if (!mapping)
		return -ENOMEM;

	r = amdgpu_vm_bo_clear_mappings(adev, bo_va->base.vm, saddr, size);
	if (r) {
		kfree(mapping);
		return r;
	}

	saddr /= AMDGPU_GPU_PAGE_SIZE;
	eaddr /= AMDGPU_GPU_PAGE_SIZE;

	mapping->start = saddr;
	mapping->last = eaddr;
	mapping->offset = offset;
	mapping->flags = flags;

	amdgpu_vm_bo_insert_map(adev, bo_va, mapping);

	return 0;
}

/**
 * amdgpu_vm_bo_unmap - remove bo mapping from vm
 *
 * @adev: amdgpu_device pointer
 * @bo_va: bo_va to remove the address from
 * @saddr: where to the BO is mapped
 *
 * Remove a mapping of the BO at the specefied addr from the VM.
 *
 * Returns:
 * 0 for success, error for failure.
 *
 * Object has to be reserved and unreserved outside!
 */
int amdgpu_vm_bo_unmap(struct amdgpu_device *adev,
		       struct amdgpu_bo_va *bo_va,
		       uint64_t saddr)
{
	struct amdgpu_bo_va_mapping *mapping;
	struct amdgpu_vm *vm = bo_va->base.vm;
	bool valid = true;

	saddr /= AMDGPU_GPU_PAGE_SIZE;

	list_for_each_entry(mapping, &bo_va->valids, list) {
		if (mapping->start == saddr)
			break;
	}

	if (&mapping->list == &bo_va->valids) {
		valid = false;

		list_for_each_entry(mapping, &bo_va->invalids, list) {
			if (mapping->start == saddr)
				break;
		}

		if (&mapping->list == &bo_va->invalids)
			return -ENOENT;
	}

	list_del(&mapping->list);
	amdgpu_vm_it_remove(mapping, &vm->va);
	mapping->bo_va = NULL;
	trace_amdgpu_vm_bo_unmap(bo_va, mapping);

	if (valid)
		list_add(&mapping->list, &vm->freed);
	else
		amdgpu_vm_free_mapping(adev, vm, mapping,
				       bo_va->last_pt_update);

	return 0;
}

/**
 * amdgpu_vm_bo_clear_mappings - remove all mappings in a specific range
 *
 * @adev: amdgpu_device pointer
 * @vm: VM structure to use
 * @saddr: start of the range
 * @size: size of the range
 *
 * Remove all mappings in a range, split them as appropriate.
 *
 * Returns:
 * 0 for success, error for failure.
 */
int amdgpu_vm_bo_clear_mappings(struct amdgpu_device *adev,
				struct amdgpu_vm *vm,
				uint64_t saddr, uint64_t size)
{
	struct amdgpu_bo_va_mapping *before, *after, *tmp, *next;
	DRM_LIST_HEAD(removed);
	uint64_t eaddr;

	eaddr = saddr + size - 1;
	saddr /= AMDGPU_GPU_PAGE_SIZE;
	eaddr /= AMDGPU_GPU_PAGE_SIZE;

	/* Allocate all the needed memory */
	before = kzalloc(sizeof(*before), GFP_KERNEL);
	if (!before)
		return -ENOMEM;
	INIT_LIST_HEAD(&before->list);

	after = kzalloc(sizeof(*after), GFP_KERNEL);
	if (!after) {
		kfree(before);
		return -ENOMEM;
	}
	INIT_LIST_HEAD(&after->list);

	/* Now gather all removed mappings */
	tmp = amdgpu_vm_it_iter_first(&vm->va, saddr, eaddr);
	while (tmp) {
		/* Remember mapping split at the start */
		if (tmp->start < saddr) {
			before->start = tmp->start;
			before->last = saddr - 1;
			before->offset = tmp->offset;
			before->flags = tmp->flags;
			before->bo_va = tmp->bo_va;
			list_add(&before->list, &tmp->bo_va->invalids);
		}

		/* Remember mapping split at the end */
		if (tmp->last > eaddr) {
			after->start = eaddr + 1;
			after->last = tmp->last;
			after->offset = tmp->offset;
			after->offset += after->start - tmp->start;
			after->flags = tmp->flags;
			after->bo_va = tmp->bo_va;
			list_add(&after->list, &tmp->bo_va->invalids);
		}

		list_del(&tmp->list);
		list_add(&tmp->list, &removed);

		tmp = amdgpu_vm_it_iter_next(tmp, saddr, eaddr);
	}

	/* And free them up */
	list_for_each_entry_safe(tmp, next, &removed, list) {
		amdgpu_vm_it_remove(tmp, &vm->va);
		list_del(&tmp->list);

		if (tmp->start < saddr)
		    tmp->start = saddr;
		if (tmp->last > eaddr)
		    tmp->last = eaddr;

		tmp->bo_va = NULL;
		list_add(&tmp->list, &vm->freed);
		trace_amdgpu_vm_bo_unmap(NULL, tmp);
	}

	/* Insert partial mapping before the range */
	if (!list_empty(&before->list)) {
		amdgpu_vm_it_insert(before, &vm->va);
		if (before->flags & AMDGPU_PTE_PRT)
			amdgpu_vm_prt_get(adev);
	} else {
		kfree(before);
	}

	/* Insert partial mapping after the range */
	if (!list_empty(&after->list)) {
		amdgpu_vm_it_insert(after, &vm->va);
		if (after->flags & AMDGPU_PTE_PRT)
			amdgpu_vm_prt_get(adev);
	} else {
		kfree(after);
	}

	return 0;
}

/**
 * amdgpu_vm_bo_lookup_mapping - find mapping by address
 *
 * @vm: the requested VM
 * @addr: the address
 *
 * Find a mapping by it's address.
 *
 * Returns:
 * The amdgpu_bo_va_mapping matching for addr or NULL
 *
 */
struct amdgpu_bo_va_mapping *amdgpu_vm_bo_lookup_mapping(struct amdgpu_vm *vm,
							 uint64_t addr)
{
	return amdgpu_vm_it_iter_first(&vm->va, addr, addr);
}

/**
 * amdgpu_vm_bo_trace_cs - trace all reserved mappings
 *
 * @vm: the requested vm
 * @ticket: CS ticket
 *
 * Trace all mappings of BOs reserved during a command submission.
 */
void amdgpu_vm_bo_trace_cs(struct amdgpu_vm *vm, struct ww_acquire_ctx *ticket)
{
	struct amdgpu_bo_va_mapping *mapping;

	if (!trace_amdgpu_vm_bo_cs_enabled())
		return;

	for (mapping = amdgpu_vm_it_iter_first(&vm->va, 0, U64_MAX); mapping;
	     mapping = amdgpu_vm_it_iter_next(mapping, 0, U64_MAX)) {
		if (mapping->bo_va && mapping->bo_va->base.bo) {
			struct amdgpu_bo *bo;

			bo = mapping->bo_va->base.bo;
			if (READ_ONCE(bo->tbo.resv->lock.ctx) != ticket)
				continue;
		}

		trace_amdgpu_vm_bo_cs(mapping);
	}
}

/**
 * amdgpu_vm_bo_rmv - remove a bo to a specific vm
 *
 * @adev: amdgpu_device pointer
 * @bo_va: requested bo_va
 *
 * Remove @bo_va->bo from the requested vm.
 *
 * Object have to be reserved!
 */
void amdgpu_vm_bo_rmv(struct amdgpu_device *adev,
		      struct amdgpu_bo_va *bo_va)
{
	struct amdgpu_bo_va_mapping *mapping, *next;
	struct amdgpu_vm *vm = bo_va->base.vm;

	list_del(&bo_va->base.bo_list);

	spin_lock(&vm->moved_lock);
	list_del(&bo_va->base.vm_status);
	spin_unlock(&vm->moved_lock);

	list_for_each_entry_safe(mapping, next, &bo_va->valids, list) {
		list_del(&mapping->list);
		amdgpu_vm_it_remove(mapping, &vm->va);
		mapping->bo_va = NULL;
		trace_amdgpu_vm_bo_unmap(bo_va, mapping);
		list_add(&mapping->list, &vm->freed);
	}
	list_for_each_entry_safe(mapping, next, &bo_va->invalids, list) {
		list_del(&mapping->list);
		amdgpu_vm_it_remove(mapping, &vm->va);
		amdgpu_vm_free_mapping(adev, vm, mapping,
				       bo_va->last_pt_update);
	}

	dma_fence_put(bo_va->last_pt_update);
	kfree(bo_va);
}

/**
 * amdgpu_vm_bo_invalidate - mark the bo as invalid
 *
 * @adev: amdgpu_device pointer
 * @bo: amdgpu buffer object
 * @evicted: is the BO evicted
 *
 * Mark @bo as invalid.
 */
void amdgpu_vm_bo_invalidate(struct amdgpu_device *adev,
			     struct amdgpu_bo *bo, bool evicted)
{
	struct amdgpu_vm_bo_base *bo_base;

	/* shadow bo doesn't have bo base, its validation needs its parent */
	if (bo->parent && bo->parent->shadow == bo)
		bo = bo->parent;

	list_for_each_entry(bo_base, &bo->va, bo_list) {
		struct amdgpu_vm *vm = bo_base->vm;
		bool was_moved = bo_base->moved;

		bo_base->moved = true;
		if (evicted && bo->tbo.resv == vm->root.base.bo->tbo.resv) {
			if (bo->tbo.type == ttm_bo_type_kernel)
				list_move(&bo_base->vm_status, &vm->evicted);
			else
				list_move_tail(&bo_base->vm_status,
					       &vm->evicted);
			continue;
		}

		if (was_moved)
			continue;

		if (bo->tbo.type == ttm_bo_type_kernel) {
			list_move(&bo_base->vm_status, &vm->relocated);
		} else {
			spin_lock(&bo_base->vm->moved_lock);
			list_move(&bo_base->vm_status, &vm->moved);
			spin_unlock(&bo_base->vm->moved_lock);
		}
	}
}

/**
 * amdgpu_vm_get_block_size - calculate VM page table size as power of two
 *
 * @vm_size: VM size
 *
 * Returns:
 * VM page table as power of two
 */
static uint32_t amdgpu_vm_get_block_size(uint64_t vm_size)
{
	/* Total bits covered by PD + PTs */
	unsigned bits = ilog2(vm_size) + 18;

	/* Make sure the PD is 4K in size up to 8GB address space.
	   Above that split equal between PD and PTs */
	if (vm_size <= 8)
		return (bits - 9);
	else
		return ((bits + 3) / 2);
}

/**
 * amdgpu_vm_adjust_size - adjust vm size, block size and fragment size
 *
 * @adev: amdgpu_device pointer
 * @min_vm_size: the minimum vm size in GB if it's set auto
 * @fragment_size_default: Default PTE fragment size
 * @max_level: max VMPT level
 * @max_bits: max address space size in bits
 *
 */
void amdgpu_vm_adjust_size(struct amdgpu_device *adev, uint32_t min_vm_size,
			   uint32_t fragment_size_default, unsigned max_level,
			   unsigned max_bits)
{
	unsigned int max_size = 1 << (max_bits - 30);
	unsigned int vm_size;
	uint64_t tmp;

	/* adjust vm size first */
	if (amdgpu_vm_size != -1) {
		vm_size = amdgpu_vm_size;
		if (vm_size > max_size) {
			dev_warn(adev->dev, "VM size (%d) too large, max is %u GB\n",
				 amdgpu_vm_size, max_size);
			vm_size = max_size;
		}
	} else {
#ifdef __linux__
		struct sysinfo si;
#endif
		unsigned int phys_ram_gb;

		/* Optimal VM size depends on the amount of physical
		 * RAM available. Underlying requirements and
		 * assumptions:
		 *
		 *  - Need to map system memory and VRAM from all GPUs
		 *     - VRAM from other GPUs not known here
		 *     - Assume VRAM <= system memory
		 *  - On GFX8 and older, VM space can be segmented for
		 *    different MTYPEs
		 *  - Need to allow room for fragmentation, guard pages etc.
		 *
		 * This adds up to a rough guess of system memory x3.
		 * Round up to power of two to maximize the available
		 * VM size with the given page table size.
		 */
#ifdef __linux__
		si_meminfo(&si);
		phys_ram_gb = ((uint64_t)si.totalram * si.mem_unit +
			       (1 << 30) - 1) >> 30;
#else
		phys_ram_gb = ((uint64_t)ptoa(physmem) +
			       (1 << 30) - 1) >> 30;
#endif
		vm_size = roundup_pow_of_two(
			min(max(phys_ram_gb * 3, min_vm_size), max_size));
	}

	adev->vm_manager.max_pfn = (uint64_t)vm_size << 18;

	tmp = roundup_pow_of_two(adev->vm_manager.max_pfn);
	if (amdgpu_vm_block_size != -1)
		tmp >>= amdgpu_vm_block_size - 9;
	tmp = DIV_ROUND_UP(fls64(tmp) - 1, 9) - 1;
	adev->vm_manager.num_level = min(max_level, (unsigned)tmp);
	switch (adev->vm_manager.num_level) {
	case 3:
		adev->vm_manager.root_level = AMDGPU_VM_PDB2;
		break;
	case 2:
		adev->vm_manager.root_level = AMDGPU_VM_PDB1;
		break;
	case 1:
		adev->vm_manager.root_level = AMDGPU_VM_PDB0;
		break;
	default:
		dev_err(adev->dev, "VMPT only supports 2~4+1 levels\n");
	}
	/* block size depends on vm size and hw setup*/
	if (amdgpu_vm_block_size != -1)
		adev->vm_manager.block_size =
			min((unsigned)amdgpu_vm_block_size, max_bits
			    - AMDGPU_GPU_PAGE_SHIFT
			    - 9 * adev->vm_manager.num_level);
	else if (adev->vm_manager.num_level > 1)
		adev->vm_manager.block_size = 9;
	else
		adev->vm_manager.block_size = amdgpu_vm_get_block_size(tmp);

	if (amdgpu_vm_fragment_size == -1)
		adev->vm_manager.fragment_size = fragment_size_default;
	else
		adev->vm_manager.fragment_size = amdgpu_vm_fragment_size;

	DRM_INFO("vm size is %u GB, %u levels, block size is %u-bit, fragment size is %u-bit\n",
		 vm_size, adev->vm_manager.num_level + 1,
		 adev->vm_manager.block_size,
		 adev->vm_manager.fragment_size);
}

/**
 * amdgpu_vm_init - initialize a vm instance
 *
 * @adev: amdgpu_device pointer
 * @vm: requested vm
 * @vm_context: Indicates if it GFX or Compute context
 * @pasid: Process address space identifier
 *
 * Init @vm fields.
 *
 * Returns:
 * 0 for success, error for failure.
 */
int amdgpu_vm_init(struct amdgpu_device *adev, struct amdgpu_vm *vm,
		   int vm_context, unsigned int pasid)
{
	struct amdgpu_bo_param bp;
	struct amdgpu_bo *root;
	const unsigned align = min(AMDGPU_VM_PTB_ALIGN_SIZE,
		AMDGPU_VM_PTE_COUNT(adev) * 8);
	unsigned ring_instance;
	struct amdgpu_ring *ring;
	struct drm_sched_rq *rq;
	unsigned long size;
	uint64_t flags;
	int r, i;

	vm->va = RB_ROOT_CACHED;
	for (i = 0; i < AMDGPU_MAX_VMHUBS; i++)
		vm->reserved_vmid[i] = NULL;
	INIT_LIST_HEAD(&vm->evicted);
	INIT_LIST_HEAD(&vm->relocated);
	mtx_init(&vm->moved_lock, IPL_TTY);
	INIT_LIST_HEAD(&vm->moved);
	INIT_LIST_HEAD(&vm->idle);
	INIT_LIST_HEAD(&vm->freed);

	/* create scheduler entity for page table updates */

	ring_instance = atomic_inc_return(&adev->vm_manager.vm_pte_next_ring);
	ring_instance %= adev->vm_manager.vm_pte_num_rings;
	ring = adev->vm_manager.vm_pte_rings[ring_instance];
	rq = &ring->sched.sched_rq[DRM_SCHED_PRIORITY_KERNEL];
	r = drm_sched_entity_init(&vm->entity, &rq, 1, NULL);
	if (r)
		return r;

	vm->pte_support_ats = false;

	if (vm_context == AMDGPU_VM_CONTEXT_COMPUTE) {
		vm->use_cpu_for_update = !!(adev->vm_manager.vm_update_mode &
						AMDGPU_VM_USE_CPU_FOR_COMPUTE);

		if (adev->asic_type == CHIP_RAVEN)
			vm->pte_support_ats = true;
	} else {
		vm->use_cpu_for_update = !!(adev->vm_manager.vm_update_mode &
						AMDGPU_VM_USE_CPU_FOR_GFX);
	}
	DRM_DEBUG_DRIVER("VM update mode is %s\n",
			 vm->use_cpu_for_update ? "CPU" : "SDMA");
	WARN_ONCE((vm->use_cpu_for_update & !amdgpu_gmc_vram_full_visible(&adev->gmc)),
		  "CPU update of VM recommended only for large BAR system\n");
	vm->last_update = NULL;

	flags = AMDGPU_GEM_CREATE_VRAM_CONTIGUOUS;
	if (vm->use_cpu_for_update)
		flags |= AMDGPU_GEM_CREATE_CPU_ACCESS_REQUIRED;
	else if (vm_context != AMDGPU_VM_CONTEXT_COMPUTE)
		flags |= AMDGPU_GEM_CREATE_SHADOW;

	size = amdgpu_vm_bo_size(adev, adev->vm_manager.root_level);
	memset(&bp, 0, sizeof(bp));
	bp.size = size;
	bp.byte_align = align;
	bp.domain = AMDGPU_GEM_DOMAIN_VRAM;
	bp.flags = flags;
	bp.type = ttm_bo_type_kernel;
	bp.resv = NULL;
	r = amdgpu_bo_create(adev, &bp, &root);
	if (r)
		goto error_free_sched_entity;

	r = amdgpu_bo_reserve(root, true);
	if (r)
		goto error_free_root;

	r = amdgpu_vm_clear_bo(adev, vm, root,
			       adev->vm_manager.root_level,
			       vm->pte_support_ats);
	if (r)
		goto error_unreserve;

	amdgpu_vm_bo_base_init(&vm->root.base, vm, root);
	amdgpu_bo_unreserve(vm->root.base.bo);

	if (pasid) {
		unsigned long flags;

		spin_lock_irqsave(&adev->vm_manager.pasid_lock, flags);
		r = idr_alloc(&adev->vm_manager.pasid_idr, vm, pasid, pasid + 1,
			      GFP_ATOMIC);
		spin_unlock_irqrestore(&adev->vm_manager.pasid_lock, flags);
		if (r < 0)
			goto error_free_root;

		vm->pasid = pasid;
	}

#ifdef __linux__
	INIT_KFIFO(vm->faults);
#else
	SIMPLEQ_INIT(&vm->faults);
#endif
	vm->fault_credit = 16;

	return 0;

error_unreserve:
	amdgpu_bo_unreserve(vm->root.base.bo);

error_free_root:
	amdgpu_bo_unref(&vm->root.base.bo->shadow);
	amdgpu_bo_unref(&vm->root.base.bo);
	vm->root.base.bo = NULL;

error_free_sched_entity:
	drm_sched_entity_destroy(&vm->entity);

	return r;
}

/**
 * amdgpu_vm_make_compute - Turn a GFX VM into a compute VM
 *
 * @adev: amdgpu_device pointer
 * @vm: requested vm
 *
 * This only works on GFX VMs that don't have any BOs added and no
 * page tables allocated yet.
 *
 * Changes the following VM parameters:
 * - use_cpu_for_update
 * - pte_supports_ats
 * - pasid (old PASID is released, because compute manages its own PASIDs)
 *
 * Reinitializes the page directory to reflect the changed ATS
 * setting.
 *
 * Returns:
 * 0 for success, -errno for errors.
 */
int amdgpu_vm_make_compute(struct amdgpu_device *adev, struct amdgpu_vm *vm)
{
	bool pte_support_ats = (adev->asic_type == CHIP_RAVEN);
	int r;

	r = amdgpu_bo_reserve(vm->root.base.bo, true);
	if (r)
		return r;

	/* Sanity checks */
#ifdef __linux__
	if (!RB_EMPTY_ROOT(&vm->va.rb_root) || vm->root.entries) {
#else
	if (!RB_EMPTY_ROOT(&vm->va) || vm->root.entries) {
#endif
		r = -EINVAL;
		goto error;
	}

	/* Check if PD needs to be reinitialized and do it before
	 * changing any other state, in case it fails.
	 */
	if (pte_support_ats != vm->pte_support_ats) {
		r = amdgpu_vm_clear_bo(adev, vm, vm->root.base.bo,
			       adev->vm_manager.root_level,
			       pte_support_ats);
		if (r)
			goto error;
	}

	/* Update VM state */
	vm->use_cpu_for_update = !!(adev->vm_manager.vm_update_mode &
				    AMDGPU_VM_USE_CPU_FOR_COMPUTE);
	vm->pte_support_ats = pte_support_ats;
	DRM_DEBUG_DRIVER("VM update mode is %s\n",
			 vm->use_cpu_for_update ? "CPU" : "SDMA");
	WARN_ONCE((vm->use_cpu_for_update & !amdgpu_gmc_vram_full_visible(&adev->gmc)),
		  "CPU update of VM recommended only for large BAR system\n");

	if (vm->pasid) {
		unsigned long flags;

		spin_lock_irqsave(&adev->vm_manager.pasid_lock, flags);
		idr_remove(&adev->vm_manager.pasid_idr, vm->pasid);
		spin_unlock_irqrestore(&adev->vm_manager.pasid_lock, flags);

		vm->pasid = 0;
	}

	/* Free the shadow bo for compute VM */
	amdgpu_bo_unref(&vm->root.base.bo->shadow);

error:
	amdgpu_bo_unreserve(vm->root.base.bo);
	return r;
}

/**
 * amdgpu_vm_free_levels - free PD/PT levels
 *
 * @adev: amdgpu device structure
 * @parent: PD/PT starting level to free
 * @level: level of parent structure
 *
 * Free the page directory or page table level and all sub levels.
 */
static void amdgpu_vm_free_levels(struct amdgpu_device *adev,
				  struct amdgpu_vm_pt *parent,
				  unsigned level)
{
	unsigned i, num_entries = amdgpu_vm_num_entries(adev, level);

	if (parent->base.bo) {
		list_del(&parent->base.bo_list);
		list_del(&parent->base.vm_status);
		amdgpu_bo_unref(&parent->base.bo->shadow);
		amdgpu_bo_unref(&parent->base.bo);
	}

	if (parent->entries)
		for (i = 0; i < num_entries; i++)
			amdgpu_vm_free_levels(adev, &parent->entries[i],
					      level + 1);

	kvfree(parent->entries);
}

/**
 * amdgpu_vm_fini - tear down a vm instance
 *
 * @adev: amdgpu_device pointer
 * @vm: requested vm
 *
 * Tear down @vm.
 * Unbind the VM and remove all bos from the vm bo list
 */
void amdgpu_vm_fini(struct amdgpu_device *adev, struct amdgpu_vm *vm)
{
	struct amdgpu_bo_va_mapping *mapping, *tmp;
	bool prt_fini_needed = !!adev->gmc.gmc_funcs->set_prt;
	struct amdgpu_bo *root;
	u64 fault;
	int i, r;
	struct amdgpu_vm_fault *vmf;

	amdgpu_amdkfd_gpuvm_destroy_cb(adev, vm);

	/* Clear pending page faults from IH when the VM is destroyed */
#ifdef __linux__
	while (kfifo_get(&vm->faults, &fault))
		amdgpu_ih_clear_fault(adev, fault);
#else
	while (!SIMPLEQ_EMPTY(&vm->faults)) {
		vmf = SIMPLEQ_FIRST(&vm->faults);
		fault = vmf->val;
		SIMPLEQ_REMOVE_HEAD(&vm->faults, vm_fault_entry);
		free(vmf, M_DRM, sizeof(*vmf));
		amdgpu_ih_clear_fault(adev, fault);
	}
#endif

	if (vm->pasid) {
		unsigned long flags;

		spin_lock_irqsave(&adev->vm_manager.pasid_lock, flags);
		idr_remove(&adev->vm_manager.pasid_idr, vm->pasid);
		spin_unlock_irqrestore(&adev->vm_manager.pasid_lock, flags);
	}

	drm_sched_entity_destroy(&vm->entity);

#ifdef __linux__
	if (!RB_EMPTY_ROOT(&vm->va.rb_root)) {
		dev_err(adev->dev, "still active bo inside vm\n");
	}
	rbtree_postorder_for_each_entry_safe(mapping, tmp,
					     &vm->va.rb_root, rb) {
		list_del(&mapping->list);
		amdgpu_vm_it_remove(mapping, &vm->va);
		kfree(mapping);
	}
#else
	if (!RB_EMPTY_ROOT(&vm->va)) {
		dev_err(adev->dev, "still active bo inside vm\n");
	}
	rbtree_postorder_for_each_entry_safe(mapping, tmp,
					     &vm->va, rb) {
		list_del(&mapping->list);
		amdgpu_vm_it_remove(mapping, &vm->va);
		kfree(mapping);
	}
#endif
	list_for_each_entry_safe(mapping, tmp, &vm->freed, list) {
		if (mapping->flags & AMDGPU_PTE_PRT && prt_fini_needed) {
			amdgpu_vm_prt_fini(adev, vm);
			prt_fini_needed = false;
		}

		list_del(&mapping->list);
		amdgpu_vm_free_mapping(adev, vm, mapping, NULL);
	}

	root = amdgpu_bo_ref(vm->root.base.bo);
	r = amdgpu_bo_reserve(root, true);
	if (r) {
		dev_err(adev->dev, "Leaking page tables because BO reservation failed\n");
	} else {
		amdgpu_vm_free_levels(adev, &vm->root,
				      adev->vm_manager.root_level);
		amdgpu_bo_unreserve(root);
	}
	amdgpu_bo_unref(&root);
	dma_fence_put(vm->last_update);
	for (i = 0; i < AMDGPU_MAX_VMHUBS; i++)
		amdgpu_vmid_free_reserved(adev, vm, i);
}

/**
 * amdgpu_vm_pasid_fault_credit - Check fault credit for given PASID
 *
 * @adev: amdgpu_device pointer
 * @pasid: PASID do identify the VM
 *
 * This function is expected to be called in interrupt context.
 *
 * Returns:
 * True if there was fault credit, false otherwise
 */
bool amdgpu_vm_pasid_fault_credit(struct amdgpu_device *adev,
				  unsigned int pasid)
{
	struct amdgpu_vm *vm;

	spin_lock(&adev->vm_manager.pasid_lock);
	vm = idr_find(&adev->vm_manager.pasid_idr, pasid);
	if (!vm) {
		/* VM not found, can't track fault credit */
		spin_unlock(&adev->vm_manager.pasid_lock);
		return true;
	}

	/* No lock needed. only accessed by IRQ handler */
	if (!vm->fault_credit) {
		/* Too many faults in this VM */
		spin_unlock(&adev->vm_manager.pasid_lock);
		return false;
	}

	vm->fault_credit--;
	spin_unlock(&adev->vm_manager.pasid_lock);
	return true;
}

/**
 * amdgpu_vm_manager_init - init the VM manager
 *
 * @adev: amdgpu_device pointer
 *
 * Initialize the VM manager structures
 */
void amdgpu_vm_manager_init(struct amdgpu_device *adev)
{
	unsigned i;

	amdgpu_vmid_mgr_init(adev);

	adev->vm_manager.fence_context =
		dma_fence_context_alloc(AMDGPU_MAX_RINGS);
	for (i = 0; i < AMDGPU_MAX_RINGS; ++i)
		adev->vm_manager.seqno[i] = 0;

	atomic_set(&adev->vm_manager.vm_pte_next_ring, 0);
	mtx_init(&adev->vm_manager.prt_lock, IPL_TTY);
	atomic_set(&adev->vm_manager.num_prt_users, 0);

	/* If not overridden by the user, by default, only in large BAR systems
	 * Compute VM tables will be updated by CPU
	 */
#ifdef __amd64__
	if (amdgpu_vm_update_mode == -1) {
		if (amdgpu_gmc_vram_full_visible(&adev->gmc))
			adev->vm_manager.vm_update_mode =
				AMDGPU_VM_USE_CPU_FOR_COMPUTE;
		else
			adev->vm_manager.vm_update_mode = 0;
	} else
		adev->vm_manager.vm_update_mode = amdgpu_vm_update_mode;
#else
	adev->vm_manager.vm_update_mode = 0;
#endif

	idr_init(&adev->vm_manager.pasid_idr);
	mtx_init(&adev->vm_manager.pasid_lock, IPL_TTY);
}

/**
 * amdgpu_vm_manager_fini - cleanup VM manager
 *
 * @adev: amdgpu_device pointer
 *
 * Cleanup the VM manager and free resources.
 */
void amdgpu_vm_manager_fini(struct amdgpu_device *adev)
{
	WARN_ON(!idr_is_empty(&adev->vm_manager.pasid_idr));
	idr_destroy(&adev->vm_manager.pasid_idr);

	amdgpu_vmid_mgr_fini(adev);
}

/**
 * amdgpu_vm_ioctl - Manages VMID reservation for vm hubs.
 *
 * @dev: drm device pointer
 * @data: drm_amdgpu_vm
 * @filp: drm file pointer
 *
 * Returns:
 * 0 for success, -errno for errors.
 */
int amdgpu_vm_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
	union drm_amdgpu_vm *args = data;
	struct amdgpu_device *adev = dev->dev_private;
	struct amdgpu_fpriv *fpriv = filp->driver_priv;
	int r;

	switch (args->in.op) {
	case AMDGPU_VM_OP_RESERVE_VMID:
		/* current, we only have requirement to reserve vmid from gfxhub */
		r = amdgpu_vmid_alloc_reserved(adev, &fpriv->vm, AMDGPU_GFXHUB);
		if (r)
			return r;
		break;
	case AMDGPU_VM_OP_UNRESERVE_VMID:
		amdgpu_vmid_free_reserved(adev, &fpriv->vm, AMDGPU_GFXHUB);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/**
 * amdgpu_vm_get_task_info - Extracts task info for a PASID.
 *
 * @dev: drm device pointer
 * @pasid: PASID identifier for VM
 * @task_info: task_info to fill.
 */
void amdgpu_vm_get_task_info(struct amdgpu_device *adev, unsigned int pasid,
			 struct amdgpu_task_info *task_info)
{
	struct amdgpu_vm *vm;
	unsigned long flags;

	spin_lock_irqsave(&adev->vm_manager.pasid_lock, flags);

	vm = idr_find(&adev->vm_manager.pasid_idr, pasid);
	if (vm)
		*task_info = vm->task_info;

	spin_unlock_irqrestore(&adev->vm_manager.pasid_lock, flags);
}

/**
 * amdgpu_vm_set_task_info - Sets VMs task info.
 *
 * @vm: vm for which to set the info
 */
void amdgpu_vm_set_task_info(struct amdgpu_vm *vm)
{
	if (!vm->task_info.pid) {
#ifdef __linux__
		vm->task_info.pid = current->pid;
		get_task_comm(vm->task_info.task_name, current);

		if (current->group_leader->mm == current->mm) {
			vm->task_info.tgid = current->group_leader->pid;
			get_task_comm(vm->task_info.process_name, current->group_leader);
		}
#else
		vm->task_info.pid = curproc->p_p->ps_pid;
		strlcpy(vm->task_info.task_name, curproc->p_p->ps_comm,
		    sizeof(vm->task_info.task_name));
#endif
	}
}
