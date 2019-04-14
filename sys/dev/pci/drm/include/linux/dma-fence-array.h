/* Public domain. */

#ifndef _LINUX_DMA_FENCE_ARRAY_H
#define _LINUX_DMA_FENCE_ARRAY_H

#include <linux/dma-fence.h>

#ifndef STUB
#include <sys/types.h>
#include <sys/systm.h>
#define STUB() do { printf("%s: stub\n", __func__); } while(0)
#endif

struct dma_fence_array {
	struct dma_fence base;
	unsigned int num_fences;
	struct dma_fence **fences;
};

static inline struct dma_fence_array *
to_dma_fence_array(struct dma_fence *fence)
{
	return NULL;
}

static inline bool
dma_fence_is_array(struct dma_fence *fence)
{
	return false;
}

static inline struct dma_fence_array *
dma_fence_array_create(int num_fences, struct dma_fence **fences, u64 context,
    unsigned seqno, bool signal_on_any)
{
	STUB();
	return NULL;
}

#endif
