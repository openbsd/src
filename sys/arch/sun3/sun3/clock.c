/*	$NetBSD: clock.c,v 1.31 1996/10/30 00:24:42 gwr Exp $	*/

/*
 * Copyright (c) 1994 Gordon W. Ross
 * Copyright (c) 1993 Adam Glass
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	from: Utah Hdr: clock.c 1.18 91/01/21$
 *	from: @(#)clock.c	8.2 (Berkeley) 1/12/94
 */

/*
 * Machine-dependent clock routines for the Intersil 7170:
 * Original by Adam Glass;  partially rewritten by Gordon Ross.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/device.h>

#include <machine/autoconf.h>
#include <machine/psl.h>
#include <machine/cpu.h>

#include <machine/mon.h>
#include <machine/obio.h>
#include <machine/control.h>

#include "intersil7170.h"
#include "interreg.h"

#define	CLOCK_PRI	5

extern volatile u_char *interrupt_reg;
volatile char *clock_va;

#define intersil_clock ((volatile struct intersil7170 *) clock_va)

#define intersil_command(run, interrupt) \
	(run | interrupt | INTERSIL_CMD_FREQ_32K | INTERSIL_CMD_24HR_MODE | \
	 INTERSIL_CMD_NORMAL_MODE)

#define intersil_clear() (void)intersil_clock->clk_intr_reg

static int  clock_match __P((struct device *, void *vcf, void *args));
static void clock_attach __P((struct device *, struct device *, void *));

struct cfattach clock_ca = {
	sizeof(struct device), clock_match, clock_attach
};

struct cfdriver clock_cd = {
	NULL, "clock", DV_DULL
};

static int
clock_match(parent, vcf, args)
    struct device *parent;
    void *vcf, *args;
{
    struct cfdata *cf = vcf;
	struct confargs *ca = args;
	int pa;

	/* This driver only supports one unit. */
	if (cf->cf_unit != 0)
		return (0);

	/* Validate the given address. */
	if (ca->ca_paddr != OBIO_CLOCK)
		return (0);

	/* Default interrupt priority. */
	if (ca->ca_intpri == -1)
		ca->ca_intpri = CLOCK_PRI;

	return (1);
}

static void
clock_attach(parent, self, args)
	struct device *parent;
	struct device *self;
	void *args;
{
	struct cfdata *cf = self->dv_cfdata;
	struct confargs *ca = args;

	printf("\n");

	/*
	 * Can not hook up the ISR until cpu_initclock()
	 * because hardclock is not ready until then.
	 */
}

/*
 * Set and/or clear the desired clock bits in the interrupt
 * register.  We have to be extremely careful that we do it
 * in such a manner that we don't get ourselves lost.
 */
set_clk_mode(on, off, enable)
	u_char on, off;
	int enable;
{
	register u_char interreg;
	register int s;

	s = getsr();
	if ((s & PSL_IPL) < PSL_IPL7)
		panic("set_clk_mode: ipl");

	if (!intersil_clock)
		panic("set_clk_mode: map");

	/*
	 * make sure that we are only playing w/ 
	 * clock interrupt register bits
	 */
	on &= (IREG_CLOCK_ENAB_7 | IREG_CLOCK_ENAB_5);
	off &= (IREG_CLOCK_ENAB_7 | IREG_CLOCK_ENAB_5);

	/*
	 * Get a copy of current interrupt register,
	 * turning off any undesired bits (aka `off')
	 */
	interreg = *interrupt_reg & ~(off | IREG_ALL_ENAB);
	*interrupt_reg &= ~IREG_ALL_ENAB;

	/*
	 * Next we turns off the CLK5 and CLK7 bits to clear
	 * the flip-flops, then we disable clock interrupts.
	 * Now we can read the clock's interrupt register
	 * to clear any pending signals there.
	 */
	*interrupt_reg &= ~(IREG_CLOCK_ENAB_7 | IREG_CLOCK_ENAB_5);
	intersil_clock->clk_cmd_reg =
		intersil_command(INTERSIL_CMD_RUN, INTERSIL_CMD_IDISABLE);
	intersil_clear();

	/*
	 * Now we set all the desired bits
	 * in the interrupt register, then
	 * we turn the clock back on and
	 * finally we can enable all interrupts.
	 */
	*interrupt_reg |= (interreg | on);		/* enable flip-flops */

	if (enable)
		intersil_clock->clk_cmd_reg =
			intersil_command(INTERSIL_CMD_RUN, INTERSIL_CMD_IENABLE);

	*interrupt_reg |= IREG_ALL_ENAB;		/* enable interrupts */
}

/* Called very early by internal_configure. */
void clock_init()
{
	clock_va = obio_find_mapping(OBIO_CLOCK, OBIO_CLOCK_SIZE);

	if (!clock_va)
		mon_panic("clock_init: clock_va\n");
	if (!interrupt_reg)
		mon_panic("clock_init: interrupt_reg\n");

	/* Turn off clock interrupts until cpu_initclocks() */
	/* isr_init() already set the interrupt reg to zero. */
	intersil_clock->clk_cmd_reg =
		intersil_command(INTERSIL_CMD_RUN, INTERSIL_CMD_IDISABLE);
	intersil_clear();
}

#ifdef	DIAGNOSTIC
static int clk_intr_ready;
#endif

/*
 * Set up the real-time clock (enable clock interrupts).
 * Leave stathz 0 since there is no secondary clock available.
 * Note that clock interrupts MUST STAY DISABLED until here.
 */
void
cpu_initclocks(void)
{
	int s;
	extern void _isr_clock();

	if (!intersil_clock)
		panic("cpu_initclocks");
	s = splhigh();

	isr_add_custom(5, _isr_clock);
#ifdef	DIAGNOSTIC
	clk_intr_ready = 1;
#endif

	/* Set the clock to interrupt 100 time per second. */
	intersil_clock->clk_intr_reg = INTERSIL_INTER_CSECONDS;

	*interrupt_reg |= IREG_CLOCK_ENAB_5;	/* enable clock */
	intersil_clock->clk_cmd_reg =
		intersil_command(INTERSIL_CMD_RUN, INTERSIL_CMD_IENABLE);
	*interrupt_reg |= IREG_ALL_ENAB;		/* enable interrupts */
	splx(s);
}

/*
 * This doesn't need to do anything, as we have only one timer and
 * profhz==stathz==hz.
 */
void
setstatclockrate(newhz)
	int newhz;
{
	/* nothing */
}

/*
 * This is is called by the "custom" interrupt handler
 * after it has reset the pending bit in the clock.
 */
int clock_count = 0;
void clock_intr(frame)
	struct clockframe *frame;
{
	static unsigned char led_pattern = 0xFE;

#ifdef	DIAGNOSTIC
	if (!clk_intr_ready)
		panic("clock_intr");
#endif

	/* XXX - Move this LED frobbing to the idle loop? */
	clock_count++;
	if ((clock_count & 7) == 0) {
		led_pattern = (led_pattern << 1) | 1;
		if (led_pattern == 0xFF)
			led_pattern = 0xFE;
		set_control_byte((char *) DIAG_REG, led_pattern);
	}
	hardclock(frame);
}


/*
 * Return the best possible estimate of the time in the timeval
 * to which tvp points.  We do this by returning the current time
 * plus the amount of time since the last clock interrupt.
 *
 * Check that this time is no less than any previously-reported time,
 * which could happen around the time of a clock adjustment.  Just for
 * fun, we guarantee that the time will be greater than the value
 * obtained by a previous call.
 */
void
microtime(tvp)
	register struct timeval *tvp;
{
	int s = splhigh();
	static struct timeval lasttime;

	*tvp = time;
	tvp->tv_usec++; 	/* XXX */
	while (tvp->tv_usec > 1000000) {
		tvp->tv_sec++;
		tvp->tv_usec -= 1000000;
	}
	if (tvp->tv_sec == lasttime.tv_sec &&
		tvp->tv_usec <= lasttime.tv_usec &&
		(tvp->tv_usec = lasttime.tv_usec + 1) > 1000000)
	{
		tvp->tv_sec++;
		tvp->tv_usec -= 1000000;
	}
	lasttime = *tvp;
	splx(s);
}


/*
 * Machine-dependent clock routines.
 *
 * Inittodr initializes the time of day hardware which provides
 * date functions.
 *
 * Resettodr restores the time of day hardware after a time change.
 */
#define SECDAY		86400L
#define SECYR		(SECDAY * 365)

static long clk_get_secs(void);
static void clk_set_secs(long);

/*
 * Initialize the time of day register, based on the time base
 * which is, e.g. from a filesystem.
 */
void inittodr(fs_time)
	time_t fs_time;
{
	long diff, clk_time;
	long long_ago = (5 * SECYR);
	int clk_bad = 0;

	/*
	 * Sanity check time from file system.
	 * If it is zero,assume filesystem time is just unknown
	 * instead of preposterous.  Don't bark.
	 */
	if (fs_time < long_ago) {
		/*
		 * If fs_time is zero, assume filesystem time is just
		 * unknown instead of preposterous.  Don't bark.
		 */
		if (fs_time != 0)
			printf("WARNING: preposterous time in file system\n");
		/* 1991/07/01  12:00:00 */
		fs_time = 21*SECYR + 186*SECDAY + SECDAY/2;
	}

	clk_time = clk_get_secs();

	/* Sanity check time from clock. */
	if (clk_time < long_ago) {
		printf("WARNING: bad date in battery clock");
		clk_bad = 1;
		clk_time = fs_time;
	} else {
		/* Does the clock time jive with the file system? */
		diff = clk_time - fs_time;
		if (diff < 0)
			diff = -diff;
		if (diff >= (SECDAY*2)) {
			printf("WARNING: clock %s %d days",
				   (clk_time < fs_time) ? "lost" : "gained",
				   diff / SECDAY);
			clk_bad = 1;
		}
	}
	if (clk_bad)
		printf(" -- CHECK AND RESET THE DATE!\n");
	time.tv_sec = clk_time;
}

/*   
 * Resettodr restores the time of day hardware after a time change.
 */
void resettodr()
{
	clk_set_secs(time.tv_sec);
}

/*
 * Machine dependent base year:
 * Note: must be < 1970
 */
#define	CLOCK_BASE_YEAR	1968


/*
 * Routine to copy state into and out of the clock.
 * The clock registers have to be read or written
 * in sequential order (or so it appears). -gwr
 */
static void clk_get_dt(struct date_time *dt)
{
	int s;
	register volatile char *src, *dst;

	src = (char *) &intersil_clock->counters;

	s = splhigh();
	intersil_clock->clk_cmd_reg =
		intersil_command(INTERSIL_CMD_STOP, INTERSIL_CMD_IENABLE);

	dst = (char *) dt;
	dt++;	/* end marker */
	do {
		*dst++ = *src++;
	} while (dst < (char*)dt);

	intersil_clock->clk_cmd_reg =
		intersil_command(INTERSIL_CMD_RUN, INTERSIL_CMD_IENABLE);
	splx(s);
}

static void clk_set_dt(struct date_time *dt)
{
	int s;
	register volatile char *src, *dst;

	dst = (char *) &intersil_clock->counters;

	s = splhigh();
	intersil_clock->clk_cmd_reg =
		intersil_command(INTERSIL_CMD_STOP, INTERSIL_CMD_IENABLE);

	src = (char *) dt;
	dt++;	/* end marker */
	do {
		*dst++ = *src++;
	} while (src < (char *)dt);

	intersil_clock->clk_cmd_reg =
		intersil_command(INTERSIL_CMD_RUN, INTERSIL_CMD_IENABLE);
	splx(s);
}



/*
 * Generic routines to convert to or from a POSIX date
 * (seconds since 1/1/1970) and  yr/mo/day/hr/min/sec
 *
 * These are organized this way mostly to so the code
 * can easily be tested in an independent user program.
 * (These are derived from the hp300 code.)
 */

/* Traditional UNIX base year */
#define	POSIX_BASE_YEAR	1970
#define FEBRUARY	2

#define	leapyear(year)		((year) % 4 == 0)
#define	days_in_year(a) 	(leapyear(a) ? 366 : 365)
#define	days_in_month(a) 	(month_days[(a) - 1])

static int month_days[12] = {
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

void gmt_to_dt(long *tp, struct date_time *dt)
{
	register int i;
	register long days, secs;

	days = *tp / SECDAY;
	secs = *tp % SECDAY;

	/* Hours, minutes, seconds are easy */
	dt->dt_hour = secs / 3600;
	secs = secs % 3600;
	dt->dt_min  = secs / 60;
	secs = secs % 60;
	dt->dt_sec  = secs;

	/* Day of week (Note: 1/1/1970 was a Thursday) */
	dt->dt_dow = (days + 4) % 7;

	/* Number of years in days */
	i = POSIX_BASE_YEAR;
	while (days >= days_in_year(i)) {
		days -= days_in_year(i);
		i++;
	}
	dt->dt_year = i - CLOCK_BASE_YEAR;

	/* Number of months in days left */
	if (leapyear(i))
		days_in_month(FEBRUARY) = 29;
	for (i = 1; days >= days_in_month(i); i++)
		days -= days_in_month(i);
	days_in_month(FEBRUARY) = 28;
	dt->dt_month = i;

	/* Days are what is left over (+1) from all that. */
	dt->dt_day = days + 1;  
}

void dt_to_gmt(struct date_time *dt, long *tp)
{
	register int i;
	register long tmp;
	int year;

	/*
	 * Hours are different for some reason. Makes no sense really.
	 */

	tmp = 0;

	if (dt->dt_hour >= 24) goto out;
	if (dt->dt_day  >  31) goto out;
	if (dt->dt_month > 12) goto out;

	year = dt->dt_year + CLOCK_BASE_YEAR;

	/*
	 * Compute days since start of time
	 * First from years, then from months.
	 */
	for (i = POSIX_BASE_YEAR; i < year; i++)
		tmp += days_in_year(i);
	if (leapyear(year) && dt->dt_month > FEBRUARY)
		tmp++;

	/* Months */
	for (i = 1; i < dt->dt_month; i++)
	  	tmp += days_in_month(i);
	tmp += (dt->dt_day - 1);

	/* Now do hours */
	tmp = tmp * 24 + dt->dt_hour;

	/* Now do minutes */
	tmp = tmp * 60 + dt->dt_min;

	/* Now do seconds */
	tmp = tmp * 60 + dt->dt_sec;

 out:
	*tp = tmp;
}

/*
 * Now routines to get and set clock as POSIX time.
 */

static long clk_get_secs()
{
	struct date_time dt;
	long gmt;

	clk_get_dt(&dt);
	dt_to_gmt(&dt, &gmt);
	return (gmt);
}

static void clk_set_secs(long secs)
{
	struct date_time dt;
	long gmt;

	gmt = secs;
	gmt_to_dt(&gmt, &dt);
	clk_set_dt(&dt);
}


#ifdef	DEBUG
/* Call this from DDB or whatever... */
int clkdebug()
{
	struct date_time dt;
	long gmt;
	long *lp;

	bzero((char*)&dt, sizeof(dt));
	clk_get_dt(&dt);
	lp = (long*)&dt;
	printf("clkdebug: dt=[%x,%x]\n", lp[0], lp[1]);

	dt_to_gmt(&dt, &gmt);
	printf("clkdebug: gmt=%x\n", gmt);
}
#endif
