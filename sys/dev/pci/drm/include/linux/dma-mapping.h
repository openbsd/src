/* Public domain. */

#ifndef _LINUX_DMA_MAPPING_H
#define _LINUX_DMA_MAPPING_H

#include <linux/sizes.h>
#include <linux/scatterlist.h>
#include <linux/dma-direction.h>

struct device;

#define DMA_BIT_MASK(n)	(((n) == 64) ? ~0ULL : (1ULL<<(n)) -1)

static inline int
dma_set_coherent_mask(struct device *dev, uint64_t m)
{
	return 0;
}

static inline int
dma_set_max_seg_size(struct device *dev, unsigned int sz)
{
	return 0;
}

static inline int
dma_set_mask(struct device *dev, uint64_t m)
{
	return 0;
}

static inline int
dma_set_mask_and_coherent(void *dev, uint64_t m)
{
	return 0;
}

static inline bool
dma_addressing_limited(void *dev)
{
	return false;
}

static inline dma_addr_t
dma_map_page(void *dev, struct vm_page *page, size_t offset,
    size_t size, enum dma_data_direction dir)
{
	return VM_PAGE_TO_PHYS(page);
}

static inline void
dma_unmap_page(void *dev, dma_addr_t addr, size_t size,
    enum dma_data_direction dir)
{
}

static inline int
dma_mapping_error(void *dev, dma_addr_t addr)
{
	return 0;
}

#endif
