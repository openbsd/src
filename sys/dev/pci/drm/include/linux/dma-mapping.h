/* Public domain. */

#ifndef _LINUX_DMA_MAPPING_H
#define _LINUX_DMA_MAPPING_H

#include <linux/sizes.h>
#include <linux/scatterlist.h>

#define DMA_BIT_MASK(n)	(((n) == 64) ? ~0ULL : (1ULL<<(n)) -1)

#define dma_set_coherent_mask(x, y)	0
#define dma_set_max_seg_size(x, y)	0
#define dma_set_mask(x, y)		0
#define dma_set_mask_and_coherent(x, y)	0
#define dma_addressing_limited(x)	false

#define DMA_BIDIRECTIONAL	0

#define dma_map_page(dev, page, offset, size, dir)	VM_PAGE_TO_PHYS(page)
#define dma_unmap_page(dev, addr, size, dir)		do {} while(0)
#define dma_mapping_error(dev, addr)			0

#endif
