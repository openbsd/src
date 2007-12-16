/* i915_drv.c -- Intel i915 driver -*- linux-c -*-
 * Created: Wed Feb 14 17:10:04 2001 by gareth@valinux.com
 */
/*-
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
 *    Gareth Hughes <gareth@valinux.com>
 *
 */

#include "drmP.h"
#include "drm.h"
#include "i915_drm.h"
#include "i915_drv.h"
#include "drm_pciids.h"

void	i915_configure(drm_device_t *);

/* drv_PCI_IDs comes from drm_pciids.h, generated from drm_pciids.txt. */
static drm_pci_id_list_t i915_pciidlist[] = {
	i915_PCI_IDS
};

void
i915_configure(drm_device_t *dev)
{
	dev->driver.buf_priv_size	= 1;	/* No dev_priv */
	dev->driver.load		= i915_driver_load;
	dev->driver.preclose		= i915_driver_preclose;
	dev->driver.lastclose		= i915_driver_lastclose;
	dev->driver.device_is_agp	= i915_driver_device_is_agp,
	dev->driver.vblank_wait		= i915_driver_vblank_wait;
	dev->driver.irq_preinstall	= i915_driver_irq_preinstall;
	dev->driver.irq_postinstall	= i915_driver_irq_postinstall;
	dev->driver.irq_uninstall	= i915_driver_irq_uninstall;
	dev->driver.irq_handler		= i915_driver_irq_handler;

	dev->driver.ioctls		= i915_ioctls;
	dev->driver.max_ioctl		= i915_max_ioctl;

	dev->driver.name		= DRIVER_NAME;
	dev->driver.desc		= DRIVER_DESC;
	dev->driver.date		= DRIVER_DATE;
	dev->driver.major		= DRIVER_MAJOR;
	dev->driver.minor		= DRIVER_MINOR;
	dev->driver.patchlevel		= DRIVER_PATCHLEVEL;

	dev->driver.use_agp		= 1;
	dev->driver.require_agp		= 1;
	dev->driver.use_mtrr		= 1;
	dev->driver.use_irq		= 1;
	dev->driver.use_vbl_irq		= 1;
}

#ifdef __FreeBSD__
static int
i915_probe(device_t dev)
{
	return drm_probe(dev, i915_pciidlist);
}

static int
i915_attach(device_t nbdev)
{
	drm_device_t *dev = device_get_softc(nbdev);

	bzero(dev, sizeof(drm_device_t));
	i915_configure(dev);
	return drm_attach(nbdev, i915_pciidlist);
}

static device_method_t i915_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		i915_probe),
	DEVMETHOD(device_attach,	i915_attach),
	DEVMETHOD(device_detach,	drm_detach),

	{ 0, 0 }
};

static driver_t i915_driver = {
#if __FreeBSD_version >= 700010
	"drm",
#else
	"drmsub",
#endif
	i915_methods,
	sizeof(drm_device_t)
};

extern devclass_t drm_devclass;
#if __FreeBSD_version >= 700010
DRIVER_MODULE(i915, vgapci, i915_driver, drm_devclass, 0, 0);
#else
DRIVER_MODULE(i915, agp, i915_driver, drm_devclass, 0, 0);
#endif
MODULE_DEPEND(i915, drm, 1, 1, 1);

#elif defined(__NetBSD__) || defined(__OpenBSD__)

int	i915drm_probe(struct device *, void *, void *);
void	i915drm_attach(struct device *, struct device *, void *);

int
#if defined(__OpenBSD__)
i915drm_probe(struct device *parent, void *match, void *aux)
#else
i915drm_probe(struct device *parent, struct cfdata *match, void *aux)
#endif
{
	DRM_DEBUG("\n");
	return drm_probe((struct pci_attach_args *)aux, i915_pciidlist);
}

void
i915drm_attach(struct device *parent, struct device *self, void *aux)
{
	struct pci_attach_args *pa = aux;
	drm_device_t *dev = (drm_device_t *)self;

	i915_configure(dev);

	drm_attach(self, pa, i915_pciidlist);
}

#if defined(__OpenBSD__)
struct cfattach inteldrm_ca = {
	sizeof(drm_device_t), i915drm_probe, i915drm_attach,
	drm_detach, drm_activate
};

struct cfdriver inteldrm_cd = {
	0, "inteldrm", DV_DULL
};

#else
#ifdef _LKM
CFDRIVER_DECL(i915drm, DV_TTY, NULL);
#else
CFATTACH_DECL(i915drm, sizeof(drm_device_t), i915drm_probe, i915drm_attach,
	drm_detach, drm_activate);
#endif
#endif

#endif
