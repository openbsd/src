/*	$OpenBSD: clock_md.c,v 1.11 2008/02/20 18:46:20 miod Exp $ */

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

#include <dev/ic/ds1687reg.h>

#include <machine/autoconf.h>
#include <machine/bus.h>

#include <mips64/archtype.h>
#include <mips64/dev/clockvar.h>

#include <sgi/localbus/macebus.h>

extern int clockmatch(struct device *, void *, void *);
extern void clockattach(struct device *, struct device *, void *);
extern void clock_int5_init(struct clock_softc *);
extern int clock_started;

#define FROMBCD(x)	(((x) >> 4) * 10 + ((x) & 0xf))
#define TOBCD(x)	(((x) / 10 * 16) + ((x) % 10))

void	ds1687_get(struct clock_softc *, time_t, struct tod_time *);
void	ds1687_set(struct clock_softc *, struct tod_time *);

void
md_clk_attach(struct device *parent, struct device *self, void *aux)
{
	struct clock_softc *sc = (struct clock_softc *)self;
	struct confargs *ca = aux;

	switch (sys_config.system_type) {
	case SGI_O2:
		sc->sc_clock.clk_get = ds1687_get;
		sc->sc_clock.clk_set = ds1687_set;
		sc->sc_clock.clk_init = clock_int5_init;
		sc->sc_clock.clk_hz = 100;
		sc->sc_clock.clk_profhz = 100;
		sc->sc_clock.clk_stathz = 0;	/* XXX no stat clock yet */
		sc->sc_clk_t = ca->ca_iot;
		if (bus_space_map(sc->sc_clk_t, MACE_ISA_RTC_OFFS, 128*256, 0,
		    &sc->sc_clk_h))
			printf("UH!? Can't map clock device!\n");
		printf(": TOD with DS1687,");

		/*
		 * XXX Expose the clock address space so that it can be used
		 * outside of clock(4). This is rather inelegant, however it
		 * will have to do for now...
		 */
		clock_h = sc->sc_clk_h;

		break;

	case SGI_O200:
		sc->sc_clock.clk_init = clock_int5_init;
		sc->sc_clock.clk_hz = 100;
		sc->sc_clock.clk_profhz = 100;
		sc->sc_clock.clk_stathz = 0;	/* XXX no stat clock yet */
		/* XXX MK48T35 */
		break;

	case SGI_OCTANE:
		sc->sc_clock.clk_init = clock_int5_init;
		sc->sc_clock.clk_hz = 100;
		sc->sc_clock.clk_profhz = 100;
		sc->sc_clock.clk_stathz = 0;	/* XXX no stat clock yet */
		/* XXX DS1687 */
		break;

	default:
		panic("don't know how to set up clock.");
	}
}


/*
 * Dallas clock driver.
 */
void
ds1687_get(struct clock_softc *sc, time_t base, struct tod_time *ct)
{
	bus_space_tag_t clk_t = sc->sc_clk_t;
	bus_space_handle_t clk_h = sc->sc_clk_h;
	int ctrl, century;

	/* Select bank 1. */
	ctrl = bus_space_read_1(clk_t, clk_h, DS1687_CTRL_A);
	bus_space_write_1(clk_t, clk_h, DS1687_CTRL_A, ctrl | DS1687_BANK_1);

	/* Select data mode 0 (BCD). */
	ctrl = bus_space_read_1(clk_t, clk_h, DS1687_CTRL_B);
	bus_space_write_1(clk_t, clk_h, DS1687_CTRL_B, ctrl & ~DS1687_DM_1);

	/* Wait for no update in progress. */
	while (bus_space_read_1(clk_t, clk_h, DS1687_CTRL_A) & DS1687_UIP)
		/* Do nothing. */;

	/* Read the RTC. */
	ct->sec = FROMBCD(bus_space_read_1(clk_t, clk_h, DS1687_SEC));
	ct->min = FROMBCD(bus_space_read_1(clk_t, clk_h, DS1687_MIN));
	ct->hour = FROMBCD(bus_space_read_1(clk_t, clk_h, DS1687_HOUR));
	ct->day = FROMBCD(bus_space_read_1(clk_t, clk_h, DS1687_DAY));
	ct->mon = FROMBCD(bus_space_read_1(clk_t, clk_h, DS1687_MONTH));
	ct->year = FROMBCD(bus_space_read_1(clk_t, clk_h, DS1687_YEAR));
	century = FROMBCD(bus_space_read_1(clk_t, clk_h, DS1687_CENTURY));

	ct->year += 100 * (century - 19);
}

void
ds1687_set(struct clock_softc *sc, struct tod_time *ct)
{
	bus_space_tag_t clk_t = sc->sc_clk_t;
	bus_space_handle_t clk_h = sc->sc_clk_h;
	int year, century, ctrl;

	century = ct->year / 100 + 19;
	year = ct->year % 100;

	/* Select bank 1. */
	ctrl = bus_space_read_1(clk_t, clk_h, DS1687_CTRL_A);
	bus_space_write_1(clk_t, clk_h, DS1687_CTRL_A, ctrl | DS1687_BANK_1);

	/* Select data mode 0 (BCD). */
	ctrl = bus_space_read_1(clk_t, clk_h, DS1687_CTRL_B);
	bus_space_write_1(clk_t, clk_h, DS1687_CTRL_B, ctrl & ~DS1687_DM_1);

	/* Prevent updates. */
	ctrl = bus_space_read_1(clk_t, clk_h, DS1687_CTRL_B);
	bus_space_write_1(clk_t, clk_h, DS1687_CTRL_B, ctrl | DS1687_SET_CLOCK);

	/* Update the RTC. */
	bus_space_write_1(clk_t, clk_h, DS1687_SEC, TOBCD(ct->sec));
	bus_space_write_1(clk_t, clk_h, DS1687_MIN, TOBCD(ct->min));
	bus_space_write_1(clk_t, clk_h, DS1687_HOUR, TOBCD(ct->hour));
	bus_space_write_1(clk_t, clk_h, DS1687_DOW, TOBCD(ct->dow));
	bus_space_write_1(clk_t, clk_h, DS1687_DAY, TOBCD(ct->day));
	bus_space_write_1(clk_t, clk_h, DS1687_MONTH, TOBCD(ct->mon));
	bus_space_write_1(clk_t, clk_h, DS1687_YEAR, TOBCD(year));
	bus_space_write_1(clk_t, clk_h, DS1687_CENTURY, TOBCD(century));

	/* Enable updates. */
	bus_space_write_1(clk_t, clk_h, DS1687_CTRL_B, ctrl);
}
