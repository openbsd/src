/*
 * dumpe2fs.c		- List the control structures of a second
 *			  extended filesystem
 *
 * Copyright (C) 1992, 1993, 1994  Remy Card <card@masi.ibp.fr>
 *                                 Laboratoire MASI, Institut Blaise Pascal
 *                                 Universite Pierre et Marie Curie (Paris VI)
 *
 * Copyright 1995, 1996, 1997 by Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

/*
 * History:
 * 94/01/09	- Creation
 * 94/02/27	- Ported to use the ext2fs library
 */

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <linux/ext2_fs.h>

#include "ext2fs/ext2fs.h"
#include "e2p/e2p.h"

#include "../version.h"

#define in_use(m, x)	(ext2fs_test_bit ((x), (m)))

const char * program_name = "dumpe2fs";
char * device_name = NULL;

static volatile void usage (void)
{
	fprintf (stderr, "usage: %s [-bV] device\n", program_name);
	exit (1);
}

static void print_free (unsigned long group, char * bitmap,
			unsigned long nbytes, unsigned long offset)
{
	int p = 0;
	unsigned long i;
	unsigned long j;

	for (i = 0; i < nbytes; i++)
		if (!in_use (bitmap, i))
		{
			if (p)
				printf (", ");
			if (i == nbytes - 1 || in_use (bitmap, i + 1))
				printf ("%lu", group * nbytes + i + offset);
			else
			{
				for (j = i; j < nbytes && !in_use (bitmap, j);
				     j++)
					;
				printf ("%lu-%lu", group * nbytes + i + offset,
					group * nbytes + (j - 1) + offset);
				i = j - 1;
			}
			p = 1;
		}
}

static void list_desc (ext2_filsys fs)
{
	unsigned long i;
	blk_t	group_blk, next_blk;
	char * block_bitmap = fs->block_map->bitmap;
	char * inode_bitmap = fs->inode_map->bitmap;

	printf ("\n");
	group_blk = fs->super->s_first_data_block;
	for (i = 0; i < fs->group_desc_count; i++) {
		next_blk = group_blk + fs->super->s_blocks_per_group;
		if (next_blk > fs->super->s_blocks_count)
			next_blk = fs->super->s_blocks_count;
		printf ("Group %lu: (Blocks %u -- %u)\n", i,
			group_blk, next_blk -1 );
		printf ("  Block bitmap at %u (+%d), "
			"Inode bitmap at %u (+%d)\n  "
			"Inode table at %u (+%d)\n",
			fs->group_desc[i].bg_block_bitmap,
			fs->group_desc[i].bg_block_bitmap - group_blk,
			fs->group_desc[i].bg_inode_bitmap,
			fs->group_desc[i].bg_inode_bitmap - group_blk,
			fs->group_desc[i].bg_inode_table,
			fs->group_desc[i].bg_inode_table - group_blk);
		printf ("  %d free blocks, %d free inodes, %d directories\n",
			fs->group_desc[i].bg_free_blocks_count,
			fs->group_desc[i].bg_free_inodes_count,
			fs->group_desc[i].bg_used_dirs_count);
		printf ("  Free blocks: ");
		print_free (i, block_bitmap, fs->super->s_blocks_per_group,
			    fs->super->s_first_data_block);
		block_bitmap += fs->super->s_blocks_per_group / 8;
		printf ("\n");
		printf ("  Free inodes: ");
		print_free (i, inode_bitmap, fs->super->s_inodes_per_group, 1);
		inode_bitmap += fs->super->s_inodes_per_group / 8;
		printf ("\n");
		group_blk = next_blk;
	}
}

static void list_bad_blocks(ext2_filsys fs)
{
	badblocks_list		bb_list = 0;
	badblocks_iterate	bb_iter;
	blk_t			blk;
	errcode_t		retval;

	retval = ext2fs_read_bb_inode(fs, &bb_list);
	if (retval) {
		com_err("ext2fs_read_bb_inode", retval, "");
		exit(1);
	}
	retval = badblocks_list_iterate_begin(bb_list, &bb_iter);
	if (retval) {
		com_err("badblocks_list_iterate_begin", retval,
			"while printing bad block list");
		exit(1);
	}
	if (badblocks_list_iterate(bb_iter, &blk))
		printf("Bad blocks: %d", blk);
	while (badblocks_list_iterate(bb_iter, &blk))
		printf(", %d", blk);
	badblocks_list_iterate_end(bb_iter);
	printf("\n");
}

static void dump_bad_blocks(ext2_filsys fs)
{
	badblocks_list		bb_list = 0;
	badblocks_iterate	bb_iter;
	blk_t			blk;
	errcode_t		retval;

	retval = ext2fs_read_bb_inode(fs, &bb_list);
	if (retval) {
		com_err("ext2fs_read_bb_inode", retval, "");
		exit(1);
	}
	retval = badblocks_list_iterate_begin(bb_list, &bb_iter);
	if (retval) {
		com_err("badblocks_list_iterate_begin", retval,
			"while printing bad block list");
		exit(1);
	}
	while (badblocks_list_iterate(bb_iter, &blk))
		printf("%d\n", blk);
	badblocks_list_iterate_end(bb_iter);
}

static int i386_byteorder(void)
{
	int one = 1;
	char *cp = (char *) &one;

	return (*cp == 1);
}

void main (int argc, char ** argv)
{
	errcode_t	retval;
	ext2_filsys	fs;
	int		print_badblocks = 0;
	int		big_endian;
	char		c;

	initialize_ext2_error_table();
	fprintf (stderr, "dumpe2fs %s, %s for EXT2 FS %s, %s\n",
		 E2FSPROGS_VERSION, E2FSPROGS_DATE,
		 EXT2FS_VERSION, EXT2FS_DATE);
	if (argc && *argv)
		program_name = *argv;
	
	while ((c = getopt (argc, argv, "bV")) != EOF) {
		switch (c) {
		case 'b':
			print_badblocks++;
			break;
		case 'V':
			/* Print version number and exit */
			fprintf(stderr, "\tUsing %s\n",
				error_message(EXT2_ET_BASE));
			exit(0);
		default:
			usage ();
		}
	}
	if (optind > argc - 1)
		usage ();
	device_name = argv[optind++];
	retval = ext2fs_open (device_name, 0, 0, 0, unix_io_manager, &fs);
	if (retval) {
		com_err (program_name, retval, "while trying to open %s",
			 device_name);
		printf ("Couldn't find valid filesystem superblock.\n");
		exit (1);
	}
	if (print_badblocks) {
		dump_bad_blocks(fs);
	} else {
		retval = ext2fs_read_bitmaps (fs);
		if (retval) {
			com_err (program_name, retval,
				 "while trying to read the bitmaps",
				 device_name);
			ext2fs_close (fs);
			exit (1);
		}
		big_endian = ((fs->flags & EXT2_FLAG_SWAP_BYTES) != 0);
		if (!i386_byteorder())
			big_endian = !big_endian;
		if (big_endian)
			printf("Note: This is a byte-swapped filesystem\n");
		list_super (fs->super);
		list_bad_blocks (fs);
		list_desc (fs);
	}
	ext2fs_close (fs);
	exit (0);
}
