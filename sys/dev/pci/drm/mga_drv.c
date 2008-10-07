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
#include "drm_pciids.h"

int	mga_driver_device_is_agp(struct drm_device * );

/* drv_PCI_IDs comes from drm_pciids.h, generated from drm_pciids.txt. */
static drm_pci_id_list_t mga_pciidlist[] = {
	mga_PCI_IDS
};

/**
 * Determine if the device really is AGP or not.
 *
 * In addition to the usual tests performed by \c drm_device_is_agp, this
 * function detects PCI G450 cards that appear to the system exactly like
 * AGP G450 cards.
 *
 * \param dev   The device to be tested.
 *
 * \returns
 * If the device is a PCI G450, zero is returned.  Otherwise non-zero is
 * returned.
 *
 * \bug
 * This function needs to be filled in!  The implementation in
 * linux-core/mga_drv.c shows what needs to be done.
 */
int
mga_driver_device_is_agp(struct drm_device * dev)
{
#ifdef __FreeBSD__
	device_t bus;

	/* There are PCI versions of the G450.  These cards have the
	 * same PCI ID as the AGP G450, but have an additional PCI-to-PCI
	 * bridge chip.  We detect these cards, which are not currently
	 * supported by this driver, by looking at the device ID of the
	 * bus the "card" is on.  If vendor is 0x3388 (Hint Corp) and the
	 * device is 0x0021 (HB6 Universal PCI-PCI bridge), we reject the
	 * device.
	 */
#if __FreeBSD_version >= 700010
	bus = device_get_parent(device_get_parent(dev->device));
#else
	bus = device_get_parent(dev->device);
#endif
	if (pci_get_device(dev->device) == 0x0525 &&
	    pci_get_vendor(bus) == 0x3388 &&
	    pci_get_device(bus) == 0x0021)
		return DRM_IS_NOT_AGP;
	else
#endif /* XXX Fixme for non freebsd */
		return DRM_MIGHT_BE_AGP;

}

struct drm_ioctl_desc mga_ioctls[] = {
	DRM_IOCTL_DEF(DRM_MGA_INIT, mga_dma_init, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF(DRM_MGA_FLUSH, mga_dma_flush, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_MGA_RESET, mga_dma_reset, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_MGA_SWAP, mga_dma_swap, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_MGA_CLEAR, mga_dma_clear, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_MGA_VERTEX, mga_dma_vertex, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_MGA_INDICES, mga_dma_indices, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_MGA_ILOAD, mga_dma_iload, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_MGA_BLIT, mga_dma_blit, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_MGA_GETPARAM, mga_getparam, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_MGA_SET_FENCE, mga_set_fence, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_MGA_WAIT_FENCE, mga_wait_fence, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_MGA_DMA_BOOTSTRAP, mga_dma_bootstrap, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),

};

static const struct drm_driver_info mga_driver = {
	.buf_priv_size		= sizeof(drm_mga_buf_priv_t),
	.load			= mga_driver_load,
	.unload			= mga_driver_unload,
	.lastclose		= mga_driver_lastclose,
	.enable_vblank		= mga_enable_vblank,
	.disable_vblank		= mga_disable_vblank,
	.get_vblank_counter	= mga_get_vblank_counter,
	.irq_preinstall		= mga_driver_irq_preinstall,
	.irq_postinstall	= mga_driver_irq_postinstall,
	.irq_uninstall		= mga_driver_irq_uninstall,
	.irq_handler		= mga_driver_irq_handler,
	.dma_ioctl		= mga_dma_buffers,
	.dma_quiescent		= mga_driver_dma_quiescent,
	.device_is_agp		= mga_driver_device_is_agp,

	.ioctls			= mga_ioctls,
	.max_ioctl		= DRM_ARRAY_SIZE(mga_ioctls),

	.name			= DRIVER_NAME,
	.desc			= DRIVER_DESC,
	.date			= DRIVER_DATE,
	.major			= DRIVER_MAJOR,
	.minor			= DRIVER_MINOR,
	.patchlevel		= DRIVER_PATCHLEVEL,

	.use_agp		= 1,
	.require_agp		= 1,
	.use_mtrr		= 1,
	.use_dma		= 1,
	.use_irq		= 1,
	.use_vbl_irq		= 1,
};

int	mgadrm_probe(struct device *, void *, void *);
void	mgadrm_attach(struct device *, struct device *, void *);
int
mgadrm_probe(struct device *parent, void *match, void *aux)
{
	return drm_probe((struct pci_attach_args *)aux, mga_pciidlist);
}

void
mgadrm_attach(struct device *parent, struct device *self, void *aux)
{
	struct pci_attach_args *pa = aux;
	struct drm_device *dev = (struct drm_device *)self;

	dev->driver = &mga_driver;
	return drm_attach(parent, self, pa, mga_pciidlist);
}

struct cfattach mgadrm_ca = {
	sizeof(struct drm_device), mgadrm_probe, mgadrm_attach,
	drm_detach, drm_activate
};

struct cfdriver mgadrm_cd = {
	0, "mgadrm", DV_DULL
};
