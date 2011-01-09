/*	$OpenBSD: clock.c,v 1.4 2011/01/09 19:37:51 jasper Exp $	*/

/*
 * Copyright (c) 2005 Michael Shalayeff
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/timetc.h>

#include <machine/iomod.h>
#include <machine/reg.h>

#include <dev/clock_subr.h>

u_long	cpu_hzticks;
int	timeset;

u_int	itmr_get_timecount(struct timecounter *);

struct timecounter itmr_timecounter = {
	itmr_get_timecount, NULL, 0xffffffff, 0, "itmr", 0, NULL
};

void
cpu_initclocks()
{
	struct cpu_info *ci = curcpu();
	u_long __itmr;

	cpu_hzticks = (PAGE0->mem_10msec * 100) / hz;

	itmr_timecounter.tc_frequency = PAGE0->mem_10msec * 100;
	tc_init(&itmr_timecounter);

	__itmr = mfctl(CR_ITMR);
	ci->ci_itmr = __itmr;
	__itmr += cpu_hzticks;
	mtctl(__itmr, CR_ITMR);
}

/*
 * initialize the system time from the time of day clock
 */
void
inittodr(time_t t)
{
	struct pdc_tod tod PDC_ALIGNMENT;
	int 	error, tbad = 0;
	struct timespec ts;

	if (t < 12*SECYR) {
		printf ("WARNING: preposterous time in file system");
		t = 6*SECYR + 186*SECDAY + SECDAY/2;
		tbad = 1;
	}

	if ((error = pdc_call((iodcio_t)pdc,
	    1, PDC_TOD, PDC_TOD_READ, &tod, 0, 0, 0, 0, 0)))
		printf("clock: failed to fetch (%d)\n", error);

	ts.tv_sec = tod.sec;
	ts.tv_nsec = tod.usec * 1000;
	tc_setclock(&ts);
	timeset = 1;

	if (!tbad) {
		u_long	dt;

		dt = (tod.sec < t)?  t - tod.sec : tod.sec - t;

		if (dt < 2 * SECDAY)
			return;
		printf("WARNING: clock %s %ld days",
		    tod.sec < t? "lost" : "gained", dt / SECDAY);
	}

	printf (" -- CHECK AND RESET THE DATE!\n");
}

/*
 * reset the time of day clock to the value in time
 */
void
resettodr()
{
	struct timeval tv;
	int error;

	/*
	 * We might have been called by boot() due to a crash early
	 * on.  Don't reset the clock chip in this case.
	 */
	if (!timeset)
		return;

	microtime(&tv);

	if ((error = pdc_call((iodcio_t)pdc, 1, PDC_TOD, PDC_TOD_WRITE,
	    tv.tv_sec, tv.tv_usec)))
		printf("clock: failed to save (%d)\n", error);
}

void
setstatclockrate(int newhz)
{
	/* nothing we can do */
}

u_int
itmr_get_timecount(struct timecounter *tc)
{
	return (mfctl(CR_ITMR));
}
