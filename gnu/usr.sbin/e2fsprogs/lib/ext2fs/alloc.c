/*
 * alloc.c --- allocate new inodes, blocks for ext2fs
 *
 * Copyright (C) 1993, 1994, 1995, 1996 Theodore Ts'o.
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
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#if HAVE_ERRNO_H
#include <errno.h>
#endif

#include <linux/ext2_fs.h>

#include "ext2fs.h"

/*
 * Right now, just search forward from the parent directory's block
 * group to find the next free inode.
 *
 * Should have a special policy for directories.
 */
errcode_t ext2fs_new_inode(ext2_filsys fs, ino_t dir, int mode,
			   ext2fs_inode_bitmap map, ino_t *ret)
{
	int	dir_group = 0;
	ino_t	i;
	ino_t	start_inode;

	EXT2_CHECK_MAGIC(fs, EXT2_ET_MAGIC_EXT2FS_FILSYS);
	
	if (!map)
		map = fs->inode_map;
	if (!map)
		return EXT2_ET_NO_INODE_BITMAP;
	
	if (dir > 0) 
		dir_group = (dir - 1) / EXT2_INODES_PER_GROUP(fs->super);

	start_inode = (dir_group * EXT2_INODES_PER_GROUP(fs->super)) + 1;
	if (start_inode < EXT2_FIRST_INODE(fs->super))
		start_inode = EXT2_FIRST_INODE(fs->super);
	i = start_inode;

	do {
		if (!ext2fs_test_inode_bitmap(map, i))
			break;
		i++;
		if (i > fs->super->s_inodes_count)
			i = EXT2_FIRST_INODE(fs->super);
	} while (i != start_inode);
	
	if (ext2fs_test_inode_bitmap(map, i))
		return ENOSPC;
	*ret = i;
	return 0;
}

/*
 * Stupid algorithm --- we now just search forward starting from the
 * goal.  Should put in a smarter one someday....
 */
errcode_t ext2fs_new_block(ext2_filsys fs, blk_t goal,
			   ext2fs_block_bitmap map, blk_t *ret)
{
	blk_t	i;

	EXT2_CHECK_MAGIC(fs, EXT2_ET_MAGIC_EXT2FS_FILSYS);

	if (!map)
		map = fs->block_map;
	if (!map)
		return EXT2_ET_NO_BLOCK_BITMAP;
	if (!goal || (goal >= fs->super->s_blocks_count))
		goal = fs->super->s_first_data_block;
	i = goal;
	do {
		if (!ext2fs_test_block_bitmap(map, i)) {
			*ret = i;
			return 0;
		}
		i++;
		if (i >= fs->super->s_blocks_count)
			i = fs->super->s_first_data_block;
	} while (i != goal);
	return ENOSPC;
}

errcode_t ext2fs_get_free_blocks(ext2_filsys fs, blk_t start, blk_t finish,
				 int num, ext2fs_block_bitmap map, blk_t *ret)
{
	blk_t	b = start;

	EXT2_CHECK_MAGIC(fs, EXT2_ET_MAGIC_EXT2FS_FILSYS);

	if (!map)
		map = fs->block_map;
	if (!map)
		return EXT2_ET_NO_BLOCK_BITMAP;
	if (!b)
		b = fs->super->s_first_data_block;
	if (!finish)
		finish = start;
	if (!num)
		num = 1;
	do {
		if (b+num-1 > fs->super->s_blocks_count)
			b = fs->super->s_first_data_block;
		if (ext2fs_fast_test_block_bitmap_range(map, b, num)) {
			*ret = b;
			return 0;
		}
		b++;
	} while (b != finish);
	return ENOSPC;
}

