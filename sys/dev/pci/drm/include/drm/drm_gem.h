#ifndef __DRM_GEM_H__
#define __DRM_GEM_H__

/*
 * GEM Graphics Execution Manager Driver Interfaces
 *
 * Copyright 1999 Precision Insight, Inc., Cedar Park, Texas.
 * Copyright 2000 VA Linux Systems, Inc., Sunnyvale, California.
 * Copyright (c) 2009-2010, Code Aurora Forum.
 * All rights reserved.
 * Copyright © 2014 Intel Corporation
 *   Daniel Vetter <daniel.vetter@ffwll.ch>
 *
 * Author: Rickard E. (Rik) Faith <faith@valinux.com>
 * Author: Gareth Hughes <gareth@valinux.com>
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
 */

#include <linux/kref.h>

#include <drm/drm_vma_manager.h>

/**
 * struct drm_gem_object - GEM buffer object
 *
 * This structure defines the generic parts for GEM buffer objects, which are
 * mostly around handling mmap and userspace handles.
 *
 * Buffer objects are often abbreviated to BO.
 */
struct drm_gem_object {
	/**
	 * @refcount:
	 *
	 * Reference count of this object
	 *
	 * Please use drm_gem_object_get() to acquire and drm_gem_object_put()
	 * or drm_gem_object_put_unlocked() to release a reference to a GEM
	 * buffer object.
	 */
	struct kref refcount;

	/**
	 * @handle_count:
	 *
	 * This is the GEM file_priv handle count of this object.
	 *
	 * Each handle also holds a reference. Note that when the handle_count
	 * drops to 0 any global names (e.g. the id in the flink namespace) will
	 * be cleared.
	 *
	 * Protected by &drm_device.object_name_lock.
	 */
	unsigned handle_count;

	/**
	 * @dev: DRM dev this object belongs to.
	 */
	struct drm_device *dev;

	/**
	 * @filp:
	 *
	 * SHMEM file node used as backing storage for swappable buffer objects.
	 * GEM also supports driver private objects with driver-specific backing
	 * storage (contiguous CMA memory, special reserved blocks). In this
	 * case @filp is NULL.
	 */
	struct file *filp;

	/**
	 * @vma_node:
	 *
	 * Mapping info for this object to support mmap. Drivers are supposed to
	 * allocate the mmap offset using drm_gem_create_mmap_offset(). The
	 * offset itself can be retrieved using drm_vma_node_offset_addr().
	 *
	 * Memory mapping itself is handled by drm_gem_mmap(), which also checks
	 * that userspace is allowed to access the object.
	 */
	struct drm_vma_offset_node vma_node;

	/**
	 * @size:
	 *
	 * Size of the object, in bytes.  Immutable over the object's
	 * lifetime.
	 */
	size_t size;

	/**
	 * @name:
	 *
	 * Global name for this object, starts at 1. 0 means unnamed.
	 * Access is covered by &drm_device.object_name_lock. This is used by
	 * the GEM_FLINK and GEM_OPEN ioctls.
	 */
	int name;

	/**
	 * @dma_buf:
	 *
	 * dma-buf associated with this GEM object.
	 *
	 * Pointer to the dma-buf associated with this gem object (either
	 * through importing or exporting). We break the resulting reference
	 * loop when the last gem handle for this object is released.
	 *
	 * Protected by &drm_device.object_name_lock.
	 */
	struct dma_buf *dma_buf;

	/**
	 * @import_attach:
	 *
	 * dma-buf attachment backing this object.
	 *
	 * Any foreign dma_buf imported as a gem object has this set to the
	 * attachment point for the device. This is invariant over the lifetime
	 * of a gem object.
	 *
	 * The &drm_driver.gem_free_object callback is responsible for cleaning
	 * up the dma_buf attachment and references acquired at import time.
	 *
	 * Note that the drm gem/prime core does not depend upon drivers setting
	 * this field any more. So for drivers where this doesn't make sense
	 * (e.g. virtual devices or a displaylink behind an usb bus) they can
	 * simply leave it as NULL.
	 */
	struct dma_buf_attachment *import_attach;

	struct uvm_object uobj;
	SPLAY_ENTRY(drm_gem_object) entry;
	struct uvm_object *uao;
};

/**
 * DEFINE_DRM_GEM_FOPS() - macro to generate file operations for GEM drivers
 * @name: name for the generated structure
 *
 * This macro autogenerates a suitable &struct file_operations for GEM based
 * drivers, which can be assigned to &drm_driver.fops. Note that this structure
 * cannot be shared between drivers, because it contains a reference to the
 * current module using THIS_MODULE.
 *
 * Note that the declaration is already marked as static - if you need a
 * non-static version of this you're probably doing it wrong and will break the
 * THIS_MODULE reference by accident.
 */
#define DEFINE_DRM_GEM_FOPS(name) \
	static const struct file_operations name = {\
		.owner		= THIS_MODULE,\
		.open		= drm_open,\
		.release	= drm_release,\
		.unlocked_ioctl	= drm_ioctl,\
		.compat_ioctl	= drm_compat_ioctl,\
		.poll		= drm_poll,\
		.read		= drm_read,\
		.llseek		= noop_llseek,\
		.mmap		= drm_gem_mmap,\
	}

void drm_gem_object_release(struct drm_gem_object *obj);
void drm_gem_object_free(struct kref *kref);
int drm_gem_object_init(struct drm_device *dev,
			struct drm_gem_object *obj, size_t size);
void drm_gem_private_object_init(struct drm_device *dev,
				 struct drm_gem_object *obj, size_t size);
#ifdef __linux__
void drm_gem_vm_open(struct vm_area_struct *vma);
void drm_gem_vm_close(struct vm_area_struct *vma);
int drm_gem_mmap_obj(struct drm_gem_object *obj, unsigned long obj_size,
		     struct vm_area_struct *vma);
int drm_gem_mmap(struct file *filp, struct vm_area_struct *vma);
#endif

/**
 * drm_gem_object_get - acquire a GEM buffer object reference
 * @obj: GEM buffer object
 *
 * This function acquires an additional reference to @obj. It is illegal to
 * call this without already holding a reference. No locks required.
 */
static inline void drm_gem_object_get(struct drm_gem_object *obj)
{
	kref_get(&obj->refcount);
}

/**
 * __drm_gem_object_put - raw function to release a GEM buffer object reference
 * @obj: GEM buffer object
 *
 * This function is meant to be used by drivers which are not encumbered with
 * &drm_device.struct_mutex legacy locking and which are using the
 * gem_free_object_unlocked callback. It avoids all the locking checks and
 * locking overhead of drm_gem_object_put() and drm_gem_object_put_unlocked().
 *
 * Drivers should never call this directly in their code. Instead they should
 * wrap it up into a ``driver_gem_object_put(struct driver_gem_object *obj)``
 * wrapper function, and use that. Shared code should never call this, to
 * avoid breaking drivers by accident which still depend upon
 * &drm_device.struct_mutex locking.
 */
static inline void
__drm_gem_object_put(struct drm_gem_object *obj)
{
	kref_put(&obj->refcount, drm_gem_object_free);
}

void drm_gem_object_put_unlocked(struct drm_gem_object *obj);
void drm_gem_object_put(struct drm_gem_object *obj);

/**
 * drm_gem_object_reference - acquire a GEM buffer object reference
 * @obj: GEM buffer object
 *
 * This is a compatibility alias for drm_gem_object_get() and should not be
 * used by new code.
 */
static inline void drm_gem_object_reference(struct drm_gem_object *obj)
{
	drm_gem_object_get(obj);
}

/**
 * __drm_gem_object_unreference - raw function to release a GEM buffer object
 *                                reference
 * @obj: GEM buffer object
 *
 * This is a compatibility alias for __drm_gem_object_put() and should not be
 * used by new code.
 */
static inline void __drm_gem_object_unreference(struct drm_gem_object *obj)
{
	__drm_gem_object_put(obj);
}

/**
 * drm_gem_object_unreference_unlocked - release a GEM buffer object reference
 * @obj: GEM buffer object
 *
 * This is a compatibility alias for drm_gem_object_put_unlocked() and should
 * not be used by new code.
 */
static inline void
drm_gem_object_unreference_unlocked(struct drm_gem_object *obj)
{
	drm_gem_object_put_unlocked(obj);
}

/**
 * drm_gem_object_unreference - release a GEM buffer object reference
 * @obj: GEM buffer object
 *
 * This is a compatibility alias for drm_gem_object_put() and should not be
 * used by new code.
 */
static inline void drm_gem_object_unreference(struct drm_gem_object *obj)
{
	drm_gem_object_put(obj);
}

int drm_gem_handle_create(struct drm_file *file_priv,
			  struct drm_gem_object *obj,
			  u32 *handlep);
int drm_gem_handle_delete(struct drm_file *filp, u32 handle);


void drm_gem_free_mmap_offset(struct drm_gem_object *obj);
int drm_gem_create_mmap_offset(struct drm_gem_object *obj);
int drm_gem_create_mmap_offset_size(struct drm_gem_object *obj, size_t size);

struct page **drm_gem_get_pages(struct drm_gem_object *obj);
void drm_gem_put_pages(struct drm_gem_object *obj, struct page **pages,
		bool dirty, bool accessed);

struct drm_gem_object *drm_gem_object_lookup(struct drm_file *filp, u32 handle);
int drm_gem_dumb_map_offset(struct drm_file *file, struct drm_device *dev,
			    u32 handle, u64 *offset);
int drm_gem_dumb_destroy(struct drm_file *file,
			 struct drm_device *dev,
			 uint32_t handle);

#endif /* __DRM_GEM_H__ */
