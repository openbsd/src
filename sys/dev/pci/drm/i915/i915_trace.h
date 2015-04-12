/*	$OpenBSD: i915_trace.h,v 1.11 2015/04/12 17:10:07 kettenis Exp $	*/
/*
 * Copyright (c) 2013 Mark Kettenis <kettenis@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

static inline void
trace_i915_gem_object_create(struct drm_i915_gem_object *obj)
{
}

static inline void
trace_i915_gem_object_destroy(struct drm_i915_gem_object *obj)
{
}

static inline void
trace_i915_gem_request_add(struct intel_ring_buffer *ring, u32 seqno)
{
}

static inline void
trace_i915_gem_request_complete(struct intel_ring_buffer *ring, u32 seqno)
{
}

static inline void
trace_i915_gem_request_retire(struct intel_ring_buffer *ring, u32 seqno)
{
}

static inline void
trace_i915_gem_request_wait_begin(struct intel_ring_buffer *ring, u32 seqno)
{
}

static inline void
trace_i915_gem_request_wait_end(struct intel_ring_buffer *ring, u32 seqno)
{
}

static inline void
trace_i915_ring_wait_begin(struct intel_ring_buffer *ring)
{
}

static inline void
trace_i915_ring_wait_end(struct intel_ring_buffer *ring)
{
}

static inline void
trace_i915_flip_request(int plane, struct drm_i915_gem_object *obj)
{
}

static inline void
trace_i915_flip_complete(int plane, struct drm_i915_gem_object *obj)
{
}

static inline void
trace_i915_gem_object_change_domain(struct drm_i915_gem_object *obj,
				    u32 old_read, u32 old_write)
{
}

static inline void
trace_i915_gem_object_pwrite(struct drm_i915_gem_object *obj,
			     u32 offset, u32 len)
{
}

static inline void
trace_i915_gem_object_pread(struct drm_i915_gem_object *obj,
			    u32 offset, u32 len)
{
}

static inline void
trace_i915_gem_object_bind(struct drm_i915_gem_object *obj, bool mappable)
{
}

static inline void
trace_i915_gem_object_unbind(struct drm_i915_gem_object *obj)
{
}

static inline void
trace_i915_reg_rw(bool write, u32 reg, u64 val, int len)
{
}

static inline void
trace_i915_gem_evict(struct drm_device *dev, u32 size, u32 align, bool mappable)
{
}

static inline void
trace_i915_gem_evict_everything(struct drm_device *dev)
{
}

static inline void
trace_i915_gem_ring_dispatch(struct intel_ring_buffer *ring, u32 seqno,
			     u32 flags)
{
}

static inline void
trace_i915_gem_ring_flush(struct intel_ring_buffer *ring, u32 invalidate,
			     u32 flush)
{
}
