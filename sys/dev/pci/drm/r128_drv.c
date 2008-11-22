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

int	r128drm_probe(struct device *, void *, void *);
void	r128drm_attach(struct device *, struct device *, void *);
int	ragedrm_ioctl(struct drm_device *, u_long, caddr_t, struct drm_file *);

static drm_pci_id_list_t r128_pciidlist[] = {
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_LE, 0, "ATI Rage 128 Mobility LE (PCI)"},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_MOBILITY_M3, 0, "ATI Rage 128 Mobility LF (AGP)"},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_MF, 0, "ATI Rage 128 Mobility MF (AGP)"},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_ML, 0, "ATI Rage 128 Mobility ML (AGP)"},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_PA, 0, "ATI Rage 128 Pro PA (PCI)"},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_PB, 0, "ATI Rage 128 Pro PB (AGP)"},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_PC, 0, "ATI Rage 128 Pro PC (AGP)"},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_PD, 0, "ATI Rage 128 Pro PD (PCI)"},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_PE, 0, "ATI Rage 128 Pro PE (AGP)"},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE_FURY, 0, "ATI Rage 128 Pro PF (AGP)"},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_PG, 0, "ATI Rage 128 Pro PG (PCI)"},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_PH, 0, "ATI Rage 128 Pro PH (AGP)"},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_PI, 0, "ATI Rage 128 Pro PI (AGP)"},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_PJ, 0, "ATI Rage 128 Pro PJ (PCI)"},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_PK, 0, "ATI Rage 128 Pro PK (AGP)"},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_PL, 0, "ATI Rage 128 Pro PL (AGP)"},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_PM, 0, "ATI Rage 128 Pro PM (PCI)"},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_PN, 0, "ATI Rage 128 Pro PN (AGP)"},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_PO, 0, "ATI Rage 128 Pro PO (AGP)"},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_PP, 0, "ATI Rage 128 Pro PP (PCI)"},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_PQ, 0, "ATI Rage 128 Pro PQ (AGP)"},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_PR, 0, "ATI Rage 128 Pro PR (PCI)"},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_PS, 0, "ATI Rage 128 Pro PS (PCI)"},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_PT, 0, "ATI Rage 128 Pro PT (AGP)"},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_PU, 0, "ATI Rage 128 Pro PU (AGP)"},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_PV, 0, "ATI Rage 128 Pro PV (PCI)"},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_PW, 0, "ATI Rage 128 Pro PW (AGP)"},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_PX, 0, "ATI Rage 128 Pro PX (AGP)"},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_GL, 0, "ATI Rage 128 RE (PCI)"},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE_MAGNUM, 0, "ATI Rage 128 RF (AGP)"},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_RG, 0, "ATI Rage 128 RG (AGP)"},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_RK, 0, "ATI Rage 128 RK (PCI)"},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_VR, 0, "ATI Rage 128 RL (AGP)"},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_SM, 0, "ATI Rage 128 SM (AGP)"},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_TF, 0, "ATI Rage 128 Pro Ultra TF (AGP)"},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_TL, 0, "ATI Rage 128 Pro Ultra TL (AGP)"},
	{PCI_VENDOR_ATI, PCI_PRODUCT_ATI_RAGE128_TR, 0, "ATI Rage 128 Pro Ultra TR (AGP)"},
	{0, 0, 0, NULL}
};

static const struct drm_driver_info r128_driver = {
	.buf_priv_size		= sizeof(drm_r128_buf_priv_t),
	.ioctl			= ragedrm_ioctl,
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

int
ragedrm_ioctl(struct drm_device *dev, u_long cmd, caddr_t data,
    struct drm_file *file_priv)
{
	if (file_priv->authenticated == 1) {
		switch (cmd) {
		case DRM_IOCTL_R128_CCE_IDLE:
			return (r128_cce_idle(dev, data, file_priv));
		case DRM_IOCTL_R128_RESET:
			return (r128_engine_reset(dev, data, file_priv));
		case DRM_IOCTL_R128_FULLSCREEN:
			return (r128_fullscreen(dev, data, file_priv));
		case DRM_IOCTL_R128_SWAP:
			return (r128_cce_swap(dev, data, file_priv));
		case DRM_IOCTL_R128_FLIP:
			return (r128_cce_flip(dev, data, file_priv));
		case DRM_IOCTL_R128_CLEAR:
			return (r128_cce_clear(dev, data, file_priv));
		case DRM_IOCTL_R128_VERTEX:
			return (r128_cce_vertex(dev, data, file_priv));
		case DRM_IOCTL_R128_INDICES:
			return (r128_cce_indices(dev, data, file_priv));
		case DRM_IOCTL_R128_BLIT:
			return (r128_cce_blit(dev, data, file_priv));
		case DRM_IOCTL_R128_DEPTH:
			return (r128_cce_depth(dev, data, file_priv));
		case DRM_IOCTL_R128_STIPPLE:
			return (r128_cce_stipple(dev, data, file_priv));
		case DRM_IOCTL_R128_GETPARAM:
			return (r128_getparam(dev, data, file_priv));
		}
	}

	if (file_priv->master == 1) {
		switch (cmd) {
		case DRM_IOCTL_R128_INIT:
			return (r128_cce_init(dev, data, file_priv));
		case DRM_IOCTL_R128_CCE_START:
			return (r128_cce_start(dev, data, file_priv));
		case DRM_IOCTL_R128_CCE_STOP:
			return (r128_cce_stop(dev, data, file_priv));
		case DRM_IOCTL_R128_CCE_RESET:
			return (r128_cce_reset(dev, data, file_priv));
		case DRM_IOCTL_R128_INDIRECT:
			return (r128_cce_indirect(dev, data, file_priv));
		}
	}
	return (EINVAL);
}
