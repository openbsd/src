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

/** @file drm_auth.c
 * Implementation of the get/authmagic ioctls implementing the authentication
 * scheme between the master and clients.
 */

#include "drmP.h"

/**
 * Called by the client, this returns a unique magic number to be authorized
 * by the master.
 *
 * The master may use its own knowledge of the client (such as the X
 * connection that the magic is passed over) to determine if the magic number
 * should be authenticated.
 */
int
drm_getmagic(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_auth		*auth = data;
	struct drm_magic_entry	*entry;

	/* Find unique magic */
	if (file_priv->magic) {
		auth->magic = file_priv->magic;
	} else {
		entry = drm_alloc(sizeof(*entry));
		if (entry == NULL)
			return (ENOMEM);
		DRM_LOCK();
		entry->magic = file_priv->magic = auth->magic = dev->magicid++;
		entry->priv = file_priv;
		SPLAY_INSERT(drm_magic_tree, &dev->magiclist, entry);

		DRM_UNLOCK();
		DRM_DEBUG("%d\n", auth->magic);
	}

	DRM_DEBUG("%u\n", auth->magic);

	return (0);
}

/**
 * Marks the client associated with the given magic number as authenticated.
 */
int
drm_authmagic(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_auth		*auth = data;
	struct drm_magic_entry	*pt, key;

	DRM_DEBUG("%u\n", auth->magic);

	key.magic = auth->magic;
	DRM_LOCK();
	if ((pt = SPLAY_FIND(drm_magic_tree, &dev->magiclist, &key)) == NULL ||
	    pt->priv == NULL) {
		DRM_UNLOCK();
		return (EINVAL);
	}

	pt->priv->authenticated = 1;
	SPLAY_REMOVE(drm_magic_tree, &dev->magiclist, pt);
	DRM_UNLOCK();

	drm_free(pt);

	return (0);
}

int
drm_magic_cmp(struct drm_magic_entry *dme1, struct drm_magic_entry *dme2)
{
	return (dme1->magic - dme2->magic);
}

SPLAY_GENERATE(drm_magic_tree, drm_magic_entry, node, drm_magic_cmp);
