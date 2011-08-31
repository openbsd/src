/*	$OpenBSD: touch.c,v 1.21 2011/08/31 08:48:40 jmc Exp $	*/
/*	$NetBSD: touch.c,v 1.11 1995/08/31 22:10:06 jtc Exp $	*/

/*
 * Copyright (c) 1993
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
#include <sys/stat.h>
#include <sys/time.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <time.h>
#include <tzfile.h>
#include <unistd.h>

void		stime_arg1(char *, struct timespec *);
void		stime_arg2(char *, int, struct timespec *);
void		stime_argd(char *, struct timespec *);
void		stime_file(char *, struct timespec *);
__dead void	usage(void);

int
main(int argc, char *argv[])
{
	struct timespec	 ts[2];
	int		 aflag, cflag, mflag, ch, fd, len, rval, timeset;
	char		*p;

	(void)setlocale(LC_ALL, "");

	aflag = cflag = mflag = timeset = 0;
	while ((ch = getopt(argc, argv, "acd:fmr:t:")) != -1)
		switch (ch) {
		case 'a':
			aflag = 1;
			break;
		case 'c':
			cflag = 1;
			break;
		case 'd':
			timeset = 1;
			stime_argd(optarg, ts);
			break;
		case 'f':
			break;
		case 'm':
			mflag = 1;
			break;
		case 'r':
			timeset = 1;
			stime_file(optarg, ts);
			break;
		case 't':
			timeset = 1;
			stime_arg1(optarg, ts);
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	/* Default is both -a and -m. */
	if (aflag == 0 && mflag == 0)
		aflag = mflag = 1;

	/*
	 * If no -r or -t flag, at least two operands, the first of which
	 * is an 8 or 10 digit number, use the obsolete time specification.
	 */
	if (!timeset && argc > 1) {
		(void)strtol(argv[0], &p, 10);
		len = p - argv[0];
		if (*p == '\0' && (len == 8 || len == 10)) {
			timeset = 1;
			stime_arg2(*argv++, len == 10, ts);
		}
	}

	/* Otherwise use the current time of day. */
	if (!timeset)
		ts[0].tv_nsec = ts[1].tv_nsec = UTIME_NOW;

	if (!aflag)
		ts[0].tv_nsec = UTIME_OMIT;
	if (!mflag)
		ts[1].tv_nsec = UTIME_OMIT;

	if (*argv == NULL)
		usage();

	for (rval = 0; *argv; ++argv) {
		/* Update the file's timestamp if it exists. */
		if (! utimensat(AT_FDCWD, *argv, ts, 0))
			continue;
		if (errno != ENOENT) {
			rval = 1;
			warn("%s", *argv);
			continue;
		}

		/* Didn't exist; should we create it? */
		if (cflag)
			continue;

		/* Create the file. */
		fd = open(*argv, O_WRONLY | O_CREAT, DEFFILEMODE);
		if (fd == -1 || futimens(fd, ts) || close(fd)) {
			rval = 1;
			warn("%s", *argv);
		}
	}
	exit(rval);
}

#define	ATOI2(s)	((s) += 2, ((s)[-2] - '0') * 10 + ((s)[-1] - '0'))

void
stime_arg1(char *arg, struct timespec *tsp)
{
	struct tm	*lt;
	time_t		 tmptime;
	int		 yearset;
	char		*dot, *p;
					/* Start with the current time. */
	tmptime = time(NULL);
	if ((lt = localtime(&tmptime)) == NULL)
		err(1, "localtime");
					/* [[CC]YY]MMDDhhmm[.SS] */
	for (p = arg, dot = NULL; *p != '\0'; p++) {
		if (*p == '.' && dot == NULL)
			dot = p;
		else if (!isdigit((unsigned char)*p))
			goto terr;
	}
	if (dot == NULL)
		lt->tm_sec = 0;		/* Seconds defaults to 0. */
	else {
		*dot++ = '\0';
		if (strlen(dot) != 2)
			goto terr;
		lt->tm_sec = ATOI2(dot);
		if (lt->tm_sec > 61)	/* Could be leap second. */
			goto terr;
	}

	yearset = 0;
	switch (strlen(arg)) {
	case 12:			/* CCYYMMDDhhmm */
		lt->tm_year = ATOI2(arg) * 100 - TM_YEAR_BASE;
		yearset = 1;
		/* FALLTHROUGH */
	case 10:			/* YYMMDDhhmm */
		if (yearset) {
			yearset = ATOI2(arg);
			lt->tm_year += yearset;
		} else {
			yearset = ATOI2(arg);
			/* POSIX logic: [00,68]=>20xx, [69,99]=>19xx */
			lt->tm_year = yearset + 1900 - TM_YEAR_BASE;
			if (yearset < 69)
				lt->tm_year += 100;
		}
		/* FALLTHROUGH */
	case 8:				/* MMDDhhmm */
		lt->tm_mon = ATOI2(arg);
		if (lt->tm_mon > 12 || lt->tm_mon == 0)
			goto terr;
		--lt->tm_mon;		/* Convert from 01-12 to 00-11 */
		lt->tm_mday = ATOI2(arg);
		if (lt->tm_mday > 31 || lt->tm_mday == 0)
			goto terr;
		lt->tm_hour = ATOI2(arg);
		if (lt->tm_hour > 23)
			goto terr;
		lt->tm_min = ATOI2(arg);
		if (lt->tm_min > 59)
			goto terr;
		break;
	default:
		goto terr;
	}

	lt->tm_isdst = -1;		/* Figure out DST. */
	tsp[0].tv_sec = tsp[1].tv_sec = mktime(lt);
	if (tsp[0].tv_sec == -1)
terr:		errx(1,
	"out of range or illegal time specification: [[CC]YY]MMDDhhmm[.SS]");

	tsp[0].tv_nsec = tsp[1].tv_nsec = 0;
}

void
stime_arg2(char *arg, int year, struct timespec *tsp)
{
	struct tm	*lt;
	time_t		 tmptime;
					/* Start with the current time. */
	tmptime = time(NULL);
	if ((lt = localtime(&tmptime)) == NULL)
		err(1, "localtime");

	lt->tm_mon = ATOI2(arg);	/* MMDDhhmm[YY] */
	if (lt->tm_mon > 12 || lt->tm_mon == 0)
		goto terr;
	--lt->tm_mon;			/* Convert from 01-12 to 00-11 */
	lt->tm_mday = ATOI2(arg);
	if (lt->tm_mday > 31 || lt->tm_mday == 0)
		goto terr;
	lt->tm_hour = ATOI2(arg);
	if (lt->tm_hour > 23)
		goto terr;
	lt->tm_min = ATOI2(arg);
	if (lt->tm_min > 59)
		goto terr;
	if (year) {
		year = ATOI2(arg);
		/* POSIX logic: [00,68]=>20xx, [69,99]=>19xx */
		lt->tm_year = year + 1900 - TM_YEAR_BASE;
		if (year < 69)
			lt->tm_year += 100;
	}
	lt->tm_sec = 0;

	lt->tm_isdst = -1;		/* Figure out DST. */
	tsp[0].tv_sec = tsp[1].tv_sec = mktime(lt);
	if (tsp[0].tv_sec == -1)
terr:		errx(1,
	"out of range or illegal time specification: MMDDhhmm[YY]");

	tsp[0].tv_nsec = tsp[1].tv_nsec = 0;
}

void
stime_file(char *fname, struct timespec *tsp)
{
	struct stat	sb;

	if (stat(fname, &sb))
		err(1, "%s", fname);
	tsp[0] = sb.st_atim;
	tsp[1] = sb.st_mtim;
}

void
stime_argd(char *arg, struct timespec *tsp)
{
	struct tm	tm;
	char		*frac, *p;
	int		utc = 0;

	/* accept YYYY-MM-DD(T| )hh:mm:ss[(.|,)frac][Z] */
	memset(&tm, 0, sizeof(tm));
	p = strptime(arg, "%F", &tm);
	if (p == NULL || (*p != 'T' && *p != ' '))
		goto terr;
	p = strptime(p + 1, "%T", &tm);
	if (p == NULL)
		goto terr;
	tsp[0].tv_nsec = 0;
	if (*p == '.' || *p == ',') {
		frac = ++p;
		while (isdigit((unsigned char)*p)) {
			if (p - frac < 9) {
				tsp[0].tv_nsec = tsp[0].tv_nsec * 10 +
				    *p - '0';
			}
			p++;
		}
		if (p == frac)
			goto terr;

		/* fill in the trailing zeros */
		while (p - frac-- < 9)
			tsp[0].tv_nsec *= 10;
	}
	if (*p == 'Z') {
		utc = 1;
		p++;
	}
	if (*p != '\0')
		goto terr;

	tm.tm_isdst = -1;
	tsp[0].tv_sec = utc ? timegm(&tm) : mktime(&tm);
	if (tsp[0].tv_sec == -1)
terr:		errx(1,
  "out of range or illegal time specification: YYYY-MM-DDThh:mm:ss[.frac][Z]");
	tsp[1] = tsp[0];
}

__dead void
usage(void)
{
	(void)fprintf(stderr,
"usage: touch [-acm] [-d ccyy-mm-ddTHH:MM:SS[.frac][Z]] [-r file]\n"
"             [-t [[cc]yy]mmddHHMM[.SS]] file ...\n");
	exit(1);
}
