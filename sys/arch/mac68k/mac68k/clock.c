/*	$OpenBSD: clock.c,v 1.24 2007/07/14 19:06:48 miod Exp $	*/
/*	$NetBSD: clock.c,v 1.39 1999/11/05 19:14:56 scottr Exp $	*/

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
 * 3. Neither the name of the University nor the names of its contributors
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

/*
 * Mac II machine-dependent clock routines.
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/limits.h>
#include <sys/kernel.h>
#include <sys/systm.h>

#include <machine/autoconf.h>
#include <machine/psl.h>
#include <machine/cpu.h>

#include <mac68k/mac68k/pram.h>
#include <machine/viareg.h>

#include <dev/clock_subr.h>

#ifdef DEBUG
int	clock_debug = 0;
#endif

int	rtclock_intr(void *);

u_int		clk_interval;
u_int8_t	clk_inth, clk_intl;

#define	DIFF19041970	2082844800

/*
 * The Macintosh timers decrement once every 1.2766 microseconds.
 * MGFH2, p. 180
 */
#define	CLK_RATE	12766

/*
 * Start the real-time clock; i.e. set timer latches and boot timer.
 *
 * We use VIA1 timer 1.
 */
void
startrtclock()
{
#ifndef HZ
	/*
	 * By default, if HZ is not specified, use 60 Hz, unless we are
	 * using A/UX style interrupts. We then need to readjust values
	 * based on a 100Hz value in param.c.
	 */
	if (mac68k_machine.aux_interrupts == 0) {
#define	HZ_60 60
		hz = HZ_60;
		tick = 1000000 / HZ_60;
		tickadj = 240000 / (60 * HZ_60);/* can adjust 240ms in 60s */
	}
#endif

	/*
	 * Calculate clocks needed to hit hz ticks/sec.
	 *
	 * The VIA clock speed is 1.2766us, so the timer value needed is:
	 *
	 *                    1       1,000,000us     1
	 *  CLK_INTERVAL = -------- * ----------- * ------
	 e                 1.2766us       1s          hz
	 *
	 * While it may be tempting to simplify the following further,
	 * we can run into integer overflow problems.
	 * Also note:  do *not* define HZ to be less than 12; overflow
	 * will occur, yielding invalid results.
	 */
	clk_interval = ((100000000UL / hz) * 100) / 12766;
	clk_inth = ((clk_interval >> 8) & 0xff);
	clk_intl = (clk_interval & 0xff);

	/* be certain clock interrupts are off */
	via_reg(VIA1, vIER) = V1IF_T1;

	/* set timer latch */
	via_reg(VIA1, vACR) |= ACR_T1LATCH;

	/* set VIA timer 1 latch to ``hz'' Hz */
	via_reg(VIA1, vT1L) = clk_intl;
	via_reg(VIA1, vT1LH) = clk_inth;

	/* set VIA timer 1 counter started for ``hz'' Hz */
	via_reg(VIA1, vT1C) = clk_intl;
	via_reg(VIA1, vT1CH) = clk_inth;
}

void
cpu_initclocks()
{
	tickfix = 1000000 - (hz * tick);
	if (tickfix != 0) {
		int ftp;

		ftp = min(ffs(tickfix), ffs(hz));
		tickfix >>= (ftp - 1);
		tickfixinterval = hz >> (ftp - 1);
	}

	/* clear then enable clock interrupt. */
	via_reg(VIA1, vIFR) |= V1IF_T1;
	via_reg(VIA1, vIER) = 0x80 | V1IF_T1;
}

void
setstatclockrate(rateinhz)
	int rateinhz;
{
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
	int high, high2, low;

	high = via_reg(VIA1, vT1CH);
	low = via_reg(VIA1, vT1C);

	high2 = via_reg(VIA1, vT1CH);
	if (high != high2)
		high = high2;

	/* return count left in timer / 1.27 */
	return ((clk_interval - (high << 8) - low) * 10000 / CLK_RATE);
}

static u_long	ugmt_2_pramt(u_long);
static u_long	pramt_2_ugmt(u_long);

/*
 * Convert GMT to Mac PRAM time, using rtc_offset
 * GMT bias adjustment is done elsewhere.
 */
static u_long
ugmt_2_pramt(t)
	u_long t;
{
	/* don't know how to open a file properly. */
	/* assume compiled timezone is correct. */

	return (t + DIFF19041970 - 60 * tz.tz_minuteswest);
}

/*
 * Convert a Mac PRAM time value to GMT, using rtc_offset
 * GMT bias adjustment is done elsewhere.
 */
static u_long
pramt_2_ugmt(t)
	u_long t;
{
	return (t - DIFF19041970 + 60 * tz.tz_minuteswest);
}

/*
 * Time from the booter.
 */
u_long	macos_boottime;

/*
 * Bias in minutes east from GMT (also from booter).
 */
long	macos_gmtbias;

/*
 * Flag for whether or not we can trust the PRAM.  If we don't
 * trust it, we don't write to it, and we take the MacOS value
 * that is passed from the booter (which will only be a second
 * or two off by now).
 */
int	mac68k_trust_pram = 1;

/*
 * Set global GMT time register, using a file system time base for comparison
 * and sanity checking.
 */
void
inittodr(base)
	time_t base;
{
	u_long timbuf;

	timbuf = pram_readtime();
	if (timbuf == 0) {
		/* We don't know how to access PRAM on this hardware. */
		timbuf = macos_boottime;
		mac68k_trust_pram = 0;
	} else {
		timbuf = pramt_2_ugmt(pram_readtime());
		if ((timbuf - (macos_boottime + 60 * tz.tz_minuteswest)) >
		    10 * 60) {
#ifdef DIAGNOSTIC
			printf("PRAM time does not appear"
			    " to have been read correctly.\n");
			printf("PRAM: 0x%lx, macos_boottime: 0x%lx.\n",
			    timbuf, macos_boottime + 60 * tz.tz_minuteswest);
#endif
			timbuf = macos_boottime;
			mac68k_trust_pram = 0;
		}
#ifdef DEBUG
		else
			printf("PRAM: 0x%lx, macos_boottime: 0x%lx.\n",
			    timbuf, macos_boottime);
#endif
	}

	/*
	 * GMT bias is passed in from Booter
	 * To get GMT, *subtract* GMTBIAS from *our* time
	 * (gmtbias is in minutes, mult by 60)
	 */
	timbuf -= macos_gmtbias * 60;

	if (base < 5 * SECYR) {
		printf("WARNING: file system time earlier than 1975\n");
		printf(" -- CHECK AND RESET THE DATE!\n");
		base = 36 * SECYR;	/* Last update here in 2006... */
	}
	if (timbuf < base) {
		printf(
		    "WARNING: Battery clock has earlier time than UNIX fs.\n");
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
resettodr()
{
	if (mac68k_trust_pram)
		/*
		 * GMT bias is passed in from the Booter.
		 * To get *our* time, add GMTBIAS to GMT.
		 * (gmtbias is in minutes, multiply by 60).
		 */
		pram_settime(ugmt_2_pramt(time.tv_sec + macos_gmtbias * 60));
}

#define	DELAY_CALIBRATE	(0xffffff << 7)	/* Large value for calibration */

u_int		delay_factor = DELAY_CALIBRATE;
volatile int	delay_flag = 1;

int		_delay(u_int);
static int	delay_timer1_irq(void *);

static int
delay_timer1_irq(dummy)
	void *dummy;
{
	delay_flag = 0;
	return (1);
}

/*
 * Calibrate delay_factor with VIA1 timer T1.
 */
void
mac68k_calibrate_delay()
{
	u_int sum, n;

	/* Disable VIA1 timer 1 interrupts and set up service routine */
	via_reg(VIA1, vIER) = V1IF_T1;
	via1_register_irq(VIA1_T1, delay_timer1_irq, NULL, NULL);

	/* Set the timer for one-shot mode, then clear and enable interrupts */
	via_reg(VIA1, vACR) &= ~ACR_T1LATCH;
	via_reg(VIA1, vIFR) = V1IF_T1;	/* (this is needed for IIsi) */
	via_reg(VIA1, vIER) = 0x80 | V1IF_T1;

#ifdef DEBUG
	if (clock_debug)
		printf("mac68k_calibrate_delay(): entering timing loop\n");
#endif

	(void)_spl(IPLTOPSL(mac68k_machine.via1_ipl) - 1);

	for (sum = 0, n = 8; n > 0; n--) {
		delay_flag = 1;
		via_reg(VIA1, vT1C) = 0;	/* 1024 clock ticks */
		via_reg(VIA1, vT1CH) = 4;	/* (approx 1.3 msec) */
		sum += ((delay_factor >> 7) - _delay(1));
	}

	(void)splhigh();

	/* Disable timer interrupts and reset service routine */
	via_reg(VIA1, vIER) = V1IF_T1;
	via1_register_irq(VIA1_T1, rtclock_intr, NULL, NULL);

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
	 * as possible.  (If the sum exceeds UINT_MAX/10000, we need
	 * to rearrange the calculation slightly to do this.)
	 */
	if (sum > (UINT_MAX / 10000))	/* This is a _fast_ machine! */
		delay_factor = (((sum >> 3) * 10000) / CLK_RATE) >> 3;
	else
		delay_factor = (((sum * 10000) >> 3) / CLK_RATE) >> 3;

	/* Reset the delay_flag for normal use */
	delay_flag = 1;

#ifdef DEBUG
	if (clock_debug)
		printf("mac68k_calibrate_delay(): delay_factor calibrated\n");
#endif
}
