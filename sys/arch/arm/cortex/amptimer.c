/* $OpenBSD: amptimer.c,v 1.6 2018/07/09 09:51:43 patrick Exp $ */
/*
 * Copyright (c) 2011 Dale Rahn <drahn@openbsd.org>
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
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/timetc.h>
#include <sys/evcount.h>

#include <arm/cpufunc.h>
#include <machine/bus.h>
#include <machine/intr.h>
#include <arm/cortex/cortex.h>

/* offset from periphbase */
#define GTIMER_ADDR	0x200
#define GTIMER_SIZE	0x100

/* registers */
#define GTIMER_CNT_LOW		0x00
#define GTIMER_CNT_HIGH		0x04
#define GTIMER_CTRL		0x08
#define GTIMER_CTRL_AA		(1 << 3)
#define GTIMER_CTRL_IRQ		(1 << 2)
#define GTIMER_CTRL_COMP	(1 << 1)
#define GTIMER_CTRL_TIMER	(1 << 0)
#define GTIMER_STATUS		0x0c
#define GTIMER_STATUS_EVENT	(1 << 0)
#define GTIMER_CMP_LOW		0x10
#define GTIMER_CMP_HIGH		0x14
#define GTIMER_AUTOINC		0x18

/* offset from periphbase */
#define PTIMER_ADDR		0x600
#define PTIMER_SIZE		0x100

/* registers */
#define PTIMER_LOAD		0x0
#define PTIMER_CNT		0x4
#define PTIMER_CTRL		0x8
#define PTIMER_CTRL_ENABLE	(1<<0)
#define PTIMER_CTRL_AUTORELOAD	(1<<1)
#define PTIMER_CTRL_IRQEN	(1<<2)
#define PTIMER_STATUS		0xC
#define PTIMER_STATUS_EVENT	(1<<0)

#define TIMER_FREQUENCY		396 * 1000 * 1000 /* ARM core clock */
int32_t amptimer_frequency = TIMER_FREQUENCY;

u_int amptimer_get_timecount(struct timecounter *);

static struct timecounter amptimer_timecounter = {
	amptimer_get_timecount, NULL, 0x7fffffff, 0, "amptimer", 0, NULL
};

#define MAX_ARM_CPUS	8

struct amptimer_pcpu_softc {
	uint64_t 		pc_nexttickevent;
	uint64_t 		pc_nextstatevent;
	u_int32_t		pc_ticks_err_sum;
};

struct amptimer_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	bus_space_handle_t	sc_pioh;

	struct amptimer_pcpu_softc sc_pstat[MAX_ARM_CPUS];

	u_int32_t		sc_ticks_err_cnt;
	u_int32_t		sc_ticks_per_second;
	u_int32_t		sc_ticks_per_intr;
	u_int32_t		sc_statvar;
	u_int32_t		sc_statmin;

#ifdef AMPTIMER_DEBUG
	struct evcount		sc_clk_count;
	struct evcount		sc_stat_count;
#endif
};

int		amptimer_match(struct device *, void *, void *);
void		amptimer_attach(struct device *, struct device *, void *);
uint64_t	amptimer_readcnt64(struct amptimer_softc *sc);
int		amptimer_intr(void *);
void		amptimer_cpu_initclocks(void);
void		amptimer_delay(u_int);
void		amptimer_setstatclockrate(int stathz);
void		amptimer_set_clockrate(int32_t new_frequency);
void		amptimer_startclock(void);

/* hack - XXXX
 * gptimer connects directly to ampintc, not thru the generic
 * interface because it uses an 'internal' interrupt
 * not a peripheral interrupt.
 */
void	*ampintc_intr_establish(int, int, int, int (*)(void *), void *, char *);



struct cfattach amptimer_ca = {
	sizeof (struct amptimer_softc), amptimer_match, amptimer_attach
};

struct cfdriver amptimer_cd = {
	NULL, "amptimer", DV_DULL
};

uint64_t
amptimer_readcnt64(struct amptimer_softc *sc)
{
	uint32_t high0, high1, low;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	do {
		high0 = bus_space_read_4(iot, ioh, GTIMER_CNT_HIGH);
		low = bus_space_read_4(iot, ioh, GTIMER_CNT_LOW);
		high1 = bus_space_read_4(iot, ioh, GTIMER_CNT_HIGH);
	} while (high0 != high1);

	return ((((uint64_t)high1) << 32) | low);
}

int
amptimer_match(struct device *parent, void *cfdata, void *aux)
{
	if ((cpufunc_id() & CPU_ID_CORTEX_A9_MASK) == CPU_ID_CORTEX_A9)
		return (1);

	return 0;
}

void
amptimer_attach(struct device *parent, struct device *self, void *args)
{
	struct amptimer_softc *sc = (struct amptimer_softc *)self;
	struct cortex_attach_args *ia = args;
	bus_space_handle_t ioh, pioh;

	sc->sc_iot = ia->ca_iot;

	if (bus_space_map(sc->sc_iot, ia->ca_periphbase + GTIMER_ADDR,
	    GTIMER_SIZE, 0, &ioh))
		panic("amptimer_attach: bus_space_map global timer failed!");

	if (bus_space_map(sc->sc_iot, ia->ca_periphbase + PTIMER_ADDR,
	    PTIMER_SIZE, 0, &pioh))
		panic("amptimer_attach: bus_space_map priv timer failed!");

	sc->sc_ticks_per_second = amptimer_frequency;
	printf(": tick rate %d KHz\n", sc->sc_ticks_per_second /1000);

	sc->sc_ioh = ioh;
	sc->sc_pioh = pioh;

	/* disable global timer */
	bus_space_write_4(sc->sc_iot, ioh, GTIMER_CTRL, 0);

	/* XXX ??? reset counters to 0 - gives us uptime in the counter */
	bus_space_write_4(sc->sc_iot, ioh, GTIMER_CNT_LOW, 0);
	bus_space_write_4(sc->sc_iot, ioh, GTIMER_CNT_HIGH, 0);

	/* enable global timer */
	bus_space_write_4(sc->sc_iot, ioh, GTIMER_CTRL, GTIMER_CTRL_TIMER);

#if defined(USE_GTIMER_CMP)

	/* clear event */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GTIMER_STATUS, 1);
#else
	bus_space_write_4(sc->sc_iot, sc->sc_pioh, PTIMER_CTRL, 0);
	bus_space_write_4(sc->sc_iot, sc->sc_pioh, PTIMER_STATUS,
	    PTIMER_STATUS_EVENT);
#endif


#ifdef AMPTIMER_DEBUG
	evcount_attach(&sc->sc_clk_count, "clock", NULL);
	evcount_attach(&sc->sc_stat_count, "stat", NULL);
#endif

	/*
	 * private timer and interrupts not enabled until
	 * timer configures
	 */

	arm_clock_register(amptimer_cpu_initclocks, amptimer_delay,
	    amptimer_setstatclockrate, amptimer_startclock);

	amptimer_timecounter.tc_frequency = sc->sc_ticks_per_second;
	amptimer_timecounter.tc_priv = sc;

	tc_init(&amptimer_timecounter);
}

u_int
amptimer_get_timecount(struct timecounter *tc)
{
	struct amptimer_softc *sc = amptimer_timecounter.tc_priv;
	return bus_space_read_4(sc->sc_iot, sc->sc_ioh, GTIMER_CNT_LOW);
}

int
amptimer_intr(void *frame)
{
	struct amptimer_softc	*sc = amptimer_cd.cd_devs[0];
	struct amptimer_pcpu_softc *pc = &sc->sc_pstat[CPU_INFO_UNIT(curcpu())];
	uint64_t		 now;
	uint64_t		 nextevent;
	uint32_t		 r, reg;
#if defined(USE_GTIMER_CMP)
	int			 skip = 1;
#else
	int64_t			 delay;
#endif
	int			 rc = 0;

	/*
	 * DSR - I know that the tick timer is 64 bits, but the following
	 * code deals with rollover, so there is no point in dealing
	 * with the 64 bit math, just let the 32 bit rollover
	 * do the right thing
	 */

	now = amptimer_readcnt64(sc);

	while (pc->pc_nexttickevent <= now) {
		pc->pc_nexttickevent += sc->sc_ticks_per_intr;
		pc->pc_ticks_err_sum += sc->sc_ticks_err_cnt;

		/* looping a few times is faster than divide */
		while (pc->pc_ticks_err_sum  > hz) {
			pc->pc_nexttickevent += 1;
			pc->pc_ticks_err_sum -= hz;
		}

#ifdef AMPTIMER_DEBUG
		sc->sc_clk_count.ec_count++;
#endif
		rc = 1;
		hardclock(frame);
	}
	while (pc->pc_nextstatevent <= now) {
		do {
			r = random() & (sc->sc_statvar -1);
		} while (r == 0); /* random == 0 not allowed */
		pc->pc_nextstatevent += sc->sc_statmin + r;

		/* XXX - correct nextstatevent? */
#ifdef AMPTIMER_DEBUG
		sc->sc_stat_count.ec_count++;
#endif
		rc = 1;
		statclock(frame);
	}

	if (pc->pc_nexttickevent < pc->pc_nextstatevent)
		nextevent = pc->pc_nexttickevent;
	else
		nextevent = pc->pc_nextstatevent;

#if defined(USE_GTIMER_CMP)
again:
	reg = bus_space_read_4(sc->sc_iot, sc->sc_ioh, GTIMER_CTRL);
	reg &= ~GTIMER_CTRL_COMP;
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GTIMER_CTRL, reg);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GTIMER_CMP_LOW,
	    nextevent & 0xffffffff);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GTIMER_CMP_HIGH,
	    nextevent >> 32);
	reg |= GTIMER_CTRL_COMP;
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GTIMER_CTRL, reg);

	now = amptimer_readcnt64(sc);
	if (now >= nextevent) {
		nextevent = now + skip;
		skip += 1;
		goto again;
	}
#else
	/* clear old status */
	bus_space_write_4(sc->sc_iot, sc->sc_pioh, PTIMER_STATUS,
	    PTIMER_STATUS_EVENT);

	delay = nextevent - now;
	if (delay < 0)
		delay = 1;

	reg = bus_space_read_4(sc->sc_iot, sc->sc_pioh, PTIMER_CTRL);
	if ((reg & (PTIMER_CTRL_ENABLE | PTIMER_CTRL_IRQEN)) !=
	    (PTIMER_CTRL_ENABLE | PTIMER_CTRL_IRQEN))
		bus_space_write_4(sc->sc_iot, sc->sc_pioh, PTIMER_CTRL,
		    (PTIMER_CTRL_ENABLE | PTIMER_CTRL_IRQEN));

	bus_space_write_4(sc->sc_iot, sc->sc_pioh, PTIMER_LOAD, delay);
#endif

	return (rc);
}

void
amptimer_set_clockrate(int32_t new_frequency)
{
	struct amptimer_softc	*sc = amptimer_cd.cd_devs[0];

	amptimer_frequency = new_frequency;

	if (sc == NULL)
		return;

	sc->sc_ticks_per_second = amptimer_frequency;
	amptimer_timecounter.tc_frequency = sc->sc_ticks_per_second;
	printf("amptimer0: adjusting clock: new tick rate %d KHz\n",
	    sc->sc_ticks_per_second /1000);
}

void
amptimer_cpu_initclocks()
{
	struct amptimer_softc	*sc = amptimer_cd.cd_devs[0];
	struct amptimer_pcpu_softc *pc = &sc->sc_pstat[CPU_INFO_UNIT(curcpu())];
	uint64_t		 next;
#if defined(USE_GTIMER_CMP)
	uint32_t		 reg;
#endif

	stathz = hz;
	profhz = hz * 10;

	if (sc->sc_ticks_per_second != amptimer_frequency) {
		amptimer_set_clockrate(amptimer_frequency);
	}

	amptimer_setstatclockrate(stathz);

	sc->sc_ticks_per_intr = sc->sc_ticks_per_second / hz;
	sc->sc_ticks_err_cnt = sc->sc_ticks_per_second % hz;
	pc->pc_ticks_err_sum = 0;

	/* establish interrupts */
	/* XXX - irq */
#if defined(USE_GTIMER_CMP)
	ampintc_intr_establish(27, IST_EDGE_RISING, IPL_CLOCK,
	    amptimer_intr, NULL, "tick");
#else
	ampintc_intr_establish(29, IST_EDGE_RISING, IPL_CLOCK,
	    amptimer_intr, NULL, "tick");
#endif

	next = amptimer_readcnt64(sc) + sc->sc_ticks_per_intr;
	pc->pc_nexttickevent = pc->pc_nextstatevent = next;

#if defined(USE_GTIMER_CMP)
	reg = bus_space_read_4(sc->sc_iot, sc->sc_ioh, GTIMER_CTRL);
	reg &= ~GTIMER_CTRL_COMP;
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GTIMER_CTRL, reg);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GTIMER_CMP_LOW,
	    next & 0xffffffff);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GTIMER_CMP_HIGH,
	    next >> 32);
	reg |= GTIMER_CTRL_COMP | GTIMER_CTRL_IRQ;
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GTIMER_CTRL, reg);
#else
	bus_space_write_4(sc->sc_iot, sc->sc_pioh, PTIMER_CTRL,
	    (PTIMER_CTRL_ENABLE | PTIMER_CTRL_IRQEN));
	bus_space_write_4(sc->sc_iot, sc->sc_pioh, PTIMER_LOAD,
	    sc->sc_ticks_per_intr);
#endif
}

void
amptimer_delay(u_int usecs)
{
	struct amptimer_softc	*sc = amptimer_cd.cd_devs[0];
	u_int32_t		clock, oclock, delta, delaycnt;
	volatile int		j;
	int			csec, usec;

	if (usecs > (0x80000000 / (sc->sc_ticks_per_second))) {
		csec = usecs / 10000;
		usec = usecs % 10000;

		delaycnt = (sc->sc_ticks_per_second / 100) * csec +
		    (sc->sc_ticks_per_second / 100) * usec / 10000;
	} else {
		delaycnt = sc->sc_ticks_per_second * usecs / 1000000;
	}
	if (delaycnt <= 1)
		for (j = 100; j > 0; j--)
			;

	oclock = bus_space_read_4(sc->sc_iot, sc->sc_ioh, GTIMER_CNT_LOW);
	while (1) {
		for (j = 100; j > 0; j--)
			;
		clock = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
		    GTIMER_CNT_LOW);
		delta = clock - oclock;
		if (delta > delaycnt)
			break;
	}
}

void
amptimer_setstatclockrate(int newhz)
{
	struct amptimer_softc	*sc = amptimer_cd.cd_devs[0];
	int			 minint, statint;
	int			 s;

	s = splclock();

	statint = sc->sc_ticks_per_second / newhz;
	/* calculate largest 2^n which is smaller that just over half statint */
	sc->sc_statvar = 0x40000000; /* really big power of two */
	minint = statint / 2 + 100;
	while (sc->sc_statvar > minint)
		sc->sc_statvar >>= 1;

	sc->sc_statmin = statint - (sc->sc_statvar >> 1);

	splx(s);

	/*
	 * XXX this allows the next stat timer to occur then it switches
	 * to the new frequency. Rather than switching instantly.
	 */
}

void
amptimer_startclock(void)
{
	struct amptimer_softc	*sc = amptimer_cd.cd_devs[0];
	struct amptimer_pcpu_softc *pc = &sc->sc_pstat[CPU_INFO_UNIT(curcpu())];
	uint64_t nextevent;

	nextevent = amptimer_readcnt64(sc) + sc->sc_ticks_per_intr;
	pc->pc_nexttickevent = pc->pc_nextstatevent = nextevent;
	
	bus_space_write_4(sc->sc_iot, sc->sc_pioh, PTIMER_LOAD,
		sc->sc_ticks_per_intr);
}
