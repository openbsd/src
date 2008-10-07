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
#include "drm_pciids.h"

/* drv_PCI_IDs comes from drm_pciids.h, generated from drm_pciids.txt. */
static drm_pci_id_list_t tdfx_pciidlist[] = {
	tdfx_PCI_IDS
};

static const struct drm_driver_info tdfx_driver = {
	.buf_priv_size	= 1, /* No dev_priv */

	.max_ioctl	= 0,

	.name		= DRIVER_NAME,
	.desc		= DRIVER_DESC,
	.date		= DRIVER_DATE,
	.major		= DRIVER_MAJOR,
	.minor		= DRIVER_MINOR,
	.patchlevel	= DRIVER_PATCHLEVEL,

	.use_mtrr	= 1,
};

int	tdfxdrm_probe(struct device *, void *, void *);
void	tdfxdrm_attach(struct device *, struct device *, void *);

int
tdfxdrm_probe(struct device *parent, void *match, void *aux)
{
	return drm_probe((struct pci_attach_args *)aux, tdfx_pciidlist);
}

void
tdfxdrm_attach(struct device *parent, struct device *self, void *aux)
{
	struct pci_attach_args *pa = aux;
	struct drm_device *dev = (struct drm_device *)self;

	dev->driver = &tdfx_driver;
	return drm_attach(parent, self, pa, tdfx_pciidlist);
}

struct cfattach tdfxdrm_ca = {
	sizeof(struct drm_device), tdfxdrm_probe, tdfxdrm_attach,
	drm_detach, drm_activate
};

struct cfdriver tdfxdrm_cd = {
	0, "tdfxdrm",  DV_DULL
};
