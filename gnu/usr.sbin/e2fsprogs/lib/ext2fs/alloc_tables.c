/*
 * alloc_tables.c --- Allocate tables for a newly initialized
 * filesystem.  Used by mke2fs when initializing a filesystem
 *
 * Copyright (C) 1996 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#if HAVE_ERRNO_H
#include <errno.h>
#endif

#include <linux/ext2_fs.h>

#include "ext2fs.h"

errcode_t ext2fs_allocate_tables(ext2_filsys fs)
{
	errcode_t	retval;
	blk_t		group_blk, start_blk, last_blk, new_blk, blk;
	int		i, j;

	group_blk = fs->super->s_first_data_block;
	for (i = 0; i < fs->group_desc_count; i++) {
		last_blk = group_blk + fs->super->s_blocks_per_group;
		if (last_blk >= fs->super->s_blocks_count)
			last_blk = fs->super->s_blocks_count - 1;

		/*
		 * Allocate the inode table
		 */
		start_blk = group_blk + 3 + fs->desc_blocks;
		if (start_blk > last_blk)
			start_blk = group_blk;
		retval = ext2fs_get_free_blocks(fs, start_blk, last_blk,
						fs->inode_blocks_per_group,
						fs->block_map, &new_blk);
		if (retval)
			return retval;
		for (j=0, blk = new_blk;
		     j < fs->inode_blocks_per_group;
		     j++, blk++)
			ext2fs_mark_block_bitmap(fs->block_map, blk);
		fs->group_desc[i].bg_inode_table = new_blk;

		/*
		 * Allocate the block and inode bitmaps
		 */
		if (fs->stride) {
			start_blk += fs->inode_blocks_per_group;
			start_blk += ((fs->stride * i) %
				      (last_blk - start_blk));
			if (start_blk > last_blk)
				 /* should never happen */
				start_blk = group_blk;
		} else
			start_blk = group_blk;
		retval = ext2fs_get_free_blocks(fs, start_blk, last_blk,
						1, fs->block_map, &new_blk);
		if (retval)
			return retval;
		ext2fs_mark_block_bitmap(fs->block_map, new_blk);
		fs->group_desc[i].bg_block_bitmap = new_blk;

		retval = ext2fs_get_free_blocks(fs, start_blk, last_blk,
						1, fs->block_map, &new_blk);
		if (retval)
			return retval;
		ext2fs_mark_block_bitmap(fs->block_map, new_blk);
		fs->group_desc[i].bg_inode_bitmap = new_blk;

		/*
		 * Increment the start of the block group
		 */
		group_blk += fs->super->s_blocks_per_group;
	}
	return 0;
}

