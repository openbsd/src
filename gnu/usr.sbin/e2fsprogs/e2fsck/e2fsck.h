/*
 * e2fsck.h
 * 
 * Copyright (C) 1993, 1994 Theodore Ts'o.  This file may be
 * redistributed under the terms of the GNU Public License.
 * 
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>

#ifdef HAVE_LINUX_FS_H
#include <linux/fs.h>
#endif
#include <linux/ext2_fs.h>

#include "ext2fs/ext2fs.h"

#ifdef __STDC__
#define NOARGS void
#else
#define NOARGS
#define const
#endif

/*
 * Exit codes used by fsck-type programs
 */
#define FSCK_OK          0	/* No errors */
#define FSCK_NONDESTRUCT 1	/* File system errors corrected */
#define FSCK_REBOOT      2	/* System should be rebooted */
#define FSCK_UNCORRECTED 4	/* File system errors left uncorrected */
#define FSCK_ERROR       8	/* Operational error */
#define FSCK_USAGE       16	/* Usage or syntax error */
#define FSCK_LIBRARY     128	/* Shared library error */

/*
 * The last ext2fs revision level that this version of e2fsck is able to
 * support
 */
#define E2FSCK_CURRENT_REV	1

/*
 * Inode count arrays
 */
extern ext2_icount_t	inode_count;
extern ext2_icount_t	inode_link_info;

/*
 * The directory information structure; stores directory information
 * collected in earlier passes, to avoid disk i/o in fetching the
 * directory information.
 */
struct dir_info {
	ino_t			ino;	/* Inode number */
	ino_t			dotdot;	/* Parent according to '..' */
	ino_t			parent; /* Parent according to treewalk */
};

/*
 * This structure is used for keeping track of how much resources have
 * been used for a particular pass of e2fsck.
 */
struct resource_track {
	struct timeval time_start;
	struct timeval user_start;
	struct timeval system_start;
	void	*brk_start;
};

/*
 * Variables
 */
extern const char * program_name;
extern const char * device_name;

extern ext2fs_inode_bitmap inode_used_map; /* Inodes which are in use */
extern ext2fs_inode_bitmap inode_bad_map; /* Inodes which are bad somehow */
extern ext2fs_inode_bitmap inode_dir_map; /* Inodes which are directories */
extern ext2fs_inode_bitmap inode_bb_map; /* Inodes which are in bad blocks */

extern ext2fs_block_bitmap block_found_map; /* Blocks which are in use */
extern ext2fs_block_bitmap block_dup_map; /* Blocks which are used by more than once */
extern ext2fs_block_bitmap block_illegal_map; /* Meta-data blocks */

extern const char *fix_msg[2];	/* Fixed or ignored! */
extern const char *clear_msg[2]; /* Cleared or ignored! */

extern int *invalid_inode_bitmap;
extern int *invalid_block_bitmap;
extern int *invalid_inode_table;
extern int restart_e2fsck;

/* Command line options */
extern int nflag;
extern int yflag;
extern int tflag;
extern int preen;
extern int verbose;
extern int list;
extern int debug;
extern int force;

extern int rwflag;

extern int inode_buffer_blocks;
extern int process_inode_size;
extern int directory_blocks;

extern int no_bad_inode;
extern int no_lpf;
extern int lpf_corrupted;

/* Files counts */
extern int fs_directory_count;
extern int fs_regular_count;
extern int fs_blockdev_count;
extern int fs_chardev_count;
extern int fs_links_count;
extern int fs_symlinks_count;
extern int fs_fast_symlinks_count;
extern int fs_fifo_count;
extern int fs_total_count;
extern int fs_badblocks_count;
extern int fs_sockets_count;
extern int fs_ind_count;
extern int fs_dind_count;
extern int fs_tind_count;
extern int fs_fragmented;

extern struct resource_track	global_rtrack;

extern int invalid_bitmaps;

/*
 * For pass1_check_directory and pass1_get_blocks
 */
extern ino_t stashed_ino;
extern struct ext2_inode *stashed_inode;

/*
 * Procedure declarations
 */

extern void pass1(ext2_filsys fs);
extern void pass1_dupblocks(ext2_filsys fs, char *block_buf);
extern void pass2(ext2_filsys fs);
extern void pass3(ext2_filsys fs);
extern void pass4(ext2_filsys fs);
extern void pass5(ext2_filsys fs);

/* pass1.c */
extern errcode_t pass1_check_directory(ext2_filsys fs, ino_t ino);
extern errcode_t pass1_get_blocks(ext2_filsys fs, ino_t ino, blk_t *blocks);
extern errcode_t pass1_read_inode(ext2_filsys fs, ino_t ino,
				  struct ext2_inode *inode);
extern errcode_t pass1_write_inode(ext2_filsys fs, ino_t ino,
				   struct ext2_inode *inode);

/* badblock.c */
extern void read_bad_blocks_file(ext2_filsys fs, const char *bad_blocks_file,
				 int replace_bad_blocks);
extern void test_disk(ext2_filsys fs);

/* dirinfo.c */
extern void add_dir_info(ext2_filsys fs, ino_t ino, ino_t parent);
extern struct dir_info *get_dir_info(ino_t ino);
extern void free_dir_info(ext2_filsys fs);
extern int get_num_dirs(ext2_filsys fs);
extern struct dir_info *dir_info_iter(int *control);

/* ehandler.c */
extern const char *ehandler_operation(const char *op);
extern void ehandler_init(io_channel channel);

/* swapfs.c */
void swap_filesys(ext2_filsys fs);

/* util.c */
extern void *allocate_memory(int size, const char *description);
extern int ask(const char * string, int def);
extern int ask_yn(const char * string, int def);
extern void fatal_error (const char * fmt_string);
extern void read_bitmaps(ext2_filsys fs);
extern void write_bitmaps(ext2_filsys fs);
extern void preenhalt(ext2_filsys fs);
extern void print_resource_track(struct resource_track *track);
extern void init_resource_track(struct resource_track *track);
extern int inode_has_valid_blocks(struct ext2_inode *inode);
extern void e2fsck_read_inode(ext2_filsys fs, unsigned long ino,
			      struct ext2_inode * inode, const char * proc);
extern void e2fsck_write_inode(ext2_filsys fs, unsigned long ino,
			       struct ext2_inode * inode, const char * proc);
#ifdef MTRACE
extern void mtrace_print(char *mesg);
#endif

#define die(str)	fatal_error(str)

/*
 * pass3.c
 */
extern int reconnect_file(ext2_filsys fs, ino_t inode);
