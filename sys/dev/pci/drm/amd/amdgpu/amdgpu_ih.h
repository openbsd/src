/*
 * Copyright 2014 Advanced Micro Devices, Inc.
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

#ifndef __AMDGPU_IH_H__
#define __AMDGPU_IH_H__

#include <linux/chash.h>
#include "soc15_ih_clientid.h"

struct amdgpu_device;

#define AMDGPU_IH_CLIENTID_LEGACY 0
#define AMDGPU_IH_CLIENTID_MAX SOC15_IH_CLIENTID_MAX

#define AMDGPU_PAGEFAULT_HASH_BITS 8
struct amdgpu_retryfault_hashtable {
	DECLARE_CHASH_TABLE(hash, AMDGPU_PAGEFAULT_HASH_BITS, 8, 0);
	spinlock_t	lock;
	int		count;
};

/*
 * R6xx+ IH ring
 */
struct amdgpu_ih_ring {
	struct amdgpu_bo	*ring_obj;
	volatile uint32_t	*ring;
	struct drm_dmamem	*dmah;
	unsigned		rptr;
	unsigned		ring_size;
	uint64_t		gpu_addr;
	uint32_t		ptr_mask;
	atomic_t		lock;
	bool                    enabled;
	unsigned		wptr_offs;
	unsigned		rptr_offs;
	u32			doorbell_index;
	bool			use_doorbell;
	bool			use_bus_addr;
	dma_addr_t		rb_dma_addr; /* only used when use_bus_addr = true */
	struct amdgpu_retryfault_hashtable *faults;
};

#define AMDGPU_IH_SRC_DATA_MAX_SIZE_DW 4

struct amdgpu_iv_entry {
	unsigned client_id;
	unsigned src_id;
	unsigned ring_id;
	unsigned vmid;
	unsigned vmid_src;
	uint64_t timestamp;
	unsigned timestamp_src;
	unsigned pasid;
	unsigned pasid_src;
	unsigned src_data[AMDGPU_IH_SRC_DATA_MAX_SIZE_DW];
	const uint32_t *iv_entry;
};

int amdgpu_ih_ring_init(struct amdgpu_device *adev, unsigned ring_size,
			bool use_bus_addr);
void amdgpu_ih_ring_fini(struct amdgpu_device *adev);
int amdgpu_ih_process(struct amdgpu_device *adev);
int amdgpu_ih_add_fault(struct amdgpu_device *adev, u64 key);
void amdgpu_ih_clear_fault(struct amdgpu_device *adev, u64 key);

#endif
