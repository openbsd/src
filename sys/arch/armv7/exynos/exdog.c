/* $OpenBSD: exdog.c,v 1.2 2015/05/27 00:06:14 jsg Exp $ */
/*
 * Copyright (c) 2012-2013 Patrick Wildt <patrick@blueri.se>
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

#include "fdt.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/evcount.h>
#include <sys/socket.h>
#include <sys/timeout.h>
#include <machine/bus.h>
#if NFDT > 0
#include <machine/fdt.h>
#endif
#include <machine/intr.h>
#include <armv7/armv7/armv7var.h>

/* registers */
#define WTCON		0x00
#define WTDAT		0x04
#define WTCNT		0x08
#define WTCLRINT	0x0C

/* bits and bytes */
#define WTCON_RESET		(1 << 0)
#define WTCON_INT		(1 << 2)
#define WTCON_CLKSEL_16		(0x0 << 3)
#define WTCON_CLKSEL_32		(0x1 << 3)
#define WTCON_CLKSEL_64		(0x2 << 3)
#define WTCON_CLKSEL_128	(0x3 << 3)
#define WTCON_EN		(1 << 5)
#define WTCON_PRESCALER(x)	(((x) & 0xff) << 8)

struct exdog_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
};

struct exdog_softc *exdog_sc;

int exdog_match(struct device *parent, void *v, void *aux);
void exdog_attach(struct device *parent, struct device *self, void *args);
void exdog_stop(void);
void exdog_reset(void);

struct cfattach	exdog_ca = {
	sizeof (struct exdog_softc), NULL, exdog_attach
};
struct cfattach	exdog_fdt_ca = {
	sizeof (struct exdog_softc), exdog_match, exdog_attach
};

struct cfdriver exdog_cd = {
	NULL, "exdog", DV_DULL
};

int
exdog_match(struct device *parent, void *v, void *aux)
{
#if NFDT > 0
	struct armv7_attach_args *aa = aux;

	if (fdt_node_compatible("samsung,exynos5250-wdt", aa->aa_node))
		return 1;
#endif

	return 0;
}

void
exdog_attach(struct device *parent, struct device *self, void *args)
{
	struct armv7_attach_args *aa = args;
	struct exdog_softc *sc = (struct exdog_softc *) self;
	struct armv7mem mem;

	sc->sc_iot = aa->aa_iot;
#if NFDT > 0
	if (aa->aa_node) {
		struct fdt_memory fdtmem;
		if (fdt_get_memory_address(aa->aa_node, 0, &fdtmem))
			panic("%s: could not extract memory data from FDT",
			    __func__);
		mem.addr = fdtmem.addr;
		mem.size = fdtmem.size;
	} else
#endif
	{
		mem.addr = aa->aa_dev->mem[0].addr;
		mem.size = aa->aa_dev->mem[0].size;
	}
	if (bus_space_map(sc->sc_iot, mem.addr, mem.size, 0, &sc->sc_ioh))
		panic("%s: bus_space_map failed!", __func__);

	printf("\n");
	exdog_sc = sc;
}

void
exdog_stop()
{
	uint32_t wtcon;

	if (exdog_sc == NULL)
		return;

	wtcon = bus_space_read_4(exdog_sc->sc_iot, exdog_sc->sc_ioh, WTCON);

	wtcon &= ~(WTCON_EN | WTCON_INT | WTCON_RESET);

	bus_space_write_4(exdog_sc->sc_iot, exdog_sc->sc_ioh, WTCON, wtcon);
}

void
exdog_reset()
{
	uint32_t wtcon;

	if (exdog_sc == NULL)
		return;

	/* disable watchdog */
	exdog_stop();

	wtcon = bus_space_read_4(exdog_sc->sc_iot, exdog_sc->sc_ioh, WTCON);

	wtcon |= WTCON_EN | WTCON_CLKSEL_128;
	wtcon &= ~WTCON_INT;
	wtcon |= WTCON_RESET;
	wtcon |= WTCON_PRESCALER(0xff);

	/* set timeout to 1 */
	bus_space_write_4(exdog_sc->sc_iot, exdog_sc->sc_ioh, WTDAT, 1);
	bus_space_write_4(exdog_sc->sc_iot, exdog_sc->sc_ioh, WTCNT, 1);

	/* kick off the watchdog */
	bus_space_write_4(exdog_sc->sc_iot, exdog_sc->sc_ioh, WTCON, wtcon);

	delay(100000);
}
