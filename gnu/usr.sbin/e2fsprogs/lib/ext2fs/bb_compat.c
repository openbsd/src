/*
 * bb_compat.c --- compatibility badblocks routines
 * 
 * Copyright (C) 1997 Theodore Ts'o.
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

#include "ext2fsP.h"

errcode_t badblocks_list_create(badblocks_list *ret, int size)
{
	return ext2fs_badblocks_list_create(ret, size);
}

void badblocks_list_free(badblocks_list bb)
{
	ext2fs_badblocks_list_free(bb);
}

errcode_t badblocks_list_add(badblocks_list bb, blk_t blk)
{
	return ext2fs_badblocks_list_add(bb, blk);
}

int badblocks_list_test(badblocks_list bb, blk_t blk)
{
	return ext2fs_badblocks_list_test(bb, blk);
}

errcode_t badblocks_list_iterate_begin(badblocks_list bb,
				       badblocks_iterate *ret)
{
	return ext2fs_badblocks_list_iterate_begin(bb, ret);
}

int badblocks_list_iterate(badblocks_iterate iter, blk_t *blk)
{
	return ext2fs_badblocks_list_iterate(iter, blk);
}

void badblocks_list_iterate_end(badblocks_iterate iter)
{
	ext2fs_badblocks_list_iterate_end(iter);
}
