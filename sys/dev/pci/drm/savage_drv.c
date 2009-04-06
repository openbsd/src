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
int	savagedrm_detach(struct device *, int);
int	savagedrm_ioctl(struct drm_device *, u_long, caddr_t, struct drm_file *);

const static struct drm_pcidev savagedrm_pciidlist[] = {
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

static const struct drm_driver_info savagedrm_driver = {
	.buf_priv_size		= sizeof(struct savagedrm_buf_priv),
	.firstopen		= savage_driver_firstopen,
	.lastclose		= savage_driver_lastclose,
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
	return drm_pciprobe((struct pci_attach_args *)aux, savagedrm_pciidlist);
}

void
savagedrm_attach(struct device *parent, struct device *self, void *aux)
{
	drm_savage_private_t	*dev_priv = (drm_savage_private_t *)self;
	struct pci_attach_args	*pa = aux;
	struct vga_pci_bar	*bar;
	const struct drm_pcidev	*id_entry;
	unsigned long		 mmio_base;
	int			 is_agp;

	id_entry = drm_find_description(PCI_VENDOR(pa->pa_id),
	    PCI_PRODUCT(pa->pa_id), savagedrm_pciidlist);
	dev_priv->chipset = (enum savage_family)id_entry->driver_private;

	if (S3_SAVAGE3D_SERIES(dev_priv->chipset)) {
		bar = vga_pci_bar_info((struct vga_pci_softc *)parent, 0);	
		if (bar == NULL) {
			printf(": can't find fb info\n");
			return;
		}
		dev_priv->fb_base = bar->base;
		dev_priv->fb_size = SAVAGE_FB_SIZE_S3;
		mmio_base = dev_priv->fb_base + dev_priv->fb_size;
		dev_priv->aperture_base = dev_priv->fb_base +
		    SAVAGE_APERTURE_OFFSET;
		/* this should always be true */
		if (bar->maxsize != 0x08000000) {
			printf(": strange pci resource len $08lx\n",
			    bar->maxsize);
			return;
		}
	} else if (dev_priv->chipset != S3_SUPERSAVAGE &&
		   dev_priv->chipset != S3_SAVAGE2000) {
		bar = vga_pci_bar_info((struct vga_pci_softc *)parent, 0);	
		if (bar == NULL) {
			printf(": can't find mmio info\n");
			return;
		}
		mmio_base = bar->base;

		bar = vga_pci_bar_info((struct vga_pci_softc *)parent, 1);	
		if (bar == NULL) {
			printf(": can't find fb info\n");
			return;
		}
		dev_priv->fb_base = bar->base;
		dev_priv->fb_size = SAVAGE_FB_SIZE_S4;
		dev_priv->aperture_base = dev_priv->fb_base +
		    SAVAGE_APERTURE_OFFSET;
		/* this should always be true */
		if (bar->maxsize != 0x08000000) {
			printf(": strange pci resource len $08lx\n",
			    bar->maxsize);
			return;
		}
	} else {
		bar = vga_pci_bar_info((struct vga_pci_softc *)parent, 0);	
		if (bar == NULL) {
			printf(": can't find mmio info\n");
			return;
		}
		mmio_base = bar->base;
		bar = vga_pci_bar_info((struct vga_pci_softc *)parent, 1);	
		if (bar == NULL) {
			printf(": can't find fb info\n");
			return;
		}
		dev_priv->fb_base = bar->base;
		dev_priv->fb_size = bar->maxsize;
		bar = vga_pci_bar_info((struct vga_pci_softc *)parent, 2);	
		if (bar == NULL) {
			printf(": can't find aperture info\n");
			return;
		}
		dev_priv->aperture_base = bar->base;
	}

	if (bus_space_map(pa->pa_memt, mmio_base, SAVAGE_MMIO_SIZE,
	    BUS_SPACE_MAP_LINEAR, &dev_priv->bsh) != 0) {
		printf(": can't map mmio space\n");
		return;
	}
	dev_priv->bst = pa->pa_memt;

	is_agp = pci_get_capability(pa->pa_pc, pa->pa_tag, PCI_CAP_AGP,
	    NULL, NULL);
	printf("\n");

	dev_priv->drmdev = drm_attach_pci(&savagedrm_driver, pa, is_agp, self);
}

int
savagedrm_detach(struct device *self, int flags)
{
	drm_savage_private_t	*dev_priv = (drm_savage_private_t *)self;

	if (dev_priv->drmdev != NULL) {
		config_detach(dev_priv->drmdev, flags);
		dev_priv->drmdev = NULL;
	}

	bus_space_unmap(dev_priv->bst, dev_priv->bsh, SAVAGE_MMIO_SIZE);

	return (0);
}

struct cfattach savagedrm_ca = {
	sizeof(drm_savage_private_t), savagedrm_probe, savagedrm_attach,
	savagedrm_detach
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
