/*	$OpenBSD: clock_md.c,v 1.1 2004/08/06 21:12:19 pefo Exp $ */

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

#include <dev/ic/mc146818reg.h>
#include <machine/m48t37.h>

#include <pmonmips/localbus/localbus.h>

#include <mips64/archtype.h>

#include <mips64/dev/clockvar.h>

extern void clock_int5_init(struct clock_softc *);
extern int clock_started;

#define FROMBCD(x)      (((x) >> 4) * 10 + ((x) & 0xf))
#define TOBCD(x)        (((x) / 10 * 16) + ((x) % 10))

void	md_clk_attach(struct device *parent, struct device *self, void *aux);

void	m48clock_get(struct clock_softc *, time_t, struct tod_time *);
void	m48clock_set(struct clock_softc *, struct tod_time *);

void	dsclock_get(struct clock_softc *, time_t, struct tod_time *);
void	dsclock_set(struct clock_softc *, struct tod_time *);

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
		sc->sc_clock.clk_get = m48clock_get;
		sc->sc_clock.clk_set = m48clock_set;
		sc->sc_clock.clk_init = clock_int5_init;
		sc->sc_clock.clk_hz = 100;
		sc->sc_clock.clk_profhz = 100;
		sc->sc_clock.clk_stathz = 100;
#if 0
		if (bus_space_map(sc->sc_clk_t, 0xfc807ff0, 16, 0,
		    &sc->sc_clk_h))
			printf("UH!? Can't map clock device!\n");
		printf(" M48x compatible using counter as ticker");
#endif
		break;

	default:
		printf("don't know how to set up clock.");
	}
}

/*
 *  M48TXX clock driver.
 */
void
m48clock_get(sc, base, ct)
	struct clock_softc *sc;
	time_t base;
	struct tod_time *ct;
{
	bus_space_tag_t clk_t = sc->sc_clk_t;
	bus_space_handle_t clk_h = sc->sc_clk_h;
        int century, tmp;

return;
	tmp = bus_space_read_1(clk_t, clk_h, TOD_CTRL) | TOD_CTRL_R;
	bus_space_write_1(clk_t, clk_h, TOD_CTRL, tmp);

	ct->sec = FROMBCD(bus_space_read_1(clk_t, clk_h, TOD_SECOND));
	ct->min = FROMBCD(bus_space_read_1(clk_t, clk_h, TOD_MINUTE));
	ct->hour = FROMBCD(bus_space_read_1(clk_t, clk_h, TOD_HOUR));
	ct->dow = FROMBCD(bus_space_read_1(clk_t, clk_h, TOD_DAY));
	ct->day = FROMBCD(bus_space_read_1(clk_t, clk_h, TOD_DATE));
	ct->mon = FROMBCD(bus_space_read_1(clk_t, clk_h, TOD_MONTH)) - 1;
	ct->year = FROMBCD(bus_space_read_1(clk_t, clk_h, TOD_YEAR));
	century = FROMBCD(bus_space_read_1(clk_t, clk_h, TOD_CENTURY));
	tmp = bus_space_read_1(clk_t, clk_h, TOD_CTRL) & ~TOD_CTRL_R;
	bus_space_write_1(clk_t, clk_h, TOD_CTRL, tmp);

        /* Since tm_year is defined to be years since 1900 we compute */
        /* the correct value here                                     */
        ct->year = century*100 + ct->year - 1900;
}

void
m48clock_set(sc, ct)
	struct clock_softc *sc;
	struct tod_time *ct;
{
	bus_space_tag_t clk_t = sc->sc_clk_t;
	bus_space_handle_t clk_h = sc->sc_clk_h;
        int tmp;
	int year, century;

return;
	century = (ct->year + 1900) / 100;
	year = ct->year % 100;

	tmp = bus_space_read_1(clk_t, clk_h, TOD_CTRL) | TOD_CTRL_W;
	bus_space_write_1(clk_t, clk_h, TOD_CTRL, tmp);

	bus_space_write_1(clk_t, clk_h, TOD_SECOND, TOBCD(ct->sec));
	bus_space_write_1(clk_t, clk_h, TOD_MINUTE, TOBCD(ct->min));
	bus_space_write_1(clk_t, clk_h, TOD_HOUR, TOBCD(ct->hour));
	bus_space_write_1(clk_t, clk_h, TOD_DAY, TOBCD(ct->dow));
	bus_space_write_1(clk_t, clk_h, TOD_DATE, TOBCD(ct->day));
	bus_space_write_1(clk_t, clk_h, TOD_MONTH, TOBCD(ct->mon + 1));
	bus_space_write_1(clk_t, clk_h, TOD_YEAR, TOBCD(year));
	bus_space_write_1(clk_t, clk_h, TOD_CENTURY, TOBCD(century));

	tmp = bus_space_read_1(clk_t, clk_h, TOD_CTRL) & ~TOD_CTRL_W;
	bus_space_write_1(clk_t, clk_h, TOD_CTRL, tmp);
}


/*
 *  Dallas clock driver.
 */
void
dsclock_get(sc, base, ct)
	struct clock_softc *sc;
	time_t base;
	struct tod_time *ct;
{
	bus_space_tag_t clk_t = sc->sc_clk_t;
	bus_space_handle_t clk_h = sc->sc_clk_h;
        int century, tmp;

	tmp = bus_space_read_1(clk_t, clk_h, 15);
	bus_space_write_1(clk_t, clk_h, 15, tmp | 0x80);

	ct->sec = FROMBCD(bus_space_read_1(clk_t, clk_h, 0));
	ct->min = FROMBCD(bus_space_read_1(clk_t, clk_h, 1));
	ct->hour = FROMBCD(bus_space_read_1(clk_t, clk_h, 2));
	ct->day = FROMBCD(bus_space_read_1(clk_t, clk_h, 4));
	ct->mon = FROMBCD(bus_space_read_1(clk_t, clk_h, 5)) - 1;
	ct->year = FROMBCD(bus_space_read_1(clk_t, clk_h, 6));
	bus_space_write_1(clk_t, clk_h, 15, tmp);

        /* Since tm_year is defined to be years since 1900 we compute */
        /* the correct value here                                     */
        ct->year = century*100 + ct->year - 1900;
}

void
dsclock_set(sc, ct)
	struct clock_softc *sc;
	struct tod_time *ct;
{
	bus_space_tag_t clk_t = sc->sc_clk_t;
	bus_space_handle_t clk_h = sc->sc_clk_h;
        int tmp;
	int year, century;

	century = (ct->year + 1900) / 100;
	year = ct->year % 100;

	tmp = bus_space_read_1(clk_t, clk_h, 15);
	bus_space_write_1(clk_t, clk_h, 15, tmp | 0x80);

	bus_space_write_1(clk_t, clk_h, 0, TOBCD(ct->sec));
	bus_space_write_1(clk_t, clk_h, 1, TOBCD(ct->min));
	bus_space_write_1(clk_t, clk_h, 2, TOBCD(ct->hour));
	bus_space_write_1(clk_t, clk_h, 3, TOBCD(ct->dow));
	bus_space_write_1(clk_t, clk_h, 4, TOBCD(ct->day));
	bus_space_write_1(clk_t, clk_h, 5, TOBCD(ct->mon + 1));
	bus_space_write_1(clk_t, clk_h, 6, TOBCD(year));

	bus_space_write_1(clk_t, clk_h, 15, tmp);
}
