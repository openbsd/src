/* Public domain. */

#ifndef _LINUX_DMA_DIRECTION_H
#define _LINUX_DMA_DIRECTION_H

enum dma_data_direction {
	DMA_NONE,
	DMA_BIDIRECTIONAL,
	DMA_FROM_DEVICE,
	DMA_TO_DEVICE,
};

#endif
