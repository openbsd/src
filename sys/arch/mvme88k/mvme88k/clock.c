/*	$OpenBSD: clock.c,v 1.5 2013/05/17 22:38:25 miod Exp $ */

/*
 * Copyright (c) 1995 Theo de Raadt
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/timetc.h>

#include <mvme88k/mvme88k/clockvar.h>

time_t (*md_inittodr)(void);
void (*md_resettodr)(void);

/*
 * Set up the system's time, given a `reasonable' time value.
 */

void
inittodr(time_t base)
{
	int badbase = 0, waszero = base == 0;
	struct timespec ts;

	ts.tv_sec = ts.tv_nsec = 0;

	if (base < 43 * SECYR) {
		/*
		 * If base is 0, assume filesystem time is just unknown
		 * in stead of preposterous. Don't bark.
		 */
		if (base != 0)
			printf("WARNING: preposterous time in file system\n");
		/* not going to use it anyway, if the chip is readable */
		base = 43 * SECYR;
		badbase = 1;
	}

	if (md_inittodr != NULL)
		ts.tv_sec = (*md_inittodr)();

	if (ts.tv_sec == 0) {
		printf("WARNING: bad date in nvram");
		/*
		 * Believe the time in the file system for lack of
		 * anything better, resetting the clock.
		 */
		ts.tv_sec = base;
		if (!badbase)
			resettodr();
	} else {
		int deltat = ts.tv_sec - base;

		if (deltat < 0)
			deltat = -deltat;
		if (waszero || deltat < 2 * SECDAY)
			goto done;
		printf("WARNING: clock %s %d days",
		       ts.tv_sec < base ? "lost" : "gained", deltat / SECDAY);
	}
	printf(" -- CHECK AND RESET THE DATE!\n");
done:
	tc_setclock(&ts);
}

/*
 * Reset the clock based on the current time.
 * Used when the current clock is preposterous, when the time is changed,
 * and when rebooting.  Do nothing if the time is not yet known, e.g.,
 * when crashing during autoconfig.
 */
void
resettodr()
{
	if (time_second == 0)
		return;

	if (md_resettodr != NULL)
		(*md_resettodr)();
}
