/*
 * openfs.c --- open an ext2 filesystem
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
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <fcntl.h>
#include <time.h>
#if HAVE_ERRNO_H
#include <errno.h>
#endif
#include <sys/stat.h>
#include <sys/types.h>

#include <linux/ext2_fs.h>

#include "ext2fs.h"

/*
 *  Note: if superblock is non-zero, block-size must also be non-zero.
 * 	Superblock and block_size can be zero to use the default size.
 *
 * Valid flags for ext2fs_open()
 * 
 * 	EXT2_FLAG_RW	- Open the filesystem for read/write.
 * 	EXT2_FLAG_FORCE - Open the filesystem even if some of the
 *  				features aren't supported.
 */
errcode_t ext2fs_open(const char *name, int flags, int superblock,
		      int block_size, io_manager manager, ext2_filsys *ret_fs)
{
	ext2_filsys	fs;
	errcode_t	retval;
	int		i, j, group_block, groups_per_block;
	char		*dest;
	struct ext2_group_desc *gdp;
	struct ext2fs_sb	*s;
	
	EXT2_CHECK_MAGIC(manager, EXT2_ET_MAGIC_IO_MANAGER);
	
	fs = (ext2_filsys) malloc(sizeof(struct struct_ext2_filsys));
	if (!fs)
		return ENOMEM;
	
	memset(fs, 0, sizeof(struct struct_ext2_filsys));
	fs->magic = EXT2_ET_MAGIC_EXT2FS_FILSYS;
	fs->flags = flags;
	retval = manager->open(name, (flags & EXT2_FLAG_RW) ? IO_FLAG_RW : 0,
			       &fs->io);
	if (retval)
		goto cleanup;
	fs->device_name = malloc(strlen(name)+1);
	if (!fs->device_name) {
		retval = ENOMEM;
		goto cleanup;
	}
	strcpy(fs->device_name, name);
	fs->super = malloc(SUPERBLOCK_SIZE);
	if (!fs->super) {
		retval = ENOMEM;
		goto cleanup;
	}

	/*
	 * If the user specifies a specific block # for the
	 * superblock, then he/she must also specify the block size!
	 * Otherwise, read the master superblock located at offset
	 * SUPERBLOCK_OFFSET from the start of the partition.
	 */
	if (superblock) {
		if (!block_size) {
			retval = EINVAL;
			goto cleanup;
		}
		io_channel_set_blksize(fs->io, block_size);
		group_block = superblock + 1;
	} else {
		io_channel_set_blksize(fs->io, SUPERBLOCK_OFFSET);
		superblock = 1;
		group_block = 0;
	}
	retval = io_channel_read_blk(fs->io, superblock, -SUPERBLOCK_SIZE,
				     fs->super);
	if (retval)
		goto cleanup;

	if ((fs->super->s_magic == ext2fs_swab16(EXT2_SUPER_MAGIC)) ||
	    (fs->flags & EXT2_FLAG_SWAP_BYTES)) {
		fs->flags |= EXT2_FLAG_SWAP_BYTES;

		ext2fs_swap_super(fs->super);
	}
	
	if (fs->super->s_magic != EXT2_SUPER_MAGIC) {
		retval = EXT2_ET_BAD_MAGIC;
		goto cleanup;
	}
#ifdef EXT2_DYNAMIC_REV
	if (fs->super->s_rev_level > EXT2_DYNAMIC_REV) {
		retval = EXT2_ET_REV_TOO_HIGH;
		goto cleanup;
	}
#else
#ifdef	EXT2_CURRENT_REV
	if (fs->super->s_rev_level > EXT2_LIB_CURRENT_REV) {
		retval = EXT2_ET_REV_TOO_HIGH;
		goto cleanup;
	}
#endif
#endif
	/*
	 * Check for feature set incompatibility
	 */
	if (!(flags & EXT2_FLAG_FORCE)) {
		s = (struct ext2fs_sb *) fs->super;
		if (s->s_feature_incompat & ~EXT2_LIB_FEATURE_INCOMPAT_SUPP) {
			retval = EXT2_ET_UNSUPP_FEATURE;
			goto cleanup;
		}
		if ((flags & EXT2_FLAG_RW) &&
		    (s->s_feature_ro_compat &
		     ~EXT2_LIB_FEATURE_RO_COMPAT_SUPP)) {
			retval = EXT2_ET_RO_UNSUPP_FEATURE;
			goto cleanup;
		}
	}
	
	fs->blocksize = EXT2_BLOCK_SIZE(fs->super);
	if (fs->blocksize == 0) {
		retval = EXT2_ET_CORRUPT_SUPERBLOCK;
		goto cleanup;
	}
	fs->fragsize = EXT2_FRAG_SIZE(fs->super);
	fs->inode_blocks_per_group = ((fs->super->s_inodes_per_group *
				       EXT2_INODE_SIZE(fs->super) +
				       EXT2_BLOCK_SIZE(fs->super) - 1) /
				      EXT2_BLOCK_SIZE(fs->super));
	if (block_size) {
		if (block_size != fs->blocksize) {
			retval = EXT2_ET_UNEXPECTED_BLOCK_SIZE;
			goto cleanup;
		}
	}
	/*
	 * Set the blocksize to the filesystem's blocksize.
	 */
	io_channel_set_blksize(fs->io, fs->blocksize);
	
	/*
	 * Read group descriptors
	 */
	fs->group_desc_count = (fs->super->s_blocks_count -
				fs->super->s_first_data_block +
				EXT2_BLOCKS_PER_GROUP(fs->super) - 1)
		/ EXT2_BLOCKS_PER_GROUP(fs->super);
	fs->desc_blocks = (fs->group_desc_count +
			   EXT2_DESC_PER_BLOCK(fs->super) - 1)
		/ EXT2_DESC_PER_BLOCK(fs->super);
	fs->group_desc = malloc(fs->desc_blocks * fs->blocksize);
	if (!fs->group_desc) {
		retval = ENOMEM;
		goto cleanup;
	}
	if (!group_block)
		group_block = fs->super->s_first_data_block + 1;
	dest = (char *) fs->group_desc;
	for (i=0 ; i < fs->desc_blocks; i++) {
		retval = io_channel_read_blk(fs->io, group_block, 1, dest);
		if (retval)
			goto cleanup;
		group_block++;
		if (fs->flags & EXT2_FLAG_SWAP_BYTES) {
			gdp = (struct ext2_group_desc *) dest;
			groups_per_block = fs->blocksize /
				sizeof(struct ext2_group_desc);
			for (j=0; j < groups_per_block; j++)
				ext2fs_swap_group_desc(gdp++);
		}
		dest += fs->blocksize;
	}

	*ret_fs = fs;
	return 0;
cleanup:
	ext2fs_free(fs);
	return retval;
}

