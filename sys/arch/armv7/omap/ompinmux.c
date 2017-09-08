/*	$OpenBSD: ompinmux.c,v 1.2 2017/09/08 05:36:51 deraadt Exp $	*/
/*
 * Copyright (c) 2016 Jonathan Gray <jsg@openbsd.org>
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
#include <armv7/armv7/armv7var.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_pinctrl.h>
#include <dev/ofw/fdt.h>

struct ompinmux_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	int			sc_node;
};

int	 ompinmux_match(struct device *, void *, void *);
void	 ompinmux_attach(struct device *, struct device *, void *);
int	 ompinmux_pinctrl(uint32_t, void *);

struct cfattach ompinmux_ca = {
	sizeof (struct ompinmux_softc), ompinmux_match, ompinmux_attach
};

struct cfdriver ompinmux_cd = {
	NULL, "ompinmux", DV_DULL
};

int
ompinmux_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "pinctrl-single");
}

void
ompinmux_attach(struct device *parent, struct device *self, void *aux)
{
	struct fdt_attach_args *faa = aux;
	struct ompinmux_softc *sc = (struct ompinmux_softc *) self;

	sc->sc_iot = faa->fa_iot;
	if (bus_space_map(sc->sc_iot, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sc->sc_ioh))
		panic("%s: bus_space_map failed!", __func__);

	sc->sc_node = faa->fa_node;
	pinctrl_register(faa->fa_node, ompinmux_pinctrl, sc);

	pinctrl_byname(faa->fa_node, "default");

	printf("\n");
}

int
ompinmux_pinctrl(uint32_t phandle, void *cookie)
{
	struct ompinmux_softc *sc = cookie;
	uint32_t *pins;
	int npins;
	int node;
	int len;
	int i, j;
	int regwidth;

	if (sc == NULL)
		return -1;

	node = OF_getnodebyphandle(phandle);
	if (node == 0)
		return -1;

	len = OF_getproplen(node, "pinctrl-single,pins");
	if (len <= 0)
		return -1;

	pins = malloc(len, M_TEMP, M_WAITOK);
	OF_getpropintarray(node, "pinctrl-single,pins", pins, len);
	npins = len / (2 * sizeof(uint32_t));

	regwidth = OF_getpropint(sc->sc_node, "pinctrl-single,register-width",
	    0);

	if (regwidth == 16) {
		uint16_t regmask = OF_getpropint(sc->sc_node,
		    "pinctrl-single,function-mask", 0xffff);
		uint32_t conf_reg;
		uint16_t conf_val;

		for (i = 0, j = 0; i < npins; i++, j += 2) {
			conf_reg = pins[2 * i + 0];

			conf_val = (bus_space_read_2(sc->sc_iot, sc->sc_ioh,
			    conf_reg) & ~regmask) | pins[2 * i + 1];
			bus_space_write_2(sc->sc_iot, sc->sc_ioh, conf_reg,
			    conf_val);
		}
	} else if (regwidth == 32) {
		uint32_t regmask = OF_getpropint(sc->sc_node,
		    "pinctrl-single,function-mask", 0xffffffff);
		uint32_t conf_reg, conf_val;

		for (i = 0, j = 0; i < npins; i++, j += 2) {
			conf_reg = pins[2 * i + 0];

			conf_val = (bus_space_read_4(sc->sc_iot, sc->sc_ioh,
			    conf_reg) & ~regmask) | pins[2 * i + 1];
			bus_space_write_4(sc->sc_iot, sc->sc_ioh, conf_reg,
			    conf_val);
		}
	}

	free(pins, M_TEMP, len);
	return 0;
}
