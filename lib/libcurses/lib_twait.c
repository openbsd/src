
/***************************************************************************
*                            COPYRIGHT NOTICE                              *
****************************************************************************
*                ncurses is copyright (C) 1992-1995                        *
*                          Zeyd M. Ben-Halim                               *
*                          zmbenhal@netcom.com                             *
*                          Eric S. Raymond                                 *
*                          esr@snark.thyrsus.com                           *
*                                                                          *
*        Permission is hereby granted to reproduce and distribute ncurses  *
*        by any means and for any fee, whether alone or as part of a       *
*        larger distribution, in source or in binary form, PROVIDED        *
*        this notice is included with any such distribution, and is not    *
*        removed from any of its header files. Mention of ncurses in any   *
*        applications linked with it is highly appreciated.                *
*                                                                          *
*        ncurses comes AS IS with no warranty, implied or expressed.       *
*                                                                          *
***************************************************************************/

/*
**	lib_twait.c
**
**	The routine _nc_timed_wait().
**
*/

#include "curses.priv.h"

#include <sys/types.h>		/* some systems can't live without this */
#include <string.h>

#if HAVE_SYS_TIME_H && ! SYSTEM_LOOKS_LIKE_SCO
#include <sys/time.h>
#endif

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

/*
 * We want to define GOOD_SELECT if the last argument of select(2) is 
 * modified to indicate time left.  The code will deal gracefully with
 * the other case, this is just an optimization to reduce the number
 * of system calls per input event.
 *
 * In general, expect System-V-like UNIXes to have this behavior and BSD-like 
 * ones to not have it.  Check your manual page.  If it doesn't explicitly
 * say the last argument is modified, assume it's not.
 * 
 * (We'd really like configure to autodetect this, but writing a proper test
 * turns out to be hard.)
 */
#if defined(linux)
#define GOOD_SELECT
#endif

#if !HAVE_USLEEP
int usleep(unsigned int usec)
{
struct timeval tval;

	tval.tv_sec = usec / 1000000;
	tval.tv_usec = usec % 1000000;
	select(0, NULL, NULL, NULL, &tval);

}
#endif

int _nc_timed_wait(int fd, int wait, int *timeleft)
{
int result;
struct timeval ntimeout;
static fd_set set;
#if !defined(GOOD_SELECT) && HAVE_GETTIMEOFDAY
struct timeval starttime, returntime;

	 gettimeofday(&starttime, NULL);
#endif

	 FD_ZERO(&set);
	 FD_SET(fd, &set);

	 /* the units of wait are milliseconds */
	 ntimeout.tv_sec = wait / 1000;
	 ntimeout.tv_usec = (wait % 1000) * 1000;

	 T(("start twait: sec = %ld, usec = %ld", ntimeout.tv_sec, ntimeout.tv_usec));

	 result = select(fd+1, &set, NULL, NULL, &ntimeout);

#if !defined(GOOD_SELECT) && HAVE_GETTIMEOFDAY
	 gettimeofday(&returntime, NULL);
	 ntimeout.tv_sec -= (returntime.tv_sec - starttime.tv_sec);
	 ntimeout.tv_usec -= (returntime.tv_usec - starttime.tv_usec);
	 if (ntimeout.tv_usec < 0 && ntimeout.tv_sec > 0) {
		ntimeout.tv_sec--;
		ntimeout.tv_usec += 1000000;
	 }
	 if (ntimeout.tv_sec < 0)
		ntimeout.tv_sec = ntimeout.tv_usec = 0;
#endif

	 /* return approximate time left on the ntimeout, in milliseconds */
	 if (timeleft)
		*timeleft = (ntimeout.tv_sec * 1000) + (ntimeout.tv_usec / 1000);

	 T(("end twait: returned %d, sec = %ld, usec = %ld (%d msec)",
		 result, ntimeout.tv_sec, ntimeout.tv_usec, 
	 	timeleft ? *timeleft : -1));

	 return(result);
}
