/*
 * mkdir.c --- make a directory in the filesystem
 * 
 * Copyright (C) 1994, 1995 Theodore Ts'o.
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

errcode_t ext2fs_mkdir(ext2_filsys fs, ino_t parent, ino_t inum,
		       const char *name)
{
	errcode_t		retval;
	struct ext2_inode	inode;
	ino_t			ino = inum;
	ino_t			scratch_ino;
	blk_t			blk;
	char			*block = 0;
	int			group;

	EXT2_CHECK_MAGIC(fs, EXT2_ET_MAGIC_EXT2FS_FILSYS);

	/*
	 * Allocate an inode, if necessary
	 */
	if (!ino) {
		retval = ext2fs_new_inode(fs, parent, LINUX_S_IFDIR | 0755,
					  0, &ino);
		if (retval)
			goto cleanup;
	}

	/*
	 * Allocate a data block for the directory
	 */
	retval = ext2fs_new_block(fs, 0, 0, &blk);
	if (retval)
		goto cleanup;

	/*
	 * Create a scratch template for the directory
	 */
	retval = ext2fs_new_dir_block(fs, ino, parent, &block);
	if (retval)
		goto cleanup;

	/*
	 * Create the inode structure....
	 */
	memset(&inode, 0, sizeof(struct ext2_inode));
	inode.i_mode = LINUX_S_IFDIR | 0755;
	inode.i_uid = inode.i_gid = 0;
	inode.i_blocks = fs->blocksize / 512;
	inode.i_block[0] = blk;
	inode.i_links_count = 2;
	inode.i_ctime = inode.i_atime = inode.i_mtime = time(NULL);
	inode.i_size = fs->blocksize;

	/*
	 * Write out the inode and inode data block
	 */
	retval = ext2fs_write_dir_block(fs, blk, block);
	if (retval)
		goto cleanup;
	retval = ext2fs_write_inode(fs, ino, &inode); 
	if (retval)
		goto cleanup;

	/*
	 * Update parent inode's counts
	 */
	if (parent != ino) {
		retval = ext2fs_read_inode(fs, parent, &inode);
		if (retval)
			goto cleanup;
		inode.i_links_count++;
		retval = ext2fs_write_inode(fs, parent, &inode);
		if (retval)
			goto cleanup;
	}
	
	/*
	 * Link the directory into the filesystem hierarchy
	 */
	if (name) {
		retval = ext2fs_lookup(fs, parent, name, strlen(name), 0,
				       &scratch_ino);
		if (!retval) {
			retval = EEXIST;
			name = 0;
			goto cleanup;
		}
		if (retval != ENOENT)
			goto cleanup;
		retval = ext2fs_link(fs, parent, name, ino, 0);
		if (retval)
			goto cleanup;
	}

	/*
	 * Update accounting....
	 */
	ext2fs_mark_block_bitmap(fs->block_map, blk);
	ext2fs_mark_bb_dirty(fs);
	ext2fs_mark_inode_bitmap(fs->inode_map, ino);
	ext2fs_mark_ib_dirty(fs);

	group = ext2fs_group_of_blk(fs, blk);
	fs->group_desc[group].bg_free_blocks_count--;
	group = ext2fs_group_of_ino(fs, ino);
	fs->group_desc[group].bg_free_inodes_count--;
	fs->group_desc[group].bg_used_dirs_count++;
	fs->super->s_free_blocks_count--;
	fs->super->s_free_inodes_count--;
	ext2fs_mark_super_dirty(fs);
	
cleanup:
	if (block)
		free(block);
	return retval;

}


