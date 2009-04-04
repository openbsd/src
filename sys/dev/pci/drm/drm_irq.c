/*-
 * Copyright 2003 Eric Anholt
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
 * ERIC ANHOLT BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Eric Anholt <anholt@FreeBSD.org>
 *
 */

/** @file drm_irq.c
 * Support code for handling setup/teardown of interrupt handlers and
 * handing interrupt handlers off to the drivers.
 */

#include <sys/workq.h>

#include "drmP.h"
#include "drm.h"

void		drm_update_vblank_count(struct drm_device *, int);
void		vblank_disable(void *);

int
drm_irq_by_busid(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_irq_busid	*irq = data;

	/*
	 * This is only ever called by root as part of a stupid interface.
	 * just hand over the irq without checking the busid. If all clients
	 * can be forced to use interface 1.2 then this can die.
	 */
	irq->irq = dev->irq;

	DRM_DEBUG("%d:%d:%d => IRQ %d\n", irq->busnum, irq->devnum,
	    irq->funcnum, irq->irq);

	return 0;
}

int
drm_irq_install(struct drm_device *dev)
{
	int	ret;

	if (dev->irq == 0 || dev->dev_private == NULL)
		return (EINVAL);

	DRM_DEBUG("irq=%d\n", dev->irq);

	DRM_LOCK();
	if (dev->irq_enabled) {
		DRM_UNLOCK();
		return (EBUSY);
	}
	dev->irq_enabled = 1;
	DRM_UNLOCK();

	if ((ret = dev->driver->irq_install(dev)) != 0)
		goto err;

	return (0);
err:
	DRM_LOCK();
	dev->irq_enabled = 0;
	DRM_UNLOCK();
	return (ret);
}

int
drm_irq_uninstall(struct drm_device *dev)
{
	int i;

	DRM_LOCK();
	if (!dev->irq_enabled) {
		DRM_UNLOCK();
		return (EINVAL);
	}

	dev->irq_enabled = 0;
	DRM_UNLOCK();

	/*
	 * Ick. we're about to turn of vblanks, so make sure anyone waiting
	 * on them gets woken up. Also make sure we update state correctly
	 * so that we can continue refcounting correctly.
	 */
	mtx_enter(&dev->vbl_lock);
	for (i = 0; i < dev->num_crtcs; i++) {
		wakeup(&dev->vblank[i]);
		dev->vblank[i].vbl_enabled = 0;
		dev->vblank[i].last_vblank =
		    dev->driver->get_vblank_counter(dev, i);
	}
	mtx_leave(&dev->vbl_lock);

	DRM_DEBUG("irq=%d\n", dev->irq);

	dev->driver->irq_uninstall(dev);

	return (0);
}

int
drm_control(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_control	*ctl = data;

	/* Handle drivers who used to require IRQ setup no longer does. */
	if (!(dev->driver->flags & DRIVER_IRQ))
		return (0);

	switch (ctl->func) {
	case DRM_INST_HANDLER:
		if (dev->if_version < DRM_IF_VERSION(1, 2) &&
		    ctl->irq != dev->irq)
			return (EINVAL);
		return (drm_irq_install(dev));
	case DRM_UNINST_HANDLER:
		return (drm_irq_uninstall(dev));
	default:
		return (EINVAL);
	}
}

void
vblank_disable(void *arg)
{
	struct drm_device *dev = (struct drm_device*)arg;
	int i;

	mtx_enter(&dev->vbl_lock);
	if (!dev->vblank_disable_allowed)
		goto out;

	for (i=0; i < dev->num_crtcs; i++){
		if (atomic_read(&dev->vblank[i].vbl_refcount) == 0 &&
		    dev->vblank[i].vbl_enabled) {
			dev->vblank[i].last_vblank =
			    dev->driver->get_vblank_counter(dev, i);
			dev->driver->disable_vblank(dev, i);
			dev->vblank[i].vbl_enabled = 0;
		}
	}
out:
	mtx_leave(&dev->vbl_lock);
}

void
drm_vblank_cleanup(struct drm_device *dev)
{
	if (dev->num_crtcs == 0)
		return; /* not initialised */

	timeout_del(&dev->vblank_disable_timer);

	vblank_disable(dev);

	drm_free(dev->vblank);

	dev->vblank = NULL;
	dev->num_crtcs = 0;
}

int
drm_vblank_init(struct drm_device *dev, int num_crtcs)
{
	timeout_set(&dev->vblank_disable_timer, vblank_disable, dev);
	mtx_init(&dev->vbl_lock, IPL_BIO);
	dev->num_crtcs = num_crtcs;

	dev->vblank = drm_calloc(num_crtcs, sizeof(*dev->vblank));
	if (dev->vblank == NULL)
		goto err;

	dev->vblank_disable_allowed = 0;

	return (0);

err:
	drm_vblank_cleanup(dev);
	return ENOMEM;
}

u_int32_t
drm_vblank_count(struct drm_device *dev, int crtc)
{
	return atomic_read(&dev->vblank[crtc].vbl_count);
}

void
drm_update_vblank_count(struct drm_device *dev, int crtc)
{
	u_int32_t cur_vblank, diff;

	/*
	 * Interrupt was disabled prior to this call, so deal with counter wrap
	 * note that we may have lost a full dev->max_vblank_count events if
	 * the register is small or the interrupts were off for a long time.
	 */
	cur_vblank = dev->driver->get_vblank_counter(dev, crtc);
	diff = cur_vblank - dev->vblank[crtc].last_vblank;
	if (cur_vblank < dev->vblank[crtc].last_vblank)
		diff += dev->max_vblank_count;

	atomic_add(diff, &dev->vblank[crtc].vbl_count);
}

int
drm_vblank_get(struct drm_device *dev, int crtc)
{
	int ret = 0;

	if (dev->irq_enabled == 0)
		return (EINVAL);

	mtx_enter(&dev->vbl_lock);
	atomic_add(1, &dev->vblank[crtc].vbl_refcount);
	if (dev->vblank[crtc].vbl_refcount == 1 &&
	    dev->vblank[crtc].vbl_enabled == 0) {
		if ((ret = dev->driver->enable_vblank(dev, crtc)) == 0) {
			dev->vblank[crtc].vbl_enabled = 1;
			drm_update_vblank_count(dev, crtc);
		} else {
			atomic_dec(&dev->vblank[crtc].vbl_refcount);
		}

	}
	mtx_leave(&dev->vbl_lock);

	return (ret);
}

void
drm_vblank_put(struct drm_device *dev, int crtc)
{
	mtx_enter(&dev->vbl_lock);
	/* Last user schedules interrupt disable */
	atomic_dec(&dev->vblank[crtc].vbl_refcount);
	if (dev->vblank[crtc].vbl_refcount == 0) 
		timeout_add_sec(&dev->vblank_disable_timer, 5);
	mtx_leave(&dev->vbl_lock);
}

int
drm_modeset_ctl(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_modeset_ctl *modeset = data;
	int crtc, ret = 0;

	/* not initialised yet, just noop */
	if (dev->num_crtcs == 0)
		goto out;

	crtc = modeset->crtc;
	if (crtc >= dev->num_crtcs) {
		ret = EINVAL;
		goto out;
	}

	/*
	 * If interrupts are enabled/disabled between calls to this ioctl then
	 * it can get nasty. So just grab a reference so that the interrupts
	 * keep going through the modeset
	 */
	switch (modeset->cmd) {
	case _DRM_PRE_MODESET:
		if (dev->vblank[crtc].vbl_inmodeset == 0) {
			mtx_enter(&dev->vbl_lock);
			dev->vblank[crtc].vbl_inmodeset = 0x1;
			mtx_leave(&dev->vbl_lock);
			if (drm_vblank_get(dev, crtc) == 0)
				dev->vblank[crtc].vbl_inmodeset |= 0x2;
		}
		break;
	case _DRM_POST_MODESET:
		if (dev->vblank[crtc].vbl_inmodeset) {
			mtx_enter(&dev->vbl_lock);
			dev->vblank_disable_allowed = 1;
			mtx_leave(&dev->vbl_lock);
			if (dev->vblank[crtc].vbl_inmodeset & 0x2)
				drm_vblank_put(dev, crtc);
			dev->vblank[crtc].vbl_inmodeset = 0;
		}
		break;
	default:
		ret = EINVAL;
		break;
	}

out:
	return (ret);
}

int
drm_wait_vblank(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	union drm_wait_vblank	*vblwait = data;
	int			 ret, flags, crtc, seq;

	if (!dev->irq_enabled)
		return EINVAL;

	flags = vblwait->request.type & _DRM_VBLANK_FLAGS_MASK;
	crtc = flags & _DRM_VBLANK_SECONDARY ? 1 : 0;

	if (crtc >= dev->num_crtcs)
		return EINVAL;

	ret = drm_vblank_get(dev, crtc);
	if (ret)
		return (ret);
	seq = drm_vblank_count(dev,crtc);

	if (vblwait->request.type & _DRM_VBLANK_RELATIVE) {
		vblwait->request.sequence += seq;
		vblwait->request.type &= ~_DRM_VBLANK_RELATIVE;
	}

	flags = vblwait->request.type & _DRM_VBLANK_FLAGS_MASK;
	if ((flags & _DRM_VBLANK_NEXTONMISS) &&
	    (seq - vblwait->request.sequence) <= (1<<23)) {
		vblwait->request.sequence = seq + 1;
	}

	if (flags & _DRM_VBLANK_SIGNAL) {
		ret = EINVAL;
	} else {
		DRM_WAIT_ON(ret, &dev->vblank[crtc], &dev->vbl_lock, 3 * hz,
		    "drmvblq", ((drm_vblank_count(dev, crtc) -
		    vblwait->request.sequence) <= (1 << 23)) ||
		    dev->irq_enabled == 0);

		if (ret != EINTR) {
			struct timeval now;

			microtime(&now);
			vblwait->reply.tval_sec = now.tv_sec;
			vblwait->reply.tval_usec = now.tv_usec;
			vblwait->reply.sequence = drm_vblank_count(dev, crtc);
		}
	}

	drm_vblank_put(dev, crtc);
	return (ret);
}

void
drm_handle_vblank(struct drm_device *dev, int crtc)
{
	atomic_inc(&dev->vblank[crtc].vbl_count);
	wakeup(&dev->vblank[crtc]);
}
