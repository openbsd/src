/* SPDX-License-Identifier: GPL-2.0 OR MIT */
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

#include <drm/ttm/ttm_module.h>
#include <drm/ttm/ttm_bo_driver.h>
#include <drm/ttm/ttm_page_alloc.h>
#include <drm/ttm/ttm_placement.h>
#include <linux/agp_backend.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <asm/agp.h>

#include <drm/drm_agpsupport.h>

struct ttm_agp_backend {
	struct ttm_tt ttm;
	int bound;
	bus_addr_t addr;
	struct drm_agp_head *agp;
};

int ttm_agp_bind(struct ttm_tt *ttm, struct ttm_resource *bo_mem)
{
	struct ttm_agp_backend	*agp_be = container_of(ttm, struct ttm_agp_backend, ttm);
	struct vm_page *dummy_read_page = ttm_bo_glob.dummy_read_page;
	struct drm_mm_node *node = bo_mem->mm_node;
	struct agp_softc *sc = agp_be->agp->agpdev;
	bus_addr_t addr;
//	int cached = (bo_mem->placement & TTM_PL_FLAG_CACHED);
	unsigned i;

#ifdef notyet
	if (agp_be->mem)
		return 0;
#endif

	addr = sc->sc_apaddr + (node->start << PAGE_SHIFT);
	for (i = 0; i < ttm->num_pages; i++) {
		struct vm_page *page = ttm->pages[i];

		if (!page)
			page = dummy_read_page;

		sc->sc_methods->bind_page(sc->sc_chipc, addr, VM_PAGE_TO_PHYS(page), 0);
		addr += PAGE_SIZE;
	}
	agp_flush_cache();
	sc->sc_methods->flush_tlb(sc->sc_chipc);
	agp_be->addr = sc->sc_apaddr + (node->start << PAGE_SHIFT);
	agp_be->bound = 1;

	return 0;
}
EXPORT_SYMBOL(ttm_agp_bind);

void ttm_agp_unbind(struct ttm_tt *ttm)
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
}
EXPORT_SYMBOL(ttm_agp_unbind);

bool ttm_agp_is_bound(struct ttm_tt *ttm)
{
	struct ttm_agp_backend *agp_be = container_of(ttm, struct ttm_agp_backend, ttm);

	if (!ttm)
		return false;

	return (agp_be->bound == 1);
}
EXPORT_SYMBOL(ttm_agp_is_bound);

void ttm_agp_destroy(struct ttm_tt *ttm)
{
	struct ttm_agp_backend *agp_be = container_of(ttm, struct ttm_agp_backend, ttm);

	if (agp_be->bound)
		ttm_agp_unbind(ttm);
	ttm_tt_fini(ttm);
	kfree(agp_be);
}
EXPORT_SYMBOL(ttm_agp_destroy);

struct ttm_tt *ttm_agp_tt_create(struct ttm_buffer_object *bo,
				 struct drm_agp_head *agp,
				 uint32_t page_flags)
{
	struct ttm_agp_backend *agp_be;

	agp_be = kmalloc(sizeof(*agp_be), GFP_KERNEL);
	if (!agp_be)
		return NULL;

	agp_be->bound = 0;
	agp_be->agp = agp;

	if (ttm_tt_init(&agp_be->ttm, bo, page_flags)) {
		return NULL;
	}

	return &agp_be->ttm;
}
EXPORT_SYMBOL(ttm_agp_tt_create);
