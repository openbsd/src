/* $OpenBSD: ati_pcigart.c,v 1.16 2011/06/02 18:22:00 weerd Exp $ */
/*-
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
 *   Gareth Hughes <gareth@valinux.com>
 *
 */

/** @file ati_pcigart.c
 * Implementation of ATI's PCIGART, which provides an aperture in card virtual
 * address space with addresses remapped to system memory.
 */

#include "drmP.h"

#define	ATI_PCIGART_PAGE_SIZE		4096	/* PCI GART page size */
#define	ATI_PCIGART_PAGE_MASK		(~(ATI_PCIGART_PAGE_SIZE-1))

#define	ATI_PCIE_WRITE			0x4
#define	ATI_PCIE_READ			0x8

void	pcigart_add_entry(struct drm_ati_pcigart_info *, bus_size_t,
	    bus_addr_t);

void
pcigart_add_entry(struct drm_ati_pcigart_info *gart_info, bus_size_t offset,
    bus_addr_t entry_addr)
{
	u_int32_t	page_base = (u_int32_t)entry_addr &
	    		    ATI_PCIGART_PAGE_MASK;

	switch(gart_info->gart_reg_if) {
	case DRM_ATI_GART_IGP:
		page_base |= (upper_32_bits(entry_addr) & 0xff) << 4;
		page_base |= 0xc;
		break;
	case DRM_ATI_GART_PCIE:
		page_base >>= 8;
		page_base |= (upper_32_bits(entry_addr) & 0xff) << 24;
		page_base |= ATI_PCIE_READ | ATI_PCIE_WRITE;
		break;
	default:
	case DRM_ATI_GART_PCI:
		break;
	}
	if (gart_info->gart_table_location == DRM_ATI_GART_MAIN)
		gart_info->tbl.dma.addr[offset] = htole32(page_base);
	else
		bus_space_write_4(gart_info->tbl.fb.bst, gart_info->tbl.fb.bsh,
		    offset * sizeof(u_int32_t), page_base);
}

int
drm_ati_pcigart_cleanup(struct drm_device *dev,
    struct drm_ati_pcigart_info *gart_info)
{
	/* we need to support large memory configurations */
	if (dev->sg == NULL) {
		DRM_ERROR("no scatter/gather memory!\n");
		return (EINVAL);
	}

	if (gart_info->bus_addr) {
		gart_info->bus_addr = 0;
		if (gart_info->gart_table_location == DRM_ATI_GART_MAIN &&
		    gart_info->tbl.dma.mem != NULL) {
			drm_dmamem_free(dev->dmat, gart_info->tbl.dma.mem);
			gart_info->tbl.dma.mem = NULL;
		}
	}

	return (0);
}

int
drm_ati_pcigart_init(struct drm_device *dev,
    struct drm_ati_pcigart_info *gart_info)
{

	bus_addr_t	 entry_addr;
	bus_size_t	 gart_idx;
	u_long		 pages, max_ati_pages, max_real_pages;
	int		 i, j, ret;

	/* we need to support large memory configurations */
	if (dev->sg == NULL) {
		DRM_ERROR("no scatter/gather memory!\n");
		ret = EINVAL;
		goto error;
	}

	if (gart_info->gart_table_location == DRM_ATI_GART_MAIN) {
		int flags = 0;

		DRM_DEBUG("PCI: no table in VRAM: using normal RAM\n");

		if (gart_info->gart_reg_if == DRM_ATI_GART_IGP)
			flags |= BUS_DMA_NOCACHE;

		gart_info->tbl.dma.mem = drm_dmamem_alloc(dev->dmat,
		    gart_info->table_size, PAGE_SIZE, 1,
		    gart_info->table_size, flags, 0);
		if (gart_info->tbl.dma.mem == NULL) {
			DRM_ERROR("cannot allocate PCI GART page!\n");
			ret = ENOMEM;
			goto error;
		}

		gart_info->tbl.dma.addr =
		    (u_int32_t *)gart_info->tbl.dma.mem->kva;
		gart_info->bus_addr =
		    gart_info->tbl.dma.mem->map->dm_segs[0].ds_addr;
	} else {
		bus_space_set_region_1(gart_info->tbl.fb.bst,
		    gart_info->tbl.fb.bsh, 0, 0, gart_info->table_size);
	}

	max_ati_pages = (gart_info->table_size / sizeof(u_int32_t));
	max_real_pages = max_ati_pages / (PAGE_SIZE / ATI_PCIGART_PAGE_SIZE);
	pages = (dev->sg->mem->map->dm_nsegs <= max_real_pages) ?
	    dev->sg->mem->map->dm_nsegs : max_real_pages;

	KASSERT(PAGE_SIZE >= ATI_PCIGART_PAGE_SIZE);

	for (gart_idx = 0, i = 0; i < pages; i++) {
		entry_addr = dev->sg->mem->map->dm_segs[i].ds_addr;
		for (j = 0; j < (PAGE_SIZE / ATI_PCIGART_PAGE_SIZE);
		    j++, entry_addr += ATI_PCIGART_PAGE_SIZE)
			pcigart_add_entry(gart_info, gart_idx++, entry_addr);
	}

	DRM_MEMORYBARRIER();

	return (0);

error:
	gart_info->bus_addr = 0;
	return (ret);
}
