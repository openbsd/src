/*-
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
 *    Gareth Hughes <gareth@valinux.com>
 *
 */

/** @file drm_drv.c
 * The catch-all file for DRM device support, including module setup/teardown,
 * open/close, and ioctl dispatch.
 */

#include <sys/limits.h>
#include "drmP.h"
#include "drm.h"
#include "drm_sarea.h"
#include <sys/ttycom.h> /* for TIOCSGRP */

#ifdef DRM_DEBUG_DEFAULT_ON
int drm_debug_flag = 1;
#else
int drm_debug_flag = 0;
#endif

int	 drm_load(struct drm_device *);
void	 drm_unload(struct drm_device *);
drm_pci_id_list_t *drm_find_description(int , int ,
	    drm_pci_id_list_t *);
int	 drm_firstopen(struct drm_device *);
int	 drm_lastclose(struct drm_device *);

static drm_ioctl_desc_t		  drm_ioctls[256] = {
	DRM_IOCTL_DEF(DRM_IOCTL_VERSION, drm_version, 0),
	DRM_IOCTL_DEF(DRM_IOCTL_GET_UNIQUE, drm_getunique, 0),
	DRM_IOCTL_DEF(DRM_IOCTL_GET_MAGIC, drm_getmagic, 0),
	DRM_IOCTL_DEF(DRM_IOCTL_IRQ_BUSID, drm_irq_by_busid, DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF(DRM_IOCTL_GET_MAP, drm_getmap, 0),
	DRM_IOCTL_DEF(DRM_IOCTL_GET_CLIENT, drm_getclient, 0),
	DRM_IOCTL_DEF(DRM_IOCTL_GET_STATS, drm_getstats, 0),
	DRM_IOCTL_DEF(DRM_IOCTL_SET_VERSION, drm_setversion, DRM_MASTER|DRM_ROOT_ONLY),

	DRM_IOCTL_DEF(DRM_IOCTL_SET_UNIQUE, drm_setunique, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF(DRM_IOCTL_BLOCK, drm_noop, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF(DRM_IOCTL_UNBLOCK, drm_noop, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF(DRM_IOCTL_AUTH_MAGIC, drm_authmagic, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),

	DRM_IOCTL_DEF(DRM_IOCTL_ADD_MAP, drm_addmap_ioctl, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF(DRM_IOCTL_RM_MAP, drm_rmmap_ioctl, DRM_AUTH),

	DRM_IOCTL_DEF(DRM_IOCTL_SET_SAREA_CTX, drm_setsareactx, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF(DRM_IOCTL_GET_SAREA_CTX, drm_getsareactx, DRM_AUTH),

	DRM_IOCTL_DEF(DRM_IOCTL_ADD_CTX, drm_addctx, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF(DRM_IOCTL_RM_CTX, drm_rmctx, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF(DRM_IOCTL_MOD_CTX, drm_modctx, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF(DRM_IOCTL_GET_CTX, drm_getctx, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_IOCTL_SWITCH_CTX, drm_switchctx, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF(DRM_IOCTL_NEW_CTX, drm_newctx, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF(DRM_IOCTL_RES_CTX, drm_resctx, DRM_AUTH),

	DRM_IOCTL_DEF(DRM_IOCTL_ADD_DRAW, drm_adddraw, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF(DRM_IOCTL_RM_DRAW, drm_rmdraw, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),

	DRM_IOCTL_DEF(DRM_IOCTL_LOCK, drm_lock, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_IOCTL_UNLOCK, drm_unlock, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_IOCTL_FINISH, drm_noop, DRM_AUTH),

	DRM_IOCTL_DEF(DRM_IOCTL_ADD_BUFS, drm_addbufs_ioctl, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF(DRM_IOCTL_MARK_BUFS, drm_markbufs, DRM_AUTH|DRM_MASTER),
	DRM_IOCTL_DEF(DRM_IOCTL_INFO_BUFS, drm_infobufs, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_IOCTL_MAP_BUFS, drm_mapbufs, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_IOCTL_FREE_BUFS, drm_freebufs, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_IOCTL_DMA, drm_dma, DRM_AUTH),

	DRM_IOCTL_DEF(DRM_IOCTL_CONTROL, drm_control, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),

	DRM_IOCTL_DEF(DRM_IOCTL_AGP_ACQUIRE, drm_agp_acquire_ioctl, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF(DRM_IOCTL_AGP_RELEASE, drm_agp_release_ioctl, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF(DRM_IOCTL_AGP_ENABLE, drm_agp_enable_ioctl, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF(DRM_IOCTL_AGP_INFO, drm_agp_info_ioctl, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_IOCTL_AGP_ALLOC, drm_agp_alloc_ioctl, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF(DRM_IOCTL_AGP_FREE, drm_agp_free_ioctl, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF(DRM_IOCTL_AGP_BIND, drm_agp_bind_ioctl, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF(DRM_IOCTL_AGP_UNBIND, drm_agp_unbind_ioctl, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),

	DRM_IOCTL_DEF(DRM_IOCTL_SG_ALLOC, drm_sg_alloc_ioctl, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF(DRM_IOCTL_SG_FREE, drm_sg_free, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),

	DRM_IOCTL_DEF(DRM_IOCTL_WAIT_VBLANK, drm_wait_vblank, 0),
	DRM_IOCTL_DEF(DRM_IOCTL_MODESET_CTL, drm_modeset_ctl, 0),
	DRM_IOCTL_DEF(DRM_IOCTL_UPDATE_DRAW, drm_update_draw, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
};

struct drm_device *drm_units[DRM_MAXUNITS];

static int init_units = 1;

int
drm_probe(struct pci_attach_args *pa, drm_pci_id_list_t *idlist)
{
	int unit;
	drm_pci_id_list_t *id_entry;

	/* first make sure there is place for the device */
	for(unit=0; unit<DRM_MAXUNITS; unit++)
		if(drm_units[unit] == NULL) break;
	if(unit == DRM_MAXUNITS) return 0;

	id_entry = drm_find_description(PCI_VENDOR(pa->pa_id),
	    PCI_PRODUCT(pa->pa_id), idlist);
	if (id_entry != NULL) {
		return 1;
	}

	return 0;
}

void
drm_attach(struct device *parent, struct device *kdev,
    struct pci_attach_args *pa, drm_pci_id_list_t *idlist)
{
	int unit;
	struct drm_device *dev;
	drm_pci_id_list_t *id_entry;

	if(init_units) {
		for(unit=0; unit<DRM_MAXUNITS;unit++)
			drm_units[unit] = NULL;
		init_units = 0;
	}


        for(unit=0; unit<DRM_MAXUNITS; unit++)
		if(drm_units[unit] == NULL) break;
	if(unit == DRM_MAXUNITS) return;

	dev = drm_units[unit] = (struct drm_device*)kdev;
	dev->unit = unit;

	/* needed for pci_mapreg_* */
	memcpy(&dev->pa, pa, sizeof(dev->pa));
	dev->vga_softc = (struct vga_pci_softc *)parent;

	dev->irq = pa->pa_intrline;
	dev->pci_bus = pa->pa_bus;
	dev->pci_slot = pa->pa_device;
	dev->pci_func = pa->pa_function;
	DRM_SPININIT(&dev->dev_lock, "drm device");
	DRM_SPININIT(&dev->drw_lock, "drm drawable lock");

	id_entry = drm_find_description(PCI_VENDOR(pa->pa_id),
	    PCI_PRODUCT(pa->pa_id), idlist);
	dev->id_entry = id_entry;
	dev->driver.id_entry = id_entry;

	printf("\n");
	DRM_INFO("%s (unit %d)\n", id_entry->name, dev->unit);
	drm_load(dev);
}

int
drm_detach(struct device *self, int flags)
{
	drm_unload((struct drm_device *)self);
	return 0;
}

int
drm_activate(struct device *self, enum devact act)
{
	switch (act) {
	case DVACT_ACTIVATE:
		return (EOPNOTSUPP);
		break;

	case DVACT_DEACTIVATE:
		/* FIXME */
		break;
	}
	return (0);
}

drm_pci_id_list_t *
drm_find_description(int vendor, int device, drm_pci_id_list_t *idlist)
{
	int i = 0;
	
	for (i = 0; idlist[i].vendor != 0; i++) {
		if ((idlist[i].vendor == vendor) &&
		    (idlist[i].device == device)) {
			return &idlist[i];
		}
	}
	return NULL;
}

int
drm_firstopen(struct drm_device *dev)
{
	drm_local_map_t *map;
	int i;

	DRM_SPINLOCK_ASSERT(&dev->dev_lock);

	/* prebuild the SAREA */
	i = drm_addmap(dev, 0, SAREA_MAX, _DRM_SHM,
	    _DRM_CONTAINS_LOCK, &map);
	if (i != 0)
		return i;

	if (dev->driver.firstopen)
		dev->driver.firstopen(dev);

	dev->buf_use = 0;

	if (dev->driver.use_dma) {
		i = drm_dma_setup(dev);
		if (i != 0)
			return i;
	}

	dev->counters = 6;
	dev->types[0] = _DRM_STAT_LOCK;
	dev->types[1] = _DRM_STAT_OPENS;
	dev->types[2] = _DRM_STAT_CLOSES;
	dev->types[3] = _DRM_STAT_IOCTLS;
	dev->types[4] = _DRM_STAT_LOCKS;
	dev->types[5] = _DRM_STAT_UNLOCKS;

	for ( i = 0 ; i < DRM_ARRAY_SIZE(dev->counts) ; i++ )
		atomic_set( &dev->counts[i], 0 );

	SPLAY_INIT(&dev->magiclist);

	dev->lock.lock_queue = 0;
	dev->irq_enabled = 0;
	dev->context_flag = 0;
	dev->last_context = 0;
	dev->if_version = 0;

	dev->buf_pgid = 0;

	DRM_DEBUG( "\n" );

	return 0;
}

int
drm_lastclose(struct drm_device *dev)
{
	struct drm_magic_entry *pt;
	drm_local_map_t *map, *mapsave;

	DRM_SPINLOCK_ASSERT(&dev->dev_lock);

	DRM_DEBUG( "\n" );

	if (dev->driver.lastclose != NULL)
		dev->driver.lastclose(dev);

	if (dev->irq_enabled)
		drm_irq_uninstall(dev);

	if ( dev->unique ) {
		free(dev->unique, M_DRM);
		dev->unique = NULL;
		dev->unique_len = 0;
	}

	drm_drawable_free_all(dev);
				/* Clear pid list */
	while ((pt = SPLAY_ROOT(&dev->magiclist)) != NULL) {
		SPLAY_REMOVE(drm_magic_tree, &dev->magiclist, pt);
		free(pt, M_DRM);
	}

				/* Clear AGP information */
	if ( dev->agp ) {
		struct drm_agp_mem *entry;

		/* Remove AGP resources, but leave dev->agp intact until
		 * drm_unload is called.
		 */
		while ((entry = TAILQ_FIRST(&dev->agp->memory)) != NULL) {
			if ( entry->bound )
				drm_agp_unbind_memory(entry->handle);
			drm_agp_free_memory(entry->handle);
			TAILQ_REMOVE(&dev->agp->memory, entry, link);
			free(entry, M_DRM);
		}

		if (dev->agp->acquired)
			drm_agp_release(dev);

		dev->agp->acquired = 0;
		dev->agp->enabled  = 0;
	}
	if (dev->sg != NULL) {
		drm_sg_cleanup(dev->sg);
		dev->sg = NULL;
	}

	for (map = TAILQ_FIRST(&dev->maplist); map != TAILQ_END(&dev->maplist);
	    map = mapsave) {
		mapsave = TAILQ_NEXT(map, link);
		if (!(map->flags & _DRM_DRIVER))
			drm_rmmap(dev, map);
	}

	drm_dma_takedown(dev);
	if ( dev->lock.hw_lock ) {
		dev->lock.hw_lock = NULL; /* SHM removed */
		dev->lock.file_priv = NULL;
		DRM_WAKEUP_INT((void *)&dev->lock.lock_queue);
	}

	return 0;
}

int
drm_load(struct drm_device *dev)
{
	int retcode;

	DRM_DEBUG( "\n" );

	dev->irq = dev->pa.pa_intrline;
	dev->pci_domain = 0;
	dev->pci_bus = dev->pa.pa_bus;
	dev->pci_slot = dev->pa.pa_device;
	dev->pci_func = dev->pa.pa_function;

	dev->pci_vendor = PCI_VENDOR(dev->pa.pa_id);
	dev->pci_device = PCI_PRODUCT(dev->pa.pa_id);

	TAILQ_INIT(&dev->maplist);

	drm_mem_init();
	TAILQ_INIT(&dev->files);

	/*
	 * the dma buffers api is just weird. offset 1Gb to ensure we don't
	 * conflict with it.
	 */
	retcode = drm_memrange_init(&dev->handle_mm, 1024*1024*1024, LONG_MAX);
	if (retcode != 0) {
		DRM_ERROR("Failed to initialise handle memrange\n");
		goto error;
	}

	if (dev->driver.load != NULL) {
		DRM_LOCK();
		/* Shared code returns -errno. */
		retcode = -dev->driver.load(dev,
		    dev->id_entry->driver_private);
		DRM_UNLOCK();
		if (retcode != 0)
			goto error;
	}

	if (dev->driver.use_agp) {
		if (drm_device_is_agp(dev))
			dev->agp = drm_agp_init();
		if (dev->driver.require_agp && dev->agp == NULL) {
			DRM_ERROR("Card isn't AGP, or couldn't initialize "
			    "AGP.\n");
			retcode = ENOMEM;
			goto error;
		}
#ifndef DRM_NO_MTRR
		if (dev->agp != NULL) {
			if (drm_mtrr_add(dev->agp->info.ai_aperture_base,
			    dev->agp->info.ai_aperture_size, DRM_MTRR_WC) == 0)
				dev->agp->mtrr = 1;
		}
#endif
	}

	retcode = drm_ctxbitmap_init(dev);
	if (retcode != 0) {
		DRM_ERROR("Cannot allocate memory for context bitmap.\n");
		goto error;
	}
	DRM_INFO("Initialized %s %d.%d.%d %s\n",
	    dev->driver.name,
	    dev->driver.major,
	    dev->driver.minor,
	    dev->driver.patchlevel,
	    dev->driver.date);

	return 0;

error:
	DRM_LOCK();
	drm_lastclose(dev);
	DRM_UNLOCK();
	DRM_SPINUNINIT(&dev->dev_lock);
	return retcode;
}

void
drm_unload(struct drm_device *dev)
{

	DRM_DEBUG( "\n" );

	drm_ctxbitmap_cleanup(dev);

	drm_memrange_takedown(&dev->handle_mm);

#if !defined(DRM_NO_MTRR) && !defined(DRM_NO_AGP)
	if (dev->agp && dev->agp->mtrr) {
		int retcode;

		retcode = drm_mtrr_del(0, dev->agp->info.ai_aperture_base,
		    dev->agp->info.ai_aperture_size, DRM_MTRR_WC);
		DRM_DEBUG("mtrr_del = %d", retcode);
	}
#endif

	DRM_LOCK();
	drm_lastclose(dev);
	DRM_UNLOCK();

	if ( dev->agp ) {
		free(dev->agp, M_DRM);
		dev->agp = NULL;
	}

	if (dev->driver.unload != NULL)
		dev->driver.unload(dev);

	drm_mem_uninit();
	DRM_SPINUNINIT(&dev->dev_lock);
}


int
drm_version(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	drm_version_t *version = data;
	int len;

#define DRM_COPY( name, value )						\
	len = strlen( value );						\
	if ( len > name##_len ) len = name##_len;			\
	name##_len = strlen( value );					\
	if ( len && name ) {						\
		if ( DRM_COPY_TO_USER( name, value, len ) )		\
			return EFAULT;				\
	}

	version->version_major		= dev->driver.major;
	version->version_minor		= dev->driver.minor;
	version->version_patchlevel	= dev->driver.patchlevel;

	DRM_COPY(version->name, dev->driver.name);
	DRM_COPY(version->date, dev->driver.date);
	DRM_COPY(version->desc, dev->driver.desc);

	return 0;
}

int
drmopen(DRM_CDEV kdev, int flags, int fmt, DRM_STRUCTPROC *p)
{
	struct drm_device *dev = NULL;
	int retcode = 0;

	dev = drm_get_device_from_kdev(kdev);
	if (dev == NULL)
		return (ENXIO);

	dev->kdev = kdev; /* hack for now */

	DRM_DEBUG( "open_count = %d\n", dev->open_count );

	retcode = drm_open_helper(kdev, flags, fmt, p, dev);

	if ( !retcode ) {
		atomic_inc( &dev->counts[_DRM_STAT_OPENS] );
		DRM_LOCK();
		if ( !dev->open_count++ )
			retcode = drm_firstopen(dev);
		DRM_UNLOCK();
	}

	return retcode;
}

int
drmclose(DRM_CDEV kdev, int flags, int fmt, DRM_STRUCTPROC *p)
{
	struct drm_device *dev = drm_get_device_from_kdev(kdev);
	struct drm_file *file_priv;
	int retcode = 0;

	DRM_DEBUG( "open_count = %d\n", dev->open_count );

	DRM_LOCK();

	file_priv = drm_find_file_by_minor(dev, minor(kdev));
	if (!file_priv) {
		DRM_UNLOCK();
		DRM_ERROR("can't find authenticator\n");
		retcode = EINVAL;
		goto done;
	}

	if (--file_priv->refs != 0)
		goto done;

	if (dev->driver.preclose != NULL)
		dev->driver.preclose(dev, file_priv);

	/* ========================================================
	 * Begin inline drm_release
	 */

	DRM_DEBUG( "pid = %d, device = 0x%lx, open_count = %d\n",
	    DRM_CURRENTPID, (long)&dev->device, dev->open_count);

	if (dev->lock.hw_lock && _DRM_LOCK_IS_HELD(dev->lock.hw_lock->lock)
	    && dev->lock.file_priv == file_priv) {
		DRM_DEBUG("Process %d dead, freeing lock for context %d\n",
		    DRM_CURRENTPID,
		    _DRM_LOCKING_CONTEXT(dev->lock.hw_lock->lock));
		if (dev->driver.reclaim_buffers_locked != NULL)
			dev->driver.reclaim_buffers_locked(dev, file_priv);

		drm_lock_free(dev, &dev->lock.hw_lock->lock,
		    _DRM_LOCKING_CONTEXT(dev->lock.hw_lock->lock));
		
				/* FIXME: may require heavy-handed reset of
                                   hardware at this point, possibly
                                   processed via a callback to the X
                                   server. */
	} else if (dev->driver.reclaim_buffers_locked != NULL &&
	    dev->lock.hw_lock != NULL) {
		/* The lock is required to reclaim buffers */
		for (;;) {
			if ( !dev->lock.hw_lock ) {
				/* Device has been unregistered */
				retcode = EINTR;
				break;
			}
			if (drm_lock_take(&dev->lock.hw_lock->lock,
			    DRM_KERNEL_CONTEXT)) {
				dev->lock.file_priv = file_priv;
				dev->lock.lock_time = jiffies;
                                atomic_inc( &dev->counts[_DRM_STAT_LOCKS] );
				break;	/* Got lock */
			}
				/* Contention */
			retcode = DRM_SLEEPLOCK((void *)&dev->lock.lock_queue,
			    &dev->dev_lock, PZERO | PCATCH, "drmlk2", 0);
			if (retcode)
				break;
		}
		if (retcode == 0) {
			dev->driver.reclaim_buffers_locked(dev, file_priv);
			drm_lock_free(dev, &dev->lock.hw_lock->lock,
			    DRM_KERNEL_CONTEXT);
		}
	}

	if (dev->driver.use_dma && !dev->driver.reclaim_buffers_locked)
		drm_reclaim_buffers(dev, file_priv);

	dev->buf_pgid = 0;

	if (dev->driver.postclose != NULL)
		dev->driver.postclose(dev, file_priv);
	TAILQ_REMOVE(&dev->files, file_priv, link);
	free(file_priv, M_DRM);

	/* ========================================================
	 * End inline drm_release
	 */

done:
	atomic_inc( &dev->counts[_DRM_STAT_CLOSES] );
	if (--dev->open_count == 0) {
		retcode = drm_lastclose(dev);
	}

	DRM_UNLOCK();
	
	return retcode;
}

/* drmioctl is called whenever a process performs an ioctl on /dev/drm.
 */
int
drmioctl(DRM_CDEV kdev, u_long cmd, caddr_t data, int flags, 
    DRM_STRUCTPROC *p)
{
	struct drm_device *dev = drm_get_device_from_kdev(kdev);
	int retcode = 0;
	drm_ioctl_desc_t *ioctl;
	int (*func)(struct drm_device *, void *, struct drm_file *);
	int nr = DRM_IOCTL_NR(cmd);
	int is_driver_ioctl = 0;
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

	atomic_inc( &dev->counts[_DRM_STAT_IOCTLS] );
	++file_priv->ioctl_count;

	DRM_DEBUG( "pid=%d, cmd=0x%02lx, nr=0x%02x, dev 0x%lx, auth=%d\n",
	    DRM_CURRENTPID, cmd, nr, (long)&dev->device,
	    file_priv->authenticated );

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
	}

	if (IOCGROUP(cmd) != DRM_IOCTL_BASE) {
		DRM_DEBUG("Bad ioctl group 0x%x\n", (int)IOCGROUP(cmd));
		return EINVAL;
	}

	ioctl = &drm_ioctls[nr];
	/* It's not a core DRM ioctl, try driver-specific. */
	if (ioctl->func == NULL && nr >= DRM_COMMAND_BASE) {
		/* The array entries begin at DRM_COMMAND_BASE ioctl nr */
		nr -= DRM_COMMAND_BASE;
		if (nr > dev->driver.max_ioctl) {
			DRM_DEBUG("Bad driver ioctl number, 0x%x (of 0x%x)\n",
			    nr, dev->driver.max_ioctl);
			return EINVAL;
		}
		ioctl = &dev->driver.ioctls[nr];
		is_driver_ioctl = 1;
	}
	func = ioctl->func;

	if (func == NULL) {
		DRM_DEBUG( "no function\n" );
		return EINVAL;
	}

	/* 
	 * master must be root, and all ioctls that are ROOT_ONLY are
	 * also DRM_MASTER.
	 */
	if (((ioctl->flags & DRM_AUTH) && !file_priv->authenticated) ||
	    ((ioctl->flags & DRM_MASTER) && !file_priv->master))
		return EACCES;

	if (is_driver_ioctl) {
		DRM_LOCK();
		/* shared code returns -errno */
		retcode = -func(dev, data, file_priv);
		DRM_UNLOCK();
	} else {
		retcode = func(dev, data, file_priv);
	}

	if (retcode != 0)
		DRM_DEBUG("    returning %d\n", retcode);

	return retcode;
}

drm_local_map_t *
drm_getsarea(struct drm_device *dev)
{
	drm_local_map_t *map;

	DRM_SPINLOCK_ASSERT(&dev->dev_lock);
	TAILQ_FOREACH(map, &dev->maplist, link) {
		if (map->type == _DRM_SHM && (map->flags & _DRM_CONTAINS_LOCK))
			return map;
	}

	return NULL;
}
