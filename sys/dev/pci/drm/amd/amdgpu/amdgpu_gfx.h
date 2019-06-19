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

#ifndef __AMDGPU_GFX_H__
#define __AMDGPU_GFX_H__

int amdgpu_gfx_scratch_get(struct amdgpu_device *adev, uint32_t *reg);
void amdgpu_gfx_scratch_free(struct amdgpu_device *adev, uint32_t reg);

void amdgpu_gfx_parse_disable_cu(unsigned *mask, unsigned max_se,
		unsigned max_sh);

void amdgpu_gfx_compute_queue_acquire(struct amdgpu_device *adev);

int amdgpu_gfx_kiq_init_ring(struct amdgpu_device *adev,
			     struct amdgpu_ring *ring,
			     struct amdgpu_irq_src *irq);

void amdgpu_gfx_kiq_free_ring(struct amdgpu_ring *ring,
			      struct amdgpu_irq_src *irq);

void amdgpu_gfx_kiq_fini(struct amdgpu_device *adev);
int amdgpu_gfx_kiq_init(struct amdgpu_device *adev,
			unsigned hpd_size);

int amdgpu_gfx_compute_mqd_sw_init(struct amdgpu_device *adev,
				   unsigned mqd_size);
void amdgpu_gfx_compute_mqd_sw_fini(struct amdgpu_device *adev);

/**
 * amdgpu_gfx_create_bitmask - create a bitmask
 *
 * @bit_width: length of the mask
 *
 * create a variable length bit mask.
 * Returns the bitmask.
 */
static inline u32 amdgpu_gfx_create_bitmask(u32 bit_width)
{
	return (u32)((1ULL << bit_width) - 1);
}

static inline int amdgpu_gfx_queue_to_bit(struct amdgpu_device *adev,
					  int mec, int pipe, int queue)
{
	int bit = 0;

	bit += mec * adev->gfx.mec.num_pipe_per_mec
		* adev->gfx.mec.num_queue_per_pipe;
	bit += pipe * adev->gfx.mec.num_queue_per_pipe;
	bit += queue;

	return bit;
}

static inline void amdgpu_gfx_bit_to_queue(struct amdgpu_device *adev, int bit,
					   int *mec, int *pipe, int *queue)
{
	*queue = bit % adev->gfx.mec.num_queue_per_pipe;
	*pipe = (bit / adev->gfx.mec.num_queue_per_pipe)
		% adev->gfx.mec.num_pipe_per_mec;
	*mec = (bit / adev->gfx.mec.num_queue_per_pipe)
	       / adev->gfx.mec.num_pipe_per_mec;

}
static inline bool amdgpu_gfx_is_mec_queue_enabled(struct amdgpu_device *adev,
						   int mec, int pipe, int queue)
{
	return test_bit(amdgpu_gfx_queue_to_bit(adev, mec, pipe, queue),
			adev->gfx.mec.queue_bitmap);
}

#endif
