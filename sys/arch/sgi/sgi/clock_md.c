/*	$OpenBSD: clock_md.c,v 1.7 2004/10/20 12:49:15 pefo Exp $ */

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

struct cfattach clock_macebus_ca = {
        sizeof(struct clock_softc), clockmatch, clockattach
};
struct cfattach clock_xbowmux_ca = {
        sizeof(struct clock_softc), clockmatch, clockattach
};


void	md_clk_attach(struct device *parent, struct device *self, void *aux);

void	ds1687_get(struct clock_softc *, time_t, struct tod_time *);
void	ds1687_set(struct clock_softc *, struct tod_time *);

void
md_clk_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct clock_softc *sc = (struct clock_softc *)self;
	struct confargs *ca;

	ca = aux;

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
		break;

	case SGI_O200:
		sc->sc_clock.clk_init = clock_int5_init;
		sc->sc_clock.clk_hz = 100;
		sc->sc_clock.clk_profhz = 100;
		sc->sc_clock.clk_stathz = 0;	/* XXX no stat clock yet */
		printf("TODO set up clock.");
		break;

	default:
		printf("don't know how to set up clock.");
	}
}


/*
 *  Dallas clock driver.
 */
void
ds1687_get(sc, base, ct)
	struct clock_softc *sc;
	time_t base;
	struct tod_time *ct;
{
	bus_space_tag_t clk_t = sc->sc_clk_t;
	bus_space_handle_t clk_h = sc->sc_clk_h;
	int tmp12, century;

	/* Select bank 1 */
	tmp12 = bus_space_read_1(clk_t, clk_h, 12);
	bus_space_write_1(clk_t, clk_h, 12, tmp12 | 0x10);

	/* Wait for no update in progress */
	while (bus_space_read_1(clk_t, clk_h, 12) & 0x80)
		/* Do nothing */;

	ct->sec = FROMBCD(bus_space_read_1(clk_t, clk_h, 0));
	ct->min = FROMBCD(bus_space_read_1(clk_t, clk_h, 2));
	ct->hour = FROMBCD(bus_space_read_1(clk_t, clk_h, 4));
	ct->day = FROMBCD(bus_space_read_1(clk_t, clk_h, 7));
	ct->mon = FROMBCD(bus_space_read_1(clk_t, clk_h, 8));
	ct->year = FROMBCD(bus_space_read_1(clk_t, clk_h, 9));
	century = FROMBCD(bus_space_read_1(clk_t, clk_h, 72));

	bus_space_write_1(clk_t, clk_h, 12, tmp12 | 0x10);

	ct->year += 100 * (century - 19);
}

void
ds1687_set(sc, ct)
	struct clock_softc *sc;
	struct tod_time *ct;
{
	bus_space_tag_t clk_t = sc->sc_clk_t;
	bus_space_handle_t clk_h = sc->sc_clk_h;
	int year, century, tmp12, tmp13;

	century = ct->year / 100 + 19;
	year = ct->year % 100;

	/* Select bank 1 */
	tmp12 = bus_space_read_1(clk_t, clk_h, 12);
	bus_space_write_1(clk_t, clk_h, 12, tmp12 | 0x10);

	/* Stop update */
	tmp13 = bus_space_read_1(clk_t, clk_h, 13);
	bus_space_write_1(clk_t, clk_h, 13, tmp13 | 0x80);

	bus_space_write_1(clk_t, clk_h, 0, TOBCD(ct->sec));
	bus_space_write_1(clk_t, clk_h, 2, TOBCD(ct->min));
	bus_space_write_1(clk_t, clk_h, 4, TOBCD(ct->hour));
	bus_space_write_1(clk_t, clk_h, 6, TOBCD(ct->dow));
	bus_space_write_1(clk_t, clk_h, 7, TOBCD(ct->day));
	bus_space_write_1(clk_t, clk_h, 8, TOBCD(ct->mon));
	bus_space_write_1(clk_t, clk_h, 9, TOBCD(year));
	bus_space_write_1(clk_t, clk_h, 72, TOBCD(century));

	bus_space_write_1(clk_t, clk_h, 12, tmp12);
	bus_space_write_1(clk_t, clk_h, 13, tmp13);
}
