/*
 * link.c --- create links in a ext2fs directory
 * 
 * Copyright (C) 1993, 1994 Theodore Ts'o.
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

#include <linux/ext2_fs.h>

#include "ext2fs.h"

struct link_struct  {
	const char	*name;
	int		namelen;
	ino_t		inode;
	int		flags;
	int		done;
};	

static int link_proc(struct ext2_dir_entry *dirent,
		     int	offset,
		     int	blocksize,
		     char	*buf,
		     void	*private)
{
	struct link_struct *ls = (struct link_struct *) private;
	struct ext2_dir_entry *next;
	int rec_len;
	int ret = 0;

	rec_len = EXT2_DIR_REC_LEN(ls->namelen);

	/*
	 * See if the following directory entry (if any) is unused;
	 * if so, absorb it into this one.
	 */
	next = (struct ext2_dir_entry *) (buf + offset + dirent->rec_len);
	if ((offset + dirent->rec_len < blocksize - 8) &&
	    (next->inode == 0) &&
	    (offset + dirent->rec_len + next->rec_len <= blocksize)) {
		dirent->rec_len += next->rec_len;
		ret = DIRENT_CHANGED;
	}

	/*
	 * If the directory entry is used, see if we can split the
	 * directory entry to make room for the new name.  If so,
	 * truncate it and return.
	 */
	if (dirent->inode) {
		if (dirent->rec_len < (EXT2_DIR_REC_LEN(dirent->name_len) +
				       rec_len))
			return ret;
		rec_len = dirent->rec_len - EXT2_DIR_REC_LEN(dirent->name_len);
		dirent->rec_len = EXT2_DIR_REC_LEN(dirent->name_len);
		next = (struct ext2_dir_entry *) (buf + offset +
						  dirent->rec_len);
		next->inode = 0;
		next->name_len = 0;
		next->rec_len = rec_len;
		return DIRENT_CHANGED;
	}

	/*
	 * If we get this far, then the directory entry is not used.
	 * See if we can fit the request entry in.  If so, do it.
	 */
	if (dirent->rec_len < rec_len)
		return ret;
	dirent->inode = ls->inode;
	dirent->name_len = ls->namelen;
	strncpy(dirent->name, ls->name, ls->namelen);

	ls->done++;
	return DIRENT_ABORT|DIRENT_CHANGED;
}

errcode_t ext2fs_link(ext2_filsys fs, ino_t dir, const char *name, ino_t ino,
		      int flags)
{
	errcode_t	retval;
	struct link_struct ls;

	EXT2_CHECK_MAGIC(fs, EXT2_ET_MAGIC_EXT2FS_FILSYS);

	if (!(fs->flags & EXT2_FLAG_RW))
		return EXT2_ET_RO_FILSYS;

	ls.name = name;
	ls.namelen = name ? strlen(name) : 0;
	ls.inode = ino;
	ls.flags = 0;
	ls.done = 0;

	retval = ext2fs_dir_iterate(fs, dir, DIRENT_FLAG_INCLUDE_EMPTY,
				    0, link_proc, &ls);
	if (retval)
		return retval;

	return (ls.done) ? 0 : EXT2_ET_DIR_NO_SPACE;
}
