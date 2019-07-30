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
 * Authors:
 *    Vinit Azad <vinit.azad@intel.com>
 *    Ben Widawsky <ben@bwidawsk.net>
 *    Dave Gordon <david.s.gordon@intel.com>
 *    Alex Dai <yu.dai@intel.com>
 */
#ifdef __linux__
#include <linux/firmware.h>
#endif
#include "i915_drv.h"
#include "intel_guc.h"

#ifdef notyet

/**
 * DOC: GuC
 *
 * intel_guc:
 * Top level structure of guc. It handles firmware loading and manages client
 * pool and doorbells. intel_guc owns a i915_guc_client to replace the legacy
 * ExecList submission.
 *
 * Firmware versioning:
 * The firmware build process will generate a version header file with major and
 * minor version defined. The versions are built into CSS header of firmware.
 * i915 kernel driver set the minimal firmware version required per platform.
 * The firmware installation package will install (symbolic link) proper version
 * of firmware.
 *
 * GuC address space:
 * GuC does not allow any gfx GGTT address that falls into range [0, WOPCM_TOP),
 * which is reserved for Boot ROM, SRAM and WOPCM. Currently this top address is
 * 512K. In order to exclude 0-512K address space from GGTT, all gfx objects
 * used by GuC is pinned with PIN_OFFSET_BIAS along with size of WOPCM.
 *
 * Firmware log:
 * Firmware log is enabled by setting i915.guc_log_level to non-negative level.
 * Log data is printed out via reading debugfs i915_guc_log_dump. Reading from
 * i915_guc_load_status will print out firmware loading status and scratch
 * registers value.
 *
 */

#define I915_SKL_GUC_UCODE "i915/skl_guc_ver4.bin"
MODULE_FIRMWARE(I915_SKL_GUC_UCODE);

#define I915_KBL_GUC_UCODE "i915/kbl_guc_ver9_14.bin"
MODULE_FIRMWARE(I915_KBL_GUC_UCODE);

/* User-friendly representation of an enum */
const char *intel_guc_fw_status_repr(enum intel_guc_fw_status status)
{
	switch (status) {
	case GUC_FIRMWARE_FAIL:
		return "FAIL";
	case GUC_FIRMWARE_NONE:
		return "NONE";
	case GUC_FIRMWARE_PENDING:
		return "PENDING";
	case GUC_FIRMWARE_SUCCESS:
		return "SUCCESS";
	default:
		return "UNKNOWN!";
	}
};

static void direct_interrupts_to_host(struct drm_i915_private *dev_priv)
{
	struct intel_engine_cs *ring;
	int i, irqs;

	/* tell all command streamers NOT to forward interrupts and vblank to GuC */
	irqs = _MASKED_FIELD(GFX_FORWARD_VBLANK_MASK, GFX_FORWARD_VBLANK_NEVER);
	irqs |= _MASKED_BIT_DISABLE(GFX_INTERRUPT_STEERING);
	for_each_ring(ring, dev_priv, i)
		I915_WRITE(RING_MODE_GEN7(ring), irqs);

	/* route all GT interrupts to the host */
	I915_WRITE(GUC_BCS_RCS_IER, 0);
	I915_WRITE(GUC_VCS2_VCS1_IER, 0);
	I915_WRITE(GUC_WD_VECS_IER, 0);
}

static void direct_interrupts_to_guc(struct drm_i915_private *dev_priv)
{
	struct intel_engine_cs *ring;
	int i, irqs;

	/* tell all command streamers to forward interrupts and vblank to GuC */
	irqs = _MASKED_FIELD(GFX_FORWARD_VBLANK_MASK, GFX_FORWARD_VBLANK_ALWAYS);
	irqs |= _MASKED_BIT_ENABLE(GFX_INTERRUPT_STEERING);
	for_each_ring(ring, dev_priv, i)
		I915_WRITE(RING_MODE_GEN7(ring), irqs);

	/* route USER_INTERRUPT to Host, all others are sent to GuC. */
	irqs = GT_RENDER_USER_INTERRUPT << GEN8_RCS_IRQ_SHIFT |
	       GT_RENDER_USER_INTERRUPT << GEN8_BCS_IRQ_SHIFT;
	/* These three registers have the same bit definitions */
	I915_WRITE(GUC_BCS_RCS_IER, ~irqs);
	I915_WRITE(GUC_VCS2_VCS1_IER, ~irqs);
	I915_WRITE(GUC_WD_VECS_IER, ~irqs);
}

static u32 get_gttype(struct drm_i915_private *dev_priv)
{
	/* XXX: GT type based on PCI device ID? field seems unused by fw */
	return 0;
}

static u32 get_core_family(struct drm_i915_private *dev_priv)
{
	switch (INTEL_INFO(dev_priv)->gen) {
	case 9:
		return GFXCORE_FAMILY_GEN9;

	default:
		DRM_ERROR("GUC: unsupported core family\n");
		return GFXCORE_FAMILY_UNKNOWN;
	}
}

static void set_guc_init_params(struct drm_i915_private *dev_priv)
{
	struct intel_guc *guc = &dev_priv->guc;
	u32 params[GUC_CTL_MAX_DWORDS];
	int i;

	memset(&params, 0, sizeof(params));

	params[GUC_CTL_DEVICE_INFO] |=
		(get_gttype(dev_priv) << GUC_CTL_GTTYPE_SHIFT) |
		(get_core_family(dev_priv) << GUC_CTL_COREFAMILY_SHIFT);

	/*
	 * GuC ARAT increment is 10 ns. GuC default scheduler quantum is one
	 * second. This ARAR is calculated by:
	 * Scheduler-Quantum-in-ns / ARAT-increment-in-ns = 1000000000 / 10
	 */
	params[GUC_CTL_ARAT_HIGH] = 0;
	params[GUC_CTL_ARAT_LOW] = 100000000;

	params[GUC_CTL_WA] |= GUC_CTL_WA_UK_BY_DRIVER;

	params[GUC_CTL_FEATURE] |= GUC_CTL_DISABLE_SCHEDULER |
			GUC_CTL_VCS2_ENABLED;

	if (i915.guc_log_level >= 0) {
		params[GUC_CTL_LOG_PARAMS] = guc->log_flags;
		params[GUC_CTL_DEBUG] =
			i915.guc_log_level << GUC_LOG_VERBOSITY_SHIFT;
	}

	/* If GuC submission is enabled, set up additional parameters here */
	if (i915.enable_guc_submission) {
		u32 pgs = i915_gem_obj_ggtt_offset(dev_priv->guc.ctx_pool_obj);
		u32 ctx_in_16 = GUC_MAX_GPU_CONTEXTS / 16;

		pgs >>= PAGE_SHIFT;
		params[GUC_CTL_CTXINFO] = (pgs << GUC_CTL_BASE_ADDR_SHIFT) |
			(ctx_in_16 << GUC_CTL_CTXNUM_IN16_SHIFT);

		params[GUC_CTL_FEATURE] |= GUC_CTL_KERNEL_SUBMISSIONS;

		/* Unmask this bit to enable the GuC's internal scheduler */
		params[GUC_CTL_FEATURE] &= ~GUC_CTL_DISABLE_SCHEDULER;
	}

	I915_WRITE(SOFT_SCRATCH(0), 0);

	for (i = 0; i < GUC_CTL_MAX_DWORDS; i++)
		I915_WRITE(SOFT_SCRATCH(1 + i), params[i]);
}

/*
 * Read the GuC status register (GUC_STATUS) and store it in the
 * specified location; then return a boolean indicating whether
 * the value matches either of two values representing completion
 * of the GuC boot process.
 *
 * This is used for polling the GuC status in a wait_for_atomic()
 * loop below.
 */
static inline bool guc_ucode_response(struct drm_i915_private *dev_priv,
				      u32 *status)
{
	u32 val = I915_READ(GUC_STATUS);
	u32 uk_val = val & GS_UKERNEL_MASK;
	*status = val;
	return (uk_val == GS_UKERNEL_READY ||
		((val & GS_MIA_CORE_STATE) && uk_val == GS_UKERNEL_LAPIC_DONE));
}

/*
 * Transfer the firmware image to RAM for execution by the microcontroller.
 *
 * GuC Firmware layout:
 * +-------------------------------+  ----
 * |          CSS header           |  128B
 * | contains major/minor version  |
 * +-------------------------------+  ----
 * |             uCode             |
 * +-------------------------------+  ----
 * |         RSA signature         |  256B
 * +-------------------------------+  ----
 *
 * Architecturally, the DMA engine is bidirectional, and can potentially even
 * transfer between GTT locations. This functionality is left out of the API
 * for now as there is no need for it.
 *
 * Note that GuC needs the CSS header plus uKernel code to be copied by the
 * DMA engine in one operation, whereas the RSA signature is loaded via MMIO.
 */

#define UOS_CSS_HEADER_OFFSET		0
#define UOS_VER_MINOR_OFFSET		0x44
#define UOS_VER_MAJOR_OFFSET		0x46
#define UOS_CSS_HEADER_SIZE		0x80
#define UOS_RSA_SIG_SIZE		0x100

static int guc_ucode_xfer_dma(struct drm_i915_private *dev_priv)
{
	struct intel_guc_fw *guc_fw = &dev_priv->guc.guc_fw;
	struct drm_i915_gem_object *fw_obj = guc_fw->guc_fw_obj;
	unsigned long offset;
	struct sg_table *sg = fw_obj->pages;
	u32 status, ucode_size, rsa[UOS_RSA_SIG_SIZE / sizeof(u32)];
	int i, ret = 0;

	/* uCode size, also is where RSA signature starts */
	offset = ucode_size = guc_fw->guc_fw_size - UOS_RSA_SIG_SIZE;
	I915_WRITE(DMA_COPY_SIZE, ucode_size);

	/* Copy RSA signature from the fw image to HW for verification */
	sg_pcopy_to_buffer(sg->sgl, sg->nents, rsa, UOS_RSA_SIG_SIZE, offset);
	for (i = 0; i < UOS_RSA_SIG_SIZE / sizeof(u32); i++)
		I915_WRITE(UOS_RSA_SCRATCH(i), rsa[i]);

	/* Set the source address for the new blob */
	offset = i915_gem_obj_ggtt_offset(fw_obj);
	I915_WRITE(DMA_ADDR_0_LOW, lower_32_bits(offset));
	I915_WRITE(DMA_ADDR_0_HIGH, upper_32_bits(offset) & 0xFFFF);

	/*
	 * Set the DMA destination. Current uCode expects the code to be
	 * loaded at 8k; locations below this are used for the stack.
	 */
	I915_WRITE(DMA_ADDR_1_LOW, 0x2000);
	I915_WRITE(DMA_ADDR_1_HIGH, DMA_ADDRESS_SPACE_WOPCM);

	/* Finally start the DMA */
	I915_WRITE(DMA_CTRL, _MASKED_BIT_ENABLE(UOS_MOVE | START_DMA));

	/*
	 * Spin-wait for the DMA to complete & the GuC to start up.
	 * NB: Docs recommend not using the interrupt for completion.
	 * Measurements indicate this should take no more than 20ms, so a
	 * timeout here indicates that the GuC has failed and is unusable.
	 * (Higher levels of the driver will attempt to fall back to
	 * execlist mode if this happens.)
	 */
	ret = wait_for_atomic(guc_ucode_response(dev_priv, &status), 100);

	DRM_DEBUG_DRIVER("DMA status 0x%x, GuC status 0x%x\n",
			I915_READ(DMA_CTRL), status);

	if ((status & GS_BOOTROM_MASK) == GS_BOOTROM_RSA_FAILED) {
		DRM_ERROR("GuC firmware signature verification failed\n");
		ret = -ENOEXEC;
	}

	DRM_DEBUG_DRIVER("returning %d\n", ret);

	return ret;
}

/*
 * Load the GuC firmware blob into the MinuteIA.
 */
static int guc_ucode_xfer(struct drm_i915_private *dev_priv)
{
	struct intel_guc_fw *guc_fw = &dev_priv->guc.guc_fw;
	struct drm_device *dev = dev_priv->dev;
	int ret;

	ret = i915_gem_object_set_to_gtt_domain(guc_fw->guc_fw_obj, false);
	if (ret) {
		DRM_DEBUG_DRIVER("set-domain failed %d\n", ret);
		return ret;
	}

	ret = i915_gem_obj_ggtt_pin(guc_fw->guc_fw_obj, 0, 0);
	if (ret) {
		DRM_DEBUG_DRIVER("pin failed %d\n", ret);
		return ret;
	}

	/* Invalidate GuC TLB to let GuC take the latest updates to GTT. */
	I915_WRITE(GEN8_GTCR, GEN8_GTCR_INVALIDATE);

	intel_uncore_forcewake_get(dev_priv, FORCEWAKE_ALL);

	/* init WOPCM */
	I915_WRITE(GUC_WOPCM_SIZE, GUC_WOPCM_SIZE_VALUE);
	I915_WRITE(DMA_GUC_WOPCM_OFFSET, GUC_WOPCM_OFFSET_VALUE);

	/* Enable MIA caching. GuC clock gating is disabled. */
	I915_WRITE(GUC_SHIM_CONTROL, GUC_SHIM_CONTROL_VALUE);

	/* WaDisableMinuteIaClockGating:skl,bxt */
	if (IS_SKL_REVID(dev, 0, SKL_REVID_B0) ||
	    IS_BXT_REVID(dev, 0, BXT_REVID_A1)) {
		I915_WRITE(GUC_SHIM_CONTROL, (I915_READ(GUC_SHIM_CONTROL) &
					      ~GUC_ENABLE_MIA_CLOCK_GATING));
	}

	/* WaC6DisallowByGfxPause*/
	I915_WRITE(GEN6_GFXPAUSE, 0x30FFF);

	if (IS_BROXTON(dev))
		I915_WRITE(GEN9LP_GT_PM_CONFIG, GT_DOORBELL_ENABLE);
	else
		I915_WRITE(GEN9_GT_PM_CONFIG, GT_DOORBELL_ENABLE);

	if (IS_GEN9(dev)) {
		/* DOP Clock Gating Enable for GuC clocks */
		I915_WRITE(GEN7_MISCCPCTL, (GEN8_DOP_CLOCK_GATE_GUC_ENABLE |
					    I915_READ(GEN7_MISCCPCTL)));

		/* allows for 5us before GT can go to RC6 */
		I915_WRITE(GUC_ARAT_C6DIS, 0x1FF);
	}

	set_guc_init_params(dev_priv);

	ret = guc_ucode_xfer_dma(dev_priv);

	intel_uncore_forcewake_put(dev_priv, FORCEWAKE_ALL);

	/*
	 * We keep the object pages for reuse during resume. But we can unpin it
	 * now that DMA has completed, so it doesn't continue to take up space.
	 */
	i915_gem_object_ggtt_unpin(guc_fw->guc_fw_obj);

	return ret;
}

/**
 * intel_guc_ucode_load() - load GuC uCode into the device
 * @dev:	drm device
 *
 * Called from gem_init_hw() during driver loading and also after a GPU reset.
 *
 * The firmware image should have already been fetched into memory by the
 * earlier call to intel_guc_ucode_init(), so here we need only check that
 * is succeeded, and then transfer the image to the h/w.
 *
 * Return:	non-zero code on error
 */
int intel_guc_ucode_load(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_guc_fw *guc_fw = &dev_priv->guc.guc_fw;
	int err = 0;

	DRM_DEBUG_DRIVER("GuC fw status: fetch %s, load %s\n",
		intel_guc_fw_status_repr(guc_fw->guc_fw_fetch_status),
		intel_guc_fw_status_repr(guc_fw->guc_fw_load_status));

	direct_interrupts_to_host(dev_priv);

	if (guc_fw->guc_fw_fetch_status == GUC_FIRMWARE_NONE)
		return 0;

	if (guc_fw->guc_fw_fetch_status == GUC_FIRMWARE_SUCCESS &&
	    guc_fw->guc_fw_load_status == GUC_FIRMWARE_FAIL)
		return -ENOEXEC;

	guc_fw->guc_fw_load_status = GUC_FIRMWARE_PENDING;

	DRM_DEBUG_DRIVER("GuC fw fetch status %s\n",
		intel_guc_fw_status_repr(guc_fw->guc_fw_fetch_status));

	switch (guc_fw->guc_fw_fetch_status) {
	case GUC_FIRMWARE_FAIL:
		/* something went wrong :( */
		err = -EIO;
		goto fail;

	case GUC_FIRMWARE_NONE:
	case GUC_FIRMWARE_PENDING:
	default:
		/* "can't happen" */
		WARN_ONCE(1, "GuC fw %s invalid guc_fw_fetch_status %s [%d]\n",
			guc_fw->guc_fw_path,
			intel_guc_fw_status_repr(guc_fw->guc_fw_fetch_status),
			guc_fw->guc_fw_fetch_status);
		err = -ENXIO;
		goto fail;

	case GUC_FIRMWARE_SUCCESS:
		break;
	}

	err = i915_guc_submission_init(dev);
	if (err)
		goto fail;

	err = guc_ucode_xfer(dev_priv);
	if (err)
		goto fail;

	guc_fw->guc_fw_load_status = GUC_FIRMWARE_SUCCESS;

	DRM_DEBUG_DRIVER("GuC fw status: fetch %s, load %s\n",
		intel_guc_fw_status_repr(guc_fw->guc_fw_fetch_status),
		intel_guc_fw_status_repr(guc_fw->guc_fw_load_status));

	if (i915.enable_guc_submission) {
		/* The execbuf_client will be recreated. Release it first. */
		i915_guc_submission_disable(dev);

		err = i915_guc_submission_enable(dev);
		if (err)
			goto fail;
		direct_interrupts_to_guc(dev_priv);
	}

	return 0;

fail:
	if (guc_fw->guc_fw_load_status == GUC_FIRMWARE_PENDING)
		guc_fw->guc_fw_load_status = GUC_FIRMWARE_FAIL;

	direct_interrupts_to_host(dev_priv);
	i915_guc_submission_disable(dev);

	return err;
}

static void guc_fw_fetch(struct drm_device *dev, struct intel_guc_fw *guc_fw)
{
	struct drm_i915_gem_object *obj;
	const struct firmware *fw;
	const u8 *css_header;
	const size_t minsize = UOS_CSS_HEADER_SIZE + UOS_RSA_SIG_SIZE;
	const size_t maxsize = GUC_WOPCM_SIZE_VALUE + UOS_RSA_SIG_SIZE
			- 0x8000; /* 32k reserved (8K stack + 24k context) */
	int err;

	DRM_DEBUG_DRIVER("before requesting firmware: GuC fw fetch status %s\n",
		intel_guc_fw_status_repr(guc_fw->guc_fw_fetch_status));

	err = request_firmware(&fw, guc_fw->guc_fw_path, &dev->pdev->dev);
	if (err)
		goto fail;
	if (!fw)
		goto fail;

	DRM_DEBUG_DRIVER("fetch GuC fw from %s succeeded, fw %p\n",
		guc_fw->guc_fw_path, fw);
	DRM_DEBUG_DRIVER("firmware file size %zu (minimum %zu, maximum %zu)\n",
		fw->size, minsize, maxsize);

	/* Check the size of the blob befoe examining buffer contents */
	if (fw->size < minsize || fw->size > maxsize)
		goto fail;

	/*
	 * The GuC firmware image has the version number embedded at a well-known
	 * offset within the firmware blob; note that major / minor version are
	 * TWO bytes each (i.e. u16), although all pointers and offsets are defined
	 * in terms of bytes (u8).
	 */
	css_header = fw->data + UOS_CSS_HEADER_OFFSET;
	guc_fw->guc_fw_major_found = *(u16 *)(css_header + UOS_VER_MAJOR_OFFSET);
	guc_fw->guc_fw_minor_found = *(u16 *)(css_header + UOS_VER_MINOR_OFFSET);

	if (guc_fw->guc_fw_major_found != guc_fw->guc_fw_major_wanted ||
	    guc_fw->guc_fw_minor_found < guc_fw->guc_fw_minor_wanted) {
		DRM_ERROR("GuC firmware version %d.%d, required %d.%d\n",
			guc_fw->guc_fw_major_found, guc_fw->guc_fw_minor_found,
			guc_fw->guc_fw_major_wanted, guc_fw->guc_fw_minor_wanted);
		err = -ENOEXEC;
		goto fail;
	}

	DRM_DEBUG_DRIVER("firmware version %d.%d OK (minimum %d.%d)\n",
			guc_fw->guc_fw_major_found, guc_fw->guc_fw_minor_found,
			guc_fw->guc_fw_major_wanted, guc_fw->guc_fw_minor_wanted);

	mutex_lock(&dev->struct_mutex);
	obj = i915_gem_object_create_from_data(dev, fw->data, fw->size);
	mutex_unlock(&dev->struct_mutex);
	if (IS_ERR_OR_NULL(obj)) {
		err = obj ? PTR_ERR(obj) : -ENOMEM;
		goto fail;
	}

	guc_fw->guc_fw_obj = obj;
	guc_fw->guc_fw_size = fw->size;

	DRM_DEBUG_DRIVER("GuC fw fetch status SUCCESS, obj %p\n",
			guc_fw->guc_fw_obj);

	release_firmware(fw);
	guc_fw->guc_fw_fetch_status = GUC_FIRMWARE_SUCCESS;
	return;

fail:
	DRM_DEBUG_DRIVER("GuC fw fetch status FAIL; err %d, fw %p, obj %p\n",
		err, fw, guc_fw->guc_fw_obj);
	DRM_ERROR("Failed to fetch GuC firmware from %s (error %d)\n",
		  guc_fw->guc_fw_path, err);

	obj = guc_fw->guc_fw_obj;
	if (obj)
		drm_gem_object_unreference(&obj->base);
	guc_fw->guc_fw_obj = NULL;

	release_firmware(fw);		/* OK even if fw is NULL */
	guc_fw->guc_fw_fetch_status = GUC_FIRMWARE_FAIL;
}

/**
 * intel_guc_ucode_init() - define parameters and fetch firmware
 * @dev:	drm device
 *
 * Called early during driver load, but after GEM is initialised.
 *
 * The firmware will be transferred to the GuC's memory later,
 * when intel_guc_ucode_load() is called.
 */
void intel_guc_ucode_init(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_guc_fw *guc_fw = &dev_priv->guc.guc_fw;
	const char *fw_path;

	if (!HAS_GUC_SCHED(dev))
		i915.enable_guc_submission = false;

	if (!HAS_GUC_UCODE(dev)) {
		fw_path = NULL;
	} else if (IS_SKYLAKE(dev)) {
		fw_path = I915_SKL_GUC_UCODE;
		guc_fw->guc_fw_major_wanted = 4;
		guc_fw->guc_fw_minor_wanted = 3;
	} else if (IS_KABYLAKE(dev)) {
		fw_path = I915_KBL_GUC_UCODE;
		guc_fw->guc_fw_major_wanted = 9;
		guc_fw->guc_fw_minor_wanted = 14;
        } else {
		i915.enable_guc_submission = false;
		fw_path = "";	/* unknown device */
	}

	guc_fw->guc_dev = dev;
	guc_fw->guc_fw_path = fw_path;
	guc_fw->guc_fw_fetch_status = GUC_FIRMWARE_NONE;
	guc_fw->guc_fw_load_status = GUC_FIRMWARE_NONE;

	if (fw_path == NULL)
		return;

	if (*fw_path == '\0') {
		DRM_ERROR("No GuC firmware known for this platform\n");
		guc_fw->guc_fw_fetch_status = GUC_FIRMWARE_FAIL;
		return;
	}

	guc_fw->guc_fw_fetch_status = GUC_FIRMWARE_PENDING;
	DRM_DEBUG_DRIVER("GuC firmware pending, path %s\n", fw_path);
	guc_fw_fetch(dev, guc_fw);
	/* status must now be FAIL or SUCCESS */
}

/**
 * intel_guc_ucode_fini() - clean up all allocated resources
 * @dev:	drm device
 */
void intel_guc_ucode_fini(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_guc_fw *guc_fw = &dev_priv->guc.guc_fw;

	direct_interrupts_to_host(dev_priv);
	i915_guc_submission_fini(dev);

	mutex_lock(&dev->struct_mutex);
	if (guc_fw->guc_fw_obj)
		drm_gem_object_unreference(&guc_fw->guc_fw_obj->base);
	guc_fw->guc_fw_obj = NULL;
	mutex_unlock(&dev->struct_mutex);

	guc_fw->guc_fw_fetch_status = GUC_FIRMWARE_NONE;
}

#else

int
intel_guc_ucode_load(struct drm_device *dev)
{
	return -ENOEXEC;
}

void
intel_guc_ucode_init(struct drm_device *dev)
{
}

void
intel_guc_ucode_fini(struct drm_device *dev)
{
}

#endif
