/*
 * Copyright © 2012-2014 Intel Corporation
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
 *    Daniel Vetter <daniel.vetter@ffwll.ch>
 *
 */

#ifdef __linux__
#include <linux/pm_runtime.h>
#include <linux/vgaarb.h>
#endif

#include "i915_drv.h"
#include "intel_drv.h"

/**
 * DOC: runtime pm
 *
 * The i915 driver supports dynamic enabling and disabling of entire hardware
 * blocks at runtime. This is especially important on the display side where
 * software is supposed to control many power gates manually on recent hardware,
 * since on the GT side a lot of the power management is done by the hardware.
 * But even there some manual control at the device level is required.
 *
 * Since i915 supports a diverse set of platforms with a unified codebase and
 * hardware engineers just love to shuffle functionality around between power
 * domains there's a sizeable amount of indirection required. This file provides
 * generic functions to the driver for grabbing and releasing references for
 * abstract power domains. It then maps those to the actual power wells
 * present for a given platform.
 */

#define GEN9_ENABLE_DC5(dev) 0
#define SKL_ENABLE_DC6(dev) (IS_SKYLAKE(dev) || IS_KABYLAKE(dev))

#define for_each_power_well(i, power_well, domain_mask, power_domains)	\
	for (i = 0;							\
	     i < (power_domains)->power_well_count &&			\
		 ((power_well) = &(power_domains)->power_wells[i]);	\
	     i++)							\
		if ((power_well)->domains & (domain_mask))

#define for_each_power_well_rev(i, power_well, domain_mask, power_domains) \
	for (i = (power_domains)->power_well_count - 1;			 \
	     i >= 0 && ((power_well) = &(power_domains)->power_wells[i]);\
	     i--)							 \
		if ((power_well)->domains & (domain_mask))

bool intel_display_power_well_is_enabled(struct drm_i915_private *dev_priv,
				    int power_well_id);

static void intel_power_well_enable(struct drm_i915_private *dev_priv,
				    struct i915_power_well *power_well)
{
	DRM_DEBUG_KMS("enabling %s\n", power_well->name);
	power_well->ops->enable(dev_priv, power_well);
	power_well->hw_enabled = true;
}

static void intel_power_well_disable(struct drm_i915_private *dev_priv,
				     struct i915_power_well *power_well)
{
	DRM_DEBUG_KMS("disabling %s\n", power_well->name);
	power_well->hw_enabled = false;
	power_well->ops->disable(dev_priv, power_well);
}

/*
 * We should only use the power well if we explicitly asked the hardware to
 * enable it, so check if it's enabled and also check if we've requested it to
 * be enabled.
 */
static bool hsw_power_well_enabled(struct drm_i915_private *dev_priv,
				   struct i915_power_well *power_well)
{
	return I915_READ(HSW_PWR_WELL_DRIVER) ==
		     (HSW_PWR_WELL_ENABLE_REQUEST | HSW_PWR_WELL_STATE_ENABLED);
}

/**
 * __intel_display_power_is_enabled - unlocked check for a power domain
 * @dev_priv: i915 device instance
 * @domain: power domain to check
 *
 * This is the unlocked version of intel_display_power_is_enabled() and should
 * only be used from error capture and recovery code where deadlocks are
 * possible.
 *
 * Returns:
 * True when the power domain is enabled, false otherwise.
 */
bool __intel_display_power_is_enabled(struct drm_i915_private *dev_priv,
				      enum intel_display_power_domain domain)
{
	struct i915_power_domains *power_domains;
	struct i915_power_well *power_well;
	bool is_enabled;
	int i;

	if (dev_priv->pm.suspended)
		return false;

	power_domains = &dev_priv->power_domains;

	is_enabled = true;

	for_each_power_well_rev(i, power_well, BIT(domain), power_domains) {
		if (power_well->always_on)
			continue;

		if (!power_well->hw_enabled) {
			is_enabled = false;
			break;
		}
	}

	return is_enabled;
}

/**
 * intel_display_power_is_enabled - check for a power domain
 * @dev_priv: i915 device instance
 * @domain: power domain to check
 *
 * This function can be used to check the hw power domain state. It is mostly
 * used in hardware state readout functions. Everywhere else code should rely
 * upon explicit power domain reference counting to ensure that the hardware
 * block is powered up before accessing it.
 *
 * Callers must hold the relevant modesetting locks to ensure that concurrent
 * threads can't disable the power well while the caller tries to read a few
 * registers.
 *
 * Returns:
 * True when the power domain is enabled, false otherwise.
 */
bool intel_display_power_is_enabled(struct drm_i915_private *dev_priv,
				    enum intel_display_power_domain domain)
{
	struct i915_power_domains *power_domains;
	bool ret;

	power_domains = &dev_priv->power_domains;

	mutex_lock(&power_domains->lock);
	ret = __intel_display_power_is_enabled(dev_priv, domain);
	mutex_unlock(&power_domains->lock);

	return ret;
}

/**
 * intel_display_set_init_power - set the initial power domain state
 * @dev_priv: i915 device instance
 * @enable: whether to enable or disable the initial power domain state
 *
 * For simplicity our driver load/unload and system suspend/resume code assumes
 * that all power domains are always enabled. This functions controls the state
 * of this little hack. While the initial power domain state is enabled runtime
 * pm is effectively disabled.
 */
void intel_display_set_init_power(struct drm_i915_private *dev_priv,
				  bool enable)
{
	if (dev_priv->power_domains.init_power_on == enable)
		return;

	if (enable)
		intel_display_power_get(dev_priv, POWER_DOMAIN_INIT);
	else
		intel_display_power_put(dev_priv, POWER_DOMAIN_INIT);

	dev_priv->power_domains.init_power_on = enable;
}

/*
 * Starting with Haswell, we have a "Power Down Well" that can be turned off
 * when not needed anymore. We have 4 registers that can request the power well
 * to be enabled, and it will only be disabled if none of the registers is
 * requesting it to be enabled.
 */
static void hsw_power_well_post_enable(struct drm_i915_private *dev_priv)
{
	struct drm_device *dev = dev_priv->dev;

	/*
	 * After we re-enable the power well, if we touch VGA register 0x3d5
	 * we'll get unclaimed register interrupts. This stops after we write
	 * anything to the VGA MSR register. The vgacon module uses this
	 * register all the time, so if we unbind our driver and, as a
	 * consequence, bind vgacon, we'll get stuck in an infinite loop at
	 * console_unlock(). So make here we touch the VGA MSR register, making
	 * sure vgacon can keep working normally without triggering interrupts
	 * and error messages.
	 */
	vga_get_uninterruptible(dev->pdev, VGA_RSRC_LEGACY_IO);
#ifdef __linux__
	outb(inb(VGA_MSR_READ), VGA_MSR_WRITE);
#else
	outb(VGA_MSR_WRITE, inb(VGA_MSR_READ));
#endif
	vga_put(dev->pdev, VGA_RSRC_LEGACY_IO);

	if (IS_BROADWELL(dev))
		gen8_irq_power_well_post_enable(dev_priv,
						1 << PIPE_C | 1 << PIPE_B);
}

static void skl_power_well_post_enable(struct drm_i915_private *dev_priv,
				       struct i915_power_well *power_well)
{
	struct drm_device *dev = dev_priv->dev;

	/*
	 * After we re-enable the power well, if we touch VGA register 0x3d5
	 * we'll get unclaimed register interrupts. This stops after we write
	 * anything to the VGA MSR register. The vgacon module uses this
	 * register all the time, so if we unbind our driver and, as a
	 * consequence, bind vgacon, we'll get stuck in an infinite loop at
	 * console_unlock(). So make here we touch the VGA MSR register, making
	 * sure vgacon can keep working normally without triggering interrupts
	 * and error messages.
	 */
	if (power_well->data == SKL_DISP_PW_2) {
		vga_get_uninterruptible(dev->pdev, VGA_RSRC_LEGACY_IO);
#ifdef __linux__
		outb(inb(VGA_MSR_READ), VGA_MSR_WRITE);
#else
		outb(VGA_MSR_WRITE, inb(VGA_MSR_READ));
#endif
		vga_put(dev->pdev, VGA_RSRC_LEGACY_IO);

		gen8_irq_power_well_post_enable(dev_priv,
						1 << PIPE_C | 1 << PIPE_B);
	}

	if (power_well->data == SKL_DISP_PW_1) {
		if (!dev_priv->power_domains.initializing)
			intel_prepare_ddi(dev);
		gen8_irq_power_well_post_enable(dev_priv, 1 << PIPE_A);
	}
}

static void hsw_set_power_well(struct drm_i915_private *dev_priv,
			       struct i915_power_well *power_well, bool enable)
{
	bool is_enabled, enable_requested;
	uint32_t tmp;

	tmp = I915_READ(HSW_PWR_WELL_DRIVER);
	is_enabled = tmp & HSW_PWR_WELL_STATE_ENABLED;
	enable_requested = tmp & HSW_PWR_WELL_ENABLE_REQUEST;

	if (enable) {
		if (!enable_requested)
			I915_WRITE(HSW_PWR_WELL_DRIVER,
				   HSW_PWR_WELL_ENABLE_REQUEST);

		if (!is_enabled) {
			DRM_DEBUG_KMS("Enabling power well\n");
			if (wait_for((I915_READ(HSW_PWR_WELL_DRIVER) &
				      HSW_PWR_WELL_STATE_ENABLED), 20))
				DRM_ERROR("Timeout enabling power well\n");
			hsw_power_well_post_enable(dev_priv);
		}

	} else {
		if (enable_requested) {
			I915_WRITE(HSW_PWR_WELL_DRIVER, 0);
			POSTING_READ(HSW_PWR_WELL_DRIVER);
			DRM_DEBUG_KMS("Requesting to disable the power well\n");
		}
	}
}

#define SKL_DISPLAY_POWERWELL_2_POWER_DOMAINS (		\
	BIT(POWER_DOMAIN_TRANSCODER_A) |		\
	BIT(POWER_DOMAIN_PIPE_B) |			\
	BIT(POWER_DOMAIN_TRANSCODER_B) |		\
	BIT(POWER_DOMAIN_PIPE_C) |			\
	BIT(POWER_DOMAIN_TRANSCODER_C) |		\
	BIT(POWER_DOMAIN_PIPE_B_PANEL_FITTER) |		\
	BIT(POWER_DOMAIN_PIPE_C_PANEL_FITTER) |		\
	BIT(POWER_DOMAIN_PORT_DDI_B_2_LANES) |		\
	BIT(POWER_DOMAIN_PORT_DDI_B_4_LANES) |		\
	BIT(POWER_DOMAIN_PORT_DDI_C_2_LANES) |		\
	BIT(POWER_DOMAIN_PORT_DDI_C_4_LANES) |		\
	BIT(POWER_DOMAIN_PORT_DDI_D_2_LANES) |		\
	BIT(POWER_DOMAIN_PORT_DDI_D_4_LANES) |		\
	BIT(POWER_DOMAIN_PORT_DDI_E_2_LANES) |		\
	BIT(POWER_DOMAIN_AUX_B) |                       \
	BIT(POWER_DOMAIN_AUX_C) |			\
	BIT(POWER_DOMAIN_AUX_D) |			\
	BIT(POWER_DOMAIN_AUDIO) |			\
	BIT(POWER_DOMAIN_VGA) |				\
	BIT(POWER_DOMAIN_INIT))
#define SKL_DISPLAY_POWERWELL_1_POWER_DOMAINS (		\
	SKL_DISPLAY_POWERWELL_2_POWER_DOMAINS |		\
	BIT(POWER_DOMAIN_PLLS) |			\
	BIT(POWER_DOMAIN_PIPE_A) |			\
	BIT(POWER_DOMAIN_TRANSCODER_EDP) |		\
	BIT(POWER_DOMAIN_PIPE_A_PANEL_FITTER) |		\
	BIT(POWER_DOMAIN_PORT_DDI_A_2_LANES) |		\
	BIT(POWER_DOMAIN_PORT_DDI_A_4_LANES) |		\
	BIT(POWER_DOMAIN_AUX_A) |			\
	BIT(POWER_DOMAIN_INIT))
#define SKL_DISPLAY_DDI_A_E_POWER_DOMAINS (		\
	BIT(POWER_DOMAIN_PORT_DDI_A_2_LANES) |		\
	BIT(POWER_DOMAIN_PORT_DDI_A_4_LANES) |		\
	BIT(POWER_DOMAIN_PORT_DDI_E_2_LANES) |		\
	BIT(POWER_DOMAIN_INIT))
#define SKL_DISPLAY_DDI_B_POWER_DOMAINS (		\
	BIT(POWER_DOMAIN_PORT_DDI_B_2_LANES) |		\
	BIT(POWER_DOMAIN_PORT_DDI_B_4_LANES) |		\
	BIT(POWER_DOMAIN_INIT))
#define SKL_DISPLAY_DDI_C_POWER_DOMAINS (		\
	BIT(POWER_DOMAIN_PORT_DDI_C_2_LANES) |		\
	BIT(POWER_DOMAIN_PORT_DDI_C_4_LANES) |		\
	BIT(POWER_DOMAIN_INIT))
#define SKL_DISPLAY_DDI_D_POWER_DOMAINS (		\
	BIT(POWER_DOMAIN_PORT_DDI_D_2_LANES) |		\
	BIT(POWER_DOMAIN_PORT_DDI_D_4_LANES) |		\
	BIT(POWER_DOMAIN_INIT))
#define SKL_DISPLAY_MISC_IO_POWER_DOMAINS (		\
	SKL_DISPLAY_POWERWELL_1_POWER_DOMAINS |		\
	BIT(POWER_DOMAIN_PLLS) |			\
	BIT(POWER_DOMAIN_INIT))
#define SKL_DISPLAY_ALWAYS_ON_POWER_DOMAINS (		\
	(POWER_DOMAIN_MASK & ~(SKL_DISPLAY_POWERWELL_1_POWER_DOMAINS |	\
	SKL_DISPLAY_POWERWELL_2_POWER_DOMAINS |		\
	SKL_DISPLAY_DDI_A_E_POWER_DOMAINS |		\
	SKL_DISPLAY_DDI_B_POWER_DOMAINS |		\
	SKL_DISPLAY_DDI_C_POWER_DOMAINS |		\
	SKL_DISPLAY_DDI_D_POWER_DOMAINS |		\
	SKL_DISPLAY_MISC_IO_POWER_DOMAINS)) |		\
	BIT(POWER_DOMAIN_INIT))

#define BXT_DISPLAY_POWERWELL_2_POWER_DOMAINS (		\
	BIT(POWER_DOMAIN_TRANSCODER_A) |		\
	BIT(POWER_DOMAIN_PIPE_B) |			\
	BIT(POWER_DOMAIN_TRANSCODER_B) |		\
	BIT(POWER_DOMAIN_PIPE_C) |			\
	BIT(POWER_DOMAIN_TRANSCODER_C) |		\
	BIT(POWER_DOMAIN_PIPE_B_PANEL_FITTER) |		\
	BIT(POWER_DOMAIN_PIPE_C_PANEL_FITTER) |		\
	BIT(POWER_DOMAIN_PORT_DDI_B_2_LANES) |		\
	BIT(POWER_DOMAIN_PORT_DDI_B_4_LANES) |		\
	BIT(POWER_DOMAIN_PORT_DDI_C_2_LANES) |		\
	BIT(POWER_DOMAIN_PORT_DDI_C_4_LANES) |		\
	BIT(POWER_DOMAIN_AUX_B) |			\
	BIT(POWER_DOMAIN_AUX_C) |			\
	BIT(POWER_DOMAIN_AUDIO) |			\
	BIT(POWER_DOMAIN_VGA) |				\
	BIT(POWER_DOMAIN_GMBUS) |			\
	BIT(POWER_DOMAIN_INIT))
#define BXT_DISPLAY_POWERWELL_1_POWER_DOMAINS (		\
	BXT_DISPLAY_POWERWELL_2_POWER_DOMAINS |		\
	BIT(POWER_DOMAIN_PIPE_A) |			\
	BIT(POWER_DOMAIN_TRANSCODER_EDP) |		\
	BIT(POWER_DOMAIN_PIPE_A_PANEL_FITTER) |		\
	BIT(POWER_DOMAIN_PORT_DDI_A_2_LANES) |		\
	BIT(POWER_DOMAIN_PORT_DDI_A_4_LANES) |		\
	BIT(POWER_DOMAIN_AUX_A) |			\
	BIT(POWER_DOMAIN_PLLS) |			\
	BIT(POWER_DOMAIN_INIT))
#define BXT_DISPLAY_ALWAYS_ON_POWER_DOMAINS (		\
	(POWER_DOMAIN_MASK & ~(BXT_DISPLAY_POWERWELL_1_POWER_DOMAINS |	\
	BXT_DISPLAY_POWERWELL_2_POWER_DOMAINS)) |	\
	BIT(POWER_DOMAIN_INIT))

static void assert_can_enable_dc9(struct drm_i915_private *dev_priv)
{
	struct drm_device *dev = dev_priv->dev;

	WARN(!IS_BROXTON(dev), "Platform doesn't support DC9.\n");
	WARN((I915_READ(DC_STATE_EN) & DC_STATE_EN_DC9),
		"DC9 already programmed to be enabled.\n");
	WARN(I915_READ(DC_STATE_EN) & DC_STATE_EN_UPTO_DC5,
		"DC5 still not disabled to enable DC9.\n");
	WARN(I915_READ(HSW_PWR_WELL_DRIVER), "Power well on.\n");
	WARN(intel_irqs_enabled(dev_priv), "Interrupts not disabled yet.\n");

	 /*
	  * TODO: check for the following to verify the conditions to enter DC9
	  * state are satisfied:
	  * 1] Check relevant display engine registers to verify if mode set
	  * disable sequence was followed.
	  * 2] Check if display uninitialize sequence is initialized.
	  */
}

static void assert_can_disable_dc9(struct drm_i915_private *dev_priv)
{
	WARN(intel_irqs_enabled(dev_priv), "Interrupts not disabled yet.\n");
	WARN(!(I915_READ(DC_STATE_EN) & DC_STATE_EN_DC9),
		"DC9 already programmed to be disabled.\n");
	WARN(I915_READ(DC_STATE_EN) & DC_STATE_EN_UPTO_DC5,
		"DC5 still not disabled.\n");

	 /*
	  * TODO: check for the following to verify DC9 state was indeed
	  * entered before programming to disable it:
	  * 1] Check relevant display engine registers to verify if mode
	  *  set disable sequence was followed.
	  * 2] Check if display uninitialize sequence is initialized.
	  */
}

void bxt_enable_dc9(struct drm_i915_private *dev_priv)
{
	uint32_t val;

	assert_can_enable_dc9(dev_priv);

	DRM_DEBUG_KMS("Enabling DC9\n");

	val = I915_READ(DC_STATE_EN);
	val |= DC_STATE_EN_DC9;
	I915_WRITE(DC_STATE_EN, val);
	POSTING_READ(DC_STATE_EN);
}

void bxt_disable_dc9(struct drm_i915_private *dev_priv)
{
	uint32_t val;

	assert_can_disable_dc9(dev_priv);

	DRM_DEBUG_KMS("Disabling DC9\n");

	val = I915_READ(DC_STATE_EN);
	val &= ~DC_STATE_EN_DC9;
	I915_WRITE(DC_STATE_EN, val);
	POSTING_READ(DC_STATE_EN);
}

static void gen9_set_dc_state_debugmask_memory_up(
			struct drm_i915_private *dev_priv)
{
	uint32_t val;

	/* The below bit doesn't need to be cleared ever afterwards */
	val = I915_READ(DC_STATE_DEBUG);
	if (!(val & DC_STATE_DEBUG_MASK_MEMORY_UP)) {
		val |= DC_STATE_DEBUG_MASK_MEMORY_UP;
		I915_WRITE(DC_STATE_DEBUG, val);
		POSTING_READ(DC_STATE_DEBUG);
	}
}

static void assert_can_enable_dc5(struct drm_i915_private *dev_priv)
{
	struct drm_device *dev = dev_priv->dev;
	bool pg2_enabled = intel_display_power_well_is_enabled(dev_priv,
					SKL_DISP_PW_2);

	WARN_ONCE(!IS_SKYLAKE(dev) && !IS_KABYLAKE(dev),
		  "Platform doesn't support DC5.\n");
	WARN_ONCE(!HAS_RUNTIME_PM(dev), "Runtime PM not enabled.\n");
	WARN_ONCE(pg2_enabled, "PG2 not disabled to enable DC5.\n");

	WARN_ONCE((I915_READ(DC_STATE_EN) & DC_STATE_EN_UPTO_DC5),
		  "DC5 already programmed to be enabled.\n");
	WARN_ONCE(dev_priv->pm.suspended,
		  "DC5 cannot be enabled, if platform is runtime-suspended.\n");

	assert_csr_loaded(dev_priv);
}

static void assert_can_disable_dc5(struct drm_i915_private *dev_priv)
{
	bool pg2_enabled = intel_display_power_well_is_enabled(dev_priv,
					SKL_DISP_PW_2);
	/*
	 * During initialization, the firmware may not be loaded yet.
	 * We still want to make sure that the DC enabling flag is cleared.
	 */
	if (dev_priv->power_domains.initializing)
		return;

	WARN_ONCE(!pg2_enabled, "PG2 not enabled to disable DC5.\n");
	WARN_ONCE(dev_priv->pm.suspended,
		"Disabling of DC5 while platform is runtime-suspended should never happen.\n");
}

static void gen9_enable_dc5(struct drm_i915_private *dev_priv)
{
	uint32_t val;

	assert_can_enable_dc5(dev_priv);

	DRM_DEBUG_KMS("Enabling DC5\n");

	gen9_set_dc_state_debugmask_memory_up(dev_priv);

	val = I915_READ(DC_STATE_EN);
	val &= ~DC_STATE_EN_UPTO_DC5_DC6_MASK;
	val |= DC_STATE_EN_UPTO_DC5;
	I915_WRITE(DC_STATE_EN, val);
	POSTING_READ(DC_STATE_EN);
}

static void gen9_disable_dc5(struct drm_i915_private *dev_priv)
{
	uint32_t val;

	assert_can_disable_dc5(dev_priv);

	DRM_DEBUG_KMS("Disabling DC5\n");

	val = I915_READ(DC_STATE_EN);
	val &= ~DC_STATE_EN_UPTO_DC5;
	I915_WRITE(DC_STATE_EN, val);
	POSTING_READ(DC_STATE_EN);
}

static void assert_can_enable_dc6(struct drm_i915_private *dev_priv)
{
	struct drm_device *dev = dev_priv->dev;

	WARN_ONCE(!IS_SKYLAKE(dev) && !IS_KABYLAKE(dev),
		  "Platform doesn't support DC6.\n");
	WARN_ONCE(!HAS_RUNTIME_PM(dev), "Runtime PM not enabled.\n");
	WARN_ONCE(I915_READ(UTIL_PIN_CTL) & UTIL_PIN_ENABLE,
		  "Backlight is not disabled.\n");
	WARN_ONCE((I915_READ(DC_STATE_EN) & DC_STATE_EN_UPTO_DC6),
		  "DC6 already programmed to be enabled.\n");

	assert_csr_loaded(dev_priv);
}

static void assert_can_disable_dc6(struct drm_i915_private *dev_priv)
{
	/*
	 * During initialization, the firmware may not be loaded yet.
	 * We still want to make sure that the DC enabling flag is cleared.
	 */
	if (dev_priv->power_domains.initializing)
		return;

	assert_csr_loaded(dev_priv);
	WARN_ONCE(!(I915_READ(DC_STATE_EN) & DC_STATE_EN_UPTO_DC6),
		  "DC6 already programmed to be disabled.\n");
}

static void skl_enable_dc6(struct drm_i915_private *dev_priv)
{
	uint32_t val;

	assert_can_enable_dc6(dev_priv);

	DRM_DEBUG_KMS("Enabling DC6\n");

	gen9_set_dc_state_debugmask_memory_up(dev_priv);

	val = I915_READ(DC_STATE_EN);
	val &= ~DC_STATE_EN_UPTO_DC5_DC6_MASK;
	val |= DC_STATE_EN_UPTO_DC6;
	I915_WRITE(DC_STATE_EN, val);
	POSTING_READ(DC_STATE_EN);
}

static void skl_disable_dc6(struct drm_i915_private *dev_priv)
{
	uint32_t val;

	assert_can_disable_dc6(dev_priv);

	DRM_DEBUG_KMS("Disabling DC6\n");

	val = I915_READ(DC_STATE_EN);
	val &= ~DC_STATE_EN_UPTO_DC6;
	I915_WRITE(DC_STATE_EN, val);
	POSTING_READ(DC_STATE_EN);
}

static void skl_set_power_well(struct drm_i915_private *dev_priv,
			struct i915_power_well *power_well, bool enable)
{
	struct drm_device *dev = dev_priv->dev;
	uint32_t tmp, fuse_status;
	uint32_t req_mask, state_mask;
	bool is_enabled, enable_requested, check_fuse_status = false;

	tmp = I915_READ(HSW_PWR_WELL_DRIVER);
	fuse_status = I915_READ(SKL_FUSE_STATUS);

	switch (power_well->data) {
	case SKL_DISP_PW_1:
		if (wait_for((I915_READ(SKL_FUSE_STATUS) &
			SKL_FUSE_PG0_DIST_STATUS), 1)) {
			DRM_ERROR("PG0 not enabled\n");
			return;
		}
		break;
	case SKL_DISP_PW_2:
		if (!(fuse_status & SKL_FUSE_PG1_DIST_STATUS)) {
			DRM_ERROR("PG1 in disabled state\n");
			return;
		}
		break;
	case SKL_DISP_PW_DDI_A_E:
	case SKL_DISP_PW_DDI_B:
	case SKL_DISP_PW_DDI_C:
	case SKL_DISP_PW_DDI_D:
	case SKL_DISP_PW_MISC_IO:
		break;
	default:
		WARN(1, "Unknown power well %lu\n", power_well->data);
		return;
	}

	req_mask = SKL_POWER_WELL_REQ(power_well->data);
	enable_requested = tmp & req_mask;
	state_mask = SKL_POWER_WELL_STATE(power_well->data);
	is_enabled = tmp & state_mask;

	if (enable) {
		if (!enable_requested) {
			WARN((tmp & state_mask) &&
				!I915_READ(HSW_PWR_WELL_BIOS),
				"Invalid for power well status to be enabled, unless done by the BIOS, \
				when request is to disable!\n");
			if ((GEN9_ENABLE_DC5(dev) || SKL_ENABLE_DC6(dev)) &&
				power_well->data == SKL_DISP_PW_2) {
				if (SKL_ENABLE_DC6(dev)) {
					skl_disable_dc6(dev_priv);
					/*
					 * DDI buffer programming unnecessary during driver-load/resume
					 * as it's already done during modeset initialization then.
					 * It's also invalid here as encoder list is still uninitialized.
					 */
					if (!dev_priv->power_domains.initializing)
						intel_prepare_ddi(dev);
				} else {
					gen9_disable_dc5(dev_priv);
				}
			}
			I915_WRITE(HSW_PWR_WELL_DRIVER, tmp | req_mask);
		}

		if (!is_enabled) {
			DRM_DEBUG_KMS("Enabling %s\n", power_well->name);
			if (wait_for((I915_READ(HSW_PWR_WELL_DRIVER) &
				state_mask), 1))
				DRM_ERROR("%s enable timeout\n",
					power_well->name);
			check_fuse_status = true;
		}
	} else {
		if (enable_requested) {
			if ((IS_SKYLAKE(dev) || IS_KABYLAKE(dev)) &&
				(power_well->data == SKL_DISP_PW_1) &&
				(intel_csr_load_status_get(dev_priv) == FW_LOADED))
				DRM_DEBUG_KMS("Not Disabling PW1, dmc will handle\n");
			else {
				I915_WRITE(HSW_PWR_WELL_DRIVER,	tmp & ~req_mask);
				POSTING_READ(HSW_PWR_WELL_DRIVER);
				DRM_DEBUG_KMS("Disabling %s\n", power_well->name);
			}

			if ((GEN9_ENABLE_DC5(dev) || SKL_ENABLE_DC6(dev)) &&
				power_well->data == SKL_DISP_PW_2) {
				enum csr_state state;
				/* TODO: wait for a completion event or
				 * similar here instead of busy
				 * waiting using wait_for function.
				 */
				wait_for((state = intel_csr_load_status_get(dev_priv)) !=
						FW_UNINITIALIZED, 1000);
				if (state != FW_LOADED)
					DRM_DEBUG("CSR firmware not ready (%d)\n",
							state);
				else
					if (SKL_ENABLE_DC6(dev))
						skl_enable_dc6(dev_priv);
					else
						gen9_enable_dc5(dev_priv);
			}
		}
	}

	if (check_fuse_status) {
		if (power_well->data == SKL_DISP_PW_1) {
			if (wait_for((I915_READ(SKL_FUSE_STATUS) &
				SKL_FUSE_PG1_DIST_STATUS), 1))
				DRM_ERROR("PG1 distributing status timeout\n");
		} else if (power_well->data == SKL_DISP_PW_2) {
			if (wait_for((I915_READ(SKL_FUSE_STATUS) &
				SKL_FUSE_PG2_DIST_STATUS), 1))
				DRM_ERROR("PG2 distributing status timeout\n");
		}
	}

	if (enable && !is_enabled)
		skl_power_well_post_enable(dev_priv, power_well);
}

static void hsw_power_well_sync_hw(struct drm_i915_private *dev_priv,
				   struct i915_power_well *power_well)
{
	hsw_set_power_well(dev_priv, power_well, power_well->count > 0);

	/*
	 * We're taking over the BIOS, so clear any requests made by it since
	 * the driver is in charge now.
	 */
	if (I915_READ(HSW_PWR_WELL_BIOS) & HSW_PWR_WELL_ENABLE_REQUEST)
		I915_WRITE(HSW_PWR_WELL_BIOS, 0);
}

static void hsw_power_well_enable(struct drm_i915_private *dev_priv,
				  struct i915_power_well *power_well)
{
	hsw_set_power_well(dev_priv, power_well, true);
}

static void hsw_power_well_disable(struct drm_i915_private *dev_priv,
				   struct i915_power_well *power_well)
{
	hsw_set_power_well(dev_priv, power_well, false);
}

static bool skl_power_well_enabled(struct drm_i915_private *dev_priv,
					struct i915_power_well *power_well)
{
	uint32_t mask = SKL_POWER_WELL_REQ(power_well->data) |
		SKL_POWER_WELL_STATE(power_well->data);

	return (I915_READ(HSW_PWR_WELL_DRIVER) & mask) == mask;
}

static void skl_power_well_sync_hw(struct drm_i915_private *dev_priv,
				struct i915_power_well *power_well)
{
	skl_set_power_well(dev_priv, power_well, power_well->count > 0);

	/* Clear any request made by BIOS as driver is taking over */
	I915_WRITE(HSW_PWR_WELL_BIOS, 0);
}

static void skl_power_well_enable(struct drm_i915_private *dev_priv,
				struct i915_power_well *power_well)
{
	skl_set_power_well(dev_priv, power_well, true);
}

static void skl_power_well_disable(struct drm_i915_private *dev_priv,
				struct i915_power_well *power_well)
{
	skl_set_power_well(dev_priv, power_well, false);
}

static void i9xx_always_on_power_well_noop(struct drm_i915_private *dev_priv,
					   struct i915_power_well *power_well)
{
}

static bool i9xx_always_on_power_well_enabled(struct drm_i915_private *dev_priv,
					     struct i915_power_well *power_well)
{
	return true;
}

static void vlv_set_power_well(struct drm_i915_private *dev_priv,
			       struct i915_power_well *power_well, bool enable)
{
	enum punit_power_well power_well_id = power_well->data;
	u32 mask;
	u32 state;
	u32 ctrl;

	mask = PUNIT_PWRGT_MASK(power_well_id);
	state = enable ? PUNIT_PWRGT_PWR_ON(power_well_id) :
			 PUNIT_PWRGT_PWR_GATE(power_well_id);

	mutex_lock(&dev_priv->rps.hw_lock);

#define COND \
	((vlv_punit_read(dev_priv, PUNIT_REG_PWRGT_STATUS) & mask) == state)

	if (COND)
		goto out;

	ctrl = vlv_punit_read(dev_priv, PUNIT_REG_PWRGT_CTRL);
	ctrl &= ~mask;
	ctrl |= state;
	vlv_punit_write(dev_priv, PUNIT_REG_PWRGT_CTRL, ctrl);

	if (wait_for(COND, 100))
		DRM_ERROR("timeout setting power well state %08x (%08x)\n",
			  state,
			  vlv_punit_read(dev_priv, PUNIT_REG_PWRGT_CTRL));

#undef COND

out:
	mutex_unlock(&dev_priv->rps.hw_lock);
}

static void vlv_power_well_sync_hw(struct drm_i915_private *dev_priv,
				   struct i915_power_well *power_well)
{
	vlv_set_power_well(dev_priv, power_well, power_well->count > 0);
}

static void vlv_power_well_enable(struct drm_i915_private *dev_priv,
				  struct i915_power_well *power_well)
{
	vlv_set_power_well(dev_priv, power_well, true);
}

static void vlv_power_well_disable(struct drm_i915_private *dev_priv,
				   struct i915_power_well *power_well)
{
	vlv_set_power_well(dev_priv, power_well, false);
}

static bool vlv_power_well_enabled(struct drm_i915_private *dev_priv,
				   struct i915_power_well *power_well)
{
	int power_well_id = power_well->data;
	bool enabled = false;
	u32 mask;
	u32 state;
	u32 ctrl;

	mask = PUNIT_PWRGT_MASK(power_well_id);
	ctrl = PUNIT_PWRGT_PWR_ON(power_well_id);

	mutex_lock(&dev_priv->rps.hw_lock);

	state = vlv_punit_read(dev_priv, PUNIT_REG_PWRGT_STATUS) & mask;
	/*
	 * We only ever set the power-on and power-gate states, anything
	 * else is unexpected.
	 */
	WARN_ON(state != PUNIT_PWRGT_PWR_ON(power_well_id) &&
		state != PUNIT_PWRGT_PWR_GATE(power_well_id));
	if (state == ctrl)
		enabled = true;

	/*
	 * A transient state at this point would mean some unexpected party
	 * is poking at the power controls too.
	 */
	ctrl = vlv_punit_read(dev_priv, PUNIT_REG_PWRGT_CTRL) & mask;
	WARN_ON(ctrl != state);

	mutex_unlock(&dev_priv->rps.hw_lock);

	return enabled;
}

static void vlv_display_power_well_init(struct drm_i915_private *dev_priv)
{
	enum pipe pipe;

	/*
	 * Enable the CRI clock source so we can get at the
	 * display and the reference clock for VGA
	 * hotplug / manual detection. Supposedly DSI also
	 * needs the ref clock up and running.
	 *
	 * CHV DPLL B/C have some issues if VGA mode is enabled.
	 */
	for_each_pipe(dev_priv->dev, pipe) {
		u32 val = I915_READ(DPLL(pipe));

		val |= DPLL_REF_CLK_ENABLE_VLV | DPLL_VGA_MODE_DIS;
		if (pipe != PIPE_A)
			val |= DPLL_INTEGRATED_CRI_CLK_VLV;

		I915_WRITE(DPLL(pipe), val);
	}

	spin_lock_irq(&dev_priv->irq_lock);
	valleyview_enable_display_irqs(dev_priv);
	spin_unlock_irq(&dev_priv->irq_lock);

	/*
	 * During driver initialization/resume we can avoid restoring the
	 * part of the HW/SW state that will be inited anyway explicitly.
	 */
	if (dev_priv->power_domains.initializing)
		return;

	intel_hpd_init(dev_priv);

	i915_redisable_vga_power_on(dev_priv->dev);
}

static void vlv_display_power_well_deinit(struct drm_i915_private *dev_priv)
{
	spin_lock_irq(&dev_priv->irq_lock);
	valleyview_disable_display_irqs(dev_priv);
	spin_unlock_irq(&dev_priv->irq_lock);

	vlv_power_sequencer_reset(dev_priv);
}

static void vlv_display_power_well_enable(struct drm_i915_private *dev_priv,
					  struct i915_power_well *power_well)
{
	WARN_ON_ONCE(power_well->data != PUNIT_POWER_WELL_DISP2D);

	vlv_set_power_well(dev_priv, power_well, true);

	vlv_display_power_well_init(dev_priv);
}

static void vlv_display_power_well_disable(struct drm_i915_private *dev_priv,
					   struct i915_power_well *power_well)
{
	WARN_ON_ONCE(power_well->data != PUNIT_POWER_WELL_DISP2D);

	vlv_display_power_well_deinit(dev_priv);

	vlv_set_power_well(dev_priv, power_well, false);
}

static void vlv_dpio_cmn_power_well_enable(struct drm_i915_private *dev_priv,
					   struct i915_power_well *power_well)
{
	WARN_ON_ONCE(power_well->data != PUNIT_POWER_WELL_DPIO_CMN_BC);

	/* since ref/cri clock was enabled */
	udelay(1); /* >10ns for cmnreset, >0ns for sidereset */

	vlv_set_power_well(dev_priv, power_well, true);

	/*
	 * From VLV2A0_DP_eDP_DPIO_driver_vbios_notes_10.docx -
	 *  6.	De-assert cmn_reset/side_reset. Same as VLV X0.
	 *   a.	GUnit 0x2110 bit[0] set to 1 (def 0)
	 *   b.	The other bits such as sfr settings / modesel may all
	 *	be set to 0.
	 *
	 * This should only be done on init and resume from S3 with
	 * both PLLs disabled, or we risk losing DPIO and PLL
	 * synchronization.
	 */
	I915_WRITE(DPIO_CTL, I915_READ(DPIO_CTL) | DPIO_CMNRST);
}

static void vlv_dpio_cmn_power_well_disable(struct drm_i915_private *dev_priv,
					    struct i915_power_well *power_well)
{
	enum pipe pipe;

	WARN_ON_ONCE(power_well->data != PUNIT_POWER_WELL_DPIO_CMN_BC);

	for_each_pipe(dev_priv, pipe)
		assert_pll_disabled(dev_priv, pipe);

	/* Assert common reset */
	I915_WRITE(DPIO_CTL, I915_READ(DPIO_CTL) & ~DPIO_CMNRST);

	vlv_set_power_well(dev_priv, power_well, false);
}

#define POWER_DOMAIN_MASK (BIT(POWER_DOMAIN_NUM) - 1)

static struct i915_power_well *lookup_power_well(struct drm_i915_private *dev_priv,
						 int power_well_id)
{
	struct i915_power_domains *power_domains = &dev_priv->power_domains;
	struct i915_power_well *power_well;
	int i;

	for_each_power_well(i, power_well, POWER_DOMAIN_MASK, power_domains) {
		if (power_well->data == power_well_id)
			return power_well;
	}

	return NULL;
}

#define BITS_SET(val, bits) (((val) & (bits)) == (bits))

static void assert_chv_phy_status(struct drm_i915_private *dev_priv)
{
	struct i915_power_well *cmn_bc =
		lookup_power_well(dev_priv, PUNIT_POWER_WELL_DPIO_CMN_BC);
	struct i915_power_well *cmn_d =
		lookup_power_well(dev_priv, PUNIT_POWER_WELL_DPIO_CMN_D);
	u32 phy_control = dev_priv->chv_phy_control;
	u32 phy_status = 0;
	u32 phy_status_mask = 0xffffffff;
	u32 tmp;

	/*
	 * The BIOS can leave the PHY is some weird state
	 * where it doesn't fully power down some parts.
	 * Disable the asserts until the PHY has been fully
	 * reset (ie. the power well has been disabled at
	 * least once).
	 */
	if (!dev_priv->chv_phy_assert[DPIO_PHY0])
		phy_status_mask &= ~(PHY_STATUS_CMN_LDO(DPIO_PHY0, DPIO_CH0) |
				     PHY_STATUS_SPLINE_LDO(DPIO_PHY0, DPIO_CH0, 0) |
				     PHY_STATUS_SPLINE_LDO(DPIO_PHY0, DPIO_CH0, 1) |
				     PHY_STATUS_CMN_LDO(DPIO_PHY0, DPIO_CH1) |
				     PHY_STATUS_SPLINE_LDO(DPIO_PHY0, DPIO_CH1, 0) |
				     PHY_STATUS_SPLINE_LDO(DPIO_PHY0, DPIO_CH1, 1));

	if (!dev_priv->chv_phy_assert[DPIO_PHY1])
		phy_status_mask &= ~(PHY_STATUS_CMN_LDO(DPIO_PHY1, DPIO_CH0) |
				     PHY_STATUS_SPLINE_LDO(DPIO_PHY1, DPIO_CH0, 0) |
				     PHY_STATUS_SPLINE_LDO(DPIO_PHY1, DPIO_CH0, 1));

	if (cmn_bc->ops->is_enabled(dev_priv, cmn_bc)) {
		phy_status |= PHY_POWERGOOD(DPIO_PHY0);

		/* this assumes override is only used to enable lanes */
		if ((phy_control & PHY_CH_POWER_DOWN_OVRD_EN(DPIO_PHY0, DPIO_CH0)) == 0)
			phy_control |= PHY_CH_POWER_DOWN_OVRD(0xf, DPIO_PHY0, DPIO_CH0);

		if ((phy_control & PHY_CH_POWER_DOWN_OVRD_EN(DPIO_PHY0, DPIO_CH1)) == 0)
			phy_control |= PHY_CH_POWER_DOWN_OVRD(0xf, DPIO_PHY0, DPIO_CH1);

		/* CL1 is on whenever anything is on in either channel */
		if (BITS_SET(phy_control,
			     PHY_CH_POWER_DOWN_OVRD(0xf, DPIO_PHY0, DPIO_CH0) |
			     PHY_CH_POWER_DOWN_OVRD(0xf, DPIO_PHY0, DPIO_CH1)))
			phy_status |= PHY_STATUS_CMN_LDO(DPIO_PHY0, DPIO_CH0);

		/*
		 * The DPLLB check accounts for the pipe B + port A usage
		 * with CL2 powered up but all the lanes in the second channel
		 * powered down.
		 */
		if (BITS_SET(phy_control,
			     PHY_CH_POWER_DOWN_OVRD(0xf, DPIO_PHY0, DPIO_CH1)) &&
		    (I915_READ(DPLL(PIPE_B)) & DPLL_VCO_ENABLE) == 0)
			phy_status |= PHY_STATUS_CMN_LDO(DPIO_PHY0, DPIO_CH1);

		if (BITS_SET(phy_control,
			     PHY_CH_POWER_DOWN_OVRD(0x3, DPIO_PHY0, DPIO_CH0)))
			phy_status |= PHY_STATUS_SPLINE_LDO(DPIO_PHY0, DPIO_CH0, 0);
		if (BITS_SET(phy_control,
			     PHY_CH_POWER_DOWN_OVRD(0xc, DPIO_PHY0, DPIO_CH0)))
			phy_status |= PHY_STATUS_SPLINE_LDO(DPIO_PHY0, DPIO_CH0, 1);

		if (BITS_SET(phy_control,
			     PHY_CH_POWER_DOWN_OVRD(0x3, DPIO_PHY0, DPIO_CH1)))
			phy_status |= PHY_STATUS_SPLINE_LDO(DPIO_PHY0, DPIO_CH1, 0);
		if (BITS_SET(phy_control,
			     PHY_CH_POWER_DOWN_OVRD(0xc, DPIO_PHY0, DPIO_CH1)))
			phy_status |= PHY_STATUS_SPLINE_LDO(DPIO_PHY0, DPIO_CH1, 1);
	}

	if (cmn_d->ops->is_enabled(dev_priv, cmn_d)) {
		phy_status |= PHY_POWERGOOD(DPIO_PHY1);

		/* this assumes override is only used to enable lanes */
		if ((phy_control & PHY_CH_POWER_DOWN_OVRD_EN(DPIO_PHY1, DPIO_CH0)) == 0)
			phy_control |= PHY_CH_POWER_DOWN_OVRD(0xf, DPIO_PHY1, DPIO_CH0);

		if (BITS_SET(phy_control,
			     PHY_CH_POWER_DOWN_OVRD(0xf, DPIO_PHY1, DPIO_CH0)))
			phy_status |= PHY_STATUS_CMN_LDO(DPIO_PHY1, DPIO_CH0);

		if (BITS_SET(phy_control,
			     PHY_CH_POWER_DOWN_OVRD(0x3, DPIO_PHY1, DPIO_CH0)))
			phy_status |= PHY_STATUS_SPLINE_LDO(DPIO_PHY1, DPIO_CH0, 0);
		if (BITS_SET(phy_control,
			     PHY_CH_POWER_DOWN_OVRD(0xc, DPIO_PHY1, DPIO_CH0)))
			phy_status |= PHY_STATUS_SPLINE_LDO(DPIO_PHY1, DPIO_CH0, 1);
	}

	phy_status &= phy_status_mask;

	/*
	 * The PHY may be busy with some initial calibration and whatnot,
	 * so the power state can take a while to actually change.
	 */
	if (wait_for((tmp = I915_READ(DISPLAY_PHY_STATUS) & phy_status_mask) == phy_status, 10))
		WARN(phy_status != tmp,
		     "Unexpected PHY_STATUS 0x%08x, expected 0x%08x (PHY_CONTROL=0x%08x)\n",
		     tmp, phy_status, dev_priv->chv_phy_control);
}

#undef BITS_SET

static void chv_dpio_cmn_power_well_enable(struct drm_i915_private *dev_priv,
					   struct i915_power_well *power_well)
{
	enum dpio_phy phy;
	enum pipe pipe;
	uint32_t tmp;

	WARN_ON_ONCE(power_well->data != PUNIT_POWER_WELL_DPIO_CMN_BC &&
		     power_well->data != PUNIT_POWER_WELL_DPIO_CMN_D);

	if (power_well->data == PUNIT_POWER_WELL_DPIO_CMN_BC) {
		pipe = PIPE_A;
		phy = DPIO_PHY0;
	} else {
		pipe = PIPE_C;
		phy = DPIO_PHY1;
	}

	/* since ref/cri clock was enabled */
	udelay(1); /* >10ns for cmnreset, >0ns for sidereset */
	vlv_set_power_well(dev_priv, power_well, true);

	/* Poll for phypwrgood signal */
	if (wait_for(I915_READ(DISPLAY_PHY_STATUS) & PHY_POWERGOOD(phy), 1))
		DRM_ERROR("Display PHY %d is not power up\n", phy);

	mutex_lock(&dev_priv->sb_lock);

	/* Enable dynamic power down */
	tmp = vlv_dpio_read(dev_priv, pipe, CHV_CMN_DW28);
	tmp |= DPIO_DYNPWRDOWNEN_CH0 | DPIO_CL1POWERDOWNEN |
		DPIO_SUS_CLK_CONFIG_GATE_CLKREQ;
	vlv_dpio_write(dev_priv, pipe, CHV_CMN_DW28, tmp);

	if (power_well->data == PUNIT_POWER_WELL_DPIO_CMN_BC) {
		tmp = vlv_dpio_read(dev_priv, pipe, _CHV_CMN_DW6_CH1);
		tmp |= DPIO_DYNPWRDOWNEN_CH1;
		vlv_dpio_write(dev_priv, pipe, _CHV_CMN_DW6_CH1, tmp);
	} else {
		/*
		 * Force the non-existing CL2 off. BXT does this
		 * too, so maybe it saves some power even though
		 * CL2 doesn't exist?
		 */
		tmp = vlv_dpio_read(dev_priv, pipe, CHV_CMN_DW30);
		tmp |= DPIO_CL2_LDOFUSE_PWRENB;
		vlv_dpio_write(dev_priv, pipe, CHV_CMN_DW30, tmp);
	}

	mutex_unlock(&dev_priv->sb_lock);

	dev_priv->chv_phy_control |= PHY_COM_LANE_RESET_DEASSERT(phy);
	I915_WRITE(DISPLAY_PHY_CONTROL, dev_priv->chv_phy_control);

	DRM_DEBUG_KMS("Enabled DPIO PHY%d (PHY_CONTROL=0x%08x)\n",
		      phy, dev_priv->chv_phy_control);

	assert_chv_phy_status(dev_priv);
}

static void chv_dpio_cmn_power_well_disable(struct drm_i915_private *dev_priv,
					    struct i915_power_well *power_well)
{
	enum dpio_phy phy;

	WARN_ON_ONCE(power_well->data != PUNIT_POWER_WELL_DPIO_CMN_BC &&
		     power_well->data != PUNIT_POWER_WELL_DPIO_CMN_D);

	if (power_well->data == PUNIT_POWER_WELL_DPIO_CMN_BC) {
		phy = DPIO_PHY0;
		assert_pll_disabled(dev_priv, PIPE_A);
		assert_pll_disabled(dev_priv, PIPE_B);
	} else {
		phy = DPIO_PHY1;
		assert_pll_disabled(dev_priv, PIPE_C);
	}

	dev_priv->chv_phy_control &= ~PHY_COM_LANE_RESET_DEASSERT(phy);
	I915_WRITE(DISPLAY_PHY_CONTROL, dev_priv->chv_phy_control);

	vlv_set_power_well(dev_priv, power_well, false);

	DRM_DEBUG_KMS("Disabled DPIO PHY%d (PHY_CONTROL=0x%08x)\n",
		      phy, dev_priv->chv_phy_control);

	/* PHY is fully reset now, so we can enable the PHY state asserts */
	dev_priv->chv_phy_assert[phy] = true;

	assert_chv_phy_status(dev_priv);
}

static void assert_chv_phy_powergate(struct drm_i915_private *dev_priv, enum dpio_phy phy,
				     enum dpio_channel ch, bool override, unsigned int mask)
{
	enum pipe pipe = phy == DPIO_PHY0 ? PIPE_A : PIPE_C;
	u32 reg, val, expected, actual;

	/*
	 * The BIOS can leave the PHY is some weird state
	 * where it doesn't fully power down some parts.
	 * Disable the asserts until the PHY has been fully
	 * reset (ie. the power well has been disabled at
	 * least once).
	 */
	if (!dev_priv->chv_phy_assert[phy])
		return;

	if (ch == DPIO_CH0)
		reg = _CHV_CMN_DW0_CH0;
	else
		reg = _CHV_CMN_DW6_CH1;

	mutex_lock(&dev_priv->sb_lock);
	val = vlv_dpio_read(dev_priv, pipe, reg);
	mutex_unlock(&dev_priv->sb_lock);

	/*
	 * This assumes !override is only used when the port is disabled.
	 * All lanes should power down even without the override when
	 * the port is disabled.
	 */
	if (!override || mask == 0xf) {
		expected = DPIO_ALLDL_POWERDOWN | DPIO_ANYDL_POWERDOWN;
		/*
		 * If CH1 common lane is not active anymore
		 * (eg. for pipe B DPLL) the entire channel will
		 * shut down, which causes the common lane registers
		 * to read as 0. That means we can't actually check
		 * the lane power down status bits, but as the entire
		 * register reads as 0 it's a good indication that the
		 * channel is indeed entirely powered down.
		 */
		if (ch == DPIO_CH1 && val == 0)
			expected = 0;
	} else if (mask != 0x0) {
		expected = DPIO_ANYDL_POWERDOWN;
	} else {
		expected = 0;
	}

	if (ch == DPIO_CH0)
		actual = val >> DPIO_ANYDL_POWERDOWN_SHIFT_CH0;
	else
		actual = val >> DPIO_ANYDL_POWERDOWN_SHIFT_CH1;
	actual &= DPIO_ALLDL_POWERDOWN | DPIO_ANYDL_POWERDOWN;

	WARN(actual != expected,
	     "Unexpected DPIO lane power down: all %d, any %d. Expected: all %d, any %d. (0x%x = 0x%08x)\n",
	     !!(actual & DPIO_ALLDL_POWERDOWN), !!(actual & DPIO_ANYDL_POWERDOWN),
	     !!(expected & DPIO_ALLDL_POWERDOWN), !!(expected & DPIO_ANYDL_POWERDOWN),
	     reg, val);
}

bool chv_phy_powergate_ch(struct drm_i915_private *dev_priv, enum dpio_phy phy,
			  enum dpio_channel ch, bool override)
{
	struct i915_power_domains *power_domains = &dev_priv->power_domains;
	bool was_override;

	mutex_lock(&power_domains->lock);

	was_override = dev_priv->chv_phy_control & PHY_CH_POWER_DOWN_OVRD_EN(phy, ch);

	if (override == was_override)
		goto out;

	if (override)
		dev_priv->chv_phy_control |= PHY_CH_POWER_DOWN_OVRD_EN(phy, ch);
	else
		dev_priv->chv_phy_control &= ~PHY_CH_POWER_DOWN_OVRD_EN(phy, ch);

	I915_WRITE(DISPLAY_PHY_CONTROL, dev_priv->chv_phy_control);

	DRM_DEBUG_KMS("Power gating DPIO PHY%d CH%d (DPIO_PHY_CONTROL=0x%08x)\n",
		      phy, ch, dev_priv->chv_phy_control);

	assert_chv_phy_status(dev_priv);

out:
	mutex_unlock(&power_domains->lock);

	return was_override;
}

void chv_phy_powergate_lanes(struct intel_encoder *encoder,
			     bool override, unsigned int mask)
{
	struct drm_i915_private *dev_priv = to_i915(encoder->base.dev);
	struct i915_power_domains *power_domains = &dev_priv->power_domains;
	enum dpio_phy phy = vlv_dport_to_phy(enc_to_dig_port(&encoder->base));
	enum dpio_channel ch = vlv_dport_to_channel(enc_to_dig_port(&encoder->base));

	mutex_lock(&power_domains->lock);

	dev_priv->chv_phy_control &= ~PHY_CH_POWER_DOWN_OVRD(0xf, phy, ch);
	dev_priv->chv_phy_control |= PHY_CH_POWER_DOWN_OVRD(mask, phy, ch);

	if (override)
		dev_priv->chv_phy_control |= PHY_CH_POWER_DOWN_OVRD_EN(phy, ch);
	else
		dev_priv->chv_phy_control &= ~PHY_CH_POWER_DOWN_OVRD_EN(phy, ch);

	I915_WRITE(DISPLAY_PHY_CONTROL, dev_priv->chv_phy_control);

	DRM_DEBUG_KMS("Power gating DPIO PHY%d CH%d lanes 0x%x (PHY_CONTROL=0x%08x)\n",
		      phy, ch, mask, dev_priv->chv_phy_control);

	assert_chv_phy_status(dev_priv);

	assert_chv_phy_powergate(dev_priv, phy, ch, override, mask);

	mutex_unlock(&power_domains->lock);
}

static bool chv_pipe_power_well_enabled(struct drm_i915_private *dev_priv,
					struct i915_power_well *power_well)
{
	enum pipe pipe = power_well->data;
	bool enabled;
	u32 state, ctrl;

	mutex_lock(&dev_priv->rps.hw_lock);

	state = vlv_punit_read(dev_priv, PUNIT_REG_DSPFREQ) & DP_SSS_MASK(pipe);
	/*
	 * We only ever set the power-on and power-gate states, anything
	 * else is unexpected.
	 */
	WARN_ON(state != DP_SSS_PWR_ON(pipe) && state != DP_SSS_PWR_GATE(pipe));
	enabled = state == DP_SSS_PWR_ON(pipe);

	/*
	 * A transient state at this point would mean some unexpected party
	 * is poking at the power controls too.
	 */
	ctrl = vlv_punit_read(dev_priv, PUNIT_REG_DSPFREQ) & DP_SSC_MASK(pipe);
	WARN_ON(ctrl << 16 != state);

	mutex_unlock(&dev_priv->rps.hw_lock);

	return enabled;
}

static void chv_set_pipe_power_well(struct drm_i915_private *dev_priv,
				    struct i915_power_well *power_well,
				    bool enable)
{
	enum pipe pipe = power_well->data;
	u32 state;
	u32 ctrl;

	state = enable ? DP_SSS_PWR_ON(pipe) : DP_SSS_PWR_GATE(pipe);

	mutex_lock(&dev_priv->rps.hw_lock);

#define COND \
	((vlv_punit_read(dev_priv, PUNIT_REG_DSPFREQ) & DP_SSS_MASK(pipe)) == state)

	if (COND)
		goto out;

	ctrl = vlv_punit_read(dev_priv, PUNIT_REG_DSPFREQ);
	ctrl &= ~DP_SSC_MASK(pipe);
	ctrl |= enable ? DP_SSC_PWR_ON(pipe) : DP_SSC_PWR_GATE(pipe);
	vlv_punit_write(dev_priv, PUNIT_REG_DSPFREQ, ctrl);

	if (wait_for(COND, 100))
		DRM_ERROR("timeout setting power well state %08x (%08x)\n",
			  state,
			  vlv_punit_read(dev_priv, PUNIT_REG_DSPFREQ));

#undef COND

out:
	mutex_unlock(&dev_priv->rps.hw_lock);
}

static void chv_pipe_power_well_sync_hw(struct drm_i915_private *dev_priv,
					struct i915_power_well *power_well)
{
	WARN_ON_ONCE(power_well->data != PIPE_A);

	chv_set_pipe_power_well(dev_priv, power_well, power_well->count > 0);
}

static void chv_pipe_power_well_enable(struct drm_i915_private *dev_priv,
				       struct i915_power_well *power_well)
{
	WARN_ON_ONCE(power_well->data != PIPE_A);

	chv_set_pipe_power_well(dev_priv, power_well, true);

	vlv_display_power_well_init(dev_priv);
}

static void chv_pipe_power_well_disable(struct drm_i915_private *dev_priv,
					struct i915_power_well *power_well)
{
	WARN_ON_ONCE(power_well->data != PIPE_A);

	vlv_display_power_well_deinit(dev_priv);

	chv_set_pipe_power_well(dev_priv, power_well, false);
}

/**
 * intel_display_power_get - grab a power domain reference
 * @dev_priv: i915 device instance
 * @domain: power domain to reference
 *
 * This function grabs a power domain reference for @domain and ensures that the
 * power domain and all its parents are powered up. Therefore users should only
 * grab a reference to the innermost power domain they need.
 *
 * Any power domain reference obtained by this function must have a symmetric
 * call to intel_display_power_put() to release the reference again.
 */
void intel_display_power_get(struct drm_i915_private *dev_priv,
			     enum intel_display_power_domain domain)
{
	struct i915_power_domains *power_domains;
	struct i915_power_well *power_well;
	int i;

	intel_runtime_pm_get(dev_priv);

	power_domains = &dev_priv->power_domains;

	mutex_lock(&power_domains->lock);

	for_each_power_well(i, power_well, BIT(domain), power_domains) {
		if (!power_well->count++)
			intel_power_well_enable(dev_priv, power_well);
	}

	power_domains->domain_use_count[domain]++;

	mutex_unlock(&power_domains->lock);
}

/**
 * intel_display_power_put - release a power domain reference
 * @dev_priv: i915 device instance
 * @domain: power domain to reference
 *
 * This function drops the power domain reference obtained by
 * intel_display_power_get() and might power down the corresponding hardware
 * block right away if this is the last reference.
 */
void intel_display_power_put(struct drm_i915_private *dev_priv,
			     enum intel_display_power_domain domain)
{
	struct i915_power_domains *power_domains;
	struct i915_power_well *power_well;
	int i;

	power_domains = &dev_priv->power_domains;

	mutex_lock(&power_domains->lock);

	WARN_ON(!power_domains->domain_use_count[domain]);
	power_domains->domain_use_count[domain]--;

	for_each_power_well_rev(i, power_well, BIT(domain), power_domains) {
		WARN_ON(!power_well->count);

		if (!--power_well->count && i915.disable_power_well)
			intel_power_well_disable(dev_priv, power_well);
	}

	mutex_unlock(&power_domains->lock);

	intel_runtime_pm_put(dev_priv);
}

#define HSW_ALWAYS_ON_POWER_DOMAINS (			\
	BIT(POWER_DOMAIN_PIPE_A) |			\
	BIT(POWER_DOMAIN_TRANSCODER_EDP) |		\
	BIT(POWER_DOMAIN_PORT_DDI_A_2_LANES) |		\
	BIT(POWER_DOMAIN_PORT_DDI_A_4_LANES) |		\
	BIT(POWER_DOMAIN_PORT_DDI_B_2_LANES) |		\
	BIT(POWER_DOMAIN_PORT_DDI_B_4_LANES) |		\
	BIT(POWER_DOMAIN_PORT_DDI_C_2_LANES) |		\
	BIT(POWER_DOMAIN_PORT_DDI_C_4_LANES) |		\
	BIT(POWER_DOMAIN_PORT_DDI_D_2_LANES) |		\
	BIT(POWER_DOMAIN_PORT_DDI_D_4_LANES) |		\
	BIT(POWER_DOMAIN_PORT_CRT) |			\
	BIT(POWER_DOMAIN_PLLS) |			\
	BIT(POWER_DOMAIN_AUX_A) |			\
	BIT(POWER_DOMAIN_AUX_B) |			\
	BIT(POWER_DOMAIN_AUX_C) |			\
	BIT(POWER_DOMAIN_AUX_D) |			\
	BIT(POWER_DOMAIN_GMBUS) |			\
	BIT(POWER_DOMAIN_INIT))
#define HSW_DISPLAY_POWER_DOMAINS (				\
	(POWER_DOMAIN_MASK & ~HSW_ALWAYS_ON_POWER_DOMAINS) |	\
	BIT(POWER_DOMAIN_INIT))

#define BDW_ALWAYS_ON_POWER_DOMAINS (			\
	HSW_ALWAYS_ON_POWER_DOMAINS |			\
	BIT(POWER_DOMAIN_PIPE_A_PANEL_FITTER))
#define BDW_DISPLAY_POWER_DOMAINS (				\
	(POWER_DOMAIN_MASK & ~BDW_ALWAYS_ON_POWER_DOMAINS) |	\
	BIT(POWER_DOMAIN_INIT))

#define VLV_ALWAYS_ON_POWER_DOMAINS	BIT(POWER_DOMAIN_INIT)
#define VLV_DISPLAY_POWER_DOMAINS	POWER_DOMAIN_MASK

#define VLV_DPIO_CMN_BC_POWER_DOMAINS (		\
	BIT(POWER_DOMAIN_PORT_DDI_B_2_LANES) |	\
	BIT(POWER_DOMAIN_PORT_DDI_B_4_LANES) |	\
	BIT(POWER_DOMAIN_PORT_DDI_C_2_LANES) |	\
	BIT(POWER_DOMAIN_PORT_DDI_C_4_LANES) |	\
	BIT(POWER_DOMAIN_PORT_CRT) |		\
	BIT(POWER_DOMAIN_AUX_B) |		\
	BIT(POWER_DOMAIN_AUX_C) |		\
	BIT(POWER_DOMAIN_INIT))

#define VLV_DPIO_TX_B_LANES_01_POWER_DOMAINS (	\
	BIT(POWER_DOMAIN_PORT_DDI_B_2_LANES) |	\
	BIT(POWER_DOMAIN_PORT_DDI_B_4_LANES) |	\
	BIT(POWER_DOMAIN_AUX_B) |		\
	BIT(POWER_DOMAIN_INIT))

#define VLV_DPIO_TX_B_LANES_23_POWER_DOMAINS (	\
	BIT(POWER_DOMAIN_PORT_DDI_B_4_LANES) |	\
	BIT(POWER_DOMAIN_AUX_B) |		\
	BIT(POWER_DOMAIN_INIT))

#define VLV_DPIO_TX_C_LANES_01_POWER_DOMAINS (	\
	BIT(POWER_DOMAIN_PORT_DDI_C_2_LANES) |	\
	BIT(POWER_DOMAIN_PORT_DDI_C_4_LANES) |	\
	BIT(POWER_DOMAIN_AUX_C) |		\
	BIT(POWER_DOMAIN_INIT))

#define VLV_DPIO_TX_C_LANES_23_POWER_DOMAINS (	\
	BIT(POWER_DOMAIN_PORT_DDI_C_4_LANES) |	\
	BIT(POWER_DOMAIN_AUX_C) |		\
	BIT(POWER_DOMAIN_INIT))

#define CHV_DPIO_CMN_BC_POWER_DOMAINS (		\
	BIT(POWER_DOMAIN_PORT_DDI_B_2_LANES) |	\
	BIT(POWER_DOMAIN_PORT_DDI_B_4_LANES) |	\
	BIT(POWER_DOMAIN_PORT_DDI_C_2_LANES) |	\
	BIT(POWER_DOMAIN_PORT_DDI_C_4_LANES) |	\
	BIT(POWER_DOMAIN_AUX_B) |		\
	BIT(POWER_DOMAIN_AUX_C) |		\
	BIT(POWER_DOMAIN_INIT))

#define CHV_DPIO_CMN_D_POWER_DOMAINS (		\
	BIT(POWER_DOMAIN_PORT_DDI_D_2_LANES) |	\
	BIT(POWER_DOMAIN_PORT_DDI_D_4_LANES) |	\
	BIT(POWER_DOMAIN_AUX_D) |		\
	BIT(POWER_DOMAIN_INIT))

static const struct i915_power_well_ops i9xx_always_on_power_well_ops = {
	.sync_hw = i9xx_always_on_power_well_noop,
	.enable = i9xx_always_on_power_well_noop,
	.disable = i9xx_always_on_power_well_noop,
	.is_enabled = i9xx_always_on_power_well_enabled,
};

static const struct i915_power_well_ops chv_pipe_power_well_ops = {
	.sync_hw = chv_pipe_power_well_sync_hw,
	.enable = chv_pipe_power_well_enable,
	.disable = chv_pipe_power_well_disable,
	.is_enabled = chv_pipe_power_well_enabled,
};

static const struct i915_power_well_ops chv_dpio_cmn_power_well_ops = {
	.sync_hw = vlv_power_well_sync_hw,
	.enable = chv_dpio_cmn_power_well_enable,
	.disable = chv_dpio_cmn_power_well_disable,
	.is_enabled = vlv_power_well_enabled,
};

static struct i915_power_well i9xx_always_on_power_well[] = {
	{
		.name = "always-on",
		.always_on = 1,
		.domains = POWER_DOMAIN_MASK,
		.ops = &i9xx_always_on_power_well_ops,
	},
};

static const struct i915_power_well_ops hsw_power_well_ops = {
	.sync_hw = hsw_power_well_sync_hw,
	.enable = hsw_power_well_enable,
	.disable = hsw_power_well_disable,
	.is_enabled = hsw_power_well_enabled,
};

static const struct i915_power_well_ops skl_power_well_ops = {
	.sync_hw = skl_power_well_sync_hw,
	.enable = skl_power_well_enable,
	.disable = skl_power_well_disable,
	.is_enabled = skl_power_well_enabled,
};

static struct i915_power_well hsw_power_wells[] = {
	{
		.name = "always-on",
		.always_on = 1,
		.domains = HSW_ALWAYS_ON_POWER_DOMAINS,
		.ops = &i9xx_always_on_power_well_ops,
	},
	{
		.name = "display",
		.domains = HSW_DISPLAY_POWER_DOMAINS,
		.ops = &hsw_power_well_ops,
	},
};

static struct i915_power_well bdw_power_wells[] = {
	{
		.name = "always-on",
		.always_on = 1,
		.domains = BDW_ALWAYS_ON_POWER_DOMAINS,
		.ops = &i9xx_always_on_power_well_ops,
	},
	{
		.name = "display",
		.domains = BDW_DISPLAY_POWER_DOMAINS,
		.ops = &hsw_power_well_ops,
	},
};

static const struct i915_power_well_ops vlv_display_power_well_ops = {
	.sync_hw = vlv_power_well_sync_hw,
	.enable = vlv_display_power_well_enable,
	.disable = vlv_display_power_well_disable,
	.is_enabled = vlv_power_well_enabled,
};

static const struct i915_power_well_ops vlv_dpio_cmn_power_well_ops = {
	.sync_hw = vlv_power_well_sync_hw,
	.enable = vlv_dpio_cmn_power_well_enable,
	.disable = vlv_dpio_cmn_power_well_disable,
	.is_enabled = vlv_power_well_enabled,
};

static const struct i915_power_well_ops vlv_dpio_power_well_ops = {
	.sync_hw = vlv_power_well_sync_hw,
	.enable = vlv_power_well_enable,
	.disable = vlv_power_well_disable,
	.is_enabled = vlv_power_well_enabled,
};

static struct i915_power_well vlv_power_wells[] = {
	{
		.name = "always-on",
		.always_on = 1,
		.domains = VLV_ALWAYS_ON_POWER_DOMAINS,
		.ops = &i9xx_always_on_power_well_ops,
	},
	{
		.name = "display",
		.domains = VLV_DISPLAY_POWER_DOMAINS,
		.data = PUNIT_POWER_WELL_DISP2D,
		.ops = &vlv_display_power_well_ops,
	},
	{
		.name = "dpio-tx-b-01",
		.domains = VLV_DPIO_TX_B_LANES_01_POWER_DOMAINS |
			   VLV_DPIO_TX_B_LANES_23_POWER_DOMAINS |
			   VLV_DPIO_TX_C_LANES_01_POWER_DOMAINS |
			   VLV_DPIO_TX_C_LANES_23_POWER_DOMAINS,
		.ops = &vlv_dpio_power_well_ops,
		.data = PUNIT_POWER_WELL_DPIO_TX_B_LANES_01,
	},
	{
		.name = "dpio-tx-b-23",
		.domains = VLV_DPIO_TX_B_LANES_01_POWER_DOMAINS |
			   VLV_DPIO_TX_B_LANES_23_POWER_DOMAINS |
			   VLV_DPIO_TX_C_LANES_01_POWER_DOMAINS |
			   VLV_DPIO_TX_C_LANES_23_POWER_DOMAINS,
		.ops = &vlv_dpio_power_well_ops,
		.data = PUNIT_POWER_WELL_DPIO_TX_B_LANES_23,
	},
	{
		.name = "dpio-tx-c-01",
		.domains = VLV_DPIO_TX_B_LANES_01_POWER_DOMAINS |
			   VLV_DPIO_TX_B_LANES_23_POWER_DOMAINS |
			   VLV_DPIO_TX_C_LANES_01_POWER_DOMAINS |
			   VLV_DPIO_TX_C_LANES_23_POWER_DOMAINS,
		.ops = &vlv_dpio_power_well_ops,
		.data = PUNIT_POWER_WELL_DPIO_TX_C_LANES_01,
	},
	{
		.name = "dpio-tx-c-23",
		.domains = VLV_DPIO_TX_B_LANES_01_POWER_DOMAINS |
			   VLV_DPIO_TX_B_LANES_23_POWER_DOMAINS |
			   VLV_DPIO_TX_C_LANES_01_POWER_DOMAINS |
			   VLV_DPIO_TX_C_LANES_23_POWER_DOMAINS,
		.ops = &vlv_dpio_power_well_ops,
		.data = PUNIT_POWER_WELL_DPIO_TX_C_LANES_23,
	},
	{
		.name = "dpio-common",
		.domains = VLV_DPIO_CMN_BC_POWER_DOMAINS,
		.data = PUNIT_POWER_WELL_DPIO_CMN_BC,
		.ops = &vlv_dpio_cmn_power_well_ops,
	},
};

static struct i915_power_well chv_power_wells[] = {
	{
		.name = "always-on",
		.always_on = 1,
		.domains = VLV_ALWAYS_ON_POWER_DOMAINS,
		.ops = &i9xx_always_on_power_well_ops,
	},
	{
		.name = "display",
		/*
		 * Pipe A power well is the new disp2d well. Pipe B and C
		 * power wells don't actually exist. Pipe A power well is
		 * required for any pipe to work.
		 */
		.domains = VLV_DISPLAY_POWER_DOMAINS,
		.data = PIPE_A,
		.ops = &chv_pipe_power_well_ops,
	},
	{
		.name = "dpio-common-bc",
		.domains = CHV_DPIO_CMN_BC_POWER_DOMAINS,
		.data = PUNIT_POWER_WELL_DPIO_CMN_BC,
		.ops = &chv_dpio_cmn_power_well_ops,
	},
	{
		.name = "dpio-common-d",
		.domains = CHV_DPIO_CMN_D_POWER_DOMAINS,
		.data = PUNIT_POWER_WELL_DPIO_CMN_D,
		.ops = &chv_dpio_cmn_power_well_ops,
	},
};

bool intel_display_power_well_is_enabled(struct drm_i915_private *dev_priv,
				    int power_well_id)
{
	struct i915_power_well *power_well;
	bool ret;

	power_well = lookup_power_well(dev_priv, power_well_id);
	ret = power_well->ops->is_enabled(dev_priv, power_well);

	return ret;
}

static struct i915_power_well skl_power_wells[] = {
	{
		.name = "always-on",
		.always_on = 1,
		.domains = SKL_DISPLAY_ALWAYS_ON_POWER_DOMAINS,
		.ops = &i9xx_always_on_power_well_ops,
	},
	{
		.name = "power well 1",
		.domains = SKL_DISPLAY_POWERWELL_1_POWER_DOMAINS,
		.ops = &skl_power_well_ops,
		.data = SKL_DISP_PW_1,
	},
	{
		.name = "MISC IO power well",
		.domains = SKL_DISPLAY_MISC_IO_POWER_DOMAINS,
		.ops = &skl_power_well_ops,
		.data = SKL_DISP_PW_MISC_IO,
	},
	{
		.name = "power well 2",
		.domains = SKL_DISPLAY_POWERWELL_2_POWER_DOMAINS,
		.ops = &skl_power_well_ops,
		.data = SKL_DISP_PW_2,
	},
	{
		.name = "DDI A/E power well",
		.domains = SKL_DISPLAY_DDI_A_E_POWER_DOMAINS,
		.ops = &skl_power_well_ops,
		.data = SKL_DISP_PW_DDI_A_E,
	},
	{
		.name = "DDI B power well",
		.domains = SKL_DISPLAY_DDI_B_POWER_DOMAINS,
		.ops = &skl_power_well_ops,
		.data = SKL_DISP_PW_DDI_B,
	},
	{
		.name = "DDI C power well",
		.domains = SKL_DISPLAY_DDI_C_POWER_DOMAINS,
		.ops = &skl_power_well_ops,
		.data = SKL_DISP_PW_DDI_C,
	},
	{
		.name = "DDI D power well",
		.domains = SKL_DISPLAY_DDI_D_POWER_DOMAINS,
		.ops = &skl_power_well_ops,
		.data = SKL_DISP_PW_DDI_D,
	},
};

static struct i915_power_well bxt_power_wells[] = {
	{
		.name = "always-on",
		.always_on = 1,
		.domains = BXT_DISPLAY_ALWAYS_ON_POWER_DOMAINS,
		.ops = &i9xx_always_on_power_well_ops,
	},
	{
		.name = "power well 1",
		.domains = BXT_DISPLAY_POWERWELL_1_POWER_DOMAINS,
		.ops = &skl_power_well_ops,
		.data = SKL_DISP_PW_1,
	},
	{
		.name = "power well 2",
		.domains = BXT_DISPLAY_POWERWELL_2_POWER_DOMAINS,
		.ops = &skl_power_well_ops,
		.data = SKL_DISP_PW_2,
	}
};

static int
sanitize_disable_power_well_option(const struct drm_i915_private *dev_priv,
				   int disable_power_well)
{
	if (disable_power_well >= 0)
		return !!disable_power_well;

	if (IS_SKYLAKE(dev_priv) || IS_KABYLAKE(dev_priv)) {
		DRM_DEBUG_KMS("Disabling display power well support\n");
		return 0;
	}

	return 1;
}

#define set_power_wells(power_domains, __power_wells) ({		\
	(power_domains)->power_wells = (__power_wells);			\
	(power_domains)->power_well_count = ARRAY_SIZE(__power_wells);	\
})

/**
 * intel_power_domains_init - initializes the power domain structures
 * @dev_priv: i915 device instance
 *
 * Initializes the power domain structures for @dev_priv depending upon the
 * supported platform.
 */
int intel_power_domains_init(struct drm_i915_private *dev_priv)
{
	struct i915_power_domains *power_domains = &dev_priv->power_domains;

	i915.disable_power_well = sanitize_disable_power_well_option(dev_priv,
						     i915.disable_power_well);

	BUILD_BUG_ON(POWER_DOMAIN_NUM > 31);

	rw_init(&power_domains->lock, "pdl");

	/*
	 * The enabling order will be from lower to higher indexed wells,
	 * the disabling order is reversed.
	 */
	if (IS_HASWELL(dev_priv->dev)) {
		set_power_wells(power_domains, hsw_power_wells);
	} else if (IS_BROADWELL(dev_priv->dev)) {
		set_power_wells(power_domains, bdw_power_wells);
	} else if (IS_SKYLAKE(dev_priv->dev) || IS_KABYLAKE(dev_priv->dev)) {
		set_power_wells(power_domains, skl_power_wells);
	} else if (IS_BROXTON(dev_priv->dev)) {
		set_power_wells(power_domains, bxt_power_wells);
	} else if (IS_CHERRYVIEW(dev_priv->dev)) {
		set_power_wells(power_domains, chv_power_wells);
	} else if (IS_VALLEYVIEW(dev_priv->dev)) {
		set_power_wells(power_domains, vlv_power_wells);
	} else {
		set_power_wells(power_domains, i9xx_always_on_power_well);
	}

	return 0;
}

static void intel_runtime_pm_disable(struct drm_i915_private *dev_priv)
{
#ifdef __linux__
	struct drm_device *dev = dev_priv->dev;
	struct device *device = &dev->pdev->dev;

	if (!HAS_RUNTIME_PM(dev))
		return;

	if (!intel_enable_rc6(dev))
		return;

	/* Make sure we're not suspended first. */
	pm_runtime_get_sync(device);
#endif
}

/**
 * intel_power_domains_fini - finalizes the power domain structures
 * @dev_priv: i915 device instance
 *
 * Finalizes the power domain structures for @dev_priv depending upon the
 * supported platform. This function also disables runtime pm and ensures that
 * the device stays powered up so that the driver can be reloaded.
 */
void intel_power_domains_fini(struct drm_i915_private *dev_priv)
{
	intel_runtime_pm_disable(dev_priv);

	/* The i915.ko module is still not prepared to be loaded when
	 * the power well is not enabled, so just enable it in case
	 * we're going to unload/reload. */
	intel_display_set_init_power(dev_priv, true);
}

static void intel_power_domains_resume(struct drm_i915_private *dev_priv)
{
	struct i915_power_domains *power_domains = &dev_priv->power_domains;
	struct i915_power_well *power_well;
	int i;

	mutex_lock(&power_domains->lock);
	for_each_power_well(i, power_well, POWER_DOMAIN_MASK, power_domains) {
		power_well->ops->sync_hw(dev_priv, power_well);
		power_well->hw_enabled = power_well->ops->is_enabled(dev_priv,
								     power_well);
	}
	mutex_unlock(&power_domains->lock);
}

static void chv_phy_control_init(struct drm_i915_private *dev_priv)
{
	struct i915_power_well *cmn_bc =
		lookup_power_well(dev_priv, PUNIT_POWER_WELL_DPIO_CMN_BC);
	struct i915_power_well *cmn_d =
		lookup_power_well(dev_priv, PUNIT_POWER_WELL_DPIO_CMN_D);

	/*
	 * DISPLAY_PHY_CONTROL can get corrupted if read. As a
	 * workaround never ever read DISPLAY_PHY_CONTROL, and
	 * instead maintain a shadow copy ourselves. Use the actual
	 * power well state and lane status to reconstruct the
	 * expected initial value.
	 */
	dev_priv->chv_phy_control =
		PHY_LDO_SEQ_DELAY(PHY_LDO_DELAY_600NS, DPIO_PHY0) |
		PHY_LDO_SEQ_DELAY(PHY_LDO_DELAY_600NS, DPIO_PHY1) |
		PHY_CH_POWER_MODE(PHY_CH_DEEP_PSR, DPIO_PHY0, DPIO_CH0) |
		PHY_CH_POWER_MODE(PHY_CH_DEEP_PSR, DPIO_PHY0, DPIO_CH1) |
		PHY_CH_POWER_MODE(PHY_CH_DEEP_PSR, DPIO_PHY1, DPIO_CH0);

	/*
	 * If all lanes are disabled we leave the override disabled
	 * with all power down bits cleared to match the state we
	 * would use after disabling the port. Otherwise enable the
	 * override and set the lane powerdown bits accding to the
	 * current lane status.
	 */
	if (cmn_bc->ops->is_enabled(dev_priv, cmn_bc)) {
		uint32_t status = I915_READ(DPLL(PIPE_A));
		unsigned int mask;

		mask = status & DPLL_PORTB_READY_MASK;
		if (mask == 0xf)
			mask = 0x0;
		else
			dev_priv->chv_phy_control |=
				PHY_CH_POWER_DOWN_OVRD_EN(DPIO_PHY0, DPIO_CH0);

		dev_priv->chv_phy_control |=
			PHY_CH_POWER_DOWN_OVRD(mask, DPIO_PHY0, DPIO_CH0);

		mask = (status & DPLL_PORTC_READY_MASK) >> 4;
		if (mask == 0xf)
			mask = 0x0;
		else
			dev_priv->chv_phy_control |=
				PHY_CH_POWER_DOWN_OVRD_EN(DPIO_PHY0, DPIO_CH1);

		dev_priv->chv_phy_control |=
			PHY_CH_POWER_DOWN_OVRD(mask, DPIO_PHY0, DPIO_CH1);

		dev_priv->chv_phy_control |= PHY_COM_LANE_RESET_DEASSERT(DPIO_PHY0);

		dev_priv->chv_phy_assert[DPIO_PHY0] = false;
	} else {
		dev_priv->chv_phy_assert[DPIO_PHY0] = true;
	}

	if (cmn_d->ops->is_enabled(dev_priv, cmn_d)) {
		uint32_t status = I915_READ(DPIO_PHY_STATUS);
		unsigned int mask;

		mask = status & DPLL_PORTD_READY_MASK;

		if (mask == 0xf)
			mask = 0x0;
		else
			dev_priv->chv_phy_control |=
				PHY_CH_POWER_DOWN_OVRD_EN(DPIO_PHY1, DPIO_CH0);

		dev_priv->chv_phy_control |=
			PHY_CH_POWER_DOWN_OVRD(mask, DPIO_PHY1, DPIO_CH0);

		dev_priv->chv_phy_control |= PHY_COM_LANE_RESET_DEASSERT(DPIO_PHY1);

		dev_priv->chv_phy_assert[DPIO_PHY1] = false;
	} else {
		dev_priv->chv_phy_assert[DPIO_PHY1] = true;
	}

	I915_WRITE(DISPLAY_PHY_CONTROL, dev_priv->chv_phy_control);

	DRM_DEBUG_KMS("Initial PHY_CONTROL=0x%08x\n",
		      dev_priv->chv_phy_control);
}

static void vlv_cmnlane_wa(struct drm_i915_private *dev_priv)
{
	struct i915_power_well *cmn =
		lookup_power_well(dev_priv, PUNIT_POWER_WELL_DPIO_CMN_BC);
	struct i915_power_well *disp2d =
		lookup_power_well(dev_priv, PUNIT_POWER_WELL_DISP2D);

	/* If the display might be already active skip this */
	if (cmn->ops->is_enabled(dev_priv, cmn) &&
	    disp2d->ops->is_enabled(dev_priv, disp2d) &&
	    I915_READ(DPIO_CTL) & DPIO_CMNRST)
		return;

	DRM_DEBUG_KMS("toggling display PHY side reset\n");

	/* cmnlane needs DPLL registers */
	disp2d->ops->enable(dev_priv, disp2d);

	/*
	 * From VLV2A0_DP_eDP_HDMI_DPIO_driver_vbios_notes_11.docx:
	 * Need to assert and de-assert PHY SB reset by gating the
	 * common lane power, then un-gating it.
	 * Simply ungating isn't enough to reset the PHY enough to get
	 * ports and lanes running.
	 */
	cmn->ops->disable(dev_priv, cmn);
}

/**
 * intel_power_domains_init_hw - initialize hardware power domain state
 * @dev_priv: i915 device instance
 *
 * This function initializes the hardware power domain state and enables all
 * power domains using intel_display_set_init_power().
 */
void intel_power_domains_init_hw(struct drm_i915_private *dev_priv)
{
	struct drm_device *dev = dev_priv->dev;
	struct i915_power_domains *power_domains = &dev_priv->power_domains;

	power_domains->initializing = true;

	if (IS_CHERRYVIEW(dev)) {
		mutex_lock(&power_domains->lock);
		chv_phy_control_init(dev_priv);
		mutex_unlock(&power_domains->lock);
	} else if (IS_VALLEYVIEW(dev)) {
		mutex_lock(&power_domains->lock);
		vlv_cmnlane_wa(dev_priv);
		mutex_unlock(&power_domains->lock);
	}

	/* For now, we need the power well to be always enabled. */
	intel_display_set_init_power(dev_priv, true);
	intel_power_domains_resume(dev_priv);
	power_domains->initializing = false;
}

/**
 * intel_runtime_pm_get - grab a runtime pm reference
 * @dev_priv: i915 device instance
 *
 * This function grabs a device-level runtime pm reference (mostly used for GEM
 * code to ensure the GTT or GT is on) and ensures that it is powered up.
 *
 * Any runtime pm reference obtained by this function must have a symmetric
 * call to intel_runtime_pm_put() to release the reference again.
 */
void intel_runtime_pm_get(struct drm_i915_private *dev_priv)
{
#ifdef __linux__
	struct drm_device *dev = dev_priv->dev;
	struct device *device = &dev->pdev->dev;

	if (!HAS_RUNTIME_PM(dev))
		return;

	pm_runtime_get_sync(device);
	WARN(dev_priv->pm.suspended, "Device still suspended.\n");
#endif
}

/**
 * intel_runtime_pm_get_noresume - grab a runtime pm reference
 * @dev_priv: i915 device instance
 *
 * This function grabs a device-level runtime pm reference (mostly used for GEM
 * code to ensure the GTT or GT is on).
 *
 * It will _not_ power up the device but instead only check that it's powered
 * on.  Therefore it is only valid to call this functions from contexts where
 * the device is known to be powered up and where trying to power it up would
 * result in hilarity and deadlocks. That pretty much means only the system
 * suspend/resume code where this is used to grab runtime pm references for
 * delayed setup down in work items.
 *
 * Any runtime pm reference obtained by this function must have a symmetric
 * call to intel_runtime_pm_put() to release the reference again.
 */
void intel_runtime_pm_get_noresume(struct drm_i915_private *dev_priv)
{
#ifdef __linux__
	struct drm_device *dev = dev_priv->dev;
	struct device *device = &dev->pdev->dev;

	if (!HAS_RUNTIME_PM(dev))
		return;

	WARN(dev_priv->pm.suspended, "Getting nosync-ref while suspended.\n");
	pm_runtime_get_noresume(device);
#endif
}

/**
 * intel_runtime_pm_put - release a runtime pm reference
 * @dev_priv: i915 device instance
 *
 * This function drops the device-level runtime pm reference obtained by
 * intel_runtime_pm_get() and might power down the corresponding
 * hardware block right away if this is the last reference.
 */
void intel_runtime_pm_put(struct drm_i915_private *dev_priv)
{
#ifdef __linux__
	struct drm_device *dev = dev_priv->dev;
	struct device *device = &dev->pdev->dev;

	if (!HAS_RUNTIME_PM(dev))
		return;

	pm_runtime_mark_last_busy(device);
	pm_runtime_put_autosuspend(device);
#endif
}

/**
 * intel_runtime_pm_enable - enable runtime pm
 * @dev_priv: i915 device instance
 *
 * This function enables runtime pm at the end of the driver load sequence.
 *
 * Note that this function does currently not enable runtime pm for the
 * subordinate display power domains. That is only done on the first modeset
 * using intel_display_set_init_power().
 */
void intel_runtime_pm_enable(struct drm_i915_private *dev_priv)
{
#ifdef __linux__
	struct drm_device *dev = dev_priv->dev;
	struct device *device = &dev->pdev->dev;

	if (!HAS_RUNTIME_PM(dev))
		return;

	/*
	 * RPM depends on RC6 to save restore the GT HW context, so make RC6 a
	 * requirement.
	 */
	if (!intel_enable_rc6(dev)) {
		DRM_INFO("RC6 disabled, disabling runtime PM support\n");
		return;
	}

	pm_runtime_set_autosuspend_delay(device, 10000); /* 10s */
	pm_runtime_mark_last_busy(device);
	pm_runtime_use_autosuspend(device);

	pm_runtime_put_autosuspend(device);
#endif
}

