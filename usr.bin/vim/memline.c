/*	$OpenBSD: memline.c,v 1.2 1996/09/21 06:23:08 downsj Exp $	*/
/* vi:set ts=4 sw=4:
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 */

/* for debugging */
/* #define CHECK(c, s)	if (c) EMSG(s) */
#define CHECK(c, s)

/*
 * memline.c: Contains the functions for appending, deleting and changing the
 * text lines. The memfile functions are used to store the information in blocks
 * of memory, backed up by a file. The structure of the information is a tree.
 * The root of the tree is a pointer block. The leaves of the tree are data
 * blocks. In between may be several layers of pointer blocks, forming branches.
 *
 * Three types of blocks are used:
 * - Block nr 0 contains information for recovery
 * - Pointer blocks contain list of pointers to other blocks.
 * - Data blocks contain the actual text.
 *
 * Block nr 0 contains the block0 structure (see below).
 *
 * Block nr 1 is the first pointer block. It is the root of the tree.
 * Other pointer blocks are branches.
 *
 *    If a line is too big to fit in a single page, the block
 *    containing that line is made big enough to hold the line. It may span
 *	  several pages. Otherwise all blocks are one page.
 *
 *    A data block that was filled when starting to edit a file and was not
 *	  changed since then, can have a negative block number. This means that it
 *	  has not yet been assigned a place in the file. When recovering, the lines
 * 	  in this data block can be read from the original file. When the block is
 *    changed (lines appended/deleted/changed) or when it is flushed it gets
 *    a positive number. Use mf_trans_del() to get the new number, before
 *    calling mf_get().
 */

/*
 * The following functions are available to work with a memline:
 * 
 * ml_open()			open a new memline for a buffer
 * ml_setname()			change the file name for a memline
 * ml_close()			close the memline for a buffer
 * ml_close_all()		close all memlines
 * ml_recover()			read a memline for recovery
 * ml_sync_all()		flush changed blocks to file for all files
 * ml_preserve()		flush everything into the file for one file
 * ml_get()				get a pointer to a line
 * ml_get_pos()			get a pointer to a position in a line
 * ml_get_cursor()		get a pointer to the char under the cursor
 * ml_get_buf()			get a pointer to a line in a specific buffer
 * ml_line_alloced()	return TRUE if line was allocated
 * ml_append()			append a new line
 * ml_replace()			replace a line
 * ml_delete()			delete a line
 * ml_setmarked()		set mark for a line (for :global command)
 * ml_firstmarked()		get first line with a mark (for :global command)
 * ml_clearmarked()		clear all line marks (for :global command)
 */

#if defined MSDOS  ||  defined WIN32
# include <io.h>
#endif

#include "vim.h"
#include "globals.h"
#include "proto.h"
#include "option.h"
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#ifndef UNIX			/* it's in unix.h for Unix */
# include <time.h>
#endif

#ifdef SASC
# include <proto/dos.h>		/* for Open() and Close() */
#endif

typedef struct block0			ZERO_BL;	/* contents of the first block */
typedef struct pointer_block	PTR_BL;		/* contents of a pointer block */
typedef struct data_block		DATA_BL;	/* contents of a data block */
typedef struct pointer_entry	PTR_EN;		/* block/line-count pair */

#define DATA_ID		(('d' << 8) + 'a')		/* data block id */
#define PTR_ID		(('p' << 8) + 't')		/* pointer block id */
#define BLOCK0_ID0	'b'						/* block 0 id 0 */
#define BLOCK0_ID1	'0'						/* block 0 id 1 */

/*
 * pointer to a block, used in a pointer block
 */
struct pointer_entry
{
	blocknr_t	pe_bnum;		/* block number */
	linenr_t	pe_line_count;	/* number of lines in this branch */
	linenr_t	pe_old_lnum;	/* lnum for this block (for recovery) */
	int			pe_page_count;	/* number of pages in block pe_bnum */
};

/*
 * A pointer block contains a list of branches in the tree.
 */
struct pointer_block
{
	short_u		pb_id;			/* ID for pointer block: PTR_ID */
	short_u		pb_count;		/* number of pointer in this block */
	short_u		pb_count_max;	/* maximum value for pb_count */
	PTR_EN		pb_pointer[1];	/* list of pointers to blocks (actually longer)
								 * followed by empty space until end of page */
};

/*
 * A data block is a leaf in the tree.
 *
 * The text of the lines is at the end of the block. The text of the first line
 * in the block is put at the end, the text of the second line in front of it,
 * etc. Thus the order of the lines is the opposite of the line number.
 */
struct data_block
{
	short_u		db_id;			/* ID for data block: DATA_ID */
	unsigned	db_free;		/* free space available */
	unsigned	db_txt_start;	/* byte where text starts */
	unsigned	db_txt_end;		/* byte just after data block */
	linenr_t	db_line_count;	/* number of lines in this block */
	unsigned	db_index[1];	/* index for start of line (actually bigger)
								 * followed by empty space upto db_txt_start
								 * followed by the text in the lines until
								 * end of page */
};

/*
 * The low bits of db_index hold the actual index. The topmost bit is
 * used for the global command to be able to mark a line. 
 * This method is not clean, but otherwise there would be at least one extra
 * byte used for each line.
 * The mark has to be in this place to keep it with the correct line when other
 * lines are inserted or deleted.
 */
#define DB_MARKED		((unsigned)1 << ((sizeof(unsigned) * 8) - 1))
#define DB_INDEX_MASK	(~DB_MARKED)

#define INDEX_SIZE	(sizeof(unsigned))		/* size of one db_index entry */
#define HEADER_SIZE	(sizeof(DATA_BL) - INDEX_SIZE)	/* size of data block header */

#define B0_FNAME_SIZE	900
#define B0_UNAME_SIZE	40
#define B0_HNAME_SIZE	40
/*
 * Restrict the numbers to 32 bits, otherwise most compilers will complain.
 * This won't detect a 64 bit machine that only swaps a byte in the top 32
 * bits, but that is crazy anyway.
 */
#define B0_MAGIC_LONG	0x30313233
#define B0_MAGIC_INT	0x20212223
#define B0_MAGIC_SHORT	0x10111213
#define B0_MAGIC_CHAR	0x55

/*
 * Block zero holds all info about the swap file.
 *
 * NOTE: DEFINITION OF BLOCK 0 SHOULD NOT CHANGE, it makes all existing swap
 * files unusable!
 * 
 * If size of block0 changes anyway, adjust minimal block size
 * in mf_open()!!
 *
 * This block is built up of single bytes, to make it portable accros
 * different machines. b0_magic_* is used to check the byte order and size of
 * variables, because the rest of the swap is not portable.
 */
struct block0
{
	char_u		b0_id[2];		/* id for block 0: BLOCK0_ID0 and BLOCK0_ID1 */
	char_u		b0_version[10];	/* Vim version string */
	char_u		b0_page_size[4];/* number of bytes per page */
	char_u		b0_mtime[4];	/* last modification time of file */
	char_u		b0_ino[4];		/* inode of b0_fname */
	char_u		b0_pid[4];		/* process id of creator (or 0) */
	char_u		b0_uname[B0_UNAME_SIZE]; /* name of user (uid if no name) */
	char_u		b0_hname[B0_HNAME_SIZE]; /* host name (if it has a name) */
	char_u		b0_fname[B0_FNAME_SIZE]; /* name of file being edited */
	long		b0_magic_long;	/* check for byte order of long */
	int			b0_magic_int;	/* check for byte order of int */
	short		b0_magic_short;	/* check for byte order of short */
	char_u		b0_magic_char;	/* check for last char */
};

#define STACK_INCR		5		/* nr of entries added to ml_stack at a time */

/*
 * The line number where the first mark may be is remembered.
 * If it is 0 there are no marks at all.
 * (always used for the current buffer only, no buffer change possible while
 * executing a global command).
 */
static linenr_t	lowest_marked = 0;

/*
 * arguments for ml_find_line()
 */
#define ML_DELETE		0x11		/* delete line */
#define ML_INSERT		0x12		/* insert line */
#define ML_FIND			0x13		/* just find the line */
#define ML_FLUSH		0x02		/* flush locked block */
#define ML_SIMPLE(x)	(x & 0x10)	/* DEL, INS or FIND */

static void ml_open_file __ARGS((BUF *));
static void set_b0_fname __ARGS((ZERO_BL *, BUF *buf));
static void swapfile_info __ARGS((char_u *));
static int get_names __ARGS((char_u **, char_u *));
static int ml_append_int __ARGS((BUF *, linenr_t, char_u *, colnr_t, int));
static int ml_delete_int __ARGS((BUF *, linenr_t, int));
static char_u *findswapname __ARGS((BUF *, char_u **, char_u *));
static void ml_flush_line __ARGS((BUF *));
static BHDR *ml_new_data __ARGS((MEMFILE *, int, int));
static BHDR *ml_new_ptr __ARGS((MEMFILE *));
static BHDR *ml_find_line __ARGS((BUF *, linenr_t, int));
static int ml_add_stack __ARGS((BUF *));
static char_u *makeswapname __ARGS((BUF *, char_u *));
static void ml_lineadd __ARGS((BUF *, int));
static int b0_magic_wrong __ARGS((ZERO_BL *));
#ifdef CHECK_INODE
static int fnamecmp_ino __ARGS((char_u *, char_u *, long));
#endif
static void long_to_char __ARGS((long, char_u *));
static long char_to_long __ARGS((char_u *));

/*
 * open a new memline for 'curbuf'
 *
 * return FAIL for failure, OK otherwise
 */
	int
ml_open()
{
	MEMFILE		*mfp;
	char_u		*fname;
	BHDR		*hp = NULL;
	ZERO_BL		*b0p;
	PTR_BL		*pp;
	DATA_BL		*dp;
	char_u		*dirp;

/*
 * init fields in memline struct
 */
	curbuf->b_ml.ml_stack_size = 0;		/* no stack yet */
	curbuf->b_ml.ml_stack = NULL;		/* no stack yet */
	curbuf->b_ml.ml_stack_top = 0;		/* nothing in the stack */
	curbuf->b_ml.ml_locked = NULL;		/* no cached block */
	curbuf->b_ml.ml_line_lnum = 0;		/* no cached line */

/*
 * Make file name for swap file.
 * If we are unable to find a file name, mf_fname will be NULL
 * and the memfile will be in memory only (no recovery possible).
 * When 'updatecount' is 0 and for help files on Unix there is never a swap
 * file.
 */
#if defined(UNIX) || defined(__EMX__)
/* why only on Unix an exception for help files??? */
	if (p_uc == 0 || curbuf->b_help)
#else
	if (p_uc == 0)
#endif
		dirp = (char_u *)"";
	else
		dirp = p_dir;
	
/*
 * Open the memfile.
 *
 * If no swap file is wanted *dirp will be NUL, try with fname == NULL.
 * Otherwise try all directories in 'directory' option. If that fails try
 * with fname == NULL.
 */
	for (;;)
	{
		if (*dirp == NUL)
			fname = NULL;
		else
		{
			fname = findswapname(curbuf, &dirp, NULL);
			if (fname == NULL)
				continue;
		}
		mfp = mf_open(fname, TRUE);
		/* stop when memfile opened or no other fname to try */
		if (mfp != NULL || (fname == NULL && *dirp == NUL))
			break;
		vim_free(fname);
	}

	if (mfp == NULL)
		goto error;
	curbuf->b_ml.ml_mfp = mfp;
	curbuf->b_ml.ml_flags = ML_EMPTY;
	curbuf->b_ml.ml_line_count = 1;

	if (!(p_uc == 0 || curbuf->b_help) && mfp->mf_fname == NULL)
	{
		need_wait_return = TRUE;		/* call wait_return later */
		++no_wait_return;
		(void)EMSG("Unable to open swap file, recovery impossible");
		--no_wait_return;
	}
#if defined(MSDOS) || defined(WIN32)
	/*
	 * set full pathname for swap file now, because a ":!cd dir" may
	 * change directory without us knowing it.
	 */
	mf_fullname(mfp);
#endif

/*
 * fill block0 struct and write page 0
 */
	if ((hp = mf_new(mfp, FALSE, 1)) == NULL)
		goto error;
	if (hp->bh_bnum != 0)
	{
		EMSG("didn't get block nr 0?");
		goto error;
	}
	b0p = (ZERO_BL *)(hp->bh_data);

	b0p->b0_id[0] = BLOCK0_ID0;
	b0p->b0_id[1] = BLOCK0_ID1;
	b0p->b0_magic_long = (long)B0_MAGIC_LONG;
	b0p->b0_magic_int = (int)B0_MAGIC_INT;
	b0p->b0_magic_short = (short)B0_MAGIC_SHORT;
	b0p->b0_magic_char = B0_MAGIC_CHAR;

	STRNCPY(b0p->b0_version, Version, 10);
	set_b0_fname(b0p, curbuf);
	long_to_char((long)mfp->mf_page_size, b0p->b0_page_size);
	(void)mch_get_user_name(b0p->b0_uname, B0_UNAME_SIZE);
	b0p->b0_uname[B0_UNAME_SIZE - 1] = NUL;
	mch_get_host_name(b0p->b0_hname, B0_HNAME_SIZE);
	b0p->b0_hname[B0_HNAME_SIZE - 1] = NUL;
	long_to_char(mch_get_pid(), b0p->b0_pid);

	/*
	 * Always sync block number 0 to disk, so we can check the file name in
	 * the swap file in findswapname(). Don't do this for help files though.
	 */
	mf_put(mfp, hp, TRUE, FALSE);
	if (!curbuf->b_help)
		mf_sync(mfp, FALSE, FALSE, FALSE);

/*
 * fill in root pointer block and write page 1
 */
	if ((hp = ml_new_ptr(mfp)) == NULL)
		goto error;
	if (hp->bh_bnum != 1)
	{
		EMSG("didn't get block nr 1?");
		goto error;
	}
	pp = (PTR_BL *)(hp->bh_data);
	pp->pb_count = 1;
	pp->pb_pointer[0].pe_bnum = 2;
	pp->pb_pointer[0].pe_page_count = 1;
	pp->pb_pointer[0].pe_old_lnum = 1;
	pp->pb_pointer[0].pe_line_count = 1;	/* line count after insertion */
	mf_put(mfp, hp, TRUE, FALSE);

/*
 * allocate first data block and create an empty line 1.
 */
	if ((hp = ml_new_data(mfp, FALSE, 1)) == NULL)
		goto error;
	if (hp->bh_bnum != 2)
	{
		EMSG("didn't get block nr 2?");
		goto error;
	}

	dp = (DATA_BL *)(hp->bh_data);
	dp->db_index[0] = --dp->db_txt_start;		/* at end of block */
	dp->db_free -= 1 + INDEX_SIZE;
	dp->db_line_count = 1;
	*((char_u *)dp + dp->db_txt_start) = NUL;	/* emtpy line */

	return OK;

error:
	if (mfp != NULL)
	{
		if (hp)
			mf_put(mfp, hp, FALSE, FALSE);
		mf_close(mfp, TRUE);		/* will also free(mfp->mf_fname) */
	}
	else
		vim_free(fname);
	curbuf->b_ml.ml_mfp = NULL;
	return FAIL;
}

/*
 * ml_setname() is called when the file name has been changed.
 * It may rename the swap file.
 */
	void
ml_setname()
{
	int			success = FALSE;
	MEMFILE		*mfp;
	char_u		*fname;
	char_u		*dirp;

/*
 * When 'updatecount' is 0 there is never a swap file.
 * For help files we will make a swap file now.
 */
	if (p_uc == 0)
		return;

	mfp = curbuf->b_ml.ml_mfp;
	if (mfp->mf_fd < 0)				/* there is no swap file yet */
	{
		ml_open_file(curbuf);		/* create a swap file */
		return;
	}

/*
 * Try all directories in the 'directory' option.
 */
	dirp = p_dir;
	for (;;)
	{
		if (*dirp == NUL)			/* tried all directories, fail */
			break;
		fname = findswapname(curbuf, &dirp, mfp->mf_fname);
		if (fname == NULL)			/* no file name found for this dir */
			continue;

#if defined(MSDOS) || defined(WIN32)
		/*
		 * Set full pathname for swap file now, because a ":!cd dir" may
		 * change directory without us knowing it.
		 */
		if (!did_cd)
		{
			char_u		*p;

			p = FullName_save(fname);
			vim_free(fname);
			fname = p;
			if (fname == NULL)
				continue;
		}
#endif
			/* if the file name is the same we don't have to do anything */
		if (fnamecmp(fname, mfp->mf_fname) == 0)
		{
			vim_free(fname);
			success = TRUE;
			break;
		}
			/* need to close the swap file before renaming */
		if (mfp->mf_fd >= 0)
		{
			close(mfp->mf_fd);
			mfp->mf_fd = -1;
		}

			/* try to rename the swap file */
		if (vim_rename(mfp->mf_fname, fname) == 0)
		{
			success = TRUE;
			vim_free(mfp->mf_fname);
			mfp->mf_fname = fname;
			vim_free(mfp->mf_xfname);
			mf_set_xfname(mfp);
			break;
		}
		vim_free(fname);			/* this fname didn't work, try another */
	}

	if (mfp->mf_fd == -1)			/* need to (re)open the swap file */
	{
		mfp->mf_fd = open((char *)mfp->mf_fname, O_RDWR | O_EXTRA);
		if (mfp->mf_fd < 0)
		{
			/* could not (re)open the swap file, what can we do???? */
			EMSG("Oops, lost the swap file!!!");
			return;
		}
	}
	if (!success)
		EMSG("Could not rename swap file");
}

/*
 * Open a file for the memfile for all buffers.
 * Used when 'updatecount' changes from zero to non-zero.
 */
	void
ml_open_files()
{
	BUF			*buf;

	for (buf = firstbuf; buf != NULL; buf = buf->b_next)
		ml_open_file(buf);
}

/*
 * Open a swap file for an existing memfile, if there is non swap file yet.
 */
	static void
ml_open_file(buf)
	BUF			*buf;
{
	MEMFILE		*mfp;
	char_u		*fname;
	char_u		*dirp;

	mfp = buf->b_ml.ml_mfp;
	if (mfp == NULL || mfp->mf_fd >= 0)			/* nothing to do */
		return;

	dirp = p_dir;
/*
 * Try all directories in 'directory' option.
 */
	for (;;)
	{
		if (*dirp == NUL)
			break;
		fname = findswapname(buf, &dirp, NULL);
		if (fname == NULL)
			continue;
		if (mf_open_file(mfp, fname) == OK)
			break;
		vim_free(fname);
	}

	if (mfp->mf_fname == NULL)			/* Failed! */
	{
		need_wait_return = TRUE;		/* call wait_return later */
		++no_wait_return;
		(void)EMSG2("Unable to open swap file for \"%s\", recovery impossible",
				buf->b_xfilename == NULL ? (char_u *)"No File"
										 : buf->b_xfilename);
		--no_wait_return;
	}
#if defined(MSDOS) || defined(WIN32)
	/*
	 * set full pathname for swap file now, because a ":!cd dir" may
	 * change directory without us knowing it.
	 */
	else
		mf_fullname(mfp);
#endif
}

/*
 * Close memline for buffer 'buf'.
 * If 'del_file' is TRUE, delete the swap file
 */
	void
ml_close(buf, del_file)
	BUF		*buf;
	int		del_file;
{
	if (buf->b_ml.ml_mfp == NULL)				/* not open */
		return;
	mf_close(buf->b_ml.ml_mfp, del_file);			/* close the .swp file */
	if (buf->b_ml.ml_line_lnum != 0 && (buf->b_ml.ml_flags & ML_LINE_DIRTY))
		vim_free(buf->b_ml.ml_line_ptr);
	vim_free(buf->b_ml.ml_stack);
	buf->b_ml.ml_mfp = NULL;
}

/*
 * Close all existing memlines and memfiles.
 * Used when exiting.
 * When 'del_file' is TRUE, delete the memfiles.
 */
	void
ml_close_all(del_file)
	int		del_file;
{
	BUF		*buf;

	for (buf = firstbuf; buf != NULL; buf = buf->b_next)
		ml_close(buf, del_file);
}

/*
 * Close all memfiles for not modified buffers.
 * Only use just before exiting!
 */
	void
ml_close_notmod()
{
	BUF		*buf;

	for (buf = firstbuf; buf != NULL; buf = buf->b_next)
		if (!buf->b_changed)
			ml_close(buf, TRUE);	/* close all not-modified buffers */
}

/*
 * Update the timestamp in the .swp file.
 * Used when the file has been written.
 */
	void
ml_timestamp(buf)
	BUF			*buf;
{
	MEMFILE		*mfp;
	BHDR		*hp;
	ZERO_BL		*b0p;

	mfp = buf->b_ml.ml_mfp;

	if (mfp == NULL || (hp = mf_get(mfp, (blocknr_t)0, 1)) == NULL)
		return;
	b0p = (ZERO_BL *)(hp->bh_data);
	if (b0p->b0_id[0] != BLOCK0_ID0 || b0p->b0_id[1] != BLOCK0_ID1)
		EMSG("ml_timestamp: Didn't get block 0??");
	else
		set_b0_fname(b0p, buf);
	mf_put(mfp, hp, TRUE, FALSE);
}

/*
 * Write file name and timestamp into block 0 of a swap file.
 * Also set buf->b_mtime.
 * Don't use NameBuff[]!!!
 */
	static void
set_b0_fname(b0p, buf)
	ZERO_BL		*b0p;
	BUF			*buf;
{
	struct stat	st;
	size_t		flen, ulen;
	char_u		uname[B0_UNAME_SIZE];

	if (buf->b_filename == NULL)
		b0p->b0_fname[0] = NUL;
	else
	{
		/*
		 * For a file under the home directory of the current user, we try to
		 * replace the home directory path with "~user". This helps when
		 * editing the same file on different machines over a network.
		 * First replace home dir path with "~/" with home_replace().
		 * Then insert the user name to get "~user/".
		 */
		home_replace(NULL, buf->b_filename, b0p->b0_fname, B0_FNAME_SIZE);
		if (b0p->b0_fname[0] == '~')
		{
			flen = STRLEN(b0p->b0_fname);
			/* If there is no user name or it is too long, don't use "~/" */
			if (mch_get_user_name(uname, B0_UNAME_SIZE) == FAIL ||
						 (ulen = STRLEN(uname)) + flen > B0_FNAME_SIZE - 1)
				STRNCPY(b0p->b0_fname, buf->b_filename, B0_FNAME_SIZE);
			else
			{
				vim_memmove(b0p->b0_fname + ulen + 1, b0p->b0_fname + 1, flen);
				vim_memmove(b0p->b0_fname + 1, uname, ulen);
			}
		}
		if (stat((char *)buf->b_filename, &st) >= 0)
		{
			long_to_char((long)st.st_mtime, b0p->b0_mtime);
#ifdef CHECK_INODE
			long_to_char((long)st.st_ino, b0p->b0_ino);
#endif
			buf->b_mtime = st.st_mtime;
			buf->b_mtime_read = st.st_mtime;
		}
		else
		{
			long_to_char(0L, b0p->b0_mtime);
#ifdef CHECK_INODE
			long_to_char(0L, b0p->b0_ino);
#endif
			buf->b_mtime = 0;
			buf->b_mtime_read = 0;
		}
	}
}

/*
 * try to recover curbuf from the .swp file
 */
	void
ml_recover()
{
	BUF			*buf = NULL;
	MEMFILE		*mfp = NULL;
	char_u		*fname;
	BHDR		*hp = NULL;
	ZERO_BL		*b0p;
	PTR_BL		*pp;
	DATA_BL		*dp;
	IPTR		*ip;
	blocknr_t	bnum;
	int			page_count;
	struct stat	org_stat, swp_stat;
	int			len;
	int			directly;
	linenr_t	lnum;
	char_u		*p;
	int			i;
	long		error;
	int			cannot_open;
	linenr_t	line_count;
	int			has_error;
	int			idx;
	int			top;
	int			txt_start;
	off_t		size;
	int			called_from_main;
	int			serious_error = TRUE;
	long		mtime;

	called_from_main = (curbuf->b_ml.ml_mfp == NULL);
/*
 * If the file name ends in ".sw?" we use it directly.
 * Otherwise a search is done to find the swap file(s).
 */
	fname = curbuf->b_xfilename;
	if (fname == NULL)				/* When there is no file name */
		fname = (char_u *)"";
	len = STRLEN(fname);
	if (len >= 4 && vim_strnicmp(fname + len - 4,
									(char_u *)".sw", (size_t)3) == 0)
	{
		directly = TRUE;
		fname = strsave(fname);		/* make a copy for mf_open */
	}
	else
	{
		directly = FALSE;

			/* count the number of matching swap files */
		len = recover_names(&fname, FALSE, 0);
		if (len == 0)				/* no swap files found */
		{
			EMSG2("No swap file found for %s", fname);
			fname = NULL;
			goto theend;
		}
		if (len == 1)				/* one swap file found, use it */
			i = 1;
		else						/* several swap files found, choose */
		{
				/* list the names of the swap files */
			(void)recover_names(&fname, TRUE, 0);
			msg_outchar('\n');
			MSG_OUTSTR("Enter number of swap file to use (0 to quit): ");
			i = get_number();
			if (i < 1 || i > len)
			{
				fname = NULL;
				goto theend;
			}
		}
				/* get the swap file name that will be used */
		(void)recover_names(&fname, FALSE, i);
	}
	if (fname == NULL)
		goto theend;					/* out of memory */

	/* When called from main() still need to initialize storage structure */
	if (called_from_main && ml_open() == FAIL)
		getout(1);

/*
 * allocate a buffer structure (only the memline in it is really used)
 */
	buf = (BUF *)alloc((unsigned)sizeof(BUF));
	if (buf == NULL)
		goto theend;

/*
 * init fields in memline struct
 */
	buf->b_ml.ml_stack_size = 0;		/* no stack yet */
	buf->b_ml.ml_stack = NULL;			/* no stack yet */
	buf->b_ml.ml_stack_top = 0;			/* nothing in the stack */
	buf->b_ml.ml_line_lnum = 0;			/* no cached line */
	buf->b_ml.ml_locked = NULL;			/* no locked block */
	buf->b_ml.ml_flags = 0;

/*
 * open the memfile from the old swap file
 */
	mfp = mf_open(fname, FALSE);
	if (mfp == NULL || mfp->mf_fd < 0)
	{
		EMSG2("Cannot open %s", fname);
		goto theend;
	}
	buf->b_ml.ml_mfp = mfp;
#if defined(MSDOS) || defined(WIN32)
	/*
	 * set full pathname for swap file now, because a ":!cd dir" may
	 * change directory without us knowing it.
	 * Careful: May free fname!
	 */
	mf_fullname(mfp);
#endif

/*
 * try to read block 0
 */
	if ((hp = mf_get(mfp, (blocknr_t)0, 1)) == NULL)
	{
		msg_start();
		MSG_OUTSTR("Unable to read block 0 from ");
		msg_outtrans(mfp->mf_fname);
		MSG_OUTSTR("\nMaybe no changes were made or Vim did not update the swap file");
		msg_end();
		goto theend;
	}
	b0p = (ZERO_BL *)(hp->bh_data);
	if (b0p->b0_id[0] != BLOCK0_ID0 || b0p->b0_id[1] != BLOCK0_ID1)
	{
		EMSG2("%s is not a swap file", mfp->mf_fname);
		goto theend;
	}
	if (b0_magic_wrong(b0p))
	{
		msg_start();
		MSG_OUTSTR("The file ");
		msg_outtrans(mfp->mf_fname);
#if defined(MSDOS) || defined(WIN32)
		if (STRNCMP(b0p->b0_hname, "PC ", 3) == 0)
			MSG_OUTSTR(" cannot be used with this version of Vim.\n");
		else
#endif
			MSG_OUTSTR(" cannot be used on this computer.\n");
		MSG_OUTSTR("The file was created on ");
				/* avoid going past the end of currupted hostname */
		b0p->b0_fname[0] = NUL;
		MSG_OUTSTR(b0p->b0_hname);
		MSG_OUTSTR(",\nor the file has been damaged.");
		msg_end();
		goto theend;
	}
	/*
	 * If we guessed the wrong page size, we have to recalculate the
	 * highest block number in the file.
	 */
	if (mfp->mf_page_size != (unsigned)char_to_long(b0p->b0_page_size))
	{
		mfp->mf_page_size = (unsigned)char_to_long(b0p->b0_page_size);
		if ((size = lseek(mfp->mf_fd, 0L, SEEK_END)) <= 0)
			mfp->mf_blocknr_max = 0;		/* no file or empty file */
		else
			mfp->mf_blocknr_max = size / mfp->mf_page_size;
		mfp->mf_infile_count = mfp->mf_blocknr_max;
	}

/*
 * If .swp file name given directly, use name from swap file for buffer.
 */
	if (directly)
	{
		expand_env(b0p->b0_fname, NameBuff, MAXPATHL);
		if (setfname(NameBuff, NULL, TRUE) == FAIL)
			goto theend;
	}

	home_replace(NULL, mfp->mf_fname, NameBuff, MAXPATHL);
	smsg((char_u *)"Using swap file \"%s\"", NameBuff);

	if (curbuf->b_filename == NULL)
		STRCPY(NameBuff, "No File");
	else
		home_replace(NULL, curbuf->b_filename, NameBuff, MAXPATHL);
	smsg((char_u *)"Original file \"%s\"", NameBuff);
	msg_outchar((char_u)'\n');

/*
 * check date of swap file and original file
 */
	mtime = char_to_long(b0p->b0_mtime);
	if (curbuf->b_filename != NULL &&
			stat((char *)curbuf->b_filename, &org_stat) != -1 &&
			((stat((char *)mfp->mf_fname, &swp_stat) != -1 &&
			org_stat.st_mtime > swp_stat.st_mtime) ||
			org_stat.st_mtime != mtime))
	{
		EMSG("Warning: Original file may have been changed");
	}
	flushbuf();
	mf_put(mfp, hp, FALSE, FALSE);		/* release block 0 */
	hp = NULL;

	/*
	 * Now that we are sure that the file is going to be recovered, clear the
	 * contents of the current buffer.
	 */
	while (!(curbuf->b_ml.ml_flags & ML_EMPTY))
		ml_delete((linenr_t)1, FALSE);

	bnum = 1;			/* start with block 1 */
	page_count = 1;		/* which is 1 page */
	lnum = 0;			/* append after line 0 in curbuf */
	line_count = 0;
	idx = 0;			/* start with first index in block 1 */
	error = 0;
	buf->b_ml.ml_stack_top = 0;
	buf->b_ml.ml_stack = NULL;
	buf->b_ml.ml_stack_size = 0;		/* no stack yet */

	if (curbuf->b_filename == NULL)
		cannot_open = TRUE;
	else
		cannot_open = FALSE;

	serious_error = FALSE;
	for ( ; !got_int; line_breakcheck())
	{
		if (hp != NULL)
			mf_put(mfp, hp, FALSE, FALSE);		/* release previous block */

		/*
		 * get block
		 */
		if ((hp = mf_get(mfp, (blocknr_t)bnum, page_count)) == NULL)
		{
			if (bnum == 1)
			{
				EMSG2("Unable to read block 1 from %s", mfp->mf_fname);
				goto theend;
			}
			++error;
			ml_append(lnum++, (char_u *)"???MANY LINES MISSING", (colnr_t)0, TRUE);
		}
		else			/* there is a block */
		{
			pp = (PTR_BL *)(hp->bh_data);
			if (pp->pb_id == PTR_ID)			/* it is a pointer block */
			{
					/* check line count when using pointer block first time */
				if (idx == 0 && line_count != 0)
				{
					for (i = 0; i < (int)pp->pb_count; ++i)
						line_count -= pp->pb_pointer[i].pe_line_count;
					if (line_count != 0)
					{
						++error;
						ml_append(lnum++, (char_u *)"???LINE COUNT WRONG", (colnr_t)0, TRUE);
					}
				}

				if (pp->pb_count == 0)
				{
					ml_append(lnum++, (char_u *)"???EMPTY BLOCK", (colnr_t)0, TRUE);
					++error;
				}
				else if (idx < (int)pp->pb_count)		/* go a block deeper */
				{
					if (pp->pb_pointer[idx].pe_bnum < 0)
					{
						/*
						 * Data block with negative block number.
						 * Try to read lines from the original file.
						 * This is slow, but it works.
						 */
						if (!cannot_open)
						{
							line_count = pp->pb_pointer[idx].pe_line_count;
							if (readfile(curbuf->b_filename, NULL, lnum, FALSE,
										pp->pb_pointer[idx].pe_old_lnum - 1,
										line_count, FALSE) == FAIL)
								cannot_open = TRUE;
							else
								lnum += line_count;
						}
						if (cannot_open)
						{
							++error;
							ml_append(lnum++, (char_u *)"???LINES MISSING", (colnr_t)0, TRUE);
						}
						++idx;		/* get same block again for next index */
						continue;
					}

					/*
					 * going one block deeper in the tree
					 */
					if ((top = ml_add_stack(buf)) < 0)		/* new entry in stack */
					{
						++error;
						break;				/* out of memory */
					}
					ip = &(buf->b_ml.ml_stack[top]);
					ip->ip_bnum = bnum;
					ip->ip_index = idx;

					bnum = pp->pb_pointer[idx].pe_bnum;
					line_count = pp->pb_pointer[idx].pe_line_count;
					page_count = pp->pb_pointer[idx].pe_page_count;
					continue;
				}
			}
			else			/* not a pointer block */
			{
				dp = (DATA_BL *)(hp->bh_data);
				if (dp->db_id != DATA_ID)		/* block id wrong */
				{
					if (bnum == 1)
					{
						EMSG2("Block 1 ID wrong (%s not a .swp file?)",
															   mfp->mf_fname);
						goto theend;
					}
					++error;
					ml_append(lnum++, (char_u *)"???BLOCK MISSING", (colnr_t)0, TRUE);
				}
				else
				{
					/*
					 * it is a data block
					 * Append all the lines in this block
					 */
					has_error = FALSE;
						/*
						 * check length of block
						 * if wrong, use length in pointer block
						 */
					if (page_count * mfp->mf_page_size != dp->db_txt_end)
					{
						ml_append(lnum++, (char_u *)"??? from here until ???END lines may be messed up", (colnr_t)0, TRUE);
						++error;
						has_error = TRUE;
						dp->db_txt_end = page_count * mfp->mf_page_size;
					}
						
						/* make sure there is a NUL at the end of the block */
					*((char_u *)dp + dp->db_txt_end - 1) = NUL;

						/*
						 * check number of lines in block
						 * if wrong, use count in data block
						 */
					if (line_count != dp->db_line_count)
					{
						ml_append(lnum++, (char_u *)"??? from here until ???END lines may have been inserted/deleted", (colnr_t)0, TRUE);
						++error;
						has_error = TRUE;
					}

					for (i = 0; i < dp->db_line_count; ++i)
					{
						txt_start = (dp->db_index[i] & DB_INDEX_MASK);
						if (txt_start <= HEADER_SIZE || txt_start >= (int)dp->db_txt_end)
						{
							p = (char_u *)"???";
							++error;
						}
						else
							p = (char_u *)dp + txt_start;
						ml_append(lnum++, p, (colnr_t)0, TRUE);
					}
					if (has_error)
						ml_append(lnum++, (char_u *)"???END", (colnr_t)0, TRUE);
				}
			}
		}

		if (buf->b_ml.ml_stack_top == 0)		/* finished */
			break;

		/*
		 * go one block up in the tree
		 */
		ip = &(buf->b_ml.ml_stack[--(buf->b_ml.ml_stack_top)]);
		bnum = ip->ip_bnum;
		idx = ip->ip_index + 1;		/* go to next index */
		page_count = 1;
	}

	/*
	 * The dummy line from the empty buffer will now be after the last line in
	 * the buffer. Delete it.
	 */
	ml_delete(curbuf->b_ml.ml_line_count, FALSE);

	recoverymode = FALSE;
	if (got_int)
		EMSG("Recovery Interrupted");
	else if (error)
		EMSG("Errors detected while recovering; look for lines starting with ???");
	else
	{
		MSG("Recovery completed. You should check if everything is OK.");
		MSG_OUTSTR("\n(You might want to write out this file under another name\n");
		MSG_OUTSTR("and run diff with the original file to check for changes)\n");
		MSG_OUTSTR("Delete the .swp file afterwards.\n\n");
		cmdline_row = msg_row;
	}

theend:
	if (mfp != NULL)
	{
		if (hp != NULL)
			mf_put(mfp, hp, FALSE, FALSE);
		mf_close(mfp, FALSE);		/* will also vim_free(mfp->mf_fname) */
	}
	else
		vim_free(fname);
	vim_free(buf);
	if (serious_error && called_from_main)
		ml_close(curbuf, TRUE);
	return;
}

/*
 * Find the names of swap files in current directory and the directory given
 * with the 'directory' option.
 *
 * Used to:
 * - list the swap files for "vim -r"
 * - count the number of swap files when recovering
 * - list the swap files when recovering
 * - find the name of the n'th swap file when recovering
 */
	int
recover_names(fname, list, nr)
	char_u		**fname;	/* base for swap file name */
	int			list;		/* when TRUE, list the swap file names */
	int			nr;			/* when non-zero, return nr'th swap file name */
{
	int			num_names;
	char_u		*(names[6]);
	char_u		*tail;
	char_u		*p;
	int			num_files;
	int			file_count = 0;
	char_u		**files;
	int			i;
	char_u		*dirp;
	char_u		*dir_name;

	if (list)
	{
			/* use msg() to start the scrolling properly */
		msg((char_u *)"Swap files found:");
		msg_outchar('\n');
	}
	expand_interactively = TRUE;

	/*
	 * Do the loop for every directory in 'directory'.
	 * First allocate some memory to put the directory name in.
	 */
	dir_name = alloc((unsigned)STRLEN(p_dir) + 1);
	dirp = p_dir;
	while (dir_name != NULL && *dirp)
	{
		/*
		 * Isolate a directory name from *dirp and put it in dir_name.
		 * Advance dirp to next directory name.
		 */
		(void)copy_option_part(&dirp, dir_name, 31000, ",");

		if (dir_name[0] == '.' && dir_name[1] == NUL)	/* check current dir */
		{
			if (fname == NULL || *fname == NULL)
			{
				names[0] = strsave((char_u *)"*.sw?");
#ifdef UNIX
					/* for Unix names starting with a dot are special */
				names[1] = strsave((char_u *)".*.sw?");
				names[2] = strsave((char_u *)".sw?");
				num_names = 3;
#else
				num_names = 1;
#endif
			}
			else
				num_names = get_names(names, *fname);
		}
		else						/* check directory dir_name */
		{
			if (fname == NULL || *fname == NULL)
			{
				names[0] = concat_fnames(dir_name, (char_u *)"*.sw?", TRUE);
#ifdef UNIX
					/* for Unix names starting with a dot are special */
				names[1] = concat_fnames(dir_name, (char_u *)".*.sw?", TRUE);
				names[2] = concat_fnames(dir_name, (char_u *)".sw?", TRUE);
				num_names = 3;
#else
				num_names = 1;
#endif
			}
			else
			{
				tail = gettail(*fname);
				tail = concat_fnames(dir_name, tail, TRUE);
				if (tail == NULL)
					num_names = 0;
				else
				{
					num_names = get_names(names, tail);
					vim_free(tail);
				}
			}
		}

			/* check for out-of-memory */
		for (i = 0; i < num_names; ++i)
		{
			if (names[i] == NULL)
			{
				for (i = 0; i < num_names; ++i)
					vim_free(names[i]);
				num_names = 0;
			}
		}
		if (num_names == 0)
			num_files = 0;
		else if (ExpandWildCards(num_names, names,
									 &num_files, &files, TRUE, FALSE) == FAIL)
		{
			MSG_OUTSTR(files);		/* print error message */
			num_files = 0;
		}
		/*
		 * remove swapfile name of the current buffer, it must be ignored
		 */
		if (curbuf->b_ml.ml_mfp != NULL &&
								(p = curbuf->b_ml.ml_mfp->mf_fname) != NULL)
		{
			for (i = 0; i < num_files; ++i)
				if (fullpathcmp(p, files[i]) == FPC_SAME)
				{
					vim_free(files[i]);
					--num_files;
					for ( ; i < num_files; ++i)
						files[i] = files[i + 1];
					break;
				}
		}
		if (nr)
		{
			file_count += num_files;
			if (nr <= file_count)
			{
				*fname = strsave(files[nr - 1 + num_files - file_count]);
				dirp = (char_u *)"";				/* stop searching */
			}
		}
		else if (list)
		{
			if (dir_name[0] == '.' && dir_name[1] == NUL)
			{
				if (fname == NULL || *fname == NULL)
					MSG_OUTSTR("   In current directory:\n");
				else
					MSG_OUTSTR("   Using specified name:\n");
			}
			else
			{
				MSG_OUTSTR("   In directory ");
				msg_home_replace(dir_name);
				MSG_OUTSTR(":\n");
			}

			if (num_files)
			{
				for (i = 0; i < num_files; ++i)
				{
					/* print the swap file name */
					msg_outnum((long)++file_count);
					MSG_OUTSTR(".    ");
					msg_outstr(gettail(files[i]));
					msg_outchar('\n');
					swapfile_info(files[i]);
				}
			}
			else
				MSG_OUTSTR("      -- none --\n");
			flushbuf();
		}
		else
			file_count += num_files;

		for (i = 0; i < num_names; ++i)
			vim_free(names[i]);
		FreeWild(num_files, files);
	}
	vim_free(dir_name);
	expand_interactively = FALSE;
	return file_count;
}

/*
 * Give information about an existing swap file
 */
	static void
swapfile_info(fname)
	char_u		*fname;
{
	struct stat		st;
	int				fd;
	struct block0	b0;
	time_t			x;

	/* print the swap file date */
	if (stat((char *)fname, &st) != -1)
	{
		MSG_OUTSTR("             dated: ");
		x = st.st_mtime;					/* Manx C can't do &st.st_mtime */
		MSG_OUTSTR(ctime(&x));
	}

	/*
	 * print the original file name
	 */
	fd = open((char *)fname, O_RDONLY | O_EXTRA);
	if (fd >= 0)
	{
		if (read(fd, (char *)&b0, sizeof(b0)) == sizeof(b0))
		{
			if (b0.b0_id[0] != BLOCK0_ID0 ||
								b0.b0_id[1] != BLOCK0_ID1)
				MSG_OUTSTR("         [is not a swap file]");
			else
			{
				MSG_OUTSTR("         file name: ");
				msg_outstr(b0.b0_fname);

				if (*(b0.b0_hname) != NUL)
				{
					MSG_OUTSTR("\n         host name: ");
					msg_outstr(b0.b0_hname);
				}

				if (*(b0.b0_uname) != NUL)
				{
					MSG_OUTSTR("\n         user name: ");
					msg_outstr(b0.b0_uname);
				}

				if (char_to_long(b0.b0_pid) != 0L)
				{
					MSG_OUTSTR("\n        process ID: ");
					msg_outnum(char_to_long(b0.b0_pid));
#if defined(UNIX) || defined(__EMX__)
					/* EMX kill() not working correctly, it seems */
					if (kill(char_to_long(b0.b0_pid), 0) == 0)
						MSG_OUTSTR(" (still running)");
#endif
				}

				if (b0_magic_wrong(&b0))
				{
#if defined(MSDOS) || defined(WIN32)
					if (STRNCMP(b0.b0_hname, "PC ", 3) == 0)
						MSG_OUTSTR("\n         [not usable with this version of Vim]");
					else
#endif
						MSG_OUTSTR("\n         [not usable on this computer]");
				}
					
			}
		}
		else
			MSG_OUTSTR("         [cannot be read]");
		close(fd);
	}
	else
		MSG_OUTSTR("         [cannot be opened]");
	msg_outchar('\n');
}

	static int
get_names(names, fname)
	char_u **names;
	char_u	*fname;
{
	int		num_names;

#ifdef SHORT_FNAME
	/*
	 * (MS-DOS) always short names
	 */
	names[0] = modname(fname, (char_u *)".sw?");
	num_names = 1;
#else /* !SHORT_FNAME */
# ifdef WIN32
	/*
	 * (WIN32) never short names
	 */
	num_names = 1;
	names[0] = concat_fnames(fname, (char_u *)".sw?", FALSE);
# else /* !WIN32 */
	/*
	 * (Not MS-DOS or WIN32) maybe short name, maybe not: try both.
	 * Only use the short name if it is different.
	 */

	int		i;

	names[0] = concat_fnames(fname, (char_u *)".sw?", FALSE);
	i = curbuf->b_shortname;
	curbuf->b_shortname = TRUE;
	names[1] = modname(fname, (char_u *)".sw?");
	curbuf->b_shortname = i;
	if (STRCMP(names[0], names[1]) == 0)
	{
		vim_free(names[1]);
		num_names = 1;
	}
	else
		num_names = 2;
# endif /* !WIN32 */
#endif /* !SHORT_FNAME */
	return num_names;
}

/*
 * sync all memlines
 *
 * If 'check_file' is TRUE, check if original file exists and was not changed.
 * If 'check_char' is TRUE, stop syncing when character becomes available, but
 * always sync at least one block.
 */
	void
ml_sync_all(check_file, check_char)
	int		check_file;
	int		check_char;
{
	BUF				*buf;
	struct stat		st;

	for (buf = firstbuf; buf != NULL; buf = buf->b_next)
	{
		if (buf->b_ml.ml_mfp == NULL || buf->b_ml.ml_mfp->mf_fname == NULL)
			continue;						/* no file */

		ml_flush_line(buf);					/* flush buffered line */
											/* flush locked block */
		(void)ml_find_line(buf, (linenr_t)0, ML_FLUSH);
		if (buf->b_changed && check_file && mf_need_trans(buf->b_ml.ml_mfp) &&
									buf->b_filename != NULL)
		{
			/*
			 * if original file does not exist anymore or has been changed
			 * call ml_preserve to get rid of all negative numbered blocks
			 */
			if (stat((char *)buf->b_filename, &st) == -1 ||
								st.st_mtime != buf->b_mtime_read)
			{
				ml_preserve(buf, FALSE);
				need_check_timestamps = TRUE;	/* give message later */
			}
		}
		if (buf->b_ml.ml_mfp->mf_dirty)
		{
			mf_sync(buf->b_ml.ml_mfp, FALSE, check_char, buf->b_changed);
			if (check_char && mch_char_avail())	/* character available now */
				break;
		}
	}
}

/*
 * sync one buffer, including negative blocks
 *
 * after this all the blocks are in the swap file
 *
 * Used for the :preserve command and when the original file has been
 * changed or deleted.
 *
 * when message is TRUE the success of preserving is reported
 */
	void
ml_preserve(buf, message)
	BUF		*buf;
	int		message;
{
	BHDR		*hp;
	linenr_t	lnum;
	MEMFILE		*mfp = buf->b_ml.ml_mfp;
	int			status;

	if (mfp == NULL || mfp->mf_fname == NULL)
	{
		if (message)
			EMSG("Cannot preserve, there is no swap file");
		return;
	}

	ml_flush_line(buf);								/* flush buffered line */
	(void)ml_find_line(buf, (linenr_t)0, ML_FLUSH);	/* flush locked block */
	status = mf_sync(mfp, TRUE, FALSE, TRUE);
		
			/* stack is invalid after mf_sync(.., TRUE, ..) */
	buf->b_ml.ml_stack_top = 0;

	/*
	 * Some of the data blocks may have been changed from negative to
	 * positive block number. In that case the pointer blocks need to be
	 * updated.
	 *
	 * We don't know in which pointer block the references are, so we visit
	 * all data blocks until there are no more translations to be done (or
	 * we hit the end of the file, which can only happen in case a write fails,
	 * e.g. when file system if full).
	 * ml_find_line() does the work by translating the negative block numbers
	 * when getting the first line of each data block.
	 */
	if (mf_need_trans(mfp))
	{
		lnum = 1;
		while (mf_need_trans(mfp) && lnum <= buf->b_ml.ml_line_count)
		{
			hp = ml_find_line(buf, lnum, ML_FIND);
			if (hp == NULL)
			{
				status = FAIL;
				goto theend;
			}
			CHECK(buf->b_ml.ml_locked_low != lnum, "low != lnum");
			lnum = buf->b_ml.ml_locked_high + 1;
		}
		(void)ml_find_line(buf, (linenr_t)0, ML_FLUSH);	/* flush locked block */
		if (mf_sync(mfp, TRUE, FALSE, TRUE) == FAIL)	/* sync the updated pointer blocks */
			status = FAIL;
		buf->b_ml.ml_stack_top = 0;			/* stack is invalid now */
	}
theend:
	if (message)
	{
		if (status == OK)
			MSG("File preserved");
		else
			EMSG("Preserve failed");
	}
}

/*
 * get a pointer to a (read-only copy of a) line 
 * 
 * On failure an error message is given and IObuff is returned (to avoid
 * having to check for error everywhere).
 */
	char_u	*
ml_get(lnum)
	linenr_t	lnum;
{
	return ml_get_buf(curbuf, lnum, FALSE);
}

/*
 * ml_get_pos: get pointer to position 'pos'
 */
    char_u *
ml_get_pos(pos)
    FPOS	*pos;
{
	return (ml_get_buf(curbuf, pos->lnum, FALSE) + pos->col);
}

/*
 * ml_get_pos: get pointer to cursor line.
 */
	char_u *
ml_get_curline()
{
	return ml_get_buf(curbuf, curwin->w_cursor.lnum, FALSE);
}

/*
 * ml_get_pos: get pointer to cursor position
 */
	char_u *
ml_get_cursor()
{
	return (ml_get_buf(curbuf, curwin->w_cursor.lnum, FALSE) +
														curwin->w_cursor.col);
}

/*
 * get a pointer to a line in a specific buffer
 *
 *  will_change: if TRUE mark the buffer dirty (chars in the line will be
 *  changed)
 */
	char_u	*
ml_get_buf(buf, lnum, will_change)
	BUF			*buf;
	linenr_t	lnum;
	int			will_change;			/* line will be changed */
{
	BHDR	*hp;
	DATA_BL	*dp;
	char_u	*ptr;

	if (lnum > buf->b_ml.ml_line_count)	/* invalid line number */
	{
		EMSGN("ml_get: invalid lnum: %ld", lnum);
errorret:
		STRCPY(IObuff, "???");
		return IObuff;
	}
	if (lnum <= 0)						/* pretend line 0 is line 1 */
		lnum = 1;
	
	if (buf->b_ml.ml_mfp == NULL)		/* there are no lines */
		return (char_u *)"";

/*
 * See if it is the same line as requested last time.
 * Otherwise may need to flush last used line.
 */
	if (buf->b_ml.ml_line_lnum != lnum)
	{
		ml_flush_line(buf);

		/*
		 * find the data block containing the line
		 * This also fills the stack with the blocks from the root to the data block
		 * This also releases any locked block.
		 */
		if ((hp = ml_find_line(buf, lnum, ML_FIND)) == NULL)
		{
			EMSGN("ml_get: cannot find line %ld", lnum);
			goto errorret;
		}

		dp = (DATA_BL *)(hp->bh_data);

		ptr = (char_u *)dp + ((dp->db_index[lnum - buf->b_ml.ml_locked_low]) & DB_INDEX_MASK);
		buf->b_ml.ml_line_ptr = ptr;
		buf->b_ml.ml_line_lnum = lnum;
		buf->b_ml.ml_flags &= ~ML_LINE_DIRTY;
	}
	if (will_change)
		buf->b_ml.ml_flags |= (ML_LOCKED_DIRTY | ML_LOCKED_POS);

	return buf->b_ml.ml_line_ptr;
}

/*
 * Check if a line that was just obtained by a call to ml_get
 * is in allocated memory.
 */
	int
ml_line_alloced()
{
	return (curbuf->b_ml.ml_flags & ML_LINE_DIRTY);
}

/*
 * append a line after lnum (may be 0 to insert a line in front of the file)
 *
 *   newfile: TRUE when starting to edit a new file, meaning that pe_old_lnum
 *				will be set for recovery
 *
 * return FAIL for failure, OK otherwise
 */
	int
ml_append(lnum, line, len, newfile)
	linenr_t	lnum;			/* append after this line (can be 0) */
	char_u		*line;			/* text of the new line */
	colnr_t		len;			/* length of new line, including NUL, or 0 */
	int			newfile;		/* flag, see above */
{
 	if (curbuf->b_ml.ml_line_lnum != 0)
		ml_flush_line(curbuf);
	return ml_append_int(curbuf, lnum, line, len, newfile);
}

	static int
ml_append_int(buf, lnum, line, len, newfile)
	BUF			*buf;
	linenr_t	lnum;			/* append after this line (can be 0) */
	char_u		*line;			/* text of the new line */
	colnr_t		len;			/* length of line, including NUL, or 0 */
	int			newfile;		/* flag, see above */
{
	int			i;
	int			line_count;		/* number of indexes in current block */
	int			offset;
	int			from, to;
	int			space_needed;	/* space needed for new line */
	int			page_size;
	int			page_count;
	int			db_idx;			/* index for lnum in data block */
	BHDR		*hp;
	MEMFILE		*mfp;
	DATA_BL		*dp;
	PTR_BL		*pp;
	IPTR		*ip;

										/* lnum out of range */
	if (lnum > buf->b_ml.ml_line_count || buf->b_ml.ml_mfp == NULL)
		return FAIL;
	
	if (lowest_marked && lowest_marked > lnum)
		lowest_marked = lnum + 1;

	if (len == 0)
		len = STRLEN(line) + 1;			/* space needed for the text */
	space_needed = len + INDEX_SIZE;	/* space needed for text + index */

	mfp = buf->b_ml.ml_mfp;
	page_size = mfp->mf_page_size;

/*
 * find the data block containing the previous line
 * This also fills the stack with the blocks from the root to the data block
 * This also releases any locked block.
 */
	if ((hp = ml_find_line(buf, lnum == 0 ? (linenr_t)1 : lnum,
													ML_INSERT)) == NULL)
		return FAIL;

	buf->b_ml.ml_flags &= ~ML_EMPTY;

	if (lnum == 0)				/* got line one instead, correct db_idx */
		db_idx = -1;			/* careful, it is negative! */
	else
		db_idx = lnum - buf->b_ml.ml_locked_low;
				/* get line count before the insertion */
	line_count = buf->b_ml.ml_locked_high - buf->b_ml.ml_locked_low;

	dp = (DATA_BL *)(hp->bh_data);

/*
 * If
 * - there is not enough room in the current block
 * - appending to the last line in the block
 * - not appending to the last line in the file
 * insert in front of the next block.
 */
	if ((int)dp->db_free < space_needed && db_idx == line_count - 1 &&
											lnum < buf->b_ml.ml_line_count)
	{
		/*
		 * Now that the line is not going to be inserted in the block that we
		 * expected, the line count has to be adjusted in the pointer blocks
		 * by using ml_locked_lineadd.
		 */
		--(buf->b_ml.ml_locked_lineadd);
		--(buf->b_ml.ml_locked_high);
		if ((hp = ml_find_line(buf, lnum + 1, ML_INSERT)) == NULL)
			return FAIL;

		db_idx = -1;				/* careful, it is negative! */
					/* get line count before the insertion */
		line_count = buf->b_ml.ml_locked_high - buf->b_ml.ml_locked_low;
		CHECK(buf->b_ml.ml_locked_low != lnum + 1, "locked_low != lnum + 1");

		dp = (DATA_BL *)(hp->bh_data);
	}

	++buf->b_ml.ml_line_count;

	if ((int)dp->db_free >= space_needed)		/* enough room in data block */
	{
/*
 * Insert new line in existing data block, or in data block allocated above.
 */
		dp->db_txt_start -= len;
		dp->db_free -= space_needed;
		++(dp->db_line_count);

		/*
		 * move the text of the lines that follow to the front
		 * adjust the indexes of the lines that follow
		 */
		if (line_count > db_idx + 1)		/* if there are following lines */
		{
			/*
			 * Offset is the start of the previous line.
			 * This will become the character just after the new line.
			 */
			if (db_idx < 0)
				offset = dp->db_txt_end;
			else
				offset = ((dp->db_index[db_idx]) & DB_INDEX_MASK);
			vim_memmove((char *)dp + dp->db_txt_start,
										  (char *)dp + dp->db_txt_start + len,
								 (size_t)(offset - (dp->db_txt_start + len)));
			for (i = line_count - 1; i > db_idx; --i)
				dp->db_index[i + 1] = dp->db_index[i] - len;
			dp->db_index[db_idx + 1] = offset - len;
		}
		else								/* add line at the end */
			dp->db_index[db_idx + 1] = dp->db_txt_start;

		/*
		 * copy the text into the block
		 */
		vim_memmove((char *)dp + dp->db_index[db_idx + 1], line, (size_t)len);

		/*
		 * Mark the block dirty.
		 */
		buf->b_ml.ml_flags |= ML_LOCKED_DIRTY;
		if (!newfile)
			buf->b_ml.ml_flags |= ML_LOCKED_POS;
	}
	else			/* not enough space in data block */
	{
/*
 * If there is not enough room we have to create a new data block and copy some
 * lines into it.
 * Then we have to insert an entry in the pointer block.
 * If this pointer block also is full, we go up another block, and so on, up
 * to the root if necessary.
 * The line counts in the pointer blocks have already been adjusted by
 * ml_find_line().
 */
		long		line_count_left, line_count_right;
		int			page_count_left, page_count_right;
		BHDR		*hp_left;
		BHDR		*hp_right;
		BHDR		*hp_new;
		int			lines_moved;
		int			data_moved = 0;			/* init to shut up gcc */
		int			total_moved = 0;		/* init to shut up gcc */
		DATA_BL		*dp_right, *dp_left;
		int			stack_idx;
		int			in_left;
		int			lineadd;
		blocknr_t	bnum_left, bnum_right;
		linenr_t	lnum_left, lnum_right;
		int			pb_idx;
		PTR_BL		*pp_new;

		/*
		 * We are going to allocate a new data block. Depending on the
		 * situation it will be put to the left or right of the existing
		 * block.  If possible we put the new line in the left block and move
		 * the lines after it to the right block. Otherwise the new line is
		 * also put in the right block. This method is more efficient when
		 * inserting a lot of lines at one place.
		 */
		if (db_idx < 0)			/* left block is new, right block is existing */
		{
			lines_moved = 0;
			in_left = TRUE;
			/* space_needed does not change */
		}
		else					/* left block is existing, right block is new */
		{
			lines_moved = line_count - db_idx - 1;
			if (lines_moved == 0)
				in_left = FALSE;		/* put new line in right block */
										/* space_needed does not change */
			else
			{
				data_moved = ((dp->db_index[db_idx]) & DB_INDEX_MASK) -
															dp->db_txt_start;
				total_moved = data_moved + lines_moved * INDEX_SIZE;
				if ((int)dp->db_free + total_moved >= space_needed)
				{
					in_left = TRUE;		/* put new line in left block */
					space_needed = total_moved;
				}
				else
				{
					in_left = FALSE;		/* put new line in right block */
					space_needed += total_moved;
				}
			}
		}

		page_count = ((space_needed + HEADER_SIZE) + page_size - 1) / page_size;
		if ((hp_new = ml_new_data(mfp, newfile, page_count)) == NULL)
		{
						/* correct line counts in pointer blocks */
			--(buf->b_ml.ml_locked_lineadd);
			--(buf->b_ml.ml_locked_high);
			return FAIL;
		}
		if (db_idx < 0)			/* left block is new */
		{
			hp_left = hp_new;
			hp_right = hp;
			line_count_left = 0;
			line_count_right = line_count;
		}
		else					/* right block is new */
		{
			hp_left = hp;
			hp_right = hp_new;
			line_count_left = line_count;
			line_count_right = 0;
		}
		dp_right = (DATA_BL *)(hp_right->bh_data);
		dp_left = (DATA_BL *)(hp_left->bh_data);
		bnum_left = hp_left->bh_bnum;
		bnum_right = hp_right->bh_bnum;
		page_count_left = hp_left->bh_page_count;
		page_count_right = hp_right->bh_page_count;

		/*
		 * May move the new line into the right/new block.
		 */
		if (!in_left)
		{
			dp_right->db_txt_start -= len;
			dp_right->db_free -= len + INDEX_SIZE;
			dp_right->db_index[0] = dp_right->db_txt_start;
			vim_memmove((char *)dp_right + dp_right->db_txt_start,
														   line, (size_t)len);
			++line_count_right;
		}
		/*
		 * may move lines from the left/old block to the right/new one.
		 */
		if (lines_moved)
		{
			/*
			 */
			dp_right->db_txt_start -= data_moved;
			dp_right->db_free -= total_moved;
			vim_memmove((char *)dp_right + dp_right->db_txt_start,
				 		(char *)dp_left + dp_left->db_txt_start,
						(size_t)data_moved);
			offset = dp_right->db_txt_start - dp_left->db_txt_start;
			dp_left->db_txt_start += data_moved;
			dp_left->db_free += total_moved;

			/*
			 * update indexes in the new block
			 */
			for (to = line_count_right, from = db_idx + 1;
										 from < line_count_left; ++from, ++to)
				dp_right->db_index[to] = dp->db_index[from] + offset;
			line_count_right += lines_moved;
			line_count_left -= lines_moved;
		}

		/*
		 * May move the new line into the left (old or new) block.
		 */
		if (in_left)
		{
			dp_left->db_txt_start -= len;
			dp_left->db_free -= len + INDEX_SIZE;
			dp_left->db_index[line_count_left] = dp_left->db_txt_start;
			vim_memmove((char *)dp_left + dp_left->db_txt_start,
														   line, (size_t)len);
			++line_count_left;
		}

		if (db_idx < 0)			/* left block is new */
		{
			lnum_left = lnum + 1;
			lnum_right = 0;
		}
		else					/* right block is new */
		{
			lnum_left = 0;
			if (in_left)
				lnum_right = lnum + 2;
			else
				lnum_right = lnum + 1;
		}
		dp_left->db_line_count = line_count_left;
		dp_right->db_line_count = line_count_right;

		/*
		 * release the two data blocks
		 * The new one (hp_new) already has a correct blocknumber.
		 * The old one (hp, in ml_locked) gets a positive blocknumber if
		 * we changed it and we are not editing a new file.
		 */
		if (lines_moved || in_left)
			buf->b_ml.ml_flags |= ML_LOCKED_DIRTY;
		if (!newfile && db_idx >= 0 && in_left)
			buf->b_ml.ml_flags |= ML_LOCKED_POS;
		mf_put(mfp, hp_new, TRUE, FALSE);

		/*
		 * flush the old data block
		 * set ml_locked_lineadd to 0, because the updating of the
		 * pointer blocks is done below
		 */
		lineadd = buf->b_ml.ml_locked_lineadd;
		buf->b_ml.ml_locked_lineadd = 0;
		ml_find_line(buf, (linenr_t)0, ML_FLUSH);	/* flush data block */

		/*
		 * update pointer blocks for the new data block
		 */
		for (stack_idx = buf->b_ml.ml_stack_top - 1; stack_idx >= 0; --stack_idx)
		{
			ip = &(buf->b_ml.ml_stack[stack_idx]);
			pb_idx = ip->ip_index;
			if ((hp = mf_get(mfp, ip->ip_bnum, 1)) == NULL)
				return FAIL;
			pp = (PTR_BL *)(hp->bh_data);	/* must be pointer block */
			if (pp->pb_id != PTR_ID)
			{
				EMSG("pointer block id wrong 3");
				mf_put(mfp, hp, FALSE, FALSE);
				return FAIL;
			}
			/*
			 * TODO: If the pointer block is full and we are adding at the end
			 * try to insert in front of the next block
			 */
			if (pp->pb_count < pp->pb_count_max)	/* block not full, add one entry */
			{
				if (pb_idx + 1 < (int)pp->pb_count)
					vim_memmove(&pp->pb_pointer[pb_idx + 2],
								&pp->pb_pointer[pb_idx + 1],
						(size_t)(pp->pb_count - pb_idx - 1) * sizeof(PTR_EN));
				++pp->pb_count;
				pp->pb_pointer[pb_idx].pe_line_count = line_count_left;
				pp->pb_pointer[pb_idx].pe_bnum = bnum_left;
				pp->pb_pointer[pb_idx].pe_page_count = page_count_left;
				pp->pb_pointer[pb_idx + 1].pe_line_count = line_count_right;
				pp->pb_pointer[pb_idx + 1].pe_bnum = bnum_right;
				pp->pb_pointer[pb_idx + 1].pe_page_count = page_count_right;

				if (lnum_left != 0)
					pp->pb_pointer[pb_idx].pe_old_lnum = lnum_left;
				if (lnum_right != 0)
					pp->pb_pointer[pb_idx + 1].pe_old_lnum = lnum_right;

				mf_put(mfp, hp, TRUE, FALSE);
				buf->b_ml.ml_stack_top = stack_idx + 1;		/* truncate stack */

				if (lineadd)
				{
					--(buf->b_ml.ml_stack_top);
						/* fix line count for rest of blocks in the stack */
					ml_lineadd(buf, lineadd);
														/* fix stack itself */
					buf->b_ml.ml_stack[buf->b_ml.ml_stack_top].ip_high +=
																	  lineadd;
					++(buf->b_ml.ml_stack_top);
				}

				return OK;
			}
			else						/* pointer block full */
			{
				/*
				 * split the pointer block
				 * allocate a new pointer block
				 * move some of the pointer into the new block
				 * prepare for updating the parent block
				 */
				for (;;)		/* do this twice when splitting block 1 */
				{
					hp_new = ml_new_ptr(mfp);
					if (hp_new == NULL)		/* TODO: try to fix tree */
						return FAIL;
					pp_new = (PTR_BL *)(hp_new->bh_data);

					if (hp->bh_bnum != 1)
						break;

					/*
					 * if block 1 becomes full the tree is given an extra level
					 * The pointers from block 1 are moved into the new block.
					 * block 1 is updated to point to the new block
					 * then continue to split the new block
					 */
					vim_memmove(pp_new, pp, (size_t)page_size);
					pp->pb_count = 1;
					pp->pb_pointer[0].pe_bnum = hp_new->bh_bnum;
					pp->pb_pointer[0].pe_line_count = buf->b_ml.ml_line_count;
					pp->pb_pointer[0].pe_old_lnum = 1;
					pp->pb_pointer[0].pe_page_count = 1;
					mf_put(mfp, hp, TRUE, FALSE);	/* release block 1 */
					hp = hp_new;					/* new block is to be split */
					pp = pp_new;
					CHECK(stack_idx != 0, "stack_idx should be 0");
					ip->ip_index = 0;
					++stack_idx;		/* do block 1 again later */
				}
				/*
				 * move the pointers after the current one to the new block
				 * If there are none, the new entry will be in the new block.
				 */
				total_moved = pp->pb_count - pb_idx - 1;
				if (total_moved)
				{
					vim_memmove(&pp_new->pb_pointer[0],
								&pp->pb_pointer[pb_idx + 1],
								(size_t)(total_moved) * sizeof(PTR_EN));
					pp_new->pb_count = total_moved;
					pp->pb_count -= total_moved - 1;
					pp->pb_pointer[pb_idx + 1].pe_bnum = bnum_right;
					pp->pb_pointer[pb_idx + 1].pe_line_count = line_count_right;
					pp->pb_pointer[pb_idx + 1].pe_page_count = page_count_right;
					if (lnum_right)
						pp->pb_pointer[pb_idx + 1].pe_old_lnum = lnum_right;
				}
				else
				{
					pp_new->pb_count = 1;
					pp_new->pb_pointer[0].pe_bnum = bnum_right;
					pp_new->pb_pointer[0].pe_line_count = line_count_right;
					pp_new->pb_pointer[0].pe_page_count = page_count_right;
					pp_new->pb_pointer[0].pe_old_lnum = lnum_right;
				}
				pp->pb_pointer[pb_idx].pe_bnum = bnum_left;
				pp->pb_pointer[pb_idx].pe_line_count = line_count_left;
				pp->pb_pointer[pb_idx].pe_page_count = page_count_left;
				if (lnum_left)
					pp->pb_pointer[pb_idx].pe_old_lnum = lnum_left;
				lnum_left = 0;
				lnum_right = 0;

				/*
				 * recompute line counts
				 */
				line_count_right = 0;
				for (i = 0; i < (int)pp_new->pb_count; ++i)
					line_count_right += pp_new->pb_pointer[i].pe_line_count;
				line_count_left = 0;
				for (i = 0; i < (int)pp->pb_count; ++i)
					line_count_left += pp->pb_pointer[i].pe_line_count;

				bnum_left = hp->bh_bnum;
				bnum_right = hp_new->bh_bnum;
				page_count_left = 1;
				page_count_right = 1;
				mf_put(mfp, hp, TRUE, FALSE);
				mf_put(mfp, hp_new, TRUE, FALSE);
			}
		}
		EMSG("Updated too many blocks?");
		buf->b_ml.ml_stack_top = 0;		/* invalidate stack */
	}
	return OK;
}

/*
 * replace line lnum, with buffering, in current buffer
 *
 * If copy is TRUE, make a copy of the line, otherwise the line has been
 * copied to allocated memory already.
 *
 * return FAIL for failure, OK otherwise
 */
	int
ml_replace(lnum, line, copy)
	linenr_t	lnum;
	char_u		*line;
	int			copy;
{
	if (line == NULL)			/* just checking... */
		return FAIL;

	if (curbuf->b_ml.ml_line_lnum != lnum)			/* other line buffered */
		ml_flush_line(curbuf);						/* flush it */
	else if (curbuf->b_ml.ml_flags & ML_LINE_DIRTY)	/* same line allocated */
		vim_free(curbuf->b_ml.ml_line_ptr);			/* free it */
	if (copy && (line = strsave(line)) == NULL)		/* allocate memory */
		return FAIL;
	curbuf->b_ml.ml_line_ptr = line;
	curbuf->b_ml.ml_line_lnum = lnum;
	curbuf->b_ml.ml_flags = (curbuf->b_ml.ml_flags | ML_LINE_DIRTY) & ~ML_EMPTY;

	return OK;
}

/*
 * delete line 'lnum'
 *
 * return FAIL for failure, OK otherwise
 */
	int
ml_delete(lnum, message)
	linenr_t	lnum;
	int			message;
{
	ml_flush_line(curbuf);
	return ml_delete_int(curbuf, lnum, message);
}

	static int
ml_delete_int(buf, lnum, message)
	BUF			*buf;
	linenr_t	lnum;
	int			message;
{
	BHDR	*hp;
	MEMFILE	*mfp;
	DATA_BL	*dp;
	PTR_BL	*pp;
	IPTR	*ip;
	int		count;			/* number of entries in block */
	int		idx;
	int		stack_idx;
	int		text_start;
	int		line_start;
	int		line_size;
	int		i;

	if (lnum < 1 || lnum > buf->b_ml.ml_line_count)
		return FAIL;

	if (lowest_marked && lowest_marked > lnum)
		lowest_marked--;

/*
 * If the file becomes empty the last line is replaced by an empty line.
 */
	if (buf->b_ml.ml_line_count == 1)		/* file becomes empty */
	{
		if (message)
			keep_msg = no_lines_msg;
		i = ml_replace((linenr_t)1, (char_u *)"", TRUE);
		buf->b_ml.ml_flags |= ML_EMPTY;
		return i;
	}

/*
 * find the data block containing the line
 * This also fills the stack with the blocks from the root to the data block
 * This also releases any locked block.
 */
	mfp = buf->b_ml.ml_mfp;
	if (mfp == NULL)
		return FAIL;

	if ((hp = ml_find_line(buf, lnum, ML_DELETE)) == NULL)
		return FAIL;

	dp = (DATA_BL *)(hp->bh_data);
			/* compute line count before the delete */
	count = (long)(buf->b_ml.ml_locked_high) - (long)(buf->b_ml.ml_locked_low) + 2;
	idx = lnum - buf->b_ml.ml_locked_low;

	--buf->b_ml.ml_line_count;

/*
 * special case: If there is only one line in the data block it becomes empty.
 * Then we have to remove the entry, pointing to this data block, from the
 * pointer block. If this pointer block also becomes empty, we go up another
 * block, and so on, up to the root if necessary.
 * The line counts in the pointer blocks have already been adjusted by
 * ml_find_line().
 */
	if (count == 1)
	{
		mf_free(mfp, hp);		/* free the data block */
		buf->b_ml.ml_locked = NULL;

		for (stack_idx = buf->b_ml.ml_stack_top - 1; stack_idx >= 0; --stack_idx)
		{
			buf->b_ml.ml_stack_top = 0;		/* stack is invalid when failing */
			ip = &(buf->b_ml.ml_stack[stack_idx]);
			idx = ip->ip_index;
			if ((hp = mf_get(mfp, ip->ip_bnum, 1)) == NULL)
				return FAIL;
			pp = (PTR_BL *)(hp->bh_data);	/* must be pointer block */
			if (pp->pb_id != PTR_ID)
			{
				EMSG("pointer block id wrong 4");
				mf_put(mfp, hp, FALSE, FALSE);
				return FAIL;
			}
			count = --(pp->pb_count);
			if (count == 0)			/* the pointer block becomes empty! */
				mf_free(mfp, hp);
			else
			{
				if (count != idx)		/* move entries after the deleted one */
					vim_memmove(&pp->pb_pointer[idx], &pp->pb_pointer[idx + 1],
									  (size_t)(count - idx) * sizeof(PTR_EN));
				mf_put(mfp, hp, TRUE, FALSE);

				buf->b_ml.ml_stack_top = stack_idx;		/* truncate stack */
					/* fix line count for rest of blocks in the stack */
				if (buf->b_ml.ml_locked_lineadd)
				{
					ml_lineadd(buf, buf->b_ml.ml_locked_lineadd);
					buf->b_ml.ml_stack[buf->b_ml.ml_stack_top].ip_high +=
												buf->b_ml.ml_locked_lineadd;
				}
				++(buf->b_ml.ml_stack_top);

				return OK;
			}
		}
		CHECK(1, "deleted block 1?");

		return OK;
	}

	/*
	 * delete the text by moving the next lines forwards
	 */
	text_start = dp->db_txt_start;
	line_start = ((dp->db_index[idx]) & DB_INDEX_MASK);
	if (idx == 0)				/* first line in block, text at the end */
		line_size = dp->db_txt_end - line_start;
	else
		line_size = ((dp->db_index[idx - 1]) & DB_INDEX_MASK) - line_start;
	vim_memmove((char *)dp + text_start + line_size, (char *)dp + text_start,
										   (size_t)(line_start - text_start));

	/*
	 * delete the index by moving the next indexes backwards
	 * Adjust the indexes for the text movement.
	 */
	for (i = idx; i < count - 1; ++i)
		dp->db_index[i] = dp->db_index[i + 1] + line_size;

	dp->db_free += line_size + INDEX_SIZE;
	dp->db_txt_start += line_size;
	--(dp->db_line_count);

	/*
	 * mark the block dirty and make sure it is in the file (for recovery)
	 */
	buf->b_ml.ml_flags |= (ML_LOCKED_DIRTY | ML_LOCKED_POS);

	return OK;
}

/*
 * set the B_MARKED flag for line 'lnum'
 */
	void
ml_setmarked(lnum)
	linenr_t lnum;
{
	BHDR	*hp;
	DATA_BL	*dp;
									/* invalid line number */
	if (lnum < 1 || lnum > curbuf->b_ml.ml_line_count ||
												curbuf->b_ml.ml_mfp == NULL)
		return;						/* give error message? */

	if (lowest_marked == 0 || lowest_marked > lnum)
		lowest_marked = lnum;

	/*
	 * find the data block containing the line
	 * This also fills the stack with the blocks from the root to the data block
	 * This also releases any locked block.
	 */
	if ((hp = ml_find_line(curbuf, lnum, ML_FIND)) == NULL)
		return;				/* give error message? */

	dp = (DATA_BL *)(hp->bh_data);
	dp->db_index[lnum - curbuf->b_ml.ml_locked_low] |= DB_MARKED;
	curbuf->b_ml.ml_flags |= ML_LOCKED_DIRTY;
}

/*
 * find the first line with its B_MARKED flag set
 */
	linenr_t
ml_firstmarked()
{
	BHDR		*hp;
	DATA_BL		*dp;
	linenr_t	lnum;
	int			i;

	if (curbuf->b_ml.ml_mfp == NULL)
		return (linenr_t) 0;

	/*
	 * The search starts with lowest_marked line. This is the last line where
	 * a mark was found, adjusted by inserting/deleting lines.
	 */
	for (lnum = lowest_marked; lnum <= curbuf->b_ml.ml_line_count; )
	{
		/*
		 * Find the data block containing the line.
		 * This also fills the stack with the blocks from the root to the data
		 * block This also releases any locked block.
		 */
		if ((hp = ml_find_line(curbuf, lnum, ML_FIND)) == NULL)
			return (linenr_t)0;				/* give error message? */

		dp = (DATA_BL *)(hp->bh_data);

		for (i = lnum - curbuf->b_ml.ml_locked_low;
							lnum <= curbuf->b_ml.ml_locked_high; ++i, ++lnum)
			if ((dp->db_index[i]) & DB_MARKED)
			{
				(dp->db_index[i]) &= DB_INDEX_MASK;
				curbuf->b_ml.ml_flags |= ML_LOCKED_DIRTY;
				lowest_marked = lnum + 1;
				return lnum;
			}
	}

	return (linenr_t) 0;
}

/*
 * return TRUE if line 'lnum' has a mark
 */
	int
ml_has_mark(lnum)
	linenr_t	lnum;
{
	BHDR		*hp;
	DATA_BL		*dp;

	if (curbuf->b_ml.ml_mfp == NULL ||
						(hp = ml_find_line(curbuf, lnum, ML_FIND)) == NULL)
		return FALSE;

	dp = (DATA_BL *)(hp->bh_data);
	return (int)((dp->db_index[lnum - curbuf->b_ml.ml_locked_low]) & DB_MARKED);
}

/*
 * clear all DB_MARKED flags
 */
	void
ml_clearmarked()
{
	BHDR		*hp;
	DATA_BL		*dp;
	linenr_t	lnum;
	int			i;

	if (curbuf->b_ml.ml_mfp == NULL)		/* nothing to do */
		return;

	/*
	 * The search starts with line lowest_marked.
	 */
	for (lnum = lowest_marked; lnum <= curbuf->b_ml.ml_line_count; )
	{
		/*
		 * Find the data block containing the line.
		 * This also fills the stack with the blocks from the root to the data block
		 * This also releases any locked block.
		 */
		if ((hp = ml_find_line(curbuf, lnum, ML_FIND)) == NULL)
			return;				/* give error message? */

		dp = (DATA_BL *)(hp->bh_data);

		for (i = lnum - curbuf->b_ml.ml_locked_low;
							lnum <= curbuf->b_ml.ml_locked_high; ++i, ++lnum)
			if ((dp->db_index[i]) & DB_MARKED)
			{
				(dp->db_index[i]) &= DB_INDEX_MASK;
				curbuf->b_ml.ml_flags |= ML_LOCKED_DIRTY;
			}
	}

	lowest_marked = 0;
	return;
}

/*
 * flush ml_line if necessary
 */
	static void
ml_flush_line(buf)
	BUF		*buf;
{
	BHDR		*hp;
	DATA_BL		*dp;
	linenr_t	lnum;
	char_u		*new_line;
	char_u		*old_line;
	colnr_t		new_len;
	int			old_len;
	int			extra;
	int			idx;
	int			start;
	int			count;
	int			i;

 	if (buf->b_ml.ml_line_lnum == 0 ||
						buf->b_ml.ml_mfp == NULL)		/* nothing to do */
		return;

	if (buf->b_ml.ml_flags & ML_LINE_DIRTY)
	{
		lnum = buf->b_ml.ml_line_lnum;
		new_line = buf->b_ml.ml_line_ptr;

		hp = ml_find_line(buf, lnum, ML_FIND);
		if (hp == NULL)
			EMSGN("Cannot find line %ld", lnum);
		else
		{
			dp = (DATA_BL *)(hp->bh_data);
			idx = lnum - buf->b_ml.ml_locked_low;
			start = ((dp->db_index[idx]) & DB_INDEX_MASK);
			old_line = (char_u *)dp + start;
			if (idx == 0)		/* line is last in block */
				old_len = dp->db_txt_end - start;
			else				/* text of previous line follows */
				old_len = (dp->db_index[idx - 1] & DB_INDEX_MASK) - start;
			new_len = STRLEN(new_line) + 1;
			extra = new_len - old_len;		/* negative if lines gets smaller */

			/*
			 * if new line fits in data block, replace directly
			 */
			if ((int)dp->db_free >= extra)
			{
					/* if the length changes and there are following lines */
				count = buf->b_ml.ml_locked_high - buf->b_ml.ml_locked_low + 1;
				if (extra != 0 && idx < count - 1)
				{
						/* move text of following lines */
					vim_memmove((char *)dp + dp->db_txt_start - extra,
								(char *)dp + dp->db_txt_start,
								(size_t)(start - dp->db_txt_start));

						/* adjust pointers of this and following lines */
					for (i = idx + 1; i < count; ++i)
						dp->db_index[i] -= extra;
				}
				dp->db_index[idx] -= extra;

					/* adjust free space */
				dp->db_free -= extra;
				dp->db_txt_start -= extra;

					/* copy new line into the data block */
				vim_memmove(old_line - extra, new_line, (size_t)new_len);
				buf->b_ml.ml_flags |= (ML_LOCKED_DIRTY | ML_LOCKED_POS);
			}
			else
			{
				/*
				 * Cannot do it in one data block: delete and append.
				 */
					/* How about handling errors??? */
				(void)ml_delete_int(buf, lnum, FALSE);
				(void)ml_append_int(buf, lnum - 1, new_line, new_len, FALSE);
			}
		}
		vim_free(new_line);
	}

	buf->b_ml.ml_line_lnum = 0;
}

/*
 * create a new, empty, data block
 */
	static BHDR *
ml_new_data(mfp, negative, page_count)
	MEMFILE		*mfp;
	int			negative;
	int			page_count;
{
	BHDR		*hp;
	DATA_BL		*dp;

	if ((hp = mf_new(mfp, negative, page_count)) == NULL)
		return NULL;
	
	dp = (DATA_BL *)(hp->bh_data);
	dp->db_id = DATA_ID;
	dp->db_txt_start = dp->db_txt_end = page_count * mfp->mf_page_size;
	dp->db_free = dp->db_txt_start - HEADER_SIZE;
	dp->db_line_count = 0;

	return hp;
}

/*
 * create a new, empty, pointer block
 */
	static BHDR *
ml_new_ptr(mfp)
	MEMFILE		*mfp;
{
	BHDR		*hp;
	PTR_BL		*pp;

	if ((hp = mf_new(mfp, FALSE, 1)) == NULL)
		return NULL;
	
	pp = (PTR_BL *)(hp->bh_data);
	pp->pb_id = PTR_ID;
	pp->pb_count = 0;
	pp->pb_count_max = (mfp->mf_page_size - sizeof(PTR_BL)) / sizeof(PTR_EN) + 1;

	return hp;
}

/*
 * lookup line 'lnum' in a memline
 *
 *   action: if ML_DELETE or ML_INSERT the line count is updated while searching
 *			 if ML_FLUSH only flush a locked block
 *			 if ML_FIND just find the line
 *
 * If the block was found it is locked and put in ml_locked.
 * The stack is updated to lead to the locked block. The ip_high field in
 * the stack is updated to reflect the last line in the block AFTER the
 * insert or delete, also if the pointer block has not been updated yet. But
 * if if ml_locked != NULL ml_locked_lineadd must be added to ip_high.
 *
 * return: NULL for failure, pointer to block header otherwise
 */
	static BHDR *
ml_find_line(buf, lnum, action)
	BUF			*buf;
	linenr_t	lnum;
	int			action;
{
	DATA_BL		*dp;
	PTR_BL		*pp;
	IPTR		*ip;
	BHDR		*hp;
	MEMFILE		*mfp;
	linenr_t	t;
	blocknr_t	bnum, bnum2;
	int			dirty;
	linenr_t	low, high;
	int			top;
	int			page_count;
	int			idx;

	mfp = buf->b_ml.ml_mfp;

	/*
	 * If there is a locked block check if the wanted line is in it.
	 * If not, flush and release the locked block.
	 * Don't do this for ML_INSERT_SAME, because the stack need to be updated.
	 * Don't do this for ML_FLUSH, because we want to flush the locked block.
	 */
	if (buf->b_ml.ml_locked)
	{
		if (ML_SIMPLE(action) && buf->b_ml.ml_locked_low <= lnum &&
									buf->b_ml.ml_locked_high >= lnum)
		{
				/* remember to update pointer blocks and stack later */
			if (action == ML_INSERT)
			{
				++(buf->b_ml.ml_locked_lineadd);
				++(buf->b_ml.ml_locked_high);
			}
			else if (action == ML_DELETE)
			{
				--(buf->b_ml.ml_locked_lineadd);
				--(buf->b_ml.ml_locked_high);
			}
			return (buf->b_ml.ml_locked);
		}

		mf_put(mfp, buf->b_ml.ml_locked, buf->b_ml.ml_flags & ML_LOCKED_DIRTY,
											buf->b_ml.ml_flags & ML_LOCKED_POS);
		buf->b_ml.ml_locked = NULL;

			/*
			 * if lines have been added or deleted in the locked block, need to
			 * update the line count in pointer blocks
			 */
		if (buf->b_ml.ml_locked_lineadd)
			ml_lineadd(buf, buf->b_ml.ml_locked_lineadd);
	}

	if (action == ML_FLUSH)			/* nothing else to do */
		return NULL;

	bnum = 1;						/* start at the root of the tree */
	page_count = 1;
	low = 1;
	high = buf->b_ml.ml_line_count;

	if (action == ML_FIND)		/* first try stack entries */
	{
		for (top = buf->b_ml.ml_stack_top - 1; top >= 0; --top)
		{
			ip = &(buf->b_ml.ml_stack[top]);
			if (ip->ip_low <= lnum && ip->ip_high >= lnum)
			{
				bnum = ip->ip_bnum;
				low = ip->ip_low;
				high = ip->ip_high;
				buf->b_ml.ml_stack_top = top;	/* truncate stack at prev entry */
				break;
			}
		}
		if (top < 0)
			buf->b_ml.ml_stack_top = 0;			/* not found, start at the root */
	}
	else		/* ML_DELETE or ML_INSERT */
		buf->b_ml.ml_stack_top = 0;		/* start at the root */

/*
 * search downwards in the tree until a data block is found
 */
	for (;;)
	{
		if ((hp = mf_get(mfp, bnum, page_count)) == NULL)
			goto error_noblock;

		/*
		 * update high for insert/delete
		 */
		if (action == ML_INSERT)
			++high;
		else if (action == ML_DELETE)
			--high;

		dp = (DATA_BL *)(hp->bh_data);
		if (dp->db_id == DATA_ID)		/* data block */
		{
			buf->b_ml.ml_locked = hp;
			buf->b_ml.ml_locked_low = low;
			buf->b_ml.ml_locked_high = high;
			buf->b_ml.ml_locked_lineadd = 0;
			buf->b_ml.ml_flags &= ~(ML_LOCKED_DIRTY | ML_LOCKED_POS);
			return hp;
		}

		pp = (PTR_BL *)(dp);			/* must be pointer block */
		if (pp->pb_id != PTR_ID)
		{
			EMSG("pointer block id wrong");
			goto error_block;
		}

		if ((top = ml_add_stack(buf)) < 0)		/* add new entry to stack */
			goto error_block;
		ip = &(buf->b_ml.ml_stack[top]);
		ip->ip_bnum = bnum;
		ip->ip_low = low;
		ip->ip_high = high;
		ip->ip_index = -1;				/* index not known yet */

		dirty = FALSE;
		for (idx = 0; idx < (int)pp->pb_count; ++idx)
		{
			t = pp->pb_pointer[idx].pe_line_count;
			CHECK(t == 0, "pe_line_count is zero");
			if ((low += t) > lnum)
			{
				ip->ip_index = idx;
				bnum = pp->pb_pointer[idx].pe_bnum;
				page_count = pp->pb_pointer[idx].pe_page_count;
				high = low - 1;
				low -= t;

				/*
				 * a negative block number may have been changed
				 */
				if (bnum < 0)
				{
					bnum2 = mf_trans_del(mfp, bnum);
					if (bnum != bnum2)
					{
						bnum = bnum2;
						pp->pb_pointer[idx].pe_bnum = bnum;
						dirty = TRUE;
					}
				}

				break;
			}
		}
		if (idx >= (int)pp->pb_count)		/* past the end: something wrong! */
		{
			if (lnum > buf->b_ml.ml_line_count)
				EMSGN("line number out of range: %ld past the end",
											  lnum - buf->b_ml.ml_line_count);

			else
				EMSGN("line count wrong in block %ld", bnum);
			goto error_block;
		}
		if (action == ML_DELETE)
		{
			pp->pb_pointer[idx].pe_line_count--;
			dirty = TRUE;
		}
		else if (action == ML_INSERT)
		{
			pp->pb_pointer[idx].pe_line_count++;
			dirty = TRUE;
		}
		mf_put(mfp, hp, dirty, FALSE);
	}

error_block:
	mf_put(mfp, hp, FALSE, FALSE);
error_noblock:
/*
 * If action is ML_DELETE or ML_INSERT we have to correct the tree for
 * the incremented/decremented line counts, because there won't be a line
 * inserted/deleted after all.
 */
	if (action == ML_DELETE)
		ml_lineadd(buf, 1);
	else if (action == ML_INSERT)
		ml_lineadd(buf, -1);
	buf->b_ml.ml_stack_top = 0;
	return NULL;
}

/*
 * add an entry to the info pointer stack
 *
 * return -1 for failure, number of the new entry otherwise
 */
	static int
ml_add_stack(buf)
	BUF		*buf;
{
	int		top;
	IPTR	*newstack;

	top = buf->b_ml.ml_stack_top;

		/* may have to increase the stack size */
	if (top == buf->b_ml.ml_stack_size)
	{
		CHECK(top > 0, "Stack size increases");	/* more than 5 levels??? */

		newstack = (IPTR *)alloc((unsigned)sizeof(IPTR) *
										(buf->b_ml.ml_stack_size + STACK_INCR));
		if (newstack == NULL)
			return -1;
		vim_memmove(newstack, buf->b_ml.ml_stack, (size_t)top * sizeof(IPTR));
		vim_free(buf->b_ml.ml_stack);
		buf->b_ml.ml_stack = newstack;
		buf->b_ml.ml_stack_size += STACK_INCR;
	}

	buf->b_ml.ml_stack_top++;
	return top;
}

/*
 * Update the pointer blocks on the stack for inserted/deleted lines.
 * The stack itself is also updated.
 *
 * When a insert/delete line action fails, the line is not inserted/deleted,
 * but the pointer blocks have already been updated. That is fixed here by
 * walking through the stack.
 *
 * Count is the number of lines added, negative if lines have been deleted.
 */
	static void
ml_lineadd(buf, count)
	BUF			*buf;
	int			count;
{
	int			idx;
	IPTR		*ip;
	PTR_BL		*pp;
	MEMFILE		*mfp = buf->b_ml.ml_mfp;
	BHDR		*hp;

	for (idx = buf->b_ml.ml_stack_top - 1; idx >= 0; --idx)
	{
		ip = &(buf->b_ml.ml_stack[idx]);
		if ((hp = mf_get(mfp, ip->ip_bnum, 1)) == NULL)
			break;
		pp = (PTR_BL *)(hp->bh_data);	/* must be pointer block */
		if (pp->pb_id != PTR_ID)
		{
			mf_put(mfp, hp, FALSE, FALSE);
			EMSG("pointer block id wrong 2");
			break;
		}
		pp->pb_pointer[ip->ip_index].pe_line_count += count;
		ip->ip_high += count;
		mf_put(mfp, hp, TRUE, FALSE);
	}
}

/*
 * make swap file name out of the filename and a directory name
 */
	static char_u *
makeswapname(buf, dir_name)
	BUF		*buf;
	char_u	*dir_name;
{
	char_u		*r, *s;

#ifdef VMS
	r = modname(buf->b_xfilename, (char_u *)"_swp");
#else
	r = modname(buf->b_xfilename, (char_u *)".swp");
#endif
	if (r == NULL)			/* out of memory */
		return NULL;

	s = get_file_in_dir(r, dir_name);
	vim_free(r);
	return s;
}

/*
 * Get file name to use for swap file or backup file.
 * Use the name of the edited file "fname" and an entry in the 'dir' or 'bdir'
 * option "dirname".
 * - If "dirname" is ".", return "fname".
 * - If "dirname" starts with "./", insert "dirname" in "fname".
 * - Otherwise, prepend "dirname" to the tail of "fname".
 *
 * The return value is an allocated string and can be NULL.
 */
	char_u *
get_file_in_dir(fname, dirname)
	char_u	*fname;
	char_u	*dirname;
{
	char_u		*t;
	char_u		*tail;
	char_u		*retval;
	int			save_char;

	tail = gettail(fname);

	if (dirname[0] == '.' && dirname[1] == NUL)
		retval = strsave(fname);
	else if (dirname[0] == '.' && ispathsep(dirname[1]))
	{
		if (tail == fname)			/* no path before file name */
			retval = concat_fnames(dirname + 2, tail, TRUE);
		else
		{
			save_char = *tail;
			*tail = NUL;
			t = concat_fnames(fname, dirname + 2, TRUE);
			*tail = save_char;
			if (t == NULL)			/* out of memory */
				retval = NULL;
			else
			{
				retval = concat_fnames(t, tail, TRUE);
				vim_free(t);
			}
		}
	}
	else
		retval = concat_fnames(dirname, tail, TRUE);

	return retval;
}

/*
 * Find out what name to use for the swap file for buffer 'buf'.
 *
 * Several names are tried to find one that does not exist
 *
 * Note: if BASENAMELEN is not correct, you will get error messages for
 *       not being able to open the swapfile
 */
	static char_u *
findswapname(buf, dirp, old_fname)
	BUF		*buf;
	char_u	**dirp;			/* pointer to list of directories */
	char_u	*old_fname;		/* don't give warning for this filename */
{
	char_u		*fname;
	int			n;
	time_t		x;
	char_u		*dir_name;

#ifdef AMIGA
	BPTR		fh;
#endif

#ifndef SHORT_FNAME
	int			r;
	FILE		*dummyfd = NULL;

/*
 * If we start editing a new file, e.g. "test.doc", which resides on an MSDOS
 * compatible filesystem, it is possible that the file "test.doc.swp" which we
 * create will be exactly the same file. To avoid this problem we temporarily
 * create "test.doc".
 */
	if (!(buf->b_p_sn || buf->b_shortname) && buf->b_xfilename &&
												getperm(buf->b_xfilename) < 0)
		dummyfd = fopen((char *)buf->b_xfilename, "w");
#endif

/*
 * Isolate a directory name from *dirp and put it in dir_name.
 * First allocate some memory to put the directory name in.
 */
	dir_name = alloc((unsigned)STRLEN(*dirp) + 1);
	if (dir_name != NULL)
		(void)copy_option_part(dirp, dir_name, 31000, ",");

/*
 * we try different names until we find one that does not exist yet
 */
	if (dir_name == NULL)			/* out of memory */
		fname = NULL;
	else
		fname = makeswapname(buf, dir_name);

	for (;;)
	{
		if (fname == NULL)		/* must be out of memory */
			break;
		if ((n = STRLEN(fname)) == 0)	/* safety check */
		{
			vim_free(fname);
			fname = NULL;
			break;
		}
#if (defined(UNIX) || defined(OS2)) && !defined(ARCHIE) && !defined(SHORT_FNAME)
/*
 * Some systems have a MS-DOS compatible filesystem that use 8.3 character
 * file names. If this is the first try and the swap file name does not fit in
 * 8.3, detect if this is the case, set shortname and try again.
 */
		if (fname[n - 1] == 'p' && !(buf->b_p_sn || buf->b_shortname))
		{
			char_u			*tail;
			char_u			*fname2;
			struct stat		s1, s2;
			int				f1, f2;
			int				created1 = FALSE, created2 = FALSE;
			int				same = FALSE;

			/*
			 * Check if swapfilename does not fit in 8.3:
			 * It either contains two dots or it is longer than 8 chars.
			 */
			tail = gettail(buf->b_xfilename);
			if (vim_strchr(tail, '.') != NULL || STRLEN(tail) > (size_t)8)
			{
				fname2 = alloc(n + 1);
				if (fname2 != NULL)
				{
					STRCPY(fname2, fname);
					if (vim_strchr(tail, '.') != NULL)
						fname2[n - 1] = 'x';	/* change ".swp" to ".swx" */
					else
						fname2[n - 5] += 1;		/* change "x.swp" to "y.swp" */
					/*
					 * may need to create the files to be able to use stat()
					 */
					f1 = open((char *)fname, O_RDONLY | O_EXTRA);
					if (f1 < 0)
					{
						f1 = open((char *)fname, O_RDWR|O_CREAT|O_EXCL|O_EXTRA
#ifdef AMIGA				/* Amiga has no mode argument */
																			);
#endif
#ifdef UNIX					/* open in rw------- mode */
															  , (mode_t)0600);
#endif
#if defined(MSDOS) || defined(WIN32) || defined(OS2)  /* open read/write */
														, S_IREAD | S_IWRITE);
#endif
#if defined(OS2)
						if (f1 < 0 && errno == ENOENT)
							same = TRUE;
#endif
						created1 = TRUE;
					}
					if (f1 >= 0)
					{
						f2 = open((char *)fname2, O_RDONLY | O_EXTRA);
						if (f2 < 0)
						{
							f2 = open((char *)fname2,
												 O_RDWR|O_CREAT|O_EXCL|O_EXTRA
#ifdef AMIGA				/* Amiga has no mode argument */
																			);
#endif
#ifdef UNIX					/* open in rw------- mode */
															  , (mode_t)0600);
#endif
#if defined(MSDOS) || defined(WIN32) || defined(OS2)  /* open read/write */
														, S_IREAD | S_IWRITE);
#endif
							created2 = TRUE;
						}
						if (f2 >= 0)
						{
							/*
							 * Both files exist now. If stat() returns the
							 * same device and inode they are the same file.
							 */
							if (fstat(f1, &s1) != -1 &&
										fstat(f2, &s2) != -1 &&
										s1.st_dev == s2.st_dev &&
										s1.st_ino == s2.st_ino)
								same = TRUE;
							close(f2);
							if (created2)
								vim_remove(fname2);
						}
						close(f1);
						if (created1)
							vim_remove(fname);
					}
					vim_free(fname2);
					if (same)
					{
						buf->b_shortname = TRUE;
						vim_free(fname);
						fname = makeswapname(buf, dir_name);
						continue;		/* try again with b_shortname set */
					}
				}
			}
		}
#endif
		/*
		 * check if the swapfile already exists
		 */
		if (getperm(fname) < 0)		/* it does not exist */
		{
#ifdef AMIGA
			fh = Open((UBYTE *)fname, (long)MODE_NEWFILE);
			/*
			 * on the Amiga getperm() will return -1 when the file exists but
			 * is being used by another program. This happens if you edit
			 * a file twice.
			 */
			if (fh != (BPTR)NULL)		/* can open file, OK */
			{
				Close(fh);
				break;
			}
			if (IoErr() != ERROR_OBJECT_IN_USE &&
											   IoErr() != ERROR_OBJECT_EXISTS)
#endif
				break;
		}
		/*
		 * A file name equal to old_fname is OK to use.
		 */
		if (old_fname != NULL && fnamecmp(fname, old_fname) == 0)
			break;

		/*
		 * get here when file already exists
		 */
		if (fname[n - 1] == 'p')		/* first try */
		{
#ifndef SHORT_FNAME
			/*
			 * on MS-DOS compatible filesystems (e.g. messydos) file.doc.swp
			 * and file.doc are the same file. To guess if this problem is
			 * present try if file.doc.swx exists. If it does, we set
			 * buf->b_shortname and try file_doc.swp (dots replaced by
			 * underscores for this file), and try again. If it doesn't we
			 * assume that "file.doc.swp" already exists.
			 */
			if (!(buf->b_p_sn || buf->b_shortname))		/* not tried yet */
			{
				fname[n - 1] = 'x';
				r = getperm(fname);			/* try "file.swx" */
				fname[n - 1] = 'p';
				if (r >= 0)					/* "file.swx" seems to exist */
				{
					buf->b_shortname = TRUE;
					vim_free(fname);
					fname = makeswapname(buf, dir_name);
					continue;		/* try again with '.' replaced by '_' */
				}
			}
#endif
			/*
			 * If we get here the ".swp" file really exists.
			 * Give an error message, unless recovering, no file name, we are
			 * viewing a help file or when the path of the file is different
			 * (happens when all .swp files are in one directory).
			 */
			if (!recoverymode && buf->b_xfilename != NULL && !buf->b_help)
			{
				int				fd;
				struct block0	b0;
				int				differ = FALSE;

				/*
				 * Try to read block 0 from the swap file to get the original
				 * file name (and inode number).
				 */
				fd = open((char *)fname, O_RDONLY | O_EXTRA);
				if (fd >= 0)
				{
					if (read(fd, (char *)&b0, sizeof(b0)) == sizeof(b0))
					{
						/*
						 * The name in the swap file may be "~user/path/file".
						 * Expand it first.
						 */
						expand_env(b0.b0_fname, NameBuff, MAXPATHL);
#ifdef CHECK_INODE
						if (fnamecmp_ino(buf->b_filename, NameBuff,
													 char_to_long(b0.b0_ino)))
							differ = TRUE;
#else
						if (fnamecmp(NameBuff, buf->b_filename) != 0)
							differ = TRUE;
#endif
					}
					close(fd);
				}
				if (differ == FALSE)
				{
					struct stat st;

					++no_wait_return;
#ifdef SLEEP_IN_EMSG
					++dont_sleep;
#endif
					(void)EMSG("ATTENTION");
#ifdef SLEEP_IN_EMSG
					--dont_sleep;
#endif
					MSG_OUTSTR("\nFound a swap file by the name \"");
					msg_home_replace(fname);
					MSG_OUTSTR("\"\n");
					swapfile_info(fname);
					MSG_OUTSTR("While opening file \"");
					msg_outtrans(buf->b_xfilename);
					MSG_OUTSTR("\"\n");
					if (stat((char *)buf->b_xfilename, &st) != -1)
					{
						MSG_OUTSTR("             dated: ");
						x = st.st_mtime;	/* Manx C can't do &st.st_mtime */
						MSG_OUTSTR(ctime(&x));
					}
					MSG_OUTSTR("\n(1) Another program may be editing the same file.\n");
					MSG_OUTSTR("    If this is the case, quit this edit session to avoid having\n");
					MSG_OUTSTR("    two different instances of the same file when making changes.\n");
					MSG_OUTSTR("\n(2) An edit session for this file crashed.\n");
					MSG_OUTSTR("    If this is the case, use \":recover\" or \"vim -r ");
					msg_outtrans(buf->b_xfilename);
					MSG_OUTSTR("\"\n    to recover the changes (see \":help recovery)\".\n");
					MSG_OUTSTR("    If you did this already, delete the swap file \"");
					msg_outtrans(fname);
					MSG_OUTSTR("\"\n    to avoid this message.\n\n");
					cmdline_row = msg_row;
					--no_wait_return;
					need_wait_return = TRUE;		/* call wait_return later */
				}
			}
		}

		if (fname[n - 1] == 'a')	/* tried enough names, give up */
		{
			vim_free(fname);
			fname = NULL;
			break;
		}
		--fname[n - 1];				/* change last char of the name */
	}

	vim_free(dir_name);
#ifndef SHORT_FNAME
	if (dummyfd)		/* file has been created temporarily */
	{
		fclose(dummyfd);
		vim_remove(buf->b_xfilename);
	}
#endif
	return fname;
}

	static int
b0_magic_wrong(b0p)
	ZERO_BL *b0p;
{
	return (b0p->b0_magic_long != (long)B0_MAGIC_LONG ||
			b0p->b0_magic_int != (int)B0_MAGIC_INT ||
			b0p->b0_magic_short != (short)B0_MAGIC_SHORT ||
			b0p->b0_magic_char != B0_MAGIC_CHAR);
}

#ifdef CHECK_INODE
/*
 * Compare current file name with file name from swap file.
 * Try to use inode numbers when possible.
 * Return non-zero when files are different.
 *
 * When comparing file names a few things have to be taken into consideration:
 * - When working over a network the full path of a file depends on the host.
 *   We check the inode number if possible.  It is not 100% reliable though,
 *   because the device number cannot be used over a network.
 * - When a file does not exist yet (editing a new file) there is no inode
 *   number.
 * - The file name in a swap file may not be valid on the current host.  The
 *	 "~user" form is used whenever possible to avoid this.
 *
 * This is getting complicated, let's make a table:
 *
 *				ino_c  ino_s  fname_c  fname_s  differ =
 *
 * both files exist -> compare inode numbers:
 *				!= 0   != 0     X        X      ino_c != ino_s
 *
 * inode number(s) unknown, file names available -> compare file names
 *				== 0    X       OK       OK     fname_c != fname_s
 *				 X     == 0     OK       OK     fname_c != fname_s
 *
 * current file doesn't exist, file for swap file exist, file name(s) not
 * available -> probably different
 *				== 0   != 0    FAIL      X      TRUE
 *				== 0   != 0     X       FAIL    TRUE
 *
 * current file exists, inode for swap unknown, file name(s) not
 * available -> probably different
 *				!= 0   == 0    FAIL      X      TRUE
 *				!= 0   == 0     X       FAIL    TRUE
 *
 * current file doesn't exist, inode for swap unknown, one file name not
 * available -> probably different
 *				== 0   == 0    FAIL      OK     TRUE
 *				== 0   == 0     OK      FAIL    TRUE
 *
 * current file doesn't exist, inode for swap unknown, both file names not
 * available -> probably same file
 *				== 0   == 0    FAIL     FAIL    FALSE
 */

	static int
fnamecmp_ino(fname_c, fname_s, ino_block0)
	char_u		*fname_c;			/* current file name */
	char_u		*fname_s;			/* file name from swap file */
	long		ino_block0;
{
	struct stat	st;
	long		ino_c = 0;			/* ino of current file */
	long		ino_s;				/* ino of file from swap file */
	char_u		buf_c[MAXPATHL];	/* full path of fname_c */
	char_u		buf_s[MAXPATHL];	/* full path of fname_s */
	int			retval_c;			/* flag: buf_c valid */
	int			retval_s;			/* flag: buf_s valid */


	if (stat((char *)fname_c, &st) == 0)
		ino_c = st.st_ino;

	/*
	 * First we try to get the inode from the file name, because the inode in
	 * the swap file may be outdated.  If that fails (e.g. this path is not
	 * valid on this machine), use the inode from block 0.
	 */
	if (stat((char *)fname_s, &st) == 0)
		ino_s = st.st_ino;
	else
		ino_s = ino_block0;

	if (ino_c && ino_s)
		return (ino_c != ino_s);

	/*
	 * One of the inode numbers is unknown, try a forced FullName() and
	 * compare the file names.
	 */
	retval_c = FullName(fname_c, buf_c, MAXPATHL, TRUE);
	retval_s = FullName(fname_s, buf_s, MAXPATHL, TRUE);
	if (retval_c == OK && retval_s == OK)
		return (STRCMP(buf_c, buf_s) != 0);

	/*
	 * Can't compare inodes or filenames, guess that the files are different,
	 * unless both appear not to exist at all.
	 */
	if (ino_s == 0 && ino_c == 0 && retval_c == FAIL && retval_s == FAIL)
		return FALSE;
	return TRUE;
}
#endif /* CHECK_INODE */

/*
 * Move a long integer into a four byte character array.
 * Used for machine independency in block zero.
 */
	static void
long_to_char(n, s)
	long	n;
	char_u	*s;
{
	s[0] = (n & 0xff);
	n >>= 8;
	s[1] = (n & 0xff);
	n >>= 8;
	s[2] = (n & 0xff);
	n >>= 8;
	s[3] = (n & 0xff);
}

	static long
char_to_long(s)
	char_u	*s;
{
	long	retval;

	retval = s[3];
	retval <<= 8;
	retval += s[2];
	retval <<= 8;
	retval += s[1];
	retval <<= 8;
	retval += s[0];

	return retval;
}
