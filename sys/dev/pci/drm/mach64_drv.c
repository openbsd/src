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

int	mach64drm_probe(struct device *, void *, void *);
void	mach64drm_attach(struct device *, struct device *, void *);
int	machdrm_ioctl(struct drm_device *, u_long, caddr_t, struct drm_file *);

/* drv_PCI_IDs comes from drm_pciids.h, generated from drm_pciids.txt. */
static drm_pci_id_list_t mach64_pciidlist[] = {
	mach64_PCI_IDS
};

static const struct drm_driver_info mach64_driver = {
	.buf_priv_size		= 1, /* No dev_priv */
	.ioctl			= machdrm_ioctl,
	.lastclose		= mach64_driver_lastclose,
	.get_vblank_counter	= mach64_get_vblank_counter,
	.enable_vblank		= mach64_enable_vblank,
	.disable_vblank		= mach64_disable_vblank,
	.irq_preinstall		= mach64_driver_irq_preinstall,
	.irq_postinstall	= mach64_driver_irq_postinstall,
	.irq_uninstall		= mach64_driver_irq_uninstall,
	.irq_handler		= mach64_driver_irq_handler,
	.dma_ioctl		= mach64_dma_buffers,

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

int
mach64drm_probe(struct device *parent, void *match, void *aux)
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

struct cfattach machdrm_ca = {
	sizeof(struct drm_device), mach64drm_probe, mach64drm_attach,
	drm_detach, drm_activate
};

struct cfdriver machdrm_cd = {
	0, "machdrm", DV_DULL
};

int
machdrm_ioctl(struct drm_device *dev, u_long cmd, caddr_t data,
    struct drm_file *file_priv)
{
	if (file_priv->authenticated == 1) {
		switch (cmd) {
		case DRM_IOCTL_MACH64_CLEAR:
			return (mach64_dma_clear(dev, data, file_priv));
		case DRM_IOCTL_MACH64_SWAP:
			return (mach64_dma_swap(dev, data, file_priv));
		case DRM_IOCTL_MACH64_IDLE:
			return (mach64_dma_idle(dev, data, file_priv));
		case DRM_IOCTL_MACH64_RESET:
			return (mach64_engine_reset(dev, data, file_priv));
		case DRM_IOCTL_MACH64_VERTEX:
			return (mach64_dma_vertex(dev, data, file_priv));
		case DRM_IOCTL_MACH64_BLIT:
			return (mach64_dma_blit(dev, data, file_priv));
		case DRM_IOCTL_MACH64_FLUSH:
			return (mach64_dma_flush(dev, data, file_priv));
		case DRM_IOCTL_MACH64_GETPARAM:
			return (mach64_get_param(dev, data, file_priv));
		}
	}

	if (file_priv->master == 1) {
		switch (cmd) {
		case DRM_IOCTL_MACH64_INIT:
			return (mach64_dma_init(dev, data, file_priv));
		}
	}
	return (EINVAL);
}
