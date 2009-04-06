/*
 * Copyright 2003 Tungsten Graphics, Inc., Cedar Park, Texas.
 * Copyright (C) The Weather Channel, Inc.  2002.  All Rights Reserved.
 *
 * The Weather Channel (TM) funded Tungsten Graphics to develop the
 * initial release of the Radeon 8500 driver under the XFree86 license.
 * This notice must be preserved.
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
 * PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Keith Whitwell <keith@tungstengraphics.com>
 */

#include "drmP.h"
#include "drm.h"

struct drm_mem *drm_split_block(struct drm_heap *, struct drm_mem *, int,
		    int, struct drm_file *);
struct drm_mem *drm_find_block(struct drm_heap *, int);
void		drm_free_block(struct drm_heap *, struct drm_mem *);
/*
 * Very simple allocator for GART memory, working on a static range
 * already mapped into each client's address space.
 */
struct drm_mem *
drm_split_block(struct drm_heap *heap, struct drm_mem *p, int start,
    int size, struct drm_file *file_priv)
{
	/* Maybe cut off the start of an existing block */
	if (start > p->start) {
		struct drm_mem *newblock = drm_alloc(sizeof(*newblock));
		if (newblock == NULL)
			goto out;
		newblock->start = start;
		newblock->size = p->size - (start - p->start);
		newblock->file_priv = NULL;
		TAILQ_INSERT_AFTER(heap, p, newblock, link);
		p->size -= newblock->size;
		p = newblock;
	}

	/* Maybe cut off the end of an existing block */
	if (size < p->size) {
		struct drm_mem *newblock = drm_alloc(sizeof(*newblock));
		if (newblock == NULL)
			goto out;
		newblock->start = start + size;
		newblock->size = p->size - size;
		newblock->file_priv = NULL;
		TAILQ_INSERT_AFTER(heap, p, newblock, link);
		p->size = size;
	}

      out:
	/* Our block is in the middle */
	p->file_priv = file_priv;
	return p;
}

struct drm_mem *
drm_alloc_block(struct drm_heap *heap, int size, int align2,
    struct drm_file *file_priv)
{
	struct drm_mem *p;
	int mask = (1 << align2) - 1;

	TAILQ_FOREACH(p, heap, link) {
		int start = (p->start + mask) & ~mask;
		if (p->file_priv == NULL && start + size <= p->start + p->size)
			return (drm_split_block(heap, p, start,
			    size, file_priv));
	}

	return NULL;
}

struct drm_mem *
drm_find_block(struct drm_heap *heap, int start)
{
	struct drm_mem *p;

	TAILQ_FOREACH(p, heap, link)
		if (p->start == start)
			return p;

	return NULL;
}

void
drm_free_block(struct drm_heap *heap, struct drm_mem *p)
{
	struct drm_mem	*q;

	p->file_priv = NULL;

	if ((q = TAILQ_NEXT(p, link)) != TAILQ_END(heap) &&
	    q->file_priv == NULL) {
		p->size += q->size;
		TAILQ_REMOVE(heap, q, link);
		drm_free(q);
	}

	if ((q = TAILQ_PREV(p, drm_heap, link)) != TAILQ_END(heap) &&
	    q->file_priv == NULL) {
		q->size += p->size;
		TAILQ_REMOVE(heap, p, link);
		drm_free(p);
	}
}

/*
 * Initialize.
 */
int
drm_init_heap(struct drm_heap *heap, int start, int size)
{
	struct drm_mem *blocks;

	if (!TAILQ_EMPTY(heap))
		return (EBUSY);

	if ((blocks  = drm_alloc(sizeof(*blocks))) == NULL)
		return (ENOMEM);

	blocks->start = start;
	blocks->size = size;
	blocks->file_priv = NULL;
	TAILQ_INSERT_HEAD(heap, blocks, link);

	return (0);
}

/*
 * Free block at offset ``offset'' owned by file_priv.
 */
int
drm_mem_free(struct drm_heap *heap, int offset, struct drm_file *file_priv)
{
	struct drm_mem	*p;

	if ((p = drm_find_block(heap, offset)) == NULL)
		return (EFAULT);
	if (p->file_priv != file_priv)
		return (EPERM);

	drm_free_block(heap, p);
	return (0);
}

/*
 * Free all blocks associated with the releasing file.
 */
void
drm_mem_release(struct drm_heap *heap, struct drm_file *file_priv)
{
	struct drm_mem	*p, *q;

	if (heap == NULL || TAILQ_EMPTY(heap))
		return;

	TAILQ_FOREACH(p, heap, link) {
		if (p->file_priv == file_priv)
			p->file_priv = NULL;
	}

	/* Coalesce the entries.  ugh... */
	for (p = TAILQ_FIRST(heap); p != TAILQ_END(heap); p = q) {
		while (p->file_priv == NULL &&
		    (q = TAILQ_NEXT(p, link)) != TAILQ_END(heap) &&
		    q->file_priv == NULL) {
			p->size += q->size;
			TAILQ_REMOVE(heap, q, link);
			drm_free(q);
		}
		q = TAILQ_NEXT(p, link);
	}
}

/*
 * Shutdown the heap.
 */
void
drm_mem_takedown(struct drm_heap *heap)
{
	struct drm_mem	*p;

	if (heap == NULL)
		return;

	while ((p = TAILQ_FIRST(heap)) != NULL) {
		TAILQ_REMOVE(heap, p, link);
		drm_free(p);
	}
}
