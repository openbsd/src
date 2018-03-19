/* $OpenBSD: mvpinctrl.c,v 1.2 2018/03/19 13:49:06 patrick Exp $ */
/*
 * Copyright (c) 2013,2016 Patrick Wildt <patrick@blueri.se>
 * Copyright (c) 2016 Mark Kettenis <kettenis@openbsd.org>
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

#include <machine/intr.h>
#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_pinctrl.h>
#include <dev/ofw/fdt.h>

#define HREAD4(sc, reg)							\
	(bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg)))
#define HWRITE4(sc, reg, val)						\
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))
#define HSET4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) | (bits))
#define HCLR4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) & ~(bits))

struct mvpinctrl_pin {
	char *pin;
	char *function;
	int value;
	int pid;
};

struct mvpinctrl_softc {
	struct device		 sc_dev;
	bus_space_tag_t		 sc_iot;
	bus_space_handle_t	 sc_ioh;
	struct mvpinctrl_pin	*sc_pins;
	int			 sc_npins;
};

int	mvpinctrl_match(struct device *, void *, void *);
void	mvpinctrl_attach(struct device *, struct device *, void *);
int	mvpinctrl_pinctrl(uint32_t, void *);

struct cfattach mvpinctrl_ca = {
	sizeof (struct mvpinctrl_softc), mvpinctrl_match, mvpinctrl_attach
};

struct cfdriver mvpinctrl_cd = {
	NULL, "mvpinctrl", DV_DULL
};

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#define MPP(id, func, val) { STR(mpp ## id), func, val, id }

struct mvpinctrl_pin mv88f6828_pins[] = {
	MPP(4,		"ge",		1),
	MPP(5,		"ge",		1),
	MPP(6,		"ge0",		1),
	MPP(7,		"ge0",		1),
	MPP(8,		"ge0",		1),
	MPP(9,		"ge0",		1),
	MPP(10,		"ge0",		1),
	MPP(11,		"ge0",		1),
	MPP(12,		"ge0",		1),
	MPP(13,		"ge0",		1),
	MPP(14,		"ge0",		1),
	MPP(15,		"ge0",		1),
	MPP(16,		"ge0",		1),
	MPP(17,		"ge0",		1),
	MPP(20,		"gpio",		0),
	MPP(21,		"sd0",		4),
	MPP(23,		"gpio",		0),
	MPP(28,		"sd0",		4),
	MPP(37,		"sd0",		4),
	MPP(38,		"sd0",		4),
	MPP(39,		"sd0",		4),
	MPP(40,		"sd0",		4),
	MPP(41,		"gpio",		0),
	MPP(45,		"ref",		1),
	MPP(46,		"ref",		1),
};

int
mvpinctrl_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "marvell,mv88f6828-pinctrl");
}

void
mvpinctrl_attach(struct device *parent, struct device *self, void *aux)
{
	struct fdt_attach_args *faa = aux;
	struct mvpinctrl_softc *sc = (struct mvpinctrl_softc *) self;

	sc->sc_iot = faa->fa_iot;
	if (bus_space_map(sc->sc_iot, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sc->sc_ioh))
		panic("mvpinctrl_attach: bus_space_map failed!");

	sc->sc_pins = mv88f6828_pins;
	sc->sc_npins = sizeof(mv88f6828_pins) / sizeof(struct mvpinctrl_pin);
	pinctrl_register(faa->fa_node, mvpinctrl_pinctrl, sc);

	printf("\n");
}

int
mvpinctrl_pinctrl(uint32_t phandle, void *cookie)
{
	struct mvpinctrl_softc *sc = cookie;
	char *pins, *pin, *func;
	int i, flen, plen, node;

	node = OF_getnodebyphandle(phandle);
	if (node == 0)
		return -1;

	flen = OF_getproplen(node, "marvell,function");
	if (flen <= 0)
		return -1;

	func = malloc(flen, M_TEMP, M_WAITOK);
	OF_getprop(node, "marvell,function", func, flen);

	plen = OF_getproplen(node, "marvell,pins");
	if (plen <= 0)
		return -1;

	pin = pins = malloc(plen, M_TEMP, M_WAITOK);
	OF_getprop(node, "marvell,pins", pins, plen);

	while (plen > 0) {
		for (i = 0; i < sc->sc_npins; i++) {
			uint32_t off, shift;

			if (strcmp(sc->sc_pins[i].pin, pin))
				continue;
			if (strcmp(sc->sc_pins[i].function, func))
				continue;

			off = (sc->sc_pins[i].pid / 8) * sizeof(uint32_t);
			shift = (sc->sc_pins[i].pid % 8) * 4;

			HWRITE4(sc, off, (HREAD4(sc, off) & ~(0xf << shift)) |
			    (sc->sc_pins[i].value << shift));
			break;
		}

		if (i == sc->sc_npins)
			printf("%s: unsupported pin %s function %s\n",
			    sc->sc_dev.dv_xname, pin, func);

		plen -= strlen(pin) + 1;
		pin += strlen(pin) + 1;
	}

	free(func, M_TEMP, flen);
	free(pins, M_TEMP, plen);
	return 0;
}
