/* i915_drv.h -- Private header for the I915 driver -*- linux-c -*-
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

#ifndef _I915_DRV_H_
#define _I915_DRV_H_

#include <uapi/drm/i915_drm.h>
#include <uapi/drm/drm_fourcc.h>

#include <linux/io-mapping.h>
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>
#include <linux/backlight.h>
#include <linux/hash.h>
#include <linux/intel-iommu.h>
#include <linux/kref.h>
#include <linux/mm_types.h>
#include <linux/perf_event.h>
#include <linux/pm_qos.h>
#include <linux/reservation.h>
#include <linux/shmem_fs.h>

#include <drm/drmP.h>
#ifdef __linux__
#include <drm/intel-gtt.h>
#include <drm/drm_legacy.h> /* for struct drm_dma_handle */
#include <drm/drm_auth.h>
#endif
#include <drm/drm_gem.h>
#include <drm/drm_cache.h>

#include "acpi.h"
#if NACPI > 0
#define CONFIG_ACPI
#endif

#include "drm.h"
#include "vga.h"

struct inteldrm_softc;
#define drm_i915_private inteldrm_softc

#include "i915_params.h"
#include "i915_reg.h"
#include "i915_utils.h"

#include "intel_bios.h"
#include "intel_device_info.h"
#include "intel_display.h"
#include "intel_dpll_mgr.h"
#include "intel_lrc.h"
#include "intel_opregion.h"
#include "intel_ringbuffer.h"
#include "intel_uncore.h"
#include "intel_wopcm.h"
#include "intel_uc.h"

#include "i915_gem.h"
#include "i915_gem_context.h"
#include "i915_gem_fence_reg.h"
#include "i915_gem_object.h"
#include "i915_gem_gtt.h"
#include "i915_gpu_error.h"
#include "i915_request.h"
#include "i915_scheduler.h"
#include "i915_timeline.h"
#include "i915_vma.h"

#include "intel_gvt.h"

#include <dev/ic/mc6845reg.h>
#include <dev/ic/pcdisplayvar.h>
#include <dev/ic/vgareg.h>
#include <dev/ic/vgavar.h>

#include <sys/task.h>
#include <dev/pci/vga_pcivar.h>
#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/rasops/rasops.h>

extern int intel_enable_gtt(void);
extern void intel_gtt_chipset_flush(void);
extern int intel_gmch_probe(struct pci_dev *, struct pci_dev *, void *);
extern void intel_gtt_get(u64 *, phys_addr_t *, resource_size_t *);
extern void intel_gtt_insert_sg_entries(struct sg_table *, unsigned int,
					unsigned int);
extern void intel_gtt_insert_page(dma_addr_t, unsigned int, unsigned int);
extern void intel_gtt_clear_range(unsigned int, unsigned int);
extern void intel_gmch_remove(void);

/*
 * The Bridge device's PCI config space has information about the
 * fb aperture size and the amount of pre-reserved memory.
 * This is all handled in the intel-gtt.ko module. i915.ko only
 * cares about the vga bit for the vga rbiter.
 */
#define INTEL_GMCH_CTRL		0x52
#define INTEL_GMCH_VGA_DISABLE  (1 << 1)
#define SNB_GMCH_CTRL		0x50
#define    SNB_GMCH_GGMS_SHIFT	8 /* GTT Graphics Memory Size */
#define    SNB_GMCH_GGMS_MASK	0x3
#define    SNB_GMCH_GMS_SHIFT   3 /* Graphics Mode Select */
#define    SNB_GMCH_GMS_MASK    0x1f
#define    BDW_GMCH_GGMS_SHIFT	6
#define    BDW_GMCH_GGMS_MASK	0x3
#define    BDW_GMCH_GMS_SHIFT   8
#define    BDW_GMCH_GMS_MASK    0xff

#define I830_GMCH_CTRL			0x52

#define I830_GMCH_GMS_MASK		0x70
#define I830_GMCH_GMS_LOCAL		0x10
#define I830_GMCH_GMS_STOLEN_512	0x20
#define I830_GMCH_GMS_STOLEN_1024	0x30
#define I830_GMCH_GMS_STOLEN_8192	0x40

#define I855_GMCH_GMS_MASK		0xF0
#define I855_GMCH_GMS_STOLEN_0M		0x0
#define I855_GMCH_GMS_STOLEN_1M		(0x1 << 4)
#define I855_GMCH_GMS_STOLEN_4M		(0x2 << 4)
#define I855_GMCH_GMS_STOLEN_8M		(0x3 << 4)
#define I855_GMCH_GMS_STOLEN_16M	(0x4 << 4)
#define I855_GMCH_GMS_STOLEN_32M	(0x5 << 4)
#define I915_GMCH_GMS_STOLEN_48M	(0x6 << 4)
#define I915_GMCH_GMS_STOLEN_64M	(0x7 << 4)
#define G33_GMCH_GMS_STOLEN_128M	(0x8 << 4)
#define G33_GMCH_GMS_STOLEN_256M	(0x9 << 4)
#define INTEL_GMCH_GMS_STOLEN_96M	(0xa << 4)
#define INTEL_GMCH_GMS_STOLEN_160M	(0xb << 4)
#define INTEL_GMCH_GMS_STOLEN_224M	(0xc << 4)
#define INTEL_GMCH_GMS_STOLEN_352M	(0xd << 4)

#define I830_DRB3		0x63
#define I85X_DRB3		0x43
#define I865_TOUD		0xc4

#define I830_ESMRAMC		0x91
#define I845_ESMRAMC		0x9e
#define I85X_ESMRAMC		0x61
#define    TSEG_ENABLE		(1 << 0)
#define    I830_TSEG_SIZE_512K	(0 << 1)
#define    I830_TSEG_SIZE_1M	(1 << 1)
#define    I845_TSEG_SIZE_MASK	(3 << 1)
#define    I845_TSEG_SIZE_512K	(2 << 1)
#define    I845_TSEG_SIZE_1M	(3 << 1)

struct intel_gtt {
	/* Size of memory reserved for graphics by the BIOS */
	unsigned int stolen_size;
	/* Total number of gtt entries. */
	unsigned int gtt_total_entries;
	/* Part of the gtt that is mappable by the cpu, for those chips where
	 * this is not the full gtt. */
	unsigned int gtt_mappable_entries;
	/* Share the scratch page dma with ppgtts. */
	bus_addr_t scratch_page_dma;
	struct drm_dmamem *scratch_page;
	/* for ppgtt PDE access */
	bus_space_handle_t gtt;
	/* needed for ioremap in drm/i915 */
	bus_addr_t gma_bus_addr;
};

/* General customization:
 */

#define DRIVER_NAME		"i915"
#define DRIVER_DESC		"Intel Graphics"
#define DRIVER_DATE		"20180719"
#define DRIVER_TIMESTAMP	1532015279

/* Use I915_STATE_WARN(x) and I915_STATE_WARN_ON() (rather than WARN() and
 * WARN_ON()) for hw state sanity checks to check for unexpected conditions
 * which may not necessarily be a user visible problem.  This will either
 * WARN() or DRM_ERROR() depending on the verbose_checks moduleparam, to
 * enable distros and users to tailor their preferred amount of i915 abrt
 * spam.
 */
#define I915_STATE_WARN(condition, format...) ({			\
	int __ret_warn_on = !!(condition);				\
	if (unlikely(__ret_warn_on))					\
		if (!WARN(i915_modparams.verbose_state_checks, format))	\
			DRM_ERROR(format);				\
	unlikely(__ret_warn_on);					\
})

#define I915_STATE_WARN_ON(x)						\
	I915_STATE_WARN((x), "%s", "WARN_ON(" __stringify(x) ")")

#if IS_ENABLED(CONFIG_DRM_I915_DEBUG)

bool __i915_inject_load_failure(const char *func, int line);
#define i915_inject_load_failure() \
	__i915_inject_load_failure(__func__, __LINE__)

bool i915_error_injected(void);

#else

#define i915_inject_load_failure() false
#define i915_error_injected() false

#endif

#define i915_load_error(i915, fmt, ...)					 \
	__i915_printk(i915, i915_error_injected() ? KERN_DEBUG : KERN_ERR, \
		      fmt, ##__VA_ARGS__)

typedef struct {
	uint32_t val;
} uint_fixed_16_16_t;

#define FP_16_16_MAX ({ \
	uint_fixed_16_16_t fp; \
	fp.val = UINT_MAX; \
	fp; \
})

static inline bool is_fixed16_zero(uint_fixed_16_16_t val)
{
	if (val.val == 0)
		return true;
	return false;
}

static inline uint_fixed_16_16_t u32_to_fixed16(uint32_t val)
{
	uint_fixed_16_16_t fp;

	WARN_ON(val > U16_MAX);

	fp.val = val << 16;
	return fp;
}

static inline uint32_t fixed16_to_u32_round_up(uint_fixed_16_16_t fp)
{
	return DIV_ROUND_UP(fp.val, 1 << 16);
}

static inline uint32_t fixed16_to_u32(uint_fixed_16_16_t fp)
{
	return fp.val >> 16;
}

static inline uint_fixed_16_16_t min_fixed16(uint_fixed_16_16_t min1,
						 uint_fixed_16_16_t min2)
{
	uint_fixed_16_16_t min;

	min.val = min(min1.val, min2.val);
	return min;
}

static inline uint_fixed_16_16_t max_fixed16(uint_fixed_16_16_t max1,
						 uint_fixed_16_16_t max2)
{
	uint_fixed_16_16_t max;

	max.val = max(max1.val, max2.val);
	return max;
}

static inline uint_fixed_16_16_t clamp_u64_to_fixed16(uint64_t val)
{
	uint_fixed_16_16_t fp;
	WARN_ON(val > U32_MAX);
	fp.val = (uint32_t) val;
	return fp;
}

static inline uint32_t div_round_up_fixed16(uint_fixed_16_16_t val,
					    uint_fixed_16_16_t d)
{
	return DIV_ROUND_UP(val.val, d.val);
}

static inline uint32_t mul_round_up_u32_fixed16(uint32_t val,
						uint_fixed_16_16_t mul)
{
	uint64_t intermediate_val;

	intermediate_val = (uint64_t) val * mul.val;
	intermediate_val = DIV_ROUND_UP_ULL(intermediate_val, 1 << 16);
	WARN_ON(intermediate_val > U32_MAX);
	return (uint32_t) intermediate_val;
}

static inline uint_fixed_16_16_t mul_fixed16(uint_fixed_16_16_t val,
					     uint_fixed_16_16_t mul)
{
	uint64_t intermediate_val;

	intermediate_val = (uint64_t) val.val * mul.val;
	intermediate_val = intermediate_val >> 16;
	return clamp_u64_to_fixed16(intermediate_val);
}

static inline uint_fixed_16_16_t div_fixed16(uint32_t val, uint32_t d)
{
	uint64_t interm_val;

	interm_val = (uint64_t)val << 16;
	interm_val = DIV_ROUND_UP_ULL(interm_val, d);
	return clamp_u64_to_fixed16(interm_val);
}

static inline uint32_t div_round_up_u32_fixed16(uint32_t val,
						uint_fixed_16_16_t d)
{
	uint64_t interm_val;

	interm_val = (uint64_t)val << 16;
	interm_val = DIV_ROUND_UP_ULL(interm_val, d.val);
	WARN_ON(interm_val > U32_MAX);
	return (uint32_t) interm_val;
}

static inline uint_fixed_16_16_t mul_u32_fixed16(uint32_t val,
						     uint_fixed_16_16_t mul)
{
	uint64_t intermediate_val;

	intermediate_val = (uint64_t) val * mul.val;
	return clamp_u64_to_fixed16(intermediate_val);
}

static inline uint_fixed_16_16_t add_fixed16(uint_fixed_16_16_t add1,
					     uint_fixed_16_16_t add2)
{
	uint64_t interm_sum;

	interm_sum = (uint64_t) add1.val + add2.val;
	return clamp_u64_to_fixed16(interm_sum);
}

static inline uint_fixed_16_16_t add_fixed16_u32(uint_fixed_16_16_t add1,
						 uint32_t add2)
{
	uint64_t interm_sum;
	uint_fixed_16_16_t interm_add2 = u32_to_fixed16(add2);

	interm_sum = (uint64_t) add1.val + interm_add2.val;
	return clamp_u64_to_fixed16(interm_sum);
}

enum hpd_pin {
	HPD_NONE = 0,
	HPD_TV = HPD_NONE,     /* TV is known to be unreliable */
	HPD_CRT,
	HPD_SDVO_B,
	HPD_SDVO_C,
	HPD_PORT_A,
	HPD_PORT_B,
	HPD_PORT_C,
	HPD_PORT_D,
	HPD_PORT_E,
	HPD_PORT_F,
	HPD_NUM_PINS
};

#define for_each_hpd_pin(__pin) \
	for ((__pin) = (HPD_NONE + 1); (__pin) < HPD_NUM_PINS; (__pin)++)

#define HPD_STORM_DEFAULT_THRESHOLD 5

struct i915_hotplug {
	struct work_struct hotplug_work;

	struct {
		unsigned long last_jiffies;
		int count;
		enum {
			HPD_ENABLED = 0,
			HPD_DISABLED = 1,
			HPD_MARK_DISABLED = 2
		} state;
	} stats[HPD_NUM_PINS];
	u32 event_bits;
	struct delayed_work reenable_work;

	u32 long_port_mask;
	u32 short_port_mask;
	struct work_struct dig_port_work;

	struct work_struct poll_init_work;
	bool poll_enabled;

	unsigned int hpd_storm_threshold;

	/*
	 * if we get a HPD irq from DP and a HPD irq from non-DP
	 * the non-DP HPD could block the workqueue on a mode config
	 * mutex getting, that userspace may have taken. However
	 * userspace is waiting on the DP workqueue to run which is
	 * blocked behind the non-DP one.
	 */
	struct workqueue_struct *dp_wq;
};

#define I915_GEM_GPU_DOMAINS \
	(I915_GEM_DOMAIN_RENDER | \
	 I915_GEM_DOMAIN_SAMPLER | \
	 I915_GEM_DOMAIN_COMMAND | \
	 I915_GEM_DOMAIN_INSTRUCTION | \
	 I915_GEM_DOMAIN_VERTEX)

struct i915_mm_struct;
struct i915_mmu_object;

struct drm_i915_file_private {
	struct drm_i915_private *dev_priv;
	struct drm_file *file;

	struct {
		spinlock_t lock;
		struct list_head request_list;
/* 20ms is a fairly arbitrary limit (greater than the average frame time)
 * chosen to prevent the CPU getting more than a frame ahead of the GPU
 * (when using lax throttling for the frontbuffer). We also use it to
 * offer free GPU waitboosts for severely congested workloads.
 */
#define DRM_I915_THROTTLE_JIFFIES msecs_to_jiffies(20)
	} mm;
	struct idr context_idr;

	struct intel_rps_client {
		atomic_t boosts;
	} rps_client;

	unsigned int bsd_engine;

/*
 * Every context ban increments per client ban score. Also
 * hangs in short succession increments ban score. If ban threshold
 * is reached, client is considered banned and submitting more work
 * will fail. This is a stop gap measure to limit the badly behaving
 * clients access to gpu. Note that unbannable contexts never increment
 * the client ban score.
 */
#define I915_CLIENT_SCORE_HANG_FAST	1
#define   I915_CLIENT_FAST_HANG_JIFFIES (60 * HZ)
#define I915_CLIENT_SCORE_CONTEXT_BAN   3
#define I915_CLIENT_SCORE_BANNED	9
	/** ban_score: Accumulated score of all ctx bans and fast hangs. */
	atomic_t ban_score;
	unsigned long hang_timestamp;
};

/* Interface history:
 *
 * 1.1: Original.
 * 1.2: Add Power Management
 * 1.3: Add vblank support
 * 1.4: Fix cmdbuffer path, add heap destroy
 * 1.5: Add vblank pipe configuration
 * 1.6: - New ioctl for scheduling buffer swaps on vertical blank
 *      - Support vertical blank on secondary display pipe
 */
#define DRIVER_MAJOR		1
#define DRIVER_MINOR		6
#define DRIVER_PATCHLEVEL	0

struct intel_overlay;
struct intel_overlay_error_state;

struct sdvo_device_mapping {
	u8 initialized;
	u8 dvo_port;
	u8 slave_addr;
	u8 dvo_wiring;
	u8 i2c_pin;
	u8 ddc_pin;
};

struct intel_connector;
struct intel_encoder;
struct intel_atomic_state;
struct intel_crtc_state;
struct intel_initial_plane_config;
struct intel_crtc;
struct intel_limit;
struct dpll;
struct intel_cdclk_state;

struct drm_i915_display_funcs {
	void (*get_cdclk)(struct drm_i915_private *dev_priv,
			  struct intel_cdclk_state *cdclk_state);
	void (*set_cdclk)(struct drm_i915_private *dev_priv,
			  const struct intel_cdclk_state *cdclk_state);
	int (*get_fifo_size)(struct drm_i915_private *dev_priv,
			     enum i9xx_plane_id i9xx_plane);
	int (*compute_pipe_wm)(struct intel_crtc_state *cstate);
	int (*compute_intermediate_wm)(struct drm_device *dev,
				       struct intel_crtc *intel_crtc,
				       struct intel_crtc_state *newstate);
	void (*initial_watermarks)(struct intel_atomic_state *state,
				   struct intel_crtc_state *cstate);
	void (*atomic_update_watermarks)(struct intel_atomic_state *state,
					 struct intel_crtc_state *cstate);
	void (*optimize_watermarks)(struct intel_atomic_state *state,
				    struct intel_crtc_state *cstate);
	int (*compute_global_watermarks)(struct drm_atomic_state *state);
	void (*update_wm)(struct intel_crtc *crtc);
	int (*modeset_calc_cdclk)(struct drm_atomic_state *state);
	/* Returns the active state of the crtc, and if the crtc is active,
	 * fills out the pipe-config with the hw state. */
	bool (*get_pipe_config)(struct intel_crtc *,
				struct intel_crtc_state *);
	void (*get_initial_plane_config)(struct intel_crtc *,
					 struct intel_initial_plane_config *);
	int (*crtc_compute_clock)(struct intel_crtc *crtc,
				  struct intel_crtc_state *crtc_state);
	void (*crtc_enable)(struct intel_crtc_state *pipe_config,
			    struct drm_atomic_state *old_state);
	void (*crtc_disable)(struct intel_crtc_state *old_crtc_state,
			     struct drm_atomic_state *old_state);
	void (*update_crtcs)(struct drm_atomic_state *state);
	void (*audio_codec_enable)(struct intel_encoder *encoder,
				   const struct intel_crtc_state *crtc_state,
				   const struct drm_connector_state *conn_state);
	void (*audio_codec_disable)(struct intel_encoder *encoder,
				    const struct intel_crtc_state *old_crtc_state,
				    const struct drm_connector_state *old_conn_state);
	void (*fdi_link_train)(struct intel_crtc *crtc,
			       const struct intel_crtc_state *crtc_state);
	void (*init_clock_gating)(struct drm_i915_private *dev_priv);
	void (*hpd_irq_setup)(struct drm_i915_private *dev_priv);
	/* clock updates for mode set */
	/* cursor updates */
	/* render clock increase/decrease */
	/* display clock increase/decrease */
	/* pll clock increase/decrease */

	void (*load_csc_matrix)(struct drm_crtc_state *crtc_state);
	void (*load_luts)(struct drm_crtc_state *crtc_state);
};

#define CSR_VERSION(major, minor)	((major) << 16 | (minor))
#define CSR_VERSION_MAJOR(version)	((version) >> 16)
#define CSR_VERSION_MINOR(version)	((version) & 0xffff)

struct intel_csr {
	struct work_struct work;
	const char *fw_path;
	uint32_t *dmc_payload;
	uint32_t dmc_fw_size;
	uint32_t version;
	uint32_t mmio_count;
	i915_reg_t mmioaddr[8];
	uint32_t mmiodata[8];
	uint32_t dc_state;
	uint32_t allowed_dc_mask;
};

enum i915_cache_level {
	I915_CACHE_NONE = 0,
	I915_CACHE_LLC, /* also used for snoopable memory on non-LLC */
	I915_CACHE_L3_LLC, /* gen7+, L3 sits between the domain specifc
			      caches, eg sampler/render caches, and the
			      large Last-Level-Cache. LLC is coherent with
			      the CPU, but L3 is only visible to the GPU. */
	I915_CACHE_WT, /* hsw:gt3e WriteThrough for scanouts */
};

#define I915_COLOR_UNEVICTABLE (-1) /* a non-vma sharing the address space */

enum fb_op_origin {
	ORIGIN_GTT,
	ORIGIN_CPU,
	ORIGIN_CS,
	ORIGIN_FLIP,
	ORIGIN_DIRTYFB,
};

struct intel_fbc {
	/* This is always the inner lock when overlapping with struct_mutex and
	 * it's the outer lock when overlapping with stolen_lock. */
	struct rwlock lock;
	unsigned threshold;
	unsigned int possible_framebuffer_bits;
	unsigned int busy_bits;
	unsigned int visible_pipes_mask;
	struct intel_crtc *crtc;

	struct drm_mm_node compressed_fb;
	struct drm_mm_node *compressed_llb;

	bool false_color;

	bool enabled;
	bool active;
	bool flip_pending;

	bool underrun_detected;
	struct work_struct underrun_work;

	/*
	 * Due to the atomic rules we can't access some structures without the
	 * appropriate locking, so we cache information here in order to avoid
	 * these problems.
	 */
	struct intel_fbc_state_cache {
		struct i915_vma *vma;
		unsigned long flags;

		struct {
			unsigned int mode_flags;
			uint32_t hsw_bdw_pixel_rate;
		} crtc;

		struct {
			unsigned int rotation;
			int src_w;
			int src_h;
			bool visible;
			/*
			 * Display surface base address adjustement for
			 * pageflips. Note that on gen4+ this only adjusts up
			 * to a tile, offsets within a tile are handled in
			 * the hw itself (with the TILEOFF register).
			 */
			int adjusted_x;
			int adjusted_y;

			int y;
		} plane;

		struct {
			const struct drm_format_info *format;
			unsigned int stride;
		} fb;
	} state_cache;

	/*
	 * This structure contains everything that's relevant to program the
	 * hardware registers. When we want to figure out if we need to disable
	 * and re-enable FBC for a new configuration we just check if there's
	 * something different in the struct. The genx_fbc_activate functions
	 * are supposed to read from it in order to program the registers.
	 */
	struct intel_fbc_reg_params {
		struct i915_vma *vma;
		unsigned long flags;

		struct {
			enum pipe pipe;
			enum i9xx_plane_id i9xx_plane;
			unsigned int fence_y_offset;
		} crtc;

		struct {
			const struct drm_format_info *format;
			unsigned int stride;
		} fb;

		int cfb_size;
		unsigned int gen9_wa_cfb_stride;
	} params;

	const char *no_fbc_reason;
};

/*
 * HIGH_RR is the highest eDP panel refresh rate read from EDID
 * LOW_RR is the lowest eDP panel refresh rate found from EDID
 * parsing for same resolution.
 */
enum drrs_refresh_rate_type {
	DRRS_HIGH_RR,
	DRRS_LOW_RR,
	DRRS_MAX_RR, /* RR count */
};

enum drrs_support_type {
	DRRS_NOT_SUPPORTED = 0,
	STATIC_DRRS_SUPPORT = 1,
	SEAMLESS_DRRS_SUPPORT = 2
};

struct intel_dp;
struct i915_drrs {
	struct rwlock mutex;
	struct delayed_work work;
	struct intel_dp *dp;
	unsigned busy_frontbuffer_bits;
	enum drrs_refresh_rate_type refresh_rate_type;
	enum drrs_support_type type;
};

struct i915_psr {
	struct rwlock lock;
	bool sink_support;
	struct intel_dp *enabled;
	bool active;
	struct work_struct work;
	unsigned busy_frontbuffer_bits;
	bool sink_psr2_support;
	bool link_standby;
	bool colorimetry_support;
	bool alpm;
	bool psr2_enabled;
	u8 sink_sync_latency;
	bool debug;
	ktime_t last_entry_attempt;
	ktime_t last_exit;
};

enum intel_pch {
	PCH_NONE = 0,	/* No PCH present */
	PCH_IBX,	/* Ibexpeak PCH */
	PCH_CPT,	/* Cougarpoint/Pantherpoint PCH */
	PCH_LPT,	/* Lynxpoint/Wildcatpoint PCH */
	PCH_SPT,        /* Sunrisepoint PCH */
	PCH_KBP,        /* Kaby Lake PCH */
	PCH_CNP,        /* Cannon Lake PCH */
	PCH_ICP,	/* Ice Lake PCH */
	PCH_NOP,	/* PCH without south display */
};

enum intel_sbi_destination {
	SBI_ICLK,
	SBI_MPHY,
};

#define QUIRK_LVDS_SSC_DISABLE (1<<1)
#define QUIRK_INVERT_BRIGHTNESS (1<<2)
#define QUIRK_BACKLIGHT_PRESENT (1<<3)
#define QUIRK_PIN_SWIZZLED_PAGES (1<<5)
#define QUIRK_INCREASE_T12_DELAY (1<<6)
#define QUIRK_INCREASE_DDI_DISABLED_TIME (1<<7)

struct intel_fbdev;
struct intel_fbc_work;

struct intel_gmbus {
	struct i2c_adapter adapter;
#define GMBUS_FORCE_BIT_RETRY (1U << 31)
	u32 force_bit;
	u32 reg0;
	i915_reg_t gpio_reg;
	struct i2c_algo_bit_data bit_algo;
	struct drm_i915_private *dev_priv;
};

struct i915_suspend_saved_registers {
	u32 saveDSPARB;
	u32 saveFBC_CONTROL;
	u32 saveCACHE_MODE_0;
	u32 saveMI_ARB_STATE;
	u32 saveSWF0[16];
	u32 saveSWF1[16];
	u32 saveSWF3[3];
	uint64_t saveFENCE[I915_MAX_NUM_FENCES];
	u32 savePCH_PORT_HOTPLUG;
	u16 saveGCDGMBUS;
};

struct vlv_s0ix_state {
	/* GAM */
	u32 wr_watermark;
	u32 gfx_prio_ctrl;
	u32 arb_mode;
	u32 gfx_pend_tlb0;
	u32 gfx_pend_tlb1;
	u32 lra_limits[GEN7_LRA_LIMITS_REG_NUM];
	u32 media_max_req_count;
	u32 gfx_max_req_count;
	u32 render_hwsp;
	u32 ecochk;
	u32 bsd_hwsp;
	u32 blt_hwsp;
	u32 tlb_rd_addr;

	/* MBC */
	u32 g3dctl;
	u32 gsckgctl;
	u32 mbctl;

	/* GCP */
	u32 ucgctl1;
	u32 ucgctl3;
	u32 rcgctl1;
	u32 rcgctl2;
	u32 rstctl;
	u32 misccpctl;

	/* GPM */
	u32 gfxpause;
	u32 rpdeuhwtc;
	u32 rpdeuc;
	u32 ecobus;
	u32 pwrdwnupctl;
	u32 rp_down_timeout;
	u32 rp_deucsw;
	u32 rcubmabdtmr;
	u32 rcedata;
	u32 spare2gh;

	/* Display 1 CZ domain */
	u32 gt_imr;
	u32 gt_ier;
	u32 pm_imr;
	u32 pm_ier;
	u32 gt_scratch[GEN7_GT_SCRATCH_REG_NUM];

	/* GT SA CZ domain */
	u32 tilectl;
	u32 gt_fifoctl;
	u32 gtlc_wake_ctrl;
	u32 gtlc_survive;
	u32 pmwgicz;

	/* Display 2 CZ domain */
	u32 gu_ctl0;
	u32 gu_ctl1;
	u32 pcbr;
	u32 clock_gate_dis2;
};

struct intel_rps_ei {
	ktime_t ktime;
	u32 render_c0;
	u32 media_c0;
};

struct intel_rps {
	/*
	 * work, interrupts_enabled and pm_iir are protected by
	 * dev_priv->irq_lock
	 */
	struct work_struct work;
	bool interrupts_enabled;
	u32 pm_iir;

	/* PM interrupt bits that should never be masked */
	u32 pm_intrmsk_mbz;

	/* Frequencies are stored in potentially platform dependent multiples.
	 * In other words, *_freq needs to be multiplied by X to be interesting.
	 * Soft limits are those which are used for the dynamic reclocking done
	 * by the driver (raise frequencies under heavy loads, and lower for
	 * lighter loads). Hard limits are those imposed by the hardware.
	 *
	 * A distinction is made for overclocking, which is never enabled by
	 * default, and is considered to be above the hard limit if it's
	 * possible at all.
	 */
	u8 cur_freq;		/* Current frequency (cached, may not == HW) */
	u8 min_freq_softlimit;	/* Minimum frequency permitted by the driver */
	u8 max_freq_softlimit;	/* Max frequency permitted by the driver */
	u8 max_freq;		/* Maximum frequency, RP0 if not overclocking */
	u8 min_freq;		/* AKA RPn. Minimum frequency */
	u8 boost_freq;		/* Frequency to request when wait boosting */
	u8 idle_freq;		/* Frequency to request when we are idle */
	u8 efficient_freq;	/* AKA RPe. Pre-determined balanced frequency */
	u8 rp1_freq;		/* "less than" RP0 power/freqency */
	u8 rp0_freq;		/* Non-overclocked max frequency. */
	u16 gpll_ref_freq;	/* vlv/chv GPLL reference frequency */

	int last_adj;

	struct {
		struct rwlock mutex;

		enum { LOW_POWER, BETWEEN, HIGH_POWER } mode;
		unsigned int interactive;

		u8 up_threshold; /* Current %busy required to uplock */
		u8 down_threshold; /* Current %busy required to downclock */
	} power;

	bool enabled;
	atomic_t num_waiters;
	atomic_t boosts;

	/* manual wa residency calculations */
	struct intel_rps_ei ei;
};

struct intel_rc6 {
	bool enabled;
	u64 prev_hw_residency[4];
	u64 cur_residency[4];
};

struct intel_llc_pstate {
	bool enabled;
};

struct intel_gen6_power_mgmt {
	struct intel_rps rps;
	struct intel_rc6 rc6;
	struct intel_llc_pstate llc_pstate;
};

/* defined intel_pm.c */
extern spinlock_t mchdev_lock;

struct intel_ilk_power_mgmt {
	u8 cur_delay;
	u8 min_delay;
	u8 max_delay;
	u8 fmax;
	u8 fstart;

	u64 last_count1;
	unsigned long last_time1;
	unsigned long chipset_power;
	u64 last_count2;
	u64 last_time2;
	unsigned long gfx_power;
	u8 corr;

	int c_m;
	int r_t;
};

struct drm_i915_private;
struct i915_power_well;

struct i915_power_well_ops {
	/*
	 * Synchronize the well's hw state to match the current sw state, for
	 * example enable/disable it based on the current refcount. Called
	 * during driver init and resume time, possibly after first calling
	 * the enable/disable handlers.
	 */
	void (*sync_hw)(struct drm_i915_private *dev_priv,
			struct i915_power_well *power_well);
	/*
	 * Enable the well and resources that depend on it (for example
	 * interrupts located on the well). Called after the 0->1 refcount
	 * transition.
	 */
	void (*enable)(struct drm_i915_private *dev_priv,
		       struct i915_power_well *power_well);
	/*
	 * Disable the well and resources that depend on it. Called after
	 * the 1->0 refcount transition.
	 */
	void (*disable)(struct drm_i915_private *dev_priv,
			struct i915_power_well *power_well);
	/* Returns the hw enabled state. */
	bool (*is_enabled)(struct drm_i915_private *dev_priv,
			   struct i915_power_well *power_well);
};

/* Power well structure for haswell */
struct i915_power_well {
	const char *name;
	bool always_on;
	/* power well enable/disable usage count */
	int count;
	/* cached hw enabled state */
	bool hw_enabled;
	u64 domains;
	/* unique identifier for this power well */
	enum i915_power_well_id id;
	/*
	 * Arbitraty data associated with this power well. Platform and power
	 * well specific.
	 */
	union {
		struct {
			enum dpio_phy phy;
		} bxt;
		struct {
			/* Mask of pipes whose IRQ logic is backed by the pw */
			u8 irq_pipe_mask;
			/* The pw is backing the VGA functionality */
			bool has_vga:1;
			bool has_fuses:1;
		} hsw;
	};
	const struct i915_power_well_ops *ops;
};

struct i915_power_domains {
	/*
	 * Power wells needed for initialization at driver init and suspend
	 * time are on. They are kept on until after the first modeset.
	 */
	bool init_power_on;
	bool initializing;
	int power_well_count;

	struct rwlock lock;
	int domain_use_count[POWER_DOMAIN_NUM];
	struct i915_power_well *power_wells;
};

#define MAX_L3_SLICES 2
struct intel_l3_parity {
	u32 *remap_info[MAX_L3_SLICES];
	struct work_struct error_work;
	int which_slice;
};

struct i915_gem_mm {
	/** Memory allocator for GTT stolen memory */
	struct drm_mm stolen;
	/** Protects the usage of the GTT stolen memory allocator. This is
	 * always the inner lock when overlapping with struct_mutex. */
	struct rwlock stolen_lock;

	/* Protects bound_list/unbound_list and #drm_i915_gem_object.mm.link */
	spinlock_t obj_lock;

	/** List of all objects in gtt_space. Used to restore gtt
	 * mappings on resume */
	struct list_head bound_list;
	/**
	 * List of objects which are not bound to the GTT (thus
	 * are idle and not used by the GPU). These objects may or may
	 * not actually have any pages attached.
	 */
	struct list_head unbound_list;

	/** List of all objects in gtt_space, currently mmaped by userspace.
	 * All objects within this list must also be on bound_list.
	 */
	struct list_head userfault_list;

	/**
	 * List of objects which are pending destruction.
	 */
	struct llist_head free_list;
	struct work_struct free_work;
	spinlock_t free_lock;
	/**
	 * Count of objects pending destructions. Used to skip needlessly
	 * waiting on an RCU barrier if no objects are waiting to be freed.
	 */
	atomic_t free_count;

	/**
	 * Small stash of WC pages
	 */
	struct pagestash wc_stash;

	/**
	 * tmpfs instance used for shmem backed objects
	 */
	struct vfsmount *gemfs;

	/** PPGTT used for aliasing the PPGTT with the GTT */
	struct i915_hw_ppgtt *aliasing_ppgtt;

	struct notifier_block oom_notifier;
	struct notifier_block vmap_notifier;
#ifdef notyet
	struct shrinker shrinker;
#endif

	/** LRU list of objects with fence regs on them. */
	struct list_head fence_list;

	/**
	 * Workqueue to fault in userptr pages, flushed by the execbuf
	 * when required but otherwise left to userspace to try again
	 * on EAGAIN.
	 */
	struct workqueue_struct *userptr_wq;

	u64 unordered_timeline;

	/* the indicator for dispatch video commands on two BSD rings */
	atomic_t bsd_engine_dispatch_index;

	/** Bit 6 swizzling required for X tiling */
	uint32_t bit_6_swizzle_x;
	/** Bit 6 swizzling required for Y tiling */
	uint32_t bit_6_swizzle_y;

	/* accounting, useful for userland debugging */
	spinlock_t object_stat_lock;
	u64 object_memory;
	u32 object_count;
};

#define I915_IDLE_ENGINES_TIMEOUT (200) /* in ms */

#define I915_RESET_TIMEOUT (10 * HZ) /* 10s */
#define I915_FENCE_TIMEOUT (10 * HZ) /* 10s */

#define I915_ENGINE_DEAD_TIMEOUT  (4 * HZ)  /* Seqno, head and subunits dead */
#define I915_SEQNO_DEAD_TIMEOUT   (12 * HZ) /* Seqno dead with active head */

#define I915_ENGINE_WEDGED_TIMEOUT  (60 * HZ)  /* Reset but no recovery? */

#define DP_AUX_A 0x40
#define DP_AUX_B 0x10
#define DP_AUX_C 0x20
#define DP_AUX_D 0x30
#define DP_AUX_E 0x50
#define DP_AUX_F 0x60

#define DDC_PIN_B  0x05
#define DDC_PIN_C  0x04
#define DDC_PIN_D  0x06

struct ddi_vbt_port_info {
	int max_tmds_clock;

	/*
	 * This is an index in the HDMI/DVI DDI buffer translation table.
	 * The special value HDMI_LEVEL_SHIFT_UNKNOWN means the VBT didn't
	 * populate this field.
	 */
#define HDMI_LEVEL_SHIFT_UNKNOWN	0xff
	uint8_t hdmi_level_shift;

	uint8_t supports_dvi:1;
	uint8_t supports_hdmi:1;
	uint8_t supports_dp:1;
	uint8_t supports_edp:1;

	uint8_t alternate_aux_channel;
	uint8_t alternate_ddc_pin;

	uint8_t dp_boost_level;
	uint8_t hdmi_boost_level;
	int dp_max_link_rate;		/* 0 for not limited by VBT */
};

enum psr_lines_to_wait {
	PSR_0_LINES_TO_WAIT = 0,
	PSR_1_LINE_TO_WAIT,
	PSR_4_LINES_TO_WAIT,
	PSR_8_LINES_TO_WAIT
};

struct intel_vbt_data {
	struct drm_display_mode *lfp_lvds_vbt_mode; /* if any */
	struct drm_display_mode *sdvo_lvds_vbt_mode; /* if any */

	/* Feature bits */
	unsigned int int_tv_support:1;
	unsigned int lvds_dither:1;
	unsigned int int_crt_support:1;
	unsigned int lvds_use_ssc:1;
	unsigned int int_lvds_support:1;
	unsigned int display_clock_mode:1;
	unsigned int fdi_rx_polarity_inverted:1;
	unsigned int panel_type:4;
	int lvds_ssc_freq;
	unsigned int bios_lvds_val; /* initial [PCH_]LVDS reg val in VBIOS */

	enum drrs_support_type drrs_type;

	struct {
		int rate;
		int lanes;
		int preemphasis;
		int vswing;
		bool low_vswing;
		bool initialized;
		int bpp;
		struct edp_power_seq pps;
	} edp;

	struct {
		bool enable;
		bool full_link;
		bool require_aux_wakeup;
		int idle_frames;
		enum psr_lines_to_wait lines_to_wait;
		int tp1_wakeup_time_us;
		int tp2_tp3_wakeup_time_us;
	} psr;

	struct {
		u16 pwm_freq_hz;
		bool present;
		bool active_low_pwm;
		u8 min_brightness;	/* min_brightness/255 of max */
		u8 controller;		/* brightness controller number */
		enum intel_backlight_type type;
	} backlight;

	/* MIPI DSI */
	struct {
		u16 panel_id;
		struct mipi_config *config;
		struct mipi_pps_data *pps;
		u16 bl_ports;
		u16 cabc_ports;
		u8 seq_version;
		u32 size;
		u8 *data;
		const u8 *sequence[MIPI_SEQ_MAX];
		u8 *deassert_seq; /* Used by fixup_mipi_sequences() */
	} dsi;

	int crt_ddc_pin;

	int child_dev_num;
	struct child_device_config *child_dev;

	struct ddi_vbt_port_info ddi_port_info[I915_MAX_PORTS];
	struct sdvo_device_mapping sdvo_mappings[2];
};

enum intel_ddb_partitioning {
	INTEL_DDB_PART_1_2,
	INTEL_DDB_PART_5_6, /* IVB+ */
};

struct intel_wm_level {
	bool enable;
	uint32_t pri_val;
	uint32_t spr_val;
	uint32_t cur_val;
	uint32_t fbc_val;
};

struct ilk_wm_values {
	uint32_t wm_pipe[3];
	uint32_t wm_lp[3];
	uint32_t wm_lp_spr[3];
	uint32_t wm_linetime[3];
	bool enable_fbc_wm;
	enum intel_ddb_partitioning partitioning;
};

struct g4x_pipe_wm {
	uint16_t plane[I915_MAX_PLANES];
	uint16_t fbc;
};

struct g4x_sr_wm {
	uint16_t plane;
	uint16_t cursor;
	uint16_t fbc;
};

struct vlv_wm_ddl_values {
	uint8_t plane[I915_MAX_PLANES];
};

struct vlv_wm_values {
	struct g4x_pipe_wm pipe[3];
	struct g4x_sr_wm sr;
	struct vlv_wm_ddl_values ddl[3];
	uint8_t level;
	bool cxsr;
};

struct g4x_wm_values {
	struct g4x_pipe_wm pipe[2];
	struct g4x_sr_wm sr;
	struct g4x_sr_wm hpll;
	bool cxsr;
	bool hpll_en;
	bool fbc_en;
};

struct skl_ddb_entry {
	uint16_t start, end;	/* in number of blocks, 'end' is exclusive */
};

static inline uint16_t skl_ddb_entry_size(const struct skl_ddb_entry *entry)
{
	return entry->end - entry->start;
}

static inline bool skl_ddb_entry_equal(const struct skl_ddb_entry *e1,
				       const struct skl_ddb_entry *e2)
{
	if (e1->start == e2->start && e1->end == e2->end)
		return true;

	return false;
}

struct skl_ddb_allocation {
	/* packed/y */
	struct skl_ddb_entry plane[I915_MAX_PIPES][I915_MAX_PLANES];
	struct skl_ddb_entry uv_plane[I915_MAX_PIPES][I915_MAX_PLANES];
	u8 enabled_slices; /* GEN11 has configurable 2 slices */
};

struct skl_ddb_values {
	unsigned dirty_pipes;
	struct skl_ddb_allocation ddb;
};

struct skl_wm_level {
	bool plane_en;
	uint16_t plane_res_b;
	uint8_t plane_res_l;
};

/* Stores plane specific WM parameters */
struct skl_wm_params {
	bool x_tiled, y_tiled;
	bool rc_surface;
	bool is_planar;
	uint32_t width;
	uint8_t cpp;
	uint32_t plane_pixel_rate;
	uint32_t y_min_scanlines;
	uint32_t plane_bytes_per_line;
	uint_fixed_16_16_t plane_blocks_per_line;
	uint_fixed_16_16_t y_tile_minimum;
	uint32_t linetime_us;
	uint32_t dbuf_block_size;
};

/*
 * This struct helps tracking the state needed for runtime PM, which puts the
 * device in PCI D3 state. Notice that when this happens, nothing on the
 * graphics device works, even register access, so we don't get interrupts nor
 * anything else.
 *
 * Every piece of our code that needs to actually touch the hardware needs to
 * either call intel_runtime_pm_get or call intel_display_power_get with the
 * appropriate power domain.
 *
 * Our driver uses the autosuspend delay feature, which means we'll only really
 * suspend if we stay with zero refcount for a certain amount of time. The
 * default value is currently very conservative (see intel_runtime_pm_enable), but
 * it can be changed with the standard runtime PM files from sysfs.
 *
 * The irqs_disabled variable becomes true exactly after we disable the IRQs and
 * goes back to false exactly before we reenable the IRQs. We use this variable
 * to check if someone is trying to enable/disable IRQs while they're supposed
 * to be disabled. This shouldn't happen and we'll print some error messages in
 * case it happens.
 *
 * For more, read the Documentation/power/runtime_pm.txt.
 */
struct i915_runtime_pm {
	atomic_t wakeref_count;
	bool suspended;
	bool irqs_enabled;
};

enum intel_pipe_crc_source {
	INTEL_PIPE_CRC_SOURCE_NONE,
	INTEL_PIPE_CRC_SOURCE_PLANE1,
	INTEL_PIPE_CRC_SOURCE_PLANE2,
	INTEL_PIPE_CRC_SOURCE_PF,
	INTEL_PIPE_CRC_SOURCE_PIPE,
	/* TV/DP on pre-gen5/vlv can't use the pipe source. */
	INTEL_PIPE_CRC_SOURCE_TV,
	INTEL_PIPE_CRC_SOURCE_DP_B,
	INTEL_PIPE_CRC_SOURCE_DP_C,
	INTEL_PIPE_CRC_SOURCE_DP_D,
	INTEL_PIPE_CRC_SOURCE_AUTO,
	INTEL_PIPE_CRC_SOURCE_MAX,
};

#define INTEL_PIPE_CRC_ENTRIES_NR	128
struct intel_pipe_crc {
	spinlock_t lock;
	int skipped;
	enum intel_pipe_crc_source source;
};

struct i915_frontbuffer_tracking {
	spinlock_t lock;

	/*
	 * Tracking bits for delayed frontbuffer flushing du to gpu activity or
	 * scheduled flips.
	 */
	unsigned busy_bits;
	unsigned flip_bits;
};

struct i915_wa_reg {
	u32 addr;
	u32 value;
	/* bitmask representing WA bits */
	u32 mask;
};

#define I915_MAX_WA_REGS 16

struct i915_workarounds {
	struct i915_wa_reg reg[I915_MAX_WA_REGS];
	u32 count;
};

struct i915_virtual_gpu {
	bool active;
	u32 caps;
};

/* used in computing the new watermarks state */
struct intel_wm_config {
	unsigned int num_pipes_active;
	bool sprites_enabled;
	bool sprites_scaled;
};

struct i915_oa_format {
	u32 format;
	int size;
};

struct i915_oa_reg {
	i915_reg_t addr;
	u32 value;
};

struct i915_oa_config {
	char uuid[UUID_STRING_LEN + 1];
	int id;

	const struct i915_oa_reg *mux_regs;
	u32 mux_regs_len;
	const struct i915_oa_reg *b_counter_regs;
	u32 b_counter_regs_len;
	const struct i915_oa_reg *flex_regs;
	u32 flex_regs_len;

#ifdef notyet
	struct attribute_group sysfs_metric;
	struct attribute *attrs[2];
	struct device_attribute sysfs_metric_id;
#endif

	atomic_t ref_count;
};

struct i915_perf_stream;

/**
 * struct i915_perf_stream_ops - the OPs to support a specific stream type
 */
struct i915_perf_stream_ops {
	/**
	 * @enable: Enables the collection of HW samples, either in response to
	 * `I915_PERF_IOCTL_ENABLE` or implicitly called when stream is opened
	 * without `I915_PERF_FLAG_DISABLED`.
	 */
	void (*enable)(struct i915_perf_stream *stream);

	/**
	 * @disable: Disables the collection of HW samples, either in response
	 * to `I915_PERF_IOCTL_DISABLE` or implicitly called before destroying
	 * the stream.
	 */
	void (*disable)(struct i915_perf_stream *stream);

	/**
	 * @poll_wait: Call poll_wait, passing a wait queue that will be woken
	 * once there is something ready to read() for the stream
	 */
#ifdef notyet
	void (*poll_wait)(struct i915_perf_stream *stream,
			  struct file *file,
			  poll_table *wait);
#endif

	/**
	 * @wait_unlocked: For handling a blocking read, wait until there is
	 * something to ready to read() for the stream. E.g. wait on the same
	 * wait queue that would be passed to poll_wait().
	 */
	int (*wait_unlocked)(struct i915_perf_stream *stream);

	/**
	 * @read: Copy buffered metrics as records to userspace
	 * **buf**: the userspace, destination buffer
	 * **count**: the number of bytes to copy, requested by userspace
	 * **offset**: zero at the start of the read, updated as the read
	 * proceeds, it represents how many bytes have been copied so far and
	 * the buffer offset for copying the next record.
	 *
	 * Copy as many buffered i915 perf samples and records for this stream
	 * to userspace as will fit in the given buffer.
	 *
	 * Only write complete records; returning -%ENOSPC if there isn't room
	 * for a complete record.
	 *
	 * Return any error condition that results in a short read such as
	 * -%ENOSPC or -%EFAULT, even though these may be squashed before
	 * returning to userspace.
	 */
	int (*read)(struct i915_perf_stream *stream,
		    char __user *buf,
		    size_t count,
		    size_t *offset);

	/**
	 * @destroy: Cleanup any stream specific resources.
	 *
	 * The stream will always be disabled before this is called.
	 */
	void (*destroy)(struct i915_perf_stream *stream);
};

/**
 * struct i915_perf_stream - state for a single open stream FD
 */
struct i915_perf_stream {
	/**
	 * @dev_priv: i915 drm device
	 */
	struct drm_i915_private *dev_priv;

	/**
	 * @link: Links the stream into ``&drm_i915_private->streams``
	 */
	struct list_head link;

	/**
	 * @sample_flags: Flags representing the `DRM_I915_PERF_PROP_SAMPLE_*`
	 * properties given when opening a stream, representing the contents
	 * of a single sample as read() by userspace.
	 */
	u32 sample_flags;

	/**
	 * @sample_size: Considering the configured contents of a sample
	 * combined with the required header size, this is the total size
	 * of a single sample record.
	 */
	int sample_size;

	/**
	 * @ctx: %NULL if measuring system-wide across all contexts or a
	 * specific context that is being monitored.
	 */
	struct i915_gem_context *ctx;

	/**
	 * @enabled: Whether the stream is currently enabled, considering
	 * whether the stream was opened in a disabled state and based
	 * on `I915_PERF_IOCTL_ENABLE` and `I915_PERF_IOCTL_DISABLE` calls.
	 */
	bool enabled;

	/**
	 * @ops: The callbacks providing the implementation of this specific
	 * type of configured stream.
	 */
	const struct i915_perf_stream_ops *ops;

	/**
	 * @oa_config: The OA configuration used by the stream.
	 */
	struct i915_oa_config *oa_config;
};

/**
 * struct i915_oa_ops - Gen specific implementation of an OA unit stream
 */
struct i915_oa_ops {
	/**
	 * @is_valid_b_counter_reg: Validates register's address for
	 * programming boolean counters for a particular platform.
	 */
	bool (*is_valid_b_counter_reg)(struct drm_i915_private *dev_priv,
				       u32 addr);

	/**
	 * @is_valid_mux_reg: Validates register's address for programming mux
	 * for a particular platform.
	 */
	bool (*is_valid_mux_reg)(struct drm_i915_private *dev_priv, u32 addr);

	/**
	 * @is_valid_flex_reg: Validates register's address for programming
	 * flex EU filtering for a particular platform.
	 */
	bool (*is_valid_flex_reg)(struct drm_i915_private *dev_priv, u32 addr);

	/**
	 * @init_oa_buffer: Resets the head and tail pointers of the
	 * circular buffer for periodic OA reports.
	 *
	 * Called when first opening a stream for OA metrics, but also may be
	 * called in response to an OA buffer overflow or other error
	 * condition.
	 *
	 * Note it may be necessary to clear the full OA buffer here as part of
	 * maintaining the invariable that new reports must be written to
	 * zeroed memory for us to be able to reliable detect if an expected
	 * report has not yet landed in memory.  (At least on Haswell the OA
	 * buffer tail pointer is not synchronized with reports being visible
	 * to the CPU)
	 */
	void (*init_oa_buffer)(struct drm_i915_private *dev_priv);

	/**
	 * @enable_metric_set: Selects and applies any MUX configuration to set
	 * up the Boolean and Custom (B/C) counters that are part of the
	 * counter reports being sampled. May apply system constraints such as
	 * disabling EU clock gating as required.
	 */
	int (*enable_metric_set)(struct drm_i915_private *dev_priv,
				 const struct i915_oa_config *oa_config);

	/**
	 * @disable_metric_set: Remove system constraints associated with using
	 * the OA unit.
	 */
	void (*disable_metric_set)(struct drm_i915_private *dev_priv);

	/**
	 * @oa_enable: Enable periodic sampling
	 */
	void (*oa_enable)(struct drm_i915_private *dev_priv);

	/**
	 * @oa_disable: Disable periodic sampling
	 */
	void (*oa_disable)(struct drm_i915_private *dev_priv);

	/**
	 * @read: Copy data from the circular OA buffer into a given userspace
	 * buffer.
	 */
	int (*read)(struct i915_perf_stream *stream,
		    char __user *buf,
		    size_t count,
		    size_t *offset);

	/**
	 * @oa_hw_tail_read: read the OA tail pointer register
	 *
	 * In particular this enables us to share all the fiddly code for
	 * handling the OA unit tail pointer race that affects multiple
	 * generations.
	 */
	u32 (*oa_hw_tail_read)(struct drm_i915_private *dev_priv);
};

struct intel_cdclk_state {
	unsigned int cdclk, vco, ref, bypass;
	u8 voltage_level;
};

struct inteldrm_softc {
	struct device sc_dev;
	bus_dma_tag_t dmat;
	bus_space_tag_t bst;
	struct agp_map *agph;
	bus_space_handle_t opregion_ioh;
	bus_space_handle_t opregion_rvda_ioh;
	bus_size_t opregion_rvda_size;

	struct drm_device drm;

	struct pool objects;
	struct pool vmas;
	struct pool luts;
	struct pool requests;
	struct pool dependencies;
	struct pool priorities;

	const struct intel_device_info info;
	struct intel_driver_caps caps;

	/**
	 * Data Stolen Memory - aka "i915 stolen memory" gives us the start and
	 * end of stolen which we can optionally use to create GEM objects
	 * backed by stolen memory. Note that stolen_usable_size tells us
	 * exactly how much of this we are actually allowed to use, given that
	 * some portion of it is in fact reserved for use by hardware functions.
	 */
	struct resource dsm;
	/**
	 * Reseved portion of Data Stolen Memory
	 */
	struct resource dsm_reserved;

	/*
	 * Stolen memory is segmented in hardware with different portions
	 * offlimits to certain functions.
	 *
	 * The drm_mm is initialised to the total accessible range, as found
	 * from the PCI config. On Broadwell+, this is further restricted to
	 * avoid the first page! The upper end of stolen memory is reserved for
	 * hardware functions and similarly removed from the accessible range.
	 */
	resource_size_t stolen_usable_size;	/* Total size minus reserved ranges */

	void __iomem *regs;
	pci_chipset_tag_t pc;
	pcitag_t tag;
	struct extent *memex;
	pci_intr_handle_t ih;
	void *irqh;

	struct vga_pci_bar bar;
	struct vga_pci_bar *vga_regs;

	struct drm_pcidev *id;

	int console;
	int primary;
	int nscreens;
	void (*switchcb)(void *, int, int);
	void *switchcbarg;
	void *switchcookie;
	struct task switchtask;
	struct rasops_info ro;

	struct task burner_task;
	int burner_fblank;

	struct backlight_device *backlight;

	struct intel_uncore uncore;

	struct i915_virtual_gpu vgpu;

	struct intel_gvt *gvt;

	struct intel_wopcm wopcm;

	struct intel_huc huc;
	struct intel_guc guc;

	struct intel_csr csr;

	struct intel_gmbus gmbus[GMBUS_NUM_PINS];

	/** gmbus_mutex protects against concurrent usage of the single hw gmbus
	 * controller on different i2c buses. */
	struct rwlock gmbus_mutex;

	/**
	 * Base address of the gmbus and gpio block.
	 */
	uint32_t gpio_mmio_base;

	/* MMIO base address for MIPI regs */
	uint32_t mipi_mmio_base;

	uint32_t psr_mmio_base;

	uint32_t pps_mmio_base;

	wait_queue_head_t gmbus_wait_queue;

	struct pci_dev *bridge_dev;
	struct intel_engine_cs *engine[I915_NUM_ENGINES];
	/* Context used internally to idle the GPU and setup initial state */
	struct i915_gem_context *kernel_context;
	/* Context only to be used for injecting preemption commands */
	struct i915_gem_context *preempt_context;
	struct intel_engine_cs *engine_class[MAX_ENGINE_CLASS + 1]
					    [MAX_ENGINE_INSTANCE + 1];

	struct drm_dma_handle *status_page_dmah;
	struct resource mch_res;
	union flush {
		struct {
			bus_space_tag_t		bst;
			bus_space_handle_t	bsh;
		} i9xx;
		struct {
			bus_dma_segment_t	seg;
			caddr_t			kva;
		} i8xx;
	}			 ifp;
	struct vm_page *pgs;

	/* protects the irq masks */
	spinlock_t irq_lock;

	bool display_irqs_enabled;

	/* To control wakeup latency, e.g. for irq-driven dp aux transfers. */
	struct pm_qos_request pm_qos;

	/* Sideband mailbox protection */
	struct rwlock sb_lock;

	/** Cached value of IMR to avoid reads in updating the bitfield */
	union {
		u32 irq_mask;
		u32 de_irq_mask[I915_MAX_PIPES];
	};
	u32 gt_irq_mask;
	u32 pm_imr;
	u32 pm_ier;
	u32 pm_rps_events;
	u32 pm_guc_events;
	u32 pipestat_irq_mask[I915_MAX_PIPES];

	struct i915_hotplug hotplug;
	struct intel_fbc fbc;
	struct i915_drrs drrs;
	struct intel_opregion opregion;
	struct intel_vbt_data vbt;

	bool preserve_bios_swizzle;

	/* overlay */
	struct intel_overlay *overlay;

	/* backlight registers and fields in struct intel_panel */
	struct rwlock backlight_lock;

	/* LVDS info */
	bool no_aux_handshake;

	/* protects panel power sequencer state */
	struct rwlock pps_mutex;

	struct drm_i915_fence_reg fence_regs[I915_MAX_NUM_FENCES]; /* assume 965 */
	int num_fence_regs; /* 8 on pre-965, 16 otherwise */

	unsigned int fsb_freq, mem_freq, is_ddr3;
	unsigned int skl_preferred_vco_freq;
	unsigned int max_cdclk_freq;

	unsigned int max_dotclk_freq;
	unsigned int rawclk_freq;
	unsigned int hpll_freq;
	unsigned int fdi_pll_freq;
	unsigned int czclk_freq;

	struct {
		/*
		 * The current logical cdclk state.
		 * See intel_atomic_state.cdclk.logical
		 *
		 * For reading holding any crtc lock is sufficient,
		 * for writing must hold all of them.
		 */
		struct intel_cdclk_state logical;
		/*
		 * The current actual cdclk state.
		 * See intel_atomic_state.cdclk.actual
		 */
		struct intel_cdclk_state actual;
		/* The current hardware cdclk state */
		struct intel_cdclk_state hw;
	} cdclk;

	/**
	 * wq - Driver workqueue for GEM.
	 *
	 * NOTE: Work items scheduled here are not allowed to grab any modeset
	 * locks, for otherwise the flushing done in the pageflip code will
	 * result in deadlocks.
	 */
	struct workqueue_struct *wq;

	/* ordered wq for modesets */
	struct workqueue_struct *modeset_wq;

	/* Display functions */
	struct drm_i915_display_funcs display;

	/* PCH chipset type */
	enum intel_pch pch_type;
	unsigned short pch_id;

	unsigned long quirks;

	struct drm_atomic_state *modeset_restore_state;
	struct drm_modeset_acquire_ctx reset_ctx;

	struct i915_ggtt ggtt; /* VM representing the global address space */

	struct i915_gem_mm mm;
	DECLARE_HASHTABLE(mm_structs, 7);
	struct rwlock mm_lock;

	struct intel_ppat ppat;

	/* Kernel Modesetting */

	struct intel_crtc *plane_to_crtc_mapping[I915_MAX_PIPES];
	struct intel_crtc *pipe_to_crtc_mapping[I915_MAX_PIPES];

#ifdef CONFIG_DEBUG_FS
	struct intel_pipe_crc pipe_crc[I915_MAX_PIPES];
#endif

	/* dpll and cdclk state is protected by connection_mutex */
	int num_shared_dpll;
	struct intel_shared_dpll shared_dplls[I915_NUM_PLLS];
	const struct intel_dpll_mgr *dpll_mgr;

	/*
	 * dpll_lock serializes intel_{prepare,enable,disable}_shared_dpll.
	 * Must be global rather than per dpll, because on some platforms
	 * plls share registers.
	 */
	struct rwlock dpll_lock;

	unsigned int active_crtcs;
	/* minimum acceptable cdclk for each pipe */
	int min_cdclk[I915_MAX_PIPES];
	/* minimum acceptable voltage level for each pipe */
	u8 min_voltage_level[I915_MAX_PIPES];

	int dpio_phy_iosf_port[I915_NUM_PHYS_VLV];

	struct i915_workarounds workarounds;

	struct i915_frontbuffer_tracking fb_tracking;

	struct intel_atomic_helper {
		struct llist_head free_list;
		struct work_struct free_work;
	} atomic_helper;

	u16 orig_clock;

	bool mchbar_need_disable;

	struct intel_l3_parity l3_parity;

	/* Cannot be determined by PCIID. You must always read a register. */
	u32 edram_cap;

	/*
	 * Protects RPS/RC6 register access and PCU communication.
	 * Must be taken after struct_mutex if nested. Note that
	 * this lock may be held for long periods of time when
	 * talking to hw - so only take it when talking to hw!
	 */
	struct rwlock pcu_lock;

	/* gen6+ GT PM state */
	struct intel_gen6_power_mgmt gt_pm;

	/* ilk-only ips/rps state. Everything in here is protected by the global
	 * mchdev_lock in intel_pm.c */
	struct intel_ilk_power_mgmt ips;

	struct i915_power_domains power_domains;

	struct i915_psr psr;

	struct i915_gpu_error gpu_error;

	struct drm_i915_gem_object *vlv_pctx;

	/* list of fbdev register on this device */
	struct intel_fbdev *fbdev;
	struct work_struct fbdev_suspend_work;

	struct drm_property *broadcast_rgb_property;
	struct drm_property *force_audio_property;

	/* hda/i915 audio component */
	struct i915_audio_component *audio_component;
	bool audio_component_registered;
	/**
	 * av_mutex - mutex for audio/video sync
	 *
	 */
	struct rwlock av_mutex;

	struct {
		struct list_head list;
		struct llist_head free_list;
		struct work_struct free_work;

		/* The hw wants to have a stable context identifier for the
		 * lifetime of the context (for OA, PASID, faults, etc).
		 * This is limited in execlists to 21 bits.
		 */
		struct ida hw_ida;
#define MAX_CONTEXT_HW_ID (1<<21) /* exclusive */
#define MAX_GUC_CONTEXT_HW_ID (1 << 20) /* exclusive */
#define GEN11_MAX_CONTEXT_HW_ID (1<<11) /* exclusive */
	} contexts;

	u32 fdi_rx_config;

	/* Shadow for DISPLAY_PHY_CONTROL which can't be safely read */
	u32 chv_phy_control;
	/*
	 * Shadows for CHV DPLL_MD regs to keep the state
	 * checker somewhat working in the presence hardware
	 * crappiness (can't read out DPLL_MD for pipes B & C).
	 */
	u32 chv_dpll_md[I915_MAX_PIPES];
	u32 bxt_phy_grc;

	u32 suspend_count;
	bool power_domains_suspended;
	struct i915_suspend_saved_registers regfile;
	struct vlv_s0ix_state vlv_s0ix_state;

	enum {
		I915_SAGV_UNKNOWN = 0,
		I915_SAGV_DISABLED,
		I915_SAGV_ENABLED,
		I915_SAGV_NOT_CONTROLLED
	} sagv_status;

	struct {
		/*
		 * Raw watermark latency values:
		 * in 0.1us units for WM0,
		 * in 0.5us units for WM1+.
		 */
		/* primary */
		uint16_t pri_latency[5];
		/* sprite */
		uint16_t spr_latency[5];
		/* cursor */
		uint16_t cur_latency[5];
		/*
		 * Raw watermark memory latency values
		 * for SKL for all 8 levels
		 * in 1us units.
		 */
		uint16_t skl_latency[8];

		/* current hardware state */
		union {
			struct ilk_wm_values hw;
			struct skl_ddb_values skl_hw;
			struct vlv_wm_values vlv;
			struct g4x_wm_values g4x;
		};

		uint8_t max_level;

		/*
		 * Should be held around atomic WM register writing; also
		 * protects * intel_crtc->wm.active and
		 * cstate->wm.need_postvbl_update.
		 */
		struct rwlock wm_mutex;

		/*
		 * Set during HW readout of watermarks/DDB.  Some platforms
		 * need to know when we're still using BIOS-provided values
		 * (which we don't fully trust).
		 */
		bool distrust_bios_wm;
	} wm;

	struct i915_runtime_pm runtime_pm;

	struct {
		bool initialized;

		struct kobject *metrics_kobj;
		struct ctl_table_header *sysctl_header;

		/*
		 * Lock associated with adding/modifying/removing OA configs
		 * in dev_priv->perf.metrics_idr.
		 */
		struct rwlock metrics_lock;

		/*
		 * List of dynamic configurations, you need to hold
		 * dev_priv->perf.metrics_lock to access it.
		 */
		struct idr metrics_idr;

		/*
		 * Lock associated with anything below within this structure
		 * except exclusive_stream.
		 */
		struct rwlock lock;
		struct list_head streams;

		struct {
			/*
			 * The stream currently using the OA unit. If accessed
			 * outside a syscall associated to its file
			 * descriptor, you need to hold
			 * dev_priv->drm.struct_mutex.
			 */
			struct i915_perf_stream *exclusive_stream;

			struct intel_context *pinned_ctx;
			u32 specific_ctx_id;
			u32 specific_ctx_id_mask;

			struct timeout poll_check_timer;
			wait_queue_head_t poll_wq;
			bool pollin;

			/**
			 * For rate limiting any notifications of spurious
			 * invalid OA reports
			 */
#ifdef notyet
			struct ratelimit_state spurious_report_rs;
#endif

			bool periodic;
			int period_exponent;

			struct i915_oa_config test_config;

			struct {
				struct i915_vma *vma;
				u8 *vaddr;
				u32 last_ctx_id;
				int format;
				int format_size;

				/**
				 * Locks reads and writes to all head/tail state
				 *
				 * Consider: the head and tail pointer state
				 * needs to be read consistently from a hrtimer
				 * callback (atomic context) and read() fop
				 * (user context) with tail pointer updates
				 * happening in atomic context and head updates
				 * in user context and the (unlikely)
				 * possibility of read() errors needing to
				 * reset all head/tail state.
				 *
				 * Note: Contention or performance aren't
				 * currently a significant concern here
				 * considering the relatively low frequency of
				 * hrtimer callbacks (5ms period) and that
				 * reads typically only happen in response to a
				 * hrtimer event and likely complete before the
				 * next callback.
				 *
				 * Note: This lock is not held *while* reading
				 * and copying data to userspace so the value
				 * of head observed in htrimer callbacks won't
				 * represent any partial consumption of data.
				 */
				spinlock_t ptr_lock;

				/**
				 * One 'aging' tail pointer and one 'aged'
				 * tail pointer ready to used for reading.
				 *
				 * Initial values of 0xffffffff are invalid
				 * and imply that an update is required
				 * (and should be ignored by an attempted
				 * read)
				 */
				struct {
					u32 offset;
				} tails[2];

				/**
				 * Index for the aged tail ready to read()
				 * data up to.
				 */
				unsigned int aged_tail_idx;

				/**
				 * A monotonic timestamp for when the current
				 * aging tail pointer was read; used to
				 * determine when it is old enough to trust.
				 */
				u64 aging_timestamp;

				/**
				 * Although we can always read back the head
				 * pointer register, we prefer to avoid
				 * trusting the HW state, just to avoid any
				 * risk that some hardware condition could
				 * somehow bump the head pointer unpredictably
				 * and cause us to forward the wrong OA buffer
				 * data to userspace.
				 */
				u32 head;
			} oa_buffer;

			u32 gen7_latched_oastatus1;
			u32 ctx_oactxctrl_offset;
			u32 ctx_flexeu0_offset;

			/**
			 * The RPT_ID/reason field for Gen8+ includes a bit
			 * to determine if the CTX ID in the report is valid
			 * but the specific bit differs between Gen 8 and 9
			 */
			u32 gen8_valid_ctx_bit;

			struct i915_oa_ops ops;
			const struct i915_oa_format *oa_formats;
		} oa;
	} perf;

	/* Abstract the submission mechanism (legacy ringbuffer or execlists) away */
	struct {
		void (*resume)(struct drm_i915_private *);
		void (*cleanup_engine)(struct intel_engine_cs *engine);

		struct list_head timelines;

		struct list_head active_rings;
		struct list_head closed_vma;
		u32 active_requests;
		u32 request_serial;

		/**
		 * Is the GPU currently considered idle, or busy executing
		 * userspace requests? Whilst idle, we allow runtime power
		 * management to power down the hardware and display clocks.
		 * In order to reduce the effect on performance, there
		 * is a slight delay before we do so.
		 */
		bool awake;

		/**
		 * The number of times we have woken up.
		 */
		unsigned int epoch;
#define I915_EPOCH_INVALID 0

		/**
		 * We leave the user IRQ off as much as possible,
		 * but this means that requests will finish and never
		 * be retired once the system goes idle. Set a timer to
		 * fire periodically while the ring is running. When it
		 * fires, go retire requests.
		 */
		struct delayed_work retire_work;

		/**
		 * When we detect an idle GPU, we want to turn on
		 * powersaving features. So once we see that there
		 * are no more requests outstanding and no more
		 * arrive within a small period of time, we fire
		 * off the idle_work.
		 */
		struct delayed_work idle_work;

		ktime_t last_init_time;
	} gt;

	/* perform PHY state sanity checks? */
	bool chv_phy_assert[2];

	bool ipc_enabled;

	/* Used to save the pipe-to-encoder mapping for audio */
	struct intel_encoder *av_enc_map[I915_MAX_PIPES];

	/* necessary resource sharing with HDMI LPE audio driver. */
	struct {
		struct platform_device *platdev;
		int	irq;
	} lpe_audio;

	struct i915_pmu pmu;

	/*
	 * NOTE: This is the dri1/ums dungeon, don't add stuff here. Your patch
	 * will be rejected. Instead look for a better place.
	 */
};

static inline struct drm_i915_private *to_i915(const struct drm_device *dev)
{
	return container_of(dev, struct drm_i915_private, drm);
}

#ifdef __linux__
static inline struct drm_i915_private *kdev_to_i915(struct device *kdev)
{
	return to_i915(dev_get_drvdata(kdev));
}
#endif

static inline struct drm_i915_private *wopcm_to_i915(struct intel_wopcm *wopcm)
{
	return container_of(wopcm, struct drm_i915_private, wopcm);
}

static inline struct drm_i915_private *guc_to_i915(struct intel_guc *guc)
{
	return container_of(guc, struct drm_i915_private, guc);
}

static inline struct drm_i915_private *huc_to_i915(struct intel_huc *huc)
{
	return container_of(huc, struct drm_i915_private, huc);
}

/* Simple iterator over all initialised engines */
#define for_each_engine(engine__, dev_priv__, id__) \
	for ((id__) = 0; \
	     (id__) < I915_NUM_ENGINES; \
	     (id__)++) \
		for_each_if ((engine__) = (dev_priv__)->engine[(id__)])

/* Iterator over subset of engines selected by mask */
#define for_each_engine_masked(engine__, dev_priv__, mask__, tmp__) \
	for ((tmp__) = (mask__) & INTEL_INFO(dev_priv__)->ring_mask; \
	     (tmp__) ? \
	     ((engine__) = (dev_priv__)->engine[__mask_next_bit(tmp__)]), 1 : \
	     0;)

enum hdmi_force_audio {
	HDMI_AUDIO_OFF_DVI = -2,	/* no aux data for HDMI-DVI converter */
	HDMI_AUDIO_OFF,			/* force turn off HDMI audio */
	HDMI_AUDIO_AUTO,		/* trust EDID */
	HDMI_AUDIO_ON,			/* force turn on HDMI audio */
};

#define I915_GTT_OFFSET_NONE ((u32)-1)

/*
 * Frontbuffer tracking bits. Set in obj->frontbuffer_bits while a gem bo is
 * considered to be the frontbuffer for the given plane interface-wise. This
 * doesn't mean that the hw necessarily already scans it out, but that any
 * rendering (by the cpu or gpu) will land in the frontbuffer eventually.
 *
 * We have one bit per pipe and per scanout plane type.
 */
#define INTEL_FRONTBUFFER_BITS_PER_PIPE 8
#define INTEL_FRONTBUFFER(pipe, plane_id) ({ \
	BUILD_BUG_ON(INTEL_FRONTBUFFER_BITS_PER_PIPE * I915_MAX_PIPES > 32); \
	BUILD_BUG_ON(I915_MAX_PLANES > INTEL_FRONTBUFFER_BITS_PER_PIPE); \
	BIT((plane_id) + INTEL_FRONTBUFFER_BITS_PER_PIPE * (pipe)); \
})
#define INTEL_FRONTBUFFER_OVERLAY(pipe) \
	BIT(INTEL_FRONTBUFFER_BITS_PER_PIPE - 1 + INTEL_FRONTBUFFER_BITS_PER_PIPE * (pipe))
#define INTEL_FRONTBUFFER_ALL_MASK(pipe) \
	GENMASK(INTEL_FRONTBUFFER_BITS_PER_PIPE * ((pipe) + 1) - 1, \
		INTEL_FRONTBUFFER_BITS_PER_PIPE * (pipe))

/*
 * Optimised SGL iterator for GEM objects
 */
static __always_inline struct sgt_iter {
	struct scatterlist *sgp;
	union {
		unsigned long pfn;
		dma_addr_t dma;
	};
	unsigned int curr;
	unsigned int max;
} __sgt_iter(struct scatterlist *sgl, bool dma) {
	struct sgt_iter s = { .sgp = sgl };

	if (s.sgp) {
		s.max = s.curr = s.sgp->offset;
		s.max += s.sgp->length;
		if (dma)
			s.dma = sg_dma_address(s.sgp);
		else
			s.pfn = page_to_pfn(sg_page(s.sgp));
	}

	return s;
}

static inline struct scatterlist *____sg_next(struct scatterlist *sg)
{
	++sg;
	if (unlikely(sg_is_chain(sg)))
		sg = sg_chain_ptr(sg);
	return sg;
}

/**
 * __sg_next - return the next scatterlist entry in a list
 * @sg:		The current sg entry
 *
 * Description:
 *   If the entry is the last, return NULL; otherwise, step to the next
 *   element in the array (@sg@+1). If that's a chain pointer, follow it;
 *   otherwise just return the pointer to the current element.
 **/
static inline struct scatterlist *__sg_next(struct scatterlist *sg)
{
	return sg_is_last(sg) ? NULL : ____sg_next(sg);
}

/**
 * for_each_sgt_dma - iterate over the DMA addresses of the given sg_table
 * @__dmap:	DMA address (output)
 * @__iter:	'struct sgt_iter' (iterator state, internal)
 * @__sgt:	sg_table to iterate over (input)
 */
#define for_each_sgt_dma(__dmap, __iter, __sgt)				\
	for ((__iter) = __sgt_iter((__sgt)->sgl, true);			\
	     ((__dmap) = (__iter).dma + (__iter).curr);			\
	     (((__iter).curr += I915_GTT_PAGE_SIZE) >= (__iter).max) ?	\
	     (__iter) = __sgt_iter(__sg_next((__iter).sgp), true), 0 : 0)

/**
 * for_each_sgt_page - iterate over the pages of the given sg_table
 * @__pp:	page pointer (output)
 * @__iter:	'struct sgt_iter' (iterator state, internal)
 * @__sgt:	sg_table to iterate over (input)
 */
#define for_each_sgt_page(__pp, __iter, __sgt)				\
	for ((__iter) = __sgt_iter((__sgt)->sgl, false);		\
	     ((__pp) = (__iter).pfn == 0 ? NULL :			\
	      pfn_to_page((__iter).pfn + ((__iter).curr >> PAGE_SHIFT))); \
	     (((__iter).curr += PAGE_SIZE) >= (__iter).max) ?		\
	     (__iter) = __sgt_iter(__sg_next((__iter).sgp), false), 0 : 0)

static inline unsigned int i915_sg_page_sizes(struct scatterlist *sg)
{
	unsigned int page_sizes;

	page_sizes = 0;
	while (sg) {
		GEM_BUG_ON(sg->offset);
		GEM_BUG_ON(!IS_ALIGNED(sg->length, PAGE_SIZE));
		page_sizes |= sg->length;
		sg = __sg_next(sg);
	}

	return page_sizes;
}

#ifdef __linux__
static inline unsigned int i915_sg_segment_size(void)
{
	unsigned int size = swiotlb_max_segment();

	if (size == 0)
		return SCATTERLIST_MAX_SEGMENT;

	size = rounddown(size, PAGE_SIZE);
	/* swiotlb_max_segment_size can return 1 byte when it means one page. */
	if (size < PAGE_SIZE)
		size = PAGE_SIZE;

	return size;
}
#else
static inline unsigned int i915_sg_segment_size(void)
{
	return PAGE_SIZE;
}
#endif

static inline const struct intel_device_info *
intel_info(const struct drm_i915_private *dev_priv)
{
	return &dev_priv->info;
}

#define INTEL_INFO(dev_priv)	intel_info((dev_priv))
#define DRIVER_CAPS(dev_priv)	(&(dev_priv)->caps)

#define INTEL_GEN(dev_priv)	((dev_priv)->info.gen)
#define INTEL_DEVID(dev_priv)	((dev_priv)->info.device_id)

#define REVID_FOREVER		0xff
#define INTEL_REVID(dev_priv)	((dev_priv)->drm.pdev->revision)

#define GEN_FOREVER (0)

#define INTEL_GEN_MASK(s, e) ( \
	BUILD_BUG_ON_ZERO(!__builtin_constant_p(s)) + \
	BUILD_BUG_ON_ZERO(!__builtin_constant_p(e)) + \
	GENMASK((e) != GEN_FOREVER ? (e) - 1 : BITS_PER_LONG - 1, \
		(s) != GEN_FOREVER ? (s) - 1 : 0) \
)

/*
 * Returns true if Gen is in inclusive range [Start, End].
 *
 * Use GEN_FOREVER for unbound start and or end.
 */
#define IS_GEN(dev_priv, s, e) \
	(!!((dev_priv)->info.gen_mask & INTEL_GEN_MASK((s), (e))))

/*
 * Return true if revision is in range [since,until] inclusive.
 *
 * Use 0 for open-ended since, and REVID_FOREVER for open-ended until.
 */
#define IS_REVID(p, since, until) \
	(INTEL_REVID(p) >= (since) && INTEL_REVID(p) <= (until))

#define IS_PLATFORM(dev_priv, p) ((dev_priv)->info.platform_mask & BIT(p))

#define IS_I830(dev_priv)	IS_PLATFORM(dev_priv, INTEL_I830)
#define IS_I845G(dev_priv)	IS_PLATFORM(dev_priv, INTEL_I845G)
#define IS_I85X(dev_priv)	IS_PLATFORM(dev_priv, INTEL_I85X)
#define IS_I865G(dev_priv)	IS_PLATFORM(dev_priv, INTEL_I865G)
#define IS_I915G(dev_priv)	IS_PLATFORM(dev_priv, INTEL_I915G)
#define IS_I915GM(dev_priv)	IS_PLATFORM(dev_priv, INTEL_I915GM)
#define IS_I945G(dev_priv)	IS_PLATFORM(dev_priv, INTEL_I945G)
#define IS_I945GM(dev_priv)	IS_PLATFORM(dev_priv, INTEL_I945GM)
#define IS_I965G(dev_priv)	IS_PLATFORM(dev_priv, INTEL_I965G)
#define IS_I965GM(dev_priv)	IS_PLATFORM(dev_priv, INTEL_I965GM)
#define IS_G45(dev_priv)	IS_PLATFORM(dev_priv, INTEL_G45)
#define IS_GM45(dev_priv)	IS_PLATFORM(dev_priv, INTEL_GM45)
#define IS_G4X(dev_priv)	(IS_G45(dev_priv) || IS_GM45(dev_priv))
#define IS_PINEVIEW_G(dev_priv)	(INTEL_DEVID(dev_priv) == 0xa001)
#define IS_PINEVIEW_M(dev_priv)	(INTEL_DEVID(dev_priv) == 0xa011)
#define IS_PINEVIEW(dev_priv)	IS_PLATFORM(dev_priv, INTEL_PINEVIEW)
#define IS_G33(dev_priv)	IS_PLATFORM(dev_priv, INTEL_G33)
#define IS_IRONLAKE_M(dev_priv)	(INTEL_DEVID(dev_priv) == 0x0046)
#define IS_IVYBRIDGE(dev_priv)	IS_PLATFORM(dev_priv, INTEL_IVYBRIDGE)
#define IS_IVB_GT1(dev_priv)	(IS_IVYBRIDGE(dev_priv) && \
				 (dev_priv)->info.gt == 1)
#define IS_VALLEYVIEW(dev_priv)	IS_PLATFORM(dev_priv, INTEL_VALLEYVIEW)
#define IS_CHERRYVIEW(dev_priv)	IS_PLATFORM(dev_priv, INTEL_CHERRYVIEW)
#define IS_HASWELL(dev_priv)	IS_PLATFORM(dev_priv, INTEL_HASWELL)
#define IS_BROADWELL(dev_priv)	IS_PLATFORM(dev_priv, INTEL_BROADWELL)
#define IS_SKYLAKE(dev_priv)	IS_PLATFORM(dev_priv, INTEL_SKYLAKE)
#define IS_BROXTON(dev_priv)	IS_PLATFORM(dev_priv, INTEL_BROXTON)
#define IS_KABYLAKE(dev_priv)	IS_PLATFORM(dev_priv, INTEL_KABYLAKE)
#define IS_GEMINILAKE(dev_priv)	IS_PLATFORM(dev_priv, INTEL_GEMINILAKE)
#define IS_COFFEELAKE(dev_priv)	IS_PLATFORM(dev_priv, INTEL_COFFEELAKE)
#define IS_CANNONLAKE(dev_priv)	IS_PLATFORM(dev_priv, INTEL_CANNONLAKE)
#define IS_ICELAKE(dev_priv)	IS_PLATFORM(dev_priv, INTEL_ICELAKE)
#define IS_MOBILE(dev_priv)	((dev_priv)->info.is_mobile)
#define IS_HSW_EARLY_SDV(dev_priv) (IS_HASWELL(dev_priv) && \
				    (INTEL_DEVID(dev_priv) & 0xFF00) == 0x0C00)
#define IS_BDW_ULT(dev_priv)	(IS_BROADWELL(dev_priv) && \
				 ((INTEL_DEVID(dev_priv) & 0xf) == 0x6 ||	\
				 (INTEL_DEVID(dev_priv) & 0xf) == 0xb ||	\
				 (INTEL_DEVID(dev_priv) & 0xf) == 0xe))
/* ULX machines are also considered ULT. */
#define IS_BDW_ULX(dev_priv)	(IS_BROADWELL(dev_priv) && \
				 (INTEL_DEVID(dev_priv) & 0xf) == 0xe)
#define IS_BDW_GT3(dev_priv)	(IS_BROADWELL(dev_priv) && \
				 (dev_priv)->info.gt == 3)
#define IS_HSW_ULT(dev_priv)	(IS_HASWELL(dev_priv) && \
				 (INTEL_DEVID(dev_priv) & 0xFF00) == 0x0A00)
#define IS_HSW_GT3(dev_priv)	(IS_HASWELL(dev_priv) && \
				 (dev_priv)->info.gt == 3)
/* ULX machines are also considered ULT. */
#define IS_HSW_ULX(dev_priv)	(INTEL_DEVID(dev_priv) == 0x0A0E || \
				 INTEL_DEVID(dev_priv) == 0x0A1E)
#define IS_SKL_ULT(dev_priv)	(INTEL_DEVID(dev_priv) == 0x1906 || \
				 INTEL_DEVID(dev_priv) == 0x1913 || \
				 INTEL_DEVID(dev_priv) == 0x1916 || \
				 INTEL_DEVID(dev_priv) == 0x1921 || \
				 INTEL_DEVID(dev_priv) == 0x1926)
#define IS_SKL_ULX(dev_priv)	(INTEL_DEVID(dev_priv) == 0x190E || \
				 INTEL_DEVID(dev_priv) == 0x1915 || \
				 INTEL_DEVID(dev_priv) == 0x191E)
#define IS_KBL_ULT(dev_priv)	(INTEL_DEVID(dev_priv) == 0x5906 || \
				 INTEL_DEVID(dev_priv) == 0x5913 || \
				 INTEL_DEVID(dev_priv) == 0x5916 || \
				 INTEL_DEVID(dev_priv) == 0x5921 || \
				 INTEL_DEVID(dev_priv) == 0x5926)
#define IS_KBL_ULX(dev_priv)	(INTEL_DEVID(dev_priv) == 0x590E || \
				 INTEL_DEVID(dev_priv) == 0x5915 || \
				 INTEL_DEVID(dev_priv) == 0x591E)
#define IS_SKL_GT2(dev_priv)	(IS_SKYLAKE(dev_priv) && \
				 (dev_priv)->info.gt == 2)
#define IS_SKL_GT3(dev_priv)	(IS_SKYLAKE(dev_priv) && \
				 (dev_priv)->info.gt == 3)
#define IS_SKL_GT4(dev_priv)	(IS_SKYLAKE(dev_priv) && \
				 (dev_priv)->info.gt == 4)
#define IS_KBL_GT2(dev_priv)	(IS_KABYLAKE(dev_priv) && \
				 (dev_priv)->info.gt == 2)
#define IS_KBL_GT3(dev_priv)	(IS_KABYLAKE(dev_priv) && \
				 (dev_priv)->info.gt == 3)
#define IS_CFL_ULT(dev_priv)	(IS_COFFEELAKE(dev_priv) && \
				 (INTEL_DEVID(dev_priv) & 0x00F0) == 0x00A0)
#define IS_CFL_GT2(dev_priv)	(IS_COFFEELAKE(dev_priv) && \
				 (dev_priv)->info.gt == 2)
#define IS_CFL_GT3(dev_priv)	(IS_COFFEELAKE(dev_priv) && \
				 (dev_priv)->info.gt == 3)
#define IS_CNL_WITH_PORT_F(dev_priv)   (IS_CANNONLAKE(dev_priv) && \
					(INTEL_DEVID(dev_priv) & 0x0004) == 0x0004)

#define IS_ALPHA_SUPPORT(intel_info) ((intel_info)->is_alpha_support)

#define SKL_REVID_A0		0x0
#define SKL_REVID_B0		0x1
#define SKL_REVID_C0		0x2
#define SKL_REVID_D0		0x3
#define SKL_REVID_E0		0x4
#define SKL_REVID_F0		0x5
#define SKL_REVID_G0		0x6
#define SKL_REVID_H0		0x7

#define IS_SKL_REVID(p, since, until) (IS_SKYLAKE(p) && IS_REVID(p, since, until))

#define BXT_REVID_A0		0x0
#define BXT_REVID_A1		0x1
#define BXT_REVID_B0		0x3
#define BXT_REVID_B_LAST	0x8
#define BXT_REVID_C0		0x9

#define IS_BXT_REVID(dev_priv, since, until) \
	(IS_BROXTON(dev_priv) && IS_REVID(dev_priv, since, until))

#define KBL_REVID_A0		0x0
#define KBL_REVID_B0		0x1
#define KBL_REVID_C0		0x2
#define KBL_REVID_D0		0x3
#define KBL_REVID_E0		0x4

#define IS_KBL_REVID(dev_priv, since, until) \
	(IS_KABYLAKE(dev_priv) && IS_REVID(dev_priv, since, until))

#define GLK_REVID_A0		0x0
#define GLK_REVID_A1		0x1

#define IS_GLK_REVID(dev_priv, since, until) \
	(IS_GEMINILAKE(dev_priv) && IS_REVID(dev_priv, since, until))

#define CNL_REVID_A0		0x0
#define CNL_REVID_B0		0x1
#define CNL_REVID_C0		0x2

#define IS_CNL_REVID(p, since, until) \
	(IS_CANNONLAKE(p) && IS_REVID(p, since, until))

#define ICL_REVID_A0		0x0
#define ICL_REVID_A2		0x1
#define ICL_REVID_B0		0x3
#define ICL_REVID_B2		0x4
#define ICL_REVID_C0		0x5

#define IS_ICL_REVID(p, since, until) \
	(IS_ICELAKE(p) && IS_REVID(p, since, until))

/*
 * The genX designation typically refers to the render engine, so render
 * capability related checks should use IS_GEN, while display and other checks
 * have their own (e.g. HAS_PCH_SPLIT for ILK+ display, IS_foo for particular
 * chips, etc.).
 */
#define IS_GEN2(dev_priv)	(!!((dev_priv)->info.gen_mask & BIT(1)))
#define IS_GEN3(dev_priv)	(!!((dev_priv)->info.gen_mask & BIT(2)))
#define IS_GEN4(dev_priv)	(!!((dev_priv)->info.gen_mask & BIT(3)))
#define IS_GEN5(dev_priv)	(!!((dev_priv)->info.gen_mask & BIT(4)))
#define IS_GEN6(dev_priv)	(!!((dev_priv)->info.gen_mask & BIT(5)))
#define IS_GEN7(dev_priv)	(!!((dev_priv)->info.gen_mask & BIT(6)))
#define IS_GEN8(dev_priv)	(!!((dev_priv)->info.gen_mask & BIT(7)))
#define IS_GEN9(dev_priv)	(!!((dev_priv)->info.gen_mask & BIT(8)))
#define IS_GEN10(dev_priv)	(!!((dev_priv)->info.gen_mask & BIT(9)))
#define IS_GEN11(dev_priv)	(!!((dev_priv)->info.gen_mask & BIT(10)))

#define IS_LP(dev_priv)	(INTEL_INFO(dev_priv)->is_lp)
#define IS_GEN9_LP(dev_priv)	(IS_GEN9(dev_priv) && IS_LP(dev_priv))
#define IS_GEN9_BC(dev_priv)	(IS_GEN9(dev_priv) && !IS_LP(dev_priv))

#define ENGINE_MASK(id)	BIT(id)
#define RENDER_RING	ENGINE_MASK(RCS)
#define BSD_RING	ENGINE_MASK(VCS)
#define BLT_RING	ENGINE_MASK(BCS)
#define VEBOX_RING	ENGINE_MASK(VECS)
#define BSD2_RING	ENGINE_MASK(VCS2)
#define BSD3_RING	ENGINE_MASK(VCS3)
#define BSD4_RING	ENGINE_MASK(VCS4)
#define VEBOX2_RING	ENGINE_MASK(VECS2)
#define ALL_ENGINES	(~0)

#define HAS_ENGINE(dev_priv, id) \
	(!!((dev_priv)->info.ring_mask & ENGINE_MASK(id)))

#define HAS_BSD(dev_priv)	HAS_ENGINE(dev_priv, VCS)
#define HAS_BSD2(dev_priv)	HAS_ENGINE(dev_priv, VCS2)
#define HAS_BLT(dev_priv)	HAS_ENGINE(dev_priv, BCS)
#define HAS_VEBOX(dev_priv)	HAS_ENGINE(dev_priv, VECS)

#define HAS_LEGACY_SEMAPHORES(dev_priv) IS_GEN7(dev_priv)

#define HAS_LLC(dev_priv)	((dev_priv)->info.has_llc)
#define HAS_SNOOP(dev_priv)	((dev_priv)->info.has_snoop)
#define HAS_EDRAM(dev_priv)	(!!((dev_priv)->edram_cap & EDRAM_ENABLED))
#define HAS_WT(dev_priv)	((IS_HASWELL(dev_priv) || \
				 IS_BROADWELL(dev_priv)) && HAS_EDRAM(dev_priv))

#define HWS_NEEDS_PHYSICAL(dev_priv)	((dev_priv)->info.hws_needs_physical)

#define HAS_LOGICAL_RING_CONTEXTS(dev_priv) \
		((dev_priv)->info.has_logical_ring_contexts)
#define HAS_LOGICAL_RING_ELSQ(dev_priv) \
		((dev_priv)->info.has_logical_ring_elsq)
#define HAS_LOGICAL_RING_PREEMPTION(dev_priv) \
		((dev_priv)->info.has_logical_ring_preemption)

#define HAS_EXECLISTS(dev_priv) HAS_LOGICAL_RING_CONTEXTS(dev_priv)

#define USES_PPGTT(dev_priv)		(i915_modparams.enable_ppgtt)
#define USES_FULL_PPGTT(dev_priv)	(i915_modparams.enable_ppgtt >= 2)
#define USES_FULL_48BIT_PPGTT(dev_priv)	(i915_modparams.enable_ppgtt == 3)
#define HAS_PAGE_SIZES(dev_priv, sizes) ({ \
	GEM_BUG_ON((sizes) == 0); \
	((sizes) & ~(dev_priv)->info.page_sizes) == 0; \
})

#define HAS_OVERLAY(dev_priv)		 ((dev_priv)->info.has_overlay)
#define OVERLAY_NEEDS_PHYSICAL(dev_priv) \
		((dev_priv)->info.overlay_needs_physical)

/* Early gen2 have a totally busted CS tlb and require pinned batches. */
#define HAS_BROKEN_CS_TLB(dev_priv)	(IS_I830(dev_priv) || IS_I845G(dev_priv))

/* WaRsDisableCoarsePowerGating:skl,cnl */
#define NEEDS_WaRsDisableCoarsePowerGating(dev_priv) \
	(IS_CANNONLAKE(dev_priv) || \
	 IS_SKL_GT3(dev_priv) || IS_SKL_GT4(dev_priv))

#define HAS_GMBUS_IRQ(dev_priv) (INTEL_GEN(dev_priv) >= 4)
#define HAS_GMBUS_BURST_READ(dev_priv) (INTEL_GEN(dev_priv) >= 10 || \
					IS_GEMINILAKE(dev_priv) || \
					IS_KABYLAKE(dev_priv))

/* With the 945 and later, Y tiling got adjusted so that it was 32 128-byte
 * rows, which changed the alignment requirements and fence programming.
 */
#define HAS_128_BYTE_Y_TILING(dev_priv) (!IS_GEN2(dev_priv) && \
					 !(IS_I915G(dev_priv) || \
					 IS_I915GM(dev_priv)))
#define SUPPORTS_TV(dev_priv)		((dev_priv)->info.supports_tv)
#define I915_HAS_HOTPLUG(dev_priv)	((dev_priv)->info.has_hotplug)

#define HAS_FW_BLC(dev_priv) 	(INTEL_GEN(dev_priv) > 2)
#define HAS_FBC(dev_priv)	((dev_priv)->info.has_fbc)
#define HAS_CUR_FBC(dev_priv)	(!HAS_GMCH_DISPLAY(dev_priv) && INTEL_GEN(dev_priv) >= 7)

#define HAS_IPS(dev_priv)	(IS_HSW_ULT(dev_priv) || IS_BROADWELL(dev_priv))

#define HAS_DP_MST(dev_priv)	((dev_priv)->info.has_dp_mst)

#define HAS_DDI(dev_priv)		 ((dev_priv)->info.has_ddi)
#define HAS_FPGA_DBG_UNCLAIMED(dev_priv) ((dev_priv)->info.has_fpga_dbg)
#define HAS_PSR(dev_priv)		 ((dev_priv)->info.has_psr)

#define HAS_RC6(dev_priv)		 ((dev_priv)->info.has_rc6)
#define HAS_RC6p(dev_priv)		 ((dev_priv)->info.has_rc6p)
#define HAS_RC6pp(dev_priv)		 (false) /* HW was never validated */

#define HAS_CSR(dev_priv)	((dev_priv)->info.has_csr)

#define HAS_RUNTIME_PM(dev_priv) ((dev_priv)->info.has_runtime_pm)
#define HAS_64BIT_RELOC(dev_priv) ((dev_priv)->info.has_64bit_reloc)

#define HAS_IPC(dev_priv)		 ((dev_priv)->info.has_ipc)

/*
 * For now, anything with a GuC requires uCode loading, and then supports
 * command submission once loaded. But these are logically independent
 * properties, so we have separate macros to test them.
 */
#define HAS_GUC(dev_priv)	((dev_priv)->info.has_guc)
#define HAS_GUC_CT(dev_priv)	((dev_priv)->info.has_guc_ct)
#define HAS_GUC_UCODE(dev_priv)	(HAS_GUC(dev_priv))
#define HAS_GUC_SCHED(dev_priv)	(HAS_GUC(dev_priv))

/* For now, anything with a GuC has also HuC */
#define HAS_HUC(dev_priv)	(HAS_GUC(dev_priv))
#define HAS_HUC_UCODE(dev_priv)	(HAS_GUC(dev_priv))

/* Having a GuC is not the same as using a GuC */
#define USES_GUC(dev_priv)		intel_uc_is_using_guc()
#define USES_GUC_SUBMISSION(dev_priv)	intel_uc_is_using_guc_submission()
#define USES_HUC(dev_priv)		intel_uc_is_using_huc()

#define HAS_RESOURCE_STREAMER(dev_priv) ((dev_priv)->info.has_resource_streamer)

#define HAS_POOLED_EU(dev_priv)	((dev_priv)->info.has_pooled_eu)

#define INTEL_PCH_DEVICE_ID_MASK		0xff80
#define INTEL_PCH_IBX_DEVICE_ID_TYPE		0x3b00
#define INTEL_PCH_CPT_DEVICE_ID_TYPE		0x1c00
#define INTEL_PCH_PPT_DEVICE_ID_TYPE		0x1e00
#define INTEL_PCH_LPT_DEVICE_ID_TYPE		0x8c00
#define INTEL_PCH_LPT_LP_DEVICE_ID_TYPE		0x9c00
#define INTEL_PCH_WPT_DEVICE_ID_TYPE		0x8c80
#define INTEL_PCH_WPT_LP_DEVICE_ID_TYPE		0x9c80
#define INTEL_PCH_SPT_DEVICE_ID_TYPE		0xA100
#define INTEL_PCH_SPT_LP_DEVICE_ID_TYPE		0x9D00
#define INTEL_PCH_KBP_DEVICE_ID_TYPE		0xA280
#define INTEL_PCH_CNP_DEVICE_ID_TYPE		0xA300
#define INTEL_PCH_CNP_LP_DEVICE_ID_TYPE		0x9D80
#define INTEL_PCH_ICP_DEVICE_ID_TYPE		0x3480
#define INTEL_PCH_P2X_DEVICE_ID_TYPE		0x7100
#define INTEL_PCH_P3X_DEVICE_ID_TYPE		0x7000
#define INTEL_PCH_QEMU_DEVICE_ID_TYPE		0x2900 /* qemu q35 has 2918 */

#define INTEL_PCH_TYPE(dev_priv) ((dev_priv)->pch_type)
#define INTEL_PCH_ID(dev_priv) ((dev_priv)->pch_id)
#define HAS_PCH_ICP(dev_priv) (INTEL_PCH_TYPE(dev_priv) == PCH_ICP)
#define HAS_PCH_CNP(dev_priv) (INTEL_PCH_TYPE(dev_priv) == PCH_CNP)
#define HAS_PCH_CNP_LP(dev_priv) \
	(INTEL_PCH_ID(dev_priv) == INTEL_PCH_CNP_LP_DEVICE_ID_TYPE)
#define HAS_PCH_KBP(dev_priv) (INTEL_PCH_TYPE(dev_priv) == PCH_KBP)
#define HAS_PCH_SPT(dev_priv) (INTEL_PCH_TYPE(dev_priv) == PCH_SPT)
#define HAS_PCH_LPT(dev_priv) (INTEL_PCH_TYPE(dev_priv) == PCH_LPT)
#define HAS_PCH_LPT_LP(dev_priv) \
	(INTEL_PCH_ID(dev_priv) == INTEL_PCH_LPT_LP_DEVICE_ID_TYPE || \
	 INTEL_PCH_ID(dev_priv) == INTEL_PCH_WPT_LP_DEVICE_ID_TYPE)
#define HAS_PCH_LPT_H(dev_priv) \
	(INTEL_PCH_ID(dev_priv) == INTEL_PCH_LPT_DEVICE_ID_TYPE || \
	 INTEL_PCH_ID(dev_priv) == INTEL_PCH_WPT_DEVICE_ID_TYPE)
#define HAS_PCH_CPT(dev_priv) (INTEL_PCH_TYPE(dev_priv) == PCH_CPT)
#define HAS_PCH_IBX(dev_priv) (INTEL_PCH_TYPE(dev_priv) == PCH_IBX)
#define HAS_PCH_NOP(dev_priv) (INTEL_PCH_TYPE(dev_priv) == PCH_NOP)
#define HAS_PCH_SPLIT(dev_priv) (INTEL_PCH_TYPE(dev_priv) != PCH_NONE)

#define HAS_GMCH_DISPLAY(dev_priv) ((dev_priv)->info.has_gmch_display)

#define HAS_LSPCON(dev_priv) (INTEL_GEN(dev_priv) >= 9)

/* DPF == dynamic parity feature */
#define HAS_L3_DPF(dev_priv) ((dev_priv)->info.has_l3_dpf)
#define NUM_L3_SLICES(dev_priv) (IS_HSW_GT3(dev_priv) ? \
				 2 : HAS_L3_DPF(dev_priv))

#define GT_FREQUENCY_MULTIPLIER 50
#define GEN9_FREQ_SCALER 3

#include "i915_trace.h"

static inline bool intel_vtd_active(void)
{
#ifdef CONFIG_INTEL_IOMMU
	if (intel_iommu_gfx_mapped)
		return true;
#endif
	return false;
}

static inline bool intel_scanout_needs_vtd_wa(struct drm_i915_private *dev_priv)
{
	return INTEL_GEN(dev_priv) >= 6 && intel_vtd_active();
}

static inline bool
intel_ggtt_update_needs_vtd_wa(struct drm_i915_private *dev_priv)
{
	return IS_BROXTON(dev_priv) && intel_vtd_active();
}

int intel_sanitize_enable_ppgtt(struct drm_i915_private *dev_priv,
				int enable_ppgtt);

/* i915_drv.c */
void __printf(3, 4)
__i915_printk(struct drm_i915_private *dev_priv, const char *level,
	      const char *fmt, ...);

#define i915_report_error(dev_priv, fmt, ...)				   \
	__i915_printk(dev_priv, KERN_ERR, fmt, ##__VA_ARGS__)

#ifdef CONFIG_COMPAT
extern long i915_compat_ioctl(struct file *filp, unsigned int cmd,
			      unsigned long arg);
#else
#define i915_compat_ioctl NULL
#endif
extern const struct dev_pm_ops i915_pm_ops;

#ifdef __linux__
extern int i915_driver_load(struct pci_dev *pdev,
			    const struct pci_device_id *ent);
#endif
extern void i915_driver_unload(struct drm_device *dev);
extern int intel_gpu_reset(struct drm_i915_private *dev_priv, u32 engine_mask);
extern bool intel_has_gpu_reset(struct drm_i915_private *dev_priv);

extern void i915_reset(struct drm_i915_private *i915,
		       unsigned int stalled_mask,
		       const char *reason);
extern int i915_reset_engine(struct intel_engine_cs *engine,
			     const char *reason);

extern bool intel_has_reset_engine(struct drm_i915_private *dev_priv);
extern int intel_reset_guc(struct drm_i915_private *dev_priv);
extern int intel_guc_reset_engine(struct intel_guc *guc,
				  struct intel_engine_cs *engine);
extern void intel_engine_init_hangcheck(struct intel_engine_cs *engine);
extern void intel_hangcheck_init(struct drm_i915_private *dev_priv);
extern unsigned long i915_chipset_val(struct drm_i915_private *dev_priv);
extern unsigned long i915_mch_val(struct drm_i915_private *dev_priv);
extern unsigned long i915_gfx_val(struct drm_i915_private *dev_priv);
extern void i915_update_gfx_val(struct drm_i915_private *dev_priv);
int vlv_force_gfx_clock(struct drm_i915_private *dev_priv, bool on);

int intel_engines_init_mmio(struct drm_i915_private *dev_priv);
int intel_engines_init(struct drm_i915_private *dev_priv);

u32 intel_calculate_mcr_s_ss_select(struct drm_i915_private *dev_priv);

/* intel_hotplug.c */
void intel_hpd_irq_handler(struct drm_i915_private *dev_priv,
			   u32 pin_mask, u32 long_mask);
void intel_hpd_init(struct drm_i915_private *dev_priv);
void intel_hpd_init_work(struct drm_i915_private *dev_priv);
void intel_hpd_cancel_work(struct drm_i915_private *dev_priv);
enum hpd_pin intel_hpd_pin_default(struct drm_i915_private *dev_priv,
				   enum port port);
bool intel_hpd_disable(struct drm_i915_private *dev_priv, enum hpd_pin pin);
void intel_hpd_enable(struct drm_i915_private *dev_priv, enum hpd_pin pin);

/* i915_irq.c */
static inline void i915_queue_hangcheck(struct drm_i915_private *dev_priv)
{
	unsigned long delay;

	if (unlikely(!i915_modparams.enable_hangcheck))
		return;

	/* Don't continually defer the hangcheck so that it is always run at
	 * least once after work has been scheduled on any ring. Otherwise,
	 * we will ignore a hung ring if a second ring is kept busy.
	 */

	delay = round_jiffies_up_relative(DRM_I915_HANGCHECK_JIFFIES);
	queue_delayed_work(system_long_wq,
			   &dev_priv->gpu_error.hangcheck_work, delay);
}

__printf(4, 5)
void i915_handle_error(struct drm_i915_private *dev_priv,
		       u32 engine_mask,
		       unsigned long flags,
		       const char *fmt, ...);
#define I915_ERROR_CAPTURE BIT(0)

extern void intel_irq_init(struct drm_i915_private *dev_priv);
extern void intel_irq_fini(struct drm_i915_private *dev_priv);
int intel_irq_install(struct drm_i915_private *dev_priv);
void intel_irq_uninstall(struct drm_i915_private *dev_priv);

static inline bool intel_gvt_active(struct drm_i915_private *dev_priv)
{
	return dev_priv->gvt;
}

static inline bool intel_vgpu_active(struct drm_i915_private *dev_priv)
{
	return dev_priv->vgpu.active;
}

u32 i915_pipestat_enable_mask(struct drm_i915_private *dev_priv,
			      enum pipe pipe);
void
i915_enable_pipestat(struct drm_i915_private *dev_priv, enum pipe pipe,
		     u32 status_mask);

void
i915_disable_pipestat(struct drm_i915_private *dev_priv, enum pipe pipe,
		      u32 status_mask);

void valleyview_enable_display_irqs(struct drm_i915_private *dev_priv);
void valleyview_disable_display_irqs(struct drm_i915_private *dev_priv);
void i915_hotplug_interrupt_update(struct drm_i915_private *dev_priv,
				   uint32_t mask,
				   uint32_t bits);
void ilk_update_display_irq(struct drm_i915_private *dev_priv,
			    uint32_t interrupt_mask,
			    uint32_t enabled_irq_mask);
static inline void
ilk_enable_display_irq(struct drm_i915_private *dev_priv, uint32_t bits)
{
	ilk_update_display_irq(dev_priv, bits, bits);
}
static inline void
ilk_disable_display_irq(struct drm_i915_private *dev_priv, uint32_t bits)
{
	ilk_update_display_irq(dev_priv, bits, 0);
}
void bdw_update_pipe_irq(struct drm_i915_private *dev_priv,
			 enum pipe pipe,
			 uint32_t interrupt_mask,
			 uint32_t enabled_irq_mask);
static inline void bdw_enable_pipe_irq(struct drm_i915_private *dev_priv,
				       enum pipe pipe, uint32_t bits)
{
	bdw_update_pipe_irq(dev_priv, pipe, bits, bits);
}
static inline void bdw_disable_pipe_irq(struct drm_i915_private *dev_priv,
					enum pipe pipe, uint32_t bits)
{
	bdw_update_pipe_irq(dev_priv, pipe, bits, 0);
}
void ibx_display_interrupt_update(struct drm_i915_private *dev_priv,
				  uint32_t interrupt_mask,
				  uint32_t enabled_irq_mask);
static inline void
ibx_enable_display_interrupt(struct drm_i915_private *dev_priv, uint32_t bits)
{
	ibx_display_interrupt_update(dev_priv, bits, bits);
}
static inline void
ibx_disable_display_interrupt(struct drm_i915_private *dev_priv, uint32_t bits)
{
	ibx_display_interrupt_update(dev_priv, bits, 0);
}

/* i915_gem.c */
int i915_gem_create_ioctl(struct drm_device *dev, void *data,
			  struct drm_file *file_priv);
int i915_gem_pread_ioctl(struct drm_device *dev, void *data,
			 struct drm_file *file_priv);
int i915_gem_pwrite_ioctl(struct drm_device *dev, void *data,
			  struct drm_file *file_priv);
int i915_gem_mmap_ioctl(struct drm_device *dev, void *data,
			struct drm_file *file_priv);
int i915_gem_mmap_gtt_ioctl(struct drm_device *dev, void *data,
			struct drm_file *file_priv);
int i915_gem_set_domain_ioctl(struct drm_device *dev, void *data,
			      struct drm_file *file_priv);
int i915_gem_sw_finish_ioctl(struct drm_device *dev, void *data,
			     struct drm_file *file_priv);
int i915_gem_execbuffer_ioctl(struct drm_device *dev, void *data,
			      struct drm_file *file_priv);
int i915_gem_execbuffer2_ioctl(struct drm_device *dev, void *data,
			       struct drm_file *file_priv);
int i915_gem_busy_ioctl(struct drm_device *dev, void *data,
			struct drm_file *file_priv);
int i915_gem_get_caching_ioctl(struct drm_device *dev, void *data,
			       struct drm_file *file);
int i915_gem_set_caching_ioctl(struct drm_device *dev, void *data,
			       struct drm_file *file);
int i915_gem_throttle_ioctl(struct drm_device *dev, void *data,
			    struct drm_file *file_priv);
int i915_gem_madvise_ioctl(struct drm_device *dev, void *data,
			   struct drm_file *file_priv);
int i915_gem_set_tiling_ioctl(struct drm_device *dev, void *data,
			      struct drm_file *file_priv);
int i915_gem_get_tiling_ioctl(struct drm_device *dev, void *data,
			      struct drm_file *file_priv);
int i915_gem_init_userptr(struct drm_i915_private *dev_priv);
void i915_gem_cleanup_userptr(struct drm_i915_private *dev_priv);
int i915_gem_userptr_ioctl(struct drm_device *dev, void *data,
			   struct drm_file *file);
int i915_gem_get_aperture_ioctl(struct drm_device *dev, void *data,
				struct drm_file *file_priv);
int i915_gem_wait_ioctl(struct drm_device *dev, void *data,
			struct drm_file *file_priv);
void i915_gem_sanitize(struct drm_i915_private *i915);
int i915_gem_init_early(struct drm_i915_private *dev_priv);
void i915_gem_cleanup_early(struct drm_i915_private *dev_priv);
void i915_gem_load_init_fences(struct drm_i915_private *dev_priv);
int i915_gem_freeze(struct drm_i915_private *dev_priv);
int i915_gem_freeze_late(struct drm_i915_private *dev_priv);

void *i915_gem_object_alloc(struct drm_i915_private *dev_priv);
void i915_gem_object_free(struct drm_i915_gem_object *obj);
void i915_gem_object_init(struct drm_i915_gem_object *obj,
			 const struct drm_i915_gem_object_ops *ops);
struct drm_i915_gem_object *
i915_gem_object_create(struct drm_i915_private *dev_priv, u64 size);
struct drm_i915_gem_object *
i915_gem_object_create_from_data(struct drm_i915_private *dev_priv,
				 const void *data, size_t size);
void i915_gem_close_object(struct drm_gem_object *gem, struct drm_file *file);
void i915_gem_free_object(struct drm_gem_object *obj);

static inline void i915_gem_drain_freed_objects(struct drm_i915_private *i915)
{
	if (!atomic_read(&i915->mm.free_count))
		return;

	/* A single pass should suffice to release all the freed objects (along
	 * most call paths) , but be a little more paranoid in that freeing
	 * the objects does take a little amount of time, during which the rcu
	 * callbacks could have added new objects into the freed list, and
	 * armed the work again.
	 */
	do {
		rcu_barrier();
	} while (flush_work(&i915->mm.free_work));
}

static inline void i915_gem_drain_workqueue(struct drm_i915_private *i915)
{
	/*
	 * Similar to objects above (see i915_gem_drain_freed-objects), in
	 * general we have workers that are armed by RCU and then rearm
	 * themselves in their callbacks. To be paranoid, we need to
	 * drain the workqueue a second time after waiting for the RCU
	 * grace period so that we catch work queued via RCU from the first
	 * pass. As neither drain_workqueue() nor flush_workqueue() report
	 * a result, we make an assumption that we only don't require more
	 * than 2 passes to catch all recursive RCU delayed work.
	 *
	 */
	int pass = 2;
	do {
		rcu_barrier();
		drain_workqueue(i915->wq);
	} while (--pass);
}

struct i915_vma * __must_check
i915_gem_object_ggtt_pin(struct drm_i915_gem_object *obj,
			 const struct i915_ggtt_view *view,
			 u64 size,
			 u64 alignment,
			 u64 flags);

int i915_gem_object_unbind(struct drm_i915_gem_object *obj);
void i915_gem_release_mmap(struct drm_i915_gem_object *obj);

void i915_gem_runtime_suspend(struct drm_i915_private *dev_priv);

static inline int __sg_page_count(const struct scatterlist *sg)
{
	return sg->length >> PAGE_SHIFT;
}

struct scatterlist *
i915_gem_object_get_sg(struct drm_i915_gem_object *obj,
		       unsigned int n, unsigned int *offset);

struct vm_page *
i915_gem_object_get_page(struct drm_i915_gem_object *obj,
			 unsigned int n);

struct vm_page *
i915_gem_object_get_dirty_page(struct drm_i915_gem_object *obj,
			       unsigned int n);

dma_addr_t
i915_gem_object_get_dma_address(struct drm_i915_gem_object *obj,
				unsigned long n);

void __i915_gem_object_set_pages(struct drm_i915_gem_object *obj,
				 struct sg_table *pages,
				 unsigned int sg_page_sizes);
int __i915_gem_object_get_pages(struct drm_i915_gem_object *obj);

static inline int __must_check
i915_gem_object_pin_pages(struct drm_i915_gem_object *obj)
{
	might_lock(&obj->mm.lock);

	if (atomic_inc_not_zero(&obj->mm.pages_pin_count))
		return 0;

	return __i915_gem_object_get_pages(obj);
}

static inline bool
i915_gem_object_has_pages(struct drm_i915_gem_object *obj)
{
	return !IS_ERR_OR_NULL(READ_ONCE(obj->mm.pages));
}

static inline void
__i915_gem_object_pin_pages(struct drm_i915_gem_object *obj)
{
	GEM_BUG_ON(!i915_gem_object_has_pages(obj));

	atomic_inc(&obj->mm.pages_pin_count);
}

static inline bool
i915_gem_object_has_pinned_pages(struct drm_i915_gem_object *obj)
{
	return atomic_read(&obj->mm.pages_pin_count);
}

static inline void
__i915_gem_object_unpin_pages(struct drm_i915_gem_object *obj)
{
	GEM_BUG_ON(!i915_gem_object_has_pages(obj));
	GEM_BUG_ON(!i915_gem_object_has_pinned_pages(obj));

	atomic_dec(&obj->mm.pages_pin_count);
}

static inline void
i915_gem_object_unpin_pages(struct drm_i915_gem_object *obj)
{
	__i915_gem_object_unpin_pages(obj);
}

enum i915_mm_subclass { /* lockdep subclass for obj->mm.lock */
	I915_MM_NORMAL = 0,
	I915_MM_SHRINKER
};

void __i915_gem_object_put_pages(struct drm_i915_gem_object *obj,
				 enum i915_mm_subclass subclass);
void __i915_gem_object_invalidate(struct drm_i915_gem_object *obj);

enum i915_map_type {
	I915_MAP_WB = 0,
	I915_MAP_WC,
#define I915_MAP_OVERRIDE BIT(31)
	I915_MAP_FORCE_WB = I915_MAP_WB | I915_MAP_OVERRIDE,
	I915_MAP_FORCE_WC = I915_MAP_WC | I915_MAP_OVERRIDE,
};

/**
 * i915_gem_object_pin_map - return a contiguous mapping of the entire object
 * @obj: the object to map into kernel address space
 * @type: the type of mapping, used to select pgprot_t
 *
 * Calls i915_gem_object_pin_pages() to prevent reaping of the object's
 * pages and then returns a contiguous mapping of the backing storage into
 * the kernel address space. Based on the @type of mapping, the PTE will be
 * set to either WriteBack or WriteCombine (via pgprot_t).
 *
 * The caller is responsible for calling i915_gem_object_unpin_map() when the
 * mapping is no longer required.
 *
 * Returns the pointer through which to access the mapped object, or an
 * ERR_PTR() on error.
 */
void *__must_check i915_gem_object_pin_map(struct drm_i915_gem_object *obj,
					   enum i915_map_type type);

/**
 * i915_gem_object_unpin_map - releases an earlier mapping
 * @obj: the object to unmap
 *
 * After pinning the object and mapping its pages, once you are finished
 * with your access, call i915_gem_object_unpin_map() to release the pin
 * upon the mapping. Once the pin count reaches zero, that mapping may be
 * removed.
 */
static inline void i915_gem_object_unpin_map(struct drm_i915_gem_object *obj)
{
	i915_gem_object_unpin_pages(obj);
}

int i915_gem_obj_prepare_shmem_read(struct drm_i915_gem_object *obj,
				    unsigned int *needs_clflush);
int i915_gem_obj_prepare_shmem_write(struct drm_i915_gem_object *obj,
				     unsigned int *needs_clflush);
#define CLFLUSH_BEFORE	BIT(0)
#define CLFLUSH_AFTER	BIT(1)
#define CLFLUSH_FLAGS	(CLFLUSH_BEFORE | CLFLUSH_AFTER)

static inline void
i915_gem_obj_finish_shmem_access(struct drm_i915_gem_object *obj)
{
	i915_gem_object_unpin_pages(obj);
}

int __must_check i915_mutex_lock_interruptible(struct drm_device *dev);
int i915_gem_dumb_create(struct drm_file *file_priv,
			 struct drm_device *dev,
			 struct drm_mode_create_dumb *args);
int i915_gem_mmap_gtt(struct drm_file *file_priv, struct drm_device *dev,
		      uint32_t handle, uint64_t *offset);
int i915_gem_mmap_gtt_version(void);

void i915_gem_track_fb(struct drm_i915_gem_object *old,
		       struct drm_i915_gem_object *new,
		       unsigned frontbuffer_bits);

int __must_check i915_gem_set_global_seqno(struct drm_device *dev, u32 seqno);

struct i915_request *
i915_gem_find_active_request(struct intel_engine_cs *engine);

static inline bool i915_reset_backoff(struct i915_gpu_error *error)
{
	return unlikely(test_bit(I915_RESET_BACKOFF, &error->flags));
}

static inline bool i915_reset_handoff(struct i915_gpu_error *error)
{
	return unlikely(test_bit(I915_RESET_HANDOFF, &error->flags));
}

static inline bool i915_terminally_wedged(struct i915_gpu_error *error)
{
	return unlikely(test_bit(I915_WEDGED, &error->flags));
}

static inline bool i915_reset_backoff_or_wedged(struct i915_gpu_error *error)
{
	return i915_reset_backoff(error) | i915_terminally_wedged(error);
}

static inline u32 i915_reset_count(struct i915_gpu_error *error)
{
	return READ_ONCE(error->reset_count);
}

static inline u32 i915_reset_engine_count(struct i915_gpu_error *error,
					  struct intel_engine_cs *engine)
{
	return READ_ONCE(error->reset_engine_count[engine->id]);
}

struct i915_request *
i915_gem_reset_prepare_engine(struct intel_engine_cs *engine);
int i915_gem_reset_prepare(struct drm_i915_private *dev_priv);
void i915_gem_reset(struct drm_i915_private *dev_priv,
		    unsigned int stalled_mask);
void i915_gem_reset_finish_engine(struct intel_engine_cs *engine);
void i915_gem_reset_finish(struct drm_i915_private *dev_priv);
void i915_gem_set_wedged(struct drm_i915_private *dev_priv);
bool i915_gem_unset_wedged(struct drm_i915_private *dev_priv);
void i915_gem_reset_engine(struct intel_engine_cs *engine,
			   struct i915_request *request,
			   bool stalled);

void i915_gem_init_mmio(struct drm_i915_private *i915);
int __must_check i915_gem_init(struct drm_i915_private *dev_priv);
int __must_check i915_gem_init_hw(struct drm_i915_private *dev_priv);
void i915_gem_init_swizzling(struct drm_i915_private *dev_priv);
void i915_gem_fini(struct drm_i915_private *dev_priv);
void i915_gem_cleanup_engines(struct drm_i915_private *dev_priv);
int i915_gem_wait_for_idle(struct drm_i915_private *dev_priv,
			   unsigned int flags, long timeout);
int __must_check i915_gem_suspend(struct drm_i915_private *dev_priv);
void i915_gem_suspend_late(struct drm_i915_private *dev_priv);
void i915_gem_resume(struct drm_i915_private *dev_priv);
#ifdef __linux__
vm_fault_t i915_gem_fault(struct vm_fault *vmf);
#else
int i915_gem_fault(struct drm_gem_object *gem_obj, struct uvm_faultinfo *ufi,
		   off_t offset, vaddr_t vaddr, vm_page_t *pps, int npages,
		   int centeridx, vm_prot_t access_type, int flags);
#endif
int i915_gem_object_wait(struct drm_i915_gem_object *obj,
			 unsigned int flags,
			 long timeout,
			 struct intel_rps_client *rps);
int i915_gem_object_wait_priority(struct drm_i915_gem_object *obj,
				  unsigned int flags,
				  const struct i915_sched_attr *attr);
#define I915_PRIORITY_DISPLAY I915_PRIORITY_MAX

int __must_check
i915_gem_object_set_to_wc_domain(struct drm_i915_gem_object *obj, bool write);
int __must_check
i915_gem_object_set_to_gtt_domain(struct drm_i915_gem_object *obj, bool write);
int __must_check
i915_gem_object_set_to_cpu_domain(struct drm_i915_gem_object *obj, bool write);
struct i915_vma * __must_check
i915_gem_object_pin_to_display_plane(struct drm_i915_gem_object *obj,
				     u32 alignment,
				     const struct i915_ggtt_view *view,
				     unsigned int flags);
void i915_gem_object_unpin_from_display_plane(struct i915_vma *vma);
int i915_gem_object_attach_phys(struct drm_i915_gem_object *obj,
				int align);
int i915_gem_open(struct drm_i915_private *i915, struct drm_file *file);
void i915_gem_release(struct drm_device *dev, struct drm_file *file);

int i915_gem_object_set_cache_level(struct drm_i915_gem_object *obj,
				    enum i915_cache_level cache_level);

struct drm_gem_object *i915_gem_prime_import(struct drm_device *dev,
				struct dma_buf *dma_buf);

struct dma_buf *i915_gem_prime_export(struct drm_device *dev,
				struct drm_gem_object *gem_obj, int flags);

static inline struct i915_hw_ppgtt *
i915_vm_to_ppgtt(struct i915_address_space *vm)
{
	return container_of(vm, struct i915_hw_ppgtt, vm);
}

/* i915_gem_fence_reg.c */
struct drm_i915_fence_reg *
i915_reserve_fence(struct drm_i915_private *dev_priv);
void i915_unreserve_fence(struct drm_i915_fence_reg *fence);

void i915_gem_revoke_fences(struct drm_i915_private *dev_priv);
void i915_gem_restore_fences(struct drm_i915_private *dev_priv);

void i915_gem_detect_bit_6_swizzle(struct drm_i915_private *dev_priv);
void i915_gem_object_do_bit_17_swizzle(struct drm_i915_gem_object *obj,
				       struct sg_table *pages);
void i915_gem_object_save_bit_17_swizzle(struct drm_i915_gem_object *obj,
					 struct sg_table *pages);

static inline struct i915_gem_context *
__i915_gem_context_lookup_rcu(struct drm_i915_file_private *file_priv, u32 id)
{
	return idr_find(&file_priv->context_idr, id);
}

static inline struct i915_gem_context *
i915_gem_context_lookup(struct drm_i915_file_private *file_priv, u32 id)
{
	struct i915_gem_context *ctx;

	rcu_read_lock();
	ctx = __i915_gem_context_lookup_rcu(file_priv, id);
	if (ctx && !kref_get_unless_zero(&ctx->ref))
		ctx = NULL;
	rcu_read_unlock();

	return ctx;
}

int i915_perf_open_ioctl(struct drm_device *dev, void *data,
			 struct drm_file *file);
int i915_perf_add_config_ioctl(struct drm_device *dev, void *data,
			       struct drm_file *file);
int i915_perf_remove_config_ioctl(struct drm_device *dev, void *data,
				  struct drm_file *file);
void i915_oa_init_reg_state(struct intel_engine_cs *engine,
			    struct i915_gem_context *ctx,
			    uint32_t *reg_state);

/* i915_gem_evict.c */
int __must_check i915_gem_evict_something(struct i915_address_space *vm,
					  u64 min_size, u64 alignment,
					  unsigned cache_level,
					  u64 start, u64 end,
					  unsigned flags);
int __must_check i915_gem_evict_for_node(struct i915_address_space *vm,
					 struct drm_mm_node *node,
					 unsigned int flags);
int i915_gem_evict_vm(struct i915_address_space *vm);

void i915_gem_flush_ggtt_writes(struct drm_i915_private *dev_priv);

/* belongs in i915_gem_gtt.h */
static inline void i915_gem_chipset_flush(struct drm_i915_private *dev_priv)
{
	wmb();
	if (INTEL_GEN(dev_priv) < 6)
		intel_gtt_chipset_flush();
}

/* i915_gem_stolen.c */
int i915_gem_stolen_insert_node(struct drm_i915_private *dev_priv,
				struct drm_mm_node *node, u64 size,
				unsigned alignment);
int i915_gem_stolen_insert_node_in_range(struct drm_i915_private *dev_priv,
					 struct drm_mm_node *node, u64 size,
					 unsigned alignment, u64 start,
					 u64 end);
void i915_gem_stolen_remove_node(struct drm_i915_private *dev_priv,
				 struct drm_mm_node *node);
int i915_gem_init_stolen(struct drm_i915_private *dev_priv);
void i915_gem_cleanup_stolen(struct drm_device *dev);
struct drm_i915_gem_object *
i915_gem_object_create_stolen(struct drm_i915_private *dev_priv,
			      resource_size_t size);
struct drm_i915_gem_object *
i915_gem_object_create_stolen_for_preallocated(struct drm_i915_private *dev_priv,
					       resource_size_t stolen_offset,
					       resource_size_t gtt_offset,
					       resource_size_t size);

/* i915_gem_internal.c */
struct drm_i915_gem_object *
i915_gem_object_create_internal(struct drm_i915_private *dev_priv,
				phys_addr_t size);

/* i915_gem_shrinker.c */
unsigned long i915_gem_shrink(struct drm_i915_private *i915,
			      unsigned long target,
			      unsigned long *nr_scanned,
			      unsigned flags);
#define I915_SHRINK_PURGEABLE 0x1
#define I915_SHRINK_UNBOUND 0x2
#define I915_SHRINK_BOUND 0x4
#define I915_SHRINK_ACTIVE 0x8
#define I915_SHRINK_VMAPS 0x10
unsigned long i915_gem_shrink_all(struct drm_i915_private *i915);
void i915_gem_shrinker_register(struct drm_i915_private *i915);
void i915_gem_shrinker_unregister(struct drm_i915_private *i915);
void i915_gem_shrinker_taints_mutex(struct rwlock *mutex);

/* i915_gem_tiling.c */
static inline bool i915_gem_object_needs_bit17_swizzle(struct drm_i915_gem_object *obj)
{
	struct drm_i915_private *dev_priv = to_i915(obj->base.dev);

	return dev_priv->mm.bit_6_swizzle_x == I915_BIT_6_SWIZZLE_9_10_17 &&
		i915_gem_object_is_tiled(obj);
}

u32 i915_gem_fence_size(struct drm_i915_private *dev_priv, u32 size,
			unsigned int tiling, unsigned int stride);
u32 i915_gem_fence_alignment(struct drm_i915_private *dev_priv, u32 size,
			     unsigned int tiling, unsigned int stride);

/* i915_debugfs.c */
#ifdef CONFIG_DEBUG_FS
int i915_debugfs_register(struct drm_i915_private *dev_priv);
int i915_debugfs_connector_add(struct drm_connector *connector);
void intel_display_crc_init(struct drm_i915_private *dev_priv);
#else
static inline int i915_debugfs_register(struct drm_i915_private *dev_priv) {return 0;}
static inline int i915_debugfs_connector_add(struct drm_connector *connector)
{ return 0; }
static inline void intel_display_crc_init(struct drm_i915_private *dev_priv) {}
#endif

const char *i915_cache_level_str(struct drm_i915_private *i915, int type);

/* i915_cmd_parser.c */
int i915_cmd_parser_get_version(struct drm_i915_private *dev_priv);
void intel_engine_init_cmd_parser(struct intel_engine_cs *engine);
void intel_engine_cleanup_cmd_parser(struct intel_engine_cs *engine);
int intel_engine_cmd_parser(struct intel_engine_cs *engine,
			    struct drm_i915_gem_object *batch_obj,
			    struct drm_i915_gem_object *shadow_batch_obj,
			    u32 batch_start_offset,
			    u32 batch_len,
			    bool is_master);

/* i915_perf.c */
extern void i915_perf_init(struct drm_i915_private *dev_priv);
extern void i915_perf_fini(struct drm_i915_private *dev_priv);
extern void i915_perf_register(struct drm_i915_private *dev_priv);
extern void i915_perf_unregister(struct drm_i915_private *dev_priv);

/* i915_suspend.c */
extern int i915_save_state(struct drm_i915_private *dev_priv);
extern int i915_restore_state(struct drm_i915_private *dev_priv);

/* i915_sysfs.c */
void i915_setup_sysfs(struct drm_i915_private *dev_priv);
void i915_teardown_sysfs(struct drm_i915_private *dev_priv);

/* intel_lpe_audio.c */
int  intel_lpe_audio_init(struct drm_i915_private *dev_priv);
void intel_lpe_audio_teardown(struct drm_i915_private *dev_priv);
void intel_lpe_audio_irq_handler(struct drm_i915_private *dev_priv);
void intel_lpe_audio_notify(struct drm_i915_private *dev_priv,
			    enum pipe pipe, enum port port,
			    const void *eld, int ls_clock, bool dp_output);

/* intel_i2c.c */
extern int intel_setup_gmbus(struct drm_i915_private *dev_priv);
extern void intel_teardown_gmbus(struct drm_i915_private *dev_priv);
extern bool intel_gmbus_is_valid_pin(struct drm_i915_private *dev_priv,
				     unsigned int pin);
extern int intel_gmbus_output_aksv(struct i2c_adapter *adapter);

extern struct i2c_adapter *
intel_gmbus_get_adapter(struct drm_i915_private *dev_priv, unsigned int pin);
extern void intel_gmbus_set_speed(struct i2c_adapter *adapter, int speed);
extern void intel_gmbus_force_bit(struct i2c_adapter *adapter, bool force_bit);
static inline bool intel_gmbus_is_forced_bit(struct i2c_adapter *adapter)
{
	return container_of(adapter, struct intel_gmbus, adapter)->force_bit;
}
extern void intel_i2c_reset(struct drm_i915_private *dev_priv);

/* intel_bios.c */
void intel_bios_init(struct drm_i915_private *dev_priv);
void intel_bios_cleanup(struct drm_i915_private *dev_priv);
bool intel_bios_is_valid_vbt(const void *buf, size_t size);
bool intel_bios_is_tv_present(struct drm_i915_private *dev_priv);
bool intel_bios_is_lvds_present(struct drm_i915_private *dev_priv, u8 *i2c_pin);
bool intel_bios_is_port_present(struct drm_i915_private *dev_priv, enum port port);
bool intel_bios_is_port_edp(struct drm_i915_private *dev_priv, enum port port);
bool intel_bios_is_port_dp_dual_mode(struct drm_i915_private *dev_priv, enum port port);
bool intel_bios_is_dsi_present(struct drm_i915_private *dev_priv, enum port *port);
bool intel_bios_is_port_hpd_inverted(struct drm_i915_private *dev_priv,
				     enum port port);
bool intel_bios_is_lspcon_present(struct drm_i915_private *dev_priv,
				enum port port);

/* intel_acpi.c */
#if defined(CONFIG_ACPI) && defined(__linux__)
extern void intel_register_dsm_handler(void);
extern void intel_unregister_dsm_handler(void);
#else
static inline void intel_register_dsm_handler(void) { return; }
static inline void intel_unregister_dsm_handler(void) { return; }
#endif /* CONFIG_ACPI */

/* intel_device_info.c */
static inline struct intel_device_info *
mkwrite_device_info(struct drm_i915_private *dev_priv)
{
	return (struct intel_device_info *)&dev_priv->info;
}

/* modesetting */
extern void intel_modeset_init_hw(struct drm_device *dev);
extern int intel_modeset_init(struct drm_device *dev);
extern void intel_modeset_cleanup(struct drm_device *dev);
extern int intel_connector_register(struct drm_connector *);
extern void intel_connector_unregister(struct drm_connector *);
extern int intel_modeset_vga_set_state(struct drm_i915_private *dev_priv,
				       bool state);
extern void intel_display_resume(struct drm_device *dev);
extern void i915_redisable_vga(struct drm_i915_private *dev_priv);
extern void i915_redisable_vga_power_on(struct drm_i915_private *dev_priv);
extern bool ironlake_set_drps(struct drm_i915_private *dev_priv, u8 val);
extern void intel_init_pch_refclk(struct drm_i915_private *dev_priv);
extern int intel_set_rps(struct drm_i915_private *dev_priv, u8 val);
extern void intel_rps_mark_interactive(struct drm_i915_private *i915,
				       bool interactive);
extern bool intel_set_memory_cxsr(struct drm_i915_private *dev_priv,
				  bool enable);

int i915_reg_read_ioctl(struct drm_device *dev, void *data,
			struct drm_file *file);

/* overlay */
extern struct intel_overlay_error_state *
intel_overlay_capture_error_state(struct drm_i915_private *dev_priv);
extern void intel_overlay_print_error_state(struct drm_i915_error_state_buf *e,
					    struct intel_overlay_error_state *error);

extern struct intel_display_error_state *
intel_display_capture_error_state(struct drm_i915_private *dev_priv);
extern void intel_display_print_error_state(struct drm_i915_error_state_buf *e,
					    struct intel_display_error_state *error);

int sandybridge_pcode_read(struct drm_i915_private *dev_priv, u32 mbox, u32 *val);
int sandybridge_pcode_write_timeout(struct drm_i915_private *dev_priv, u32 mbox,
				    u32 val, int fast_timeout_us,
				    int slow_timeout_ms);
#define sandybridge_pcode_write(dev_priv, mbox, val)	\
	sandybridge_pcode_write_timeout(dev_priv, mbox, val, 500, 0)

int skl_pcode_request(struct drm_i915_private *dev_priv, u32 mbox, u32 request,
		      u32 reply_mask, u32 reply, int timeout_base_ms);

/* intel_sideband.c */
u32 vlv_punit_read(struct drm_i915_private *dev_priv, u32 addr);
int vlv_punit_write(struct drm_i915_private *dev_priv, u32 addr, u32 val);
u32 vlv_nc_read(struct drm_i915_private *dev_priv, u8 addr);
u32 vlv_iosf_sb_read(struct drm_i915_private *dev_priv, u8 port, u32 reg);
void vlv_iosf_sb_write(struct drm_i915_private *dev_priv, u8 port, u32 reg, u32 val);
u32 vlv_cck_read(struct drm_i915_private *dev_priv, u32 reg);
void vlv_cck_write(struct drm_i915_private *dev_priv, u32 reg, u32 val);
u32 vlv_ccu_read(struct drm_i915_private *dev_priv, u32 reg);
void vlv_ccu_write(struct drm_i915_private *dev_priv, u32 reg, u32 val);
u32 vlv_bunit_read(struct drm_i915_private *dev_priv, u32 reg);
void vlv_bunit_write(struct drm_i915_private *dev_priv, u32 reg, u32 val);
u32 vlv_dpio_read(struct drm_i915_private *dev_priv, enum pipe pipe, int reg);
void vlv_dpio_write(struct drm_i915_private *dev_priv, enum pipe pipe, int reg, u32 val);
u32 intel_sbi_read(struct drm_i915_private *dev_priv, u16 reg,
		   enum intel_sbi_destination destination);
void intel_sbi_write(struct drm_i915_private *dev_priv, u16 reg, u32 value,
		     enum intel_sbi_destination destination);
u32 vlv_flisdsi_read(struct drm_i915_private *dev_priv, u32 reg);
void vlv_flisdsi_write(struct drm_i915_private *dev_priv, u32 reg, u32 val);

/* intel_dpio_phy.c */
void bxt_port_to_phy_channel(struct drm_i915_private *dev_priv, enum port port,
			     enum dpio_phy *phy, enum dpio_channel *ch);
void bxt_ddi_phy_set_signal_level(struct drm_i915_private *dev_priv,
				  enum port port, u32 margin, u32 scale,
				  u32 enable, u32 deemphasis);
void bxt_ddi_phy_init(struct drm_i915_private *dev_priv, enum dpio_phy phy);
void bxt_ddi_phy_uninit(struct drm_i915_private *dev_priv, enum dpio_phy phy);
bool bxt_ddi_phy_is_enabled(struct drm_i915_private *dev_priv,
			    enum dpio_phy phy);
bool bxt_ddi_phy_verify_state(struct drm_i915_private *dev_priv,
			      enum dpio_phy phy);
uint8_t bxt_ddi_phy_calc_lane_lat_optim_mask(uint8_t lane_count);
void bxt_ddi_phy_set_lane_optim_mask(struct intel_encoder *encoder,
				     uint8_t lane_lat_optim_mask);
uint8_t bxt_ddi_phy_get_lane_lat_optim_mask(struct intel_encoder *encoder);

void chv_set_phy_signal_level(struct intel_encoder *encoder,
			      u32 deemph_reg_value, u32 margin_reg_value,
			      bool uniq_trans_scale);
void chv_data_lane_soft_reset(struct intel_encoder *encoder,
			      const struct intel_crtc_state *crtc_state,
			      bool reset);
void chv_phy_pre_pll_enable(struct intel_encoder *encoder,
			    const struct intel_crtc_state *crtc_state);
void chv_phy_pre_encoder_enable(struct intel_encoder *encoder,
				const struct intel_crtc_state *crtc_state);
void chv_phy_release_cl2_override(struct intel_encoder *encoder);
void chv_phy_post_pll_disable(struct intel_encoder *encoder,
			      const struct intel_crtc_state *old_crtc_state);

void vlv_set_phy_signal_level(struct intel_encoder *encoder,
			      u32 demph_reg_value, u32 preemph_reg_value,
			      u32 uniqtranscale_reg_value, u32 tx3_demph);
void vlv_phy_pre_pll_enable(struct intel_encoder *encoder,
			    const struct intel_crtc_state *crtc_state);
void vlv_phy_pre_encoder_enable(struct intel_encoder *encoder,
				const struct intel_crtc_state *crtc_state);
void vlv_phy_reset_lanes(struct intel_encoder *encoder,
			 const struct intel_crtc_state *old_crtc_state);

int intel_gpu_freq(struct drm_i915_private *dev_priv, int val);
int intel_freq_opcode(struct drm_i915_private *dev_priv, int val);
u64 intel_rc6_residency_ns(struct drm_i915_private *dev_priv,
			   const i915_reg_t reg);

u32 intel_get_cagf(struct drm_i915_private *dev_priv, u32 rpstat1);

static inline u64 intel_rc6_residency_us(struct drm_i915_private *dev_priv,
					 const i915_reg_t reg)
{
	return DIV_ROUND_UP_ULL(intel_rc6_residency_ns(dev_priv, reg), 1000);
}

#define I915_READ8(reg)		dev_priv->uncore.funcs.mmio_readb(dev_priv, (reg), true)
#define I915_WRITE8(reg, val)	dev_priv->uncore.funcs.mmio_writeb(dev_priv, (reg), (val), true)

#define I915_READ16(reg)	dev_priv->uncore.funcs.mmio_readw(dev_priv, (reg), true)
#define I915_WRITE16(reg, val)	dev_priv->uncore.funcs.mmio_writew(dev_priv, (reg), (val), true)
#define I915_READ16_NOTRACE(reg)	dev_priv->uncore.funcs.mmio_readw(dev_priv, (reg), false)
#define I915_WRITE16_NOTRACE(reg, val)	dev_priv->uncore.funcs.mmio_writew(dev_priv, (reg), (val), false)

#define I915_READ(reg)		dev_priv->uncore.funcs.mmio_readl(dev_priv, (reg), true)
#define I915_WRITE(reg, val)	dev_priv->uncore.funcs.mmio_writel(dev_priv, (reg), (val), true)
#define I915_READ_NOTRACE(reg)		dev_priv->uncore.funcs.mmio_readl(dev_priv, (reg), false)
#define I915_WRITE_NOTRACE(reg, val)	dev_priv->uncore.funcs.mmio_writel(dev_priv, (reg), (val), false)

/* Be very careful with read/write 64-bit values. On 32-bit machines, they
 * will be implemented using 2 32-bit writes in an arbitrary order with
 * an arbitrary delay between them. This can cause the hardware to
 * act upon the intermediate value, possibly leading to corruption and
 * machine death. For this reason we do not support I915_WRITE64, or
 * dev_priv->uncore.funcs.mmio_writeq.
 *
 * When reading a 64-bit value as two 32-bit values, the delay may cause
 * the two reads to mismatch, e.g. a timestamp overflowing. Also note that
 * occasionally a 64-bit register does not actualy support a full readq
 * and must be read using two 32-bit reads.
 *
 * You have been warned.
 */
#define I915_READ64(reg)	dev_priv->uncore.funcs.mmio_readq(dev_priv, (reg), true)

#define I915_READ64_2x32(lower_reg, upper_reg) ({			\
	u32 upper, lower, old_upper, loop = 0;				\
	upper = I915_READ(upper_reg);					\
	do {								\
		old_upper = upper;					\
		lower = I915_READ(lower_reg);				\
		upper = I915_READ(upper_reg);				\
	} while (upper != old_upper && loop++ < 2);			\
	(u64)upper << 32 | lower; })

#define POSTING_READ(reg)	(void)I915_READ_NOTRACE(reg)
#define POSTING_READ16(reg)	(void)I915_READ16_NOTRACE(reg)

#define __raw_read(x, s) \
static inline uint##x##_t __raw_i915_read##x(const struct drm_i915_private *dev_priv, \
					     i915_reg_t reg) \
{ \
	return read##s(dev_priv->regs + i915_mmio_reg_offset(reg)); \
}

#define __raw_write(x, s) \
static inline void __raw_i915_write##x(const struct drm_i915_private *dev_priv, \
				       i915_reg_t reg, uint##x##_t val) \
{ \
	write##s(val, dev_priv->regs + i915_mmio_reg_offset(reg)); \
}
__raw_read(8, b)
__raw_read(16, w)
__raw_read(32, l)
__raw_read(64, q)

__raw_write(8, b)
__raw_write(16, w)
__raw_write(32, l)
__raw_write(64, q)

#undef __raw_read
#undef __raw_write

/* These are untraced mmio-accessors that are only valid to be used inside
 * critical sections, such as inside IRQ handlers, where forcewake is explicitly
 * controlled.
 *
 * Think twice, and think again, before using these.
 *
 * As an example, these accessors can possibly be used between:
 *
 * spin_lock_irq(&dev_priv->uncore.lock);
 * intel_uncore_forcewake_get__locked();
 *
 * and
 *
 * intel_uncore_forcewake_put__locked();
 * spin_unlock_irq(&dev_priv->uncore.lock);
 *
 *
 * Note: some registers may not need forcewake held, so
 * intel_uncore_forcewake_{get,put} can be omitted, see
 * intel_uncore_forcewake_for_reg().
 *
 * Certain architectures will die if the same cacheline is concurrently accessed
 * by different clients (e.g. on Ivybridge). Access to registers should
 * therefore generally be serialised, by either the dev_priv->uncore.lock or
 * a more localised lock guarding all access to that bank of registers.
 */
#define I915_READ_FW(reg__) __raw_i915_read32(dev_priv, (reg__))
#define I915_WRITE_FW(reg__, val__) __raw_i915_write32(dev_priv, (reg__), (val__))
#define I915_WRITE64_FW(reg__, val__) __raw_i915_write64(dev_priv, (reg__), (val__))
#define POSTING_READ_FW(reg__) (void)I915_READ_FW(reg__)

/* "Broadcast RGB" property */
#define INTEL_BROADCAST_RGB_AUTO 0
#define INTEL_BROADCAST_RGB_FULL 1
#define INTEL_BROADCAST_RGB_LIMITED 2

static inline i915_reg_t i915_vgacntrl_reg(struct drm_i915_private *dev_priv)
{
	if (IS_VALLEYVIEW(dev_priv) || IS_CHERRYVIEW(dev_priv))
		return VLV_VGACNTRL;
	else if (INTEL_GEN(dev_priv) >= 5)
		return CPU_VGACNTRL;
	else
		return VGACNTRL;
}

static inline unsigned long msecs_to_jiffies_timeout(const unsigned int m)
{
	unsigned long j = msecs_to_jiffies(m);

	return min_t(unsigned long, MAX_JIFFY_OFFSET, j + 1);
}

static inline unsigned long nsecs_to_jiffies_timeout(const u64 n)
{
	/* nsecs_to_jiffies64() does not guard against overflow */
	if (NSEC_PER_SEC % HZ &&
	    div_u64(n, NSEC_PER_SEC) >= MAX_JIFFY_OFFSET / HZ)
		return MAX_JIFFY_OFFSET;

        return min_t(u64, MAX_JIFFY_OFFSET, nsecs_to_jiffies64(n) + 1);
}

/*
 * If you need to wait X milliseconds between events A and B, but event B
 * doesn't happen exactly after event A, you record the timestamp (jiffies) of
 * when event A happened, then just before event B you call this function and
 * pass the timestamp as the first argument, and X as the second argument.
 */
#ifdef __linux__
static inline void
wait_remaining_ms_from_jiffies(unsigned long timestamp_jiffies, int to_wait_ms)
{
	unsigned long target_jiffies, tmp_jiffies, remaining_jiffies;

	/*
	 * Don't re-read the value of "jiffies" every time since it may change
	 * behind our back and break the math.
	 */
	tmp_jiffies = jiffies;
	target_jiffies = timestamp_jiffies +
			 msecs_to_jiffies_timeout(to_wait_ms);

	if (time_after(target_jiffies, tmp_jiffies)) {
		remaining_jiffies = target_jiffies - tmp_jiffies;
		while (remaining_jiffies)
			remaining_jiffies =
			    schedule_timeout_uninterruptible(remaining_jiffies);
	}
}
#else
static inline void
wait_remaining_ms_from_jiffies(unsigned long timestamp_jiffies, int to_wait_ms)
{
	unsigned long target_jiffies, tmp_jiffies, remaining_jiffies;

	if (cold) {
		delay(to_wait_ms * 1000);
		return;
	}

	/*
	 * Don't re-read the value of "jiffies" every time since it may change
	 * behind our back and break the math.
	 */
	tmp_jiffies = jiffies;
	target_jiffies = timestamp_jiffies +
			 msecs_to_jiffies_timeout(to_wait_ms);

	while (time_after(target_jiffies, tmp_jiffies)) {
		remaining_jiffies = target_jiffies - tmp_jiffies;
		tsleep(&tmp_jiffies, PWAIT, "wrmfj", remaining_jiffies);
		tmp_jiffies = jiffies;
	}
}
#endif

static inline bool
__i915_request_irq_complete(const struct i915_request *rq)
{
	struct intel_engine_cs *engine = rq->engine;
	u32 seqno;

	/* Note that the engine may have wrapped around the seqno, and
	 * so our request->global_seqno will be ahead of the hardware,
	 * even though it completed the request before wrapping. We catch
	 * this by kicking all the waiters before resetting the seqno
	 * in hardware, and also signal the fence.
	 */
	if (test_bit(DMA_FENCE_FLAG_SIGNALED_BIT, &rq->fence.flags))
		return true;

	/* The request was dequeued before we were awoken. We check after
	 * inspecting the hw to confirm that this was the same request
	 * that generated the HWS update. The memory barriers within
	 * the request execution are sufficient to ensure that a check
	 * after reading the value from hw matches this request.
	 */
	seqno = i915_request_global_seqno(rq);
	if (!seqno)
		return false;

	/* Before we do the heavier coherent read of the seqno,
	 * check the value (hopefully) in the CPU cacheline.
	 */
	if (__i915_request_completed(rq, seqno))
		return true;

	/* Ensure our read of the seqno is coherent so that we
	 * do not "miss an interrupt" (i.e. if this is the last
	 * request and the seqno write from the GPU is not visible
	 * by the time the interrupt fires, we will see that the
	 * request is incomplete and go back to sleep awaiting
	 * another interrupt that will never come.)
	 *
	 * Strictly, we only need to do this once after an interrupt,
	 * but it is easier and safer to do it every time the waiter
	 * is woken.
	 */
	if (engine->irq_seqno_barrier &&
	    test_and_clear_bit(ENGINE_IRQ_BREADCRUMB, &engine->irq_posted)) {
		struct intel_breadcrumbs *b = &engine->breadcrumbs;

		/* The ordering of irq_posted versus applying the barrier
		 * is crucial. The clearing of the current irq_posted must
		 * be visible before we perform the barrier operation,
		 * such that if a subsequent interrupt arrives, irq_posted
		 * is reasserted and our task rewoken (which causes us to
		 * do another __i915_request_irq_complete() immediately
		 * and reapply the barrier). Conversely, if the clear
		 * occurs after the barrier, then an interrupt that arrived
		 * whilst we waited on the barrier would not trigger a
		 * barrier on the next pass, and the read may not see the
		 * seqno update.
		 */
		engine->irq_seqno_barrier(engine);

		/* If we consume the irq, but we are no longer the bottom-half,
		 * the real bottom-half may not have serialised their own
		 * seqno check with the irq-barrier (i.e. may have inspected
		 * the seqno before we believe it coherent since they see
		 * irq_posted == false but we are still running).
		 */
		spin_lock_irq(&b->irq_lock);
#ifdef __linux__
		if (b->irq_wait && b->irq_wait->tsk != current)
#else
		if (b->irq_wait && b->irq_wait->tsk != curproc)
#endif
			/* Note that if the bottom-half is changed as we
			 * are sending the wake-up, the new bottom-half will
			 * be woken by whomever made the change. We only have
			 * to worry about when we steal the irq-posted for
			 * ourself.
			 */
			wake_up_process(b->irq_wait->tsk);
		spin_unlock_irq(&b->irq_lock);

		if (__i915_request_completed(rq, seqno))
			return true;
	}

	return false;
}

void i915_memcpy_init_early(struct drm_i915_private *dev_priv);
bool i915_memcpy_from_wc(void *dst, const void *src, unsigned long len);

/* The movntdqa instructions used for memcpy-from-wc require 16-byte alignment,
 * as well as SSE4.1 support. i915_memcpy_from_wc() will report if it cannot
 * perform the operation. To check beforehand, pass in the parameters to
 * to i915_can_memcpy_from_wc() - since we only care about the low 4 bits,
 * you only need to pass in the minor offsets, page-aligned pointers are
 * always valid.
 *
 * For just checking for SSE4.1, in the foreknowledge that the future use
 * will be correctly aligned, just use i915_has_memcpy_from_wc().
 */
#define i915_can_memcpy_from_wc(dst, src, len) \
	i915_memcpy_from_wc((void *)((unsigned long)(dst) | (unsigned long)(src) | (len)), NULL, 0)

#define i915_has_memcpy_from_wc() \
	i915_memcpy_from_wc(NULL, NULL, 0)

/* i915_mm.c */
#ifdef notyet
int remap_io_mapping(struct vm_area_struct *vma,
		     unsigned long addr, unsigned long pfn, unsigned long size,
		     struct io_mapping *iomap);
#endif

static inline int intel_hws_csb_write_index(struct drm_i915_private *i915)
{
	if (INTEL_GEN(i915) >= 10)
		return CNL_HWS_CSB_WRITE_INDEX;
	else
		return I915_HWS_CSB_WRITE_INDEX;
}

#endif
