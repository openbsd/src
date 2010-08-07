/* $OpenBSD: gptimer.c,v 1.4 2010/08/07 03:50:01 krw Exp $ */
/*
 * Copyright (c) 2007,2009 Dale Rahn <drahn@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 *	WARNING - this timer initializion has not been checked
 *	to see if it will do _ANYTHING_ sane if the omap enters
 *	low power mode.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/time.h>
#include <sys/evcount.h>
#include <sys/device.h>
#include <sys/timetc.h>
#include <dev/clock_subr.h>
#include <beagle/dev/prcmvar.h>
#include <machine/bus.h>
#include <arch/beagle/beagle/ahb.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <arm/cpufunc.h>

/* registers */
#define	GP_TIDR		0x000
#define		GP_TIDR_REV	0xff
#define GP_TIOCP_CFG	0x010
#define 	GP_TIOCP_CFG_CLKA	0x000000300
#define 	GP_TIOCP_CFG_EMUFREE	0x000000020
#define 	GP_TIOCP_CFG_IDLEMODE	0x000000018
#define 	GP_TIOCP_CFG_ENAPWAKEUP	0x000000004
#define 	GP_TIOCP_CFG_SOFTRESET	0x000000002
#define 	GP_TIOCP_CFG_AUTOIDLE	0x000000001
#define	GP_TISTAT	0x014
#define 	GP_TISTAT_RESETDONE	0x000000001
#define	GP_TISR		0x018
#define		GP_TISTAT_TCAR		0x00000004
#define		GP_TISTAT_OVF		0x00000002
#define		GP_TISTAT_MATCH		0x00000001
#define GP_TIER		0x1c
#define		GP_TIER_TCAR_EN		0x4
#define		GP_TIER_OVF_EN		0x2
#define		GP_TIER_MAT_EN		0x1
#define	GP_TWER		0x020
#define		GP_TWER_TCAR_EN		0x00000004
#define		GP_TWER_OVF_EN		0x00000002
#define		GP_TWER_MAT_EN		0x00000001
#define	GP_TCLR		0x024
#define		GP_TCLR_GPO		(1<<14)
#define		GP_TCLR_CAPT		(1<<13)
#define		GP_TCLR_PT		(1<<12)
#define		GP_TCLR_TRG		(3<<10)
#define		GP_TCLR_TRG_O		(1<<10)
#define		GP_TCLR_TRG_OM		(2<<10)
#define		GP_TCLR_TCM		(3<<8)
#define		GP_TCLR_TCM_RISE	(1<<8)
#define		GP_TCLR_TCM_FALL	(2<<8)
#define		GP_TCLR_TCM_BOTH	(3<<8)
#define		GP_TCLR_SCPWM		(1<<7)
#define		GP_TCLR_CE		(1<<6)
#define		GP_TCLR_PRE		(1<<5)
#define		GP_TCLR_PTV		(7<<2)
#define		GP_TCLR_AR		(1<<1)
#define		GP_TCLR_ST		(1<<0)
#define	GP_TCRR		0x028				/* counter */
#define	GP_TLDR		0x02c				/* reload */
#define	GP_TTGR		0x030
#define	GP_TWPS		0x034
#define		GP_TWPS_TCLR	0x01
#define		GP_TWPS_TCRR	0x02
#define		GP_TWPS_TLDR	0x04
#define		GP_TWPS_TTGR	0x08
#define		GP_TWPS_TMAR	0x10
#define		GP_TWPS_ALL	0x1f
#define	GP_TMAR		0x038
#define	GP_TCAR		0x03C
#define	GP_TSICR	0x040
#define		GP_TSICR_POSTED		0x00000002
#define		GP_TSICR_SFT		0x00000001
#define	GP_TCAR2	0x044
#define	GP_SIZE	0x100


#define TIMER_FREQUENCY			32768	/* 32kHz is used, selectable */

static struct evcount clk_count;
static struct evcount stat_count;
#define GPT1_IRQ  38
#define GPTIMER0_IRQ	38

//static int clk_irq = GPT1_IRQ; /* XXX 37 */

int gptimer_match(struct device *parent, void *v, void *aux);
void gptimer_attach(struct device *parent, struct device *self, void *args);
int gptimer_intr(void *frame);
void gptimer_delay(int reg);

bus_space_tag_t gptimer_iot;
bus_space_handle_t gptimer_ioh0,  gptimer_ioh1; 
int gptimer_irq = 0;

u_int gptimer_get_timecount(struct timecounter *);

static struct timecounter gptimer_timecounter = {
	gptimer_get_timecount, NULL, 0x7fffffff, 0, "gptimer", 0, NULL
};

volatile u_int32_t nexttickevent;
volatile u_int32_t nextstatevent;
u_int32_t	ticks_per_second;
u_int32_t	ticks_per_intr;
u_int32_t	ticks_err_cnt;
u_int32_t	ticks_err_sum;
u_int32_t	statvar, statmin;

struct cfattach	gptimer_ca = {
	sizeof (struct device), gptimer_match, gptimer_attach
};

struct cfdriver gptimer_cd = {
	NULL, "gptimer", DV_DULL
};

int
gptimer_match(struct device *parent, void *v, void *aux)
{
	return (1);
}

void
gptimer_attach(struct device *parent, struct device *self, void *args)
{
        struct ahb_attach_args *aa = args;
	bus_space_handle_t ioh; 
	u_int32_t rev;

	gptimer_iot = aa->aa_iot;
	if (bus_space_map(gptimer_iot, aa->aa_addr, GP_SIZE, 0, &ioh))
		panic("gptimer_attach: bus_space_map failed!");

	rev = bus_space_read_4(gptimer_iot, ioh, GP_TIDR);

	printf(" rev %d.%d\n", rev >> 4 & 0xf, rev & 0xf);
	if (self->dv_unit == 0) {
		gptimer_ioh0 = ioh;
		gptimer_irq = aa->aa_intr;
		bus_space_write_4(gptimer_iot, gptimer_ioh0, GP_TCLR, 0);
	} else if (self->dv_unit == 1) {
		/* start timer because it is used in delay */
		gptimer_ioh1 = ioh;
		bus_space_write_4(gptimer_iot, gptimer_ioh1, GP_TCRR, 0);
		gptimer_delay(GP_TWPS_ALL);
		bus_space_write_4(gptimer_iot, gptimer_ioh1, GP_TLDR, 0);
		gptimer_delay(GP_TWPS_ALL);
		bus_space_write_4(gptimer_iot, gptimer_ioh1, GP_TCLR,
		    GP_TCLR_AR | GP_TCLR_ST);
		gptimer_delay(GP_TWPS_ALL);

		gptimer_timecounter.tc_frequency = TIMER_FREQUENCY;
		tc_init(&gptimer_timecounter);
	}
	else 
		panic("attaching too many gptimers at %x", aa->aa_addr);
}

/* 
 * See comment in arm/xscale/i80321_clock.c
 *
 * counter is count up, but with autoreload timers it is not possible
 * to detect how many  interrupts passed while interrupts were blocked.
 * also it is not possible to atomically add to the register
 * get get it to precisely fire at a non-fixed interval.
 *
 * To work around this two timers are used, GPT1 is used as a reference
 * clock without reload , however we just ignore the interrupt it
 * would (may?) generate.
 *
 * Internally this keeps track of when the next timer should fire
 * and based on that time and the current value of the reference
 * clock a number is written into the timer count register to schedule
 * the next event.
 */

int
gptimer_intr(void *frame)
{
	u_int32_t now, r;
	u_int32_t nextevent, duration;

	/* clear interrupt */
	now = bus_space_read_4(gptimer_iot, gptimer_ioh1, GP_TCRR);

	while ((int32_t) (nexttickevent - now) < 0) {
		nexttickevent += ticks_per_intr;
		ticks_err_sum += ticks_err_cnt;
#if 0
		if (ticks_err_sum  > hz) {
			u_int32_t match_error;
			match_error = ticks_err_sum / hz
			ticks_err_sum -= (match_error * hz);
		}
#else
		/* looping a few times is faster than divide */
		while (ticks_err_sum  > hz) {
			nexttickevent += 1;
			ticks_err_sum -= hz;
		}
#endif
		clk_count.ec_count++;
		hardclock(frame);
	}
	while ((int32_t) (nextstatevent - now) < 0) {
		do {
			r = random() & (statvar -1);
		} while (r == 0); /* random == 0 not allowed */
		nextstatevent += statmin + r;
		/* XXX - correct nextstatevent? */
		stat_count.ec_count++;
		statclock(frame);
	}
        if ((now - nexttickevent) < (now - nextstatevent))
                nextevent = nexttickevent;
        else
                nextevent = nextstatevent;

/* XXX */
	duration = nextevent -
	    bus_space_read_4(gptimer_iot, gptimer_ioh1, GP_TCRR);
#if 0
	printf("duration 0x%x %x %x\n", nextevent -
	    bus_space_read_4(gptimer_iot, gptimer_ioh1, GP_TCRR),
	    bus_space_read_4(gptimer_iot, gptimer_ioh0, GP_TCRR),
	    bus_space_read_4(gptimer_iot, gptimer_ioh1, GP_TCRR));
#endif


        if (duration <= 0)
                duration = 1; /* trigger immediately. */

        if (duration > ticks_per_intr) {
                /*
                 * If interrupts are blocked too long, like during
                 * the root prompt or ddb, the timer can roll over,
                 * this will allow the system to continue to run
                 * even if time is lost.
                 */
                duration = ticks_per_intr;
                nexttickevent = now;
                nextstatevent = now;
        }

	gptimer_delay(GP_TWPS_ALL);
	bus_space_write_4(gptimer_iot, gptimer_ioh0, GP_TISR,
		bus_space_read_4(gptimer_iot, gptimer_ioh0, GP_TISR));
	gptimer_delay(GP_TWPS_ALL);
        bus_space_write_4(gptimer_iot, gptimer_ioh0, GP_TCRR, -duration);
 
	return 1;
}

/*
 * would be interesting to play with trigger mode while having one timer
 * in 32KHz mode, and the other timer running in sysclk mode and use
 * the high resolution speeds (matters more for delay than tick timer
 */

void
cpu_initclocks()
{
//	u_int32_t now;
	stathz = 128;
	profhz = 1024;

	ticks_per_second = TIMER_FREQUENCY;

	setstatclockrate(stathz);

	ticks_per_intr = ticks_per_second / hz;
	ticks_err_cnt = ticks_per_second % hz;
	ticks_err_sum = 0;; 

	prcm_setclock(1, PRCM_CLK_SPEED_32);
	prcm_setclock(2, PRCM_CLK_SPEED_32);
	/* establish interrupts */
	intc_intr_establish(gptimer_irq, IPL_CLOCK, gptimer_intr,
	    NULL, "tick");

	/* setup timer 0 (hardware timer 2) */
	/* reset? - XXX */

        bus_space_write_4(gptimer_iot, gptimer_ioh0, GP_TLDR, 0);

	nexttickevent = nextstatevent = bus_space_read_4(gptimer_iot,
	    gptimer_ioh1, GP_TCRR) + ticks_per_intr;

	gptimer_delay(GP_TWPS_ALL);
	bus_space_write_4(gptimer_iot, gptimer_ioh0, GP_TIER, GP_TIER_OVF_EN);
	gptimer_delay(GP_TWPS_ALL);
	bus_space_write_4(gptimer_iot, gptimer_ioh0, GP_TWER, GP_TWER_OVF_EN);
	gptimer_delay(GP_TWPS_ALL);
	bus_space_write_4(gptimer_iot, gptimer_ioh0, GP_TCLR,
	    GP_TCLR_AR | GP_TCLR_ST);
	gptimer_delay(GP_TWPS_ALL);
	bus_space_write_4(gptimer_iot, gptimer_ioh0, GP_TISR,
		bus_space_read_4(gptimer_iot, gptimer_ioh0, GP_TISR));
	gptimer_delay(GP_TWPS_ALL);
	bus_space_write_4(gptimer_iot, gptimer_ioh0, GP_TCRR, -ticks_per_intr);
	gptimer_delay(GP_TWPS_ALL);
}

void
gptimer_delay(int reg)
{
	while (bus_space_read_4(gptimer_iot, gptimer_ioh0, GP_TWPS) & reg)
		;
}

#if 0
void
microtime(struct timeval *tvp)
{
	int s;
	int deltacnt;
	u_int32_t counter, expected;
	s = splhigh();

	if (1) {	/* not inited */
		tvp->tv_sec = 0;
		tvp->tv_usec = 0;
		return;
	}
	s = splhigh();
	counter = bus_space_read_4(gptimer_iot, gptimer_ioh1, GP_TCRR);
	expected = nexttickevent;

	*tvp = time;
	splx(s);

	deltacnt = counter - expected + ticks_per_intr;

#if 1
	/* low frequency timer algorithm */
	tvp->tv_usec += deltacnt * 1000000ULL / TIMER_FREQUENCY;
#else
	/* high frequency timer algorithm - XXX */
	tvp->tv_usec += deltacnt / (TIMER_FREQUENCY / 1000000ULL);
#endif

	while (tvp->tv_usec >= 1000000) {
		tvp->tv_sec++;
		tvp->tv_usec -= 1000000;
	}

}
#endif

void
delay(u_int usecs)
{
	u_int32_t clock, oclock, delta, delaycnt;
	volatile int j;
	int csec, usec;

	if (usecs > (0x80000000 / (TIMER_FREQUENCY))) {
		csec = usecs / 10000;
		usec = usecs % 10000;

		delaycnt = (TIMER_FREQUENCY / 100) * csec +
		    (TIMER_FREQUENCY / 100) * usec / 10000;
	} else {
		delaycnt = TIMER_FREQUENCY * usecs / 1000000;
	}
	if (delaycnt <= 1)
		for (j = 100; j > 0; j--)
			;

	if (gptimer_ioh1 == NULL) {
		/* BAH */
		for (; usecs > 0; usecs--)
			for (j = 100; j > 0; j--)
				;
		return;
	}
	oclock = bus_space_read_4(gptimer_iot, gptimer_ioh1, GP_TCRR);
	while (1) {
		for (j = 100; j > 0; j--)
			;
		clock = bus_space_read_4(gptimer_iot, gptimer_ioh1, GP_TCRR);
		delta = clock - oclock;
		if (delta > delaycnt)
			break;
	}
	
}

void
setstatclockrate(int newhz)
{
	int minint, statint;
	int s;
	
	s = splclock();

	statint = ticks_per_second / newhz;
	/* calculate largest 2^n which is smaller that just over half statint */
	statvar = 0x40000000; /* really big power of two */
	minint = statint / 2 + 100;
	while (statvar > minint)
		statvar >>= 1;

	statmin = statint - (statvar >> 1);
	
	splx(s);

	/*
	 * XXX this allows the next stat timer to occur then it switches
	 * to the new frequency. Rather than switching instantly.
	 */
}

todr_chip_handle_t todr_handle;

/*
 * inittodr:
 *
 *      Initialize time from the time-of-day register.
 */
#define MINYEAR         2003    /* minimum plausible year */
void
inittodr(time_t base)
{
	time_t deltat;
	struct timeval rtctime;
	struct timespec ts;
	int badbase;

	if (base < (MINYEAR - 1970) * SECYR) {
		printf("WARNING: preposterous time in file system\n");
		/* read the system clock anyway */
		base = (MINYEAR - 1970) * SECYR;
		badbase = 1;
	} else
		badbase = 0;

	if (todr_handle == NULL ||
	    todr_gettime(todr_handle, &rtctime) != 0 ||
	    rtctime.tv_sec == 0) {
		/*
		 * Believe the time in the file system for lack of
		 * anything better, resetting the TODR.
		 */
		rtctime.tv_sec = base;
		rtctime.tv_usec = 0;
		if (todr_handle != NULL && !badbase) {
			printf("WARNING: preposterous clock chip time\n");
			resettodr();
		}
		goto bad;
	} else {
		ts.tv_sec = rtctime.tv_sec;
		ts.tv_nsec = rtctime.tv_usec * 1000;
		tc_setclock(&ts);
	}

	if (!badbase) {
		/*
		 * See if we gained/lost two or more days; if
		 * so, assume something is amiss.
		 */
		deltat = rtctime.tv_sec - base;
		if (deltat < 0)
			deltat = -deltat;
		if (deltat < 2 * SECDAY)
			return;         /* all is well */
		printf("WARNING: clock %s %ld days\n",
		    rtctime.tv_sec < base ? "lost" : "gained",
		    (long)deltat / SECDAY);
	}
 bad:
	printf("WARNING: CHECK AND RESET THE DATE!\n");
}


/*
 * resettodr:
 *
 *      Reset the time-of-day register with the current time.
 */
void
resettodr(void)
{
	struct timeval rtctime;

	if (rtctime.tv_sec == 0)
		return;
			
	microtime(&rtctime);

	if (todr_handle != NULL &&
	   todr_settime(todr_handle, &rtctime) != 0)
		printf("resettodr: failed to set time\n");
}

u_int
gptimer_get_timecount(struct timecounter *tc)
{
	return bus_space_read_4(gptimer_iot, gptimer_ioh1, GP_TCRR);
}
