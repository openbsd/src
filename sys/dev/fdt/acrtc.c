/*	$OpenBSD: acrtc.c,v 1.1 2017/12/16 10:22:13 kettenis Exp $	*/
/*
 * Copyright (c) 2017 Mark Kettenis <kettenis@openbsd.org>
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
#include <sys/device.h>
#include <sys/malloc.h>

#include <dev/fdt/rsbvar.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/fdt.h>

#include <dev/clock_subr.h>

#define isleap(y) ((((y) % 4) == 0 && ((y) % 100) != 0) || ((y) % 400) == 0)

extern todr_chip_handle_t todr_handle;

#define RTC_CTRL		0xc7
#define  RTC_CTRL_12H_24H_MODE	(1 << 0)
#define RTC_SEC			0xc8
#define RTC_MIN			0xc9
#define RTC_HOU			0xca
#define RTC_WEE			0xcb
#define RTC_DAY			0xcc
#define RTC_MON			0xcd
#define RTC_YEA			0xce
#define  RTC_YEA_LEAP_YEAR	(1 << 15)
#define RTC_UPD_TRIG		0xcf
#define  RTC_UPD_TRIG_UPDATE	(1 << 15)

struct acrtc_softc {
	struct device	sc_dev;
	void		*sc_cookie;
	uint16_t 	sc_rta;

	struct todr_chip_handle sc_todr;
};

int	acrtc_match(struct device *, void *, void *);
void	acrtc_attach(struct device *, struct device *, void *);

struct cfattach acrtc_ca = {
	sizeof(struct acrtc_softc), acrtc_match, acrtc_attach
};

struct cfdriver acrtc_cd = {
	NULL, "acrtc", DV_DULL
};

int	acrtc_clock_read(struct acrtc_softc *, struct clock_ymdhms *);
int	acrtc_clock_write(struct acrtc_softc *, struct clock_ymdhms *);
int	acrtc_gettime(struct todr_chip_handle *, struct timeval *);
int	acrtc_settime(struct todr_chip_handle *, struct timeval *);

int
acrtc_match(struct device *parent, void *match, void *aux)
{
	struct rsb_attach_args *ra = aux;

	if (strcmp(ra->ra_name, "x-powers,ac100") == 0)
		return 1;
	return 0;
}

void
acrtc_attach(struct device *parent, struct device *self, void *aux)
{
	struct acrtc_softc *sc = (struct acrtc_softc *)self;
	struct rsb_attach_args *ra = aux;

	sc->sc_cookie = ra->ra_cookie;
	sc->sc_rta = ra->ra_rta;

	printf("\n");

	sc->sc_todr.cookie = sc;
	sc->sc_todr.todr_gettime = acrtc_gettime;
	sc->sc_todr.todr_settime = acrtc_settime;
	todr_handle = &sc->sc_todr;
}

inline uint16_t
acrtc_read_reg(struct acrtc_softc *sc, uint8_t reg)
{
	return rsb_read_2(sc->sc_cookie, sc->sc_rta, reg);
}

inline void
acrtc_write_reg(struct acrtc_softc *sc, uint8_t reg, uint16_t value)
{
	rsb_write_2(sc->sc_cookie, sc->sc_rta, reg, value);
}

int
acrtc_gettime(struct todr_chip_handle *handle, struct timeval *tv)
{
	struct acrtc_softc *sc = handle->cookie;
	struct clock_ymdhms dt;
	int error;

	error = acrtc_clock_read(sc, &dt);
	if (error)
		return error;

	if (dt.dt_sec > 59 || dt.dt_min > 59 || dt.dt_hour > 23 ||
	    dt.dt_day > 31 || dt.dt_day == 0 ||
	    dt.dt_mon > 12 || dt.dt_mon == 0 ||
	    dt.dt_year < POSIX_BASE_YEAR)
		return EINVAL;

	tv->tv_sec = clock_ymdhms_to_secs(&dt);
	tv->tv_usec = 0;
	return 0;
}

int
acrtc_settime(struct todr_chip_handle *handle, struct timeval *tv)
{
	struct acrtc_softc *sc = handle->cookie;
	struct clock_ymdhms dt;

	clock_secs_to_ymdhms(tv->tv_sec, &dt);

	return acrtc_clock_write(sc, &dt);
}

int
acrtc_clock_read(struct acrtc_softc *sc, struct clock_ymdhms *dt)
{
	uint16_t ctrl;

	dt->dt_sec = FROMBCD(acrtc_read_reg(sc, RTC_SEC));
	dt->dt_min = FROMBCD(acrtc_read_reg(sc, RTC_MIN));
	dt->dt_hour = FROMBCD(acrtc_read_reg(sc, RTC_HOU));
	dt->dt_day = FROMBCD(acrtc_read_reg(sc, RTC_DAY));
	dt->dt_mon = FROMBCD(acrtc_read_reg(sc, RTC_MON));
	dt->dt_year = FROMBCD(acrtc_read_reg(sc, RTC_YEA)) + 2000;

#ifdef DEBUG
	printf("%02d/%02d/%04d %02d:%02d:%0d\n", dt->dt_day, dt->dt_mon,
	    dt->dt_year, dt->dt_hour, dt->dt_min, dt->dt_sec);
#endif

	/* Consider the time to be invalid if the clock is in 12H mode. */
	ctrl = acrtc_read_reg(sc, RTC_CTRL);
	if ((ctrl & RTC_CTRL_12H_24H_MODE) == 0)
		return EINVAL;

	return 0;
}

int
acrtc_clock_write(struct acrtc_softc *sc, struct clock_ymdhms *dt)
{
	uint16_t leap = isleap(dt->dt_year) ? RTC_YEA_LEAP_YEAR : 0;

	acrtc_write_reg(sc, RTC_SEC, TOBCD(dt->dt_sec));
	acrtc_write_reg(sc, RTC_MIN, TOBCD(dt->dt_min));
	acrtc_write_reg(sc, RTC_HOU, TOBCD(dt->dt_hour));
	acrtc_write_reg(sc, RTC_WEE, TOBCD(dt->dt_wday));
	acrtc_write_reg(sc, RTC_DAY, TOBCD(dt->dt_day));
	acrtc_write_reg(sc, RTC_MON, TOBCD(dt->dt_mon));
	acrtc_write_reg(sc, RTC_YEA, TOBCD(dt->dt_year - 2000) | leap);
	acrtc_write_reg(sc, RTC_UPD_TRIG, RTC_UPD_TRIG_UPDATE);

	/* Switch to 24H mode to indicate the time is now valid. */
	acrtc_write_reg(sc, RTC_CTRL, RTC_CTRL_12H_24H_MODE);

	return 0;
}
