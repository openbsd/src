/* savage_drv.c -- Savage DRI driver
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
 */

#include "drmP.h"
#include "drm.h"
#include "savage_drm.h"
#include "savage_drv.h"

int	savagedrm_probe(struct device *, void *, void *);
void	savagedrm_attach(struct device *, struct device *, void *);
int	savagedrm_ioctl(struct drm_device *, u_long, caddr_t, struct drm_file *);

static drm_pci_id_list_t savage_pciidlist[] = {
	{PCI_VENDOR_S3, PCI_PRODUCT_S3_SAVAGE3D, S3_SAVAGE3D},
	{PCI_VENDOR_S3, PCI_PRODUCT_S3_SAVAGE3D_M, S3_SAVAGE3D},
	{PCI_VENDOR_S3, PCI_PRODUCT_S3_SAVAGE4, S3_SAVAGE4},
	{PCI_VENDOR_S3, PCI_PRODUCT_S3_SAVAGE4_2, S3_SAVAGE4},
	{PCI_VENDOR_S3, PCI_PRODUCT_S3_SAVAGE_MXMV, S3_SAVAGE_MX},
	{PCI_VENDOR_S3, PCI_PRODUCT_S3_SAVAGE_MX, S3_SAVAGE_MX},
	{PCI_VENDOR_S3, PCI_PRODUCT_S3_SAVAGE_IXMV, S3_SAVAGE_MX},
	{PCI_VENDOR_S3, PCI_PRODUCT_S3_SAVAGE_IX, S3_SAVAGE_MX},
	{PCI_VENDOR_S3, PCI_PRODUCT_S3_SUPERSAVAGE_MX128, S3_SUPERSAVAGE},
	{PCI_VENDOR_S3, PCI_PRODUCT_S3_SUPERSAVAGE_MX64, S3_SUPERSAVAGE},
	{PCI_VENDOR_S3, PCI_PRODUCT_S3_SUPERSAVAGE_MX64C, S3_SUPERSAVAGE},
	{PCI_VENDOR_S3, PCI_PRODUCT_S3_SUPERSAVAGE_IX128SDR, S3_SUPERSAVAGE},
	{PCI_VENDOR_S3, PCI_PRODUCT_S3_SUPERSAVAGE_IX128DDR, S3_SUPERSAVAGE},
	{PCI_VENDOR_S3, PCI_PRODUCT_S3_SUPERSAVAGE_IX64SDR, S3_SUPERSAVAGE},
	{PCI_VENDOR_S3, PCI_PRODUCT_S3_SUPERSAVAGE_IX64DDR, S3_SUPERSAVAGE},
	{PCI_VENDOR_S3, PCI_PRODUCT_S3_SUPERSAVAGE_IXCSDR, S3_SUPERSAVAGE},
	{PCI_VENDOR_S3, PCI_PRODUCT_S3_SUPERSAVAGE_IXCDDR, S3_SUPERSAVAGE},
	{PCI_VENDOR_S3, PCI_PRODUCT_S3_PROSAVAGE_PM133, S3_PROSAVAGE},
	{PCI_VENDOR_S3, PCI_PRODUCT_S3_PROSAVAGE_KM133, S3_PROSAVAGE},
	{PCI_VENDOR_S3, PCI_PRODUCT_S3_TWISTER, S3_TWISTER},
	{PCI_VENDOR_S3, PCI_PRODUCT_S3_TWISTER_K, S3_TWISTER},
	{PCI_VENDOR_S3, PCI_PRODUCT_S3_PROSAVAGE_DDR, S3_PROSAVAGEDDR},
	{PCI_VENDOR_S3, PCI_PRODUCT_S3_PROSAVAGE_DDR_K, S3_PROSAVAGEDDR},
	{0, 0, 0}
};

static const struct drm_driver_info savage_driver = {
	.buf_priv_size		= sizeof(drm_savage_buf_priv_t),
	.load			= savage_driver_load,
	.firstopen		= savage_driver_firstopen,
	.lastclose		= savage_driver_lastclose,
	.unload			= savage_driver_unload,
	.reclaim_buffers_locked = savage_reclaim_buffers,
	.dma_ioctl		= savage_bci_buffers,

	.name			= DRIVER_NAME,
	.desc			= DRIVER_DESC,
	.date			= DRIVER_DATE,
	.major			= DRIVER_MAJOR,
	.minor			= DRIVER_MINOR,
	.patchlevel		= DRIVER_PATCHLEVEL,

	.flags			= DRIVER_AGP | DRIVER_MTRR | DRIVER_PCI_DMA |
				    DRIVER_DMA,
};

int
savagedrm_probe(struct device *parent, void *match, void *aux)
{
	return drm_probe((struct pci_attach_args *)aux, savage_pciidlist);
}

void
savagedrm_attach(struct device *parent, struct device *self, void *aux)
{
	struct pci_attach_args *pa = aux;
	struct drm_device *dev = (struct drm_device *)self;

	dev->driver = &savage_driver;
	return drm_attach(parent, self, pa, savage_pciidlist);
}

struct cfattach savagedrm_ca = {
	sizeof(struct drm_device), savagedrm_probe, savagedrm_attach,
	drm_detach, drm_activate
};

struct cfdriver savagedrm_cd = {
	0, "savagedrm", DV_DULL
};

int
savagedrm_ioctl(struct drm_device *dev, u_long cmd, caddr_t data,
    struct drm_file *file_priv)
{
	if (file_priv->authenticated == 1) {
		switch (cmd) {
		case DRM_IOCTL_SAVAGE_CMDBUF:
			return (savage_bci_cmdbuf(dev, data, file_priv));
		case DRM_IOCTL_SAVAGE_EVENT_EMIT:
			return (savage_bci_event_emit(dev, data, file_priv));
		case DRM_IOCTL_SAVAGE_EVENT_WAIT:
			return (savage_bci_event_wait(dev, data, file_priv));
		}
	}

	if (file_priv->master == 1) {
		switch (cmd) {
		case DRM_IOCTL_SAVAGE_INIT:
			return (savage_bci_init(dev, data, file_priv));
		}
	}
	return (EINVAL);
}
