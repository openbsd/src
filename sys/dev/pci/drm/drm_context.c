/* $OpenBSD: drm_context.c,v 1.15 2011/06/02 18:22:00 weerd Exp $ */
/*-
 * Copyright 1999, 2000 Precision Insight, Inc., Cedar Park, Texas.
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

/** @file drm_context.c
 * Implementation of the context management ioctls.
 */

#include "drmP.h"

/* ================================================================
 * Context bitmap support
 */

void
drm_ctxbitmap_free(struct drm_device *dev, int ctx_handle)
{
	if (ctx_handle < 0 || ctx_handle >= DRM_MAX_CTXBITMAP || 
	    dev->ctx_bitmap == NULL) {
		DRM_ERROR("Attempt to free invalid context handle: %d\n",
		   ctx_handle);
		return;
	}

	DRM_LOCK();
	clear_bit(ctx_handle, dev->ctx_bitmap);
	DRM_UNLOCK();
	return;
}

int
drm_ctxbitmap_next(struct drm_device *dev)
{
	int	bit;

	if (dev->ctx_bitmap == NULL)
		return (-1);

	DRM_LOCK();
	bit = find_first_zero_bit(dev->ctx_bitmap, DRM_MAX_CTXBITMAP);
	if (bit >= DRM_MAX_CTXBITMAP) {
		DRM_UNLOCK();
		return (-1);
	}

	set_bit(bit, dev->ctx_bitmap);
	DRM_DEBUG("drm_ctxbitmap_next bit : %d\n", bit);
	DRM_UNLOCK();
	return (bit);
}

int
drm_ctxbitmap_init(struct drm_device *dev)
{
	atomic_t	*bitmap;
	int		 i, temp;

	
	if ((bitmap = drm_calloc(1, PAGE_SIZE)) == NULL)
		return (ENOMEM);
	DRM_LOCK();
	dev->ctx_bitmap = bitmap;
	DRM_UNLOCK();

	for (i = 0; i < DRM_RESERVED_CONTEXTS; i++) {
		temp = drm_ctxbitmap_next(dev);
	   	DRM_DEBUG("drm_ctxbitmap_init : %d\n", temp);
	}

	return (0);
}

void
drm_ctxbitmap_cleanup(struct drm_device *dev)
{
	atomic_t *bitmap;

	DRM_LOCK();
	bitmap = dev->ctx_bitmap;
	dev->ctx_bitmap = NULL;
	DRM_UNLOCK();
	drm_free(bitmap);
}

int
drm_resctx(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_ctx_res	*res = data;
	struct drm_ctx		 ctx;
	int			 i;

	if (res->count >= DRM_RESERVED_CONTEXTS) {
		bzero(&ctx, sizeof(ctx));
		for (i = 0; i < DRM_RESERVED_CONTEXTS; i++) {
			ctx.handle = i;
			if (DRM_COPY_TO_USER(&res->contexts[i],
			    &ctx, sizeof(ctx)))
				return (EFAULT);
		}
	}
	res->count = DRM_RESERVED_CONTEXTS;

	return (0);
}

int
drm_addctx(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_ctx	*ctx = data;

	ctx->handle = drm_ctxbitmap_next(dev);
	if (ctx->handle == DRM_KERNEL_CONTEXT) {
		/* Skip kernel's context and get a new one. */
		ctx->handle = drm_ctxbitmap_next(dev);
	}
	DRM_DEBUG("%d\n", ctx->handle);
	if (ctx->handle == -1) {
		DRM_DEBUG("Not enough free contexts.\n");
		/* Should this return -EBUSY instead? */
		return (ENOMEM);
	}

	return (0);
}

int
drm_getctx(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_ctx	*ctx = data;

	/* This is 0, because we don't handle any context flags */
	ctx->flags = 0;

	return (0);
}

int
drm_rmctx(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_ctx	*ctx = data;

	DRM_DEBUG("%d\n", ctx->handle);
	if (ctx->handle != DRM_KERNEL_CONTEXT)
		drm_ctxbitmap_free(dev, ctx->handle);

	return (0);
}
