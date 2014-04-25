/*	$OpenBSD: sxirtc.c,v 1.4 2014/04/25 09:49:33 jsg Exp $	*/
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
/* XXX this doesn't support A20 yet. */
	/* year & 0xff on A20, 0x3f on A10 */
	/* leap << 24 on A20, << 22 on A10 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/systm.h>

#include <dev/clock_subr.h>

#include <machine/bus.h>

#include <armv7/armv7/armv7var.h>
#include <armv7/sunxi/sunxireg.h>

#define	SXIRTC_YYMMDD	0x00
#define	SXIRTC_HHMMSS	0x04

#define LEAPYEAR(y)        \
    (((y) % 4 == 0 &&    \
    (y) % 100 != 0) ||    \
    (y) % 400 == 0) 


/* XXX other way around than bus_space_subregion? */
extern bus_space_handle_t sxitimer_ioh;

extern todr_chip_handle_t todr_handle;

struct sxirtc_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
};

void	sxirtc_attach(struct device *, struct device *, void *);

struct cfattach sxirtc_ca = {
	sizeof(struct device), NULL, sxirtc_attach
};

struct cfdriver sxirtc_cd = {
	NULL, "sxirtc", DV_DULL
};

int	sxirtc_gettime(todr_chip_handle_t, struct timeval *);
int	sxirtc_settime(todr_chip_handle_t, struct timeval *);

void
sxirtc_attach(struct device *parent, struct device *self, void *args)
{
	struct sxirtc_softc *sc = (struct sxirtc_softc *)self;
	struct armv7_attach_args *aa = args;
	todr_chip_handle_t handle;

	handle = malloc(sizeof(struct todr_chip_handle), M_DEVBUF, M_NOWAIT);
	if (handle == NULL)
		panic("sxirtc_attach: couldn't allocate todr_handle");

	sc->sc_iot = aa->aa_iot;
	if (bus_space_subregion(sc->sc_iot, sxitimer_ioh,
	    aa->aa_dev->mem[0].addr, aa->aa_dev->mem[0].size, &sc->sc_ioh))
		panic("sxirtc_attach: bus_space_subregion failed!");

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
	dt.dt_year = (reg >> 16 & 0x3f) + 2010; /* 0xff on A20 */

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

	SXICMS4(sc, SXIRTC_YYMMDD, 0x00400000 | 0x003f0000 | 0x0f00 | 0x1f,
	   dt.dt_day | (dt.dt_mon << 8) | ((dt.dt_year - 2010) << 16) |
	   (LEAPYEAR(dt.dt_year) << 22));

	return 0;
}
