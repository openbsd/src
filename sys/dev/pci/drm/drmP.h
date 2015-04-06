/* $OpenBSD: drmP.h,v 1.184 2015/04/06 10:56:37 jsg Exp $ */
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

#include <uvm/uvm_extern.h>
#include <uvm/uvm_object.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/agpvar.h>
#include <machine/bus.h>

#include "drm_linux.h"
#include "drm_linux_list.h"
#include "drm.h"
#include "drm_mm.h"
#include "drm_atomic.h"
#include "agp.h"

#define	DRM_DEBUGBITS_DEBUG		0x1
#define	DRM_DEBUGBITS_KMS		0x2
#define	DRM_DEBUGBITS_FAILED_IOCTL	0x4

#define __OS_HAS_AGP		(NAGP > 0)

#if BYTE_ORDER == BIG_ENDIAN
#define __BIG_ENDIAN
#else
#define __LITTLE_ENDIAN
#endif

				/* Internal types and structures */
#define DRM_IF_VERSION(maj, min) (maj << 16 | min)

#define DRM_CURRENTPID		curproc->p_pid
#define DRM_MAXUNITS		8

/* DRM_SUSER returns true if the user is superuser */
#define DRM_SUSER(p)		(suser(p, 0) == 0)
#define DRM_MTRR_WC		MDF_WRITECOMBINE

#define PAGE_ALIGN(addr)	(((addr) + PAGE_MASK) & ~PAGE_MASK)
#define roundup2(x, y)	(((x)+((y)-1))&(~((y)-1))) /* if y is powers of two */
#define jiffies_to_msecs(x)	(((int64_t)(x)) * 1000 / hz)
#define msecs_to_jiffies(x)	(((int64_t)(x)) * hz / 1000)
#define time_after(a,b)		((long)(b) - (long)(a) < 0)
#define time_after_eq(a,b)	((long)(b) - (long)(a) <= 0)
#define drm_msleep(x, msg)	delay(x * 1000)

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

/* linux compat */
typedef u_int64_t u64;
typedef u_int32_t u32;
typedef u_int16_t u16;
typedef u_int8_t u8;

typedef int32_t s32;
typedef int64_t s64;

typedef uint16_t __le16;
typedef uint16_t __be16;
typedef uint32_t __le32;
typedef uint32_t __be32;

#define EXPORT_SYMBOL(x)
#define MODULE_FIRMWARE(x)
#define __iomem
#define __must_check
#define __init
#define ARRAY_SIZE nitems
#define DRM_ARRAY_SIZE nitems
#define DIV_ROUND_UP(x, y)	(((x) + ((y) - 1)) / (y))
#define DIV_ROUND_CLOSEST(x, y)	(((x) + ((y) / 2)) / (y))

#define ERESTARTSYS EINTR
#define ETIME ETIMEDOUT
#define EREMOTEIO EIO
#define EPROTO EIO
#define ENOTSUPP ENOTSUP

#define unlikely(x)	__builtin_expect(!!(x), 0)
#define likely(x)	__builtin_expect(!!(x), 1)

#define	IS_ALIGNED(x, y)	(((x) & ((y) - 1)) == 0)

#define BUG()								\
do {									\
	panic("BUG at %s:%d", __FILE__, __LINE__);			\
} while (0)

#define BUG_ON(x) KASSERT(!(x))

#define BUILD_BUG_ON(x) CTASSERT(!(x))

#define WARN(condition, fmt...) ({ 					\
	int __ret = !!(condition);					\
	if (__ret)							\
		printf(fmt);						\
	unlikely(__ret);						\
})

#define WARN_ONCE(condition, fmt...) ({					\
	static int __warned;						\
	int __ret = !!(condition);					\
	if (__ret && !__warned) {					\
		printf(fmt);						\
		__warned = 1;						\
	}								\
	unlikely(__ret);						\
})

#define _WARN_STR(x) #x

#define WARN_ON(condition) ({						\
	int __ret = !!(condition);					\
	if (__ret)							\
		printf("WARNING %s failed at %s:%d\n",			\
		    _WARN_STR(condition), __FILE__, __LINE__);		\
	unlikely(__ret);						\
})

#define WARN_ON_ONCE(condition) ({					\
	static int __warned;						\
	int __ret = !!(condition);					\
	if (__ret && !__warned) {					\
		printf("WARNING %s failed at %s:%d\n",			\
		    _WARN_STR(condition), __FILE__, __LINE__);		\
		__warned = 1;						\
	}								\
	unlikely(__ret);						\
})

#define IS_ERR_VALUE(x) unlikely((x) >= (unsigned long)-ELAST)

static inline void *
ERR_PTR(long error)
{
	return (void *) error;
}

static inline long
PTR_ERR(const void *ptr)
{
	return (long) ptr;
}

static inline long
IS_ERR(const void *ptr)
{
        return IS_ERR_VALUE((unsigned long)ptr);
}

static inline long
IS_ERR_OR_NULL(const void *ptr)
{
        return !ptr || IS_ERR_VALUE((unsigned long)ptr);
}

#define container_of(ptr, type, member) ({                      \
	__typeof( ((type *)0)->member ) *__mptr = (ptr);        \
	(type *)( (char *)__mptr - offsetof(type,member) );})

#ifndef __DECONST
#define __DECONST(type, var)    ((type)(__uintptr_t)(const void *)(var))
#endif

#define GFP_ATOMIC	M_NOWAIT
#define GFP_KERNEL	(M_WAITOK | M_CANFAIL)
#define __GFP_NOWARN	0
#define __GFP_NORETRY	0

static inline void *
kmalloc(size_t size, int flags)
{
	return malloc(size, M_DRM, flags);
}

static inline void *
kmalloc_array(size_t n, size_t size, int flags)
{
	if (n == 0 || SIZE_MAX / n < size)
		return NULL;
	return malloc(n * size, M_DRM, flags);
}

static inline void *
kcalloc(size_t n, size_t size, int flags)
{
	if (n == 0 || SIZE_MAX / n < size)
		return NULL;
	return malloc(n * size, M_DRM, flags | M_ZERO);
}

static inline void *
kzalloc(size_t size, int flags)
{
	return malloc(size, M_DRM, flags | M_ZERO);
}

static inline void
kfree(void *objp)
{
	free(objp, M_DRM, 0);
}

static inline void *
vzalloc(unsigned long size)
{
	return malloc(size, M_DRM, M_WAITOK | M_CANFAIL | M_ZERO);
}

static inline void
vfree(void *objp)
{
	free(objp, M_DRM, 0);
}

#define min_t(t, a, b) ({ \
	t __min_a = (a); \
	t __min_b = (b); \
	__min_a < __min_b ? __min_a : __min_b; })

static inline uint64_t
div_u64(uint64_t x, uint32_t y)
{
	return (x / y);
}

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
#define DRM_MEMORYBARRIER()		membar(Sync)
#endif

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

#define le16_to_cpu(x) letoh16(x)
#define le32_to_cpu(x) letoh32(x)
#define cpu_to_le16(x) htole16(x)
#define cpu_to_le32(x) htole32(x)

#define be32_to_cpup(x) betoh32(*x)

#ifdef __macppc__
static __inline int
of_machine_is_compatible(const char *model)
{
	extern char *hw_prod;
	return (strcmp(model, hw_prod) == 0);
}
#endif

static inline unsigned long
roundup_pow_of_two(unsigned long x)
{
	return (1UL << flsl(x - 1));
}

static inline uint32_t ror32(uint32_t word, unsigned int shift)
{
	return (word >> shift) | (word << (32 - shift));
}

#define DRM_UDELAY(udelay)	DELAY(udelay)

static __inline void
udelay(unsigned long usecs)
{
	DELAY(usecs);
}

static __inline void
usleep_range(unsigned long min, unsigned long max)
{
	DELAY(min);
}

static __inline void
mdelay(unsigned long msecs)
{
	int loops = msecs;
	while (loops--)
		DELAY(1000);
}

#define	drm_can_sleep()	(hz & 1)

#define DRM_WAIT_ON(ret, queue, lock,  timeout, msg, condition ) do {	\
	mtx_enter(lock);						\
	while ((ret) == 0) {						\
		if (condition)						\
			break;						\
		ret = msleep((queue), (lock), PZERO | PCATCH,		\
		    (msg), (timeout));					\
	}								\
	mtx_leave(lock);						\
} while (/* CONSTCOND */ 0)

#define DRM_ERROR(fmt, arg...) \
	printf("error: [" DRM_NAME ":pid%d:%s] *ERROR* " fmt,		\
	    curproc->p_pid, __func__ , ## arg)


#ifdef DRM_DEBUG
#define DRM_INFO(fmt, arg...)  printf("drm: " fmt, ## arg)
#else
#define DRM_INFO(fmt, arg...) do { } while(/* CONSTCOND */ 0)
#endif

#ifdef DRMDEBUG
#undef DRM_DEBUG
#define DRM_DEBUG(fmt, arg...) do {					\
	if (drm_debug_flag)						\
		printf("[" DRM_NAME ":pid%d:%s] " fmt, curproc->p_pid,	\
			__func__ , ## arg);				\
} while (0)
#else
#define DRM_DEBUG(fmt, arg...) do { } while(/* CONSTCOND */ 0)
#endif

#ifdef DRMDEBUG
#undef DRM_DEBUG_KMS
#define DRM_DEBUG_KMS(fmt, arg...) do {					\
	if (drm_debug_flag)						\
		printf("[" DRM_NAME ":pid%d:%s] " fmt, curproc->p_pid,	\
			__func__ , ## arg);				\
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
		printf("[" DRM_NAME ":pid%d:%s] " fmt, curproc->p_pid,	\
			__func__ , ## arg);				\
} while (0)
#else
#define DRM_DEBUG_DRIVER(fmt, arg...) do { } while(/* CONSTCOND */ 0)
#endif

#define PCI_ANY_ID (uint16_t) (~0U)

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

struct drm_buf {
	int		  idx;	       /* Index into master buflist	     */
	int		  total;       /* Buffer size			     */
	int		  used;	       /* Amount of buffer in use (for DMA)  */
	unsigned long	  offset;      /* Byte offset (used internally)	     */
	void 		  *address;    /* KVA of buffer			     */
	unsigned long	  bus_address; /* Bus address of buffer		     */
	__volatile__ int  pending;     /* On hardware DMA queue		     */
	struct drm_file   *file_priv;  /* Unique identifier of holding process */
	void		  *dev_private;  /* Per-buffer private storage       */
};

struct drm_dmamem {
	bus_dmamap_t		map;
	caddr_t			kva;
	bus_size_t		size;
	int			nsegs;
	bus_dma_segment_t	segs[1];
};
typedef struct drm_dmamem drm_dma_handle_t;

struct drm_buf_entry {
	struct drm_dmamem	**seglist;
	struct drm_buf		*buflist;
	int			 buf_count;
	int			 buf_size;
	int			 page_order;
	int			 seg_count;
};

struct drm_pending_event {
	TAILQ_ENTRY(drm_pending_event)	 link;
	struct drm_event		*event;
	struct drm_file			*file_priv;
	pid_t				 pid;
	void				(*destroy)(struct drm_pending_event *);
};

struct drm_pending_vblank_event {
	struct drm_pending_event	base;
	int				pipe;
	struct drm_event_vblank		event;
};

TAILQ_HEAD(drmevlist, drm_pending_event);

struct drm_file {
	SPLAY_HEAD(drm_obj_tree, drm_handle)	 obj_tree;
	struct drmevlist			 evlist;
	struct mutex				 table_lock;
	struct selinfo				 rsel;
	SPLAY_ENTRY(drm_file)			 link;
	int					 authenticated;
	unsigned long				 ioctl_count;
	dev_t					 kdev;
	drm_magic_t				 magic;
	int					 event_space;
	int					 flags;
	int					 master;
	int					 minor;
	u_int					 obj_id; /*next gem id*/
	struct list_head			 fbs;
	void					*driver_priv;
};

/* This structure, in the struct drm_device, is always initialized while
 * the device is open.  dev->dma_lock protects the incrementing of
 * dev->buf_use, which when set marks that no further bufs may be allocated
 * until device teardown occurs (when the last open of the device has closed).
 * The high/low watermarks of bufs are only touched by the X Server, and thus
 * not concurrently accessed, so no locking is needed.
 */
struct drm_device_dma {
	struct rwlock	 	 dma_lock;
	struct drm_buf_entry	 bufs[DRM_MAX_ORDER+1];
	struct drm_buf		**buflist;	/* Vector of pointers info bufs*/
	unsigned long		*pagelist;
	unsigned long		 byte_count;
	int			 buf_use;	/* Buffers used no more alloc */
	int			 buf_count;
	int			 page_count;
	int			 seg_count;
	enum {
		_DRM_DMA_USE_AGP = 0x01,
		_DRM_DMA_USE_SG  = 0x02
	} flags;
};

struct drm_agp_mem {
	void               *handle;
	unsigned long      bound; /* address */
	int                pages;
	TAILQ_ENTRY(drm_agp_mem) link;
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

struct drm_local_map {
	TAILQ_ENTRY(drm_local_map)	 link;	/* Link for map list */
	struct drm_dmamem		*dmamem;/* Handle to DMA mem */
	void				*handle;/* KVA, if mapped */
	bus_space_tag_t			 bst;	/* Tag for mapped pci mem */
	bus_space_handle_t		 bsh;	/* Handle to mapped pci mem */
	u_long				 ext;	/* extent for mmap */
	u_long				 offset;/* Physical address */
	u_long				 size;	/* Physical size (bytes) */
	int				 mtrr;	/* Boolean: MTRR used */
	enum drm_map_flags		 flags;	/* Flags */
	enum drm_map_type		 type;	/* Type of memory mapped */
};

/* Heap implementation for radeon and i915 legacy */
TAILQ_HEAD(drm_heap, drm_mem);

struct drm_mem {
	TAILQ_ENTRY(drm_mem)	 link;
	struct drm_file		*file_priv; /* NULL: free, other: real files */
	int			 start;
	int			 size;
};

/* location of GART table */
#define DRM_ATI_GART_MAIN 1
#define DRM_ATI_GART_FB   2

#define DRM_ATI_GART_PCI  1
#define DRM_ATI_GART_PCIE 2
#define DRM_ATI_GART_IGP  3
#define DRM_ATI_GART_R600 4

#define DMA_BIT_MASK(n) (((n) == 64) ? ~0ULL : (1ULL<<(n)) -1)
#define lower_32_bits(n)	((u32)(n))
#define upper_32_bits(_val) ((u_int32_t)(((_val) >> 16) >> 16))

/*
 *  Locking protocol:
 * All drm object are uvm objects, as such they have a reference count and
 * a lock. On the other hand, operations carries out by the drm may involve
 * sleeping (waiting for rendering to finish, say), while you wish to have
 * mutual exclusion on an object. For this reason, all drm-related operations
 * on drm objects must acquire the DRM_BUSY flag on the object as the first
 * thing that they do. If the BUSY flag is already on the object, set DRM_WANTED
 * and sleep until the other locker is done with it. When the BUSY flag is 
 * acquired then only that flag and a reference is required to do most 
 * operations on the drm_object. The uvm object is still bound by uvm locking
 * protocol.
 *
 * Subdrivers (radeon, intel, etc) may have other locking requirement, these
 * requirements will be detailed in those drivers.
 */
struct drm_gem_object {
	struct uvm_object		 uobj;
	SPLAY_ENTRY(drm_gem_object) 	 entry;
	struct drm_device		*dev;
	struct uvm_object		*uao;
	struct drm_local_map		*map;

	size_t				 size;
	int				 name;
	int				 handlecount;
/* any flags over 0x00000010 are device specific */
#define	DRM_BUSY	0x00000001
#define	DRM_WANTED	0x00000002
	u_int				 do_flags;
	uint32_t			 read_domains;
	uint32_t			 write_domain;

	uint32_t			 pending_read_domains;
	uint32_t			 pending_write_domain;
};

struct drm_handle {
	SPLAY_ENTRY(drm_handle)	 entry;
	struct drm_gem_object	*obj;
	uint32_t		 handle;
};

struct drm_mode_handle {
	SPLAY_ENTRY(drm_mode_handle) entry;
	struct drm_mode_object	*obj;
	uint32_t		 handle;
};

/* Size of ringbuffer for vblank timestamps. Just double-buffer
 * in initial implementation.
 */
#define DRM_VBLANKTIME_RBSIZE 2

/* Flags and return codes for get_vblank_timestamp() driver function. */
#define DRM_CALLED_FROM_VBLIRQ 1
#define DRM_VBLANKTIME_SCANOUTPOS_METHOD (1 << 0)
#define DRM_VBLANKTIME_INVBL             (1 << 1)

/* get_scanout_position() return flags */
#define DRM_SCANOUTPOS_VALID        (1 << 0)
#define DRM_SCANOUTPOS_INVBL        (1 << 1)
#define DRM_SCANOUTPOS_ACCURATE     (1 << 2)

struct drm_driver_info {
	int	(*firstopen)(struct drm_device *);
	int	(*open)(struct drm_device *, struct drm_file *);
	int	(*ioctl)(struct drm_device*, u_long, caddr_t,
		    struct drm_file *);
	void	(*close)(struct drm_device *, struct drm_file *);
	void	(*lastclose)(struct drm_device *);
	struct uvm_object *(*mmap)(struct drm_device *, voff_t, vsize_t);
	int	(*dma_ioctl)(struct drm_device *, struct drm_dma *,
		    struct drm_file *);
	int	(*irq_handler)(void *);
	void	(*irq_preinstall) (struct drm_device *);
	int	(*irq_install)(struct drm_device *);
	int	(*irq_postinstall) (struct drm_device *);
	void	(*irq_uninstall)(struct drm_device *);
	int	vblank_pipes;
	u_int32_t (*get_vblank_counter)(struct drm_device *, int);
	int	(*enable_vblank)(struct drm_device *, int);
	void	(*disable_vblank)(struct drm_device *, int);
	int	(*get_scanout_position)(struct drm_device *, int, int *, int *);
	int	(*get_vblank_timestamp)(struct drm_device *, int, int *,
		    struct timeval *, unsigned);;

	/**
	 * Driver-specific constructor for drm_gem_objects, to set up
	 * obj->driver_private.
	 *
	 * Returns 0 on success.
	 */
	int (*gem_init_object) (struct drm_gem_object *obj);
	void (*gem_free_object) (struct drm_gem_object *obj);
	int (*gem_open_object) (struct drm_gem_object *, struct drm_file *);
	void (*gem_close_object) (struct drm_gem_object *, struct drm_file *);

	int	(*gem_fault)(struct drm_gem_object *, struct uvm_faultinfo *,
		    off_t, vaddr_t, vm_page_t *, int, int, vm_prot_t, int);

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

#define DRIVER_AGP		0x1
#define DRIVER_AGP_REQUIRE	0x2
#define DRIVER_MTRR		0x4
#define DRIVER_DMA		0x8
#define DRIVER_PCI_DMA		0x10
#define DRIVER_SG		0x20
#define DRIVER_IRQ		0x40
#define DRIVER_GEM		0x80
#define DRIVER_MODESET		0x100

	u_int	flags;
};

#include "drm_crtc.h"

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

struct pci_dev {
	uint16_t	vendor;
	uint16_t	device;
	uint16_t	subsystem_vendor;
	uint16_t	subsystem_device;
};

/** 
 * DRM device functions structure
 */
struct drm_device {
	struct device	  device; /* softc is an extension of struct device */

	struct drm_driver_info *driver;

	struct pci_dev	 drm_pci;
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

	/* Linked list of mappable regions. Protected by struct_mutex */
	struct extent				*handle_ext;
	TAILQ_HEAD(drm_map_list, drm_local_map)	 maplist;

				/* DMA queues (contexts) */
	struct drm_device_dma  *dma;		/* Optional pointer for DMA support */

				/* Context support */
	int		  irq;		/* Interrupt used by board	   */
	int		  irq_enabled;	/* True if the irq handler is enabled */

	/* VBLANK support */
	struct drmevlist	vbl_events;		/* vblank events */
	int			 vblank_disable_allowed;
	/**< size of vblank counter register */
	uint32_t		 max_vblank_count;
	struct mutex		 event_lock;

	int			*vbl_queue;
	atomic_t		*_vblank_count;
	struct timeval		*_vblank_time;
	struct mutex		 vblank_time_lock;
	struct mutex		 vbl_lock;
	atomic_t		*vblank_refcount;
	uint32_t		*last_vblank;

	int			*vblank_enabled;
	int			*vblank_inmodeset;
	u32			*last_vblank_wait;
	struct timeout		 vblank_disable_timer;

	int			 num_crtcs;

	pid_t			 buf_pgid;

	struct drm_agp_head	*agp;
	void			*dev_private;
	struct drm_local_map	*agp_buffer_map;

	struct drm_mode_config	 mode_config; /* Current mode config */

	/* GEM info */
	struct mutex		 obj_name_lock;
	atomic_t		 obj_count;
	u_int			 obj_name;
	atomic_t		 obj_memory;
	SPLAY_HEAD(drm_name_tree, drm_gem_object) name_tree;
	struct pool				objpl;
	
	/* mode stuff */
};

struct drm_attach_args {
	struct drm_driver_info		*driver;
	char				*busid;
	bus_dma_tag_t			 dmat;
	bus_space_tag_t			 bst;
	size_t				 busid_len;
	int				 is_agp;
	u_int8_t			 irq;
	u_int16_t			 pci_vendor;
	u_int16_t			 pci_device;
	u_int16_t			 pci_subvendor;
	u_int16_t			 pci_subdevice;
	pci_chipset_tag_t		 pc;
	pcitag_t			*bridgetag;
	int				 console;
};

#define DRMDEVCF_CONSOLE	0
#define drmdevcf_console	cf_loc[DRMDEVCF_CONSOLE]
/* spec'd as console? */
#define DRMDEVCF_CONSOLE_UNK	-1

extern int	drm_debug_flag;

enum dmi_field {
        DMI_NONE,
        DMI_BIOS_VENDOR,
        DMI_BIOS_VERSION,
        DMI_BIOS_DATE,
        DMI_SYS_VENDOR,
        DMI_PRODUCT_NAME,
        DMI_PRODUCT_VERSION,
        DMI_PRODUCT_SERIAL,
        DMI_PRODUCT_UUID,
        DMI_BOARD_VENDOR,
        DMI_BOARD_NAME,
        DMI_BOARD_VERSION,
        DMI_BOARD_SERIAL,
        DMI_BOARD_ASSET_TAG,
        DMI_CHASSIS_VENDOR,
        DMI_CHASSIS_TYPE,
        DMI_CHASSIS_VERSION,
        DMI_CHASSIS_SERIAL,
        DMI_CHASSIS_ASSET_TAG,
        DMI_STRING_MAX,
};

struct dmi_strmatch {
	unsigned char slot;
	char substr[79];
};

struct dmi_system_id {
        int (*callback)(const struct dmi_system_id *);
        const char *ident;
        struct dmi_strmatch matches[4];
};
#define	DMI_MATCH(a, b) {(a), (b)}
#define	DMI_EXACT_MATCH(a, b) {(a), (b)}
int dmi_check_system(const struct dmi_system_id *);


/* Device setup support (drm_drv.c) */
int	drm_pciprobe(struct pci_attach_args *, const struct drm_pcidev * );
struct device	*drm_attach_pci(struct drm_driver_info *, 
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

const struct drm_pcidev	*drm_find_description(int , int ,
			     const struct drm_pcidev *);
int	 drm_order(unsigned long);

/* File operations helpers (drm_fops.c) */
struct drm_file	*drm_find_file_by_minor(struct drm_device *, int);

/* Memory management support (drm_memory.c) */
void	*drm_alloc(size_t);
void	*drm_calloc(size_t, size_t);
void	*drm_realloc(void *, size_t, size_t);
void	 drm_free(void *);

/* XXX until we get PAT support */
#define drm_core_ioremap_wc drm_core_ioremap
void	drm_core_ioremap(struct drm_local_map *, struct drm_device *);
void	drm_core_ioremapfree(struct drm_local_map *, struct drm_device *);

int	drm_mtrr_add(unsigned long, size_t, int);
int	drm_mtrr_del(int, unsigned long, size_t, int);

/* IRQ support (drm_irq.c) */
int	drm_irq_install(struct drm_device *);
int	drm_irq_uninstall(struct drm_device *);
void	drm_vblank_cleanup(struct drm_device *);
u32	drm_get_last_vbltimestamp(struct drm_device *, int ,
				  struct timeval *, unsigned);
int	drm_vblank_init(struct drm_device *, int);
u_int32_t drm_vblank_count(struct drm_device *, int);
u_int32_t drm_vblank_count_and_time(struct drm_device *, int, struct timeval *);
int	drm_vblank_get(struct drm_device *, int);
void	drm_vblank_put(struct drm_device *, int);
void	drm_vblank_off(struct drm_device *, int);
void	drm_vblank_pre_modeset(struct drm_device *, int);
void	drm_vblank_post_modeset(struct drm_device *, int);
int	drm_modeset_ctl(struct drm_device *, void *, struct drm_file *);
bool	drm_handle_vblank(struct drm_device *, int);
void	drm_calc_timestamping_constants(struct drm_crtc *);
int	drm_calc_vbltimestamp_from_scanoutpos(struct drm_device *,
	    int, int *, struct timeval *, unsigned, struct drm_crtc *);
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

/* IRQ support (drm_irq.c) */
int	drm_control(struct drm_device *, void *, struct drm_file *);
int	drm_wait_vblank(struct drm_device *, void *, struct drm_file *);
int	drm_irq_by_busid(struct drm_device *, void *, struct drm_file *);
void	drm_send_vblank_event(struct drm_device *, int,
	    struct drm_pending_vblank_event *);

/* AGP/GART support (drm_agpsupport.c) */
int	drm_agp_acquire_ioctl(struct drm_device *, void *, struct drm_file *);
int	drm_agp_release_ioctl(struct drm_device *, void *, struct drm_file *);
int	drm_agp_enable_ioctl(struct drm_device *, void *, struct drm_file *);
int	drm_agp_info_ioctl(struct drm_device *, void *, struct drm_file *);
int	drm_agp_alloc_ioctl(struct drm_device *, void *, struct drm_file *);
int	drm_agp_free_ioctl(struct drm_device *, void *, struct drm_file *);
int	drm_agp_unbind_ioctl(struct drm_device *, void *, struct drm_file *);
int	drm_agp_bind_ioctl(struct drm_device *, void *, struct drm_file *);

static inline int
drm_sysfs_connector_add(struct drm_connector *connector)
{
	return 0;
}

static inline void
drm_sysfs_connector_remove(struct drm_connector *connector)
{
}

static inline void
drm_sysfs_hotplug_event(struct drm_device *dev)
{
}

/* Graphics Execution Manager library functions (drm_gem.c) */
void drm_gem_object_release(struct drm_gem_object *obj);
struct drm_gem_object *drm_gem_object_alloc(struct drm_device *dev,
					    size_t size);
int drm_gem_object_init(struct drm_device *dev,
			struct drm_gem_object *obj, size_t size);

void	 drm_unref(struct uvm_object *);
void	 drm_ref(struct uvm_object *);

int drm_gem_handle_create(struct drm_file *file_priv,
			  struct drm_gem_object *obj,
			  u32 *handlep);
int drm_gem_handle_delete(struct drm_file *filp, u32 handle);

void drm_gem_free_mmap_offset(struct drm_gem_object *obj);
int drm_gem_create_mmap_offset(struct drm_gem_object *obj);

struct drm_gem_object *drm_gem_object_lookup(struct drm_device *dev,
					     struct drm_file *filp,
					     u32 handle);
int	drm_gem_close_ioctl(struct drm_device *, void *, struct drm_file *);
int	drm_gem_flink_ioctl(struct drm_device *, void *, struct drm_file *);
int	drm_gem_open_ioctl(struct drm_device *, void *, struct drm_file *);

static __inline void
drm_gem_object_reference(struct drm_gem_object *obj)
{
	drm_ref(&obj->uobj);
}

static __inline void
drm_gem_object_unreference(struct drm_gem_object *obj)
{
	drm_unref(&obj->uobj);
}

static __inline void
drm_gem_object_unreference_unlocked(struct drm_gem_object *obj)
{
	struct drm_device *dev = obj->dev;

	mutex_lock(&dev->struct_mutex);
	drm_unref(&obj->uobj);
	mutex_unlock(&dev->struct_mutex);
}

static __inline__ int drm_core_check_feature(struct drm_device *dev,
					     int feature)
{
	return ((dev->driver->flags & feature) ? 1 : 0);
}

#define DRM_PCIE_SPEED_25 1
#define DRM_PCIE_SPEED_50 2
#define DRM_PCIE_SPEED_80 4

int	 drm_pcie_get_speed_cap_mask(struct drm_device *, u32 *);

#endif /* __KERNEL__ */
#endif /* _DRM_P_H_ */
