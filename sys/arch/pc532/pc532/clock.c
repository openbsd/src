/*	$NetBSD: clock.c,v 1.11 1995/05/16 07:30:46 phil Exp $	*/

/*-
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *
 */

/*
 * Primitive clock interrupt routines.
 *
 * Improved by Phil Budne ... 10/17/94.
 * Pulled over code from i386/isa/clock.c (Matthias Pfaller 12/03/94).
 */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/systm.h>

#include <machine/icu.h>

void spinwait __P((int));

void
startrtclock()
{
	int timer = (ICU_CLK_HZ) / hz;

	/* Write the timer values to the ICU. */
	WR_ADR (unsigned short, ICU_ADR + HCSV, timer);
	WR_ADR (unsigned short, ICU_ADR + HCCV, timer);

	/* Establish interrupt vector */
	intr_establish(IR_CLK, hardclock, NULL, "clock", IPL_CLOCK,
		       FALLING_EDGE);
}

void
cpu_initclocks()
{
	/* enable clock interrupt */
	WR_ADR (unsigned char, ICU_ADR +CICTL, 0x30);
}

void
spinwait(int millisecs)
{
	DELAY(5000 * millisecs);
}

DELAY(n)
{
	volatile int N = (n);
	while (--N > 0)
		;
}

void
setstatclockrate(int dummy)
{
	printf ("setstatclockrate\n");
}


static int month[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

static int
yeartoday(year)
	int year;
{

	return((year % 4) ? 365 : 366);
}

int
hexdectodec(n)
	char n;
{

	return(((n >> 4) & 0x0F) * 10 + (n & 0x0F));
}

char
dectohexdec(n)
	int n;
{

	return((char)(((n / 10) << 4) & 0xF0) | ((n % 10) & 0x0F));
}

static int timeset;
struct rtc_st {
	unsigned char rtc_csec;
	unsigned char rtc_sec;
	unsigned char rtc_min;
	unsigned char rtc_hr;
	unsigned char rtc_dow;
	unsigned char rtc_dom;
	unsigned char rtc_mon;
	unsigned char rtc_yr;
};

/*
 * Initialize the time of day register, based on the time base which is, e.g.
 * from a filesystem.
 */
void
inittodr(base)
	time_t base;
{
        /*
         * We ignore the suggested time for now and go for the RTC
         * clock time stored in the CMOS RAM.
         */
	struct rtc_st rtclk;
	time_t n;
	int csec, sec, min, hr, dom, mon, yr;
	int i, days = 0;
	int s;
	extern int have_rtc;

	timeset = 1;
	if (!have_rtc) {
		time.tv_sec = base;
		return;
	}

	rw_rtc((unsigned char *)&rtclk, 0);

	csec = hexdectodec(rtclk.rtc_csec);
	sec  = hexdectodec(rtclk.rtc_sec);
	min  = hexdectodec(rtclk.rtc_min);
	hr   = hexdectodec(rtclk.rtc_hr);
	dom  = hexdectodec(rtclk.rtc_dom);
	mon  = hexdectodec(rtclk.rtc_mon);
	yr   = hexdectodec(rtclk.rtc_yr);
	yr   = (yr < 70) ? yr + 100 : yr;

	/*
	 * Check to see if it was really the rtc
	 * by checking for bad date info.
	 */
	if (sec > 59 || min > 59 || hr > 23 || dom > 31 || mon > 12) {
		printf("inittodr: No clock found\n");
		time.tv_sec = base;
		return;
	}

	n = sec + 60 * min + 3600 * hr;
	n += (dom - 1) * 3600 * 24;

	if (yeartoday(yr) == 366)
		month[1] = 29;
	for (i = mon - 2; i >= 0; i--)
		days += month[i];
	month[1] = 28;
	for (i = 70; i < yr; i++)
		days += yeartoday(i);
	n += days * 3600 * 24;

	n += tz.tz_minuteswest * 60;
	if (tz.tz_dsttime)
		n -= 3600;
	s = splclock();
	time.tv_sec = n;
	time.tv_usec = csec * 10000;
	splx(s);
}

/*
 * Reset the clock.
 */
void
resettodr()
{
	struct rtc_st rtclk;
	time_t n;
	int diff, i, j;
	int s;

	/*
	 * We might have been called by boot() due to a crash early
	 * on.  Don't reset the clock chip in this case.
	 */
	if (!timeset)
		return;

	diff = tz.tz_minuteswest * 60;
	if (tz.tz_dsttime)
		diff -= 3600;

	s = splclock();
	n = (time.tv_sec - diff) % (3600 * 24);   /* hrs+mins+secs */
	rtclk.rtc_csec = dectohexdec(time.tv_usec / 10000);
	rtclk.rtc_sec = dectohexdec(n%60);
	n /= 60;
	rtclk.rtc_min = dectohexdec(n%60);
	rtclk.rtc_hr = dectohexdec(n/60);

	n = (time.tv_sec - diff) / (3600 * 24);	/* days */
	splx(s);
	rtclk.rtc_dow = (n + 4) % 7;  /* 1/1/70 is Thursday */

	for (j = 1970, i = yeartoday(j); n >= i; j++, i = yeartoday(j))
		n -= i;

	rtclk.rtc_yr = dectohexdec(j - 1900);

	if (i == 366)
		month[1] = 29;
	for (i = 0; n >= month[i]; i++)
		n -= month[i];
	month[1] = 28;
	rtclk.rtc_mon = dectohexdec(++i);

	rtclk.rtc_dom = dectohexdec(++n);

	rw_rtc((unsigned char *)&rtclk, 1);
}
