/*
 * problem.c --- report filesystem problems to the user
 *
 * Copyright 1996, 1997 by Theodore Ts'o
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <termios.h>

#include "e2fsck.h"

#include "problem.h"

#define PROMPT_FIX	0
#define PROMPT_CLEAR	1
#define PROMPT_RELOCATE	2
#define PROMPT_ALLOCATE 3
#define PROMPT_EXPAND	4
#define PROMPT_CONNECT 	5
#define PROMPT_CREATE	6
#define PROMPT_SALVAGE	7
#define PROMPT_TRUNCATE	8
#define PROMPT_CLEAR_INODE 9

/*
 * These are the prompts which are used to ask the user if they want
 * to fix a problem.
 */
static const char *prompt[] = {
	"Fix",			/* 0 */
	"Clear",		/* 1 */
	"Relocate",		/* 2 */
	"Allocate",		/* 3 */
	"Expand",		/* 4 */
	"Connect to /lost+found", /* 5 */
	"Create",		/* 6 */	
	"Salvage",		/* 7 */
	"Truncate",		/* 8 */
	"Clear inode"		/* 9 */
	};

/*
 * These messages are printed when we are preen mode and we will be
 * automatically fixing the problem.
 */
static const char *preen_msg[] = {
	"FIXED",		/* 0 */
	"CLEARED",		/* 1 */
	"RELOCATED",		/* 2 */
	"ALLOCATED",		/* 3 */
	"EXPANDED",		/* 4 */
	"RECONNECTED",		/* 5 */
	"CREATED",		/* 6 */
	"SALVAGED",		/* 7 */
	"TRUNCATED",		/* 8 */
	"INODE CLEARED"		/* 9 */
};

static struct e2fsck_problem problem_table[] = {

	/* Pre-Pass 1 errors */

	/* Block bitmap not in group */
	{ PR_0_BB_NOT_GROUP, "@b @B for @g %g is not in @g.  (@b %b)\n",
	  PROMPT_RELOCATE, 0 }, 

	/* Inode bitmap not in group */
	{ PR_0_IB_NOT_GROUP, "@i @B for @g %g is not in @g.  (@b %b)\n",
	  PROMPT_RELOCATE, 0 }, 

	/* Inode table not in group */
	{ PR_0_ITABLE_NOT_GROUP,
	  "@i table for @g %g is not in @g.  (@b %b)\n"
	  "WARNING: SEVERE DATA LOSS POSSIBLE.\n",
	  PROMPT_RELOCATE, 0 }, 
	
	/* Pass 1 errors */
	
	/* Root directory is not an inode */
	{ PR_1_ROOT_NO_DIR, "@r is not a @d.  ",
	  PROMPT_CLEAR, 0 }, 

	/* Root directory has dtime set */
	{ PR_1_ROOT_DTIME,
	  "@r has dtime set (probably due to old mke2fs).  ",
	  PROMPT_FIX, PR_PREEN_OK },

	/* Reserved inode has bad mode */
	{ PR_1_RESERVED_BAD_MODE,
	  "Reserved @i %i has bad mode.  ",
	  PROMPT_CLEAR, PR_PREEN_OK },

	/* Deleted inode has zero dtime */
	{ PR_1_ZERO_DTIME,
	  "@D @i %i has zero dtime.  ",
	  PROMPT_FIX, PR_PREEN_OK },

	/* Inode in use, but dtime set */
	{ PR_1_SET_DTIME,
	  "@i %i is in use, but has dtime set.  ",
	  PROMPT_FIX, PR_PREEN_OK },

	/* Zero-length directory */
	{ PR_1_ZERO_LENGTH_DIR,
	  "@i %i is a @z @d.  ",
	  PROMPT_CLEAR, PR_PREEN_OK },

	/* Block bitmap conflicts with some other fs block */
	{ PR_1_BB_CONFLICT,
	  "@g %N's @b @B at %b @C.\n",
	  PROMPT_RELOCATE, 0 },

	/* Inode bitmap conflicts with some other fs block */
	{ PR_1_IB_CONFLICT,
	  "@g %N's @i @B at %b @C.\n",
	  PROMPT_RELOCATE, 0 },

	/* Inode table conflicts with some other fs block */
	{ PR_1_ITABLE_CONFLICT,
	  "@g %g's @i table at %b @C.\n",
	  PROMPT_RELOCATE, 0 },

	/* Block bitmap is on a bad block */
	{ PR_1_BB_BAD_BLOCK,
	  "@g %g's @b @B (%b) is bad.  ",
	  PROMPT_RELOCATE, 0 },

	/* Inode bitmap is on a bad block */
	{ PR_1_IB_BAD_BLOCK,
	  "@g %g's @i @B (%b) is bad.  ",
	  PROMPT_RELOCATE, 0 },

	/* Inode has incorrect i_size */
	{ PR_1_BAD_I_SIZE,
	  "@i %i, i_size is %Is, @s %N.  ",
		  PROMPT_FIX, PR_PREEN_OK },
		  
	/* Inode has incorrect i_blocks */
	{ PR_1_BAD_I_BLOCKS,
	  "@i %i, i_blocks is %Ib, @s %N.  ",
		  PROMPT_FIX, PR_PREEN_OK },

	/* Illegal block number in inode */
	{ PR_1_ILLEGAL_BLOCK_NUM,
	  "Illegal @b #%B (%b) in @i %i.  ",
	  PROMPT_CLEAR, PR_LATCH_BLOCK },

	/* Block number overlaps fs metadata */
	{ PR_1_BLOCK_OVERLAPS_METADATA,
	  "@b #%B (%b) overlaps filesystem metadata in @i %i.  ",
	  PROMPT_CLEAR, PR_LATCH_BLOCK },

	/* Inode has illegal blocks (latch question) */
	{ PR_1_INODE_BLOCK_LATCH,
	  "@i %i has illegal @b(s).  ",
	  PROMPT_CLEAR, 0 },

	/* Too many bad blocks in inode */
	{ PR_1_TOO_MANY_BAD_BLOCKS,
	  "Too many illegal @bs in @i %i.\n",
	  PROMPT_CLEAR_INODE, PR_NO_OK }, 	

	/* Illegal block number in bad block inode */
	{ PR_1_BB_ILLEGAL_BLOCK_NUM,
	  "Illegal @b #%B (%b) in bad @b @i.  ",
	  PROMPT_CLEAR, PR_LATCH_BBLOCK },

	/* Bad block inode has illegal blocks (latch question) */
	{ PR_1_INODE_BBLOCK_LATCH,
	  "Bad @b @i has illegal @b(s).  ",
	  PROMPT_CLEAR, 0 },

	/* Pass 1b errors */

	/* File has duplicate blocks */
	{ PR_1B_DUP_FILE,
	  "File %Q (@i #%i, mod time %IM) \n"
	  "  has %B duplicate @b(s), shared with %N file(s):\n",
	  PROMPT_FIX, PR_MSG_ONLY },
		  
	/* List of files sharing duplicate blocks */	
	{ PR_1B_DUP_FILE_LIST,
	  "\t%Q (@i #%i, mod time %IM)\n",
	  PROMPT_FIX, PR_MSG_ONLY },
	  
	/* File sharing blocks with filesystem metadata  */	
	{ PR_1B_SHARE_METADATA,
	  "\t<filesystem metadata>\n",
	  PROMPT_FIX, PR_MSG_ONLY },

	/* Pass 2 errors */

	/* Bad inode number for '.' */
	{ PR_2_BAD_INODE_DOT,
	  "Bad @i number for '.' in @d @i %i.\n",
	  PROMPT_FIX, 0 },

	/* Directory entry has bad inode number */
	{ PR_2_BAD_INO, 
	  "@E has bad @i #: %Di.\n",
	  PROMPT_CLEAR, 0 },

	/* Directory entry has deleted or unused inode */
	{ PR_2_UNUSED_INODE, 
	  "@E has @D/unused @i %Di.  ",
	  PROMPT_CLEAR, PR_PREEN_OK },

	/* Directry entry is link to '.' */
	{ PR_2_LINK_DOT, 
	  "@E @L to '.'  ",
	  PROMPT_CLEAR, 0 },

	/* Directory entry points to inode now located in a bad block */
	{ PR_2_BB_INODE,
	  "@E points to @i (%Di) located in a bad @b.\n",
	  PROMPT_CLEAR, 0 },

	/* Directory entry contains a link to a directory */
	{ PR_2_LINK_DIR, 
	  "@E @L to @d %P (%Di).\n",
	  PROMPT_CLEAR, 0 },

	/* Directory entry contains a link to the root directry */
	{ PR_2_LINK_ROOT, 
	  "@E @L to the @r.\n",
	  PROMPT_CLEAR, 0 },

	/* Directory entry has illegal characters in its name */
	{ PR_2_BAD_NAME, 
	  "@E has illegal characters in its name.\n",
	  PROMPT_FIX, 0 },

	/* Missing '.' in directory inode */	  
	{ PR_2_MISSING_DOT,
	  "Missing '.' in @d @i %i.\n",
	  PROMPT_FIX, 0 },

	/* Missing '..' in directory inode */	  
	{ PR_2_MISSING_DOT_DOT,
	  "Missing '..' in @d @i %i.\n",
	  PROMPT_FIX, 0 },

	/* First entry in directory inode doesn't contain '.' */
	{ PR_2_1ST_NOT_DOT,
	  "First @e '%Dn' (inode=%Di) in @d @i %i (%p) @s '.'\n",
	  PROMPT_FIX, 0 },

	/* Second entry in directory inode doesn't contain '..' */
	{ PR_2_2ND_NOT_DOT_DOT,
	  "Second @e '%Dn' (inode=%Di) in @d @i %i @s '..'\n",
	  PROMPT_FIX, 0 },
		  
	/* i_faddr should be zero */
	{ PR_2_FADDR_ZERO,
	  "i_faddr @F %IF, @s zero.\n",
	  PROMPT_CLEAR, 0 },

  	/* i_file_acl should be zero */
	{ PR_2_FILE_ACL_ZERO,
	  "i_file_acl @F %If, @s zero.\n",
	  PROMPT_CLEAR, 0 },

  	/* i_dir_acl should be zero */
	{ PR_2_DIR_ACL_ZERO,
	  "i_dir_acl @F %Id, @s zero.\n",
	  PROMPT_CLEAR, 0 },

  	/* i_frag should be zero */
	{ PR_2_FRAG_ZERO,
	  "i_frag @F %N, @s zero.\n",
	  PROMPT_CLEAR, 0 },

  	/* i_fsize should be zero */
	{ PR_2_FSIZE_ZERO,
	  "i_fsize @F %N, @s zero.\n",
	  PROMPT_CLEAR, 0 },

	/* inode has bad mode */
	{ PR_2_BAD_MODE,
	  "@i %i (%Q) has a bad mode (%Im).\n",
	  PROMPT_CLEAR, 0 },

	/* directory corrupted */
	{ PR_2_DIR_CORRUPTED,	  
	  "@d @i %i, @b %B, offset %N: @d corrupted\n",
	  PROMPT_SALVAGE, 0 },
		  
	/* filename too long */
	{ PR_2_FILENAME_LONG,	  
	  "@d @i %i, @b %B, offset %N: filename too long\n",
	  PROMPT_TRUNCATE, 0 },

	/* Directory inode has a missing block (hole) */
	{ PR_2_DIRECTORY_HOLE,	  
	  "@d @i %i has an unallocated @b #%B.  ",
	  PROMPT_ALLOCATE, 0 },

	/* '.' is not NULL terminated */
	{ PR_2_DOT_NULL_TERM,
	  "'.' directory entry in @d @i %i is not NULL terminated\n",
	  PROMPT_FIX, 0 },

	/* '..' is not NULL terminated */
	{ PR_2_DOT_DOT_NULL_TERM,
	  "'..' directory entry in @d @i %i is not NULL terminated\n",
	  PROMPT_FIX, 0 },

	  /* Pass 3 errors */

	/* Root inode not allocated */
	{ PR_3_NO_ROOT_INODE,
	  "@r not allocated.  ",
	  PROMPT_ALLOCATE, 0 },	
		  
	/* No room in lost+found */
	{ PR_3_EXPAND_LF_DIR,
	  "No room in @l @d.  ",
	  PROMPT_EXPAND, 0 },

	/* Unconnected directory inode */
	{ PR_3_UNCONNECTED_DIR,
	  "Unconnected @d @i %i (%p)\n",
	  PROMPT_CONNECT, 0 },

	/* /lost+found not found */
	{ PR_3_NO_LF_DIR,
	  "/@l not found.  ",
	  PROMPT_CREATE, 0 },

	/* .. entry is incorrect */
	{ PR_3_BAD_DOT_DOT,
	  "'..' in %Q (%i) is %P (%j), @s %q (%d).\n",
	  PROMPT_FIX, 0 },

	/* Pass 4 errors */
	
	/* Unattached zero-length inode */
	{ PR_4_ZERO_LEN_INODE,
	  "@u @z @i %i.  ",
	  PROMPT_CLEAR, PR_PREEN_OK|PR_NO_OK },

	/* Unattached inode */
	{ PR_4_UNATTACHED_INODE,
	  "@u @i %i\n",
	  PROMPT_CONNECT, 0 },

	/* Inode ref count wrong */
	{ PR_4_BAD_REF_COUNT,
	  "@i %i ref count is %Il, @s %N.  ",
	  PROMPT_FIX, PR_PREEN_OK },
	
	{ 0 }
};

/*
 * This is the latch flags register.  It allows several problems to be
 * "latched" together.  This means that the user has to answer but one
 * question for the set of problems, and all of the associated
 * problems will be either fixed or not fixed.
 */
char pr_latch[7];		/* Latch flags register */
char pr_suppress[7];		/* Latch groups which are suppressed */
int latch_question[7] = {
	PR_1_INODE_BLOCK_LATCH,
	PR_1_INODE_BBLOCK_LATCH	
};

static struct e2fsck_problem *find_problem(int code)
{
	int 	i;

	for (i=0; problem_table[i].e2p_code; i++) {
		if (problem_table[i].e2p_code == code)
			return &problem_table[i];
	}
	return 0;
}

void reset_problem_latch(int mask)
{
	pr_latch[PR_LATCH(mask)] = 0;
	pr_suppress[PR_LATCH(mask)] = 0;
}

void suppress_latch_group(int mask, int value)
{
	pr_suppress[PR_LATCH(mask)] = value;
}

void clear_problem_context(struct problem_context *ctx)
{
	memset(ctx, 0, sizeof(struct problem_context));
	ctx->blkcount = -1;
	ctx->group = -1;
}

int fix_problem(ext2_filsys fs, int code, struct problem_context *ctx)
{
	struct e2fsck_problem *ptr;
	int 		def_yn, answer;
	int		latch;
	int		print_answer = 0;
	int		suppress = 0;

	ptr = find_problem(code);
	if (!ptr) {
		printf("Unhandled error code (%d)!\n", code);
		return 0;
	}
	def_yn = (ptr->flags & PR_NO_DEFAULT) ? 0 : 1;

	/*
	 * Do special latch processing.  This is where we ask the
	 * latch question, if it exists
	 */
	if (ptr->flags & PR_LATCH_MASK) {
		latch = PR_LATCH(ptr->flags);
		if (latch_question[latch] && !pr_latch[latch])
			pr_latch[latch] = fix_problem(fs,
						      latch_question[latch],
						      ctx) + 1;
		if (pr_suppress[latch])
			suppress++;
	}

	if (!suppress) {
		if (preen)
			printf("%s: ", device_name);
		print_e2fsck_message(fs, ptr->e2p_description, ctx, 1);
	}
	if (!(ptr->flags & PR_PREEN_OK))
		preenhalt(fs);

	if (ptr->flags & PR_MSG_ONLY)
		return 1;
	
	if (preen) {
		answer = def_yn;
		print_answer = 1;
	} else if (ptr->flags & PR_LATCH_MASK) {
		latch = PR_LATCH(ptr->flags);
		if (!pr_latch[latch])
			pr_latch[latch] =
				ask(prompt[(int) ptr->prompt], def_yn) + 1;
		else
			print_answer = 1;
		answer = pr_latch[latch] - 1;
	} else
		answer = ask(prompt[(int) ptr->prompt], def_yn);
	if (!answer && !(ptr->flags & PR_NO_OK))
		ext2fs_unmark_valid(fs);
	
	if (print_answer)
		printf("%s.\n",
		       answer ? preen_msg[(int) ptr->prompt] : "IGNORED");
	
	return answer;
}
