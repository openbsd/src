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

RCSID("$Id: rx_clock.c,v 1.3 2000/09/11 14:41:21 art Exp $");

#ifndef KERNEL

#ifdef HAVE_GETITIMER

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

/*
 * Dynamically figured out start-value of the ITIMER_REAL interval timer
 */

static struct clock startvalue;

/*
 * The offset we need to add.
 */

static struct clock offset;

/*
 * We need to restart the timer.
 */

static int do_restart;

/*
 * When the interval timer has counted to zero, set offset and make
 * sure it gets restarted.
 */

static void
alrm_handler (int signo)
{
    clock_Add(&offset, &startvalue);
    do_restart  = 1;
    clock_NewTime ();
}

/*
 * Install handler for SIGALRM
 */

static void
set_alrm_handler (void)
{
#if defined(HAVE_POSIX_SIGNALS) && defined(SA_RESTART)
    {
	struct sigaction act;

	sigemptyset(&act.sa_mask);
	act.sa_flags   = SA_RESTART;
	act.sa_handler = alrm_handler;
	if (sigaction (SIGALRM, &act, NULL) < 0) {
	    fprintf (stderr, "sigaction failed");
	    exit (1);
	}
    }
#else
    signal (SIGALRM, alrm_handler);
#endif
}

/*
 * Start an interval timer counting down to `timer_val'
 */

static int
clock_starttimer (time_t timer_val)
{
    struct itimerval itimer;

    itimer.it_value.tv_sec     = timer_val; 
    itimer.it_value.tv_usec    = 0;
    itimer.it_interval.tv_sec  = 0;
    itimer.it_interval.tv_usec = 0;

    if (setitimer (ITIMER_REAL, &itimer, NULL) != 0) {
	if (errno != EINVAL) {
	    fprintf (stderr, "setitimer failed\n");
	    exit (1);
	}
	return 1;
    }
    if (getitimer (ITIMER_REAL, &itimer) != 0) {
	fprintf (stderr, "setitimer failed\n");
	exit (1);
    }
    startvalue.sec  = itimer.it_value.tv_sec;
    startvalue.usec = itimer.it_value.tv_usec;
    do_restart  = 0;
    set_alrm_handler ();
    return 0;
}

/* Initialize the clock */
void
clock_Init(void)
{
    static int initialized = 0;
    int timer_val;

    for (timer_val = INT_MAX;
	 !initialized && clock_starttimer (timer_val);
	 timer_val /= 2)
	;
    initialized = 1;
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
    struct clock tmp_clock;

    if (do_restart)
	clock_starttimer (startvalue.sec);

    getitimer(ITIMER_REAL, &itimer);
    tmp_clock.sec  = itimer.it_value.tv_sec;
    tmp_clock.usec = itimer.it_value.tv_usec;

    clock_now = offset;
    clock_Add(&clock_now, &startvalue);
    clock_Sub(&clock_now, &tmp_clock);

    clock_haveCurrentTime = 1;
    clock_nUpdates++;
}

/* Restart the interval timer */
void
clock_ReInit(void)
{
    struct clock tmp_clock;
    struct itimerval itimer;

    tmp_clock = offset;
    clock_Add(&tmp_clock, &startvalue);
    clock_Sub(&tmp_clock, &clock_now);
    itimer.it_value.tv_sec  = tmp_clock.sec;
    itimer.it_value.tv_usec = tmp_clock.usec;
    itimer.it_interval.tv_sec  = 0;
    itimer.it_interval.tv_usec = 0;

    if (setitimer(ITIMER_REAL, &itimer, NULL) != 0) {
	fprintf(stderr, "clock:  could not set interval timer; aborted\n");
	fflush(stderr);
	exit(1);
    }
}

#else /* ! HAVE_GETITIMER */

struct clock clock_now;		       /* The last elapsed time ready by
				        * clock_GetTimer */

static struct clock offset;	/* time when we start counting */

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
    clock_UpdateTime();
    offset = clock_now;
    clock_now.sec = clock_now.usec = 0;
}

/*
 * Compute the current time.
 */

void 
clock_UpdateTime(void)
{
    struct timeval tv;

    gettimeofday (&tv, NULL);

    clock_now.sec  = tv.tv_sec;
    clock_now.usec = tv.tv_usec;

    clock_Sub(&clock_now, &offset);

    clock_haveCurrentTime = 1;
    clock_nUpdates++;
}

/* Restart the interval timer */
void
clock_ReInit(void)
{
}

#endif /* HAVE_GETITIMER */

#else				       /* KERNEL */
#endif				       /* KERNEL */
