/*	$OpenBSD: date.c,v 1.30 2007/10/16 20:19:26 sobrado Exp $	*/
/*	$NetBSD: date.c,v 1.11 1995/09/07 06:21:05 jtc Exp $	*/

/*
 * Copyright (c) 1985, 1987, 1988, 1993
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

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1985, 1987, 1988, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)date.c	8.2 (Berkeley) 4/28/95";
#else
static char rcsid[] = "$OpenBSD: date.c,v 1.30 2007/10/16 20:19:26 sobrado Exp $";
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/time.h>

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <syslog.h>
#include <time.h>
#include <tzfile.h>
#include <unistd.h>
#include <util.h>

#include "extern.h"

extern	char *__progname;

time_t tval;
int retval, jflag, nflag;
int slidetime;

static void setthetime(char *);
static void badformat(void);
static void usage(void);

int
main(int argc, char *argv[])
{
	struct timezone tz;
	int ch, rflag;
	char *format, buf[1024];

	setlocale(LC_ALL, "");

	tz.tz_dsttime = tz.tz_minuteswest = 0;
	rflag = 0;
	while ((ch = getopt(argc, argv, "ad:jnr:ut:")) != -1)
		switch((char)ch) {
		case 'd':		/* daylight saving time */
			tz.tz_dsttime = atoi(optarg) ? 1 : 0;
			break;
		case 'a':
			slidetime++;
			break;
		case 'j':		/* don't set */
			jflag = 1;
			break;
		case 'n':		/* don't set network */
			nflag = 1;
			break;
		case 'r':		/* user specified seconds */
			rflag = 1;
			tval = atol(optarg);
			break;
		case 'u':		/* do everything in UTC */
			if (setenv("TZ", "UTC", 1) == -1)
				err(1, "cannot unsetenv TZ");
			break;
		case 't':		/* minutes west of GMT */
					/* error check; don't allow "PST" */
			if (isdigit(*optarg)) {
				tz.tz_minuteswest = atoi(optarg);
				break;
			}
			/* FALLTHROUGH */
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	/*
	 * If -d or -t, set the timezone or daylight saving time; this
	 * doesn't belong here, the kernel should not know about either.
	 */
	if ((tz.tz_minuteswest || tz.tz_dsttime) &&
	    settimeofday(NULL, &tz))
		err(1, "settimeofday");

	if (!rflag && time(&tval) == -1)
		err(1, "time");

	format = "%a %b %e %H:%M:%S %Z %Y";

	/* allow the operands in any order */
	if (*argv && **argv == '+') {
		format = *argv + 1;
		++argv;
	}

	if (*argv) {
		setthetime(*argv);
		++argv;
	}

	if (*argv && **argv == '+')
		format = *argv + 1;

	(void)strftime(buf, sizeof(buf), format, localtime(&tval));
	(void)printf("%s\n", buf);
	exit(retval);
}

#define	ATOI2(ar)	((ar) += 2, ((ar)[-2] - '0') * 10 + ((ar)[-1] - '0'))
void
setthetime(char *p)
{
	struct tm *lt;
	struct timeval tv;
	char *dot, *t;
	int yearset = 0;

	for (t = p, dot = NULL; *t; ++t) {
		if (isdigit(*t))
			continue;
		if (*t == '.' && dot == NULL) {
			dot = t;
			continue;
		}
		badformat();
	}

	lt = localtime(&tval);

	lt->tm_isdst = -1;			/* correct for DST */

	if (dot != NULL) {			/* .SS */
		*dot++ = '\0';
		if (strlen(dot) != 2)
			badformat();
		lt->tm_sec = ATOI2(dot);
		if (lt->tm_sec > 61)
			badformat();
	} else
		lt->tm_sec = 0;

	switch (strlen(p)) {
	case 12:				/* cc */
		lt->tm_year = ATOI2(p) * 100 - TM_YEAR_BASE;
		yearset = 1;
		/* FALLTHROUGH */
	case 10:				/* yy */
		if (!yearset) {
			/* mask out current year, leaving only century */
			lt->tm_year = ((lt->tm_year / 100) * 100);
		}
		lt->tm_year += ATOI2(p);
		/* FALLTHROUGH */
	case 8:					/* mm */
		lt->tm_mon = ATOI2(p);
		if ((lt->tm_mon > 12) || !lt->tm_mon)
			badformat();
		--lt->tm_mon;			/* time struct is 0 - 11 */
		/* FALLTHROUGH */
	case 6:					/* dd */
		lt->tm_mday = ATOI2(p);
		if ((lt->tm_mday > 31) || !lt->tm_mday)
			badformat();
		/* FALLTHROUGH */
	case 4:					/* HH */
		lt->tm_hour = ATOI2(p);
		if (lt->tm_hour > 23)
			badformat();
		/* FALLTHROUGH */
	case 2:					/* MM */
		lt->tm_min = ATOI2(p);
		if (lt->tm_min > 59)
			badformat();
		break;
	default:
		badformat();
	}

	/* convert broken-down time to UTC clock time */
	if ((tval = mktime(lt)) < 0)
		errx(1, "specified date is outside allowed range");

	if (jflag)
		return;

	/* set the time */
	if (nflag || netsettime(tval)) {
		if (slidetime) {
			struct timeval tv_current;

			if (gettimeofday(&tv_current, NULL) == -1)
				err(1, "Could not get local time of day");

			tv.tv_sec = tval - tv_current.tv_sec;
			tv.tv_usec = 0;
			if (adjtime(&tv, NULL) == -1)
				errx(1, "adjtime");
		} else {
			logwtmp("|", "date", "");
			tv.tv_sec = tval;
			tv.tv_usec = 0;
			if (settimeofday(&tv, NULL))
				err(1, "settimeofday");
			logwtmp("{", "date", "");
		}
	}

	if ((p = getlogin()) == NULL)
		p = "???";
	syslog(LOG_AUTH | LOG_NOTICE, "date set by %s", p);
}

static void
badformat(void)
{
	warnx("illegal time format");
	usage();
}

static void
usage(void)
{
	(void)fprintf(stderr,
	    "usage: %s [-ajnu] [-d dst] [-r seconds] [-t minutes_west] [+format]\n",
	     __progname);
	(void)fprintf(stderr,
	    "%-*s[[[[[[cc]yy]mm]dd]HH]MM[.SS]]\n", strlen(__progname) + 8, "");
	exit(1);
}
