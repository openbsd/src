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

int	sisdrm_probe(struct device *, void *, void *);
void	sisdrm_attach(struct device *, struct device *, void *);
int	sisdrm_ioctl(struct drm_device *, u_long, caddr_t, struct drm_file *);

/* drv_PCI_IDs comes from drm_pciids.h, generated from drm_pciids.txt. */
static drm_pci_id_list_t sis_pciidlist[] = {
	sis_PCI_IDS
};

static const struct drm_driver_info sis_driver = {
	.buf_priv_size		= 1, /* No dev_priv */
	.ioctl			= sisdrm_ioctl,
	.context_ctor		= sis_init_context,
	.context_dtor		= sis_final_context,

	.name			= DRIVER_NAME,
	.desc			= DRIVER_DESC,
	.date			= DRIVER_DATE,
	.major			= DRIVER_MAJOR,
	.minor			= DRIVER_MINOR,
	.patchlevel		= DRIVER_PATCHLEVEL,

	.flags			= DRIVER_AGP | DRIVER_MTRR,
};

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

int
sisdrm_ioctl(struct drm_device *dev, u_long cmd, caddr_t data,
    struct drm_file *file_priv)
{
	if (file_priv->authenticated == 1) {
		switch (cmd) {
		case DRM_IOCTL_SIS_FB_ALLOC:
			return (sis_fb_alloc(dev, data, file_priv));
		case DRM_IOCTL_SIS_FB_FREE:
			return (sis_fb_free(dev, data, file_priv));
		case DRM_IOCTL_SIS_AGP_ALLOC:
			return (sis_ioctl_agp_alloc(dev, data, file_priv));
		case DRM_IOCTL_SIS_AGP_FREE:
			return (sis_ioctl_agp_free(dev, data, file_priv));
		}
	}

	if (file_priv->master == 1) {
		switch (cmd) {
		case DRM_IOCTL_SIS_AGP_INIT:
			return (sis_ioctl_agp_init(dev, data, file_priv));
		case DRM_IOCTL_SIS_FB_INIT:
			return (sis_fb_init(dev, data, file_priv));
		}
	}
	return (EINVAL);
}
