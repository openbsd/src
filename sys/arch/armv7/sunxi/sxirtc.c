/*	$OpenBSD: sxirtc.c,v 1.7 2016/08/05 21:29:23 kettenis Exp $	*/
/*
 * Copyright (c) 2008 Mark Kettenis
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
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/systm.h>

#include <dev/clock_subr.h>

#include <machine/bus.h>
#include <machine/fdt.h>

#include <armv7/armv7/armv7var.h>
#include <armv7/sunxi/sunxireg.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/fdt.h>

#define	SXIRTC_YYMMDD	0x04
#define	SXIRTC_HHMMSS	0x08

#define LEAPYEAR(y)        \
    (((y) % 4 == 0 &&    \
    (y) % 100 != 0) ||    \
    (y) % 400 == 0) 


extern todr_chip_handle_t todr_handle;

struct sxirtc_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	uint32_t		base_year;
	uint32_t		year_mask;
	uint32_t		leap_shift;
};

int	sxirtc_match(struct device *, void *, void *);
void	sxirtc_attach(struct device *, struct device *, void *);

struct cfattach sxirtc_ca = {
	sizeof(struct device), sxirtc_match, sxirtc_attach
};

struct cfdriver sxirtc_cd = {
	NULL, "sxirtc", DV_DULL
};

int	sxirtc_gettime(todr_chip_handle_t, struct timeval *);
int	sxirtc_settime(todr_chip_handle_t, struct timeval *);

int
sxirtc_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return (OF_is_compatible(faa->fa_node, "allwinner,sun4i-a10-rtc") ||
	    OF_is_compatible(faa->fa_node, "allwinner,sun7i-a20-rtc"));
}

void
sxirtc_attach(struct device *parent, struct device *self, void *aux)
{
	struct sxirtc_softc *sc = (struct sxirtc_softc *)self;
	struct fdt_attach_args *faa = aux;
	todr_chip_handle_t handle;

	if (faa->fa_nreg < 1)
		return;

	handle = malloc(sizeof(struct todr_chip_handle), M_DEVBUF, M_NOWAIT);
	if (handle == NULL)
		panic("sxirtc_attach: couldn't allocate todr_handle");

	sc->sc_iot = faa->fa_iot;
	if (bus_space_map(sc->sc_iot, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sc->sc_ioh))
		panic("sxirtc_attach: bus_space_map failed!");

	if (OF_is_compatible(faa->fa_node, "allwinner,sun7i-a20-rtc")) {
		sc->base_year = 1970;
		sc->year_mask = 0xff;
		sc->leap_shift = 24;
	} else {
		sc->base_year = 2010;
		sc->year_mask = 0x3f;
		sc->leap_shift = 22;
	}

	handle->cookie = self;
	handle->todr_gettime = sxirtc_gettime;
	handle->todr_settime = sxirtc_settime;
	handle->todr_getcal = NULL;
	handle->todr_setcal = NULL;
	handle->bus_cookie = NULL;
	handle->todr_setwen = NULL;
	todr_handle = handle;

	printf("\n");
}

int
sxirtc_gettime(todr_chip_handle_t handle, struct timeval *tv)
{
	struct sxirtc_softc *sc = (struct sxirtc_softc *)handle->cookie;
	struct clock_ymdhms dt;
	uint32_t reg;

	reg = SXIREAD4(sc, SXIRTC_HHMMSS);
	dt.dt_sec = reg & 0x3f;
	dt.dt_min = reg >> 8 & 0x3f;
	dt.dt_hour = reg >> 16 & 0x1f;
	dt.dt_wday = reg >> 29 & 0x07;

	reg = SXIREAD4(sc, SXIRTC_YYMMDD);
	dt.dt_day = reg & 0x1f;
	dt.dt_mon = reg >> 8 & 0x0f;
	dt.dt_year = (reg >> 16 & sc->year_mask) + sc->base_year;

	if (dt.dt_sec > 59 || dt.dt_min > 59 ||
	    dt.dt_hour > 23 || dt.dt_wday > 6 ||
	    dt.dt_day > 31 || dt.dt_day == 0 ||
	    dt.dt_mon > 12 || dt.dt_mon == 0)
		return 1;

	tv->tv_sec = clock_ymdhms_to_secs(&dt);
	tv->tv_usec = 0;
	return 0;
}

int
sxirtc_settime(todr_chip_handle_t handle, struct timeval *tv)
{
	struct sxirtc_softc *sc = (struct sxirtc_softc *)handle->cookie;
	struct clock_ymdhms dt;

	clock_secs_to_ymdhms(tv->tv_sec, &dt);

	if (dt.dt_sec > 59 || dt.dt_min > 59 ||
	    dt.dt_hour > 23 || dt.dt_wday > 6 ||
	    dt.dt_day > 31 || dt.dt_day == 0 ||
	    dt.dt_mon > 12 || dt.dt_mon == 0)
		return 1;

	SXICMS4(sc, SXIRTC_HHMMSS, 0xe0000000 | 0x1f0000 | 0x3f00 | 0x3f,
	    dt.dt_sec | (dt.dt_min << 8) | (dt.dt_hour << 16) |
	    (dt.dt_wday << 29));

	SXICMS4(sc, SXIRTC_YYMMDD, 0x00400000 | (sc->year_mask << 16) |
	    0x0f00 | 0x1f, dt.dt_day | (dt.dt_mon << 8) |
	    ((dt.dt_year - sc->base_year) << 16) |
	    (LEAPYEAR(dt.dt_year) << sc->leap_shift));

	return 0;
}
