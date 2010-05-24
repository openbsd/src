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
 * Authors:
 *    Rickard E. (Rik) Faith <faith@valinux.com>
 *    Gareth Hughes <gareth@valinux.com>
 *
 */

/** @file drm_lock.c
 * Implementation of the ioctls and other support code for dealing with the
 * hardware lock.
 *
 * The DRM hardware lock is a shared structure between the kernel and userland.
 *
 * On uncontended access where the new context was the last context, the
 * client may take the lock without dropping down into the kernel, using atomic
 * compare-and-set.
 *
 * If the client finds during compare-and-set that it was not the last owner
 * of the lock, it calls the DRM lock ioctl, which may sleep waiting for the
 * lock, and may have side-effects of kernel-managed context switching.
 *
 * When the client releases the lock, if the lock is marked as being contended
 * by another client, then the DRM unlock ioctl is called so that the
 * contending client may be woken up.
 */

#include "drmP.h"

int
drm_lock_take(struct drm_lock_data *lock_data, unsigned int context)
{
	volatile unsigned int	*lock = &lock_data->hw_lock->lock;
	unsigned int		 old, new;

	do {
		old = *lock;
		if (old & _DRM_LOCK_HELD)
			new = old | _DRM_LOCK_CONT;
		else
			new = context | _DRM_LOCK_HELD;
	} while (!atomic_cmpset_int(lock, old, new));

	if (_DRM_LOCKING_CONTEXT(old) == context && _DRM_LOCK_IS_HELD(old)) {
		if (context != DRM_KERNEL_CONTEXT)
			DRM_ERROR("%d holds heavyweight lock\n", context);
		return (0);
	}
	/* If the lock wasn't held before, it's ours */
	return (!_DRM_LOCK_IS_HELD(old));
}

int
drm_lock_free(struct drm_lock_data *lock_data, unsigned int context)
{
	volatile unsigned int	*lock = &lock_data->hw_lock->lock;
	unsigned int		 old, new;

	mtx_enter(&lock_data->spinlock);
	lock_data->file_priv = NULL;
	do {
		old  = *lock;
		new  = 0;
	} while (!atomic_cmpset_int(lock, old, new));
	mtx_leave(&lock_data->spinlock);

	if (_DRM_LOCK_IS_HELD(old) && _DRM_LOCKING_CONTEXT(old) != context) {
		DRM_ERROR("%d freed heavyweight lock held by %d\n",
			  context, _DRM_LOCKING_CONTEXT(old));
		return 1;
	}
	wakeup(lock_data);
	return 0;
}

int
drm_lock(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
        struct drm_lock	*lock = data;
        int		 ret = 0;

        if (lock->context == DRM_KERNEL_CONTEXT) {
                DRM_ERROR("Process %d using kernel context %d\n",
		    DRM_CURRENTPID, lock->context);
                return EINVAL;
        }

        DRM_DEBUG("%d (pid %d) requests lock (0x%08x), flags = 0x%08x\n",
	    lock->context, DRM_CURRENTPID, dev->lock.hw_lock->lock,
	    lock->flags);

	mtx_enter(&dev->lock.spinlock);
	for (;;) {
		if (drm_lock_take(&dev->lock, lock->context)) {
			dev->lock.file_priv = file_priv;
			break;  /* Got lock */
		}

		/* Contention */
		ret = msleep(&dev->lock, &dev->lock.spinlock,
		    PZERO | PCATCH, "drmlkq", 0);
		if (ret != 0)
			break;
	}
	mtx_leave(&dev->lock.spinlock);
	DRM_DEBUG("%d %s\n", lock->context, ret ? "interrupted" : "has lock");

	if (ret != 0)
		return ret;

	/* XXX: Add signal blocking here */

	return 0;
}

int
drm_unlock(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_lock	*lock = data;

	if (lock->context == DRM_KERNEL_CONTEXT) {
		DRM_ERROR("Process %d using kernel context %d\n",
		    DRM_CURRENTPID, lock->context);
		return EINVAL;
	}
	/* Check that the context unlock being requested actually matches
	 * who currently holds the lock.
	 */
	if (!_DRM_LOCK_IS_HELD(dev->lock.hw_lock->lock) ||
	    _DRM_LOCKING_CONTEXT(dev->lock.hw_lock->lock) != lock->context)
		return EINVAL;

	if (drm_lock_free(&dev->lock, lock->context)) {
		DRM_ERROR("\n");
	}

	return 0;
}
