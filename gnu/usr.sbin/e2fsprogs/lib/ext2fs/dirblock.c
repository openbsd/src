/*
 * dirblock.c --- directory block routines.
 * 
 * Copyright (C) 1995, 1996 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include <linux/ext2_fs.h>

#include "ext2fs.h"

errcode_t ext2fs_read_dir_block(ext2_filsys fs, blk_t block,
				void *buf)
{
	errcode_t	retval;
	char		*p, *end;
	struct ext2_dir_entry *dirent;

 	retval = io_channel_read_blk(fs->io, block, 1, buf);
	if (retval)
		return retval;
	if ((fs->flags & (EXT2_FLAG_SWAP_BYTES|
			  EXT2_FLAG_SWAP_BYTES_READ)) == 0)
		return 0;
	p = buf;
	end = (char *) buf + fs->blocksize;
	while (p < end) {
		dirent = (struct ext2_dir_entry *) p;
		dirent->inode = ext2fs_swab32(dirent->inode);
		dirent->rec_len = ext2fs_swab16(dirent->rec_len);
		dirent->name_len = ext2fs_swab16(dirent->name_len);
		p += (dirent->rec_len < 8) ? 8 : dirent->rec_len;
	}
	return 0;
}

errcode_t ext2fs_write_dir_block(ext2_filsys fs, blk_t block,
				 void *inbuf)
{
	errcode_t	retval;
	char		*p, *end, *write_buf;
	char		*buf = 0;
	struct ext2_dir_entry *dirent;

	if ((fs->flags & EXT2_FLAG_SWAP_BYTES) ||
	    (fs->flags & EXT2_FLAG_SWAP_BYTES_WRITE)) {
		write_buf = buf = malloc(fs->blocksize);
		if (!buf)
			return ENOMEM;
		memcpy(buf, inbuf, fs->blocksize);
		p = buf;
		end = buf + fs->blocksize;
		while (p < end) {
			dirent = (struct ext2_dir_entry *) p;
			p += (dirent->rec_len < 8) ? 8 : dirent->rec_len;
			dirent->inode = ext2fs_swab32(dirent->inode);
			dirent->rec_len = ext2fs_swab16(dirent->rec_len);
			dirent->name_len = ext2fs_swab16(dirent->name_len);
		}
	} else
		write_buf = inbuf;
 	retval = io_channel_write_blk(fs->io, block, 1, write_buf);
	if (buf)
		free(buf);
	return retval;
}


