/*-
 * Copyright 2007-2009 Owain G. Ainsworth <oga@openbsd.org>
 * Copyright © 2008 Intel Corporation
 * Copyright 2003 Eric Anholt
 * Copyright 1999, 2000 Precision Insight, Inc., Cedar Park, Texas.
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
 *    Daryll Strauss <daryll@valinux.com>
 *    Gareth Hughes <gareth@valinux.com>
 *    Eric Anholt <eric@anholt.net>
 *    Owain Ainsworth <oga@openbsd.org>
 *
 */

/** @file drm_drv.c
 * The catch-all file for DRM device support, including module setup/teardown,
 * open/close, and ioctl dispatch.
 */

#include <sys/param.h>
#include <sys/limits.h>
#include <sys/systm.h>
#include <uvm/uvm_extern.h>

#include <sys/ttycom.h> /* for TIOCSGRP */

#include "drmP.h"
#include "drm.h"
#include "drm_sarea.h"

#ifdef DRM_DEBUG_DEFAULT_ON
int drm_debug_flag = 1;
#else
int drm_debug_flag = 0;
#endif

int	 drm_firstopen(struct drm_device *);
int	 drm_lastclose(struct drm_device *);
void	 drm_attach(struct device *, struct device *, void *);
int	 drm_probe(struct device *, void *, void *);
int	 drm_detach(struct device *, int);
int	 drm_activate(struct device *, int);
int	 drmprint(void *, const char *);

int	 drm_getunique(struct drm_device *, void *, struct drm_file *);
int	 drm_version(struct drm_device *, void *, struct drm_file *);
int	 drm_setversion(struct drm_device *, void *, struct drm_file *);
int	 drm_getmagic(struct drm_device *, void *, struct drm_file *);
int	 drm_authmagic(struct drm_device *, void *, struct drm_file *);
int	 drm_file_cmp(struct drm_file *, struct drm_file *);
SPLAY_PROTOTYPE(drm_file_tree, drm_file, link, drm_file_cmp);

/* functions used by the per-open handle  code to grab references to object */
void	 drm_handle_ref(struct drm_obj *);
void	 drm_handle_unref(struct drm_obj *);

int	 drm_handle_cmp(struct drm_handle *, struct drm_handle *);
int	 drm_name_cmp(struct drm_obj *, struct drm_obj *);
int	 drm_fault(struct uvm_faultinfo *, vaddr_t, vm_page_t *, int, int,
	     vm_fault_t, vm_prot_t, int);
boolean_t	 drm_flush(struct uvm_object *, voff_t, voff_t, int);

SPLAY_PROTOTYPE(drm_obj_tree, drm_handle, entry, drm_handle_cmp);
SPLAY_PROTOTYPE(drm_name_tree, drm_obj, entry, drm_name_cmp);

/*
 * attach drm to a pci-based driver.
 *
 * This function does all the pci-specific calculations for the 
 * drm_attach_args.
 */
struct device *
drm_attach_pci(const struct drm_driver_info *driver, struct pci_attach_args *pa,
    int is_agp, struct device *dev)
{
	struct drm_attach_args arg;

	arg.driver = driver;
	arg.dmat = pa->pa_dmat;
	arg.bst = pa->pa_memt;
	arg.irq = pa->pa_intrline;
	arg.is_agp = is_agp;

	arg.busid_len = 20;
	arg.busid = malloc(arg.busid_len + 1, M_DRM, M_NOWAIT);
	if (arg.busid == NULL) {
		printf("%s: no memory for drm\n", dev->dv_xname);
		return (NULL);
	}
	snprintf(arg.busid, arg.busid_len, "pci:%04x:%02x:%02x.%1x",
	    pa->pa_domain, pa->pa_bus, pa->pa_device, pa->pa_function);

	return (config_found(dev, &arg, drmprint));
}

int
drmprint(void *aux, const char *pnp)
{
	if (pnp != NULL)
		printf("drm at %s", pnp);
	return (UNCONF);
}

int
drm_pciprobe(struct pci_attach_args *pa, const struct drm_pcidev *idlist)
{
	const struct drm_pcidev *id_entry;

	id_entry = drm_find_description(PCI_VENDOR(pa->pa_id),
	    PCI_PRODUCT(pa->pa_id), idlist);
	if (id_entry != NULL)
		return 1;

	return 0;
}

int
drm_probe(struct device *parent, void *match, void *aux)
{
	struct drm_attach_args *da = aux;

	return (da->driver != NULL ? 1 : 0);
}

void
drm_attach(struct device *parent, struct device *self, void *aux)
{
	struct drm_device	*dev = (struct drm_device *)self;
	struct drm_attach_args	*da = aux;

	dev->dev_private = parent;
	dev->driver = da->driver;

	dev->dmat = da->dmat;
	dev->bst = da->bst;
	dev->irq = da->irq;
	dev->unique = da->busid;
	dev->unique_len = da->busid_len;

	rw_init(&dev->dev_lock, "drmdevlk");
	mtx_init(&dev->lock.spinlock, IPL_NONE);

	TAILQ_INIT(&dev->maplist);
	SPLAY_INIT(&dev->files);

	if (dev->driver->vblank_pipes != 0 && drm_vblank_init(dev,
	    dev->driver->vblank_pipes)) {
		printf(": failed to allocate vblank data\n");
		goto error;
	}

	/*
	 * the dma buffers api is just weird. offset 1Gb to ensure we don't
	 * conflict with it.
	 */
	dev->handle_ext = extent_create("drmext", 1024*1024*1024, LONG_MAX,
	    M_DRM, NULL, NULL, EX_NOWAIT | EX_NOCOALESCE);
	if (dev->handle_ext == NULL) {
		DRM_ERROR("Failed to initialise handle extent\n");
		goto error;
	}

	if (dev->driver->flags & DRIVER_AGP) {
		if (da->is_agp)
			dev->agp = drm_agp_init();
		if (dev->driver->flags & DRIVER_AGP_REQUIRE &&
		    dev->agp == NULL) {
			printf(": couldn't find agp\n");
			goto error;
		}
		if (dev->agp != NULL) {
			if (drm_mtrr_add(dev->agp->info.ai_aperture_base,
			    dev->agp->info.ai_aperture_size, DRM_MTRR_WC) == 0)
				dev->agp->mtrr = 1;
		}
	}

	if (drm_ctxbitmap_init(dev) != 0) {
		printf(": couldn't allocate memory for context bitmap.\n");
		goto error;
	}

	if (dev->driver->flags & DRIVER_GEM) {
		mtx_init(&dev->obj_name_lock, IPL_NONE);
		SPLAY_INIT(&dev->name_tree);
		KASSERT(dev->driver->gem_size >= sizeof(struct drm_obj));
		/* XXX unique name */
		pool_init(&dev->objpl, dev->driver->gem_size, 0, 0, 0,
		    "drmobjpl", &pool_allocator_nointr);
	}

	printf("\n");
	return;

error:
	drm_lastclose(dev);
}

int
drm_detach(struct device *self, int flags)
{
	struct drm_device *dev = (struct drm_device *)self;

	drm_lastclose(dev);

	drm_ctxbitmap_cleanup(dev);

	extent_destroy(dev->handle_ext);

	drm_vblank_cleanup(dev);

	if (dev->agp && dev->agp->mtrr) {
		int retcode;

		retcode = drm_mtrr_del(0, dev->agp->info.ai_aperture_base,
		    dev->agp->info.ai_aperture_size, DRM_MTRR_WC);
		DRM_DEBUG("mtrr_del = %d", retcode);
	}


	if (dev->agp != NULL) {
		drm_free(dev->agp);
		dev->agp = NULL;
	}

	return 0;
}

int
drm_activate(struct device *self, int act)
{
	switch (act) {
	case DVACT_ACTIVATE:
		break;

	case DVACT_DEACTIVATE:
		/* FIXME */
		break;
	}
	return (0);
}

struct cfattach drm_ca = {
	sizeof(struct drm_device), drm_probe, drm_attach,
	drm_detach, drm_activate
};

struct cfdriver drm_cd = {
	0, "drm", DV_DULL
};

const struct drm_pcidev *
drm_find_description(int vendor, int device, const struct drm_pcidev *idlist)
{
	int i = 0;
	
	for (i = 0; idlist[i].vendor != 0; i++) {
		if ((idlist[i].vendor == vendor) &&
		    (idlist[i].device == device))
			return &idlist[i];
	}
	return NULL;
}

int
drm_file_cmp(struct drm_file *f1, struct drm_file *f2)
{
	return (f1->minor < f2->minor ? -1 : f1->minor > f2->minor);
}

SPLAY_GENERATE(drm_file_tree, drm_file, link, drm_file_cmp);

struct drm_file *
drm_find_file_by_minor(struct drm_device *dev, int minor)
{
	struct drm_file	key;

	key.minor = minor;
	return (SPLAY_FIND(drm_file_tree, &dev->files, &key));
}

int
drm_firstopen(struct drm_device *dev)
{
	struct drm_local_map	*map;
	int			 i;

	/* prebuild the SAREA */
	i = drm_addmap(dev, 0, SAREA_MAX, _DRM_SHM,
	    _DRM_CONTAINS_LOCK, &map);
	if (i != 0)
		return i;

	if (dev->driver->firstopen)
		dev->driver->firstopen(dev);

	if (dev->driver->flags & DRIVER_DMA) {
		if ((i = drm_dma_setup(dev)) != 0)
			return (i);
	}

	dev->magicid = 1;

	dev->irq_enabled = 0;
	dev->if_version = 0;

	dev->buf_pgid = 0;

	DRM_DEBUG("\n");

	return 0;
}

int
drm_lastclose(struct drm_device *dev)
{
	struct drm_local_map	*map, *mapsave;

	DRM_DEBUG("\n");

	if (dev->driver->lastclose != NULL)
		dev->driver->lastclose(dev);

	if (dev->irq_enabled)
		drm_irq_uninstall(dev);

	drm_agp_takedown(dev);
	drm_dma_takedown(dev);

	DRM_LOCK();
	if (dev->sg != NULL) {
		struct drm_sg_mem *sg = dev->sg; 
		dev->sg = NULL;

		DRM_UNLOCK();
		drm_sg_cleanup(dev, sg);
		DRM_LOCK();
	}

	for (map = TAILQ_FIRST(&dev->maplist); map != TAILQ_END(&dev->maplist);
	    map = mapsave) {
		mapsave = TAILQ_NEXT(map, link);
		if ((map->flags & _DRM_DRIVER) == 0)
			drm_rmmap_locked(dev, map);
	}

	if (dev->lock.hw_lock != NULL) {
		dev->lock.hw_lock = NULL; /* SHM removed */
		dev->lock.file_priv = NULL;
		wakeup(&dev->lock); /* there should be nothing sleeping on it */
	}
	DRM_UNLOCK();

	return 0;
}

int
drmopen(dev_t kdev, int flags, int fmt, struct proc *p)
{
	struct drm_device	*dev = NULL;
	struct drm_file		*file_priv;
	int			 ret = 0;

	dev = drm_get_device_from_kdev(kdev);
	if (dev == NULL)
		return (ENXIO);

	DRM_DEBUG("open_count = %d\n", dev->open_count);

	if (flags & O_EXCL)
		return (EBUSY); /* No exclusive opens */

	DRM_LOCK();
	if (dev->open_count++ == 0) {
		DRM_UNLOCK();
		if ((ret = drm_firstopen(dev)) != 0)
			goto err;
	} else {
		DRM_UNLOCK();
	}

	/* always allocate at least enough space for our data */
	file_priv = drm_calloc(1, max(dev->driver->file_priv_size,
	    sizeof(*file_priv)));
	if (file_priv == NULL) {
		ret = ENOMEM;
		goto err;
	}

	file_priv->kdev = kdev;
	file_priv->flags = flags;
	file_priv->minor = minor(kdev);
	DRM_DEBUG("minor = %d\n", file_priv->minor);

	/* for compatibility root is always authenticated */
	file_priv->authenticated = DRM_SUSER(p);

	if (dev->driver->flags & DRIVER_GEM) {
		SPLAY_INIT(&file_priv->obj_tree);
		mtx_init(&file_priv->table_lock, IPL_NONE);
	}

	if (dev->driver->open) {
		ret = dev->driver->open(dev, file_priv);
		if (ret != 0) {
			goto free_priv;
		}
	}

	DRM_LOCK();
	/* first opener automatically becomes master if root */
	if (SPLAY_EMPTY(&dev->files) && !DRM_SUSER(p)) {
		DRM_UNLOCK();
		ret = EPERM;
		goto free_priv;
	}

	file_priv->master = SPLAY_EMPTY(&dev->files);

	SPLAY_INSERT(drm_file_tree, &dev->files, file_priv);
	DRM_UNLOCK();

	return (0);

free_priv:
	drm_free(file_priv);
err:
	DRM_LOCK();
	--dev->open_count;
	DRM_UNLOCK();
	return (ret);
}

int
drmclose(dev_t kdev, int flags, int fmt, struct proc *p)
{
	struct drm_device *dev = drm_get_device_from_kdev(kdev);
	struct drm_file *file_priv;
	int retcode = 0;

	if (dev == NULL)
		return (ENXIO);

	DRM_DEBUG("open_count = %d\n", dev->open_count);

	DRM_LOCK();
	file_priv = drm_find_file_by_minor(dev, minor(kdev));
	if (file_priv == NULL) {
		DRM_ERROR("can't find authenticator\n");
		retcode = EINVAL;
		goto done;
	}
	DRM_UNLOCK();

	if (dev->driver->close != NULL)
		dev->driver->close(dev, file_priv);

	DRM_DEBUG("pid = %d, device = 0x%lx, open_count = %d\n",
	    DRM_CURRENTPID, (long)&dev->device, dev->open_count);

	if (dev->lock.hw_lock && _DRM_LOCK_IS_HELD(dev->lock.hw_lock->lock)
	    && dev->lock.file_priv == file_priv) {
		DRM_DEBUG("Process %d dead, freeing lock for context %d\n",
		    DRM_CURRENTPID,
		    _DRM_LOCKING_CONTEXT(dev->lock.hw_lock->lock));
		if (dev->driver->reclaim_buffers_locked != NULL)
			dev->driver->reclaim_buffers_locked(dev, file_priv);

		drm_lock_free(&dev->lock,
		    _DRM_LOCKING_CONTEXT(dev->lock.hw_lock->lock));
	} else if (dev->driver->reclaim_buffers_locked != NULL &&
	    dev->lock.hw_lock != NULL) {
		mtx_enter(&dev->lock.spinlock);
		/* The lock is required to reclaim buffers */
		for (;;) {
			if (dev->lock.hw_lock == NULL) {
				/* Device has been unregistered */
				retcode = EINTR;
				break;
			}
			if (drm_lock_take(&dev->lock, DRM_KERNEL_CONTEXT)) {
				dev->lock.file_priv = file_priv;
				break;	/* Got lock */
			}
				/* Contention */
			retcode = msleep(&dev->lock,
			    &dev->lock.spinlock, PZERO | PCATCH, "drmlk2", 0);
			if (retcode)
				break;
		}
		mtx_leave(&dev->lock.spinlock);
		if (retcode == 0) {
			dev->driver->reclaim_buffers_locked(dev, file_priv);
			drm_lock_free(&dev->lock, DRM_KERNEL_CONTEXT);
		}
	}

	if (dev->driver->flags & DRIVER_DMA &&
	    !dev->driver->reclaim_buffers_locked)
		drm_reclaim_buffers(dev, file_priv);

	DRM_LOCK();
	if (dev->driver->flags & DRIVER_GEM) {
		struct drm_handle	*han;
		mtx_enter(&file_priv->table_lock);
		while ((han = SPLAY_ROOT(&file_priv->obj_tree)) != NULL) {
			SPLAY_REMOVE(drm_obj_tree, &file_priv->obj_tree, han);
			drm_handle_unref(han->obj);
			drm_free(han);
		}
		mtx_leave(&file_priv->table_lock);
	}

	dev->buf_pgid = 0;

	SPLAY_REMOVE(drm_file_tree, &dev->files, file_priv);
	drm_free(file_priv);

done:
	if (--dev->open_count == 0) {
		DRM_UNLOCK();
		retcode = drm_lastclose(dev);
	}

	DRM_UNLOCK();
	
	return (retcode);
}

/* drmioctl is called whenever a process performs an ioctl on /dev/drm.
 */
int
drmioctl(dev_t kdev, u_long cmd, caddr_t data, int flags, 
    struct proc *p)
{
	struct drm_device *dev = drm_get_device_from_kdev(kdev);
	struct drm_file *file_priv;

	if (dev == NULL)
		return ENODEV;

	DRM_LOCK();
	file_priv = drm_find_file_by_minor(dev, minor(kdev));
	DRM_UNLOCK();
	if (file_priv == NULL) {
		DRM_ERROR("can't find authenticator\n");
		return EINVAL;
	}

	++file_priv->ioctl_count;

	DRM_DEBUG("pid=%d, cmd=0x%02lx, nr=0x%02x, dev 0x%lx, auth=%d\n",
	    DRM_CURRENTPID, cmd, DRM_IOCTL_NR(cmd), (long)&dev->device,
	    file_priv->authenticated);

	switch (cmd) {
	case FIONBIO:
	case FIOASYNC:
		return 0;

	case TIOCSPGRP:
		dev->buf_pgid = *(int *)data;
		return 0;

	case TIOCGPGRP:
		*(int *)data = dev->buf_pgid;
		return 0;
	case DRM_IOCTL_VERSION:
		return (drm_version(dev, data, file_priv));
	case DRM_IOCTL_GET_UNIQUE:
		return (drm_getunique(dev, data, file_priv));
	case DRM_IOCTL_GET_MAGIC:
		return (drm_getmagic(dev, data, file_priv));
	case DRM_IOCTL_WAIT_VBLANK:
		return (drm_wait_vblank(dev, data, file_priv));
	case DRM_IOCTL_MODESET_CTL:
		return (drm_modeset_ctl(dev, data, file_priv));
	case DRM_IOCTL_GEM_CLOSE:
		return (drm_gem_close_ioctl(dev, data, file_priv));

	/* removed */
	case DRM_IOCTL_GET_MAP:
		/* FALLTHROUGH */
	case DRM_IOCTL_GET_CLIENT:
		/* FALLTHROUGH */
	case DRM_IOCTL_GET_STATS:
		return (EINVAL);
	/*
	 * no-oped ioctls, we don't check permissions on them because
	 * they do nothing. they'll be removed as soon as userland is
	 * definitely purged
	 */
	case DRM_IOCTL_SET_SAREA_CTX:
	case DRM_IOCTL_BLOCK:
	case DRM_IOCTL_UNBLOCK:
	case DRM_IOCTL_MOD_CTX:
	case DRM_IOCTL_MARK_BUFS:
	case DRM_IOCTL_FINISH:
	case DRM_IOCTL_INFO_BUFS:
	case DRM_IOCTL_SWITCH_CTX:
	case DRM_IOCTL_NEW_CTX:
	case DRM_IOCTL_GET_SAREA_CTX:
		return (0);
	}

	if (file_priv->authenticated == 1) {
		switch (cmd) {
		case DRM_IOCTL_RM_MAP:
			return (drm_rmmap_ioctl(dev, data, file_priv));
		case DRM_IOCTL_GET_CTX:
			return (drm_getctx(dev, data, file_priv));
		case DRM_IOCTL_RES_CTX:
			return (drm_resctx(dev, data, file_priv));
		case DRM_IOCTL_LOCK:
			return (drm_lock(dev, data, file_priv));
		case DRM_IOCTL_UNLOCK:
			return (drm_unlock(dev, data, file_priv));
		case DRM_IOCTL_MAP_BUFS:
			return (drm_mapbufs(dev, data, file_priv));
		case DRM_IOCTL_FREE_BUFS:
			return (drm_freebufs(dev, data, file_priv));
		case DRM_IOCTL_DMA:
			return (drm_dma(dev, data, file_priv));
		case DRM_IOCTL_AGP_INFO:
			return (drm_agp_info_ioctl(dev, data, file_priv));
		case DRM_IOCTL_GEM_FLINK:
			return (drm_gem_flink_ioctl(dev, data, file_priv));
		case DRM_IOCTL_GEM_OPEN:
			return (drm_gem_open_ioctl(dev, data, file_priv));

		}
	}

	/* master is always root */
	if (file_priv->master == 1) {
		switch(cmd) {
		case DRM_IOCTL_SET_VERSION:
			return (drm_setversion(dev, data, file_priv));
		case DRM_IOCTL_IRQ_BUSID:
			return (drm_irq_by_busid(dev, data, file_priv));
		case DRM_IOCTL_AUTH_MAGIC:
			return (drm_authmagic(dev, data, file_priv));
		case DRM_IOCTL_ADD_MAP:
			return (drm_addmap_ioctl(dev, data, file_priv));
		case DRM_IOCTL_ADD_CTX:
			return (drm_addctx(dev, data, file_priv));
		case DRM_IOCTL_RM_CTX:
			return (drm_rmctx(dev, data, file_priv));
		case DRM_IOCTL_ADD_BUFS:
			return (drm_addbufs(dev, (struct drm_buf_desc *)data));
		case DRM_IOCTL_CONTROL:
			return (drm_control(dev, data, file_priv));
		case DRM_IOCTL_AGP_ACQUIRE:
			return (drm_agp_acquire_ioctl(dev, data, file_priv));
		case DRM_IOCTL_AGP_RELEASE:
			return (drm_agp_release_ioctl(dev, data, file_priv));
		case DRM_IOCTL_AGP_ENABLE:
			return (drm_agp_enable_ioctl(dev, data, file_priv));
		case DRM_IOCTL_AGP_ALLOC:
			return (drm_agp_alloc_ioctl(dev, data, file_priv));
		case DRM_IOCTL_AGP_FREE:
			return (drm_agp_free_ioctl(dev, data, file_priv));
		case DRM_IOCTL_AGP_BIND:
			return (drm_agp_bind_ioctl(dev, data, file_priv));
		case DRM_IOCTL_AGP_UNBIND:
			return (drm_agp_unbind_ioctl(dev, data, file_priv));
		case DRM_IOCTL_SG_ALLOC:
			return (drm_sg_alloc_ioctl(dev, data, file_priv));
		case DRM_IOCTL_SG_FREE:
			return (drm_sg_free(dev, data, file_priv));
		case DRM_IOCTL_ADD_DRAW:
		case DRM_IOCTL_RM_DRAW:
		case DRM_IOCTL_UPDATE_DRAW:
			/*
			 * Support removed from kernel since it's not used.
			 * just return zero until userland stops calling this
			 * ioctl.
			 */
			return (0);
		case DRM_IOCTL_SET_UNIQUE:
		/*
		 * Deprecated in DRM version 1.1, and will return EBUSY
		 * when setversion has
		 * requested version 1.1 or greater.
		 */
			return (EBUSY);
		}
	}
	if (dev->driver->ioctl != NULL)
		return (dev->driver->ioctl(dev, cmd, data, file_priv));
	else
		return (EINVAL);
}

struct drm_local_map *
drm_getsarea(struct drm_device *dev)
{
	struct drm_local_map	*map;

	DRM_LOCK();
	TAILQ_FOREACH(map, &dev->maplist, link) {
		if (map->type == _DRM_SHM && (map->flags & _DRM_CONTAINS_LOCK))
			break;
	}
	DRM_UNLOCK();
	return (map);
}

paddr_t
drmmmap(dev_t kdev, off_t offset, int prot)
{
	struct drm_device	*dev = drm_get_device_from_kdev(kdev);
	struct drm_local_map	*map;
	struct drm_file		*file_priv;
	enum drm_map_type	 type;

	if (dev == NULL)
		return (-1);

	DRM_LOCK();
	file_priv = drm_find_file_by_minor(dev, minor(kdev));
	DRM_UNLOCK();
	if (file_priv == NULL) {
		DRM_ERROR("can't find authenticator\n");
		return (-1);
	}

	if (!file_priv->authenticated)
		return (-1);

	if (dev->dma && offset >= 0 && offset < ptoa(dev->dma->page_count)) {
		struct drm_device_dma *dma = dev->dma;
		paddr_t	phys = -1;

		rw_enter_write(&dma->dma_lock);
		if (dma->pagelist != NULL)
			phys = atop(dma->pagelist[offset >> PAGE_SHIFT]);
		rw_exit_write(&dma->dma_lock);

		return (phys);
	}

	/*
	 * A sequential search of a linked list is
 	 * fine here because: 1) there will only be
	 * about 5-10 entries in the list and, 2) a
	 * DRI client only has to do this mapping
	 * once, so it doesn't have to be optimized
	 * for performance, even if the list was a
	 * bit longer.
	 */
	DRM_LOCK();
	TAILQ_FOREACH(map, &dev->maplist, link) {
		if (offset >= map->ext &&
		    offset < map->ext + map->size) {
			offset -= map->ext;
			break;
		}
	}

	if (map == NULL) {
		DRM_UNLOCK();
		DRM_DEBUG("can't find map\n");
		return (-1);
	}
	if (((map->flags & _DRM_RESTRICTED) && file_priv->master == 0)) {
		DRM_UNLOCK();
		DRM_DEBUG("restricted map\n");
		return (-1);
	}
	type = map->type;
	DRM_UNLOCK();

	switch (type) {
	case _DRM_FRAME_BUFFER:
	case _DRM_REGISTERS:
	case _DRM_AGP:
		return (atop(offset + map->offset));
		break;
	/* XXX unify all the bus_dmamem_mmap bits */
	case _DRM_SCATTER_GATHER:
		return (bus_dmamem_mmap(dev->dmat, dev->sg->mem->segs,
		    dev->sg->mem->nsegs, map->offset - dev->sg->handle +
		    offset, prot, BUS_DMA_NOWAIT));
	case _DRM_SHM:
	case _DRM_CONSISTENT:
		return (bus_dmamem_mmap(dev->dmat, map->dmamem->segs,
		    map->dmamem->nsegs, offset, prot, BUS_DMA_NOWAIT));
	default:
		DRM_ERROR("bad map type %d\n", type);
		return (-1);	/* This should never happen. */
	}
	/* NOTREACHED */
}

/*
 * Beginning in revision 1.1 of the DRM interface, getunique will return
 * a unique in the form pci:oooo:bb:dd.f (o=domain, b=bus, d=device, f=function)
 * before setunique has been called.  The format for the bus-specific part of
 * the unique is not defined for any other bus.
 */
int
drm_getunique(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_unique	 *u = data;

	if (u->unique_len >= dev->unique_len) {
		if (DRM_COPY_TO_USER(u->unique, dev->unique, dev->unique_len))
			return EFAULT;
	}
	u->unique_len = dev->unique_len;

	return 0;
}

#define DRM_IF_MAJOR	1
#define DRM_IF_MINOR	2

int
drm_version(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_version	*version = data;
	int			 len;

#define DRM_COPY(name, value)						\
	len = strlen( value );						\
	if ( len > name##_len ) len = name##_len;			\
	name##_len = strlen( value );					\
	if ( len && name ) {						\
		if ( DRM_COPY_TO_USER( name, value, len ) )		\
			return EFAULT;				\
	}

	version->version_major = dev->driver->major;
	version->version_minor = dev->driver->minor;
	version->version_patchlevel = dev->driver->patchlevel;

	DRM_COPY(version->name, dev->driver->name);
	DRM_COPY(version->date, dev->driver->date);
	DRM_COPY(version->desc, dev->driver->desc);

	return 0;
}

int
drm_setversion(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_set_version	ver, *sv = data;
	int			if_version;

	/* Save the incoming data, and set the response before continuing
	 * any further.
	 */
	ver = *sv;
	sv->drm_di_major = DRM_IF_MAJOR;
	sv->drm_di_minor = DRM_IF_MINOR;
	sv->drm_dd_major = dev->driver->major;
	sv->drm_dd_minor = dev->driver->minor;

	/*
	 * We no longer support interface versions less than 1.1, so error
	 * out if the xserver is too old. 1.1 always ties the drm to a
	 * certain busid, this was done on attach
	 */
	if (ver.drm_di_major != -1) {
		if (ver.drm_di_major != DRM_IF_MAJOR || ver.drm_di_minor < 1 ||
		    ver.drm_di_minor > DRM_IF_MINOR) {
			return EINVAL;
		}
		if_version = DRM_IF_VERSION(ver.drm_di_major, ver.drm_dd_minor);
		dev->if_version = imax(if_version, dev->if_version);
	}

	if (ver.drm_dd_major != -1) {
		if (ver.drm_dd_major != dev->driver->major ||
		    ver.drm_dd_minor < 0 ||
		    ver.drm_dd_minor > dev->driver->minor)
			return EINVAL;
	}

	return 0;
}

struct drm_dmamem *
drm_dmamem_alloc(bus_dma_tag_t dmat, bus_size_t size, bus_size_t alignment,
    int nsegments, bus_size_t maxsegsz, int mapflags, int loadflags)
{
	struct drm_dmamem	*mem;
	size_t			 strsize;
	/*
	 * segs is the last member of the struct since we modify the size 
	 * to allow extra segments if more than one are allowed.
	 */
	strsize = sizeof(*mem) + (sizeof(bus_dma_segment_t) * (nsegments - 1));
	mem = malloc(strsize, M_DRM, M_NOWAIT | M_ZERO);
	if (mem == NULL)
		return (NULL);

	mem->size = size;

	if (bus_dmamap_create(dmat, size, nsegments, maxsegsz, 0,
	    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, &mem->map) != 0)
		goto strfree;

	if (bus_dmamem_alloc(dmat, size, alignment, 0, mem->segs, nsegments,
	    &mem->nsegs, BUS_DMA_NOWAIT | BUS_DMA_ZERO) != 0)
		goto destroy;

	if (bus_dmamem_map(dmat, mem->segs, mem->nsegs, size, 
	    &mem->kva, BUS_DMA_NOWAIT | mapflags) != 0)
		goto free;

	if (bus_dmamap_load(dmat, mem->map, mem->kva, size,
	    NULL, BUS_DMA_NOWAIT | loadflags) != 0)
		goto unmap;

	return (mem);

unmap:
	bus_dmamem_unmap(dmat, mem->kva, size);
free:
	bus_dmamem_free(dmat, mem->segs, mem->nsegs);
destroy:
	bus_dmamap_destroy(dmat, mem->map);
strfree:
	free(mem, M_DRM);

	return (NULL);
}

void
drm_dmamem_free(bus_dma_tag_t dmat, struct drm_dmamem *mem)
{
	if (mem == NULL)
		return;

	bus_dmamap_unload(dmat, mem->map);
	bus_dmamem_unmap(dmat, mem->kva, mem->size);
	bus_dmamem_free(dmat, mem->segs, mem->nsegs);
	bus_dmamap_destroy(dmat, mem->map);
	free(mem, M_DRM);
}

/**
 * Called by the client, this returns a unique magic number to be authorized
 * by the master.
 *
 * The master may use its own knowledge of the client (such as the X
 * connection that the magic is passed over) to determine if the magic number
 * should be authenticated.
 */
int
drm_getmagic(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_auth		*auth = data;

	if (dev->magicid == 0)
		dev->magicid = 1;

	/* Find unique magic */
	if (file_priv->magic) {
		auth->magic = file_priv->magic;
	} else {
		DRM_LOCK();
		file_priv->magic = auth->magic = dev->magicid++;
		DRM_UNLOCK();
		DRM_DEBUG("%d\n", auth->magic);
	}

	DRM_DEBUG("%u\n", auth->magic);
	return (0);
}

/**
 * Marks the client associated with the given magic number as authenticated.
 */
int
drm_authmagic(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_file	*p;
	struct drm_auth	*auth = data;
	int		 ret = EINVAL;

	DRM_DEBUG("%u\n", auth->magic);

	if (auth->magic == 0)
		return (ret);

	DRM_LOCK();
	SPLAY_FOREACH(p, drm_file_tree, &dev->files) {
		if (p->magic == auth->magic) {
			p->authenticated = 1;
			p->magic = 0;
			ret = 0;
			break;
		}
	}
	DRM_UNLOCK();

	return (ret);
}

struct uvm_pagerops drm_pgops = {
	NULL,
	drm_ref,
	drm_unref,
	drm_fault,
	drm_flush,
};


void
drm_hold_object_locked(struct drm_obj *obj)
{
	while (obj->do_flags & DRM_BUSY) {
		atomic_setbits_int(&obj->do_flags, DRM_WANTED);
		simple_unlock(&uobj->vmobjlock);
#ifdef DRMLOCKDEBUG
		{
		int ret = 0;
		ret = tsleep(obj, PVM, "drm_hold", 3 * hz); /* XXX msleep */
		if (ret)
			printf("still waiting for obj %p, owned by %p\n",
			    obj, obj->holding_proc);
		}
#else
		tsleep(obj, PVM, "drm_hold", 0); /* XXX msleep */
#endif
		simple_lock(&uobj->vmobjlock);
	}
#ifdef DRMLOCKDEBUG
	obj->holding_proc = curproc;
#endif
	atomic_setbits_int(&obj->do_flags, DRM_BUSY);
}

void
drm_hold_object(struct drm_obj *obj)
{
	simple_lock(&obj->uobj->vmobjlock);
	drm_hold_object_locked(obj);
	simple_unlock(&obj->uobj->vmobjlock);
}

int
drm_try_hold_object(struct drm_obj *obj)
{
	simple_lock(&obj->uobj->vmobjlock);
	/* if the object is free, grab it */
	if (obj->do_flags & (DRM_BUSY | DRM_WANTED))
		return (0);
	atomic_setbits_int(&obj->do_flags, DRM_BUSY);
#ifdef DRMLOCKDEBUG
	obj->holding_proc = curproc;
#endif
	simple_unlock(&obj->uobj->vmobjlock);
	return (1);
}


void
drm_unhold_object_locked(struct drm_obj *obj)
{
	if (obj->do_flags & DRM_WANTED)
		wakeup(obj);
#ifdef DRMLOCKDEBUG
	obj->holding_proc = NULL;
#endif
	atomic_clearbits_int(&obj->do_flags, DRM_WANTED | DRM_BUSY);	
}

void
drm_unhold_object(struct drm_obj *obj)
{
	simple_lock(&obj->uobj->vmobjlock);
	drm_unhold_object_locked(obj);
	simple_unlock(&obj->uobj->vmobjlock);
}

void
drm_ref_locked(struct uvm_object *uobj)
{
	uobj->uo_refs++;
}

void
drm_ref(struct uvm_object *uobj)
{
	simple_lock(&uobj->vmobjlock);
	drm_ref_locked(uobj);
	simple_unlock(&uobj->vmobjlock);
}

void
drm_unref(struct uvm_object *uobj)
{
	simple_lock(&uobj->vmobjlock);
	drm_unref_locked(uobj);
}

void
drm_unref_locked(struct uvm_object *uobj)
{
	struct drm_obj		*obj = (struct drm_obj *)uobj;
	struct drm_device	*dev = obj->dev;

again:
	if (uobj->uo_refs > 1) {
		uobj->uo_refs--;
		simple_unlock(&uobj->vmobjlock);
		return;
	}

	/* inlined version of drm_hold because we want to trylock then sleep */
	if (obj->do_flags & DRM_BUSY) {
		atomic_setbits_int(&obj->do_flags, DRM_WANTED);
		simple_unlock(&uobj->vmobjlock);
		tsleep(obj, PVM, "drm_unref", 0); /* XXX msleep */
		simple_lock(&uobj->vmobjlock);
		goto again;
	}
#ifdef DRMLOCKDEBUG
	obj->holding_proc = curproc;
#endif
	atomic_setbits_int(&obj->do_flags, DRM_BUSY);
	simple_unlock(&obj->vmobjlock);
	/* We own this thing now. it is on no queues, though it may still
	 * be bound to the aperture (and on the inactive list, in which case
	 * idling the buffer is what triggered the free. Since we know no one 
	 * else can grab it now, we can nuke with impunity.
	 */
	if (dev->driver->gem_free_object != NULL)
		dev->driver->gem_free_object(obj);

	uao_detach(obj->uao);

	atomic_dec(&dev->obj_count);
	atomic_sub(obj->size, &dev->obj_memory);
	if (obj->do_flags & DRM_WANTED) /* should never happen, not on lists */
		wakeup(obj);
	pool_put(&dev->objpl, obj);
}

/*
 * convenience function to unreference and unhold an object.
 */
void
drm_unhold_and_unref(struct drm_obj *obj)
{
	drm_lock_obj(obj);
	drm_unhold_object_locked(obj);
	drm_unref_locked(&obj->uobj);
}


boolean_t	
drm_flush(struct uvm_object *uobj, voff_t start, voff_t stop, int flags)
{
	return (TRUE);
}


int
drm_fault(struct uvm_faultinfo *ufi, vaddr_t vaddr, vm_page_t *pps,
    int npages, int centeridx, vm_fault_t fault_type,
    vm_prot_t access_type, int flags)
{
	struct vm_map_entry *entry = ufi->entry;
	struct uvm_object *uobj = entry->object.uvm_obj;
	struct drm_obj *obj = (struct drm_obj *)uobj;
	struct drm_device *dev = obj->dev;
	int ret;
	UVMHIST_FUNC("udv_fault"); UVMHIST_CALLED(maphist);
	UVMHIST_LOG(maphist,"  flags=%ld", flags,0,0,0);

	/*
	 * we do not allow device mappings to be mapped copy-on-write
	 * so we kill any attempt to do so here.
	 */
	
	if (UVM_ET_ISCOPYONWRITE(entry)) {
		UVMHIST_LOG(maphist, "<- failed -- COW entry (etype=0x%lx)", 
		    entry->etype, 0,0,0);
		uvmfault_unlockall(ufi, ufi->entry->aref.ar_amap, uobj, NULL);
		return(VM_PAGER_ERROR);
	}

	/* Call down into driver to do the magic */
	ret = dev->driver->gem_fault(obj, ufi, entry->offset + (vaddr -
	    entry->start), vaddr, pps, npages, centeridx,
	    access_type, flags);
	return (ret);
}

/*
 * Code to support memory managers based on the GEM (Graphics
 * Execution Manager) api.
 */
struct drm_obj *
drm_gem_object_alloc(struct drm_device *dev, size_t size)
{
	struct drm_obj	*obj;

	KASSERT((size & (PAGE_SIZE -1)) == 0);

	if ((obj = pool_get(&dev->objpl, PR_WAITOK | PR_ZERO)) == NULL)
		return (NULL);

	obj->dev = dev;

	/* uao create can't fail in the 0 case, it just sleeps */
	obj->uao = uao_create(size, 0);
	obj->size = size;
	simple_lock_init(&obj->uobj.vmobjlock);
	obj->uobj.pgops = &drm_pgops;
	RB_INIT(&obj->uobj.memt);
	obj->uobj.uo_npages = 0;
	obj->uobj.uo_refs = 1;

	if (dev->driver->gem_init_object != NULL &&
	    dev->driver->gem_init_object(obj) != 0) {
		uao_detach(obj->uao);
		pool_put(&dev->objpl, obj);
		return (NULL);
	}
	atomic_inc(&dev->obj_count);
	atomic_add(obj->size, &dev->obj_memory);
	return (obj);
}

int
drm_handle_create(struct drm_file *file_priv, struct drm_obj *obj,
    int *handlep)
{
	struct drm_handle	*han;

	if ((han = drm_calloc(1, sizeof(*han))) == NULL)
		return (ENOMEM);

	han->obj = obj;
	mtx_enter(&file_priv->table_lock);
again:
	*handlep = han->handle = ++file_priv->obj_id;
	/*
	 * Make sure we have no duplicates. this'll hurt once we wrap, 0 is
	 * reserved.
	 */
	if (han->handle == 0 || SPLAY_INSERT(drm_obj_tree,
	    &file_priv->obj_tree, han))
		goto again;
	mtx_leave(&file_priv->table_lock);
	
	drm_handle_ref(obj);
	return (0);
}

struct drm_obj *
drm_gem_object_lookup(struct drm_device *dev, struct drm_file *file_priv,
    int handle)
{
	struct drm_obj		*obj;
	struct drm_handle	*han, search;

	search.handle = handle;

	mtx_enter(&file_priv->table_lock);
	han = SPLAY_FIND(drm_obj_tree, &file_priv->obj_tree, &search);
	if (han == NULL) {
		mtx_leave(&file_priv->table_lock);
		return (NULL);
	}

	obj = han->obj;
	drm_ref(&obj->uobj);
	mtx_leave(&file_priv->table_lock);

	return (obj);
}

int
drm_gem_close_ioctl(struct drm_device *dev, void *data,
    struct drm_file *file_priv)
{
	struct drm_gem_close	*args = data;
	struct drm_handle	*han, find;
	struct drm_obj		*obj;

	if ((dev->driver->flags & DRIVER_GEM) == 0)
		return (ENODEV);

	find.handle = args->handle;
	mtx_enter(&file_priv->table_lock);
	han = SPLAY_FIND(drm_obj_tree, &file_priv->obj_tree, &find);
	if (han == NULL) {
		mtx_leave(&file_priv->table_lock);
		return (EINVAL);
	}

	obj = han->obj;
	SPLAY_REMOVE(drm_obj_tree, &file_priv->obj_tree, han);
	mtx_leave(&file_priv->table_lock);

	drm_free(han);

	DRM_LOCK();
	drm_handle_unref(obj);
	DRM_UNLOCK();

	return (0);
}

int
drm_gem_flink_ioctl(struct drm_device *dev, void *data,
    struct drm_file *file_priv)
{
	struct drm_gem_flink	*args = data;
	struct drm_obj		*obj;

	if (!dev->driver->flags & DRIVER_GEM)
		return (ENODEV);

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);
	if (obj == NULL)
		return (EBADF);

	mtx_enter(&dev->obj_name_lock);
	if (!obj->name) {
again:
		obj->name = ++dev->obj_name; 
		/* 0 is reserved, make sure we don't clash. */
		if (obj->name == 0 || SPLAY_INSERT(drm_name_tree,
		    &dev->name_tree, obj))
			goto again;
		/* name holds a reference to the object */
		drm_ref(&obj->uobj);
	}
	mtx_leave(&dev->obj_name_lock);

	args->name = (uint64_t)obj->name;

	drm_unref(&obj->uobj);

	return (0);
}

int
drm_gem_open_ioctl(struct drm_device *dev, void *data,
    struct drm_file *file_priv)
{
	struct drm_gem_open	*args = data;
	struct drm_obj		*obj, search;
	int			 ret, handle;

	if (!dev->driver->flags & DRIVER_GEM)
		return (ENODEV);

	search.name = args->name;
	mtx_enter(&dev->obj_name_lock);
	obj = SPLAY_FIND(drm_name_tree, &dev->name_tree, &search);
	if (obj != NULL)
		drm_ref(&obj->uobj);
	mtx_leave(&dev->obj_name_lock);
	if (obj == NULL)
		return (ENOENT);

	/* this gives our reference to the handle */
	ret = drm_handle_create(file_priv, obj, &handle);
	if (ret) {
		drm_unref(&obj->uobj);
		return (ret);
	}

	args->handle = handle;
	args->size = obj->size;

        return (0);
}

/*
 * grab a reference for a per-open handle. 
 * The object contains a handlecount too because if all handles disappear we
 * need to also remove the global name (names initially are per open unless the
 * flink ioctl is called.
 */
void
drm_handle_ref(struct drm_obj *obj)
{
	/* we are given the reference from the caller, so just
	 * crank handlecount.
	 */
	obj->handlecount++;
}

/*
 * Remove the reference owned by a per-open handle. If we're the last one,
 * remove the reference from flink, too.
 */
void
drm_handle_unref(struct drm_obj *obj)
{
	/* do this first in case this is the last reference */
	if (--obj->handlecount == 0) {
		struct drm_device	*dev = obj->dev;

		mtx_enter(&dev->obj_name_lock);
		if (obj->name) {
			SPLAY_REMOVE(drm_name_tree, &dev->name_tree, obj);
			obj->name = 0;
			mtx_leave(&dev->obj_name_lock);
			/* name held a reference to object */
			drm_unref(&obj->uobj);
		} else {
			mtx_leave(&dev->obj_name_lock);
		}
	}
	drm_unref(&obj->uobj);
}

/*
 * Helper function to load a uvm anonymous object into a dmamap, to be used
 * for binding to a translation-table style sg mechanism (e.g. agp, or intel
 * gtt).
 *
 * For now we ignore maxsegsz.
 */
int
drm_gem_load_uao(bus_dma_tag_t dmat, bus_dmamap_t map, struct uvm_object *uao,
    bus_size_t size, int flags, bus_dma_segment_t **segp)
{
	bus_dma_segment_t	*segs;
	struct vm_page		*pg;
	struct pglist		 plist;
	u_long			 npages = size >> PAGE_SHIFT, i = 0;
	int			 ret;

	TAILQ_INIT(&plist);

	/*
	 * This is really quite ugly, but nothing else would need
	 * bus_dmamap_load_uao() yet.
	 */
	segs = malloc(npages * sizeof(*segs), M_DRM, M_WAITOK | M_ZERO);
	if (segs == NULL)
		return (ENOMEM);

	/* This may sleep, no choice in the matter */
	if (uvm_objwire(uao, 0, size, &plist) != 0) {
		ret = ENOMEM;
		goto free;
	}

	TAILQ_FOREACH(pg, &plist, pageq) {
		paddr_t pa = VM_PAGE_TO_PHYS(pg);
		
		if (i > 0 && pa == (segs[i - 1].ds_addr +
		    segs[i - 1].ds_len)) {
			/* contiguous, yay */
			segs[i - 1].ds_len += PAGE_SIZE;
			continue;
		}
		segs[i].ds_addr = pa;
		segs[i].ds_len = PAGE_SIZE;
		if (i++ > npages)
			break;
	}
	/* this should be impossible */
	if (pg != TAILQ_END(&pageq)) {
		ret = EINVAL;
		goto unwire;
	}

	if ((ret = bus_dmamap_load_raw(dmat, map, segs, i, size, flags)) != 0)
		goto unwire;

	*segp = segs;

	return (0);

unwire:
	uvm_objunwire(uao, 0, size);
free:
	free(segs, M_DRM);
	return (ret);
}

int
drm_handle_cmp(struct drm_handle *a, struct drm_handle *b)
{
	return (a->handle < b->handle ? -1 : a->handle > b->handle);
}

int
drm_name_cmp(struct drm_obj *a, struct drm_obj *b)
{
	return (a->name < b->name ? -1 : a->name > b->name);
}

SPLAY_GENERATE(drm_obj_tree, drm_handle, entry, drm_handle_cmp);

SPLAY_GENERATE(drm_name_tree, drm_obj, entry, drm_name_cmp);
