/*	$OpenBSD: split.c,v 1.18 2015/01/16 06:40:12 deraadt Exp $	*/
/*	$NetBSD: split.c,v 1.5 1995/08/31 22:22:05 jtc Exp $	*/

/*
 * Copyright (c) 1987, 1993, 1994
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

#include <sys/param.h>	/* MAXBSIZE */
#include <sys/types.h>

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <regex.h>
#include <sysexits.h>

#define DEFLINE	1000			/* Default num lines per file. */

ssize_t	 bytecnt;			/* Byte count to split on. */
long	 numlines;			/* Line count to split on. */
int	 file_open;			/* If a file open. */
int	 ifd = -1, ofd = -1;		/* Input/output file descriptors. */
char	 bfr[MAXBSIZE];			/* I/O buffer. */
char	 fname[PATH_MAX];		/* File name prefix. */
regex_t	 rgx;
int	 pflag;
int	 sufflen = 2;			/* File name suffix length. */

void newfile(void);
void split1(void);
void split2(void);
__dead void usage(void);

int
main(int argc, char *argv[])
{
	int ch, scale;
	char *ep, *p;
	const char *errstr;

	while ((ch = getopt(argc, argv, "0123456789a:b:l:p:-")) != -1)
		switch (ch) {
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			/*
			 * Undocumented kludge: split was originally designed
			 * to take a number after a dash.
			 */
			if (numlines == 0) {
				p = argv[optind - 1];
				if (p[0] == '-' && p[1] == ch && !p[2])
					numlines = strtol(++p, &ep, 10);
				else
					numlines =
					    strtol(argv[optind] + 1, &ep, 10);
				if (numlines <= 0 || *ep)
					errx(EX_USAGE,
					    "%s: illegal line count", optarg);
			}
			break;
		case '-':		/* Undocumented: historic stdin flag. */
			if (ifd != -1)
				usage();
			ifd = 0;
			break;
		case 'a':		/* suffix length. */
			sufflen = strtonum(optarg, 1, NAME_MAX, &errstr);
			if (errstr)
				errx(EX_USAGE, "%s: %s", optarg, errstr);
			break;
		case 'b':		/* Byte count. */
			if ((bytecnt = strtol(optarg, &ep, 10)) <= 0 ||
			    (*ep != '\0' && *ep != 'k' && *ep != 'm'))
				errx(EX_USAGE,
				    "%s: illegal byte count", optarg);
			if (*ep == 'k')
				scale = 1024;
			else if (*ep == 'm')
				scale = 1048576;
			else
				scale = 1;
			if (bytecnt > SSIZE_MAX / scale)
				errx(EX_USAGE, "%s: byte count too large",
				    optarg);
			bytecnt *= scale;
			break;
		case 'p' :      /* pattern matching. */
			if (regcomp(&rgx, optarg, REG_EXTENDED|REG_NOSUB) != 0)
				errx(EX_USAGE, "%s: illegal regexp", optarg);
			pflag = 1;
			break;
		case 'l':		/* Line count. */
			if (numlines != 0)
				usage();
			if ((numlines = strtol(optarg, &ep, 10)) <= 0 || *ep)
				errx(EX_USAGE,
				    "%s: illegal line count", optarg);
			break;
		default:
			usage();
		}
	argv += optind;
	argc -= optind;

	if (*argv != NULL)
		if (ifd == -1) {		/* Input file. */
			if ((ifd = open(*argv, O_RDONLY, 0)) < 0)
				err(EX_NOINPUT, "%s", *argv);
			++argv;
		}
	if (*argv != NULL)			/* File name prefix. */
		(void)strlcpy(fname, *argv++, sizeof(fname));
	if (*argv != NULL)
		usage();

	if (strlen(fname) + sufflen >= sizeof(fname))
		errx(EX_USAGE, "suffix is too long");
	if (pflag && (numlines != 0 || bytecnt != 0))
		usage();

	if (numlines == 0)
		numlines = DEFLINE;
	else if (bytecnt != 0)
		usage();

	if (ifd == -1)				/* Stdin by default. */
		ifd = 0;

	if (bytecnt) {
		split1();
		exit (0);
	}
	split2();
	if (pflag)
		regfree(&rgx);
	exit(0);
}

/*
 * split1 --
 *	Split the input by bytes.
 */
void
split1(void)
{
	ssize_t bcnt, dist, len;
	char *C;

	for (bcnt = 0;;)
		switch ((len = read(ifd, bfr, MAXBSIZE))) {
		case 0:
			exit(0);
		case -1:
			err(EX_IOERR, "read");
			/* NOTREACHED */
		default:
			if (!file_open)
				newfile();
			if (bcnt + len >= bytecnt) {
				dist = bytecnt - bcnt;
				if (write(ofd, bfr, dist) != dist)
					err(EX_IOERR, "write");
				len -= dist;
				for (C = bfr + dist; len >= bytecnt;
				    len -= bytecnt, C += bytecnt) {
					newfile();
					if (write(ofd, C, bytecnt) != bytecnt)
						err(EX_IOERR, "write");
				}
				if (len != 0) {
					newfile();
					if (write(ofd, C, len) != len)
						err(EX_IOERR, "write");
				} else
					file_open = 0;
				bcnt = len;
			} else {
				bcnt += len;
				if (write(ofd, bfr, len) != len)
					err(EX_IOERR, "write");
			}
		}
}

/*
 * split2 --
 *	Split the input by lines.
 */
void
split2(void)
{
	long lcnt = 0;
	FILE *infp;

	/* Stick a stream on top of input file descriptor */
	if ((infp = fdopen(ifd, "r")) == NULL)
		err(EX_NOINPUT, "fdopen");

	/* Process input one line at a time */
	while (fgets(bfr, sizeof(bfr), infp) != NULL) {
		const int len = strlen(bfr);

		if (len == 0)
			continue;

		/* If line is too long to deal with, just write it out */
		if (bfr[len - 1] != '\n')
			goto writeit;

		/* Check if we need to start a new file */
		if (pflag) {
			regmatch_t pmatch;

			pmatch.rm_so = 0;
			pmatch.rm_eo = len - 1;
			if (regexec(&rgx, bfr, 0, &pmatch, REG_STARTEND) == 0)
				newfile();
		} else if (lcnt++ == numlines) {
			newfile();
			lcnt = 1;
		}

writeit:
		/* Open output file if needed */
		if (!file_open)
			newfile();

		/* Write out line */
		if (write(ofd, bfr, len) != len)
			err(EX_IOERR, "write");
	}

	/* EOF or error? */
	if (ferror(infp))
		err(EX_IOERR, "read");
	else
		exit(0);
}

/*
 * newfile --
 *	Open a new output file.
 */
void
newfile(void)
{
	static char *suffix, *sufftail;
	char *sptr;

	if (ofd == -1) {
		ofd = fileno(stdout);
		if (*fname == '\0') {
			*fname = 'x';	/* no name specified, use 'x' */
			memset(fname + 1, 'a', sufflen);
			suffix = fname;
			sufflen++;	/* treat 'x' as part of suffix */
		} else {
			suffix = fname + strlen(fname);
			memset(suffix, 'a', sufflen);
		}
		suffix[sufflen] = '\0';
		sufftail = suffix + sufflen - 1;
	} else {
		for (sptr = sufftail; sptr >= suffix; sptr--) {
			if (*sptr != 'z') {
				(*sptr)++;
				break;
			} else
				*sptr = 'a';
		}
		if (sptr < suffix)
			errx(EX_DATAERR, "too many files");
	}

	if (!freopen(fname, "w", stdout))
		err(EX_IOERR, "%s", fname);
	file_open = 1;
}

__dead void
usage(void)
{
	extern char *__progname;

	(void)fprintf(stderr, "usage: %s [-a suffix_length]\n"
	    "             [-b byte_count[k|m] | -l line_count | -p pattern] "
	    "[file [name]]\n", __progname);
	exit(EX_USAGE);
}
