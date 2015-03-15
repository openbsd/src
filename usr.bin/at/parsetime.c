/*	$OpenBSD: parsetime.c,v 1.21 2015/03/15 00:41:27 millert Exp $	*/

/*
 * parsetime.c - parse time for at(1)
 * Copyright (C) 1993, 1994  Thomas Koenig
 *
 * modifications for english-language times
 * Copyright (C) 1993  David Parsons
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author(s) may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  at [NOW] PLUS NUMBER MINUTES|HOURS|DAYS|WEEKS|MONTHS|YEARS
 *     /NUMBER [DOT NUMBER] [AM|PM]\ /[MONTH NUMBER [NUMBER]]             \
 *     |NOON                       | |[TOMORROW]                          |
 *     |MIDNIGHT                   | |[DAY OF WEEK]                       |
 *     \TEATIME                    / |NUMBER [SLASH NUMBER [SLASH NUMBER]]|
 *                                   \PLUS NUMBER MINUTES|HOURS|DAYS|WEEKS|MONTHS|YEARS/
 */

#include <sys/types.h>
#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "globals.h"
#include "at.h"

/* Structures and unions */

enum {	/* symbols */
	MIDNIGHT, NOON, TEATIME,
	PM, AM, TOMORROW, TODAY, NOW,
	MINUTES, HOURS, DAYS, WEEKS, MONTHS, YEARS,
	NUMBER, NEXT, PLUS, DOT, SLASH, ID, JUNK,
	JAN, FEB, MAR, APR, MAY, JUN,
	JUL, AUG, SEP, OCT, NOV, DEC,
	SUN, MON, TUE, WED, THU, FRI, SAT
};

/*
 * parse translation table - table driven parsers can be your FRIEND!
 */
struct {
	char *name;	/* token name */
	int value;	/* token id */
	int plural;	/* is this plural? */
} Specials[] = {
	{ "midnight", MIDNIGHT, 0 },	/* 00:00:00 of today or tomorrow */
	{ "noon", NOON, 0 },		/* 12:00:00 of today or tomorrow */
	{ "teatime", TEATIME, 0 },	/* 16:00:00 of today or tomorrow */
	{ "am", AM, 0 },		/* morning times for 0-12 clock */
	{ "pm", PM, 0 },		/* evening times for 0-12 clock */
	{ "tomorrow", TOMORROW, 0 },	/* execute 24 hours from time */
	{ "today", TODAY, 0 },		/* execute today - don't advance time */
	{ "now", NOW, 0 },		/* opt prefix for PLUS */
	{ "next", NEXT, 0 },		/* opt prefix for + 1 */

	{ "minute", MINUTES, 0 },	/* minutes multiplier */
	{ "min", MINUTES, 0 },
	{ "m", MINUTES, 0 },
	{ "minutes", MINUTES, 1 },	/* (pluralized) */
	{ "hour", HOURS, 0 },		/* hours ... */
	{ "hr", HOURS, 0 },		/* abbreviated */
	{ "h", HOURS, 0 },
	{ "hours", HOURS, 1 },		/* (pluralized) */
	{ "day", DAYS, 0 },		/* days ... */
	{ "d", DAYS, 0 },
	{ "days", DAYS, 1 },		/* (pluralized) */
	{ "week", WEEKS, 0 },		/* week ... */
	{ "w", WEEKS, 0 },
	{ "weeks", WEEKS, 1 },		/* (pluralized) */
	{ "month", MONTHS, 0 },		/* month ... */
	{ "mo", MONTHS, 0 },
	{ "mth", MONTHS, 0 },
	{ "months", MONTHS, 1 },	/* (pluralized) */
	{ "year", YEARS, 0 },		/* year ... */
	{ "y", YEARS, 0 },
	{ "years", YEARS, 1 },		/* (pluralized) */
	{ "jan", JAN, 0 },
	{ "feb", FEB, 0 },
	{ "mar", MAR, 0 },
	{ "apr", APR, 0 },
	{ "may", MAY, 0 },
	{ "jun", JUN, 0 },
	{ "jul", JUL, 0 },
	{ "aug", AUG, 0 },
	{ "sep", SEP, 0 },
	{ "oct", OCT, 0 },
	{ "nov", NOV, 0 },
	{ "dec", DEC, 0 },
	{ "january", JAN,0 },
	{ "february", FEB,0 },
	{ "march", MAR,0 },
	{ "april", APR,0 },
	{ "may", MAY,0 },
	{ "june", JUN,0 },
	{ "july", JUL,0 },
	{ "august", AUG,0 },
	{ "september", SEP,0 },
	{ "october", OCT,0 },
	{ "november", NOV,0 },
	{ "december", DEC,0 },
	{ "sunday", SUN, 0 },
	{ "sun", SUN, 0 },
	{ "monday", MON, 0 },
	{ "mon", MON, 0 },
	{ "tuesday", TUE, 0 },
	{ "tue", TUE, 0 },
	{ "wednesday", WED, 0 },
	{ "wed", WED, 0 },
	{ "thursday", THU, 0 },
	{ "thu", THU, 0 },
	{ "friday", FRI, 0 },
	{ "fri", FRI, 0 },
	{ "saturday", SAT, 0 },
	{ "sat", SAT, 0 },
};

static char **scp;	/* scanner - pointer at arglist */
static int scc;		/* scanner - count of remaining arguments */
static char *sct;	/* scanner - next char pointer in current argument */
static int need;	/* scanner - need to advance to next argument */
static char *sc_token;	/* scanner - token buffer */
static size_t sc_len;   /* scanner - length of token buffer */
static int sc_tokid;	/* scanner - token id */
static int sc_tokplur;	/* scanner - is token plural? */

/*
 * parse a token, checking if it's something special to us
 */
static int
parse_token(char *arg)
{
	int i;

	for (i=0; i < sizeof(Specials) / sizeof(Specials[0]); i++) {
		if (strcasecmp(Specials[i].name, arg) == 0) {
			sc_tokplur = Specials[i].plural;
		    	return (sc_tokid = Specials[i].value);
		}
	}

	/* not special - must be some random id */
	return (ID);
}


/*
 * init_scanner() sets up the scanner to eat arguments
 */
static int
init_scanner(int argc, char **argv)
{
	scp = argv;
	scc = argc;
	need = 1;
	sc_len = 1;
	while (argc-- > 0)
		sc_len += strlen(*argv++);

	if ((sc_token = (char *) malloc(sc_len)) == NULL) {
		fprintf(stderr, "%s: Insufficient virtual memory\n",
		    ProgramName);
		return (-1);
	}
	return (0);
}

/*
 * token() fetches a token from the input stream
 */
static int
token(void)
{
	int idx;

	for (;;) {
		bzero(sc_token, sc_len);
		sc_tokid = EOF;
		sc_tokplur = 0;
		idx = 0;

		/*
		 * if we need to read another argument, walk along the
		 * argument list; when we fall off the arglist, we'll
		 * just return EOF forever
		 */
		if (need) {
			if (scc < 1)
				return (sc_tokid);
			sct = *scp;
			scp++;
			scc--;
			need = 0;
		}
		/*
		 * eat whitespace now - if we walk off the end of the argument,
		 * we'll continue, which puts us up at the top of the while loop
		 * to fetch the next argument in
		 */
		while (isspace((unsigned char)*sct))
			++sct;
		if (!*sct) {
			need = 1;
			continue;
		}

		/*
		 * preserve the first character of the new token
		 */
		sc_token[0] = *sct++;

		/*
		 * then see what it is
		 */
		if (isdigit((unsigned char)sc_token[0])) {
			while (isdigit((unsigned char)*sct))
				sc_token[++idx] = *sct++;
			sc_token[++idx] = 0;
			return ((sc_tokid = NUMBER));
		} else if (isalpha((unsigned char)sc_token[0])) {
			while (isalpha((unsigned char)*sct))
				sc_token[++idx] = *sct++;
			sc_token[++idx] = 0;
			return (parse_token(sc_token));
		}
		else if (sc_token[0] == ':' || sc_token[0] == '.')
			return ((sc_tokid = DOT));
		else if (sc_token[0] == '+')
			return ((sc_tokid = PLUS));
		else if (sc_token[0] == '/')
			return ((sc_tokid = SLASH));
		else
			return ((sc_tokid = JUNK));
	}
}


/*
 * plonk() gives an appropriate error message if a token is incorrect
 */
static void
plonk(int tok)
{
	fprintf(stderr, "%s: %s time\n", ProgramName,
	    (tok == EOF) ? "incomplete" : "garbled");
}


/*
 * expect() gets a token and returns -1 if it's not the token we want
 */
static int
expect(int desired)
{
	if (token() != desired) {
		plonk(sc_tokid);
		return (-1);
	}
	return (0);
}


/*
 * dateadd() adds a number of minutes to a date.  It is extraordinarily
 * stupid regarding day-of-month overflow, and will most likely not
 * work properly
 */
static void
dateadd(int minutes, struct tm *tm)
{
	/* increment days */

	while (minutes > 24*60) {
		minutes -= 24*60;
		tm->tm_mday++;
	}

	/* increment hours */
	while (minutes > 60) {
		minutes -= 60;
		tm->tm_hour++;
		if (tm->tm_hour > 23) {
			tm->tm_mday++;
			tm->tm_hour = 0;
		}
	}

	/* increment minutes */
	tm->tm_min += minutes;

	if (tm->tm_min > 59) {
		tm->tm_hour++;
		tm->tm_min -= 60;

		if (tm->tm_hour > 23) {
			tm->tm_mday++;
			tm->tm_hour = 0;
		}
	}
}


/*
 * plus() parses a now + time
 *
 *  at [NOW] PLUS NUMBER [MINUTES|HOURS|DAYS|WEEKS|MONTHS|YEARS]
 *
 */
static int
plus(struct tm *tm)
{
	int increment;
	int expectplur;

	if (sc_tokid == NEXT) {
		increment = 1;
		expectplur = 0;
	} else {
		if (expect(NUMBER) != 0)
			return (-1);
		increment = atoi(sc_token);
		expectplur = (increment != 1) ? 1 : 0;
	}

	switch (token()) {
	case YEARS:
		tm->tm_year += increment;
		return (0);
	case MONTHS:
		tm->tm_mon += increment;
		while (tm->tm_mon >= 12) {
		    tm->tm_year++;
		    tm->tm_mon -= 12;
		}
		return (0);
	case WEEKS:
		increment *= 7;
		/* FALLTHROUGH */
	case DAYS:
		increment *= 24;
		/* FALLTHROUGH */
	case HOURS:
		increment *= 60;
		/* FALLTHROUGH */
	case MINUTES:
		if (expectplur != sc_tokplur)
			fprintf(stderr, "%s: pluralization is wrong\n",
			    ProgramName);
		dateadd(increment, tm);
		return (0);
	}

	plonk(sc_tokid);
	return (-1);
}


/*
 * tod() computes the time of day
 *     [NUMBER [DOT NUMBER] [AM|PM]]
 */
static int
tod(struct tm *tm)
{
	int hour, minute = 0;
	size_t tlen;

	hour = atoi(sc_token);
	tlen = strlen(sc_token);

	/*
	 * first pick out the time of day - if it's 4 digits, we assume
	 * a HHMM time, otherwise it's HH DOT MM time
	 */
	if (token() == DOT) {
		if (expect(NUMBER) != 0)
			return (-1);
		minute = atoi(sc_token);
		if (minute > 59)
			goto bad;
		token();
	} else if (tlen == 4) {
		minute = hour % 100;
		if (minute > 59)
			goto bad;
		hour = hour / 100;
	}

	/*
	 * check if an AM or PM specifier was given
	 */
	if (sc_tokid == AM || sc_tokid == PM) {
		if (hour > 12)
			goto bad;

		if (sc_tokid == PM) {
			if (hour != 12)	/* 12:xx PM is 12:xx, not 24:xx */
				hour += 12;
		} else {
			if (hour == 12)	/* 12:xx AM is 00:xx, not 12:xx */
				hour = 0;
		}
		token();
	} else if (hour > 23)
		goto bad;

	/*
	 * if we specify an absolute time, we don't want to bump the day even
	 * if we've gone past that time - but if we're specifying a time plus
	 * a relative offset, it's okay to bump things
	 */
	if ((sc_tokid == EOF || sc_tokid == PLUS || sc_tokid == NEXT) &&
	    tm->tm_hour > hour) {
		tm->tm_mday++;
		tm->tm_wday++;
	}

	tm->tm_hour = hour;
	tm->tm_min = minute;
	if (tm->tm_hour == 24) {
		tm->tm_hour = 0;
		tm->tm_mday++;
	}
	return (0);
bad:
	fprintf(stderr, "%s: garbled time\n", ProgramName);
	return (-1);
}


/*
 * assign_date() assigns a date, wrapping to next year if needed
 */
static void
assign_date(struct tm *tm, int mday, int mon, int year)
{

	/*
	 * Convert year into tm_year format (year - 1900).
	 * We may be given the year in 2 digit, 4 digit, or tm_year format.
	 */
	if (year != -1) {
		if (year >= TM_YEAR_BASE)
			year -= TM_YEAR_BASE;	/* convert from 4 digit year */
		else if (year < 100) {
			/* Convert to tm_year assuming current century */
			year += (tm->tm_year / 100) * 100;

			if (year == tm->tm_year - 1)
				year++;		/* Common off by one error */
			else if (year < tm->tm_year)
				year += 100;	/* must be in next century */
		}
	}

	if (year < 0 &&
	    (tm->tm_mon > mon ||(tm->tm_mon == mon && tm->tm_mday > mday)))
		year = tm->tm_year + 1;

	tm->tm_mday = mday;
	tm->tm_mon = mon;

	if (year >= 0)
		tm->tm_year = year;
}


/*
 * month() picks apart a month specification
 *
 *  /[<month> NUMBER [NUMBER]]           \
 *  |[TOMORROW]                          |
 *  |[DAY OF WEEK]                       |
 *  |NUMBER [SLASH NUMBER [SLASH NUMBER]]|
 *  \PLUS NUMBER MINUTES|HOURS|DAYS|WEEKS|MONTHS|YEARS/
 */
static int
month(struct tm *tm)
{
	int year = (-1);
	int mday, wday, mon;
	size_t tlen;

	switch (sc_tokid) {
	case NEXT:
	case PLUS:
		if (plus(tm) != 0)
			return (-1);
		break;

	case TOMORROW:
		/* do something tomorrow */
		tm->tm_mday++;
		tm->tm_wday++;
	case TODAY:
		/* force ourselves to stay in today - no further processing */
		token();
		break;

	case JAN: case FEB: case MAR: case APR: case MAY: case JUN:
	case JUL: case AUG: case SEP: case OCT: case NOV: case DEC:
		/*
		 * do month mday [year]
		 */
		mon = sc_tokid - JAN;
		if (expect(NUMBER) != 0)
			return (-1);
		mday = atoi(sc_token);
		if (token() == NUMBER) {
			year = atoi(sc_token);
			token();
		}
		assign_date(tm, mday, mon, year);
		break;

	case SUN: case MON: case TUE:
	case WED: case THU: case FRI:
	case SAT:
		/* do a particular day of the week */
		wday = sc_tokid - SUN;

		mday = tm->tm_mday;

		/* if this day is < today, then roll to next week */
		if (wday < tm->tm_wday)
			mday += 7 - (tm->tm_wday - wday);
		else
			mday += (wday - tm->tm_wday);

		tm->tm_wday = wday;

		assign_date(tm, mday, tm->tm_mon, tm->tm_year);
		break;

	case NUMBER:
		/*
		 * get numeric MMDDYY, mm/dd/yy, or dd.mm.yy
		 */
		tlen = strlen(sc_token);
		mon = atoi(sc_token);
		token();

		if (sc_tokid == SLASH || sc_tokid == DOT) {
			int sep;

			sep = sc_tokid;
			if (expect(NUMBER) != 0)
				return (-1);
			mday = atoi(sc_token);
			if (token() == sep) {
				if (expect(NUMBER) != 0)
					return (-1);
				year = atoi(sc_token);
				token();
			}

			/*
			 * flip months and days for european timing
			 */
			if (sep == DOT) {
				int x = mday;
				mday = mon;
				mon = x;
			}
		} else if (tlen == 6 || tlen == 8) {
			if (tlen == 8) {
				year = (mon % 10000) - TM_YEAR_BASE;
				mon /= 10000;
			} else {
				year = mon % 100;
				mon /= 100;
			}
			mday = mon % 100;
			mon /= 100;
		} else
			goto bad;

		mon--;
		if (mon < 0 || mon > 11 || mday < 1 || mday > 31)
			goto bad;

		assign_date(tm, mday, mon, year);
		break;
	}
	return (0);
bad:
	fprintf(stderr, "%s: garbled time\n", ProgramName);
	return (-1);
}


time_t
parsetime(int argc, char **argv)
{
	/*
	 * Do the argument parsing, die if necessary, and return the
	 * time the job should be run.
	 */
	time_t nowtimer, runtimer;
	struct tm nowtime, runtime;
	int hr = 0;
	/* this MUST be initialized to zero for midnight/noon/teatime */

	if (argc == 0)
		return (-1);

	nowtimer = time(NULL);
	nowtime = *localtime(&nowtimer);

	runtime = nowtime;
	runtime.tm_sec = 0;
	runtime.tm_isdst = 0;

	if (init_scanner(argc, argv) == -1)
		return (-1);

	switch (token()) {
	case NOW:	/* now is optional prefix for PLUS tree */
		token();
		if (sc_tokid == EOF) {
			runtime = nowtime;
			break;
		}
		else if (sc_tokid != PLUS && sc_tokid != NEXT)
			plonk(sc_tokid);
	case NEXT:
	case PLUS:
		if (plus(&runtime) != 0)
			return (-1);
		break;

	case NUMBER:
		if (tod(&runtime) != 0 || month(&runtime) != 0)
			return (-1);
		break;

		/*
		 * evil coding for TEATIME|NOON|MIDNIGHT - we've initialised
		 * hr to zero up above, then fall into this case in such a
		 * way so we add +12 +4 hours to it for teatime, +12 hours
		 * to it for noon, and nothing at all for midnight, then
		 * set our runtime to that hour before leaping into the
		 * month scanner
		 */
	case TEATIME:
		hr += 4;
		/* FALLTHROUGH */
	case NOON:
		hr += 12;
		/* FALLTHROUGH */
	case MIDNIGHT:
		if (runtime.tm_hour >= hr) {
			runtime.tm_mday++;
			runtime.tm_wday++;
		}
		runtime.tm_hour = hr;
		runtime.tm_min = 0;
		token();
		/* fall through to month setting */
		/* FALLTHROUGH */
	default:
		if (month(&runtime) != 0)
			return (-1);
		break;
	} /* ugly case statement */
	if (expect(EOF) != 0)
		return (-1);

	/*
	 * adjust for daylight savings time
	 */
	runtime.tm_isdst = -1;
	runtimer = mktime(&runtime);
	if (runtime.tm_isdst > 0) {
		runtimer -= 3600;
		runtimer = mktime(&runtime);
	}

	if (runtimer < 0) {
		fprintf(stderr, "%s: garbled time\n", ProgramName);
		return (-1);
	}

	if (nowtimer > runtimer) {
		fprintf(stderr, "%s: cannot schedule jobs in the past\n",
		    ProgramName);
		return (-1);
	}

	return (runtimer);
}
