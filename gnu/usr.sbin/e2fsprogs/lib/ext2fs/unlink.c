/*
 * unlink.c --- delete links in a ext2fs directory
 * 
 * Copyright (C) 1993, 1994, 1997 Theodore Ts'o.
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

static int unlink_proc(struct ext2_dir_entry *dirent,
		     int	offset,
		     int	blocksize,
		     char	*buf,
		     void	*private)
{
	struct link_struct *ls = (struct link_struct *) private;

	if (ls->name && (dirent->name_len != ls->namelen))
		return 0;
	if (ls->name && strncmp(ls->name, dirent->name, dirent->name_len))
		return 0;
	if (ls->inode && (dirent->inode != ls->inode))
		return 0;

	dirent->inode = 0;
	ls->done++;
	return DIRENT_ABORT|DIRENT_CHANGED;
}

errcode_t ext2fs_unlink(ext2_filsys fs, ino_t dir, const char *name, ino_t ino,
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

	retval = ext2fs_dir_iterate(fs, dir, 0, 0, unlink_proc, &ls);
	if (retval)
		return retval;

	return (ls.done) ? 0 : EXT2_ET_DIR_NO_SPACE;
}

