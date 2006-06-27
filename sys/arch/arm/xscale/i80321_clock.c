/*	$OpenBSD: i80321_clock.c,v 1.1 2006/06/27 05:18:25 drahn Exp $ */

/*
 * Copyright (c) 2006 Dale Rahn <drahn@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/time.h>
#include <sys/device.h>
#include <dev/clock_subr.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <arm/cpufunc.h>

#include <arm/xscale/i80321reg.h>
#include <arm/xscale/i80321var.h>

#define TIMER_FREQUENCY	200000000	/* 200MHz */

static struct evcount clk_count;
static struct evcount stat_count;
static int clk_irq = 129; /* XXX */
static int stat_irq = 130; /* XXX */

uint32_t nextstatevent;
uint32_t nexttickevent;
uint32_t ticks_per_intr;
uint32_t ticks_per_second;
uint32_t lastnow;
uint32_t stat_error_cnt, tick_error_cnt;
uint32_t statvar, statmin;
int i80321_timer_inited;

u_int32_t tmr0_read(void);
void tmr0_write(u_int32_t val);
inline u_int32_t tcr0_read(void);
void tcr0_write(u_int32_t val);
u_int32_t trr0_read(void);
void trr0_write(u_int32_t val);
u_int32_t tmr1_read(void);
void tmr1_write(u_int32_t val);
u_int32_t tcr1_read(void);
void tcr1_write(u_int32_t val);
u_int32_t trr1_read(void);
void trr1_write(u_int32_t val);
u_int32_t tisr_read(void);
void tisr_write(u_int32_t val);
int i80321_intr(void *frame);

/* 
 * TMR0 is used in non-reload mode as it is used for both the clock
 * timer and sched timer.
 *
 * The counters on 80321 are count down interrupt on 0, not match
 * register based, so it is not possible to find out how much
 * many interrupts passed while irqs were blocked.
 * also it is not possible to atomically add to the register
 * get get it to precisely fire at a non-fixed interval.
 *
 * To work around this both timers are used, TMR1 is used as a reference
 * clock set to  auto reload with 0xffffffff, however we just ignore the
 * interrupt it would generate. NOTE: does this drop one tick
 * ever wrap? Could the reference timer be used in non-reload mode,
 * where it would just keep counting, and not stop at 0 ?
 *
 * Internally this keeps track of when the next timer should fire
 * and based on that time and the current value of the reference
 * clock a number is written into the timer count register to schedule
 * the next event.
 */


u_int32_t
tmr0_read(void)
{
	u_int32_t ret;
	__asm volatile ("mrc p6, 0, %0, c0, c1, 0" : "=r" (ret));
	return ret;
}

void
tmr0_write(u_int32_t val)
{
	__asm volatile ("mcr p6, 0, %0, c0, c1, 0" :: "r" (val));
}

inline u_int32_t
tcr0_read(void)
{
	u_int32_t ret;
	__asm volatile ("mrc p6, 0, %0, c2, c1, 0" : "=r" (ret));
	return ret;
}

void
tcr0_write(u_int32_t val)
{
	__asm volatile ("mcr p6, 0, %0, c2, c1, 0" :: "r" (val));
}

u_int32_t
trr0_read(void)
{
	u_int32_t ret;
	__asm volatile ("mrc p6, 0, %0, c4, c1, 0" : "=r" (ret));
	return ret;
}

void
trr0_write(u_int32_t val)
{
	__asm volatile ("mcr p6, 0, %0, c4, c1, 0" :: "r" (val));
}

u_int32_t
tmr1_read(void)
{
	u_int32_t ret;
	__asm volatile ("mrc p6, 0, %0, c1, c1, 0" : "=r" (ret));
	return ret;
}

void
tmr1_write(u_int32_t val)
{
	__asm volatile ("mcr p6, 0, %0, c1, c1, 0" :: "r" (val));
}

u_int32_t
tcr1_read(void)
{
	u_int32_t ret;
	__asm volatile ("mrc p6, 0, %0, c3, c1, 0" : "=r" (ret));
	return ret;
}

void
tcr1_write(u_int32_t val)
{
	__asm volatile ("mcr p6, 0, %0, c3, c1, 0" :: "r" (val));
}

u_int32_t
trr1_read(void)
{
	u_int32_t ret;
	__asm volatile ("mrc p6, 0, %0, c5, c1, 0" : "=r" (ret));
	return ret;
}

void
trr1_write(u_int32_t val)
{
	__asm volatile ("mcr p6, 0, %0, c5, c1, 0" :: "r" (val));
}

u_int32_t
tisr_read()
{
	u_int32_t ret;
	__asm volatile ("mrc p6, 0, %0, c6, c1, 0" : "=r" (ret));
	return ret;
}

void
tisr_write(u_int32_t val)
{
	__asm volatile ("mcr p6, 0, %0, c6, c1, 0" :: "r" (val));
}

/*
 * timer 1 is running a timebase counter,
 * ie reload 0xffffffff, reload, interrupt ignored
 * timer 0 will be programmed with the delay until the next
 * event. this is not set for reload 
 */
int
i80321_intr(void *frame)
{
	uint32_t now, r;
	uint32_t nextevent;

	tisr_write(TISR_TMR0);
	now = tcr1_read();

#if 0
	if (lastnow < now) {
		/* rollover, remove the missing 'tick'; 1-0xffffffff, not 0- */
		nextstatevent -=1;
		nexttickevent -=1;
	}
#endif
	while ((int32_t) (now - nexttickevent) < 0) {
		nexttickevent -= ticks_per_intr;
		/* XXX - correct nexttickevent? */
		clk_count.ec_count++;
		hardclock(frame);
	}
	while ((int32_t) (now - nextstatevent) < 0) {
		do {   
			r = random() & (statvar -1);
		} while (r == 0); /* random == 0 not allowed */
		nextstatevent -= statmin + r;
		/* XXX - correct nextstatevent? */
		stat_count.ec_count++;
		statclock(frame);
	}
	if ((now - nexttickevent) < (now - nextstatevent))
		nextevent = now - nexttickevent;
	else
		nextevent = now - nextstatevent;
	if (nextevent < 10 /* XXX */)
		nextevent = 10;
	if (nextevent > ticks_per_intr) {
		printf("nextevent out of bounds %x\n", nextevent);
		nextevent = ticks_per_intr;
	}


	tcr0_write(nextevent);
	tmr0_write(TMRx_ENABLE|TMRx_PRIV|TMRx_CSEL_CORE);

	lastnow = now;
	
	return 1;
}

void
cpu_initclocks()
{
	int minint;
	uint32_t now;
	uint32_t statint;
	int s;

	s = splclock();
	/* would it make sense to have this be 100/1000 to round nicely? */
	/* 100/1000 or 128/1024 ? */
	stathz = 100;
	profhz = 1000;

	ticks_per_second = 200 * 1000000; /* 200 MHz */
	statint = ticks_per_second / stathz;
	stat_error_cnt = ticks_per_second % stathz;

	/* calculate largest 2^n which is smaller that just over half statint */
	statvar = 0x40000000;
	minint = statint / 2 + 100;
	while (statvar > minint)
		statvar >>= 1;

	statmin = statint - (statvar >> 1);

	ticks_per_intr = ticks_per_second / hz;
	tick_error_cnt = ticks_per_second % hz;

	printf("clock: hz= %d stathz = %d\n", hz, stathz);
	printf("ticks_per_intr %x tick_error_cnt %x statmin %x statvar %x\n",
	 ticks_per_intr, tick_error_cnt, statmin, statvar);

	evcount_attach(&clk_count, "clock", (void *)&clk_irq, &evcount_intr);
	evcount_attach(&stat_count, "stat", (void *)&stat_irq, &evcount_intr);

	(void) i80321_intr_establish(ICU_INT_TMR0, IPL_CLOCK, i80321_intr,
	    NULL, "tick");


	now = 0xffffffff;
	nextstatevent = now - ticks_per_intr;
	nexttickevent = now - ticks_per_intr;

	tcr1_write(now);
	trr1_write(now);
	tmr1_write(TMRx_ENABLE|TMRx_RELOAD|TMRx_PRIV|TMRx_CSEL_CORE);

	tcr0_write(now); /* known big value */
	tmr0_write(TMRx_ENABLE|TMRx_PRIV|TMRx_CSEL_CORE);
	tcr0_write(ticks_per_intr);

	i80321_timer_inited = 1;
	splx(s);
}

void
microtime(struct timeval *tvp)
{
	int s, deltacnt;
	u_int32_t counter, expected;

	if (i80321_timer_inited == 0) {
		tvp->tv_sec = 0;
		tvp->tv_usec = 0;
		return;
	}

	s = splhigh();
	counter = tcr1_read();
	expected = nexttickevent;

	*tvp = time;
	splx (s);

	deltacnt = ticks_per_intr -counter + expected;

#if 0
	/* low frequency timer algorithm */
	tvp->tv_usec +_= deltacnt * 1000000ULL / TIMER_FREQUENCY;
#else
	/* high frequency timer algorithm - XXX */
	tvp->tv_usec += deltacnt / (TIMER_FREQUENCY / 1000000ULL);
#endif

        while (tvp->tv_usec >= 1000000) {
		tvp->tv_sec++;
		tvp->tv_usec -= 1000000;
	}
}

void
delay(u_int usecs)
{
	u_int32_t clock, oclock, delta, delaycnt;
	volatile int j;
	int csec, usec;

		csec = usecs / 10000;
		usec = usecs % 10000;

		delaycnt = (TIMER_FREQUENCY / 100) * csec +
		    (TIMER_FREQUENCY / 100) * usec / 10000;

	if (delaycnt <= 1) /* delay too short spin for a bit */
		for (j = 100; j > 0; j--)
			;

	if (i80321_timer_inited == 0) {
		/* clock isn't initialized yet */
		for (; usecs > 0; usecs--)
			for (j = 100; j > 0; j--)
				;
		return;
	}

	oclock = tcr1_read();

	while(1) {
		clock = tcr1_read();
		/* timer counts down, not up so old - new */
		delta = oclock - clock;
		if (delta > delaycnt)
			break;
	}
}

void
setstatclockrate(int newhz)
{
	int minint, statint;
	int s;

	s = splclock();

	statint = ticks_per_second / newhz;
	statvar = 0x40000000; /* really big power of two */
	/* find largest 2^n which is nearly smaller than statint/2 */
	minint = statint / 2 + 100;
	while (statvar > minint)
		statvar >>= 1;

	statmin = statint - (statvar >> 1);

	splx(s);

	/*
	 * XXX this allows the next stat timer to occur then it switches
	 * to the new frequency. Rather than switching instantly.
	 */
}

void
i80321_calibrate_delay(void)
{

	tmr1_write(0);			/* stop timer */
	tisr_write(TISR_TMR1);		/* clear interrupt */
	trr1_write(0xffffffff);	/* reload value */
	tcr1_write(0xffffffff);	/* current value */

	tmr1_write(TMRx_ENABLE|TMRx_RELOAD|TMRx_PRIV|TMRx_CSEL_CORE);
}

todr_chip_handle_t todr_handle;

/*
 * inittodr:
 *
 *	Initialize time from the time-of-day register.
 */
#define	MINYEAR		2003	/* minimum plausible year */
void
inittodr(time_t base)
{
	time_t deltat;
	struct timeval rtctime;
	int badbase;

	if (base < (MINYEAR - 1970) * SECYR) {
		printf("WARNING: preposterous time in file system");
		/* read the system clock anyway */
		base = (MINYEAR - 1970) * SECYR;
		badbase = 1;
	} else
		badbase = 0;

	if (todr_handle == NULL ||
	    todr_gettime(todr_handle, &rtctime) != 0 ||
	    rtctime.tv_sec == 0) {
		/*
		 * Believe the time in the file system for lack of
		 * anything better, resetting the TODR.
		 */
		time.tv_sec = base;
		time.tv_usec = 0;
		if (todr_handle != NULL && !badbase) {
			printf("WARNING: preposterous clock chip time\n");
			resettodr();
		}
		goto bad;
	} else {
		time.tv_sec = rtctime.tv_sec;
		time.tv_usec = rtctime.tv_usec;
	}

	if (!badbase) {
		/*
		 * See if we gained/lost two or more days; if
		 * so, assume something is amiss.
		 */
		deltat = time.tv_sec - base;
		if (deltat < 0)
			deltat = -deltat;
		if (deltat < 2 * SECDAY)
			return;		/* all is well */
		printf("WARNING: clock %s %ld days\n",
		    time.tv_sec < base ? "lost" : "gained",
		    (long)deltat / SECDAY);
	}
 bad:
	printf("WARNING: CHECK AND RESET THE DATE!\n");
}

/*
 * resettodr:
 *
 *	Reset the time-of-day register with the current time.
 */
void
resettodr(void)
{
	struct timeval rtctime;

	if (time.tv_sec == 0)
		return;

	rtctime.tv_sec = time.tv_sec;
	rtctime.tv_usec = time.tv_usec;

	if (todr_handle != NULL &&
	   todr_settime(todr_handle, &rtctime) != 0)
		printf("resettodr: failed to set time\n");
}

