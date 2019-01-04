/*	$OpenBSD: tail.c,v 1.22 2019/01/04 15:04:28 martijn Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Edward Sze-Tyan Wang.
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
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

int fflag, rflag, rval;

static void obsolete(char **);
static void usage(void);

int
main(int argc, char *argv[])
{
	struct tailfile *tf;
	off_t off = 0;
	enum STYLE style;
	int ch;
	int i;
	char *p;

	if (pledge("stdio rpath", NULL) == -1)
		err(1, "pledge");

	/*
	 * Tail's options are weird.  First, -n10 is the same as -n-10, not
	 * -n+10.  Second, the number options are 1 based and not offsets,
	 * so -n+1 is the first line, and -c-1 is the last byte.  Third, the
	 * number options for the -r option specify the number of things that
	 * get displayed, not the starting point in the file.  The one major
	 * incompatibility in this version as compared to historical versions
	 * is that the 'r' option couldn't be modified by the -lbc options,
	 * i.e. it was always done in lines.  This version treats -rc as a
	 * number of characters in reverse order.  Finally, the default for
	 * -r is the entire file, not 10 lines.
	 */
#define	ARG(units, forward, backward) {					\
	if (style)							\
		usage();						\
	off = strtoll(optarg, &p, 10) * (units);			\
	if (*p)								\
		errx(1, "illegal offset -- %s", optarg);		\
	switch(optarg[0]) {						\
	case '+':							\
		if (off)						\
			off -= (units);					\
			style = (forward);				\
		break;							\
	case '-':							\
		off = -off;						\
		/* FALLTHROUGH */					\
	default:							\
		style = (backward);					\
		break;							\
	}								\
}

	obsolete(argv);
	style = NOTSET;
	while ((ch = getopt(argc, argv, "b:c:fn:r")) != -1)
		switch(ch) {
		case 'b':
			ARG(512, FBYTES, RBYTES);
			break;
		case 'c':
			ARG(1, FBYTES, RBYTES);
			break;
		case 'f':
			fflag = 1;
			break;
		case 'n':
			ARG(1, FLINES, RLINES);
			break;
		case 'r':
			rflag = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	/*
	 * If displaying in reverse, don't permit follow option, and convert
	 * style values.
	 */
	if (rflag) {
		if (fflag)
			usage();
		if (style == FBYTES)
			style = RBYTES;
		else if (style == FLINES)
			style = RLINES;
	}

	/*
	 * If style not specified, the default is the whole file for -r, and
	 * the last 10 lines if not -r.
	 */
	if (style == NOTSET) {
		if (rflag) {
			off = 0;
			style = REVERSE;
		} else {
			off = 10;
			style = RLINES;
		}
	}

	if ((tf = reallocarray(NULL, argc ? argc : 1, sizeof(*tf))) == NULL)
		err(1, "reallocarray");

	if (argc) {
		for (i = 0; *argv; i++) {
			tf[i].fname = *argv++;
			if ((tf[i].fp = fopen(tf[i].fname, "r")) == NULL ||
			    fstat(fileno(tf[i].fp), &(tf[i].sb))) {
				ierr(tf[i].fname);
				i--;
				continue;
			}
		}
		if (rflag)
			reverse(tf, i, style, off);
		else
			forward(tf, i, style, off);
	}
	else {
		if (pledge("stdio", NULL) == -1)
			err(1, "pledge");

		tf[0].fname = "stdin";
		tf[0].fp = stdin;

		if (fstat(fileno(stdin), &(tf[0].sb))) {
			ierr(tf[0].fname);
			exit(1);
		}

		/*
		 * Determine if input is a pipe.  4.4BSD will set the SOCKET
		 * bit in the st_mode field for pipes.  Fix this then.
		 */
		if (lseek(fileno(tf[0].fp), (off_t)0, SEEK_CUR) == -1 &&
		    errno == ESPIPE) {
			errno = 0;
			fflag = 0;		/* POSIX.2 requires this. */
		}

		if (rflag)
			reverse(tf, 1, style, off);
		else
			forward(tf, 1, style, off);
	}
	exit(rval);
}

/*
 * Convert the obsolete argument form into something that getopt can handle.
 * This means that anything of the form [+-][0-9][0-9]*[lbc][fr] that isn't
 * the option argument for a -b, -c or -n option gets converted.
 */
static void
obsolete(char *argv[])
{
	char *ap, *p, *t;
	size_t len;
	char *start;

	while ((ap = *++argv)) {
		/* Return if "--" or not an option of any form. */
		if (ap[0] != '-') {
			if (ap[0] != '+')
				return;
		} else if (ap[1] == '-')
			return;

		switch(*++ap) {
		/* Old-style option. */
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':

			/* Malloc space for dash, new option and argument. */
			len = strlen(*argv);
			if ((start = p = malloc(len + 4)) == NULL)
				err(1, NULL);
			*p++ = '-';

			/*
			 * Go to the end of the option argument.  Save off any
			 * trailing options (-3lf) and translate any trailing
			 * output style characters.
			 */
			t = *argv + len - 1;
			if (*t == 'f' || *t == 'r') {
				*p++ = *t;
				*t-- = '\0';
			}
			switch(*t) {
			case 'b':
				*p++ = 'b';
				*t = '\0';
				break;
			case 'c':
				*p++ = 'c';
				*t = '\0';
				break;
			case 'l':
				*t = '\0';
				/* FALLTHROUGH */
			case '0': case '1': case '2': case '3': case '4':
			case '5': case '6': case '7': case '8': case '9':
				*p++ = 'n';
				break;
			default:
				errx(1, "illegal option -- %s", *argv);
			}
			*p++ = *argv[0];
			(void)strlcpy(p, ap, start + len + 4 - p);
			*argv = start;
			continue;

		/*
		 * Options w/ arguments, skip the argument and continue
		 * with the next option.
		 */
		case 'b':
		case 'c':
		case 'n':
			if (!ap[1])
				++argv;
			/* FALLTHROUGH */
		/* Options w/o arguments, continue with the next option. */
		case 'f':
		case 'r':
			continue;

		/* Illegal option, return and let getopt handle it. */
		default:
			return;
		}
	}
}

static void
usage(void)
{
	(void)fprintf(stderr,
	    "usage: tail [-f | -r] "
	    "[-b number | -c number | -n number | -number] [file ...]\n");
	exit(1);
}
