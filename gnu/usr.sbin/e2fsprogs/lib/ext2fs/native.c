/*
 * native.c --- returns the ext2_flag for a native byte order
 * 
 * Copyright (C) 1996 Theodore Ts'o.
 * 
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include <stdio.h>
#include <linux/ext2_fs.h>

#include "ext2fs.h"

static int i386_byteorder(void)
{
	int one = 1;
	char *cp = (char *) &one;

	return (*cp == 1);
}

int ext2fs_native_flag(void)
{
	if (i386_byteorder())
		return 0;
	return EXT2_FLAG_SWAP_BYTES;
}

	
	
