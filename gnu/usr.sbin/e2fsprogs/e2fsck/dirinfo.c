/*
 * dirinfo.c --- maintains the directory information table for e2fsck.
 *
 * Copyright (C) 1993 Theodore Ts'o.  This file may be redistributed
 * under the terms of the GNU Public License.
 */

#include <et/com_err.h>
#include "e2fsck.h"

static int		dir_info_count = 0;
static int		dir_info_size = 0;
static struct dir_info	*dir_info = 0;

int get_num_dirs(ext2_filsys fs)
{
	int	i, num_dirs;

	num_dirs = 0;
	for (i = 0; i < fs->group_desc_count; i++)
		num_dirs += fs->group_desc[i].bg_used_dirs_count;

	return num_dirs;
}

/*
 * This subroutine is called during pass1 to create a directory info
 * entry.  During pass1, the passed-in parent is 0; it will get filled
 * in during pass2.  
 */
void add_dir_info(ext2_filsys fs, ino_t ino, ino_t parent)
{
	struct dir_info *dir;
	int	i, j;

#if 0
	printf("add_dir_info for inode %lu...\n", ino);
#endif
	if (!dir_info) {
		dir_info_count = 0;
		dir_info_size = get_num_dirs(fs) + 10;

		dir_info  = allocate_memory(dir_info_size *
					   sizeof (struct dir_info),
					   "directory map");
	}
	
	if (dir_info_count >= dir_info_size) {
		dir_info_size += 10;
		dir_info = realloc(dir_info,
				  dir_info_size * sizeof(struct dir_info));
	}

	/*
	 * Normally, add_dir_info is called with each inode in
	 * sequential order; but once in a while (like when pass 3
	 * needs to recreate the root directory or lost+found
	 * directory) it is called out of order.  In those cases, we
	 * need to move the dir_info entries down to make room, since
	 * the dir_info array needs to be sorted by inode number for
	 * get_dir_info()'s sake.
	 */
	if (dir_info_count && dir_info[dir_info_count-1].ino >= ino) {
		for (i = dir_info_count-1; i > 0; i--)
			if (dir_info[i-1].ino < ino)
				break;
		dir = &dir_info[i];
		if (dir->ino != ino) 
			for (j = dir_info_count++; j > i; j--)
				dir_info[j] = dir_info[j-1];
	} else
		dir = &dir_info[dir_info_count++];
	
	dir->ino = ino;
	dir->dotdot = parent;
	dir->parent = parent;
}

/*
 * get_dir_info() --- given an inode number, try to find the directory
 * information entry for it.
 */
struct dir_info *get_dir_info(ino_t ino)
{
	int	low, high, mid;

	low = 0;
	high = dir_info_count-1;
	if (!dir_info)
		return 0;
	if (ino == dir_info[low].ino)
		return &dir_info[low];
	if  (ino == dir_info[high].ino)
		return &dir_info[high];

	while (low < high) {
		mid = (low+high)/2;
		if (mid == low || mid == high)
			break;
		if (ino == dir_info[mid].ino)
			return &dir_info[mid];
		if (ino < dir_info[mid].ino)
			high = mid;
		else
			low = mid;
	}
	return 0;
}

/*
 * Free the dir_info structure when it isn't needed any more.
 */
void free_dir_info(ext2_filsys fs)
{
	if (dir_info) {
		free(dir_info);
		dir_info = 0;
	}
	dir_info_size = 0;
	dir_info_count = 0;
}

/*
 * A simple interator function
 */
struct dir_info *dir_info_iter(int *control)
{
	if (*control >= dir_info_count)
		return 0;

	return(dir_info + (*control)++);
}
