/*
 * dump.c --- dump the contents of an inode out to a file
 * 
 * Copyright (C) 1994 Theodore Ts'o.  This file may be redistributed
 * under the terms of the GNU Public License.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <utime.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else 
extern int optind;
extern char *optarg;
#endif
#ifdef HAVE_OPTRESET
extern int optreset;		/* defined by BSD, but not others */
#endif

#include "debugfs.h"

/*
 * The mode_xlate function translates a linux mode into a native-OS mode_t.
 */
static struct {
	__u16 lmask;
	mode_t mask;
} mode_table[] = {
	{ LINUX_S_IRUSR, S_IRUSR },
	{ LINUX_S_IWUSR, S_IWUSR },
	{ LINUX_S_IXUSR, S_IXUSR },
	{ LINUX_S_IRGRP, S_IRGRP },
	{ LINUX_S_IWGRP, S_IWGRP },
	{ LINUX_S_IXGRP, S_IXGRP },
	{ LINUX_S_IROTH, S_IROTH },
	{ LINUX_S_IWOTH, S_IWOTH },
	{ LINUX_S_IXOTH, S_IXOTH },
	{ 0, 0 }
};
 
static mode_t mode_xlate(__u16 lmode)
{
	mode_t	mode = 0;
	int	i;

	for (i=0; mode_table[i].lmask; i++) {
		if (lmode & mode_table[i].lmask)
			mode |= mode_table[i].mask;
	}
	return mode;
}

struct dump_block_struct {
	int		fd;
	char		*buf;
	int		left;
	errcode_t	errcode;
};

static int dump_block(ext2_filsys fs, blk_t *blocknr, int blockcnt,
		      void *private)
{
	int nbytes, left;
	off_t	ret_off;
	
	struct dump_block_struct *rec = (struct dump_block_struct *) private;
	
	if (blockcnt < 0)
		return 0;

	if (*blocknr) {
		rec->errcode = io_channel_read_blk(fs->io, *blocknr,
						   1, rec->buf);
		if (rec->errcode)
			return BLOCK_ABORT;
	} else {
		/*
		 * OK, the file has a hole.  Let's try to seek past
		 * the hole in the destination file, so that the
		 * destination file has a hole too.
		 */
		ret_off = lseek(rec->fd, fs->blocksize, SEEK_CUR);
		if (ret_off >= 0)
			return 0;
		memset(rec->buf, 0, fs->blocksize);
	}

	left = (rec->left > fs->blocksize) ? fs->blocksize : rec->left;
	rec->left -= left;
	
	while (left > 0) {
		nbytes = write(rec->fd, rec->buf, left);
		if (nbytes == -1) {
			if (errno == EINTR)
				continue;
			rec->errcode = errno;
			return BLOCK_ABORT;
		}
		left -= nbytes;
	}
	if (rec->left <= 0)
		return BLOCK_ABORT;
	return 0;
}

static void dump_file(char *cmdname, ino_t ino, int fd, int preserve,
		      char *outname)
{
	errcode_t retval;
	struct dump_block_struct rec;
	struct ext2_inode	inode;
	struct utimbuf	ut;

	retval = ext2fs_read_inode(current_fs, ino, &inode);
	if (retval) {
		com_err(cmdname, retval,
			"while reading inode %u in dump_file", ino);
		return;
	}

	rec.fd = fd;
	rec.errcode = 0;
	rec.buf = malloc(current_fs->blocksize);
	rec.left = inode.i_size;

	if (rec.buf == 0) {
		com_err(cmdname, ENOMEM,
			"while allocating block buffer for dump_inode");
		return;
	}
	
	retval = ext2fs_block_iterate(current_fs, ino,
				      BLOCK_FLAG_HOLE|BLOCK_FLAG_DATA_ONLY,
				      NULL, dump_block, &rec);
	if (retval) {
		com_err(cmdname, retval, "while iterating over blocks in %s",
			outname);
		goto cleanup;
	}
	if (rec.errcode) {
		com_err(cmdname, retval, "in dump_block while dumping %s",
			outname);
		goto cleanup;
	}
	
cleanup:
	if (preserve) {
#ifdef HAVE_FCHOWN
		if (fchown(fd, inode.i_uid, inode.i_gid) < 0)
			com_err("dump_file", errno,
				"while changing ownership of %s", outname);
#else
		if (chown(outname, inode.i_uid, inode.i_gid) < 0)
			com_err("dump_file", errno,
				"while changing ownership of %s", outname);
			
#endif
		if (fchmod(fd, mode_xlate(inode.i_mode)) < 0)
			com_err("dump_file", errno,
				"while setting permissions of %s", outname);
		ut.actime = inode.i_atime;
		ut.modtime = inode.i_mtime;
		close(fd);
		if (utime(outname, &ut) < 0)
			com_err("dump_file", errno,
				"while setting times on %s", outname);
	} else if (fd != 1)
		close(fd);
				    
	free(rec.buf);
	return;
}

void do_dump(int argc, char **argv)
{
	ino_t	inode;
	int	fd;
	char	c;
	int	preserve = 0;
	const char *dump_usage = "Usage: dump_inode [-p] <file> <output_file>";
	char	*in_fn, *out_fn;
	
	optind = 0;
#ifdef HAVE_OPTRESET
	optreset = 1;		/* Makes BSD getopt happy */
#endif
	while ((c = getopt (argc, argv, "p")) != EOF) {
		switch (c) {
		case 'p':
			preserve++;
			break;
		default:
			com_err(argv[0], 0, dump_usage);
			return;
		}
	}
	if (optind != argc-2) {
		com_err(argv[0], 0, dump_usage);
		return;
	}

	if (check_fs_open(argv[0]))
		return;

	in_fn = argv[optind];
	out_fn = argv[optind+1];

	inode = string_to_inode(in_fn);
	if (!inode) 
		return;

	fd = open(out_fn, O_CREAT | O_WRONLY | O_TRUNC, 0666);
	if (fd < 0) {
		com_err(argv[0], errno, "while opening %s for dump_inode",
			out_fn);
		return;
	}

	dump_file(argv[0], inode, fd, preserve, out_fn);

	return;
}

void do_cat(int argc, char **argv)
{
	ino_t	inode;

	if (argc != 2) {
		com_err(argv[0], 0, "Usage: cat <file>");
		return;
	}

	if (check_fs_open(argv[0]))
		return;

	inode = string_to_inode(argv[1]);
	if (!inode) 
		return;

	fflush(stdout);
	fflush(stderr);
	dump_file(argv[0], inode, 1, 0, argv[2]); 

	return;
}

