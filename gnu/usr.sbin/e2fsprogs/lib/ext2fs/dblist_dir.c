/*
 * dblist_dir.c --- iterate by directory entry
 *
 * Copyright 1997 by Theodore Ts'o
 * 
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 * 
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include <linux/ext2_fs.h>

#include "ext2fsP.h"

static int db_dir_proc(ext2_filsys fs, struct ext2_db_entry *db_info,
		       void *private);

extern errcode_t
	ext2fs_dblist_dir_iterate(ext2_dblist dblist,
				  int	flags,
				  char	*block_buf,
				  int (*func)(ino_t	dir,
					      int	entry,
					      struct ext2_dir_entry *dirent,
					      int	offset,
					      int	blocksize,
					      char	*buf,
					      void	*private),
				  void *private)
{
	errcode_t		retval;
	struct dir_context	ctx;

	EXT2_CHECK_MAGIC(dblist, EXT2_ET_MAGIC_DBLIST);

	ctx.dir = 0;
	ctx.flags = flags;
	if (block_buf)
		ctx.buf = block_buf;
	else {
		ctx.buf = malloc(dblist->fs->blocksize);
		if (!ctx.buf)
			return ENOMEM;
	}
	ctx.func = 0;
	ctx.func2 = func;
	ctx.private = private;
	ctx.errcode = 0;

	retval = ext2fs_dblist_iterate(dblist, db_dir_proc, &ctx);
	
	if (!block_buf)
		free(ctx.buf);
	if (retval)
		return retval;
	return ctx.errcode;
}

static int db_dir_proc(ext2_filsys fs, struct ext2_db_entry *db_info,
		       void *private)
{
	struct dir_context	*ctx = private;

	ctx->dir = db_info->ino;
	
	return ext2fs_process_dir_block(fs, &db_info->blk,
					db_info->blockcnt, private);
}
