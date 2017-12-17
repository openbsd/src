/*	$OpenBSD: axppmic.c,v 1.1 2017/12/17 18:25:25 kettenis Exp $	*/
/*
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

#include <dev/fdt/rsbvar.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_regulator.h>
#include <dev/ofw/fdt.h>

#define AXP806_REG_ADDR_EXT			0xff
#define  AXP806_REG_ADDR_EXT_MASTER_MODE	(0 << 4)
#define  AXP806_REG_ADDR_EXT_SLAVE_MODE		(1 << 4)

struct axppmic_regdata {
	const char *name;
	uint8_t ereg, emask, eval, dval;
	uint8_t vreg, vmask;
	uint32_t base, delta;
	uint32_t base2, delta2;
};

struct axppmic_regdata axp806_regdata[] = {
	{ "dcdca", 0x10, (1 << 0), (1 << 0), (0 << 0),
	  0x12, 0x7f, 600000, 10000, 1120000, 20000 },
	{ "dcdcb", 0x10, (1 << 1), (1 << 1), (0 << 1),
	  0x13, 0x1f, 1000000, 50000 },
	{ "dcdcc", 0x10, (1 << 2), (1 << 2), (0 << 2),
	  0x14, 0x7f, 600000, 10000, 1120000, 20000 },
	{ "dcdcd", 0x10, (1 << 3), (1 << 3), (0 << 3),
	  0x15, 0x3f, 600000, 20000, 1600000, 100000 },
	{ "dcdce", 0x10, (1 << 4), (1 << 4), (0 << 4),
	  0x16, 0x1f, 1100000, 100000 },
	{ "aldo1", 0x10, (1 << 5), (1 << 5), (0 << 5),
	  0x17, 0x1f, 700000, 100000 },
	{ "aldo2", 0x10, (1 << 6), (1 << 6), (0 << 6),
	  0x18, 0x1f, 700000, 100000 },
	{ "aldo3", 0x10, (1 << 7), (1 << 7), (0 << 7),
	  0x19, 0x1f, 700000, 100000 },
	{ "bldo1", 0x11, (1 << 0), (1 << 0), (0 << 0),
	  0x20, 0x0f, 700000, 100000 },
	{ "bldo2", 0x11, (1 << 1), (1 << 1), (0 << 1),
	  0x21, 0x0f, 700000, 100000 },
	{ "bldo3", 0x11, (1 << 2), (1 << 2), (0 << 2),
	  0x22, 0x0f, 700000, 100000 },
	{ "bldo4", 0x11, (1 << 3), (1 << 3), (0 << 3),
	  0x23, 0x0f, 700000, 100000 },
	{ "cldo1", 0x11, (1 << 4), (1 << 4), (0 << 4),
	  0x24, 0x1f, 700000, 100000 },
	{ "cldo2", 0x11, (1 << 5), (1 << 5), (0 << 5),
	  0x25, 0x1f, 700000, 100000, 3600000, 200000 },
	{ "cldo3", 0x11, (1 << 6), (1 << 6), (0 << 6),
	  0x26, 0x1f, 700000, 100000 },
	{ "sw", 0x11, (1 << 7), (1 << 7), (0 << 7) },
	{ NULL }
};

struct axppmic_regdata axp809_regdata[] = {
	{ "dcdc1", 0x10, (1 << 1), (1 << 1), (0 << 1),
	  0x21, 0x1f, 1600000, 100000 },
	{ "dcdc2", 0x10, (1 << 2), (1 << 2), (0 << 2),
	  0x22, 0x3f, 600000, 20000 },
	{ "dcdc3", 0x10, (1 << 3), (1 << 3), (0 << 3),
	  0x23, 0x3f, 600000, 20000 },
	{ "dcdc4", 0x10, (1 << 4), (1 << 4), (0 << 4),
	  0x24, 0x3f, 600000, 20000, 1800000, 100000 },
	{ "dcdc5", 0x10, (1 << 5), (1 << 5), (0 << 5),
	  0x25, 0x1f, 1000000, 50000 },
	{ "dc5ldo", 0x10, (1 << 0), (1 << 0), (0 << 0),
	  0x1c, 0x07, 700000, 100000 },
	{ "aldo1", 0x10, (1 << 6), (1 << 6), (0 << 6),
	  0x28, 0x1f, 700000, 100000 },
	{ "aldo2", 0x10, (1 << 7), (1 << 7), (0 << 7),
	  0x28, 0x1f, 700000, 100000 },
	{ "aldo3", 0x12, (1 << 5), (1 << 5), (0 << 5),
	  0x28, 0x1f, 700000, 100000 },
	{ "dldo1", 0x12, (1 << 3), (1 << 3), (0 << 3),
	  0x15, 0x1f, 700000, 100000 },
	{ "dldo2", 0x12, (1 << 4), (1 << 4), (0 << 4),
	  0x16, 0x1f, 700000, 100000 },
	{ "eldo1", 0x12, (1 << 0), (1 << 0), (0 << 0),
	  0x19, 0x1f, 700000, 100000 },
	{ "eldo2", 0x12, (1 << 1), (1 << 1), (0 << 1),
	  0x1a, 0x1f, 700000, 100000 },
	{ "eldo3", 0x12, (1 << 2), (1 << 2), (0 << 2),
	  0x19, 0x1f, 700000, 100000 },
	{ "ldo_io0", 0x90, 0x07, 0x03, 0x04,
	  0x91, 0x1f, 700000, 100000 },
	{ "ldo_io1", 0x92, 0x07, 0x03, 0x04,
	  0x93, 0x1f, 700000, 100000 },
	{ NULL }
};

struct axppmic_device {
	const char *name;
	const char *chip;
	struct axppmic_regdata *regdata;
};

struct axppmic_device axppmic_devices[] = {
	{ "x-powers,axp806", "AXP806", axp806_regdata },
	{ "x-powers,axp809", "AXP809", axp809_regdata }
};

const struct axppmic_device *
axppmic_lookup(const char *name)
{
	int i;

	for (i = 0; i < nitems(axppmic_devices); i++) {
		if (strcmp(name, axppmic_devices[i].name) == 0)
			return &axppmic_devices[i];
	}

	return NULL;
}

struct axppmic_softc {
	struct device	sc_dev;
	void		*sc_cookie;
	uint16_t 	sc_rta;

	struct axppmic_regdata *sc_regdata;
};

inline uint8_t
axppmic_read_reg(struct axppmic_softc *sc, uint8_t reg)
{
	return rsb_read_1(sc->sc_cookie, sc->sc_rta, reg);
}

inline void
axppmic_write_reg(struct axppmic_softc *sc, uint8_t reg, uint8_t value)
{
	rsb_write_1(sc->sc_cookie, sc->sc_rta, reg, value);
}

int	axppmic_match(struct device *, void *, void *);
void	axppmic_attach(struct device *, struct device *, void *);

struct cfattach axppmic_rsb_ca = {
	sizeof(struct axppmic_softc), axppmic_match, axppmic_attach
};

struct cfdriver axppmic_rsb_cd = {
	NULL, "axppmic", DV_DULL
};

void	axppmic_attach_regulator(struct axppmic_softc *, int);

int
axppmic_match(struct device *parent, void *match, void *aux)
{
	struct rsb_attach_args *ra = aux;

	if (axppmic_lookup(ra->ra_name))
		return 1;
	return 0;
}

void
axppmic_attach(struct device *parent, struct device *self, void *aux)
{
	struct axppmic_softc *sc = (struct axppmic_softc *)self;
	const struct axppmic_device *device;
	struct rsb_attach_args *ra = aux;
	int node;

	sc->sc_cookie = ra->ra_cookie;
	sc->sc_rta = ra->ra_rta;

	device = axppmic_lookup(ra->ra_name);
	printf(": %s\n", device->chip);

	sc->sc_regdata = device->regdata;

	/* Switch AXP806 into master or slave mode. */
	if (strcmp(ra->ra_name, "x-powers,axp806") == 0) {
	    if (OF_getproplen(ra->ra_node, "x-powers,master-mode") == 0) {
			axppmic_write_reg(sc, AXP806_REG_ADDR_EXT,
			    AXP806_REG_ADDR_EXT_MASTER_MODE);
		} else {
			axppmic_write_reg(sc, AXP806_REG_ADDR_EXT,
			    AXP806_REG_ADDR_EXT_SLAVE_MODE);
		}
	}

	node = OF_getnodebyname(ra->ra_node, "regulators");
	if (node == 0)
		return;
	for (node = OF_child(node); node; node = OF_peer(node))
		axppmic_attach_regulator(sc, node);
}

struct axppmic_regulator {
	struct axppmic_softc *ar_sc;

	uint8_t ar_ereg, ar_emask;
	uint8_t ar_eval, ar_dval;

	uint8_t ar_vreg, ar_vmask;
	uint32_t ar_base, ar_delta;
	uint32_t ar_base2, ar_delta2;

	struct regulator_device ar_rd;
};

uint32_t axppmic_get_voltage(void *);
int	axppmic_set_voltage(void *, uint32_t);
int	axppmic_enable(void *, int);

void
axppmic_attach_regulator(struct axppmic_softc *sc, int node)
{
	struct axppmic_regulator *ar;
	char name[32];
	int i;

	name[0] = 0;
	OF_getprop(node, "name", name, sizeof(name));
	name[sizeof(name) - 1] = 0;
	for (i = 0; sc->sc_regdata[i].name; i++) {
		if (strcmp(sc->sc_regdata[i].name, name) == 0)
			break;
	}
	if (sc->sc_regdata[i].name == NULL)
		return;

	ar = malloc(sizeof(*ar), M_DEVBUF, M_WAITOK | M_ZERO);
	ar->ar_sc = sc;

	ar->ar_ereg = sc->sc_regdata[i].ereg;
	ar->ar_emask = sc->sc_regdata[i].emask;
	ar->ar_eval = sc->sc_regdata[i].eval;
	ar->ar_dval = sc->sc_regdata[i].dval;
	ar->ar_vreg = sc->sc_regdata[i].vreg;
	ar->ar_vmask = sc->sc_regdata[i].vmask;
	ar->ar_base = sc->sc_regdata[i].base;
	ar->ar_delta = sc->sc_regdata[i].delta;

	ar->ar_rd.rd_node = node;
	ar->ar_rd.rd_cookie = ar;
	ar->ar_rd.rd_get_voltage = axppmic_get_voltage;
	ar->ar_rd.rd_set_voltage = axppmic_set_voltage;
	ar->ar_rd.rd_enable = axppmic_enable;
	regulator_register(&ar->ar_rd);
}

uint32_t
axppmic_get_voltage(void *cookie)
{
	struct axppmic_regulator *ar = cookie;
	uint32_t voltage;
	uint8_t value;

	value = axppmic_read_reg(ar->ar_sc, ar->ar_vreg);
	value &= ar->ar_vmask;
	voltage = ar->ar_base + value * ar->ar_delta;
	if (ar->ar_base2 > 0 && voltage > ar->ar_base2) {
		value -= (ar->ar_base2 - ar->ar_base) / ar->ar_delta;
		voltage = ar->ar_base2 + value * ar->ar_delta2;
	}
	return voltage;
}

int
axppmic_set_voltage(void *cookie, uint32_t voltage)
{
	struct axppmic_regulator *ar = cookie;
	uint32_t value, reg;

	if (voltage < ar->ar_base)
		return EINVAL;
	value = (voltage - ar->ar_base) / ar->ar_delta;
	if (ar->ar_base2 > 0 && voltage > ar->ar_base2) {
		value = (ar->ar_base2 - ar->ar_base) / ar->ar_delta;
		value += (voltage - ar->ar_base2) / ar->ar_delta2;
	}
	if (value > ar->ar_vmask)
		return EINVAL;

	reg = axppmic_read_reg(ar->ar_sc, ar->ar_vreg);
	reg &= ar->ar_vmask;
	axppmic_write_reg(ar->ar_sc, ar->ar_vreg, reg | value);
	return 0;
}

int
axppmic_enable(void *cookie, int on)
{
	struct axppmic_regulator *ar = cookie;
	uint8_t reg;

	reg = axppmic_read_reg(ar->ar_sc, ar->ar_ereg);
	reg &= ~ar->ar_emask;
	if (on)
		reg |= ar->ar_eval;
	else
		reg |= ar->ar_dval;
	axppmic_write_reg(ar->ar_sc, ar->ar_ereg, reg);
	return 0;
}
