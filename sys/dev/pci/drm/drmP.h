/* $OpenBSD: drmP.h,v 1.219 2018/08/20 19:36:04 kettenis Exp $ */
/* drmP.h -- Private header for Direct Rendering Manager -*- linux-c -*-
 * Created: Mon Jan  4 10:05:05 1999 by faith@precisioninsight.com
 */
/*-
 * Copyright 1999 Precision Insight, Inc., Cedar Park, Texas.
 * Copyright 2000 VA Linux Systems, Inc., Sunnyvale, California.
 * All rights reserved.
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
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Rickard E. (Rik) Faith <faith@valinux.com>
 *    Gareth Hughes <gareth@valinux.com>
 *
 */

#ifndef _DRM_P_H_
#define _DRM_P_H_

#if defined(_KERNEL) || defined(__KERNEL__)

//#define DRMDEBUG

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/mutex.h>
#include <sys/tree.h>
#include <sys/endian.h>
#include <sys/stdint.h>
#include <sys/memrange.h>
#include <sys/extent.h>
#include <sys/rwlock.h>

#ifdef DDB
#include <ddb/db_var.h>
#endif

#include <uvm/uvm_extern.h>
#include <uvm/uvm_object.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/agpvar.h>
#include <machine/bus.h>

#define CONFIG_DRM_FBDEV_EMULATION 1
#define CONFIG_BACKLIGHT_CLASS_DEVICE 1

#include "drm_linux.h"
#include "drm_linux_list.h"
#include "drm.h"
#include "drm_vma_manager.h"
#include <dev/pci/drm/drm_crtc.h>
#include "drm_mm.h"
#include "drm_linux_atomic.h"
#include "agp.h"

struct fb_cmap;
struct fb_fillrect;
struct fb_copyarea;
struct fb_image;

#define drm_debug	0
#define DRM_UT_KMS	0

/***********************************************************************/
/** \name DRM template customization defaults */
/*@{*/

/* driver capabilities and requirements mask */
#define DRIVER_USE_AGP     0x1
#define DRIVER_PCI_DMA     0x8
#define DRIVER_SG          0x10
#define DRIVER_HAVE_DMA    0x20
#define DRIVER_HAVE_IRQ    0x40
#define DRIVER_IRQ_SHARED  0x80
#define DRIVER_GEM         0x1000
#define DRIVER_MODESET     0x2000
#define DRIVER_PRIME       0x4000
#define DRIVER_RENDER      0x8000
#define DRIVER_ATOMIC      0x10000

#define	DRM_DEBUGBITS_DEBUG		0x1
#define	DRM_DEBUGBITS_KMS		0x2
#define	DRM_DEBUGBITS_FAILED_IOCTL	0x4

#define __OS_HAS_AGP		(NAGP > 0)

				/* Internal types and structures */
#define DRM_IF_VERSION(maj, min) (maj << 16 | min)

#define DRM_CURRENTPID		curproc->p_p->ps_pid
#define DRM_MAXUNITS		8

/* DRM_SUSER returns true if the user is superuser */
#define DRM_SUSER(p)		(suser(p) == 0)
#define DRM_MTRR_WC		MDF_WRITECOMBINE

#define DRM_WAKEUP(x)		wakeup(x)

#define drm_msleep(x)		mdelay(x)

extern struct cfdriver drm_cd;

/* freebsd compat */
#define	TAILQ_CONCAT(head1, head2, field) do {				\
	if (!TAILQ_EMPTY(head2)) {					\
		*(head1)->tqh_last = (head2)->tqh_first;		\
		(head2)->tqh_first->field.tqe_prev = (head1)->tqh_last;	\
		(head1)->tqh_last = (head2)->tqh_last;			\
		TAILQ_INIT((head2));					\
	}								\
} while (0)

#define DRM_ARRAY_SIZE nitems

/* DRM_READMEMORYBARRIER() prevents reordering of reads.
 * DRM_WRITEMEMORYBARRIER() prevents reordering of writes.
 * DRM_MEMORYBARRIER() prevents reordering of reads and writes.
 */
#if defined(__i386__)
#define DRM_READMEMORYBARRIER()		__asm __volatile( \
					"lock; addl $0,0(%%esp)" : : : "memory");
#define DRM_WRITEMEMORYBARRIER()	__asm __volatile("" : : : "memory");
#define DRM_MEMORYBARRIER()		__asm __volatile( \
					"lock; addl $0,0(%%esp)" : : : "memory");
#elif defined(__alpha__)
#define DRM_READMEMORYBARRIER()		alpha_mb();
#define DRM_WRITEMEMORYBARRIER()	alpha_wmb();
#define DRM_MEMORYBARRIER()		alpha_mb();
#elif defined(__amd64__)
#define DRM_READMEMORYBARRIER()		__asm __volatile( \
					"lock; addl $0,0(%%rsp)" : : : "memory");
#define DRM_WRITEMEMORYBARRIER()	__asm __volatile("" : : : "memory");
#define DRM_MEMORYBARRIER()		__asm __volatile( \
					"lock; addl $0,0(%%rsp)" : : : "memory");
#elif defined(__aarch64__)
#define DRM_READMEMORYBARRIER()		__membar("dsb ld")
#define DRM_WRITEMEMORYBARRIER()	__membar("dsb st")
#define DRM_MEMORYBARRIER()		__membar("dsb sy")
#elif defined(__mips64__)
#define DRM_READMEMORYBARRIER()		DRM_MEMORYBARRIER() 
#define DRM_WRITEMEMORYBARRIER()	DRM_MEMORYBARRIER()
#define DRM_MEMORYBARRIER()		mips_sync()
#elif defined(__powerpc__)
#define DRM_READMEMORYBARRIER()		DRM_MEMORYBARRIER() 
#define DRM_WRITEMEMORYBARRIER()	DRM_MEMORYBARRIER()
#define DRM_MEMORYBARRIER()		__asm __volatile("sync" : : : "memory");
#elif defined(__sparc64__)
#define DRM_READMEMORYBARRIER()		DRM_MEMORYBARRIER() 
#define DRM_WRITEMEMORYBARRIER()	DRM_MEMORYBARRIER()
#define DRM_MEMORYBARRIER()		membar_sync()
#endif

#define smp_mb__before_atomic()		DRM_MEMORYBARRIER()
#define smp_mb__before_atomic_dec()	DRM_MEMORYBARRIER()
#define smp_mb__after_atomic_dec()	DRM_MEMORYBARRIER()
#define smp_mb__before_atomic_inc()	DRM_MEMORYBARRIER()
#define smp_mb__after_atomic_inc()	DRM_MEMORYBARRIER()

#define mb()				DRM_MEMORYBARRIER()
#define rmb()				DRM_READMEMORYBARRIER()
#define wmb()				DRM_WRITEMEMORYBARRIER()
#define smp_rmb()			DRM_READMEMORYBARRIER()
#define smp_wmb()			DRM_WRITEMEMORYBARRIER()
#define mmiowb()			DRM_WRITEMEMORYBARRIER()

#define	DRM_COPY_TO_USER(user, kern, size)	copyout(kern, user, size)
#define	DRM_COPY_FROM_USER(kern, user, size)	copyin(user, kern, size)

#define DRM_UDELAY(udelay)	DELAY(udelay)

static inline bool
drm_can_sleep(void)
{
#if defined(__i386__) || defined(__amd64__)
	if (pagefault_disabled() || in_dbg_master() || irqs_disabled())
#else
	if (in_dbg_master() || irqs_disabled())
#endif
		return false;
	return true;
}

#define DRM_WAIT_ON(ret, wq, timo, condition) do {			\
	ret = wait_event_interruptible_timeout(wq, condition, timo);	\
	if (ret == 0)							\
		ret = -EBUSY;						\
	if (ret > 0)							\
		ret = 0;						\
} while (0)

#define DRM_ERROR(fmt, arg...) \
	printf("error: [" DRM_NAME ":pid%d:%s] *ERROR* " fmt,		\
	    curproc->p_p->ps_pid, __func__ , ## arg)


#ifdef DRMDEBUG
#define DRM_INFO(fmt, arg...)  printf("drm: " fmt, ## arg)
#define DRM_INFO_ONCE(fmt, arg...)  printf("drm: " fmt, ## arg)
#else
#define DRM_INFO(fmt, arg...) do { } while(/* CONSTCOND */ 0)
#define DRM_INFO_ONCE(fmt, arg...) do { } while(/* CONSTCOND */ 0)
#endif

#ifdef DRMDEBUG
#undef DRM_DEBUG
#define DRM_DEBUG(fmt, arg...) do {					\
	if (drm_debug_flag)						\
		printf("[" DRM_NAME ":pid%d:%s] " fmt,			\
		    curproc->p_p->ps_pid, __func__ , ## arg);		\
} while (0)
#else
#define DRM_DEBUG(fmt, arg...) do { } while(/* CONSTCOND */ 0)
#endif

#ifdef DRMDEBUG
#undef DRM_DEBUG_KMS
#define DRM_DEBUG_KMS(fmt, arg...) do {					\
	if (drm_debug_flag)						\
		printf("[" DRM_NAME ":pid%d:%s] " fmt,			\
		    curproc->p_p->ps_pid, __func__ , ## arg);		\
} while (0)
#else
#define DRM_DEBUG_KMS(fmt, arg...) do { } while(/* CONSTCOND */ 0)
#endif

#ifdef DRMDEBUG
#undef DRM_LOG_KMS
#define DRM_LOG_KMS(fmt, arg...) do {					\
	if (drm_debug_flag)						\
		printf(fmt, ## arg);					\
} while (0)
#else
#define DRM_LOG_KMS(fmt, arg...) do { } while(/* CONSTCOND */ 0)
#endif

#ifdef DRMDEBUG
#undef DRM_DEBUG_DRIVER
#define DRM_DEBUG_DRIVER(fmt, arg...) do {					\
	if (drm_debug_flag)						\
		printf("[" DRM_NAME ":pid%d:%s] " fmt,			\
		    curproc->p_p->ps_pid, __func__ , ## arg);		\
} while (0)
#else
#define DRM_DEBUG_DRIVER(fmt, arg...) do { } while(/* CONSTCOND */ 0)
#endif

#ifdef DRMDEBUG
#undef DRM_DEBUG_ATOMIC
#define DRM_DEBUG_ATOMIC(fmt, arg...) do {					\
	if (drm_debug_flag)						\
		printf("[" DRM_NAME ":pid%d:%s] " fmt,			\
		    curproc->p_p->ps_pid, __func__ , ## arg);		\
} while (0)
#else
#define DRM_DEBUG_ATOMIC(fmt, arg...) do { } while(0)
#endif

#define DRM_DEBUG_VBL(fmt, arg...) do {} while(0)

struct drm_pcidev {
	uint16_t vendor;
	uint16_t device;
	uint16_t subvendor;
	uint16_t subdevice;
	uint32_t class;
	uint32_t class_mask;
	unsigned long driver_data;
};

struct drm_file;
struct drm_device;

/**
 * Ioctl function type.
 *
 * \param inode device inode.
 * \param file_priv DRM file private pointer.
 * \param cmd command.
 * \param arg argument.
 */
typedef int drm_ioctl_t(struct drm_device *dev, void *data,
			struct drm_file *file_priv);

typedef int drm_ioctl_compat_t(struct file *filp, unsigned int cmd,
			       unsigned long arg);

#define DRM_AUTH	0x1
#define DRM_MASTER	0x2
#define DRM_ROOT_ONLY	0x4
#define DRM_CONTROL_ALLOW 0x8
#define DRM_UNLOCKED	0x10
#define DRM_RENDER_ALLOW 0x20

struct drm_ioctl_desc {
	unsigned int cmd;
	int flags;
	drm_ioctl_t *func;
	unsigned int cmd_drv;
};

/**
 * Creates a driver or general drm_ioctl_desc array entry for the given
 * ioctl, for use by drm_ioctl().
 */

#define DRM_IOCTL_DEF_DRV(ioctl, _func, _flags)			\
	[DRM_IOCTL_NR(DRM_##ioctl)] = {.cmd = DRM_##ioctl, .func = _func, .flags = _flags, .cmd_drv = DRM_IOCTL_##ioctl}

struct drm_dmamem {
	bus_dmamap_t		map;
	caddr_t			kva;
	bus_size_t		size;
	int			nsegs;
	bus_dma_segment_t	segs[1];
};

typedef struct drm_dma_handle {
	struct drm_dmamem *mem;
	dma_addr_t busaddr;
	void *vaddr;
	size_t size;
} drm_dma_handle_t;

struct drm_pending_event {
	struct drm_event *event;
	struct list_head link;
	struct drm_file *file_priv;
	pid_t pid; /* pid of requester, no guarantee it's valid by the time
		      we deliver the event, for tracing only */
	void (*destroy)(struct drm_pending_event *event);
};

/* initial implementaton using a linked list - todo hashtab */
struct drm_prime_file_private {
	struct list_head head;
	struct rwlock lock;
};

/** File private data */
struct drm_file {
	unsigned always_authenticated :1;
	unsigned authenticated :1;
	unsigned is_master :1; /* this file private is a master for a minor */
	/* true when the client has asked us to expose stereo 3D mode flags */
	unsigned stereo_allowed :1;
	/*
	 * true if client understands CRTC primary planes and cursor planes
	 * in the plane list
	 */
	unsigned universal_planes:1;
	/* true if client understands atomic properties */
	unsigned atomic:1;

	drm_magic_t magic;
	int minor;

	/** Mapping of mm object handles to object pointers. */
	struct idr object_idr;
	/** Lock for synchronization of access to object_idr. */
	spinlock_t table_lock;

	struct file *filp;
	void *driver_priv;

	/**
	 * fbs - List of framebuffers associated with this file.
	 *
	 * Protected by fbs_lock. Note that the fbs list holds a reference on
	 * the fb object to prevent it from untimely disappearing.
	 */
	struct list_head fbs;
	struct rwlock fbs_lock;

	/** User-created blob properties; this retains a reference on the
	 *  property. */
	struct list_head blobs;

	wait_queue_head_t event_wait;
	struct list_head event_list;
	int event_space;

	struct drm_prime_file_private prime;

	struct selinfo rsel;
	SPLAY_ENTRY(drm_file) link;
};

struct drm_agp_head {
	struct agp_softc			*agpdev;
	const char				*chipset;
	TAILQ_HEAD(agp_memlist, drm_agp_mem)	 memory;
	struct agp_info				 info;
	unsigned long				 base;
	unsigned long				 mode;
	unsigned long				 page_mask;
	int					 acquired;
	int					 cant_use_aperture;
	int					 enabled;
   	int					 mtrr;
};

/* location of GART table */
#define DRM_ATI_GART_MAIN 1
#define DRM_ATI_GART_FB   2

#define DRM_ATI_GART_PCI  1
#define DRM_ATI_GART_PCIE 2
#define DRM_ATI_GART_IGP  3
#define DRM_ATI_GART_R600 4

/**
 * This structure defines the drm_mm memory object, which will be used by the
 * DRM for its buffer objects.
 */
struct drm_gem_object {
	/** Reference count of this object */
	struct kref refcount;

	/**
	 * handle_count - gem file_priv handle count of this object
	 *
	 * Each handle also holds a reference. Note that when the handle_count
	 * drops to 0 any global names (e.g. the id in the flink namespace) will
	 * be cleared.
	 *
	 * Protected by dev->object_name_lock.
	 * */
	unsigned handle_count;

	/** Related drm device */
	struct drm_device *dev;

	/** File representing the shmem storage */
	struct file *filp;

	/* Mapping info for this object */
	struct drm_vma_offset_node vma_node;

	/**
	 * Size of the object, in bytes.  Immutable over the object's
	 * lifetime.
	 */
	size_t size;

	/**
	 * Global name for this object, starts at 1. 0 means unnamed.
	 * Access is covered by the object_name_lock in the related drm_device
	 */
	int name;

	/**
	 * Memory domains. These monitor which caches contain read/write data
	 * related to the object. When transitioning from one set of domains
	 * to another, the driver is called to ensure that caches are suitably
	 * flushed and invalidated
	 */
	uint32_t read_domains;
	uint32_t write_domain;

	/**
	 * While validating an exec operation, the
	 * new read/write domain values are computed here.
	 * They will be transferred to the above values
	 * at the point that any cache flushing occurs
	 */
	uint32_t pending_read_domains;
	uint32_t pending_write_domain;

	/**
	 * dma_buf - dma buf associated with this GEM object
	 *
	 * Pointer to the dma-buf associated with this gem object (either
	 * through importing or exporting). We break the resulting reference
	 * loop when the last gem handle for this object is released.
	 *
	 * Protected by obj->object_name_lock
	 */
	struct dma_buf *dma_buf;

	/**
	 * import_attach - dma buf attachment backing this object
	 *
	 * Any foreign dma_buf imported as a gem object has this set to the
	 * attachment point for the device. This is invariant over the lifetime
	 * of a gem object.
	 *
	 * The driver's ->gem_free_object callback is responsible for cleaning
	 * up the dma_buf attachment and references acquired at import time.
	 *
	 * Note that the drm gem/prime core does not depend upon drivers setting
	 * this field any more. So for drivers where this doesn't make sense
	 * (e.g. virtual devices or a displaylink behind an usb bus) they can
	 * simply leave it as NULL.
	 */
	struct dma_buf_attachment *import_attach;

	struct uvm_object uobj;
	SPLAY_ENTRY(drm_gem_object) entry;
	struct uvm_object *uao;
};

/* Size of ringbuffer for vblank timestamps. Just double-buffer
 * in initial implementation.
 */
#define DRM_VBLANKTIME_RBSIZE 2

/* Flags and return codes for get_vblank_timestamp() driver function. */
#define DRM_CALLED_FROM_VBLIRQ 1
#define DRM_VBLANKTIME_SCANOUTPOS_METHOD (1 << 0)
#define DRM_VBLANKTIME_IN_VBLANK         (1 << 1)

/* get_scanout_position() return flags */
#define DRM_SCANOUTPOS_VALID        (1 << 0)
#define DRM_SCANOUTPOS_IN_VBLANK    (1 << 1)
#define DRM_SCANOUTPOS_ACCURATE     (1 << 2)

struct drm_driver {
	int	(*firstopen)(struct drm_device *);
	int	(*open)(struct drm_device *, struct drm_file *);
	void	(*close)(struct drm_device *, struct drm_file *);
	void	(*preclose)(struct drm_device *, struct drm_file *);
	void	(*postclose)(struct drm_device *, struct drm_file *);
	void	(*lastclose)(struct drm_device *);
	struct uvm_object *(*mmap)(struct drm_device *, voff_t, vsize_t);
	int	(*dma_ioctl)(struct drm_device *, struct drm_dma *,
		    struct drm_file *);
	int	(*irq_handler)(int, void *);
	void	(*irq_preinstall) (struct drm_device *);
	int	(*irq_install)(struct drm_device *);
	int	(*irq_postinstall) (struct drm_device *);
	void	(*irq_uninstall)(struct drm_device *);
	int	vblank_pipes;

	/**
	 * get_vblank_counter - get raw hardware vblank counter
	 * @dev: DRM device
	 * @pipe: counter to fetch
	 *
	 * Driver callback for fetching a raw hardware vblank counter for @crtc.
	 * If a device doesn't have a hardware counter, the driver can simply
	 * return the value of drm_vblank_count. The DRM core will account for
	 * missed vblank events while interrupts where disabled based on system
	 * timestamps.
	 *
	 * Wraparound handling and loss of events due to modesetting is dealt
	 * with in the DRM core code.
	 *
	 * RETURNS
	 * Raw vblank counter value.
	 */
	u32 (*get_vblank_counter) (struct drm_device *dev, unsigned int pipe);

	/**
	 * enable_vblank - enable vblank interrupt events
	 * @dev: DRM device
	 * @pipe: which irq to enable
	 *
	 * Enable vblank interrupts for @crtc.  If the device doesn't have
	 * a hardware vblank counter, this routine should be a no-op, since
	 * interrupts will have to stay on to keep the count accurate.
	 *
	 * RETURNS
	 * Zero on success, appropriate errno if the given @crtc's vblank
	 * interrupt cannot be enabled.
	 */
	int (*enable_vblank) (struct drm_device *dev, unsigned int pipe);

	/**
	 * disable_vblank - disable vblank interrupt events
	 * @dev: DRM device
	 * @pipe: which irq to enable
	 *
	 * Disable vblank interrupts for @crtc.  If the device doesn't have
	 * a hardware vblank counter, this routine should be a no-op, since
	 * interrupts will have to stay on to keep the count accurate.
	 */
	void (*disable_vblank) (struct drm_device *dev, unsigned int pipe);

	/**
	 * Called by vblank timestamping code.
	 *
	 * Return the current display scanout position from a crtc, and an
	 * optional accurate ktime_get timestamp of when position was measured.
	 *
	 * \param dev  DRM device.
	 * \param pipe Id of the crtc to query.
	 * \param flags Flags from the caller (DRM_CALLED_FROM_VBLIRQ or 0).
	 * \param *vpos Target location for current vertical scanout position.
	 * \param *hpos Target location for current horizontal scanout position.
	 * \param *stime Target location for timestamp taken immediately before
	 *               scanout position query. Can be NULL to skip timestamp.
	 * \param *etime Target location for timestamp taken immediately after
	 *               scanout position query. Can be NULL to skip timestamp.
	 * \param mode Current display timings.
	 *
	 * Returns vpos as a positive number while in active scanout area.
	 * Returns vpos as a negative number inside vblank, counting the number
	 * of scanlines to go until end of vblank, e.g., -1 means "one scanline
	 * until start of active scanout / end of vblank."
	 *
	 * \return Flags, or'ed together as follows:
	 *
	 * DRM_SCANOUTPOS_VALID = Query successful.
	 * DRM_SCANOUTPOS_INVBL = Inside vblank.
	 * DRM_SCANOUTPOS_ACCURATE = Returned position is accurate. A lack of
	 * this flag means that returned position may be offset by a constant
	 * but unknown small number of scanlines wrt. real scanout position.
	 *
	 */
	int (*get_scanout_position) (struct drm_device *dev, unsigned int pipe,
				     unsigned int flags, int *vpos, int *hpos,
				     ktime_t *stime, ktime_t *etime,
				     const struct drm_display_mode *mode);

	/**
	 * Called by \c drm_get_last_vbltimestamp. Should return a precise
	 * timestamp when the most recent VBLANK interval ended or will end.
	 *
	 * Specifically, the timestamp in @vblank_time should correspond as
	 * closely as possible to the time when the first video scanline of
	 * the video frame after the end of VBLANK will start scanning out,
	 * the time immediately after end of the VBLANK interval. If the
	 * @crtc is currently inside VBLANK, this will be a time in the future.
	 * If the @crtc is currently scanning out a frame, this will be the
	 * past start time of the current scanout. This is meant to adhere
	 * to the OpenML OML_sync_control extension specification.
	 *
	 * \param dev dev DRM device handle.
	 * \param pipe crtc for which timestamp should be returned.
	 * \param *max_error Maximum allowable timestamp error in nanoseconds.
	 *                   Implementation should strive to provide timestamp
	 *                   with an error of at most *max_error nanoseconds.
	 *                   Returns true upper bound on error for timestamp.
	 * \param *vblank_time Target location for returned vblank timestamp.
	 * \param flags 0 = Defaults, no special treatment needed.
	 * \param       DRM_CALLED_FROM_VBLIRQ = Function is called from vblank
	 *	        irq handler. Some drivers need to apply some workarounds
	 *              for gpu-specific vblank irq quirks if flag is set.
	 *
	 * \returns
	 * Zero if timestamping isn't supported in current display mode or a
	 * negative number on failure. A positive status code on success,
	 * which describes how the vblank_time timestamp was computed.
	 */
	int (*get_vblank_timestamp) (struct drm_device *dev, unsigned int pipe,
				     int *max_error,
				     struct timeval *vblank_time,
				     unsigned flags);

	/**
	 * Driver-specific constructor for drm_gem_objects, to set up
	 * obj->driver_private.
	 *
	 * Returns 0 on success.
	 */
	void (*gem_free_object) (struct drm_gem_object *obj);
	int (*gem_open_object) (struct drm_gem_object *, struct drm_file *);
	void (*gem_close_object) (struct drm_gem_object *, struct drm_file *);

	int	(*gem_fault)(struct drm_gem_object *, struct uvm_faultinfo *,
		    off_t, vaddr_t, vm_page_t *, int, int, vm_prot_t, int);

	/* prime: */
	/* export handle -> fd (see drm_gem_prime_handle_to_fd() helper) */
	int (*prime_handle_to_fd)(struct drm_device *dev, struct drm_file *file_priv,
				uint32_t handle, uint32_t flags, int *prime_fd);
	/* import fd -> handle (see drm_gem_prime_fd_to_handle() helper) */
	int (*prime_fd_to_handle)(struct drm_device *dev, struct drm_file *file_priv,
				int prime_fd, uint32_t *handle);
	/* export GEM -> dmabuf */
	struct dma_buf * (*gem_prime_export)(struct drm_device *dev,
				struct drm_gem_object *obj, int flags);
	/* import dmabuf -> GEM */
	struct drm_gem_object * (*gem_prime_import)(struct drm_device *dev,
				struct dma_buf *dma_buf);

	int	(*dumb_create)(struct drm_file *file_priv,
		    struct drm_device *dev, struct drm_mode_create_dumb *args);
	int	(*dumb_map_offset)(struct drm_file *file_priv,
		    struct drm_device *dev, uint32_t handle, uint64_t *offset);
	int	(*dumb_destroy)(struct drm_file *file_priv,
		    struct drm_device *dev, uint32_t handle);

	size_t	gem_size;
	size_t	buf_priv_size;
	size_t	file_priv_size;

	int	major;
	int	minor;
	int	patchlevel;
	const char *name;		/* Simple driver name		   */
	const char *desc;		/* Longer driver name		   */
	const char *date;		/* Date of last major changes.	   */

	u32 driver_features;
	const struct drm_ioctl_desc *ioctls;
	int num_ioctls;

};

enum drm_minor_type {
	DRM_MINOR_LEGACY,
	DRM_MINOR_CONTROL,
	DRM_MINOR_RENDER,
	DRM_MINOR_CNT,
};

#include "drm_crtc.h"

#if 0
/* mode specified on the command line */
struct drm_cmdline_mode {
	bool specified;
	bool refresh_specified;
	bool bpp_specified;
	int xres, yres;
	int bpp;
	int refresh;
	bool rb;
	bool interlace;
	bool cvt;
	bool margins;
	enum drm_connector_force force;
};
#endif

struct drm_minor;

struct drm_pending_vblank_event {
	struct drm_pending_event base;
	int pipe;
	struct drm_event_vblank event;
};

struct drm_vblank_crtc {
	struct drm_device *dev;		/* pointer to the drm_device */
	wait_queue_head_t queue;	/**< VBLANK wait queue */
	struct timeout disable_timer;		/* delayed disable timer */

	/* vblank counter, protected by dev->vblank_time_lock for writes */
	u32 count;
	/* vblank timestamps, protected by dev->vblank_time_lock for writes */
	struct timeval time[DRM_VBLANKTIME_RBSIZE];

	atomic_t refcount;		/* number of users of vblank interruptsper crtc */
	u32 last;			/* protected by dev->vbl_lock, used */
					/* for wraparound handling */
	u32 last_wait;			/* Last vblank seqno waited per CRTC */
	unsigned int inmodeset;		/* Display driver is setting mode */
	unsigned int pipe;		/* crtc index */
	int framedur_ns;		/* frame/field duration in ns */
	int linedur_ns;			/* line duration in ns */
	bool enabled;			/* so we don't call enable more than
					   once per disable */
};

/** 
 * DRM device functions structure
 */
struct drm_device {
	struct device	 device; /* softc is an extension of struct device */
	struct device	*dev;

	struct drm_driver *driver;

	struct klist	 note;

	struct pci_dev	_pdev;
	struct pci_dev	*pdev;
	u_int16_t	 pci_device;
	u_int16_t	 pci_vendor;

	pci_chipset_tag_t		 pc;
	pcitag_t	 		*bridgetag;

	bus_dma_tag_t			dmat;
	bus_space_tag_t			bst;

	struct mutex	quiesce_mtx;
	int		quiesce;
	int		quiesce_count;

	char		  *unique;	/* Unique identifier: e.g., busid  */
	int		  unique_len;	/* Length of unique field	   */
	
	int		  if_version;	/* Highest interface version set */
				/* Locks */
	struct rwlock	  struct_mutex;	/* protects everything else */

				/* Usage Counters */
	int		  open_count;	/* Outstanding files open	   */

				/* Authentication */
	SPLAY_HEAD(drm_file_tree, drm_file)	files;
	drm_magic_t	  magicid;

				/* Context support */

	/** \name VBLANK IRQ support */
	/*@{ */
	bool irq_enabled;
	int irq;

	/*
	 * At load time, disabling the vblank interrupt won't be allowed since
	 * old clients may not call the modeset ioctl and therefore misbehave.
	 * Once the modeset ioctl *has* been called though, we can safely
	 * disable them when unused.
	 */
	bool vblank_disable_allowed;

	/*
	 * If true, vblank interrupt will be disabled immediately when the
	 * refcount drops to zero, as opposed to via the vblank disable
	 * timer.
	 * This can be set to true it the hardware has a working vblank
	 * counter and the driver uses drm_vblank_on() and drm_vblank_off()
	 * appropriately.
	 */
	bool vblank_disable_immediate;

	/* array of size num_crtcs */
	struct drm_vblank_crtc *vblank;

	struct mutex vblank_time_lock;    /**< Protects vblank count and time updates during vblank enable/disable */
	struct mutex vbl_lock;

	u32 max_vblank_count;           /**< size of vblank counter register */

	/**
	 * List of events
	 */
	struct list_head vblank_event_list;
	spinlock_t event_lock;

	/*@} */

	int			*vblank_enabled;
	int			*vblank_inmodeset;
	u32			*last_vblank_wait;

	int			 num_crtcs;

	pid_t			 buf_pgid;

	struct drm_agp_head	*agp;
	void			*dev_private;
	struct address_space	*dev_mapping;
	struct drm_local_map	*agp_buffer_map;

	struct drm_mode_config	 mode_config; /* Current mode config */

	/* GEM info */
	atomic_t		 obj_count;
	u_int			 obj_name;
	atomic_t		 obj_memory;
	struct pool				objpl;

	/** \name GEM information */
	/*@{ */
	struct rwlock object_name_lock;
	struct idr object_name_idr;
	struct drm_vma_offset_manager *vma_offset_manager;
	/*@} */
};

struct drm_attach_args {
	struct drm_driver		*driver;
	char				*busid;
	bus_dma_tag_t			 dmat;
	bus_space_tag_t			 bst;
	size_t				 busid_len;
	int				 is_agp;
	u_int16_t			 pci_vendor;
	u_int16_t			 pci_device;
	u_int16_t			 pci_subvendor;
	u_int16_t			 pci_subdevice;
	u_int8_t			 pci_revision;
	struct pci_attach_args		*pa;
	pci_chipset_tag_t		 pc;
	pcitag_t			 tag;
	pcitag_t			*bridgetag;
	int				 console;
};

#define DRMDEVCF_CONSOLE	0
#define drmdevcf_console	cf_loc[DRMDEVCF_CONSOLE]
/* spec'd as console? */
#define DRMDEVCF_CONSOLE_UNK	-1

extern int	drm_debug_flag;

/* Device setup support (drm_drv.c) */
int	drm_pciprobe(struct pci_attach_args *, const struct drm_pcidev * );
struct device	*drm_attach_pci(struct drm_driver *, 
		     struct pci_attach_args *, int, int, struct device *);
dev_type_ioctl(drmioctl);
dev_type_read(drmread);
dev_type_poll(drmpoll);
dev_type_open(drmopen);
dev_type_close(drmclose);
dev_type_mmap(drmmmap);
struct drm_local_map	*drm_getsarea(struct drm_device *);
struct drm_dmamem	*drm_dmamem_alloc(bus_dma_tag_t, bus_size_t, bus_size_t,
			     int, bus_size_t, int, int);
void			 drm_dmamem_free(bus_dma_tag_t, struct drm_dmamem *);

extern struct drm_dma_handle *drm_pci_alloc(struct drm_device *dev, size_t size,
					    size_t align);
extern void drm_pci_free(struct drm_device *dev, struct drm_dma_handle * dmah);

const struct drm_pcidev	*drm_find_description(int , int ,
			     const struct drm_pcidev *);
int	 drm_order(unsigned long);

/* File operations helpers (drm_fops.c) */
struct drm_file	*drm_find_file_by_minor(struct drm_device *, int);
struct drm_device *drm_get_device_from_kdev(dev_t);

/* Memory management support (drm_memory.c) */
void	*drm_alloc(size_t);
void	*drm_calloc(size_t, size_t);
void	*drm_realloc(void *, size_t, size_t);
void	 drm_free(void *);

#include "drm_mem_util.h"

/* XXX until we get PAT support */
#define drm_core_ioremap_wc drm_core_ioremap
void	drm_core_ioremap(struct drm_local_map *, struct drm_device *);
void	drm_core_ioremapfree(struct drm_local_map *, struct drm_device *);

int	drm_mtrr_add(unsigned long, size_t, int);
int	drm_mtrr_del(int, unsigned long, size_t, int);

/* Misc. IOCTL support (drm_ioctl.c) */
int drm_noop(struct drm_device *dev, void *data,
	     struct drm_file *file_priv);
int drm_invalid_op(struct drm_device *dev, void *data,
		   struct drm_file *file_priv);

/* Cache management (drm_cache.c) */
void drm_clflush_pages(struct vm_page *pages[], unsigned long num_pages);
void drm_clflush_sg(struct sg_table *st);
void drm_clflush_virt_range(void *addr, unsigned long length);

/*
 * These are exported to drivers so that they can implement fencing using
 * DMA quiscent + idle. DMA quiescent usually requires the hardware lock.
 */

				/* IRQ support (drm_irq.h) */
extern int drm_irq_install(struct drm_device *dev, int irq);
extern int drm_irq_uninstall(struct drm_device *dev);

extern int drm_vblank_init(struct drm_device *dev, unsigned int num_crtcs);
extern int drm_wait_vblank(struct drm_device *dev, void *data,
			   struct drm_file *filp);
extern u32 drm_vblank_count(struct drm_device *dev, unsigned int pipe);
extern u32 drm_crtc_vblank_count(struct drm_crtc *crtc);
extern u32 drm_vblank_count_and_time(struct drm_device *dev, unsigned int pipe,
				     struct timeval *vblanktime);
extern u32 drm_crtc_vblank_count_and_time(struct drm_crtc *crtc,
					  struct timeval *vblanktime);
extern void drm_send_vblank_event(struct drm_device *dev, unsigned int pipe,
				  struct drm_pending_vblank_event *e);
extern void drm_crtc_send_vblank_event(struct drm_crtc *crtc,
				       struct drm_pending_vblank_event *e);
extern void drm_arm_vblank_event(struct drm_device *dev, unsigned int pipe,
				 struct drm_pending_vblank_event *e);
extern void drm_crtc_arm_vblank_event(struct drm_crtc *crtc,
				      struct drm_pending_vblank_event *e);
extern bool drm_handle_vblank(struct drm_device *dev, unsigned int pipe);
extern bool drm_crtc_handle_vblank(struct drm_crtc *crtc);
extern int drm_vblank_get(struct drm_device *dev, unsigned int pipe);
extern void drm_vblank_put(struct drm_device *dev, unsigned int pipe);
extern int drm_crtc_vblank_get(struct drm_crtc *crtc);
extern void drm_crtc_vblank_put(struct drm_crtc *crtc);
extern void drm_wait_one_vblank(struct drm_device *dev, unsigned int pipe);
extern void drm_crtc_wait_one_vblank(struct drm_crtc *crtc);
extern void drm_vblank_off(struct drm_device *dev, unsigned int pipe);
extern void drm_vblank_on(struct drm_device *dev, unsigned int pipe);
extern void drm_crtc_vblank_off(struct drm_crtc *crtc);
extern void drm_crtc_vblank_reset(struct drm_crtc *crtc);
extern void drm_crtc_vblank_on(struct drm_crtc *crtc);
extern void drm_vblank_cleanup(struct drm_device *dev);
extern u32 drm_vblank_no_hw_counter(struct drm_device *dev, unsigned int pipe);

extern int drm_calc_vbltimestamp_from_scanoutpos(struct drm_device *dev,
						 unsigned int pipe, int *max_error,
						 struct timeval *vblank_time,
						 unsigned flags,
						 const struct drm_display_mode *mode);
extern void drm_calc_timestamping_constants(struct drm_crtc *crtc,
					    const struct drm_display_mode *mode);

/**
 * drm_crtc_vblank_waitqueue - get vblank waitqueue for the CRTC
 * @crtc: which CRTC's vblank waitqueue to retrieve
 *
 * This function returns a pointer to the vblank waitqueue for the CRTC.
 * Drivers can use this to implement vblank waits using wait_event() & co.
 */
static inline wait_queue_head_t *drm_crtc_vblank_waitqueue(struct drm_crtc *crtc)
{
	return &crtc->dev->vblank[drm_crtc_index(crtc)].queue;
}

/* Modesetting support */
extern void drm_vblank_pre_modeset(struct drm_device *dev, unsigned int pipe);
extern void drm_vblank_post_modeset(struct drm_device *dev, unsigned int pipe);

extern struct dma_buf *drm_gem_prime_export(struct drm_device *dev,
					    struct drm_gem_object *obj,
					    int flags);
extern int drm_gem_prime_handle_to_fd(struct drm_device *dev,
		struct drm_file *file_priv, uint32_t handle, uint32_t flags,
		int *prime_fd);
extern struct drm_gem_object *drm_gem_prime_import(struct drm_device *dev,
		struct dma_buf *dma_buf);
extern int drm_gem_prime_fd_to_handle(struct drm_device *dev,
		struct drm_file *file_priv, int prime_fd, uint32_t *handle);
extern void drm_gem_dmabuf_release(struct dma_buf *dma_buf);

bool	drm_mode_parse_command_line_for_connector(const char *,
	    struct drm_connector *, struct drm_cmdline_mode *);
struct drm_display_mode *
	 drm_mode_create_from_cmdline_mode(struct drm_device *,
	     struct drm_cmdline_mode *);

extern unsigned int drm_timestamp_monotonic;

/* AGP/PCI Express/GART support (drm_agpsupport.c) */
struct drm_agp_head *drm_agp_init(void);
void	drm_agp_takedown(struct drm_device *);
int	drm_agp_acquire(struct drm_device *);
int	drm_agp_release(struct drm_device *);
int	drm_agp_info(struct drm_device *, struct drm_agp_info *);
int	drm_agp_enable(struct drm_device *, struct drm_agp_mode);
void	*drm_agp_allocate_memory(size_t, u32);
int	drm_agp_free_memory(void *);
int	drm_agp_bind_memory(void *, off_t);
int	drm_agp_unbind_memory(void *);
int	drm_agp_alloc(struct drm_device *, struct drm_agp_buffer *);
int	drm_agp_free(struct drm_device *, struct drm_agp_buffer *);
int	drm_agp_bind(struct drm_device *, struct drm_agp_binding *);
int	drm_agp_unbind(struct drm_device *, struct drm_agp_binding *);

/* AGP/GART support (drm_agpsupport.c) */
int	drm_agp_acquire_ioctl(struct drm_device *, void *, struct drm_file *);
int	drm_agp_release_ioctl(struct drm_device *, void *, struct drm_file *);
int	drm_agp_enable_ioctl(struct drm_device *, void *, struct drm_file *);
int	drm_agp_info_ioctl(struct drm_device *, void *, struct drm_file *);
int	drm_agp_alloc_ioctl(struct drm_device *, void *, struct drm_file *);
int	drm_agp_free_ioctl(struct drm_device *, void *, struct drm_file *);
int	drm_agp_unbind_ioctl(struct drm_device *, void *, struct drm_file *);
int	drm_agp_bind_ioctl(struct drm_device *, void *, struct drm_file *);

/* hotplug support */
void	drm_sysfs_hotplug_event(struct drm_device *);

static inline int
drm_sysfs_connector_add(struct drm_connector *connector)
{
	return 0;
}

static inline void
drm_sysfs_connector_remove(struct drm_connector *connector)
{
}

/* Graphics Execution Manager library functions (drm_gem.c) */
int drm_gem_init(struct drm_device *dev);
void drm_gem_destroy(struct drm_device *dev);
void drm_gem_object_release(struct drm_gem_object *obj);
void drm_gem_object_free(struct kref *kref);
int drm_gem_object_init(struct drm_device *dev,
			struct drm_gem_object *obj, size_t size);
void drm_gem_private_object_init(struct drm_device *dev,
				 struct drm_gem_object *obj, size_t size);

int drm_gem_handle_create_tail(struct drm_file *file_priv,
			       struct drm_gem_object *obj,
			       u32 *handlep);
int drm_gem_handle_create(struct drm_file *file_priv,
			  struct drm_gem_object *obj,
			  u32 *handlep);
int drm_gem_handle_delete(struct drm_file *filp, u32 handle);

void drm_gem_free_mmap_offset(struct drm_gem_object *obj);
int drm_gem_create_mmap_offset(struct drm_gem_object *obj);

struct drm_gem_object *drm_gem_object_lookup(struct drm_device *dev,
					     struct drm_file *filp,
					     u32 handle);
struct drm_gem_object *drm_gem_object_find(struct drm_file *, u32);
int drm_gem_close_ioctl(struct drm_device *dev, void *data,
			struct drm_file *file_priv);
int drm_gem_flink_ioctl(struct drm_device *dev, void *data,
			struct drm_file *file_priv);
int drm_gem_open_ioctl(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);
void drm_gem_open(struct drm_device *dev, struct drm_file *file_private);
void drm_gem_release(struct drm_device *dev,struct drm_file *file_private);

static __inline void
drm_gem_object_reference(struct drm_gem_object *obj)
{
	kref_get(&obj->refcount);
}

static __inline void
drm_gem_object_unreference(struct drm_gem_object *obj)
{
	if (obj != NULL)
		kref_put(&obj->refcount, drm_gem_object_free);
}

static __inline void
drm_gem_object_unreference_unlocked(struct drm_gem_object *obj)
{
	if (obj && !atomic_add_unless(&obj->refcount.refcount, -1, 1)) {
		struct drm_device *dev = obj->dev;

		mutex_lock(&dev->struct_mutex);
		if (likely(atomic_dec_and_test(&obj->refcount.refcount)))
			drm_gem_object_free(&obj->refcount);
		mutex_unlock(&dev->struct_mutex);
	}
}

int drm_gem_dumb_destroy(struct drm_file *file,
			 struct drm_device *dev,
			 uint32_t handle);

static __inline__ int drm_core_check_feature(struct drm_device *dev,
					     int feature)
{
	return ((dev->driver->driver_features & feature) ? 1 : 0);
}

static inline bool drm_is_render_client(const struct drm_file *file_priv)
{
#ifdef notyet
	return file_priv->minor->type == DRM_MINOR_RENDER;
#else
	return 0;
#endif
}

static inline bool drm_is_control_client(const struct drm_file *file_priv)
{
#ifdef notyet
	return file_priv->minor->type == DRM_MINOR_CONTROL;
#else
	return 0;
#endif
}

static inline bool drm_is_primary_client(const struct drm_file *file_priv)
{
#ifdef notyet
	return file_priv->minor->type == DRM_MINOR_LEGACY;
#else
	return 1;
#endif
}

static inline int drm_dev_to_irq(struct drm_device *dev)
{
	return -1;
}

#define DRM_PCIE_SPEED_25 1
#define DRM_PCIE_SPEED_50 2
#define DRM_PCIE_SPEED_80 4

int	 drm_pcie_get_speed_cap_mask(struct drm_device *, u32 *);

#endif /* __KERNEL__ */
#endif /* _DRM_P_H_ */
