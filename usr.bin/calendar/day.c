/*	$OpenBSD: day.c,v 1.17 2004/12/10 15:31:01 mickey Exp $	*/

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

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1989, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static const char sccsid[] = "@(#)calendar.c  8.3 (Berkeley) 3/25/94";
#else
static const char rcsid[] = "$OpenBSD: day.c,v 1.17 2004/12/10 15:31:01 mickey Exp $";
#endif
#endif /* not lint */

#include <sys/types.h>
#include <sys/uio.h>

#include <ctype.h>
#include <err.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <tzfile.h>

#include "pathnames.h"
#include "calendar.h"

#define WEEKLY 1
#define MONTHLY 2
#define YEARLY 3

struct tm *tp;
int *cumdays, offset;
char dayname[10];
enum calendars calendar;
u_long julian;


/* 1-based month, 0-based days, cumulative */
int daytab[][14] = {
	{ 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, 364 },
	{ 0, -1, 30, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 },
};

static char *days[] = {
	"sun", "mon", "tue", "wed", "thu", "fri", "sat", NULL,
};

static char *months[] = {
	"jan", "feb", "mar", "apr", "may", "jun",
	"jul", "aug", "sep", "oct", "nov", "dec", NULL,
};

static struct fixs fndays[8];         /* full national days names */
static struct fixs ndays[8];          /* short national days names */

static struct fixs fnmonths[13];      /* full national months names */
static struct fixs nmonths[13];       /* short national month names */


void setnnames(void)
{
	char buf[80];
	int i, l;
	struct tm tm;

	for (i = 0; i < 7; i++) {
		tm.tm_wday = i;
		l = strftime(buf, sizeof(buf), "%a", &tm);
		for (; l > 0 && isspace((int)buf[l - 1]); l--)
			;
		buf[l] = '\0';
		if (ndays[i].name != NULL)
			free(ndays[i].name);
		if ((ndays[i].name = strdup(buf)) == NULL)
			err(1, NULL);
		ndays[i].len = strlen(buf);

		l = strftime(buf, sizeof(buf), "%A", &tm);
		for (; l > 0 && isspace((int)buf[l - 1]); l--)
			;
		buf[l] = '\0';
		if (fndays[i].name != NULL)
			free(fndays[i].name);
		if ((fndays[i].name = strdup(buf)) == NULL)
			err(1, NULL);
		fndays[i].len = strlen(buf);
	}

	for (i = 0; i < 12; i++) {
		tm.tm_mon = i;
		l = strftime(buf, sizeof(buf), "%b", &tm);
		for (; l > 0 && isspace((int)buf[l - 1]); l--)
			;
		buf[l] = '\0';
		if (nmonths[i].name != NULL)
			free(nmonths[i].name);
		if ((nmonths[i].name = strdup(buf)) == NULL)
			err(1, NULL);
		nmonths[i].len = strlen(buf);

		l = strftime(buf, sizeof(buf), "%B", &tm);
		for (; l > 0 && isspace((int)buf[l - 1]); l--)
			;
		buf[l] = '\0';
		if (fnmonths[i].name != NULL)
			free(fnmonths[i].name);
		if ((fnmonths[i].name = strdup(buf)) == NULL)
			err(1, NULL);
		fnmonths[i].len = strlen(buf);
	}
	/* Hardwired special events */
	spev[0].name = strdup(EASTER);
	spev[0].nlen = EASTERNAMELEN;
	spev[0].getev = easter;
	spev[1].name = strdup(PASKHA);
	spev[1].nlen = PASKHALEN;
	spev[1].getev = paskha;
	for (i = 0; i < NUMEV; i++) {
		if (spev[i].name == NULL)
			err(1, NULL);
		spev[i].uname = NULL;
	}
}

void
settime(now)
	time_t *now;
{
	tp = localtime(now);
	tp->tm_sec = 0;
	tp->tm_min = 0;
	/* Avoid getting caught by a timezone shift; set time to noon */
	tp->tm_isdst = 0;
	tp->tm_hour = 12;
	*now = mktime(tp);
	if (isleap(tp->tm_year + TM_YEAR_BASE))
		cumdays = daytab[1];
	else
		cumdays = daytab[0];
	/* Friday displays Monday's events */
	offset = tp->tm_wday == 5 ? 3 : 1;
	if (f_dayAfter)
		offset = 0;	/* Except not when range is set explicitly */
	header[5].iov_base = dayname;

	(void) setlocale(LC_TIME, "C");
	header[5].iov_len = strftime(dayname, sizeof(dayname), "%A", tp);
	(void) setlocale(LC_TIME, "");

	setnnames();
}

/* convert [Year][Month]Day into unix time (since 1970)
 * Year: two or four digits, Month: two digits, Day: two digits
 */
time_t Mktime (date)
    char *date;
{
    time_t t;
    int len;
    struct tm tm;

    (void)time(&t);
    tp = localtime(&t);

    len = strlen(date);
    if (len < 2)
	return((time_t)-1);
    tm.tm_sec = 0;
    tm.tm_min = 0;
    /* Avoid getting caught by a timezone shift; set time to noon */
    tm.tm_isdst = 0;
    tm.tm_hour = 12;
    tm.tm_wday = 0;
    tm.tm_mday = tp->tm_mday;
    tm.tm_mon = tp->tm_mon;
    tm.tm_year = tp->tm_year;

    /* Day */
    tm.tm_mday = atoi(date + len - 2);

    /* Month */
    if (len >= 4) {
	*(date + len - 2) = '\0';
	tm.tm_mon = atoi(date + len - 4) - 1;
    }

    /* Year */
    if (len >= 6) {
		*(date + len - 4) = '\0';
		tm.tm_year = atoi(date);

	/* tm_year up TM_YEAR_BASE ... */
	if (tm.tm_year < 69)		/* Y2K */
		tm.tm_year += 2000 - TM_YEAR_BASE;
	else if (tm.tm_year < 100)
		tm.tm_year += 1900 - TM_YEAR_BASE;
	else if (tm.tm_year > TM_YEAR_BASE)
		tm.tm_year -= TM_YEAR_BASE;
    }

#if DEBUG
    printf("Mktime: %d %d %d %s\n", (int)mktime(&tm), (int)t, len,
	   asctime(&tm));
#endif
    return(mktime(&tm));
}

void
adjust_calendar(int *day, int *month)
{
	switch (calendar) {
	case GREGORIAN:
		break;

	case JULIAN:
		*day += julian;
		if (*day > (cumdays[*month + 1] - cumdays[*month])) {
			*day -= (cumdays[*month + 1] - cumdays[*month]);
			if (++*month > 12)
				*month = 1;
		}
		break;
	case LUNAR:
		break;
	}
}

/*
 * Possible date formats include any combination of:
 *	3-charmonth			(January, Jan, Jan)
 *	3-charweekday			(Friday, Monday, mon.)
 *	numeric month or day		(1, 2, 04)
 *
 * Any character except \t or '*' may separate them, or they may not be
 * separated.  Any line following a line that is matched, that starts
 * with \t, is shown along with the matched line.
 */
struct match *
isnow(endp, bodun)
	char	*endp;
	int	bodun;
{
	int day = 0, flags = 0, month = 0, v1, v2, i;
	int monthp, dayp, varp = 0;
	struct match *matches = NULL, *tmp, *tmp2;
	int interval = YEARLY;	/* how frequently the event repeats. */
	int vwd = 0;	/* Variable weekday */
	time_t tdiff, ttmp;
	struct tm tmtmp;

	/*
	 * CONVENTION
	 *
	 * Month:     1-12
	 * Monthname: Jan .. Dec
	 * Day:       1-31
	 * Weekday:   Mon-Sun
	 *
	 */

	/* read first field */
	/* didn't recognize anything, skip it */
	if (!(v1 = getfield(endp, &endp, &flags)))
		return (NULL);

	/* adjust bodun rate */
	if (bodun && !bodun_always)
		bodun = !(arc4random() % 3);
		
	/* Easter or Easter depending days */
	if (flags & F_SPECIAL)
		vwd = v1;

	 /*
	  * 1. {Weekday,Day} XYZ ...
	  *
	  *    where Day is > 12
	  */
	else if (flags & F_ISDAY || v1 > 12) {

		/* found a day; day: 13-31 or weekday: 1-7 */
		day = v1;

		/* {Day,Weekday} {Month,Monthname} ... */
		/* if no recognizable month, assume just a day alone -- this is
		 * very unlikely and can only happen after the first 12 days.
		 * --find month or use current month */
		if (!(month = getfield(endp, &endp, &flags))) {
			month = tp->tm_mon + 1;
			/* F_ISDAY is set only if a weekday was spelled out */
			/* F_ISDAY must be set if 0 < day < 8 */
			if ((day <= 7) && (day >= 1))
				interval = WEEKLY;
			else
				interval = MONTHLY;
		} else if ((day <= 7) && (day >= 1))
			day += 10;
			/* it's a weekday; make it the first one of the month */
		if (month == -1) {
			month = tp->tm_mon + 1;
			interval = MONTHLY;
		} else if (calendar)
			adjust_calendar(&day, &month);
		if ((month > 12) || (month < 1))
			return (NULL);
	}

	/* 2. {Monthname} XYZ ... */
	else if (flags & F_ISMONTH) {
		month = v1;
		if (month == -1) {
			month = tp->tm_mon + 1;
			interval = MONTHLY;
		}
		/* Monthname {day,weekday} */
		/* if no recognizable day, assume the first day in month */
		if (!(day = getfield(endp, &endp, &flags)))
			day = 1;
		/* If a weekday was spelled out without an ordering,
		 * assume the first of that day in the month */
		if ((flags & F_ISDAY)) {
			if ((day >= 1) && (day <=7))
				day += 10;
		} else if (calendar)
			adjust_calendar(&day, &month);
	}

	/* Hm ... */
	else {
		v2 = getfield(endp, &endp, &flags);

		/*
		 * {Day} {Monthname} ...
		 * where Day <= 12
		 */
		if (flags & F_ISMONTH) {
			day = v1;
			month = v2;
			if (month == -1) {
				month = tp->tm_mon + 1;
				interval = MONTHLY;
			} else if (calendar)
				adjust_calendar(&day, &month);
		}

		/* {Month} {Weekday,Day} ...  */
		else {
			/* F_ISDAY set, v2 > 12, or no way to tell */
			month = v1;
			/* if no recognizable day, assume the first */
			day = v2 ? v2 : 1;
			if ((flags & F_ISDAY)) {
				if ((day >= 1) && (day <= 7))
					day += 10;
			} else
				adjust_calendar(&day, &month);
		}
	}

	/* convert Weekday into *next*  Day,
	 * e.g.: 'Sunday' -> 22
	 *       'SundayLast' -> ??
	 */
	if (flags & F_ISDAY) {
#if DEBUG
		fprintf(stderr, "\nday: %d %s month %d\n", day, endp, month);
#endif

		varp = 1;
		/* variable weekday, SundayLast, MondayFirst ... */
		if (day < 0 || day >= 10)
			vwd = day;
		else {
			day = tp->tm_mday + (((day - 1) - tp->tm_wday + 7) % 7);
			interval = WEEKLY;
		}
	} else
	/* Check for silliness.  Note we still catch Feb 29 */
		if (!(flags & F_SPECIAL) &&
		    (day > (cumdays[month + 1] - cumdays[month]) || day < 1)) {
			if (!((month == 2 && day == 29) ||
			    (interval == MONTHLY && day <= 31)))
				return (NULL);
		}

	if (!(flags & F_SPECIAL)) {
		monthp = month;
		dayp = day;
		day = cumdays[month] + day;
#if DEBUG
		fprintf(stderr, "day2: day %d(%d) yday %d\n", dayp, day, tp->tm_yday);
#endif
	/* Speed up processing for the most common situation:  yearly events
	 * when the interval being checked is less than a month or so (this
	 * could be less than a year, but then we have to start worrying about
	 * leap years).  Only one event can match, and it's easy to find.
	 * Note we can't check special events, because they can wander widely.
	 */
		if (((v1 = offset + f_dayAfter) < 50) && (interval == YEARLY)) {
			memcpy(&tmtmp, tp, sizeof(struct tm));
			tmtmp.tm_mday = dayp;
			tmtmp.tm_mon = monthp - 1;
			if (vwd) {
			/* We want the event next year if it's late now
			 * this year.  The 50-day limit means we don't have to
			 * worry if next year is or isn't a leap year.
			 */
				if (tp->tm_yday > 300 && tmtmp.tm_mon <= 1)
					variable_weekday(&vwd, tmtmp.tm_mon + 1,
					    tmtmp.tm_year + TM_YEAR_BASE + 1);
				else
					variable_weekday(&vwd, tmtmp.tm_mon + 1,
					    tmtmp.tm_year + TM_YEAR_BASE);
				day = cumdays[tmtmp.tm_mon + 1] + vwd;
				tmtmp.tm_mday = vwd;
			}
			v2 = day - tp->tm_yday;
			if ((v2 > v1) || (v2 < 0)) {
				if ((v2 += isleap(tp->tm_year + TM_YEAR_BASE) ? 366 : 365)
				    <= v1)
					tmtmp.tm_year++;
				else if(!bodun || (day - tp->tm_yday) != -1)
					return(NULL);
			}
			if ((tmp = malloc(sizeof(struct match))) == NULL)
				err(1, NULL);

			if (bodun && (day - tp->tm_yday) == -1) {
				tmp->when = f_time - 1 * SECSPERDAY;
				tmtmp.tm_mday++;
				tmp->bodun = 1;
			} else {
				tmp->when = f_time + v2 * SECSPERDAY;
				tmp->bodun = 0;
			}

			(void)mktime(&tmtmp);
			if (strftime(tmp->print_date,
			    sizeof(tmp->print_date),
			/*    "%a %b %d", &tm);  Skip weekdays */
			    "%b %d", &tmtmp) == 0)
				tmp->print_date[sizeof(tmp->print_date) - 1] = '\0';

			tmp->var   = varp;
			tmp->next  = NULL;
			return(tmp);
		}
	} else {
		varp = 1;
		/* Set up v1 to the event number and ... */
		v1 = vwd % (NUMEV + 1) - 1;
		vwd /= (NUMEV + 1);
		if (v1 < 0) {
			v1 += NUMEV + 1;
			vwd--;
		}
		dayp = monthp = 1;	/* Why not */
	}

	/* Compare to past and coming instances of the event.  The i == 0 part
	 * of the loop corresponds to this specific instance.  Note that we
	 * can leave things sort of higgledy-piggledy since a mktime() happens
	 * on this before anything gets printed.  Also note that even though
	 * we've effectively gotten rid of f_dayBefore, we still have to check
	 * the one prior event for situations like "the 31st of every month"
	 * and "yearly" events which could happen twice in one year but not in
	 * the next */
	tmp2 = matches;
	for (i = -1; i < 2; i++) {
		memcpy(&tmtmp, tp, sizeof(struct tm));
		tmtmp.tm_mday = dayp;
		tmtmp.tm_mon = month = monthp - 1;
		do {
			v2 = 0;
			switch (interval) {
			case WEEKLY:
				tmtmp.tm_mday += 7 * i;
				break;
			case MONTHLY:
				month += i;
				tmtmp.tm_mon = month;
				switch(tmtmp.tm_mon) {
				case -1:
					tmtmp.tm_mon = month = 11;
					tmtmp.tm_year--;
					break;
				case 12:
					tmtmp.tm_mon = month = 0;
					tmtmp.tm_year++;
					break;
				}
				if (vwd) {
					v1 = vwd;
					variable_weekday(&v1, tmtmp.tm_mon + 1,
					    tmtmp.tm_year + TM_YEAR_BASE);
					tmtmp.tm_mday = v1;
				} else
					tmtmp.tm_mday = dayp;
				break;
			case YEARLY:
			default:
				tmtmp.tm_year += i;
				if (flags & F_SPECIAL) {
					tmtmp.tm_mon = 0;	/* Gee, mktime() is nice */
					tmtmp.tm_mday = spev[v1].getev(tmtmp.tm_year +
					    TM_YEAR_BASE) + vwd;
				} else if (vwd) {
					v1 = vwd;
					variable_weekday(&v1, tmtmp.tm_mon + 1,
					    tmtmp.tm_year + TM_YEAR_BASE);
					tmtmp.tm_mday = v1;
				} else {
				/* Need the following to keep Feb 29 from
				 * becoming Mar 1 */
				tmtmp.tm_mday = dayp;
				tmtmp.tm_mon = monthp - 1;
				}
				break;
			}
			/* How many days apart are we */
			if ((ttmp = mktime(&tmtmp)) == -1)
				warnx("time out of range: %s", endp);
			else {
				tdiff = difftime(ttmp, f_time)/ SECSPERDAY;
				if (tdiff <= offset + f_dayAfter ||
				    (bodun && tdiff == -1)) {
					if (tdiff >=  0 ||
					    (bodun && tdiff == -1)) {
					if ((tmp = malloc(sizeof(struct match))) == NULL)
						err(1, NULL);
					tmp->when = ttmp;
					if (strftime(tmp->print_date,
					    sizeof(tmp->print_date),
					/*    "%a %b %d", &tm);  Skip weekdays */
					    "%b %d", &tmtmp) == 0)
						tmp->print_date[sizeof(tmp->print_date) - 1] = '\0';
					tmp->bodun = bodun && tdiff == -1;
					tmp->var   = varp;
					tmp->next  = NULL;
					if (tmp2)
						tmp2->next = tmp;
					else
						matches = tmp;
					tmp2 = tmp;
					v2 = (i == 1) ? 1 : 0;
					}
				} else
					i = 2; /* No point checking in the future */
			}
		} while (v2 != 0);
	}
	return (matches);
}


int
getmonth(s)
	char *s;
{
	char **p;
	struct fixs *n;

	for (n = fnmonths; n->name; ++n)
		if (!strncasecmp(s, n->name, n->len))
			return ((n - fnmonths) + 1);
	for (n = nmonths; n->name; ++n)
		if (!strncasecmp(s, n->name, n->len))
			return ((n - nmonths) + 1);
	for (p = months; *p; ++p)
		if (!strncasecmp(s, *p, 3))
			return ((p - months) + 1);
	return (0);
}


int
getday(s)
	char *s;
{
	char **p;
	struct fixs *n;

	for (n = fndays; n->name; ++n)
		if (!strncasecmp(s, n->name, n->len))
			return ((n - fndays) + 1);
	for (n = ndays; n->name; ++n)
		if (!strncasecmp(s, n->name, n->len))
			return ((n - ndays) + 1);
	for (p = days; *p; ++p)
		if (!strncasecmp(s, *p, 3))
			return ((p - days) + 1);
	return (0);
}

/* return offset for variable weekdays
 * -1 -> last weekday in month
 * +1 -> first weekday in month
 * ... etc ...
 */
int
getdayvar(s)
	char *s;
{
	int offset;


	offset = strlen(s);

	/* Sun+1 or Wednesday-2
	 *    ^              ^   */

	/* printf ("x: %s %s %d\n", s, s + offset - 2, offset); */
	switch(*(s + offset - 2)) {
	case '-':
	case '+':
	    return(atoi(s + offset - 2));
	    break;
	}

	/*
	 * some aliases: last, first, second, third, fourth
	 */

	/* last */
	if      (offset > 4 && !strcasecmp(s + offset - 4, "last"))
	    return(-1);
	else if (offset > 5 && !strcasecmp(s + offset - 5, "first"))
	    return(+1);
	else if (offset > 6 && !strcasecmp(s + offset - 6, "second"))
	    return(+2);
	else if (offset > 5 && !strcasecmp(s + offset - 5, "third"))
	    return(+3);
	else if (offset > 6 && !strcasecmp(s + offset - 6, "fourth"))
	    return(+4);

	/* no offset detected */
	return(0);
}


int
foy(year)
	int year;
{
	/* 0-6; what weekday Jan 1 is */
	year--;
	return ((1 - year/100 + year/400 + (int)(365.25 * year)) % 7);
}



void
variable_weekday(day, month, year)
	int *day, month, year;
{
	int v1, v2;
	int *cumdays;
	int day1;

	if (isleap(year))
		cumdays = daytab[1];
	else
		cumdays = daytab[0];
	day1 = foy(year);
	/* negative offset; last, -4 .. -1 */
	if (*day < 0) {
		v1 = *day/10 - 1;          /* offset -4 ... -1 */
		*day = 10 + (*day % 10);    /* day 1 ... 7 */

		/* which weekday the end of the month is (1-7) */
		v2 = (cumdays[month + 1] + day1) % 7 + 1;

		/* and subtract enough days */
		*day = cumdays[month + 1] - cumdays[month] +
		    (v1 + 1) * 7 - (v2 - *day + 7) % 7;
#if DEBUG
		fprintf(stderr, "\nMonth %d ends on weekday %d\n", month, v2);
#endif
	}

	/* first, second ... +1 ... +5 */
	else {
		v1 = *day/10;        /* offset */
		*day = *day % 10;

		/* which weekday the first of the month is (1-7) */
		v2 = (cumdays[month] + 1 + day1) % 7 + 1;

		/* and add enough days */
		*day = 1 + (v1 - 1) * 7 + (*day - v2 + 7) % 7;
#if DEBUG
		fprintf(stderr, "\nMonth %d starts on weekday %d\n", month, v2);
#endif
	}
}
