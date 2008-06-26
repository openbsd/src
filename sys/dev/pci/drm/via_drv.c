/* via_drv.c -- VIA unichrome driver -*- linux-c -*-
 * Created: Fri Aug 12 2005 by anholt@FreeBSD.org
 */
/*-
 * Copyright 2005 Eric Anholt
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

#include "drmP.h"
#include "drm.h"
#include "via_drm.h"
#include "via_drv.h"
#include "drm_pciids.h"

void	via_configure(struct drm_device *);

/* drv_PCI_IDs comes from drm_pciids.h, generated from drm_pciids.txt. */
static drm_pci_id_list_t via_pciidlist[] = {
	viadrv_PCI_IDS
};

void
via_configure(struct drm_device *dev)
{
	dev->driver.buf_priv_size	= 1;
	dev->driver.load		= via_driver_load;
	dev->driver.unload		= via_driver_unload;
	dev->driver.context_ctor	= via_init_context;
	dev->driver.context_dtor	= via_final_context;
	dev->driver.get_vblank_counter	= via_get_vblank_counter;
	dev->driver.enable_vblank	= via_enable_vblank;
	dev->driver.disable_vblank	= via_disable_vblank;
	dev->driver.irq_preinstall	= via_driver_irq_preinstall;
	dev->driver.irq_postinstall	= via_driver_irq_postinstall;
	dev->driver.irq_uninstall	= via_driver_irq_uninstall;
	dev->driver.irq_handler		= via_driver_irq_handler;
	dev->driver.dma_quiescent	= via_driver_dma_quiescent;

	dev->driver.ioctls		= via_ioctls;
	dev->driver.max_ioctl		= via_max_ioctl;

	dev->driver.name		= DRIVER_NAME;
	dev->driver.desc		= DRIVER_DESC;
	dev->driver.date		= DRIVER_DATE;
	dev->driver.major		= DRIVER_MAJOR;
	dev->driver.minor		= DRIVER_MINOR;
	dev->driver.patchlevel		= DRIVER_PATCHLEVEL;

	dev->driver.use_agp		= 1;
	dev->driver.use_mtrr		= 1;
	dev->driver.use_irq		= 1;
	dev->driver.use_vbl_irq		= 1;
}

#ifdef __FreeBSD__
static int
via_probe(device_t dev)
{
	return drm_probe(dev, via_pciidlist);
}

static int
via_attach(device_t nbdev)
{
	struct drm_device *dev = device_get_softc(nbdev);

	bzero(dev, sizeof(struct drm_device));
	via_configure(dev);
	return drm_attach(nbdev, via_pciidlist);
}

static device_method_t via_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		via_probe),
	DEVMETHOD(device_attach,	via_attach),
	DEVMETHOD(device_detach,	drm_detach),

	{ 0, 0 }
};

static driver_t via_driver = {
	"drm",
	via_methods,
	sizeof(struct drm_device)
};

extern devclass_t drm_devclass;
DRIVER_MODULE(via, pci, via_driver, drm_devclass, 0, 0);
MODULE_DEPEND(via, drm, 1, 1, 1);

#elif defined(__NetBSD__) || defined(__OpenBSD__)

int	viadrm_probe(struct device *, void *, void *);
void	viadrm_attach(struct device *, struct device *, void *);

int
#if defined(__OpenBSD__)
viadrm_probe(struct device *parent, void *match, void *aux)
#else
viadrm_probe(struct device *parent, struct cfdata *match, void *aux)
#endif
{
	return drm_probe((struct pci_attach_args *)aux, via_pciidlist);
}

void
viadrm_attach(struct device *parent, struct device *self, void *opaque)
{
	struct pci_attach_args *pa = opaque;
	struct drm_device *dev = (struct drm_device *)self;

	viadrm_configure(dev);
	drm_attach(parent, self, pa, via_pciidlist);
}

#if defined(__OpenBSD__)
struct cfattach viadrm_ca = {
	sizeof(struct drm_device), viadrm_probe, viadrm_attach,
	drm_detach, drm_activate
};

struct cfdriver viadrm_cd = {
	0, "viadrm", DV_DULL
};
#else
#ifdef _LKM
CFDRIVER_DECL(viadrm, DV_TTY, NULL);
#else
CFATTACH_DECL(viadrm, sizeof(struct drm_device), viadrm_probe, viadrm_attach,
	drm_detach, drm_activate);
#endif
#endif

#endif
