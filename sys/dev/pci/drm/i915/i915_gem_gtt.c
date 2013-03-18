/*	$OpenBSD: i915_gem_gtt.c,v 1.1 2013/03/18 12:36:52 jsg Exp $	*/
/*
 * Copyright © 2010 Daniel Vetter
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
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */

#include <dev/pci/drm/drmP.h>
#include <dev/pci/drm/drm.h>
#include "i915_drv.h"
#include "intel_drv.h"

void
i915_gem_cleanup_aliasing_ppgtt(struct drm_device *dev)
{
	printf("%s stub\n", __func__);
}

void
i915_gem_restore_gtt_mappings(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct drm_i915_gem_object *obj;

	list_for_each_entry(obj, &dev_priv->mm.bound_list, gtt_list) {
		i915_gem_clflush_object(obj);
		i915_gem_gtt_rebind_object(obj, obj->cache_level);
	}

	i915_gem_chipset_flush(dev);
}

void
i915_gem_gtt_rebind_object(struct drm_i915_gem_object *obj,
			   enum i915_cache_level cache_level)
{
	struct drm_device *dev = obj->base.dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	int flags = obj->dma_flags;

	switch (cache_level) {
	case I915_CACHE_NONE:
		flags |= BUS_DMA_GTT_NOCACHE;
		break;
	case I915_CACHE_LLC:
		flags |= BUS_DMA_GTT_CACHE_LLC;
		break;
	case I915_CACHE_LLC_MLC:
		flags |= BUS_DMA_GTT_CACHE_LLC_MLC;
		break;
	default:
		BUG();
	}

	agp_bus_dma_rebind(dev_priv->agpdmat, obj->dmamap, flags);
}
