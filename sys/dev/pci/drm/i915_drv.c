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
/*-
 * Copyright © 2008 Intel Corporation
 * Copyright 2003 Tungsten Graphics, Inc., Cedar Park, Texas.
 * copyright 2000 VA Linux Systems, Inc., Sunnyvale, California.
 * All Rights Reserved.
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
 *    Gareth Hughes <gareth@valinux.com>
 *    Eric Anholt <eric@anholt.net>
 *
 */

#include "drmP.h"
#include "drm.h"
#include "i915_drm.h"
#include "i915_drv.h"

#include <machine/pmap.h>

#include <sys/queue.h>
#include <sys/workq.h>
#if 0
#	define INTELDRM_WATCH_COHERENCY
#	define WATCH_INACTIVE
#endif

#define I915_GEM_GPU_DOMAINS	(~(I915_GEM_DOMAIN_CPU | I915_GEM_DOMAIN_GTT))

int	inteldrm_probe(struct device *, void *, void *);
void	inteldrm_attach(struct device *, struct device *, void *);
int	inteldrm_detach(struct device *, int);
int	inteldrm_activate(struct device *, int);
int	inteldrm_ioctl(struct drm_device *, u_long, caddr_t, struct drm_file *);
int	inteldrm_intr(void *);
void	inteldrm_lastclose(struct drm_device *);

void	inteldrm_wrap_ring(struct drm_i915_private *);
int	inteldrm_gmch_match(struct pci_attach_args *);
void	inteldrm_chipset_flush(struct drm_i915_private *);
void	inteldrm_timeout(void *);
void	inteldrm_hangcheck(void *);
void	inteldrm_hung(void *, void *);
void	inteldrm_965_reset(struct drm_i915_private *, u_int8_t);
int	inteldrm_fault(struct drm_obj *, struct uvm_faultinfo *, off_t,
	    vaddr_t, vm_page_t *, int, int, vm_prot_t, int );
void	inteldrm_wipe_mappings(struct drm_obj *);
void	inteldrm_purge_obj(struct drm_obj *);
void	inteldrm_set_max_obj_size(struct drm_i915_private *);

/* For reset and suspend */
int	inteldrm_save_state(struct drm_i915_private *);
int	inteldrm_restore_state(struct drm_i915_private *);
int	inteldrm_save_display(struct drm_i915_private *);
int	inteldrm_restore_display(struct drm_i915_private *);
void	i915_save_vga(struct drm_i915_private *);
void	i915_restore_vga(struct drm_i915_private *);
void	i915_save_modeset_reg(struct drm_i915_private *);
void	i915_restore_modeset_reg(struct drm_i915_private *);
u_int8_t	i915_read_indexed(struct drm_i915_private *, u_int16_t,
		    u_int16_t, u_int8_t);
void	i915_write_indexed(struct drm_i915_private *, u_int16_t,
	    u_int16_t, u_int8_t, u_int8_t);
void	i915_write_ar(struct drm_i915_private *, u_int16_t, u_int8_t,
	    u_int8_t, u_int16_t);
u_int8_t	i915_read_ar(struct drm_i915_private *, u_int16_t,
		    u_int8_t, u_int16_t);
void	i915_save_palette(struct drm_i915_private *, enum pipe);
void	i915_restore_palette(struct drm_i915_private *, enum pipe);

void	i915_alloc_ifp(struct drm_i915_private *, struct pci_attach_args *);
void	i965_alloc_ifp(struct drm_i915_private *, struct pci_attach_args *);

void	inteldrm_detect_bit_6_swizzle(drm_i915_private_t *,
	    struct pci_attach_args *);

int	inteldrm_setup_mchbar(struct drm_i915_private *,
	    struct pci_attach_args *);
void	inteldrm_teardown_mchbar(struct drm_i915_private *,
	    struct pci_attach_args *, int);

/* Ioctls */
int	i915_gem_init_ioctl(struct drm_device *, void *, struct drm_file *);
int	i915_gem_create_ioctl(struct drm_device *, void *, struct drm_file *);
int	i915_gem_pread_ioctl(struct drm_device *, void *, struct drm_file *);
int	i915_gem_pwrite_ioctl(struct drm_device *, void *, struct drm_file *);
int	i915_gem_set_domain_ioctl(struct drm_device *, void *,
	    struct drm_file *);
int	i915_gem_execbuffer2(struct drm_device *, void *, struct drm_file *);
int	i915_gem_pin_ioctl(struct drm_device *, void *, struct drm_file *);
int	i915_gem_unpin_ioctl(struct drm_device *, void *, struct drm_file *);
int	i915_gem_busy_ioctl(struct drm_device *, void *, struct drm_file *);
int	i915_gem_entervt_ioctl(struct drm_device *, void *, struct drm_file *);
int	i915_gem_leavevt_ioctl(struct drm_device *, void *, struct drm_file *);
int	i915_gem_get_aperture_ioctl(struct drm_device *, void *,
	    struct drm_file *);
int	i915_gem_set_tiling(struct drm_device *, void *, struct drm_file *);
int	i915_gem_get_tiling(struct drm_device *, void *, struct drm_file *);
int	i915_gem_gtt_map_ioctl(struct drm_device *, void *, struct drm_file *);
int	i915_gem_madvise_ioctl(struct drm_device *, void *, struct drm_file *);

/* GEM memory manager functions */
int	i915_gem_init_object(struct drm_obj *);
void	i915_gem_free_object(struct drm_obj *);
int	i915_gem_object_pin(struct drm_obj *, uint32_t, int);
void	i915_gem_object_unpin(struct drm_obj *);
void	i915_gem_retire_requests(struct drm_i915_private *);
void	i915_gem_retire_request(struct drm_i915_private *,
	    struct inteldrm_request *);
void	i915_gem_retire_work_handler(void *, void*);
int	i915_gem_idle(struct drm_i915_private *);
void	i915_gem_object_move_to_active(struct drm_obj *);
void	i915_gem_object_move_off_active(struct drm_obj *);
void	i915_gem_object_move_to_inactive(struct drm_obj *);
void	i915_gem_object_move_to_inactive_locked(struct drm_obj *);
uint32_t	i915_add_request(struct drm_i915_private *);
void	inteldrm_process_flushing(struct drm_i915_private *, u_int32_t);
void	i915_move_to_tail(struct inteldrm_obj *, struct i915_gem_list *);
void	i915_list_remove(struct inteldrm_obj *);
int	i915_gem_init_hws(struct drm_i915_private *);
void	i915_gem_cleanup_hws(struct drm_i915_private *);
int	i915_gem_init_ringbuffer(struct drm_i915_private *);
int	inteldrm_start_ring(struct drm_i915_private *);
void	i915_gem_cleanup_ringbuffer(struct drm_i915_private *);
int	i915_gem_ring_throttle(struct drm_device *, struct drm_file *);
int	i915_gem_evict_inactive(struct drm_i915_private *);
int	i915_gem_get_relocs_from_user(struct drm_i915_gem_exec_object2 *,
	    u_int32_t, struct drm_i915_gem_relocation_entry **);
int	i915_gem_put_relocs_to_user(struct drm_i915_gem_exec_object2 *,
	    u_int32_t, struct drm_i915_gem_relocation_entry *);
void	i915_dispatch_gem_execbuffer(struct drm_device *,
	    struct drm_i915_gem_execbuffer2 *, uint64_t);
void	i915_gem_object_set_to_gpu_domain(struct drm_obj *);
int	i915_gem_object_pin_and_relocate(struct drm_obj *,
	    struct drm_file *, struct drm_i915_gem_exec_object2 *,
	    struct drm_i915_gem_relocation_entry *);
int	i915_gem_object_bind_to_gtt(struct drm_obj *, bus_size_t, int);
int	i915_wait_request(struct drm_i915_private *, uint32_t, int);
u_int32_t	i915_gem_flush(struct drm_i915_private *, uint32_t, uint32_t);
int	i915_gem_object_unbind(struct drm_obj *, int);

struct drm_obj	*i915_gem_find_inactive_object(struct drm_i915_private *,
		     size_t);

int	i915_gem_evict_everything(struct drm_i915_private *, int);
int	i915_gem_evict_something(struct drm_i915_private *, size_t, int);
int	i915_gem_object_set_to_gtt_domain(struct drm_obj *, int, int);
int	i915_gem_object_set_to_cpu_domain(struct drm_obj *, int, int);
int	i915_gem_object_flush_gpu_write_domain(struct drm_obj *, int, int, int);
int	i915_gem_get_fence_reg(struct drm_obj *, int);
int	i915_gem_object_put_fence_reg(struct drm_obj *, int);
bus_size_t	i915_gem_get_gtt_alignment(struct drm_obj *);

bus_size_t	i915_get_fence_size(struct drm_i915_private *, bus_size_t);
int	i915_tiling_ok(struct drm_device *, int, int, int);
int	i915_gem_object_fence_offset_ok(struct drm_obj *, int);
void	i965_write_fence_reg(struct inteldrm_fence *);
void	i915_write_fence_reg(struct inteldrm_fence *);
void	i830_write_fence_reg(struct inteldrm_fence *);
void	i915_gem_bit_17_swizzle(struct drm_obj *);
void	i915_gem_save_bit_17_swizzle(struct drm_obj *);
int	inteldrm_swizzle_page(struct vm_page *page);

/* Debug functions, mostly called from ddb */
void	i915_gem_seqno_info(int);
void	i915_interrupt_info(int);
void	i915_gem_fence_regs_info(int);
void	i915_hws_info(int);
void	i915_batchbuffer_info(int);
void	i915_ringbuffer_data(int);
void	i915_ringbuffer_info(int);
#ifdef WATCH_INACTIVE
void inteldrm_verify_inactive(struct drm_i915_private *, char *, int);
#else
#define inteldrm_verify_inactive(dev,file,line)
#endif

const static struct drm_pcidev inteldrm_pciidlist[] = {
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82830M_IGD,
	    CHIP_I830|CHIP_M|CHIP_GEN2},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82845G_IGD,
	    CHIP_I845G|CHIP_GEN2},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82855GM_IGD,
	    CHIP_I85X|CHIP_M|CHIP_GEN2},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82865G_IGD,
	    CHIP_I865G|CHIP_GEN2},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82915G_IGD_1,
	    CHIP_I915G|CHIP_I9XX|CHIP_GEN3},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_E7221_IGD,
	    CHIP_I915G|CHIP_I9XX|CHIP_GEN3},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82915GM_IGD_1,
	    CHIP_I915GM|CHIP_I9XX|CHIP_M|CHIP_GEN3},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82945G_IGD_1,
	    CHIP_I945G|CHIP_I9XX|CHIP_GEN3},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82945GM_IGD_1,
	    CHIP_I945GM|CHIP_I9XX|CHIP_M|CHIP_GEN3},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82945GME_IGD_1,
	    CHIP_I945GM|CHIP_I9XX|CHIP_M|CHIP_GEN3},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82946GZ_IGD_1,
	    CHIP_I965|CHIP_I9XX|CHIP_GEN4},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82G35_IGD_1,
	    CHIP_I965|CHIP_I9XX|CHIP_GEN4},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82Q965_IGD_1,
	    CHIP_I965|CHIP_I9XX|CHIP_GEN4},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82G965_IGD_1,
	    CHIP_I965|CHIP_I9XX|CHIP_GEN4},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82GM965_IGD_1,
	    CHIP_I965GM|CHIP_I965|CHIP_I9XX|CHIP_M|CHIP_GEN4},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82GME965_IGD_1,
	    CHIP_I965|CHIP_I9XX|CHIP_GEN4},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82G33_IGD_1,
	    CHIP_G33|CHIP_I9XX|CHIP_HWS|CHIP_GEN3},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82Q35_IGD_1,
	    CHIP_G33|CHIP_I9XX|CHIP_HWS|CHIP_GEN3},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82Q33_IGD_1,
	    CHIP_G33|CHIP_I9XX|CHIP_HWS|CHIP_GEN3},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82GM45_IGD_1,
	    CHIP_G4X|CHIP_GM45|CHIP_I965|CHIP_I9XX|CHIP_M|CHIP_HWS|CHIP_GEN4},
	{PCI_VENDOR_INTEL, 0x2E02,
	    CHIP_G4X|CHIP_I965|CHIP_I9XX|CHIP_HWS|CHIP_GEN4},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82Q45_IGD_1,
	    CHIP_G4X|CHIP_I965|CHIP_I9XX|CHIP_HWS|CHIP_GEN4},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82G45_IGD_1,
	    CHIP_G4X|CHIP_I965|CHIP_I9XX|CHIP_HWS|CHIP_GEN4},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82G41_IGD_1,
	    CHIP_G4X|CHIP_I965|CHIP_I9XX|CHIP_HWS|CHIP_GEN4},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_PINEVIEW_IGC_1,
	    CHIP_G33|CHIP_PINEVIEW|CHIP_M|CHIP_I9XX|CHIP_HWS|CHIP_GEN3},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_PINEVIEW_M_IGC_1,
	    CHIP_G33|CHIP_PINEVIEW|CHIP_M|CHIP_I9XX|CHIP_HWS|CHIP_GEN3},
	{0, 0, 0}
};

static const struct drm_driver_info inteldrm_driver = {
	.buf_priv_size		= 1,	/* No dev_priv */
	.file_priv_size		= sizeof(struct inteldrm_file),
	.ioctl			= inteldrm_ioctl,
	.lastclose		= inteldrm_lastclose,
	.vblank_pipes		= 2,
	.get_vblank_counter	= i915_get_vblank_counter,
	.enable_vblank		= i915_enable_vblank,
	.disable_vblank		= i915_disable_vblank,
	.irq_install		= i915_driver_irq_install,
	.irq_uninstall		= i915_driver_irq_uninstall,

	.gem_init_object	= i915_gem_init_object,
	.gem_free_object	= i915_gem_free_object,
	.gem_fault		= inteldrm_fault,
	.gem_size		= sizeof(struct inteldrm_obj),

	.name			= DRIVER_NAME,
	.desc			= DRIVER_DESC,
	.date			= DRIVER_DATE,
	.major			= DRIVER_MAJOR,
	.minor			= DRIVER_MINOR,
	.patchlevel		= DRIVER_PATCHLEVEL,

	.flags			= DRIVER_AGP | DRIVER_AGP_REQUIRE |
				    DRIVER_MTRR | DRIVER_IRQ
				    | DRIVER_GEM,
};

int
inteldrm_probe(struct device *parent, void *match, void *aux)
{
	return (drm_pciprobe((struct pci_attach_args *)aux,
	    inteldrm_pciidlist));
}

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
inteldrm_attach(struct device *parent, struct device *self, void *aux)
{
	struct drm_i915_private	*dev_priv = (struct drm_i915_private *)self;
	struct pci_attach_args	*pa = aux, bpa;
	struct vga_pci_bar	*bar;
	struct drm_device 	*dev;
	const struct drm_pcidev	*id_entry;
	int			 i;

	id_entry = drm_find_description(PCI_VENDOR(pa->pa_id),
	    PCI_PRODUCT(pa->pa_id), inteldrm_pciidlist);
	dev_priv->flags = id_entry->driver_private;
	dev_priv->pci_device = PCI_PRODUCT(pa->pa_id);

	dev_priv->pc = pa->pa_pc;
	dev_priv->tag = pa->pa_tag;
	dev_priv->dmat = pa->pa_dmat;
	dev_priv->bst = pa->pa_memt;

	/* we need to use this api for now due to sharing with intagp */
	bar = vga_pci_bar_info((struct vga_pci_softc *)parent,
	    (IS_I9XX(dev_priv) ? 0 : 1));
	if (bar == NULL) {
		printf(": can't get BAR info\n");
		return;
	}

	dev_priv->regs = vga_pci_bar_map((struct vga_pci_softc *)parent, 
	    bar->addr, 0, 0);
	if (dev_priv->regs == NULL) {
		printf(": can't map mmio space\n");
		return;
	}

	if (pci_intr_map(pa, &dev_priv->ih) != 0) {
		printf(": couldn't map interrupt\n");
		return;
	}

	/*
	 * set up interrupt handler, note that we don't switch the interrupt
	 * on until the X server talks to us, kms will change this.
	 */
	dev_priv->irqh = pci_intr_establish(dev_priv->pc, dev_priv->ih, IPL_TTY,
	    inteldrm_intr, dev_priv, dev_priv->dev.dv_xname);
	if (dev_priv->irqh == NULL) {
		printf(": couldn't  establish interrupt\n");
		return;
	}

	/* Unmask the interrupts that we always want on. */
	dev_priv->irq_mask_reg = ~I915_INTERRUPT_ENABLE_FIX;

	dev_priv->workq = workq_create("intelrel", 1, IPL_TTY);
	if (dev_priv->workq == NULL) {
		printf("couldn't create workq\n");
		return;
	}

	/* GEM init */
	TAILQ_INIT(&dev_priv->mm.active_list);
	TAILQ_INIT(&dev_priv->mm.flushing_list);
	TAILQ_INIT(&dev_priv->mm.inactive_list);
	TAILQ_INIT(&dev_priv->mm.gpu_write_list);
	TAILQ_INIT(&dev_priv->mm.request_list);
	TAILQ_INIT(&dev_priv->mm.fence_list);
	timeout_set(&dev_priv->mm.retire_timer, inteldrm_timeout, dev_priv);
	timeout_set(&dev_priv->mm.hang_timer, inteldrm_hangcheck, dev_priv);
	dev_priv->mm.next_gem_seqno = 1;
	dev_priv->mm.suspended = 1;

	/* For the X server, in kms mode this will not be needed */
	dev_priv->fence_reg_start = 3;

	if (IS_I965G(dev_priv) || IS_I945G(dev_priv) || IS_I945GM(dev_priv) ||
	    IS_G33(dev_priv))
		dev_priv->num_fence_regs = 16;
	else
		dev_priv->num_fence_regs = 8;
	/* Initialise fences to zero, else on some macs we'll get corruption */
	if (IS_I965G(dev_priv)) {
		for (i = 0; i < 16; i++)
			I915_WRITE64(FENCE_REG_965_0 + (i * 8), 0);
	} else {
		for (i = 0; i < 8; i++)
			I915_WRITE(FENCE_REG_830_0 + (i * 4), 0);
		if (IS_I945G(dev_priv) || IS_I945GM(dev_priv) ||
		    IS_G33(dev_priv))
			for (i = 0; i < 8; i++)
				I915_WRITE(FENCE_REG_945_8 + (i * 4), 0);
	}

	if (pci_find_device(&bpa, inteldrm_gmch_match) == 0) {
		printf(": can't find GMCH\n");
		return;
	}

	/* Set up the IFP for chipset flushing */
	if (dev_priv->flags & (CHIP_I915G|CHIP_I915GM|CHIP_I945G|CHIP_I945GM)) {
		i915_alloc_ifp(dev_priv, &bpa);
	} else if (IS_I965G(dev_priv) || IS_G33(dev_priv)) {
		i965_alloc_ifp(dev_priv, &bpa);	
	} else {
		int nsegs;
		/*
		 * I8XX has no flush page mechanism, we fake it by writing until
		 * the cache is empty. allocate a page to scribble on
		 */
		dev_priv->ifp.i8xx.kva = NULL;
		if (bus_dmamem_alloc(pa->pa_dmat, PAGE_SIZE, 0, 0,
		    &dev_priv->ifp.i8xx.seg, 1, &nsegs, BUS_DMA_WAITOK) == 0) {
			if (bus_dmamem_map(pa->pa_dmat, &dev_priv->ifp.i8xx.seg,
			    1, PAGE_SIZE, &dev_priv->ifp.i8xx.kva, 0) != 0) {
				bus_dmamem_free(pa->pa_dmat,
				    &dev_priv->ifp.i8xx.seg, nsegs);
				dev_priv->ifp.i8xx.kva = NULL;
			}
		}
	}

	inteldrm_detect_bit_6_swizzle(dev_priv, &bpa);
	/* Init HWS */
	if (!I915_NEED_GFX_HWS(dev_priv)) {
		if (i915_init_phys_hws(dev_priv, pa->pa_dmat) != 0) {
			printf(": couldn't alloc HWS page\n");
			return;
		}
	}

	printf(": %s\n", pci_intr_string(pa->pa_pc, dev_priv->ih));

	mtx_init(&dev_priv->user_irq_lock, IPL_TTY);
	mtx_init(&dev_priv->list_lock, IPL_NONE);
	mtx_init(&dev_priv->request_lock, IPL_NONE);
	mtx_init(&dev_priv->fence_lock, IPL_NONE);

	/* All intel chipsets need to be treated as agp, so just pass one */
	dev_priv->drmdev = drm_attach_pci(&inteldrm_driver, pa, 1, self);

	dev = (struct drm_device *)dev_priv->drmdev;

	/* XXX would be a lot nicer to get agp info before now */
	uvm_page_physload_flags(atop(dev->agp->base), atop(dev->agp->base +
	    dev->agp->info.ai_aperture_size), atop(dev->agp->base),
	    atop(dev->agp->base + dev->agp->info.ai_aperture_size), 0,
	    PHYSLOAD_DEVICE);
	/* array of vm pages that physload introduced. */
	dev_priv->pgs = PHYS_TO_VM_PAGE(dev->agp->base);
	KASSERT(dev_priv->pgs != NULL);
	/*
	 * XXX mark all pages write combining so user mmaps get the right
	 * bits. We really need a proper MI api for doing this, but for now
	 * this allows us to use PAT where available.
	 */
	for (i = 0; i < atop(dev->agp->info.ai_aperture_size); i++)
		atomic_setbits_int(&(dev_priv->pgs[i].pg_flags), PG_PMAP_WC);
	if (agp_init_map(dev_priv->bst, dev->agp->base,
	    dev->agp->info.ai_aperture_size, BUS_SPACE_MAP_LINEAR |
	    BUS_SPACE_MAP_PREFETCHABLE, &dev_priv->agph))
		panic("can't map aperture");
}

int
inteldrm_detach(struct device *self, int flags)
{
	struct drm_i915_private *dev_priv = (struct drm_i915_private *)self;

	/* this will quiesce any dma that's going on and kill the timeouts. */
	if (dev_priv->drmdev != NULL) {
		config_detach(dev_priv->drmdev, flags);
		dev_priv->drmdev = NULL;
	}

	i915_free_hws(dev_priv, dev_priv->dmat);

	if (IS_I9XX(dev_priv) && dev_priv->ifp.i9xx.bsh != NULL) {
		bus_space_unmap(dev_priv->ifp.i9xx.bst, dev_priv->ifp.i9xx.bsh,
		    PAGE_SIZE);
	} else if (dev_priv->flags & (CHIP_I830 | CHIP_I845G | CHIP_I85X |
	    CHIP_I865G) && dev_priv->ifp.i8xx.kva != NULL) {
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
inteldrm_activate(struct device *arg, int act)
{
	struct drm_i915_private	*dev_priv = (struct drm_i915_private *)arg;

	switch (act) {
	case DVACT_SUSPEND:
		inteldrm_save_state(dev_priv);
		break;
	case DVACT_RESUME:
		inteldrm_restore_state(dev_priv);
		break;
	}

	return (0);
}

struct cfattach inteldrm_ca = {
	sizeof(struct drm_i915_private), inteldrm_probe, inteldrm_attach,
	inteldrm_detach, inteldrm_activate
};

struct cfdriver inteldrm_cd = {
	0, "inteldrm", DV_DULL
};

int
inteldrm_ioctl(struct drm_device *dev, u_long cmd, caddr_t data,
    struct drm_file *file_priv)
{
	if (file_priv->authenticated == 1) {
		switch (cmd) {
		case DRM_IOCTL_I915_FLUSH:
			return (i915_flush_ioctl(dev, data, file_priv));
		case DRM_IOCTL_I915_BATCHBUFFER:
			return (i915_batchbuffer(dev, data, file_priv));
		case DRM_IOCTL_I915_IRQ_EMIT:
			return (i915_irq_emit(dev, data, file_priv));
		case DRM_IOCTL_I915_IRQ_WAIT:
			return (i915_irq_wait(dev, data, file_priv));
		case DRM_IOCTL_I915_GETPARAM:
			return (i915_getparam(dev, data, file_priv));
		case DRM_IOCTL_I915_CMDBUFFER:
			return (i915_cmdbuffer(dev, data, file_priv));
		case DRM_IOCTL_I915_GET_VBLANK_PIPE:
			return (i915_vblank_pipe_get(dev, data, file_priv));
		case DRM_IOCTL_I915_GEM_EXECBUFFER2:
			return (i915_gem_execbuffer2(dev, data, file_priv));
		case DRM_IOCTL_I915_GEM_BUSY:
			return (i915_gem_busy_ioctl(dev, data, file_priv));
		case DRM_IOCTL_I915_GEM_THROTTLE:
			return (i915_gem_ring_throttle(dev, file_priv));
		case DRM_IOCTL_I915_GEM_MMAP:
			return (i915_gem_gtt_map_ioctl(dev, data, file_priv));
		case DRM_IOCTL_I915_GEM_CREATE:
			return (i915_gem_create_ioctl(dev, data, file_priv));
		case DRM_IOCTL_I915_GEM_PREAD:
			return (i915_gem_pread_ioctl(dev, data, file_priv));
		case DRM_IOCTL_I915_GEM_PWRITE:
			return (i915_gem_pwrite_ioctl(dev, data, file_priv));
		case DRM_IOCTL_I915_GEM_SET_DOMAIN:
			return (i915_gem_set_domain_ioctl(dev, data,
			    file_priv));
		case DRM_IOCTL_I915_GEM_SET_TILING:
			return (i915_gem_set_tiling(dev, data, file_priv));
		case DRM_IOCTL_I915_GEM_GET_TILING:
			return (i915_gem_get_tiling(dev, data, file_priv));
		case DRM_IOCTL_I915_GEM_GET_APERTURE:
			return (i915_gem_get_aperture_ioctl(dev, data,
			    file_priv));
		case DRM_IOCTL_I915_GEM_MADVISE:
			return (i915_gem_madvise_ioctl(dev, data, file_priv));
		default:
			break;
		}
	}

	if (file_priv->master == 1) {
		switch (cmd) {
		case DRM_IOCTL_I915_SETPARAM:
			return (i915_setparam(dev, data, file_priv));
		case DRM_IOCTL_I915_INIT:
			return (i915_dma_init(dev, data, file_priv));
		case DRM_IOCTL_I915_HWS_ADDR:
			return (i915_set_status_page(dev, data, file_priv));
		/* Removed, but still used by userland, so just say `fine' */
		case DRM_IOCTL_I915_INIT_HEAP:
		case DRM_IOCTL_I915_DESTROY_HEAP:
		case DRM_IOCTL_I915_SET_VBLANK_PIPE:
			return (0);
		case DRM_IOCTL_I915_GEM_INIT:
			return (i915_gem_init_ioctl(dev, data, file_priv));
		case DRM_IOCTL_I915_GEM_ENTERVT:
			return (i915_gem_entervt_ioctl(dev, data, file_priv));
		case DRM_IOCTL_I915_GEM_LEAVEVT:
			return (i915_gem_leavevt_ioctl(dev, data, file_priv));
		case DRM_IOCTL_I915_GEM_PIN:
			return (i915_gem_pin_ioctl(dev, data, file_priv));
		case DRM_IOCTL_I915_GEM_UNPIN:
			return (i915_gem_unpin_ioctl(dev, data, file_priv));
		}
	}
	return (EINVAL);
}

int
inteldrm_intr(void *arg)
{
	drm_i915_private_t	*dev_priv = arg;
	struct drm_device	*dev = (struct drm_device *)dev_priv->drmdev;
	u_int32_t		 iir, pipea_stats = 0, pipeb_stats = 0;

	/* we're not set up, don't poke the hw */
	if (dev_priv->hw_status_page == NULL)
		return (0);
	/*
	 * lock is to protect from writes to PIPESTAT and IMR from other cores.
	 */
	mtx_enter(&dev_priv->user_irq_lock);
	iir = I915_READ(IIR);
	if (iir == 0) {
		mtx_leave(&dev_priv->user_irq_lock);
		return (0);
	}

	/*
	 * Clear the PIPE(A|B)STAT regs before the IIR
	 */
	if (iir & I915_DISPLAY_PIPE_A_EVENT_INTERRUPT) {
		pipea_stats = I915_READ(PIPEASTAT);
		I915_WRITE(PIPEASTAT, pipea_stats);
	}
	if (iir & I915_DISPLAY_PIPE_B_EVENT_INTERRUPT) {
		pipeb_stats = I915_READ(PIPEBSTAT);
		I915_WRITE(PIPEBSTAT, pipeb_stats);
	}
	if (iir & I915_RENDER_COMMAND_PARSER_ERROR_INTERRUPT)
		inteldrm_error(dev_priv);

	I915_WRITE(IIR, iir);
	(void)I915_READ(IIR); /* Flush posted writes */

	if (dev_priv->sarea_priv != NULL)
		dev_priv->sarea_priv->last_dispatch = READ_BREADCRUMB(dev_priv);

	if (iir & I915_USER_INTERRUPT) {
		wakeup(dev_priv);
		dev_priv->mm.hang_cnt = 0;
		timeout_add_msec(&dev_priv->mm.hang_timer, 750);
	}

	mtx_leave(&dev_priv->user_irq_lock);

	if (pipea_stats & I915_VBLANK_INTERRUPT_STATUS)
		drm_handle_vblank(dev, 0);

	if (pipeb_stats & I915_VBLANK_INTERRUPT_STATUS)
		drm_handle_vblank(dev, 1);

	return (1);
}

u_int32_t
inteldrm_read_hws(struct drm_i915_private *dev_priv, int reg)
{
	struct drm_device	*dev = (struct drm_device *)dev_priv->drmdev;
	u_int32_t		 val;

	/*
	 * When we eventually go GEM only we'll always have a dmamap, so this
	 * madness won't be for long.
	 */
	if (dev_priv->hws_dmamem)
		bus_dmamap_sync(dev->dmat, dev_priv->hws_dmamem->map, 0,
		    PAGE_SIZE, BUS_DMASYNC_POSTREAD);
	
	val = ((volatile u_int32_t *)(dev_priv->hw_status_page))[reg];

	if (dev_priv->hws_dmamem)
		bus_dmamap_sync(dev->dmat, dev_priv->hws_dmamem->map, 0,
		    PAGE_SIZE, BUS_DMASYNC_PREREAD);
	return (val);
}

/*
 * These five ring manipulation functions are protected by dev->dev_lock.
 */
int
inteldrm_wait_ring(struct drm_i915_private *dev_priv, int n)
{
	struct inteldrm_ring	*ring = &dev_priv->ring;
	u_int32_t		 acthd_reg, acthd, last_acthd, last_head;
	int			 i;

	acthd_reg = IS_I965G(dev_priv) ? ACTHD_I965 : ACTHD;
	last_head = I915_READ(PRB0_HEAD) & HEAD_ADDR;
	last_acthd = I915_READ(acthd_reg);

	/* ugh. Could really do with a proper, resettable timer here. */
	for (i = 0; i < 100000; i++) {
		ring->head = I915_READ(PRB0_HEAD) & HEAD_ADDR;
		acthd = I915_READ(acthd_reg);
		ring->space = ring->head - (ring->tail + 8);

		INTELDRM_VPRINTF("%s: head: %x tail: %x space: %x\n", __func__,
			ring->head, ring->tail, ring->space);
		if (ring->space < 0)
			ring->space += ring->size;
		if (ring->space >= n)
			return (0);

		/* Only timeout if the ring isn't chewing away on something */
		if (ring->head != last_head || acthd != last_acthd)
			i = 0;

		last_head = ring->head;
		last_acthd = acthd;
		delay(10);
	}

	return (EBUSY);
}

void
inteldrm_wrap_ring(struct drm_i915_private *dev_priv)
{
	u_int32_t	rem;;

	rem = dev_priv->ring.size - dev_priv->ring.tail;
	if (dev_priv->ring.space < rem &&
	    inteldrm_wait_ring(dev_priv, rem) != 0)
			return; /* XXX */

	dev_priv->ring.space -= rem;

	bus_space_set_region_4(dev_priv->bst, dev_priv->ring.bsh,
	    dev_priv->ring.woffset, MI_NOOP, rem / 4);

	dev_priv->ring.tail = 0;
}

void
inteldrm_begin_ring(struct drm_i915_private *dev_priv, int ncmd)
{
	int	bytes = 4 * ncmd;

	INTELDRM_VPRINTF("%s: %d\n", __func__, ncmd);
	if (dev_priv->ring.tail + bytes > dev_priv->ring.size)
		inteldrm_wrap_ring(dev_priv);
	if (dev_priv->ring.space < bytes)
		inteldrm_wait_ring(dev_priv, bytes);
	dev_priv->ring.woffset = dev_priv->ring.tail;
	dev_priv->ring.tail += bytes;
	dev_priv->ring.tail &= dev_priv->ring.size - 1;
	dev_priv->ring.space -= bytes;
}

void
inteldrm_out_ring(struct drm_i915_private *dev_priv, u_int32_t cmd)
{
	INTELDRM_VPRINTF("%s: %x\n", __func__, cmd);
	bus_space_write_4(dev_priv->bst, dev_priv->ring.bsh,
	    dev_priv->ring.woffset, cmd);
	/*
	 * don't need to deal with wrap here because we padded
	 * the ring out if we would wrap
	 */
	dev_priv->ring.woffset += 4;
}

void
inteldrm_advance_ring(struct drm_i915_private *dev_priv)
{
	INTELDRM_VPRINTF("%s: %x, %x\n", __func__, dev_priv->ring.wspace,
	    dev_priv->ring.woffset);
	DRM_MEMORYBARRIER();
	I915_WRITE(PRB0_TAIL, dev_priv->ring.tail);
}

void
inteldrm_update_ring(struct drm_i915_private *dev_priv)
{
	struct inteldrm_ring	*ring = &dev_priv->ring;

	ring->head = (I915_READ(PRB0_HEAD) & HEAD_ADDR);
	ring->tail = (I915_READ(PRB0_TAIL) & TAIL_ADDR);
	ring->space = ring->head - (ring->tail + 8);
	if (ring->space < 0)
		ring->space += ring->size;
	INTELDRM_VPRINTF("%s: head: %x tail: %x space: %x\n", __func__,
		ring->head, ring->tail, ring->space);
}

void
i915_alloc_ifp(struct drm_i915_private *dev_priv, struct pci_attach_args *bpa)
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
	} else if (bpa->pa_memex == NULL || extent_alloc(bpa->pa_memex,
	    PAGE_SIZE, PAGE_SIZE, 0, 0, 0, &addr) || bus_space_map(bpa->pa_memt,
	    addr, PAGE_SIZE, 0, &dev_priv->ifp.i9xx.bsh))
		goto nope;

	pci_conf_write(bpa->pa_pc, bpa->pa_tag, I915_IFPADDR, addr | 0x1);
	
	return;

nope:
	dev_priv->ifp.i9xx.bsh = NULL;
	printf(": no ifp ");
}

void
i965_alloc_ifp(struct drm_i915_private *dev_priv, struct pci_attach_args *bpa)
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
	} else if (bpa->pa_memex == NULL || extent_alloc(bpa->pa_memex,
	    PAGE_SIZE, PAGE_SIZE, 0, 0, 0, &addr) || bus_space_map(bpa->pa_memt,
	    addr, PAGE_SIZE, 0, &dev_priv->ifp.i9xx.bsh))
		goto nope;

	pci_conf_write(bpa->pa_pc, bpa->pa_tag, I965_IFPADDR + 4,
	    upper_32_bits(addr));
	pci_conf_write(bpa->pa_pc, bpa->pa_tag, I965_IFPADDR,
	    (addr & 0xffffffff) | 0x1);
	
	return;

nope:
	dev_priv->ifp.i9xx.bsh = NULL;
	printf(": no ifp ");
}

void
inteldrm_chipset_flush(struct drm_i915_private *dev_priv)
{
	/*
	 * Write to this flush page flushes the chipset write cache.
	 * The write will return when it is done.
	 */
	if (IS_I9XX(dev_priv)) {
	    if (dev_priv->ifp.i9xx.bsh != NULL)
		bus_space_write_4(dev_priv->ifp.i9xx.bst,
		    dev_priv->ifp.i9xx.bsh, 0, 1);
	} else {
		/*
		 * I8XX don't have a flush page mechanism, but do have the
		 * cache. Do it the bruteforce way. we write 1024 byes into
		 * the cache, then clflush them out so they'll kick the stuff
		 * we care about out of the chipset cache.
		 */
		if (dev_priv->ifp.i8xx.kva != NULL) {
			memset(dev_priv->ifp.i8xx.kva, 0, 1024);
			agp_flush_cache_range((vaddr_t)dev_priv->ifp.i8xx.kva,
			    1024);
		}
	}
}

void
inteldrm_lastclose(struct drm_device *dev)
{
	drm_i915_private_t	*dev_priv = dev->dev_private;
	struct vm_page		*p;
	int			 ret;

	ret = i915_gem_idle(dev_priv);
	if (ret)
		DRM_ERROR("failed to idle hardware: %d\n", ret);

	if (dev_priv->agpdmat != NULL) {
		/*
		 * make sure we nuke everything, we may have mappings that we've
		 * unrefed, but uvm has a reference to them for maps. Make sure
		 * they get unbound and any accesses will segfault.
		 * XXX only do ones in GEM.
		 */
		for (p = dev_priv->pgs; p < dev_priv->pgs +
		    (dev->agp->info.ai_aperture_size / PAGE_SIZE); p++)
			pmap_page_protect(p, VM_PROT_NONE);
		agp_bus_dma_destroy((struct agp_softc *)dev->agp->agpdev,
		    dev_priv->agpdmat);
	}
	dev_priv->agpdmat = NULL;


	dev_priv->sarea_priv = NULL;

	i915_dma_cleanup(dev);
}


int
i915_gem_init_ioctl(struct drm_device *dev, void *data,
		    struct drm_file *file_priv)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct drm_i915_gem_init *args = data;

	DRM_LOCK();

	if (args->gtt_start >= args->gtt_end ||
	    args->gtt_end > dev->agp->info.ai_aperture_size ||
	    (args->gtt_start & PAGE_MASK) != 0 ||
	    (args->gtt_end & PAGE_MASK) != 0) {
		DRM_UNLOCK();
		return (EINVAL);
	}
	/*
	 * putting stuff in the last page of the aperture can cause nasty
	 * problems with prefetch going into unassigned memory. Since we put
	 * a scratch page on all unused aperture pages, just leave the last
	 * page as a spill to prevent gpu hangs.
	 */
	if (args->gtt_end == dev->agp->info.ai_aperture_size)
		args->gtt_end -= 4096;

	if (agp_bus_dma_init((struct agp_softc *)dev->agp->agpdev,
	    dev->agp->base + args->gtt_start, dev->agp->base + args->gtt_end,
	    &dev_priv->agpdmat) != 0) {
		DRM_UNLOCK();
		return (ENOMEM);
	}

	dev->gtt_total = (uint32_t)(args->gtt_end - args->gtt_start);
	inteldrm_set_max_obj_size(dev_priv);

	DRM_UNLOCK();

	return 0;
}

void
inteldrm_set_max_obj_size(struct drm_i915_private *dev_priv)
{
	struct drm_device	*dev = (struct drm_device *)dev_priv->drmdev;

	/*
	 * Allow max obj size up to the size where ony 2 would fit the
	 * aperture, but some slop exists due to alignment etc
	 */
	dev_priv->max_gem_obj_size = (dev->gtt_total -
	    atomic_read(&dev->pin_memory)) * 3 / 4 / 2;

}

int
i915_gem_get_aperture_ioctl(struct drm_device *dev, void *data,
    struct drm_file *file_priv)
{
	struct drm_i915_gem_get_aperture	*args = data;

	/* we need a write lock here to make sure we get the right value */
	DRM_LOCK();
	args->aper_size = dev->gtt_total;
	args->aper_available_size = (args->aper_size -
	    atomic_read(&dev->pin_memory));
	DRM_UNLOCK();

	return (0);
}

/**
 * Creates a new mm object and returns a handle to it.
 */
int
i915_gem_create_ioctl(struct drm_device *dev, void *data,
    struct drm_file *file_priv)
{
	struct drm_i915_private		*dev_priv = dev->dev_private;
	struct drm_i915_gem_create	*args = data;
	struct drm_obj			*obj;
	int				 handle, ret;

	args->size = round_page(args->size);
	/*
	 * XXX to avoid copying between 2 objs more than half the aperture size
	 * we don't allow allocations that are that big. This will be fixed
	 * eventually by intelligently falling back to cpu reads/writes in
	 * such cases. (linux allows this but does cpu maps in the ddx instead).
	 */
	if (args->size > dev_priv->max_gem_obj_size)
		return (EFBIG);

	/* Allocate the new object */
	obj = drm_gem_object_alloc(dev, args->size);
	if (obj == NULL)
		return (ENOMEM);

	/* we give our reference to the handle */
	ret = drm_handle_create(file_priv, obj, &handle);

	if (ret == 0)
		args->handle = handle;
	else
		drm_unref(&obj->uobj);

	return (ret);
}

/**
 * Reads data from the object referenced by handle.
 *
 * On error, the contents of *data are undefined.
 */
int
i915_gem_pread_ioctl(struct drm_device *dev, void *data,
		     struct drm_file *file_priv)
{
	struct drm_i915_private		*dev_priv = dev->dev_private;
	struct drm_i915_gem_pread	*args = data;
	struct drm_obj			*obj;
	struct inteldrm_obj		*obj_priv;
	char				*vaddr;
	bus_space_handle_t	 	 bsh;
	bus_size_t		 	 bsize;
	voff_t				 offset;
	int				 ret;

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);
	if (obj == NULL)
		return (EBADF);
	DRM_READLOCK();
	drm_hold_object(obj);

	/*
	 * Bounds check source.
	 */
	if (args->offset > obj->size || args->size > obj->size ||
	    args->offset + args->size > obj->size) {
		ret = EINVAL;
		goto out;
	}

	ret = i915_gem_object_pin(obj, 0, 1);
	if (ret) {
		goto out;
	}
	ret = i915_gem_object_set_to_gtt_domain(obj, 0, 1);
	if (ret)
		goto unpin;

	obj_priv = (struct inteldrm_obj *)obj;
	offset = obj_priv->gtt_offset + args->offset;

	bsize = round_page(offset + args->size) - trunc_page(offset);

	if ((ret = agp_map_subregion(dev_priv->agph,
	    trunc_page(offset), bsize, &bsh)) != 0)
		goto unpin;
	vaddr = bus_space_vaddr(dev->bst, bsh);
	if (vaddr == NULL) {
		ret = EFAULT;
		goto unmap;
	}

	ret = copyout(vaddr + (offset & PAGE_MASK),
	    (char *)(uintptr_t)args->data_ptr, args->size);

unmap:
	agp_unmap_subregion(dev_priv->agph, bsh, bsize);
unpin:
	i915_gem_object_unpin(obj);
out:
	drm_unhold_and_unref(obj);
	DRM_READUNLOCK();

	return (ret);
}


/**
 * Writes data to the object referenced by handle.
 *
 * On error, the contents of the buffer that were to be modified are undefined.
 */
int
i915_gem_pwrite_ioctl(struct drm_device *dev, void *data,
    struct drm_file *file_priv)
{
	struct drm_i915_private		*dev_priv = dev->dev_private;
	struct drm_i915_gem_pwrite	*args = data;
	struct drm_obj			*obj;
	struct inteldrm_obj		*obj_priv;
	char 				*vaddr;
	bus_space_handle_t	 	 bsh;
	bus_size_t		 	 bsize;
	off_t			 	 offset;
	int				 ret = 0;

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);
	if (obj == NULL)
		return (EBADF);
	DRM_READLOCK();
	drm_hold_object(obj);

	/* Bounds check destination. */
	if (args->offset > obj->size || args->size > obj->size ||
	    args->offset + args->size > obj->size) {
		ret = EINVAL;
		goto out;
	}

	ret = i915_gem_object_pin(obj, 0, 1);
	if (ret) {
		goto out;
	}
	ret = i915_gem_object_set_to_gtt_domain(obj, 1, 1);
	if (ret)
		goto unpin;

	obj_priv = (struct inteldrm_obj *)obj;
	offset = obj_priv->gtt_offset + args->offset;
	bsize = round_page(offset + args->size) - trunc_page(offset);

	if ((ret = agp_map_subregion(dev_priv->agph,
	    trunc_page(offset), bsize, &bsh)) != 0)
		goto unpin;
	vaddr = bus_space_vaddr(dev_priv->bst, bsh);
	if (vaddr == NULL) {
		ret = EFAULT;
		goto unmap;
	}

	ret = copyin((char *)(uintptr_t)args->data_ptr,
	    vaddr + (offset & PAGE_MASK), args->size);


unmap:
	agp_unmap_subregion(dev_priv->agph, bsh, bsize);
unpin:
	i915_gem_object_unpin(obj);
out:
	drm_unhold_and_unref(obj);
	DRM_READUNLOCK();

	return (ret);
}

/**
 * Called when user space prepares to use an object with the CPU, either through
 * the mmap ioctl's mapping or a GTT mapping.
 */
int
i915_gem_set_domain_ioctl(struct drm_device *dev, void *data,
			  struct drm_file *file_priv)
{
	struct drm_i915_gem_set_domain	*args = data;
	struct drm_obj			*obj;
	u_int32_t			 read_domains = args->read_domains;
	u_int32_t			 write_domain = args->write_domain;
	int				 ret;

	/*
	 * Only handle setting domains to types we allow the cpu to see.
	 * while linux allows the CPU domain here, we only allow GTT since that
	 * is all that we let userland near.
	 * Also sanity check that having something in the write domain implies
	 * it's in the read domain, and only that read domain.
	 */
	if ((write_domain | read_domains)  & ~I915_GEM_DOMAIN_GTT ||
	    (write_domain != 0 && read_domains != write_domain))
		return (EINVAL);
		
	obj = drm_gem_object_lookup(dev, file_priv, args->handle);
	if (obj == NULL)
		return (EBADF);
	drm_hold_object(obj);

	ret = i915_gem_object_set_to_gtt_domain(obj, write_domain != 0, 1);

	drm_unhold_and_unref(obj);
	/*
	 * Silently promote `you're not bound, there was nothing to do'
	 * to success, since the client was just asking us to make sure
	 * everything was done.
	 */
	return ((ret == EINVAL) ? 0 : ret);
}

int
i915_gem_gtt_map_ioctl(struct drm_device *dev, void *data,
    struct drm_file *file_priv)
{
	struct drm_i915_gem_mmap	*args = data;
	struct drm_obj			*obj;
	struct inteldrm_obj		*obj_priv;
	vaddr_t				 addr;
	voff_t				 offset;
	vsize_t				 end, nsize;
	int				 ret;

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);
	if (obj == NULL)
		return (EBADF);

	/* Since we are doing purely uvm-related operations here we do
	 * not need to hold the object, a reference alone is sufficient
	 */
	obj_priv = (struct inteldrm_obj *)obj;

	/* Check size. Also ensure that the object is not purgeable */
	if (args->size == 0 || args->offset > obj->size || args->size >
	    obj->size || (args->offset + args->size) > obj->size ||
	    i915_obj_purgeable(obj_priv)) {
		ret = EINVAL;
		goto done;
	}

	end = round_page(args->offset + args->size);
	offset = trunc_page(args->offset);
	nsize = end - offset;

	/*
	 * We give our reference from object_lookup to the mmap, so only
	 * must free it in the case that the map fails.
	 */
	addr = uvm_map_hint(curproc, VM_PROT_READ | VM_PROT_WRITE);
	ret = uvm_map_p(&curproc->p_vmspace->vm_map, &addr, nsize, &obj->uobj,
	    offset, 0, UVM_MAPFLAG(UVM_PROT_RW, UVM_PROT_RW,
	    UVM_INH_SHARE, UVM_ADV_RANDOM, 0), curproc);

done:
	if (ret != 0)
		drm_unref(&obj->uobj);
	DRM_UNLOCK();

	if (ret == 0)
		args->addr_ptr = (uint64_t) addr + (args->offset & PAGE_MASK);

	return (ret);
}

/* called locked */
void
i915_gem_object_move_to_active(struct drm_obj *obj)
{
	struct drm_device	*dev = obj->dev;
	drm_i915_private_t	*dev_priv = dev->dev_private;
	struct inteldrm_obj	*obj_priv = (struct inteldrm_obj *)obj;
	struct inteldrm_fence	*reg;
	u_int32_t		 seqno = dev_priv->mm.next_gem_seqno;

	MUTEX_ASSERT_LOCKED(&dev_priv->request_lock);
	MUTEX_ASSERT_LOCKED(&dev_priv->list_lock);

	/* Add a reference if we're newly entering the active list. */
	if (!inteldrm_is_active(obj_priv)) {
		drm_ref(&obj->uobj);
		atomic_setbits_int(&obj->do_flags, I915_ACTIVE);
	}

	if (inteldrm_needs_fence(obj_priv)) {
		reg = &dev_priv->fence_regs[obj_priv->fence_reg];
		reg->last_rendering_seqno = seqno;
	}
	if (obj->write_domain)
		obj_priv->last_write_seqno = seqno;

	/* Move from whatever list we were on to the tail of execution. */
	i915_move_to_tail(obj_priv, &dev_priv->mm.active_list);
	obj_priv->last_rendering_seqno = seqno;
}

void
i915_gem_object_move_off_active(struct drm_obj *obj)
{
	struct drm_device	*dev = obj->dev;
	struct drm_i915_private	*dev_priv = dev->dev_private;
	struct inteldrm_obj	*obj_priv = (struct inteldrm_obj *)obj;
	struct inteldrm_fence	*reg;

	MUTEX_ASSERT_LOCKED(&dev_priv->list_lock);
	DRM_OBJ_ASSERT_LOCKED(obj);
	
	obj_priv->last_rendering_seqno = 0;
	/* if we have a fence register, then reset the seqno */
	if (obj_priv->fence_reg != I915_FENCE_REG_NONE) {
		reg = &dev_priv->fence_regs[obj_priv->fence_reg];
		reg->last_rendering_seqno = 0;
	}
	if (obj->write_domain == 0)
		obj_priv->last_write_seqno = 0;
}

/* If you call this on an object that you have held, you must have your own
 * reference, not just the reference from the active list.
 */
void
i915_gem_object_move_to_inactive(struct drm_obj *obj)
{
	struct drm_device	*dev = obj->dev;
	drm_i915_private_t	*dev_priv = dev->dev_private;

	mtx_enter(&dev_priv->list_lock);
	drm_lock_obj(obj);
	/* unlocks list lock and object lock */
	i915_gem_object_move_to_inactive_locked(obj);
}

/* called locked */
void
i915_gem_object_move_to_inactive_locked(struct drm_obj *obj)
{
	struct drm_device	*dev = obj->dev;
	drm_i915_private_t	*dev_priv = dev->dev_private;
	struct inteldrm_obj	*obj_priv = (struct inteldrm_obj *)obj;

	MUTEX_ASSERT_LOCKED(&dev_priv->list_lock);
	DRM_OBJ_ASSERT_LOCKED(obj);

	inteldrm_verify_inactive(dev_priv, __FILE__, __LINE__);
	if (obj_priv->pin_count != 0)
		i915_list_remove(obj_priv);
	else
		i915_move_to_tail(obj_priv, &dev_priv->mm.inactive_list);

	i915_gem_object_move_off_active(obj);
	atomic_clearbits_int(&obj->do_flags, I915_FENCED_EXEC);

	KASSERT((obj->do_flags & I915_GPU_WRITE) == 0);
	/* unlock becauase this unref could recurse */
	mtx_leave(&dev_priv->list_lock);
	if (inteldrm_is_active(obj_priv)) {
		atomic_clearbits_int(&obj->do_flags,
		    I915_ACTIVE);
		drm_unref_locked(&obj->uobj);
	} else {
		drm_unlock_obj(obj);
	}
	inteldrm_verify_inactive(dev_priv, __FILE__, __LINE__);
}

void
inteldrm_purge_obj(struct drm_obj *obj)
{
	DRM_ASSERT_HELD(obj);
	/*
	 * may sleep. We free here instead of deactivate (which
	 * the madvise() syscall would do) because in this case
	 * (userland bo cache and GL_APPLE_object_purgeable objects in
	 * OpenGL) the pages are defined to be freed if they were cleared
	 * so kill them and free up the memory
	 */
	simple_lock(&obj->uao->vmobjlock);
	obj->uao->pgops->pgo_flush(obj->uao, 0, obj->size,
	    PGO_ALLPAGES | PGO_FREE);
	simple_unlock(&obj->uao->vmobjlock);

	/*
	 * If flush failed, it may have halfway through, so just
	 * always mark as purged
	 */
	atomic_setbits_int(&obj->do_flags, I915_PURGED);
}

void
inteldrm_process_flushing(struct drm_i915_private *dev_priv,
    u_int32_t flush_domains)
{
	struct inteldrm_obj		*obj_priv, *next;

	MUTEX_ASSERT_LOCKED(&dev_priv->request_lock);
	mtx_enter(&dev_priv->list_lock);
	for (obj_priv = TAILQ_FIRST(&dev_priv->mm.gpu_write_list);
	    obj_priv != TAILQ_END(&dev_priv->mm.gpu_write_list);
	    obj_priv = next) {
		struct drm_obj *obj = &(obj_priv->obj);

		next = TAILQ_NEXT(obj_priv, write_list);

		if ((obj->write_domain & flush_domains)) {
			TAILQ_REMOVE(&dev_priv->mm.gpu_write_list,
			    obj_priv, write_list);
			atomic_clearbits_int(&obj->do_flags,
			     I915_GPU_WRITE);
			i915_gem_object_move_to_active(obj);
			obj->write_domain = 0;
			/* if we still need the fence, update LRU */
			if (inteldrm_needs_fence(obj_priv)) {
				KASSERT(obj_priv->fence_reg !=
				    I915_FENCE_REG_NONE);
				/* we have a fence, won't sleep, can't fail
				 * since we have the fence we no not need
				 * to have the object held
				 */
				i915_gem_get_fence_reg(obj, 1);
			}
				
		}
	}
	mtx_leave(&dev_priv->list_lock);
}

/**
 * Creates a new sequence number, emitting a write of it to the status page
 * plus an interrupt, which will trigger and interrupt if they are currently
 * enabled.
 *
 * Must be called with struct_lock held.
 *
 * Returned sequence numbers are nonzero on success.
 */
uint32_t
i915_add_request(struct drm_i915_private *dev_priv)
{
	struct inteldrm_request	*request;
	uint32_t		 seqno;
	int			 was_empty;

	MUTEX_ASSERT_LOCKED(&dev_priv->request_lock);

	request = drm_calloc(1, sizeof(*request));
	if (request == NULL) {
		printf("%s: failed to allocate request\n", __func__);
		return 0;
	}

	/* Grab the seqno we're going to make this request be, and bump the
	 * next (skipping 0 so it can be the reserved no-seqno value).
	 */
	seqno = dev_priv->mm.next_gem_seqno;
	dev_priv->mm.next_gem_seqno++;
	if (dev_priv->mm.next_gem_seqno == 0)
		dev_priv->mm.next_gem_seqno++;

	BEGIN_LP_RING(4);
	OUT_RING(MI_STORE_DWORD_INDEX);
	OUT_RING(I915_GEM_HWS_INDEX << MI_STORE_DWORD_INDEX_SHIFT);
	OUT_RING(seqno);

	OUT_RING(MI_USER_INTERRUPT);
	ADVANCE_LP_RING();

	DRM_DEBUG("%d\n", seqno);

	/* XXX request timing for throttle */
	request->seqno = seqno;
	was_empty = TAILQ_EMPTY(&dev_priv->mm.request_list);
	TAILQ_INSERT_TAIL(&dev_priv->mm.request_list, request, list);

	if (dev_priv->mm.suspended == 0) {
		if (was_empty)
			timeout_add_sec(&dev_priv->mm.retire_timer, 1);
		/* XXX was_empty? */
		timeout_add_msec(&dev_priv->mm.hang_timer, 750);
	}
	return seqno;
}

/**
 * Moves buffers associated only with the given active seqno from the active
 * to inactive list, potentially freeing them.
 *
 * called with and sleeps with the drm_lock.
 */
void
i915_gem_retire_request(struct drm_i915_private *dev_priv,
    struct inteldrm_request *request)
{
	struct inteldrm_obj	*obj_priv;

	MUTEX_ASSERT_LOCKED(&dev_priv->request_lock);
	mtx_enter(&dev_priv->list_lock);
	/* Move any buffers on the active list that are no longer referenced
	 * by the ringbuffer to the flushing/inactive lists as appropriate.  */
	while ((obj_priv  = TAILQ_FIRST(&dev_priv->mm.active_list)) != NULL) {
		struct drm_obj *obj = &obj_priv->obj;

		/* If the seqno being retired doesn't match the oldest in the
		 * list, then the oldest in the list must still be newer than
		 * this seqno.
		 */
		if (obj_priv->last_rendering_seqno != request->seqno)
			break;

		drm_lock_obj(obj);
		/*
		 * If we're now clean and can be read from, move inactive,
		 * else put on the flushing list to signify that we're not
		 * available quite yet.
		 */
		if (obj->write_domain != 0) {
			KASSERT(inteldrm_is_active(obj_priv));
			i915_move_to_tail(obj_priv,
			    &dev_priv->mm.flushing_list);
			i915_gem_object_move_off_active(obj);
			drm_unlock_obj(obj);
		} else {
			/* unlocks object for us and drops ref */
			i915_gem_object_move_to_inactive_locked(obj);
			mtx_enter(&dev_priv->list_lock);
		}
	}
	mtx_leave(&dev_priv->list_lock);
}

/**
 * This function clears the request list as sequence numbers are passed.
 */
void
i915_gem_retire_requests(struct drm_i915_private *dev_priv)
{
	struct inteldrm_request	*request;
	uint32_t		 seqno;

	if (dev_priv->hw_status_page == NULL)
		return;

	seqno = i915_get_gem_seqno(dev_priv);

	mtx_enter(&dev_priv->request_lock);
	while ((request = TAILQ_FIRST(&dev_priv->mm.request_list)) != NULL) {
		if (i915_seqno_passed(seqno, request->seqno) ||
		    dev_priv->mm.wedged) {
			TAILQ_REMOVE(&dev_priv->mm.request_list, request, list);
			i915_gem_retire_request(dev_priv, request);
			mtx_leave(&dev_priv->request_lock);

			drm_free(request);
			mtx_enter(&dev_priv->request_lock);
		} else
			break;
	}
	mtx_leave(&dev_priv->request_lock);
}

void
i915_gem_retire_work_handler(void *arg1, void *unused)
{
	drm_i915_private_t	*dev_priv = arg1;

	i915_gem_retire_requests(dev_priv);
	if (!TAILQ_EMPTY(&dev_priv->mm.request_list))
		timeout_add_sec(&dev_priv->mm.retire_timer, 1);
}

/**
 * Waits for a sequence number to be signaled, and cleans up the
 * request and object lists appropriately for that event.
 *
 * Called locked, sleeps with it.
 */
int
i915_wait_request(struct drm_i915_private *dev_priv, uint32_t seqno,
    int interruptible)
{
	int ret = 0;

	/* Check first because poking a wedged chip is bad. */
	if (dev_priv->mm.wedged)
		return (EIO);

	if (seqno == dev_priv->mm.next_gem_seqno) {
		mtx_enter(&dev_priv->request_lock);
		seqno = i915_add_request(dev_priv);
		mtx_leave(&dev_priv->request_lock);
		if (seqno == 0)
			return (ENOMEM);
	}

	if (!i915_seqno_passed(i915_get_gem_seqno(dev_priv), seqno)) {
		mtx_enter(&dev_priv->user_irq_lock);
		i915_user_irq_get(dev_priv);
		while (ret == 0) {
			if (i915_seqno_passed(i915_get_gem_seqno(dev_priv),
			    seqno) || dev_priv->mm.wedged)
				break;
			ret = msleep(dev_priv, &dev_priv->user_irq_lock,
			    PZERO | (interruptible ? PCATCH : 0), "gemwt", 0);
		}
		i915_user_irq_put(dev_priv);
		mtx_leave(&dev_priv->user_irq_lock);
	}
	if (dev_priv->mm.wedged)
		ret = EIO;

	/* Directly dispatch request retiring.  While we have the work queue
	 * to handle this, the waiter on a request often wants an associated
	 * buffer to have made it to the inactive list, and we would need
	 * a separate wait queue to handle that.
	 */
	if (ret == 0)
		i915_gem_retire_requests(dev_priv);

	return (ret);
}

/*
 * flush and invalidate the provided domains
 * if we have successfully queued a gpu flush, then we return a seqno from
 * the request. else (failed or just cpu flushed)  we return 0.
 */
u_int32_t
i915_gem_flush(struct drm_i915_private *dev_priv, uint32_t invalidate_domains,
    uint32_t flush_domains)
{
	uint32_t	cmd;
	int		ret = 0;

	if (flush_domains & I915_GEM_DOMAIN_CPU)
		inteldrm_chipset_flush(dev_priv);
	if (((invalidate_domains | flush_domains) & I915_GEM_GPU_DOMAINS) == 0) 
		return (0);
	/*
	 * read/write caches:
	 *
	 * I915_GEM_DOMAIN_RENDER is always invalidated, but is
	 * only flushed if MI_NO_WRITE_FLUSH is unset.  On 965, it is
	 * also flushed at 2d versus 3d pipeline switches.
	 *
	 * read-only caches:
	 *
	 * I915_GEM_DOMAIN_SAMPLER is flushed on pre-965 if
	 * MI_READ_FLUSH is set, and is always flushed on 965.
	 *
	 * I915_GEM_DOMAIN_COMMAND may not exist?
	 *
	 * I915_GEM_DOMAIN_INSTRUCTION, which exists on 965, is
	 * invalidated when MI_EXE_FLUSH is set.
	 *
	 * I915_GEM_DOMAIN_VERTEX, which exists on 965, is
	 * invalidated with every MI_FLUSH.
	 *
	 * TLBs:
	 *
	 * On 965, TLBs associated with I915_GEM_DOMAIN_COMMAND
	 * and I915_GEM_DOMAIN_CPU in are invalidated at PTE write and
	 * I915_GEM_DOMAIN_RENDER and I915_GEM_DOMAIN_SAMPLER
	 * are flushed at any MI_FLUSH.
	 */

	cmd = MI_FLUSH | MI_NO_WRITE_FLUSH;
	if ((invalidate_domains | flush_domains) &
	    I915_GEM_DOMAIN_RENDER)
		cmd &= ~MI_NO_WRITE_FLUSH;
	/*
	 * On the 965, the sampler cache always gets flushed
	 * and this bit is reserved.
	 */
	if (!IS_I965G(dev_priv) &&
	    invalidate_domains & I915_GEM_DOMAIN_SAMPLER)
		cmd |= MI_READ_FLUSH;
	if (invalidate_domains & I915_GEM_DOMAIN_INSTRUCTION)
		cmd |= MI_EXE_FLUSH;

	mtx_enter(&dev_priv->request_lock);
	BEGIN_LP_RING(2);
	OUT_RING(cmd);
	OUT_RING(MI_NOOP);
	ADVANCE_LP_RING();

	/* if this is a gpu flush, process the results */
	if (flush_domains & I915_GEM_GPU_DOMAINS) {
		inteldrm_process_flushing(dev_priv, flush_domains);
		ret = i915_add_request(dev_priv);
	}
	mtx_leave(&dev_priv->request_lock);

	return (ret);
}

/**
 * Unbinds an object from the GTT aperture.
 *
 * XXX track dirty and pass down to uvm (note, DONTNEED buffers are clean).
 */
int
i915_gem_object_unbind(struct drm_obj *obj, int interruptible)
{
	struct drm_device	*dev = obj->dev;
	drm_i915_private_t	*dev_priv = dev->dev_private;
	struct inteldrm_obj	*obj_priv = (struct inteldrm_obj *)obj;
	int			 ret = 0;

	DRM_ASSERT_HELD(obj);
	/*
	 * if it's already unbound, or we've already done lastclose, just
	 * let it happen. XXX does this fail to unwire?
	 */
	if (obj_priv->dmamap == NULL || dev_priv->agpdmat == NULL)
		return 0;

	if (obj_priv->pin_count != 0) {
		DRM_ERROR("Attempting to unbind pinned buffer\n");
		return (EINVAL);
	}

	KASSERT(!i915_obj_purged(obj_priv));

	/* Move the object to the CPU domain to ensure that
	 * any possible CPU writes while it's not in the GTT
	 * are flushed when we go to remap it. This will
	 * also ensure that all pending GPU writes are finished
	 * before we unbind.
	 */
	ret = i915_gem_object_set_to_cpu_domain(obj, 1, interruptible);
	if (ret)
		return ret;

	KASSERT(!inteldrm_is_active(obj_priv));

	/* if it's purgeable don't bother dirtying the pages */
	if (i915_obj_purgeable(obj_priv))
		atomic_clearbits_int(&obj->do_flags, I915_DIRTY);
	/*
	 * unload the map, then unwire the backing object.
	 */
	i915_gem_save_bit_17_swizzle(obj);
	bus_dmamap_unload(dev_priv->agpdmat, obj_priv->dmamap);
	uvm_objunwire(obj->uao, 0, obj->size);
	/* XXX persistent dmamap worth the memory? */
	bus_dmamap_destroy(dev_priv->agpdmat, obj_priv->dmamap);
	obj_priv->dmamap = NULL;
	free(obj_priv->dma_segs, M_DRM);
	obj_priv->dma_segs = NULL;
	/* XXX this should change whether we tell uvm the page is dirty */
	atomic_clearbits_int(&obj->do_flags, I915_DIRTY);

	obj_priv->gtt_offset = 0;
	atomic_dec(&dev->gtt_count);
	atomic_sub(obj->size, &dev->gtt_memory);

	/* Remove ourselves from any LRU list if present. */
	i915_list_remove((struct inteldrm_obj *)obj);

	if (i915_obj_purgeable(obj_priv))
		inteldrm_purge_obj(obj);

	return (0);
}

int
i915_gem_evict_something(struct drm_i915_private *dev_priv, size_t min_size,
    int interruptible)
{
	struct drm_obj		*obj;
	struct inteldrm_request	*request;
	struct inteldrm_obj	*obj_priv;
	u_int32_t		 seqno;
	int			 ret = 0, write_domain = 0;

	for (;;) {
		i915_gem_retire_requests(dev_priv);

		/* If there's an inactive buffer available now, grab it
		 * and be done.
		 */
		obj = i915_gem_find_inactive_object(dev_priv, min_size);
		if (obj != NULL) {
			obj_priv = (struct inteldrm_obj *)obj;
			/* find inactive object returns the object with a
			 * reference for us, and held
			 */
			KASSERT(obj_priv->pin_count == 0);
			KASSERT(!inteldrm_is_active(obj_priv));
			DRM_ASSERT_HELD(obj);

			/* Wait on the rendering and unbind the buffer. */
			ret = i915_gem_object_unbind(obj, interruptible);
			drm_unhold_and_unref(obj);
			return (ret);
		}

		/* If we didn't get anything, but the ring is still processing
		 * things, wait for one of those things to finish and hopefully
		 * leave us a buffer to evict.
		 */
		mtx_enter(&dev_priv->request_lock);
		if ((request = TAILQ_FIRST(&dev_priv->mm.request_list))
		    != NULL) {
			seqno = request->seqno;
			mtx_leave(&dev_priv->request_lock);

			ret = i915_wait_request(dev_priv, seqno, interruptible);
			if (ret)
				return (ret);

			continue;
		}
		mtx_leave(&dev_priv->request_lock);

		/* If we didn't have anything on the request list but there
		 * are buffers awaiting a flush, emit one and try again.
		 * When we wait on it, those buffers waiting for that flush
		 * will get moved to inactive.
		 */
		mtx_enter(&dev_priv->list_lock);
		TAILQ_FOREACH(obj_priv, &dev_priv->mm.flushing_list, list) {
			obj = &obj_priv->obj;
			if (obj->size >= min_size) {
				write_domain = obj->write_domain;
				break;
			}
			obj = NULL;
		}
		mtx_leave(&dev_priv->list_lock);

		if (write_domain) {
			if (i915_gem_flush(dev_priv, write_domain,
			    write_domain) == 0)
				return (ENOMEM);
			continue;
		}

		/*
		 * If we didn't do any of the above, there's no single buffer
		 * large enough to swap out for the new one, so just evict
		 * everything and start again. (This should be rare.)
		 */
		if (!TAILQ_EMPTY(&dev_priv->mm.inactive_list))
			return (i915_gem_evict_inactive(dev_priv));
		else
			return (i915_gem_evict_everything(dev_priv,
			    interruptible));
	}
	/* NOTREACHED */
}

struct drm_obj *
i915_gem_find_inactive_object(struct drm_i915_private *dev_priv,
    size_t min_size)
{
	struct drm_obj		*obj, *best = NULL, *first = NULL;
	struct inteldrm_obj	*obj_priv;

	/*
	 * We don't need references to the object as long as we hold the list
	 * lock, they won't disappear until we release the lock.
	 */
	mtx_enter(&dev_priv->list_lock);
	TAILQ_FOREACH(obj_priv, &dev_priv->mm.inactive_list, list) {
		obj = &obj_priv->obj;
		if (obj->size >= min_size) {
			if ((!inteldrm_is_dirty(obj_priv) ||
			    i915_obj_purgeable(obj_priv)) &&
			    (best == NULL || obj->size < best->size)) {
				best = obj;
				if (best->size == min_size)
					break;
			}
		}
		if (first == NULL)
			first = obj;
	}
	if (best == NULL)
		best = first;
	if (best) {
		drm_ref(&best->uobj);
		/*
		 * if we couldn't grab it, we may as well fail and go
		 * onto the next step for the sake of simplicity.
		 */
		if (drm_try_hold_object(best) == 0) {
			drm_unref(&best->uobj);
			best = NULL;
		}
	}
	mtx_leave(&dev_priv->list_lock);
	return (best);
}

int
i915_gem_evict_everything(struct drm_i915_private *dev_priv, int interruptible)
{
	u_int32_t	seqno;
	int		ret;

	if (TAILQ_EMPTY(&dev_priv->mm.inactive_list) &&
	    TAILQ_EMPTY(&dev_priv->mm.flushing_list) &&
	    TAILQ_EMPTY(&dev_priv->mm.active_list))
		return (ENOSPC);

	seqno = i915_gem_flush(dev_priv, I915_GEM_GPU_DOMAINS,
	    I915_GEM_GPU_DOMAINS);
	if (seqno == 0)
		return (ENOMEM);

	if ((ret = i915_wait_request(dev_priv, seqno, interruptible)) != 0 ||
	    (ret = i915_gem_evict_inactive(dev_priv)) != 0)
		return (ret);

	/*
	 * All lists should be empty because we flushed the whole queue, then
	 * we evicted the whole shebang, only pinned objects are still bound.
	 */
	KASSERT(TAILQ_EMPTY(&dev_priv->mm.inactive_list));
	KASSERT(TAILQ_EMPTY(&dev_priv->mm.flushing_list));
	KASSERT(TAILQ_EMPTY(&dev_priv->mm.active_list));

	return (0);
}
/*
 * return required GTT alignment for an object, taking into account potential
 * fence register needs
 */
bus_size_t
i915_gem_get_gtt_alignment(struct drm_obj *obj)
{
	struct drm_device	*dev = obj->dev;
	struct drm_i915_private	*dev_priv = dev->dev_private;
	struct inteldrm_obj	*obj_priv = (struct inteldrm_obj *)obj;
	bus_size_t		 start, i;

	/*
	 * Minimum alignment is 4k (GTT page size), but fence registers may
	 * modify this
	 */
	if (IS_I965G(dev_priv) || obj_priv->tiling_mode == I915_TILING_NONE)
		return (4096);

	/*
	 * Older chips need to be aligned to the size of the smallest fence
	 * register that can contain the object.
	 */
	if (IS_I9XX(dev_priv))
		start = 1024 * 1024;
	else
		start = 512 * 1024;

	for (i = start; i < obj->size; i <<= 1)
		;

	return (i);
}

void
i965_write_fence_reg(struct inteldrm_fence *reg)
{
	struct drm_obj		*obj = reg->obj;
	struct drm_device	*dev = obj->dev;
	drm_i915_private_t	*dev_priv = dev->dev_private;
	struct inteldrm_obj	*obj_priv = (struct inteldrm_obj *)obj;
	int			 regnum = obj_priv->fence_reg;
	u_int64_t		 val;

	val = (uint64_t)((obj_priv->gtt_offset + obj->size - 4096) &
		    0xfffff000) << 32;
	val |= obj_priv->gtt_offset & 0xfffff000;
	val |= ((obj_priv->stride / 128) - 1) << I965_FENCE_PITCH_SHIFT;
	if (obj_priv->tiling_mode == I915_TILING_Y)
		val |= 1 << I965_FENCE_TILING_Y_SHIFT;
	val |= I965_FENCE_REG_VALID;

	I915_WRITE64(FENCE_REG_965_0 + (regnum * 8), val);
}

void
i915_write_fence_reg(struct inteldrm_fence *reg)
{
	struct drm_obj		*obj = reg->obj;
	struct drm_device	*dev = obj->dev;
	drm_i915_private_t	*dev_priv = dev->dev_private;
	struct inteldrm_obj	*obj_priv = (struct inteldrm_obj *)obj;
	bus_size_t		 fence_reg;
	u_int32_t		 val;
	u_int32_t		 pitch_val;
	int			 regnum = obj_priv->fence_reg;
	int			 tile_width;

	if ((obj_priv->gtt_offset & ~I915_FENCE_START_MASK) ||
	    (obj_priv->gtt_offset & (obj->size - 1))) {
		DRM_ERROR("object 0x%lx not 1M or size (0x%zx) aligned\n",
		     obj_priv->gtt_offset, obj->size);
		return;
	}

	if (obj_priv->tiling_mode == I915_TILING_Y &&
	    HAS_128_BYTE_Y_TILING(dev_priv))
		tile_width = 128;
	else
		tile_width = 512;

	/* Note: pitch better be a power of two tile widths */
	pitch_val = obj_priv->stride / tile_width;
	pitch_val = ffs(pitch_val) - 1;

	if ((obj_priv->tiling_mode == I915_TILING_Y &&
	    HAS_128_BYTE_Y_TILING(dev_priv) &&
	    pitch_val > I830_FENCE_MAX_PITCH_VAL) ||
	    pitch_val > I915_FENCE_MAX_PITCH_VAL)
		printf("%s: invalid pitch provided"); /* XXX print more */

	val = obj_priv->gtt_offset;
	if (obj_priv->tiling_mode == I915_TILING_Y)
		val |= 1 << I830_FENCE_TILING_Y_SHIFT;
	val |= I915_FENCE_SIZE_BITS(obj->size);
	val |= pitch_val << I830_FENCE_PITCH_SHIFT;
	val |= I830_FENCE_REG_VALID;

	if (regnum < 8)
		fence_reg = FENCE_REG_830_0 + (regnum * 4);
	else
		fence_reg = FENCE_REG_945_8 + ((regnum - 8) * 4);
	I915_WRITE(fence_reg, val);
}

void
i830_write_fence_reg(struct inteldrm_fence *reg)
{
	struct drm_obj		*obj = reg->obj;
	struct drm_device	*dev = obj->dev;
	struct drm_i915_private	*dev_priv = dev->dev_private;
	struct inteldrm_obj	*obj_priv = (struct inteldrm_obj *)obj;
	int			 regnum = obj_priv->fence_reg;
	u_int32_t		 pitch_val, val;

	if ((obj_priv->gtt_offset & ~I830_FENCE_START_MASK) ||
	    (obj_priv->gtt_offset & (obj->size - 1))) {
		DRM_ERROR("object 0x%08x not 512K or size aligned 0x%lx\n",
		     obj_priv->gtt_offset, obj->size);
		return;
	}

	pitch_val = ffs(obj_priv->stride / 128) - 1;

	val = obj_priv->gtt_offset;
	if (obj_priv->tiling_mode == I915_TILING_Y)
		val |= 1 << I830_FENCE_TILING_Y_SHIFT;
	val |= I830_FENCE_SIZE_BITS(obj->size);
	val |= pitch_val << I830_FENCE_PITCH_SHIFT;
	val |= I830_FENCE_REG_VALID;

	I915_WRITE(FENCE_REG_830_0 + (regnum * 4), val);

}

/*
 * i915_gem_get_fence_reg - set up a fence reg for an object
 *
 * When mapping objects through the GTT, userspace wants to be able to write
 * to them without having to worry about swizzling if the object is tiled.
 *
 * This function walks the fence regs looking for a free one, stealing one
 * if it can't find any.
 *
 * It then sets up the reg based on the object's properties: address, pitch
 * and tiling format.
 */
int
i915_gem_get_fence_reg(struct drm_obj *obj, int interruptible)
{
	struct drm_device	*dev = obj->dev;
	struct drm_i915_private	*dev_priv = dev->dev_private;
	struct inteldrm_obj	*obj_priv = (struct inteldrm_obj *)obj;
	struct inteldrm_obj	*old_obj_priv = NULL;
	struct drm_obj		*old_obj = NULL;
	struct inteldrm_fence	*reg = NULL;
	int			 i, ret, avail;

	/* If our fence is getting used, just update our place in the LRU */
	if (obj_priv->fence_reg != I915_FENCE_REG_NONE) {
		mtx_enter(&dev_priv->fence_lock);
		reg = &dev_priv->fence_regs[obj_priv->fence_reg];

		TAILQ_REMOVE(&dev_priv->mm.fence_list, reg, list);
		TAILQ_INSERT_TAIL(&dev_priv->mm.fence_list, reg, list);
		mtx_leave(&dev_priv->fence_lock);
		return (0);
	}

	DRM_ASSERT_HELD(obj);
	switch (obj_priv->tiling_mode) {
	case I915_TILING_NONE:
		DRM_ERROR("allocating a fence for non-tiled object?\n");
		break;
	case I915_TILING_X:
		if (obj_priv->stride == 0)
			return (EINVAL);
		if (obj_priv->stride & (512 - 1))
			DRM_ERROR("object 0x%08x is X tiled but has non-512B"
			    " pitch\n", obj_priv->gtt_offset);
		break;
	case I915_TILING_Y:
		if (obj_priv->stride == 0)
			return (EINVAL);
		if (obj_priv->stride & (128 - 1))
			DRM_ERROR("object 0x%08x is Y tiled but has non-128B"
			    " pitch\n", obj_priv->gtt_offset);
		break;
	}

again:
	/* First try to find a free reg */
	avail = 0;
	mtx_enter(&dev_priv->fence_lock);
	for (i = dev_priv->fence_reg_start; i < dev_priv->num_fence_regs; i++) {
		reg = &dev_priv->fence_regs[i];
		if (reg->obj == NULL)
			break;

		old_obj_priv = (struct inteldrm_obj *)reg->obj;
		if (old_obj_priv->pin_count == 0)
			avail++;
	}

	/* None available, try to steal one or wait for a user to finish */
	if (i == dev_priv->num_fence_regs) {
		if (avail == 0) {
			mtx_leave(&dev_priv->fence_lock);
			return (ENOMEM);
		}

		TAILQ_FOREACH(reg, &dev_priv->mm.fence_list,
		    list) {
			old_obj = reg->obj;
			old_obj_priv = (struct inteldrm_obj *)old_obj;

			if (old_obj_priv->pin_count)
				continue;

			/* Ref it so that wait_rendering doesn't free it under
			 * us. if we can't hold it, it may change state soon
			 * so grab the next one.
			 */
			drm_ref(&old_obj->uobj);
			if (drm_try_hold_object(old_obj) == 0) {
				drm_unref(&old_obj->uobj);
				continue;
			}

			break;
		}
		mtx_leave(&dev_priv->fence_lock);

		/* if we tried all of them, give it another whirl. we failed to
		 * get a hold this go round.
		 */
		if (reg == NULL)
			goto again;

		ret = i915_gem_object_put_fence_reg(old_obj, interruptible);
		drm_unhold_and_unref(old_obj);
		if (ret != 0)
			return (ret);
		/* we should have freed one up now, so relock and re-search */
		goto again;
	}

	obj_priv->fence_reg = i;
	reg->obj = obj;
	TAILQ_INSERT_TAIL(&dev_priv->mm.fence_list, reg, list);

	if (IS_I965G(dev_priv))
		i965_write_fence_reg(reg);
	else if (IS_I9XX(dev_priv))
		i915_write_fence_reg(reg);
	else
		i830_write_fence_reg(reg);
	mtx_leave(&dev_priv->fence_lock);

	return 0;
}

int
i915_gem_object_put_fence_reg(struct drm_obj *obj, int interruptible)
{
	struct drm_device	*dev = obj->dev;
	struct drm_i915_private	*dev_priv = dev->dev_private;
	struct inteldrm_obj	*obj_priv = (struct inteldrm_obj *)obj;
	struct inteldrm_fence	*reg;
	int			 ret;

	DRM_ASSERT_HELD(obj);
	if (obj_priv->fence_reg == I915_FENCE_REG_NONE)
		return (0);

	/* 
	 * If the last execbuffer we did on the object needed a fence then
	 * we must emit a flush.
	 */
	if (inteldrm_needs_fence(obj_priv)) {
		ret = i915_gem_object_flush_gpu_write_domain(obj, 1,
		    interruptible, 0);
		if (ret != 0)
			return (ret);
	}

	/* if rendering is queued up that depends on the fence, wait for it */
	reg = &dev_priv->fence_regs[obj_priv->fence_reg];
	if (reg->last_rendering_seqno != 0) {
		ret = i915_wait_request(dev_priv, reg->last_rendering_seqno,
		    interruptible);
		if (ret != 0)
			return (ret);
	}

	/* tiling changed, must wipe userspace mappings */
	if ((obj->write_domain | obj->read_domains) & I915_GEM_DOMAIN_GTT) {
		inteldrm_wipe_mappings(obj);
		if (obj->write_domain == I915_GEM_DOMAIN_GTT)
			obj->write_domain = 0;
	}

	mtx_enter(&dev_priv->fence_lock);
	if (IS_I965G(dev_priv)) {
		I915_WRITE64(FENCE_REG_965_0 + (obj_priv->fence_reg * 8), 0);
	} else {
		u_int32_t fence_reg;

		if (obj_priv->fence_reg < 8)
			fence_reg = FENCE_REG_830_0 + obj_priv->fence_reg * 4;
		else 
			fence_reg = FENCE_REG_945_8 +
			    (obj_priv->fence_reg - 8) * 4;
		I915_WRITE(fence_reg , 0);
	}

	reg->obj = NULL;
	TAILQ_REMOVE(&dev_priv->mm.fence_list, reg, list);
	obj_priv->fence_reg = I915_FENCE_REG_NONE;
	mtx_leave(&dev_priv->fence_lock);
	atomic_clearbits_int(&obj->do_flags, I915_FENCE_INVALID);

	return (0);
}

int
inteldrm_fault(struct drm_obj *obj, struct uvm_faultinfo *ufi, off_t offset,
    vaddr_t vaddr, vm_page_t *pps, int npages, int centeridx,
    vm_prot_t access_type, int flags)
{
	struct drm_device	*dev = obj->dev;
	struct inteldrm_obj	*obj_priv = (struct inteldrm_obj *)obj;
	paddr_t			 paddr;
	int			 lcv, ret;
	int			 write = !!(access_type & VM_PROT_WRITE);
	vm_prot_t		 mapprot;
	boolean_t		 locked = TRUE;

	if (rw_enter(&dev->dev_lock, RW_NOSLEEP | RW_READ) != 0) {
		uvmfault_unlockall(ufi, NULL, &obj->uobj, NULL);
		DRM_READLOCK();
		locked = uvmfault_relock(ufi);
		if (locked)
			drm_lock_obj(obj);
	}
	if (locked)
		drm_hold_object_locked(obj);
	else { /* obj already unlocked */
		return (VM_PAGER_REFAULT);
	}

	/* we have a hold set on the object now, we can unlock so that we can
	 * sleep in binding and flushing.
	 */
	drm_unlock_obj(obj);
	

	if (obj_priv->dmamap != NULL &&
	    (obj_priv->gtt_offset & (i915_gem_get_gtt_alignment(obj) - 1) ||
	    (!i915_gem_object_fence_offset_ok(obj, obj_priv->tiling_mode)))) {
		/*
		 * pinned objects are defined to have a sane alignment which can
		 * not change.
		 */
		KASSERT(obj_priv->pin_count == 0);
		if ((ret = i915_gem_object_unbind(obj, 0)))
			goto error;
	}

	if (obj_priv->dmamap == NULL) {
		ret = i915_gem_object_bind_to_gtt(obj, 0, 0);
		if (ret) {
			printf("%s: failed to bind\n", __func__);
			goto error;
		}
		i915_gem_object_move_to_inactive(obj);
	}

	/*
	 * We could only do this on bind so allow for map_buffer_range
	 * unsynchronised objects (where buffer suballocation
	 * is done by the GL application, however it gives coherency problems
	 * normally.
	 */
	ret = i915_gem_object_set_to_gtt_domain(obj, write, 0);
	if (ret) {
		printf("%s: failed to set to gtt (%d)\n",
		    __func__, ret);
		goto error;
	}

	mapprot = ufi->entry->protection;
	/*
	 * if it's only a read fault, we only put ourselves into the gtt
	 * read domain, so make sure we fault again and set ourselves to write.
	 * this prevents us needing userland to do domain management and get
	 * it wrong, and makes us fully coherent with the gpu re mmap.
	 */
	if (write == 0)
		mapprot &= ~VM_PROT_WRITE;
	/* XXX try and  be more efficient when we do this */
	for (lcv = 0 ; lcv < npages ; lcv++, offset += PAGE_SIZE,
	    vaddr += PAGE_SIZE) {
		if ((flags & PGO_ALLPAGES) == 0 && lcv != centeridx)
			continue;

		if (pps[lcv] == PGO_DONTCARE)
			continue;

		paddr = dev->agp->base + obj_priv->gtt_offset + offset;

		UVMHIST_LOG(maphist,
		    "  MAPPING: device: pm=%p, va=0x%lx, pa=0x%lx, at=%ld",
		    ufi->orig_map->pmap, vaddr, (u_long)paddr, mapprot);
		if (pmap_enter(ufi->orig_map->pmap, vaddr, paddr,
		    mapprot, PMAP_CANFAIL | mapprot) != 0) {
			drm_unhold_object(obj);
			uvmfault_unlockall(ufi, ufi->entry->aref.ar_amap,
			    NULL, NULL);
			DRM_READUNLOCK();
			uvm_wait("intelflt");
			return (VM_PAGER_REFAULT);
		}
	}
	drm_unhold_object(obj);
	uvmfault_unlockall(ufi, ufi->entry->aref.ar_amap, NULL, NULL);
	DRM_READUNLOCK();
	pmap_update(ufi->orig_map->pmap);
	return (VM_PAGER_OK);

error:
	/*
	 * EIO means we're wedged so when we reset the gpu this will
	 * work, so don't segfault. XXX only on resettable chips
	 */
	drm_unhold_object(obj);
	uvmfault_unlockall(ufi, ufi->entry->aref.ar_amap, NULL, NULL);
	DRM_READUNLOCK();
	pmap_update(ufi->orig_map->pmap);
	return ((ret == EIO) ?  VM_PAGER_REFAULT : VM_PAGER_ERROR);
		
}

void
inteldrm_wipe_mappings(struct drm_obj *obj)
{
	struct inteldrm_obj	*obj_priv = (struct inteldrm_obj *)obj;
	struct drm_device	*dev = obj->dev;
	struct drm_i915_private	*dev_priv = dev->dev_private;
	struct vm_page		*pg;

	DRM_ASSERT_HELD(obj);
	/* make sure any writes hit the bus before we do whatever change
	 * that prompted us to kill the mappings.
	 */
	DRM_MEMORYBARRIER();
	/* nuke all our mappings. XXX optimise. */
	for (pg = &dev_priv->pgs[atop(obj_priv->gtt_offset)]; pg !=
	    &dev_priv->pgs[atop(obj_priv->gtt_offset + obj->size)]; pg++)
		pmap_page_protect(pg, VM_PROT_NONE);
}

/**
 * Finds free space in the GTT aperture and binds the object there.
 */
int
i915_gem_object_bind_to_gtt(struct drm_obj *obj, bus_size_t alignment,
    int interruptible)
{
	struct drm_device	*dev = obj->dev;
	drm_i915_private_t	*dev_priv = dev->dev_private;
	struct inteldrm_obj	*obj_priv = (struct inteldrm_obj *)obj;
	int			 ret;

	DRM_ASSERT_HELD(obj);
	if (dev_priv->agpdmat == NULL)
		return (EINVAL);
	if (alignment == 0) {
		alignment = i915_gem_get_gtt_alignment(obj);
	} else if (alignment & (i915_gem_get_gtt_alignment(obj) - 1)) {
		DRM_ERROR("Invalid object alignment requested %u\n", alignment);
		return (EINVAL);
	}

	/* Can't bind a purgeable buffer */
	if (i915_obj_purgeable(obj_priv)) {
		printf("tried to bind purgeable buffer");
		return (EINVAL);
	}

	if ((ret = bus_dmamap_create(dev_priv->agpdmat, obj->size, 1,
	    obj->size, 0, BUS_DMA_WAITOK, &obj_priv->dmamap)) != 0) {
		DRM_ERROR("Failed to create dmamap\n");
		return (ret);
	}
	agp_bus_dma_set_alignment(dev_priv->agpdmat, obj_priv->dmamap,
	    alignment);

 search_free:
	/*
	 * the helper function wires the uao then binds it to the aperture for 
	 * us, so all we have to do is set up the dmamap then load it.
	 */
	ret = drm_gem_load_uao(dev_priv->agpdmat, obj_priv->dmamap, obj->uao,
	    obj->size, BUS_DMA_WAITOK | obj_priv->dma_flags,
	    &obj_priv->dma_segs);
	/* XXX NOWAIT? */
	if (ret != 0) {
		/* If the gtt is empty and we're still having trouble
		 * fitting our object in, we're out of memory.
		 */
		if (TAILQ_EMPTY(&dev_priv->mm.inactive_list) &&
		    TAILQ_EMPTY(&dev_priv->mm.flushing_list) &&
		    TAILQ_EMPTY(&dev_priv->mm.active_list)) {
			DRM_ERROR("GTT full, but LRU list empty\n");
			goto error;
		}

		ret = i915_gem_evict_something(dev_priv, obj->size,
		    interruptible);
		if (ret != 0)
			goto error;
		goto search_free;
	}
	i915_gem_bit_17_swizzle(obj);

	obj_priv->gtt_offset = obj_priv->dmamap->dm_segs[0].ds_addr -
	    dev->agp->base;

	atomic_inc(&dev->gtt_count);
	atomic_add(obj->size, &dev->gtt_memory);

	/* Assert that the object is not currently in any GPU domain. As it
	 * wasn't in the GTT, there shouldn't be any way it could have been in
	 * a GPU cache
	 */
	KASSERT((obj->read_domains & I915_GEM_GPU_DOMAINS) == 0);
	KASSERT((obj->write_domain & I915_GEM_GPU_DOMAINS) == 0);

	return (0);

error:
	bus_dmamap_destroy(dev_priv->agpdmat, obj_priv->dmamap);
	obj_priv->dmamap = NULL;
	obj_priv->gtt_offset = 0;
	return (ret);
}

/*
 * Flush the GPU write domain for the object if dirty, then wait for the
 * rendering to complete. When this returns it is safe to unbind from the 
 * GTT or access from the CPU.
 */
int
i915_gem_object_flush_gpu_write_domain(struct drm_obj *obj, int pipelined,
    int interruptible, int write)
{
	struct drm_device	*dev = obj->dev;
	struct drm_i915_private	*dev_priv = dev->dev_private;
	struct inteldrm_obj	*obj_priv = (struct inteldrm_obj *)obj;
	u_int32_t		 seqno;
	int			 ret = 0;

	DRM_ASSERT_HELD(obj);
	if ((obj->write_domain & I915_GEM_GPU_DOMAINS) != 0) {
		/*
		 * Queue the GPU write cache flushing we need.
		 * This call will move stuff form the flushing list to the
		 * active list so all we need to is wait for it.
		 */
		(void)i915_gem_flush(dev_priv, 0, obj->write_domain);
		KASSERT(obj->write_domain == 0);
	}

	/* wait for queued rendering so we know it's flushed and bo is idle */
	if (pipelined == 0 && inteldrm_is_active(obj_priv)) {
		if (write) {
			seqno = obj_priv->last_rendering_seqno;
		} else {
			seqno = obj_priv->last_write_seqno;
		}
		ret =  i915_wait_request(dev_priv,
		    obj_priv->last_rendering_seqno, interruptible);
	}
	return (ret);
}

/* 
 * Moves a single object to the GTT and possibly write domain.
 *
 * This function returns when the move is complete, including waiting on
 * flushes to occur.
 */
int
i915_gem_object_set_to_gtt_domain(struct drm_obj *obj, int write,
    int interruptible)
{
	struct drm_device	*dev = (struct drm_device *)obj->dev;
	struct drm_i915_private	*dev_priv = dev->dev_private;
	struct inteldrm_obj	*obj_priv = (struct inteldrm_obj *)obj;
	int			 ret;

	DRM_ASSERT_HELD(obj);
	/* Not valid to be called on unbound objects. */
	if (obj_priv->dmamap == NULL)
		return (EINVAL);
	/* Wait on any GPU rendering and flushing to occur. */
	if ((ret = i915_gem_object_flush_gpu_write_domain(obj, 0,
	    interruptible, write)) != 0)
		return (ret);

	if (obj->write_domain == I915_GEM_DOMAIN_CPU) {
		/* clflush the pages, and flush chipset cache */
		bus_dmamap_sync(dev_priv->agpdmat, obj_priv->dmamap, 0,
		    obj->size, BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
		inteldrm_chipset_flush(dev_priv);
		obj->write_domain = 0;
	}

	/* We're accessing through the gpu, so grab a new fence register or
	 * update the LRU.
	 */
	if (obj->do_flags & I915_FENCE_INVALID) {
		ret = i915_gem_object_put_fence_reg(obj, interruptible);
		if (ret)
			return (ret);
	}
	if (obj_priv->tiling_mode != I915_TILING_NONE)
		ret = i915_gem_get_fence_reg(obj, interruptible);

	/*
	 * If we're writing through the GTT domain then the CPU and GPU caches
	 * will need to be invalidated at next use.
	 * It should now be out of any other write domains and we can update
	 * to the correct ones
	 */
	KASSERT((obj->write_domain & ~I915_GEM_DOMAIN_GTT) == 0);
	if (write) {
		obj->read_domains = obj->write_domain = I915_GEM_DOMAIN_GTT;
		atomic_setbits_int(&obj->do_flags, I915_DIRTY);
	} else {
		obj->read_domains |= I915_GEM_DOMAIN_GTT;
	}

	return (ret);
}

/*
 * Moves a single object to the CPU read and possibly write domain.
 *
 * This function returns when the move is complete, including waiting on
 * flushes to return.
 */
int
i915_gem_object_set_to_cpu_domain(struct drm_obj *obj, int write,
    int interruptible)
{
	struct drm_device	*dev = obj->dev;
	struct drm_i915_private	*dev_priv = dev->dev_private;
	struct inteldrm_obj	*obj_priv = (struct inteldrm_obj *)obj;
	int			 ret;

	DRM_ASSERT_HELD(obj);
	/* Wait on any GPU rendering and flushing to occur. */
	if ((ret = i915_gem_object_flush_gpu_write_domain(obj, 0,
	    interruptible, write)) != 0)
		return (ret);

	if (obj->write_domain == I915_GEM_DOMAIN_GTT ||
	    (write && obj->read_domains & I915_GEM_DOMAIN_GTT)) {
		/*
		 * No actual flushing is required for the GTT write domain.
		 * Writes to it immeditately go to main memory as far as we
		 * know, so there's no chipset flush. It also doesn't land
		 * in render cache.
		 */
		inteldrm_wipe_mappings(obj);
		if (obj->write_domain == I915_GEM_DOMAIN_GTT)
			obj->write_domain = 0;
	}

	/* remove the fence register since we're not using it anymore */
	if ((ret = i915_gem_object_put_fence_reg(obj, interruptible)) != 0)
		return (ret);

	/* Flush the CPU cache if it's still invalid. */
	if ((obj->read_domains & I915_GEM_DOMAIN_CPU) == 0) {
		bus_dmamap_sync(dev_priv->agpdmat, obj_priv->dmamap, 0,
		    obj->size, BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

		obj->read_domains |= I915_GEM_DOMAIN_CPU;
	}

	/*
	 * It should now be out of any other write domain, and we can update
	 * the domain value for our changes.
	 */
	KASSERT((obj->write_domain & ~I915_GEM_DOMAIN_CPU) == 0);

	/*
	 * If we're writing through the CPU, then the GPU read domains will
	 * need to be invalidated at next use.
	 */
	if (write)
		obj->read_domains = obj->write_domain = I915_GEM_DOMAIN_CPU;

	return (0);
}

/*
 * Set the next domain for the specified object. This
 * may not actually perform the necessary flushing/invaliding though,
 * as that may want to be batched with other set_domain operations
 *
 * This is (we hope) the only really tricky part of gem. The goal
 * is fairly simple -- track which caches hold bits of the object
 * and make sure they remain coherent. A few concrete examples may
 * help to explain how it works. For shorthand, we use the notation
 * (read_domains, write_domain), e.g. (CPU, CPU) to indicate the
 * a pair of read and write domain masks.
 *
 * Case 1: the batch buffer
 *
 *	1. Allocated
 *	2. Written by CPU
 *	3. Mapped to GTT
 *	4. Read by GPU
 *	5. Unmapped from GTT
 *	6. Freed
 *
 *	Let's take these a step at a time
 *
 *	1. Allocated
 *		Pages allocated from the kernel may still have
 *		cache contents, so we set them to (CPU, CPU) always.
 *	2. Written by CPU (using pwrite)
 *		The pwrite function calls set_domain (CPU, CPU) and
 *		this function does nothing (as nothing changes)
 *	3. Mapped by GTT
 *		This function asserts that the object is not
 *		currently in any GPU-based read or write domains
 *	4. Read by GPU
 *		i915_gem_execbuffer calls set_domain (COMMAND, 0).
 *		As write_domain is zero, this function adds in the
 *		current read domains (CPU+COMMAND, 0).
 *		flush_domains is set to CPU.
 *		invalidate_domains is set to COMMAND
 *		clflush is run to get data out of the CPU caches
 *		then i915_dev_set_domain calls i915_gem_flush to
 *		emit an MI_FLUSH and drm_agp_chipset_flush
 *	5. Unmapped from GTT
 *		i915_gem_object_unbind calls set_domain (CPU, CPU)
 *		flush_domains and invalidate_domains end up both zero
 *		so no flushing/invalidating happens
 *	6. Freed
 *		yay, done
 *
 * Case 2: The shared render buffer
 *
 *	1. Allocated
 *	2. Mapped to GTT
 *	3. Read/written by GPU
 *	4. set_domain to (CPU,CPU)
 *	5. Read/written by CPU
 *	6. Read/written by GPU
 *
 *	1. Allocated
 *		Same as last example, (CPU, CPU)
 *	2. Mapped to GTT
 *		Nothing changes (assertions find that it is not in the GPU)
 *	3. Read/written by GPU
 *		execbuffer calls set_domain (RENDER, RENDER)
 *		flush_domains gets CPU
 *		invalidate_domains gets GPU
 *		clflush (obj)
 *		MI_FLUSH and drm_agp_chipset_flush
 *	4. set_domain (CPU, CPU)
 *		flush_domains gets GPU
 *		invalidate_domains gets CPU
 *		flush_gpu_write (obj) to make sure all drawing is complete.
 *		This will include an MI_FLUSH to get the data from GPU
 *		to memory
 *		clflush (obj) to invalidate the CPU cache
 *		Another MI_FLUSH in i915_gem_flush (eliminate this somehow?)
 *	5. Read/written by CPU
 *		cache lines are loaded and dirtied
 *	6. Read written by GPU
 *		Same as last GPU access
 *
 * Case 3: The constant buffer
 *
 *	1. Allocated
 *	2. Written by CPU
 *	3. Read by GPU
 *	4. Updated (written) by CPU again
 *	5. Read by GPU
 *
 *	1. Allocated
 *		(CPU, CPU)
 *	2. Written by CPU
 *		(CPU, CPU)
 *	3. Read by GPU
 *		(CPU+RENDER, 0)
 *		flush_domains = CPU
 *		invalidate_domains = RENDER
 *		clflush (obj)
 *		MI_FLUSH
 *		drm_agp_chipset_flush
 *	4. Updated (written) by CPU again
 *		(CPU, CPU)
 *		flush_domains = 0 (no previous write domain)
 *		invalidate_domains = 0 (no new read domains)
 *	5. Read by GPU
 *		(CPU+RENDER, 0)
 *		flush_domains = CPU
 *		invalidate_domains = RENDER
 *		clflush (obj)
 *		MI_FLUSH
 *		drm_agp_chipset_flush
 */
void
i915_gem_object_set_to_gpu_domain(struct drm_obj *obj)
{
	struct drm_device	*dev = obj->dev;
	drm_i915_private_t	*dev_priv = dev->dev_private;
	struct inteldrm_obj	*obj_priv = (struct inteldrm_obj *)obj;
	u_int32_t		 invalidate_domains = 0;
	u_int32_t		 flush_domains = 0;

	DRM_ASSERT_HELD(obj);
	KASSERT((obj->pending_read_domains & I915_GEM_DOMAIN_CPU) == 0);
	KASSERT(obj->pending_write_domain != I915_GEM_DOMAIN_CPU);
	/*
	 * If the object isn't moving to a new write domain,
	 * let the object stay in multiple read domains
	 */
	if (obj->pending_write_domain == 0)
		obj->pending_read_domains |= obj->read_domains;
	else
		atomic_setbits_int(&obj->do_flags, I915_DIRTY);

	/*
	 * Flush the current write domain if
	 * the new read domains don't match. Invalidate
	 * any read domains which differ from the old
	 * write domain
	 */
	if (obj->write_domain &&
	    obj->write_domain != obj->pending_read_domains) {
		flush_domains |= obj->write_domain;
		invalidate_domains |= obj->pending_read_domains &
		    ~obj->write_domain;
	}
	/*
	 * Invalidate any read caches which may have
	 * stale data. That is, any new read domains.
	 */
	invalidate_domains |= obj->pending_read_domains & ~obj->read_domains;
	/* clflush the cpu now, gpu caches get queued. */
	if ((flush_domains | invalidate_domains) & I915_GEM_DOMAIN_CPU) {
		bus_dmamap_sync(dev_priv->agpdmat, obj_priv->dmamap, 0,
		    obj->size, BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	}
	if ((flush_domains | invalidate_domains) & I915_GEM_DOMAIN_GTT) {
		inteldrm_wipe_mappings(obj);
	}

	/* The actual obj->write_domain will be updated with
	 * pending_write_domain after we emit the accumulated flush for all of
	 * the domain changes in execuffer (which clears object's write
	 * domains). So if we have a current write domain that we aren't
	 * changing, set pending_write_domain to it.
	 */
	if (flush_domains == 0 && obj->pending_write_domain == 0 &&
	    (obj->pending_read_domains == obj->write_domain ||
	    obj->write_domain == 0))
		obj->pending_write_domain = obj->write_domain;
	obj->read_domains = obj->pending_read_domains;
	obj->pending_read_domains = 0;

	dev->invalidate_domains |= invalidate_domains;
	dev->flush_domains |= flush_domains;
}

/**
 * Pin an object to the GTT and evaluate the relocations landing in it.
 */
int
i915_gem_object_pin_and_relocate(struct drm_obj *obj,
    struct drm_file *file_priv, struct drm_i915_gem_exec_object2 *entry,
    struct drm_i915_gem_relocation_entry *relocs)
{
	struct drm_device			*dev = obj->dev;
	struct drm_i915_private			*dev_priv = dev->dev_private;
	struct drm_obj				*target_obj;
	struct inteldrm_obj			*obj_priv =
						    (struct inteldrm_obj *)obj;
	bus_space_handle_t			 bsh;
	int					 i, ret, needs_fence;

	DRM_ASSERT_HELD(obj);
	needs_fence = (entry->flags & EXEC_OBJECT_NEEDS_FENCE) &&
	    obj_priv->tiling_mode != I915_TILING_NONE;
	if (needs_fence)
		atomic_setbits_int(&obj->do_flags, I915_EXEC_NEEDS_FENCE);

	/* Choose the GTT offset for our buffer and put it there. */
	ret = i915_gem_object_pin(obj, (u_int32_t)entry->alignment,
	    needs_fence);
	if (ret)
		return ret;

	entry->offset = obj_priv->gtt_offset;

	/* Apply the relocations, using the GTT aperture to avoid cache
	 * flushing requirements.
	 */
	for (i = 0; i < entry->relocation_count; i++) {
		struct drm_i915_gem_relocation_entry *reloc = &relocs[i];
		struct inteldrm_obj *target_obj_priv;
		uint32_t reloc_val, reloc_offset;

		target_obj = drm_gem_object_lookup(obj->dev, file_priv,
		    reloc->target_handle);
		/* object must have come before us in the list */
		if (target_obj == NULL) {
			i915_gem_object_unpin(obj);
			return (EBADF);
		}
		if ((target_obj->do_flags & I915_IN_EXEC) == 0) {
			printf("%s: object not already in execbuffer\n",
			__func__);
			ret = EBADF;
			goto err;
		}
			
		target_obj_priv = (struct inteldrm_obj *)target_obj;

		/* The target buffer should have appeared before us in the
		 * exec_object list, so it should have a GTT space bound by now.
		 */
		if (target_obj_priv->dmamap == 0) {
			DRM_ERROR("No GTT space found for object %d\n",
				  reloc->target_handle);
			ret = EINVAL;
			goto err;
		}

		/* must be in one write domain and one only */
		if (reloc->write_domain & (reloc->write_domain - 1)) {
			ret = EINVAL;
			goto err;
		}
		if (reloc->read_domains & I915_GEM_DOMAIN_CPU ||
		    reloc->write_domain & I915_GEM_DOMAIN_CPU) {
			DRM_ERROR("relocation with read/write CPU domains: "
			    "obj %p target %d offset %d "
			    "read %08x write %08x", obj,
			    reloc->target_handle, (int)reloc->offset,
			    reloc->read_domains, reloc->write_domain);
			ret = EINVAL;
			goto err;
		}

		if (reloc->write_domain && target_obj->pending_write_domain &&
		    reloc->write_domain != target_obj->pending_write_domain) {
			DRM_ERROR("Write domain conflict: "
				  "obj %p target %d offset %d "
				  "new %08x old %08x\n",
				  obj, reloc->target_handle,
				  (int) reloc->offset,
				  reloc->write_domain,
				  target_obj->pending_write_domain);
			ret = EINVAL;
			goto err;
		}

		target_obj->pending_read_domains |= reloc->read_domains;
		target_obj->pending_write_domain |= reloc->write_domain;


		if (reloc->offset > obj->size - 4) {
			DRM_ERROR("Relocation beyond object bounds: "
				  "obj %p target %d offset %d size %d.\n",
				  obj, reloc->target_handle,
				  (int) reloc->offset, (int) obj->size);
			ret = EINVAL;
			goto err;
		}
		if (reloc->offset & 3) {
			DRM_ERROR("Relocation not 4-byte aligned: "
				  "obj %p target %d offset %d.\n",
				  obj, reloc->target_handle,
				  (int) reloc->offset);
			ret = EINVAL;
			goto err;
		}

		if (reloc->delta > target_obj->size) {
			DRM_ERROR("reloc larger than target\n");
			ret = EINVAL;
			goto err;
		}

		/* Map the page containing the relocation we're going to
		 * perform.
		 */
		reloc_offset = obj_priv->gtt_offset + reloc->offset;
		reloc_val = target_obj_priv->gtt_offset + reloc->delta;

		if (target_obj_priv->gtt_offset == reloc->presumed_offset) {
			drm_gem_object_unreference(target_obj);
			 continue;
		}

		ret = i915_gem_object_set_to_gtt_domain(obj, 1, 1);
		if (ret != 0)
			goto err;

		if ((ret = agp_map_subregion(dev_priv->agph,
		    trunc_page(reloc_offset), PAGE_SIZE, &bsh)) != 0) {
			DRM_ERROR("map failed...\n");
			goto err;
		}

		bus_space_write_4(dev_priv->bst, bsh, reloc_offset & PAGE_MASK,
		     reloc_val);

		reloc->presumed_offset = target_obj_priv->gtt_offset;

		agp_unmap_subregion(dev_priv->agph, bsh, PAGE_SIZE);
		drm_gem_object_unreference(target_obj);
	}

	return 0;

err:
	/* we always jump to here mid-loop */
	drm_gem_object_unreference(target_obj);
	i915_gem_object_unpin(obj);
	return (ret);
}

/** Dispatch a batchbuffer to the ring
 */
void
i915_dispatch_gem_execbuffer(struct drm_device *dev,
    struct drm_i915_gem_execbuffer2 *exec, uint64_t exec_offset)
{
	drm_i915_private_t	*dev_priv = dev->dev_private;
	uint32_t		 exec_start, exec_len;

	MUTEX_ASSERT_LOCKED(&dev_priv->request_lock);
	exec_start = (uint32_t)exec_offset + exec->batch_start_offset;
	exec_len = (uint32_t)exec->batch_len;

	if (IS_I830(dev_priv) || IS_845G(dev_priv)) {
		BEGIN_LP_RING(6);
		OUT_RING(MI_BATCH_BUFFER);
		OUT_RING(exec_start | MI_BATCH_NON_SECURE);
		OUT_RING(exec_start + exec_len - 4);
		OUT_RING(MI_NOOP);
	} else {
		BEGIN_LP_RING(4);
		if (IS_I965G(dev_priv)) {
			OUT_RING(MI_BATCH_BUFFER_START | (2 << 6) |
			    MI_BATCH_NON_SECURE_I965);
			OUT_RING(exec_start);
		} else {
			OUT_RING(MI_BATCH_BUFFER_START | (2 << 6));
			OUT_RING(exec_start | MI_BATCH_NON_SECURE);
		}
	}

	/*
	 * Ensure that the commands in the batch buffer are
	 * finished before the interrupt fires (from a subsequent request
	 * added). We get back a seqno representing the execution of the
	 * current buffer, which we can wait on.  We would like to mitigate
	 * these interrupts, likely by only creating seqnos occasionally
	 * (so that we have *some* interrupts representing completion of
	 * buffers that we can wait on when trying to clear up gtt space).
	 */
	OUT_RING(MI_FLUSH | MI_NO_WRITE_FLUSH);
	OUT_RING(MI_NOOP);
	ADVANCE_LP_RING();
	/*
	 * move to active associated all previous buffers with the seqno
	 * that this call will emit. so we don't need the return. If it fails
	 * then the next seqno will take care of it.
	 */
	(void)i915_add_request(dev_priv);

	inteldrm_verify_inactive(dev_priv, __FILE__, __LINE__);
#if 0
	/* The sampler always gets flushed on i965 (sigh) */
	if (IS_I965G(dev_priv))
		inteldrm_process_flushing(dev_priv, I915_GEM_DOMAIN_SAMPLER);
#endif
}

/* Throttle our rendering by waiting until the ring has completed our requests
 * emitted over 20 msec ago.
 *
 * This should get us reasonable parallelism between CPU and GPU but also
 * relatively low latency when blocking on a particular request to finish.
 */
int
i915_gem_ring_throttle(struct drm_device *dev, struct drm_file *file_priv)
{
#if 0
	struct inteldrm_file	*intel_file = (struct inteldrm_file *)file_priv;
	u_int32_t		 seqno;
#endif 
	int			 ret = 0;

	return ret;
}

int
i915_gem_get_relocs_from_user(struct drm_i915_gem_exec_object2 *exec_list,
    u_int32_t buffer_count, struct drm_i915_gem_relocation_entry **relocs)
{
	u_int32_t	reloc_count = 0, reloc_index = 0, i;
	int		ret;

	*relocs = NULL;
	for (i = 0; i < buffer_count; i++) {
		if (reloc_count + exec_list[i].relocation_count < reloc_count)
			return (EINVAL);
		reloc_count += exec_list[i].relocation_count;
	}

	if (reloc_count == 0)
		return (0);

	if (SIZE_MAX / reloc_count < sizeof(**relocs))
		return (EINVAL);
	*relocs = drm_alloc(reloc_count * sizeof(**relocs));
	for (i = 0; i < buffer_count; i++) {
		if ((ret = copyin((void *)(uintptr_t)exec_list[i].relocs_ptr,
		    &(*relocs)[reloc_index], exec_list[i].relocation_count *
		    sizeof(**relocs))) != 0) {
			drm_free(*relocs);
			*relocs = NULL;
			return (ret);
		}
		reloc_index += exec_list[i].relocation_count;
	}

	return (0);
}

int
i915_gem_put_relocs_to_user(struct drm_i915_gem_exec_object2 *exec_list,
    u_int32_t buffer_count, struct drm_i915_gem_relocation_entry *relocs)
{
	u_int32_t	reloc_count = 0, i;
	int		ret = 0;

	if (relocs == NULL)
		return (0);

	for (i = 0; i < buffer_count; i++) {
		if ((ret = copyout(&relocs[reloc_count],
		    (void *)(uintptr_t)exec_list[i].relocs_ptr,
		    exec_list[i].relocation_count * sizeof(*relocs))) != 0)
			break;
		reloc_count += exec_list[i].relocation_count;
	}

	drm_free(relocs);

	return (ret);
}

int
i915_gem_execbuffer2(struct drm_device *dev, void *data,
    struct drm_file *file_priv)
{
	drm_i915_private_t			*dev_priv = dev->dev_private;
	struct drm_i915_gem_execbuffer2		*args = data;
	struct drm_i915_gem_exec_object2	*exec_list = NULL;
	struct drm_i915_gem_relocation_entry	*relocs = NULL;
	struct inteldrm_obj			*obj_priv, *batch_obj_priv;
	struct drm_obj				**object_list = NULL;
	struct drm_obj				*batch_obj, *obj;
	size_t					 oflow;
	int					 ret, ret2, i;
	int					 pinned = 0, pin_tries;
	uint32_t				 reloc_index;

	/*
	 * Check for valid execbuffer offset. We can do this early because
	 * bound object are always page aligned, so only the start offset
	 * matters. Also check for integer overflow in the batch offset and size
	 */
	 if ((args->batch_start_offset | args->batch_len) & 0x7 ||
	    args->batch_start_offset + args->batch_len < args->batch_len ||
	    args->batch_start_offset + args->batch_len <
	    args->batch_start_offset)
		return (EINVAL);

	if (args->buffer_count < 1) {
		DRM_ERROR("execbuf with %d buffers\n", args->buffer_count);
		return (EINVAL);
	}
	/* Copy in the exec list from userland, check for overflow */
	oflow = SIZE_MAX / args->buffer_count;
	if (oflow < sizeof(*exec_list) || oflow < sizeof(*object_list))
		return (EINVAL);
	exec_list = drm_alloc(sizeof(*exec_list) * args->buffer_count);
	object_list = drm_alloc(sizeof(*object_list) * args->buffer_count);
	if (exec_list == NULL || object_list == NULL) {
		ret = ENOMEM;
		goto pre_mutex_err;
	}
	ret = copyin((void *)(uintptr_t)args->buffers_ptr, exec_list,
	    sizeof(*exec_list) * args->buffer_count);
	if (ret != 0)
		goto pre_mutex_err;

	ret = i915_gem_get_relocs_from_user(exec_list, args->buffer_count,
	    &relocs);
	if (ret != 0)
		goto pre_mutex_err;

	DRM_LOCK();
	inteldrm_verify_inactive(dev_priv, __FILE__, __LINE__);

	/* XXX check these before we copyin... but we do need the lock */
	if (dev_priv->mm.wedged) {
		ret = EIO;
		goto unlock;
	}

	if (dev_priv->mm.suspended) {
		ret = EBUSY;
		goto unlock;
	}

	/* Look up object handles */
	for (i = 0; i < args->buffer_count; i++) {
		object_list[i] = drm_gem_object_lookup(dev, file_priv,
		    exec_list[i].handle);
		obj = object_list[i];
		if (obj == NULL) {
			DRM_ERROR("Invalid object handle %d at index %d\n",
				   exec_list[i].handle, i);
			ret = EBADF;
			goto err;
		}
		if (obj->do_flags & I915_IN_EXEC) {
			DRM_ERROR("Object %p appears more than once in object_list\n",
			    object_list[i]);
			ret = EBADF;
			goto err;
		}
		atomic_setbits_int(&obj->do_flags, I915_IN_EXEC);
	}

	/* Pin and relocate */
	for (pin_tries = 0; ; pin_tries++) {
		ret = pinned = 0;
		reloc_index = 0;

		for (i = 0; i < args->buffer_count; i++) {
			object_list[i]->pending_read_domains = 0;
			object_list[i]->pending_write_domain = 0;
			drm_hold_object(object_list[i]);
			ret = i915_gem_object_pin_and_relocate(object_list[i],
			    file_priv, &exec_list[i], &relocs[reloc_index]);
			if (ret) {
				drm_unhold_object(object_list[i]);
				break;
			}
			pinned++;
			reloc_index += exec_list[i].relocation_count;
		}
		/* success */
		if (ret == 0)
			break;

		/* error other than GTT full, or we've already tried again */
		if (ret != ENOSPC || pin_tries >= 1)
			goto err;

		/*
		 * unpin all of our buffers and unhold them so they can be
		 * unbound so we can try and refit everything in the aperture.
		 */
		for (i = 0; i < pinned; i++) {
			i915_gem_object_unpin(object_list[i]);
			drm_unhold_object(object_list[i]);
		}
		pinned = 0;
		/* evict everyone we can from the aperture */
		ret = i915_gem_evict_everything(dev_priv, 1);
		if (ret)
			goto err;
	}

	/* If we get here all involved objects are referenced, pinned, relocated
	 * and held. Now we can finish off the exec processing.
	 *
	 * First, set the pending read domains for the batch buffer to
	 * command.
	 */
	batch_obj = object_list[args->buffer_count - 1];
	batch_obj_priv = (struct inteldrm_obj *)batch_obj;
	if (args->batch_start_offset + args->batch_len > batch_obj->size ||
	    batch_obj->pending_write_domain) {
		ret = EINVAL;
		goto err;
	}
	batch_obj->pending_read_domains |= I915_GEM_DOMAIN_COMMAND;

	inteldrm_verify_inactive(dev_priv, __FILE__, __LINE__);

	/*
	 * Zero the global flush/invalidate flags. These will be modified as
	 * new domains are computed for each object
	 */
	dev->invalidate_domains = 0;
	dev->flush_domains = 0;

	/* Compute new gpu domains and update invalidate/flush */
	for (i = 0; i < args->buffer_count; i++)
		i915_gem_object_set_to_gpu_domain(object_list[i]);

	inteldrm_verify_inactive(dev_priv, __FILE__, __LINE__);

	/* flush and invalidate any domains that need them. */
	(void)i915_gem_flush(dev_priv, dev->invalidate_domains,
	    dev->flush_domains);

	/*
	 * update the write domains, and fence/gpu write accounting information.
	 * Also do the move to active list here. The lazy seqno accounting will
	 * make sure that they have the correct seqno. If the add_request
	 * fails, then we will wait for a later batch (or one added on the
	 * wait), which will waste some time, but if we're that low on memory
	 * then we could fail in much worse ways.
	 */
	mtx_enter(&dev_priv->request_lock); /* to prevent races on next_seqno */
	mtx_enter(&dev_priv->list_lock);
	for (i = 0; i < args->buffer_count; i++) {
		obj = object_list[i];
		obj_priv = (struct inteldrm_obj *)obj;
		drm_lock_obj(obj);

		obj->write_domain = obj->pending_write_domain;
		/*
		 * if we have a write domain, add us to the gpu write list
		 * else we can remove the bit because it has been flushed.
		 */
		if (obj->do_flags & I915_GPU_WRITE)
			TAILQ_REMOVE(&dev_priv->mm.gpu_write_list, obj_priv,
			     write_list);
		if (obj->write_domain) {
			TAILQ_INSERT_TAIL(&dev_priv->mm.gpu_write_list,
			    obj_priv, write_list);
			atomic_setbits_int(&obj->do_flags, I915_GPU_WRITE);
		} else {
			atomic_clearbits_int(&obj->do_flags,
			     I915_GPU_WRITE);
		}
		/* if this batchbuffer needs a fence, then the object is
		 * counted as fenced exec. else any outstanding fence waits
		 * will just wait on the fence last_seqno.
		 */
		if (inteldrm_exec_needs_fence(obj_priv)) {
			atomic_setbits_int(&obj->do_flags,
			    I915_FENCED_EXEC);
		} else {
			atomic_clearbits_int(&obj->do_flags,
			    I915_FENCED_EXEC);
		}
			
		i915_gem_object_move_to_active(object_list[i]);
		drm_unlock_obj(obj);
	}
	mtx_leave(&dev_priv->list_lock);

	inteldrm_verify_inactive(dev_priv, __FILE__, __LINE__);

	/* Exec the batchbuffer */
	/*
	 * XXX make sure that this may never fail by preallocating the request.
	 */
	i915_dispatch_gem_execbuffer(dev, args, batch_obj_priv->gtt_offset);
	mtx_leave(&dev_priv->request_lock);

	inteldrm_verify_inactive(dev_priv, __FILE__, __LINE__);

	ret = copyout(exec_list, (void *)(uintptr_t)args->buffers_ptr,
	    sizeof(*exec_list) * args->buffer_count);

err:
	for (i = 0; i < args->buffer_count; i++) {
		if (object_list[i] == NULL)
			break;

		atomic_clearbits_int(&object_list[i]->do_flags, I915_IN_EXEC |
		    I915_EXEC_NEEDS_FENCE);
		if (i < pinned) {
			i915_gem_object_unpin(object_list[i]);
			drm_unhold_and_unref(object_list[i]);
		} else {
			drm_unref(&object_list[i]->uobj);
		}
	}

unlock:
	DRM_UNLOCK();

pre_mutex_err:
	/* update userlands reloc state. */
	ret2 = i915_gem_put_relocs_to_user(exec_list,
	    args->buffer_count, relocs);
	if (ret2 != 0 && ret == 0)
		ret = ret2;

	drm_free(object_list);
	drm_free(exec_list);

	return ret;
}

int
i915_gem_object_pin(struct drm_obj *obj, uint32_t alignment, int needs_fence)
{
	struct drm_device	*dev = obj->dev;
	struct inteldrm_obj	*obj_priv = (struct inteldrm_obj *)obj;
	int			 ret;

	DRM_ASSERT_HELD(obj);
	inteldrm_verify_inactive(dev_priv, __FILE__, __LINE__);
	/*
	 * if already bound, but alignment is unsuitable, unbind so we can
	 * fix it. Similarly if we have constraints due to fence registers,
	 * adjust if needed. Note that if we are already pinned we may as well
	 * fail because whatever depends on this alignment will render poorly
	 * otherwise, so just fail the pin (with a printf so we can fix a
	 * wrong userland).
	 */
	if (obj_priv->dmamap != NULL &&
	    ((alignment && obj_priv->gtt_offset & (alignment - 1)) ||
	    obj_priv->gtt_offset & (i915_gem_get_gtt_alignment(obj) - 1) ||
	    (needs_fence && !i915_gem_object_fence_offset_ok(obj,
	    obj_priv->tiling_mode)))) {
		/* if it is already pinned we sanitised the alignment then */
		KASSERT(obj_priv->pin_count == 0);
		if ((ret = i915_gem_object_unbind(obj, 1)))
			return (ret);
	}

	if (obj_priv->dmamap == NULL) {
		ret = i915_gem_object_bind_to_gtt(obj, alignment, 1);
		if (ret != 0)
			return (ret);
	}

	/*
	 * Pre-965 chips may need a fence register set up in order to
	 * handle tiling properly. GTT mapping may have blown it away so
	 * restore.
	 * With execbuf2 support we don't always need it, but if we do grab
	 * it.
	 */
	/* if we need a fence now, check that the one we may have is correct */
	if (needs_fence && obj->do_flags & I915_FENCE_INVALID) {
		ret= i915_gem_object_put_fence_reg(obj, 1);
		if (ret)
			return (ret);
	}
	if (needs_fence && obj_priv->tiling_mode != I915_TILING_NONE &&
	    (ret = i915_gem_get_fence_reg(obj, 1)) != 0)
		return (ret);

	/* If the object is not active and not pending a flush,
	 * remove it from the inactive list
	 */
	if (++obj_priv->pin_count == 1) {
		atomic_inc(&dev->pin_count);
		atomic_add(obj->size, &dev->pin_memory);
		if (!inteldrm_is_active(obj_priv))
			i915_list_remove(obj_priv);
	}
	inteldrm_verify_inactive(dev_priv, __FILE__, __LINE__);

	return (0);
}

void
i915_gem_object_unpin(struct drm_obj *obj)
{
	struct drm_device	*dev = obj->dev;
	struct inteldrm_obj	*obj_priv = (struct inteldrm_obj *)obj;

	inteldrm_verify_inactive(dev_priv, __FILE__, __LINE__);
	KASSERT(obj_priv->pin_count >= 1);
	KASSERT(obj_priv->dmamap != NULL);
	DRM_ASSERT_HELD(obj);

	/* If the object is no longer pinned, and is
	 * neither active nor being flushed, then stick it on
	 * the inactive list
	 */
	if (--obj_priv->pin_count == 0) {
		if (!inteldrm_is_active(obj_priv))
			i915_gem_object_move_to_inactive(obj);
		atomic_dec(&dev->pin_count);
		atomic_sub(obj->size, &dev->pin_memory);
	}
	inteldrm_verify_inactive(dev_priv, __FILE__, __LINE__);
}

int
i915_gem_pin_ioctl(struct drm_device *dev, void *data,
		   struct drm_file *file_priv)
{
	struct drm_i915_private	*dev_priv = dev->dev_private;
	struct drm_i915_gem_pin	*args = data;
	struct drm_obj		*obj;
	struct inteldrm_obj	*obj_priv;
	int			 ret = 0;

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);
	if (obj == NULL)
		return (EBADF);
	DRM_LOCK();
	drm_hold_object(obj);

	obj_priv = (struct inteldrm_obj *)obj;
	if (i915_obj_purgeable(obj_priv)) {
		printf("%s: pinning purgeable object\n", __func__);
		ret = EINVAL;
		goto out;
	}

	if (++obj_priv->user_pin_count == 1) {
		ret = i915_gem_object_pin(obj, args->alignment, 1);
		if (ret != 0)
			goto out;
		inteldrm_set_max_obj_size(dev_priv);
	}

	/* XXX - flush the CPU caches for pinned objects
	 * as the X server doesn't manage domains yet
	 */
	i915_gem_object_set_to_gtt_domain(obj, 1, 1);
	args->offset = obj_priv->gtt_offset;

out:
	drm_unhold_and_unref(obj);
	DRM_UNLOCK();

	return (ret);
}

int
i915_gem_unpin_ioctl(struct drm_device *dev, void *data,
		     struct drm_file *file_priv)
{
	struct drm_i915_private	*dev_priv = dev->dev_private;
	struct drm_i915_gem_pin	*args = data;
	struct inteldrm_obj	*obj_priv;
	struct drm_obj		*obj;
	int			 ret = 0;

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);
	if (obj == NULL)
		return (EBADF);

	DRM_LOCK();
	drm_hold_object(obj);

	obj_priv = (struct inteldrm_obj *)obj;
	if (obj_priv->user_pin_count == 0) {
		ret = EINVAL;
		goto out;
	}

	if (--obj_priv->user_pin_count == 0) {
		i915_gem_object_unpin(obj);
		inteldrm_set_max_obj_size(dev_priv);
	}

out:
	drm_unhold_and_unref(obj);
	DRM_UNLOCK();
	return (ret);
}

int
i915_gem_busy_ioctl(struct drm_device *dev, void *data,
    struct drm_file *file_priv)
{
	struct drm_i915_private		*dev_priv = dev->dev_private;
	struct drm_i915_gem_busy	*args = data;
	struct drm_obj			*obj;
	struct inteldrm_obj		*obj_priv;
	int				 ret = 0;

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);
	if (obj == NULL) {
		DRM_ERROR("Bad handle in i915_gem_busy_ioctl(): %d\n",
			  args->handle);
		return (EBADF);
	}

	/*
	 * Update the active list for the hardware's current position.
	 * otherwise this will only update on a delayed timer or when
	 * the irq is unmasked. This keeps our working set smaller.
	 */
	i915_gem_retire_requests(dev_priv);

	obj_priv = (struct inteldrm_obj *)obj;
	/* Count all active objects as busy, even if they are currently not
	 * used by the gpu. Users of this interface expect objects to eventually
	 * become non-busy without any further actions, therefore emit any
	 * necessary flushes here.
	 */
	args->busy = inteldrm_is_active(obj_priv);

	/* Unconditionally flush objects, even when the gpu still uses them.
	 * Userspace calling this function indicates that it wants to use
	 * this buffer sooner rather than later, so flushing now helps.
	 */
	if (obj->write_domain && i915_gem_flush(dev_priv,
	    obj->write_domain, obj->write_domain) == 0)
		ret = ENOMEM;

	drm_unref(&obj->uobj);
	return (ret);
}

int
i915_gem_madvise_ioctl(struct drm_device *dev, void *data,
    struct drm_file *file_priv)
{
	struct drm_i915_gem_madvise	*args = data;
	struct drm_obj			*obj;
	struct inteldrm_obj		*obj_priv;
	int				 need, ret = 0;

	switch (args->madv) {
	case I915_MADV_DONTNEED:
		need = 0;
		break;
	case I915_MADV_WILLNEED:
		need = 1;
		break;
	default:
		return (EINVAL);
	}

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);
	if (obj == NULL)
		return (EBADF);

	drm_hold_object(obj);
	obj_priv = (struct inteldrm_obj *)obj;

	/* invalid to madvise on a pinned BO */
	if (obj_priv->pin_count) {
		ret = EINVAL;
		goto out;
	}

	if (!i915_obj_purged(obj_priv)) {
		if (need) {
			atomic_clearbits_int(&obj->do_flags,
			    I915_DONTNEED);
		} else {
			atomic_setbits_int(&obj->do_flags, I915_DONTNEED);
		}
	}


	/* if the object is no longer bound, discard its backing storage */
	if (i915_obj_purgeable(obj_priv) && obj_priv->dmamap == NULL)
		inteldrm_purge_obj(obj);
		
	args->retained = !i915_obj_purged(obj_priv);

out:
	drm_unhold_and_unref(obj);

	return (ret);
}

int
i915_gem_init_object(struct drm_obj *obj)
{
	struct inteldrm_obj	*obj_priv = (struct inteldrm_obj *)obj;

	/*
	 * We've just allocated pages from the kernel,
	 * so they've just been written by the CPU with
	 * zeros. They'll need to be flushed before we
	 * use them with the GPU.
	 */
	obj->write_domain = I915_GEM_DOMAIN_CPU;
	obj->read_domains = I915_GEM_DOMAIN_CPU;

	/* normal objects don't need special treatment */
	obj_priv->dma_flags = 0;
	obj_priv->fence_reg = I915_FENCE_REG_NONE;

	return 0;
}

/*
 * NOTE all object unreferences in this driver need to hold the DRM_LOCK(),
 * because if they free they poke around in driver structures.
 */
void
i915_gem_free_object(struct drm_obj *obj)
{
	struct inteldrm_obj	*obj_priv = (struct inteldrm_obj *)obj;

	DRM_ASSERT_HELD(obj);
	while (obj_priv->pin_count > 0)
		i915_gem_object_unpin(obj);

	i915_gem_object_unbind(obj, 0);
	drm_free(obj_priv->bit_17);
	obj_priv->bit_17 = NULL;
	/* XXX dmatag went away? */
}

/* Clear out the inactive list and unbind everything in it. */
int
i915_gem_evict_inactive(struct drm_i915_private *dev_priv)
{
	struct inteldrm_obj	*obj_priv;
	int			 ret = 0;

	mtx_enter(&dev_priv->list_lock);
	while ((obj_priv = TAILQ_FIRST(&dev_priv->mm.inactive_list)) != NULL) {
		if (obj_priv->pin_count != 0) {
			ret = EINVAL;
			DRM_ERROR("Pinned object in unbind list\n");
			break;
		}
		/* reference it so that we can frob it outside the lock */
		drm_ref(&obj_priv->obj.uobj);
		mtx_leave(&dev_priv->list_lock);

		drm_hold_object(&obj_priv->obj);
		ret = i915_gem_object_unbind(&obj_priv->obj, 1);
		drm_unhold_and_unref(&obj_priv->obj);

		mtx_enter(&dev_priv->list_lock);
		if (ret)
			break;
	}
	mtx_leave(&dev_priv->list_lock);

	return (ret);
}

int
i915_gem_idle(struct drm_i915_private *dev_priv)
{
	struct drm_device	*dev = (struct drm_device *)dev_priv->drmdev;
	int			 ret;

	DRM_LOCK();
	if (dev_priv->mm.suspended || dev_priv->ring.ring_obj == NULL) { 
		DRM_UNLOCK();
		return (0);
	}

	/*
	 * If we're wedged, the workq will clear everything, else this will
	 * empty out the lists for us.
	 */
	if ((ret = i915_gem_evict_everything(dev_priv, 1)) != 0 && ret != ENOSPC) {
		DRM_UNLOCK();
		return (ret);
	}

	/* Hack!  Don't let anybody do execbuf while we don't control the chip.
	 * We need to replace this with a semaphore, or something.
	 */
	dev_priv->mm.suspended = 1;
	/* if we hung then the timer alredy fired. */
	timeout_del(&dev_priv->mm.hang_timer);

	inteldrm_update_ring(dev_priv);
	i915_gem_cleanup_ringbuffer(dev_priv);
	DRM_UNLOCK();

	/* this should be idle now */
	timeout_del(&dev_priv->mm.retire_timer);

	return 0;
}

int
i915_gem_init_hws(struct drm_i915_private *dev_priv)
{
	struct drm_device	*dev = (struct drm_device *)dev_priv->drmdev;
	struct drm_obj		*obj;
	struct inteldrm_obj	*obj_priv;
	int			 ret;

	/* If we need a physical address for the status page, it's already
	 * initialized at driver load time.
	 */
	if (!I915_NEED_GFX_HWS(dev_priv))
		return 0;

	obj = drm_gem_object_alloc(dev, 4096);
	if (obj == NULL) {
		DRM_ERROR("Failed to allocate status page\n");
		return (ENOMEM);
	}
	obj_priv = (struct inteldrm_obj *)obj;
	drm_hold_object(obj);
	/*
	 * snooped gtt mapping please .
	 * Normally this flag is only to dmamem_map, but it's been overloaded
	 * for the agp mapping
	 */
	obj_priv->dma_flags = BUS_DMA_COHERENT | BUS_DMA_READ;

	ret = i915_gem_object_pin(obj, 4096, 0);
	if (ret != 0) {
		drm_unhold_and_unref(obj);
		return ret;
	}

	dev_priv->hw_status_page = (void *)vm_map_min(kernel_map);
	obj->uao->pgops->pgo_reference(obj->uao);
	if ((ret = uvm_map(kernel_map, (vaddr_t *)&dev_priv->hw_status_page,
	    PAGE_SIZE, obj->uao, 0, 0, UVM_MAPFLAG(UVM_PROT_RW, UVM_PROT_RW,
	    UVM_INH_SHARE, UVM_ADV_RANDOM, 0))) != 0)
	if (ret != 0) {
		DRM_ERROR("Failed to map status page.\n");
		obj->uao->pgops->pgo_detach(obj->uao);
		memset(&dev_priv->hws_map, 0, sizeof(dev_priv->hws_map));
		i915_gem_object_unpin(obj);
		drm_unhold_and_unref(obj);
		return (EINVAL);
	}
	drm_unhold_object(obj);
	dev_priv->hws_obj = obj;
	memset(dev_priv->hw_status_page, 0, PAGE_SIZE);
	I915_WRITE(HWS_PGA, obj_priv->gtt_offset);
	I915_READ(HWS_PGA); /* posting read */
	DRM_DEBUG("hws offset: 0x%08x\n", obj_priv->gtt_offset);

	return 0;
}

void
i915_gem_cleanup_hws(struct drm_i915_private *dev_priv)
{
	struct drm_obj		*obj;

	if (dev_priv->hws_obj == NULL)
		return;

	obj = dev_priv->hws_obj;

	uvm_unmap(kernel_map, (vaddr_t)dev_priv->hw_status_page,
	    (vaddr_t)dev_priv->hw_status_page + PAGE_SIZE);
	drm_hold_object(obj);
	i915_gem_object_unpin(obj);
	drm_unhold_and_unref(obj);
	dev_priv->hws_obj = NULL;

	memset(&dev_priv->hws_map, 0, sizeof(dev_priv->hws_map));
	dev_priv->hw_status_page = NULL;

	/* Write high address into HWS_PGA when disabling. */
	I915_WRITE(HWS_PGA, 0x1ffff000);
}

int
i915_gem_init_ringbuffer(struct drm_i915_private *dev_priv)
{
	struct drm_device	*dev = (struct drm_device *)dev_priv->drmdev;
	struct drm_obj		*obj;
	struct inteldrm_obj	*obj_priv;
	int			 ret;

	ret = i915_gem_init_hws(dev_priv);
	if (ret != 0)
		return ret;

	obj = drm_gem_object_alloc(dev, 128 * 1024);
	if (obj == NULL) {
		DRM_ERROR("Failed to allocate ringbuffer\n");
		ret = ENOMEM;
		goto delhws;
	}
	drm_hold_object(obj);
	obj_priv = (struct inteldrm_obj *)obj;

	ret = i915_gem_object_pin(obj, 4096, 0);
	if (ret != 0)
		goto unref;

	/* Set up the kernel mapping for the ring. */
	dev_priv->ring.size = obj->size;

	if ((ret = agp_map_subregion(dev_priv->agph, obj_priv->gtt_offset,
	    obj->size, &dev_priv->ring.bsh)) != 0) {
		DRM_INFO("can't map ringbuffer\n");
		goto unpin;
	}
	dev_priv->ring.ring_obj = obj;

	if ((ret = inteldrm_start_ring(dev_priv)) != 0)
		goto unmap;

	drm_unhold_object(obj);
	return (0);

unmap:
	agp_unmap_subregion(dev_priv->agph, dev_priv->ring.bsh, obj->size);
unpin:
	memset(&dev_priv->ring, 0, sizeof(dev_priv->ring));
	i915_gem_object_unpin(obj);
unref:
	drm_unhold_and_unref(obj);
delhws:
	i915_gem_cleanup_hws(dev_priv);
	return (ret);
}

int
inteldrm_start_ring(struct drm_i915_private *dev_priv)
{
	struct drm_obj		*obj = dev_priv->ring.ring_obj;
	struct inteldrm_obj	*obj_priv = (struct inteldrm_obj *)obj;
	u_int32_t		 head;

	/* Stop the ring if it's running. */
	I915_WRITE(PRB0_CTL, 0);
	I915_WRITE(PRB0_TAIL, 0);
	I915_WRITE(PRB0_HEAD, 0);

	/* Initialize the ring. */
	I915_WRITE(PRB0_START, obj_priv->gtt_offset);
	head = I915_READ(PRB0_HEAD) & HEAD_ADDR;

	/* G45 ring initialisation fails to reset head to zero */
	if (head != 0) {
		I915_WRITE(PRB0_HEAD, 0);
		DRM_DEBUG("Forced ring head to zero ctl %08x head %08x"
		    "tail %08x start %08x\n", I915_READ(PRB0_CTL),
		    I915_READ(PRB0_HEAD), I915_READ(PRB0_TAIL),
		    I915_READ(PRB0_START));
	}

	I915_WRITE(PRB0_CTL, ((obj->size - 4096) & RING_NR_PAGES) |
	    RING_NO_REPORT | RING_VALID);

	head = I915_READ(PRB0_HEAD) & HEAD_ADDR;
	/* If ring head still != 0, the ring is dead */
	if (head != 0) {
		DRM_ERROR("Ring initialisation failed: ctl %08x head %08x"
		    "tail %08x start %08x\n", I915_READ(PRB0_CTL),
		    I915_READ(PRB0_HEAD), I915_READ(PRB0_TAIL),
		    I915_READ(PRB0_START));
		return (EIO);
	}

	/* Update our cache of the ring state */
	inteldrm_update_ring(dev_priv);

	if (IS_I9XX(dev_priv) && !IS_GEN3(dev_priv))
		I915_WRITE(MI_MODE, (VS_TIMER_DISPATCH) << 15 |
		    VS_TIMER_DISPATCH);

	return (0);
}

void
i915_gem_cleanup_ringbuffer(struct drm_i915_private *dev_priv)
{
	if (dev_priv->ring.ring_obj == NULL)
		return;
	agp_unmap_subregion(dev_priv->agph, dev_priv->ring.bsh,
	    dev_priv->ring.ring_obj->size);
	drm_hold_object(dev_priv->ring.ring_obj);
	i915_gem_object_unpin(dev_priv->ring.ring_obj);
	drm_unhold_and_unref(dev_priv->ring.ring_obj);
	dev_priv->ring.ring_obj = NULL;
	memset(&dev_priv->ring, 0, sizeof(dev_priv->ring));

	i915_gem_cleanup_hws(dev_priv);
}

int
i915_gem_entervt_ioctl(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	int ret;

	if (dev_priv->mm.wedged) {
		DRM_ERROR("Reenabling wedged hardware, good luck\n");
		dev_priv->mm.wedged = 0;
	}


	DRM_LOCK();
	dev_priv->mm.suspended = 0;

	ret = i915_gem_init_ringbuffer(dev_priv);
	if (ret != 0) {
		DRM_UNLOCK();
		return (ret);
	}

	/* gtt mapping means that the inactive list may not be empty */
	KASSERT(TAILQ_EMPTY(&dev_priv->mm.active_list));
	KASSERT(TAILQ_EMPTY(&dev_priv->mm.flushing_list));
	KASSERT(TAILQ_EMPTY(&dev_priv->mm.request_list));
	DRM_UNLOCK();

	drm_irq_install(dev);

	return (0);
}

int
i915_gem_leavevt_ioctl(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	struct drm_i915_private	*dev_priv = dev->dev_private;
	int			 ret;

	/* don't unistall if we fail, repeat calls on failure will screw us */
	if ((ret = i915_gem_idle(dev_priv)) == 0)
		drm_irq_uninstall(dev);
	return (ret);
}

void
inteldrm_timeout(void *arg)
{
	drm_i915_private_t *dev_priv = arg;

	if (workq_add_task(dev_priv->workq, 0, i915_gem_retire_work_handler,
	    dev_priv, NULL) == ENOMEM)
		DRM_ERROR("failed to run retire handler\n");
}

/*
 * handle hung hardware, or error interrupts. for now print debug info.
 */
void
inteldrm_error(struct drm_i915_private *dev_priv)
{
	u_int32_t	eir, ipeir, pgtbl_err, pipea_stats, pipeb_stats;
	u_int8_t	reset = GDRST_RENDER;

	eir = I915_READ(EIR);
	pipea_stats = I915_READ(PIPEASTAT);
	pipeb_stats = I915_READ(PIPEBSTAT);

	/*
	 * only actually check the error bits if we register one.
	 * else we just hung, stay silent.
	 */
	if (eir != 0) {
		printf("render error detected, EIR: 0x%08x\n", eir);
		if (IS_G4X(dev_priv)) {
			if (eir & (GM45_ERROR_MEM_PRIV | GM45_ERROR_CP_PRIV)) {
				ipeir = I915_READ(IPEIR_I965);

				printf("  IPEIR: 0x%08x\n",
				    I915_READ(IPEIR_I965));
				printf("  IPEHR: 0x%08x\n",
				    I915_READ(IPEHR_I965));
				printf("  INSTDONE: 0x%08x\n",
				    I915_READ(INSTDONE_I965));
				printf("  INSTPS: 0x%08x\n",
				    I915_READ(INSTPS));
				printf("  INSTDONE1: 0x%08x\n",
				    I915_READ(INSTDONE1));
				printf("  ACTHD: 0x%08x\n",
				    I915_READ(ACTHD_I965));
				I915_WRITE(IPEIR_I965, ipeir);
				(void)I915_READ(IPEIR_I965);
			}
			if (eir & GM45_ERROR_PAGE_TABLE) {
				pgtbl_err = I915_READ(PGTBL_ER);
				printf("page table error\n");
				printf("  PGTBL_ER: 0x%08x\n", pgtbl_err);
				I915_WRITE(PGTBL_ER, pgtbl_err);
				(void)I915_READ(PGTBL_ER);
				dev_priv->mm.wedged = 1;
				reset = GDRST_FULL;

			}
		} else if (IS_I9XX(dev_priv) && eir & I915_ERROR_PAGE_TABLE) {
			pgtbl_err = I915_READ(PGTBL_ER);
			printf("page table error\n");
			printf("  PGTBL_ER: 0x%08x\n", pgtbl_err);
			I915_WRITE(PGTBL_ER, pgtbl_err);
			(void)I915_READ(PGTBL_ER);
			dev_priv->mm.wedged = 1;
			reset = GDRST_FULL;
		}
		if (eir & I915_ERROR_MEMORY_REFRESH) {
			printf("memory refresh error\n");
			printf("PIPEASTAT: 0x%08x\n",
			       pipea_stats);
			printf("PIPEBSTAT: 0x%08x\n",
			       pipeb_stats);
			/* pipestat has already been acked */
		}
		if (eir & I915_ERROR_INSTRUCTION) {
			printf("instruction error\n");
			printf("  INSTPM: 0x%08x\n",
			       I915_READ(INSTPM));
			if (!IS_I965G(dev_priv)) {
				ipeir = I915_READ(IPEIR);

				printf("  IPEIR: 0x%08x\n",
				       I915_READ(IPEIR));
				printf("  IPEHR: 0x%08x\n",
					   I915_READ(IPEHR));
				printf("  INSTDONE: 0x%08x\n",
					   I915_READ(INSTDONE));
				printf("  ACTHD: 0x%08x\n",
					   I915_READ(ACTHD));
				I915_WRITE(IPEIR, ipeir);
				(void)I915_READ(IPEIR);
			} else {
				ipeir = I915_READ(IPEIR_I965);

				printf("  IPEIR: 0x%08x\n",
				       I915_READ(IPEIR_I965));
				printf("  IPEHR: 0x%08x\n",
				       I915_READ(IPEHR_I965));
				printf("  INSTDONE: 0x%08x\n",
				       I915_READ(INSTDONE_I965));
				printf("  INSTPS: 0x%08x\n",
				       I915_READ(INSTPS));
				printf("  INSTDONE1: 0x%08x\n",
				       I915_READ(INSTDONE1));
				printf("  ACTHD: 0x%08x\n",
				       I915_READ(ACTHD_I965));
				I915_WRITE(IPEIR_I965, ipeir);
				(void)I915_READ(IPEIR_I965);
			}
		}

		I915_WRITE(EIR, eir);
		eir = I915_READ(EIR);
	}
	/*
	 * nasty errors don't clear and need a reset, mask them until we reset
	 * else we'll get infinite interrupt storms.
	 */
	if (eir) {
		/* print so we know that we may want to reset here too */
		if (dev_priv->mm.wedged == 0)
			DRM_ERROR("EIR stuck: 0x%08x, masking\n", eir);
		I915_WRITE(EMR, I915_READ(EMR) | eir);
		I915_WRITE(IIR, I915_RENDER_COMMAND_PARSER_ERROR_INTERRUPT);
	}
	/*
	 * if it was a pagetable error, or we were called from hangcheck, then
	 * reset the gpu.
	 */
	if (dev_priv->mm.wedged && workq_add_task(dev_priv->workq, 0,
	    inteldrm_hung, dev_priv, (void *)(uintptr_t)reset) == ENOMEM)
		DRM_INFO("failed to schedule reset task\n");

}

void
inteldrm_hung(void *arg, void *reset_type)
{
	struct drm_i915_private	*dev_priv = arg;
	struct drm_device	*dev = (struct drm_device *)dev_priv->drmdev;
	struct inteldrm_obj	*obj_priv;
	u_int8_t		 reset = (u_int8_t)(uintptr_t)reset_type;

	DRM_LOCK();
	if (HAS_RESET(dev_priv)) {
		DRM_INFO("resetting gpu: ");
		inteldrm_965_reset(dev_priv, reset);
		printf("done!\n");
	} else
		printf("no reset function for chipset.\n");

	/*
	 * Clear out all of the requests and make everything inactive.
	 */
	i915_gem_retire_requests(dev_priv);

	/*
	 * Clear the active and flushing lists to inactive. Since
	 * we've reset the hardware then they're not going to get
	 * flushed or completed otherwise. nuke the domains since
	 * they're now irrelavent.
	 */
	mtx_enter(&dev_priv->list_lock);
	while ((obj_priv = TAILQ_FIRST(&dev_priv->mm.active_list)) != NULL) {
		drm_lock_obj(&obj_priv->obj);
		if (obj_priv->obj.write_domain & I915_GEM_GPU_DOMAINS) {
			TAILQ_REMOVE(&dev_priv->mm.gpu_write_list,
			    obj_priv, write_list);
			atomic_clearbits_int(&obj_priv->obj.do_flags,
			     I915_GPU_WRITE);
			obj_priv->obj.write_domain &= ~I915_GEM_GPU_DOMAINS;
		}
		/* unlocks object and list */
		i915_gem_object_move_to_inactive_locked(&obj_priv->obj);;
		mtx_enter(&dev_priv->list_lock);
	}

	while ((obj_priv = TAILQ_FIRST(&dev_priv->mm.flushing_list)) != NULL) {
		drm_lock_obj(&obj_priv->obj);
		if (obj_priv->obj.write_domain & I915_GEM_GPU_DOMAINS) {
			TAILQ_REMOVE(&dev_priv->mm.gpu_write_list,
			    obj_priv, write_list);
			atomic_clearbits_int(&obj_priv->obj.do_flags,
			    I915_GPU_WRITE);
			obj_priv->obj.write_domain &= ~I915_GEM_GPU_DOMAINS;
		}
		/* unlocks object and list */
		i915_gem_object_move_to_inactive_locked(&obj_priv->obj);
		mtx_enter(&dev_priv->list_lock);
	}
	mtx_leave(&dev_priv->list_lock);

	/* unbind everything */
	(void)i915_gem_evict_inactive(dev_priv);

	if (HAS_RESET(dev_priv))
		dev_priv->mm.wedged = 0;
	DRM_UNLOCK();
}

void
inteldrm_hangcheck(void *arg)
{
	struct drm_i915_private	*dev_priv = arg;
	u_int32_t		 acthd;

	/* are we idle? no requests, or ring is empty */
	if (TAILQ_EMPTY(&dev_priv->mm.request_list) ||
	    (I915_READ(PRB0_HEAD) & HEAD_ADDR) ==
	    (I915_READ(PRB0_TAIL) & TAIL_ADDR)) {
		dev_priv->mm.hang_cnt = 0;
		return;
	}

	if (IS_I965G(dev_priv))
		acthd = I915_READ(ACTHD_I965);
	else
		acthd = I915_READ(ACTHD);

	/* if we've hit ourselves before and the hardware hasn't moved, hung. */
	if (dev_priv->mm.last_acthd == acthd) {
		/* if that's twice we didn't hit it, then we're hung */
		if (++dev_priv->mm.hang_cnt >= 2) {
			dev_priv->mm.hang_cnt = 0;
			/* XXX atomic */
			dev_priv->mm.wedged = 1; 
			DRM_INFO("gpu hung!\n");
			/* XXX locking */
			wakeup(dev_priv);
			inteldrm_error(dev_priv);
			return;
		} 
	} else {
		dev_priv->mm.hang_cnt = 0;
	}

	dev_priv->mm.last_acthd = acthd;
	/* Set ourselves up again, in case we haven't added another batch */
	timeout_add_msec(&dev_priv->mm.hang_timer, 750);
}

void
i915_move_to_tail(struct inteldrm_obj *obj_priv, struct i915_gem_list *head)
{
	i915_list_remove(obj_priv);
	TAILQ_INSERT_TAIL(head, obj_priv, list);
	obj_priv->current_list = head;
}

void
i915_list_remove(struct inteldrm_obj *obj_priv)
{
	if (obj_priv->current_list != NULL)
		TAILQ_REMOVE(obj_priv->current_list, obj_priv, list);
	obj_priv->current_list = NULL;
}

/*
 *
 * Support for managing tiling state of buffer objects.
 *
 * The idea behind tiling is to increase cache hit rates by rearranging
 * pixel data so that a group of pixel accesses are in the same cacheline.
 * Performance improvement from doing this on the back/depth buffer are on
 * the order of 30%.
 *
 * Intel architectures make this somewhat more complicated, though, by
 * adjustments made to addressing of data when the memory is in interleaved
 * mode (matched pairs of DIMMS) to improve memory bandwidth.
 * For interleaved memory, the CPU sends every sequential 64 bytes
 * to an alternate memory channel so it can get the bandwidth from both.
 *
 * The GPU also rearranges its accesses for increased bandwidth to interleaved
 * memory, and it matches what the CPU does for non-tiled.  However, when tiled
 * it does it a little differently, since one walks addresses not just in the
 * X direction but also Y.  So, along with alternating channels when bit
 * 6 of the address flips, it also alternates when other bits flip --  Bits 9
 * (every 512 bytes, an X tile scanline) and 10 (every two X tile scanlines)
 * are common to both the 915 and 965-class hardware.
 *
 * The CPU also sometimes XORs in higher bits as well, to improve
 * bandwidth doing strided access like we do so frequently in graphics.  This
 * is called "Channel XOR Randomization" in the MCH documentation.  The result
 * is that the CPU is XORing in either bit 11 or bit 17 to bit 6 of its address
 * decode.
 *
 * All of this bit 6 XORing has an effect on our memory management,
 * as we need to make sure that the 3d driver can correctly address object
 * contents.
 *
 * If we don't have interleaved memory, all tiling is safe and no swizzling is
 * required.
 *
 * When bit 17 is XORed in, we simply refuse to tile at all.  Bit
 * 17 is not just a page offset, so as we page an object out and back in,
 * individual pages in it will have different bit 17 addresses, resulting in
 * each 64 bytes being swapped with its neighbor!
 *
 * Otherwise, if interleaved, we have to tell the 3d driver what the address
 * swizzling it needs to do is, since it's writing with the CPU to the pages
 * (bit 6 and potentially bit 11 XORed in), and the GPU is reading from the
 * pages (bit 6, 9, and 10 XORed in), resulting in a cumulative bit swizzling
 * required by the CPU of XORing in bit 6, 9, 10, and potentially 11, in order
 * to match what the GPU expects.
 */

#define MCHBAR_I915	0x44
#define MCHBAR_I965	0x48
#define	MCHBAR_SIZE	(4*4096)

#define	DEVEN_REG	0x54
#define	DEVEN_MCHBAR_EN	(1 << 28)


/* 
 * Check the MCHBAR on the host bridge is enabled, and if not allocate it.
 * we do not need to actually map it because we access the bar through it's
 * mirror on the IGD, however, if it is disabled or not allocated then
 * the mirror does not work. *sigh*.
 *
 * we return a trinary state:
 * 0 = already enabled, or can not enable
 * 1 = enabled, needs disable
 * 2 = enabled, needs disable and free.
 */
int
inteldrm_setup_mchbar(struct drm_i915_private *dev_priv,
    struct pci_attach_args *bpa)
{
	u_int64_t	mchbar_addr;
	pcireg_t	tmp, low, high = 0;
	u_long		addr;
	int		reg = IS_I965G(dev_priv) ? MCHBAR_I965 : MCHBAR_I915;
	int		ret = 1, enabled = 0;

	if (IS_I915G(dev_priv) || IS_I915GM(dev_priv)) {
		tmp = pci_conf_read(bpa->pa_pc, bpa->pa_tag, DEVEN_REG);
		enabled = !!(tmp & DEVEN_MCHBAR_EN);
	} else {
		tmp = pci_conf_read(bpa->pa_pc, bpa->pa_tag, reg);
		enabled = tmp & 1;
	}

	if (enabled) {
		return (0);
	}

	if (IS_I965G(dev_priv))
		high = pci_conf_read(bpa->pa_pc, bpa->pa_tag, reg + 4);
	low = pci_conf_read(bpa->pa_pc, bpa->pa_tag, reg);
	mchbar_addr = ((u_int64_t)high << 32) | low;

	/*
	 * XXX need to check to see if it's allocated in the pci resources,
	 * right now we just check to see if there's any address there
	 *
	 * if there's no address, then we allocate one.
	 * note that we can't just use pci_mapreg_map here since some intel
	 * BARs are special in that they set bit 0 to show they're enabled,
	 * this is not handled by generic pci code.
	 */
	if (mchbar_addr == 0) {
		addr = (u_long)mchbar_addr;
		if (bpa->pa_memex == NULL || extent_alloc(bpa->pa_memex,
	            MCHBAR_SIZE, MCHBAR_SIZE, 0, 0, 0, &addr)) {
			return (0); /* just say we don't need to disable */
		} else {
			mchbar_addr = addr;
			ret = 2;
			/* We've allocated it, now fill in the BAR again */
			if (IS_I965G(dev_priv))
				pci_conf_write(bpa->pa_pc, bpa->pa_tag,
				    reg + 4, upper_32_bits(mchbar_addr));
			pci_conf_write(bpa->pa_pc, bpa->pa_tag,
			    reg, mchbar_addr & 0xffffffff);
		}
	}
	/* set the enable bit */
	if (IS_I915G(dev_priv) || IS_I915GM(dev_priv)) {
		pci_conf_write(bpa->pa_pc, bpa->pa_tag, DEVEN_REG,
		    tmp | DEVEN_MCHBAR_EN);
	} else {
		tmp = pci_conf_read(bpa->pa_pc, bpa->pa_tag, reg);
		pci_conf_write(bpa->pa_pc, bpa->pa_tag, reg, tmp | 1);
	}

	return (ret);
}

/*
 * we take the trinary returned from inteldrm_setup_mchbar and clean up after
 * it.
 */
void
inteldrm_teardown_mchbar(struct drm_i915_private *dev_priv,
    struct pci_attach_args *bpa, int disable)
{
	u_int64_t	mchbar_addr;
	pcireg_t	tmp, low, high = 0;
	int		reg = IS_I965G(dev_priv) ? MCHBAR_I965 : MCHBAR_I915;

	switch(disable) {
	case 2:
		if (IS_I965G(dev_priv))
			high = pci_conf_read(bpa->pa_pc, bpa->pa_tag, reg + 4);
		low = pci_conf_read(bpa->pa_pc, bpa->pa_tag, reg);
		mchbar_addr = ((u_int64_t)high << 32) | low;
		if (bpa->pa_memex)
			extent_free(bpa->pa_memex, mchbar_addr, MCHBAR_SIZE, 0);
		/* FALLTHROUGH */
	case 1:
		if (IS_I915G(dev_priv) || IS_I915GM(dev_priv)) {
			tmp = pci_conf_read(bpa->pa_pc, bpa->pa_tag, DEVEN_REG);
			tmp &= ~DEVEN_MCHBAR_EN;
			pci_conf_write(bpa->pa_pc, bpa->pa_tag, DEVEN_REG, tmp);
		} else {
			tmp = pci_conf_read(bpa->pa_pc, bpa->pa_tag, reg);
			tmp &= ~1;
			pci_conf_write(bpa->pa_pc, bpa->pa_tag, reg, tmp);
		}
		break;
	case 0:
	default:
		break;
	};
}

/**
 * Detects bit 6 swizzling of address lookup between IGD access and CPU
 * access through main memory.
 */
void
inteldrm_detect_bit_6_swizzle(drm_i915_private_t *dev_priv,
    struct pci_attach_args *bpa)
{
	uint32_t	swizzle_x = I915_BIT_6_SWIZZLE_UNKNOWN;
	uint32_t	swizzle_y = I915_BIT_6_SWIZZLE_UNKNOWN;
	int		need_disable;

	if (!IS_I9XX(dev_priv)) {
		/* As far as we know, the 865 doesn't have these bit 6
		 * swizzling issues.
		 */
		swizzle_x = I915_BIT_6_SWIZZLE_NONE;
		swizzle_y = I915_BIT_6_SWIZZLE_NONE;
	} else if (IS_MOBILE(dev_priv)) {
		uint32_t dcc;

		/* try to enable MCHBAR, a lot of biosen disable it */
		need_disable = inteldrm_setup_mchbar(dev_priv, bpa);

		/* On 915-945 and GM965, channel interleave by the CPU is
		 * determined by DCC.  The CPU will alternate based on bit 6
		 * in interleaved mode, and the GPU will then also alternate
		 * on bit 6, 9, and 10 for X, but the CPU may also optionally
		 * alternate based on bit 17 (XOR not disabled and XOR
		 * bit == 17).
		 */
		dcc = I915_READ(DCC);
		switch (dcc & DCC_ADDRESSING_MODE_MASK) {
		case DCC_ADDRESSING_MODE_SINGLE_CHANNEL:
		case DCC_ADDRESSING_MODE_DUAL_CHANNEL_ASYMMETRIC:
			swizzle_x = I915_BIT_6_SWIZZLE_NONE;
			swizzle_y = I915_BIT_6_SWIZZLE_NONE;
			break;
		case DCC_ADDRESSING_MODE_DUAL_CHANNEL_INTERLEAVED:
			if (dcc & DCC_CHANNEL_XOR_DISABLE) {
				/* This is the base swizzling by the GPU for
				 * tiled buffers.
				 */
				swizzle_x = I915_BIT_6_SWIZZLE_9_10;
				swizzle_y = I915_BIT_6_SWIZZLE_9;
			} else if ((dcc & DCC_CHANNEL_XOR_BIT_17) == 0) {
				/* Bit 11 swizzling by the CPU in addition. */
				swizzle_x = I915_BIT_6_SWIZZLE_9_10_11;
				swizzle_y = I915_BIT_6_SWIZZLE_9_11;
			} else {
				/* Bit 17 swizzling by the CPU in addition. */
				swizzle_x = I915_BIT_6_SWIZZLE_9_10_17;
				swizzle_y = I915_BIT_6_SWIZZLE_9_17;
			}
			break;
		}
		if (dcc == 0xffffffff) {
			DRM_ERROR("Couldn't read from MCHBAR.  "
				  "Disabling tiling.\n");
			swizzle_x = I915_BIT_6_SWIZZLE_UNKNOWN;
			swizzle_y = I915_BIT_6_SWIZZLE_UNKNOWN;
		}

		inteldrm_teardown_mchbar(dev_priv, bpa, need_disable);	
	} else {
		/* The 965, G33, and newer, have a very flexible memory
		 * configuration. It will enable dual-channel mode
		 * (interleaving) on as much memory as it can, and the GPU
		 * will additionally sometimes enable different bit 6
		 * swizzling for tiled objects from the CPU.
		 *
		 * Here's what I found on G965:
		 *
		 *    slot fill			memory size	swizzling
		 * 0A   0B	1A	1B	1-ch	2-ch
		 * 512	0	0	0	512	0	O
		 * 512	0	512	0	16	1008	X
		 * 512	0	0	512	16	1008	X
		 * 0	512	0	512	16	1008	X
		 * 1024	1024	1024	0	2048	1024	O
		 *
		 * We could probably detect this based on either the DRB
		 * matching, which was the case for the swizzling required in
		 * the table above, or from the 1-ch value being less than
		 * the minimum size of a rank.
		 */
		if (I915_READ16(C0DRB3) != I915_READ16(C1DRB3)) {
			swizzle_x = I915_BIT_6_SWIZZLE_NONE;
			swizzle_y = I915_BIT_6_SWIZZLE_NONE;
		} else {
			swizzle_x = I915_BIT_6_SWIZZLE_9_10;
			swizzle_y = I915_BIT_6_SWIZZLE_9;
		}
	}

	dev_priv->mm.bit_6_swizzle_x = swizzle_x;
	dev_priv->mm.bit_6_swizzle_y = swizzle_y;
}

int
inteldrm_swizzle_page(struct vm_page *pg)
{
	vaddr_t	 va;
	int	 i;
	u_int8_t temp[64], *vaddr;

#if defined (__HAVE_PMAP_DIRECT)
	va = pmap_map_direct(pg);
#else
	va = uvm_km_valloc(kernel_map, PAGE_SIZE);
	if (va == 0)
		return (ENOMEM);
	pmap_kenter_pa(va, VM_PAGE_TO_PHYS(pg), UVM_PROT_RW);
	pmap_update(pmap_kernel());
#endif
	vaddr = (u_int8_t *)va;

	for (i = 0; i < PAGE_SIZE; i += 128) {
		memcpy(temp, &vaddr[i], 64);
		memcpy(&vaddr[i], &vaddr[i + 64], 64);
		memcpy(&vaddr[i + 64], temp, 64);
	}

#if defined (__HAVE_PMAP_DIRECT)
	pmap_unmap_direct(va);
#else
	pmap_kremove(va, PAGE_SIZE);
	pmap_update(pmap_kernel());
	uvm_km_free(kernel_map, va, PAGE_SIZE);
#endif
	return (0);
}

void
i915_gem_bit_17_swizzle(struct drm_obj *obj)
{
	struct drm_device	*dev = obj->dev;
	struct drm_i915_private	*dev_priv = dev->dev_private;
	struct inteldrm_obj	*obj_priv = (struct inteldrm_obj *)obj;
	struct vm_page		*pg;
	bus_dma_segment_t	*segp;
	int			 page_count = obj->size >> PAGE_SHIFT;
	int                      i, n, ret;

	if (dev_priv->mm.bit_6_swizzle_x != I915_BIT_6_SWIZZLE_9_10_17 ||
	    obj_priv->bit_17 == NULL)
		return;

	segp = &obj_priv->dma_segs[0];
	n = 0;
	for (i = 0; i < page_count; i++) {
		/* compare bit 17 with previous one (in case we swapped).
		 * if they don't match we'll have to swizzle the page
		 */
		if ((((segp->ds_addr + n) >> 17) & 0x1) !=
		    test_bit(i, obj_priv->bit_17)) {
			/* XXX move this to somewhere where we already have pg */
			pg = PHYS_TO_VM_PAGE(segp->ds_addr + n);
			KASSERT(pg != NULL);
			ret = inteldrm_swizzle_page(pg);
			if (ret)
				return;
			atomic_clearbits_int(&pg->pg_flags, PG_CLEAN);
		}

		n += PAGE_SIZE;
		if (n >= segp->ds_len) {
			n = 0;
			segp++;
		}
	}

}

void 
i915_gem_save_bit_17_swizzle(struct drm_obj *obj)
{
	struct drm_device	*dev = obj->dev;
	struct drm_i915_private	*dev_priv = dev->dev_private;
	struct inteldrm_obj	*obj_priv = (struct inteldrm_obj *)obj;
	bus_dma_segment_t	*segp;
	int			 page_count = obj->size >> PAGE_SHIFT, i, n;

	if (dev_priv->mm.bit_6_swizzle_x != I915_BIT_6_SWIZZLE_9_10_17)
		return;

	if (obj_priv->bit_17 == NULL) {
		/* round up number of pages to a multiple of 32 so we know what
		 * size to make the bitmask. XXX this is wasteful with malloc
		 * and a better way should be done
		 */
		size_t nb17 = ((page_count + 31) & ~31)/32;
		obj_priv->bit_17 = drm_alloc(nb17 * sizeof(u_int32_t));
		if (obj_priv-> bit_17 == NULL) {
			return;
		}

	}

	segp = &obj_priv->dma_segs[0];
	n = 0;
	for (i = 0; i < page_count; i++) {
		if ((segp->ds_addr + n) & (1 << 17))
			set_bit(i, obj_priv->bit_17);
		else
			clear_bit(i, obj_priv->bit_17);

		n += PAGE_SIZE;
		if (n >= segp->ds_len) {
			n = 0;
			segp++;
		}
	}
}

bus_size_t
i915_get_fence_size(struct drm_i915_private *dev_priv, bus_size_t size)
{
	bus_size_t	i, start;

	if (IS_I965G(dev_priv)) {
		/* 965 can have fences anywhere, so align to gpu-page size */
		return ((size + (4096 - 1)) & ~(4096 - 1));
	} else {
		/*
		 * Align the size to a power of two greater than the smallest
		 * fence size.
		 */
		if (IS_I9XX(dev_priv))
			start = 1024 * 1024;
		else
			start = 512 * 1024;

		for (i = start; i < size; i <<= 1)
			;

		return (i);
	}
}

int
i915_tiling_ok(struct drm_device *dev, int stride, int size, int tiling_mode)
{
	struct drm_i915_private	*dev_priv = dev->dev_private;
	int			 tile_width;

	/* Linear is always ok */
	if (tiling_mode == I915_TILING_NONE)
		return (1);

	if (!IS_I9XX(dev_priv) || (tiling_mode == I915_TILING_Y &&
	    HAS_128_BYTE_Y_TILING(dev_priv)))
		tile_width = 128;
	else
		tile_width = 512;

	/* Check stride and size constraints */
	if (IS_I965G(dev_priv)) {
		/* fence reg has end address, so size is ok */
		if (stride / 128 > I965_FENCE_MAX_PITCH_VAL)
			return (0);
	} else if (IS_GEN3(dev_priv) || IS_GEN2(dev_priv)) {
		if (stride > 8192)
			return (0);
		if (IS_GEN3(dev_priv)) {
			if (size > I830_FENCE_MAX_SIZE_VAL << 20)
				return (0);
		} else if (size > I830_FENCE_MAX_SIZE_VAL << 19)
			return (0);
	}

	/* 965+ just needs multiples of the tile width */
	if (IS_I965G(dev_priv))
		return ((stride & (tile_width - 1)) == 0);

	/* Pre-965 needs power-of-two */
	if (stride < tile_width || stride & (stride - 1) ||
	    i915_get_fence_size(dev_priv, size) != size)
		return (0);
	return (1);
}

int
i915_gem_object_fence_offset_ok(struct drm_obj *obj, int tiling_mode)
{
	struct drm_device	*dev = obj->dev;
	struct drm_i915_private	*dev_priv = dev->dev_private;
	struct inteldrm_obj	*obj_priv = (struct inteldrm_obj *)obj;

	if (obj_priv->dmamap == NULL || tiling_mode == I915_TILING_NONE)
		return (1);

	if (!IS_I965G(dev_priv)) {
		if (obj_priv->gtt_offset & (obj->size -1))
			return (0);
		if (IS_I9XX(dev_priv)) {
			if (obj_priv->gtt_offset & ~I915_FENCE_START_MASK)
				return (0);
		} else {
			if (obj_priv->gtt_offset & ~I830_FENCE_START_MASK)
				return (0);
		}
	}
	return (1);
}
/**
 * Sets the tiling mode of an object, returning the required swizzling of
 * bit 6 of addresses in the object.
 */
int
i915_gem_set_tiling(struct drm_device *dev, void *data,
		   struct drm_file *file_priv)
{
	struct drm_i915_gem_set_tiling	*args = data;
	drm_i915_private_t		*dev_priv = dev->dev_private;
	struct drm_obj			*obj;
	struct inteldrm_obj		*obj_priv;
	int				 ret = 0;

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);
	if (obj == NULL)
		return (EBADF);
	obj_priv = (struct inteldrm_obj *)obj;
	drm_hold_object(obj);

	if (obj_priv->pin_count != 0) {
		ret = EBUSY;
		goto out;
	}
	if (i915_tiling_ok(dev, args->stride, obj->size,
	    args->tiling_mode) == 0) {
		ret = EINVAL;
		goto out;
	}

	if (args->tiling_mode == I915_TILING_NONE) {
		args->swizzle_mode = I915_BIT_6_SWIZZLE_NONE;
		args->stride = 0;
	} else {
		if (args->tiling_mode == I915_TILING_X)
			args->swizzle_mode = dev_priv->mm.bit_6_swizzle_x;
		else
			args->swizzle_mode = dev_priv->mm.bit_6_swizzle_y;
		/* If we can't handle the swizzling, make it untiled. */
		if (args->swizzle_mode == I915_BIT_6_SWIZZLE_UNKNOWN) {
			args->tiling_mode = I915_TILING_NONE;
			args->swizzle_mode = I915_BIT_6_SWIZZLE_NONE;
			args->stride = 0;
		}
	}

	if (args->tiling_mode != obj_priv->tiling_mode ||
	    args->stride != obj_priv->stride) {
		/*
		 * We need to rebind the object if its current allocation no
		 * longer meets the alignment restrictions for its new tiling
		 * mode. Otherwise we can leave it alone, but must clear any
		 * fence register.
		 */
		/* fence may no longer be correct, wipe it */
		inteldrm_wipe_mappings(obj);
		if (obj_priv->fence_reg != I915_FENCE_REG_NONE)
			atomic_setbits_int(&obj->do_flags,
			    I915_FENCE_INVALID);
		obj_priv->tiling_mode = args->tiling_mode;
		obj_priv->stride = args->stride;
	}
	 
out:
	drm_unhold_and_unref(obj);

	return (ret);
}

/**
 * Returns the current tiling mode and required bit 6 swizzling for the object.
 */
int
i915_gem_get_tiling(struct drm_device *dev, void *data,
		   struct drm_file *file_priv)
{
	struct drm_i915_gem_get_tiling	*args = data;
	drm_i915_private_t		*dev_priv = dev->dev_private;
	struct drm_obj			*obj;
	struct inteldrm_obj		*obj_priv;

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);
	if (obj == NULL)
		return (EBADF);
	drm_hold_object(obj);
	obj_priv = (struct inteldrm_obj *)obj;

	args->tiling_mode = obj_priv->tiling_mode;
	switch (obj_priv->tiling_mode) {
	case I915_TILING_X:
		args->swizzle_mode = dev_priv->mm.bit_6_swizzle_x;
		break;
	case I915_TILING_Y:
		args->swizzle_mode = dev_priv->mm.bit_6_swizzle_y;
		break;
	case I915_TILING_NONE:
		args->swizzle_mode = I915_BIT_6_SWIZZLE_NONE;
		break;
	default:
		DRM_ERROR("unknown tiling mode\n");
	}

	drm_unhold_and_unref(obj);

	return 0;
}


/**
 * inteldrm_pipe_enabled - check if a pipe is enabled
 * @dev: DRM device
 * @pipe: pipe to check
 *
 * Reading certain registers when the pipe is disabled can hang the chip.
 * Use this routine to make sure the PLL is running and the pipe is active
 * before reading such registers if unsure.
 */
int
inteldrm_pipe_enabled(struct drm_i915_private *dev_priv, int pipe)
{
	bus_size_t	pipeconf = pipe ? PIPEBCONF : PIPEACONF;

	return ((I915_READ(pipeconf) & PIPEACONF_ENABLE) == PIPEACONF_ENABLE);
}

/*
 * Register save/restore for various instances
 */
void
i915_save_palette(struct drm_i915_private *dev_priv, enum pipe pipe)
{
	u_int32_t	*array;
	bus_size_t	 reg = (pipe == PIPE_A ? PALETTE_A : PALETTE_B);
	int		 i;

	if (!inteldrm_pipe_enabled(dev_priv, pipe))
		return;

	if (pipe == PIPE_A)
		array = dev_priv->save_palette_a;
	else
		array = dev_priv->save_palette_b;

	for (i = 0; i < 256; i++)
		array[i] = I915_READ(reg + (i << 2));
}

void
i915_restore_palette(struct drm_i915_private *dev_priv, enum pipe pipe)
{
	u_int32_t	*array;
	bus_size_t	 reg = (pipe == PIPE_A ? PALETTE_A : PALETTE_B);
	int		 i;

	if (!inteldrm_pipe_enabled(dev_priv, pipe))
		return;

	if (pipe == PIPE_A)
		array = dev_priv->save_palette_a;
	else
		array = dev_priv->save_palette_b;

	for(i = 0; i < 256; i++)
		I915_WRITE(reg + (i << 2), array[i]);
}

u_int8_t
i915_read_ar(struct drm_i915_private *dev_priv, u_int16_t st01,
    u_int8_t reg, u_int16_t palette_enable)
{
	I915_READ8(st01);
	I915_WRITE8(VGA_AR_INDEX, palette_enable | reg);
	return I915_READ8(VGA_AR_DATA_READ);
}

void
i915_write_ar(struct drm_i915_private *dev_priv, u_int16_t st01, u_int8_t reg,
    u_int8_t val, u_int16_t palette_enable)
{
	I915_READ8(st01);
	I915_WRITE8(VGA_AR_INDEX, palette_enable | reg);
	I915_WRITE8(VGA_AR_DATA_WRITE, val);
}

u_int8_t
i915_read_indexed(struct drm_i915_private *dev_priv, u_int16_t index_port,
    u_int16_t data_port, u_int8_t reg)
{
	I915_WRITE8(index_port, reg);
	return I915_READ8(data_port);
}

void
i915_write_indexed(struct drm_i915_private *dev_priv, u_int16_t index_port,
    u_int16_t data_port, u_int8_t reg, u_int8_t val)
{
	I915_WRITE8(index_port, reg);
	I915_WRITE8(data_port, val);
}

void
i915_save_vga(struct drm_i915_private *dev_priv)
{
	int i;
	u16 cr_index, cr_data, st01;

	/* VGA color palette registers */
	dev_priv->saveDACMASK = I915_READ8(VGA_DACMASK);

	/* MSR bits */
	dev_priv->saveMSR = I915_READ8(VGA_MSR_READ);
	if (dev_priv->saveMSR & VGA_MSR_CGA_MODE) {
		cr_index = VGA_CR_INDEX_CGA;
		cr_data = VGA_CR_DATA_CGA;
		st01 = VGA_ST01_CGA;
	} else {
		cr_index = VGA_CR_INDEX_MDA;
		cr_data = VGA_CR_DATA_MDA;
		st01 = VGA_ST01_MDA;
	}

	/* CRT controller regs */
	i915_write_indexed(dev_priv, cr_index, cr_data, 0x11,
	    i915_read_indexed(dev_priv, cr_index, cr_data, 0x11) & (~0x80));
	for (i = 0; i <= 0x24; i++)
		dev_priv->saveCR[i] = i915_read_indexed(dev_priv,
		    cr_index, cr_data, i);
	/* Make sure we don't turn off CR group 0 writes */
	dev_priv->saveCR[0x11] &= ~0x80;

	/* Attribute controller registers */
	I915_READ8(st01);
	dev_priv->saveAR_INDEX = I915_READ8(VGA_AR_INDEX);
	for (i = 0; i <= 0x14; i++)
		dev_priv->saveAR[i] = i915_read_ar(dev_priv, st01, i, 0);
	I915_READ8(st01);
	I915_WRITE8(VGA_AR_INDEX, dev_priv->saveAR_INDEX);
	I915_READ8(st01);

	/* Graphics controller registers */
	for (i = 0; i < 9; i++)
		dev_priv->saveGR[i] = i915_read_indexed(dev_priv,
		    VGA_GR_INDEX, VGA_GR_DATA, i);

	dev_priv->saveGR[0x10] = i915_read_indexed(dev_priv,
	    VGA_GR_INDEX, VGA_GR_DATA, 0x10);
	dev_priv->saveGR[0x11] = i915_read_indexed(dev_priv,
	    VGA_GR_INDEX, VGA_GR_DATA, 0x11);
	dev_priv->saveGR[0x18] = i915_read_indexed(dev_priv,
	    VGA_GR_INDEX, VGA_GR_DATA, 0x18);

	/* Sequencer registers */
	for (i = 0; i < 8; i++)
		dev_priv->saveSR[i] = i915_read_indexed(dev_priv,
		    VGA_SR_INDEX, VGA_SR_DATA, i);
}

void
i915_restore_vga(struct drm_i915_private *dev_priv)
{
	u_int16_t	cr_index, cr_data, st01;
	int		i;

	/* MSR bits */
	I915_WRITE8(VGA_MSR_WRITE, dev_priv->saveMSR);
	if (dev_priv->saveMSR & VGA_MSR_CGA_MODE) {
		cr_index = VGA_CR_INDEX_CGA;
		cr_data = VGA_CR_DATA_CGA;
		st01 = VGA_ST01_CGA;
	} else {
		cr_index = VGA_CR_INDEX_MDA;
		cr_data = VGA_CR_DATA_MDA;
		st01 = VGA_ST01_MDA;
	}

	/* Sequencer registers, don't write SR07 */
	for (i = 0; i < 7; i++)
		i915_write_indexed(dev_priv, VGA_SR_INDEX, VGA_SR_DATA, i,
		    dev_priv->saveSR[i]);

	/* CRT controller regs */
	/* Enable CR group 0 writes */
	i915_write_indexed(dev_priv, cr_index, cr_data, 0x11,
	    dev_priv->saveCR[0x11]);
	for (i = 0; i <= 0x24; i++)
		i915_write_indexed(dev_priv, cr_index, cr_data,
		    i, dev_priv->saveCR[i]);

	/* Graphics controller regs */
	for (i = 0; i < 9; i++)
		i915_write_indexed(dev_priv, VGA_GR_INDEX, VGA_GR_DATA, i,
				   dev_priv->saveGR[i]);

	i915_write_indexed(dev_priv, VGA_GR_INDEX, VGA_GR_DATA, 0x10,
			   dev_priv->saveGR[0x10]);
	i915_write_indexed(dev_priv, VGA_GR_INDEX, VGA_GR_DATA, 0x11,
			   dev_priv->saveGR[0x11]);
	i915_write_indexed(dev_priv, VGA_GR_INDEX, VGA_GR_DATA, 0x18,
			   dev_priv->saveGR[0x18]);

	/* Attribute controller registers */
	I915_READ8(st01); /* switch back to index mode */
	for (i = 0; i <= 0x14; i++)
		i915_write_ar(dev_priv, st01, i, dev_priv->saveAR[i], 0);
	I915_READ8(st01); /* switch back to index mode */
	I915_WRITE8(VGA_AR_INDEX, dev_priv->saveAR_INDEX | 0x20);
	I915_READ8(st01);

	/* VGA color palette registers */
	I915_WRITE8(VGA_DACMASK, dev_priv->saveDACMASK);
}

void
i915_save_modeset_reg(struct drm_i915_private *dev_priv)
{
	/* Pipe & plane A info */
	dev_priv->savePIPEACONF = I915_READ(PIPEACONF);
	dev_priv->savePIPEASRC = I915_READ(PIPEASRC);
	dev_priv->saveFPA0 = I915_READ(FPA0);
	dev_priv->saveFPA1 = I915_READ(FPA1);
	dev_priv->saveDPLL_A = I915_READ(DPLL_A);
	if (IS_I965G(dev_priv))
		dev_priv->saveDPLL_A_MD = I915_READ(DPLL_A_MD);
	dev_priv->saveHTOTAL_A = I915_READ(HTOTAL_A);
	dev_priv->saveHBLANK_A = I915_READ(HBLANK_A);
	dev_priv->saveHSYNC_A = I915_READ(HSYNC_A);
	dev_priv->saveVTOTAL_A = I915_READ(VTOTAL_A);
	dev_priv->saveVBLANK_A = I915_READ(VBLANK_A);
	dev_priv->saveVSYNC_A = I915_READ(VSYNC_A);
	dev_priv->saveBCLRPAT_A = I915_READ(BCLRPAT_A);

	dev_priv->saveDSPACNTR = I915_READ(DSPACNTR);
	dev_priv->saveDSPASTRIDE = I915_READ(DSPASTRIDE);
	dev_priv->saveDSPASIZE = I915_READ(DSPASIZE);
	dev_priv->saveDSPAPOS = I915_READ(DSPAPOS);
	dev_priv->saveDSPAADDR = I915_READ(DSPAADDR);
	if (IS_I965G(dev_priv)) {
		dev_priv->saveDSPASURF = I915_READ(DSPASURF);
		dev_priv->saveDSPATILEOFF = I915_READ(DSPATILEOFF);
	}
	i915_save_palette(dev_priv, PIPE_A);
	dev_priv->savePIPEASTAT = I915_READ(PIPEASTAT);

	/* Pipe & plane B info */
	dev_priv->savePIPEBCONF = I915_READ(PIPEBCONF);
	dev_priv->savePIPEBSRC = I915_READ(PIPEBSRC);
	dev_priv->saveFPB0 = I915_READ(FPB0);
	dev_priv->saveFPB1 = I915_READ(FPB1);
	dev_priv->saveDPLL_B = I915_READ(DPLL_B);
	if (IS_I965G(dev_priv))
		dev_priv->saveDPLL_B_MD = I915_READ(DPLL_B_MD);
	dev_priv->saveHTOTAL_B = I915_READ(HTOTAL_B);
	dev_priv->saveHBLANK_B = I915_READ(HBLANK_B);
	dev_priv->saveHSYNC_B = I915_READ(HSYNC_B);
	dev_priv->saveVTOTAL_B = I915_READ(VTOTAL_B);
	dev_priv->saveVBLANK_B = I915_READ(VBLANK_B);
	dev_priv->saveVSYNC_B = I915_READ(VSYNC_B);
	dev_priv->saveBCLRPAT_A = I915_READ(BCLRPAT_A);

	dev_priv->saveDSPBCNTR = I915_READ(DSPBCNTR);
	dev_priv->saveDSPBSTRIDE = I915_READ(DSPBSTRIDE);
	dev_priv->saveDSPBSIZE = I915_READ(DSPBSIZE);
	dev_priv->saveDSPBPOS = I915_READ(DSPBPOS);
	dev_priv->saveDSPBADDR = I915_READ(DSPBADDR);
	if (IS_I965GM(dev_priv) || IS_GM45(dev_priv)) {
		dev_priv->saveDSPBSURF = I915_READ(DSPBSURF);
		dev_priv->saveDSPBTILEOFF = I915_READ(DSPBTILEOFF);
	}
	i915_save_palette(dev_priv, PIPE_B);
	dev_priv->savePIPEBSTAT = I915_READ(PIPEBSTAT);
}

void
i915_restore_modeset_reg(struct drm_i915_private *dev_priv)
{
	/* Pipe & plane A info */
	/* Prime the clock */
	if (dev_priv->saveDPLL_A & DPLL_VCO_ENABLE) {
		I915_WRITE(DPLL_A, dev_priv->saveDPLL_A &
			   ~DPLL_VCO_ENABLE);
		DRM_UDELAY(150);
	}
	I915_WRITE(FPA0, dev_priv->saveFPA0);
	I915_WRITE(FPA1, dev_priv->saveFPA1);
	/* Actually enable it */
	I915_WRITE(DPLL_A, dev_priv->saveDPLL_A);
	DRM_UDELAY(150);
	if (IS_I965G(dev_priv))
		I915_WRITE(DPLL_A_MD, dev_priv->saveDPLL_A_MD);
	DRM_UDELAY(150);

	/* Restore mode */
	I915_WRITE(HTOTAL_A, dev_priv->saveHTOTAL_A);
	I915_WRITE(HBLANK_A, dev_priv->saveHBLANK_A);
	I915_WRITE(HSYNC_A, dev_priv->saveHSYNC_A);
	I915_WRITE(VTOTAL_A, dev_priv->saveVTOTAL_A);
	I915_WRITE(VBLANK_A, dev_priv->saveVBLANK_A);
	I915_WRITE(VSYNC_A, dev_priv->saveVSYNC_A);
	I915_WRITE(BCLRPAT_A, dev_priv->saveBCLRPAT_A);

	/* Restore plane info */
	I915_WRITE(DSPASIZE, dev_priv->saveDSPASIZE);
	I915_WRITE(DSPAPOS, dev_priv->saveDSPAPOS);
	I915_WRITE(PIPEASRC, dev_priv->savePIPEASRC);
	I915_WRITE(DSPAADDR, dev_priv->saveDSPAADDR);
	I915_WRITE(DSPASTRIDE, dev_priv->saveDSPASTRIDE);
	if (IS_I965G(dev_priv)) {
		I915_WRITE(DSPASURF, dev_priv->saveDSPASURF);
		I915_WRITE(DSPATILEOFF, dev_priv->saveDSPATILEOFF);
	}

	I915_WRITE(PIPEACONF, dev_priv->savePIPEACONF);

	i915_restore_palette(dev_priv, PIPE_A);
	/* Enable the plane */
	I915_WRITE(DSPACNTR, dev_priv->saveDSPACNTR);
	I915_WRITE(DSPAADDR, I915_READ(DSPAADDR));

	/* Pipe & plane B info */
	if (dev_priv->saveDPLL_B & DPLL_VCO_ENABLE) {
		I915_WRITE(DPLL_B, dev_priv->saveDPLL_B &
			   ~DPLL_VCO_ENABLE);
		DRM_UDELAY(150);
	}
	I915_WRITE(FPB0, dev_priv->saveFPB0);
	I915_WRITE(FPB1, dev_priv->saveFPB1);
	/* Actually enable it */
	I915_WRITE(DPLL_B, dev_priv->saveDPLL_B);
	DRM_UDELAY(150);
	if (IS_I965G(dev_priv))
		I915_WRITE(DPLL_B_MD, dev_priv->saveDPLL_B_MD);
	DRM_UDELAY(150);

	/* Restore mode */
	I915_WRITE(HTOTAL_B, dev_priv->saveHTOTAL_B);
	I915_WRITE(HBLANK_B, dev_priv->saveHBLANK_B);
	I915_WRITE(HSYNC_B, dev_priv->saveHSYNC_B);
	I915_WRITE(VTOTAL_B, dev_priv->saveVTOTAL_B);
	I915_WRITE(VBLANK_B, dev_priv->saveVBLANK_B);
	I915_WRITE(VSYNC_B, dev_priv->saveVSYNC_B);
	I915_WRITE(BCLRPAT_B, dev_priv->saveBCLRPAT_B);

	/* Restore plane info */
	I915_WRITE(DSPBSIZE, dev_priv->saveDSPBSIZE);
	I915_WRITE(DSPBPOS, dev_priv->saveDSPBPOS);
	I915_WRITE(PIPEBSRC, dev_priv->savePIPEBSRC);
	I915_WRITE(DSPBADDR, dev_priv->saveDSPBADDR);
	I915_WRITE(DSPBSTRIDE, dev_priv->saveDSPBSTRIDE);
	if (IS_I965G(dev_priv)) {
		I915_WRITE(DSPBSURF, dev_priv->saveDSPBSURF);
		I915_WRITE(DSPBTILEOFF, dev_priv->saveDSPBTILEOFF);
	}

	I915_WRITE(PIPEBCONF, dev_priv->savePIPEBCONF);

	i915_restore_palette(dev_priv, PIPE_B);
	/* Enable the plane */
	I915_WRITE(DSPBCNTR, dev_priv->saveDSPBCNTR);
	I915_WRITE(DSPBADDR, I915_READ(DSPBADDR));
}

int
inteldrm_save_display(struct drm_i915_private *dev_priv)
{
	/* Display arbitration control */
	dev_priv->saveDSPARB = I915_READ(DSPARB);

	/* This is only meaningful in non-KMS mode */
	/* Don't save them in KMS mode */
	i915_save_modeset_reg(dev_priv);
	/* Cursor state */
	dev_priv->saveCURACNTR = I915_READ(CURACNTR);
	dev_priv->saveCURAPOS = I915_READ(CURAPOS);
	dev_priv->saveCURABASE = I915_READ(CURABASE);
	dev_priv->saveCURBCNTR = I915_READ(CURBCNTR);
	dev_priv->saveCURBPOS = I915_READ(CURBPOS);
	dev_priv->saveCURBBASE = I915_READ(CURBBASE);
	if (!IS_I9XX(dev_priv))
		dev_priv->saveCURSIZE = I915_READ(CURSIZE);

	/* CRT state */
	dev_priv->saveADPA = I915_READ(ADPA);

	/* LVDS state */
	dev_priv->savePP_CONTROL = I915_READ(PP_CONTROL);
	dev_priv->savePFIT_PGM_RATIOS = I915_READ(PFIT_PGM_RATIOS);
	dev_priv->saveBLC_PWM_CTL = I915_READ(BLC_PWM_CTL);
	if (IS_I965G(dev_priv))
		dev_priv->saveBLC_PWM_CTL2 = I915_READ(BLC_PWM_CTL2);
	if (IS_MOBILE(dev_priv) && !IS_I830(dev_priv))
		dev_priv->saveLVDS = I915_READ(LVDS);
	if (!IS_I830(dev_priv) && !IS_845G(dev_priv))
		dev_priv->savePFIT_CONTROL = I915_READ(PFIT_CONTROL);
	dev_priv->savePP_ON_DELAYS = I915_READ(PP_ON_DELAYS);
	dev_priv->savePP_OFF_DELAYS = I915_READ(PP_OFF_DELAYS);
	dev_priv->savePP_DIVISOR = I915_READ(PP_DIVISOR);

	/* FIXME: save TV & SDVO state */

	/* FBC state */
	dev_priv->saveFBC_CFB_BASE = I915_READ(FBC_CFB_BASE);
	dev_priv->saveFBC_LL_BASE = I915_READ(FBC_LL_BASE);
	dev_priv->saveFBC_CONTROL2 = I915_READ(FBC_CONTROL2);
	dev_priv->saveFBC_CONTROL = I915_READ(FBC_CONTROL);

	/* VGA state */
	dev_priv->saveVGA0 = I915_READ(VGA0);
	dev_priv->saveVGA1 = I915_READ(VGA1);
	dev_priv->saveVGA_PD = I915_READ(VGA_PD);
	dev_priv->saveVGACNTRL = I915_READ(VGACNTRL);

	i915_save_vga(dev_priv);

	return 0;
}

int
inteldrm_restore_display(struct drm_i915_private *dev_priv)
{
	/* Display arbitration */
	I915_WRITE(DSPARB, dev_priv->saveDSPARB);

	/* This is only meaningful in non-KMS mode */
	/* Don't restore them in KMS mode */
	i915_restore_modeset_reg(dev_priv);
	/* Cursor state */
	I915_WRITE(CURAPOS, dev_priv->saveCURAPOS);
	I915_WRITE(CURACNTR, dev_priv->saveCURACNTR);
	I915_WRITE(CURABASE, dev_priv->saveCURABASE);
	I915_WRITE(CURBPOS, dev_priv->saveCURBPOS);
	I915_WRITE(CURBCNTR, dev_priv->saveCURBCNTR);
	I915_WRITE(CURBBASE, dev_priv->saveCURBBASE);
	if (!IS_I9XX(dev_priv))
		I915_WRITE(CURSIZE, dev_priv->saveCURSIZE);

	/* CRT state */
	I915_WRITE(ADPA, dev_priv->saveADPA);

	/* LVDS state */
	if (IS_I965G(dev_priv))
		I915_WRITE(BLC_PWM_CTL2, dev_priv->saveBLC_PWM_CTL2);
	if (IS_MOBILE(dev_priv) && !IS_I830(dev_priv))
		I915_WRITE(LVDS, dev_priv->saveLVDS);
	if (!IS_I830(dev_priv) && !IS_845G(dev_priv))
		I915_WRITE(PFIT_CONTROL, dev_priv->savePFIT_CONTROL);

	I915_WRITE(PFIT_PGM_RATIOS, dev_priv->savePFIT_PGM_RATIOS);
	I915_WRITE(BLC_PWM_CTL, dev_priv->saveBLC_PWM_CTL);
	I915_WRITE(PP_ON_DELAYS, dev_priv->savePP_ON_DELAYS);
	I915_WRITE(PP_OFF_DELAYS, dev_priv->savePP_OFF_DELAYS);
	I915_WRITE(PP_DIVISOR, dev_priv->savePP_DIVISOR);
	I915_WRITE(PP_CONTROL, dev_priv->savePP_CONTROL);

	/* FIXME: restore TV & SDVO state */

	/* FBC info */
	I915_WRITE(FBC_CFB_BASE, dev_priv->saveFBC_CFB_BASE);
	I915_WRITE(FBC_LL_BASE, dev_priv->saveFBC_LL_BASE);
	I915_WRITE(FBC_CONTROL2, dev_priv->saveFBC_CONTROL2);
	I915_WRITE(FBC_CONTROL, dev_priv->saveFBC_CONTROL);

	/* VGA state */
	I915_WRITE(VGACNTRL, dev_priv->saveVGACNTRL);
	I915_WRITE(VGA0, dev_priv->saveVGA0);
	I915_WRITE(VGA1, dev_priv->saveVGA1);
	I915_WRITE(VGA_PD, dev_priv->saveVGA_PD);
	DRM_UDELAY(150);

	i915_restore_vga(dev_priv);

	return 0;
}

int
inteldrm_save_state(struct drm_i915_private *dev_priv)
{
	int i;

	dev_priv->saveLBB = pci_conf_read(dev_priv->pc, dev_priv->tag, LBB);

	/* Render Standby */
	if (IS_I965G(dev_priv) && IS_MOBILE(dev_priv))
		dev_priv->saveRENDERSTANDBY = I915_READ(MCHBAR_RENDER_STANDBY);

	/* Hardware status page */
	dev_priv->saveHWS = I915_READ(HWS_PGA);

	/* Display arbitration control */
	dev_priv->saveDSPARB = I915_READ(DSPARB);

	/* This is only meaningful in non-KMS mode */
	/* Don't save them in KMS mode */
	i915_save_modeset_reg(dev_priv);
	/* Cursor state */
	dev_priv->saveCURACNTR = I915_READ(CURACNTR);
	dev_priv->saveCURAPOS = I915_READ(CURAPOS);
	dev_priv->saveCURABASE = I915_READ(CURABASE);
	dev_priv->saveCURBCNTR = I915_READ(CURBCNTR);
	dev_priv->saveCURBPOS = I915_READ(CURBPOS);
	dev_priv->saveCURBBASE = I915_READ(CURBBASE);
	if (!IS_I9XX(dev_priv))
		dev_priv->saveCURSIZE = I915_READ(CURSIZE);

	/* CRT state */
	dev_priv->saveADPA = I915_READ(ADPA);

	/* LVDS state */
	dev_priv->savePP_CONTROL = I915_READ(PP_CONTROL);
	dev_priv->savePFIT_PGM_RATIOS = I915_READ(PFIT_PGM_RATIOS);
	dev_priv->saveBLC_PWM_CTL = I915_READ(BLC_PWM_CTL);
	if (IS_I965G(dev_priv))
		dev_priv->saveBLC_PWM_CTL2 = I915_READ(BLC_PWM_CTL2);
	if (IS_MOBILE(dev_priv) && !IS_I830(dev_priv))
		dev_priv->saveLVDS = I915_READ(LVDS);
	if (!IS_I830(dev_priv) && !IS_845G(dev_priv))
		dev_priv->savePFIT_CONTROL = I915_READ(PFIT_CONTROL);
	dev_priv->savePP_ON_DELAYS = I915_READ(PP_ON_DELAYS);
	dev_priv->savePP_OFF_DELAYS = I915_READ(PP_OFF_DELAYS);
	dev_priv->savePP_DIVISOR = I915_READ(PP_DIVISOR);

	/* XXX: displayport */
	/* FIXME: save TV & SDVO state */

	/* FBC state */
	dev_priv->saveFBC_CFB_BASE = I915_READ(FBC_CFB_BASE);
	dev_priv->saveFBC_LL_BASE = I915_READ(FBC_LL_BASE);
	dev_priv->saveFBC_CONTROL2 = I915_READ(FBC_CONTROL2);
	dev_priv->saveFBC_CONTROL = I915_READ(FBC_CONTROL);

	/* Interrupt state */
	dev_priv->saveIIR = I915_READ(IIR);
	dev_priv->saveIER = I915_READ(IER);
	dev_priv->saveIMR = I915_READ(IMR);

	/* VGA state */
	dev_priv->saveVGA0 = I915_READ(VGA0);
	dev_priv->saveVGA1 = I915_READ(VGA1);
	dev_priv->saveVGA_PD = I915_READ(VGA_PD);
	dev_priv->saveVGACNTRL = I915_READ(VGACNTRL);

	/* Clock gating state */
	dev_priv->saveD_STATE = I915_READ(D_STATE);
	dev_priv->saveDSPCLK_GATE_D = I915_READ(DSPCLK_GATE_D);

	/* Cache mode state */
	dev_priv->saveCACHE_MODE_0 = I915_READ(CACHE_MODE_0);

	/* Memory Arbitration state */
	dev_priv->saveMI_ARB_STATE = I915_READ(MI_ARB_STATE);

	/* Scratch space */
	for (i = 0; i < 16; i++) {
		dev_priv->saveSWF0[i] = I915_READ(SWF00 + (i << 2));
		dev_priv->saveSWF1[i] = I915_READ(SWF10 + (i << 2));
	}
	for (i = 0; i < 3; i++)
		dev_priv->saveSWF2[i] = I915_READ(SWF30 + (i << 2));

	/* Fences */
	if (IS_I965G(dev_priv)) {
		for (i = 0; i < 16; i++)
			dev_priv->saveFENCE[i] = I915_READ64(FENCE_REG_965_0 +
			    (i * 8));
	} else {
		for (i = 0; i < 8; i++)
			dev_priv->saveFENCE[i] = I915_READ(FENCE_REG_830_0 + (i * 4));

		if (IS_I945G(dev_priv) || IS_I945GM(dev_priv) ||
		    IS_G33(dev_priv))
			for (i = 0; i < 8; i++)
				dev_priv->saveFENCE[i+8] = I915_READ(FENCE_REG_945_8 + (i * 4));
	}
	i915_save_vga(dev_priv);

	return 0;
}

int
inteldrm_restore_state(struct drm_i915_private *dev_priv)
{
	int	i;

	pci_conf_write(dev_priv->pc, dev_priv->tag, LBB, dev_priv->saveLBB);

	/* Render Standby */
	if (IS_I965G(dev_priv) && IS_MOBILE(dev_priv))
		I915_WRITE(MCHBAR_RENDER_STANDBY, dev_priv->saveRENDERSTANDBY);

	/* Hardware status page */
	I915_WRITE(HWS_PGA, dev_priv->saveHWS);

	/* Display arbitration */
	I915_WRITE(DSPARB, dev_priv->saveDSPARB);

	/* Fences */
	if (IS_I965G(dev_priv)) {
		for (i = 0; i < 16; i++)
			I915_WRITE64(FENCE_REG_965_0 + (i * 8), dev_priv->saveFENCE[i]);
	} else {
		for (i = 0; i < 8; i++)
			I915_WRITE(FENCE_REG_830_0 + (i * 4), dev_priv->saveFENCE[i]);
		if (IS_I945G(dev_priv) || IS_I945GM(dev_priv) ||
		    IS_G33(dev_priv))
			for (i = 0; i < 8; i++)
				I915_WRITE(FENCE_REG_945_8 + (i * 4), dev_priv->saveFENCE[i+8]);
	}
	
	/* This is only meaningful in non-KMS mode */
	/* Don't restore them in KMS mode */
	i915_restore_modeset_reg(dev_priv);
	/* Cursor state */
	I915_WRITE(CURAPOS, dev_priv->saveCURAPOS);
	I915_WRITE(CURACNTR, dev_priv->saveCURACNTR);
	I915_WRITE(CURABASE, dev_priv->saveCURABASE);
	I915_WRITE(CURBPOS, dev_priv->saveCURBPOS);
	I915_WRITE(CURBCNTR, dev_priv->saveCURBCNTR);
	I915_WRITE(CURBBASE, dev_priv->saveCURBBASE);
	if (!IS_I9XX(dev_priv))
		I915_WRITE(CURSIZE, dev_priv->saveCURSIZE);

	/* CRT state */
	I915_WRITE(ADPA, dev_priv->saveADPA);

	/* LVDS state */
	if (IS_I965G(dev_priv))
		I915_WRITE(BLC_PWM_CTL2, dev_priv->saveBLC_PWM_CTL2);
	if (IS_MOBILE(dev_priv) && !IS_I830(dev_priv))
		I915_WRITE(LVDS, dev_priv->saveLVDS);
	if (!IS_I830(dev_priv) && !IS_845G(dev_priv))
		I915_WRITE(PFIT_CONTROL, dev_priv->savePFIT_CONTROL);

	I915_WRITE(PFIT_PGM_RATIOS, dev_priv->savePFIT_PGM_RATIOS);
	I915_WRITE(BLC_PWM_CTL, dev_priv->saveBLC_PWM_CTL);
	I915_WRITE(PP_ON_DELAYS, dev_priv->savePP_ON_DELAYS);
	I915_WRITE(PP_OFF_DELAYS, dev_priv->savePP_OFF_DELAYS);
	I915_WRITE(PP_DIVISOR, dev_priv->savePP_DIVISOR);
	I915_WRITE(PP_CONTROL, dev_priv->savePP_CONTROL);

	/* XXX: Display Port state */

	/* FIXME: restore TV & SDVO state */

	/* FBC info */
	I915_WRITE(FBC_CFB_BASE, dev_priv->saveFBC_CFB_BASE);
	I915_WRITE(FBC_LL_BASE, dev_priv->saveFBC_LL_BASE);
	I915_WRITE(FBC_CONTROL2, dev_priv->saveFBC_CONTROL2);
	I915_WRITE(FBC_CONTROL, dev_priv->saveFBC_CONTROL);

	/* VGA state */
	I915_WRITE(VGACNTRL, dev_priv->saveVGACNTRL);
	I915_WRITE(VGA0, dev_priv->saveVGA0);
	I915_WRITE(VGA1, dev_priv->saveVGA1);
	I915_WRITE(VGA_PD, dev_priv->saveVGA_PD);
	DRM_UDELAY(150);

	/* Clock gating state */
	I915_WRITE (D_STATE, dev_priv->saveD_STATE);
	I915_WRITE (DSPCLK_GATE_D, dev_priv->saveDSPCLK_GATE_D);

	/* Cache mode state */
	I915_WRITE (CACHE_MODE_0, dev_priv->saveCACHE_MODE_0 | 0xffff0000);

	/* Memory arbitration state */
	I915_WRITE (MI_ARB_STATE, dev_priv->saveMI_ARB_STATE | 0xffff0000);

	for (i = 0; i < 16; i++) {
		I915_WRITE(SWF00 + (i << 2), dev_priv->saveSWF0[i]);
		I915_WRITE(SWF10 + (i << 2), dev_priv->saveSWF1[i]);
	}
	for (i = 0; i < 3; i++)
		I915_WRITE(SWF30 + (i << 2), dev_priv->saveSWF2[i]);

	i915_restore_vga(dev_priv);

	return 0;
}

/* 
 * Reset the chip after a hang (965 only)
 *
 * The procedure that should be followed is relatively simple:
 *	- reset the chip using the reset reg
 *	- re-init context state
 *	- re-init Hardware status page
 *	- re-init ringbuffer
 *	- re-init interrupt state
 *	- re-init display
 */
void
inteldrm_965_reset(struct drm_i915_private *dev_priv, u_int8_t flags)
{
	pcireg_t	reg;
	int		i = 0;

	if (flags == GDRST_FULL)
		inteldrm_save_display(dev_priv);

	reg = pci_conf_read(dev_priv->pc, dev_priv->tag, GDRST);
	/*
	 * Set the domains we want to reset, then bit 0 (reset itself).
	 * then we wait for the hardware to clear it.
	 */
	pci_conf_write(dev_priv->pc, dev_priv->tag, GDRST,
	    reg | (u_int32_t)flags | ((flags == GDRST_FULL) ? 0x1 : 0x0));
	delay(50);
	/* don't clobber the rest of the register */
	pci_conf_write(dev_priv->pc, dev_priv->tag, GDRST, reg & 0xfe);

	/* if this fails we're pretty much fucked, but don't loop forever */
	do {
		delay(100);
		reg = pci_conf_read(dev_priv->pc, dev_priv->tag, GDRST);
	} while ((reg & 0x1) && ++i < 10);

	if (reg & 0x1)
		printf("bit 0 not cleared .. ");

	/* put everything back together again */

	/*
	 * GTT is already up (we didn't do a pci-level reset, thank god.
	 *
	 * We don't have to restore the contexts (we don't use them yet).
	 * So, if X is running we need to put the ringbuffer back first.
	 */
	 if (dev_priv->mm.suspended == 0) {
		struct drm_device *dev = (struct drm_device *)dev_priv->drmdev;
		if (inteldrm_start_ring(dev_priv) != 0)
			panic("can't restart ring, we're fucked"); 

		/* put the hardware status page back */
		if (I915_NEED_GFX_HWS(dev_priv))
			I915_WRITE(HWS_PGA, ((struct inteldrm_obj *)
			    dev_priv->hws_obj)->gtt_offset);
		else
			I915_WRITE(HWS_PGA,
			    dev_priv->hws_dmamem->map->dm_segs[0].ds_addr);
		I915_READ(HWS_PGA); /* posting read */

		/* so we remove the handler and can put it back in */
		DRM_UNLOCK();
		drm_irq_uninstall(dev);
		drm_irq_install(dev);
		DRM_LOCK();
	 } else
		printf("not restarting ring...\n");


	 if (flags == GDRST_FULL)
		inteldrm_restore_display(dev_priv);
}

/*
 * Debug code from here. 
 */
#ifdef WATCH_INACTIVE
void
inteldrm_verify_inactive(struct drm_i915_private *dev_priv, char *file,
    int line)
{
	struct drm_obj		*obj;
	struct inteldrm_obj	*obj_priv;

	TAILQ_FOREACH(obj_priv, &dev_priv->mm.inactive_list, list) {
		obj = (struct drm_obj *)obj_priv;
		if (obj_priv->pin_count || inteldrm_is_active(obj_priv) ||
		    obj->write_domain & I915_GEM_GPU_DOMAINS)
			DRM_ERROR("inactive %p (p $d a $d w $x) %s:%d\n",
			    obj, obj_priv->pin_count,
			    inteldrm_is_active(obj_priv),
			    obj->write_domain, file, line);
	}
}
#endif /* WATCH_INACTIVE */

#if (INTELDRM_DEBUG > 1)

static const char *get_pin_flag(struct inteldrm_obj *obj_priv)
{
	if (obj_priv->pin_count > 0)
		return "p";
	else
		return " ";
}

static const char *get_tiling_flag(struct inteldrm_obj *obj_priv)
{
    switch (obj_priv->tiling_mode) {
    default:
    case I915_TILING_NONE: return " ";
    case I915_TILING_X: return "X";
    case I915_TILING_Y: return "Y";
    }
}

void
i915_gem_seqno_info(int kdev)
{
	struct drm_device *dev = drm_get_device_from_kdev(kdev);
	drm_i915_private_t *dev_priv = dev->dev_private;

	if (dev_priv->hw_status_page != NULL) {
		printf("Current sequence: %d\n", i915_get_gem_seqno(dev_priv));
	} else {
		printf("Current sequence: hws uninitialized\n");
	}
}


void
i915_interrupt_info(int kdev)
{
	struct drm_device *dev = drm_get_device_from_kdev(kdev);
	drm_i915_private_t *dev_priv = dev->dev_private;

	printf("Interrupt enable:    %08x\n",
		   I915_READ(IER));
	printf("Interrupt identity:  %08x\n",
		   I915_READ(IIR));
	printf("Interrupt mask:      %08x\n",
		   I915_READ(IMR));
	printf("Pipe A stat:         %08x\n",
		   I915_READ(PIPEASTAT));
	printf("Pipe B stat:         %08x\n",
		   I915_READ(PIPEBSTAT));
	printf("Interrupts received: 0\n");
	if (dev_priv->hw_status_page != NULL) {
		printf("Current sequence:    %d\n",
			   i915_get_gem_seqno(dev_priv));
	} else {
		printf("Current sequence:    hws uninitialized\n");
	}
}

void
i915_gem_fence_regs_info(int kdev)
{
	struct drm_device *dev = drm_get_device_from_kdev(kdev);
	drm_i915_private_t *dev_priv = dev->dev_private;
	int i;

	printf("Reserved fences = %d\n", dev_priv->fence_reg_start);
	printf("Total fences = %d\n", dev_priv->num_fence_regs);
	for (i = 0; i < dev_priv->num_fence_regs; i++) {
		struct drm_obj *obj = dev_priv->fence_regs[i].obj;

		if (obj == NULL) {
			printf("Fenced object[%2d] = unused\n", i);
		} else {
			struct inteldrm_obj *obj_priv;

			obj_priv = (struct inteldrm_obj *)obj;
			printf("Fenced object[%2d] = %p: %s "
				   "%08x %08zx %08x %s %08x %08x %d",
				   i, obj, get_pin_flag(obj_priv),
				   obj_priv->gtt_offset,
				   obj->size, obj_priv->stride,
				   get_tiling_flag(obj_priv),
				   obj->read_domains, obj->write_domain,
				   obj_priv->last_rendering_seqno);
			if (obj->name)
				printf(" (name: %d)", obj->name);
			printf("\n");
		}
	}
}

void
i915_hws_info(int kdev)
{
	struct drm_device *dev = drm_get_device_from_kdev(kdev);
	drm_i915_private_t *dev_priv = dev->dev_private;
	int i;
	volatile u32 *hws;

	hws = (volatile u32 *)dev_priv->hw_status_page;
	if (hws == NULL)
		return;

	for (i = 0; i < 4096 / sizeof(u32) / 4; i += 4) {
		printf("0x%08x: 0x%08x 0x%08x 0x%08x 0x%08x\n",
			   i * 4,
			   hws[i], hws[i + 1], hws[i + 2], hws[i + 3]);
	}
}

static void
i915_dump_pages(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_size_t size)
{
	bus_addr_t	offset = 0;
	int		i = 0;

	/*
	 * this is a bit odd so i don't have to play with the intel
	 * tools too much.
	 */
	for (offset = 0; offset < size; offset += 4, i += 4) {
		if (i == PAGE_SIZE)
			i = 0;
		printf("%08x :  %08x\n", i, bus_space_read_4(bst, bsh,
		    offset));
	}
}

void
i915_batchbuffer_info(int kdev)
{
	struct drm_device	*dev = drm_get_device_from_kdev(kdev);
	drm_i915_private_t	*dev_priv = dev->dev_private;
	struct drm_obj		*obj;
	struct inteldrm_obj	*obj_priv;
	bus_space_handle_t	 bsh;
	int			 ret;

	TAILQ_FOREACH(obj_priv, &dev_priv->mm.active_list, list) {
		obj = &obj_priv->obj;
		if (obj->read_domains & I915_GEM_DOMAIN_COMMAND) {
			if ((ret = agp_map_subregion(dev_priv->agph,
			    obj_priv->gtt_offset, obj->size, &bsh)) != 0) {
				DRM_ERROR("Failed to map pages: %d\n", ret);
				return;
			}
			printf("--- gtt_offset = 0x%08x\n",
			    obj_priv->gtt_offset);
			i915_dump_pages(dev_priv->bst, bsh, obj->size);
			agp_unmap_subregion(dev_priv->agph, dev_priv->ring.bsh,
			    obj->size);
		}
	}
}

void
i915_ringbuffer_data(int kdev)
{
	struct drm_device	*dev = drm_get_device_from_kdev(kdev);
	drm_i915_private_t	*dev_priv = dev->dev_private;
	bus_size_t		 off;

	if (!dev_priv->ring.ring_obj) {
		printf("No ringbuffer setup\n");
		return;
	}

	for (off = 0; off < dev_priv->ring.size; off += 4)
		printf("%08x :  %08x\n", off, bus_space_read_4(dev_priv->bst,
		    dev_priv->ring.bsh, off));
}

void
i915_ringbuffer_info(int kdev)
{
	struct drm_device	*dev = drm_get_device_from_kdev(kdev);
	drm_i915_private_t	*dev_priv = dev->dev_private;
	u_int32_t		 head, tail;

	head = I915_READ(PRB0_HEAD) & HEAD_ADDR;
	tail = I915_READ(PRB0_TAIL) & TAIL_ADDR;

	printf("RingHead :  %08x\n", head);
	printf("RingTail :  %08x\n", tail);
	printf("RingMask :  %08x\n", dev_priv->ring.size - 1);
	printf("RingSize :  %08lx\n", dev_priv->ring.size);
	printf("Acthd :  %08x\n", I915_READ(IS_I965G(dev_priv) ?
	    ACTHD_I965 : ACTHD));
}

#endif
