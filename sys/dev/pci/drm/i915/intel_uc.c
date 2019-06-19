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

#include "i915_drv.h"
#include "intel_uc.h"
#include "intel_guc_submission.h"
#include "intel_guc.h"

static void guc_free_load_err_log(struct intel_guc *guc);

/* Reset GuC providing us with fresh state for both GuC and HuC.
 */
static int __intel_uc_reset_hw(struct drm_i915_private *dev_priv)
{
	int ret;
	u32 guc_status;

	ret = intel_reset_guc(dev_priv);
	if (ret) {
		DRM_ERROR("Failed to reset GuC, ret = %d\n", ret);
		return ret;
	}

	guc_status = I915_READ(GUC_STATUS);
	WARN(!(guc_status & GS_MIA_IN_RESET),
	     "GuC status: 0x%x, MIA core expected to be in reset\n",
	     guc_status);

	return ret;
}

static int __get_platform_enable_guc(struct drm_i915_private *i915)
{
	struct intel_uc_fw *guc_fw = &i915->guc.fw;
	struct intel_uc_fw *huc_fw = &i915->huc.fw;
	int enable_guc = 0;

	/* Default is to enable GuC/HuC if we know their firmwares */
	if (intel_uc_fw_is_selected(guc_fw))
		enable_guc |= ENABLE_GUC_SUBMISSION;
	if (intel_uc_fw_is_selected(huc_fw))
		enable_guc |= ENABLE_GUC_LOAD_HUC;

	/* Any platform specific fine-tuning can be done here */

	return enable_guc;
}

static int __get_default_guc_log_level(struct drm_i915_private *i915)
{
	int guc_log_level;

	if (!HAS_GUC(i915) || !intel_uc_is_using_guc())
		guc_log_level = GUC_LOG_LEVEL_DISABLED;
	else if (IS_ENABLED(CONFIG_DRM_I915_DEBUG) ||
		 IS_ENABLED(CONFIG_DRM_I915_DEBUG_GEM))
		guc_log_level = GUC_LOG_LEVEL_MAX;
	else
		guc_log_level = GUC_LOG_LEVEL_NON_VERBOSE;

	/* Any platform specific fine-tuning can be done here */

	return guc_log_level;
}

/**
 * sanitize_options_early - sanitize uC related modparam options
 * @i915: device private
 *
 * In case of "enable_guc" option this function will attempt to modify
 * it only if it was initially set to "auto(-1)". Default value for this
 * modparam varies between platforms and it is hardcoded in driver code.
 * Any other modparam value is only monitored against availability of the
 * related hardware or firmware definitions.
 *
 * In case of "guc_log_level" option this function will attempt to modify
 * it only if it was initially set to "auto(-1)" or if initial value was
 * "enable(1..4)" on platforms without the GuC. Default value for this
 * modparam varies between platforms and is usually set to "disable(0)"
 * unless GuC is enabled on given platform and the driver is compiled with
 * debug config when this modparam will default to "enable(1..4)".
 */
static void sanitize_options_early(struct drm_i915_private *i915)
{
	struct intel_uc_fw *guc_fw = &i915->guc.fw;
	struct intel_uc_fw *huc_fw = &i915->huc.fw;

	/* A negative value means "use platform default" */
	if (i915_modparams.enable_guc < 0)
		i915_modparams.enable_guc = __get_platform_enable_guc(i915);

	DRM_DEBUG_DRIVER("enable_guc=%d (submission:%s huc:%s)\n",
			 i915_modparams.enable_guc,
			 yesno(intel_uc_is_using_guc_submission()),
			 yesno(intel_uc_is_using_huc()));

	/* Verify GuC firmware availability */
	if (intel_uc_is_using_guc() && !intel_uc_fw_is_selected(guc_fw)) {
		DRM_WARN("Incompatible option detected: %s=%d, %s!\n",
			 "enable_guc", i915_modparams.enable_guc,
			 !HAS_GUC(i915) ? "no GuC hardware" :
					  "no GuC firmware");
	}

	/* Verify HuC firmware availability */
	if (intel_uc_is_using_huc() && !intel_uc_fw_is_selected(huc_fw)) {
		DRM_WARN("Incompatible option detected: %s=%d, %s!\n",
			 "enable_guc", i915_modparams.enable_guc,
			 !HAS_HUC(i915) ? "no HuC hardware" :
					  "no HuC firmware");
	}

	/* A negative value means "use platform/config default" */
	if (i915_modparams.guc_log_level < 0)
		i915_modparams.guc_log_level =
			__get_default_guc_log_level(i915);

	if (i915_modparams.guc_log_level > 0 && !intel_uc_is_using_guc()) {
		DRM_WARN("Incompatible option detected: %s=%d, %s!\n",
			 "guc_log_level", i915_modparams.guc_log_level,
			 !HAS_GUC(i915) ? "no GuC hardware" :
					  "GuC not enabled");
		i915_modparams.guc_log_level = 0;
	}

	if (i915_modparams.guc_log_level > GUC_LOG_LEVEL_MAX) {
		DRM_WARN("Incompatible option detected: %s=%d, %s!\n",
			 "guc_log_level", i915_modparams.guc_log_level,
			 "verbosity too high");
		i915_modparams.guc_log_level = GUC_LOG_LEVEL_MAX;
	}

	DRM_DEBUG_DRIVER("guc_log_level=%d (enabled:%s, verbose:%s, verbosity:%d)\n",
			 i915_modparams.guc_log_level,
			 yesno(i915_modparams.guc_log_level),
			 yesno(GUC_LOG_LEVEL_IS_VERBOSE(i915_modparams.guc_log_level)),
			 GUC_LOG_LEVEL_TO_VERBOSITY(i915_modparams.guc_log_level));

	/* Make sure that sanitization was done */
	GEM_BUG_ON(i915_modparams.enable_guc < 0);
	GEM_BUG_ON(i915_modparams.guc_log_level < 0);
}

void intel_uc_init_early(struct drm_i915_private *i915)
{
	struct intel_guc *guc = &i915->guc;
	struct intel_huc *huc = &i915->huc;

	intel_guc_init_early(guc);
	intel_huc_init_early(huc);

	sanitize_options_early(i915);
}

void intel_uc_cleanup_early(struct drm_i915_private *i915)
{
	struct intel_guc *guc = &i915->guc;

	guc_free_load_err_log(guc);
}

/**
 * intel_uc_init_mmio - setup uC MMIO access
 * @i915: device private
 *
 * Setup minimal state necessary for MMIO accesses later in the
 * initialization sequence.
 */
void intel_uc_init_mmio(struct drm_i915_private *i915)
{
	intel_guc_init_send_regs(&i915->guc);
}

static void guc_capture_load_err_log(struct intel_guc *guc)
{
	if (!guc->log.vma || !intel_guc_log_get_level(&guc->log))
		return;

	if (!guc->load_err_log)
		guc->load_err_log = i915_gem_object_get(guc->log.vma->obj);

	return;
}

static void guc_free_load_err_log(struct intel_guc *guc)
{
	if (guc->load_err_log)
		i915_gem_object_put(guc->load_err_log);
}

static int guc_enable_communication(struct intel_guc *guc)
{
	struct drm_i915_private *i915 = guc_to_i915(guc);

	gen9_enable_guc_interrupts(i915);

	if (HAS_GUC_CT(i915))
		return intel_guc_ct_enable(&guc->ct);

	guc->send = intel_guc_send_mmio;
	guc->handler = intel_guc_to_host_event_handler_mmio;
	return 0;
}

static void guc_disable_communication(struct intel_guc *guc)
{
	struct drm_i915_private *i915 = guc_to_i915(guc);

	if (HAS_GUC_CT(i915))
		intel_guc_ct_disable(&guc->ct);

	gen9_disable_guc_interrupts(i915);

	guc->send = intel_guc_send_nop;
	guc->handler = intel_guc_to_host_event_handler_nop;
}

int intel_uc_init_misc(struct drm_i915_private *i915)
{
	struct intel_guc *guc = &i915->guc;
	struct intel_huc *huc = &i915->huc;
	int ret;

	if (!USES_GUC(i915))
		return 0;

	ret = intel_guc_init_misc(guc);
	if (ret)
		return ret;

	if (USES_HUC(i915)) {
		ret = intel_huc_init_misc(huc);
		if (ret)
			goto err_guc;
	}

	return 0;

err_guc:
	intel_guc_fini_misc(guc);
	return ret;
}

void intel_uc_fini_misc(struct drm_i915_private *i915)
{
	struct intel_guc *guc = &i915->guc;
	struct intel_huc *huc = &i915->huc;

	if (!USES_GUC(i915))
		return;

	if (USES_HUC(i915))
		intel_huc_fini_misc(huc);

	intel_guc_fini_misc(guc);
}

int intel_uc_init(struct drm_i915_private *i915)
{
	struct intel_guc *guc = &i915->guc;
	int ret;

	if (!USES_GUC(i915))
		return 0;

	if (!HAS_GUC(i915))
		return -ENODEV;

	ret = intel_guc_init(guc);
	if (ret)
		return ret;

	if (USES_GUC_SUBMISSION(i915)) {
		/*
		 * This is stuff we need to have available at fw load time
		 * if we are planning to enable submission later
		 */
		ret = intel_guc_submission_init(guc);
		if (ret) {
			intel_guc_fini(guc);
			return ret;
		}
	}

	return 0;
}

void intel_uc_fini(struct drm_i915_private *i915)
{
	struct intel_guc *guc = &i915->guc;

	if (!USES_GUC(i915))
		return;

	GEM_BUG_ON(!HAS_GUC(i915));

	if (USES_GUC_SUBMISSION(i915))
		intel_guc_submission_fini(guc);

	intel_guc_fini(guc);
}

void intel_uc_sanitize(struct drm_i915_private *i915)
{
	struct intel_guc *guc = &i915->guc;
	struct intel_huc *huc = &i915->huc;

	if (!USES_GUC(i915))
		return;

	GEM_BUG_ON(!HAS_GUC(i915));

	guc_disable_communication(guc);

	intel_huc_sanitize(huc);
	intel_guc_sanitize(guc);

	__intel_uc_reset_hw(i915);
}

int intel_uc_init_hw(struct drm_i915_private *i915)
{
	struct intel_guc *guc = &i915->guc;
	struct intel_huc *huc = &i915->huc;
	int ret, attempts;

	if (!USES_GUC(i915))
		return 0;

	GEM_BUG_ON(!HAS_GUC(i915));

	gen9_reset_guc_interrupts(i915);

	/* WaEnableuKernelHeaderValidFix:skl */
	/* WaEnableGuCBootHashCheckNotSet:skl,bxt,kbl */
	if (IS_GEN9(i915))
		attempts = 3;
	else
		attempts = 1;

	while (attempts--) {
		/*
		 * Always reset the GuC just before (re)loading, so
		 * that the state and timing are fairly predictable
		 */
		ret = __intel_uc_reset_hw(i915);
		if (ret)
			goto err_out;

		if (USES_HUC(i915)) {
			ret = intel_huc_fw_upload(huc);
			if (ret)
				goto err_out;
		}

		intel_guc_init_params(guc);
		ret = intel_guc_fw_upload(guc);
		if (ret == 0 || ret != -EAGAIN)
			break;

		DRM_DEBUG_DRIVER("GuC fw load failed: %d; will reset and "
				 "retry %d more time(s)\n", ret, attempts);
	}

	/* Did we succeded or run out of retries? */
	if (ret)
		goto err_log_capture;

	ret = guc_enable_communication(guc);
	if (ret)
		goto err_log_capture;

	if (USES_HUC(i915)) {
		ret = intel_huc_auth(huc);
		if (ret)
			goto err_communication;
	}

	if (USES_GUC_SUBMISSION(i915)) {
		ret = intel_guc_submission_enable(guc);
		if (ret)
			goto err_communication;
	}

	dev_info(i915->drm.dev, "GuC firmware version %u.%u\n",
		 guc->fw.major_ver_found, guc->fw.minor_ver_found);
	dev_info(i915->drm.dev, "GuC submission %s\n",
		 enableddisabled(USES_GUC_SUBMISSION(i915)));
	dev_info(i915->drm.dev, "HuC %s\n",
		 enableddisabled(USES_HUC(i915)));

	return 0;

	/*
	 * We've failed to load the firmware :(
	 */
err_communication:
	guc_disable_communication(guc);
err_log_capture:
	guc_capture_load_err_log(guc);
err_out:
	/*
	 * Note that there is no fallback as either user explicitly asked for
	 * the GuC or driver default option was to run with the GuC enabled.
	 */
	if (GEM_WARN_ON(ret == -EIO))
		ret = -EINVAL;

	dev_err(i915->drm.dev, "GuC initialization failed %d\n", ret);
	return ret;
}

void intel_uc_fini_hw(struct drm_i915_private *i915)
{
	struct intel_guc *guc = &i915->guc;

	if (!USES_GUC(i915))
		return;

	GEM_BUG_ON(!HAS_GUC(i915));

	if (USES_GUC_SUBMISSION(i915))
		intel_guc_submission_disable(guc);

	guc_disable_communication(guc);
}

int intel_uc_suspend(struct drm_i915_private *i915)
{
	struct intel_guc *guc = &i915->guc;
	int err;

	if (!USES_GUC(i915))
		return 0;

	if (guc->fw.load_status != INTEL_UC_FIRMWARE_SUCCESS)
		return 0;

	err = intel_guc_suspend(guc);
	if (err) {
		DRM_DEBUG_DRIVER("Failed to suspend GuC, err=%d", err);
		return err;
	}

	gen9_disable_guc_interrupts(i915);

	return 0;
}

int intel_uc_resume(struct drm_i915_private *i915)
{
	struct intel_guc *guc = &i915->guc;
	int err;

	if (!USES_GUC(i915))
		return 0;

	if (guc->fw.load_status != INTEL_UC_FIRMWARE_SUCCESS)
		return 0;

	gen9_enable_guc_interrupts(i915);

	err = intel_guc_resume(guc);
	if (err) {
		DRM_DEBUG_DRIVER("Failed to resume GuC, err=%d", err);
		return err;
	}

	return 0;
}
