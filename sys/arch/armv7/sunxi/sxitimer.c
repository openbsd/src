/*	$OpenBSD: sxitimer.c,v 1.12 2018/06/04 09:25:16 kettenis Exp $	*/
/*
 * Copyright (c) 2007,2009 Dale Rahn <drahn@openbsd.org>
 * Copyright (c) 2013 Raphael Graf <r@undefined.ch>
 * Copyright (c) 2013 Artturi Alm
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/time.h>
#include <sys/evcount.h>
#include <sys/device.h>
#include <sys/timetc.h>
#include <dev/clock_subr.h>

#include <arm/cpufunc.h>

#include <machine/bus.h>
#include <machine/fdt.h>
#include <machine/intr.h>

#include <dev/fdt/sunxireg.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/fdt.h>

#define	TIMER_IER 		0x00
#define	TIMER_ISR 		0x04
#define	TIMER_IRQ(x)		(1 << (x))

#define	TIMER_CTRL(x)		(0x10 + (0x10 * (x)))
#define	TIMER_INTV(x)		(0x14 + (0x10 * (x)))
#define	TIMER_CURR(x)		(0x18 + (0x10 * (x)))

/* A1X counter */
#define	CNT64_CTRL		0xa0
#define	CNT64_LOW		0xa4
#define	CNT64_HIGH		0xa8

#define	CNT64_CLR_EN		(1 << 0) /* clear enable */
#define	CNT64_RL_EN		(1 << 1) /* read latch enable */

#define	TIMER_ENABLE		(1 << 0)
#define	TIMER_RELOAD		(1 << 1)
#define	TIMER_CLK_SRC_MASK	(3 << 2)
#define	TIMER_OSC24M		(1 << 2)
#define	TIMER_PLL6_6		(2 << 2)
#define	TIMER_PRESC_1		(0 << 4)
#define	TIMER_PRESC_2		(1 << 4)
#define	TIMER_PRESC_4		(2 << 4)
#define	TIMER_PRESC_8		(3 << 4)
#define	TIMER_PRESC_16		(4 << 4)
#define	TIMER_PRESC_32		(5 << 4)
#define	TIMER_PRESC_64		(6 << 4)
#define	TIMER_PRESC_128		(7 << 4)
#define	TIMER_CONTINOUS		(0 << 7)
#define	TIMER_SINGLESHOT	(1 << 7)

#define	TICKTIMER		0
#define	STATTIMER		1
#define	CNTRTIMER		2

#define TIMER_SYNC		3

int	sxitimer_match(struct device *, void *, void *);
void	sxitimer_attach(struct device *, struct device *, void *);
int	sxitimer_tickintr(void *);
int	sxitimer_statintr(void *);
void	sxitimer_cpu_initclocks(void);
void	sxitimer_setstatclockrate(int);
uint64_t	sxitimer_readcnt64(void);
uint32_t	sxitimer_readcnt32(void);
void	sxitimer_sync(void);
void	sxitimer_delay(u_int);

u_int sxitimer_get_timecount(struct timecounter *);

static struct timecounter sxitimer_timecounter = {
	sxitimer_get_timecount, NULL, 0xffffffff, 0, "sxitimer", 0, NULL
};

bus_space_tag_t		sxitimer_iot;
bus_space_handle_t	sxitimer_ioh;

uint32_t sxitimer_freq[] = {
	TIMER0_FREQUENCY,
	TIMER1_FREQUENCY,
	TIMER2_FREQUENCY,
	0
};

uint32_t sxitimer_irq[] = {
	TIMER0_IRQ,
	TIMER1_IRQ,
	TIMER2_IRQ,
	0
};

uint32_t sxitimer_stat_tpi, sxitimer_tick_tpi;
uint32_t sxitimer_statvar, sxitimer_statmin;
uint32_t sxitimer_tick_nextevt, sxitimer_stat_nextevt;
uint32_t sxitimer_ticks_err_cnt, sxitimer_ticks_err_sum;

struct sxitimer_softc {
	struct device		sc_dev;
};

struct cfattach sxitimer_ca = {
	sizeof (struct sxitimer_softc), sxitimer_match, sxitimer_attach
};

struct cfdriver sxitimer_cd = {
	NULL, "sxitimer", DV_DULL
};

int
sxitimer_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;
	int node;

	node = OF_finddevice("/");
	if (!OF_is_compatible(node, "allwinner,sun4i-a10") &&
	    !OF_is_compatible(node, "allwinner,sun5i-a10s") &&
	    !OF_is_compatible(node, "allwinner,sun5i-a13"))
		return 0;

	return OF_is_compatible(faa->fa_node, "allwinner,sun4i-a10-timer");
}

void
sxitimer_attach(struct device *parent, struct device *self, void *aux)
{
	struct fdt_attach_args *faa = aux;
	uint32_t freq, ival, now;

	KASSERT(faa->fa_nreg > 0);

	sxitimer_iot = faa->fa_iot;
	if (bus_space_map(sxitimer_iot, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sxitimer_ioh))
		panic("%s: bus_space_map failed!", __func__);

	/* clear counter, loop until ready */
	bus_space_write_4(sxitimer_iot, sxitimer_ioh, CNT64_CTRL,
	    CNT64_CLR_EN); /* XXX as a side-effect counter clk src=OSC24M */
	while (bus_space_read_4(sxitimer_iot, sxitimer_ioh, CNT64_CTRL)
	    & CNT64_CLR_EN)
		continue;

	/* timers are down-counters, from interval to 0 */
	now = 0xffffffff; /* known big value */
	freq = sxitimer_freq[TICKTIMER];

	/* stop timer, and set clk src */
	bus_space_write_4(sxitimer_iot, sxitimer_ioh,
	    TIMER_CTRL(TICKTIMER), TIMER_OSC24M);

	ival = sxitimer_tick_tpi = freq / hz;
	sxitimer_tick_nextevt = now - ival;

	sxitimer_ticks_err_cnt = freq % hz;
	sxitimer_ticks_err_sum = 0;

	bus_space_write_4(sxitimer_iot, sxitimer_ioh,
	    TIMER_INTV(TICKTIMER), ival);

	/* timers are down-counters, from interval to 0 */
	now = 0xffffffff; /* known big value */
	freq = sxitimer_freq[STATTIMER];

	/* stop timer, and set clk src */
	bus_space_write_4(sxitimer_iot, sxitimer_ioh,
	    TIMER_CTRL(STATTIMER), TIMER_OSC24M);

	/* 100/1000 or 128/1024 ? */
	stathz = 128;
	profhz = 1024;
	sxitimer_setstatclockrate(stathz);

	ival = sxitimer_stat_tpi = freq / stathz;
	sxitimer_stat_nextevt = now - ival;

	bus_space_write_4(sxitimer_iot, sxitimer_ioh,
	    TIMER_INTV(STATTIMER), ival);

	/* timers are down-counters, from interval to 0 */
	now = 0xffffffff; /* known big value */
	freq = sxitimer_freq[CNTRTIMER];

	/* stop timer, and set clk src */
	bus_space_write_4(sxitimer_iot, sxitimer_ioh,
	    TIMER_CTRL(CNTRTIMER), TIMER_OSC24M);

	ival = now;

	sxitimer_timecounter.tc_frequency = freq;
	tc_init(&sxitimer_timecounter);
	arm_clock_register(sxitimer_cpu_initclocks, sxitimer_delay,
	    sxitimer_setstatclockrate, NULL);

	printf(": cntrtimer @ %dKHz", freq / 1000);

	bus_space_write_4(sxitimer_iot, sxitimer_ioh,
	    TIMER_INTV(CNTRTIMER), ival);

	printf("\n");
}

/*
 * would be interesting to play with trigger mode while having one timer
 * in 32KHz mode, and the other timer running in sysclk mode and use
 * the high resolution speeds (matters more for delay than tick timer)
 */

void
sxitimer_cpu_initclocks(void)
{
	uint32_t isr, ier, ctrl;

	/* establish interrupts */
	arm_intr_establish(sxitimer_irq[TICKTIMER], IPL_CLOCK,
	    sxitimer_tickintr, NULL, "tick");
	arm_intr_establish(sxitimer_irq[STATTIMER], IPL_STATCLOCK,
	    sxitimer_statintr, NULL, "stattick");

	/* clear timer interrupt pending bits */
	isr = bus_space_read_4(sxitimer_iot, sxitimer_ioh, TIMER_ISR);
	isr |= TIMER_IRQ(STATTIMER) | TIMER_IRQ(TICKTIMER);
	bus_space_write_4(sxitimer_iot, sxitimer_ioh, TIMER_ISR, isr);

	/* enable timer IRQs */
	ier = bus_space_read_4(sxitimer_iot, sxitimer_ioh, TIMER_IER);
	ier |= TIMER_IRQ(STATTIMER) | TIMER_IRQ(TICKTIMER);
	bus_space_write_4(sxitimer_iot, sxitimer_ioh, TIMER_IER, ier);

	/* enable timers */
	ctrl = bus_space_read_4(sxitimer_iot, sxitimer_ioh,
	    TIMER_CTRL(CNTRTIMER));
	bus_space_write_4(sxitimer_iot, sxitimer_ioh,
	    TIMER_CTRL(CNTRTIMER),
	    ctrl | TIMER_ENABLE | TIMER_RELOAD | TIMER_CONTINOUS);

	ctrl = bus_space_read_4(sxitimer_iot, sxitimer_ioh,
	    TIMER_CTRL(STATTIMER));
	bus_space_write_4(sxitimer_iot, sxitimer_ioh,
	    TIMER_CTRL(STATTIMER),
	    ctrl | TIMER_ENABLE | TIMER_RELOAD | TIMER_SINGLESHOT);

	ctrl = bus_space_read_4(sxitimer_iot, sxitimer_ioh,
	    TIMER_CTRL(TICKTIMER));
	bus_space_write_4(sxitimer_iot, sxitimer_ioh,
	    TIMER_CTRL(TICKTIMER),
	    ctrl | TIMER_ENABLE | TIMER_RELOAD | TIMER_SINGLESHOT);
}

/* 
 * See comment in arm/xscale/i80321_clock.c
 *
 * Counter is count up, but with autoreload timers it is not possible
 * to detect how many interrupts passed while interrupts were blocked.
 * Also it is not possible to atomically add to the register.
 *
 * To work around this two timers are used, one is used as a reference
 * clock without reload, however we just disable the interrupt it
 * could generate.
 *
 * Internally this keeps track of when the next timer should fire
 * and based on that time and the current value of the reference
 * clock a number is written into the timer count register to schedule
 * the next event.
 */
/* XXX update above comment */
int
sxitimer_tickintr(void *frame)
{
	uint32_t now, nextevent;
	uint32_t val;
	int rc = 0;

	splassert(IPL_CLOCK);	

	/* clear timer pending interrupt bit */
	bus_space_write_4(sxitimer_iot, sxitimer_ioh,
	    TIMER_ISR, TIMER_IRQ(TICKTIMER));

	now = sxitimer_readcnt32();

	while ((int32_t)(now - sxitimer_tick_nextevt) < 0) {
		sxitimer_tick_nextevt -= sxitimer_tick_tpi;
		sxitimer_ticks_err_sum += sxitimer_ticks_err_cnt;

		while (sxitimer_ticks_err_sum  > hz) {
			sxitimer_tick_nextevt += 1;
			sxitimer_ticks_err_sum -= hz;
		}

		rc = 1;
		hardclock(frame);
	}
	nextevent = now - sxitimer_tick_nextevt;
	if (nextevent < 10 /* XXX */)
		nextevent = 10;

	if (nextevent > sxitimer_tick_tpi) {
		/*
		 * If interrupts are blocked too long, like during
		 * the root prompt or ddb, the timer can roll over,
		 * this will allow the system to continue to run
		 * even if time is lost.
		 */
		nextevent = sxitimer_tick_tpi;
		sxitimer_tick_nextevt = now;
	}

	val = bus_space_read_4(sxitimer_iot, sxitimer_ioh,
	    TIMER_CTRL(TICKTIMER));
	bus_space_write_4(sxitimer_iot, sxitimer_ioh,
	    TIMER_CTRL(TICKTIMER), val & ~TIMER_ENABLE);

	sxitimer_sync();

	bus_space_write_4(sxitimer_iot, sxitimer_ioh,
	    TIMER_INTV(TICKTIMER), nextevent);

	val = bus_space_read_4(sxitimer_iot, sxitimer_ioh,
	    TIMER_CTRL(TICKTIMER));
	bus_space_write_4(sxitimer_iot, sxitimer_ioh,
	    TIMER_CTRL(TICKTIMER),
	    val | TIMER_ENABLE | TIMER_RELOAD | TIMER_SINGLESHOT);

	return rc;
}

int
sxitimer_statintr(void *frame)
{
	uint32_t now, nextevent, r;
	uint32_t val;
	int rc = 0;

	splassert(IPL_STATCLOCK);	

	/* clear timer pending interrupt bit */
	bus_space_write_4(sxitimer_iot, sxitimer_ioh,
	    TIMER_ISR, TIMER_IRQ(STATTIMER));

	now = sxitimer_readcnt32();
	while ((int32_t)(now - sxitimer_stat_nextevt) < 0) {
		do {
			r = random() & (sxitimer_statvar -1);
		} while (r == 0); /* random == 0 not allowed */
		sxitimer_stat_nextevt -= sxitimer_statmin + r;
		rc = 1;
		statclock(frame);
	}

	nextevent = now - sxitimer_stat_nextevt;

	if (nextevent < 10 /* XXX */)
		nextevent = 10;

	if (nextevent > sxitimer_stat_tpi) {
		/*
		 * If interrupts are blocked too long, like during
		 * the root prompt or ddb, the timer can roll over,
		 * this will allow the system to continue to run
		 * even if time is lost.
		 */
		nextevent = sxitimer_stat_tpi;
		sxitimer_stat_nextevt = now;
	}

	val = bus_space_read_4(sxitimer_iot, sxitimer_ioh,
	    TIMER_CTRL(STATTIMER));
	bus_space_write_4(sxitimer_iot, sxitimer_ioh,
	    TIMER_CTRL(STATTIMER), val & ~TIMER_ENABLE);

	sxitimer_sync();

	bus_space_write_4(sxitimer_iot, sxitimer_ioh,
	    TIMER_INTV(STATTIMER), nextevent);

	val = bus_space_read_4(sxitimer_iot, sxitimer_ioh,
	    TIMER_CTRL(STATTIMER));
	bus_space_write_4(sxitimer_iot, sxitimer_ioh,
	    TIMER_CTRL(STATTIMER),
	    val | TIMER_ENABLE | TIMER_RELOAD | TIMER_SINGLESHOT);

	return rc;
}

uint64_t
sxitimer_readcnt64(void)
{
	uint32_t low, high;

	/* latch counter, loop until ready */
	bus_space_write_4(sxitimer_iot, sxitimer_ioh, CNT64_CTRL, CNT64_RL_EN);
	while (bus_space_read_4(sxitimer_iot, sxitimer_ioh, CNT64_CTRL)
	    & CNT64_RL_EN)
		continue;

	/*
	 * A10 usermanual doesn't mention anything about order, but fwiw
	 * iirc. A20 manual mentions that low should be read first.
	 */
	/* XXX check above */
	low = bus_space_read_4(sxitimer_iot, sxitimer_ioh, CNT64_LOW);
	high = bus_space_read_4(sxitimer_iot, sxitimer_ioh, CNT64_HIGH);
	return (uint64_t)high << 32 | low;
}

uint32_t
sxitimer_readcnt32(void)
{
	return bus_space_read_4(sxitimer_iot, sxitimer_ioh,
	    TIMER_CURR(CNTRTIMER));
}

void
sxitimer_sync(void)
{
	uint32_t now = sxitimer_readcnt32();

	while ((now - sxitimer_readcnt32()) < TIMER_SYNC)
		CPU_BUSY_CYCLE();
}

void
sxitimer_delay(u_int usecs)
{
	uint64_t oclock, timeout;

	oclock = sxitimer_readcnt64();
	timeout = oclock + (COUNTER_FREQUENCY / 1000000) * usecs;

	while (oclock < timeout)
		oclock = sxitimer_readcnt64();
}

void
sxitimer_setstatclockrate(int newhz)
{
	int minint, statint, s;
	
	s = splstatclock();

	statint = sxitimer_freq[STATTIMER] / newhz;
	/* calculate largest 2^n which is smaller than just over half statint */
	sxitimer_statvar = 0x40000000; /* really big power of two */
	minint = statint / 2 + 100;
	while (sxitimer_statvar > minint)
		sxitimer_statvar >>= 1;

	sxitimer_statmin = statint - (sxitimer_statvar >> 1);

	splx(s);

	/*
	 * XXX this allows the next stat timer to occur then it switches
	 * to the new frequency. Rather than switching instantly.
	 */
}

u_int
sxitimer_get_timecount(struct timecounter *tc)
{
	return (u_int)UINT_MAX - sxitimer_readcnt32();
}
