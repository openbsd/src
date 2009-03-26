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

void*
drm_alloc(size_t size)
{
	return (malloc(size, M_DRM, M_NOWAIT));
}

void *
drm_calloc(size_t nmemb, size_t size)
{
	if (nmemb == 0 || SIZE_MAX / nmemb < size)
		return (NULL);
	else
		return malloc(size * nmemb, M_DRM, M_NOWAIT | M_ZERO);
}

void *
drm_realloc(void *oldpt, size_t oldsize, size_t size)
{
	void *pt;

	pt = malloc(size, M_DRM, M_NOWAIT);
	if (pt == NULL)
		return NULL;
	if (oldpt && oldsize) {
		memcpy(pt, oldpt, min(oldsize, size));
		free(oldpt, M_DRM);
	}
	return pt;
}

void
drm_free(void *pt)
{
	if (pt != NULL)
		free(pt, M_DRM);
}

void *
drm_ioremap(struct drm_device *dev, drm_local_map_t *map)
{
	DRM_DEBUG("offset: 0x%x size: 0x%x type: %d\n", map->offset, map->size,
	    map->type);

	if (map->type == _DRM_AGP || map->type == _DRM_FRAME_BUFFER) {
	/*
	 * there can be multiple agp maps in the same BAR, agp also
	 * quite possibly isn't the same as the vga device, just try
	 * to map it.
	 */
		DRM_DEBUG("AGP map\n");
		map->bst = dev->bst;
		if (bus_space_map(map->bst, map->offset,
		    map->size, BUS_SPACE_MAP_LINEAR, &map->bsh)) {
			DRM_ERROR("ioremap fail\n");
			return (NULL);
		}
	} else {
		return (NULL);
	}
	/* handles are still supposed to be kernel virtual addresses */
	return bus_space_vaddr(map->bst, map->bsh);
}

void
drm_ioremapfree(drm_local_map_t *map)
{
	if (map == NULL || (map->type != _DRM_AGP && map->type !=
	    _DRM_FRAME_BUFFER))
		return;

	bus_space_unmap(map->bst, map->bsh, map->size);
}

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

u_int8_t
drm_read8(drm_local_map_t *map, unsigned long offset)
{
	u_int8_t *ptr;

	switch (map->type) {
	case _DRM_SCATTER_GATHER:
		ptr = map->handle + offset;
		return  (*ptr);
		
	default:
		return (bus_space_read_1(map->bst, map->bsh, offset));
	}
}

u_int16_t
drm_read16(drm_local_map_t *map, unsigned long offset)
{
	u_int16_t *ptr;
	switch (map->type) {
	case _DRM_SCATTER_GATHER:
		ptr = map->handle + offset;
		return  (*ptr);
	default:
		return (bus_space_read_2(map->bst, map->bsh, offset));
	}
}

u_int32_t
drm_read32(drm_local_map_t *map, unsigned long offset)
{
	u_int32_t *ptr;
	switch (map->type) {
	case _DRM_SCATTER_GATHER:
		ptr = map->handle + offset;
		return  (*ptr);
	default:
		return (bus_space_read_4(map->bst, map->bsh, offset));
	}
}

void
drm_write8(drm_local_map_t *map, unsigned long offset, u_int8_t val)
{
	u_int8_t *ptr;
	switch (map->type) {
	case _DRM_SCATTER_GATHER:
		ptr = map->handle + offset;
		*ptr = val;
		break;
	default:
		bus_space_write_1(map->bst, map->bsh, offset, val);
	}
}

void
drm_write16(drm_local_map_t *map, unsigned long offset, u_int16_t val)
{
	u_int16_t *ptr;
	switch (map->type) {
	case _DRM_SCATTER_GATHER:
		ptr = map->handle + offset;
		*ptr = val;
		break;
	default:
		bus_space_write_2(map->bst, map->bsh, offset, val);
	}
}

void
drm_write32(drm_local_map_t *map, unsigned long offset, u_int32_t val)
{
	u_int32_t *ptr;
	switch (map->type) {
	case _DRM_SCATTER_GATHER:
		ptr = map->handle + offset;
		*ptr = val;
		break;
	default:
		bus_space_write_4(map->bst, map->bsh, offset, val);
	}
}
