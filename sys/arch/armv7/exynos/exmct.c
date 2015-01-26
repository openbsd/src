/* $OpenBSD: exmct.c,v 1.1 2015/01/26 02:48:24 bmercer Exp $ */
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/evcount.h>
#include <sys/socket.h>
#include <sys/timeout.h>
#include <arm/cpufunc.h>
#include <machine/intr.h>
#include <machine/bus.h>
#include <machine/fdt.h>
#include <armv7/armv7/armv7var.h>

/* registers */
#define MCT_CTRL	0x240
#define MCT_WRITE_STAT	0x24C

/* bits and bytes */
#define MCT_CTRL_START	(1 << 8)

struct exmct_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
};

struct exmct_softc *exmct_sc;

int exmct_match(struct device *parent, void *v, void *aux);
void exmct_attach(struct device *parent, struct device *self, void *args);
void exmct_stop(void);
void exmct_reset(void);

struct cfattach	exmct_ca = {
	sizeof (struct exmct_softc), NULL, exmct_attach
};
struct cfattach	exmct_fdt_ca = {
	sizeof (struct exmct_softc), exmct_match, exmct_attach
};

struct cfdriver exmct_cd = {
	NULL, "exmct", DV_DULL
};

int
exmct_match(struct device *parent, void *v, void *aux)
{
	struct armv7_attach_args *aa = aux;

	if (fdt_node_compatible("samsung,exynos4210-mct", aa->aa_node))
		return 1;

	return 0;
}

void
exmct_attach(struct device *parent, struct device *self, void *args)
{
	struct armv7_attach_args *aa = args;
	struct exmct_softc *sc = (struct exmct_softc *) self;
	struct fdt_memory mem;
	uint32_t i, mask, reg;

	sc->sc_iot = aa->aa_iot;
	if (aa->aa_node) {
		if (fdt_get_memory_address(aa->aa_node, 0, &mem))
			panic("%s: could not extract memory data from FDT",
			    __func__);
	} else {
		mem.addr = aa->aa_dev->mem[0].addr;
		mem.size = aa->aa_dev->mem[0].size;
	}
	if (bus_space_map(sc->sc_iot, mem.addr, mem.size, 0, &sc->sc_ioh))
		panic("%s: bus_space_map failed!", __func__);

	printf("\n");

	exmct_sc = sc;

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, MCT_CTRL,
	    bus_space_read_4(sc->sc_iot, sc->sc_ioh, MCT_CTRL) | MCT_CTRL_START);

	mask = (1 << 16);

	/* Wait 10 times until written value is applied */
	for (i = 0; i < 10; i++) {
		reg = bus_space_read_4(sc->sc_iot, sc->sc_ioh, MCT_WRITE_STAT);
		if (reg & mask) {
			bus_space_write_4(sc->sc_iot, sc->sc_ioh,
			    MCT_WRITE_STAT, mask);
			return;
		}
		cpufunc_nullop();
	}

	/* NOTREACHED */

	panic("%s: Can't enable timer!", __func__);
}
