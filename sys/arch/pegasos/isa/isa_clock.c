/*	$OpenBSD: isa_clock.c,v 1.1 2003/10/31 03:54:33 drahn Exp $	*/
/*	$NetBSD: clock.c,v 1.39 1996/05/12 23:11:54 mycroft Exp $	*/

/*-
 * Copyright (c) 1993, 1994 Charles Hannum.
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
 *
 *	@(#)clock.c	7.2 (Berkeley) 5/12/91
 */
/* 
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
/*
  Copyright 1988, 1989 by Intel Corporation, Santa Clara, California.

		All Rights Reserved

Permission to use, copy, modify, and distribute this software and
its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appears in all
copies and that both the copyright notice and this permission notice
appear in supporting documentation, and that the name of Intel
not be used in advertising or publicity pertaining to distribution
of the software without specific, written prior permission.

INTEL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
IN NO EVENT SHALL INTEL BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

/*
 * Primitive clock interrupt routines.
 */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/timeout.h>

#include <machine/cpu.h>
#include <machine/intr.h>
#include <machine/pio.h>

#include <dev/clock_subr.h>
#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <dev/ic/mc146818reg.h>
#include <pegasos/isa/nvram.h>
#include <pegasos/isa/timerreg.h>
#include <powerpc/isa/isa_machdep.h>

#include "pcppi.h"
#if (NPCPPI > 0)
#include <dev/isa/pcppivar.h>

#define __BROKEN_INDIRECT_CONFIG /* XXX */
#ifdef __BROKEN_INDIRECT_CONFIG
int sysbeepmatch(struct device *, void *, void *);
#else
int sysbeepmatch(struct device *, struct cfdata *, void *);
#endif
void sysbeepattach(struct device *, struct device *, void *);

struct cfattach sysbeep_ca = {
	sizeof(struct device), sysbeepmatch, sysbeepattach
};

struct cfdriver sysbeep_cd = {
	NULL, "sysbeep", DV_DULL
};

static int ppi_attached;
static pcppi_tag_t ppicookie;
#endif /* PCPPI */

void	spinwait(int);
void	findcpuspeed(void);
int	clockintr(void *);
int	gettick(void);
void	sysbeep(int, int);
int	rtcget(mc_todregs *);
void	rtcput(mc_todregs *);
int 	hexdectodec(int);
int	dectohexdec(int);
int	rtcintr(void *);
void	rtcdrain(void *);

u_int mc146818_read(void *, u_int);
void mc146818_write(void *, u_int, u_int);

#if defined(I486_CPU) || defined(I586_CPU) || defined(I686_CPU)
int pentium_mhz, clock_broken_latch;
#endif

#define	SECMIN	((unsigned)60)			/* seconds per minute */
#define	SECHOUR	((unsigned)(60*SECMIN))		/* seconds per hour */

u_int
mc146818_read(void *sc, u_int reg)
{
	int s;
	u_char v;

	s = splhigh();
	isa_outb(IO_RTC, reg);
	DELAY(1);
	v = isa_inb(IO_RTC+1);
	DELAY(1);
	splx(s);
	return (v);
}

void
mc146818_write(void *sc, u_int reg, u_int datum)
{
	int s;

	s = splhigh();
	isa_outb(IO_RTC, reg);
	DELAY(1);
	isa_outb(IO_RTC+1, datum);
	DELAY(1);
	splx(s);
}

int
rtcget(mc_todregs *regs)
{
	if ((mc146818_read(NULL, MC_REGD) & MC_REGD_VRT) == 0) /* XXX softc */
		return (-1);
	MC146818_GETTOD(NULL, regs);			/* XXX softc */
	return (0);
}	

void
rtcput(mc_todregs *regs)
{
	MC146818_PUTTOD(NULL, regs);			/* XXX softc */
}

int
hexdectodec(int n)
{
	return (((n >> 4) & 0x0f) * 10 + (n & 0x0f));
}

int
dectohexdec(int n)
{
	return ((u_char)(((n / 10) << 4) & 0xf0) | ((n % 10) & 0x0f));
}

static int timeset;

/*
 * check whether the CMOS layout is "standard"-like (ie, not PS/2-like),
 * to be called at splclock()
 */
int cmoscheck(void);
int
cmoscheck()
{
	int i;
	unsigned short cksum = 0;

	for (i = 0x10; i <= 0x2d; i++)
		cksum += mc146818_read(NULL, i); /* XXX softc */

#if 0
	return (cksum == (mc146818_read(NULL, 0x2e) << 8)
			  + mc146818_read(NULL, 0x2f));
#else
	  return 1;
#endif
}

/*
 * patchable to control century byte handling:
 * 1: always update
 * -1: never touch
 * 0: try to figure out itself
 */
int rtc_update_century = 0;

/*
 * Expand a two-digit year as read from the clock chip
 * into full width.
 * Being here, deal with the CMOS century byte.
 */
int clock_expandyear(int);
int
clock_expandyear(clockyear)
	int clockyear;
{
	int clockcentury;

	clockcentury = (clockyear < 70) ? 20 : 19;
	clockyear += 100 * clockcentury;

	if (rtc_update_century < 0)
		return (clockyear);

	return (clockyear);
}

/*
 * Initialize the time of day register, based on the time base which is, e.g.
 * from a filesystem.
 */
void
inittodr(time_t base)
{
	mc_todregs rtclk;
	struct clock_ymdhms dt;
	int s;

	/*
	 * We mostly ignore the suggested time and go for the RTC clock time
	 * stored in the CMOS RAM.  If the time can't be obtained from the
	 * CMOS, or if the time obtained from the CMOS is 5 or more years
	 * less than the suggested time, we used the suggested time.  (In
	 * the latter case, it's likely that the CMOS battery has died.)
	 */

	if (base < 15*SECYR) {	/* if before 1985, something's odd... */
		printf("WARNING: preposterous time in file system\n");
		/* read the system clock anyway */
		base = 17*SECYR + 186*SECDAY + SECDAY/2;
	}

	mc146818_write(NULL, MC_REGB, MC_REGB_24HR | MC_REGB_PIE);

	time.tv_usec = 0;

	s = splclock();
	if (rtcget(&rtclk)) {
		splx(s);
		printf("WARNING: invalid time in clock chip\n");
		goto fstime;
	}
	splx(s);

	dt.dt_sec = hexdectodec(rtclk[MC_SEC]);
	dt.dt_min = hexdectodec(rtclk[MC_MIN]);
	dt.dt_hour = hexdectodec(rtclk[MC_HOUR]);
	dt.dt_day = hexdectodec(rtclk[MC_DOM]);
	dt.dt_mon = hexdectodec(rtclk[MC_MONTH]);
	dt.dt_year = clock_expandyear(hexdectodec(rtclk[MC_YEAR]));


	/*
	 * If time_t is 32 bits, then the "End of Time" is 
	 * Mon Jan 18 22:14:07 2038 (US/Eastern)
	 * This code copes with RTC's past the end of time if time_t
	 * is an int32 or less. Needed because sometimes RTCs screw
	 * up or are badly set, and that would cause the time to go
	 * negative in the calculation below, which causes Very Bad
	 * Mojo. This at least lets the user boot and fix the problem.
	 * Note the code is self eliminating once time_t goes to 64 bits.
	 */
	if (sizeof(time_t) <= sizeof(int32_t)) {
		if (dt.dt_year >= 2038) {
			printf("WARNING: RTC time at or beyond 2038.\n");
			dt.dt_year = 2037;
			printf("WARNING: year set back to 2037.\n");
			printf("WARNING: CHECK AND RESET THE DATE!\n");
		}
	}

	time.tv_sec = clock_ymdhms_to_secs(&dt) + tz.tz_minuteswest * 60;
	if (tz.tz_dsttime)
		time.tv_sec -= 3600;

	if (base < time.tv_sec - 5*SECYR)
		printf("WARNING: file system time much less than clock time\n");
	else if (base > time.tv_sec + 5*SECYR) {
		printf("WARNING: clock time much less than file system time\n");
		printf("WARNING: using file system time\n");
		goto fstime;
	}

	timeset = 1;
	return;

fstime:
	timeset = 1;
	time.tv_sec = base;
	printf("WARNING: CHECK AND RESET THE DATE!\n");
}

/*
 * Reset the clock.
 */
void
resettodr()
{
	mc_todregs rtclk;
	struct clock_ymdhms dt;
	int diff;
	int century;
	int s;

	/*
	 * We might have been called by boot() due to a crash early
	 * on.  Don't reset the clock chip in this case.
	 */
	if (!timeset)
		return;

	s = splclock();
	if (rtcget(&rtclk))
		bzero(&rtclk, sizeof(rtclk));
	splx(s);

	diff = tz.tz_minuteswest * 60;
	if (tz.tz_dsttime)
		diff -= 3600;
	clock_secs_to_ymdhms(time.tv_sec - diff, &dt);

	rtclk[MC_SEC] = dectohexdec(dt.dt_sec);
	rtclk[MC_MIN] = dectohexdec(dt.dt_min);
	rtclk[MC_HOUR] = dectohexdec(dt.dt_hour);
	rtclk[MC_DOW] = dt.dt_wday;
	rtclk[MC_YEAR] = dectohexdec(dt.dt_year % 100);
	rtclk[MC_MONTH] = dectohexdec(dt.dt_mon);
	rtclk[MC_DOM] = dectohexdec(dt.dt_day);
	s = splclock();
	rtcput(&rtclk);
	if (rtc_update_century > 0) {
		century = dectohexdec(dt.dt_year / 100);
		mc146818_write(NULL, NVRAM_CENTURY, century); /* XXX softc */
	}
	splx(s);
}
