/*
 * problem.h --- e2fsck problem error codes
 *
 * Copyright 1996 by Theodore Ts'o
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

struct problem_context {
	ino_t ino, ino2, dir;
	struct ext2_inode *inode;
	struct ext2_dir_entry *dirent;
	blk_t	blk;
	int	blkcount, group;
	__u32	num;
};

struct e2fsck_problem {
	int		e2p_code;
	const char *	e2p_description;
	char		prompt;
	short		flags;
};

#define PR_PREEN_OK	0x0001	/* Don't need to do preenhalt */
#define PR_NO_OK	0x0002	/* If user answers no, don't make fs invalid */
#define PR_NO_DEFAULT	0x0004	/* Default to no */
#define PR_MSG_ONLY	0x0008	/* Print message only */

/*
 * We define a set of "latch groups"; these are problems which are
 * handled as a set.  The user answers once for a particular latch
 * group.
 */
#define PR_LATCH_MASK	0x0070  /* Latch mask */
#define PR_LATCH_BLOCK	0x0010	/* Latch for illegal blocks (pass 1) */
#define PR_LATCH_BBLOCK	0x0020	/* Latch for bad block inode blocks (pass 1) */

#define PR_LATCH(x)	((((x) & PR_LATCH_MASK) >> 4) - 1)

/*
 * Pre-Pass 1 errors
 */

/* Block bitmap not in group */
#define PR_0_BB_NOT_GROUP	0x000001

/* Inode bitmap not in group */
#define PR_0_IB_NOT_GROUP	0x000002

/* Inode table not in group */
#define PR_0_ITABLE_NOT_GROUP	0x000003

/*
 * Pass 1 errors
 */

/* Root directory is not an inode */
#define PR_1_ROOT_NO_DIR	0x010001

/* Root directory has dtime set */
#define PR_1_ROOT_DTIME		0x010002

/* Reserved inode has bad mode */
#define PR_1_RESERVED_BAD_MODE	0x010003

/* Deleted inode has zero dtime */
#define PR_1_ZERO_DTIME		0x010004

/* Inode in use, but dtime set */
#define PR_1_SET_DTIME		0x010005

/* Zero-length directory */
#define PR_1_ZERO_LENGTH_DIR	0x010006

/* Block bitmap conflicts with some other fs block */
#define PR_1_BB_CONFLICT	0x010007

/* Inode bitmap conflicts with some other fs block */
#define PR_1_IB_CONFLICT	0x010008

/* Inode table conflicts with some other fs block */
#define PR_1_ITABLE_CONFLICT	0x010009

/* Block bitmap is on a bad block */
#define PR_1_BB_BAD_BLOCK	0x01000A

/* Inode bitmap is on a bad block */
#define PR_1_IB_BAD_BLOCK	0x01000B

/* Inode has incorrect i_size */
#define PR_1_BAD_I_SIZE		0x01000C

/* Inode has incorrect i_blocks */
#define PR_1_BAD_I_BLOCKS	0x01000D

/* Illegal block number in inode */
#define PR_1_ILLEGAL_BLOCK_NUM	0x01000E

/* Block number overlaps fs metadata */
#define PR_1_BLOCK_OVERLAPS_METADATA	0x01000F

/* Inode has illegal blocks (latch question) */
#define PR_1_INODE_BLOCK_LATCH	0x010010

/* Too many bad blocks in inode */
#define	PR_1_TOO_MANY_BAD_BLOCKS 0x010011
	
/* Illegal block number in bad block inode */
#define PR_1_BB_ILLEGAL_BLOCK_NUM 0x010012

/* Bad block inode has illegal blocks (latch question) */
#define PR_1_INODE_BBLOCK_LATCH	0x010013

/*
 * Pass 1b errors
 */

/* File has duplicate blocks */
#define PR_1B_DUP_FILE		0x011001

/* List of files sharing duplicate blocks */	
#define PR_1B_DUP_FILE_LIST	0x011002

/* File sharing blocks with filesystem metadata  */	
#define PR_1B_SHARE_METADATA	0x011003

/*
 * Pass 2 errors
 */

/* Bad inode number for '.' */
#define PR_2_BAD_INODE_DOT	0x020001

/* Directory entry has bad inode number */
#define PR_2_BAD_INO		0x020002

/* Directory entry has deleted or unused inode */
#define PR_2_UNUSED_INODE	0x020003

/* Directry entry is link to '.' */
#define PR_2_LINK_DOT		0x020004

/* Directory entry points to inode now located in a bad block */
#define PR_2_BB_INODE		0x020005

/* Directory entry contains a link to a directory */
#define PR_2_LINK_DIR		0x020006

/* Directory entry contains a link to the root directry */
#define PR_2_LINK_ROOT		0x020007

/* Directory entry has illegal characters in its name */
#define PR_2_BAD_NAME		0x020008

/* Missing '.' in directory inode */	  
#define PR_2_MISSING_DOT	0x020009

/* Missing '..' in directory inode */	  
#define PR_2_MISSING_DOT_DOT	0x02000A

/* First entry in directory inode doesn't contain '.' */
#define PR_2_1ST_NOT_DOT	0x02000B

/* Second entry in directory inode doesn't contain '..' */
#define PR_2_2ND_NOT_DOT_DOT	0x02000C

/* i_faddr should be zero */
#define PR_2_FADDR_ZERO		0x02000D

/* i_file_acl should be zero */
#define PR_2_FILE_ACL_ZERO	0x02000E

/* i_dir_acl should be zero */
#define PR_2_DIR_ACL_ZERO	0x02000F

/* i_frag should be zero */
#define PR_2_FRAG_ZERO		0x020010

/* i_fsize should be zero */
#define PR_2_FSIZE_ZERO		0x020011
		  
/* inode has bad mode */
#define PR_2_BAD_MODE		0x020012

/* directory corrupted */
#define PR_2_DIR_CORRUPTED	0x020013
		  
/* filename too long */
#define PR_2_FILENAME_LONG	0x020014
		  
/* Directory inode has a missing block (hole) */
#define PR_2_DIRECTORY_HOLE	0x020015

/* '.' is not NULL terminated */
#define PR_2_DOT_NULL_TERM	0x020016

/* '..' is not NULL terminated */
#define PR_2_DOT_DOT_NULL_TERM	0x020017

/*
 * Pass 3 errors
 */

/* Root inode not allocated */
#define PR_3_NO_ROOT_INODE	0x030001

/* No room in lost+found */
#define PR_3_EXPAND_LF_DIR	0x030002

/* Unconnected directory inode */
#define PR_3_UNCONNECTED_DIR	0x030003

/* /lost+found not found */
#define PR_3_NO_LF_DIR		0x030004

/* .. entry is incorrect */
#define PR_3_BAD_DOT_DOT	0x030005


/*
 * Pass 4 errors
 */

/* Unattached zero-length inode */
#define PR_4_ZERO_LEN_INODE	0x040001

/* Unattached inode */
#define PR_4_UNATTACHED_INODE	0x040002

/* Inode ref count wrong */
#define PR_4_BAD_REF_COUNT	0x040003

/*
 * Pass 5 errors
 */

/*
 * Function declarations
 */
int fix_problem(ext2_filsys fs, int code, struct problem_context *ctx);
void reset_problem_latch(int mask);
void suppress_latch_group(int mask, int value);
void clear_problem_context(struct problem_context *ctx);

/* message.c */
void print_e2fsck_message(ext2_filsys fs, const char *msg,
			  struct problem_context *ctx, int first);

