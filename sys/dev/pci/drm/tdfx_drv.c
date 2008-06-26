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

void	tdfx_configure(struct drm_device *);

/* drv_PCI_IDs comes from drm_pciids.h, generated from drm_pciids.txt. */
static drm_pci_id_list_t tdfx_pciidlist[] = {
	tdfx_PCI_IDS
};

void
tdfx_configure(struct drm_device *dev)
{
	dev->driver.buf_priv_size	= 1; /* No dev_priv */

	dev->driver.max_ioctl		= 0;

	dev->driver.name		= DRIVER_NAME;
	dev->driver.desc		= DRIVER_DESC;
	dev->driver.date		= DRIVER_DATE;
	dev->driver.major		= DRIVER_MAJOR;
	dev->driver.minor		= DRIVER_MINOR;
	dev->driver.patchlevel		= DRIVER_PATCHLEVEL;

	dev->driver.use_mtrr		= 1;
}

#ifdef __FreeBSD__
static int
tdfx_probe(device_t dev)
{
	return drm_probe(dev, tdfx_pciidlist);
}

static int
tdfx_attach(device_t nbdev)
{
	struct drm_device *dev = device_get_softc(nbdev);

	bzero(dev, sizeof(struct drm_device));
	tdfx_configure(dev);
	return drm_attach(nbdev, tdfx_pciidlist);
}

static device_method_t tdfx_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		tdfx_probe),
	DEVMETHOD(device_attach,	tdfx_attach),
	DEVMETHOD(device_detach,	drm_detach),

	{ 0, 0 }
};

static driver_t tdfx_driver = {
	"drm",
	tdfx_methods,
	sizeof(struct drm_device)
};

extern devclass_t drm_devclass;
#if __FreeBSD_version >= 700010
DRIVER_MODULE(tdfx, vgapci, tdfx_driver, drm_devclass, 0, 0);
#else
DRIVER_MODULE(tdfx, pci, tdfx_driver, drm_devclass, 0, 0);
#endif
MODULE_DEPEND(tdfx, drm, 1, 1, 1);

#elif defined(__NetBSD__) || defined(__OpenBSD__)

int	tdfxdrm_probe(struct device *, void *, void *);
void	tdfxdrm_attach(struct device *, struct device *, void *);

int
#if defined(__OpenBSD__)
tdfxdrm_probe(struct device *parent, void *match, void *aux)
#else
tdfxdrm_probe(struct device *parent, struct cfdata *match, void *aux)
#endif
{
	return drm_probe((struct pci_attach_args *)aux, tdfx_pciidlist);
}

void
tdfxdrm_attach(struct device *parent, struct device *self, void *aux)
{
	struct pci_attach_args *pa = aux;
	struct drm_device *dev = (struct drm_device *)self;

	tdfx_configure(dev);
	return drm_attach(parent, self, pa, tdfx_pciidlist);
}

#if defined(__OpenBSD__)
struct cfattach tdfxdrm_ca = {
	sizeof(struct drm_device), tdfxdrm_probe, tdfxdrm_attach,
	drm_detach, drm_activate
};

struct cfdriver tdfxdrm_cd = {
	0, "tdfxdrm",  DV_DULL
};
#else
#ifdef _LKM
CFDRIVER_DECL(tdfxdrm, DV_TTY, NULL);
#else
CFATTACH_DECL(tdfxdrm, sizeof(struct drm_device), tdfxdrm_probe, tdfxdrm_attach,
    drm_detach, drm_activate);
#endif
#endif

#endif
