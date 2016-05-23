/*	$OpenBSD: fold.c,v 1.18 2016/05/23 10:31:42 schwarze Exp $	*/
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

#include <ctype.h>
#include <err.h>
#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>

#define	DEFLINEWIDTH	80

static void fold(unsigned int);
static int isu8cont(unsigned char);
static __dead void usage(void);

int count_bytes = 0;
int split_words = 0;

int
main(int argc, char *argv[])
{
	int ch, lastch, newarg, prevoptind;
	unsigned int width;
	const char *errstr;

	setlocale(LC_CTYPE, "");

	if (pledge("stdio rpath", NULL) == -1)
		err(1, "pledge");

	width = 0;
	lastch = '\0';
	prevoptind = 1;
	newarg = 1;
	while ((ch = getopt(argc, argv, "0123456789bsw:")) != -1) {
		switch (ch) {
		case 'b':
			count_bytes = 1;
			break;
		case 's':
			split_words = 1;
			break;
		case 'w':
			width = strtonum(optarg, 1, UINT_MAX, &errstr);
			if (errstr != NULL)
				errx(1, "illegal width value, %s: %s", errstr, 
					optarg);
			break;
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			if (newarg)
				width = 0;
			else if (!isdigit(lastch))
				usage();
			if (width > UINT_MAX / 10 - 1)
				errx(1, "illegal width value, too large");
			width = (width * 10) + (ch - '0');
			if (width < 1)
				errx(1, "illegal width value, too small");
			break;
		default:
			usage();
		}
		lastch = ch;
		newarg = optind != prevoptind;
		prevoptind = optind;
	}
	argv += optind;
	argc -= optind;

	if (width == 0)
		width = DEFLINEWIDTH;

	if (!*argv) {
		if (pledge("stdio", NULL) == -1)
			err(1, "pledge");
		fold(width);
	} else {
		for (; *argv; ++argv) {
			if (!freopen(*argv, "r", stdin))
				err(1, "%s", *argv);
			else
				fold(width);
		}
	}
	return 0;
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
fold(unsigned int max_width)
{
	static char	*buf = NULL;
	static size_t	 bufsz = 2048;
	char		*cp;	/* Current mb character. */
	char		*np;	/* Next mb character. */
	char		*sp;	/* To search for the last space. */
	char		*nbuf;	/* For buffer reallocation. */
	wchar_t		 wc;	/* Current wide character. */
	int		 ch;	/* Last byte read. */
	int		 len;	/* Bytes in the current mb character. */
	unsigned int	 col;	/* Current display position. */
	int		 width; /* Display width of wc. */

	if (buf == NULL && (buf = malloc(bufsz)) == NULL)
		err(1, NULL);

	np = cp = buf;
	ch = 0;
	col = 0;

	while (ch != EOF) {  /* Loop on input characters. */
		while ((ch = getchar()) != EOF) {  /* Loop on input bytes. */
			if (np + 1 == buf + bufsz) {
				nbuf = reallocarray(buf, 2, bufsz);
				if (nbuf == NULL)
					err(1, NULL);
				bufsz *= 2;
				cp = nbuf + (cp - buf);
				np = nbuf + (np - buf);
				buf = nbuf;
			}
			*np++ = ch;

			/*
			 * Read up to and including the first byte of
			 * the next character, such that we are sure
			 * to have a complete character in the buffer.
			 * There is no need to read more than five bytes
			 * ahead, since UTF-8 characters are four bytes
			 * long at most.
			 */

			if (np - cp > 4 || (np - cp > 1 && !isu8cont(ch)))
				break;
		}

		while (cp < np) {  /* Loop on output characters. */

			/* Handle end of line and backspace. */

			if (*cp == '\n' || (*cp == '\r' && !count_bytes)) {
				fwrite(buf, 1, ++cp - buf, stdout);
				memmove(buf, cp, np - cp);
				np = buf + (np - cp);
				cp = buf;
				col = 0;
				continue;
			}
			if (*cp == '\b' && !count_bytes) {
				if (col)
					col--;
				cp++;
				continue;
			}

			/*
			 * Measure display width.
			 * Process the last byte only if
			 * end of file was reached.
			 */

			if (np - cp > (ch != EOF)) {
				len = 1;
				width = 1;

				if (*cp == '\t') {
					if (count_bytes == 0)
						width = 8 - (col & 7);
				} else if ((len = mbtowc(&wc, cp,
				    np - cp)) < 1)
					len = 1;
				else if (count_bytes)
					width = len;
				else if ((width = wcwidth(wc)) < 0)
					width = 1;

				col += width;
				if (col <= max_width || cp == buf) {
					cp += len;
					continue;
				}
			}

			/* Line break required. */

			if (col > max_width) {
				if (split_words) {
					for (sp = cp; sp > buf; sp--) {
						if (sp[-1] == ' ') {
							cp = sp;
							break;
						}
					}
				}
				fwrite(buf, 1, cp - buf, stdout);
				putchar('\n');
				memmove(buf, cp, np - cp);
				np = buf + (np - cp);
				cp = buf;
				col = 0;
				continue;
			}

			/* Need more input. */

			break;
		}
	}
	fwrite(buf, 1, np - buf, stdout);

	if (ferror(stdin))
		err(1, NULL);
}

static int
isu8cont(unsigned char c)
{
	return MB_CUR_MAX > 1 && (c & (0x80 | 0x40)) == 0x80;
}

static __dead void
usage(void)
{
	(void)fprintf(stderr, "usage: fold [-bs] [-w width] [file ...]\n");
	exit(1);
}
