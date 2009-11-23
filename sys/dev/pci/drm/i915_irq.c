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

/*
 * Interrupts that are always left unmasked.
 *
 * Since pipe events are edge-triggered from the PIPESTAT register to IIRC,
 * we leave them always unmasked in IMR and then control enabling them through
 * PIPESTAT alone.
 */
#define I915_INTERRUPT_ENABLE_FIX		\
	(I915_DISPLAY_PIPE_A_EVENT_INTERRUPT |	\
    	I915_DISPLAY_PIPE_B_EVENT_INTERRUPT)	

/* Interrupts that we mask and unmask at runtime */
#define I915_INTERRUPT_ENABLE_VAR	(I915_USER_INTERRUPT)

/* These are all of the interrupts used by the driver */
#define I915_INTERRUPT_ENABLE_MASK	\
	(I915_INTERRUPT_ENABLE_FIX |	\
	I915_INTERRUPT_ENABLE_VAR)

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
inteldrm_intr(void *arg)
{
	struct drm_device	*dev = arg;
	drm_i915_private_t	*dev_priv = dev->dev_private;
	u_int32_t		 iir, pipea_stats = 0, pipeb_stats = 0;

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

	I915_WRITE(IIR, iir);
	(void)I915_READ(IIR); /* Flush posted writes */

	if (dev_priv->sarea_priv != NULL)
		dev_priv->sarea_priv->last_dispatch = READ_BREADCRUMB(dev_priv);

	if (iir & I915_USER_INTERRUPT)
		wakeup(dev_priv);

	mtx_leave(&dev_priv->user_irq_lock);

	if (pipea_stats & I915_VBLANK_INTERRUPT_STATUS)
		drm_handle_vblank(dev, 0);

	if (pipeb_stats & I915_VBLANK_INTERRUPT_STATUS)
		drm_handle_vblank(dev, 1);

	return (1);
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
	struct drm_device	*dev = (struct drm_device *)dev_priv->drmdev;

	if (dev->irq_enabled && (++dev_priv->user_irq_refcount == 1))
		i915_enable_irq(dev_priv, I915_USER_INTERRUPT);
}

void
i915_user_irq_put(struct drm_i915_private *dev_priv)
{
	struct drm_device	*dev = (struct drm_device *)dev_priv->drmdev;

	if (dev->irq_enabled && (--dev_priv->user_irq_refcount == 0))
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

	LOCK_TEST_WITH_RETURN(dev, file_priv);

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

	dev_priv->irqh = pci_intr_establish(dev_priv->pc, dev_priv->ih, IPL_BIO,
	    inteldrm_intr, dev, dev_priv->dev.dv_xname);
	if (dev_priv->irqh == NULL)
		return (ENOENT);

	dev->vblank->vb_max = 0xffffff; /* only 24 bits of frame count */

	/* Unmask the interrupts that we always want on. */
	dev_priv->irq_mask_reg = ~I915_INTERRUPT_ENABLE_FIX;

	dev_priv->pipestat[0] = dev_priv->pipestat[1] = 0;

	/* Disable pipe interrupt enables, clear pending pipe status */
	I915_WRITE(PIPEASTAT, I915_READ(PIPEASTAT) & 0x8000ffff);
	I915_WRITE(PIPEBSTAT, I915_READ(PIPEBSTAT) & 0x8000ffff);
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

	if (!dev_priv)
		return;

	I915_WRITE(HWSTAM, 0xffffffff);
	I915_WRITE(PIPEASTAT, 0);
	I915_WRITE(PIPEBSTAT, 0);
	I915_WRITE(IMR, 0xffffffff);
	I915_WRITE(IER, 0x0);

	I915_WRITE(PIPEASTAT, I915_READ(PIPEASTAT) & 0x8000ffff);
	I915_WRITE(PIPEBSTAT, I915_READ(PIPEBSTAT) & 0x8000ffff);
	I915_WRITE(IIR, I915_READ(IIR));

	pci_intr_disestablish(dev_priv->pc, dev_priv->irqh);
}
