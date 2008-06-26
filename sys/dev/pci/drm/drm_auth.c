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

struct drm_file	*drm_find_file(struct drm_device *, drm_magic_t);
int		 drm_add_magic(struct drm_device *, struct drm_file *,
		     drm_magic_t);
int		 drm_remove_magic(struct drm_device *, drm_magic_t);

/**
 * Returns the file private associated with the given magic number.
 */
struct drm_file *
drm_find_file(struct drm_device *dev, drm_magic_t magic)
{
	struct drm_magic_entry	*pt;
	struct drm_magic_entry	 key;

	DRM_SPINLOCK_ASSERT(&dev->dev_lock);

	key.magic = magic;
	if ((pt = SPLAY_FIND(drm_magic_tree, &dev->magiclist, &key)) != NULL)
		return (pt->priv);
	return (NULL);
}

/**
 * Inserts the given magic number into the hash table of used magic number
 * lists.
 */
int
drm_add_magic(struct drm_device *dev, struct drm_file *priv, drm_magic_t magic)
{
	struct drm_magic_entry	*entry;

	DRM_DEBUG("%d\n", magic);

	DRM_SPINLOCK_ASSERT(&dev->dev_lock);

	if ((entry = malloc(sizeof(*entry), M_DRM, M_ZERO | M_NOWAIT)) == NULL)
		return (ENOMEM);
	entry->magic = magic;
	entry->priv = priv;
	SPLAY_INSERT(drm_magic_tree, &dev->magiclist, entry);

	return (0);
}

/**
 * Removes the given magic number from the hash table of used magic number
 * lists.
 */
int
drm_remove_magic(struct drm_device *dev, drm_magic_t magic)
{
	struct drm_magic_entry	*pt;
	struct drm_magic_entry	 key;

	DRM_SPINLOCK_ASSERT(&dev->dev_lock);

	DRM_DEBUG("%d\n", magic);

	key.magic = magic;
	if ((pt = SPLAY_FIND(drm_magic_tree, &dev->magiclist, &key)) == NULL)
		return (EINVAL);
	SPLAY_REMOVE(drm_magic_tree, &dev->magiclist, pt);
	free(pt, M_DRM);
	return (0);
}

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
	static drm_magic_t	 sequence = 0;
	drm_auth_t		*auth = data;

	/* Find unique magic */
	if (file_priv->magic) {
		auth->magic = file_priv->magic;
	} else {
		DRM_LOCK();
		do {
			int old = sequence;

			auth->magic = ++old;

			if (!atomic_cmpset_int(&sequence, old, auth->magic))
				continue;
		} while (drm_find_file(dev, auth->magic));

		file_priv->magic = auth->magic;
		drm_add_magic(dev, file_priv, auth->magic);
		DRM_UNLOCK();
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
	drm_auth_t	*auth = data;
	struct drm_file	*priv;

	DRM_DEBUG("%u\n", auth->magic);

	DRM_LOCK();
	priv = drm_find_file(dev, auth->magic);
	if (priv != NULL) {
		priv->authenticated = 1;
		drm_remove_magic(dev, auth->magic);
		DRM_UNLOCK();
		return (0);
	} else {
		DRM_UNLOCK();
		return (EINVAL);
	}
}

int
drm_magic_cmp(struct drm_magic_entry *dme1, struct drm_magic_entry *dme2)
{
	return (dme1->magic - dme2->magic);
}

SPLAY_GENERATE(drm_magic_tree, drm_magic_entry, node, drm_magic_cmp);
