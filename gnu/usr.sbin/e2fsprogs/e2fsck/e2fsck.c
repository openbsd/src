/*
 * e2fsck.c - a consistency checker for the new extended file system.
 * 
 * Copyright (C) 1993, 1994, 1995, 1996, 1997 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

/* Usage: e2fsck [-dfpnsvy] device
 *	-d -- debugging this program
 *	-f -- check the fs even if it is marked valid
 *	-p -- "preen" the filesystem
 * 	-n -- open the filesystem r/o mode; never try to fix problems
 *	-v -- verbose (tells how many files)
 * 	-y -- always answer yes to questions
 *
 * The device may be a block device or a image of one, but this isn't
 * enforced (but it's not much fun on a character device :-). 
 */

#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
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
#include "uuid/uuid.h"
#include "e2fsck.h"
#include "problem.h"
#include "../version.h"

extern int isatty(int);

const char * program_name = "e2fsck";
const char * device_name = NULL;
const char * filesystem_name = NULL;

/* Command line options */
int nflag = 0;
int yflag = 0;
int tflag = 0;			/* Do timing */
int cflag = 0;			/* check disk */
int preen = 0;
int rwflag = 1;
int swapfs = 0;
int normalize_swapfs = 0;
int inode_buffer_blocks = 0;
blk_t use_superblock;
blk_t superblock;
int blocksize = 0;
int verbose = 0;
int list = 0;
int debug = 0;
int force = 0;
int invalid_bitmaps = 0;
static int show_version_only = 0;

static int replace_bad_blocks = 0;
static char *bad_blocks_file = 0;

static int possible_block_sizes[] = { 1024, 2048, 4096, 8192, 0};

struct resource_track	global_rtrack;

static int root_filesystem = 0;
static int read_only_root = 0;

int *invalid_inode_bitmap;
int *invalid_block_bitmap;
int *invalid_inode_table;
int restart_e2fsck = 0;

static void usage(NOARGS)
{
	fprintf(stderr,
		"Usage: %s [-panyrcdfvstFSV] [-b superblock] [-B blocksize]\n"
		"\t\t[-I inode_buffer_blocks] [-P process_inode_size]\n"
		"\t\t[-l|-L bad_blocks_file] device\n", program_name);
	exit(FSCK_USAGE);
}

static void show_stats(ext2_filsys fs)
{
	int inodes, inodes_used, blocks, blocks_used;
	int dir_links;
	int num_files, num_links;
	int frag_percent;

	dir_links = 2 * fs_directory_count - 1;
	num_files = fs_total_count - dir_links;
	num_links = fs_links_count - dir_links;
	inodes = fs->super->s_inodes_count;
	inodes_used = (fs->super->s_inodes_count -
		       fs->super->s_free_inodes_count);
	blocks = fs->super->s_blocks_count;
	blocks_used = (fs->super->s_blocks_count -
		       fs->super->s_free_blocks_count);

	frag_percent = (10000 * fs_fragmented) / inodes_used;
	frag_percent = (frag_percent + 5) / 10;
	
	if (!verbose) {
		printf("%s: %d/%d files (%0d.%d%% non-contiguous), %d/%d blocks\n",
		       device_name, inodes_used, inodes,
		       frag_percent / 10, frag_percent % 10,
		       blocks_used, blocks);
		return;
	}
	printf ("\n%8d inode%s used (%d%%)\n", inodes_used,
		(inodes_used != 1) ? "s" : "",
		100 * inodes_used / inodes);
	printf ("%8d non-contiguous inodes (%0d.%d%%)\n",
		fs_fragmented, frag_percent / 10, frag_percent % 10);
	printf ("         # of inodes with ind/dind/tind blocks: %d/%d/%d\n",
		fs_ind_count, fs_dind_count, fs_tind_count);
	printf ("%8d block%s used (%d%%)\n"
		"%8d bad block%s\n", blocks_used,
		(blocks_used != 1) ? "s" : "",
		100 * blocks_used / blocks, fs_badblocks_count,
		fs_badblocks_count != 1 ? "s" : "");
	printf ("\n%8d regular file%s\n"
		"%8d director%s\n"
		"%8d character device file%s\n"
		"%8d block device file%s\n"
		"%8d fifo%s\n"
		"%8d link%s\n"
		"%8d symbolic link%s (%d fast symbolic link%s)\n"
		"%8d socket%s\n"
		"--------\n"
		"%8d file%s\n",
		fs_regular_count, (fs_regular_count != 1) ? "s" : "",
		fs_directory_count, (fs_directory_count != 1) ? "ies" : "y",
		fs_chardev_count, (fs_chardev_count != 1) ? "s" : "",
		fs_blockdev_count, (fs_blockdev_count != 1) ? "s" : "",
		fs_fifo_count, (fs_fifo_count != 1) ? "s" : "",
		fs_links_count - dir_links,
		((fs_links_count - dir_links) != 1) ? "s" : "",
		fs_symlinks_count, (fs_symlinks_count != 1) ? "s" : "",
		fs_fast_symlinks_count, (fs_fast_symlinks_count != 1) ? "s" : "",
		fs_sockets_count, (fs_sockets_count != 1) ? "s" : "",
		fs_total_count - dir_links,
		((fs_total_count - dir_links) != 1) ? "s" : "");
}

static void check_mount(NOARGS)
{
	errcode_t	retval;
	int		mount_flags, cont, fd;

	retval = ext2fs_check_if_mounted(filesystem_name, &mount_flags);
	if (retval) {
		com_err("ext2fs_check_if_mount", retval,
			"while determining whether %s is mounted.",
			filesystem_name);
		return;
	}
	if (!(mount_flags & EXT2_MF_MOUNTED))
		return;

#if (defined(__linux__) && defined(HAVE_MNTENT_H))
	/*
	 * If the root is mounted read-only, then /etc/mtab is
	 * probably not correct; so we won't issue a warning based on
	 * it.
	 */
	fd = open(MOUNTED, O_RDWR);
	if (fd < 0) {
		if (errno == EROFS)
			return;
	} else
		close(fd);
#endif
	
	if (!rwflag) {
		printf("Warning!  %s is mounted.\n", device_name);
		return;
	}

	printf ("%s is mounted.  ", device_name);
	if (isatty (0) && isatty (1))
		cont = ask_yn("Do you really want to continue", -1);
	else
		cont = 0;
	if (!cont) {
		printf ("check aborted.\n");
		exit (0);
	}
	return;
}

static void sync_disks(NOARGS)
{
	sync();
	sync();
	sleep(1);
	sync();
}

static blk_t get_backup_sb(ext2_filsys fs)
{
	if (!fs || !fs->super)
		return 8193;
	return fs->super->s_blocks_per_group + 1;
}

#define MIN_CHECK 1
#define MAX_CHECK 2

static const char *corrupt_msg =
"\nThe superblock could not be read or does not describe a correct ext2\n"
"filesystem.  If the device is valid and it really contains an ext2\n"
"filesystem (and not swap or ufs or something else), then the superblock\n"
"is corrupt, and you might try running e2fsck with an alternate superblock:\n"
"    e2fsck -b %d <device>\n\n";

static void check_super_value(ext2_filsys fs, const char *descr,
			      unsigned long value, int flags,
			      unsigned long min, unsigned long max)
{
	if (((flags & MIN_CHECK) && (value < min)) ||
	    ((flags & MAX_CHECK) && (value > max))) {
		printf("Corruption found in superblock.  (%s = %lu).\n",
		       descr, value);
		printf(corrupt_msg, get_backup_sb(fs));
		fatal_error(0);
	}
}

static void relocate_hint(ext2_filsys fs)
{
	static hint_issued = 0;

	/*
	 * Only issue the hint once, and only if we're using the
	 * primary superblocks.
	 */
	if (hint_issued || superblock)
		return;

	printf("Note: if there is several inode or block bitmap blocks\n"
	       "which require relocation, or one part of the inode table\n"
	       "which must be moved, you may wish to try running e2fsck\n"
	       "with the '-b %d' option first.  The problem may lie only\n"
	       "with the primary block group descriptor, and the backup\n"
	       "block group descriptor may be OK.\n\n", get_backup_sb(fs));
	hint_issued = 1;
}

	
static void check_super_block(ext2_filsys fs)
{
	blk_t	first_block, last_block;
	struct ext2fs_sb *s = (struct ext2fs_sb *) fs->super;
	blk_t	blocks_per_group = fs->super->s_blocks_per_group;
	int	i;
	blk_t	should_be;
	errcode_t retval;
	struct problem_context	pctx;

	clear_problem_context(&pctx);

	/*
	 * Verify the super block constants...
	 */
	check_super_value(fs, "inodes_count", s->s_inodes_count,
			  MIN_CHECK, 1, 0);
	check_super_value(fs, "blocks_count", s->s_blocks_count,
			  MIN_CHECK, 1, 0);
	check_super_value(fs, "first_data_block", s->s_first_data_block,
			  MAX_CHECK, 0, s->s_blocks_count);
	check_super_value(fs, "log_frag_size", s->s_log_frag_size,
			  MAX_CHECK, 0, 2);
	check_super_value(fs, "log_block_size", s->s_log_block_size,
			  MIN_CHECK | MAX_CHECK, s->s_log_frag_size,
			  2);
	check_super_value(fs, "frags_per_group", s->s_frags_per_group,
			  MIN_CHECK | MAX_CHECK, 1, 8 * EXT2_BLOCK_SIZE(s));
	check_super_value(fs, "blocks_per_group", s->s_blocks_per_group,
			  MIN_CHECK | MAX_CHECK, 1, 8 * EXT2_BLOCK_SIZE(s));
	check_super_value(fs, "inodes_per_group", s->s_inodes_per_group,
			  MIN_CHECK, 1, 0);
	check_super_value(fs, "r_blocks_count", s->s_r_blocks_count,
			  MAX_CHECK, 0, s->s_blocks_count);

	retval = ext2fs_get_device_size(filesystem_name, EXT2_BLOCK_SIZE(s),
					&should_be);
	if (retval) {
		com_err("ext2fs_get_device_size", retval,
			"while trying to check physical size of filesystem");
		fatal_error(0);
	}
	if (should_be < s->s_blocks_count) {
		printf("The filesystem size (according to the superblock) is %d blocks\n", s->s_blocks_count);
		printf("The physical size of the device is %d blocks\n",
		       should_be);
		printf("Either the superblock or the partition table is likely to be corrupt!\n");
		preenhalt(fs);
		if (ask("Abort", 1))
			fatal_error(0);
	}

	if (s->s_log_block_size != s->s_log_frag_size) {
		printf("Superblock block_size = %d, fragsize = %d.\n",
		       EXT2_BLOCK_SIZE(s), EXT2_FRAG_SIZE(s));
		printf("This version of e2fsck does not support fragment "
		       "sizes different\n"
		       "from the block size.\n");
		fatal_error(0);
	}

	should_be = s->s_frags_per_group /
		(s->s_log_block_size - s->s_log_frag_size + 1);
	if (s->s_blocks_per_group != should_be) {
		printf("Superblock blocks_per_group = %u, should "
		       "have been %u\n", s->s_blocks_per_group,
		       should_be);
		printf(corrupt_msg, get_backup_sb(fs));
		fatal_error(0);
	}

	should_be = (s->s_log_block_size == 0) ? 1 : 0;
	if (s->s_first_data_block != should_be) {
		printf("Superblock first_data_block = %u, should "
		       "have been %u\n", s->s_first_data_block,
		       should_be);
		printf(corrupt_msg, get_backup_sb(fs));
		fatal_error(0);
	}

	/*
	 * Verify the group descriptors....
	 */
	first_block =  fs->super->s_first_data_block;
	last_block = first_block + blocks_per_group;

	for (i = 0; i < fs->group_desc_count; i++) {
		pctx.group = i;
		
		if (i == fs->group_desc_count - 1)
			last_block = fs->super->s_blocks_count;
		if ((fs->group_desc[i].bg_block_bitmap < first_block) ||
		    (fs->group_desc[i].bg_block_bitmap >= last_block)) {
			relocate_hint(fs);
			pctx.blk = fs->group_desc[i].bg_block_bitmap;
			if (fix_problem(fs, PR_0_BB_NOT_GROUP, &pctx)) {
				fs->group_desc[i].bg_block_bitmap = 0;
				invalid_block_bitmap[i]++;
				invalid_bitmaps++;
			}
		}
		if ((fs->group_desc[i].bg_inode_bitmap < first_block) ||
		    (fs->group_desc[i].bg_inode_bitmap >= last_block)) {
			relocate_hint(fs);
			pctx.blk = fs->group_desc[i].bg_inode_bitmap;
			if (fix_problem(fs, PR_0_IB_NOT_GROUP, &pctx)) {
				fs->group_desc[i].bg_inode_bitmap = 0;
				invalid_inode_bitmap[i]++;
				invalid_bitmaps++;
			}
		}
		if ((fs->group_desc[i].bg_inode_table < first_block) ||
		    ((fs->group_desc[i].bg_inode_table +
		      fs->inode_blocks_per_group - 1) >= last_block)) {
			relocate_hint(fs);
			pctx.blk = fs->group_desc[i].bg_inode_table;
			if (fix_problem(fs, PR_0_ITABLE_NOT_GROUP, &pctx)) {
				fs->group_desc[i].bg_inode_table = 0;
				invalid_inode_table[i]++;
				invalid_bitmaps++;
			}
		}
		first_block += fs->super->s_blocks_per_group;
		last_block += fs->super->s_blocks_per_group;
	}
	/*
	 * If we have invalid bitmaps, set the error state of the
	 * filesystem.
	 */
	if (invalid_bitmaps && rwflag) {
		fs->super->s_state &= ~EXT2_VALID_FS;
		ext2fs_mark_super_dirty(fs);
	}

	/*
	 * If the UUID field isn't assigned, assign it.
	 */
	if (rwflag && uuid_is_null(s->s_uuid)) {
		if (preen)
			printf("%s: Adding UUID to filesystem.\n",
			       device_name);
		else
			printf("Filesystem did not have a UUID; "
			       "generating one.\n\n");
		uuid_generate(s->s_uuid);
		ext2fs_mark_super_dirty(fs);
	}
	return;
}

/*
 * This routine checks to see if a filesystem can be skipped; if so,
 * it will exit with E2FSCK_OK.  Under some conditions it will print a
 * message explaining why a check is being forced.
 */
static void check_if_skip(ext2_filsys fs)
{
	const char *reason = NULL;
	
	if (force || bad_blocks_file || cflag || swapfs)
		return;
	
	if (fs->super->s_state & EXT2_ERROR_FS)
		reason = "contains a file system with errors";
	else if (fs->super->s_mnt_count >=
		 (unsigned) fs->super->s_max_mnt_count)
		reason = "has reached maximal mount count";
	else if (fs->super->s_checkinterval &&
		 time(0) >= (fs->super->s_lastcheck +
			     fs->super->s_checkinterval))
		reason = "has gone too long without being checked";
	else if ((fs->super->s_state & EXT2_VALID_FS) == 0)
		reason = "was not cleanly unmounted";
	if (reason) {
		printf("%s %s, check forced.\n", device_name, reason);
		return;
	}
	printf("%s: clean, %d/%d files, %d/%d blocks\n", device_name,
	       fs->super->s_inodes_count - fs->super->s_free_inodes_count,
	       fs->super->s_inodes_count,
	       fs->super->s_blocks_count - fs->super->s_free_blocks_count,
	       fs->super->s_blocks_count);
	ext2fs_close(fs);
	exit(FSCK_OK);
}	

#define PATH_SET "PATH=/sbin"

static void PRS(int argc, char *argv[])
{
	int		flush = 0;
	char		c;
#ifdef MTRACE
	extern void	*mallwatch;
#endif
	char		*oldpath = getenv("PATH");

	/* Update our PATH to include /sbin  */
	if (oldpath) {
		char *newpath;

		newpath = malloc(sizeof (PATH_SET) + 1 + strlen (oldpath));
		if (!newpath)
			fatal_error("Couldn't malloc() newpath");
		strcpy (newpath, PATH_SET);
		strcat (newpath, ":");
		strcat (newpath, oldpath);
		putenv (newpath);
	} else
		putenv (PATH_SET);

	setbuf(stdout, NULL);
	setbuf(stderr, NULL);
	initialize_ext2_error_table();
	
	if (argc && *argv)
		program_name = *argv;
	while ((c = getopt (argc, argv, "panyrcB:dfvtFVM:b:I:P:l:L:N:Ss")) != EOF)
		switch (c) {
		case 'p':
		case 'a':
			preen = 1;
			yflag = nflag = 0;
			break;
		case 'n':
			nflag = 1;
			preen = yflag = 0;
			break;
		case 'y':
			yflag = 1;
			preen = nflag = 0;
			break;
		case 't':
			tflag++;
			break;
		case 'c':
			cflag++;
			break;
		case 'r':
			/* What we do by default, anyway! */
			break;
		case 'b':
			use_superblock = atoi(optarg);
			break;
		case 'B':
			blocksize = atoi(optarg);
			break;
		case 'I':
			inode_buffer_blocks = atoi(optarg);
			break;
		case 'P':
			process_inode_size = atoi(optarg);
			break;
		case 'L':
			replace_bad_blocks++;
		case 'l':
			bad_blocks_file = malloc(strlen(optarg)+1);
			if (!bad_blocks_file)
				fatal_error("Couldn't malloc bad_blocks_file");
			strcpy(bad_blocks_file, optarg);
			break;
		case 'd':
			debug = 1;
			break;
		case 'f':
			force = 1;
			break;
		case 'F':
#ifdef BLKFLSBUF
			flush = 1;
#else
			fatal_error ("-F not supported");
#endif
			break;
		case 'v':
			verbose = 1;
			break;
		case 'V':
			show_version_only = 1;
			break;
#ifdef MTRACE
		case 'M':
			mallwatch = (void *) strtol(optarg, NULL, 0);
			break;
#endif
		case 'N':
			device_name = optarg;
			break;
		case 's':
			normalize_swapfs = 1;
		case 'S':
			swapfs = 1;
			break;
		default:
			usage ();
		}
	if (show_version_only)
		return;
	if (optind != argc - 1)
		usage ();
	if (nflag && !bad_blocks_file && !cflag && !swapfs)
		rwflag = 0;
	filesystem_name = argv[optind];
	if (device_name == 0)
		device_name = filesystem_name;
	if (flush) {
#ifdef BLKFLSBUF
		int	fd = open(filesystem_name, O_RDONLY, 0);

		if (fd < 0) {
			com_err("open", errno, "while opening %s for flushing",
				filesystem_name);
			exit(FSCK_ERROR);
		}
		if (ioctl(fd, BLKFLSBUF, 0) < 0) {
			com_err("BLKFLSBUF", errno, "while trying to flush %s",
				filesystem_name);
			exit(FSCK_ERROR);
		}
		close(fd);
#else
		fatal_error ("BLKFLSBUF not supported");
#endif /* BLKFLSBUF */
	}
	if (swapfs) {
		if (cflag || bad_blocks_file) {
			fprintf(stderr, "Incompatible options not "
				"allowed when byte-swapping.\n");
			fatal_error(0);
		}
	}
}

static const char *my_ver_string = E2FSPROGS_VERSION;
static const char *my_ver_date = E2FSPROGS_DATE;
					
int main (int argc, char *argv[])
{
	errcode_t	retval = 0;
	int		exit_value = FSCK_OK;
	int		i;
	ext2_filsys	fs = 0;
	io_manager	io_ptr;
	struct ext2fs_sb *s;
	const char	*lib_ver_date;
	int		my_ver, lib_ver;
	
#ifdef MTRACE
	mtrace();
#endif
#ifdef MCHECK
	mcheck(0);
#endif
	my_ver = ext2fs_parse_version_string(my_ver_string);
	lib_ver = ext2fs_get_library_version(0, &lib_ver_date);
	if (my_ver > lib_ver) {
		fprintf( stderr, "Error: ext2fs library version "
			"out of date!\n");
		show_version_only++;
	}
	
	init_resource_track(&global_rtrack);

	PRS(argc, argv);

	if (!preen || show_version_only)
		fprintf (stderr, "e2fsck %s, %s for EXT2 FS %s, %s\n",
			 my_ver_string, my_ver_date, EXT2FS_VERSION,
			 EXT2FS_DATE);

	if (show_version_only) {
		fprintf(stderr, "\tUsing %s, %s\n",
			error_message(EXT2_ET_BASE), lib_ver_date);
		exit(0);
	}
	
	check_mount();
	
	if (!preen && !nflag && !yflag) {
		if (!isatty (0) || !isatty (1))
			die ("need terminal for interactive repairs");
	}
	superblock = use_superblock;
restart:
#if 1
	io_ptr = unix_io_manager;
#else
	io_ptr = test_io_manager;
	test_io_backing_manager = unix_io_manager;
#endif
	sync_disks();
	if (superblock && blocksize) {
		retval = ext2fs_open(filesystem_name,
				     rwflag ? EXT2_FLAG_RW : 0,
				     superblock, blocksize, io_ptr, &fs);
	} else if (superblock) {
		for (i=0; possible_block_sizes[i]; i++) {
			retval = ext2fs_open(filesystem_name,
					     rwflag ? EXT2_FLAG_RW : 0,
					     superblock,
					     possible_block_sizes[i],
					     io_ptr, &fs);
			if (!retval)
				break;
		}
	} else 
		retval = ext2fs_open(filesystem_name,
				     rwflag ? EXT2_FLAG_RW : 0,
				     0, 0, io_ptr, &fs);
	if (!superblock && !preen && 
	    ((retval == EXT2_ET_BAD_MAGIC) ||
	     ((retval == 0) && ext2fs_check_desc(fs)))) {
		if (!fs || (fs->group_desc_count > 1)) {
			printf("%s trying backup blocks...\n",
			       retval ? "Couldn't find ext2 superblock," :
			       "Group descriptors look bad...");
			superblock = get_backup_sb(fs);
			if (fs)
				ext2fs_close(fs);
			goto restart;
		}
	}
	if (retval) {
		com_err(program_name, retval, "while trying to open %s",
			filesystem_name);
		if (retval == EXT2_ET_REV_TOO_HIGH)
			printf ("Get a newer version of e2fsck!\n");
		else if (retval == EXT2_ET_SHORT_READ)
			printf ("Could this be a zero-length partition?\n");
		else if ((retval == EPERM) || (retval == EACCES))
			printf("You must have %s access to the "
			       "filesystem or be root\n",
			       rwflag ? "r/w" : "r/o");
		else if (retval == ENXIO)
			printf("Possibly non-existent or swap device?\n");
		else
			printf(corrupt_msg, get_backup_sb(fs));
		fatal_error(0);
	}
#ifdef	EXT2_CURRENT_REV
	if (fs->super->s_rev_level > E2FSCK_CURRENT_REV) {
		com_err(program_name, EXT2_ET_REV_TOO_HIGH,
			"while trying to open %s",
			filesystem_name);
		goto get_newer;
	}
#endif
	/*
	 * Check for compatibility with the feature sets.  We need to
	 * be more stringent than ext2fs_open().
	 */
	s = (struct ext2fs_sb *) fs->super;
	if ((s->s_feature_compat & ~EXT2_LIB_FEATURE_COMPAT_SUPP) ||
	    (s->s_feature_incompat & ~EXT2_LIB_FEATURE_INCOMPAT_SUPP)) {
		com_err(program_name, EXT2_ET_UNSUPP_FEATURE,
			"(%s)", filesystem_name);
	get_newer:
		printf ("Get a newer version of e2fsck!\n");
		fatal_error(0);
	}
	if (s->s_feature_ro_compat & ~EXT2_LIB_FEATURE_RO_COMPAT_SUPP) {
		com_err(program_name, EXT2_ET_RO_UNSUPP_FEATURE,
			"(%s)", filesystem_name);
		goto get_newer;
	}
	
	/*
	 * If the user specified a specific superblock, presumably the
	 * master superblock has been trashed.  So we mark the
	 * superblock as dirty, so it can be written out.
	 */
	if (superblock && rwflag)
		ext2fs_mark_super_dirty(fs);

	/*
	 * Don't overwrite the backup superblock and block
	 * descriptors, until we're sure the filesystem is OK....
	 */
	fs->flags |= EXT2_FLAG_MASTER_SB_ONLY;

	ehandler_init(fs->io);

	invalid_inode_bitmap = allocate_memory(sizeof(int) *
					       fs->group_desc_count,
					       "invalid_inode_bitmap");
	invalid_block_bitmap = allocate_memory(sizeof(int) *
					       fs->group_desc_count,
					       "invalid_block_bitmap");
	invalid_inode_table = allocate_memory(sizeof(int) *
					      fs->group_desc_count,
					      "invalid_inode_table");
		
	check_super_block(fs);
	check_if_skip(fs);
	if (bad_blocks_file)
		read_bad_blocks_file(fs, bad_blocks_file, replace_bad_blocks);
	else if (cflag)
		test_disk(fs);

	if (normalize_swapfs) {
		if ((fs->flags & EXT2_FLAG_SWAP_BYTES) ==
		    ext2fs_native_flag()) {
			fprintf(stderr, "%s: Filesystem byte order "
				"already normalized.\n", device_name);
			fatal_error(0);
		}
	}
	if (swapfs)
		swap_filesys(fs);

	/*
	 * Mark the system as valid, 'til proven otherwise
	 */
	ext2fs_mark_valid(fs);

	retval = ext2fs_read_bb_inode(fs, &fs->badblocks);
	if (retval) {
		com_err(program_name, retval,
			"while reading bad blocks inode");
		preenhalt(fs);
		printf("This doesn't bode well, but we'll try to go on...\n");
	}
	
	pass1(fs);
	free(invalid_inode_bitmap);
	free(invalid_block_bitmap);
	free(invalid_inode_table);
	if (restart_e2fsck) {
		ext2fs_close(fs);
		printf("Restarting e2fsck from the beginning...\n");
		restart_e2fsck = 0;
		superblock = use_superblock;
		goto restart;
	}
	pass2(fs);
	pass3(fs);
	pass4(fs);
	pass5(fs);

#ifdef MTRACE
	mtrace_print("Cleanup");
#endif
	if (ext2fs_test_changed(fs)) {
		exit_value = FSCK_NONDESTRUCT;
		if (!preen)
			printf("\n%s: ***** FILE SYSTEM WAS MODIFIED *****\n",
			       device_name);
		if (root_filesystem && !read_only_root) {
			printf("%s: ***** REBOOT LINUX *****\n", device_name);
			exit_value = FSCK_REBOOT;
		}
	}
	if (ext2fs_test_valid(fs))
		fs->flags &= ~EXT2_FLAG_MASTER_SB_ONLY;
	else
		exit_value = FSCK_UNCORRECTED;
	if (rwflag) {
		if (ext2fs_test_valid(fs)) {
			if (!(fs->super->s_state & EXT2_VALID_FS))
				exit_value = FSCK_NONDESTRUCT;
			fs->super->s_state = EXT2_VALID_FS;
		} else
			fs->super->s_state &= ~EXT2_VALID_FS;
		fs->super->s_mnt_count = 0;
		fs->super->s_lastcheck = time(NULL);
		ext2fs_mark_super_dirty(fs);
	}
	show_stats(fs);

	write_bitmaps(fs);
	ext2fs_close(fs);
	sync_disks();
	
	if (tflag)
		print_resource_track(&global_rtrack);
	
	return exit_value;
}
