/*
 * bitmaps.c --- routines to read, write, and manipulate the inode and
 * block bitmaps.
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
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#if HAVE_ERRNO_H
#include <errno.h>
#endif

#include <linux/ext2_fs.h>

#include "ext2fs.h"

static errcode_t make_bitmap(__u32 start, __u32 end, __u32 real_end,
			     const char *descr, char *init_map,
			     ext2fs_generic_bitmap *ret)
{
	ext2fs_generic_bitmap bitmap;
	int	size;

	bitmap = malloc(sizeof(struct ext2fs_struct_generic_bitmap));
	if (!bitmap)
		return ENOMEM;

	bitmap->magic = EXT2_ET_MAGIC_GENERIC_BITMAP;
	bitmap->fs = NULL;
	bitmap->start = start;
	bitmap->end = end;
	bitmap->real_end = real_end;
	bitmap->base_error_code = EXT2_ET_BAD_GENERIC_MARK;
	if (descr) {
		bitmap->description = malloc(strlen(descr)+1);
		if (!bitmap->description) {
			free(bitmap);
			return ENOMEM;
		}
		strcpy(bitmap->description, descr);
	} else
		bitmap->description = 0;

	size = ((bitmap->real_end - bitmap->start) / 8) + 1;
	bitmap->bitmap = malloc(size);
	if (!bitmap->bitmap) {
		free(bitmap->description);
		free(bitmap);
		return ENOMEM;
	}

	if (init_map)
		memcpy(bitmap->bitmap, init_map, size);
	else
		memset(bitmap->bitmap, 0, size);
	*ret = bitmap;
	return 0;
}

errcode_t ext2fs_allocate_generic_bitmap(__u32 start,
					 __u32 end,
					 __u32 real_end,
					 const char *descr,
					 ext2fs_generic_bitmap *ret)
{
	return make_bitmap(start, end, real_end, descr, 0, ret);
}

errcode_t ext2fs_copy_bitmap(ext2fs_generic_bitmap src,
			     ext2fs_generic_bitmap *dest)
{
	errcode_t		retval;
	ext2fs_generic_bitmap	new;

	retval = make_bitmap(src->start, src->end, src->real_end,
			     src->description, src->bitmap, &new);
	if (retval)
		return retval;
	new->magic = src->magic;
	new->fs = src->fs;
	new->base_error_code = src->base_error_code;
	return 0;
}


errcode_t ext2fs_allocate_inode_bitmap(ext2_filsys fs,
				       const char *descr,
				       ext2fs_inode_bitmap *ret)
{
	ext2fs_inode_bitmap bitmap;
	errcode_t	retval;
	__u32		start, end, real_end;

	EXT2_CHECK_MAGIC(fs, EXT2_ET_MAGIC_EXT2FS_FILSYS);

	fs->write_bitmaps = ext2fs_write_bitmaps;

	start = 1;
	end = fs->super->s_inodes_count;
	real_end = (EXT2_INODES_PER_GROUP(fs->super) * fs->group_desc_count);

	retval = ext2fs_allocate_generic_bitmap(start, end, real_end,
						descr, &bitmap);
	if (retval)
		return retval;
	
	bitmap->magic = EXT2_ET_MAGIC_INODE_BITMAP;
	bitmap->fs = fs;
	bitmap->base_error_code = EXT2_ET_BAD_INODE_MARK;
	
	*ret = bitmap;
	return 0;
}

errcode_t ext2fs_allocate_block_bitmap(ext2_filsys fs,
				       const char *descr,
				       ext2fs_block_bitmap *ret)
{
	ext2fs_block_bitmap bitmap;
	errcode_t	retval;
	__u32		start, end, real_end;

	EXT2_CHECK_MAGIC(fs, EXT2_ET_MAGIC_EXT2FS_FILSYS);

	fs->write_bitmaps = ext2fs_write_bitmaps;

	start = fs->super->s_first_data_block;
	end = fs->super->s_blocks_count-1;
	real_end = (EXT2_BLOCKS_PER_GROUP(fs->super)  
		    * fs->group_desc_count)-1 + start;
	
	retval = ext2fs_allocate_generic_bitmap(start, end, real_end,
						descr, &bitmap);
	if (retval)
		return retval;

	bitmap->magic = EXT2_ET_MAGIC_BLOCK_BITMAP;
	bitmap->fs = fs;
	bitmap->base_error_code = EXT2_ET_BAD_BLOCK_MARK;
	
	*ret = bitmap;
	return 0;
}

errcode_t ext2fs_fudge_inode_bitmap_end(ext2fs_inode_bitmap bitmap,
					ino_t end, ino_t *oend)
{
	EXT2_CHECK_MAGIC(bitmap, EXT2_ET_MAGIC_INODE_BITMAP);
	
	if (end > bitmap->real_end)
		return EXT2_ET_FUDGE_INODE_BITMAP_END;
	if (oend)
		*oend = bitmap->end;
	bitmap->end = end;
	return 0;
}

errcode_t ext2fs_fudge_block_bitmap_end(ext2fs_block_bitmap bitmap,
					blk_t end, blk_t *oend)
{
	EXT2_CHECK_MAGIC(bitmap, EXT2_ET_MAGIC_BLOCK_BITMAP);
	
	if (end > bitmap->real_end)
		return EXT2_ET_FUDGE_BLOCK_BITMAP_END;
	if (oend)
		*oend = bitmap->end;
	bitmap->end = end;
	return 0;
}

void ext2fs_clear_inode_bitmap(ext2fs_inode_bitmap bitmap)
{
	if (!bitmap || (bitmap->magic != EXT2_ET_MAGIC_INODE_BITMAP))
		return;

	memset(bitmap->bitmap, 0,
	       ((bitmap->real_end - bitmap->start) / 8) + 1);
}

void ext2fs_clear_block_bitmap(ext2fs_block_bitmap bitmap)
{
	if (!bitmap || (bitmap->magic != EXT2_ET_MAGIC_BLOCK_BITMAP))
		return;

	memset(bitmap->bitmap, 0,
	       ((bitmap->real_end - bitmap->start) / 8) + 1);
}
