/* $OpenBSD: drmP.h,v 1.8 2020/03/04 21:19:15 kettenis Exp $ */
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

#include <linux/agp_backend.h>
#include <linux/cdev.h>
#include <linux/dma-mapping.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/highmem.h>
#include <linux/idr.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/kref.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/ratelimit.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/vmalloc.h>
#include <linux/workqueue.h>
#include <linux/dma-fence.h>
#include <linux/module.h>

#include <dev/pci/drm/drm_linux.h>
#include <linux/list.h>
#include <linux/pci.h>
/* these match drm_os_linux.h includes */
#include <linux/interrupt.h>
#include <linux/sched/signal.h>
#include <linux/delay.h>

#include <uapi/drm/drm.h>
#include <uapi/drm/drm_mode.h>

#include <drm/drm_crtc.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_global.h>
#include <drm/drm_hashtab.h>
#include <drm/drm_mm.h>
#include <drm/drm_drv.h>
#include <drm/drm_prime.h>
#include <drm/drm_print.h>
#include <drm/drm_file.h>
#include <drm/drm_ioctl.h>
#include <drm/drm_vblank.h>
#include <drm/drm_irq.h>
#include <drm/drm_device.h>

struct fb_cmap;
struct fb_fillrect;
struct fb_copyarea;
struct fb_image;

				/* Internal types and structures */
#define DRM_IF_VERSION(maj, min) (maj << 16 | min)

#define DRM_CURRENTPID		curproc->p_p->ps_pid

/* DRM_SUSER returns true if the user is superuser */
#define DRM_SUSER(p)		(suser(p) == 0)
#define DRM_MTRR_WC		MDF_WRITECOMBINE

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

#define	DRM_COPY_TO_USER(user, kern, size)	copyout(kern, user, size)

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

struct drm_pcidev {
	uint16_t vendor;
	uint16_t device;
	uint16_t subvendor;
	uint16_t subdevice;
	uint32_t class;
	uint32_t class_mask;
	unsigned long driver_data;
};

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

/* Flags and return codes for get_vblank_timestamp() driver function. */
#define DRM_CALLED_FROM_VBLIRQ 1

/* get_scanout_position() return flags */
#define DRM_SCANOUTPOS_VALID        (1 << 0)
#define DRM_SCANOUTPOS_IN_VBLANK    (1 << 1)
#define DRM_SCANOUTPOS_ACCURATE     (1 << 2)

struct drm_attach_args {
	struct drm_device		*drm;
	struct drm_driver		*driver;
	char				*busid;
	bus_dma_tag_t			 dmat;
	bus_space_tag_t			 bst;
	size_t				 busid_len;
	int				 is_agp;
	struct pci_attach_args		*pa;
	int				 primary;
};

#define DRMDEVCF_PRIMARY	0
#define drmdevcf_primary	cf_loc[DRMDEVCF_PRIMARY]	/* spec'd as primary? */
#define DRMDEVCF_PRIMARY_UNK	-1

void	drm_linux_init(void);
int	drm_linux_acpi_notify(struct aml_node *, int, void *);

/* Device setup support (drm_drv.c) */
int	drm_pciprobe(struct pci_attach_args *, const struct drm_pcidev * );
struct drm_device *drm_attach_pci(struct drm_driver *, 
		     struct pci_attach_args *, int, int, struct device *, struct drm_device *);
int	 drmprint(void *, const char *);
int	 drmsubmatch(struct device *, void *, void *);
dev_type_ioctl(drmioctl);
dev_type_read(drmread);
dev_type_poll(drmpoll);
dev_type_open(drmopen);
dev_type_close(drmclose);
dev_type_mmap(drmmmap);
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

int	drm_mtrr_add(unsigned long, size_t, int);
int	drm_mtrr_del(int, unsigned long, size_t, int);

/* Cache management (drm_cache.c) */
void drm_clflush_pages(struct vm_page *pages[], unsigned long num_pages);
void drm_clflush_sg(struct sg_table *st);
void drm_clflush_virt_range(void *addr, unsigned long length);

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

/* helper for handling conditionals in various for_each macros */
#define for_each_if(condition) if (!(condition)) {} else

static inline bool
drm_is_current_master(struct drm_file *file_priv)
{
	return (file_priv->is_master == 1);
}

#endif /* __KERNEL__ */
#endif /* _DRM_P_H_ */
