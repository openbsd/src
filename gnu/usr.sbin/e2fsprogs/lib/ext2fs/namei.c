/*
 * namei.c --- ext2fs directory lookup operations
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

/* #define NAMEI_DEBUG */

#include <linux/ext2_fs.h>

#include "ext2fs.h"

static errcode_t open_namei(ext2_filsys fs, ino_t root, ino_t base,
			    const char *pathname, int pathlen, int follow,
			    int link_count, char *buf, ino_t *res_inode);

static errcode_t follow_link(ext2_filsys fs, ino_t root, ino_t dir,
			     ino_t inode, int link_count,
			     char *buf, ino_t *res_inode)
{
	char *pathname;
	char *buffer = 0;
	errcode_t retval;
	struct ext2_inode ei;

#ifdef NAMEI_DEBUG
	printf("follow_link: root=%lu, dir=%lu, inode=%lu, lc=%d\n",
	       root, dir, inode, link_count);
	
#endif
	retval = ext2fs_read_inode (fs, inode, &ei);
	if (retval) return retval;
	if (!LINUX_S_ISLNK (ei.i_mode)) {
		*res_inode = inode;
		return 0;
	}
	if (link_count++ > 5) {
		return EXT2_ET_SYMLINK_LOOP;
	}
	if (ei.i_blocks) {
		buffer = malloc (fs->blocksize);
		if (!buffer)
			return ENOMEM;
		retval = io_channel_read_blk(fs->io, ei.i_block[0], 1, buffer);
		if (retval) {
			free(buffer);
			return retval;
		}
		pathname = buffer;
	} else
		pathname = (char *)&(ei.i_block[0]);
	retval = open_namei(fs, root, dir, pathname, ei.i_size, 1,
			    link_count, buf, res_inode);
	if (buffer)
		free (buffer);
	return retval;
}

/*
 * This routine interprets a pathname in the context of the current
 * directory and the root directory, and returns the inode of the
 * containing directory, and a pointer to the filename of the file
 * (pointing into the pathname) and the length of the filename.
 */
static errcode_t dir_namei(ext2_filsys fs, ino_t root, ino_t dir,
			   const char *pathname, int pathlen,
			   int link_count, char *buf,
			   const char **name, int *namelen, ino_t *res_inode)
{
	char c;
	const char *thisname;
	int len;
	ino_t inode;
	errcode_t retval;

	if ((c = *pathname) == '/') {
        	dir = root;
		pathname++;
		pathlen--;
	}
	while (1) {
        	thisname = pathname;
		for (len=0; --pathlen >= 0;len++) {
			c = *(pathname++);
			if (c == '/')
				break;
		}
		if (pathlen < 0)
			break;
		retval = ext2fs_lookup (fs, dir, thisname, len, buf, &inode);
		if (retval) return retval;
        	retval = follow_link (fs, root, dir, inode,
				      link_count, buf, &dir);
        	if (retval) return retval;
    	}
	*name = thisname;
	*namelen = len;
	*res_inode = dir;
	return 0;
}

static errcode_t open_namei(ext2_filsys fs, ino_t root, ino_t base,
			    const char *pathname, int pathlen, int follow,
			    int link_count, char *buf, ino_t *res_inode)
{
	const char *basename;
	int namelen;
	ino_t dir, inode;
	errcode_t retval;

#ifdef NAMEI_DEBUG
	printf("open_namei: root=%lu, dir=%lu, path=%*s, lc=%d\n",
	       root, base, pathlen, pathname, link_count);
#endif
	retval = dir_namei(fs, root, base, pathname, pathlen,
			   link_count, buf, &basename, &namelen, &dir);
	if (retval) return retval;
	if (!namelen) {                     /* special case: '/usr/' etc */
		*res_inode=dir;
		return 0;
	}
	retval = ext2fs_lookup (fs, dir, basename, namelen, buf, &inode);
	if (retval)
		return retval;
	if (follow) {
		retval = follow_link(fs, root, dir, inode, link_count,
				     buf, &inode);
		if (retval)
			return retval;
	}
#ifdef NAMEI_DEBUG
	printf("open_namei: (link_count=%d) returns %lu\n",
	       link_count, inode);
#endif
	*res_inode = inode;
	return 0;
}

errcode_t ext2fs_namei(ext2_filsys fs, ino_t root, ino_t cwd,
		       const char *name, ino_t *inode)
{
	char *buf;
	errcode_t retval;
	
	EXT2_CHECK_MAGIC(fs, EXT2_ET_MAGIC_EXT2FS_FILSYS);

	buf = malloc(fs->blocksize);
	if (!buf)
		return ENOMEM;
	
	retval = open_namei(fs, root, cwd, name, strlen(name), 0, 0,
			    buf, inode);

	free(buf);
	return retval;
}

errcode_t ext2fs_namei_follow(ext2_filsys fs, ino_t root, ino_t cwd,
			      const char *name, ino_t *inode)
{
	char *buf;
	errcode_t retval;
	
	EXT2_CHECK_MAGIC(fs, EXT2_ET_MAGIC_EXT2FS_FILSYS);

	buf = malloc(fs->blocksize);
	if (!buf)
		return ENOMEM;
	
	retval = open_namei(fs, root, cwd, name, strlen(name), 1, 0,
			    buf, inode);

	free(buf);
	return retval;
}

extern errcode_t ext2fs_follow_link(ext2_filsys fs, ino_t root, ino_t cwd,
			ino_t inode, ino_t *res_inode)
{
	char *buf;
	errcode_t retval;
	
	EXT2_CHECK_MAGIC(fs, EXT2_ET_MAGIC_EXT2FS_FILSYS);

	buf = malloc(fs->blocksize);
	if (!buf)
		return ENOMEM;
	
	retval = follow_link(fs, root, cwd, inode, 0, buf, res_inode);

	free(buf);
	return retval;
}

