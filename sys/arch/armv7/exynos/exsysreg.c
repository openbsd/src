/* $OpenBSD: exsysreg.c,v 1.1 2015/01/26 02:48:24 bmercer Exp $ */
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
#include <sys/sysctl.h>
#include <sys/device.h>
#include <sys/evcount.h>
#include <sys/socket.h>
#include <sys/timeout.h>
#include <machine/intr.h>
#include <machine/bus.h>
#include <machine/fdt.h>
#include <armv7/armv7/armv7var.h>
#include <armv7/exynos/exsysregvar.h>

/* registers */
#define SYSREG_USB20PHY_CFG			0x230

/* bits and bytes */
#define SYSREG_USB20PHY_CFG_HOST_LINK_EN	(1 << 0)

#define HREAD4(sc, reg)							\
	(bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg)))
#define HWRITE4(sc, reg, val)						\
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))
#define HSET4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) | (bits))
#define HCLR4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) & ~(bits))

struct exsysreg_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
};

struct exsysreg_softc *exsysreg_sc;

int exsysreg_match(struct device *parent, void *v, void *aux);
void exsysreg_attach(struct device *parent, struct device *self, void *args);

struct cfattach	exsysreg_ca = {
	sizeof (struct exsysreg_softc), NULL, exsysreg_attach
};
struct cfattach	exsysreg_fdt_ca = {
	sizeof (struct exsysreg_softc), exsysreg_match, exsysreg_attach
};

struct cfdriver exsysreg_cd = {
	NULL, "exsysreg", DV_DULL
};

int
exsysreg_match(struct device *parent, void *v, void *aux)
{
	struct armv7_attach_args *aa = aux;

	if (fdt_node_compatible("samsung,exynos5-sysreg", aa->aa_node))
		return 1;

	return 0;
}

void
exsysreg_attach(struct device *parent, struct device *self, void *args)
{
	struct armv7_attach_args *aa = args;
	struct exsysreg_softc *sc = (struct exsysreg_softc *) self;
	struct fdt_memory mem;

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

	exsysreg_sc = sc;
}

void
exsysreg_usbhost_mode(int on)
{
	struct exsysreg_softc *sc = exsysreg_sc;

	if (on)
		HSET4(sc, SYSREG_USB20PHY_CFG, SYSREG_USB20PHY_CFG_HOST_LINK_EN);
	else
		HCLR4(sc, SYSREG_USB20PHY_CFG, SYSREG_USB20PHY_CFG_HOST_LINK_EN);
}
