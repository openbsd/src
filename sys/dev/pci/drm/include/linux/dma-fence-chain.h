/* Public domain. */

#ifndef _LINUX_DMA_FENCE_CHAIN_H
#define _LINUX_DMA_FENCE_CHAIN_H

#include <linux/dma-fence.h>

struct dma_fence_chain {
	struct dma_fence base;
	struct dma_fence *fence;
	struct dma_fence *prev;
	uint64_t prev_seqno;
	struct mutex lock;
};

int dma_fence_chain_find_seqno(struct dma_fence **, uint64_t);
void dma_fence_chain_init(struct dma_fence_chain *, struct dma_fence *,
    struct dma_fence *, uint64_t);

extern const struct dma_fence_ops dma_fence_chain_ops;

static inline struct dma_fence_chain *
to_dma_fence_chain(struct dma_fence *fence)
{
	if (fence && fence->ops != &dma_fence_chain_ops)
		return NULL;

	return container_of(fence, struct dma_fence_chain, base);
}

struct dma_fence *dma_fence_chain_next(struct dma_fence *);

/* XXX walk chain */
#define dma_fence_chain_for_each(f, h) \
	for (f = dma_fence_get(h); f != NULL; f = dma_fence_chain_next(f))

#endif
