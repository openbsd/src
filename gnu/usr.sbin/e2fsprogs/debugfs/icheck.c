/*
 * icheck.c --- given a list of blocks, generate a list of inodes
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

struct block_info {
	blk_t	blk;
	ino_t	ino;
};

struct block_walk_struct {
	struct block_info	*barray;
	int			blocks_left;
	int			num_blocks;
	ino_t			inode;
};

static int icheck_proc(ext2_filsys fs,
		       blk_t	*block_nr,
		       int blockcnt,
		       void *private)
{
	struct block_walk_struct *bw = (struct block_walk_struct *) private;
	int	i;

	for (i=0; i < bw->num_blocks; i++) {
		if (bw->barray[i].blk == *block_nr) {
			bw->barray[i].ino = bw->inode;
			bw->blocks_left--;
		}
	}
	if (!bw->blocks_left)
		return BLOCK_ABORT;
	
	return 0;
}

void do_icheck(int argc, char **argv)
{
	struct block_walk_struct bw;
	struct block_info	*binfo;
	int			i;
	ext2_inode_scan		scan = 0;
	ino_t			ino;
	struct ext2_inode	inode;
	errcode_t		retval;
	char			*tmp;
	char			*block_buf;
	
	if (argc < 2) {
		com_err(argv[0], 0, "Usage: icheck <block number> ...");
		return;
	}
	if (check_fs_open(argv[0]))
		return;

	bw.barray = malloc(sizeof(struct block_info) * argc);
	if (!bw.barray) {
		com_err("icheck", ENOMEM,
			"while allocating inode info array");
		return;
	}
	memset(bw.barray, 0, sizeof(struct block_info) * argc);

	block_buf = malloc(current_fs->blocksize * 3);
	if (!block_buf) {
		com_err("icheck", ENOMEM, "while allocating block buffer");
		goto error_out;
	}

	for (i=1; i < argc; i++) {
		bw.barray[i-1].blk = strtol(argv[i], &tmp, 0);
		if (*tmp) {
			com_err(argv[0], 0, "Bad block - %s", argv[i]);
			return;
		}
	}

	bw.num_blocks = bw.blocks_left = argc-1;

	retval = ext2fs_open_inode_scan(current_fs, 0, &scan);
	if (retval) {
		com_err("icheck", retval, "while opening inode scan");
		goto error_out;
	}

	retval = ext2fs_get_next_inode(scan, &ino, &inode);
	if (retval) {
		com_err("icheck", retval, "while starting inode scan");
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

		bw.inode = ino;
		
		retval = ext2fs_block_iterate(current_fs, ino, 0, block_buf,
					      icheck_proc, &bw);
		if (retval) {
			com_err("icheck", retval,
				"while calling ext2_block_iterate");
			goto next;
		}

		if (bw.blocks_left == 0)
			break;

	next:
		retval = ext2fs_get_next_inode(scan, &ino, &inode);
		if (retval) {
			com_err("icheck", retval,
				"while doing inode scan");
			goto error_out;
		}
	}

	printf("Block\tInode number\n");
	for (i=0, binfo = bw.barray; i < bw.num_blocks; i++, binfo++) {
		if (binfo->ino == 0) {
			printf("%u\t<block not found>\n", binfo->blk);
			continue;
		}
		printf("%u\t%ld\n", binfo->blk, binfo->ino);
	}

error_out:
	free(bw.barray);
	free(block_buf);
	if (scan)
		ext2fs_close_inode_scan(scan);
	return;
}
