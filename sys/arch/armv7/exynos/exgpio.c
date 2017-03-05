/* $OpenBSD: exgpio.c,v 1.5 2017/03/05 20:53:19 kettenis Exp $ */
/*
 * Copyright (c) 2007,2009 Dale Rahn <drahn@openbsd.org>
 * Copyright (c) 2012-2013 Patrick Wildt <patrick@blueri.se>
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

#include <machine/bus.h>
#include <machine/fdt.h>
#include <machine/intr.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_pinctrl.h>
#include <dev/ofw/fdt.h>

#define GPXCON(x)	((x) + 0x0000)
#define GPXDAT(x)	((x) + 0x0004)
#define GPXPUD(x)	((x) + 0x0008)
#define GPXDRV(x)	((x) + 0x000c)

#define HREAD4(sc, reg)							\
	(bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg)))
#define HWRITE4(sc, reg, val)						\
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))

struct exgpio_bank {
	const char name[8];
	bus_addr_t addr;
};

struct exgpio_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;

	struct exgpio_bank	*sc_banks;
	int			sc_nbanks;
};

int exgpio_match(struct device *, void *, void *);
void exgpio_attach(struct device *, struct device *, void *);

struct cfattach	exgpio_ca = {
	sizeof (struct exgpio_softc), exgpio_match, exgpio_attach
};

struct cfdriver exgpio_cd = {
	NULL, "exgpio", DV_DULL
};

/* Exynos 5420/5422 */
struct exgpio_bank exynos5420_banks[] = {
	/* Controller 0 */
	{ "gpy7", 0x0000 },
	{ "gpx0", 0x0c00 },
	{ "gpx1", 0x0c20 },
	{ "gpx2", 0x0c40 },
	{ "gpx3", 0x0c60 },

	/* Controller 1 */
	{ "gpc0", 0x0000 },
	{ "gpc1", 0x0020 },
	{ "gpc2", 0x0040 },
	{ "gpc3", 0x0060 },
	{ "gpc4", 0x0080 },
	{ "gpd1", 0x00a0 },
	{ "gpy0", 0x00c0 },
	{ "gpy1", 0x00e0 },
	{ "gpy2", 0x0100 },
	{ "gpy3", 0x0120 },
	{ "gpy4", 0x0140 },
	{ "gpy5", 0x0160 },
	{ "gpy6", 0x0180 },

	/* Controller 2 */
	{ "gpe0", 0x0000 },
	{ "gpe1", 0x0020 },
	{ "gpf0", 0x0040 },
	{ "gpf1", 0x0060 },
	{ "gpg0", 0x0080 },
	{ "gpg1", 0x00a0 },
	{ "gpg2", 0x00c0 },
	{ "gpj4", 0x00e0 },

	/* Controller 3 */
	{ "gpa0", 0x0000 },
	{ "gpa1", 0x0020 },
	{ "gpa2", 0x0040 },
	{ "gpb0", 0x0060 },
	{ "gpb1", 0x0080 },
	{ "gpb2", 0x00a0 },
	{ "gpb3", 0x00c0 },
	{ "gpb4", 0x00e0 },
	{ "gph0", 0x0100 },

	/* Controller 4 */
	{ "gpz", 0x0000 },
};

int exgpio_pinctrl(uint32_t, void *);

int
exgpio_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "samsung,exynos5420-pinctrl");
}

void
exgpio_attach(struct device *parent, struct device *self, void *aux)
{
	struct exgpio_softc *sc = (struct exgpio_softc *)self;
	struct fdt_attach_args *faa = aux;

	sc->sc_iot = faa->fa_iot;

	if (bus_space_map(sc->sc_iot, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sc->sc_ioh))
		panic("%s: bus_space_map failed!", __func__);

	if (OF_is_compatible(faa->fa_node, "samsung,exynos5420-pinctrl")) {
		sc->sc_banks = exynos5420_banks;
		sc->sc_nbanks = nitems(exynos5420_banks);
	}

	pinctrl_register(faa->fa_node, exgpio_pinctrl, sc);
	printf("\n");
}

int
exgpio_pinctrl(uint32_t phandle, void *cookie)
{
	struct exgpio_softc *sc = cookie;
	char *pins;
	char *bank, *next;
	uint32_t func, val, pud, drv;
	uint32_t reg;
	int node;
	int len;
	int pin;
	int i;

	node = OF_getnodebyphandle(phandle);
	if (node == 0)
		return -1;

	len = OF_getproplen(node, "samsung,pins");
	if (len <= 0)
		return -1;

	pins = malloc(len, M_TEMP, M_WAITOK);
	OF_getprop(node, "samsung,pins", pins, len);

	func = OF_getpropint(node, "samsung,pin-function", 0);
	val = OF_getpropint(node, "samsung,pin-val", 0);
	pud = OF_getpropint(node, "samsung,pin-pud", 1);
	drv = OF_getpropint(node, "samsung,pin-drv", 0);

	bank = pins;
	while (bank < pins + len) {
		next = strchr(bank, '-');
		if (next == NULL)
			return -1;
		*next++ = 0;
		pin = *next++ - '0';
		if (pin < 0 || pin > 7)
			return -1;
		next++;

		for (i = 0; i < sc->sc_nbanks; i++) {
			if (strcmp(bank, sc->sc_banks[i].name) != 0)
				break;

			reg = HREAD4(sc, GPXCON(sc->sc_banks[i].addr));
			reg &= ~(0xf << (pin * 4));
			reg |= (func << (pin * 4));
			HWRITE4(sc, GPXCON(sc->sc_banks[i].addr), reg);

			reg = HREAD4(sc, GPXDAT(sc->sc_banks[i].addr));
			if (val)
				reg |= (1 << pin);
			else
				reg &= ~(1 << pin);
			HWRITE4(sc, GPXDAT(sc->sc_banks[i].addr), reg);

			reg = HREAD4(sc, GPXPUD(sc->sc_banks[i].addr));
			reg &= ~(0x3 << (pin * 2));
			reg |= (pud << (pin * 2));
			HWRITE4(sc, GPXPUD(sc->sc_banks[i].addr), reg);

			reg = HREAD4(sc, GPXDRV(sc->sc_banks[i].addr));
			reg &= ~(0x3 << (pin * 2));
			reg |= (drv << (pin * 2));
			HWRITE4(sc, GPXDRV(sc->sc_banks[i].addr), reg);
			break;
		}
		if (i == sc->sc_nbanks)
			return -1;

		bank = next;
	}

	free(pins, M_TEMP, len);
	return 0;
}
