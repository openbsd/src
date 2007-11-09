/*	$OpenBSD: uniq.c,v 1.16 2007/11/09 20:04:03 kili Exp $	*/
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

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1989, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)uniq.c	8.3 (Berkeley) 5/4/95";
#endif
static char rcsid[] = "$OpenBSD: uniq.c,v 1.16 2007/11/09 20:04:03 kili Exp $";
#endif /* not lint */

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define	MAXLINELEN	(8 * 1024)

int cflag, dflag, uflag;
int numchars, numfields, repeats;

FILE	*file(char *, char *);
void	 show(FILE *, char *);
char	*skip(char *);
void	 obsolete(char *[]);
__dead void	usage(void);

int
main(int argc, char *argv[])
{
	char *t1, *t2;
	FILE *ifp = NULL, *ofp = NULL;
	int ch;
	char *prevline, *thisline;

	obsolete(argv);
	while ((ch = getopt(argc, argv, "cdf:s:u")) != -1) {
		const char *errstr;

		switch (ch) {
		case 'c':
			cflag = 1;
			break;
		case 'd':
			dflag = 1;
			break;
		case 'f':
			numfields = (int)strtonum(optarg, 0, INT_MAX,
			    &errstr);
			if (errstr)
				errx(1, "field skip value is %s: %s",
				    errstr, optarg);
			break;
		case 's':
			numchars = (int)strtonum(optarg, 0, INT_MAX,
			    &errstr);
			if (errstr)
				errx(1,
				    "character skip value is %s: %s",
				    errstr, optarg);
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

	switch(argc) {
	case 0:
		ifp = stdin;
		ofp = stdout;
		break;
	case 1:
		ifp = file(argv[0], "r");
		ofp = stdout;
		break;
	case 2:
		ifp = file(argv[0], "r");
		ofp = file(argv[1], "w");
		break;
	default:
		usage();
	}

	prevline = malloc(MAXLINELEN);
	thisline = malloc(MAXLINELEN);
	if (prevline == NULL || thisline == NULL)
		err(1, "malloc");

	if (fgets(prevline, MAXLINELEN, ifp) == NULL)
		exit(0);

	while (fgets(thisline, MAXLINELEN, ifp)) {
		/* If requested get the chosen fields + character offsets. */
		if (numfields || numchars) {
			t1 = skip(thisline);
			t2 = skip(prevline);
		} else {
			t1 = thisline;
			t2 = prevline;
		}

		/* If different, print; set previous to new value. */
		if (strcmp(t1, t2)) {
			show(ofp, prevline);
			t1 = prevline;
			prevline = thisline;
			thisline = t1;
			repeats = 0;
		} else
			++repeats;
	}
	show(ofp, prevline);
	exit(0);
}

/*
 * show --
 *	Output a line depending on the flags and number of repetitions
 *	of the line.
 */
void
show(FILE *ofp, char *str)
{
	if ((dflag && repeats) || (uflag && !repeats)) {
		if (cflag)
			(void)fprintf(ofp, "%4d %s", repeats + 1, str);
		else
			(void)fprintf(ofp, "%s", str);
	}
}

char *
skip(char *str)
{
	int nchars, nfields;

	for (nfields = numfields; nfields && *str; nfields--) {
		while (isblank(*str))
			str++;
		while (*str && !isblank(*str))
			str++;
	}
	for (nchars = numchars; nchars-- && *str && *str != '\n'; ++str)
		;
	return (str);
}

FILE *
file(char *name, char *mode)
{
	FILE *fp;

	if (strcmp(name, "-") == 0)
		return(*mode == 'r' ? stdin : stdout);
	if ((fp = fopen(name, mode)) == NULL)
		err(1, "%s", name);
	return (fp);
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
		if (!isdigit(ap[1]))
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
	extern char *__progname;

	(void)fprintf(stderr,
	    "usage: %s [-c | -d | -u] [-f fields] [-s chars] [input_file [output_file]]\n",
	    __progname);
	exit(1);
}
