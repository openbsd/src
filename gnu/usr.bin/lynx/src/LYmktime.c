/* $LynxId: LYmktime.c,v 1.14 2013/05/03 20:14:06 tom Exp $ */

#include <LYStrings.h>
#include <LYUtils.h>

#include <parsdate.h>

#ifdef TEST_DRIVER
char *LYstrncpy(char *dst,
		const char *src,
		int n)
{
    char *val;
    int len;

    if (src == 0)
	src = "";
    len = strlen(src);

    if (n < 0)
	n = 0;

    val = StrNCpy(dst, src, n);
    if (len < n)
	*(dst + len) = '\0';
    else
	*(dst + n) = '\0';
    return val;
}
#define strcasecomp strcasecmp
BOOLEAN WWW_TraceFlag = FALSE;
FILE *TraceFP(void)
{
    return stderr;
}
#define USE_PARSDATE 0
#else
#define USE_PARSDATE 1
#endif

/*
 * This function takes a string in the format
 *	"Mon, 01-Jan-96 13:45:35 GMT" or
 *	"Mon,  1 Jan 1996 13:45:35 GMT" or
 *	"dd-mm-yyyy"
 * as an argument, and returns its conversion to clock format (seconds since
 * 00:00:00 Jan 1 1970), or 0 if the string doesn't match the expected pattern. 
 * It also returns 0 if the time is in the past and the "absolute" argument is
 * FALSE.  It is intended for handling 'expires' strings in Version 0 cookies
 * homologously to 'max-age' strings in Version 1 cookies, for which 0 is the
 * minimum, and greater values are handled as '[max-age seconds] + time(NULL)'. 
 * If "absolute" if TRUE, we return the clock format value itself, but if
 * anything goes wrong when parsing the expected patterns, we still return 0. 
 * - FM
 */
time_t LYmktime(char *string,
		int absolute)
{
#if USE_PARSDATE
    time_t result = 0;

    if (non_empty(string)) {
	CTRACE((tfp, "LYmktime: Parsing '%s'\n", string));
	result = parsedate(string, 0);

	if (!absolute) {
	    if ((long) (time((time_t *) 0) - result) >= 0)
		result = 0;
	}
	if (result != 0) {
	    CTRACE((tfp, "LYmktime: clock=%" PRI_time_t ", ctime=%s",
		    CAST_time_t (result),
		    ctime(&result)));
	}
    }
    return result;
#else
    char *s;
    time_t now, clock2;
    int day, month, year, hour, minutes, seconds;
    char *start;
    char temp[8];

    /*
     * Make sure we have a string to parse.  - FM
     */
    if (!non_empty(string))
	return (0);
    s = string;
    CTRACE((tfp, "LYmktime: Parsing '%s'\n", s));

    /*
     * Skip any lead alphabetic "Day, " field and seek a numeric day field.  -
     * FM
     */
    while (*s != '\0' && !isdigit(UCH(*s)))
	s++;
    if (*s == '\0')
	return (0);

    /*
     * Get the numeric day and convert to an integer.  - FM
     */
    start = s;
    while (*s != '\0' && isdigit(UCH(*s)))
	s++;
    if (*s == '\0' || (s - start) > 2)
	return (0);
    LYStrNCpy(temp, start, (s - start));
    day = atoi(temp);
    if (day < 1 || day > 31)
	return (0);

    /*
     * Get the month string and convert to an integer.  - FM
     */
    while (*s != '\0' && !isalnum(UCH(*s)))
	s++;
    if (*s == '\0')
	return (0);
    start = s;
    while (*s != '\0' && isalnum(UCH(*s)))
	s++;
    if ((*s == '\0') ||
	(s - start) < (isdigit(UCH(*(s - 1))) ? 2 : 3) ||
	(s - start) > (isdigit(UCH(*(s - 1))) ? 2 : 9))
	return (0);
    LYStrNCpy(temp, start, (isdigit(UCH(*(s - 1))) ? 2 : 3));
    switch (TOUPPER(temp[0])) {
    case '0':
    case '1':
	month = atoi(temp);
	if (month < 1 || month > 12) {
	    return (0);
	}
	break;
    case 'A':
	if (!strcasecomp(temp, "Apr")) {
	    month = 4;
	} else if (!strcasecomp(temp, "Aug")) {
	    month = 8;
	} else {
	    return (0);
	}
	break;
    case 'D':
	if (!strcasecomp(temp, "Dec")) {
	    month = 12;
	} else {
	    return (0);
	}
	break;
    case 'F':
	if (!strcasecomp(temp, "Feb")) {
	    month = 2;
	} else {
	    return (0);
	}
	break;
    case 'J':
	if (!strcasecomp(temp, "Jan")) {
	    month = 1;
	} else if (!strcasecomp(temp, "Jun")) {
	    month = 6;
	} else if (!strcasecomp(temp, "Jul")) {
	    month = 7;
	} else {
	    return (0);
	}
	break;
    case 'M':
	if (!strcasecomp(temp, "Mar")) {
	    month = 3;
	} else if (!strcasecomp(temp, "May")) {
	    month = 5;
	} else {
	    return (0);
	}
	break;
    case 'N':
	if (!strcasecomp(temp, "Nov")) {
	    month = 11;
	} else {
	    return (0);
	}
	break;
    case 'O':
	if (!strcasecomp(temp, "Oct")) {
	    month = 10;
	} else {
	    return (0);
	}
	break;
    case 'S':
	if (!strcasecomp(temp, "Sep")) {
	    month = 9;
	} else {
	    return (0);
	}
	break;
    default:
	return (0);
    }

    /*
     * Get the numeric year string and convert to an integer.  - FM
     */
    while (*s != '\0' && !isdigit(UCH(*s)))
	s++;
    if (*s == '\0')
	return (0);
    start = s;
    while (*s != '\0' && isdigit(UCH(*s)))
	s++;
    if ((s - start) == 4) {
	LYStrNCpy(temp, start, 4);
    } else if ((s - start) == 2) {
	now = time(NULL);
	/*
	 * Assume that received 2-digit dates >= 70 are 19xx; others
	 * are 20xx.  Only matters when dealing with broken software
	 * (HTTP server or web page) which is not Y2K compliant.  The
	 * line is drawn on a best-guess basis; it is impossible for
	 * this to be completely accurate because it depends on what
	 * the broken sender software intends.  (This totally breaks
	 * in 2100 -- setting up the next crisis...) - BL
	 */
	if (atoi(start) >= 70)
	    LYStrNCpy(temp, "19", 2);
	else
	    LYStrNCpy(temp, "20", 2);
	strncat(temp, start, 2);
	temp[4] = '\0';
    } else {
	return (0);
    }
    year = atoi(temp);

    /*
     * Get the numeric hour string and convert to an integer.  - FM
     */
    while (*s != '\0' && !isdigit(UCH(*s)))
	s++;
    if (*s == '\0') {
	hour = 0;
	minutes = 0;
	seconds = 0;
    } else {
	start = s;
	while (*s != '\0' && isdigit(UCH(*s)))
	    s++;
	if (*s != ':' || (s - start) > 2)
	    return (0);
	LYStrNCpy(temp, start, (s - start));
	hour = atoi(temp);

	/*
	 * Get the numeric minutes string and convert to an integer.  - FM
	 */
	while (*s != '\0' && !isdigit(UCH(*s)))
	    s++;
	if (*s == '\0')
	    return (0);
	start = s;
	while (*s != '\0' && isdigit(UCH(*s)))
	    s++;
	if (*s != ':' || (s - start) > 2)
	    return (0);
	LYStrNCpy(temp, start, (s - start));
	minutes = atoi(temp);

	/*
	 * Get the numeric seconds string and convert to an integer.  - FM
	 */
	while (*s != '\0' && !isdigit(UCH(*s)))
	    s++;
	if (*s == '\0')
	    return (0);
	start = s;
	while (*s != '\0' && isdigit(UCH(*s)))
	    s++;
	if (*s == '\0' || (s - start) > 2)
	    return (0);
	LYStrNCpy(temp, start, (s - start));
	seconds = atoi(temp);
    }

    /*
     * Convert to clock format (seconds since 00:00:00 Jan 1 1970), but then
     * zero it if it's in the past and "absolute" is not TRUE.  - FM
     */
    month -= 3;
    if (month < 0) {
	month += 12;
	year--;
    }
    day += (year - 1968) * 1461 / 4;
    day += ((((month * 153) + 2) / 5) - 672);
    clock2 = (time_t) ((day * 60 * 60 * 24) +
		       (hour * 60 * 60) +
		       (minutes * 60) +
		       seconds);
    if (absolute == FALSE && (long) (time((time_t *) 0) - clock2) >= 0)
	clock2 = (time_t) 0;
    if (clock2 > 0)
	CTRACE((tfp, "LYmktime: clock=%" PRI_time_t ", ctime=%s",
		CAST_time_t (clock2),
		ctime(&clock2)));

    return (clock2);
#endif
}

#ifdef TEST_DRIVER
static void test_mktime(char *source)
{
    time_t before = LYmktime(source, TRUE);
    time_t after = parsedate(source, 0);

    printf("TEST %s\n", source);
    printf("\t%" PRI_time_t "  %s", CAST_time_t (before), ctime(&before));
    printf("\t%" PRI_time_t "  %s", CAST_time_t (after), ctime(&after));

    if (before != after)
	printf("\t****\n");
}

int main(void)
{
    test_mktime("Mon, 01-Jan-96 13:45:35 GMT");
    test_mktime("Mon,  1 Jan 1996 13:45:35 GMT");
    test_mktime("31-12-1999");
    test_mktime("Wed May 14 22:00:00 2008");
    test_mktime("Sun, 29-Jun-2008 23:19:30 GMT");
    test_mktime("Sun Jul 06 07:00:00 2008 GMT");
    return 0;
}
#endif
