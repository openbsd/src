/*
** This file is in the public domain, so clarified as of
** 1996-06-05 by Arthur David Olson (arthur_david_olson@nih.gov).
*/

#if defined(LIBC_SCCS) && !defined(lint) && !defined(NOID)
static char elsieid[] = "@(#)asctime.c	7.9";
static char rcsid[] = "$OpenBSD: asctime.c,v 1.6 1999/01/29 07:09:26 d Exp $";
#endif /* LIBC_SCCS and not lint */

/*LINTLIBRARY*/

#include "private.h"
#include "tzfile.h"
#include "thread_private.h"

/*
** A la ISO/IEC 9945-1, ANSI/IEEE Std 1003.1, Second Edition, 1996-07-12.
*/

char *
asctime_r(timeptr, buf)
register const struct tm *	timeptr;
char *				buf;
{
	static const char	wday_name[][3] = {
		"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
	};
	static const char	mon_name[][3] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};
	register const char *	wn;
	register const char *	mn;
	int size;

	if (timeptr->tm_wday < 0 || timeptr->tm_wday >= DAYSPERWEEK)
		wn = "???";
	else	wn = wday_name[timeptr->tm_wday];
	if (timeptr->tm_mon < 0 || timeptr->tm_mon >= MONSPERYEAR)
		mn = "???";
	else	mn = mon_name[timeptr->tm_mon];
	/*
	** The X3J11-suggested format is
	**	"%.3s %.3s%3d %02.2d:%02.2d:%02.2d %d\n"
	** Since the .2 in 02.2d is ignored, we drop it.
	*/
	/*
	 * P1003 8.3.5.2 says that asctime_r() can only assume at most
	 * a 26 byte buffer.  *XXX*
	 */
	size = snprintf(buf, 26, "%.3s %.3s%3d %02d:%02d:%02d %d\n",
		wn, mn,
		timeptr->tm_mday, timeptr->tm_hour,
		timeptr->tm_min, timeptr->tm_sec,
		TM_YEAR_BASE + timeptr->tm_year);
	if (size >= 26)
		return NULL;
	return buf;
}

/*
** A la X3J11, with core dump avoidance.
*/

char *
asctime(timeptr)
const struct tm *	timeptr;
{
	/* asctime_r won't exceed this buffer: */
	static char result[26];
	_THREAD_PRIVATE_KEY(asctime)
	char *resultp = (char*) _THREAD_PRIVATE(asctime, result, NULL);

	if (resultp == NULL)
		return NULL;
	else
		return asctime_r(timeptr, resultp);
}
