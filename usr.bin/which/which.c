/*	$OpenBSD: which.c,v 1.14 2007/08/14 17:41:10 sobrado Exp $	*/

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

#ifndef lint                                                              
static const char rcsid[] = "$OpenBSD: which.c,v 1.14 2007/08/14 17:41:10 sobrado Exp $";
#endif /* not lint */                                                        

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/sysctl.h>

#include <err.h>
#include <errno.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

	if (argc == 1)
		usage();

	/* Don't accept command args but check since old whereis(1) used to */
	while ((ch = getopt(argc, argv, "a")) != -1) {
		switch (ch) {
		case 'a':
			allmatches = 1;
			break;
		default:
			usage();
		}
	}

	/*
	 * which(1) uses user's $PATH.
	 * whereis(1) uses user.cs_path from sysctl(3).
	 */
	if (strcmp(__progname, "whereis") == 0) {
		int mib[2];

		progmode = PROG_WHEREIS;
		mib[0] = CTL_USER;
		mib[1] = USER_CS_PATH;
		if (sysctl(mib, 2, NULL, &n, NULL, 0) == -1)
			err(1, "unable to get length of user.cs_path");
		if (n == 0)
			errx(1, "user.cs_path was zero length!");
		if ((path = (char *)malloc(n)) == NULL)
			errx(1, "can't allocate memory.");
		if (sysctl(mib, 2, path, &n, NULL, 0) == -1)
			err(1, "unable to get user.cs_path");
	} else {
		if ((path = getenv("PATH")) == NULL)
			err(1, "can't get $PATH from environment");
	}

	/* To make access(2) do what we want */
	if (setgid(getegid()))
		err(1, "Can't set gid to %u", getegid());
	if (setuid(geteuid()))
		err(1, "Can't set uid to %u", geteuid());

	for (n = optind; n < argc; n++)
		if (findprog(argv[n], path, progmode, allmatches) == 0)
			notfound++;

	exit((notfound == 0) ? 0 : ((notfound == argc - 1) ? 2 : 1));
}

int
findprog(char *prog, char *path, int progmode, int allmatches)
{
	char *p, filename[MAXPATHLEN];
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
			(void)printf("%s: Command not found.\n", prog);
			return (0);
		}
	}

	if ((path = strdup(path)) == NULL)
		errx(1, "Can't allocate memory.");
	pathcpy = path;

	proglen = strlen(prog);
	while ((p = strsep(&pathcpy, ":")) != NULL) {
		if (*p == '\0')
			p = ".";

		plen = strlen(p);
		while (p[plen-1] == '/')
			p[--plen] = '\0';	/* strip trailing '/' */

		if (plen + 1 + proglen >= sizeof(filename)) {
			warnx("%s/%s: %s", p, prog, strerror(ENAMETOOLONG));
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
		(void)printf("%s: Command not found.\n", prog);
	return (rval);
}

__dead void
usage(void)
{
	(void)fprintf(stderr, "usage: %s [-a] name ...\n", __progname);
	exit(1);
}
