/* $OpenBSD: omdog.c,v 1.1 2013/09/04 14:38:31 patrick Exp $ */
/*
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
#include <armv7/omap/omapvar.h>
#include <armv7/omap/omgpiovar.h>

/* registers */
#define WIDR		0x00
#define WD_SYSCONFIG	0x10
#define WD_SYSSTATUS	0x14
#define WISR		0x18
#define WIER		0x1C
#define WCLR		0x24
#define WCRR		0x28
#define WLDR		0x2C
#define WTGR		0x30
#define WWPS		0x34
#define		WWPS_PEND_ALL	0x1f
#define WSPR		0x48


struct omdog_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
};

struct omdog_softc *omdog_sc;

/* 
 * to enable  the watchdog, write 0xXXXXbbbb then 0xXXXX4444 to WSPR
 * to disable the watchdog, write 0xXXXXaaaa then 0xXXXX5555 to WSPR
 */

void omdog_attach(struct device *parent, struct device *self, void *args);
void omdog_wpending(int flags);

struct cfattach	omdog_ca = {
	sizeof (struct omdog_softc), NULL, omdog_attach
};

struct cfdriver omdog_cd = {
	NULL, "omdog", DV_DULL
};

void
omdog_attach(struct device *parent, struct device *self, void *args)
{
	struct omap_attach_args *oa = args;
	struct omdog_softc *sc = (struct omdog_softc *) self;
	u_int32_t rev;

	sc->sc_iot = oa->oa_iot;
	if (bus_space_map(sc->sc_iot, oa->oa_dev->mem[0].addr,
	    oa->oa_dev->mem[0].size, 0, &sc->sc_ioh))
		panic("gptimer_attach: bus_space_map failed!");

	rev = bus_space_read_4(sc->sc_iot, sc->sc_ioh, WIDR);

	printf(" rev %d.%d\n", rev >> 4 & 0xf, rev & 0xf);
	omdog_sc = sc;
}

void
omdog_wpending(int flags)
{
	struct omdog_softc *sc = omdog_sc;
	while (bus_space_read_4(sc->sc_iot, sc->sc_ioh, WWPS) & flags)
		;
}

void omdog_reset(void); 	/* XXX */

void
omdog_reset()
{
	if (omdog_sc == NULL)
		return;

	bus_space_write_4(omdog_sc->sc_iot, omdog_sc->sc_ioh, WCRR, 0xffffff80);
	omdog_wpending(WWPS_PEND_ALL);

	/* this sequence will start the watchdog. */
	bus_space_write_4(omdog_sc->sc_iot, omdog_sc->sc_ioh, WSPR, 0xbbbb);
	omdog_wpending(WWPS_PEND_ALL);
	bus_space_write_4(omdog_sc->sc_iot, omdog_sc->sc_ioh, WSPR, 0x4444);
	omdog_wpending(WWPS_PEND_ALL);
	delay(100000);
}
