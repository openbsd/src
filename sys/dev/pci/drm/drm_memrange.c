/**************************************************************************
 *
 * Copyright 2006 Tungsten Graphics, Inc., Bismarck, ND., USA.
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 *
 **************************************************************************/

/*
 * Generic simple memory manager implementation. Intended to be used as a base
 * class implementation for more advanced memory managers.
 *
 * Note that the algorithm used is quite simple and there might be substantial
 * performance gains if a smarter free list is implemented. Currently it is just an
 * unordered stack of free regions. This could easily be improved if an RB-tree
 * is used instead. At least if we expect heavy fragmentation.
 *
 * Aligned allocations can also see improvement.
 *
 * Authors:
 * Thomas Hellström <thomas-at-tungstengraphics-dot-com>
 * Ported to OpenBSD by:
 * Owain Ainsworth <oga@openbsd.org>
 */

#include "drmP.h"

int	drm_memrange_create_tail_node(struct drm_memrange *, unsigned long, unsigned long);
struct drm_memrange_node	*drm_memrange_split_at_start(struct
				     drm_memrange_node *, unsigned long);

unsigned long
drm_memrange_tail_space(struct drm_memrange *mm)
{
	struct drm_memrange_node	*entry = TAILQ_LAST(&mm->ml, drm_mmq);

	if (!entry->free)
		return (0);

	return (entry->size);
}

int
drm_memrange_remove_space_from_tail(struct drm_memrange *mm, unsigned long size)
{
	struct drm_memrange_node	*entry  = TAILQ_LAST(&mm->ml, drm_mmq);

	if (!entry->free)
		return (ENOMEM);

	if (entry->size <= size)
		return (ENOMEM);

	entry->size -=size;
	return (0);
}

int
drm_memrange_create_tail_node(struct drm_memrange *mm, unsigned long start,
    unsigned long size)
{
	struct drm_memrange_node	*child;

	child = (struct drm_memrange_node *)drm_calloc(1, sizeof(*child),
	    DRM_MEM_MM);
	if (child == NULL)
		return (ENOMEM);

	child->free = 1;
	child->size = size;
	child->start = start;
	child->mm = mm;

	TAILQ_INSERT_TAIL(&mm->ml, child, ml_entry);
	TAILQ_INSERT_TAIL(&mm->fl, child, fl_entry);

	return (0);
}

int
drm_memrange_add_space_to_tail(struct drm_memrange *mm, unsigned long size)
{
	struct drm_memrange_node	*entry = TAILQ_LAST(&mm->ml, drm_mmq);

	if (!entry->free)
		return (drm_memrange_create_tail_node(mm,
		    entry->start + entry->size, size));
	entry->size += size;
	return (0);


}

struct drm_memrange_node *
drm_memrange_split_at_start(struct drm_memrange_node *parent, unsigned long size)
{
	struct drm_memrange_node	*child;

	child = (struct drm_memrange_node *)drm_calloc(1, sizeof(*child),
	    DRM_MEM_MM);
	if (child == NULL)
		return (NULL);

	child->size = size;
	child->start = parent->start;
	child->mm = parent->mm;

	TAILQ_INSERT_TAIL(&child->mm->ml, child, ml_entry);

	parent->size -= size;
	parent->start += size;
	return (child);
}

struct drm_memrange_node *
drm_memrange_get_block(struct drm_memrange_node *parent, unsigned long size,
    unsigned alignment)
{
	struct drm_memrange_node	*child, *align_splitoff = NULL;
	unsigned		 tmp = 0;

	if (alignment)
		tmp = parent->start & alignment;

	if (tmp) {
		align_splitoff = drm_memrange_split_at_start(parent,
		    alignment - tmp);
		if (align_splitoff == NULL)
			return (NULL);
	}

	if (parent->size == size) {
		TAILQ_REMOVE(&parent->mm->fl, parent, fl_entry);
		parent->free = 0;
		return (parent);
	} else 
		child = drm_memrange_split_at_start(parent, size);

	if (align_splitoff)
		drm_memrange_put_block(align_splitoff);

	return (child);

}


void
drm_memrange_put_block(struct drm_memrange_node *cur)
{
	struct drm_memrange		*mm = cur->mm;
	struct drm_memrange_node	*prev_node = NULL;
	struct drm_memrange_node	*next_node;
	int			 merged = 0;

	if ((prev_node = TAILQ_PREV(cur, drm_mmq, ml_entry)) != NULL) {
		if (prev_node->free) {
			prev_node->size += cur->size;
			merged = 1;
		}
	}
	if ((next_node = TAILQ_NEXT(cur, ml_entry)) != NULL) {
		if (next_node ->free) {
			if (merged) {
				prev_node->size += next_node->size;
				TAILQ_REMOVE(&mm->ml,  next_node,
				    ml_entry);
				TAILQ_REMOVE(&mm->fl,  next_node,
				    fl_entry);
				drm_free(next_node, sizeof(*next_node),
				    DRM_MEM_MM);
			}
		}
	}
	if (!merged) {
		cur->free = 1;
		TAILQ_INSERT_TAIL(&mm->fl, cur, fl_entry);
	} else {
		TAILQ_REMOVE(&mm->ml, cur, ml_entry);
		drm_free(cur, sizeof(*cur), DRM_MEM_MM);
	}
}

struct drm_memrange_node *
drm_memrange_search_free(const struct drm_memrange *mm, unsigned long size,
    unsigned alignment, int best_match)
{
	struct drm_memrange_node	*entry, *best;
	unsigned long		 best_size;
	unsigned		 wasted;

	best = NULL;
	best_size = ~0UL;

	TAILQ_FOREACH(entry, &mm->fl, fl_entry ) {
		wasted = 0;

		if (entry->size < size)
			continue;

		if (alignment) {
			unsigned tmp = entry->start % alignment;
			if (tmp)
				wasted += alignment - tmp;
		}

		if (entry->size >= size + wasted) {
			if (!best_match) {
				return (entry);
			}
			if (size < best_size) {
				best = entry;
				best_size = entry->size;
			}
		}
	}
	return (best);
}

int
drm_memrange_clean(struct drm_memrange *mm)
{
	return (TAILQ_FIRST(&mm->ml) == TAILQ_LAST(&mm->ml,drm_mmq));
}

int
drm_memrange_init(struct drm_memrange *mm, unsigned long start,
    unsigned long size)
{
	TAILQ_INIT(&mm->ml);
	TAILQ_INIT(&mm->fl);

	return (drm_memrange_create_tail_node(mm, start, size));
}

void
drm_memrange_takedown(struct drm_memrange *mm)
{
	struct drm_memrange_node *entry;

	entry = TAILQ_FIRST(&mm->ml);

	if (!TAILQ_EMPTY(&mm->ml) || !TAILQ_EMPTY(&mm->fl)) {
		DRM_ERROR("Memory manager not clean, Delaying takedown\n");
		return;
	}
	
	TAILQ_INIT(&mm->ml);
	TAILQ_INIT(&mm->fl);
	drm_free(entry, sizeof(*entry), DRM_MEM_MM);
}
