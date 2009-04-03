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

int	sisdrm_probe(struct device *, void *, void *);
void	sisdrm_attach(struct device *, struct device *, void *);
int	sisdrm_detach(struct device *, int);
int	sisdrm_ioctl(struct drm_device *, u_long, caddr_t, struct drm_file *);

const static drm_pci_id_list_t sis_pciidlist[] = {
	{PCI_VENDOR_SIS, PCI_PRODUCT_SIS_300},
	{PCI_VENDOR_SIS, PCI_PRODUCT_SIS_5300},
	{PCI_VENDOR_SIS, PCI_PRODUCT_SIS_6300},
	{PCI_VENDOR_SIS, PCI_PRODUCT_SIS_6330},
	{PCI_VENDOR_SIS, PCI_PRODUCT_SIS_7300},
	{PCI_VENDOR_XGI, 0x0042, SIS_CHIP_315},
	{PCI_VENDOR_XGI, PCI_PRODUCT_XGI_VOLARI_V3XT},
	{0, 0, 0}
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
	return drm_pciprobe((struct pci_attach_args *)aux, sis_pciidlist);
}

void
sisdrm_attach(struct device *parent, struct device *self, void *aux)
{
	drm_sis_private_t	*dev_priv = (drm_sis_private_t *)self;
	struct pci_attach_args	*pa = aux;
	int			 is_agp;

	is_agp = pci_get_capability(pa->pa_pc, pa->pa_tag, PCI_CAP_AGP,
	    NULL, NULL);
	printf("\n");

	dev_priv->drmdev = drm_attach_pci(&sis_driver, pa, is_agp, self);
}

int
sisdrm_detach(struct device *self, int flags)
{
	drm_sis_private_t *dev_priv = (drm_sis_private_t *)self;

	if (dev_priv->drmdev != NULL) {
		config_detach(dev_priv->drmdev, flags);
		dev_priv->drmdev = NULL;
	}

	return (0);
}

struct cfattach sisdrm_ca = {
	sizeof(drm_sis_private_t), sisdrm_probe, sisdrm_attach,
	sisdrm_detach
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
