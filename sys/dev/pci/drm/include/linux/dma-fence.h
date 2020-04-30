/* Public domain. */

#ifndef _LINUX_DMA_FENCE_H
#define _LINUX_DMA_FENCE_H

#include <sys/types.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <linux/kref.h>
#include <linux/list.h>
#include <linux/bug.h>
#include <linux/sched.h>
#include <linux/rcupdate.h>

#define DMA_FENCE_TRACE(fence, fmt, args...) do {} while(0)

struct dma_fence {
	struct kref refcount;
	const struct dma_fence_ops *ops;
	unsigned long flags;
	unsigned int context;
	unsigned int seqno;
	struct mutex *lock;
	struct list_head cb_list;
	int error;
	struct rcu_head rcu;
};

enum dma_fence_flag_bits {
	DMA_FENCE_FLAG_SIGNALED_BIT,
	DMA_FENCE_FLAG_ENABLE_SIGNAL_BIT,
	DMA_FENCE_FLAG_USER_BITS,
};

struct dma_fence_ops {
	const char * (*get_driver_name)(struct dma_fence *);
	const char * (*get_timeline_name)(struct dma_fence *);
	bool (*enable_signaling)(struct dma_fence *);
	bool (*signaled)(struct dma_fence *);
	long (*wait)(struct dma_fence *, bool, long);
	void (*release)(struct dma_fence *);
};

struct dma_fence_cb;
typedef void (*dma_fence_func_t)(struct dma_fence *fence, struct dma_fence_cb *cb);

struct dma_fence_cb {
	struct list_head node;
	dma_fence_func_t func;
};

unsigned int dma_fence_context_alloc(unsigned int);

static inline struct dma_fence *
dma_fence_get(struct dma_fence *fence)
{
	if (fence)
		kref_get(&fence->refcount);
	return fence;
}

static inline struct dma_fence *
dma_fence_get_rcu(struct dma_fence *fence)
{
	if (fence)
		kref_get(&fence->refcount);
	return fence;
}

static inline struct dma_fence *
dma_fence_get_rcu_safe(struct dma_fence **dfp)
{
	struct dma_fence *fence;
	if (dfp == NULL)
		return NULL;
	fence = *dfp;
	if (fence)
		kref_get(&fence->refcount);
	return fence;
}

static inline void
dma_fence_release(struct kref *ref)
{
	struct dma_fence *fence = container_of(ref, struct dma_fence, refcount);
	if (fence->ops && fence->ops->release)
		fence->ops->release(fence);
	else
		free(fence, M_DRM, 0);
}

static inline void
dma_fence_free(struct dma_fence *fence)
{
	free(fence, M_DRM, 0);
}

static inline void
dma_fence_put(struct dma_fence *fence)
{
	if (fence)
		kref_put(&fence->refcount, dma_fence_release);
}

static inline int
dma_fence_signal(struct dma_fence *fence)
{
	if (fence == NULL)
		return -EINVAL;

	if (test_and_set_bit(DMA_FENCE_FLAG_SIGNALED_BIT, &fence->flags))
		return -EINVAL;

	if (test_bit(DMA_FENCE_FLAG_ENABLE_SIGNAL_BIT, &fence->flags)) {
		struct dma_fence_cb *cur, *tmp;

		mtx_enter(fence->lock);
		list_for_each_entry_safe(cur, tmp, &fence->cb_list, node) {
			list_del_init(&cur->node);
			cur->func(fence, cur);
		}
		mtx_leave(fence->lock);
	}

	return 0;
}

static inline int
dma_fence_signal_locked(struct dma_fence *fence)
{
	struct dma_fence_cb *cur, *tmp;

	if (fence == NULL)
		return -EINVAL;

	if (test_and_set_bit(DMA_FENCE_FLAG_SIGNALED_BIT, &fence->flags))
		return -EINVAL;

	list_for_each_entry_safe(cur, tmp, &fence->cb_list, node) {
		list_del_init(&cur->node);
		cur->func(fence, cur);
	}

	return 0;
}

static inline bool
dma_fence_is_signaled(struct dma_fence *fence)
{
	if (test_bit(DMA_FENCE_FLAG_SIGNALED_BIT, &fence->flags))
		return true;

	if (fence->ops->signaled && fence->ops->signaled(fence)) {
		dma_fence_signal(fence);
		return true;
	}

	return false;
}

static inline bool
dma_fence_is_signaled_locked(struct dma_fence *fence)
{
	if (test_bit(DMA_FENCE_FLAG_SIGNALED_BIT, &fence->flags))
		return true;

	if (fence->ops->signaled && fence->ops->signaled(fence)) {
		dma_fence_signal_locked(fence);
		return true;
	}

	return false;
}

long dma_fence_default_wait(struct dma_fence *, bool, long);

static inline long
dma_fence_wait_timeout(struct dma_fence *fence, bool intr, long timeout)
{
	if (timeout < 0)
		return -EINVAL;

	if (fence->ops->wait)
		return fence->ops->wait(fence, intr, timeout);
	else
		return dma_fence_default_wait(fence, intr, timeout);
}

static inline long
dma_fence_wait(struct dma_fence *fence, bool intr)
{
	long ret;

	ret = dma_fence_wait_timeout(fence, intr, MAX_SCHEDULE_TIMEOUT);
	if (ret < 0)
		return ret;
	
	return 0;
}

static inline void
dma_fence_enable_sw_signaling(struct dma_fence *fence)
{
	if (!test_and_set_bit(DMA_FENCE_FLAG_ENABLE_SIGNAL_BIT, &fence->flags) &&
	    !test_bit(DMA_FENCE_FLAG_SIGNALED_BIT, &fence->flags) &&
	    fence->ops->enable_signaling) {
		mtx_enter(fence->lock);
		if (!fence->ops->enable_signaling(fence))
			dma_fence_signal_locked(fence);
		mtx_leave(fence->lock);
	}
}

static inline void
dma_fence_init(struct dma_fence *fence, const struct dma_fence_ops *ops,
    struct mutex *lock, unsigned context, unsigned seqno)
{
	fence->ops = ops;
	fence->lock = lock;
	fence->context = context;
	fence->seqno = seqno;
	fence->flags = 0;
	fence->error = 0;
	kref_init(&fence->refcount);
	INIT_LIST_HEAD(&fence->cb_list);
}

static inline int
dma_fence_add_callback(struct dma_fence *fence, struct dma_fence_cb *cb,
    dma_fence_func_t func)
{
	int ret = 0;
	bool was_set;

	if (WARN_ON(!fence || !func))
		return -EINVAL;

	if (test_bit(DMA_FENCE_FLAG_SIGNALED_BIT, &fence->flags)) {
		INIT_LIST_HEAD(&cb->node);
		return -ENOENT;
	}

	mtx_enter(fence->lock);

	was_set = test_and_set_bit(DMA_FENCE_FLAG_ENABLE_SIGNAL_BIT, &fence->flags);

	if (test_bit(DMA_FENCE_FLAG_SIGNALED_BIT, &fence->flags))
		ret = -ENOENT;
	else if (!was_set && fence->ops->enable_signaling) {
		if (!fence->ops->enable_signaling(fence)) {
			dma_fence_signal_locked(fence);
			ret = -ENOENT;
		}
	}

	if (!ret) {
		cb->func = func;
		list_add_tail(&cb->node, &fence->cb_list);
	} else
		INIT_LIST_HEAD(&cb->node);
	mtx_leave(fence->lock);

	return ret;
}

static inline bool
dma_fence_remove_callback(struct dma_fence *fence, struct dma_fence_cb *cb)
{
	bool ret;

	mtx_enter(fence->lock);

	ret = !list_empty(&cb->node);
	if (ret)
		list_del_init(&cb->node);

	mtx_leave(fence->lock);

	return ret;
}

static inline bool
dma_fence_is_later(struct dma_fence *a, struct dma_fence *b)
{
	return (a->seqno > b->seqno);
}

static inline void
dma_fence_set_error(struct dma_fence *fence, int error)
{
	fence->error = error;
}

long dma_fence_wait_any_timeout(struct dma_fence **, uint32_t, bool, long,
    uint32_t *);

#endif
