/* $NetBSD: clock.c,v 1.4 1996/04/19 19:39:17 mark Exp $ */

/*
 * Copyright (c) 1994-1996 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
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
 *	This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * clock.c
 *
 * Timer related machine specific code
 *
 * Created      : 29/09/94
 */

/* Include header files */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/time.h>

#include <machine/katelib.h>
#include <machine/iomd.h>
#include <machine/irqhandler.h>
#include <machine/cpu.h>
#include <machine/rtc.h>

#define TIMER0_COUNT 20000		/* 100Hz */
#define TIMER_FREQUENCY 20000000	/* 2MHz clock */
#define TICKS_PER_MICROSECOND (TIMER_FREQUENCY / 10000000)

static irqhandler_t clockirq;
static irqhandler_t statclockirq;


/*
 * int clockhandler(struct clockframe *frame)
 *
 * Function called by timer 0 interrupts. This just calls
 * hardclock(). Eventually the irqhandler can call hardclock() directly
 * but for now we use this function so that we can debug IRQ's
 */
 
int
clockhandler(frame)
	struct clockframe *frame;
{
	hardclock(frame);
	return(1);
}


/*
 * int statclockhandler(struct clockframe *frame)
 *
 * Function called by timer 1 interrupts. This just calls
 * statclock(). Eventually the irqhandler can call statclock() directly
 * but for now we use this function so that we can debug IRQ's
 */
 
int
statclockhandler(frame)
	struct clockframe *frame;
{
	statclock(frame);
	return(1);
}


/*
 * void setstatclockrate(int hz)
 *
 * Set the stat clock rate. The stat clock uses timer1
 */

void
setstatclockrate(hz)
	int hz;
{
	int count;
    
	count = TIMER_FREQUENCY / hz;
    
	printf("Setting statclock to %dHz (%d ticks)\n", hz, count);
    
	WriteByte(IOMD_T1LOW,  (count >> 0) & 0xff);
	WriteByte(IOMD_T1HIGH, (count >> 8) & 0xff);

/* reload the counter */

	WriteByte(IOMD_T1GO, 0);
}


/*
 * void cpu_initclocks(void)
 *
 * Initialise the clocks.
 * This sets up the two timers in the IOMD and installs the IRQ handlers
 *
 * NOTE: Currently only timer 0 is setup and the IRQ handler is not installed
 */
 
void
cpu_initclocks()
{
/*
 * Load timer 0 with count down value
 * This timer generates 100Hz interrupts for the system clock
 */

	printf("clock: hz=%d stathz = %d profhz = %d\n", hz, stathz, profhz);

	WriteByte(IOMD_T0LOW,  (TIMER0_COUNT >> 0) & 0xff);
	WriteByte(IOMD_T0HIGH, (TIMER0_COUNT >> 8) & 0xff);

/* reload the counter */

	WriteByte(IOMD_T0GO, 0);

	clockirq.ih_func = clockhandler;
	clockirq.ih_arg = 0;
	clockirq.ih_level = IPL_CLOCK;
	clockirq.ih_name = "TMR0 hard clk";
	if (irq_claim(IRQ_TIMER0, &clockirq) == -1)
		panic("Cannot installer timer 0 IRQ handler");

	if (stathz) {
		setstatclockrate(stathz);
        
		statclockirq.ih_func = statclockhandler;
		statclockirq.ih_arg = 0;
		statclockirq.ih_level = IPL_CLOCK;
		if (irq_claim(IRQ_TIMER1, &clockirq) == -1)
			panic("Cannot installer timer 1 IRQ handler");
	}
}


/*
 * void microtime(struct timeval *tvp)
 *
 * Fill in the specified timeval struct with the current time
 * accurate to the microsecond.
 */

void
microtime(tvp)
	struct timeval *tvp;
{
	int s;
	int tm;
	int deltatm;
	static int oldtm;
	static struct timeval oldtv;

	s = splhigh();

/*
 * Latch the current value of the timer and then read it. This garentees
 * an atmoic reading of the time.
 */
 
	WriteByte(IOMD_T0LATCH, 0);
	tm = ReadByte(IOMD_T0LOW) + (ReadByte(IOMD_T0HIGH) << 8);
	deltatm = tm - oldtm;
	if (deltatm < 0) deltatm += TIMER0_COUNT;
	if (deltatm < 0) {
		printf("opps deltatm < 0 tm=%d oldtm=%d deltatm=%d\n",
		    tm, oldtm, deltatm);
	}
	oldtm = tm;

/* Fill in the timeval struct */

	*tvp = time;    
	tvp->tv_usec += (deltatm / TICKS_PER_MICROSECOND);

/* Make sure the micro seconds don't overflow. */

	while (tvp->tv_usec > 1000000) {
		tvp->tv_usec -= 1000000;
		++tvp->tv_sec;
	}

/* Make sure the time has advanced. */

	if (tvp->tv_sec == oldtv.tv_sec &&
	    tvp->tv_usec <= oldtv.tv_usec) {
		tvp->tv_usec = oldtv.tv_usec + 1;
		if (tvp->tv_usec > 1000000) {
			tvp->tv_usec -= 1000000;
			++tvp->tv_sec;
		}
	}
	    

	oldtv = *tvp;
	(void)splx(s);		
}


void
need_proftick(p)
	struct proc *p;
{
}


static inline int
yeartoday(year)
	int year;
{
	return((year % 4) ? 365 : 366);
}

                 
static int month[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
static int timeset = 0;

#define SECPERDAY	(24*60*60)
#define SECPERNYEAR	(365*SECPERDAY)
#define SECPER4YEARS	(4*SECPERNYEAR+SECPERDAY)
#define EPOCHYEAR	1970

/*
 * Write back the time of day to the rtc
 */

void
resettodr()
{
	int s;
	time_t year, mon, day, hour, min, sec;
	rtc_t rtc;

	if (!timeset)
		return;

	sec = time.tv_sec;
	year = (sec / SECPER4YEARS) * 4;
	sec %= SECPER4YEARS;

	/* year now hold the number of years rounded down 4 */

	while (sec > (yeartoday(EPOCHYEAR+year) * SECPERDAY)) {
		sec -= yeartoday(EPOCHYEAR+year)*SECPERDAY;
		year++;
	}

	/* year is now a correct offset from the EPOCHYEAR */

	year+=EPOCHYEAR;
	mon=0;
	if (yeartoday(year) == 366)
		month[1]=29;
	else
		month[1]=28;
	while ((sec/SECPERDAY) > month[mon]) {
		sec -= month[mon]*SECPERDAY;
		mon++;
	}

	day = sec / SECPERDAY;
	sec %= SECPERDAY;
	hour = sec / 3600;
	sec %= 3600;
	min = sec / 60;
	sec %= 60;
	rtc.rtc_cen = year / 100;
	rtc.rtc_year = year % 100;
	rtc.rtc_mon = mon+1;
	rtc.rtc_day = day+1;
	rtc.rtc_hour = hour;
	rtc.rtc_min = min;
	rtc.rtc_sec = sec;
	rtc.rtc_centi =
	rtc.rtc_micro = 0;

/*
	printf("resettod: %d/%d/%d%d %d:%d:%d\n", rtc.rtc_day,
	    rtc.rtc_mon, rtc.rtc_cen, rtc.rtc_year, rtc.rtc_hour,
	    rtc.rtc_min, rtc.rtc_sec);
*/

	s = splclock();
	rtc_write(&rtc);
	(void)splx(s);
}

/*
 * Initialise the time of day register, based on the time base which is, e.g.
 * from a filesystem.
 */

void
inittodr(base)
	time_t base;
{
	time_t n;
	int i, days = 0;
	int s;
	int year;
	rtc_t rtc;

/*
 * We ignore the suggested time for now and go for the RTC
 * clock time stored in the CMOS RAM.
 */

	s = splclock();
	if (rtc_read(&rtc) == 0) {
		(void)splx(s);
		return;
	}

	(void)splx(s);

	n = rtc.rtc_sec + 60 * rtc.rtc_min + 3600 * rtc.rtc_hour;
	n += (rtc.rtc_day - 1) * 3600 * 24;
	year = (rtc.rtc_year + rtc.rtc_cen * 100) - 1900;

	if (yeartoday(year) == 366)
		month[1] = 29;
	for (i = rtc.rtc_mon - 2; i >= 0; i--)
		days += month[i];
	month[1] = 28;

	for (i = 70; i < year; i++)
		days += yeartoday(i);

	n += days * 3600 * 24;

	n += tz.tz_minuteswest * 60;
	if (tz.tz_dsttime)
		n -= 3600;

	time.tv_sec = n;
	time.tv_usec = 0;

/* timeset is used to ensure the time is valid before a resettodr() */

	timeset = 1;

/* If the base was 0 then keep quiet */

	if (base) {
		printf("inittodr: %02d:%02d:%02d.%02d%02d %02d/%02d/%02d%02d\n",
		    rtc.rtc_hour, rtc.rtc_min, rtc.rtc_sec, rtc.rtc_centi,
		    rtc.rtc_micro, rtc.rtc_day, rtc.rtc_mon, rtc.rtc_cen,
		    rtc.rtc_year);

		if (n > base + 60) {
			days = (n - base) / SECPERDAY;
			printf("Clock has gained %d day%c %ld hours %ld minutes %ld secs\n",
			    days, ((days == 1) ? 0 : 's'), ((n - base)  / 3600) % 24,
			    ((n - base) / 60) % 60, (n - base) % 60);
		}
	}
}  

/* End of clock.c */
