/* r128_drv.c -- ATI Rage 128 driver -*- linux-c -*-
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
 *
 */

#include "drmP.h"
#include "drm.h"
#include "r128_drm.h"
#include "r128_drv.h"

int	ragedrm_probe(struct device *, void *, void *);
void	ragedrm_attach(struct device *, struct device *, void *);
int	ragedrm_detach(struct device *, int);
int	ragedrm_ioctl(struct drm_device *, u_long, caddr_t, struct drm_file *);

static drm_pci_id_list_t ragedrm_pciidlist[] = {
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_LE},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_MOBILITY_M3},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_MF},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_ML},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_PA},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_PB},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_PC},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_PD},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_PE},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE_FURY},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_PG},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_PH},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_PI},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_PJ},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_PK},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_PL},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_PM},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_PN},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_PO},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_PP},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_PQ},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_PR},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_PS},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_PT},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_PU},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_PV},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_PW},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_PX},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_GL},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE_MAGNUM},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_RG},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_RK},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_VR},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_SM},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_TF},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_TL},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_TR},
	{0, 0, 0}
};

static const struct drm_driver_info ragedrm_driver = {
	.buf_priv_size		= sizeof(drm_r128_buf_priv_t),
	.ioctl			= ragedrm_ioctl,
	.close			= r128_driver_close,
	.lastclose		= r128_driver_lastclose,
	.vblank_pipes		= 1,
	.get_vblank_counter	= r128_get_vblank_counter,
	.enable_vblank 		= r128_enable_vblank,
	.disable_vblank		= r128_disable_vblank,
	.irq_install		= r128_driver_irq_install,
	.irq_uninstall		= r128_driver_irq_uninstall,
	.dma_ioctl		= r128_cce_buffers,

	.name			= DRIVER_NAME,
	.desc			= DRIVER_DESC,
	.date			= DRIVER_DATE,
	.major			= DRIVER_MAJOR,
	.minor			= DRIVER_MINOR,
	.patchlevel		= DRIVER_PATCHLEVEL,

	.flags			= DRIVER_AGP | DRIVER_MTRR | DRIVER_SG |
				    DRIVER_DMA | DRIVER_IRQ,
};

int
ragedrm_probe(struct device *parent, void *match, void *aux)
{
	return drm_pciprobe((struct pci_attach_args *)aux, ragedrm_pciidlist);
}

void
ragedrm_attach(struct device *parent, struct device *self, void *aux)
{
	drm_r128_private_t	*dev_priv = (drm_r128_private_t *)self;
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

	dev_priv->drmdev = drm_attach_pci(&ragedrm_driver, pa, is_agp, self);
}

int
ragedrm_detach(struct device *self, int flags)
{
	drm_r128_private_t	*dev_priv = (drm_r128_private_t *)self;

	if (dev_priv->drmdev != NULL) {
		config_detach(dev_priv->drmdev, flags);
		dev_priv->drmdev = NULL;
	}

	if (dev_priv->regs != NULL)
		vga_pci_bar_unmap(dev_priv->regs);

	return (0);
}

struct cfattach ragedrm_ca = {
	sizeof(drm_r128_private_t), ragedrm_probe, ragedrm_attach,
	ragedrm_detach
};

struct cfdriver ragedrm_cd = {
	0, "ragedrm", DV_DULL
};

int
ragedrm_ioctl(struct drm_device *dev, u_long cmd, caddr_t data,
    struct drm_file *file_priv)
{
	if (file_priv->authenticated == 1) {
		switch (cmd) {
		case DRM_IOCTL_R128_CCE_IDLE:
			return (r128_cce_idle(dev, data, file_priv));
		case DRM_IOCTL_R128_RESET:
			return (r128_engine_reset(dev, data, file_priv));
		case DRM_IOCTL_R128_FULLSCREEN:
			return (r128_fullscreen(dev, data, file_priv));
		case DRM_IOCTL_R128_SWAP:
			return (r128_cce_swap(dev, data, file_priv));
		case DRM_IOCTL_R128_FLIP:
			return (r128_cce_flip(dev, data, file_priv));
		case DRM_IOCTL_R128_CLEAR:
			return (r128_cce_clear(dev, data, file_priv));
		case DRM_IOCTL_R128_VERTEX:
			return (r128_cce_vertex(dev, data, file_priv));
		case DRM_IOCTL_R128_INDICES:
			return (r128_cce_indices(dev, data, file_priv));
		case DRM_IOCTL_R128_BLIT:
			return (r128_cce_blit(dev, data, file_priv));
		case DRM_IOCTL_R128_DEPTH:
			return (r128_cce_depth(dev, data, file_priv));
		case DRM_IOCTL_R128_STIPPLE:
			return (r128_cce_stipple(dev, data, file_priv));
		case DRM_IOCTL_R128_GETPARAM:
			return (r128_getparam(dev, data, file_priv));
		}
	}

	if (file_priv->master == 1) {
		switch (cmd) {
		case DRM_IOCTL_R128_INIT:
			return (r128_cce_init(dev, data, file_priv));
		case DRM_IOCTL_R128_CCE_START:
			return (r128_cce_start(dev, data, file_priv));
		case DRM_IOCTL_R128_CCE_STOP:
			return (r128_cce_stop(dev, data, file_priv));
		case DRM_IOCTL_R128_CCE_RESET:
			return (r128_cce_reset(dev, data, file_priv));
		case DRM_IOCTL_R128_INDIRECT:
			return (r128_cce_indirect(dev, data, file_priv));
		}
	}
	return (EINVAL);
}
