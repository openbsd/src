/* $OpenBSD: drm_drv.c,v 1.134 2015/04/15 09:48:18 kettenis Exp $ */
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
#include <sys/fcntl.h>
#include <sys/filio.h>
#include <sys/limits.h>
#include <sys/poll.h>
#include <sys/specdev.h>
#include <sys/systm.h>
#include <sys/ttycom.h> /* for TIOCSGRP */
#include <sys/vnode.h>

#include <uvm/uvm.h>
#include <uvm/uvm_device.h>

#include "drmP.h"
#include "drm.h"
#include "drm_sarea.h"

#ifdef DRMDEBUG
int drm_debug_flag = 1;
#endif

struct drm_device *drm_get_device_from_kdev(dev_t);
int	 drm_firstopen(struct drm_device *);
int	 drm_lastclose(struct drm_device *);
void	 drm_attach(struct device *, struct device *, void *);
int	 drm_probe(struct device *, void *, void *);
int	 drm_detach(struct device *, int);
void	 drm_quiesce(struct drm_device *);
void	 drm_wakeup(struct drm_device *);
int	 drm_activate(struct device *, int);
int	 drmprint(void *, const char *);
int	 drmsubmatch(struct device *, void *, void *);
int	 drm_do_ioctl(struct drm_device *, int, u_long, caddr_t);
int	 drm_dequeue_event(struct drm_device *, struct drm_file *, size_t,
	     struct drm_pending_event **);

int	 drm_getunique(struct drm_device *, void *, struct drm_file *);
int	 drm_version(struct drm_device *, void *, struct drm_file *);
int	 drm_setversion(struct drm_device *, void *, struct drm_file *);
int	 drm_getmagic(struct drm_device *, void *, struct drm_file *);
int	 drm_authmagic(struct drm_device *, void *, struct drm_file *);
int	 drm_file_cmp(struct drm_file *, struct drm_file *);
SPLAY_PROTOTYPE(drm_file_tree, drm_file, link, drm_file_cmp);

/* functions used by the per-open handle  code to grab references to object */
void	 drm_gem_object_handle_reference(struct drm_gem_object *);
void	 drm_gem_object_handle_unreference(struct drm_gem_object *);
void	 drm_gem_object_handle_unreference_unlocked(struct drm_gem_object *);

int	 drm_handle_cmp(struct drm_handle *, struct drm_handle *);
int	 drm_name_cmp(struct drm_gem_object *, struct drm_gem_object *);
int	 drm_fault(struct uvm_faultinfo *, vaddr_t, vm_page_t *, int, int,
	     vm_fault_t, vm_prot_t, int);
boolean_t	 drm_flush(struct uvm_object *, voff_t, voff_t, int);

SPLAY_PROTOTYPE(drm_obj_tree, drm_handle, entry, drm_handle_cmp);
SPLAY_PROTOTYPE(drm_name_tree, drm_gem_object, entry, drm_name_cmp);

int	 drm_getcap(struct drm_device *, void *, struct drm_file *);

/*
 * attach drm to a pci-based driver.
 *
 * This function does all the pci-specific calculations for the 
 * drm_attach_args.
 */
struct device *
drm_attach_pci(struct drm_driver_info *driver, struct pci_attach_args *pa,
    int is_agp, int console, struct device *dev)
{
	struct drm_attach_args arg;
	pcireg_t subsys;

	arg.driver = driver;
	arg.dmat = pa->pa_dmat;
	arg.bst = pa->pa_memt;
	arg.irq = pa->pa_intrline;
	arg.is_agp = is_agp;
	arg.console = console;

	arg.pci_vendor = PCI_VENDOR(pa->pa_id);
	arg.pci_device = PCI_PRODUCT(pa->pa_id);

	subsys = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_SUBSYS_ID_REG);
	arg.pci_subvendor = PCI_VENDOR(subsys);
	arg.pci_subdevice = PCI_PRODUCT(subsys);

	arg.pc = pa->pa_pc;
	arg.bridgetag = pa->pa_bridgetag;

	arg.busid_len = 20;
	arg.busid = malloc(arg.busid_len + 1, M_DRM, M_NOWAIT);
	if (arg.busid == NULL) {
		printf("%s: no memory for drm\n", dev->dv_xname);
		return (NULL);
	}
	snprintf(arg.busid, arg.busid_len, "pci:%04x:%02x:%02x.%1x",
	    pa->pa_domain, pa->pa_bus, pa->pa_device, pa->pa_function);

	return (config_found_sm(dev, &arg, drmprint, drmsubmatch));
}

int
drmprint(void *aux, const char *pnp)
{
	if (pnp != NULL)
		printf("drm at %s", pnp);
	return (UNCONF);
}

int
drmsubmatch(struct device *parent, void *match, void *aux)
{
	extern struct cfdriver drm_cd;
	struct cfdata *cf = match;

	/* only allow drm to attach */
	if (cf->cf_driver == &drm_cd)
		return ((*cf->cf_attach->ca_match)(parent, match, aux));
	return (0);
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
	struct cfdata *cf = match;
	struct drm_attach_args *da = aux;

	if (cf->drmdevcf_console != DRMDEVCF_CONSOLE_UNK) {
		/*
		 * If console-ness of device specified, either match
		 * exactly (at high priority), or fail.
		 */
		if (cf->drmdevcf_console != 0 && da->console != 0)
			return (10);
		else
			return (0);
	}

	/* If console-ness unspecified, it wins. */
	return (1);
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
	dev->pdev = &dev->drm_pci;
	dev->pci_vendor = dev->pdev->vendor = da->pci_vendor;
	dev->pci_device = dev->pdev->device = da->pci_device;
	dev->pdev->subsystem_vendor = da->pci_subvendor;
	dev->pdev->subsystem_device = da->pci_subdevice;

	dev->pc = da->pc;
	dev->bridgetag = da->bridgetag;

	rw_init(&dev->struct_mutex, "drmdevlk");
	mtx_init(&dev->event_lock, IPL_TTY);
	mtx_init(&dev->quiesce_mtx, IPL_NONE);

	TAILQ_INIT(&dev->maplist);
	SPLAY_INIT(&dev->files);
	TAILQ_INIT(&dev->vbl_events);

	/*
	 * the dma buffers api is just weird. offset 1Gb to ensure we don't
	 * conflict with it.
	 */
	dev->handle_ext = extent_create("drmext", 1024*1024*1024, LONG_MAX,
	    M_DRM, NULL, 0, EX_NOWAIT | EX_NOCOALESCE);
	if (dev->handle_ext == NULL) {
		DRM_ERROR("Failed to initialise handle extent\n");
		goto error;
	}

	if (dev->driver->flags & DRIVER_AGP) {
#if __OS_HAS_AGP
		if (da->is_agp)
			dev->agp = drm_agp_init();
#endif
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

	if (dev->driver->flags & DRIVER_GEM) {
		mtx_init(&dev->obj_name_lock, IPL_NONE);
		SPLAY_INIT(&dev->name_tree);
		KASSERT(dev->driver->gem_size >= sizeof(struct drm_gem_object));
		/* XXX unique name */
		pool_init(&dev->objpl, dev->driver->gem_size, 0, 0, 0,
		    "drmobjpl", &pool_allocator_nointr);
	}

	printf("\n");
	return;

error:
	drm_lastclose(dev);
	dev->dev_private = NULL;
}

int
drm_detach(struct device *self, int flags)
{
	struct drm_device *dev = (struct drm_device *)self;

	drm_lastclose(dev);

	if (dev->driver->flags & DRIVER_GEM)
		pool_destroy(&dev->objpl);

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

void
drm_quiesce(struct drm_device *dev)
{
	mtx_enter(&dev->quiesce_mtx);
	dev->quiesce = 1;
	while (dev->quiesce_count > 0) {
		msleep(&dev->quiesce_count, &dev->quiesce_mtx,
		    PZERO, "drmqui", 0);
	}
	mtx_leave(&dev->quiesce_mtx);
}

void
drm_wakeup(struct drm_device *dev)
{
	mtx_enter(&dev->quiesce_mtx);
	dev->quiesce = 0;
	wakeup(&dev->quiesce);
	mtx_leave(&dev->quiesce_mtx);
}

int
drm_activate(struct device *self, int act)
{
	struct drm_device *dev = (struct drm_device *)self;

	switch (act) {
	case DVACT_QUIESCE:
		drm_quiesce(dev);
		break;
	case DVACT_WAKEUP:
		drm_wakeup(dev);
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

struct drm_device *
drm_get_device_from_kdev(dev_t kdev)
{
	int unit = minor(kdev) & ((1 << CLONE_SHIFT) - 1);

	if (unit < drm_cd.cd_ndevs)
		return drm_cd.cd_devs[unit];

	return NULL;
}

int
drm_firstopen(struct drm_device *dev)
{
	if (dev->driver->firstopen)
		dev->driver->firstopen(dev);

	dev->magicid = 1;

	if (!drm_core_check_feature(dev, DRIVER_MODESET))
		dev->irq_enabled = 0;
	dev->if_version = 0;

	dev->buf_pgid = 0;

	DRM_DEBUG("\n");

	return 0;
}

int
drm_lastclose(struct drm_device *dev)
{
	DRM_DEBUG("\n");

	if (dev->driver->lastclose != NULL)
		dev->driver->lastclose(dev);

	if (!drm_core_check_feature(dev, DRIVER_MODESET) && dev->irq_enabled)
		drm_irq_uninstall(dev);

#if __OS_HAS_AGP
	if (!drm_core_check_feature(dev, DRIVER_MODESET))
		drm_agp_takedown(dev);
#endif

	return 0;
}

int
drmopen(dev_t kdev, int flags, int fmt, struct proc *p)
{
	struct drm_device	*dev = NULL;
	struct drm_file		*file_priv;
	int			 ret = 0;

	dev = drm_get_device_from_kdev(kdev);
	if (dev == NULL || dev->dev_private == NULL)
		return (ENXIO);

	DRM_DEBUG("open_count = %d\n", dev->open_count);

	if (flags & O_EXCL)
		return (EBUSY); /* No exclusive opens */

	mutex_lock(&dev->struct_mutex);
	if (dev->open_count++ == 0) {
		mutex_unlock(&dev->struct_mutex);
		if ((ret = drm_firstopen(dev)) != 0)
			goto err;
	} else {
		mutex_unlock(&dev->struct_mutex);
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
	INIT_LIST_HEAD(&file_priv->fbs);
	TAILQ_INIT(&file_priv->evlist);
	file_priv->event_space = 4096; /* 4k for event buffer */
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

	mutex_lock(&dev->struct_mutex);
	/* first opener automatically becomes master if root */
	if (SPLAY_EMPTY(&dev->files) && !DRM_SUSER(p)) {
		mutex_unlock(&dev->struct_mutex);
		ret = EPERM;
		goto free_priv;
	}

	file_priv->master = SPLAY_EMPTY(&dev->files);

	SPLAY_INSERT(drm_file_tree, &dev->files, file_priv);
	mutex_unlock(&dev->struct_mutex);

	return (0);

free_priv:
	drm_free(file_priv);
err:
	mutex_lock(&dev->struct_mutex);
	--dev->open_count;
	mutex_unlock(&dev->struct_mutex);
	return (ret);
}

int
drmclose(dev_t kdev, int flags, int fmt, struct proc *p)
{
	struct drm_device		*dev = drm_get_device_from_kdev(kdev);
	struct drm_file			*file_priv;
	struct drm_pending_event	*ev, *evtmp;
	struct drm_pending_vblank_event	*vev;
	int				 retcode = 0;

	if (dev == NULL)
		return (ENXIO);

	DRM_DEBUG("open_count = %d\n", dev->open_count);

	mutex_lock(&dev->struct_mutex);
	file_priv = drm_find_file_by_minor(dev, minor(kdev));
	if (file_priv == NULL) {
		DRM_ERROR("can't find authenticator\n");
		retcode = EINVAL;
		goto done;
	}
	mutex_unlock(&dev->struct_mutex);

	if (dev->driver->close != NULL)
		dev->driver->close(dev, file_priv);

	DRM_DEBUG("pid = %d, device = 0x%lx, open_count = %d\n",
	    DRM_CURRENTPID, (long)&dev->device, dev->open_count);

	mtx_enter(&dev->event_lock);
	struct drmevlist *list = &dev->vbl_events;
	for (ev = TAILQ_FIRST(list); ev != NULL; ev = evtmp) {
		evtmp = TAILQ_NEXT(ev, link);
		vev = (struct drm_pending_vblank_event *)ev;
		if (ev->file_priv == file_priv) {
			TAILQ_REMOVE(list, ev, link);
			drm_vblank_put(dev, vev->pipe);
			ev->destroy(ev);
		}
	}
	while ((ev = TAILQ_FIRST(&file_priv->evlist)) != NULL) {
		TAILQ_REMOVE(&file_priv->evlist, ev, link);
		ev->destroy(ev);
	}
	mtx_leave(&dev->event_lock);

	if (dev->driver->flags & DRIVER_MODESET)
		drm_fb_release(dev, file_priv);

	mutex_lock(&dev->struct_mutex);
	if (dev->driver->flags & DRIVER_GEM) {
		struct drm_handle	*han;
		mtx_enter(&file_priv->table_lock);
		while ((han = SPLAY_ROOT(&file_priv->obj_tree)) != NULL) {
			SPLAY_REMOVE(drm_obj_tree, &file_priv->obj_tree, han);
			mtx_leave(&file_priv->table_lock);
			drm_gem_object_handle_unreference(han->obj);
			drm_free(han);
			mtx_enter(&file_priv->table_lock);
		}
		mtx_leave(&file_priv->table_lock);
	}

	dev->buf_pgid = 0;

	SPLAY_REMOVE(drm_file_tree, &dev->files, file_priv);
	drm_free(file_priv);

done:
	if (--dev->open_count == 0) {
		mutex_unlock(&dev->struct_mutex);
		retcode = drm_lastclose(dev);
	} else
		mutex_unlock(&dev->struct_mutex);

	return (retcode);
}

int
drm_do_ioctl(struct drm_device *dev, int minor, u_long cmd, caddr_t data)
{
	struct drm_file *file_priv;

	mutex_lock(&dev->struct_mutex);
	file_priv = drm_find_file_by_minor(dev, minor);
	mutex_unlock(&dev->struct_mutex);
	if (file_priv == NULL) {
		DRM_ERROR("can't find authenticator\n");
		return EINVAL;
	}

	++file_priv->ioctl_count;

	DRM_DEBUG("pid=%d, cmd=0x%02lx, nr=0x%02x, dev 0x%lx, auth=%d\n",
	    DRM_CURRENTPID, cmd, (u_int)DRM_IOCTL_NR(cmd), (long)&dev->device,
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
		return -drm_version(dev, data, file_priv);
	case DRM_IOCTL_GET_UNIQUE:
		return -drm_getunique(dev, data, file_priv);
	case DRM_IOCTL_GET_MAGIC:
		return -drm_getmagic(dev, data, file_priv);
	case DRM_IOCTL_WAIT_VBLANK:
		return -drm_wait_vblank(dev, data, file_priv);
	case DRM_IOCTL_MODESET_CTL:
		return -drm_modeset_ctl(dev, data, file_priv);
	case DRM_IOCTL_GEM_CLOSE:
		return -drm_gem_close_ioctl(dev, data, file_priv);

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
		case DRM_IOCTL_GEM_FLINK:
			return -drm_gem_flink_ioctl(dev, data, file_priv);
		case DRM_IOCTL_GEM_OPEN:
			return -drm_gem_open_ioctl(dev, data, file_priv);
		case DRM_IOCTL_GET_CAP:
			return -drm_getcap(dev, data, file_priv);
		}
	}

	/* master is always root */
	if (file_priv->master == 1) {
		switch(cmd) {
		case DRM_IOCTL_SET_VERSION:
			return -drm_setversion(dev, data, file_priv);
		case DRM_IOCTL_IRQ_BUSID:
			return -drm_irq_by_busid(dev, data, file_priv);
		case DRM_IOCTL_AUTH_MAGIC:
			return -drm_authmagic(dev, data, file_priv);
		case DRM_IOCTL_CONTROL:
			return -drm_control(dev, data, file_priv);
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
		case DRM_IOCTL_MODE_GETRESOURCES:
			return -drm_mode_getresources(dev, data, file_priv);
		case DRM_IOCTL_MODE_GETPLANERESOURCES:
			return -drm_mode_getplane_res(dev, data, file_priv);
		case DRM_IOCTL_MODE_GETCRTC:
			return -drm_mode_getcrtc(dev, data, file_priv);
		case DRM_IOCTL_MODE_SETCRTC:
			return -drm_mode_setcrtc(dev, data, file_priv);
		case DRM_IOCTL_MODE_GETPLANE:
			return -drm_mode_getplane(dev, data, file_priv);
		case DRM_IOCTL_MODE_SETPLANE:
			return -drm_mode_setplane(dev, data, file_priv);
		case DRM_IOCTL_MODE_CURSOR:
			return -drm_mode_cursor_ioctl(dev, data, file_priv);
		case DRM_IOCTL_MODE_GETGAMMA:
			return -drm_mode_gamma_get_ioctl(dev, data, file_priv);
		case DRM_IOCTL_MODE_SETGAMMA:
			return -drm_mode_gamma_set_ioctl(dev, data, file_priv);
		case DRM_IOCTL_MODE_GETENCODER:
			return -drm_mode_getencoder(dev, data, file_priv);
		case DRM_IOCTL_MODE_GETCONNECTOR:
			return -drm_mode_getconnector(dev, data, file_priv);
		case DRM_IOCTL_MODE_ATTACHMODE:
			return -drm_mode_attachmode_ioctl(dev, data, file_priv);
		case DRM_IOCTL_MODE_DETACHMODE:
			return -drm_mode_detachmode_ioctl(dev, data, file_priv);
		case DRM_IOCTL_MODE_GETPROPERTY:
			return -drm_mode_getproperty_ioctl(dev, data, 
			    file_priv);
		case DRM_IOCTL_MODE_SETPROPERTY:
			return -drm_mode_connector_property_set_ioctl(dev, 
			    data, file_priv);
		case DRM_IOCTL_MODE_GETPROPBLOB:
			return -drm_mode_getblob_ioctl(dev, data, file_priv);
		case DRM_IOCTL_MODE_GETFB:
			return -drm_mode_getfb(dev, data, file_priv);
		case DRM_IOCTL_MODE_ADDFB:
			return -drm_mode_addfb(dev, data, file_priv);
		case DRM_IOCTL_MODE_ADDFB2:
			return -drm_mode_addfb2(dev, data, file_priv);
		case DRM_IOCTL_MODE_RMFB:
			return -drm_mode_rmfb(dev, data, file_priv);
		case DRM_IOCTL_MODE_PAGE_FLIP:
			return -drm_mode_page_flip_ioctl(dev, data, file_priv);
		case DRM_IOCTL_MODE_DIRTYFB:
			return -drm_mode_dirtyfb_ioctl(dev, data, file_priv);
		case DRM_IOCTL_MODE_CREATE_DUMB:
			return -drm_mode_create_dumb_ioctl(dev, data, 
			    file_priv);
		case DRM_IOCTL_MODE_MAP_DUMB:
			return -drm_mode_mmap_dumb_ioctl(dev, data, file_priv);
		case DRM_IOCTL_MODE_DESTROY_DUMB:
			return -drm_mode_destroy_dumb_ioctl(dev, data, 
			    file_priv);
		case DRM_IOCTL_MODE_OBJ_GETPROPERTIES:
			return -drm_mode_obj_get_properties_ioctl(dev, data,
			    file_priv);
		case DRM_IOCTL_MODE_OBJ_SETPROPERTY:
			return -drm_mode_obj_set_property_ioctl(dev, data,
			    file_priv);
		}
	}
	if (dev->driver->ioctl != NULL)
		return (dev->driver->ioctl(dev, cmd, data, file_priv));
	else
		return (EINVAL);
}

/* drmioctl is called whenever a process performs an ioctl on /dev/drm.
 */
int
drmioctl(dev_t kdev, u_long cmd, caddr_t data, int flags, struct proc *p)
{
	struct drm_device *dev = drm_get_device_from_kdev(kdev);
	int error;

	if (dev == NULL)
		return ENODEV;

	mtx_enter(&dev->quiesce_mtx);
	while (dev->quiesce)
		msleep(&dev->quiesce, &dev->quiesce_mtx, PZERO, "drmioc", 0);
	dev->quiesce_count++;
	mtx_leave(&dev->quiesce_mtx);

	error = drm_do_ioctl(dev, minor(kdev), cmd, data);
	if (error < 0 && error != ERESTART && error != EJUSTRETURN)
		printf("%s: cmd 0x%lx errno %d\n", __func__, cmd, error);

	mtx_enter(&dev->quiesce_mtx);
	dev->quiesce_count--;
	if (dev->quiesce)
		wakeup(&dev->quiesce_count);
	mtx_leave(&dev->quiesce_mtx);

	return (error);
}

int
drmread(dev_t kdev, struct uio *uio, int ioflag)
{
	struct drm_device		*dev = drm_get_device_from_kdev(kdev);
	struct drm_file			*file_priv;
	struct drm_pending_event	*ev;
	int		 		 error = 0;

	if (dev == NULL)
		return (ENXIO);

	mutex_lock(&dev->struct_mutex);
	file_priv = drm_find_file_by_minor(dev, minor(kdev));
	mutex_unlock(&dev->struct_mutex);
	if (file_priv == NULL)
		return (ENXIO);

	/*
	 * The semantics are a little weird here. We will wait until we
	 * have events to process, but as soon as we have events we will
	 * only deliver as many as we have.
	 * Note that events are atomic, if the read buffer will not fit in
	 * a whole event, we won't read any of it out.
	 */
	mtx_enter(&dev->event_lock);
	while (error == 0 && TAILQ_EMPTY(&file_priv->evlist)) {
		if (ioflag & IO_NDELAY) {
			mtx_leave(&dev->event_lock);
			return (EAGAIN);
		}
		error = msleep(&file_priv->evlist, &dev->event_lock,
		    PWAIT | PCATCH, "drmread", 0);
	}
	if (error) {
		mtx_leave(&dev->event_lock);
		return (error);
	}
	while (drm_dequeue_event(dev, file_priv, uio->uio_resid, &ev)) {
		MUTEX_ASSERT_UNLOCKED(&dev->event_lock);
		/* XXX we always destroy the event on error. */
		error = uiomovei(ev->event, ev->event->length, uio);
		ev->destroy(ev);
		if (error)
			break;
		mtx_enter(&dev->event_lock);
	}
	MUTEX_ASSERT_UNLOCKED(&dev->event_lock);

	return (error);
}

/*
 * Deqeue an event from the file priv in question. returning 1 if an
 * event was found. We take the resid from the read as a parameter because
 * we will only dequeue and event if the read buffer has space to fit the
 * entire thing.
 *
 * We are called locked, but we will *unlock* the queue on return so that
 * we may sleep to copyout the event.
 */
int
drm_dequeue_event(struct drm_device *dev, struct drm_file *file_priv,
    size_t resid, struct drm_pending_event **out)
{
	struct drm_pending_event	*ev = NULL;
	int				 gotone = 0;

	MUTEX_ASSERT_LOCKED(&dev->event_lock);
	if ((ev = TAILQ_FIRST(&file_priv->evlist)) == NULL ||
	    ev->event->length > resid)
		goto out;

	TAILQ_REMOVE(&file_priv->evlist, ev, link);
	file_priv->event_space += ev->event->length;
	*out = ev;
	gotone = 1;

out:
	mtx_leave(&dev->event_lock);

	return (gotone);
}

/* XXX kqfilter ... */
int
drmpoll(dev_t kdev, int events, struct proc *p)
{
	struct drm_device	*dev = drm_get_device_from_kdev(kdev);
	struct drm_file		*file_priv;
	int		 	 revents = 0;

	if (dev == NULL)
		return (POLLERR);

	mutex_lock(&dev->struct_mutex);
	file_priv = drm_find_file_by_minor(dev, minor(kdev));
	mutex_unlock(&dev->struct_mutex);
	if (file_priv == NULL)
		return (POLLERR);

	mtx_enter(&dev->event_lock);
	if (events & (POLLIN | POLLRDNORM)) {
		if (!TAILQ_EMPTY(&file_priv->evlist))
			revents |=  events & (POLLIN | POLLRDNORM);
		else
			selrecord(p, &file_priv->rsel);
	}
	mtx_leave(&dev->event_lock);

	return (revents);
}

struct drm_local_map *
drm_getsarea(struct drm_device *dev)
{
	struct drm_local_map	*map;

	mutex_lock(&dev->struct_mutex);
	TAILQ_FOREACH(map, &dev->maplist, link) {
		if (map->type == _DRM_SHM && (map->flags & _DRM_CONTAINS_LOCK))
			break;
	}
	mutex_unlock(&dev->struct_mutex);
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

	mutex_lock(&dev->struct_mutex);
	file_priv = drm_find_file_by_minor(dev, minor(kdev));
	mutex_unlock(&dev->struct_mutex);
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
			phys = dma->pagelist[offset >> PAGE_SHIFT];
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
	mutex_lock(&dev->struct_mutex);
	TAILQ_FOREACH(map, &dev->maplist, link) {
		if (offset >= map->ext &&
		    offset < map->ext + map->size) {
			offset -= map->ext;
			break;
		}
	}

	if (map == NULL) {
		mutex_unlock(&dev->struct_mutex);
		DRM_DEBUG("can't find map\n");
		return (-1);
	}
	if (((map->flags & _DRM_RESTRICTED) && file_priv->master == 0)) {
		mutex_unlock(&dev->struct_mutex);
		DRM_DEBUG("restricted map\n");
		return (-1);
	}
	type = map->type;
	mutex_unlock(&dev->struct_mutex);

	switch (type) {
#if __OS_HAS_AGP
	case _DRM_AGP:
		return agp_mmap(dev->agp->agpdev,
		    offset + map->offset - dev->agp->base, prot);
#endif
	case _DRM_FRAME_BUFFER:
	case _DRM_REGISTERS:
		return (offset + map->offset);
		break;
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
			return -EFAULT;
	}
	u->unique_len = dev->unique_len;

	return 0;
}

int
drm_getcap(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_get_cap *req = data;

	req->value = 0;
	switch (req->capability) {
	case DRM_CAP_DUMB_BUFFER:
		if (dev->driver->dumb_create)
			req->value = 1;
		break;
	case DRM_CAP_VBLANK_HIGH_CRTC:
		req->value = 1;
		break;
	case DRM_CAP_DUMB_PREFERRED_DEPTH:
		req->value = dev->mode_config.preferred_depth;
		break;
	case DRM_CAP_DUMB_PREFER_SHADOW:
		req->value = dev->mode_config.prefer_shadow;
		break;
	case DRM_CAP_TIMESTAMP_MONOTONIC:
		req->value = drm_timestamp_monotonic;
		break;
	default:
		return -EINVAL;
	}
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
			return -EFAULT;				\
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
			return -EINVAL;
		}
		if_version = DRM_IF_VERSION(ver.drm_di_major, ver.drm_dd_minor);
		dev->if_version = imax(if_version, dev->if_version);
	}

	if (ver.drm_dd_major != -1) {
		if (ver.drm_dd_major != dev->driver->major ||
		    ver.drm_dd_minor < 0 ||
		    ver.drm_dd_minor > dev->driver->minor)
			return -EINVAL;
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
	free(mem, M_DRM, 0);

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
	free(mem, M_DRM, 0);
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
		mutex_lock(&dev->struct_mutex);
		file_priv->magic = auth->magic = dev->magicid++;
		mutex_unlock(&dev->struct_mutex);
		DRM_DEBUG("%d\n", auth->magic);
	}

	DRM_DEBUG("%u\n", auth->magic);
	return 0;
}

/**
 * Marks the client associated with the given magic number as authenticated.
 */
int
drm_authmagic(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_file	*p;
	struct drm_auth	*auth = data;
	int		 ret = -EINVAL;

	DRM_DEBUG("%u\n", auth->magic);

	if (auth->magic == 0)
		return ret;

	mutex_lock(&dev->struct_mutex);
	SPLAY_FOREACH(p, drm_file_tree, &dev->files) {
		if (p->magic == auth->magic) {
			p->authenticated = 1;
			p->magic = 0;
			ret = 0;
			break;
		}
	}
	mutex_unlock(&dev->struct_mutex);

	return ret;
}

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
	uobj->uo_refs++;
}

void
drm_unref(struct uvm_object *uobj)
{
	struct drm_gem_object *obj = (struct drm_gem_object *)uobj;
	struct drm_device *dev = obj->dev;

	if (uobj->uo_refs > 1) {
		uobj->uo_refs--;
		return;
	}

	/* We own this thing now. It is on no queues, though it may still
	 * be bound to the aperture (and on the inactive list, in which case
	 * idling the buffer is what triggered the free. Since we know no one 
	 * else can grab it now, we can nuke with impunity.
	 */
	if (dev->driver->gem_free_object != NULL)
		dev->driver->gem_free_object(obj);
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
	struct drm_gem_object *obj = (struct drm_gem_object *)uobj;
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

/*
 * Code to support memory managers based on the GEM (Graphics
 * Execution Manager) api.
 */
struct drm_gem_object *
drm_gem_object_alloc(struct drm_device *dev, size_t size)
{
	struct drm_gem_object	*obj;

	KASSERT((size & (PAGE_SIZE -1)) == 0);

	if ((obj = pool_get(&dev->objpl, PR_WAITOK | PR_ZERO)) == NULL)
		return (NULL);

	obj->dev = dev;

	/* uao create can't fail in the 0 case, it just sleeps */
	obj->uao = uao_create(size, 0);
	obj->size = size;
	uvm_objinit(&obj->uobj, &drm_pgops, 1);

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
drm_gem_object_init(struct drm_device *dev, struct drm_gem_object *obj, size_t size)
{
	BUG_ON((size & (PAGE_SIZE -1)) != 0);

	obj->dev = dev;

	/* uao create can't fail in the 0 case, it just sleeps */
	obj->uao = uao_create(size, 0);
	obj->size = size;
	uvm_objinit(&obj->uobj, &drm_pgops, 1);

	atomic_inc(&dev->obj_count);
	atomic_add(obj->size, &dev->obj_memory);
	return 0;
}

void
drm_gem_object_release(struct drm_gem_object *obj)
{
	struct drm_device *dev = obj->dev;

	if (obj->uao)
		uao_detach(obj->uao);

	atomic_dec(&dev->obj_count);
	atomic_sub(obj->size, &dev->obj_memory);
	if (obj->do_flags & DRM_WANTED) /* should never happen, not on lists */
		wakeup(obj);
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
	struct drm_device *dev = obj->dev;
	struct drm_handle *han;
	int ret;

	if ((han = drm_calloc(1, sizeof(*han))) == NULL)
		return -ENOMEM;

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

	drm_gem_object_handle_reference(obj);

	if (dev->driver->gem_open_object) {
		ret = dev->driver->gem_open_object(obj, file_priv);
		if (ret) {
			drm_gem_handle_delete(file_priv, *handlep);
			return ret;
		}
	}

	return 0;
}

/**
 * Removes the mapping from handle to filp for this object.
 */
int
drm_gem_handle_delete(struct drm_file *filp, u32 handle)
{
	struct drm_device *dev;
	struct drm_gem_object *obj;
	struct drm_handle *han, find;

	find.handle = handle;
	mtx_enter(&filp->table_lock);
	han = SPLAY_FIND(drm_obj_tree, &filp->obj_tree, &find);
	if (han == NULL) {
		mtx_leave(&filp->table_lock);
		return -EINVAL;
	}
	obj = han->obj;
	dev = obj->dev;

	SPLAY_REMOVE(drm_obj_tree, &filp->obj_tree, han);
	mtx_leave(&filp->table_lock);

	drm_free(han);

	if (dev->driver->gem_close_object)
		dev->driver->gem_close_object(obj, filp);
	drm_gem_object_handle_unreference_unlocked(obj);

	return 0;
}

/** Returns a reference to the object named by the handle. */
struct drm_gem_object *
drm_gem_object_lookup(struct drm_device *dev, struct drm_file *filp,
		      u32 handle)
{
	struct drm_gem_object *obj;
	struct drm_handle *han, search;

	mtx_enter(&filp->table_lock);

	/* Check if we currently have a reference on the object */
	search.handle = handle;
	han = SPLAY_FIND(drm_obj_tree, &filp->obj_tree, &search);
	if (han == NULL) {
		mtx_leave(&filp->table_lock);
		return NULL;
	}
	obj = han->obj;

	drm_gem_object_reference(obj);

	mtx_leave(&filp->table_lock);

	return obj;
}

/**
 * Releases the handle to an mm object.
 */
int
drm_gem_close_ioctl(struct drm_device *dev, void *data,
		    struct drm_file *file_priv)
{
	struct drm_gem_close *args = data;
	int ret;

	if (!(dev->driver->flags & DRIVER_GEM))
		return -ENODEV;

	ret = drm_gem_handle_delete(file_priv, args->handle);

	return ret;
}

int
drm_gem_flink_ioctl(struct drm_device *dev, void *data,
    struct drm_file *file_priv)
{
	struct drm_gem_flink	*args = data;
	struct drm_gem_object	*obj;

	if (!(dev->driver->flags & DRIVER_GEM))
		return -ENODEV;

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);
	if (obj == NULL)
		return -ENOENT;

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

	return 0;
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
	struct drm_gem_object *obj, search;
	int ret;
	u32 handle;

	if (!(dev->driver->flags & DRIVER_GEM))
		return -ENODEV;

	mtx_enter(&dev->obj_name_lock);
	search.name = args->name;
	obj = SPLAY_FIND(drm_name_tree, &dev->name_tree, &search);
	if (obj)
		drm_gem_object_reference(obj);
	mtx_leave(&dev->obj_name_lock);
	if (!obj)
		return -ENOENT;

	ret = drm_gem_handle_create(file_priv, obj, &handle);
	drm_gem_object_unreference_unlocked(obj);
	if (ret)
		return ret;

	args->handle = handle;
	args->size = obj->size;

        return 0;
}

void
drm_gem_object_handle_reference(struct drm_gem_object *obj)
{
	drm_gem_object_reference(obj);
	obj->handlecount++;
}

void
drm_gem_object_handle_unreference(struct drm_gem_object *obj)
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
			drm_gem_object_unreference(obj);
		} else {
			mtx_leave(&dev->obj_name_lock);
		}
	}

	drm_gem_object_unreference(obj);
}

void
drm_gem_object_handle_unreference_unlocked(struct drm_gem_object *obj)
{
	struct drm_device *dev = obj->dev;

	mutex_lock(&dev->struct_mutex);
	drm_gem_object_handle_unreference(obj);
	mutex_unlock(&dev->struct_mutex);
}

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
	struct drm_local_map *map = obj->map;

	TAILQ_REMOVE(&dev->maplist, map, link);
	obj->map = NULL;

	/* NOCOALESCE set, can't fail */
	extent_free(dev->handle_ext, map->ext, map->size, EX_NOWAIT);

	drm_free(map);
}

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
int
drm_gem_create_mmap_offset(struct drm_gem_object *obj)
{
	struct drm_device *dev = obj->dev;
	struct drm_local_map *map;
	int ret;

	/* Set the object up for mmap'ing */
	map = drm_calloc(1, sizeof(*map));
	if (map == NULL)
		return -ENOMEM;

	map->flags = _DRM_DRIVER;
	map->type = _DRM_GEM;
	map->size = obj->size;
	map->handle = obj;

	/* Get a DRM GEM mmap offset allocated... */
	ret = extent_alloc(dev->handle_ext, map->size, PAGE_SIZE, 0,
	    0, EX_NOWAIT, &map->ext);
	if (ret) {
		DRM_ERROR("failed to allocate offset for bo %d\n", obj->name);
		ret = -ENOSPC;
		goto out_free_list;
	}

	TAILQ_INSERT_TAIL(&dev->maplist, map, link);
	obj->map = map;
	return 0;

out_free_list:
	drm_free(map);

	return ret;
}

struct uvm_object *
udv_attach_drm(dev_t device, vm_prot_t accessprot, voff_t off, vsize_t size)
{
	struct drm_device *dev = drm_get_device_from_kdev(device);
	struct drm_local_map *map;
	struct drm_gem_object *obj;

	if (cdevsw[major(device)].d_mmap != drmmmap)
		return NULL;

	if (dev == NULL)
		return NULL;

	if (dev->driver->mmap)
		return dev->driver->mmap(dev, off, size);

	mutex_lock(&dev->struct_mutex);
	TAILQ_FOREACH(map, &dev->maplist, link) {
		if (off >= map->ext && off + size <= map->ext + map->size)
			break;
	}

	if (map == NULL || map->type != _DRM_GEM) {
		mutex_unlock(&dev->struct_mutex);
		return NULL;
	}

	obj = (struct drm_gem_object *)map->handle;
	drm_ref(&obj->uobj);
	mutex_unlock(&dev->struct_mutex);
	return &obj->uobj;
}

/*
 * Compute order.  Can be made faster.
 */
int
drm_order(unsigned long size)
{
	int order;
	unsigned long tmp;

	for (order = 0, tmp = size; tmp >>= 1; ++order)
		;

	if (size & ~(1 << order))
		++order;

	return order;
}

int drm_pcie_get_speed_cap_mask(struct drm_device *dev, u32 *mask)
{
	pci_chipset_tag_t	pc = dev->pc;
	pcitag_t		tag;
	int			pos ;
	pcireg_t		xcap, lnkcap = 0, lnkcap2 = 0;
	pcireg_t		id;

	*mask = 0;

	if (dev->bridgetag == NULL)
		return -EINVAL;
	tag = *dev->bridgetag;

	if (!pci_get_capability(pc, tag, PCI_CAP_PCIEXPRESS,
	    &pos, NULL)) 
		return -EINVAL;

	id = pci_conf_read(pc, tag, PCI_ID_REG);

	/* we've been informed via and serverworks don't make the cut */
	if (PCI_VENDOR(id) == PCI_VENDOR_VIATECH ||
	    PCI_VENDOR(id) == PCI_VENDOR_RCC)
		return -EINVAL;

	lnkcap = pci_conf_read(pc, tag, pos + PCI_PCIE_LCAP);
	xcap = pci_conf_read(pc, tag, pos + PCI_PCIE_XCAP);
	if (PCI_PCIE_XCAP_VER(xcap) >= 2)
		lnkcap2 = pci_conf_read(pc, tag, pos + PCI_PCIE_LCAP2);

	lnkcap &= 0x0f;
	lnkcap2 &= 0xfe;

	if (lnkcap2) { /* PCIE GEN 3.0 */
		if (lnkcap2 & 2)
			*mask |= DRM_PCIE_SPEED_25;
		if (lnkcap2 & 4)
			*mask |= DRM_PCIE_SPEED_50;
		if (lnkcap2 & 8)
			*mask |= DRM_PCIE_SPEED_80;
	} else {
		if (lnkcap & 1)
			*mask |= DRM_PCIE_SPEED_25;
		if (lnkcap & 2)
			*mask |= DRM_PCIE_SPEED_50;
	}

	DRM_INFO("probing gen 2 caps for device 0x%04x:0x%04x = %x/%x\n",
	    PCI_VENDOR(id), PCI_PRODUCT(id), lnkcap, lnkcap2);
	return 0;
}

int
drm_handle_cmp(struct drm_handle *a, struct drm_handle *b)
{
	return (a->handle < b->handle ? -1 : a->handle > b->handle);
}

int
drm_name_cmp(struct drm_gem_object *a, struct drm_gem_object *b)
{
	return (a->name < b->name ? -1 : a->name > b->name);
}

SPLAY_GENERATE(drm_obj_tree, drm_handle, entry, drm_handle_cmp);

SPLAY_GENERATE(drm_name_tree, drm_gem_object, entry, drm_name_cmp);
