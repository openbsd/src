/* Public domain. */

#ifndef _LINUX_DMA_MAPPING_H
#define _LINUX_DMA_MAPPING_H

#include <linux/sizes.h>

#define DMA_BIT_MASK(n)	(((n) == 64) ? ~0ULL : (1ULL<<(n)) -1)

#define dma_set_coherent_mask(x, y)	0
#define dma_set_max_seg_size(x, y)	0

#endif
