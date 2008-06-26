/*	$OpenBSD: clock.c,v 1.6 2008/06/26 05:42:13 ray Exp $	*/
/*	$NetBSD: clock.c,v 1.32 2006/09/05 11:09:36 uwe Exp $	*/

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by UCHIYAMA Yasushi.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>

#include <dev/clock_subr.h>

#include <sh/clock.h>
#include <sh/trap.h>
#include <sh/rtcreg.h>
#include <sh/tmureg.h>

#include <machine/intr.h>

#define	NWDOG 0

#ifndef HZ
#define	HZ		64
#endif
#define	MINYEAR		2002	/* "today" */
#define	SH_RTC_CLOCK	16384	/* Hz */

/*
 * OpenBSD/sh clock module
 *  + default 64Hz
 *  + use TMU channel 0 as clock interrupt source.
 *  + use TMU channel 1 and 2 as emulated software interrupt source.
 *  + If RTC module is active, TMU channel 0 input source is RTC output.
 *    (1.6384kHz)
 */
struct {
	/* Hard clock */
	uint32_t hz_cnt;	/* clock interrupt interval count */
	uint32_t cpucycle_1us;	/* calibrated loop variable (1 us) */
	uint32_t tmuclk;	/* source clock of TMU0 (Hz) */

	/* RTC ops holder. default SH RTC module */
	struct rtc_ops rtc;
	int rtc_initialized;

	uint32_t pclock;	/* PCLOCK */
	uint32_t cpuclock;	/* CPU clock */
	int flags;
} sh_clock = {
#ifdef PCLOCK
	.pclock = PCLOCK,
#endif
	.rtc = {
		/* SH RTC module to default RTC */
		.init	= sh_rtc_init,
		.get	= sh_rtc_get,
		.set	= sh_rtc_set
	}
};

uint32_t maxwdog;

/* TMU */
/* interrupt handler is timing critical. prepared for each. */
int sh3_clock_intr(void *);
int sh4_clock_intr(void *);

/*
 * Estimate CPU and Peripheral clock.
 */
#define	TMU_START(x)							\
do {									\
	_reg_bclr_1(SH_(TSTR), TSTR_STR##x);				\
	_reg_write_4(SH_(TCNT ## x), 0xffffffff);			\
	_reg_bset_1(SH_(TSTR), TSTR_STR##x);				\
} while (/*CONSTCOND*/0)
#define	TMU_ELAPSED(x)							\
	(0xffffffff - _reg_read_4(SH_(TCNT ## x)))

void
sh_clock_init(int flags, struct rtc_ops *rtc)
{
	uint32_t s, t0, t1 __attribute__((__unused__));

	sh_clock.flags = flags;
	if (rtc != NULL)
		sh_clock.rtc = *rtc;	/* structure copy */

	/* Initialize TMU */
	_reg_write_2(SH_(TCR0), 0);
	_reg_write_2(SH_(TCR1), 0);
	_reg_write_2(SH_(TCR2), 0);

	/* Reset RTC alarm and interrupt */
	_reg_write_1(SH_(RCR1), 0);

	/* Stop all counter */
	_reg_write_1(SH_(TSTR), 0);

	/*
	 * Estimate CPU clock.
	 */
	if (sh_clock.flags & SH_CLOCK_NORTC) {
		/* Set TMU channel 0 source to PCLOCK / 16 */
		_reg_write_2(SH_(TCR0), TCR_TPSC_P16);
		sh_clock.tmuclk = sh_clock.pclock / 16;
	} else {
		/* Set TMU channel 0 source to RTC counter clock (16.384kHz) */
		_reg_write_2(SH_(TCR0),
		    CPU_IS_SH3 ? SH3_TCR_TPSC_RTC : SH4_TCR_TPSC_RTC);
		sh_clock.tmuclk = SH_RTC_CLOCK;
	}

	s = _cpu_exception_suspend();
	_cpu_spin(1);	/* load function on cache. */
	TMU_START(0);
	_cpu_spin(10000000);
	t0 = TMU_ELAPSED(0);
	_cpu_exception_resume(s);

	sh_clock.cpuclock = ((10000000 * 10) / t0) * sh_clock.tmuclk;
	sh_clock.cpucycle_1us = (sh_clock.tmuclk * 10) / t0;

	if (CPU_IS_SH4)
		sh_clock.cpuclock >>= 1;	/* two-issue */

	/*
	 * Estimate PCLOCK
	 */
	if (sh_clock.pclock == 0) {
		/* set TMU channel 1 source to PCLOCK / 4 */
		_reg_write_2(SH_(TCR1), TCR_TPSC_P4);
		s = _cpu_exception_suspend();
		_cpu_spin(1);	/* load function on cache. */
		TMU_START(0);
		TMU_START(1);
		_cpu_spin(sh_clock.cpucycle_1us * 1000000);	/* 1 sec. */
		t0 = TMU_ELAPSED(0);
		t1 = TMU_ELAPSED(1);
		_cpu_exception_resume(s);

		sh_clock.pclock = ((t1 * 4)/ t0) * SH_RTC_CLOCK;
	}

	/* Stop all counter */
	_reg_write_1(SH_(TSTR), 0);

#undef TMU_START
#undef TMU_ELAPSED
}

int
sh_clock_get_cpuclock()
{
	return (sh_clock.cpuclock);
}

int
sh_clock_get_pclock()
{
	return (sh_clock.pclock);
}

void
setstatclockrate(int newhz)
{
	/* XXX not yet */
}

/*
 * Return the best possible estimate of the time in the timeval to
 * which tv points.
 */
void
microtime(struct timeval *tv)
{
	static struct timeval lasttime;
	u_int32_t tcnt0;
	int s;

	s = splclock();
	*tv = time;
	tcnt0 = _reg_read_4(SH_(TCNT0));
	splx(s);

	tv->tv_usec += ((sh_clock.hz_cnt - tcnt0) * 1000000) / sh_clock.tmuclk;
	while (tv->tv_usec >= 1000000) {
		tv->tv_usec -= 1000000;
		tv->tv_sec++;
	}

	if (tv->tv_sec == lasttime.tv_sec &&
	    tv->tv_usec <= lasttime.tv_usec &&
	    (tv->tv_usec = lasttime.tv_usec + 1) >= 1000000) {
		tv->tv_usec -= 1000000;
		tv->tv_sec++;
	}
	lasttime = *tv;
}

/*
 *  Wait at least `n' usec.
 */
void
delay(int n)
{
	_cpu_spin(sh_clock.cpucycle_1us * n);
}

/*
 * Start the clock interrupt.
 */
void
cpu_initclocks()
{
	if (sh_clock.pclock == 0)
		panic("No PCLOCK information.");

	/* Set global variables. */
	hz = HZ;
	tick = 1000000 / hz;

	/*
	 * Use TMU channel 0 as hard clock
	 */
	_reg_bclr_1(SH_(TSTR), TSTR_STR0);

	if (sh_clock.flags & SH_CLOCK_NORTC) {
		/* use PCLOCK/16 as TMU0 source */
		_reg_write_2(SH_(TCR0), TCR_UNIE | TCR_TPSC_P16);
	} else {
		/* use RTC clock as TMU0 source */
		_reg_write_2(SH_(TCR0), TCR_UNIE |
		    (CPU_IS_SH3 ? SH3_TCR_TPSC_RTC : SH4_TCR_TPSC_RTC));
	}
	sh_clock.hz_cnt = sh_clock.tmuclk / hz - 1;

	_reg_write_4(SH_(TCOR0), sh_clock.hz_cnt);
	_reg_write_4(SH_(TCNT0), sh_clock.hz_cnt);

	intc_intr_establish(SH_INTEVT_TMU0_TUNI0, IST_LEVEL, IPL_CLOCK,
	    CPU_IS_SH3 ? sh3_clock_intr : sh4_clock_intr, NULL, "clock");
	/* start hardclock */
	_reg_bset_1(SH_(TSTR), TSTR_STR0);

	/*
	 * TMU channel 1, 2 are one shot timer.
	 */
	_reg_write_2(SH_(TCR1), TCR_UNIE | TCR_TPSC_P4);
	_reg_write_4(SH_(TCOR1), 0xffffffff);
	_reg_write_2(SH_(TCR2), TCR_UNIE | TCR_TPSC_P4);
	_reg_write_4(SH_(TCOR2), 0xffffffff);

	/* Make sure to start RTC */
	if (sh_clock.rtc.init != NULL)
		sh_clock.rtc.init(sh_clock.rtc._cookie);
}

void
inittodr(time_t base)
{
	struct clock_ymdhms dt;
	time_t rtc;
	int s;

	if (!sh_clock.rtc_initialized)
		sh_clock.rtc_initialized = 1;

	sh_clock.rtc.get(sh_clock.rtc._cookie, base, &dt);
	rtc = clock_ymdhms_to_secs(&dt);

#ifdef DEBUG
	printf("inittodr: %d/%d/%d/%d/%d/%d(%d)\n", dt.dt_year,
	    dt.dt_mon, dt.dt_day, dt.dt_hour, dt.dt_min, dt.dt_sec,
	    dt.dt_wday);
#endif

	if (!(sh_clock.flags & SH_CLOCK_NOINITTODR) &&
	    (rtc < base ||
		dt.dt_year < MINYEAR || dt.dt_year > 2037 ||
		dt.dt_mon < 1 || dt.dt_mon > 12 ||
		dt.dt_wday > 6 ||
		dt.dt_day < 1 || dt.dt_day > 31 ||
		dt.dt_hour > 23 || dt.dt_min > 59 || dt.dt_sec > 59)) {
		/*
		 * Believe the time in the file system for lack of
		 * anything better, resetting the RTC.
		 */
		s = splclock();
		time.tv_sec = base;
		time.tv_usec = 0;
		splx(s);
		printf("WARNING: preposterous clock chip time\n");
		resettodr();
		printf(" -- CHECK AND RESET THE DATE!\n");
		return;
	}

	s = splclock();
	time.tv_sec = rtc;
	time.tv_usec = 0;
	splx(s);

	return;
}

void
resettodr()
{
	struct clock_ymdhms dt;
	int s;

	if (!sh_clock.rtc_initialized)
		return;

	s = splclock();
	clock_secs_to_ymdhms(time.tv_sec, &dt);
	splx(s);

	sh_clock.rtc.set(sh_clock.rtc._cookie, &dt);
#ifdef DEBUG
        printf("%s: %d/%d/%d/%d/%d/%d(%d)\n", __FUNCTION__,
	    dt.dt_year, dt.dt_mon, dt.dt_day, dt.dt_hour, dt.dt_min, dt.dt_sec,
	    dt.dt_wday);
#endif
}

#ifdef SH3
int
sh3_clock_intr(void *arg) /* trap frame */
{
#if (NWDOG > 0)
	uint32_t i;

	i = (uint32_t)SHREG_WTCNT_R;
	if (i > maxwdog)
		maxwdog = i;
	wdog_wr_cnt(0);			/* reset to zero */
#endif
	/* clear underflow status */
	_reg_bclr_2(SH3_TCR0, TCR_UNF);

	hardclock(arg);

	return (1);
}
#endif /* SH3 */

#ifdef SH4
int
sh4_clock_intr(void *arg) /* trap frame */
{
#if (NWDOG > 0)
	uint32_t i;

	i = (uint32_t)SHREG_WTCNT_R;
	if (i > maxwdog)
		maxwdog = i;
	wdog_wr_cnt(0);			/* reset to zero */
#endif
	/* clear underflow status */
	_reg_bclr_2(SH4_TCR0, TCR_UNF);

	hardclock(arg);

	return (1);
}
#endif /* SH4 */

/*
 * SH3 RTC module ops.
 */

void
sh_rtc_init(void *cookie)
{
	/* Make sure to start RTC */
	_reg_write_1(SH_(RCR2), SH_RCR2_ENABLE | SH_RCR2_START);
}

void
sh_rtc_get(void *cookie, time_t base, struct clock_ymdhms *dt)
{
	int retry = 8;

	/* disable carry interrupt */
	_reg_bclr_1(SH_(RCR1), SH_RCR1_CIE);

	do {
		uint8_t r = _reg_read_1(SH_(RCR1));
		r &= ~SH_RCR1_CF;
		r |= SH_RCR1_AF; /* don't clear alarm flag */
		_reg_write_1(SH_(RCR1), r);

		if (CPU_IS_SH3)
			dt->dt_year = FROMBCD(_reg_read_1(SH3_RYRCNT));
		else
			dt->dt_year = FROMBCD(_reg_read_2(SH4_RYRCNT) & 0x00ff);

		/* read counter */
#define	RTCGET(x, y)	dt->dt_ ## x = FROMBCD(_reg_read_1(SH_(R ## y ## CNT)))
		RTCGET(mon, MON);
		RTCGET(wday, WK);
		RTCGET(day, DAY);
		RTCGET(hour, HR);
		RTCGET(min, MIN);
		RTCGET(sec, SEC);
#undef RTCGET
	} while ((_reg_read_1(SH_(RCR1)) & SH_RCR1_CF) && --retry > 0);

	if (retry == 0) {
		printf("rtc_gettime: couldn't read RTC register.\n");
		memset(dt, 0, sizeof(*dt));
		return;
	}

	dt->dt_year = (dt->dt_year % 100) + 1900;
	if (dt->dt_year < 1970)
		dt->dt_year += 100;
}

void
sh_rtc_set(void *cookie, struct clock_ymdhms *dt)
{
	uint8_t r;

	/* stop clock */
	r = _reg_read_1(SH_(RCR2));
	r |= SH_RCR2_RESET;
	r &= ~SH_RCR2_START;
	_reg_write_1(SH_(RCR2), r);

	/* set time */
	if (CPU_IS_SH3)
		_reg_write_1(SH3_RYRCNT, TOBCD(dt->dt_year % 100));
	else
		_reg_write_2(SH4_RYRCNT, TOBCD(dt->dt_year % 100));
#define	RTCSET(x, y)	_reg_write_1(SH_(R ## x ## CNT), TOBCD(dt->dt_ ## y))
	RTCSET(MON, mon);
	RTCSET(WK, wday);
	RTCSET(DAY, day);
	RTCSET(HR, hour);
	RTCSET(MIN, min);
	RTCSET(SEC, sec);
#undef RTCSET
	/* start clock */
	_reg_write_1(SH_(RCR2), r | SH_RCR2_START);
}
