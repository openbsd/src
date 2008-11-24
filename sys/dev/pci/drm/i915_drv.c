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

int	inteldrm_probe(struct device *, void *, void *);
void	inteldrm_attach(struct device *, struct device *, void *);
int	inteldrm_detach(struct device *, int);
int	inteldrm_ioctl(struct drm_device *, u_long, caddr_t, struct drm_file *);

static drm_pci_id_list_t inteldrm_pciidlist[] = {
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82830M_IGD,
	    CHIP_I830|CHIP_M},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82845G_IGD,
	    CHIP_I845G},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82855GM_IGD,
	    CHIP_I85X|CHIP_M},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82865G_IGD,
	    CHIP_I865G},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82915G_IGD_1,
	    CHIP_I915G|CHIP_I9XX},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_E7221_IGD,
	    CHIP_I915G|CHIP_I9XX},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82915GM_IGD_1,
	    CHIP_I915GM|CHIP_I9XX|CHIP_M},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82945G_IGD_1,
	    CHIP_I945G|CHIP_I9XX},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82945GM_IGD_1,
	    CHIP_I945GM|CHIP_I9XX|CHIP_M},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82945GME_IGD_1,
	    CHIP_I945GM|CHIP_I9XX|CHIP_M},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82946GZ_IGD_1,
	    CHIP_I965|CHIP_I9XX},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82G35_IGD_1,
	    CHIP_I965|CHIP_I9XX},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82Q965_IGD_1,
	    CHIP_I965|CHIP_I9XX},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82G965_IGD_1,
	    CHIP_I965|CHIP_I9XX},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82GM965_IGD_1,
	    CHIP_I965GM|CHIP_I965|CHIP_I9XX|CHIP_M},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82GME965_IGD_1,
	    CHIP_I965|CHIP_I9XX},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82G33_IGD_1,
	    CHIP_G33|CHIP_I9XX|CHIP_HWS},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82Q35_IGD_1,
	    CHIP_G33|CHIP_I9XX|CHIP_HWS},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82Q33_IGD_1,
	    CHIP_G33|CHIP_I9XX|CHIP_HWS},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82GM45_IGD_1,
	    CHIP_GM45|CHIP_I965|CHIP_I9XX|CHIP_M|CHIP_HWS},
	{PCI_VENDOR_INTEL, 0x2E02,
	    CHIP_G4X|CHIP_I965|CHIP_I9XX|CHIP_HWS},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82Q45_IGD_1,
	    CHIP_G4X|CHIP_I965|CHIP_I9XX|CHIP_HWS},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82G45_IGD_1,
	    CHIP_G4X|CHIP_I965|CHIP_I9XX|CHIP_HWS},
	{0, 0, 0}
};

static const struct drm_driver_info inteldrm_driver = {
	.buf_priv_size		= 1,	/* No dev_priv */
	.ioctl			= inteldrm_ioctl,
	.preclose		= i915_driver_preclose,
	.lastclose		= i915_driver_lastclose,
	.device_is_agp		= i915_driver_device_is_agp,
	.vblank_pipes		= 2,
	.get_vblank_counter	= i915_get_vblank_counter,
	.enable_vblank		= i915_enable_vblank,
	.disable_vblank		= i915_disable_vblank,
	.irq_install		= i915_driver_irq_install,
	.irq_uninstall		= i915_driver_irq_uninstall,
	.irq_handler		= i915_driver_irq_handler,

	.name			= DRIVER_NAME,
	.desc			= DRIVER_DESC,
	.date			= DRIVER_DATE,
	.major			= DRIVER_MAJOR,
	.minor			= DRIVER_MINOR,
	.patchlevel		= DRIVER_PATCHLEVEL,

	.flags			= DRIVER_AGP | DRIVER_AGP_REQUIRE |
				    DRIVER_MTRR | DRIVER_IRQ,
};

int
inteldrm_probe(struct device *parent, void *match, void *aux)
{
	return drm_pciprobe((struct pci_attach_args *)aux, inteldrm_pciidlist);
}

void
inteldrm_attach(struct device *parent, struct device *self, void *aux)
{
	struct drm_i915_private	*dev_priv = (struct drm_i915_private *)self;
	struct pci_attach_args	*pa = aux;
	struct vga_pci_bar	*bar;
	drm_pci_id_list_t	*id_entry;

	id_entry = drm_find_description(PCI_VENDOR(pa->pa_id),
	    PCI_PRODUCT(pa->pa_id), inteldrm_pciidlist);
	dev_priv->flags = id_entry->driver_private;
	dev_priv->pci_device = PCI_PRODUCT(pa->pa_id);
	dev_priv->pc = pa->pa_pc;

	/* Add register map (needed for suspend/resume) */
	bar = vga_pci_bar_info((struct vga_pci_softc *)parent,
	    (IS_I9XX(dev_priv) ? 0 : 1));
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

	/* Init HWS */
	if (!I915_NEED_GFX_HWS(dev_priv)) {
		if (i915_init_phys_hws(dev_priv, pa->pa_dmat) != 0) {
			printf(": couldn't initialize hardware status page\n");
			return;
		}
	}

	mtx_init(&dev_priv->user_irq_lock, IPL_BIO);

	dev_priv->drmdev = drm_attach_mi(&inteldrm_driver, pa->pa_dmat, pa, self);
}

int
inteldrm_detach(struct device *self, int flags)
{
	struct drm_i915_private *dev_priv = (struct drm_i915_private *)self;

	if (dev_priv->drmdev != NULL) {
		config_detach(dev_priv->drmdev, flags);
		dev_priv->drmdev = NULL;
	}

	i915_free_hws(dev_priv, dev_priv->dmat);

	if (dev_priv->regs != NULL)
		vga_pci_bar_unmap(dev_priv->regs);

	DRM_SPINUNINIT(&dev_priv->user_irq_lock);

	return (0);
}

struct cfattach inteldrm_ca = {
	sizeof(struct drm_i915_private), inteldrm_probe, inteldrm_attach,
	inteldrm_detach
};

struct cfdriver inteldrm_cd = {
	0, "inteldrm", DV_DULL
};

int
inteldrm_ioctl(struct drm_device *dev, u_long cmd, caddr_t data,
    struct drm_file *file_priv)
{
	if (file_priv->authenticated == 1) {
		switch (cmd) {
		case DRM_IOCTL_I915_FLUSH:
			return (i915_flush_ioctl(dev, data, file_priv));
		case DRM_IOCTL_I915_FLIP:
			return (i915_flip_bufs(dev, data, file_priv));
		case DRM_IOCTL_I915_BATCHBUFFER:
			return (i915_batchbuffer(dev, data, file_priv));
		case DRM_IOCTL_I915_IRQ_EMIT:
			return (i915_irq_emit(dev, data, file_priv));
		case DRM_IOCTL_I915_IRQ_WAIT:
			return (i915_irq_wait(dev, data, file_priv));
		case DRM_IOCTL_I915_GETPARAM:
			return (i915_getparam(dev, data, file_priv));
		case DRM_IOCTL_I915_ALLOC:
			return (i915_mem_alloc(dev, data, file_priv));
		case DRM_IOCTL_I915_FREE:
			return (i915_mem_free(dev, data, file_priv));
		case DRM_IOCTL_I915_CMDBUFFER:
			return (i915_cmdbuffer(dev, data, file_priv));
		case DRM_IOCTL_I915_GET_VBLANK_PIPE:
			return (i915_vblank_pipe_get(dev, data, file_priv));
		case DRM_IOCTL_I915_VBLANK_SWAP:
			/*
			 * removed due to being racy. Userland falls back
			 * correctly when it errors out
			 */
			return (EINVAL);
		}
	}

	if (file_priv->master == 1) {
		switch (cmd) {
		case DRM_IOCTL_I915_SETPARAM:
			return (i915_setparam(dev, data, file_priv));
		case DRM_IOCTL_I915_INIT:
			return (i915_dma_init(dev, data, file_priv));
		case DRM_IOCTL_I915_INIT_HEAP:
			return (i915_mem_init_heap(dev, data, file_priv));
		case DRM_IOCTL_I915_DESTROY_HEAP:
			return (i915_mem_destroy_heap(dev, data, file_priv));
		case DRM_IOCTL_I915_HWS_ADDR:
			return (i915_set_status_page(dev, data, file_priv));
		case DRM_IOCTL_I915_SET_VBLANK_PIPE:
			return (0);
		}
	}
	return (EINVAL);
}
