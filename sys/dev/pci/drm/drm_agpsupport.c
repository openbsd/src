/*-
 * Copyright 1999 Precision Insight, Inc., Cedar Park, Texas.
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
 * Author:
 *    Rickard E. (Rik) Faith <faith@valinux.com>
 *    Gareth Hughes <gareth@valinux.com>
 *
 */

/*
 * Support code for tying the kernel AGP support to DRM drivers and
 * the DRM's AGP ioctls.
 */

#include "drmP.h"

struct drm_agp_mem	*drm_agp_lookup_entry(struct drm_device *, void *);
void			 drm_agp_remove_entry(struct drm_device *,
			     struct drm_agp_mem *);

int
drm_agp_info(struct drm_device * dev, struct drm_agp_info *info)
{
	struct agp_info	*kern;

	if (dev->agp == NULL || !dev->agp->acquired)
		return (EINVAL);

	kern = &dev->agp->info;
#ifndef DRM_NO_AGP
	agp_get_info(dev->agp->agpdev, kern);
#endif
	info->agp_version_major = 1;
	info->agp_version_minor = 0;
	info->mode = kern->ai_mode;
	info->aperture_base = kern->ai_aperture_base;
	info->aperture_size = kern->ai_aperture_size;
	info->memory_allowed = kern->ai_memory_allowed;
	info->memory_used = kern->ai_memory_used;
	info->id_vendor = kern->ai_devid & 0xffff;
	info->id_device = kern->ai_devid >> 16;

	return (0);
}

int
drm_agp_info_ioctl(struct drm_device *dev, void *data,
    struct drm_file *file_priv)
{
	struct drm_agp_info	*info = data;

	return (drm_agp_info(dev, info));
}

int
drm_agp_acquire_ioctl(struct drm_device *dev, void *data,
    struct drm_file *file_priv)
{
	return (drm_agp_acquire(dev));
}

int
drm_agp_acquire(struct drm_device *dev)
{
#ifndef DRM_NO_AGP
	int	retcode;

	if (dev->agp == NULL || dev->agp->acquired)
		return (EINVAL);

	retcode = agp_acquire(dev->agp->agpdev);
	if (retcode)
		return (retcode);

	dev->agp->acquired = 1;
#endif
	return (0);
}

int
drm_agp_release_ioctl(struct drm_device *dev, void *data,
    struct drm_file *file_priv)
{
	return (drm_agp_release(dev));
}

int
drm_agp_release(struct drm_device * dev)
{
#ifndef DRM_NO_AGP
	if (dev->agp == NULL || !dev->agp->acquired)
		return (EINVAL);
	agp_release(dev->agp->agpdev);
	dev->agp->acquired = 0;
#endif
	return (0);
}

int
drm_agp_enable(struct drm_device *dev, drm_agp_mode_t mode)
{
	int	retcode = 0;
#ifndef DRM_NO_AGP
	if (dev->agp == NULL || !dev->agp->acquired)
		return (EINVAL);
	
	dev->agp->mode = mode.mode;
	if ((retcode = agp_enable(dev->agp->agpdev, mode.mode)) == 0)
		dev->agp->enabled = 1;
#endif
	return (retcode);
}

int
drm_agp_enable_ioctl(struct drm_device *dev, void *data,
    struct drm_file *file_priv)
{
	struct drm_agp_mode	*mode = data;

	return (drm_agp_enable(dev, *mode));
}

int
drm_agp_alloc(struct drm_device *dev, struct drm_agp_buffer *request)
{
#ifndef DRM_NO_AGP
	struct drm_agp_mem	*entry;
	void			*handle;
	struct agp_memory_info	 info;
	unsigned long		 pages;
	u_int32_t		 type;

	if (dev->agp == NULL || !dev->agp->acquired)
		return (EINVAL);

	entry = drm_alloc(sizeof(*entry));
	if (entry == NULL)
		return (ENOMEM);

	pages = (request->size + PAGE_SIZE - 1) / PAGE_SIZE;
	type = (u_int32_t)request->type;

	handle = agp_alloc_memory(dev->agp->agpdev, type,
	    pages << AGP_PAGE_SHIFT);
	if (handle == NULL) {
		drm_free(entry);
		return (ENOMEM);
	}
	
	entry->handle = handle;
	entry->bound = 0;
	entry->pages = pages;

	agp_memory_info(dev->agp->agpdev, entry->handle, &info);

	request->handle = (unsigned long)entry->handle;
        request->physical = info.ami_physical;
	DRM_LOCK();
	TAILQ_INSERT_HEAD(&dev->agp->memory, entry, link);
	DRM_UNLOCK();
#endif

	return (0);
}

int
drm_agp_alloc_ioctl(struct drm_device *dev, void *data,
    struct drm_file *file_priv)
{
	struct drm_agp_buffer	*request = data;

	return (drm_agp_alloc(dev, request));
}

/*
 * find entry on agp list. Must be called with dev_lock locked.
 */
struct drm_agp_mem *
drm_agp_lookup_entry(struct drm_device *dev, void *handle)
{
	struct drm_agp_mem	*entry;

	TAILQ_FOREACH(entry, &dev->agp->memory, link) {
		if (entry->handle == handle)
			break;
	}
	return (entry);
}

int
drm_agp_unbind(struct drm_device *dev, struct drm_agp_binding *request)
{
	struct drm_agp_mem	*entry;
	int			 retcode;

	if (dev->agp == NULL || !dev->agp->acquired)
		return (EINVAL);

	DRM_LOCK();
	entry = drm_agp_lookup_entry(dev, (void *)request->handle);
	if (entry == NULL || !entry->bound) {
		DRM_UNLOCK();
		return (EINVAL);
	}

	retcode =  agp_unbind_memory(dev->agp->agpdev, entry->handle);

	if (retcode == 0)
		entry->bound = 0;
	DRM_UNLOCK();

	return (retcode);
}

int
drm_agp_unbind_ioctl(struct drm_device *dev, void *data,
    struct drm_file *file_priv)
{
	struct drm_agp_binding	*request = data;

	return (drm_agp_unbind(dev, request));
}

int
drm_agp_bind(struct drm_device *dev, struct drm_agp_binding *request)
{
	struct drm_agp_mem	*entry;
	int			 retcode, page;
	
	if (dev->agp == NULL || !dev->agp->acquired)
		return (EINVAL);

	DRM_DEBUG("agp_bind, page_size=%x\n", PAGE_SIZE);

	DRM_LOCK();
	entry = drm_agp_lookup_entry(dev, (void *)request->handle);
	if (entry == NULL || entry->bound) {
		DRM_UNLOCK();
		return (EINVAL);
	}

	page = (request->offset + PAGE_SIZE - 1) / PAGE_SIZE;

	retcode = agp_bind_memory(dev->agp->agpdev, entry->handle,
	    page * PAGE_SIZE);
	if (retcode == 0)
		entry->bound = dev->agp->base + (page << PAGE_SHIFT);
	DRM_UNLOCK();

	return (retcode);
}

int
drm_agp_bind_ioctl(struct drm_device *dev, void *data,
    struct drm_file *file_priv)
{
	struct drm_agp_binding	*request = data;

	return (drm_agp_bind(dev, request));
}

/*
 * Remove entry from list and free. Call locked.
 */
void
drm_agp_remove_entry(struct drm_device *dev, struct drm_agp_mem *entry)
{
	TAILQ_REMOVE(&dev->agp->memory, entry, link);

	if (entry->bound)
		agp_unbind_memory(dev->agp->agpdev, entry->handle);
	agp_free_memory(dev->agp->agpdev, entry->handle);
	drm_free(entry);
}

void
drm_agp_takedown(struct drm_device *dev)
{
	struct drm_agp_mem	*entry;

	if (dev->agp == NULL)
		return;

	/*
	 * Remove AGP resources, but leave dev->agp intact until
	 * we detach the device
	 */
	DRM_LOCK();
	while ((entry = TAILQ_FIRST(&dev->agp->memory)) != NULL)
		drm_agp_remove_entry(dev, entry);
	DRM_UNLOCK();

	drm_agp_release(dev);
	dev->agp->enabled  = 0;
}

int
drm_agp_free(struct drm_device *dev, struct drm_agp_buffer *request)
{
	struct drm_agp_mem	*entry;
	
	if (dev->agp == NULL || !dev->agp->acquired)
		return (EINVAL);

	DRM_LOCK();
	entry = drm_agp_lookup_entry(dev, (void*)request->handle);
	if (entry == NULL) {
		DRM_UNLOCK();
		return (EINVAL);
	}

	drm_agp_remove_entry(dev, entry);
	DRM_UNLOCK();
   
	return (0);
}

int
drm_agp_free_ioctl(struct drm_device *dev, void *data,
    struct drm_file *file_priv)
{
	struct drm_agp_buffer	*request = data;

	return (drm_agp_free(dev, request));
}

struct drm_agp_head *
drm_agp_init(void)
{
#ifndef DRM_NO_AGP
	struct device		*agpdev;
	struct drm_agp_head	*head = NULL;
	int		 	 agp_available = 1;
   
	agpdev = agp_find_device(0);
	if (agpdev == NULL)
		agp_available = 0;

	DRM_DEBUG("agp_available = %d\n", agp_available);

	if (agp_available) {
		head = drm_calloc(1, sizeof(*head));
		if (head == NULL)
			return (NULL);
		head->agpdev = agpdev;
		agp_get_info(agpdev, &head->info);
		head->base = head->info.ai_aperture_base;
		TAILQ_INIT(&head->memory);
	}
	return (head);
#else
	return (NULL);
#endif
}
