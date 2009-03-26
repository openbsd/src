/* i915_mem.c -- Simple agp/fb memory manager for i915 -*- linux-c -*-
 */
/*
 * Copyright 2003 Tungsten Graphics, Inc., Cedar Park, Texas.
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL TUNGSTEN GRAPHICS AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "drmP.h"
#include "drm.h"
#include "i915_drm.h"
#include "i915_drv.h"

struct drm_heap *intel_get_heap(drm_i915_private_t *, int);
void	intel_mark_block(struct drm_device *, struct drm_mem *, int);

/* This memory manager is integrated into the global/local lru
 * mechanisms used by the clients.  Specifically, it operates by
 * setting the 'in_use' fields of the global LRU to indicate whether
 * this region is privately allocated to a client.
 *
 * This does require the client to actually respect that field.
 *
 * Currently no effort is made to allocate 'private' memory in any
 * clever way - the LRU information isn't used to determine which
 * block to allocate, and the ring is drained prior to allocations --
 * in other words allocation is expensive.
 */
void
intel_mark_block(struct drm_device * dev, struct drm_mem *p, int in_use)
{
	drm_i915_private_t	*dev_priv = dev->dev_private;
	drm_i915_sarea_t 	*sarea_priv = dev_priv->sarea_priv;
	struct drm_tex_region 	*list;
	unsigned		 shift, nr, start, end, i;
	int			 age;

	shift = dev_priv->tex_lru_log_granularity;
	nr = I915_NR_TEX_REGIONS;

	start = p->start >> shift;
	end = (p->start + p->size - 1) >> shift;

	age = ++sarea_priv->texAge;
	list = sarea_priv->texList;

	/* Mark the regions with the new flag and update their age.  Move
	 * them to head of list to preserve LRU semantics.
	 */
	for (i = start; i <= end; i++) {
		list[i].in_use = in_use;
		list[i].age = age;

		/* remove_from_list(i)
		 */
		list[(unsigned)list[i].next].prev = list[i].prev;
		list[(unsigned)list[i].prev].next = list[i].next;

		/* insert_at_head(list, i)
		 */
		list[i].prev = nr;
		list[i].next = list[nr].next;
		list[(unsigned)list[nr].next].prev = i;
		list[nr].next = i;
	}
}

/* Free all blocks associated with the releasing file.
 */
void
i915_mem_release(struct drm_device * dev, struct drm_file *file_priv,
    struct drm_heap *heap)
{
	struct drm_mem	*p, *q;

	if (heap == NULL || TAILQ_EMPTY(heap))
		return;

	TAILQ_FOREACH(p, heap, link) {
		if (p->file_priv == file_priv) {
			intel_mark_block(dev, p, 0);
			p->file_priv = NULL;
		}
	}

	/* Coalesce the entries.  ugh... */
	for (p = TAILQ_FIRST(heap); p != TAILQ_END(heap); p = q) {
		while (p->file_priv == NULL &&
		    (q = TAILQ_NEXT(p, link)) != TAILQ_END(heap) &&
		    q->file_priv == NULL) {
			p->size += q->size;
			TAILQ_REMOVE(heap, q, link);
			drm_free(q);
		}
		q = TAILQ_NEXT(p, link);
	}
}

/* Shutdown.
 */
void
i915_mem_takedown(struct drm_heap *heap)
{
	struct drm_mem	*p;

	if (heap == NULL)
		return;

	while ((p = TAILQ_FIRST(heap)) != NULL) {
		TAILQ_REMOVE(heap, p, link);
		drm_free(p);
	}
}

struct drm_heap *
intel_get_heap(drm_i915_private_t * dev_priv, int region)
{
	switch (region) {
	case I915_MEM_REGION_AGP:
		return (&dev_priv->agp_heap);
	default:
		return (NULL);
	}
}

/* IOCTL HANDLERS */

int i915_mem_alloc(struct drm_device *dev, void *data,
		   struct drm_file *file_priv)
{
	drm_i915_private_t	*dev_priv = dev->dev_private;
	drm_i915_mem_alloc_t	*alloc = data;
	struct drm_heap		*heap;
	struct drm_mem		*block;

	if (dev_priv == NULL) {
		DRM_ERROR("called with no initialization\n");
		return (EINVAL);
	}

	heap = intel_get_heap(dev_priv, alloc->region);
	if (heap == NULL)
		return (EFAULT);

	/* Make things easier on ourselves: all allocations at least
	 * 4k aligned.
	 */
	if (alloc->alignment < 12)
		alloc->alignment = 12;

	block = drm_alloc_block(heap, alloc->size, alloc->alignment, file_priv);

	if (block == NULL)
		return (ENOMEM);

	intel_mark_block(dev, block, 1);

	if (DRM_COPY_TO_USER(alloc->region_offset, &block->start,
			     sizeof(int))) {
		DRM_ERROR("copy_to_user\n");
		return (EFAULT);
	}

	return (0);
}

int
i915_mem_free(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	drm_i915_private_t	*dev_priv = dev->dev_private;
	drm_i915_mem_free_t	*memfree = data;
	struct drm_heap		*heap;
	struct drm_mem		*block;

	if (dev_priv == NULL) {
		DRM_ERROR("called with no initialization\n");
		return (EINVAL);
	}

	heap = intel_get_heap(dev_priv, memfree->region);
	if (heap == NULL)
		return (EFAULT);

	block = drm_find_block(heap, memfree->region_offset);
	if (block == NULL)
		return (EFAULT);

	if (block->file_priv != file_priv)
		return (EPERM);

	intel_mark_block(dev, block, 0);
	drm_free_block(heap, block);

	return (0);
}

int
i915_mem_init_heap(struct drm_device *dev, void *data,
    struct drm_file *file_priv)
{
	drm_i915_private_t		*dev_priv = dev->dev_private;
	drm_i915_mem_init_heap_t	*initheap = data;
	struct drm_heap			*heap;

	if (dev_priv == NULL) {
		DRM_ERROR("called with no initialization\n");
		return (EINVAL);
	}

	/* Make sure it's valid and initialised */
	heap = intel_get_heap(dev_priv, initheap->region);
	if (heap == NULL || !TAILQ_EMPTY(heap))
		return (EFAULT);

	return (drm_init_heap(heap, initheap->start, initheap->size));
}

int
i915_mem_destroy_heap( struct drm_device *dev, void *data,
    struct drm_file *file_priv )
{
	drm_i915_private_t		*dev_priv = dev->dev_private;
	drm_i915_mem_destroy_heap_t	*destroyheap = data;
	struct drm_heap			*heap;

	if (dev_priv == NULL) {
		DRM_ERROR( "called with no initialization\n" );
		return (EINVAL);
	}

	heap = intel_get_heap( dev_priv, destroyheap->region );
	if (heap == NULL) {
		DRM_ERROR("intel_get_heap failed");
		return (EFAULT);
	}

	if (TAILQ_EMPTY(heap)) {
		DRM_ERROR("heap not initialized?");
		return (EFAULT);
	}

	i915_mem_takedown(heap);
	return (0);
}
