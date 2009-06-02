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

/** @file drm_bufs.c
 * Implementation of the ioctls for setup of DRM mappings and DMA buffers.
 */

#include "sys/types.h"
#include "dev/pci/pcireg.h"

#include "drmP.h"

int	drm_addbufs_pci(struct drm_device *, struct drm_buf_desc *);
int	drm_addbufs_sg(struct drm_device *, struct drm_buf_desc *);
int	drm_addbufs_agp(struct drm_device *, struct drm_buf_desc *);

/*
 * Compute order.  Can be made faster.
 */
int
drm_order(unsigned long size)
{
	int order;
	unsigned long tmp;

	for (order = 0, tmp = size; tmp >>= 1; ++order)
		;

	if (size & ~(1 << order))
		++order;

	return order;
}

struct drm_local_map *
drm_core_findmap(struct drm_device *dev, unsigned long offset)
{
	struct drm_local_map	*map;

	DRM_LOCK();
	TAILQ_FOREACH(map, &dev->maplist, link) {
		if (offset == map->ext)
			break;
	}
	DRM_UNLOCK();
	return (map);
}

int
drm_addmap(struct drm_device * dev, unsigned long offset, unsigned long size,
    enum drm_map_type type, enum drm_map_flags flags,
    struct drm_local_map **map_ptr)
{
	struct drm_local_map	*map;
	int			 align, ret = 0;
#if 0 /* disabled for now */
	struct drm_agp_mem	*entry;
	int			 valid;
#endif 

	/* Only allow shared memory to be removable since we only keep enough
	 * book keeping information about shared memory to allow for removal
	 * when processes fork.
	 */
	if ((flags & _DRM_REMOVABLE) && type != _DRM_SHM) {
		DRM_ERROR("Requested removable map for non-DRM_SHM\n");
		return EINVAL;
	}
	if ((offset & PAGE_MASK) || (size & PAGE_MASK)) {
		DRM_ERROR("offset/size not page aligned: 0x%lx/0x%lx\n",
		    offset, size);
		return EINVAL;
	}
	if (offset + size < offset) {
		DRM_ERROR("offset and size wrap around: 0x%lx/0x%lx\n",
		    offset, size);
		return EINVAL;
	}

	DRM_DEBUG("offset = 0x%08lx, size = 0x%08lx, type = %d\n", offset,
	    size, type);

	/*
	 * Check if this is just another version of a kernel-allocated map, and
	 * just hand that back if so.
	 */
	DRM_LOCK();
	if (type == _DRM_REGISTERS || type == _DRM_FRAME_BUFFER ||
	    type == _DRM_SHM) {
		TAILQ_FOREACH(map, &dev->maplist, link) {
			if (map->type == type && (map->offset == offset ||
			    (map->type == _DRM_SHM &&
			    map->flags == _DRM_CONTAINS_LOCK))) {
				DRM_DEBUG("Found kernel map %d\n", type);
				goto done;
			}
		}
	}
	DRM_UNLOCK();

	/* Allocate a new map structure, fill it in, and do any type-specific
	 * initialization necessary.
	 */
	map = drm_calloc(1, sizeof(*map));
	if (map == NULL) {
		DRM_LOCK();
		return ENOMEM;
	}

	map->offset = offset;
	map->size = size;
	map->type = type;
	map->flags = flags;


	DRM_LOCK();
	ret = extent_alloc(dev->handle_ext, map->size, PAGE_SIZE, 0,
	    0, EX_NOWAIT, &map->ext);
	if (ret) {
		DRM_ERROR("can't find free offset\n");
		DRM_UNLOCK();
		drm_free(map);
		return (ret);
	}
	DRM_UNLOCK();

	switch (map->type) {
	case _DRM_REGISTERS:
		if (!(map->flags & _DRM_WRITE_COMBINING))
			break;
		/* FALLTHROUGH */
	case _DRM_FRAME_BUFFER:
		if (drm_mtrr_add(map->offset, map->size, DRM_MTRR_WC) == 0)
			map->mtrr = 1;
		break;
	case _DRM_AGP:
		/*valid = 0;*/
		/* In some cases (i810 driver), user space may have already
		 * added the AGP base itself, because dev->agp->base previously
		 * only got set during AGP enable.  So, only add the base
		 * address if the map's offset isn't already within the
		 * aperture.
		 */
		if (map->offset < dev->agp->base ||
		    map->offset > dev->agp->base +
		    dev->agp->info.ai_aperture_size - 1) {
			map->offset += dev->agp->base;
		}
		map->mtrr   = dev->agp->mtrr; /* for getmap */
#if 0 /* disabled for now */
		/*
		 * If agp is in control of userspace (some intel drivers for
		 * example. In which case ignore this loop.
		 */
		DRM_LOCK();
		TAILQ_FOREACH(entry, &dev->agp->memory, link) {
			DRM_DEBUG("bound = %p, pages = %p, %p\n",
			    entry->bound, entry->pages,
			    entry->pages * PAGE_SIZE);
			if ((map->offset >= entry->bound) &&
			    (map->offset + map->size <=
			    entry->bound + entry->pages * PAGE_SIZE)) {
				valid = 1;
				break;
			}
		}
		if (!TAILQ_EMPTY(&dev->agp->memory) && !valid) {
			DRM_UNLOCK();
			drm_free(map);
			DRM_ERROR("invalid agp map requested\n");
			return (EACCES);
		}
		DRM_UNLOCK();
#endif
		break;
	case _DRM_SCATTER_GATHER:
		if (dev->sg == NULL) {
			drm_free(map);
			return (EINVAL);
		}
		map->offset += dev->sg->handle;
		break;
	case _DRM_SHM:
	case _DRM_CONSISTENT:
		/*
		 * Unfortunately, we don't get any alignment specification from
		 * the caller, so we have to guess. So try to align the bus
		 * address of the map to its size if possible, otherwise just
		 * assume PAGE_SIZE alignment.
		 */
		align = map->size;
		if ((align & (align - 1)) != 0)
			align = PAGE_SIZE;
		map->dmamem = drm_dmamem_alloc(dev->dmat, map->size, align,
		    1, map->size, 0, 0);
		if (map->dmamem == NULL) {
			drm_free(map);
			return (ENOMEM);
		}
		map->handle = map->dmamem->kva;
		map->offset = map->dmamem->map->dm_segs[0].ds_addr;
		if (map->type == _DRM_SHM && map->flags & _DRM_CONTAINS_LOCK) {
			DRM_LOCK();
			/* Prevent a 2nd X Server from creating a 2nd lock */
			if (dev->lock.hw_lock != NULL) {
				DRM_UNLOCK();
				drm_dmamem_free(dev->dmat, map->dmamem);
				drm_free(map);
				return (EBUSY);
			}
			dev->lock.hw_lock = map->handle;
			DRM_UNLOCK();
		}
		break;
	default:
		DRM_ERROR("Bad map type %d\n", map->type);
		drm_free(map);
		return EINVAL;
	}

	DRM_LOCK();
	TAILQ_INSERT_TAIL(&dev->maplist, map, link);
done:
	DRM_UNLOCK();

	DRM_DEBUG("Added map %d 0x%lx/0x%lx\n", map->type, map->offset,
	    map->size);

	*map_ptr = map;

	return 0;
}

int
drm_addmap_ioctl(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_map		*request = data;
	struct drm_local_map	*map;
	int			 err;

	if (!(file_priv->flags & (FREAD|FWRITE)))
		return EACCES; /* Require read/write */

	err = drm_addmap(dev, request->offset, request->size, request->type,
	    request->flags, &map);
	if (err != 0)
		return err;

	request->offset = map->offset;
	request->size = map->size;
	request->type = map->type;
	request->flags = map->flags;
	request->mtrr = map->mtrr;
	request->handle = map->handle;

	request->handle = (void *)map->ext;

	return 0;
}

void
drm_rmmap(struct drm_device *dev, struct drm_local_map *map)
{
	DRM_LOCK();
	drm_rmmap_locked(dev, map);
	DRM_UNLOCK();
}


void
drm_rmmap_locked(struct drm_device *dev, struct drm_local_map *map)
{
	TAILQ_REMOVE(&dev->maplist, map, link);

	switch (map->type) {
	case _DRM_REGISTERS:
		/* FALLTHROUGH */
	case _DRM_FRAME_BUFFER:
		if (map->mtrr) {
			int retcode;
			
			retcode = drm_mtrr_del(0, map->offset, map->size,
			    DRM_MTRR_WC);
			DRM_DEBUG("mtrr_del = %d\n", retcode);
		}
		break;
	case _DRM_AGP:
		/* FALLTHROUGH */
	case _DRM_SCATTER_GATHER:
		break;
	case _DRM_SHM:
		/* FALLTHROUGH */
	case _DRM_CONSISTENT:
		drm_dmamem_free(dev->dmat, map->dmamem);
		break;
	default:
		DRM_ERROR("Bad map type %d\n", map->type);
		break;
	}

	/* NOCOALESCE set, can't fail */
	extent_free(dev->handle_ext, map->ext, map->size, EX_NOWAIT);

	drm_free(map);
}

/* Remove a map private from list and deallocate resources if the mapping
 * isn't in use.
 */

int
drm_rmmap_ioctl(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_local_map	*map;
	struct drm_map		*request = data;

	DRM_LOCK();
	TAILQ_FOREACH(map, &dev->maplist, link) {
		if (map->handle == request->handle &&
		    map->flags & _DRM_REMOVABLE)
			break;
	}

	/* No match found. */
	if (map == NULL) {
		DRM_UNLOCK();
		return (EINVAL);
	}

	drm_rmmap_locked(dev, map);

	DRM_UNLOCK();

	return 0;
}

/*
 * DMA buffers api.
 *
 * The implementation used to be significantly more complicated, but the
 * complexity has been moved into the drivers as different buffer management
 * schemes evolved.
 *
 * This api is going to die eventually.
 */

int
drm_dma_setup(struct drm_device *dev)
{

	dev->dma = drm_calloc(1, sizeof(*dev->dma));
	if (dev->dma == NULL)
		return (ENOMEM);

	rw_init(&dev->dma->dma_lock, "drmdma");

	return (0);
}

void
drm_cleanup_buf(struct drm_device *dev, struct drm_buf_entry *entry)
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
	struct drm_device_dma	*dma = dev->dma;
	int			 i;

	if (dma == NULL)
		return;

	/* Clear dma buffers */
	for (i = 0; i <= DRM_MAX_ORDER; i++)
		drm_cleanup_buf(dev, &dma->bufs[i]);

	drm_free(dma->buflist);
	drm_free(dma->pagelist);
	drm_free(dev->dma);
	dev->dma = NULL;
}


void
drm_free_buffer(struct drm_device *dev, struct drm_buf *buf)
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
	struct drm_device_dma	*dma = dev->dma;
	int			 i;

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
	struct drm_device_dma	*dma = dev->dma;
	struct drm_dma		*d = data;
	int			 ret = 0;

	if (dev->driver->dma_ioctl == NULL) {
		DRM_DEBUG("DMA ioctl on driver with no dma handler\n");
		return (EINVAL);
	}

	LOCK_TEST_WITH_RETURN(dev, file_priv);

	/* Please don't send us buffers.
	 */
	if (d->send_count != 0) {
		DRM_ERROR("process trying to send %d buffers via drmDMA\n",
		    d->send_count);
		return (EINVAL);
	}

	/* We'll send you buffers.
	 */
	if (d->request_count < 0 || d->request_count > dma->buf_count) {
		DRM_ERROR("Process trying to get %d buffers (of %d max)\n",
			  d->request_count, dma->buf_count);
		return (EINVAL);
	}
	d->granted_count = 0;

	if (d->request_count)
		ret = dev->driver->dma_ioctl(dev, d, file_priv);
	return (ret);
}

int
drm_addbufs_agp(struct drm_device *dev, struct drm_buf_desc *request)
{
	struct drm_device_dma	*dma = dev->dma;
	struct drm_buf_entry	*entry;
	struct drm_buf		*buf, **temp_buflist;
	unsigned long		 agp_offset, offset;
	int			 alignment, count, order, page_order, size;
	int			 total, byte_count, i;
#if 0 /* disabled for now */
	struct drm_agp_mem	*agp_entry;
	int			 valid;
#endif

	count = request->count;
	order = drm_order(request->size);
	size = 1 << order;

	alignment  = (request->flags & _DRM_PAGE_ALIGN)
	    ? round_page(size) : size;
	page_order = order - PAGE_SHIFT > 0 ? order - PAGE_SHIFT : 0;
	total = PAGE_SIZE << page_order;

	byte_count = 0;
	agp_offset = dev->agp->base + request->agp_start;

	DRM_DEBUG("count:      %d\n",  count);
	DRM_DEBUG("order:      %d\n",  order);
	DRM_DEBUG("size:       %d\n",  size);
	DRM_DEBUG("agp_offset: 0x%lx\n", agp_offset);
	DRM_DEBUG("alignment:  %d\n",  alignment);
	DRM_DEBUG("page_order: %d\n",  page_order);
	DRM_DEBUG("total:      %d\n",  total);

	/* Make sure buffers are located in AGP memory that we own */

	/* Breaks MGA due to drm_alloc_agp not setting up entries for the
	 * memory.  Safe to ignore for now because these ioctls are still
	 * root-only.
	 */
#if 0 /* disabled for now */
	valid = 0;
	DRM_LOCK();
	TAILQ_FOREACH(agp_entry, &dev->agp->memory, link) {
		if ((agp_offset >= agp_entry->bound) &&
		    (agp_offset + total * count <=
		    agp_entry->bound + agp_entry->pages * PAGE_SIZE)) {
			valid = 1;
			break;
		}
	}
	if (!TAILQ_EMPTY(&dev->agp->memory) && !valid) {
		DRM_DEBUG("zone invalid\n");
		DRM_UNLOCK();
		return (EINVAL);
	}
	DRM_UNLOCK();
#endif

	entry = &dma->bufs[order];

	entry->buflist = drm_calloc(count, sizeof(*entry->buflist));
	if (entry->buflist == NULL)
		return ENOMEM;

	entry->buf_size = size;
	entry->page_order = page_order;

	offset = 0;

	while (entry->buf_count < count) {
		buf = &entry->buflist[entry->buf_count];
		buf->idx = dma->buf_count + entry->buf_count;
		buf->total = alignment;
		buf->used = 0;

		buf->offset = (dma->byte_count + offset);
		buf->bus_address = agp_offset + offset;
		buf->pending = 0;
		buf->file_priv = NULL;

		buf->dev_private = drm_calloc(1, dev->driver->buf_priv_size);
		if (buf->dev_private == NULL) {
			/* Set count correctly so we free the proper amount. */
			entry->buf_count = count;
			drm_cleanup_buf(dev, entry);
			return ENOMEM;
		}

		offset += alignment;
		entry->buf_count++;
		byte_count += PAGE_SIZE << page_order;
	}

	DRM_DEBUG("byte_count: %d\n", byte_count);

	/* OpenBSD lacks realloc in kernel */
	temp_buflist = drm_realloc(dma->buflist,
	    dma->buf_count * sizeof(*dma->buflist),
	    (dma->buf_count + entry->buf_count) * sizeof(*dma->buflist));
	if (temp_buflist == NULL) {
		/* Free the entry because it isn't valid */
		drm_cleanup_buf(dev, entry);
		return ENOMEM;
	}
	dma->buflist = temp_buflist;

	for (i = 0; i < entry->buf_count; i++)
		dma->buflist[i + dma->buf_count] = &entry->buflist[i];

	dma->buf_count += entry->buf_count;
	dma->byte_count += byte_count;

	DRM_DEBUG("dma->buf_count : %d\n", dma->buf_count);
	DRM_DEBUG("entry->buf_count : %d\n", entry->buf_count);

	request->count = entry->buf_count;
	request->size = size;

	dma->flags = _DRM_DMA_USE_AGP;

	return 0;
}

int
drm_addbufs_pci(struct drm_device *dev, struct drm_buf_desc *request)
{
	struct drm_device_dma	*dma = dev->dma;
	struct drm_buf		*buf, **temp_buflist;
	struct drm_buf_entry	*entry;
	int			 alignment, byte_count, count, i, order;
	int			 page_count, page_order, size, total;
	unsigned long		 offset, *temp_pagelist;

	count = request->count;
	order = drm_order(request->size);
	size = 1 << order;

	DRM_DEBUG("count=%d, size=%d (%d), order=%d\n",
	    request->count, request->size, size, order);

	alignment = (request->flags & _DRM_PAGE_ALIGN)
	    ? round_page(size) : size;
	page_order = order - PAGE_SHIFT > 0 ? order - PAGE_SHIFT : 0;
	total = PAGE_SIZE << page_order;

	entry = &dma->bufs[order];

	entry->buflist = drm_calloc(count, sizeof(*entry->buflist));
	entry->seglist = drm_calloc(count, sizeof(*entry->seglist));

	/* Keep the original pagelist until we know all the allocations
	 * have succeeded
	 */
	temp_pagelist = drm_calloc((dma->page_count + (count << page_order)),
	    sizeof(*dma->pagelist));

	if (entry->buflist == NULL || entry->seglist == NULL || 
	    temp_pagelist == NULL) {
		drm_free(temp_pagelist);
		drm_free(entry->seglist);
		drm_free(entry->buflist);
		return ENOMEM;
	}

	memcpy(temp_pagelist, dma->pagelist, dma->page_count * 
	    sizeof(*dma->pagelist));

	DRM_DEBUG("pagelist: %d entries\n",
	    dma->page_count + (count << page_order));

	entry->buf_size	= size;
	entry->page_order = page_order;
	byte_count = 0;
	page_count = 0;

	while (entry->buf_count < count) {
		struct drm_dmamem *mem = drm_dmamem_alloc(dev->dmat, size,
		    alignment, 1, size, 0, 0);
		if (mem == NULL) {
			/* Set count correctly so we free the proper amount. */
			entry->buf_count = count;
			entry->seg_count = count;
			drm_cleanup_buf(dev, entry);
			drm_free(temp_pagelist);
			return ENOMEM;
		}

		entry->seglist[entry->seg_count++] = mem;
		for (i = 0; i < (1 << page_order); i++) {
			DRM_DEBUG("page %d @ %p\n", dma->page_count +
			    page_count, mem->kva + PAGE_SIZE * i);
			temp_pagelist[dma->page_count + page_count++] = 
			    (long)mem->kva + PAGE_SIZE * i;
		}
		for (offset = 0;
		    offset + size <= total && entry->buf_count < count;
		    offset += alignment, ++entry->buf_count) {
			buf = &entry->buflist[entry->buf_count];
			buf->idx = dma->buf_count + entry->buf_count;
			buf->total = alignment;
			buf->used = 0;
			buf->offset = (dma->byte_count + byte_count + offset);
			buf->address = mem->kva + offset;
			buf->bus_address = mem->map->dm_segs[0].ds_addr +
			    offset;
			buf->pending = 0;
			buf->file_priv = NULL;

			buf->dev_private = drm_calloc(1,
			    dev->driver->buf_priv_size);
			if (buf->dev_private == NULL) {
				/* Set count so we free the proper amount. */
				entry->buf_count = count;
				entry->seg_count = count;
				drm_cleanup_buf(dev, entry);
				drm_free(temp_pagelist);
				return ENOMEM;
			}

			DRM_DEBUG("buffer %d\n",
			    entry->buf_count);
		}
		byte_count += PAGE_SIZE << page_order;
	}

	temp_buflist = drm_realloc(dma->buflist,
	    dma->buf_count * sizeof(*dma->buflist),
	    (dma->buf_count + entry->buf_count) * sizeof(*dma->buflist));
	if (temp_buflist == NULL) {
		/* Free the entry because it isn't valid */
		drm_cleanup_buf(dev, entry);
		drm_free(temp_pagelist);
		return ENOMEM;
	}
	dma->buflist = temp_buflist;

	for (i = 0; i < entry->buf_count; i++)
		dma->buflist[i + dma->buf_count] = &entry->buflist[i];

	/* No allocations failed, so now we can replace the orginal pagelist
	 * with the new one.
	 */
	drm_free(dma->pagelist);
	dma->pagelist = temp_pagelist;

	dma->buf_count += entry->buf_count;
	dma->seg_count += entry->seg_count;
	dma->page_count += entry->seg_count << page_order;
	dma->byte_count += PAGE_SIZE * (entry->seg_count << page_order);

	request->count = entry->buf_count;
	request->size = size;

	return 0;

}

int
drm_addbufs_sg(struct drm_device *dev, struct drm_buf_desc *request)
{
	struct drm_device_dma	*dma = dev->dma;
	struct drm_buf_entry	*entry;
	struct drm_buf		*buf, **temp_buflist;
	unsigned long		 agp_offset, offset;
	int			 alignment, byte_count, count, i, order;
	int			 page_order, size, total;

	count = request->count;
	order = drm_order(request->size);
	size = 1 << order;

	alignment  = (request->flags & _DRM_PAGE_ALIGN)
	    ? round_page(size) : size;
	page_order = order - PAGE_SHIFT > 0 ? order - PAGE_SHIFT : 0;
	total = PAGE_SIZE << page_order;

	byte_count = 0;
	agp_offset = request->agp_start;

	DRM_DEBUG("count:      %d\n",  count);
	DRM_DEBUG("order:      %d\n",  order);
	DRM_DEBUG("size:       %d\n",  size);
	DRM_DEBUG("agp_offset: %ld\n", agp_offset);
	DRM_DEBUG("alignment:  %d\n",  alignment);
	DRM_DEBUG("page_order: %d\n",  page_order);
	DRM_DEBUG("total:      %d\n",  total);

	entry = &dma->bufs[order];

	entry->buflist = drm_calloc(count, sizeof(*entry->buflist));
	if (entry->buflist == NULL)
		return ENOMEM;

	entry->buf_size = size;
	entry->page_order = page_order;

	offset = 0;

	while (entry->buf_count < count) {
		buf = &entry->buflist[entry->buf_count];
		buf->idx = dma->buf_count + entry->buf_count;
		buf->total = alignment;
		buf->used = 0;

		buf->offset = (dma->byte_count + offset);
		buf->bus_address = agp_offset + offset;
		buf->pending = 0;
		buf->file_priv = NULL;

		buf->dev_private = drm_calloc(1, dev->driver->buf_priv_size);
		if (buf->dev_private == NULL) {
			/* Set count correctly so we free the proper amount. */
			entry->buf_count = count;
			drm_cleanup_buf(dev, entry);
			return ENOMEM;
		}

		DRM_DEBUG("buffer %d\n", entry->buf_count);

		offset += alignment;
		entry->buf_count++;
		byte_count += PAGE_SIZE << page_order;
	}

	DRM_DEBUG("byte_count: %d\n", byte_count);

	temp_buflist = drm_realloc(dma->buflist, 
	    dma->buf_count * sizeof(*dma->buflist),
	    (dma->buf_count + entry->buf_count) * sizeof(*dma->buflist));
	if (temp_buflist == NULL) {
		/* Free the entry because it isn't valid */
		drm_cleanup_buf(dev, entry);
		return ENOMEM;
	}
	dma->buflist = temp_buflist;

	for (i = 0; i < entry->buf_count; i++)
		dma->buflist[i + dma->buf_count] = &entry->buflist[i];

	dma->buf_count += entry->buf_count;
	dma->byte_count += byte_count;

	DRM_DEBUG("dma->buf_count : %d\n", dma->buf_count);
	DRM_DEBUG("entry->buf_count : %d\n", entry->buf_count);

	request->count = entry->buf_count;
	request->size = size;

	dma->flags = _DRM_DMA_USE_SG;

	return 0;
}

int
drm_addbufs(struct drm_device *dev, struct drm_buf_desc *request)
{
	struct drm_device_dma	*dma = dev->dma;
	int			 order, ret;

	if (request->count < 0 || request->count > 4096)
		return (EINVAL);
	
	order = drm_order(request->size);
	if (order < DRM_MIN_ORDER || order > DRM_MAX_ORDER)
		return (EINVAL);

	rw_enter_write(&dma->dma_lock);

	/* No more allocations after first buffer-using ioctl. */
	if (dma->buf_use != 0) {
		rw_exit_write(&dma->dma_lock);
		return (EBUSY);
	}
	/* No more than one allocation per order */
	if (dma->bufs[order].buf_count != 0) {
		rw_exit_write(&dma->dma_lock);
		return (ENOMEM);
	}

	if (request->flags & _DRM_AGP_BUFFER)
		ret = drm_addbufs_agp(dev, request);
	else if (request->flags & _DRM_SG_BUFFER)
		ret = drm_addbufs_sg(dev, request);
	else
		ret = drm_addbufs_pci(dev, request);

	rw_exit_write(&dma->dma_lock);

	return (ret);
}

int
drm_freebufs(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_device_dma	*dma = dev->dma;
	struct drm_buf_free	*request = data;
	struct drm_buf		*buf;
	int			 i, idx, retcode = 0;

	DRM_DEBUG("%d\n", request->count);
	
	rw_enter_write(&dma->dma_lock);
	for (i = 0; i < request->count; i++) {
		if (DRM_COPY_FROM_USER(&idx, &request->list[i], sizeof(idx))) {
			retcode = EFAULT;
			break;
		}
		if (idx < 0 || idx >= dma->buf_count) {
			DRM_ERROR("Index %d (of %d max)\n", idx,
			    dma->buf_count - 1);
			retcode = EINVAL;
			break;
		}
		buf = dma->buflist[idx];
		if (buf->file_priv != file_priv) {
			DRM_ERROR("Process %d freeing buffer not owned\n",
			    DRM_CURRENTPID);
			retcode = EINVAL;
			break;
		}
		drm_free_buffer(dev, buf);
	}
	rw_exit_write(&dma->dma_lock);

	return retcode;
}

int
drm_mapbufs(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_device_dma	*dma = dev->dma;
	struct drm_buf_map	*request = data;
	struct vmspace		*vms;
	struct vnode		*vn;
	vaddr_t			 address, vaddr;
	voff_t			 foff;
	vsize_t			 size;
	const int		 zero = 0;
	int			 i, retcode = 0;

	if (!vfinddev(file_priv->kdev, VCHR, &vn))
		return EINVAL;

	vms = curproc->p_vmspace;

	rw_enter_write(&dma->dma_lock);
	dev->dma->buf_use++;	/* Can't allocate more after this call */
	rw_exit_write(&dma->dma_lock);

	if (request->count < dma->buf_count)
		goto done;

	if ((dev->driver->flags & DRIVER_AGP &&
	    (dma->flags & _DRM_DMA_USE_AGP)) ||
	    (dev->driver->flags & DRIVER_SG &&
	    (dma->flags & _DRM_DMA_USE_SG))) {
		struct drm_local_map *map = dev->agp_buffer_map;

		if (map == NULL) {
			DRM_DEBUG("couldn't find agp buffer map\n");
			retcode = EINVAL;
			goto done;
		}
		size = round_page(map->size);
		foff = map->ext;
	} else {
		size = round_page(dma->byte_count),
		foff = 0;
	}

	vaddr = round_page((vaddr_t)vms->vm_daddr + MAXDSIZ);
	retcode = uvm_mmap(&vms->vm_map, &vaddr, size,
	    UVM_PROT_READ | UVM_PROT_WRITE, UVM_PROT_ALL, MAP_SHARED,
	    (caddr_t)vn, foff, curproc->p_rlimit[RLIMIT_MEMLOCK].rlim_cur,
	    curproc);
	if (retcode) {
		DRM_DEBUG("uvm_mmap failed\n");
		goto done;
	}

	request->virtual = (void *)vaddr;

	for (i = 0; i < dma->buf_count; i++) {
		if (DRM_COPY_TO_USER(&request->list[i].idx,
		    &dma->buflist[i]->idx, sizeof(request->list[0].idx))) {
			retcode = EFAULT;
			goto done;
		}
		if (DRM_COPY_TO_USER(&request->list[i].total,
		    &dma->buflist[i]->total, sizeof(request->list[0].total))) {
			retcode = EFAULT;
			goto done;
		}
		if (DRM_COPY_TO_USER(&request->list[i].used, &zero,
		    sizeof(zero))) {
			retcode = EFAULT;
			goto done;
		}
		address = vaddr + dma->buflist[i]->offset; /* *** */
		if (DRM_COPY_TO_USER(&request->list[i].address, &address,
		    sizeof(address))) {
			retcode = EFAULT;
			goto done;
		}
	}

 done:
	request->count = dma->buf_count;

	DRM_DEBUG("%d buffers, retcode = %d\n", request->count, retcode);

	return retcode;
}
