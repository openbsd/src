/*
 * irel_ma.c
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
#include "irel.h"

static errcode_t ima_put(ext2_irel irel, ino_t old,
			 struct ext2_inode_relocate_entry *ent);
static errcode_t ima_get(ext2_irel irel, ino_t old,
			 struct ext2_inode_relocate_entry *ent);
static errcode_t ima_get_by_orig(ext2_irel irel, ino_t orig, ino_t *old,
				 struct ext2_inode_relocate_entry *ent);
static errcode_t ima_start_iter(ext2_irel irel);
static errcode_t ima_next(ext2_irel irel, ino_t *old,
			  struct ext2_inode_relocate_entry *ent);
static errcode_t ima_add_ref(ext2_irel irel, ino_t ino,
			     struct ext2_inode_reference *ref);
static errcode_t ima_start_iter_ref(ext2_irel irel, ino_t ino);
static errcode_t ima_next_ref(ext2_irel irel, struct ext2_inode_reference *ref);
static errcode_t ima_move(ext2_irel irel, ino_t old, ino_t new);
static errcode_t ima_delete(ext2_irel irel, ino_t old);
static errcode_t ima_free(ext2_irel irel);

/*
 * This data structure stores the array of inode references; there is
 * a structure for each inode.
 */
struct inode_reference_entry {
	__u16 num;
	struct ext2_inode_reference *refs;
};

struct irel_ma {
	__u32 magic;
	ino_t max_inode;
	ino_t ref_current;
	int   ref_iter;
	ino_t	*orig_map;
	struct ext2_inode_relocate_entry *entries;
	struct inode_reference_entry *ref_entries;
};

errcode_t ext2fs_irel_memarray_create(char *name, ino_t max_inode,
				      ext2_irel *new_irel)
{
	ext2_irel		irel = 0;
	errcode_t	retval;
	struct irel_ma 	*ma = 0;
	size_t		size;

	*new_irel = 0;

	/*
	 * Allocate memory structures
	 */
	retval = ENOMEM;
	irel = malloc(sizeof(struct ext2_inode_relocation_table));
	if (!irel)
		goto errout;
	memset(irel, 0, sizeof(struct ext2_inode_relocation_table));
	
	irel->name = malloc(strlen(name)+1);
	if (!irel->name)
		goto errout;
	strcpy(irel->name, name);
	
	ma = malloc(sizeof(struct irel_ma));
	if (!ma)
		goto errout;
	memset(ma, 0, sizeof(struct irel_ma));
	irel->private = ma;
	
	size = sizeof(ino_t) * (max_inode+1);
	ma->orig_map = malloc(size);
	if (!ma->orig_map)
		goto errout;
	memset(ma->orig_map, 0, size);

	size = sizeof(struct ext2_inode_relocate_entry) * (max_inode+1);
	ma->entries = malloc(size);
	if (!ma->entries)
		goto errout;
	memset(ma->entries, 0, size);

	size = sizeof(struct inode_reference_entry) * (max_inode+1);
	ma->ref_entries = malloc(size);
	if (!ma->ref_entries)
		goto errout;
	memset(ma->ref_entries, 0, size);
	ma->max_inode = max_inode;

	/*
	 * Fill in the irel data structure
	 */
	irel->put = ima_put;
	irel->get = ima_get;
	irel->get_by_orig = ima_get_by_orig;
	irel->start_iter = ima_start_iter;
	irel->next = ima_next;
	irel->add_ref = ima_add_ref;
	irel->start_iter_ref = ima_start_iter_ref;
	irel->next_ref = ima_next_ref;
	irel->move = ima_move;
	irel->delete = ima_delete;
	irel->free = ima_free;
	
	*new_irel = irel;
	return 0;

errout:
	ima_free(irel);
	return retval;
}

static errcode_t ima_put(ext2_irel irel, ino_t old,
			struct ext2_inode_relocate_entry *ent)
{
	struct irel_ma 	*ma;
	struct inode_reference_entry *ref_ent;
	struct ext2_inode_reference *new_refs;
	int size;

	ma = irel->private;
	if (old > ma->max_inode)
		return EINVAL;

	/*
	 * Force the orig field to the correct value; the application
	 * program shouldn't be messing with this field.
	 */
	if (ma->entries[old].new == 0)
		ent->orig = old;
	else
		ent->orig = ma->entries[old].orig;
	
	/*
	 * If max_refs has changed, reallocate the refs array
	 */
	ref_ent = ma->ref_entries + old;
	if (ref_ent->refs && ent->max_refs != ma->entries[old].max_refs) {
		size = (sizeof(struct ext2_inode_reference) * ent->max_refs);
		new_refs = realloc(ref_ent->refs, size);
		if (!new_refs)
			return ENOMEM;
		ref_ent->refs = new_refs;
	}

	ma->entries[old] = *ent;
	ma->orig_map[ent->orig] = old;
	return 0;
}

static errcode_t ima_get(ext2_irel irel, ino_t old,
			struct ext2_inode_relocate_entry *ent)
{
	struct irel_ma 	*ma;

	ma = irel->private;
	if (old > ma->max_inode)
		return EINVAL;
	if (ma->entries[old].new == 0)
		return ENOENT;
	*ent = ma->entries[old];
	return 0;
}

static errcode_t ima_get_by_orig(ext2_irel irel, ino_t orig, ino_t *old,
			struct ext2_inode_relocate_entry *ent)
{
	struct irel_ma 	*ma;
	ino_t	ino;

	ma = irel->private;
	if (orig > ma->max_inode)
		return EINVAL;
	ino = ma->orig_map[orig];
	if (ino == 0)
		return ENOENT;
	*old = ino;
	*ent = ma->entries[ino];
	return 0;
}

static errcode_t ima_start_iter(ext2_irel irel)
{
	irel->current = 0;
	return 0;
}

static errcode_t ima_next(ext2_irel irel, ino_t *old,
			 struct ext2_inode_relocate_entry *ent)
{
	struct irel_ma 	*ma;

	ma = irel->private;
	while (++irel->current < ma->max_inode) {
		if (ma->entries[irel->current].new == 0)
			continue;
		*old = irel->current;
		*ent = ma->entries[irel->current];
		return 0;
	}
	*old = 0;
	return 0;
}

static errcode_t ima_add_ref(ext2_irel irel, ino_t ino,
			     struct ext2_inode_reference *ref)
{
	struct irel_ma 	*ma;
	size_t		size;
	struct inode_reference_entry *ref_ent;
	struct ext2_inode_relocate_entry *ent;

	ma = irel->private;
	if (ino > ma->max_inode)
		return EINVAL;

	ref_ent = ma->ref_entries + ino;
	ent = ma->entries + ino;
	
	/*
	 * If the inode reference array doesn't exist, create it.
	 */
	if (ref_ent->refs == 0) {
		size = (sizeof(struct ext2_inode_reference) * ent->max_refs);
		ref_ent->refs = malloc(size);
		if (ref_ent->refs == 0)
			return ENOMEM;
		memset(ref_ent->refs, 0, size);
		ref_ent->num = 0;
	}

	if (ref_ent->num >= ent->max_refs)
		return ENOSPC;

	ref_ent->refs[ref_ent->num++] = *ref;
	return 0;
}

static errcode_t ima_start_iter_ref(ext2_irel irel, ino_t ino)
{
	struct irel_ma 	*ma;

	ma = irel->private;
	if (ino > ma->max_inode)
		return EINVAL;
	if (ma->entries[ino].new == 0)
		return ENOENT;
	ma->ref_current = ino;
	ma->ref_iter = 0;
	return 0;
}

static errcode_t ima_next_ref(ext2_irel irel,
			      struct ext2_inode_reference *ref)
{
	struct irel_ma 	*ma;
	struct inode_reference_entry *ref_ent;

	ma = irel->private;
	
	ref_ent = ma->ref_entries + ma->ref_current;

	if ((ref_ent->refs == NULL) ||
	    (ma->ref_iter >= ref_ent->num)) {
		ref->block = 0;
		ref->offset = 0;
		return 0;
	}
	*ref = ref_ent->refs[ma->ref_iter++];
	return 0;
}


static errcode_t ima_move(ext2_irel irel, ino_t old, ino_t new)
{
	struct irel_ma 	*ma;

	ma = irel->private;
	if ((old > ma->max_inode) || (new > ma->max_inode))
		return EINVAL;
	if (ma->entries[old].new == 0)
		return ENOENT;
	
	ma->entries[new] = ma->entries[old];
	if (ma->ref_entries[new].refs)
		free(ma->ref_entries[new].refs);
	ma->ref_entries[new] = ma->ref_entries[old];
	
	ma->entries[old].new = 0;
	ma->ref_entries[old].num = 0;
	ma->ref_entries[old].refs = 0;

	ma->orig_map[ma->entries[new].orig] = new;
	return 0;
}

static errcode_t ima_delete(ext2_irel irel, ino_t old)
{
	struct irel_ma 	*ma;

	ma = irel->private;
	if (old > ma->max_inode)
		return EINVAL;
	if (ma->entries[old].new == 0)
		return ENOENT;
	
	ma->entries[old].new = 0;
	if (ma->ref_entries[old].refs)
		free(ma->ref_entries[old].refs);
	ma->orig_map[ma->entries[old].orig] = 0;
	
	ma->ref_entries[old].num = 0;
	ma->ref_entries[old].refs = 0;
	return 0;
}

static errcode_t ima_free(ext2_irel irel)
{
	struct irel_ma 	*ma;
	ino_t	ino;

	if (!irel)
		return 0;

	ma = irel->private;

	if (ma) {
		if (ma->orig_map)
			free (ma->orig_map);
		if (ma->entries)
			free (ma->entries);
		if (ma->ref_entries) {
			for (ino = 0; ino <= ma->max_inode; ino++) {
				if (ma->ref_entries[ino].refs)
					free(ma->ref_entries[ino].refs);
			}
			free(ma->ref_entries);
		}
		free(ma);
	}
	if (irel->name)
		free(irel->name);
	free (irel);
	return 0;
}
