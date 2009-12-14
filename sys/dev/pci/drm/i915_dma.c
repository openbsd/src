/* i915_dma.c -- DMA support for the I915 -*- linux-c -*-
 */
/*
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

#include "drmP.h"
#include "drm.h"
#include "i915_drm.h"
#include "i915_drv.h"

/**
 * Sets up the hardware status page for devices that need a physical address
 * in the register.
 */
int
i915_init_phys_hws(drm_i915_private_t *dev_priv, bus_dma_tag_t dmat)
{
	/* Program Hardware Status Page */
	if ((dev_priv->hws_dmamem = drm_dmamem_alloc(dmat, PAGE_SIZE,
	    PAGE_SIZE, 1, PAGE_SIZE, 0, BUS_DMA_READ)) == NULL) {
		return (ENOMEM);
	}

	dev_priv->hw_status_page = dev_priv->hws_dmamem->kva;

	memset(dev_priv->hw_status_page, 0, PAGE_SIZE);

	bus_dmamap_sync(dmat, dev_priv->hws_dmamem->map, 0, PAGE_SIZE,
	    BUS_DMASYNC_PREREAD);
	I915_WRITE(HWS_PGA, dev_priv->hws_dmamem->map->dm_segs[0].ds_addr);
	DRM_DEBUG("Enabled hardware status page\n");
	return (0);
}

/**
 * Frees the hardware status page, whether it's a physical address of a virtual
 * address set up by the X Server.
 */
void
i915_free_hws(drm_i915_private_t *dev_priv, bus_dma_tag_t dmat)
{
	if (dev_priv->hws_dmamem) {
		drm_dmamem_free(dmat, dev_priv->hws_dmamem);
		dev_priv->hws_dmamem = NULL;
	}

	if (dev_priv->status_gfx_addr) {
		dev_priv->status_gfx_addr = 0;
		bus_space_unmap(dev_priv->bst, dev_priv->hws_map.bsh, 4 * 1024);
		dev_priv->hws_map.bsh = NULL;
	}

	/* Need to rewrite hardware status page */
	I915_WRITE(HWS_PGA, 0x1ffff000);
	dev_priv->hw_status_page = NULL;
}

int
i915_dma_cleanup(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;

	if (dev_priv->ring.bsh != NULL) {
		bus_space_unmap(dev_priv->bst, dev_priv->ring.bsh,
		    dev_priv->ring.size);
		dev_priv->ring.bsh = NULL;
		dev_priv->ring.size = 0;
	}

	/* Clear the HWS virtual address at teardown */
	if (I915_NEED_GFX_HWS(dev_priv))
		i915_free_hws(dev_priv, dev->dmat);

	return 0;
}

static int i915_initialize(struct drm_device * dev, drm_i915_init_t * init)
{
	drm_i915_private_t	*dev_priv = dev->dev_private;
	int			 ret;

	dev_priv->sarea = drm_getsarea(dev);
	if (!dev_priv->sarea) {
		DRM_ERROR("can not find sarea!\n");
		i915_dma_cleanup(dev);
		return EINVAL;
	}

	if (init->sarea_priv_offset)
		dev_priv->sarea_priv = (drm_i915_sarea_t *)
			((u8 *) dev_priv->sarea->handle +
			 init->sarea_priv_offset);
	else {
		/* No sarea_priv for you! */
		dev_priv->sarea_priv = NULL;
	}

	dev_priv->ring.size = init->ring_size;

	if ((ret = bus_space_map(dev_priv->bst, init->ring_start,
	    init->ring_size, 0, &dev_priv->ring.bsh)) != 0) {
		DRM_INFO("can't map ringbuffer\n");
		i915_dma_cleanup(dev);
		return (ret);
	}

	/* Allow hardware batchbuffers unless told otherwise.
	 */
	dev_priv->allow_batchbuffer = 1;

	return 0;
}

static int i915_dma_resume(struct drm_device * dev)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;

	DRM_DEBUG("\n");

	if (!dev_priv->sarea) {
		DRM_ERROR("can not find sarea!\n");
		return EINVAL;
	}

	if (dev_priv->ring.bsh == NULL) {
		DRM_ERROR("dma_resume without mapped ring buffer\n");
		return ENOMEM;
	}

	/* Program Hardware Status Page */
	if (!dev_priv->hw_status_page) {
		DRM_ERROR("Can not find hardware status page\n");
		return EINVAL;
	}
	DRM_DEBUG("hw status page @ %p\n", dev_priv->hw_status_page);

	if (dev_priv->status_gfx_addr != 0)
		I915_WRITE(HWS_PGA, dev_priv->status_gfx_addr);
	else {
		bus_dmamap_sync(dev->dmat, dev_priv->hws_dmamem->map, 0,
		    PAGE_SIZE, BUS_DMASYNC_PREREAD);
		I915_WRITE(HWS_PGA,
		    dev_priv->hws_dmamem->map->dm_segs[0].ds_addr);
	}
	DRM_DEBUG("Enabled hardware status page\n");

	return 0;
}

int i915_dma_init(struct drm_device *dev, void *data,
			 struct drm_file *file_priv)
{
	drm_i915_init_t *init = data;
	int retcode = 0;

	switch (init->func) {
	case I915_INIT_DMA:
		retcode = i915_initialize(dev, init);
		break;
	case I915_CLEANUP_DMA:
		retcode = i915_dma_cleanup(dev);
		break;
	case I915_RESUME_DMA:
		retcode = i915_dma_resume(dev);
		break;
	default:
		retcode = EINVAL;
		break;
	}

	return retcode;
}

/* Implement basically the same security restrictions as hardware does
 * for MI_BATCH_NON_SECURE.  These can be made stricter at any time.
 *
 * Most of the calculations below involve calculating the size of a
 * particular instruction.  It's important to get the size right as
 * that tells us where the next instruction to check is.  Any illegal
 * instruction detected will be given a size of zero, which is a
 * signal to abort the rest of the buffer.
 */
static int validate_cmd(int cmd)
{
	switch (((cmd >> 29) & 0x7)) {
	case 0x0:
		switch ((cmd >> 23) & 0x3f) {
		case 0x0:
			return 1;	/* MI_NOOP */
		case 0x4:
			return 1;	/* MI_FLUSH */
		default:
			return 0;	/* disallow everything else */
		}
		break;
	case 0x1:
		return 0;	/* reserved */
	case 0x2:
		return (cmd & 0xff) + 2;	/* 2d commands */
	case 0x3:
		if (((cmd >> 24) & 0x1f) <= 0x18)
			return 1;

		switch ((cmd >> 24) & 0x1f) {
		case 0x1c:
			return 1;
		case 0x1d:
			switch ((cmd >> 16) & 0xff) {
			case 0x3:
				return (cmd & 0x1f) + 2;
			case 0x4:
				return (cmd & 0xf) + 2;
			default:
				return (cmd & 0xffff) + 2;
			}
		case 0x1e:
			if (cmd & (1 << 23))
				return (cmd & 0xffff) + 1;
			else
				return 1;
		case 0x1f:
			if ((cmd & (1 << 23)) == 0)	/* inline vertices */
				return (cmd & 0x1ffff) + 2;
			else if (cmd & (1 << 17))	/* indirect random */
				if ((cmd & 0xffff) == 0)
					return 0;	/* unknown length, too hard */
				else
					return (((cmd & 0xffff) + 1) / 2) + 1;
			else
				return 2;	/* indirect sequential */
		default:
			return 0;
		}
	default:
		return 0;
	}

	return 0;
}

static int i915_emit_cmds(struct drm_device *dev, int __user *buffer,
			  int dwords)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	int i;

	if ((dwords + 1) * sizeof(u_int32_t) >= dev_priv->ring.size - 8)
		return EINVAL;

	BEGIN_LP_RING((dwords+1)&~1);

	for (i = 0; i < dwords;) {
		int cmd, sz;

		if (DRM_COPY_FROM_USER(&cmd, &buffer[i], sizeof(cmd)))
			return EINVAL;

		if ((sz = validate_cmd(cmd)) == 0 || i + sz > dwords)
			return EINVAL;

		OUT_RING(cmd);

		while (++i, --sz) {
			if (DRM_COPY_FROM_USER(&cmd, &buffer[i], sizeof(cmd))) {
				return EINVAL;
			}
			OUT_RING(cmd);
		}
	}

	if (dwords & 1)
		OUT_RING(0);

	ADVANCE_LP_RING();

	return 0;
}

static int
i915_emit_box(struct drm_device * dev, struct drm_clip_rect *boxes,
    int i, int DR1, int DR4)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct drm_clip_rect box;

	if (DRM_COPY_FROM_USER(&box, &boxes[i], sizeof(box))) {
		return EFAULT;
	}

	if (box.y2 <= box.y1 || box.x2 <= box.x1 || box.y2 <= 0 ||
	    box.x2 <= 0) {
		DRM_ERROR("Bad box %d,%d..%d,%d\n",
		    box.x1, box.y1, box.x2, box.y2);
		return EINVAL;
	}

	if (IS_I965G(dev_priv)) {
		BEGIN_LP_RING(4);
		OUT_RING(GFX_OP_DRAWRECT_INFO_I965);
		OUT_RING((box.x1 & 0xffff) | (box.y1 << 16));
		OUT_RING(((box.x2 - 1) & 0xffff) | ((box.y2 - 1) << 16));
		OUT_RING(DR4);
		ADVANCE_LP_RING();
	} else {
		BEGIN_LP_RING(6);
		OUT_RING(GFX_OP_DRAWRECT_INFO);
		OUT_RING(DR1);
		OUT_RING((box.x1 & 0xffff) | (box.y1 << 16));
		OUT_RING(((box.x2 - 1) & 0xffff) | ((box.y2 - 1) << 16));
		OUT_RING(DR4);
		OUT_RING(0);
		ADVANCE_LP_RING();
	}

	return 0;
}

/* XXX: Emitting the counter should really be moved to part of the IRQ
 * emit. For now, do it in both places:
 */

void i915_emit_breadcrumb(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;

	if (++dev_priv->counter > BREADCRUMB_MASK) {
		 dev_priv->counter = 1;
		 DRM_DEBUG("Breadcrumb counter wrapped around\n");
	}

	if (dev_priv->sarea_priv)
		dev_priv->sarea_priv->last_enqueue = dev_priv->counter;

	BEGIN_LP_RING(4);
	OUT_RING(MI_STORE_DWORD_INDEX);
	OUT_RING(I915_BREADCRUMB_INDEX << MI_STORE_DWORD_INDEX_SHIFT);
	OUT_RING(dev_priv->counter);
	OUT_RING(0);
	ADVANCE_LP_RING();
}

static int i915_dispatch_cmdbuffer(struct drm_device * dev,
				   drm_i915_cmdbuffer_t * cmd)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	int nbox = cmd->num_cliprects;
	int i = 0, count, ret;

	if (cmd->sz <= 0 || (cmd->sz & 0x3) != 0) {
		DRM_ERROR("negative value or incorrect alignment\n");
		return EINVAL;
	}

	inteldrm_update_ring(dev_priv);

	count = nbox ? nbox : 1;

	for (i = 0; i < count; i++) {
		if (i < nbox) {
			ret = i915_emit_box(dev, cmd->cliprects, i,
					    cmd->DR1, cmd->DR4);
			if (ret)
				return ret;
		}

		ret = i915_emit_cmds(dev, (int __user *)cmd->buf, cmd->sz / 4);
		if (ret)
			return ret;
	}

	i915_emit_breadcrumb(dev);
	return 0;
}

int i915_dispatch_batchbuffer(struct drm_device * dev,
			      drm_i915_batchbuffer_t * batch)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct drm_clip_rect __user *boxes = batch->cliprects;
	int nbox = batch->num_cliprects;
	int i = 0, count;

	if ((batch->start | batch->used) & 0x7) {
		DRM_ERROR("alignment\n");
		return EINVAL;
	}

	inteldrm_update_ring(dev_priv);

	count = nbox ? nbox : 1;

	for (i = 0; i < count; i++) {
		if (i < nbox) {
			int ret = i915_emit_box(dev, boxes, i,
						batch->DR1, batch->DR4);
			if (ret)
				return ret;
		}

		if (!IS_I830(dev_priv) && !IS_845G(dev_priv)) {
			BEGIN_LP_RING(2);
			if (IS_I965G(dev_priv)) {
				OUT_RING(MI_BATCH_BUFFER_START | (2 << 6) | MI_BATCH_NON_SECURE_I965);
				OUT_RING(batch->start);
			} else {
				OUT_RING(MI_BATCH_BUFFER_START | (2 << 6));
				OUT_RING(batch->start | MI_BATCH_NON_SECURE);
			}
			ADVANCE_LP_RING();
		} else {
			BEGIN_LP_RING(4);
			OUT_RING(MI_BATCH_BUFFER);
			OUT_RING(batch->start | MI_BATCH_NON_SECURE);
			OUT_RING(batch->start + batch->used - 4);
			OUT_RING(0);
			ADVANCE_LP_RING();
		}
	}

	i915_emit_breadcrumb(dev);

	return 0;
}

int i915_flush_ioctl(struct drm_device *dev, void *data,
			    struct drm_file *file_priv)
{
	drm_i915_private_t	*dev_priv = dev->dev_private;
	int			 ret;

	LOCK_TEST_WITH_RETURN(dev, file_priv);

	DRM_LOCK();
	inteldrm_update_ring(dev_priv);
	ret = inteldrm_wait_ring(dev_priv, dev_priv->ring.size - 8);
	DRM_UNLOCK();

	return (ret);
}

int i915_batchbuffer(struct drm_device *dev, void *data,
			    struct drm_file *file_priv)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;
	drm_i915_batchbuffer_t *batch = data;
	int ret;

	if (!dev_priv->allow_batchbuffer) {
		DRM_ERROR("Batchbuffer ioctl disabled\n");
		return EINVAL;
	}

	DRM_DEBUG("i915 batchbuffer, start %x used %d cliprects %d\n",
		  batch->start, batch->used, batch->num_cliprects);

	if (batch->num_cliprects < 0)
		return EINVAL;

	LOCK_TEST_WITH_RETURN(dev, file_priv);

	DRM_LOCK();
	ret = i915_dispatch_batchbuffer(dev, batch);
	DRM_UNLOCK();

	if (dev_priv->sarea_priv != NULL)
		dev_priv->sarea_priv->last_dispatch = READ_BREADCRUMB(dev_priv);
	return ret;
}

int i915_cmdbuffer(struct drm_device *dev, void *data,
			  struct drm_file *file_priv)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *)dev->dev_private;
	drm_i915_cmdbuffer_t *cmdbuf = data;
	int ret;

	DRM_DEBUG("i915 cmdbuffer, buf %p sz %d cliprects %d\n",
		  cmdbuf->buf, cmdbuf->sz, cmdbuf->num_cliprects);

	if (cmdbuf->num_cliprects < 0)
		return EINVAL;

	LOCK_TEST_WITH_RETURN(dev, file_priv);

	DRM_LOCK();
	ret = i915_dispatch_cmdbuffer(dev, cmdbuf);
	DRM_UNLOCK();
	if (ret) {
		DRM_ERROR("i915_dispatch_cmdbuffer failed\n");
		return ret;
	}

	if (dev_priv->sarea_priv != NULL)
		dev_priv->sarea_priv->last_dispatch = READ_BREADCRUMB(dev_priv);
	return 0;
}

int i915_getparam(struct drm_device *dev, void *data,
			 struct drm_file *file_priv)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	drm_i915_getparam_t *param = data;
	int value;

	if (!dev_priv) {
		DRM_ERROR("called with no initialization\n");
		return EINVAL;
	}

	switch (param->param) {
	case I915_PARAM_IRQ_ACTIVE:
		value = dev->irq_enabled;
		break;
	case I915_PARAM_ALLOW_BATCHBUFFER:
		value = dev_priv->allow_batchbuffer ? 1 : 0;
		break;
	case I915_PARAM_LAST_DISPATCH:
		value = READ_BREADCRUMB(dev_priv);
		break;
	case I915_PARAM_CHIPSET_ID:
		value = dev_priv->pci_device;
		break;
	case I915_PARAM_HAS_GEM:
		value = 0;
		break;
	case I915_PARAM_NUM_FENCES_AVAIL:
		value = 0;
		break;
	default:
		DRM_ERROR("Unknown parameter %d\n", param->param);
		return EINVAL;
	}

	if (DRM_COPY_TO_USER(param->value, &value, sizeof(int))) {
		DRM_ERROR("DRM_COPY_TO_USER failed\n");
		return EFAULT;
	}

	return 0;
}

int i915_setparam(struct drm_device *dev, void *data,
			 struct drm_file *file_priv)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	drm_i915_setparam_t *param = data;

	if (!dev_priv) {
		DRM_ERROR("called with no initialization\n");
		return EINVAL;
	}

	switch (param->param) {
	case I915_SETPARAM_USE_MI_BATCHBUFFER_START:
		break;
	case I915_SETPARAM_TEX_LRU_LOG_GRANULARITY:
		/* We really don't care anymore */
		break;
	case I915_SETPARAM_ALLOW_BATCHBUFFER:
		dev_priv->allow_batchbuffer = param->value;
		break;
	default:
		DRM_ERROR("unknown parameter %d\n", param->param);
		return EINVAL;
	}

	return 0;
}

int i915_set_status_page(struct drm_device *dev, void *data,
				struct drm_file *file_priv)
{
	drm_i915_private_t	*dev_priv = dev->dev_private;
	drm_i915_hws_addr_t	*hws = data;
	int			 ret;

	if (!I915_NEED_GFX_HWS(dev_priv))
		return EINVAL;

	if (!dev_priv) {
		DRM_ERROR("called with no initialization\n");
		return EINVAL;
	}
	DRM_DEBUG("set status page addr 0x%08x\n", (u32)hws->addr);

	dev_priv->status_gfx_addr = hws->addr & (0x1ffff<<12);

	dev_priv->hws_map.offset = dev->agp->base + hws->addr;
	if (dev_priv->hws_map.offset > (dev->agp->base +
	    dev->agp->info.ai_aperture_size)) {
		DRM_INFO("tried to map hws past end of aperture!\n");
		return (EINVAL);
	}
	dev_priv->hws_map.size = 4*1024;

	if ((ret = bus_space_map(dev_priv->bst, dev_priv->hws_map.offset,
	    dev_priv->hws_map.size, BUS_SPACE_MAP_LINEAR,
	    &dev_priv->hws_map.bsh)) != 0) {
		DRM_INFO("can't hws page\n");
		i915_dma_cleanup(dev);
		dev_priv->status_gfx_addr = 0;
		return (ret);
	}
	dev_priv->hw_status_page = bus_space_vaddr(dev_priv->bst,
	    dev_priv->hws_map.bsh);

	memset(dev_priv->hw_status_page, 0, PAGE_SIZE);
	I915_WRITE(HWS_PGA, dev_priv->status_gfx_addr);
	DRM_DEBUG("load hws HWS_PGA with gfx mem 0x%x\n",
			dev_priv->status_gfx_addr);
	DRM_DEBUG("load hws at %p\n", dev_priv->hw_status_page);
	return 0;
}
