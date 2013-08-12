/*	$OpenBSD: ttm_agp_backend.c,v 1.1 2013/08/12 04:11:53 jsg Exp $	*/
/**************************************************************************
 *
 * Copyright (c) 2006-2009 VMware, Inc., Palo Alto, CA., USA
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/
/*
 * Authors: Thomas Hellstrom <thellstrom-at-vmware-dot-com>
 *          Keith Packard.
 */

#define pr_fmt(fmt) "[TTM] " fmt

#include <dev/pci/drm/ttm/ttm_module.h>
#include <dev/pci/drm/ttm/ttm_bo_driver.h>
#include <dev/pci/drm/ttm/ttm_page_alloc.h>
#ifdef TTM_HAS_AGP
#include <dev/pci/drm/ttm/ttm_placement.h>

struct ttm_agp_backend {
	struct ttm_tt ttm;
	int bound;
	bus_addr_t addr;
	struct drm_agp_head *agp;
};

int	 ttm_agp_bind(struct ttm_tt *, struct ttm_mem_reg *);
int	 ttm_agp_unbind(struct ttm_tt *);
void	 ttm_agp_destroy(struct ttm_tt *);

int
ttm_agp_bind(struct ttm_tt *ttm, struct ttm_mem_reg *bo_mem)
{
	struct ttm_agp_backend	*agp_be = container_of(ttm, struct ttm_agp_backend, ttm);
	struct drm_mm_node *node = bo_mem->mm_node;
	struct agp_softc *sc = agp_be->agp->agpdev;
	bus_addr_t addr;
//	int cached = (bo_mem->placement & TTM_PL_FLAG_CACHED);
	unsigned i;

	addr = sc->sc_apaddr + (node->start << PAGE_SHIFT);
	for (i = 0; i < ttm->num_pages; i++) {
		struct vm_page *page = ttm->pages[i];

		if (!page)
			page = ttm->dummy_read_page;

		sc->sc_methods->bind_page(sc->sc_chipc, addr, VM_PAGE_TO_PHYS(page), 0);
		addr += PAGE_SIZE;
	}
	agp_flush_cache();
	sc->sc_methods->flush_tlb(sc->sc_chipc);
	agp_be->addr = sc->sc_apaddr + (node->start << PAGE_SHIFT);
	agp_be->bound = 1;

	return 0;
}

int
ttm_agp_unbind(struct ttm_tt *ttm)
{
	struct ttm_agp_backend *agp_be = container_of(ttm, struct ttm_agp_backend, ttm);
	struct agp_softc *sc = agp_be->agp->agpdev;
	bus_addr_t addr;
	unsigned i;

	if (agp_be->bound) {
		addr = agp_be->addr;
		for (i = 0; i < ttm->num_pages; i++) {
			sc->sc_methods->unbind_page(sc->sc_chipc, addr);
			addr += PAGE_SIZE;
		}
		agp_flush_cache();
		sc->sc_methods->flush_tlb(sc->sc_chipc);
		agp_be->bound = 0;
	}
	return 0;
}

void
ttm_agp_destroy(struct ttm_tt *ttm)
{
	struct ttm_agp_backend *agp_be = container_of(ttm, struct ttm_agp_backend, ttm);

	if (agp_be->bound)
		ttm_agp_unbind(ttm);
	ttm_tt_fini(ttm);
	free(agp_be, M_DRM);
}

static struct ttm_backend_func ttm_agp_func = {
	.bind = ttm_agp_bind,
	.unbind = ttm_agp_unbind,
	.destroy = ttm_agp_destroy,
};

struct ttm_tt *
ttm_agp_tt_create(struct ttm_bo_device *bdev,
				 struct drm_agp_head *agp,
				 unsigned long size, uint32_t page_flags,
				 struct vm_page *dummy_read_page)
{
	struct ttm_agp_backend *agp_be;

	agp_be = malloc(sizeof(*agp_be), M_DRM, M_WAITOK);
	if (!agp_be)
		return NULL;

	agp_be->bound = 0;
	agp_be->agp = agp;
	agp_be->ttm.func = &ttm_agp_func;

	if (ttm_tt_init(&agp_be->ttm, bdev, size, page_flags, dummy_read_page)) {
		return NULL;
	}

	return &agp_be->ttm;
}
EXPORT_SYMBOL(ttm_agp_tt_create);

int
ttm_agp_tt_populate(struct ttm_tt *ttm)
{
	if (ttm->state != tt_unpopulated)
		return 0;

	return ttm_pool_populate(ttm);
}
EXPORT_SYMBOL(ttm_agp_tt_populate);

void
ttm_agp_tt_unpopulate(struct ttm_tt *ttm)
{
	ttm_pool_unpopulate(ttm);
}
EXPORT_SYMBOL(ttm_agp_tt_unpopulate);

#endif
