/*
 * Copyright (c) 1993 Winning Strategies, Inc.
 * All rights reserved.
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
 *      This product includes software developed by Winning Strategies, Inc.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef lint
static char rcsid[] = "$Id: catman.c,v 1.1.1.1 1995/10/18 08:47:29 deraadt Exp $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <paths.h>

#include "pathnames.h"

int f_nowhatis;
int f_noaction;
int f_noformat;
int f_ignerr;
int f_noprint;

int dowhatis;

char *mp = _PATH_MAN;
char *sp = _MAN_SECTIONS;

void usage __P((void));
void catman __P((const char *, char *));
void makewhatis __P((const char *));
void dosystem __P((const char *));

int
main(argc, argv)
	int argc;
	char **argv;
{
	int c;

	while ((c = getopt(argc, argv, "knpswM:")) != EOF) {
		switch (c) {
		case 'k':
			f_ignerr = 1;
			break;
		case 'n':
			f_nowhatis = 1;
			break;
		case 'p':
			f_noaction = 1;
			break;
		case 's':
			f_noprint = 1;
			break;
		case 'w':
			f_noformat = 1;
			break;
		case 'M':
			mp = optarg;
			break;

		case '?':
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (f_noprint && f_noaction)
		f_noprint = 0;

	if (argc > 1)
		usage();
	if (argc == 1)
		sp = *argv;

	if (f_noformat == 0 || f_nowhatis == 0)
		catman(mp, sp);
	if (f_nowhatis == 0 && dowhatis)
		makewhatis(mp);

	exit(0);
}


void
catman(path, section)
	const char *path;
	char *section;
{
	char mandir[PATH_MAX];
	char catdir[PATH_MAX];
	char manpage[PATH_MAX];
	char catpage[PATH_MAX];
	char sysbuf[1024];
	struct stat manstat;
	struct stat catstat;
	struct dirent *dp;
	DIR *dirp;
	char *s, *tmp;
	int sectlen, error;

	for (s = section; *s; s += sectlen) {
#ifdef notdef
		tmp = s;
		sectlen = 0;
		if (isdigit(*tmp)) {
			sectlen++;
			tmp++;
			while (*tmp && isdigit(*tmp) == 0) {
				sectlen++;
				tmp++;
			}
		}
#else
		sectlen = 1;
#endif
		if (sectlen == 0)
			errx(1, "malformed section string");

		sprintf(mandir, "%s/%s%.*s", path, _PATH_MANPAGES, sectlen, s);
		sprintf(catdir, "%s/%s%.*s", path, _PATH_CATPAGES, sectlen, s);

		if ((dirp = opendir(mandir)) == 0) {
			warn("can't open %s", mandir);
			continue;
		}

		if (stat(catdir, &catstat) < 0) {
			if (errno != ENOENT) {
				warn("can't stat %s", catdir);
				closedir(dirp);
				continue;
			}
			if (f_noprint == 0)
				printf("mkdir %s\n", catdir);
			if (f_noaction == 0 && mkdir(catdir, 0755) < 0) {
				warn("can't create %s", catdir);
				closedir(dirp);
				return;
			}

		}

		while ((dp = readdir(dirp)) != NULL) {
			if (strcmp(dp->d_name, ".") == 0 ||
			    strcmp(dp->d_name, "..") == 0)
				continue;

			sprintf(manpage, "%s/%s", mandir, dp->d_name);
			sprintf(catpage, "%s/%s", catdir, dp->d_name);
			if ((tmp = strrchr(catpage, '.')) != NULL)
				strcpy(tmp, ".0");
			else
				continue;

			if (stat(manpage, &manstat) < 0) {
				warn("can't stat %s", manpage);
				continue;
			}

			if (!S_ISREG(manstat.st_mode)) {
				warnx("not a regular file %s", manpage);
				continue;
			}
			if ((error = stat(catpage, &catstat)) &&
			    errno != ENOENT) {
				warn("can't stat %s", catpage);
				continue;
			}

			if ((error && errno == ENOENT) || 
			    manstat.st_mtime >= catstat.st_mtime) {
				if (f_noformat)
					dowhatis = 1;
				else {
					/*
					 * manpage is out of date,
					 * reformat
					 */
					sprintf(sysbuf, "nroff -mandoc %s > %s",
					    manpage, catpage);
					if (f_noprint == 0)
						printf("%s\n", sysbuf);
					if (f_noaction == 0)
						dosystem(sysbuf);
					dowhatis = 1;
				}
			}
		}
		closedir(dirp);
	}
}

void
makewhatis(path)
	const char *path;
{
	char sysbuf[1024];

	sprintf(sysbuf, "%s %s", _PATH_MAKEWHATIS, path);
	if (f_noprint == 0)
		printf("%s\n", sysbuf);
	if (f_noaction == 0)
		dosystem(sysbuf);
}

void
dosystem(cmd)
	const char *cmd;
{
	int status;

	if ((status = system(cmd)) == 0)
		return;

	if (status == -1)
		err(1, "cannot execute action");
	if (WIFSIGNALED(status))
		errx(1, "child was signaled to quit. aborting");
	if (WIFSTOPPED(status))
		errx(1, "child was stopped. aborting");
	if (f_ignerr == 0)
		errx(1,"*** Exited %d");
	warnx("*** Exited %d (continuing)");
}

void
usage()
{
	(void)fprintf(stderr,
	    "usage: catman [-knpsw] [-M manpath] [sections]\n");
	exit(1);
}
