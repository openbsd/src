/*-
 * Copyright 1999, 2000 Precision Insight, Inc., Cedar Park, Texas.
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
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Rickard E. (Rik) Faith <faith@valinux.com>
 *    Gareth Hughes <gareth@valinux.com>
 *
 */

/** @file drm_dma.c
 * Support code for DMA buffer management.
 *
 * The implementation used to be significantly more complicated, but the
 * complexity has been moved into the drivers as different buffer management
 * schemes evolved.
 */

#include "drmP.h"

int
drm_dma_setup(struct drm_device *dev)
{

	dev->dma = drm_calloc(1, sizeof(*dev->dma));
	if (dev->dma == NULL)
		return ENOMEM;

	dev->buf_use = 0;

	DRM_SPININIT(&dev->dma_lock, "drmdma");

	return 0;
}

void
drm_cleanup_buf(struct drm_device *dev, drm_buf_entry_t *entry)
{
	int i;

	if (entry->seg_count) {
		for (i = 0; i < entry->seg_count; i++)
			drm_dmamem_free(dev->dmat, entry->seglist[i]);
		drm_free(entry->seglist);

		entry->seg_count = 0;
	}

   	if (entry->buf_count) {
	   	for (i = 0; i < entry->buf_count; i++) {
			drm_free(entry->buflist[i].dev_private);
		}
		drm_free(entry->buflist);

		entry->buf_count = 0;
	}
}

void
drm_dma_takedown(struct drm_device *dev)
{
	drm_device_dma_t *dma = dev->dma;
	int i;

	if (dma == NULL)
		return;

	/* Clear dma buffers */
	for (i = 0; i <= DRM_MAX_ORDER; i++)
		drm_cleanup_buf(dev, &dma->bufs[i]);

	drm_free(dma->buflist);
	drm_free(dma->pagelist);
	drm_free(dev->dma);
	dev->dma = NULL;
	DRM_SPINUNINIT(&dev->dma_lock);
}


void
drm_free_buffer(struct drm_device *dev, drm_buf_t *buf)
{
	if (buf == NULL)
		return;

	buf->pending = 0;
	buf->file_priv= NULL;
	buf->used = 0;
}

void
drm_reclaim_buffers(struct drm_device *dev, struct drm_file *file_priv)
{
	drm_device_dma_t *dma = dev->dma;
	int i;

	if (dma == NULL)
		return;
	for (i = 0; i < dma->buf_count; i++) {
		if (dma->buflist[i]->file_priv == file_priv)
				drm_free_buffer(dev, dma->buflist[i]);
	}
}

/* Call into the driver-specific DMA handler */
int
drm_dma(struct drm_device *dev, void *data, struct drm_file *file_priv)
{

	if (dev->driver->dma_ioctl != NULL) {
		return (dev->driver->dma_ioctl(dev, data, file_priv));
	} else {
		DRM_DEBUG("DMA ioctl on driver with no dma handler\n");
		return EINVAL;
	}
}
