/*-
 *Copyright 1999 Precision Insight, Inc., Cedar Park, Texas.
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

/** @file drm_memory.c
 * Wrappers for kernel memory allocation routines, and MTRR management support.
 *
 * This file previously implemented a memory consumption tracking system using
 * the "area" argument for various different types of allocations, but that
 * has been stripped out for now.
 */

#include "drmP.h"

#ifndef __OpenBSD__
MALLOC_DEFINE(M_DRM, "drm", "DRM Data Structures");
#endif

void
drm_mem_init(void)
{
#if defined(__NetBSD__) 
/*
	malloc_type_attach(M_DRM);
*/
#endif
}

void
drm_mem_uninit(void)
{
}

void*
drm_alloc(size_t size, int area)
{
	return malloc(size, M_DRM, M_NOWAIT);
}

void *
drm_calloc(size_t nmemb, size_t size, int area)
{
	/* XXX overflow checking */
	return malloc(size * nmemb, M_DRM, M_NOWAIT | M_ZERO);
}

void *
drm_realloc(void *oldpt, size_t oldsize, size_t size, int area)
{
	void *pt;

	pt = malloc(size, M_DRM, M_NOWAIT);
	if (pt == NULL)
		return NULL;
	if (oldpt && oldsize) {
		memcpy(pt, oldpt, oldsize);
		free(oldpt, M_DRM);
	}
	return pt;
}

void
drm_free(void *pt, size_t size, int area)
{
	free(pt, M_DRM);
}

void *
drm_ioremap(drm_device_t *dev, drm_local_map_t *map)
{
#ifdef __FreeBSD__
	return pmap_mapdev(map->offset, map->size);
#elif defined(__NetBSD__) || defined(__OpenBSD__)
	struct vga_pci_bar *bar = NULL;
	int i;

	DRM_DEBUG("offset: 0x%x size: 0x%x type: %d\n", map->offset, map->size,
	    map->type);

	if (map->type == _DRM_AGP || map->type == _DRM_FRAME_BUFFER) {
	/*
	 * there can be multiple agp maps in the same BAR, agp also
	 * quite possibly isn't the same as the vga device, just try
	 * to map it.
	 */
		DRM_DEBUG("AGP map\n");
		map->bst = dev->pa.pa_memt;
		if (bus_space_map(map->bst, map->offset,
		    map->size, BUS_SPACE_MAP_LINEAR, &map->bsh)) {
			DRM_ERROR("ioremap fail\n");
			return (NULL);
		}
		goto done;
	} else {
		for (i = 0 ; i < DRM_MAX_PCI_RESOURCE; ++i) {
			bar = vga_pci_bar_info(dev->vga_softc, i);
			if (bar == NULL)
				continue;

			if (bar->base == map->offset) {
				DRM_DEBUG("REGISTERS map\n");
				map->bsr = vga_pci_bar_map(dev->vga_softc,
				    bar->addr, map->size, BUS_SPACE_MAP_LINEAR);
				if (map->bsr == NULL) {
					DRM_ERROR("ioremap fail\n");
					return (NULL);
				}
				map->bst = map->bsr->bst;
				map->bsh = map->bsr->bsh;
				goto done;
			}
		}
	}
done:
	/* handles are still supposed to be kernel virtual addresses */
	return bus_space_vaddr(map->bst, map->bsh);
#endif
}

void
drm_ioremapfree(drm_local_map_t *map)
{
#ifdef __FreeBSD__
	pmap_unmapdev((vm_offset_t) map->handle, map->size);
#elif defined(__NetBSD__) || defined(__OpenBSD__)
	if (map != NULL && map->bsr != NULL)
		vga_pci_bar_unmap(map->bsr);
	else
		bus_space_unmap(map->bst, map->bsh, map->size);
#endif
}

#if defined(__FreeBSD__) || defined(__OpenBSD__)
int
drm_mtrr_add(unsigned long offset, size_t size, int flags)
{
#ifndef DRM_NO_MTRR
	int act;
	struct mem_range_desc mrdesc;

	mrdesc.mr_base = offset;
	mrdesc.mr_len = size;
	mrdesc.mr_flags = flags;
	act = MEMRANGE_SET_UPDATE;
	strlcpy(mrdesc.mr_owner, "drm", sizeof(mrdesc.mr_owner));
	return mem_range_attr_set(&mrdesc, &act);
#else
	return 0;
#endif
}

int
drm_mtrr_del(int __unused handle, unsigned long offset, size_t size, int flags)
{
#ifndef DRM_NO_MTRR
	int act;
	struct mem_range_desc mrdesc;

	mrdesc.mr_base = offset;
	mrdesc.mr_len = size;
	mrdesc.mr_flags = flags;
	act = MEMRANGE_SET_REMOVE;
	strlcpy(mrdesc.mr_owner, "drm", sizeof(mrdesc.mr_owner));
	return mem_range_attr_set(&mrdesc, &act);
#else
	return 0;
#endif
}
#elif defined(__NetBSD__) 
int
drm_mtrr_add(unsigned long offset, size_t size, int flags)
{
#ifndef DRM_NO_MTRR
	struct mtrr mtrrmap;
	int one = 1;

	DRM_DEBUG("offset=%lx size=%ld\n", (long)offset, (long)size);
	mtrrmap.base = offset;
	mtrrmap.len = size;
	mtrrmap.type = flags;
	mtrrmap.flags = MTRR_VALID;
	return mtrr_set(&mtrrmap, &one, NULL, MTRR_GETSET_KERNEL);
#else
	return 0;
#endif
}

int
drm_mtrr_del(int __unused handle, unsigned long offset, size_t size, int flags)
{
#ifndef DRM_NO_MTRR
	struct mtrr mtrrmap;
	int one = 1;

	DRM_DEBUG("offset=%lx size=%ld\n", (long)offset, (long)size);
	mtrrmap.base = offset;
	mtrrmap.len = size;
	mtrrmap.type = flags;
	mtrrmap.flags = 0;
	return mtrr_set(&mtrrmap, &one, NULL, MTRR_GETSET_KERNEL);
#else
	return 0;
#endif
}
#endif
