/* i915_irq.c -- IRQ support for the I915 -*- linux-c -*-
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

int	inteldrm_intr(void *);
void	i915_enable_irq(drm_i915_private_t *, u_int32_t);
void	i915_disable_irq(drm_i915_private_t *, u_int32_t);
void	i915_enable_pipestat(drm_i915_private_t *, int, u_int32_t);
void	i915_disable_pipestat(drm_i915_private_t *, int, u_int32_t);
int	i915_wait_irq(struct drm_device *, int);

inline void
i915_enable_irq(drm_i915_private_t *dev_priv, u_int32_t mask)
{
	if ((dev_priv->irq_mask_reg & mask) != 0) {
		dev_priv->irq_mask_reg &= ~mask;
		I915_WRITE(IMR, dev_priv->irq_mask_reg);
		(void)I915_READ(IMR);
	}
}

inline void
i915_disable_irq(drm_i915_private_t *dev_priv, u_int32_t mask)
{
	if ((dev_priv->irq_mask_reg & mask) != mask) {
		dev_priv->irq_mask_reg |= mask;
		I915_WRITE(IMR, dev_priv->irq_mask_reg);
		(void)I915_READ(IMR);
	}
}

void
i915_enable_pipestat(drm_i915_private_t *dev_priv, int pipe, u_int32_t mask)
{
	if ((dev_priv->pipestat[pipe] & mask) != mask) {
		bus_size_t reg = pipe == 0 ? PIPEASTAT : PIPEBSTAT;

		dev_priv->pipestat[pipe] |= mask;
		/* Enable the interrupt, clear and pending status */
		I915_WRITE(reg, dev_priv->pipestat[pipe] | (mask >> 16));
		(void)I915_READ(reg);
	}
}

void
i915_disable_pipestat(drm_i915_private_t *dev_priv, int pipe, u_int32_t mask)
{
	if ((dev_priv->pipestat[pipe] & mask) != 0) {
		bus_size_t reg = pipe == 0 ? PIPEASTAT : PIPEBSTAT;

		dev_priv->pipestat[pipe] &= ~mask;
		I915_WRITE(reg, dev_priv->pipestat[pipe]);
		(void)I915_READ(reg);
	}
}

u_int32_t
i915_get_vblank_counter(struct drm_device *dev, int pipe)
{
	drm_i915_private_t	*dev_priv = dev->dev_private;
	bus_size_t		 high_frame, low_frame;
	u_int32_t		 high1, high2, low;

	high_frame = pipe ? PIPEBFRAMEHIGH : PIPEAFRAMEHIGH;
	low_frame = pipe ? PIPEBFRAMEPIXEL : PIPEAFRAMEPIXEL;

	if (inteldrm_pipe_enabled(dev_priv, pipe) == 0) {
		DRM_DEBUG("trying to get vblank count for disabled pipe %d\n",
		    pipe);
		return (0);
	}

	/* GM45 just had to be different... */
	if (IS_GM45(dev_priv) || IS_G4X(dev_priv)) {
		return (I915_READ(pipe ? PIPEB_FRMCOUNT_GM45 :
		    PIPEA_FRMCOUNT_GM45));
	}

	/*
	 * High & low register fields aren't synchronized, so make sure
	 * we get a low value that's stable across two reads of the high
	 * register.
	 */
	do {
		high1 = ((I915_READ(high_frame) & PIPE_FRAME_HIGH_MASK) >>
			 PIPE_FRAME_HIGH_SHIFT);
		low =  ((I915_READ(low_frame) & PIPE_FRAME_LOW_MASK) >>
			PIPE_FRAME_LOW_SHIFT);
		high2 = ((I915_READ(high_frame) & PIPE_FRAME_HIGH_MASK) >>
			 PIPE_FRAME_HIGH_SHIFT);
	} while (high1 != high2);

	return ((high1 << 8) | low);
}

int
i915_emit_irq(struct drm_device *dev)
{
	drm_i915_private_t	*dev_priv = dev->dev_private;

	inteldrm_update_ring(dev_priv);

	DRM_DEBUG("\n");

	i915_emit_breadcrumb(dev);

	BEGIN_LP_RING(2);
	OUT_RING(0);
	OUT_RING(MI_USER_INTERRUPT);
	ADVANCE_LP_RING();

	return (dev_priv->counter);
}

void
i915_user_irq_get(struct drm_i915_private *dev_priv)
{
	if (++dev_priv->user_irq_refcount == 1)
		i915_enable_irq(dev_priv, I915_USER_INTERRUPT);
}

void
i915_user_irq_put(struct drm_i915_private *dev_priv)
{
	if (--dev_priv->user_irq_refcount == 0)
		i915_disable_irq(dev_priv, I915_USER_INTERRUPT);
}


int
i915_wait_irq(struct drm_device *dev, int irq_nr)
{
	drm_i915_private_t	*dev_priv =  dev->dev_private;
	int			 ret = 0;

	DRM_DEBUG("irq_nr=%d breadcrumb=%d\n", irq_nr,
		  READ_BREADCRUMB(dev_priv));

	mtx_enter(&dev_priv->user_irq_lock);
	i915_user_irq_get(dev_priv);
	while (ret == 0) {
		if (READ_BREADCRUMB(dev_priv) >= irq_nr)
			break;
		ret = msleep(dev_priv, &dev_priv->user_irq_lock,
		    PZERO | PCATCH, "i915wt", 3 * hz);
	}
	i915_user_irq_put(dev_priv);
	mtx_leave(&dev_priv->user_irq_lock);

	if (dev_priv->sarea_priv != NULL)
		dev_priv->sarea_priv->last_dispatch = READ_BREADCRUMB(dev_priv);
	return (ret);
}

/* Needs the lock as it touches the ring.
 */
int
i915_irq_emit(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	drm_i915_private_t	*dev_priv = dev->dev_private;
	drm_i915_irq_emit_t	*emit = data;
	int			 result;

	RING_LOCK_TEST_WITH_RETURN(dev, file_priv);

	if (!dev_priv) {
		DRM_ERROR("called with no initialization\n");
		return EINVAL;
	}

	DRM_LOCK();
	result = i915_emit_irq(dev);
	DRM_UNLOCK();

	return (copyout(&result, emit->irq_seq, sizeof(result)));
}

/* Doesn't need the hardware lock.
 */
int
i915_irq_wait(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	drm_i915_private_t	*dev_priv = dev->dev_private;
	drm_i915_irq_wait_t	*irqwait = data;

	if (!dev_priv) {
		DRM_ERROR("called with no initialization\n");
		return (EINVAL);
	}

	return (i915_wait_irq(dev, irqwait->irq_seq));
}

int
i915_enable_vblank(struct drm_device *dev, int pipe)
{
	drm_i915_private_t	*dev_priv = dev->dev_private;

	if (inteldrm_pipe_enabled(dev_priv, pipe) == 0)
		return (EINVAL);

	mtx_enter(&dev_priv->user_irq_lock);
	i915_enable_pipestat(dev_priv, pipe, (IS_I965G(dev_priv) ? 
	    PIPE_START_VBLANK_INTERRUPT_ENABLE : PIPE_VBLANK_INTERRUPT_ENABLE));
	mtx_leave(&dev_priv->user_irq_lock);

	return (0);
}

void
i915_disable_vblank(struct drm_device *dev, int pipe)
{
	drm_i915_private_t	*dev_priv = dev->dev_private;

	mtx_enter(&dev_priv->user_irq_lock);
	i915_disable_pipestat(dev_priv, pipe, 
	    PIPE_START_VBLANK_INTERRUPT_ENABLE | PIPE_VBLANK_INTERRUPT_ENABLE);
	mtx_leave(&dev_priv->user_irq_lock);
}

int
i915_vblank_pipe_get(struct drm_device *dev, void *data,
    struct drm_file *file_priv)
{
	drm_i915_private_t	*dev_priv = dev->dev_private;
	drm_i915_vblank_pipe_t	*pipe = data;

	if (!dev_priv) {
		DRM_ERROR("called with no initialization\n");
		return (EINVAL);
	}

	pipe->pipe = DRM_I915_VBLANK_PIPE_A | DRM_I915_VBLANK_PIPE_B;

	return (0);
}

/* drm_dma.h hooks
*/
int
i915_driver_irq_install(struct drm_device *dev)
{
	drm_i915_private_t	*dev_priv = dev->dev_private;

	I915_WRITE(HWSTAM, 0xeffe);
	I915_WRITE(PIPEASTAT, 0);
	I915_WRITE(PIPEBSTAT, 0);
	I915_WRITE(IMR, 0xffffffff);
	I915_WRITE(IER, 0x0);
	(void)I915_READ(IER);

	dev->vblank->vb_max = 0xffffff; /* only 24 bits of frame count */
	if (IS_G4X(dev_priv))
		dev->vblank->vb_max = 0xffffffff;

#ifdef INTELDRM_GEM
	/*
	 * Enable some error detection, note the instruction error mask
	 * bit is reserved, so we leave it masked.
	 */
	I915_WRITE(EMR, IS_G4X(dev_priv) ? 
	    ~(GM45_ERROR_PAGE_TABLE | GM45_ERROR_MEM_PRIV |
	    GM45_ERROR_CP_PRIV | I915_ERROR_MEMORY_REFRESH) :
	    ~(I915_ERROR_PAGE_TABLE | I915_ERROR_MEMORY_REFRESH));
#endif /* INTELDRM_GEM */

	/*
	 * Disable pipe interrupt enables, clear pending pipe status
	 * add back in the enabled interrupts from previous iterations
	 * (say in the reset case where we want vblank interrupts etc to be
	 * switched back on if they were running
	 */
	I915_WRITE(PIPEASTAT, (I915_READ(PIPEASTAT) & 0x8000ffff) |
	    dev_priv->pipestat[0]);
	I915_WRITE(PIPEBSTAT, (I915_READ(PIPEBSTAT) & 0x8000ffff) |
	    dev_priv->pipestat[1]);
	/* Clear pending interrupt status */
	I915_WRITE(IIR, I915_READ(IIR));

	I915_WRITE(IER, I915_INTERRUPT_ENABLE_MASK);
	I915_WRITE(IMR, dev_priv->irq_mask_reg);
	(void)I915_READ(IER);

	return (0);
}

void
i915_driver_irq_uninstall(struct drm_device *dev)
{
	drm_i915_private_t	*dev_priv = dev->dev_private;

	I915_WRITE(HWSTAM, 0xffffffff);
	I915_WRITE(PIPEASTAT, 0);
	I915_WRITE(PIPEBSTAT, 0);
	I915_WRITE(IMR, 0xffffffff);
	I915_WRITE(IER, 0x0);

	I915_WRITE(PIPEASTAT, I915_READ(PIPEASTAT) & 0x8000ffff);
	I915_WRITE(PIPEBSTAT, I915_READ(PIPEBSTAT) & 0x8000ffff);
	I915_WRITE(IIR, I915_READ(IIR));
}
