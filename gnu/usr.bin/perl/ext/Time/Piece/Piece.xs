#ifdef __cplusplus
extern "C" {
#endif
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include <time.h>
#ifdef __cplusplus
}
#endif

/* XXX struct tm on some systems (SunOS4/BSD) contains extra (non POSIX)
 * fields for which we don't have Configure support yet:
 *   char *tm_zone;   -- abbreviation of timezone name
 *   long tm_gmtoff;  -- offset from GMT in seconds
 * To workaround core dumps from the uninitialised tm_zone we get the
 * system to give us a reasonable struct to copy.  This fix means that
 * strftime uses the tm_zone and tm_gmtoff values returned by
 * localtime(time()). That should give the desired result most of the
 * time. But probably not always!
 *
 * This is a temporary workaround to be removed once Configure
 * support is added and NETaa14816 is considered in full.
 * It does not address tzname aspects of NETaa14816.
 */
#if !defined(HAS_GNULIBC)
# ifndef STRUCT_TM_HASZONE
#    define STRUCT_TM_HASZONE
# else
#    define USE_TM_GMTOFF
# endif
#endif

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

/*
 * my_mini_mktime - normalise struct tm values without the localtime()
 * semantics (and overhead) of mktime().
 */
static void
my_mini_mktime(struct tm *ptm)
{
    int yearday;
    int secs;
    int month, mday, year, jday;
    int odd_cent, odd_year;

/*
 * Year/day algorithm notes:
 *
 * With a suitable offset for numeric value of the month, one can find
 * an offset into the year by considering months to have 30.6 (153/5) days,
 * using integer arithmetic (i.e., with truncation).  To avoid too much
 * messing about with leap days, we consider January and February to be
 * the 13th and 14th month of the previous year.  After that transformation,
 * we need the month index we use to be high by 1 from 'normal human' usage,
 * so the month index values we use run from 4 through 15.
 *
 * Given that, and the rules for the Gregorian calendar (leap years are those
 * divisible by 4 unless also divisible by 100, when they must be divisible
 * by 400 instead), we can simply calculate the number of days since some
 * arbitrary 'beginning of time' by futzing with the (adjusted) year number,
 * the days we derive from our month index, and adding in the day of the
 * month.  The value used here is not adjusted for the actual origin which
 * it normally would use (1 January A.D. 1), since we're not exposing it.
 * We're only building the value so we can turn around and get the
 * normalised values for the year, month, day-of-month, and day-of-year.
 *
 * For going backward, we need to bias the value we're using so that we find
 * the right year value.  (Basically, we don't want the contribution of
 * March 1st to the number to apply while deriving the year).  Having done
 * that, we 'count up' the contribution to the year number by accounting for
 * full quadracenturies (400-year periods) with their extra leap days, plus
 * the contribution from full centuries (to avoid counting in the lost leap
 * days), plus the contribution from full quad-years (to count in the normal
 * leap days), plus the leftover contribution from any non-leap years.
 * At this point, if we were working with an actual leap day, we'll have 0
 * days left over.  This is also true for March 1st, however.  So, we have
 * to special-case that result, and (earlier) keep track of the 'odd'
 * century and year contributions.  If we got 4 extra centuries in a qcent,
 * or 4 extra years in a qyear, then it's a leap day and we call it 29 Feb.
 * Otherwise, we add back in the earlier bias we removed (the 123 from
 * figuring in March 1st), find the month index (integer division by 30.6),
 * and the remainder is the day-of-month.  We then have to convert back to
 * 'real' months (including fixing January and February from being 14/15 in
 * the previous year to being in the proper year).  After that, to get
 * tm_yday, we work with the normalised year and get a new yearday value for
 * January 1st, which we subtract from the yearday value we had earlier,
 * representing the date we've re-built.  This is done from January 1
 * because tm_yday is 0-origin.
 *
 * Since POSIX time routines are only guaranteed to work for times since the
 * UNIX epoch (00:00:00 1 Jan 1970 UTC), the fact that this algorithm
 * applies Gregorian calendar rules even to dates before the 16th century
 * doesn't bother me.  Besides, you'd need cultural context for a given
 * date to know whether it was Julian or Gregorian calendar, and that's
 * outside the scope for this routine.  Since we convert back based on the
 * same rules we used to build the yearday, you'll only get strange results
 * for input which needed normalising, or for the 'odd' century years which
 * were leap years in the Julian calander but not in the Gregorian one.
 * I can live with that.
 *
 * This algorithm also fails to handle years before A.D. 1 gracefully, but
 * that's still outside the scope for POSIX time manipulation, so I don't
 * care.
 */

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

#if defined(WIN32) || (defined(__QNX__) && defined(__WATCOMC__)) /* No strptime on Win32 or QNX4 */
#define strncasecmp(x,y,n) strnicmp(x,y,n)

#if defined(WIN32)
#define alloca _alloca
#endif

#include <time.h>
#include <ctype.h>
#include <string.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"
#endif /* _THREAD_SAFE */

static char * _strptime(const char *, const char *, struct tm *);

#ifdef _THREAD_SAFE
static struct pthread_mutex	_gotgmt_mutexd = PTHREAD_MUTEX_STATIC_INITIALIZER;
static pthread_mutex_t		gotgmt_mutex   = &_gotgmt_mutexd;
#endif
static int got_GMT;

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
_strptime(const char *buf, const char *fmt, struct tm *tm)
{
	char c;
	const char *ptr;
	int i,
		len;
	int Ealternative, Oalternative;

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
			buf = _strptime(buf, Locale->date_fmt, tm);
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
                        buf = _strptime(buf, "%a %Ef %T %Y", tm);
			if (buf == 0)
				return 0;
			break;

		case 'D':
			buf = _strptime(buf, "%m/%d/%y", tm);
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
			buf = _strptime(buf, (c == 'f') ? Locale->Ef_fmt : Locale->EF_fmt, tm);
			if (buf == 0)
				return 0;
			break;

		case 'R':
			buf = _strptime(buf, "%H:%M", tm);
			if (buf == 0)
				return 0;
			break;

		case 'r':
			buf = _strptime(buf, "%I:%M:%S %p", tm);
			if (buf == 0)
				return 0;
			break;

		case 'T':
			buf = _strptime(buf, "%H:%M:%S", tm);
			if (buf == 0)
				return 0;
			break;

		case 'X':
			buf = _strptime(buf, Locale->X_fmt, tm);
			if (buf == 0)
				return 0;
			break;

		case 'x':
			buf = _strptime(buf, Locale->x_fmt, tm);
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
				zonestr = (char *)alloca(cp - buf + 1);
				strncpy(zonestr, buf, cp - buf);
				zonestr[cp - buf] = '\0';
				tzset();
				if (0 == strcmp(zonestr, "GMT")) {
				    got_GMT = 1;
				} else {
				    return 0;
				}
				buf += cp - buf;
			}
			}
			break;
		}
	}
	return (char *)buf;
}


char *
strptime(const char *buf, const char *fmt, struct tm *tm)
{
	char *ret;

#ifdef _THREAD_SAFE
pthread_mutex_lock(&gotgmt_mutex);
#endif

        got_GMT = 0;
	ret = _strptime(buf, fmt, tm);

#ifdef _THREAD_SAFE
	pthread_mutex_unlock(&gotgmt_mutex);
#endif

	return ret;
}

#endif /* Mac OS X */

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
    tzset();


void
_strptime ( string, format )
	char * string
	char * format
  PREINIT:
       struct tm mytm;
       time_t t;
       char * remainder;
  PPCODE:
       t = 0;
       mytm = *gmtime(&t);
       
       remainder = (char *)strptime(string, format, &mytm);
       
       if (remainder == NULL) {
	  croak("Error parsing time");
       }

       if (*remainder != '\0') {
           warn("garbage at end of string in strptime: %s", remainder);
       }
	  
       my_mini_mktime(&mytm);

  /* warn("tm: %d-%d-%d %d:%d:%d\n", mytm.tm_year, mytm.tm_mon, mytm.tm_mday, mytm.tm_hour, mytm.tm_min, mytm.tm_sec); */
	  
       EXTEND(SP, 11);
       PUSHs(sv_2mortal(newSViv(mytm.tm_sec)));
       PUSHs(sv_2mortal(newSViv(mytm.tm_min)));
       PUSHs(sv_2mortal(newSViv(mytm.tm_hour)));
       PUSHs(sv_2mortal(newSViv(mytm.tm_mday)));
       PUSHs(sv_2mortal(newSViv(mytm.tm_mon)));
       PUSHs(sv_2mortal(newSViv(mytm.tm_year)));
       PUSHs(sv_2mortal(newSViv(mytm.tm_wday)));
       PUSHs(sv_2mortal(newSViv(mytm.tm_yday)));
       /* isdst */
       PUSHs(sv_2mortal(newSViv(0)));
       /* epoch */
       PUSHs(sv_2mortal(newSViv(0)));
       /* islocal */
       PUSHs(sv_2mortal(newSViv(0)));
