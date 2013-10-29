/*	$OpenBSD: i915_gem_evict.c,v 1.5 2013/10/29 06:30:57 jsg Exp $	*/
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
/*
 * Copyright © 2008-2010 Intel Corporation
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
 * Authors:
 *    Eric Anholt <eric@anholt.net>
 *    Chris Wilson <chris@chris-wilson.co.uuk>
 *
 */

#include <dev/pci/drm/drmP.h>
#include <dev/pci/drm/drm.h>
#include <dev/pci/drm/i915_drm.h>
#include "i915_drv.h"

#include <machine/pmap.h>

#include <sys/queue.h>
#include <sys/task.h>

struct drm_obj *i915_gem_find_inactive_object(struct inteldrm_softc *, size_t);

struct drm_obj *
i915_gem_find_inactive_object(struct inteldrm_softc *dev_priv,
    size_t min_size)
{
	struct drm_obj		*obj, *best = NULL, *first = NULL;
	struct drm_i915_gem_object *obj_priv;

	/*
	 * We don't need references to the object as long as we hold the list
	 * lock, they won't disappear until we release the lock.
	 */
	list_for_each_entry(obj_priv, &dev_priv->mm.inactive_list, mm_list) {
		obj = &obj_priv->base;
		if (obj_priv->pin_count)
			continue;
		if (obj->size >= min_size) {
			if ((!obj_priv->dirty ||
			    i915_gem_object_is_purgeable(obj_priv)) &&
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
	return best;
}

int
i915_gem_evict_something(struct inteldrm_softc *dev_priv, size_t min_size)
{
	struct drm_device *dev = (struct drm_device *)dev_priv->drmdev;
	struct drm_obj		*obj;
	struct drm_i915_gem_request *request;
	struct drm_i915_gem_object *obj_priv;
	struct intel_ring_buffer *ring;
	u_int32_t		 seqno;
	int			 ret = 0, i;
	int			 found;

	for (;;) {
		i915_gem_retire_requests(dev);

		/* If there's an inactive buffer available now, grab it
		 * and be done.
		 */
		obj = i915_gem_find_inactive_object(dev_priv, min_size);
		if (obj != NULL) {
			obj_priv = to_intel_bo(obj);
			/* find inactive object returns the object with a
			 * reference for us, and held
			 */
			KASSERT(obj_priv->pin_count == 0);
			KASSERT(!obj_priv->active);
			DRM_ASSERT_HELD(obj);

			/* Wait on the rendering and unbind the buffer. */
			ret = i915_gem_object_unbind(obj_priv);
			drm_unhold_and_unref(obj);
			return ret;
		}

		/* If we didn't get anything, but the ring is still processing
		 * things, wait for one of those things to finish and hopefully
		 * leave us a buffer to evict.
		 */
		found = 0;
		for_each_ring(ring, dev_priv, i) {
			if (!list_empty(&ring->request_list)) {
				request = list_first_entry(&ring->request_list,
				    struct drm_i915_gem_request, list);

				seqno = request->seqno;

				ret = i915_wait_seqno(request->ring, seqno);
				if (ret)
					return ret;

				found = 1;
				break;
			}
		}
		if (found)
			continue;

		/*
		 * If we didn't do any of the above, there's no single buffer
		 * large enough to swap out for the new one, so just evict
		 * everything and start again. (This should be rare.)
		 */
		if (!list_empty(&dev_priv->mm.inactive_list))
			return i915_gem_evict_inactive(dev_priv);
		else
			return i915_gem_evict_everything(dev);
	}
	/* NOTREACHED */
}

int
i915_gem_evict_everything(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	int ret;

	if (list_empty(&dev_priv->mm.inactive_list) &&
	    list_empty(&dev_priv->mm.active_list))
		return -ENOSPC;

	/* The gpu_idle will flush everything in the write domain to the
	 * active list. Then we must move everything off the active list
	 * with retire requests.
	 */
	ret = i915_gpu_idle(dev);
	if (ret)
		return ret;

	i915_gem_retire_requests(dev);

	i915_gem_evict_inactive(dev_priv);

	/*
	 * All lists should be empty because we flushed the whole queue, then
	 * we evicted the whole shebang, only pinned objects are still bound.
	 */
	KASSERT(list_empty(&dev_priv->mm.inactive_list));
	KASSERT(list_empty(&dev_priv->mm.active_list));

	return 0;
}

/* Clear out the inactive list and unbind everything in it. */
int
i915_gem_evict_inactive(struct inteldrm_softc *dev_priv)
{
	struct drm_i915_gem_object *obj_priv, *next;
	int			 ret = 0;

	list_for_each_entry_safe(obj_priv, next,
				 &dev_priv->mm.inactive_list, mm_list) {
		if (obj_priv->pin_count != 0) {
			ret = -EINVAL;
			DRM_ERROR("Pinned object in unbind list\n");
			break;
		}
		/* reference it so that we can frob it outside the lock */
		drm_ref(&obj_priv->base.uobj);

		drm_hold_object(&obj_priv->base);
		ret = i915_gem_object_unbind(obj_priv);
		drm_unhold_and_unref(&obj_priv->base);

		if (ret)
			break;
	}

	return ret;
}
