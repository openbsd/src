/*	$OpenBSD: omdog.c,v 1.5 2014/12/10 12:27:56 mikeb Exp $	*/
/*
 * Copyright (c) 2013 Federico G. Schwindt <fgsch@openbsd.org>
 * Copyright (c) 2007,2009 Dale Rahn <drahn@openbsd.org>
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
#include <sys/evcount.h>
#include <sys/socket.h>
#include <sys/timeout.h>
#include <machine/intr.h>
#include <machine/bus.h>
#include <armv7/armv7/armv7var.h>

#define WIDR		0x00			/* Identification Register */
#define WCLR		0x24			/* Control Register */
#define  WCLR_PRE		(1 << 5)
#define  WCLR_PTV		(0 << 2)
#define WCRR		0x28			/* Counter Register */
#define WLDR		0x2c			/* Load Register */
#define WTGR		0x30			/* Trigger Register */
#define WWPS		0x34			/* Write Posting Bits Reg. */
#define  WWPS_WSPR		(1 << 4)
#define  WWPS_WTGR		(1 << 3)
#define  WWPS_WLDR		(1 << 2)
#define  WWPS_WCRR		(1 << 1)
#define  WWPS_WCLR		(1 << 0)
#define WSPR		0x48			/* Start/Stop Register */

#define OMDOG_VAL(secs)	(0xffffffff - ((secs) * (32768 / (1 << 0))) + 1)


struct omdog_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;

	int			sc_period;
};

struct omdog_softc *omdog_sc;

void	omdog_attach(struct device *, struct device *, void *);
int	omdog_activate(struct device *, int);
void	omdog_start(struct omdog_softc *);
void	omdog_stop(struct omdog_softc *);
void	omdog_sync(struct omdog_softc *);
int	omdog_cb(void *, int);
void	omdog_reset(void);

struct cfattach	omdog_ca = {
	sizeof (struct omdog_softc), NULL, omdog_attach, NULL, omdog_activate
};

struct cfdriver omdog_cd = {
	NULL, "omdog", DV_DULL
};

void
omdog_attach(struct device *parent, struct device *self, void *args)
{
	struct armv7_attach_args *aa = args;
	struct omdog_softc *sc = (struct omdog_softc *) self;
	u_int32_t rev;

	sc->sc_iot = aa->aa_iot;
	if (bus_space_map(sc->sc_iot, aa->aa_dev->mem[0].addr,
	    aa->aa_dev->mem[0].size, 0, &sc->sc_ioh))
		panic("%s: bus_space_map failed!", __func__);

	rev = bus_space_read_4(sc->sc_iot, sc->sc_ioh, WIDR);

	printf(" rev %d.%d\n", rev >> 4 & 0xf, rev & 0xf);
	omdog_sc = sc;

	omdog_stop(sc);

#ifndef SMALL_KERNEL
	wdog_register(omdog_cb, sc);
#endif
}

int
omdog_activate(struct device *self, int act)
{
	switch (act) {
	case DVACT_POWERDOWN:
#ifndef SMALL_KERNEL
		wdog_shutdown(self);
#endif
		break;
	}

	return (0);
}

void
omdog_start(struct omdog_softc *sc)
{
	/* Write the enable sequence data BBBBh followed by 4444h */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, WSPR, 0xbbbb);
	omdog_sync(sc);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, WSPR, 0x4444);
	omdog_sync(sc);
}

void
omdog_stop(struct omdog_softc *sc)
{
	/* Write the disable sequence data AAAAh followed by 5555h */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, WSPR, 0xaaaa);
	omdog_sync(sc);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, WSPR, 0x5555);
	omdog_sync(sc);
}

void
omdog_sync(struct omdog_softc *sc)
{
	while (bus_space_read_4(sc->sc_iot, sc->sc_ioh, WWPS) &
	    (WWPS_WSPR|WWPS_WTGR|WWPS_WLDR|WWPS_WCRR|WWPS_WCLR))
		delay(10);
}

int
omdog_cb(void *self, int period)
{
	struct omdog_softc *sc = self;

	if (sc->sc_period != 0 && sc->sc_period != period)
		omdog_stop(sc);

	if (period != 0) {
		if (sc->sc_period != period) {
			/* Set the prescaler */
			bus_space_write_4(sc->sc_iot, sc->sc_ioh, WCLR,
			    (WCLR_PRE|WCLR_PTV));

			/* Set the reload counter */
			bus_space_write_4(sc->sc_iot, sc->sc_ioh, WLDR,
			    OMDOG_VAL(period));
		}

		omdog_sync(sc);

		/* Trigger the reload */
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, WTGR,
		    ~bus_space_read_4(sc->sc_iot, sc->sc_ioh, WTGR));

		if (sc->sc_period != period)
			omdog_start(sc);
	}

	sc->sc_period = period;

	return (period);
}

void
omdog_reset(void)
{
	if (omdog_sc == NULL)
		return;

	if (omdog_sc->sc_period != 0)
		omdog_stop(omdog_sc);

	bus_space_write_4(omdog_sc->sc_iot, omdog_sc->sc_ioh, WCRR,
	    0xffffff80);

	omdog_start(omdog_sc);

	delay(100000);
}
