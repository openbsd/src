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

#define MAX_NOPID ((u32)~0)
void	i915_enable_irq(drm_i915_private_t *, u_int32_t);
void	i915_disable_irq(drm_i915_private_t *, u_int32_t);
int	i915_wait_irq(struct drm_device *, int);

/** These are the interrupts used by the driver */
#define I915_INTERRUPT_ENABLE_MASK	(I915_USER_INTERRUPT |		\
					I915_DISPLAY_PIPE_A_EVENT_INTERRUPT | \
					I915_DISPLAY_PIPE_B_EVENT_INTERRUPT)

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

/**
 * i915_get_pipe - return the the pipe associated with a given plane
 * @dev: DRM device
 * @plane: plane to look for
 *
 * The Intel Mesa & 2D drivers call the vblank routines with a plane number
 * rather than a pipe number, since they may not always be equal.  This routine
 * maps the given @plane back to a pipe number.
 */
static int
i915_get_pipe(struct drm_device *dev, int plane)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;
	u32 dspcntr;

	dspcntr = plane ? I915_READ(DSPBCNTR) : I915_READ(DSPACNTR);

	return dspcntr & DISPPLANE_SEL_PIPE_MASK ? 1 : 0;
}

/**
 * i915_get_plane - return the the plane associated with a given pipe
 * @dev: DRM device
 * @pipe: pipe to look for
 *
 * The Intel Mesa & 2D drivers call the vblank routines with a plane number
 * rather than a plane number, since they may not always be equal.  This routine
 * maps the given @pipe back to a plane number.
 */
static int
i915_get_plane(struct drm_device *dev, int pipe)
{
	if (i915_get_pipe(dev, 0) == pipe)
		return 0;
	return 1;
}

/**
 * i915_pipe_enabled - check if a pipe is enabled
 * @dev: DRM device
 * @pipe: pipe to check
 *
 * Reading certain registers when the pipe is disabled can hang the chip.
 * Use this routine to make sure the PLL is running and the pipe is active
 * before reading such registers if unsure.
 */
static int
i915_pipe_enabled(struct drm_device *dev, int pipe)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;
	unsigned long pipeconf = pipe ? PIPEBCONF : PIPEACONF;

	if (I915_READ(pipeconf) & PIPEACONF_ENABLE)
		return 1;

	return 0;
}

u32 i915_get_vblank_counter(struct drm_device *dev, int plane)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;
	unsigned long high_frame;
	unsigned long low_frame;
	u32 high1, high2, low, count;
	int pipe;

	pipe = i915_get_pipe(dev, plane);
	high_frame = pipe ? PIPEBFRAMEHIGH : PIPEAFRAMEHIGH;
	low_frame = pipe ? PIPEBFRAMEPIXEL : PIPEAFRAMEPIXEL;

	if (!i915_pipe_enabled(dev, pipe)) {
		DRM_DEBUG("trying to get vblank count for disabled pipe %d\n",
		    pipe);
		return 0;
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

	count = (high1 << 8) | low;

	return count;
}

irqreturn_t i915_driver_irq_handler(DRM_IRQ_ARGS)
{
	struct drm_device *dev = (struct drm_device *) arg;
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;
	u32 iir;
	u32 pipea_stats, pipeb_stats;
	int vblank = 0;

	atomic_inc(&dev_priv->irq_received);
	iir = I915_READ(IIR);
	if (iir == 0)
		return IRQ_NONE;

	/*
	 * Clear the PIPE(A|B)STAT regs before the IIR otherwise
	 * we may get extra interrupts.
	 */
	if (iir & I915_DISPLAY_PIPE_A_EVENT_INTERRUPT) {
		pipea_stats = I915_READ(PIPEASTAT);
		if (pipea_stats & (PIPE_START_VBLANK_INTERRUPT_STATUS|
		    PIPE_VBLANK_INTERRUPT_STATUS)) {
			vblank++;
			drm_handle_vblank(dev, i915_get_plane(dev, 0));
		}

		I915_WRITE(PIPEASTAT, pipea_stats);
	}
	if (iir & I915_DISPLAY_PIPE_B_EVENT_INTERRUPT) {
		pipeb_stats = I915_READ(PIPEBSTAT);
		/* Ack the event */
		I915_WRITE(PIPEBSTAT, pipeb_stats);

		if (pipeb_stats & (PIPE_START_VBLANK_INTERRUPT_STATUS|
		    PIPE_VBLANK_INTERRUPT_STATUS)) {
			vblank++;
			drm_handle_vblank(dev, i915_get_plane(dev, 1));
		}

		I915_WRITE(PIPEBSTAT, pipeb_stats);
	}

	I915_WRITE(IIR, iir);
	(void) I915_READ(IIR); /* Flush posted writes */

	if (dev_priv->sarea_priv)
		dev_priv->sarea_priv->last_dispatch = READ_BREADCRUMB(dev_priv);

	if (iir & I915_USER_INTERRUPT) {
		DRM_WAKEUP(&dev_priv->irq_queue);
	}

	return IRQ_HANDLED;
}

int i915_emit_irq(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	RING_LOCALS;

	i915_kernel_lost_context(dev);

	DRM_DEBUG("\n");

	i915_emit_breadcrumb(dev);

	BEGIN_LP_RING(2);
	OUT_RING(0);
	OUT_RING(MI_USER_INTERRUPT);
	ADVANCE_LP_RING();

	return dev_priv->counter;
}

void i915_user_irq_get(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *)dev->dev_private;

	mtx_enter(&dev_priv->user_irq_lock);
	if (dev->irq_enabled && (++dev_priv->user_irq_refcount == 1))
		i915_enable_irq(dev_priv, I915_USER_INTERRUPT);
	mtx_leave(&dev_priv->user_irq_lock);
}

void i915_user_irq_put(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *)dev->dev_private;

	mtx_enter(&dev_priv->user_irq_lock);
	if (dev->irq_enabled && (--dev_priv->user_irq_refcount == 0))
		i915_disable_irq(dev_priv, I915_USER_INTERRUPT);
	mtx_leave(&dev_priv->user_irq_lock);
}


int i915_wait_irq(struct drm_device * dev, int irq_nr)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;
	int ret = 0;

	DRM_DEBUG("irq_nr=%d breadcrumb=%d\n", irq_nr,
		  READ_BREADCRUMB(dev_priv));

	if (READ_BREADCRUMB(dev_priv) >= irq_nr) {
		if (dev_priv->sarea_priv) {
			dev_priv->sarea_priv->last_dispatch =
			    READ_BREADCRUMB(dev_priv);
		}
		return 0;
	}

	i915_user_irq_get(dev);
	DRM_WAIT_ON(ret, dev_priv->irq_queue, 3 * DRM_HZ,
		    READ_BREADCRUMB(dev_priv) >= irq_nr);
	i915_user_irq_put(dev);

	if (ret == EBUSY) {
		DRM_ERROR("EBUSY -- rec: %d emitted: %d\n",
			  READ_BREADCRUMB(dev_priv), (int)dev_priv->counter);
	}

	if (dev_priv->sarea_priv)
		dev_priv->sarea_priv->last_dispatch =
			READ_BREADCRUMB(dev_priv);
	return ret;
}

/* Needs the lock as it touches the ring.
 */
int i915_irq_emit(struct drm_device *dev, void *data,
			 struct drm_file *file_priv)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	drm_i915_irq_emit_t *emit = data;
	int result;

	LOCK_TEST_WITH_RETURN(dev, file_priv);

	if (!dev_priv) {
		DRM_ERROR("called with no initialization\n");
		return EINVAL;
	}

	DRM_LOCK();
	result = i915_emit_irq(dev);
	DRM_UNLOCK();

	if (DRM_COPY_TO_USER(emit->irq_seq, &result, sizeof(int))) {
		DRM_ERROR("copy_to_user\n");
		return EFAULT;
	}

	return 0;
}

/* Doesn't need the hardware lock.
 */
int i915_irq_wait(struct drm_device *dev, void *data,
		  struct drm_file *file_priv)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	drm_i915_irq_wait_t *irqwait = data;

	if (!dev_priv) {
		DRM_ERROR("called with no initialization\n");
		return EINVAL;
	}

	return i915_wait_irq(dev, irqwait->irq_seq);
}

int i915_enable_vblank(struct drm_device *dev, int plane)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;
	int pipe = i915_get_pipe(dev, plane);
	u32	pipestat_reg = 0;
	u32	pipestat;
	u32	interrupt = 0;

	switch (pipe) {
	case 0:
		pipestat_reg = PIPEASTAT;
		interrupt = I915_DISPLAY_PIPE_A_EVENT_INTERRUPT;
		break;
	case 1:
		pipestat_reg = PIPEBSTAT;
		interrupt = I915_DISPLAY_PIPE_B_EVENT_INTERRUPT;
		break;
	default:
		DRM_ERROR("tried to enable vblank on non-existent pipe %d\n",
			  pipe);
		return (0);
	}

	mtx_enter(&dev_priv->user_irq_lock);
	/*
	 * Enabling vblank events in IMR comes before PIPESTAT write, or
	 * there's a race where the PIPESTAT vblank bit gets set to 1, so
	 * the OR of enabled PIPESTAT bits goes to 1, so the PIPExEVENT in
	 * ISR flashes to 1, but the IIR bit doesn't get set to 1 because
	 * IMR masks it.  It doesn't ever get set after we clear the masking
	 * in IMR because the ISR bit is edge, not level-triggered, on the
	 * OR of PIPESTAT bits.
	 */
	i915_enable_irq(dev_priv, interrupt);
	pipestat = I915_READ(pipestat_reg);
	if (IS_I965G(dev))
		pipestat |= PIPE_START_VBLANK_INTERRUPT_ENABLE;
	else
		pipestat |= PIPE_VBLANK_INTERRUPT_ENABLE;
	/* Clear any stale interrupt status */
	pipestat |= (PIPE_START_VBLANK_INTERRUPT_STATUS |
	    PIPE_VBLANK_INTERRUPT_STATUS);
	I915_WRITE(pipestat_reg, pipestat);
	(void)I915_READ(pipestat_reg); /* Posting read */
	mtx_leave(&dev_priv->user_irq_lock);

	return 0;
}

void i915_disable_vblank(struct drm_device *dev, int plane)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;
	int pipe = i915_get_pipe(dev, plane);
	u32	pipestat_reg = 0;
	u32	pipestat;
	u32	interrupt = 0;

	switch (pipe) {
	case 0:
		pipestat_reg = PIPEASTAT;
		interrupt = I915_DISPLAY_PIPE_A_EVENT_INTERRUPT;
		break;
	case 1:
		pipestat_reg = PIPEBSTAT;
		interrupt = I915_DISPLAY_PIPE_B_EVENT_INTERRUPT;
		break;
	default:
		DRM_ERROR("tried to disable vblank on non-existent pipe %d\n",
			  pipe);
		return;
	}

	mtx_enter(&dev_priv->user_irq_lock);
	i915_disable_irq(dev_priv, interrupt);
	pipestat = I915_READ(pipestat_reg);
	pipestat &= ~(PIPE_START_VBLANK_INTERRUPT_ENABLE |
	    PIPE_VBLANK_INTERRUPT_ENABLE);
	/* Clear any stale interrupt status */
	pipestat |= (PIPE_START_VBLANK_INTERRUPT_STATUS |
	    PIPE_VBLANK_INTERRUPT_STATUS);
	I915_WRITE(pipestat_reg, pipestat);
	(void)I915_READ(pipestat_reg);
	mtx_leave(&dev_priv->user_irq_lock);
}

int i915_vblank_pipe_get(struct drm_device *dev, void *data,
			 struct drm_file *file_priv)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	drm_i915_vblank_pipe_t *pipe = data;

	if (!dev_priv) {
		DRM_ERROR("called with no initialization\n");
		return EINVAL;
	}

	pipe->pipe = DRM_I915_VBLANK_PIPE_A | DRM_I915_VBLANK_PIPE_B;

	return 0;
}

/*
 * Schedule buffer swap at given vertical blank.
 */
int i915_vblank_swap(struct drm_device *dev, void *data,
		     struct drm_file *file_priv)
{
	/* The delayed swap mechanism was fundamentally racy, and has been
	 * removed.  The model was that the client requested a delayed flip/swap
	 * from the kernel, then waited for vblank before continuing to perform
	 * rendering.  The problem was that the kernel might wake the client
	 * up before it dispatched the vblank swap (since the lock has to be
	 * held while touching the ringbuffer), in which case the client would
	 * clear and start the next frame before the swap occurred, and   
	 * flicker would occur in addition to likely missing the vblank.
	 *
	 * In the absence of this ioctl, userland falls back to a correct path
	 * of waiting for a vblank, then dispatching the swap on its own.
	 * Context switching to userland and back is plenty fast enough for
	 * meeting the requirements of vblank swapping.
	 */
	return (EINVAL);
}

/* drm_dma.h hooks
*/
void i915_driver_irq_preinstall(struct drm_device * dev)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;

	I915_WRITE(HWSTAM, 0xeffe);
	I915_WRITE(IMR, 0xffffffff);
	I915_WRITE(IER, 0x0);
}

int i915_driver_irq_postinstall(struct drm_device * dev)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;
	int ret, num_pipes = 2;

	dev_priv->irq_mask_reg = ~0;

	ret = drm_vblank_init(dev, num_pipes);
	if (ret)
		return ret;

	dev->max_vblank_count = 0xffffff; /* only 24 bits of frame count */

	I915_WRITE(IMR, dev_priv->irq_mask_reg);
	I915_WRITE(IER, I915_INTERRUPT_ENABLE_MASK);
	(void)I915_READ(IER);

	DRM_INIT_WAITQUEUE(&dev_priv->irq_queue);

	return 0;
}

void i915_driver_irq_uninstall(struct drm_device * dev)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;
	u32 temp;

	if (!dev_priv)
		return;

	I915_WRITE(HWSTAM, 0xffffffff);
	I915_WRITE(IMR, 0xffffffff);
	I915_WRITE(IER, 0x0);

	temp = I915_READ(PIPEASTAT);
	I915_WRITE(PIPEASTAT, temp);
	temp = I915_READ(PIPEBSTAT);
	I915_WRITE(PIPEBSTAT, temp);
	temp = I915_READ(IIR);
	I915_WRITE(IIR, temp);
}
