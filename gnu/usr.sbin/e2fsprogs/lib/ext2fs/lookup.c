/*
 * lookup.c --- ext2fs directory lookup operations
 * 
 * Copyright (C) 1993, 1994, 1994, 1995 Theodore Ts'o.
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
#if HAVE_ERRNO_H
#include <errno.h>
#endif

#include <linux/ext2_fs.h>

#include "ext2fs.h"

struct lookup_struct  {
	const char	*name;
	int		len;
	ino_t		*inode;
	int		found;
};	

static int lookup_proc(struct ext2_dir_entry *dirent,
		       int	offset,
		       int	blocksize,
		       char	*buf,
		       void	*private)
{
	struct lookup_struct *ls = (struct lookup_struct *) private;

	if (ls->len != dirent->name_len)
		return 0;
	if (strncmp(ls->name, dirent->name, dirent->name_len))
		return 0;
	*ls->inode = dirent->inode;
	ls->found++;
	return DIRENT_ABORT;
}


errcode_t ext2fs_lookup(ext2_filsys fs, ino_t dir, const char *name,
			int namelen, char *buf, ino_t *inode)
{
	errcode_t	retval;
	struct lookup_struct ls;

	EXT2_CHECK_MAGIC(fs, EXT2_ET_MAGIC_EXT2FS_FILSYS);

	ls.name = name;
	ls.len = namelen;
	ls.inode = inode;
	ls.found = 0;

	retval = ext2fs_dir_iterate(fs, dir, 0, buf, lookup_proc, &ls);
	if (retval)
		return retval;

	return (ls.found) ? 0 : ENOENT;
}


