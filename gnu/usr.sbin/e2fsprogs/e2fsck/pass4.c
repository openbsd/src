/*
 * pass4.c -- pass #4 of e2fsck: Check reference counts
 *
 * Copyright (C) 1993, 1994, 1995, 1996, 1997 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 *
 * Pass 4 frees the following data structures:
 * 	- A bitmap of which inodes are in bad blocks.	(inode_bb_map)
 */

#include "e2fsck.h"
#include "problem.h"

/*
 * This routine is called when an inode is not connected to the
 * directory tree.
 * 
 * This subroutine returns 1 then the caller shouldn't bother with the
 * rest of the pass 4 tests.
 */
static int disconnect_inode(ext2_filsys fs, ino_t i)
{
	struct ext2_inode	inode;
	struct problem_context	pctx;

	e2fsck_read_inode(fs, i, &inode, "pass4: disconnect_inode");
	clear_problem_context(&pctx);
	pctx.ino = i;
	pctx.inode = &inode;
	
	if (!inode.i_blocks && (LINUX_S_ISREG(inode.i_mode) ||
				LINUX_S_ISDIR(inode.i_mode))) {
		/*
		 * This is a zero-length file; prompt to delete it...
		 */
		if (fix_problem(fs, PR_4_ZERO_LEN_INODE, &pctx)) {
			ext2fs_icount_store(inode_link_info, i, 0);
			inode.i_links_count = 0;
			inode.i_dtime = time(0);
			e2fsck_write_inode(fs, i, &inode,
					   "disconnect_inode");
			/*
			 * Fix up the bitmaps...
			 */
			read_bitmaps(fs);
			ext2fs_unmark_inode_bitmap(inode_used_map, i);
			ext2fs_unmark_inode_bitmap(inode_dir_map, i);
			ext2fs_unmark_inode_bitmap(fs->inode_map, i);
			ext2fs_mark_ib_dirty(fs);
			return 0;
		}
	}
	
	/*
	 * Prompt to reconnect.
	 */
	if (fix_problem(fs, PR_4_UNATTACHED_INODE, &pctx)) {
		if (reconnect_file(fs, i))
			ext2fs_unmark_valid(fs);
	} else {
		/*
		 * If we don't attach the inode, then skip the
		 * i_links_test since there's no point in trying to
		 * force i_links_count to zero.
		 */
		ext2fs_unmark_valid(fs);
		return 1;
	}
	return 0;
}


void pass4(ext2_filsys fs)
{
	ino_t	i;
	struct ext2_inode	inode;
	struct resource_track	rtrack;
	struct problem_context	pctx;
	__u16	link_count, link_counted;
	
	init_resource_track(&rtrack);

#ifdef MTRACE
	mtrace_print("Pass 4");
#endif

	if (!preen)
		printf("Pass 4: Checking reference counts\n");
	clear_problem_context(&pctx);
	for (i=1; i <= fs->super->s_inodes_count; i++) {
		if (i == EXT2_BAD_INO ||
		    (i > EXT2_ROOT_INO && i < EXT2_FIRST_INODE(fs->super)))
			continue;
		if (!(ext2fs_test_inode_bitmap(inode_used_map, i)) ||
		    (inode_bb_map &&
		     ext2fs_test_inode_bitmap(inode_bb_map, i)))
			continue;
		ext2fs_icount_fetch(inode_link_info, i, &link_count);
		ext2fs_icount_fetch(inode_count, i, &link_counted);
		if (link_counted == 0) {
			if (disconnect_inode(fs, i))
				continue;
			ext2fs_icount_fetch(inode_link_info, i, &link_count);
			ext2fs_icount_fetch(inode_count, i, &link_counted);
		}
		if (link_counted != link_count) {
			e2fsck_read_inode(fs, i, &inode, "pass4");
			pctx.ino = i;
			pctx.inode = &inode;
			if (link_count != inode.i_links_count) {
				printf("WARNING: PROGRAMMING BUG IN E2FSCK!\n");
				printf("\tOR SOME BONEHEAD (YOU) IS CHECKING "
				       "A MOUNTED (LIVE) FILESYSTEM.\n"); 
				printf("inode_link_info[%ld] is %u, "
				       "inode.i_links_count is %d.  "
				       "They should be the same!\n",
				       i, link_count,
				       inode.i_links_count);
			}
			pctx.num = link_counted;
			if (fix_problem(fs, PR_4_BAD_REF_COUNT, &pctx)) {
				inode.i_links_count = link_counted;
				e2fsck_write_inode(fs, i, &inode, "pass4");
			}
		}
	}
	ext2fs_free_icount(inode_link_info); inode_link_info = 0;
	ext2fs_free_icount(inode_count); inode_count = 0;
	ext2fs_free_inode_bitmap(inode_bb_map);
	if (tflag > 1) {
		printf("Pass 4: ");
		print_resource_track(&rtrack);
	}
}

