/*	$OpenBSD: which.c,v 1.4 1998/05/07 19:12:20 deraadt Exp $	*/

/*
 * Copyright (c) 1997 Todd C. Miller <Todd.Miller@courtesan.com>
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
 *	This product includes software developed by Todd C. Miller.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef lint                                                              
static char rcsid[] = "$OpenBSD: which.c,v 1.4 1998/05/07 19:12:20 deraadt Exp $";
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

int findprog __P((char *, char *, int, int));
void usage __P((void));

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
main(argc, argv)
	int argc;
	char **argv;
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
findprog(prog, path, progmode, allmatches)
	char *prog;
	char *path;
	int progmode;
	int allmatches;
{
	char *p, filename[MAXPATHLEN];
	int proglen, plen, rval = 0;
	struct stat sbuf;

	/* Special case if prog contains '/' */
	if (strchr(prog, '/')) {
		if ((stat(prog, &sbuf) == 0) && S_ISREG(sbuf.st_mode) &&
		    access(prog, X_OK) == 0) {
			(void)puts(prog);
			return(1);
		} else {
			(void)printf("%s: Command not found.\n", prog);
			return(0);
		}
	}

	if ((path = strdup(path)) == NULL)
		errx(1, "Can't allocate memory.");

	proglen = strlen(prog);
	while ((p = strsep(&path, ":")) != NULL) {
		if (*p == '\0')
			p = ".";

		plen = strlen(p);
		while (p[plen-1] == '/')
			p[--plen] = '\0';	/* strip trailing '/' */

		if (plen + 1 + proglen >= sizeof(filename)) {
			warnx("%s/%s: %s", p, prog, strerror(ENAMETOOLONG));
			return(0);
		}

		(void)strcpy(filename, p);
		filename[plen] = '/';
		(void)strcpy(filename + plen + 1, prog);
		if ((stat(filename, &sbuf) == 0) && S_ISREG(sbuf.st_mode) &&
		    access(filename, X_OK) == 0) {
			(void)puts(filename);
			rval = 1;
			if (!allmatches)
				return(rval);
		}
	}
	(void)free(path);

	/* whereis(1) is silent on failure. */
	if (!rval && progmode != PROG_WHEREIS)
		(void)printf("%s: Command not found.\n", prog);
	return(rval);
}

void
usage()
{
	(void) fprintf(stderr, "Usage: %s [-a] name [...]\n", __progname);
	exit(1);
}
