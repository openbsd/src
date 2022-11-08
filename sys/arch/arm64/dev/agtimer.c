/* $OpenBSD: agtimer.c,v 1.20 2022/11/08 17:56:38 cheloha Exp $ */
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
#include <sys/clockintr.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/stdint.h>
#include <sys/timetc.h>
#include <sys/evcount.h>

#include <machine/intr.h>
#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/ofw/fdt.h>
#include <dev/ofw/openfirm.h>

/* registers */
#define GTIMER_CNTV_CTL_ENABLE		(1 << 0)
#define GTIMER_CNTV_CTL_IMASK		(1 << 1)
#define GTIMER_CNTV_CTL_ISTATUS		(1 << 2)

#define TIMER_FREQUENCY		24 * 1000 * 1000 /* ARM core clock */
int32_t agtimer_frequency = TIMER_FREQUENCY;

u_int agtimer_get_timecount(struct timecounter *);

static struct timecounter agtimer_timecounter = {
	.tc_get_timecount = agtimer_get_timecount,
	.tc_poll_pps = NULL,
	.tc_counter_mask = 0xffffffff,
	.tc_frequency = 0,
	.tc_name = "agtimer",
	.tc_quality = 0,
	.tc_priv = NULL,
	.tc_user = TC_AGTIMER,
};

struct agtimer_softc {
	struct device		sc_dev;
	int			sc_node;

	u_int32_t		sc_ticks_per_second;
	uint64_t		sc_nsec_cycle_ratio;
	uint64_t		sc_nsec_max;
	void			*sc_ih;
};

int		agtimer_match(struct device *, void *, void *);
void		agtimer_attach(struct device *, struct device *, void *);
uint64_t	agtimer_readcnt64(void);
int		agtimer_intr(void *);
void		agtimer_cpu_initclocks(void);
void		agtimer_delay(u_int);
void		agtimer_rearm(void *, uint64_t);
void		agtimer_setstatclockrate(int stathz);
void		agtimer_set_clockrate(int32_t new_frequency);
void		agtimer_startclock(void);
void		agtimer_trigger(void *);

const struct cfattach agtimer_ca = {
	sizeof (struct agtimer_softc), agtimer_match, agtimer_attach
};

struct cfdriver agtimer_cd = {
	NULL, "agtimer", DV_DULL
};

struct intrclock agtimer_intrclock = {
	.ic_rearm = agtimer_rearm,
	.ic_trigger = agtimer_trigger
};

uint64_t
agtimer_readcnt64(void)
{
	uint64_t val0, val1;

	/*
	 * Work around Cortex-A73 errata 858921, where there is a
	 * one-cycle window where the read might return the old value
	 * for the low 32 bits and the new value for the high 32 bits
	 * upon roll-over of the low 32 bits.
	 */
	__asm volatile("isb" ::: "memory");
	__asm volatile("mrs %x0, CNTVCT_EL0" : "=r" (val0));
	__asm volatile("mrs %x0, CNTVCT_EL0" : "=r" (val1));
	return ((val0 ^ val1) & 0x100000000ULL) ? val0 : val1;
}

static inline uint64_t
agtimer_get_freq(void)
{
	uint64_t val;

	__asm volatile("mrs %x0, CNTFRQ_EL0" : "=r" (val));

	return (val);
}

static inline int
agtimer_get_ctrl(void)
{
	uint32_t val;

	__asm volatile("mrs %x0, CNTV_CTL_EL0" : "=r" (val));

	return (val);
}

static inline int
agtimer_set_ctrl(uint32_t val)
{
	__asm volatile("msr CNTV_CTL_EL0, %x0" :: "r" (val));
	__asm volatile("isb" ::: "memory");

	return (0);
}

static inline int
agtimer_set_tval(uint32_t val)
{
	__asm volatile("msr CNTV_TVAL_EL0, %x0" :: "r" (val));
	__asm volatile("isb" ::: "memory");

	return (0);
}

int
agtimer_match(struct device *parent, void *cfdata, void *aux)
{
	struct fdt_attach_args *faa = (struct fdt_attach_args *)aux;

	return (OF_is_compatible(faa->fa_node, "arm,armv7-timer") ||
	    OF_is_compatible(faa->fa_node, "arm,armv8-timer"));
}

void
agtimer_attach(struct device *parent, struct device *self, void *aux)
{
	struct agtimer_softc *sc = (struct agtimer_softc *)self;
	struct fdt_attach_args *faa = aux;

	sc->sc_node = faa->fa_node;

	if (agtimer_get_freq() != 0)
		agtimer_frequency = agtimer_get_freq();
	agtimer_frequency =
	    OF_getpropint(sc->sc_node, "clock-frequency", agtimer_frequency);
	sc->sc_ticks_per_second = agtimer_frequency;
	sc->sc_nsec_cycle_ratio =
	    sc->sc_ticks_per_second * (1ULL << 32) / 1000000000;
	sc->sc_nsec_max = UINT64_MAX / sc->sc_nsec_cycle_ratio;

	printf(": %u kHz\n", sc->sc_ticks_per_second / 1000);

	/*
	 * private timer and interrupts not enabled until
	 * timer configures
	 */

	arm_clock_register(agtimer_cpu_initclocks, agtimer_delay,
	    agtimer_setstatclockrate, agtimer_startclock);

	agtimer_timecounter.tc_frequency = sc->sc_ticks_per_second;
	agtimer_timecounter.tc_priv = sc;
	tc_init(&agtimer_timecounter);

	agtimer_intrclock.ic_cookie = sc;
}

u_int
agtimer_get_timecount(struct timecounter *tc)
{
	uint64_t val;

	/*
	 * No need to work around Cortex-A73 errata 858921 since we
	 * only look at the low 32 bits here.
	 */
	__asm volatile("isb" ::: "memory");
	__asm volatile("mrs %x0, CNTVCT_EL0" : "=r" (val));
	return (val & 0xffffffff);
}

void
agtimer_rearm(void *cookie, uint64_t nsecs)
{
	struct agtimer_softc *sc = cookie;
	uint32_t cycles;

	if (nsecs > sc->sc_nsec_max)
		nsecs = sc->sc_nsec_max;
	cycles = (nsecs * sc->sc_nsec_cycle_ratio) >> 32;
	if (cycles > INT32_MAX)
		cycles = INT32_MAX;
	agtimer_set_tval(cycles);
}

void
agtimer_trigger(void *unused)
{
	agtimer_set_tval(0);
}

int
agtimer_intr(void *frame)
{
	return clockintr_dispatch(frame);
}

void
agtimer_set_clockrate(int32_t new_frequency)
{
	struct agtimer_softc	*sc = agtimer_cd.cd_devs[0];

	agtimer_frequency = new_frequency;

	if (sc == NULL)
		return;

	sc->sc_ticks_per_second = agtimer_frequency;
	sc->sc_nsec_cycle_ratio =
	    sc->sc_ticks_per_second * (1ULL << 32) / 1000000000;
	sc->sc_nsec_max = UINT64_MAX / sc->sc_nsec_cycle_ratio;

	agtimer_timecounter.tc_frequency = sc->sc_ticks_per_second;

	printf("agtimer0: adjusting clock: new tick rate %u kHz\n",
	    sc->sc_ticks_per_second / 1000);
}

void
agtimer_cpu_initclocks(void)
{
	struct agtimer_softc	*sc = agtimer_cd.cd_devs[0];
	uint32_t		 reg;
	uint64_t		 kctl;

	stathz = hz;
	profhz = stathz * 10;
	clockintr_init(CL_RNDSTAT);

	if (sc->sc_ticks_per_second != agtimer_frequency) {
		agtimer_set_clockrate(agtimer_frequency);
	}

	/* configure virtual timer interrupt */
	sc->sc_ih = arm_intr_establish_fdt_idx(sc->sc_node, 2,
	    IPL_CLOCK|IPL_MPSAFE, agtimer_intr, NULL, "tick");

	clockintr_cpu_init(&agtimer_intrclock);

	reg = agtimer_get_ctrl();
	reg &= ~GTIMER_CNTV_CTL_IMASK;
	reg |= GTIMER_CNTV_CTL_ENABLE;
	agtimer_set_tval(INT32_MAX);
	agtimer_set_ctrl(reg);

	clockintr_trigger();

	/* enable userland access to virtual counter */
	kctl = READ_SPECIALREG(CNTKCTL_EL1);
	WRITE_SPECIALREG(CNTKCTL_EL1, kctl | CNTKCTL_EL0VCTEN);
}

void
agtimer_delay(u_int usecs)
{
	uint64_t		clock, oclock, delta, delaycnt;
	uint64_t		csec, usec;
	volatile int		j;

	if (usecs > (0x80000000 / agtimer_frequency)) {
		csec = usecs / 10000;
		usec = usecs % 10000;

		delaycnt = (agtimer_frequency / 100) * csec +
		    (agtimer_frequency / 100) * usec / 10000;
	} else {
		delaycnt = agtimer_frequency * usecs / 1000000;
	}
	if (delaycnt <= 1)
		for (j = 100; j > 0; j--)
			;

	oclock = agtimer_readcnt64();
	while (1) {
		for (j = 100; j > 0; j--)
			;
		clock = agtimer_readcnt64();
		delta = clock - oclock;
		if (delta > delaycnt)
			break;
	}
}

void
agtimer_setstatclockrate(int newhz)
{
	clockintr_setstatclockrate(newhz);
}

void
agtimer_startclock(void)
{
	struct agtimer_softc	*sc = agtimer_cd.cd_devs[0];
	uint64_t kctl;
	uint32_t reg;

	arm_intr_route(sc->sc_ih, 1, curcpu());

	clockintr_cpu_init(&agtimer_intrclock);

	reg = agtimer_get_ctrl();
	reg &= ~GTIMER_CNTV_CTL_IMASK;
	reg |= GTIMER_CNTV_CTL_ENABLE;
	agtimer_set_tval(INT32_MAX);
	agtimer_set_ctrl(reg);

	clockintr_trigger();

	/* enable userland access to virtual counter */
	kctl = READ_SPECIALREG(CNTKCTL_EL1);
	WRITE_SPECIALREG(CNTKCTL_EL1, kctl | CNTKCTL_EL0VCTEN);
}

void
agtimer_init(void)
{
	uint64_t cntfrq = 0;

	/* XXX: Check for Generic Timer support. */
	cntfrq = agtimer_get_freq();

	if (cntfrq != 0) {
		agtimer_frequency = cntfrq;
		arm_clock_register(NULL, agtimer_delay, NULL, NULL);
	}
}
