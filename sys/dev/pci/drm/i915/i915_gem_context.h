/*
 * Copyright © 2016 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */

#ifndef __I915_GEM_CONTEXT_H__
#define __I915_GEM_CONTEXT_H__

#include <linux/bitops.h>
#include <linux/list.h>
#include <linux/radix-tree.h>

#include "i915_gem.h"
#include "i915_scheduler.h"

struct pid;

struct drm_device;
struct drm_file;

struct drm_i915_private;
struct drm_i915_file_private;
struct i915_hw_ppgtt;
struct i915_request;
struct i915_vma;
struct intel_ring;

#define DEFAULT_CONTEXT_HANDLE 0

struct intel_context;

struct intel_context_ops {
	void (*unpin)(struct intel_context *ce);
	void (*destroy)(struct intel_context *ce);
};

/**
 * struct i915_gem_context - client state
 *
 * The struct i915_gem_context represents the combined view of the driver and
 * logical hardware state for a particular client.
 */
struct i915_gem_context {
	/** i915: i915 device backpointer */
	struct drm_i915_private *i915;

	/** file_priv: owning file descriptor */
	struct drm_i915_file_private *file_priv;

	/**
	 * @ppgtt: unique address space (GTT)
	 *
	 * In full-ppgtt mode, each context has its own address space ensuring
	 * complete seperation of one client from all others.
	 *
	 * In other modes, this is a NULL pointer with the expectation that
	 * the caller uses the shared global GTT.
	 */
	struct i915_hw_ppgtt *ppgtt;

	/**
	 * @pid: process id of creator
	 *
	 * Note that who created the context may not be the principle user,
	 * as the context may be shared across a local socket. However,
	 * that should only affect the default context, all contexts created
	 * explicitly by the client are expected to be isolated.
	 */
#ifdef __linux__
	struct pid *pid;
#else
	pid_t pid;
#endif

	/**
	 * @name: arbitrary name
	 *
	 * A name is constructed for the context from the creator's process
	 * name, pid and user handle in order to uniquely identify the
	 * context in messages.
	 */
	const char *name;

	/** link: place with &drm_i915_private.context_list */
	struct list_head link;
	struct llist_node free_link;

	/**
	 * @ref: reference count
	 *
	 * A reference to a context is held by both the client who created it
	 * and on each request submitted to the hardware using the request
	 * (to ensure the hardware has access to the state until it has
	 * finished all pending writes). See i915_gem_context_get() and
	 * i915_gem_context_put() for access.
	 */
	struct kref ref;

	/**
	 * @rcu: rcu_head for deferred freeing.
	 */
	struct rcu_head rcu;

	/**
	 * @flags: small set of booleans
	 */
	unsigned long flags;
#define CONTEXT_NO_ZEROMAP		BIT(0)
#define CONTEXT_NO_ERROR_CAPTURE	1
#define CONTEXT_CLOSED			2
#define CONTEXT_BANNABLE		3
#define CONTEXT_BANNED			4
#define CONTEXT_FORCE_SINGLE_SUBMISSION	5

	/**
	 * @hw_id: - unique identifier for the context
	 *
	 * The hardware needs to uniquely identify the context for a few
	 * functions like fault reporting, PASID, scheduling. The
	 * &drm_i915_private.context_hw_ida is used to assign a unqiue
	 * id for the lifetime of the context.
	 */
	unsigned int hw_id;

	/**
	 * @user_handle: userspace identifier
	 *
	 * A unique per-file identifier is generated from
	 * &drm_i915_file_private.contexts.
	 */
	u32 user_handle;

	struct i915_sched_attr sched;

	/** ggtt_offset_bias: placement restriction for context objects */
	u32 ggtt_offset_bias;

	/** engine: per-engine logical HW state */
	struct intel_context {
		struct i915_gem_context *gem_context;
		struct i915_vma *state;
		struct intel_ring *ring;
		u32 *lrc_reg_state;
		u64 lrc_desc;
		int pin_count;

		const struct intel_context_ops *ops;
	} __engine[I915_NUM_ENGINES];

	/** ring_size: size for allocating the per-engine ring buffer */
	u32 ring_size;
	/** desc_template: invariant fields for the HW context descriptor */
	u32 desc_template;

	/** guilty_count: How many times this context has caused a GPU hang. */
	atomic_t guilty_count;
	/**
	 * @active_count: How many times this context was active during a GPU
	 * hang, but did not cause it.
	 */
	atomic_t active_count;

#define CONTEXT_SCORE_GUILTY		10
#define CONTEXT_SCORE_BAN_THRESHOLD	40
	/** ban_score: Accumulated score of all hangs caused by this context. */
	atomic_t ban_score;

	/** remap_slice: Bitmask of cache lines that need remapping */
	u8 remap_slice;

	/** handles_vma: rbtree to look up our context specific obj/vma for
	 * the user handle. (user handles are per fd, but the binding is
	 * per vm, which may be one per context or shared with the global GTT)
	 */
	struct radix_tree_root handles_vma;

	/** handles_list: reverse list of all the rbtree entries in use for
	 * this context, which allows us to free all the allocations on
	 * context close.
	 */
	struct list_head handles_list;
};

static inline bool i915_gem_context_is_closed(const struct i915_gem_context *ctx)
{
	return test_bit(CONTEXT_CLOSED, &ctx->flags);
}

static inline void i915_gem_context_set_closed(struct i915_gem_context *ctx)
{
	GEM_BUG_ON(i915_gem_context_is_closed(ctx));
	__set_bit(CONTEXT_CLOSED, &ctx->flags);
}

static inline bool i915_gem_context_no_error_capture(const struct i915_gem_context *ctx)
{
	return test_bit(CONTEXT_NO_ERROR_CAPTURE, &ctx->flags);
}

static inline void i915_gem_context_set_no_error_capture(struct i915_gem_context *ctx)
{
	__set_bit(CONTEXT_NO_ERROR_CAPTURE, &ctx->flags);
}

static inline void i915_gem_context_clear_no_error_capture(struct i915_gem_context *ctx)
{
	__clear_bit(CONTEXT_NO_ERROR_CAPTURE, &ctx->flags);
}

static inline bool i915_gem_context_is_bannable(const struct i915_gem_context *ctx)
{
	return test_bit(CONTEXT_BANNABLE, &ctx->flags);
}

static inline void i915_gem_context_set_bannable(struct i915_gem_context *ctx)
{
	__set_bit(CONTEXT_BANNABLE, &ctx->flags);
}

static inline void i915_gem_context_clear_bannable(struct i915_gem_context *ctx)
{
	__clear_bit(CONTEXT_BANNABLE, &ctx->flags);
}

static inline bool i915_gem_context_is_banned(const struct i915_gem_context *ctx)
{
	return test_bit(CONTEXT_BANNED, &ctx->flags);
}

static inline void i915_gem_context_set_banned(struct i915_gem_context *ctx)
{
	__set_bit(CONTEXT_BANNED, &ctx->flags);
}

static inline bool i915_gem_context_force_single_submission(const struct i915_gem_context *ctx)
{
	return test_bit(CONTEXT_FORCE_SINGLE_SUBMISSION, &ctx->flags);
}

static inline void i915_gem_context_set_force_single_submission(struct i915_gem_context *ctx)
{
	__set_bit(CONTEXT_FORCE_SINGLE_SUBMISSION, &ctx->flags);
}

static inline bool i915_gem_context_is_default(const struct i915_gem_context *c)
{
	return c->user_handle == DEFAULT_CONTEXT_HANDLE;
}

static inline bool i915_gem_context_is_kernel(struct i915_gem_context *ctx)
{
	return !ctx->file_priv;
}

static inline struct intel_context *
to_intel_context(struct i915_gem_context *ctx,
		 const struct intel_engine_cs *engine)
{
	return &ctx->__engine[engine->id];
}

static inline struct intel_context *
intel_context_pin(struct i915_gem_context *ctx, struct intel_engine_cs *engine)
{
	return engine->context_pin(engine, ctx);
}

static inline void __intel_context_pin(struct intel_context *ce)
{
	GEM_BUG_ON(!ce->pin_count);
	ce->pin_count++;
}

static inline void intel_context_unpin(struct intel_context *ce)
{
	GEM_BUG_ON(!ce->pin_count);
	if (--ce->pin_count)
		return;

	GEM_BUG_ON(!ce->ops);
	ce->ops->unpin(ce);
}

/* i915_gem_context.c */
int __must_check i915_gem_contexts_init(struct drm_i915_private *dev_priv);
void i915_gem_contexts_lost(struct drm_i915_private *dev_priv);
void i915_gem_contexts_fini(struct drm_i915_private *dev_priv);

int i915_gem_context_open(struct drm_i915_private *i915,
			  struct drm_file *file);
void i915_gem_context_close(struct drm_file *file);

int i915_switch_context(struct i915_request *rq);
int i915_gem_switch_to_kernel_context(struct drm_i915_private *dev_priv);

void i915_gem_context_release(struct kref *ctx_ref);
struct i915_gem_context *
i915_gem_context_create_gvt(struct drm_device *dev);

int i915_gem_context_create_ioctl(struct drm_device *dev, void *data,
				  struct drm_file *file);
int i915_gem_context_destroy_ioctl(struct drm_device *dev, void *data,
				   struct drm_file *file);
int i915_gem_context_getparam_ioctl(struct drm_device *dev, void *data,
				    struct drm_file *file_priv);
int i915_gem_context_setparam_ioctl(struct drm_device *dev, void *data,
				    struct drm_file *file_priv);
int i915_gem_context_reset_stats_ioctl(struct drm_device *dev, void *data,
				       struct drm_file *file);

struct i915_gem_context *
i915_gem_context_create_kernel(struct drm_i915_private *i915, int prio);

static inline struct i915_gem_context *
i915_gem_context_get(struct i915_gem_context *ctx)
{
	kref_get(&ctx->ref);
	return ctx;
}

static inline void i915_gem_context_put(struct i915_gem_context *ctx)
{
	kref_put(&ctx->ref, i915_gem_context_release);
}

#endif /* !__I915_GEM_CONTEXT_H__ */
