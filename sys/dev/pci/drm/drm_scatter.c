/*-
 * Copyright 2000 VA Linux Systems, Inc., Sunnyvale, California.
 * All Rights Reserved.
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
 *   Gareth Hughes <gareth@valinux.com>
 *   Eric Anholt <anholt@FreeBSD.org>
 *
 */

/*
 * Allocation of memory for scatter-gather mappings by the graphics chip.
 *
 * The memory allocated here is then made into an aperture in the card
 * by drm_ati_pcigart_init().
 */
#include "drmP.h"

void
drm_sg_cleanup(struct drm_device *dev, struct drm_sg_mem *entry)
{
	if (entry == NULL)
		return;

	drm_dmamem_free(dev->dmat, entry->mem);
	drm_free(entry);
}

int
drm_sg_alloc(struct drm_device * dev, struct drm_scatter_gather *request)
{
	struct drm_sg_mem	*entry;
	bus_size_t		 size;
	unsigned long		 pages;

	if (dev->sg != NULL)
		return (EINVAL);

	entry = drm_calloc(1, sizeof(*entry));
        if (entry == NULL)
                return (ENOMEM);

	pages = round_page(request->size) / PAGE_SIZE;
	size = pages << PAGE_SHIFT;

	DRM_DEBUG("sg size=%ld pages=%ld\n", request->size, pages);

	if ((entry->mem = drm_dmamem_alloc(dev->dmat, size, PAGE_SIZE, pages,
	    PAGE_SIZE, 0, 0)) == NULL)
		return (ENOMEM);

	request->handle = entry->handle = (unsigned long)entry->mem->kva;
	DRM_DEBUG("sg alloc handle  = %08lx\n", entry->handle);

	DRM_LOCK();
	if (dev->sg) {
		DRM_UNLOCK();
		drm_sg_cleanup(dev, entry);
		return EINVAL;
	}
	dev->sg = entry;
	DRM_UNLOCK();

	return (0);
}

int
drm_sg_alloc_ioctl(struct drm_device *dev, void *data,
    struct drm_file *file_priv)
{
	struct drm_scatter_gather	*request = data;
	int				 ret;

	DRM_DEBUG("\n");

	ret = drm_sg_alloc(dev, request);
	return ret;
}

int
drm_sg_free(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_scatter_gather	*request = data;
	struct drm_sg_mem		*entry;

	DRM_LOCK();
	entry = dev->sg;
	dev->sg = NULL;
	DRM_UNLOCK();

	if (entry == NULL || entry->handle != request->handle)
		return EINVAL;

	DRM_DEBUG("sg free virtual  = 0x%lx\n", entry->handle);

	drm_sg_cleanup(dev, entry);

	return 0;
}
