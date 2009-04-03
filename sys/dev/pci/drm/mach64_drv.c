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

int	machdrm_probe(struct device *, void *, void *);
void	machdrm_attach(struct device *, struct device *, void *);
int	machdrm_detach(struct device *, int);
int	machdrm_ioctl(struct drm_device *, u_long, caddr_t, struct drm_file *);

const static drm_pci_id_list_t mach64_pciidlist[] = {
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_MACH64_GI},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_MACH64_GP},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_MACH64_GQ},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGEPRO},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_MACH64_GD},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_MACH64_LI},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_MACH64_LP},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_MACH64_LQ},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_MACH64_LB},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_MACH64_LD},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_MACH64_GL},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_MACH64_GO},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGEXL},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_MACH64_GS},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_MACH64_GM},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_MACH64_GN},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE_PM},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_MACH64LS},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_MOBILITY_1},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_MACH64_LN},
	{0, 0, 0}
};

static const struct drm_driver_info machdrm_driver = {
	.buf_priv_size		= 1, /* No dev_priv */
	.ioctl			= machdrm_ioctl,
	.lastclose		= mach64_driver_lastclose,
	.vblank_pipes		= 1,
	.get_vblank_counter	= mach64_get_vblank_counter,
	.enable_vblank		= mach64_enable_vblank,
	.disable_vblank		= mach64_disable_vblank,
	.irq_install		= mach64_driver_irq_install,
	.irq_uninstall		= mach64_driver_irq_uninstall,
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
machdrm_probe(struct device *parent, void *match, void *aux)
{
	return drm_pciprobe((struct pci_attach_args *)aux, mach64_pciidlist);
}

void
machdrm_attach(struct device *parent, struct device *self, void *aux)
{
	drm_mach64_private_t	*dev_priv = (drm_mach64_private_t *)self;
	struct pci_attach_args	*pa = aux;
	struct vga_pci_bar	*bar;
	int			 is_agp;

	dev_priv->pc = pa->pa_pc;

	bar = vga_pci_bar_info((struct vga_pci_softc *)parent, 2);
	if (bar == NULL) {
		printf(": can't get BAR info\n");
		return;
	}

	dev_priv->regs = vga_pci_bar_map((struct vga_pci_softc *)parent, 
	    bar->addr, 0, 0);
	if (dev_priv->regs == NULL) {
		printf(": can't map mmio space\n");
		return;
	}

	if (pci_intr_map(pa, &dev_priv->ih) != 0) {
		printf(": couldn't map interrupt\n");
		return;
	}
	printf(": %s\n", pci_intr_string(pa->pa_pc, dev_priv->ih));

	is_agp = pci_get_capability(pa->pa_pc, pa->pa_tag, PCI_CAP_AGP,
	    NULL, NULL);

	dev_priv->drmdev = drm_attach_pci(&machdrm_driver, pa, is_agp, self);
}

int
machdrm_detach(struct device *self, int flags)
{
	drm_mach64_private_t *dev_priv = (drm_mach64_private_t *)self;

	if (dev_priv->drmdev != NULL) {
		config_detach(dev_priv->drmdev, flags);
		dev_priv->drmdev = NULL;
	}

	if (dev_priv->regs != NULL)
		vga_pci_bar_unmap(dev_priv->regs);

	return (0);
}

struct cfattach machdrm_ca = {
	sizeof(drm_mach64_private_t), machdrm_probe, machdrm_attach,
	machdrm_detach
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
