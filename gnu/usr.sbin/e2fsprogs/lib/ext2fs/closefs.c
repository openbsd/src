/*
 * closefs.c --- close an ext2 filesystem
 * 
 * Copyright (C) 1993, 1994, 1995, 1996 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include <linux/ext2_fs.h>

#include "ext2fsP.h"

static int test_root(int a, int b)
{
	if (a == 0)
		return 1;
	while (1) {
		if (a == 1)
			return 1;
		if (a % b)
			return 0;
		a = a / b;
	}
}

int ext2fs_bg_has_super(ext2_filsys fs, int group_block)
{
#ifdef EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER
	struct ext2fs_sb	*s;

	s = (struct ext2fs_sb *) fs->super;
	if (!(s->s_feature_ro_compat & EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER))
		return 1;

	if (test_root(group_block, 3) || (test_root(group_block, 5)) ||
	    test_root(group_block, 7))
		return 1;
	
	return 0;
#else
	return 1;
#endif
}

errcode_t ext2fs_flush(ext2_filsys fs)
{
	int		i,j,maxgroup;
	int		group_block;
	errcode_t	retval;
	char		*group_ptr;
	unsigned long	fs_state;
	struct ext2_super_block *super_shadow = 0;
	struct ext2_group_desc *group_shadow = 0;
	struct ext2_group_desc *s, *t;
	
	EXT2_CHECK_MAGIC(fs, EXT2_ET_MAGIC_EXT2FS_FILSYS);

	fs_state = fs->super->s_state;

	fs->super->s_wtime = time(NULL);
	if (fs->flags & EXT2_FLAG_SWAP_BYTES) {
		retval = ENOMEM;
		if (!(super_shadow = malloc(SUPERBLOCK_SIZE)))
			goto errout;
		if (!(group_shadow = malloc(fs->blocksize*fs->desc_blocks)))
			goto errout;
		memset(group_shadow, 0, fs->blocksize*fs->desc_blocks);

		/* swap the superblock */
		*super_shadow = *fs->super;
		ext2fs_swap_super(super_shadow);

		/* swap the group descriptors */
		for (j=0, s=fs->group_desc, t=group_shadow;
		     j < fs->group_desc_count; j++, t++, s++) {
			*t = *s;
			ext2fs_swap_group_desc(t);
		}
	} else {
		super_shadow = fs->super;
		group_shadow = fs->group_desc;
	}
	
	/*
	 * Write out master superblock.  This has to be done
	 * separately, since it is located at a fixed location
	 * (SUPERBLOCK_OFFSET).
	 */
	io_channel_set_blksize(fs->io, SUPERBLOCK_OFFSET);
	retval = io_channel_write_blk(fs->io, 1, -SUPERBLOCK_SIZE,
				      super_shadow);
	if (retval)
		goto errout;
	io_channel_set_blksize(fs->io, fs->blocksize);

	/*
	 * Set the state of the FS to be non-valid.  (The state has
	 * already been backed up earlier, and will be restored when
	 * we exit.)
	 */
	fs->super->s_state &= ~EXT2_VALID_FS;
	if (fs->flags & EXT2_FLAG_SWAP_BYTES) {
		*super_shadow = *fs->super;
		ext2fs_swap_super(super_shadow);
	}

	/*
	 * Write out the master group descriptors, and the backup
	 * superblocks and group descriptors.
	 */
	group_block = fs->super->s_first_data_block;
	maxgroup = (fs->flags & EXT2_FLAG_MASTER_SB_ONLY) ? 1 :
		fs->group_desc_count;
	for (i = 0; i < maxgroup; i++) {
		if (!ext2fs_bg_has_super(fs, i))
			goto next_group;

		if (i !=0 ) {
			retval = io_channel_write_blk(fs->io, group_block,
						      -SUPERBLOCK_SIZE,
						      super_shadow);
			if (retval)
				goto errout;
		}
		group_ptr = (char *) group_shadow;
		for (j=0; j < fs->desc_blocks; j++) {
			retval = io_channel_write_blk(fs->io,
						      group_block+1+j, 1,
						      group_ptr);
			if (retval)
				goto errout;
			group_ptr += fs->blocksize;
		}
	next_group:
		group_block += EXT2_BLOCKS_PER_GROUP(fs->super);
	}

	/*
	 * If the write_bitmaps() function is present, call it to
	 * flush the bitmaps.  This is done this way so that a simple
	 * program that doesn't mess with the bitmaps doesn't need to
	 * drag in the bitmaps.c code.
	 */
	if (fs->write_bitmaps) {
		retval = fs->write_bitmaps(fs);
		if (retval)
			goto errout;
	}
	retval = 0;
errout:
	fs->super->s_state = fs_state;
	if (fs->flags & EXT2_FLAG_SWAP_BYTES) {
		if (super_shadow)
			free(super_shadow);
		if (group_shadow)
			free(group_shadow);
	}
	return retval;
}

errcode_t ext2fs_close(ext2_filsys fs)
{
	errcode_t	retval;
	
	EXT2_CHECK_MAGIC(fs, EXT2_ET_MAGIC_EXT2FS_FILSYS);

	if (fs->flags & EXT2_FLAG_DIRTY) {
		retval = ext2fs_flush(fs);
		if (retval)
			return retval;
	}
	if (fs->write_bitmaps) {
		retval = fs->write_bitmaps(fs);
		if (retval)
			return retval;
	}
	ext2fs_free(fs);
	return 0;
}

