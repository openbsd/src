/*	$OpenBSD: rev.c,v 1.13 2016/04/10 17:06:52 martijn Exp $	*/
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

int isu8cont(unsigned char);
void usage(void);

int
main(int argc, char *argv[])
{
	char *filename, *p = NULL, *t, *u;
	FILE *fp;
	ssize_t len;
	size_t ps = 0;
	int ch, rval;

	setlocale(LC_CTYPE, "");

	if (pledge("stdio rpath", NULL) == -1)
		err(1, "pledge");

	while ((ch = getopt(argc, argv, "")) != -1)
		switch(ch) {
		default:
			usage();
		}

	argc -= optind;
	argv += optind;

	fp = stdin;
	filename = "stdin";
	rval = 0;
	do {
		if (*argv) {
			if ((fp = fopen(*argv, "r")) == NULL) {
				warn("%s", *argv);
				rval = 1;
				++argv;
				continue;
			}
			filename = *argv++;
		}
		while ((len = getline(&p, &ps, fp)) != -1) {
			if (p[len - 1] == '\n')
				--len;
			for (t = p + len - 1; t >= p; --t) {
				if (isu8cont(*t))
					continue;
				u = t;
				do {
					putchar(*u);
				} while (isu8cont(*(++u)));
			}
			putchar('\n');
		}
		if (ferror(fp)) {
			warn("%s", filename);
			rval = 1;
		}
		(void)fclose(fp);
	} while(*argv);
	return rval;
}

int
isu8cont(unsigned char c)
{
	return MB_CUR_MAX > 1 && (c & (0x80 | 0x40)) == 0x80;
}

void
usage(void)
{
	extern char *__progname;

	(void)fprintf(stderr, "usage: %s [file ...]\n", __progname);
	exit(1);
}
