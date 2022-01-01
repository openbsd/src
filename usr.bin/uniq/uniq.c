/*	$OpenBSD: uniq.c,v 1.33 2022/01/01 18:20:52 cheloha Exp $	*/
/*	$NetBSD: uniq.c,v 1.7 1995/08/31 22:03:48 jtc Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Case Larsen.
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
#include <strings.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>

long long numchars, numfields;
unsigned long long repeats;
int cflag, dflag, iflag, uflag;

void	 show(const char *);
char	*skip(char *);
void	 obsolete(char *[]);
__dead void	usage(void);

int
main(int argc, char *argv[])
{
	const char *errstr;
	char *p, *prevline, *t, *thisline, *tmp;
	size_t prevsize, thissize, tmpsize;
	ssize_t len;
	int ch;

	setlocale(LC_CTYPE, "");

	if (pledge("stdio rpath wpath cpath", NULL) == -1)
		err(1, "pledge");

	obsolete(argv);
	while ((ch = getopt(argc, argv, "cdf:is:u")) != -1) {
		switch (ch) {
		case 'c':
			cflag = 1;
			break;
		case 'd':
			dflag = 1;
			break;
		case 'f':
			numfields = strtonum(optarg, 0, LLONG_MAX, &errstr);
			if (errstr)
				errx(1, "fields is %s: %s", errstr, optarg);
			break;
		case 'i':
			iflag = 1;
			break;
		case 's':
			numchars = strtonum(optarg, 0, LLONG_MAX, &errstr);
			if (errstr)
				errx(1, "chars is %s: %s", errstr, optarg);
			break;
		case 'u':
			uflag = 1;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	/* If neither -d nor -u are set, default is -d -u. */
	if (!dflag && !uflag)
		dflag = uflag = 1;

	if (argc > 2)
		usage();
	if (argc >= 1 && strcmp(argv[0], "-") != 0) {
		if (freopen(argv[0], "r", stdin) == NULL)
			err(1, "%s", argv[0]);
	}
	if (argc == 2 && strcmp(argv[1], "-") != 0) {
		if (freopen(argv[1], "w", stdout) == NULL)
			err(1, "%s", argv[1]);
	}

	if (pledge("stdio", NULL) == -1)
		err(1, "pledge");

	prevsize = 0;
	prevline = NULL;
	if ((len = getline(&prevline, &prevsize, stdin)) == -1) {
		free(prevline);
		if (ferror(stdin))
			err(1, "getline");
		return 0;
	}
	if (prevline[len - 1] == '\n')
		prevline[len - 1] = '\0';
	if (numfields || numchars)
		p = skip(prevline);
	else
		p = prevline;
	
	thissize = 0;
	thisline = NULL;
	while ((len = getline(&thisline, &thissize, stdin)) != -1) {
		if (thisline[len - 1] == '\n')
			thisline[len - 1] = '\0';

		/* If requested get the chosen fields + character offsets. */
		if (numfields || numchars)
			t = skip(thisline);
		else
			t = thisline;

		/* If different, print; set previous to new value. */
		if ((iflag ? strcasecmp : strcmp)(p, t)) {
			show(prevline);
			tmp = prevline;
			prevline = thisline;
			thisline = tmp;
			tmp = p;
			p = t;
			t = tmp;
			tmpsize = prevsize;
			prevsize = thissize;
			thissize = tmpsize;
			repeats = 0;
		} else
			++repeats;
	}
	free(thisline);
	if (ferror(stdin))
		err(1, "getline");

	show(prevline);
	free(prevline);

	return 0;
}

/*
 * show --
 *	Output a line depending on the flags and number of repetitions
 *	of the line.
 */
void
show(const char *str)
{
	if ((dflag && repeats) || (uflag && !repeats)) {
		if (cflag)
			printf("%4llu %s\n", repeats + 1, str);
		else
			printf("%s\n", str);
	}
}

char *
skip(char *str)
{
	long long nchars, nfields;
	wchar_t wc;
	int len;
	int field_started;

	for (nfields = numfields; nfields && *str; nfields--) {
		/* Skip one field, including preceding blanks. */
		for (field_started = 0; *str != '\0'; str += len) {
			if ((len = mbtowc(&wc, str, MB_CUR_MAX)) == -1) {
				(void)mbtowc(NULL, NULL, MB_CUR_MAX);
				wc = L'?';
				len = 1;
			}
			if (iswblank(wc)) {
				if (field_started)
					break;
			} else
				field_started = 1;
		}
	}

	/* Skip some additional characters. */
	for (nchars = numchars; nchars-- && *str != '\0'; str += len)
		if ((len = mblen(str, MB_CUR_MAX)) == -1)
			len = 1;

	return (str);
}

void
obsolete(char *argv[])
{
	size_t len;
	char *ap, *p, *start;

	while ((ap = *++argv)) {
		/* Return if "--" or not an option of any form. */
		if (ap[0] != '-') {
			if (ap[0] != '+')
				return;
		} else if (ap[1] == '-')
			return;
		if (!isdigit((unsigned char)ap[1]))
			continue;
		/*
		 * Digit signifies an old-style option.  Malloc space for dash,
		 * new option and argument.
		 */
		len = strlen(ap) + 3;
		if ((start = p = malloc(len)) == NULL)
			err(1, "malloc");
		*p++ = '-';
		*p++ = ap[0] == '+' ? 's' : 'f';
		(void)strlcpy(p, ap + 1, len - 2);
		*argv = start;
	}
}

__dead void
usage(void)
{
	fprintf(stderr,
	    "usage: %s [-ci] [-d | -u] [-f fields] [-s chars] [input_file [output_file]]\n",
	    getprogname());
	exit(1);
}
