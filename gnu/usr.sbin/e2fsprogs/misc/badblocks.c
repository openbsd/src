/*
 * badblocks.c		- Bad blocks checker
 *
 * Copyright (C) 1992, 1993, 1994  Remy Card <card@masi.ibp.fr>
 *                                 Laboratoire MASI, Institut Blaise Pascal
 *                                 Universite Pierre et Marie Curie (Paris VI)
 *
 * Copyright 1995, 1996, 1997 by Theodore Ts'o
 *
 * This file is based on the minix file system programs fsck and mkfs
 * written and copyrighted by Linus Torvalds <Linus.Torvalds@cs.helsinki.fi>
 * 
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

/*
 * History:
 * 93/05/26	- Creation from e2fsck
 * 94/02/27	- Made a separate bad blocks checker
 */

#include <errno.h>
#include <fcntl.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/types.h>

#if HAVE_LINUX_FS_H
#include <linux/fd.h>
#include <linux/fs.h>
#endif

#include "et/com_err.h"
#include "ext2fs/io.h"

const char * program_name = "badblocks";

int v_flag = 0;			/* verbose */
int w_flag = 0;			/* do r/w test */
int s_flag = 0;			/* show progress of test */

static volatile void usage (void)
{
	fprintf (stderr, "Usage: %s [-b block_size] [-o output_file] [-svw] device blocks_count\n [start_count]\n",
		 program_name);
	exit (1);
}

static unsigned long currently_testing = 0;
static unsigned long num_blocks = 0;

static void print_status(void)
{
	fprintf(stderr, "%9ld/%9ld", currently_testing, num_blocks);
	fprintf(stderr, "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
	fflush (stderr);
}

static void alarm_intr (int alnum)
{
	signal (SIGALRM, alarm_intr);
	alarm(1);
	if (!num_blocks)
		return;
	fprintf(stderr, "%9ld/%9ld", currently_testing, num_blocks);
	fprintf(stderr, "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
	fflush (stderr);
}

/*
 * Perform a test of a block; return the number of blocks readable/writeable.
 */
static long do_test (int dev, char * buffer, int try, unsigned long block_size,
		     unsigned long current_block)
{
	long got;

	if (v_flag > 1)
		print_status();

	/* Seek to the correct loc. */
	if (ext2fs_llseek (dev, (ext2_loff_t) current_block * block_size,
			 SEEK_SET) != (ext2_loff_t) current_block * block_size)
		com_err (program_name, errno, "during seek");

	/* Try the read */
	got = read (dev, buffer, try * block_size);
	if (got < 0)
		got = 0;	
	if (got & (block_size - 1))
		fprintf (stderr,
			 "Weird value (%ld) in do_test: probably bugs\n",
			 got);
	got /= block_size;
	return got;
}

static void flush_bufs (int dev, int sync)
{
  if (v_flag
#if !defined (BLKFLSBUF) && !defined (FDFLUSH)
      && sync
#endif
      )
    fprintf (stderr, "Flushing buffers\n");

  if (sync && fsync (dev) == -1)
    com_err (program_name, errno, "during fsync");

#ifdef BLKLSBUF
  ioctl (dev, BLKFLSBUF, 0);	/* In case this is a HD */
#endif
#ifdef FDFLUSH
  ioctl (dev, FDFLUSH, 0);	/* In case this is floppy */
#endif
}

static void test_ro (int dev, unsigned long blocks_count,
		     unsigned long block_size, FILE * out,
		     unsigned long from_count)
{
#define TEST_BUFFER_BLOCKS 16
	char * blkbuf;
	int try;
	long got;

	blkbuf = malloc (TEST_BUFFER_BLOCKS * block_size);
	if (!blkbuf)
	{
		com_err (program_name, ENOMEM, "while allocating buffers");
		exit (1);
	}
	flush_bufs (dev, 0);
	if (v_flag) {
	    fprintf (stderr,
		     "Checking for bad blocks in read-only mode\n");
	    fprintf (stderr, "From block %lu to %lu\n", from_count, blocks_count);
	}
	try = TEST_BUFFER_BLOCKS;
	currently_testing = from_count;
	num_blocks = blocks_count;
	if (s_flag || v_flag > 1) {
		fprintf(stderr, "Checking for bad blocks (read-only test): ");
		if (v_flag <= 1)
			alarm_intr(SIGALRM);
	}
	while (currently_testing < blocks_count)
	{
		if (currently_testing + try > blocks_count)
			try = blocks_count - currently_testing;
		got = do_test (dev, blkbuf, try, block_size, currently_testing);
		currently_testing += got;
		if (got == try) {
			try = TEST_BUFFER_BLOCKS;
			continue;
		}
		else
			try = 1;
		if (got == 0)
			fprintf (out, "%lu\n", currently_testing++);
	}
	num_blocks = 0;
	alarm(0);
	if (s_flag || v_flag > 1)
		fprintf(stderr, "done               \n");
	fflush (stderr);
	free (blkbuf);
}

static void test_rw (int dev, unsigned long blocks_count,
		     unsigned long block_size, FILE * out,
		     unsigned long from_count)
{
	int i;
	char * buffer;
	unsigned char pattern[] = {0xaa, 0x55, 0xff, 0x00};

	buffer = malloc (2 * block_size);
	if (!buffer)
	{
		com_err (program_name, ENOMEM, "while allocating buffers");
		exit (1);
	}

	flush_bufs (dev, 0);

	if (v_flag) {
		fprintf(stderr,
			"Checking for bad blocks in read-write mode\n");
		fprintf(stderr, "From block %lu to %lu\n",
			 from_count, blocks_count);
	}
	for (i = 0; i < sizeof (pattern); i++) {
		memset (buffer, pattern[i], block_size);
		if (s_flag | v_flag)
			fprintf (stderr, "Writing pattern 0x%08x: ",
				 *((int *) buffer));
		num_blocks = blocks_count;
		currently_testing = from_count;
		if (s_flag && v_flag <= 1)
			alarm_intr(SIGALRM);
		for (;
		     currently_testing < blocks_count;
		     currently_testing++)
		{
			if (ext2fs_llseek (dev, (ext2_loff_t) currently_testing *
					 block_size, SEEK_SET) !=
			    (ext2_loff_t) currently_testing * block_size)
				com_err (program_name, errno,
					 "during seek on block %d",
					 currently_testing);
			if (v_flag > 1)
				print_status();
			write (dev, buffer, block_size);
		}
		num_blocks = 0;
		alarm (0);
		if (s_flag | v_flag)
			fprintf(stderr, "done               \n");
		flush_bufs (dev, 1);
		if (s_flag | v_flag)
			fprintf (stderr, "Reading and comparing: ");
		num_blocks = blocks_count;
		currently_testing = from_count;
		if (s_flag && v_flag <= 1)
			alarm_intr(SIGALRM);
		for (;
		     currently_testing < blocks_count;
		     currently_testing++)
		{
			if (ext2fs_llseek (dev, (ext2_loff_t) currently_testing *
					 block_size, SEEK_SET) !=
			    (ext2_loff_t) currently_testing * block_size)
				com_err (program_name, errno,
					 "during seek on block %d",
					 currently_testing);
			if (v_flag > 1)
				print_status();
			if (read (dev, buffer + block_size, block_size) < block_size)
				fprintf (out, "%ld\n", currently_testing);
			else if (memcmp (buffer, buffer + block_size, block_size))
				fprintf (out, "%ld\n", currently_testing);
		}
		num_blocks = 0;
		alarm (0);
		if (s_flag | v_flag)
			fprintf(stderr, "done           \n");
		flush_bufs (dev, 0);
	}
}

void main (int argc, char ** argv)
{
	char c;
	char * tmp;
	char * device_name;
	char * output_file = NULL;
	FILE * out;
	unsigned long block_size = 1024;
	unsigned long blocks_count, from_count;
	int dev;

	setbuf(stdout, NULL);
	setbuf(stderr, NULL);
	if (argc && *argv)
		program_name = *argv;
	while ((c = getopt (argc, argv, "b:o:svw")) != EOF) {
		switch (c) {
		case 'b':
			block_size = strtoul (optarg, &tmp, 0);
			if (*tmp || block_size > 4096) {
				com_err (program_name, 0,
					 "bad block size - %s", optarg);
				exit (1);
			}
			break;
		case 'o':
			output_file = optarg;
			break;
		case 's':
			s_flag = 1;
			break;
		case 'v':
			v_flag++;
			break;
		case 'w':
			w_flag = 1;
			break;
		default:
			usage ();
		}
	}
	if (optind > argc - 1)
		usage ();
	device_name = argv[optind++];
	if (optind > argc - 1)
		usage ();
	blocks_count = strtoul (argv[optind], &tmp, 0);
	if (*tmp)
	{
		com_err (program_name, 0, "bad blocks count - %s", argv[optind]);
		exit (1);
	}
	if (++optind <= argc-1) {
		from_count = strtoul (argv[optind], &tmp, 0);
	} else from_count = 0;
	if (from_count >= blocks_count) {
	    com_err (program_name, 0, "bad blocks range: %lu-%lu",
		     from_count, blocks_count);
	    exit (1);
	}
	dev = open (device_name, w_flag ? O_RDWR : O_RDONLY);
	if (dev == -1)
	{
		com_err (program_name, errno,"while trying to open %s",
			 device_name);
		exit (1);
	}
	if (output_file && strcmp (output_file, "-") != 0)
	{
		out = fopen (output_file, "w");
		if (out == NULL)
		{
			com_err (program_name, errno,"while trying to open %s",
				 device_name);
			exit (1);
		}
	}
	else
		out = stdout;
	if (w_flag)
		test_rw (dev, blocks_count, block_size, out, from_count);
	else
		test_ro (dev, blocks_count, block_size, out, from_count);
	close (dev);
	if (out != stdout)
		fclose (out);
}
