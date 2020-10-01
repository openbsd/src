/* Public domain. */

#ifndef _LINUX_DMA_FENCE_CHAIN_H
#define _LINUX_DMA_FENCE_CHAIN_H

#include <linux/dma-fence.h>

struct dma_fence_chain {
	struct dma_fence base;
	struct dma_fence *fence;
	struct dma_fence *prev;
	uint64_t prev_seqno;
};

static inline int
dma_fence_chain_find_seqno(struct dma_fence **df, uint64_t seqno)
{
	if (seqno == 0)
		return 0;
	STUB();
	return -ENOSYS;
}

static inline struct dma_fence_chain *
to_dma_fence_chain(struct dma_fence *fence)
{
	STUB();
	return NULL;
}

/* XXX walk chain */
#define dma_fence_chain_for_each(f, h) \
	for (f = dma_fence_get(h); f != NULL; dma_fence_put(f), f = NULL)

static inline void
dma_fence_chain_init(struct dma_fence_chain *chain, struct dma_fence *prev,
    struct dma_fence *fence, uint64_t seqno)
{
	chain->fence = fence;
	chain->prev = prev;
	chain->prev_seqno = 0;
}

#endif
