/*
 * ncheck.c --- given a list of inodes, generate a list of names
 * 
 * Copyright (C) 1994 Theodore Ts'o.  This file may be redistributed
 * under the terms of the GNU Public License.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include <sys/types.h>

#include "debugfs.h"

struct inode_info {
	ino_t	ino;
	ino_t	parent;
	char	*pathname;
};

struct inode_walk_struct {
	struct inode_info	*iarray;
	int			inodes_left;
	int			num_inodes;
	int			position;
	ino_t			parent;
};

static int ncheck_proc(struct ext2_dir_entry *dirent,
		       int	offset,
		       int	blocksize,
		       char	*buf,
		       void	*private)
{
	struct inode_walk_struct *iw = (struct inode_walk_struct *) private;
	int	i;

	iw->position++;
	if (iw->position <= 2)
		return 0;
	for (i=0; i < iw->num_inodes; i++) {
		if (iw->iarray[i].ino == dirent->inode) {
			iw->iarray[i].parent = iw->parent;
			iw->inodes_left--;
		}
	}
	if (!iw->inodes_left)
		return DIRENT_ABORT;
	
	return 0;
}

void do_ncheck(int argc, char **argv)
{
	struct inode_walk_struct iw;
	struct inode_info	*iinfo;
	int			i;
	ext2_inode_scan		scan = 0;
	ino_t			ino;
	struct ext2_inode	inode;
	errcode_t		retval;
	char			*tmp;
	
	if (argc < 2) {
		com_err(argv[0], 0, "Usage: ncheck <inode number> ...");
		return;
	}
	if (check_fs_open(argv[0]))
		return;

	iw.iarray = malloc(sizeof(struct inode_info) * argc);
	if (!iw.iarray) {
		com_err("do_ncheck", ENOMEM,
			"while allocating inode info array");
		return;
	}
	memset(iw.iarray, 0, sizeof(struct inode_info) * argc);

	for (i=1; i < argc; i++) {
		iw.iarray[i-1].ino = strtol(argv[i], &tmp, 0);
		if (*tmp) {
			com_err(argv[0], 0, "Bad inode - %s", argv[i]);
			return;
		}
	}

	iw.num_inodes = iw.inodes_left = argc-1;

	retval = ext2fs_open_inode_scan(current_fs, 0, &scan);
	if (retval) {
		com_err("ncheck", retval, "while opening inode scan");
		goto error_out;
	}

	retval = ext2fs_get_next_inode(scan, &ino, &inode);
	if (retval) {
		com_err("ncheck", retval, "while starting inode scan");
		goto error_out;
	}
	
	while (ino) {
		if (!inode.i_links_count)
			goto next;
		/*
		 * To handle filesystems touched by 0.3c extfs; can be
		 * removed later.
		 */
		if (inode.i_dtime)
			goto next;
		/* Ignore anything that isn't a directory */
		if (!LINUX_S_ISDIR(inode.i_mode))
			goto next;

		iw.position = 0;
		iw.parent = ino;
		
		retval = ext2fs_dir_iterate(current_fs, ino, 0, 0,
					    ncheck_proc, &iw);
		if (retval) {
			com_err("ncheck", retval,
				"while calling ext2_dir_iterate");
			goto next;
		}

		if (iw.inodes_left == 0)
			break;

	next:
		retval = ext2fs_get_next_inode(scan, &ino, &inode);
		if (retval) {
			com_err("ncheck", retval,
				"while doing inode scan");
			goto error_out;
		}
	}

	for (i=0, iinfo = iw.iarray; i < iw.num_inodes; i++, iinfo++) {
		if (iinfo->parent == 0)
			continue;
		retval = ext2fs_get_pathname(current_fs, iinfo->parent,
					     iinfo->ino, &iinfo->pathname);
		if (retval)
			com_err("ncheck", retval,
				"while resolving pathname for inode %d (%d)",
				iinfo->parent, iinfo->ino);
	}
	
	printf("Inode\tPathname\n");
	for (i=0, iinfo = iw.iarray; i < iw.num_inodes; i++, iinfo++) {
		if (iinfo->parent == 0) {
			printf("%ld\t<inode not found>\n", iinfo->ino);
			continue;
		}
		printf("%ld\t%s\n", iinfo->ino, iinfo->pathname ?
		       iinfo->pathname : "<unknown pathname>");
		if (iinfo->pathname)
			free(iinfo->pathname);
	}

error_out:
	free(iw.iarray);
	if (scan)
		ext2fs_close_inode_scan(scan);
	return;
}



