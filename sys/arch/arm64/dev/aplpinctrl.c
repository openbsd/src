/*	$OpenBSD: aplpinctrl.c,v 1.1 2021/08/31 15:20:06 kettenis Exp $	*/
/*
 * Copyright (c) 2021 Mark Kettenis <kettenis@openbsd.org>
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

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_pinctrl.h>
#include <dev/ofw/fdt.h>

#define APPLE_PIN(pinmux) ((pinmux) & 0xffff)
#define APPLE_FUNC(pinmux) ((pinmux) >> 16)

#define GPIO_PIN(reg)		((reg) * 4)
#define  GPIO_PIN_FUNC_MASK	(1 << 5)
#define  GPIO_PIN_FUNC_SHIFT	5

#define HREAD4(sc, reg)						\
	(bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg)))
#define HWRITE4(sc, reg, val)						\
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))

struct aplpinctrl_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
};

int	aplpinctrl_match(struct device *, void *, void *);
void	aplpinctrl_attach(struct device *, struct device *, void *);

struct cfattach	aplpinctrl_ca = {
	sizeof (struct aplpinctrl_softc), aplpinctrl_match, aplpinctrl_attach
};

struct cfdriver aplpinctrl_cd = {
	NULL, "aplpinctrl", DV_DULL
};

int	aplpinctrl_pinctrl(uint32_t, void *);

int
aplpinctrl_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "apple,pinctrl");
}

void
aplpinctrl_attach(struct device *parent, struct device *self, void *aux)
{
	struct aplpinctrl_softc *sc = (struct aplpinctrl_softc *)self;
	struct fdt_attach_args *faa = aux;

	if (faa->fa_nreg < 1) {
		printf(": no registers\n");
		return;
	}

	sc->sc_iot = faa->fa_iot;
	if (bus_space_map(sc->sc_iot, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sc->sc_ioh)) {
		printf(": can't map registers\n");
		return;
	}

	pinctrl_register(faa->fa_node, aplpinctrl_pinctrl, sc);

	printf("\n");
}

int
aplpinctrl_pinctrl(uint32_t phandle, void *cookie)
{
	struct aplpinctrl_softc *sc = cookie;
	uint32_t *pinmux;
	int node, len, i;
	uint16_t pin, func;
	uint32_t reg;

	node = OF_getnodebyphandle(phandle);
	if (node == 0)
		return -1;

	len = OF_getproplen(node, "pinmux");
	if (len <= 0)
		return -1;

	pinmux = malloc(len, M_TEMP, M_WAITOK);
	OF_getpropintarray(node, "pinmux", pinmux, len);

	for (i = 0; i < len / sizeof(uint32_t); i++) {
		pin = APPLE_PIN(pinmux[i]);
		func = APPLE_FUNC(pinmux[i]);
		reg = HREAD4(sc, GPIO_PIN(pin));
		reg &= ~GPIO_PIN_FUNC_MASK;
		reg |= (func << GPIO_PIN_FUNC_SHIFT) & GPIO_PIN_FUNC_MASK;
		HWRITE4(sc, GPIO_PIN(pin), reg);
	}

	free(pinmux, M_TEMP, len);
	return 0;
}
