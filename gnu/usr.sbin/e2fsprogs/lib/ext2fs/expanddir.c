/*
 * expand.c --- expand an ext2fs directory
 * 
 * Copyright (C) 1993, 1994, 1995, 1996 Theodore Ts'o.
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
#if HAVE_ERRNO_H
#include <errno.h>
#endif

#include <linux/ext2_fs.h>

#include "ext2fs.h"

struct expand_dir_struct {
	int	done;
	errcode_t	err;
};

static int expand_dir_proc(ext2_filsys fs,
			   blk_t	*blocknr,
			   int	blockcnt,
			   void	*private)
{
	struct expand_dir_struct *es = (struct expand_dir_struct *) private;
	blk_t	new_blk;
	static blk_t	last_blk = 0;
	char		*block;
	errcode_t	retval;
	int		group;
	
	if (*blocknr) {
		last_blk = *blocknr;
		return 0;
	}
	retval = ext2fs_new_block(fs, last_blk, 0, &new_blk);
	if (retval) {
		es->err = retval;
		return BLOCK_ABORT;
	}
	if (blockcnt > 0) {
		retval = ext2fs_new_dir_block(fs, 0, 0, &block);
		if (retval) {
			es->err = retval;
			return BLOCK_ABORT;
		}
		es->done = 1;
	} else {
		block = malloc(fs->blocksize);
		if (!block) {
			es->err = ENOMEM;
			return BLOCK_ABORT;
		}
		memset(block, 0, fs->blocksize);
	}	
	retval = ext2fs_write_dir_block(fs, new_blk, block);
	if (retval) {
		es->err = retval;
		return BLOCK_ABORT;
	}
	free(block);
	*blocknr = new_blk;
	ext2fs_mark_block_bitmap(fs->block_map, new_blk);
	ext2fs_mark_bb_dirty(fs);
	group = ext2fs_group_of_blk(fs, new_blk);
	fs->group_desc[group].bg_free_blocks_count--;
	fs->super->s_free_blocks_count--;
	ext2fs_mark_super_dirty(fs);
	if (es->done)
		return (BLOCK_CHANGED | BLOCK_ABORT);
	else
		return BLOCK_CHANGED;
}

errcode_t ext2fs_expand_dir(ext2_filsys fs, ino_t dir)
{
	errcode_t	retval;
	struct expand_dir_struct es;
	struct ext2_inode	inode;
	
	EXT2_CHECK_MAGIC(fs, EXT2_ET_MAGIC_EXT2FS_FILSYS);

	if (!(fs->flags & EXT2_FLAG_RW))
		return EXT2_ET_RO_FILSYS;

	retval = ext2fs_check_directory(fs, dir);
	if (retval)
		return retval;
	
	es.done = 0;
	es.err = 0;
	
	retval = ext2fs_block_iterate(fs, dir, BLOCK_FLAG_APPEND,
				      0, expand_dir_proc, &es);

	if (es.err)
		return es.err;
	if (!es.done)
		return EXT2_ET_EXPAND_DIR_ERR;

	/*
	 * Update the size and block count fields in the inode.
	 */
	retval = ext2fs_read_inode(fs, dir, &inode);
	if (retval)
		return retval;
	
	inode.i_size += fs->blocksize;
	inode.i_blocks += fs->blocksize / 512;

	retval = ext2fs_write_inode(fs, dir, &inode);
	if (retval)
		return retval;

	return 0;
}
