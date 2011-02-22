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
int		drm_queue_vblank_event(struct drm_device *, int,
		    union drm_wait_vblank *, struct drm_file *);
void		drm_handle_vblank_events(struct drm_device *, int);

#ifdef DRM_VBLANK_DEBUG
#define DPRINTF(x...)	do { printf(x); } while(/* CONSTCOND */ 0)
#else
#define DPRINTF(x...)	do { } while(/* CONSTCOND */ 0)
#endif

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
	if (dev->vblank != NULL) {
		mtx_enter(&dev->vblank->vb_lock);
		for (i = 0; i < dev->vblank->vb_num; i++) {
			wakeup(&dev->vblank->vb_crtcs[i]);
			dev->vblank->vb_crtcs[i].vbl_enabled = 0;
			dev->vblank->vb_crtcs[i].vbl_last =
			    dev->driver->get_vblank_counter(dev, i);
		}
		mtx_leave(&dev->vblank->vb_lock);
	}

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
	struct drm_device	*dev = (struct drm_device*)arg;
	struct drm_vblank_info	*vbl = dev->vblank;
	struct drm_vblank	*crtc;
	int			 i;

	mtx_enter(&vbl->vb_lock);
	for (i = 0; i < vbl->vb_num; i++) {
		crtc = &vbl->vb_crtcs[i];

		if (crtc->vbl_refs == 0 && crtc->vbl_enabled) {
			DPRINTF("%s: disabling crtc %d\n", __func__, i);
			crtc->vbl_last =
			    dev->driver->get_vblank_counter(dev, i);
			dev->driver->disable_vblank(dev, i);
			crtc->vbl_enabled = 0;
		}
	}
	mtx_leave(&vbl->vb_lock);
}

void
drm_vblank_cleanup(struct drm_device *dev)
{
	if (dev->vblank == NULL)
		return; /* not initialised */

	timeout_del(&dev->vblank->vb_disable_timer);

	vblank_disable(dev);

	drm_free(dev->vblank);
	dev->vblank = NULL;
}

int
drm_vblank_init(struct drm_device *dev, int num_crtcs)
{
	int	i;

	dev->vblank = malloc(sizeof(*dev->vblank) + (num_crtcs *
	    sizeof(struct drm_vblank)), M_DRM,  M_WAITOK | M_CANFAIL | M_ZERO);
	if (dev->vblank == NULL)
		return (ENOMEM);

	dev->vblank->vb_num = num_crtcs;
	mtx_init(&dev->vblank->vb_lock, IPL_TTY);
	timeout_set(&dev->vblank->vb_disable_timer, vblank_disable, dev);
	for (i = 0; i < num_crtcs; i++)
		TAILQ_INIT(&dev->vblank->vb_crtcs[i].vbl_events);

	return (0);
}

u_int32_t
drm_vblank_count(struct drm_device *dev, int crtc)
{
	return (dev->vblank->vb_crtcs[crtc].vbl_count);
}

void
drm_update_vblank_count(struct drm_device *dev, int crtc)
{
	u_int32_t	cur_vblank, diff;

	/*
	 * Interrupt was disabled prior to this call, so deal with counter wrap
	 * note that we may have lost a full vb_max events if
	 * the register is small or the interrupts were off for a long time.
	 */
	cur_vblank = dev->driver->get_vblank_counter(dev, crtc);
	diff = cur_vblank - dev->vblank->vb_crtcs[crtc].vbl_last;
	if (cur_vblank < dev->vblank->vb_crtcs[crtc].vbl_last)
		diff += dev->vblank->vb_max;

	dev->vblank->vb_crtcs[crtc].vbl_count += diff;
}

int
drm_vblank_get(struct drm_device *dev, int crtc)
{
	struct drm_vblank_info	*vbl = dev->vblank;
	int			 ret = 0;

	if (dev->irq_enabled == 0)
		return (EINVAL);

	mtx_enter(&vbl->vb_lock);
	DPRINTF("%s: %d refs = %d\n", __func__, crtc,
	    vbl->vb_crtcs[crtc].vbl_refs);
	vbl->vb_crtcs[crtc].vbl_refs++;
	if (vbl->vb_crtcs[crtc].vbl_refs == 1 &&
	    vbl->vb_crtcs[crtc].vbl_enabled == 0) {
		if ((ret = dev->driver->enable_vblank(dev, crtc)) == 0) {
			vbl->vb_crtcs[crtc].vbl_enabled = 1;
			drm_update_vblank_count(dev, crtc);
		} else {
			vbl->vb_crtcs[crtc].vbl_refs--;
		}

	}
	mtx_leave(&vbl->vb_lock);

	return (ret);
}

void
drm_vblank_put(struct drm_device *dev, int crtc)
{
	mtx_enter(&dev->vblank->vb_lock);
	/* Last user schedules disable */
	DPRINTF("%s: %d  refs = %d\n", __func__, crtc,
	    dev->vblank->vb_crtcs[crtc].vbl_refs);
	KASSERT(dev->vblank->vb_crtcs[crtc].vbl_refs > 0);
	if (--dev->vblank->vb_crtcs[crtc].vbl_refs == 0)
		timeout_add_sec(&dev->vblank->vb_disable_timer, 5);
	mtx_leave(&dev->vblank->vb_lock);
}

int
drm_modeset_ctl(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_modeset_ctl	*modeset = data;
	struct drm_vblank	*vbl;
	int			 crtc, ret = 0;

	/* not initialised yet, just noop */
	if (dev->vblank == NULL)
		return (0);

	crtc = modeset->crtc;
	if (crtc >= dev->vblank->vb_num || crtc < 0)
		return (EINVAL);

	vbl = &dev->vblank->vb_crtcs[crtc];

	/*
	 * If interrupts are enabled/disabled between calls to this ioctl then
	 * it can get nasty. So just grab a reference so that the interrupts
	 * keep going through the modeset
	 */
	switch (modeset->cmd) {
	case _DRM_PRE_MODESET:
		DPRINTF("%s: pre modeset on %d\n", __func__, crtc);
		if (vbl->vbl_inmodeset == 0) {
			mtx_enter(&dev->vblank->vb_lock);
			vbl->vbl_inmodeset = 0x1;
			mtx_leave(&dev->vblank->vb_lock);
			if (drm_vblank_get(dev, crtc) == 0)
				vbl->vbl_inmodeset |= 0x2;
		}
		break;
	case _DRM_POST_MODESET:
		DPRINTF("%s: post modeset on %d\n", __func__, crtc);
		if (vbl->vbl_inmodeset) {
			if (vbl->vbl_inmodeset & 0x2)
				drm_vblank_put(dev, crtc);
			vbl->vbl_inmodeset = 0;
		}
		break;
	default:
		ret = EINVAL;
		break;
	}

	return (ret);
}

int
drm_wait_vblank(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct timeval		 now;
	union drm_wait_vblank	*vblwait = data;
	int			 ret, flags, crtc, seq;

	if (!dev->irq_enabled || dev->vblank == NULL ||
	    vblwait->request.type & _DRM_VBLANK_SIGNAL)
		return (EINVAL);

	flags = vblwait->request.type & _DRM_VBLANK_FLAGS_MASK;
	crtc = flags & _DRM_VBLANK_SECONDARY ? 1 : 0;

	if (crtc >= dev->vblank->vb_num)
		return (EINVAL);

	if ((ret = drm_vblank_get(dev, crtc)) != 0)
		return (ret);
	seq = drm_vblank_count(dev, crtc);

	if (vblwait->request.type & _DRM_VBLANK_RELATIVE) {
		vblwait->request.sequence += seq;
		vblwait->request.type &= ~_DRM_VBLANK_RELATIVE;
	}

	flags = vblwait->request.type & _DRM_VBLANK_FLAGS_MASK;
	if ((flags & _DRM_VBLANK_NEXTONMISS) &&
	    (seq - vblwait->request.sequence) <= (1<<23)) {
		vblwait->request.sequence = seq + 1;
	}

	if (flags & _DRM_VBLANK_EVENT)
		return (drm_queue_vblank_event(dev, crtc, vblwait, file_priv));

	DPRINTF("%s: %d waiting on %d, current %d\n", __func__, crtc,
	     vblwait->request.sequence, drm_vblank_count(dev, crtc));
	DRM_WAIT_ON(ret, &dev->vblank->vb_crtcs[crtc], &dev->vblank->vb_lock,
	    3 * hz, "drmvblq", ((drm_vblank_count(dev, crtc) -
	    vblwait->request.sequence) <= (1 << 23)) || dev->irq_enabled == 0);

	microtime(&now);
	vblwait->reply.tval_sec = now.tv_sec;
	vblwait->reply.tval_usec = now.tv_usec;
	vblwait->reply.sequence = drm_vblank_count(dev, crtc);
	DPRINTF("%s: %d done waiting, seq = %d\n", __func__, crtc,
	    vblwait->reply.sequence);

	drm_vblank_put(dev, crtc);
	return (ret);
}

int
drm_queue_vblank_event(struct drm_device *dev, int crtc,
    union drm_wait_vblank *vblwait, struct drm_file *file_priv)
{
	struct drm_pending_vblank_event	*vev;
	struct timeval			 now;
	u_int				 seq;


	vev = drm_calloc(1, sizeof(*vev));
	if (vev == NULL)
		return (ENOMEM);

	vev->event.base.type = DRM_EVENT_VBLANK;
	vev->event.base.length = sizeof(vev->event);
	vev->event.user_data = vblwait->request.signal;
	vev->base.event = &vev->event.base;
	vev->base.file_priv = file_priv;
	vev->base.destroy = (void (*) (struct drm_pending_event *))drm_free;

	microtime(&now);

	mtx_enter(&dev->event_lock);
	if (file_priv->event_space < sizeof(vev->event)) {
		mtx_leave(&dev->event_lock);
		drm_free(vev);
		return (ENOMEM);
	}


	seq = drm_vblank_count(dev, crtc);
	file_priv->event_space -= sizeof(vev->event);

	DPRINTF("%s: queueing event %d on crtc %d\n", __func__, seq, crtc);

	if ((vblwait->request.type & _DRM_VBLANK_NEXTONMISS) &&
	    (seq - vblwait->request.sequence) <= (1 << 23)) {
		vblwait->request.sequence = seq + 1;
		vblwait->reply.sequence = vblwait->request.sequence;
	}

	vev->event.sequence = vblwait->request.sequence;
	if ((seq - vblwait->request.sequence) <= (1 << 23)) {
		vev->event.tv_sec = now.tv_sec;
		vev->event.tv_usec = now.tv_usec;
		DPRINTF("%s: already passed, dequeuing: crtc %d, value %d\n",
		    __func__, crtc, seq);
		drm_vblank_put(dev, crtc);
		TAILQ_INSERT_TAIL(&file_priv->evlist, &vev->base, link);
		wakeup(&file_priv->evlist);
		selwakeup(&file_priv->rsel);
	} else {
		TAILQ_INSERT_TAIL(&dev->vblank->vb_crtcs[crtc].vbl_events,
		    &vev->base, link);
	}
	mtx_leave(&dev->event_lock);

	return (0);
}

void
drm_handle_vblank_events(struct drm_device *dev, int crtc)
{
	struct drmevlist		*list;
	struct drm_pending_event	*ev, *tmp;
	struct drm_pending_vblank_event	*vev;
	struct timeval			 now;
	u_int				 seq;

	list = &dev->vblank->vb_crtcs[crtc].vbl_events;
	microtime(&now);
	seq = drm_vblank_count(dev, crtc);

	mtx_enter(&dev->event_lock);
	for (ev = TAILQ_FIRST(list); ev != TAILQ_END(list); ev = tmp) {
		tmp = TAILQ_NEXT(ev, link);

		vev = (struct drm_pending_vblank_event *)ev;

		if ((seq - vev->event.sequence) > (1 << 23))
			continue;
		DPRINTF("%s: got vblank event on crtc %d, value %d\n",
		    __func__, crtc, seq);
		
		vev->event.sequence = seq;
		vev->event.tv_sec = now.tv_sec;
		vev->event.tv_usec = now.tv_usec;
		drm_vblank_put(dev, crtc);
		TAILQ_REMOVE(list, ev, link);
		TAILQ_INSERT_TAIL(&ev->file_priv->evlist, ev, link);
		wakeup(&ev->file_priv->evlist);
		selwakeup(&ev->file_priv->rsel);
	}
	mtx_leave(&dev->event_lock);
}

void
drm_handle_vblank(struct drm_device *dev, int crtc)
{
	/*
	 * XXX if we had proper atomic operations this mutex wouldn't
	 * XXX need to be held.
	 */
	mtx_enter(&dev->vblank->vb_lock);
	dev->vblank->vb_crtcs[crtc].vbl_count++;
	wakeup(&dev->vblank->vb_crtcs[crtc]);
	mtx_leave(&dev->vblank->vb_lock);
	drm_handle_vblank_events(dev, crtc);
}
