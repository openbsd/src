/*	$OpenBSD: xinstall.c,v 1.19 1999/01/26 04:09:35 millert Exp $	*/
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1987, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)xinstall.c	8.1 (Berkeley) 7/21/93";
#endif
static char rcsid[] = "$OpenBSD: xinstall.c,v 1.19 1999/01/26 04:09:35 millert Exp $";
#endif /* not lint */

#include <sys/param.h>
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
#include <sysexits.h>
#include <utime.h>

#include "pathnames.h"

#define	DIRECTORY	0x01		/* Tell install it's a directory. */
#define	SETFLAGS	0x02		/* Tell install to set flags. */
#define NOCHANGEBITS	(UF_IMMUTABLE | UF_APPEND | SF_IMMUTABLE | SF_APPEND)
#define BACKUP_SUFFIX	".old"

struct passwd *pp;
struct group *gp;
int dobackup, docompare, dodir, dopreserve, dostrip, safecopy;
int mode = S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH;
char pathbuf[MAXPATHLEN], tempfile[MAXPATHLEN];
char *suffix = BACKUP_SUFFIX;
uid_t uid;
gid_t gid;

void	copy __P((int, char *, int, char *, off_t, int));
int	compare __P((int, const char *, size_t, int, const char *, size_t));
void	install __P((char *, char *, u_long, u_int));
void	install_dir __P((char *));
u_long	string_to_flags __P((char **, u_long *, u_long *));
void	strip __P((char *));
void	usage __P((void));
int	create_newfile __P((char *, struct stat *));
int	create_tempfile __P((char *, char *, size_t));
int	file_write __P((int, char *, register size_t, int *, int *, int));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	struct stat from_sb, to_sb;
	mode_t *set;
	u_long fset;
	u_int iflags;
	int ch, no_target;
	char *flags, *to_name, *group = NULL, *owner = NULL;

	iflags = 0;
	while ((ch = getopt(argc, argv, "BbCcdf:g:m:o:pSs")) != -1)
		switch((char)ch) {
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
		case 'f':
			flags = optarg;
			if (string_to_flags(&flags, &fset, NULL))
				errx(EX_USAGE, "%s: invalid flag", flags);
			iflags |= SETFLAGS;
			break;
		case 'g':
			group = optarg;
			break;
		case 'm':
			if (!(set = setmode(optarg)))
				errx(EX_USAGE, "%s: invalid file mode", optarg);
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
			safecopy = 1;
			break;
		case 's':
			dostrip = 1;
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
	if ((safecopy || docompare || dostrip) && dodir)
		usage();

	/* must have at least two arguments, except when creating directories */
	if (argc < 2 && !dodir)
		usage();

	/* need to make a temp copy so we can compare stripped version */
	if (docompare && dostrip)
		safecopy = 1;

	/* get group and owner id's */
	if (group && !(gp = getgrnam(group)) && !isdigit(*group))
		errx(EX_NOUSER, "unknown group %s", group);
	gid = (group) ? ((gp) ? gp->gr_gid : (gid_t)strtoul(group, NULL, 10)) : (gid_t)-1;
	if (owner && !(pp = getpwnam(owner)) && !isdigit(*owner))
		errx(EX_NOUSER, "unknown user %s", owner);
	uid = (owner) ? ((pp) ? pp->pw_uid : (uid_t)strtoul(owner, NULL, 10)) : (uid_t)-1;

	if (dodir) {
		for (; *argv != NULL; ++argv)
			install_dir(*argv);
		exit(EX_OK);
		/* NOTREACHED */
	}

	no_target = stat(to_name = argv[argc - 1], &to_sb);
	if (!no_target && S_ISDIR(to_sb.st_mode)) {
		for (; *argv != to_name; ++argv)
			install(*argv, to_name, fset, iflags | DIRECTORY);
		exit(EX_OK);
		/* NOTREACHED */
	}

	/* can't do file1 file2 directory/file */
	if (argc != 2)
		usage();

	if (!no_target) {
		if (stat(*argv, &from_sb))
			err(EX_OSERR, "%s", *argv);
		if (!S_ISREG(to_sb.st_mode))
			errx(EX_OSERR, "%s: %s", to_name, strerror(EFTYPE));
		if (to_sb.st_dev == from_sb.st_dev &&
		    to_sb.st_ino == from_sb.st_ino)
			errx(EX_USAGE, "%s and %s are the same file", *argv, to_name);
	}
	install(*argv, to_name, fset, iflags);
	exit(EX_OK);
	/* NOTREACHED */
}

/*
 * install --
 *	build a path name and install the file
 */
void
install(from_name, to_name, fset, flags)
	char *from_name, *to_name;
	u_long fset;
	u_int flags;
{
	struct stat from_sb, to_sb;
	struct utimbuf utb;
	int devnull, from_fd, to_fd, serrno, files_match = 0;
	char *p;

	(void)memset((void *)&from_sb, 0, sizeof(from_sb));
	(void)memset((void *)&to_sb, 0, sizeof(to_sb));

	/* If try to install NULL file to a directory, fails. */
	if (flags & DIRECTORY || strcmp(from_name, _PATH_DEVNULL)) {
		if (stat(from_name, &from_sb))
			err(EX_OSERR, "%s", from_name);
		if (!S_ISREG(from_sb.st_mode))
			errx(EX_OSERR, "%s: %s", from_name, strerror(EFTYPE));
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
			warnx("%s: %s", to_name, strerror(EFTYPE));
		}
	} else if (docompare) {
		/* File does not exist so silently ignore compare flag. */
		docompare = 0;
	}

	if (safecopy) {
		to_fd = create_tempfile(to_name, tempfile, sizeof(tempfile));
		if (to_fd < 0)
			err(EX_OSERR, "%s", tempfile);
	} else if (docompare && !dostrip) {
		if ((to_fd = open(to_name, O_RDONLY, 0)) < 0)
			err(EX_OSERR, "%s", to_name);
	} else {
		if ((to_fd = create_newfile(to_name, &to_sb)) < 0)
			err(EX_OSERR, "%s", to_name);
	}

	if (!devnull) {
		if ((from_fd = open(from_name, O_RDONLY, 0)) < 0) {
			serrno = errno;
			(void)unlink(safecopy ? tempfile : to_name);
			errx(EX_OSERR, "%s: %s", from_name, strerror(serrno));
		}

		if (docompare && !safecopy) {
			files_match = !(compare(from_fd, from_name,
					(size_t)from_sb.st_size, to_fd,
					to_name, (size_t)to_sb.st_size));

			/* Truncate "to" file for copy unless we match */
			if (!files_match) {
				(void)close(to_fd);
				if ((to_fd = create_newfile(to_name, &to_sb)) < 0)
					err(EX_OSERR, "%s", to_name);
			}
		}
		if (!files_match)
			copy(from_fd, from_name, to_fd,
			     safecopy ? tempfile : to_name, from_sb.st_size,
			     ((off_t)from_sb.st_blocks * S_BLKSIZE < from_sb.st_size));
	}

	if (dostrip) {
		strip(safecopy ? tempfile : to_name);

		/*
		 * Re-open our fd on the target, in case we used a strip
		 *  that does not work in-place -- like gnu binutils strip.
		 */
		close(to_fd);
		if ((to_fd = open(safecopy ? tempfile : to_name, O_RDONLY,
		     0)) < 0)
			err(EX_OSERR, "stripping %s", to_name);
	}

	/*
	 * Compare the (possibly stripped) temp file to the target.
	 */
	if (safecopy && docompare) {
		int temp_fd = to_fd;
		struct stat temp_sb;

		/* Re-open to_fd using the real target name. */
		if ((to_fd = open(to_name, O_RDONLY, 0)) < 0)
			err(EX_OSERR, "%s", to_name);

		if (fstat(temp_fd, &temp_sb)) {
			serrno = errno;
			(void)unlink(tempfile);
			errx(EX_OSERR, "%s: %s", tempfile, strerror(serrno));
		}

		if (compare(temp_fd, tempfile, (size_t)temp_sb.st_size, to_fd,
			    to_name, (size_t)to_sb.st_size) == 0) {
			/*
			 * If target has more than one link we need to
			 * replace it in order to snap the extra links.
			 * Need to preserve target file times, though.
			 */
			if (to_sb.st_nlink != 1) {
				utb.actime = to_sb.st_atime;
				utb.modtime = to_sb.st_mtime;
				(void)utime(tempfile, &utb);
			} else {
				files_match = 1;
				(void)unlink(tempfile);
			}
			(void) close(temp_fd);
		}
	}

	/*
	 * Move the new file into place if doing a safe copy
	 * and the files are different (or just not compared).
	 */
	if (safecopy && !files_match) {
		/* Try to turn off the immutable bits. */
		if (to_sb.st_flags & (NOCHANGEBITS))
			(void)chflags(to_name, to_sb.st_flags & ~(NOCHANGEBITS));
		if (dobackup) {
			char backup[MAXPATHLEN];
			(void)snprintf(backup, MAXPATHLEN, "%s%s", to_name,
			    suffix);
			if (rename(to_name, backup) < 0) {
				serrno = errno;
				unlink(tempfile);
				errx(EX_OSERR, "rename: %s to %s: %s", to_name,
				     backup, strerror(serrno));
			}
		}
		if (rename(tempfile, to_name) < 0 ) {
			serrno = errno;
			unlink(tempfile);
			errx(EX_OSERR, "rename: %s to %s: %s", tempfile,
			     to_name, strerror(serrno));
		}

		/* Re-open to_fd so we aren't hosed by the rename(2). */
		(void) close(to_fd);
		if ((to_fd = open(to_name, O_RDONLY, 0)) < 0)
			err(EX_OSERR, "%s", to_name);
	}

	/*
	 * Preserve the timestamp of the source file if necesary.
	 */
	if (dopreserve && !files_match) {
		utb.actime = from_sb.st_atime;
		utb.modtime = from_sb.st_mtime;
		(void)utime(to_name, &utb);
	}

	/*
	 * Set owner, group, mode for target; do the chown first,
	 * chown may lose the setuid bits.
	 */
	if ((gid != (gid_t)-1 || uid != (uid_t)-1) && fchown(to_fd, uid, gid)) {
		serrno = errno;
		(void)unlink(to_name);
		errx(EX_OSERR, "%s: chown/chgrp: %s", to_name, strerror(serrno));
	}
	if (fchmod(to_fd, mode)) {
		serrno = errno;
		(void)unlink(to_name);
		errx(EX_OSERR, "%s: chmod: %s", to_name, strerror(serrno));
	}

	/*
	 * If provided a set of flags, set them, otherwise, preserve the
	 * flags, except for the dump flag.
	 */
	if (fchflags(to_fd,
	    flags & SETFLAGS ? fset : from_sb.st_flags & ~UF_NODUMP)) {
		if (errno != EOPNOTSUPP || (from_sb.st_flags & ~UF_NODUMP) != 0)
			warnx("%s: chflags: %s", to_name, strerror(errno));
	}

	(void)close(to_fd);
	if (!devnull)
		(void)close(from_fd);
}

/*
 * copy --
 *	copy from one file to another
 */
void
copy(from_fd, from_name, to_fd, to_name, size, sparse)
	register int from_fd, to_fd;
	char *from_name, *to_name;
	off_t size;
	int sparse;
{
	register ssize_t nr, nw;
	int serrno;
	char *p, buf[MAXBSIZE];

	/* Rewind file descriptors. */
	if (lseek(from_fd, (off_t)0, SEEK_SET) == (off_t)-1)
		err(EX_OSERR, "lseek: %s", from_name);
	if (lseek(to_fd, (off_t)0, SEEK_SET) == (off_t)-1)
		err(EX_OSERR, "lseek: %s", to_name);

	/*
	 * Mmap and write if less than 8M (the limit is so we don't totally
	 * trash memory on big files.  This is really a minor hack, but it
	 * wins some CPU back.  Sparse files need special treatment.
	 */
	if (!sparse && size <= 8 * 1048576) {
		volatile size_t siz;

		if ((p = mmap(NULL, (size_t)size, PROT_READ, MAP_PRIVATE,
		    from_fd, (off_t)0)) == (char *)-1) {
			serrno = errno;
			(void)unlink(to_name);
			errx(EX_OSERR, "%s: %s", from_name, strerror(serrno));
		}
		siz = (size_t)size;
		if ((nw = write(to_fd, p, siz)) != siz) {
			serrno = errno;
			(void)unlink(to_name);
			errx(EX_OSERR, "%s: %s",
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

		while ((nr = read(from_fd, buf, sizeof(buf))) > 0) {
			if (sparse)
				nw = file_write(to_fd, buf, nr, &rem, &isem, sz);
			else
				nw = write(to_fd, buf, nr);
			if (nw != nr) {
				serrno = errno;
				(void)unlink(to_name);
				errx(EX_OSERR, "%s: %s",
				    to_name, strerror(nw > 0 ? EIO : serrno));
			}
		}
		if (nr != 0) {
			serrno = errno;
			(void)unlink(to_name);
			errx(EX_OSERR, "%s: %s", from_name, strerror(serrno));
		}
	}
}

/*
 * compare --
 *	compare two files; non-zero means files differ
 */
int
compare(from_fd, from_name, from_len, to_fd, to_name, to_len)
	int from_fd;
	const char *from_name;
	size_t from_len;
	int to_fd;
	const char *to_name;
	size_t to_len;
{
	caddr_t p1, p2;
	register size_t length, remainder;
	int dfound;

	if (from_len != to_len)
		return(1);

	/* Rewind file descriptors. */
	if (lseek(from_fd, (off_t)0, SEEK_SET) == (off_t)-1)
		err(EX_OSERR, "lseek: %s", from_name);
	if (lseek(to_fd, (off_t)0, SEEK_SET) == (off_t)-1)
		err(EX_OSERR, "lseek: %s", to_name);

	/*
	 * Compare the two files being careful not to mmap
	 * more than 8M at a time.
	 */
	remainder = from_len;
	do {
		length = MIN(remainder, 8 * 1048576);
		remainder -= length;

		if ((p1 = mmap(NULL, length, PROT_READ, 0, from_fd, (off_t)0))
		     == (caddr_t)-1)
			err(EX_OSERR, "%s", from_name);
		if ((p2 = mmap(NULL, length, PROT_READ, 0, to_fd, (off_t)0))
		     == (caddr_t)-1)
			err(EX_OSERR, "%s", to_name);

		dfound = memcmp(p1, p2, length);

		(void) munmap(p1, length);
		(void) munmap(p2, length);

	} while (!dfound && remainder > 0);

	return(dfound);
}

/*
 * strip --
 *	use strip(1) to strip the target file
 */
void
strip(to_name)
	char *to_name;
{
	int serrno, status;
	char *path_strip;

#ifdef __GNUC__				/* XXX: to shut up gcc warnings */
        (void)&path_strip;
#endif

	if (issetugid() || (path_strip = getenv("STRIP")) == NULL)
		path_strip = _PATH_STRIP;

	switch (vfork()) {
	case -1:
		serrno = errno;
		(void)unlink(to_name);
		errx(EX_TEMPFAIL, "forks: %s", strerror(serrno));
	case 0:
		execl(path_strip, "strip", to_name, NULL);
		warn("%s", path_strip);
		_exit(EX_OSERR);
	default:
		if (wait(&status) == -1 || !WIFEXITED(status))
			(void)unlink(to_name);
	}
}

/*
 * install_dir --
 *	build directory heirarchy
 */
void
install_dir(path)
        char *path;
{
	register char *p;
	struct stat sb;
	int ch;

	for (p = path;; ++p)
		if (!*p || (p != path && *p  == '/')) {
			ch = *p;
			*p = '\0';
			if (stat(path, &sb)) {
				if (errno != ENOENT || mkdir(path, 0777) < 0) {
					err(EX_OSERR, "%s", path);
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
usage()
{
	(void)fprintf(stderr, "\
usage: install [-bCcpSs] [-B suffix] [-f flags] [-g group] [-m mode] [-o owner] file1 file2\n\
       install [-bCcpSs] [-B suffix] [-f flags] [-g group] [-m mode] [-o owner] file1 ... fileN directory\n\
       install  -d   [-g group] [-m mode] [-o owner] directory ...\n");
	exit(EX_USAGE);
	/* NOTREACHED */
}

/*
 * create_tempfile --
 *	create a temporary file based on path and open it
 */
int
create_tempfile(path, temp, tsize)
        char *path;
        char *temp;
	size_t tsize;
{
	char *p;

	(void)strncpy(temp, path, tsize);
	temp[tsize - 1] = '\0';
	if ((p = strrchr(temp, '/')))
		p++;
	else
		p = temp;
	(void)strncpy(p, "INS@XXXXXX", &temp[tsize - 1] - p);
	temp[tsize - 1] = '\0';

	return(mkstemp(temp));
}

/*
 * create_newfile --
 *	create a new file, overwriting an existing one if necesary
 */
int
create_newfile(path, sbp)
        char *path;
	struct stat *sbp;
{
	char backup[MAXPATHLEN];

	/*
	 * Unlink now... avoid ETXTBSY errors later.  Try and turn
	 * off the append/immutable bits -- if we fail, go ahead,
	 * it might work.
	 */
	if (sbp->st_flags & (NOCHANGEBITS))
		(void)chflags(path, sbp->st_flags & ~(NOCHANGEBITS));

	if (dobackup) {
		(void)snprintf(backup, MAXPATHLEN, "%s%s", path, suffix);
		if (rename(path, backup) < 0)
			err(EX_OSERR, "rename: %s to %s", path, backup);
	} else
		(void)unlink(path);

	return(open(path, O_CREAT | O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR));
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
 *	pass over file block boundries. Only write when we hit a non zero
 *	input. once we have written a file block, we continue to write it to
 *	the end (we stop looking at the input). When we reach the start of the
 *	next file block, start checking for zero blocks again. Working on file
 *	block boundries significantly reduces the overhead when copying files
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
file_write(fd, str, cnt, rem, isempt, sz)
	int fd;
	char *str;
	register size_t cnt;
	int *rem;
	int *isempt;
	int sz;
{
	register char *pt;
	register char *end;
	register size_t wcnt;
	register char *st = str;

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
		wcnt = MIN(cnt, *rem);
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
