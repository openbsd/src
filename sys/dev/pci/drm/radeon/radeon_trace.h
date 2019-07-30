/*	$OpenBSD: radeon_trace.h,v 1.2 2015/04/18 11:41:29 jsg Exp $	*/

#if !defined(_RADEON_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _RADEON_TRACE_H_

#include <dev/pci/drm/drmP.h>

#undef TRACE_SYSTEM
#define TRACE_SYSTEM radeon
#define TRACE_SYSTEM_STRING __stringify(TRACE_SYSTEM)
#define TRACE_INCLUDE_FILE radeon_trace

TRACE_EVENT(radeon_bo_create,
	    TP_PROTO(struct radeon_bo *bo),
	    TP_ARGS(bo),
	    TP_STRUCT__entry(
			     __field(struct radeon_bo *, bo)
			     __field(u32, pages)
			     ),

	    TP_fast_assign(
			   __entry->bo = bo;
			   __entry->pages = bo->tbo.num_pages;
			   ),
	    TP_printk("bo=%p, pages=%u", __entry->bo, __entry->pages)
);

DECLARE_EVENT_CLASS(radeon_fence_request,

	    TP_PROTO(struct drm_device *dev, u32 seqno),

	    TP_ARGS(dev, seqno),

	    TP_STRUCT__entry(
			     __field(u32, dev)
			     __field(u32, seqno)
			     ),

	    TP_fast_assign(
			   __entry->dev = dev->primary->index;
			   __entry->seqno = seqno;
			   ),

	    TP_printk("dev=%u, seqno=%u", __entry->dev, __entry->seqno)
);

DEFINE_EVENT(radeon_fence_request, radeon_fence_emit,

	    TP_PROTO(struct drm_device *dev, u32 seqno),

	    TP_ARGS(dev, seqno)
);

DEFINE_EVENT(radeon_fence_request, radeon_fence_retire,

	    TP_PROTO(struct drm_device *dev, u32 seqno),

	    TP_ARGS(dev, seqno)
);

DEFINE_EVENT(radeon_fence_request, radeon_fence_wait_begin,

	    TP_PROTO(struct drm_device *dev, u32 seqno),

	    TP_ARGS(dev, seqno)
);

DEFINE_EVENT(radeon_fence_request, radeon_fence_wait_end,

	    TP_PROTO(struct drm_device *dev, u32 seqno),

	    TP_ARGS(dev, seqno)
);

DECLARE_EVENT_CLASS(radeon_semaphore_request,

	    TP_PROTO(int ring, struct radeon_semaphore *sem),

	    TP_ARGS(ring, sem),

	    TP_STRUCT__entry(
			     __field(int, ring)
			     __field(signed, waiters)
			     __field(uint64_t, gpu_addr)
			     ),

	    TP_fast_assign(
			   __entry->ring = ring;
			   __entry->waiters = sem->waiters;
			   __entry->gpu_addr = sem->gpu_addr;
			   ),

	    TP_printk("ring=%u, waiters=%d, addr=%010Lx", __entry->ring,
		      __entry->waiters, __entry->gpu_addr)
);

DEFINE_EVENT(radeon_semaphore_request, radeon_semaphore_signale,

	    TP_PROTO(int ring, struct radeon_semaphore *sem),

	    TP_ARGS(ring, sem)
);

DEFINE_EVENT(radeon_semaphore_request, radeon_semaphore_wait,

	    TP_PROTO(int ring, struct radeon_semaphore *sem),

	    TP_ARGS(ring, sem)
);

#endif

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
