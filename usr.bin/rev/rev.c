/*	$OpenBSD: rev.c,v 1.16 2022/02/08 17:44:18 cheloha Exp $	*/
/*	$NetBSD: rev.c,v 1.5 1995/09/28 08:49:40 tls Exp $	*/

/*-
 * Copyright (c) 1987, 1992, 1993
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

#include <sys/types.h>

#include <err.h>
#include <errno.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int multibyte;

int isu8cont(unsigned char);
int rev_file(const char *);
void usage(void);

int
main(int argc, char *argv[])
{
	int ch, rval;

	setlocale(LC_CTYPE, "");
	multibyte = MB_CUR_MAX > 1;

	if (pledge("stdio rpath", NULL) == -1)
		err(1, "pledge");

	while ((ch = getopt(argc, argv, "")) != -1)
		switch(ch) {
		default:
			usage();
		}

	argc -= optind;
	argv += optind;

	rval = 0;
	if (argc == 0) {
		if (pledge("stdio", NULL) == -1)
			err(1, "pledge");

		rval = rev_file(NULL);
	} else {
		for (; *argv != NULL; argv++)
			rval |= rev_file(*argv);
	}
	return rval;
}

int
isu8cont(unsigned char c)
{
	return (c & (0x80 | 0x40)) == 0x80;
}

int
rev_file(const char *path)
{
	char *p = NULL, *t, *te, *u;
	const char *filename;
	FILE *fp;
	size_t ps = 0;
	ssize_t len;
	int rval = 0;

	if (path != NULL) {
		fp = fopen(path, "r");
		if (fp == NULL) {
			warn("%s", path);
			return 1;
		}
		filename = path;
	} else {
		fp = stdin;
		filename = "stdin";
	}

	while ((len = getline(&p, &ps, fp)) != -1) {
		if (p[len - 1] == '\n')
			--len;
		for (t = p + len - 1; t >= p; --t) {
			te = t;
			if (multibyte)
				while (t > p && isu8cont(*t))
					--t;
			for (u = t; u <= te; ++u)
				if (putchar(*u) == EOF)
					err(1, "stdout");
		}
		if (putchar('\n') == EOF)
			err(1, "stdout");
	}
	free(p);
	if (ferror(fp)) {
		warn("%s", filename);
		rval = 1;
	}

	(void)fclose(fp);

	return rval;
}

void
usage(void)
{
	extern char *__progname;

	(void)fprintf(stderr, "usage: %s [file ...]\n", __progname);
	exit(1);
}
