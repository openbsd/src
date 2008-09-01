/*-
 * Copyright 1999 Precision Insight, Inc., Cedar Park, Texas.
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
 *    Rickard E. (Rik) Faith <faith@valinux.com>
 *    Gareth Hughes <gareth@valinux.com>
 *
 */

/** @file drm_drawable.c
 * This file implements ioctls to store information along with DRM drawables,
 * such as the current set of cliprects for vblank-synced buffer swaps.
 */

#include "drmP.h"

struct bsd_drm_drawable_info;

int	drm_drawable_compare(struct bsd_drm_drawable_info *,
	    struct bsd_drm_drawable_info *);
void	drm_drawable_free(struct drm_device *dev,
	    struct bsd_drm_drawable_info *draw);
struct bsd_drm_drawable_info *
	drm_get_drawable(struct drm_device *, unsigned int);

RB_PROTOTYPE(drawable_tree, bsd_drm_drawable_info, tree,
    drm_drawable_compare);

struct bsd_drm_drawable_info {
	struct drm_drawable_info	info;
	unsigned int			handle;
	RB_ENTRY(bsd_drm_drawable_info)	tree;
};

int
drm_drawable_compare(struct bsd_drm_drawable_info *a,
    struct bsd_drm_drawable_info *b)
{
	return (a->handle - b->handle);
}

RB_GENERATE(drawable_tree, bsd_drm_drawable_info, tree,
    drm_drawable_compare);

struct bsd_drm_drawable_info *
drm_get_drawable(struct drm_device *dev, unsigned int handle)
{
	struct bsd_drm_drawable_info find;

	find.handle = handle;
	return (RB_FIND(drawable_tree, &dev->drw_head, &find));
}

struct drm_drawable_info *
drm_get_drawable_info(struct drm_device *dev, unsigned int handle)
{
	struct bsd_drm_drawable_info *result = NULL;

	if ((result = drm_get_drawable(dev, handle)))
		return (&result->info);

	return (NULL);
}

int
drm_adddraw(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	drm_draw_t *draw = data;
	struct bsd_drm_drawable_info *info;

	info = drm_calloc(1, sizeof(struct bsd_drm_drawable_info),
	    DRM_MEM_DRAWABLE);
	if (info == NULL)
		return (ENOMEM);

	info->handle = ++dev->drw_no;
	DRM_SPINLOCK(&dev->drw_lock);
	RB_INSERT(drawable_tree, &dev->drw_head, info);
	draw->handle = info->handle;
	DRM_SPINUNLOCK(&dev->drw_lock);

	DRM_DEBUG("%d\n", draw->handle);

	return (0);
}

int
drm_rmdraw(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	drm_draw_t *draw = (drm_draw_t *)data;
	struct bsd_drm_drawable_info *info;

	DRM_SPINLOCK(&dev->drw_lock);
	info = drm_get_drawable(dev, draw->handle);
	if (info != NULL) {
		drm_drawable_free(dev, info);
		DRM_SPINUNLOCK(&dev->drw_lock);
		return (0);
	} else {
		DRM_SPINUNLOCK(&dev->drw_lock);
		return (EINVAL);
	}
}

int
drm_update_draw(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_drawable_info *info;
	struct drm_update_draw *update = (struct drm_update_draw *)data;
	int ret = 0;

	DRM_SPINLOCK(&dev->drw_lock);
	info = drm_get_drawable_info(dev, update->handle);
	if (info == NULL) {
		ret = EINVAL;
		goto out;
	}

	switch (update->type) {
	case DRM_DRAWABLE_CLIPRECTS:
		if (update->num != info->num_rects) {
			struct drm_clip_rect  *free = info->rects;
			size_t no = info->num_rects;

			info->rects = NULL;
			info->num_rects = 0;
			DRM_SPINUNLOCK(&dev->drw_lock);
			drm_free(free, sizeof(*info->rects) * no,
			    DRM_MEM_DRAWABLE);
			DRM_SPINLOCK(&dev->drw_lock);
		}
		if (update->num == 0)
			goto out;

		if (info->rects == NULL) {
			struct drm_clip_rect *rects;

			DRM_SPINUNLOCK(&dev->drw_lock);
			rects = drm_calloc(update->num, sizeof(*info->rects),
			    DRM_MEM_DRAWABLE);
			DRM_SPINLOCK(&dev->drw_lock);
			if (rects == NULL) {
				ret = ENOMEM;
				goto out;
			}

			info->rects = rects;
			info->num_rects = update->num;
		}
		/* For some reason the pointer arg is unsigned long long. */
		ret = copyin((void *)(intptr_t)update->data, info->rects,
		    sizeof(*info->rects) * info->num_rects);
		break;
	default:
		ret =  EINVAL;
	}

out:
	DRM_SPINUNLOCK(&dev->drw_lock);
	return (ret);
}

void
drm_drawable_free(struct drm_device *dev, struct bsd_drm_drawable_info *draw)
{
	if (draw == NULL)
		return;
	RB_REMOVE(drawable_tree, &dev->drw_head, draw);
	DRM_SPINUNLOCK(&dev->drw_lock);
	drm_free(draw->info.rects, sizeof(*draw->info.rects) *
	    draw->info.num_rects, DRM_MEM_DRAWABLE);
	drm_free(draw, sizeof(*draw), DRM_MEM_DRAWABLE);
	DRM_SPINLOCK(&dev->drw_lock);
}

void
drm_drawable_free_all(struct drm_device *dev)
{
	struct bsd_drm_drawable_info *draw, *nxt;

	DRM_SPINLOCK(&dev->drw_lock);
	for (draw = RB_MIN(drawable_tree, &dev->drw_head); draw != NULL;
	    draw = nxt) {
		nxt = RB_NEXT(drawable_tree, &dev->drw_head, draw);
		drm_drawable_free(dev, draw);
	}
	DRM_SPINUNLOCK(&dev->drw_lock);
}
