/*	$OpenBSD: clock.c,v 1.6 2000/03/23 04:12:48 rahnds Exp $	*/
/*	$NetBSD: clock.c,v 1.1 1996/09/30 16:34:40 ws Exp $	*/

/*
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/kernel.h>

#include <machine/pio.h>

#if 0
#include <powerpc/pci/mpc106reg.h>
#endif

void resettodr();
/*
 * Initially we assume a processor with a bus frequency of 12.5 MHz.
 */
static u_long ticks_per_sec = 3125000;
static u_long ns_per_tick = 320;
static long ticks_per_intr;
static volatile u_long lasttb;

/*
 * BCD to decimal and decimal to BCD.
 */
#define FROMBCD(x)      (((x) >> 4) * 10 + ((x) & 0xf))
#define TOBCD(x)        (((x) / 10 * 16) + ((x) % 10))

#define SECDAY          (24 * 60 * 60)
#define SECYR           (SECDAY * 365)
#define LEAPYEAR(y)     (((y) & 3) == 0)
#define YEAR0		1900

typedef int (clock_read_t)(int *sec, int *min, int *hour, int *day,
         int *mon, int *yr);

int power4e_getclock(int *, int *, int *, int *, int *, int *);

clock_read_t *clock_read = NULL;

static u_long
chiptotime(int sec, int min, int hour, int day, int mon, int year);

/*
 * For now we let the machine run with boot time, not changing the clock
 * at inittodr at all.
 *
 * We might continue to do this due to setting up the real wall clock with
 * a user level utility in the future.
 */

/* ARGSUSED */
void
inittodr(base)
	time_t base;
{
	int sec, min, hour, day, mon, year;

	int badbase = 0, waszero = base == 0;

        if (base < 5 * SECYR) {
                /*
                 * If base is 0, assume filesystem time is just unknown
                 * instead of preposterous. Don't bark.
                 */
                if (base != 0)
                        printf("WARNING: preposterous time in file system\n");
                /* not going to use it anyway, if the chip is readable */
                base = 21*SECYR + 186*SECDAY + SECDAY/2;
                badbase = 1;
        }

	if (clock_read != NULL ) {
		(*clock_read)( &sec, &min, &hour, &day, &mon, &year);
	} else {
		/* force failure on chiptotime */
		mon = 0;
	} 
	if ((time.tv_sec = chiptotime(sec, min, hour, day, mon, year)) == 0) {
		printf("WARNING: unable to get date/time");
		/*
		 * Believe the time in the file system for lack of
		 * anything better, resetting the clock.
		 */
		time.tv_sec = base;
		if (!badbase)
			resettodr();
	} else {
		int deltat = time.tv_sec - base;

		if (deltat < 0)
			deltat = -deltat;
		if (waszero || deltat < 2 * SECDAY)
			return;
		printf("WARNING: clock %s %d days",
		    time.tv_sec < base ? "lost" : "gained", deltat / SECDAY);
	}
	printf(" -- CHECK AND RESET THE DATE!\n");
}

/*
 * This code is defunct after 2068.
 * Will Unix still be here then??
 */
const short dayyr[12] =
    { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };

static u_long
chiptotime(sec, min, hour, day, mon, year)
        int sec, min, hour, day, mon, year;
{
	int days, yr;
		
	sec = FROMBCD(sec);
	min = FROMBCD(min);
	hour = FROMBCD(hour);
	day = FROMBCD(day);
	mon = FROMBCD(mon);
	year = FROMBCD(year) + YEAR0;
		
	/* simple sanity checks */
	if (year < 1970 || mon < 1 || mon > 12 || day < 1 || day > 31)
		return (0);
	days = 0;
	for (yr = 1970; yr < year; yr++)
		days += LEAPYEAR(yr) ? 366 : 365;
	days += dayyr[mon - 1] + day - 1;
	if (LEAPYEAR(yr) && mon > 2)
		days++;
	/* now have days since Jan 1, 1970; the rest is easy... */
	return (days * SECDAY + hour * 3600 + min * 60 + sec);
}


/*
 * Similar to the above
 */
void
resettodr()
{
}

void
decr_intr(frame)
	struct clockframe *frame;
{
	int msr;
	u_long tb;
	long tick;
	int nticks;
	int pri;

	/*
	 * Check whether we are initialized.
	 */
	if (!ticks_per_intr)
		return;

	/*
	 * Based on the actual time delay since the last decrementer reload,
	 * we arrange for earlier interrupt next time.
	 */
	asm ("mftb %0; mfdec %1" : "=r"(tb), "=r"(tick));
	for (nticks = 0; tick < 0; nticks++)
		tick += ticks_per_intr;
	asm volatile ("mtdec %0" :: "r"(tick));
	/*
	 * lasttb is used during microtime. Set it to the virtual
	 * start of this tick interval.
	 */
	lasttb = tb + tick - ticks_per_intr;

	pri = splclock();

	if (pri & SPL_CLOCK)
		tickspending += nticks;
	else {
		/*
		 * Reenable interrupts
		 */
		asm volatile ("mfmsr %0; ori %0, %0, %1; mtmsr %0"
			      : "=r"(msr) : "K"(PSL_EE));
		
		/*
		 * Do standard timer interrupt stuff.
		 * Do softclock stuff only on the last iteration.
		 */
		frame->pri = pri | SINT_CLOCK;
		while (--nticks > 0)
			hardclock(frame);
		frame->pri = pri;
		hardclock(frame);
	}
	splx(pri);
}

void
cpu_initclocks()
{
	int msr, scratch;
	asm volatile ("mfmsr %0; andi. %1, %0, %2; mtmsr %1"
		      : "=r"(msr), "=r"(scratch) : "K"((u_short)~PSL_EE));
	asm volatile ("mftb %0" : "=r"(lasttb));
	asm volatile ("mtdec %0" :: "r"(ticks_per_intr));
	asm volatile ("mtmsr %0" :: "r"(msr));
}

void
calc_delayconst()
{
	int qhandle, phandle;
	char name[32];
	int msr, scratch;
	
	/*
	 * Get this info during autoconf?				XXX
	 */
	for (qhandle = OF_peer(0); qhandle; qhandle = phandle) {
		if (OF_getprop(qhandle, "device_type", name, sizeof name) >= 0
		    && !strcmp(name, "cpu")
		    && OF_getprop(qhandle, "timebase-frequency",
				  &ticks_per_sec, sizeof ticks_per_sec) >= 0) {
			/*
			 * Should check for correct CPU here?		XXX
			 */
			asm volatile ("mfmsr %0; andi. %1, %0, %2; mtmsr %1"
				      : "=r"(msr), "=r"(scratch) : "K"((u_short)~PSL_EE));
			ns_per_tick = 1000000000 / ticks_per_sec;
			ticks_per_intr = ticks_per_sec / hz;
			asm volatile ("mtmsr %0" :: "r"(msr));
			break;
		}
		if (phandle = OF_child(qhandle))
			continue;
		while (qhandle) {
			if (phandle = OF_peer(qhandle))
				break;
			qhandle = OF_parent(qhandle);
		}
	}
	if (!phandle)
		panic("no cpu node");
}

static inline u_quad_t
mftb()
{
	u_long scratch;
	u_quad_t tb;
	
	asm ("1: mftbu %0; mftb %0+1; mftbu %1; cmpw 0,%0,%1; bne 1b"
	     : "=r"(tb), "=r"(scratch));
	return tb;
}

/*
 * Fill in *tvp with current time with microsecond resolution.
 */
void
microtime(tvp)
	struct timeval *tvp;
{
	u_long tb;
	u_long ticks;
	int msr, scratch;
	
	asm volatile ("mfmsr %0; andi. %1,%0,%2; mtmsr %1"
		      : "=r"(msr), "=r"(scratch) : "K"((u_short)~PSL_EE));
	asm ("mftb %0" : "=r"(tb));
	ticks = (tb - lasttb) * ns_per_tick;
	*tvp = time;
	asm volatile ("mtmsr %0" :: "r"(msr));
	ticks /= 1000;
	tvp->tv_usec += ticks;
	while (tvp->tv_usec > 1000000) {
		tvp->tv_usec -= 1000000;
		tvp->tv_sec++;
	}
}

/*
 * Wait for about n microseconds (us) (at least!).
 */
void
delay(n)
	unsigned n;
{
	u_quad_t tb;
	u_long tbh, tbl, scratch;
	
	tb = mftb();
	tb += (n * 1000 + ns_per_tick - 1) / ns_per_tick;
	tbh = tb >> 32;
	tbl = tb;
	asm ("1: mftbu %0; cmplw %0,%1; blt 1b; bgt 2f;"
	     " mftb %0; cmplw %0,%2; blt 1b; 2:"
	     :: "r"(scratch), "r"(tbh), "r"(tbl));

	tb = mftb();
}

/*
 * Nothing to do.
 */
void
setstatclockrate(arg)
	int arg;
{
	/* Do nothing */
}


#if 0
int
power4e_getclock(sec, min, hour, day, mon, year)
	int *sec;
	int *min;
	int *hour;
	int *day;
	int *mon;
	int *year;
{
	int clkbase = MPC106_V_PCI_MEM_SPACE + 0x000f1ff8;

	outb(clkbase, inb(clkbase) | 0x40);	/* stop update */
	*sec = inb(clkbase + 1);
	*min = inb(clkbase + 2);
	*hour = inb(clkbase + 3);
	*day = inb(clkbase + 5);
	*mon = inb(clkbase + 6);
	*year = inb(clkbase + 7);
	outb(clkbase, inb(clkbase) & ~0x40);
	return(0);
}
#endif
