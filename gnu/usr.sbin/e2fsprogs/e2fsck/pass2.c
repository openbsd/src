/*
 * pass2.c --- check directory structure
 * 
 * Copyright (C) 1993, 1994, 1995, 1996, 1997 Theodore Ts'o
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 * 
 * Pass 2 of e2fsck iterates through all active directory inodes, and
 * applies to following tests to each directory entry in the directory
 * blocks in the inodes:
 *
 *	- The length of the directory entry (rec_len) should be at
 * 		least 8 bytes, and no more than the remaining space
 * 		left in the directory block.
 * 	- The length of the name in the directory entry (name_len)
 * 		should be less than (rec_len - 8).  
 *	- The inode number in the directory entry should be within
 * 		legal bounds.
 * 	- The inode number should refer to a in-use inode.
 *	- The first entry should be '.', and its inode should be
 * 		the inode of the directory.
 * 	- The second entry should be '..'.
 *
 * To minimize disk seek time, the directory blocks are processed in
 * sorted order of block numbers.
 *
 * Pass 2 also collects the following information:
 * 	- The inode numbers of the subdirectories for each directory.
 *
 * Pass 2 relies on the following information from previous passes:
 * 	- The directory information collected in pass 1.
 * 	- The inode_used_map bitmap
 * 	- The inode_bad_map bitmap
 * 	- The inode_dir_map bitmap
 *
 * Pass 2 frees the following data structures
 * 	- The inode_bad_map bitmap
 */

#include "et/com_err.h"

#include "e2fsck.h"
#include "problem.h"

/*
 * Keeps track of how many times an inode is referenced.
 */
ext2_icount_t inode_count = 0;	

static void deallocate_inode(ext2_filsys fs, ino_t ino,
			     char* block_buf);
static int process_bad_inode(ext2_filsys fs, ino_t dir, ino_t ino);
static int check_dir_block(ext2_filsys fs,
			   struct ext2_db_entry *dir_blocks_info,
			   void *private);
static int allocate_dir_block(ext2_filsys fs,
			      struct ext2_db_entry *dir_blocks_info,
			      char *buf, struct problem_context *pctx);
static int update_dir_block(ext2_filsys fs,
			    blk_t	*block_nr,
			    int blockcnt,
			    void *private);

struct check_dir_struct {
	char *buf;
	struct problem_context	pctx;
};	

void pass2(ext2_filsys fs)
{
	char	*buf;
	struct resource_track	rtrack;
	struct dir_info *dir;
	errcode_t	retval;
	ino_t		size;
	struct check_dir_struct cd;
		
	init_resource_track(&rtrack);

#ifdef MTRACE
	mtrace_print("Pass 2");
#endif

	if (!preen)
		printf("Pass 2: Checking directory structure\n");
	retval = ext2fs_create_icount2(fs, EXT2_ICOUNT_OPT_INCREMENT,
				       0, inode_link_info, &inode_count);
	if (retval) {
		com_err("ext2fs_create_icount", retval,
			"while creating inode_count");
		fatal_error(0);
	}
	buf = allocate_memory(fs->blocksize, "directory scan buffer");

	/*
	 * Set up the parent pointer for the root directory, if
	 * present.  (If the root directory is not present, we will
	 * create it in pass 3.)
	 */
	dir = get_dir_info(EXT2_ROOT_INO);
	if (dir)
		dir->parent = EXT2_ROOT_INO;

	cd.buf = buf;
	clear_problem_context(&cd.pctx);
	
	retval = ext2fs_dblist_iterate(fs->dblist, check_dir_block, &cd);

	free(buf);
	ext2fs_free_dblist(fs->dblist);

	if (inode_bad_map) {
		ext2fs_free_inode_bitmap(inode_bad_map);
		inode_bad_map = 0;
	}
	if (tflag > 1) {
		printf("Pass 2: ");
		print_resource_track(&rtrack);
	}
}

/*
 * Make sure the first entry in the directory is '.', and that the
 * directory entry is sane.
 */
static int check_dot(ext2_filsys fs,
		     struct ext2_dir_entry *dirent,
		     ino_t ino, struct problem_context *pctx)
{
	struct ext2_dir_entry *nextdir;
	int	status = 0;
	int	created = 0;
	int	new_len;
	int	problem = 0;
	
	if (!dirent->inode)
		problem = PR_2_MISSING_DOT;
	else if ((dirent->name_len != 1) ||
		 (dirent->name[0] != '.'))
		problem = PR_2_1ST_NOT_DOT;
	else if (dirent->name[1] != '\0')
		problem = PR_2_DOT_NULL_TERM;
	
	if (problem) {
		if (fix_problem(fs, problem, pctx)) {
			if (dirent->rec_len < 12)
				dirent->rec_len = 12;
			dirent->inode = ino;
			dirent->name_len = 1;
			dirent->name[0] = '.';
			dirent->name[1] = '\0';
			status = 1;
			created = 1;
		}
	}
	if (dirent->inode != ino) {
		if (fix_problem(fs, PR_2_BAD_INODE_DOT, pctx)) {
			dirent->inode = ino;
			status = 1;
		}
	}
	if (dirent->rec_len > 12) {
		new_len = dirent->rec_len - 12;
		if (new_len > 12) {
			preenhalt(fs);
			if (created ||
			    ask("Directory entry for '.' is big.  Split", 1)) {
				nextdir = (struct ext2_dir_entry *)
					((char *) dirent + 12);
				dirent->rec_len = 12;
				nextdir->rec_len = new_len;
				nextdir->inode = 0;
				nextdir->name_len = 0;
				status = 1;
			}
		}
	}
	return status;
}

/*
 * Make sure the second entry in the directory is '..', and that the
 * directory entry is sane.  We do not check the inode number of '..'
 * here; this gets done in pass 3.
 */
static int check_dotdot(ext2_filsys fs,
			struct ext2_dir_entry *dirent,
			struct dir_info *dir, struct problem_context *pctx)
{
	int		problem = 0;
	
	if (!dirent->inode)
		problem = PR_2_MISSING_DOT_DOT;
	else if ((dirent->name_len != 2) ||
		 (dirent->name[0] != '.') ||
		 (dirent->name[1] != '.'))
		problem = PR_2_2ND_NOT_DOT_DOT;
	else if (dirent->name[2] != '\0')
		problem = PR_2_DOT_DOT_NULL_TERM;

	if (problem) {
		if (fix_problem(fs, problem, pctx)) {
			if (dirent->rec_len < 12)
				dirent->rec_len = 12;
			/*
			 * Note: we don't have the parent inode just
			 * yet, so we will fill it in with the root
			 * inode.  This will get fixed in pass 3.
			 */
			dirent->inode = EXT2_ROOT_INO;
			dirent->name_len = 2;
			dirent->name[0] = '.';
			dirent->name[1] = '.';
			dirent->name[2] = '\0';
			return 1;
		} 
		return 0;
	}
	dir->dotdot = dirent->inode;
	return 0;
}

/*
 * Check to make sure a directory entry doesn't contain any illegal
 * characters.
 */
static int check_name(ext2_filsys fs,
		      struct ext2_dir_entry *dirent,
		      ino_t dir_ino, struct problem_context *pctx)
{
	int	i;
	int	fixup = -1;
	int	ret = 0;
	
	for ( i = 0; i < dirent->name_len; i++) {
		if (dirent->name[i] == '/' || dirent->name[i] == '\0') {
			if (fixup < 0) {
				fixup = fix_problem(fs, PR_2_BAD_NAME, pctx);
			}
			if (fixup) {
				dirent->name[i] = '.';
				ret = 1;
			}
		}
	}
	return ret;
}

static int check_dir_block(ext2_filsys fs,
			   struct ext2_db_entry *db,
			   void *private)
{
	struct dir_info		*subdir, *dir;
	struct ext2_dir_entry 	*dirent;
	int			offset = 0;
	int			dir_modified = 0;
	errcode_t		retval;
	int			dot_state;
	blk_t			block_nr = db->blk;
	ino_t 			ino = db->ino;
	__u16			links;
	struct check_dir_struct	*cd = private;
	char 			*buf = cd->buf;
	
	/*
	 * Make sure the inode is still in use (could have been 
	 * deleted in the duplicate/bad blocks pass.
	 */
	if (!(ext2fs_test_inode_bitmap(inode_used_map, ino))) 
		return 0;

	cd->pctx.ino = ino;
	cd->pctx.blk = block_nr;
	cd->pctx.blkcount = db->blockcnt;
	cd->pctx.ino2 = 0;
	cd->pctx.dirent = 0;
	cd->pctx.num = 0;

	if (db->blk == 0) {
		if (allocate_dir_block(fs, db, buf, &cd->pctx))
			return 0;
		block_nr = db->blk;
	}
	
	if (db->blockcnt)
		dot_state = 2;
	else
		dot_state = 0;

#if 0
	printf("In process_dir_block block %lu, #%d, inode %lu\n", block_nr,
	       db->blockcnt, ino);
#endif
	
	retval = ext2fs_read_dir_block(fs, block_nr, buf);
	if (retval) {
		com_err(program_name, retval,
			"while reading directory block %d", block_nr);
	}

	do {
		dot_state++;
		dirent = (struct ext2_dir_entry *) (buf + offset);
		cd->pctx.dirent = dirent;
		cd->pctx.num = offset;
		if (((offset + dirent->rec_len) > fs->blocksize) ||
		    (dirent->rec_len < 8) ||
		    ((dirent->rec_len % 4) != 0) ||
		    ((dirent->name_len+8) > dirent->rec_len)) {
			if (fix_problem(fs, PR_2_DIR_CORRUPTED, &cd->pctx)) {
				dirent->rec_len = fs->blocksize - offset;
				dirent->name_len = 0;
				dirent->inode = 0;
				dir_modified++;
			} else
				return DIRENT_ABORT;
		}

		if (dirent->name_len > EXT2_NAME_LEN) {
			if (fix_problem(fs, PR_2_FILENAME_LONG, &cd->pctx)) {
				dirent->name_len = EXT2_NAME_LEN;
				dir_modified++;
			}
		}

		if (dot_state == 1) {
			if (check_dot(fs, dirent, ino, &cd->pctx))
				dir_modified++;
		} else if (dot_state == 2) {
			dir = get_dir_info(ino);
			if (!dir) {
				printf("Internal error: couldn't find dir_info for %lu\n",
				       ino);
				fatal_error(0);
			}
			if (check_dotdot(fs, dirent, dir, &cd->pctx))
				dir_modified++;
		} else if (dirent->inode == ino) {
			if (fix_problem(fs, PR_2_LINK_DOT, &cd->pctx)) {
				dirent->inode = 0;
				dir_modified++;
				goto next;
			}
		}
		if (!dirent->inode) 
			goto next;
		
		if (check_name(fs, dirent, ino, &cd->pctx))
			dir_modified++;

		/*
		 * Make sure the inode listed is a legal one.
		 */ 
		if (((dirent->inode != EXT2_ROOT_INO) &&
		     (dirent->inode < EXT2_FIRST_INODE(fs->super))) ||
		    (dirent->inode > fs->super->s_inodes_count)) {
			if (fix_problem(fs, PR_2_BAD_INO, &cd->pctx)) {
				dirent->inode = 0;
				dir_modified++;
				goto next;
			}
		}

		/*
		 * If the inode is unused, offer to clear it.
		 */
		if (!(ext2fs_test_inode_bitmap(inode_used_map,
					       dirent->inode))) {
			if (fix_problem(fs, PR_2_UNUSED_INODE, &cd->pctx)) {
				dirent->inode = 0;
				dir_modified++;
				goto next;
			}
		}

		/*
		 * If the inode is in a bad block, offer to clear it.
		 */
		if (inode_bb_map &&
		    (ext2fs_test_inode_bitmap(inode_bb_map,
					      dirent->inode))) {
			if (fix_problem(fs, PR_2_BB_INODE, &cd->pctx)) {
				dirent->inode = 0;
				dir_modified++;
				goto next;
			}
		}

		/*
		 * If the inode was marked as having bad fields in
		 * pass1, process it and offer to fix/clear it.
		 * (We wait until now so that we can display the
		 * pathname to the user.)
		 */
		if (inode_bad_map &&
		    ext2fs_test_inode_bitmap(inode_bad_map,
					     dirent->inode)) {
			if (process_bad_inode(fs, ino, dirent->inode)) {
				dirent->inode = 0;
				dir_modified++;
				goto next;
			}
		}

		/*
		 * Don't allow links to the root directory.  We check
		 * this specially to make sure we catch this error
		 * case even if the root directory hasn't been created
		 * yet.
		 */
		if ((dot_state > 2) && (dirent->inode == EXT2_ROOT_INO)) {
			if (fix_problem(fs, PR_2_LINK_ROOT, &cd->pctx)) {
				dirent->inode = 0;
				dir_modified++;
				goto next;
			}
		}
		
		/*
		 * If this is a directory, then mark its parent in its
		 * dir_info structure.  If the parent field is already
		 * filled in, then this directory has more than one
		 * hard link.  We assume the first link is correct,
		 * and ask the user if he/she wants to clear this one.
		 */
		if ((dot_state > 2) &&
		    (ext2fs_test_inode_bitmap(inode_dir_map,
					      dirent->inode))) {
			subdir = get_dir_info(dirent->inode);
			if (!subdir) {
				printf("INTERNAL ERROR: missing dir %u\n",
				       dirent->inode);
				fatal_error(0);
			}
			if (subdir->parent) {
				cd->pctx.ino2 = subdir->parent;
				if (fix_problem(fs, PR_2_LINK_DIR,
						&cd->pctx)) {
					dirent->inode = 0;
					dir_modified++;
					goto next;
				}
				cd->pctx.ino2 = 0;
			} else
				subdir->parent = ino;
		}
		
		ext2fs_icount_increment(inode_count, dirent->inode, &links);
		if (links > 1)
			fs_links_count++;
		fs_total_count++;
	next:
		offset += dirent->rec_len;
	} while (offset < fs->blocksize);
#if 0
	printf("\n");
#endif
	if (offset != fs->blocksize) {
		printf("Final rec_len is %d, should be %d\n",
		       dirent->rec_len,
		       dirent->rec_len - fs->blocksize + offset);
	}
	if (dir_modified) {
		retval = ext2fs_write_dir_block(fs, block_nr, buf);
		if (retval) {
			com_err(program_name, retval,
				"while writing directory block %d", block_nr);
		}
		ext2fs_mark_changed(fs);
	}
	return 0;
}

/*
 * This function is called to deallocate a block, and is an interator
 * functioned called by deallocate inode via ext2fs_iterate_block().
 */
static int deallocate_inode_block(ext2_filsys fs,
			     blk_t	*block_nr,
			     int blockcnt,
			     void *private)
{
	if (!*block_nr)
		return 0;
	ext2fs_unmark_block_bitmap(block_found_map, *block_nr);
	ext2fs_unmark_block_bitmap(fs->block_map, *block_nr);
	return 0;
}
		
/*
 * This fuction deallocates an inode
 */
static void deallocate_inode(ext2_filsys fs, ino_t ino,
			     char* block_buf)
{
	errcode_t		retval;
	struct ext2_inode	inode;

	ext2fs_icount_store(inode_link_info, ino, 0);
	e2fsck_read_inode(fs, ino, &inode, "deallocate_inode");
	inode.i_links_count = 0;
	inode.i_dtime = time(0);
	e2fsck_write_inode(fs, ino, &inode, "deallocate_inode");

	/*
	 * Fix up the bitmaps...
	 */
	read_bitmaps(fs);
	ext2fs_unmark_inode_bitmap(inode_used_map, ino);
	ext2fs_unmark_inode_bitmap(inode_dir_map, ino);
	if (inode_bad_map)
		ext2fs_unmark_inode_bitmap(inode_bad_map, ino);
	ext2fs_unmark_inode_bitmap(fs->inode_map, ino);
	ext2fs_mark_ib_dirty(fs);

	if (!ext2fs_inode_has_valid_blocks(&inode))
		return;
	
	ext2fs_mark_bb_dirty(fs);
	retval = ext2fs_block_iterate(fs, ino, 0, block_buf,
				      deallocate_inode_block, 0);
	if (retval)
		com_err("deallocate_inode", retval,
			"while calling ext2fs_block_iterate for inode %d",
			ino);
}

static int process_bad_inode(ext2_filsys fs, ino_t dir, ino_t ino)
{
	struct ext2_inode	inode;
	int			inode_modified = 0;
	unsigned char		*frag, *fsize;
	struct problem_context	pctx;

	e2fsck_read_inode(fs, ino, &inode, "process_bad_inode");

	clear_problem_context(&pctx);
	pctx.ino = ino;
	pctx.dir = dir;
	pctx.inode = &inode;

	if (!LINUX_S_ISDIR(inode.i_mode) && !LINUX_S_ISREG(inode.i_mode) &&
	    !LINUX_S_ISCHR(inode.i_mode) && !LINUX_S_ISBLK(inode.i_mode) &&
	    !LINUX_S_ISLNK(inode.i_mode) && !LINUX_S_ISFIFO(inode.i_mode) &&
	    !(LINUX_S_ISSOCK(inode.i_mode))) {
		if (fix_problem(fs, PR_2_BAD_MODE, &pctx)) {
			deallocate_inode(fs, ino, 0);
			return 1;
		}
	}
	if (inode.i_faddr &&
	    fix_problem(fs, PR_2_FADDR_ZERO, &pctx)) {
		inode.i_faddr = 0;
		inode_modified++;
	}

	switch (fs->super->s_creator_os) {
	    case EXT2_OS_LINUX:
		frag = &inode.osd2.linux2.l_i_frag;
		fsize = &inode.osd2.linux2.l_i_fsize;
		break;
	    case EXT2_OS_HURD:
		frag = &inode.osd2.hurd2.h_i_frag;
		fsize = &inode.osd2.hurd2.h_i_fsize;
		break;
	    case EXT2_OS_MASIX:
		frag = &inode.osd2.masix2.m_i_frag;
		fsize = &inode.osd2.masix2.m_i_fsize;
		break;
	    default:
		frag = fsize = 0;
	}
	if (frag && *frag) {
		pctx.num = *frag;
		if (fix_problem(fs, PR_2_FRAG_ZERO, &pctx)) {
			*frag = 0;
			inode_modified++;
		}
		pctx.num = 0;
	}
	if (fsize && *fsize) {
		pctx.num = *fsize;
		if (fix_problem(fs, PR_2_FSIZE_ZERO, &pctx)) {
			*fsize = 0;
			inode_modified++;
		}
		pctx.num = 0;
	}

	if (inode.i_file_acl &&
	    fix_problem(fs, PR_2_FILE_ACL_ZERO, &pctx)) {
		inode.i_file_acl = 0;
		inode_modified++;
	}
	if (inode.i_dir_acl &&
	    fix_problem(fs, PR_2_DIR_ACL_ZERO, &pctx)) {
		inode.i_dir_acl = 0;
		inode_modified++;
	}
	if (inode_modified)
		e2fsck_write_inode(fs, ino, &inode, "process_bad_inode");
	return 0;
}


/*
 * allocate_dir_block --- this function allocates a new directory
 * 	block for a particular inode; this is done if a directory has
 * 	a "hole" in it, or if a directory has a illegal block number
 * 	that was zeroed out and now needs to be replaced.
 */
static int allocate_dir_block(ext2_filsys fs,
			      struct ext2_db_entry *db,
			      char *buf, struct problem_context *pctx)
{
	blk_t			blk;
	char			*block;
	struct ext2_inode	inode;
	errcode_t		retval;

	if (fix_problem(fs, PR_2_DIRECTORY_HOLE, pctx) == 0)
		return 1;

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
		com_err("allocate_dir_block", retval,
			"while trying to fill a hole in a directory inode");
		return 1;
	}
	ext2fs_mark_block_bitmap(block_found_map, blk);
	ext2fs_mark_block_bitmap(fs->block_map, blk);
	ext2fs_mark_bb_dirty(fs);

	/*
	 * Now let's create the actual data block for the inode
	 */
	if (db->blockcnt)
		retval = ext2fs_new_dir_block(fs, 0, 0, &block);
	else
		retval = ext2fs_new_dir_block(fs, db->ino, EXT2_ROOT_INO,
					      &block);

	if (retval) {
		com_err("allocate_dir_block", retval,
			"while creating new directory block");
		return 1;
	}

	retval = ext2fs_write_dir_block(fs, blk, block);
	free(block);
	if (retval) {
		com_err("allocate_dir_block", retval,
			"while writing an empty directory block");
		return 1;
	}

	/*
	 * Update the inode block count
	 */
	e2fsck_read_inode(fs, db->ino, &inode, "allocate_dir_block");
	inode.i_blocks += fs->blocksize / 512;
	if (inode.i_size < (db->blockcnt+1) * fs->blocksize)
		inode.i_size = (db->blockcnt+1) * fs->blocksize;
	e2fsck_write_inode(fs, db->ino, &inode, "allocate_dir_block");

	/*
	 * Finally, update the block pointers for the inode
	 */
	db->blk = blk;
	retval = ext2fs_block_iterate(fs, db->ino, BLOCK_FLAG_HOLE,
				      0, update_dir_block, db);
	if (retval) {
		com_err("allocate_dir_block", retval,
			"while calling ext2fs_block_iterate");
		return 1;
	}

	return 0;
}

/*
 * This is a helper function for allocate_dir_block().
 */
static int update_dir_block(ext2_filsys fs,
			    blk_t	*block_nr,
			    int blockcnt,
			    void *private)
{
	struct ext2_db_entry *db = private;

	if (db->blockcnt == blockcnt) {
		*block_nr = db->blk;
		return BLOCK_CHANGED;
	}
	return 0;
}
	
