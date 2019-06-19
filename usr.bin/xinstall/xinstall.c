/*	$OpenBSD: xinstall.c,v 1.72 2019/05/09 22:44:53 guenther Exp $	*/
/*	$NetBSD: xinstall.c,v 1.9 1995/12/20 10:25:17 jonathan Exp $	*/

/*
 * Copyright (c) 1987, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>	/* MAXBSIZE */
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <paths.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <libgen.h>

#include "pathnames.h"

#define MINIMUM(a, b)	(((a) < (b)) ? (a) : (b))

#define	DIRECTORY	0x01		/* Tell install it's a directory. */
#define	SETFLAGS	0x02		/* Tell install to set flags. */
#define	USEFSYNC	0x04		/* Tell install to use fsync(2). */
#define NOCHANGEBITS	(UF_IMMUTABLE | UF_APPEND | SF_IMMUTABLE | SF_APPEND)
#define BACKUP_SUFFIX	".old"

int dobackup, docompare, dodest, dodir, dopreserve, dostrip;
int mode = S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH;
char pathbuf[PATH_MAX], tempfile[PATH_MAX];
char *suffix = BACKUP_SUFFIX;
uid_t uid = (uid_t)-1;
gid_t gid = (gid_t)-1;

void	copy(int, char *, int, char *, off_t, int);
int	compare(int, const char *, off_t, int, const char *, off_t);
void	install(char *, char *, u_long, u_int);
void	install_dir(char *, int);
void	strip(char *);
void	usage(void);
int	create_tempfile(char *, char *, size_t);
int	file_write(int, char *, size_t, int *, int *, int);
void	file_flush(int, int);

int
main(int argc, char *argv[])
{
	struct stat from_sb, to_sb;
	void *set;
	u_int32_t fset;
	u_int iflags;
	int ch, no_target;
	char *flags, *to_name, *group = NULL, *owner = NULL;
	const char *errstr;

	iflags = 0;
	while ((ch = getopt(argc, argv, "B:bCcDdFf:g:m:o:pSs")) != -1)
		switch(ch) {
		case 'C':
			docompare = 1;
			break;
		case 'B':
			suffix = optarg;
			/* fall through; -B implies -b */
		case 'b':
			dobackup = 1;
			break;
		case 'c':
			/* For backwards compatibility. */
			break;
		case 'F':
			iflags |= USEFSYNC;
			break;
		case 'f':
			flags = optarg;
			if (strtofflags(&flags, &fset, NULL))
				errx(1, "%s: invalid flag", flags);
			iflags |= SETFLAGS;
			break;
		case 'g':
			group = optarg;
			break;
		case 'm':
			if (!(set = setmode(optarg)))
				errx(1, "%s: invalid file mode", optarg);
			mode = getmode(set, 0);
			free(set);
			break;
		case 'o':
			owner = optarg;
			break;
		case 'p':
			docompare = dopreserve = 1;
			break;
		case 'S':
			/* For backwards compatibility. */
			break;
		case 's':
			dostrip = 1;
			break;
		case 'D':
			dodest = 1;
			break;
		case 'd':
			dodir = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	/* some options make no sense when creating directories */
	if ((docompare || dostrip) && dodir)
		usage();

	/* must have at least two arguments, except when creating directories */
	if (argc < 2 && !dodir)
		usage();

	/* get group and owner id's */
	if (group != NULL && gid_from_group(group, &gid) == -1) {
		gid = strtonum(group, 0, GID_MAX, &errstr);
		if (errstr != NULL)
			errx(1, "unknown group %s", group);
	}
	if (owner != NULL && uid_from_user(owner, &uid) == -1) {
		uid = strtonum(owner, 0, UID_MAX, &errstr);
		if (errstr != NULL)
			errx(1, "unknown user %s", owner);
	}

	if (dodir) {
		for (; *argv != NULL; ++argv)
			install_dir(*argv, mode);
		exit(0);
		/* NOTREACHED */
	}

	if (dodest) {
		char *dest = dirname(argv[argc - 1]);
		if (dest == NULL)
			errx(1, "cannot determine dirname");
		/*
		 * When -D is passed, do not chmod the directory with the mode set for
		 * the target file. If more restrictive permissions are required then
		 * '-d -m' ought to be used instead.
		 */
		install_dir(dest, 0755);
	}

	no_target = stat(to_name = argv[argc - 1], &to_sb);
	if (!no_target && S_ISDIR(to_sb.st_mode)) {
		for (; *argv != to_name; ++argv)
			install(*argv, to_name, fset, iflags | DIRECTORY);
		exit(0);
		/* NOTREACHED */
	}

	/* can't do file1 file2 directory/file */
	if (argc != 2)
		errx(1, "Target: %s", argv[argc-1]);

	if (!no_target) {
		if (stat(*argv, &from_sb))
			err(1, "%s", *argv);
		if (!S_ISREG(to_sb.st_mode))
			errc(1, EFTYPE, "%s", to_name);
		if (to_sb.st_dev == from_sb.st_dev &&
		    to_sb.st_ino == from_sb.st_ino)
			errx(1, "%s and %s are the same file", *argv, to_name);
	}
	install(*argv, to_name, fset, iflags);
	exit(0);
	/* NOTREACHED */
}

/*
 * install --
 *	build a path name and install the file
 */
void
install(char *from_name, char *to_name, u_long fset, u_int flags)
{
	struct stat from_sb, to_sb;
	struct timespec ts[2];
	int devnull, from_fd, to_fd, serrno, files_match = 0;
	char *p;
	char *target_name = tempfile;

	(void)memset((void *)&from_sb, 0, sizeof(from_sb));
	(void)memset((void *)&to_sb, 0, sizeof(to_sb));

	/* If try to install NULL file to a directory, fails. */
	if (flags & DIRECTORY || strcmp(from_name, _PATH_DEVNULL)) {
		if (stat(from_name, &from_sb))
			err(1, "%s", from_name);
		if (!S_ISREG(from_sb.st_mode))
			errc(1, EFTYPE, "%s", from_name);
		/* Build the target path. */
		if (flags & DIRECTORY) {
			(void)snprintf(pathbuf, sizeof(pathbuf), "%s/%s",
			    to_name,
			    (p = strrchr(from_name, '/')) ? ++p : from_name);
			to_name = pathbuf;
		}
		devnull = 0;
	} else {
		devnull = 1;
	}

	if (stat(to_name, &to_sb) == 0) {
		/* Only compare against regular files. */
		if (docompare && !S_ISREG(to_sb.st_mode)) {
			docompare = 0;
			warnc(EFTYPE, "%s", to_name);
		}
	} else if (docompare) {
		/* File does not exist so silently ignore compare flag. */
		docompare = 0;
	}

	if (!devnull) {
		if ((from_fd = open(from_name, O_RDONLY, 0)) < 0)
			err(1, "%s", from_name);
	}

	to_fd = create_tempfile(to_name, tempfile, sizeof(tempfile));
	if (to_fd < 0)
		err(1, "%s", tempfile);

	if (!devnull)
		copy(from_fd, from_name, to_fd, tempfile, from_sb.st_size,
		    ((off_t)from_sb.st_blocks * S_BLKSIZE < from_sb.st_size));

	if (dostrip) {
		strip(tempfile);

		/*
		 * Re-open our fd on the target, in case we used a strip
		 *  that does not work in-place -- like gnu binutils strip.
		 */
		close(to_fd);
		if ((to_fd = open(tempfile, O_RDONLY, 0)) < 0)
			err(1, "stripping %s", to_name);
	}

	/*
	 * Compare the (possibly stripped) temp file to the target.
	 */
	if (docompare) {
		int temp_fd = to_fd;
		struct stat temp_sb;

		/* Re-open to_fd using the real target name. */
		if ((to_fd = open(to_name, O_RDONLY, 0)) < 0)
			err(1, "%s", to_name);

		if (fstat(temp_fd, &temp_sb)) {
			serrno = errno;
			(void)unlink(tempfile);
			errc(1, serrno, "%s", tempfile);
		}

		if (compare(temp_fd, tempfile, temp_sb.st_size, to_fd,
			    to_name, to_sb.st_size) == 0) {
			/*
			 * If target has more than one link we need to
			 * replace it in order to snap the extra links.
			 * Need to preserve target file times, though.
			 */
			if (to_sb.st_nlink != 1) {
				ts[0] = to_sb.st_atim;
				ts[1] = to_sb.st_mtim;
				futimens(temp_fd, ts);
			} else {
				files_match = 1;
				(void)unlink(tempfile);
				target_name = to_name;
				(void)close(temp_fd);
			}
		}
		if (!files_match) {
			(void)close(to_fd);
			to_fd = temp_fd;
		}
	}

	/*
	 * Preserve the timestamp of the source file if necessary.
	 */
	if (dopreserve && !files_match) {
		ts[0] = from_sb.st_atim;
		ts[1] = from_sb.st_mtim;
		futimens(to_fd, ts);
	}

	/*
	 * Set owner, group, mode for target; do the chown first,
	 * chown may lose the setuid bits.
	 */
	if ((gid != (gid_t)-1 || uid != (uid_t)-1) &&
	    fchown(to_fd, uid, gid)) {
		serrno = errno;
		if (target_name == tempfile)
			(void)unlink(target_name);
		errx(1, "%s: chown/chgrp: %s", target_name, strerror(serrno));
	}
	if (fchmod(to_fd, mode)) {
		serrno = errno;
		if (target_name == tempfile)
			(void)unlink(target_name);
		errx(1, "%s: chmod: %s", target_name, strerror(serrno));
	}

	/*
	 * If provided a set of flags, set them, otherwise, preserve the
	 * flags, except for the dump flag.
	 */
	if (fchflags(to_fd,
	    flags & SETFLAGS ? fset : from_sb.st_flags & ~UF_NODUMP)) {
		if (errno != EOPNOTSUPP || (from_sb.st_flags & ~UF_NODUMP) != 0)
			warnx("%s: chflags: %s", target_name, strerror(errno));
	}

	if (flags & USEFSYNC)
		fsync(to_fd);
	(void)close(to_fd);
	if (!devnull)
		(void)close(from_fd);

	/*
	 * Move the new file into place if the files are different
	 * or were not compared.
	 */
	if (!files_match) {
		/* Try to turn off the immutable bits. */
		if (to_sb.st_flags & (NOCHANGEBITS))
			(void)chflags(to_name, to_sb.st_flags & ~(NOCHANGEBITS));
		if (dobackup) {
			char backup[PATH_MAX];
			(void)snprintf(backup, PATH_MAX, "%s%s", to_name,
			    suffix);
			/* It is ok for the target file not to exist. */
			if (rename(to_name, backup) < 0 && errno != ENOENT) {
				serrno = errno;
				unlink(tempfile);
				errx(1, "rename: %s to %s: %s", to_name,
				     backup, strerror(serrno));
			}
		}
		if (rename(tempfile, to_name) < 0 ) {
			serrno = errno;
			unlink(tempfile);
			errx(1, "rename: %s to %s: %s", tempfile,
			     to_name, strerror(serrno));
		}
	}
}

/*
 * copy --
 *	copy from one file to another
 */
void
copy(int from_fd, char *from_name, int to_fd, char *to_name, off_t size,
    int sparse)
{
	ssize_t nr, nw;
	int serrno;
	char *p, buf[MAXBSIZE];

	if (size == 0)
		return;

	/* Rewind file descriptors. */
	if (lseek(from_fd, (off_t)0, SEEK_SET) == (off_t)-1)
		err(1, "lseek: %s", from_name);
	if (lseek(to_fd, (off_t)0, SEEK_SET) == (off_t)-1)
		err(1, "lseek: %s", to_name);

	/*
	 * Mmap and write if less than 8M (the limit is so we don't totally
	 * trash memory on big files.  This is really a minor hack, but it
	 * wins some CPU back.  Sparse files need special treatment.
	 */
	if (!sparse && size <= 8 * 1048576) {
		size_t siz;

		if ((p = mmap(NULL, (size_t)size, PROT_READ, MAP_PRIVATE,
		    from_fd, (off_t)0)) == MAP_FAILED) {
			serrno = errno;
			(void)unlink(to_name);
			errc(1, serrno, "%s", from_name);
		}
		madvise(p, size, MADV_SEQUENTIAL);
		siz = (size_t)size;
		if ((nw = write(to_fd, p, siz)) != siz) {
			serrno = errno;
			(void)unlink(to_name);
			errx(1, "%s: %s",
			    to_name, strerror(nw > 0 ? EIO : serrno));
		}
		(void) munmap(p, (size_t)size);
	} else {
		int sz, rem, isem = 1;
		struct stat sb;

		/*
		 * Pass the blocksize of the file being written to the write
		 * routine.  if the size is zero, use the default S_BLKSIZE.
		 */
		if (fstat(to_fd, &sb) != 0 || sb.st_blksize == 0)
			sz = S_BLKSIZE;
		else
			sz = sb.st_blksize;
		rem = sz;

		while ((nr = read(from_fd, buf, sizeof(buf))) > 0) {
			if (sparse)
				nw = file_write(to_fd, buf, nr, &rem, &isem, sz);
			else
				nw = write(to_fd, buf, nr);
			if (nw != nr) {
				serrno = errno;
				(void)unlink(to_name);
				errx(1, "%s: %s",
				    to_name, strerror(nw > 0 ? EIO : serrno));
			}
		}
		if (sparse)
			file_flush(to_fd, isem);
		if (nr != 0) {
			serrno = errno;
			(void)unlink(to_name);
			errc(1, serrno, "%s", from_name);
		}
	}
}

/*
 * compare --
 *	compare two files; non-zero means files differ
 */
int
compare(int from_fd, const char *from_name, off_t from_len, int to_fd,
    const char *to_name, off_t to_len)
{
	caddr_t p1, p2;
	size_t length;
	off_t from_off, to_off, remainder;
	int dfound;

	if (from_len == 0 && from_len == to_len)
		return (0);

	if (from_len != to_len)
		return (1);

	/*
	 * Compare the two files being careful not to mmap
	 * more than 8M at a time.
	 */
	from_off = to_off = (off_t)0;
	remainder = from_len;
	do {
		length = MINIMUM(remainder, 8 * 1048576);
		remainder -= length;

		if ((p1 = mmap(NULL, length, PROT_READ, MAP_PRIVATE,
		    from_fd, from_off)) == MAP_FAILED)
			err(1, "%s", from_name);
		if ((p2 = mmap(NULL, length, PROT_READ, MAP_PRIVATE,
		    to_fd, to_off)) == MAP_FAILED)
			err(1, "%s", to_name);
		if (length) {
			madvise(p1, length, MADV_SEQUENTIAL);
			madvise(p2, length, MADV_SEQUENTIAL);
		}

		dfound = memcmp(p1, p2, length);

		(void) munmap(p1, length);
		(void) munmap(p2, length);

		from_off += length;
		to_off += length;

	} while (!dfound && remainder > 0);

	return(dfound);
}

/*
 * strip --
 *	use strip(1) to strip the target file
 */
void
strip(char *to_name)
{
	int serrno, status;
	char * volatile path_strip;
	pid_t pid;

	if (issetugid() || (path_strip = getenv("STRIP")) == NULL)
		path_strip = _PATH_STRIP;

	switch ((pid = vfork())) {
	case -1:
		serrno = errno;
		(void)unlink(to_name);
		errc(1, serrno, "forks");
	case 0:
		execl(path_strip, "strip", "--", to_name, (char *)NULL);
		warn("%s", path_strip);
		_exit(1);
	default:
		while (waitpid(pid, &status, 0) == -1) {
			if (errno != EINTR)
				break;
		}
		if (!WIFEXITED(status))
			(void)unlink(to_name);
	}
}

/*
 * install_dir --
 *	build directory hierarchy
 */
void
install_dir(char *path, int mode)
{
	char *p;
	struct stat sb;
	int ch;

	for (p = path;; ++p)
		if (!*p || (p != path && *p  == '/')) {
			ch = *p;
			*p = '\0';
			if (mkdir(path, 0777)) {
				int mkdir_errno = errno;
				if (stat(path, &sb)) {
					/* Not there; use mkdir()s errno */
					errc(1, mkdir_errno, "%s",
					    path);
					/* NOTREACHED */
				}
				if (!S_ISDIR(sb.st_mode)) {
					/* Is there, but isn't a directory */
					errc(1, ENOTDIR, "%s", path);
					/* NOTREACHED */
				}
			}
			if (!(*p = ch))
				break;
 		}

	if (((gid != (gid_t)-1 || uid != (uid_t)-1) && chown(path, uid, gid)) ||
	    chmod(path, mode)) {
		warn("%s", path);
	}
}

/*
 * usage --
 *	print a usage message and die
 */
void
usage(void)
{
	(void)fprintf(stderr, "\
usage: install [-bCcDdFpSs] [-B suffix] [-f flags] [-g group] [-m mode] [-o owner]\n	       source ... target ...\n");
	exit(1);
	/* NOTREACHED */
}

/*
 * create_tempfile --
 *	create a temporary file based on path and open it
 */
int
create_tempfile(char *path, char *temp, size_t tsize)
{
	char *p;

	strlcpy(temp, path, tsize);
	if ((p = strrchr(temp, '/')) != NULL)
		p++;
	else
		p = temp;
	*p = '\0';
	strlcat(p, "INS@XXXXXXXXXX", tsize);

	return(mkstemp(temp));
}

/*
 * file_write()
 *	Write/copy a file (during copy or archive extract). This routine knows
 *	how to copy files with lseek holes in it. (Which are read as file
 *	blocks containing all 0's but do not have any file blocks associated
 *	with the data). Typical examples of these are files created by dbm
 *	variants (.pag files). While the file size of these files are huge, the
 *	actual storage is quite small (the files are sparse). The problem is
 *	the holes read as all zeros so are probably stored on the archive that
 *	way (there is no way to determine if the file block is really a hole,
 *	we only know that a file block of all zero's can be a hole).
 *	At this writing, no major archive format knows how to archive files
 *	with holes. However, on extraction (or during copy, -rw) we have to
 *	deal with these files. Without detecting the holes, the files can
 *	consume a lot of file space if just written to disk. This replacement
 *	for write when passed the basic allocation size of a file system block,
 *	uses lseek whenever it detects the input data is all 0 within that
 *	file block. In more detail, the strategy is as follows:
 *	While the input is all zero keep doing an lseek. Keep track of when we
 *	pass over file block boundaries. Only write when we hit a non zero
 *	input. once we have written a file block, we continue to write it to
 *	the end (we stop looking at the input). When we reach the start of the
 *	next file block, start checking for zero blocks again. Working on file
 *	block boundaries significantly reduces the overhead when copying files
 *	that are NOT very sparse. This overhead (when compared to a write) is
 *	almost below the measurement resolution on many systems. Without it,
 *	files with holes cannot be safely copied. It does has a side effect as
 *	it can put holes into files that did not have them before, but that is
 *	not a problem since the file contents are unchanged (in fact it saves
 *	file space). (Except on paging files for diskless clients. But since we
 *	cannot determine one of those file from here, we ignore them). If this
 *	ever ends up on a system where CTG files are supported and the holes
 *	are not desired, just do a conditional test in those routines that
 *	call file_write() and have it call write() instead. BEFORE CLOSING THE
 *	FILE, make sure to call file_flush() when the last write finishes with
 *	an empty block. A lot of file systems will not create an lseek hole at
 *	the end. In this case we drop a single 0 at the end to force the
 *	trailing 0's in the file.
 *	---Parameters---
 *	rem: how many bytes left in this file system block
 *	isempt: have we written to the file block yet (is it empty)
 *	sz: basic file block allocation size
 *	cnt: number of bytes on this write
 *	str: buffer to write
 * Return:
 *	number of bytes written, -1 on write (or lseek) error.
 */

int
file_write(int fd, char *str, size_t cnt, int *rem, int *isempt, int sz)
{
	char *pt;
	char *end;
	size_t wcnt;
	char *st = str;

	/*
	 * while we have data to process
	 */
	while (cnt) {
		if (!*rem) {
			/*
			 * We are now at the start of file system block again
			 * (or what we think one is...). start looking for
			 * empty blocks again
			 */
			*isempt = 1;
			*rem = sz;
		}

		/*
		 * only examine up to the end of the current file block or
		 * remaining characters to write, whatever is smaller
		 */
		wcnt = MINIMUM(cnt, *rem);
		cnt -= wcnt;
		*rem -= wcnt;
		if (*isempt) {
			/*
			 * have not written to this block yet, so we keep
			 * looking for zero's
			 */
			pt = st;
			end = st + wcnt;

			/*
			 * look for a zero filled buffer
			 */
			while ((pt < end) && (*pt == '\0'))
				++pt;

			if (pt == end) {
				/*
				 * skip, buf is empty so far
				 */
				if (lseek(fd, (off_t)wcnt, SEEK_CUR) < 0) {
					warn("lseek");
					return(-1);
				}
				st = pt;
				continue;
			}
			/*
			 * drat, the buf is not zero filled
			 */
			*isempt = 0;
		}

		/*
		 * have non-zero data in this file system block, have to write
		 */
		if (write(fd, st, wcnt) != wcnt) {
			warn("write");
			return(-1);
		}
		st += wcnt;
	}
	return(st - str);
}

/*
 * file_flush()
 *	when the last file block in a file is zero, many file systems will not
 *	let us create a hole at the end. To get the last block with zeros, we
 *	write the last BYTE with a zero (back up one byte and write a zero).
 */
void
file_flush(int fd, int isempt)
{
	static char blnk[] = "\0";

	/*
	 * silly test, but make sure we are only called when the last block is
	 * filled with all zeros.
	 */
	if (!isempt)
		return;

	/*
	 * move back one byte and write a zero
	 */
	if (lseek(fd, (off_t)-1, SEEK_CUR) < 0) {
		warn("Failed seek on file");
		return;
	}

	if (write(fd, blnk, 1) < 0)
		warn("Failed write to file");
	return;
}
