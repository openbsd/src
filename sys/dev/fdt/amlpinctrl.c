/*	$OpenBSD: amlpinctrl.c,v 1.5 2019/10/06 16:17:06 kettenis Exp $	*/
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
#include <sys/malloc.h>

#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_gpio.h>
#include <dev/ofw/ofw_pinctrl.h>
#include <dev/ofw/fdt.h>

#define BIAS_DISABLE	0x00
#define BIAS_PULL_UP	0x01
#define BIAS_PULL_DOWN	0x02

#define GPIOZ_0		0
#define GPIOZ_1		1
#define GPIOZ_7		7
#define GPIOZ_8		8
#define GPIOZ_14	14
#define GPIOZ_15	15
#define GPIOH_0		16
#define GPIOH_1		17
#define GPIOH_2		18
#define GPIOH_3		19
#define GPIOH_6		22
#define GPIOH_7		23
#define BOOT_0		25
#define BOOT_1		26
#define BOOT_2		27
#define BOOT_3		28
#define BOOT_4		29
#define BOOT_5		30
#define BOOT_6		31
#define BOOT_7		32
#define BOOT_8		33
#define BOOT_10		35
#define BOOT_13		38
#define GPIOC_0		41
#define GPIOC_1		42
#define GPIOC_2		43
#define GPIOC_3		44
#define GPIOC_4		45
#define GPIOC_5		46
#define GPIOC_6		47
#define GPIOA_0		49
#define GPIOA_14	63
#define GPIOA_15	64
#define GPIOX_0		65
#define GPIOX_10	75
#define GPIOX_11	76
#define GPIOX_17	82
#define GPIOX_18	83

#define PERIPHS_PIN_MUX_0		0xb0
#define PERIPHS_PIN_MUX_3		0xb3
#define PERIPHS_PIN_MUX_6		0xb6
#define PERIPHS_PIN_MUX_9		0xb9
#define PERIPHS_PIN_MUX_B		0xbb
#define PERIPHS_PIN_MUX_D		0xbd
#define PREG_PAD_GPIO0_EN_N		0x10
#define PREG_PAD_GPIO1_EN_N		0x13
#define PREG_PAD_GPIO2_EN_N		0x16
#define PREG_PAD_GPIO3_EN_N		0x19
#define PREG_PAD_GPIO4_EN_N		0x1c
#define PREG_PAD_GPIO5_EN_N		0x20
#define PAD_PULL_UP_EN_0		0x48
#define PAD_PULL_UP_EN_1		0x49
#define PAD_PULL_UP_EN_2		0x4a
#define PAD_PULL_UP_EN_3		0x4b
#define PAD_PULL_UP_EN_4		0x4c
#define PAD_PULL_UP_EN_5		0x4d
#define PAD_PULL_UP_0			0x3a
#define PAD_PULL_UP_1			0x3b
#define PAD_PULL_UP_2			0x3c
#define PAD_PULL_UP_3			0x3d
#define PAD_PULL_UP_4			0x3e
#define PAD_PULL_UP_5			0x3f
#define PAD_DS_0A			0xd0
#define PAD_DS_1A			0xd1
#define PAD_DS_2A			0xd2
#define PAD_DS_3A			0xd4
#define PAD_DS_4A			0xd5
#define PAD_DS_5A			0xd6

struct aml_gpio_bank {
	uint8_t first_pin, num_pins;
	uint8_t mux_reg;
	uint8_t gpio_reg;
	uint8_t pull_reg;
	uint8_t pull_en_reg;
	uint8_t ds_reg;
};

struct aml_pin_group {
	const char *name;
	uint8_t	pin;
	uint8_t func;
	const char *function;
};

struct aml_gpio_bank aml_g12a_gpio_banks[] = {
	/* BOOT */
	{ BOOT_0, 16, PERIPHS_PIN_MUX_0 - PERIPHS_PIN_MUX_0,
	  PREG_PAD_GPIO0_EN_N - PREG_PAD_GPIO0_EN_N,
	  PAD_PULL_UP_0 - PAD_PULL_UP_0,
	  PAD_PULL_UP_EN_0 - PAD_PULL_UP_EN_0, PAD_DS_0A - PAD_DS_0A },

	/* GPIOC */
	{ GPIOC_0, 8, PERIPHS_PIN_MUX_9 - PERIPHS_PIN_MUX_0,
	  PREG_PAD_GPIO1_EN_N - PREG_PAD_GPIO0_EN_N,
	  PAD_PULL_UP_1 - PAD_PULL_UP_0,
	  PAD_PULL_UP_EN_1 - PAD_PULL_UP_EN_0, PAD_DS_1A - PAD_DS_0A },

	/* GPIOX */
	{ GPIOX_0, 20, PERIPHS_PIN_MUX_3 - PERIPHS_PIN_MUX_0,
	  PREG_PAD_GPIO2_EN_N - PREG_PAD_GPIO0_EN_N,
	  PAD_PULL_UP_2 - PAD_PULL_UP_0,
	  PAD_PULL_UP_EN_2 - PAD_PULL_UP_EN_0, PAD_DS_2A - PAD_DS_0A },

	/* GPIOH */
	{ GPIOH_0, 9, PERIPHS_PIN_MUX_B - PERIPHS_PIN_MUX_0,
	  PREG_PAD_GPIO3_EN_N - PREG_PAD_GPIO0_EN_N,
	  PAD_PULL_UP_3 - PAD_PULL_UP_0,
	  PAD_PULL_UP_EN_3 - PAD_PULL_UP_EN_0, PAD_DS_3A - PAD_DS_0A },

	/* GPIOZ */
	{ GPIOZ_0, 16, PERIPHS_PIN_MUX_6 - PERIPHS_PIN_MUX_0,
	  PREG_PAD_GPIO4_EN_N - PREG_PAD_GPIO0_EN_N,
	  PAD_PULL_UP_4 - PAD_PULL_UP_0,
	  PAD_PULL_UP_EN_4 - PAD_PULL_UP_EN_0, PAD_DS_4A - PAD_DS_0A },

	/* GPIOA */
	{ GPIOA_0, 16, PERIPHS_PIN_MUX_D - PERIPHS_PIN_MUX_0,
	  PREG_PAD_GPIO5_EN_N - PREG_PAD_GPIO0_EN_N,
	  PAD_PULL_UP_5 - PAD_PULL_UP_0,
	  PAD_PULL_UP_EN_5 - PAD_PULL_UP_EN_0, PAD_DS_5A - PAD_DS_0A },

	{ }
};

struct aml_pin_group aml_g12a_pin_groups[] = {
	/* GPIOZ */
	{ "i2c0_sda_z0", GPIOZ_0, 4, "i2c0" },
	{ "i2c0_sck_z1", GPIOZ_1, 4, "i2c0" },
	{ "i2c0_sda_z7", GPIOZ_7, 7, "i2c0" },
	{ "i2c0_sck_z8", GPIOZ_8, 7, "i2c0" },
	{ "i2c2_sda_z", GPIOZ_14, 3, "i2c2" },
	{ "i2c2_sck_z", GPIOZ_15, 3, "i2c2" },

	/* GPIOA */
	{ "i2c3_sda_a", GPIOA_14, 2, "i2c3" },
	{ "i2c3_sck_a", GPIOA_15, 2, "i2c3" },

	/* BOOT */
	{ "emmc_nand_d0", BOOT_0, 1, "emmc" },
	{ "emmc_nand_d1", BOOT_1, 1, "emmc" },
	{ "emmc_nand_d2", BOOT_2, 1, "emmc" },
	{ "emmc_nand_d3", BOOT_3, 1, "emmc" },
	{ "emmc_nand_d4", BOOT_4, 1, "emmc" },
	{ "emmc_nand_d5", BOOT_5, 1, "emmc" },
	{ "emmc_nand_d6", BOOT_6, 1, "emmc" },
	{ "emmc_nand_d7", BOOT_7, 1, "emmc" },
	{ "BOOT_8", BOOT_8, 0, "gpio_periphs" },
	{ "emmc_clk", BOOT_8, 1, "emmc" },
	{ "emmc_cmd", BOOT_10, 1, "emmc" },
	{ "emmc_nand_ds", BOOT_13, 1, "emmc" },

	/* GPIOC */
	{ "sdcard_d0_c", GPIOC_0, 1, "sdcard" },
	{ "sdcard_d1_c", GPIOC_1, 1, "sdcard" },
	{ "sdcard_d2_c", GPIOC_2, 1, "sdcard" },
	{ "sdcard_d3_c", GPIOC_3, 1, "sdcard" },
	{ "GPIOC_4", GPIOC_4, 0, "gpio_periphs" },
	{ "sdcard_clk_c", GPIOC_4, 1, "sdcard" },
	{ "sdcard_cmd_c", GPIOC_5, 1, "sdcard" },
	{ "i2c0_sda_c", GPIOC_5, 3, "i2c0" },
	{ "i2c0_sck_c", GPIOC_6, 3, "i2c0" },

	/* GPIOX */
	{ "i2c1_sda_x", GPIOX_10, 5, "i2c1" },
	{ "i2c1_sck_x", GPIOX_11, 5, "i2c1" },
	{ "i2c2_sda_x", GPIOX_17, 1, "i2c2" },
	{ "i2c2_sck_x", GPIOX_18, 1, "i2c2" },

	/* GPIOH */
	{ "i2c3_sda_h", GPIOH_0, 2, "i2c3" },
	{ "i2c3_sck_h", GPIOH_1, 2, "i2c3" },
	{ "i2c1_sda_h2", GPIOH_2, 2, "i2c1" },
	{ "i2c1_sck_h3", GPIOH_3, 2, "i2c1" },
	{ "i2c1_sda_h6", GPIOH_6, 4, "i2c1" },
	{ "i2c1_sck_h7", GPIOH_7, 4, "i2c1" },

	{ }
};

struct amlpinctrl_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_gpio_ioh;
	bus_space_handle_t	sc_pull_ioh;
	bus_space_handle_t	sc_pull_en_ioh;
	bus_space_handle_t	sc_mux_ioh;
	bus_space_handle_t	sc_ds_ioh;

	struct aml_gpio_bank	*sc_gpio_banks;
	struct aml_pin_group	*sc_pin_groups;

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
	if (bus_space_map(sc->sc_iot, addr[1], size[1], 0, &sc->sc_pull_ioh)) {
		printf(": can't map pull registers\n");
		return;
	}
	if (bus_space_map(sc->sc_iot, addr[2], size[2], 0, &sc->sc_pull_en_ioh)) {
		printf(": can't map pull-enable registers\n");
		return;
	}
	if (bus_space_map(sc->sc_iot, addr[3], size[3], 0, &sc->sc_mux_ioh)) {
		printf(": can't map mux registers\n");
		return;
	}
	if (bus_space_map(sc->sc_iot, addr[4], size[4], 0, &sc->sc_ds_ioh)) {
		printf(": can't map ds registers\n");
		return;
	}

	printf("\n");

	sc->sc_gpio_banks = aml_g12a_gpio_banks;
	sc->sc_pin_groups = aml_g12a_pin_groups;

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

struct aml_pin_group *
amlpinctrl_lookup_group(struct amlpinctrl_softc *sc, const char *name)
{
	struct aml_pin_group *group;

	for (group = sc->sc_pin_groups; group->name; group++) {
		if (strcmp(name, group->name) == 0)
			return group;
	}

	return NULL;
}

void
amlpinctrl_config_func(struct amlpinctrl_softc *sc, const char *name,
    const char *function, int bias, int ds)
{
	struct aml_pin_group *group;
	struct aml_gpio_bank *bank;
	bus_addr_t off;
	uint32_t pin;
	uint32_t reg;

	group = amlpinctrl_lookup_group(sc, name);
	if (group == NULL) {
		printf("%s: %s\n", __func__, name);
		return;
	}
	if (strcmp(function, group->function) != 0) {
		printf("%s: mismatched function %s\n", __func__, function);
		return;
	}

	bank = amlpinctrl_lookup_bank(sc, group->pin);
	KASSERT(bank);

	pin = group->pin - bank->first_pin;

	/* mux */
	off = (bank->mux_reg + pin / 8) << 2;
	reg = bus_space_read_4(sc->sc_iot, sc->sc_mux_ioh, off);
	reg &= ~(0xf << ((pin % 8) * 4));
	reg |= (group->func << ((pin % 8) * 4));
	bus_space_write_4(sc->sc_iot, sc->sc_mux_ioh, off, reg);
	
	/* pull */
	off = bank->pull_reg << 2;
	reg = bus_space_read_4(sc->sc_iot, sc->sc_pull_ioh, off);
	if (bias == BIAS_PULL_UP)
		reg |= (1 << pin);
	else
		reg &= ~(1 << pin);
	bus_space_write_4(sc->sc_iot, sc->sc_pull_ioh, off, reg);

	/* pull-enable */
	off = bank->pull_en_reg << 2;
	reg = bus_space_read_4(sc->sc_iot, sc->sc_pull_en_ioh, off);
	if (bias != BIAS_DISABLE)
		reg |= (1 << pin);
	else
		reg &= ~(1 << pin);
	bus_space_write_4(sc->sc_iot, sc->sc_pull_en_ioh, off, reg);

	if (ds < 0)
		return;
	else if (ds <= 500)
		ds = 0;
	else if (ds <= 2500)
		ds = 1;
	else if (ds <= 3000)
		ds = 2;
	else if (ds <= 4000)
		ds = 3;
	else {
		printf("%s: invalid drive-strength %d\n", __func__, ds);
		ds = 3;
	}

	/* ds */
	off = (bank->ds_reg + pin / 16) << 2;
	reg = bus_space_read_4(sc->sc_iot, sc->sc_ds_ioh, off);
	reg &= ~(0x3 << ((pin % 16) * 2));
	reg |= (ds << ((pin % 16) * 2));
	bus_space_write_4(sc->sc_iot, sc->sc_ds_ioh, off, reg);
}

int
amlpinctrl_pinctrl(uint32_t phandle, void *cookie)
{
	struct amlpinctrl_softc *sc = cookie;
	int node, child;

	node = OF_getnodebyphandle(phandle);
	if (node == 0)
		return -1;

	for (child = OF_child(node); child; child = OF_peer(child)) {
		char function[16];
		char *groups;
		char *group;
		int bias, ds;
		int len;

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

		/* Drive-strength */
		ds = OF_getpropint(child, "drive-strength-microamp", -1);

		len = OF_getproplen(child, "groups");
		if (len <= 0) {
			printf("%s: 0x%08x\n", __func__, phandle);
			continue;
		}

		groups = malloc(len, M_TEMP, M_WAITOK);
		OF_getprop(child, "groups", groups, len);

		group = groups;
		while (group < groups + len) {
			amlpinctrl_config_func(sc, group, function, bias, ds);
			group += strlen(group) + 1;
		}

		free(groups, M_TEMP, len);
	}

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
	off = (bank->gpio_reg + 1) << 2;
	reg = bus_space_read_4(sc->sc_iot, sc->sc_gpio_ioh, off);
	if (val)
		reg |= (1 << pin);
	else
		reg &= ~(1 << pin);
	bus_space_write_4(sc->sc_iot, sc->sc_gpio_ioh, off, reg);
}
