/* sis.c -- sis driver -*- linux-c -*-
 */
/*-
 * Copyright 2000 Silicon Integrated Systems Corp, Inc., HsinChu, Taiwan.
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
 * Authors:
 *    Sung-Ching Lin <sclin@sis.com.tw>
 */

#include "drmP.h"
#include "sis_drm.h"

#define DRIVER_AUTHOR		"SIS, Tungsten Graphics"
#define DRIVER_NAME		"sis"
#define DRIVER_DESC		"SIS 300/630/540 and XGI V3XE/V5/V8"
#define DRIVER_DATE		"20070626"
#define DRIVER_MAJOR		1
#define DRIVER_MINOR		3
#define DRIVER_PATCHLEVEL	0

struct drm_sis_private {
	struct device	 dev;
	struct device	*drmdev;

	struct drm_heap	 agp_heap;
	struct drm_heap	 fb_heap;
} drm_sis_private_t;

enum sis_family {
	SIS_OTHER = 0,
	SIS_CHIP_315 = 1,
};

int	sisdrm_probe(struct device *, void *, void *);
void	sisdrm_attach(struct device *, struct device *, void *);
int	sisdrm_detach(struct device *, int);
void	sisdrm_lastclose(struct drm_device *);
void	sisdrm_close(struct drm_device *, struct drm_file *);
int	sisdrm_ioctl(struct drm_device *, u_long, caddr_t, struct drm_file *);

int	sis_alloc(struct drm_heap *, drm_sis_mem_t *, struct drm_file *);
int	sis_free(struct drm_heap *, drm_sis_mem_t *, struct drm_file *);
int	sis_fb_init(struct drm_device *, void *, struct drm_file *);
int	sis_agp_init(struct drm_device *, void *, struct drm_file *);

const static struct drm_pcidev sis_pciidlist[] = {
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
	.close			= sisdrm_close,
	.lastclose		= sisdrm_lastclose,
	.ioctl			= sisdrm_ioctl,

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
	return (drm_pciprobe((struct pci_attach_args *)aux, sis_pciidlist));
}

void
sisdrm_attach(struct device *parent, struct device *self, void *aux)
{
	struct drm_sis_private	*dev_priv = (struct drm_sis_private *)self;
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
	struct drm_sis_private *dev_priv = (struct drm_sis_private *)self;

	if (dev_priv->drmdev != NULL) {
		config_detach(dev_priv->drmdev, flags);
		dev_priv->drmdev = NULL;
	}

	return (0);
}

struct cfattach sisdrm_ca = {
	sizeof(struct drm_sis_private), sisdrm_probe, sisdrm_attach,
	sisdrm_detach
};

struct cfdriver sisdrm_cd = {
	0, "sisdrm", DV_DULL
};

void
sisdrm_close(struct drm_device *dev, struct drm_file *file_priv)
{
	struct drm_sis_private	*dev_priv = dev->dev_private;

	drm_mem_release(&dev_priv->agp_heap, file_priv);
	drm_mem_release(&dev_priv->fb_heap, file_priv);
}

void
sisdrm_lastclose(struct drm_device *dev)
{
	struct drm_sis_private	*dev_priv = dev->dev_private;

	drm_mem_takedown(&dev_priv->agp_heap);
	drm_mem_takedown(&dev_priv->fb_heap);
}
int
sisdrm_ioctl(struct drm_device *dev, u_long cmd, caddr_t data,
    struct drm_file *file_priv)
{
	struct drm_sis_private *dev_priv = dev->dev_private;

	if (dev_priv == NULL)
		return (EINVAL);

	if (file_priv->authenticated == 1) {
		switch (cmd) {
		case DRM_IOCTL_SIS_FB_ALLOC:
			return (sis_alloc(&dev_priv->fb_heap,
			    (drm_sis_mem_t *)data, file_priv));
		case DRM_IOCTL_SIS_FB_FREE:
			return (sis_free(&dev_priv->fb_heap,
			    (drm_sis_mem_t *)data, file_priv));
		case DRM_IOCTL_SIS_AGP_ALLOC:
			return (sis_alloc(&dev_priv->agp_heap,
			    (drm_sis_mem_t *)data, file_priv));
		case DRM_IOCTL_SIS_AGP_FREE:
			return (sis_free(&dev_priv->agp_heap,
			    (drm_sis_mem_t *)data, file_priv));
		}
	}

	if (file_priv->master == 1) {
		switch (cmd) {
		case DRM_IOCTL_SIS_AGP_INIT:
			return (sis_agp_init(dev, data, file_priv));
		case DRM_IOCTL_SIS_FB_INIT:
			return (sis_fb_init(dev, data, file_priv));
		}
	}
	return (EINVAL);
}

/* fb management via fb device */
/* Called by the X Server to initialize the FB heap.  Allocations will fail
 * unless this is called.  Offset is the beginning of the heap from the
 * framebuffer offset (MaxXFBMem in XFree86).
 *
 * Memory layout according to Thomas Winischofer:
 * |------------------|DDDDDDDDDDDDDDDDDDDDDDDDDDDDD|HHHH|CCCCCCCCCCC|
 *
 *    X driver/sisfb                                  HW-   Command-
 *  framebuffer memory           DRI heap           Cursor   queue
 */
int
sis_fb_init(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_sis_private	*dev_priv = dev->dev_private;
	drm_sis_fb_t		*fb = data;

	DRM_DEBUG("offset = %u, size = %u", fb->offset, fb->size);

	return (drm_init_heap(&dev_priv->fb_heap, fb->offset, fb->size));
}

int
sis_agp_init(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_sis_private	*dev_priv = dev->dev_private;
	drm_sis_agp_t		*agp = data;

	DRM_DEBUG("offset = %u, size = %u", agp->offset, agp->size);

	return (drm_init_heap(&dev_priv->agp_heap, agp->offset, agp->size));
}

int
sis_alloc(struct drm_heap *heap, drm_sis_mem_t *mem,
    struct drm_file *file_priv)
{
	struct drm_mem		*block;

	/* Original code had no aligment restrictions. Should we page align? */
	if ((block = drm_alloc_block(heap, mem->size, 0, file_priv)) == NULL)
		return (ENOMEM);

	mem->offset = block->start;
	mem->free = block->start;
	DRM_DEBUG("alloc agp, size = %d, offset = %d\n", mem->size,
	    mem->offset);

	return (0);
}

int
sis_free(struct drm_heap *heap, drm_sis_mem_t *mem, struct drm_file *file_priv)
{
	struct drm_mem		*block; 

	DRM_DEBUG("free fb, free = 0x%lx\n", mem->free);

	if ((block = drm_find_block(heap, mem->free)) == NULL)
		return (EFAULT);
	if (block->file_priv != file_priv)
		return (EPERM);

	drm_free_block(heap, block);
	return (0);
}
