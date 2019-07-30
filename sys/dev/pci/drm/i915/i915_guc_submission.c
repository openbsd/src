/*
 * Copyright © 2014 Intel Corporation
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
#ifdef __linux__
#include <linux/firmware.h>
#include <linux/circ_buf.h>
#endif
#include "i915_drv.h"
#include "intel_guc.h"

#ifdef notyet

/**
 * DOC: GuC Client
 *
 * i915_guc_client:
 * We use the term client to avoid confusion with contexts. A i915_guc_client is
 * equivalent to GuC object guc_context_desc. This context descriptor is
 * allocated from a pool of 1024 entries. Kernel driver will allocate doorbell
 * and workqueue for it. Also the process descriptor (guc_process_desc), which
 * is mapped to client space. So the client can write Work Item then ring the
 * doorbell.
 *
 * To simplify the implementation, we allocate one gem object that contains all
 * pages for doorbell, process descriptor and workqueue.
 *
 * The Scratch registers:
 * There are 16 MMIO-based registers start from 0xC180. The kernel driver writes
 * a value to the action register (SOFT_SCRATCH_0) along with any data. It then
 * triggers an interrupt on the GuC via another register write (0xC4C8).
 * Firmware writes a success/fail code back to the action register after
 * processes the request. The kernel driver polls waiting for this update and
 * then proceeds.
 * See host2guc_action()
 *
 * Doorbells:
 * Doorbells are interrupts to uKernel. A doorbell is a single cache line (QW)
 * mapped into process space.
 *
 * Work Items:
 * There are several types of work items that the host may place into a
 * workqueue, each with its own requirements and limitations. Currently only
 * WQ_TYPE_INORDER is needed to support legacy submission via GuC, which
 * represents in-order queue. The kernel driver packs ring tail pointer and an
 * ELSP context descriptor dword into Work Item.
 * See guc_add_workqueue_item()
 *
 */

/*
 * Read GuC command/status register (SOFT_SCRATCH_0)
 * Return true if it contains a response rather than a command
 */
static inline bool host2guc_action_response(struct drm_i915_private *dev_priv,
					    u32 *status)
{
	u32 val = I915_READ(SOFT_SCRATCH(0));
	*status = val;
	return GUC2HOST_IS_RESPONSE(val);
}

static int host2guc_action(struct intel_guc *guc, u32 *data, u32 len)
{
	struct drm_i915_private *dev_priv = guc_to_i915(guc);
	u32 status;
	int i;
	int ret;

	if (WARN_ON(len < 1 || len > 15))
		return -EINVAL;

	intel_uncore_forcewake_get(dev_priv, FORCEWAKE_ALL);
	spin_lock(&dev_priv->guc.host2guc_lock);

	dev_priv->guc.action_count += 1;
	dev_priv->guc.action_cmd = data[0];

	for (i = 0; i < len; i++)
		I915_WRITE(SOFT_SCRATCH(i), data[i]);

	POSTING_READ(SOFT_SCRATCH(i - 1));

	I915_WRITE(HOST2GUC_INTERRUPT, HOST2GUC_TRIGGER);

	/* No HOST2GUC command should take longer than 10ms */
	ret = wait_for_atomic(host2guc_action_response(dev_priv, &status), 10);
	if (status != GUC2HOST_STATUS_SUCCESS) {
		/*
		 * Either the GuC explicitly returned an error (which
		 * we convert to -EIO here) or no response at all was
		 * received within the timeout limit (-ETIMEDOUT)
		 */
		if (ret != -ETIMEDOUT)
			ret = -EIO;

		DRM_ERROR("GUC: host2guc action 0x%X failed. ret=%d "
				"status=0x%08X response=0x%08X\n",
				data[0], ret, status,
				I915_READ(SOFT_SCRATCH(15)));

		dev_priv->guc.action_fail += 1;
		dev_priv->guc.action_err = ret;
	}
	dev_priv->guc.action_status = status;

	spin_unlock(&dev_priv->guc.host2guc_lock);
	intel_uncore_forcewake_put(dev_priv, FORCEWAKE_ALL);

	return ret;
}

/*
 * Tell the GuC to allocate or deallocate a specific doorbell
 */

static int host2guc_allocate_doorbell(struct intel_guc *guc,
				      struct i915_guc_client *client)
{
	u32 data[2];

	data[0] = HOST2GUC_ACTION_ALLOCATE_DOORBELL;
	data[1] = client->ctx_index;

	return host2guc_action(guc, data, 2);
}

static int host2guc_release_doorbell(struct intel_guc *guc,
				     struct i915_guc_client *client)
{
	u32 data[2];

	data[0] = HOST2GUC_ACTION_DEALLOCATE_DOORBELL;
	data[1] = client->ctx_index;

	return host2guc_action(guc, data, 2);
}

static int host2guc_sample_forcewake(struct intel_guc *guc,
				     struct i915_guc_client *client)
{
	struct drm_i915_private *dev_priv = guc_to_i915(guc);
	struct drm_device *dev = dev_priv->dev;
	u32 data[2];

	data[0] = HOST2GUC_ACTION_SAMPLE_FORCEWAKE;
	/* WaRsDisableCoarsePowerGating:skl,bxt */
	if (!intel_enable_rc6(dev_priv->dev) ||
	    IS_BXT_REVID(dev, 0, BXT_REVID_A1) ||
	    (IS_SKL_GT3(dev) && IS_SKL_REVID(dev, 0, SKL_REVID_E0)) ||
	    (IS_SKL_GT4(dev) && IS_SKL_REVID(dev, 0, SKL_REVID_E0)))
		data[1] = 0;
	else
		/* bit 0 and 1 are for Render and Media domain separately */
		data[1] = GUC_FORCEWAKE_RENDER | GUC_FORCEWAKE_MEDIA;

	return host2guc_action(guc, data, ARRAY_SIZE(data));
}

/*
 * Initialise, update, or clear doorbell data shared with the GuC
 *
 * These functions modify shared data and so need access to the mapped
 * client object which contains the page being used for the doorbell
 */

static void guc_init_doorbell(struct intel_guc *guc,
			      struct i915_guc_client *client)
{
	struct guc_doorbell_info *doorbell;
	void *base;

	base = kmap_atomic(i915_gem_object_get_page(client->client_obj, 0));
	doorbell = base + client->doorbell_offset;

	doorbell->db_status = 1;
	doorbell->cookie = 0;

	kunmap_atomic(base);
}

static int guc_ring_doorbell(struct i915_guc_client *gc)
{
	struct guc_process_desc *desc;
	union guc_doorbell_qw db_cmp, db_exc, db_ret;
	union guc_doorbell_qw *db;
	void *base;
	int attempt = 2, ret = -EAGAIN;

	base = kmap_atomic(i915_gem_object_get_page(gc->client_obj, 0));
	desc = base + gc->proc_desc_offset;

	/* Update the tail so it is visible to GuC */
	desc->tail = gc->wq_tail;

	/* current cookie */
	db_cmp.db_status = GUC_DOORBELL_ENABLED;
	db_cmp.cookie = gc->cookie;

	/* cookie to be updated */
	db_exc.db_status = GUC_DOORBELL_ENABLED;
	db_exc.cookie = gc->cookie + 1;
	if (db_exc.cookie == 0)
		db_exc.cookie = 1;

	/* pointer of current doorbell cacheline */
	db = base + gc->doorbell_offset;

	while (attempt--) {
		/* lets ring the doorbell */
		db_ret.value_qw = atomic64_cmpxchg((atomic64_t *)db,
			db_cmp.value_qw, db_exc.value_qw);

		/* if the exchange was successfully executed */
		if (db_ret.value_qw == db_cmp.value_qw) {
			/* db was successfully rung */
			gc->cookie = db_exc.cookie;
			ret = 0;
			break;
		}

		/* XXX: doorbell was lost and need to acquire it again */
		if (db_ret.db_status == GUC_DOORBELL_DISABLED)
			break;

		DRM_ERROR("Cookie mismatch. Expected %d, returned %d\n",
			  db_cmp.cookie, db_ret.cookie);

		/* update the cookie to newly read cookie from GuC */
		db_cmp.cookie = db_ret.cookie;
		db_exc.cookie = db_ret.cookie + 1;
		if (db_exc.cookie == 0)
			db_exc.cookie = 1;
	}

	kunmap_atomic(base);
	return ret;
}

static void guc_disable_doorbell(struct intel_guc *guc,
				 struct i915_guc_client *client)
{
	struct drm_i915_private *dev_priv = guc_to_i915(guc);
	struct guc_doorbell_info *doorbell;
	void *base;
	int drbreg = GEN8_DRBREGL(client->doorbell_id);
	int value;

	base = kmap_atomic(i915_gem_object_get_page(client->client_obj, 0));
	doorbell = base + client->doorbell_offset;

	doorbell->db_status = 0;

	kunmap_atomic(base);

	I915_WRITE(drbreg, I915_READ(drbreg) & ~GEN8_DRB_VALID);

	value = I915_READ(drbreg);
	WARN_ON((value & GEN8_DRB_VALID) != 0);

	I915_WRITE(GEN8_DRBREGU(client->doorbell_id), 0);
	I915_WRITE(drbreg, 0);

	/* XXX: wait for any interrupts */
	/* XXX: wait for workqueue to drain */
}

/*
 * Select, assign and relase doorbell cachelines
 *
 * These functions track which doorbell cachelines are in use.
 * The data they manipulate is protected by the host2guc lock.
 */

static uint32_t select_doorbell_cacheline(struct intel_guc *guc)
{
	const uint32_t cacheline_size = cache_line_size();
	uint32_t offset;

	spin_lock(&guc->host2guc_lock);

	/* Doorbell uses a single cache line within a page */
	offset = offset_in_page(guc->db_cacheline);

	/* Moving to next cache line to reduce contention */
	guc->db_cacheline += cacheline_size;

	spin_unlock(&guc->host2guc_lock);

	DRM_DEBUG_DRIVER("selected doorbell cacheline 0x%x, next 0x%x, linesize %u\n",
			offset, guc->db_cacheline, cacheline_size);

	return offset;
}

static uint16_t assign_doorbell(struct intel_guc *guc, uint32_t priority)
{
	/*
	 * The bitmap is split into two halves; the first half is used for
	 * normal priority contexts, the second half for high-priority ones.
	 * Note that logically higher priorities are numerically less than
	 * normal ones, so the test below means "is it high-priority?"
	 */
	const bool hi_pri = (priority <= GUC_CTX_PRIORITY_HIGH);
	const uint16_t half = GUC_MAX_DOORBELLS / 2;
	const uint16_t start = hi_pri ? half : 0;
	const uint16_t end = start + half;
	uint16_t id;

	spin_lock(&guc->host2guc_lock);
	id = find_next_zero_bit(guc->doorbell_bitmap, end, start);
	if (id == end)
		id = GUC_INVALID_DOORBELL_ID;
	else
		bitmap_set(guc->doorbell_bitmap, id, 1);
	spin_unlock(&guc->host2guc_lock);

	DRM_DEBUG_DRIVER("assigned %s priority doorbell id 0x%x\n",
			hi_pri ? "high" : "normal", id);

	return id;
}

static void release_doorbell(struct intel_guc *guc, uint16_t id)
{
	spin_lock(&guc->host2guc_lock);
	bitmap_clear(guc->doorbell_bitmap, id, 1);
	spin_unlock(&guc->host2guc_lock);
}

/*
 * Initialise the process descriptor shared with the GuC firmware.
 */
static void guc_init_proc_desc(struct intel_guc *guc,
			       struct i915_guc_client *client)
{
	struct guc_process_desc *desc;
	void *base;

	base = kmap_atomic(i915_gem_object_get_page(client->client_obj, 0));
	desc = base + client->proc_desc_offset;

	memset(desc, 0, sizeof(*desc));

	/*
	 * XXX: pDoorbell and WQVBaseAddress are pointers in process address
	 * space for ring3 clients (set them as in mmap_ioctl) or kernel
	 * space for kernel clients (map on demand instead? May make debug
	 * easier to have it mapped).
	 */
	desc->wq_base_addr = 0;
	desc->db_base_addr = 0;

	desc->context_id = client->ctx_index;
	desc->wq_size_bytes = client->wq_size;
	desc->wq_status = WQ_STATUS_ACTIVE;
	desc->priority = client->priority;

	kunmap_atomic(base);
}

/*
 * Initialise/clear the context descriptor shared with the GuC firmware.
 *
 * This descriptor tells the GuC where (in GGTT space) to find the important
 * data structures relating to this client (doorbell, process descriptor,
 * write queue, etc).
 */

static void guc_init_ctx_desc(struct intel_guc *guc,
			      struct i915_guc_client *client)
{
	struct intel_context *ctx = client->owner;
	struct guc_context_desc desc;
	struct sg_table *sg;
	int i;

	memset(&desc, 0, sizeof(desc));

	desc.attribute = GUC_CTX_DESC_ATTR_ACTIVE | GUC_CTX_DESC_ATTR_KERNEL;
	desc.context_id = client->ctx_index;
	desc.priority = client->priority;
	desc.db_id = client->doorbell_id;

	for (i = 0; i < I915_NUM_RINGS; i++) {
		struct guc_execlist_context *lrc = &desc.lrc[i];
		struct intel_ringbuffer *ringbuf = ctx->engine[i].ringbuf;
		struct intel_engine_cs *ring;
		struct drm_i915_gem_object *obj;
		uint64_t ctx_desc;

		/* TODO: We have a design issue to be solved here. Only when we
		 * receive the first batch, we know which engine is used by the
		 * user. But here GuC expects the lrc and ring to be pinned. It
		 * is not an issue for default context, which is the only one
		 * for now who owns a GuC client. But for future owner of GuC
		 * client, need to make sure lrc is pinned prior to enter here.
		 */
		obj = ctx->engine[i].state;
		if (!obj)
			break;	/* XXX: continue? */

		ring = ringbuf->ring;
		ctx_desc = intel_lr_context_descriptor(ctx, ring);
		lrc->context_desc = (u32)ctx_desc;

		/* The state page is after PPHWSP */
		lrc->ring_lcra = i915_gem_obj_ggtt_offset(obj) +
				LRC_STATE_PN * PAGE_SIZE;
		lrc->context_id = (client->ctx_index << GUC_ELC_CTXID_OFFSET) |
				(ring->id << GUC_ELC_ENGINE_OFFSET);

		obj = ringbuf->obj;

		lrc->ring_begin = i915_gem_obj_ggtt_offset(obj);
		lrc->ring_end = lrc->ring_begin + obj->base.size - 1;
		lrc->ring_next_free_location = lrc->ring_begin;
		lrc->ring_current_tail_pointer_value = 0;

		desc.engines_used |= (1 << ring->id);
	}

	WARN_ON(desc.engines_used == 0);

	/*
	 * The CPU address is only needed at certain points, so kmap_atomic on
	 * demand instead of storing it in the ctx descriptor.
	 * XXX: May make debug easier to have it mapped
	 */
	desc.db_trigger_cpu = 0;
	desc.db_trigger_uk = client->doorbell_offset +
		i915_gem_obj_ggtt_offset(client->client_obj);
	desc.db_trigger_phy = client->doorbell_offset +
		sg_dma_address(client->client_obj->pages->sgl);

	desc.process_desc = client->proc_desc_offset +
		i915_gem_obj_ggtt_offset(client->client_obj);

	desc.wq_addr = client->wq_offset +
		i915_gem_obj_ggtt_offset(client->client_obj);

	desc.wq_size = client->wq_size;

	/*
	 * XXX: Take LRCs from an existing intel_context if this is not an
	 * IsKMDCreatedContext client
	 */
	desc.desc_private = (uintptr_t)client;

	/* Pool context is pinned already */
	sg = guc->ctx_pool_obj->pages;
	sg_pcopy_from_buffer(sg->sgl, sg->nents, &desc, sizeof(desc),
			     sizeof(desc) * client->ctx_index);
}

static void guc_fini_ctx_desc(struct intel_guc *guc,
			      struct i915_guc_client *client)
{
	struct guc_context_desc desc;
	struct sg_table *sg;

	memset(&desc, 0, sizeof(desc));

	sg = guc->ctx_pool_obj->pages;
	sg_pcopy_from_buffer(sg->sgl, sg->nents, &desc, sizeof(desc),
			     sizeof(desc) * client->ctx_index);
}

/* Get valid workqueue item and return it back to offset */
static int guc_get_workqueue_space(struct i915_guc_client *gc, u32 *offset)
{
	struct guc_process_desc *desc;
	void *base;
	u32 size = sizeof(struct guc_wq_item);
	int ret = 0, timeout_counter = 200;

	base = kmap_atomic(i915_gem_object_get_page(gc->client_obj, 0));
	desc = base + gc->proc_desc_offset;

	while (timeout_counter-- > 0) {
		ret = wait_for_atomic(CIRC_SPACE(gc->wq_tail, desc->head,
				gc->wq_size) >= size, 1);

		if (!ret) {
			*offset = gc->wq_tail;

			/* advance the tail for next workqueue item */
			gc->wq_tail += size;
			gc->wq_tail &= gc->wq_size - 1;

			/* this will break the loop */
			timeout_counter = 0;
		}
	};

	kunmap_atomic(base);

	return ret;
}

static int guc_add_workqueue_item(struct i915_guc_client *gc,
				  struct drm_i915_gem_request *rq)
{
	enum intel_ring_id ring_id = rq->ring->id;
	struct guc_wq_item *wqi;
	void *base;
	u32 tail, wq_len, wq_off = 0;
	int ret;

	ret = guc_get_workqueue_space(gc, &wq_off);
	if (ret)
		return ret;

	/* For now workqueue item is 4 DWs; workqueue buffer is 2 pages. So we
	 * should not have the case where structure wqi is across page, neither
	 * wrapped to the beginning. This simplifies the implementation below.
	 *
	 * XXX: if not the case, we need save data to a temp wqi and copy it to
	 * workqueue buffer dw by dw.
	 */
	WARN_ON(sizeof(struct guc_wq_item) != 16);
	WARN_ON(wq_off & 3);

	/* wq starts from the page after doorbell / process_desc */
	base = kmap_atomic(i915_gem_object_get_page(gc->client_obj,
			(wq_off + GUC_DB_SIZE) >> PAGE_SHIFT));
	wq_off &= PAGE_SIZE - 1;
	wqi = (struct guc_wq_item *)((char *)base + wq_off);

	/* len does not include the header */
	wq_len = sizeof(struct guc_wq_item) / sizeof(u32) - 1;
	wqi->header = WQ_TYPE_INORDER |
			(wq_len << WQ_LEN_SHIFT) |
			(ring_id << WQ_TARGET_SHIFT) |
			WQ_NO_WCFLUSH_WAIT;

	/* The GuC wants only the low-order word of the context descriptor */
	wqi->context_desc = (u32)intel_lr_context_descriptor(rq->ctx, rq->ring);

	/* The GuC firmware wants the tail index in QWords, not bytes */
	tail = rq->ringbuf->tail >> 3;
	wqi->ring_tail = tail << WQ_RING_TAIL_SHIFT;
	wqi->fence_id = 0; /*XXX: what fence to be here */

	kunmap_atomic(base);

	return 0;
}

#define CTX_RING_BUFFER_START		0x08

/* Update the ringbuffer pointer in a saved context image */
static void lr_context_update(struct drm_i915_gem_request *rq)
{
	enum intel_ring_id ring_id = rq->ring->id;
	struct drm_i915_gem_object *ctx_obj = rq->ctx->engine[ring_id].state;
	struct drm_i915_gem_object *rb_obj = rq->ringbuf->obj;
	struct page *page;
	uint32_t *reg_state;

	BUG_ON(!ctx_obj);
	WARN_ON(!i915_gem_obj_is_pinned(ctx_obj));
	WARN_ON(!i915_gem_obj_is_pinned(rb_obj));

	page = i915_gem_object_get_page(ctx_obj, LRC_STATE_PN);
	reg_state = kmap_atomic(page);

	reg_state[CTX_RING_BUFFER_START+1] = i915_gem_obj_ggtt_offset(rb_obj);

	kunmap_atomic(reg_state);
}

/**
 * i915_guc_submit() - Submit commands through GuC
 * @client:	the guc client where commands will go through
 * @ctx:	LRC where commands come from
 * @ring:	HW engine that will excute the commands
 *
 * Return:	0 if succeed
 */
int i915_guc_submit(struct i915_guc_client *client,
		    struct drm_i915_gem_request *rq)
{
	struct intel_guc *guc = client->guc;
	enum intel_ring_id ring_id = rq->ring->id;
	unsigned long flags;
	int q_ret, b_ret;

	/* Need this because of the deferred pin ctx and ring */
	/* Shall we move this right after ring is pinned? */
	lr_context_update(rq);

	spin_lock_irqsave(&client->wq_lock, flags);

	q_ret = guc_add_workqueue_item(client, rq);
	if (q_ret == 0)
		b_ret = guc_ring_doorbell(client);

	client->submissions[ring_id] += 1;
	if (q_ret) {
		client->q_fail += 1;
		client->retcode = q_ret;
	} else if (b_ret) {
		client->b_fail += 1;
		client->retcode = q_ret = b_ret;
	} else {
		client->retcode = 0;
	}
	spin_unlock_irqrestore(&client->wq_lock, flags);

	spin_lock(&guc->host2guc_lock);
	guc->submissions[ring_id] += 1;
	guc->last_seqno[ring_id] = rq->seqno;
	spin_unlock(&guc->host2guc_lock);

	return q_ret;
}

/*
 * Everything below here is concerned with setup & teardown, and is
 * therefore not part of the somewhat time-critical batch-submission
 * path of i915_guc_submit() above.
 */

/**
 * gem_allocate_guc_obj() - Allocate gem object for GuC usage
 * @dev:	drm device
 * @size:	size of object
 *
 * This is a wrapper to create a gem obj. In order to use it inside GuC, the
 * object needs to be pinned lifetime. Also we must pin it to gtt space other
 * than [0, GUC_WOPCM_TOP) because this range is reserved inside GuC.
 *
 * Return:	A drm_i915_gem_object if successful, otherwise NULL.
 */
static struct drm_i915_gem_object *gem_allocate_guc_obj(struct drm_device *dev,
							u32 size)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_i915_gem_object *obj;

	obj = i915_gem_alloc_object(dev, size);
	if (!obj)
		return NULL;

	if (i915_gem_object_get_pages(obj)) {
		drm_gem_object_unreference(&obj->base);
		return NULL;
	}

	if (i915_gem_obj_ggtt_pin(obj, PAGE_SIZE,
			PIN_OFFSET_BIAS | GUC_WOPCM_TOP)) {
		drm_gem_object_unreference(&obj->base);
		return NULL;
	}

	/* Invalidate GuC TLB to let GuC take the latest updates to GTT. */
	I915_WRITE(GEN8_GTCR, GEN8_GTCR_INVALIDATE);

	return obj;
}

/**
 * gem_release_guc_obj() - Release gem object allocated for GuC usage
 * @obj:	gem obj to be released
  */
static void gem_release_guc_obj(struct drm_i915_gem_object *obj)
{
	if (!obj)
		return;

	if (i915_gem_obj_is_pinned(obj))
		i915_gem_object_ggtt_unpin(obj);

	drm_gem_object_unreference(&obj->base);
}

static void guc_client_free(struct drm_device *dev,
			    struct i915_guc_client *client)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_guc *guc = &dev_priv->guc;

	if (!client)
		return;

	if (client->doorbell_id != GUC_INVALID_DOORBELL_ID) {
		/*
		 * First disable the doorbell, then tell the GuC we've
		 * finished with it, finally deallocate it in our bitmap
		 */
		guc_disable_doorbell(guc, client);
		host2guc_release_doorbell(guc, client);
		release_doorbell(guc, client->doorbell_id);
	}

	/*
	 * XXX: wait for any outstanding submissions before freeing memory.
	 * Be sure to drop any locks
	 */

	gem_release_guc_obj(client->client_obj);

	if (client->ctx_index != GUC_INVALID_CTX_ID) {
		guc_fini_ctx_desc(guc, client);
		ida_simple_remove(&guc->ctx_ids, client->ctx_index);
	}

	kfree(client);
}

/**
 * guc_client_alloc() - Allocate an i915_guc_client
 * @dev:	drm device
 * @priority:	four levels priority _CRITICAL, _HIGH, _NORMAL and _LOW
 * 		The kernel client to replace ExecList submission is created with
 * 		NORMAL priority. Priority of a client for scheduler can be HIGH,
 * 		while a preemption context can use CRITICAL.
 * @ctx		the context to own the client (we use the default render context)
 *
 * Return:	An i915_guc_client object if success.
 */
static struct i915_guc_client *guc_client_alloc(struct drm_device *dev,
						uint32_t priority,
						struct intel_context *ctx)
{
	struct i915_guc_client *client;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_guc *guc = &dev_priv->guc;
	struct drm_i915_gem_object *obj;

	client = kzalloc(sizeof(*client), GFP_KERNEL);
	if (!client)
		return NULL;

	client->doorbell_id = GUC_INVALID_DOORBELL_ID;
	client->priority = priority;
	client->owner = ctx;
	client->guc = guc;

	client->ctx_index = (uint32_t)ida_simple_get(&guc->ctx_ids, 0,
			GUC_MAX_GPU_CONTEXTS, GFP_KERNEL);
	if (client->ctx_index >= GUC_MAX_GPU_CONTEXTS) {
		client->ctx_index = GUC_INVALID_CTX_ID;
		goto err;
	}

	/* The first page is doorbell/proc_desc. Two followed pages are wq. */
	obj = gem_allocate_guc_obj(dev, GUC_DB_SIZE + GUC_WQ_SIZE);
	if (!obj)
		goto err;

	client->client_obj = obj;
	client->wq_offset = GUC_DB_SIZE;
	client->wq_size = GUC_WQ_SIZE;
	spin_lock_init(&client->wq_lock);

	client->doorbell_offset = select_doorbell_cacheline(guc);

	/*
	 * Since the doorbell only requires a single cacheline, we can save
	 * space by putting the application process descriptor in the same
	 * page. Use the half of the page that doesn't include the doorbell.
	 */
	if (client->doorbell_offset >= (GUC_DB_SIZE / 2))
		client->proc_desc_offset = 0;
	else
		client->proc_desc_offset = (GUC_DB_SIZE / 2);

	client->doorbell_id = assign_doorbell(guc, client->priority);
	if (client->doorbell_id == GUC_INVALID_DOORBELL_ID)
		/* XXX: evict a doorbell instead */
		goto err;

	guc_init_proc_desc(guc, client);
	guc_init_ctx_desc(guc, client);
	guc_init_doorbell(guc, client);

	/* XXX: Any cache flushes needed? General domain mgmt calls? */

	if (host2guc_allocate_doorbell(guc, client))
		goto err;

	DRM_DEBUG_DRIVER("new priority %u client %p: ctx_index %u db_id %u\n",
		priority, client, client->ctx_index, client->doorbell_id);

	return client;

err:
	DRM_ERROR("FAILED to create priority %u GuC client!\n", priority);

	guc_client_free(dev, client);
	return NULL;
}

static void guc_create_log(struct intel_guc *guc)
{
	struct drm_i915_private *dev_priv = guc_to_i915(guc);
	struct drm_i915_gem_object *obj;
	unsigned long offset;
	uint32_t size, flags;

	if (i915.guc_log_level < GUC_LOG_VERBOSITY_MIN)
		return;

	if (i915.guc_log_level > GUC_LOG_VERBOSITY_MAX)
		i915.guc_log_level = GUC_LOG_VERBOSITY_MAX;

	/* The first page is to save log buffer state. Allocate one
	 * extra page for others in case for overlap */
	size = (1 + GUC_LOG_DPC_PAGES + 1 +
		GUC_LOG_ISR_PAGES + 1 +
		GUC_LOG_CRASH_PAGES + 1) << PAGE_SHIFT;

	obj = guc->log_obj;
	if (!obj) {
		obj = gem_allocate_guc_obj(dev_priv->dev, size);
		if (!obj) {
			/* logging will be off */
			i915.guc_log_level = -1;
			return;
		}

		guc->log_obj = obj;
	}

	/* each allocated unit is a page */
	flags = GUC_LOG_VALID | GUC_LOG_NOTIFY_ON_HALF_FULL |
		(GUC_LOG_DPC_PAGES << GUC_LOG_DPC_SHIFT) |
		(GUC_LOG_ISR_PAGES << GUC_LOG_ISR_SHIFT) |
		(GUC_LOG_CRASH_PAGES << GUC_LOG_CRASH_SHIFT);

	offset = i915_gem_obj_ggtt_offset(obj) >> PAGE_SHIFT; /* in pages */
	guc->log_flags = (offset << GUC_LOG_BUF_ADDR_SHIFT) | flags;
}

/*
 * Set up the memory resources to be shared with the GuC.  At this point,
 * we require just one object that can be mapped through the GGTT.
 */
int i915_guc_submission_init(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	const size_t ctxsize = sizeof(struct guc_context_desc);
	const size_t poolsize = GUC_MAX_GPU_CONTEXTS * ctxsize;
	const size_t gemsize = round_up(poolsize, PAGE_SIZE);
	struct intel_guc *guc = &dev_priv->guc;

	if (!i915.enable_guc_submission)
		return 0; /* not enabled  */

	if (guc->ctx_pool_obj)
		return 0; /* already allocated */

	guc->ctx_pool_obj = gem_allocate_guc_obj(dev_priv->dev, gemsize);
	if (!guc->ctx_pool_obj)
		return -ENOMEM;

	spin_lock_init(&dev_priv->guc.host2guc_lock);

	ida_init(&guc->ctx_ids);

	guc_create_log(guc);

	return 0;
}

int i915_guc_submission_enable(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_guc *guc = &dev_priv->guc;
	struct intel_context *ctx = dev_priv->ring[RCS].default_context;
	struct i915_guc_client *client;

	/* client for execbuf submission */
	client = guc_client_alloc(dev, GUC_CTX_PRIORITY_KMD_NORMAL, ctx);
	if (!client) {
		DRM_ERROR("Failed to create execbuf guc_client\n");
		return -ENOMEM;
	}

	guc->execbuf_client = client;

	host2guc_sample_forcewake(guc, client);

	return 0;
}

void i915_guc_submission_disable(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_guc *guc = &dev_priv->guc;

	guc_client_free(dev, guc->execbuf_client);
	guc->execbuf_client = NULL;
}

void i915_guc_submission_fini(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_guc *guc = &dev_priv->guc;

	gem_release_guc_obj(dev_priv->guc.log_obj);
	guc->log_obj = NULL;

	if (guc->ctx_pool_obj)
		ida_destroy(&guc->ctx_ids);
	gem_release_guc_obj(guc->ctx_pool_obj);
	guc->ctx_pool_obj = NULL;
}

/**
 * intel_guc_suspend() - notify GuC entering suspend state
 * @dev:	drm device
 */
int intel_guc_suspend(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_guc *guc = &dev_priv->guc;
	struct intel_context *ctx;
	u32 data[3];

	if (!i915.enable_guc_submission)
		return 0;

	ctx = dev_priv->ring[RCS].default_context;

	data[0] = HOST2GUC_ACTION_ENTER_S_STATE;
	/* any value greater than GUC_POWER_D0 */
	data[1] = GUC_POWER_D1;
	/* first page is shared data with GuC */
	data[2] = i915_gem_obj_ggtt_offset(ctx->engine[RCS].state);

	return host2guc_action(guc, data, ARRAY_SIZE(data));
}


/**
 * intel_guc_resume() - notify GuC resuming from suspend state
 * @dev:	drm device
 */
int intel_guc_resume(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_guc *guc = &dev_priv->guc;
	struct intel_context *ctx;
	u32 data[3];

	if (!i915.enable_guc_submission)
		return 0;

	ctx = dev_priv->ring[RCS].default_context;

	data[0] = HOST2GUC_ACTION_EXIT_S_STATE;
	data[1] = GUC_POWER_D0;
	/* first page is shared data with GuC */
	data[2] = i915_gem_obj_ggtt_offset(ctx->engine[RCS].state);

	return host2guc_action(guc, data, ARRAY_SIZE(data));
}

#else

int i915_guc_submit(struct i915_guc_client *client,
		    struct drm_i915_gem_request *rq)
{
	panic("%s", __func__);
}

#endif
