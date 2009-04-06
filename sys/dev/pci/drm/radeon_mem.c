/*
 * Copyright (C) The Weather Channel, Inc.  2002.  All Rights Reserved.
 *
 * The Weather Channel (TM) funded Tungsten Graphics to develop the
 * initial release of the Radeon 8500 driver under the XFree86 license.
 * This notice must be preserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Keith Whitwell <keith@tungstengraphics.com>
 */

#include "drmP.h"
#include "drm.h"
#include "radeon_drm.h"
#include "radeon_drv.h"

struct drm_heap *radeon_get_heap(drm_radeon_private_t *, int);


/* IOCTL HANDLERS */

struct drm_heap *
radeon_get_heap(drm_radeon_private_t * dev_priv, int region)
{
	switch (region) {
	case RADEON_MEM_REGION_GART:
		return (&dev_priv->gart_heap);
	case RADEON_MEM_REGION_FB:
		return (&dev_priv->fb_heap);
	default:
		return (NULL);
	}
}

int
radeon_mem_alloc(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	drm_radeon_private_t	*dev_priv = dev->dev_private;
	drm_radeon_mem_alloc_t	*alloc = data;
	struct drm_heap		*heap;
	struct drm_mem		*block; 

	if (dev_priv == NULL) {
		DRM_ERROR("called with no initialization\n");
		return (EINVAL);
	}

	heap = radeon_get_heap(dev_priv, alloc->region);
	if (heap == NULL)
		return (EFAULT);

	/*
	 * Make things easier on ourselves: all allocations at least
	 * 4k aligned.
	 */
	if (alloc->alignment < 12)
		alloc->alignment = 12;

	block = drm_alloc_block(heap, alloc->size, alloc->alignment, file_priv);

	if (block == NULL)
		return (ENOMEM);

	if (DRM_COPY_TO_USER(alloc->region_offset, &block->start, sizeof(int)))
		return (EFAULT);

	return 0;
}

int
radeon_mem_free(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	drm_radeon_private_t	*dev_priv = dev->dev_private;
	drm_radeon_mem_free_t	*memfree = data;
	struct drm_heap		*heap;
	struct drm_mem		*block;

	if (dev_priv == NULL) {
		DRM_ERROR("called with no initialization\n");
		return (EINVAL);
	}

	heap = radeon_get_heap(dev_priv, memfree->region);
	if (heap == NULL)
		return (EFAULT);

	block = drm_find_block(heap, memfree->region_offset);
	if (block == NULL)
		return (EFAULT);

	if (block->file_priv != file_priv)
		return (EPERM);

	drm_free_block(heap, block);
	return (0);
}

int
radeon_mem_init_heap(struct drm_device *dev, void *data,
    struct drm_file *file_priv)
{
	drm_radeon_private_t		*dev_priv = dev->dev_private;
	drm_radeon_mem_init_heap_t	*initheap = data;
	struct drm_heap			*heap;

	if (dev_priv == NULL) {
		DRM_ERROR("called with no initialization\n");
		return (EINVAL);
	}

	/* Make sure it's valid and initialised */
	heap = radeon_get_heap(dev_priv, initheap->region);
	if (heap == NULL || !TAILQ_EMPTY(heap))
                return (EFAULT);

	return (drm_init_heap(heap, initheap->start, initheap->size));
}
