/*
 * rw_bitmaps.c --- routines to read and write the  inode and block bitmaps.
 *
 * Copyright (C) 1993, 1994, 1994, 1996 Theodore Ts'o.
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

errcode_t ext2fs_write_inode_bitmap(ext2_filsys fs)
{
	int 		i;
	int		nbytes;
	errcode_t	retval;
	char * inode_bitmap = fs->inode_map->bitmap;
	char * bitmap_block = NULL;
	blk_t		blk;

	EXT2_CHECK_MAGIC(fs, EXT2_ET_MAGIC_EXT2FS_FILSYS);

	if (!(fs->flags & EXT2_FLAG_RW))
		return EXT2_ET_RO_FILSYS;
	if (!inode_bitmap)
		return 0;
	nbytes = (EXT2_INODES_PER_GROUP(fs->super)+7) / 8;
	
	bitmap_block = malloc(fs->blocksize);
	if (!bitmap_block)
		return ENOMEM;
	memset(bitmap_block, 0xff, fs->blocksize);
	for (i = 0; i < fs->group_desc_count; i++) {
		memcpy(bitmap_block, inode_bitmap, nbytes);
		blk = fs->group_desc[i].bg_inode_bitmap;
		if (blk) {
			retval = io_channel_write_blk(fs->io, blk, 1,
						      bitmap_block);
			if (retval)
				return EXT2_ET_INODE_BITMAP_WRITE;
		}
		inode_bitmap += nbytes;
	}
	fs->flags |= EXT2_FLAG_CHANGED;
	fs->flags &= ~EXT2_FLAG_IB_DIRTY;
	free(bitmap_block);
	return 0;
}

errcode_t ext2fs_write_block_bitmap (ext2_filsys fs)
{
	int 		i;
	int		j;
	int		nbytes;
	int		nbits;
	errcode_t	retval;
	char * block_bitmap = fs->block_map->bitmap;
	char * bitmap_block = NULL;
	blk_t		blk;

	EXT2_CHECK_MAGIC(fs, EXT2_ET_MAGIC_EXT2FS_FILSYS);

	if (!(fs->flags & EXT2_FLAG_RW))
		return EXT2_ET_RO_FILSYS;
	if (!block_bitmap)
		return 0;
	nbytes = EXT2_BLOCKS_PER_GROUP(fs->super) / 8;
	bitmap_block = malloc(fs->blocksize);
	if (!bitmap_block)
		return ENOMEM;
	memset(bitmap_block, 0xff, fs->blocksize);
	for (i = 0; i < fs->group_desc_count; i++) {
		memcpy(bitmap_block, block_bitmap, nbytes);
		if (i == fs->group_desc_count - 1) {
			/* Force bitmap padding for the last group */
			nbits = (fs->super->s_blocks_count
				 - fs->super->s_first_data_block)
				% EXT2_BLOCKS_PER_GROUP(fs->super);
			if (nbits)
				for (j = nbits; j < fs->blocksize * 8; j++)
					ext2fs_set_bit(j, bitmap_block);
		}
		blk = fs->group_desc[i].bg_block_bitmap;
		if (blk) {
			retval = io_channel_write_blk(fs->io, blk, 1,
						      bitmap_block);
			if (retval)
				return EXT2_ET_BLOCK_BITMAP_WRITE;
		}
		block_bitmap += nbytes;
	}
	fs->flags |= EXT2_FLAG_CHANGED;
	fs->flags &= ~EXT2_FLAG_BB_DIRTY;
	free(bitmap_block);
	return 0;
}

static errcode_t read_bitmaps(ext2_filsys fs, int do_inode, int do_block)
{
	int i;
	char *block_bitmap = 0, *inode_bitmap = 0;
	char *buf;
	errcode_t retval;
	int block_nbytes = EXT2_BLOCKS_PER_GROUP(fs->super) / 8;
	int inode_nbytes = EXT2_INODES_PER_GROUP(fs->super) / 8;
	blk_t	blk;

	EXT2_CHECK_MAGIC(fs, EXT2_ET_MAGIC_EXT2FS_FILSYS);

	fs->write_bitmaps = ext2fs_write_bitmaps;

	buf = malloc(strlen(fs->device_name) + 80);
	if (do_block) {
		if (fs->block_map)
			ext2fs_free_block_bitmap(fs->block_map);
		sprintf(buf, "block bitmap for %s", fs->device_name);
		retval = ext2fs_allocate_block_bitmap(fs, buf, &fs->block_map);
		if (retval)
			goto cleanup;
		block_bitmap = fs->block_map->bitmap;
	}
	if (do_inode) {
		if (fs->inode_map)
			ext2fs_free_inode_bitmap(fs->inode_map);
		sprintf(buf, "inode bitmap for %s", fs->device_name);
		retval = ext2fs_allocate_inode_bitmap(fs, buf, &fs->inode_map);
		if (retval)
			goto cleanup;
		inode_bitmap = fs->inode_map->bitmap;
	}
	free(buf);

	for (i = 0; i < fs->group_desc_count; i++) {
		if (block_bitmap) {
			blk = fs->group_desc[i].bg_block_bitmap;
			if (blk) {
				retval = io_channel_read_blk(fs->io, blk,
					     -block_nbytes, block_bitmap);
				if (retval) {
					retval = EXT2_ET_BLOCK_BITMAP_READ;
					goto cleanup;
				}
			} else
				memset(block_bitmap, 0, block_nbytes);
			block_bitmap += block_nbytes;
		}
		if (inode_bitmap) {
			blk = fs->group_desc[i].bg_inode_bitmap;
			if (blk) {
				retval = io_channel_read_blk(fs->io, blk,
					     -inode_nbytes, inode_bitmap);
				if (retval) {
					retval = EXT2_ET_INODE_BITMAP_READ;
					goto cleanup;
				}
			} else
				memset(inode_bitmap, 0, inode_nbytes);
			inode_bitmap += inode_nbytes;
		}
	}
	return 0;
	
cleanup:
	if (do_block) {
		free(fs->block_map);
		fs->block_map = 0;
	}
	if (do_inode) {
		free(fs->inode_map);
		fs->inode_map = 0;
	}
	if (buf)
		free(buf);
	return retval;
}

errcode_t ext2fs_read_inode_bitmap (ext2_filsys fs)
{
	return read_bitmaps(fs, 1, 0);
}

errcode_t ext2fs_read_block_bitmap(ext2_filsys fs)
{
	return read_bitmaps(fs, 0, 1);
}

errcode_t ext2fs_read_bitmaps(ext2_filsys fs)
{

	EXT2_CHECK_MAGIC(fs, EXT2_ET_MAGIC_EXT2FS_FILSYS);

	return read_bitmaps(fs, !fs->inode_map, !fs->block_map);
}

errcode_t ext2fs_write_bitmaps(ext2_filsys fs)
{
	errcode_t	retval;

	EXT2_CHECK_MAGIC(fs, EXT2_ET_MAGIC_EXT2FS_FILSYS);

	if (fs->block_map && ext2fs_test_bb_dirty(fs)) {
		retval = ext2fs_write_block_bitmap(fs);
		if (retval)
			return retval;
	}
	if (fs->inode_map && ext2fs_test_ib_dirty(fs)) {
		retval = ext2fs_write_inode_bitmap(fs);
		if (retval)
			return retval;
	}
	return 0;
}	

