/*
 * $OpenBSD: readlink.c,v 1.24 2007/09/10 07:42:26 sobrado Exp $
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

static void	usage(void);

int
main(int argc, char *argv[])
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
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();

	n = strlen(argv[0]);
	if (n > PATH_MAX - 1) {
		fprintf(stderr,
		    "readlink: filename longer than PATH_MAX-1 (%d)\n",
		    PATH_MAX - 1);
		exit(1);
	}

	if (fflag) {
		if (realpath(argv[0], buf) == NULL)
			err(1, "%s", argv[0]);
	} else {
		if ((n = readlink(argv[0], buf, sizeof buf-1)) < 0)
			exit(1);
		buf[n] = '\0';
	}

	printf("%s", buf);
	if (!nflag)
		putchar('\n');
	exit(0);
}

static void
usage(void)
{
	(void)fprintf(stderr, "usage: readlink [-fn] file\n");
	exit(1);
}
