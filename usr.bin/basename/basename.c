/*	$OpenBSD: basename.c,v 1.9 2009/10/27 23:59:36 deraadt Exp $	*/
/*	$NetBSD: basename.c,v 1.9 1995/09/02 05:29:46 jtc Exp $	*/

/*-
 * Copyright (c) 1991, 1993, 1994
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

#include <err.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <unistd.h>

void usage(void);

int
main(int argc, char *argv[])
{
	int ch;
	char *p;

	setlocale(LC_ALL, "");

	while ((ch = getopt(argc, argv, "")) != -1) {
		switch (ch) {
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1 && argc != 2)
		usage();

	if (**argv == '\0') {
		(void)puts("");
		exit(0);
	}
	p = basename(*argv);
	if (p == NULL)
		err(1, "%s", *argv);
	/*
	 * If the suffix operand is present, is not identical to the
	 * characters remaining in string, and is identical to a suffix
	 * of the characters remaining in string, the suffix suffix
	 * shall be removed from string.
	 */
	if (*++argv) {
		size_t suffixlen, stringlen, off;

		suffixlen = strlen(*argv);
		stringlen = strlen(p);

		if (suffixlen < stringlen) {
			off = stringlen - suffixlen;
			if (!strcmp(p + off, *argv))
				p[off] = '\0';
		}
	}
	(void)puts(p);
	exit(0);
}

extern char *__progname;
void
usage(void)
{

	(void)fprintf(stderr, "usage: %s string [suffix]\n", __progname);
	exit(1);
}
