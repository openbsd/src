/*
 * bb_inode.c --- routines to update the bad block inode.
 * 
 * WARNING: This routine modifies a lot of state in the filesystem; if
 * this routine returns an error, the bad block inode may be in an
 * inconsistent state.
 * 
 * Copyright (C) 1994, 1995 Theodore Ts'o.
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
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#if HAVE_ERRNO_H
#include <errno.h>
#endif

#include <linux/ext2_fs.h>

#include "ext2fs.h"

struct set_badblock_record {
	ext2_badblocks_iterate	bb_iter;
	int		bad_block_count;
	blk_t		*ind_blocks;
	int		max_ind_blocks;
	int		ind_blocks_size;
	int		ind_blocks_ptr;
	char		*block_buf;
	errcode_t	err;
};

static int set_bad_block_proc(ext2_filsys fs, blk_t *block_nr, int blockcnt,
			      blk_t ref_block, int ref_offset, void *private);
static int clear_bad_block_proc(ext2_filsys fs, blk_t *block_nr, int blockcnt,
				blk_t ref_block, int ref_offset,
				void *private);
	
/*
 * Given a bad blocks bitmap, update the bad blocks inode to reflect
 * the map.
 */
errcode_t ext2fs_update_bb_inode(ext2_filsys fs, ext2_badblocks_list bb_list)
{
	errcode_t			retval;
	struct set_badblock_record 	rec;
	struct ext2_inode		inode;
	blk_t				blk;
	
	EXT2_CHECK_MAGIC(fs, EXT2_ET_MAGIC_EXT2FS_FILSYS);

	if (!fs->block_map)
		return EXT2_ET_NO_BLOCK_BITMAP;
	
	rec.bad_block_count = 0;
	rec.ind_blocks_size = rec.ind_blocks_ptr = 0;
	rec.max_ind_blocks = 10;
	rec.ind_blocks = malloc(rec.max_ind_blocks * sizeof(blk_t));
	if (!rec.ind_blocks)
		return ENOMEM;
	memset(rec.ind_blocks, 0, rec.max_ind_blocks * sizeof(blk_t));
	rec.block_buf = malloc(fs->blocksize);
	if (!rec.block_buf) {
		retval = ENOMEM;
		goto cleanup;
	}
	memset(rec.block_buf, 0, fs->blocksize);
	rec.err = 0;
	
	/*
	 * First clear the old bad blocks (while saving the indirect blocks) 
	 */
	retval = ext2fs_block_iterate2(fs, EXT2_BAD_INO,
				       BLOCK_FLAG_DEPTH_TRAVERSE, 0,
				       clear_bad_block_proc, &rec);
	if (retval)
		goto cleanup;
	if (rec.err) {
		retval = rec.err;
		goto cleanup;
	}
	
	/*
	 * Now set the bad blocks!
	 *
	 * First, mark the bad blocks as used.  This prevents a bad
	 * block from being used as an indirecto block for the bad
	 * block inode (!).
	 */
	if (bb_list) {
		retval = ext2fs_badblocks_list_iterate_begin(bb_list,
							     &rec.bb_iter);
		if (retval)
			goto cleanup;
		while (ext2fs_badblocks_list_iterate(rec.bb_iter, &blk)) {
			ext2fs_mark_block_bitmap(fs->block_map, blk); 
		}
		ext2fs_badblocks_list_iterate_end(rec.bb_iter);
		ext2fs_mark_bb_dirty(fs);
		
		retval = ext2fs_badblocks_list_iterate_begin(bb_list,
							     &rec.bb_iter);
		if (retval)
			goto cleanup;
		retval = ext2fs_block_iterate2(fs, EXT2_BAD_INO,
					       BLOCK_FLAG_APPEND, 0,
					       set_bad_block_proc, &rec);
		ext2fs_badblocks_list_iterate_end(rec.bb_iter);
		if (retval) 
			goto cleanup;
		if (rec.err) {
			retval = rec.err;
			goto cleanup;
		}
	}
	
	/*
	 * Update the bad block inode's mod time and block count
	 * field.  
	 */
	retval = ext2fs_read_inode(fs, EXT2_BAD_INO, &inode);
	if (retval)
		goto cleanup;
	
	inode.i_atime = inode.i_mtime = time(0);
	if (!inode.i_ctime)
		inode.i_ctime = time(0);
	inode.i_blocks = rec.bad_block_count * (fs->blocksize / 512);
	inode.i_size = rec.bad_block_count * fs->blocksize;

	retval = ext2fs_write_inode(fs, EXT2_BAD_INO, &inode);
	if (retval)
		goto cleanup;
	
cleanup:
	free(rec.ind_blocks);
	free(rec.block_buf);
	return retval;
}

/*
 * Helper function for update_bb_inode()
 *
 * Clear the bad blocks in the bad block inode, while saving the
 * indirect blocks.
 */
static int clear_bad_block_proc(ext2_filsys fs, blk_t *block_nr, int blockcnt,
				blk_t ref_block, int ref_offset, void *private)
{
	struct set_badblock_record *rec = (struct set_badblock_record *)
		private;
	int	group;

	if (!*block_nr)
		return 0;

	/*
	 * If the block number is outrageous, clear it and ignore it.
	 */
	if (*block_nr >= fs->super->s_blocks_count ||
	    *block_nr < fs->super->s_first_data_block) {
		*block_nr = 0;
		return BLOCK_CHANGED;
	}

	if (blockcnt < 0) {
		if (rec->ind_blocks_size >= rec->max_ind_blocks) {
			rec->max_ind_blocks += 10;
			rec->ind_blocks = realloc(rec->ind_blocks,
						  rec->max_ind_blocks *
						  sizeof(blk_t));
			if (!rec->ind_blocks) {
				rec->err = ENOMEM;
				return BLOCK_ABORT;
			}
		}
		rec->ind_blocks[rec->ind_blocks_size++] = *block_nr;
	}

	/*
	 * Mark the block as unused, and update accounting information
	 */
	ext2fs_unmark_block_bitmap(fs->block_map, *block_nr);
	ext2fs_mark_bb_dirty(fs);
	group = ext2fs_group_of_blk(fs, *block_nr);
	fs->group_desc[group].bg_free_blocks_count++;
	fs->super->s_free_blocks_count++;
	ext2fs_mark_super_dirty(fs);
	
	*block_nr = 0;
	return BLOCK_CHANGED;
}

	
/*
 * Helper function for update_bb_inode()
 *
 * Set the block list in the bad block inode, using the supplied bitmap.
 */
static int set_bad_block_proc(ext2_filsys fs, blk_t *block_nr,
			      int blockcnt, blk_t ref_block, 
			      int ref_offset, void *private)
{
	struct set_badblock_record *rec = (struct set_badblock_record *)
		private;
	errcode_t	retval;
	blk_t		blk;
	int		group;

	if (blockcnt >= 0) {
		/*
		 * Get the next bad block.
		 */
		if (!ext2fs_badblocks_list_iterate(rec->bb_iter, &blk))
			return BLOCK_ABORT;
		rec->bad_block_count++;
	} else {
		/*
		 * An indirect block; fetch a block from the
		 * previously used indirect block list.  The block
		 * most be not marked as used; if so, get another one.
		 * If we run out of reserved indirect blocks, allocate
		 * a new one.
		 */
	retry:
		if (rec->ind_blocks_ptr < rec->ind_blocks_size) {
			blk = rec->ind_blocks[rec->ind_blocks_ptr++];
			if (ext2fs_test_block_bitmap(fs->block_map, blk))
				goto retry;
		} else {
			retval = ext2fs_new_block(fs, 0, 0, &blk);
			if (retval) {
				rec->err = retval;
				return BLOCK_ABORT;
			}
		}
		retval = io_channel_write_blk(fs->io, blk, 1, rec->block_buf);
		if (retval) {
			rec->err = retval;
			return BLOCK_ABORT;
		}
		ext2fs_mark_block_bitmap(fs->block_map, blk); 
		ext2fs_mark_bb_dirty(fs);
	}
	
	/*
	 * Update block counts
	 */
	group = ext2fs_group_of_blk(fs, blk);
	fs->group_desc[group].bg_free_blocks_count--;
	fs->super->s_free_blocks_count--;
	ext2fs_mark_super_dirty(fs);
	
	*block_nr = blk;
	return BLOCK_CHANGED;
}






