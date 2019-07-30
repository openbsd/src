/*	$OpenBSD: drm_gem.c,v 1.5 2017/09/03 13:28:54 jsg Exp $	*/
/*
 * Copyright © 2008 Intel Corporation
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
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 *    Eric Anholt <eric@anholt.net>
 *
 */

#include <dev/pci/drm/drmP.h>
#include <dev/pci/drm/drm_vma_manager.h>

#include <uvm/uvm.h>

void drm_unref(struct uvm_object *);
void drm_ref(struct uvm_object *);
boolean_t drm_flush(struct uvm_object *, voff_t, voff_t, int);
int drm_fault(struct uvm_faultinfo *, vaddr_t, vm_page_t *, int, int,
    vm_fault_t, vm_prot_t, int);

struct uvm_pagerops drm_pgops = {
	NULL,
	drm_ref,
	drm_unref,
	drm_fault,
	drm_flush,
};

void
drm_ref(struct uvm_object *uobj)
{
	struct drm_gem_object *obj =
	    container_of(uobj, struct drm_gem_object, uobj);

	drm_gem_object_reference(obj);
}

void
drm_unref(struct uvm_object *uobj)
{
	struct drm_gem_object *obj =
	    container_of(uobj, struct drm_gem_object, uobj);

	drm_gem_object_unreference_unlocked(obj);
}

int
drm_fault(struct uvm_faultinfo *ufi, vaddr_t vaddr, vm_page_t *pps,
    int npages, int centeridx, vm_fault_t fault_type,
    vm_prot_t access_type, int flags)
{
	struct vm_map_entry *entry = ufi->entry;
	struct uvm_object *uobj = entry->object.uvm_obj;
	struct drm_gem_object *obj =
	    container_of(uobj, struct drm_gem_object, uobj);
	struct drm_device *dev = obj->dev;
	int ret;

	/*
	 * we do not allow device mappings to be mapped copy-on-write
	 * so we kill any attempt to do so here.
	 */
	
	if (UVM_ET_ISCOPYONWRITE(entry)) {
		uvmfault_unlockall(ufi, ufi->entry->aref.ar_amap, uobj, NULL);
		return(VM_PAGER_ERROR);
	}

	/*
	 * We could end up here as the result of a copyin(9) or
	 * copyout(9) while handling an ioctl.  So we must be careful
	 * not to deadlock.  Therefore we only block if the quiesce
	 * count is zero, which guarantees we didn't enter from within
	 * an ioctl code path.
	 */
	mtx_enter(&dev->quiesce_mtx);
	if (dev->quiesce && dev->quiesce_count == 0) {
		mtx_leave(&dev->quiesce_mtx);
		uvmfault_unlockall(ufi, ufi->entry->aref.ar_amap, uobj, NULL);
		mtx_enter(&dev->quiesce_mtx);
		while (dev->quiesce) {
			msleep(&dev->quiesce, &dev->quiesce_mtx,
			    PZERO, "drmflt", 0);
		}
		mtx_leave(&dev->quiesce_mtx);
		return(VM_PAGER_REFAULT);
	}
	dev->quiesce_count++;
	mtx_leave(&dev->quiesce_mtx);

	/* Call down into driver to do the magic */
	ret = dev->driver->gem_fault(obj, ufi, entry->offset + (vaddr -
	    entry->start), vaddr, pps, npages, centeridx,
	    access_type, flags);

	mtx_enter(&dev->quiesce_mtx);
	dev->quiesce_count--;
	if (dev->quiesce)
		wakeup(&dev->quiesce_count);
	mtx_leave(&dev->quiesce_mtx);

	return (ret);
}

boolean_t	
drm_flush(struct uvm_object *uobj, voff_t start, voff_t stop, int flags)
{
	return (TRUE);
}

struct uvm_object *
udv_attach_drm(dev_t device, vm_prot_t accessprot, voff_t off, vsize_t size)
{
	struct drm_device *dev = drm_get_device_from_kdev(device);
	struct drm_gem_object *obj;
	struct drm_vma_offset_node *node;
	struct drm_file *priv;
	struct file *filp;

	if (cdevsw[major(device)].d_mmap != drmmmap)
		return NULL;

	if (dev == NULL)
		return NULL;

	if (dev->driver->mmap)
		return dev->driver->mmap(dev, off, size);

	mutex_lock(&dev->struct_mutex);

	priv = drm_find_file_by_minor(dev, minor(device));
	if (priv == 0) {
		mutex_unlock(&dev->struct_mutex);
		return NULL;
	}
	filp = priv->filp;

	node = drm_vma_offset_exact_lookup(dev->vma_offset_manager,
					   off >> PAGE_SHIFT,
					   atop(round_page(size)));
	if (!node) {
		mutex_unlock(&dev->struct_mutex);
		return NULL;
	} else if (!drm_vma_node_is_allowed(node, filp)) {
		mutex_unlock(&dev->struct_mutex);
		return NULL;
	}

	obj = container_of(node, struct drm_gem_object, vma_node);
	drm_gem_object_reference(obj);

	mutex_unlock(&dev->struct_mutex);
	return &obj->uobj;
}

/** @file drm_gem.c
 *
 * This file provides some of the base ioctls and library routines for
 * the graphics memory manager implemented by each device driver.
 *
 * Because various devices have different requirements in terms of
 * synchronization and migration strategies, implementing that is left up to
 * the driver, and all that the general API provides should be generic --
 * allocating objects, reading/writing data with the cpu, freeing objects.
 * Even there, platform-dependent optimizations for reading/writing data with
 * the CPU mean we'll likely hook those out to driver-specific calls.  However,
 * the DRI2 implementation wants to have at least allocate/mmap be generic.
 *
 * The goal was to have swap-backed object allocation managed through
 * struct file.  However, file descriptors as handles to a struct file have
 * two major failings:
 * - Process limits prevent more than 1024 or so being used at a time by
 *   default.
 * - Inability to allocate high fds will aggravate the X Server's select()
 *   handling, and likely that of many GL client applications as well.
 *
 * This led to a plan of using our own integer IDs (called handles, following
 * DRM terminology) to mimic fds, and implement the fd syscalls we need as
 * ioctls.  The objects themselves will still include the struct file so
 * that we can transition to fds if the required kernel infrastructure shows
 * up at a later date, and as our interface with shmfs for memory allocation.
 */

/*
 * We make up offsets for buffer objects so we can recognize them at
 * mmap time.
 */

/* pgoff in mmap is an unsigned long, so we need to make sure that
 * the faked up offset will fit
 */

#if BITS_PER_LONG == 64
#define DRM_FILE_PAGE_OFFSET_START ((0xFFFFFFFFUL >> PAGE_SHIFT) + 1)
#define DRM_FILE_PAGE_OFFSET_SIZE ((0xFFFFFFFFUL >> PAGE_SHIFT) * 16)
#else
#define DRM_FILE_PAGE_OFFSET_START ((0xFFFFFFFUL >> PAGE_SHIFT) + 1)
#define DRM_FILE_PAGE_OFFSET_SIZE ((0xFFFFFFFUL >> PAGE_SHIFT) * 16)
#endif

/**
 * Initialize the GEM device fields
 */

int
drm_gem_init(struct drm_device *dev)
{
	struct drm_vma_offset_manager *vma_offset_manager;

	rw_init(&dev->object_name_lock, "drmonl");
	idr_init(&dev->object_name_idr);

	vma_offset_manager = kzalloc(sizeof(*vma_offset_manager), GFP_KERNEL);
	if (!vma_offset_manager) {
		DRM_ERROR("out of memory\n");
		return -ENOMEM;
	}

	dev->vma_offset_manager = vma_offset_manager;
	drm_vma_offset_manager_init(vma_offset_manager,
				    DRM_FILE_PAGE_OFFSET_START,
				    DRM_FILE_PAGE_OFFSET_SIZE);

	return 0;
}

void
drm_gem_destroy(struct drm_device *dev)
{

	drm_vma_offset_manager_destroy(dev->vma_offset_manager);
	kfree(dev->vma_offset_manager);
	dev->vma_offset_manager = NULL;
}

#ifdef __linux__

/**
 * Initialize an already allocated GEM object of the specified size with
 * shmfs backing store.
 */
int drm_gem_object_init(struct drm_device *dev,
			struct drm_gem_object *obj, size_t size)
{
	struct file *filp;

	drm_gem_private_object_init(dev, obj, size);

	filp = shmem_file_setup("drm mm object", size, VM_NORESERVE);
	if (IS_ERR(filp))
		return PTR_ERR(filp);

	obj->filp = filp;

	return 0;
}
EXPORT_SYMBOL(drm_gem_object_init);

#else

int drm_gem_object_init(struct drm_device *dev,
			struct drm_gem_object *obj, size_t size)
{
	drm_gem_private_object_init(dev, obj, size);

	obj->uao = uao_create(size, 0);
	uvm_objinit(&obj->uobj, &drm_pgops, 1);

	atomic_inc(&dev->obj_count);
	atomic_add(obj->size, &dev->obj_memory);

	obj->filp = (void *)obj->uao;

	return 0;
}

#endif

/**
 * Initialize an already allocated GEM object of the specified size with
 * no GEM provided backing store. Instead the caller is responsible for
 * backing the object and handling it.
 */
void drm_gem_private_object_init(struct drm_device *dev,
				 struct drm_gem_object *obj, size_t size)
{
	BUG_ON((size & (PAGE_SIZE - 1)) != 0);

	obj->dev = dev;
	obj->filp = NULL;

	kref_init(&obj->refcount);
	obj->handle_count = 0;
	obj->size = size;
	drm_vma_node_reset(&obj->vma_node);
}
EXPORT_SYMBOL(drm_gem_private_object_init);

static void
drm_gem_remove_prime_handles(struct drm_gem_object *obj, struct drm_file *filp)
{
#ifdef __linux__
	/*
	 * Note: obj->dma_buf can't disappear as long as we still hold a
	 * handle reference in obj->handle_count.
	 */
	mutex_lock(&filp->prime.lock);
	if (obj->dma_buf) {
		drm_prime_remove_buf_handle_locked(&filp->prime,
						   obj->dma_buf);
	}
	mutex_unlock(&filp->prime.lock);
#endif
}

/**
 * Called after the last handle to the object has been closed
 *
 * Removes any name for the object. Note that this must be
 * called before drm_gem_object_free or we'll be touching
 * freed memory
 */
static void drm_gem_object_handle_free(struct drm_gem_object *obj)
{
	struct drm_device *dev = obj->dev;

	/* Remove any name for this object */
	if (obj->name) {
		idr_remove(&dev->object_name_idr, obj->name);
		obj->name = 0;
	}
}

static void drm_gem_object_exported_dma_buf_free(struct drm_gem_object *obj)
{
#ifdef __linux__
	/* Unbreak the reference cycle if we have an exported dma_buf. */
	if (obj->dma_buf) {
		dma_buf_put(obj->dma_buf);
		obj->dma_buf = NULL;
	}
#endif
}

static void
drm_gem_object_handle_unreference_unlocked(struct drm_gem_object *obj)
{
	if (WARN_ON(obj->handle_count == 0))
		return;

	/*
	* Must bump handle count first as this may be the last
	* ref, in which case the object would disappear before we
	* checked for a name
	*/

	mutex_lock(&obj->dev->object_name_lock);
	if (--obj->handle_count == 0) {
		drm_gem_object_handle_free(obj);
		drm_gem_object_exported_dma_buf_free(obj);
	}
	mutex_unlock(&obj->dev->object_name_lock);

	drm_gem_object_unreference_unlocked(obj);
}

/**
 * Removes the mapping from handle to filp for this object.
 */
int
drm_gem_handle_delete(struct drm_file *filp, u32 handle)
{
	struct drm_device *dev;
	struct drm_gem_object *obj;

	/* This is gross. The idr system doesn't let us try a delete and
	 * return an error code.  It just spews if you fail at deleting.
	 * So, we have to grab a lock around finding the object and then
	 * doing the delete on it and dropping the refcount, or the user
	 * could race us to double-decrement the refcount and cause a
	 * use-after-free later.  Given the frequency of our handle lookups,
	 * we may want to use ida for number allocation and a hash table
	 * for the pointers, anyway.
	 */
	spin_lock(&filp->table_lock);

	/* Check if we currently have a reference on the object */
	obj = idr_find(&filp->object_idr, handle);
	if (obj == NULL) {
		spin_unlock(&filp->table_lock);
		return -EINVAL;
	}
	dev = obj->dev;

	/* Release reference and decrement refcount. */
	idr_remove(&filp->object_idr, handle);
	spin_unlock(&filp->table_lock);

	if (drm_core_check_feature(dev, DRIVER_PRIME))
		drm_gem_remove_prime_handles(obj, filp);
	drm_vma_node_revoke(&obj->vma_node, filp->filp);

	if (dev->driver->gem_close_object)
		dev->driver->gem_close_object(obj, filp);
	drm_gem_object_handle_unreference_unlocked(obj);

	return 0;
}
EXPORT_SYMBOL(drm_gem_handle_delete);

/**
 * drm_gem_dumb_destroy - dumb fb callback helper for gem based drivers
 * 
 * This implements the ->dumb_destroy kms driver callback for drivers which use
 * gem to manage their backing storage.
 */
int drm_gem_dumb_destroy(struct drm_file *file,
			 struct drm_device *dev,
			 uint32_t handle)
{
	return drm_gem_handle_delete(file, handle);
}
EXPORT_SYMBOL(drm_gem_dumb_destroy);

/**
 * drm_gem_handle_create_tail - internal functions to create a handle
 * 
 * This expects the dev->object_name_lock to be held already and will drop it
 * before returning. Used to avoid races in establishing new handles when
 * importing an object from either an flink name or a dma-buf.
 */
int
drm_gem_handle_create_tail(struct drm_file *file_priv,
			   struct drm_gem_object *obj,
			   u32 *handlep)
{
	struct drm_device *dev = obj->dev;
	int ret;

	WARN_ON(!mutex_is_locked(&dev->object_name_lock));

	/*
	 * Get the user-visible handle using idr.  Preload and perform
	 * allocation under our spinlock.
	 */
	idr_preload(GFP_KERNEL);
	spin_lock(&file_priv->table_lock);

	ret = idr_alloc(&file_priv->object_idr, obj, 1, 0, GFP_NOWAIT);
	drm_gem_object_reference(obj);
	obj->handle_count++;
	spin_unlock(&file_priv->table_lock);
	idr_preload_end();
	mutex_unlock(&dev->object_name_lock);
	if (ret < 0)
		goto err_unref;

	*handlep = ret;

	ret = drm_vma_node_allow(&obj->vma_node, file_priv->filp);
	if (ret)
		goto err_remove;

	if (dev->driver->gem_open_object) {
		ret = dev->driver->gem_open_object(obj, file_priv);
		if (ret)
			goto err_revoke;
	}

	return 0;

err_revoke:
	drm_vma_node_revoke(&obj->vma_node, file_priv->filp);
err_remove:
	spin_lock(&file_priv->table_lock);
	idr_remove(&file_priv->object_idr, *handlep);
	spin_unlock(&file_priv->table_lock);
err_unref:
	drm_gem_object_handle_unreference_unlocked(obj);
	return ret;
}

/**
 * Create a handle for this object. This adds a handle reference
 * to the object, which includes a regular reference count. Callers
 * will likely want to dereference the object afterwards.
 */
int
drm_gem_handle_create(struct drm_file *file_priv,
		       struct drm_gem_object *obj,
		       u32 *handlep)
{
	mutex_lock(&obj->dev->object_name_lock);

	return drm_gem_handle_create_tail(file_priv, obj, handlep);
}
EXPORT_SYMBOL(drm_gem_handle_create);


/**
 * drm_gem_free_mmap_offset - release a fake mmap offset for an object
 * @obj: obj in question
 *
 * This routine frees fake offsets allocated by drm_gem_create_mmap_offset().
 */
void
drm_gem_free_mmap_offset(struct drm_gem_object *obj)
{
	struct drm_device *dev = obj->dev;

	drm_vma_offset_remove(dev->vma_offset_manager, &obj->vma_node);
}
EXPORT_SYMBOL(drm_gem_free_mmap_offset);

/**
 * drm_gem_create_mmap_offset_size - create a fake mmap offset for an object
 * @obj: obj in question
 * @size: the virtual size
 *
 * GEM memory mapping works by handing back to userspace a fake mmap offset
 * it can use in a subsequent mmap(2) call.  The DRM core code then looks
 * up the object based on the offset and sets up the various memory mapping
 * structures.
 *
 * This routine allocates and attaches a fake offset for @obj, in cases where
 * the virtual size differs from the physical size (ie. obj->size).  Otherwise
 * just use drm_gem_create_mmap_offset().
 */
int
drm_gem_create_mmap_offset_size(struct drm_gem_object *obj, size_t size)
{
	struct drm_device *dev = obj->dev;

	return drm_vma_offset_add(dev->vma_offset_manager, &obj->vma_node,
				  size / PAGE_SIZE);
}
EXPORT_SYMBOL(drm_gem_create_mmap_offset_size);

/**
 * drm_gem_create_mmap_offset - create a fake mmap offset for an object
 * @obj: obj in question
 *
 * GEM memory mapping works by handing back to userspace a fake mmap offset
 * it can use in a subsequent mmap(2) call.  The DRM core code then looks
 * up the object based on the offset and sets up the various memory mapping
 * structures.
 *
 * This routine allocates and attaches a fake offset for @obj.
 */
int drm_gem_create_mmap_offset(struct drm_gem_object *obj)
{
	return drm_gem_create_mmap_offset_size(obj, obj->size);
}
EXPORT_SYMBOL(drm_gem_create_mmap_offset);

#ifdef __linux__

/**
 * drm_gem_get_pages - helper to allocate backing pages for a GEM object
 * from shmem
 * @obj: obj in question
 * @gfpmask: gfp mask of requested pages
 */
struct page **drm_gem_get_pages(struct drm_gem_object *obj, gfp_t gfpmask)
{
	struct inode *inode;
	struct address_space *mapping;
	struct page *p, **pages;
	int i, npages;

	/* This is the shared memory object that backs the GEM resource */
	inode = file_inode(obj->filp);
	mapping = inode->i_mapping;

	/* We already BUG_ON() for non-page-aligned sizes in
	 * drm_gem_object_init(), so we should never hit this unless
	 * driver author is doing something really wrong:
	 */
	WARN_ON((obj->size & (PAGE_SIZE - 1)) != 0);

	npages = obj->size >> PAGE_SHIFT;

	pages = drm_malloc_ab(npages, sizeof(struct page *));
	if (pages == NULL)
		return ERR_PTR(-ENOMEM);

	gfpmask |= mapping_gfp_mask(mapping);

	for (i = 0; i < npages; i++) {
		p = shmem_read_mapping_page_gfp(mapping, i, gfpmask);
		if (IS_ERR(p))
			goto fail;
		pages[i] = p;

		/* There is a hypothetical issue w/ drivers that require
		 * buffer memory in the low 4GB.. if the pages are un-
		 * pinned, and swapped out, they can end up swapped back
		 * in above 4GB.  If pages are already in memory, then
		 * shmem_read_mapping_page_gfp will ignore the gfpmask,
		 * even if the already in-memory page disobeys the mask.
		 *
		 * It is only a theoretical issue today, because none of
		 * the devices with this limitation can be populated with
		 * enough memory to trigger the issue.  But this BUG_ON()
		 * is here as a reminder in case the problem with
		 * shmem_read_mapping_page_gfp() isn't solved by the time
		 * it does become a real issue.
		 *
		 * See this thread: http://lkml.org/lkml/2011/7/11/238
		 */
		BUG_ON((gfpmask & __GFP_DMA32) &&
				(page_to_pfn(p) >= 0x00100000UL));
	}

	return pages;

fail:
	while (i--)
		page_cache_release(pages[i]);

	drm_free_large(pages);
	return ERR_CAST(p);
}
EXPORT_SYMBOL(drm_gem_get_pages);

/**
 * drm_gem_put_pages - helper to free backing pages for a GEM object
 * @obj: obj in question
 * @pages: pages to free
 * @dirty: if true, pages will be marked as dirty
 * @accessed: if true, the pages will be marked as accessed
 */
void drm_gem_put_pages(struct drm_gem_object *obj, struct page **pages,
		bool dirty, bool accessed)
{
	int i, npages;

	/* We already BUG_ON() for non-page-aligned sizes in
	 * drm_gem_object_init(), so we should never hit this unless
	 * driver author is doing something really wrong:
	 */
	WARN_ON((obj->size & (PAGE_SIZE - 1)) != 0);

	npages = obj->size >> PAGE_SHIFT;

	for (i = 0; i < npages; i++) {
		if (dirty)
			set_page_dirty(pages[i]);

		if (accessed)
			mark_page_accessed(pages[i]);

		/* Undo the reference we took when populating the table */
		page_cache_release(pages[i]);
	}

	drm_free_large(pages);
}
EXPORT_SYMBOL(drm_gem_put_pages);

#endif

/** Returns a reference to the object named by the handle. */
struct drm_gem_object *
drm_gem_object_lookup(struct drm_device *dev, struct drm_file *filp,
		      u32 handle)
{
	struct drm_gem_object *obj;

	spin_lock(&filp->table_lock);

	/* Check if we currently have a reference on the object */
	obj = idr_find(&filp->object_idr, handle);
	if (obj == NULL) {
		spin_unlock(&filp->table_lock);
		return NULL;
	}

	drm_gem_object_reference(obj);

	spin_unlock(&filp->table_lock);

	return obj;
}
EXPORT_SYMBOL(drm_gem_object_lookup);

/**
 * Releases the handle to an mm object.
 */
int
drm_gem_close_ioctl(struct drm_device *dev, void *data,
		    struct drm_file *file_priv)
{
	struct drm_gem_close *args = data;
	int ret;

	if (!(dev->driver->driver_features & DRIVER_GEM))
		return -ENODEV;

	ret = drm_gem_handle_delete(file_priv, args->handle);

	return ret;
}

/**
 * Create a global name for an object, returning the name.
 *
 * Note that the name does not hold a reference; when the object
 * is freed, the name goes away.
 */
int
drm_gem_flink_ioctl(struct drm_device *dev, void *data,
		    struct drm_file *file_priv)
{
	struct drm_gem_flink *args = data;
	struct drm_gem_object *obj;
	int ret;

	if (!(dev->driver->driver_features & DRIVER_GEM))
		return -ENODEV;

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);
	if (obj == NULL)
		return -ENOENT;

	mutex_lock(&dev->object_name_lock);
	idr_preload(GFP_KERNEL);
	/* prevent races with concurrent gem_close. */
	if (obj->handle_count == 0) {
		ret = -ENOENT;
		goto err;
	}

	if (!obj->name) {
		ret = idr_alloc(&dev->object_name_idr, obj, 1, 0, GFP_NOWAIT);
		if (ret < 0)
			goto err;

		obj->name = ret;
	}

	args->name = (uint64_t) obj->name;
	ret = 0;

err:
	idr_preload_end();
	mutex_unlock(&dev->object_name_lock);
	drm_gem_object_unreference_unlocked(obj);
	return ret;
}

/**
 * Open an object using the global name, returning a handle and the size.
 *
 * This handle (of course) holds a reference to the object, so the object
 * will not go away until the handle is deleted.
 */
int
drm_gem_open_ioctl(struct drm_device *dev, void *data,
		   struct drm_file *file_priv)
{
	struct drm_gem_open *args = data;
	struct drm_gem_object *obj;
	int ret;
	u32 handle;

	if (!(dev->driver->driver_features & DRIVER_GEM))
		return -ENODEV;

	mutex_lock(&dev->object_name_lock);
	obj = idr_find(&dev->object_name_idr, (int) args->name);
	if (obj) {
		drm_gem_object_reference(obj);
	} else {
		mutex_unlock(&dev->object_name_lock);
		return -ENOENT;
	}

	/* drm_gem_handle_create_tail unlocks dev->object_name_lock. */
	ret = drm_gem_handle_create_tail(file_priv, obj, &handle);
	drm_gem_object_unreference_unlocked(obj);
	if (ret)
		return ret;

	args->handle = handle;
	args->size = obj->size;

	return 0;
}

/**
 * Called at device open time, sets up the structure for handling refcounting
 * of mm objects.
 */
void
drm_gem_open(struct drm_device *dev, struct drm_file *file_private)
{
	idr_init(&file_private->object_idr);
	mtx_init(&file_private->table_lock, IPL_NONE);
}

/**
 * Called at device close to release the file's
 * handle references on objects.
 */
static int
drm_gem_object_release_handle(int id, void *ptr, void *data)
{
	struct drm_file *file_priv = data;
	struct drm_gem_object *obj = ptr;
	struct drm_device *dev = obj->dev;

	if (dev->driver->gem_close_object)
		dev->driver->gem_close_object(obj, file_priv);

	if (drm_core_check_feature(dev, DRIVER_PRIME))
		drm_gem_remove_prime_handles(obj, file_priv);
	drm_vma_node_revoke(&obj->vma_node, file_priv->filp);

	drm_gem_object_handle_unreference_unlocked(obj);

	return 0;
}

/**
 * Called at close time when the filp is going away.
 *
 * Releases any remaining references on objects by this filp.
 */
void
drm_gem_release(struct drm_device *dev, struct drm_file *file_private)
{
	idr_for_each(&file_private->object_idr,
		     &drm_gem_object_release_handle, file_private);
	idr_destroy(&file_private->object_idr);
}

#ifdef __linux__

void
drm_gem_object_release(struct drm_gem_object *obj)
{
	WARN_ON(obj->dma_buf);

	if (obj->filp)
	    fput(obj->filp);
}
EXPORT_SYMBOL(drm_gem_object_release);

#else

void
drm_gem_object_release(struct drm_gem_object *obj)
{
	struct drm_device *dev = obj->dev;

	if (obj->uao)
		uao_detach(obj->uao);

	atomic_dec(&dev->obj_count);
	atomic_sub(obj->size, &dev->obj_memory);
}

#endif

/**
 * Called after the last reference to the object has been lost.
 * Must be called holding struct_ mutex
 *
 * Frees the object
 */
void
drm_gem_object_free(struct kref *kref)
{
	struct drm_gem_object *obj = (struct drm_gem_object *) kref;
	struct drm_device *dev = obj->dev;

	BUG_ON(!mutex_is_locked(&dev->struct_mutex));

	if (dev->driver->gem_free_object != NULL)
		dev->driver->gem_free_object(obj);
}
EXPORT_SYMBOL(drm_gem_object_free);

#ifdef __linux__

void drm_gem_vm_open(struct vm_area_struct *vma)
{
	struct drm_gem_object *obj = vma->vm_private_data;

	drm_gem_object_reference(obj);

	mutex_lock(&obj->dev->struct_mutex);
	drm_vm_open_locked(obj->dev, vma);
	mutex_unlock(&obj->dev->struct_mutex);
}
EXPORT_SYMBOL(drm_gem_vm_open);

void drm_gem_vm_close(struct vm_area_struct *vma)
{
	struct drm_gem_object *obj = vma->vm_private_data;
	struct drm_device *dev = obj->dev;

	mutex_lock(&dev->struct_mutex);
	drm_vm_close_locked(obj->dev, vma);
	drm_gem_object_unreference(obj);
	mutex_unlock(&dev->struct_mutex);
}
EXPORT_SYMBOL(drm_gem_vm_close);

/**
 * drm_gem_mmap_obj - memory map a GEM object
 * @obj: the GEM object to map
 * @obj_size: the object size to be mapped, in bytes
 * @vma: VMA for the area to be mapped
 *
 * Set up the VMA to prepare mapping of the GEM object using the gem_vm_ops
 * provided by the driver. Depending on their requirements, drivers can either
 * provide a fault handler in their gem_vm_ops (in which case any accesses to
 * the object will be trapped, to perform migration, GTT binding, surface
 * register allocation, or performance monitoring), or mmap the buffer memory
 * synchronously after calling drm_gem_mmap_obj.
 *
 * This function is mainly intended to implement the DMABUF mmap operation, when
 * the GEM object is not looked up based on its fake offset. To implement the
 * DRM mmap operation, drivers should use the drm_gem_mmap() function.
 *
 * drm_gem_mmap_obj() assumes the user is granted access to the buffer while
 * drm_gem_mmap() prevents unprivileged users from mapping random objects. So
 * callers must verify access restrictions before calling this helper.
 *
 * NOTE: This function has to be protected with dev->struct_mutex
 *
 * Return 0 or success or -EINVAL if the object size is smaller than the VMA
 * size, or if no gem_vm_ops are provided.
 */
int drm_gem_mmap_obj(struct drm_gem_object *obj, unsigned long obj_size,
		     struct vm_area_struct *vma)
{
	struct drm_device *dev = obj->dev;

	lockdep_assert_held(&dev->struct_mutex);

	/* Check for valid size. */
	if (obj_size < vma->vm_end - vma->vm_start)
		return -EINVAL;

	if (!dev->driver->gem_vm_ops)
		return -EINVAL;

	vma->vm_flags |= VM_IO | VM_PFNMAP | VM_DONTEXPAND | VM_DONTDUMP;
	vma->vm_ops = dev->driver->gem_vm_ops;
	vma->vm_private_data = obj;
	vma->vm_page_prot =  pgprot_writecombine(vm_get_page_prot(vma->vm_flags));

	/* Take a ref for this mapping of the object, so that the fault
	 * handler can dereference the mmap offset's pointer to the object.
	 * This reference is cleaned up by the corresponding vm_close
	 * (which should happen whether the vma was created by this call, or
	 * by a vm_open due to mremap or partial unmap or whatever).
	 */
	drm_gem_object_reference(obj);

	drm_vm_open_locked(dev, vma);
	return 0;
}
EXPORT_SYMBOL(drm_gem_mmap_obj);

/**
 * drm_gem_mmap - memory map routine for GEM objects
 * @filp: DRM file pointer
 * @vma: VMA for the area to be mapped
 *
 * If a driver supports GEM object mapping, mmap calls on the DRM file
 * descriptor will end up here.
 *
 * Look up the GEM object based on the offset passed in (vma->vm_pgoff will
 * contain the fake offset we created when the GTT map ioctl was called on
 * the object) and map it with a call to drm_gem_mmap_obj().
 *
 * If the caller is not granted access to the buffer object, the mmap will fail
 * with EACCES. Please see the vma manager for more information.
 */
int drm_gem_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct drm_file *priv = filp->private_data;
	struct drm_device *dev = priv->minor->dev;
	struct drm_gem_object *obj;
	struct drm_vma_offset_node *node;
	int ret = 0;

	if (drm_device_is_unplugged(dev))
		return -ENODEV;

	mutex_lock(&dev->struct_mutex);

	node = drm_vma_offset_exact_lookup(dev->vma_offset_manager,
					   vma->vm_pgoff,
					   vma_pages(vma));
	if (!node) {
		mutex_unlock(&dev->struct_mutex);
		return drm_mmap(filp, vma);
	} else if (!drm_vma_node_is_allowed(node, filp)) {
		mutex_unlock(&dev->struct_mutex);
		return -EACCES;
	}

	obj = container_of(node, struct drm_gem_object, vma_node);
	ret = drm_gem_mmap_obj(obj, drm_vma_node_size(node) << PAGE_SHIFT, vma);

	mutex_unlock(&dev->struct_mutex);

	return ret;
}
EXPORT_SYMBOL(drm_gem_mmap);

#endif
