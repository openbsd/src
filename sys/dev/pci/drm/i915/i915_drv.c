/* $OpenBSD: i915_drv.c,v 1.89 2015/09/25 16:15:19 jsg Exp $ */
/*
 * Copyright (c) 2008-2009 Owain G. Ainsworth <oga@openbsd.org>
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

#include <dev/pci/drm/drmP.h>
#include <dev/pci/drm/drm.h>
#include <dev/pci/drm/i915_drm.h>
#include <dev/pci/drm/i915_pciids.h> /* XXX */
#include "i915_drv.h"
#include "i915_trace.h"
#include "intel_drv.h"

#include <machine/pmap.h>

#define IS_I9XX(dev)	(INTEL_INFO(dev)->gen >= 3)
/* MCH IFP BARs */
#define I915_IFPADDR	0x60
#define I965_IFPADDR	0x70

struct inteldrm_file {
	struct drm_file	file_priv;
	struct {
	} mm;
};

#ifdef __linux__
static int i915_modeset __read_mostly = -1;
module_param_named(modeset, i915_modeset, int, 0400);
MODULE_PARM_DESC(modeset,
		"Use kernel modesetting [KMS] (0=DRM_I915_KMS from .config, "
		"1=on, -1=force vga console preference [default])");
#endif

unsigned int i915_fbpercrtc __always_unused = 0;
module_param_named(fbpercrtc, i915_fbpercrtc, int, 0400);

int i915_panel_ignore_lid __read_mostly = 1;
module_param_named(panel_ignore_lid, i915_panel_ignore_lid, int, 0600);
MODULE_PARM_DESC(panel_ignore_lid,
		"Override lid status (0=autodetect, 1=autodetect disabled [default], "
		"-1=force lid closed, -2=force lid open)");

unsigned int i915_powersave __read_mostly = 1;
module_param_named(powersave, i915_powersave, int, 0600);
MODULE_PARM_DESC(powersave,
		"Enable powersavings, fbc, downclocking, etc. (default: true)");

int i915_semaphores __read_mostly = -1;
module_param_named(semaphores, i915_semaphores, int, 0400);
MODULE_PARM_DESC(semaphores,
		"Use semaphores for inter-ring sync (default: -1 (use per-chip defaults))");

int i915_enable_rc6 __read_mostly = -1;
module_param_named(i915_enable_rc6, i915_enable_rc6, int, 0400);
MODULE_PARM_DESC(i915_enable_rc6,
		"Enable power-saving render C-state 6. "
		"Different stages can be selected via bitmask values "
		"(0 = disable; 1 = enable rc6; 2 = enable deep rc6; 4 = enable deepest rc6). "
		"For example, 3 would enable rc6 and deep rc6, and 7 would enable everything. "
		"default: -1 (use per-chip default)");

int i915_enable_fbc __read_mostly = -1;
module_param_named(i915_enable_fbc, i915_enable_fbc, int, 0600);
MODULE_PARM_DESC(i915_enable_fbc,
		"Enable frame buffer compression for power savings "
		"(default: -1 (use per-chip default))");

unsigned int i915_lvds_downclock __read_mostly = 0;
module_param_named(lvds_downclock, i915_lvds_downclock, int, 0400);
MODULE_PARM_DESC(lvds_downclock,
		"Use panel (LVDS/eDP) downclocking for power savings "
		"(default: false)");

int i915_lvds_channel_mode __read_mostly;
module_param_named(lvds_channel_mode, i915_lvds_channel_mode, int, 0600);
MODULE_PARM_DESC(lvds_channel_mode,
		 "Specify LVDS channel mode "
		 "(0=probe BIOS [default], 1=single-channel, 2=dual-channel)");

int i915_panel_use_ssc __read_mostly = -1;
module_param_named(lvds_use_ssc, i915_panel_use_ssc, int, 0600);
MODULE_PARM_DESC(lvds_use_ssc,
		"Use Spread Spectrum Clock with panels [LVDS/eDP] "
		"(default: auto from VBT)");

int i915_vbt_sdvo_panel_type __read_mostly = -1;
module_param_named(vbt_sdvo_panel_type, i915_vbt_sdvo_panel_type, int, 0600);
MODULE_PARM_DESC(vbt_sdvo_panel_type,
		"Override/Ignore selection of SDVO panel mode in the VBT "
		"(-2=ignore, -1=auto [default], index in VBT BIOS table)");

static bool i915_try_reset __read_mostly = true;
module_param_named(reset, i915_try_reset, bool, 0600);
MODULE_PARM_DESC(reset, "Attempt GPU resets (default: true)");

bool i915_enable_hangcheck __read_mostly = true;
module_param_named(enable_hangcheck, i915_enable_hangcheck, bool, 0644);
MODULE_PARM_DESC(enable_hangcheck,
		"Periodically check GPU activity for detecting hangs. "
		"WARNING: Disabling this can cause system wide hangs. "
		"(default: true)");

int i915_enable_ppgtt __read_mostly = -1;
module_param_named(i915_enable_ppgtt, i915_enable_ppgtt, int, 0400);
MODULE_PARM_DESC(i915_enable_ppgtt,
		"Enable PPGTT (default: true)");

int i915_enable_psr __read_mostly = 0;
module_param_named(enable_psr, i915_enable_psr, int, 0600);
MODULE_PARM_DESC(enable_psr, "Enable PSR (default: false)");

unsigned int i915_preliminary_hw_support __read_mostly = IS_ENABLED(CONFIG_DRM_I915_PRELIMINARY_HW_SUPPORT);
module_param_named(preliminary_hw_support, i915_preliminary_hw_support, int, 0600);
MODULE_PARM_DESC(preliminary_hw_support,
		"Enable preliminary hardware support.");

int i915_disable_power_well __read_mostly = 1;
module_param_named(disable_power_well, i915_disable_power_well, int, 0600);
MODULE_PARM_DESC(disable_power_well,
		 "Disable the power well when possible (default: true)");

int i915_enable_ips __read_mostly = 0;
module_param_named(enable_ips, i915_enable_ips, int, 0600);
MODULE_PARM_DESC(enable_ips, "Enable IPS (default: true)");

bool i915_fastboot __read_mostly = 0;
module_param_named(fastboot, i915_fastboot, bool, 0600);
MODULE_PARM_DESC(fastboot, "Try to skip unnecessary mode sets at boot time "
		 "(default: false)");

int i915_enable_pc8 __read_mostly = 1;
module_param_named(enable_pc8, i915_enable_pc8, int, 0600);
MODULE_PARM_DESC(enable_pc8, "Enable support for low power package C states (PC8+) (default: true)");

int i915_pc8_timeout __read_mostly = 5000;
module_param_named(pc8_timeout, i915_pc8_timeout, int, 0600);
MODULE_PARM_DESC(pc8_timeout, "Number of msecs of idleness required to enter PC8+ (default: 5000)");

bool i915_prefault_disable __read_mostly;
module_param_named(prefault_disable, i915_prefault_disable, bool, 0600);
MODULE_PARM_DESC(prefault_disable,
		"Disable page prefaulting for pread/pwrite/reloc (default:false). For developers only.");

const struct intel_device_info *
	i915_get_device_id(int);
int	inteldrm_probe(struct device *, void *, void *);
void	inteldrm_attach(struct device *, struct device *, void *);
int	inteldrm_detach(struct device *, int);
int	inteldrm_activate(struct device *, int);
int	inteldrm_intr(void *);
int	inteldrm_ioctl(struct drm_device *, u_long, caddr_t, struct drm_file *);
int	inteldrm_doioctl(struct drm_device *, u_long, caddr_t, struct drm_file *);

int	inteldrm_gmch_match(struct pci_attach_args *);

void	i915_alloc_ifp(struct inteldrm_softc *, struct pci_attach_args *);
void	i965_alloc_ifp(struct inteldrm_softc *, struct pci_attach_args *);
void	intel_gtt_chipset_setup(struct drm_device *);

/* i915_dma.c */
void	intel_setup_mchbar(struct drm_device *);
int	i915_load_modeset_init(struct drm_device *);

#undef INTEL_VGA_DEVICE
#define INTEL_VGA_DEVICE(id, info) {		\
	.class = PCI_CLASS_DISPLAY << 16,	\
	.class_mask = 0xff0000,			\
	.vendor = 0x8086,			\
	.device = id,				\
	.subvendor = PCI_ANY_ID,		\
	.subdevice = PCI_ANY_ID,		\
	.driver_data = (unsigned long) info }

#undef INTEL_QUANTA_VGA_DEVICE
#define INTEL_QUANTA_VGA_DEVICE(info) {		\
	.class = PCI_CLASS_DISPLAY << 16,	\
	.class_mask = 0xff0000,			\
	.vendor = 0x8086,			\
	.device = 0x16a,			\
	.subvendor = 0x152d,			\
	.subdevice = 0x8990,			\
	.driver_data = (unsigned long) info }

static const struct intel_device_info intel_i830_info = {
	.gen = 2, .is_mobile = 1, .cursor_needs_physical = 1, .num_pipes = 2,
	.has_overlay = 1, .overlay_needs_physical = 1,
	.ring_mask = RENDER_RING,
};

static const struct intel_device_info intel_845g_info = {
	.gen = 2, .num_pipes = 1,
	.has_overlay = 1, .overlay_needs_physical = 1,
	.ring_mask = RENDER_RING,
};

static const struct intel_device_info intel_i85x_info = {
	.gen = 2, .is_i85x = 1, .is_mobile = 1, .num_pipes = 2,
	.cursor_needs_physical = 1,
	.has_overlay = 1, .overlay_needs_physical = 1,
	.has_fbc = 1,
	.ring_mask = RENDER_RING,
};

static const struct intel_device_info intel_i865g_info = {
	.gen = 2, .num_pipes = 1,
	.has_overlay = 1, .overlay_needs_physical = 1,
	.ring_mask = RENDER_RING,
};

static const struct intel_device_info intel_i915g_info = {
	.gen = 3, .is_i915g = 1, .cursor_needs_physical = 1, .num_pipes = 2,
	.has_overlay = 1, .overlay_needs_physical = 1,
	.ring_mask = RENDER_RING,
};
static const struct intel_device_info intel_i915gm_info = {
	.gen = 3, .is_mobile = 1, .num_pipes = 2,
	.cursor_needs_physical = 1,
	.has_overlay = 1, .overlay_needs_physical = 1,
	.supports_tv = 1,
	.has_fbc = 1,
	.ring_mask = RENDER_RING,
};
static const struct intel_device_info intel_i945g_info = {
	.gen = 3, .has_hotplug = 1, .cursor_needs_physical = 1, .num_pipes = 2,
	.has_overlay = 1, .overlay_needs_physical = 1,
	.ring_mask = RENDER_RING,
};
static const struct intel_device_info intel_i945gm_info = {
	.gen = 3, .is_i945gm = 1, .is_mobile = 1, .num_pipes = 2,
	.has_hotplug = 1, .cursor_needs_physical = 1,
	.has_overlay = 1, .overlay_needs_physical = 1,
	.supports_tv = 1,
	.has_fbc = 1,
	.ring_mask = RENDER_RING,
};

static const struct intel_device_info intel_i965g_info = {
	.gen = 4, .is_broadwater = 1, .num_pipes = 2,
	.has_hotplug = 1,
	.has_overlay = 1,
	.ring_mask = RENDER_RING,
};

static const struct intel_device_info intel_i965gm_info = {
	.gen = 4, .is_crestline = 1, .num_pipes = 2,
	.is_mobile = 1, .has_fbc = 1, .has_hotplug = 1,
	.has_overlay = 1,
	.supports_tv = 1,
	.ring_mask = RENDER_RING,
};

static const struct intel_device_info intel_g33_info = {
	.gen = 3, .is_g33 = 1, .num_pipes = 2,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_overlay = 1,
	.ring_mask = RENDER_RING,
};

static const struct intel_device_info intel_g45_info = {
	.gen = 4, .is_g4x = 1, .need_gfx_hws = 1, .num_pipes = 2,
	.has_pipe_cxsr = 1, .has_hotplug = 1,
	.ring_mask = RENDER_RING | BSD_RING,
};

static const struct intel_device_info intel_gm45_info = {
	.gen = 4, .is_g4x = 1, .num_pipes = 2,
	.is_mobile = 1, .need_gfx_hws = 1, .has_fbc = 1,
	.has_pipe_cxsr = 1, .has_hotplug = 1,
	.supports_tv = 1,
	.ring_mask = RENDER_RING | BSD_RING,
};

static const struct intel_device_info intel_pineview_info = {
	.gen = 3, .is_g33 = 1, .is_pineview = 1, .is_mobile = 1, .num_pipes = 2,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_overlay = 1,
};

static const struct intel_device_info intel_ironlake_d_info = {
	.gen = 5, .num_pipes = 2,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.ring_mask = RENDER_RING | BSD_RING,
};

static const struct intel_device_info intel_ironlake_m_info = {
	.gen = 5, .is_mobile = 1, .num_pipes = 2,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_fbc = 1,
	.ring_mask = RENDER_RING | BSD_RING,
};

static const struct intel_device_info intel_sandybridge_d_info = {
	.gen = 6, .num_pipes = 2,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_fbc = 1,
	.ring_mask = RENDER_RING | BSD_RING | BLT_RING,
	.has_llc = 1,
};

static const struct intel_device_info intel_sandybridge_m_info = {
	.gen = 6, .is_mobile = 1, .num_pipes = 2,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_fbc = 1,
	.ring_mask = RENDER_RING | BSD_RING | BLT_RING,
	.has_llc = 1,
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
};

static const struct intel_device_info intel_ivybridge_m_info = {
	GEN7_FEATURES,
	.is_ivybridge = 1,
	.is_mobile = 1,
};

static const struct intel_device_info intel_ivybridge_q_info = {
	GEN7_FEATURES,
	.is_ivybridge = 1,
	.num_pipes = 0, /* legal, last one wins */
};

static const struct intel_device_info intel_valleyview_m_info = {
	GEN7_FEATURES,
	.is_mobile = 1,
	.num_pipes = 2,
	.is_valleyview = 1,
	.display_mmio_offset = VLV_DISPLAY_BASE,
	.has_fbc = 0, /* legal, last one wins */
	.has_llc = 0, /* legal, last one wins */
};

static const struct intel_device_info intel_valleyview_d_info = {
	GEN7_FEATURES,
	.num_pipes = 2,
	.is_valleyview = 1,
	.display_mmio_offset = VLV_DISPLAY_BASE,
	.has_fbc = 0, /* legal, last one wins */
	.has_llc = 0, /* legal, last one wins */
};

static const struct intel_device_info intel_haswell_d_info = {
	GEN7_FEATURES,
	.is_haswell = 1,
	.has_ddi = 1,
	.has_fpga_dbg = 1,
	.ring_mask = RENDER_RING | BSD_RING | BLT_RING | VEBOX_RING,
};

static const struct intel_device_info intel_haswell_m_info = {
	GEN7_FEATURES,
	.is_haswell = 1,
	.is_mobile = 1,
	.has_ddi = 1,
	.has_fpga_dbg = 1,
	.ring_mask = RENDER_RING | BSD_RING | BLT_RING | VEBOX_RING,
};

static const struct intel_device_info intel_broadwell_d_info = {
	.gen = 8, .num_pipes = 3,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.ring_mask = RENDER_RING | BSD_RING | BLT_RING | VEBOX_RING,
	.has_llc = 1,
	.has_ddi = 1,
};

static const struct intel_device_info intel_broadwell_m_info = {
	.gen = 8, .is_mobile = 1, .num_pipes = 3,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.ring_mask = RENDER_RING | BSD_RING | BLT_RING | VEBOX_RING,
	.has_llc = 1,
	.has_ddi = 1,
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
	INTEL_BDW_M_IDS(&intel_broadwell_m_info),	\
	INTEL_BDW_D_IDS(&intel_broadwell_d_info)

static const struct drm_pcidev pciidlist[] = {		/* aka */
	INTEL_PCI_IDS,
	{0, 0, 0}
};

static struct drm_driver_info inteldrm_driver = {
	.buf_priv_size		= 1,	/* No dev_priv */
	.file_priv_size		= sizeof(struct inteldrm_file),
	.open = i915_driver_open,
	.lastclose = i915_driver_lastclose,
	.preclose = i915_driver_preclose,
	.postclose = i915_driver_postclose,

//	.gem_init_object	= i915_gem_init_object,
	.gem_free_object	= i915_gem_free_object,
	.gem_fault		= i915_gem_fault,
	.gem_size		= sizeof(struct drm_i915_gem_object),

	.dumb_create = i915_gem_dumb_create,
	.dumb_map_offset = i915_gem_mmap_gtt,
	.dumb_destroy = drm_gem_dumb_destroy,
	.ioctls	= i915_ioctls,

	.name = DRIVER_NAME,
	.desc = DRIVER_DESC,
	.date = DRIVER_DATE,
	.major = DRIVER_MAJOR,
	.minor = DRIVER_MINOR,
	.patchlevel = DRIVER_PATCHLEVEL,

	.flags			= DRIVER_AGP | DRIVER_AGP_REQUIRE |
				    DRIVER_MTRR | DRIVER_IRQ | DRIVER_GEM |
				    DRIVER_MODESET,
};

const struct intel_device_info *
i915_get_device_id(int device)
{
	const struct drm_pcidev *did;

	for (did = &pciidlist[0]; did->device != 0; did++) {
		if (did->device != device)
			continue;
		return ((const struct intel_device_info *)did->driver_data);
	}
	return (NULL);
}

int
inteldrm_probe(struct device *parent, void *match, void *aux)
{
	return (drm_pciprobe((struct pci_attach_args *)aux, pciidlist));
}

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
				WARN_ON(!IS_HASWELL(dev));
				WARN_ON(IS_ULT(dev));
			} else if (IS_BROADWELL(dev)) {
				dev_priv->pch_type = PCH_LPT;
				dev_priv->pch_id =
					INTEL_PCH_LPT_LP_DEVICE_ID_TYPE;
				DRM_DEBUG_KMS("This is Broadwell, assuming "
					      "LynxPoint LP PCH\n");
			} else if (id == INTEL_PCH_LPT_LP_DEVICE_ID_TYPE) {
				dev_priv->pch_type = PCH_LPT;
				DRM_DEBUG_KMS("Found LynxPoint LP PCH\n");
				WARN_ON(!IS_HASWELL(dev));
				WARN_ON(!IS_ULT(dev));
			}
		}
	} else
		DRM_DEBUG_KMS("No PCH found.\n");
}

bool i915_semaphore_is_enabled(struct drm_device *dev)
{
	if (INTEL_INFO(dev)->gen < 6)
		return false;

	/* Until we get further testing... */
	if (IS_GEN8(dev))
		return false;

	if (i915_semaphores >= 0)
		return i915_semaphores;

#ifdef CONFIG_INTEL_IOMMU
	/* Enable semaphores on SNB when IO remapping is off */
	if (INTEL_INFO(dev)->gen == 6 && intel_iommu_gfx_mapped)
		return false;
#endif

	return true;
}

static int i915_drm_freeze(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_crtc *crtc;

	intel_runtime_pm_get(dev_priv);

	/* ignore lid events during suspend */
	mutex_lock(&dev_priv->modeset_restore_lock);
	dev_priv->modeset_restore = MODESET_SUSPENDED;
	mutex_unlock(&dev_priv->modeset_restore_lock);

	/* We do a lot of poking in a lot of registers, make sure they work
	 * properly. */
	hsw_disable_package_c8(dev_priv);
	intel_display_set_init_power(dev, true);

	drm_kms_helper_poll_disable(dev);

#ifdef __linux__
	pci_save_state(dev->pdev);
#endif

	/* If KMS is active, we do the leavevt stuff here */
	if (drm_core_check_feature(dev, DRIVER_MODESET)) {
		int error;

		error = i915_gem_suspend(dev);
		if (error) {
			dev_err(&dev->pdev->dev,
				"GEM idle failed, resume might fail\n");
			return error;
		}

		cancel_delayed_work_sync(&dev_priv->rps.delayed_resume_work);

		drm_irq_uninstall(dev);
		dev_priv->enable_hotplug_processing = false;
		/*
		 * Disable CRTCs directly since we want to preserve sw state
		 * for _thaw.
		 */
		mutex_lock(&dev->mode_config.mutex);
		list_for_each_entry(crtc, &dev->mode_config.crtc_list, head)
			dev_priv->display.crtc_disable(crtc);
		mutex_unlock(&dev->mode_config.mutex);

		intel_modeset_suspend_hw(dev);
	}

	i915_gem_suspend_gtt_mappings(dev);

	i915_save_state(dev);

	intel_opregion_fini(dev);

#ifdef __linux__
	console_lock();
	intel_fbdev_set_suspend(dev, FBINFO_STATE_SUSPENDED);
	console_unlock();
#endif

	return 0;
}

#ifdef __linux__
int i915_suspend(struct drm_device *dev, pm_message_t state)
{
	int error;

	if (!dev || !dev->dev_private) {
		DRM_ERROR("dev: %p\n", dev);
		DRM_ERROR("DRM not initialized, aborting suspend.\n");
		return -ENODEV;
	}

	if (state.event == PM_EVENT_PRETHAW)
		return 0;


	if (dev->switch_power_state == DRM_SWITCH_POWER_OFF)
		return 0;

	error = i915_drm_freeze(dev);
	if (error)
		return error;

	if (state.event == PM_EVENT_SUSPEND) {
		/* Shut down the device */
		pci_disable_device(dev->pdev);
		pci_set_power_state(dev->pdev, PCI_D3hot);
	}

	return 0;
}

void intel_console_resume(struct work_struct *work)
{
	struct drm_i915_private *dev_priv =
		container_of(work, struct drm_i915_private,
			     console_resume_work);
	struct drm_device *dev = dev_priv->dev;

	console_lock();
	intel_fbdev_set_suspend(dev, FBINFO_STATE_RUNNING);
	console_unlock();
}

#else

void intel_console_resume(struct work_struct *work)
{
}

#endif

static void intel_resume_hotplug(struct drm_device *dev)
{
	struct drm_mode_config *mode_config = &dev->mode_config;
	struct intel_encoder *encoder;

	mutex_lock(&mode_config->mutex);
	DRM_DEBUG_KMS("running encoder hotplug functions\n");

	list_for_each_entry(encoder, &mode_config->encoder_list, base.head)
		if (encoder->hot_plug)
			encoder->hot_plug(encoder);

	mutex_unlock(&mode_config->mutex);

	/* Just fire off a uevent and let userspace tell us what to do */
	drm_helper_hpd_irq_event(dev);
}

static int i915_drm_thaw_early(struct drm_device *dev)
{
	intel_uncore_early_sanitize(dev);
	intel_uncore_sanitize(dev);
	intel_power_domains_init_hw(dev);

	return 0;
}

static int __i915_drm_thaw(struct drm_device *dev, bool restore_gtt_mappings)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	int error = 0;

	if (drm_core_check_feature(dev, DRIVER_MODESET) &&
	    restore_gtt_mappings) {
		mutex_lock(&dev->struct_mutex);
		i915_gem_restore_gtt_mappings(dev);
		mutex_unlock(&dev->struct_mutex);
	}

	i915_restore_state(dev);
	intel_opregion_setup(dev);

	/* KMS EnterVT equivalent */
	if (drm_core_check_feature(dev, DRIVER_MODESET)) {
		intel_init_pch_refclk(dev);

		mutex_lock(&dev->struct_mutex);

		error = i915_gem_init_hw(dev);
		mutex_unlock(&dev->struct_mutex);

		/* We need working interrupts for modeset enabling ... */
		drm_irq_install(dev);

		intel_modeset_init_hw(dev);

		drm_modeset_lock_all(dev);
		drm_mode_config_reset(dev);
		intel_modeset_setup_hw_state(dev, true);
		drm_modeset_unlock_all(dev);

		/*
		 * ... but also need to make sure that hotplug processing
		 * doesn't cause havoc. Like in the driver load code we don't
		 * bother with the tiny race here where we might loose hotplug
		 * notifications.
		 * */
		intel_hpd_init(dev);
		dev_priv->enable_hotplug_processing = true;
		/* Config may have changed between suspend and resume */
		intel_resume_hotplug(dev);
	}

	intel_opregion_init(dev);

#ifdef __linux__
	/*
	 * The console lock can be pretty contented on resume due
	 * to all the printk activity.  Try to keep it out of the hot
	 * path of resume if possible.
	 */
	if (console_trylock()) {
		intel_fbdev_set_suspend(dev, FBINFO_STATE_RUNNING);
		console_unlock();
	} else {
		schedule_work(&dev_priv->console_resume_work);
	}
#endif

	/* Undo what we did at i915_drm_freeze so the refcount goes back to the
	 * expected level. */
	hsw_enable_package_c8(dev_priv);

	mutex_lock(&dev_priv->modeset_restore_lock);
	dev_priv->modeset_restore = MODESET_DONE;
	mutex_unlock(&dev_priv->modeset_restore_lock);

	intel_runtime_pm_put(dev_priv);
	return error;
}

static int i915_drm_thaw(struct drm_device *dev)
{
	if (drm_core_check_feature(dev, DRIVER_MODESET))
		i915_check_and_clear_faults(dev);

	return __i915_drm_thaw(dev, true);
}

#ifdef __linux__
static int i915_resume_early(struct drm_device *dev)
{
	if (dev->switch_power_state == DRM_SWITCH_POWER_OFF)
		return 0;

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

	return i915_drm_thaw_early(dev);
}

int i915_resume(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	int ret;

	/*
	 * Platforms with opregion should have sane BIOS, older ones (gen3 and
	 * earlier) need to restore the GTT mappings since the BIOS might clear
	 * all our scratch PTEs.
	 */
	ret = __i915_drm_thaw(dev, !dev_priv->opregion.header);
	if (ret)
		return ret;

	drm_kms_helper_poll_enable(dev);
	return 0;
}

static int i915_resume_legacy(struct drm_device *dev)
{
	i915_resume_early(dev);
	i915_resume(dev);

	return 0;
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
	drm_i915_private_t *dev_priv = dev->dev_private;
	bool simulated;
	int ret;

	if (!i915_try_reset)
		return 0;

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

	if (ret) {
		DRM_ERROR("Failed to reset chip: %i\n", ret);
		mutex_unlock(&dev->struct_mutex);
		return ret;
	}

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
	if (drm_core_check_feature(dev, DRIVER_MODESET) ||
			!dev_priv->ums.mm_suspended) {
		dev_priv->ums.mm_suspended = 0;

		ret = i915_gem_init_hw(dev);
		mutex_unlock(&dev->struct_mutex);
		if (ret) {
			DRM_ERROR("Failed hw init on reset %d\n", ret);
			return ret;
		}

		drm_irq_uninstall(dev);
		drm_irq_install(dev);
		intel_hpd_init(dev);
	} else {
		mutex_unlock(&dev->struct_mutex);
	}

	return 0;
}


int inteldrm_wsioctl(void *, u_long, caddr_t, int, struct proc *);
paddr_t inteldrm_wsmmap(void *, off_t, int);
int inteldrm_alloc_screen(void *, const struct wsscreen_descr *,
    void **, int *, int *, long *);
void inteldrm_free_screen(void *, void *);
int inteldrm_show_screen(void *, void *, int,
    void (*)(void *, int, int), void *);
void inteldrm_doswitch(void *);
int inteldrm_load_font(void *, void *, struct wsdisplay_font *);
int inteldrm_list_font(void *, struct wsdisplay_font *);
int inteldrm_getchar(void *, int, int, struct wsdisplay_charcell *);
void inteldrm_burner(void *, u_int, u_int);

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
	.getchar = inteldrm_getchar,
	.load_font = inteldrm_load_font,
	.list_font = inteldrm_list_font,
	.burn_screen = inteldrm_burner
};

extern int (*ws_get_param)(struct wsdisplay_param *);
extern int (*ws_set_param)(struct wsdisplay_param *);

int
inteldrm_wsioctl(void *v, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct inteldrm_softc *dev_priv = v;
	struct drm_device *dev = dev_priv->dev;
	struct wsdisplay_param *dp = (struct wsdisplay_param *)data;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(int *)data = WSDISPLAY_TYPE_INTELDRM;
		return 0;
	case WSDISPLAYIO_GETPARAM:
		if (ws_get_param && ws_get_param(dp) == 0)
			return 0;

		if (dev_priv->backlight.connector == NULL)
			return -1;

		switch (dp->param) {
		case WSDISPLAYIO_PARAM_BRIGHTNESS:
			dp->min = 0;
			dp->max = dev_priv->backlight.props.max_brightness;
			dp->curval = dev_priv->backlight.props.brightness;
			return (dp->max > dp->min) ? 0 : -1;
		}
		break;
	case WSDISPLAYIO_SETPARAM:
		if (ws_set_param && ws_set_param(dp) == 0)
			return 0;

		if (dev_priv->backlight.connector == NULL ||
		    dp->curval > dev_priv->backlight.props.max_brightness)
			return -1;

		switch (dp->param) {
		case WSDISPLAYIO_PARAM_BRIGHTNESS:
			mutex_lock(&dev->mode_config.mutex);
			intel_panel_set_backlight(dev_priv->backlight.connector,
			    dp->curval, dev_priv->backlight.props.max_brightness);
			mutex_unlock(&dev->mode_config.mutex);
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
	if (cb) {
		dev_priv->switchcookie = cookie;
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
	struct drm_fb_helper *helper = &dev_priv->fbdev->helper;
	int dpms_mode;

	if (on)
		dpms_mode = DRM_MODE_DPMS_ON;
	else {
		if (flags & WSDISPLAY_BURN_VBLANK)
			dpms_mode = DRM_MODE_DPMS_OFF;
		else
			dpms_mode = DRM_MODE_DPMS_STANDBY;
	}

	drm_fb_helper_dpms(helper, dpms_mode);
}

int
inteldrm_intr(void *arg)
{
	struct inteldrm_softc *dev_priv = arg;
	struct drm_device *dev = dev_priv->dev;

	return dev->driver->irq_handler(0, dev);
}

void
inteldrm_attach(struct device *parent, struct device *self, void *aux)
{
	struct inteldrm_softc	*dev_priv = (struct inteldrm_softc *)self;
	struct vga_pci_softc	*vga_sc = (struct vga_pci_softc *)parent;
	struct pci_attach_args	*pa = aux;
	struct vga_pci_bar	*bar;
	struct drm_device	*dev;
	const struct drm_pcidev	*id_entry;
	int			 i;
	uint16_t		 pci_device;
	const struct intel_device_info *info;
	int ret = 0, mmio_bar, mmio_size;
	uint32_t aperture_size;

	id_entry = drm_find_description(PCI_VENDOR(pa->pa_id),
	    PCI_PRODUCT(pa->pa_id), pciidlist);
	pci_device = PCI_PRODUCT(pa->pa_id);
	info = i915_get_device_id(pci_device);
	KASSERT(info->gen != 0);

	dev_priv->info = info;

	dev_priv->pc = pa->pa_pc;
	dev_priv->tag = pa->pa_tag;
	dev_priv->dmat = pa->pa_dmat;
	dev_priv->bst = pa->pa_memt;

	printf("\n");

	if (dev_priv->info->gen >= 6)
		inteldrm_driver.flags &= ~(DRIVER_AGP | DRIVER_AGP_REQUIRE);

	inteldrm_driver.num_ioctls = i915_max_ioctl;

	/* All intel chipsets need to be treated as agp, so just pass one */
	dev = dev_priv->dev = (struct drm_device *)
	    drm_attach_pci(&inteldrm_driver, pa, 1, 1, self);

	printf("%s", dev_priv->sc_dev.dv_xname);

	intel_gtt_chipset_setup(dev);
	mtx_init(&mchdev_lock, IPL_TTY);

	mtx_init(&dev_priv->irq_lock, IPL_TTY);
	mtx_init(&dev_priv->gpu_error.lock, IPL_TTY);
	mtx_init(&dev_priv->backlight_lock, IPL_TTY);
	mtx_init(&dev_priv->uncore.lock, IPL_TTY);
	mtx_init(&dev_priv->mm.object_stat_lock, IPL_TTY);
	rw_init(&dev_priv->dpio_lock, "dpio");
	rw_init(&dev_priv->modeset_restore_lock, "rest");

	intel_pm_setup(dev);

#ifdef __linux__
	intel_display_crc_init(dev);

	i915_dump_device_info(dev_priv);
#endif

	/* Not all pre-production machines fall into this category, only the
	 * very first ones. Almost everything should work, except for maybe
	 * suspend/resume. And we don't implement workarounds that affect only
	 * pre-production machines. */
	if (IS_HSW_EARLY_SDV(dev))
		DRM_INFO("This is an early pre-production Haswell machine. "
			 "It may not be fully functional.\n");

#ifdef __linux__
	if (i915_get_bridge_dev(dev)) {
		ret = -EIO;
		goto free_priv;
	}
#endif

	mmio_bar = IS_GEN2(dev) ? 1 : 0;
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

	/* we need to use this api for now due to sharing with intagp */
	bar = vga_pci_bar_info(vga_sc, mmio_bar);
	if (bar == NULL) {
		printf(": can't get BAR info\n");
		return;
	}

	dev_priv->regs = vga_pci_bar_map(vga_sc, bar->addr, mmio_size, 0);
	if (dev_priv->regs == NULL) {
		printf(": can't map mmio space\n");
		return;
	}

	intel_uncore_early_sanitize(dev);

	/* This must be called before any calls to HAS_PCH_* */
	intel_detect_pch(dev);

	intel_uncore_init(dev);

	ret = i915_gem_gtt_init(dev);
	if (ret)
		goto out_regs;

#ifdef __linux__
	if (drm_core_check_feature(dev, DRIVER_MODESET))
		i915_kick_out_firmware_fb(dev_priv);

	pci_set_master(dev->pdev);

	/* overlay on gen2 is broken and can't address above 1G */
	if (IS_GEN2(dev))
		dma_set_coherent_mask(&dev->pdev->dev, DMA_BIT_MASK(30));

	/* 965GM sometimes incorrectly writes to hardware status page (HWS)
	 * using 32bit addressing, overwriting memory if HWS is located
	 * above 4GB.
	 *
	 * The documentation also mentions an issue with undefined
	 * behaviour if any general state is accessed within a page above 4GB,
	 * which also needs to be handled carefully.
	 */
	if (IS_BROADWATER(dev) || IS_CRESTLINE(dev))
		dma_set_coherent_mask(&dev->pdev->dev, DMA_BIT_MASK(32));
#endif

	aperture_size = dev_priv->gtt.mappable_end;

#ifdef __linux__
	dev_priv->gtt.mappable =
		io_mapping_create_wc(dev_priv->gtt.mappable_base,
				     aperture_size);
	if (dev_priv->gtt.mappable == NULL) {
		ret = -EIO;
		goto out_gtt;
	}

	dev_priv->gtt.mtrr = arch_phys_wc_add(dev_priv->gtt.mappable_base,
					      aperture_size);
#else
	/* XXX would be a lot nicer to get agp info before now */
	uvm_page_physload(atop(dev_priv->gtt.mappable_base),
	    atop(dev_priv->gtt.mappable_base + aperture_size),
	    atop(dev_priv->gtt.mappable_base),
	    atop(dev_priv->gtt.mappable_base + aperture_size),
	    PHYSLOAD_DEVICE);
	/* array of vm pages that physload introduced. */
	dev_priv->pgs = PHYS_TO_VM_PAGE(dev_priv->gtt.mappable_base);
	KASSERT(dev_priv->pgs != NULL);
	/*
	 * XXX mark all pages write combining so user mmaps get the right
	 * bits. We really need a proper MI api for doing this, but for now
	 * this allows us to use PAT where available.
	 */
	for (i = 0; i < atop(aperture_size); i++)
		atomic_setbits_int(&(dev_priv->pgs[i].pg_flags), PG_PMAP_WC);
	if (agp_init_map(dev_priv->bst, dev_priv->gtt.mappable_base,
	    aperture_size, BUS_SPACE_MAP_LINEAR | BUS_SPACE_MAP_PREFETCHABLE,
	    &dev_priv->agph))
		panic("can't map aperture");
#endif

	/* The i915 workqueue is primarily used for batched retirement of
	 * requests (and thus managing bo) once the task has been completed
	 * by the GPU. i915_gem_retire_requests() is called directly when we
	 * need high-priority retirement, such as waiting for an explicit
	 * bo.
	 *
	 * It is also used for periodic low-priority events, such as
	 * idle-timers and recording error state.
	 *
	 * All tasks on the workqueue are expected to acquire the dev mutex
	 * so there is no point in running more than one instance of the
	 * workqueue at any time.  Use an ordered one.
	 */
#ifdef __linux__
	dev_priv->wq = alloc_ordered_workqueue("i915", 0);
	if (dev_priv->wq == NULL) {
		DRM_ERROR("Failed to create our workqueue.\n");
		ret = -ENOMEM;
		goto out_mtrrfree;
	}
#else
	dev_priv->wq = (struct workqueue_struct *)
	    taskq_create("intelrel", 1, IPL_TTY, 0);
	if (dev_priv->wq == NULL) {
		printf(": couldn't create taskq\n");
		goto out_mtrrfree;
	}
#endif

	intel_irq_init(dev);
	intel_uncore_sanitize(dev);

	/* Try to make sure MCHBAR is enabled before poking at it */
	intel_setup_mchbar(dev);
	intel_setup_gmbus(dev);
	intel_opregion_setup(dev);

	intel_setup_bios(dev);

	i915_gem_load(dev);

	/* On the 945G/GM, the chipset reports the MSI capability on the
	 * integrated graphics even though the support isn't actually there
	 * according to the published specs.  It doesn't appear to function
	 * correctly in testing on 945G.
	 * This may be a side effect of MSI having been made available for PEG
	 * and the registers being closely associated.
	 *
	 * According to chipset errata, on the 965GM, MSI interrupts may
	 * be lost or delayed, but we use them anyways to avoid
	 * stuck interrupts on some machines.
	 */
	if (IS_I945G(dev) || IS_I945GM(dev))
		pa->pa_flags &= ~PCI_FLAGS_MSI_ENABLED;

	if (pci_intr_map_msi(pa, &dev_priv->ih) != 0 &&
	    pci_intr_map(pa, &dev_priv->ih) != 0) {
		printf(": couldn't map interrupt\n");
		return;
	}

	printf(": %s", pci_intr_string(dev_priv->pc, dev_priv->ih));

	dev_priv->irqh = pci_intr_establish(dev_priv->pc, dev_priv->ih,
	    IPL_TTY, inteldrm_intr, dev_priv, dev_priv->sc_dev.dv_xname);
	if (dev_priv->irqh == NULL) {
		printf(": couldn't  establish interrupt\n");
		return;
	}

	dev_priv->num_plane = 1;
	if (IS_VALLEYVIEW(dev))
		dev_priv->num_plane = 2;

	if (INTEL_INFO(dev)->num_pipes) {
		ret = drm_vblank_init(dev, INTEL_INFO(dev)->num_pipes);
		if (ret)
			goto out_gem_unload;
	}

	intel_power_domains_init(dev);

	if (drm_core_check_feature(dev, DRIVER_MODESET)) {
		ret = i915_load_modeset_init(dev);
		if (ret < 0) {
			DRM_ERROR("failed to init modeset\n");
			goto out_power_well;
		}
	} else {
		/* Start out suspended in ums mode. */
		dev_priv->ums.mm_suspended = 1;
	}

#ifdef __linux__
	i915_setup_sysfs(dev);
#endif

	if (INTEL_INFO(dev)->num_pipes) {
		/* Must be done after probing outputs */
		intel_opregion_init(dev);
#ifdef __linux__
		acpi_video_register();
#endif
	}

	if (IS_GEN5(dev))
		intel_gpu_ips_init(dev_priv);

	intel_init_runtime_pm(dev_priv);

#if 1
{
	extern int wsdisplay_console_initted;
	struct wsemuldisplaydev_attach_args aa;
	struct rasops_info *ri = &dev_priv->ro;

	if (ri->ri_bits == NULL)
		return;

	intel_fbdev_restore_mode(dev);

	ri->ri_flg = RI_CENTER | RI_WRONLY | RI_VCONS;
	ri->ri_hw = dev_priv;
	rasops_init(ri, 160, 160);

	task_set(&dev_priv->switchtask, inteldrm_doswitch, dev_priv);

	inteldrm_stdscreen.capabilities = ri->ri_caps;
	inteldrm_stdscreen.nrows = ri->ri_rows;
	inteldrm_stdscreen.ncols = ri->ri_cols;
	inteldrm_stdscreen.textops = &ri->ri_ops;
	inteldrm_stdscreen.fontwidth = ri->ri_font->fontwidth;
	inteldrm_stdscreen.fontheight = ri->ri_font->fontheight;

	aa.console = 0;
	aa.scrdata = &inteldrm_screenlist;
	aa.accessops = &inteldrm_accessops;
	aa.accesscookie = dev_priv;
	aa.defaultscreens = 0;

	if (wsdisplay_console_initted) {
		long defattr;

		ri->ri_ops.alloc_attr(ri->ri_active, 0, 0, 0, &defattr);
		wsdisplay_cnattach(&inteldrm_stdscreen, ri->ri_active,
		    0, 0, defattr);
		aa.console = 1;
	}

	printf(", %dx%d\n", ri->ri_width, ri->ri_height);

	vga_sc->sc_type = -1;
	config_found(parent, &aa, wsemuldisplaydevprint);
}
#endif

out_power_well:
out_gem_unload:
out_mtrrfree:
out_regs:
	return;
}

int
inteldrm_detach(struct device *self, int flags)
{
	struct inteldrm_softc	*dev_priv = (struct inteldrm_softc *)self;
	struct drm_device	*dev = dev_priv->dev;

	/* this will quiesce any dma that's going on and kill the timeouts. */
	if (dev_priv->dev != NULL) {
		config_detach((struct device *)dev_priv->dev, flags);
		dev_priv->dev = NULL;
	}

	if (IS_I9XX(dev) && dev_priv->ifp.i9xx.bsh != 0) {
		bus_space_unmap(dev_priv->ifp.i9xx.bst, dev_priv->ifp.i9xx.bsh,
		    PAGE_SIZE);
	} else if ((IS_I830(dev) || IS_845G(dev) || IS_I85X(dev) ||
	    IS_I865G(dev)) && dev_priv->ifp.i8xx.kva != NULL) {
		bus_dmamem_unmap(dev_priv->dmat, dev_priv->ifp.i8xx.kva,
		     PAGE_SIZE);
		bus_dmamem_free(dev_priv->dmat, &dev_priv->ifp.i8xx.seg, 1);
	}

	pci_intr_disestablish(dev_priv->pc, dev_priv->irqh);

	if (dev_priv->regs != NULL)
		vga_pci_bar_unmap(dev_priv->regs);

	return (0);
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
		rv = config_activate_children(self, act);
		i915_drm_freeze(dev);
		break;
	case DVACT_SUSPEND:
		break;
	case DVACT_RESUME:
		break;
	case DVACT_WAKEUP:
		i915_drm_thaw_early(dev);
		i915_drm_thaw(dev);
		intel_fbdev_restore_mode(dev);
		rv = config_activate_children(self, act);
		break;
	}

	return (rv);
}

struct cfattach inteldrm_ca = {
	sizeof(struct inteldrm_softc), inteldrm_probe, inteldrm_attach,
	inteldrm_detach, inteldrm_activate
};

struct cfdriver inteldrm_cd = {
	0, "inteldrm", DV_DULL
};

/*
 * We're intel IGD, bus 0 function 0 dev 0 should be the GMCH, so it should
 * be Intel
 */
int
inteldrm_gmch_match(struct pci_attach_args *pa)
{
	if (pa->pa_bus == 0 && pa->pa_device == 0 && pa->pa_function == 0 &&
	    PCI_VENDOR(pa->pa_id) == PCI_VENDOR_INTEL &&
	    PCI_CLASS(pa->pa_class) == PCI_CLASS_BRIDGE &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_BRIDGE_HOST)
		return (1);
	return (0);
}

void
i915_alloc_ifp(struct inteldrm_softc *dev_priv, struct pci_attach_args *bpa)
{
	bus_addr_t	addr;
	u_int32_t	reg;

	dev_priv->ifp.i9xx.bst = bpa->pa_memt;

	reg = pci_conf_read(bpa->pa_pc, bpa->pa_tag, I915_IFPADDR);
	if (reg & 0x1) {
		addr = (bus_addr_t)reg;
		addr &= ~0x1;
		/* XXX extents ... need data on whether bioses alloc or not. */
		if (bus_space_map(bpa->pa_memt, addr, PAGE_SIZE, 0,
		    &dev_priv->ifp.i9xx.bsh) != 0)
			goto nope;
		return;
	} else if (bpa->pa_memex == NULL ||
	    extent_alloc_subregion(bpa->pa_memex, 0x100000, 0xffffffff,
	    PAGE_SIZE, PAGE_SIZE, 0, 0, 0, &addr) ||
	    bus_space_map(bpa->pa_memt, addr, PAGE_SIZE, 0,
	    &dev_priv->ifp.i9xx.bsh))
		goto nope;

	pci_conf_write(bpa->pa_pc, bpa->pa_tag, I915_IFPADDR, addr | 0x1);

	return;

nope:
	dev_priv->ifp.i9xx.bsh = 0;
	printf(": no ifp ");
}

void
i965_alloc_ifp(struct inteldrm_softc *dev_priv, struct pci_attach_args *bpa)
{
	bus_addr_t	addr;
	u_int32_t	lo, hi;

	dev_priv->ifp.i9xx.bst = bpa->pa_memt;

	hi = pci_conf_read(bpa->pa_pc, bpa->pa_tag, I965_IFPADDR + 4);
	lo = pci_conf_read(bpa->pa_pc, bpa->pa_tag, I965_IFPADDR);
	if (lo & 0x1) {
		addr = (((u_int64_t)hi << 32) | lo);
		addr &= ~0x1;
		/* XXX extents ... need data on whether bioses alloc or not. */
		if (bus_space_map(bpa->pa_memt, addr, PAGE_SIZE, 0,
		    &dev_priv->ifp.i9xx.bsh) != 0)
			goto nope;
		return;
	} else if (bpa->pa_memex == NULL ||
	    extent_alloc_subregion(bpa->pa_memex, 0x100000, 0xffffffff,
	    PAGE_SIZE, PAGE_SIZE, 0, 0, 0, &addr) ||
	    bus_space_map(bpa->pa_memt, addr, PAGE_SIZE, 0,
	    &dev_priv->ifp.i9xx.bsh))
		goto nope;

	pci_conf_write(bpa->pa_pc, bpa->pa_tag, I965_IFPADDR + 4,
	    upper_32_bits(addr));
	pci_conf_write(bpa->pa_pc, bpa->pa_tag, I965_IFPADDR,
	    (addr & 0xffffffff) | 0x1);

	return;

nope:
	dev_priv->ifp.i9xx.bsh = 0;
	printf(": no ifp ");
}

void
intel_gtt_chipset_setup(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct pci_attach_args bpa;

	if (INTEL_INFO(dev)->gen >= 6)
		return;

	if (pci_find_device(&bpa, inteldrm_gmch_match) == 0) {
		printf(": can't find GMCH\n");
		return;
	}

	/* Set up the IFP for chipset flushing */
	if (IS_I915G(dev) || IS_I915GM(dev) || IS_I945G(dev) ||
	    IS_I945GM(dev)) {
		i915_alloc_ifp(dev_priv, &bpa);
	} else if (INTEL_INFO(dev)->gen >= 4 || IS_G33(dev)) {
		i965_alloc_ifp(dev_priv, &bpa);
	} else {
		int nsegs;
		/*
		 * I8XX has no flush page mechanism, we fake it by writing until
		 * the cache is empty. allocate a page to scribble on
		 */
		dev_priv->ifp.i8xx.kva = NULL;
		if (bus_dmamem_alloc(dev_priv->dmat, PAGE_SIZE, 0, 0,
		    &dev_priv->ifp.i8xx.seg, 1, &nsegs, BUS_DMA_WAITOK) == 0) {
			if (bus_dmamem_map(dev_priv->dmat, &dev_priv->ifp.i8xx.seg,
			    1, PAGE_SIZE, &dev_priv->ifp.i8xx.kva, 0) != 0) {
				bus_dmamem_free(dev_priv->dmat,
				    &dev_priv->ifp.i8xx.seg, nsegs);
				dev_priv->ifp.i8xx.kva = NULL;
			}
		}
	}
}

void
intel_gtt_chipset_flush(void)
{
	struct inteldrm_softc *dev_priv = (void *)inteldrm_cd.cd_devs[0];

	/*
	 * Write to this flush page flushes the chipset write cache.
	 * The write will return when it is done.
	 */
	if (IS_I9XX(dev_priv->dev)) {
	    if (dev_priv->ifp.i9xx.bsh != 0)
		bus_space_write_4(dev_priv->ifp.i9xx.bst,
		    dev_priv->ifp.i9xx.bsh, 0, 1);
	} else {
		int i;

		wbinvd();

#define I830_HIC        0x70

		I915_WRITE(I830_HIC, (I915_READ(I830_HIC) | (1<<31)));
		for (i = 1000; i; i--) {
			if (!(I915_READ(I830_HIC) & (1<<31)))
				break;
			delay(100);
		}

	}
}

int
intel_gmch_probe(struct pci_dev *bridge_pdev, struct pci_dev *gpu_pdev,
		 void *bridge)
{
	return 1;
}

void
intel_gtt_get(size_t *gtt_total, size_t *stolen_size,
    phys_addr_t *mappable_base, unsigned long *mappable_end)
{
	struct inteldrm_softc *dev_priv = (void *)inteldrm_cd.cd_devs[0];
	struct agp_info *ai = &dev_priv->dev->agp->info;
	
	*gtt_total = ai->ai_aperture_size;
	*stolen_size = 0;
	*mappable_base = ai->ai_aperture_base;
	*mappable_end = ai->ai_aperture_size;
}

void
intel_gmch_remove(void)
{
}
