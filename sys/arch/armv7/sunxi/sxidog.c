/* $OpenBSD: sxidog.c,v 1.4 2014/11/01 07:08:43 jsg Exp $ */
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

#include <armv7/sunxi/sunxireg.h>
#include <armv7/armv7/armv7var.h>

/* XXX other way around than bus_space_subregion? */
extern bus_space_handle_t sxitimer_ioh;

/* registers */
#define WDOG_CR			0x00
#define WDOG_MR			0x04

#define WDOG_CTRL_KEY		(0x0a57 << 1)
#define WDOG_RESTART		0x01
/*
 * 0x00 0,5sec
 * 0x01 1sec
 * 0x02 2sec
 * 0x03 3sec
 * 0x04 4sec
 * 0x05 5sec
 * 0x06 6sec
 * 0x07 8sec
 * 0x08 10sec
 * 0x09 12sec
 * 0x0a 14sec
 * 0x0b 16sec
 */
#define WDOG_INTV_VALUE(x)	((x) << 3)
#define WDOG_RST_EN		(1 << 1) /* system reset */
#define WDOG_EN			(1 << 0)

struct sxidog_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
};

struct sxidog_softc *sxidog_sc = NULL;	/* for sxidog_reset() */

void sxidog_attach(struct device *, struct device *, void *);
int sxidog_callback(void *, int);
#if 0
int sxidog_intr(void *);
#endif
void sxidog_reset(void);

struct cfattach	sxidog_ca = {
	sizeof (struct sxidog_softc), NULL, sxidog_attach
};

struct cfdriver sxidog_cd = {
	NULL, "sxidog", DV_DULL
};

void
sxidog_attach(struct device *parent, struct device *self, void *args)
{
	struct armv7_attach_args *aa = args;
	struct sxidog_softc *sc = (struct sxidog_softc *)self;

	sc->sc_iot = aa->aa_iot;
	if (bus_space_subregion(sc->sc_iot, sxitimer_ioh,
	    aa->aa_dev->mem[0].addr, aa->aa_dev->mem[0].size, &sc->sc_ioh))
		panic("sxidog_attach: bus_space_subregion failed!");

#ifdef DEBUG
	printf(": ctrl %x mode %x\n", SXIREAD4(sc, WDOG_CR),
	    SXIREAD4(sc, WDOG_MR));
#endif
#if 0
	(void)intc_intr_establish(aa->aa_dev->irq[0], IPL_HIGH, /* XXX */
	    sxidog_intr, sc, sc->sc_dev.dv_xname);
#endif
	sxidog_sc = sc;

#ifndef SMALL_KERNEL
	wdog_register(sxidog_callback, sc);
#endif

	printf("\n");
}

int
sxidog_callback(void *arg, int period)
{
	struct sxidog_softc *sc = (struct sxidog_softc *)arg;

	if (period > 0x0b)
		period = 0x0b;
	else if (period < 0)
		period = 0;
	/*
	 * clearing bits in mode reg has no effect according
	 * to the user manual, so just set new timeout and enable it.
	 * XXX
	 */
	SXIWRITE4(sc, WDOG_MR, WDOG_EN | WDOG_RST_EN |
	    WDOG_INTV_VALUE(period));
	/* reset */
	SXIWRITE4(sc, WDOG_CR, WDOG_CTRL_KEY | WDOG_RESTART);

	return period;
}

#if 0
int
sxidog_intr(void *arg)
{
	struct sxidog_softc *sc = (struct sxidog_softc *)arg;

	/* XXX */
	SXIWRITE4(sc, WDOG_CR, WDOG_CTRL_KEY | WDOG_RESTART);
	return 1;
}
#endif

void
sxidog_reset(void)
{
	if (sxidog_sc == NULL)
		return;

	SXIWRITE4(sxidog_sc, WDOG_MR, WDOG_INTV_VALUE(0x00) |
	    WDOG_RST_EN | WDOG_EN);
	SXIWRITE4(sxidog_sc, WDOG_CR, WDOG_CTRL_KEY | WDOG_RESTART);
	delay(900000);
}
