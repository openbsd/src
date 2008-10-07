/* sis.c -- sis driver -*- linux-c -*-
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
 * PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 */

#include "drmP.h"
#include "sis_drm.h"
#include "sis_drv.h"
#include "drm_pciids.h"

/* drv_PCI_IDs comes from drm_pciids.h, generated from drm_pciids.txt. */
static drm_pci_id_list_t sis_pciidlist[] = {
	sis_PCI_IDS
};

drm_ioctl_desc_t sis_ioctls[] = {
	DRM_IOCTL_DEF(DRM_SIS_FB_ALLOC, sis_fb_alloc, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_SIS_FB_FREE, sis_fb_free, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_SIS_AGP_INIT, sis_ioctl_agp_init, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF(DRM_SIS_AGP_ALLOC, sis_ioctl_agp_alloc, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_SIS_AGP_FREE, sis_ioctl_agp_free, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_SIS_FB_INIT, sis_fb_init, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY)
};

int sis_max_ioctl = DRM_ARRAY_SIZE(sis_ioctls);

static const struct drm_driver_info sis_driver = {
	.buf_priv_size		= 1, /* No dev_priv */
	.context_ctor		= sis_init_context,
	.context_dtor		= sis_final_context,

	.ioctls			= sis_ioctls,
	.max_ioctl		= DRM_ARRAY_SIZE(sis_ioctls),

	.name			= DRIVER_NAME,
	.desc			= DRIVER_DESC,
	.date			= DRIVER_DATE,
	.major			= DRIVER_MAJOR,
	.minor			= DRIVER_MINOR,
	.patchlevel		= DRIVER_PATCHLEVEL,

	.use_agp		= 1,
	.use_mtrr		= 1,
};

int	sisdrm_probe(struct device *, void *, void *);
void	sisdrm_attach(struct device *, struct device *, void *);

int
sisdrm_probe(struct device *parent, void *match, void *aux)
{
	return drm_probe((struct pci_attach_args *)aux, sis_pciidlist);
}

void
sisdrm_attach(struct device *parent, struct device *self, void *aux)
{
	struct pci_attach_args *pa = aux;
	struct drm_device *dev = (struct drm_device *)self;

	dev->driver = &sis_driver;
	return drm_attach(parent, self, pa, sis_pciidlist);
}

struct cfattach sisdrm_ca = {
	sizeof(struct drm_device), sisdrm_probe, sisdrm_attach,
	drm_detach, drm_activate
};

struct cfdriver sisdrm_cd = {
	0, "sisdrm", DV_DULL
};
