/*	$OpenBSD: pxa2x0_clock.c,v 1.6 2008/01/03 17:59:32 kettenis Exp $ */

/*
 * Copyright (c) 2005 Dale Rahn <drahn@openbsd.org>
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
#include <sys/device.h>
#include <sys/timetc.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <arm/cpufunc.h>

#include <arm/sa11x0/sa11x0_reg.h>
#include <arm/sa11x0/sa11x0_var.h>
#include <arm/sa11x0/sa11x0_ostreg.h>
#include <arm/xscale/pxa2x0reg.h>

int	pxaost_match(struct device *, void *, void *);
void	pxaost_attach(struct device *, struct device *, void *);

int	doclockintr(void *);
int	clockintr(void *);
int	statintr(void *);
void	rtcinit(void);

struct pxaost_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;

	u_int32_t		sc_clock_count;
	u_int32_t		sc_statclock_count;
	u_int32_t		sc_statclock_step;
	u_int32_t		sc_clock_step;
	u_int32_t		sc_clock_step_err_cnt;
	u_int32_t		sc_clock_step_error;
};

static struct pxaost_softc *pxaost_sc = NULL;

#define CLK4_TIMER_FREQUENCY	32768		/* 32.768KHz */

#define CLK0_TIMER_FREQUENCY	3250000		/* 3.2500MHz */

#ifndef STATHZ
#define STATHZ	64
#endif

struct cfattach pxaost_ca = {
	sizeof (struct pxaost_softc), pxaost_match, pxaost_attach
};

struct cfdriver pxaost_cd = {
	NULL, "pxaost", DV_DULL
};

u_int	pxaost_get_timecount(struct timecounter *tc);

static struct timecounter pxaost_timecounter = {
	pxaost_get_timecount, NULL, 0xffffffff, CLK4_TIMER_FREQUENCY,
	"pxaost", 0, NULL
};

int
pxaost_match(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	return (1);
}

void
pxaost_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct pxaost_softc *sc = (struct pxaost_softc*)self;
	struct sa11x0_attach_args *sa = aux;

	printf("\n");

	sc->sc_iot = sa->sa_iot;

	pxaost_sc = sc;

	if (bus_space_map(sa->sa_iot, sa->sa_addr, sa->sa_size, 0,
	    &sc->sc_ioh))
		panic("%s: Cannot map registers", self->dv_xname);

	/* disable all channel and clear interrupt status */
	bus_space_write_4(pxaost_sc->sc_iot, pxaost_sc->sc_ioh, SAOST_IR, 0);
	bus_space_write_4(pxaost_sc->sc_iot, pxaost_sc->sc_ioh, SAOST_SR, 0x3f);

	bus_space_write_4(pxaost_sc->sc_iot, pxaost_sc->sc_ioh, OST_OMCR4, 0xc1);
	bus_space_write_4(pxaost_sc->sc_iot, pxaost_sc->sc_ioh, OST_OMCR5, 0x41);

	bus_space_write_4(pxaost_sc->sc_iot, pxaost_sc->sc_ioh, OST_OSMR4,
	    pxaost_sc->sc_clock_count);
	bus_space_write_4(pxaost_sc->sc_iot, pxaost_sc->sc_ioh, OST_OSMR5,
	    pxaost_sc->sc_statclock_count);

	/* Zero the counter value */
	bus_space_write_4(pxaost_sc->sc_iot, pxaost_sc->sc_ioh, OST_OSCR4, 0);

}

u_int
pxaost_get_timecount(struct timecounter *tc)
{
	return bus_space_read_4(pxaost_sc->sc_iot, pxaost_sc->sc_ioh,
	    OST_OSCR4);
}

int
clockintr(arg)
	void *arg;
{
	struct clockframe *frame = arg;
	u_int32_t oscr, match;
	u_int32_t match_error;

	bus_space_write_4(pxaost_sc->sc_iot, pxaost_sc->sc_ioh, SAOST_SR, 0x10);

	match = pxaost_sc->sc_clock_count;

	do {
		match += pxaost_sc->sc_clock_step;
		pxaost_sc->sc_clock_step_error +=
		    pxaost_sc->sc_clock_step_err_cnt;
		if (pxaost_sc->sc_clock_count > hz) {
			match_error = pxaost_sc->sc_clock_step_error / hz;
			pxaost_sc->sc_clock_step_error -= (match_error * hz);
			match += match_error;
		}
		pxaost_sc->sc_clock_count = match;
		hardclock(frame);

		oscr = bus_space_read_4(pxaost_sc->sc_iot, pxaost_sc->sc_ioh,
		    OST_OSCR4);

	} while ((signed)(oscr - match) > 0);

	 /* prevent missed interrupts */
	if (oscr - match < 5)
		match += 5;

	bus_space_write_4(pxaost_sc->sc_iot, pxaost_sc->sc_ioh, OST_OSMR4,
	    match);

	return(1);
}

int
statintr(arg)
	void *arg;
{
	struct clockframe *frame = arg;
	u_int32_t oscr, match;

	bus_space_write_4(pxaost_sc->sc_iot, pxaost_sc->sc_ioh, SAOST_SR, 0x20);

	/* schedule next clock intr */
	match = pxaost_sc->sc_statclock_count;
	do {
		match += pxaost_sc->sc_statclock_step;
		pxaost_sc->sc_statclock_count = match;
		statclock(frame);

		oscr = bus_space_read_4(pxaost_sc->sc_iot, pxaost_sc->sc_ioh,
		    OST_OSCR4);

	} while ((signed)(oscr - match) > 0);

	 /* prevent missed interrupts */
	if (oscr - match < 5)
		match += 5;
	bus_space_write_4(pxaost_sc->sc_iot, pxaost_sc->sc_ioh, OST_OSMR5,
	    match);

	return(1);
}

void
setstatclockrate(int newstathz)
{
	u_int32_t count;
	pxaost_sc->sc_statclock_step = CLK4_TIMER_FREQUENCY / newstathz;
	count = bus_space_read_4(pxaost_sc->sc_iot, pxaost_sc->sc_ioh, OST_OSCR4);
	count += pxaost_sc->sc_statclock_step;
	pxaost_sc->sc_statclock_count = count;
	bus_space_write_4(pxaost_sc->sc_iot, pxaost_sc->sc_ioh,
	    OST_OSMR5, count);
}

int
doclockintr(void *arg)
{
	u_int32_t status;
	int result = 0;

	status = bus_space_read_4(pxaost_sc->sc_iot, pxaost_sc->sc_ioh, SAOST_SR);
	if (status & 0x10)
		result |= clockintr(arg);
	if (status & 0x20)
		result |= statintr(arg);

	return (result);
}

void
cpu_initclocks()
{
	u_int32_t clk;

	stathz = STATHZ;
	profhz = stathz;
	pxaost_sc->sc_statclock_step = CLK4_TIMER_FREQUENCY / stathz;
	pxaost_sc->sc_clock_step = CLK4_TIMER_FREQUENCY / hz;
	pxaost_sc->sc_clock_step_err_cnt = CLK4_TIMER_FREQUENCY % hz;
	pxaost_sc->sc_clock_step_error = 0;

	/* Use the channels 0 and 1 for hardclock and statclock, respectively */
	pxaost_sc->sc_clock_count = pxaost_sc->sc_clock_step;
	pxaost_sc->sc_statclock_count = CLK4_TIMER_FREQUENCY / stathz;

	pxa2x0_intr_establish(PXA2X0_INT_OST, IPL_CLOCK, doclockintr, 0, "clock");

	clk = bus_space_read_4(pxaost_sc->sc_iot, pxaost_sc->sc_ioh, OST_OSCR4);

	bus_space_write_4(pxaost_sc->sc_iot, pxaost_sc->sc_ioh, SAOST_SR, 0x3f);
	bus_space_write_4(pxaost_sc->sc_iot, pxaost_sc->sc_ioh, SAOST_IR, 0x30);
	bus_space_write_4(pxaost_sc->sc_iot, pxaost_sc->sc_ioh, OST_OSMR4,
	    clk + pxaost_sc->sc_clock_count);
	bus_space_write_4(pxaost_sc->sc_iot, pxaost_sc->sc_ioh, OST_OSMR5,
	    clk + pxaost_sc->sc_statclock_count);

	tc_init(&pxaost_timecounter);
}

void
delay(usecs)
	u_int usecs;
{
	u_int32_t clock, oclock, delta, delaycnt;
	volatile int j;
	int csec, usec;

	if (usecs > (0x80000000 / (CLK4_TIMER_FREQUENCY))) {
		csec = usecs / 10000;
		usec = usecs % 10000;

		delaycnt = (CLK4_TIMER_FREQUENCY / 100) * csec +
		    (CLK4_TIMER_FREQUENCY / 100) * usec / 10000;
	} else {
		delaycnt = CLK4_TIMER_FREQUENCY * usecs / 1000000;
	}

	if (delaycnt <= 1)
		for (j = 100; j > 0; j--)
			;

	if (!pxaost_sc) {
		/* clock isn't initialized yet */
		for (; usecs > 0; usecs--)
			for (j = 100; j > 0; j--)
				;
		return;
	}

	oclock = bus_space_read_4(pxaost_sc->sc_iot, pxaost_sc->sc_ioh,
	    OST_OSCR4);

	while (1) {
		for (j = 100; j > 0; j--)
			;
		clock = bus_space_read_4(pxaost_sc->sc_iot, pxaost_sc->sc_ioh,
		    OST_OSCR4);
		delta = clock - oclock;
		if (delta > delaycnt)
			break;
	}
}
