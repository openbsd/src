/*	$OpenBSD: clock.c,v 1.19 2010/06/30 20:38:49 tedu Exp $	*/
/*	$NetBSD: clock.c,v 1.29 2000/06/05 21:47:10 thorpej Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and Ralph Campbell.
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
 *
 * from: Utah Hdr: clock.c 1.18 91/01/21
 *
 *	@(#)clock.c	8.1 (Berkeley) 6/10/93
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/evcount.h>
#include <sys/timetc.h>

#include <dev/clock_subr.h>

#include <machine/rpb.h>
#include <machine/autoconf.h>
#include <machine/cpuconf.h>

#include <alpha/alpha/clockvar.h>

#define MINYEAR 1998 /* "today" */
#define UNIX_YEAR_OFFSET 0
 
extern int schedhz;

struct device *clockdev;
const struct clockfns *clockfns;
int clockinitted;
struct evcount clk_count;
int clk_irq = 0;

u_int rpcc_get_timecount(struct timecounter *);
struct timecounter rpcc_timecounter = {
	rpcc_get_timecount, NULL, ~0u, 0, "rpcc", 0, NULL
};

void
clockattach(dev, fns)
	struct device *dev;
	const struct clockfns *fns;
{

	/*
	 * Just bookkeeping.
	 */
	printf("\n");

	if (clockfns != NULL)
		panic("clockattach: multiple clocks");
	clockdev = dev;
	clockfns = fns;
}

/*
 * Machine-dependent clock routines.
 *
 * Startrtclock restarts the real-time clock, which provides
 * hardclock interrupts to kern_clock.c.
 *
 * Inittodr initializes the time of day hardware which provides
 * date functions.  Its primary function is to use some file
 * system information in case the hardware clock lost state.
 *
 * Resettodr restores the time of day hardware after a time change.
 */

/*
 * Start the real-time and statistics clocks. Leave stathz 0 since there
 * are no other timers available.
 */
void
cpu_initclocks(void)
{
	u_int32_t cycles_per_sec;
	struct clocktime ct;
	u_int32_t first_rpcc, second_rpcc; /* only lower 32 bits are valid */
	int first_sec;

	if (clockfns == NULL)
		panic("cpu_initclocks: no clock attached");

	tick = 1000000 / hz;	/* number of microseconds between interrupts */

	/*
	 * Establish the clock interrupt; it's a special case.
	 *
	 * We establish the clock interrupt this late because if
	 * we do it at clock attach time, we may have never been at
	 * spl0() since taking over the system.  Some versions of
	 * PALcode save a clock interrupt, which would get delivered
	 * when we spl0() in autoconf.c.  If established the clock
	 * interrupt handler earlier, that interrupt would go to
	 * hardclock, which would then fall over because p->p_stats
	 * isn't set at that time.
	 */
	platform.clockintr = hardclock;
	schedhz = 16;

	evcount_attach(&clk_count, "clock", (void *)&clk_irq, &evcount_intr);

	/*
	 * Get the clock started.
	 */
	(*clockfns->cf_init)(clockdev);

	/*
	 * Calibrate the cycle counter frequency.
	 */
	(*clockfns->cf_get)(clockdev, 0, &ct);
	first_sec = ct.sec;

	/* Let the clock tick one second. */
	do {
		first_rpcc = alpha_rpcc();
		(*clockfns->cf_get)(clockdev, 0, &ct);
	} while (ct.sec == first_sec);
	first_sec = ct.sec;
	/* Let the clock tick one more second. */
	do {
		second_rpcc = alpha_rpcc();
		(*clockfns->cf_get)(clockdev, 0, &ct);
	} while (ct.sec == first_sec);

	cycles_per_sec = second_rpcc - first_rpcc;

	rpcc_timecounter.tc_frequency = cycles_per_sec;

	tc_init(&rpcc_timecounter);
}

/*
 * We assume newhz is either stathz or profhz, and that neither will
 * change after being set up above.  Could recalculate intervals here
 * but that would be a drag.
 */
void
setstatclockrate(newhz)
	int newhz;
{

	/* nothing we can do */
}

/*
 * Initialize the time of day register, based on the time base which is, e.g.
 * from a filesystem.  Base provides the time to within six months,
 * and the time of year clock (if any) provides the rest.
 */
void
inittodr(time_t base)
{
	struct clocktime ct;
	int year;
	struct clock_ymdhms dt;
	time_t deltat;
	int badbase;
	struct timespec ts;

	ts.tv_sec = ts.tv_nsec = 0;

	if (base < (MINYEAR-1970)*SECYR) {
		printf("WARNING: preposterous time in file system");
		/* read the system clock anyway */
		base = (MINYEAR-1970)*SECYR;
		badbase = 1;
	} else
		badbase = 0;

	(*clockfns->cf_get)(clockdev, base, &ct);
#ifdef DEBUG
	printf("readclock: %d/%d/%d/%d/%d/%d", ct.year, ct.mon, ct.day,
	       ct.hour, ct.min, ct.sec);
#endif
	clockinitted = 1;

	year = 1900 + UNIX_YEAR_OFFSET + ct.year;
	if (year < 1970)
		year += 100;
	/* simple sanity checks (2037 = time_t overflow) */
	if (year < MINYEAR || year > 2037 ||
	    ct.mon < 1 || ct.mon > 12 || ct.day < 1 ||
	    ct.day > 31 || ct.hour > 23 || ct.min > 59 || ct.sec > 59) {
		/*
		 * Believe the time in the file system for lack of
		 * anything better, resetting the TODR.
		 */
		ts.tv_sec = base;
		if (!badbase) {
			printf("WARNING: preposterous clock chip time\n");
			resettodr();
		}
		goto bad;
	}

	dt.dt_year = year;
	dt.dt_mon = ct.mon;
	dt.dt_day = ct.day;
	dt.dt_hour = ct.hour;
	dt.dt_min = ct.min;
	dt.dt_sec = ct.sec;
	ts.tv_sec = clock_ymdhms_to_secs(&dt);
#ifdef DEBUG
	printf("=>%ld (%d)\n", ts.tv_sec, base);
#endif

	if (!badbase) {
		/*
		 * See if we gained/lost two or more days;
		 * if so, assume something is amiss.
		 */
		deltat = ts.tv_sec - base;
		if (deltat < 0)
			deltat = -deltat;
		if (deltat < 2 * SECDAY) {
			tc_setclock(&ts);
			return;
		}
		printf("WARNING: clock %s %ld days",
		    ts.tv_sec < base ? "lost" : "gained",
		    (long)deltat / SECDAY);
	}
bad:
	tc_setclock(&ts);
	printf(" -- CHECK AND RESET THE DATE!\n");
}

/*
 * Reset the TODR based on the time value; used when the TODR
 * has a preposterous value and also when the time is reset
 * by the stime system call.  Also called when the TODR goes past
 * TODRZERO + 100*(SECYEAR+2*SECDAY) (e.g. on Jan 2 just after midnight)
 * to wrap the TODR around.
 */
void
resettodr()
{
	struct clock_ymdhms dt;
	struct clocktime ct;

	if (!clockinitted)
		return;

	clock_secs_to_ymdhms(time_second, &dt);

	/* rt clock wants 2 digits */
	ct.year = (dt.dt_year - UNIX_YEAR_OFFSET) % 100;
	ct.mon = dt.dt_mon;
	ct.day = dt.dt_day;
	ct.hour = dt.dt_hour;
	ct.min = dt.dt_min;
	ct.sec = dt.dt_sec;
	ct.dow = dt.dt_wday;
#ifdef DEBUG
	printf("setclock: %d/%d/%d/%d/%d/%d\n", ct.year, ct.mon, ct.day,
	       ct.hour, ct.min, ct.sec);
#endif

	(*clockfns->cf_set)(clockdev, &ct);
}

u_int
rpcc_get_timecount(struct timecounter *tc)
{
	return alpha_rpcc();
}
