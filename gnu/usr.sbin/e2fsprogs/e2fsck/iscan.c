/*
 * Test to see how quickly we can scan the inode table (not doing
 * anything else)
 */

#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <termios.h>
#include <time.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#include <unistd.h>
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_MNTENT_H
#include <mntent.h>
#endif
#include <sys/ioctl.h>
#include <malloc.h>

#include "et/com_err.h"
#include "e2fsck.h"
#include "../version.h"

extern int isatty(int);

const char * program_name = "iscan";
const char * device_name = NULL;

int yflag = 0;
int nflag = 0;
int preen = 0;
int inode_buffer_blocks = 0;
int invalid_bitmaps = 0;

struct resource_track	global_rtrack;

static void usage(NOARGS)
{
	fprintf(stderr,
		"Usage: %s [-F] [-I inode_buffer_blocks] device\n",
		program_name);
	exit(1);
}

static void PRS(int argc, char *argv[])
{
	int		flush = 0;
	char		c;
#ifdef MTRACE
	extern void	*mallwatch;
#endif

	setbuf(stdout, NULL);
	setbuf(stderr, NULL);
	initialize_ext2_error_table();
	
	if (argc && *argv)
		program_name = *argv;
	while ((c = getopt (argc, argv, "FI")) != EOF)
		switch (c) {
		case 'F':
#ifdef BLKFLSBUF
			flush = 1;
#else
			fatal_error ("-F not supported");
#endif
			break;
		case 'I':
			inode_buffer_blocks = atoi(optarg);
			break;
		default:
			usage ();
		}
	device_name = argv[optind];
	if (flush) {
#ifdef BLKFLSBUF
		int	fd = open(device_name, O_RDONLY, 0);

		if (fd < 0) {
			com_err("open", errno, "while opening %s for flushing",
				device_name);
			exit(FSCK_ERROR);
		}
		if (ioctl(fd, BLKFLSBUF, 0) < 0) {
			com_err("BLKFLSBUF", errno, "while trying to flush %s",
				device_name);
			exit(FSCK_ERROR);
		}
		close(fd);
#else
		fatal_error ("BLKFLSBUF not supported");
#endif /* BLKFLSBUF */
	}
}
					
int main (int argc, char *argv[])
{
	errcode_t	retval = 0;
	int		exit_value = FSCK_OK;
	ext2_filsys	fs;
	ino_t	ino;
	int	num_inodes = 0;
	struct ext2_inode inode;
	ext2_inode_scan	scan;
	
	init_resource_track(&global_rtrack);

	PRS(argc, argv);

	retval = ext2fs_open(device_name, 0,
			     0, 0, unix_io_manager, &fs);
	if (retval) {
		com_err(program_name, retval, "while trying to open %s",
			device_name);
		exit(1);
	}

	ehandler_init(fs->io);
	
	retval = ext2fs_open_inode_scan(fs, inode_buffer_blocks, &scan);
	if (retval) {
		com_err(program_name, retval, "while opening inode scan");
		fatal_error(0);
	}

	while (1) {
		retval = ext2fs_get_next_inode(scan, &ino, &inode);
		if (retval) {
			com_err(program_name, retval,
				"while getting next inode");
			fatal_error(0);
		}
		if (ino == 0)
			break;
		num_inodes++;
	}
	
	print_resource_track(&global_rtrack);
	printf("%d inodes scanned.\n", num_inodes);
	
	exit(0);
}
