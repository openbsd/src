/*	$OpenBSD: cal.c,v 1.27 2015/03/15 00:41:27 millert Exp $	*/
/*	$NetBSD: cal.c,v 1.6 1995/03/26 03:10:24 glass Exp $	*/

/*
 * Copyright (c) 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kim Letkeman.
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

#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define	THURSDAY		4		/* for reformation */
#define	SATURDAY		6		/* 1 Jan 1 was a Saturday */

#define	FIRST_MISSING_DAY	639799		/* 3 Sep 1752 */
#define	NUMBER_MISSING_DAYS	11		/* 11 day correction */

#define	MAXDAYS			42		/* max slots in a month array */
#define	SPACE			-1		/* used in day array */

static const int days_in_month[2][13] = {
	{0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
	{0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
};

const int sep1752s[MAXDAYS] = {
	SPACE,	SPACE,	1,	2,	14,	15,	16,
	17,	18,	19,	20,	21,	22,	23,
	24,	25,	26,	27,	28,	29,	30,
	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,
	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,
	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,
}, sep1752m[MAXDAYS] = {
	SPACE,	1,	2,	14,	15,	16,	17,
	18,	19,	20,	21,	22,	23,	24,
	25,	26,	27,	28,	29,	30,	SPACE,
	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,
	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,
	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,
}, sep1752js[MAXDAYS] = {
	SPACE,	SPACE,	245,	246,	258,	259,	260,
	261,	262,	263,	264,	265,	266,	267,
	268,	269,	270,	271,	272,	273,	274,
	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,
	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,
	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,
}, sep1752jm[MAXDAYS] = {
	SPACE,	245,	246,	258,	259,	260,	261,
	262,	263,	264,	265,	266,	267,	268,
	269,	270,	271,	272,	273,	274,	SPACE,
	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,
	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,
	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,
}, empty[MAXDAYS] = {
	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,
	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,
	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,
	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,
	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,
	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,
};

const char *month_names[12] = {
	"January", "February", "March", "April", "May", "June",
	"July", "August", "September", "October", "November", "December",
};

#define	DAY_HEADINGS_S	"Su Mo Tu We Th Fr Sa"
#define	DAY_HEADINGS_M	"Mo Tu We Th Fr Sa Su"
#define	DAY_HEADINGS_JS	" Su  Mo  Tu  We  Th  Fr  Sa"
#define	DAY_HEADINGS_JM	" Mo  Tu  We  Th  Fr  Sa  Su"

const int	*sep1752 = NULL;
const char	*day_headings = NULL;

/* leap year -- account for gregorian reformation in 1752 */
#define	leap_year(yr) \
	((yr) <= 1752 ? !((yr) % 4) : \
	(!((yr) % 4) && ((yr) % 100)) || !((yr) % 400))

/* number of centuries since 1700, not inclusive */
#define	centuries_since_1700(yr) \
	((yr) > 1700 ? (yr) / 100 - 17 : 0)

/* number of centuries since 1700 whose modulo of 400 is 0 */
#define	quad_centuries_since_1700(yr) \
	((yr) > 1600 ? ((yr) - 1600) / 400 : 0)

/* number of leap years between year 1 and this year, not inclusive */
#define	leap_years_since_year_1(yr) \
	((yr) / 4 - centuries_since_1700(yr) + quad_centuries_since_1700(yr))

int julian;
int mflag = 0;
int wflag = 0;

void	ascii_day(char *, int);
void	center(const char *, int, int);
void	day_array(int, int, int *);
int	day_in_week(int, int, int);
int	day_in_year(int, int, int);
int	week(int, int, int);
int	isoweek(int, int, int);
void	j_yearly(int);
void	monthly(int, int);
void	trim_trailing_spaces(char *);
void	usage(void);
void	yearly(int);
int	parsemonth(const char *);

int
main(int argc, char *argv[])
{
	struct tm *local_time;
	time_t now;
	int ch, month, year, yflag;
	const char *errstr;

	yflag = year = 0;
	while ((ch = getopt(argc, argv, "jmwy")) != -1)
		switch(ch) {
		case 'j':
			julian = 1;
			break;
		case 'm':
			mflag = 1;
			break;
		case 'w':
			wflag = 1;
			break;
		case 'y':
			yflag = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (julian && wflag)
		usage();

	day_headings = DAY_HEADINGS_S;
	sep1752 = sep1752s;
	if (mflag && julian) {
		sep1752 = sep1752jm;
		day_headings = DAY_HEADINGS_JM;
	} else if (mflag) {
		sep1752 = sep1752m;
		day_headings = DAY_HEADINGS_M;
	} else if (julian) {
		sep1752 = sep1752js;
		day_headings = DAY_HEADINGS_JS;
	}

	month = 0;
	switch(argc) {
	case 2:
		month = parsemonth(*argv++);
		if (!month)
			errx(1, "Unable to parse month");
		/* FALLTHROUGH */
	case 1:
		if (argc == 1 && !isdigit((unsigned char)*argv[0])) {
			month = parsemonth(*argv);
			if (!month)
				errx(1, "illegal year value: use 1-9999");
			(void)time(&now);
			local_time = localtime(&now);
			year = local_time->tm_year + TM_YEAR_BASE;
		} else {
			year = strtonum(*argv, 1, 9999, &errstr);
			if (errstr)
				errx(1, "illegal year value: use 1-9999");
		}
		break;
	case 0:
		(void)time(&now);
		local_time = localtime(&now);
		year = local_time->tm_year + TM_YEAR_BASE;
		if (!yflag)
			month = local_time->tm_mon + 1;
		break;
	default:
		usage();
	}

	if (month)
		monthly(month, year);
	else if (julian)
		j_yearly(year);
	else
		yearly(year);
	exit(0);
}

#define	DAY_LEN		3		/* 3 spaces per day */
#define	J_DAY_LEN	4		/* 4 spaces per day */
#define	WEEK_LEN	20		/* 7 * 3 - one space at the end */
#define WEEKNUMBER_LEN	5		/* 5 spaces per week number */
#define	J_WEEK_LEN	27		/* 7 * 4 - one space at the end */
#define	HEAD_SEP	2		/* spaces between day headings */
#define	J_HEAD_SEP	2

int
week(int day, int month, int year)
{
	int	yearday;
	int	firstweekday;
	int	weekday;
	int	firstday;
	int	firstsunday;
	int	shift;
	
	if (mflag)
		return isoweek(day, month, year);

	yearday = day_in_year(day, month, year);
	firstweekday = day_in_week(1, 1, year) + 1;
	weekday = day_in_week(day, month, year) + 1;
	firstday = day_in_year(1, 1, year);
	firstsunday = firstday + (8 - firstweekday);

	shift = 1;
	if (yearday < firstsunday)
		return (1);
	if (firstweekday > THURSDAY - 1) 
		shift = 2;
	return ((((yearday + 1) - (weekday - 1)) / 7) + shift);
}

int
isoweek(int day, int month, int year)
{
	/* http://www.tondering.dk/claus/cal/node8.html */
	int a, b, c, s, e, f, g, d, n;

	a = month <= 2 ? year - 1 : year;
	b = a/4 - a/100 + a/400;
	c = (a-1)/4 - (a-1)/100 + (a-1)/400;
	s = b - c;
	if (month <= 2) {
		e = 0;
		f = day - 1 + 31 * (month-1);
	} else {
		e = s + 1;
		f = day + ((153 * (month-3) + 2) / 5) + 58 + s;
	}
	g = (a + b) % 7;
	d = (f + g - e) % 7;
	n = f + 3 - d;

	if (n < 0)
		return 53 - (g - s) / 5;
	else if (n > 364 + s)
		return 1;
	else 
		return n/7 + 1;
}

void
monthly(int month, int year)
{
	int col, row, len, days[MAXDAYS], firstday;
	char *p, lineout[30];

	day_array(month, year, days);
	(void)snprintf(lineout, sizeof(lineout), "%s %d",
	    month_names[month - 1], year);
	len = strlen(lineout);
	(void)printf("%*s%s\n%s\n",
	    ((julian ? J_WEEK_LEN : WEEK_LEN) - len) / 2, "",
	    lineout, day_headings);
	for (row = 0; row < 6; row++) {
		firstday = SPACE;
		for (col = 0, p = lineout; col < 7; col++,
		    p += julian ? J_DAY_LEN : DAY_LEN) {
			if (firstday == SPACE && days[row * 7 + col] != SPACE)
				firstday = days[row * 7 + col];
			ascii_day(p, days[row * 7 + col]);
		}
		*p = '\0';
		trim_trailing_spaces(lineout);
		(void)printf("%-20s", lineout);
		if (wflag && firstday != SPACE)
			printf(" [%2d]", week(firstday, month, year));
		printf("\n");
	}
}

void
j_yearly(int year)
{
	int col, *dp, i, month, row, which_cal;
	int days[12][MAXDAYS];
	char *p, lineout[80];

	(void)snprintf(lineout, sizeof(lineout), "%d", year);
	center(lineout, J_WEEK_LEN * 2 + J_HEAD_SEP, 0);
	(void)printf("\n\n");
	for (i = 0; i < 12; i++)
		day_array(i + 1, year, days[i]);
	(void)memset(lineout, ' ', sizeof(lineout) - 1);
	lineout[sizeof(lineout) - 1] = '\0';
	for (month = 0; month < 12; month += 2) {
		center(month_names[month], J_WEEK_LEN, J_HEAD_SEP);
		center(month_names[month + 1], J_WEEK_LEN, 0);
		(void)printf("\n%s%*s%s\n", day_headings,
		    J_HEAD_SEP, "", day_headings);

		for (row = 0; row < 6; row++) {
			for (which_cal = 0; which_cal < 2; which_cal++) {
				p = lineout + which_cal * (J_WEEK_LEN + 2);
				dp = &days[month + which_cal][row * 7];
				for (col = 0; col < 7; col++, p += J_DAY_LEN)
					ascii_day(p, *dp++);
			}
			*p = '\0';
			trim_trailing_spaces(lineout);
			(void)printf("%s\n", lineout);
		}
	}
	(void)printf("\n");
}

void
yearly(int year)
{
	int col, *dp, i, month, row, which_cal, week_len, wn, firstday;
	int days[12][MAXDAYS];
	char *p, lineout[81];

	week_len = WEEK_LEN;
	if (wflag)
		week_len += WEEKNUMBER_LEN;
	(void)snprintf(lineout, sizeof(lineout), "%d", year);
	center(lineout, week_len * 3 + HEAD_SEP * 2, 0);
	(void)printf("\n\n");
	for (i = 0; i < 12; i++)
		day_array(i + 1, year, days[i]);
	(void)memset(lineout, ' ', sizeof(lineout) - 1);
	lineout[sizeof(lineout) - 1] = '\0';
	for (month = 0; month < 12; month += 3) {
		center(month_names[month], week_len, HEAD_SEP);
		center(month_names[month + 1], week_len, HEAD_SEP);
		center(month_names[month + 2], week_len, 0);
		(void)printf("\n%s%*s%s%*s%s\n", day_headings,
		    HEAD_SEP + (wflag ? WEEKNUMBER_LEN : 0), "", day_headings,
		    HEAD_SEP + (wflag ? WEEKNUMBER_LEN : 0), "", day_headings);

		for (row = 0; row < 6; row++) {
			for (which_cal = 0; which_cal < 3; which_cal++) {
				p = lineout + which_cal * (week_len + 2);
				
				dp = &days[month + which_cal][row * 7];
				firstday = SPACE;
				for (col = 0; col < 7; col++, p += DAY_LEN) {
					if (firstday == SPACE && *dp != SPACE)
						firstday = *dp;
					ascii_day(p, *dp++);
				}
				if (wflag && firstday != SPACE) {
					wn = week(firstday,
					    month + which_cal + 1, year);
					(void)snprintf(p, 5, "[%2d]", wn);
					p += strlen(p);
					*p = ' ';
				} else
					memset(p, ' ', 4);
			}
			*p = '\0';
			trim_trailing_spaces(lineout);
			(void)printf("%s\n", lineout);
		}
	}
	(void)printf("\n");
}

/*
 * day_array --
 *	Fill in an array of 42 integers with a calendar.  Assume for a moment
 *	that you took the (maximum) 6 rows in a calendar and stretched them
 *	out end to end.  You would have 42 numbers or spaces.  This routine
 *	builds that array for any month from Jan. 1 through Dec. 9999.
 */
void
day_array(int month, int year, int *days)
{
	int day, dw, dm;

	if (month == 9 && year == 1752) {
		memmove(days, sep1752, MAXDAYS * sizeof(int));
		return;
	}
	memmove(days, empty, MAXDAYS * sizeof(int));
	dm = days_in_month[leap_year(year)][month];
	dw = day_in_week(mflag?0:1, month, year);
	day = julian ? day_in_year(1, month, year) : 1;
	while (dm--)
		days[dw++] = day++;
}

/*
 * day_in_year --
 *	return the 1 based day number within the year
 */
int
day_in_year(int day, int month, int year)
{
	int i, leap;

	leap = leap_year(year);
	for (i = 1; i < month; i++)
		day += days_in_month[leap][i];
	return (day);
}

/*
 * day_in_week
 *	return the 0 based day number for any date from 1 Jan. 1 to
 *	31 Dec. 9999.  Assumes the Gregorian reformation eliminates
 *	3 Sep. 1752 through 13 Sep. 1752.  Returns Thursday for all
 *	missing days.
 */
int
day_in_week(int day, int month, int year)
{
	long temp;

	temp = (long)(year - 1) * 365 + leap_years_since_year_1(year - 1)
	    + day_in_year(day, month, year);
	if (temp < FIRST_MISSING_DAY)
		return ((temp - 1 + SATURDAY) % 7);
	if (temp >= (FIRST_MISSING_DAY + NUMBER_MISSING_DAYS))
		return (((temp - 1 + SATURDAY) - NUMBER_MISSING_DAYS) % 7);
	return (THURSDAY);
}

void
ascii_day(char *p, int day)
{
	int display, val;
	static const char *aday[] = {
		"",
		" 1", " 2", " 3", " 4", " 5", " 6", " 7",
		" 8", " 9", "10", "11", "12", "13", "14",
		"15", "16", "17", "18", "19", "20", "21",
		"22", "23", "24", "25", "26", "27", "28",
		"29", "30", "31",
	};

	if (day == SPACE) {
		memset(p, ' ', julian ? J_DAY_LEN : DAY_LEN);
		return;
	}
	if (julian) {
		val = day / 100;
		if (val) {
			day %= 100;
			*p++ = val + '0';
			display = 1;
		} else {
			*p++ = ' ';
			display = 0;
		}
		val = day / 10;
		if (val || display)
			*p++ = val + '0';
		else
			*p++ = ' ';
		*p++ = day % 10 + '0';
	} else {
		*p++ = aday[day][0];
		*p++ = aday[day][1];
	}
	*p = ' ';
}

void
trim_trailing_spaces(char *s)
{
	char *p;

	for (p = s; *p; ++p)
		continue;
	while (p > s && isspace((unsigned char)*--p))
		continue;
	if (p > s)
		++p;
	*p = '\0';
}

void
center(const char *str, int len, int separate)
{

	len -= strlen(str);
	(void)printf("%*s%s%*s", len / 2, "", str,
	    len / 2 + len % 2 + separate, "");
}

void
usage(void)
{

	(void)fprintf(stderr, "usage: cal [-jmwy] [month] [year]\n");
	exit(1);
}

int
parsemonth(const char *s)
{
	struct tm tm;
	char *cp;
	int v;

	v = (int)strtol(s, &cp, 10);
	if (*cp != '\0') {		/* s wasn't purely numeric */
		v = 0;
		if ((cp = strptime(s, "%b", &tm)) != NULL && *cp == '\0')
			v = tm.tm_mon + 1;
	}
	if (v <= 0 || v > 12)
		errx(1, "invalid month: use 1-12 or a name");
	return (v);
}
