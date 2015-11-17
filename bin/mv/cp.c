/*	$OpenBSD: cp.c,v 1.1 2015/11/17 17:24:26 tedu Exp $	*/
/*	$NetBSD: cp.c,v 1.14 1995/09/07 06:14:51 jtc Exp $	*/

/*
 * Copyright (c) 1988, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * David Hitz of Auspex Systems Inc.
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

/*
 * Cp copies source files to target files.
 *
 * The global PATH_T structure "to" always contains the path to the
 * current target file.  Since fts(3) does not change directories,
 * this path can be either absolute or dot-relative.
 *
 * The basic algorithm is to initialize "to" and use fts(3) to traverse
 * the file hierarchy rooted in the argument list.  A trivial case is the
 * case of 'cp file1 file2'.  The more interesting case is the case of
 * 'cp file1 file2 ... fileN dir' where the hierarchy is traversed and the
 * path (relative to the root of the traversal) is appended to dir (stored
 * in "to") to form the final target path.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/time.h>

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#define	fts_dne(_x)	(_x->fts_pointer != NULL)

typedef struct {
	char *p_end;                    /* pointer to NULL at end of path */
	char *target_end;               /* pointer to end of target base */
	char p_path[PATH_MAX];          /* pointer to the start of a path */
} PATH_T;

static PATH_T to = { to.p_path, "" };

static int     copy_fifo(struct stat *, int);
static int     copy_file(FTSENT *, int);
static int     copy_link(FTSENT *, int);
static int     copy_special(struct stat *, int);
static int     setfile(struct stat *, int);
static int     setlink(struct stat *);


extern char *__progname;

static uid_t myuid;
static int Rflag, fflag, iflag, pflag, rflag;
static mode_t myumask;

enum op { FILE_TO_FILE, FILE_TO_DIR, DIR_TO_DNE };

static int copy(char *[], enum op, int);
static char *find_last_component(char *);

static void __dead
usage(void)
{
	(void)fprintf(stderr,
	    "usage: %s [-fip] [-R [-H | -L | -P]] source target\n", __progname);
	(void)fprintf(stderr,
	    "       %s [-fip] [-R [-H | -L | -P]] source ... directory\n",
	    __progname);
	exit(1);
}

int
cpmain(int argc, char *argv[])
{
	struct stat to_stat, tmp_stat;
	enum op type;
	int Hflag, Lflag, Pflag, ch, fts_options, r;
	char *target;

	(void)setlocale(LC_ALL, "");

	Hflag = Lflag = Pflag = Rflag = 0;
	while ((ch = getopt(argc, argv, "HLPRfipr")) != -1)
		switch (ch) {
		case 'H':
			Hflag = 1;
			Lflag = Pflag = 0;
			break;
		case 'L':
			Lflag = 1;
			Hflag = Pflag = 0;
			break;
		case 'P':
			Pflag = 1;
			Hflag = Lflag = 0;
			break;
		case 'R':
			Rflag = 1;
			break;
		case 'f':
			fflag = 1;
			iflag = 0;
			break;
		case 'i':
			iflag = isatty(STDIN_FILENO);
			fflag = 0;
			break;
		case 'p':
			pflag = 1;
			break;
		case 'r':
			rflag = 1;
			break;
		default:
			usage();
			break;
		}
	argc -= optind;
	argv += optind;

	/*
	 * Unfortunately, -R will use mkfifo & mknod;
	 * -p will use fchown, fchmod, lchown, fchflags..
	 */
	if (Rflag == 0 && pflag == 0)
		if (pledge("stdio rpath wpath cpath fattr", NULL) == -1)
			err(1, "pledge");

	if (argc < 2)
		usage();

	fts_options = FTS_NOCHDIR | FTS_PHYSICAL;
	if (rflag) {
		if (Rflag)
			errx(1,
		    "the -R and -r options may not be specified together.");
		if (Hflag || Lflag || Pflag)
			errx(1,
	"the -H, -L, and -P options may not be specified with the -r option.");
		fts_options &= ~FTS_PHYSICAL;
		fts_options |= FTS_LOGICAL;
	}
	if (Rflag) {
		if (Hflag)
			fts_options |= FTS_COMFOLLOW;
		if (Lflag) {
			fts_options &= ~FTS_PHYSICAL;
			fts_options |= FTS_LOGICAL;
		}
	} else {
		fts_options &= ~FTS_PHYSICAL;
		fts_options |= FTS_LOGICAL;
	}

	myuid = getuid();

	/* Copy the umask for explicit mode setting. */
	myumask = umask(0);
	(void)umask(myumask);

	/* Save the target base in "to". */
	target = argv[--argc];
	if (strlcpy(to.p_path, target, sizeof to.p_path) >= sizeof(to.p_path))
		errx(1, "%s: name too long", target);
	to.p_end = to.p_path + strlen(to.p_path);
	if (to.p_path == to.p_end) {
		*to.p_end++ = '.';
		*to.p_end = '\0';
	}
	to.target_end = to.p_end;

	/* Set end of argument list for fts(3). */
	argv[argc] = NULL;

	/*
	 * Cp has two distinct cases:
	 *
	 * cp [-R] source target
	 * cp [-R] source1 ... sourceN directory
	 *
	 * In both cases, source can be either a file or a directory.
	 *
	 * In (1), the target becomes a copy of the source. That is, if the
	 * source is a file, the target will be a file, and likewise for
	 * directories.
	 *
	 * In (2), the real target is not directory, but "directory/source".
	 */
	r = stat(to.p_path, &to_stat);
	if (r == -1 && errno != ENOENT)
		err(1, "%s", to.p_path);
	if (r == -1 || !S_ISDIR(to_stat.st_mode)) {
		/*
		 * Case (1).  Target is not a directory.
		 */
		if (argc > 1)
			usage();
		/*
		 * Need to detect the case:
		 *	cp -R dir foo
		 * Where dir is a directory and foo does not exist, where
		 * we want pathname concatenations turned on but not for
		 * the initial mkdir().
		 */
		if (r == -1) {
			if (rflag || (Rflag && (Lflag || Hflag)))
				stat(*argv, &tmp_stat);
			else
				lstat(*argv, &tmp_stat);

			if (S_ISDIR(tmp_stat.st_mode) && (Rflag || rflag))
				type = DIR_TO_DNE;
			else
				type = FILE_TO_FILE;
		} else
			type = FILE_TO_FILE;
	} else {
		/*
		 * Case (2).  Target is a directory.
		 */
		type = FILE_TO_DIR;
	}

	return (copy(argv, type, fts_options));
}

static char *
find_last_component(char *path)
{
	char *p;

	if ((p = strrchr(path, '/')) == NULL)
		p = path;
	else {
		/* Special case foo/ */
		if (!*(p+1)) {
			while ((p >= path) && *p == '/')
				p--;

			while ((p >= path) && *p != '/')
				p--;
		}

		p++;
	}

	return (p);
}

static int
copy(char *argv[], enum op type, int fts_options)
{
	struct stat to_stat;
	FTS *ftsp;
	FTSENT *curr;
	int base, nlen, rval;
	char *p, *target_mid;
	base = 0;

	if ((ftsp = fts_open(argv, fts_options, NULL)) == NULL)
		err(1, NULL);
	for (rval = 0; (curr = fts_read(ftsp)) != NULL;) {
		switch (curr->fts_info) {
		case FTS_NS:
		case FTS_DNR:
		case FTS_ERR:
			warnx("%s: %s",
			    curr->fts_path, strerror(curr->fts_errno));
			rval = 1;
			continue;
		case FTS_DC:
			warnx("%s: directory causes a cycle", curr->fts_path);
			rval = 1;
			continue;
		}

		/*
		 * If we are in case (2) or (3) above, we need to append the
		 * source name to the target name.
		 */
		if (type != FILE_TO_FILE) {
			/*
			 * Need to remember the roots of traversals to create
			 * correct pathnames.  If there's a directory being
			 * copied to a non-existent directory, e.g.
			 *	cp -R a/dir noexist
			 * the resulting path name should be noexist/foo, not
			 * noexist/dir/foo (where foo is a file in dir), which
			 * is the case where the target exists.
			 *
			 * Also, check for "..".  This is for correct path
			 * concatenation for paths ending in "..", e.g.
			 *	cp -R .. /tmp
			 * Paths ending in ".." are changed to ".".  This is
			 * tricky, but seems the easiest way to fix the problem.
			 *
			 * XXX
			 * Since the first level MUST be FTS_ROOTLEVEL, base
			 * is always initialized.
			 */
			if (curr->fts_level == FTS_ROOTLEVEL) {
				if (type != DIR_TO_DNE) {
					p = find_last_component(curr->fts_path);
					base = p - curr->fts_path;
					
					if (!strcmp(&curr->fts_path[base],
					    ".."))
						base += 1;
				} else
					base = curr->fts_pathlen;
			}

			p = &curr->fts_path[base];
			nlen = curr->fts_pathlen - base;
			target_mid = to.target_end;
			if (*p != '/' && target_mid[-1] != '/')
				*target_mid++ = '/';
			*target_mid = '\0';
			if (target_mid - to.p_path + nlen >= PATH_MAX) {
				warnx("%s%s: name too long (not copied)",
				    to.p_path, p);
				rval = 1;
				continue;
			}
			(void)strncat(target_mid, p, nlen);
			to.p_end = target_mid + nlen;
			*to.p_end = '\0';
		}

		/* Not an error but need to remember it happened */
		if (stat(to.p_path, &to_stat) == -1) {
			if (curr->fts_info == FTS_DP)
				continue;
			/*
			 * We use fts_pointer as a boolean to indicate that
			 * we created this directory ourselves.  We'll use
			 * this later on via the fts_dne macro to decide
			 * whether or not to set the directory mode during
			 * the post-order pass.
			 */
			curr->fts_pointer = (void *)1;
		} else {
			/*
			 * Set directory mode/user/times on the post-order
			 * pass.  We can't do this earlier because the mode
			 * may not allow us write permission.  Furthermore,
			 * if we set the times during the pre-order pass,
			 * they will get changed later when the directory
			 * is populated.
			 */
			if (curr->fts_info == FTS_DP) {
				if (!S_ISDIR(to_stat.st_mode))
					continue;
				/*
				 * If not -p and directory didn't exist, set
				 * it to be the same as the from directory,
				 * unmodified by the umask; arguably wrong,
				 * but it's been that way forever.
				 */
				if (pflag && setfile(curr->fts_statp, -1))
					rval = 1;
				else if (fts_dne(curr))
					(void)chmod(to.p_path,
					    curr->fts_statp->st_mode);
				continue;
			}
			if (to_stat.st_dev == curr->fts_statp->st_dev &&
			    to_stat.st_ino == curr->fts_statp->st_ino) {
				warnx("%s and %s are identical (not copied).",
				    to.p_path, curr->fts_path);
				rval = 1;
				if (S_ISDIR(curr->fts_statp->st_mode))
					(void)fts_set(ftsp, curr, FTS_SKIP);
				continue;
			}
			if (!S_ISDIR(curr->fts_statp->st_mode) &&
			    S_ISDIR(to_stat.st_mode)) {
		warnx("cannot overwrite directory %s with non-directory %s",
				    to.p_path, curr->fts_path);
				rval = 1;
				continue;
			}
		}

		switch (curr->fts_statp->st_mode & S_IFMT) {
		case S_IFLNK:
			if (copy_link(curr, !fts_dne(curr)))
				rval = 1;
			break;
		case S_IFDIR:
			if (!Rflag && !rflag) {
				warnx("%s is a directory (not copied).",
				    curr->fts_path);
				(void)fts_set(ftsp, curr, FTS_SKIP);
				rval = 1;
				break;
			}
			/*
			 * If the directory doesn't exist, create the new
			 * one with the from file mode plus owner RWX bits,
			 * modified by the umask.  Trade-off between being
			 * able to write the directory (if from directory is
			 * 555) and not causing a permissions race.  If the
			 * umask blocks owner writes, we fail..
			 */
			if (fts_dne(curr)) {
				if (mkdir(to.p_path,
				    curr->fts_statp->st_mode | S_IRWXU) < 0)
					err(1, "%s", to.p_path);
			} else if (!S_ISDIR(to_stat.st_mode))
				errc(1, ENOTDIR, "%s", to.p_path);
			break;
		case S_IFBLK:
		case S_IFCHR:
			if (Rflag) {
				if (copy_special(curr->fts_statp, !fts_dne(curr)))
					rval = 1;
			} else
				if (copy_file(curr, fts_dne(curr)))
					rval = 1;
			break;
		case S_IFIFO:
			if (Rflag) {
				if (copy_fifo(curr->fts_statp, !fts_dne(curr)))
					rval = 1;
			} else
				if (copy_file(curr, fts_dne(curr)))
					rval = 1;
			break;
		case S_IFSOCK:
			warnc(EOPNOTSUPP, "%s", curr->fts_path);
			break;
		default:
			if (copy_file(curr, fts_dne(curr)))
				rval = 1;
			break;
		}
	}
	if (errno)
		err(1, "fts_read");
	(void)fts_close(ftsp);
	return (rval);
}


/*	$OpenBSD: cp.c,v 1.1 2015/11/17 17:24:26 tedu Exp $	*/
/*	$NetBSD: utils.c,v 1.6 1997/02/26 14:40:51 cgd Exp $	*/

/*-
 * Copyright (c) 1991, 1993, 1994
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

#include <sys/param.h>		/* MAXBSIZE */
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/time.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

static int
copy_file(FTSENT *entp, int dne)
{
	static char *buf;
	static char *zeroes;
	struct stat to_stat, *fs;
	int ch, checkch, from_fd, rcount, rval, to_fd, wcount;
#ifdef VM_AND_BUFFER_CACHE_SYNCHRONIZED
	char *p;
#endif

	if (!buf) {
		buf = malloc(MAXBSIZE);
		if (!buf)
			err(1, "malloc");
	}
	if (!zeroes) {
		zeroes = calloc(1, MAXBSIZE);
		if (!zeroes)
			err(1, "calloc");
	}

	if ((from_fd = open(entp->fts_path, O_RDONLY, 0)) == -1) {
		warn("%s", entp->fts_path);
		return (1);
	}

	fs = entp->fts_statp;

	/*
	 * In -f (force) mode, we always unlink the destination first
	 * if it exists.  Note that -i and -f are mutually exclusive.
	 */
	if (!dne && fflag)
		(void)unlink(to.p_path);

	/*
	 * If the file exists and we're interactive, verify with the user.
	 * If the file DNE, set the mode to be the from file, minus setuid
	 * bits, modified by the umask; arguably wrong, but it makes copying
	 * executables work right and it's been that way forever.  (The
	 * other choice is 666 or'ed with the execute bits on the from file
	 * modified by the umask.)
	 */
	if (!dne && !fflag) {
		if (iflag) {
			(void)fprintf(stderr, "overwrite %s? ", to.p_path);
			checkch = ch = getchar();
			while (ch != '\n' && ch != EOF)
				ch = getchar();
			if (checkch != 'y' && checkch != 'Y') {
				(void)close(from_fd);
				return (0);
			}
		}
		to_fd = open(to.p_path, O_WRONLY | O_TRUNC, 0);
	} else
		to_fd = open(to.p_path, O_WRONLY | O_TRUNC | O_CREAT,
		    fs->st_mode & ~(S_ISTXT | S_ISUID | S_ISGID));

	if (to_fd == -1) {
		warn("%s", to.p_path);
		(void)close(from_fd);
		return (1);
	}

	rval = 0;

	/*
	 * Mmap and write if less than 8M (the limit is so we don't totally
	 * trash memory on big files.  This is really a minor hack, but it
	 * wins some CPU back.
	 */
#ifdef VM_AND_BUFFER_CACHE_SYNCHRONIZED
	/* XXX broken for 0-size mmap */
	if (fs->st_size <= 8 * 1048576) {
		if ((p = mmap(NULL, (size_t)fs->st_size, PROT_READ,
		    MAP_FILE|MAP_SHARED, from_fd, (off_t)0)) == MAP_FAILED) {
			warn("mmap: %s", entp->fts_path);
			rval = 1;
		} else {
			madvise(p, fs->st_size, MADV_SEQUENTIAL);
			if (write(to_fd, p, fs->st_size) != fs->st_size) {
				warn("%s", to.p_path);
				rval = 1;
			}
			/* Some systems don't unmap on close(2). */
			if (munmap(p, fs->st_size) < 0) {
				warn("%s", entp->fts_path);
				rval = 1;
			}
		}
	} else
#endif
	{
		int skipholes = 0;
		struct stat tosb;
		if (!fstat(to_fd, &tosb) && S_ISREG(tosb.st_mode))
			skipholes = 1;
		while ((rcount = read(from_fd, buf, MAXBSIZE)) > 0) {
			if (skipholes && memcmp(buf, zeroes, rcount) == 0)
				wcount = lseek(to_fd, rcount, SEEK_CUR) == -1 ? -1 : rcount;
			else
				wcount = write(to_fd, buf, rcount);
			if (rcount != wcount || wcount == -1) {
				warn("%s", to.p_path);
				rval = 1;
				break;
			}
		}
		if (skipholes && rcount >= 0)
			rcount = ftruncate(to_fd, lseek(to_fd, 0, SEEK_CUR));
		if (rcount < 0) {
			warn("%s", entp->fts_path);
			rval = 1;
		}
	}

	if (rval == 1) {
		(void)close(from_fd);
		(void)close(to_fd);
		return (1);
	}

	if (pflag && setfile(fs, to_fd))
		rval = 1;
	/*
	 * If the source was setuid or setgid, lose the bits unless the
	 * copy is owned by the same user and group.
	 */
#define	RETAINBITS \
	(S_ISUID | S_ISGID | S_ISVTX | S_IRWXU | S_IRWXG | S_IRWXO)
	if (!pflag && dne &&
	    fs->st_mode & (S_ISUID | S_ISGID) && fs->st_uid == myuid) {
		if (fstat(to_fd, &to_stat)) {
			warn("%s", to.p_path);
			rval = 1;
		} else if (fs->st_gid == to_stat.st_gid &&
		    fchmod(to_fd, fs->st_mode & RETAINBITS & ~myumask)) {
			warn("%s", to.p_path);
			rval = 1;
		}
	}
	(void)close(from_fd);
	if (close(to_fd)) {
		warn("%s", to.p_path);
		rval = 1;
	}
	return (rval);
}

static int
copy_link(FTSENT *p, int exists)
{
	int len;
	char linkname[PATH_MAX];

	if ((len = readlink(p->fts_path, linkname, sizeof(linkname)-1)) == -1) {
		warn("readlink: %s", p->fts_path);
		return (1);
	}
	linkname[len] = '\0';
	if (exists && unlink(to.p_path)) {
		warn("unlink: %s", to.p_path);
		return (1);
	}
	if (symlink(linkname, to.p_path)) {
		warn("symlink: %s", linkname);
		return (1);
	}
	return (pflag ? setlink(p->fts_statp) : 0);
}

static int
copy_fifo(struct stat *from_stat, int exists)
{
	if (exists && unlink(to.p_path)) {
		warn("unlink: %s", to.p_path);
		return (1);
	}
	if (mkfifo(to.p_path, from_stat->st_mode)) {
		warn("mkfifo: %s", to.p_path);
		return (1);
	}
	return (pflag ? setfile(from_stat, -1) : 0);
}

static int
copy_special(struct stat *from_stat, int exists)
{
	if (exists && unlink(to.p_path)) {
		warn("unlink: %s", to.p_path);
		return (1);
	}
	if (mknod(to.p_path, from_stat->st_mode, from_stat->st_rdev)) {
		warn("mknod: %s", to.p_path);
		return (1);
	}
	return (pflag ? setfile(from_stat, -1) : 0);
}


static int
setfile(struct stat *fs, int fd)
{
	struct timespec ts[2];
	int rval;

	rval = 0;
	fs->st_mode &= S_ISTXT | S_ISUID | S_ISGID | S_IRWXU | S_IRWXG | S_IRWXO;

	ts[0] = fs->st_atim;
	ts[1] = fs->st_mtim;
	if (fd >= 0 ? futimens(fd, ts) :
	    utimensat(AT_FDCWD, to.p_path, ts, AT_SYMLINK_NOFOLLOW)) {
		warn("update times: %s", to.p_path);
		rval = 1;
	}
	/*
	 * Changing the ownership probably won't succeed, unless we're root
	 * or POSIX_CHOWN_RESTRICTED is not set.  Set uid/gid before setting
	 * the mode; current BSD behavior is to remove all setuid bits on
	 * chown.  If chown fails, lose setuid/setgid bits.
	 */
	if (fd >= 0 ? fchown(fd, fs->st_uid, fs->st_gid) :
	    lchown(to.p_path, fs->st_uid, fs->st_gid)) {
		if (errno != EPERM) {
			warn("chown: %s", to.p_path);
			rval = 1;
		}
		fs->st_mode &= ~(S_ISTXT | S_ISUID | S_ISGID);
	}
	if (fd >= 0 ? fchmod(fd, fs->st_mode) :
	    fchmodat(AT_FDCWD, to.p_path, fs->st_mode, AT_SYMLINK_NOFOLLOW)) {
		warn("chmod: %s", to.p_path);
		rval = 1;
	}

	/*
	 * XXX
	 * NFS doesn't support chflags; ignore errors unless there's reason
	 * to believe we're losing bits.  (Note, this still won't be right
	 * if the server supports flags and we were trying to *remove* flags
	 * on a file that we copied, i.e., that we didn't create.)
	 */
	errno = 0;
	if (fd >= 0 ? fchflags(fd, fs->st_flags) :
	    chflagsat(AT_FDCWD, to.p_path, fs->st_flags, AT_SYMLINK_NOFOLLOW))
		if (errno != EOPNOTSUPP || fs->st_flags != 0) {
			warn("chflags: %s", to.p_path);
			rval = 1;
		}
	return (rval);
}


static int
setlink(struct stat *fs)
{

	if (lchown(to.p_path, fs->st_uid, fs->st_gid)) {
		if (errno != EPERM) {
			warn("lchown: %s", to.p_path);
			return (1);
		}
	}
	return (0);
}
