/*
 * pass3.c -- pass #3 of e2fsck: Check for directory connectivity
 *
 * Copyright (C) 1993, 1994, 1995, 1996, 1997 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 * 
 * Pass #3 assures that all directories are connected to the
 * filesystem tree, using the following algorithm:
 *
 * First, the root directory is checked to make sure it exists; if
 * not, e2fsck will offer to create a new one.  It is then marked as
 * "done".
 * 
 * Then, pass3 interates over all directory inodes; for each directory
 * it attempts to trace up the filesystem tree, using dirinfo.parent
 * until it reaches a directory which has been marked "done".  If it
 * can not do so, then the directory must be disconnected, and e2fsck
 * will offer to reconnect it to /lost+found.  While it is chasing
 * parent pointers up the filesystem tree, if pass3 sees a directory
 * twice, then it has detected a filesystem loop, and it will again
 * offer to reconnect the directory to /lost+found in to break the
 * filesystem loop.
 * 
 * Pass 3 also contains the subroutine, reconnect_file() to reconnect
 * inodes to /lost+found; this subroutine is also used by pass 4.
 * reconnect_file() calls get_lost_and_found(), which is responsible
 * for creating /lost+found if it does not exist.
 *
 * Pass 3 frees the following data structures:
 *     	- The dirinfo directory information cache.
 */

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include "et/com_err.h"

#include "e2fsck.h"
#include "problem.h"

static void check_root(ext2_filsys fs);
static void check_directory(ext2_filsys fs, struct dir_info *dir,
			    struct problem_context *pctx);
static ino_t get_lost_and_found(ext2_filsys fs);
static void fix_dotdot(ext2_filsys fs, struct dir_info *dir, ino_t parent);
static errcode_t adjust_inode_count(ext2_filsys fs, ino_t ino, int adj);
static errcode_t expand_directory(ext2_filsys fs, ino_t dir);

static ino_t lost_and_found = 0;
static int bad_lost_and_found = 0;

static ext2fs_inode_bitmap inode_loop_detect;
static ext2fs_inode_bitmap inode_done_map;
	
void pass3(ext2_filsys fs)
{
	int		i;
	errcode_t	retval;
	struct resource_track	rtrack;
	struct problem_context	pctx;
	struct dir_info	*dir;
	
	init_resource_track(&rtrack);

#ifdef MTRACE
	mtrace_print("Pass 3");
#endif

	if (!preen)
		printf("Pass 3: Checking directory connectivity\n");

	/*
	 * Allocate some bitmaps to do loop detection.
	 */
	retval = ext2fs_allocate_inode_bitmap(fs,
					      "inode loop detection bitmap",
					      &inode_loop_detect);
	if (retval) {
		com_err("ext2fs_allocate_inode_bitmap", retval,
			"while allocating inode_loop_detect");
		fatal_error(0);
	}
	retval = ext2fs_allocate_inode_bitmap(fs, "inode done bitmap",
					      &inode_done_map);
	if (retval) {
		com_err("ext2fs_allocate_inode_bitmap", retval,
			"while allocating inode_done_map");
		fatal_error(0);
	}
	if (tflag) {
		printf("Peak memory: ");
		print_resource_track(&global_rtrack);
	}

	check_root(fs);
	ext2fs_mark_inode_bitmap(inode_done_map, EXT2_ROOT_INO);

	clear_problem_context(&pctx);
	for (i=0; (dir = dir_info_iter(&i)) != 0;) {
		if (ext2fs_test_inode_bitmap(inode_dir_map, dir->ino))
			check_directory(fs, dir, &pctx);
	}
	
	
	free_dir_info(fs);
	ext2fs_free_inode_bitmap(inode_loop_detect);
	ext2fs_free_inode_bitmap(inode_done_map);
	if (tflag > 1) {
		printf("Pass 3: ");
		print_resource_track(&rtrack);
	}
}

/*
 * This makes sure the root inode is present; if not, we ask if the
 * user wants us to create it.  Not creating it is a fatal error.
 */
static void check_root(ext2_filsys fs)
{
	blk_t			blk;
	errcode_t		retval;
	struct ext2_inode	inode;
	char *			block;
	
	if (ext2fs_test_inode_bitmap(inode_used_map, EXT2_ROOT_INO)) {
		/*
		 * If the root inode is a directory, die here.  The
		 * user must have answered 'no' in pass1 when we
		 * offered to clear it.
		 */
		if (!(ext2fs_test_inode_bitmap(inode_dir_map, EXT2_ROOT_INO)))
			fatal_error("Root inode not directory");
		return;
	}

	if (!fix_problem(fs, PR_3_NO_ROOT_INODE, 0))
		fatal_error("Cannot proceed without a root inode.");

	read_bitmaps(fs);
	
	/*
	 * First, find a free block
	 */
	retval = ext2fs_new_block(fs, 0, block_found_map, &blk);
	if (retval) {
		com_err("ext2fs_new_block", retval,
			"while trying to create root directory");
		fatal_error(0);
	}
	ext2fs_mark_block_bitmap(block_found_map, blk);
	ext2fs_mark_block_bitmap(fs->block_map, blk);
	ext2fs_mark_bb_dirty(fs);

	/*
	 * Now let's create the actual data block for the inode
	 */
	retval = ext2fs_new_dir_block(fs, EXT2_ROOT_INO, EXT2_ROOT_INO,
				      &block);
	if (retval) {
		com_err("ext2fs_new_dir_block", retval,
			"while creating new root directory");
		fatal_error(0);
	}

	retval = ext2fs_write_dir_block(fs, blk, block);
	if (retval) {
		com_err("ext2fs_write_dir_block", retval,
			"while writing the root directory block");
		fatal_error(0);
	}
	free(block);

	/*
	 * Set up the inode structure
	 */
	memset(&inode, 0, sizeof(inode));
	inode.i_mode = 040755;
	inode.i_size = fs->blocksize;
	inode.i_atime = inode.i_ctime = inode.i_mtime = time(0);
	inode.i_links_count = 2;
	inode.i_blocks = fs->blocksize / 512;
	inode.i_block[0] = blk;

	/*
	 * Write out the inode.
	 */
	retval = ext2fs_write_inode(fs, EXT2_ROOT_INO, &inode);
	if (retval) {
		com_err("ext2fs_write_inode", retval,
			"While trying to create /lost+found");
		fatal_error(0);
	}
	
	/*
	 * Miscellaneous bookkeeping...
	 */
	add_dir_info(fs, EXT2_ROOT_INO, EXT2_ROOT_INO);
	ext2fs_icount_store(inode_count, EXT2_ROOT_INO, 2);
	ext2fs_icount_store(inode_link_info, EXT2_ROOT_INO, 2);

	ext2fs_mark_inode_bitmap(inode_used_map, EXT2_ROOT_INO);
	ext2fs_mark_inode_bitmap(inode_dir_map, EXT2_ROOT_INO);
	ext2fs_mark_inode_bitmap(fs->inode_map, EXT2_ROOT_INO);
	ext2fs_mark_ib_dirty(fs);
}

/*
 * This subroutine is responsible for making sure that a particular
 * directory is connected to the root; if it isn't we trace it up as
 * far as we can go, and then offer to connect the resulting parent to
 * the lost+found.  We have to do loop detection; if we ever discover
 * a loop, we treat that as a disconnected directory and offer to
 * reparent it to lost+found.
 */
static void check_directory(ext2_filsys fs, struct dir_info *dir,
			    struct problem_context *pctx)
{
	struct dir_info *p = dir;

	ext2fs_clear_inode_bitmap(inode_loop_detect);
	while (p) {
		/*
		 * If we find a parent which we've already checked,
		 * then stop; we know it's either already connected to
		 * the directory tree, or it isn't but the user has
		 * already told us he doesn't want us to reconnect the
		 * disconnected subtree.
		 */
		if (ext2fs_test_inode_bitmap(inode_done_map, p->ino))
			goto check_dot_dot;
		/*
		 * Mark this inode as being "done"; by the time we
		 * return from this function, the inode we either be
		 * verified as being connected to the directory tree,
		 * or we will have offered to reconnect this to
		 * lost+found.
		 */
		ext2fs_mark_inode_bitmap(inode_done_map, p->ino);
		/*
		 * If this directory doesn't have a parent, or we've
		 * seen the parent once already, then offer to
		 * reparent it to lost+found
		 */
		if (!p->parent ||
		    (ext2fs_test_inode_bitmap(inode_loop_detect,
					      p->parent)))
			break;
		ext2fs_mark_inode_bitmap(inode_loop_detect,
					 p->parent);
		p = get_dir_info(p->parent);
	}
	/*
	 * If we've reached here, we've hit a detached directory
	 * inode; offer to reconnect it to lost+found.
	 */
	pctx->ino = p->ino;
	if (fix_problem(fs, PR_3_UNCONNECTED_DIR, pctx)) {
		if (reconnect_file(fs, p->ino))
			ext2fs_unmark_valid(fs);
		else {
			p->parent = lost_and_found;
			fix_dotdot(fs, p, lost_and_found);
		}
	}

	/*
	 * Make sure that .. and the parent directory are the same;
	 * offer to fix it if not.
	 */
check_dot_dot:
	if (dir->parent != dir->dotdot) {
		pctx->ino = dir->ino;
		pctx->ino2 = dir->dotdot;
		pctx->dir = dir->parent;
		if (fix_problem(fs, PR_3_BAD_DOT_DOT, pctx))
			fix_dotdot(fs, dir, dir->parent);
	}
}	

/*
 * This routine gets the lost_and_found inode, making it a directory
 * if necessary
 */
ino_t get_lost_and_found(ext2_filsys fs)
{
	ino_t			ino;
	blk_t			blk;
	errcode_t		retval;
	struct ext2_inode	inode;
	char *			block;
	const char 		name[] = "lost+found";

	retval = ext2fs_lookup(fs, EXT2_ROOT_INO, name,
			       sizeof(name)-1, 0, &ino);
	if (!retval)
		return ino;
	if (retval != ENOENT)
		printf("Error while trying to find /lost+found: %s",
		       error_message(retval));
	if (!fix_problem(fs, PR_3_NO_LF_DIR, 0))
		return 0;

	/*
	 * Read the inode and block bitmaps in; we'll be messing with
	 * them.
	 */
	read_bitmaps(fs);
	
	/*
	 * First, find a free block
	 */
	retval = ext2fs_new_block(fs, 0, block_found_map, &blk);
	if (retval) {
		com_err("ext2fs_new_block", retval,
			"while trying to create /lost+found directory");
		return 0;
	}
	ext2fs_mark_block_bitmap(block_found_map, blk);
	ext2fs_mark_block_bitmap(fs->block_map, blk);
	ext2fs_mark_bb_dirty(fs);

	/*
	 * Next find a free inode.
	 */
	retval = ext2fs_new_inode(fs, EXT2_ROOT_INO, 040755, inode_used_map,
				  &ino);
	if (retval) {
		com_err("ext2fs_new_inode", retval,
			"while trying to create /lost+found directory");
		return 0;
	}
	ext2fs_mark_inode_bitmap(inode_used_map, ino);
	ext2fs_mark_inode_bitmap(inode_dir_map, ino);
	ext2fs_mark_inode_bitmap(fs->inode_map, ino);
	ext2fs_mark_ib_dirty(fs);

	/*
	 * Now let's create the actual data block for the inode
	 */
	retval = ext2fs_new_dir_block(fs, ino, EXT2_ROOT_INO, &block);
	if (retval) {
		com_err("ext2fs_new_dir_block", retval,
			"while creating new directory block");
		return 0;
	}

	retval = ext2fs_write_dir_block(fs, blk, block);
	free(block);
	if (retval) {
		com_err("ext2fs_write_dir_block", retval,
			"while writing the directory block for /lost+found");
		return 0;
	}

	/*
	 * Set up the inode structure
	 */
	memset(&inode, 0, sizeof(inode));
	inode.i_mode = 040755;
	inode.i_size = fs->blocksize;
	inode.i_atime = inode.i_ctime = inode.i_mtime = time(0);
	inode.i_links_count = 2;
	inode.i_blocks = fs->blocksize / 512;
	inode.i_block[0] = blk;

	/*
	 * Next, write out the inode.
	 */
	retval = ext2fs_write_inode(fs, ino, &inode);
	if (retval) {
		com_err("ext2fs_write_inode", retval,
			"While trying to create /lost+found");
		return 0;
	}
	/*
	 * Finally, create the directory link
	 */
	retval = ext2fs_link(fs, EXT2_ROOT_INO, name, ino, 0);
	if (retval) {
		com_err("ext2fs_link", retval, "While creating /lost+found");
		return 0;
	}

	/*
	 * Miscellaneous bookkeeping that needs to be kept straight.
	 */
	add_dir_info(fs, ino, EXT2_ROOT_INO);
	adjust_inode_count(fs, EXT2_ROOT_INO, +1);
	ext2fs_icount_store(inode_count, ino, 2);
	ext2fs_icount_store(inode_link_info, ino, 2);
#if 0
	printf("/lost+found created; inode #%lu\n", ino);
#endif
	return ino;
}

/*
 * This routine will connect a file to lost+found
 */
int reconnect_file(ext2_filsys fs, ino_t inode)
{
	errcode_t	retval;
	char		name[80];
	
	if (bad_lost_and_found) {
		printf("Bad or nonexistent /lost+found.  Cannot reconnect.\n");
		return 1;
	}
	if (!lost_and_found) {
		lost_and_found = get_lost_and_found(fs);
		if (!lost_and_found) {
			printf("Bad or nonexistent /lost+found.  Cannot reconnect.\n");
			bad_lost_and_found++;
			return 1;
		}
	}

	sprintf(name, "#%lu", inode);
	retval = ext2fs_link(fs, lost_and_found, name, inode, 0);
	if (retval == EXT2_ET_DIR_NO_SPACE) {
		if (!fix_problem(fs, PR_3_EXPAND_LF_DIR, 0))
			return 1;
		retval = expand_directory(fs, lost_and_found);
		if (retval) {
			printf("Could not expand /lost+found: %s\n",
			       error_message(retval));
			return 1;
		}
		retval = ext2fs_link(fs, lost_and_found, name, inode, 0);
	}
	if (retval) {
		printf("Could not reconnect %lu: %s\n", inode,
		       error_message(retval));
		return 1;
	}

	adjust_inode_count(fs, inode, +1);

	return 0;
}

/*
 * Utility routine to adjust the inode counts on an inode.
 */
static errcode_t adjust_inode_count(ext2_filsys fs, ino_t ino, int adj)
{
	errcode_t		retval;
	struct ext2_inode 	inode;
	
	if (!ino)
		return 0;

	retval = ext2fs_read_inode(fs, ino, &inode);
	if (retval)
		return retval;

#if 0
	printf("Adjusting link count for inode %lu by %d (from %d)\n", ino, adj,
	       inode.i_links_count);
#endif

	inode.i_links_count += adj;
	if (adj == 1) {
		ext2fs_icount_increment(inode_count, ino, 0);
		ext2fs_icount_increment(inode_link_info, ino, 0);
	} else {
		ext2fs_icount_decrement(inode_count, ino, 0);
		ext2fs_icount_decrement(inode_link_info, ino, 0);
	}
	

	retval = ext2fs_write_inode(fs, ino, &inode);
	if (retval)
		return retval;

	return 0;
}

/*
 * Fix parent --- this routine fixes up the parent of a directory.
 */
struct fix_dotdot_struct {
	ext2_filsys	fs;
	ino_t		parent;
	int		done;
};

static int fix_dotdot_proc(struct ext2_dir_entry *dirent,
			   int	offset,
			   int	blocksize,
			   char	*buf,
			   void	*private)
{
	struct fix_dotdot_struct *fp = (struct fix_dotdot_struct *) private;
	errcode_t	retval;

	if (dirent->name_len != 2)
		return 0;
	if (strncmp(dirent->name, "..", 2))
		return 0;
	
	retval = adjust_inode_count(fp->fs, dirent->inode, -1);
	if (retval)
		printf("Error while adjusting inode count on inode %u\n",
		       dirent->inode);
	retval = adjust_inode_count(fp->fs, fp->parent, 1);
	if (retval)
		printf("Error while adjusting inode count on inode %lu\n",
		       fp->parent);

	dirent->inode = fp->parent;

	fp->done++;
	return DIRENT_ABORT | DIRENT_CHANGED;
}

static void fix_dotdot(ext2_filsys fs, struct dir_info *dir, ino_t parent)
{
	errcode_t	retval;
	struct fix_dotdot_struct fp;

	fp.fs = fs;
	fp.parent = parent;
	fp.done = 0;

#if 0
	printf("Fixing '..' of inode %lu to be %lu...\n", dir->ino, parent);
#endif
	
	retval = ext2fs_dir_iterate(fs, dir->ino, DIRENT_FLAG_INCLUDE_EMPTY,
				    0, fix_dotdot_proc, &fp);
	if (retval || !fp.done) {
		printf("Couldn't fix parent of inode %lu: %s\n\n",
		       dir->ino, retval ? error_message(retval) :
		       "Couldn't find parent direntory entry");
		ext2fs_unmark_valid(fs);
	}
	dir->dotdot = parent;
	
	return;
}

/*
 * These routines are responsible for expanding a /lost+found if it is
 * too small.
 */

struct expand_dir_struct {
	int	done;
	errcode_t	err;
};

static int expand_dir_proc(ext2_filsys fs,
			   blk_t	*blocknr,
			   int	blockcnt,
			   void	*private)
{
	struct expand_dir_struct *es = (struct expand_dir_struct *) private;
	blk_t	new_blk;
	static blk_t	last_blk = 0;
	char		*block;
	errcode_t	retval;
	
	if (*blocknr) {
		last_blk = *blocknr;
		return 0;
	}
	retval = ext2fs_new_block(fs, last_blk, block_found_map, &new_blk);
	if (retval) {
		es->err = retval;
		return BLOCK_ABORT;
	}
	if (blockcnt > 0) {
		retval = ext2fs_new_dir_block(fs, 0, 0, &block);
		if (retval) {
			es->err = retval;
			return BLOCK_ABORT;
		}
		es->done = 1;
	} else {
		block = malloc(fs->blocksize);
		if (!block) {
			es->err = ENOMEM;
			return BLOCK_ABORT;
		}
		memset(block, 0, fs->blocksize);
	}	
	retval = ext2fs_write_dir_block(fs, new_blk, block);
	if (retval) {
		es->err = retval;
		return BLOCK_ABORT;
	}
	free(block);
	*blocknr = new_blk;
	ext2fs_mark_block_bitmap(block_found_map, new_blk);
	ext2fs_mark_block_bitmap(fs->block_map, new_blk);
	ext2fs_mark_bb_dirty(fs);
	if (es->done)
		return (BLOCK_CHANGED | BLOCK_ABORT);
	else
		return BLOCK_CHANGED;
}

static errcode_t expand_directory(ext2_filsys fs, ino_t dir)
{
	errcode_t	retval;
	struct expand_dir_struct es;
	struct ext2_inode	inode;
	
	if (!(fs->flags & EXT2_FLAG_RW))
		return EXT2_ET_RO_FILSYS;

	retval = ext2fs_check_directory(fs, dir);
	if (retval)
		return retval;
	
	es.done = 0;
	es.err = 0;
	
	retval = ext2fs_block_iterate(fs, dir, BLOCK_FLAG_APPEND,
				      0, expand_dir_proc, &es);

	if (es.err)
		return es.err;
	if (!es.done)
		return EXT2_ET_EXPAND_DIR_ERR;

	/*
	 * Update the size and block count fields in the inode.
	 */
	retval = ext2fs_read_inode(fs, dir, &inode);
	if (retval)
		return retval;
	
	inode.i_size += fs->blocksize;
	inode.i_blocks += fs->blocksize / 512;

	e2fsck_write_inode(fs, dir, &inode, "expand_directory");

	return 0;
}


		
