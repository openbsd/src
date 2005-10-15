/*	$OpenBSD: chmod.c,v 1.19 2005/10/15 08:57:21 jmc Exp $	*/
/*	$NetBSD: chmod.c,v 1.12 1995/03/21 09:02:09 cgd Exp $	*/

/*
 * Copyright (c) 1989, 1993, 1994
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
static char copyright[] =
"@(#) Copyright (c) 1989, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)chmod.c	8.8 (Berkeley) 4/1/94";
#else
static char rcsid[] = "$OpenBSD: chmod.c,v 1.19 2005/10/15 08:57:21 jmc Exp $";
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fts.h>
#include <pwd.h>
#include <grp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <locale.h>

int ischflags, ischown, ischgrp, ischmod;
extern char *__progname;

gid_t a_gid(const char *);
uid_t a_uid(const char *, int);
void usage(void);

int
main(int argc, char *argv[])
{
	FTS *ftsp;
	FTSENT *p;
	void *set;
	long val;
	int oct, omode;
	int Hflag, Lflag, Pflag, Rflag, ch, fflag, fts_options, hflag, rval;
	uid_t uid;
	gid_t gid;
	u_int32_t fclear, fset;
	char *ep, *mode, *cp, *flags;
#ifdef lint
	set = NULL;
	oct = omode = 0;
#endif

	setlocale(LC_ALL, "");

	ischown = __progname[2] == 'o';
	ischgrp = __progname[2] == 'g';
	ischmod = __progname[2] == 'm';
	ischflags = __progname[2] == 'f';

	uid = -1;
	gid = -1;
	Hflag = Lflag = Pflag = Rflag = fflag = hflag = 0;
	while ((ch = getopt(argc, argv, "HLPRXfghorstuwx")) != -1)
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
		case 'f':		/* XXX: undocumented. */
			fflag = 1;
			break;
		case 'h':
			/*
			 * In System V (and probably POSIX.2) the -h option
			 * causes chmod to change the mode of the symbolic
			 * link.  4.4BSD's symbolic links don't have modes,
			 * so it's an undocumented noop.  Do syntax checking,
			 * though.
			 */
			hflag = 1;
			break;
		/*
		 * XXX
		 * "-[rwx]" are valid mode commands.  If they are the entire
		 * argument, getopt has moved past them, so decrement optind.
		 * Regardless, we're done argument processing.
		 */
		case 'g': case 'o': case 'r': case 's':
		case 't': case 'u': case 'w': case 'X': case 'x':
			if (!ischmod)
				usage();
			if (argv[optind - 1][0] == '-' &&
			    argv[optind - 1][1] == ch &&
			    argv[optind - 1][2] == '\0')
				--optind;
			goto done;
		default:
			usage();
		}
done:	argv += optind;
	argc -= optind;

	if (argc < 2)
		usage();

	fts_options = FTS_PHYSICAL;
	if (Rflag) {
		if (hflag)
			errx(1,
		"the -R and -h options may not be specified together.");
		if (Hflag)
			fts_options |= FTS_COMFOLLOW;
		if (Lflag) {
			fts_options &= ~FTS_PHYSICAL;
			fts_options |= FTS_LOGICAL;
		}
	}

	if (ischflags) {
		flags = *argv;
		if (*flags >= '0' && *flags <= '7') {
			errno = 0;
			val = strtoul(flags, &ep, 8);
			if (val > UINT_MAX)
				errno = ERANGE;
			if (errno)
				err(1, "invalid flags: %s", flags);
			if (*ep)
				errx(1, "invalid flags: %s", flags);
			fset = val;
			oct = 1;
		} else {
			if (strtofflags(&flags, &fset, &fclear))
				errx(1, "invalid flag: %s", flags);
			fclear = ~fclear;
			oct = 0;
		}
	} else if (ischmod) {
		mode = *argv;
		if (*mode >= '0' && *mode <= '7') {
			errno = 0;
			val = strtol(mode, &ep, 8);
			if (val > INT_MAX || val < 0)
				errno = ERANGE;
			if (errno)
				err(1, "invalid file mode: %s", mode);
			if (*ep)
				errx(1, "invalid file mode: %s", mode);
			omode = val;
			oct = 1;
		} else {
			if ((set = setmode(mode)) == NULL)
				errx(1, "invalid file mode: %s", mode);
			oct = 0;
		}
	} else if (ischown) {
		if ((cp = strchr(*argv, ':')) != NULL) {
			*cp++ = '\0';
			gid = a_gid(cp);
		}
#ifdef SUPPORT_DOT
		else if ((cp = strchr(*argv, '.')) != NULL &&
		    (uid = a_uid(*argv, 1)) == -1) {
			*cp++ = '\0';
			gid = a_gid(cp);
		}
#endif
		if (uid == -1)
			uid = a_uid(*argv, 0);
	} else
		gid = a_gid(*argv);

	if ((ftsp = fts_open(++argv, fts_options, 0)) == NULL)
		err(1, NULL);
	for (rval = 0; (p = fts_read(ftsp)) != NULL;) {
		switch (p->fts_info) {
		case FTS_D:
			if (!Rflag)
				fts_set(ftsp, p, FTS_SKIP);
			if (ischmod)
				break;
			else
				continue;
		case FTS_DNR:			/* Warn, chmod, continue. */
			warnx("%s: %s", p->fts_path, strerror(p->fts_errno));
			rval = 1;
			break;
		case FTS_DP:			/* Already changed at FTS_D. */
			if (ischmod)
				continue;
			else
				break;
		case FTS_ERR:			/* Warn, continue. */
		case FTS_NS:
			warnx("%s: %s", p->fts_path, strerror(p->fts_errno));
			rval = 1;
			continue;
		case FTS_SL:			/* Ignore. */
		case FTS_SLNONE:
			/*
			 * The only symlinks that end up here are ones that
			 * don't point to anything and ones that we found
			 * doing a physical walk.
			 */
			if (ischflags || ischmod || !hflag)
				continue;
		default:
			break;
		}
		if (ischflags) {
			if (oct) {
				if (!chflags(p->fts_accpath, fset))
					continue;
			} else {
				p->fts_statp->st_flags |= fset;
				p->fts_statp->st_flags &= fclear;
				if (!chflags(p->fts_accpath, p->fts_statp->st_flags))
					continue;
			}
			warn("%s", p->fts_path);
			rval = 1;
		} else if (ischmod && chmod(p->fts_accpath, oct ? omode :
		    getmode(set, p->fts_statp->st_mode)) && !fflag) {
			warn("%s", p->fts_path);
			rval = 1;
		} else if (!ischmod && !ischflags &&
		    (hflag ? lchown(p->fts_accpath, uid, gid) :
		    chown(p->fts_accpath, uid, gid)) && !fflag) {
			warn("%s", p->fts_path);
			rval = 1;
		}
	}
	if (errno)
		err(1, "fts_read");
	exit(rval);
}

uid_t
a_uid(const char *s, int silent)
{
	struct passwd *pw;
	char *ep;
	u_long ul;

	if (*s == '\0')			/* Argument was "gid[:.]". */
		return -1;

	if ((pw = getpwnam(s)) != NULL) {
		return pw->pw_uid;
	} else {
		if ((ul = strtoul(s, &ep, 10)) == ULONG_MAX) {
			if (silent)
				return -1;
			err(1, "%s", s);
		}
		if (*ep != '\0') {
			if (silent)
				return -1;
			errx(1, "%s: invalid user name", s);
		}
		/* XXX long -> int */
		return (uid_t)ul;
	}
}

gid_t
a_gid(const char *s)
{
	struct group *gr;
	char *ep;
	u_long ul;

	if (*s == '\0')			/* Argument was "gid[:.]". */
		return -1;

	if ((gr = getgrnam(s)) != NULL) {
		return gr->gr_gid;
	} else {
		if ((ul = strtoul(s, &ep, 10)) == ULONG_MAX)
			err(1, "%s", s);
		if (*ep != '\0')
			errx(1, "%s: invalid group name", s);
		/* XXX long -> int */
		return (gid_t)ul;
	}
}

void
usage(void)
{
	if (ischmod || ischflags)
		fprintf(stderr,
		    "usage: %s [-R [-H | -L | -P]] %s file ...\n",
		    __progname, (ischmod? "mode" : "flags"));
	else
		fprintf(stderr,
		    "usage: %s [-fh] [-R [-H | -L | -P]] %s file ...\n",
		    __progname, ischown ? "[owner][:group]" : "group");
	exit(1);
}
