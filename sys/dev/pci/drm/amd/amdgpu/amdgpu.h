/*
 * Copyright 2008 Advanced Micro Devices, Inc.
 * Copyright 2008 Red Hat Inc.
 * Copyright 2009 Jerome Glisse.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors: Dave Airlie
 *          Alex Deucher
 *          Jerome Glisse
 */
#ifndef __AMDGPU_H__
#define __AMDGPU_H__

#include <linux/atomic.h>
#include <linux/wait.h>
#include <linux/list.h>
#include <linux/kref.h>
#include <linux/rbtree.h>
#include <linux/hashtable.h>
#include <linux/dma-fence.h>

#include <drm/ttm/ttm_bo_api.h>
#include <drm/ttm/ttm_bo_driver.h>
#include <drm/ttm/ttm_placement.h>
#include <drm/ttm/ttm_module.h>
#include <drm/ttm/ttm_execbuf_util.h>

#include <drm/drmP.h>
#include <drm/drm_gem.h>
#include <drm/amdgpu_drm.h>
#include <drm/gpu_scheduler.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/rasops/rasops.h>

#include <kgd_kfd_interface.h>
#include "dm_pp_interface.h"
#include "kgd_pp_interface.h"

#include "amd_shared.h"
#include "amdgpu_mode.h"
#include "amdgpu_ih.h"
#include "amdgpu_irq.h"
#include "amdgpu_ucode.h"
#include "amdgpu_ttm.h"
#include "amdgpu_psp.h"
#include "amdgpu_gds.h"
#include "amdgpu_sync.h"
#include "amdgpu_ring.h"
#include "amdgpu_vm.h"
#include "amdgpu_dpm.h"
#include "amdgpu_acp.h"
#include "amdgpu_uvd.h"
#include "amdgpu_vce.h"
#include "amdgpu_vcn.h"
#include "amdgpu_mn.h"
#include "amdgpu_gmc.h"
#include "amdgpu_dm.h"
#include "amdgpu_virt.h"
#include "amdgpu_gart.h"
#include "amdgpu_debugfs.h"
#include "amdgpu_job.h"
#include "amdgpu_bo_list.h"

/*
 * Modules parameters.
 */
extern int amdgpu_modeset;
extern int amdgpu_vram_limit;
extern int amdgpu_vis_vram_limit;
extern int amdgpu_gart_size;
extern int amdgpu_gtt_size;
extern int amdgpu_moverate;
extern int amdgpu_benchmarking;
extern int amdgpu_testing;
extern int amdgpu_audio;
extern int amdgpu_disp_priority;
extern int amdgpu_hw_i2c;
extern int amdgpu_pcie_gen2;
extern int amdgpu_msi;
extern int amdgpu_lockup_timeout;
extern int amdgpu_dpm;
extern int amdgpu_fw_load_type;
extern int amdgpu_aspm;
extern int amdgpu_runtime_pm;
extern uint amdgpu_ip_block_mask;
extern int amdgpu_bapm;
extern int amdgpu_deep_color;
extern int amdgpu_vm_size;
extern int amdgpu_vm_block_size;
extern int amdgpu_vm_fragment_size;
extern int amdgpu_vm_fault_stop;
extern int amdgpu_vm_debug;
extern int amdgpu_vm_update_mode;
extern int amdgpu_dc;
extern int amdgpu_sched_jobs;
extern int amdgpu_sched_hw_submission;
extern uint amdgpu_pcie_gen_cap;
extern uint amdgpu_pcie_lane_cap;
extern uint amdgpu_cg_mask;
extern uint amdgpu_pg_mask;
extern uint amdgpu_sdma_phase_quantum;
extern char *amdgpu_disable_cu;
extern char *amdgpu_virtual_display;
extern uint amdgpu_pp_feature_mask;
extern int amdgpu_vram_page_split;
extern int amdgpu_ngg;
extern int amdgpu_prim_buf_per_se;
extern int amdgpu_pos_buf_per_se;
extern int amdgpu_cntl_sb_buf_per_se;
extern int amdgpu_param_buf_per_se;
extern int amdgpu_job_hang_limit;
extern int amdgpu_lbpw;
extern int amdgpu_compute_multipipe;
extern int amdgpu_gpu_recovery;
extern int amdgpu_emu_mode;
extern uint amdgpu_smu_memory_pool_size;

#ifdef CONFIG_DRM_AMDGPU_SI
extern int amdgpu_si_support;
#endif
#ifdef CONFIG_DRM_AMDGPU_CIK
extern int amdgpu_cik_support;
#endif

#define AMDGPU_SG_THRESHOLD			(256*1024*1024)
#define AMDGPU_DEFAULT_GTT_SIZE_MB		3072ULL /* 3GB by default */
#define AMDGPU_WAIT_IDLE_TIMEOUT_IN_MS	        3000
#define AMDGPU_MAX_USEC_TIMEOUT			100000	/* 100 ms */
#define AMDGPU_FENCE_JIFFIES_TIMEOUT		(HZ / 2)
/* AMDGPU_IB_POOL_SIZE must be a power of 2 */
#define AMDGPU_IB_POOL_SIZE			16
#define AMDGPU_DEBUGFS_MAX_COMPONENTS		32
#define AMDGPUFB_CONN_LIMIT			4
#define AMDGPU_BIOS_NUM_SCRATCH			16

/* max number of IP instances */
#define AMDGPU_MAX_SDMA_INSTANCES		2

/* hard reset data */
#define AMDGPU_ASIC_RESET_DATA                  0x39d5e86b

/* reset flags */
#define AMDGPU_RESET_GFX			(1 << 0)
#define AMDGPU_RESET_COMPUTE			(1 << 1)
#define AMDGPU_RESET_DMA			(1 << 2)
#define AMDGPU_RESET_CP				(1 << 3)
#define AMDGPU_RESET_GRBM			(1 << 4)
#define AMDGPU_RESET_DMA1			(1 << 5)
#define AMDGPU_RESET_RLC			(1 << 6)
#define AMDGPU_RESET_SEM			(1 << 7)
#define AMDGPU_RESET_IH				(1 << 8)
#define AMDGPU_RESET_VMC			(1 << 9)
#define AMDGPU_RESET_MC				(1 << 10)
#define AMDGPU_RESET_DISPLAY			(1 << 11)
#define AMDGPU_RESET_UVD			(1 << 12)
#define AMDGPU_RESET_VCE			(1 << 13)
#define AMDGPU_RESET_VCE1			(1 << 14)

/* GFX current status */
#define AMDGPU_GFX_NORMAL_MODE			0x00000000L
#define AMDGPU_GFX_SAFE_MODE			0x00000001L
#define AMDGPU_GFX_PG_DISABLED_MODE		0x00000002L
#define AMDGPU_GFX_CG_DISABLED_MODE		0x00000004L
#define AMDGPU_GFX_LBPW_DISABLED_MODE		0x00000008L

/* max cursor sizes (in pixels) */
#define CIK_CURSOR_WIDTH 128
#define CIK_CURSOR_HEIGHT 128

struct amdgpu_device;
struct amdgpu_ib;
struct amdgpu_cs_parser;
struct amdgpu_job;
struct amdgpu_irq_src;
struct amdgpu_fpriv;
struct amdgpu_bo_va_mapping;
struct amdgpu_atif;

enum amdgpu_cp_irq {
	AMDGPU_CP_IRQ_GFX_EOP = 0,
	AMDGPU_CP_IRQ_COMPUTE_MEC1_PIPE0_EOP,
	AMDGPU_CP_IRQ_COMPUTE_MEC1_PIPE1_EOP,
	AMDGPU_CP_IRQ_COMPUTE_MEC1_PIPE2_EOP,
	AMDGPU_CP_IRQ_COMPUTE_MEC1_PIPE3_EOP,
	AMDGPU_CP_IRQ_COMPUTE_MEC2_PIPE0_EOP,
	AMDGPU_CP_IRQ_COMPUTE_MEC2_PIPE1_EOP,
	AMDGPU_CP_IRQ_COMPUTE_MEC2_PIPE2_EOP,
	AMDGPU_CP_IRQ_COMPUTE_MEC2_PIPE3_EOP,

	AMDGPU_CP_IRQ_LAST
};

enum amdgpu_sdma_irq {
	AMDGPU_SDMA_IRQ_TRAP0 = 0,
	AMDGPU_SDMA_IRQ_TRAP1,

	AMDGPU_SDMA_IRQ_LAST
};

enum amdgpu_thermal_irq {
	AMDGPU_THERMAL_IRQ_LOW_TO_HIGH = 0,
	AMDGPU_THERMAL_IRQ_HIGH_TO_LOW,

	AMDGPU_THERMAL_IRQ_LAST
};

enum amdgpu_kiq_irq {
	AMDGPU_CP_KIQ_IRQ_DRIVER0 = 0,
	AMDGPU_CP_KIQ_IRQ_LAST
};

int amdgpu_device_ip_set_clockgating_state(void *dev,
					   enum amd_ip_block_type block_type,
					   enum amd_clockgating_state state);
int amdgpu_device_ip_set_powergating_state(void *dev,
					   enum amd_ip_block_type block_type,
					   enum amd_powergating_state state);
void amdgpu_device_ip_get_clockgating_state(struct amdgpu_device *adev,
					    u32 *flags);
int amdgpu_device_ip_wait_for_idle(struct amdgpu_device *adev,
				   enum amd_ip_block_type block_type);
bool amdgpu_device_ip_is_idle(struct amdgpu_device *adev,
			      enum amd_ip_block_type block_type);

#define AMDGPU_MAX_IP_NUM 16

struct amdgpu_ip_block_status {
	bool valid;
	bool sw;
	bool hw;
	bool late_initialized;
	bool hang;
};

struct amdgpu_ip_block_version {
	const enum amd_ip_block_type type;
	const u32 major;
	const u32 minor;
	const u32 rev;
	const struct amd_ip_funcs *funcs;
};

struct amdgpu_ip_block {
	struct amdgpu_ip_block_status status;
	const struct amdgpu_ip_block_version *version;
};

int amdgpu_device_ip_block_version_cmp(struct amdgpu_device *adev,
				       enum amd_ip_block_type type,
				       u32 major, u32 minor);

struct amdgpu_ip_block *
amdgpu_device_ip_get_ip_block(struct amdgpu_device *adev,
			      enum amd_ip_block_type type);

int amdgpu_device_ip_block_add(struct amdgpu_device *adev,
			       const struct amdgpu_ip_block_version *ip_block_version);

/* provided by hw blocks that can move/clear data.  e.g., gfx or sdma */
struct amdgpu_buffer_funcs {
	/* maximum bytes in a single operation */
	uint32_t	copy_max_bytes;

	/* number of dw to reserve per operation */
	unsigned	copy_num_dw;

	/* used for buffer migration */
	void (*emit_copy_buffer)(struct amdgpu_ib *ib,
				 /* src addr in bytes */
				 uint64_t src_offset,
				 /* dst addr in bytes */
				 uint64_t dst_offset,
				 /* number of byte to transfer */
				 uint32_t byte_count);

	/* maximum bytes in a single operation */
	uint32_t	fill_max_bytes;

	/* number of dw to reserve per operation */
	unsigned	fill_num_dw;

	/* used for buffer clearing */
	void (*emit_fill_buffer)(struct amdgpu_ib *ib,
				 /* value to write to memory */
				 uint32_t src_data,
				 /* dst addr in bytes */
				 uint64_t dst_offset,
				 /* number of byte to fill */
				 uint32_t byte_count);
};

/* provided by hw blocks that can write ptes, e.g., sdma */
struct amdgpu_vm_pte_funcs {
	/* number of dw to reserve per operation */
	unsigned	copy_pte_num_dw;

	/* copy pte entries from GART */
	void (*copy_pte)(struct amdgpu_ib *ib,
			 uint64_t pe, uint64_t src,
			 unsigned count);

	/* write pte one entry at a time with addr mapping */
	void (*write_pte)(struct amdgpu_ib *ib, uint64_t pe,
			  uint64_t value, unsigned count,
			  uint32_t incr);
	/* for linear pte/pde updates without addr mapping */
	void (*set_pte_pde)(struct amdgpu_ib *ib,
			    uint64_t pe,
			    uint64_t addr, unsigned count,
			    uint32_t incr, uint64_t flags);
};

/* provided by the ih block */
struct amdgpu_ih_funcs {
	/* ring read/write ptr handling, called from interrupt context */
	u32 (*get_wptr)(struct amdgpu_device *adev);
	bool (*prescreen_iv)(struct amdgpu_device *adev);
	void (*decode_iv)(struct amdgpu_device *adev,
			  struct amdgpu_iv_entry *entry);
	void (*set_rptr)(struct amdgpu_device *adev);
};

/*
 * BIOS.
 */
bool amdgpu_get_bios(struct amdgpu_device *adev);
bool amdgpu_read_bios(struct amdgpu_device *adev);

/*
 * Clocks
 */

#define AMDGPU_MAX_PPLL 3

struct amdgpu_clock {
	struct amdgpu_pll ppll[AMDGPU_MAX_PPLL];
	struct amdgpu_pll spll;
	struct amdgpu_pll mpll;
	/* 10 Khz units */
	uint32_t default_mclk;
	uint32_t default_sclk;
	uint32_t default_dispclk;
	uint32_t current_dispclk;
	uint32_t dp_extclk;
	uint32_t max_pixel_clock;
};

/*
 * GEM.
 */

#define AMDGPU_GEM_DOMAIN_MAX		0x3
#define gem_to_amdgpu_bo(gobj) container_of((gobj), struct amdgpu_bo, gem_base)

void amdgpu_gem_object_free(struct drm_gem_object *obj);
int amdgpu_gem_object_open(struct drm_gem_object *obj,
				struct drm_file *file_priv);
void amdgpu_gem_object_close(struct drm_gem_object *obj,
				struct drm_file *file_priv);
unsigned long amdgpu_gem_timeout(uint64_t timeout_ns);
struct sg_table *amdgpu_gem_prime_get_sg_table(struct drm_gem_object *obj);
struct drm_gem_object *
amdgpu_gem_prime_import_sg_table(struct drm_device *dev,
				 struct dma_buf_attachment *attach,
				 struct sg_table *sg);
struct dma_buf *amdgpu_gem_prime_export(struct drm_device *dev,
					struct drm_gem_object *gobj,
					int flags);
struct drm_gem_object *amdgpu_gem_prime_import(struct drm_device *dev,
					    struct dma_buf *dma_buf);
struct reservation_object *amdgpu_gem_prime_res_obj(struct drm_gem_object *);
void *amdgpu_gem_prime_vmap(struct drm_gem_object *obj);
void amdgpu_gem_prime_vunmap(struct drm_gem_object *obj, void *vaddr);
#ifdef notyet
int amdgpu_gem_prime_mmap(struct drm_gem_object *obj, struct vm_area_struct *vma);
#endif

/* sub-allocation manager, it has to be protected by another lock.
 * By conception this is an helper for other part of the driver
 * like the indirect buffer or semaphore, which both have their
 * locking.
 *
 * Principe is simple, we keep a list of sub allocation in offset
 * order (first entry has offset == 0, last entry has the highest
 * offset).
 *
 * When allocating new object we first check if there is room at
 * the end total_size - (last_object_offset + last_object_size) >=
 * alloc_size. If so we allocate new object there.
 *
 * When there is not enough room at the end, we start waiting for
 * each sub object until we reach object_offset+object_size >=
 * alloc_size, this object then become the sub object we return.
 *
 * Alignment can't be bigger than page size.
 *
 * Hole are not considered for allocation to keep things simple.
 * Assumption is that there won't be hole (all object on same
 * alignment).
 */

#define AMDGPU_SA_NUM_FENCE_LISTS	32

struct amdgpu_sa_manager {
	wait_queue_head_t	wq;
	struct amdgpu_bo	*bo;
	struct list_head	*hole;
	struct list_head	flist[AMDGPU_SA_NUM_FENCE_LISTS];
	struct list_head	olist;
	unsigned		size;
	uint64_t		gpu_addr;
	void			*cpu_ptr;
	uint32_t		domain;
	uint32_t		align;
};

/* sub-allocation buffer */
struct amdgpu_sa_bo {
	struct list_head		olist;
	struct list_head		flist;
	struct amdgpu_sa_manager	*manager;
	unsigned			soffset;
	unsigned			eoffset;
	struct dma_fence	        *fence;
};

/*
 * GEM objects.
 */
void amdgpu_gem_force_release(struct amdgpu_device *adev);
int amdgpu_gem_object_create(struct amdgpu_device *adev, unsigned long size,
			     int alignment, u32 initial_domain,
			     u64 flags, enum ttm_bo_type type,
			     struct reservation_object *resv,
			     struct drm_gem_object **obj);

int amdgpu_mode_dumb_create(struct drm_file *file_priv,
			    struct drm_device *dev,
			    struct drm_mode_create_dumb *args);
int amdgpu_mode_dumb_mmap(struct drm_file *filp,
			  struct drm_device *dev,
			  uint32_t handle, uint64_t *offset_p);
int amdgpu_fence_slab_init(void);
void amdgpu_fence_slab_fini(void);

/*
 * GPU doorbell structures, functions & helpers
 */
typedef enum _AMDGPU_DOORBELL_ASSIGNMENT
{
	AMDGPU_DOORBELL_KIQ                     = 0x000,
	AMDGPU_DOORBELL_HIQ                     = 0x001,
	AMDGPU_DOORBELL_DIQ                     = 0x002,
	AMDGPU_DOORBELL_MEC_RING0               = 0x010,
	AMDGPU_DOORBELL_MEC_RING1               = 0x011,
	AMDGPU_DOORBELL_MEC_RING2               = 0x012,
	AMDGPU_DOORBELL_MEC_RING3               = 0x013,
	AMDGPU_DOORBELL_MEC_RING4               = 0x014,
	AMDGPU_DOORBELL_MEC_RING5               = 0x015,
	AMDGPU_DOORBELL_MEC_RING6               = 0x016,
	AMDGPU_DOORBELL_MEC_RING7               = 0x017,
	AMDGPU_DOORBELL_GFX_RING0               = 0x020,
	AMDGPU_DOORBELL_sDMA_ENGINE0            = 0x1E0,
	AMDGPU_DOORBELL_sDMA_ENGINE1            = 0x1E1,
	AMDGPU_DOORBELL_IH                      = 0x1E8,
	AMDGPU_DOORBELL_MAX_ASSIGNMENT          = 0x3FF,
	AMDGPU_DOORBELL_INVALID                 = 0xFFFF
} AMDGPU_DOORBELL_ASSIGNMENT;

struct amdgpu_doorbell {
	/* doorbell mmio */
	resource_size_t		base;
	resource_size_t		size;
#ifdef __linux__
	u32 __iomem		*ptr;
#endif
	bus_space_tag_t		bst;
	bus_space_handle_t	bsh;
	u32			num_doorbells;	/* Number of doorbells actually reserved for amdgpu. */
};

/*
 * 64bit doorbell, offset are in QWORD, occupy 2KB doorbell space
 */
typedef enum _AMDGPU_DOORBELL64_ASSIGNMENT
{
	/*
	 * All compute related doorbells: kiq, hiq, diq, traditional compute queue, user queue, should locate in
	 * a continues range so that programming CP_MEC_DOORBELL_RANGE_LOWER/UPPER can cover this range.
	 *  Compute related doorbells are allocated from 0x00 to 0x8a
	 */


	/* kernel scheduling */
	AMDGPU_DOORBELL64_KIQ                     = 0x00,

	/* HSA interface queue and debug queue */
	AMDGPU_DOORBELL64_HIQ                     = 0x01,
	AMDGPU_DOORBELL64_DIQ                     = 0x02,

	/* Compute engines */
	AMDGPU_DOORBELL64_MEC_RING0               = 0x03,
	AMDGPU_DOORBELL64_MEC_RING1               = 0x04,
	AMDGPU_DOORBELL64_MEC_RING2               = 0x05,
	AMDGPU_DOORBELL64_MEC_RING3               = 0x06,
	AMDGPU_DOORBELL64_MEC_RING4               = 0x07,
	AMDGPU_DOORBELL64_MEC_RING5               = 0x08,
	AMDGPU_DOORBELL64_MEC_RING6               = 0x09,
	AMDGPU_DOORBELL64_MEC_RING7               = 0x0a,

	/* User queue doorbell range (128 doorbells) */
	AMDGPU_DOORBELL64_USERQUEUE_START         = 0x0b,
	AMDGPU_DOORBELL64_USERQUEUE_END           = 0x8a,

	/* Graphics engine */
	AMDGPU_DOORBELL64_GFX_RING0               = 0x8b,

	/*
	 * Other graphics doorbells can be allocated here: from 0x8c to 0xef
	 * Graphics voltage island aperture 1
	 * default non-graphics QWORD index is 0xF0 - 0xFF inclusive
	 */

	/* sDMA engines */
	AMDGPU_DOORBELL64_sDMA_ENGINE0            = 0xF0,
	AMDGPU_DOORBELL64_sDMA_HI_PRI_ENGINE0     = 0xF1,
	AMDGPU_DOORBELL64_sDMA_ENGINE1            = 0xF2,
	AMDGPU_DOORBELL64_sDMA_HI_PRI_ENGINE1     = 0xF3,

	/* Interrupt handler */
	AMDGPU_DOORBELL64_IH                      = 0xF4,  /* For legacy interrupt ring buffer */
	AMDGPU_DOORBELL64_IH_RING1                = 0xF5,  /* For page migration request log */
	AMDGPU_DOORBELL64_IH_RING2                = 0xF6,  /* For page migration translation/invalidation log */

	/* VCN engine use 32 bits doorbell  */
	AMDGPU_DOORBELL64_VCN0_1                  = 0xF8, /* lower 32 bits for VNC0 and upper 32 bits for VNC1 */
	AMDGPU_DOORBELL64_VCN2_3                  = 0xF9,
	AMDGPU_DOORBELL64_VCN4_5                  = 0xFA,
	AMDGPU_DOORBELL64_VCN6_7                  = 0xFB,

	/* overlap the doorbell assignment with VCN as they are  mutually exclusive
	 * VCE engine's doorbell is 32 bit and two VCE ring share one QWORD
	 */
	AMDGPU_DOORBELL64_UVD_RING0_1             = 0xF8,
	AMDGPU_DOORBELL64_UVD_RING2_3             = 0xF9,
	AMDGPU_DOORBELL64_UVD_RING4_5             = 0xFA,
	AMDGPU_DOORBELL64_UVD_RING6_7             = 0xFB,

	AMDGPU_DOORBELL64_VCE_RING0_1             = 0xFC,
	AMDGPU_DOORBELL64_VCE_RING2_3             = 0xFD,
	AMDGPU_DOORBELL64_VCE_RING4_5             = 0xFE,
	AMDGPU_DOORBELL64_VCE_RING6_7             = 0xFF,

	AMDGPU_DOORBELL64_MAX_ASSIGNMENT          = 0xFF,
	AMDGPU_DOORBELL64_INVALID                 = 0xFFFF
} AMDGPU_DOORBELL64_ASSIGNMENT;

/*
 * IRQS.
 */

struct amdgpu_flip_work {
	struct delayed_work		flip_work;
	struct work_struct		unpin_work;
	struct amdgpu_device		*adev;
	int				crtc_id;
	u32				target_vblank;
	uint64_t			base;
	struct drm_pending_vblank_event *event;
	struct amdgpu_bo		*old_abo;
	struct dma_fence		*excl;
	unsigned			shared_count;
	struct dma_fence		**shared;
	struct dma_fence_cb		cb;
	bool				async;
};


/*
 * CP & rings.
 */

struct amdgpu_ib {
	struct amdgpu_sa_bo		*sa_bo;
	uint32_t			length_dw;
	uint64_t			gpu_addr;
	uint32_t			*ptr;
	uint32_t			flags;
};

extern const struct drm_sched_backend_ops amdgpu_sched_ops;

/*
 * Queue manager
 */
struct amdgpu_queue_mapper {
	int 		hw_ip;
	struct rwlock	lock;
	/* protected by lock */
	struct amdgpu_ring *queue_map[AMDGPU_MAX_RINGS];
};

struct amdgpu_queue_mgr {
	struct amdgpu_queue_mapper mapper[AMDGPU_MAX_IP_NUM];
};

int amdgpu_queue_mgr_init(struct amdgpu_device *adev,
			  struct amdgpu_queue_mgr *mgr);
int amdgpu_queue_mgr_fini(struct amdgpu_device *adev,
			  struct amdgpu_queue_mgr *mgr);
int amdgpu_queue_mgr_map(struct amdgpu_device *adev,
			 struct amdgpu_queue_mgr *mgr,
			 u32 hw_ip, u32 instance, u32 ring,
			 struct amdgpu_ring **out_ring);

/*
 * context related structures
 */

struct amdgpu_ctx_ring {
	uint64_t		sequence;
	struct dma_fence	**fences;
	struct drm_sched_entity	entity;
};

struct amdgpu_ctx {
	struct kref		refcount;
	struct amdgpu_device    *adev;
	struct amdgpu_queue_mgr queue_mgr;
	unsigned		reset_counter;
	unsigned        reset_counter_query;
	uint32_t		vram_lost_counter;
	spinlock_t		ring_lock;
	struct dma_fence	**fences;
	struct amdgpu_ctx_ring	rings[AMDGPU_MAX_RINGS];
	bool			preamble_presented;
	enum drm_sched_priority init_priority;
	enum drm_sched_priority override_priority;
	struct rwlock            lock;
	atomic_t	guilty;
};

struct amdgpu_ctx_mgr {
	struct amdgpu_device	*adev;
	struct rwlock		lock;
	/* protected by lock */
	struct idr		ctx_handles;
};

struct amdgpu_ctx *amdgpu_ctx_get(struct amdgpu_fpriv *fpriv, uint32_t id);
int amdgpu_ctx_put(struct amdgpu_ctx *ctx);

int amdgpu_ctx_add_fence(struct amdgpu_ctx *ctx, struct amdgpu_ring *ring,
			      struct dma_fence *fence, uint64_t *seq);
struct dma_fence *amdgpu_ctx_get_fence(struct amdgpu_ctx *ctx,
				   struct amdgpu_ring *ring, uint64_t seq);
void amdgpu_ctx_priority_override(struct amdgpu_ctx *ctx,
				  enum drm_sched_priority priority);

int amdgpu_ctx_ioctl(struct drm_device *dev, void *data,
		     struct drm_file *filp);

int amdgpu_ctx_wait_prev_fence(struct amdgpu_ctx *ctx, unsigned ring_id);

void amdgpu_ctx_mgr_init(struct amdgpu_ctx_mgr *mgr);
void amdgpu_ctx_mgr_entity_fini(struct amdgpu_ctx_mgr *mgr);
void amdgpu_ctx_mgr_entity_flush(struct amdgpu_ctx_mgr *mgr);
void amdgpu_ctx_mgr_fini(struct amdgpu_ctx_mgr *mgr);


/*
 * file private structure
 */

struct amdgpu_fpriv {
	struct amdgpu_vm	vm;
	struct amdgpu_bo_va	*prt_va;
	struct amdgpu_bo_va	*csa_va;
	struct rwlock		bo_list_lock;
	struct idr		bo_list_handles;
	struct amdgpu_ctx_mgr	ctx_mgr;
};

/*
 * GFX stuff
 */
#include "clearstate_defs.h"

struct amdgpu_rlc_funcs {
	void (*enter_safe_mode)(struct amdgpu_device *adev);
	void (*exit_safe_mode)(struct amdgpu_device *adev);
};

struct amdgpu_rlc {
	/* for power gating */
	struct amdgpu_bo	*save_restore_obj;
	uint64_t		save_restore_gpu_addr;
	volatile uint32_t	*sr_ptr;
	const u32               *reg_list;
	u32                     reg_list_size;
	/* for clear state */
	struct amdgpu_bo	*clear_state_obj;
	uint64_t		clear_state_gpu_addr;
	volatile uint32_t	*cs_ptr;
	const struct cs_section_def   *cs_data;
	u32                     clear_state_size;
	/* for cp tables */
	struct amdgpu_bo	*cp_table_obj;
	uint64_t		cp_table_gpu_addr;
	volatile uint32_t	*cp_table_ptr;
	u32                     cp_table_size;

	/* safe mode for updating CG/PG state */
	bool in_safe_mode;
	const struct amdgpu_rlc_funcs *funcs;

	/* for firmware data */
	u32 save_and_restore_offset;
	u32 clear_state_descriptor_offset;
	u32 avail_scratch_ram_locations;
	u32 reg_restore_list_size;
	u32 reg_list_format_start;
	u32 reg_list_format_separate_start;
	u32 starting_offsets_start;
	u32 reg_list_format_size_bytes;
	u32 reg_list_size_bytes;
	u32 reg_list_format_direct_reg_list_length;
	u32 save_restore_list_cntl_size_bytes;
	u32 save_restore_list_gpm_size_bytes;
	u32 save_restore_list_srm_size_bytes;

	u32 *register_list_format;
	u32 *register_restore;
	u8 *save_restore_list_cntl;
	u8 *save_restore_list_gpm;
	u8 *save_restore_list_srm;

	bool is_rlc_v2_1;
};

#define AMDGPU_MAX_COMPUTE_QUEUES KGD_MAX_QUEUES

struct amdgpu_mec {
	struct amdgpu_bo	*hpd_eop_obj;
	u64			hpd_eop_gpu_addr;
	struct amdgpu_bo	*mec_fw_obj;
	u64			mec_fw_gpu_addr;
	u32 num_mec;
	u32 num_pipe_per_mec;
	u32 num_queue_per_pipe;
	void			*mqd_backup[AMDGPU_MAX_COMPUTE_RINGS + 1];

	/* These are the resources for which amdgpu takes ownership */
	DECLARE_BITMAP(queue_bitmap, AMDGPU_MAX_COMPUTE_QUEUES);
};

struct amdgpu_kiq {
	u64			eop_gpu_addr;
	struct amdgpu_bo	*eop_obj;
	spinlock_t              ring_lock;
	struct amdgpu_ring	ring;
	struct amdgpu_irq_src	irq;
};

/*
 * GPU scratch registers structures, functions & helpers
 */
struct amdgpu_scratch {
	unsigned		num_reg;
	uint32_t                reg_base;
	uint32_t		free_mask;
};

/*
 * GFX configurations
 */
#define AMDGPU_GFX_MAX_SE 4
#define AMDGPU_GFX_MAX_SH_PER_SE 2

struct amdgpu_rb_config {
	uint32_t rb_backend_disable;
	uint32_t user_rb_backend_disable;
	uint32_t raster_config;
	uint32_t raster_config_1;
};

struct gb_addr_config {
	uint16_t pipe_interleave_size;
	uint8_t num_pipes;
	uint8_t max_compress_frags;
	uint8_t num_banks;
	uint8_t num_se;
	uint8_t num_rb_per_se;
};

struct amdgpu_gfx_config {
	unsigned max_shader_engines;
	unsigned max_tile_pipes;
	unsigned max_cu_per_sh;
	unsigned max_sh_per_se;
	unsigned max_backends_per_se;
	unsigned max_texture_channel_caches;
	unsigned max_gprs;
	unsigned max_gs_threads;
	unsigned max_hw_contexts;
	unsigned sc_prim_fifo_size_frontend;
	unsigned sc_prim_fifo_size_backend;
	unsigned sc_hiz_tile_fifo_size;
	unsigned sc_earlyz_tile_fifo_size;

	unsigned num_tile_pipes;
	unsigned backend_enable_mask;
	unsigned mem_max_burst_length_bytes;
	unsigned mem_row_size_in_kb;
	unsigned shader_engine_tile_size;
	unsigned num_gpus;
	unsigned multi_gpu_tile_size;
	unsigned mc_arb_ramcfg;
	unsigned gb_addr_config;
	unsigned num_rbs;
	unsigned gs_vgt_table_depth;
	unsigned gs_prim_buffer_depth;

	uint32_t tile_mode_array[32];
	uint32_t macrotile_mode_array[16];

	struct gb_addr_config gb_addr_config_fields;
	struct amdgpu_rb_config rb_config[AMDGPU_GFX_MAX_SE][AMDGPU_GFX_MAX_SH_PER_SE];

	/* gfx configure feature */
	uint32_t double_offchip_lds_buf;
	/* cached value of DB_DEBUG2 */
	uint32_t db_debug2;
};

struct amdgpu_cu_info {
	uint32_t simd_per_cu;
	uint32_t max_waves_per_simd;
	uint32_t wave_front_size;
	uint32_t max_scratch_slots_per_cu;
	uint32_t lds_size;

	/* total active CU number */
	uint32_t number;
	uint32_t ao_cu_mask;
	uint32_t ao_cu_bitmap[4][4];
	uint32_t bitmap[4][4];
};

struct amdgpu_gfx_funcs {
	/* get the gpu clock counter */
	uint64_t (*get_gpu_clock_counter)(struct amdgpu_device *adev);
	void (*select_se_sh)(struct amdgpu_device *adev, u32 se_num, u32 sh_num, u32 instance);
	void (*read_wave_data)(struct amdgpu_device *adev, uint32_t simd, uint32_t wave, uint32_t *dst, int *no_fields);
	void (*read_wave_vgprs)(struct amdgpu_device *adev, uint32_t simd, uint32_t wave, uint32_t thread, uint32_t start, uint32_t size, uint32_t *dst);
	void (*read_wave_sgprs)(struct amdgpu_device *adev, uint32_t simd, uint32_t wave, uint32_t start, uint32_t size, uint32_t *dst);
	void (*select_me_pipe_q)(struct amdgpu_device *adev, u32 me, u32 pipe, u32 queue);
};

struct amdgpu_ngg_buf {
	struct amdgpu_bo	*bo;
	uint64_t		gpu_addr;
	uint32_t		size;
	uint32_t		bo_size;
};

enum {
	NGG_PRIM = 0,
	NGG_POS,
	NGG_CNTL,
	NGG_PARAM,
	NGG_BUF_MAX
};

struct amdgpu_ngg {
	struct amdgpu_ngg_buf	buf[NGG_BUF_MAX];
	uint32_t		gds_reserve_addr;
	uint32_t		gds_reserve_size;
	bool			init;
};

struct sq_work {
	struct work_struct	work;
	unsigned ih_data;
};

struct amdgpu_gfx {
	struct rwlock			gpu_clock_mutex;
	struct amdgpu_gfx_config	config;
	struct amdgpu_rlc		rlc;
	struct amdgpu_mec		mec;
	struct amdgpu_kiq		kiq;
	struct amdgpu_scratch		scratch;
	const struct firmware		*me_fw;	/* ME firmware */
	uint32_t			me_fw_version;
	const struct firmware		*pfp_fw; /* PFP firmware */
	uint32_t			pfp_fw_version;
	const struct firmware		*ce_fw;	/* CE firmware */
	uint32_t			ce_fw_version;
	const struct firmware		*rlc_fw; /* RLC firmware */
	uint32_t			rlc_fw_version;
	const struct firmware		*mec_fw; /* MEC firmware */
	uint32_t			mec_fw_version;
	const struct firmware		*mec2_fw; /* MEC2 firmware */
	uint32_t			mec2_fw_version;
	uint32_t			me_feature_version;
	uint32_t			ce_feature_version;
	uint32_t			pfp_feature_version;
	uint32_t			rlc_feature_version;
	uint32_t			rlc_srlc_fw_version;
	uint32_t			rlc_srlc_feature_version;
	uint32_t			rlc_srlg_fw_version;
	uint32_t			rlc_srlg_feature_version;
	uint32_t			rlc_srls_fw_version;
	uint32_t			rlc_srls_feature_version;
	uint32_t			mec_feature_version;
	uint32_t			mec2_feature_version;
	struct amdgpu_ring		gfx_ring[AMDGPU_MAX_GFX_RINGS];
	unsigned			num_gfx_rings;
	struct amdgpu_ring		compute_ring[AMDGPU_MAX_COMPUTE_RINGS];
	unsigned			num_compute_rings;
	struct amdgpu_irq_src		eop_irq;
	struct amdgpu_irq_src		priv_reg_irq;
	struct amdgpu_irq_src		priv_inst_irq;
	struct amdgpu_irq_src		cp_ecc_error_irq;
	struct amdgpu_irq_src		sq_irq;
	struct sq_work			sq_work;

	/* gfx status */
	uint32_t			gfx_current_status;
	/* ce ram size*/
	unsigned			ce_ram_size;
	struct amdgpu_cu_info		cu_info;
	const struct amdgpu_gfx_funcs	*funcs;

	/* reset mask */
	uint32_t                        grbm_soft_reset;
	uint32_t                        srbm_soft_reset;
	/* s3/s4 mask */
	bool                            in_suspend;
	/* NGG */
	struct amdgpu_ngg		ngg;

	/* pipe reservation */
	struct rwlock			pipe_reserve_mutex;
	DECLARE_BITMAP			(pipe_reserve_bitmap, AMDGPU_MAX_COMPUTE_QUEUES);
};

int amdgpu_ib_get(struct amdgpu_device *adev, struct amdgpu_vm *vm,
		  unsigned size, struct amdgpu_ib *ib);
void amdgpu_ib_free(struct amdgpu_device *adev, struct amdgpu_ib *ib,
		    struct dma_fence *f);
int amdgpu_ib_schedule(struct amdgpu_ring *ring, unsigned num_ibs,
		       struct amdgpu_ib *ibs, struct amdgpu_job *job,
		       struct dma_fence **f);
int amdgpu_ib_pool_init(struct amdgpu_device *adev);
void amdgpu_ib_pool_fini(struct amdgpu_device *adev);
int amdgpu_ib_ring_tests(struct amdgpu_device *adev);

/*
 * CS.
 */
struct amdgpu_cs_chunk {
	uint32_t		chunk_id;
	uint32_t		length_dw;
	void			*kdata;
};

struct amdgpu_cs_parser {
	struct amdgpu_device	*adev;
	struct drm_file		*filp;
	struct amdgpu_ctx	*ctx;

	/* chunks */
	unsigned		nchunks;
	struct amdgpu_cs_chunk	*chunks;

	/* scheduler job object */
	struct amdgpu_job	*job;
	struct amdgpu_ring	*ring;

	/* buffer objects */
	struct ww_acquire_ctx		ticket;
	struct amdgpu_bo_list		*bo_list;
	struct amdgpu_mn		*mn;
	struct amdgpu_bo_list_entry	vm_pd;
	struct list_head		validated;
	struct dma_fence		*fence;
	uint64_t			bytes_moved_threshold;
	uint64_t			bytes_moved_vis_threshold;
	uint64_t			bytes_moved;
	uint64_t			bytes_moved_vis;
	struct amdgpu_bo_list_entry	*evictable;

	/* user fence */
	struct amdgpu_bo_list_entry	uf_entry;

	unsigned num_post_dep_syncobjs;
	struct drm_syncobj **post_dep_syncobjs;
};

static inline u32 amdgpu_get_ib_value(struct amdgpu_cs_parser *p,
				      uint32_t ib_idx, int idx)
{
	return p->job->ibs[ib_idx].ptr[idx];
}

static inline void amdgpu_set_ib_value(struct amdgpu_cs_parser *p,
				       uint32_t ib_idx, int idx,
				       uint32_t value)
{
	p->job->ibs[ib_idx].ptr[idx] = value;
}

/*
 * Writeback
 */
#define AMDGPU_MAX_WB 128	/* Reserve at most 128 WB slots for amdgpu-owned rings. */

struct amdgpu_wb {
	struct amdgpu_bo	*wb_obj;
	volatile uint32_t	*wb;
	uint64_t		gpu_addr;
	u32			num_wb;	/* Number of wb slots actually reserved for amdgpu. */
	unsigned long		used[DIV_ROUND_UP(AMDGPU_MAX_WB, BITS_PER_LONG)];
};

int amdgpu_device_wb_get(struct amdgpu_device *adev, u32 *wb);
void amdgpu_device_wb_free(struct amdgpu_device *adev, u32 wb);

/*
 * SDMA
 */
struct amdgpu_sdma_instance {
	/* SDMA firmware */
	const struct firmware	*fw;
	uint32_t		fw_version;
	uint32_t		feature_version;

	struct amdgpu_ring	ring;
	bool			burst_nop;
};

struct amdgpu_sdma {
	struct amdgpu_sdma_instance instance[AMDGPU_MAX_SDMA_INSTANCES];
#ifdef CONFIG_DRM_AMDGPU_SI
	//SI DMA has a difference trap irq number for the second engine
	struct amdgpu_irq_src	trap_irq_1;
#endif
	struct amdgpu_irq_src	trap_irq;
	struct amdgpu_irq_src	illegal_inst_irq;
	int			num_instances;
	uint32_t                    srbm_soft_reset;
};

/*
 * Firmware
 */
enum amdgpu_firmware_load_type {
	AMDGPU_FW_LOAD_DIRECT = 0,
	AMDGPU_FW_LOAD_SMU,
	AMDGPU_FW_LOAD_PSP,
};

struct amdgpu_firmware {
	struct amdgpu_firmware_info ucode[AMDGPU_UCODE_ID_MAXIMUM];
	enum amdgpu_firmware_load_type load_type;
	struct amdgpu_bo *fw_buf;
	unsigned int fw_size;
	unsigned int max_ucodes;
	/* firmwares are loaded by psp instead of smu from vega10 */
	const struct amdgpu_psp_funcs *funcs;
	struct amdgpu_bo *rbuf;
	struct rwlock mutex;

	/* gpu info firmware data pointer */
	const struct firmware *gpu_info_fw;

	void *fw_buf_ptr;
	uint64_t fw_buf_mc;
};

/*
 * Benchmarking
 */
void amdgpu_benchmark(struct amdgpu_device *adev, int test_number);


/*
 * Testing
 */
void amdgpu_test_moves(struct amdgpu_device *adev);


/*
 * amdgpu smumgr functions
 */
struct amdgpu_smumgr_funcs {
	int (*check_fw_load_finish)(struct amdgpu_device *adev, uint32_t fwtype);
	int (*request_smu_load_fw)(struct amdgpu_device *adev);
	int (*request_smu_specific_fw)(struct amdgpu_device *adev, uint32_t fwtype);
};

/*
 * amdgpu smumgr
 */
struct amdgpu_smumgr {
	struct amdgpu_bo *toc_buf;
	struct amdgpu_bo *smu_buf;
	/* asic priv smu data */
	void *priv;
	spinlock_t smu_lock;
	/* smumgr functions */
	const struct amdgpu_smumgr_funcs *smumgr_funcs;
	/* ucode loading complete flag */
	uint32_t fw_flags;
};

/*
 * ASIC specific register table accessible by UMD
 */
struct amdgpu_allowed_register_entry {
	uint32_t reg_offset;
	bool grbm_indexed;
};

/*
 * ASIC specific functions.
 */
struct amdgpu_asic_funcs {
	bool (*read_disabled_bios)(struct amdgpu_device *adev);
	bool (*read_bios_from_rom)(struct amdgpu_device *adev,
				   u8 *bios, u32 length_bytes);
	int (*read_register)(struct amdgpu_device *adev, u32 se_num,
			     u32 sh_num, u32 reg_offset, u32 *value);
	void (*set_vga_state)(struct amdgpu_device *adev, bool state);
	int (*reset)(struct amdgpu_device *adev);
	/* get the reference clock */
	u32 (*get_xclk)(struct amdgpu_device *adev);
	/* MM block clocks */
	int (*set_uvd_clocks)(struct amdgpu_device *adev, u32 vclk, u32 dclk);
	int (*set_vce_clocks)(struct amdgpu_device *adev, u32 evclk, u32 ecclk);
	/* static power management */
	int (*get_pcie_lanes)(struct amdgpu_device *adev);
	void (*set_pcie_lanes)(struct amdgpu_device *adev, int lanes);
	/* get config memsize register */
	u32 (*get_config_memsize)(struct amdgpu_device *adev);
	/* flush hdp write queue */
	void (*flush_hdp)(struct amdgpu_device *adev, struct amdgpu_ring *ring);
	/* invalidate hdp read cache */
	void (*invalidate_hdp)(struct amdgpu_device *adev,
			       struct amdgpu_ring *ring);
	/* check if the asic needs a full reset of if soft reset will work */
	bool (*need_full_reset)(struct amdgpu_device *adev);
};

/*
 * IOCTL.
 */
int amdgpu_gem_create_ioctl(struct drm_device *dev, void *data,
			    struct drm_file *filp);
int amdgpu_bo_list_ioctl(struct drm_device *dev, void *data,
				struct drm_file *filp);

int amdgpu_gem_info_ioctl(struct drm_device *dev, void *data,
			  struct drm_file *filp);
int amdgpu_gem_userptr_ioctl(struct drm_device *dev, void *data,
			struct drm_file *filp);
int amdgpu_gem_mmap_ioctl(struct drm_device *dev, void *data,
			  struct drm_file *filp);
int amdgpu_gem_wait_idle_ioctl(struct drm_device *dev, void *data,
			      struct drm_file *filp);
int amdgpu_gem_va_ioctl(struct drm_device *dev, void *data,
			  struct drm_file *filp);
int amdgpu_gem_op_ioctl(struct drm_device *dev, void *data,
			struct drm_file *filp);
int amdgpu_cs_ioctl(struct drm_device *dev, void *data, struct drm_file *filp);
int amdgpu_cs_fence_to_handle_ioctl(struct drm_device *dev, void *data,
				    struct drm_file *filp);
int amdgpu_cs_wait_ioctl(struct drm_device *dev, void *data, struct drm_file *filp);
int amdgpu_cs_wait_fences_ioctl(struct drm_device *dev, void *data,
				struct drm_file *filp);

int amdgpu_gem_metadata_ioctl(struct drm_device *dev, void *data,
				struct drm_file *filp);

/* VRAM scratch page for HDP bug, default vram page */
struct amdgpu_vram_scratch {
	struct amdgpu_bo		*robj;
	volatile uint32_t		*ptr;
	u64				gpu_addr;
};

/*
 * ACPI
 */
struct amdgpu_atcs_functions {
	bool get_ext_state;
	bool pcie_perf_req;
	bool pcie_dev_rdy;
	bool pcie_bus_width;
};

struct amdgpu_atcs {
	struct amdgpu_atcs_functions functions;
};

/*
 * Firmware VRAM reservation
 */
struct amdgpu_fw_vram_usage {
	u64 start_offset;
	u64 size;
	struct amdgpu_bo *reserved_bo;
	void *va;
};

/*
 * CGS
 */
struct cgs_device *amdgpu_cgs_create_device(struct amdgpu_device *adev);
void amdgpu_cgs_destroy_device(struct cgs_device *cgs_device);

/*
 * Core structure, functions and helpers.
 */
typedef uint32_t (*amdgpu_rreg_t)(struct amdgpu_device*, uint32_t);
typedef void (*amdgpu_wreg_t)(struct amdgpu_device*, uint32_t, uint32_t);

typedef uint32_t (*amdgpu_block_rreg_t)(struct amdgpu_device*, uint32_t, uint32_t);
typedef void (*amdgpu_block_wreg_t)(struct amdgpu_device*, uint32_t, uint32_t, uint32_t);


/*
 * amdgpu nbio functions
 *
 */
struct nbio_hdp_flush_reg {
	u32 ref_and_mask_cp0;
	u32 ref_and_mask_cp1;
	u32 ref_and_mask_cp2;
	u32 ref_and_mask_cp3;
	u32 ref_and_mask_cp4;
	u32 ref_and_mask_cp5;
	u32 ref_and_mask_cp6;
	u32 ref_and_mask_cp7;
	u32 ref_and_mask_cp8;
	u32 ref_and_mask_cp9;
	u32 ref_and_mask_sdma0;
	u32 ref_and_mask_sdma1;
};

struct amdgpu_nbio_funcs {
	const struct nbio_hdp_flush_reg *hdp_flush_reg;
	u32 (*get_hdp_flush_req_offset)(struct amdgpu_device *adev);
	u32 (*get_hdp_flush_done_offset)(struct amdgpu_device *adev);
	u32 (*get_pcie_index_offset)(struct amdgpu_device *adev);
	u32 (*get_pcie_data_offset)(struct amdgpu_device *adev);
	u32 (*get_rev_id)(struct amdgpu_device *adev);
	void (*mc_access_enable)(struct amdgpu_device *adev, bool enable);
	void (*hdp_flush)(struct amdgpu_device *adev, struct amdgpu_ring *ring);
	u32 (*get_memsize)(struct amdgpu_device *adev);
	void (*sdma_doorbell_range)(struct amdgpu_device *adev, int instance,
				    bool use_doorbell, int doorbell_index);
	void (*enable_doorbell_aperture)(struct amdgpu_device *adev,
					 bool enable);
	void (*enable_doorbell_selfring_aperture)(struct amdgpu_device *adev,
						  bool enable);
	void (*ih_doorbell_range)(struct amdgpu_device *adev,
				  bool use_doorbell, int doorbell_index);
	void (*update_medium_grain_clock_gating)(struct amdgpu_device *adev,
						 bool enable);
	void (*update_medium_grain_light_sleep)(struct amdgpu_device *adev,
						bool enable);
	void (*get_clockgating_state)(struct amdgpu_device *adev,
				      u32 *flags);
	void (*ih_control)(struct amdgpu_device *adev);
	void (*init_registers)(struct amdgpu_device *adev);
	void (*detect_hw_virt)(struct amdgpu_device *adev);
};

struct amdgpu_df_funcs {
	void (*init)(struct amdgpu_device *adev);
	void (*enable_broadcast_mode)(struct amdgpu_device *adev,
				      bool enable);
	u32 (*get_fb_channel_number)(struct amdgpu_device *adev);
	u32 (*get_hbm_channel_number)(struct amdgpu_device *adev);
	void (*update_medium_grain_clock_gating)(struct amdgpu_device *adev,
						 bool enable);
	void (*get_clockgating_state)(struct amdgpu_device *adev,
				      u32 *flags);
	void (*enable_ecc_force_par_wr_rmw)(struct amdgpu_device *adev,
					    bool enable);
};
/* Define the HW IP blocks will be used in driver , add more if necessary */
enum amd_hw_ip_block_type {
	GC_HWIP = 1,
	HDP_HWIP,
	SDMA0_HWIP,
	SDMA1_HWIP,
	MMHUB_HWIP,
	ATHUB_HWIP,
	NBIO_HWIP,
	MP0_HWIP,
	MP1_HWIP,
	UVD_HWIP,
	VCN_HWIP = UVD_HWIP,
	VCE_HWIP,
	DF_HWIP,
	DCE_HWIP,
	OSSSYS_HWIP,
	SMUIO_HWIP,
	PWR_HWIP,
	NBIF_HWIP,
	THM_HWIP,
	CLK_HWIP,
	MAX_HWIP
};

#define HWIP_MAX_INSTANCE	6

struct amd_powerplay {
	void *pp_handle;
	const struct amd_pm_funcs *pp_funcs;
	uint32_t pp_feature;
};

#define AMDGPU_RESET_MAGIC_NUM 64
struct amdgpu_device {
	struct device			self;
	struct device			*dev;
	struct drm_device		*ddev;
	struct pci_dev			*pdev;

#ifdef CONFIG_DRM_AMD_ACP
	struct amdgpu_acp		acp;
#endif

	pci_chipset_tag_t		pc;
	pcitag_t			pa_tag;
	pci_intr_handle_t		intrh;
	bus_space_tag_t			iot;
	bus_space_tag_t			memt;
	bus_dma_tag_t			dmat;
	void				*irqh;

	void				(*switchcb)(void *, int, int);
	void				*switchcbarg;
	void				*switchcookie;
	struct task			switchtask;
	struct rasops_info		ro;
	int				console;
	int				primary;

	struct task			burner_task;
	int				burner_fblank;

	unsigned long			fb_aper_offset;
	unsigned long			fb_aper_size;

	/* ASIC */
	enum amd_asic_type		asic_type;
	uint32_t			family;
	uint32_t			rev_id;
	uint32_t			external_rev_id;
	unsigned long			flags;
	int				usec_timeout;
	const struct amdgpu_asic_funcs	*asic_funcs;
	bool				shutdown;
	bool				need_dma32;
	bool				need_swiotlb;
	bool				accel_working;
	struct work_struct		reset_work;
	struct notifier_block		acpi_nb;
	struct amdgpu_i2c_chan		*i2c_bus[AMDGPU_MAX_I2C_BUS];
	struct amdgpu_debugfs		debugfs[AMDGPU_DEBUGFS_MAX_COMPONENTS];
	unsigned			debugfs_count;
#if defined(CONFIG_DEBUG_FS)
	struct dentry			*debugfs_regs[AMDGPU_DEBUGFS_MAX_COMPONENTS];
#endif
	struct amdgpu_atif		*atif;
	struct amdgpu_atcs		atcs;
	struct rwlock			srbm_mutex;
	/* GRBM index mutex. Protects concurrent access to GRBM index */
	struct rwlock                    grbm_idx_mutex;
	struct dev_pm_domain		vga_pm_domain;
	bool				have_disp_power_ref;

	/* BIOS */
	bool				is_atom_fw;
	uint8_t				*bios;
	uint32_t			bios_size;
	struct amdgpu_bo		*stolen_vga_memory;
	uint32_t			bios_scratch_reg_offset;
	uint32_t			bios_scratch[AMDGPU_BIOS_NUM_SCRATCH];

	/* Register/doorbell mmio */
	resource_size_t			rmmio_base;
	resource_size_t			rmmio_size;
#ifdef __linux__
	void __iomem			*rmmio;
#endif
	bus_space_tag_t			rmmio_bst;
	bus_space_handle_t		rmmio_bsh;
	/* protects concurrent MM_INDEX/DATA based register access */
	spinlock_t mmio_idx_lock;
	/* protects concurrent SMC based register access */
	spinlock_t smc_idx_lock;
	amdgpu_rreg_t			smc_rreg;
	amdgpu_wreg_t			smc_wreg;
	/* protects concurrent PCIE register access */
	spinlock_t pcie_idx_lock;
	amdgpu_rreg_t			pcie_rreg;
	amdgpu_wreg_t			pcie_wreg;
	amdgpu_rreg_t			pciep_rreg;
	amdgpu_wreg_t			pciep_wreg;
	/* protects concurrent UVD register access */
	spinlock_t uvd_ctx_idx_lock;
	amdgpu_rreg_t			uvd_ctx_rreg;
	amdgpu_wreg_t			uvd_ctx_wreg;
	/* protects concurrent DIDT register access */
	spinlock_t didt_idx_lock;
	amdgpu_rreg_t			didt_rreg;
	amdgpu_wreg_t			didt_wreg;
	/* protects concurrent gc_cac register access */
	spinlock_t gc_cac_idx_lock;
	amdgpu_rreg_t			gc_cac_rreg;
	amdgpu_wreg_t			gc_cac_wreg;
	/* protects concurrent se_cac register access */
	spinlock_t se_cac_idx_lock;
	amdgpu_rreg_t			se_cac_rreg;
	amdgpu_wreg_t			se_cac_wreg;
	/* protects concurrent ENDPOINT (audio) register access */
	spinlock_t audio_endpt_idx_lock;
	amdgpu_block_rreg_t		audio_endpt_rreg;
	amdgpu_block_wreg_t		audio_endpt_wreg;
#ifdef notyet
	void __iomem			*rio_mem;
#endif
	bus_space_tag_t			rio_mem_bst;
	bus_space_handle_t		rio_mem_bsh;
	resource_size_t			rio_mem_size;
	struct amdgpu_doorbell		doorbell;

	/* clock/pll info */
	struct amdgpu_clock            clock;

	/* MC */
	struct amdgpu_gmc		gmc;
	struct amdgpu_gart		gart;
	dma_addr_t			dummy_page_addr;
	struct amdgpu_vm_manager	vm_manager;
	struct amdgpu_vmhub             vmhub[AMDGPU_MAX_VMHUBS];

	/* memory management */
	struct amdgpu_mman		mman;
	struct amdgpu_vram_scratch	vram_scratch;
	struct amdgpu_wb		wb;
	atomic64_t			num_bytes_moved;
	atomic64_t			num_evictions;
	atomic64_t			num_vram_cpu_page_faults;
	atomic_t			gpu_reset_counter;
	atomic_t			vram_lost_counter;

	/* data for buffer migration throttling */
	struct {
		spinlock_t		lock;
		s64			last_update_us;
		s64			accum_us; /* accumulated microseconds */
		s64			accum_us_vis; /* for visible VRAM */
		u32			log2_max_MBps;
	} mm_stats;

	/* display */
	bool				enable_virtual_display;
	struct amdgpu_mode_info		mode_info;
	/* For pre-DCE11. DCE11 and later are in "struct amdgpu_device->dm" */
	struct work_struct		hotplug_work;
	struct amdgpu_irq_src		crtc_irq;
	struct amdgpu_irq_src		pageflip_irq;
	struct amdgpu_irq_src		hpd_irq;

	/* rings */
	u64				fence_context;
	unsigned			num_rings;
	struct amdgpu_ring		*rings[AMDGPU_MAX_RINGS];
	bool				ib_pool_ready;
	struct amdgpu_sa_manager	ring_tmp_bo;

	/* interrupts */
	struct amdgpu_irq		irq;

	/* powerplay */
	struct amd_powerplay		powerplay;
	bool				pp_force_state_enabled;

	/* dpm */
	struct amdgpu_pm		pm;
	u32				cg_flags;
	u32				pg_flags;

	/* amdgpu smumgr */
	struct amdgpu_smumgr smu;

	/* gfx */
	struct amdgpu_gfx		gfx;

	/* sdma */
	struct amdgpu_sdma		sdma;

	/* uvd */
	struct amdgpu_uvd		uvd;

	/* vce */
	struct amdgpu_vce		vce;

	/* vcn */
	struct amdgpu_vcn		vcn;

	/* firmwares */
	struct amdgpu_firmware		firmware;

	/* PSP */
	struct psp_context		psp;

	/* GDS */
	struct amdgpu_gds		gds;

	/* display related functionality */
	struct amdgpu_display_manager dm;

	struct amdgpu_ip_block          ip_blocks[AMDGPU_MAX_IP_NUM];
	int				num_ip_blocks;
	struct rwlock	mn_lock;
	DECLARE_HASHTABLE(mn_hash, 7);

	/* tracking pinned memory */
	atomic64_t vram_pin_size;
	atomic64_t visible_pin_size;
	atomic64_t gart_pin_size;

	/* amdkfd interface */
	struct kfd_dev          *kfd;

	/* soc15 register offset based on ip, instance and  segment */
	uint32_t 		*reg_offset[MAX_HWIP][HWIP_MAX_INSTANCE];

	const struct amdgpu_nbio_funcs	*nbio_funcs;
	const struct amdgpu_df_funcs	*df_funcs;

	/* delayed work_func for deferring clockgating during resume */
	struct delayed_work     late_init_work;

	struct amdgpu_virt	virt;
	/* firmware VRAM reservation */
	struct amdgpu_fw_vram_usage fw_vram_usage;

	/* link all shadow bo */
	struct list_head                shadow_list;
	struct rwlock                    shadow_list_lock;
	/* keep an lru list of rings by HW IP */
	struct list_head		ring_lru_list;
	spinlock_t			ring_lru_list_lock;

	/* record hw reset is performed */
	bool has_hw_reset;
	u8				reset_magic[AMDGPU_RESET_MAGIC_NUM];

	/* record last mm index being written through WREG32*/
	unsigned long last_mm_index;
	bool                            in_gpu_reset;
	struct rwlock  lock_reset;
};

static inline struct amdgpu_device *amdgpu_ttm_adev(struct ttm_bo_device *bdev)
{
	return container_of(bdev, struct amdgpu_device, mman.bdev);
}

int amdgpu_device_init(struct amdgpu_device *adev,
		       struct drm_device *ddev,
		       struct pci_dev *pdev,
		       uint32_t flags);
void amdgpu_device_fini(struct amdgpu_device *adev);
int amdgpu_gpu_wait_for_idle(struct amdgpu_device *adev);

uint32_t amdgpu_mm_rreg(struct amdgpu_device *adev, uint32_t reg,
			uint32_t acc_flags);
void amdgpu_mm_wreg(struct amdgpu_device *adev, uint32_t reg, uint32_t v,
		    uint32_t acc_flags);
void amdgpu_mm_wreg8(struct amdgpu_device *adev, uint32_t offset, uint8_t value);
uint8_t amdgpu_mm_rreg8(struct amdgpu_device *adev, uint32_t offset);

u32 amdgpu_io_rreg(struct amdgpu_device *adev, u32 reg);
void amdgpu_io_wreg(struct amdgpu_device *adev, u32 reg, u32 v);

u32 amdgpu_mm_rdoorbell(struct amdgpu_device *adev, u32 index);
void amdgpu_mm_wdoorbell(struct amdgpu_device *adev, u32 index, u32 v);
u64 amdgpu_mm_rdoorbell64(struct amdgpu_device *adev, u32 index);
void amdgpu_mm_wdoorbell64(struct amdgpu_device *adev, u32 index, u64 v);

bool amdgpu_device_asic_has_dc_support(enum amd_asic_type asic_type);
bool amdgpu_device_has_dc_support(struct amdgpu_device *adev);

int emu_soc_asic_init(struct amdgpu_device *adev);

/*
 * Registers read & write functions.
 */

#define AMDGPU_REGS_IDX       (1<<0)
#define AMDGPU_REGS_NO_KIQ    (1<<1)

#define RREG32_NO_KIQ(reg) amdgpu_mm_rreg(adev, (reg), AMDGPU_REGS_NO_KIQ)
#define WREG32_NO_KIQ(reg, v) amdgpu_mm_wreg(adev, (reg), (v), AMDGPU_REGS_NO_KIQ)

#define RREG8(reg) amdgpu_mm_rreg8(adev, (reg))
#define WREG8(reg, v) amdgpu_mm_wreg8(adev, (reg), (v))

#define RREG32(reg) amdgpu_mm_rreg(adev, (reg), 0)
#define RREG32_IDX(reg) amdgpu_mm_rreg(adev, (reg), AMDGPU_REGS_IDX)
#define DREG32(reg) printk(KERN_INFO "REGISTER: " #reg " : 0x%08X\n", amdgpu_mm_rreg(adev, (reg), 0))
#define WREG32(reg, v) amdgpu_mm_wreg(adev, (reg), (v), 0)
#define WREG32_IDX(reg, v) amdgpu_mm_wreg(adev, (reg), (v), AMDGPU_REGS_IDX)
#define REG_SET(FIELD, v) (((v) << FIELD##_SHIFT) & FIELD##_MASK)
#define REG_GET(FIELD, v) (((v) << FIELD##_SHIFT) & FIELD##_MASK)
#define RREG32_PCIE(reg) adev->pcie_rreg(adev, (reg))
#define WREG32_PCIE(reg, v) adev->pcie_wreg(adev, (reg), (v))
#define RREG32_PCIE_PORT(reg) adev->pciep_rreg(adev, (reg))
#define WREG32_PCIE_PORT(reg, v) adev->pciep_wreg(adev, (reg), (v))
#define RREG32_SMC(reg) adev->smc_rreg(adev, (reg))
#define WREG32_SMC(reg, v) adev->smc_wreg(adev, (reg), (v))
#define RREG32_UVD_CTX(reg) adev->uvd_ctx_rreg(adev, (reg))
#define WREG32_UVD_CTX(reg, v) adev->uvd_ctx_wreg(adev, (reg), (v))
#define RREG32_DIDT(reg) adev->didt_rreg(adev, (reg))
#define WREG32_DIDT(reg, v) adev->didt_wreg(adev, (reg), (v))
#define RREG32_GC_CAC(reg) adev->gc_cac_rreg(adev, (reg))
#define WREG32_GC_CAC(reg, v) adev->gc_cac_wreg(adev, (reg), (v))
#define RREG32_SE_CAC(reg) adev->se_cac_rreg(adev, (reg))
#define WREG32_SE_CAC(reg, v) adev->se_cac_wreg(adev, (reg), (v))
#define RREG32_AUDIO_ENDPT(block, reg) adev->audio_endpt_rreg(adev, (block), (reg))
#define WREG32_AUDIO_ENDPT(block, reg, v) adev->audio_endpt_wreg(adev, (block), (reg), (v))
#define WREG32_P(reg, val, mask)				\
	do {							\
		uint32_t tmp_ = RREG32(reg);			\
		tmp_ &= (mask);					\
		tmp_ |= ((val) & ~(mask));			\
		WREG32(reg, tmp_);				\
	} while (0)
#define WREG32_AND(reg, and) WREG32_P(reg, 0, and)
#define WREG32_OR(reg, or) WREG32_P(reg, or, ~(or))
#define WREG32_PLL_P(reg, val, mask)				\
	do {							\
		uint32_t tmp_ = RREG32_PLL(reg);		\
		tmp_ &= (mask);					\
		tmp_ |= ((val) & ~(mask));			\
		WREG32_PLL(reg, tmp_);				\
	} while (0)
#define DREG32_SYS(sqf, adev, reg) seq_printf((sqf), #reg " : 0x%08X\n", amdgpu_mm_rreg((adev), (reg), false))
#define RREG32_IO(reg) amdgpu_io_rreg(adev, (reg))
#define WREG32_IO(reg, v) amdgpu_io_wreg(adev, (reg), (v))

#define RDOORBELL32(index) amdgpu_mm_rdoorbell(adev, (index))
#define WDOORBELL32(index, v) amdgpu_mm_wdoorbell(adev, (index), (v))
#define RDOORBELL64(index) amdgpu_mm_rdoorbell64(adev, (index))
#define WDOORBELL64(index, v) amdgpu_mm_wdoorbell64(adev, (index), (v))

#define REG_FIELD_SHIFT(reg, field) reg##__##field##__SHIFT
#define REG_FIELD_MASK(reg, field) reg##__##field##_MASK

#define REG_SET_FIELD(orig_val, reg, field, field_val)			\
	(((orig_val) & ~REG_FIELD_MASK(reg, field)) |			\
	 (REG_FIELD_MASK(reg, field) & ((field_val) << REG_FIELD_SHIFT(reg, field))))

#define REG_GET_FIELD(value, reg, field)				\
	(((value) & REG_FIELD_MASK(reg, field)) >> REG_FIELD_SHIFT(reg, field))

#define WREG32_FIELD(reg, field, val)	\
	WREG32(mm##reg, (RREG32(mm##reg) & ~REG_FIELD_MASK(reg, field)) | (val) << REG_FIELD_SHIFT(reg, field))

#define WREG32_FIELD_OFFSET(reg, offset, field, val)	\
	WREG32(mm##reg + offset, (RREG32(mm##reg + offset) & ~REG_FIELD_MASK(reg, field)) | (val) << REG_FIELD_SHIFT(reg, field))

/*
 * BIOS helpers.
 */
#define RBIOS8(i) (adev->bios[i])
#define RBIOS16(i) (RBIOS8(i) | (RBIOS8((i)+1) << 8))
#define RBIOS32(i) ((RBIOS16(i)) | (RBIOS16((i)+2) << 16))

static inline struct amdgpu_sdma_instance *
amdgpu_get_sdma_instance(struct amdgpu_ring *ring)
{
	struct amdgpu_device *adev = ring->adev;
	int i;

	for (i = 0; i < adev->sdma.num_instances; i++)
		if (&adev->sdma.instance[i].ring == ring)
			break;

	if (i < AMDGPU_MAX_SDMA_INSTANCES)
		return &adev->sdma.instance[i];
	else
		return NULL;
}

/*
 * ASICs macro.
 */
#define amdgpu_asic_set_vga_state(adev, state) (adev)->asic_funcs->set_vga_state((adev), (state))
#define amdgpu_asic_reset(adev) (adev)->asic_funcs->reset((adev))
#define amdgpu_asic_get_xclk(adev) (adev)->asic_funcs->get_xclk((adev))
#define amdgpu_asic_set_uvd_clocks(adev, v, d) (adev)->asic_funcs->set_uvd_clocks((adev), (v), (d))
#define amdgpu_asic_set_vce_clocks(adev, ev, ec) (adev)->asic_funcs->set_vce_clocks((adev), (ev), (ec))
#define amdgpu_get_pcie_lanes(adev) (adev)->asic_funcs->get_pcie_lanes((adev))
#define amdgpu_set_pcie_lanes(adev, l) (adev)->asic_funcs->set_pcie_lanes((adev), (l))
#define amdgpu_asic_get_gpu_clock_counter(adev) (adev)->asic_funcs->get_gpu_clock_counter((adev))
#define amdgpu_asic_read_disabled_bios(adev) (adev)->asic_funcs->read_disabled_bios((adev))
#define amdgpu_asic_read_bios_from_rom(adev, b, l) (adev)->asic_funcs->read_bios_from_rom((adev), (b), (l))
#define amdgpu_asic_read_register(adev, se, sh, offset, v)((adev)->asic_funcs->read_register((adev), (se), (sh), (offset), (v)))
#define amdgpu_asic_get_config_memsize(adev) (adev)->asic_funcs->get_config_memsize((adev))
#define amdgpu_asic_flush_hdp(adev, r) (adev)->asic_funcs->flush_hdp((adev), (r))
#define amdgpu_asic_invalidate_hdp(adev, r) (adev)->asic_funcs->invalidate_hdp((adev), (r))
#define amdgpu_asic_need_full_reset(adev) (adev)->asic_funcs->need_full_reset((adev))
#define amdgpu_gmc_flush_gpu_tlb(adev, vmid) (adev)->gmc.gmc_funcs->flush_gpu_tlb((adev), (vmid))
#define amdgpu_gmc_emit_flush_gpu_tlb(r, vmid, addr) (r)->adev->gmc.gmc_funcs->emit_flush_gpu_tlb((r), (vmid), (addr))
#define amdgpu_gmc_emit_pasid_mapping(r, vmid, pasid) (r)->adev->gmc.gmc_funcs->emit_pasid_mapping((r), (vmid), (pasid))
#define amdgpu_gmc_set_pte_pde(adev, pt, idx, addr, flags) (adev)->gmc.gmc_funcs->set_pte_pde((adev), (pt), (idx), (addr), (flags))
#define amdgpu_gmc_get_vm_pde(adev, level, dst, flags) (adev)->gmc.gmc_funcs->get_vm_pde((adev), (level), (dst), (flags))
#define amdgpu_gmc_get_pte_flags(adev, flags) (adev)->gmc.gmc_funcs->get_vm_pte_flags((adev),(flags))
#define amdgpu_vm_copy_pte(adev, ib, pe, src, count) ((adev)->vm_manager.vm_pte_funcs->copy_pte((ib), (pe), (src), (count)))
#define amdgpu_vm_write_pte(adev, ib, pe, value, count, incr) ((adev)->vm_manager.vm_pte_funcs->write_pte((ib), (pe), (value), (count), (incr)))
#define amdgpu_vm_set_pte_pde(adev, ib, pe, addr, count, incr, flags) ((adev)->vm_manager.vm_pte_funcs->set_pte_pde((ib), (pe), (addr), (count), (incr), (flags)))
#define amdgpu_ring_parse_cs(r, p, ib) ((r)->funcs->parse_cs((p), (ib)))
#define amdgpu_ring_patch_cs_in_place(r, p, ib) ((r)->funcs->patch_cs_in_place((p), (ib)))
#define amdgpu_ring_test_ring(r) (r)->funcs->test_ring((r))
#define amdgpu_ring_test_ib(r, t) (r)->funcs->test_ib((r), (t))
#define amdgpu_ring_get_rptr(r) (r)->funcs->get_rptr((r))
#define amdgpu_ring_get_wptr(r) (r)->funcs->get_wptr((r))
#define amdgpu_ring_set_wptr(r) (r)->funcs->set_wptr((r))
#define amdgpu_ring_emit_ib(r, ib, vmid, c) (r)->funcs->emit_ib((r), (ib), (vmid), (c))
#define amdgpu_ring_emit_pipeline_sync(r) (r)->funcs->emit_pipeline_sync((r))
#define amdgpu_ring_emit_vm_flush(r, vmid, addr) (r)->funcs->emit_vm_flush((r), (vmid), (addr))
#define amdgpu_ring_emit_fence(r, addr, seq, flags) (r)->funcs->emit_fence((r), (addr), (seq), (flags))
#define amdgpu_ring_emit_gds_switch(r, v, db, ds, wb, ws, ab, as) (r)->funcs->emit_gds_switch((r), (v), (db), (ds), (wb), (ws), (ab), (as))
#define amdgpu_ring_emit_hdp_flush(r) (r)->funcs->emit_hdp_flush((r))
#define amdgpu_ring_emit_switch_buffer(r) (r)->funcs->emit_switch_buffer((r))
#define amdgpu_ring_emit_cntxcntl(r, d) (r)->funcs->emit_cntxcntl((r), (d))
#define amdgpu_ring_emit_rreg(r, d) (r)->funcs->emit_rreg((r), (d))
#define amdgpu_ring_emit_wreg(r, d, v) (r)->funcs->emit_wreg((r), (d), (v))
#define amdgpu_ring_emit_reg_wait(r, d, v, m) (r)->funcs->emit_reg_wait((r), (d), (v), (m))
#define amdgpu_ring_emit_reg_write_reg_wait(r, d0, d1, v, m) (r)->funcs->emit_reg_write_reg_wait((r), (d0), (d1), (v), (m))
#define amdgpu_ring_emit_tmz(r, b) (r)->funcs->emit_tmz((r), (b))
#define amdgpu_ring_pad_ib(r, ib) ((r)->funcs->pad_ib((r), (ib)))
#define amdgpu_ring_init_cond_exec(r) (r)->funcs->init_cond_exec((r))
#define amdgpu_ring_patch_cond_exec(r,o) (r)->funcs->patch_cond_exec((r),(o))
#define amdgpu_ih_get_wptr(adev) (adev)->irq.ih_funcs->get_wptr((adev))
#define amdgpu_ih_prescreen_iv(adev) (adev)->irq.ih_funcs->prescreen_iv((adev))
#define amdgpu_ih_decode_iv(adev, iv) (adev)->irq.ih_funcs->decode_iv((adev), (iv))
#define amdgpu_ih_set_rptr(adev) (adev)->irq.ih_funcs->set_rptr((adev))
#define amdgpu_display_vblank_get_counter(adev, crtc) (adev)->mode_info.funcs->vblank_get_counter((adev), (crtc))
#define amdgpu_display_backlight_set_level(adev, e, l) (adev)->mode_info.funcs->backlight_set_level((e), (l))
#define amdgpu_display_backlight_get_level(adev, e) (adev)->mode_info.funcs->backlight_get_level((e))
#define amdgpu_display_hpd_sense(adev, h) (adev)->mode_info.funcs->hpd_sense((adev), (h))
#define amdgpu_display_hpd_set_polarity(adev, h) (adev)->mode_info.funcs->hpd_set_polarity((adev), (h))
#define amdgpu_display_hpd_get_gpio_reg(adev) (adev)->mode_info.funcs->hpd_get_gpio_reg((adev))
#define amdgpu_display_bandwidth_update(adev) (adev)->mode_info.funcs->bandwidth_update((adev))
#define amdgpu_display_page_flip(adev, crtc, base, async) (adev)->mode_info.funcs->page_flip((adev), (crtc), (base), (async))
#define amdgpu_display_page_flip_get_scanoutpos(adev, crtc, vbl, pos) (adev)->mode_info.funcs->page_flip_get_scanoutpos((adev), (crtc), (vbl), (pos))
#define amdgpu_display_add_encoder(adev, e, s, c) (adev)->mode_info.funcs->add_encoder((adev), (e), (s), (c))
#define amdgpu_display_add_connector(adev, ci, sd, ct, ib, coi, h, r) (adev)->mode_info.funcs->add_connector((adev), (ci), (sd), (ct), (ib), (coi), (h), (r))
#define amdgpu_emit_copy_buffer(adev, ib, s, d, b) (adev)->mman.buffer_funcs->emit_copy_buffer((ib),  (s), (d), (b))
#define amdgpu_emit_fill_buffer(adev, ib, s, d, b) (adev)->mman.buffer_funcs->emit_fill_buffer((ib), (s), (d), (b))
#define amdgpu_gfx_get_gpu_clock_counter(adev) (adev)->gfx.funcs->get_gpu_clock_counter((adev))
#define amdgpu_gfx_select_se_sh(adev, se, sh, instance) (adev)->gfx.funcs->select_se_sh((adev), (se), (sh), (instance))
#define amdgpu_gds_switch(adev, r, v, d, w, a) (adev)->gds.funcs->patch_gds_switch((r), (v), (d), (w), (a))
#define amdgpu_psp_check_fw_loading_status(adev, i) (adev)->firmware.funcs->check_fw_loading_status((adev), (i))
#define amdgpu_gfx_select_me_pipe_q(adev, me, pipe, q) (adev)->gfx.funcs->select_me_pipe_q((adev), (me), (pipe), (q))

/* Common functions */
int amdgpu_device_gpu_recover(struct amdgpu_device *adev,
			      struct amdgpu_job* job, bool force);
void amdgpu_device_pci_config_reset(struct amdgpu_device *adev);
bool amdgpu_device_need_post(struct amdgpu_device *adev);
void amdgpu_display_update_priority(struct amdgpu_device *adev);

void amdgpu_cs_report_moved_bytes(struct amdgpu_device *adev, u64 num_bytes,
				  u64 num_vis_bytes);
void amdgpu_device_vram_location(struct amdgpu_device *adev,
				 struct amdgpu_gmc *mc, u64 base);
void amdgpu_device_gart_location(struct amdgpu_device *adev,
				 struct amdgpu_gmc *mc);
int amdgpu_device_resize_fb_bar(struct amdgpu_device *adev);
void amdgpu_device_program_register_sequence(struct amdgpu_device *adev,
					     const u32 *registers,
					     const u32 array_size);

bool amdgpu_device_is_px(struct drm_device *dev);
/* atpx handler */
#if defined(CONFIG_VGA_SWITCHEROO)
void amdgpu_register_atpx_handler(void);
void amdgpu_unregister_atpx_handler(void);
bool amdgpu_has_atpx_dgpu_power_cntl(void);
bool amdgpu_is_atpx_hybrid(void);
bool amdgpu_atpx_dgpu_req_power_for_displays(void);
bool amdgpu_has_atpx(void);
#else
static inline void amdgpu_register_atpx_handler(void) {}
static inline void amdgpu_unregister_atpx_handler(void) {}
static inline bool amdgpu_has_atpx_dgpu_power_cntl(void) { return false; }
static inline bool amdgpu_is_atpx_hybrid(void) { return false; }
static inline bool amdgpu_atpx_dgpu_req_power_for_displays(void) { return false; }
static inline bool amdgpu_has_atpx(void) { return false; }
#endif

#if defined(CONFIG_VGA_SWITCHEROO) && defined(CONFIG_ACPI)
void *amdgpu_atpx_get_dhandle(void);
#else
static inline void *amdgpu_atpx_get_dhandle(void) { return NULL; }
#endif

/*
 * KMS
 */
extern const struct drm_ioctl_desc amdgpu_ioctls_kms[];
extern const int amdgpu_max_kms_ioctl;

int amdgpu_driver_load_kms(struct drm_device *dev, unsigned long flags);
void amdgpu_driver_unload_kms(struct drm_device *dev);
void amdgpu_driver_lastclose_kms(struct drm_device *dev);
int amdgpu_driver_open_kms(struct drm_device *dev, struct drm_file *file_priv);
void amdgpu_driver_postclose_kms(struct drm_device *dev,
				 struct drm_file *file_priv);
int amdgpu_device_ip_suspend(struct amdgpu_device *adev);
int amdgpu_device_suspend(struct drm_device *dev, bool suspend, bool fbcon);
int amdgpu_device_resume(struct drm_device *dev, bool resume, bool fbcon);
u32 amdgpu_get_vblank_counter_kms(struct drm_device *dev, unsigned int pipe);
int amdgpu_enable_vblank_kms(struct drm_device *dev, unsigned int pipe);
void amdgpu_disable_vblank_kms(struct drm_device *dev, unsigned int pipe);
long amdgpu_kms_compat_ioctl(struct file *filp, unsigned int cmd,
			     unsigned long arg);

/*
 * functions used by amdgpu_encoder.c
 */
struct amdgpu_afmt_acr {
	u32 clock;

	int n_32khz;
	int cts_32khz;

	int n_44_1khz;
	int cts_44_1khz;

	int n_48khz;
	int cts_48khz;

};

struct amdgpu_afmt_acr amdgpu_afmt_acr(uint32_t clock);

/* amdgpu_acpi.c */
#if defined(CONFIG_ACPI)
int amdgpu_acpi_init(struct amdgpu_device *adev);
void amdgpu_acpi_fini(struct amdgpu_device *adev);
bool amdgpu_acpi_is_pcie_performance_request_supported(struct amdgpu_device *adev);
int amdgpu_acpi_pcie_performance_request(struct amdgpu_device *adev,
						u8 perf_req, bool advertise);
int amdgpu_acpi_pcie_notify_device_ready(struct amdgpu_device *adev);
#else
static inline int amdgpu_acpi_init(struct amdgpu_device *adev) { return 0; }
static inline void amdgpu_acpi_fini(struct amdgpu_device *adev) { }
#endif

int amdgpu_cs_find_mapping(struct amdgpu_cs_parser *parser,
			   uint64_t addr, struct amdgpu_bo **bo,
			   struct amdgpu_bo_va_mapping **mapping);

#if defined(CONFIG_DRM_AMD_DC)
int amdgpu_dm_display_resume(struct amdgpu_device *adev );
#else
static inline int amdgpu_dm_display_resume(struct amdgpu_device *adev) { return 0; }
#endif

#include "amdgpu_object.h"
#endif
