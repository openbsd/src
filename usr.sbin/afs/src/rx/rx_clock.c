/*	$OpenBSD: rx_clock.c,v 1.1.1.1 1998/09/14 21:53:14 art Exp $	*/
/*
****************************************************************************
*        Copyright IBM Corporation 1988, 1989 - All Rights Reserved        *
*                                                                          *
* Permission to use, copy, modify, and distribute this software and its    *
* documentation for any purpose and without fee is hereby granted,         *
* provided that the above copyright notice appear in all copies and        *
* that both that copyright notice and this permission notice appear in     *
* supporting documentation, and that the name of IBM not be used in        *
* advertising or publicity pertaining to distribution of the software      *
* without specific, written prior permission.                              *
*                                                                          *
* IBM DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL *
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL IBM *
* BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY      *
* DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER  *
* IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING   *
* OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.    *
****************************************************************************
*/

/* Elapsed time package */
/* See rx_clock.h for calling conventions */

#include "rx_locl.h"

RCSID("$KTH: rx_clock.c,v 1.4 1998/03/13 01:10:57 art Exp $");

#ifndef KERNEL

#if defined(AGS_GFS_ENV)
#define STARTVALUE 8000000	       /* Ultrix bounds smaller, too small
				        * for general use */
#else
#ifdef	AFS_SUN5_ENV
#define	STARTVALUE 10000000	       /* Max number of seconds setitimer
				        * allows, for some reason */
#else
#define	STARTVALUE 100000000	       /* Max number of seconds setitimer
				        * allows, for some reason */
#endif
#endif

struct clock clock_now;		       /* The last elapsed time ready by
				        * clock_GetTimer */

/*
 * This is set to 1 whenever the time is read, and reset to 0 whenever
 * clock_NewTime is called.  This is to allow the caller to control the
 * frequency with which the actual time is re-evaluated (an expensive
 * operation)
 */
int clock_haveCurrentTime;

int clock_nUpdates;		       /* The actual number of clock updates */

/* Initialize the clock */
void
clock_Init(void)
{
    static int initialized = 0;
    struct itimerval itimer, otimer;

    if (!initialized) {
	itimer.it_value.tv_sec = STARTVALUE;
	itimer.it_value.tv_usec = 0;
	itimer.it_interval.tv_sec = 0;
	itimer.it_interval.tv_usec = 0;

	if (setitimer(ITIMER_REAL, &itimer, &otimer) != 0) {
	    fprintf(stderr, "clock:  could not set interval timer; aborted\n");
	    fflush(stderr);
	    exit(1);
	}
	initialized = 1;
    }
    clock_UpdateTime();
}

/*
 * Compute the current time.  The timer gets the current total elapsed
 * time since startup, expressed in seconds and microseconds.  This call
 * is almost 200 usec on an APC RT
 */
void 
clock_UpdateTime(void)
{
    struct itimerval itimer;

    getitimer(ITIMER_REAL, &itimer);

    clock_now.sec = STARTVALUE - 1 - itimer.it_value.tv_sec;
    /* The "-1" makes up for adding 1000000 usec, on the next line */

    clock_now.usec = 1000000 - itimer.it_value.tv_usec;
    if (clock_now.usec == 1000000)
	clock_now.usec = 0, clock_now.sec++;
    clock_haveCurrentTime = 1;
    clock_nUpdates++;
}

#else				       /* KERNEL */
#endif				       /* KERNEL */
