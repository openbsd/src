/*	$NetBSD: clock.c,v 1.9 1996/01/07 15:38:44 jonathan Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and Ralph Campbell.
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
 * from: Utah Hdr: clock.c 1.18 91/01/21
 *
 *	@(#)clock.c	8.1 (Berkeley) 6/10/93
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/autoconf.h>
#include <machine/machConst.h>
#include <pmax/pmax/clockreg.h>

/*
 * Machine-dependent clock routines.
 *
 * Startrtclock restarts the real-time clock, which provides
 * hardclock interrupts to kern_clock.c.
 *
 * Inittodr initializes the time of day hardware which provides
 * date functions.  Its primary function is to use some file
 * system information in case the hardare clock lost state.
 *
 * Resettodr restores the time of day hardware after a time change.
 */

volatile struct chiptime *Mach_clock_addr;


/* Some values for rates for the RTC interrupt */
#define RATE_32_HZ	0xB	/* 31.250 ms */
#define RATE_64_HZ	0xA	/* 15.625 ms */
#define RATE_128_HZ	0x9	/* 7.8125 ms */
#define RATE_256_HZ	0x8	/* 3.90625 ms */
#define RATE_512_HZ	0x7	/* 1953.125 us*/
#define RATE_1024_HZ	0x6	/* 976.562 us */
#define RATE_2048_HZ	0x5	/* 488.281 usecs/interrupt */

#undef SELECTED_RATE

/*
 * RTC interrupt rate: pick one of 64, 128, 256, 512, 1024, 2048.
 * The appropriate rate is machine-dependent, or even model-dependent.
 *
 * Unless a machine has a hardware free-running clock, the RTC interrupt
 * rate is an upper limit on gettimeofday(), context switch interval,
 * and generally the resolution of real-time.  The standard 4.4bsd pmax
 * RTC tick rate is 64Hz, which has low overhead but is ludicrous when
 * doing serious performance measurement.  For machines faster than 3100s,
 * 1024Hz gives millisecond resolution.   Alphas have an on-chip counter,
 * and at least some IOASIC Decstations have  a turbochannel cycle-counter,
 * either of which which can be interpolated between RTC interrupts, to
 * give resolution in ns or tens of ns.
 */

#ifndef RTC_HZ
#ifdef __mips__
#define RTC_HZ 64
#else
# error Kernel config parameter RTC_HZ not defined
#endif
#endif

/* Compute value to program clock with, given config parameter RTC_HZ */

#if (RTC_HZ == 128)
# define SELECTED_RATE RATE_128_HZ
#else	/* !128 Hz */
#if (RTC_HZ == 256)
# define SELECTED_RATE RATE_256_HZ
#else /*!256Hz*/
#if (RTC_HZ == 512)
# define SELECTED_RATE RATE_512_HZ
#else /*!512hz*/
#if (RTC_HZ == 1024)
# define SELECTED_RATE RATE_1024_HZ
#else /* !1024hz*/
# if (RTC_HZ == 64)
# define SELECTED_RATE RATE_64_HZ	/* 4.4bsd default on pmax */
# else
# error RTC interrupt rate RTC_HZ not recognised; must be a power of 2
#endif /*!64Hz*/
#endif /*!1024Hz*/
#endif /*!512 Hz*/
#endif /*!256 Hz*/
#endif /*!128Hz*/


/* Definition of the driver for autoconfig. */
static int	clockmatch __P((struct device *, void *, void *));
static void	clockattach __P((struct device *, struct device *, void *));
struct cfdriver clockcd = {
	NULL, "clock", clockmatch, clockattach, DV_DULL, sizeof(struct device),
};

static void	clock_startintr __P((void *));
static void	clock_stopintr __P((void *));

volatile struct chiptime *Mach_clock_addr;

static int
clockmatch(parent, cfdata, aux)
	struct device *parent;
	void *cfdata;
	void *aux;
{
	struct cfdata *cf = cfdata;
	struct confargs *ca = aux;
#ifdef notdef /* XXX */
	struct tc_cfloc *asic_locp = (struct asic_cfloc *)cf->cf_loc;
#endif
	register volatile struct chiptime *c;
	int vec, ipl;
	int nclocks;

	/* make sure that we're looking for this type of device. */
	if (!BUS_MATCHNAME(ca, "dallas_rtc"))
		return (0);

	/* All known decstations have a Dallas RTC */
#ifdef pmax
	nclocks = 1;
#else      
	/*See how many clocks this system has */	
	switch (hwrpb->rpb_type) {
	case ST_DEC_3000_500:
	case ST_DEC_3000_300:
		nclocks = 1;
		break;
	default:
		nclocks = 0;
	}
#endif

	/* if it can't have the one mentioned, reject it */
	if (cf->cf_unit >= nclocks)
		return (0);

	return (1);
}

static void
clockattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	register volatile struct chiptime *c;
	struct confargs *ca = aux;

	Mach_clock_addr = (struct chiptime *)
		MACH_PHYS_TO_UNCACHED(BUS_CVTADDR(ca));

#ifdef pmax
	printf("\n");
	return;
#endif

#ifndef pmax	/* Turn interrupts off, just in case. */
	
	c = Mach_clock_addr;
	c->regb = REGB_DATA_MODE | REGB_HOURS_FORMAT;
	MachEmptyWriteBuffer();
#endif

#ifdef notyet /*XXX*/ /*FIXME*/
	BUS_INTR_ESTABLISH(ca, (intr_handler_t)hardclock, NULL);
#endif
}

/*
 * Start the real-time and statistics clocks. Leave stathz 0 since there
 * are no other timers available.
 */
void
cpu_initclocks()
{
	register volatile struct chiptime *c;
	extern int tickadj;

	if (Mach_clock_addr == NULL)
		panic("cpu_initclocks: no clock to initialize");

	hz = RTC_HZ;		/* Clock Hz is a configuration parameter */
	tick = 1000000 / hz;	/* number of microseconds between interrupts */
	tickfix = 1000000 - (hz * tick);
	if (tickfix) {
		int ftp;

		ftp = min(ffs(tickfix), ffs(hz));
		tickfix >>= (ftp - 1);
		tickfixinterval = hz >> (ftp - 1);
        }

	c = Mach_clock_addr;
	c->rega = REGA_TIME_BASE | SELECTED_RATE;
	c->regb = REGB_PER_INT_ENA | REGB_DATA_MODE | REGB_HOURS_FORMAT;
	MachEmptyWriteBuffer();		/* Alpha needs this */
}

/*
 * We assume newhz is either stathz or profhz, and that neither will
 * change after being set up above.  Could recalculate intervals here
 * but that would be a drag.
 */
void
setstatclockrate(newhz)
	int newhz;
{
	/* nothing we can do */
}

/*
 * This is the amount to add to the value stored in the clock chip
 * to get the current year.
 *
 * Experimentation (and passing years) show that Decstation PROMS
 * assume the kernel uses the clock chip as a time-of-year clock.
 * The PROM assumes the clock is always set to 1972 or 1973, and contains
 * time-of-year in seconds.   The PROM checks the clock at boot time,
 * and if it's outside that range, sets it to 1972-01-01.
 */
#if 1		/* testing, until we write time-of-year code as aboce */
#define YR_OFFSET	24	/* good til dec 31, 1997 */
#define DAY_OFFSET	/*1*/ 0
#else
#define YR_OFFSET	22
#define DAY_OFFSET	1
#endif

#define	BASE_YEAR	1972

/*
 * This code is defunct after 2099.
 * Will Unix still be here then??
 */
static short dayyr[12] = {
	0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
};

/*
 * Initialize the time of day register, based on the time base which is, e.g.
 * from a filesystem.  Base provides the time to within six months,
 * and the time of year clock (if any) provides the rest.
 */
void
inittodr(base)
	time_t base;
{
	register volatile struct chiptime *c;
	register int days, yr;
	int sec, min, hour, day, mon, year;
	long deltat;
	int badbase, s;

	if (base < 5*SECYR) {
		printf("WARNING: preposterous time in file system");
		/* read the system clock anyway */
		base = 6*SECYR + 186*SECDAY + SECDAY/2;
		badbase = 1;
	} else
		badbase = 0;

	c = Mach_clock_addr;

	/*
	 * Don't read clock registers while they are being updated,
	 * and make sure we don't re-read the clock's registers
	 * too often while waiting.
	 */
	s = splclock();
	while ((c->rega & REGA_UIP) == 1) {
		splx(s);
		DELAY(10);
		s = splx();
	}

	sec = c->sec;
	min = c->min;
	hour = c->hour;
	day = c->day;
	mon = c->mon;
	year = c->year;

	splx(s);

#ifdef	DEBUG_CLOCK
	printf("inittodr(): todr hw yy/mm/dd= %d/%d/%d\n", year, mon, day);
#endif
	/* convert from PROM time-of-year convention to actual time */
	day  += DAY_OFFSET;
	year += YR_OFFSET;

	/* simple sanity checks */
	if (year < 70 || mon < 1 || mon > 12 || day < 1 || day > 31 ||
	    hour > 23 || min > 59 || sec > 59) {
		/*
		 * Believe the time in the file system for lack of
		 * anything better, resetting the TODR.
		 */
		time.tv_sec = base;
		if (!badbase) {
			printf("WARNING: preposterous clock chip time\n");
			resettodr();
		}
		goto bad;
	}
	days = 0;
	for (yr = 70; yr < year; yr++)
		days += LEAPYEAR(yr) ? 366 : 365;
	days += dayyr[mon - 1] + day - 1;
	if (LEAPYEAR(yr) && mon > 2)
		days++;
	/* now have days since Jan 1, 1970; the rest is easy... */
	time.tv_sec = days * SECDAY + hour * 3600 + min * 60 + sec;

	if (!badbase) {
		/*
		 * See if we gained/lost two or more days;
		 * if so, assume something is amiss.
		 */
		deltat = time.tv_sec - base;
		if (deltat < 0)
			deltat = -deltat;
		if (deltat < 2 * SECDAY)
			return;
		printf("WARNING: clock %s %d days",
		    time.tv_sec < base ? "lost" : "gained", deltat / SECDAY);
	}
bad:
	printf(" -- CHECK AND RESET THE DATE!\n");
}

/*
 * Reset the TODR based on the time value; used when the TODR
 * has a preposterous value and also when the time is reset
 * by the stime system call.  Also called when the TODR goes past
 * TODRZERO + 100*(SECYEAR+2*SECDAY) (e.g. on Jan 2 just after midnight)
 * to wrap the TODR around.
 */
void
resettodr()
{
	register volatile struct chiptime *c;
	register int t, t2;
	int sec, min, hour, day, dow, mon, year;
	int s;

	/* compute the day of week. */
	t2 = time.tv_sec / SECDAY;
	dow = (t2 + 4) % 7;	/* 1/1/1970 was thursday */

	/* compute the year */
	year = 69;
	while (t2 >= 0) {	/* whittle off years */
		t = t2;
		year++;
		t2 -= LEAPYEAR(year) ? 366 : 365;
	}

	/* t = month + day; separate */
	t2 = LEAPYEAR(year);
	for (mon = 1; mon < 12; mon++)
		if (t < dayyr[mon] + (t2 && mon > 1))
			break;

	day = t - dayyr[mon - 1] + 1;
	if (t2 && mon > 2)
		day--;

	/* the rest is easy */
	t = time.tv_sec % SECDAY;
	hour = t / 3600;
	t %= 3600;
	min = t / 60;
	sec = t % 60;

	c = Mach_clock_addr;

	/* convert to the  format the PROM uses */
	day  -= DAY_OFFSET;
 	year -= YR_OFFSET;

	s = splclock();
	t = c->regd;				/* reset VRT */
	c->regb = REGB_SET_TIME | REGB_DATA_MODE | REGB_HOURS_FORMAT;
	MachEmptyWriteBuffer();
	c->rega = 0x70;				/* reset time base */
	MachEmptyWriteBuffer();

	c->sec = sec;
	c->min = min;
	c->hour = hour;
	/*c->dayw = dow;*/
	c->day = day;
	c->mon = mon;
	c->year = year;
	MachEmptyWriteBuffer();

	c->rega = REGA_TIME_BASE | SELECTED_RATE;
	c->regb = REGB_PER_INT_ENA | REGB_DATA_MODE | REGB_HOURS_FORMAT;
	MachEmptyWriteBuffer();
	splx(s);
#ifdef	DEBUG_CLOCK
	printf("resettodr(): todr hw yy/mm/dd= %d/%d/%d\n", year, mon, day);
#endif

	c->nvram[48*4] |= 1;		/* Set PROM time-valid bit */
	MachEmptyWriteBuffer();
}

/*XXX*/
/*
 * Wait "n" microseconds.
 * (scsi code needs this).
*/
void
delay(n)
	int n;
{
	DELAY(n);
}
