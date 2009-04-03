/* tdfx_drv.c -- tdfx driver -*- linux-c -*-
 * Created: Thu Oct  7 10:38:32 1999 by faith@precisioninsight.com
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
 * Authors:
 *    Rickard E. (Rik) Faith <faith@valinux.com>
 *    Daryll Strauss <daryll@valinux.com>
 *    Gareth Hughes <gareth@valinux.com>
 *
 */

#include "tdfx_drv.h"
#include "drmP.h"

struct tdfxdrm_softc {
	struct device	 dev;
	struct device	*drmdev;
};

int	tdfxdrm_probe(struct device *, void *, void *);
void	tdfxdrm_attach(struct device *, struct device *, void *);

const static drm_pci_id_list_t tdfxdrm_pciidlist[] = {
	{PCI_VENDOR_3DFX, PCI_PRODUCT_3DFX_BANSHEE},
	{PCI_VENDOR_3DFX, PCI_PRODUCT_3DFX_VOODOO32000},
	{PCI_VENDOR_3DFX, PCI_PRODUCT_3DFX_VOODOO3},
	{PCI_VENDOR_3DFX, PCI_PRODUCT_3DFX_VOODOO4},
	{PCI_VENDOR_3DFX, PCI_PRODUCT_3DFX_VOODOO5},
	{PCI_VENDOR_3DFX, PCI_PRODUCT_3DFX_VOODOO44200},
        {0, 0, 0}
};

static const struct drm_driver_info tdfxdrm_driver = {
	.buf_priv_size	= 1, /* No dev_priv */

	.name		= DRIVER_NAME,
	.desc		= DRIVER_DESC,
	.date		= DRIVER_DATE,
	.major		= DRIVER_MAJOR,
	.minor		= DRIVER_MINOR,
	.patchlevel	= DRIVER_PATCHLEVEL,

	.flags		= DRIVER_MTRR,
};

int
tdfxdrm_probe(struct device *parent, void *match, void *aux)
{
	return drm_pciprobe((struct pci_attach_args *)aux, tdfxdrm_pciidlist);
}

void
tdfxdrm_attach(struct device *parent, struct device *self, void *aux)
{
	struct tdfxdrm_softc	*dev_priv = (struct tdfxdrm_softc *)self;
	struct pci_attach_args	*pa = aux;

	printf("\n");

	/* never agp */
	dev_priv->drmdev = drm_attach_pci(&tdfxdrm_driver, pa, 0, self);
}

struct cfattach tdfxdrm_ca = {
	sizeof(struct tdfxdrm_softc), tdfxdrm_probe, tdfxdrm_attach,
};

struct cfdriver tdfxdrm_cd = {
	0, "tdfxdrm",  DV_DULL
};
