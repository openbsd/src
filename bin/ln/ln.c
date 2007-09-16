/*	$OpenBSD: ln.c,v 1.16 2007/09/16 18:13:57 sobrado Exp $	*/
/*	$NetBSD: ln.c,v 1.10 1995/03/21 09:06:10 cgd Exp $	*/

/*
 * Copyright (c) 1987, 1993, 1994
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

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1987, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static const char sccsid[] = "@(#)ln.c	8.2 (Berkeley) 3/31/94";
#else
static const char rcsid[] = "$OpenBSD: ln.c,v 1.16 2007/09/16 18:13:57 sobrado Exp $";
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int	dirflag;			/* Undocumented directory flag. */
int	fflag;				/* Unlink existing files. */
int	hflag;				/* Check new name for symlink first. */
int	sflag;				/* Symbolic, not hard, link. */
					/* System link call. */
int (*linkf)(const char *, const char *);

int	linkit(char *, char *, int);
void	usage(void) __dead;

int
main(int argc, char *argv[])
{
	struct stat sb;
	int ch, exitval;
	char *sourcedir;

	while ((ch = getopt(argc, argv, "Ffhns")) != -1)
		switch (ch) {
		case 'F':
			dirflag = 1;	/* XXX: deliberately undocumented. */
			break;
		case 'f':
			fflag = 1;
			break;
		case 'h':
		case 'n':
			hflag = 1;
			break;
		case 's':
			sflag = 1;
			break;
		default:
			usage();
		}

	argv += optind;
	argc -= optind;

	linkf = sflag ? symlink : link;

	switch(argc) {
	case 0:
		usage();
	case 1:				/* ln target */
		exit(linkit(argv[0], ".", 1));
	case 2:				/* ln target source */
		exit(linkit(argv[0], argv[1], 0));
	}
					/* ln target1 target2 directory */
	sourcedir = argv[argc - 1];
	if (stat(sourcedir, &sb))
		err(1, "%s", sourcedir);
	if (!S_ISDIR(sb.st_mode))
		usage();
	for (exitval = 0; *argv != sourcedir; ++argv)
		exitval |= linkit(*argv, sourcedir, 1);
	exit(exitval);
}

 /*
  * Nomenclature warning!
  *
  * In this source "target" and "source" are used the opposite way they
  * are used in the ln(1) manual.  Here "target" is the existing file and
  * "source" specifies the to-be-created link to "target".
  */
int
linkit(char *target, char *source, int isdir)
{
	struct stat sb;
	char *p, path[MAXPATHLEN];
	int (*statf)(const char *, struct stat *);
	int exists, n;

	if (!sflag) {
		/* If target doesn't exist, quit now. */
		if (stat(target, &sb)) {
			warn("%s", target);
			return (1);
		}
		/* Only symbolic links to directories, unless -F option used. */
		if (!dirflag && S_ISDIR(sb.st_mode)) {
			errno = EISDIR;
			warn("%s", target);
			return (1);
		}
	}

	statf = hflag ? lstat : stat;

	/* If the source is a directory, append the target's name. */
	if (isdir || (!statf(source, &sb) && S_ISDIR(sb.st_mode))) {
		if ((p = basename(target)) == NULL) {
			warn("%s", target);
			return (1);
		}
		n = snprintf(path, sizeof(path), "%s/%s", source, p);
		if (n < 0 || n >= sizeof(path)) {
			errno = ENAMETOOLONG;
			warn("%s/%s", source, p);
			return (1);
		}
		source = path;
	}

	exists = (lstat(source, &sb) == 0);
	/*
	 * If doing hard links and the source (destination) exists and it
	 * actually is the same file like the target (existing file), we
	 * complain that the files are identical.  If -f is specified, we
	 * accept the job as already done and return with success.
	 */
	if (exists && !sflag) {
		struct stat tsb;

		if (stat(target, &tsb) != 0) {
			warn("%s: disappeared", target);
			return (1);
		}

		if (tsb.st_dev == sb.st_dev && tsb.st_ino == sb.st_ino) {
			warnx("%s and %s are identical (nothing done).",
			    target, source);
			if (fflag)
				return (0);
			else
				return (1);
		}
	}
	/*
	 * If the file exists, and -f was specified, unlink it.
	 * Attempt the link.
	 */
	if ((fflag && unlink(source) < 0 && errno != ENOENT) ||
	    (*linkf)(target, source)) {
		warn("%s", source);
		return (1);
	}

	return (0);
}

void
usage(void)
{
	extern char *__progname;

	(void)fprintf(stderr,
	    "usage: %s [-fhns] source [target]\n"
	    "       %s [-fs] source ... [directory]\n",
	    __progname, __progname);
	exit(1);
}
