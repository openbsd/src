/*	$OpenBSD: clock.c,v 1.13 2005/02/13 22:04:34 grange Exp $ */

/*
 * Copyright (c) 2001-2004 Opsycon AB  (www.opsycon.se / www.opsycon.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/evcount.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <mips64/dev/clockvar.h>
#include <mips64/archtype.h>

static struct evcount clk_count;
static int clk_irq = 5;

/* Definition of the driver for autoconfig. */
int	clockmatch(struct device *, void *, void *);
void	clockattach(struct device *, struct device *, void *);
intrmask_t clock_int5_dummy(intrmask_t, struct trap_frame *);
intrmask_t clock_int5(intrmask_t, struct trap_frame *);
void clock_int5_init(struct clock_softc *);

struct cfdriver clock_cd = {
	NULL, "clock", DV_DULL, NULL, 0
};

struct cfattach clock_ca = {
	sizeof(struct clock_softc), clockmatch, clockattach
};

int	clock_started = 0;
u_int32_t cpu_counter_last;
u_int32_t cpu_counter_interval;
u_int32_t pendingticks;
u_int32_t ticktime;

#define	SECDAY	(24*SECHOUR)	/* seconds per day */
#define	SECYR	(365*SECDAY)	/* seconds per common year */
#define	SECMIN	(60)		/* seconds per minute */
#define	SECHOUR	(60*SECMIN)	/* seconds per hour */

#define	YEARDAYS(year)	(((((year) + 1900) % 4) == 0 && \
			 ((((year) + 1900) % 100) != 0 || \
			  (((year) + 1900) % 400) == 0)) ? 366 : 365)

int
clockmatch(struct device *parent, void *cfdata, void *aux)
{
        struct confargs *ca = aux;
	struct cfdata *cf = cfdata;

        /* Make sure that we're looking for a clock. */
        if (strcmp(ca->ca_name, clock_cd.cd_name) != 0)
                return (0);

	if (cf->cf_unit >= 1)
		return 0;
	return 10;	/* Try to get clock early */
}

void
clockattach(struct device *parent, struct device *self, void *aux)
{
	struct clock_softc *sc;

	md_clk_attach(parent, self, aux);
	sc = (struct clock_softc *)self;

	switch (sys_config.system_type) {
	case ALGOR_P4032:
	case ALGOR_P5064:
	case MOMENTUM_CP7000:
	case MOMENTUM_CP7000G:
	case MOMENTUM_JAGUAR:
	case GALILEO_EV64240:
	case SGI_INDY:
	case SGI_O2:
	case SGI_O200:
		printf(" ticker on int5 using count register.");
		set_intr(INTPRI_CLOCK, CR_INT_5, clock_int5);
		ticktime = sys_config.cpu[0].clock / 2000;
		break;

	default:
		panic("clockattach: it didn't get here.  really.");
	}

	printf("\n");
}

/*
 *	Clock interrupt code for machines using the on cpu chip
 *	counter register. This register counts at half the pipeline
 *	frequency so the frequency must be known and the options
 *	register wired to allow it's use.
 *
 *	The code is enabled by setting 'cpu_counter_interval'.
 */
void
clock_int5_init(struct clock_softc *sc)
{
        int s;

        s = splclock();
        cpu_counter_interval = sys_config.cpu[0].clock / (hz * 2);
        cpu_counter_last = cp0_get_count() + cpu_counter_interval * 4;
        cp0_set_compare(cpu_counter_last);
        splx(s);
}

/*
 *  Dummy count register interrupt handler used on some targets.
 *  Just resets the compare register and acknowledge the interrupt.
 */
intrmask_t
clock_int5_dummy(intrmask_t mask, struct trap_frame *tf)
{
        cp0_set_compare(0);      /* Shut up counter int's for a while */
	return CR_INT_5;	/* Clock is always on 5 */
}

/*
 *  Interrupt handler for targets using the internal count register
 *  as interval clock. Normally the system is run with the clock
 *  interrupt always enabled. Masking is done here and if the clock
 *  can not be run the tick is just counted and handled later when
 *  the clock is unmasked again.
 */
intrmask_t
clock_int5(intrmask_t mask, struct trap_frame *tf)
{
	u_int32_t clkdiff;

	/*
	 * If clock is started count the tick, else just arm for a new.
	 */
	if (clock_started && cpu_counter_interval != 0) {
		clkdiff = cp0_get_count() - cpu_counter_last;
		while (clkdiff >= cpu_counter_interval) {
			cpu_counter_last += cpu_counter_interval;
			clkdiff = cp0_get_count() - cpu_counter_last;
			pendingticks++;
		}
		cpu_counter_last += cpu_counter_interval;
		pendingticks++;
	} else {
		cpu_counter_last = cpu_counter_interval + cp0_get_count();
	}

	cp0_set_compare(cpu_counter_last);
	/* Make sure that next clock tick has not passed */
	clkdiff = cp0_get_count() - cpu_counter_last;
	if (clkdiff > 0) {
		cpu_counter_last += cpu_counter_interval;
		pendingticks++;
		cp0_set_compare(cpu_counter_last);
	}

	if ((tf->cpl & SPL_CLOCKMASK) == 0) {
		while (pendingticks) {
			clk_count.ec_count++;
			hardclock(tf);
			pendingticks--;
		}
	}

	return CR_INT_5;	/* Clock is always on 5 */
}

/*
 * Wait "n" microseconds.
 */
void
delay(int n)
{
	int dly;
	int p, c;

	p = cp0_get_count();
	dly = (sys_config.cpu[0].clock / 1000000) * n / 2;
	while (dly > 0) {
		c = cp0_get_count();
		dly -= c - p;
		p = c;
	}
}

/*
 * Wait "n" nanoseconds.
 */
void
nanodelay(int n)
{
	int dly;
	int p, c;

	p = cp0_get_count();
	dly = ((sys_config.cpu[0].clock * n) / 1000000000) / 2;
	while (dly > 0) {
		c = cp0_get_count();
		dly -= c - p;
		p = c;
	}
}

/*
 * Return the best possible estimate of the time in the timeval
 * to which tvp points.  We guarantee that the time will be greater
 * than the value obtained by a previous call.
 */
void
microtime(struct timeval *tvp)
{
	static struct timeval lasttime;
	u_int32_t clkdiff;
	int s = splclock();

	*tvp = time;
	clkdiff = (cp0_get_count() - cpu_counter_last) * 1000;
	tvp->tv_usec += clkdiff / ticktime;
	while (tvp->tv_usec >= 1000000) {
		tvp->tv_sec++;
		tvp->tv_usec -= 1000000;
	}

	if (tvp->tv_sec == lasttime.tv_sec &&
	    tvp->tv_usec <= lasttime.tv_usec) {
		tvp->tv_usec++;
		if (tvp->tv_usec >= 1000000) {
			tvp->tv_sec++;
			tvp->tv_usec -= 1000000;
		}
	}
	lasttime = *tvp;
	splx(s);
}

/*
 *	Mips machine independent clock routines.
 */

/*
 * Start the real-time and statistics clocks. Leave stathz 0 since there
 * are no other timers available.
 */
void
cpu_initclocks()
{
	struct clock_softc *sc = (struct clock_softc *)clock_cd.cd_devs[0];

	hz = sc->sc_clock.clk_hz;
	stathz = sc->sc_clock.clk_stathz;
	profhz = sc->sc_clock.clk_profhz;

	evcount_attach(&clk_count, "clock", (void *)&clk_irq, &evcount_intr);

	/* Start the clock.  */
	if (sc->sc_clock.clk_init != NULL)
		(*sc->sc_clock.clk_init)(sc);

	tick = 1000000 / hz;	/* number of micro-seconds between interrupts */
	tickadj = 240000 / (60 * hz);           /* can adjust 240ms in 60s */

	clock_started++;
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
}

/*
 * This code is defunct after 2099. Will Unix still be here then??
 */
static short dayyr[12] = {
	0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
};

/*
 * Initialize the time of day register, based on the time base which
 * is, e.g. from a filesystem.
 */
void
inittodr(time_t base)
{
	struct tod_time c;
	struct clock_softc *sc = (struct clock_softc *)clock_cd.cd_devs[0];
	int days, yr;

	if (base < 15*SECYR) {
		printf("WARNING: preposterous time in file system");
		/* read the system clock anyway */
		base = 17*SECYR + 186*SECDAY + SECDAY/2;
	}

	/*
	 * Read RTC chip registers NOTE: Read routines are responsible
	 * for sanity checking clock. Dates after 19991231 should be
	 * returned as year >= 100.
	 */
	if (sc->sc_clock.clk_get) {
		(*sc->sc_clock.clk_get)(sc, base, &c);
	} else {
		printf("WARNING: No TOD clock, believing file system.\n");
		goto bad;
	}

	days = 0;
	for (yr = 70; yr < c.year; yr++) {
		days += YEARDAYS(yr);
	}

	days += dayyr[c.mon - 1] + c.day - 1;
	if (YEARDAYS(c.year) == 366 && c.mon > 2) {
		days++;
	}

	/* now have days since Jan 1, 1970; the rest is easy... */
	time.tv_sec = days * SECDAY + c.hour * 3600 + c.min * 60 + c.sec;
	sc->sc_initted = 1;

	/*
	 * See if we gained/lost time.
	 */
	if (base < time.tv_sec - 5*SECYR) {
		printf("WARNING: file system time much less than clock time\n");
	} else if (base > time.tv_sec + 5*SECYR) {
		printf("WARNING: clock time much less than file system time\n");
		printf("WARNING: using file system time\n");
	} else {
		return;
	}

bad:
	time.tv_sec = base;
	sc->sc_initted = 1;
	printf("WARNING: CHECK AND RESET THE DATE!\n");
}

/*
 * Reset the TOD clock. This is done when the system is halted or
 * when the time is reset by the stime system call.
 */
void
resettodr()
{
	struct tod_time c;
	struct clock_softc *sc = (struct clock_softc *)clock_cd.cd_devs[0];
	register int t, t2;

	/*
	 *  Don't reset clock if time has not been set!
	 */
	if (!sc->sc_initted) {
		return;
	}

	/* compute the day of week. 1 is Sunday*/
	t2 = time.tv_sec / SECDAY;
	c.dow = (t2 + 5) % 7 + 1;	/* 1/1/1970 was thursday */

	/* compute the year */
	t2 = time.tv_sec / SECDAY;
	c.year = 69;
	while (t2 >= 0) {	/* whittle off years */
		t = t2;
		c.year++;
		t2 -= YEARDAYS(c.year);
	}

	/* t = month + day; separate */
	t2 = YEARDAYS(c.year);
	for (c.mon = 1; c.mon < 12; c.mon++) {
		if (t < dayyr[c.mon] + (t2 == 366 && c.mon > 1))
			break;
	}

	c.day = t - dayyr[c.mon - 1] + 1;
	if (t2 == 366 && c.mon > 2) {
		c.day--;
	}

	t = time.tv_sec % SECDAY;
	c.hour = t / 3600;
	t %= 3600;
	c.min = t / 60;
	c.sec = t % 60;

	if (sc->sc_clock.clk_set) {
		(*sc->sc_clock.clk_set)(sc, &c);
	}
}
