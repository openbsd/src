/* 
 * tclWinTime.c --
 *
 *	Contains Windows specific versions of Tcl functions that
 *	obtain time values from the operating system.
 *
 * Copyright 1995 by Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclWinTime.c 1.4 96/04/02 18:49:06
 */

#include "tclInt.h"
#include "tclPort.h"

/*
 *----------------------------------------------------------------------
 *
 * TclGetSeconds --
 *
 *	This procedure returns the number of seconds from the epoch.
 *	On most Unix systems the epoch is Midnight Jan 1, 1970 GMT.
 *
 * Results:
 *	Number of seconds from the epoch.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

unsigned long
TclGetSeconds()
{
    return (unsigned long) time((time_t *) NULL);
}

/*
 *----------------------------------------------------------------------
 *
 * TclGetClicks --
 *
 *	This procedure returns a value that represents the highest
 *	resolution clock available on the system.  There are no
 *	guarantees on what the resolution will be.  In Tcl we will
 *	call this value a "click".  The start time is also system
 *	dependant.
 *
 * Results:
 *	Number of clicks from some start time.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

unsigned long
TclGetClicks()
{
    return GetTickCount();
}

/*
 *----------------------------------------------------------------------
 *
 * TclGetTimeZone --
 *
 *	Determines the current timezone.  The method varies wildly
 *	between different Platform implementations, so its hidden in
 *	this function.
 *
 * Results:
 *	Hours east of GMT.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclGetTimeZone (currentTime)
    unsigned long  currentTime;
{
    static int setTZ = 0;
    int timeZone;

    if (!setTZ) {
        tzset();
        setTZ = 1;
    }
    timeZone = _timezone / 60;

    return timeZone;
}

/*
 *----------------------------------------------------------------------
 *
 * TclGetTime --
 *
 *	Gets the current system time in seconds and microseconds
 *	since the beginning of the epoch: 00:00 UCT, January 1, 1970.
 *
 * Results:
 *	Returns the current time in timePtr.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TclGetTime(timePtr)
    Tcl_Time *timePtr;		/* Location to store time information. */
{
    struct timeb t;

    ftime(&t);
    timePtr->sec = t.time;
    timePtr->usec = t.millitm * 1000;
}
