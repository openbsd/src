/*	$OpenBSD: head.c,v 1.24 2022/02/07 17:19:57 cheloha Exp $	*/

/*
 * Copyright (c) 1980, 1987 Regents of the University of California.
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

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include <err.h>
#include <errno.h>
#include <unistd.h>

int head_file(const char *, long, int);
static void usage(void);

/*
 * head - give the first few lines of a stream or of each of a set of files
 *
 * Bill Joy UCB August 24, 1977
 */

int
main(int argc, char *argv[])
{
	const char *errstr;
	int	ch;
	long	linecnt = 10;
	int	status = 0;

	if (pledge("stdio rpath", NULL) == -1)
		err(1, "pledge");

	/* handle obsolete -number syntax */
	if (argc > 1 && argv[1][0] == '-' &&
	    isdigit((unsigned char)argv[1][1])) {
		linecnt = strtonum(argv[1] + 1, 1, LONG_MAX, &errstr);
		if (errstr != NULL)
			errx(1, "count is %s: %s", errstr, argv[1] + 1);
		argc--;
		argv++;
	}

	while ((ch = getopt(argc, argv, "n:")) != -1) {
		switch (ch) {
		case 'n':
			linecnt = strtonum(optarg, 1, LONG_MAX, &errstr);
			if (errstr != NULL)
				errx(1, "count is %s: %s", errstr, optarg);
			break;
		default:
			usage();
		}
	}
	argc -= optind, argv += optind;

	if (argc == 0) {
		if (pledge("stdio", NULL) == -1)
			err(1, "pledge");

		status = head_file(NULL, linecnt, 0);
	} else {
		for (; *argv != NULL; argv++)
			status |= head_file(*argv, linecnt, argc > 1);
	}

	return status;
}

int
head_file(const char *path, long count, int need_header)
{
	const char *name;
	FILE *fp;
	int ch, status = 0;
	static int first = 1;

	if (path != NULL) {
		name = path;
		fp = fopen(name, "r");
		if (fp == NULL) {
			warn("%s", name);
			return 1;
		}
		if (need_header) {
			printf("%s==> %s <==\n", first ? "" : "\n", name);
			if (ferror(stdout))
				err(1, "stdout");
			first = 0;
		}
	} else {
		name = "stdin";
		fp = stdin;
	}

	while ((ch = getc(fp)) != EOF) {
		if (putchar(ch) == EOF)
			err(1, "stdout");
		if (ch == '\n' && --count == 0)
			break;
	}
	if (ferror(fp)) {
		warn("%s", name);
		status = 1;
	}

	fclose(fp);

	return status;
}


static void
usage(void)
{
	fputs("usage: head [-count | -n count] [file ...]\n", stderr);
	exit(1);
}
