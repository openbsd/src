/*	$NetBSD: clock.c,v 1.10 1995/02/20 00:53:42 chopps Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 * from: Utah $Hdr: clock.c 1.18 91/01/21$
 *
 *	@(#)clock.c	7.6 (Berkeley) 5/7/91
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <machine/psl.h>
#include <machine/cpu.h>
#include <amiga/amiga/device.h>
#include <amiga/amiga/custom.h>
#include <amiga/amiga/cia.h>
#include <amiga/dev/rtc.h>
#include <amiga/dev/zbusvar.h>

#if defined(PROF) && defined(PROFTIMER)
#include <sys/PROF.h>
#endif

/* the clocks run at NTSC: 715.909kHz or PAL: 709.379kHz. 
   We're using a 100 Hz clock. */

#define CLK_INTERVAL amiga_clk_interval
int amiga_clk_interval;
int eclockfreq;

/*
 * Machine-dependent clock routines.
 *
 * Startrtclock restarts the real-time clock, which provides
 * hardclock interrupts to kern_clock.c.
 *
 * Inittodr initializes the time of day hardware which provides
 * date functions.
 *
 * Resettodr restores the time of day hardware after a time change.
 *
 * A note on the real-time clock:
 * We actually load the clock with CLK_INTERVAL-1 instead of CLK_INTERVAL.
 * This is because the counter decrements to zero after N+1 enabled clock
 * periods where N is the value loaded into the counter.
 */

int clockmatch __P((struct device *, struct cfdata *, void *));
void clockattach __P((struct device *, struct device *, void *));

struct cfdriver clockcd = {
	NULL, "clock", (cfmatch_t)clockmatch, clockattach, 
	DV_DULL, sizeof(struct device), NULL, 0 };

int
clockmatch(pdp, cfp, auxp)
	struct device *pdp;
	struct cfdata *cfp;
	void *auxp;
{
	if (matchname("clock", auxp))
		return(1);
	return(0);
}

/*
 * Start the real-time clock.
 */
void
clockattach(pdp, dp, auxp)
	struct device *pdp, *dp;
	void *auxp;
{
	unsigned short interval;

	if (eclockfreq == 0)
		eclockfreq = 715909;	/* guess NTSC */
		
	CLK_INTERVAL = (eclockfreq / 100);

	printf(": system hz %d hardware hz %d\n", hz, eclockfreq);

	/*
	 * stop timer A 
	 */
	ciab.cra = ciab.cra & 0xc0;
	ciab.icr = 1 << 0;		/* disable timer A interrupt */
	interval = ciab.icr;		/* and make sure it's clear */

	/*
	 * load interval into registers.
         * the clocks run at NTSC: 715.909kHz or PAL: 709.379kHz
	 * supprort for PAL WHEN?!?! XXX
	 */
	interval = CLK_INTERVAL - 1;

	/*
	 * order of setting is important !
	 */
	ciab.talo = interval & 0xff;
	ciab.tahi = interval >> 8;
}

void
cpu_initclocks()
{
	/*
	 * enable interrupts for timer A
	 */
	ciab.icr = (1<<7) | (1<<0);

	/*
	 * start timer A in continuous shot mode
	 */
	ciab.cra = (ciab.cra & 0xc0) | 1;
  
	/*
	 * and globally enable interrupts for ciab
	 */
	custom.intena = INTF_SETCLR | INTF_EXTER;
}

setstatclockrate(hz)
	int hz;
{
}

/*
 * Returns number of usec since last recorded clock "tick"
 * (i.e. clock interrupt).
 */
clkread()
{
	u_char hi, hi2, lo;
	u_int interval;
   
	hi  = ciab.tahi;
	lo  = ciab.talo;
	hi2 = ciab.tahi;
	if (hi != hi2) {
		lo = ciab.talo;
		hi = hi2;
	}

	interval = (CLK_INTERVAL - 1) - ((hi<<8) | lo);
   
	/*
	 * should read ICR and if there's an int pending, adjust interval.
	 * However, * since reading ICR clears the interrupt, we'd lose a
	 * hardclock int, and * this is not tolerable.
	 */

	return((interval * tick) / CLK_INTERVAL);
}

u_int micspertick;

/*
 * we set up as much of the CIAa as possible
 * as all access to chip memory are very slow.
 */
void
setmicspertick()
{
	micspertick = (1000000ULL << 20) / 715909;

	/*
	 * disable interrupts (just in case.)
	 */
	ciaa.icr = 0x3;

	/*
	 * stop both timers if not already
	 */
	ciaa.cra &= ~1;
	ciaa.crb &= ~1;

	/*
	 * set timer B in "count timer A underflows" mode
	 * set tiemr A in one-shot mode
	 */
	ciaa.crb = (ciaa.crb & 0x80) | 0x48;
	ciaa.cra = (ciaa.cra & 0xc0) | 0x08;
}

/*
 * this function assumes that on any entry beyond the first
 * the following condintions exist:
 * Interrupts for Timers A and B are disabled.
 * Timers A and B are stoped. 
 * Timers A and B are in one-shot mode with B counting timer A underflows
 *
 */
void
delay(mic)
	int mic;
{
	u_int temp;
	int s;

	if (micspertick == 0)
		setmicspertick();

	if (mic <= 1)
		return;

	/*
	 * basically this is going to do an integer
	 * usec / (1000000 / 715909) with no loss of
	 * precision
	 */
	temp = mic >> 12;
	asm("divul %3,%1:%0" : "=d" (temp) : "d" (mic >> 12), "0" (mic << 20),
	    "d" (micspertick));

	if ((temp & 0xffff0000) > 0x10000) {
		mic = (temp >> 16) - 1;
		temp &= 0xffff;

		/*
		 * set timer A in continous mode
		 */
		ciaa.cra = (ciaa.cra & 0xc0) | 0x00;
	
		/*
		 * latch/load/start "counts of timer A underflows" in B
		 */
		ciaa.tblo = mic & 0xff;
		ciaa.tbhi = mic >> 8;
		
		/*
		 * timer A latches 0xffff
		 * and start it.
		 */
		ciaa.talo = 0xff;
		ciaa.tahi = 0xff;
		ciaa.cra |= 1;

		while (ciaa.crb & 1)
			;

		/* 
		 * stop timer A 
		 */
		ciaa.cra &= ~1;

		/*
		 * set timer A in one shot mode
		 */
		ciaa.cra = (ciaa.cra & 0xc0) | 0x08;
	} else if ((temp & 0xffff0000) == 0x10000) {
		temp &= 0xffff;

		/*
		 * timer A is in one shot latch/load/start 1 full turn
		 */
		ciaa.talo = 0xff;
		ciaa.tahi = 0xff;
		while (ciaa.cra & 1)
			;
	}
	if (temp < 1)
		return;

	/*
	 * temp is now residual ammount, latch/load/start it.
	 */
	ciaa.talo = temp & 0xff;
	ciaa.tahi = temp >> 8;
	while (ciaa.cra & 1)
		;
}

/*
 * Needs to be calibrated for use, its way off most of the time
 */
void
DELAY(mic)
	int mic;
{
	u_long n;
	short hpos;

	/*
	 * this function uses HSync pulses as base units. The custom chips 
	 * display only deals with 31.6kHz/2 refresh, this gives us a
	 * resolution of 1/15800 s, which is ~63us (add some fuzz so we really
	 * wait awhile, even if using small timeouts)
	 */
	n = mic/63 + 2;
	do {
		hpos = custom.vhposr & 0xff00;
		while (hpos == (custom.vhposr & 0xff00))
			;
	} while (n--);
}

#if notyet

/* implement this later. I'd suggest using both timers in CIA-A, they're
   not yet used. */

#include "clock.h"
#if NCLOCK > 0
/*
 * /dev/clock: mappable high resolution timer.
 *
 * This code implements a 32-bit recycling counter (with a 4 usec period)
 * using timers 2 & 3 on the 6840 clock chip.  The counter can be mapped
 * RO into a user's address space to achieve low overhead (no system calls),
 * high-precision timing.
 *
 * Note that timer 3 is also used for the high precision profiling timer
 * (PROFTIMER code above).  Care should be taken when both uses are
 * configured as only a token effort is made to avoid conflicting use.
 */
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <vm/vm.h>
#include <amiga/amiga/clockioctl.h>
#include <sys/specdev.h>
#include <sys/vnode.h>
#include <sys/mman.h>

int clockon = 0;		/* non-zero if high-res timer enabled */
#ifdef PROFTIMER
int  profprocs = 0;		/* # of procs using profiling timer */
#endif
#ifdef DEBUG
int clockdebug = 0;
#endif

/*ARGSUSED*/
clockopen(dev, flags)
	dev_t dev;
{
#ifdef PROFTIMER
#ifdef PROF
	/*
	 * Kernel profiling enabled, give up.
	 */
	if (profiling)
		return(EBUSY);
#endif
	/*
	 * If any user processes are profiling, give up.
	 */
	if (profprocs)
		return(EBUSY);
#endif
	if (!clockon) {
		startclock();
		clockon++;
	}
	return(0);
}

/*ARGSUSED*/
clockclose(dev, flags)
	dev_t dev;
{
	(void) clockunmmap(dev, (caddr_t)0, curproc);	/* XXX */
	stopclock();
	clockon = 0;
	return(0);
}

/*ARGSUSED*/
clockioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	struct proc *p;
{
	int error = 0;
	
	switch (cmd) {

	case CLOCKMAP:
		error = clockmmap(dev, (caddr_t *)data, p);
		break;

	case CLOCKUNMAP:
		error = clockunmmap(dev, *(caddr_t *)data, p);
		break;

	case CLOCKGETRES:
		*(int *)data = CLK_RESOLUTION;
		break;

	default:
		error = EINVAL;
		break;
	}
	return(error);
}

/*ARGSUSED*/
clockmap(dev, off, prot)
	dev_t dev;
{
	return((off + (INTIOBASE+CLKBASE+CLKSR-1)) >> PGSHIFT);
}

clockmmap(dev, addrp, p)
	dev_t dev;
	caddr_t *addrp;
	struct proc *p;
{
	int error;
	struct vnode vn;
	struct specinfo si;
	int flags;

	flags = MAP_FILE|MAP_SHARED;
	if (*addrp)
		flags |= MAP_FIXED;
	else
		*addrp = (caddr_t)0x1000000;	/* XXX */
	vn.v_type = VCHR;			/* XXX */
	vn.v_specinfo = &si;			/* XXX */
	vn.v_rdev = dev;			/* XXX */
	error = vm_mmap(&p->p_vmspace->vm_map, (vm_offset_t *)addrp,
			PAGE_SIZE, VM_PROT_ALL, flags, (caddr_t)&vn, 0);
	return(error);
}

clockunmmap(dev, addr, p)
	dev_t dev;
	caddr_t addr;
	struct proc *p;
{
	int rv;

	if (addr == 0)
		return(EINVAL);		/* XXX: how do we deal with this? */
	rv = vm_deallocate(p->p_vmspace->vm_map, (vm_offset_t)addr, PAGE_SIZE);
	return(rv == KERN_SUCCESS ? 0 : EINVAL);
}

startclock()
{
	register struct clkreg *clk = (struct clkreg *)clkstd[0];

	clk->clk_msb2 = -1; clk->clk_lsb2 = -1;
	clk->clk_msb3 = -1; clk->clk_lsb3 = -1;

	clk->clk_cr2 = CLK_CR3;
	clk->clk_cr3 = CLK_OENAB|CLK_8BIT;
	clk->clk_cr2 = CLK_CR1;
	clk->clk_cr1 = CLK_IENAB;
}

stopclock()
{
	register struct clkreg *clk = (struct clkreg *)clkstd[0];

	clk->clk_cr2 = CLK_CR3;
	clk->clk_cr3 = 0;
	clk->clk_cr2 = CLK_CR1;
	clk->clk_cr1 = CLK_IENAB;
}
#endif

#endif


#ifdef PROFTIMER
/*
 * This code allows the amiga kernel to use one of the extra timers on
 * the clock chip for profiling, instead of the regular system timer.
 * The advantage of this is that the profiling timer can be turned up to
 * a higher interrupt rate, giving finer resolution timing. The profclock
 * routine is called from the lev6intr in locore, and is a specialized
 * routine that calls addupc. The overhead then is far less than if
 * hardclock/softclock was called. Further, the context switch code in
 * locore has been changed to turn the profile clock on/off when switching
 * into/out of a process that is profiling (startprofclock/stopprofclock).
 * This reduces the impact of the profiling clock on other users, and might
 * possibly increase the accuracy of the profiling. 
 */
int  profint   = PRF_INTERVAL;	/* Clock ticks between interrupts */
int  profscale = 0;		/* Scale factor from sys clock to prof clock */
char profon    = 0;		/* Is profiling clock on? */

/* profon values - do not change, locore.s assumes these values */
#define PRF_NONE	0x00
#define	PRF_USER	0x01
#define	PRF_KERNEL	0x80

initprofclock()
{
#if NCLOCK > 0
	struct proc *p = curproc;		/* XXX */

	/*
	 * If the high-res timer is running, force profiling off.
	 * Unfortunately, this gets reflected back to the user not as
	 * an error but as a lack of results.
	 */
	if (clockon) {
		p->p_stats->p_prof.pr_scale = 0;
		return;
	}
	/*
	 * Keep track of the number of user processes that are profiling
	 * by checking the scale value.
	 *
	 * XXX: this all assumes that the profiling code is well behaved;
	 * i.e. profil() is called once per process with pcscale non-zero
	 * to turn it on, and once with pcscale zero to turn it off.
	 * Also assumes you don't do any forks or execs.  Oh well, there
	 * is always adb...
	 */
	if (p->p_stats->p_prof.pr_scale)
		profprocs++;
	else
		profprocs--;
#endif
	/*
	 * The profile interrupt interval must be an even divisor
	 * of the CLK_INTERVAL so that scaling from a system clock
	 * tick to a profile clock tick is possible using integer math.
	 */
	if (profint > CLK_INTERVAL || (CLK_INTERVAL % profint) != 0)
		profint = CLK_INTERVAL;
	profscale = CLK_INTERVAL / profint;
}

startprofclock()
{
  unsigned short interval;

  /* stop timer B */
  ciab.crb = ciab.crb & 0xc0;

  /* load interval into registers.
     the clocks run at NTSC: 715.909kHz or PAL: 709.379kHz */

  interval = profint - 1;

  /* order of setting is important ! */
  ciab.tblo = interval & 0xff;
  ciab.tbhi = interval >> 8;

  /* enable interrupts for timer B */
  ciab.icr = (1<<7) | (1<<1);

  /* start timer B in continuous shot mode */
  ciab.crb = (ciab.crb & 0xc0) | 1;
}

stopprofclock()
{
  /* stop timer B */
  ciab.crb = ciab.crb & 0xc0;
}

#ifdef PROF
/*
 * profclock() is expanded in line in lev6intr() unless profiling kernel.
 * Assumes it is called with clock interrupts blocked.
 */
profclock(pc, ps)
	caddr_t pc;
	int ps;
{
	/*
	 * Came from user mode.
	 * If this process is being profiled record the tick.
	 */
	if (USERMODE(ps)) {
		if (p->p_stats.p_prof.pr_scale)
			addupc(pc, &curproc->p_stats.p_prof, 1);
	}
	/*
	 * Came from kernel (supervisor) mode.
	 * If we are profiling the kernel, record the tick.
	 */
	else if (profiling < 2) {
		register int s = pc - s_lowpc;

		if (s < s_textsize)
			kcount[s / (HISTFRACTION * sizeof (*kcount))]++;
	}
	/*
	 * Kernel profiling was on but has been disabled.
	 * Mark as no longer profiling kernel and if all profiling done,
	 * disable the clock.
	 */
	if (profiling && (profon & PRF_KERNEL)) {
		profon &= ~PRF_KERNEL;
		if (profon == PRF_NONE)
			stopprofclock();
	}
}
#endif
#endif

/* this is a hook set by a clock driver for the configured realtime clock,
   returning plain current unix-time */
long (*gettod) __P((void));
int (*settod) __P((long));
void *clockaddr;

long a3gettod __P((void));
long a2gettod __P((void));
int a3settod __P((long));
int a2settod __P((long));
int rtcinit __P((void));

/*
 * Initialize the time of day register, based on the time base which is, e.g.
 * from a filesystem.
 */
inittodr(base)
	time_t base;
{
	u_long timbuf = base;	/* assume no battery clock exists */
  
	if (gettod == NULL && rtcinit() == 0)
		printf("WARNING: no battery clock\n");
	else
		timbuf = gettod();
  
	if (timbuf < base) {
		printf("WARNING: bad date in battery clock\n");
		timbuf = base;
	}
  
	/* Battery clock does not store usec's, so forget about it. */
	time.tv_sec = timbuf;
}

resettodr()
{
	if (settod && settod(time.tv_sec) == 1)
		return;
	printf("Cannot set battery backed clock\n");
}

int
rtcinit()
{
	clockaddr = (void *)ztwomap(0xdc0000);
	if (is_a3000() || is_a4000()) {
		if (a3gettod() == 0)
			return(0);
		gettod = a3gettod;
		settod = a3settod;
	} else {
		if (a2gettod() == 0)
			return(0);
		gettod = a2gettod;
		settod = a2settod;
	}
	return(1);
}

static int month_days[12] = {
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

long
a3gettod()
{
	struct rtclock3000 *rt;
	int i, year, month, day, wday, hour, min, sec;
	u_long tmp;

	rt = clockaddr;

	/* hold clock */
	rt->control1 = A3CONTROL1_HOLD_CLOCK;

	/* read it */
	sec   = rt->second1 * 10 + rt->second2;
	min   = rt->minute1 * 10 + rt->minute2;
	hour  = rt->hour1   * 10 + rt->hour2;
	wday  = rt->weekday;
	day   = rt->day1    * 10 + rt->day2;
	month = rt->month1  * 10 + rt->month2;
	year  = rt->year1   * 10 + rt->year2   + 1900;

	/* let it run again.. */
	rt->control1 = A3CONTROL1_FREE_CLOCK;

	if (range_test(hour, 0, 23))
		return(0);
	if (range_test(wday, 0, 6))
		return(0);
	if (range_test(day, 1, 31))
		return(0);
	if (range_test(month, 1, 12))
		return(0);
	if (range_test(year, STARTOFTIME, 2000))
		return(0);

	tmp = 0;

	for (i = STARTOFTIME; i < year; i++)
		tmp += days_in_year(i);
	if (leapyear(year) && month > FEBRUARY)
		tmp++;

	for (i = 1; i < month; i++)
		tmp += days_in_month(i);

	tmp += (day - 1);
	tmp = ((tmp * 24 + hour) * 60 + min) * 60 + sec;

	return(tmp);
}

int
a3settod(tim)
	long tim;
{
	register int i;
	register long hms, day;
	u_char sec1, sec2;
	u_char min1, min2;
	u_char hour1, hour2;
/*	u_char wday; */
	u_char day1, day2;
	u_char mon1, mon2;
	u_char year1, year2;
	struct rtclock3000 *rt;

	rt = clockaddr;
	/*
	 * there seem to be problems with the bitfield addressing
	 * currently used..
	 */

	if (! rt)
		return 0;

	/* prepare values to be written to clock */
	day = tim / SECDAY;
	hms = tim % SECDAY;

	hour2 = hms / 3600;
	hour1 = hour2 / 10;
	hour2 %= 10;

	min2 = (hms % 3600) / 60;
	min1 = min2 / 10;
	min2 %= 10;


	sec2 = (hms % 3600) % 60;
	sec1 = sec2 / 10;
	sec2 %= 10;

	/* Number of years in days */
	for (i = STARTOFTIME - 1900; day >= days_in_year(i); i++)
		day -= days_in_year(i);
	year1 = i / 10;
	year2 = i % 10;

	/* Number of months in days left */
	if (leapyear(i))
		days_in_month(FEBRUARY) = 29;
	for (i = 1; day >= days_in_month(i); i++)
		day -= days_in_month(i);
	days_in_month(FEBRUARY) = 28;

	mon1 = i / 10;
	mon2 = i % 10;

	/* Days are what is left over (+1) from all that. */
	day ++;
	day1 = day / 10;
	day2 = day % 10;

	rt->control1 = A3CONTROL1_HOLD_CLOCK;
	rt->second1 = sec1;
	rt->second2 = sec2;
	rt->minute1 = min1;
	rt->minute2 = min2;
	rt->hour1   = hour1;
	rt->hour2   = hour2;
/*	rt->weekday = wday; */
	rt->day1    = day1;
	rt->day2    = day2;
	rt->month1  = mon1;
	rt->month2  = mon2;
	rt->year1   = year1;
	rt->year2   = year2;
	rt->control1 = A3CONTROL1_FREE_CLOCK;

	return 1;
}

long
a2gettod()
{
	struct rtclock2000 *rt;
	int i, year, month, day, hour, min, sec;
	u_long tmp;

	rt = clockaddr;

	/*
	 * hold clock
	 */
	rt->control1 |= A2CONTROL1_HOLD;
	i = 0x1000;
	while (rt->control1 & A2CONTROL1_BUSY && i--)
		;
	if (rt->control1 & A2CONTROL1_BUSY)
		return (0);	/* Give up and say it's not there */

	/*
	 * read it
	 */
	sec = rt->second1 * 10 + rt->second2;
	min = rt->minute1 * 10 + rt->minute2;
	hour = (rt->hour1 & 3)  * 10 + rt->hour2;
	day = rt->day1 * 10 + rt->day2;
	month = rt->month1 * 10 + rt->month2;
	year = rt->year1 * 10 + rt->year2   + 1900;

	if ((rt->control3 & A2CONTROL3_24HMODE) == 0) {
		if ((rt->hour1 & A2HOUR1_PM) == 0 && hour == 12)
			hour = 0;
		else if ((rt->hour1 & A2HOUR1_PM) && hour != 12)
			hour += 12;
	}

	/* 
	 * release the clock 
	 */
	rt->control1 &= ~A2CONTROL1_HOLD;

	if (range_test(hour, 0, 23))
		return(0);
	if (range_test(day, 1, 31))
		return(0);
	if (range_test(month, 1, 12))
		return(0);
	if (range_test(year, STARTOFTIME, 2000))
		return(0);
  
	tmp = 0;
  
	for (i = STARTOFTIME; i < year; i++)
		tmp += days_in_year(i);
	if (leapyear(year) && month > FEBRUARY)
		tmp++;
  
	for (i = 1; i < month; i++)
		tmp += days_in_month(i);
  
	tmp += (day - 1);
	tmp = ((tmp * 24 + hour) * 60 + min) * 60 + sec;
  
	return(tmp);
}

/*
 * there is some question as to whether this works
 * I guess
 */
int
a2settod(tim)
	long tim;
{

	int i;
	long hms, day;
	u_char sec1, sec2;
	u_char min1, min2;
	u_char hour1, hour2;
	u_char day1, day2;
	u_char mon1, mon2;
	u_char year1, year2;
	struct rtclock2000 *rt;

	rt = clockaddr;
	/* 
	 * there seem to be problems with the bitfield addressing
	 * currently used..
	 *
	 * XXX Check out the above where we (hour1 & 3)
	 */
	if (! rt)
		return 0;

	/* prepare values to be written to clock */
	day = tim / SECDAY;
	hms = tim % SECDAY;

	hour2 = hms / 3600;
	hour1 = hour2 / 10;
	hour2 %= 10;

	min2 = (hms % 3600) / 60;
	min1 = min2 / 10;
	min2 %= 10;


	sec2 = (hms % 3600) % 60;
	sec1 = sec2 / 10;
	sec2 %= 10;

	/* Number of years in days */
	for (i = STARTOFTIME - 1900; day >= days_in_year(i); i++)
		day -= days_in_year(i);
	year1 = i / 10;
	year2 = i % 10;

	/* Number of months in days left */
	if (leapyear(i))
		days_in_month(FEBRUARY) = 29;
	for (i = 1; day >= days_in_month(i); i++)
		day -= days_in_month(i);
	days_in_month(FEBRUARY) = 28;

	mon1 = i / 10;
	mon2 = i % 10;
  
	/* Days are what is left over (+1) from all that. */
	day ++;
	day1 = day / 10;
	day2 = day % 10;

	/* 
	 * XXXX spin wait as with reading???
	 */
	rt->control1 |= A2CONTROL1_HOLD;
	rt->second1 = sec1;
	rt->second2 = sec2;
	rt->minute1 = min1;
	rt->minute2 = min2;
	rt->hour1   = hour1;
	rt->hour2   = hour2;
	rt->day1    = day1;
	rt->day2    = day2;
	rt->month1  = mon1;
	rt->month2  = mon2;
	rt->year1   = year1;
	rt->year2   = year2;
	rt->control2 &= ~A2CONTROL1_HOLD;

  return 1;
}
