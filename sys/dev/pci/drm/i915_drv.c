/* i915_drv.c -- Intel i915 driver -*- linux-c -*-
 * Created: Wed Feb 14 17:10:04 2001 by gareth@valinux.com
 */
/*-
 * Copyright 2003 Tungsten Graphics, Inc., Cedar Park, Texas.
 * Copyright 2000 VA Linux Systems, Inc., Sunnyvale, California.
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
 *
 */

#include "drmP.h"
#include "drm.h"
#include "i915_drm.h"
#include "i915_drv.h"

int	inteldrm_probe(struct device *, void *, void *);
void	inteldrm_attach(struct device *, struct device *, void *);
int	inteldrm_detach(struct device *, int);
int	inteldrm_ioctl(struct drm_device *, u_long, caddr_t, struct drm_file *);
int	inteldrm_activate(struct device *, int);
void	inteldrm_lastclose(struct drm_device *);

void	inteldrm_wrap_ring(struct drm_i915_private *);

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

const static struct drm_pcidev inteldrm_pciidlist[] = {
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82830M_IGD,
	    CHIP_I830|CHIP_M},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82845G_IGD,
	    CHIP_I845G},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82855GM_IGD,
	    CHIP_I85X|CHIP_M},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82865G_IGD,
	    CHIP_I865G},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82915G_IGD_1,
	    CHIP_I915G|CHIP_I9XX},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_E7221_IGD,
	    CHIP_I915G|CHIP_I9XX},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82915GM_IGD_1,
	    CHIP_I915GM|CHIP_I9XX|CHIP_M},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82945G_IGD_1,
	    CHIP_I945G|CHIP_I9XX},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82945GM_IGD_1,
	    CHIP_I945GM|CHIP_I9XX|CHIP_M},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82945GME_IGD_1,
	    CHIP_I945GM|CHIP_I9XX|CHIP_M},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82946GZ_IGD_1,
	    CHIP_I965|CHIP_I9XX},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82G35_IGD_1,
	    CHIP_I965|CHIP_I9XX},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82Q965_IGD_1,
	    CHIP_I965|CHIP_I9XX},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82G965_IGD_1,
	    CHIP_I965|CHIP_I9XX},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82GM965_IGD_1,
	    CHIP_I965GM|CHIP_I965|CHIP_I9XX|CHIP_M},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82GME965_IGD_1,
	    CHIP_I965|CHIP_I9XX},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82G33_IGD_1,
	    CHIP_G33|CHIP_I9XX|CHIP_HWS},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82Q35_IGD_1,
	    CHIP_G33|CHIP_I9XX|CHIP_HWS},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82Q33_IGD_1,
	    CHIP_G33|CHIP_I9XX|CHIP_HWS},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82GM45_IGD_1,
	    CHIP_GM45|CHIP_I965|CHIP_I9XX|CHIP_M|CHIP_HWS},
	{PCI_VENDOR_INTEL, 0x2E02,
	    CHIP_G4X|CHIP_I965|CHIP_I9XX|CHIP_HWS},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82Q45_IGD_1,
	    CHIP_G4X|CHIP_I965|CHIP_I9XX|CHIP_HWS},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82G45_IGD_1,
	    CHIP_G4X|CHIP_I965|CHIP_I9XX|CHIP_HWS},
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82G41_IGD_1,
	    CHIP_G4X|CHIP_I965|CHIP_I9XX|CHIP_HWS},
	{0, 0, 0}
};

static const struct drm_driver_info inteldrm_driver = {
	.buf_priv_size		= 1,	/* No dev_priv */
	.ioctl			= inteldrm_ioctl,
	.lastclose		= inteldrm_lastclose,
	.vblank_pipes		= 2,
	.get_vblank_counter	= i915_get_vblank_counter,
	.enable_vblank		= i915_enable_vblank,
	.disable_vblank		= i915_disable_vblank,
	.irq_install		= i915_driver_irq_install,
	.irq_uninstall		= i915_driver_irq_uninstall,

	.name			= DRIVER_NAME,
	.desc			= DRIVER_DESC,
	.date			= DRIVER_DATE,
	.major			= DRIVER_MAJOR,
	.minor			= DRIVER_MINOR,
	.patchlevel		= DRIVER_PATCHLEVEL,

	.flags			= DRIVER_AGP | DRIVER_AGP_REQUIRE |
				    DRIVER_MTRR | DRIVER_IRQ,
};

int
inteldrm_probe(struct device *parent, void *match, void *aux)
{
	return (drm_pciprobe((struct pci_attach_args *)aux,
	    inteldrm_pciidlist));
}

void
inteldrm_attach(struct device *parent, struct device *self, void *aux)
{
	struct drm_i915_private	*dev_priv = (struct drm_i915_private *)self;
	struct pci_attach_args	*pa = aux;
	struct vga_pci_bar	*bar;
	const struct drm_pcidev	*id_entry;

	id_entry = drm_find_description(PCI_VENDOR(pa->pa_id),
	    PCI_PRODUCT(pa->pa_id), inteldrm_pciidlist);
	dev_priv->flags = id_entry->driver_private;
	dev_priv->pci_device = PCI_PRODUCT(pa->pa_id);

	dev_priv->pc = pa->pa_pc;
	dev_priv->tag = pa->pa_tag;
	dev_priv->dmat = pa->pa_dmat;
	dev_priv->bst = pa->pa_memt;

	/* Add register map (needed for suspend/resume) */
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

	/* Init HWS */
	if (!I915_NEED_GFX_HWS(dev_priv)) {
		if (i915_init_phys_hws(dev_priv, pa->pa_dmat) != 0) {
			printf(": couldn't initialize hardware status page\n");
			return;
		}
	}

	printf(": %s\n", pci_intr_string(pa->pa_pc, dev_priv->ih));

	mtx_init(&dev_priv->user_irq_lock, IPL_BIO);

	/* All intel chipsets need to be treated as agp, so just pass one */
	dev_priv->drmdev = drm_attach_pci(&inteldrm_driver, pa, 1, self);
}

int
inteldrm_detach(struct device *self, int flags)
{
	struct drm_i915_private *dev_priv = (struct drm_i915_private *)self;

	if (dev_priv->drmdev != NULL) {
		config_detach(dev_priv->drmdev, flags);
		dev_priv->drmdev = NULL;
	}

	i915_free_hws(dev_priv, dev_priv->dmat);

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
		}
	}
	return (EINVAL);
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
		tsleep(dev_priv, PZERO | PCATCH, "i915wt",
		    hz / 100);
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
inteldrm_lastclose(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;

	dev_priv->sarea_priv = NULL;

	i915_dma_cleanup(dev);
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
