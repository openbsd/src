/*
 * badblocks.c --- routines to manipulate the bad block structure
 * 
 * Copyright (C) 1994, 1995, 1996 Theodore Ts'o.
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

/*
 * Helper function for making a badblocks list
 */
static errcode_t make_badblocks_list(int size, int num, blk_t *list,
				     ext2_badblocks_list *ret)
{
	ext2_badblocks_list	bb;
	
	bb = malloc(sizeof(struct ext2_struct_badblocks_list));
	if (!bb)
		return ENOMEM;
	memset(bb, 0, sizeof(struct ext2_struct_badblocks_list));
	bb->magic = EXT2_ET_MAGIC_BADBLOCKS_LIST;
	bb->size = size ? size : 10;
	bb->num = num;
	bb->list = malloc(bb->size * sizeof(blk_t));
	if (!bb->list) {
		free(bb);
		return ENOMEM;
	}
	if (list)
		memcpy(bb->list, list, bb->size * sizeof(blk_t));
	else
		memset(bb->list, 0, bb->size * sizeof(blk_t));
	*ret = bb;
	return 0;
}
	

/*
 * This procedure creates an empty badblocks list.
 */
errcode_t ext2fs_badblocks_list_create(ext2_badblocks_list *ret, int size)
{
	return make_badblocks_list(size, 0, 0, ret);
}

/*
 * This procedure copies a badblocks list
 */
errcode_t ext2fs_badblocks_copy(ext2_badblocks_list src,
				ext2_badblocks_list *dest)
{
	errcode_t	retval;
	
	retval = make_badblocks_list(src->size, src->num, src->list,
				     dest);
	if (retval)
		return retval;
	(*dest)->badblocks_flags = src->badblocks_flags;
	return 0;
}


/*
 * This procedure frees a badblocks list.
 *
 * (note: moved to closefs.c)
 */


/*
 * This procedure adds a block to a badblocks list.
 */
errcode_t ext2fs_badblocks_list_add(ext2_badblocks_list bb, blk_t blk)
{
	int	i, j;
	blk_t	*new_list;

	EXT2_CHECK_MAGIC(bb, EXT2_ET_MAGIC_BADBLOCKS_LIST);

	if (bb->num >= bb->size) {
		bb->size += 10;
		new_list = realloc(bb->list, bb->size * sizeof(blk_t));
		if (!new_list)
			return ENOMEM;
		bb->list = new_list;
	}

	j = bb->num;
	for (i=0; i < bb->num; i++) {
		if (bb->list[i] == blk)
			return 0;
		if (bb->list[i] > blk) {
			j = i;
			break;
		}
	}
	for (i=bb->num; i > j; i--)
		bb->list[i] = bb->list[i-1];
	bb->list[j] = blk;
	bb->num++;
	return 0;
}

/*
 * This procedure tests to see if a particular block is on a badblocks
 * list.
 */
int ext2fs_badblocks_list_test(ext2_badblocks_list bb, blk_t blk)
{
	int	low, high, mid;

	if (bb->magic != EXT2_ET_MAGIC_BADBLOCKS_LIST)
		return 0;

	low = 0;
	high = bb->num-1;
	if (blk == bb->list[low])
		return 1;
	if (blk == bb->list[high])
		return 1;

	while (low < high) {
		mid = (low+high)/2;
		if (mid == low || mid == high)
			break;
		if (blk == bb->list[mid])
			return 1;
		if (blk < bb->list[mid])
			high = mid;
		else
			low = mid;
	}
	return 0;
}

errcode_t ext2fs_badblocks_list_iterate_begin(ext2_badblocks_list bb,
					      ext2_badblocks_iterate *ret)
{
	ext2_badblocks_iterate iter;

	EXT2_CHECK_MAGIC(bb, EXT2_ET_MAGIC_BADBLOCKS_LIST);

	iter = malloc(sizeof(struct ext2_struct_badblocks_iterate));
	if (!iter)
		return ENOMEM;

	iter->magic = EXT2_ET_MAGIC_BADBLOCKS_ITERATE;
	iter->bb = bb;
	iter->ptr = 0;
	*ret = iter;
	return 0;
}

int ext2fs_badblocks_list_iterate(ext2_badblocks_iterate iter, blk_t *blk)
{
	ext2_badblocks_list	bb;

	if (iter->magic != EXT2_ET_MAGIC_BADBLOCKS_ITERATE)
		return 0;

	bb = iter->bb;

	if (bb->magic != EXT2_ET_MAGIC_BADBLOCKS_LIST)
		return 0;
	
	if (iter->ptr < bb->num) {
		*blk = bb->list[iter->ptr++];
		return 1;
	} 
	*blk = 0;
	return 0;
}

void ext2fs_badblocks_list_iterate_end(ext2_badblocks_iterate iter)
{
	if (!iter || (iter->magic != EXT2_ET_MAGIC_BADBLOCKS_ITERATE))
		return;

	iter->bb = 0;
	free(iter);
}




		
