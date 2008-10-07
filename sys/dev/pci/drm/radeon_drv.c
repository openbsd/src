/* radeon_drv.c -- ATI Radeon driver -*- linux-c -*-
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
#include "radeon_drm.h"
#include "radeon_drv.h"
#include "drm_pciids.h"

int radeon_no_wb;

/* drv_PCI_IDs comes from drm_pciids.h, generated from drm_pciids.txt. */
static drm_pci_id_list_t radeon_pciidlist[] = {
	radeon_PCI_IDS
};

struct drm_ioctl_desc radeon_ioctls[] = {
	DRM_IOCTL_DEF(DRM_RADEON_CP_INIT, radeon_cp_init, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF(DRM_RADEON_CP_START, radeon_cp_start, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF(DRM_RADEON_CP_STOP, radeon_cp_stop, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF(DRM_RADEON_CP_RESET, radeon_cp_reset, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF(DRM_RADEON_CP_IDLE, radeon_cp_idle, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_RADEON_CP_RESUME, radeon_cp_resume, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_RADEON_RESET, radeon_engine_reset, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_RADEON_FULLSCREEN, radeon_fullscreen, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_RADEON_SWAP, radeon_cp_swap, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_RADEON_CLEAR, radeon_cp_clear, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_RADEON_VERTEX, radeon_cp_vertex, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_RADEON_INDICES, radeon_cp_indices, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_RADEON_TEXTURE, radeon_cp_texture, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_RADEON_STIPPLE, radeon_cp_stipple, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_RADEON_INDIRECT, radeon_cp_indirect, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF(DRM_RADEON_VERTEX2, radeon_cp_vertex2, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_RADEON_CMDBUF, radeon_cp_cmdbuf, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_RADEON_GETPARAM, radeon_cp_getparam, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_RADEON_FLIP, radeon_cp_flip, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_RADEON_ALLOC, radeon_mem_alloc, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_RADEON_FREE, radeon_mem_free, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_RADEON_INIT_HEAP, radeon_mem_init_heap, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF(DRM_RADEON_IRQ_EMIT, radeon_irq_emit, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_RADEON_IRQ_WAIT, radeon_irq_wait, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_RADEON_SETPARAM, radeon_cp_setparam, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_RADEON_SURF_ALLOC, radeon_surface_alloc, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_RADEON_SURF_FREE, radeon_surface_free, DRM_AUTH)
};

static const struct drm_driver_info radeon_driver = {
	.buf_priv_size		= sizeof(drm_radeon_buf_priv_t),
	.load			= radeon_driver_load,
	.unload			= radeon_driver_unload,
	.firstopen		= radeon_driver_firstopen,
	.open			= radeon_driver_open,
	.preclose		= radeon_driver_preclose,
	.postclose		= radeon_driver_postclose,
	.lastclose		= radeon_driver_lastclose,
	.get_vblank_counter	= radeon_get_vblank_counter,
	.enable_vblank		= radeon_enable_vblank,
	.disable_vblank		= radeon_disable_vblank,
	.irq_preinstall		= radeon_driver_irq_preinstall,
	.irq_postinstall	= radeon_driver_irq_postinstall,
	.irq_uninstall		= radeon_driver_irq_uninstall,
	.irq_handler		= radeon_driver_irq_handler,
	.dma_ioctl		= radeon_cp_buffers,

	.ioctls			= radeon_ioctls,
	.max_ioctl		= DRM_ARRAY_SIZE(radeon_ioctls),

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

int	radeondrm_probe(struct device *, void *, void *);
void	radeondrm_attach(struct device *, struct device *, void *);

int
radeondrm_probe(struct device *parent, void *match, void *aux)
{
	return drm_probe((struct pci_attach_args *)aux, radeon_pciidlist);
}

void
radeondrm_attach(struct device *parent, struct device *self, void *aux)
{
	struct pci_attach_args *pa = aux;
	struct drm_device *dev = (struct drm_device *)self;

	dev->driver = &radeon_driver;
	return drm_attach(parent, self, pa, radeon_pciidlist);
}

struct cfattach radeondrm_ca = {
        sizeof (struct drm_device), radeondrm_probe, radeondrm_attach, 
	drm_detach, drm_activate
}; 

struct cfdriver radeondrm_cd = {
	NULL, "radeondrm", DV_DULL
}; 
