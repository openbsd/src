/*	$OpenBSD: column.c,v 1.26 2018/06/22 12:27:00 rob Exp $	*/
/*	$NetBSD: column.c,v 1.4 1995/09/02 05:53:03 jtc Exp $	*/

/*
 * Copyright (c) 1989, 1993, 1994
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
#include <sys/ioctl.h>

#include <ctype.h>
#include <err.h>
#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>

void  c_columnate(void);
void *ereallocarray(void *, size_t, size_t);
void  input(FILE *);
void  maketbl(void);
void  print(void);
void  r_columnate(void);
__dead void usage(void);

struct field {
	char *content;
	int width;
};

int termwidth;			/* default terminal width */
int entries;			/* number of records */
int eval;			/* exit value */
int *maxwidths;			/* longest record per column */
struct field **table;		/* one array of pointers per line */
wchar_t *separator = L"\t ";	/* field separator for table option */

int
main(int argc, char *argv[])
{
	struct winsize win;
	FILE *fp;
	int ch, tflag, xflag;
	char *p;
	const char *errstr;

	setlocale(LC_CTYPE, "");

	termwidth = 0;
	if ((p = getenv("COLUMNS")) != NULL)
		termwidth = strtonum(p, 1, INT_MAX, NULL);
	if (termwidth == 0 && ioctl(STDOUT_FILENO, TIOCGWINSZ, &win) == 0 &&
	    win.ws_col > 0)
		termwidth = win.ws_col;
	if (termwidth == 0)
		termwidth = 80;

	if (pledge("stdio rpath", NULL) == -1)
		err(1, "pledge");

	tflag = xflag = 0;
	while ((ch = getopt(argc, argv, "c:s:tx")) != -1) {
		switch(ch) {
		case 'c':
			termwidth = strtonum(optarg, 1, INT_MAX, &errstr);
			if (errstr != NULL)
				errx(1, "%s: %s", errstr, optarg);
			break;
		case 's':
			if ((separator = reallocarray(NULL, strlen(optarg) + 1,
			    sizeof(*separator))) == NULL)
				err(1, NULL);
			if (mbstowcs(separator, optarg, strlen(optarg) + 1) ==
			    (size_t) -1)
				err(1, "sep");
			break;
		case 't':
			tflag = 1;
			break;
		case 'x':
			xflag = 1;
			break;
		default:
			usage();
		}
	}

	if (!tflag)
		separator = L"";
	argv += optind;

	if (*argv == NULL) {
		input(stdin);
	} else {
		for (; *argv; ++argv) {
			if ((fp = fopen(*argv, "r"))) {
				input(fp);
				(void)fclose(fp);
			} else {
				warn("%s", *argv);
				eval = 1;
			}
		}
	}

	if (pledge("stdio", NULL) == -1)
		err(1, "pledge");

	if (!entries)
		return eval;

	if (tflag)
		maketbl();
	else if (*maxwidths >= termwidth)
		print();
	else if (xflag)
		c_columnate();
	else
		r_columnate();
	return eval;
}

#define	INCR_NEXTTAB(x)	(x = (x + 8) & ~7)
void
c_columnate(void)
{
	int col, numcols;
	struct field **row;

	INCR_NEXTTAB(*maxwidths);
	if ((numcols = termwidth / *maxwidths) == 0)
		numcols = 1;
	for (col = 0, row = table;; ++row) {
		fputs((*row)->content, stdout);
		if (!--entries)
			break;
		if (++col == numcols) {
			col = 0;
			putchar('\n');
		} else {
			while (INCR_NEXTTAB((*row)->width) <= *maxwidths)
				putchar('\t');
		}
	}
	putchar('\n');
}

void
r_columnate(void)
{
	int base, col, numcols, numrows, row;

	INCR_NEXTTAB(*maxwidths);
	if ((numcols = termwidth / *maxwidths) == 0)
		numcols = 1;
	numrows = entries / numcols;
	if (entries % numcols)
		++numrows;

	for (base = row = 0; row < numrows; base = ++row) {
		for (col = 0; col < numcols; ++col, base += numrows) {
			fputs(table[base]->content, stdout);
			if (base + numrows >= entries)
				break;
			while (INCR_NEXTTAB(table[base]->width) <= *maxwidths)
				putchar('\t');
		}
		putchar('\n');
	}
}

void
print(void)
{
	int row;

	for (row = 0; row < entries; row++)
		puts(table[row]->content);
}


void
maketbl(void)
{
	struct field **row;
	int col;

	for (row = table; entries--; ++row) {
		for (col = 0; (*row)[col + 1].content != NULL; ++col)
			printf("%s%*s  ", (*row)[col].content,
			    maxwidths[col] - (*row)[col].width, "");
		puts((*row)[col].content);
	}
}

#define	DEFNUM		1000
#define	DEFCOLS		25

void
input(FILE *fp)
{
	static int maxentry = 0;
	static int maxcols = 0;
	static struct field *cols = NULL;
	int col, width, twidth;
	size_t blen;
	ssize_t llen;
	char *p, *s, *buf = NULL;
	wchar_t wc;
	int wlen;

	while ((llen = getline(&buf, &blen, fp)) > -1) {
		if (buf[llen - 1] == '\n')
			buf[llen - 1] = '\0';

		p = buf;
		for (col = 0;; col++) {

			/* Skip lines containing nothing but whitespace. */

			for (s = p; (wlen = mbtowc(&wc, s, MB_CUR_MAX)) > 0;
			     s += wlen)
				if (!iswspace(wc))
					break;
			if (*s == '\0')
				break;

			/* Skip leading, multiple, and trailing separators. */

			while ((wlen = mbtowc(&wc, p, MB_CUR_MAX)) > 0 &&
			    wcschr(separator, wc) != NULL)
				p += wlen;
			if (*p == '\0')
				break;

			/*
			 * Found a non-empty field.
			 * Remember the start and measure the width.
			 */

			s = p;
			width = 0;
			while (*p != '\0') {
				if ((wlen = mbtowc(&wc, p, MB_CUR_MAX)) == -1) {
					width++;
					p++;
					continue;
				}
				if (wcschr(separator, wc) != NULL)
					break;
				if (*p == '\t')
					INCR_NEXTTAB(width);
				else  {
					width += (twidth = wcwidth(wc)) == -1 ?
					    1 : twidth;
				}
				p += wlen;
			}

			if (col + 1 >= maxcols) {
				if (maxcols > INT_MAX - DEFCOLS)
					err(1, "too many columns");
				maxcols += DEFCOLS;
				cols = ereallocarray(cols, maxcols,
				    sizeof(*cols));
				maxwidths = ereallocarray(maxwidths, maxcols,
				    sizeof(*maxwidths));
				memset(maxwidths + col, 0,
				    DEFCOLS * sizeof(*maxwidths));
			}

			/*
			 * Remember the width of the field,
			 * NUL-terminate and remeber the content,
			 * and advance beyond the separator, if any.
			 */

			cols[col].width = width;
			if (maxwidths[col] < width)
				maxwidths[col] = width;
			if (*p != '\0') {
				*p = '\0';
				p += wlen;
			}
			if ((cols[col].content = strdup(s)) == NULL)
				err(1, NULL);
		}
		if (col == 0)
			continue;

		/* Found a non-empty line; remember it. */

		if (entries == maxentry) {
			if (maxentry > INT_MAX - DEFNUM)
				errx(1, "too many input lines");
			maxentry += DEFNUM;
			table = ereallocarray(table, maxentry, sizeof(*table));
		}
		table[entries] = ereallocarray(NULL, col + 1,
		    sizeof(*(table[entries])));
		table[entries][col].content = NULL;
		while (col--)
			table[entries][col] = cols[col];
		entries++;
	}
}

void *
ereallocarray(void *ptr, size_t nmemb, size_t size)
{
	if ((ptr = reallocarray(ptr, nmemb, size)) == NULL)
		err(1, NULL);
	return ptr;
}

__dead void
usage(void)
{
	(void)fprintf(stderr,
	    "usage: column [-tx] [-c columns] [-s sep] [file ...]\n");
	exit(1);
}
