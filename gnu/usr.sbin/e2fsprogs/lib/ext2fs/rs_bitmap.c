/*
 * rs_bitmap.c --- routine for changing the size of a bitmap
 *
 * Copyright (C) 1996, 1997 Theodore Ts'o.
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

errcode_t ext2fs_resize_generic_bitmap(__u32 new_end, __u32 new_real_end,
				       ext2fs_generic_bitmap bmap)
{
	size_t	size, new_size;
	char 	*new_bitmap;

	if (!bmap)
		return EINVAL;

	EXT2_CHECK_MAGIC(bmap, EXT2_ET_MAGIC_GENERIC_BITMAP);
	
	if (new_real_end == bmap->real_end) {
		bmap->end = new_end;
		return 0;
	}
	
	size = ((bmap->real_end - bmap->start) / 8) + 1;
	new_size = ((new_real_end - bmap->start) / 8) + 1;

	new_bitmap = realloc(bmap->bitmap, new_size);
	if (!new_bitmap)
		return ENOMEM;
	if (new_size > size)
		memset(new_bitmap + size, 0, new_size - size);

	bmap->bitmap = new_bitmap;
	bmap->end = new_end;
	bmap->real_end = new_real_end;
	return 0;
}

errcode_t ext2fs_resize_inode_bitmap(__u32 new_end, __u32 new_real_end,
				     ext2fs_inode_bitmap bmap)
{
	errcode_t	retval;
	
	if (!bmap)
		return EINVAL;

	EXT2_CHECK_MAGIC(bmap, EXT2_ET_MAGIC_INODE_BITMAP);

	bmap->magic = EXT2_ET_MAGIC_GENERIC_BITMAP;
	retval = ext2fs_resize_generic_bitmap(new_end, new_real_end,
					      bmap);
	bmap->magic = EXT2_ET_MAGIC_INODE_BITMAP;
	return retval;
}

errcode_t ext2fs_resize_block_bitmap(__u32 new_end, __u32 new_real_end,
				     ext2fs_block_bitmap bmap)
{
	errcode_t	retval;
	
	if (!bmap)
		return EINVAL;

	EXT2_CHECK_MAGIC(bmap, EXT2_ET_MAGIC_BLOCK_BITMAP);

	bmap->magic = EXT2_ET_MAGIC_GENERIC_BITMAP;
	retval = ext2fs_resize_generic_bitmap(new_end, new_real_end,
					      bmap);
	bmap->magic = EXT2_ET_MAGIC_BLOCK_BITMAP;
	return retval;
}

