/*
 * dupfs.c --- duplicate a ext2 filesystem handle
 * 
 * Copyright (C) 1997 Theodore Ts'o.
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

errcode_t ext2fs_dup_handle(ext2_filsys src, ext2_filsys *dest)
{
	ext2_filsys	fs;
	errcode_t	retval;

	EXT2_CHECK_MAGIC(src, EXT2_ET_MAGIC_EXT2FS_FILSYS);
	
	fs = (ext2_filsys) malloc(sizeof(struct struct_ext2_filsys));
	if (!fs)
		return ENOMEM;

	*fs = *src;
	fs->device_name = 0;
	fs->super = 0;
	fs->group_desc = 0;
	fs->inode_map = 0;
	fs->block_map = 0;
	fs->badblocks = 0;
	fs->dblist = 0;

	io_channel_bumpcount(fs->io);
	if (fs->icache)
		fs->icache->refcount++;

	retval = ENOMEM;
	fs->device_name = malloc(strlen(src->device_name)+1);
	if (!fs->device_name)
		goto errout;
	strcpy(fs->device_name, src->device_name);

	fs->super = malloc(SUPERBLOCK_SIZE);
	if (!fs->super)
		goto errout;
	memcpy(fs->super, src->super, SUPERBLOCK_SIZE);

	fs->group_desc = malloc(fs->desc_blocks * fs->blocksize);
	if (!fs->group_desc)
		goto errout;
	memcpy(fs->group_desc, src->group_desc,
	       fs->desc_blocks * fs->blocksize);

	if (src->inode_map) {
		retval = ext2fs_copy_bitmap(src->inode_map, &fs->inode_map);
		if (retval)
			goto errout;
	}
	if (src->block_map) {
		retval = ext2fs_copy_bitmap(src->block_map, &fs->block_map);
		if (retval)
			goto errout;
	}
	if (src->badblocks) {
		retval = ext2fs_badblocks_copy(src->badblocks, &fs->badblocks);
		if (retval)
			goto errout;
	}
	if (src->dblist) {
		retval = ext2fs_copy_dblist(src->dblist, &fs->dblist);
		if (retval)
			goto errout;
	}
	*dest = fs;
	return 0;
errout:
	ext2fs_free(fs);
	return retval;
	
}

