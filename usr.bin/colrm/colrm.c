/*	$OpenBSD: colrm.c,v 1.14 2022/12/04 23:50:47 cheloha Exp $	*/
/*	$NetBSD: colrm.c,v 1.4 1995/09/02 05:51:37 jtc Exp $	*/

/*-
 * Copyright (c) 1991, 1993
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
#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>

#define	TAB	8

void usage(void);

int
main(int argc, char *argv[])
{
	char	 *line, *p;
	ssize_t	  linesz;
	wchar_t	  wc;
	u_long	  column, newcol, start, stop;
	int	  ch, len, width;

	setlocale(LC_CTYPE, "");

	if (pledge("stdio", NULL) == -1)
		err(1, "pledge");

	while ((ch = getopt(argc, argv, "")) != -1)
		switch(ch) {
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	start = stop = 0;
	switch(argc) {
	case 2:
		stop = strtol(argv[1], &p, 10);
		if (stop <= 0 || *p)
			errx(1, "illegal column -- %s", argv[1]);
		/* FALLTHROUGH */
	case 1:
		start = strtol(argv[0], &p, 10);
		if (start <= 0 || *p)
			errx(1, "illegal column -- %s", argv[0]);
		break;
	case 0:
		break;
	default:
		usage();
	}

	if (stop && start > stop)
		err(1, "illegal start and stop columns");

	line = NULL;
	while (getline(&line, &linesz, stdin) != -1) {
		column = 0;
		width = 0;
		for (p = line; *p != '\0'; p += len) {
			len = 1;
			switch (*p) {
			case '\n':
				putchar('\n');
				continue;
			case '\b':
				/*
				 * Pass it through if the previous character
				 * was in scope, still represented by the
				 * current value of "column".
				 * Allow an optional second backspace
				 * after a double-width character.
				 */
				if (start == 0 || column < start ||
				    (stop > 0 &&
				     column > stop + (width > 1))) {
					putchar('\b');
					if (width > 1 && p[1] == '\b')
						putchar('\b');
				} 
				if (width > 1 && p[1] == '\b')
					p++;
				column -= width;
				continue;
			case '\t':
				newcol = (column + TAB) & ~(TAB - 1);
				if (start == 0 || newcol < start) {
					putchar('\t');
					column = newcol;
				} else
					/*
					 * Expand tabs that intersect or
					 * follow deleted columns.
					 */
					while (column < newcol)
						if (++column < start ||
						    (stop > 0 &&
						     column > stop))
				 			putchar(' ');
				continue;
			default:
				break;
			}

			/*
			 * Handle the three cases of invalid bytes,
			 * non-printable, and printable characters.
			 */

			if ((len = mbtowc(&wc, p, MB_CUR_MAX)) == -1) {
				(void)mbtowc(NULL, NULL, MB_CUR_MAX);
				len = 1;
				width = 1;
			} else if ((width = wcwidth(wc)) == -1)
				width = 1;

			/*
			 * If the character completely fits before or
			 * after the cut, keep it; otherwise, skip it.
			 */

			if ((start == 0 || column + width < start ||
			    (stop > 0 && column + (width > 0) > stop)))
			    	fwrite(p, 1, len, stdout);

			/*
			 * If the cut cuts the character in half
			 * and no backspace follows,
			 * print a blank for correct columnation.
			 */

			else if (width > 1 && p[len] != '\b' &&
			    (start == 0 || column + 1 < start ||
			    (stop > 0 && column + width > stop)))
				putchar(' ');

			column += width;
		}
	}
	if (ferror(stdin))
		err(1, "stdin");
	if (ferror(stdout))
		err(1, "stdout");
	return 0;
}

void
usage(void)
{
	(void)fprintf(stderr, "usage: colrm [start [stop]]\n");
	exit(1);
}
