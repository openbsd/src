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

int	i915drm_probe(struct device *, void *, void *);
void	i915drm_attach(struct device *, struct device *, void *);
int	inteldrm_ioctl(struct drm_device *, u_long, caddr_t, struct drm_file *);

static drm_pci_id_list_t i915_pciidlist[] = {
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82830M_IGD, CHIP_I8XX, "Intel i830M GMCH"},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82845G_IGD, CHIP_I8XX, "Intel i845G GMCH"},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82855GM_IGD, CHIP_I8XX, "Intel i852GM/i855GM GMCH"},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82865G_IGD, CHIP_I8XX, "Intel i865G GMCH"},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82915G_IGD_1, CHIP_I9XX|CHIP_I915, "Intel i915G"},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_E7221_IGD, CHIP_I9XX|CHIP_I915, "Intel E7221 (i915)"},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82915GM_IGD_1, CHIP_I9XX|CHIP_I915, "Intel i915GM"},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82945G_IGD_1, CHIP_I9XX|CHIP_I915, "Intel i945G"},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82945GM_IGD_1, CHIP_I9XX|CHIP_I915, "Intel i945GM"},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82945GME_IGD_1, CHIP_I9XX|CHIP_I915, "Intel i945GME"},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82946GZ_IGD_1, CHIP_I9XX|CHIP_I965, "Intel i946GZ"},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82G35_IGD_1, CHIP_I9XX|CHIP_I965, "Intel i965G"},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82Q965_IGD_1, CHIP_I9XX|CHIP_I965, "Intel i965Q"},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82G965_IGD_1, CHIP_I9XX|CHIP_I965, "Intel i965G"},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82GM965_IGD_1, CHIP_I9XX|CHIP_I965, "Intel i965GM"},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82GME965_IGD_1, CHIP_I9XX|CHIP_I965, "Intel i965GME/GLE"},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82G33_IGD_1, CHIP_I9XX|CHIP_I915, "Intel G33"},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82Q35_IGD_1, CHIP_I9XX|CHIP_I915, "Intel Q35"},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82Q33_IGD_1, CHIP_I9XX|CHIP_I915, "Intel Q33"},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82GM45_IGD_1, CHIP_I9XX|CHIP_I965, "Mobile Intel GM45 Express Chipset"},
	{PCI_VENDOR_INTEL, 0x2E02, CHIP_I9XX|CHIP_I965, "Intel Integrated Graphics Device"},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82Q45_IGD_1, CHIP_I9XX|CHIP_I965, "Intel Q45/Q43"},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82G45_IGD_1, CHIP_I9XX|CHIP_I965, "Intel G45/G43"},
	{0, 0, 0, NULL}
};

static const struct drm_driver_info i915_driver = {
	.buf_priv_size		= 1,	/* No dev_priv */
	.load			= i915_driver_load,
	.ioctl			= inteldrm_ioctl,
	.preclose		= i915_driver_preclose,
	.lastclose		= i915_driver_lastclose,
	.device_is_agp		= i915_driver_device_is_agp,
	.get_vblank_counter	= i915_get_vblank_counter,
	.enable_vblank		= i915_enable_vblank,
	.disable_vblank		= i915_disable_vblank,
	.irq_preinstall		= i915_driver_irq_preinstall,
	.irq_postinstall	= i915_driver_irq_postinstall,
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
i915drm_probe(struct device *parent, void *match, void *aux)
{
	return drm_probe((struct pci_attach_args *)aux, i915_pciidlist);
}

void
i915drm_attach(struct device *parent, struct device *self, void *aux)
{
	struct pci_attach_args *pa = aux;
	struct drm_device *dev = (struct drm_device *)self;

	dev->driver = &i915_driver;

	drm_attach(parent, self, pa, i915_pciidlist);
}

struct cfattach inteldrm_ca = {
	sizeof(struct drm_device), i915drm_probe, i915drm_attach,
	drm_detach, drm_activate
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
