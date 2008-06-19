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

irqreturn_t	drm_irq_handler_wrap(DRM_IRQ_ARGS);
void		vblank_disable(void *);

#ifdef __OpenBSD__
void		drm_locked_task(void *context, void *pending);
#else
void		drm_locked_task(void *context, int pending __unused);
#endif

int
drm_irq_by_busid(drm_device_t *dev, void *data, struct drm_file *file_priv)
{
	drm_irq_busid_t *irq = data;

	if ((irq->busnum >> 8) != dev->pci_domain ||
	    (irq->busnum & 0xff) != dev->pci_bus ||
	    irq->devnum != dev->pci_slot ||
	    irq->funcnum != dev->pci_func)
		return EINVAL;

	irq->irq = dev->irq;

	DRM_DEBUG("%d:%d:%d => IRQ %d\n",
		  irq->busnum, irq->devnum, irq->funcnum, irq->irq);

	return 0;
}

#if defined(__FreeBSD__) && __FreeBSD_version >= 500000
static irqreturn_t
drm_irq_handler_wrap(DRM_IRQ_ARGS)
{
	drm_device_t *dev = (drm_device_t *)arg;

	DRM_SPINLOCK(&dev->irq_lock);
	dev->driver.irq_handler(arg);
	DRM_SPINUNLOCK(&dev->irq_lock);
}
#endif

#ifdef __NetBSD__
static irqreturn_t
drm_irq_handler_wrap(DRM_IRQ_ARGS)
{
	int s;
	irqreturn_t ret;
	drm_device_t *dev = (drm_device_t *)arg;

	s = spldrm();
	DRM_SPINLOCK(&dev->irq_lock);
	ret = dev->driver.irq_handler(arg);
	DRM_SPINUNLOCK(&dev->irq_lock);
	splx(s);
	return ret;
}
#endif

#ifdef __OpenBSD__
irqreturn_t
drm_irq_handler_wrap(DRM_IRQ_ARGS)
{
	irqreturn_t ret;
	drm_device_t *dev = (drm_device_t *)arg;

	DRM_SPINLOCK(&dev->irq_lock);
	ret = dev->driver.irq_handler(arg);
	DRM_SPINUNLOCK(&dev->irq_lock);

	return ret;
}
#endif

int
drm_irq_install(drm_device_t *dev)
{
	int retcode;
#if defined(__NetBSD__) || defined(__OpenBSD__)
	pci_intr_handle_t ih;
	const char *istr;
#endif

	if (dev->irq == 0 || dev->dev_private == NULL)
		return EINVAL;

	DRM_DEBUG( "%s: irq=%d\n", __FUNCTION__, dev->irq );

	DRM_LOCK();
	if (dev->irq_enabled) {
		DRM_UNLOCK();
		return EBUSY;
	}
	dev->irq_enabled = 1;

	dev->context_flag = 0;

#ifdef __OpenBSD__
	mtx_init(&dev->irq_lock, IPL_BIO);
#else
	DRM_SPININIT(&dev->irq_lock, "DRM IRQ lock");
#endif

				/* Before installing handler */
	dev->driver.irq_preinstall(dev);
	DRM_UNLOCK();

				/* Install handler */
#ifdef __FreeBSD__
	dev->irqrid = 0;
	dev->irqr = bus_alloc_resource_any(dev->device, SYS_RES_IRQ, 
				      &dev->irqrid, RF_SHAREABLE);
	if (!dev->irqr) {
		retcode = ENOENT;
		goto err;
	}
#if __FreeBSD_version >= 700031
	retcode = bus_setup_intr(dev->device, dev->irqr,
				 INTR_TYPE_TTY | INTR_MPSAFE,
				 NULL, drm_irq_handler_wrap, dev, &dev->irqh);
#else
	retcode = bus_setup_intr(dev->device, dev->irqr,
				 INTR_TYPE_TTY | INTR_MPSAFE,
				 drm_irq_handler_wrap, dev, &dev->irqh);
#endif
	if (retcode != 0)
		goto err;
#elif defined(__NetBSD__) || defined(__OpenBSD__)
	if (pci_intr_map(&dev->pa, &ih) != 0) {
		retcode = ENOENT;
		goto err;
	}
	istr = pci_intr_string(dev->pa.pa_pc, ih);
#if defined(__OpenBSD__)
	dev->irqh = pci_intr_establish(dev->pa.pa_pc, ih, IPL_BIO,
	    drm_irq_handler_wrap, dev,
	    dev->device.dv_xname);
#else
	dev->irqh = pci_intr_establish(dev->pa.pa_pc, ih, IPL_BIO,
	    drm_irq_handler_wrap, dev);
#endif
	if (!dev->irqh) {
		retcode = ENOENT;
		goto err;
	}
	DRM_DEBUG("%s: interrupting at %s\n", dev->device.dv_xname, istr);
#endif

				/* After installing handler */
	DRM_LOCK();
	dev->driver.irq_postinstall(dev);
	DRM_UNLOCK();

#ifdef __FreeBSD__
	TASK_INIT(&dev->locked_task, 0, drm_locked_task, dev);
#endif
	return 0;
err:
	DRM_LOCK();
	dev->irq_enabled = 0;
#ifdef ___FreeBSD__
	if (dev->irqrid != 0) {
		bus_release_resource(dev->device, SYS_RES_IRQ, dev->irqrid,
		    dev->irqr);
		dev->irqrid = 0;
	}
#endif
	DRM_SPINUNINIT(&dev->irq_lock);
	DRM_UNLOCK();
	return retcode;
}

int
drm_irq_uninstall(drm_device_t *dev)
{
#ifdef __FreeBSD__
	int irqrid;
#endif

	if (!dev->irq_enabled)
		return EINVAL;

	dev->irq_enabled = 0;
#ifdef __FreeBSD__
	irqrid = dev->irqrid;
	dev->irqrid = 0;
#endif

	DRM_DEBUG( "%s: irq=%d\n", __FUNCTION__, dev->irq );

	dev->driver.irq_uninstall(dev);

#ifdef __FreeBSD__
	DRM_UNLOCK();
	bus_teardown_intr(dev->device, dev->irqr, dev->irqh);
	bus_release_resource(dev->device, SYS_RES_IRQ, irqrid, dev->irqr);
	DRM_LOCK();
#elif defined(__NetBSD__) || defined(__OpenBSD__)
	pci_intr_disestablish(dev->pa.pa_pc, dev->irqh);
#endif
	drm_vblank_cleanup(dev);
	DRM_SPINUNINIT(&dev->irq_lock);

	return 0;
}

int
drm_control(drm_device_t *dev, void *data, struct drm_file *file_priv)
{
	drm_control_t *ctl = data;
	int err;

	switch ( ctl->func ) {
	case DRM_INST_HANDLER:
		/* Handle drivers whose DRM used to require IRQ setup but the
		 * no longer does.
		 */
		if (!dev->driver.use_irq)
			return 0;
		if (dev->if_version < DRM_IF_VERSION(1, 2) &&
		    ctl->irq != dev->irq)
			return EINVAL;
		return drm_irq_install(dev);
	case DRM_UNINST_HANDLER:
		if (!dev->driver.use_irq)
			return 0;
		DRM_LOCK();
		err = drm_irq_uninstall(dev);
		DRM_UNLOCK();
		return err;
	default:
		return EINVAL;
	}
}

void
vblank_disable(void *arg)
{
	struct drm_device *dev = (struct drm_device*)arg;
	int i;

	for (i=0; i < dev->num_crtcs; i++){
		DRM_SPINLOCK(&dev->vbl_lock);
		if (atomic_read(&dev->vblank_refcount[i]) == 0 &&
		    dev->vblank_enabled[i]) {
			dev->driver.disable_vblank(dev, i);
			dev->vblank_enabled[i] = 0;
		}
		DRM_SPINUNLOCK(&dev->vbl_lock);
	}
}

void
drm_vblank_cleanup(struct drm_device *dev)
{
	if (dev->num_crtcs == 0)
		return; /* not initialised */

	timeout_del(&dev->vblank_disable_timer);

	vblank_disable(dev);

	if (dev->vbl_queue)
		drm_free(dev->vbl_queue, sizeof(*dev->vbl_queue) *
		    dev->num_crtcs, M_DRM);
#if 0 /* disabled for now */
	if (dev-vbl_sigs)
		drm_free(dev->vbl_sigs, sizeof(*dev->vbl_sigs) * dev->num_crtcs,
		    M_DRM);
#endif
	if (dev->_vblank_count)
		drm_free(dev->_vblank_count, sizeof(*dev->_vblank_count) *
		    dev->num_crtcs, M_DRM);
	if (dev->vblank_refcount)
		drm_free(dev->vblank_refcount, sizeof(*dev->vblank_refcount) *
		    dev->num_crtcs, M_DRM);
	if (dev->vblank_enabled)
		drm_free(dev->vblank_enabled, sizeof(*dev->vblank_enabled) *
		    dev->num_crtcs, M_DRM);
	if (dev->last_vblank)
		drm_free(dev->last_vblank, sizeof(*dev->last_vblank) *
		    dev->num_crtcs, M_DRM);
	if (dev->vblank_premodeset)
		drm_free(dev->vblank_premodeset,
		    sizeof(*dev->vblank_premodeset) * dev->num_crtcs, M_DRM);
	if (dev->vblank_suspend)
		drm_free(dev->vblank_suspend,
		    sizeof(*dev->vblank_suspend) * dev->num_crtcs, M_DRM);

	dev->num_crtcs = 0;
	DRM_SPINUNINIT(&dev->vbl_lock);
}

int
drm_vblank_init(struct drm_device *dev, int num_crtcs)
{
	int i;

	timeout_set(&dev->vblank_disable_timer, vblank_disable, dev);
	mtx_init(&dev->vbl_lock, IPL_BIO);
	atomic_set(&dev->vbl_signal_pending, 0);
	dev->num_crtcs = num_crtcs;

	if ((dev->vbl_queue = drm_calloc(num_crtcs, sizeof(*dev->vbl_queue),
	    M_DRM)) == NULL)
		goto err;

	if ((dev->_vblank_count = drm_calloc(num_crtcs,
	    sizeof(*dev->_vblank_count), M_DRM)) == NULL)
		goto err;

	if ((dev->vblank_refcount = drm_calloc(num_crtcs,
	    sizeof(*dev->vblank_refcount), M_DRM)) == NULL)
		goto err;
	if ((dev->vblank_enabled = drm_calloc(num_crtcs,
	    sizeof(*dev->vblank_enabled), M_DRM)) == NULL)
		goto err;
	if ((dev->last_vblank = drm_calloc(num_crtcs, sizeof(*dev->last_vblank),
	    M_DRM)) == NULL)
		goto err;
	if ((dev->vblank_premodeset = drm_calloc(num_crtcs,
	    sizeof(*dev->vblank_premodeset), M_DRM)) == NULL)
		goto err;
	if ((dev->vblank_suspend = drm_calloc(num_crtcs,
	    sizeof(*dev->vblank_suspend), M_DRM)) == NULL)
		goto err;


	/* Zero everything */
	for (i = 0; i < num_crtcs; i++) {
		atomic_set(&dev->_vblank_count[i], 0);
		atomic_set(&dev->vblank_refcount[i], 0);
	}

	return (0);

err:
	drm_vblank_cleanup(dev);
	return ENOMEM;
}

u_int32_t
drm_vblank_count(struct drm_device *dev, int crtc)
{
	return atomic_read(&dev->_vblank_count[crtc]);
}

void
drm_update_vblank_count(struct drm_device *dev, int crtc)
{
	u_int32_t cur_vblank, diff;

	if (dev->vblank_suspend[crtc])
		return;

	/*
	 * Deal with the possibility of lost vblanks due to disabled interrupts
	 * counter overflow may have happened. 
	 */
	cur_vblank = dev->driver.get_vblank_counter(dev, crtc);
	DRM_SPINLOCK(&dev->vbl_lock);
	if (cur_vblank < dev->last_vblank[crtc]) {
		if (cur_vblank == dev->last_vblank[crtc] -1)
			diff = 0;
		else {
			diff = dev->max_vblank_count - dev->last_vblank[crtc];
			diff += cur_vblank;
		}
	} else {
		diff = cur_vblank - dev->last_vblank[crtc];
	}
	dev->last_vblank[crtc] = cur_vblank;
	DRM_SPINUNLOCK(&dev->vbl_lock);

	atomic_add(diff, &dev->_vblank_count[crtc]);
}

int
drm_vblank_get(struct drm_device *dev, int crtc)
{
	int ret = 0;

	DRM_SPINLOCK(&dev->vbl_lock);

	atomic_add(1, &dev->vblank_refcount[crtc]);
	if (dev->vblank_refcount[crtc] == 1 &&
	    dev->vblank_enabled[crtc] == 0) {
		ret = dev->driver.enable_vblank(dev, crtc);
		if (ret) {
			atomic_dec(&dev->vblank_refcount[crtc]);
		} else {
			dev->vblank_enabled[crtc] = 1;
		}
	}
	DRM_SPINUNLOCK(&dev->vbl_lock);

	return ret;
}

void
drm_vblank_put(struct drm_device *dev, int crtc)
{
	DRM_SPINLOCK(&dev->vbl_lock);
	/* Last user schedules interrupt disable */
	atomic_dec(&dev->vblank_refcount[crtc]);
	if (dev->vblank_refcount[crtc] == 0) 
		timeout_add(&dev->vblank_disable_timer, 5*DRM_HZ);
	DRM_SPINUNLOCK(&dev->vbl_lock);
}

int
drm_modeset_ctl(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_modeset_ctl *modeset = data;
	int crtc, ret = 0;

	crtc = modeset->crtc;
	if (crtc >= dev->num_crtcs) {
		ret = EINVAL;
		goto out;
	}

	switch (modeset->cmd) {
	case _DRM_PRE_MODESET:
		dev->vblank_premodeset[crtc] =
			dev->driver.get_vblank_counter(dev, crtc);
		dev->vblank_suspend[crtc] = 1;
		break;
	case _DRM_POST_MODESET:
		if (dev->vblank_suspend[crtc]) {
			uint32_t new =
			    dev->driver.get_vblank_counter(dev, crtc);
			/* Compensate for spurious wraparound */
			if (new < dev->vblank_premodeset[crtc])
				atomic_sub(dev->max_vblank_count + new -
				    dev->vblank_premodeset[crtc],
				    &dev->_vblank_count[crtc]);
		}
		dev->vblank_suspend[crtc] = 0;
		break;
	default:
		ret = EINVAL;
	}

out:
	return ret;
}

int
drm_wait_vblank(drm_device_t *dev, void *data, struct drm_file *file_priv)
{
	drm_wait_vblank_t *vblwait = data;
	int ret, flags, crtc, seq;

	if (!dev->irq_enabled)
		return EINVAL;

	flags = vblwait->request.type & _DRM_VBLANK_FLAGS_MASK;
	crtc = flags & _DRM_VBLANK_SECONDARY ? 1 : 0;

	if (crtc >= dev->num_crtcs)
		return EINVAL;

	drm_update_vblank_count(dev, crtc);
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
#if 0 /* disabled */
		if (dev->vblank_suspend[crtc])
			return (EBUSY);

		drm_vbl_sig_t *vbl_sig = malloc(sizeof(drm_vbl_sig_t), M_DRM,
		    M_NOWAIT | M_ZERO);
		if (vbl_sig == NULL)
			return ENOMEM;

		vbl_sig->sequence = vblwait->request.sequence;
		vbl_sig->signo = vblwait->request.signal;
		vbl_sig->pid = DRM_CURRENTPID;

		vblwait->reply.sequence = atomic_read(&dev->vbl_received);

		
		DRM_SPINLOCK(&dev->vbl_lock);
		TAILQ_INSERT_HEAD(&dev->vbl_sig_list, vbl_sig, link);
		DRM_SPINUNLOCK(&dev->vbl_lock);
		ret = 0;
#endif
		ret = EINVAL;
	} else {
		if (!dev->vblank_suspend[crtc]) {
			unsigned long cur_vblank;

			ret = drm_vblank_get(dev, crtc);
			if (ret)
				return ret;
			while (ret == 0) {
				DRM_SPINLOCK(&dev->vbl_lock);
				if (((cur_vblank = drm_vblank_count(dev, crtc))
				    - vblwait->request.sequence) <= (1 << 23)) {
					DRM_SPINUNLOCK(&dev->vbl_lock);
					break;
				}
				ret = msleep(&dev->vbl_queue[crtc],
				    &dev->vbl_lock, PZERO | PCATCH,
				    "drmvblq", 3 * DRM_HZ);
				DRM_SPINUNLOCK(&dev->vbl_lock);
			}
		}
		drm_vblank_put(dev, crtc);

		if (ret != EINTR) {
			struct timeval now;
			microtime(&now);
			vblwait->reply.tval_sec = now.tv_sec;
			vblwait->reply.tval_usec = now.tv_usec;
			vblwait->reply.sequence = drm_vblank_count(dev, crtc);
		}
	}

	return (ret);
}

void
drm_vbl_send_signals(drm_device_t *dev, int crtc)
{
}

#if 0 /* disabled */
void
drm_vbl_send_signals(drm_device_t *dev, int crtc)
{
	drm_vbl_sig_t *vbl_sig;
	unsigned int vbl_seq = atomic_read( &dev->vbl_received );
	struct proc *p;

	vbl_sig = TAILQ_FIRST(&dev->vbl_sig_list);
	while (vbl_sig != NULL) {
		drm_vbl_sig_t *next = TAILQ_NEXT(vbl_sig, link);

		if ( ( vbl_seq - vbl_sig->sequence ) <= (1<<23) ) {
			p = pfind(vbl_sig->pid);
			if (p != NULL)
				psignal(p, vbl_sig->signo);

			TAILQ_REMOVE(&dev->vbl_sig_list, vbl_sig, link);
			DRM_FREE(vbl_sig,sizeof(*vbl_sig));
		}
		vbl_sig = next;
	}
}
#endif

void
drm_handle_vblank(struct drm_device *dev, int crtc)
{
	drm_update_vblank_count(dev, crtc);
	DRM_WAKEUP(&dev->vbl_queue[crtc]);
	drm_vbl_send_signals(dev, crtc);
}

void
#ifdef __OpenBSD__
drm_locked_task(void *context, void *pending)
#else
drm_locked_task(void *context, int pending __unused)
#endif
{
	drm_device_t *dev = context;

	DRM_LOCK();
	for (;;) {
		int ret;

		if (drm_lock_take(&dev->lock.hw_lock->lock,
		    DRM_KERNEL_CONTEXT))
		{
			dev->lock.file_priv = NULL; /* kernel owned */
			dev->lock.lock_time = jiffies;
			atomic_inc(&dev->counts[_DRM_STAT_LOCKS]);
			break;  /* Got lock */
		}

		/* Contention */
		ret = DRM_SLEEPLOCK((void *)&dev->lock.lock_queue, &dev->dev_lock,
		    PZERO | PCATCH, "drmlk2", 0);
		if (ret != 0) {
			DRM_UNLOCK();
			return;
		}
	}
	DRM_UNLOCK();

	dev->locked_task_call(dev);

	drm_lock_free(dev, &dev->lock.hw_lock->lock, DRM_KERNEL_CONTEXT);
}

void
drm_locked_tasklet(drm_device_t *dev, void (*tasklet)(drm_device_t *dev))
{
	dev->locked_task_call = tasklet;
#ifdef __FreeBSD__
	taskqueue_enqueue(taskqueue_swi, &dev->locked_task);
#else
	if (workq_add_task(NULL, 0, drm_locked_task, dev, NULL) == ENOMEM)
		DRM_ERROR("error adding task to workq\n");
#endif
}
