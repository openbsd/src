/*	$OpenBSD: clock.c,v 1.8 1997/09/10 12:04:42 maja Exp $	 */
/*	$NetBSD: clock.c,v 1.20 1997/04/18 18:49:37 ragge Exp $	 */
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

static unsigned long year;     /*  start of current year in seconds */
static unsigned long year_len; /* length of current year in 100th of seconds */

/*
 * microtime() should return number of usecs in struct timeval.
 * We may get wrap-arounds, but that will be fixed with lasttime
 * check. This may fault within 10 msecs.
 */
void
microtime(tvp)
	struct timeval *tvp;
{
	u_int int_time, tmp_year;
	int s, i;
	static struct timeval lasttime;

	s = splhigh();
	int_time = mfpr(PR_TODR);

	asm ("movc3 %0,(%1),(%2)" 
		:
		: "r" (sizeof(struct timeval)),"r" (&time),"r"(tvp)
		:"r0","r1","r2","r3","r4","r5"); 

	i = mfpr(PR_ICR) + tick; /* Get current interval count */
	tvp->tv_usec += i;
	while (tvp->tv_usec > 1000000) {
		tvp->tv_sec++;
		tvp->tv_usec -= 1000000;
	}
	if (tvp->tv_sec == lasttime.tv_sec &&
	    tvp->tv_usec <= lasttime.tv_usec &&
	    (tvp->tv_usec = lasttime.tv_usec + 1) > 1000000) {
		tvp->tv_sec++;
		tvp->tv_usec -= 1000000;
	}
	bcopy(tvp, &lasttime, sizeof(struct timeval));
	if (int_time > year_len) {
		mtpr(mfpr(PR_TODR) - year_len, PR_TODR);
		year += year_len / 100;
		tmp_year = year / SEC_PER_DAY / 365 + 2;
		year_len = 100 * SEC_PER_DAY *
		    ((tmp_year % 4 && tmp_year != 32) ? 365 : 366);
	}
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
	int rv;

	rv = (*dep_call->cpu_clkread) (fs_time);
	switch (rv) {

	case CLKREAD_BAD: /* No useable information from system clock */
		time.tv_sec = fs_time;
		resettodr();
		break;

	case CLKREAD_WARN: /* Just give the warning */
		break;

	default: /* System clock OK, no warning if we don't want to. */
		if (time.tv_sec > fs_time + 3 * SEC_PER_DAY) {
			printf("Clock has gained %d days",
			    (time.tv_sec - fs_time) / SEC_PER_DAY);
			rv = CLKREAD_WARN;
		} else if (time.tv_sec + SEC_PER_DAY < fs_time) {
			printf("Clock has lost %d day(s)",
			    (fs_time - time.tv_sec) / SEC_PER_DAY);
			rv = CLKREAD_WARN;
		}
		break;
	}

	if (rv < CLKREAD_OK)
		printf(" - CHECK AND RESET THE DATE.\n");
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

#if VAX750 || VAX780 || VAX8200 || VAX8600 || VAX8800
/*
 * On most VAXen there are a microsecond clock that should
 * be used for interval interrupts. Have a generic version here.
 */
void
generic_clock()
{
	mtpr(-10000, PR_NICR); /* Load in count register */
	mtpr(0x800000d1, PR_ICCS); /* Start clock and enable interrupt */
}
#endif

#if VAX650 || VAX630 || VAX410 || VAX43
/*
 * Most microvaxen don't have a interval count register.
 */
void
no_nicr_clock()
{
	mtpr(0x800000d1, PR_ICCS); /* Start clock and enable interrupt */
}
#endif

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
	while(num >= (j = SECPERYEAR(y))) {
		y++;
		num -= j;
	}
	return y;
}

#if VAX750 || VAX780 || VAX8600 || VAX650
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

#if VAX630 || VAX410 || VAX43 || VAX8200

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
	c.dt_year = REGPEEK(YR_OFF) + 1970;
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

	REGPOKE(YR_OFF, c.dt_year - 1970);
	REGPOKE(MON_OFF, c.dt_mon);
	REGPOKE(DAY_OFF, c.dt_day);
	REGPOKE(WDAY_OFF, c.dt_wday);
	REGPOKE(HR_OFF, c.dt_hour);
	REGPOKE(MIN_OFF, c.dt_min);
	REGPOKE(SEC_OFF, c.dt_sec);

	REGPOKE(CSRB_OFF, CSRB_DM|CSRB_24);
};
#endif
