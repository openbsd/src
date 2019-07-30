/*
 * Copyright (c) 2008-2009 Owain G. Ainsworth <oga@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "i915_drv.h"

/* MCH IFP BARs */
#define I915_IFPADDR	0x60
#define I965_IFPADDR	0x70

extern struct cfdriver inteldrm_cd;

/*
 * We're intel IGD, bus 0 function 0 dev 0 should be the GMCH, so it should
 * be Intel
 */
int
inteldrm_gmch_match(struct pci_attach_args *pa)
{
	if (pa->pa_bus == 0 && pa->pa_device == 0 && pa->pa_function == 0 &&
	    PCI_VENDOR(pa->pa_id) == PCI_VENDOR_INTEL &&
	    PCI_CLASS(pa->pa_class) == PCI_CLASS_BRIDGE &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_BRIDGE_HOST)
		return (1);
	return (0);
}

void
i915_alloc_ifp(struct inteldrm_softc *dev_priv, struct pci_attach_args *bpa)
{
	bus_addr_t	addr;
	u_int32_t	reg;

	dev_priv->ifp.i9xx.bst = bpa->pa_memt;

	reg = pci_conf_read(bpa->pa_pc, bpa->pa_tag, I915_IFPADDR);
	if (reg & 0x1) {
		addr = (bus_addr_t)reg;
		addr &= ~0x1;
		/* XXX extents ... need data on whether bioses alloc or not. */
		if (bus_space_map(bpa->pa_memt, addr, PAGE_SIZE, 0,
		    &dev_priv->ifp.i9xx.bsh) != 0)
			goto nope;
		return;
	} else if (bpa->pa_memex == NULL ||
	    extent_alloc_subregion(bpa->pa_memex, 0x100000, 0xffffffff,
	    PAGE_SIZE, PAGE_SIZE, 0, 0, 0, &addr) ||
	    bus_space_map(bpa->pa_memt, addr, PAGE_SIZE, 0,
	    &dev_priv->ifp.i9xx.bsh))
		goto nope;

	pci_conf_write(bpa->pa_pc, bpa->pa_tag, I915_IFPADDR, addr | 0x1);

	return;

nope:
	dev_priv->ifp.i9xx.bsh = 0;
	printf("%s: no ifp\n", dev_priv->sc_dev.dv_xname);
}

void
i965_alloc_ifp(struct inteldrm_softc *dev_priv, struct pci_attach_args *bpa)
{
	bus_addr_t	addr;
	u_int32_t	lo, hi;

	dev_priv->ifp.i9xx.bst = bpa->pa_memt;

	hi = pci_conf_read(bpa->pa_pc, bpa->pa_tag, I965_IFPADDR + 4);
	lo = pci_conf_read(bpa->pa_pc, bpa->pa_tag, I965_IFPADDR);
	if (lo & 0x1) {
		addr = (((u_int64_t)hi << 32) | lo);
		addr &= ~0x1;
		/* XXX extents ... need data on whether bioses alloc or not. */
		if (bus_space_map(bpa->pa_memt, addr, PAGE_SIZE, 0,
		    &dev_priv->ifp.i9xx.bsh) != 0)
			goto nope;
		return;
	} else if (bpa->pa_memex == NULL ||
	    extent_alloc_subregion(bpa->pa_memex, 0x100000, 0xffffffff,
	    PAGE_SIZE, PAGE_SIZE, 0, 0, 0, &addr) ||
	    bus_space_map(bpa->pa_memt, addr, PAGE_SIZE, 0,
	    &dev_priv->ifp.i9xx.bsh))
		goto nope;

	pci_conf_write(bpa->pa_pc, bpa->pa_tag, I965_IFPADDR + 4,
	    upper_32_bits(addr));
	pci_conf_write(bpa->pa_pc, bpa->pa_tag, I965_IFPADDR,
	    (addr & 0xffffffff) | 0x1);

	return;

nope:
	dev_priv->ifp.i9xx.bsh = 0;
	printf("%s: no ifp\n", dev_priv->sc_dev.dv_xname);
}

void
intel_gtt_chipset_setup(struct drm_device *dev)
{
	struct inteldrm_softc *dev_priv = dev->dev_private;
	struct pci_attach_args bpa;

	if (INTEL_INFO(dev_priv->dev)->gen >= 6)
		return;

	if (pci_find_device(&bpa, inteldrm_gmch_match) == 0) {
		printf("%s: can't find GMCH\n",
		    dev_priv->sc_dev.dv_xname);
		return;
	}

	/* Set up the IFP for chipset flushing */
	if (IS_I915G(dev) || IS_I915GM(dev) || IS_I945G(dev) ||
	    IS_I945GM(dev)) {
		i915_alloc_ifp(dev_priv, &bpa);
	} else if (INTEL_INFO(dev)->gen >= 4 || IS_G33(dev)) {
		i965_alloc_ifp(dev_priv, &bpa);
	} else {
		int nsegs;
		/*
		 * I8XX has no flush page mechanism, we fake it by writing until
		 * the cache is empty. allocate a page to scribble on
		 */
		dev_priv->ifp.i8xx.kva = NULL;
		if (bus_dmamem_alloc(dev_priv->dmat, PAGE_SIZE, 0, 0,
		    &dev_priv->ifp.i8xx.seg, 1, &nsegs, BUS_DMA_WAITOK) == 0) {
			if (bus_dmamem_map(dev_priv->dmat, &dev_priv->ifp.i8xx.seg,
			    1, PAGE_SIZE, &dev_priv->ifp.i8xx.kva, 0) != 0) {
				bus_dmamem_free(dev_priv->dmat,
				    &dev_priv->ifp.i8xx.seg, nsegs);
				dev_priv->ifp.i8xx.kva = NULL;
			}
		}
	}
}

int
intel_enable_gtt()
{
	struct inteldrm_softc *dev_priv = (void *)inteldrm_cd.cd_devs[0];

	intel_gtt_chipset_setup(dev_priv->dev);
	return 1;
}

int
intel_gmch_probe(struct pci_dev *bridge_dev, struct pci_dev *gpu_pdev,
    void *bridge)
{
	return 1;
}

void
intel_gtt_get(u64 *gtt_total, size_t *stolen_size,
    phys_addr_t *mappable_base, u64 *mappable_end)
{
	struct inteldrm_softc *dev_priv = (void *)inteldrm_cd.cd_devs[0];
	struct agp_info *ai = &dev_priv->dev->agp->info;
	
	*gtt_total = ai->ai_aperture_size;
	*stolen_size = 0;
	*mappable_base = ai->ai_aperture_base;
	*mappable_end = ai->ai_aperture_size;
}

void
intel_gtt_chipset_flush(void)
{
	struct inteldrm_softc *dev_priv = (void *)inteldrm_cd.cd_devs[0];

	/*
	 * Write to this flush page flushes the chipset write cache.
	 * The write will return when it is done.
	 */
	if (INTEL_INFO(dev_priv->dev)->gen >= 3) {
	    if (dev_priv->ifp.i9xx.bsh != 0)
		bus_space_write_4(dev_priv->ifp.i9xx.bst,
		    dev_priv->ifp.i9xx.bsh, 0, 1);
	} else {
		int i;

		wbinvd();

#define I830_HIC        0x70

		I915_WRITE(I830_HIC, (I915_READ(I830_HIC) | (1<<31)));
		for (i = 1000; i; i--) {
			if (!(I915_READ(I830_HIC) & (1<<31)))
				break;
			delay(100);
		}

	}
}

void
intel_gmch_remove(void)
{
}

void
intel_gtt_insert_sg_entries(struct sg_table *pages, unsigned int pg_start,
    unsigned int flags)
{
	struct inteldrm_softc *dev_priv = (void *)inteldrm_cd.cd_devs[0];
	struct agp_softc *sc = dev_priv->dev->agp->agpdev;
	bus_addr_t addr = sc->sc_apaddr + pg_start * PAGE_SIZE;
	struct sg_page_iter sg_iter;

	for_each_sg_page(pages->sgl, &sg_iter, pages->nents, 0) {
		sc->sc_methods->bind_page(sc->sc_chipc, addr,
		    sg_page_iter_dma_address(&sg_iter), flags);
		addr += PAGE_SIZE;
	}
}

void
intel_gtt_clear_range(unsigned int first_entry, unsigned int num_entries)
{
	struct inteldrm_softc *dev_priv = (void *)inteldrm_cd.cd_devs[0];
	struct agp_softc *sc = dev_priv->dev->agp->agpdev;
	bus_addr_t addr = sc->sc_apaddr + first_entry * PAGE_SIZE;
	int i;

	for (i = 0; i < num_entries; i++) {
		sc->sc_methods->unbind_page(sc->sc_chipc, addr);
		addr += PAGE_SIZE;
	}
}
