/*
 * undelete.c --- routines to try to help a user recover a deleted file.
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
#include <sys/stat.h>

#include "debugfs.h"

struct deleted_info {
	ino_t	ino;
	unsigned short mode;
	unsigned short uid;
	unsigned long size;
	time_t	dtime;
	int	num_blocks;
	int	free_blocks;
};

struct lsdel_struct {
	ino_t			inode;
	int			num_blocks;
	int			free_blocks;
	int			bad_blocks;
};

static int deleted_info_compare(const void *a, const void *b)
{
	const struct deleted_info *arg1, *arg2;

	arg1 = (const struct deleted_info *) a;
	arg2 = (const struct deleted_info *) b;

	return arg1->dtime - arg2->dtime;
}

static int lsdel_proc(ext2_filsys fs,
		      blk_t	*block_nr,
		      int blockcnt,
		      void *private)
{
	struct lsdel_struct *lsd = (struct lsdel_struct *) private;

	lsd->num_blocks++;

	if (*block_nr < fs->super->s_first_data_block ||
	    *block_nr >= fs->super->s_blocks_count) {
		lsd->bad_blocks++;
		return BLOCK_ABORT;
	}

	if (!ext2fs_test_block_bitmap(fs->block_map,*block_nr))
		lsd->free_blocks++;

	return 0;
}

void do_lsdel(int argc, char **argv)
{
	struct lsdel_struct 	lsd;
	struct deleted_info	*delarray;
	int			num_delarray, max_delarray;
	ext2_inode_scan		scan = 0;
	ino_t			ino;
	struct ext2_inode	inode;
	errcode_t		retval;
	char			*block_buf;
	int			i;
	
	if (argc > 1) {
		com_err(argv[0], 0, "Usage: ls_deleted_inodes\n");
		return;
	}
	if (check_fs_open(argv[0]))
		return;

	max_delarray = 100;
	num_delarray = 0;
	delarray = malloc(max_delarray * sizeof(struct deleted_info));
	if (!delarray) {
		com_err("ls_deleted_inodes", ENOMEM,
			"while allocating deleted information storage");
		exit(1);
	}

	block_buf = malloc(current_fs->blocksize * 3);
	if (!block_buf) {
		com_err("ls_deleted_inodes", ENOMEM, "while allocating block buffer");
		goto error_out;
	}

	retval = ext2fs_open_inode_scan(current_fs, 0, &scan);
	if (retval) {
		com_err("ls_deleted_inodes", retval,
			"while opening inode scan");
		goto error_out;
	}

	retval = ext2fs_get_next_inode(scan, &ino, &inode);
	if (retval) {
		com_err("ls_deleted_inodes", retval,
			"while starting inode scan");
		goto error_out;
	}
	
	while (ino) {
		if (inode.i_dtime == 0)
			goto next;

		lsd.inode = ino;
		lsd.num_blocks = 0;
		lsd.free_blocks = 0;
		lsd.bad_blocks = 0;
		
		retval = ext2fs_block_iterate(current_fs, ino, 0, block_buf,
					      lsdel_proc, &lsd);
		if (retval) {
			com_err("ls_deleted_inodes", retval,
				"while calling ext2_block_iterate");
			goto next;
		}
		if (lsd.free_blocks && !lsd.bad_blocks) {
			if (num_delarray >= max_delarray) {
				max_delarray += 50;
				delarray = realloc(delarray,
			   max_delarray * sizeof(struct deleted_info));
				if (!delarray) {
					com_err("ls_deleted_inodes",
						ENOMEM,
						"while reallocating array");
					exit(1);
				}
			}
				
			delarray[num_delarray].ino = ino;
			delarray[num_delarray].mode = inode.i_mode;
			delarray[num_delarray].uid = inode.i_uid;
			delarray[num_delarray].size = inode.i_size;
			delarray[num_delarray].dtime = inode.i_dtime;
			delarray[num_delarray].num_blocks = lsd.num_blocks;
			delarray[num_delarray].free_blocks = lsd.free_blocks;
			num_delarray++;
		}
		
	next:
		retval = ext2fs_get_next_inode(scan, &ino, &inode);
		if (retval) {
			com_err("ls_deleted_inodes", retval,
				"while doing inode scan");
			goto error_out;
		}
	}

	printf("%d deleted inodes found.\n", num_delarray);
	printf(" Inode  Owner  Mode    Size    Blocks    Time deleted\n");
	
	qsort(delarray, num_delarray, sizeof(struct deleted_info),
	      deleted_info_compare);
	
	for (i = 0; i < num_delarray; i++) {
		printf("%6lu %6d %6o %6lu %4d/%4d %s", delarray[i].ino,
		       delarray[i].uid, delarray[i].mode, delarray[i].size,
		       delarray[i].free_blocks, delarray[i].num_blocks, 
		       time_to_string(delarray[i].dtime));
	}
	
error_out:
	free(block_buf);
	free(delarray);
	if (scan)
		ext2fs_close_inode_scan(scan);
	return;
}



