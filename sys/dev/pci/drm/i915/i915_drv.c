/* i915_drv.c -- i830,i845,i855,i865,i915 driver -*- linux-c -*-
 */
/*
 *
 * Copyright 2003 Tungsten Graphics, Inc., Cedar Park, Texas.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL TUNGSTEN GRAPHICS AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#ifdef __linux__
#include <linux/device.h>
#include <linux/acpi.h>
#endif
#include <dev/pci/drm/drmP.h>
#include <dev/pci/drm/i915_drm.h>
#include <dev/pci/drm/i915_pciids.h>
#include "i915_drv.h"
#include "i915_trace.h"
#include "intel_drv.h"

#ifdef __linux__
#include <linux/console.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#endif
#include <dev/pci/drm/drm_crtc_helper.h>

static struct drm_driver driver;

#define GEN_DEFAULT_PIPEOFFSETS \
	.pipe_offsets = { PIPE_A_OFFSET, PIPE_B_OFFSET, \
			  PIPE_C_OFFSET, PIPE_EDP_OFFSET }, \
	.trans_offsets = { TRANSCODER_A_OFFSET, TRANSCODER_B_OFFSET, \
			   TRANSCODER_C_OFFSET, TRANSCODER_EDP_OFFSET }, \
	.palette_offsets = { PALETTE_A_OFFSET, PALETTE_B_OFFSET }

#define GEN_CHV_PIPEOFFSETS \
	.pipe_offsets = { PIPE_A_OFFSET, PIPE_B_OFFSET, \
			  CHV_PIPE_C_OFFSET }, \
	.trans_offsets = { TRANSCODER_A_OFFSET, TRANSCODER_B_OFFSET, \
			   CHV_TRANSCODER_C_OFFSET, }, \
	.palette_offsets = { PALETTE_A_OFFSET, PALETTE_B_OFFSET, \
			     CHV_PALETTE_C_OFFSET }

#define CURSOR_OFFSETS \
	.cursor_offsets = { CURSOR_A_OFFSET, CURSOR_B_OFFSET, CHV_CURSOR_C_OFFSET }

#define IVB_CURSOR_OFFSETS \
	.cursor_offsets = { CURSOR_A_OFFSET, IVB_CURSOR_B_OFFSET, IVB_CURSOR_C_OFFSET }

static const struct intel_device_info intel_i830_info = {
	.gen = 2, .is_mobile = 1, .cursor_needs_physical = 1, .num_pipes = 2,
	.has_overlay = 1, .overlay_needs_physical = 1,
	.ring_mask = RENDER_RING,
	GEN_DEFAULT_PIPEOFFSETS,
	CURSOR_OFFSETS,
};

static const struct intel_device_info intel_845g_info = {
	.gen = 2, .num_pipes = 1,
	.has_overlay = 1, .overlay_needs_physical = 1,
	.ring_mask = RENDER_RING,
	GEN_DEFAULT_PIPEOFFSETS,
	CURSOR_OFFSETS,
};

static const struct intel_device_info intel_i85x_info = {
	.gen = 2, .is_i85x = 1, .is_mobile = 1, .num_pipes = 2,
	.cursor_needs_physical = 1,
	.has_overlay = 1, .overlay_needs_physical = 1,
	.has_fbc = 1,
	.ring_mask = RENDER_RING,
	GEN_DEFAULT_PIPEOFFSETS,
	CURSOR_OFFSETS,
};

static const struct intel_device_info intel_i865g_info = {
	.gen = 2, .num_pipes = 1,
	.has_overlay = 1, .overlay_needs_physical = 1,
	.ring_mask = RENDER_RING,
	GEN_DEFAULT_PIPEOFFSETS,
	CURSOR_OFFSETS,
};

static const struct intel_device_info intel_i915g_info = {
	.gen = 3, .is_i915g = 1, .cursor_needs_physical = 1, .num_pipes = 2,
	.has_overlay = 1, .overlay_needs_physical = 1,
	.ring_mask = RENDER_RING,
	GEN_DEFAULT_PIPEOFFSETS,
	CURSOR_OFFSETS,
};
static const struct intel_device_info intel_i915gm_info = {
	.gen = 3, .is_mobile = 1, .num_pipes = 2,
	.cursor_needs_physical = 1,
	.has_overlay = 1, .overlay_needs_physical = 1,
	.supports_tv = 1,
	.has_fbc = 1,
	.ring_mask = RENDER_RING,
	GEN_DEFAULT_PIPEOFFSETS,
	CURSOR_OFFSETS,
};
static const struct intel_device_info intel_i945g_info = {
	.gen = 3, .has_hotplug = 1, .cursor_needs_physical = 1, .num_pipes = 2,
	.has_overlay = 1, .overlay_needs_physical = 1,
	.ring_mask = RENDER_RING,
	GEN_DEFAULT_PIPEOFFSETS,
	CURSOR_OFFSETS,
};
static const struct intel_device_info intel_i945gm_info = {
	.gen = 3, .is_i945gm = 1, .is_mobile = 1, .num_pipes = 2,
	.has_hotplug = 1, .cursor_needs_physical = 1,
	.has_overlay = 1, .overlay_needs_physical = 1,
	.supports_tv = 1,
	.has_fbc = 1,
	.ring_mask = RENDER_RING,
	GEN_DEFAULT_PIPEOFFSETS,
	CURSOR_OFFSETS,
};

static const struct intel_device_info intel_i965g_info = {
	.gen = 4, .is_broadwater = 1, .num_pipes = 2,
	.has_hotplug = 1,
	.has_overlay = 1,
	.ring_mask = RENDER_RING,
	GEN_DEFAULT_PIPEOFFSETS,
	CURSOR_OFFSETS,
};

static const struct intel_device_info intel_i965gm_info = {
	.gen = 4, .is_crestline = 1, .num_pipes = 2,
	.is_mobile = 1, .has_fbc = 1, .has_hotplug = 1,
	.has_overlay = 1,
	.supports_tv = 1,
	.ring_mask = RENDER_RING,
	GEN_DEFAULT_PIPEOFFSETS,
	CURSOR_OFFSETS,
};

static const struct intel_device_info intel_g33_info = {
	.gen = 3, .is_g33 = 1, .num_pipes = 2,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_overlay = 1,
	.ring_mask = RENDER_RING,
	GEN_DEFAULT_PIPEOFFSETS,
	CURSOR_OFFSETS,
};

static const struct intel_device_info intel_g45_info = {
	.gen = 4, .is_g4x = 1, .need_gfx_hws = 1, .num_pipes = 2,
	.has_pipe_cxsr = 1, .has_hotplug = 1,
	.ring_mask = RENDER_RING | BSD_RING,
	GEN_DEFAULT_PIPEOFFSETS,
	CURSOR_OFFSETS,
};

static const struct intel_device_info intel_gm45_info = {
	.gen = 4, .is_g4x = 1, .num_pipes = 2,
	.is_mobile = 1, .need_gfx_hws = 1, .has_fbc = 1,
	.has_pipe_cxsr = 1, .has_hotplug = 1,
	.supports_tv = 1,
	.ring_mask = RENDER_RING | BSD_RING,
	GEN_DEFAULT_PIPEOFFSETS,
	CURSOR_OFFSETS,
};

static const struct intel_device_info intel_pineview_info = {
	.gen = 3, .is_g33 = 1, .is_pineview = 1, .is_mobile = 1, .num_pipes = 2,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_overlay = 1,
	GEN_DEFAULT_PIPEOFFSETS,
	CURSOR_OFFSETS,
};

static const struct intel_device_info intel_ironlake_d_info = {
	.gen = 5, .num_pipes = 2,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.ring_mask = RENDER_RING | BSD_RING,
	GEN_DEFAULT_PIPEOFFSETS,
	CURSOR_OFFSETS,
};

static const struct intel_device_info intel_ironlake_m_info = {
	.gen = 5, .is_mobile = 1, .num_pipes = 2,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_fbc = 1,
	.ring_mask = RENDER_RING | BSD_RING,
	GEN_DEFAULT_PIPEOFFSETS,
	CURSOR_OFFSETS,
};

static const struct intel_device_info intel_sandybridge_d_info = {
	.gen = 6, .num_pipes = 2,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_fbc = 1,
	.ring_mask = RENDER_RING | BSD_RING | BLT_RING,
	.has_llc = 1,
	GEN_DEFAULT_PIPEOFFSETS,
	CURSOR_OFFSETS,
};

static const struct intel_device_info intel_sandybridge_m_info = {
	.gen = 6, .is_mobile = 1, .num_pipes = 2,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_fbc = 1,
	.ring_mask = RENDER_RING | BSD_RING | BLT_RING,
	.has_llc = 1,
	GEN_DEFAULT_PIPEOFFSETS,
	CURSOR_OFFSETS,
};

#define GEN7_FEATURES  \
	.gen = 7, .num_pipes = 3, \
	.need_gfx_hws = 1, .has_hotplug = 1, \
	.has_fbc = 1, \
	.ring_mask = RENDER_RING | BSD_RING | BLT_RING, \
	.has_llc = 1

static const struct intel_device_info intel_ivybridge_d_info = {
	GEN7_FEATURES,
	.is_ivybridge = 1,
	GEN_DEFAULT_PIPEOFFSETS,
	IVB_CURSOR_OFFSETS,
};

static const struct intel_device_info intel_ivybridge_m_info = {
	GEN7_FEATURES,
	.is_ivybridge = 1,
	.is_mobile = 1,
	GEN_DEFAULT_PIPEOFFSETS,
	IVB_CURSOR_OFFSETS,
};

static const struct intel_device_info intel_ivybridge_q_info = {
	GEN7_FEATURES,
	.is_ivybridge = 1,
	.num_pipes = 0, /* legal, last one wins */
	GEN_DEFAULT_PIPEOFFSETS,
	IVB_CURSOR_OFFSETS,
};

static const struct intel_device_info intel_valleyview_m_info = {
	GEN7_FEATURES,
	.is_mobile = 1,
	.num_pipes = 2,
	.is_valleyview = 1,
	.display_mmio_offset = VLV_DISPLAY_BASE,
	.has_fbc = 0, /* legal, last one wins */
	.has_llc = 0, /* legal, last one wins */
	GEN_DEFAULT_PIPEOFFSETS,
	CURSOR_OFFSETS,
};

static const struct intel_device_info intel_valleyview_d_info = {
	GEN7_FEATURES,
	.num_pipes = 2,
	.is_valleyview = 1,
	.display_mmio_offset = VLV_DISPLAY_BASE,
	.has_fbc = 0, /* legal, last one wins */
	.has_llc = 0, /* legal, last one wins */
	GEN_DEFAULT_PIPEOFFSETS,
	CURSOR_OFFSETS,
};

static const struct intel_device_info intel_haswell_d_info = {
	GEN7_FEATURES,
	.is_haswell = 1,
	.has_ddi = 1,
	.has_fpga_dbg = 1,
	.ring_mask = RENDER_RING | BSD_RING | BLT_RING | VEBOX_RING,
	GEN_DEFAULT_PIPEOFFSETS,
	IVB_CURSOR_OFFSETS,
};

static const struct intel_device_info intel_haswell_m_info = {
	GEN7_FEATURES,
	.is_haswell = 1,
	.is_mobile = 1,
	.has_ddi = 1,
	.has_fpga_dbg = 1,
	.ring_mask = RENDER_RING | BSD_RING | BLT_RING | VEBOX_RING,
	GEN_DEFAULT_PIPEOFFSETS,
	IVB_CURSOR_OFFSETS,
};

static const struct intel_device_info intel_broadwell_d_info = {
	.gen = 8, .num_pipes = 3,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.ring_mask = RENDER_RING | BSD_RING | BLT_RING | VEBOX_RING,
	.has_llc = 1,
	.has_ddi = 1,
	.has_fpga_dbg = 1,
	.has_fbc = 1,
	GEN_DEFAULT_PIPEOFFSETS,
	IVB_CURSOR_OFFSETS,
};

static const struct intel_device_info intel_broadwell_m_info = {
	.gen = 8, .is_mobile = 1, .num_pipes = 3,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.ring_mask = RENDER_RING | BSD_RING | BLT_RING | VEBOX_RING,
	.has_llc = 1,
	.has_ddi = 1,
	.has_fpga_dbg = 1,
	.has_fbc = 1,
	GEN_DEFAULT_PIPEOFFSETS,
	IVB_CURSOR_OFFSETS,
};

static const struct intel_device_info intel_broadwell_gt3d_info = {
	.gen = 8, .num_pipes = 3,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.ring_mask = RENDER_RING | BSD_RING | BLT_RING | VEBOX_RING | BSD2_RING,
	.has_llc = 1,
	.has_ddi = 1,
	.has_fpga_dbg = 1,
	.has_fbc = 1,
	GEN_DEFAULT_PIPEOFFSETS,
	IVB_CURSOR_OFFSETS,
};

static const struct intel_device_info intel_broadwell_gt3m_info = {
	.gen = 8, .is_mobile = 1, .num_pipes = 3,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.ring_mask = RENDER_RING | BSD_RING | BLT_RING | VEBOX_RING | BSD2_RING,
	.has_llc = 1,
	.has_ddi = 1,
	.has_fpga_dbg = 1,
	.has_fbc = 1,
	GEN_DEFAULT_PIPEOFFSETS,
	IVB_CURSOR_OFFSETS,
};

static const struct intel_device_info intel_cherryview_info = {
	.gen = 8, .num_pipes = 3,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.ring_mask = RENDER_RING | BSD_RING | BLT_RING | VEBOX_RING,
	.is_valleyview = 1,
	.display_mmio_offset = VLV_DISPLAY_BASE,
	GEN_CHV_PIPEOFFSETS,
	CURSOR_OFFSETS,
};

static const struct intel_device_info intel_skylake_info = {
	.is_skylake = 1,
	.gen = 9, .num_pipes = 3,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.ring_mask = RENDER_RING | BSD_RING | BLT_RING | VEBOX_RING,
	.has_llc = 1,
	.has_ddi = 1,
	.has_fpga_dbg = 1,
	.has_fbc = 1,
	GEN_DEFAULT_PIPEOFFSETS,
	IVB_CURSOR_OFFSETS,
};

static const struct intel_device_info intel_skylake_gt3_info = {
	.is_skylake = 1,
	.gen = 9, .num_pipes = 3,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.ring_mask = RENDER_RING | BSD_RING | BLT_RING | VEBOX_RING | BSD2_RING,
	.has_llc = 1,
	.has_ddi = 1,
	.has_fpga_dbg = 1,
	.has_fbc = 1,
	GEN_DEFAULT_PIPEOFFSETS,
	IVB_CURSOR_OFFSETS,
};

static const struct intel_device_info intel_broxton_info = {
	.is_preliminary = 1,
	.is_broxton = 1,
	.gen = 9,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.ring_mask = RENDER_RING | BSD_RING | BLT_RING | VEBOX_RING,
	.num_pipes = 3,
	.has_ddi = 1,
	.has_fpga_dbg = 1,
	.has_fbc = 1,
	GEN_DEFAULT_PIPEOFFSETS,
	IVB_CURSOR_OFFSETS,
};

static const struct intel_device_info intel_kabylake_info = {
	.is_kabylake = 1,
	.gen = 9,
	.num_pipes = 3,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.ring_mask = RENDER_RING | BSD_RING | BLT_RING | VEBOX_RING,
	.has_llc = 1,
	.has_ddi = 1,
	.has_fpga_dbg = 1,
	.has_fbc = 1,
	GEN_DEFAULT_PIPEOFFSETS,
	IVB_CURSOR_OFFSETS,
};

static const struct intel_device_info intel_kabylake_gt3_info = {
	.is_kabylake = 1,
	.gen = 9,
	.num_pipes = 3,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.ring_mask = RENDER_RING | BSD_RING | BLT_RING | VEBOX_RING | BSD2_RING,
	.has_llc = 1,
	.has_ddi = 1,
	.has_fpga_dbg = 1,
	.has_fbc = 1,
	GEN_DEFAULT_PIPEOFFSETS,
	IVB_CURSOR_OFFSETS,
};

/*
 * Make sure any device matches here are from most specific to most
 * general.  For example, since the Quanta match is based on the subsystem
 * and subvendor IDs, we need it to come before the more general IVB
 * PCI ID matches, otherwise we'll use the wrong info struct above.
 */
#define INTEL_PCI_IDS \
	INTEL_I830_IDS(&intel_i830_info),	\
	INTEL_I845G_IDS(&intel_845g_info),	\
	INTEL_I85X_IDS(&intel_i85x_info),	\
	INTEL_I865G_IDS(&intel_i865g_info),	\
	INTEL_I915G_IDS(&intel_i915g_info),	\
	INTEL_I915GM_IDS(&intel_i915gm_info),	\
	INTEL_I945G_IDS(&intel_i945g_info),	\
	INTEL_I945GM_IDS(&intel_i945gm_info),	\
	INTEL_I965G_IDS(&intel_i965g_info),	\
	INTEL_G33_IDS(&intel_g33_info),		\
	INTEL_I965GM_IDS(&intel_i965gm_info),	\
	INTEL_GM45_IDS(&intel_gm45_info), 	\
	INTEL_G45_IDS(&intel_g45_info), 	\
	INTEL_PINEVIEW_IDS(&intel_pineview_info),	\
	INTEL_IRONLAKE_D_IDS(&intel_ironlake_d_info),	\
	INTEL_IRONLAKE_M_IDS(&intel_ironlake_m_info),	\
	INTEL_SNB_D_IDS(&intel_sandybridge_d_info),	\
	INTEL_SNB_M_IDS(&intel_sandybridge_m_info),	\
	INTEL_IVB_Q_IDS(&intel_ivybridge_q_info), /* must be first IVB */ \
	INTEL_IVB_M_IDS(&intel_ivybridge_m_info),	\
	INTEL_IVB_D_IDS(&intel_ivybridge_d_info),	\
	INTEL_HSW_D_IDS(&intel_haswell_d_info), \
	INTEL_HSW_M_IDS(&intel_haswell_m_info), \
	INTEL_VLV_M_IDS(&intel_valleyview_m_info),	\
	INTEL_VLV_D_IDS(&intel_valleyview_d_info),	\
	INTEL_BDW_GT12M_IDS(&intel_broadwell_m_info),	\
	INTEL_BDW_GT12D_IDS(&intel_broadwell_d_info),	\
	INTEL_BDW_GT3M_IDS(&intel_broadwell_gt3m_info),	\
	INTEL_BDW_GT3D_IDS(&intel_broadwell_gt3d_info), \
	INTEL_CHV_IDS(&intel_cherryview_info),	\
	INTEL_SKL_GT1_IDS(&intel_skylake_info),	\
	INTEL_SKL_GT2_IDS(&intel_skylake_info),	\
	INTEL_SKL_GT3_IDS(&intel_skylake_gt3_info),	\
	INTEL_SKL_GT4_IDS(&intel_skylake_gt3_info),	\
	INTEL_BXT_IDS(&intel_broxton_info),		\
	INTEL_KBL_GT1_IDS(&intel_kabylake_info),	\
	INTEL_KBL_GT2_IDS(&intel_kabylake_info),	\
	INTEL_KBL_GT3_IDS(&intel_kabylake_gt3_info),	\
	INTEL_KBL_GT4_IDS(&intel_kabylake_gt3_info)

static const struct drm_pcidev pciidlist[] = {		/* aka */
	INTEL_PCI_IDS,
	{0, 0, 0}
};

MODULE_DEVICE_TABLE(pci, pciidlist);

#ifdef notyet
static enum intel_pch intel_virt_detect_pch(struct drm_device *dev)
{
	enum intel_pch ret = PCH_NOP;

	/*
	 * In a virtualized passthrough environment we can be in a
	 * setup where the ISA bridge is not able to be passed through.
	 * In this case, a south bridge can be emulated and we have to
	 * make an educated guess as to which PCH is really there.
	 */

	if (IS_GEN5(dev)) {
		ret = PCH_IBX;
		DRM_DEBUG_KMS("Assuming Ibex Peak PCH\n");
	} else if (IS_GEN6(dev) || IS_IVYBRIDGE(dev)) {
		ret = PCH_CPT;
		DRM_DEBUG_KMS("Assuming CouarPoint PCH\n");
	} else if (IS_HASWELL(dev) || IS_BROADWELL(dev)) {
		ret = PCH_LPT;
		DRM_DEBUG_KMS("Assuming LynxPoint PCH\n");
	} else if (IS_SKYLAKE(dev) || IS_KABYLAKE(dev)) {
		ret = PCH_SPT;
		DRM_DEBUG_KMS("Assuming SunrisePoint PCH\n");
	}

	return ret;
}
#endif

static int
intel_pch_match(struct pci_attach_args *pa)
{
	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_INTEL &&
	    PCI_CLASS(pa->pa_class) == PCI_CLASS_BRIDGE &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_BRIDGE_ISA)
		return (1);
	return (0);
}

void intel_detect_pch(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct pci_attach_args pa;

	/* In all current cases, num_pipes is equivalent to the PCH_NOP setting
	 * (which really amounts to a PCH but no South Display).
	 */
	if (INTEL_INFO(dev)->num_pipes == 0) {
		dev_priv->pch_type = PCH_NOP;
		return;
	}

	/*
	 * The reason to probe ISA bridge instead of Dev31:Fun0 is to
	 * make graphics device passthrough work easy for VMM, that only
	 * need to expose ISA bridge to let driver know the real hardware
	 * underneath. This is a requirement from virtualization team.
	 *
	 * In some virtualized environments (e.g. XEN), there is irrelevant
	 * ISA bridge in the system. To work reliably, we should scan trhough
	 * all the ISA bridge devices and check for the first match, instead
	 * of only checking the first one.
	 */
	if (pci_find_device(&pa, intel_pch_match)) {
		if (PCI_VENDOR(pa.pa_id) == PCI_VENDOR_ID_INTEL) {
			unsigned short id = PCI_PRODUCT(pa.pa_id) & INTEL_PCH_DEVICE_ID_MASK;
			dev_priv->pch_id = id;

			if (id == INTEL_PCH_IBX_DEVICE_ID_TYPE) {
				dev_priv->pch_type = PCH_IBX;
				DRM_DEBUG_KMS("Found Ibex Peak PCH\n");
				WARN_ON(!IS_GEN5(dev));
			} else if (id == INTEL_PCH_CPT_DEVICE_ID_TYPE) {
				dev_priv->pch_type = PCH_CPT;
				DRM_DEBUG_KMS("Found CougarPoint PCH\n");
				WARN_ON(!(IS_GEN6(dev) || IS_IVYBRIDGE(dev)));
			} else if (id == INTEL_PCH_PPT_DEVICE_ID_TYPE) {
				/* PantherPoint is CPT compatible */
				dev_priv->pch_type = PCH_CPT;
				DRM_DEBUG_KMS("Found PantherPoint PCH\n");
				WARN_ON(!(IS_GEN6(dev) || IS_IVYBRIDGE(dev)));
			} else if (id == INTEL_PCH_LPT_DEVICE_ID_TYPE) {
				dev_priv->pch_type = PCH_LPT;
				DRM_DEBUG_KMS("Found LynxPoint PCH\n");
				WARN_ON(!IS_HASWELL(dev) && !IS_BROADWELL(dev));
				WARN_ON(IS_HSW_ULT(dev) || IS_BDW_ULT(dev));
			} else if (id == INTEL_PCH_LPT_LP_DEVICE_ID_TYPE) {
				dev_priv->pch_type = PCH_LPT;
				DRM_DEBUG_KMS("Found LynxPoint LP PCH\n");
				WARN_ON(!IS_HASWELL(dev) && !IS_BROADWELL(dev));
				WARN_ON(!IS_HSW_ULT(dev) && !IS_BDW_ULT(dev));
			} else if (id == INTEL_PCH_SPT_DEVICE_ID_TYPE) {
				dev_priv->pch_type = PCH_SPT;
				DRM_DEBUG_KMS("Found SunrisePoint PCH\n");
				WARN_ON(!IS_SKYLAKE(dev) &&
					!IS_KABYLAKE(dev));
			} else if (id == INTEL_PCH_SPT_LP_DEVICE_ID_TYPE) {
				dev_priv->pch_type = PCH_SPT;
				DRM_DEBUG_KMS("Found SunrisePoint LP PCH\n");
				WARN_ON(!IS_SKYLAKE(dev) &&
					!IS_KABYLAKE(dev));
			} else if (id == INTEL_PCH_KBP_DEVICE_ID_TYPE) {
				dev_priv->pch_type = PCH_KBP;
				DRM_DEBUG_KMS("Found KabyPoint PCH\n");
				WARN_ON(!IS_KABYLAKE(dev));
#ifdef notyet
			} else if ((id == INTEL_PCH_P2X_DEVICE_ID_TYPE) ||
				   ((id == INTEL_PCH_QEMU_DEVICE_ID_TYPE) &&
				    pch->subsystem_vendor == 0x1af4 &&
				    pch->subsystem_device == 0x1100)) {
				dev_priv->pch_type = intel_virt_detect_pch(dev);
#endif
			}
		}
	} else
		DRM_DEBUG_KMS("No PCH found.\n");
}

bool i915_semaphore_is_enabled(struct drm_device *dev)
{
	if (INTEL_INFO(dev)->gen < 6)
		return false;

	if (i915.semaphores >= 0)
		return i915.semaphores;

	/* TODO: make semaphores and Execlists play nicely together */
	if (i915.enable_execlists)
		return false;

	/* Until we get further testing... */
	if (IS_GEN8(dev))
		return false;

#ifdef CONFIG_INTEL_IOMMU
	/* Enable semaphores on SNB when IO remapping is off */
	if (INTEL_INFO(dev)->gen == 6 && intel_iommu_gfx_mapped)
		return false;
#endif

	return true;
}

void i915_firmware_load_error_print(const char *fw_path, int err)
{
	DRM_ERROR("failed to load firmware %s (%d)\n", fw_path, err);

	/*
	 * If the reason is not known assume -ENOENT since that's the most
	 * usual failure mode.
	 */
	if (!err)
		err = -ENOENT;

	if (!(IS_BUILTIN(CONFIG_DRM_I915) && err == -ENOENT))
		return;

	DRM_ERROR(
	  "The driver is built-in, so to load the firmware you need to\n"
	  "include it either in the kernel (see CONFIG_EXTRA_FIRMWARE) or\n"
	  "in your initrd/initramfs image.\n");
}

static void intel_suspend_encoders(struct drm_i915_private *dev_priv)
{
	struct drm_device *dev = dev_priv->dev;
	struct drm_encoder *encoder;

	drm_modeset_lock_all(dev);
	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {
		struct intel_encoder *intel_encoder = to_intel_encoder(encoder);

		if (intel_encoder->suspend)
			intel_encoder->suspend(intel_encoder);
	}
	drm_modeset_unlock_all(dev);
}

static int intel_suspend_complete(struct drm_i915_private *dev_priv);
static int vlv_resume_prepare(struct drm_i915_private *dev_priv,
			      bool rpm_resume);
static int skl_resume_prepare(struct drm_i915_private *dev_priv);
static int bxt_resume_prepare(struct drm_i915_private *dev_priv);


static int i915_drm_suspend(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	pci_power_t opregion_target_state;
	int error;

	/* ignore lid events during suspend */
	mutex_lock(&dev_priv->modeset_restore_lock);
	dev_priv->modeset_restore = MODESET_SUSPENDED;
	mutex_unlock(&dev_priv->modeset_restore_lock);

	/* We do a lot of poking in a lot of registers, make sure they work
	 * properly. */
	intel_display_set_init_power(dev_priv, true);

	drm_kms_helper_poll_disable(dev);

	pci_save_state(dev->pdev);

	error = i915_gem_suspend(dev);
	if (error) {
		dev_err(&dev->pdev->dev,
			"GEM idle failed, resume might fail\n");
		return error;
	}

#ifdef notyet
	intel_guc_suspend(dev);
#endif

	intel_suspend_gt_powersave(dev);

	/*
	 * Disable CRTCs directly since we want to preserve sw state
	 * for _thaw. Also, power gate the CRTC power wells.
	 */
	drm_modeset_lock_all(dev);
	intel_display_suspend(dev);
	drm_modeset_unlock_all(dev);

	intel_dp_mst_suspend(dev);

	intel_runtime_pm_disable_interrupts(dev_priv);
	intel_hpd_cancel_work(dev_priv);

	intel_suspend_encoders(dev_priv);

	intel_suspend_hw(dev);

	i915_gem_suspend_gtt_mappings(dev);

	i915_save_state(dev);

	opregion_target_state = PCI_D3cold;
#if IS_ENABLED(CONFIG_ACPI_SLEEP)
	if (acpi_target_system_state() < ACPI_STATE_S3)
		opregion_target_state = PCI_D1;
#endif
	intel_opregion_notify_adapter(dev, opregion_target_state);

	intel_uncore_forcewake_reset(dev, false);
	intel_opregion_fini(dev);

#ifdef __linux__
	intel_fbdev_set_suspend(dev, FBINFO_STATE_SUSPENDED, true);
#endif

	dev_priv->suspend_count++;

	intel_display_set_init_power(dev_priv, false);

	return 0;
}

static int i915_drm_suspend_late(struct drm_device *drm_dev, bool hibernation)
{
	struct drm_i915_private *dev_priv = drm_dev->dev_private;
	int ret;

	ret = intel_suspend_complete(dev_priv);

	if (ret) {
		DRM_ERROR("Suspend complete failed: %d\n", ret);

		return ret;
	}

	pci_disable_device(drm_dev->pdev);
#ifdef notyet
	/*
	 * During hibernation on some platforms the BIOS may try to access
	 * the device even though it's already in D3 and hang the machine. So
	 * leave the device in D0 on those platforms and hope the BIOS will
	 * power down the device properly. The issue was seen on multiple old
	 * GENs with different BIOS vendors, so having an explicit blacklist
	 * is inpractical; apply the workaround on everything pre GEN6. The
	 * platforms where the issue was seen:
	 * Lenovo Thinkpad X301, X61s, X60, T60, X41
	 * Fujitsu FSC S7110
	 * Acer Aspire 1830T
	 */
	if (!(hibernation && INTEL_INFO(dev_priv)->gen < 6))
		pci_set_power_state(drm_dev->pdev, PCI_D3hot);
#endif

	return 0;
}

#ifdef __linux__
int i915_suspend_switcheroo(struct drm_device *dev, pm_message_t state)
{
	int error;

	if (!dev || !dev->dev_private) {
		DRM_ERROR("dev: %p\n", dev);
		DRM_ERROR("DRM not initialized, aborting suspend.\n");
		return -ENODEV;
	}

	if (WARN_ON_ONCE(state.event != PM_EVENT_SUSPEND &&
			 state.event != PM_EVENT_FREEZE))
		return -EINVAL;

	if (dev->switch_power_state == DRM_SWITCH_POWER_OFF)
		return 0;

	error = i915_drm_suspend(dev);
	if (error)
		return error;

	return i915_drm_suspend_late(dev, false);
}
#endif

static int i915_drm_resume(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	mutex_lock(&dev->struct_mutex);
	i915_gem_restore_gtt_mappings(dev);
	mutex_unlock(&dev->struct_mutex);

	i915_restore_state(dev);
	intel_opregion_setup(dev);

	intel_init_pch_refclk(dev);
	drm_mode_config_reset(dev);

	/*
	 * Interrupts have to be enabled before any batches are run. If not the
	 * GPU will hang. i915_gem_init_hw() will initiate batches to
	 * update/restore the context.
	 *
	 * Modeset enabling in intel_modeset_init_hw() also needs working
	 * interrupts.
	 */
	intel_runtime_pm_enable_interrupts(dev_priv);

	mutex_lock(&dev->struct_mutex);
	if (i915_gem_init_hw(dev)) {
		DRM_ERROR("failed to re-initialize GPU, declaring wedged!\n");
			atomic_or(I915_WEDGED, &dev_priv->gpu_error.reset_counter);
	}
	mutex_unlock(&dev->struct_mutex);

#ifdef notyet
	intel_guc_resume(dev);
#endif

	intel_modeset_init_hw(dev);

	spin_lock_irq(&dev_priv->irq_lock);
	if (dev_priv->display.hpd_irq_setup)
		dev_priv->display.hpd_irq_setup(dev);
	spin_unlock_irq(&dev_priv->irq_lock);

	drm_modeset_lock_all(dev);
	intel_display_resume(dev);
	drm_modeset_unlock_all(dev);

	intel_dp_mst_resume(dev);

	/*
	 * ... but also need to make sure that hotplug processing
	 * doesn't cause havoc. Like in the driver load code we don't
	 * bother with the tiny race here where we might loose hotplug
	 * notifications.
	 * */
	intel_hpd_init(dev_priv);
	/* Config may have changed between suspend and resume */
	drm_helper_hpd_irq_event(dev);

	intel_opregion_init(dev);

#ifdef __linux__
	intel_fbdev_set_suspend(dev, FBINFO_STATE_RUNNING, false);
#endif

	mutex_lock(&dev_priv->modeset_restore_lock);
	dev_priv->modeset_restore = MODESET_DONE;
	mutex_unlock(&dev_priv->modeset_restore_lock);

	intel_opregion_notify_adapter(dev, PCI_D0);

	drm_kms_helper_poll_enable(dev);

	return 0;
}

static int i915_drm_resume_early(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	int ret = 0;

	/*
	 * We have a resume ordering issue with the snd-hda driver also
	 * requiring our device to be power up. Due to the lack of a
	 * parent/child relationship we currently solve this with an early
	 * resume hook.
	 *
	 * FIXME: This should be solved with a special hdmi sink device or
	 * similar so that power domains can be employed.
	 */
	if (pci_enable_device(dev->pdev))
		return -EIO;

	pci_set_master(dev->pdev);

	if (IS_VALLEYVIEW(dev_priv))
		ret = vlv_resume_prepare(dev_priv, false);
	if (ret)
		DRM_ERROR("Resume prepare failed: %d, continuing anyway\n",
			  ret);

	intel_uncore_early_sanitize(dev, true);

	if (IS_BROXTON(dev))
		ret = bxt_resume_prepare(dev_priv);
	else if (IS_SKYLAKE(dev_priv) || IS_KABYLAKE(dev_priv))
		ret = skl_resume_prepare(dev_priv);
	else if (IS_HASWELL(dev_priv) || IS_BROADWELL(dev_priv))
		hsw_disable_pc8(dev_priv);

	intel_uncore_sanitize(dev);
	intel_power_domains_init_hw(dev_priv);

	return ret;
}

#ifdef __linux__
int i915_resume_switcheroo(struct drm_device *dev)
{
	int ret;

	if (dev->switch_power_state == DRM_SWITCH_POWER_OFF)
		return 0;

	ret = i915_drm_resume_early(dev);
	if (ret)
		return ret;

	return i915_drm_resume(dev);
}
#endif

/**
 * i915_reset - reset chip after a hang
 * @dev: drm device to reset
 *
 * Reset the chip.  Useful if a hang is detected. Returns zero on successful
 * reset or otherwise an error code.
 *
 * Procedure is fairly simple:
 *   - reset the chip using the reset reg
 *   - re-init context state
 *   - re-init hardware status page
 *   - re-init ring buffer
 *   - re-init interrupt state
 *   - re-init display
 */
int i915_reset(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	bool simulated;
	int ret;

	intel_reset_gt_powersave(dev);

	mutex_lock(&dev->struct_mutex);

	i915_gem_reset(dev);

	simulated = dev_priv->gpu_error.stop_rings != 0;

	ret = intel_gpu_reset(dev);

	/* Also reset the gpu hangman. */
	if (simulated) {
		DRM_INFO("Simulated gpu hang, resetting stop_rings\n");
		dev_priv->gpu_error.stop_rings = 0;
		if (ret == -ENODEV) {
			DRM_INFO("Reset not implemented, but ignoring "
				 "error for simulated gpu hangs\n");
			ret = 0;
		}
	}

	if (i915_stop_ring_allow_warn(dev_priv))
		pr_notice("drm/i915: Resetting chip after gpu hang\n");

	if (ret) {
		DRM_ERROR("Failed to reset chip: %i\n", ret);
		mutex_unlock(&dev->struct_mutex);
		return ret;
	}

	intel_overlay_reset(dev_priv);

	/* Ok, now get things going again... */

	/*
	 * Everything depends on having the GTT running, so we need to start
	 * there.  Fortunately we don't need to do this unless we reset the
	 * chip at a PCI level.
	 *
	 * Next we need to restore the context, but we don't use those
	 * yet either...
	 *
	 * Ring buffer needs to be re-initialized in the KMS case, or if X
	 * was running at the time of the reset (i.e. we weren't VT
	 * switched away).
	 */

	/* Used to prevent gem_check_wedged returning -EAGAIN during gpu reset */
	dev_priv->gpu_error.reload_in_reset = true;

	ret = i915_gem_init_hw(dev);

	dev_priv->gpu_error.reload_in_reset = false;

	mutex_unlock(&dev->struct_mutex);
	if (ret) {
		DRM_ERROR("Failed hw init on reset %d\n", ret);
		return ret;
	}

	/*
	 * rps/rc6 re-init is necessary to restore state lost after the
	 * reset and the re-install of gt irqs. Skip for ironlake per
	 * previous concerns that it doesn't respond well to some forms
	 * of re-init after reset.
	 */
	if (INTEL_INFO(dev)->gen > 5)
		intel_enable_gt_powersave(dev);

	return 0;
}

#ifdef __linux__
static int i915_pci_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	struct intel_device_info *intel_info =
		(struct intel_device_info *) ent->driver_data;

	if (IS_PRELIMINARY_HW(intel_info) && !i915.preliminary_hw_support) {
		DRM_INFO("This hardware requires preliminary hardware support.\n"
			 "See CONFIG_DRM_I915_PRELIMINARY_HW_SUPPORT, and/or modparam preliminary_hw_support\n");
		return -ENODEV;
	}

	/* Only bind to function 0 of the device. Early generations
	 * used function 1 as a placeholder for multi-head. This causes
	 * us confusion instead, especially on the systems where both
	 * functions have the same PCI-ID!
	 */
	if (PCI_FUNC(pdev->devfn))
		return -ENODEV;

	return drm_get_pci_dev(pdev, ent, &driver);
}

static void
i915_pci_remove(struct pci_dev *pdev)
{
	struct drm_device *dev = pci_get_drvdata(pdev);

	drm_put_dev(dev);
}

static int i915_pm_suspend(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct drm_device *drm_dev = pci_get_drvdata(pdev);

	if (!drm_dev || !drm_dev->dev_private) {
		dev_err(dev, "DRM not initialized, aborting suspend.\n");
		return -ENODEV;
	}

	if (drm_dev->switch_power_state == DRM_SWITCH_POWER_OFF)
		return 0;

	return i915_drm_suspend(drm_dev);
}

static int i915_pm_suspend_late(struct device *dev)
{
	struct drm_device *drm_dev = dev_to_i915(dev)->dev;

	/*
	 * We have a suspend ordering issue with the snd-hda driver also
	 * requiring our device to be power up. Due to the lack of a
	 * parent/child relationship we currently solve this with an late
	 * suspend hook.
	 *
	 * FIXME: This should be solved with a special hdmi sink device or
	 * similar so that power domains can be employed.
	 */
	if (drm_dev->switch_power_state == DRM_SWITCH_POWER_OFF)
		return 0;

	return i915_drm_suspend_late(drm_dev, false);
}

static int i915_pm_poweroff_late(struct device *dev)
{
	struct drm_device *drm_dev = dev_to_i915(dev)->dev;

	if (drm_dev->switch_power_state == DRM_SWITCH_POWER_OFF)
		return 0;

	return i915_drm_suspend_late(drm_dev, true);
}

static int i915_pm_resume_early(struct device *dev)
{
	struct drm_device *drm_dev = dev_to_i915(dev)->dev;

	if (drm_dev->switch_power_state == DRM_SWITCH_POWER_OFF)
		return 0;

	return i915_drm_resume_early(drm_dev);
}

static int i915_pm_resume(struct device *dev)
{
	struct drm_device *drm_dev = dev_to_i915(dev)->dev;

	if (drm_dev->switch_power_state == DRM_SWITCH_POWER_OFF)
		return 0;

	return i915_drm_resume(drm_dev);
}
#endif

static int skl_suspend_complete(struct drm_i915_private *dev_priv)
{
	/* Enabling DC6 is not a hard requirement to enter runtime D3 */

	skl_uninit_cdclk(dev_priv);

	return 0;
}

static int hsw_suspend_complete(struct drm_i915_private *dev_priv)
{
	hsw_enable_pc8(dev_priv);

	return 0;
}

static int bxt_suspend_complete(struct drm_i915_private *dev_priv)
{
	struct drm_device *dev = dev_priv->dev;

	/* TODO: when DC5 support is added disable DC5 here. */

	broxton_ddi_phy_uninit(dev);
	broxton_uninit_cdclk(dev);
	bxt_enable_dc9(dev_priv);

	return 0;
}

static int bxt_resume_prepare(struct drm_i915_private *dev_priv)
{
	struct drm_device *dev = dev_priv->dev;

	/* TODO: when CSR FW support is added make sure the FW is loaded */

	bxt_disable_dc9(dev_priv);

	/*
	 * TODO: when DC5 support is added enable DC5 here if the CSR FW
	 * is available.
	 */
	broxton_init_cdclk(dev);
	broxton_ddi_phy_init(dev);
	intel_prepare_ddi(dev);

	return 0;
}

static int skl_resume_prepare(struct drm_i915_private *dev_priv)
{
	struct drm_device *dev = dev_priv->dev;

	skl_init_cdclk(dev_priv);
	intel_csr_load_program(dev);

	return 0;
}

/*
 * Save all Gunit registers that may be lost after a D3 and a subsequent
 * S0i[R123] transition. The list of registers needing a save/restore is
 * defined in the VLV2_S0IXRegs document. This documents marks all Gunit
 * registers in the following way:
 * - Driver: saved/restored by the driver
 * - Punit : saved/restored by the Punit firmware
 * - No, w/o marking: no need to save/restore, since the register is R/O or
 *                    used internally by the HW in a way that doesn't depend
 *                    keeping the content across a suspend/resume.
 * - Debug : used for debugging
 *
 * We save/restore all registers marked with 'Driver', with the following
 * exceptions:
 * - Registers out of use, including also registers marked with 'Debug'.
 *   These have no effect on the driver's operation, so we don't save/restore
 *   them to reduce the overhead.
 * - Registers that are fully setup by an initialization function called from
 *   the resume path. For example many clock gating and RPS/RC6 registers.
 * - Registers that provide the right functionality with their reset defaults.
 *
 * TODO: Except for registers that based on the above 3 criteria can be safely
 * ignored, we save/restore all others, practically treating the HW context as
 * a black-box for the driver. Further investigation is needed to reduce the
 * saved/restored registers even further, by following the same 3 criteria.
 */
static void vlv_save_gunit_s0ix_state(struct drm_i915_private *dev_priv)
{
	struct vlv_s0ix_state *s = &dev_priv->vlv_s0ix_state;
	int i;

	/* GAM 0x4000-0x4770 */
	s->wr_watermark		= I915_READ(GEN7_WR_WATERMARK);
	s->gfx_prio_ctrl	= I915_READ(GEN7_GFX_PRIO_CTRL);
	s->arb_mode		= I915_READ(ARB_MODE);
	s->gfx_pend_tlb0	= I915_READ(GEN7_GFX_PEND_TLB0);
	s->gfx_pend_tlb1	= I915_READ(GEN7_GFX_PEND_TLB1);

	for (i = 0; i < ARRAY_SIZE(s->lra_limits); i++)
		s->lra_limits[i] = I915_READ(GEN7_LRA_LIMITS(i));

	s->media_max_req_count	= I915_READ(GEN7_MEDIA_MAX_REQ_COUNT);
	s->gfx_max_req_count	= I915_READ(GEN7_GFX_MAX_REQ_COUNT);

	s->render_hwsp		= I915_READ(RENDER_HWS_PGA_GEN7);
	s->ecochk		= I915_READ(GAM_ECOCHK);
	s->bsd_hwsp		= I915_READ(BSD_HWS_PGA_GEN7);
	s->blt_hwsp		= I915_READ(BLT_HWS_PGA_GEN7);

	s->tlb_rd_addr		= I915_READ(GEN7_TLB_RD_ADDR);

	/* MBC 0x9024-0x91D0, 0x8500 */
	s->g3dctl		= I915_READ(VLV_G3DCTL);
	s->gsckgctl		= I915_READ(VLV_GSCKGCTL);
	s->mbctl		= I915_READ(GEN6_MBCTL);

	/* GCP 0x9400-0x9424, 0x8100-0x810C */
	s->ucgctl1		= I915_READ(GEN6_UCGCTL1);
	s->ucgctl3		= I915_READ(GEN6_UCGCTL3);
	s->rcgctl1		= I915_READ(GEN6_RCGCTL1);
	s->rcgctl2		= I915_READ(GEN6_RCGCTL2);
	s->rstctl		= I915_READ(GEN6_RSTCTL);
	s->misccpctl		= I915_READ(GEN7_MISCCPCTL);

	/* GPM 0xA000-0xAA84, 0x8000-0x80FC */
	s->gfxpause		= I915_READ(GEN6_GFXPAUSE);
	s->rpdeuhwtc		= I915_READ(GEN6_RPDEUHWTC);
	s->rpdeuc		= I915_READ(GEN6_RPDEUC);
	s->ecobus		= I915_READ(ECOBUS);
	s->pwrdwnupctl		= I915_READ(VLV_PWRDWNUPCTL);
	s->rp_down_timeout	= I915_READ(GEN6_RP_DOWN_TIMEOUT);
	s->rp_deucsw		= I915_READ(GEN6_RPDEUCSW);
	s->rcubmabdtmr		= I915_READ(GEN6_RCUBMABDTMR);
	s->rcedata		= I915_READ(VLV_RCEDATA);
	s->spare2gh		= I915_READ(VLV_SPAREG2H);

	/* Display CZ domain, 0x4400C-0x4402C, 0x4F000-0x4F11F */
	s->gt_imr		= I915_READ(GTIMR);
	s->gt_ier		= I915_READ(GTIER);
	s->pm_imr		= I915_READ(GEN6_PMIMR);
	s->pm_ier		= I915_READ(GEN6_PMIER);

	for (i = 0; i < ARRAY_SIZE(s->gt_scratch); i++)
		s->gt_scratch[i] = I915_READ(GEN7_GT_SCRATCH(i));

	/* GT SA CZ domain, 0x100000-0x138124 */
	s->tilectl		= I915_READ(TILECTL);
	s->gt_fifoctl		= I915_READ(GTFIFOCTL);
	s->gtlc_wake_ctrl	= I915_READ(VLV_GTLC_WAKE_CTRL);
	s->gtlc_survive		= I915_READ(VLV_GTLC_SURVIVABILITY_REG);
	s->pmwgicz		= I915_READ(VLV_PMWGICZ);

	/* Gunit-Display CZ domain, 0x182028-0x1821CF */
	s->gu_ctl0		= I915_READ(VLV_GU_CTL0);
	s->gu_ctl1		= I915_READ(VLV_GU_CTL1);
	s->pcbr			= I915_READ(VLV_PCBR);
	s->clock_gate_dis2	= I915_READ(VLV_GUNIT_CLOCK_GATE2);

	/*
	 * Not saving any of:
	 * DFT,		0x9800-0x9EC0
	 * SARB,	0xB000-0xB1FC
	 * GAC,		0x5208-0x524C, 0x14000-0x14C000
	 * PCI CFG
	 */
}

static void vlv_restore_gunit_s0ix_state(struct drm_i915_private *dev_priv)
{
	struct vlv_s0ix_state *s = &dev_priv->vlv_s0ix_state;
	u32 val;
	int i;

	/* GAM 0x4000-0x4770 */
	I915_WRITE(GEN7_WR_WATERMARK,	s->wr_watermark);
	I915_WRITE(GEN7_GFX_PRIO_CTRL,	s->gfx_prio_ctrl);
	I915_WRITE(ARB_MODE,		s->arb_mode | (0xffff << 16));
	I915_WRITE(GEN7_GFX_PEND_TLB0,	s->gfx_pend_tlb0);
	I915_WRITE(GEN7_GFX_PEND_TLB1,	s->gfx_pend_tlb1);

	for (i = 0; i < ARRAY_SIZE(s->lra_limits); i++)
		I915_WRITE(GEN7_LRA_LIMITS(i), s->lra_limits[i]);

	I915_WRITE(GEN7_MEDIA_MAX_REQ_COUNT, s->media_max_req_count);
	I915_WRITE(GEN7_GFX_MAX_REQ_COUNT, s->gfx_max_req_count);

	I915_WRITE(RENDER_HWS_PGA_GEN7,	s->render_hwsp);
	I915_WRITE(GAM_ECOCHK,		s->ecochk);
	I915_WRITE(BSD_HWS_PGA_GEN7,	s->bsd_hwsp);
	I915_WRITE(BLT_HWS_PGA_GEN7,	s->blt_hwsp);

	I915_WRITE(GEN7_TLB_RD_ADDR,	s->tlb_rd_addr);

	/* MBC 0x9024-0x91D0, 0x8500 */
	I915_WRITE(VLV_G3DCTL,		s->g3dctl);
	I915_WRITE(VLV_GSCKGCTL,	s->gsckgctl);
	I915_WRITE(GEN6_MBCTL,		s->mbctl);

	/* GCP 0x9400-0x9424, 0x8100-0x810C */
	I915_WRITE(GEN6_UCGCTL1,	s->ucgctl1);
	I915_WRITE(GEN6_UCGCTL3,	s->ucgctl3);
	I915_WRITE(GEN6_RCGCTL1,	s->rcgctl1);
	I915_WRITE(GEN6_RCGCTL2,	s->rcgctl2);
	I915_WRITE(GEN6_RSTCTL,		s->rstctl);
	I915_WRITE(GEN7_MISCCPCTL,	s->misccpctl);

	/* GPM 0xA000-0xAA84, 0x8000-0x80FC */
	I915_WRITE(GEN6_GFXPAUSE,	s->gfxpause);
	I915_WRITE(GEN6_RPDEUHWTC,	s->rpdeuhwtc);
	I915_WRITE(GEN6_RPDEUC,		s->rpdeuc);
	I915_WRITE(ECOBUS,		s->ecobus);
	I915_WRITE(VLV_PWRDWNUPCTL,	s->pwrdwnupctl);
	I915_WRITE(GEN6_RP_DOWN_TIMEOUT,s->rp_down_timeout);
	I915_WRITE(GEN6_RPDEUCSW,	s->rp_deucsw);
	I915_WRITE(GEN6_RCUBMABDTMR,	s->rcubmabdtmr);
	I915_WRITE(VLV_RCEDATA,		s->rcedata);
	I915_WRITE(VLV_SPAREG2H,	s->spare2gh);

	/* Display CZ domain, 0x4400C-0x4402C, 0x4F000-0x4F11F */
	I915_WRITE(GTIMR,		s->gt_imr);
	I915_WRITE(GTIER,		s->gt_ier);
	I915_WRITE(GEN6_PMIMR,		s->pm_imr);
	I915_WRITE(GEN6_PMIER,		s->pm_ier);

	for (i = 0; i < ARRAY_SIZE(s->gt_scratch); i++)
		I915_WRITE(GEN7_GT_SCRATCH(i), s->gt_scratch[i]);

	/* GT SA CZ domain, 0x100000-0x138124 */
	I915_WRITE(TILECTL,			s->tilectl);
	I915_WRITE(GTFIFOCTL,			s->gt_fifoctl);
	/*
	 * Preserve the GT allow wake and GFX force clock bit, they are not
	 * be restored, as they are used to control the s0ix suspend/resume
	 * sequence by the caller.
	 */
	val = I915_READ(VLV_GTLC_WAKE_CTRL);
	val &= VLV_GTLC_ALLOWWAKEREQ;
	val |= s->gtlc_wake_ctrl & ~VLV_GTLC_ALLOWWAKEREQ;
	I915_WRITE(VLV_GTLC_WAKE_CTRL, val);

	val = I915_READ(VLV_GTLC_SURVIVABILITY_REG);
	val &= VLV_GFX_CLK_FORCE_ON_BIT;
	val |= s->gtlc_survive & ~VLV_GFX_CLK_FORCE_ON_BIT;
	I915_WRITE(VLV_GTLC_SURVIVABILITY_REG, val);

	I915_WRITE(VLV_PMWGICZ,			s->pmwgicz);

	/* Gunit-Display CZ domain, 0x182028-0x1821CF */
	I915_WRITE(VLV_GU_CTL0,			s->gu_ctl0);
	I915_WRITE(VLV_GU_CTL1,			s->gu_ctl1);
	I915_WRITE(VLV_PCBR,			s->pcbr);
	I915_WRITE(VLV_GUNIT_CLOCK_GATE2,	s->clock_gate_dis2);
}

int vlv_force_gfx_clock(struct drm_i915_private *dev_priv, bool force_on)
{
	u32 val;
	int err;

#define COND (I915_READ(VLV_GTLC_SURVIVABILITY_REG) & VLV_GFX_CLK_STATUS_BIT)

	val = I915_READ(VLV_GTLC_SURVIVABILITY_REG);
	val &= ~VLV_GFX_CLK_FORCE_ON_BIT;
	if (force_on)
		val |= VLV_GFX_CLK_FORCE_ON_BIT;
	I915_WRITE(VLV_GTLC_SURVIVABILITY_REG, val);

	if (!force_on)
		return 0;

	err = wait_for(COND, 20);
	if (err)
		DRM_ERROR("timeout waiting for GFX clock force-on (%08x)\n",
			  I915_READ(VLV_GTLC_SURVIVABILITY_REG));

	return err;
#undef COND
}

static int vlv_allow_gt_wake(struct drm_i915_private *dev_priv, bool allow)
{
	u32 val;
	int err = 0;

	val = I915_READ(VLV_GTLC_WAKE_CTRL);
	val &= ~VLV_GTLC_ALLOWWAKEREQ;
	if (allow)
		val |= VLV_GTLC_ALLOWWAKEREQ;
	I915_WRITE(VLV_GTLC_WAKE_CTRL, val);
	POSTING_READ(VLV_GTLC_WAKE_CTRL);

#define COND (!!(I915_READ(VLV_GTLC_PW_STATUS) & VLV_GTLC_ALLOWWAKEACK) == \
	      allow)
	err = wait_for(COND, 1);
	if (err)
		DRM_ERROR("timeout disabling GT waking\n");
	return err;
#undef COND
}

static int vlv_wait_for_gt_wells(struct drm_i915_private *dev_priv,
				 bool wait_for_on)
{
	u32 mask;
	u32 val;
	int err;

	mask = VLV_GTLC_PW_MEDIA_STATUS_MASK | VLV_GTLC_PW_RENDER_STATUS_MASK;
	val = wait_for_on ? mask : 0;
#define COND ((I915_READ(VLV_GTLC_PW_STATUS) & mask) == val)
	if (COND)
		return 0;

	DRM_DEBUG_KMS("waiting for GT wells to go %s (%08x)\n",
			wait_for_on ? "on" : "off",
			I915_READ(VLV_GTLC_PW_STATUS));

	/*
	 * RC6 transitioning can be delayed up to 2 msec (see
	 * valleyview_enable_rps), use 3 msec for safety.
	 */
	err = wait_for(COND, 3);
	if (err)
		DRM_ERROR("timeout waiting for GT wells to go %s\n",
			  wait_for_on ? "on" : "off");

	return err;
#undef COND
}

static void vlv_check_no_gt_access(struct drm_i915_private *dev_priv)
{
	if (!(I915_READ(VLV_GTLC_PW_STATUS) & VLV_GTLC_ALLOWWAKEERR))
		return;

	DRM_ERROR("GT register access while GT waking disabled\n");
	I915_WRITE(VLV_GTLC_PW_STATUS, VLV_GTLC_ALLOWWAKEERR);
}

static int vlv_suspend_complete(struct drm_i915_private *dev_priv)
{
	u32 mask;
	int err;

	/*
	 * Bspec defines the following GT well on flags as debug only, so
	 * don't treat them as hard failures.
	 */
	(void)vlv_wait_for_gt_wells(dev_priv, false);

	mask = VLV_GTLC_RENDER_CTX_EXISTS | VLV_GTLC_MEDIA_CTX_EXISTS;
	WARN_ON((I915_READ(VLV_GTLC_WAKE_CTRL) & mask) != mask);

	vlv_check_no_gt_access(dev_priv);

	err = vlv_force_gfx_clock(dev_priv, true);
	if (err)
		goto err1;

	err = vlv_allow_gt_wake(dev_priv, false);
	if (err)
		goto err2;

	if (!IS_CHERRYVIEW(dev_priv->dev))
		vlv_save_gunit_s0ix_state(dev_priv);

	err = vlv_force_gfx_clock(dev_priv, false);
	if (err)
		goto err2;

	return 0;

err2:
	/* For safety always re-enable waking and disable gfx clock forcing */
	vlv_allow_gt_wake(dev_priv, true);
err1:
	vlv_force_gfx_clock(dev_priv, false);

	return err;
}

static int vlv_resume_prepare(struct drm_i915_private *dev_priv,
				bool rpm_resume)
{
	struct drm_device *dev = dev_priv->dev;
	int err;
	int ret;

	/*
	 * If any of the steps fail just try to continue, that's the best we
	 * can do at this point. Return the first error code (which will also
	 * leave RPM permanently disabled).
	 */
	ret = vlv_force_gfx_clock(dev_priv, true);

	if (!IS_CHERRYVIEW(dev_priv->dev))
		vlv_restore_gunit_s0ix_state(dev_priv);

	err = vlv_allow_gt_wake(dev_priv, true);
	if (!ret)
		ret = err;

	err = vlv_force_gfx_clock(dev_priv, false);
	if (!ret)
		ret = err;

	vlv_check_no_gt_access(dev_priv);

	if (rpm_resume) {
		intel_init_clock_gating(dev);
		i915_gem_restore_fences(dev);
	}

	return ret;
}

#ifdef __linux__
static int intel_runtime_suspend(struct device *device)
{
	struct pci_dev *pdev = to_pci_dev(device);
	struct drm_device *dev = pci_get_drvdata(pdev);
	struct drm_i915_private *dev_priv = dev->dev_private;
	int ret;

	if (WARN_ON_ONCE(!(dev_priv->rps.enabled && intel_enable_rc6(dev))))
		return -ENODEV;

	if (WARN_ON_ONCE(!HAS_RUNTIME_PM(dev)))
		return -ENODEV;

	DRM_DEBUG_KMS("Suspending device\n");

	/*
	 * We could deadlock here in case another thread holding struct_mutex
	 * calls RPM suspend concurrently, since the RPM suspend will wait
	 * first for this RPM suspend to finish. In this case the concurrent
	 * RPM resume will be followed by its RPM suspend counterpart. Still
	 * for consistency return -EAGAIN, which will reschedule this suspend.
	 */
	if (!mutex_trylock(&dev->struct_mutex)) {
		DRM_DEBUG_KMS("device lock contention, deffering suspend\n");
		/*
		 * Bump the expiration timestamp, otherwise the suspend won't
		 * be rescheduled.
		 */
		pm_runtime_mark_last_busy(device);

		return -EAGAIN;
	}
	/*
	 * We are safe here against re-faults, since the fault handler takes
	 * an RPM reference.
	 */
	i915_gem_release_all_mmaps(dev_priv);
	mutex_unlock(&dev->struct_mutex);

	intel_guc_suspend(dev);

	intel_suspend_gt_powersave(dev);
	intel_runtime_pm_disable_interrupts(dev_priv);

	ret = intel_suspend_complete(dev_priv);
	if (ret) {
		DRM_ERROR("Runtime suspend failed, disabling it (%d)\n", ret);
		intel_runtime_pm_enable_interrupts(dev_priv);

		return ret;
	}

	cancel_delayed_work_sync(&dev_priv->gpu_error.hangcheck_work);
	intel_uncore_forcewake_reset(dev, false);
	dev_priv->pm.suspended = true;

	/*
	 * FIXME: We really should find a document that references the arguments
	 * used below!
	 */
	if (IS_BROADWELL(dev)) {
		/*
		 * On Broadwell, if we use PCI_D1 the PCH DDI ports will stop
		 * being detected, and the call we do at intel_runtime_resume()
		 * won't be able to restore them. Since PCI_D3hot matches the
		 * actual specification and appears to be working, use it.
		 */
		intel_opregion_notify_adapter(dev, PCI_D3hot);
	} else {
		/*
		 * current versions of firmware which depend on this opregion
		 * notification have repurposed the D1 definition to mean
		 * "runtime suspended" vs. what you would normally expect (D3)
		 * to distinguish it from notifications that might be sent via
		 * the suspend path.
		 */
		intel_opregion_notify_adapter(dev, PCI_D1);
	}

	assert_forcewakes_inactive(dev_priv);

	DRM_DEBUG_KMS("Device suspended\n");
	return 0;
}

static int intel_runtime_resume(struct device *device)
{
	struct pci_dev *pdev = to_pci_dev(device);
	struct drm_device *dev = pci_get_drvdata(pdev);
	struct drm_i915_private *dev_priv = dev->dev_private;
	int ret = 0;

	if (WARN_ON_ONCE(!HAS_RUNTIME_PM(dev)))
		return -ENODEV;

	DRM_DEBUG_KMS("Resuming device\n");

	intel_opregion_notify_adapter(dev, PCI_D0);
	dev_priv->pm.suspended = false;

	intel_guc_resume(dev);

	if (IS_GEN6(dev_priv))
		intel_init_pch_refclk(dev);

	if (IS_BROXTON(dev))
		ret = bxt_resume_prepare(dev_priv);
	else if (IS_SKYLAKE(dev) || IS_KABYLAKE(dev))
		ret = skl_resume_prepare(dev_priv);
	else if (IS_HASWELL(dev_priv) || IS_BROADWELL(dev_priv))
		hsw_disable_pc8(dev_priv);
	else if (IS_VALLEYVIEW(dev_priv))
		ret = vlv_resume_prepare(dev_priv, true);

	/*
	 * No point of rolling back things in case of an error, as the best
	 * we can do is to hope that things will still work (and disable RPM).
	 */
	i915_gem_init_swizzling(dev);
	gen6_update_ring_freq(dev);

	intel_runtime_pm_enable_interrupts(dev_priv);

	/*
	 * On VLV/CHV display interrupts are part of the display
	 * power well, so hpd is reinitialized from there. For
	 * everyone else do it here.
	 */
	if (!IS_VALLEYVIEW(dev_priv))
		intel_hpd_init(dev_priv);

	intel_enable_gt_powersave(dev);

	if (ret)
		DRM_ERROR("Runtime resume failed, disabling it (%d)\n", ret);
	else
		DRM_DEBUG_KMS("Device resumed\n");

	return ret;
}
#endif

/*
 * This function implements common functionality of runtime and system
 * suspend sequence.
 */
static int intel_suspend_complete(struct drm_i915_private *dev_priv)
{
	int ret;

	if (IS_BROXTON(dev_priv))
		ret = bxt_suspend_complete(dev_priv);
	else if (IS_SKYLAKE(dev_priv) || IS_KABYLAKE(dev_priv))
		ret = skl_suspend_complete(dev_priv);
	else if (IS_HASWELL(dev_priv) || IS_BROADWELL(dev_priv))
		ret = hsw_suspend_complete(dev_priv);
	else if (IS_VALLEYVIEW(dev_priv))
		ret = vlv_suspend_complete(dev_priv);
	else
		ret = 0;

	return ret;
}

#ifdef __linux__
static const struct dev_pm_ops i915_pm_ops = {
	/*
	 * S0ix (via system suspend) and S3 event handlers [PMSG_SUSPEND,
	 * PMSG_RESUME]
	 */
	.suspend = i915_pm_suspend,
	.suspend_late = i915_pm_suspend_late,
	.resume_early = i915_pm_resume_early,
	.resume = i915_pm_resume,

	/*
	 * S4 event handlers
	 * @freeze, @freeze_late    : called (1) before creating the
	 *                            hibernation image [PMSG_FREEZE] and
	 *                            (2) after rebooting, before restoring
	 *                            the image [PMSG_QUIESCE]
	 * @thaw, @thaw_early       : called (1) after creating the hibernation
	 *                            image, before writing it [PMSG_THAW]
	 *                            and (2) after failing to create or
	 *                            restore the image [PMSG_RECOVER]
	 * @poweroff, @poweroff_late: called after writing the hibernation
	 *                            image, before rebooting [PMSG_HIBERNATE]
	 * @restore, @restore_early : called after rebooting and restoring the
	 *                            hibernation image [PMSG_RESTORE]
	 */
	.freeze = i915_pm_suspend,
	.freeze_late = i915_pm_suspend_late,
	.thaw_early = i915_pm_resume_early,
	.thaw = i915_pm_resume,
	.poweroff = i915_pm_suspend,
	.poweroff_late = i915_pm_poweroff_late,
	.restore_early = i915_pm_resume_early,
	.restore = i915_pm_resume,

	/* S0ix (via runtime suspend) event handlers */
	.runtime_suspend = intel_runtime_suspend,
	.runtime_resume = intel_runtime_resume,
};

static const struct vm_operations_struct i915_gem_vm_ops = {
	.fault = i915_gem_fault,
	.open = drm_gem_vm_open,
	.close = drm_gem_vm_close,
};

static const struct file_operations i915_driver_fops = {
	.owner = THIS_MODULE,
	.open = drm_open,
	.release = drm_release,
	.unlocked_ioctl = drm_ioctl,
	.mmap = drm_gem_mmap,
	.poll = drm_poll,
	.read = drm_read,
#ifdef CONFIG_COMPAT
	.compat_ioctl = i915_compat_ioctl,
#endif
	.llseek = noop_llseek,
};
#endif

static struct drm_driver driver = {
	/* Don't use MTRRs here; the Xserver or userspace app should
	 * deal with them for Intel hardware.
	 */
	.driver_features =
	    DRIVER_HAVE_IRQ | DRIVER_IRQ_SHARED | DRIVER_GEM | DRIVER_PRIME |
	    DRIVER_RENDER | DRIVER_MODESET,
#ifdef __linux__
	.load = i915_driver_load,
	.unload = i915_driver_unload,
#endif
	.open = i915_driver_open,
	.lastclose = i915_driver_lastclose,
	.preclose = i915_driver_preclose,
	.postclose = i915_driver_postclose,
#ifdef __linux__
	.set_busid = drm_pci_set_busid,
#endif

#if defined(CONFIG_DEBUG_FS)
	.debugfs_init = i915_debugfs_init,
	.debugfs_cleanup = i915_debugfs_cleanup,
#endif
	.gem_free_object = i915_gem_free_object,
#ifdef __linux__
	.gem_vm_ops = &i915_gem_vm_ops,
#else
	.gem_fault = i915_gem_fault,
#endif

	.prime_handle_to_fd = drm_gem_prime_handle_to_fd,
	.prime_fd_to_handle = drm_gem_prime_fd_to_handle,
	.gem_prime_export = i915_gem_prime_export,
	.gem_prime_import = i915_gem_prime_import,

	.dumb_create = i915_gem_dumb_create,
	.dumb_map_offset = i915_gem_mmap_gtt,
	.dumb_destroy = drm_gem_dumb_destroy,
	.ioctls = i915_ioctls,
#ifdef __linux__
	.fops = &i915_driver_fops,
#endif
	.name = DRIVER_NAME,
	.desc = DRIVER_DESC,
	.date = DRIVER_DATE,
	.major = DRIVER_MAJOR,
	.minor = DRIVER_MINOR,
	.patchlevel = DRIVER_PATCHLEVEL,
};

#ifdef __linux__

static struct pci_driver i915_pci_driver = {
	.name = DRIVER_NAME,
	.id_table = pciidlist,
	.probe = i915_pci_probe,
	.remove = i915_pci_remove,
	.driver.pm = &i915_pm_ops,
};

static int __init i915_init(void)
{
	driver.num_ioctls = i915_max_ioctl;

	/*
	 * Enable KMS by default, unless explicitly overriden by
	 * either the i915.modeset prarameter or by the
	 * vga_text_mode_force boot option.
	 */

	if (i915.modeset == 0)
		driver.driver_features &= ~DRIVER_MODESET;

#ifdef CONFIG_VGA_CONSOLE
	if (vgacon_text_force() && i915.modeset == -1)
		driver.driver_features &= ~DRIVER_MODESET;
#endif

	if (!(driver.driver_features & DRIVER_MODESET)) {
		/* Silently fail loading to not upset userspace. */
		DRM_DEBUG_DRIVER("KMS and UMS disabled.\n");
		return 0;
	}

	if (i915.nuclear_pageflip)
		driver.driver_features |= DRIVER_ATOMIC;

	return drm_pci_init(&driver, &i915_pci_driver);
}

static void __exit i915_exit(void)
{
	if (!(driver.driver_features & DRIVER_MODESET))
		return; /* Never loaded a driver. */

	drm_pci_exit(&driver, &i915_pci_driver);
}

module_init(i915_init);
module_exit(i915_exit);

#endif

MODULE_AUTHOR("Tungsten Graphics, Inc.");
MODULE_AUTHOR("Intel Corporation");

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL and additional rights");

#ifdef __OpenBSD__

#ifdef __amd64__
#include "efifb.h"
#endif

#if NEFIFB > 0
#include <machine/efifbvar.h>
#endif

#include "intagp.h"

#if NINTAGP > 0
int	intagpsubmatch(struct device *, void *, void *);
int	intagp_print(void *, const char *);

int
intagpsubmatch(struct device *parent, void *match, void *aux)
{
	extern struct cfdriver intagp_cd;
	struct cfdata *cf = match;

	/* only allow intagp to attach */
	if (cf->cf_driver == &intagp_cd)
		return ((*cf->cf_attach->ca_match)(parent, match, aux));
	return (0);
}

int
intagp_print(void *vaa, const char *pnp)
{
	if (pnp)
		printf("intagp at %s", pnp);
	return (UNCONF);
}
#endif

int	inteldrm_wsioctl(void *, u_long, caddr_t, int, struct proc *);
paddr_t	inteldrm_wsmmap(void *, off_t, int);
int	inteldrm_alloc_screen(void *, const struct wsscreen_descr *,
	    void **, int *, int *, long *);
void	inteldrm_free_screen(void *, void *);
int	inteldrm_show_screen(void *, void *, int,
	    void (*)(void *, int, int), void *);
void	inteldrm_doswitch(void *);
void	inteldrm_enter_ddb(void *, void *);
int	inteldrm_load_font(void *, void *, struct wsdisplay_font *);
int	inteldrm_list_font(void *, struct wsdisplay_font *);
int	inteldrm_getchar(void *, int, int, struct wsdisplay_charcell *);
void	inteldrm_burner(void *, u_int, u_int);
void	inteldrm_burner_cb(void *);
void	inteldrm_scrollback(void *, void *, int lines);

struct wsscreen_descr inteldrm_stdscreen = {
	"std",
	0, 0,
	0,
	0, 0,
	WSSCREEN_UNDERLINE | WSSCREEN_HILIT |
	WSSCREEN_REVERSE | WSSCREEN_WSCOLORS
};

const struct wsscreen_descr *inteldrm_scrlist[] = {
	&inteldrm_stdscreen,
};

struct wsscreen_list inteldrm_screenlist = {
	nitems(inteldrm_scrlist), inteldrm_scrlist
};

struct wsdisplay_accessops inteldrm_accessops = {
	.ioctl = inteldrm_wsioctl,
	.mmap = inteldrm_wsmmap,
	.alloc_screen = inteldrm_alloc_screen,
	.free_screen = inteldrm_free_screen,
	.show_screen = inteldrm_show_screen,
	.enter_ddb = inteldrm_enter_ddb,
	.getchar = inteldrm_getchar,
	.load_font = inteldrm_load_font,
	.list_font = inteldrm_list_font,
	.scrollback = inteldrm_scrollback,
	.burn_screen = inteldrm_burner
};

extern int (*ws_get_param)(struct wsdisplay_param *);
extern int (*ws_set_param)(struct wsdisplay_param *);

int
inteldrm_wsioctl(void *v, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct inteldrm_softc *dev_priv = v;
	struct backlight_device *bd = dev_priv->backlight;
	struct rasops_info *ri = &dev_priv->ro;
	struct wsdisplay_fbinfo *wdf;
	struct wsdisplay_param *dp = (struct wsdisplay_param *)data;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(int *)data = WSDISPLAY_TYPE_INTELDRM;
		return 0;
	case WSDISPLAYIO_GINFO:
		wdf = (struct wsdisplay_fbinfo *)data;
		wdf->width = ri->ri_width;
		wdf->height = ri->ri_height;
		wdf->depth = ri->ri_depth;
		wdf->cmsize = 0;
		return 0;
	case WSDISPLAYIO_GETPARAM:
		if (ws_get_param && ws_get_param(dp) == 0)
			return 0;

		if (bd == NULL)
			return -1;

		switch (dp->param) {
		case WSDISPLAYIO_PARAM_BRIGHTNESS:
			dp->min = 0;
			dp->max = bd->props.max_brightness;
			dp->curval = bd->ops->get_brightness(bd);
			return (dp->max > dp->min) ? 0 : -1;
		}
		break;
	case WSDISPLAYIO_SETPARAM:
		if (ws_set_param && ws_set_param(dp) == 0)
			return 0;

		if (bd == NULL || dp->curval > bd->props.max_brightness)
			return -1;

		switch (dp->param) {
		case WSDISPLAYIO_PARAM_BRIGHTNESS:
			bd->props.brightness = dp->curval;
			backlight_update_status(bd);
			return 0;
		}
		break;
	}

	return (-1);
}

paddr_t
inteldrm_wsmmap(void *v, off_t off, int prot)
{
	return (-1);
}

int
inteldrm_alloc_screen(void *v, const struct wsscreen_descr *type,
    void **cookiep, int *curxp, int *curyp, long *attrp)
{
	struct inteldrm_softc *dev_priv = v;
	struct rasops_info *ri = &dev_priv->ro;

	return rasops_alloc_screen(ri, cookiep, curxp, curyp, attrp);
}

void
inteldrm_free_screen(void *v, void *cookie)
{
	struct inteldrm_softc *dev_priv = v;
	struct rasops_info *ri = &dev_priv->ro;

	return rasops_free_screen(ri, cookie);
}

int
inteldrm_show_screen(void *v, void *cookie, int waitok,
    void (*cb)(void *, int, int), void *cbarg)
{
	struct inteldrm_softc *dev_priv = v;
	struct rasops_info *ri = &dev_priv->ro;

	if (cookie == ri->ri_active)
		return (0);

	dev_priv->switchcb = cb;
	dev_priv->switchcbarg = cbarg;
	dev_priv->switchcookie = cookie;
	if (cb) {
		task_add(systq, &dev_priv->switchtask);
		return (EAGAIN);
	}

	inteldrm_doswitch(v);

	return (0);
}

void
inteldrm_doswitch(void *v)
{
	struct inteldrm_softc *dev_priv = v;
	struct rasops_info *ri = &dev_priv->ro;
	struct drm_device *dev = dev_priv->dev;

	rasops_show_screen(ri, dev_priv->switchcookie, 0, NULL, NULL);
	intel_fbdev_restore_mode(dev);

	if (dev_priv->switchcb)
		(*dev_priv->switchcb)(dev_priv->switchcbarg, 0, 0);
}

void
inteldrm_enter_ddb(void *v, void *cookie)
{
	struct inteldrm_softc *dev_priv = v;
	struct rasops_info *ri = &dev_priv->ro;
	struct drm_fb_helper *helper = &dev_priv->fbdev->helper;

	if (cookie == ri->ri_active)
		return;

	rasops_show_screen(ri, cookie, 0, NULL, NULL);
	drm_fb_helper_debug_enter(helper->fbdev);
}

int
inteldrm_getchar(void *v, int row, int col, struct wsdisplay_charcell *cell)
{
	struct inteldrm_softc *dev_priv = v;
	struct rasops_info *ri = &dev_priv->ro;

	return rasops_getchar(ri, row, col, cell);
}

int
inteldrm_load_font(void *v, void *cookie, struct wsdisplay_font *font)
{
	struct inteldrm_softc *dev_priv = v;
	struct rasops_info *ri = &dev_priv->ro;

	return rasops_load_font(ri, cookie, font);
}

int
inteldrm_list_font(void *v, struct wsdisplay_font *font)
{
	struct inteldrm_softc *dev_priv = v;
	struct rasops_info *ri = &dev_priv->ro;

	return rasops_list_font(ri, font);
}

void
inteldrm_burner(void *v, u_int on, u_int flags)
{
	struct inteldrm_softc *dev_priv = v;

	task_del(systq, &dev_priv->burner_task);

	if (on)
		dev_priv->burner_fblank = FB_BLANK_UNBLANK;
	else {
		if (flags & WSDISPLAY_BURN_VBLANK)
			dev_priv->burner_fblank = FB_BLANK_VSYNC_SUSPEND;
		else
			dev_priv->burner_fblank = FB_BLANK_NORMAL;
	}

	/*
	 * Setting the DPMS mode may sleep while waiting for the display
	 * to come back on so hand things off to a taskq.
	 */
	task_add(systq, &dev_priv->burner_task);
}

void
inteldrm_burner_cb(void *arg1)
{
	struct inteldrm_softc *dev_priv = arg1;
	struct drm_fb_helper *helper = &dev_priv->fbdev->helper;

	drm_fb_helper_blank(dev_priv->burner_fblank, helper->fbdev);
}

int
inteldrm_backlight_update_status(struct backlight_device *bd)
{
	struct wsdisplay_param dp;

	dp.param = WSDISPLAYIO_PARAM_BRIGHTNESS;
	dp.curval = bd->props.brightness;
	ws_set_param(&dp);
	return 0;
}

int
inteldrm_backlight_get_brightness(struct backlight_device *bd)
{
	struct wsdisplay_param dp;

	dp.param = WSDISPLAYIO_PARAM_BRIGHTNESS;
	ws_get_param(&dp);
	return dp.curval;
}

const struct backlight_ops inteldrm_backlight_ops = {
	.update_status = inteldrm_backlight_update_status,
	.get_brightness = inteldrm_backlight_get_brightness
};

void
inteldrm_scrollback(void *v, void *cookie, int lines)
{
	struct inteldrm_softc *dev_priv = v;
	struct rasops_info *ri = &dev_priv->ro;

	rasops_scrollback(ri, cookie, lines);
}

int	inteldrm_match(struct device *, void *, void *);
void	inteldrm_attach(struct device *, struct device *, void *);
int	inteldrm_detach(struct device *, int);
int	inteldrm_activate(struct device *, int);

struct cfattach inteldrm_ca = {
	sizeof(struct inteldrm_softc), inteldrm_match, inteldrm_attach,
	inteldrm_detach, inteldrm_activate
};

struct cfdriver inteldrm_cd = {
	0, "inteldrm", DV_DULL
};

void	inteldrm_init_backlight(struct inteldrm_softc *);
int	inteldrm_intr(void *);

int
inteldrm_match(struct device *parent, void *match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (drm_pciprobe(aux, pciidlist) && pa->pa_function == 0)
		return 20;
	return 0;
}

void
inteldrm_attach(struct device *parent, struct device *self, void *aux)
{
	struct inteldrm_softc *dev_priv = (struct inteldrm_softc *)self;
	struct drm_device *dev;
	struct pci_attach_args *pa = aux;
	const struct drm_pcidev *id;
	struct intel_device_info *info, *device_info;
	struct rasops_info *ri = &dev_priv->ro;
	struct wsemuldisplaydev_attach_args aa;
	extern int vga_console_attached;
	int mmio_bar, mmio_size, mmio_type;
	int console = 0;

	dev_priv->pc = pa->pa_pc;
	dev_priv->tag = pa->pa_tag;
	dev_priv->dmat = pa->pa_dmat;
	dev_priv->bst = pa->pa_memt;
	dev_priv->memex = pa->pa_memex;
	dev_priv->regs = &dev_priv->bar;

	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_DISPLAY &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_DISPLAY_VGA &&
	    (pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG)
	    & (PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE))
	    == (PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE)) {
		vga_console_attached = 1;
		console = 1;
	}

#if NEFIFB > 0
	if (efifb_is_console(pa))
		console = 1;
#endif

	printf("\n");

	driver.num_ioctls = i915_max_ioctl;

	if (i915.nuclear_pageflip)
		driver.driver_features |= DRIVER_ATOMIC;

	dev_priv->dev = dev = (struct drm_device *)
	    drm_attach_pci(&driver, pa, 0, console, self);

	id = drm_find_description(PCI_VENDOR(pa->pa_id),
	    PCI_PRODUCT(pa->pa_id), pciidlist);
	info = (struct intel_device_info *)id->driver_data;

	/* Setup the write-once "constant" device info */
	device_info = (struct intel_device_info *)&dev_priv->info;
	memcpy(device_info, info, sizeof(dev_priv->info));
	device_info->device_id = dev->pdev->device;

	mmio_bar = IS_GEN2(dev) ? 0x14 : 0x10;
	/* Before gen4, the registers and the GTT are behind different BARs.
	 * However, from gen4 onwards, the registers and the GTT are shared
	 * in the same BAR, so we want to restrict this ioremap from
	 * clobbering the GTT which we want ioremap_wc instead. Fortunately,
	 * the register BAR remains the same size for all the earlier
	 * generations up to Ironlake.
	 */
	if (info->gen < 5)
		mmio_size = 512*1024;
	else
		mmio_size = 2*1024*1024;

	mmio_type = pci_mapreg_type(pa->pa_pc, pa->pa_tag, mmio_bar);
	if (pci_mapreg_map(pa, mmio_bar, mmio_type, 0, &dev_priv->regs->bst,
	    &dev_priv->regs->bsh, &dev_priv->regs->base,
	    &dev_priv->regs->size, mmio_size)) {
		printf("%s: can't map registers\n",
		    dev_priv->sc_dev.dv_xname);
		return;
	}

#if NINTAGP > 0
	if (info->gen <= 5) {
		config_found_sm(self, aux, intagp_print, intagpsubmatch);
		dev->agp = drm_agp_init();
		if (dev->agp) {
			if (drm_mtrr_add(dev->agp->info.ai_aperture_base,
			    dev->agp->info.ai_aperture_size, DRM_MTRR_WC) == 0)
				dev->agp->mtrr = 1;
		}
	}
#endif

	if (IS_I945G(dev) || IS_I945GM(dev))
		pa->pa_flags &= ~PCI_FLAGS_MSI_ENABLED;

	if (pci_intr_map_msi(pa, &dev_priv->ih) != 0 &&
	    pci_intr_map(pa, &dev_priv->ih) != 0) {
		printf("%s: couldn't map interrupt\n",
		    dev_priv->sc_dev.dv_xname);
		return;
	}

	printf("%s: %s\n", dev_priv->sc_dev.dv_xname,
	    pci_intr_string(dev_priv->pc, dev_priv->ih));

	dev_priv->irqh = pci_intr_establish(dev_priv->pc, dev_priv->ih,
	    IPL_TTY, inteldrm_intr, dev_priv, dev_priv->sc_dev.dv_xname);
	if (dev_priv->irqh == NULL) {
		printf("%s: couldn't establish interrupt\n",
		    dev_priv->sc_dev.dv_xname);
		return;
	}
	dev->pdev->irq = -1;

	if (i915_driver_load(dev, id->driver_data))
		return;

#if NEFIFB > 0
	if (efifb_is_console(pa))
		efifb_cndetach();
#endif

	printf("%s: %dx%d, %dbpp\n", dev_priv->sc_dev.dv_xname,
	    ri->ri_width, ri->ri_height, ri->ri_depth);

	intel_fbdev_restore_mode(dev);

	inteldrm_init_backlight(dev_priv);

	ri->ri_flg = RI_CENTER | RI_WRONLY | RI_VCONS | RI_CLEAR;
	if (ri->ri_width < ri->ri_height) {
		pcireg_t subsys;

#define PCI_PRODUCT_ASUSTEK_T100HA	0x1bdd

		/*
		 * Asus T100HA needs to be rotated counter-clockwise.
		 * Everybody else seems to mount their panels the
		 * other way around.
		 */
		subsys = pci_conf_read(pa->pa_pc, pa->pa_tag,
		    PCI_SUBSYS_ID_REG);
		if (PCI_VENDOR(subsys) == PCI_VENDOR_ASUSTEK &&
		    PCI_PRODUCT(subsys) == PCI_PRODUCT_ASUSTEK_T100HA)
			ri->ri_flg |= RI_ROTATE_CCW;
		else
			ri->ri_flg |= RI_ROTATE_CW;
	}
	ri->ri_hw = dev_priv;
	rasops_init(ri, 160, 160);

	task_set(&dev_priv->switchtask, inteldrm_doswitch, dev_priv);
	task_set(&dev_priv->burner_task, inteldrm_burner_cb, dev_priv);

	inteldrm_stdscreen.capabilities = ri->ri_caps;
	inteldrm_stdscreen.nrows = ri->ri_rows;
	inteldrm_stdscreen.ncols = ri->ri_cols;
	inteldrm_stdscreen.textops = &ri->ri_ops;
	inteldrm_stdscreen.fontwidth = ri->ri_font->fontwidth;
	inteldrm_stdscreen.fontheight = ri->ri_font->fontheight;

	aa.console = console;
	aa.scrdata = &inteldrm_screenlist;
	aa.accessops = &inteldrm_accessops;
	aa.accesscookie = dev_priv;
	aa.defaultscreens = 0;

	if (console) {
		long defattr;

		/*
		 * Clear the entire screen if we're doing rotation to
		 * make sure no unrotated content survives.
		 */
		if (ri->ri_flg & (RI_ROTATE_CW | RI_ROTATE_CCW))
			memset(ri->ri_bits, 0, ri->ri_height * ri->ri_stride);

		ri->ri_ops.alloc_attr(ri->ri_active, 0, 0, 0, &defattr);
		wsdisplay_cnattach(&inteldrm_stdscreen, ri->ri_active,
		    0, 0, defattr);
	}

	config_found_sm(self, &aa, wsemuldisplaydevprint,
	    wsemuldisplaydevsubmatch);
	return;
}

int
inteldrm_detach(struct device *self, int flags)
{
	return 0;
}

int
inteldrm_activate(struct device *self, int act)
{
	struct inteldrm_softc *dev_priv = (struct inteldrm_softc *)self;
	struct drm_device *dev = dev_priv->dev;
	int rv = 0;

	if (dev == NULL)
		return (0);

	switch (act) {
	case DVACT_QUIESCE:
		rv = config_suspend((struct device *)dev, act);
		i915_drm_suspend(dev);
		i915_drm_suspend_late(dev, false);
		break;
	case DVACT_SUSPEND:
		if (dev->agp)
			config_suspend(dev->agp->agpdev->sc_chipc, act);
		break;
	case DVACT_RESUME:
		if (dev->agp)
			config_suspend(dev->agp->agpdev->sc_chipc, act);
		break;
	case DVACT_WAKEUP:
		i915_drm_resume_early(dev);
		i915_drm_resume(dev);
		intel_fbdev_restore_mode(dev);
		rv = config_suspend((struct device *)dev, act);
		break;
	}

	return (rv);
}

void
inteldrm_native_backlight(struct inteldrm_softc *dev_priv)
{
	struct drm_device *dev = dev_priv->dev;
	struct intel_connector *intel_connector;

	list_for_each_entry(intel_connector,
	    &dev->mode_config.connector_list, base.head) {
		struct drm_connector *connector = &intel_connector->base;
		struct intel_panel *panel = &intel_connector->panel;
		struct backlight_device *bd = panel->backlight.device;

		if (!panel->backlight.present)
			continue;

		connector->backlight_device = bd;
		connector->backlight_property = drm_property_create_range(dev,
		    0, "Backlight", 0, bd->props.max_brightness);
		drm_object_attach_property(&connector->base,
		    connector->backlight_property, bd->props.brightness);

		/*
		 * Use backlight from the first connector that has one
		 * for wscons(4).
		 */
		if (dev_priv->backlight == NULL)
			dev_priv->backlight = bd;
	}
}

void
inteldrm_firmware_backlight(struct inteldrm_softc *dev_priv,
    struct wsdisplay_param *dp)
{
	struct drm_device *dev = dev_priv->dev;
	struct intel_connector *intel_connector;
	struct backlight_properties props;
	struct backlight_device *bd;

	memset(&props, 0, sizeof(props));
	props.type = BACKLIGHT_FIRMWARE;
	props.brightness = dp->curval;
	bd = backlight_device_register(dev->device.dv_xname, NULL, NULL,
	    &inteldrm_backlight_ops, &props);

	list_for_each_entry(intel_connector,
	    &dev->mode_config.connector_list, base.head) {
		struct drm_connector *connector = &intel_connector->base;

		if (connector->connector_type != DRM_MODE_CONNECTOR_LVDS &&
		    connector->connector_type != DRM_MODE_CONNECTOR_eDP &&
		    connector->connector_type != DRM_MODE_CONNECTOR_DSI)
			continue;

		connector->backlight_device = bd;
		connector->backlight_property = drm_property_create_range(dev,
		    0, "Backlight", dp->min, dp->max);
		drm_object_attach_property(&connector->base,
		    connector->backlight_property, dp->curval);
	}
}

void
inteldrm_init_backlight(struct inteldrm_softc *dev_priv)
{
	struct drm_device *dev = dev_priv->dev;
	struct wsdisplay_param dp;

	drm_modeset_lock(&dev->mode_config.connection_mutex, NULL);

	dp.param = WSDISPLAYIO_PARAM_BRIGHTNESS;
	if (ws_get_param && ws_get_param(&dp) == 0)
		inteldrm_firmware_backlight(dev_priv, &dp);
	else
		inteldrm_native_backlight(dev_priv);

	drm_modeset_unlock(&dev->mode_config.connection_mutex);
}

int
inteldrm_intr(void *arg)
{
	struct inteldrm_softc *dev_priv = arg;
	struct drm_device *dev = dev_priv->dev;

	return dev->driver->irq_handler(0, dev);
}

#endif
