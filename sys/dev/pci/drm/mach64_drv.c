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

int	mach64drm_probe(struct device *, void *, void *);
void	mach64drm_attach(struct device *, struct device *, void *);
int	machdrm_ioctl(struct drm_device *, u_long, caddr_t, struct drm_file *);

static drm_pci_id_list_t mach64_pciidlist[] = {
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_MACH64_GI, 0, "3D Rage Pro"},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_MACH64_GP, 0, "3D Rage Pro 215GP"},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_MACH64_GQ, 0, "3D Rage Pro 215GQ"},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGEPRO, 0, "3D Rage Pro AGP 1X/2X"},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_MACH64_GD, 0, "3D Rage Pro AGP 1X"},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_MACH64_LI, 0, "3D Rage LT Pro"},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_MACH64_LP, 0, "3D Rage LT Pro"},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_MACH64_LQ, 0, "3D Rage LT Pro"},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_MACH64_LB, 0, "3D Rage LT Pro AGP-133"},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_MACH64_LD, 0, "3D Rage LT Pro AGP-66"},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_MACH64_GL, 0, "Rage XC"},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_MACH64_GO, 0, "Rage XL"},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGEXL, 0, "Rage XL"},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_MACH64_GS, 0, "Rage XC"},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_MACH64_GM, 0, "Rage XL AGP 2X"},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_MACH64_GN, 0, "Rage XC AGP"},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE_PM, 0, "Rage Mobility P/M"},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_MACH64LS, 0, "Rage Mobility L"},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_MOBILITY_1, 0, "Rage Mobility P/M AGP 2X"},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_MACH64_LN, 0, "Rage Mobility L AGP 2X"},
	{0, 0, 0, NULL}
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

	.flags			= DRIVER_AGP | DRIVER_MTRR | DRIVER_PCI_DMA |
				    DRIVER_DMA | DRIVER_SG | DRIVER_IRQ,
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
