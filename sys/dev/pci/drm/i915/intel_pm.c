/*
 * Copyright © 2012 Intel Corporation
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
 *    Eugeni Dodonov <eugeni.dodonov@intel.com>
 *
 */

#ifdef __linux__
#include <linux/cpufreq.h>
#endif
#include "i915_drv.h"
#include "intel_drv.h"
#ifdef __linux__
#include "../../../platform/x86/intel_ips.h"
#include <linux/module.h>
#endif

/**
 * RC6 is a special power stage which allows the GPU to enter an very
 * low-voltage mode when idle, using down to 0V while at this stage.  This
 * stage is entered automatically when the GPU is idle when RC6 support is
 * enabled, and as soon as new workload arises GPU wakes up automatically as well.
 *
 * There are different RC6 modes available in Intel GPU, which differentiate
 * among each other with the latency required to enter and leave RC6 and
 * voltage consumed by the GPU in different states.
 *
 * The combination of the following flags define which states GPU is allowed
 * to enter, while RC6 is the normal RC6 state, RC6p is the deep RC6, and
 * RC6pp is deepest RC6. Their support by hardware varies according to the
 * GPU, BIOS, chipset and platform. RC6 is usually the safest one and the one
 * which brings the most power savings; deeper states save more power, but
 * require higher latency to switch to and wake up.
 */
#define INTEL_RC6_ENABLE			(1<<0)
#define INTEL_RC6p_ENABLE			(1<<1)
#define INTEL_RC6pp_ENABLE			(1<<2)

static void bxt_init_clock_gating(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	/* WaDisableSDEUnitClockGating:bxt */
	I915_WRITE(GEN8_UCGCTL6, I915_READ(GEN8_UCGCTL6) |
		   GEN8_SDEUNIT_CLOCK_GATE_DISABLE);

	/*
	 * FIXME:
	 * GEN8_HDCUNIT_CLOCK_GATE_DISABLE_HDCREQ applies on 3x6 GT SKUs only.
	 */
	I915_WRITE(GEN8_UCGCTL6, I915_READ(GEN8_UCGCTL6) |
		   GEN8_HDCUNIT_CLOCK_GATE_DISABLE_HDCREQ);
}

static void i915_pineview_get_mem_freq(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	u32 tmp;

	tmp = I915_READ(CLKCFG);

	switch (tmp & CLKCFG_FSB_MASK) {
	case CLKCFG_FSB_533:
		dev_priv->fsb_freq = 533; /* 133*4 */
		break;
	case CLKCFG_FSB_800:
		dev_priv->fsb_freq = 800; /* 200*4 */
		break;
	case CLKCFG_FSB_667:
		dev_priv->fsb_freq =  667; /* 167*4 */
		break;
	case CLKCFG_FSB_400:
		dev_priv->fsb_freq = 400; /* 100*4 */
		break;
	}

	switch (tmp & CLKCFG_MEM_MASK) {
	case CLKCFG_MEM_533:
		dev_priv->mem_freq = 533;
		break;
	case CLKCFG_MEM_667:
		dev_priv->mem_freq = 667;
		break;
	case CLKCFG_MEM_800:
		dev_priv->mem_freq = 800;
		break;
	}

	/* detect pineview DDR3 setting */
	tmp = I915_READ(CSHRDDR3CTL);
	dev_priv->is_ddr3 = (tmp & CSHRDDR3CTL_DDR3) ? 1 : 0;
}

static void i915_ironlake_get_mem_freq(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	u16 ddrpll, csipll;

	ddrpll = I915_READ16(DDRMPLL1);
	csipll = I915_READ16(CSIPLL0);

	switch (ddrpll & 0xff) {
	case 0xc:
		dev_priv->mem_freq = 800;
		break;
	case 0x10:
		dev_priv->mem_freq = 1066;
		break;
	case 0x14:
		dev_priv->mem_freq = 1333;
		break;
	case 0x18:
		dev_priv->mem_freq = 1600;
		break;
	default:
		DRM_DEBUG_DRIVER("unknown memory frequency 0x%02x\n",
				 ddrpll & 0xff);
		dev_priv->mem_freq = 0;
		break;
	}

	dev_priv->ips.r_t = dev_priv->mem_freq;

	switch (csipll & 0x3ff) {
	case 0x00c:
		dev_priv->fsb_freq = 3200;
		break;
	case 0x00e:
		dev_priv->fsb_freq = 3733;
		break;
	case 0x010:
		dev_priv->fsb_freq = 4266;
		break;
	case 0x012:
		dev_priv->fsb_freq = 4800;
		break;
	case 0x014:
		dev_priv->fsb_freq = 5333;
		break;
	case 0x016:
		dev_priv->fsb_freq = 5866;
		break;
	case 0x018:
		dev_priv->fsb_freq = 6400;
		break;
	default:
		DRM_DEBUG_DRIVER("unknown fsb frequency 0x%04x\n",
				 csipll & 0x3ff);
		dev_priv->fsb_freq = 0;
		break;
	}

	if (dev_priv->fsb_freq == 3200) {
		dev_priv->ips.c_m = 0;
	} else if (dev_priv->fsb_freq > 3200 && dev_priv->fsb_freq <= 4800) {
		dev_priv->ips.c_m = 1;
	} else {
		dev_priv->ips.c_m = 2;
	}
}

static const struct cxsr_latency cxsr_latency_table[] = {
	{1, 0, 800, 400, 3382, 33382, 3983, 33983},    /* DDR2-400 SC */
	{1, 0, 800, 667, 3354, 33354, 3807, 33807},    /* DDR2-667 SC */
	{1, 0, 800, 800, 3347, 33347, 3763, 33763},    /* DDR2-800 SC */
	{1, 1, 800, 667, 6420, 36420, 6873, 36873},    /* DDR3-667 SC */
	{1, 1, 800, 800, 5902, 35902, 6318, 36318},    /* DDR3-800 SC */

	{1, 0, 667, 400, 3400, 33400, 4021, 34021},    /* DDR2-400 SC */
	{1, 0, 667, 667, 3372, 33372, 3845, 33845},    /* DDR2-667 SC */
	{1, 0, 667, 800, 3386, 33386, 3822, 33822},    /* DDR2-800 SC */
	{1, 1, 667, 667, 6438, 36438, 6911, 36911},    /* DDR3-667 SC */
	{1, 1, 667, 800, 5941, 35941, 6377, 36377},    /* DDR3-800 SC */

	{1, 0, 400, 400, 3472, 33472, 4173, 34173},    /* DDR2-400 SC */
	{1, 0, 400, 667, 3443, 33443, 3996, 33996},    /* DDR2-667 SC */
	{1, 0, 400, 800, 3430, 33430, 3946, 33946},    /* DDR2-800 SC */
	{1, 1, 400, 667, 6509, 36509, 7062, 37062},    /* DDR3-667 SC */
	{1, 1, 400, 800, 5985, 35985, 6501, 36501},    /* DDR3-800 SC */

	{0, 0, 800, 400, 3438, 33438, 4065, 34065},    /* DDR2-400 SC */
	{0, 0, 800, 667, 3410, 33410, 3889, 33889},    /* DDR2-667 SC */
	{0, 0, 800, 800, 3403, 33403, 3845, 33845},    /* DDR2-800 SC */
	{0, 1, 800, 667, 6476, 36476, 6955, 36955},    /* DDR3-667 SC */
	{0, 1, 800, 800, 5958, 35958, 6400, 36400},    /* DDR3-800 SC */

	{0, 0, 667, 400, 3456, 33456, 4103, 34106},    /* DDR2-400 SC */
	{0, 0, 667, 667, 3428, 33428, 3927, 33927},    /* DDR2-667 SC */
	{0, 0, 667, 800, 3443, 33443, 3905, 33905},    /* DDR2-800 SC */
	{0, 1, 667, 667, 6494, 36494, 6993, 36993},    /* DDR3-667 SC */
	{0, 1, 667, 800, 5998, 35998, 6460, 36460},    /* DDR3-800 SC */

	{0, 0, 400, 400, 3528, 33528, 4255, 34255},    /* DDR2-400 SC */
	{0, 0, 400, 667, 3500, 33500, 4079, 34079},    /* DDR2-667 SC */
	{0, 0, 400, 800, 3487, 33487, 4029, 34029},    /* DDR2-800 SC */
	{0, 1, 400, 667, 6566, 36566, 7145, 37145},    /* DDR3-667 SC */
	{0, 1, 400, 800, 6042, 36042, 6584, 36584},    /* DDR3-800 SC */
};

static const struct cxsr_latency *intel_get_cxsr_latency(int is_desktop,
							 int is_ddr3,
							 int fsb,
							 int mem)
{
	const struct cxsr_latency *latency;
	int i;

	if (fsb == 0 || mem == 0)
		return NULL;

	for (i = 0; i < ARRAY_SIZE(cxsr_latency_table); i++) {
		latency = &cxsr_latency_table[i];
		if (is_desktop == latency->is_desktop &&
		    is_ddr3 == latency->is_ddr3 &&
		    fsb == latency->fsb_freq && mem == latency->mem_freq)
			return latency;
	}

	DRM_DEBUG_KMS("Unknown FSB/MEM found, disable CxSR\n");

	return NULL;
}

static void chv_set_memory_dvfs(struct drm_i915_private *dev_priv, bool enable)
{
	u32 val;

	mutex_lock(&dev_priv->rps.hw_lock);

	val = vlv_punit_read(dev_priv, PUNIT_REG_DDR_SETUP2);
	if (enable)
		val &= ~FORCE_DDR_HIGH_FREQ;
	else
		val |= FORCE_DDR_HIGH_FREQ;
	val &= ~FORCE_DDR_LOW_FREQ;
	val |= FORCE_DDR_FREQ_REQ_ACK;
	vlv_punit_write(dev_priv, PUNIT_REG_DDR_SETUP2, val);

	if (wait_for((vlv_punit_read(dev_priv, PUNIT_REG_DDR_SETUP2) &
		      FORCE_DDR_FREQ_REQ_ACK) == 0, 3))
		DRM_ERROR("timed out waiting for Punit DDR DVFS request\n");

	mutex_unlock(&dev_priv->rps.hw_lock);
}

static void chv_set_memory_pm5(struct drm_i915_private *dev_priv, bool enable)
{
	u32 val;

	mutex_lock(&dev_priv->rps.hw_lock);

	val = vlv_punit_read(dev_priv, PUNIT_REG_DSPFREQ);
	if (enable)
		val |= DSP_MAXFIFO_PM5_ENABLE;
	else
		val &= ~DSP_MAXFIFO_PM5_ENABLE;
	vlv_punit_write(dev_priv, PUNIT_REG_DSPFREQ, val);

	mutex_unlock(&dev_priv->rps.hw_lock);
}

#define FW_WM(value, plane) \
	(((value) << DSPFW_ ## plane ## _SHIFT) & DSPFW_ ## plane ## _MASK)

void intel_set_memory_cxsr(struct drm_i915_private *dev_priv, bool enable)
{
	struct drm_device *dev = dev_priv->dev;
	u32 val;

	if (IS_VALLEYVIEW(dev)) {
		I915_WRITE(FW_BLC_SELF_VLV, enable ? FW_CSPWRDWNEN : 0);
		POSTING_READ(FW_BLC_SELF_VLV);
		dev_priv->wm.vlv.cxsr = enable;
	} else if (IS_G4X(dev) || IS_CRESTLINE(dev)) {
		I915_WRITE(FW_BLC_SELF, enable ? FW_BLC_SELF_EN : 0);
		POSTING_READ(FW_BLC_SELF);
	} else if (IS_PINEVIEW(dev)) {
		val = I915_READ(DSPFW3) & ~PINEVIEW_SELF_REFRESH_EN;
		val |= enable ? PINEVIEW_SELF_REFRESH_EN : 0;
		I915_WRITE(DSPFW3, val);
		POSTING_READ(DSPFW3);
	} else if (IS_I945G(dev) || IS_I945GM(dev)) {
		val = enable ? _MASKED_BIT_ENABLE(FW_BLC_SELF_EN) :
			       _MASKED_BIT_DISABLE(FW_BLC_SELF_EN);
		I915_WRITE(FW_BLC_SELF, val);
		POSTING_READ(FW_BLC_SELF);
	} else if (IS_I915GM(dev)) {
		val = enable ? _MASKED_BIT_ENABLE(INSTPM_SELF_EN) :
			       _MASKED_BIT_DISABLE(INSTPM_SELF_EN);
		I915_WRITE(INSTPM, val);
		POSTING_READ(INSTPM);
	} else {
		return;
	}

	DRM_DEBUG_KMS("memory self-refresh is %s\n",
		      enable ? "enabled" : "disabled");
}


/*
 * Latency for FIFO fetches is dependent on several factors:
 *   - memory configuration (speed, channels)
 *   - chipset
 *   - current MCH state
 * It can be fairly high in some situations, so here we assume a fairly
 * pessimal value.  It's a tradeoff between extra memory fetches (if we
 * set this value too high, the FIFO will fetch frequently to stay full)
 * and power consumption (set it too low to save power and we might see
 * FIFO underruns and display "flicker").
 *
 * A value of 5us seems to be a good balance; safe for very low end
 * platforms but not overly aggressive on lower latency configs.
 */
static const int pessimal_latency_ns = 5000;

#define VLV_FIFO_START(dsparb, dsparb2, lo_shift, hi_shift) \
	((((dsparb) >> (lo_shift)) & 0xff) | ((((dsparb2) >> (hi_shift)) & 0x1) << 8))

static int vlv_get_fifo_size(struct drm_device *dev,
			      enum pipe pipe, int plane)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	int sprite0_start, sprite1_start, size;

	switch (pipe) {
		uint32_t dsparb, dsparb2, dsparb3;
	case PIPE_A:
		dsparb = I915_READ(DSPARB);
		dsparb2 = I915_READ(DSPARB2);
		sprite0_start = VLV_FIFO_START(dsparb, dsparb2, 0, 0);
		sprite1_start = VLV_FIFO_START(dsparb, dsparb2, 8, 4);
		break;
	case PIPE_B:
		dsparb = I915_READ(DSPARB);
		dsparb2 = I915_READ(DSPARB2);
		sprite0_start = VLV_FIFO_START(dsparb, dsparb2, 16, 8);
		sprite1_start = VLV_FIFO_START(dsparb, dsparb2, 24, 12);
		break;
	case PIPE_C:
		dsparb2 = I915_READ(DSPARB2);
		dsparb3 = I915_READ(DSPARB3);
		sprite0_start = VLV_FIFO_START(dsparb3, dsparb2, 0, 16);
		sprite1_start = VLV_FIFO_START(dsparb3, dsparb2, 8, 20);
		break;
	default:
		return 0;
	}

	switch (plane) {
	case 0:
		size = sprite0_start;
		break;
	case 1:
		size = sprite1_start - sprite0_start;
		break;
	case 2:
		size = 512 - 1 - sprite1_start;
		break;
	default:
		return 0;
	}

	DRM_DEBUG_KMS("Pipe %c %s %c FIFO size: %d\n",
		      pipe_name(pipe), plane == 0 ? "primary" : "sprite",
		      plane == 0 ? plane_name(pipe) : sprite_name(pipe, plane - 1),
		      size);

	return size;
}

static int i9xx_get_fifo_size(struct drm_device *dev, int plane)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	uint32_t dsparb = I915_READ(DSPARB);
	int size;

	size = dsparb & 0x7f;
	if (plane)
		size = ((dsparb >> DSPARB_CSTART_SHIFT) & 0x7f) - size;

	DRM_DEBUG_KMS("FIFO size - (0x%08x) %s: %d\n", dsparb,
		      plane ? "B" : "A", size);

	return size;
}

static int i830_get_fifo_size(struct drm_device *dev, int plane)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	uint32_t dsparb = I915_READ(DSPARB);
	int size;

	size = dsparb & 0x1ff;
	if (plane)
		size = ((dsparb >> DSPARB_BEND_SHIFT) & 0x1ff) - size;
	size >>= 1; /* Convert to cachelines */

	DRM_DEBUG_KMS("FIFO size - (0x%08x) %s: %d\n", dsparb,
		      plane ? "B" : "A", size);

	return size;
}

static int i845_get_fifo_size(struct drm_device *dev, int plane)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	uint32_t dsparb = I915_READ(DSPARB);
	int size;

	size = dsparb & 0x7f;
	size >>= 2; /* Convert to cachelines */

	DRM_DEBUG_KMS("FIFO size - (0x%08x) %s: %d\n", dsparb,
		      plane ? "B" : "A",
		      size);

	return size;
}

/* Pineview has different values for various configs */
static const struct intel_watermark_params pineview_display_wm = {
	.fifo_size = PINEVIEW_DISPLAY_FIFO,
	.max_wm = PINEVIEW_MAX_WM,
	.default_wm = PINEVIEW_DFT_WM,
	.guard_size = PINEVIEW_GUARD_WM,
	.cacheline_size = PINEVIEW_FIFO_LINE_SIZE,
};
static const struct intel_watermark_params pineview_display_hplloff_wm = {
	.fifo_size = PINEVIEW_DISPLAY_FIFO,
	.max_wm = PINEVIEW_MAX_WM,
	.default_wm = PINEVIEW_DFT_HPLLOFF_WM,
	.guard_size = PINEVIEW_GUARD_WM,
	.cacheline_size = PINEVIEW_FIFO_LINE_SIZE,
};
static const struct intel_watermark_params pineview_cursor_wm = {
	.fifo_size = PINEVIEW_CURSOR_FIFO,
	.max_wm = PINEVIEW_CURSOR_MAX_WM,
	.default_wm = PINEVIEW_CURSOR_DFT_WM,
	.guard_size = PINEVIEW_CURSOR_GUARD_WM,
	.cacheline_size = PINEVIEW_FIFO_LINE_SIZE,
};
static const struct intel_watermark_params pineview_cursor_hplloff_wm = {
	.fifo_size = PINEVIEW_CURSOR_FIFO,
	.max_wm = PINEVIEW_CURSOR_MAX_WM,
	.default_wm = PINEVIEW_CURSOR_DFT_WM,
	.guard_size = PINEVIEW_CURSOR_GUARD_WM,
	.cacheline_size = PINEVIEW_FIFO_LINE_SIZE,
};
static const struct intel_watermark_params g4x_wm_info = {
	.fifo_size = G4X_FIFO_SIZE,
	.max_wm = G4X_MAX_WM,
	.default_wm = G4X_MAX_WM,
	.guard_size = 2,
	.cacheline_size = G4X_FIFO_LINE_SIZE,
};
static const struct intel_watermark_params g4x_cursor_wm_info = {
	.fifo_size = I965_CURSOR_FIFO,
	.max_wm = I965_CURSOR_MAX_WM,
	.default_wm = I965_CURSOR_DFT_WM,
	.guard_size = 2,
	.cacheline_size = G4X_FIFO_LINE_SIZE,
};
static const struct intel_watermark_params valleyview_wm_info = {
	.fifo_size = VALLEYVIEW_FIFO_SIZE,
	.max_wm = VALLEYVIEW_MAX_WM,
	.default_wm = VALLEYVIEW_MAX_WM,
	.guard_size = 2,
	.cacheline_size = G4X_FIFO_LINE_SIZE,
};
static const struct intel_watermark_params valleyview_cursor_wm_info = {
	.fifo_size = I965_CURSOR_FIFO,
	.max_wm = VALLEYVIEW_CURSOR_MAX_WM,
	.default_wm = I965_CURSOR_DFT_WM,
	.guard_size = 2,
	.cacheline_size = G4X_FIFO_LINE_SIZE,
};
static const struct intel_watermark_params i965_cursor_wm_info = {
	.fifo_size = I965_CURSOR_FIFO,
	.max_wm = I965_CURSOR_MAX_WM,
	.default_wm = I965_CURSOR_DFT_WM,
	.guard_size = 2,
	.cacheline_size = I915_FIFO_LINE_SIZE,
};
static const struct intel_watermark_params i945_wm_info = {
	.fifo_size = I945_FIFO_SIZE,
	.max_wm = I915_MAX_WM,
	.default_wm = 1,
	.guard_size = 2,
	.cacheline_size = I915_FIFO_LINE_SIZE,
};
static const struct intel_watermark_params i915_wm_info = {
	.fifo_size = I915_FIFO_SIZE,
	.max_wm = I915_MAX_WM,
	.default_wm = 1,
	.guard_size = 2,
	.cacheline_size = I915_FIFO_LINE_SIZE,
};
static const struct intel_watermark_params i830_a_wm_info = {
	.fifo_size = I855GM_FIFO_SIZE,
	.max_wm = I915_MAX_WM,
	.default_wm = 1,
	.guard_size = 2,
	.cacheline_size = I830_FIFO_LINE_SIZE,
};
static const struct intel_watermark_params i830_bc_wm_info = {
	.fifo_size = I855GM_FIFO_SIZE,
	.max_wm = I915_MAX_WM/2,
	.default_wm = 1,
	.guard_size = 2,
	.cacheline_size = I830_FIFO_LINE_SIZE,
};
static const struct intel_watermark_params i845_wm_info = {
	.fifo_size = I830_FIFO_SIZE,
	.max_wm = I915_MAX_WM,
	.default_wm = 1,
	.guard_size = 2,
	.cacheline_size = I830_FIFO_LINE_SIZE,
};

/**
 * intel_calculate_wm - calculate watermark level
 * @clock_in_khz: pixel clock
 * @wm: chip FIFO params
 * @pixel_size: display pixel size
 * @latency_ns: memory latency for the platform
 *
 * Calculate the watermark level (the level at which the display plane will
 * start fetching from memory again).  Each chip has a different display
 * FIFO size and allocation, so the caller needs to figure that out and pass
 * in the correct intel_watermark_params structure.
 *
 * As the pixel clock runs, the FIFO will be drained at a rate that depends
 * on the pixel size.  When it reaches the watermark level, it'll start
 * fetching FIFO line sized based chunks from memory until the FIFO fills
 * past the watermark point.  If the FIFO drains completely, a FIFO underrun
 * will occur, and a display engine hang could result.
 */
static unsigned long intel_calculate_wm(unsigned long clock_in_khz,
					const struct intel_watermark_params *wm,
					int fifo_size,
					int pixel_size,
					unsigned long latency_ns)
{
	long entries_required, wm_size;

	/*
	 * Note: we need to make sure we don't overflow for various clock &
	 * latency values.
	 * clocks go from a few thousand to several hundred thousand.
	 * latency is usually a few thousand
	 */
	entries_required = ((clock_in_khz / 1000) * pixel_size * latency_ns) /
		1000;
	entries_required = DIV_ROUND_UP(entries_required, wm->cacheline_size);

	DRM_DEBUG_KMS("FIFO entries required for mode: %ld\n", entries_required);

	wm_size = fifo_size - (entries_required + wm->guard_size);

	DRM_DEBUG_KMS("FIFO watermark level: %ld\n", wm_size);

	/* Don't promote wm_size to unsigned... */
	if (wm_size > (long)wm->max_wm)
		wm_size = wm->max_wm;
	if (wm_size <= 0)
		wm_size = wm->default_wm;

	/*
	 * Bspec seems to indicate that the value shouldn't be lower than
	 * 'burst size + 1'. Certainly 830 is quite unhappy with low values.
	 * Lets go for 8 which is the burst size since certain platforms
	 * already use a hardcoded 8 (which is what the spec says should be
	 * done).
	 */
	if (wm_size <= 8)
		wm_size = 8;

	return wm_size;
}

static struct drm_crtc *single_enabled_crtc(struct drm_device *dev)
{
	struct drm_crtc *crtc, *enabled = NULL;

	for_each_crtc(dev, crtc) {
		if (intel_crtc_active(crtc)) {
			if (enabled)
				return NULL;
			enabled = crtc;
		}
	}

	return enabled;
}

static void pineview_update_wm(struct drm_crtc *unused_crtc)
{
	struct drm_device *dev = unused_crtc->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_crtc *crtc;
	const struct cxsr_latency *latency;
	u32 reg;
	unsigned long wm;

	latency = intel_get_cxsr_latency(IS_PINEVIEW_G(dev), dev_priv->is_ddr3,
					 dev_priv->fsb_freq, dev_priv->mem_freq);
	if (!latency) {
		DRM_DEBUG_KMS("Unknown FSB/MEM found, disable CxSR\n");
		intel_set_memory_cxsr(dev_priv, false);
		return;
	}

	crtc = single_enabled_crtc(dev);
	if (crtc) {
		const struct drm_display_mode *adjusted_mode = &to_intel_crtc(crtc)->config->base.adjusted_mode;
		int pixel_size = crtc->primary->state->fb->bits_per_pixel / 8;
		int clock = adjusted_mode->crtc_clock;

		/* Display SR */
		wm = intel_calculate_wm(clock, &pineview_display_wm,
					pineview_display_wm.fifo_size,
					pixel_size, latency->display_sr);
		reg = I915_READ(DSPFW1);
		reg &= ~DSPFW_SR_MASK;
		reg |= FW_WM(wm, SR);
		I915_WRITE(DSPFW1, reg);
		DRM_DEBUG_KMS("DSPFW1 register is %x\n", reg);

		/* cursor SR */
		wm = intel_calculate_wm(clock, &pineview_cursor_wm,
					pineview_display_wm.fifo_size,
					pixel_size, latency->cursor_sr);
		reg = I915_READ(DSPFW3);
		reg &= ~DSPFW_CURSOR_SR_MASK;
		reg |= FW_WM(wm, CURSOR_SR);
		I915_WRITE(DSPFW3, reg);

		/* Display HPLL off SR */
		wm = intel_calculate_wm(clock, &pineview_display_hplloff_wm,
					pineview_display_hplloff_wm.fifo_size,
					pixel_size, latency->display_hpll_disable);
		reg = I915_READ(DSPFW3);
		reg &= ~DSPFW_HPLL_SR_MASK;
		reg |= FW_WM(wm, HPLL_SR);
		I915_WRITE(DSPFW3, reg);

		/* cursor HPLL off SR */
		wm = intel_calculate_wm(clock, &pineview_cursor_hplloff_wm,
					pineview_display_hplloff_wm.fifo_size,
					pixel_size, latency->cursor_hpll_disable);
		reg = I915_READ(DSPFW3);
		reg &= ~DSPFW_HPLL_CURSOR_MASK;
		reg |= FW_WM(wm, HPLL_CURSOR);
		I915_WRITE(DSPFW3, reg);
		DRM_DEBUG_KMS("DSPFW3 register is %x\n", reg);

		intel_set_memory_cxsr(dev_priv, true);
	} else {
		intel_set_memory_cxsr(dev_priv, false);
	}
}

static bool g4x_compute_wm0(struct drm_device *dev,
			    int plane,
			    const struct intel_watermark_params *display,
			    int display_latency_ns,
			    const struct intel_watermark_params *cursor,
			    int cursor_latency_ns,
			    int *plane_wm,
			    int *cursor_wm)
{
	struct drm_crtc *crtc;
	const struct drm_display_mode *adjusted_mode;
	int htotal, hdisplay, clock, pixel_size;
	int line_time_us, line_count;
	int entries, tlb_miss;

	crtc = intel_get_crtc_for_plane(dev, plane);
	if (!intel_crtc_active(crtc)) {
		*cursor_wm = cursor->guard_size;
		*plane_wm = display->guard_size;
		return false;
	}

	adjusted_mode = &to_intel_crtc(crtc)->config->base.adjusted_mode;
	clock = adjusted_mode->crtc_clock;
	htotal = adjusted_mode->crtc_htotal;
	hdisplay = to_intel_crtc(crtc)->config->pipe_src_w;
	pixel_size = crtc->primary->state->fb->bits_per_pixel / 8;

	/* Use the small buffer method to calculate plane watermark */
	entries = ((clock * pixel_size / 1000) * display_latency_ns) / 1000;
	tlb_miss = display->fifo_size*display->cacheline_size - hdisplay * 8;
	if (tlb_miss > 0)
		entries += tlb_miss;
	entries = DIV_ROUND_UP(entries, display->cacheline_size);
	*plane_wm = entries + display->guard_size;
	if (*plane_wm > (int)display->max_wm)
		*plane_wm = display->max_wm;

	/* Use the large buffer method to calculate cursor watermark */
	line_time_us = max(htotal * 1000 / clock, 1);
	line_count = (cursor_latency_ns / line_time_us + 1000) / 1000;
	entries = line_count * crtc->cursor->state->crtc_w * pixel_size;
	tlb_miss = cursor->fifo_size*cursor->cacheline_size - hdisplay * 8;
	if (tlb_miss > 0)
		entries += tlb_miss;
	entries = DIV_ROUND_UP(entries, cursor->cacheline_size);
	*cursor_wm = entries + cursor->guard_size;
	if (*cursor_wm > (int)cursor->max_wm)
		*cursor_wm = (int)cursor->max_wm;

	return true;
}

/*
 * Check the wm result.
 *
 * If any calculated watermark values is larger than the maximum value that
 * can be programmed into the associated watermark register, that watermark
 * must be disabled.
 */
static bool g4x_check_srwm(struct drm_device *dev,
			   int display_wm, int cursor_wm,
			   const struct intel_watermark_params *display,
			   const struct intel_watermark_params *cursor)
{
	DRM_DEBUG_KMS("SR watermark: display plane %d, cursor %d\n",
		      display_wm, cursor_wm);

	if (display_wm > display->max_wm) {
		DRM_DEBUG_KMS("display watermark is too large(%d/%ld), disabling\n",
			      display_wm, display->max_wm);
		return false;
	}

	if (cursor_wm > cursor->max_wm) {
		DRM_DEBUG_KMS("cursor watermark is too large(%d/%ld), disabling\n",
			      cursor_wm, cursor->max_wm);
		return false;
	}

	if (!(display_wm || cursor_wm)) {
		DRM_DEBUG_KMS("SR latency is 0, disabling\n");
		return false;
	}

	return true;
}

static bool g4x_compute_srwm(struct drm_device *dev,
			     int plane,
			     int latency_ns,
			     const struct intel_watermark_params *display,
			     const struct intel_watermark_params *cursor,
			     int *display_wm, int *cursor_wm)
{
	struct drm_crtc *crtc;
	const struct drm_display_mode *adjusted_mode;
	int hdisplay, htotal, pixel_size, clock;
	unsigned long line_time_us;
	int line_count, line_size;
	int small, large;
	int entries;

	if (!latency_ns) {
		*display_wm = *cursor_wm = 0;
		return false;
	}

	crtc = intel_get_crtc_for_plane(dev, plane);
	adjusted_mode = &to_intel_crtc(crtc)->config->base.adjusted_mode;
	clock = adjusted_mode->crtc_clock;
	htotal = adjusted_mode->crtc_htotal;
	hdisplay = to_intel_crtc(crtc)->config->pipe_src_w;
	pixel_size = crtc->primary->state->fb->bits_per_pixel / 8;

	line_time_us = max(htotal * 1000 / clock, 1);
	line_count = (latency_ns / line_time_us + 1000) / 1000;
	line_size = hdisplay * pixel_size;

	/* Use the minimum of the small and large buffer method for primary */
	small = ((clock * pixel_size / 1000) * latency_ns) / 1000;
	large = line_count * line_size;

	entries = DIV_ROUND_UP(min(small, large), display->cacheline_size);
	*display_wm = entries + display->guard_size;

	/* calculate the self-refresh watermark for display cursor */
	entries = line_count * pixel_size * crtc->cursor->state->crtc_w;
	entries = DIV_ROUND_UP(entries, cursor->cacheline_size);
	*cursor_wm = entries + cursor->guard_size;

	return g4x_check_srwm(dev,
			      *display_wm, *cursor_wm,
			      display, cursor);
}

#define FW_WM_VLV(value, plane) \
	(((value) << DSPFW_ ## plane ## _SHIFT) & DSPFW_ ## plane ## _MASK_VLV)

static void vlv_write_wm_values(struct intel_crtc *crtc,
				const struct vlv_wm_values *wm)
{
	struct drm_i915_private *dev_priv = to_i915(crtc->base.dev);
	enum pipe pipe = crtc->pipe;

	I915_WRITE(VLV_DDL(pipe),
		   (wm->ddl[pipe].cursor << DDL_CURSOR_SHIFT) |
		   (wm->ddl[pipe].sprite[1] << DDL_SPRITE_SHIFT(1)) |
		   (wm->ddl[pipe].sprite[0] << DDL_SPRITE_SHIFT(0)) |
		   (wm->ddl[pipe].primary << DDL_PLANE_SHIFT));

	I915_WRITE(DSPFW1,
		   FW_WM(wm->sr.plane, SR) |
		   FW_WM(wm->pipe[PIPE_B].cursor, CURSORB) |
		   FW_WM_VLV(wm->pipe[PIPE_B].primary, PLANEB) |
		   FW_WM_VLV(wm->pipe[PIPE_A].primary, PLANEA));
	I915_WRITE(DSPFW2,
		   FW_WM_VLV(wm->pipe[PIPE_A].sprite[1], SPRITEB) |
		   FW_WM(wm->pipe[PIPE_A].cursor, CURSORA) |
		   FW_WM_VLV(wm->pipe[PIPE_A].sprite[0], SPRITEA));
	I915_WRITE(DSPFW3,
		   FW_WM(wm->sr.cursor, CURSOR_SR));

	if (IS_CHERRYVIEW(dev_priv)) {
		I915_WRITE(DSPFW7_CHV,
			   FW_WM_VLV(wm->pipe[PIPE_B].sprite[1], SPRITED) |
			   FW_WM_VLV(wm->pipe[PIPE_B].sprite[0], SPRITEC));
		I915_WRITE(DSPFW8_CHV,
			   FW_WM_VLV(wm->pipe[PIPE_C].sprite[1], SPRITEF) |
			   FW_WM_VLV(wm->pipe[PIPE_C].sprite[0], SPRITEE));
		I915_WRITE(DSPFW9_CHV,
			   FW_WM_VLV(wm->pipe[PIPE_C].primary, PLANEC) |
			   FW_WM(wm->pipe[PIPE_C].cursor, CURSORC));
		I915_WRITE(DSPHOWM,
			   FW_WM(wm->sr.plane >> 9, SR_HI) |
			   FW_WM(wm->pipe[PIPE_C].sprite[1] >> 8, SPRITEF_HI) |
			   FW_WM(wm->pipe[PIPE_C].sprite[0] >> 8, SPRITEE_HI) |
			   FW_WM(wm->pipe[PIPE_C].primary >> 8, PLANEC_HI) |
			   FW_WM(wm->pipe[PIPE_B].sprite[1] >> 8, SPRITED_HI) |
			   FW_WM(wm->pipe[PIPE_B].sprite[0] >> 8, SPRITEC_HI) |
			   FW_WM(wm->pipe[PIPE_B].primary >> 8, PLANEB_HI) |
			   FW_WM(wm->pipe[PIPE_A].sprite[1] >> 8, SPRITEB_HI) |
			   FW_WM(wm->pipe[PIPE_A].sprite[0] >> 8, SPRITEA_HI) |
			   FW_WM(wm->pipe[PIPE_A].primary >> 8, PLANEA_HI));
	} else {
		I915_WRITE(DSPFW7,
			   FW_WM_VLV(wm->pipe[PIPE_B].sprite[1], SPRITED) |
			   FW_WM_VLV(wm->pipe[PIPE_B].sprite[0], SPRITEC));
		I915_WRITE(DSPHOWM,
			   FW_WM(wm->sr.plane >> 9, SR_HI) |
			   FW_WM(wm->pipe[PIPE_B].sprite[1] >> 8, SPRITED_HI) |
			   FW_WM(wm->pipe[PIPE_B].sprite[0] >> 8, SPRITEC_HI) |
			   FW_WM(wm->pipe[PIPE_B].primary >> 8, PLANEB_HI) |
			   FW_WM(wm->pipe[PIPE_A].sprite[1] >> 8, SPRITEB_HI) |
			   FW_WM(wm->pipe[PIPE_A].sprite[0] >> 8, SPRITEA_HI) |
			   FW_WM(wm->pipe[PIPE_A].primary >> 8, PLANEA_HI));
	}

	/* zero (unused) WM1 watermarks */
	I915_WRITE(DSPFW4, 0);
	I915_WRITE(DSPFW5, 0);
	I915_WRITE(DSPFW6, 0);
	I915_WRITE(DSPHOWM1, 0);

	POSTING_READ(DSPFW1);
}

#undef FW_WM_VLV

enum vlv_wm_level {
	VLV_WM_LEVEL_PM2,
	VLV_WM_LEVEL_PM5,
	VLV_WM_LEVEL_DDR_DVFS,
};

/* latency must be in 0.1us units. */
static unsigned int vlv_wm_method2(unsigned int pixel_rate,
				   unsigned int pipe_htotal,
				   unsigned int horiz_pixels,
				   unsigned int bytes_per_pixel,
				   unsigned int latency)
{
	unsigned int ret;

	ret = (latency * pixel_rate) / (pipe_htotal * 10000);
	ret = (ret + 1) * horiz_pixels * bytes_per_pixel;
	ret = DIV_ROUND_UP(ret, 64);

	return ret;
}

static void vlv_setup_wm_latency(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	/* all latencies in usec */
	dev_priv->wm.pri_latency[VLV_WM_LEVEL_PM2] = 3;

	dev_priv->wm.max_level = VLV_WM_LEVEL_PM2;

	if (IS_CHERRYVIEW(dev_priv)) {
		dev_priv->wm.pri_latency[VLV_WM_LEVEL_PM5] = 12;
		dev_priv->wm.pri_latency[VLV_WM_LEVEL_DDR_DVFS] = 33;

		dev_priv->wm.max_level = VLV_WM_LEVEL_DDR_DVFS;
	}
}

static uint16_t vlv_compute_wm_level(struct intel_plane *plane,
				     struct intel_crtc *crtc,
				     const struct intel_plane_state *state,
				     int level)
{
	struct drm_i915_private *dev_priv = to_i915(plane->base.dev);
	int clock, htotal, pixel_size, width, wm;

	if (dev_priv->wm.pri_latency[level] == 0)
		return USHRT_MAX;

	if (!state->visible)
		return 0;

	pixel_size = drm_format_plane_cpp(state->base.fb->pixel_format, 0);
	clock = crtc->config->base.adjusted_mode.crtc_clock;
	htotal = crtc->config->base.adjusted_mode.crtc_htotal;
	width = crtc->config->pipe_src_w;
	if (WARN_ON(htotal == 0))
		htotal = 1;

	if (plane->base.type == DRM_PLANE_TYPE_CURSOR) {
		/*
		 * FIXME the formula gives values that are
		 * too big for the cursor FIFO, and hence we
		 * would never be able to use cursors. For
		 * now just hardcode the watermark.
		 */
		wm = 63;
	} else {
		wm = vlv_wm_method2(clock, htotal, width, pixel_size,
				    dev_priv->wm.pri_latency[level] * 10);
	}

	return min_t(int, wm, USHRT_MAX);
}

static void vlv_compute_fifo(struct intel_crtc *crtc)
{
	struct drm_device *dev = crtc->base.dev;
	struct vlv_wm_state *wm_state = &crtc->wm_state;
	struct intel_plane *plane;
	unsigned int total_rate = 0;
	const int fifo_size = 512 - 1;
	int fifo_extra, fifo_left = fifo_size;

	for_each_intel_plane_on_crtc(dev, crtc, plane) {
		struct intel_plane_state *state =
			to_intel_plane_state(plane->base.state);

		if (plane->base.type == DRM_PLANE_TYPE_CURSOR)
			continue;

		if (state->visible) {
			wm_state->num_active_planes++;
			total_rate += drm_format_plane_cpp(state->base.fb->pixel_format, 0);
		}
	}

	for_each_intel_plane_on_crtc(dev, crtc, plane) {
		struct intel_plane_state *state =
			to_intel_plane_state(plane->base.state);
		unsigned int rate;

		if (plane->base.type == DRM_PLANE_TYPE_CURSOR) {
			plane->wm.fifo_size = 63;
			continue;
		}

		if (!state->visible) {
			plane->wm.fifo_size = 0;
			continue;
		}

		rate = drm_format_plane_cpp(state->base.fb->pixel_format, 0);
		plane->wm.fifo_size = fifo_size * rate / total_rate;
		fifo_left -= plane->wm.fifo_size;
	}

	fifo_extra = DIV_ROUND_UP(fifo_left, wm_state->num_active_planes ?: 1);

	/* spread the remainder evenly */
	for_each_intel_plane_on_crtc(dev, crtc, plane) {
		int plane_extra;

		if (fifo_left == 0)
			break;

		if (plane->base.type == DRM_PLANE_TYPE_CURSOR)
			continue;

		/* give it all to the first plane if none are active */
		if (plane->wm.fifo_size == 0 &&
		    wm_state->num_active_planes)
			continue;

		plane_extra = min(fifo_extra, fifo_left);
		plane->wm.fifo_size += plane_extra;
		fifo_left -= plane_extra;
	}

	WARN_ON(fifo_left != 0);
}

static void vlv_invert_wms(struct intel_crtc *crtc)
{
	struct vlv_wm_state *wm_state = &crtc->wm_state;
	int level;

	for (level = 0; level < wm_state->num_levels; level++) {
		struct drm_device *dev = crtc->base.dev;
		const int sr_fifo_size = INTEL_INFO(dev)->num_pipes * 512 - 1;
		struct intel_plane *plane;

		wm_state->sr[level].plane = sr_fifo_size - wm_state->sr[level].plane;
		wm_state->sr[level].cursor = 63 - wm_state->sr[level].cursor;

		for_each_intel_plane_on_crtc(dev, crtc, plane) {
			switch (plane->base.type) {
				int sprite;
			case DRM_PLANE_TYPE_CURSOR:
				wm_state->wm[level].cursor = plane->wm.fifo_size -
					wm_state->wm[level].cursor;
				break;
			case DRM_PLANE_TYPE_PRIMARY:
				wm_state->wm[level].primary = plane->wm.fifo_size -
					wm_state->wm[level].primary;
				break;
			case DRM_PLANE_TYPE_OVERLAY:
				sprite = plane->plane;
				wm_state->wm[level].sprite[sprite] = plane->wm.fifo_size -
					wm_state->wm[level].sprite[sprite];
				break;
			}
		}
	}
}

static void vlv_compute_wm(struct intel_crtc *crtc)
{
	struct drm_device *dev = crtc->base.dev;
	struct vlv_wm_state *wm_state = &crtc->wm_state;
	struct intel_plane *plane;
	int sr_fifo_size = INTEL_INFO(dev)->num_pipes * 512 - 1;
	int level;

	memset(wm_state, 0, sizeof(*wm_state));

	wm_state->cxsr = crtc->pipe != PIPE_C && crtc->wm.cxsr_allowed;
	wm_state->num_levels = to_i915(dev)->wm.max_level + 1;

	wm_state->num_active_planes = 0;

	vlv_compute_fifo(crtc);

	if (wm_state->num_active_planes != 1)
		wm_state->cxsr = false;

	if (wm_state->cxsr) {
		for (level = 0; level < wm_state->num_levels; level++) {
			wm_state->sr[level].plane = sr_fifo_size;
			wm_state->sr[level].cursor = 63;
		}
	}

	for_each_intel_plane_on_crtc(dev, crtc, plane) {
		struct intel_plane_state *state =
			to_intel_plane_state(plane->base.state);

		if (!state->visible)
			continue;

		/* normal watermarks */
		for (level = 0; level < wm_state->num_levels; level++) {
			int wm = vlv_compute_wm_level(plane, crtc, state, level);
			int max_wm = plane->base.type == DRM_PLANE_TYPE_CURSOR ? 63 : 511;

			/* hack */
			if (WARN_ON(level == 0 && wm > max_wm))
				wm = max_wm;

			if (wm > plane->wm.fifo_size)
				break;

			switch (plane->base.type) {
				int sprite;
			case DRM_PLANE_TYPE_CURSOR:
				wm_state->wm[level].cursor = wm;
				break;
			case DRM_PLANE_TYPE_PRIMARY:
				wm_state->wm[level].primary = wm;
				break;
			case DRM_PLANE_TYPE_OVERLAY:
				sprite = plane->plane;
				wm_state->wm[level].sprite[sprite] = wm;
				break;
			}
		}

		wm_state->num_levels = level;

		if (!wm_state->cxsr)
			continue;

		/* maxfifo watermarks */
		switch (plane->base.type) {
			int sprite, level;
		case DRM_PLANE_TYPE_CURSOR:
			for (level = 0; level < wm_state->num_levels; level++)
				wm_state->sr[level].cursor =
					wm_state->wm[level].cursor;
			break;
		case DRM_PLANE_TYPE_PRIMARY:
			for (level = 0; level < wm_state->num_levels; level++)
				wm_state->sr[level].plane =
					min(wm_state->sr[level].plane,
					    wm_state->wm[level].primary);
			break;
		case DRM_PLANE_TYPE_OVERLAY:
			sprite = plane->plane;
			for (level = 0; level < wm_state->num_levels; level++)
				wm_state->sr[level].plane =
					min(wm_state->sr[level].plane,
					    wm_state->wm[level].sprite[sprite]);
			break;
		}
	}

	/* clear any (partially) filled invalid levels */
	for (level = wm_state->num_levels; level < to_i915(dev)->wm.max_level + 1; level++) {
		memset(&wm_state->wm[level], 0, sizeof(wm_state->wm[level]));
		memset(&wm_state->sr[level], 0, sizeof(wm_state->sr[level]));
	}

	vlv_invert_wms(crtc);
}

#define VLV_FIFO(plane, value) \
	(((value) << DSPARB_ ## plane ## _SHIFT_VLV) & DSPARB_ ## plane ## _MASK_VLV)

static void vlv_pipe_set_fifo_size(struct intel_crtc *crtc)
{
	struct drm_device *dev = crtc->base.dev;
	struct drm_i915_private *dev_priv = to_i915(dev);
	struct intel_plane *plane;
	int sprite0_start = 0, sprite1_start = 0, fifo_size = 0;

	for_each_intel_plane_on_crtc(dev, crtc, plane) {
		if (plane->base.type == DRM_PLANE_TYPE_CURSOR) {
			WARN_ON(plane->wm.fifo_size != 63);
			continue;
		}

		if (plane->base.type == DRM_PLANE_TYPE_PRIMARY)
			sprite0_start = plane->wm.fifo_size;
		else if (plane->plane == 0)
			sprite1_start = sprite0_start + plane->wm.fifo_size;
		else
			fifo_size = sprite1_start + plane->wm.fifo_size;
	}

	WARN_ON(fifo_size != 512 - 1);

	DRM_DEBUG_KMS("Pipe %c FIFO split %d / %d / %d\n",
		      pipe_name(crtc->pipe), sprite0_start,
		      sprite1_start, fifo_size);

	switch (crtc->pipe) {
		uint32_t dsparb, dsparb2, dsparb3;
	case PIPE_A:
		dsparb = I915_READ(DSPARB);
		dsparb2 = I915_READ(DSPARB2);

		dsparb &= ~(VLV_FIFO(SPRITEA, 0xff) |
			    VLV_FIFO(SPRITEB, 0xff));
		dsparb |= (VLV_FIFO(SPRITEA, sprite0_start) |
			   VLV_FIFO(SPRITEB, sprite1_start));

		dsparb2 &= ~(VLV_FIFO(SPRITEA_HI, 0x1) |
			     VLV_FIFO(SPRITEB_HI, 0x1));
		dsparb2 |= (VLV_FIFO(SPRITEA_HI, sprite0_start >> 8) |
			   VLV_FIFO(SPRITEB_HI, sprite1_start >> 8));

		I915_WRITE(DSPARB, dsparb);
		I915_WRITE(DSPARB2, dsparb2);
		break;
	case PIPE_B:
		dsparb = I915_READ(DSPARB);
		dsparb2 = I915_READ(DSPARB2);

		dsparb &= ~(VLV_FIFO(SPRITEC, 0xff) |
			    VLV_FIFO(SPRITED, 0xff));
		dsparb |= (VLV_FIFO(SPRITEC, sprite0_start) |
			   VLV_FIFO(SPRITED, sprite1_start));

		dsparb2 &= ~(VLV_FIFO(SPRITEC_HI, 0xff) |
			     VLV_FIFO(SPRITED_HI, 0xff));
		dsparb2 |= (VLV_FIFO(SPRITEC_HI, sprite0_start >> 8) |
			   VLV_FIFO(SPRITED_HI, sprite1_start >> 8));

		I915_WRITE(DSPARB, dsparb);
		I915_WRITE(DSPARB2, dsparb2);
		break;
	case PIPE_C:
		dsparb3 = I915_READ(DSPARB3);
		dsparb2 = I915_READ(DSPARB2);

		dsparb3 &= ~(VLV_FIFO(SPRITEE, 0xff) |
			     VLV_FIFO(SPRITEF, 0xff));
		dsparb3 |= (VLV_FIFO(SPRITEE, sprite0_start) |
			    VLV_FIFO(SPRITEF, sprite1_start));

		dsparb2 &= ~(VLV_FIFO(SPRITEE_HI, 0xff) |
			     VLV_FIFO(SPRITEF_HI, 0xff));
		dsparb2 |= (VLV_FIFO(SPRITEE_HI, sprite0_start >> 8) |
			   VLV_FIFO(SPRITEF_HI, sprite1_start >> 8));

		I915_WRITE(DSPARB3, dsparb3);
		I915_WRITE(DSPARB2, dsparb2);
		break;
	default:
		break;
	}
}

#undef VLV_FIFO

static void vlv_merge_wm(struct drm_device *dev,
			 struct vlv_wm_values *wm)
{
	struct intel_crtc *crtc;
	int num_active_crtcs = 0;

	wm->level = to_i915(dev)->wm.max_level;
	wm->cxsr = true;

	for_each_intel_crtc(dev, crtc) {
		const struct vlv_wm_state *wm_state = &crtc->wm_state;

		if (!crtc->active)
			continue;

		if (!wm_state->cxsr)
			wm->cxsr = false;

		num_active_crtcs++;
		wm->level = min_t(int, wm->level, wm_state->num_levels - 1);
	}

	if (num_active_crtcs != 1)
		wm->cxsr = false;

	if (num_active_crtcs > 1)
		wm->level = VLV_WM_LEVEL_PM2;

	for_each_intel_crtc(dev, crtc) {
		struct vlv_wm_state *wm_state = &crtc->wm_state;
		enum pipe pipe = crtc->pipe;

		if (!crtc->active)
			continue;

		wm->pipe[pipe] = wm_state->wm[wm->level];
		if (wm->cxsr)
			wm->sr = wm_state->sr[wm->level];

		wm->ddl[pipe].primary = DDL_PRECISION_HIGH | 2;
		wm->ddl[pipe].sprite[0] = DDL_PRECISION_HIGH | 2;
		wm->ddl[pipe].sprite[1] = DDL_PRECISION_HIGH | 2;
		wm->ddl[pipe].cursor = DDL_PRECISION_HIGH | 2;
	}
}

static void vlv_update_wm(struct drm_crtc *crtc)
{
	struct drm_device *dev = crtc->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_crtc *intel_crtc = to_intel_crtc(crtc);
#ifdef DRMDEBUG
	enum pipe pipe = intel_crtc->pipe;
#endif
	struct vlv_wm_values wm = {};

	vlv_compute_wm(intel_crtc);
	vlv_merge_wm(dev, &wm);

	if (memcmp(&dev_priv->wm.vlv, &wm, sizeof(wm)) == 0) {
		/* FIXME should be part of crtc atomic commit */
		vlv_pipe_set_fifo_size(intel_crtc);
		return;
	}

	if (wm.level < VLV_WM_LEVEL_DDR_DVFS &&
	    dev_priv->wm.vlv.level >= VLV_WM_LEVEL_DDR_DVFS)
		chv_set_memory_dvfs(dev_priv, false);

	if (wm.level < VLV_WM_LEVEL_PM5 &&
	    dev_priv->wm.vlv.level >= VLV_WM_LEVEL_PM5)
		chv_set_memory_pm5(dev_priv, false);

	if (!wm.cxsr && dev_priv->wm.vlv.cxsr)
		intel_set_memory_cxsr(dev_priv, false);

	/* FIXME should be part of crtc atomic commit */
	vlv_pipe_set_fifo_size(intel_crtc);

	vlv_write_wm_values(intel_crtc, &wm);

	DRM_DEBUG_KMS("Setting FIFO watermarks - %c: plane=%d, cursor=%d, "
		      "sprite0=%d, sprite1=%d, SR: plane=%d, cursor=%d level=%d cxsr=%d\n",
		      pipe_name(pipe), wm.pipe[pipe].primary, wm.pipe[pipe].cursor,
		      wm.pipe[pipe].sprite[0], wm.pipe[pipe].sprite[1],
		      wm.sr.plane, wm.sr.cursor, wm.level, wm.cxsr);

	if (wm.cxsr && !dev_priv->wm.vlv.cxsr)
		intel_set_memory_cxsr(dev_priv, true);

	if (wm.level >= VLV_WM_LEVEL_PM5 &&
	    dev_priv->wm.vlv.level < VLV_WM_LEVEL_PM5)
		chv_set_memory_pm5(dev_priv, true);

	if (wm.level >= VLV_WM_LEVEL_DDR_DVFS &&
	    dev_priv->wm.vlv.level < VLV_WM_LEVEL_DDR_DVFS)
		chv_set_memory_dvfs(dev_priv, true);

	dev_priv->wm.vlv = wm;
}

#define single_plane_enabled(mask) is_power_of_2(mask)

static void g4x_update_wm(struct drm_crtc *crtc)
{
	struct drm_device *dev = crtc->dev;
	static const int sr_latency_ns = 12000;
	struct drm_i915_private *dev_priv = dev->dev_private;
	int planea_wm, planeb_wm, cursora_wm, cursorb_wm;
	int plane_sr, cursor_sr;
	unsigned int enabled = 0;
	bool cxsr_enabled;

	if (g4x_compute_wm0(dev, PIPE_A,
			    &g4x_wm_info, pessimal_latency_ns,
			    &g4x_cursor_wm_info, pessimal_latency_ns,
			    &planea_wm, &cursora_wm))
		enabled |= 1 << PIPE_A;

	if (g4x_compute_wm0(dev, PIPE_B,
			    &g4x_wm_info, pessimal_latency_ns,
			    &g4x_cursor_wm_info, pessimal_latency_ns,
			    &planeb_wm, &cursorb_wm))
		enabled |= 1 << PIPE_B;

	if (single_plane_enabled(enabled) &&
	    g4x_compute_srwm(dev, ffs(enabled) - 1,
			     sr_latency_ns,
			     &g4x_wm_info,
			     &g4x_cursor_wm_info,
			     &plane_sr, &cursor_sr)) {
		cxsr_enabled = true;
	} else {
		cxsr_enabled = false;
		intel_set_memory_cxsr(dev_priv, false);
		plane_sr = cursor_sr = 0;
	}

	DRM_DEBUG_KMS("Setting FIFO watermarks - A: plane=%d, cursor=%d, "
		      "B: plane=%d, cursor=%d, SR: plane=%d, cursor=%d\n",
		      planea_wm, cursora_wm,
		      planeb_wm, cursorb_wm,
		      plane_sr, cursor_sr);

	I915_WRITE(DSPFW1,
		   FW_WM(plane_sr, SR) |
		   FW_WM(cursorb_wm, CURSORB) |
		   FW_WM(planeb_wm, PLANEB) |
		   FW_WM(planea_wm, PLANEA));
	I915_WRITE(DSPFW2,
		   (I915_READ(DSPFW2) & ~DSPFW_CURSORA_MASK) |
		   FW_WM(cursora_wm, CURSORA));
	/* HPLL off in SR has some issues on G4x... disable it */
	I915_WRITE(DSPFW3,
		   (I915_READ(DSPFW3) & ~(DSPFW_HPLL_SR_EN | DSPFW_CURSOR_SR_MASK)) |
		   FW_WM(cursor_sr, CURSOR_SR));

	if (cxsr_enabled)
		intel_set_memory_cxsr(dev_priv, true);
}

static void i965_update_wm(struct drm_crtc *unused_crtc)
{
	struct drm_device *dev = unused_crtc->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_crtc *crtc;
	int srwm = 1;
	int cursor_sr = 16;
	bool cxsr_enabled;

	/* Calc sr entries for one plane configs */
	crtc = single_enabled_crtc(dev);
	if (crtc) {
		/* self-refresh has much higher latency */
		static const int sr_latency_ns = 12000;
		const struct drm_display_mode *adjusted_mode = &to_intel_crtc(crtc)->config->base.adjusted_mode;
		int clock = adjusted_mode->crtc_clock;
		int htotal = adjusted_mode->crtc_htotal;
		int hdisplay = to_intel_crtc(crtc)->config->pipe_src_w;
		int pixel_size = crtc->primary->state->fb->bits_per_pixel / 8;
		unsigned long line_time_us;
		int entries;

		line_time_us = max(htotal * 1000 / clock, 1);

		/* Use ns/us then divide to preserve precision */
		entries = (((sr_latency_ns / line_time_us) + 1000) / 1000) *
			pixel_size * hdisplay;
		entries = DIV_ROUND_UP(entries, I915_FIFO_LINE_SIZE);
		srwm = I965_FIFO_SIZE - entries;
		if (srwm < 0)
			srwm = 1;
		srwm &= 0x1ff;
		DRM_DEBUG_KMS("self-refresh entries: %d, wm: %d\n",
			      entries, srwm);

		entries = (((sr_latency_ns / line_time_us) + 1000) / 1000) *
			pixel_size * crtc->cursor->state->crtc_w;
		entries = DIV_ROUND_UP(entries,
					  i965_cursor_wm_info.cacheline_size);
		cursor_sr = i965_cursor_wm_info.fifo_size -
			(entries + i965_cursor_wm_info.guard_size);

		if (cursor_sr > i965_cursor_wm_info.max_wm)
			cursor_sr = i965_cursor_wm_info.max_wm;

		DRM_DEBUG_KMS("self-refresh watermark: display plane %d "
			      "cursor %d\n", srwm, cursor_sr);

		cxsr_enabled = true;
	} else {
		cxsr_enabled = false;
		/* Turn off self refresh if both pipes are enabled */
		intel_set_memory_cxsr(dev_priv, false);
	}

	DRM_DEBUG_KMS("Setting FIFO watermarks - A: 8, B: 8, C: 8, SR %d\n",
		      srwm);

	/* 965 has limitations... */
	I915_WRITE(DSPFW1, FW_WM(srwm, SR) |
		   FW_WM(8, CURSORB) |
		   FW_WM(8, PLANEB) |
		   FW_WM(8, PLANEA));
	I915_WRITE(DSPFW2, FW_WM(8, CURSORA) |
		   FW_WM(8, PLANEC_OLD));
	/* update cursor SR watermark */
	I915_WRITE(DSPFW3, FW_WM(cursor_sr, CURSOR_SR));

	if (cxsr_enabled)
		intel_set_memory_cxsr(dev_priv, true);
}

#undef FW_WM

static void i9xx_update_wm(struct drm_crtc *unused_crtc)
{
	struct drm_device *dev = unused_crtc->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	const struct intel_watermark_params *wm_info;
	uint32_t fwater_lo;
	uint32_t fwater_hi;
	int cwm, srwm = 1;
	int fifo_size;
	int planea_wm, planeb_wm;
	struct drm_crtc *crtc, *enabled = NULL;

	if (IS_I945GM(dev))
		wm_info = &i945_wm_info;
	else if (!IS_GEN2(dev))
		wm_info = &i915_wm_info;
	else
		wm_info = &i830_a_wm_info;

	fifo_size = dev_priv->display.get_fifo_size(dev, 0);
	crtc = intel_get_crtc_for_plane(dev, 0);
	if (intel_crtc_active(crtc)) {
		const struct drm_display_mode *adjusted_mode;
		int cpp = crtc->primary->state->fb->bits_per_pixel / 8;
		if (IS_GEN2(dev))
			cpp = 4;

		adjusted_mode = &to_intel_crtc(crtc)->config->base.adjusted_mode;
		planea_wm = intel_calculate_wm(adjusted_mode->crtc_clock,
					       wm_info, fifo_size, cpp,
					       pessimal_latency_ns);
		enabled = crtc;
	} else {
		planea_wm = fifo_size - wm_info->guard_size;
		if (planea_wm > (long)wm_info->max_wm)
			planea_wm = wm_info->max_wm;
	}

	if (IS_GEN2(dev))
		wm_info = &i830_bc_wm_info;

	fifo_size = dev_priv->display.get_fifo_size(dev, 1);
	crtc = intel_get_crtc_for_plane(dev, 1);
	if (intel_crtc_active(crtc)) {
		const struct drm_display_mode *adjusted_mode;
		int cpp = crtc->primary->state->fb->bits_per_pixel / 8;
		if (IS_GEN2(dev))
			cpp = 4;

		adjusted_mode = &to_intel_crtc(crtc)->config->base.adjusted_mode;
		planeb_wm = intel_calculate_wm(adjusted_mode->crtc_clock,
					       wm_info, fifo_size, cpp,
					       pessimal_latency_ns);
		if (enabled == NULL)
			enabled = crtc;
		else
			enabled = NULL;
	} else {
		planeb_wm = fifo_size - wm_info->guard_size;
		if (planeb_wm > (long)wm_info->max_wm)
			planeb_wm = wm_info->max_wm;
	}

	DRM_DEBUG_KMS("FIFO watermarks - A: %d, B: %d\n", planea_wm, planeb_wm);

	if (IS_I915GM(dev) && enabled) {
		struct drm_i915_gem_object *obj;

		obj = intel_fb_obj(enabled->primary->state->fb);

		/* self-refresh seems busted with untiled */
		if (obj->tiling_mode == I915_TILING_NONE)
			enabled = NULL;
	}

	/*
	 * Overlay gets an aggressive default since video jitter is bad.
	 */
	cwm = 2;

	/* Play safe and disable self-refresh before adjusting watermarks. */
	intel_set_memory_cxsr(dev_priv, false);

	/* Calc sr entries for one plane configs */
	if (HAS_FW_BLC(dev) && enabled) {
		/* self-refresh has much higher latency */
		static const int sr_latency_ns = 6000;
		const struct drm_display_mode *adjusted_mode = &to_intel_crtc(enabled)->config->base.adjusted_mode;
		int clock = adjusted_mode->crtc_clock;
		int htotal = adjusted_mode->crtc_htotal;
		int hdisplay = to_intel_crtc(enabled)->config->pipe_src_w;
		int pixel_size = enabled->primary->state->fb->bits_per_pixel / 8;
		unsigned long line_time_us;
		int entries;

		line_time_us = max(htotal * 1000 / clock, 1);

		/* Use ns/us then divide to preserve precision */
		entries = (((sr_latency_ns / line_time_us) + 1000) / 1000) *
			pixel_size * hdisplay;
		entries = DIV_ROUND_UP(entries, wm_info->cacheline_size);
		DRM_DEBUG_KMS("self-refresh entries: %d\n", entries);
		srwm = wm_info->fifo_size - entries;
		if (srwm < 0)
			srwm = 1;

		if (IS_I945G(dev) || IS_I945GM(dev))
			I915_WRITE(FW_BLC_SELF,
				   FW_BLC_SELF_FIFO_MASK | (srwm & 0xff));
		else if (IS_I915GM(dev))
			I915_WRITE(FW_BLC_SELF, srwm & 0x3f);
	}

	DRM_DEBUG_KMS("Setting FIFO watermarks - A: %d, B: %d, C: %d, SR %d\n",
		      planea_wm, planeb_wm, cwm, srwm);

	fwater_lo = ((planeb_wm & 0x3f) << 16) | (planea_wm & 0x3f);
	fwater_hi = (cwm & 0x1f);

	/* Set request length to 8 cachelines per fetch */
	fwater_lo = fwater_lo | (1 << 24) | (1 << 8);
	fwater_hi = fwater_hi | (1 << 8);

	I915_WRITE(FW_BLC, fwater_lo);
	I915_WRITE(FW_BLC2, fwater_hi);

	if (enabled)
		intel_set_memory_cxsr(dev_priv, true);
}

static void i845_update_wm(struct drm_crtc *unused_crtc)
{
	struct drm_device *dev = unused_crtc->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_crtc *crtc;
	const struct drm_display_mode *adjusted_mode;
	uint32_t fwater_lo;
	int planea_wm;

	crtc = single_enabled_crtc(dev);
	if (crtc == NULL)
		return;

	adjusted_mode = &to_intel_crtc(crtc)->config->base.adjusted_mode;
	planea_wm = intel_calculate_wm(adjusted_mode->crtc_clock,
				       &i845_wm_info,
				       dev_priv->display.get_fifo_size(dev, 0),
				       4, pessimal_latency_ns);
	fwater_lo = I915_READ(FW_BLC) & ~0xfff;
	fwater_lo |= (3<<8) | planea_wm;

	DRM_DEBUG_KMS("Setting FIFO watermarks - A: %d\n", planea_wm);

	I915_WRITE(FW_BLC, fwater_lo);
}

uint32_t ilk_pipe_pixel_rate(const struct intel_crtc_state *pipe_config)
{
	uint32_t pixel_rate;

	pixel_rate = pipe_config->base.adjusted_mode.crtc_clock;

	/* We only use IF-ID interlacing. If we ever use PF-ID we'll need to
	 * adjust the pixel_rate here. */

	if (pipe_config->pch_pfit.enabled) {
		uint64_t pipe_w, pipe_h, pfit_w, pfit_h;
		uint32_t pfit_size = pipe_config->pch_pfit.size;

		pipe_w = pipe_config->pipe_src_w;
		pipe_h = pipe_config->pipe_src_h;

		pfit_w = (pfit_size >> 16) & 0xFFFF;
		pfit_h = pfit_size & 0xFFFF;
		if (pipe_w < pfit_w)
			pipe_w = pfit_w;
		if (pipe_h < pfit_h)
			pipe_h = pfit_h;

		pixel_rate = div_u64((uint64_t) pixel_rate * pipe_w * pipe_h,
				     pfit_w * pfit_h);
	}

	return pixel_rate;
}

/* latency must be in 0.1us units. */
static uint32_t ilk_wm_method1(uint32_t pixel_rate, uint8_t bytes_per_pixel,
			       uint32_t latency)
{
	uint64_t ret;

	if (WARN(latency == 0, "Latency value missing\n"))
		return UINT_MAX;

	ret = (uint64_t) pixel_rate * bytes_per_pixel * latency;
	ret = DIV_ROUND_UP_ULL(ret, 64 * 10000) + 2;

	return ret;
}

/* latency must be in 0.1us units. */
static uint32_t ilk_wm_method2(uint32_t pixel_rate, uint32_t pipe_htotal,
			       uint32_t horiz_pixels, uint8_t bytes_per_pixel,
			       uint32_t latency)
{
	uint32_t ret;

	if (WARN(latency == 0, "Latency value missing\n"))
		return UINT_MAX;

	ret = (latency * pixel_rate) / (pipe_htotal * 10000);
	ret = (ret + 1) * horiz_pixels * bytes_per_pixel;
	ret = DIV_ROUND_UP(ret, 64) + 2;
	return ret;
}

static uint32_t ilk_wm_fbc(uint32_t pri_val, uint32_t horiz_pixels,
			   uint8_t bytes_per_pixel)
{
	return DIV_ROUND_UP(pri_val * 64, horiz_pixels * bytes_per_pixel) + 2;
}

struct skl_pipe_wm_parameters {
	bool active;
	uint32_t pipe_htotal;
	uint32_t pixel_rate; /* in KHz */
	struct intel_plane_wm_parameters plane[I915_MAX_PLANES];
};

struct ilk_wm_maximums {
	uint16_t pri;
	uint16_t spr;
	uint16_t cur;
	uint16_t fbc;
};

/* used in computing the new watermarks state */
struct intel_wm_config {
	unsigned int num_pipes_active;
	bool sprites_enabled;
	bool sprites_scaled;
};

/*
 * For both WM_PIPE and WM_LP.
 * mem_value must be in 0.1us units.
 */
static uint32_t ilk_compute_pri_wm(const struct intel_crtc_state *cstate,
				   const struct intel_plane_state *pstate,
				   uint32_t mem_value,
				   bool is_lp)
{
	int bpp = pstate->base.fb ? pstate->base.fb->bits_per_pixel / 8 : 0;
	uint32_t method1, method2;

	if (!cstate->base.active || !pstate->visible)
		return 0;

	method1 = ilk_wm_method1(ilk_pipe_pixel_rate(cstate), bpp, mem_value);

	if (!is_lp)
		return method1;

	method2 = ilk_wm_method2(ilk_pipe_pixel_rate(cstate),
				 cstate->base.adjusted_mode.crtc_htotal,
				 drm_rect_width(&pstate->dst),
				 bpp,
				 mem_value);

	return min(method1, method2);
}

/*
 * For both WM_PIPE and WM_LP.
 * mem_value must be in 0.1us units.
 */
static uint32_t ilk_compute_spr_wm(const struct intel_crtc_state *cstate,
				   const struct intel_plane_state *pstate,
				   uint32_t mem_value)
{
	int bpp = pstate->base.fb ? pstate->base.fb->bits_per_pixel / 8 : 0;
	uint32_t method1, method2;

	if (!cstate->base.active || !pstate->visible)
		return 0;

	method1 = ilk_wm_method1(ilk_pipe_pixel_rate(cstate), bpp, mem_value);
	method2 = ilk_wm_method2(ilk_pipe_pixel_rate(cstate),
				 cstate->base.adjusted_mode.crtc_htotal,
				 drm_rect_width(&pstate->dst),
				 bpp,
				 mem_value);
	return min(method1, method2);
}

/*
 * For both WM_PIPE and WM_LP.
 * mem_value must be in 0.1us units.
 */
static uint32_t ilk_compute_cur_wm(const struct intel_crtc_state *cstate,
				   const struct intel_plane_state *pstate,
				   uint32_t mem_value)
{
	/*
	 * We treat the cursor plane as always-on for the purposes of watermark
	 * calculation.  Until we have two-stage watermark programming merged,
	 * this is necessary to avoid flickering.
	 */
	int cpp = 4;
	int width = pstate->visible ? pstate->base.crtc_w : 64;

	if (!cstate->base.active)
		return 0;

	return ilk_wm_method2(ilk_pipe_pixel_rate(cstate),
			      cstate->base.adjusted_mode.crtc_htotal,
			      width, cpp, mem_value);
}

/* Only for WM_LP. */
static uint32_t ilk_compute_fbc_wm(const struct intel_crtc_state *cstate,
				   const struct intel_plane_state *pstate,
				   uint32_t pri_val)
{
	int bpp = pstate->base.fb ? pstate->base.fb->bits_per_pixel / 8 : 0;

	if (!cstate->base.active || !pstate->visible)
		return 0;

	return ilk_wm_fbc(pri_val, drm_rect_width(&pstate->dst), bpp);
}

static unsigned int ilk_display_fifo_size(const struct drm_device *dev)
{
	if (INTEL_INFO(dev)->gen >= 8)
		return 3072;
	else if (INTEL_INFO(dev)->gen >= 7)
		return 768;
	else
		return 512;
}

static unsigned int ilk_plane_wm_reg_max(const struct drm_device *dev,
					 int level, bool is_sprite)
{
	if (INTEL_INFO(dev)->gen >= 8)
		/* BDW primary/sprite plane watermarks */
		return level == 0 ? 255 : 2047;
	else if (INTEL_INFO(dev)->gen >= 7)
		/* IVB/HSW primary/sprite plane watermarks */
		return level == 0 ? 127 : 1023;
	else if (!is_sprite)
		/* ILK/SNB primary plane watermarks */
		return level == 0 ? 127 : 511;
	else
		/* ILK/SNB sprite plane watermarks */
		return level == 0 ? 63 : 255;
}

static unsigned int ilk_cursor_wm_reg_max(const struct drm_device *dev,
					  int level)
{
	if (INTEL_INFO(dev)->gen >= 7)
		return level == 0 ? 63 : 255;
	else
		return level == 0 ? 31 : 63;
}

static unsigned int ilk_fbc_wm_reg_max(const struct drm_device *dev)
{
	if (INTEL_INFO(dev)->gen >= 8)
		return 31;
	else
		return 15;
}

/* Calculate the maximum primary/sprite plane watermark */
static unsigned int ilk_plane_wm_max(const struct drm_device *dev,
				     int level,
				     const struct intel_wm_config *config,
				     enum intel_ddb_partitioning ddb_partitioning,
				     bool is_sprite)
{
	unsigned int fifo_size = ilk_display_fifo_size(dev);

	/* if sprites aren't enabled, sprites get nothing */
	if (is_sprite && !config->sprites_enabled)
		return 0;

	/* HSW allows LP1+ watermarks even with multiple pipes */
	if (level == 0 || config->num_pipes_active > 1) {
		fifo_size /= INTEL_INFO(dev)->num_pipes;

		/*
		 * For some reason the non self refresh
		 * FIFO size is only half of the self
		 * refresh FIFO size on ILK/SNB.
		 */
		if (INTEL_INFO(dev)->gen <= 6)
			fifo_size /= 2;
	}

	if (config->sprites_enabled) {
		/* level 0 is always calculated with 1:1 split */
		if (level > 0 && ddb_partitioning == INTEL_DDB_PART_5_6) {
			if (is_sprite)
				fifo_size *= 5;
			fifo_size /= 6;
		} else {
			fifo_size /= 2;
		}
	}

	/* clamp to max that the registers can hold */
	return min(fifo_size, ilk_plane_wm_reg_max(dev, level, is_sprite));
}

/* Calculate the maximum cursor plane watermark */
static unsigned int ilk_cursor_wm_max(const struct drm_device *dev,
				      int level,
				      const struct intel_wm_config *config)
{
	/* HSW LP1+ watermarks w/ multiple pipes */
	if (level > 0 && config->num_pipes_active > 1)
		return 64;

	/* otherwise just report max that registers can hold */
	return ilk_cursor_wm_reg_max(dev, level);
}

static void ilk_compute_wm_maximums(const struct drm_device *dev,
				    int level,
				    const struct intel_wm_config *config,
				    enum intel_ddb_partitioning ddb_partitioning,
				    struct ilk_wm_maximums *max)
{
	max->pri = ilk_plane_wm_max(dev, level, config, ddb_partitioning, false);
	max->spr = ilk_plane_wm_max(dev, level, config, ddb_partitioning, true);
	max->cur = ilk_cursor_wm_max(dev, level, config);
	max->fbc = ilk_fbc_wm_reg_max(dev);
}

static void ilk_compute_wm_reg_maximums(struct drm_device *dev,
					int level,
					struct ilk_wm_maximums *max)
{
	max->pri = ilk_plane_wm_reg_max(dev, level, false);
	max->spr = ilk_plane_wm_reg_max(dev, level, true);
	max->cur = ilk_cursor_wm_reg_max(dev, level);
	max->fbc = ilk_fbc_wm_reg_max(dev);
}

static bool ilk_validate_wm_level(int level,
				  const struct ilk_wm_maximums *max,
				  struct intel_wm_level *result)
{
	bool ret;

	/* already determined to be invalid? */
	if (!result->enable)
		return false;

	result->enable = result->pri_val <= max->pri &&
			 result->spr_val <= max->spr &&
			 result->cur_val <= max->cur;

	ret = result->enable;

	/*
	 * HACK until we can pre-compute everything,
	 * and thus fail gracefully if LP0 watermarks
	 * are exceeded...
	 */
	if (level == 0 && !result->enable) {
		if (result->pri_val > max->pri)
			DRM_DEBUG_KMS("Primary WM%d too large %u (max %u)\n",
				      level, result->pri_val, max->pri);
		if (result->spr_val > max->spr)
			DRM_DEBUG_KMS("Sprite WM%d too large %u (max %u)\n",
				      level, result->spr_val, max->spr);
		if (result->cur_val > max->cur)
			DRM_DEBUG_KMS("Cursor WM%d too large %u (max %u)\n",
				      level, result->cur_val, max->cur);

		result->pri_val = min_t(uint32_t, result->pri_val, max->pri);
		result->spr_val = min_t(uint32_t, result->spr_val, max->spr);
		result->cur_val = min_t(uint32_t, result->cur_val, max->cur);
		result->enable = true;
	}

	return ret;
}

static void ilk_compute_wm_level(const struct drm_i915_private *dev_priv,
				 const struct intel_crtc *intel_crtc,
				 int level,
				 struct intel_crtc_state *cstate,
				 struct intel_wm_level *result)
{
	struct intel_plane *intel_plane;
	uint16_t pri_latency = dev_priv->wm.pri_latency[level];
	uint16_t spr_latency = dev_priv->wm.spr_latency[level];
	uint16_t cur_latency = dev_priv->wm.cur_latency[level];

	/* WM1+ latency values stored in 0.5us units */
	if (level > 0) {
		pri_latency *= 5;
		spr_latency *= 5;
		cur_latency *= 5;
	}

	for_each_intel_plane_on_crtc(dev_priv->dev, intel_crtc, intel_plane) {
		struct intel_plane_state *pstate =
			to_intel_plane_state(intel_plane->base.state);

		switch (intel_plane->base.type) {
		case DRM_PLANE_TYPE_PRIMARY:
			result->pri_val = ilk_compute_pri_wm(cstate, pstate,
							     pri_latency,
							     level);
			result->fbc_val = ilk_compute_fbc_wm(cstate, pstate,
							     result->pri_val);
			break;
		case DRM_PLANE_TYPE_OVERLAY:
			result->spr_val = ilk_compute_spr_wm(cstate, pstate,
							     spr_latency);
			break;
		case DRM_PLANE_TYPE_CURSOR:
			result->cur_val = ilk_compute_cur_wm(cstate, pstate,
							     cur_latency);
			break;
		}
	}

	result->enable = true;
}

static uint32_t
hsw_compute_linetime_wm(struct drm_device *dev, struct drm_crtc *crtc)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_crtc *intel_crtc = to_intel_crtc(crtc);
	const struct drm_display_mode *adjusted_mode = &intel_crtc->config->base.adjusted_mode;
	u32 linetime, ips_linetime;

	if (!intel_crtc->active)
		return 0;

	/* The WM are computed with base on how long it takes to fill a single
	 * row at the given clock rate, multiplied by 8.
	 * */
	linetime = DIV_ROUND_CLOSEST(adjusted_mode->crtc_htotal * 1000 * 8,
				     adjusted_mode->crtc_clock);
	ips_linetime = DIV_ROUND_CLOSEST(adjusted_mode->crtc_htotal * 1000 * 8,
					 dev_priv->cdclk_freq);

	return PIPE_WM_LINETIME_IPS_LINETIME(ips_linetime) |
	       PIPE_WM_LINETIME_TIME(linetime);
}

static void intel_read_wm_latency(struct drm_device *dev, uint16_t wm[8])
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	if (IS_GEN9(dev)) {
		uint32_t val;
		int ret, i;
		int level, max_level = ilk_wm_max_level(dev);

		/* read the first set of memory latencies[0:3] */
		val = 0; /* data0 to be programmed to 0 for first set */
		mutex_lock(&dev_priv->rps.hw_lock);
		ret = sandybridge_pcode_read(dev_priv,
					     GEN9_PCODE_READ_MEM_LATENCY,
					     &val);
		mutex_unlock(&dev_priv->rps.hw_lock);

		if (ret) {
			DRM_ERROR("SKL Mailbox read error = %d\n", ret);
			return;
		}

		wm[0] = val & GEN9_MEM_LATENCY_LEVEL_MASK;
		wm[1] = (val >> GEN9_MEM_LATENCY_LEVEL_1_5_SHIFT) &
				GEN9_MEM_LATENCY_LEVEL_MASK;
		wm[2] = (val >> GEN9_MEM_LATENCY_LEVEL_2_6_SHIFT) &
				GEN9_MEM_LATENCY_LEVEL_MASK;
		wm[3] = (val >> GEN9_MEM_LATENCY_LEVEL_3_7_SHIFT) &
				GEN9_MEM_LATENCY_LEVEL_MASK;

		/* read the second set of memory latencies[4:7] */
		val = 1; /* data0 to be programmed to 1 for second set */
		mutex_lock(&dev_priv->rps.hw_lock);
		ret = sandybridge_pcode_read(dev_priv,
					     GEN9_PCODE_READ_MEM_LATENCY,
					     &val);
		mutex_unlock(&dev_priv->rps.hw_lock);
		if (ret) {
			DRM_ERROR("SKL Mailbox read error = %d\n", ret);
			return;
		}

		wm[4] = val & GEN9_MEM_LATENCY_LEVEL_MASK;
		wm[5] = (val >> GEN9_MEM_LATENCY_LEVEL_1_5_SHIFT) &
				GEN9_MEM_LATENCY_LEVEL_MASK;
		wm[6] = (val >> GEN9_MEM_LATENCY_LEVEL_2_6_SHIFT) &
				GEN9_MEM_LATENCY_LEVEL_MASK;
		wm[7] = (val >> GEN9_MEM_LATENCY_LEVEL_3_7_SHIFT) &
				GEN9_MEM_LATENCY_LEVEL_MASK;

		/*
		 * If a level n (n > 1) has a 0us latency, all levels m (m >= n)
		 * need to be disabled. We make sure to sanitize the values out
		 * of the punit to satisfy this requirement.
		 */
		for (level = 1; level <= max_level; level++) {
			if (wm[level] == 0) {
				for (i = level + 1; i <= max_level; i++)
					wm[i] = 0;
				break;
			}
		}

		/*
		 * WaWmMemoryReadLatency:skl
		 *
		 * punit doesn't take into account the read latency so we need
		 * to add 2us to the various latency levels we retrieve from the
		 * punit when level 0 response data us 0us.
		 */
		if (wm[0] == 0) {
			wm[0] += 2;
			for (level = 1; level <= max_level; level++) {
				if (wm[level] == 0)
					break;
				wm[level] += 2;
			}
		}

	} else if (IS_HASWELL(dev) || IS_BROADWELL(dev)) {
		uint64_t sskpd = I915_READ64(MCH_SSKPD);

		wm[0] = (sskpd >> 56) & 0xFF;
		if (wm[0] == 0)
			wm[0] = sskpd & 0xF;
		wm[1] = (sskpd >> 4) & 0xFF;
		wm[2] = (sskpd >> 12) & 0xFF;
		wm[3] = (sskpd >> 20) & 0x1FF;
		wm[4] = (sskpd >> 32) & 0x1FF;
	} else if (INTEL_INFO(dev)->gen >= 6) {
		uint32_t sskpd = I915_READ(MCH_SSKPD);

		wm[0] = (sskpd >> SSKPD_WM0_SHIFT) & SSKPD_WM_MASK;
		wm[1] = (sskpd >> SSKPD_WM1_SHIFT) & SSKPD_WM_MASK;
		wm[2] = (sskpd >> SSKPD_WM2_SHIFT) & SSKPD_WM_MASK;
		wm[3] = (sskpd >> SSKPD_WM3_SHIFT) & SSKPD_WM_MASK;
	} else if (INTEL_INFO(dev)->gen >= 5) {
		uint32_t mltr = I915_READ(MLTR_ILK);

		/* ILK primary LP0 latency is 700 ns */
		wm[0] = 7;
		wm[1] = (mltr >> MLTR_WM1_SHIFT) & ILK_SRLT_MASK;
		wm[2] = (mltr >> MLTR_WM2_SHIFT) & ILK_SRLT_MASK;
	}
}

static void intel_fixup_spr_wm_latency(struct drm_device *dev, uint16_t wm[5])
{
	/* ILK sprite LP0 latency is 1300 ns */
	if (INTEL_INFO(dev)->gen == 5)
		wm[0] = 13;
}

static void intel_fixup_cur_wm_latency(struct drm_device *dev, uint16_t wm[5])
{
	/* ILK cursor LP0 latency is 1300 ns */
	if (INTEL_INFO(dev)->gen == 5)
		wm[0] = 13;

	/* WaDoubleCursorLP3Latency:ivb */
	if (IS_IVYBRIDGE(dev))
		wm[3] *= 2;
}

int ilk_wm_max_level(const struct drm_device *dev)
{
	/* how many WM levels are we expecting */
	if (INTEL_INFO(dev)->gen >= 9)
		return 7;
	else if (IS_HASWELL(dev) || IS_BROADWELL(dev))
		return 4;
	else if (INTEL_INFO(dev)->gen >= 6)
		return 3;
	else
		return 2;
}

static void intel_print_wm_latency(struct drm_device *dev,
				   const char *name,
				   const uint16_t wm[8])
{
	int level, max_level = ilk_wm_max_level(dev);

	for (level = 0; level <= max_level; level++) {
		unsigned int latency = wm[level];

		if (latency == 0) {
			DRM_ERROR("%s WM%d latency not provided\n",
				  name, level);
			continue;
		}

		/*
		 * - latencies are in us on gen9.
		 * - before then, WM1+ latency values are in 0.5us units
		 */
		if (IS_GEN9(dev))
			latency *= 10;
		else if (level > 0)
			latency *= 5;

		DRM_DEBUG_KMS("%s WM%d latency %u (%u.%u usec)\n",
			      name, level, wm[level],
			      latency / 10, latency % 10);
	}
}

static bool ilk_increase_wm_latency(struct drm_i915_private *dev_priv,
				    uint16_t wm[5], uint16_t min)
{
	int level, max_level = ilk_wm_max_level(dev_priv->dev);

	if (wm[0] >= min)
		return false;

	wm[0] = max(wm[0], min);
	for (level = 1; level <= max_level; level++)
		wm[level] = max_t(uint16_t, wm[level], DIV_ROUND_UP(min, 5));

	return true;
}

static void snb_wm_latency_quirk(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	bool changed;

	/*
	 * The BIOS provided WM memory latency values are often
	 * inadequate for high resolution displays. Adjust them.
	 */
	changed = ilk_increase_wm_latency(dev_priv, dev_priv->wm.pri_latency, 12) |
		ilk_increase_wm_latency(dev_priv, dev_priv->wm.spr_latency, 12) |
		ilk_increase_wm_latency(dev_priv, dev_priv->wm.cur_latency, 12);

	if (!changed)
		return;

	DRM_DEBUG_KMS("WM latency values increased to avoid potential underruns\n");
	intel_print_wm_latency(dev, "Primary", dev_priv->wm.pri_latency);
	intel_print_wm_latency(dev, "Sprite", dev_priv->wm.spr_latency);
	intel_print_wm_latency(dev, "Cursor", dev_priv->wm.cur_latency);
}

static void ilk_setup_wm_latency(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	intel_read_wm_latency(dev, dev_priv->wm.pri_latency);

	memcpy(dev_priv->wm.spr_latency, dev_priv->wm.pri_latency,
	       sizeof(dev_priv->wm.pri_latency));
	memcpy(dev_priv->wm.cur_latency, dev_priv->wm.pri_latency,
	       sizeof(dev_priv->wm.pri_latency));

	intel_fixup_spr_wm_latency(dev, dev_priv->wm.spr_latency);
	intel_fixup_cur_wm_latency(dev, dev_priv->wm.cur_latency);

	intel_print_wm_latency(dev, "Primary", dev_priv->wm.pri_latency);
	intel_print_wm_latency(dev, "Sprite", dev_priv->wm.spr_latency);
	intel_print_wm_latency(dev, "Cursor", dev_priv->wm.cur_latency);

	if (IS_GEN6(dev))
		snb_wm_latency_quirk(dev);
}

static void skl_setup_wm_latency(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	intel_read_wm_latency(dev, dev_priv->wm.skl_latency);
	intel_print_wm_latency(dev, "Gen9 Plane", dev_priv->wm.skl_latency);
}

static void ilk_compute_wm_config(struct drm_device *dev,
				  struct intel_wm_config *config)
{
	struct intel_crtc *intel_crtc;

	/* Compute the currently _active_ config */
	for_each_intel_crtc(dev, intel_crtc) {
		const struct intel_pipe_wm *wm = &intel_crtc->wm.active;

		if (!wm->pipe_enabled)
			continue;

		config->sprites_enabled |= wm->sprites_enabled;
		config->sprites_scaled |= wm->sprites_scaled;
		config->num_pipes_active++;
	}
}

/* Compute new watermarks for the pipe */
static bool intel_compute_pipe_wm(struct intel_crtc_state *cstate,
				  struct intel_pipe_wm *pipe_wm)
{
	struct drm_crtc *crtc = cstate->base.crtc;
	struct drm_device *dev = crtc->dev;
	const struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_crtc *intel_crtc = to_intel_crtc(crtc);
	struct intel_plane *intel_plane;
	struct intel_plane_state *sprstate = NULL;
	int level, max_level = ilk_wm_max_level(dev);
	/* LP0 watermark maximums depend on this pipe alone */
	struct intel_wm_config config = {
		.num_pipes_active = 1,
	};
	struct ilk_wm_maximums max;

	for_each_intel_plane_on_crtc(dev, intel_crtc, intel_plane) {
		if (intel_plane->base.type == DRM_PLANE_TYPE_OVERLAY) {
			sprstate = to_intel_plane_state(intel_plane->base.state);
			break;
		}
	}

	config.sprites_enabled = sprstate->visible;
	config.sprites_scaled = sprstate->visible &&
		(drm_rect_width(&sprstate->dst) != drm_rect_width(&sprstate->src) >> 16 ||
		drm_rect_height(&sprstate->dst) != drm_rect_height(&sprstate->src) >> 16);

	pipe_wm->pipe_enabled = cstate->base.active;
	pipe_wm->sprites_enabled = sprstate->visible;
	pipe_wm->sprites_scaled = config.sprites_scaled;

	/* ILK/SNB: LP2+ watermarks only w/o sprites */
	if (INTEL_INFO(dev)->gen <= 6 && sprstate->visible)
		max_level = 1;

	/* ILK/SNB/IVB: LP1+ watermarks only w/o scaling */
	if (config.sprites_scaled)
		max_level = 0;

	ilk_compute_wm_level(dev_priv, intel_crtc, 0, cstate, &pipe_wm->wm[0]);

	if (IS_HASWELL(dev) || IS_BROADWELL(dev))
		pipe_wm->linetime = hsw_compute_linetime_wm(dev, crtc);

	/* LP0 watermarks always use 1/2 DDB partitioning */
	ilk_compute_wm_maximums(dev, 0, &config, INTEL_DDB_PART_1_2, &max);

	/* At least LP0 must be valid */
	if (!ilk_validate_wm_level(0, &max, &pipe_wm->wm[0]))
		return false;

	ilk_compute_wm_reg_maximums(dev, 1, &max);

	for (level = 1; level <= max_level; level++) {
		struct intel_wm_level wm = {};

		ilk_compute_wm_level(dev_priv, intel_crtc, level, cstate, &wm);

		/*
		 * Disable any watermark level that exceeds the
		 * register maximums since such watermarks are
		 * always invalid.
		 */
		if (!ilk_validate_wm_level(level, &max, &wm))
			break;

		pipe_wm->wm[level] = wm;
	}

	return true;
}

/*
 * Merge the watermarks from all active pipes for a specific level.
 */
static void ilk_merge_wm_level(struct drm_device *dev,
			       int level,
			       struct intel_wm_level *ret_wm)
{
	const struct intel_crtc *intel_crtc;

	ret_wm->enable = true;

	for_each_intel_crtc(dev, intel_crtc) {
		const struct intel_pipe_wm *active = &intel_crtc->wm.active;
		const struct intel_wm_level *wm = &active->wm[level];

		if (!active->pipe_enabled)
			continue;

		/*
		 * The watermark values may have been used in the past,
		 * so we must maintain them in the registers for some
		 * time even if the level is now disabled.
		 */
		if (!wm->enable)
			ret_wm->enable = false;

		ret_wm->pri_val = max(ret_wm->pri_val, wm->pri_val);
		ret_wm->spr_val = max(ret_wm->spr_val, wm->spr_val);
		ret_wm->cur_val = max(ret_wm->cur_val, wm->cur_val);
		ret_wm->fbc_val = max(ret_wm->fbc_val, wm->fbc_val);
	}
}

/*
 * Merge all low power watermarks for all active pipes.
 */
static void ilk_wm_merge(struct drm_device *dev,
			 const struct intel_wm_config *config,
			 const struct ilk_wm_maximums *max,
			 struct intel_pipe_wm *merged)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	int level, max_level = ilk_wm_max_level(dev);
	int last_enabled_level = max_level;

	/* ILK/SNB/IVB: LP1+ watermarks only w/ single pipe */
	if ((INTEL_INFO(dev)->gen <= 6 || IS_IVYBRIDGE(dev)) &&
	    config->num_pipes_active > 1)
		return;

	/* ILK: FBC WM must be disabled always */
	merged->fbc_wm_enabled = INTEL_INFO(dev)->gen >= 6;

	/* merge each WM1+ level */
	for (level = 1; level <= max_level; level++) {
		struct intel_wm_level *wm = &merged->wm[level];

		ilk_merge_wm_level(dev, level, wm);

		if (level > last_enabled_level)
			wm->enable = false;
		else if (!ilk_validate_wm_level(level, max, wm))
			/* make sure all following levels get disabled */
			last_enabled_level = level - 1;

		/*
		 * The spec says it is preferred to disable
		 * FBC WMs instead of disabling a WM level.
		 */
		if (wm->fbc_val > max->fbc) {
			if (wm->enable)
				merged->fbc_wm_enabled = false;
			wm->fbc_val = 0;
		}
	}

	/* ILK: LP2+ must be disabled when FBC WM is disabled but FBC enabled */
	/*
	 * FIXME this is racy. FBC might get enabled later.
	 * What we should check here is whether FBC can be
	 * enabled sometime later.
	 */
	if (IS_GEN5(dev) && !merged->fbc_wm_enabled &&
	    intel_fbc_enabled(dev_priv)) {
		for (level = 2; level <= max_level; level++) {
			struct intel_wm_level *wm = &merged->wm[level];

			wm->enable = false;
		}
	}
}

static int ilk_wm_lp_to_level(int wm_lp, const struct intel_pipe_wm *pipe_wm)
{
	/* LP1,LP2,LP3 levels are either 1,2,3 or 1,3,4 */
	return wm_lp + (wm_lp >= 2 && pipe_wm->wm[4].enable);
}

/* The value we need to program into the WM_LPx latency field */
static unsigned int ilk_wm_lp_latency(struct drm_device *dev, int level)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	if (IS_HASWELL(dev) || IS_BROADWELL(dev))
		return 2 * level;
	else
		return dev_priv->wm.pri_latency[level];
}

static void ilk_compute_wm_results(struct drm_device *dev,
				   const struct intel_pipe_wm *merged,
				   enum intel_ddb_partitioning partitioning,
				   struct ilk_wm_values *results)
{
	struct intel_crtc *intel_crtc;
	int level, wm_lp;

	results->enable_fbc_wm = merged->fbc_wm_enabled;
	results->partitioning = partitioning;

	/* LP1+ register values */
	for (wm_lp = 1; wm_lp <= 3; wm_lp++) {
		const struct intel_wm_level *r;

		level = ilk_wm_lp_to_level(wm_lp, merged);

		r = &merged->wm[level];

		/*
		 * Maintain the watermark values even if the level is
		 * disabled. Doing otherwise could cause underruns.
		 */
		results->wm_lp[wm_lp - 1] =
			(ilk_wm_lp_latency(dev, level) << WM1_LP_LATENCY_SHIFT) |
			(r->pri_val << WM1_LP_SR_SHIFT) |
			r->cur_val;

		if (r->enable)
			results->wm_lp[wm_lp - 1] |= WM1_LP_SR_EN;

		if (INTEL_INFO(dev)->gen >= 8)
			results->wm_lp[wm_lp - 1] |=
				r->fbc_val << WM1_LP_FBC_SHIFT_BDW;
		else
			results->wm_lp[wm_lp - 1] |=
				r->fbc_val << WM1_LP_FBC_SHIFT;

		/*
		 * Always set WM1S_LP_EN when spr_val != 0, even if the
		 * level is disabled. Doing otherwise could cause underruns.
		 */
		if (INTEL_INFO(dev)->gen <= 6 && r->spr_val) {
			WARN_ON(wm_lp != 1);
			results->wm_lp_spr[wm_lp - 1] = WM1S_LP_EN | r->spr_val;
		} else
			results->wm_lp_spr[wm_lp - 1] = r->spr_val;
	}

	/* LP0 register values */
	for_each_intel_crtc(dev, intel_crtc) {
		enum pipe pipe = intel_crtc->pipe;
		const struct intel_wm_level *r =
			&intel_crtc->wm.active.wm[0];

		if (WARN_ON(!r->enable))
			continue;

		results->wm_linetime[pipe] = intel_crtc->wm.active.linetime;

		results->wm_pipe[pipe] =
			(r->pri_val << WM0_PIPE_PLANE_SHIFT) |
			(r->spr_val << WM0_PIPE_SPRITE_SHIFT) |
			r->cur_val;
	}
}

/* Find the result with the highest level enabled. Check for enable_fbc_wm in
 * case both are at the same level. Prefer r1 in case they're the same. */
static struct intel_pipe_wm *ilk_find_best_result(struct drm_device *dev,
						  struct intel_pipe_wm *r1,
						  struct intel_pipe_wm *r2)
{
	int level, max_level = ilk_wm_max_level(dev);
	int level1 = 0, level2 = 0;

	for (level = 1; level <= max_level; level++) {
		if (r1->wm[level].enable)
			level1 = level;
		if (r2->wm[level].enable)
			level2 = level;
	}

	if (level1 == level2) {
		if (r2->fbc_wm_enabled && !r1->fbc_wm_enabled)
			return r2;
		else
			return r1;
	} else if (level1 > level2) {
		return r1;
	} else {
		return r2;
	}
}

/* dirty bits used to track which watermarks need changes */
#define WM_DIRTY_PIPE(pipe) (1 << (pipe))
#define WM_DIRTY_LINETIME(pipe) (1 << (8 + (pipe)))
#define WM_DIRTY_LP(wm_lp) (1 << (15 + (wm_lp)))
#define WM_DIRTY_LP_ALL (WM_DIRTY_LP(1) | WM_DIRTY_LP(2) | WM_DIRTY_LP(3))
#define WM_DIRTY_FBC (1 << 24)
#define WM_DIRTY_DDB (1 << 25)

static unsigned int ilk_compute_wm_dirty(struct drm_i915_private *dev_priv,
					 const struct ilk_wm_values *old,
					 const struct ilk_wm_values *new)
{
	unsigned int dirty = 0;
	enum pipe pipe;
	int wm_lp;

	for_each_pipe(dev_priv, pipe) {
		if (old->wm_linetime[pipe] != new->wm_linetime[pipe]) {
			dirty |= WM_DIRTY_LINETIME(pipe);
			/* Must disable LP1+ watermarks too */
			dirty |= WM_DIRTY_LP_ALL;
		}

		if (old->wm_pipe[pipe] != new->wm_pipe[pipe]) {
			dirty |= WM_DIRTY_PIPE(pipe);
			/* Must disable LP1+ watermarks too */
			dirty |= WM_DIRTY_LP_ALL;
		}
	}

	if (old->enable_fbc_wm != new->enable_fbc_wm) {
		dirty |= WM_DIRTY_FBC;
		/* Must disable LP1+ watermarks too */
		dirty |= WM_DIRTY_LP_ALL;
	}

	if (old->partitioning != new->partitioning) {
		dirty |= WM_DIRTY_DDB;
		/* Must disable LP1+ watermarks too */
		dirty |= WM_DIRTY_LP_ALL;
	}

	/* LP1+ watermarks already deemed dirty, no need to continue */
	if (dirty & WM_DIRTY_LP_ALL)
		return dirty;

	/* Find the lowest numbered LP1+ watermark in need of an update... */
	for (wm_lp = 1; wm_lp <= 3; wm_lp++) {
		if (old->wm_lp[wm_lp - 1] != new->wm_lp[wm_lp - 1] ||
		    old->wm_lp_spr[wm_lp - 1] != new->wm_lp_spr[wm_lp - 1])
			break;
	}

	/* ...and mark it and all higher numbered LP1+ watermarks as dirty */
	for (; wm_lp <= 3; wm_lp++)
		dirty |= WM_DIRTY_LP(wm_lp);

	return dirty;
}

static bool _ilk_disable_lp_wm(struct drm_i915_private *dev_priv,
			       unsigned int dirty)
{
	struct ilk_wm_values *previous = &dev_priv->wm.hw;
	bool changed = false;

	if (dirty & WM_DIRTY_LP(3) && previous->wm_lp[2] & WM1_LP_SR_EN) {
		previous->wm_lp[2] &= ~WM1_LP_SR_EN;
		I915_WRITE(WM3_LP_ILK, previous->wm_lp[2]);
		changed = true;
	}
	if (dirty & WM_DIRTY_LP(2) && previous->wm_lp[1] & WM1_LP_SR_EN) {
		previous->wm_lp[1] &= ~WM1_LP_SR_EN;
		I915_WRITE(WM2_LP_ILK, previous->wm_lp[1]);
		changed = true;
	}
	if (dirty & WM_DIRTY_LP(1) && previous->wm_lp[0] & WM1_LP_SR_EN) {
		previous->wm_lp[0] &= ~WM1_LP_SR_EN;
		I915_WRITE(WM1_LP_ILK, previous->wm_lp[0]);
		changed = true;
	}

	/*
	 * Don't touch WM1S_LP_EN here.
	 * Doing so could cause underruns.
	 */

	return changed;
}

/*
 * The spec says we shouldn't write when we don't need, because every write
 * causes WMs to be re-evaluated, expending some power.
 */
static void ilk_write_wm_values(struct drm_i915_private *dev_priv,
				struct ilk_wm_values *results)
{
	struct drm_device *dev = dev_priv->dev;
	struct ilk_wm_values *previous = &dev_priv->wm.hw;
	unsigned int dirty;
	uint32_t val;

	dirty = ilk_compute_wm_dirty(dev_priv, previous, results);
	if (!dirty)
		return;

	_ilk_disable_lp_wm(dev_priv, dirty);

	if (dirty & WM_DIRTY_PIPE(PIPE_A))
		I915_WRITE(WM0_PIPEA_ILK, results->wm_pipe[0]);
	if (dirty & WM_DIRTY_PIPE(PIPE_B))
		I915_WRITE(WM0_PIPEB_ILK, results->wm_pipe[1]);
	if (dirty & WM_DIRTY_PIPE(PIPE_C))
		I915_WRITE(WM0_PIPEC_IVB, results->wm_pipe[2]);

	if (dirty & WM_DIRTY_LINETIME(PIPE_A))
		I915_WRITE(PIPE_WM_LINETIME(PIPE_A), results->wm_linetime[0]);
	if (dirty & WM_DIRTY_LINETIME(PIPE_B))
		I915_WRITE(PIPE_WM_LINETIME(PIPE_B), results->wm_linetime[1]);
	if (dirty & WM_DIRTY_LINETIME(PIPE_C))
		I915_WRITE(PIPE_WM_LINETIME(PIPE_C), results->wm_linetime[2]);

	if (dirty & WM_DIRTY_DDB) {
		if (IS_HASWELL(dev) || IS_BROADWELL(dev)) {
			val = I915_READ(WM_MISC);
			if (results->partitioning == INTEL_DDB_PART_1_2)
				val &= ~WM_MISC_DATA_PARTITION_5_6;
			else
				val |= WM_MISC_DATA_PARTITION_5_6;
			I915_WRITE(WM_MISC, val);
		} else {
			val = I915_READ(DISP_ARB_CTL2);
			if (results->partitioning == INTEL_DDB_PART_1_2)
				val &= ~DISP_DATA_PARTITION_5_6;
			else
				val |= DISP_DATA_PARTITION_5_6;
			I915_WRITE(DISP_ARB_CTL2, val);
		}
	}

	if (dirty & WM_DIRTY_FBC) {
		val = I915_READ(DISP_ARB_CTL);
		if (results->enable_fbc_wm)
			val &= ~DISP_FBC_WM_DIS;
		else
			val |= DISP_FBC_WM_DIS;
		I915_WRITE(DISP_ARB_CTL, val);
	}

	if (dirty & WM_DIRTY_LP(1) &&
	    previous->wm_lp_spr[0] != results->wm_lp_spr[0])
		I915_WRITE(WM1S_LP_ILK, results->wm_lp_spr[0]);

	if (INTEL_INFO(dev)->gen >= 7) {
		if (dirty & WM_DIRTY_LP(2) && previous->wm_lp_spr[1] != results->wm_lp_spr[1])
			I915_WRITE(WM2S_LP_IVB, results->wm_lp_spr[1]);
		if (dirty & WM_DIRTY_LP(3) && previous->wm_lp_spr[2] != results->wm_lp_spr[2])
			I915_WRITE(WM3S_LP_IVB, results->wm_lp_spr[2]);
	}

	if (dirty & WM_DIRTY_LP(1) && previous->wm_lp[0] != results->wm_lp[0])
		I915_WRITE(WM1_LP_ILK, results->wm_lp[0]);
	if (dirty & WM_DIRTY_LP(2) && previous->wm_lp[1] != results->wm_lp[1])
		I915_WRITE(WM2_LP_ILK, results->wm_lp[1]);
	if (dirty & WM_DIRTY_LP(3) && previous->wm_lp[2] != results->wm_lp[2])
		I915_WRITE(WM3_LP_ILK, results->wm_lp[2]);

	dev_priv->wm.hw = *results;
}

static bool ilk_disable_lp_wm(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	return _ilk_disable_lp_wm(dev_priv, WM_DIRTY_LP_ALL);
}

/*
 * On gen9, we need to allocate Display Data Buffer (DDB) portions to the
 * different active planes.
 */

#define SKL_DDB_SIZE		896	/* in blocks */
#define BXT_DDB_SIZE		512

static void
skl_ddb_get_pipe_allocation_limits(struct drm_device *dev,
				   struct drm_crtc *for_crtc,
				   const struct intel_wm_config *config,
				   const struct skl_pipe_wm_parameters *params,
				   struct skl_ddb_entry *alloc /* out */)
{
	struct drm_crtc *crtc;
	unsigned int pipe_size, ddb_size;
	int nth_active_pipe;

	if (!params->active) {
		alloc->start = 0;
		alloc->end = 0;
		return;
	}

	if (IS_BROXTON(dev))
		ddb_size = BXT_DDB_SIZE;
	else
		ddb_size = SKL_DDB_SIZE;

	ddb_size -= 4; /* 4 blocks for bypass path allocation */

	nth_active_pipe = 0;
	for_each_crtc(dev, crtc) {
		if (!to_intel_crtc(crtc)->active)
			continue;

		if (crtc == for_crtc)
			break;

		nth_active_pipe++;
	}

	pipe_size = ddb_size / config->num_pipes_active;
	alloc->start = nth_active_pipe * ddb_size / config->num_pipes_active;
	alloc->end = alloc->start + pipe_size;
}

static unsigned int skl_cursor_allocation(const struct intel_wm_config *config)
{
	if (config->num_pipes_active == 1)
		return 32;

	return 8;
}

static void skl_ddb_entry_init_from_hw(struct skl_ddb_entry *entry, u32 reg)
{
	entry->start = reg & 0x3ff;
	entry->end = (reg >> 16) & 0x3ff;
	if (entry->end)
		entry->end += 1;
}

void skl_ddb_get_hw_state(struct drm_i915_private *dev_priv,
			  struct skl_ddb_allocation *ddb /* out */)
{
	enum pipe pipe;
	int plane;
	u32 val;

	memset(ddb, 0, sizeof(*ddb));

	for_each_pipe(dev_priv, pipe) {
		if (!intel_display_power_is_enabled(dev_priv, POWER_DOMAIN_PIPE(pipe)))
			continue;

		for_each_plane(dev_priv, pipe, plane) {
			val = I915_READ(PLANE_BUF_CFG(pipe, plane));
			skl_ddb_entry_init_from_hw(&ddb->plane[pipe][plane],
						   val);
		}

		val = I915_READ(CUR_BUF_CFG(pipe));
		skl_ddb_entry_init_from_hw(&ddb->plane[pipe][PLANE_CURSOR],
					   val);
	}
}

static unsigned int
skl_plane_relative_data_rate(const struct intel_plane_wm_parameters *p, int y)
{

	/* for planar format */
	if (p->y_bytes_per_pixel) {
		if (y)  /* y-plane data rate */
			return p->horiz_pixels * p->vert_pixels * p->y_bytes_per_pixel;
		else    /* uv-plane data rate */
			return (p->horiz_pixels/2) * (p->vert_pixels/2) * p->bytes_per_pixel;
	}

	/* for packed formats */
	return p->horiz_pixels * p->vert_pixels * p->bytes_per_pixel;
}

/*
 * We don't overflow 32 bits. Worst case is 3 planes enabled, each fetching
 * a 8192x4096@32bpp framebuffer:
 *   3 * 4096 * 8192  * 4 < 2^32
 */
static unsigned int
skl_get_total_relative_data_rate(struct intel_crtc *intel_crtc,
				 const struct skl_pipe_wm_parameters *params)
{
	unsigned int total_data_rate = 0;
	int plane;

	for (plane = 0; plane < intel_num_planes(intel_crtc); plane++) {
		const struct intel_plane_wm_parameters *p;

		p = &params->plane[plane];
		if (!p->enabled)
			continue;

		total_data_rate += skl_plane_relative_data_rate(p, 0); /* packed/uv */
		if (p->y_bytes_per_pixel) {
			total_data_rate += skl_plane_relative_data_rate(p, 1); /* y-plane */
		}
	}

	return total_data_rate;
}

static void
skl_allocate_pipe_ddb(struct drm_crtc *crtc,
		      const struct intel_wm_config *config,
		      const struct skl_pipe_wm_parameters *params,
		      struct skl_ddb_allocation *ddb /* out */)
{
	struct drm_device *dev = crtc->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_crtc *intel_crtc = to_intel_crtc(crtc);
	enum pipe pipe = intel_crtc->pipe;
	struct skl_ddb_entry *alloc = &ddb->pipe[pipe];
	uint16_t alloc_size, start, cursor_blocks;
	uint16_t minimum[I915_MAX_PLANES];
	uint16_t y_minimum[I915_MAX_PLANES];
	unsigned int total_data_rate;
	int plane;

	skl_ddb_get_pipe_allocation_limits(dev, crtc, config, params, alloc);
	alloc_size = skl_ddb_entry_size(alloc);
	if (alloc_size == 0) {
		memset(ddb->plane[pipe], 0, sizeof(ddb->plane[pipe]));
		memset(&ddb->plane[pipe][PLANE_CURSOR], 0,
		       sizeof(ddb->plane[pipe][PLANE_CURSOR]));
		return;
	}

	cursor_blocks = skl_cursor_allocation(config);
	ddb->plane[pipe][PLANE_CURSOR].start = alloc->end - cursor_blocks;
	ddb->plane[pipe][PLANE_CURSOR].end = alloc->end;

	alloc_size -= cursor_blocks;
	alloc->end -= cursor_blocks;

	/* 1. Allocate the mininum required blocks for each active plane */
	for_each_plane(dev_priv, pipe, plane) {
		const struct intel_plane_wm_parameters *p;

		p = &params->plane[plane];
		if (!p->enabled)
			continue;

		minimum[plane] = 8;
		alloc_size -= minimum[plane];
		y_minimum[plane] = p->y_bytes_per_pixel ? 8 : 0;
		alloc_size -= y_minimum[plane];
	}

	/*
	 * 2. Distribute the remaining space in proportion to the amount of
	 * data each plane needs to fetch from memory.
	 *
	 * FIXME: we may not allocate every single block here.
	 */
	total_data_rate = skl_get_total_relative_data_rate(intel_crtc, params);

	start = alloc->start;
	for (plane = 0; plane < intel_num_planes(intel_crtc); plane++) {
		const struct intel_plane_wm_parameters *p;
		unsigned int data_rate, y_data_rate;
		uint16_t plane_blocks, y_plane_blocks = 0;

		p = &params->plane[plane];
		if (!p->enabled)
			continue;

		data_rate = skl_plane_relative_data_rate(p, 0);

		/*
		 * allocation for (packed formats) or (uv-plane part of planar format):
		 * promote the expression to 64 bits to avoid overflowing, the
		 * result is < available as data_rate / total_data_rate < 1
		 */
		plane_blocks = minimum[plane];
		plane_blocks += div_u64((uint64_t)alloc_size * data_rate,
					total_data_rate);

		ddb->plane[pipe][plane].start = start;
		ddb->plane[pipe][plane].end = start + plane_blocks;

		start += plane_blocks;

		/*
		 * allocation for y_plane part of planar format:
		 */
		if (p->y_bytes_per_pixel) {
			y_data_rate = skl_plane_relative_data_rate(p, 1);
			y_plane_blocks = y_minimum[plane];
			y_plane_blocks += div_u64((uint64_t)alloc_size * y_data_rate,
						total_data_rate);

			ddb->y_plane[pipe][plane].start = start;
			ddb->y_plane[pipe][plane].end = start + y_plane_blocks;

			start += y_plane_blocks;
		}

	}

}

static uint32_t skl_pipe_pixel_rate(const struct intel_crtc_state *config)
{
	/* TODO: Take into account the scalers once we support them */
	return config->base.adjusted_mode.crtc_clock;
}

/*
 * The max latency should be 257 (max the punit can code is 255 and we add 2us
 * for the read latency) and bytes_per_pixel should always be <= 8, so that
 * should allow pixel_rate up to ~2 GHz which seems sufficient since max
 * 2xcdclk is 1350 MHz and the pixel rate should never exceed that.
*/
static uint32_t skl_wm_method1(uint32_t pixel_rate, uint8_t bytes_per_pixel,
			       uint32_t latency)
{
	uint32_t wm_intermediate_val, ret;

	if (latency == 0)
		return UINT_MAX;

	wm_intermediate_val = latency * pixel_rate * bytes_per_pixel / 512;
	ret = DIV_ROUND_UP(wm_intermediate_val, 1000);

	return ret;
}

static uint32_t skl_wm_method2(uint32_t pixel_rate, uint32_t pipe_htotal,
			       uint32_t horiz_pixels, uint8_t bytes_per_pixel,
			       uint64_t tiling, uint32_t latency)
{
	uint32_t ret;
	uint32_t plane_bytes_per_line, plane_blocks_per_line;
	uint32_t wm_intermediate_val;

	if (latency == 0)
		return UINT_MAX;

	plane_bytes_per_line = horiz_pixels * bytes_per_pixel;

	if (tiling == I915_FORMAT_MOD_Y_TILED ||
	    tiling == I915_FORMAT_MOD_Yf_TILED) {
		plane_bytes_per_line *= 4;
		plane_blocks_per_line = DIV_ROUND_UP(plane_bytes_per_line, 512);
		plane_blocks_per_line /= 4;
	} else {
		plane_blocks_per_line = DIV_ROUND_UP(plane_bytes_per_line, 512);
	}

	wm_intermediate_val = latency * pixel_rate;
	ret = DIV_ROUND_UP(wm_intermediate_val, pipe_htotal * 1000) *
				plane_blocks_per_line;

	return ret;
}

static bool skl_ddb_allocation_changed(const struct skl_ddb_allocation *new_ddb,
				       const struct intel_crtc *intel_crtc)
{
	struct drm_device *dev = intel_crtc->base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	const struct skl_ddb_allocation *cur_ddb = &dev_priv->wm.skl_hw.ddb;
	enum pipe pipe = intel_crtc->pipe;

	if (memcmp(new_ddb->plane[pipe], cur_ddb->plane[pipe],
		   sizeof(new_ddb->plane[pipe])))
		return true;

	if (memcmp(&new_ddb->plane[pipe][PLANE_CURSOR], &cur_ddb->plane[pipe][PLANE_CURSOR],
		    sizeof(new_ddb->plane[pipe][PLANE_CURSOR])))
		return true;

	return false;
}

static void skl_compute_wm_global_parameters(struct drm_device *dev,
					     struct intel_wm_config *config)
{
	struct drm_crtc *crtc;
	struct drm_plane *plane;

	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head)
		config->num_pipes_active += to_intel_crtc(crtc)->active;

	/* FIXME: I don't think we need those two global parameters on SKL */
	list_for_each_entry(plane, &dev->mode_config.plane_list, head) {
		struct intel_plane *intel_plane = to_intel_plane(plane);

		config->sprites_enabled |= intel_plane->wm.enabled;
		config->sprites_scaled |= intel_plane->wm.scaled;
	}
}

static void skl_compute_wm_pipe_parameters(struct drm_crtc *crtc,
					   struct skl_pipe_wm_parameters *p)
{
	struct drm_device *dev = crtc->dev;
	struct intel_crtc *intel_crtc = to_intel_crtc(crtc);
	enum pipe pipe = intel_crtc->pipe;
	struct drm_plane *plane;
	struct drm_framebuffer *fb;
	int i = 1; /* Index for sprite planes start */

	p->active = intel_crtc->active;
	if (p->active) {
		p->pipe_htotal = intel_crtc->config->base.adjusted_mode.crtc_htotal;
		p->pixel_rate = skl_pipe_pixel_rate(intel_crtc->config);

		fb = crtc->primary->state->fb;
		/* For planar: Bpp is for uv plane, y_Bpp is for y plane */
		if (fb) {
			p->plane[0].enabled = true;
			p->plane[0].bytes_per_pixel = fb->pixel_format == DRM_FORMAT_NV12 ?
				drm_format_plane_cpp(fb->pixel_format, 1) :
				drm_format_plane_cpp(fb->pixel_format, 0);
			p->plane[0].y_bytes_per_pixel = fb->pixel_format == DRM_FORMAT_NV12 ?
				drm_format_plane_cpp(fb->pixel_format, 0) : 0;
			p->plane[0].tiling = fb->modifier[0];
		} else {
			p->plane[0].enabled = false;
			p->plane[0].bytes_per_pixel = 0;
			p->plane[0].y_bytes_per_pixel = 0;
			p->plane[0].tiling = DRM_FORMAT_MOD_NONE;
		}
		p->plane[0].horiz_pixels = intel_crtc->config->pipe_src_w;
		p->plane[0].vert_pixels = intel_crtc->config->pipe_src_h;
		p->plane[0].rotation = crtc->primary->state->rotation;

		fb = crtc->cursor->state->fb;
		p->plane[PLANE_CURSOR].y_bytes_per_pixel = 0;
		if (fb) {
			p->plane[PLANE_CURSOR].enabled = true;
			p->plane[PLANE_CURSOR].bytes_per_pixel = fb->bits_per_pixel / 8;
			p->plane[PLANE_CURSOR].horiz_pixels = crtc->cursor->state->crtc_w;
			p->plane[PLANE_CURSOR].vert_pixels = crtc->cursor->state->crtc_h;
		} else {
			p->plane[PLANE_CURSOR].enabled = false;
			p->plane[PLANE_CURSOR].bytes_per_pixel = 0;
			p->plane[PLANE_CURSOR].horiz_pixels = 64;
			p->plane[PLANE_CURSOR].vert_pixels = 64;
		}
	}

	list_for_each_entry(plane, &dev->mode_config.plane_list, head) {
		struct intel_plane *intel_plane = to_intel_plane(plane);

		if (intel_plane->pipe == pipe &&
			plane->type == DRM_PLANE_TYPE_OVERLAY)
			p->plane[i++] = intel_plane->wm;
	}
}

static bool skl_compute_plane_wm(const struct drm_i915_private *dev_priv,
				 struct skl_pipe_wm_parameters *p,
				 struct intel_plane_wm_parameters *p_params,
				 uint16_t ddb_allocation,
				 int level,
				 uint16_t *out_blocks, /* out */
				 uint8_t *out_lines /* out */)
{
	uint32_t latency = dev_priv->wm.skl_latency[level];
	uint32_t method1, method2;
	uint32_t plane_bytes_per_line, plane_blocks_per_line;
	uint32_t res_blocks, res_lines;
	uint32_t selected_result;
	uint8_t bytes_per_pixel;

	if (latency == 0 || !p->active || !p_params->enabled)
		return false;

	bytes_per_pixel = p_params->y_bytes_per_pixel ?
		p_params->y_bytes_per_pixel :
		p_params->bytes_per_pixel;
	method1 = skl_wm_method1(p->pixel_rate,
				 bytes_per_pixel,
				 latency);
	method2 = skl_wm_method2(p->pixel_rate,
				 p->pipe_htotal,
				 p_params->horiz_pixels,
				 bytes_per_pixel,
				 p_params->tiling,
				 latency);

	plane_bytes_per_line = p_params->horiz_pixels * bytes_per_pixel;
	plane_blocks_per_line = DIV_ROUND_UP(plane_bytes_per_line, 512);

	if (p_params->tiling == I915_FORMAT_MOD_Y_TILED ||
	    p_params->tiling == I915_FORMAT_MOD_Yf_TILED) {
		uint32_t min_scanlines = 4;
		uint32_t y_tile_minimum;
		if (intel_rotation_90_or_270(p_params->rotation)) {
			switch (p_params->bytes_per_pixel) {
			case 1:
				min_scanlines = 16;
				break;
			case 2:
				min_scanlines = 8;
				break;
			case 8:
				WARN(1, "Unsupported pixel depth for rotation");
			}
		}
		y_tile_minimum = plane_blocks_per_line * min_scanlines;
		selected_result = max(method2, y_tile_minimum);
	} else {
		if ((ddb_allocation / plane_blocks_per_line) >= 1)
			selected_result = min(method1, method2);
		else
			selected_result = method1;
	}

	res_blocks = selected_result + 1;
	res_lines = DIV_ROUND_UP(selected_result, plane_blocks_per_line);

	if (level >= 1 && level <= 7) {
		if (p_params->tiling == I915_FORMAT_MOD_Y_TILED ||
		    p_params->tiling == I915_FORMAT_MOD_Yf_TILED)
			res_lines += 4;
		else
			res_blocks++;
	}

	if (res_blocks >= ddb_allocation || res_lines > 31)
		return false;

	*out_blocks = res_blocks;
	*out_lines = res_lines;

	return true;
}

static void skl_compute_wm_level(const struct drm_i915_private *dev_priv,
				 struct skl_ddb_allocation *ddb,
				 struct skl_pipe_wm_parameters *p,
				 enum pipe pipe,
				 int level,
				 int num_planes,
				 struct skl_wm_level *result)
{
	uint16_t ddb_blocks;
	int i;

	for (i = 0; i < num_planes; i++) {
		ddb_blocks = skl_ddb_entry_size(&ddb->plane[pipe][i]);

		result->plane_en[i] = skl_compute_plane_wm(dev_priv,
						p, &p->plane[i],
						ddb_blocks,
						level,
						&result->plane_res_b[i],
						&result->plane_res_l[i]);
	}

	ddb_blocks = skl_ddb_entry_size(&ddb->plane[pipe][PLANE_CURSOR]);
	result->plane_en[PLANE_CURSOR] = skl_compute_plane_wm(dev_priv, p,
						 &p->plane[PLANE_CURSOR],
						 ddb_blocks, level,
						 &result->plane_res_b[PLANE_CURSOR],
						 &result->plane_res_l[PLANE_CURSOR]);
}

static uint32_t
skl_compute_linetime_wm(struct drm_crtc *crtc, struct skl_pipe_wm_parameters *p)
{
	if (!to_intel_crtc(crtc)->active)
		return 0;

	if (WARN_ON(p->pixel_rate == 0))
		return 0;

	return DIV_ROUND_UP(8 * p->pipe_htotal * 1000, p->pixel_rate);
}

static void skl_compute_transition_wm(struct drm_crtc *crtc,
				      struct skl_pipe_wm_parameters *params,
				      struct skl_wm_level *trans_wm /* out */)
{
	struct intel_crtc *intel_crtc = to_intel_crtc(crtc);
	int i;

	if (!params->active)
		return;

	/* Until we know more, just disable transition WMs */
	for (i = 0; i < intel_num_planes(intel_crtc); i++)
		trans_wm->plane_en[i] = false;
	trans_wm->plane_en[PLANE_CURSOR] = false;
}

static void skl_compute_pipe_wm(struct drm_crtc *crtc,
				struct skl_ddb_allocation *ddb,
				struct skl_pipe_wm_parameters *params,
				struct skl_pipe_wm *pipe_wm)
{
	struct drm_device *dev = crtc->dev;
	const struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_crtc *intel_crtc = to_intel_crtc(crtc);
	int level, max_level = ilk_wm_max_level(dev);

	for (level = 0; level <= max_level; level++) {
		skl_compute_wm_level(dev_priv, ddb, params, intel_crtc->pipe,
				     level, intel_num_planes(intel_crtc),
				     &pipe_wm->wm[level]);
	}
	pipe_wm->linetime = skl_compute_linetime_wm(crtc, params);

	skl_compute_transition_wm(crtc, params, &pipe_wm->trans_wm);
}

static void skl_compute_wm_results(struct drm_device *dev,
				   struct skl_pipe_wm_parameters *p,
				   struct skl_pipe_wm *p_wm,
				   struct skl_wm_values *r,
				   struct intel_crtc *intel_crtc)
{
	int level, max_level = ilk_wm_max_level(dev);
	enum pipe pipe = intel_crtc->pipe;
	uint32_t temp;
	int i;

	for (level = 0; level <= max_level; level++) {
		for (i = 0; i < intel_num_planes(intel_crtc); i++) {
			temp = 0;

			temp |= p_wm->wm[level].plane_res_l[i] <<
					PLANE_WM_LINES_SHIFT;
			temp |= p_wm->wm[level].plane_res_b[i];
			if (p_wm->wm[level].plane_en[i])
				temp |= PLANE_WM_EN;

			r->plane[pipe][i][level] = temp;
		}

		temp = 0;

		temp |= p_wm->wm[level].plane_res_l[PLANE_CURSOR] << PLANE_WM_LINES_SHIFT;
		temp |= p_wm->wm[level].plane_res_b[PLANE_CURSOR];

		if (p_wm->wm[level].plane_en[PLANE_CURSOR])
			temp |= PLANE_WM_EN;

		r->plane[pipe][PLANE_CURSOR][level] = temp;

	}

	/* transition WMs */
	for (i = 0; i < intel_num_planes(intel_crtc); i++) {
		temp = 0;
		temp |= p_wm->trans_wm.plane_res_l[i] << PLANE_WM_LINES_SHIFT;
		temp |= p_wm->trans_wm.plane_res_b[i];
		if (p_wm->trans_wm.plane_en[i])
			temp |= PLANE_WM_EN;

		r->plane_trans[pipe][i] = temp;
	}

	temp = 0;
	temp |= p_wm->trans_wm.plane_res_l[PLANE_CURSOR] << PLANE_WM_LINES_SHIFT;
	temp |= p_wm->trans_wm.plane_res_b[PLANE_CURSOR];
	if (p_wm->trans_wm.plane_en[PLANE_CURSOR])
		temp |= PLANE_WM_EN;

	r->plane_trans[pipe][PLANE_CURSOR] = temp;

	r->wm_linetime[pipe] = p_wm->linetime;
}

static void skl_ddb_entry_write(struct drm_i915_private *dev_priv, uint32_t reg,
				const struct skl_ddb_entry *entry)
{
	if (entry->end)
		I915_WRITE(reg, (entry->end - 1) << 16 | entry->start);
	else
		I915_WRITE(reg, 0);
}

static void skl_write_wm_values(struct drm_i915_private *dev_priv,
				const struct skl_wm_values *new)
{
	struct drm_device *dev = dev_priv->dev;
	struct intel_crtc *crtc;

	list_for_each_entry(crtc, &dev->mode_config.crtc_list, base.head) {
		int i, level, max_level = ilk_wm_max_level(dev);
		enum pipe pipe = crtc->pipe;

		if (!new->dirty[pipe])
			continue;

		I915_WRITE(PIPE_WM_LINETIME(pipe), new->wm_linetime[pipe]);

		for (level = 0; level <= max_level; level++) {
			for (i = 0; i < intel_num_planes(crtc); i++)
				I915_WRITE(PLANE_WM(pipe, i, level),
					   new->plane[pipe][i][level]);
			I915_WRITE(CUR_WM(pipe, level),
				   new->plane[pipe][PLANE_CURSOR][level]);
		}
		for (i = 0; i < intel_num_planes(crtc); i++)
			I915_WRITE(PLANE_WM_TRANS(pipe, i),
				   new->plane_trans[pipe][i]);
		I915_WRITE(CUR_WM_TRANS(pipe),
			   new->plane_trans[pipe][PLANE_CURSOR]);

		for (i = 0; i < intel_num_planes(crtc); i++) {
			skl_ddb_entry_write(dev_priv,
					    PLANE_BUF_CFG(pipe, i),
					    &new->ddb.plane[pipe][i]);
			skl_ddb_entry_write(dev_priv,
					    PLANE_NV12_BUF_CFG(pipe, i),
					    &new->ddb.y_plane[pipe][i]);
		}

		skl_ddb_entry_write(dev_priv, CUR_BUF_CFG(pipe),
				    &new->ddb.plane[pipe][PLANE_CURSOR]);
	}
}

/*
 * When setting up a new DDB allocation arrangement, we need to correctly
 * sequence the times at which the new allocations for the pipes are taken into
 * account or we'll have pipes fetching from space previously allocated to
 * another pipe.
 *
 * Roughly the sequence looks like:
 *  1. re-allocate the pipe(s) with the allocation being reduced and not
 *     overlapping with a previous light-up pipe (another way to put it is:
 *     pipes with their new allocation strickly included into their old ones).
 *  2. re-allocate the other pipes that get their allocation reduced
 *  3. allocate the pipes having their allocation increased
 *
 * Steps 1. and 2. are here to take care of the following case:
 * - Initially DDB looks like this:
 *     |   B    |   C    |
 * - enable pipe A.
 * - pipe B has a reduced DDB allocation that overlaps with the old pipe C
 *   allocation
 *     |  A  |  B  |  C  |
 *
 * We need to sequence the re-allocation: C, B, A (and not B, C, A).
 */

static void
skl_wm_flush_pipe(struct drm_i915_private *dev_priv, enum pipe pipe, int pass)
{
	int plane;

	DRM_DEBUG_KMS("flush pipe %c (pass %d)\n", pipe_name(pipe), pass);

	for_each_plane(dev_priv, pipe, plane) {
		I915_WRITE(PLANE_SURF(pipe, plane),
			   I915_READ(PLANE_SURF(pipe, plane)));
	}
	I915_WRITE(CURBASE(pipe), I915_READ(CURBASE(pipe)));
}

static bool
skl_ddb_allocation_included(const struct skl_ddb_allocation *old,
			    const struct skl_ddb_allocation *new,
			    enum pipe pipe)
{
	uint16_t old_size, new_size;

	old_size = skl_ddb_entry_size(&old->pipe[pipe]);
	new_size = skl_ddb_entry_size(&new->pipe[pipe]);

	return old_size != new_size &&
	       new->pipe[pipe].start >= old->pipe[pipe].start &&
	       new->pipe[pipe].end <= old->pipe[pipe].end;
}

static void skl_flush_wm_values(struct drm_i915_private *dev_priv,
				struct skl_wm_values *new_values)
{
	struct drm_device *dev = dev_priv->dev;
	struct skl_ddb_allocation *cur_ddb, *new_ddb;
	bool reallocated[I915_MAX_PIPES] = {};
	struct intel_crtc *crtc;
	enum pipe pipe;

	new_ddb = &new_values->ddb;
	cur_ddb = &dev_priv->wm.skl_hw.ddb;

	/*
	 * First pass: flush the pipes with the new allocation contained into
	 * the old space.
	 *
	 * We'll wait for the vblank on those pipes to ensure we can safely
	 * re-allocate the freed space without this pipe fetching from it.
	 */
	for_each_intel_crtc(dev, crtc) {
		if (!crtc->active)
			continue;

		pipe = crtc->pipe;

		if (!skl_ddb_allocation_included(cur_ddb, new_ddb, pipe))
			continue;

		skl_wm_flush_pipe(dev_priv, pipe, 1);
		intel_wait_for_vblank(dev, pipe);

		reallocated[pipe] = true;
	}


	/*
	 * Second pass: flush the pipes that are having their allocation
	 * reduced, but overlapping with a previous allocation.
	 *
	 * Here as well we need to wait for the vblank to make sure the freed
	 * space is not used anymore.
	 */
	for_each_intel_crtc(dev, crtc) {
		if (!crtc->active)
			continue;

		pipe = crtc->pipe;

		if (reallocated[pipe])
			continue;

		if (skl_ddb_entry_size(&new_ddb->pipe[pipe]) <
		    skl_ddb_entry_size(&cur_ddb->pipe[pipe])) {
			skl_wm_flush_pipe(dev_priv, pipe, 2);
			intel_wait_for_vblank(dev, pipe);
			reallocated[pipe] = true;
		}
	}

	/*
	 * Third pass: flush the pipes that got more space allocated.
	 *
	 * We don't need to actively wait for the update here, next vblank
	 * will just get more DDB space with the correct WM values.
	 */
	for_each_intel_crtc(dev, crtc) {
		if (!crtc->active)
			continue;

		pipe = crtc->pipe;

		/*
		 * At this point, only the pipes more space than before are
		 * left to re-allocate.
		 */
		if (reallocated[pipe])
			continue;

		skl_wm_flush_pipe(dev_priv, pipe, 3);
	}
}

static bool skl_update_pipe_wm(struct drm_crtc *crtc,
			       struct skl_pipe_wm_parameters *params,
			       struct intel_wm_config *config,
			       struct skl_ddb_allocation *ddb, /* out */
			       struct skl_pipe_wm *pipe_wm /* out */)
{
	struct intel_crtc *intel_crtc = to_intel_crtc(crtc);

	skl_compute_wm_pipe_parameters(crtc, params);
	skl_allocate_pipe_ddb(crtc, config, params, ddb);
	skl_compute_pipe_wm(crtc, ddb, params, pipe_wm);

	if (!memcmp(&intel_crtc->wm.skl_active, pipe_wm, sizeof(*pipe_wm)))
		return false;

	intel_crtc->wm.skl_active = *pipe_wm;

	return true;
}

static void skl_update_other_pipe_wm(struct drm_device *dev,
				     struct drm_crtc *crtc,
				     struct intel_wm_config *config,
				     struct skl_wm_values *r)
{
	struct intel_crtc *intel_crtc;
	struct intel_crtc *this_crtc = to_intel_crtc(crtc);

	/*
	 * If the WM update hasn't changed the allocation for this_crtc (the
	 * crtc we are currently computing the new WM values for), other
	 * enabled crtcs will keep the same allocation and we don't need to
	 * recompute anything for them.
	 */
	if (!skl_ddb_allocation_changed(&r->ddb, this_crtc))
		return;

	/*
	 * Otherwise, because of this_crtc being freshly enabled/disabled, the
	 * other active pipes need new DDB allocation and WM values.
	 */
	list_for_each_entry(intel_crtc, &dev->mode_config.crtc_list,
				base.head) {
		struct skl_pipe_wm_parameters params = {};
		struct skl_pipe_wm pipe_wm = {};
		bool wm_changed;

		if (this_crtc->pipe == intel_crtc->pipe)
			continue;

		if (!intel_crtc->active)
			continue;

		wm_changed = skl_update_pipe_wm(&intel_crtc->base,
						&params, config,
						&r->ddb, &pipe_wm);

		/*
		 * If we end up re-computing the other pipe WM values, it's
		 * because it was really needed, so we expect the WM values to
		 * be different.
		 */
		WARN_ON(!wm_changed);

		skl_compute_wm_results(dev, &params, &pipe_wm, r, intel_crtc);
		r->dirty[intel_crtc->pipe] = true;
	}
}

static void skl_clear_wm(struct skl_wm_values *watermarks, enum pipe pipe)
{
	watermarks->wm_linetime[pipe] = 0;
	memset(watermarks->plane[pipe], 0,
	       sizeof(uint32_t) * 8 * I915_MAX_PLANES);
	memset(watermarks->plane_trans[pipe],
	       0, sizeof(uint32_t) * I915_MAX_PLANES);
	watermarks->plane_trans[pipe][PLANE_CURSOR] = 0;

	/* Clear ddb entries for pipe */
	memset(&watermarks->ddb.pipe[pipe], 0, sizeof(struct skl_ddb_entry));
	memset(&watermarks->ddb.plane[pipe], 0,
	       sizeof(struct skl_ddb_entry) * I915_MAX_PLANES);
	memset(&watermarks->ddb.y_plane[pipe], 0,
	       sizeof(struct skl_ddb_entry) * I915_MAX_PLANES);
	memset(&watermarks->ddb.plane[pipe][PLANE_CURSOR], 0,
	       sizeof(struct skl_ddb_entry));

}

static void skl_update_wm(struct drm_crtc *crtc)
{
	struct intel_crtc *intel_crtc = to_intel_crtc(crtc);
	struct drm_device *dev = crtc->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct skl_pipe_wm_parameters params = {};
	struct skl_wm_values *results = &dev_priv->wm.skl_results;
	struct skl_pipe_wm pipe_wm = {};
	struct intel_wm_config config = {};


	/* Clear all dirty flags */
	memset(results->dirty, 0, sizeof(bool) * I915_MAX_PIPES);

	skl_clear_wm(results, intel_crtc->pipe);

	skl_compute_wm_global_parameters(dev, &config);

	if (!skl_update_pipe_wm(crtc, &params, &config,
				&results->ddb, &pipe_wm))
		return;

	skl_compute_wm_results(dev, &params, &pipe_wm, results, intel_crtc);
	results->dirty[intel_crtc->pipe] = true;

	skl_update_other_pipe_wm(dev, crtc, &config, results);
	skl_write_wm_values(dev_priv, results);
	skl_flush_wm_values(dev_priv, results);

	/* store the new configuration */
	dev_priv->wm.skl_hw = *results;
}

static void
skl_update_sprite_wm(struct drm_plane *plane, struct drm_crtc *crtc,
		     uint32_t sprite_width, uint32_t sprite_height,
		     int pixel_size, bool enabled, bool scaled)
{
	struct intel_plane *intel_plane = to_intel_plane(plane);
	struct drm_framebuffer *fb = plane->state->fb;

	intel_plane->wm.enabled = enabled;
	intel_plane->wm.scaled = scaled;
	intel_plane->wm.horiz_pixels = sprite_width;
	intel_plane->wm.vert_pixels = sprite_height;
	intel_plane->wm.tiling = DRM_FORMAT_MOD_NONE;

	/* For planar: Bpp is for UV plane, y_Bpp is for Y plane */
	intel_plane->wm.bytes_per_pixel =
		(fb && fb->pixel_format == DRM_FORMAT_NV12) ?
		drm_format_plane_cpp(plane->state->fb->pixel_format, 1) : pixel_size;
	intel_plane->wm.y_bytes_per_pixel =
		(fb && fb->pixel_format == DRM_FORMAT_NV12) ?
		drm_format_plane_cpp(plane->state->fb->pixel_format, 0) : 0;

	/*
	 * Framebuffer can be NULL on plane disable, but it does not
	 * matter for watermarks if we assume no tiling in that case.
	 */
	if (fb)
		intel_plane->wm.tiling = fb->modifier[0];
	intel_plane->wm.rotation = plane->state->rotation;

	skl_update_wm(crtc);
}

static void ilk_update_wm(struct drm_crtc *crtc)
{
	struct intel_crtc *intel_crtc = to_intel_crtc(crtc);
	struct intel_crtc_state *cstate = to_intel_crtc_state(crtc->state);
	struct drm_device *dev = crtc->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct ilk_wm_maximums max;
	struct ilk_wm_values results = {};
	enum intel_ddb_partitioning partitioning;
	struct intel_pipe_wm pipe_wm = {};
	struct intel_pipe_wm lp_wm_1_2 = {}, lp_wm_5_6 = {}, *best_lp_wm;
	struct intel_wm_config config = {};

	WARN_ON(cstate->base.active != intel_crtc->active);

	intel_compute_pipe_wm(cstate, &pipe_wm);

	if (!memcmp(&intel_crtc->wm.active, &pipe_wm, sizeof(pipe_wm)))
		return;

	intel_crtc->wm.active = pipe_wm;

	ilk_compute_wm_config(dev, &config);

	ilk_compute_wm_maximums(dev, 1, &config, INTEL_DDB_PART_1_2, &max);
	ilk_wm_merge(dev, &config, &max, &lp_wm_1_2);

	/* 5/6 split only in single pipe config on IVB+ */
	if (INTEL_INFO(dev)->gen >= 7 &&
	    config.num_pipes_active == 1 && config.sprites_enabled) {
		ilk_compute_wm_maximums(dev, 1, &config, INTEL_DDB_PART_5_6, &max);
		ilk_wm_merge(dev, &config, &max, &lp_wm_5_6);

		best_lp_wm = ilk_find_best_result(dev, &lp_wm_1_2, &lp_wm_5_6);
	} else {
		best_lp_wm = &lp_wm_1_2;
	}

	partitioning = (best_lp_wm == &lp_wm_1_2) ?
		       INTEL_DDB_PART_1_2 : INTEL_DDB_PART_5_6;

	ilk_compute_wm_results(dev, best_lp_wm, partitioning, &results);

	ilk_write_wm_values(dev_priv, &results);
}

static void
ilk_update_sprite_wm(struct drm_plane *plane,
		     struct drm_crtc *crtc,
		     uint32_t sprite_width, uint32_t sprite_height,
		     int pixel_size, bool enabled, bool scaled)
{
	struct drm_device *dev = plane->dev;
	struct intel_plane *intel_plane = to_intel_plane(plane);

	/*
	 * IVB workaround: must disable low power watermarks for at least
	 * one frame before enabling scaling.  LP watermarks can be re-enabled
	 * when scaling is disabled.
	 *
	 * WaCxSRDisabledForSpriteScaling:ivb
	 */
	if (IS_IVYBRIDGE(dev) && scaled && ilk_disable_lp_wm(dev))
		intel_wait_for_vblank(dev, intel_plane->pipe);

	ilk_update_wm(crtc);
}

static void skl_pipe_wm_active_state(uint32_t val,
				     struct skl_pipe_wm *active,
				     bool is_transwm,
				     bool is_cursor,
				     int i,
				     int level)
{
	bool is_enabled = (val & PLANE_WM_EN) != 0;

	if (!is_transwm) {
		if (!is_cursor) {
			active->wm[level].plane_en[i] = is_enabled;
			active->wm[level].plane_res_b[i] =
					val & PLANE_WM_BLOCKS_MASK;
			active->wm[level].plane_res_l[i] =
					(val >> PLANE_WM_LINES_SHIFT) &
						PLANE_WM_LINES_MASK;
		} else {
			active->wm[level].plane_en[PLANE_CURSOR] = is_enabled;
			active->wm[level].plane_res_b[PLANE_CURSOR] =
					val & PLANE_WM_BLOCKS_MASK;
			active->wm[level].plane_res_l[PLANE_CURSOR] =
					(val >> PLANE_WM_LINES_SHIFT) &
						PLANE_WM_LINES_MASK;
		}
	} else {
		if (!is_cursor) {
			active->trans_wm.plane_en[i] = is_enabled;
			active->trans_wm.plane_res_b[i] =
					val & PLANE_WM_BLOCKS_MASK;
			active->trans_wm.plane_res_l[i] =
					(val >> PLANE_WM_LINES_SHIFT) &
						PLANE_WM_LINES_MASK;
		} else {
			active->trans_wm.plane_en[PLANE_CURSOR] = is_enabled;
			active->trans_wm.plane_res_b[PLANE_CURSOR] =
					val & PLANE_WM_BLOCKS_MASK;
			active->trans_wm.plane_res_l[PLANE_CURSOR] =
					(val >> PLANE_WM_LINES_SHIFT) &
						PLANE_WM_LINES_MASK;
		}
	}
}

static void skl_pipe_wm_get_hw_state(struct drm_crtc *crtc)
{
	struct drm_device *dev = crtc->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct skl_wm_values *hw = &dev_priv->wm.skl_hw;
	struct intel_crtc *intel_crtc = to_intel_crtc(crtc);
	struct skl_pipe_wm *active = &intel_crtc->wm.skl_active;
	enum pipe pipe = intel_crtc->pipe;
	int level, i, max_level;
	uint32_t temp;

	max_level = ilk_wm_max_level(dev);

	hw->wm_linetime[pipe] = I915_READ(PIPE_WM_LINETIME(pipe));

	for (level = 0; level <= max_level; level++) {
		for (i = 0; i < intel_num_planes(intel_crtc); i++)
			hw->plane[pipe][i][level] =
					I915_READ(PLANE_WM(pipe, i, level));
		hw->plane[pipe][PLANE_CURSOR][level] = I915_READ(CUR_WM(pipe, level));
	}

	for (i = 0; i < intel_num_planes(intel_crtc); i++)
		hw->plane_trans[pipe][i] = I915_READ(PLANE_WM_TRANS(pipe, i));
	hw->plane_trans[pipe][PLANE_CURSOR] = I915_READ(CUR_WM_TRANS(pipe));

	if (!intel_crtc->active)
		return;

	hw->dirty[pipe] = true;

	active->linetime = hw->wm_linetime[pipe];

	for (level = 0; level <= max_level; level++) {
		for (i = 0; i < intel_num_planes(intel_crtc); i++) {
			temp = hw->plane[pipe][i][level];
			skl_pipe_wm_active_state(temp, active, false,
						false, i, level);
		}
		temp = hw->plane[pipe][PLANE_CURSOR][level];
		skl_pipe_wm_active_state(temp, active, false, true, i, level);
	}

	for (i = 0; i < intel_num_planes(intel_crtc); i++) {
		temp = hw->plane_trans[pipe][i];
		skl_pipe_wm_active_state(temp, active, true, false, i, 0);
	}

	temp = hw->plane_trans[pipe][PLANE_CURSOR];
	skl_pipe_wm_active_state(temp, active, true, true, i, 0);
}

void skl_wm_get_hw_state(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct skl_ddb_allocation *ddb = &dev_priv->wm.skl_hw.ddb;
	struct drm_crtc *crtc;

	skl_ddb_get_hw_state(dev_priv, ddb);
	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head)
		skl_pipe_wm_get_hw_state(crtc);
}

static void ilk_pipe_wm_get_hw_state(struct drm_crtc *crtc)
{
	struct drm_device *dev = crtc->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct ilk_wm_values *hw = &dev_priv->wm.hw;
	struct intel_crtc *intel_crtc = to_intel_crtc(crtc);
	struct intel_pipe_wm *active = &intel_crtc->wm.active;
	enum pipe pipe = intel_crtc->pipe;
	static const unsigned int wm0_pipe_reg[] = {
		[PIPE_A] = WM0_PIPEA_ILK,
		[PIPE_B] = WM0_PIPEB_ILK,
		[PIPE_C] = WM0_PIPEC_IVB,
	};

	hw->wm_pipe[pipe] = I915_READ(wm0_pipe_reg[pipe]);
	if (IS_HASWELL(dev) || IS_BROADWELL(dev))
		hw->wm_linetime[pipe] = I915_READ(PIPE_WM_LINETIME(pipe));

	memset(active, 0, sizeof(*active));

	active->pipe_enabled = intel_crtc->active;

	if (active->pipe_enabled) {
		u32 tmp = hw->wm_pipe[pipe];

		/*
		 * For active pipes LP0 watermark is marked as
		 * enabled, and LP1+ watermaks as disabled since
		 * we can't really reverse compute them in case
		 * multiple pipes are active.
		 */
		active->wm[0].enable = true;
		active->wm[0].pri_val = (tmp & WM0_PIPE_PLANE_MASK) >> WM0_PIPE_PLANE_SHIFT;
		active->wm[0].spr_val = (tmp & WM0_PIPE_SPRITE_MASK) >> WM0_PIPE_SPRITE_SHIFT;
		active->wm[0].cur_val = tmp & WM0_PIPE_CURSOR_MASK;
		active->linetime = hw->wm_linetime[pipe];
	} else {
		int level, max_level = ilk_wm_max_level(dev);

		/*
		 * For inactive pipes, all watermark levels
		 * should be marked as enabled but zeroed,
		 * which is what we'd compute them to.
		 */
		for (level = 0; level <= max_level; level++)
			active->wm[level].enable = true;
	}
}

#define _FW_WM(value, plane) \
	(((value) & DSPFW_ ## plane ## _MASK) >> DSPFW_ ## plane ## _SHIFT)
#define _FW_WM_VLV(value, plane) \
	(((value) & DSPFW_ ## plane ## _MASK_VLV) >> DSPFW_ ## plane ## _SHIFT)

static void vlv_read_wm_values(struct drm_i915_private *dev_priv,
			       struct vlv_wm_values *wm)
{
	enum pipe pipe;
	uint32_t tmp;

	for_each_pipe(dev_priv, pipe) {
		tmp = I915_READ(VLV_DDL(pipe));

		wm->ddl[pipe].primary =
			(tmp >> DDL_PLANE_SHIFT) & (DDL_PRECISION_HIGH | DRAIN_LATENCY_MASK);
		wm->ddl[pipe].cursor =
			(tmp >> DDL_CURSOR_SHIFT) & (DDL_PRECISION_HIGH | DRAIN_LATENCY_MASK);
		wm->ddl[pipe].sprite[0] =
			(tmp >> DDL_SPRITE_SHIFT(0)) & (DDL_PRECISION_HIGH | DRAIN_LATENCY_MASK);
		wm->ddl[pipe].sprite[1] =
			(tmp >> DDL_SPRITE_SHIFT(1)) & (DDL_PRECISION_HIGH | DRAIN_LATENCY_MASK);
	}

	tmp = I915_READ(DSPFW1);
	wm->sr.plane = _FW_WM(tmp, SR);
	wm->pipe[PIPE_B].cursor = _FW_WM(tmp, CURSORB);
	wm->pipe[PIPE_B].primary = _FW_WM_VLV(tmp, PLANEB);
	wm->pipe[PIPE_A].primary = _FW_WM_VLV(tmp, PLANEA);

	tmp = I915_READ(DSPFW2);
	wm->pipe[PIPE_A].sprite[1] = _FW_WM_VLV(tmp, SPRITEB);
	wm->pipe[PIPE_A].cursor = _FW_WM(tmp, CURSORA);
	wm->pipe[PIPE_A].sprite[0] = _FW_WM_VLV(tmp, SPRITEA);

	tmp = I915_READ(DSPFW3);
	wm->sr.cursor = _FW_WM(tmp, CURSOR_SR);

	if (IS_CHERRYVIEW(dev_priv)) {
		tmp = I915_READ(DSPFW7_CHV);
		wm->pipe[PIPE_B].sprite[1] = _FW_WM_VLV(tmp, SPRITED);
		wm->pipe[PIPE_B].sprite[0] = _FW_WM_VLV(tmp, SPRITEC);

		tmp = I915_READ(DSPFW8_CHV);
		wm->pipe[PIPE_C].sprite[1] = _FW_WM_VLV(tmp, SPRITEF);
		wm->pipe[PIPE_C].sprite[0] = _FW_WM_VLV(tmp, SPRITEE);

		tmp = I915_READ(DSPFW9_CHV);
		wm->pipe[PIPE_C].primary = _FW_WM_VLV(tmp, PLANEC);
		wm->pipe[PIPE_C].cursor = _FW_WM(tmp, CURSORC);

		tmp = I915_READ(DSPHOWM);
		wm->sr.plane |= _FW_WM(tmp, SR_HI) << 9;
		wm->pipe[PIPE_C].sprite[1] |= _FW_WM(tmp, SPRITEF_HI) << 8;
		wm->pipe[PIPE_C].sprite[0] |= _FW_WM(tmp, SPRITEE_HI) << 8;
		wm->pipe[PIPE_C].primary |= _FW_WM(tmp, PLANEC_HI) << 8;
		wm->pipe[PIPE_B].sprite[1] |= _FW_WM(tmp, SPRITED_HI) << 8;
		wm->pipe[PIPE_B].sprite[0] |= _FW_WM(tmp, SPRITEC_HI) << 8;
		wm->pipe[PIPE_B].primary |= _FW_WM(tmp, PLANEB_HI) << 8;
		wm->pipe[PIPE_A].sprite[1] |= _FW_WM(tmp, SPRITEB_HI) << 8;
		wm->pipe[PIPE_A].sprite[0] |= _FW_WM(tmp, SPRITEA_HI) << 8;
		wm->pipe[PIPE_A].primary |= _FW_WM(tmp, PLANEA_HI) << 8;
	} else {
		tmp = I915_READ(DSPFW7);
		wm->pipe[PIPE_B].sprite[1] = _FW_WM_VLV(tmp, SPRITED);
		wm->pipe[PIPE_B].sprite[0] = _FW_WM_VLV(tmp, SPRITEC);

		tmp = I915_READ(DSPHOWM);
		wm->sr.plane |= _FW_WM(tmp, SR_HI) << 9;
		wm->pipe[PIPE_B].sprite[1] |= _FW_WM(tmp, SPRITED_HI) << 8;
		wm->pipe[PIPE_B].sprite[0] |= _FW_WM(tmp, SPRITEC_HI) << 8;
		wm->pipe[PIPE_B].primary |= _FW_WM(tmp, PLANEB_HI) << 8;
		wm->pipe[PIPE_A].sprite[1] |= _FW_WM(tmp, SPRITEB_HI) << 8;
		wm->pipe[PIPE_A].sprite[0] |= _FW_WM(tmp, SPRITEA_HI) << 8;
		wm->pipe[PIPE_A].primary |= _FW_WM(tmp, PLANEA_HI) << 8;
	}
}

#undef _FW_WM
#undef _FW_WM_VLV

void vlv_wm_get_hw_state(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = to_i915(dev);
	struct vlv_wm_values *wm = &dev_priv->wm.vlv;
	struct intel_plane *plane;
	enum pipe pipe;
	u32 val;

	vlv_read_wm_values(dev_priv, wm);

	for_each_intel_plane(dev, plane) {
		switch (plane->base.type) {
			int sprite;
		case DRM_PLANE_TYPE_CURSOR:
			plane->wm.fifo_size = 63;
			break;
		case DRM_PLANE_TYPE_PRIMARY:
			plane->wm.fifo_size = vlv_get_fifo_size(dev, plane->pipe, 0);
			break;
		case DRM_PLANE_TYPE_OVERLAY:
			sprite = plane->plane;
			plane->wm.fifo_size = vlv_get_fifo_size(dev, plane->pipe, sprite + 1);
			break;
		}
	}

	wm->cxsr = I915_READ(FW_BLC_SELF_VLV) & FW_CSPWRDWNEN;
	wm->level = VLV_WM_LEVEL_PM2;

	if (IS_CHERRYVIEW(dev_priv)) {
		mutex_lock(&dev_priv->rps.hw_lock);

		val = vlv_punit_read(dev_priv, PUNIT_REG_DSPFREQ);
		if (val & DSP_MAXFIFO_PM5_ENABLE)
			wm->level = VLV_WM_LEVEL_PM5;

		/*
		 * If DDR DVFS is disabled in the BIOS, Punit
		 * will never ack the request. So if that happens
		 * assume we don't have to enable/disable DDR DVFS
		 * dynamically. To test that just set the REQ_ACK
		 * bit to poke the Punit, but don't change the
		 * HIGH/LOW bits so that we don't actually change
		 * the current state.
		 */
		val = vlv_punit_read(dev_priv, PUNIT_REG_DDR_SETUP2);
		val |= FORCE_DDR_FREQ_REQ_ACK;
		vlv_punit_write(dev_priv, PUNIT_REG_DDR_SETUP2, val);

		if (wait_for((vlv_punit_read(dev_priv, PUNIT_REG_DDR_SETUP2) &
			      FORCE_DDR_FREQ_REQ_ACK) == 0, 3)) {
			DRM_DEBUG_KMS("Punit not acking DDR DVFS request, "
				      "assuming DDR DVFS is disabled\n");
			dev_priv->wm.max_level = VLV_WM_LEVEL_PM5;
		} else {
			val = vlv_punit_read(dev_priv, PUNIT_REG_DDR_SETUP2);
			if ((val & FORCE_DDR_HIGH_FREQ) == 0)
				wm->level = VLV_WM_LEVEL_DDR_DVFS;
		}

		mutex_unlock(&dev_priv->rps.hw_lock);
	}

	for_each_pipe(dev_priv, pipe)
		DRM_DEBUG_KMS("Initial watermarks: pipe %c, plane=%d, cursor=%d, sprite0=%d, sprite1=%d\n",
			      pipe_name(pipe), wm->pipe[pipe].primary, wm->pipe[pipe].cursor,
			      wm->pipe[pipe].sprite[0], wm->pipe[pipe].sprite[1]);

	DRM_DEBUG_KMS("Initial watermarks: SR plane=%d, SR cursor=%d level=%d cxsr=%d\n",
		      wm->sr.plane, wm->sr.cursor, wm->level, wm->cxsr);
}

void ilk_wm_get_hw_state(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct ilk_wm_values *hw = &dev_priv->wm.hw;
	struct drm_crtc *crtc;

	for_each_crtc(dev, crtc)
		ilk_pipe_wm_get_hw_state(crtc);

	hw->wm_lp[0] = I915_READ(WM1_LP_ILK);
	hw->wm_lp[1] = I915_READ(WM2_LP_ILK);
	hw->wm_lp[2] = I915_READ(WM3_LP_ILK);

	hw->wm_lp_spr[0] = I915_READ(WM1S_LP_ILK);
	if (INTEL_INFO(dev)->gen >= 7) {
		hw->wm_lp_spr[1] = I915_READ(WM2S_LP_IVB);
		hw->wm_lp_spr[2] = I915_READ(WM3S_LP_IVB);
	}

	if (IS_HASWELL(dev) || IS_BROADWELL(dev))
		hw->partitioning = (I915_READ(WM_MISC) & WM_MISC_DATA_PARTITION_5_6) ?
			INTEL_DDB_PART_5_6 : INTEL_DDB_PART_1_2;
	else if (IS_IVYBRIDGE(dev))
		hw->partitioning = (I915_READ(DISP_ARB_CTL2) & DISP_DATA_PARTITION_5_6) ?
			INTEL_DDB_PART_5_6 : INTEL_DDB_PART_1_2;

	hw->enable_fbc_wm =
		!(I915_READ(DISP_ARB_CTL) & DISP_FBC_WM_DIS);
}

/**
 * intel_update_watermarks - update FIFO watermark values based on current modes
 *
 * Calculate watermark values for the various WM regs based on current mode
 * and plane configuration.
 *
 * There are several cases to deal with here:
 *   - normal (i.e. non-self-refresh)
 *   - self-refresh (SR) mode
 *   - lines are large relative to FIFO size (buffer can hold up to 2)
 *   - lines are small relative to FIFO size (buffer can hold more than 2
 *     lines), so need to account for TLB latency
 *
 *   The normal calculation is:
 *     watermark = dotclock * bytes per pixel * latency
 *   where latency is platform & configuration dependent (we assume pessimal
 *   values here).
 *
 *   The SR calculation is:
 *     watermark = (trunc(latency/line time)+1) * surface width *
 *       bytes per pixel
 *   where
 *     line time = htotal / dotclock
 *     surface width = hdisplay for normal plane and 64 for cursor
 *   and latency is assumed to be high, as above.
 *
 * The final value programmed to the register should always be rounded up,
 * and include an extra 2 entries to account for clock crossings.
 *
 * We don't use the sprite, so we can ignore that.  And on Crestline we have
 * to set the non-SR watermarks to 8.
 */
void intel_update_watermarks(struct drm_crtc *crtc)
{
	struct drm_i915_private *dev_priv = crtc->dev->dev_private;

	if (dev_priv->display.update_wm)
		dev_priv->display.update_wm(crtc);
}

void intel_update_sprite_watermarks(struct drm_plane *plane,
				    struct drm_crtc *crtc,
				    uint32_t sprite_width,
				    uint32_t sprite_height,
				    int pixel_size,
				    bool enabled, bool scaled)
{
	struct drm_i915_private *dev_priv = plane->dev->dev_private;

	if (dev_priv->display.update_sprite_wm)
		dev_priv->display.update_sprite_wm(plane, crtc,
						   sprite_width, sprite_height,
						   pixel_size, enabled, scaled);
}

/**
 * Lock protecting IPS related data structures
 */
DEFINE_SPINLOCK(mchdev_lock);

/* Global for IPS driver to get at the current i915 device. Protected by
 * mchdev_lock. */
static struct drm_i915_private *i915_mch_dev;

bool ironlake_set_drps(struct drm_device *dev, u8 val)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	u16 rgvswctl;

	assert_spin_locked(&mchdev_lock);

	rgvswctl = I915_READ16(MEMSWCTL);
	if (rgvswctl & MEMCTL_CMD_STS) {
		DRM_DEBUG("gpu busy, RCS change rejected\n");
		return false; /* still busy with another command */
	}

	rgvswctl = (MEMCTL_CMD_CHFREQ << MEMCTL_CMD_SHIFT) |
		(val << MEMCTL_FREQ_SHIFT) | MEMCTL_SFCAVM;
	I915_WRITE16(MEMSWCTL, rgvswctl);
	POSTING_READ16(MEMSWCTL);

	rgvswctl |= MEMCTL_CMD_STS;
	I915_WRITE16(MEMSWCTL, rgvswctl);

	return true;
}

static void ironlake_enable_drps(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	u32 rgvmodectl = I915_READ(MEMMODECTL);
	u8 fmax, fmin, fstart, vstart;

	spin_lock_irq(&mchdev_lock);

	/* Enable temp reporting */
	I915_WRITE16(PMMISC, I915_READ(PMMISC) | MCPPCE_EN);
	I915_WRITE16(TSC1, I915_READ(TSC1) | TSE);

	/* 100ms RC evaluation intervals */
	I915_WRITE(RCUPEI, 100000);
	I915_WRITE(RCDNEI, 100000);

	/* Set max/min thresholds to 90ms and 80ms respectively */
	I915_WRITE(RCBMAXAVG, 90000);
	I915_WRITE(RCBMINAVG, 80000);

	I915_WRITE(MEMIHYST, 1);

	/* Set up min, max, and cur for interrupt handling */
	fmax = (rgvmodectl & MEMMODE_FMAX_MASK) >> MEMMODE_FMAX_SHIFT;
	fmin = (rgvmodectl & MEMMODE_FMIN_MASK);
	fstart = (rgvmodectl & MEMMODE_FSTART_MASK) >>
		MEMMODE_FSTART_SHIFT;

	vstart = (I915_READ(PXVFREQ(fstart)) & PXVFREQ_PX_MASK) >>
		PXVFREQ_PX_SHIFT;

	dev_priv->ips.fmax = fmax; /* IPS callback will increase this */
	dev_priv->ips.fstart = fstart;

	dev_priv->ips.max_delay = fstart;
	dev_priv->ips.min_delay = fmin;
	dev_priv->ips.cur_delay = fstart;

	DRM_DEBUG_DRIVER("fmax: %d, fmin: %d, fstart: %d\n",
			 fmax, fmin, fstart);

	I915_WRITE(MEMINTREN, MEMINT_CX_SUPR_EN | MEMINT_EVAL_CHG_EN);

	/*
	 * Interrupts will be enabled in ironlake_irq_postinstall
	 */

	I915_WRITE(VIDSTART, vstart);
	POSTING_READ(VIDSTART);

	rgvmodectl |= MEMMODE_SWMODE_EN;
	I915_WRITE(MEMMODECTL, rgvmodectl);

	if (wait_for_atomic((I915_READ(MEMSWCTL) & MEMCTL_CMD_STS) == 0, 10))
		DRM_ERROR("stuck trying to change perf mode\n");
	mdelay(1);

	ironlake_set_drps(dev, fstart);

	dev_priv->ips.last_count1 = I915_READ(DMIEC) +
		I915_READ(DDREC) + I915_READ(CSIEC);
	dev_priv->ips.last_time1 = jiffies_to_msecs(jiffies);
	dev_priv->ips.last_count2 = I915_READ(GFXEC);
	dev_priv->ips.last_time2 = ktime_get_raw_ns();

	spin_unlock_irq(&mchdev_lock);
}

static void ironlake_disable_drps(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	u16 rgvswctl;

	spin_lock_irq(&mchdev_lock);

	rgvswctl = I915_READ16(MEMSWCTL);

	/* Ack interrupts, disable EFC interrupt */
	I915_WRITE(MEMINTREN, I915_READ(MEMINTREN) & ~MEMINT_EVAL_CHG_EN);
	I915_WRITE(MEMINTRSTS, MEMINT_EVAL_CHG);
	I915_WRITE(DEIER, I915_READ(DEIER) & ~DE_PCU_EVENT);
	I915_WRITE(DEIIR, DE_PCU_EVENT);
	I915_WRITE(DEIMR, I915_READ(DEIMR) | DE_PCU_EVENT);

	/* Go back to the starting frequency */
	ironlake_set_drps(dev, dev_priv->ips.fstart);
	mdelay(1);
	rgvswctl |= MEMCTL_CMD_STS;
	I915_WRITE(MEMSWCTL, rgvswctl);
	mdelay(1);

	spin_unlock_irq(&mchdev_lock);
}

/* There's a funny hw issue where the hw returns all 0 when reading from
 * GEN6_RP_INTERRUPT_LIMITS. Hence we always need to compute the desired value
 * ourselves, instead of doing a rmw cycle (which might result in us clearing
 * all limits and the gpu stuck at whatever frequency it is at atm).
 */
static u32 intel_rps_limits(struct drm_i915_private *dev_priv, u8 val)
{
	u32 limits;

	/* Only set the down limit when we've reached the lowest level to avoid
	 * getting more interrupts, otherwise leave this clear. This prevents a
	 * race in the hw when coming out of rc6: There's a tiny window where
	 * the hw runs at the minimal clock before selecting the desired
	 * frequency, if the down threshold expires in that window we will not
	 * receive a down interrupt. */
	if (IS_GEN9(dev_priv->dev)) {
		limits = (dev_priv->rps.max_freq_softlimit) << 23;
		if (val <= dev_priv->rps.min_freq_softlimit)
			limits |= (dev_priv->rps.min_freq_softlimit) << 14;
	} else {
		limits = dev_priv->rps.max_freq_softlimit << 24;
		if (val <= dev_priv->rps.min_freq_softlimit)
			limits |= dev_priv->rps.min_freq_softlimit << 16;
	}

	return limits;
}

static void gen6_set_rps_thresholds(struct drm_i915_private *dev_priv, u8 val)
{
	int new_power;
	u32 threshold_up = 0, threshold_down = 0; /* in % */
	u32 ei_up = 0, ei_down = 0;

	new_power = dev_priv->rps.power;
	switch (dev_priv->rps.power) {
	case LOW_POWER:
		if (val > dev_priv->rps.efficient_freq + 1 && val > dev_priv->rps.cur_freq)
			new_power = BETWEEN;
		break;

	case BETWEEN:
		if (val <= dev_priv->rps.efficient_freq && val < dev_priv->rps.cur_freq)
			new_power = LOW_POWER;
		else if (val >= dev_priv->rps.rp0_freq && val > dev_priv->rps.cur_freq)
			new_power = HIGH_POWER;
		break;

	case HIGH_POWER:
		if (val < (dev_priv->rps.rp1_freq + dev_priv->rps.rp0_freq) >> 1 && val < dev_priv->rps.cur_freq)
			new_power = BETWEEN;
		break;
	}
	/* Max/min bins are special */
	if (val <= dev_priv->rps.min_freq_softlimit)
		new_power = LOW_POWER;
	if (val >= dev_priv->rps.max_freq_softlimit)
		new_power = HIGH_POWER;
	if (new_power == dev_priv->rps.power)
		return;

	/* Note the units here are not exactly 1us, but 1280ns. */
	switch (new_power) {
	case LOW_POWER:
		/* Upclock if more than 95% busy over 16ms */
		ei_up = 16000;
		threshold_up = 95;

		/* Downclock if less than 85% busy over 32ms */
		ei_down = 32000;
		threshold_down = 85;
		break;

	case BETWEEN:
		/* Upclock if more than 90% busy over 13ms */
		ei_up = 13000;
		threshold_up = 90;

		/* Downclock if less than 75% busy over 32ms */
		ei_down = 32000;
		threshold_down = 75;
		break;

	case HIGH_POWER:
		/* Upclock if more than 85% busy over 10ms */
		ei_up = 10000;
		threshold_up = 85;

		/* Downclock if less than 60% busy over 32ms */
		ei_down = 32000;
		threshold_down = 60;
		break;
	}

	/* When byt can survive without system hang with dynamic
	 * sw freq adjustments, this restriction can be lifted.
	 */
	if (IS_VALLEYVIEW(dev_priv))
		goto skip_hw_write;

	I915_WRITE(GEN6_RP_UP_EI,
		GT_INTERVAL_FROM_US(dev_priv, ei_up));
	I915_WRITE(GEN6_RP_UP_THRESHOLD,
		GT_INTERVAL_FROM_US(dev_priv, (ei_up * threshold_up / 100)));

	I915_WRITE(GEN6_RP_DOWN_EI,
		GT_INTERVAL_FROM_US(dev_priv, ei_down));
	I915_WRITE(GEN6_RP_DOWN_THRESHOLD,
		GT_INTERVAL_FROM_US(dev_priv, (ei_down * threshold_down / 100)));

	 I915_WRITE(GEN6_RP_CONTROL,
		    GEN6_RP_MEDIA_TURBO |
		    GEN6_RP_MEDIA_HW_NORMAL_MODE |
		    GEN6_RP_MEDIA_IS_GFX |
		    GEN6_RP_ENABLE |
		    GEN6_RP_UP_BUSY_AVG |
		    GEN6_RP_DOWN_IDLE_AVG);

skip_hw_write:
	dev_priv->rps.power = new_power;
	dev_priv->rps.up_threshold = threshold_up;
	dev_priv->rps.down_threshold = threshold_down;
	dev_priv->rps.last_adj = 0;
}

static u32 gen6_rps_pm_mask(struct drm_i915_private *dev_priv, u8 val)
{
	u32 mask = 0;

	/* We use UP_EI_EXPIRED interupts for both up/down in manual mode */
	if (val > dev_priv->rps.min_freq_softlimit)
		mask |= GEN6_PM_RP_UP_EI_EXPIRED | GEN6_PM_RP_DOWN_THRESHOLD | GEN6_PM_RP_DOWN_TIMEOUT;
	if (val < dev_priv->rps.max_freq_softlimit)
		mask |= GEN6_PM_RP_UP_EI_EXPIRED | GEN6_PM_RP_UP_THRESHOLD;

	mask &= dev_priv->pm_rps_events;

	return gen6_sanitize_rps_pm_mask(dev_priv, ~mask);
}

/* gen6_set_rps is called to update the frequency request, but should also be
 * called when the range (min_delay and max_delay) is modified so that we can
 * update the GEN6_RP_INTERRUPT_LIMITS register accordingly. */
static void gen6_set_rps(struct drm_device *dev, u8 val)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	/* WaGsvDisableTurbo: Workaround to disable turbo on BXT A* */
	if (IS_BXT_REVID(dev, 0, BXT_REVID_A1))
		return;

	WARN_ON(!mutex_is_locked(&dev_priv->rps.hw_lock));
	WARN_ON(val > dev_priv->rps.max_freq);
	WARN_ON(val < dev_priv->rps.min_freq);

	/* min/max delay may still have been modified so be sure to
	 * write the limits value.
	 */
	if (val != dev_priv->rps.cur_freq) {
		gen6_set_rps_thresholds(dev_priv, val);

		if (IS_GEN9(dev))
			I915_WRITE(GEN6_RPNSWREQ,
				   GEN9_FREQUENCY(val));
		else if (IS_HASWELL(dev) || IS_BROADWELL(dev))
			I915_WRITE(GEN6_RPNSWREQ,
				   HSW_FREQUENCY(val));
		else
			I915_WRITE(GEN6_RPNSWREQ,
				   GEN6_FREQUENCY(val) |
				   GEN6_OFFSET(0) |
				   GEN6_AGGRESSIVE_TURBO);
	}

	/* Make sure we continue to get interrupts
	 * until we hit the minimum or maximum frequencies.
	 */
	I915_WRITE(GEN6_RP_INTERRUPT_LIMITS, intel_rps_limits(dev_priv, val));
	I915_WRITE(GEN6_PMINTRMSK, gen6_rps_pm_mask(dev_priv, val));

	POSTING_READ(GEN6_RPNSWREQ);

	dev_priv->rps.cur_freq = val;
	trace_intel_gpu_freq_change(intel_gpu_freq(dev_priv, val));
}

static void valleyview_set_rps(struct drm_device *dev, u8 val)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	WARN_ON(!mutex_is_locked(&dev_priv->rps.hw_lock));
	WARN_ON(val > dev_priv->rps.max_freq);
	WARN_ON(val < dev_priv->rps.min_freq);

	if (WARN_ONCE(IS_CHERRYVIEW(dev) && (val & 1),
		      "Odd GPU freq value\n"))
		val &= ~1;

	I915_WRITE(GEN6_PMINTRMSK, gen6_rps_pm_mask(dev_priv, val));

	if (val != dev_priv->rps.cur_freq) {
		vlv_punit_write(dev_priv, PUNIT_REG_GPU_FREQ_REQ, val);
		if (!IS_CHERRYVIEW(dev_priv))
			gen6_set_rps_thresholds(dev_priv, val);
	}

	dev_priv->rps.cur_freq = val;
	trace_intel_gpu_freq_change(intel_gpu_freq(dev_priv, val));
}

/* vlv_set_rps_idle: Set the frequency to idle, if Gfx clocks are down
 *
 * * If Gfx is Idle, then
 * 1. Forcewake Media well.
 * 2. Request idle freq.
 * 3. Release Forcewake of Media well.
*/
static void vlv_set_rps_idle(struct drm_i915_private *dev_priv)
{
	u32 val = dev_priv->rps.idle_freq;

	if (dev_priv->rps.cur_freq <= val)
		return;

	/* Wake up the media well, as that takes a lot less
	 * power than the Render well. */
	intel_uncore_forcewake_get(dev_priv, FORCEWAKE_MEDIA);
	valleyview_set_rps(dev_priv->dev, val);
	intel_uncore_forcewake_put(dev_priv, FORCEWAKE_MEDIA);
}

void gen6_rps_busy(struct drm_i915_private *dev_priv)
{
	mutex_lock(&dev_priv->rps.hw_lock);
	if (dev_priv->rps.enabled) {
		if (dev_priv->pm_rps_events & GEN6_PM_RP_UP_EI_EXPIRED)
			gen6_rps_reset_ei(dev_priv);
		I915_WRITE(GEN6_PMINTRMSK,
			   gen6_rps_pm_mask(dev_priv, dev_priv->rps.cur_freq));
	}
	mutex_unlock(&dev_priv->rps.hw_lock);
}

void gen6_rps_idle(struct drm_i915_private *dev_priv)
{
	struct drm_device *dev = dev_priv->dev;

	mutex_lock(&dev_priv->rps.hw_lock);
	if (dev_priv->rps.enabled) {
		if (IS_VALLEYVIEW(dev))
			vlv_set_rps_idle(dev_priv);
		else
			gen6_set_rps(dev_priv->dev, dev_priv->rps.idle_freq);
		dev_priv->rps.last_adj = 0;
		I915_WRITE(GEN6_PMINTRMSK,
			   gen6_sanitize_rps_pm_mask(dev_priv, ~0));
	}
	mutex_unlock(&dev_priv->rps.hw_lock);

	spin_lock(&dev_priv->rps.client_lock);
	while (!list_empty(&dev_priv->rps.clients))
		list_del_init(dev_priv->rps.clients.next);
	spin_unlock(&dev_priv->rps.client_lock);
}

void gen6_rps_boost(struct drm_i915_private *dev_priv,
		    struct intel_rps_client *rps,
		    unsigned long submitted)
{
	/* This is intentionally racy! We peek at the state here, then
	 * validate inside the RPS worker.
	 */
	if (!(dev_priv->mm.busy &&
	      dev_priv->rps.enabled &&
	      dev_priv->rps.cur_freq < dev_priv->rps.max_freq_softlimit))
		return;

	/* Force a RPS boost (and don't count it against the client) if
	 * the GPU is severely congested.
	 */
	if (rps && time_after(jiffies, submitted + DRM_I915_THROTTLE_JIFFIES))
		rps = NULL;

	spin_lock(&dev_priv->rps.client_lock);
	if (rps == NULL || list_empty(&rps->link)) {
		spin_lock_irq(&dev_priv->irq_lock);
		if (dev_priv->rps.interrupts_enabled) {
			dev_priv->rps.client_boost = true;
			queue_work(dev_priv->wq, &dev_priv->rps.work);
		}
		spin_unlock_irq(&dev_priv->irq_lock);

		if (rps != NULL) {
			list_add(&rps->link, &dev_priv->rps.clients);
			rps->boosts++;
		} else
			dev_priv->rps.boosts++;
	}
	spin_unlock(&dev_priv->rps.client_lock);
}

void intel_set_rps(struct drm_device *dev, u8 val)
{
	if (IS_VALLEYVIEW(dev))
		valleyview_set_rps(dev, val);
	else
		gen6_set_rps(dev, val);
}

static void gen9_disable_rps(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	I915_WRITE(GEN6_RC_CONTROL, 0);
	I915_WRITE(GEN9_PG_ENABLE, 0);
}

static void gen6_disable_rps(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	I915_WRITE(GEN6_RC_CONTROL, 0);
	I915_WRITE(GEN6_RPNSWREQ, 1 << 31);
}

static void cherryview_disable_rps(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	I915_WRITE(GEN6_RC_CONTROL, 0);
}

static void valleyview_disable_rps(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	/* we're doing forcewake before Disabling RC6,
	 * This what the BIOS expects when going into suspend */
	intel_uncore_forcewake_get(dev_priv, FORCEWAKE_ALL);

	I915_WRITE(GEN6_RC_CONTROL, 0);

	intel_uncore_forcewake_put(dev_priv, FORCEWAKE_ALL);
}

static void intel_print_rc6_info(struct drm_device *dev, u32 mode)
{
	if (IS_VALLEYVIEW(dev)) {
		if (mode & (GEN7_RC_CTL_TO_MODE | GEN6_RC_CTL_EI_MODE(1)))
			mode = GEN6_RC_CTL_RC6_ENABLE;
		else
			mode = 0;
	}
	if (HAS_RC6p(dev))
		DRM_DEBUG_KMS("Enabling RC6 states: RC6 %s RC6p %s RC6pp %s\n",
			      (mode & GEN6_RC_CTL_RC6_ENABLE) ? "on" : "off",
			      (mode & GEN6_RC_CTL_RC6p_ENABLE) ? "on" : "off",
			      (mode & GEN6_RC_CTL_RC6pp_ENABLE) ? "on" : "off");

	else
		DRM_DEBUG_KMS("Enabling RC6 states: RC6 %s\n",
			      (mode & GEN6_RC_CTL_RC6_ENABLE) ? "on" : "off");
}

static int sanitize_rc6_option(const struct drm_device *dev, int enable_rc6)
{
	/* No RC6 before Ironlake and code is gone for ilk. */
	if (INTEL_INFO(dev)->gen < 6)
		return 0;

	/* Respect the kernel parameter if it is set */
	if (enable_rc6 >= 0) {
		int mask;

		if (HAS_RC6p(dev))
			mask = INTEL_RC6_ENABLE | INTEL_RC6p_ENABLE |
			       INTEL_RC6pp_ENABLE;
		else
			mask = INTEL_RC6_ENABLE;

		if ((enable_rc6 & mask) != enable_rc6)
			DRM_DEBUG_KMS("Adjusting RC6 mask to %d (requested %d, valid %d)\n",
				      enable_rc6 & mask, enable_rc6, mask);

		return enable_rc6 & mask;
	}

	if (IS_IVYBRIDGE(dev))
		return (INTEL_RC6_ENABLE | INTEL_RC6p_ENABLE);

	return INTEL_RC6_ENABLE;
}

int intel_enable_rc6(const struct drm_device *dev)
{
	return i915.enable_rc6;
}

static void gen6_init_rps_frequencies(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	uint32_t rp_state_cap;
	u32 ddcc_status = 0;
	int ret;

	/* All of these values are in units of 50MHz */
	dev_priv->rps.cur_freq		= 0;
	/* static values from HW: RP0 > RP1 > RPn (min_freq) */
	if (IS_BROXTON(dev)) {
		rp_state_cap = I915_READ(BXT_RP_STATE_CAP);
		dev_priv->rps.rp0_freq = (rp_state_cap >> 16) & 0xff;
		dev_priv->rps.rp1_freq = (rp_state_cap >>  8) & 0xff;
		dev_priv->rps.min_freq = (rp_state_cap >>  0) & 0xff;
	} else {
		rp_state_cap = I915_READ(GEN6_RP_STATE_CAP);
		dev_priv->rps.rp0_freq = (rp_state_cap >>  0) & 0xff;
		dev_priv->rps.rp1_freq = (rp_state_cap >>  8) & 0xff;
		dev_priv->rps.min_freq = (rp_state_cap >> 16) & 0xff;
	}

	/* hw_max = RP0 until we check for overclocking */
	dev_priv->rps.max_freq		= dev_priv->rps.rp0_freq;

	dev_priv->rps.efficient_freq = dev_priv->rps.rp1_freq;
	if (IS_HASWELL(dev) || IS_BROADWELL(dev) ||
	    IS_SKYLAKE(dev) || IS_KABYLAKE(dev)) {
		ret = sandybridge_pcode_read(dev_priv,
					HSW_PCODE_DYNAMIC_DUTY_CYCLE_CONTROL,
					&ddcc_status);
		if (0 == ret)
			dev_priv->rps.efficient_freq =
				clamp_t(u8,
					((ddcc_status >> 8) & 0xff),
					dev_priv->rps.min_freq,
					dev_priv->rps.max_freq);
	}

	if (IS_SKYLAKE(dev) || IS_KABYLAKE(dev)) {
		/* Store the frequency values in 16.66 MHZ units, which is
		   the natural hardware unit for SKL */
		dev_priv->rps.rp0_freq *= GEN9_FREQ_SCALER;
		dev_priv->rps.rp1_freq *= GEN9_FREQ_SCALER;
		dev_priv->rps.min_freq *= GEN9_FREQ_SCALER;
		dev_priv->rps.max_freq *= GEN9_FREQ_SCALER;
		dev_priv->rps.efficient_freq *= GEN9_FREQ_SCALER;
	}

	dev_priv->rps.idle_freq = dev_priv->rps.min_freq;

	/* Preserve min/max settings in case of re-init */
	if (dev_priv->rps.max_freq_softlimit == 0)
		dev_priv->rps.max_freq_softlimit = dev_priv->rps.max_freq;

	if (dev_priv->rps.min_freq_softlimit == 0) {
		if (IS_HASWELL(dev) || IS_BROADWELL(dev))
			dev_priv->rps.min_freq_softlimit =
				max_t(int, dev_priv->rps.efficient_freq,
				      intel_freq_opcode(dev_priv, 450));
		else
			dev_priv->rps.min_freq_softlimit =
				dev_priv->rps.min_freq;
	}
}

/* See the Gen9_GT_PM_Programming_Guide doc for the below */
static void gen9_enable_rps(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	intel_uncore_forcewake_get(dev_priv, FORCEWAKE_ALL);

	gen6_init_rps_frequencies(dev);

	/* WaGsvDisableTurbo: Workaround to disable turbo on BXT A* */
	if (IS_BXT_REVID(dev, 0, BXT_REVID_A1)) {
		intel_uncore_forcewake_put(dev_priv, FORCEWAKE_ALL);
		return;
	}

	/* Program defaults and thresholds for RPS*/
	I915_WRITE(GEN6_RC_VIDEO_FREQ,
		GEN9_FREQUENCY(dev_priv->rps.rp1_freq));

	/* 1 second timeout*/
	I915_WRITE(GEN6_RP_DOWN_TIMEOUT,
		GT_INTERVAL_FROM_US(dev_priv, 1000000));

	I915_WRITE(GEN6_RP_IDLE_HYSTERSIS, 0xa);

	/* Leaning on the below call to gen6_set_rps to program/setup the
	 * Up/Down EI & threshold registers, as well as the RP_CONTROL,
	 * RP_INTERRUPT_LIMITS & RPNSWREQ registers */
	dev_priv->rps.power = HIGH_POWER; /* force a reset */
	gen6_set_rps(dev_priv->dev, dev_priv->rps.min_freq_softlimit);

	intel_uncore_forcewake_put(dev_priv, FORCEWAKE_ALL);
}

static void gen9_enable_rc6(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_engine_cs *ring;
	uint32_t rc6_mask = 0;
	int unused;

	/* 1a: Software RC state - RC0 */
	I915_WRITE(GEN6_RC_STATE, 0);

	/* 1b: Get forcewake during program sequence. Although the driver
	 * hasn't enabled a state yet where we need forcewake, BIOS may have.*/
	intel_uncore_forcewake_get(dev_priv, FORCEWAKE_ALL);

	/* 2a: Disable RC states. */
	I915_WRITE(GEN6_RC_CONTROL, 0);

	/* 2b: Program RC6 thresholds.*/

	/* WaRsDoubleRc6WrlWithCoarsePowerGating: Doubling WRL only when CPG is enabled */
	if (IS_SKYLAKE(dev))
		I915_WRITE(GEN6_RC6_WAKE_RATE_LIMIT, 108 << 16);
	else
		I915_WRITE(GEN6_RC6_WAKE_RATE_LIMIT, 54 << 16);
	I915_WRITE(GEN6_RC_EVALUATION_INTERVAL, 125000); /* 12500 * 1280ns */
	I915_WRITE(GEN6_RC_IDLE_HYSTERSIS, 25); /* 25 * 1280ns */
	for_each_ring(ring, dev_priv, unused)
		I915_WRITE(RING_MAX_IDLE(ring->mmio_base), 10);

	if (HAS_GUC_UCODE(dev))
		I915_WRITE(GUC_MAX_IDLE_COUNT, 0xA);

	I915_WRITE(GEN6_RC_SLEEP, 0);

	/* 2c: Program Coarse Power Gating Policies. */
	I915_WRITE(GEN9_MEDIA_PG_IDLE_HYSTERESIS, 25);
	I915_WRITE(GEN9_RENDER_PG_IDLE_HYSTERESIS, 25);

	/* 3a: Enable RC6 */
	if (intel_enable_rc6(dev) & INTEL_RC6_ENABLE)
		rc6_mask = GEN6_RC_CTL_RC6_ENABLE;
	DRM_INFO("RC6 %s\n", (rc6_mask & GEN6_RC_CTL_RC6_ENABLE) ?
			"on" : "off");
	/* WaRsUseTimeoutMode */
	if (IS_SKL_REVID(dev, 0, SKL_REVID_D0) ||
	    IS_BXT_REVID(dev, 0, BXT_REVID_A0)) {
		I915_WRITE(GEN6_RC6_THRESHOLD, 625); /* 800us */
		I915_WRITE(GEN6_RC_CONTROL, GEN6_RC_CTL_HW_ENABLE |
			   GEN7_RC_CTL_TO_MODE |
			   rc6_mask);
	} else {
		I915_WRITE(GEN6_RC6_THRESHOLD, 37500); /* 37.5/125ms per EI */
		I915_WRITE(GEN6_RC_CONTROL, GEN6_RC_CTL_HW_ENABLE |
			   GEN6_RC_CTL_EI_MODE(1) |
			   rc6_mask);
	}

	/*
	 * 3b: Enable Coarse Power Gating only when RC6 is enabled.
	 * WaRsDisableCoarsePowerGating:skl,bxt - Render/Media PG need to be disabled with RC6.
	 */
	if (IS_BXT_REVID(dev, 0, BXT_REVID_A1) ||
	    ((IS_SKL_GT3(dev) || IS_SKL_GT4(dev)) &&
	     IS_SKL_REVID(dev, 0, SKL_REVID_E0)))
		I915_WRITE(GEN9_PG_ENABLE, 0);
	else
		I915_WRITE(GEN9_PG_ENABLE, (rc6_mask & GEN6_RC_CTL_RC6_ENABLE) ?
				(GEN9_RENDER_PG_ENABLE | GEN9_MEDIA_PG_ENABLE) : 0);

	intel_uncore_forcewake_put(dev_priv, FORCEWAKE_ALL);

}

static void gen8_enable_rps(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_engine_cs *ring;
	uint32_t rc6_mask = 0;
	int unused;

	/* 1a: Software RC state - RC0 */
	I915_WRITE(GEN6_RC_STATE, 0);

	/* 1c & 1d: Get forcewake during program sequence. Although the driver
	 * hasn't enabled a state yet where we need forcewake, BIOS may have.*/
	intel_uncore_forcewake_get(dev_priv, FORCEWAKE_ALL);

	/* 2a: Disable RC states. */
	I915_WRITE(GEN6_RC_CONTROL, 0);

	/* Initialize rps frequencies */
	gen6_init_rps_frequencies(dev);

	/* 2b: Program RC6 thresholds.*/
	I915_WRITE(GEN6_RC6_WAKE_RATE_LIMIT, 40 << 16);
	I915_WRITE(GEN6_RC_EVALUATION_INTERVAL, 125000); /* 12500 * 1280ns */
	I915_WRITE(GEN6_RC_IDLE_HYSTERSIS, 25); /* 25 * 1280ns */
	for_each_ring(ring, dev_priv, unused)
		I915_WRITE(RING_MAX_IDLE(ring->mmio_base), 10);
	I915_WRITE(GEN6_RC_SLEEP, 0);
	if (IS_BROADWELL(dev))
		I915_WRITE(GEN6_RC6_THRESHOLD, 625); /* 800us/1.28 for TO */
	else
		I915_WRITE(GEN6_RC6_THRESHOLD, 50000); /* 50/125ms per EI */

	/* 3: Enable RC6 */
	if (intel_enable_rc6(dev) & INTEL_RC6_ENABLE)
		rc6_mask = GEN6_RC_CTL_RC6_ENABLE;
	intel_print_rc6_info(dev, rc6_mask);
	if (IS_BROADWELL(dev))
		I915_WRITE(GEN6_RC_CONTROL, GEN6_RC_CTL_HW_ENABLE |
				GEN7_RC_CTL_TO_MODE |
				rc6_mask);
	else
		I915_WRITE(GEN6_RC_CONTROL, GEN6_RC_CTL_HW_ENABLE |
				GEN6_RC_CTL_EI_MODE(1) |
				rc6_mask);

	/* 4 Program defaults and thresholds for RPS*/
	I915_WRITE(GEN6_RPNSWREQ,
		   HSW_FREQUENCY(dev_priv->rps.rp1_freq));
	I915_WRITE(GEN6_RC_VIDEO_FREQ,
		   HSW_FREQUENCY(dev_priv->rps.rp1_freq));
	/* NB: Docs say 1s, and 1000000 - which aren't equivalent */
	I915_WRITE(GEN6_RP_DOWN_TIMEOUT, 100000000 / 128); /* 1 second timeout */

	/* Docs recommend 900MHz, and 300 MHz respectively */
	I915_WRITE(GEN6_RP_INTERRUPT_LIMITS,
		   dev_priv->rps.max_freq_softlimit << 24 |
		   dev_priv->rps.min_freq_softlimit << 16);

	I915_WRITE(GEN6_RP_UP_THRESHOLD, 7600000 / 128); /* 76ms busyness per EI, 90% */
	I915_WRITE(GEN6_RP_DOWN_THRESHOLD, 31300000 / 128); /* 313ms busyness per EI, 70%*/
	I915_WRITE(GEN6_RP_UP_EI, 66000); /* 84.48ms, XXX: random? */
	I915_WRITE(GEN6_RP_DOWN_EI, 350000); /* 448ms, XXX: random? */

	I915_WRITE(GEN6_RP_IDLE_HYSTERSIS, 10);

	/* 5: Enable RPS */
	I915_WRITE(GEN6_RP_CONTROL,
		   GEN6_RP_MEDIA_TURBO |
		   GEN6_RP_MEDIA_HW_NORMAL_MODE |
		   GEN6_RP_MEDIA_IS_GFX |
		   GEN6_RP_ENABLE |
		   GEN6_RP_UP_BUSY_AVG |
		   GEN6_RP_DOWN_IDLE_AVG);

	/* 6: Ring frequency + overclocking (our driver does this later */

	dev_priv->rps.power = HIGH_POWER; /* force a reset */
	gen6_set_rps(dev_priv->dev, dev_priv->rps.idle_freq);

	intel_uncore_forcewake_put(dev_priv, FORCEWAKE_ALL);
}

static void gen6_enable_rps(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_engine_cs *ring;
	u32 rc6vids, pcu_mbox = 0, rc6_mask = 0;
	u32 gtfifodbg;
	int rc6_mode;
	int i, ret;

	WARN_ON(!mutex_is_locked(&dev_priv->rps.hw_lock));

	/* Here begins a magic sequence of register writes to enable
	 * auto-downclocking.
	 *
	 * Perhaps there might be some value in exposing these to
	 * userspace...
	 */
	I915_WRITE(GEN6_RC_STATE, 0);

	/* Clear the DBG now so we don't confuse earlier errors */
	if ((gtfifodbg = I915_READ(GTFIFODBG))) {
		DRM_ERROR("GT fifo had a previous error %x\n", gtfifodbg);
		I915_WRITE(GTFIFODBG, gtfifodbg);
	}

	intel_uncore_forcewake_get(dev_priv, FORCEWAKE_ALL);

	/* Initialize rps frequencies */
	gen6_init_rps_frequencies(dev);

	/* disable the counters and set deterministic thresholds */
	I915_WRITE(GEN6_RC_CONTROL, 0);

	I915_WRITE(GEN6_RC1_WAKE_RATE_LIMIT, 1000 << 16);
	I915_WRITE(GEN6_RC6_WAKE_RATE_LIMIT, 40 << 16 | 30);
	I915_WRITE(GEN6_RC6pp_WAKE_RATE_LIMIT, 30);
	I915_WRITE(GEN6_RC_EVALUATION_INTERVAL, 125000);
	I915_WRITE(GEN6_RC_IDLE_HYSTERSIS, 25);

	for_each_ring(ring, dev_priv, i)
		I915_WRITE(RING_MAX_IDLE(ring->mmio_base), 10);

	I915_WRITE(GEN6_RC_SLEEP, 0);
	I915_WRITE(GEN6_RC1e_THRESHOLD, 1000);
	if (IS_IVYBRIDGE(dev))
		I915_WRITE(GEN6_RC6_THRESHOLD, 125000);
	else
		I915_WRITE(GEN6_RC6_THRESHOLD, 50000);
	I915_WRITE(GEN6_RC6p_THRESHOLD, 150000);
	I915_WRITE(GEN6_RC6pp_THRESHOLD, 64000); /* unused */

	/* Check if we are enabling RC6 */
	rc6_mode = intel_enable_rc6(dev_priv->dev);
	if (rc6_mode & INTEL_RC6_ENABLE)
		rc6_mask |= GEN6_RC_CTL_RC6_ENABLE;

	/* We don't use those on Haswell */
	if (!IS_HASWELL(dev)) {
		if (rc6_mode & INTEL_RC6p_ENABLE)
			rc6_mask |= GEN6_RC_CTL_RC6p_ENABLE;

		if (rc6_mode & INTEL_RC6pp_ENABLE)
			rc6_mask |= GEN6_RC_CTL_RC6pp_ENABLE;
	}

	intel_print_rc6_info(dev, rc6_mask);

	I915_WRITE(GEN6_RC_CONTROL,
		   rc6_mask |
		   GEN6_RC_CTL_EI_MODE(1) |
		   GEN6_RC_CTL_HW_ENABLE);

	/* Power down if completely idle for over 50ms */
	I915_WRITE(GEN6_RP_DOWN_TIMEOUT, 50000);
	I915_WRITE(GEN6_RP_IDLE_HYSTERSIS, 10);

	ret = sandybridge_pcode_write(dev_priv, GEN6_PCODE_WRITE_MIN_FREQ_TABLE, 0);
	if (ret)
		DRM_DEBUG_DRIVER("Failed to set the min frequency\n");

	ret = sandybridge_pcode_read(dev_priv, GEN6_READ_OC_PARAMS, &pcu_mbox);
	if (!ret && (pcu_mbox & (1<<31))) { /* OC supported */
		DRM_DEBUG_DRIVER("Overclocking supported. Max: %dMHz, Overclock max: %dMHz\n",
				 (dev_priv->rps.max_freq_softlimit & 0xff) * 50,
				 (pcu_mbox & 0xff) * 50);
		dev_priv->rps.max_freq = pcu_mbox & 0xff;
	}

	dev_priv->rps.power = HIGH_POWER; /* force a reset */
	gen6_set_rps(dev_priv->dev, dev_priv->rps.idle_freq);

	rc6vids = 0;
	ret = sandybridge_pcode_read(dev_priv, GEN6_PCODE_READ_RC6VIDS, &rc6vids);
	if (IS_GEN6(dev) && ret) {
		DRM_DEBUG_DRIVER("Couldn't check for BIOS workaround\n");
	} else if (IS_GEN6(dev) && (GEN6_DECODE_RC6_VID(rc6vids & 0xff) < 450)) {
		DRM_DEBUG_DRIVER("You should update your BIOS. Correcting minimum rc6 voltage (%dmV->%dmV)\n",
			  GEN6_DECODE_RC6_VID(rc6vids & 0xff), 450);
		rc6vids &= 0xffff00;
		rc6vids |= GEN6_ENCODE_RC6_VID(450);
		ret = sandybridge_pcode_write(dev_priv, GEN6_PCODE_WRITE_RC6VIDS, rc6vids);
		if (ret)
			DRM_ERROR("Couldn't fix incorrect rc6 voltage\n");
	}

	intel_uncore_forcewake_put(dev_priv, FORCEWAKE_ALL);
}

static void __gen6_update_ring_freq(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	int min_freq = 15;
	unsigned int gpu_freq;
	unsigned int max_ia_freq, min_ring_freq;
	unsigned int max_gpu_freq, min_gpu_freq;
	int scaling_factor = 180;
#ifdef notyet
	struct cpufreq_policy *policy;
#endif

	WARN_ON(!mutex_is_locked(&dev_priv->rps.hw_lock));

#ifdef notyet
	policy = cpufreq_cpu_get(0);
	if (policy) {
		max_ia_freq = policy->cpuinfo.max_freq;
		cpufreq_cpu_put(policy);
	} else {
		/*
		 * Default to measured freq if none found, PCU will ensure we
		 * don't go over
		 */
		max_ia_freq = tsc_khz;
	}
#else
	/* XXX we ideally want the max not cpuspeed... */
	max_ia_freq = cpuspeed;
#endif

	/* Convert from kHz to MHz */
	max_ia_freq /= 1000;

	min_ring_freq = I915_READ(DCLK) & 0xf;
	/* convert DDR frequency from units of 266.6MHz to bandwidth */
	min_ring_freq = mult_frac(min_ring_freq, 8, 3);

	if (IS_SKYLAKE(dev) || IS_KABYLAKE(dev)) {
		/* Convert GT frequency to 50 HZ units */
		min_gpu_freq = dev_priv->rps.min_freq / GEN9_FREQ_SCALER;
		max_gpu_freq = dev_priv->rps.max_freq / GEN9_FREQ_SCALER;
	} else {
		min_gpu_freq = dev_priv->rps.min_freq;
		max_gpu_freq = dev_priv->rps.max_freq;
	}

	/*
	 * For each potential GPU frequency, load a ring frequency we'd like
	 * to use for memory access.  We do this by specifying the IA frequency
	 * the PCU should use as a reference to determine the ring frequency.
	 */
	for (gpu_freq = max_gpu_freq; gpu_freq >= min_gpu_freq; gpu_freq--) {
		int diff = max_gpu_freq - gpu_freq;
		unsigned int ia_freq = 0, ring_freq = 0;

		if (IS_SKYLAKE(dev) || IS_KABYLAKE(dev)) {
			/*
			 * ring_freq = 2 * GT. ring_freq is in 100MHz units
			 * No floor required for ring frequency on SKL.
			 */
			ring_freq = gpu_freq;
		} else if (INTEL_INFO(dev)->gen >= 8) {
			/* max(2 * GT, DDR). NB: GT is 50MHz units */
			ring_freq = max(min_ring_freq, gpu_freq);
		} else if (IS_HASWELL(dev)) {
			ring_freq = mult_frac(gpu_freq, 5, 4);
			ring_freq = max(min_ring_freq, ring_freq);
			/* leave ia_freq as the default, chosen by cpufreq */
		} else {
			/* On older processors, there is no separate ring
			 * clock domain, so in order to boost the bandwidth
			 * of the ring, we need to upclock the CPU (ia_freq).
			 *
			 * For GPU frequencies less than 750MHz,
			 * just use the lowest ring freq.
			 */
			if (gpu_freq < min_freq)
				ia_freq = 800;
			else
				ia_freq = max_ia_freq - ((diff * scaling_factor) / 2);
			ia_freq = DIV_ROUND_CLOSEST(ia_freq, 100);
		}

		sandybridge_pcode_write(dev_priv,
					GEN6_PCODE_WRITE_MIN_FREQ_TABLE,
					ia_freq << GEN6_PCODE_FREQ_IA_RATIO_SHIFT |
					ring_freq << GEN6_PCODE_FREQ_RING_RATIO_SHIFT |
					gpu_freq);
	}
}

void gen6_update_ring_freq(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	if (!HAS_CORE_RING_FREQ(dev))
		return;

	mutex_lock(&dev_priv->rps.hw_lock);
	__gen6_update_ring_freq(dev);
	mutex_unlock(&dev_priv->rps.hw_lock);
}

static int cherryview_rps_max_freq(struct drm_i915_private *dev_priv)
{
	struct drm_device *dev = dev_priv->dev;
	u32 val, rp0;

	val = vlv_punit_read(dev_priv, FB_GFX_FMAX_AT_VMAX_FUSE);

	switch (INTEL_INFO(dev)->eu_total) {
	case 8:
		/* (2 * 4) config */
		rp0 = (val >> FB_GFX_FMAX_AT_VMAX_2SS4EU_FUSE_SHIFT);
		break;
	case 12:
		/* (2 * 6) config */
		rp0 = (val >> FB_GFX_FMAX_AT_VMAX_2SS6EU_FUSE_SHIFT);
		break;
	case 16:
		/* (2 * 8) config */
	default:
		/* Setting (2 * 8) Min RP0 for any other combination */
		rp0 = (val >> FB_GFX_FMAX_AT_VMAX_2SS8EU_FUSE_SHIFT);
		break;
	}

	rp0 = (rp0 & FB_GFX_FREQ_FUSE_MASK);

	return rp0;
}

static int cherryview_rps_rpe_freq(struct drm_i915_private *dev_priv)
{
	u32 val, rpe;

	val = vlv_punit_read(dev_priv, PUNIT_GPU_DUTYCYCLE_REG);
	rpe = (val >> PUNIT_GPU_DUTYCYCLE_RPE_FREQ_SHIFT) & PUNIT_GPU_DUTYCYCLE_RPE_FREQ_MASK;

	return rpe;
}

static int cherryview_rps_guar_freq(struct drm_i915_private *dev_priv)
{
	u32 val, rp1;

	val = vlv_punit_read(dev_priv, FB_GFX_FMAX_AT_VMAX_FUSE);
	rp1 = (val & FB_GFX_FREQ_FUSE_MASK);

	return rp1;
}

static int valleyview_rps_guar_freq(struct drm_i915_private *dev_priv)
{
	u32 val, rp1;

	val = vlv_nc_read(dev_priv, IOSF_NC_FB_GFX_FREQ_FUSE);

	rp1 = (val & FB_GFX_FGUARANTEED_FREQ_FUSE_MASK) >> FB_GFX_FGUARANTEED_FREQ_FUSE_SHIFT;

	return rp1;
}

static int valleyview_rps_max_freq(struct drm_i915_private *dev_priv)
{
	u32 val, rp0;

	val = vlv_nc_read(dev_priv, IOSF_NC_FB_GFX_FREQ_FUSE);

	rp0 = (val & FB_GFX_MAX_FREQ_FUSE_MASK) >> FB_GFX_MAX_FREQ_FUSE_SHIFT;
	/* Clamp to max */
	rp0 = min_t(u32, rp0, 0xea);

	return rp0;
}

static int valleyview_rps_rpe_freq(struct drm_i915_private *dev_priv)
{
	u32 val, rpe;

	val = vlv_nc_read(dev_priv, IOSF_NC_FB_GFX_FMAX_FUSE_LO);
	rpe = (val & FB_FMAX_VMIN_FREQ_LO_MASK) >> FB_FMAX_VMIN_FREQ_LO_SHIFT;
	val = vlv_nc_read(dev_priv, IOSF_NC_FB_GFX_FMAX_FUSE_HI);
	rpe |= (val & FB_FMAX_VMIN_FREQ_HI_MASK) << 5;

	return rpe;
}

static int valleyview_rps_min_freq(struct drm_i915_private *dev_priv)
{
	return vlv_punit_read(dev_priv, PUNIT_REG_GPU_LFM) & 0xff;
}

/* Check that the pctx buffer wasn't move under us. */
static void valleyview_check_pctx(struct drm_i915_private *dev_priv)
{
	unsigned long pctx_addr = I915_READ(VLV_PCBR) & ~4095;

	WARN_ON(pctx_addr != dev_priv->mm.stolen_base +
			     dev_priv->vlv_pctx->stolen->start);
}


/* Check that the pcbr address is not empty. */
static void cherryview_check_pctx(struct drm_i915_private *dev_priv)
{
	unsigned long pctx_addr = I915_READ(VLV_PCBR) & ~4095;

	WARN_ON((pctx_addr >> VLV_PCBR_ADDR_SHIFT) == 0);
}

static void cherryview_setup_pctx(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	unsigned long pctx_paddr, paddr;
	struct i915_gtt *gtt = &dev_priv->gtt;
	u32 pcbr;
	int pctx_size = 32*1024;

	WARN_ON(!mutex_is_locked(&dev->struct_mutex));

	pcbr = I915_READ(VLV_PCBR);
	if ((pcbr >> VLV_PCBR_ADDR_SHIFT) == 0) {
		DRM_DEBUG_DRIVER("BIOS didn't set up PCBR, fixing up\n");
		paddr = (dev_priv->mm.stolen_base +
			 (gtt->stolen_size - pctx_size));

		pctx_paddr = (paddr & (~4095));
		I915_WRITE(VLV_PCBR, pctx_paddr);
	}

	DRM_DEBUG_DRIVER("PCBR: 0x%08x\n", I915_READ(VLV_PCBR));
}

static void valleyview_setup_pctx(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_i915_gem_object *pctx;
	unsigned long pctx_paddr;
	u32 pcbr;
	int pctx_size = 24*1024;

	WARN_ON(!mutex_is_locked(&dev->struct_mutex));

	pcbr = I915_READ(VLV_PCBR);
	if (pcbr) {
		/* BIOS set it up already, grab the pre-alloc'd space */
		int pcbr_offset;

		pcbr_offset = (pcbr & (~4095)) - dev_priv->mm.stolen_base;
		pctx = i915_gem_object_create_stolen_for_preallocated(dev_priv->dev,
								      pcbr_offset,
								      I915_GTT_OFFSET_NONE,
								      pctx_size);
		goto out;
	}

	DRM_DEBUG_DRIVER("BIOS didn't set up PCBR, fixing up\n");

	/*
	 * From the Gunit register HAS:
	 * The Gfx driver is expected to program this register and ensure
	 * proper allocation within Gfx stolen memory.  For example, this
	 * register should be programmed such than the PCBR range does not
	 * overlap with other ranges, such as the frame buffer, protected
	 * memory, or any other relevant ranges.
	 */
	pctx = i915_gem_object_create_stolen(dev, pctx_size);
	if (!pctx) {
		DRM_DEBUG("not enough stolen space for PCTX, disabling\n");
		return;
	}

	pctx_paddr = dev_priv->mm.stolen_base + pctx->stolen->start;
	I915_WRITE(VLV_PCBR, pctx_paddr);

out:
	DRM_DEBUG_DRIVER("PCBR: 0x%08x\n", I915_READ(VLV_PCBR));
	dev_priv->vlv_pctx = pctx;
}

static void valleyview_cleanup_pctx(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	if (WARN_ON(!dev_priv->vlv_pctx))
		return;

	drm_gem_object_unreference(&dev_priv->vlv_pctx->base);
	dev_priv->vlv_pctx = NULL;
}

static void valleyview_init_gt_powersave(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	u32 val;

	valleyview_setup_pctx(dev);

	mutex_lock(&dev_priv->rps.hw_lock);

	val = vlv_punit_read(dev_priv, PUNIT_REG_GPU_FREQ_STS);
	switch ((val >> 6) & 3) {
	case 0:
	case 1:
		dev_priv->mem_freq = 800;
		break;
	case 2:
		dev_priv->mem_freq = 1066;
		break;
	case 3:
		dev_priv->mem_freq = 1333;
		break;
	}
	DRM_DEBUG_DRIVER("DDR speed: %d MHz\n", dev_priv->mem_freq);

	dev_priv->rps.max_freq = valleyview_rps_max_freq(dev_priv);
	dev_priv->rps.rp0_freq = dev_priv->rps.max_freq;
	DRM_DEBUG_DRIVER("max GPU freq: %d MHz (%u)\n",
			 intel_gpu_freq(dev_priv, dev_priv->rps.max_freq),
			 dev_priv->rps.max_freq);

	dev_priv->rps.efficient_freq = valleyview_rps_rpe_freq(dev_priv);
	DRM_DEBUG_DRIVER("RPe GPU freq: %d MHz (%u)\n",
			 intel_gpu_freq(dev_priv, dev_priv->rps.efficient_freq),
			 dev_priv->rps.efficient_freq);

	dev_priv->rps.rp1_freq = valleyview_rps_guar_freq(dev_priv);
	DRM_DEBUG_DRIVER("RP1(Guar Freq) GPU freq: %d MHz (%u)\n",
			 intel_gpu_freq(dev_priv, dev_priv->rps.rp1_freq),
			 dev_priv->rps.rp1_freq);

	dev_priv->rps.min_freq = valleyview_rps_min_freq(dev_priv);
	DRM_DEBUG_DRIVER("min GPU freq: %d MHz (%u)\n",
			 intel_gpu_freq(dev_priv, dev_priv->rps.min_freq),
			 dev_priv->rps.min_freq);

	dev_priv->rps.idle_freq = dev_priv->rps.min_freq;

	/* Preserve min/max settings in case of re-init */
	if (dev_priv->rps.max_freq_softlimit == 0)
		dev_priv->rps.max_freq_softlimit = dev_priv->rps.max_freq;

	if (dev_priv->rps.min_freq_softlimit == 0)
		dev_priv->rps.min_freq_softlimit = dev_priv->rps.min_freq;

	mutex_unlock(&dev_priv->rps.hw_lock);
}

static void cherryview_init_gt_powersave(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	u32 val;

	cherryview_setup_pctx(dev);

	mutex_lock(&dev_priv->rps.hw_lock);

	mutex_lock(&dev_priv->sb_lock);
	val = vlv_cck_read(dev_priv, CCK_FUSE_REG);
	mutex_unlock(&dev_priv->sb_lock);

	switch ((val >> 2) & 0x7) {
	case 3:
		dev_priv->mem_freq = 2000;
		break;
	default:
		dev_priv->mem_freq = 1600;
		break;
	}
	DRM_DEBUG_DRIVER("DDR speed: %d MHz\n", dev_priv->mem_freq);

	dev_priv->rps.max_freq = cherryview_rps_max_freq(dev_priv);
	dev_priv->rps.rp0_freq = dev_priv->rps.max_freq;
	DRM_DEBUG_DRIVER("max GPU freq: %d MHz (%u)\n",
			 intel_gpu_freq(dev_priv, dev_priv->rps.max_freq),
			 dev_priv->rps.max_freq);

	dev_priv->rps.efficient_freq = cherryview_rps_rpe_freq(dev_priv);
	DRM_DEBUG_DRIVER("RPe GPU freq: %d MHz (%u)\n",
			 intel_gpu_freq(dev_priv, dev_priv->rps.efficient_freq),
			 dev_priv->rps.efficient_freq);

	dev_priv->rps.rp1_freq = cherryview_rps_guar_freq(dev_priv);
	DRM_DEBUG_DRIVER("RP1(Guar) GPU freq: %d MHz (%u)\n",
			 intel_gpu_freq(dev_priv, dev_priv->rps.rp1_freq),
			 dev_priv->rps.rp1_freq);

	/* PUnit validated range is only [RPe, RP0] */
	dev_priv->rps.min_freq = dev_priv->rps.efficient_freq;
	DRM_DEBUG_DRIVER("min GPU freq: %d MHz (%u)\n",
			 intel_gpu_freq(dev_priv, dev_priv->rps.min_freq),
			 dev_priv->rps.min_freq);

	WARN_ONCE((dev_priv->rps.max_freq |
		   dev_priv->rps.efficient_freq |
		   dev_priv->rps.rp1_freq |
		   dev_priv->rps.min_freq) & 1,
		  "Odd GPU freq values\n");

	dev_priv->rps.idle_freq = dev_priv->rps.min_freq;

	/* Preserve min/max settings in case of re-init */
	if (dev_priv->rps.max_freq_softlimit == 0)
		dev_priv->rps.max_freq_softlimit = dev_priv->rps.max_freq;

	if (dev_priv->rps.min_freq_softlimit == 0)
		dev_priv->rps.min_freq_softlimit = dev_priv->rps.min_freq;

	mutex_unlock(&dev_priv->rps.hw_lock);
}

static void valleyview_cleanup_gt_powersave(struct drm_device *dev)
{
	valleyview_cleanup_pctx(dev);
}

static void cherryview_enable_rps(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_engine_cs *ring;
	u32 gtfifodbg, val, rc6_mode = 0, pcbr;
	int i;

	WARN_ON(!mutex_is_locked(&dev_priv->rps.hw_lock));

	gtfifodbg = I915_READ(GTFIFODBG);
	if (gtfifodbg) {
		DRM_DEBUG_DRIVER("GT fifo had a previous error %x\n",
				 gtfifodbg);
		I915_WRITE(GTFIFODBG, gtfifodbg);
	}

	cherryview_check_pctx(dev_priv);

	/* 1a & 1b: Get forcewake during program sequence. Although the driver
	 * hasn't enabled a state yet where we need forcewake, BIOS may have.*/
	intel_uncore_forcewake_get(dev_priv, FORCEWAKE_ALL);

	/*  Disable RC states. */
	I915_WRITE(GEN6_RC_CONTROL, 0);

	/* 2a: Program RC6 thresholds.*/
	I915_WRITE(GEN6_RC6_WAKE_RATE_LIMIT, 40 << 16);
	I915_WRITE(GEN6_RC_EVALUATION_INTERVAL, 125000); /* 12500 * 1280ns */
	I915_WRITE(GEN6_RC_IDLE_HYSTERSIS, 25); /* 25 * 1280ns */

	for_each_ring(ring, dev_priv, i)
		I915_WRITE(RING_MAX_IDLE(ring->mmio_base), 10);
	I915_WRITE(GEN6_RC_SLEEP, 0);

	/* TO threshold set to 500 us ( 0x186 * 1.28 us) */
	I915_WRITE(GEN6_RC6_THRESHOLD, 0x186);

	/* allows RC6 residency counter to work */
	I915_WRITE(VLV_COUNTER_CONTROL,
		   _MASKED_BIT_ENABLE(VLV_COUNT_RANGE_HIGH |
				      VLV_MEDIA_RC6_COUNT_EN |
				      VLV_RENDER_RC6_COUNT_EN));

	/* For now we assume BIOS is allocating and populating the PCBR  */
	pcbr = I915_READ(VLV_PCBR);

	/* 3: Enable RC6 */
	if ((intel_enable_rc6(dev) & INTEL_RC6_ENABLE) &&
						(pcbr >> VLV_PCBR_ADDR_SHIFT))
		rc6_mode = GEN7_RC_CTL_TO_MODE;

	I915_WRITE(GEN6_RC_CONTROL, rc6_mode);

	/* 4 Program defaults and thresholds for RPS*/
	I915_WRITE(GEN6_RP_DOWN_TIMEOUT, 1000000);
	I915_WRITE(GEN6_RP_UP_THRESHOLD, 59400);
	I915_WRITE(GEN6_RP_DOWN_THRESHOLD, 245000);
	I915_WRITE(GEN6_RP_UP_EI, 66000);
	I915_WRITE(GEN6_RP_DOWN_EI, 350000);

	I915_WRITE(GEN6_RP_IDLE_HYSTERSIS, 10);

	/* 5: Enable RPS */
	I915_WRITE(GEN6_RP_CONTROL,
		   GEN6_RP_MEDIA_HW_NORMAL_MODE |
		   GEN6_RP_MEDIA_IS_GFX |
		   GEN6_RP_ENABLE |
		   GEN6_RP_UP_BUSY_AVG |
		   GEN6_RP_DOWN_IDLE_AVG);

	/* Setting Fixed Bias */
	val = VLV_OVERRIDE_EN |
		  VLV_SOC_TDP_EN |
		  CHV_BIAS_CPU_50_SOC_50;
	vlv_punit_write(dev_priv, VLV_TURBO_SOC_OVERRIDE, val);

	val = vlv_punit_read(dev_priv, PUNIT_REG_GPU_FREQ_STS);

	/* RPS code assumes GPLL is used */
	WARN_ONCE((val & GPLLENABLE) == 0, "GPLL not enabled\n");

	DRM_DEBUG_DRIVER("GPLL enabled? %s\n", yesno(val & GPLLENABLE));
	DRM_DEBUG_DRIVER("GPU status: 0x%08x\n", val);

	dev_priv->rps.cur_freq = (val >> 8) & 0xff;
	DRM_DEBUG_DRIVER("current GPU freq: %d MHz (%u)\n",
			 intel_gpu_freq(dev_priv, dev_priv->rps.cur_freq),
			 dev_priv->rps.cur_freq);

	DRM_DEBUG_DRIVER("setting GPU freq to %d MHz (%u)\n",
			 intel_gpu_freq(dev_priv, dev_priv->rps.efficient_freq),
			 dev_priv->rps.efficient_freq);

	valleyview_set_rps(dev_priv->dev, dev_priv->rps.efficient_freq);

	intel_uncore_forcewake_put(dev_priv, FORCEWAKE_ALL);
}

static void valleyview_enable_rps(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_engine_cs *ring;
	u32 gtfifodbg, val, rc6_mode = 0;
	int i;

	WARN_ON(!mutex_is_locked(&dev_priv->rps.hw_lock));

	valleyview_check_pctx(dev_priv);

	if ((gtfifodbg = I915_READ(GTFIFODBG))) {
		DRM_DEBUG_DRIVER("GT fifo had a previous error %x\n",
				 gtfifodbg);
		I915_WRITE(GTFIFODBG, gtfifodbg);
	}

	/* If VLV, Forcewake all wells, else re-direct to regular path */
	intel_uncore_forcewake_get(dev_priv, FORCEWAKE_ALL);

	/*  Disable RC states. */
	I915_WRITE(GEN6_RC_CONTROL, 0);

	I915_WRITE(GEN6_RP_DOWN_TIMEOUT, 1000000);
	I915_WRITE(GEN6_RP_UP_THRESHOLD, 59400);
	I915_WRITE(GEN6_RP_DOWN_THRESHOLD, 245000);
	I915_WRITE(GEN6_RP_UP_EI, 66000);
	I915_WRITE(GEN6_RP_DOWN_EI, 350000);

	I915_WRITE(GEN6_RP_IDLE_HYSTERSIS, 10);

	I915_WRITE(GEN6_RP_CONTROL,
		   GEN6_RP_MEDIA_TURBO |
		   GEN6_RP_MEDIA_HW_NORMAL_MODE |
		   GEN6_RP_MEDIA_IS_GFX |
		   GEN6_RP_ENABLE |
		   GEN6_RP_UP_BUSY_AVG |
		   GEN6_RP_DOWN_IDLE_CONT);

	I915_WRITE(GEN6_RC6_WAKE_RATE_LIMIT, 0x00280000);
	I915_WRITE(GEN6_RC_EVALUATION_INTERVAL, 125000);
	I915_WRITE(GEN6_RC_IDLE_HYSTERSIS, 25);

	for_each_ring(ring, dev_priv, i)
		I915_WRITE(RING_MAX_IDLE(ring->mmio_base), 10);

	I915_WRITE(GEN6_RC6_THRESHOLD, 0x557);

	/* allows RC6 residency counter to work */
	I915_WRITE(VLV_COUNTER_CONTROL,
		   _MASKED_BIT_ENABLE(VLV_MEDIA_RC0_COUNT_EN |
				      VLV_RENDER_RC0_COUNT_EN |
				      VLV_MEDIA_RC6_COUNT_EN |
				      VLV_RENDER_RC6_COUNT_EN));

	if (intel_enable_rc6(dev) & INTEL_RC6_ENABLE)
		rc6_mode = GEN7_RC_CTL_TO_MODE | VLV_RC_CTL_CTX_RST_PARALLEL;

	intel_print_rc6_info(dev, rc6_mode);

	I915_WRITE(GEN6_RC_CONTROL, rc6_mode);

	/* Setting Fixed Bias */
	val = VLV_OVERRIDE_EN |
		  VLV_SOC_TDP_EN |
		  VLV_BIAS_CPU_125_SOC_875;
	vlv_punit_write(dev_priv, VLV_TURBO_SOC_OVERRIDE, val);

	val = vlv_punit_read(dev_priv, PUNIT_REG_GPU_FREQ_STS);

	/* RPS code assumes GPLL is used */
	WARN_ONCE((val & GPLLENABLE) == 0, "GPLL not enabled\n");

	DRM_DEBUG_DRIVER("GPLL enabled? %s\n", yesno(val & GPLLENABLE));
	DRM_DEBUG_DRIVER("GPU status: 0x%08x\n", val);

	dev_priv->rps.cur_freq = (val >> 8) & 0xff;
	DRM_DEBUG_DRIVER("current GPU freq: %d MHz (%u)\n",
			 intel_gpu_freq(dev_priv, dev_priv->rps.cur_freq),
			 dev_priv->rps.cur_freq);

	DRM_DEBUG_DRIVER("setting GPU freq to %d MHz (%u)\n",
			 intel_gpu_freq(dev_priv, dev_priv->rps.efficient_freq),
			 dev_priv->rps.efficient_freq);

	valleyview_set_rps(dev_priv->dev, dev_priv->rps.efficient_freq);

	intel_uncore_forcewake_put(dev_priv, FORCEWAKE_ALL);
}

static unsigned long intel_pxfreq(u32 vidfreq)
{
	unsigned long freq;
	int div = (vidfreq & 0x3f0000) >> 16;
	int post = (vidfreq & 0x3000) >> 12;
	int pre = (vidfreq & 0x7);

	if (!pre)
		return 0;

	freq = ((div * 133333) / ((1<<post) * pre));

	return freq;
}

static const struct cparams {
	u16 i;
	u16 t;
	u16 m;
	u16 c;
} cparams[] = {
	{ 1, 1333, 301, 28664 },
	{ 1, 1066, 294, 24460 },
	{ 1, 800, 294, 25192 },
	{ 0, 1333, 276, 27605 },
	{ 0, 1066, 276, 27605 },
	{ 0, 800, 231, 23784 },
};

static unsigned long __i915_chipset_val(struct drm_i915_private *dev_priv)
{
	u64 total_count, diff, ret;
	u32 count1, count2, count3, m = 0, c = 0;
	unsigned long now = jiffies_to_msecs(jiffies), diff1;
	int i;

	assert_spin_locked(&mchdev_lock);

	diff1 = now - dev_priv->ips.last_time1;

	/* Prevent division-by-zero if we are asking too fast.
	 * Also, we don't get interesting results if we are polling
	 * faster than once in 10ms, so just return the saved value
	 * in such cases.
	 */
	if (diff1 <= 10)
		return dev_priv->ips.chipset_power;

	count1 = I915_READ(DMIEC);
	count2 = I915_READ(DDREC);
	count3 = I915_READ(CSIEC);

	total_count = count1 + count2 + count3;

	/* FIXME: handle per-counter overflow */
	if (total_count < dev_priv->ips.last_count1) {
		diff = ~0UL - dev_priv->ips.last_count1;
		diff += total_count;
	} else {
		diff = total_count - dev_priv->ips.last_count1;
	}

	for (i = 0; i < ARRAY_SIZE(cparams); i++) {
		if (cparams[i].i == dev_priv->ips.c_m &&
		    cparams[i].t == dev_priv->ips.r_t) {
			m = cparams[i].m;
			c = cparams[i].c;
			break;
		}
	}

	diff = div_u64(diff, diff1);
	ret = ((m * diff) + c);
	ret = div_u64(ret, 10);

	dev_priv->ips.last_count1 = total_count;
	dev_priv->ips.last_time1 = now;

	dev_priv->ips.chipset_power = ret;

	return ret;
}

unsigned long i915_chipset_val(struct drm_i915_private *dev_priv)
{
	struct drm_device *dev = dev_priv->dev;
	unsigned long val;

	if (INTEL_INFO(dev)->gen != 5)
		return 0;

	spin_lock_irq(&mchdev_lock);

	val = __i915_chipset_val(dev_priv);

	spin_unlock_irq(&mchdev_lock);

	return val;
}

unsigned long i915_mch_val(struct drm_i915_private *dev_priv)
{
	unsigned long m, x, b;
	u32 tsfs;

	tsfs = I915_READ(TSFS);

	m = ((tsfs & TSFS_SLOPE_MASK) >> TSFS_SLOPE_SHIFT);
	x = I915_READ8(TR1);

	b = tsfs & TSFS_INTR_MASK;

	return ((m * x) / 127) - b;
}

static int _pxvid_to_vd(u8 pxvid)
{
	if (pxvid == 0)
		return 0;

	if (pxvid >= 8 && pxvid < 31)
		pxvid = 31;

	return (pxvid + 2) * 125;
}

static u32 pvid_to_extvid(struct drm_i915_private *dev_priv, u8 pxvid)
{
	struct drm_device *dev = dev_priv->dev;
	const int vd = _pxvid_to_vd(pxvid);
	const int vm = vd - 1125;

	if (INTEL_INFO(dev)->is_mobile)
		return vm > 0 ? vm : 0;

	return vd;
}

static void __i915_update_gfx_val(struct drm_i915_private *dev_priv)
{
	u64 now, diff, diffms;
	u32 count;

	assert_spin_locked(&mchdev_lock);

	now = ktime_get_raw_ns();
	diffms = now - dev_priv->ips.last_time2;
	do_div(diffms, NSEC_PER_MSEC);

	/* Don't divide by 0 */
	if (!diffms)
		return;

	count = I915_READ(GFXEC);

	if (count < dev_priv->ips.last_count2) {
		diff = ~0UL - dev_priv->ips.last_count2;
		diff += count;
	} else {
		diff = count - dev_priv->ips.last_count2;
	}

	dev_priv->ips.last_count2 = count;
	dev_priv->ips.last_time2 = now;

	/* More magic constants... */
	diff = diff * 1181;
	diff = div_u64(diff, diffms * 10);
	dev_priv->ips.gfx_power = diff;
}

void i915_update_gfx_val(struct drm_i915_private *dev_priv)
{
	struct drm_device *dev = dev_priv->dev;

	if (INTEL_INFO(dev)->gen != 5)
		return;

	spin_lock_irq(&mchdev_lock);

	__i915_update_gfx_val(dev_priv);

	spin_unlock_irq(&mchdev_lock);
}

static unsigned long __i915_gfx_val(struct drm_i915_private *dev_priv)
{
	unsigned long t, corr, state1, corr2, state2;
	u32 pxvid, ext_v;

	assert_spin_locked(&mchdev_lock);

	pxvid = I915_READ(PXVFREQ(dev_priv->rps.cur_freq));
	pxvid = (pxvid >> 24) & 0x7f;
	ext_v = pvid_to_extvid(dev_priv, pxvid);

	state1 = ext_v;

	t = i915_mch_val(dev_priv);

	/* Revel in the empirically derived constants */

	/* Correction factor in 1/100000 units */
	if (t > 80)
		corr = ((t * 2349) + 135940);
	else if (t >= 50)
		corr = ((t * 964) + 29317);
	else /* < 50 */
		corr = ((t * 301) + 1004);

	corr = corr * ((150142 * state1) / 10000 - 78642);
	corr /= 100000;
	corr2 = (corr * dev_priv->ips.corr);

	state2 = (corr2 * state1) / 10000;
	state2 /= 100; /* convert to mW */

	__i915_update_gfx_val(dev_priv);

	return dev_priv->ips.gfx_power + state2;
}

unsigned long i915_gfx_val(struct drm_i915_private *dev_priv)
{
	struct drm_device *dev = dev_priv->dev;
	unsigned long val;

	if (INTEL_INFO(dev)->gen != 5)
		return 0;

	spin_lock_irq(&mchdev_lock);

	val = __i915_gfx_val(dev_priv);

	spin_unlock_irq(&mchdev_lock);

	return val;
}

#ifdef __linux__
/**
 * i915_read_mch_val - return value for IPS use
 *
 * Calculate and return a value for the IPS driver to use when deciding whether
 * we have thermal and power headroom to increase CPU or GPU power budget.
 */
unsigned long i915_read_mch_val(void)
{
	struct drm_i915_private *dev_priv;
	unsigned long chipset_val, graphics_val, ret = 0;

	spin_lock_irq(&mchdev_lock);
	if (!i915_mch_dev)
		goto out_unlock;
	dev_priv = i915_mch_dev;

	chipset_val = __i915_chipset_val(dev_priv);
	graphics_val = __i915_gfx_val(dev_priv);

	ret = chipset_val + graphics_val;

out_unlock:
	spin_unlock_irq(&mchdev_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(i915_read_mch_val);

/**
 * i915_gpu_raise - raise GPU frequency limit
 *
 * Raise the limit; IPS indicates we have thermal headroom.
 */
bool i915_gpu_raise(void)
{
	struct drm_i915_private *dev_priv;
	bool ret = true;

	spin_lock_irq(&mchdev_lock);
	if (!i915_mch_dev) {
		ret = false;
		goto out_unlock;
	}
	dev_priv = i915_mch_dev;

	if (dev_priv->ips.max_delay > dev_priv->ips.fmax)
		dev_priv->ips.max_delay--;

out_unlock:
	spin_unlock_irq(&mchdev_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(i915_gpu_raise);

/**
 * i915_gpu_lower - lower GPU frequency limit
 *
 * IPS indicates we're close to a thermal limit, so throttle back the GPU
 * frequency maximum.
 */
bool i915_gpu_lower(void)
{
	struct drm_i915_private *dev_priv;
	bool ret = true;

	spin_lock_irq(&mchdev_lock);
	if (!i915_mch_dev) {
		ret = false;
		goto out_unlock;
	}
	dev_priv = i915_mch_dev;

	if (dev_priv->ips.max_delay < dev_priv->ips.min_delay)
		dev_priv->ips.max_delay++;

out_unlock:
	spin_unlock_irq(&mchdev_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(i915_gpu_lower);

/**
 * i915_gpu_busy - indicate GPU business to IPS
 *
 * Tell the IPS driver whether or not the GPU is busy.
 */
bool i915_gpu_busy(void)
{
	struct drm_i915_private *dev_priv;
	struct intel_engine_cs *ring;
	bool ret = false;
	int i;

	spin_lock_irq(&mchdev_lock);
	if (!i915_mch_dev)
		goto out_unlock;
	dev_priv = i915_mch_dev;

	for_each_ring(ring, dev_priv, i)
		ret |= !list_empty(&ring->request_list);

out_unlock:
	spin_unlock_irq(&mchdev_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(i915_gpu_busy);

/**
 * i915_gpu_turbo_disable - disable graphics turbo
 *
 * Disable graphics turbo by resetting the max frequency and setting the
 * current frequency to the default.
 */
bool i915_gpu_turbo_disable(void)
{
	struct drm_i915_private *dev_priv;
	bool ret = true;

	spin_lock_irq(&mchdev_lock);
	if (!i915_mch_dev) {
		ret = false;
		goto out_unlock;
	}
	dev_priv = i915_mch_dev;

	dev_priv->ips.max_delay = dev_priv->ips.fstart;

	if (!ironlake_set_drps(dev_priv->dev, dev_priv->ips.fstart))
		ret = false;

out_unlock:
	spin_unlock_irq(&mchdev_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(i915_gpu_turbo_disable);

/**
 * Tells the intel_ips driver that the i915 driver is now loaded, if
 * IPS got loaded first.
 *
 * This awkward dance is so that neither module has to depend on the
 * other in order for IPS to do the appropriate communication of
 * GPU turbo limits to i915.
 */
static void
ips_ping_for_i915_load(void)
{
	void (*link)(void);

	link = symbol_get(ips_link_to_i915_driver);
	if (link) {
		link();
		symbol_put(ips_link_to_i915_driver);
	}
}
#endif

void intel_gpu_ips_init(struct drm_i915_private *dev_priv)
{
	/* We only register the i915 ips part with intel-ips once everything is
	 * set up, to avoid intel-ips sneaking in and reading bogus values. */
	spin_lock_irq(&mchdev_lock);
	i915_mch_dev = dev_priv;
	spin_unlock_irq(&mchdev_lock);

#ifdef __linux__
	ips_ping_for_i915_load();
#endif
}

void intel_gpu_ips_teardown(void)
{
#ifdef __linix__
	spin_lock_irq(&mchdev_lock);
	i915_mch_dev = NULL;
	spin_unlock_irq(&mchdev_lock);
#endif
}

static void intel_init_emon(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	u32 lcfuse;
	u8 pxw[16];
	int i;

	/* Disable to program */
	I915_WRITE(ECR, 0);
	POSTING_READ(ECR);

	/* Program energy weights for various events */
	I915_WRITE(SDEW, 0x15040d00);
	I915_WRITE(CSIEW0, 0x007f0000);
	I915_WRITE(CSIEW1, 0x1e220004);
	I915_WRITE(CSIEW2, 0x04000004);

	for (i = 0; i < 5; i++)
		I915_WRITE(PEW(i), 0);
	for (i = 0; i < 3; i++)
		I915_WRITE(DEW(i), 0);

	/* Program P-state weights to account for frequency power adjustment */
	for (i = 0; i < 16; i++) {
		u32 pxvidfreq = I915_READ(PXVFREQ(i));
		unsigned long freq = intel_pxfreq(pxvidfreq);
		unsigned long vid = (pxvidfreq & PXVFREQ_PX_MASK) >>
			PXVFREQ_PX_SHIFT;
		unsigned long val;

		val = vid * vid;
		val *= (freq / 1000);
		val *= 255;
		val /= (127*127*900);
		if (val > 0xff)
			DRM_ERROR("bad pxval: %ld\n", val);
		pxw[i] = val;
	}
	/* Render standby states get 0 weight */
	pxw[14] = 0;
	pxw[15] = 0;

	for (i = 0; i < 4; i++) {
		u32 val = (pxw[i*4] << 24) | (pxw[(i*4)+1] << 16) |
			(pxw[(i*4)+2] << 8) | (pxw[(i*4)+3]);
		I915_WRITE(PXW(i), val);
	}

	/* Adjust magic regs to magic values (more experimental results) */
	I915_WRITE(OGW0, 0);
	I915_WRITE(OGW1, 0);
	I915_WRITE(EG0, 0x00007f00);
	I915_WRITE(EG1, 0x0000000e);
	I915_WRITE(EG2, 0x000e0000);
	I915_WRITE(EG3, 0x68000300);
	I915_WRITE(EG4, 0x42000000);
	I915_WRITE(EG5, 0x00140031);
	I915_WRITE(EG6, 0);
	I915_WRITE(EG7, 0);

	for (i = 0; i < 8; i++)
		I915_WRITE(PXWL(i), 0);

	/* Enable PMON + select events */
	I915_WRITE(ECR, 0x80000019);

	lcfuse = I915_READ(LCFUSE02);

	dev_priv->ips.corr = (lcfuse & LCFUSE_HIV_MASK);
}

void intel_init_gt_powersave(struct drm_device *dev)
{
	i915.enable_rc6 = sanitize_rc6_option(dev, i915.enable_rc6);

	if (IS_CHERRYVIEW(dev))
		cherryview_init_gt_powersave(dev);
	else if (IS_VALLEYVIEW(dev))
		valleyview_init_gt_powersave(dev);
}

void intel_cleanup_gt_powersave(struct drm_device *dev)
{
	if (IS_CHERRYVIEW(dev))
		return;
	else if (IS_VALLEYVIEW(dev))
		valleyview_cleanup_gt_powersave(dev);
}

static void gen6_suspend_rps(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	flush_delayed_work(&dev_priv->rps.delayed_resume_work);

	gen6_disable_rps_interrupts(dev);
}

/**
 * intel_suspend_gt_powersave - suspend PM work and helper threads
 * @dev: drm device
 *
 * We don't want to disable RC6 or other features here, we just want
 * to make sure any work we've queued has finished and won't bother
 * us while we're suspended.
 */
void intel_suspend_gt_powersave(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	if (INTEL_INFO(dev)->gen < 6)
		return;

	gen6_suspend_rps(dev);

	/* Force GPU to min freq during suspend */
	gen6_rps_idle(dev_priv);
}

void intel_disable_gt_powersave(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	if (IS_IRONLAKE_M(dev)) {
		ironlake_disable_drps(dev);
	} else if (INTEL_INFO(dev)->gen >= 6) {
		intel_suspend_gt_powersave(dev);

		mutex_lock(&dev_priv->rps.hw_lock);
		if (INTEL_INFO(dev)->gen >= 9)
			gen9_disable_rps(dev);
		else if (IS_CHERRYVIEW(dev))
			cherryview_disable_rps(dev);
		else if (IS_VALLEYVIEW(dev))
			valleyview_disable_rps(dev);
		else
			gen6_disable_rps(dev);

		dev_priv->rps.enabled = false;
		mutex_unlock(&dev_priv->rps.hw_lock);
	}
}

static void intel_gen6_powersave_work(struct work_struct *work)
{
	struct drm_i915_private *dev_priv =
		container_of(work, struct drm_i915_private,
			     rps.delayed_resume_work.work);
	struct drm_device *dev = dev_priv->dev;

	mutex_lock(&dev_priv->rps.hw_lock);

	gen6_reset_rps_interrupts(dev);

	if (IS_CHERRYVIEW(dev)) {
		cherryview_enable_rps(dev);
	} else if (IS_VALLEYVIEW(dev)) {
		valleyview_enable_rps(dev);
	} else if (INTEL_INFO(dev)->gen >= 9) {
		gen9_enable_rc6(dev);
		gen9_enable_rps(dev);
		if (IS_SKYLAKE(dev) || IS_KABYLAKE(dev))
			__gen6_update_ring_freq(dev);
	} else if (IS_BROADWELL(dev)) {
		gen8_enable_rps(dev);
		__gen6_update_ring_freq(dev);
	} else {
		gen6_enable_rps(dev);
		__gen6_update_ring_freq(dev);
	}

	WARN_ON(dev_priv->rps.max_freq < dev_priv->rps.min_freq);
	WARN_ON(dev_priv->rps.idle_freq > dev_priv->rps.max_freq);

	WARN_ON(dev_priv->rps.efficient_freq < dev_priv->rps.min_freq);
	WARN_ON(dev_priv->rps.efficient_freq > dev_priv->rps.max_freq);

	dev_priv->rps.enabled = true;

	gen6_enable_rps_interrupts(dev);

	mutex_unlock(&dev_priv->rps.hw_lock);

	intel_runtime_pm_put(dev_priv);
}

void intel_enable_gt_powersave(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	/* Powersaving is controlled by the host when inside a VM */
	if (intel_vgpu_active(dev))
		return;

	if (IS_IRONLAKE_M(dev)) {
		mutex_lock(&dev->struct_mutex);
		ironlake_enable_drps(dev);
		intel_init_emon(dev);
		mutex_unlock(&dev->struct_mutex);
	} else if (INTEL_INFO(dev)->gen >= 6) {
		/*
		 * PCU communication is slow and this doesn't need to be
		 * done at any specific time, so do this out of our fast path
		 * to make resume and init faster.
		 *
		 * We depend on the HW RC6 power context save/restore
		 * mechanism when entering D3 through runtime PM suspend. So
		 * disable RPM until RPS/RC6 is properly setup. We can only
		 * get here via the driver load/system resume/runtime resume
		 * paths, so the _noresume version is enough (and in case of
		 * runtime resume it's necessary).
		 */
		if (schedule_delayed_work(&dev_priv->rps.delayed_resume_work,
					   round_jiffies_up_relative(HZ)))
			intel_runtime_pm_get_noresume(dev_priv);
	}
}

void intel_reset_gt_powersave(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	if (INTEL_INFO(dev)->gen < 6)
		return;

	gen6_suspend_rps(dev);
	dev_priv->rps.enabled = false;
}

static void ibx_init_clock_gating(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	/*
	 * On Ibex Peak and Cougar Point, we need to disable clock
	 * gating for the panel power sequencer or it will fail to
	 * start up when no ports are active.
	 */
	I915_WRITE(SOUTH_DSPCLK_GATE_D, PCH_DPLSUNIT_CLOCK_GATE_DISABLE);
}

static void g4x_disable_trickle_feed(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	enum pipe pipe;

	for_each_pipe(dev_priv, pipe) {
		I915_WRITE(DSPCNTR(pipe),
			   I915_READ(DSPCNTR(pipe)) |
			   DISPPLANE_TRICKLE_FEED_DISABLE);

		I915_WRITE(DSPSURF(pipe), I915_READ(DSPSURF(pipe)));
		POSTING_READ(DSPSURF(pipe));
	}
}

static void ilk_init_lp_watermarks(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	I915_WRITE(WM3_LP_ILK, I915_READ(WM3_LP_ILK) & ~WM1_LP_SR_EN);
	I915_WRITE(WM2_LP_ILK, I915_READ(WM2_LP_ILK) & ~WM1_LP_SR_EN);
	I915_WRITE(WM1_LP_ILK, I915_READ(WM1_LP_ILK) & ~WM1_LP_SR_EN);

	/*
	 * Don't touch WM1S_LP_EN here.
	 * Doing so could cause underruns.
	 */
}

static void ironlake_init_clock_gating(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	uint32_t dspclk_gate = ILK_VRHUNIT_CLOCK_GATE_DISABLE;

	/*
	 * Required for FBC
	 * WaFbcDisableDpfcClockGating:ilk
	 */
	dspclk_gate |= ILK_DPFCRUNIT_CLOCK_GATE_DISABLE |
		   ILK_DPFCUNIT_CLOCK_GATE_DISABLE |
		   ILK_DPFDUNIT_CLOCK_GATE_ENABLE;

	I915_WRITE(PCH_3DCGDIS0,
		   MARIUNIT_CLOCK_GATE_DISABLE |
		   SVSMUNIT_CLOCK_GATE_DISABLE);
	I915_WRITE(PCH_3DCGDIS1,
		   VFMUNIT_CLOCK_GATE_DISABLE);

	/*
	 * According to the spec the following bits should be set in
	 * order to enable memory self-refresh
	 * The bit 22/21 of 0x42004
	 * The bit 5 of 0x42020
	 * The bit 15 of 0x45000
	 */
	I915_WRITE(ILK_DISPLAY_CHICKEN2,
		   (I915_READ(ILK_DISPLAY_CHICKEN2) |
		    ILK_DPARB_GATE | ILK_VSDPFD_FULL));
	dspclk_gate |= ILK_DPARBUNIT_CLOCK_GATE_ENABLE;

	/* WaFbcWakeMemOn:skl,bxt,kbl */
	I915_WRITE(DISP_ARB_CTL, I915_READ(DISP_ARB_CTL) |
		   DISP_FBC_WM_DIS |
		   DISP_FBC_MEMORY_WAKE);

	ilk_init_lp_watermarks(dev);

	/*
	 * Based on the document from hardware guys the following bits
	 * should be set unconditionally in order to enable FBC.
	 * The bit 22 of 0x42000
	 * The bit 22 of 0x42004
	 * The bit 7,8,9 of 0x42020.
	 */
	if (IS_IRONLAKE_M(dev)) {
		/* WaFbcAsynchFlipDisableFbcQueue:ilk */
		I915_WRITE(ILK_DISPLAY_CHICKEN1,
			   I915_READ(ILK_DISPLAY_CHICKEN1) |
			   ILK_FBCQ_DIS);
		I915_WRITE(ILK_DISPLAY_CHICKEN2,
			   I915_READ(ILK_DISPLAY_CHICKEN2) |
			   ILK_DPARB_GATE);
	}

	I915_WRITE(ILK_DSPCLK_GATE_D, dspclk_gate);

	I915_WRITE(ILK_DISPLAY_CHICKEN2,
		   I915_READ(ILK_DISPLAY_CHICKEN2) |
		   ILK_ELPIN_409_SELECT);
	I915_WRITE(_3D_CHICKEN2,
		   _3D_CHICKEN2_WM_READ_PIPELINED << 16 |
		   _3D_CHICKEN2_WM_READ_PIPELINED);

	/* WaDisableRenderCachePipelinedFlush:ilk */
	I915_WRITE(CACHE_MODE_0,
		   _MASKED_BIT_ENABLE(CM0_PIPELINED_RENDER_FLUSH_DISABLE));

	/* WaDisable_RenderCache_OperationalFlush:ilk */
	I915_WRITE(CACHE_MODE_0, _MASKED_BIT_DISABLE(RC_OP_FLUSH_ENABLE));

	g4x_disable_trickle_feed(dev);

	ibx_init_clock_gating(dev);
}

static void cpt_init_clock_gating(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	int pipe;
	uint32_t val;

	/*
	 * On Ibex Peak and Cougar Point, we need to disable clock
	 * gating for the panel power sequencer or it will fail to
	 * start up when no ports are active.
	 */
	I915_WRITE(SOUTH_DSPCLK_GATE_D, PCH_DPLSUNIT_CLOCK_GATE_DISABLE |
		   PCH_DPLUNIT_CLOCK_GATE_DISABLE |
		   PCH_CPUNIT_CLOCK_GATE_DISABLE);
	I915_WRITE(SOUTH_CHICKEN2, I915_READ(SOUTH_CHICKEN2) |
		   DPLS_EDP_PPS_FIX_DIS);
	/* The below fixes the weird display corruption, a few pixels shifted
	 * downward, on (only) LVDS of some HP laptops with IVY.
	 */
	for_each_pipe(dev_priv, pipe) {
		val = I915_READ(TRANS_CHICKEN2(pipe));
		val |= TRANS_CHICKEN2_TIMING_OVERRIDE;
		val &= ~TRANS_CHICKEN2_FDI_POLARITY_REVERSED;
		if (dev_priv->vbt.fdi_rx_polarity_inverted)
			val |= TRANS_CHICKEN2_FDI_POLARITY_REVERSED;
		val &= ~TRANS_CHICKEN2_FRAME_START_DELAY_MASK;
		val &= ~TRANS_CHICKEN2_DISABLE_DEEP_COLOR_COUNTER;
		val &= ~TRANS_CHICKEN2_DISABLE_DEEP_COLOR_MODESWITCH;
		I915_WRITE(TRANS_CHICKEN2(pipe), val);
	}
	/* WADP0ClockGatingDisable */
	for_each_pipe(dev_priv, pipe) {
		I915_WRITE(TRANS_CHICKEN1(pipe),
			   TRANS_CHICKEN1_DP0UNIT_GC_DISABLE);
	}
}

static void gen6_check_mch_setup(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	uint32_t tmp;

	tmp = I915_READ(MCH_SSKPD);
	if ((tmp & MCH_SSKPD_WM0_MASK) != MCH_SSKPD_WM0_VAL)
		DRM_DEBUG_KMS("Wrong MCH_SSKPD value: 0x%08x This can cause underruns.\n",
			      tmp);
}

static void gen6_init_clock_gating(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	uint32_t dspclk_gate = ILK_VRHUNIT_CLOCK_GATE_DISABLE;

	I915_WRITE(ILK_DSPCLK_GATE_D, dspclk_gate);

	I915_WRITE(ILK_DISPLAY_CHICKEN2,
		   I915_READ(ILK_DISPLAY_CHICKEN2) |
		   ILK_ELPIN_409_SELECT);

	/* WaDisableHiZPlanesWhenMSAAEnabled:snb */
	I915_WRITE(_3D_CHICKEN,
		   _MASKED_BIT_ENABLE(_3D_CHICKEN_HIZ_PLANE_DISABLE_MSAA_4X_SNB));

	/* WaDisable_RenderCache_OperationalFlush:snb */
	I915_WRITE(CACHE_MODE_0, _MASKED_BIT_DISABLE(RC_OP_FLUSH_ENABLE));

	/*
	 * BSpec recoomends 8x4 when MSAA is used,
	 * however in practice 16x4 seems fastest.
	 *
	 * Note that PS/WM thread counts depend on the WIZ hashing
	 * disable bit, which we don't touch here, but it's good
	 * to keep in mind (see 3DSTATE_PS and 3DSTATE_WM).
	 */
	I915_WRITE(GEN6_GT_MODE,
		   _MASKED_FIELD(GEN6_WIZ_HASHING_MASK, GEN6_WIZ_HASHING_16x4));

	ilk_init_lp_watermarks(dev);

	I915_WRITE(CACHE_MODE_0,
		   _MASKED_BIT_DISABLE(CM0_STC_EVICT_DISABLE_LRA_SNB));

	I915_WRITE(GEN6_UCGCTL1,
		   I915_READ(GEN6_UCGCTL1) |
		   GEN6_BLBUNIT_CLOCK_GATE_DISABLE |
		   GEN6_CSUNIT_CLOCK_GATE_DISABLE);

	/* According to the BSpec vol1g, bit 12 (RCPBUNIT) clock
	 * gating disable must be set.  Failure to set it results in
	 * flickering pixels due to Z write ordering failures after
	 * some amount of runtime in the Mesa "fire" demo, and Unigine
	 * Sanctuary and Tropics, and apparently anything else with
	 * alpha test or pixel discard.
	 *
	 * According to the spec, bit 11 (RCCUNIT) must also be set,
	 * but we didn't debug actual testcases to find it out.
	 *
	 * WaDisableRCCUnitClockGating:snb
	 * WaDisableRCPBUnitClockGating:snb
	 */
	I915_WRITE(GEN6_UCGCTL2,
		   GEN6_RCPBUNIT_CLOCK_GATE_DISABLE |
		   GEN6_RCCUNIT_CLOCK_GATE_DISABLE);

	/* WaStripsFansDisableFastClipPerformanceFix:snb */
	I915_WRITE(_3D_CHICKEN3,
		   _MASKED_BIT_ENABLE(_3D_CHICKEN3_SF_DISABLE_FASTCLIP_CULL));

	/*
	 * Bspec says:
	 * "This bit must be set if 3DSTATE_CLIP clip mode is set to normal and
	 * 3DSTATE_SF number of SF output attributes is more than 16."
	 */
	I915_WRITE(_3D_CHICKEN3,
		   _MASKED_BIT_ENABLE(_3D_CHICKEN3_SF_DISABLE_PIPELINED_ATTR_FETCH));

	/*
	 * According to the spec the following bits should be
	 * set in order to enable memory self-refresh and fbc:
	 * The bit21 and bit22 of 0x42000
	 * The bit21 and bit22 of 0x42004
	 * The bit5 and bit7 of 0x42020
	 * The bit14 of 0x70180
	 * The bit14 of 0x71180
	 *
	 * WaFbcAsynchFlipDisableFbcQueue:snb
	 */
	I915_WRITE(ILK_DISPLAY_CHICKEN1,
		   I915_READ(ILK_DISPLAY_CHICKEN1) |
		   ILK_FBCQ_DIS | ILK_PABSTRETCH_DIS);
	I915_WRITE(ILK_DISPLAY_CHICKEN2,
		   I915_READ(ILK_DISPLAY_CHICKEN2) |
		   ILK_DPARB_GATE | ILK_VSDPFD_FULL);
	I915_WRITE(ILK_DSPCLK_GATE_D,
		   I915_READ(ILK_DSPCLK_GATE_D) |
		   ILK_DPARBUNIT_CLOCK_GATE_ENABLE  |
		   ILK_DPFDUNIT_CLOCK_GATE_ENABLE);

	g4x_disable_trickle_feed(dev);

	cpt_init_clock_gating(dev);

	gen6_check_mch_setup(dev);
}

static void gen7_setup_fixed_func_scheduler(struct drm_i915_private *dev_priv)
{
	uint32_t reg = I915_READ(GEN7_FF_THREAD_MODE);

	/*
	 * WaVSThreadDispatchOverride:ivb,vlv
	 *
	 * This actually overrides the dispatch
	 * mode for all thread types.
	 */
	reg &= ~GEN7_FF_SCHED_MASK;
	reg |= GEN7_FF_TS_SCHED_HW;
	reg |= GEN7_FF_VS_SCHED_HW;
	reg |= GEN7_FF_DS_SCHED_HW;

	I915_WRITE(GEN7_FF_THREAD_MODE, reg);
}

static void lpt_init_clock_gating(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	/*
	 * TODO: this bit should only be enabled when really needed, then
	 * disabled when not needed anymore in order to save power.
	 */
	if (HAS_PCH_LPT_LP(dev))
		I915_WRITE(SOUTH_DSPCLK_GATE_D,
			   I915_READ(SOUTH_DSPCLK_GATE_D) |
			   PCH_LP_PARTITION_LEVEL_DISABLE);

	/* WADPOClockGatingDisable:hsw */
	I915_WRITE(TRANS_CHICKEN1(PIPE_A),
		   I915_READ(TRANS_CHICKEN1(PIPE_A)) |
		   TRANS_CHICKEN1_DP0UNIT_GC_DISABLE);
}

static void lpt_suspend_hw(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	if (HAS_PCH_LPT_LP(dev)) {
		uint32_t val = I915_READ(SOUTH_DSPCLK_GATE_D);

		val &= ~PCH_LP_PARTITION_LEVEL_DISABLE;
		I915_WRITE(SOUTH_DSPCLK_GATE_D, val);
	}
}

static void broadwell_init_clock_gating(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	enum pipe pipe;
	uint32_t misccpctl;

	ilk_init_lp_watermarks(dev);

	/* WaSwitchSolVfFArbitrationPriority:bdw */
	I915_WRITE(GAM_ECOCHK, I915_READ(GAM_ECOCHK) | HSW_ECOCHK_ARB_PRIO_SOL);

	/* WaPsrDPAMaskVBlankInSRD:bdw */
	I915_WRITE(CHICKEN_PAR1_1,
		   I915_READ(CHICKEN_PAR1_1) | DPA_MASK_VBLANK_SRD);

	/* WaPsrDPRSUnmaskVBlankInSRD:bdw */
	for_each_pipe(dev_priv, pipe) {
		I915_WRITE(CHICKEN_PIPESL_1(pipe),
			   I915_READ(CHICKEN_PIPESL_1(pipe)) |
			   BDW_DPRS_MASK_VBLANK_SRD);
	}

	/* WaVSRefCountFullforceMissDisable:bdw */
	/* WaDSRefCountFullforceMissDisable:bdw */
	I915_WRITE(GEN7_FF_THREAD_MODE,
		   I915_READ(GEN7_FF_THREAD_MODE) &
		   ~(GEN8_FF_DS_REF_CNT_FFME | GEN7_FF_VS_REF_CNT_FFME));

	I915_WRITE(GEN6_RC_SLEEP_PSMI_CONTROL,
		   _MASKED_BIT_ENABLE(GEN8_RC_SEMA_IDLE_MSG_DISABLE));

	/* WaDisableSDEUnitClockGating:bdw */
	I915_WRITE(GEN8_UCGCTL6, I915_READ(GEN8_UCGCTL6) |
		   GEN8_SDEUNIT_CLOCK_GATE_DISABLE);

	/*
	 * WaProgramL3SqcReg1Default:bdw
	 * WaTempDisableDOPClkGating:bdw
	 */
	misccpctl = I915_READ(GEN7_MISCCPCTL);
	I915_WRITE(GEN7_MISCCPCTL, misccpctl & ~GEN7_DOP_CLOCK_GATE_ENABLE);
	I915_WRITE(GEN8_L3SQCREG1, BDW_WA_L3SQCREG1_DEFAULT);
	/*
	 * Wait at least 100 clocks before re-enabling clock gating. See
	 * the definition of L3SQCREG1 in BSpec.
	 */
	POSTING_READ(GEN8_L3SQCREG1);
	udelay(1);
	I915_WRITE(GEN7_MISCCPCTL, misccpctl);

	/*
	 * WaGttCachingOffByDefault:bdw
	 * GTT cache may not work with big pages, so if those
	 * are ever enabled GTT cache may need to be disabled.
	 */
	I915_WRITE(HSW_GTT_CACHE_EN, GTT_CACHE_EN_ALL);

	lpt_init_clock_gating(dev);
}

static void haswell_init_clock_gating(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	ilk_init_lp_watermarks(dev);

	/* L3 caching of data atomics doesn't work -- disable it. */
	I915_WRITE(HSW_SCRATCH1, HSW_SCRATCH1_L3_DATA_ATOMICS_DISABLE);
	I915_WRITE(HSW_ROW_CHICKEN3,
		   _MASKED_BIT_ENABLE(HSW_ROW_CHICKEN3_L3_GLOBAL_ATOMICS_DISABLE));

	/* This is required by WaCatErrorRejectionIssue:hsw */
	I915_WRITE(GEN7_SQ_CHICKEN_MBCUNIT_CONFIG,
			I915_READ(GEN7_SQ_CHICKEN_MBCUNIT_CONFIG) |
			GEN7_SQ_CHICKEN_MBCUNIT_SQINTMOB);

	/* WaVSRefCountFullforceMissDisable:hsw */
	I915_WRITE(GEN7_FF_THREAD_MODE,
		   I915_READ(GEN7_FF_THREAD_MODE) & ~GEN7_FF_VS_REF_CNT_FFME);

	/* WaDisable_RenderCache_OperationalFlush:hsw */
	I915_WRITE(CACHE_MODE_0_GEN7, _MASKED_BIT_DISABLE(RC_OP_FLUSH_ENABLE));

	/* enable HiZ Raw Stall Optimization */
	I915_WRITE(CACHE_MODE_0_GEN7,
		   _MASKED_BIT_DISABLE(HIZ_RAW_STALL_OPT_DISABLE));

	/* WaDisable4x2SubspanOptimization:hsw */
	I915_WRITE(CACHE_MODE_1,
		   _MASKED_BIT_ENABLE(PIXEL_SUBSPAN_COLLECT_OPT_DISABLE));

	/*
	 * BSpec recommends 8x4 when MSAA is used,
	 * however in practice 16x4 seems fastest.
	 *
	 * Note that PS/WM thread counts depend on the WIZ hashing
	 * disable bit, which we don't touch here, but it's good
	 * to keep in mind (see 3DSTATE_PS and 3DSTATE_WM).
	 */
	I915_WRITE(GEN7_GT_MODE,
		   _MASKED_FIELD(GEN6_WIZ_HASHING_MASK, GEN6_WIZ_HASHING_16x4));

	/* WaSampleCChickenBitEnable:hsw */
	I915_WRITE(HALF_SLICE_CHICKEN3,
		   _MASKED_BIT_ENABLE(HSW_SAMPLE_C_PERFORMANCE));

	/* WaSwitchSolVfFArbitrationPriority:hsw */
	I915_WRITE(GAM_ECOCHK, I915_READ(GAM_ECOCHK) | HSW_ECOCHK_ARB_PRIO_SOL);

	/* WaRsPkgCStateDisplayPMReq:hsw */
	I915_WRITE(CHICKEN_PAR1_1,
		   I915_READ(CHICKEN_PAR1_1) | FORCE_ARB_IDLE_PLANES);

	lpt_init_clock_gating(dev);
}

static void ivybridge_init_clock_gating(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	uint32_t snpcr;

	ilk_init_lp_watermarks(dev);

	I915_WRITE(ILK_DSPCLK_GATE_D, ILK_VRHUNIT_CLOCK_GATE_DISABLE);

	/* WaDisableEarlyCull:ivb */
	I915_WRITE(_3D_CHICKEN3,
		   _MASKED_BIT_ENABLE(_3D_CHICKEN_SF_DISABLE_OBJEND_CULL));

	/* WaDisableBackToBackFlipFix:ivb */
	I915_WRITE(IVB_CHICKEN3,
		   CHICKEN3_DGMG_REQ_OUT_FIX_DISABLE |
		   CHICKEN3_DGMG_DONE_FIX_DISABLE);

	/* WaDisablePSDDualDispatchEnable:ivb */
	if (IS_IVB_GT1(dev))
		I915_WRITE(GEN7_HALF_SLICE_CHICKEN1,
			   _MASKED_BIT_ENABLE(GEN7_PSD_SINGLE_PORT_DISPATCH_ENABLE));

	/* WaDisable_RenderCache_OperationalFlush:ivb */
	I915_WRITE(CACHE_MODE_0_GEN7, _MASKED_BIT_DISABLE(RC_OP_FLUSH_ENABLE));

	/* Apply the WaDisableRHWOOptimizationForRenderHang:ivb workaround. */
	I915_WRITE(GEN7_COMMON_SLICE_CHICKEN1,
		   GEN7_CSC1_RHWO_OPT_DISABLE_IN_RCC);

	/* WaApplyL3ControlAndL3ChickenMode:ivb */
	I915_WRITE(GEN7_L3CNTLREG1,
			GEN7_WA_FOR_GEN7_L3_CONTROL);
	I915_WRITE(GEN7_L3_CHICKEN_MODE_REGISTER,
		   GEN7_WA_L3_CHICKEN_MODE);
	if (IS_IVB_GT1(dev))
		I915_WRITE(GEN7_ROW_CHICKEN2,
			   _MASKED_BIT_ENABLE(DOP_CLOCK_GATING_DISABLE));
	else {
		/* must write both registers */
		I915_WRITE(GEN7_ROW_CHICKEN2,
			   _MASKED_BIT_ENABLE(DOP_CLOCK_GATING_DISABLE));
		I915_WRITE(GEN7_ROW_CHICKEN2_GT2,
			   _MASKED_BIT_ENABLE(DOP_CLOCK_GATING_DISABLE));
	}

	/* WaForceL3Serialization:ivb */
	I915_WRITE(GEN7_L3SQCREG4, I915_READ(GEN7_L3SQCREG4) &
		   ~L3SQ_URB_READ_CAM_MATCH_DISABLE);

	/*
	 * According to the spec, bit 13 (RCZUNIT) must be set on IVB.
	 * This implements the WaDisableRCZUnitClockGating:ivb workaround.
	 */
	I915_WRITE(GEN6_UCGCTL2,
		   GEN6_RCZUNIT_CLOCK_GATE_DISABLE);

	/* This is required by WaCatErrorRejectionIssue:ivb */
	I915_WRITE(GEN7_SQ_CHICKEN_MBCUNIT_CONFIG,
			I915_READ(GEN7_SQ_CHICKEN_MBCUNIT_CONFIG) |
			GEN7_SQ_CHICKEN_MBCUNIT_SQINTMOB);

	g4x_disable_trickle_feed(dev);

	gen7_setup_fixed_func_scheduler(dev_priv);

	if (0) { /* causes HiZ corruption on ivb:gt1 */
		/* enable HiZ Raw Stall Optimization */
		I915_WRITE(CACHE_MODE_0_GEN7,
			   _MASKED_BIT_DISABLE(HIZ_RAW_STALL_OPT_DISABLE));
	}

	/* WaDisable4x2SubspanOptimization:ivb */
	I915_WRITE(CACHE_MODE_1,
		   _MASKED_BIT_ENABLE(PIXEL_SUBSPAN_COLLECT_OPT_DISABLE));

	/*
	 * BSpec recommends 8x4 when MSAA is used,
	 * however in practice 16x4 seems fastest.
	 *
	 * Note that PS/WM thread counts depend on the WIZ hashing
	 * disable bit, which we don't touch here, but it's good
	 * to keep in mind (see 3DSTATE_PS and 3DSTATE_WM).
	 */
	I915_WRITE(GEN7_GT_MODE,
		   _MASKED_FIELD(GEN6_WIZ_HASHING_MASK, GEN6_WIZ_HASHING_16x4));

	snpcr = I915_READ(GEN6_MBCUNIT_SNPCR);
	snpcr &= ~GEN6_MBC_SNPCR_MASK;
	snpcr |= GEN6_MBC_SNPCR_MED;
	I915_WRITE(GEN6_MBCUNIT_SNPCR, snpcr);

	if (!HAS_PCH_NOP(dev))
		cpt_init_clock_gating(dev);

	gen6_check_mch_setup(dev);
}

static void vlv_init_display_clock_gating(struct drm_i915_private *dev_priv)
{
        u32 val;

        /*
        * On driver load, a pipe may be active and driving a DSI display.
        * Preserve DPOUNIT_CLOCK_GATE_DISABLE to avoid the pipe getting stuck
        * (and never recovering) in this case. intel_dsi_post_disable() will
        * clear it when we turn off the display.
        */
        val = I915_READ(DSPCLK_GATE_D);
        val &= DPOUNIT_CLOCK_GATE_DISABLE;
        val |= VRHUNIT_CLOCK_GATE_DISABLE;
        I915_WRITE(DSPCLK_GATE_D, val);

	/*
	 * Disable trickle feed and enable pnd deadline calculation
	 */
	I915_WRITE(MI_ARB_VLV, MI_ARB_DISPLAY_TRICKLE_FEED_DISABLE);
	I915_WRITE(CBR1_VLV, 0);
}

static void valleyview_init_clock_gating(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	vlv_init_display_clock_gating(dev_priv);

	/* WaDisableEarlyCull:vlv */
	I915_WRITE(_3D_CHICKEN3,
		   _MASKED_BIT_ENABLE(_3D_CHICKEN_SF_DISABLE_OBJEND_CULL));

	/* WaDisableBackToBackFlipFix:vlv */
	I915_WRITE(IVB_CHICKEN3,
		   CHICKEN3_DGMG_REQ_OUT_FIX_DISABLE |
		   CHICKEN3_DGMG_DONE_FIX_DISABLE);

	/* WaPsdDispatchEnable:vlv */
	/* WaDisablePSDDualDispatchEnable:vlv */
	I915_WRITE(GEN7_HALF_SLICE_CHICKEN1,
		   _MASKED_BIT_ENABLE(GEN7_MAX_PS_THREAD_DEP |
				      GEN7_PSD_SINGLE_PORT_DISPATCH_ENABLE));

	/* WaDisable_RenderCache_OperationalFlush:vlv */
	I915_WRITE(CACHE_MODE_0_GEN7, _MASKED_BIT_DISABLE(RC_OP_FLUSH_ENABLE));

	/* WaForceL3Serialization:vlv */
	I915_WRITE(GEN7_L3SQCREG4, I915_READ(GEN7_L3SQCREG4) &
		   ~L3SQ_URB_READ_CAM_MATCH_DISABLE);

	/* WaDisableDopClockGating:vlv */
	I915_WRITE(GEN7_ROW_CHICKEN2,
		   _MASKED_BIT_ENABLE(DOP_CLOCK_GATING_DISABLE));

	/* This is required by WaCatErrorRejectionIssue:vlv */
	I915_WRITE(GEN7_SQ_CHICKEN_MBCUNIT_CONFIG,
		   I915_READ(GEN7_SQ_CHICKEN_MBCUNIT_CONFIG) |
		   GEN7_SQ_CHICKEN_MBCUNIT_SQINTMOB);

	gen7_setup_fixed_func_scheduler(dev_priv);

	/*
	 * According to the spec, bit 13 (RCZUNIT) must be set on IVB.
	 * This implements the WaDisableRCZUnitClockGating:vlv workaround.
	 */
	I915_WRITE(GEN6_UCGCTL2,
		   GEN6_RCZUNIT_CLOCK_GATE_DISABLE);

	/* WaDisableL3Bank2xClockGate:vlv
	 * Disabling L3 clock gating- MMIO 940c[25] = 1
	 * Set bit 25, to disable L3_BANK_2x_CLK_GATING */
	I915_WRITE(GEN7_UCGCTL4,
		   I915_READ(GEN7_UCGCTL4) | GEN7_L3BANK2X_CLOCK_GATE_DISABLE);

	/*
	 * BSpec says this must be set, even though
	 * WaDisable4x2SubspanOptimization isn't listed for VLV.
	 */
	I915_WRITE(CACHE_MODE_1,
		   _MASKED_BIT_ENABLE(PIXEL_SUBSPAN_COLLECT_OPT_DISABLE));

	/*
	 * BSpec recommends 8x4 when MSAA is used,
	 * however in practice 16x4 seems fastest.
	 *
	 * Note that PS/WM thread counts depend on the WIZ hashing
	 * disable bit, which we don't touch here, but it's good
	 * to keep in mind (see 3DSTATE_PS and 3DSTATE_WM).
	 */
	I915_WRITE(GEN7_GT_MODE,
		   _MASKED_FIELD(GEN6_WIZ_HASHING_MASK, GEN6_WIZ_HASHING_16x4));

	/*
	 * WaIncreaseL3CreditsForVLVB0:vlv
	 * This is the hardware default actually.
	 */
	I915_WRITE(GEN7_L3SQCREG1, VLV_B0_WA_L3SQCREG1_VALUE);

	/*
	 * WaDisableVLVClockGating_VBIIssue:vlv
	 * Disable clock gating on th GCFG unit to prevent a delay
	 * in the reporting of vblank events.
	 */
	I915_WRITE(VLV_GUNIT_CLOCK_GATE, GCFG_DIS);
}

static void cherryview_init_clock_gating(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	vlv_init_display_clock_gating(dev_priv);

	/* WaVSRefCountFullforceMissDisable:chv */
	/* WaDSRefCountFullforceMissDisable:chv */
	I915_WRITE(GEN7_FF_THREAD_MODE,
		   I915_READ(GEN7_FF_THREAD_MODE) &
		   ~(GEN8_FF_DS_REF_CNT_FFME | GEN7_FF_VS_REF_CNT_FFME));

	/* WaDisableSemaphoreAndSyncFlipWait:chv */
	I915_WRITE(GEN6_RC_SLEEP_PSMI_CONTROL,
		   _MASKED_BIT_ENABLE(GEN8_RC_SEMA_IDLE_MSG_DISABLE));

	/* WaDisableCSUnitClockGating:chv */
	I915_WRITE(GEN6_UCGCTL1, I915_READ(GEN6_UCGCTL1) |
		   GEN6_CSUNIT_CLOCK_GATE_DISABLE);

	/* WaDisableSDEUnitClockGating:chv */
	I915_WRITE(GEN8_UCGCTL6, I915_READ(GEN8_UCGCTL6) |
		   GEN8_SDEUNIT_CLOCK_GATE_DISABLE);

	/*
	 * GTT cache may not work with big pages, so if those
	 * are ever enabled GTT cache may need to be disabled.
	 */
	I915_WRITE(HSW_GTT_CACHE_EN, GTT_CACHE_EN_ALL);
}

static void g4x_init_clock_gating(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	uint32_t dspclk_gate;

	I915_WRITE(RENCLK_GATE_D1, 0);
	I915_WRITE(RENCLK_GATE_D2, VF_UNIT_CLOCK_GATE_DISABLE |
		   GS_UNIT_CLOCK_GATE_DISABLE |
		   CL_UNIT_CLOCK_GATE_DISABLE);
	I915_WRITE(RAMCLK_GATE_D, 0);
	dspclk_gate = VRHUNIT_CLOCK_GATE_DISABLE |
		OVRUNIT_CLOCK_GATE_DISABLE |
		OVCUNIT_CLOCK_GATE_DISABLE;
	if (IS_GM45(dev))
		dspclk_gate |= DSSUNIT_CLOCK_GATE_DISABLE;
	I915_WRITE(DSPCLK_GATE_D, dspclk_gate);

	/* WaDisableRenderCachePipelinedFlush */
	I915_WRITE(CACHE_MODE_0,
		   _MASKED_BIT_ENABLE(CM0_PIPELINED_RENDER_FLUSH_DISABLE));

	/* WaDisable_RenderCache_OperationalFlush:g4x */
	I915_WRITE(CACHE_MODE_0, _MASKED_BIT_DISABLE(RC_OP_FLUSH_ENABLE));

	g4x_disable_trickle_feed(dev);
}

static void crestline_init_clock_gating(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	I915_WRITE(RENCLK_GATE_D1, I965_RCC_CLOCK_GATE_DISABLE);
	I915_WRITE(RENCLK_GATE_D2, 0);
	I915_WRITE(DSPCLK_GATE_D, 0);
	I915_WRITE(RAMCLK_GATE_D, 0);
	I915_WRITE16(DEUC, 0);
	I915_WRITE(MI_ARB_STATE,
		   _MASKED_BIT_ENABLE(MI_ARB_DISPLAY_TRICKLE_FEED_DISABLE));

	/* WaDisable_RenderCache_OperationalFlush:gen4 */
	I915_WRITE(CACHE_MODE_0, _MASKED_BIT_DISABLE(RC_OP_FLUSH_ENABLE));
}

static void broadwater_init_clock_gating(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	I915_WRITE(RENCLK_GATE_D1, I965_RCZ_CLOCK_GATE_DISABLE |
		   I965_RCC_CLOCK_GATE_DISABLE |
		   I965_RCPB_CLOCK_GATE_DISABLE |
		   I965_ISC_CLOCK_GATE_DISABLE |
		   I965_FBC_CLOCK_GATE_DISABLE);
	I915_WRITE(RENCLK_GATE_D2, 0);
	I915_WRITE(MI_ARB_STATE,
		   _MASKED_BIT_ENABLE(MI_ARB_DISPLAY_TRICKLE_FEED_DISABLE));

	/* WaDisable_RenderCache_OperationalFlush:gen4 */
	I915_WRITE(CACHE_MODE_0, _MASKED_BIT_DISABLE(RC_OP_FLUSH_ENABLE));
}

static void gen3_init_clock_gating(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	u32 dstate = I915_READ(D_STATE);

	dstate |= DSTATE_PLL_D3_OFF | DSTATE_GFX_CLOCK_GATING |
		DSTATE_DOT_CLOCK_GATING;
	I915_WRITE(D_STATE, dstate);

	if (IS_PINEVIEW(dev))
		I915_WRITE(ECOSKPD, _MASKED_BIT_ENABLE(ECO_GATING_CX_ONLY));

	/* IIR "flip pending" means done if this bit is set */
	I915_WRITE(ECOSKPD, _MASKED_BIT_DISABLE(ECO_FLIP_DONE));

	/* interrupts should cause a wake up from C3 */
	I915_WRITE(INSTPM, _MASKED_BIT_ENABLE(INSTPM_AGPBUSY_INT_EN));

	/* On GEN3 we really need to make sure the ARB C3 LP bit is set */
	I915_WRITE(MI_ARB_STATE, _MASKED_BIT_ENABLE(MI_ARB_C3_LP_WRITE_ENABLE));

	I915_WRITE(MI_ARB_STATE,
		   _MASKED_BIT_ENABLE(MI_ARB_DISPLAY_TRICKLE_FEED_DISABLE));
}

static void i85x_init_clock_gating(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	I915_WRITE(RENCLK_GATE_D1, SV_CLOCK_GATE_DISABLE);

	/* interrupts should cause a wake up from C3 */
	I915_WRITE(MI_STATE, _MASKED_BIT_ENABLE(MI_AGPBUSY_INT_EN) |
		   _MASKED_BIT_DISABLE(MI_AGPBUSY_830_MODE));

	I915_WRITE(MEM_MODE,
		   _MASKED_BIT_ENABLE(MEM_DISPLAY_TRICKLE_FEED_DISABLE));
}

static void i830_init_clock_gating(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	I915_WRITE(DSPCLK_GATE_D, OVRUNIT_CLOCK_GATE_DISABLE);

	I915_WRITE(MEM_MODE,
		   _MASKED_BIT_ENABLE(MEM_DISPLAY_A_TRICKLE_FEED_DISABLE) |
		   _MASKED_BIT_ENABLE(MEM_DISPLAY_B_TRICKLE_FEED_DISABLE));
}

void intel_init_clock_gating(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	if (dev_priv->display.init_clock_gating)
		dev_priv->display.init_clock_gating(dev);
}

void intel_suspend_hw(struct drm_device *dev)
{
	if (HAS_PCH_LPT(dev))
		lpt_suspend_hw(dev);
}

/* Set up chip specific power management-related functions */
void intel_init_pm(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	intel_fbc_init(dev_priv);

	/* For cxsr */
	if (IS_PINEVIEW(dev))
		i915_pineview_get_mem_freq(dev);
	else if (IS_GEN5(dev))
		i915_ironlake_get_mem_freq(dev);

	/* For FIFO watermark updates */
	if (INTEL_INFO(dev)->gen >= 9) {
		skl_setup_wm_latency(dev);

		if (IS_BROXTON(dev))
			dev_priv->display.init_clock_gating =
				bxt_init_clock_gating;
		dev_priv->display.update_wm = skl_update_wm;
		dev_priv->display.update_sprite_wm = skl_update_sprite_wm;
	} else if (HAS_PCH_SPLIT(dev)) {
		ilk_setup_wm_latency(dev);

		if ((IS_GEN5(dev) && dev_priv->wm.pri_latency[1] &&
		     dev_priv->wm.spr_latency[1] && dev_priv->wm.cur_latency[1]) ||
		    (!IS_GEN5(dev) && dev_priv->wm.pri_latency[0] &&
		     dev_priv->wm.spr_latency[0] && dev_priv->wm.cur_latency[0])) {
			dev_priv->display.update_wm = ilk_update_wm;
			dev_priv->display.update_sprite_wm = ilk_update_sprite_wm;
		} else {
			DRM_DEBUG_KMS("Failed to read display plane latency. "
				      "Disable CxSR\n");
		}

		if (IS_GEN5(dev))
			dev_priv->display.init_clock_gating = ironlake_init_clock_gating;
		else if (IS_GEN6(dev))
			dev_priv->display.init_clock_gating = gen6_init_clock_gating;
		else if (IS_IVYBRIDGE(dev))
			dev_priv->display.init_clock_gating = ivybridge_init_clock_gating;
		else if (IS_HASWELL(dev))
			dev_priv->display.init_clock_gating = haswell_init_clock_gating;
		else if (INTEL_INFO(dev)->gen == 8)
			dev_priv->display.init_clock_gating = broadwell_init_clock_gating;
	} else if (IS_CHERRYVIEW(dev)) {
		vlv_setup_wm_latency(dev);

		dev_priv->display.update_wm = vlv_update_wm;
		dev_priv->display.init_clock_gating =
			cherryview_init_clock_gating;
	} else if (IS_VALLEYVIEW(dev)) {
		vlv_setup_wm_latency(dev);

		dev_priv->display.update_wm = vlv_update_wm;
		dev_priv->display.init_clock_gating =
			valleyview_init_clock_gating;
	} else if (IS_PINEVIEW(dev)) {
		if (!intel_get_cxsr_latency(IS_PINEVIEW_G(dev),
					    dev_priv->is_ddr3,
					    dev_priv->fsb_freq,
					    dev_priv->mem_freq)) {
			DRM_INFO("failed to find known CxSR latency "
				 "(found ddr%s fsb freq %d, mem freq %d), "
				 "disabling CxSR\n",
				 (dev_priv->is_ddr3 == 1) ? "3" : "2",
				 dev_priv->fsb_freq, dev_priv->mem_freq);
			/* Disable CxSR and never update its watermark again */
			intel_set_memory_cxsr(dev_priv, false);
			dev_priv->display.update_wm = NULL;
		} else
			dev_priv->display.update_wm = pineview_update_wm;
		dev_priv->display.init_clock_gating = gen3_init_clock_gating;
	} else if (IS_G4X(dev)) {
		dev_priv->display.update_wm = g4x_update_wm;
		dev_priv->display.init_clock_gating = g4x_init_clock_gating;
	} else if (IS_GEN4(dev)) {
		dev_priv->display.update_wm = i965_update_wm;
		if (IS_CRESTLINE(dev))
			dev_priv->display.init_clock_gating = crestline_init_clock_gating;
		else if (IS_BROADWATER(dev))
			dev_priv->display.init_clock_gating = broadwater_init_clock_gating;
	} else if (IS_GEN3(dev)) {
		dev_priv->display.update_wm = i9xx_update_wm;
		dev_priv->display.get_fifo_size = i9xx_get_fifo_size;
		dev_priv->display.init_clock_gating = gen3_init_clock_gating;
	} else if (IS_GEN2(dev)) {
		if (INTEL_INFO(dev)->num_pipes == 1) {
			dev_priv->display.update_wm = i845_update_wm;
			dev_priv->display.get_fifo_size = i845_get_fifo_size;
		} else {
			dev_priv->display.update_wm = i9xx_update_wm;
			dev_priv->display.get_fifo_size = i830_get_fifo_size;
		}

		if (IS_I85X(dev) || IS_I865G(dev))
			dev_priv->display.init_clock_gating = i85x_init_clock_gating;
		else
			dev_priv->display.init_clock_gating = i830_init_clock_gating;
	} else {
		DRM_ERROR("unexpected fall-through in intel_init_pm\n");
	}
}

int sandybridge_pcode_read(struct drm_i915_private *dev_priv, u32 mbox, u32 *val)
{
	WARN_ON(!mutex_is_locked(&dev_priv->rps.hw_lock));

	if (I915_READ(GEN6_PCODE_MAILBOX) & GEN6_PCODE_READY) {
		DRM_DEBUG_DRIVER("warning: pcode (read) mailbox access failed\n");
		return -EAGAIN;
	}

	I915_WRITE(GEN6_PCODE_DATA, *val);
	I915_WRITE(GEN6_PCODE_DATA1, 0);
	I915_WRITE(GEN6_PCODE_MAILBOX, GEN6_PCODE_READY | mbox);

	if (wait_for((I915_READ(GEN6_PCODE_MAILBOX) & GEN6_PCODE_READY) == 0,
		     500)) {
		DRM_ERROR("timeout waiting for pcode read (%d) to finish\n", mbox);
		return -ETIMEDOUT;
	}

	*val = I915_READ(GEN6_PCODE_DATA);
	I915_WRITE(GEN6_PCODE_DATA, 0);

	return 0;
}

int sandybridge_pcode_write(struct drm_i915_private *dev_priv, u32 mbox, u32 val)
{
	WARN_ON(!mutex_is_locked(&dev_priv->rps.hw_lock));

	if (I915_READ(GEN6_PCODE_MAILBOX) & GEN6_PCODE_READY) {
		DRM_DEBUG_DRIVER("warning: pcode (write) mailbox access failed\n");
		return -EAGAIN;
	}

	I915_WRITE(GEN6_PCODE_DATA, val);
	I915_WRITE(GEN6_PCODE_MAILBOX, GEN6_PCODE_READY | mbox);

	if (wait_for((I915_READ(GEN6_PCODE_MAILBOX) & GEN6_PCODE_READY) == 0,
		     500)) {
		DRM_ERROR("timeout waiting for pcode write (%d) to finish\n", mbox);
		return -ETIMEDOUT;
	}

	I915_WRITE(GEN6_PCODE_DATA, 0);

	return 0;
}

static int vlv_gpu_freq_div(unsigned int czclk_freq)
{
	switch (czclk_freq) {
	case 200:
		return 10;
	case 267:
		return 12;
	case 320:
	case 333:
		return 16;
	case 400:
		return 20;
	default:
		return -1;
	}
}

static int byt_gpu_freq(struct drm_i915_private *dev_priv, int val)
{
	int div, czclk_freq = DIV_ROUND_CLOSEST(dev_priv->czclk_freq, 1000);

	div = vlv_gpu_freq_div(czclk_freq);
	if (div < 0)
		return div;

	return DIV_ROUND_CLOSEST(czclk_freq * (val + 6 - 0xbd), div);
}

static int byt_freq_opcode(struct drm_i915_private *dev_priv, int val)
{
	int mul, czclk_freq = DIV_ROUND_CLOSEST(dev_priv->czclk_freq, 1000);

	mul = vlv_gpu_freq_div(czclk_freq);
	if (mul < 0)
		return mul;

	return DIV_ROUND_CLOSEST(mul * val, czclk_freq) + 0xbd - 6;
}

static int chv_gpu_freq(struct drm_i915_private *dev_priv, int val)
{
	int div, czclk_freq = DIV_ROUND_CLOSEST(dev_priv->czclk_freq, 1000);

	div = vlv_gpu_freq_div(czclk_freq) / 2;
	if (div < 0)
		return div;

	return DIV_ROUND_CLOSEST(czclk_freq * val, 2 * div) / 2;
}

static int chv_freq_opcode(struct drm_i915_private *dev_priv, int val)
{
	int mul, czclk_freq = DIV_ROUND_CLOSEST(dev_priv->czclk_freq, 1000);

	mul = vlv_gpu_freq_div(czclk_freq) / 2;
	if (mul < 0)
		return mul;

	/* CHV needs even values */
	return DIV_ROUND_CLOSEST(val * 2 * mul, czclk_freq) * 2;
}

int intel_gpu_freq(struct drm_i915_private *dev_priv, int val)
{
	if (IS_GEN9(dev_priv->dev))
		return DIV_ROUND_CLOSEST(val * GT_FREQUENCY_MULTIPLIER,
					 GEN9_FREQ_SCALER);
	else if (IS_CHERRYVIEW(dev_priv->dev))
		return chv_gpu_freq(dev_priv, val);
	else if (IS_VALLEYVIEW(dev_priv->dev))
		return byt_gpu_freq(dev_priv, val);
	else
		return val * GT_FREQUENCY_MULTIPLIER;
}

int intel_freq_opcode(struct drm_i915_private *dev_priv, int val)
{
	if (IS_GEN9(dev_priv->dev))
		return DIV_ROUND_CLOSEST(val * GEN9_FREQ_SCALER,
					 GT_FREQUENCY_MULTIPLIER);
	else if (IS_CHERRYVIEW(dev_priv->dev))
		return chv_freq_opcode(dev_priv, val);
	else if (IS_VALLEYVIEW(dev_priv->dev))
		return byt_freq_opcode(dev_priv, val);
	else
		return DIV_ROUND_CLOSEST(val, GT_FREQUENCY_MULTIPLIER);
}

struct request_boost {
	struct work_struct work;
	struct drm_i915_gem_request *req;
};

static void __intel_rps_boost_work(struct work_struct *work)
{
	struct request_boost *boost = container_of(work, struct request_boost, work);
	struct drm_i915_gem_request *req = boost->req;

	if (!i915_gem_request_completed(req, true))
		gen6_rps_boost(to_i915(req->ring->dev), NULL,
			       req->emitted_jiffies);

	i915_gem_request_unreference__unlocked(req);
	kfree(boost);
}

void intel_queue_rps_boost_for_request(struct drm_device *dev,
				       struct drm_i915_gem_request *req)
{
	struct request_boost *boost;

	if (req == NULL || INTEL_INFO(dev)->gen < 6)
		return;

	if (i915_gem_request_completed(req, true))
		return;

	boost = kmalloc(sizeof(*boost), GFP_ATOMIC);
	if (boost == NULL)
		return;

	i915_gem_request_reference(req);
	boost->req = req;

	INIT_WORK(&boost->work, __intel_rps_boost_work);
	queue_work(to_i915(dev)->wq, &boost->work);
}

void intel_pm_setup(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	rw_init(&dev_priv->rps.hw_lock, "rpshw");
	mtx_init(&dev_priv->rps.client_lock, IPL_NONE);

	INIT_DELAYED_WORK(&dev_priv->rps.delayed_resume_work,
			  intel_gen6_powersave_work);
	INIT_LIST_HEAD(&dev_priv->rps.clients);
	INIT_LIST_HEAD(&dev_priv->rps.semaphores.link);
	INIT_LIST_HEAD(&dev_priv->rps.mmioflips.link);

	dev_priv->pm.suspended = false;
}
