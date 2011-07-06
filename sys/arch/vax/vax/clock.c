/*	$OpenBSD: clock.c,v 1.23 2011/07/06 18:32:59 miod Exp $	 */
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

#include <dev/clock_subr.h>

#include <machine/mtpr.h>
#include <machine/sid.h>
#include <machine/clock.h>
#include <machine/cpu.h>
#include <machine/uvax.h>

struct evcount clock_intrcnt;

/*
 * microtime() should return number of usecs in struct timeval.
 * We may get wrap-arounds, but that will be fixed with lasttime
 * check. This may fault within 10 msecs.
 */
void
microtime(tvp)
	struct timeval *tvp;
{
	int s, i;
	static struct timeval lasttime;

	s = splhigh();
	bcopy((caddr_t)&time, tvp, sizeof(struct timeval));

	switch (vax_boardtype) {
#ifdef VAX46
	case VAX_BTYP_46:
	    {
		extern struct vs_cpu *ka46_cpu;
		i = *(volatile int *)(&ka46_cpu->vc_diagtimu);
		i = (i >> 16) * 1024 + (i & 0x3ff);
	    }
		break;
#endif
#if defined(VAX48) || defined(VXT)
	case VAX_BTYP_48:
	case VAX_BTYP_VXT:
		/*
		 * PR_ICR doesn't exist.  We could use the vc_diagtimu
		 * counter, saving the value on the timer interrupt and
		 * subtracting that from the current value.
		 */
		i = 0;
		break;
#endif
	default:
		i = mfpr(PR_ICR);
		break;
	}
	i += tick; /* Get current interval count */
	tvp->tv_usec += i;
	while (tvp->tv_usec >= 1000000) {
		tvp->tv_sec++;
		tvp->tv_usec -= 1000000;
	}
	if (tvp->tv_sec == lasttime.tv_sec &&
	    tvp->tv_usec <= lasttime.tv_usec &&
	    (tvp->tv_usec = lasttime.tv_usec + 1) >= 1000000) {
		tvp->tv_sec++;
		tvp->tv_usec -= 1000000;
	}
	bcopy(tvp, &lasttime, sizeof(struct timeval));
	splx(s);
}

/*
 * Sets year to the year in fs_time and then calculates the number of
 * 100th of seconds in the current year and saves that info in year_len.
 * fs_time contains the time set in the superblock in the root filesystem.
 * If the clock is started, it then checks if the time is valid
 * compared with the time in fs_time. If the clock is stopped, an
 * alert is printed and the time is temporary set to the time in fs_time.
 */

void
inittodr(fs_time) 
	time_t fs_time;
{
	int rv, deltat;

	rv = (*dep_call->cpu_clkread) (fs_time);
	switch (rv) {

	case CLKREAD_BAD: /* No useable information from system clock */
		time.tv_sec = fs_time;
		resettodr();
		break;

	case CLKREAD_WARN: /* Just give the warning */
		break;

	default: /* System clock OK, no warning if we don't want to. */
		deltat = time.tv_sec - fs_time;

		if (deltat < 0)
			deltat = -deltat;
		if (deltat >= 2 * SEC_PER_DAY) {
			printf("Clock has %s %d days",
			    time.tv_sec < fs_time ? "lost" : "gained",
			    deltat / SEC_PER_DAY);
			rv = CLKREAD_WARN;
		}
		break;
	}

	if (rv < CLKREAD_OK)
		printf(" -- CHECK AND RESET THE DATE!\n");
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
delay(i)
	int i;
{
	asm ("1: sobgtr %0, 1b" : : "r" (dep_call->cpu_vups * i));
}

/*
 * On all VAXen there are a microsecond clock that should
 * be used for interval interrupts. Some CPUs don't use the ICR interval
 * register but it doesn't hurt to load it anyway.
 */
void
cpu_initclocks()
{
	if (vax_boardtype != VAX_BTYP_VXT)
		mtpr(-10000, PR_NICR); /* Load in count register */
	mtpr(0x800000d1, PR_ICCS); /* Start clock and enable interrupt */
	evcount_attach(&clock_intrcnt, "clock", NULL);
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
yeartonum(y)
	int y;
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
numtoyear(num)
	int num;
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
 * Reads the TODR register; returns a (probably) true tick value,
 * or CLKREAD_BAD if failed. The year is based on the argument
 * year; the TODR doesn't hold years.
 */
int
generic_clkread(base)
	time_t base;
{
	unsigned klocka = mfpr(PR_TODR);

	/*
	 * Sanity check.
	 */
	if (klocka < TODRBASE) {
		if (klocka == 0)
			printf("TODR stopped");
		else
			printf("TODR too small");
		return CLKREAD_BAD;
	}

	time.tv_sec = yeartonum(numtoyear(base)) + (klocka - TODRBASE) / 100;
	return CLKREAD_OK;
}

/*
 * Takes the current system time and writes it to the TODR.
 */
void
generic_clkwrite()
{
	unsigned tid = time.tv_sec, bastid;

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
chip_clkread(base)
	time_t base;
{
	struct clock_ymdhms c;
	int timeout = 1<<15, s;

#ifdef DIAGNOSTIC
	if (clk_page == 0)
		panic("trying to use unset chip clock page");
#endif

	if ((REGPEEK(CSRD_OFF) & CSRD_VRT) == 0) {
		printf("WARNING: TOY clock not marked valid");
		return CLKREAD_BAD;
	}
	while (REGPEEK(CSRA_OFF) & CSRA_UIP)
		if (--timeout == 0) {
			printf ("TOY clock timed out");
			return CLKREAD_BAD;
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

	time.tv_sec = clock_ymdhms_to_secs(&c);
	return CLKREAD_OK;
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

	clock_secs_to_ymdhms(time.tv_sec, &c);

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
