/*
 * read_bb_file.c --- read a list of bad blocks for a FILE *
 *
 * Copyright (C) 1994, 1995 Theodore Ts'o.
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

#include <linux/ext2_fs.h>

#include "ext2fs.h"

/*
 * Reads a list of bad blocks from  a FILE *
 */
errcode_t ext2fs_read_bb_FILE(ext2_filsys fs, FILE *f, 
			      ext2_badblocks_list *bb_list,
			      void (*invalid)(ext2_filsys fs, blk_t blk))
{
	errcode_t	retval;
	blk_t		blockno;
	int		count;
	char		buf[128];

	EXT2_CHECK_MAGIC(fs, EXT2_ET_MAGIC_EXT2FS_FILSYS);

	if (!*bb_list) {
		retval = ext2fs_badblocks_list_create(bb_list, 10);
		if (retval)
			return retval;
	}

	while (!feof (f)) {
		if (fgets(buf, sizeof(buf), f) == NULL)
			break;
		count = sscanf(buf, "%u", &blockno);
		if (count <= 0)
			continue;
		if ((blockno < fs->super->s_first_data_block) ||
		    (blockno >= fs->super->s_blocks_count)) {
			if (invalid)
				(invalid)(fs, blockno);
			continue;
		}
		retval = ext2fs_badblocks_list_add(*bb_list, blockno);
		if (retval)
			return retval;
	}
	return 0;
}


