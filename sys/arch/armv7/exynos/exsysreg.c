/* $OpenBSD: exsysreg.c,v 1.5 2017/03/04 18:17:24 kettenis Exp $ */
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
#include <sys/device.h>

#include <machine/intr.h>
#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/fdt.h>

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

int exsysreg_match(struct device *, void *, void *);
void exsysreg_attach(struct device *, struct device *, void *);

struct cfattach	exsysreg_ca = {
	sizeof (struct exsysreg_softc), exsysreg_match, exsysreg_attach
};

struct cfdriver exsysreg_cd = {
	NULL, "exsysreg", DV_DULL
};

int
exsysreg_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "samsung,exynos5-sysreg");
}

void
exsysreg_attach(struct device *parent, struct device *self, void *aux)
{
	struct exsysreg_softc *sc = (struct exsysreg_softc *)self;
	struct fdt_attach_args *faa = aux;

	sc->sc_iot = faa->fa_iot;

	if (bus_space_map(sc->sc_iot, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sc->sc_ioh))
		panic("%s: bus_space_map failed!", __func__);

	printf("\n");

	exsysreg_sc = sc;
}

void
exsysreg_usbhost_mode(int on)
{
	struct exsysreg_softc *sc = exsysreg_sc;
	KASSERT(sc);

	if (on)
		HSET4(sc, SYSREG_USB20PHY_CFG, SYSREG_USB20PHY_CFG_HOST_LINK_EN);
	else
		HCLR4(sc, SYSREG_USB20PHY_CFG, SYSREG_USB20PHY_CFG_HOST_LINK_EN);
}
