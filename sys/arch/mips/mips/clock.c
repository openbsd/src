/*	$OpenBSD: clock.c,v 1.6 1998/10/15 21:30:15 imp Exp $	*/
/*
 * Copyright (c) 1997 Per Fogelstrom.
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
 *	from: @(#)clock.c	8.1 (Berkeley) 6/10/93
 *      $Id: clock.c,v 1.6 1998/10/15 21:30:15 imp Exp $
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <mips/dev/clockvar.h>
#include <mips/archtype.h>

#ifdef arc
#include <dev/isa/isavar.h>
#include <arc/isa/isa_machdep.h>
#endif

int	clock_started = 0;

/* Definition of the driver for autoconfig. */
static int	clockmatch __P((struct device *, void *, void *));
static void	clockattach __P((struct device *, struct device *, void *));

void delay __P((int));

struct cfdriver clock_cd = {
	NULL, "clock", DV_DULL, NULL, 0
};

#ifdef arc
struct cfattach clock_isa_ca = {
	sizeof(struct clock_softc), clockmatch, clockattach
};

struct cfattach clock_pica_ca = {
	sizeof(struct clock_softc), clockmatch, clockattach
};

struct cfattach clock_algor_ca = {
	sizeof(struct clock_softc), clockmatch, clockattach
};
static int int5_dummy __P((unsigned mask, struct clockframe *cf));
#endif

#ifdef sgi
struct cfattach clock_indy_ca = {
	sizeof(struct clock_softc), clockmatch, clockattach
};
#endif

void	md_clk_attach __P((struct device *, struct device *, void *));
int	clockintr __P((void *));

#define	SECMIN	((unsigned)60)			/* seconds per minute */
#define	SECHOUR	((unsigned)(60*SECMIN))		/* seconds per hour */
#define	SECDAY	((unsigned)(24*SECHOUR))	/* seconds per day */
#define	SECYR	((unsigned)(365*SECDAY))	/* seconds per common year */

#define	LEAPYEAR(year)	(((year) % 4) == 0)

static int
clockmatch(parent, cfdata, aux)
	struct device *parent;
	void *cfdata;
	void *aux;
{
	struct cfdata *cf;
	struct confargs *ca;

	cf = cfdata;
	ca = aux;
	/* See how many clocks this system has */	
	switch (system_type) {

#ifdef arc
	case ACER_PICA_61:
	case MAGNUM:
		/* make sure that we're looking for this type of device. */
		if (!BUS_MATCHNAME(ca, "dallas_rtc"))
			return (0);

		break;

	case DESKSTATION_RPC44:
	case DESKSTATION_TYNE:
	case ALGOR_P4032:
	case ALGOR_P5064:
		break;

#endif

#ifdef sgi
	case SGI_INDY:
		break;
#endif
	default:
		panic("clockmatch unknown CPU");
	}

	if (cf->cf_unit >= 1)
		return (0);

	return (1);
}

static void
clockattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct isa_attach_args *ia;

	ia = aux;
	md_clk_attach(parent, self, aux);

	switch (system_type) {

#ifdef arc
	case ACER_PICA_61:
	case MAGNUM:
	case ALGOR_P4032:
	case ALGOR_P5064:
		BUS_INTR_ESTABLISH((struct confargs *)aux,
			(intr_handler_t)hardclock, self);
		set_intr(SOFT_INT_MASK_0 << 7, int5_dummy, 7);
		break;

	case DESKSTATION_RPC44:
	case DESKSTATION_TYNE:
		(void)isa_intr_establish(ia->ia_ic,
				0, 1, 3, clockintr, 0, "clock");
		set_intr(SOFT_INT_MASK_0 << 7, int5_dummy, 7);
		break;
#endif

#ifdef sgi
	case SGI_INDY:
		BUS_INTR_ESTABLISH((struct confargs *)aux,
			(intr_handler_t)hardclock, self);
		break;
#endif

	default:
		panic("clockattach: it didn't get here.  really.");
	}

	printf("\n");
}

#ifdef arc
/*
 *   This code is a stub to take care of the unused int 5 in arc's.
 *   We may in the future use this for high precision timing...
 */
static int
int5_dummy(mask, cf)
	unsigned mask;
	struct clockframe *cf;
{
	R4K_SetCOMPARE(0);	/* Shut up counter int's for a while */
	return(~0);
}
#endif

/*
 * Wait "n" microseconds.  This doesn't belong here.  XXX.
 */
void
delay(n)
	int n;
{
	DELAY(n);
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
	extern int tickadj;
	struct clock_softc *csc = (struct clock_softc *)clock_cd.cd_devs[0];


	/*  Assume 100 Hz */
	hz = 100;

	/* Start the clock.  */
	(*csc->sc_init)(csc);

	/* Recalculate theese if clock init changed hz */
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
 * This code is defunct after 2099.
 * Will Unix still be here then??
 */
static short dayyr[12] = {
	0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
};

/*
 * Initialize the time of day register, based on the time base which
 * is, e.g. from a filesystem.  Base provides the time to within six
 * months, and the time of year clock (if any) provides the rest.
 */
void
inittodr(base)
	time_t base;
{
	struct tod_time c;
	struct clock_softc *csc = (struct clock_softc *)clock_cd.cd_devs[0];
	register int days, yr;
	long deltat;
	int badbase;

	if (base < 5*SECYR) {
		printf("WARNING: preposterous time in file system");
		/* read the system clock anyway */
		base = 6*SECYR + 186*SECDAY + SECDAY/2;
		badbase = 1;
	} else
		badbase = 0;

	/* Read RTC chip registers */ 
	(*csc->sc_get)(csc, base, &c);

	csc->sc_initted = 1;

	/* simple sanity checks */
	if (c.year < 70 || c.mon < 1 || c.mon > 12 || c.day < 1 ||
	    c.day > 31 || c.hour > 23 || c.min > 59 || c.sec > 59) {
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
	for (yr = 70; yr < c.year; yr++)
		days += LEAPYEAR(yr) ? 366 : 365;
	days += dayyr[c.mon - 1] + c.day - 1;
	if (LEAPYEAR(yr) && c.mon > 2)
		days++;
	/* now have days since Jan 1, 1970; the rest is easy... */
	time.tv_sec = days * SECDAY + c.hour * 3600 + c.min * 60 + c.sec;

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
 * Reset the TODR based on the time value; used when the TODR has a
 * preposterous value and also when the time is reset by the stime
 * system call.
 * Also called when the TODR goes past TODRZERO + 100*(SECYEAR+2*SECDAY)
 * (e.g. on Jan 2 just after midnight) to wrap the TODR around.
 */
void
resettodr()
{
	struct tod_time c;
	struct clock_softc *csc = (struct clock_softc *)clock_cd.cd_devs[0];
	register int t, t2;

	if(!csc->sc_initted)
		return;

	/* compute the day of week. 1 is Sunday*/
	t2 = time.tv_sec / SECDAY;
	c.dow = (t2 + 5) % 7;	/* 1/1/1970 was thursday */

	/* compute the year */
	t2 = time.tv_sec / SECDAY;
	c.year = 69;
	while (t2 >= 0) {	/* whittle off years */
		t = t2;
		c.year++;
		t2 -= LEAPYEAR(c.year) ? 366 : 365;
	}

	/* t = month + day; separate */
	t2 = LEAPYEAR(c.year);
	for (c.mon = 1; c.mon < 12; c.mon++)
		if (t < dayyr[c.mon] + (t2 && c.mon > 1))
			break;

	c.day = t - dayyr[c.mon - 1] + 1;
	if (t2 && c.mon > 2)
		c.day--;

	/* the rest is easy */
	t = time.tv_sec % SECDAY;
	c.hour = t / 3600;
	t %= 3600;
	c.min = t / 60;
	c.sec = t % 60;

	(*csc->sc_set)(csc, &c);
}
