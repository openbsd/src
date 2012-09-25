/* $OpenBSD: i915_drv.h,v 1.73 2012/09/25 10:19:46 jsg Exp $ */
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

#include "i915_reg.h"

/* General customization:
 */

#define DRIVER_AUTHOR		"Tungsten Graphics, Inc."

#define DRIVER_NAME		"i915"
#define DRIVER_DESC		"Intel Graphics"
#define DRIVER_DATE		"20080730"

enum pipe {
	PIPE_A = 0,
	PIPE_B,
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

struct inteldrm_ring {
	struct drm_obj		*ring_obj;
	bus_space_handle_t	 bsh;
	bus_size_t		 size;
	u_int32_t		 head;
	u_int32_t		 space;
	u_int32_t		 tail;
	u_int32_t		 woffset;
};

#define I915_FENCE_REG_NONE -1

struct inteldrm_fence {
	TAILQ_ENTRY(inteldrm_fence)	 list;
	struct drm_obj			*obj;
	u_int32_t			 last_rendering_seqno;
};

/*
 * lock ordering:
 * exec lock,
 * request lock
 * list lock.
 *
 * XXX fence lock ,object lock
 */
struct inteldrm_softc {
	struct device		 dev;
	struct device		*drmdev;
	bus_dma_tag_t		 agpdmat; /* tag from intagp for GEM */
	bus_dma_tag_t		 dmat;
	bus_space_tag_t		 bst;
	struct agp_map		*agph;

	u_long			 flags;
	u_int16_t		 pci_device;
	int			 gen;

	pci_chipset_tag_t	 pc;
	pcitag_t		 tag;
	pci_intr_handle_t	 ih;
	void			*irqh;

	struct vga_pci_bar	*regs;

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
	struct inteldrm_ring	 ring;
	struct workq		*workq;
	struct vm_page		*pgs;
	union hws {
		struct drm_obj		*obj;
		struct drm_dmamem	*dmamem;
	}	hws;
#define				 hws_obj	hws.obj
#define				 hws_dmamem	hws.dmamem
	void			*hw_status_page;
	size_t			 max_gem_obj_size; /* XXX */

	/* Protects user_irq_refcount and irq_mask reg */
	struct mutex		 user_irq_lock;
	/* Refcount for user irq, only enabled when needed */
	int			 user_irq_refcount;
	/* Cached value of IMR to avoid reads in updating the bitfield */
	u_int32_t		 irq_mask_reg;
	u_int32_t		 pipestat[2];
	/* these two  ironlake only, we should union this with pipestat XXX */
	u_int32_t		 gt_irq_mask_reg;
	u_int32_t		 pch_irq_mask_reg;

	struct mutex		 fence_lock;
	struct inteldrm_fence	 fence_regs[16]; /* 965 */
	int			 fence_reg_start; /* 4 by default */
	int			 num_fence_regs; /* 8 pre-965, 16 post */

#define	INTELDRM_QUIET		0x01 /* suspend close, get off the hardware */
#define	INTELDRM_WEDGED		0x02 /* chipset hung pending reset */
#define	INTELDRM_SUSPENDED	0x04 /* in vt switch, no commands */
	int			 sc_flags; /* quiet, suspended, hung */
	/* number of ioctls + faults in flight */
	int			 entries;

	/* protects inactive, flushing, active and exec locks */
	struct mutex		 list_lock;

	/* protects access to request_list */
	struct mutex		 request_lock;

	/* Register state */
	u8 saveLBB;
	u32 saveDSPACNTR;
	u32 saveDSPBCNTR;
	u32 saveDSPARB;
	u32 saveHWS;
	u32 savePIPEACONF;
	u32 savePIPEBCONF;
	u32 savePIPEASRC;
	u32 savePIPEBSRC;
	u32 saveFPA0;
	u32 saveFPA1;
	u32 saveDPLL_A;
	u32 saveDPLL_A_MD;
	u32 saveHTOTAL_A;
	u32 saveHBLANK_A;
	u32 saveHSYNC_A;
	u32 saveVTOTAL_A;
	u32 saveVBLANK_A;
	u32 saveVSYNC_A;
	u32 saveBCLRPAT_A;
	u32 saveTRANSACONF;
	u32 saveTRANS_HTOTAL_A;
	u32 saveTRANS_HBLANK_A;
	u32 saveTRANS_HSYNC_A;
	u32 saveTRANS_VTOTAL_A;
	u32 saveTRANS_VBLANK_A;
	u32 saveTRANS_VSYNC_A;
	u32 savePIPEASTAT;
	u32 saveDSPASTRIDE;
	u32 saveDSPASIZE;
	u32 saveDSPAPOS;
	u32 saveDSPAADDR;
	u32 saveDSPASURF;
	u32 saveDSPATILEOFF;
	u32 savePFIT_PGM_RATIOS;
	u32 saveBLC_HIST_CTL;
	u32 saveBLC_PWM_CTL;
	u32 saveBLC_PWM_CTL2;
	u32 saveBLC_CPU_PWM_CTL;
	u32 saveBLC_CPU_PWM_CTL2;
	u32 saveFPB0;
	u32 saveFPB1;
	u32 saveDPLL_B;
	u32 saveDPLL_B_MD;
	u32 saveHTOTAL_B;
	u32 saveHBLANK_B;
	u32 saveHSYNC_B;
	u32 saveVTOTAL_B;
	u32 saveVBLANK_B;
	u32 saveVSYNC_B;
	u32 saveBCLRPAT_B;
	u32 saveTRANSBCONF;
	u32 saveTRANS_HTOTAL_B;
	u32 saveTRANS_HBLANK_B;
	u32 saveTRANS_HSYNC_B;
	u32 saveTRANS_VTOTAL_B;
	u32 saveTRANS_VBLANK_B;
	u32 saveTRANS_VSYNC_B;
	u32 savePIPEBSTAT;
	u32 saveDSPBSTRIDE;
	u32 saveDSPBSIZE;
	u32 saveDSPBPOS;
	u32 saveDSPBADDR;
	u32 saveDSPBSURF;
	u32 saveDSPBTILEOFF;
	u32 saveVGA0;
	u32 saveVGA1;
	u32 saveVGA_PD;
	u32 saveVGACNTRL;
	u32 saveADPA;
	u32 saveLVDS;
	u32 savePP_ON_DELAYS;
	u32 savePP_OFF_DELAYS;
	u32 saveDVOA;
	u32 saveDVOB;
	u32 saveDVOC;
	u32 savePP_ON;
	u32 savePP_OFF;
	u32 savePP_CONTROL;
	u32 savePP_DIVISOR;
	u32 savePFIT_CONTROL;
	u32 save_palette_a[256];
	u32 save_palette_b[256];
	u32 saveDPFC_CB_BASE;
	u32 saveFBC_CFB_BASE;
	u32 saveFBC_LL_BASE;
	u32 saveFBC_CONTROL;
	u32 saveFBC_CONTROL2;
	u32 saveIER;
	u32 saveIIR;
	u32 saveIMR;
	u32 saveDEIER;
	u32 saveDEIMR;
	u32 saveGTIER;
	u32 saveGTIMR;
	u32 saveFDI_RXA_IMR;
	u32 saveFDI_RXB_IMR;
	u32 saveCACHE_MODE_0;
	u32 saveD_STATE;
	u32 saveDSPCLK_GATE_D;
	u32 saveDSPCLK_GATE;
	u32 saveRENCLK_GATE_D1;
	u32 saveRENCLK_GATE_D2;
	u32 saveRAMCLK_GATE_D;
	u32 saveDEUC;
	u32 saveMI_ARB_STATE;
	u32 saveSWF0[16];
	u32 saveSWF1[16];
	u32 saveSWF2[3];
	u8 saveMSR;
	u8 saveSR[8];
	u8 saveGR[25];
	u8 saveAR_INDEX;
	u8 saveAR[21];
	u8 saveDACMASK;
	u8 saveCR[37];
	uint64_t saveFENCE[16];
	u32 saveCURACNTR;
	u32 saveCURAPOS;
	u32 saveCURABASE;
	u32 saveCURBCNTR;
	u32 saveCURBPOS;
	u32 saveCURBBASE;
	u32 saveCURSIZE;
	u32 saveDP_B;
	u32 saveDP_C;
	u32 saveDP_D;
	u32 savePIPEA_GMCH_DATA_M;
	u32 savePIPEB_GMCH_DATA_M;
	u32 savePIPEA_GMCH_DATA_N;
	u32 savePIPEB_GMCH_DATA_N;
	u32 savePIPEA_DP_LINK_M;
	u32 savePIPEB_DP_LINK_M;
	u32 savePIPEA_DP_LINK_N;
	u32 savePIPEB_DP_LINK_N;
	u32 saveFDI_RXA_CTL;
	u32 saveFDI_TXA_CTL;
	u32 saveFDI_RXB_CTL;
	u32 saveFDI_TXB_CTL;
	u32 savePFA_CTL_1;
	u32 savePFB_CTL_1;
	u32 savePFA_WIN_SZ;
	u32 savePFB_WIN_SZ;
	u32 savePFA_WIN_POS;
	u32 savePFB_WIN_POS;
	u32 savePCH_DREF_CONTROL;
	u32 saveDISP_ARB_CTL;
	u32 savePIPEA_DATA_M1;
	u32 savePIPEA_DATA_N1;
	u32 savePIPEA_LINK_M1;
	u32 savePIPEA_LINK_N1;
	u32 savePIPEB_DATA_M1;
	u32 savePIPEB_DATA_N1;
	u32 savePIPEB_LINK_M1;
	u32 savePIPEB_LINK_N1;
	u32 saveMCHBAR_RENDER_STANDBY;
	u32 savePCH_PORT_HOTPLUG;

	struct {
		/**
		 * List of objects currently involved in rendering from the
		 * ringbuffer.
		 *
		 * Includes buffers having the contents of their GPU caches
		 * flushed, not necessarily primitives. last_rendering_seqno
		 * represents when the rendering involved will be completed.
		 *
		 * A reference is held on the buffer while on this list.
		 */
		TAILQ_HEAD(i915_gem_list, inteldrm_obj) active_list;

		/**
		 * List of objects which are not in the ringbuffer but which
		 * still have a write_domain which needs to be flushed before
		 * unbinding.
		 *
		 * last_rendering_seqno is 0 while an object is in this list
		 *
		 * A reference is held on the buffer while on this list.
		 */
		struct i915_gem_list flushing_list;

		/*
		 * list of objects currently pending a GPU write flush.
		 *
		 * All elements on this list will either be on the active
		 * or flushing list, last rendiering_seqno differentiates the
		 * two.
		 */
		struct i915_gem_list gpu_write_list;
		/**
		 * LRU list of objects which are not in the ringbuffer and
		 * are ready to unbind, but are still in the GTT.
		 *
		 * last_rendering_seqno is 0 while an object is in this list
		 *
		 * A reference is not held on the buffer while on this list,
		 * as merely being GTT-bound shouldn't prevent its being
		 * freed, and we'll pull it off the list in the free path.
		 */
		struct i915_gem_list inactive_list;

		/* Fence LRU */
		TAILQ_HEAD(i915_fence, inteldrm_fence)	fence_list;

		/**
		 * List of breadcrumbs associated with GPU requests currently
		 * outstanding.
		 */
		TAILQ_HEAD(i915_request , inteldrm_request) request_list;

		/**
		 * We leave the user IRQ off as much as possible,
		 * but this means that requests will finish and never
		 * be retired once the system goes idle. Set a timer to
		 * fire periodically while the ring is running. When it
		 * fires, go retire requests in a workq.
		 */
		struct timeout retire_timer;
		struct timeout hang_timer;
		/* for hangcheck */
		int		hang_cnt;
		u_int32_t	last_acthd;
		u_int32_t	last_instdone;
		u_int32_t	last_instdone1;

		uint32_t next_gem_seqno;

		/**
		 * Flag if the X Server, and thus DRM, is not currently in
		 * control of the device.
		 *
		 * This is set between LeaveVT and EnterVT.  It needs to be
		 * replaced with a semaphore.  It also needs to be
		 * transitioned away from for kernel modesetting.
		 */
		int suspended;

		/**
		 * Flag if the hardware appears to be wedged.
		 *
		 * This is set when attempts to idle the device timeout.
		 * It prevents command submission from occuring and makes
		 * every pending request fail
		 */
		int wedged;

		/** Bit 6 swizzling required for X tiling */
		uint32_t bit_6_swizzle_x;
		/** Bit 6 swizzling required for Y tiling */
		uint32_t bit_6_swizzle_y;
	} mm;
};

struct inteldrm_file {
	struct drm_file	file_priv;
	struct {
	} mm;
};

/* chip type flags */
#define CHIP_I830	0x00001
#define CHIP_I845G	0x00002
#define CHIP_I85X	0x00004
#define CHIP_I865G	0x00008
#define CHIP_I9XX	0x00010
#define CHIP_I915G	0x00020
#define CHIP_I915GM	0x00040
#define CHIP_I945G	0x00080
#define CHIP_I945GM	0x00100
#define CHIP_I965	0x00200
#define CHIP_I965GM	0x00400
#define CHIP_G33	0x00800
#define CHIP_GM45	0x01000
#define CHIP_G4X	0x02000
#define CHIP_M		0x04000
#define CHIP_HWS	0x08000
#define CHIP_GEN2	0x10000
#define CHIP_GEN3	0x20000
#define CHIP_GEN4	0x40000
#define CHIP_GEN6	0x80000
#define	CHIP_PINEVIEW	0x100000
#define	CHIP_IRONLAKE	0x200000
#define CHIP_IRONLAKE_D	0x400000
#define CHIP_IRONLAKE_M	0x800000
#define CHIP_SANDYBRIDGE	0x1000000
#define CHIP_IVYBRIDGE	0x2000000
#define CHIP_GEN7	0x4000000

/* flags we use in drm_obj's do_flags */
#define I915_ACTIVE		0x0010	/* being used by the gpu. */
#define I915_IN_EXEC		0x0020	/* being processed in execbuffer */
#define I915_USER_PINNED	0x0040	/* BO has been pinned from userland */
#define I915_GPU_WRITE		0x0080	/* BO has been not flushed */
#define I915_DONTNEED		0x0100	/* BO backing pages purgable */
#define I915_PURGED		0x0200	/* BO backing pages purged */
#define I915_DIRTY		0x0400	/* BO written to since last bound */
#define I915_EXEC_NEEDS_FENCE	0x0800	/* being processed but will need fence*/
#define I915_FENCED_EXEC	0x1000	/* Most recent exec needs fence */
#define I915_FENCE_INVALID	0x2000	/* fence has been lazily invalidated */

/** driver private structure attached to each drm_gem_object */
struct inteldrm_obj {
	struct drm_obj				 obj;

	/** This object's place on the active/flushing/inactive lists */
	TAILQ_ENTRY(inteldrm_obj)		 list;
	TAILQ_ENTRY(inteldrm_obj)		 write_list;
	struct i915_gem_list			*current_list;
	/* GTT binding. */
	bus_dmamap_t				 dmamap;
	bus_dma_segment_t			*dma_segs;
	/* Current offset of the object in GTT space. */
	bus_addr_t				 gtt_offset;
	u_int32_t				*bit_17;
	/* extra flags to bus_dma */
	int					 dma_flags;
	/* Fence register for this object. needed for tiling. */
	int					 fence_reg;
	/** refcount for times pinned this object in GTT space */
	int					 pin_count;
	/* number of times pinned by pin ioctl. */
	u_int					 user_pin_count;

	/** Breadcrumb of last rendering to the buffer. */
	u_int32_t				 last_rendering_seqno;
	u_int32_t				 last_write_seqno;
	/** Current tiling mode for the object. */
	u_int32_t				 tiling_mode;
	u_int32_t				 stride;
};

/**
 * Request queue structure.
 *
 * The request queue allows us to note sequence numbers that have been emitted
 * and may be associated with active buffers to be retired.
 *
 * By keeping this list, we can avoid having to do questionable
 * sequence-number comparisons on buffer last_rendering_seqnos, and associate
 * an emission time with seqnos for tracking how far ahead of the GPU we are.
 */
struct inteldrm_request {
	TAILQ_ENTRY(inteldrm_request)	list;
	/** GEM sequence number associated with this request. */
	uint32_t			seqno;
};

u_int32_t	inteldrm_read_hws(struct inteldrm_softc *, int);
int		inteldrm_wait_ring(struct inteldrm_softc *dev, int n);
void		inteldrm_begin_ring(struct inteldrm_softc *, int);
void		inteldrm_out_ring(struct inteldrm_softc *, u_int32_t);
void		inteldrm_advance_ring(struct inteldrm_softc *);
void		inteldrm_update_ring(struct inteldrm_softc *);
int		inteldrm_pipe_enabled(struct inteldrm_softc *, int);
int		i915_init_phys_hws(struct inteldrm_softc *, bus_dma_tag_t);

/* i915_irq.c */

extern int i915_driver_irq_install(struct drm_device * dev);
extern void i915_driver_irq_uninstall(struct drm_device * dev);
extern int i915_enable_vblank(struct drm_device *dev, int crtc);
extern void i915_disable_vblank(struct drm_device *dev, int crtc);
extern u32 i915_get_vblank_counter(struct drm_device *dev, int crtc);
extern void i915_user_irq_get(struct inteldrm_softc *);
extern void i915_user_irq_put(struct inteldrm_softc *);

/* i915_suspend.c */

extern void i915_save_display(struct inteldrm_softc *);
extern void i915_restore_display(struct inteldrm_softc *);
extern int i915_save_state(struct inteldrm_softc *);
extern int i915_restore_state(struct inteldrm_softc *);

/* XXX need bus_space_write_8, this evaluated arguments twice */
static __inline void
write64(struct inteldrm_softc *dev_priv, bus_size_t off, u_int64_t reg)
{
	bus_space_write_4(dev_priv->regs->bst, dev_priv->regs->bsh,
	    off, (u_int32_t)reg);
	bus_space_write_4(dev_priv->regs->bst, dev_priv->regs->bsh,
	    off + 4, upper_32_bits(reg));
}

static __inline u_int64_t
read64(struct inteldrm_softc *dev_priv, bus_size_t off)
{
	u_int32_t low, high;

	low = bus_space_read_4(dev_priv->regs->bst,
	    dev_priv->regs->bsh, off);
	high = bus_space_read_4(dev_priv->regs->bst,
	    dev_priv->regs->bsh, off + 4);

	return ((u_int64_t)low | ((u_int64_t)high << 32));
}

#define I915_READ64(off)	read64(dev_priv, off)

#define I915_WRITE64(off, reg)	write64(dev_priv, off, reg)

#define I915_READ(reg)		bus_space_read_4(dev_priv->regs->bst,	\
				    dev_priv->regs->bsh, (reg))
#define I915_WRITE(reg,val)	bus_space_write_4(dev_priv->regs->bst,	\
				    dev_priv->regs->bsh, (reg), (val))
#define I915_READ16(reg)	bus_space_read_2(dev_priv->regs->bst,	\
				    dev_priv->regs->bsh, (reg))
#define I915_WRITE16(reg,val)	bus_space_write_2(dev_priv->regs->bst,	\
				    dev_priv->regs->bsh, (reg), (val))
#define I915_READ8(reg)		bus_space_read_1(dev_priv->regs->bst,	\
				    dev_priv->regs->bsh, (reg))
#define I915_WRITE8(reg,val)	bus_space_write_1(dev_priv->regs->bst,	\
				    dev_priv->regs->bsh, (reg), (val))

#define POSTING_READ(reg)	(void)I915_READ(reg)
#define POSTING_READ16(reg)	(void)I915_READ16(reg)

#define INTELDRM_VERBOSE 0
#if INTELDRM_VERBOSE > 0
#define	INTELDRM_VPRINTF(fmt, args...) DRM_INFO(fmt, ##args)
#else
#define	INTELDRM_VPRINTF(fmt, args...)
#endif

#define BEGIN_LP_RING(n) inteldrm_begin_ring(dev_priv, n)
#define OUT_RING(n) inteldrm_out_ring(dev_priv, n)
#define ADVANCE_LP_RING() inteldrm_advance_ring(dev_priv)

/* MCH IFP BARs */
#define	I915_IFPADDR	0x60
#define I965_IFPADDR	0x70

/**
 * Reads a dword out of the status page, which is written to from the command
 * queue by automatic updates, MI_REPORT_HEAD, MI_STORE_DATA_INDEX, or
 * MI_STORE_DATA_IMM.
 *
 * The following dwords have a reserved meaning:
 * 0x00: ISR copy, updated when an ISR bit not set in the HWSTAM changes.
 * 0x04: ring 0 head pointer
 * 0x05: ring 1 head pointer (915-class)
 * 0x06: ring 2 head pointer (915-class)
 * 0x10-0x1b: Context status DWords (GM45)
 * 0x1f: Last written status offset. (GM45)
 *
 * The area from dword 0x20 to 0x3ff is available for driver usage.
 */
#define READ_HWSP(dev_priv, reg)  inteldrm_read_hws(dev_priv, reg)
#define I915_GEM_HWS_INDEX		0x20

/* Chipset type macros */

#define IS_I830(dev_priv) ((dev_priv)->flags & CHIP_I830)
#define IS_845G(dev_priv) ((dev_priv)->flags & CHIP_I845G)
#define IS_I85X(dev_priv) ((dev_priv)->flags & CHIP_I85X)
#define IS_I865G(dev_priv) ((dev_priv)->flags & CHIP_I865G)

#define IS_I915G(dev_priv) ((dev_priv)->flags & CHIP_I915G)
#define IS_I915GM(dev_priv) ((dev_priv)->flags & CHIP_I915GM)
#define IS_I945G(dev_priv) ((dev_priv)->flags & CHIP_I945G)
#define IS_I945GM(dev_priv) ((dev_priv)->flags & CHIP_I945GM)
#define IS_I965G(dev_priv) ((dev_priv)->flags & CHIP_I965)
#define IS_I965GM(dev_priv) ((dev_priv)->flags & CHIP_I965GM)

#define IS_GM45(dev_priv) ((dev_priv)->flags & CHIP_GM45)
#define IS_G4X(dev_priv) ((dev_priv)->flags & CHIP_G4X)

#define IS_G33(dev_priv)    ((dev_priv)->flags & CHIP_G33)

#define IS_I9XX(dev_priv) ((dev_priv)->flags & CHIP_I9XX)

#define IS_IRONLAKE(dev_priv) ((dev_priv)->flags & CHIP_IRONLAKE)
#define IS_IRONLAKE_D(dev_priv) ((dev_priv)->flags & CHIP_IRONLAKE_D)
#define IS_IRONLAKE_M(dev_priv) ((dev_priv)->flags & CHIP_IRONLAKE_M)

#define IS_SANDYBRIDGE(dev_priv) ((dev_priv)->flags & CHIP_SANDYBRIDGE)
#define IS_SANDYBRIDGE_D(dev_priv) ((dev_priv)->flags & CHIP_SANDYBRIDGE_D)
#define IS_SANDYBRIDGE_M(dev_priv) ((dev_priv)->flags & CHIP_SANDYBRIDGE_M)

#define IS_MOBILE(dev_priv) (dev_priv->flags & CHIP_M)

#define I915_NEED_GFX_HWS(dev_priv) (dev_priv->flags & CHIP_HWS)

#define HAS_RESET(dev_priv)	IS_I965G(dev_priv) && (!IS_GEN6(dev_priv)) \
    && (!IS_GEN7(dev_priv))

#define IS_GEN2(dev_priv)	(dev_priv->flags & CHIP_GEN2)
#define IS_GEN3(dev_priv)	(dev_priv->flags & CHIP_GEN3)
#define IS_GEN4(dev_priv)	(dev_priv->flags & CHIP_GEN4)
#define IS_GEN6(dev_priv)	(dev_priv->flags & CHIP_GEN6)
#define IS_GEN7(dev_priv)	(dev_priv->flags & CHIP_GEN7)

#define I915_HAS_FBC(dev)	(IS_I965GM(dev) || IS_GM45(dev))

#define HAS_PCH_SPLIT(dev)	(IS_IRONLAKE(dev) || IS_GEN6(dev) || \
    IS_GEN7(dev))

#define INTEL_INFO(dev)		(dev)

/*
 * Interrupts that are always left unmasked.
 *
 * Since pipe events are edge-triggered from the PIPESTAT register to IIRC,
 * we leave them always unmasked in IMR and then control enabling them through
 * PIPESTAT alone.
 */
#define I915_INTERRUPT_ENABLE_FIX		\
	(I915_DISPLAY_PIPE_A_EVENT_INTERRUPT |	\
	I915_DISPLAY_PIPE_B_EVENT_INTERRUPT |	\
	I915_RENDER_COMMAND_PARSER_ERROR_INTERRUPT)

/* Interrupts that we mask and unmask at runtime */
#define I915_INTERRUPT_ENABLE_VAR	(I915_USER_INTERRUPT)

/* These are all of the interrupts used by the driver */
#define I915_INTERRUPT_ENABLE_MASK	\
	(I915_INTERRUPT_ENABLE_FIX |	\
	I915_INTERRUPT_ENABLE_VAR)

/*
 * if kms we want pch event, gse, and plane flip masks too
 */
#define PCH_SPLIT_DISPLAY_INTR_FIX	(DE_MASTER_IRQ_CONTROL)
#define PCH_SPLIT_DISPLAY_INTR_VAR	(DE_PIPEA_VBLANK | DE_PIPEB_VBLANK)
#define PCH_SPLIT_DISPLAY_ENABLE_MASK	\
	(PCH_SPLIT_DISPLAY_INTR_FIX | PCH_SPLIT_DISPLAY_INTR_VAR)
#define PCH_SPLIT_RENDER_INTR_FIX	(0)
#define PCH_SPLIT_RENDER_INTR_VAR	(GT_USER_INTERRUPT | GT_MASTER_ERROR)
#define PCH_SPLIT_RENDER_ENABLE_MASK	\
	(PCH_SPLIT_RENDER_INTR_FIX | PCH_SPLIT_RENDER_INTR_VAR)
/* not yet */
#define PCH_SPLIT_HOTPLUG_INTR_FIX	(0)
#define PCH_SPLIT_HOTPLUG_INTR_VAR	(0)
#define PCH_SPLIT_HOTPLUG_ENABLE_MASK	\
	(PCH_SPLIT_HOTPLUG_INTR_FIX | PCH_SPLIT_HOTPLUG_INTR_VAR)

#define	printeir(val)	printf("%s: error reg: %b\n", __func__, val,	\
	"\20\x10PTEERR\x2REFRESHERR\x1INSTERR")

/*
 * With the i45 and later, Y tiling got adjusted so that it was 32 128-byte
 * rows, which changes the alignment requirements and fence programming.
 */
#define HAS_128_BYTE_Y_TILING(dev_priv) (IS_I9XX(dev_priv) &&	\
	!(IS_I915G(dev_priv) || IS_I915GM(dev_priv)))

#define PRIMARY_RINGBUFFER_SIZE         (128*1024)

/* Inlines */

/**
 * Returns true if seq1 is later than seq2.
 */
static __inline int
i915_seqno_passed(uint32_t seq1, uint32_t seq2)
{
	return ((int32_t)(seq1 - seq2) >= 0);
}

/*
 * Read seqence number from the Hardware status page.
 */
static __inline u_int32_t
i915_get_gem_seqno(struct inteldrm_softc *dev_priv)
{
	return (READ_HWSP(dev_priv, I915_GEM_HWS_INDEX));
}

static __inline int
i915_obj_purgeable(struct inteldrm_obj *obj_priv)
{
	return (obj_priv->obj.do_flags & I915_DONTNEED);
}

static __inline int
i915_obj_purged(struct inteldrm_obj *obj_priv)
{
	return (obj_priv->obj.do_flags & I915_PURGED);
}

static __inline int
inteldrm_is_active(struct inteldrm_obj *obj_priv)
{
	return (obj_priv->obj.do_flags & I915_ACTIVE);
}

static __inline int
inteldrm_is_dirty(struct inteldrm_obj *obj_priv)
{
	return (obj_priv->obj.do_flags & I915_DIRTY);
}

static __inline int
inteldrm_exec_needs_fence(struct inteldrm_obj *obj_priv)
{
	return (obj_priv->obj.do_flags & I915_EXEC_NEEDS_FENCE);
}

static __inline int
inteldrm_needs_fence(struct inteldrm_obj *obj_priv)
{
	return (obj_priv->obj.do_flags & I915_FENCED_EXEC);
}

#endif
