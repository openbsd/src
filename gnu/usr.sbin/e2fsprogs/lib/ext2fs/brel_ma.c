/*
 * brel_ma.c
 * 
 * Copyright (C) 1996, 1997 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <linux/ext2_fs.h>

#include "ext2fs.h"
#include "brel.h"

static errcode_t bma_put(ext2_brel brel, blk_t old,
			struct ext2_block_relocate_entry *ent);
static errcode_t bma_get(ext2_brel brel, blk_t old,
			struct ext2_block_relocate_entry *ent);
static errcode_t bma_start_iter(ext2_brel brel);
static errcode_t bma_next(ext2_brel brel, blk_t *old,
			 struct ext2_block_relocate_entry *ent);
static errcode_t bma_move(ext2_brel brel, blk_t old, blk_t new);
static errcode_t bma_delete(ext2_brel brel, blk_t old);
static errcode_t bma_free(ext2_brel brel);

struct brel_ma {
	__u32 magic;
	blk_t max_block;
	struct ext2_block_relocate_entry *entries;
};

errcode_t ext2fs_brel_memarray_create(char *name, blk_t max_block,
				      ext2_brel *new_brel)
{
	ext2_brel		brel = 0;
	errcode_t	retval;
	struct brel_ma 	*ma = 0;
	size_t		size;

	*new_brel = 0;

	/*
	 * Allocate memory structures
	 */
	retval = ENOMEM;
	brel = malloc(sizeof(struct ext2_block_relocation_table));
	if (!brel)
		goto errout;
	memset(brel, 0, sizeof(struct ext2_block_relocation_table));
	
	brel->name = malloc(strlen(name)+1);
	if (!brel->name)
		goto errout;
	strcpy(brel->name, name);
	
	ma = malloc(sizeof(struct brel_ma));
	if (!ma)
		goto errout;
	memset(ma, 0, sizeof(struct brel_ma));
	brel->private = ma;
	
	size = sizeof(struct ext2_block_relocate_entry) * (max_block+1);
	ma->entries = malloc(size);
	if (!ma->entries)
		goto errout;
	memset(ma->entries, 0, size);
	ma->max_block = max_block;

	/*
	 * Fill in the brel data structure
	 */
	brel->put = bma_put;
	brel->get = bma_get;
	brel->start_iter = bma_start_iter;
	brel->next = bma_next;
	brel->move = bma_move;
	brel->delete = bma_delete;
	brel->free = bma_free;
	
	*new_brel = brel;
	return 0;

errout:
	bma_free(brel);
	return retval;
}

static errcode_t bma_put(ext2_brel brel, blk_t old,
			struct ext2_block_relocate_entry *ent)
{
	struct brel_ma 	*ma;

	ma = brel->private;
	if (old > ma->max_block)
		return EINVAL;
	ma->entries[old] = *ent;
	return 0;
}

static errcode_t bma_get(ext2_brel brel, blk_t old,
			struct ext2_block_relocate_entry *ent)
{
	struct brel_ma 	*ma;

	ma = brel->private;
	if (old > ma->max_block)
		return EINVAL;
	if (ma->entries[old].new == 0)
		return ENOENT;
	*ent = ma->entries[old];
	return 0;
}

static errcode_t bma_start_iter(ext2_brel brel)
{
	brel->current = 0;
	return 0;
}

static errcode_t bma_next(ext2_brel brel, blk_t *old,
			  struct ext2_block_relocate_entry *ent)
{
	struct brel_ma 	*ma;

	ma = brel->private;
	while (++brel->current < ma->max_block) {
		if (ma->entries[brel->current].new == 0)
			continue;
		*old = brel->current;
		*ent = ma->entries[brel->current];
		return 0;
	}
	*old = 0;
	return 0;
}

static errcode_t bma_move(ext2_brel brel, blk_t old, blk_t new)
{
	struct brel_ma 	*ma;

	ma = brel->private;
	if ((old > ma->max_block) || (new > ma->max_block))
		return EINVAL;
	if (ma->entries[old].new == 0)
		return ENOENT;
	ma->entries[new] = ma->entries[old];
	ma->entries[old].new = 0;
	return 0;
}

static errcode_t bma_delete(ext2_brel brel, blk_t old)
{
	struct brel_ma 	*ma;

	ma = brel->private;
	if (old > ma->max_block)
		return EINVAL;
	if (ma->entries[old].new == 0)
		return ENOENT;
	ma->entries[old].new = 0;
	return 0;
}

static errcode_t bma_free(ext2_brel brel)
{
	struct brel_ma 	*ma;

	if (!brel)
		return 0;

	ma = brel->private;

	if (ma) {
		if (ma->entries)
			free (ma->entries);
		free(ma);
	}
	if (brel->name)
		free(brel->name);
	free (brel);
	return 0;
}
