/*
 * pass5.c --- check block and inode bitmaps against on-disk bitmaps
 * 
 * Copyright (C) 1993, 1994, 1995, 1996, 1997 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 * 
 */

#include "et/com_err.h"

#include "e2fsck.h"

static void check_block_bitmaps(ext2_filsys fs);
static void check_inode_bitmaps(ext2_filsys fs);
static void check_inode_end(ext2_filsys fs);
static void check_block_end(ext2_filsys fs);

static int do_fix = -1;
static const char *fix_question = "Fix summary information";

void pass5(ext2_filsys fs)
{
	struct resource_track	rtrack;
	
#ifdef MTRACE
	mtrace_print("Pass 5");
#endif

	init_resource_track(&rtrack);
	
	if (!preen)
		printf("Pass 5: Checking group summary information\n");

	read_bitmaps(fs);

	check_block_bitmaps(fs);
	check_inode_bitmaps(fs);
	check_inode_end(fs);
	check_block_end(fs);

	ext2fs_free_inode_bitmap(inode_used_map);
	ext2fs_free_inode_bitmap(inode_dir_map);
	ext2fs_free_block_bitmap(block_found_map);

	if (tflag > 1) {
		printf("Pass 5: ");
		print_resource_track(&rtrack);
	}
}

static void check_block_bitmaps(ext2_filsys fs)
{
	blk_t	i;
	int	*free_array;
	int	group = 0;
	int	blocks = 0;
	int	free_blocks = 0;
	int	group_free = 0;
	int	actual, bitmap;
	const char 	*print_header = "Block bitmap differences:";
	
	free_array = allocate_memory(fs->group_desc_count * sizeof(int),
				     "free block count array");

	if ((fs->super->s_first_data_block <
	     ext2fs_get_block_bitmap_start(block_found_map)) ||
	    (fs->super->s_blocks_count-1 >
	     ext2fs_get_block_bitmap_end(block_found_map))) {
		printf("PROGRAMMING ERROR: filesystem endpoints (%d, %d)\n\t"
		       "don't match block_found_map endpoints (%d, %d).\n",
		       fs->super->s_first_data_block,
		       fs->super->s_blocks_count -1,
		       ext2fs_get_block_bitmap_start(block_found_map),
		       ext2fs_get_block_bitmap_end(block_found_map));
		fatal_error(0);
	}
		       
	if ((fs->super->s_first_data_block <
	     ext2fs_get_block_bitmap_start(fs->block_map)) ||
	    (fs->super->s_blocks_count-1 >
	     ext2fs_get_block_bitmap_end(fs->block_map))) {
		printf("PROGRAMMING ERROR: filesystem endpoints (%d, %d)\n\t"
		       "don't match fs->block_map endpoints (%d, %d).\n",
		       fs->super->s_first_data_block,
		       fs->super->s_blocks_count -1,
		       ext2fs_get_block_bitmap_start(fs->block_map),
		       ext2fs_get_block_bitmap_end(fs->block_map));
		fatal_error(0);
	}
		       
		       
	for (i = fs->super->s_first_data_block;
	     i < fs->super->s_blocks_count;
	     i++) {
		actual = ext2fs_fast_test_block_bitmap(block_found_map, i);
		bitmap = ext2fs_fast_test_block_bitmap(fs->block_map, i);
		
		if (actual == bitmap)
			goto do_counts;

		if (do_fix < 0)
			do_fix = ask(fix_question, 1);
		if (!preen && print_header) {
			printf(print_header);
			print_header = 0;
		}
		if (!actual && bitmap) {
			/*
			 * Block not used, but marked in use in the bitmap.
			 */
			if (!preen)
				printf(" -%u", i);
			if (do_fix)
				ext2fs_unmark_block_bitmap(fs->block_map,
							   i);
		} else {
			/*
			 * Block used, but not marked in use in the bitmap.
			 */
			if (!preen)
				printf(" +%u", i);
			if (do_fix)
				ext2fs_mark_block_bitmap(fs->block_map,
							 i);
		}
		if (do_fix) {
			ext2fs_mark_bb_dirty(fs);
			bitmap = actual;
		} else
			ext2fs_unmark_valid(fs);
			
	do_counts:
		if (!bitmap) {
			group_free++;
			free_blocks++;
		}
		blocks ++;
		if ((blocks == fs->super->s_blocks_per_group) ||
		    (i == fs->super->s_blocks_count-1)) {
			free_array[group] = group_free;
			group ++;
			blocks = 0;
			group_free = 0;
		}
	}
	if (!print_header)
		printf(".  %s\n", fix_msg[do_fix]);
	for (i = 0; i < fs->group_desc_count; i++) {
		if (free_array[i] != fs->group_desc[i].bg_free_blocks_count) {
			if (do_fix < 0)
				do_fix = ask(fix_question, 1);
			if (!preen)
				printf("Free blocks count wrong for "
				       "group %u (%u, counted=%d).  %s\n", i,
				       fs->group_desc[i].bg_free_blocks_count,
				       free_array[i], fix_msg[do_fix]);
			if (do_fix) {
				fs->group_desc[i].bg_free_blocks_count =
					free_array[i];
				ext2fs_mark_super_dirty(fs);
			} else
				ext2fs_unmark_valid(fs);
		}
	}
	if (free_blocks != fs->super->s_free_blocks_count) {
		if (do_fix < 0)
			do_fix = ask(fix_question, 1);
		if (!preen)
			printf("Free blocks count wrong "
			       "(%u, counted=%d).  %s\n",
			       fs->super->s_free_blocks_count, free_blocks,
			       fix_msg[do_fix]);
		if (do_fix) {
			fs->super->s_free_blocks_count = free_blocks;
			ext2fs_mark_super_dirty(fs);
		} else
			ext2fs_unmark_valid(fs);
	}
	free(free_array);
}
			
static void check_inode_bitmaps(ext2_filsys fs)
{
	ino_t	i;
	int	free_inodes = 0;
	int	group_free = 0;
	int	dirs_count = 0;
	int	group = 0;
	int	inodes = 0;
	int	*free_array;
	int	*dir_array;
	int	actual, bitmap;
	const char *print_header = "Inode bitmap differences:";
	
	free_array = allocate_memory(fs->group_desc_count * sizeof(int),
				     "free inode count array");
				     
	dir_array = allocate_memory(fs->group_desc_count * sizeof(int),
				    "directory count array");
				     
	if ((1 < ext2fs_get_inode_bitmap_start(inode_used_map)) ||
	    (fs->super->s_inodes_count > 
	     ext2fs_get_inode_bitmap_end(inode_used_map))) {
		printf("PROGRAMMING ERROR: filesystem inode endpoints (%d, %d)\n\t"
		       "don't match inode_used_map endpoints (%d, %d).\n",
		       1, fs->super->s_inodes_count,
		       ext2fs_get_inode_bitmap_start(inode_used_map),
		       ext2fs_get_inode_bitmap_end(inode_used_map));
		fatal_error(0);
	}
	if ((1 < ext2fs_get_inode_bitmap_start(fs->inode_map)) ||
	    (fs->super->s_inodes_count > 
	     ext2fs_get_inode_bitmap_end(fs->inode_map))) {
		printf("PROGRAMMING ERROR: filesystem inode endpoints (%d, %d)\n\t"
		       "don't match fs->inode_map endpoints (%d, %d).\n",
		       1, fs->super->s_inodes_count,
		       ext2fs_get_inode_bitmap_start(fs->inode_map),
		       ext2fs_get_inode_bitmap_end(fs->inode_map));
		fatal_error(0);
	}

	for (i = 1; i <= fs->super->s_inodes_count; i++) {
		actual = ext2fs_fast_test_inode_bitmap(inode_used_map, i);
		bitmap = ext2fs_fast_test_inode_bitmap(fs->inode_map, i);
		
		if (actual == bitmap)
			goto do_counts;
		
		if (do_fix < 0)
			do_fix = ask(fix_question, 1);
		if (!preen && print_header) {
			printf(print_header);
			print_header = 0;
		}
		if (!actual && bitmap) {
			/*
			 * Inode wasn't used, but marked in bitmap
			 */
			if (!preen)
				printf(" -%lu", i);
			if (do_fix)
				ext2fs_unmark_inode_bitmap(fs->inode_map, i);
		} else if (actual && !bitmap) {
			/*
			 * Inode used, but not in bitmap
			 */
			if (!preen)
				printf (" +%lu", i);
			if (do_fix)
				ext2fs_mark_inode_bitmap(fs->inode_map, i);
		}
		if (do_fix) {
			ext2fs_mark_ib_dirty(fs);
			bitmap = actual;
		} else
			ext2fs_unmark_valid(fs);
			
do_counts:
		if (!bitmap) {
			group_free++;
			free_inodes++;
		} else {
			if (ext2fs_test_inode_bitmap(inode_dir_map, i))
				dirs_count++;
		}
		inodes++;
		if ((inodes == fs->super->s_inodes_per_group) ||
		    (i == fs->super->s_inodes_count)) {
			free_array[group] = group_free;
			dir_array[group] = dirs_count;
			group ++;
			inodes = 0;
			group_free = 0;
			dirs_count = 0;
		}
	}
	if (!print_header)
		printf(".  %s\n", fix_msg[do_fix]);
	
	for (i = 0; i < fs->group_desc_count; i++) {
		if (free_array[i] != fs->group_desc[i].bg_free_inodes_count) {
			if (do_fix < 0)
				do_fix = ask(fix_question, 1);
			if (!preen)
				printf ("Free inodes count wrong for "
					"group #%lu (%u, counted=%d).  %s\n", i,
					fs->group_desc[i].bg_free_inodes_count,
					free_array[i], fix_msg[do_fix]);
			if (do_fix) {
				fs->group_desc[i].bg_free_inodes_count =
					free_array[i];
				ext2fs_mark_super_dirty(fs);
			} else
				ext2fs_unmark_valid(fs);
		}
		if (dir_array[i] != fs->group_desc[i].bg_used_dirs_count) {
			if (do_fix < 0)
				do_fix = ask(fix_question, 1);
			if (!preen)
				printf ("Directories count wrong for "
					"group #%lu (%u, counted=%d).  %s\n", i,
					fs->group_desc[i].bg_used_dirs_count,
					dir_array[i], fix_msg[do_fix]);
			if (do_fix) {
				fs->group_desc[i].bg_used_dirs_count =
					dir_array[i];
				ext2fs_mark_super_dirty(fs);
			} else
				ext2fs_unmark_valid(fs);
		}
	}
	if (free_inodes != fs->super->s_free_inodes_count) {
		if (do_fix < 0)
			do_fix = ask(fix_question, 1);
		if (!preen)
			printf("Free inodes count wrong "
			       "(%u, counted=%d).  %s\n",
			       fs->super->s_free_inodes_count, free_inodes,
			       fix_msg[do_fix]);
		if (do_fix) {
			fs->super->s_free_inodes_count = free_inodes;
			ext2fs_mark_super_dirty(fs);
		} else
			ext2fs_unmark_valid(fs);
	}
	free(free_array);
	free(dir_array);
}

static void check_inode_end(ext2_filsys fs)
{
	ino_t	end, save_inodes_count, i;
	errcode_t	retval;

	end = EXT2_INODES_PER_GROUP(fs->super) * fs->group_desc_count;
	retval = ext2fs_fudge_inode_bitmap_end(fs->inode_map, end,
					       &save_inodes_count);
	if (retval) {
		com_err("check_inode_end", retval,
			"while trying to fudge end of inode bitmap");
		fatal_error(0);
	}
	if (save_inodes_count == end)
		return;
	
	for (i = save_inodes_count + 1; i <= end; i++) {
		if (!ext2fs_test_inode_bitmap(fs->inode_map, i)) {
			printf("Padding at end of inode bitmap is not set. ");
			if (ask("Fix", 1)) {
				for (i = save_inodes_count + 1; i <= end; i++)
					ext2fs_mark_inode_bitmap(fs->inode_map,
								 i);
				ext2fs_mark_ib_dirty(fs);
			} else
				ext2fs_unmark_valid(fs);
			break;
		}
	}

	retval = ext2fs_fudge_inode_bitmap_end(fs->inode_map,
					       save_inodes_count, 0);
	if (retval) {
		com_err("check_inode_end", retval,
			"while trying to fudge end of inode bitmap back");
		fatal_error(0);
	}
}

static void check_block_end(ext2_filsys fs)
{
	blk_t	end, save_blocks_count, i;
	errcode_t	retval;

	end = fs->block_map->start +
		(EXT2_BLOCKS_PER_GROUP(fs->super) * fs->group_desc_count) - 1;
	retval = ext2fs_fudge_block_bitmap_end(fs->block_map, end,
					       &save_blocks_count);
	if (retval) {
		com_err("check_block_end", retval,
			"while trying to fudge end of block bitmap");
		fatal_error(0);
	}
	if (save_blocks_count == end)
		return;
	
	for (i = save_blocks_count + 1; i <= end; i++) {
		if (!ext2fs_test_block_bitmap(fs->block_map, i)) {
			printf("Padding at end of block bitmap is not set. ");

			if (ask("Fix", 1)) {
				for (i = save_blocks_count + 1; i < end; i++)
					ext2fs_mark_block_bitmap(fs->block_map,
								 i);
				ext2fs_mark_bb_dirty(fs);
			} else
				ext2fs_unmark_valid(fs);
			break;
		}
	}

	retval = ext2fs_fudge_block_bitmap_end(fs->block_map,
					       save_blocks_count, 0);
	if (retval) {
		com_err("check_block_end", retval,
			"while trying to fudge end of block bitmap back");
		fatal_error(0);
	}
}

