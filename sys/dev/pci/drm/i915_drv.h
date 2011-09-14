/* $OpenBSD: i915_drv.h,v 1.70 2011/09/14 10:26:16 oga Exp $ */
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

/*
 * The Bridge device's PCI config space has information about the
 * fb aperture size and the amount of pre-reserved memory.
 */
#define INTEL_GMCH_CTRL		0x52
#define INTEL_GMCH_ENABLED	0x4
#define INTEL_GMCH_MEM_MASK	0x1
#define INTEL_GMCH_MEM_64M	0x1
#define INTEL_GMCH_MEM_128M	0

#define INTEL_855_GMCH_GMS_MASK		(0x7 << 4)
#define INTEL_855_GMCH_GMS_DISABLED	(0x0 << 4)
#define INTEL_855_GMCH_GMS_STOLEN_1M	(0x1 << 4)
#define INTEL_855_GMCH_GMS_STOLEN_4M	(0x2 << 4)
#define INTEL_855_GMCH_GMS_STOLEN_8M	(0x3 << 4)
#define INTEL_855_GMCH_GMS_STOLEN_16M	(0x4 << 4)
#define INTEL_855_GMCH_GMS_STOLEN_32M	(0x5 << 4)

#define INTEL_915G_GMCH_GMS_STOLEN_48M	(0x6 << 4)
#define INTEL_915G_GMCH_GMS_STOLEN_64M	(0x7 << 4)

/* PCI config space */

#define HPLLCC	0xc0 /* 855 only */
#define   GC_CLOCK_CONTROL_MASK		(3 << 0)
#define   GC_CLOCK_133_200		(0 << 0)
#define   GC_CLOCK_100_200		(1 << 0)
#define   GC_CLOCK_100_133		(2 << 0)
#define   GC_CLOCK_166_250		(3 << 0)
#define GCFGC	0xf0 /* 915+ only */
#define   GC_LOW_FREQUENCY_ENABLE	(1 << 7)
#define   GC_DISPLAY_CLOCK_190_200_MHZ	(0 << 4)
#define   GC_DISPLAY_CLOCK_333_MHZ	(4 << 4)
#define   GC_DISPLAY_CLOCK_MASK		(7 << 4)
#define LBB	0xf4
/* reset register, 965+. this is byte-sized */
#define GDRST	0xc0
#define GDRST_FULL	(0<<2)
#define GDRST_RENDER	(1<<2)
#define GDRST_MEDIA	(3<<2)

/* VGA stuff */

#define VGA_ST01_MDA 0x3ba
#define VGA_ST01_CGA 0x3da

#define VGA_MSR_WRITE 0x3c2
#define VGA_MSR_READ 0x3cc
#define   VGA_MSR_MEM_EN (1<<1)
#define   VGA_MSR_CGA_MODE (1<<0)

#define VGA_SR_INDEX 0x3c4
#define VGA_SR_DATA 0x3c5

#define VGA_AR_INDEX 0x3c0
#define   VGA_AR_VID_EN (1<<5)
#define VGA_AR_DATA_WRITE 0x3c0
#define VGA_AR_DATA_READ 0x3c1

#define VGA_GR_INDEX 0x3ce
#define VGA_GR_DATA 0x3cf
/* GR05 */
#define   VGA_GR_MEM_READ_MODE_SHIFT 3
#define     VGA_GR_MEM_READ_MODE_PLANE 1
/* GR06 */
#define   VGA_GR_MEM_MODE_MASK 0xc
#define   VGA_GR_MEM_MODE_SHIFT 2
#define   VGA_GR_MEM_A0000_AFFFF 0
#define   VGA_GR_MEM_A0000_BFFFF 1
#define   VGA_GR_MEM_B0000_B7FFF 2
#define   VGA_GR_MEM_B0000_BFFFF 3

#define VGA_DACMASK 0x3c6
#define VGA_DACRX 0x3c7
#define VGA_DACWX 0x3c8
#define VGA_DACDATA 0x3c9

#define VGA_CR_INDEX_MDA 0x3b4
#define VGA_CR_DATA_MDA 0x3b5
#define VGA_CR_INDEX_CGA 0x3d4
#define VGA_CR_DATA_CGA 0x3d5

/*
 * Memory interface instructions used by the kernel
 */
#define MI_INSTR(opcode, flags) (((opcode) << 23) | (flags))

#define MI_NOOP			MI_INSTR(0, 0)
#define MI_USER_INTERRUPT	MI_INSTR(0x02, 0)
#define MI_WAIT_FOR_EVENT       MI_INSTR(0x03, 0)
#define   MI_WAIT_FOR_PLANE_B_FLIP      (1<<6)
#define   MI_WAIT_FOR_PLANE_A_FLIP      (1<<2)
#define   MI_WAIT_FOR_PLANE_A_SCANLINES (1<<1)
#define MI_FLUSH		MI_INSTR(0x04, 0)
#define   MI_READ_FLUSH		(1 << 0)
#define   MI_EXE_FLUSH		(1 << 1)
#define   MI_NO_WRITE_FLUSH	(1 << 2)
#define   MI_SCENE_COUNT	(1 << 3) /* just increment scene count */
#define   MI_END_SCENE		(1 << 4) /* flush binner and incr scene count */
#define MI_BATCH_BUFFER_END	MI_INSTR(0x0a, 0)
#define MI_REPORT_HEAD		MI_INSTR(0x07, 0)
#define MI_LOAD_SCAN_LINES_INCL MI_INSTR(0x12, 0)
#define MI_STORE_DWORD_IMM	MI_INSTR(0x20, 1)
#define   MI_MEM_VIRTUAL	(1 << 22) /* 965+ only */
#define MI_STORE_DWORD_INDEX	MI_INSTR(0x21, 1)
#define MI_STORE_DWORD_INDEX_SHIFT 2
#define MI_LOAD_REGISTER_IMM	MI_INSTR(0x22, 1)
#define MI_BATCH_BUFFER		MI_INSTR(0x30, 1)
#define   MI_BATCH_NON_SECURE	(1)
#define   MI_BATCH_NON_SECURE_I965 (1<<8)
#define MI_BATCH_BUFFER_START	MI_INSTR(0x31, 0)

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

/*
 * 3D instructions used by the kernel
 */
#define GFX_INSTR(opcode, flags) ((0x3 << 29) | ((opcode) << 24) | (flags))

#define GFX_OP_RASTER_RULES    ((0x3<<29)|(0x7<<24))
#define GFX_OP_SCISSOR         ((0x3<<29)|(0x1c<<24)|(0x10<<19))
#define   SC_UPDATE_SCISSOR       (0x1<<1)
#define   SC_ENABLE_MASK          (0x1<<0)
#define   SC_ENABLE               (0x1<<0)
#define GFX_OP_LOAD_INDIRECT   ((0x3<<29)|(0x1d<<24)|(0x7<<16))
#define GFX_OP_SCISSOR_INFO    ((0x3<<29)|(0x1d<<24)|(0x81<<16)|(0x1))
#define   SCI_YMIN_MASK      (0xffff<<16)
#define   SCI_XMIN_MASK      (0xffff<<0)
#define   SCI_YMAX_MASK      (0xffff<<16)
#define   SCI_XMAX_MASK      (0xffff<<0)
#define GFX_OP_SCISSOR_ENABLE	 ((0x3<<29)|(0x1c<<24)|(0x10<<19))
#define GFX_OP_SCISSOR_RECT	 ((0x3<<29)|(0x1d<<24)|(0x81<<16)|1)
#define GFX_OP_COLOR_FACTOR      ((0x3<<29)|(0x1d<<24)|(0x1<<16)|0x0)
#define GFX_OP_STIPPLE           ((0x3<<29)|(0x1d<<24)|(0x83<<16))
#define GFX_OP_MAP_INFO          ((0x3<<29)|(0x1d<<24)|0x4)
#define GFX_OP_DESTBUFFER_VARS   ((0x3<<29)|(0x1d<<24)|(0x85<<16)|0x0)
#define GFX_OP_DESTBUFFER_INFO	 ((0x3<<29)|(0x1d<<24)|(0x8e<<16)|1)
#define GFX_OP_DRAWRECT_INFO     ((0x3<<29)|(0x1d<<24)|(0x80<<16)|(0x3))
#define GFX_OP_DRAWRECT_INFO_I965  ((0x7900<<16)|0x2)
#define SRC_COPY_BLT_CMD                ((2<<29)|(0x43<<22)|4)
#define XY_SRC_COPY_BLT_CMD		((2<<29)|(0x53<<22)|6)
#define XY_MONO_SRC_COPY_IMM_BLT	((2<<29)|(0x71<<22)|5)
#define XY_SRC_COPY_BLT_WRITE_ALPHA	(1<<21)
#define XY_SRC_COPY_BLT_WRITE_RGB	(1<<20)
#define   BLT_DEPTH_8			(0<<24)
#define   BLT_DEPTH_16_565		(1<<24)
#define   BLT_DEPTH_16_1555		(2<<24)
#define   BLT_DEPTH_32			(3<<24)
#define   BLT_ROP_GXCOPY		(0xcc<<16)
#define XY_SRC_COPY_BLT_SRC_TILED	(1<<15) /* 965+ only */
#define XY_SRC_COPY_BLT_DST_TILED	(1<<11) /* 965+ only */
#define CMD_OP_DISPLAYBUFFER_INFO ((0x0<<29)|(0x14<<23)|2)
#define   ASYNC_FLIP                (1<<22)
#define   DISPLAY_PLANE_A           (0<<20)
#define   DISPLAY_PLANE_B           (1<<20)

/*
 * Fence registers
 */
#define FENCE_REG_830_0			0x2000
#define FENCE_REG_945_8			0x3000
#define   I830_FENCE_START_MASK		0x07f80000
#define   I830_FENCE_TILING_Y_SHIFT	12
#define   I830_FENCE_SIZE_BITS(size)	((ffs((size) >> 19) - 1) << 8)
#define   I830_FENCE_PITCH_SHIFT	4
#define   I830_FENCE_REG_VALID		(1<<0)
#define   I915_FENCE_MAX_PITCH_VAL	4
#define   I830_FENCE_MAX_PITCH_VAL	6
#define   I830_FENCE_MAX_SIZE_VAL	(1<<8)

#define   I915_FENCE_START_MASK		0x0ff00000
#define   I915_FENCE_SIZE_BITS(size)	((ffs((size) >> 20) - 1) << 8)

#define FENCE_REG_965_0			0x03000
#define   I965_FENCE_PITCH_SHIFT	2
#define   I965_FENCE_TILING_Y_SHIFT	1
#define   I965_FENCE_REG_VALID		(1<<0)
#define   I965_FENCE_MAX_PITCH_VAL	0x0400

#define FENCE_REG_SANDYBRIDGE_0		0x100000
#define   SANDYBRIDGE_FENCE_PITCH_SHIFT	32

/*
 * Instruction and interrupt control regs
 */

#define PGTBL_ER	0x02024
#define PRB0_TAIL	0x02030
#define PRB0_HEAD	0x02034
#define PRB0_START	0x02038
#define PRB0_CTL	0x0203c
#define   TAIL_ADDR		0x001FFFF8
#define   HEAD_WRAP_COUNT	0xFFE00000
#define   HEAD_WRAP_ONE		0x00200000
#define   HEAD_ADDR		0x001FFFFC
#define   RING_NR_PAGES		0x001FF000
#define   RING_REPORT_MASK	0x00000006
#define   RING_REPORT_64K	0x00000002
#define   RING_REPORT_128K	0x00000004
#define   RING_NO_REPORT	0x00000000
#define   RING_VALID_MASK	0x00000001
#define   RING_VALID		0x00000001
#define   RING_INVALID		0x00000000
#define   RING_WAIT_I8XX	(1<<0) /* gen2, PRBx_HEAD */
#define   RING_WAIT		(1<<11) /* gen3+, PRBx_CTL */
#define PRB1_TAIL	0x02040 /* 915+ only */
#define PRB1_HEAD	0x02044 /* 915+ only */
#define PRB1_START	0x02048 /* 915+ only */
#define PRB1_CTL	0x0204c /* 915+ only */
#define IPEIR_I965	0x02064
#define IPEHR_I965	0x02068
#define INSTDONE_I965	0x0206c
#define INSTPS		0x02070 /* 965+ only */
#define INSTDONE1	0x0207c /* 965+ only */
#define ACTHD_I965	0x02074
#define HWS_PGA		0x02080
#define HWS_PGA_GEN6	0x04080
#define HWS_ADDRESS_MASK	0xfffff000
#define HWS_START_ADDRESS_SHIFT	4
#define IPEIR		0x02088
#define IPEHR		0x0208c
#define INSTDONE	0x02090
#define NOPID		0x02094
#define HWSTAM		0x02098

#define MI_MODE		0x0209c
#define VS_TIMER_DISPATCH	(1 << 6)
#define MI_FLUSH_ENABLE		(1 << 11)

#define SCPD0		0x0209c /* 915+ only */
#define IER		0x020a0
#define IIR		0x020a4
#define IMR		0x020a8
#define ISR		0x020ac
#define   I915_PIPE_CONTROL_NOTIFY_INTERRUPT		(1<<18)
#define   I915_DISPLAY_PORT_INTERRUPT			(1<<17)
#define   I915_RENDER_COMMAND_PARSER_ERROR_INTERRUPT	(1<<15)
#define   I915_GMCH_THERMAL_SENSOR_EVENT_INTERRUPT	(1<<14)
#define   I915_HWB_OOM_INTERRUPT			(1<<13)
#define   I915_SYNC_STATUS_INTERRUPT			(1<<12)
#define   I915_DISPLAY_PLANE_A_FLIP_PENDING_INTERRUPT	(1<<11)
#define   I915_DISPLAY_PLANE_B_FLIP_PENDING_INTERRUPT	(1<<10)
#define   I915_OVERLAY_PLANE_FLIP_PENDING_INTERRUPT	(1<<9)
#define   I915_DISPLAY_PLANE_C_FLIP_PENDING_INTERRUPT	(1<<8)
#define   I915_DISPLAY_PIPE_A_VBLANK_INTERRUPT		(1<<7)
#define   I915_DISPLAY_PIPE_A_EVENT_INTERRUPT		(1<<6)
#define   I915_DISPLAY_PIPE_B_VBLANK_INTERRUPT		(1<<5)
#define   I915_DISPLAY_PIPE_B_EVENT_INTERRUPT		(1<<4)
#define   I915_DEBUG_INTERRUPT				(1<<2)
#define   I915_USER_INTERRUPT				(1<<1)
#define   I915_ASLE_INTERRUPT				(1<<0)
#define EIR		0x020b0
#define EMR		0x020b4
#define ESR		0x020b8
#define   GM45_ERROR_PAGE_TABLE				(1<<5)
#define   GM45_ERROR_MEM_PRIV				(1<<4)
#define   I915_ERROR_PAGE_TABLE				(1<<4)
#define   GM45_ERROR_CP_PRIV				(1<<3)
#define   I915_ERROR_MEMORY_REFRESH			(1<<1)
#define   I915_ERROR_INSTRUCTION			(1<<0)
/* ironlake error bits */
#define   GT_ERROR_PTE					(1<<4)
	/* memory privilege violation error */
#define   GT_ERROR_MPE					(1<<3)
	/* command privilege violation error */
#define   GT_ERROR_CPE					(1<<2)
#define INSTPM	        0x020c0
#define ACTHD	        0x020c8
#define FW_BLC		0x020d8
#define FW_BLC_SELF	0x020e0 /* 915+ only */
#define MI_ARB_STATE	0x020e4 /* 915+ only */
#define   MI_ARB_MASK_SHIFT	  16	/* shift for enable bits */

/* Make render/texture TLB fetches lower priorty than associated data
 *   fetches. This is not turned on by default
 */
#define   MI_ARB_RENDER_TLB_LOW_PRIORITY	(1 << 15)

/* Isoch request wait on GTT enable (Display A/B/C streams).
 * Make isoch requests stall on the TLB update. May cause
 * display underruns (test mode only)
 */
#define   MI_ARB_ISOCH_WAIT_GTT			(1 << 14)

/* Block grant count for isoch requests when block count is
 * set to a finite value.
 */
#define   MI_ARB_BLOCK_GRANT_MASK		(3 << 12)
#define   MI_ARB_BLOCK_GRANT_8			(0 << 12)	/* for 3 display planes */
#define   MI_ARB_BLOCK_GRANT_4			(1 << 12)	/* for 2 display planes */
#define   MI_ARB_BLOCK_GRANT_2			(2 << 12)	/* for 1 display plane */
#define   MI_ARB_BLOCK_GRANT_0			(3 << 12)	/* don't use */

/* Enable render writes to complete in C2/C3/C4 power states.
 * If this isn't enabled, render writes are prevented in low
 * power states. That seems bad to me.
 */
#define   MI_ARB_C3_LP_WRITE_ENABLE		(1 << 11)

/* This acknowledges an async flip immediately instead
 * of waiting for 2TLB fetches.
 */
#define   MI_ARB_ASYNC_FLIP_ACK_IMMEDIATE	(1 << 10)

/* Enables non-sequential data reads through arbiter
 */
#define   MI_ARB_DUAL_DATA_PHASE_DISABLE       	(1 << 9)

/* Disable FSB snooping of cacheable write cycles from binner/render
 * command stream
 */
#define   MI_ARB_CACHE_SNOOP_DISABLE		(1 << 8)

/* Arbiter time slice for non-isoch streams */
#define   MI_ARB_TIME_SLICE_MASK		(7 << 5)
#define   MI_ARB_TIME_SLICE_1			(0 << 5)
#define   MI_ARB_TIME_SLICE_2			(1 << 5)
#define   MI_ARB_TIME_SLICE_4			(2 << 5)
#define   MI_ARB_TIME_SLICE_6			(3 << 5)
#define   MI_ARB_TIME_SLICE_8			(4 << 5)
#define   MI_ARB_TIME_SLICE_10			(5 << 5)
#define   MI_ARB_TIME_SLICE_14			(6 << 5)
#define   MI_ARB_TIME_SLICE_16			(7 << 5)

/* Low priority grace period page size */
#define   MI_ARB_LOW_PRIORITY_GRACE_4KB		(0 << 4)	/* default */
#define   MI_ARB_LOW_PRIORITY_GRACE_8KB		(1 << 4)

/* Disable display A/B trickle feed */
#define   MI_ARB_DISPLAY_TRICKLE_FEED_DISABLE	(1 << 2)

/* Set display plane priority */
#define   MI_ARB_DISPLAY_PRIORITY_A_B		(0 << 0)	/* display A > display B */
#define   MI_ARB_DISPLAY_PRIORITY_B_A		(1 << 0)	/* display B > display A */

#define CACHE_MODE_0	0x02120 /* 915+ only */
#define   CM0_MASK_SHIFT          16
#define   CM0_IZ_OPT_DISABLE      (1<<6)
#define   CM0_ZR_OPT_DISABLE      (1<<5)
#define   CM0_DEPTH_EVICT_DISABLE (1<<4)
#define   CM0_COLOR_EVICT_DISABLE (1<<3)
#define   CM0_DEPTH_WRITE_DISABLE (1<<1)
#define   CM0_RC_OP_FLUSH_DISABLE (1<<0)
#define GFX_FLSH_CNTL	0x02170 /* 915+ only */

/*
 * Framebuffer compression (915+ only)
 */

#define FBC_CFB_BASE		0x03200 /* 4k page aligned */
#define FBC_LL_BASE		0x03204 /* 4k page aligned */
#define FBC_CONTROL		0x03208
#define   FBC_CTL_EN		(1<<31)
#define   FBC_CTL_PERIODIC	(1<<30)
#define   FBC_CTL_INTERVAL_SHIFT (16)
#define   FBC_CTL_UNCOMPRESSIBLE (1<<14)
#define   FBC_CTL_STRIDE_SHIFT	(5)
#define   FBC_CTL_FENCENO	(1<<0)
#define FBC_COMMAND		0x0320c
#define   FBC_CMD_COMPRESS	(1<<0)
#define FBC_STATUS		0x03210
#define   FBC_STAT_COMPRESSING	(1<<31)
#define   FBC_STAT_COMPRESSED	(1<<30)
#define   FBC_STAT_MODIFIED	(1<<29)
#define   FBC_STAT_CURRENT_LINE	(1<<0)
#define FBC_CONTROL2		0x03214
#define   FBC_CTL_FENCE_DBL	(0<<4)
#define   FBC_CTL_IDLE_IMM	(0<<2)
#define   FBC_CTL_IDLE_FULL	(1<<2)
#define   FBC_CTL_IDLE_LINE	(2<<2)
#define   FBC_CTL_IDLE_DEBUG	(3<<2)
#define   FBC_CTL_CPU_FENCE	(1<<1)
#define   FBC_CTL_PLANEA	(0<<0)
#define   FBC_CTL_PLANEB	(1<<0)
#define FBC_FENCE_OFF		0x0321b

#define FBC_LL_SIZE		(1536)

/* Framebuffer compression for GM45+ */
#define DPFC_CB_BASE		0x3200
#define DPFC_CONTROL		0x3208
#define   DPFC_CTL_EN		(1<<31)
#define   DPFC_CTL_PLANEA	(0<<30)
#define   DPFC_CTL_PLANEB	(1<<30)
#define   DPFC_CTL_FENCE_EN	(1<<29)
#define   DPFC_SR_EN		(1<<10)
#define   DPFC_CTL_LIMIT_1X	(0<<6)
#define   DPFC_CTL_LIMIT_2X	(1<<6)
#define   DPFC_CTL_LIMIT_4X	(2<<6)
#define DPFC_RECOMP_CTL		0x320c
#define   DPFC_RECOMP_STALL_EN	(1<<27)
#define   DPFC_RECOMP_STALL_WM_SHIFT (16)
#define   DPFC_RECOMP_STALL_WM_MASK (0x07ff0000)
#define   DPFC_RECOMP_TIMER_COUNT_SHIFT (0)
#define   DPFC_RECOMP_TIMER_COUNT_MASK (0x0000003f)
#define DPFC_STATUS		0x3210
#define   DPFC_INVAL_SEG_SHIFT  (16)
#define   DPFC_INVAL_SEG_MASK	(0x07ff0000)
#define   DPFC_COMP_SEG_SHIFT	(0)
#define   DPFC_COMP_SEG_MASK	(0x000003ff)
#define DPFC_STATUS2		0x3214
#define DPFC_FENCE_YOFF		0x3218
#define DPFC_CHICKEN		0x3224
#define   DPFC_HT_MODIFY	(1<<31)

/*
 * GPIO regs
 */
#define GPIOA			0x5010
#define GPIOB			0x5014
#define GPIOC			0x5018
#define GPIOD			0x501c
#define GPIOE			0x5020
#define GPIOF			0x5024
#define GPIOG			0x5028
#define GPIOH			0x502c
# define GPIO_CLOCK_DIR_MASK		(1 << 0)
# define GPIO_CLOCK_DIR_IN		(0 << 1)
# define GPIO_CLOCK_DIR_OUT		(1 << 1)
# define GPIO_CLOCK_VAL_MASK		(1 << 2)
# define GPIO_CLOCK_VAL_OUT		(1 << 3)
# define GPIO_CLOCK_VAL_IN		(1 << 4)
# define GPIO_CLOCK_PULLUP_DISABLE	(1 << 5)
# define GPIO_DATA_DIR_MASK		(1 << 8)
# define GPIO_DATA_DIR_IN		(0 << 9)
# define GPIO_DATA_DIR_OUT		(1 << 9)
# define GPIO_DATA_VAL_MASK		(1 << 10)
# define GPIO_DATA_VAL_OUT		(1 << 11)
# define GPIO_DATA_VAL_IN		(1 << 12)
# define GPIO_DATA_PULLUP_DISABLE	(1 << 13)

/*
 * Clock control & power management
 */

#define VGA0	0x6000
#define VGA1	0x6004
#define VGA_PD	0x6010
#define   VGA0_PD_P2_DIV_4	(1 << 7)
#define   VGA0_PD_P1_DIV_2	(1 << 5)
#define   VGA0_PD_P1_SHIFT	0
#define   VGA0_PD_P1_MASK	(0x1f << 0)
#define   VGA1_PD_P2_DIV_4	(1 << 15)
#define   VGA1_PD_P1_DIV_2	(1 << 13)
#define   VGA1_PD_P1_SHIFT	8
#define   VGA1_PD_P1_MASK	(0x1f << 8)
#define DPLL_A	0x06014
#define DPLL_B	0x06018
#define   DPLL_VCO_ENABLE		(1 << 31)
#define   DPLL_DVO_HIGH_SPEED		(1 << 30)
#define   DPLL_SYNCLOCK_ENABLE		(1 << 29)
#define   DPLL_VGA_MODE_DIS		(1 << 28)
#define   DPLLB_MODE_DAC_SERIAL		(1 << 26) /* i915 */
#define   DPLLB_MODE_LVDS		(2 << 26) /* i915 */
#define   DPLL_MODE_MASK		(3 << 26)
#define   DPLL_DAC_SERIAL_P2_CLOCK_DIV_10 (0 << 24) /* i915 */
#define   DPLL_DAC_SERIAL_P2_CLOCK_DIV_5 (1 << 24) /* i915 */
#define   DPLLB_LVDS_P2_CLOCK_DIV_14	(0 << 24) /* i915 */
#define   DPLLB_LVDS_P2_CLOCK_DIV_7	(1 << 24) /* i915 */
#define   DPLL_P2_CLOCK_DIV_MASK	0x03000000 /* i915 */
#define   DPLL_FPA01_P1_POST_DIV_MASK	0x00ff0000 /* i915 */

#define I915_FIFO_UNDERRUN_STATUS		(1UL<<31)
#define I915_CRC_ERROR_ENABLE			(1UL<<29)
#define I915_CRC_DONE_ENABLE			(1UL<<28)
#define I915_GMBUS_EVENT_ENABLE			(1UL<<27)
#define I915_VSYNC_INTERRUPT_ENABLE		(1UL<<25)
#define I915_DISPLAY_LINE_COMPARE_ENABLE	(1UL<<24)
#define I915_DPST_EVENT_ENABLE			(1UL<<23)
#define I915_LEGACY_BLC_EVENT_ENABLE		(1UL<<22)
#define I915_ODD_FIELD_INTERRUPT_ENABLE		(1UL<<21)
#define I915_EVEN_FIELD_INTERRUPT_ENABLE	(1UL<<20)
#define I915_START_VBLANK_INTERRUPT_ENABLE	(1UL<<18)	/* 965 or later */
#define I915_VBLANK_INTERRUPT_ENABLE		(1UL<<17)
#define I915_OVERLAY_UPDATED_ENABLE		(1UL<<16)
#define I915_CRC_ERROR_INTERRUPT_STATUS		(1UL<<13)
#define I915_CRC_DONE_INTERRUPT_STATUS		(1UL<<12)
#define I915_GMBUS_INTERRUPT_STATUS		(1UL<<11)
#define I915_VSYNC_INTERRUPT_STATUS		(1UL<<9)
#define I915_DISPLAY_LINE_COMPARE_STATUS	(1UL<<8)
#define I915_DPST_EVENT_STATUS			(1UL<<7)
#define I915_LEGACY_BLC_EVENT_STATUS		(1UL<<6)
#define I915_ODD_FIELD_INTERRUPT_STATUS		(1UL<<5)
#define I915_EVEN_FIELD_INTERRUPT_STATUS	(1UL<<4)
#define I915_START_VBLANK_INTERRUPT_STATUS	(1UL<<2)	/* 965 or later */
#define I915_VBLANK_INTERRUPT_STATUS		(1UL<<1)
#define I915_OVERLAY_UPDATED_STATUS		(1UL<<0)

#define SRX_INDEX		0x3c4
#define SRX_DATA		0x3c5
#define SR01			1
#define SR01_SCREEN_OFF		(1<<5)

#define PPCR			0x61204
#define PPCR_ON			(1<<0)

#define DVOB			0x61140
#define DVOB_ON			(1<<31)
#define DVOC			0x61160
#define DVOC_ON			(1<<31)
#define LVDS			0x61180
#define LVDS_ON			(1<<31)

#define ADPA			0x61100
#define ADPA_DPMS_MASK		(~(3<<10))
#define ADPA_DPMS_ON		(0<<10)
#define ADPA_DPMS_SUSPEND	(1<<10)
#define ADPA_DPMS_STANDBY	(2<<10)
#define ADPA_DPMS_OFF		(3<<10)

#define RING_TAIL		0x00
#define TAIL_ADDR		0x001FFFF8
#define RING_HEAD		0x04
#define HEAD_WRAP_COUNT		0xFFE00000
#define HEAD_WRAP_ONE		0x00200000
#define HEAD_ADDR		0x001FFFFC
#define RING_START		0x08
#define START_ADDR		0xFFFFF000
#define RING_LEN		0x0C
#define RING_NR_PAGES		0x001FF000
#define RING_REPORT_MASK	0x00000006
#define RING_REPORT_64K		0x00000002
#define RING_REPORT_128K	0x00000004
#define RING_NO_REPORT		0x00000000
#define RING_VALID_MASK		0x00000001
#define RING_VALID		0x00000001
#define RING_INVALID		0x00000000

/* Scratch pad debug 0 reg:
 */
#define   DPLL_FPA01_P1_POST_DIV_MASK_I830	0x001f0000
/*
 * The i830 generation, in LVDS mode, defines P1 as the bit number set within
 * this field (only one bit may be set).
 */
#define   DPLL_FPA01_P1_POST_DIV_MASK_I830_LVDS	0x003f0000
#define   DPLL_FPA01_P1_POST_DIV_SHIFT	16
#define   DPLL_FPA01_P1_POST_DIV_SHIFT_IGD 15
/* i830, required in DVO non-gang */
#define   PLL_P2_DIVIDE_BY_4		(1 << 23)
#define   PLL_P1_DIVIDE_BY_TWO		(1 << 21) /* i830 */
#define   PLL_REF_INPUT_DREFCLK		(0 << 13)
#define   PLL_REF_INPUT_TVCLKINA	(1 << 13) /* i830 */
#define   PLL_REF_INPUT_TVCLKINBC	(2 << 13) /* SDVO TVCLKIN */
#define   PLLB_REF_INPUT_SPREADSPECTRUMIN (3 << 13)
#define   PLL_REF_INPUT_MASK		(3 << 13)
#define   PLL_LOAD_PULSE_PHASE_SHIFT		9
/* IGDNG */
# define PLL_REF_SDVO_HDMI_MULTIPLIER_SHIFT     9
# define PLL_REF_SDVO_HDMI_MULTIPLIER_MASK      (7 << 9)
# define PLL_REF_SDVO_HDMI_MULTIPLIER(x)	(((x)-1) << 9)
# define DPLL_FPA1_P1_POST_DIV_SHIFT            0
# define DPLL_FPA1_P1_POST_DIV_MASK             0xff

/*
 * Parallel to Serial Load Pulse phase selection.
 * Selects the phase for the 10X DPLL clock for the PCIe
 * digital display port. The range is 4 to 13; 10 or more
 * is just a flip delay. The default is 6
 */
#define   PLL_LOAD_PULSE_PHASE_MASK		(0xf << PLL_LOAD_PULSE_PHASE_SHIFT)
#define   DISPLAY_RATE_SELECT_FPA1		(1 << 8)
/*
 * SDVO multiplier for 945G/GM. Not used on 965.
 */
#define   SDVO_MULTIPLIER_MASK			0x000000ff
#define   SDVO_MULTIPLIER_SHIFT_HIRES		4
#define   SDVO_MULTIPLIER_SHIFT_VGA		0
#define DPLL_A_MD 0x0601c /* 965+ only */
/*
 * UDI pixel divider, controlling how many pixels are stuffed into a packet.
 *
 * Value is pixels minus 1.  Must be set to 1 pixel for SDVO.
 */
#define   DPLL_MD_UDI_DIVIDER_MASK		0x3f000000
#define   DPLL_MD_UDI_DIVIDER_SHIFT		24
/* UDI pixel divider for VGA, same as DPLL_MD_UDI_DIVIDER_MASK. */
#define   DPLL_MD_VGA_UDI_DIVIDER_MASK		0x003f0000
#define   DPLL_MD_VGA_UDI_DIVIDER_SHIFT		16
/*
 * SDVO/UDI pixel multiplier.
 *
 * SDVO requires that the bus clock rate be between 1 and 2 GHz, and the bus
 * clock rate is 10 times the DPLL clock.  At low resolution/refresh rate
 * modes, the bus rate would be below the limits, so SDVO allows for stuffing
 * dummy bytes in the datastream at an increased clock rate, with both sides of
 * the link knowing how many bytes are fill.
 *
 * So, for a mode with a dotclock of 65MHz, we would want to double the clock
 * rate to 130MHz to get a bus rate of 1.30GHz.  The DPLL clock rate would be
 * set to 130MHz, and the SDVO multiplier set to 2x in this register and
 * through an SDVO command.
 *
 * This register field has values of multiplication factor minus 1, with
 * a maximum multiplier of 5 for SDVO.
 */
#define   DPLL_MD_UDI_MULTIPLIER_MASK		0x00003f00
#define   DPLL_MD_UDI_MULTIPLIER_SHIFT		8
/*
 * SDVO/UDI pixel multiplier for VGA, same as DPLL_MD_UDI_MULTIPLIER_MASK.
 * This best be set to the default value (3) or the CRT won't work. No,
 * I don't entirely understand what this does...
 */
#define   DPLL_MD_VGA_UDI_MULTIPLIER_MASK	0x0000003f
#define   DPLL_MD_VGA_UDI_MULTIPLIER_SHIFT	0
#define DPLL_B_MD 0x06020 /* 965+ only */
#define FPA0	0x06040
#define FPA1	0x06044
#define FPB0	0x06048
#define FPB1	0x0604c
#define   FP_N_DIV_MASK		0x003f0000
#define   FP_N_IGD_DIV_MASK	0x00ff0000
#define   FP_N_DIV_SHIFT		16
#define   FP_M1_DIV_MASK	0x00003f00
#define   FP_M1_DIV_SHIFT		 8
#define   FP_M2_DIV_MASK	0x0000003f
#define   FP_M2_IGD_DIV_MASK	0x000000ff
#define   FP_M2_DIV_SHIFT		 0
#define DPLL_TEST	0x606c
#define   DPLLB_TEST_SDVO_DIV_1		(0 << 22)
#define   DPLLB_TEST_SDVO_DIV_2		(1 << 22)
#define   DPLLB_TEST_SDVO_DIV_4		(2 << 22)
#define   DPLLB_TEST_SDVO_DIV_MASK	(3 << 22)
#define   DPLLB_TEST_N_BYPASS		(1 << 19)
#define   DPLLB_TEST_M_BYPASS		(1 << 18)
#define   DPLLB_INPUT_BUFFER_ENABLE	(1 << 16)
#define   DPLLA_TEST_N_BYPASS		(1 << 3)
#define   DPLLA_TEST_M_BYPASS		(1 << 2)
#define   DPLLA_INPUT_BUFFER_ENABLE	(1 << 0)
#define D_STATE		0x6104
#define  DSTATE_PLL_D3_OFF			(1<<3)
#define  DSTATE_GFX_CLOCK_GATING		(1<<1)
#define  DSTATE_DOT_CLOCK_GATING		(1<<0)
#define DSPCLK_GATE_D		0x6200
# define DPUNIT_B_CLOCK_GATE_DISABLE		(1 << 30) /* 965 */
# define VSUNIT_CLOCK_GATE_DISABLE		(1 << 29) /* 965 */
# define VRHUNIT_CLOCK_GATE_DISABLE		(1 << 28) /* 965 */
# define VRDUNIT_CLOCK_GATE_DISABLE		(1 << 27) /* 965 */
# define AUDUNIT_CLOCK_GATE_DISABLE		(1 << 26) /* 965 */
# define DPUNIT_A_CLOCK_GATE_DISABLE		(1 << 25) /* 965 */
# define DPCUNIT_CLOCK_GATE_DISABLE		(1 << 24) /* 965 */
# define TVRUNIT_CLOCK_GATE_DISABLE		(1 << 23) /* 915-945 */
# define TVCUNIT_CLOCK_GATE_DISABLE		(1 << 22) /* 915-945 */
# define TVFUNIT_CLOCK_GATE_DISABLE		(1 << 21) /* 915-945 */
# define TVEUNIT_CLOCK_GATE_DISABLE		(1 << 20) /* 915-945 */
# define DVSUNIT_CLOCK_GATE_DISABLE		(1 << 19) /* 915-945 */
# define DSSUNIT_CLOCK_GATE_DISABLE		(1 << 18) /* 915-945 */
# define DDBUNIT_CLOCK_GATE_DISABLE		(1 << 17) /* 915-945 */
# define DPRUNIT_CLOCK_GATE_DISABLE		(1 << 16) /* 915-945 */
# define DPFUNIT_CLOCK_GATE_DISABLE		(1 << 15) /* 915-945 */
# define DPBMUNIT_CLOCK_GATE_DISABLE		(1 << 14) /* 915-945 */
# define DPLSUNIT_CLOCK_GATE_DISABLE		(1 << 13) /* 915-945 */
# define DPLUNIT_CLOCK_GATE_DISABLE		(1 << 12) /* 915-945 */
# define DPOUNIT_CLOCK_GATE_DISABLE		(1 << 11)
# define DPBUNIT_CLOCK_GATE_DISABLE		(1 << 10)
# define DCUNIT_CLOCK_GATE_DISABLE		(1 << 9)
# define DPUNIT_CLOCK_GATE_DISABLE		(1 << 8)
# define VRUNIT_CLOCK_GATE_DISABLE		(1 << 7) /* 915+: reserved */
# define OVHUNIT_CLOCK_GATE_DISABLE		(1 << 6) /* 830-865 */
# define DPIOUNIT_CLOCK_GATE_DISABLE		(1 << 6) /* 915-945 */
# define OVFUNIT_CLOCK_GATE_DISABLE		(1 << 5)
# define OVBUNIT_CLOCK_GATE_DISABLE		(1 << 4)
/**
 * This bit must be set on the 830 to prevent hangs when turning off the
 * overlay scaler.
 */
# define OVRUNIT_CLOCK_GATE_DISABLE		(1 << 3)
# define OVCUNIT_CLOCK_GATE_DISABLE		(1 << 2)
# define OVUUNIT_CLOCK_GATE_DISABLE		(1 << 1)
# define ZVUNIT_CLOCK_GATE_DISABLE		(1 << 0) /* 830 */
# define OVLUNIT_CLOCK_GATE_DISABLE		(1 << 0) /* 845,865 */

#define RENCLK_GATE_D1		0x6204
# define BLITTER_CLOCK_GATE_DISABLE		(1 << 13) /* 945GM only */
# define MPEG_CLOCK_GATE_DISABLE		(1 << 12) /* 945GM only */
# define PC_FE_CLOCK_GATE_DISABLE		(1 << 11)
# define PC_BE_CLOCK_GATE_DISABLE		(1 << 10)
# define WINDOWER_CLOCK_GATE_DISABLE		(1 << 9)
# define INTERPOLATOR_CLOCK_GATE_DISABLE	(1 << 8)
# define COLOR_CALCULATOR_CLOCK_GATE_DISABLE	(1 << 7)
# define MOTION_COMP_CLOCK_GATE_DISABLE		(1 << 6)
# define MAG_CLOCK_GATE_DISABLE			(1 << 5)
/** This bit must be unset on 855,865 */
# define MECI_CLOCK_GATE_DISABLE		(1 << 4)
# define DCMP_CLOCK_GATE_DISABLE		(1 << 3)
# define MEC_CLOCK_GATE_DISABLE			(1 << 2)
# define MECO_CLOCK_GATE_DISABLE		(1 << 1)
/** This bit must be set on 855,865. */
# define SV_CLOCK_GATE_DISABLE			(1 << 0)
# define I915_MPEG_CLOCK_GATE_DISABLE		(1 << 16)
# define I915_VLD_IP_PR_CLOCK_GATE_DISABLE	(1 << 15)
# define I915_MOTION_COMP_CLOCK_GATE_DISABLE	(1 << 14)
# define I915_BD_BF_CLOCK_GATE_DISABLE		(1 << 13)
# define I915_SF_SE_CLOCK_GATE_DISABLE		(1 << 12)
# define I915_WM_CLOCK_GATE_DISABLE		(1 << 11)
# define I915_IZ_CLOCK_GATE_DISABLE		(1 << 10)
# define I915_PI_CLOCK_GATE_DISABLE		(1 << 9)
# define I915_DI_CLOCK_GATE_DISABLE		(1 << 8)
# define I915_SH_SV_CLOCK_GATE_DISABLE		(1 << 7)
# define I915_PL_DG_QC_FT_CLOCK_GATE_DISABLE	(1 << 6)
# define I915_SC_CLOCK_GATE_DISABLE		(1 << 5)
# define I915_FL_CLOCK_GATE_DISABLE		(1 << 4)
# define I915_DM_CLOCK_GATE_DISABLE		(1 << 3)
# define I915_PS_CLOCK_GATE_DISABLE		(1 << 2)
# define I915_CC_CLOCK_GATE_DISABLE		(1 << 1)
# define I915_BY_CLOCK_GATE_DISABLE		(1 << 0)

# define I965_RCZ_CLOCK_GATE_DISABLE		(1 << 30)
/** This bit must always be set on 965G/965GM */
# define I965_RCC_CLOCK_GATE_DISABLE		(1 << 29)
# define I965_RCPB_CLOCK_GATE_DISABLE		(1 << 28)
# define I965_DAP_CLOCK_GATE_DISABLE		(1 << 27)
# define I965_ROC_CLOCK_GATE_DISABLE		(1 << 26)
# define I965_GW_CLOCK_GATE_DISABLE		(1 << 25)
# define I965_TD_CLOCK_GATE_DISABLE		(1 << 24)
/** This bit must always be set on 965G */
# define I965_ISC_CLOCK_GATE_DISABLE		(1 << 23)
# define I965_IC_CLOCK_GATE_DISABLE		(1 << 22)
# define I965_EU_CLOCK_GATE_DISABLE		(1 << 21)
# define I965_IF_CLOCK_GATE_DISABLE		(1 << 20)
# define I965_TC_CLOCK_GATE_DISABLE		(1 << 19)
# define I965_SO_CLOCK_GATE_DISABLE		(1 << 17)
# define I965_FBC_CLOCK_GATE_DISABLE		(1 << 16)
# define I965_MARI_CLOCK_GATE_DISABLE		(1 << 15)
# define I965_MASF_CLOCK_GATE_DISABLE		(1 << 14)
# define I965_MAWB_CLOCK_GATE_DISABLE		(1 << 13)
# define I965_EM_CLOCK_GATE_DISABLE		(1 << 12)
# define I965_UC_CLOCK_GATE_DISABLE		(1 << 11)
# define I965_SI_CLOCK_GATE_DISABLE		(1 << 6)
# define I965_MT_CLOCK_GATE_DISABLE		(1 << 5)
# define I965_PL_CLOCK_GATE_DISABLE		(1 << 4)
# define I965_DG_CLOCK_GATE_DISABLE		(1 << 3)
# define I965_QC_CLOCK_GATE_DISABLE		(1 << 2)
# define I965_FT_CLOCK_GATE_DISABLE		(1 << 1)
# define I965_DM_CLOCK_GATE_DISABLE		(1 << 0)

#define RENCLK_GATE_D2		0x6208
#define VF_UNIT_CLOCK_GATE_DISABLE		(1 << 9)
#define GS_UNIT_CLOCK_GATE_DISABLE		(1 << 7)
#define CL_UNIT_CLOCK_GATE_DISABLE		(1 << 6)
#define RAMCLK_GATE_D		0x6210		/* CRL only */
#define DEUC			0x6214          /* CRL only */

/*
 * Palette regs
 */

#define PALETTE_A		0x0a000
#define PALETTE_B		0x0a800

/* MCH MMIO space */

/*
 * MCHBAR mirror.
 *
 * This mirrors the MCHBAR MMIO space whose location is determined by
 * device 0 function 0's pci config register 0x44 or 0x48 and matches it in
 * every way.  It is not accessible from the CP register read instructions.
 *
 */
#define MCHBAR_MIRROR_BASE	0x10000

/** 915-945 and GM965 MCH register controlling DRAM channel access */
#define DCC			0x10200
#define DCC_ADDRESSING_MODE_SINGLE_CHANNEL		(0 << 0)
#define DCC_ADDRESSING_MODE_DUAL_CHANNEL_ASYMMETRIC	(1 << 0)
#define DCC_ADDRESSING_MODE_DUAL_CHANNEL_INTERLEAVED	(2 << 0)
#define DCC_ADDRESSING_MODE_MASK			(3 << 0)
#define DCC_CHANNEL_XOR_DISABLE				(1 << 10)
#define DCC_CHANNEL_XOR_BIT_17				(1 << 9)

/** 965 MCH register controlling DRAM channel configuration */
#define C0DRB3			0x10206
#define C1DRB3			0x10606

/* Clocking configuration register */
#define CLKCFG			0x10c00
#define CLKCFG_FSB_400					(5 << 0)	/* hrawclk 100 */
#define CLKCFG_FSB_533					(1 << 0)	/* hrawclk 133 */
#define CLKCFG_FSB_667					(3 << 0)	/* hrawclk 166 */
#define CLKCFG_FSB_800					(2 << 0)	/* hrawclk 200 */
#define CLKCFG_FSB_1067					(6 << 0)	/* hrawclk 266 */
#define CLKCFG_FSB_1333					(7 << 0)	/* hrawclk 333 */
/* Note, below two are guess */
#define CLKCFG_FSB_1600					(4 << 0)	/* hrawclk 400 */
#define CLKCFG_FSB_1600_ALT				(0 << 0)	/* hrawclk 400 */
#define CLKCFG_FSB_MASK					(7 << 0)
#define CLKCFG_MEM_533					(1 << 4)
#define CLKCFG_MEM_667					(2 << 4)
#define CLKCFG_MEM_800					(3 << 4)
#define CLKCFG_MEM_MASK					(7 << 4)

/** GM965 GM45 render standby register */
#define MCHBAR_RENDER_STANDBY	0x111B8

#define PEG_BAND_GAP_DATA	0x14d68

/*
 * Overlay regs
 */

#define OVADD			0x30000
#define DOVSTA			0x30008
#define OC_BUF			(0x3<<20)
#define OGAMC5			0x30010
#define OGAMC4			0x30014
#define OGAMC3			0x30018
#define OGAMC2			0x3001c
#define OGAMC1			0x30020
#define OGAMC0			0x30024

/*
 * Display engine regs
 */

/* Pipe A timing regs */
#define HTOTAL_A	0x60000
#define HBLANK_A	0x60004
#define HSYNC_A		0x60008
#define VTOTAL_A	0x6000c
#define VBLANK_A	0x60010
#define VSYNC_A		0x60014
#define PIPEASRC	0x6001c
#define BCLRPAT_A	0x60020

/* Pipe B timing regs */
#define HTOTAL_B	0x61000
#define HBLANK_B	0x61004
#define HSYNC_B		0x61008
#define VTOTAL_B	0x6100c
#define VBLANK_B	0x61010
#define VSYNC_B		0x61014
#define PIPEBSRC	0x6101c
#define BCLRPAT_B	0x61020

/* VGA port control */
#define ADPA			0x61100
#define   ADPA_DAC_ENABLE	(1<<31)
#define   ADPA_DAC_DISABLE	0
#define   ADPA_PIPE_SELECT_MASK	(1<<30)
#define   ADPA_PIPE_A_SELECT	0
#define   ADPA_PIPE_B_SELECT	(1<<30)
#define   ADPA_USE_VGA_HVPOLARITY (1<<15)
#define   ADPA_SETS_HVPOLARITY	0
#define   ADPA_VSYNC_CNTL_DISABLE (1<<11)
#define   ADPA_VSYNC_CNTL_ENABLE 0
#define   ADPA_HSYNC_CNTL_DISABLE (1<<10)
#define   ADPA_HSYNC_CNTL_ENABLE 0
#define   ADPA_VSYNC_ACTIVE_HIGH (1<<4)
#define   ADPA_VSYNC_ACTIVE_LOW	0
#define   ADPA_HSYNC_ACTIVE_HIGH (1<<3)
#define   ADPA_HSYNC_ACTIVE_LOW	0
#define   ADPA_DPMS_MASK	(~(3<<10))
#define   ADPA_DPMS_ON		(0<<10)
#define   ADPA_DPMS_SUSPEND	(1<<10)
#define   ADPA_DPMS_STANDBY	(2<<10)
#define   ADPA_DPMS_OFF		(3<<10)

/* Hotplug control (945+ only) */
#define PORT_HOTPLUG_EN		0x61110
#define   SDVOB_HOTPLUG_INT_EN			(1 << 26)
#define   SDVOC_HOTPLUG_INT_EN			(1 << 25)
#define   TV_HOTPLUG_INT_EN			(1 << 18)
#define   CRT_HOTPLUG_INT_EN			(1 << 9)
#define   CRT_HOTPLUG_FORCE_DETECT		(1 << 3)

#define PORT_HOTPLUG_STAT	0x61114
#define   CRT_HOTPLUG_INT_STATUS		(1 << 11)
#define   TV_HOTPLUG_INT_STATUS			(1 << 10)
#define   CRT_HOTPLUG_MONITOR_MASK		(3 << 8)
#define   CRT_HOTPLUG_MONITOR_COLOR		(3 << 8)
#define   CRT_HOTPLUG_MONITOR_MONO		(2 << 8)
#define   CRT_HOTPLUG_MONITOR_NONE		(0 << 8)
#define   SDVOC_HOTPLUG_INT_STATUS		(1 << 7)
#define   SDVOB_HOTPLUG_INT_STATUS		(1 << 6)

/* SDVO port control */
#define SDVOB			0x61140
#define SDVOC			0x61160
#define   SDVO_ENABLE		(1 << 31)
#define   SDVO_PIPE_B_SELECT	(1 << 30)
#define   SDVO_STALL_SELECT	(1 << 29)
#define   SDVO_INTERRUPT_ENABLE	(1 << 26)
/**
 * 915G/GM SDVO pixel multiplier.
 *
 * Programmed value is multiplier - 1, up to 5x.
 *
 * \sa DPLL_MD_UDI_MULTIPLIER_MASK
 */
#define   SDVO_PORT_MULTIPLY_MASK	(7 << 23)
#define   SDVO_PORT_MULTIPLY_SHIFT		23
#define   SDVO_PHASE_SELECT_MASK	(15 << 19)
#define   SDVO_PHASE_SELECT_DEFAULT	(6 << 19)
#define   SDVO_CLOCK_OUTPUT_INVERT	(1 << 18)
#define   SDVOC_GANG_MODE		(1 << 16)
#define   SDVO_BORDER_ENABLE		(1 << 7)
#define   SDVOB_PCIE_CONCURRENCY	(1 << 3)
#define   SDVO_DETECTED			(1 << 2)
/* Bits to be preserved when writing */
#define   SDVOB_PRESERVE_MASK ((1 << 17) | (1 << 16) | (1 << 14) | (1 << 26))
#define   SDVOC_PRESERVE_MASK ((1 << 17) | (1 << 26))

/* DVO port control */
#define DVOA			0x61120
#define DVOB			0x61140
#define DVOC			0x61160
#define   DVO_ENABLE			(1 << 31)
#define   DVO_PIPE_B_SELECT		(1 << 30)
#define   DVO_PIPE_STALL_UNUSED		(0 << 28)
#define   DVO_PIPE_STALL		(1 << 28)
#define   DVO_PIPE_STALL_TV		(2 << 28)
#define   DVO_PIPE_STALL_MASK		(3 << 28)
#define   DVO_USE_VGA_SYNC		(1 << 15)
#define   DVO_DATA_ORDER_I740		(0 << 14)
#define   DVO_DATA_ORDER_FP		(1 << 14)
#define   DVO_VSYNC_DISABLE		(1 << 11)
#define   DVO_HSYNC_DISABLE		(1 << 10)
#define   DVO_VSYNC_TRISTATE		(1 << 9)
#define   DVO_HSYNC_TRISTATE		(1 << 8)
#define   DVO_BORDER_ENABLE		(1 << 7)
#define   DVO_DATA_ORDER_GBRG		(1 << 6)
#define   DVO_DATA_ORDER_RGGB		(0 << 6)
#define   DVO_DATA_ORDER_GBRG_ERRATA	(0 << 6)
#define   DVO_DATA_ORDER_RGGB_ERRATA	(1 << 6)
#define   DVO_VSYNC_ACTIVE_HIGH		(1 << 4)
#define   DVO_HSYNC_ACTIVE_HIGH		(1 << 3)
#define   DVO_BLANK_ACTIVE_HIGH		(1 << 2)
#define   DVO_OUTPUT_CSTATE_PIXELS	(1 << 1)	/* SDG only */
#define   DVO_OUTPUT_SOURCE_SIZE_PIXELS	(1 << 0)	/* SDG only */
#define   DVO_PRESERVE_MASK		(0x7<<24)
#define DVOA_SRCDIM		0x61124
#define DVOB_SRCDIM		0x61144
#define DVOC_SRCDIM		0x61164
#define   DVO_SRCDIM_HORIZONTAL_SHIFT	12
#define   DVO_SRCDIM_VERTICAL_SHIFT	0

/* LVDS port control */
#define LVDS			0x61180
/*
 * Enables the LVDS port.  This bit must be set before DPLLs are enabled, as
 * the DPLL semantics change when the LVDS is assigned to that pipe.
 */
#define   LVDS_PORT_EN			(1 << 31)
/* Selects pipe B for LVDS data.  Must be set on pre-965. */
#define   LVDS_PIPEB_SELECT		(1 << 30)
/*
 * Enables the A0-A2 data pairs and CLKA, containing 18 bits of color data per
 * pixel.
 */
#define   LVDS_A0A2_CLKA_POWER_MASK	(3 << 8)
#define   LVDS_A0A2_CLKA_POWER_DOWN	(0 << 8)
#define   LVDS_A0A2_CLKA_POWER_UP	(3 << 8)
/*
 * Controls the A3 data pair, which contains the additional LSBs for 24 bit
 * mode.  Only enabled if LVDS_A0A2_CLKA_POWER_UP also indicates it should be
 * on.
 */
#define   LVDS_A3_POWER_MASK		(3 << 6)
#define   LVDS_A3_POWER_DOWN		(0 << 6)
#define   LVDS_A3_POWER_UP		(3 << 6)
/*
 * Controls the CLKB pair.  This should only be set when LVDS_B0B3_POWER_UP
 * is set.
 */
#define   LVDS_CLKB_POWER_MASK		(3 << 4)
#define   LVDS_CLKB_POWER_DOWN		(0 << 4)
#define   LVDS_CLKB_POWER_UP		(3 << 4)
/*
 * Controls the B0-B3 data pairs.  This must be set to match the DPLL p2
 * setting for whether we are in dual-channel mode.  The B3 pair will
 * additionally only be powered up when LVDS_A3_POWER_UP is set.
 */
#define   LVDS_B0B3_POWER_MASK		(3 << 2)
#define   LVDS_B0B3_POWER_DOWN		(0 << 2)
#define   LVDS_B0B3_POWER_UP		(3 << 2)

/* Panel power sequencing */
#define PP_STATUS	0x61200
#define   PP_ON		(1 << 31)
/*
 * Indicates that all dependencies of the panel are on:
 *
 * - PLL enabled
 * - pipe enabled
 * - LVDS/DVOB/DVOC on
 */
#define   PP_READY		(1 << 30)
#define   PP_SEQUENCE_NONE	(0 << 28)
#define   PP_SEQUENCE_ON	(1 << 28)
#define   PP_SEQUENCE_OFF	(2 << 28)
#define   PP_SEQUENCE_MASK	0x30000000
#define PP_CONTROL	0x61204
#define   POWER_TARGET_ON	(1 << 0)
#define PP_ON_DELAYS	0x61208
#define PP_OFF_DELAYS	0x6120c
#define PP_DIVISOR	0x61210

/* Panel fitting */
#define PFIT_CONTROL	0x61230
#define   PFIT_ENABLE		(1 << 31)
#define   PFIT_PIPE_MASK	(3 << 29)
#define   PFIT_PIPE_SHIFT	29
#define   VERT_INTERP_DISABLE	(0 << 10)
#define   VERT_INTERP_BILINEAR	(1 << 10)
#define   VERT_INTERP_MASK	(3 << 10)
#define   VERT_AUTO_SCALE	(1 << 9)
#define   HORIZ_INTERP_DISABLE	(0 << 6)
#define   HORIZ_INTERP_BILINEAR	(1 << 6)
#define   HORIZ_INTERP_MASK	(3 << 6)
#define   HORIZ_AUTO_SCALE	(1 << 5)
#define   PANEL_8TO6_DITHER_ENABLE (1 << 3)
#define PFIT_PGM_RATIOS	0x61234
#define   PFIT_VERT_SCALE_MASK			0xfff00000
#define   PFIT_HORIZ_SCALE_MASK			0x0000fff0
#define PFIT_AUTO_RATIOS 0x61238

/* Backlight control */
#define BLC_PWM_CTL		0x61254
#define   BACKLIGHT_MODULATION_FREQ_SHIFT		(17)
#define BLC_PWM_CTL2		0x61250 /* 965+ only */
/*
 * This is the most significant 15 bits of the number of backlight cycles in a
 * complete cycle of the modulated backlight control.
 *
 * The actual value is this field multiplied by two.
 */
#define   BACKLIGHT_MODULATION_FREQ_MASK		(0x7fff << 17)
#define   BLM_LEGACY_MODE				(1 << 16)
/*
 * This is the number of cycles out of the backlight modulation cycle for which
 * the backlight is on.
 *
 * This field must be no greater than the number of cycles in the complete
 * backlight modulation cycle.
 */
#define   BACKLIGHT_DUTY_CYCLE_SHIFT		(0)
#define   BACKLIGHT_DUTY_CYCLE_MASK		(0xffff)

#define BLC_HIST_CTL		0x61260

/* TV port control */
#define TV_CTL			0x68000
/** Enables the TV encoder */
# define TV_ENC_ENABLE			(1 << 31)
/** Sources the TV encoder input from pipe B instead of A. */
# define TV_ENC_PIPEB_SELECT		(1 << 30)
/** Outputs composite video (DAC A only) */
# define TV_ENC_OUTPUT_COMPOSITE	(0 << 28)
/** Outputs SVideo video (DAC B/C) */
# define TV_ENC_OUTPUT_SVIDEO		(1 << 28)
/** Outputs Component video (DAC A/B/C) */
# define TV_ENC_OUTPUT_COMPONENT	(2 << 28)
/** Outputs Composite and SVideo (DAC A/B/C) */
# define TV_ENC_OUTPUT_SVIDEO_COMPOSITE	(3 << 28)
# define TV_TRILEVEL_SYNC		(1 << 21)
/** Enables slow sync generation (945GM only) */
# define TV_SLOW_SYNC			(1 << 20)
/** Selects 4x oversampling for 480i and 576p */
# define TV_OVERSAMPLE_4X		(0 << 18)
/** Selects 2x oversampling for 720p and 1080i */
# define TV_OVERSAMPLE_2X		(1 << 18)
/** Selects no oversampling for 1080p */
# define TV_OVERSAMPLE_NONE		(2 << 18)
/** Selects 8x oversampling */
# define TV_OVERSAMPLE_8X		(3 << 18)
/** Selects progressive mode rather than interlaced */
# define TV_PROGRESSIVE			(1 << 17)
/** Sets the colorburst to PAL mode.  Required for non-M PAL modes. */
# define TV_PAL_BURST			(1 << 16)
/** Field for setting delay of Y compared to C */
# define TV_YC_SKEW_MASK		(7 << 12)
/** Enables a fix for 480p/576p standard definition modes on the 915GM only */
# define TV_ENC_SDP_FIX			(1 << 11)
/**
 * Enables a fix for the 915GM only.
 *
 * Not sure what it does.
 */
# define TV_ENC_C0_FIX			(1 << 10)
/** Bits that must be preserved by software */
# define TV_CTL_SAVE			((3 << 8) | (3 << 6))
# define TV_FUSE_STATE_MASK		(3 << 4)
/** Read-only state that reports all features enabled */
# define TV_FUSE_STATE_ENABLED		(0 << 4)
/** Read-only state that reports that Macrovision is disabled in hardware*/
# define TV_FUSE_STATE_NO_MACROVISION	(1 << 4)
/** Read-only state that reports that TV-out is disabled in hardware. */
# define TV_FUSE_STATE_DISABLED		(2 << 4)
/** Normal operation */
# define TV_TEST_MODE_NORMAL		(0 << 0)
/** Encoder test pattern 1 - combo pattern */
# define TV_TEST_MODE_PATTERN_1		(1 << 0)
/** Encoder test pattern 2 - full screen vertical 75% color bars */
# define TV_TEST_MODE_PATTERN_2		(2 << 0)
/** Encoder test pattern 3 - full screen horizontal 75% color bars */
# define TV_TEST_MODE_PATTERN_3		(3 << 0)
/** Encoder test pattern 4 - random noise */
# define TV_TEST_MODE_PATTERN_4		(4 << 0)
/** Encoder test pattern 5 - linear color ramps */
# define TV_TEST_MODE_PATTERN_5		(5 << 0)
/**
 * This test mode forces the DACs to 50% of full output.
 *
 * This is used for load detection in combination with TVDAC_SENSE_MASK
 */
# define TV_TEST_MODE_MONITOR_DETECT	(7 << 0)
# define TV_TEST_MODE_MASK		(7 << 0)

#define TV_DAC			0x68004
/**
 * Reports that DAC state change logic has reported change (RO).
 *
 * This gets cleared when TV_DAC_STATE_EN is cleared
*/
# define TVDAC_STATE_CHG		(1 << 31)
# define TVDAC_SENSE_MASK		(7 << 28)
/** Reports that DAC A voltage is above the detect threshold */
# define TVDAC_A_SENSE			(1 << 30)
/** Reports that DAC B voltage is above the detect threshold */
# define TVDAC_B_SENSE			(1 << 29)
/** Reports that DAC C voltage is above the detect threshold */
# define TVDAC_C_SENSE			(1 << 28)
/**
 * Enables DAC state detection logic, for load-based TV detection.
 *
 * The PLL of the chosen pipe (in TV_CTL) must be running, and the encoder set
 * to off, for load detection to work.
 */
# define TVDAC_STATE_CHG_EN		(1 << 27)
/** Sets the DAC A sense value to high */
# define TVDAC_A_SENSE_CTL		(1 << 26)
/** Sets the DAC B sense value to high */
# define TVDAC_B_SENSE_CTL		(1 << 25)
/** Sets the DAC C sense value to high */
# define TVDAC_C_SENSE_CTL		(1 << 24)
/** Overrides the ENC_ENABLE and DAC voltage levels */
# define DAC_CTL_OVERRIDE		(1 << 7)
/** Sets the slew rate.  Must be preserved in software */
# define ENC_TVDAC_SLEW_FAST		(1 << 6)
# define DAC_A_1_3_V			(0 << 4)
# define DAC_A_1_1_V			(1 << 4)
# define DAC_A_0_7_V			(2 << 4)
# define DAC_A_OFF			(3 << 4)
# define DAC_B_1_3_V			(0 << 2)
# define DAC_B_1_1_V			(1 << 2)
# define DAC_B_0_7_V			(2 << 2)
# define DAC_B_OFF			(3 << 2)
# define DAC_C_1_3_V			(0 << 0)
# define DAC_C_1_1_V			(1 << 0)
# define DAC_C_0_7_V			(2 << 0)
# define DAC_C_OFF			(3 << 0)

/**
 * CSC coefficients are stored in a floating point format with 9 bits of
 * mantissa and 2 or 3 bits of exponent.  The exponent is represented as 2**-n,
 * where 2-bit exponents are unsigned n, and 3-bit exponents are signed n with
 * -1 (0x3) being the only legal negative value.
 */
#define TV_CSC_Y		0x68010
# define TV_RY_MASK			0x07ff0000
# define TV_RY_SHIFT			16
# define TV_GY_MASK			0x00000fff
# define TV_GY_SHIFT			0

#define TV_CSC_Y2		0x68014
# define TV_BY_MASK			0x07ff0000
# define TV_BY_SHIFT			16
/**
 * Y attenuation for component video.
 *
 * Stored in 1.9 fixed point.
 */
# define TV_AY_MASK			0x000003ff
# define TV_AY_SHIFT			0

#define TV_CSC_U		0x68018
# define TV_RU_MASK			0x07ff0000
# define TV_RU_SHIFT			16
# define TV_GU_MASK			0x000007ff
# define TV_GU_SHIFT			0

#define TV_CSC_U2		0x6801c
# define TV_BU_MASK			0x07ff0000
# define TV_BU_SHIFT			16
/**
 * U attenuation for component video.
 *
 * Stored in 1.9 fixed point.
 */
# define TV_AU_MASK			0x000003ff
# define TV_AU_SHIFT			0

#define TV_CSC_V		0x68020
# define TV_RV_MASK			0x0fff0000
# define TV_RV_SHIFT			16
# define TV_GV_MASK			0x000007ff
# define TV_GV_SHIFT			0

#define TV_CSC_V2		0x68024
# define TV_BV_MASK			0x07ff0000
# define TV_BV_SHIFT			16
/**
 * V attenuation for component video.
 *
 * Stored in 1.9 fixed point.
 */
# define TV_AV_MASK			0x000007ff
# define TV_AV_SHIFT			0

#define TV_CLR_KNOBS		0x68028
/** 2s-complement brightness adjustment */
# define TV_BRIGHTNESS_MASK		0xff000000
# define TV_BRIGHTNESS_SHIFT		24
/** Contrast adjustment, as a 2.6 unsigned floating point number */
# define TV_CONTRAST_MASK		0x00ff0000
# define TV_CONTRAST_SHIFT		16
/** Saturation adjustment, as a 2.6 unsigned floating point number */
# define TV_SATURATION_MASK		0x0000ff00
# define TV_SATURATION_SHIFT		8
/** Hue adjustment, as an integer phase angle in degrees */
# define TV_HUE_MASK			0x000000ff
# define TV_HUE_SHIFT			0

#define TV_CLR_LEVEL		0x6802c
/** Controls the DAC level for black */
# define TV_BLACK_LEVEL_MASK		0x01ff0000
# define TV_BLACK_LEVEL_SHIFT		16
/** Controls the DAC level for blanking */
# define TV_BLANK_LEVEL_MASK		0x000001ff
# define TV_BLANK_LEVEL_SHIFT		0

#define TV_H_CTL_1		0x68030
/** Number of pixels in the hsync. */
# define TV_HSYNC_END_MASK		0x1fff0000
# define TV_HSYNC_END_SHIFT		16
/** Total number of pixels minus one in the line (display and blanking). */
# define TV_HTOTAL_MASK			0x00001fff
# define TV_HTOTAL_SHIFT		0

#define TV_H_CTL_2		0x68034
/** Enables the colorburst (needed for non-component color) */
# define TV_BURST_ENA			(1 << 31)
/** Offset of the colorburst from the start of hsync, in pixels minus one. */
# define TV_HBURST_START_SHIFT		16
# define TV_HBURST_START_MASK		0x1fff0000
/** Length of the colorburst */
# define TV_HBURST_LEN_SHIFT		0
# define TV_HBURST_LEN_MASK		0x0001fff

#define TV_H_CTL_3		0x68038
/** End of hblank, measured in pixels minus one from start of hsync */
# define TV_HBLANK_END_SHIFT		16
# define TV_HBLANK_END_MASK		0x1fff0000
/** Start of hblank, measured in pixels minus one from start of hsync */
# define TV_HBLANK_START_SHIFT		0
# define TV_HBLANK_START_MASK		0x0001fff

#define TV_V_CTL_1		0x6803c
/** XXX */
# define TV_NBR_END_SHIFT		16
# define TV_NBR_END_MASK		0x07ff0000
/** XXX */
# define TV_VI_END_F1_SHIFT		8
# define TV_VI_END_F1_MASK		0x00003f00
/** XXX */
# define TV_VI_END_F2_SHIFT		0
# define TV_VI_END_F2_MASK		0x0000003f

#define TV_V_CTL_2		0x68040
/** Length of vsync, in half lines */
# define TV_VSYNC_LEN_MASK		0x07ff0000
# define TV_VSYNC_LEN_SHIFT		16
/** Offset of the start of vsync in field 1, measured in one less than the
 * number of half lines.
 */
# define TV_VSYNC_START_F1_MASK		0x00007f00
# define TV_VSYNC_START_F1_SHIFT	8
/**
 * Offset of the start of vsync in field 2, measured in one less than the
 * number of half lines.
 */
# define TV_VSYNC_START_F2_MASK		0x0000007f
# define TV_VSYNC_START_F2_SHIFT	0

#define TV_V_CTL_3		0x68044
/** Enables generation of the equalization signal */
# define TV_EQUAL_ENA			(1 << 31)
/** Length of vsync, in half lines */
# define TV_VEQ_LEN_MASK		0x007f0000
# define TV_VEQ_LEN_SHIFT		16
/** Offset of the start of equalization in field 1, measured in one less than
 * the number of half lines.
 */
# define TV_VEQ_START_F1_MASK		0x0007f00
# define TV_VEQ_START_F1_SHIFT		8
/**
 * Offset of the start of equalization in field 2, measured in one less than
 * the number of half lines.
 */
# define TV_VEQ_START_F2_MASK		0x000007f
# define TV_VEQ_START_F2_SHIFT		0

#define TV_V_CTL_4		0x68048
/**
 * Offset to start of vertical colorburst, measured in one less than the
 * number of lines from vertical start.
 */
# define TV_VBURST_START_F1_MASK	0x003f0000
# define TV_VBURST_START_F1_SHIFT	16
/**
 * Offset to the end of vertical colorburst, measured in one less than the
 * number of lines from the start of NBR.
 */
# define TV_VBURST_END_F1_MASK		0x000000ff
# define TV_VBURST_END_F1_SHIFT		0

#define TV_V_CTL_5		0x6804c
/**
 * Offset to start of vertical colorburst, measured in one less than the
 * number of lines from vertical start.
 */
# define TV_VBURST_START_F2_MASK	0x003f0000
# define TV_VBURST_START_F2_SHIFT	16
/**
 * Offset to the end of vertical colorburst, measured in one less than the
 * number of lines from the start of NBR.
 */
# define TV_VBURST_END_F2_MASK		0x000000ff
# define TV_VBURST_END_F2_SHIFT		0

#define TV_V_CTL_6		0x68050
/**
 * Offset to start of vertical colorburst, measured in one less than the
 * number of lines from vertical start.
 */
# define TV_VBURST_START_F3_MASK	0x003f0000
# define TV_VBURST_START_F3_SHIFT	16
/**
 * Offset to the end of vertical colorburst, measured in one less than the
 * number of lines from the start of NBR.
 */
# define TV_VBURST_END_F3_MASK		0x000000ff
# define TV_VBURST_END_F3_SHIFT		0

#define TV_V_CTL_7		0x68054
/**
 * Offset to start of vertical colorburst, measured in one less than the
 * number of lines from vertical start.
 */
# define TV_VBURST_START_F4_MASK	0x003f0000
# define TV_VBURST_START_F4_SHIFT	16
/**
 * Offset to the end of vertical colorburst, measured in one less than the
 * number of lines from the start of NBR.
 */
# define TV_VBURST_END_F4_MASK		0x000000ff
# define TV_VBURST_END_F4_SHIFT		0

#define TV_SC_CTL_1		0x68060
/** Turns on the first subcarrier phase generation DDA */
# define TV_SC_DDA1_EN			(1 << 31)
/** Turns on the first subcarrier phase generation DDA */
# define TV_SC_DDA2_EN			(1 << 30)
/** Turns on the first subcarrier phase generation DDA */
# define TV_SC_DDA3_EN			(1 << 29)
/** Sets the subcarrier DDA to reset frequency every other field */
# define TV_SC_RESET_EVERY_2		(0 << 24)
/** Sets the subcarrier DDA to reset frequency every fourth field */
# define TV_SC_RESET_EVERY_4		(1 << 24)
/** Sets the subcarrier DDA to reset frequency every eighth field */
# define TV_SC_RESET_EVERY_8		(2 << 24)
/** Sets the subcarrier DDA to never reset the frequency */
# define TV_SC_RESET_NEVER		(3 << 24)
/** Sets the peak amplitude of the colorburst.*/
# define TV_BURST_LEVEL_MASK		0x00ff0000
# define TV_BURST_LEVEL_SHIFT		16
/** Sets the increment of the first subcarrier phase generation DDA */
# define TV_SCDDA1_INC_MASK		0x00000fff
# define TV_SCDDA1_INC_SHIFT		0

#define TV_SC_CTL_2		0x68064
/** Sets the rollover for the second subcarrier phase generation DDA */
# define TV_SCDDA2_SIZE_MASK		0x7fff0000
# define TV_SCDDA2_SIZE_SHIFT		16
/** Sets the increent of the second subcarrier phase generation DDA */
# define TV_SCDDA2_INC_MASK		0x00007fff
# define TV_SCDDA2_INC_SHIFT		0

#define TV_SC_CTL_3		0x68068
/** Sets the rollover for the third subcarrier phase generation DDA */
# define TV_SCDDA3_SIZE_MASK		0x7fff0000
# define TV_SCDDA3_SIZE_SHIFT		16
/** Sets the increent of the third subcarrier phase generation DDA */
# define TV_SCDDA3_INC_MASK		0x00007fff
# define TV_SCDDA3_INC_SHIFT		0

#define TV_WIN_POS		0x68070
/** X coordinate of the display from the start of horizontal active */
# define TV_XPOS_MASK			0x1fff0000
# define TV_XPOS_SHIFT			16
/** Y coordinate of the display from the start of vertical active (NBR) */
# define TV_YPOS_MASK			0x00000fff
# define TV_YPOS_SHIFT			0

#define TV_WIN_SIZE		0x68074
/** Horizontal size of the display window, measured in pixels*/
# define TV_XSIZE_MASK			0x1fff0000
# define TV_XSIZE_SHIFT			16
/**
 * Vertical size of the display window, measured in pixels.
 *
 * Must be even for interlaced modes.
 */
# define TV_YSIZE_MASK			0x00000fff
# define TV_YSIZE_SHIFT			0

#define TV_FILTER_CTL_1		0x68080
/**
 * Enables automatic scaling calculation.
 *
 * If set, the rest of the registers are ignored, and the calculated values can
 * be read back from the register.
 */
# define TV_AUTO_SCALE			(1 << 31)
/**
 * Disables the vertical filter.
 *
 * This is required on modes more than 1024 pixels wide */
# define TV_V_FILTER_BYPASS		(1 << 29)
/** Enables adaptive vertical filtering */
# define TV_VADAPT			(1 << 28)
# define TV_VADAPT_MODE_MASK		(3 << 26)
/** Selects the least adaptive vertical filtering mode */
# define TV_VADAPT_MODE_LEAST		(0 << 26)
/** Selects the moderately adaptive vertical filtering mode */
# define TV_VADAPT_MODE_MODERATE	(1 << 26)
/** Selects the most adaptive vertical filtering mode */
# define TV_VADAPT_MODE_MOST		(3 << 26)
/**
 * Sets the horizontal scaling factor.
 *
 * This should be the fractional part of the horizontal scaling factor divided
 * by the oversampling rate.  TV_HSCALE should be less than 1, and set to:
 *
 * (src width - 1) / ((oversample * dest width) - 1)
 */
# define TV_HSCALE_FRAC_MASK		0x00003fff
# define TV_HSCALE_FRAC_SHIFT		0

#define TV_FILTER_CTL_2		0x68084
/**
 * Sets the integer part of the 3.15 fixed-point vertical scaling factor.
 *
 * TV_VSCALE should be (src height - 1) / ((interlace * dest height) - 1)
 */
# define TV_VSCALE_INT_MASK		0x00038000
# define TV_VSCALE_INT_SHIFT		15
/**
 * Sets the fractional part of the 3.15 fixed-point vertical scaling factor.
 *
 * \sa TV_VSCALE_INT_MASK
 */
# define TV_VSCALE_FRAC_MASK		0x00007fff
# define TV_VSCALE_FRAC_SHIFT		0

#define TV_FILTER_CTL_3		0x68088
/**
 * Sets the integer part of the 3.15 fixed-point vertical scaling factor.
 *
 * TV_VSCALE should be (src height - 1) / (1/4 * (dest height - 1))
 *
 * For progressive modes, TV_VSCALE_IP_INT should be set to zeroes.
 */
# define TV_VSCALE_IP_INT_MASK		0x00038000
# define TV_VSCALE_IP_INT_SHIFT		15
/**
 * Sets the fractional part of the 3.15 fixed-point vertical scaling factor.
 *
 * For progressive modes, TV_VSCALE_IP_INT should be set to zeroes.
 *
 * \sa TV_VSCALE_IP_INT_MASK
 */
# define TV_VSCALE_IP_FRAC_MASK		0x00007fff
# define TV_VSCALE_IP_FRAC_SHIFT		0

#define TV_CC_CONTROL		0x68090
# define TV_CC_ENABLE			(1 << 31)
/**
 * Specifies which field to send the CC data in.
 *
 * CC data is usually sent in field 0.
 */
# define TV_CC_FID_MASK			(1 << 27)
# define TV_CC_FID_SHIFT		27
/** Sets the horizontal position of the CC data.  Usually 135. */
# define TV_CC_HOFF_MASK		0x03ff0000
# define TV_CC_HOFF_SHIFT		16
/** Sets the vertical position of the CC data.  Usually 21 */
# define TV_CC_LINE_MASK		0x0000003f
# define TV_CC_LINE_SHIFT		0

#define TV_CC_DATA		0x68094
# define TV_CC_RDY			(1 << 31)
/** Second word of CC data to be transmitted. */
# define TV_CC_DATA_2_MASK		0x007f0000
# define TV_CC_DATA_2_SHIFT		16
/** First word of CC data to be transmitted. */
# define TV_CC_DATA_1_MASK		0x0000007f
# define TV_CC_DATA_1_SHIFT		0

#define TV_H_LUMA_0		0x68100
#define TV_H_LUMA_59		0x681ec
#define TV_H_CHROMA_0		0x68200
#define TV_H_CHROMA_59		0x682ec
#define TV_V_LUMA_0		0x68300
#define TV_V_LUMA_42		0x683a8
#define TV_V_CHROMA_0		0x68400
#define TV_V_CHROMA_42		0x684a8

/* Display Port */
#define DP_A				0x64000 /* eDP */
#define DP_B				0x64100
#define DP_C				0x64200
#define DP_D				0x64300

#define   DP_PORT_EN			(1 << 31)
#define   DP_PIPEB_SELECT		(1 << 30)

/* Link training mode - select a suitable mode for each stage */
#define   DP_LINK_TRAIN_PAT_1		(0 << 28)
#define   DP_LINK_TRAIN_PAT_2		(1 << 28)
#define   DP_LINK_TRAIN_PAT_IDLE	(2 << 28)
#define   DP_LINK_TRAIN_OFF		(3 << 28)
#define   DP_LINK_TRAIN_MASK		(3 << 28)
#define   DP_LINK_TRAIN_SHIFT		28

/* Signal voltages. These are mostly controlled by the other end */
#define   DP_VOLTAGE_0_4		(0 << 25)
#define   DP_VOLTAGE_0_6		(1 << 25)
#define   DP_VOLTAGE_0_8		(2 << 25)
#define   DP_VOLTAGE_1_2		(3 << 25)
#define   DP_VOLTAGE_MASK		(7 << 25)
#define   DP_VOLTAGE_SHIFT		25

/* Signal pre-emphasis levels, like voltages, the other end tells us what
 * they want
 */
#define   DP_PRE_EMPHASIS_0		(0 << 22)
#define   DP_PRE_EMPHASIS_3_5		(1 << 22)
#define   DP_PRE_EMPHASIS_6		(2 << 22)
#define   DP_PRE_EMPHASIS_9_5		(3 << 22)
#define   DP_PRE_EMPHASIS_MASK		(7 << 22)
#define   DP_PRE_EMPHASIS_SHIFT		22

/* How many wires to use. I guess 3 was too hard */
#define   DP_PORT_WIDTH_1		(0 << 19)
#define   DP_PORT_WIDTH_2		(1 << 19)
#define   DP_PORT_WIDTH_4		(3 << 19)
#define   DP_PORT_WIDTH_MASK		(7 << 19)

/* Mystic DPCD version 1.1 special mode */
#define   DP_ENHANCED_FRAMING		(1 << 18)

/* eDP */
#define   DP_PLL_FREQ_270MHZ		(0 << 16)
#define   DP_PLL_FREQ_160MHZ		(1 << 16)
#define   DP_PLL_FREQ_MASK		(3 << 16)

/** locked once port is enabled */
#define   DP_PORT_REVERSAL		(1 << 15)

/* eDP */
#define   DP_PLL_ENABLE			(1 << 14)

/** sends the clock on lane 15 of the PEG for debug */
#define   DP_CLOCK_OUTPUT_ENABLE	(1 << 13)

#define   DP_SCRAMBLING_DISABLE		(1 << 12)
#define   DP_SCRAMBLING_DISABLE_IGDNG	(1 << 7)

/** limit RGB values to avoid confusing TVs */
#define   DP_COLOR_RANGE_16_235		(1 << 8)

/** Turn on the audio link */
#define   DP_AUDIO_OUTPUT_ENABLE	(1 << 6)

/** vs and hs sync polarity */
#define   DP_SYNC_VS_HIGH		(1 << 4)
#define   DP_SYNC_HS_HIGH		(1 << 3)

/** A fantasy */
#define   DP_DETECTED			(1 << 2)

/** The aux channel provides a way to talk to the
 * signal sink for DDC etc. Max packet size supported
 * is 20 bytes in each direction, hence the 5 fixed
 * data registers
 */
#define DPA_AUX_CH_CTL			0x64010
#define DPA_AUX_CH_DATA1		0x64014
#define DPA_AUX_CH_DATA2		0x64018
#define DPA_AUX_CH_DATA3		0x6401c
#define DPA_AUX_CH_DATA4		0x64020
#define DPA_AUX_CH_DATA5		0x64024

#define DPB_AUX_CH_CTL			0x64110
#define DPB_AUX_CH_DATA1		0x64114
#define DPB_AUX_CH_DATA2		0x64118
#define DPB_AUX_CH_DATA3		0x6411c
#define DPB_AUX_CH_DATA4		0x64120
#define DPB_AUX_CH_DATA5		0x64124

#define DPC_AUX_CH_CTL			0x64210
#define DPC_AUX_CH_DATA1		0x64214
#define DPC_AUX_CH_DATA2		0x64218
#define DPC_AUX_CH_DATA3		0x6421c
#define DPC_AUX_CH_DATA4		0x64220
#define DPC_AUX_CH_DATA5		0x64224

#define DPD_AUX_CH_CTL			0x64310
#define DPD_AUX_CH_DATA1		0x64314
#define DPD_AUX_CH_DATA2		0x64318
#define DPD_AUX_CH_DATA3		0x6431c
#define DPD_AUX_CH_DATA4		0x64320
#define DPD_AUX_CH_DATA5		0x64324

#define   DP_AUX_CH_CTL_SEND_BUSY	    (1 << 31)
#define   DP_AUX_CH_CTL_DONE		    (1 << 30)
#define   DP_AUX_CH_CTL_INTERRUPT	    (1 << 29)
#define   DP_AUX_CH_CTL_TIME_OUT_ERROR	    (1 << 28)
#define   DP_AUX_CH_CTL_TIME_OUT_400us	    (0 << 26)
#define   DP_AUX_CH_CTL_TIME_OUT_600us	    (1 << 26)
#define   DP_AUX_CH_CTL_TIME_OUT_800us	    (2 << 26)
#define   DP_AUX_CH_CTL_TIME_OUT_1600us	    (3 << 26)
#define   DP_AUX_CH_CTL_TIME_OUT_MASK	    (3 << 26)
#define   DP_AUX_CH_CTL_RECEIVE_ERROR	    (1 << 25)
#define   DP_AUX_CH_CTL_MESSAGE_SIZE_MASK    (0x1f << 20)
#define   DP_AUX_CH_CTL_MESSAGE_SIZE_SHIFT   20
#define   DP_AUX_CH_CTL_PRECHARGE_2US_MASK   (0xf << 16)
#define   DP_AUX_CH_CTL_PRECHARGE_2US_SHIFT  16
#define   DP_AUX_CH_CTL_AUX_AKSV_SELECT	    (1 << 15)
#define   DP_AUX_CH_CTL_MANCHESTER_TEST	    (1 << 14)
#define   DP_AUX_CH_CTL_SYNC_TEST	    (1 << 13)
#define   DP_AUX_CH_CTL_DEGLITCH_TEST	    (1 << 12)
#define   DP_AUX_CH_CTL_PRECHARGE_TEST	    (1 << 11)
#define   DP_AUX_CH_CTL_BIT_CLOCK_2X_MASK    (0x7ff)
#define   DP_AUX_CH_CTL_BIT_CLOCK_2X_SHIFT   0

/*
 * Computing GMCH M and N values for the Display Port link
 *
 * GMCH M/N = dot clock * bytes per pixel / ls_clk * # of lanes
 *
 * ls_clk (we assume) is the DP link clock (1.62 or 2.7 GHz)
 *
 * The GMCH value is used internally
 *
 * bytes_per_pixel is the number of bytes coming out of the plane,
 * which is after the LUTs, so we want the bytes for our color format.
 * For our current usage, this is always 3, one byte for R, G and B.
 */
#define PIPEA_GMCH_DATA_M			0x70050
#define PIPEB_GMCH_DATA_M			0x71050

/* Transfer unit size for display port - 1, default is 0x3f (for TU size 64) */
#define   PIPE_GMCH_DATA_M_TU_SIZE_MASK		(0x3f << 25)
#define   PIPE_GMCH_DATA_M_TU_SIZE_SHIFT	25

#define   PIPE_GMCH_DATA_M_MASK			(0xffffff)

#define PIPEA_GMCH_DATA_N			0x70054
#define PIPEB_GMCH_DATA_N			0x71054
#define   PIPE_GMCH_DATA_N_MASK			(0xffffff)

/*
 * Computing Link M and N values for the Display Port link
 *
 * Link M / N = pixel_clock / ls_clk
 *
 * (the DP spec calls pixel_clock the 'strm_clk')
 *
 * The Link value is transmitted in the Main Stream
 * Attributes and VB-ID.
 */

#define PIPEA_DP_LINK_M				0x70060
#define PIPEB_DP_LINK_M				0x71060
#define   PIPEA_DP_LINK_M_MASK			(0xffffff)

#define PIPEA_DP_LINK_N				0x70064
#define PIPEB_DP_LINK_N				0x71064
#define   PIPEA_DP_LINK_N_MASK			(0xffffff)

/* Display & cursor control */

/* Pipe A */
#define PIPEADSL		0x70000
#define PIPEACONF		 0x70008
#define   PIPEACONF_ENABLE	(1<<31)
#define   PIPEACONF_DISABLE	0
#define   PIPEACONF_DOUBLE_WIDE	(1<<30)
#define   I965_PIPECONF_ACTIVE	(1<<30)
#define   PIPEACONF_SINGLE_WIDE	0
#define   PIPEACONF_PIPE_UNLOCKED 0
#define   PIPEACONF_PIPE_LOCKED	(1<<25)
#define   PIPEACONF_PALETTE	0
#define   PIPEACONF_GAMMA		(1<<24)
#define   PIPECONF_FORCE_BORDER	(1<<25)
#define   PIPECONF_PROGRESSIVE	(0 << 21)
#define   PIPECONF_INTERLACE_W_FIELD_INDICATION	(6 << 21)
#define   PIPECONF_INTERLACE_FIELD_0_ONLY		(7 << 21)
#define PIPEASTAT		0x70024
#define   PIPE_FIFO_UNDERRUN_STATUS		(1UL<<31)
#define   PIPE_CRC_ERROR_ENABLE			(1UL<<29)
#define   PIPE_CRC_DONE_ENABLE			(1UL<<28)
#define   PIPE_GMBUS_EVENT_ENABLE		(1UL<<27)
#define   PIPE_HOTPLUG_INTERRUPT_ENABLE		(1UL<<26)
#define   PIPE_VSYNC_INTERRUPT_ENABLE		(1UL<<25)
#define   PIPE_DISPLAY_LINE_COMPARE_ENABLE	(1UL<<24)
#define   PIPE_DPST_EVENT_ENABLE		(1UL<<23)
#define   PIPE_LEGACY_BLC_EVENT_ENABLE		(1UL<<22)
#define   PIPE_ODD_FIELD_INTERRUPT_ENABLE	(1UL<<21)
#define   PIPE_EVEN_FIELD_INTERRUPT_ENABLE	(1UL<<20)
#define   PIPE_HOTPLUG_TV_INTERRUPT_ENABLE	(1UL<<18) /* pre-965 */
#define   PIPE_START_VBLANK_INTERRUPT_ENABLE	(1UL<<18) /* 965 or later */
#define   PIPE_VBLANK_INTERRUPT_ENABLE		(1UL<<17)
#define   PIPE_OVERLAY_UPDATED_ENABLE		(1UL<<16)
#define   PIPE_CRC_ERROR_INTERRUPT_STATUS	(1UL<<13)
#define   PIPE_CRC_DONE_INTERRUPT_STATUS	(1UL<<12)
#define   PIPE_GMBUS_INTERRUPT_STATUS		(1UL<<11)
#define   PIPE_HOTPLUG_INTERRUPT_STATUS		(1UL<<10)
#define   PIPE_VSYNC_INTERRUPT_STATUS		(1UL<<9)
#define   PIPE_DISPLAY_LINE_COMPARE_STATUS	(1UL<<8)
#define   PIPE_DPST_EVENT_STATUS		(1UL<<7)
#define   PIPE_LEGACY_BLC_EVENT_STATUS		(1UL<<6)
#define   PIPE_ODD_FIELD_INTERRUPT_STATUS	(1UL<<5)
#define   PIPE_EVEN_FIELD_INTERRUPT_STATUS	(1UL<<4)
#define   PIPE_HOTPLUG_TV_INTERRUPT_STATUS	(1UL<<2) /* pre-965 */
#define   PIPE_START_VBLANK_INTERRUPT_STATUS	(1UL<<2) /* 965 or later */
#define   PIPE_VBLANK_INTERRUPT_STATUS		(1UL<<1)
#define   PIPE_OVERLAY_UPDATED_STATUS		(1UL<<0)

#define DSPARB			0x70030
#define   DSPARB_CSTART_MASK	(0x7f << 7)
#define   DSPARB_CSTART_SHIFT	7
#define   DSPARB_BSTART_MASK	(0x7f)
#define   DSPARB_BSTART_SHIFT	0
/*
 * The two pipe frame counter registers are not synchronized, so
 * reading a stable value is somewhat tricky. The following code
 * should work:
 *
 *  do {
 *    high1 = ((INREG(PIPEAFRAMEHIGH) & PIPE_FRAME_HIGH_MASK) >>
 *             PIPE_FRAME_HIGH_SHIFT;
 *    low1 =  ((INREG(PIPEAFRAMEPIXEL) & PIPE_FRAME_LOW_MASK) >>
 *             PIPE_FRAME_LOW_SHIFT);
 *    high2 = ((INREG(PIPEAFRAMEHIGH) & PIPE_FRAME_HIGH_MASK) >>
 *             PIPE_FRAME_HIGH_SHIFT);
 *  } while (high1 != high2);
 *  frame = (high1 << 8) | low1;
 */
#define PIPEAFRAMEHIGH          0x70040
#define   PIPE_FRAME_HIGH_MASK    0x0000ffff
#define   PIPE_FRAME_HIGH_SHIFT   0
#define PIPEAFRAMEPIXEL         0x70044
#define   PIPE_FRAME_LOW_MASK     0xff000000
#define   PIPE_FRAME_LOW_SHIFT    24
#define   PIPE_PIXEL_MASK         0x00ffffff
#define   PIPE_PIXEL_SHIFT        0
/* GM45+ just has to be different */
#define	PIPEA_FRMCOUNT_GM45       0x70040
#define	PIPEA_FLIPCOUNT_GM45      0x70044

/* Cursor A & B regs */
#define CURACNTR		0x70080
#define   CURSOR_MODE_DISABLE   0x00
#define   CURSOR_MODE_64_32B_AX 0x07
#define   CURSOR_MODE_64_ARGB_AX ((1 << 5) | CURSOR_MODE_64_32B_AX)
#define   MCURSOR_GAMMA_ENABLE  (1 << 26)
#define CURABASE		0x70084
#define CURAPOS			0x70088
#define   CURSOR_POS_MASK       0x007FF
#define   CURSOR_POS_SIGN       0x8000
#define   CURSOR_X_SHIFT        0
#define   CURSOR_Y_SHIFT        16
#define CURSIZE			0x700a0
#define CURBCNTR		0x700c0
#define CURBBASE		0x700c4
#define CURBPOS			0x700c8

/* Display A control */
#define DSPACNTR                0x70180
#define   DISPLAY_PLANE_ENABLE			(1<<31)
#define   DISPLAY_PLANE_DISABLE			0
#define   DISPPLANE_GAMMA_ENABLE		(1<<30)
#define   DISPPLANE_GAMMA_DISABLE		0
#define   DISPPLANE_PIXFORMAT_MASK		(0xf<<26)
#define   DISPPLANE_8BPP			(0x2<<26)
#define   DISPPLANE_15_16BPP			(0x4<<26)
#define   DISPPLANE_16BPP			(0x5<<26)
#define   DISPPLANE_32BPP_NO_ALPHA		(0x6<<26)
#define   DISPPLANE_32BPP			(0x7<<26)
#define   DISPPLANE_STEREO_ENABLE		(1<<25)
#define   DISPPLANE_STEREO_DISABLE		0
#define   DISPPLANE_SEL_PIPE_MASK		(1<<24)
#define   DISPPLANE_SEL_PIPE_A			0
#define   DISPPLANE_SEL_PIPE_B			(1<<24)
#define   DISPPLANE_SRC_KEY_ENABLE		(1<<22)
#define   DISPPLANE_SRC_KEY_DISABLE		0
#define   DISPPLANE_LINE_DOUBLE			(1<<20)
#define   DISPPLANE_NO_LINE_DOUBLE		0
#define   DISPPLANE_STEREO_POLARITY_FIRST	0
#define   DISPPLANE_STEREO_POLARITY_SECOND	(1<<18)
#define DSPAADDR		0x70184
#define DSPASTRIDE		0x70188
#define DSPAPOS			0x7018C /* reserved */
#define DSPASIZE		0x70190
#define DSPASURF		0x7019C /* 965+ only */
#define DSPATILEOFF		0x701A4 /* 965+ only */

/* VBIOS flags */
#define SWF00			0x71410
#define SWF01			0x71414
#define SWF02			0x71418
#define SWF03			0x7141c
#define SWF04			0x71420
#define SWF05			0x71424
#define SWF06			0x71428
#define SWF10			0x70410
#define SWF11			0x70414
#define SWF14			0x71420
#define SWF30			0x72414
#define SWF31			0x72418
#define SWF32			0x7241c

/* Pipe B */
#define PIPEBDSL		0x71000
#define PIPEBCONF		0x71008
#define PIPEBSTAT		0x71024
#define PIPEBFRAMEHIGH		0x71040
#define PIPEBFRAMEPIXEL		0x71044
#define PIPEB_FRMCOUNT_GM45	0x71040
#define PIPEB_FLIPCOUNT_GM45	0x71044

/* Display B control */
#define DSPBCNTR		0x71180
#define   DISPPLANE_ALPHA_TRANS_ENABLE		(1<<15)
#define   DISPPLANE_ALPHA_TRANS_DISABLE		0
#define   DISPPLANE_SPRITE_ABOVE_DISPLAY	0
#define   DISPPLANE_SPRITE_ABOVE_OVERLAY	(1)
#define DSPBADDR		0x71184
#define DSPBSTRIDE		0x71188
#define DSPBPOS			0x7118C
#define DSPBSIZE		0x71190
#define DSPBSURF		0x7119C
#define DSPBTILEOFF		0x711A4

/* VBIOS regs */
#define VGACNTRL		0x71400
# define VGA_DISP_DISABLE			(1 << 31)
# define VGA_2X_MODE				(1 << 30)
# define VGA_PIPE_B_SELECT			(1 << 29)

/* Ironlake */

#define CPU_VGACNTRL	0x41000

#define DIGITAL_PORT_HOTPLUG_CNTRL      0x44030
#define  DIGITAL_PORTA_HOTPLUG_ENABLE           (1 << 4)
#define  DIGITAL_PORTA_SHORT_PULSE_2MS          (0 << 2)
#define  DIGITAL_PORTA_SHORT_PULSE_4_5MS        (1 << 2)
#define  DIGITAL_PORTA_SHORT_PULSE_6MS          (2 << 2)
#define  DIGITAL_PORTA_SHORT_PULSE_100MS        (3 << 2)
#define  DIGITAL_PORTA_NO_DETECT                (0 << 0)
#define  DIGITAL_PORTA_LONG_PULSE_DETECT_MASK   (1 << 1)
#define  DIGITAL_PORTA_SHORT_PULSE_DETECT_MASK  (1 << 0)

/* refresh rate hardware control */
#define RR_HW_CTL       0x45300
#define  RR_HW_LOW_POWER_FRAMES_MASK    0xff
#define  RR_HW_HIGH_POWER_FRAMES_MASK   0xff00

#define FDI_PLL_BIOS_0  0x46000
#define FDI_PLL_BIOS_1  0x46004
#define FDI_PLL_BIOS_2  0x46008
#define DISPLAY_PORT_PLL_BIOS_0         0x4600c
#define DISPLAY_PORT_PLL_BIOS_1         0x46010
#define DISPLAY_PORT_PLL_BIOS_2         0x46014

#define PCH_DSPCLK_GATE_D	0x42020
# define DPFDUNIT_CLOCK_GATE_DISABLE		(1 << 7)
# define DPARBUNIT_CLOCK_GATE_DISABLE		(1 << 5)

#define PCH_3DCGDIS0		0x46020
# define MARIUNIT_CLOCK_GATE_DISABLE		(1 << 18)
# define SVSMUNIT_CLOCK_GATE_DISABLE		(1 << 1)

#define FDI_PLL_FREQ_CTL        0x46030
#define  FDI_PLL_FREQ_CHANGE_REQUEST    (1<<24)
#define  FDI_PLL_FREQ_LOCK_LIMIT_MASK   0xfff00
#define  FDI_PLL_FREQ_DISABLE_COUNT_LIMIT_MASK  0xff


#define PIPEA_DATA_M1           0x60030
#define  TU_SIZE(x)             (((x)-1) << 25) /* default size 64 */
#define  TU_SIZE_MASK           0x7e000000
#define  PIPEA_DATA_M1_OFFSET   0
#define PIPEA_DATA_N1           0x60034
#define  PIPEA_DATA_N1_OFFSET   0

#define PIPEA_DATA_M2           0x60038
#define  PIPEA_DATA_M2_OFFSET   0
#define PIPEA_DATA_N2           0x6003c
#define  PIPEA_DATA_N2_OFFSET   0

#define PIPEA_LINK_M1           0x60040
#define  PIPEA_LINK_M1_OFFSET   0
#define PIPEA_LINK_N1           0x60044
#define  PIPEA_LINK_N1_OFFSET   0

#define PIPEA_LINK_M2           0x60048
#define  PIPEA_LINK_M2_OFFSET   0
#define PIPEA_LINK_N2           0x6004c
#define  PIPEA_LINK_N2_OFFSET   0

/* PIPEB timing regs are same start from 0x61000 */

#define PIPEB_DATA_M1           0x61030
#define  PIPEB_DATA_M1_OFFSET   0
#define PIPEB_DATA_N1           0x61034
#define  PIPEB_DATA_N1_OFFSET   0

#define PIPEB_DATA_M2           0x61038
#define  PIPEB_DATA_M2_OFFSET   0
#define PIPEB_DATA_N2           0x6103c
#define  PIPEB_DATA_N2_OFFSET   0

#define PIPEB_LINK_M1           0x61040
#define  PIPEB_LINK_M1_OFFSET   0
#define PIPEB_LINK_N1           0x61044
#define  PIPEB_LINK_N1_OFFSET   0

#define PIPEB_LINK_M2           0x61048
#define  PIPEB_LINK_M2_OFFSET   0
#define PIPEB_LINK_N2           0x6104c
#define  PIPEB_LINK_N2_OFFSET   0

/* CPU panel fitter */
#define PFA_CTL_1               0x68080
#define PFB_CTL_1               0x68880
#define  PF_ENABLE              (1<<31)
#define  PF_FILTER_MASK		(3<<23)
#define  PF_FILTER_PROGRAMMED	(0<<23)
#define  PF_FILTER_MED_3x3	(1<<23)
#define  PF_FILTER_EDGE_ENHANCE	(2<<23)
#define  PF_FILTER_EDGE_SOFTEN	(3<<23)
#define PFA_WIN_SZ		0x68074
#define PFB_WIN_SZ		0x68874
#define PFA_WIN_POS		0x68070
#define PFB_WIN_POS		0x68870

/* legacy palette */
#define LGC_PALETTE_A           0x4a000
#define LGC_PALETTE_B           0x4a800

/* interrupts */
#define DE_MASTER_IRQ_CONTROL   (1 << 31)
#define DE_SPRITEB_FLIP_DONE    (1 << 29)
#define DE_SPRITEA_FLIP_DONE    (1 << 28)
#define DE_PLANEB_FLIP_DONE     (1 << 27)
#define DE_PLANEA_FLIP_DONE     (1 << 26)
#define DE_PCU_EVENT            (1 << 25)
#define DE_GTT_FAULT            (1 << 24)
#define DE_POISON               (1 << 23)
#define DE_PERFORM_COUNTER      (1 << 22)
#define DE_PCH_EVENT            (1 << 21)
#define DE_AUX_CHANNEL_A        (1 << 20)
#define DE_DP_A_HOTPLUG         (1 << 19)
#define DE_GSE                  (1 << 18)
#define DE_PIPEB_VBLANK         (1 << 15)
#define DE_PIPEB_EVEN_FIELD     (1 << 14)
#define DE_PIPEB_ODD_FIELD      (1 << 13)
#define DE_PIPEB_LINE_COMPARE   (1 << 12)
#define DE_PIPEB_VSYNC          (1 << 11)
#define DE_PIPEB_FIFO_UNDERRUN  (1 << 8)
#define DE_PIPEA_VBLANK         (1 << 7)
#define DE_PIPEA_EVEN_FIELD     (1 << 6)
#define DE_PIPEA_ODD_FIELD      (1 << 5)
#define DE_PIPEA_LINE_COMPARE   (1 << 4)
#define DE_PIPEA_VSYNC          (1 << 3)
#define DE_PIPEA_FIFO_UNDERRUN  (1 << 0)

#define DEISR   0x44000
#define DEIMR   0x44004
#define DEIIR   0x44008
#define DEIER   0x4400c

/* GT interrupt */
#define GT_MASTER_ERROR         (1 << 3)
#define GT_SYNC_STATUS          (1 << 2)
#define GT_USER_INTERRUPT       (1 << 0)

#define GTISR   0x44010
#define GTIMR   0x44014
#define GTIIR   0x44018
#define GTIER   0x4401c

#define ILK_DISPLAY_CHICKEN2	0x42004
#define  ILK_DPARB_GATE	(1<<22)
#define  ILK_VSDPFD_FULL	(1<<21)
#define ILK_DSPCLK_GATE		0x42020
#define  ILK_DPARB_CLK_GATE	(1<<5)

#define DISP_ARB_CTL	0x45000
#define  DISP_TILE_SURFACE_SWIZZLING	(1<<13)
#define  DISP_FBC_WM_DIS		(1<<15)

/* PCH */

/* south display engine interrupt */
#define SDE_CRT_HOTPLUG         (1 << 11)
#define SDE_PORTD_HOTPLUG       (1 << 10)
#define SDE_PORTC_HOTPLUG       (1 << 9)
#define SDE_PORTB_HOTPLUG       (1 << 8)
#define SDE_SDVOB_HOTPLUG       (1 << 6)
#define SDE_HOTPLUG_MASK	(0xf << 8)
/* CPT */
#define SDE_CRT_HOTPLUG_CPT	(1 << 19)
#define SDE_PORTD_HOTPLUG_CPT	(1 << 23)
#define SDE_PORTC_HOTPLUG_CPT	(1 << 22)
#define SDE_PORTB_HOTPLUG_CPT	(1 << 21)

#define SDEISR  0xc4000
#define SDEIMR  0xc4004
#define SDEIIR  0xc4008
#define SDEIER  0xc400c

/* digital port hotplug */
#define PCH_PORT_HOTPLUG        0xc4030
#define PORTD_HOTPLUG_ENABLE            (1 << 20)
#define PORTD_PULSE_DURATION_2ms        (0)
#define PORTD_PULSE_DURATION_4_5ms      (1 << 18)
#define PORTD_PULSE_DURATION_6ms        (2 << 18)
#define PORTD_PULSE_DURATION_100ms      (3 << 18)
#define PORTD_HOTPLUG_NO_DETECT         (0)
#define PORTD_HOTPLUG_SHORT_DETECT      (1 << 16)
#define PORTD_HOTPLUG_LONG_DETECT       (1 << 17)
#define PORTC_HOTPLUG_ENABLE            (1 << 12)
#define PORTC_PULSE_DURATION_2ms        (0)
#define PORTC_PULSE_DURATION_4_5ms      (1 << 10)
#define PORTC_PULSE_DURATION_6ms        (2 << 10)
#define PORTC_PULSE_DURATION_100ms      (3 << 10)
#define PORTC_HOTPLUG_NO_DETECT         (0)
#define PORTC_HOTPLUG_SHORT_DETECT      (1 << 8)
#define PORTC_HOTPLUG_LONG_DETECT       (1 << 9)
#define PORTB_HOTPLUG_ENABLE            (1 << 4)
#define PORTB_PULSE_DURATION_2ms        (0)
#define PORTB_PULSE_DURATION_4_5ms      (1 << 2)
#define PORTB_PULSE_DURATION_6ms        (2 << 2)
#define PORTB_PULSE_DURATION_100ms      (3 << 2)
#define PORTB_HOTPLUG_NO_DETECT         (0)
#define PORTB_HOTPLUG_SHORT_DETECT      (1 << 0)
#define PORTB_HOTPLUG_LONG_DETECT       (1 << 1)

#define PCH_GPIOA               0xc5010
#define PCH_GPIOB               0xc5014
#define PCH_GPIOC               0xc5018
#define PCH_GPIOD               0xc501c
#define PCH_GPIOE               0xc5020
#define PCH_GPIOF               0xc5024

#define PCH_GMBUS0		0xc5100
#define PCH_GMBUS1		0xc5104
#define PCH_GMBUS2		0xc5108
#define PCH_GMBUS3		0xc510c
#define PCH_GMBUS4		0xc5110
#define PCH_GMBUS5		0xc5120

#define PCH_DPLL_A              0xc6014
#define PCH_DPLL_B              0xc6018

#define PCH_FPA0                0xc6040
#define PCH_FPA1                0xc6044
#define PCH_FPB0                0xc6048
#define PCH_FPB1                0xc604c

#define PCH_DPLL_TEST           0xc606c

#define PCH_DREF_CONTROL        0xC6200
#define  DREF_CONTROL_MASK      0x7fc3
#define  DREF_CPU_SOURCE_OUTPUT_DISABLE         (0<<13)
#define  DREF_CPU_SOURCE_OUTPUT_DOWNSPREAD      (2<<13)
#define  DREF_CPU_SOURCE_OUTPUT_NONSPREAD       (3<<13)
#define  DREF_CPU_SOURCE_OUTPUT_MASK		(3<<13)
#define  DREF_SSC_SOURCE_DISABLE                (0<<11)
#define  DREF_SSC_SOURCE_ENABLE                 (2<<11)
#define  DREF_SSC_SOURCE_MASK			(3<<11)
#define  DREF_NONSPREAD_SOURCE_DISABLE          (0<<9)
#define  DREF_NONSPREAD_CK505_ENABLE		(1<<9)
#define  DREF_NONSPREAD_SOURCE_ENABLE           (2<<9)
#define  DREF_NONSPREAD_SOURCE_MASK		(3<<9)
#define  DREF_SUPERSPREAD_SOURCE_DISABLE        (0<<7)
#define  DREF_SUPERSPREAD_SOURCE_ENABLE         (2<<7)
#define  DREF_SSC4_DOWNSPREAD                   (0<<6)
#define  DREF_SSC4_CENTERSPREAD                 (1<<6)
#define  DREF_SSC1_DISABLE                      (0<<1)
#define  DREF_SSC1_ENABLE                       (1<<1)
#define  DREF_SSC4_DISABLE                      (0)
#define  DREF_SSC4_ENABLE                       (1)

#define PCH_RAWCLK_FREQ         0xc6204
#define  FDL_TP1_TIMER_SHIFT    12
#define  FDL_TP1_TIMER_MASK     (3<<12)
#define  FDL_TP2_TIMER_SHIFT    10
#define  FDL_TP2_TIMER_MASK     (3<<10)
#define  RAWCLK_FREQ_MASK       0x3ff

#define PCH_DPLL_TMR_CFG        0xc6208

#define PCH_SSC4_PARMS          0xc6210
#define PCH_SSC4_AUX_PARMS      0xc6214

#define PCH_DPLL_SEL		0xc7000
#define  TRANSA_DPLL_ENABLE	(1<<3)
#define	 TRANSA_DPLLB_SEL	(1<<0)
#define	 TRANSA_DPLLA_SEL	0
#define  TRANSB_DPLL_ENABLE	(1<<7)
#define	 TRANSB_DPLLB_SEL	(1<<4)
#define	 TRANSB_DPLLA_SEL	(0)
#define  TRANSC_DPLL_ENABLE	(1<<11)
#define	 TRANSC_DPLLB_SEL	(1<<8)
#define	 TRANSC_DPLLA_SEL	(0)

/* transcoder */

#define TRANS_HTOTAL_A          0xe0000
#define  TRANS_HTOTAL_SHIFT     16
#define  TRANS_HACTIVE_SHIFT    0
#define TRANS_HBLANK_A          0xe0004
#define  TRANS_HBLANK_END_SHIFT 16
#define  TRANS_HBLANK_START_SHIFT 0
#define TRANS_HSYNC_A           0xe0008
#define  TRANS_HSYNC_END_SHIFT  16
#define  TRANS_HSYNC_START_SHIFT 0
#define TRANS_VTOTAL_A          0xe000c
#define  TRANS_VTOTAL_SHIFT     16
#define  TRANS_VACTIVE_SHIFT    0
#define TRANS_VBLANK_A          0xe0010
#define  TRANS_VBLANK_END_SHIFT 16
#define  TRANS_VBLANK_START_SHIFT 0
#define TRANS_VSYNC_A           0xe0014
#define  TRANS_VSYNC_END_SHIFT  16
#define  TRANS_VSYNC_START_SHIFT 0

#define TRANSA_DATA_M1          0xe0030
#define TRANSA_DATA_N1          0xe0034
#define TRANSA_DATA_M2          0xe0038
#define TRANSA_DATA_N2          0xe003c
#define TRANSA_DP_LINK_M1       0xe0040
#define TRANSA_DP_LINK_N1       0xe0044
#define TRANSA_DP_LINK_M2       0xe0048
#define TRANSA_DP_LINK_N2       0xe004c

#define TRANS_HTOTAL_B          0xe1000
#define TRANS_HBLANK_B          0xe1004
#define TRANS_HSYNC_B           0xe1008
#define TRANS_VTOTAL_B          0xe100c
#define TRANS_VBLANK_B          0xe1010
#define TRANS_VSYNC_B           0xe1014

#define TRANSB_DATA_M1          0xe1030
#define TRANSB_DATA_N1          0xe1034
#define TRANSB_DATA_M2          0xe1038
#define TRANSB_DATA_N2          0xe103c
#define TRANSB_DP_LINK_M1       0xe1040
#define TRANSB_DP_LINK_N1       0xe1044
#define TRANSB_DP_LINK_M2       0xe1048
#define TRANSB_DP_LINK_N2       0xe104c

#define TRANSACONF              0xf0008
#define TRANSBCONF              0xf1008
#define  TRANS_DISABLE          (0<<31)
#define  TRANS_ENABLE           (1<<31)
#define  TRANS_STATE_MASK       (1<<30)
#define  TRANS_STATE_DISABLE    (0<<30)
#define  TRANS_STATE_ENABLE     (1<<30)
#define  TRANS_FSYNC_DELAY_HB1  (0<<27)
#define  TRANS_FSYNC_DELAY_HB2  (1<<27)
#define  TRANS_FSYNC_DELAY_HB3  (2<<27)
#define  TRANS_FSYNC_DELAY_HB4  (3<<27)
#define  TRANS_DP_AUDIO_ONLY    (1<<26)
#define  TRANS_DP_VIDEO_AUDIO   (0<<26)
#define  TRANS_PROGRESSIVE      (0<<21)
#define  TRANS_8BPC             (0<<5)
#define  TRANS_10BPC            (1<<5)
#define  TRANS_6BPC             (2<<5)
#define  TRANS_12BPC            (3<<5)

#define FDI_RXA_CHICKEN         0xc200c
#define FDI_RXB_CHICKEN         0xc2010
#define  FDI_RX_PHASE_SYNC_POINTER_ENABLE       (1)

/* CPU: FDI_TX */
#define FDI_TXA_CTL             0x60100
#define FDI_TXB_CTL             0x61100
#define  FDI_TX_DISABLE         (0<<31)
#define  FDI_TX_ENABLE          (1<<31)
#define  FDI_LINK_TRAIN_PATTERN_1       (0<<28)
#define  FDI_LINK_TRAIN_PATTERN_2       (1<<28)
#define  FDI_LINK_TRAIN_PATTERN_IDLE    (2<<28)
#define  FDI_LINK_TRAIN_NONE            (3<<28)
#define  FDI_LINK_TRAIN_VOLTAGE_0_4V    (0<<25)
#define  FDI_LINK_TRAIN_VOLTAGE_0_6V    (1<<25)
#define  FDI_LINK_TRAIN_VOLTAGE_0_8V    (2<<25)
#define  FDI_LINK_TRAIN_VOLTAGE_1_2V    (3<<25)
#define  FDI_LINK_TRAIN_PRE_EMPHASIS_NONE (0<<22)
#define  FDI_LINK_TRAIN_PRE_EMPHASIS_1_5X (1<<22)
#define  FDI_LINK_TRAIN_PRE_EMPHASIS_2X   (2<<22)
#define  FDI_LINK_TRAIN_PRE_EMPHASIS_3X   (3<<22)
/* ILK always use 400mV 0dB for voltage swing and pre-emphasis level.
   SNB has different settings. */
/* SNB A-stepping */
#define  FDI_LINK_TRAIN_400MV_0DB_SNB_A		(0x38<<22)
#define  FDI_LINK_TRAIN_400MV_6DB_SNB_A		(0x02<<22)
#define  FDI_LINK_TRAIN_600MV_3_5DB_SNB_A	(0x01<<22)
#define  FDI_LINK_TRAIN_800MV_0DB_SNB_A		(0x0<<22)
/* SNB B-stepping */
#define  FDI_LINK_TRAIN_400MV_0DB_SNB_B		(0x0<<22)
#define  FDI_LINK_TRAIN_400MV_6DB_SNB_B		(0x3a<<22)
#define  FDI_LINK_TRAIN_600MV_3_5DB_SNB_B	(0x39<<22)
#define  FDI_LINK_TRAIN_800MV_0DB_SNB_B		(0x38<<22)
#define  FDI_LINK_TRAIN_VOL_EMP_MASK		(0x3f<<22)
#define  FDI_DP_PORT_WIDTH_X1           (0<<19)
#define  FDI_DP_PORT_WIDTH_X2           (1<<19)
#define  FDI_DP_PORT_WIDTH_X3           (2<<19)
#define  FDI_DP_PORT_WIDTH_X4           (3<<19)
#define  FDI_TX_ENHANCE_FRAME_ENABLE    (1<<18)
/* Ironlake: hardwired to 1 */
#define  FDI_TX_PLL_ENABLE              (1<<14)
/* both Tx and Rx */
#define  FDI_SCRAMBLING_ENABLE          (0<<7)
#define  FDI_SCRAMBLING_DISABLE         (1<<7)

/* FDI_RX, FDI_X is hard-wired to Transcoder_X */
#define FDI_RXA_CTL             0xf000c
#define FDI_RXB_CTL             0xf100c
#define  FDI_RX_ENABLE          (1<<31)
#define  FDI_RX_DISABLE         (0<<31)
/* train, dp width same as FDI_TX */
#define  FDI_DP_PORT_WIDTH_X8           (7<<19)
#define  FDI_8BPC                       (0<<16)
#define  FDI_10BPC                      (1<<16)
#define  FDI_6BPC                       (2<<16)
#define  FDI_12BPC                      (3<<16)
#define  FDI_LINK_REVERSE_OVERWRITE     (1<<15)
#define  FDI_DMI_LINK_REVERSE_MASK      (1<<14)
#define  FDI_RX_PLL_ENABLE              (1<<13)
#define  FDI_FS_ERR_CORRECT_ENABLE      (1<<11)
#define  FDI_FE_ERR_CORRECT_ENABLE      (1<<10)
#define  FDI_FS_ERR_REPORT_ENABLE       (1<<9)
#define  FDI_FE_ERR_REPORT_ENABLE       (1<<8)
#define  FDI_RX_ENHANCE_FRAME_ENABLE    (1<<6)
#define  FDI_SEL_RAWCLK                 (0<<4)
#define  FDI_SEL_PCDCLK                 (1<<4)
/* CPT */
#define  FDI_AUTO_TRAINING			(1<<10)
#define  FDI_LINK_TRAIN_PATTERN_1_CPT		(0<<8)
#define  FDI_LINK_TRAIN_PATTERN_2_CPT		(1<<8)
#define  FDI_LINK_TRAIN_PATTERN_IDLE_CPT	(2<<8)
#define  FDI_LINK_TRAIN_NORMAL_CPT		(3<<8)
#define  FDI_LINK_TRAIN_PATTERN_MASK_CPT	(3<<8)

#define FDI_RXA_MISC            0xf0010
#define FDI_RXB_MISC            0xf1010
#define FDI_RXA_TUSIZE1         0xf0030
#define FDI_RXA_TUSIZE2         0xf0038
#define FDI_RXB_TUSIZE1         0xf1030
#define FDI_RXB_TUSIZE2         0xf1038

/* FDI_RX interrupt register format */
#define FDI_RX_INTER_LANE_ALIGN         (1<<10)
#define FDI_RX_SYMBOL_LOCK              (1<<9) /* train 2 */
#define FDI_RX_BIT_LOCK                 (1<<8) /* train 1 */
#define FDI_RX_TRAIN_PATTERN_2_FAIL     (1<<7)
#define FDI_RX_FS_CODE_ERR              (1<<6)
#define FDI_RX_FE_CODE_ERR              (1<<5)
#define FDI_RX_SYMBOL_ERR_RATE_ABOVE    (1<<4)
#define FDI_RX_HDCP_LINK_FAIL           (1<<3)
#define FDI_RX_PIXEL_FIFO_OVERFLOW      (1<<2)
#define FDI_RX_CROSS_CLOCK_OVERFLOW     (1<<1)
#define FDI_RX_SYMBOL_QUEUE_OVERFLOW    (1<<0)

#define FDI_RXA_IIR             0xf0014
#define FDI_RXA_IMR             0xf0018
#define FDI_RXB_IIR             0xf1014
#define FDI_RXB_IMR             0xf1018

#define FDI_PLL_CTL_1           0xfe000
#define FDI_PLL_CTL_2           0xfe004

/* CRT */
#define PCH_ADPA                0xe1100
#define  ADPA_TRANS_SELECT_MASK (1<<30)
#define  ADPA_TRANS_A_SELECT    0
#define  ADPA_TRANS_B_SELECT    (1<<30)
#define  ADPA_CRT_HOTPLUG_MASK  0x03ff0000 /* bit 25-16 */
#define  ADPA_CRT_HOTPLUG_MONITOR_NONE  (0<<24)
#define  ADPA_CRT_HOTPLUG_MONITOR_MASK  (3<<24)
#define  ADPA_CRT_HOTPLUG_MONITOR_COLOR (3<<24)
#define  ADPA_CRT_HOTPLUG_MONITOR_MONO  (2<<24)
#define  ADPA_CRT_HOTPLUG_ENABLE        (1<<23)
#define  ADPA_CRT_HOTPLUG_PERIOD_64     (0<<22)
#define  ADPA_CRT_HOTPLUG_PERIOD_128    (1<<22)
#define  ADPA_CRT_HOTPLUG_WARMUP_5MS    (0<<21)
#define  ADPA_CRT_HOTPLUG_WARMUP_10MS   (1<<21)
#define  ADPA_CRT_HOTPLUG_SAMPLE_2S     (0<<20)
#define  ADPA_CRT_HOTPLUG_SAMPLE_4S     (1<<20)
#define  ADPA_CRT_HOTPLUG_VOLTAGE_40    (0<<18)
#define  ADPA_CRT_HOTPLUG_VOLTAGE_50    (1<<18)
#define  ADPA_CRT_HOTPLUG_VOLTAGE_60    (2<<18)
#define  ADPA_CRT_HOTPLUG_VOLTAGE_70    (3<<18)
#define  ADPA_CRT_HOTPLUG_VOLREF_325MV  (0<<17)
#define  ADPA_CRT_HOTPLUG_VOLREF_475MV  (1<<17)
#define  ADPA_CRT_HOTPLUG_FORCE_TRIGGER (1<<16)

/* or SDVOB */
#define HDMIB   0xe1140
#define  PORT_ENABLE    (1 << 31)
#define  TRANSCODER_A   (0)
#define  TRANSCODER_B   (1 << 30)
#define  COLOR_FORMAT_8bpc      (0)
#define  COLOR_FORMAT_12bpc     (3 << 26)
#define  SDVOB_HOTPLUG_ENABLE   (1 << 23)
#define  SDVO_ENCODING          (0)
#define  TMDS_ENCODING          (2 << 10)
#define  NULL_PACKET_VSYNC_ENABLE       (1 << 9)
#define  SDVOB_BORDER_ENABLE    (1 << 7)
#define  AUDIO_ENABLE           (1 << 6)
#define  VSYNC_ACTIVE_HIGH      (1 << 4)
#define  HSYNC_ACTIVE_HIGH      (1 << 3)
#define  PORT_DETECTED          (1 << 2)

/* PCH SDVOB multiplex with HDMIB */
#define PCH_SDVOB	HDMIB

#define HDMIC   0xe1150
#define HDMID   0xe1160

#define PCH_LVDS	0xe1180
#define  LVDS_DETECTED	(1 << 1)

#define BLC_PWM_CPU_CTL2	0x48250
#define  PWM_ENABLE		(1 << 31)
#define  PWM_PIPE_A		(0 << 29)
#define  PWM_PIPE_B		(1 << 29)
#define BLC_PWM_CPU_CTL		0x48254

#define BLC_PWM_PCH_CTL1	0xc8250
#define  PWM_PCH_ENABLE		(1 << 31)
#define  PWM_POLARITY_ACTIVE_LOW	(1 << 29)
#define  PWM_POLARITY_ACTIVE_HIGH	(0 << 29)
#define  PWM_POLARITY_ACTIVE_LOW2	(1 << 28)
#define  PWM_POLARITY_ACTIVE_HIGH2	(0 << 28)

#define BLC_PWM_PCH_CTL2	0xc8254

#define PCH_PP_STATUS		0xc7200
#define PCH_PP_CONTROL		0xc7204
#define  EDP_FORCE_VDD		(1 << 3)
#define  EDP_BLC_ENABLE		(1 << 2)
#define  PANEL_POWER_RESET	(1 << 1)
#define  PANEL_POWER_OFF	(0 << 0)
#define  PANEL_POWER_ON		(1 << 0)
#define PCH_PP_ON_DELAYS	0xc7208
#define  EDP_PANEL		(1 << 30)
#define PCH_PP_OFF_DELAYS	0xc720c
#define PCH_PP_DIVISOR		0xc7210

#define PCH_DP_B		0xe4100
#define PCH_DPB_AUX_CH_CTL	0xe4110
#define PCH_DPB_AUX_CH_DATA1	0xe4114
#define PCH_DPB_AUX_CH_DATA2	0xe4118
#define PCH_DPB_AUX_CH_DATA3	0xe411c
#define PCH_DPB_AUX_CH_DATA4	0xe4120
#define PCH_DPB_AUX_CH_DATA5	0xe4124

#define PCH_DP_C		0xe4200
#define PCH_DPC_AUX_CH_CTL	0xe4210
#define PCH_DPC_AUX_CH_DATA1	0xe4214
#define PCH_DPC_AUX_CH_DATA2	0xe4218
#define PCH_DPC_AUX_CH_DATA3	0xe421c
#define PCH_DPC_AUX_CH_DATA4	0xe4220
#define PCH_DPC_AUX_CH_DATA5	0xe4224

#define PCH_DP_D		0xe4300
#define PCH_DPD_AUX_CH_CTL	0xe4310
#define PCH_DPD_AUX_CH_DATA1	0xe4314
#define PCH_DPD_AUX_CH_DATA2	0xe4318
#define PCH_DPD_AUX_CH_DATA3	0xe431c
#define PCH_DPD_AUX_CH_DATA4	0xe4320
#define PCH_DPD_AUX_CH_DATA5	0xe4324

/* CPT */
#define  PORT_TRANS_A_SEL_CPT	0
#define  PORT_TRANS_B_SEL_CPT	(1<<29)
#define  PORT_TRANS_C_SEL_CPT	(2<<29)
#define  PORT_TRANS_SEL_MASK	(3<<29)

#define TRANS_DP_CTL_A		0xe0300
#define TRANS_DP_CTL_B		0xe1300
#define TRANS_DP_CTL_C		0xe2300
#define  TRANS_DP_OUTPUT_ENABLE	(1<<31)
#define  TRANS_DP_PORT_SEL_B	(0<<29)
#define  TRANS_DP_PORT_SEL_C	(1<<29)
#define  TRANS_DP_PORT_SEL_D	(2<<29)
#define  TRANS_DP_PORT_SEL_MASK	(3<<29)
#define  TRANS_DP_AUDIO_ONLY	(1<<26)
#define  TRANS_DP_ENH_FRAMING	(1<<18)
#define  TRANS_DP_8BPC		(0<<9)
#define  TRANS_DP_10BPC		(1<<9)
#define  TRANS_DP_6BPC		(2<<9)
#define  TRANS_DP_12BPC		(3<<9)
#define  TRANS_DP_VSYNC_ACTIVE_HIGH	(1<<4)
#define  TRANS_DP_VSYNC_ACTIVE_LOW	0
#define  TRANS_DP_HSYNC_ACTIVE_HIGH	(1<<3)
#define  TRANS_DP_HSYNC_ACTIVE_LOW	0

/* SNB eDP training params */
/* SNB A-stepping */
#define  EDP_LINK_TRAIN_400MV_0DB_SNB_A		(0x38<<22)
#define  EDP_LINK_TRAIN_400MV_6DB_SNB_A		(0x02<<22)
#define  EDP_LINK_TRAIN_600MV_3_5DB_SNB_A	(0x01<<22)
#define  EDP_LINK_TRAIN_800MV_0DB_SNB_A		(0x0<<22)
/* SNB B-stepping */
#define  EDP_LINK_TRAIN_400MV_0DB_SNB_B		(0x0<<22)
#define  EDP_LINK_TRAIN_400MV_6DB_SNB_B		(0x3a<<22)
#define  EDP_LINK_TRAIN_600MV_3_5DB_SNB_B	(0x39<<22)
#define  EDP_LINK_TRAIN_800MV_0DB_SNB_B		(0x38<<22)
#define  EDP_LINK_TRAIN_VOL_EMP_MASK_SNB	(0x3f<<22)


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

#define HAS_RESET(dev_priv)	IS_I965G(dev_priv) && (!IS_GEN6(dev_priv))

#define IS_GEN2(dev_priv)	(dev_priv->flags & CHIP_GEN2)
#define IS_GEN3(dev_priv)	(dev_priv->flags & CHIP_GEN3)
#define IS_GEN4(dev_priv)	(dev_priv->flags & CHIP_GEN4)
#define IS_GEN6(dev_priv)	(dev_priv->flags & CHIP_GEN6)

#define HAS_PCH_SPLIT(dev_priv) (IS_IRONLAKE(dev_priv) ||	\
				 IS_GEN6(dev_priv))

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
