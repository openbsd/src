/* mga_drv.c -- Matrox G200/G400 driver -*- linux-c -*-
 * Created: Mon Dec 13 01:56:22 1999 by jhartmann@precisioninsight.com
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
 *
 */

#include "drmP.h"
#include "drm.h"
#include "mga_drm.h"
#include "mga_drv.h"

int	mgadrm_probe(struct device *, void *, void *);
void	mgadrm_attach(struct device *, struct device *, void *);
int	mgadrm_detach(struct device *, int);
int	mgadrm_ioctl(struct drm_device *, u_long, caddr_t, struct drm_file *);

#define MGA_DEFAULT_USEC_TIMEOUT	10000

const static struct drm_pcidev mgadrm_pciidlist[] = {
	{PCI_VENDOR_MATROX, PCI_PRODUCT_MATROX_MILL_II_G200_PCI,
	    MGA_CARD_TYPE_G200},
	{PCI_VENDOR_MATROX, PCI_PRODUCT_MATROX_MILL_II_G200_AGP,
	    MGA_CARD_TYPE_G200},
	{PCI_VENDOR_MATROX, PCI_PRODUCT_MATROX_MILL_II_G400_AGP,
	    MGA_CARD_TYPE_G400},
	{PCI_VENDOR_MATROX, PCI_PRODUCT_MATROX_MILL_II_G550_AGP,
	    MGA_CARD_TYPE_G550},
	{0, 0, 0}
};

static const struct drm_driver_info mga_driver = {
	.buf_priv_size		= sizeof(struct mgadrm_buf_priv),
	.ioctl			= mgadrm_ioctl,
	.lastclose		= mga_driver_lastclose,
	.vblank_pipes		= 1,
	.enable_vblank		= mga_enable_vblank,
	.disable_vblank		= mga_disable_vblank,
	.get_vblank_counter	= mga_get_vblank_counter,
	.irq_install		= mga_driver_irq_install,
	.irq_uninstall		= mga_driver_irq_uninstall,
	.dma_ioctl		= mga_dma_buffers,
	.dma_quiescent		= mga_driver_dma_quiescent,

	.name			= DRIVER_NAME,
	.desc			= DRIVER_DESC,
	.date			= DRIVER_DATE,
	.major			= DRIVER_MAJOR,
	.minor			= DRIVER_MINOR,
	.patchlevel		= DRIVER_PATCHLEVEL,

	.flags			= DRIVER_AGP | DRIVER_AGP_REQUIRE |
				    DRIVER_MTRR | DRIVER_DMA | DRIVER_IRQ,
};

int
mgadrm_probe(struct device *parent, void *match, void *aux)
{
	return drm_pciprobe((struct pci_attach_args *)aux, mgadrm_pciidlist);
}

void
mgadrm_attach(struct device *parent, struct device *self, void *aux)
{
	drm_mga_private_t	*dev_priv = (drm_mga_private_t *)self;
	struct pci_attach_args	*pa = aux;
	struct vga_pci_bar	*bar;
	const struct drm_pcidev	*id_entry;
	int			 is_agp;

	dev_priv->usec_timeout = MGA_DEFAULT_USEC_TIMEOUT;
	dev_priv->pc = pa->pa_pc;

	id_entry = drm_find_description(PCI_VENDOR(pa->pa_id),
	    PCI_PRODUCT(pa->pa_id), mgadrm_pciidlist);
	dev_priv->chipset = id_entry->driver_private;

	bar = vga_pci_bar_info((struct vga_pci_softc *)parent, 1);
	if (bar == NULL) {
		printf(": couldn't get BAR info\n");
		return;
	}
	dev_priv->regs = vga_pci_bar_map((struct vga_pci_softc *)parent, 
	    bar->addr, 0, 0);
	if (dev_priv->regs == NULL) {
		printf(": can't map mmio space\n");
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
	mtx_init(&dev_priv->fence_lock, IPL_TTY);

	/* XXX pcie */
	is_agp = pci_get_capability(pa->pa_pc, pa->pa_tag, PCI_CAP_AGP,
	    NULL, NULL);

	dev_priv->drmdev = drm_attach_pci(&mga_driver, pa, is_agp, self);
}

int
mgadrm_detach(struct device *self, int flags)
{
	drm_mga_private_t	*dev_priv = (drm_mga_private_t *)self;

	if (dev_priv->drmdev != NULL) {
		config_detach(dev_priv->drmdev, flags);
		dev_priv->drmdev = NULL;
	}

	if (dev_priv->regs != NULL)
		vga_pci_bar_unmap(dev_priv->regs);

	return (0);
}

struct cfattach mgadrm_ca = {
	sizeof(drm_mga_private_t), mgadrm_probe, mgadrm_attach,
	mgadrm_detach
};

struct cfdriver mgadrm_cd = {
	0, "mgadrm", DV_DULL
};

int
mgadrm_ioctl(struct drm_device *dev, u_long cmd, caddr_t data,
    struct drm_file *file_priv)
{
	if (file_priv->authenticated == 1) {
		switch (cmd) {
		case DRM_IOCTL_MGA_FLUSH:
			return (mga_dma_flush(dev, data, file_priv));
		case DRM_IOCTL_MGA_RESET:
			return (mga_dma_reset(dev, data, file_priv));
		case DRM_IOCTL_MGA_SWAP:
			return (mga_dma_swap(dev, data, file_priv));
		case DRM_IOCTL_MGA_CLEAR:
			return (mga_dma_clear(dev, data, file_priv));
		case DRM_IOCTL_MGA_VERTEX:
			return (mga_dma_vertex(dev, data, file_priv));
		case DRM_IOCTL_MGA_INDICES:
			return (mga_dma_indices(dev, data, file_priv));
		case DRM_IOCTL_MGA_ILOAD:
			return (mga_dma_iload(dev, data, file_priv));
		case DRM_IOCTL_MGA_BLIT:
			return (mga_dma_blit(dev, data, file_priv));
		case DRM_IOCTL_MGA_GETPARAM:
			return (mga_getparam(dev, data, file_priv));
		case DRM_IOCTL_MGA_SET_FENCE:
			return (mga_set_fence(dev, data, file_priv));
		case DRM_IOCTL_MGA_WAIT_FENCE:
			return (mga_wait_fence(dev, data, file_priv));
		}
	}

	if (file_priv->master == 1) {
		switch (cmd) {
		case DRM_IOCTL_MGA_INIT:
			return (mga_dma_init(dev, data, file_priv));
		case DRM_IOCTL_MGA_DMA_BOOTSTRAP:
			return (mga_dma_bootstrap(dev, data, file_priv));
		}
	}
	return (EINVAL);

}
