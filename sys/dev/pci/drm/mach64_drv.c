/* mach64_drv.c -- ATI Rage 128 driver -*- linux-c -*-
 * Created: Mon Dec 13 09:47:27 1999 by faith@precisioninsight.com
 */
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
 */


#include <sys/types.h>

#include "drmP.h"
#include "drm.h"
#include "mach64_drm.h"
#include "mach64_drv.h"
#include "drm_pciids.h"

/* drv_PCI_IDs comes from drm_pciids.h, generated from drm_pciids.txt. */
static drm_pci_id_list_t mach64_pciidlist[] = {
	mach64_PCI_IDS
};

/* Interface history:
 *
 * 1.0 - Initial mach64 DRM
 *
 */
struct drm_ioctl_desc mach64_ioctls[] = {
	DRM_IOCTL_DEF(DRM_MACH64_INIT, mach64_dma_init,
	    DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF(DRM_MACH64_CLEAR, mach64_dma_clear, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_MACH64_SWAP, mach64_dma_swap, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_MACH64_IDLE, mach64_dma_idle, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_MACH64_RESET, mach64_engine_reset, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_MACH64_VERTEX, mach64_dma_vertex, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_MACH64_BLIT, mach64_dma_blit, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_MACH64_FLUSH, mach64_dma_flush, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_MACH64_GETPARAM, mach64_get_param, DRM_AUTH),
};

static const struct drm_driver_info mach64_driver = {
	.buf_priv_size		= 1, /* No dev_priv */
	.lastclose		= mach64_driver_lastclose,
	.get_vblank_counter	= mach64_get_vblank_counter,
	.enable_vblank		= mach64_enable_vblank,
	.disable_vblank		= mach64_disable_vblank,
	.irq_preinstall		= mach64_driver_irq_preinstall,
	.irq_postinstall	= mach64_driver_irq_postinstall,
	.irq_uninstall		= mach64_driver_irq_uninstall,
	.irq_handler		= mach64_driver_irq_handler,
	.dma_ioctl		= mach64_dma_buffers,

	.ioctls			= mach64_ioctls,
	.max_ioctl		= DRM_ARRAY_SIZE(mach64_ioctls),

	.name			= DRIVER_NAME,
	.desc			= DRIVER_DESC,
	.date			= DRIVER_DATE,
	.major			= DRIVER_MAJOR,
	.minor			= DRIVER_MINOR,
	.patchlevel		= DRIVER_PATCHLEVEL,

	.use_agp		= 1,
	.use_mtrr		= 1,
	.use_pci_dma		= 1,
	.use_dma		= 1,
	.use_irq		= 1,
	.use_vbl_irq		= 1,
};

#ifdef __FreeBSD__
static int
mach64_probe(device_t dev)
{
	return drm_probe(dev, mach64_pciidlist);
}

static int
mach64_attach(device_t nbdev)
{
	struct drm_device *dev = device_get_softc(nbdev);

	bzero(dev, sizeof(struct drm_device));
	mach64_configure(dev);
	return drm_attach(nbdev, mach64_pciidlist);
}

static device_method_t mach64_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		mach64_probe),
	DEVMETHOD(device_attach,	mach64_attach),
	DEVMETHOD(device_detach,	drm_detach),

	{ 0, 0 }
};

static driver_t mach64_driver = {
	"drm",
	mach64_methods,
	sizeof(struct drm_device)
};

extern devclass_t drm_devclass;
#if __FreeBSD_version >= 700010
DRIVER_MODULE(mach64, vgapci, mach64_driver, drm_devclass, 0, 0);
#else
DRIVER_MODULE(mach64, pci, mach64_driver, drm_devclass, 0, 0);
#endif
MODULE_DEPEND(mach64, drm, 1, 1, 1);

#elif defined(__NetBSD__) || defined(__OpenBSD__)

int	mach64drm_probe(struct device *, void *, void *);
void	mach64drm_attach(struct device *, struct device *, void *);

int
#if defined(__OpenBSD__)
mach64drm_probe(struct device *parent, void *match, void *aux)
#else
mach64drm_probe(struct device *parent, struct cfdata *match, void *aux)
#endif
{
	return drm_probe((struct pci_attach_args *)aux, mach64_pciidlist);
}

void
mach64drm_attach(struct device *parent, struct device *self, void *aux)
{
	struct pci_attach_args *pa = aux;
	struct drm_device *dev = (struct drm_device *)self;

	dev->driver = &mach64_driver;

	return drm_attach(parent, self, pa, mach64_pciidlist);
}

#if defined(__OpenBSD__)
struct cfattach machdrm_ca = {
	sizeof(struct drm_device), mach64drm_probe, mach64drm_attach,
	drm_detach, drm_activate
};

struct cfdriver machdrm_cd = {
	0, "machdrm", DV_DULL
};
#else
#ifdef _LKM
CFDRIVER_DECL(mach64drm, DV_TTY, NULL);
#else
CFATTACH_DECL(mach64drm, sizeof(struct drm_device), mach64drm_probe,
    mach64drm_attach, drm_detach, drm_activate);
#endif
#endif

#endif
