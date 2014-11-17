#ifdef __cplusplus
extern "C" {
#endif
#define PERL_NO_GET_CONTEXT
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include <time.h>
#ifdef __cplusplus
}
#endif

/* XXX struct tm on some systems (SunOS4/BSD) contains extra (non POSIX)
 * fields for which we don't have Configure support prior to Perl 5.8.0:
 *   char *tm_zone;   -- abbreviation of timezone name
 *   long tm_gmtoff;  -- offset from GMT in seconds
 * To workaround core dumps from the uninitialised tm_zone we get the
 * system to give us a reasonable struct to copy.  This fix means that
 * strftime uses the tm_zone and tm_gmtoff values returned by
 * localtime(time()). That should give the desired result most of the
 * time. But probably not always!
 *
 * This is a vestigial workaround for Perls prior to 5.8.0.  We now
 * rely on the initialization (still likely a workaround) in util.c.
 */
#if !defined(PERL_VERSION) || PERL_VERSION < 8

#if defined(HAS_GNULIBC)
# ifndef STRUCT_TM_HASZONE
#    define STRUCT_TM_HASZONE
# else
#    define USE_TM_GMTOFF
# endif
#endif

#endif /* end of pre-5.8 */

#define    DAYS_PER_YEAR    365
#define    DAYS_PER_QYEAR    (4*DAYS_PER_YEAR+1)
#define    DAYS_PER_CENT    (25*DAYS_PER_QYEAR-1)
#define    DAYS_PER_QCENT    (4*DAYS_PER_CENT+1)
#define    SECS_PER_HOUR    (60*60)
#define    SECS_PER_DAY    (24*SECS_PER_HOUR)
/* parentheses deliberately absent on these two, otherwise they don't work */
#define    MONTH_TO_DAYS    153/5
#define    DAYS_TO_MONTH    5/153
/* offset to bias by March (month 4) 1st between month/mday & year finding */
#define    YEAR_ADJUST    (4*MONTH_TO_DAYS+1)
/* as used here, the algorithm leaves Sunday as day 1 unless we adjust it */
#define    WEEKDAY_BIAS    6    /* (1+6)%7 makes Sunday 0 again */

#if !defined(PERL_VERSION) || PERL_VERSION < 8

#ifdef STRUCT_TM_HASZONE
static void
my_init_tm(struct tm *ptm)        /* see mktime, strftime and asctime    */
{
    Time_t now;
    (void)time(&now);
    Copy(localtime(&now), ptm, 1, struct tm);
}

#else
# define my_init_tm(ptm)
#endif

#else
/* use core version from util.c in 5.8.0 and later */
# define my_init_tm init_tm
#endif 

#ifdef WIN32

/*
 * (1) The CRT maintains its own copy of the environment, separate from
 * the Win32API copy.
 *
 * (2) CRT getenv() retrieves from this copy. CRT putenv() updates this
 * copy, and then calls SetEnvironmentVariableA() to update the Win32API
 * copy.
 *
 * (3) win32_getenv() and win32_putenv() call GetEnvironmentVariableA() and
 * SetEnvironmentVariableA() directly, bypassing the CRT copy of the
 * environment.
 *
 * (4) The CRT strftime() "%Z" implementation calls __tzset(). That
 * calls CRT tzset(), but only the first time it is called, and in turn
 * that uses CRT getenv("TZ") to retrieve the timezone info from the CRT
 * local copy of the environment and hence gets the original setting as
 * perl never updates the CRT copy when assigning to $ENV{TZ}.
 *
 * Therefore, we need to retrieve the value of $ENV{TZ} and call CRT
 * putenv() to update the CRT copy of the environment (if it is different)
 * whenever we're about to call tzset().
 *
 * In addition to all that, when perl is built with PERL_IMPLICIT_SYS
 * defined:
 *
 * (a) Each interpreter has its own copy of the environment inside the
 * perlhost structure. That allows applications that host multiple
 * independent Perl interpreters to isolate environment changes from
 * each other. (This is similar to how the perlhost mechanism keeps a
 * separate working directory for each Perl interpreter, so that calling
 * chdir() will not affect other interpreters.)
 *
 * (b) Only the first Perl interpreter instantiated within a process will
 * "write through" environment changes to the process environment.
 *
 * (c) Even the primary Perl interpreter won't update the CRT copy of the
 * the environment, only the Win32API copy (it calls win32_putenv()).
 *
 * As with CPerlHost::Getenv() and CPerlHost::Putenv() themselves, it makes
 * sense to only update the process environment when inside the main
 * interpreter, but we don't have access to CPerlHost's m_bTopLevel member
 * from here so we'll just have to check PL_curinterp instead.
 *
 * Therefore, we can simply #undef getenv() and putenv() so that those names
 * always refer to the CRT functions, and explicitly call win32_getenv() to
 * access perl's %ENV.
 *
 * We also #undef malloc() and free() to be sure we are using the CRT
 * functions otherwise under PERL_IMPLICIT_SYS they are redefined to calls
 * into VMem::Malloc() and VMem::Free() and all allocations will be freed
 * when the Perl interpreter is being destroyed so we'd end up with a pointer
 * into deallocated memory in environ[] if a program embedding a Perl
 * interpreter continues to operate even after the main Perl interpreter has
 * been destroyed.
 *
 * Note that we don't free() the malloc()ed memory unless and until we call
 * malloc() again ourselves because the CRT putenv() function simply puts its
 * pointer argument into the environ[] arrary (it doesn't make a copy of it)
 * so this memory must otherwise be leaked.
 */

#undef getenv
#undef putenv
#  ifdef UNDER_CE
#    define getenv xcegetenv
#    define putenv xceputenv
#  endif
#undef malloc
#undef free

static void
fix_win32_tzenv(void)
{
    static char* oldenv = NULL;
    char* newenv;
    const char* perl_tz_env = win32_getenv("TZ");
    const char* crt_tz_env = getenv("TZ");
    if (perl_tz_env == NULL)
        perl_tz_env = "";
    if (crt_tz_env == NULL)
        crt_tz_env = "";
    if (strcmp(perl_tz_env, crt_tz_env) != 0) {
        newenv = (char*)malloc((strlen(perl_tz_env) + 4) * sizeof(char));
        if (newenv != NULL) {
            sprintf(newenv, "TZ=%s", perl_tz_env);
            putenv(newenv);
            if (oldenv != NULL)
                free(oldenv);
            oldenv = newenv;
        }
    }
}

#endif

/*
 * my_tzset - wrapper to tzset() with a fix to make it work (better) on Win32.
 * This code is duplicated in the POSIX module, so any changes made here
 * should be made there too.
 */
static void
my_tzset(pTHX)
{
#ifdef WIN32
#if defined(USE_ITHREADS) && defined(PERL_IMPLICIT_SYS)
    if (PL_curinterp == aTHX)
#endif
        fix_win32_tzenv();
#endif
    tzset();
}

/*
 * my_mini_mktime - normalise struct tm values without the localtime()
 * semantics (and overhead) of mktime(). Stolen shamelessly from Perl's
 * Perl_mini_mktime() in util.c - for details on the algorithm, see that
 * file.
 */
static void
my_mini_mktime(struct tm *ptm)
{
    int yearday;
    int secs;
    int month, mday, year, jday;
    int odd_cent, odd_year;

    year = 1900 + ptm->tm_year;
    month = ptm->tm_mon;
    mday = ptm->tm_mday;
    /* allow given yday with no month & mday to dominate the result */
    if (ptm->tm_yday >= 0 && mday <= 0 && month <= 0) {
        month = 0;
        mday = 0;
        jday = 1 + ptm->tm_yday;
    }
    else {
        jday = 0;
    }
    if (month >= 2)
        month+=2;
    else
        month+=14, year--;

    yearday = DAYS_PER_YEAR * year + year/4 - year/100 + year/400;
    yearday += month*MONTH_TO_DAYS + mday + jday;
    /*
     * Note that we don't know when leap-seconds were or will be,
     * so we have to trust the user if we get something which looks
     * like a sensible leap-second.  Wild values for seconds will
     * be rationalised, however.
     */
    if ((unsigned) ptm->tm_sec <= 60) {
        secs = 0;
    }
    else {
        secs = ptm->tm_sec;
        ptm->tm_sec = 0;
    }
    secs += 60 * ptm->tm_min;
    secs += SECS_PER_HOUR * ptm->tm_hour;
    if (secs < 0) {
        if (secs-(secs/SECS_PER_DAY*SECS_PER_DAY) < 0) {
            /* got negative remainder, but need positive time */
            /* back off an extra day to compensate */
            yearday += (secs/SECS_PER_DAY)-1;
            secs -= SECS_PER_DAY * (secs/SECS_PER_DAY - 1);
        }
        else {
            yearday += (secs/SECS_PER_DAY);
            secs -= SECS_PER_DAY * (secs/SECS_PER_DAY);
        }
    }
    else if (secs >= SECS_PER_DAY) {
        yearday += (secs/SECS_PER_DAY);
        secs %= SECS_PER_DAY;
    }
    ptm->tm_hour = secs/SECS_PER_HOUR;
    secs %= SECS_PER_HOUR;
    ptm->tm_min = secs/60;
    secs %= 60;
    ptm->tm_sec += secs;
    /* done with time of day effects */
    /*
     * The algorithm for yearday has (so far) left it high by 428.
     * To avoid mistaking a legitimate Feb 29 as Mar 1, we need to
     * bias it by 123 while trying to figure out what year it
     * really represents.  Even with this tweak, the reverse
     * translation fails for years before A.D. 0001.
     * It would still fail for Feb 29, but we catch that one below.
     */
    jday = yearday;    /* save for later fixup vis-a-vis Jan 1 */
    yearday -= YEAR_ADJUST;
    year = (yearday / DAYS_PER_QCENT) * 400;
    yearday %= DAYS_PER_QCENT;
    odd_cent = yearday / DAYS_PER_CENT;
    year += odd_cent * 100;
    yearday %= DAYS_PER_CENT;
    year += (yearday / DAYS_PER_QYEAR) * 4;
    yearday %= DAYS_PER_QYEAR;
    odd_year = yearday / DAYS_PER_YEAR;
    year += odd_year;
    yearday %= DAYS_PER_YEAR;
    if (!yearday && (odd_cent==4 || odd_year==4)) { /* catch Feb 29 */
        month = 1;
        yearday = 29;
    }
    else {
        yearday += YEAR_ADJUST;    /* recover March 1st crock */
        month = yearday*DAYS_TO_MONTH;
        yearday -= month*MONTH_TO_DAYS;
        /* recover other leap-year adjustment */
        if (month > 13) {
            month-=14;
            year++;
        }
        else {
            month-=2;
        }
    }
    ptm->tm_year = year - 1900;
    if (yearday) {
      ptm->tm_mday = yearday;
      ptm->tm_mon = month;
    }
    else {
      ptm->tm_mday = 31;
      ptm->tm_mon = month - 1;
    }
    /* re-build yearday based on Jan 1 to get tm_yday */
    year--;
    yearday = year*DAYS_PER_YEAR + year/4 - year/100 + year/400;
    yearday += 14*MONTH_TO_DAYS + 1;
    ptm->tm_yday = jday - yearday;
    /* fix tm_wday if not overridden by caller */
    ptm->tm_wday = (jday + WEEKDAY_BIAS) % 7;
}

#   if defined(WIN32) || (defined(__QNX__) && defined(__WATCOMC__))
#       define strncasecmp(x,y,n) strnicmp(x,y,n)
#   endif

/* strptime copied from freebsd with the following copyright: */
/*
 * Copyright (c) 1994 Powerdog Industries.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgement:
 *      This product includes software developed by Powerdog Industries.
 * 4. The name of Powerdog Industries may not be used to endorse or
 *    promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY POWERDOG INDUSTRIES ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE POWERDOG INDUSTRIES BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
 
#ifndef lint
#ifndef NOID
static char copyright[] =
"@(#) Copyright (c) 1994 Powerdog Industries.  All rights reserved.";
static char sccsid[] = "@(#)strptime.c	0.1 (Powerdog) 94/03/27";
#endif /* !defined NOID */
#endif /* not lint */

#include <time.h>
#include <ctype.h>
#include <string.h>
static char * _strptime(pTHX_ const char *, const char *, struct tm *,
			int *got_GMT);

#define asizeof(a)	(sizeof (a) / sizeof ((a)[0]))

struct lc_time_T {
    const char *    mon[12];
    const char *    month[12];
    const char *    wday[7];
    const char *    weekday[7];
    const char *    X_fmt;     
    const char *    x_fmt;
    const char *    c_fmt;
    const char *    am;
    const char *    pm;
    const char *    date_fmt;
    const char *    alt_month[12];
    const char *    Ef_fmt;
    const char *    EF_fmt;
};

struct lc_time_T _time_localebuf;
int _time_using_locale;

const struct lc_time_T	_C_time_locale = {
	{
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	}, {
		"January", "February", "March", "April", "May", "June",
		"July", "August", "September", "October", "November", "December"
	}, {
		"Sun", "Mon", "Tue", "Wed",
		"Thu", "Fri", "Sat"
	}, {
		"Sunday", "Monday", "Tuesday", "Wednesday",
		"Thursday", "Friday", "Saturday"
	},

	/* X_fmt */
	"%H:%M:%S",

	/*
	** x_fmt
	** Since the C language standard calls for
	** "date, using locale's date format," anything goes.
	** Using just numbers (as here) makes Quakers happier;
	** it's also compatible with SVR4.
	*/
	"%m/%d/%y",

	/*
	** c_fmt (ctime-compatible)
	** Not used, just compatibility placeholder.
	*/
	NULL,

	/* am */
	"AM",

	/* pm */
	"PM",

	/* date_fmt */
	"%a %Ef %X %Z %Y",
	
	{
		"January", "February", "March", "April", "May", "June",
		"July", "August", "September", "October", "November", "December"
	},

	/* Ef_fmt
	** To determine short months / day order
	*/
	"%b %e",

	/* EF_fmt
	** To determine long months / day order
	*/
	"%B %e"
};

#define Locale (&_C_time_locale)

static char *
_strptime(pTHX_ const char *buf, const char *fmt, struct tm *tm, int *got_GMT)
{
	char c;
	const char *ptr;
	int i,
		len;
	int Ealternative, Oalternative;

    /* There seems to be a slightly improved version at
     * http://www.opensource.apple.com/source/Libc/Libc-583/stdtime/strptime-fbsd.c
     * which we may end up borrowing more from
     */
	ptr = fmt;
	while (*ptr != 0) {
		if (*buf == 0)
			break;

		c = *ptr++;
		
		if (c != '%') {
			if (isspace((unsigned char)c))
				while (*buf != 0 && isspace((unsigned char)*buf))
					buf++;
			else if (c != *buf++)
				return 0;
			continue;
		}

		Ealternative = 0;
		Oalternative = 0;
label:
		c = *ptr++;
		switch (c) {
		case 0:
		case '%':
			if (*buf++ != '%')
				return 0;
			break;

		case '+':
			buf = _strptime(aTHX_ buf, Locale->date_fmt, tm, got_GMT);
			if (buf == 0)
				return 0;
			break;

		case 'C':
			if (!isdigit((unsigned char)*buf))
				return 0;

			/* XXX This will break for 3-digit centuries. */
                        len = 2;
			for (i = 0; len && *buf != 0 && isdigit((unsigned char)*buf); buf++) {
				i *= 10;
				i += *buf - '0';
				len--;
			}
			if (i < 19)
				return 0;

			tm->tm_year = i * 100 - 1900;
			break;

		case 'c':
			/* NOTE: c_fmt is intentionally ignored */
                        buf = _strptime(aTHX_ buf, "%a %Ef %T %Y", tm, got_GMT);
			if (buf == 0)
				return 0;
			break;

		case 'D':
			buf = _strptime(aTHX_ buf, "%m/%d/%y", tm, got_GMT);
			if (buf == 0)
				return 0;
			break;

		case 'E':
			if (Ealternative || Oalternative)
				break;
			Ealternative++;
			goto label;

		case 'O':
			if (Ealternative || Oalternative)
				break;
			Oalternative++;
			goto label;

		case 'F':
		case 'f':
			if (!Ealternative)
				break;
			buf = _strptime(aTHX_ buf, (c == 'f') ? Locale->Ef_fmt : Locale->EF_fmt, tm, got_GMT);
			if (buf == 0)
				return 0;
			break;

		case 'R':
			buf = _strptime(aTHX_ buf, "%H:%M", tm, got_GMT);
			if (buf == 0)
				return 0;
			break;

		case 'r':
			buf = _strptime(aTHX_ buf, "%I:%M:%S %p", tm, got_GMT);
			if (buf == 0)
				return 0;
			break;

		case 'n': /* whitespace */
		case 't':
			if (!isspace((unsigned char)*buf))
				return 0;
			while (isspace((unsigned char)*buf))
				buf++;
			break;
		
		case 'T':
			buf = _strptime(aTHX_ buf, "%H:%M:%S", tm, got_GMT);
			if (buf == 0)
				return 0;
			break;

		case 'X':
			buf = _strptime(aTHX_ buf, Locale->X_fmt, tm, got_GMT);
			if (buf == 0)
				return 0;
			break;

		case 'x':
			buf = _strptime(aTHX_ buf, Locale->x_fmt, tm, got_GMT);
			if (buf == 0)
				return 0;
			break;

		case 'j':
			if (!isdigit((unsigned char)*buf))
				return 0;

			len = 3;
			for (i = 0; len && *buf != 0 && isdigit((unsigned char)*buf); buf++) {
				i *= 10;
				i += *buf - '0';
				len--;
			}
			if (i < 1 || i > 366)
				return 0;

			tm->tm_yday = i - 1;
			tm->tm_mday = 0;
			break;

		case 'M':
		case 'S':
			if (*buf == 0 || isspace((unsigned char)*buf))
				break;

			if (!isdigit((unsigned char)*buf))
				return 0;

			len = 2;
			for (i = 0; len && *buf != 0 && isdigit((unsigned char)*buf); buf++) {
				i *= 10;
				i += *buf - '0';
				len--;
			}

			if (c == 'M') {
				if (i > 59)
					return 0;
				tm->tm_min = i;
			} else {
				if (i > 60)
					return 0;
				tm->tm_sec = i;
			}

			if (*buf != 0 && isspace((unsigned char)*buf))
				while (*ptr != 0 && !isspace((unsigned char)*ptr))
					ptr++;
			break;

		case 'H':
		case 'I':
		case 'k':
		case 'l':
			/*
			 * Of these, %l is the only specifier explicitly
			 * documented as not being zero-padded.  However,
			 * there is no harm in allowing zero-padding.
			 *
			 * XXX The %l specifier may gobble one too many
			 * digits if used incorrectly.
			 */
            if (!isdigit((unsigned char)*buf))
				return 0;

			len = 2;
			for (i = 0; len && *buf != 0 && isdigit((unsigned char)*buf); buf++) {
				i *= 10;
				i += *buf - '0';
				len--;
			}
			if (c == 'H' || c == 'k') {
				if (i > 23)
					return 0;
			} else if (i > 12)
				return 0;

			tm->tm_hour = i;

			if (*buf != 0 && isspace((unsigned char)*buf))
				while (*ptr != 0 && !isspace((unsigned char)*ptr))
					ptr++;
			break;

		case 'p':
			/*
			 * XXX This is bogus if parsed before hour-related
			 * specifiers.
			 */
            len = strlen(Locale->am);
			if (strncasecmp(buf, Locale->am, len) == 0) {
				if (tm->tm_hour > 12)
					return 0;
				if (tm->tm_hour == 12)
					tm->tm_hour = 0;
				buf += len;
				break;
			}

			len = strlen(Locale->pm);
			if (strncasecmp(buf, Locale->pm, len) == 0) {
				if (tm->tm_hour > 12)
					return 0;
				if (tm->tm_hour != 12)
					tm->tm_hour += 12;
				buf += len;
				break;
			}

			return 0;

		case 'A':
		case 'a':
			for (i = 0; i < asizeof(Locale->weekday); i++) {
				if (c == 'A') {
					len = strlen(Locale->weekday[i]);
					if (strncasecmp(buf,
							Locale->weekday[i],
							len) == 0)
						break;
				} else {
					len = strlen(Locale->wday[i]);
					if (strncasecmp(buf,
							Locale->wday[i],
							len) == 0)
						break;
				}
			}
			if (i == asizeof(Locale->weekday))
				return 0;

			tm->tm_wday = i;
			buf += len;
			break;

		case 'U':
		case 'W':
			/*
			 * XXX This is bogus, as we can not assume any valid
			 * information present in the tm structure at this
			 * point to calculate a real value, so just check the
			 * range for now.
			 */
            if (!isdigit((unsigned char)*buf))
				return 0;

			len = 2;
			for (i = 0; len && *buf != 0 && isdigit((unsigned char)*buf); buf++) {
				i *= 10;
				i += *buf - '0';
				len--;
			}
			if (i > 53)
				return 0;

			if (*buf != 0 && isspace((unsigned char)*buf))
				while (*ptr != 0 && !isspace((unsigned char)*ptr))
					ptr++;
			break;

		case 'w':
			if (!isdigit((unsigned char)*buf))
				return 0;

			i = *buf - '0';
			if (i > 6)
				return 0;

			tm->tm_wday = i;

			if (*buf != 0 && isspace((unsigned char)*buf))
				while (*ptr != 0 && !isspace((unsigned char)*ptr))
					ptr++;
			break;

		case 'd':
		case 'e':
			/*
			 * The %e specifier is explicitly documented as not
			 * being zero-padded but there is no harm in allowing
			 * such padding.
			 *
			 * XXX The %e specifier may gobble one too many
			 * digits if used incorrectly.
			 */
                        if (!isdigit((unsigned char)*buf))
				return 0;

			len = 2;
			for (i = 0; len && *buf != 0 && isdigit((unsigned char)*buf); buf++) {
				i *= 10;
				i += *buf - '0';
				len--;
			}
			if (i > 31)
				return 0;

			tm->tm_mday = i;

			if (*buf != 0 && isspace((unsigned char)*buf))
				while (*ptr != 0 && !isspace((unsigned char)*ptr))
					ptr++;
			break;

		case 'B':
		case 'b':
		case 'h':
			for (i = 0; i < asizeof(Locale->month); i++) {
				if (Oalternative) {
					if (c == 'B') {
						len = strlen(Locale->alt_month[i]);
						if (strncasecmp(buf,
								Locale->alt_month[i],
								len) == 0)
							break;
					}
				} else {
					if (c == 'B') {
						len = strlen(Locale->month[i]);
						if (strncasecmp(buf,
								Locale->month[i],
								len) == 0)
							break;
					} else {
						len = strlen(Locale->mon[i]);
						if (strncasecmp(buf,
								Locale->mon[i],
								len) == 0)
							break;
					}
				}
			}
			if (i == asizeof(Locale->month))
				return 0;

			tm->tm_mon = i;
			buf += len;
			break;

		case 'm':
			if (!isdigit((unsigned char)*buf))
				return 0;

			len = 2;
			for (i = 0; len && *buf != 0 && isdigit((unsigned char)*buf); buf++) {
				i *= 10;
				i += *buf - '0';
				len--;
			}
			if (i < 1 || i > 12)
				return 0;

			tm->tm_mon = i - 1;

			if (*buf != 0 && isspace((unsigned char)*buf))
				while (*ptr != 0 && !isspace((unsigned char)*ptr))
					ptr++;
			break;

		case 's':
			{
			char *cp;
			int sverrno;
			long n;
			time_t t;
            struct tm mytm;

			sverrno = errno;
			errno = 0;
			n = strtol(buf, &cp, 10);
			if (errno == ERANGE || (long)(t = n) != n) {
				errno = sverrno;
				return 0;
			}
			errno = sverrno;
			buf = cp;
            memset(&mytm, 0, sizeof(mytm));
            my_init_tm(&mytm);    /* XXX workaround - see my_init_tm() above */
            mytm = *gmtime(&t);
            tm->tm_sec    = mytm.tm_sec;
            tm->tm_min    = mytm.tm_min;
            tm->tm_hour   = mytm.tm_hour;
            tm->tm_mday   = mytm.tm_mday;
            tm->tm_mon    = mytm.tm_mon;
            tm->tm_year   = mytm.tm_year;
            tm->tm_wday   = mytm.tm_wday;
            tm->tm_yday   = mytm.tm_yday;
            tm->tm_isdst  = mytm.tm_isdst;
			}
			break;

		case 'Y':
		case 'y':
			if (*buf == 0 || isspace((unsigned char)*buf))
				break;

			if (!isdigit((unsigned char)*buf))
				return 0;

			len = (c == 'Y') ? 4 : 2;
			for (i = 0; len && *buf != 0 && isdigit((unsigned char)*buf); buf++) {
				i *= 10;
				i += *buf - '0';
				len--;
			}
			if (c == 'Y')
				i -= 1900;
			if (c == 'y' && i < 69)
				i += 100;
			if (i < 0)
				return 0;

			tm->tm_year = i;

			if (*buf != 0 && isspace((unsigned char)*buf))
				while (*ptr != 0 && !isspace((unsigned char)*ptr))
					ptr++;
			break;

		case 'Z':
			{
			const char *cp;
			char *zonestr;

			for (cp = buf; *cp && isupper((unsigned char)*cp); ++cp) 
                            {/*empty*/}
			if (cp - buf) {
				zonestr = (char *)malloc(cp - buf + 1);
				if (!zonestr) {
				    errno = ENOMEM;
				    return 0;
				}
				strncpy(zonestr, buf, cp - buf);
				zonestr[cp - buf] = '\0';
				my_tzset(aTHX);
				if (0 == strcmp(zonestr, "GMT")) {
				    *got_GMT = 1;
				}
				free(zonestr);
				if (!*got_GMT) return 0;
				buf += cp - buf;
			}
			}
			break;

		case 'z':
			{
			int sign = 1;

			if (*buf != '+') {
				if (*buf == '-')
					sign = -1;
				else
					return 0;
			}

			buf++;
			i = 0;
			for (len = 4; len > 0; len--) {
				if (isdigit((int)*buf)) {
					i *= 10;
					i += *buf - '0';
					buf++;
				} else
					return 0;
			}

			tm->tm_hour -= sign * (i / 100);
			tm->tm_min  -= sign * (i % 100);
			*got_GMT = 1;
			}
			break;
		}
	}
	return (char *)buf;
}

/* Saves alot of machine code.
   Takes a (auto) SP, which may or may not have been PUSHed before, puts
   tm struct members on Perl stack, then returns new, advanced, SP to caller.
   Assign the return of push_common_tm to your SP, so you can continue to PUSH
   or do a PUTBACK and return eventually.
   !!!! push_common_tm does not touch PL_stack_sp !!!!
   !!!! do not use PUTBACK then SPAGAIN semantics around push_common_tm !!!!
   !!!! You must mortalize whatever push_common_tm put on stack yourself to
        avoid leaking !!!!
*/
SV **
push_common_tm(pTHX_ SV ** SP, struct tm *mytm)
{
	PUSHs(newSViv(mytm->tm_sec));
	PUSHs(newSViv(mytm->tm_min));
	PUSHs(newSViv(mytm->tm_hour));
	PUSHs(newSViv(mytm->tm_mday));
	PUSHs(newSViv(mytm->tm_mon));
	PUSHs(newSViv(mytm->tm_year));
	PUSHs(newSViv(mytm->tm_wday));
	PUSHs(newSViv(mytm->tm_yday));
	return SP;
}

/* specialized common end of 2 XSUBs
  SV ** SP -- pass your (auto) SP, which has not been PUSHed before, but was
              reset to 0 (PPCODE only or SP -= items or XSprePUSH)
  tm *mytm -- a tm *, will be proprocessed with my_mini_mktime
  return   -- none, after calling return_11part_tm, you must call "return;"
              no exceptions
*/
void
return_11part_tm(pTHX_ SV ** SP, struct tm *mytm)
{
       my_mini_mktime(mytm);

  /* warn("tm: %d-%d-%d %d:%d:%d\n", mytm.tm_year, mytm.tm_mon, mytm.tm_mday, mytm.tm_hour, mytm.tm_min, mytm.tm_sec); */

       EXTEND(SP, 11);
       SP = push_common_tm(aTHX_ SP, mytm);
       /* isdst */
       PUSHs(newSViv(0));
       /* epoch */
       PUSHs(newSViv(0));
       /* islocal */
       PUSHs(newSViv(0));
       PUTBACK;
       {
            SV ** endsp = SP; /* the SV * under SP needs to be mortaled */
            SP -= (11 - 1); /* subtract 0 based count of SVs to mortal */
/* mortal target of SP, then increment before function call
   so SP is already calculated before next comparison to not stall CPU */
            do {
                sv_2mortal(*SP++);
            } while(SP <= endsp);
       }
       return;
}

MODULE = Time::Piece     PACKAGE = Time::Piece

PROTOTYPES: ENABLE

void
_strftime(fmt, sec, min, hour, mday, mon, year, wday = -1, yday = -1, isdst = -1)
    char *        fmt
    int        sec
    int        min
    int        hour
    int        mday
    int        mon
    int        year
    int        wday
    int        yday
    int        isdst
    CODE:
    {
        char tmpbuf[128];
        struct tm mytm;
        int len;
        memset(&mytm, 0, sizeof(mytm));
        my_init_tm(&mytm);    /* XXX workaround - see my_init_tm() above */
        mytm.tm_sec = sec;
        mytm.tm_min = min;
        mytm.tm_hour = hour;
        mytm.tm_mday = mday;
        mytm.tm_mon = mon;
        mytm.tm_year = year;
        mytm.tm_wday = wday;
        mytm.tm_yday = yday;
        mytm.tm_isdst = isdst;
        my_mini_mktime(&mytm);
        len = strftime(tmpbuf, sizeof tmpbuf, fmt, &mytm);
        /*
        ** The following is needed to handle to the situation where 
        ** tmpbuf overflows.  Basically we want to allocate a buffer
        ** and try repeatedly.  The reason why it is so complicated
        ** is that getting a return value of 0 from strftime can indicate
        ** one of the following:
        ** 1. buffer overflowed,
        ** 2. illegal conversion specifier, or
        ** 3. the format string specifies nothing to be returned(not
        **      an error).  This could be because format is an empty string
        **    or it specifies %p that yields an empty string in some locale.
        ** If there is a better way to make it portable, go ahead by
        ** all means.
        */
        if ((len > 0 && len < sizeof(tmpbuf)) || (len == 0 && *fmt == '\0'))
        ST(0) = sv_2mortal(newSVpv(tmpbuf, len));
        else {
        /* Possibly buf overflowed - try again with a bigger buf */
        int     fmtlen = strlen(fmt);
        int    bufsize = fmtlen + sizeof(tmpbuf);
        char*     buf;
        int    buflen;

        New(0, buf, bufsize, char);
        while (buf) {
            buflen = strftime(buf, bufsize, fmt, &mytm);
            if (buflen > 0 && buflen < bufsize)
            break;
            /* heuristic to prevent out-of-memory errors */
            if (bufsize > 100*fmtlen) {
            Safefree(buf);
            buf = NULL;
            break;
            }
            bufsize *= 2;
            Renew(buf, bufsize, char);
        }
        if (buf) {
            ST(0) = sv_2mortal(newSVpv(buf, buflen));
            Safefree(buf);
        }
        else
            ST(0) = sv_2mortal(newSVpv(tmpbuf, len));
        }
    }

void
_tzset()
  PPCODE:
    PUTBACK; /* makes rest of this function tailcall friendly */
    my_tzset(aTHX);
    return; /* skip XSUBPP's PUTBACK */

void
_strptime ( string, format )
	char * string
	char * format
  PREINIT:
       struct tm mytm;
       time_t t;
       char * remainder;
       int got_GMT;
  PPCODE:
       t = 0;
       mytm = *gmtime(&t);
       got_GMT = 0;

       remainder = (char *)_strptime(aTHX_ string, format, &mytm, &got_GMT);
       if (remainder == NULL) {
           croak("Error parsing time");
       }
       if (*remainder != '\0') {
           warn("garbage at end of string in strptime: %s", remainder);
       }

       return_11part_tm(aTHX_ SP, &mytm);
       return;

void
_mini_mktime(int sec, int min, int hour, int mday, int mon, int year)
  PREINIT:
       struct tm mytm;
       time_t t;
  PPCODE:
       t = 0;
       mytm = *gmtime(&t);

       mytm.tm_sec = sec;
       mytm.tm_min = min;
       mytm.tm_hour = hour;
       mytm.tm_mday = mday;
       mytm.tm_mon = mon;
       mytm.tm_year = year;

       return_11part_tm(aTHX_ SP, &mytm);
       return;

void
_crt_localtime(time_t sec)
    ALIAS:
        _crt_gmtime = 1
    PREINIT:
        struct tm mytm;
    PPCODE:
        if(ix) mytm = *gmtime(&sec);
        else mytm = *localtime(&sec);
        /* Need to get: $s,$n,$h,$d,$m,$y */
        
        EXTEND(SP, 9);
        SP = push_common_tm(aTHX_ SP, &mytm);
        PUSHs(newSViv(mytm.tm_isdst));
        PUTBACK;
        {
            SV ** endsp = SP; /* the SV * under SP needs to be mortaled */
            SP -= (9 - 1); /* subtract 0 based count of SVs to mortal */
/* mortal target of SP, then increment before function call
   so SP is already calculated before next comparison to not stall CPU */
            do {
                sv_2mortal(*SP++);
            } while(SP <= endsp);
        }
        return;
