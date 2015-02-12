/*	$OpenBSD: i915_dma.c,v 1.18 2015/02/12 04:56:03 kettenis Exp $	*/
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

#include <dev/pci/drm/drmP.h>
#include <dev/pci/drm/drm.h>
#include <dev/pci/drm/i915_drm.h>
#include "i915_drv.h"
#include "intel_drv.h"
#include <dev/pci/drm/drm_crtc_helper.h>

void
i915_kernel_lost_context(struct drm_device * dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
#if 0
	struct drm_i915_master_private *master_priv;
#endif
	struct intel_ring_buffer *ring = LP_RING(dev_priv);

	/*
	 * We should never lose context on the ring with modesetting
	 * as we don't expose it to userspace
	 */
	if (drm_core_check_feature(dev, DRIVER_MODESET))
		return;

	ring->head = I915_READ_HEAD(ring) & HEAD_ADDR;
	ring->tail = I915_READ_TAIL(ring) & TAIL_ADDR;
	ring->space = ring->head - (ring->tail + I915_RING_FREE_SPACE);
	if (ring->space < 0)
		ring->space += ring->size;

#if 0
	if (!dev->primary->master)
		return;

	master_priv = dev->primary->master->driver_priv;
	if (ring->head == ring->tail && master_priv->sarea_priv)
		master_priv->sarea_priv->perf_boxes |= I915_BOX_RING_EMPTY;
#endif
}


int
i915_getparam(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	drm_i915_getparam_t *param = data;
	int value;

	if (!dev_priv) {
		DRM_ERROR("called with no initialization\n");
		return -EINVAL;
	}

	switch (param->param) {
	case I915_PARAM_CHIPSET_ID:
		value = dev->pci_device;
		break;
	case I915_PARAM_HAS_GEM:
		value = 1;
		break;
	case I915_PARAM_NUM_FENCES_AVAIL:
		value = dev_priv->num_fence_regs - dev_priv->fence_reg_start;
		break;
	case I915_PARAM_HAS_OVERLAY:
		value = dev_priv->overlay ? 1 : 0;
		break;
	case I915_PARAM_HAS_PAGEFLIPPING:
		value = 1;
		break;
	case I915_PARAM_HAS_EXECBUF2:
		/* depends on GEM */
		value = 1;
		break;
	case I915_PARAM_HAS_BSD:
		value = intel_ring_initialized(&dev_priv->ring[VCS]);
		break;
	case I915_PARAM_HAS_BLT:
		value = intel_ring_initialized(&dev_priv->ring[BCS]);
		break;
	case I915_PARAM_HAS_RELAXED_FENCING:
		value = 1;
		break;
	case I915_PARAM_HAS_COHERENT_RINGS:
		value = 1;
		break;
	case I915_PARAM_HAS_EXEC_CONSTANTS:
		value = INTEL_INFO(dev)->gen >= 4;
		break;
	case I915_PARAM_HAS_RELAXED_DELTA:
		value = 1;
		break;
	case I915_PARAM_HAS_GEN7_SOL_RESET:
		value = 1;
		break;
	case I915_PARAM_HAS_LLC:
		value = HAS_LLC(dev);
		break;
	case I915_PARAM_HAS_WAIT_TIMEOUT:
		value = 1;
		break;
	case I915_PARAM_HAS_SEMAPHORES:
		value = i915_semaphore_is_enabled(dev);
		break;
	case I915_PARAM_HAS_SECURE_BATCHES:
		value = DRM_SUSER(curproc);
		break;
	case I915_PARAM_HAS_PINNED_BATCHES:
		value = 1;
		break;
	default:
		DRM_DEBUG_DRIVER("Unknown parameter %d\n",
				 param->param);
		return -EINVAL;
	}

	return -copyout(&value, param->value, sizeof(int));
}

int
i915_setparam(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	drm_i915_setparam_t *param = data;

	if (!dev_priv) {
		DRM_ERROR("called with no initialization\n");
		return -EINVAL;
	}

	switch (param->param) {
	case I915_SETPARAM_NUM_USED_FENCES:
		if (param->value > dev_priv->num_fence_regs ||
		    param->value < 0)
			return -EINVAL;
		/* Userspace can use first N regs */
		dev_priv->fence_reg_start = param->value;
		break;
	default:
		DRM_DEBUG_DRIVER("unknown parameter %d\n",
					param->param);
		return -EINVAL;
	}

	return 0;
}

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
intel_setup_mchbar(struct inteldrm_softc *dev_priv,
    struct pci_attach_args *bpa)
{
	struct drm_device	*dev = dev_priv->dev;
	u_int64_t		 mchbar_addr;
	pcireg_t		 tmp, low, high = 0;
	u_long			 addr;
	int			 reg, ret = 1, enabled = 0;

	reg = INTEL_INFO(dev)->gen >= 4 ?  MCHBAR_I965 : MCHBAR_I915;

	if (IS_I915G(dev) || IS_I915GM(dev)) {
		tmp = pci_conf_read(bpa->pa_pc, bpa->pa_tag, DEVEN_REG);
		enabled = !!(tmp & DEVEN_MCHBAR_EN);
	} else {
		tmp = pci_conf_read(bpa->pa_pc, bpa->pa_tag, reg);
		enabled = tmp & 1;
	}

	if (enabled) {
		return (0);
	}

	if (INTEL_INFO(dev)->gen >= 4)
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
			if (INTEL_INFO(dev)->gen >= 4)
				pci_conf_write(bpa->pa_pc, bpa->pa_tag,
				    reg + 4, upper_32_bits(mchbar_addr));
			pci_conf_write(bpa->pa_pc, bpa->pa_tag,
			    reg, mchbar_addr & 0xffffffff);
		}
	}
	/* set the enable bit */
	if (IS_I915G(dev) || IS_I915GM(dev)) {
		pci_conf_write(bpa->pa_pc, bpa->pa_tag, DEVEN_REG,
		    tmp | DEVEN_MCHBAR_EN);
	} else {
		tmp = pci_conf_read(bpa->pa_pc, bpa->pa_tag, reg);
		pci_conf_write(bpa->pa_pc, bpa->pa_tag, reg, tmp | 1);
	}

	return (ret);
}

/*
 * we take the trinary returned from intel_setup_mchbar and clean up after
 * it.
 */
void
intel_teardown_mchbar(struct inteldrm_softc *dev_priv,
    struct pci_attach_args *bpa, int disable)
{
	struct drm_device	*dev = dev_priv->dev;
	u_int64_t		 mchbar_addr;
	pcireg_t		 tmp, low, high = 0;
	int			 reg;

	reg = INTEL_INFO(dev)->gen >= 4 ? MCHBAR_I965 : MCHBAR_I915;

	switch(disable) {
	case 2:
		if (INTEL_INFO(dev)->gen >= 4)
			high = pci_conf_read(bpa->pa_pc, bpa->pa_tag, reg + 4);
		low = pci_conf_read(bpa->pa_pc, bpa->pa_tag, reg);
		mchbar_addr = ((u_int64_t)high << 32) | low;
		if (bpa->pa_memex)
			extent_free(bpa->pa_memex, mchbar_addr, MCHBAR_SIZE, 0);
		/* FALLTHROUGH */
	case 1:
		if (IS_I915G(dev) || IS_I915GM(dev)) {
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

int
i915_load_modeset_init(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	int ret;

	ret = intel_parse_bios(dev);
	if (ret)
		DRM_INFO("failed to find VBIOS tables\n");

#if 0
	intel_register_dsm_handler();
#endif

	/* IIR "flip pending" bit means done if this bit is set */
	if (IS_GEN3(dev) && (I915_READ(ECOSKPD) & ECO_FLIP_DONE))
		dev_priv->flip_pending_is_done = true;

#ifdef notyet
	ret = vga_switcheroo_register_client(dev->pdev, &i915_switcheroo_ops);
	if (ret)
		goto cleanup_vga_client;

	/* Initialise stolen first so that we may reserve preallocated
	 * objects for the BIOS to KMS transition.
	 */
	ret = i915_gem_init_stolen(dev);
	if (ret)
		goto cleanup_vga_switcheroo;
#endif
	intel_modeset_init(dev);

	ret = i915_gem_init(dev);
	if (ret)
		goto cleanup_gem_stolen;

	intel_modeset_gem_init(dev);

	ret = drm_irq_install(dev);
	if (ret)
		goto cleanup_gem;

	/* Always safe in the mode setting case. */
	/* FIXME: do pre/post-mode set stuff in core KMS code */
	dev->vblank_disable_allowed = 1;

	ret = intel_fbdev_init(dev);
	if (ret)
		goto cleanup_irq;

	drm_kms_helper_poll_init(dev);

	/* We're off and running w/KMS */
	dev_priv->mm.suspended = 0;

	return (0);

cleanup_irq:
	drm_irq_uninstall(dev);
cleanup_gem:
	mutex_lock(&dev->struct_mutex);
	i915_gem_cleanup_ringbuffer(dev);
	mutex_unlock(&dev->struct_mutex);
	i915_gem_cleanup_aliasing_ppgtt(dev);
cleanup_gem_stolen:
#ifdef notyet
	i915_gem_cleanup_stolen(dev);
#endif
	return (ret);
}

void
i915_driver_lastclose(struct drm_device *dev)
{
	int			 ret;

	if (drm_core_check_feature(dev, DRIVER_MODESET)) {
		intel_fb_restore_mode(dev);
		return;
	}

	ret = i915_gem_idle(dev);
	if (ret)
		DRM_ERROR("failed to idle hardware: %d\n", ret);
}

int
i915_driver_open(struct drm_device *dev, struct drm_file *file)
{
	struct drm_i915_file_private *file_priv;

	file_priv = kmalloc(sizeof(*file_priv), GFP_KERNEL);
	if (!file_priv)
		return ENOMEM;

	file->driver_priv = file_priv;

	mtx_init(&file_priv->mm.lock, IPL_NONE);
	INIT_LIST_HEAD(&file_priv->mm.request_list);

	SPLAY_INIT(&file_priv->ctx_tree);

	return 0;
}

void
i915_driver_close(struct drm_device *dev, struct drm_file *file)
{
	struct drm_i915_file_private *file_priv = file->driver_priv;

	mutex_lock(&dev->struct_mutex);
	i915_gem_context_close(dev, file);
	i915_gem_release(dev, file);
	mutex_unlock(&dev->struct_mutex);
	kfree(file_priv);
}
