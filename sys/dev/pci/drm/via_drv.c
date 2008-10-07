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

/* drv_PCI_IDs comes from drm_pciids.h, generated from drm_pciids.txt. */
static drm_pci_id_list_t via_pciidlist[] = {
	viadrv_PCI_IDS
};

struct drm_ioctl_desc via_ioctls[] = {
	DRM_IOCTL_DEF(DRM_VIA_ALLOCMEM, via_mem_alloc, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_VIA_FREEMEM, via_mem_free, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_VIA_AGP_INIT, via_agp_init, DRM_AUTH|DRM_MASTER),
	DRM_IOCTL_DEF(DRM_VIA_FB_INIT, via_fb_init, DRM_AUTH|DRM_MASTER),
	DRM_IOCTL_DEF(DRM_VIA_MAP_INIT, via_map_init, DRM_AUTH|DRM_MASTER),
	DRM_IOCTL_DEF(DRM_VIA_DEC_FUTEX, via_decoder_futex, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_VIA_DMA_INIT, via_dma_init, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_VIA_CMDBUFFER, via_cmdbuffer, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_VIA_FLUSH, via_flush_ioctl, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_VIA_PCICMD, via_pci_cmdbuffer, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_VIA_CMDBUF_SIZE, via_cmdbuf_size, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_VIA_WAIT_IRQ, via_wait_irq, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_VIA_DMA_BLIT, via_dma_blit, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_VIA_BLIT_SYNC, via_dma_blit_sync, DRM_AUTH)
};

static const struct drm_driver_info via_driver = {
	.buf_priv_size		= 1,
	.load			= via_driver_load,
	.unload			= via_driver_unload,
	.context_ctor		= via_init_context,
	.context_dtor		= via_final_context,
	.get_vblank_counter	= via_get_vblank_counter,
	.enable_vblank		= via_enable_vblank,
	.disable_vblank		= via_disable_vblank,
	.irq_preinstall		= via_driver_irq_preinstall,
	.irq_postinstall	= via_driver_irq_postinstall,
	.irq_uninstall		= via_driver_irq_uninstall,
	.irq_handler		= via_driver_irq_handler,
	.dma_quiescent		= via_driver_dma_quiescent,

	.ioctls			= via_ioctls,
	.max_ioctl		= DRM_ARRAY_SIZE(via_ioctls),

	.name			= DRIVER_NAME,
	.desc			= DRIVER_DESC,
	.date			= DRIVER_DATE,
	.major			= DRIVER_MAJOR,
	.minor			= DRIVER_MINOR,
	.patchlevel		= DRIVER_PATCHLEVEL,

	.use_agp		= 1,
	.use_mtrr		= 1,
	.use_irq		= 1,
	.use_vbl_irq		= 1,
};

int	viadrm_probe(struct device *, void *, void *);
void	viadrm_attach(struct device *, struct device *, void *);

int
viadrm_probe(struct device *parent, void *match, void *aux)
{
	return drm_probe((struct pci_attach_args *)aux, via_pciidlist);
}

void
viadrm_attach(struct device *parent, struct device *self, void *opaque)
{
	struct pci_attach_args *pa = opaque;
	struct drm_device *dev = (struct drm_device *)self;

	dev->driver = &via_driver;
	drm_attach(parent, self, pa, via_pciidlist);
}

struct cfattach viadrm_ca = {
	sizeof(struct drm_device), viadrm_probe, viadrm_attach,
	drm_detach, drm_activate
};

struct cfdriver viadrm_cd = {
	0, "viadrm", DV_DULL
};
