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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/**
 * DOC: Frame Buffer Compression (FBC)
 *
 * FBC tries to save memory bandwidth (and so power consumption) by
 * compressing the amount of memory used by the display. It is total
 * transparent to user space and completely handled in the kernel.
 *
 * The benefits of FBC are mostly visible with solid backgrounds and
 * variation-less patterns. It comes from keeping the memory footprint small
 * and having fewer memory pages opened and accessed for refreshing the display.
 *
 * i915 is responsible to reserve stolen memory for FBC and configure its
 * offset on proper registers. The hardware takes care of all
 * compress/decompress. However there are many known cases where we have to
 * forcibly disable it to allow proper screen updates.
 */

#include "intel_drv.h"
#include "i915_drv.h"

static inline bool fbc_supported(struct drm_i915_private *dev_priv)
{
	return dev_priv->fbc.enable_fbc != NULL;
}

/*
 * In some platforms where the CRTC's x:0/y:0 coordinates doesn't match the
 * frontbuffer's x:0/y:0 coordinates we lie to the hardware about the plane's
 * origin so the x and y offsets can actually fit the registers. As a
 * consequence, the fence doesn't really start exactly at the display plane
 * address we program because it starts at the real start of the buffer, so we
 * have to take this into consideration here.
 */
static unsigned int get_crtc_fence_y_offset(struct intel_crtc *crtc)
{
	return crtc->base.y - crtc->adjusted_y;
}

static void i8xx_fbc_disable(struct drm_i915_private *dev_priv)
{
	u32 fbc_ctl;

	dev_priv->fbc.enabled = false;

	/* Disable compression */
	fbc_ctl = I915_READ(FBC_CONTROL);
	if ((fbc_ctl & FBC_CTL_EN) == 0)
		return;

	fbc_ctl &= ~FBC_CTL_EN;
	I915_WRITE(FBC_CONTROL, fbc_ctl);

	/* Wait for compressing bit to clear */
	if (wait_for((I915_READ(FBC_STATUS) & FBC_STAT_COMPRESSING) == 0, 10)) {
		DRM_DEBUG_KMS("FBC idle timed out\n");
		return;
	}

	DRM_DEBUG_KMS("disabled FBC\n");
}

static void i8xx_fbc_enable(struct intel_crtc *crtc)
{
	struct drm_i915_private *dev_priv = crtc->base.dev->dev_private;
	struct drm_framebuffer *fb = crtc->base.primary->fb;
	struct drm_i915_gem_object *obj = intel_fb_obj(fb);
	int cfb_pitch;
	int i;
	u32 fbc_ctl;

	dev_priv->fbc.enabled = true;

	/* Note: fbc.threshold == 1 for i8xx */
	cfb_pitch = dev_priv->fbc.uncompressed_size / FBC_LL_SIZE;
	if (fb->pitches[0] < cfb_pitch)
		cfb_pitch = fb->pitches[0];

	/* FBC_CTL wants 32B or 64B units */
	if (IS_GEN2(dev_priv))
		cfb_pitch = (cfb_pitch / 32) - 1;
	else
		cfb_pitch = (cfb_pitch / 64) - 1;

	/* Clear old tags */
	for (i = 0; i < (FBC_LL_SIZE / 32) + 1; i++)
		I915_WRITE(FBC_TAG(i), 0);

	if (IS_GEN4(dev_priv)) {
		u32 fbc_ctl2;

		/* Set it up... */
		fbc_ctl2 = FBC_CTL_FENCE_DBL | FBC_CTL_IDLE_IMM | FBC_CTL_CPU_FENCE;
		fbc_ctl2 |= FBC_CTL_PLANE(crtc->plane);
		I915_WRITE(FBC_CONTROL2, fbc_ctl2);
		I915_WRITE(FBC_FENCE_OFF, get_crtc_fence_y_offset(crtc));
	}

	/* enable it... */
	fbc_ctl = I915_READ(FBC_CONTROL);
	fbc_ctl &= 0x3fff << FBC_CTL_INTERVAL_SHIFT;
	fbc_ctl |= FBC_CTL_EN | FBC_CTL_PERIODIC;
	if (IS_I945GM(dev_priv))
		fbc_ctl |= FBC_CTL_C3_IDLE; /* 945 needs special SR handling */
	fbc_ctl |= (cfb_pitch & 0xff) << FBC_CTL_STRIDE_SHIFT;
	fbc_ctl |= obj->fence_reg;
	I915_WRITE(FBC_CONTROL, fbc_ctl);

	DRM_DEBUG_KMS("enabled FBC, pitch %d, yoff %d, plane %c\n",
		      cfb_pitch, crtc->base.y, plane_name(crtc->plane));
}

static bool i8xx_fbc_enabled(struct drm_i915_private *dev_priv)
{
	return I915_READ(FBC_CONTROL) & FBC_CTL_EN;
}

static void g4x_fbc_enable(struct intel_crtc *crtc)
{
	struct drm_i915_private *dev_priv = crtc->base.dev->dev_private;
	struct drm_framebuffer *fb = crtc->base.primary->fb;
	struct drm_i915_gem_object *obj = intel_fb_obj(fb);
	u32 dpfc_ctl;

	dev_priv->fbc.enabled = true;

	dpfc_ctl = DPFC_CTL_PLANE(crtc->plane) | DPFC_SR_EN;
	if (drm_format_plane_cpp(fb->pixel_format, 0) == 2)
		dpfc_ctl |= DPFC_CTL_LIMIT_2X;
	else
		dpfc_ctl |= DPFC_CTL_LIMIT_1X;
	dpfc_ctl |= DPFC_CTL_FENCE_EN | obj->fence_reg;

	I915_WRITE(DPFC_FENCE_YOFF, get_crtc_fence_y_offset(crtc));

	/* enable it... */
	I915_WRITE(DPFC_CONTROL, dpfc_ctl | DPFC_CTL_EN);

	DRM_DEBUG_KMS("enabled fbc on plane %c\n", plane_name(crtc->plane));
}

static void g4x_fbc_disable(struct drm_i915_private *dev_priv)
{
	u32 dpfc_ctl;

	dev_priv->fbc.enabled = false;

	/* Disable compression */
	dpfc_ctl = I915_READ(DPFC_CONTROL);
	if (dpfc_ctl & DPFC_CTL_EN) {
		dpfc_ctl &= ~DPFC_CTL_EN;
		I915_WRITE(DPFC_CONTROL, dpfc_ctl);

		DRM_DEBUG_KMS("disabled FBC\n");
	}
}

static bool g4x_fbc_enabled(struct drm_i915_private *dev_priv)
{
	return I915_READ(DPFC_CONTROL) & DPFC_CTL_EN;
}

static void intel_fbc_nuke(struct drm_i915_private *dev_priv)
{
	I915_WRITE(MSG_FBC_REND_STATE, FBC_REND_NUKE);
	POSTING_READ(MSG_FBC_REND_STATE);
}

static void ilk_fbc_enable(struct intel_crtc *crtc)
{
	struct drm_i915_private *dev_priv = crtc->base.dev->dev_private;
	struct drm_framebuffer *fb = crtc->base.primary->fb;
	struct drm_i915_gem_object *obj = intel_fb_obj(fb);
	u32 dpfc_ctl;
	int threshold = dev_priv->fbc.threshold;
	unsigned int y_offset;

	dev_priv->fbc.enabled = true;

	dpfc_ctl = DPFC_CTL_PLANE(crtc->plane);
	if (drm_format_plane_cpp(fb->pixel_format, 0) == 2)
		threshold++;

	switch (threshold) {
	case 4:
	case 3:
		dpfc_ctl |= DPFC_CTL_LIMIT_4X;
		break;
	case 2:
		dpfc_ctl |= DPFC_CTL_LIMIT_2X;
		break;
	case 1:
		dpfc_ctl |= DPFC_CTL_LIMIT_1X;
		break;
	}
	dpfc_ctl |= DPFC_CTL_FENCE_EN;
	if (IS_GEN5(dev_priv))
		dpfc_ctl |= obj->fence_reg;

	y_offset = get_crtc_fence_y_offset(crtc);
	I915_WRITE(ILK_DPFC_FENCE_YOFF, y_offset);
	I915_WRITE(ILK_FBC_RT_BASE, i915_gem_obj_ggtt_offset(obj) | ILK_FBC_RT_VALID);
	/* enable it... */
	I915_WRITE(ILK_DPFC_CONTROL, dpfc_ctl | DPFC_CTL_EN);

	if (IS_GEN6(dev_priv)) {
		I915_WRITE(SNB_DPFC_CTL_SA,
			   SNB_CPU_FENCE_ENABLE | obj->fence_reg);
		I915_WRITE(DPFC_CPU_FENCE_OFFSET, y_offset);
	}

	intel_fbc_nuke(dev_priv);

	DRM_DEBUG_KMS("enabled fbc on plane %c\n", plane_name(crtc->plane));
}

static void ilk_fbc_disable(struct drm_i915_private *dev_priv)
{
	u32 dpfc_ctl;

	dev_priv->fbc.enabled = false;

	/* Disable compression */
	dpfc_ctl = I915_READ(ILK_DPFC_CONTROL);
	if (dpfc_ctl & DPFC_CTL_EN) {
		dpfc_ctl &= ~DPFC_CTL_EN;
		I915_WRITE(ILK_DPFC_CONTROL, dpfc_ctl);

		DRM_DEBUG_KMS("disabled FBC\n");
	}
}

static bool ilk_fbc_enabled(struct drm_i915_private *dev_priv)
{
	return I915_READ(ILK_DPFC_CONTROL) & DPFC_CTL_EN;
}

static void gen7_fbc_enable(struct intel_crtc *crtc)
{
	struct drm_i915_private *dev_priv = crtc->base.dev->dev_private;
	struct drm_framebuffer *fb = crtc->base.primary->fb;
	struct drm_i915_gem_object *obj = intel_fb_obj(fb);
	u32 dpfc_ctl;
	int threshold = dev_priv->fbc.threshold;

	dev_priv->fbc.enabled = true;

	dpfc_ctl = 0;
	if (IS_IVYBRIDGE(dev_priv))
		dpfc_ctl |= IVB_DPFC_CTL_PLANE(crtc->plane);

	if (drm_format_plane_cpp(fb->pixel_format, 0) == 2)
		threshold++;

	switch (threshold) {
	case 4:
	case 3:
		dpfc_ctl |= DPFC_CTL_LIMIT_4X;
		break;
	case 2:
		dpfc_ctl |= DPFC_CTL_LIMIT_2X;
		break;
	case 1:
		dpfc_ctl |= DPFC_CTL_LIMIT_1X;
		break;
	}

	dpfc_ctl |= IVB_DPFC_CTL_FENCE_EN;

	if (dev_priv->fbc.false_color)
		dpfc_ctl |= FBC_CTL_FALSE_COLOR;

	if (IS_IVYBRIDGE(dev_priv)) {
		/* WaFbcAsynchFlipDisableFbcQueue:ivb */
		I915_WRITE(ILK_DISPLAY_CHICKEN1,
			   I915_READ(ILK_DISPLAY_CHICKEN1) |
			   ILK_FBCQ_DIS);
	} else if (IS_HASWELL(dev_priv) || IS_BROADWELL(dev_priv)) {
		/* WaFbcAsynchFlipDisableFbcQueue:hsw,bdw */
		I915_WRITE(CHICKEN_PIPESL_1(crtc->pipe),
			   I915_READ(CHICKEN_PIPESL_1(crtc->pipe)) |
			   HSW_FBCQ_DIS);
	}

	I915_WRITE(ILK_DPFC_CONTROL, dpfc_ctl | DPFC_CTL_EN);

	I915_WRITE(SNB_DPFC_CTL_SA,
		   SNB_CPU_FENCE_ENABLE | obj->fence_reg);
	I915_WRITE(DPFC_CPU_FENCE_OFFSET, get_crtc_fence_y_offset(crtc));

	intel_fbc_nuke(dev_priv);

	DRM_DEBUG_KMS("enabled fbc on plane %c\n", plane_name(crtc->plane));
}

/**
 * intel_fbc_enabled - Is FBC enabled?
 * @dev_priv: i915 device instance
 *
 * This function is used to verify the current state of FBC.
 * FIXME: This should be tracked in the plane config eventually
 *        instead of queried at runtime for most callers.
 */
bool intel_fbc_enabled(struct drm_i915_private *dev_priv)
{
	return dev_priv->fbc.enabled;
}

static void intel_fbc_enable(struct intel_crtc *crtc,
			     const struct drm_framebuffer *fb)
{
	struct drm_i915_private *dev_priv = crtc->base.dev->dev_private;

	dev_priv->fbc.enable_fbc(crtc);

	dev_priv->fbc.crtc = crtc;
	dev_priv->fbc.fb_id = fb->base.id;
	dev_priv->fbc.y = crtc->base.y;
}

static void intel_fbc_work_fn(struct work_struct *__work)
{
	struct intel_fbc_work *work =
		container_of(to_delayed_work(__work),
			     struct intel_fbc_work, work);
	struct drm_i915_private *dev_priv = work->crtc->base.dev->dev_private;
	struct drm_framebuffer *crtc_fb = work->crtc->base.primary->fb;

	mutex_lock(&dev_priv->fbc.lock);
	if (work == dev_priv->fbc.fbc_work) {
		/* Double check that we haven't switched fb without cancelling
		 * the prior work.
		 */
		if (crtc_fb == work->fb)
			intel_fbc_enable(work->crtc, work->fb);

		dev_priv->fbc.fbc_work = NULL;
	}
	mutex_unlock(&dev_priv->fbc.lock);

	kfree(work);
}

static void intel_fbc_cancel_work(struct drm_i915_private *dev_priv)
{
	WARN_ON(!mutex_is_locked(&dev_priv->fbc.lock));

	if (dev_priv->fbc.fbc_work == NULL)
		return;

	DRM_DEBUG_KMS("cancelling pending FBC enable\n");

	/* Synchronisation is provided by struct_mutex and checking of
	 * dev_priv->fbc.fbc_work, so we can perform the cancellation
	 * entirely asynchronously.
	 */
	if (cancel_delayed_work(&dev_priv->fbc.fbc_work->work))
		/* tasklet was killed before being run, clean up */
		kfree(dev_priv->fbc.fbc_work);

	/* Mark the work as no longer wanted so that if it does
	 * wake-up (because the work was already running and waiting
	 * for our mutex), it will discover that is no longer
	 * necessary to run.
	 */
	dev_priv->fbc.fbc_work = NULL;
}

static void intel_fbc_schedule_enable(struct intel_crtc *crtc)
{
	struct intel_fbc_work *work;
	struct drm_i915_private *dev_priv = crtc->base.dev->dev_private;

	WARN_ON(!mutex_is_locked(&dev_priv->fbc.lock));

	intel_fbc_cancel_work(dev_priv);

	work = kzalloc(sizeof(*work), GFP_KERNEL);
	if (work == NULL) {
		DRM_ERROR("Failed to allocate FBC work structure\n");
		intel_fbc_enable(crtc, crtc->base.primary->fb);
		return;
	}

	work->crtc = crtc;
	work->fb = crtc->base.primary->fb;
	INIT_DELAYED_WORK(&work->work, intel_fbc_work_fn);

	dev_priv->fbc.fbc_work = work;

	/* Delay the actual enabling to let pageflipping cease and the
	 * display to settle before starting the compression. Note that
	 * this delay also serves a second purpose: it allows for a
	 * vblank to pass after disabling the FBC before we attempt
	 * to modify the control registers.
	 *
	 * A more complicated solution would involve tracking vblanks
	 * following the termination of the page-flipping sequence
	 * and indeed performing the enable as a co-routine and not
	 * waiting synchronously upon the vblank.
	 *
	 * WaFbcWaitForVBlankBeforeEnable:ilk,snb
	 */
	schedule_delayed_work(&work->work, msecs_to_jiffies(50));
}

static void __intel_fbc_disable(struct drm_i915_private *dev_priv)
{
	WARN_ON(!mutex_is_locked(&dev_priv->fbc.lock));

	intel_fbc_cancel_work(dev_priv);

	dev_priv->fbc.disable_fbc(dev_priv);
	dev_priv->fbc.crtc = NULL;
}

/**
 * intel_fbc_disable - disable FBC
 * @dev_priv: i915 device instance
 *
 * This function disables FBC.
 */
void intel_fbc_disable(struct drm_i915_private *dev_priv)
{
	if (!fbc_supported(dev_priv))
		return;

	mutex_lock(&dev_priv->fbc.lock);
	__intel_fbc_disable(dev_priv);
	mutex_unlock(&dev_priv->fbc.lock);
}

/*
 * intel_fbc_disable_crtc - disable FBC if it's associated with crtc
 * @crtc: the CRTC
 *
 * This function disables FBC if it's associated with the provided CRTC.
 */
void intel_fbc_disable_crtc(struct intel_crtc *crtc)
{
	struct drm_i915_private *dev_priv = crtc->base.dev->dev_private;

	if (!fbc_supported(dev_priv))
		return;

	mutex_lock(&dev_priv->fbc.lock);
	if (dev_priv->fbc.crtc == crtc)
		__intel_fbc_disable(dev_priv);
	mutex_unlock(&dev_priv->fbc.lock);
}

const char *intel_no_fbc_reason_str(enum no_fbc_reason reason)
{
	switch (reason) {
	case FBC_OK:
		return "FBC enabled but currently disabled in hardware";
	case FBC_UNSUPPORTED:
		return "unsupported by this chipset";
	case FBC_NO_OUTPUT:
		return "no output";
	case FBC_STOLEN_TOO_SMALL:
		return "not enough stolen memory";
	case FBC_UNSUPPORTED_MODE:
		return "mode incompatible with compression";
	case FBC_MODE_TOO_LARGE:
		return "mode too large for compression";
	case FBC_BAD_PLANE:
		return "FBC unsupported on plane";
	case FBC_NOT_TILED:
		return "framebuffer not tiled or fenced";
	case FBC_MULTIPLE_PIPES:
		return "more than one pipe active";
	case FBC_MODULE_PARAM:
		return "disabled per module param";
	case FBC_CHIP_DEFAULT:
		return "disabled per chip default";
	case FBC_ROTATION:
		return "rotation unsupported";
	case FBC_IN_DBG_MASTER:
		return "Kernel debugger is active";
	case FBC_BAD_STRIDE:
		return "framebuffer stride not supported";
	case FBC_PIXEL_RATE:
		return "pixel rate is too big";
	case FBC_PIXEL_FORMAT:
		return "pixel format is invalid";
	default:
		MISSING_CASE(reason);
		return "unknown reason";
	}
}

static void set_no_fbc_reason(struct drm_i915_private *dev_priv,
			      enum no_fbc_reason reason)
{
	if (dev_priv->fbc.no_fbc_reason == reason)
		return;

	dev_priv->fbc.no_fbc_reason = reason;
	DRM_DEBUG_KMS("Disabling FBC: %s\n", intel_no_fbc_reason_str(reason));
}

static struct drm_crtc *intel_fbc_find_crtc(struct drm_i915_private *dev_priv)
{
	struct drm_crtc *crtc = NULL, *tmp_crtc;
	enum pipe pipe;
	bool pipe_a_only = false;

	if (IS_HASWELL(dev_priv) || INTEL_INFO(dev_priv)->gen >= 8)
		pipe_a_only = true;

	for_each_pipe(dev_priv, pipe) {
		tmp_crtc = dev_priv->pipe_to_crtc_mapping[pipe];

		if (intel_crtc_active(tmp_crtc) &&
		    to_intel_plane_state(tmp_crtc->primary->state)->visible)
			crtc = tmp_crtc;

		if (pipe_a_only)
			break;
	}

	if (!crtc || crtc->primary->fb == NULL)
		return NULL;

	return crtc;
}

static bool multiple_pipes_ok(struct drm_i915_private *dev_priv)
{
	enum pipe pipe;
	int n_pipes = 0;
	struct drm_crtc *crtc;

	if (INTEL_INFO(dev_priv)->gen > 4)
		return true;

	for_each_pipe(dev_priv, pipe) {
		crtc = dev_priv->pipe_to_crtc_mapping[pipe];

		if (intel_crtc_active(crtc) &&
		    to_intel_plane_state(crtc->primary->state)->visible)
			n_pipes++;
	}

	return (n_pipes < 2);
}

static int find_compression_threshold(struct drm_i915_private *dev_priv,
				      struct drm_mm_node *node,
				      int size,
				      int fb_cpp)
{
	int compression_threshold = 1;
	int ret;
	u64 end;

	/* The FBC hardware for BDW/SKL doesn't have access to the stolen
	 * reserved range size, so it always assumes the maximum (8mb) is used.
	 * If we enable FBC using a CFB on that memory range we'll get FIFO
	 * underruns, even if that range is not reserved by the BIOS. */
	if (IS_BROADWELL(dev_priv) ||
	    IS_SKYLAKE(dev_priv) || IS_KABYLAKE(dev_priv))
		end = dev_priv->gtt.stolen_size - 8 * 1024 * 1024;
	else
		end = dev_priv->gtt.stolen_usable_size;

	/* HACK: This code depends on what we will do in *_enable_fbc. If that
	 * code changes, this code needs to change as well.
	 *
	 * The enable_fbc code will attempt to use one of our 2 compression
	 * thresholds, therefore, in that case, we only have 1 resort.
	 */

	/* Try to over-allocate to reduce reallocations and fragmentation. */
	ret = i915_gem_stolen_insert_node_in_range(dev_priv, node, size <<= 1,
						   4096, 0, end);
	if (ret == 0)
		return compression_threshold;

again:
	/* HW's ability to limit the CFB is 1:4 */
	if (compression_threshold > 4 ||
	    (fb_cpp == 2 && compression_threshold == 2))
		return 0;

	ret = i915_gem_stolen_insert_node_in_range(dev_priv, node, size >>= 1,
						   4096, 0, end);
	if (ret && INTEL_INFO(dev_priv)->gen <= 4) {
		return 0;
	} else if (ret) {
		compression_threshold <<= 1;
		goto again;
	} else {
		return compression_threshold;
	}
}

static int intel_fbc_alloc_cfb(struct drm_i915_private *dev_priv, int size,
			       int fb_cpp)
{
	struct drm_mm_node *uninitialized_var(compressed_llb);
	int ret;

	ret = find_compression_threshold(dev_priv, &dev_priv->fbc.compressed_fb,
					 size, fb_cpp);
	if (!ret)
		goto err_llb;
	else if (ret > 1) {
		DRM_INFO("Reducing the compressed framebuffer size. This may lead to less power savings than a non-reduced-size. Try to increase stolen memory size if available in BIOS.\n");

	}

	dev_priv->fbc.threshold = ret;

	if (INTEL_INFO(dev_priv)->gen >= 5)
		I915_WRITE(ILK_DPFC_CB_BASE, dev_priv->fbc.compressed_fb.start);
	else if (IS_GM45(dev_priv)) {
		I915_WRITE(DPFC_CB_BASE, dev_priv->fbc.compressed_fb.start);
	} else {
		compressed_llb = kzalloc(sizeof(*compressed_llb), GFP_KERNEL);
		if (!compressed_llb)
			goto err_fb;

		ret = i915_gem_stolen_insert_node(dev_priv, compressed_llb,
						  4096, 4096);
		if (ret)
			goto err_fb;

		dev_priv->fbc.compressed_llb = compressed_llb;

		I915_WRITE(FBC_CFB_BASE,
			   dev_priv->mm.stolen_base + dev_priv->fbc.compressed_fb.start);
		I915_WRITE(FBC_LL_BASE,
			   dev_priv->mm.stolen_base + compressed_llb->start);
	}

	dev_priv->fbc.uncompressed_size = size;

	DRM_DEBUG_KMS("reserved %llu bytes of contiguous stolen space for FBC, threshold: %d\n",
		      dev_priv->fbc.compressed_fb.size,
		      dev_priv->fbc.threshold);

	return 0;

err_fb:
	kfree(compressed_llb);
	i915_gem_stolen_remove_node(dev_priv, &dev_priv->fbc.compressed_fb);
err_llb:
	pr_info_once("drm: not enough stolen space for compressed buffer (need %d more bytes), disabling. Hint: you may be able to increase stolen memory size in the BIOS to avoid this.\n", size);
	return -ENOSPC;
}

static void __intel_fbc_cleanup_cfb(struct drm_i915_private *dev_priv)
{
	if (dev_priv->fbc.uncompressed_size == 0)
		return;

	i915_gem_stolen_remove_node(dev_priv, &dev_priv->fbc.compressed_fb);

	if (dev_priv->fbc.compressed_llb) {
		i915_gem_stolen_remove_node(dev_priv,
					    dev_priv->fbc.compressed_llb);
		kfree(dev_priv->fbc.compressed_llb);
	}

	dev_priv->fbc.uncompressed_size = 0;
}

void intel_fbc_cleanup_cfb(struct drm_i915_private *dev_priv)
{
	if (!fbc_supported(dev_priv))
		return;

	mutex_lock(&dev_priv->fbc.lock);
	__intel_fbc_cleanup_cfb(dev_priv);
	mutex_unlock(&dev_priv->fbc.lock);
}

/*
 * For SKL+, the plane source size used by the hardware is based on the value we
 * write to the PLANE_SIZE register. For BDW-, the hardware looks at the value
 * we wrote to PIPESRC.
 */
static void intel_fbc_get_plane_source_size(struct intel_crtc *crtc,
					    int *width, int *height)
{
	struct intel_plane_state *plane_state =
			to_intel_plane_state(crtc->base.primary->state);
	int w, h;

	if (intel_rotation_90_or_270(plane_state->base.rotation)) {
		w = drm_rect_height(&plane_state->src) >> 16;
		h = drm_rect_width(&plane_state->src) >> 16;
	} else {
		w = drm_rect_width(&plane_state->src) >> 16;
		h = drm_rect_height(&plane_state->src) >> 16;
	}

	if (width)
		*width = w;
	if (height)
		*height = h;
}

static int intel_fbc_calculate_cfb_size(struct intel_crtc *crtc)
{
	struct drm_i915_private *dev_priv = crtc->base.dev->dev_private;
	struct drm_framebuffer *fb = crtc->base.primary->fb;
	int lines;

	intel_fbc_get_plane_source_size(crtc, NULL, &lines);
	if (INTEL_INFO(dev_priv)->gen >= 7)
		lines = min(lines, 2048);

	return lines * fb->pitches[0];
}

static int intel_fbc_setup_cfb(struct intel_crtc *crtc)
{
	struct drm_i915_private *dev_priv = crtc->base.dev->dev_private;
	struct drm_framebuffer *fb = crtc->base.primary->fb;
	int size, cpp;

	size = intel_fbc_calculate_cfb_size(crtc);
	cpp = drm_format_plane_cpp(fb->pixel_format, 0);

	if (size <= dev_priv->fbc.uncompressed_size)
		return 0;

	/* Release any current block */
	__intel_fbc_cleanup_cfb(dev_priv);

	return intel_fbc_alloc_cfb(dev_priv, size, cpp);
}

static bool stride_is_valid(struct drm_i915_private *dev_priv,
			    unsigned int stride)
{
	/* These should have been caught earlier. */
	WARN_ON(stride < 512);
	WARN_ON((stride & (64 - 1)) != 0);

	/* Below are the additional FBC restrictions. */

	if (IS_GEN2(dev_priv) || IS_GEN3(dev_priv))
		return stride == 4096 || stride == 8192;

	if (IS_GEN4(dev_priv) && !IS_G4X(dev_priv) && stride < 2048)
		return false;

	if (stride > 16384)
		return false;

	return true;
}

static bool pixel_format_is_valid(struct drm_framebuffer *fb)
{
	struct drm_device *dev = fb->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;

	switch (fb->pixel_format) {
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_XBGR8888:
		return true;
	case DRM_FORMAT_XRGB1555:
	case DRM_FORMAT_RGB565:
		/* 16bpp not supported on gen2 */
		if (IS_GEN2(dev))
			return false;
		/* WaFbcOnly1to1Ratio:ctg */
		if (IS_G4X(dev_priv))
			return false;
		return true;
	default:
		return false;
	}
}

/*
 * For some reason, the hardware tracking starts looking at whatever we
 * programmed as the display plane base address register. It does not look at
 * the X and Y offset registers. That's why we look at the crtc->adjusted{x,y}
 * variables instead of just looking at the pipe/plane size.
 */
static bool intel_fbc_hw_tracking_covers_screen(struct intel_crtc *crtc)
{
	struct drm_i915_private *dev_priv = crtc->base.dev->dev_private;
	unsigned int effective_w, effective_h, max_w, max_h;

	if (INTEL_INFO(dev_priv)->gen >= 8 || IS_HASWELL(dev_priv)) {
		max_w = 4096;
		max_h = 4096;
	} else if (IS_G4X(dev_priv) || INTEL_INFO(dev_priv)->gen >= 5) {
		max_w = 4096;
		max_h = 2048;
	} else {
		max_w = 2048;
		max_h = 1536;
	}

	intel_fbc_get_plane_source_size(crtc, &effective_w, &effective_h);
	effective_w += crtc->adjusted_x;
	effective_h += crtc->adjusted_y;

	return effective_w <= max_w && effective_h <= max_h;
}

/**
 * __intel_fbc_update - enable/disable FBC as needed, unlocked
 * @dev_priv: i915 device instance
 *
 * Set up the framebuffer compression hardware at mode set time.  We
 * enable it if possible:
 *   - plane A only (on pre-965)
 *   - no pixel mulitply/line duplication
 *   - no alpha buffer discard
 *   - no dual wide
 *   - framebuffer <= max_hdisplay in width, max_vdisplay in height
 *
 * We can't assume that any compression will take place (worst case),
 * so the compressed buffer has to be the same size as the uncompressed
 * one.  It also must reside (along with the line length buffer) in
 * stolen memory.
 *
 * We need to enable/disable FBC on a global basis.
 */
static void __intel_fbc_update(struct drm_i915_private *dev_priv)
{
	struct drm_crtc *crtc = NULL;
	struct intel_crtc *intel_crtc;
	struct drm_framebuffer *fb;
	struct drm_i915_gem_object *obj;
	const struct drm_display_mode *adjusted_mode;

	WARN_ON(!mutex_is_locked(&dev_priv->fbc.lock));

	/* disable framebuffer compression in vGPU */
	if (intel_vgpu_active(dev_priv->dev))
		i915.enable_fbc = 0;

	if (i915.enable_fbc < 0) {
		set_no_fbc_reason(dev_priv, FBC_CHIP_DEFAULT);
		goto out_disable;
	}

	if (!i915.enable_fbc) {
		set_no_fbc_reason(dev_priv, FBC_MODULE_PARAM);
		goto out_disable;
	}

	/*
	 * If FBC is already on, we just have to verify that we can
	 * keep it that way...
	 * Need to disable if:
	 *   - more than one pipe is active
	 *   - changing FBC params (stride, fence, mode)
	 *   - new fb is too large to fit in compressed buffer
	 *   - going to an unsupported config (interlace, pixel multiply, etc.)
	 */
	crtc = intel_fbc_find_crtc(dev_priv);
	if (!crtc) {
		set_no_fbc_reason(dev_priv, FBC_NO_OUTPUT);
		goto out_disable;
	}

	if (!multiple_pipes_ok(dev_priv)) {
		set_no_fbc_reason(dev_priv, FBC_MULTIPLE_PIPES);
		goto out_disable;
	}

	intel_crtc = to_intel_crtc(crtc);
	fb = crtc->primary->fb;
	obj = intel_fb_obj(fb);
	adjusted_mode = &intel_crtc->config->base.adjusted_mode;

	if ((adjusted_mode->flags & DRM_MODE_FLAG_INTERLACE) ||
	    (adjusted_mode->flags & DRM_MODE_FLAG_DBLSCAN)) {
		set_no_fbc_reason(dev_priv, FBC_UNSUPPORTED_MODE);
		goto out_disable;
	}

	if (!intel_fbc_hw_tracking_covers_screen(intel_crtc)) {
		set_no_fbc_reason(dev_priv, FBC_MODE_TOO_LARGE);
		goto out_disable;
	}

	if ((INTEL_INFO(dev_priv)->gen < 4 || HAS_DDI(dev_priv)) &&
	    intel_crtc->plane != PLANE_A) {
		set_no_fbc_reason(dev_priv, FBC_BAD_PLANE);
		goto out_disable;
	}

	/* The use of a CPU fence is mandatory in order to detect writes
	 * by the CPU to the scanout and trigger updates to the FBC.
	 */
	if (obj->tiling_mode != I915_TILING_X ||
	    obj->fence_reg == I915_FENCE_REG_NONE) {
		set_no_fbc_reason(dev_priv, FBC_NOT_TILED);
		goto out_disable;
	}
	if (INTEL_INFO(dev_priv)->gen <= 4 && !IS_G4X(dev_priv) &&
	    crtc->primary->state->rotation != BIT(DRM_ROTATE_0)) {
		set_no_fbc_reason(dev_priv, FBC_ROTATION);
		goto out_disable;
	}

	if (!stride_is_valid(dev_priv, fb->pitches[0])) {
		set_no_fbc_reason(dev_priv, FBC_BAD_STRIDE);
		goto out_disable;
	}

	if (!pixel_format_is_valid(fb)) {
		set_no_fbc_reason(dev_priv, FBC_PIXEL_FORMAT);
		goto out_disable;
	}

	/* If the kernel debugger is active, always disable compression */
	if (in_dbg_master()) {
		set_no_fbc_reason(dev_priv, FBC_IN_DBG_MASTER);
		goto out_disable;
	}

	/* WaFbcExceedCdClockThreshold:hsw,bdw */
	if ((IS_HASWELL(dev_priv) || IS_BROADWELL(dev_priv)) &&
	    ilk_pipe_pixel_rate(intel_crtc->config) >=
	    dev_priv->cdclk_freq * 95 / 100) {
		set_no_fbc_reason(dev_priv, FBC_PIXEL_RATE);
		goto out_disable;
	}

	if (intel_fbc_setup_cfb(intel_crtc)) {
		set_no_fbc_reason(dev_priv, FBC_STOLEN_TOO_SMALL);
		goto out_disable;
	}

	/* If the scanout has not changed, don't modify the FBC settings.
	 * Note that we make the fundamental assumption that the fb->obj
	 * cannot be unpinned (and have its GTT offset and fence revoked)
	 * without first being decoupled from the scanout and FBC disabled.
	 */
	if (dev_priv->fbc.crtc == intel_crtc &&
	    dev_priv->fbc.fb_id == fb->base.id &&
	    dev_priv->fbc.y == crtc->y)
		return;

	if (intel_fbc_enabled(dev_priv)) {
		/* We update FBC along two paths, after changing fb/crtc
		 * configuration (modeswitching) and after page-flipping
		 * finishes. For the latter, we know that not only did
		 * we disable the FBC at the start of the page-flip
		 * sequence, but also more than one vblank has passed.
		 *
		 * For the former case of modeswitching, it is possible
		 * to switch between two FBC valid configurations
		 * instantaneously so we do need to disable the FBC
		 * before we can modify its control registers. We also
		 * have to wait for the next vblank for that to take
		 * effect. However, since we delay enabling FBC we can
		 * assume that a vblank has passed since disabling and
		 * that we can safely alter the registers in the deferred
		 * callback.
		 *
		 * In the scenario that we go from a valid to invalid
		 * and then back to valid FBC configuration we have
		 * no strict enforcement that a vblank occurred since
		 * disabling the FBC. However, along all current pipe
		 * disabling paths we do need to wait for a vblank at
		 * some point. And we wait before enabling FBC anyway.
		 */
		DRM_DEBUG_KMS("disabling active FBC for update\n");
		__intel_fbc_disable(dev_priv);
	}

	intel_fbc_schedule_enable(intel_crtc);
	dev_priv->fbc.no_fbc_reason = FBC_OK;
	return;

out_disable:
	/* Multiple disables should be harmless */
	if (intel_fbc_enabled(dev_priv)) {
		DRM_DEBUG_KMS("unsupported config, disabling FBC\n");
		__intel_fbc_disable(dev_priv);
	}
	__intel_fbc_cleanup_cfb(dev_priv);
}

/*
 * intel_fbc_update - enable/disable FBC as needed
 * @dev_priv: i915 device instance
 *
 * This function reevaluates the overall state and enables or disables FBC.
 */
void intel_fbc_update(struct drm_i915_private *dev_priv)
{
	if (!fbc_supported(dev_priv))
		return;

	mutex_lock(&dev_priv->fbc.lock);
	__intel_fbc_update(dev_priv);
	mutex_unlock(&dev_priv->fbc.lock);
}

void intel_fbc_invalidate(struct drm_i915_private *dev_priv,
			  unsigned int frontbuffer_bits,
			  enum fb_op_origin origin)
{
	unsigned int fbc_bits;

	if (!fbc_supported(dev_priv))
		return;

	if (origin == ORIGIN_GTT)
		return;

	mutex_lock(&dev_priv->fbc.lock);

	if (dev_priv->fbc.enabled)
		fbc_bits = INTEL_FRONTBUFFER_PRIMARY(dev_priv->fbc.crtc->pipe);
	else if (dev_priv->fbc.fbc_work)
		fbc_bits = INTEL_FRONTBUFFER_PRIMARY(
					dev_priv->fbc.fbc_work->crtc->pipe);
	else
		fbc_bits = dev_priv->fbc.possible_framebuffer_bits;

	dev_priv->fbc.busy_bits |= (fbc_bits & frontbuffer_bits);

	if (dev_priv->fbc.busy_bits)
		__intel_fbc_disable(dev_priv);

	mutex_unlock(&dev_priv->fbc.lock);
}

void intel_fbc_flush(struct drm_i915_private *dev_priv,
		     unsigned int frontbuffer_bits, enum fb_op_origin origin)
{
	if (!fbc_supported(dev_priv))
		return;

	if (origin == ORIGIN_GTT)
		return;

	mutex_lock(&dev_priv->fbc.lock);

	dev_priv->fbc.busy_bits &= ~frontbuffer_bits;

	if (!dev_priv->fbc.busy_bits) {
		__intel_fbc_disable(dev_priv);
		__intel_fbc_update(dev_priv);
	}

	mutex_unlock(&dev_priv->fbc.lock);
}

/**
 * intel_fbc_init - Initialize FBC
 * @dev_priv: the i915 device
 *
 * This function might be called during PM init process.
 */
void intel_fbc_init(struct drm_i915_private *dev_priv)
{
	enum pipe pipe;

	rw_init(&dev_priv->fbc.lock, "fbclk");

	if (!HAS_FBC(dev_priv)) {
		dev_priv->fbc.enabled = false;
		dev_priv->fbc.no_fbc_reason = FBC_UNSUPPORTED;
		return;
	}

	for_each_pipe(dev_priv, pipe) {
		dev_priv->fbc.possible_framebuffer_bits |=
				INTEL_FRONTBUFFER_PRIMARY(pipe);

		if (IS_HASWELL(dev_priv) || INTEL_INFO(dev_priv)->gen >= 8)
			break;
	}

	if (INTEL_INFO(dev_priv)->gen >= 7) {
		dev_priv->fbc.fbc_enabled = ilk_fbc_enabled;
		dev_priv->fbc.enable_fbc = gen7_fbc_enable;
		dev_priv->fbc.disable_fbc = ilk_fbc_disable;
	} else if (INTEL_INFO(dev_priv)->gen >= 5) {
		dev_priv->fbc.fbc_enabled = ilk_fbc_enabled;
		dev_priv->fbc.enable_fbc = ilk_fbc_enable;
		dev_priv->fbc.disable_fbc = ilk_fbc_disable;
	} else if (IS_GM45(dev_priv)) {
		dev_priv->fbc.fbc_enabled = g4x_fbc_enabled;
		dev_priv->fbc.enable_fbc = g4x_fbc_enable;
		dev_priv->fbc.disable_fbc = g4x_fbc_disable;
	} else {
		dev_priv->fbc.fbc_enabled = i8xx_fbc_enabled;
		dev_priv->fbc.enable_fbc = i8xx_fbc_enable;
		dev_priv->fbc.disable_fbc = i8xx_fbc_disable;

		/* This value was pulled out of someone's hat */
		I915_WRITE(FBC_CONTROL, 500 << FBC_CTL_INTERVAL_SHIFT);
	}

	dev_priv->fbc.enabled = dev_priv->fbc.fbc_enabled(dev_priv);
}
