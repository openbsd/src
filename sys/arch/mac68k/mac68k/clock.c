/*	$NetBSD: clock.c,v 1.22 1996/02/19 21:40:48 scottr Exp $	*/

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
 */
/*-
 * Copyright (C) 1993	Allen K. Briggs, Chris P. Caputo,
 *			Michael L. Finch, Bradley A. Grantham, and
 *			Lawrence A. Kesteloot
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
 *	This product includes software developed by the Alice Group.
 * 4. The names of the Alice Group or any of its members may not be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE ALICE GROUP ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE ALICE GROUP BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
/*
 *
 * from: Utah $Hdr: clock.c 1.18 91/01/21$
 *
 *	@(#)clock.c   7.6 (Berkeley) 5/7/91
 */

#if !defined(STANDALONE)
#include <sys/param.h>
#include <sys/kernel.h>

#include <machine/psl.h>
#include <machine/cpu.h>

#if defined(GPROF) && defined(PROFTIMER)
#include <sys/gprof.h>
#endif

#else				/* STANDALONE */
#include "stand.h"
#endif				/* STANDALONE */

#include "clockreg.h"
#include "via.h"

static int month_days[12] = {
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};
#define DIFF19041970 2082844800
#define DIFF19701990 630720000
#define DIFF19702010 1261440000

/*
 * Mac II machine-dependent clock routines.
 */

/*
 * Start the real-time clock; i.e. set timer latches and boot timer.
 *
 * We use VIA1 timer 1.
 */
void 
startrtclock(void)
{
/* BARF MF startrt clock is called twice in init_main, configure,
   the reason why is doced in configure */

	/* be certain clock interrupts are off */
	via_reg(VIA1, vIER) = V1IF_T1;

	/* set timer latch */
	via_reg(VIA1, vACR) |= ACR_T1LATCH;

	/* set VIA timer 1 latch to 60 Hz (100 Hz) */
	via_reg(VIA1, vT1L) = CLK_INTL;
	via_reg(VIA1, vT1LH) = CLK_INTH;

	/* set VIA timer 1 counter started for 60(100) Hz */
	via_reg(VIA1, vT1C) = CLK_INTL;
	via_reg(VIA1, vT1CH) = CLK_INTH;
}

void
enablertclock(void)
{
	/* clear then enable clock interrupt. */
	via_reg(VIA1, vIFR) |= V1IF_T1;
	via_reg(VIA1, vIER) = 0x80 | V1IF_T1;
}

void
cpu_initclocks(void)
{
	enablertclock();
}

void
setstatclockrate(int rateinhz)
{
}

void
disablertclock(void)
{
	/* disable clock interrupt */
	via_reg(VIA1, vIER) = V1IF_T1;
}

/*
 * Returns number of usec since last clock tick/interrupt.
 *
 * Check high byte twice to prevent missing a roll-over.
 * (race condition?)
 */
u_long 
clkread()
{
	register int high, high2, low;

	high = via_reg(VIA1, vT1CH);
	low = via_reg(VIA1, vT1C);

	high2 = via_reg(VIA1, vT1CH);
	if (high != high2)
		high = high2;

	/* return count left in timer / 1.27 */
	/* return((CLK_INTERVAL - (high << 8) - low) / CLK_SPEED); */
	return ((CLK_INTERVAL - (high << 8) - low) * 10000 / 12700);
}


#ifdef PROFTIMER
/*
 * Here, we have implemented code that causes VIA2's timer to count
 * the profiling clock.  Following the HP300's lead, this reduces
 * the impact on other tasks, since locore turns off the profiling clock
 * on context switches.  If need be, the profiling clock's resolution can
 * be cranked higher than the real-time clock's resolution, to prevent
 * aliasing and allow higher accuracy.
 */
int     profint = PRF_INTERVAL;	/* Clock ticks between interrupts */
int     profinthigh;
int     profintlow;
int     profscale = 0;		/* Scale factor from sys clock to prof clock */
char    profon = 0;		/* Is profiling clock on? */

/* profon values - do not change, locore.s assumes these values */
#define PRF_NONE   0x00
#define   PRF_USER   0x01
#define   PRF_KERNEL   0x80

void 
initprofclock(void)
{
	/* profile interval must be even divisor of system clock interval */
	if (profint > CLK_INTERVAL)
		profint = CLK_INTERVAL;
	else
		if (CLK_INTERVAL % profint != 0)
			/* try to intelligently fix clock interval */
			profint = CLK_INTERVAL / (CLK_INTERVAL / profint);

	profscale = CLK_INTERVAL / profint;

	profinthigh = profint >> 8;
	profintlow = profint & 0xff;
}

void 
startprofclock(void)
{
	via_reg(VIA2, vT1L) = (profint - 1) & 0xff;
	via_reg(VIA2, vT1LH) = (profint - 1) >> 8;
	via_reg(VIA2, vACR) |= ACR_T1LATCH;
	via_reg(VIA2, vT1C) = (profint - 1) & 0xff;
	via_reg(VIA2, vT1CH) = (profint - 1) >> 8;
}

void 
stopprofclock(void)
{
	via_reg(VIA2, vT1L) = 0;
	via_reg(VIA2, vT1LH) = 0;
	via_reg(VIA2, vT1C) = 0;
	via_reg(VIA2, vT1CH) = 0;
}

#ifdef GPROF
/*
 * BARF: we should check this:
 *
 * profclock() is expanded in line in lev6intr() unless profiling kernel.
 * Assumes it is called with clock interrupts blocked.
 */
void 
profclock(clockframe * pclk)
{
	/*
         * Came from user mode.
         * If this process is being profiled record the tick.
         */
	if (USERMODE(pclk->ps)) {
		if (p->p_stats.p_prof.pr_scale)
			addupc_task(&curproc, pclk->pc, 1);
	}
	/*
         * Came from kernel (supervisor) mode.
         * If we are profiling the kernel, record the tick.
         */
	else
		if (profiling < 2) {
			register int s = pclk->pc - s_lowpc;

			if (s < s_textsize)
				kcount[s / (HISTFRACTION * sizeof(*kcount))]++;
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

/*
 * Convert GMT to Mac PRAM time, using global timezone
 * GMT bias adjustment is done elsewhere.
 */
static u_long 
ugmt_2_pramt(u_long t)
{
	/* don't know how to open a file properly. */
	/* assume compiled timezone is correct. */

	return (t = t + DIFF19041970 - 60*tz.tz_minuteswest);
}

/*
 * Convert a Mac PRAM time value to GMT, using compiled-in timezone
 * GMT bias adjustment is done elsewhere.
 */
static u_long 
pramt_2_ugmt(u_long t)
{
	return (t = t - DIFF19041970 + 60*tz.tz_minuteswest);
}

/*
 * Time from the booter.
 */
u_long  macos_boottime;

/*
 * Bias in minutes east from GMT (also from booter).
 */
long    macos_gmtbias;

/*
 * Flag for whether or not we can trust the PRAM.  If we don't
 * trust it, we don't write to it, and we take the MacOS value
 * that is passed from the booter (which will only be a second
 * or two off by now).
 */
int     mac68k_trust_pram = 1;

/*
 * Set global GMT time register, using a file system time base for comparison
 * and sanity checking.
 */
void 
inittodr(time_t base)
{
	u_long  timbuf;
	u_long  pramtime;

	timbuf = pramt_2_ugmt(pram_readtime());
	if ((timbuf - (macos_boottime + 60 * tz.tz_minuteswest)) > 10 * 60) {
#if DIAGNOSTIC
		printf(
		   "PRAM time does not appear to have been read correctly.\n");
		printf("PRAM: 0x%x, macos_boottime: 0x%x.\n",
			timbuf, macos_boottime + 60 * tz.tz_minuteswest);
#endif
		timbuf = macos_boottime;
		mac68k_trust_pram = 0;
	}
#ifdef DIAGNOSTIC
	else
		printf("PRAM: 0x%x, macos_boottime: 0x%x.\n",
			timbuf, macos_boottime);
#endif

	/*
	 * GMT bias is passed in from Booter
	 * To get GMT, *subtract* GMTBIAS from *our* time
	 * (gmtbias is in minutes, mult by 60)
	 */
	timbuf -= macos_gmtbias * 60;

	if (base < 5 * SECYR) {
		printf("WARNING: file system time earlier than 1975\n");
		printf(" -- CHECK AND RESET THE DATE!\n");
		base = 21 * SECYR;	/* 1991 is our sane date */
	}
	/*
	 * Check sanity against the year 2010.  Let's hope NetBSD/mac68k
	 * doesn't run that long!
	 */
	if (base > 40 * SECYR) {
		printf("WARNING: file system time later than 2010\n");
		printf(" -- CHECK AND RESET THE DATE!\n");
		base = 21 * SECYR;	/* 1991 is our sane date */
	}
	if (timbuf < base) {
		printf(
		    "WARNING: Battery clock has earlier time than UNIX fs.\n");
		if (((u_long) base) < (40 * SECYR))
			timbuf = base;
	}
	time.tv_sec = timbuf;
	time.tv_usec = 0;
}

/*
 * Set battery backed clock to a new time, presumably after someone has
 * changed system time.
 */
void 
resettodr(void)
{
	if (mac68k_trust_pram)
		/*
		 * GMT bias is passed in from the Booter.
		 * To get *our* time, add GMTBIAS to GMT.
		 * (gmtbias is in minutes, multiply by 60).
		 */
		pram_settime(ugmt_2_pramt(time.tv_sec + macos_gmtbias * 60));
#if DIAGNOSTIC
	else
		printf("NetBSD/mac68k does not trust itself to try and write "
		    "to the pram on this system.\n");
#endif
}

/*
 * The Macintosh timers decrement once every 1.2766 microseconds.
 * MGFH2, p. 180
 */
#define	CLK_RATE	12766

#define	DELAY_CALIBRATE	(0xffffff << 7)	/* Large value for calibration */
#define	LARGE_DELAY	0x40000		/* About 335 msec */

int delay_factor = DELAY_CALIBRATE;
volatile int delay_flag = 1;

/*
 * delay(usec)
 *	Delay usec microseconds.
 *
 * The delay_factor is scaled up by a factor of 128 to avoid loss
 * of precision for small delays.  As a result of this, note that
 * delays larger that LARGE_DELAY will be up to 128 usec too short,
 * due to adjustments for calculations involving 32 bit values.
 */
void
delay(unsigned usec)
{
	register unsigned int cycles;

	if (usec > LARGE_DELAY)
		cycles = (usec >> 7) * delay_factor;
	else
		cycles = ((usec > 0 ? usec : 1) * delay_factor) >> 7;

	while ((cycles-- > 0) && delay_flag)
		;
}

/*
 * Dummy delay calibration.  Functionally identical to delay(), but
 * returns the number of times through the loop.
 */
static int
dummy_delay(usec)
	unsigned usec;
{
	register unsigned int cycles;

	if (usec > LARGE_DELAY)
		cycles = (usec >> 7) * delay_factor;
	else
		cycles = ((usec > 0 ? usec : 1) * delay_factor) >> 7;

	while ((cycles-- > 0) && delay_flag)
		;

	return ((delay_factor >> 7) - cycles);
}

static void
delay_timer1_irq()
{
	delay_flag = 0;
}

/*
 * Calibrate delay_factor with VIA1 timer T1.
 */
void
mac68k_calibrate_delay()
{
	int n;
	int sum;

	/* Disable VIA1 timer 1 interrupts and set up service routine */
	via_reg(VIA1, vIER) = V1IF_T1;
	mac68k_register_via1_t1_irq(delay_timer1_irq);

	/* Set the timer for one-shot mode, then clear and enable interrupts */
	via_reg(VIA1, vACR) &= ~ACR_T1LATCH;
	via_reg(VIA1, vIFR) = V1IF_T1;		/* (this is needed for IIsi) */
	via_reg(VIA1, vIER) = 0x80 | V1IF_T1;

	for (sum = 0, n = 8; n > 0; n--) {
		delay_flag = 1;
		via_reg(VIA1, vT1C) = 0;	/* 1024 clock ticks */
		via_reg(VIA1, vT1CH) = 4;	/* (approx 1.3 msec) */
		sum += dummy_delay(1);
	}

	/* Disable timer interrupts and reset service routine */
	via_reg(VIA1, vIER) = V1IF_T1;
	mac68k_register_via1_t1_irq(NULL);

	/*
	 * If this weren't integer math, the following would look
	 * a lot prettier.  It should really be something like
	 * this:
	 *	delay_factor = ((sum / 8) / (1024 * 1.2766)) * 128;
	 * That is, average the sum, divide by the number of usec,
	 * and multiply by a scale factor of 128.
	 * 
	 * We can accomplish the same thing by simplifying and using
	 * shifts, being careful to avoid as much loss of precision
	 * as possible.  (If the sum exceeds (2^31-1)/10000, we need
	 * to rearrange the calculation slightly to do this.)
	 */
	if (sum > 214748)	/* This is a _fast_ machine! */
		delay_factor = (((sum >> 3) * 10000) / CLK_RATE) >> 3;
	else
		delay_factor = (((sum * 10000) >> 3) / CLK_RATE) >> 3;

	/* Reset the delay_flag for normal use */
	delay_flag = 1;

#ifdef DEBUG
	printf("delay calibrated, factor = %d\n", delay_factor);
#endif
}
