/*	$OpenBSD: amlpinctrl.c,v 1.1 2019/08/28 07:12:37 kettenis Exp $	*/
/*
 * Copyright (c) 2019 Mark Kettenis <kettenis@openbsd.org>
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

#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_gpio.h>
#include <dev/ofw/ofw_pinctrl.h>
#include <dev/ofw/fdt.h>

#define PERIPHS_PIN_MUX_0		0xb0
#define PERIPHS_PIN_MUX_B		0xbb
#define PREG_PAD_GPIO0_EN_N		0x10
#define PREG_PAD_GPIO3_EN_N		0x19

struct aml_gpio_bank {
	uint8_t first_pin, num_pins;
	uint8_t mux_reg;
	uint8_t gpio_reg;
};

struct aml_gpio_bank aml_g12a_gpio_banks[] = {
	{ 16, 9, PERIPHS_PIN_MUX_B - PERIPHS_PIN_MUX_0, /* GPIOH */
	  PREG_PAD_GPIO3_EN_N - PREG_PAD_GPIO0_EN_N },
	{ }
};

struct amlpinctrl_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_gpio_ioh;
	bus_space_handle_t	sc_mux_ioh;

	struct aml_gpio_bank	*sc_gpio_banks;
	struct gpio_controller	sc_gc;
};

int	amlpinctrl_match(struct device *, void *, void *);
void	amlpinctrl_attach(struct device *, struct device *, void *);

struct cfattach amlpinctrl_ca = {
	sizeof(struct amlpinctrl_softc), amlpinctrl_match, amlpinctrl_attach
};

struct cfdriver amlpinctrl_cd = {
	NULL, "amlpinctrl", DV_DULL
};

int	amlpinctrl_pinctrl(uint32_t, void *);
void	amlpinctrl_config_pin(void *, uint32_t *, int);
int	amlpinctrl_get_pin(void *, uint32_t *);
void	amlpinctrl_set_pin(void *, uint32_t *, int);

int
amlpinctrl_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;
	int node = faa->fa_node;

	return OF_is_compatible(node, "amlogic,meson-g12a-periphs-pinctrl");
}

void
amlpinctrl_attach(struct device *parent, struct device *self, void *aux)
{
	struct amlpinctrl_softc *sc = (struct amlpinctrl_softc *)self;
	struct fdt_attach_args *faa = aux;
	uint64_t addr[5], size[5];
	uint32_t *cell;
	uint32_t acells, scells;
	uint32_t reg[20];
	int node = faa->fa_node;
	int child;
	int i, len, line;

	for (child = OF_child(node); child; child = OF_peer(child)) {
		if (OF_getproplen(child, "gpio-controller") == 0)
			break;
	}
	if (child == 0) {
		printf(": no register banks\n");
		return;
	}

	acells = OF_getpropint(node, "#address-cells", faa->fa_acells);
	scells = OF_getpropint(node, "#size-cells", faa->fa_scells);
	len = OF_getproplen(child, "reg");
	line = (acells + scells) * sizeof(uint32_t);
	if (acells < 1 || acells > 2 || scells < 1 || scells > 2 ||
	    len > sizeof(reg) || (len / line) > nitems(addr)) {
		printf(": unexpected register layout\n");
		return;
	}

	OF_getpropintarray(child, "reg", reg, len);
	for (i = 0, cell = reg; i < len / line; i++) {
		addr[i] = cell[0];
		if (acells > 1)
			addr[i] = (addr[i] << 32) | cell[1];
		cell += acells;
		size[i] = cell[0];
		if (scells > 1)
			size[i] = (size[i] << 32) | cell[1];
		cell += scells;
	}

	sc->sc_iot = faa->fa_iot;
	if (bus_space_map(sc->sc_iot, addr[0], size[0], 0, &sc->sc_gpio_ioh)) {
		printf(": can't map gpio registers\n");
		return;
	}
	if (bus_space_map(sc->sc_iot, addr[3], size[3], 0, &sc->sc_mux_ioh)) {
		printf(": can't map mux registers\n");
		return;
	}

	printf("\n");

	sc->sc_gpio_banks = aml_g12a_gpio_banks;

	pinctrl_register(faa->fa_node, amlpinctrl_pinctrl, sc);

	sc->sc_gc.gc_node = child;
	sc->sc_gc.gc_cookie = sc;
	sc->sc_gc.gc_config_pin = amlpinctrl_config_pin;
	sc->sc_gc.gc_get_pin = amlpinctrl_get_pin;
	sc->sc_gc.gc_set_pin = amlpinctrl_set_pin;
	gpio_controller_register(&sc->sc_gc);
}

struct aml_gpio_bank *
amlpinctrl_lookup_bank(struct amlpinctrl_softc *sc, uint32_t pin)
{
	struct aml_gpio_bank *bank;

	for (bank = sc->sc_gpio_banks; bank->num_pins > 0; bank++) {
		if (pin >= bank->first_pin &&
		    pin < bank->first_pin + bank->num_pins)
			return bank;
	}

	return NULL;
}

int
amlpinctrl_pinctrl(uint32_t phandle, void *cookie)
{
	printf("%s: 0x%08x\n", __func__, phandle);
	return 0;
}

void
amlpinctrl_config_pin(void *cookie, uint32_t *cells, int config)
{
	struct amlpinctrl_softc *sc = cookie;
	struct aml_gpio_bank *bank;
	bus_addr_t off;
	uint32_t pin = cells[0];
	uint32_t reg;

	bank = amlpinctrl_lookup_bank(sc, pin);
	if (bank == NULL) {
		printf("%s: 0x%08x 0x%08x\n", __func__, cells[0], cells[1]);
		return;
	}

	pin = pin - bank->first_pin;

	/* mux */
	off = (bank->mux_reg + pin / 8) << 2;
	reg = bus_space_read_4(sc->sc_iot, sc->sc_mux_ioh, off);
	reg &= ~(0xf << ((pin % 8) * 4));
	bus_space_write_4(sc->sc_iot, sc->sc_mux_ioh, off, reg);

	/* gpio */
	off = bank->gpio_reg << 2;
	reg = bus_space_read_4(sc->sc_iot, sc->sc_gpio_ioh, off);
	if (config & GPIO_CONFIG_OUTPUT)
		reg &= ~(1 << pin);
	else
		reg |= (1 << pin);
	bus_space_write_4(sc->sc_iot, sc->sc_gpio_ioh, off, reg);
}

int
amlpinctrl_get_pin(void *cookie, uint32_t *cells)
{
	struct amlpinctrl_softc *sc = cookie;
	struct aml_gpio_bank *bank;
	bus_addr_t off;
	uint32_t pin = cells[0];
	uint32_t flags = cells[1];
	uint32_t reg;
	int val;

	bank = amlpinctrl_lookup_bank(sc, pin);
	if (bank == NULL) {
		printf("%s: 0x%08x 0x%08x\n", __func__, cells[0], cells[1]);
		return 0;
	}

	pin = pin - bank->first_pin;

	/* gpio */
	off = (bank->gpio_reg + 2) << 2;
	reg = bus_space_read_4(sc->sc_iot, sc->sc_gpio_ioh, off);
	val = (reg >> pin) & 1;
	if (flags & GPIO_ACTIVE_LOW)
		val = !val;

	return val;
}

void
amlpinctrl_set_pin(void *cookie, uint32_t *cells, int val)
{
	struct amlpinctrl_softc *sc = cookie;
	struct aml_gpio_bank *bank;
	bus_addr_t off;
	uint32_t pin = cells[0];
	uint32_t flags = cells[1];
	int reg;

	bank = amlpinctrl_lookup_bank(sc, pin);
	if (bank == NULL) {
		printf("%s: 0x%08x 0x%08x\n", __func__, cells[0], cells[1]);
		return;
	}

	if (flags & GPIO_ACTIVE_LOW)
		val = !val;

	pin = pin - bank->first_pin;

	/* gpio */
	off = (bank->gpio_reg + 2) << 2;
	reg = bus_space_read_4(sc->sc_iot, sc->sc_gpio_ioh, off);
	if (val)
		reg |= (1 << pin);
	else
		reg &= ~(1 << pin);
	bus_space_write_4(sc->sc_iot, sc->sc_gpio_ioh, off, reg);
}
