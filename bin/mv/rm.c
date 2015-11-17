/*	$OpenBSD: rm.c,v 1.4 2015/11/17 19:09:06 tedu Exp $	*/
/*	$NetBSD: rm.c,v 1.19 1995/09/07 06:48:50 jtc Exp $	*/

/*-
 * Copyright (c) 1990, 1993, 1994
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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>

#include <locale.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <pwd.h>
#include <grp.h>

#define MAXIMUM(a, b)	(((a) > (b)) ? (a) : (b))

extern char *__progname;

static int eval, stdin_ok;

static int	check(char *, char *, struct stat *);
static void	checkdot(char **);
static void	rm_file(char **);
static void	rm_tree(char **);

static void __dead
usage(void)
{
	(void)fprintf(stderr, "usage: %s [-dfiPRr] file ...\n", __progname);
	exit(1);
}

/*
 * rm --
 *	This rm is different from historic rm's, but is expected to match
 *	POSIX 1003.2 behavior.  The most visible difference is that -f
 *	has two specific effects now, ignore non-existent files and force
 * 	file removal.
 */
int
rmmain(int argc, char *argv[])
{
	if (pledge("stdio rpath cpath", NULL) == -1)
		err(1, "pledge");

	checkdot(argv);

	if (*argv) {
		stdin_ok = isatty(STDIN_FILENO);

		rm_tree(argv);
	}

	return (eval);
}

static void
rm_tree(char **argv)
{
	FTS *fts;
	FTSENT *p;
	int flags;

	/*
	 * If the -i option is specified, the user can skip on the pre-order
	 * visit.  The fts_number field flags skipped directories.
	 */
#define	SKIPPED	1

	flags = FTS_PHYSICAL;
	flags |= FTS_NOSTAT;
	if (!(fts = fts_open(argv, flags, NULL)))
		err(1, NULL);
	while ((p = fts_read(fts)) != NULL) {
		switch (p->fts_info) {
		case FTS_DNR:
			if (p->fts_errno != ENOENT) {
				warnx("%s: %s",
				    p->fts_path, strerror(p->fts_errno));
				eval = 1;
			}
			continue;
		case FTS_ERR:
			errc(1, p->fts_errno, "%s", p->fts_path);
		case FTS_NS:
			/*
			 * FTS_NS: assume that if can't stat the file, it
			 * can't be unlinked.
			 */
			break;
		case FTS_D:
			/* Pre-order: give user chance to skip. */
			continue;
		case FTS_DP:
			/* Post-order: see if user skipped. */
			if (p->fts_number == SKIPPED)
				continue;
			break;
		default:
			break;
		}

		/*
		 * If we can't read or search the directory, may still be
		 * able to remove it.  Don't print out the un{read,search}able
		 * message unless the remove fails.
		 */
		switch (p->fts_info) {
		case FTS_DP:
		case FTS_DNR:
			if (!rmdir(p->fts_accpath) ||
			    (errno == ENOENT))
				continue;
			break;

		case FTS_F:
		case FTS_NSOK:
		default:
			if (!unlink(p->fts_accpath) ||
			    (errno == ENOENT))
				continue;
		}
		warn("%s", p->fts_path);
		eval = 1;
	}
	if (errno)
		err(1, "fts_read");
	fts_close(fts);
}

static void
rm_file(char **argv)
{
	struct stat sb;
	int rval;
	char *f;

	/*
	 * Remove a file.  POSIX 1003.2 states that, by default, attempting
	 * to remove a directory is an error, so must always stat the file.
	 */
	while ((f = *argv++) != NULL) {
		/* Assume if can't stat the file, can't unlink it. */
		if (lstat(f, &sb)) {
			if (errno != ENOENT) {
				warn("%s", f);
				eval = 1;
			}
			continue;
		}

		if (S_ISDIR(sb.st_mode)) {
			warnx("%s: is a directory", f);
			eval = 1;
			continue;
		}
		if (S_ISDIR(sb.st_mode))
			rval = rmdir(f);
		else {
			rval = unlink(f);
		}
		if (rval && (errno != ENOENT)) {
			warn("%s", f);
			eval = 1;
		}
	}
}

static int
check(char *path, char *name, struct stat *sp)
{
	int ch, first;
	char modep[15];

	/*
	 * If it's not a symbolic link and it's unwritable and we're
	 * talking to a terminal, ask.  Symbolic links are excluded
	 * because their permissions are meaningless.  Check stdin_ok
	 * first because we may not have stat'ed the file.
	 */
	if (!stdin_ok || S_ISLNK(sp->st_mode) || !access(name, W_OK) ||
	    errno != EACCES)
		return (1);
	strmode(sp->st_mode, modep);
	(void)fprintf(stderr, "override %s%s%s/%s for %s? ",
	    modep + 1, modep[9] == ' ' ? "" : " ",
	    user_from_uid(sp->st_uid, 0),
	    group_from_gid(sp->st_gid, 0), path);
	(void)fflush(stderr);

	first = ch = getchar();
	while (ch != '\n' && ch != EOF)
		ch = getchar();
	return (first == 'y' || first == 'Y');
}

/*
 * POSIX.2 requires that if "." or ".." are specified as the basename
 * portion of an operand, a diagnostic message be written to standard
 * error and nothing more be done with such operands.
 *
 * Since POSIX.2 defines basename as the final portion of a path after
 * trailing slashes have been removed, we'll remove them here.
 */
#define ISDOT(a)	((a)[0] == '.' && (!(a)[1] || ((a)[1] == '.' && !(a)[2])))
static void
checkdot(char **argv)
{
	char *p, **save, **t;
	int complained;

	complained = 0;
	for (t = argv; *t;) {
		/* strip trailing slashes */
		p = strrchr (*t, '\0');
		while (--p > *t && *p == '/')
			*p = '\0';

		/* extract basename */
		if ((p = strrchr(*t, '/')) != NULL)
			++p;
		else
			p = *t;

		if (ISDOT(p)) {
			if (!complained++)
				warnx("\".\" and \"..\" may not be removed");
			eval = 1;
			for (save = t; (t[0] = t[1]) != NULL; ++t)
				continue;
			t = save;
		} else
			++t;
	}
}
