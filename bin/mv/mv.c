/*	$OpenBSD: mv.c,v 1.12 1998/07/02 18:46:34 deraadt Exp $	*/
/*	$NetBSD: mv.c,v 1.9 1995/03/21 09:06:52 cgd Exp $	*/

/*
 * Copyright (c) 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ken Smith of The State University of New York at Buffalo.
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
"@(#) Copyright (c) 1989, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)mv.c	8.2 (Berkeley) 4/2/94";
#else
static char rcsid[] = "$OpenBSD: mv.c,v 1.12 1998/07/02 18:46:34 deraadt Exp $";
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/mount.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>

#include "pathnames.h"

int fflg, iflg;
int stdin_ok;

int	copy __P((char *, char *));
int	do_move __P((char *, char *));
int	fastcopy __P((char *, char *, struct stat *));
void	usage __P((void));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	register int baselen, len, rval;
	register char *p, *endp;
	struct stat sb;
	int ch;
	char path[MAXPATHLEN];

	while ((ch = getopt(argc, argv, "if")) != -1)
		switch (ch) {
		case 'i':
			fflg = 0;
			iflg = 1;
			break;
		case 'f':
			iflg = 0;
			fflg = 1;
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc < 2)
		usage();

	stdin_ok = isatty(STDIN_FILENO);

	/*
	 * If the stat on the target fails or the target isn't a directory,
	 * try the move.  More than 2 arguments is an error in this case.
	 */
	if (stat(argv[argc - 1], &sb) || !S_ISDIR(sb.st_mode)) {
		if (argc > 2)
			usage();
		exit(do_move(argv[0], argv[1]));
	}

	/* It's a directory, move each file into it. */
	if (strlen(argv[argc - 1]) > sizeof path - 1)
		errx(1, "%s: destination pathname too long", *argv);
	(void)strcpy(path, argv[argc - 1]);
	baselen = strlen(path);
	endp = &path[baselen];
	if (*(endp - 1) != '/') {
		*endp++ = '/';
		++baselen;
	}
	for (rval = 0; --argc; ++argv) {
		char *current_arg = *argv;

		/*
		 * Get the name of the file to create from
		 * the argument. This is a bit tricky because
		 * in the case of b/ we actually want b and empty
		 * string
		 */
		if ((p = strrchr(current_arg, '/')) == NULL)
			p = current_arg;
		else {
			/* Special case foo/ */
			if (!*(p+1)) {
				while (p >= current_arg && *p == '/')
					p--;

				while (p >= current_arg && *p != '/')
					p--;
			}

			p++;
		}

		if ((baselen + (len = strlen(p))) >= MAXPATHLEN) {
			warnx("%s: destination pathname too long", *argv);
			rval = 1;
		} else {
			memmove(endp, p, len + 1);
			if (do_move(current_arg, path))
				rval = 1;
		}
	}
	exit(rval);
}

int
do_move(from, to)
	char *from, *to;
{
	struct stat sb;
	char modep[15];

	/*
	 * (1)	If the destination path exists, the -f option is not specified
	 *	and either of the following conditions are true:
	 *
	 *	(a) The perimissions of the destination path do not permit
	 *	    writing and the standard input is a terminal.
	 *	(b) The -i option is specified.
	 *
	 *	the mv utility shall write a prompt to standard error and
	 *	read a line from standard input.  If the response is not
	 *	affirmative, mv shall do nothing more with the current
	 *	source file...
	 */
	if (!fflg && !access(to, F_OK)) {
		int ask = 1;
		int ch, first;

		if (iflg) {
			(void)fprintf(stderr, "overwrite %s? ", to);
		} else if (stdin_ok && access(to, W_OK) && !stat(to, &sb)) {
			strmode(sb.st_mode, modep);
			(void)fprintf(stderr, "override %s%s%s/%s for %s? ",
			    modep + 1, modep[9] == ' ' ? "" : " ",
			    user_from_uid(sb.st_uid, 0),
			    group_from_gid(sb.st_gid, 0), to);
		} else
			ask = 0;
		if (ask) {
			first = ch = getchar();
			while (ch != '\n' && ch != EOF)
				ch = getchar();
			if (first != 'y' && first != 'Y')
				return (0);
		}
	}

	/*
	 * (2)	If rename() succeeds, mv shall do nothing more with the
	 *	current source file.  If it fails for any other reason than
	 *	EXDEV, mv shall write a diagnostic message to the standard
	 *	error and do nothing more with the current source file.
	 *
	 * (3)	If the destination path exists, and it is a file of type
	 *	directory and source_file is not a file of type directory,
	 *	or it is a file not of type directory, and source file is
	 *	a file of type directory, mv shall write a diagnostic
	 *	message to standard error, and do nothing more with the
	 *	current source file...
	 */
	if (!rename(from, to))
		return (0);

	if (errno == EXDEV) {
		struct statfs sfs;
		char path[MAXPATHLEN];

		/* Can't mv(1) a mount point. */
		if (realpath(from, path) == NULL) {
			warnx("cannot resolve %s: %s", from, path);
			return (1);
		}
		if (!statfs(path, &sfs) && !strcmp(path, sfs.f_mntonname)) {
			warnx("cannot rename a mount point");
			return (1);
		}
	} else {
		warn("rename %s to %s", from, to);
		return (1);
	}

	/*
	 * (4)	If the destination path exists, mv shall attempt to remove it.
	 *	If this fails for any reason, mv shall write a diagnostic
	 *	message to the standard error and do nothing more with the
	 *	current source file...
	 */
	if (!stat(to, &sb)) {
		if ((S_ISDIR(sb.st_mode)) ? rmdir(to) : unlink(to)) {
			warn("can't remove %s", to);
			return (1);
		}
	}

	/*
	 * (5)	The file hierarchy rooted in source_file shall be duplicated
	 *	as a file hiearchy rooted in the destination path...
	 */
	if (stat(from, &sb)) {
		warn("%s", from);
		return (1);
	}
	return (S_ISREG(sb.st_mode) ?
	    fastcopy(from, to, &sb) : copy(from, to));
}

int
fastcopy(from, to, sbp)
	char *from, *to;
	struct stat *sbp;
{
	struct timeval tval[2];
	static u_int blen;
	static char *bp;
	register int nread, from_fd, to_fd;
	int badchown = 0, serrno;

	if ((from_fd = open(from, O_RDONLY, 0)) < 0) {
		warn("%s", from);
		return (1);
	}
	if ((to_fd = open(to, O_CREAT | O_TRUNC | O_WRONLY, 0600)) < 0) {
		warn("%s", to);
		(void)close(from_fd);
		return (1);
	}

	if (fchown(to_fd, sbp->st_uid, sbp->st_gid)) {
		serrno = errno;
		badchown = 1;
	}
	(void) fchmod(to_fd, sbp->st_mode & ~(S_ISUID|S_ISGID));

	if (!blen && !(bp = malloc(blen = sbp->st_blksize))) {
		warn(NULL);
		return (1);
	}
	while ((nread = read(from_fd, bp, blen)) > 0)
		if (write(to_fd, bp, nread) != nread) {
			warn("%s", to);
			goto err;
		}
	if (nread < 0) {
		warn("%s", from);
err:		if (unlink(to))
			warn("%s: remove", to);
		(void)close(from_fd);
		(void)close(to_fd);
		return (1);
	}
	(void)close(from_fd);

	if (badchown) {
		errno = serrno;
		if ((sbp->st_mode & (S_ISUID|S_ISGID)))  {
			warn("%s: set owner/group; not setting setuid/setgid",
			    to);
			sbp->st_mode &= ~(S_ISUID|S_ISGID);
		} else if (!fflg)
			warn("%s: set owner/group", to);
	}
	if (fchmod(to_fd, sbp->st_mode))
		warn("%s: set mode", to);

	tval[0].tv_sec = sbp->st_atime;
	tval[1].tv_sec = sbp->st_mtime;
	tval[0].tv_usec = tval[1].tv_usec = 0;
	if (utimes(to, tval))
		warn("%s: set times", to);

	if (close(to_fd)) {
		warn("%s", to);
		return (1);
	}

	if (unlink(from)) {
		warn("%s: remove", from);
		return (1);
	}
	return (0);
}

int
copy(from, to)
	char *from, *to;
{
	int pid, status;

	if ((pid = vfork()) == 0) {
		execl(_PATH_CP, "mv", "-PRp", from, to, NULL);
		warn("%s", _PATH_CP);
		_exit(1);
	}
	if (waitpid(pid, &status, 0) == -1) {
		warn("%s: waitpid", _PATH_CP);
		return (1);
	}
	if (!WIFEXITED(status)) {
		warn("%s: did not terminate normally", _PATH_CP);
		return (1);
	}
	if (WEXITSTATUS(status)) {
		warn("%s: terminated with %d (non-zero) status",
		    _PATH_CP, WEXITSTATUS(status));
		return (1);
	}
	if (!(pid = vfork())) {
		execl(_PATH_RM, "mv", "-rf", from, NULL);
		warn("%s", _PATH_RM);
		_exit(1);
	}
	if (waitpid(pid, &status, 0) == -1) {
		warn("%s: waitpid", _PATH_RM);
		return (1);
	}
	if (!WIFEXITED(status)) {
		warn("%s: did not terminate normally", _PATH_RM);
		return (1);
	}
	if (WEXITSTATUS(status)) {
		warn("%s: terminated with %d (non-zero) status",
		    _PATH_RM, WEXITSTATUS(status));
		return (1);
	}
	return (0);
}

void
usage()
{

	(void)fprintf(stderr, "usage: mv [-fi] source target\n");
	(void)fprintf(stderr, "       mv [-fi] source ... directory\n");
	exit(1);
}
