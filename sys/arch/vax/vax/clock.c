/*	$OpenBSD: clock.c,v 1.25 2015/02/07 00:09:09 miod Exp $	 */
/*	$NetBSD: clock.c,v 1.35 2000/06/04 06:16:58 matt Exp $	 */
/*
 * Copyright (c) 1995 Ludd, University of Lule}, Sweden.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *     This product includes software developed at Ludd, University of Lule}.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/timetc.h>

#include <dev/clock_subr.h>

#include <machine/mtpr.h>
#include <machine/sid.h>
#include <machine/clock.h>
#include <machine/cpu.h>
#include <machine/uvax.h>

struct evcount clock_intrcnt;
uint32_t icr_count;

#if defined(VAX46) || defined(VAX48)
u_int	vax_diagtmr_get_tc(struct timecounter *);

/*
 * This counter is only reliable in millisecond units.  See comments in
 * vax_diagtmr_get_tc() for details.
 */
struct timecounter vax_diagtmr_tc = {
	.tc_get_timecount = vax_diagtmr_get_tc,
	.tc_counter_mask = 0xffff,	/* 16 bits */
	.tc_frequency = 1000,		/* 1kHz */
	/* tc_name will be filled in */
	.tc_quality = 100
};
#endif

#ifdef VXT
u_int	vax_vxt_get_tc(struct timecounter *);

struct timecounter vax_vxt_tc = {
	.tc_get_timecount = vax_vxt_get_tc,
	.tc_counter_mask = 0xffffffff,
	.tc_frequency = 100,		/* 100Hz */
	.tc_name = "vxt",
	.tc_quality = 0
};
#endif

u_int	vax_icr_get_tc(struct timecounter *);

struct timecounter vax_icr_tc = {
	.tc_get_timecount = vax_icr_get_tc,
	.tc_counter_mask = 0xffffffff,
	.tc_frequency = 1000000,	/* 1MHz */
	.tc_name = "icr",
	.tc_quality = 100
};

/*
 * Sets year to the year in fs_time and then calculates the number of
 * 100th of seconds in the current year and saves that info in year_len.
 * fs_time contains the time set in the superblock in the root filesystem.
 * If the clock is started, it then checks if the time is valid
 * compared with the time in fs_time. If the clock is stopped, an
 * alert is printed and the time is temporary set to the time in fs_time.
 */

void
inittodr(time_t fs_time)
{
	int rv, deltat;
	struct timespec ts;

	rv = (*dep_call->cpu_clkread)(&ts, fs_time);
	if (rv != 0) {
		/* No useable information from system clock */
		ts.tv_sec = fs_time;
		ts.tv_nsec = 0;
		resettodr();
	} else {
		/* System clock OK, no warning if we don't want to. */
		deltat = ts.tv_sec - fs_time;

		if (deltat < 0)
			deltat = -deltat;
		if (deltat >= 2 * SEC_PER_DAY) {
			printf("clock has %s %d days",
			    ts.tv_sec < fs_time ? "lost" : "gained",
			    deltat / SEC_PER_DAY);
			rv = EINVAL;
		}
	}

	if (rv != 0)
		printf(" -- CHECK AND RESET THE DATE!\n");

	tc_setclock(&ts);
}

/*   
 * Resettodr restores the time of day hardware after a time change.
 */

void
resettodr()
{
	(*dep_call->cpu_clkwrite)();
}

/*
 * A delayloop that delays about the number of milliseconds that is
 * given as argument.
 */
void
delay(int i)
{
	asm ("1: sobgtr %0, 1b" : : "r" (dep_call->cpu_vups * i));
}

/*
 * On all VAXen there are a microsecond clock that should
 * be used for interval interrupts. We have to be wary of the few CPUs
 * which don't implement the ICR interval register.
 */
void
cpu_initclocks()
{
	switch (vax_boardtype) {
#ifdef VAX46
	/* VAXstation 4000/60: no ICR register but a specific timer */
	case VAX_BTYP_46:
	    {
		extern struct vs_cpu *ka46_cpu;

		vax_diagtmr_tc.tc_priv = ka46_cpu;
		vax_diagtmr_tc.tc_name = "KA46";
		tc_init(&vax_diagtmr_tc);
	    }
		break;
#endif
#ifdef VAX48
	/* VAXstation 4000/VLC: no ICR register but a specific timer */
	case VAX_BTYP_48:
	    {
		extern struct vs_cpu *ka48_cpu;

		vax_diagtmr_tc.tc_priv = ka48_cpu;
		vax_diagtmr_tc.tc_name = "KA48";
		tc_init(&vax_diagtmr_tc);
	    }
		break;
#endif
#ifdef VXT
	/* VXT2000: neither NICR nor ICR registers, no known other timer. */
	case VAX_BTYP_VXT:
		tc_init(&vax_vxt_tc);
		break;
#endif
	/* all other systems: NICR+ICR registers implementing a 1MHz timer. */
	default:
		tc_init(&vax_icr_tc);
		break;
	}

	evcount_attach(&clock_intrcnt, "clock", NULL);
	if (vax_boardtype != VAX_BTYP_VXT)
		mtpr(-tick, PR_NICR); /* Load in count register */
	mtpr(ICCS_ERR | ICCS_OFLOW | ICCS_INTENA | ICCS_RESET | ICCS_RUN,
	    PR_ICCS); /* Reset errors, start clock and enable interrupt */
}

void
icr_hardclock(struct clockframe *cf)
{
	icr_count += 10000;	/* tick */
	hardclock(cf);
}

/*
 * There are two types of real-time battery-backed up clocks on
 * VAX computers, one with a register that counts up every 1/100 second,
 * one with a clock chip that delivers time. For the register clock
 * we have a generic version, and for the chip clock there are 
 * support routines for time conversion.
 */
/*
 * Converts a year to corresponding number of ticks.
 */
int
yeartonum(int y)
{
	int n;

	for (n = 0, y -= 1; y > 69; y--)
		n += SECPERYEAR(y);
	return n;
}

/* 
 * Converts tick number to a year 70 ->
 */
int
numtoyear(int num)
{
	int y = 70, j;
	while (num >= (j = SECPERYEAR(y))) {
		y++;
		num -= j;
	}
	return y;
}

#if VAX650 || VAX660 || VAX670 || VAX680 || VAX53
/*
 * Reads the TODR register; returns 0 if a valid time has been found, EINVAL
 * otherwise.  The year is based on the argument year; the TODR doesn't hold
 * years.
 */
int
generic_clkread(struct timespec *ts, time_t base)
{
	uint32_t klocka = mfpr(PR_TODR);

	/*
	 * Sanity check.
	 */
	if (klocka < TODRBASE) {
		if (klocka == 0)
			printf("TODR stopped");
		else
			printf("TODR too small");
		return EINVAL;
	}

	ts->tv_sec =
	    yeartonum(numtoyear(base)) + (klocka - TODRBASE) / 100;
	ts->tv_nsec = 0;
	return 0;
}

/*
 * Takes the current system time and writes it to the TODR.
 */
void
generic_clkwrite()
{
	uint32_t tid = time_second, bastid;

	bastid = tid - yeartonum(numtoyear(tid));
	mtpr((bastid * 100) + TODRBASE, PR_TODR);
}
#endif

#if VAX630 || VAX410 || VAX43 || VAX46 || VAX48 || VAX49

volatile short *clk_page;	/* where the chip is mapped in virtual memory */
int	clk_adrshift;	/* how much to multiply the in-page address with */
int	clk_tweak;	/* Offset of time into word. */

#define	REGPEEK(off)	(clk_page[off << clk_adrshift] >> clk_tweak)
#define	REGPOKE(off, v)	(clk_page[off << clk_adrshift] = ((v) << clk_tweak))

int
chip_clkread(struct timespec *ts, time_t base)
{
	struct clock_ymdhms c;
	int timeout = 1<<15, s;

#ifdef DIAGNOSTIC
	if (clk_page == 0)
		panic("trying to use unset chip clock page");
#endif

	if ((REGPEEK(CSRD_OFF) & CSRD_VRT) == 0) {
		printf("WARNING: TOY clock not marked valid");
		return EINVAL;
	}
	while (REGPEEK(CSRA_OFF) & CSRA_UIP)
		if (--timeout == 0) {
			printf ("TOY clock timed out");
			return EINVAL;
		}

	s = splhigh();
	c.dt_year = ((u_char)REGPEEK(YR_OFF)) + 1970;
	c.dt_mon = REGPEEK(MON_OFF);
	c.dt_day = REGPEEK(DAY_OFF);
	c.dt_wday = REGPEEK(WDAY_OFF);
	c.dt_hour = REGPEEK(HR_OFF);
	c.dt_min = REGPEEK(MIN_OFF);
	c.dt_sec = REGPEEK(SEC_OFF);
	splx(s);

	ts->tv_sec = clock_ymdhms_to_secs(&c);
	ts->tv_nsec = 0;
	return 0;
}

void
chip_clkwrite()
{
	struct clock_ymdhms c;

#ifdef DIAGNOSTIC
	if (clk_page == 0)
		panic("trying to use unset chip clock page");
#endif

	REGPOKE(CSRB_OFF, CSRB_SET);

	clock_secs_to_ymdhms(time_second, &c);

	REGPOKE(YR_OFF, ((u_char)(c.dt_year - 1970)));
	REGPOKE(MON_OFF, c.dt_mon);
	REGPOKE(DAY_OFF, c.dt_day);
	REGPOKE(WDAY_OFF, c.dt_wday);
	REGPOKE(HR_OFF, c.dt_hour);
	REGPOKE(MIN_OFF, c.dt_min);
	REGPOKE(SEC_OFF, c.dt_sec);

	REGPOKE(CSRB_OFF, CSRB_DM|CSRB_24);
};
#endif

#if defined(VAX46) || defined(VAX48)
u_int
vax_diagtmr_get_tc(struct timecounter *tc)
{
	/*
	 * The diagnostic timer runs at about 1024kHz.
	 * The microsecond counter counts from 0 to 1023 inclusive (so it
	 * really is a 1/1024th millisecond counter) and increments the
	 * millisecond counter, which is a free-running 16 bit counter.
	 *
	 * To compensate for the timer not running at exactly 1024kHz,
	 * the microsecond counter is reset (to zero) every clock interrupt,
	 * i.e. every 10 millisecond.
	 *
	 * Without resetting the microsecond counter, experiments show that,
	 * on KA48, the millisecond counter increments of 960 in a second,
	 * instead of the expected 1000 (i.e. a 24/25 ratio).  But resetting
	 * the microsecond counter (which does not affect the millisecond
	 * counter value) ought to make it __slower__ - who can explain
	 * this behaviour?
	 *
	 * Because we reset the ``binary microsecond'' counter and can not
	 * afford time moving backwards, we only return the millisecond
	 * counter here.
	 */
	struct vs_cpu *vscpu;

	vscpu = (struct vs_cpu *)tc->tc_priv;
	return *(volatile uint16_t *)&vscpu->vc_diagtimm;
}
#endif

#ifdef VXT
u_int
vax_vxt_get_tc(struct timecounter *tc)
{
	return (u_int)clock_intrcnt.ec_count;
}
#endif

u_int
vax_icr_get_tc(struct timecounter *tc)
{
	return icr_count + (u_int)(tick + (int)mfpr(PR_ICR));
}
