/*
 * $OpenBSD: readlink.c,v 1.10 1997/09/23 20:13:21 niklas Exp $
 *
 * Copyright (c) 1997
 *	Kenneth Stailey (hereinafter referred to as the author)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#include <limits.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

void canonicalize __P((const char *, char *));

int
main(argc, argv)
	int argc;
	char **argv;
{
	char buf[PATH_MAX];
	int n, ch, nflag = 0, fflag = 0;
	extern int optind;

	while ((ch = getopt(argc, argv, "fn")) != -1)
		switch (ch) {
		case 'f':
			fflag = 1;
			break;
		case 'n':
			nflag = 1;
			break;
		default:
			(void)fprintf(stderr,
			    "usage: readlink [-n] [-f] symlink\n");
			exit(1);
		}
	argc -= optind;
	argv += optind;

	if (argc != 1) {
		fprintf(stderr, "usage: readlink [-n] [-f] symlink\n");
		exit(1);
	}

	n = strlen(argv[0]);
	if (n > PATH_MAX - 1)
		errx(1, "filename longer than PATH_MAX-1 (%d)\n",
		    PATH_MAX - 1);

	if (fflag)
		canonicalize(argv[0], buf);
	else if ((n = readlink(argv[0], buf, PATH_MAX)) < 0)
		exit(1);

	printf("%s", buf);
	if (!nflag)
		putchar('\n');
	exit(0);
}

void
canonicalize(path, newpath)
	const char *path;
	char *newpath;
{
	int n;
	char *p, *np, *lp, c  ;
	char target[PATH_MAX];

	strcpy(newpath, path);
	for (;;) {
		p = np = newpath;

		/*
		 * If absolute path, skip the root slash now so we won't
		 * think of this as a NULL component.
		 */
		if (*p == '/')
			p++;

		/*
		 * loop through all components of the path until a link is
		 * found then expand it, if no link is found we are ready.
		 */
		for (; *p; lp = ++p) {
			while (*p && *p != '/')
				p++;
			c = *p;
			*p = '\0';
			n = readlink(newpath, target, PATH_MAX);
			*p = c;
			if (n > 0 || errno != EINVAL)
				break;
		}
		if (!*p && n < 0 && errno == EINVAL)
			break;
		if (n < 0)
			err(1, "%s", newpath);
		target[n] = '\0';
#ifdef DEBUG
		fprintf(stderr, "%.*s -> %s : ", p - newpath, newpath, target);
#endif
		if (*target == '/') {
			bcopy(p, newpath + n, strlen(p) + 1);
		        bcopy(target, newpath, n);
		} else {
			bcopy(p, lp + n, strlen(p) + 1);
			bcopy(target, lp, n);
		}
#ifdef DEBUG
		fprintf(stderr, "%s\n", newpath);
#endif
		strcpy(target, newpath);
		path = target;
	}
}
