/*	$OpenBSD: which.c,v 1.20 2015/01/16 06:40:14 deraadt Exp $	*/

/*
 * Copyright (c) 1997 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/stat.h>
#include <sys/sysctl.h>

#include <err.h>
#include <errno.h>
#include <locale.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#define PROG_WHICH	1
#define PROG_WHEREIS	2

extern char *__progname;

int findprog(char *, char *, int, int);
__dead void usage(void);

/*
 * which(1) -- find an executable(s) in the user's path
 * whereis(1) -- find an executable(s) in the default user path
 *
 * Return values:
 *	0 - all executables found
 *	1 - some found, some not
 *	2 - none found
 */

int
main(int argc, char *argv[])
{
	char *path;
	size_t n;
	int ch, allmatches = 0, notfound = 0, progmode = PROG_WHICH;

	(void)setlocale(LC_ALL, "");

	while ((ch = getopt(argc, argv, "a")) != -1)
		switch (ch) {
		case 'a':
			allmatches = 1;
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc == 0)
		usage();

	if (strcmp(__progname, "whereis") == 0) {
		progmode = PROG_WHEREIS;
		path = _PATH_STDPATH;
	} else {
		if ((path = getenv("PATH")) == NULL)
			err(1, "can't get $PATH from environment");
	}

	/* To make access(2) do what we want */
	if (setgid(getegid()))
		err(1, "Can't set gid to %u", getegid());
	if (setuid(geteuid()))
		err(1, "Can't set uid to %u", geteuid());

	for (n = 0; n < argc; n++)
		if (findprog(argv[n], path, progmode, allmatches) == 0)
			notfound++;

	exit((notfound == 0) ? 0 : ((notfound == argc) ? 2 : 1));
}

int
findprog(char *prog, char *path, int progmode, int allmatches)
{
	char *p, filename[PATH_MAX];
	int proglen, plen, rval = 0;
	struct stat sbuf;
	char *pathcpy;

	/* Special case if prog contains '/' */
	if (strchr(prog, '/')) {
		if ((stat(prog, &sbuf) == 0) && S_ISREG(sbuf.st_mode) &&
		    access(prog, X_OK) == 0) {
			(void)puts(prog);
			return (1);
		} else {
			warnx("%s: Command not found.", prog);
			return (0);
		}
	}

	if ((path = strdup(path)) == NULL)
		err(1, "strdup");
	pathcpy = path;

	proglen = strlen(prog);
	while ((p = strsep(&pathcpy, ":")) != NULL) {
		if (*p == '\0')
			p = ".";

		plen = strlen(p);
		while (p[plen-1] == '/')
			p[--plen] = '\0';	/* strip trailing '/' */

		if (plen + 1 + proglen >= sizeof(filename)) {
			warnc(ENAMETOOLONG, "%s/%s", p, prog);
			free(path);
			return (0);
		}

		snprintf(filename, sizeof(filename), "%s/%s", p, prog);
		if ((stat(filename, &sbuf) == 0) && S_ISREG(sbuf.st_mode) &&
		    access(filename, X_OK) == 0) {
			(void)puts(filename);
			rval = 1;
			if (!allmatches) {
				free(path);
				return (rval);
			}
		}
	}
	(void)free(path);

	/* whereis(1) is silent on failure. */
	if (!rval && progmode != PROG_WHEREIS)
		warnx("%s: Command not found.", prog);
	return (rval);
}

__dead void
usage(void)
{
	(void)fprintf(stderr, "usage: %s [-a] name ...\n", __progname);
	exit(1);
}
