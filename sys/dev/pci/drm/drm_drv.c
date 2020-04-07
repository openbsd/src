/* $OpenBSD: drm_drv.c,v 1.174 2020/04/07 13:27:51 visa Exp $ */
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
#include <sys/poll.h>
#include <sys/specdev.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/event.h>

#include <machine/bus.h>

#ifdef __HAVE_ACPI
#include <dev/acpi/acpidev.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/dsdt.h>
#endif

#include <drm/drmP.h>
#include <drm/drm_gem.h>
#include <uapi/drm/drm.h>
#include "drm_internal.h"
#include "drm_crtc_internal.h"
#include <drm/drm_vblank.h>
#include <drm/drm_print.h>

struct drm_softc {
	struct device		sc_dev;
	struct drm_device 	*sc_drm;
	int			sc_allocated;
};

/*
 * drm_debug: Enable debug output.
 * Bitmask of DRM_UT_x. See include/drm/drm_print.h for details.
 */
#ifdef DRMDEBUG
unsigned int drm_debug = DRM_UT_DRIVER | DRM_UT_KMS;
#else
unsigned int drm_debug = 0;
#endif

int	 drm_firstopen(struct drm_device *);
void	 drm_attach(struct device *, struct device *, void *);
int	 drm_probe(struct device *, void *, void *);
int	 drm_detach(struct device *, int);
void	 drm_quiesce(struct drm_device *);
void	 drm_wakeup(struct drm_device *);
int	 drm_activate(struct device *, int);
int	 drm_dequeue_event(struct drm_device *, struct drm_file *, size_t,
	     struct drm_pending_event **);

int	 drm_getmagic(struct drm_device *, void *, struct drm_file *);
int	 drm_authmagic(struct drm_device *, void *, struct drm_file *);
int	 drm_getpciinfo(struct drm_device *, void *, struct drm_file *);
int	 drm_file_cmp(struct drm_file *, struct drm_file *);
SPLAY_PROTOTYPE(drm_file_tree, drm_file, link, drm_file_cmp);

/*
 * attach drm to a pci-based driver.
 *
 * This function does all the pci-specific calculations for the 
 * drm_attach_args.
 */
struct drm_device *
drm_attach_pci(struct drm_driver *driver, struct pci_attach_args *pa,
    int is_agp, int primary, struct device *dev, struct drm_device *drm)
{
	struct drm_attach_args arg;
	struct drm_softc *sc;

	arg.drm = drm;
	arg.driver = driver;
	arg.dmat = pa->pa_dmat;
	arg.bst = pa->pa_memt;
	arg.is_agp = is_agp;
	arg.primary = primary;
	arg.pa = pa;

	arg.busid_len = 20;
	arg.busid = malloc(arg.busid_len + 1, M_DRM, M_NOWAIT);
	if (arg.busid == NULL) {
		printf("%s: no memory for drm\n", dev->dv_xname);
		return (NULL);
	}
	snprintf(arg.busid, arg.busid_len, "pci:%04x:%02x:%02x.%1x",
	    pa->pa_domain, pa->pa_bus, pa->pa_device, pa->pa_function);

	sc = (struct drm_softc *)config_found_sm(dev, &arg, drmprint, drmsubmatch);
	if (sc == NULL)
		return NULL;
	
	return sc->sc_drm;
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

	if (cf->drmdevcf_primary != DRMDEVCF_PRIMARY_UNK) {
		/*
		 * If primary-ness of device specified, either match
		 * exactly (at high priority), or fail.
		 */
		if (cf->drmdevcf_primary != 0 && da->primary != 0)
			return (10);
		else
			return (0);
	}

	/* If primary-ness unspecified, it wins. */
	return (1);
}

void
drm_attach(struct device *parent, struct device *self, void *aux)
{
	struct drm_softc *sc = (struct drm_softc *)self;
	struct drm_attach_args *da = aux;
	struct drm_device *dev = da->drm;
	int ret;

	drm_linux_init();

	if (dev == NULL) {
		dev = malloc(sizeof(struct drm_device), M_DRM,
		    M_WAITOK | M_ZERO);
		sc->sc_allocated = 1;
	}

	sc->sc_drm = dev;

	dev->dev = self;
	dev->dev_private = parent;
	dev->driver = da->driver;

	dev->dmat = da->dmat;
	dev->bst = da->bst;
	dev->unique = da->busid;
	dev->unique_len = da->busid_len;

	if (da->pa) {
		struct pci_attach_args *pa = da->pa;
		pcireg_t subsys;

		subsys = pci_conf_read(pa->pa_pc, pa->pa_tag,
		    PCI_SUBSYS_ID_REG);

		dev->pdev = &dev->_pdev;
		dev->pdev->vendor = PCI_VENDOR(pa->pa_id);
		dev->pdev->device = PCI_PRODUCT(pa->pa_id);
		dev->pdev->subsystem_vendor = PCI_VENDOR(subsys);
		dev->pdev->subsystem_device = PCI_PRODUCT(subsys);
		dev->pdev->revision = PCI_REVISION(pa->pa_class);

		dev->pdev->devfn = PCI_DEVFN(pa->pa_device, pa->pa_function);
		dev->pdev->bus = &dev->pdev->_bus;
		dev->pdev->bus->pc = pa->pa_pc;
		dev->pdev->bus->number = pa->pa_bus;
		dev->pdev->bus->bridgetag = pa->pa_bridgetag;

		if (pa->pa_bridgetag != NULL) {
			dev->pdev->bus->self = malloc(sizeof(struct pci_dev),
			    M_DRM, M_WAITOK | M_ZERO);
			dev->pdev->bus->self->pc = pa->pa_pc;
			dev->pdev->bus->self->tag = *pa->pa_bridgetag;
		}

		dev->pdev->pc = pa->pa_pc;
		dev->pdev->tag = pa->pa_tag;
		dev->pdev->pci = (struct pci_softc *)parent->dv_parent;

#ifdef CONFIG_ACPI
		dev->pdev->dev.node = acpi_find_pci(pa->pa_pc, pa->pa_tag);
		aml_register_notify(dev->pdev->dev.node, NULL,
		    drm_linux_acpi_notify, NULL, ACPIDEV_NOPOLL);
#endif
	}

	rw_init(&dev->struct_mutex, "drmdevlk");
	mtx_init(&dev->event_lock, IPL_TTY);
	mtx_init(&dev->quiesce_mtx, IPL_NONE);

	SPLAY_INIT(&dev->files);
	INIT_LIST_HEAD(&dev->vblank_event_list);

	if (drm_core_check_feature(dev, DRIVER_USE_AGP)) {
#if IS_ENABLED(CONFIG_AGP)
		if (da->is_agp)
			dev->agp = drm_agp_init();
#endif
		if (dev->agp != NULL) {
			if (drm_mtrr_add(dev->agp->info.ai_aperture_base,
			    dev->agp->info.ai_aperture_size, DRM_MTRR_WC) == 0)
				dev->agp->mtrr = 1;
		}
	}

	if (dev->driver->gem_size > 0) {
		KASSERT(dev->driver->gem_size >= sizeof(struct drm_gem_object));
		/* XXX unique name */
		pool_init(&dev->objpl, dev->driver->gem_size, 0, IPL_NONE, 0,
		    "drmobjpl", NULL);
	}

	if (dev->driver->driver_features & DRIVER_GEM) {
		ret = drm_gem_init(dev);
		if (ret) {
			DRM_ERROR("Cannot initialize graphics execution manager (GEM)\n");
			goto error;
		}
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
	struct drm_softc *sc = (struct drm_softc *)self;
	struct drm_device *dev = sc->sc_drm;

	drm_lastclose(dev);

	if (dev->driver->driver_features & DRIVER_GEM)
		drm_gem_destroy(dev);

	if (dev->driver->driver_features & DRIVER_GEM)
		pool_destroy(&dev->objpl);

	drm_vblank_cleanup(dev);

	if (dev->agp && dev->agp->mtrr) {
		int retcode;

		retcode = drm_mtrr_del(0, dev->agp->info.ai_aperture_base,
		    dev->agp->info.ai_aperture_size, DRM_MTRR_WC);
		DRM_DEBUG("mtrr_del = %d", retcode);
	}

	free(dev->agp, M_DRM, 0);
	if (dev->pdev && dev->pdev->bus)
		free(dev->pdev->bus->self, M_DRM, sizeof(struct pci_dev));

	if (sc->sc_allocated)
		free(dev, M_DRM, sizeof(struct drm_device));

	return 0;
}

void
drm_quiesce(struct drm_device *dev)
{
	mtx_enter(&dev->quiesce_mtx);
	dev->quiesce = 1;
	while (dev->quiesce_count > 0) {
		msleep_nsec(&dev->quiesce_count, &dev->quiesce_mtx,
		    PZERO, "drmqui", INFSLP);
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
	struct drm_softc *sc = (struct drm_softc *)self;
	struct drm_device *dev = sc->sc_drm;

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
	sizeof(struct drm_softc), drm_probe, drm_attach,
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
		    (idlist[i].device == device) &&
		    (idlist[i].subvendor == PCI_ANY_ID) &&
		    (idlist[i].subdevice == PCI_ANY_ID))
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
	/* control */
	if (unit >= 64 && unit < 128)
		unit -= 64;
	/* render */
	if (unit >= 128)
		unit -= 128;
	struct drm_softc *sc;

	if (unit < drm_cd.cd_ndevs) {
		sc = (struct drm_softc *)drm_cd.cd_devs[unit];
		if (sc)
			return sc->sc_drm;
	}

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

#if IS_ENABLED(CONFIG_AGP)
	if (!drm_core_check_feature(dev, DRIVER_MODESET))
		drm_agp_takedown(dev);
#endif

	return 0;
}

void
filt_drmdetach(struct knote *kn)
{
	struct drm_device *dev = kn->kn_hook;
	int s;

	s = spltty();
	klist_remove(&dev->note, kn);
	splx(s);
}

int
filt_drmkms(struct knote *kn, long hint)
{
	if (kn->kn_sfflags & hint)
		kn->kn_fflags |= hint;
	return (kn->kn_fflags != 0);
}

const struct filterops drm_filtops = {
	.f_flags	= FILTEROP_ISFD,
	.f_attach	= NULL,
	.f_detach	= filt_drmdetach,
	.f_event	= filt_drmkms,
};

int
drmkqfilter(dev_t kdev, struct knote *kn)
{
	struct drm_device	*dev = NULL;
	int s;

	dev = drm_get_device_from_kdev(kdev);
	if (dev == NULL || dev->dev_private == NULL)
		return (ENXIO);

	switch (kn->kn_filter) {
	case EVFILT_DEVICE:
		kn->kn_fop = &drm_filtops;
		break;
	default:
		return (EINVAL);
	}

	kn->kn_hook = dev;

	s = spltty();
	klist_insert(&dev->note, kn);
	splx(s);

	return (0);
}

int
drmopen(dev_t kdev, int flags, int fmt, struct proc *p)
{
	struct drm_device	*dev = NULL;
	struct drm_file		*file_priv;
	int			 ret = 0;
	int			 realminor;

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
	file_priv = mallocarray(1, max(dev->driver->file_priv_size,
	    sizeof(*file_priv)), M_DRM, M_NOWAIT | M_ZERO);
	if (file_priv == NULL) {
		ret = ENOMEM;
		goto err;
	}

	file_priv->filp = (void *)&file_priv;
	file_priv->minor = minor(kdev);
	realminor =  file_priv->minor & ((1 << CLONE_SHIFT) - 1);
	if (realminor < 64)
		file_priv->minor_type = DRM_MINOR_PRIMARY;
	else if (realminor >= 64 && realminor < 128)
		file_priv->minor_type = DRM_MINOR_CONTROL;
	else
		file_priv->minor_type = DRM_MINOR_RENDER;

	INIT_LIST_HEAD(&file_priv->lhead);
	INIT_LIST_HEAD(&file_priv->fbs);
	rw_init(&file_priv->fbs_lock, "fbslk");
	INIT_LIST_HEAD(&file_priv->blobs);
	INIT_LIST_HEAD(&file_priv->pending_event_list);
	INIT_LIST_HEAD(&file_priv->event_list);
	init_waitqueue_head(&file_priv->event_wait);
	file_priv->event_space = 4096; /* 4k for event buffer */
	DRM_DEBUG("minor = %d\n", file_priv->minor);

	/* for compatibility root is always authenticated */
	file_priv->authenticated = DRM_SUSER(p);

	rw_init(&file_priv->event_read_lock, "evread");

	if (drm_core_check_feature(dev, DRIVER_GEM))
		drm_gem_open(dev, file_priv);

	if (drm_core_check_feature(dev, DRIVER_SYNCOBJ))
		drm_syncobj_open(file_priv);

	if (drm_core_check_feature(dev, DRIVER_PRIME))
		drm_prime_init_file_private(&file_priv->prime);

	if (dev->driver->open) {
		ret = dev->driver->open(dev, file_priv);
		if (ret != 0) {
			goto out_prime_destroy;
		}
	}

	mutex_lock(&dev->struct_mutex);
	/* first opener automatically becomes master */
	if (drm_is_primary_client(file_priv))
		file_priv->is_master = SPLAY_EMPTY(&dev->files);
	if (file_priv->is_master)
		file_priv->authenticated = 1;

	SPLAY_INSERT(drm_file_tree, &dev->files, file_priv);
	mutex_unlock(&dev->struct_mutex);

	return (0);

out_prime_destroy:
	if (drm_core_check_feature(dev, DRIVER_PRIME))
		drm_prime_destroy_file_private(&file_priv->prime);
	if (drm_core_check_feature(dev, DRIVER_SYNCOBJ))
		drm_syncobj_release(file_priv);
	if (drm_core_check_feature(dev, DRIVER_GEM))
		drm_gem_release(dev, file_priv);
	free(file_priv, M_DRM, 0);
err:
	mutex_lock(&dev->struct_mutex);
	--dev->open_count;
	mutex_unlock(&dev->struct_mutex);
	return (ret);
}

void drm_events_release(struct drm_file *file_priv, struct drm_device *dev);

int
drmclose(dev_t kdev, int flags, int fmt, struct proc *p)
{
	struct drm_device		*dev = drm_get_device_from_kdev(kdev);
	struct drm_file			*file_priv;
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

	if (drm_core_check_feature(dev, DRIVER_LEGACY) &&
	    dev->driver->preclose)
		dev->driver->preclose(dev, file_priv);

	DRM_DEBUG("pid = %d, device = 0x%lx, open_count = %d\n",
	    DRM_CURRENTPID, (long)&dev->dev, dev->open_count);

	drm_events_release(file_priv, dev);

	if (drm_core_check_feature(dev, DRIVER_MODESET)) {
		drm_fb_release(file_priv);
		drm_property_destroy_user_blobs(dev, file_priv);
	}

	if (drm_core_check_feature(dev, DRIVER_SYNCOBJ))
		drm_syncobj_release(file_priv);

	if (drm_core_check_feature(dev, DRIVER_GEM))
		drm_gem_release(dev, file_priv);

	if (dev->driver->postclose)
		dev->driver->postclose(dev, file_priv);

	if (drm_core_check_feature(dev, DRIVER_PRIME))
		drm_prime_destroy_file_private(&file_priv->prime);

	mutex_lock(&dev->struct_mutex);

	SPLAY_REMOVE(drm_file_tree, &dev->files, file_priv);
	free(file_priv, M_DRM, 0);

done:
	if (--dev->open_count == 0) {
		mutex_unlock(&dev->struct_mutex);
		retcode = drm_lastclose(dev);
	} else
		mutex_unlock(&dev->struct_mutex);

	return (retcode);
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
	while (error == 0 && list_empty(&file_priv->event_list)) {
		if (ioflag & IO_NDELAY) {
			mtx_leave(&dev->event_lock);
			return (EAGAIN);
		}
		error = msleep_nsec(&file_priv->event_wait, &dev->event_lock,
		    PWAIT | PCATCH, "drmread", INFSLP);
	}
	if (error) {
		mtx_leave(&dev->event_lock);
		return (error);
	}
	while (drm_dequeue_event(dev, file_priv, uio->uio_resid, &ev)) {
		MUTEX_ASSERT_UNLOCKED(&dev->event_lock);
		/* XXX we always destroy the event on error. */
		error = uiomove(ev->event, ev->event->length, uio);
		kfree(ev);
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
	struct drm_pending_event *e = NULL;
	int gotone = 0;

	MUTEX_ASSERT_LOCKED(&dev->event_lock);

	*out = NULL;
	if (list_empty(&file_priv->event_list))
		goto out;
	e = list_first_entry(&file_priv->event_list,
			     struct drm_pending_event, link);
	if (e->event->length > resid)
		goto out;

	file_priv->event_space += e->event->length;
	list_del(&e->link);
	*out = e;
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
		if (!list_empty(&file_priv->event_list))
			revents |=  events & (POLLIN | POLLRDNORM);
		else
			selrecord(p, &file_priv->rsel);
	}
	mtx_leave(&dev->event_lock);

	return (revents);
}

paddr_t
drmmmap(dev_t kdev, off_t offset, int prot)
{
	return -1;
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

struct drm_dma_handle *
drm_pci_alloc(struct drm_device *dev, size_t size, size_t align)
{
	struct drm_dma_handle *dmah;

	dmah = malloc(sizeof(*dmah), M_DRM, M_WAITOK);
	dmah->mem = drm_dmamem_alloc(dev->dmat, size, align, 1, size,
	    BUS_DMA_NOCACHE, 0);
	if (dmah->mem == NULL) {
		free(dmah, M_DRM, sizeof(*dmah));
		return NULL;
	}
	dmah->busaddr = dmah->mem->segs[0].ds_addr;
	dmah->size = dmah->mem->size;
	dmah->vaddr = dmah->mem->kva;
	return (dmah);
}

void
drm_pci_free(struct drm_device *dev, struct drm_dma_handle *dmah)
{
	if (dmah == NULL)
		return;

	drm_dmamem_free(dev->dmat, dmah->mem);
	free(dmah, M_DRM, sizeof(*dmah));
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

int
drm_getpciinfo(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_pciinfo *info = data;

	if (dev->pdev == NULL)
		return -ENOTTY;

	info->domain = 0;
	info->bus = dev->pdev->bus->number;
	info->dev = PCI_SLOT(dev->pdev->devfn);
	info->func = PCI_FUNC(dev->pdev->devfn);
	info->vendor_id = dev->pdev->vendor;
	info->device_id = dev->pdev->device;
	info->subvendor_id = dev->pdev->subsystem_vendor;
	info->subdevice_id = dev->pdev->subsystem_device;
	info->revision_id = 0;

	return 0;
}

/**
 * drm_dev_register - Register DRM device
 * @dev: Device to register
 * @flags: Flags passed to the driver's .load() function
 *
 * Register the DRM device @dev with the system, advertise device to user-space
 * and start normal device operation. @dev must be allocated via drm_dev_alloc()
 * previously.
 *
 * Never call this twice on any device!
 *
 * NOTE: To ensure backward compatibility with existing drivers method this
 * function calls the &drm_driver.load method after registering the device
 * nodes, creating race conditions. Usage of the &drm_driver.load methods is
 * therefore deprecated, drivers must perform all initialization before calling
 * drm_dev_register().
 *
 * RETURNS:
 * 0 on success, negative error code on failure.
 */
int drm_dev_register(struct drm_device *dev, unsigned long flags)
{
	dev->registered = true;

	if (drm_core_check_feature(dev, DRIVER_MODESET))
		drm_modeset_register_all(dev);

	return 0;
}
EXPORT_SYMBOL(drm_dev_register);

/**
 * drm_dev_unregister - Unregister DRM device
 * @dev: Device to unregister
 *
 * Unregister the DRM device from the system. This does the reverse of
 * drm_dev_register() but does not deallocate the device. The caller must call
 * drm_dev_put() to drop their final reference.
 *
 * A special form of unregistering for hotpluggable devices is drm_dev_unplug(),
 * which can be called while there are still open users of @dev.
 *
 * This should be called first in the device teardown code to make sure
 * userspace can't access the device instance any more.
 */
void drm_dev_unregister(struct drm_device *dev)
{
	if (drm_core_check_feature(dev, DRIVER_LEGACY))
		drm_lastclose(dev);

	dev->registered = false;

	if (drm_core_check_feature(dev, DRIVER_MODESET))
		drm_modeset_unregister_all(dev);
}
EXPORT_SYMBOL(drm_dev_unregister);

