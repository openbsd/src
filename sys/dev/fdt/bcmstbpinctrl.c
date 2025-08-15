/*	$OpenBSD: bcmstbpinctrl.c,v 1.1 2025/08/15 13:35:49 kettenis Exp $	*/
/*
 * Copyright (c) 2025 Mark Kettenis <kettenis@openbsd.org>
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
#include <dev/ofw/ofw_gpio.h>
#include <dev/ofw/ofw_pinctrl.h>
#include <dev/ofw/fdt.h>

#define BIAS_DISABLE	0x0
#define BIAS_PULL_DOWN	0x1
#define BIAS_PULL_UP	0x2
#define BIAS_MASK	0x3
#define FUNC_MASK	0xf

#define FUNC_MAX	9

#define HREAD4(sc, reg)							\
	(bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg)))
#define HWRITE4(sc, reg, val)						\
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))

struct bcmstbpinctrl_pin {
	char name[8];
	uint8_t func_reg, func_off;
	uint8_t bias_reg, bias_off;
	const char *func[FUNC_MAX];
};

const struct bcmstbpinctrl_pin bcmstbpinctrl_c0_pins[] = {
	{ "gpio30", 3, 6, 9, 7, { NULL, NULL, NULL, "sd2" } },
	{ "gpio31", 3, 7, 9, 8, { NULL, NULL, NULL, "sd2" } },
	{ "gpio32", 4, 0, 9, 9, { NULL, NULL, NULL, "sd2" } },
	{ "gpio33", 4, 1, 9, 10, { NULL, NULL, "sd2" } },
	{ "gpio34", 4, 2, 9, 11, { NULL, NULL, NULL, "sd2" } },
	{ "gpio35", 4, 3, 9, 12, { NULL, NULL, "sd2" } },
};

const struct bcmstbpinctrl_pin bcmstbpinctrl_d0_pins[] = {
	{ "gpio30", 2, 6, 5, 12, { "sd2" } },
	{ "gpio31", 2, 7, 5, 13, { "sd2" } },
	{ "gpio32", 3, 0, 5, 14, { "sd2" } },
	{ "gpio33", 3, 1, 6, 0, { "sd2" } },
	{ "gpio34", 3, 2, 6, 1, { "sd2" } },
	{ "gpio35", 3, 3, 6, 2, { "sd2" } },
};

struct bcmstbpinctrl_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;

	const struct bcmstbpinctrl_pin *sc_pins;
	u_int			sc_npins;
};

int	bcmstbpinctrl_match(struct device *, void *, void *);
void	bcmstbpinctrl_attach(struct device *, struct device *, void *);

const struct cfattach bcmstbpinctrl_ca = {
	sizeof(struct bcmstbpinctrl_softc),
	bcmstbpinctrl_match, bcmstbpinctrl_attach
};

struct cfdriver bcmstbpinctrl_cd = {
	NULL, "bcmstbpinctrl", DV_DULL
};

int	bcmstbpinctrl_pinctrl(uint32_t, void *);

int
bcmstbpinctrl_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return (OF_is_compatible(faa->fa_node, "brcm,bcm2712-pinctrl") ||
	    OF_is_compatible(faa->fa_node, "brcm,bcm2712d0-pinctrl"));
}

void
bcmstbpinctrl_attach(struct device *parent, struct device *self, void *aux)
{
	struct bcmstbpinctrl_softc *sc = (struct bcmstbpinctrl_softc *)self;
	struct fdt_attach_args *faa = aux;

	sc->sc_iot = faa->fa_iot;
	if (bus_space_map(sc->sc_iot, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sc->sc_ioh)) {
		printf(": can't map registers\n");
		return;
	}

	printf("\n");

	if (OF_is_compatible(faa->fa_node, "brcm,bcm2712d0-pinctrl")) {
		sc->sc_pins = bcmstbpinctrl_d0_pins;
		sc->sc_npins = nitems(bcmstbpinctrl_d0_pins);
	} else {
		sc->sc_pins = bcmstbpinctrl_c0_pins;
		sc->sc_npins = nitems(bcmstbpinctrl_c0_pins);
	}

	pinctrl_register(faa->fa_node, bcmstbpinctrl_pinctrl, sc);
}

void
bcmstbpinctrl_config_func(struct bcmstbpinctrl_softc *sc, const char *name,
    const char *function, int bias)
{
	uint32_t val;
	int pin, func;

	for (pin = 0; pin < sc->sc_npins; pin++) {
		if (strcmp(name, sc->sc_pins[pin].name) == 0)
			break;
	}
	if (pin == sc->sc_npins) {
		printf("%s: %s\n", __func__, name);
		return;
	}

	for (func = 1; func <= FUNC_MAX; func++) {
		if (sc->sc_pins[pin].func[func - 1] &&
		    strcmp(function, sc->sc_pins[pin].func[func - 1]) == 0)
			break;
	}
	if (func > FUNC_MAX) {
		printf("%s: %s %s\n", __func__, name, function);
		return;
	}

	val = HREAD4(sc, sc->sc_pins[pin].func_reg * 4);
	val &= ~(FUNC_MASK << (sc->sc_pins[pin].func_off * 4));
	val |= (func << (sc->sc_pins[pin].func_off * 4));
	HWRITE4(sc, sc->sc_pins[pin].func_reg * 4, val);

	val = HREAD4(sc, sc->sc_pins[pin].bias_reg * 4);
	val &= ~(BIAS_MASK << (sc->sc_pins[pin].bias_off * 2));
	val |= (bias << (sc->sc_pins[pin].bias_off * 2));
	HWRITE4(sc, sc->sc_pins[pin].bias_reg * 4, val);
}

int
bcmstbpinctrl_pinctrl(uint32_t phandle, void *cookie)
{
	struct bcmstbpinctrl_softc *sc = cookie;
	int node, child;

	node = OF_getnodebyphandle(phandle);
	if (node == 0)
		return -1;

	for (child = OF_child(node); child; child = OF_peer(child)) {
		char function[16];
		char *pins;
		char *pin;
		int bias, len;

		/* Function */
		memset(function, 0, sizeof(function));
		OF_getprop(child, "function", function, sizeof(function));
		function[sizeof(function) - 1] = 0;

		/* Bias */
		if (OF_getproplen(child, "bias-pull-up") == 0)
			bias = BIAS_PULL_UP;
		else if (OF_getproplen(child, "bias-pull-down") == 0)
			bias = BIAS_PULL_DOWN;
		else
			bias = BIAS_DISABLE;

		len = OF_getproplen(child, "pins");
		if (len <= 0) {
			printf("%s: 0x%08x\n", __func__, phandle);
			continue;
		}

		pins = malloc(len, M_TEMP, M_WAITOK);
		OF_getprop(child, "pins", pins, len);

		pin = pins;
		while (pin < pins + len) {
			bcmstbpinctrl_config_func(sc, pin, function, bias);
			pin += strlen(pin) + 1;
		}

		free(pins, M_TEMP, len);
	}

	return 0;
}
