/* $OpenBSD: agtimer.c,v 1.4 2015/06/06 16:49:04 jsg Exp $ */
/*
 * Copyright (c) 2011 Dale Rahn <drahn@openbsd.org>
 * Copyright (c) 2013 Patrick Wildt <patrick@blueri.se>
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

/* registers */
#define GTIMER_CNTP_CTL_ENABLE		(1 << 0)
#define GTIMER_CNTP_CTL_IMASK		(1 << 1)
#define GTIMER_CNTP_CTL_ISTATUS		(1 << 2)

#define TIMER_FREQUENCY		24 * 1000 * 1000 /* ARM core clock */
int32_t agtimer_frequency = TIMER_FREQUENCY;

u_int agtimer_get_timecount(struct timecounter *);

static struct timecounter agtimer_timecounter = {
	agtimer_get_timecount, NULL, 0x7fffffff, 0, "agtimer", 0, NULL
};

#define MAX_ARM_CPUS	8

struct agtimer_pcpu_softc {
	uint64_t 		pc_nexttickevent;
	uint64_t 		pc_nextstatevent;
	u_int32_t		pc_ticks_err_sum;
};

struct agtimer_softc {
	struct device		sc_dev;

	struct agtimer_pcpu_softc sc_pstat[MAX_ARM_CPUS];

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

int		agtimer_match(struct device *, void *, void *);
void		agtimer_attach(struct device *, struct device *, void *);
uint64_t	agtimer_readcnt64(struct agtimer_softc *sc);
int		agtimer_intr(void *);
void		agtimer_cpu_initclocks(void);
void		agtimer_delay(u_int);
void		agtimer_setstatclockrate(int stathz);
void		agtimer_set_clockrate(int32_t new_frequency);
void		agtimer_startclock(void);

/* hack - XXXX
 * agtimer connects directly to ampintc, not thru the generic
 * inteface because it uses an 'internal' interupt
 * not a peripheral interrupt.
 */
void	*ampintc_intr_establish(int, int, int (*)(void *), void *, char *);



struct cfattach agtimer_ca = {
	sizeof (struct agtimer_softc), agtimer_match, agtimer_attach
};

struct cfdriver agtimer_cd = {
	NULL, "agtimer", DV_DULL
};

uint64_t
agtimer_readcnt64(struct agtimer_softc *sc)
{
	uint64_t val;

	__asm volatile("mrrc p15, 0, %Q0, %R0, c14" : "=r" (val));

	return (val);
}

static inline int
agtimer_get_ctrl(void)
{
	uint32_t val;

	__asm volatile("mrc p15, 0, %0, c14, c2, 1" : "=r" (val));

	return (val);
}

static inline int
agtimer_set_ctrl(uint32_t val)
{
	__asm volatile("mcr p15, 0, %[val], c14, c2, 1" : :
	    [val] "r" (val));

	cpu_drain_writebuf();
	//isb();

	return (0);
}

static inline int
agtimer_get_tval(void)
{
	uint32_t val;

	__asm volatile("mrc p15, 0, %0, c14, c2, 0" : "=r" (val));

	return (val);
}

static inline int
agtimer_set_tval(uint32_t val)
{
	__asm volatile("mcr p15, 0, %[val], c14, c2, 0" : :
	    [val] "r" (val));
	cpu_drain_writebuf();
	//isb();

	return (0);
}

int
agtimer_match(struct device *parent, void *cfdata, void *aux)
{
	if ((cpufunc_id() & CPU_ID_CORTEX_A7_MASK) == CPU_ID_CORTEX_A7 ||
	    (cpufunc_id() & CPU_ID_CORTEX_A15_MASK) == CPU_ID_CORTEX_A15 ||
	    (cpufunc_id() & CPU_ID_CORTEX_A17_MASK) == CPU_ID_CORTEX_A17)
		return (1);

	return 0;
}

void
agtimer_attach(struct device *parent, struct device *self, void *args)
{
	struct agtimer_softc *sc = (struct agtimer_softc *)self;

	sc->sc_ticks_per_second = agtimer_frequency;
	printf(": tick rate %d KHz\n", sc->sc_ticks_per_second /1000);

	/* XXX: disable user access */

#ifdef AMPTIMER_DEBUG
	evcount_attach(&sc->sc_clk_count, "clock", NULL);
	evcount_attach(&sc->sc_stat_count, "stat", NULL);
#endif

	/*
	 * private timer and interrupts not enabled until
	 * timer configures
	 */

	arm_clock_register(agtimer_cpu_initclocks, agtimer_delay,
	    agtimer_setstatclockrate, agtimer_startclock);

	agtimer_timecounter.tc_frequency = sc->sc_ticks_per_second;
	agtimer_timecounter.tc_priv = sc;

	tc_init(&agtimer_timecounter);
}

u_int
agtimer_get_timecount(struct timecounter *tc)
{
	struct agtimer_softc *sc = agtimer_timecounter.tc_priv;
	return agtimer_readcnt64(sc);
}

int
agtimer_intr(void *frame)
{
	struct agtimer_softc	*sc = agtimer_cd.cd_devs[0];
	struct agtimer_pcpu_softc *pc = &sc->sc_pstat[CPU_INFO_UNIT(curcpu())];
	uint64_t		 now;
	uint64_t		 nextevent;
	uint32_t		 r;
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

	now = agtimer_readcnt64(sc);

	while (pc->pc_nexttickevent <= now) {
		pc->pc_nexttickevent += sc->sc_ticks_per_intr;
		pc->pc_ticks_err_sum += sc->sc_ticks_err_cnt;

		/* looping a few times is faster than divide */
		while (pc->pc_ticks_err_sum > hz) {
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

	delay = nextevent - now;
	if (delay < 0)
		delay = 1;

	agtimer_set_tval(delay);

	return (rc);
}

void
agtimer_set_clockrate(int32_t new_frequency)
{
	struct agtimer_softc	*sc = agtimer_cd.cd_devs[0];

	agtimer_frequency = new_frequency;

	if (sc == NULL)
		return;

	sc->sc_ticks_per_second = agtimer_frequency;
	agtimer_timecounter.tc_frequency = sc->sc_ticks_per_second;
	printf("agtimer0: adjusting clock: new tick rate %d KHz\n",
	    sc->sc_ticks_per_second /1000);
}

void
agtimer_cpu_initclocks()
{
	struct agtimer_softc	*sc = agtimer_cd.cd_devs[0];
	struct agtimer_pcpu_softc *pc = &sc->sc_pstat[CPU_INFO_UNIT(curcpu())];
	uint32_t		 reg;
	uint64_t		 next;

	stathz = hz;
	profhz = hz * 10;

	if (sc->sc_ticks_per_second != agtimer_frequency) {
		agtimer_set_clockrate(agtimer_frequency);
	}

	agtimer_setstatclockrate(stathz);

	sc->sc_ticks_per_intr = sc->sc_ticks_per_second / hz;
	sc->sc_ticks_err_cnt = sc->sc_ticks_per_second % hz;
	pc->pc_ticks_err_sum = 0;

	/* establish interrupts */
	/* XXX - irq */

	/* secure physical timer */
	ampintc_intr_establish(29, IPL_CLOCK, agtimer_intr,
	    NULL, "tick");
	/* non-secure physical timer */
	ampintc_intr_establish(30, IPL_CLOCK, agtimer_intr,
	    NULL, "tick");

	next = agtimer_readcnt64(sc) + sc->sc_ticks_per_intr;
	pc->pc_nexttickevent = pc->pc_nextstatevent = next;

	reg = agtimer_get_ctrl();
	reg &= GTIMER_CNTP_CTL_IMASK;
	reg |= GTIMER_CNTP_CTL_ENABLE;
	agtimer_set_tval(sc->sc_ticks_per_second);
	agtimer_set_ctrl(reg);
}

void
agtimer_delay(u_int usecs)
{
	struct agtimer_softc	*sc = agtimer_cd.cd_devs[0];
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

	oclock = agtimer_readcnt64(sc);
	while (1) {
		for (j = 100; j > 0; j--)
			;
		clock = agtimer_readcnt64(sc);
		delta = clock - oclock;
		if (delta > delaycnt)
			break;
	}
}

void
agtimer_setstatclockrate(int newhz)
{
	struct agtimer_softc	*sc = agtimer_cd.cd_devs[0];
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
agtimer_startclock(void)
{
	struct agtimer_softc	*sc = agtimer_cd.cd_devs[0];
	struct agtimer_pcpu_softc *pc = &sc->sc_pstat[CPU_INFO_UNIT(curcpu())];
	uint64_t nextevent;
	uint32_t reg;

	nextevent = agtimer_readcnt64(sc) + sc->sc_ticks_per_intr;
	pc->pc_nexttickevent = pc->pc_nextstatevent = nextevent;

	reg = agtimer_get_ctrl();
	reg &= GTIMER_CNTP_CTL_IMASK;
	reg |= GTIMER_CNTP_CTL_ENABLE;
	agtimer_set_tval(sc->sc_ticks_per_second);
	agtimer_set_ctrl(reg);
}
