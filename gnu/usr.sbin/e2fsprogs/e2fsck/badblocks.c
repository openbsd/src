/*
 * badblocks.c --- replace/append bad blocks to the bad block inode
 * 
 * Copyright (C) 1993, 1994 Theodore Ts'o.  This file may be
 * redistributed under the terms of the GNU Public License.
 */

#include <time.h>
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include <et/com_err.h>
#include "e2fsck.h"

static int check_bb_inode_blocks(ext2_filsys fs, blk_t *block_nr, int blockcnt,
				 void *private);


static void invalid_block(ext2_filsys fs, blk_t blk)
{
	printf("Bad block %u out of range; ignored.\n", blk);
	return;
}

void read_bad_blocks_file(ext2_filsys fs, const char *bad_blocks_file,
			  int replace_bad_blocks)
{
	errcode_t	retval;
	badblocks_list	bb_list = 0;
	FILE		*f;
	char		buf[1024];

	read_bitmaps(fs);

	/*
	 * Make sure the bad block inode is sane.  If there are any
	 * illegal blocks, clear them.
	 */
	retval = ext2fs_block_iterate(fs, EXT2_BAD_INO, 0, 0,
				      check_bb_inode_blocks, 0);
	if (retval) {
		com_err("ext2fs_block_iterate", retval,
			"while sanity checking the bad blocks inode");
		fatal_error(0);
	}
	
	/*
	 * If we're appending to the bad blocks inode, read in the
	 * current bad blocks.
	 */
	if (!replace_bad_blocks) {
		retval = ext2fs_read_bb_inode(fs, &bb_list);
		if (retval) {
			com_err("ext2fs_read_bb_inode", retval,
				"while reading the bad blocks inode");
			fatal_error(0);
		}
	}
	
	/*
	 * Now read in the bad blocks from the file; if
	 * bad_blocks_file is null, then try to run the badblocks
	 * command.
	 */
	if (bad_blocks_file) {
		f = fopen(bad_blocks_file, "r");
		if (!f) {
			com_err("read_bad_blocks_file", errno,
				"while trying to open %s", bad_blocks_file);
			fatal_error(0);
		}
	} else {
		sprintf(buf, "badblocks %s%s %d", preen ? "" : "-s ",
			fs->device_name,
			fs->super->s_blocks_count);
		f = popen(buf, "r");
		if (!f) {
			com_err("read_bad_blocks_file", errno,
				"while trying popen '%s'", buf);
			fatal_error(0);
		}
	}
	retval = ext2fs_read_bb_FILE(fs, f, &bb_list, invalid_block);
	if (bad_blocks_file) 
		fclose(f);
	else
		pclose(f);
	if (retval) {
		com_err("ext2fs_read_bb_FILE", retval,
			"while reading in list of bad blocks from file");
		fatal_error(0);
	}
	
	/*
	 * Finally, update the bad blocks from the bad_block_map
	 */
	retval = ext2fs_update_bb_inode(fs, bb_list);
	if (retval) {
		com_err("ext2fs_update_bb_inode", retval,
			"while updating bad block inode");
		fatal_error(0);
	}

	badblocks_list_free(bb_list);
	return;
}

void test_disk(ext2_filsys fs)
{
	read_bad_blocks_file(fs, 0, 1);
}

static int check_bb_inode_blocks(ext2_filsys fs, blk_t *block_nr, int blockcnt,
				 void *private)
{
	if (!*block_nr)
		return 0;

	/*
	 * If the block number is outrageous, clear it and ignore it.
	 */
	if (*block_nr >= fs->super->s_blocks_count ||
	    *block_nr < fs->super->s_first_data_block) {
		printf("Warning illegal block %u found in bad block inode.  Cleared.\n", *block_nr);
		*block_nr = 0;
		return BLOCK_CHANGED;
	}

	return 0;
}

