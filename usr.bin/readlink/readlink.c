/*
 * $OpenBSD: readlink.c,v 1.14 1998/05/18 19:12:03 deraadt Exp $
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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
		realpath(argv[0], buf);
	else if ((n = readlink(argv[0], buf, sizeof buf-1)) < 0)
		exit(1);
	buf[n] = '\0';

	printf("%s", buf);
	if (!nflag)
		putchar('\n');
	exit(0);
}
