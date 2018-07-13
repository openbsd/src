/*	$OpenBSD: cut.c,v 1.25 2018/07/13 08:51:14 krw Exp $	*/
/*	$NetBSD: cut.c,v 1.9 1995/09/02 05:59:23 jtc Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam S. Moskowitz of Menlo Consulting and Marciano Pitargue.
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

#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char	dchar[5];
int	dlen;

int	bflag;
int	cflag;
int	dflag;
int	fflag;
int	nflag;
int	sflag;

void	b_cut(FILE *, char *);
void	c_cut(FILE *, char *);
void	f_cut(FILE *, char *);
void	get_list(char *);
void	usage(void);

int
main(int argc, char *argv[])
{
	FILE *fp;
	void (*fcn)(FILE *, char *);
	int ch, rval;

	setlocale(LC_CTYPE, "");

	if (pledge("stdio rpath", NULL) == -1)
		err(1, "pledge");

	dchar[0] = '\t';		/* default delimiter */
	dchar[1] = '\0';
	dlen = 1;

	while ((ch = getopt(argc, argv, "b:c:d:f:sn")) != -1)
		switch(ch) {
		case 'b':
			get_list(optarg);
			bflag = 1;
			break;
		case 'c':
			get_list(optarg);
			cflag = 1;
			break;
		case 'd':
			if ((dlen = mblen(optarg, MB_CUR_MAX)) == -1)
				usage();
			assert(dlen < sizeof(dchar));
			(void)memcpy(dchar, optarg, dlen);
			dchar[dlen] = '\0';
			dflag = 1;
			break;
		case 'f':
			get_list(optarg);
			fflag = 1;
			break;
		case 'n':
			nflag = 1;
			break;
		case 's':
			sflag = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (bflag + cflag + fflag != 1 ||
	    (nflag && !bflag) ||
	    ((dflag || sflag) && !fflag))
		usage();

	if (MB_CUR_MAX == 1) {
		nflag = 0;
		if (cflag) {
			bflag = 1;
			cflag = 0;
		}
	}

	fcn = fflag ? f_cut : (cflag || nflag) ? c_cut : b_cut;

	rval = 0;
	if (*argv)
		for (; *argv; ++argv) {
			if (strcmp(*argv, "-") == 0)
				fcn(stdin, "stdin");
			else {
				if ((fp = fopen(*argv, "r"))) {
					fcn(fp, *argv);
					(void)fclose(fp);
				} else {
					rval = 1;
					warn("%s", *argv);
				}
			}
		}
	else {
		if (pledge("stdio rpath", NULL) == -1)
			err(1, "pledge");

		fcn(stdin, "stdin");
	}
	exit(rval);
}

int autostart, autostop, maxval;

char positions[_POSIX2_LINE_MAX + 1];

int
read_number(char **p)
{
	int dash, n;
	const char *errstr;
	char *q;

	q = *p + strcspn(*p, "-");
	dash = *q == '-';
	*q = '\0';
	n = strtonum(*p, 1, _POSIX2_LINE_MAX, &errstr);
	if (errstr != NULL)
		errx(1, "[-bcf] list: %s %s (allowed 1-%d)", *p, errstr,
		    _POSIX2_LINE_MAX);
	if (dash)
		*q = '-';
	*p = q;

	return n;
}

void
get_list(char *list)
{
	int setautostart, start, stop;
	char *p;

	/*
	 * set a byte in the positions array to indicate if a field or
	 * column is to be selected; use +1, it's 1-based, not 0-based.
	 * This parser is less restrictive than the Draft 9 POSIX spec.
	 * POSIX doesn't allow lists that aren't in increasing order or
	 * overlapping lists.  We also handle "-3-5" although there's no
	 * real reason too.
	 */
	while ((p = strsep(&list, ", \t"))) {
		setautostart = start = stop = 0;
		if (*p == '-') {
			++p;
			setautostart = 1;
		}
		if (isdigit((unsigned char)*p)) {
			start = stop = read_number(&p);
			if (setautostart && start > autostart)
				autostart = start;
		}
		if (*p == '-') {
			if (isdigit((unsigned char)p[1])) {
				++p;
				stop = read_number(&p);
			}
			if (*p == '-') {
				++p;
				if (!autostop || autostop > stop)
					autostop = stop;
			}
		}
		if (*p != '\0' || !stop || !start)
			errx(1, "[-bcf] list: illegal list value");
		if (maxval < stop)
			maxval = stop;
		if (start <= stop)
			memset(positions + start, 1, stop - start + 1);
	}

	/* overlapping ranges */
	if (autostop && maxval > autostop)
		maxval = autostop;

	/* set autostart */
	if (autostart)
		memset(positions + 1, '1', autostart);
}

/* ARGSUSED */
void
b_cut(FILE *fp, char *fname)
{
	int ch, col;
	char *pos;

	for (;;) {
		pos = positions + 1;
		for (col = maxval; col; --col) {
			if ((ch = getc(fp)) == EOF)
				return;
			if (ch == '\n')
				break;
			if (*pos++)
				(void)putchar(ch);
		}
		if (ch != '\n') {
			if (autostop)
				while ((ch = getc(fp)) != EOF && ch != '\n')
					(void)putchar(ch);
			else
				while ((ch = getc(fp)) != EOF && ch != '\n')
					;
		}
		(void)putchar('\n');
	}
}

void
c_cut(FILE *fp, char *fname)
{
	static char	*line = NULL;
	static size_t	 linesz = 0;
	ssize_t		 linelen;
	char		*cp, *pos, *maxpos;
	int		 len;

	while ((linelen = getline(&line, &linesz, fp)) != -1) {
		if (line[linelen - 1] == '\n')
			line[linelen - 1] = '\0';

		cp = line;
		pos = positions + 1;
		maxpos = pos + maxval;
		while(pos < maxpos && *cp != '\0') {
			len = mblen(cp, MB_CUR_MAX);
			if (len == -1)
				len = 1;
			pos += nflag ? len : 1;
			if (pos[-1] == '\0')
				cp += len;
			else
				while (len--)
					putchar(*cp++);
		}
		if (autostop)
			puts(cp);
		else
			putchar('\n');
	}
}

void
f_cut(FILE *fp, char *fname)
{
	static char	*line = NULL;
	static size_t	 linesz = 0;
	ssize_t		 linelen;
	char		*sp, *ep, *pos, *maxpos;
	int		 output;

	while ((linelen = getline(&line, &linesz, fp)) != -1) {
		if (line[linelen - 1] == '\n')
			line[linelen - 1] = '\0';

		if ((ep = strstr(line, dchar)) == NULL) {
			if (!sflag)
				puts(line);
			continue;
		}

		pos = positions + 1;
		maxpos = pos + maxval;
		output = 0;
		sp = line;
		for (;;) {
			if (*pos++) {
				if (output)
					fputs(dchar, stdout);
				while (sp < ep)
					putchar(*sp++);
				output = 1;
			} else
				sp = ep;
			if (*sp == '\0' || pos == maxpos)
				break;
			sp += dlen;
			if ((ep = strstr(sp, dchar)) == NULL)
				ep = strchr(sp, '\0');
		}
		if (autostop)
			puts(sp);
		else
			putchar('\n');
	}
}

void
usage(void)
{
	(void)fprintf(stderr,
	    "usage: cut -b list [-n] [file ...]\n"
	    "       cut -c list [file ...]\n"
	    "       cut -f list [-s] [-d delim] [file ...]\n");
	exit(1);
}
