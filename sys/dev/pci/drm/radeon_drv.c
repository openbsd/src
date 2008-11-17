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

int	radeondrm_probe(struct device *, void *, void *);
void	radeondrm_attach(struct device *, struct device *, void *);
int	radeondrm_ioctl(struct drm_device *, u_long, caddr_t, struct drm_file *);

int radeon_no_wb;

/* drv_PCI_IDs comes from drm_pciids.h, generated from drm_pciids.txt. */
static drm_pci_id_list_t radeon_pciidlist[] = {
	radeon_PCI_IDS
};

static const struct drm_driver_info radeon_driver = {
	.buf_priv_size		= sizeof(drm_radeon_buf_priv_t),
	.load			= radeon_driver_load,
	.unload			= radeon_driver_unload,
	.firstopen		= radeon_driver_firstopen,
	.open			= radeon_driver_open,
	.ioctl			= radeondrm_ioctl,
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

int
radeondrm_ioctl(struct drm_device *dev, u_long cmd, caddr_t data,
    struct drm_file *file_priv)
{
	if (file_priv->authenticated == 1) {
		switch (cmd) {
		case DRM_IOCTL_RADEON_CP_IDLE:
			return (radeon_cp_idle(dev, data, file_priv));
		case DRM_IOCTL_RADEON_CP_RESUME:
			return (radeon_cp_resume(dev, data, file_priv));
		case DRM_IOCTL_RADEON_RESET:
			return (radeon_engine_reset(dev, data, file_priv));
		case DRM_IOCTL_RADEON_FULLSCREEN:
			return (radeon_fullscreen(dev, data, file_priv));
		case DRM_IOCTL_RADEON_SWAP:
			return (radeon_cp_swap(dev, data, file_priv));
		case DRM_IOCTL_RADEON_CLEAR:
			return (radeon_cp_clear(dev, data, file_priv));
		case DRM_IOCTL_RADEON_VERTEX:
			return (radeon_cp_vertex(dev, data, file_priv));
		case DRM_IOCTL_RADEON_INDICES:
			return (radeon_cp_indices(dev, data, file_priv));
		case DRM_IOCTL_RADEON_TEXTURE:
			return (radeon_cp_texture(dev, data, file_priv));
		case DRM_IOCTL_RADEON_STIPPLE:
			return (radeon_cp_stipple(dev, data, file_priv));
		case DRM_IOCTL_RADEON_VERTEX2:
			return (radeon_cp_vertex2(dev, data, file_priv));
		case DRM_IOCTL_RADEON_CMDBUF:
			return (radeon_cp_cmdbuf(dev, data, file_priv));
		case DRM_IOCTL_RADEON_GETPARAM:
			return (radeon_cp_getparam(dev, data, file_priv));
		case DRM_IOCTL_RADEON_FLIP:
			return (radeon_cp_flip(dev, data, file_priv));
		case DRM_IOCTL_RADEON_ALLOC:
			return (radeon_mem_alloc(dev, data, file_priv));
		case DRM_IOCTL_RADEON_FREE:
			return (radeon_mem_free(dev, data, file_priv));
		case DRM_IOCTL_RADEON_IRQ_EMIT:
			return (radeon_irq_emit(dev, data, file_priv));
		case DRM_IOCTL_RADEON_IRQ_WAIT:
			return (radeon_irq_wait(dev, data, file_priv));
		case DRM_IOCTL_RADEON_SETPARAM:
			return (radeon_cp_setparam(dev, data, file_priv));
		case DRM_IOCTL_RADEON_SURF_ALLOC:
			return (radeon_surface_alloc(dev, data, file_priv));
		case DRM_IOCTL_RADEON_SURF_FREE:
			return (radeon_surface_free(dev, data, file_priv));
		}
	}

	if (file_priv->master == 1) {
		switch (cmd) {
		case DRM_IOCTL_RADEON_CP_INIT:
			return (radeon_cp_init(dev, data, file_priv));
		case DRM_IOCTL_RADEON_CP_START:
			return (radeon_cp_start(dev, data, file_priv));
		case DRM_IOCTL_RADEON_CP_STOP:
			return (radeon_cp_stop(dev, data, file_priv));
		case DRM_IOCTL_RADEON_CP_RESET:
			return (radeon_cp_reset(dev, data, file_priv));
		case DRM_IOCTL_RADEON_INDIRECT:
			return (radeon_cp_indirect(dev, data, file_priv));
		case DRM_IOCTL_RADEON_INIT_HEAP:
			return (radeon_mem_init_heap(dev, data, file_priv));
		}
	}
	return (EINVAL);
}
