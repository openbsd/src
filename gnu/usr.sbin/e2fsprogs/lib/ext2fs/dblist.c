/*
 * dblist.c -- directory block list functions
 * 
 * Copyright 1997 by Theodore Ts'o
 * 
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 * 
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include <linux/ext2_fs.h>

#include "ext2fsP.h"

static int dir_block_cmp(const void *a, const void *b);

/*
 * Returns the number of directories in the filesystem as reported by
 * the group descriptors.  Of course, the group descriptors could be
 * wrong!
 */
errcode_t ext2fs_get_num_dirs(ext2_filsys fs, ino_t *ret_num_dirs)
{
	int	i;
	ino_t	num_dirs;

	EXT2_CHECK_MAGIC(fs, EXT2_ET_MAGIC_EXT2FS_FILSYS);
	
	num_dirs = 0;
	for (i = 0; i < fs->group_desc_count; i++)
		num_dirs += fs->group_desc[i].bg_used_dirs_count;

	*ret_num_dirs = num_dirs;

	return 0;
}

/*
 * helper function for making a new directory block list (for
 * initialize and copy).
 */
static errcode_t make_dblist(ext2_filsys fs, ino_t size, ino_t count,
			     struct ext2_db_entry *list,
			     ext2_dblist *ret_dblist)
{
	ext2_dblist	dblist;
	errcode_t	retval;
	size_t		len;

	EXT2_CHECK_MAGIC(fs, EXT2_ET_MAGIC_EXT2FS_FILSYS);

	if ((ret_dblist == 0) && fs->dblist &&
	    (fs->dblist->magic == EXT2_ET_MAGIC_DBLIST))
		return 0;

	dblist = malloc(sizeof(struct ext2_struct_dblist));
	if (!dblist)
		return ENOMEM;
	memset(dblist, 0, sizeof(struct ext2_struct_dblist));

	dblist->magic = EXT2_ET_MAGIC_DBLIST;
	dblist->fs = fs;
	if (size)
		dblist->size = size;
	else {
		retval = ext2fs_get_num_dirs(fs, &dblist->size);
		if (retval)
			goto cleanup;
	}
	len = sizeof(struct ext2_db_entry) * dblist->size;
	dblist->count = count;
	dblist->list = malloc(len);
	if (dblist->list == NULL) {
		retval = ENOMEM;
		goto cleanup;
	}
	if (list)
		memcpy(dblist->list, list, len);
	else
		memset(dblist->list, 0, len);
	*ret_dblist = dblist;
	return 0;
cleanup:
	if (dblist)
		free(dblist);
	return retval;
}

/*
 * Initialize a directory block list
 */
errcode_t ext2fs_init_dblist(ext2_filsys fs, ext2_dblist *ret_dblist)
{
	ext2_dblist	dblist;
	errcode_t	retval;

	retval = make_dblist(fs, 0, 0, 0, &dblist);
	if (retval)
		return retval;

	dblist->sorted = 1;
	if (ret_dblist)
		*ret_dblist = dblist;
	else
		fs->dblist = dblist;

	return 0;
}

/*
 * Copy a directory block list
 */
errcode_t ext2fs_copy_dblist(ext2_dblist src, ext2_dblist *dest)
{
	ext2_dblist	dblist;
	errcode_t	retval;

	retval = make_dblist(src->fs, src->size, src->count, src->list,
			     &dblist);
	if (retval)
		return retval;
	dblist->sorted = src->sorted;
	*dest = dblist;
	return 0;
}

/*
 * Close a directory block list
 *
 * (moved to closefs.c)
 */


/*
 * Add a directory block to the directory block list
 */
errcode_t ext2fs_add_dir_block(ext2_dblist dblist, ino_t ino, blk_t blk,
			       int blockcnt)
{
	struct ext2_db_entry 	*nlist, *new;
	
	EXT2_CHECK_MAGIC(dblist, EXT2_ET_MAGIC_DBLIST);

	if (dblist->count >= dblist->size) {
		dblist->size += 100;
		nlist = realloc(dblist->list,
				dblist->size * sizeof(struct ext2_db_entry));
		if (nlist == 0) {
			dblist->size -= 100;
			return ENOMEM;
		}
		dblist->list = nlist;
	}
	new = dblist->list + dblist->count++;
	new->blk = blk;
	new->ino = ino;
	new->blockcnt = blockcnt;

	dblist->sorted = 0;

	return 0;
}

/*
 * Change the directory block to the directory block list
 */
errcode_t ext2fs_set_dir_block(ext2_dblist dblist, ino_t ino, blk_t blk,
			       int blockcnt)
{
	struct ext2_db_entry 	*ent;
	int			i;
	
	EXT2_CHECK_MAGIC(dblist, EXT2_ET_MAGIC_DBLIST);

	for (i=0; i < dblist->count; i++) {
		if ((dblist->list[i].ino != ino) ||
		    (dblist->list[i].blockcnt != blockcnt))
			continue;
		dblist->list[i].blk = blk;
		dblist->sorted = 0;
		return 0;
	}
	return ENOENT;
}

/*
 * This function iterates over the directory block list
 */
errcode_t ext2fs_dblist_iterate(ext2_dblist dblist,
				int (*func)(ext2_filsys fs,
					    struct ext2_db_entry *db_info,
					    void	*private),
				void *private)
{
	ino_t	i;
	int	ret;
	
	EXT2_CHECK_MAGIC(dblist, EXT2_ET_MAGIC_DBLIST);

	if (!dblist->sorted) {
		qsort(dblist->list, dblist->count,
		      sizeof(struct ext2_db_entry), dir_block_cmp);
		dblist->sorted = 1;
	}
	for (i=0; i < dblist->count; i++) {
		ret = (*func)(dblist->fs, &dblist->list[i], private);
		if (ret & DBLIST_ABORT)
			return 0;
	}
	return 0;
}


static int dir_block_cmp(const void *a, const void *b)
{
	const struct ext2_db_entry *db_a =
		(const struct ext2_db_entry *) a;
	const struct ext2_db_entry *db_b =
		(const struct ext2_db_entry *) b;

	if (db_a->blk != db_b->blk)
		return (db_a->blk - db_b->blk);
	
	if (db_a->ino != db_b->ino)
		return (db_a->ino - db_b->ino);

	return (db_a->blockcnt - db_b->blockcnt);
}

			     
