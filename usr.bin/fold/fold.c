/*	$OpenBSD: fold.c,v 1.3 1999/02/07 21:40:49 deraadt Exp $	*/
/*	$NetBSD: fold.c,v 1.6 1995/09/01 01:42:44 jtc Exp $	*/

/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kevin Ruddy.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1990, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)fold.c	8.1 (Berkeley) 6/6/93";
#endif
static char rcsid[] = "$OpenBSD: fold.c,v 1.3 1999/02/07 21:40:49 deraadt Exp $";
#endif /* not lint */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <err.h>

#define	DEFLINEWIDTH	80

static void fold ();
static int new_column_position ();
int count_bytes = 0;
int split_words = 0;

int
main(argc, argv)
	int argc;
	char **argv;
{
	register int ch;
	int width;
	char *p;

	width = -1;
	while ((ch = getopt(argc, argv, "0123456789bsw:")) != -1)
		switch (ch) {
		case 'b':
			count_bytes = 1;
			break;
		case 's':
			split_words = 1;
			break;
		case 'w':
			if ((width = atoi(optarg)) <= 0) {
				(void)fprintf(stderr,
				    "fold: illegal width value.\n");
				exit(1);
			}
			break;
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			if (width == -1) {
				p = argv[optind - 1];
				if (p[0] == '-' && p[1] == ch && !p[2])
					width = atoi(++p);
				else
					width = atoi(argv[optind] + 1);
			}
			break;
		default:
			(void)fprintf(stderr,
			    "usage: fold [-bs] [-w width] [file ...]\n");
			exit(1);
		}
	argv += optind;
	argc -= optind;

	if (width == -1)
		width = DEFLINEWIDTH;

	if (!*argv)
		fold(width);
	else for (; *argv; ++argv)
		if (!freopen(*argv, "r", stdin)) {
			err (1, "%s", *argv);
			/* NOTREACHED */
		} else
			fold(width);
	exit(0);
}

/*
 * Fold the contents of standard input to fit within WIDTH columns
 * (or bytes) and write to standard output.
 *
 * If split_words is set, split the line at the last space character
 * on the line.  This flag necessitates storing the line in a buffer
 * until the current column > width, or a newline or EOF is read.
 *
 * The buffer can grow larger than WIDTH due to backspaces and carriage
 * returns embedded in the input stream.
 */
static void
fold(width)
	register int width;
{
	static char *buf = NULL;
	static int   buf_max = 0;
	register int ch, col;
	register int indx;

	col = indx = 0;
	while ((ch = getchar()) != EOF) {
		if (ch == '\n') {
			if (indx != 0)
				fwrite (buf, 1, indx, stdout);
			putchar('\n');
			col = indx = 0;
			continue;
		}

		col = new_column_position (col, ch);
		if (col > width) {
			int i, last_space;

			if (split_words) {
				for (i = 0, last_space = -1; i < indx; i++)
					if(buf[i] == ' ') last_space = i;
			}

			if (split_words && last_space != -1) {
				last_space++;

				fwrite (buf, 1, last_space, stdout);
				memmove (buf, buf+last_space, indx-last_space);

				indx -= last_space;
				col = 0;
				for (i = 0; i < indx; i++) {
					col = new_column_position (col, buf[i]);
				}
			} else {
				fwrite (buf, 1, indx, stdout);
				col = indx = 0;
			}
			putchar('\n');

			/* calculate the column position for the next line. */
			col = new_column_position (col, ch);
		}

		if (indx + 1 > buf_max) {
			/* Allocate buffer in LINE_MAX increments */
			buf_max += 2048;
			if((buf = realloc (buf, buf_max)) == NULL) {
				err (1, NULL);
				/* NOTREACHED */
			}
		}
		buf[indx++] = ch;
	}

	if (indx != 0)
		fwrite (buf, 1, indx, stdout);
}

/*
 * calculate the column position 
 */
static int
new_column_position (col, ch)
	int col;
	int ch;
{
	if (!count_bytes) {
		switch (ch) {
		case '\b':
			if (col > 0)
				--col;
			break;
		case '\r':
			col = 0;
			break;
		case '\t':
			col = (col + 8) & ~7;
			break;
		default:
			++col;
			break;
		}
	} else {
		++col;
	}

	return col;
}
