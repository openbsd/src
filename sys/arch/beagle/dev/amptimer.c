/* $OpenBSD: amptimer.c,v 1.1 2011/11/05 11:48:26 drahn Exp $ */
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
#include <arch/beagle/beagle/ahb.h>

#define GTIMER_CNT_LOW		0x00
#define GTIMER_CNT_HIGH		0x04
#define GTIMER_CTRL		0x08
#define 	GTIMER_CTRL_AA		(1 << 3)
#define 	GTIMER_CTRL_IRQ		(1 << 2)
#define 	GTIMER_CTRL_COMP	(1 << 1)
#define 	GTIMER_CTRL_TIMER	(1 << 0)
#define GTIMER_STATUS		0x0c
#define 	GTIMER_STATUS_EVENT		(1 << 0)
#define GTIMER_CMP_LOW		0x10
#define GTIMER_CMP_HIGH		0x14
#define GTIMER_AUTOINC		0x18

#define GTIMER_SIZE		0x20

/* XXX - PERIPHCLK */
#define TIMER_FREQUENCY                 32768

u_int amptimer_get_timecount(struct timecounter *);

static struct timecounter amptimer_timecounter = {
        amptimer_get_timecount, NULL, 0x7fffffff, 0, "amptimer", 0, NULL
};

struct amptimer_softc {
	struct device		sc_dev;
        bus_space_tag_t		sc_iot;
        bus_space_handle_t	sc_ioh;
	volatile u_int64_t	sc_nexttickevent;
	volatile u_int64_t	sc_nextstatevent;
	u_int32_t		sc_ticks_per_second;
	u_int32_t		sc_ticks_per_intr;
	u_int32_t		sc_ticks_err_cnt;
	u_int32_t		sc_ticks_err_sum;
	u_int32_t		sc_statvar;
	u_int32_t		sc_statmin;

	struct evcount		sc_clk_count;
	struct evcount		sc_stat_count;

};

int		amptimer_match(struct device *, void *, void *);
void		amptimer_attach(struct device *, struct device *, void *);
uint64_t	amptimer_readcnt64(struct amptimer_softc *sc);
int		amptimer_intr(void *);
void		amptimer_cpu_initclocks(void);

struct cfattach amptimer_ca = {
	sizeof (struct amptimer_softc), amptimer_match, amptimer_attach
};

struct cfdriver amptimer_cd = {
	NULL, "amptimer", DV_DULL
};

int
amptimer_match(struct device *parent, void *v, void *aux)
{
	switch (board_id) {
	case BOARD_ID_OMAP3_BEAGLE:
		break; /* continue trying */
	case BOARD_ID_OMAP4_PANDA:
		return 0; /* not ported yet ??? - different */
	default:
		return 0; /* unknown */
	}
	return (1);
}

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


void
amptimer_attach(struct device *parent, struct device *self, void *args)
{
	struct amptimer_softc *sc = (struct amptimer_softc *)self;
	struct ahb_attach_args *aa = args;
        bus_space_handle_t ioh;

	sc->sc_iot = aa->aa_iot;

        if (bus_space_map(sc->sc_iot, aa->aa_addr, GTIMER_SIZE, 0, &ioh))
                panic("amptimer_attach: bus_space_map failed!");

	sc->sc_ioh = ioh;

	/* disable global timer */
	bus_space_write_4(sc->sc_iot, ioh, GTIMER_CTRL, 0);

	/* XXX ??? reset counters to 0 - gives us uptime in the counter */
	bus_space_write_4(sc->sc_iot, ioh, GTIMER_CNT_LOW, 0);
	bus_space_write_4(sc->sc_iot, ioh, GTIMER_CNT_HIGH, 0);

	/* enable global timer */
	bus_space_write_4(sc->sc_iot, ioh, GTIMER_CTRL, GTIMER_CTRL_TIMER);

	/* clear event */
	bus_space_write_4(sc->sc_iot, ioh, GTIMER_STATUS, 1);

	/*
	 * comparitor registers and interrupts not enabled until
	 * timer configures
	 */
	/*
	 * XXX - configure delay -> amptimer_delay()
	 * XXX - configure cpu_initclocks -> amptimer_initclocks()
	 */
	sc->sc_ticks_per_second = TIMER_FREQUENCY;

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
	uint64_t		 now;
	uint64_t		 nextevent;
	uint32_t		 r, reg;
	int			 skip = 1;
	int			 rc = 0;

	/*
	 * DSR - I know that the tick timer is 64 bits, but the following
	 * code deals with rollover, so there is no point in dealing
	 * with the 64 bit math, just let the 32 bit rollover 
	 * do the right thing
	 */

	now = bus_space_read_4(sc->sc_iot, sc->sc_ioh, GTIMER_CNT_LOW);

	while (sc->sc_nexttickevent <= now) {
		sc->sc_nexttickevent += sc->sc_ticks_per_intr;
		sc->sc_ticks_err_sum += sc->sc_ticks_err_cnt;
		/* looping a few times is faster than divide */
		while (sc->sc_ticks_err_sum > hz) {
			sc->sc_nexttickevent += 1;
			sc->sc_ticks_err_sum -= hz;
		}

		/* looping a few times is faster than divide */
		while (sc->sc_ticks_err_sum  > hz) {
			sc->sc_nexttickevent += 1;
			sc->sc_ticks_err_sum -= hz;
		}

		sc->sc_clk_count.ec_count++;
		rc = 1;
		hardclock(frame);
	}
	while (sc->sc_nextstatevent <= now) {
		do {
			r = random() & (sc->sc_statvar -1);
		} while (r == 0); /* random == 0 not allowed */
		sc->sc_nextstatevent += sc->sc_statmin + r;

		/* XXX - correct nextstatevent? */
		sc->sc_stat_count.ec_count++;
		rc = 1;
		statclock(frame);
	}

	if ((sc->sc_nexttickevent - now) < (sc->sc_nextstatevent - now))
		nextevent = sc->sc_nexttickevent;
	else
		nextevent = sc->sc_nextstatevent;

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
		
	now = amptimer_readcnt64(sc) + sc->sc_ticks_per_intr;
	if (now >= nextevent) {
		nextevent = now + skip;
		skip += 1;
		goto again;
	}

	return (rc);
}

void
amptimer_cpu_initclocks()
{
	struct amptimer_softc	*sc = amptimer_cd.cd_devs[0];
	uint64_t		 next;
	uint32_t		 reg;

	stathz = 128;
	profhz = 1024;

	sc->sc_ticks_per_second = TIMER_FREQUENCY;

	setstatclockrate(stathz);

	sc->sc_ticks_per_intr = sc->sc_ticks_per_second / hz;
	sc->sc_ticks_err_cnt = sc->sc_ticks_per_second % hz;
	sc->sc_ticks_err_sum = 0;; 

	/* establish interrupts */
	/* XXX - irq */
	arm_intr_establish(27, IPL_CLOCK, amptimer_intr,
	    NULL, "tick");

	/* setup timer 0 (hardware timer 2) */
	next = amptimer_readcnt64(sc) + sc->sc_ticks_per_intr;
	sc->sc_nexttickevent = sc->sc_nextstatevent = next;

	reg = bus_space_read_4(sc->sc_iot, sc->sc_ioh, GTIMER_CTRL);
	reg &= ~GTIMER_CTRL_COMP;
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GTIMER_CTRL, reg);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GTIMER_CMP_LOW,
	    next & 0xffffffff);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GTIMER_CMP_HIGH,
	    next >> 32);
	reg |= GTIMER_CTRL_COMP | GTIMER_CTRL_IRQ;
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GTIMER_CTRL, reg);
	
}
