/*
 * read_bb --- read the bad blocks inode
 *
 * Copyright (C) 1994 Theodore Ts'o.
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

#include <linux/ext2_fs.h>

#include "ext2fs.h"

struct read_bb_record {
	ext2_badblocks_list	bb_list;
	errcode_t	err;
};

/*
 * Helper function for ext2fs_read_bb_inode()
 */
static int mark_bad_block(ext2_filsys fs, blk_t *block_nr,
			     int blockcnt, void *private)
{
	struct read_bb_record *rb = (struct read_bb_record *) private;
	
	if (blockcnt < 0)
		return 0;
	
	rb->err = ext2fs_badblocks_list_add(rb->bb_list, *block_nr);
	if (rb->err)
		return BLOCK_ABORT;
	return 0;
}

/*
 * Reads the current bad blocks from the bad blocks inode.
 */
errcode_t ext2fs_read_bb_inode(ext2_filsys fs, ext2_badblocks_list *bb_list)
{
	errcode_t	retval;
	struct read_bb_record rb;
	struct ext2_inode inode;
	int	numblocks;

	EXT2_CHECK_MAGIC(fs, EXT2_ET_MAGIC_EXT2FS_FILSYS);

	if (!*bb_list) {
		retval = ext2fs_read_inode(fs, EXT2_BAD_INO, &inode);
		if (retval)
			return retval;
		numblocks = (inode.i_blocks / (fs->blocksize / 512)) + 20;
		retval = ext2fs_badblocks_list_create(bb_list, numblocks);
		if (retval)
			return retval;
	}

	rb.bb_list = *bb_list;
	rb.err = 0;
	retval = ext2fs_block_iterate(fs, EXT2_BAD_INO, 0, 0,
				      mark_bad_block, &rb);
	if (retval)
		return retval;

	return rb.err;
}


