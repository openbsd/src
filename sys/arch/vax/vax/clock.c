/*	$NetBSD: clock.c,v 1.18 1996/10/13 03:35:33 christos Exp $	 */
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

#include <machine/mtpr.h>
#include <machine/sid.h>
#include <machine/uvax.h>
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

	rv = (*cpu_calls[vax_cputype].cpu_clkread) (fs_time);
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
	(*cpu_calls[vax_cputype].cpu_clkwrite)();
}
/*
 * A delayloop that delays about the number of milliseconds that is
 * given as argument.
 */
void
delay(i)
	int i;
{
	int	mul;

	switch (vax_cputype) {
#if VAX750 || VAX630 || VAX410
	case VAX_750:
	case VAX_78032:
		mul = 1; /* <= 1 VUPS */
		break;
#endif
#if VAX780 || VAX8200
	case VAX_780:
	case VAX_8200:
		mul = 2; /* <= 2 VUPS */
		break;
#endif
#if VAX650
	case VAX_650:
		mul = 3; /* <= 3 VUPS */
		break;
#endif
	default:	/* Would be enough... */
	case VAX_8600:
		mul = 6; /* <= 6 VUPS */
		break;
	}
	asm ("1: sobgtr %0, 1b" : : "r" (mul * i));
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

static int dagar[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

/*
 * Returns the number of days in month based on the current year.
 */
int
daysinmonth(m, y)
	int m, y;
{
	if (m == 2 && IS_LEAPYEAR(y))
		return 29;
	else
		return dagar[m - 1];
}

/*
 * Converts chiptime (year/month/day/hour/min/sek) and returns ticks.
 */
long
chiptotime(c)
	struct chiptime *c;
{
	int num, i;

	num = c->sec;
	num += c->min * SEC_PER_MIN;
	num += c->hour * SEC_PER_HOUR;
	num += (c->day - 1) * SEC_PER_DAY;
	for(i = c->mon - 1; i > 0; i--)
		num += daysinmonth(i, c->year) * SEC_PER_DAY;
	num += yeartonum(c->year);

	return num;
}

/*
 * Reads the system time and puts it into a chiptime struct.
 */
void
timetochip(c)
	struct chiptime *c;
{
	int tid = time.tv_sec, i, j;

	c->year = numtoyear(tid);
	tid -= yeartonum(c->year);

	c->mon = 1;
	while(tid >= (j = (daysinmonth(c->mon, c->year) * SEC_PER_DAY))) {
		c->mon++;
		tid -= j;
	}
	c->day = (tid / SEC_PER_DAY) + 1;
	tid %= SEC_PER_DAY;

	c->hour = tid / SEC_PER_HOUR;
	tid %= SEC_PER_HOUR;

	c->min = tid / SEC_PER_MIN;
	c->sec = tid % SEC_PER_MIN;
}
#endif
