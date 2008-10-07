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
#include "drm_pciids.h"

/* drv_PCI_IDs comes from drm_pciids.h, generated from drm_pciids.txt. */
static drm_pci_id_list_t r128_pciidlist[] = {
	r128_PCI_IDS
};

struct drm_ioctl_desc r128_ioctls[] = {
	DRM_IOCTL_DEF(DRM_R128_INIT, r128_cce_init, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF(DRM_R128_CCE_START, r128_cce_start, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF(DRM_R128_CCE_STOP, r128_cce_stop, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF(DRM_R128_CCE_RESET, r128_cce_reset, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF(DRM_R128_CCE_IDLE, r128_cce_idle, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_R128_RESET, r128_engine_reset, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_R128_FULLSCREEN, r128_fullscreen, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_R128_SWAP, r128_cce_swap, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_R128_FLIP, r128_cce_flip, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_R128_CLEAR, r128_cce_clear, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_R128_VERTEX, r128_cce_vertex, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_R128_INDICES, r128_cce_indices, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_R128_BLIT, r128_cce_blit, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_R128_DEPTH, r128_cce_depth, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_R128_STIPPLE, r128_cce_stipple, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_R128_INDIRECT, r128_cce_indirect, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF(DRM_R128_GETPARAM, r128_getparam, DRM_AUTH),
};

static const struct drm_driver_info r128_driver = {
	.buf_priv_size		= sizeof(drm_r128_buf_priv_t),
	.preclose		= r128_driver_preclose,
	.lastclose		= r128_driver_lastclose,
	.get_vblank_counter	= r128_get_vblank_counter,
	.enable_vblank 		= r128_enable_vblank,
	.disable_vblank		= r128_disable_vblank,
	.irq_preinstall		= r128_driver_irq_preinstall,
	.irq_postinstall	= r128_driver_irq_postinstall,
	.irq_uninstall		= r128_driver_irq_uninstall,
	.irq_handler		= r128_driver_irq_handler,
	.dma_ioctl		= r128_cce_buffers,

	.ioctls			= r128_ioctls,
	.max_ioctl		= DRM_ARRAY_SIZE(r128_ioctls),

	.name			= DRIVER_NAME,
	.desc			= DRIVER_DESC,
	.date			= DRIVER_DATE,
	.major			= DRIVER_MAJOR,
	.minor			= DRIVER_MINOR,
	.patchlevel		= DRIVER_PATCHLEVEL,

	.use_agp		= 1,
	.use_mtrr		= 1,
	.use_pci_dma		= 1,
	.use_sg			= 1,
	.use_dma		= 1,
	.use_irq		= 1,
	.use_vbl_irq		= 1,
};

int	r128drm_probe(struct device *, void *, void *);
void	r128drm_attach(struct device *, struct device *, void *);

int
r128drm_probe(struct device *parent, void *match, void *aux)
{
	return drm_probe((struct pci_attach_args *)aux, r128_pciidlist);
}

void
r128drm_attach(struct device *parent, struct device *self, void *aux)
{
	struct pci_attach_args *pa = aux;
	struct drm_device *dev = (struct drm_device *)self;

	dev->driver = &r128_driver;
	return drm_attach(parent, self, pa, r128_pciidlist);
}

struct cfattach ragedrm_ca = {
	sizeof(struct drm_device), r128drm_probe, r128drm_attach,
	drm_detach, drm_activate
};

struct cfdriver ragedrm_cd = {
	0, "ragedrm", DV_DULL
};
