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

/** @file drm_scatter.c
 * Allocation of memory for scatter-gather mappings by the graphics chip.
 *
 * The memory allocated here is then made into an aperture in the card
 * by drm_ati_pcigart_init().
 */
#include "drmP.h"

#define DEBUG_SCATTER 0
struct drm_sg_dmamem	*drm_sg_dmamem_alloc(drm_device_t *, size_t);
void	drm_sg_dmamem_free(struct drm_sg_dmamem *);

void
drm_sg_cleanup(drm_sg_mem_t *entry)
{
	if (entry) {
#ifdef __OpenBSD__
		if (entry->mem)
			drm_sg_dmamem_free(entry->mem);
#else
		if (entry->handle)
			free((void *)entry->handle, M_DRM);
#endif
		if (entry->busaddr)
			free(entry->busaddr, M_DRM);
		free(entry, M_DRM);
	}
}

int
drm_sg_alloc(drm_device_t * dev, drm_scatter_gather_t * request)
{
	drm_sg_mem_t *entry;
	unsigned long pages;
	int i;

	if ( dev->sg )
		return EINVAL;

	entry = malloc(sizeof(*entry), M_DRM, M_WAITOK | M_ZERO);
	if ( !entry )
		return ENOMEM;

	pages = round_page(request->size) / PAGE_SIZE;
	DRM_DEBUG( "sg size=%ld pages=%ld\n", request->size, pages );

	entry->pages = pages;

	entry->busaddr = malloc(pages * sizeof(*entry->busaddr), M_DRM,
	    M_WAITOK | M_ZERO);
	if ( !entry->busaddr ) {
		drm_sg_cleanup(entry);
		return ENOMEM;
	}

#ifdef __OpenBSD__
	if ((entry->mem = drm_sg_dmamem_alloc(dev, pages)) == NULL) {
		drm_sg_cleanup(entry);
		return ENOMEM;
	}

	entry->handle = (unsigned long)entry->mem->sg_kva;

	for (i = 0; i < pages; i++) 
		entry->busaddr[i] = entry->mem->sg_map->dm_segs[i].ds_addr;
#else
	entry->handle = (long)malloc(pages << PAGE_SHIFT, M_DRM,
	    M_WAITOK | M_ZERO);
	if (entry->handle == NULL) {
		drm_sg_cleanup(entry);
		return ENOMEM;
	}

	for (i = 0; i < pages; i++) {
		entry->busaddr[i] = vtophys(entry->handle + i * PAGE_SIZE);
	}
#endif

	DRM_DEBUG( "sg alloc handle  = %08lx\n", entry->handle );

	entry->virtual = (void *)entry->handle;
	request->handle = entry->handle;

	DRM_LOCK();
	if (dev->sg) {
		DRM_UNLOCK();
		drm_sg_cleanup(entry);
		return EINVAL;
	}
	dev->sg = entry;
	DRM_UNLOCK();

	return 0;
}

int
drm_sg_alloc_ioctl(drm_device_t *dev, void *data, struct drm_file *file_priv)
{
	drm_scatter_gather_t *request = data;
	int ret;

	DRM_DEBUG( "%s\n", __FUNCTION__ );

	ret = drm_sg_alloc(dev, request);
	return ret;
}

int
drm_sg_free(drm_device_t *dev, void *data, struct drm_file *file_priv)
{
	drm_scatter_gather_t *request = data;
	drm_sg_mem_t *entry;

	DRM_LOCK();
	entry = dev->sg;
	dev->sg = NULL;
	DRM_UNLOCK();

	if ( !entry || entry->handle != request->handle )
		return EINVAL;

	DRM_DEBUG( "sg free virtual  = 0x%lx\n", entry->handle );

	drm_sg_cleanup(entry);

	return 0;
}

/*
 * allocate `pages' pages of dma memory for use in
 * scatter/gather
 */
struct drm_sg_dmamem*
drm_sg_dmamem_alloc(drm_device_t *dev, size_t pages)
{
	struct drm_sg_dmamem	*dsd = NULL;
	bus_size_t	  	 size = pages << PAGE_SHIFT;
	int			 ret = 0;

	dsd = malloc(sizeof(*dsd), M_DRM, M_NOWAIT | M_ZERO);
	if (dsd == NULL)
		return (NULL);

	dsd->sg_segs = malloc(sizeof(*dsd->sg_segs) * pages, M_DRM,
	    M_NOWAIT | M_ZERO);
	if (dsd->sg_segs == NULL)
		goto dsdfree;

	dsd->sg_tag = dev->pa.pa_dmat;
	dsd->sg_size = size;

	if (bus_dmamap_create(dev->pa.pa_dmat, size, pages, PAGE_SIZE, 0,
	    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, &dsd->sg_map) != 0)
		goto segsfree;

	if ((ret = bus_dmamem_alloc(dev->pa.pa_dmat, size, PAGE_SIZE, 0,
	    dsd->sg_segs, pages, &dsd->sg_nsegs, BUS_DMA_NOWAIT)) != 0) {
		printf("alloc failed, value= %d\n",ret);
		goto destroy;
	}

	if (bus_dmamem_map(dev->pa.pa_dmat, dsd->sg_segs, dsd->sg_nsegs, size, 
	    &dsd->sg_kva, BUS_DMA_NOWAIT) != 0)
		goto free;

	if (bus_dmamap_load(dev->pa.pa_dmat, dsd->sg_map, dsd->sg_kva, size,
	    NULL, BUS_DMA_NOWAIT) != 0)
		goto unmap;

	bzero(dsd->sg_kva, size);

	return (dsd);

unmap:
	bus_dmamem_unmap(dev->pa.pa_dmat, dsd->sg_kva, size);
free:
	bus_dmamem_free(dev->pa.pa_dmat, dsd->sg_segs, dsd->sg_nsegs);
destroy:
	bus_dmamap_destroy(dev->pa.pa_dmat, dsd->sg_map);
segsfree:
	free(dsd->sg_segs, M_DRM);

dsdfree:
	free(dsd, M_DRM);

	return (NULL);
}

void
drm_sg_dmamem_free(struct drm_sg_dmamem *mem)
{
	bus_dmamap_unload(mem->sg_tag, mem->sg_map);
	bus_dmamem_unmap(mem->sg_tag, mem->sg_kva, mem->sg_size);
	bus_dmamem_free(mem->sg_tag, mem->sg_segs, mem->sg_nsegs);
	bus_dmamap_destroy(mem->sg_tag, mem->sg_map);
	free(mem->sg_segs, M_DRM);
	free(mem, M_DRM);
}
