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
static char rcsid[] = "$NetBSD: xinstall.c,v 1.9 1995/12/20 10:25:17 jonathan Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <paths.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <err.h>

#include "pathnames.h"

struct passwd *pp;
struct group *gp;
int docopy, dodir, dostrip;
int mode = S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH;
char pathbuf[MAXPATHLEN];
uid_t uid;
gid_t gid;

#define	DIRECTORY	0x01		/* Tell install it's a directory. */
#define	SETFLAGS	0x02		/* Tell install to set flags. */

void	copy __P((int, char *, int, char *, off_t));
void	install __P((char *, char *, u_long, u_int));
void	install_dir __P((char *));
u_long	string_to_flags __P((char **, u_long *, u_long *));
void	strip __P((char *));
void	usage __P((void));

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
	while ((ch = getopt(argc, argv, "cf:g:m:o:sd")) != EOF)
		switch((char)ch) {
		case 'c':
			docopy = 1;
			break;
		case 'f':
			flags = optarg;
			if (string_to_flags(&flags, &fset, NULL))
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
			break;
		case 'o':
			owner = optarg;
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

	/* copy and strip options make no sense when creating directories */
	if ((docopy || dostrip) && dodir)
		usage();

	/* must have at least two arguments, except when creating directories */
	if (argc < 2 && !dodir)
		usage();

	/* get group and owner id's */
	if (group && !(gp = getgrnam(group)) && !isdigit(*group))
		errx(1, "unknown group %s", group);
	gid = (group) ? ((gp) ? gp->gr_gid : atoi(group)) : -1;
	if (owner && !(pp = getpwnam(owner)) && !isdigit(*owner))
		errx(1, "unknown user %s", owner);
	uid = (owner) ? ((pp) ? pp->pw_uid : atoi(owner)) : -1;

	if (dodir) {
		for (; *argv != NULL; ++argv)
			install_dir(*argv);
		exit (0);
		/* NOTREACHED */
	}

	no_target = stat(to_name = argv[argc - 1], &to_sb);
	if (!no_target && S_ISDIR(to_sb.st_mode)) {
		for (; *argv != to_name; ++argv)
			install(*argv, to_name, fset, iflags | DIRECTORY);
		exit(0);
	}

	/* can't do file1 file2 directory/file */
	if (argc != 2)
		usage();

	if (!no_target) {
		if (stat(*argv, &from_sb))
			err(1, "%s", *argv);
		if (!S_ISREG(to_sb.st_mode))
			errx(1, "%s: %s", to_name, strerror(EFTYPE));
		if (to_sb.st_dev == from_sb.st_dev &&
		    to_sb.st_ino == from_sb.st_ino)
			errx(1, "%s and %s are the same file", *argv, to_name);
		/*
		 * Unlink now... avoid ETXTBSY errors later.  Try and turn
		 * off the append/immutable bits -- if we fail, go ahead,
		 * it might work.
		 */
#define	NOCHANGEBITS	(UF_IMMUTABLE | UF_APPEND | SF_IMMUTABLE | SF_APPEND)
		if (to_sb.st_flags & NOCHANGEBITS)
			(void)chflags(to_name,
			    to_sb.st_flags & ~(NOCHANGEBITS));
		(void)unlink(to_name);
	}
	install(*argv, to_name, fset, iflags);
	exit(0);
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
	int devnull, from_fd, to_fd, serrno;
	char *p;

	/* If try to install NULL file to a directory, fails. */
	if (flags & DIRECTORY || strcmp(from_name, _PATH_DEVNULL)) {
		if (stat(from_name, &from_sb))
			err(1, "%s", from_name);
		if (!S_ISREG(from_sb.st_mode))
			errx(1, "%s: %s", from_name, strerror(EFTYPE));
		/* Build the target path. */
		if (flags & DIRECTORY) {
			(void)snprintf(pathbuf, sizeof(pathbuf), "%s/%s",
			    to_name,
			    (p = rindex(from_name, '/')) ? ++p : from_name);
			to_name = pathbuf;
		}
		devnull = 0;
	} else {
		from_sb.st_flags = 0;	/* XXX */
		devnull = 1;
	}

	/*
	 * Unlink now... avoid ETXTBSY errors later.  Try and turn
	 * off the append/immutable bits -- if we fail, go ahead,
	 * it might work.
	 */
	if (stat(to_name, &to_sb) == 0 &&
	    to_sb.st_flags & (NOCHANGEBITS))
		(void)chflags(to_name, to_sb.st_flags & ~(NOCHANGEBITS));
	(void)unlink(to_name);

	/* Create target. */
	if ((to_fd = open(to_name,
	    O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR)) < 0)
		err(1, "%s", to_name);
	if (!devnull) {
		if ((from_fd = open(from_name, O_RDONLY, 0)) < 0) {
			(void)unlink(to_name);
			err(1, "%s", from_name);
		}
		copy(from_fd, from_name, to_fd, to_name, from_sb.st_size);
		(void)close(from_fd);
	}

	if (dostrip) {
		strip(to_name);

		/*
		 * Re-open our fd on the target, in case we used a strip
		 *  that does not work in-place -- like gnu binutils strip.
		 */
		close(to_fd);
		if ((to_fd = open(to_name, O_RDONLY, S_IRUSR | S_IWUSR)) < 0)
		  err(1, "stripping %s", to_name);
	}

	/*
	 * Set owner, group, mode for target; do the chown first,
	 * chown may lose the setuid bits.
	 */
	if ((gid != -1 || uid != -1) && fchown(to_fd, uid, gid)) {
		serrno = errno;
		(void)unlink(to_name);
		errx(1, "%s: chown/chgrp: %s", to_name, strerror(serrno));
	}
	if (fchmod(to_fd, mode)) {
		serrno = errno;
		(void)unlink(to_name);
		errx(1, "%s: chmod: %s", to_name, strerror(serrno));
	}

	/*
	 * If provided a set of flags, set them, otherwise, preserve the
	 * flags, except for the dump flag.
	 */
	if (fchflags(to_fd,
	    flags & SETFLAGS ? fset : from_sb.st_flags & ~UF_NODUMP)) {
		serrno = errno;
		(void)unlink(to_name);
		errx(1, "%s: chflags: %s", to_name, strerror(serrno));
	}

	(void)close(to_fd);
	if (!docopy && !devnull && unlink(from_name))
		err(1, "%s", from_name);
}

/*
 * copy --
 *	copy from one file to another
 */
void
copy(from_fd, from_name, to_fd, to_name, size)
	register int from_fd, to_fd;
	char *from_name, *to_name;
	off_t size;
{
	register int nr, nw;
	int serrno;
	char *p, buf[MAXBSIZE];

	/*
	 * Mmap and write if less than 8M (the limit is so we don't totally
	 * trash memory on big files.  This is really a minor hack, but it
	 * wins some CPU back.
	 */
	if (size <= 8 * 1048576) {
		if ((p = mmap(NULL, (size_t)size, PROT_READ,
		    0, from_fd, (off_t)0)) == (char *)-1)
			err(1, "%s", from_name);
		if (write(to_fd, p, size) != size)
			err(1, "%s", to_name);
	} else {
		while ((nr = read(from_fd, buf, sizeof(buf))) > 0)
			if ((nw = write(to_fd, buf, nr)) != nr) {
				serrno = errno;
				(void)unlink(to_name);
				errx(1, "%s: %s",
				    to_name, strerror(nw > 0 ? EIO : serrno));
			}
		if (nr != 0) {
			serrno = errno;
			(void)unlink(to_name);
			errx(1, "%s: %s", from_name, strerror(serrno));
		}
	}
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

	switch (vfork()) {
	case -1:
		serrno = errno;
		(void)unlink(to_name);
		errx(1, "forks: %s", strerror(errno));
	case 0:
		execl(_PATH_STRIP, "strip", to_name, NULL);
		err(1, "%s", _PATH_STRIP);
	default:
		if (wait(&status) == -1 || status)
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
					err(1, "%s", path);
					/* NOTREACHED */
                                }
                        }
                        if (!(*p = ch))
				break;
                }

	if (((gid != -1 || uid != -1) && chown(path, uid, gid)) ||
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
usage: install [-cs] [-f flags] [-g group] [-m mode] [-o owner] file1 file2\n\
       install [-cs] [-f flags] [-g group] [-m mode] [-o owner] file1 ... fileN directory\n\
       install  -d   [-g group] [-m mode] [-o owner] directory ...\n");
	exit(1);
}
