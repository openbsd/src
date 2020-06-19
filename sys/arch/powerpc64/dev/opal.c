/*	$OpenBSD: opal.c,v 1.3 2020/06/19 22:20:08 kettenis Exp $	*/
/*
 * Copyright (c) 2020 Mark Kettenis <kettenis@openbsd.org>
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

#include <machine/bus.h>
#include <machine/fdt.h>
#include <machine/opal.h>

#include <dev/clock_subr.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/fdt.h>

struct opal_softc {
	struct device		sc_dev;
	struct todr_chip_handle	sc_todr;
};

int	opal_match(struct device *, void *, void *);
void	opal_attach(struct device *, struct device *, void *);

struct cfattach	opal_ca = {
	sizeof (struct opal_softc), opal_match, opal_attach
};

struct cfdriver opal_cd = {
	NULL, "opal", DV_DULL
};

int	opal_gettime(struct todr_chip_handle *, struct timeval *);
int	opal_settime(struct todr_chip_handle *, struct timeval *);

int
opal_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "ibm,opal-v3");
}

void
opal_attach(struct device *parent, struct device *self, void *aux)
{
	struct opal_softc *sc = (struct opal_softc *)self;
	struct fdt_attach_args *faa = aux;
	int node;

	node = OF_getnodebyname(faa->fa_node, "firmware");
	if (node) {
		char version[64];

		version[0] = 0;
		OF_getprop(node, "version", version, sizeof(version));
		version[sizeof(version) - 1] = 0;
		printf(": %s", version);
	}

	printf("\n");

	sc->sc_todr.todr_gettime = opal_gettime;
	sc->sc_todr.todr_settime = opal_settime;
	todr_attach(&sc->sc_todr);
}

int
opal_gettime(struct todr_chip_handle *ch, struct timeval *tv)
{
	struct clock_ymdhms dt;
	uint64_t time;
	uint32_t date;
	int64_t error;

	do {
		error = opal_rtc_read(opal_phys(&date), opal_phys(&time));
		if (error == OPAL_BUSY_EVENT)
			opal_poll_events(NULL);
	} while (error == OPAL_BUSY_EVENT);

	if (error != OPAL_SUCCESS)
		return EIO;

	dt.dt_sec = FROMBCD((time >> 40) & 0xff);
	dt.dt_min = FROMBCD((time >> 48) & 0xff);
	dt.dt_hour = FROMBCD((time >> 56) & 0xff);
	dt.dt_day = FROMBCD((date >> 0) & 0xff);
	dt.dt_mon = FROMBCD((date >> 8) & 0xff);
	dt.dt_year = FROMBCD((date >> 16) & 0xff);
	dt.dt_year += 100 * FROMBCD((date >> 24) & 0xff);

	tv->tv_sec = clock_ymdhms_to_secs(&dt);
	tv->tv_usec = 0;

	return 0;
}

int
opal_settime(struct todr_chip_handle *ch, struct timeval *tv)
{
	struct clock_ymdhms dt;
	uint64_t time = 0;
	uint32_t date = 0;
	int64_t error;

	clock_secs_to_ymdhms(tv->tv_sec, &dt);

	time |= (uint64_t)TOBCD(dt.dt_sec) << 40;
	time |= (uint64_t)TOBCD(dt.dt_min) << 48;
	time |= (uint64_t)TOBCD(dt.dt_hour) << 56;
	date |= (uint32_t)TOBCD(dt.dt_day);
	date |= (uint32_t)TOBCD(dt.dt_mon) << 8;
	date |= (uint32_t)TOBCD(dt.dt_year) << 16;
	date |= (uint32_t)TOBCD(dt.dt_year / 100) << 24;

	do {
		error = opal_rtc_write(date, time);
		if (error == OPAL_BUSY_EVENT)
			opal_poll_events(NULL);
	} while (error == OPAL_BUSY_EVENT);

	if (error != OPAL_SUCCESS)
		return EIO;

	return 0;
}
