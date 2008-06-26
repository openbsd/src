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

#define ATI_PCIGART_PAGE_SIZE		4096	/* PCI GART page size */

int drm_ati_pcigart_init(struct drm_device *dev,
    struct drm_ati_pcigart_info *gart_info)
{
	unsigned long pages;
	u32 *pci_gart = NULL, page_base;
	int i, j;

	if (dev->sg == NULL) {
		DRM_ERROR( "no scatter/gather memory!\n" );
		return 0;
	}

	if (gart_info->gart_table_location == DRM_ATI_GART_MAIN) {
		/* GART table in system memory */
		dev->sg->dmah = drm_pci_alloc(dev, gart_info->table_size, 0,
		    0xfffffffful);
		if (dev->sg->dmah == NULL) {
			DRM_ERROR("cannot allocate PCI GART table!\n");
			return 0;
		}
	
		gart_info->addr = (void *)dev->sg->dmah->vaddr;
		gart_info->bus_addr = dev->sg->dmah->busaddr;
		pci_gart = (u32 *)dev->sg->dmah->vaddr;
	} else {
		/* GART table in framebuffer memory */
		pci_gart = gart_info->addr;
	}
	
	pages = DRM_MIN(dev->sg->pages, gart_info->table_size / sizeof(u32));

	bzero(pci_gart, gart_info->table_size);

#if defined(__FreeBSD__)
	KASSERT(PAGE_SIZE >= ATI_PCIGART_PAGE_SIZE, ("page size too small"));
#else
	KASSERT(PAGE_SIZE >= ATI_PCIGART_PAGE_SIZE);
#endif

	for ( i = 0 ; i < pages ; i++ ) {
		page_base = (u32) dev->sg->busaddr[i];

		for (j = 0; j < (PAGE_SIZE / ATI_PCIGART_PAGE_SIZE); j++) {
			switch(gart_info->gart_reg_if) {
			case DRM_ATI_GART_IGP:
				*pci_gart = cpu_to_le32(page_base | 0xc);
				break;
			case DRM_ATI_GART_PCIE:
				*pci_gart = cpu_to_le32((page_base >> 8) | 0xc);
				break;
			default:
				*pci_gart = cpu_to_le32(page_base);
				break;
			}
			pci_gart++;
			page_base += ATI_PCIGART_PAGE_SIZE;
		}
	}

	DRM_MEMORYBARRIER();

	return 1;
}

int drm_ati_pcigart_cleanup(struct drm_device *dev,
    struct drm_ati_pcigart_info *gart_info)
{
	if (dev->sg == NULL) {
		DRM_ERROR( "no scatter/gather memory!\n" );
		return 0;
	}

	drm_pci_free(dev, dev->sg->dmah);

	return 1;
}
