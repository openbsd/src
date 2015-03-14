/*	$OpenBSD: clock.c,v 1.22 2015/03/14 03:38:46 jsg Exp $	*/
/*	$NetBSD: clock.c,v 1.1 2003/04/26 18:39:50 fvdl Exp $	*/

/*-
 * Copyright (c) 1993, 1994 Charles M. Hannum.
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz and Don Ahn.
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
 *	@(#)clock.c	7.2 (Berkeley) 5/12/91
 */
/*
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
/*
  Copyright 1988, 1989 by Intel Corporation, Santa Clara, California.

		All Rights Reserved

Permission to use, copy, modify, and distribute this software and
its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appears in all
copies and that both the copyright notice and this permission notice
appear in supporting documentation, and that the name of Intel
not be used in advertising or publicity pertaining to distribution
of the software without specific, written prior permission.

INTEL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
IN NO EVENT SHALL INTEL BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

/*
 * Primitive clock interrupt routines.
 */

/* #define CLOCKDEBUG */
/* #define CLOCK_PARANOIA */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/timeout.h>
#include <sys/timetc.h>

#include <machine/cpu.h>
#include <machine/intr.h>
#include <machine/pio.h>
#include <machine/cpufunc.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <dev/ic/mc146818reg.h>
#include <dev/ic/i8253reg.h>
#include <amd64/isa/nvram.h>
#include <machine/specialreg.h>

/* Timecounter on the i8254 */
u_int32_t i8254_lastcount;
u_int32_t i8254_offset;
int i8254_ticked;
u_int i8254_get_timecount(struct timecounter *tc);

u_int i8254_simple_get_timecount(struct timecounter *tc);

static struct timecounter i8254_timecounter = {
	i8254_get_timecount, NULL, ~0u, TIMER_FREQ, "i8254", 0, NULL
};

int	clockintr(void *);
int	rtcintr(void *);
int	gettick(void);
void	rtcdrain(void *v);
int	rtcget(mc_todregs *);
void	rtcput(mc_todregs *);
int	bcdtobin(int);
int	bintobcd(int);

u_int mc146818_read(void *, u_int);
void mc146818_write(void *, u_int, u_int);

u_int
mc146818_read(void *sc, u_int reg)
{
	outb(IO_RTC, reg);
	DELAY(1);
	return (inb(IO_RTC+1));
}

void
mc146818_write(void *sc, u_int reg, u_int datum)
{
	outb(IO_RTC, reg);
	DELAY(1);
	outb(IO_RTC+1, datum);
	DELAY(1);
}

struct mutex timer_mutex = MUTEX_INITIALIZER(IPL_HIGH);

u_long rtclock_tval;

void
startclocks(void)
{
	int s;

	mtx_enter(&timer_mutex);
	rtclock_tval = TIMER_DIV(hz);
	i8254_startclock();
	mtx_leave(&timer_mutex);

	/* Check diagnostic status */
	if ((s = mc146818_read(NULL, NVRAM_DIAG)) != 0)	/* XXX softc */
		printf("RTC BIOS diagnostic error %b\n", s, NVRAM_DIAG_BITS);
}

int
clockintr(void *arg)
{
	struct clockframe *frame = arg;

	if (timecounter->tc_get_timecount == i8254_get_timecount) {
		if (i8254_ticked) {
			i8254_ticked = 0;
		} else {
			i8254_offset += rtclock_tval;
			i8254_lastcount = 0;
		}
	}

	hardclock(frame);

	return 1;
}

int
rtcintr(void *arg)
{
	struct clockframe *frame = arg;
	u_int stat = 0;

	/*
	* If rtcintr is 'late', next intr may happen immediately.
	* Get them all. (Also, see comment in cpu_initclocks().)
	*/
	while (mc146818_read(NULL, MC_REGC) & MC_REGC_PF) {
		statclock(frame);
		stat = 1;
	}

	return (stat);
}

int
gettick(void)
{
	u_long ef;
	u_char lo, hi;

	/* Don't want someone screwing with the counter while we're here. */
	mtx_enter(&timer_mutex);
	ef = read_rflags();
	disable_intr();
	/* Select counter 0 and latch it. */
	outb(IO_TIMER1+TIMER_MODE, TIMER_SEL0 | TIMER_LATCH);
	lo = inb(IO_TIMER1+TIMER_CNTR0);
	hi = inb(IO_TIMER1+TIMER_CNTR0);
	write_rflags(ef);
	mtx_leave(&timer_mutex);
	return ((hi << 8) | lo);
}

/*
 * Wait "n" microseconds.
 * Relies on timer 1 counting down from (TIMER_FREQ / hz) at TIMER_FREQ Hz.
 * Note: timer had better have been programmed before this is first used!
 * (Note that we use `rate generator' mode, which counts at 1:1; `square
 * wave' mode counts at 2:1).
 */
void
i8254_delay(int n)
{
	int limit, tick, otick;
	static const int delaytab[26] = {
		 0,  2,  3,  4,  5,  6,  7,  9, 10, 11,
		12, 13, 15, 16, 17, 18, 19, 21, 22, 23,
		24, 25, 27, 28, 29, 30,
	};

	/*
	 * Read the counter first, so that the rest of the setup overhead is
	 * counted.
	 */
	otick = gettick();

	if (n <= 25)
		n = delaytab[n];
	else {
#ifdef __GNUC__
		/*
		 * Calculate ((n * TIMER_FREQ) / 1e6) using explicit assembler
		 * code so we can take advantage of the intermediate 64-bit
		 * quantity to prevent loss of significance.
		 */
		int m;
		__asm volatile("mul %3"
				 : "=a" (n), "=d" (m)
				 : "0" (n), "r" (TIMER_FREQ));
		__asm volatile("div %4"
				 : "=a" (n), "=d" (m)
				 : "0" (n), "1" (m), "r" (1000000));
#else
		/*
		 * Calculate ((n * TIMER_FREQ) / 1e6) without using floating
		 * point and without any avoidable overflows.
		 */
		int sec = n / 1000000,
		    usec = n % 1000000;
		n = sec * TIMER_FREQ +
		    usec * (TIMER_FREQ / 1000000) +
		    usec * ((TIMER_FREQ % 1000000) / 1000) / 1000 +
		    usec * (TIMER_FREQ % 1000) / 1000000;
#endif
	}

	limit = TIMER_FREQ / hz;

	while (n > 0) {
		tick = gettick();
		if (tick > otick)
			n -= limit - (tick - otick);
		else
			n -= otick - tick;
		otick = tick;
	}
}

void
rtcdrain(void *v)
{
	struct timeout *to = (struct timeout *)v;

	if (to != NULL)
		timeout_del(to);

	/*
	* Drain any un-acknowledged RTC interrupts.
	* See comment in cpu_initclocks().
	*/
	while (mc146818_read(NULL, MC_REGC) & MC_REGC_PF)
		; /* Nothing. */
}

void
i8254_initclocks(void)
{
	stathz = 128;
	profhz = 1024;

	isa_intr_establish(NULL, 0, IST_PULSE, IPL_CLOCK, clockintr,
	    0, "clock");
	isa_intr_establish(NULL, 8, IST_PULSE, IPL_STATCLOCK, rtcintr,
	    0, "rtc");

	rtcstart();			/* start the mc146818 clock */

	i8254_inittimecounter();	/* hook the interrupt-based i8254 tc */
}

void
rtcstart(void)
{
	static struct timeout rtcdrain_timeout;

	mc146818_write(NULL, MC_REGA, MC_BASE_32_KHz | MC_RATE_128_Hz);
	mc146818_write(NULL, MC_REGB, MC_REGB_24HR | MC_REGB_PIE);

	/*
	 * On a number of i386 systems, the rtc will fail to start when booting
	 * the system. This is due to us missing to acknowledge an interrupt
	 * during early stages of the boot process. If we do not acknowledge
	 * the interrupt, the rtc clock will not generate further interrupts.
	 * To solve this, once interrupts are enabled, use a timeout (once)
	 * to drain any un-acknowledged rtc interrupt(s).
	 */
	timeout_set(&rtcdrain_timeout, rtcdrain, (void *)&rtcdrain_timeout);
	timeout_add(&rtcdrain_timeout, 1);
}

void
rtcstop(void)
{
	mc146818_write(NULL, MC_REGB, MC_REGB_24HR);
}

int
rtcget(mc_todregs *regs)
{
	if ((mc146818_read(NULL, MC_REGD) & MC_REGD_VRT) == 0) /* XXX softc */
		return (-1);
	MC146818_GETTOD(NULL, regs);			/* XXX softc */
	return (0);
}

void
rtcput(mc_todregs *regs)
{
	MC146818_PUTTOD(NULL, regs);			/* XXX softc */
}

int
bcdtobin(int n)
{
	return (((n >> 4) & 0x0f) * 10 + (n & 0x0f));
}

int
bintobcd(int n)
{
	return ((u_char)(((n / 10) << 4) & 0xf0) | ((n % 10) & 0x0f));
}

static int timeset;

/*
 * check whether the CMOS layout is "standard"-like (ie, not PS/2-like),
 * to be called at splclock()
 */
static int cmoscheck(void);
static int
cmoscheck(void)
{
	int i;
	unsigned short cksum = 0;

	for (i = 0x10; i <= 0x2d; i++)
		cksum += mc146818_read(NULL, i); /* XXX softc */

	return (cksum == (mc146818_read(NULL, 0x2e) << 8)
			  + mc146818_read(NULL, 0x2f));
}

/*
 * patchable to control century byte handling:
 * 1: always update
 * -1: never touch
 * 0: try to figure out itself
 */
int rtc_update_century = 0;

/*
 * Expand a two-digit year as read from the clock chip
 * into full width.
 * Being here, deal with the CMOS century byte.
 */
static int centb = NVRAM_CENTURY;
static int clock_expandyear(int);
static int
clock_expandyear(int clockyear)
{
	int s, clockcentury, cmoscentury;

	clockcentury = (clockyear < 70) ? 20 : 19;
	clockyear += 100 * clockcentury;

	if (rtc_update_century < 0)
		return (clockyear);

	s = splclock();
	if (cmoscheck())
		cmoscentury = mc146818_read(NULL, NVRAM_CENTURY);
	else
		cmoscentury = 0;
	splx(s);
	if (!cmoscentury) {
#ifdef DIAGNOSTIC
		printf("clock: unknown CMOS layout\n");
#endif
		return (clockyear);
	}
	cmoscentury = bcdtobin(cmoscentury);

	if (cmoscentury != clockcentury) {
		/* XXX note: saying "century is 20" might confuse the naive. */
		printf("WARNING: NVRAM century is %d but RTC year is %d\n",
		       cmoscentury, clockyear);

		/* Kludge to roll over century. */
		if ((rtc_update_century > 0) ||
		    ((cmoscentury == 19) && (clockcentury == 20) &&
		     (clockyear == 2000))) {
			printf("WARNING: Setting NVRAM century to %d\n",
			       clockcentury);
			s = splclock();
			mc146818_write(NULL, centb, bintobcd(clockcentury));
			splx(s);
		}
	} else if (cmoscentury == 19 && rtc_update_century == 0)
		rtc_update_century = 1; /* will update later in resettodr() */

	return (clockyear);
}

/*
 * Initialize the time of day register, based on the time base which is, e.g.
 * from a filesystem.
 */
void
inittodr(time_t base)
{
	struct timespec ts;
	mc_todregs rtclk;
	struct clock_ymdhms dt;
	int s;

	ts.tv_nsec = 0;

	/*
	 * We mostly ignore the suggested time (which comes from the
	 * file system) and go for the RTC clock time stored in the
	 * CMOS RAM.  If the time can't be obtained from the CMOS, or
	 * if the time obtained from the CMOS is 5 or more years less
	 * than the suggested time, we used the suggested time.  (In
	 * the latter case, it's likely that the CMOS battery has
	 * died.)
	 */

	/*
	 * if the file system time is more than a year older than the
	 * kernel, warn and then set the base time to the CONFIG_TIME.
	 */
	if (base < 30*SECYR) {	/* if before 2000, something's odd... */
		printf("WARNING: preposterous time in file system\n");
		base = 30*SECYR;
	}

	s = splclock();
	if (rtcget(&rtclk)) {
		splx(s);
		printf("WARNING: invalid time in clock chip\n");
		goto fstime;
	}
	splx(s);
#ifdef DEBUG_CLOCK
	printf("readclock: %x/%x/%x %x:%x:%x\n", rtclk[MC_YEAR],
	    rtclk[MC_MONTH], rtclk[MC_DOM], rtclk[MC_HOUR], rtclk[MC_MIN],
	    rtclk[MC_SEC]);
#endif

	dt.dt_sec = bcdtobin(rtclk[MC_SEC]);
	dt.dt_min = bcdtobin(rtclk[MC_MIN]);
	dt.dt_hour = bcdtobin(rtclk[MC_HOUR]);
	dt.dt_day = bcdtobin(rtclk[MC_DOM]);
	dt.dt_mon = bcdtobin(rtclk[MC_MONTH]);
	dt.dt_year = clock_expandyear(bcdtobin(rtclk[MC_YEAR]));

	/*
	 * If time_t is 32 bits, then the "End of Time" is
	 * Mon Jan 18 22:14:07 2038 (US/Eastern)
	 * This code copes with RTC's past the end of time if time_t
	 * is an int32 or less. Needed because sometimes RTCs screw
	 * up or are badly set, and that would cause the time to go
	 * negative in the calculation below, which causes Very Bad
	 * Mojo. This at least lets the user boot and fix the problem.
	 * Note the code is self eliminating once time_t goes to 64 bits.
	 */
	if (sizeof(time_t) <= sizeof(int32_t)) {
		if (dt.dt_year >= 2038) {
			printf("WARNING: RTC time at or beyond 2038.\n");
			dt.dt_year = 2037;
			printf("WARNING: year set back to 2037.\n");
			printf("WARNING: CHECK AND RESET THE DATE!\n");
		}
	}

	ts.tv_sec = clock_ymdhms_to_secs(&dt) + tz.tz_minuteswest * 60;
	if (tz.tz_dsttime)
		ts.tv_sec -= 3600;

	if (base != 0 && base < ts.tv_sec - 5*SECYR)
		printf("WARNING: file system time much less than clock time\n");
	else if (base > ts.tv_sec + 5*SECYR) {
		printf("WARNING: clock time much less than file system time\n");
		printf("WARNING: using file system time\n");
		goto fstime;
	}

	tc_setclock(&ts);
	timeset = 1;
	return;

fstime:
	ts.tv_sec = base;
	tc_setclock(&ts);
	timeset = 1;
	printf("WARNING: CHECK AND RESET THE DATE!\n");
}

/*
 * Reset the clock.
 */
void
resettodr(void)
{
	mc_todregs rtclk;
	struct clock_ymdhms dt;
	int century, diff, s;

	/*
	 * We might have been called by boot() due to a crash early
	 * on.  Don't reset the clock chip in this case.
	 */
	if (!timeset)
		return;

	s = splclock();
	if (rtcget(&rtclk))
		memset(&rtclk, 0, sizeof(rtclk));
	splx(s);

	diff = tz.tz_minuteswest * 60;
	if (tz.tz_dsttime)
		diff -= 3600;
	clock_secs_to_ymdhms(time_second - diff, &dt);

	rtclk[MC_SEC] = bintobcd(dt.dt_sec);
	rtclk[MC_MIN] = bintobcd(dt.dt_min);
	rtclk[MC_HOUR] = bintobcd(dt.dt_hour);
	rtclk[MC_DOW] = dt.dt_wday + 1;
	rtclk[MC_YEAR] = bintobcd(dt.dt_year % 100);
	rtclk[MC_MONTH] = bintobcd(dt.dt_mon);
	rtclk[MC_DOM] = bintobcd(dt.dt_day);

#ifdef DEBUG_CLOCK
	printf("setclock: %x/%x/%x %x:%x:%x\n", rtclk[MC_YEAR], rtclk[MC_MONTH],
	   rtclk[MC_DOM], rtclk[MC_HOUR], rtclk[MC_MIN], rtclk[MC_SEC]);
#endif
	s = splclock();
	rtcput(&rtclk);
	if (rtc_update_century > 0) {
		century = bintobcd(dt.dt_year / 100);
		mc146818_write(NULL, centb, century); /* XXX softc */
	}
	splx(s);
}

void
setstatclockrate(int arg)
{
	if (initclock_func == i8254_initclocks) {
		if (arg == stathz)
			mc146818_write(NULL, MC_REGA,
			    MC_BASE_32_KHz | MC_RATE_128_Hz);
		else
			mc146818_write(NULL, MC_REGA,
			    MC_BASE_32_KHz | MC_RATE_1024_Hz);
	}
}

void
i8254_inittimecounter(void)
{
	tc_init(&i8254_timecounter);
}

/*
 * If we're using lapic to drive hardclock, we can use a simpler
 * algorithm for the i8254 timecounters.
 */
void
i8254_inittimecounter_simple(void)
{
	i8254_timecounter.tc_get_timecount = i8254_simple_get_timecount;
	i8254_timecounter.tc_counter_mask = 0x7fff;
        i8254_timecounter.tc_frequency = TIMER_FREQ;

	mtx_enter(&timer_mutex);
	rtclock_tval = 0x8000;
	i8254_startclock();
	mtx_leave(&timer_mutex);

	tc_init(&i8254_timecounter);
}

void
i8254_startclock(void)
{
	u_long tval = rtclock_tval;

	outb(IO_TIMER1 + TIMER_MODE, TIMER_SEL0 | TIMER_RATEGEN | TIMER_16BIT);
	outb(IO_TIMER1 + TIMER_CNTR0, tval & 0xff);
	outb(IO_TIMER1 + TIMER_CNTR0, tval >> 8);
}

u_int
i8254_simple_get_timecount(struct timecounter *tc)
{
	return (rtclock_tval - gettick());
}

u_int
i8254_get_timecount(struct timecounter *tc)
{
	u_char hi, lo;
	u_int count;
	u_long ef;

	ef = read_rflags();
	disable_intr();

	outb(IO_TIMER1+TIMER_MODE, TIMER_SEL0 | TIMER_LATCH);
	lo = inb(IO_TIMER1+TIMER_CNTR0);
	hi = inb(IO_TIMER1+TIMER_CNTR0);

	count = rtclock_tval - ((hi << 8) | lo);

	if (count < i8254_lastcount) {
		i8254_ticked = 1;
		i8254_offset += rtclock_tval;
	}
	i8254_lastcount = count;
	count += i8254_offset;
	write_rflags(ef);

	return (count);
}
