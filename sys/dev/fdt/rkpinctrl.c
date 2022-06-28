/*	$OpenBSD: rkpinctrl.c,v 1.8 2022/06/28 23:43:12 naddy Exp $	*/
/*
 * Copyright (c) 2017, 2018 Mark Kettenis <kettenis@openbsd.org>
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
#include <dev/ofw/ofw_misc.h>
#include <dev/ofw/ofw_pinctrl.h>
#include <dev/ofw/fdt.h>

#ifdef __armv7__
#include <arm/simplebus/simplebusvar.h>
#else
#include <arm64/dev/simplebusvar.h>
#endif

/* RK3288 registers */
#define RK3288_GRF_GPIO1A_IOMUX		0x0000
#define RK3288_PMUGRF_GPIO0A_IOMUX	0x0084

/* RK3308 registers */
#define RK3308_GRF_GPIO0A_IOMUX		0x0000

/* RK3328 registers */
#define RK3328_GRF_GPIO0A_IOMUX		0x0000

/* RK3399 registers */
#define RK3399_GRF_GPIO2A_IOMUX		0xe000
#define RK3399_PMUGRF_GPIO0A_IOMUX	0x0000

struct rkpinctrl_softc {
	struct simplebus_softc	sc_sbus;

	struct regmap		*sc_grf;
	struct regmap		*sc_pmu;
};

int	rkpinctrl_match(struct device *, void *, void *);
void	rkpinctrl_attach(struct device *, struct device *, void *);

const struct cfattach	rkpinctrl_ca = {
	sizeof (struct rkpinctrl_softc), rkpinctrl_match, rkpinctrl_attach
};

struct cfdriver rkpinctrl_cd = {
	NULL, "rkpinctrl", DV_DULL
};

int	rk3288_pinctrl(uint32_t, void *);
int	rk3308_pinctrl(uint32_t, void *);
int	rk3328_pinctrl(uint32_t, void *);
int	rk3399_pinctrl(uint32_t, void *);

int
rkpinctrl_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return (OF_is_compatible(faa->fa_node, "rockchip,rk3288-pinctrl") ||
	    OF_is_compatible(faa->fa_node, "rockchip,rk3308-pinctrl") ||
	    OF_is_compatible(faa->fa_node, "rockchip,rk3328-pinctrl") ||
	    OF_is_compatible(faa->fa_node, "rockchip,rk3399-pinctrl"));
}

void
rkpinctrl_attach(struct device *parent, struct device *self, void *aux)
{
	struct rkpinctrl_softc *sc = (struct rkpinctrl_softc *)self;
	struct fdt_attach_args *faa = aux;
	uint32_t grf, pmu;

	grf = OF_getpropint(faa->fa_node, "rockchip,grf", 0);
	pmu = OF_getpropint(faa->fa_node, "rockchip,pmu", 0);
	sc->sc_grf = regmap_byphandle(grf);
	sc->sc_pmu = regmap_byphandle(pmu);

	if (sc->sc_grf == NULL && sc->sc_pmu == NULL) {
		printf(": no registers\n");
		return;
	}

	if (OF_is_compatible(faa->fa_node, "rockchip,rk3288-pinctrl"))
		pinctrl_register(faa->fa_node, rk3288_pinctrl, sc);
	else if (OF_is_compatible(faa->fa_node, "rockchip,rk3308-pinctrl"))
		pinctrl_register(faa->fa_node, rk3308_pinctrl, sc);
	else if (OF_is_compatible(faa->fa_node, "rockchip,rk3328-pinctrl"))
		pinctrl_register(faa->fa_node, rk3328_pinctrl, sc);
	else
		pinctrl_register(faa->fa_node, rk3399_pinctrl, sc);

	/* Attach GPIO banks. */
	simplebus_attach(parent, &sc->sc_sbus.sc_dev, faa);
}

/*
 * Rockchip RK3288
 */

int
rk3288_pull(uint32_t bank, uint32_t idx, uint32_t phandle)
{
	int node;

	node = OF_getnodebyphandle(phandle);
	if (node == 0)
		return -1;

	/* XXX */
	if (bank == 0)
		return -1;

	if (OF_getproplen(node, "bias-disable") == 0)
		return 0;
	if (OF_getproplen(node, "bias-pull-up") == 0)
		return 1;
	if (OF_getproplen(node, "bias-pull-down") == 0)
		return 2;

	return -1;
}

int
rk3288_strength(uint32_t bank, uint32_t idx, uint32_t phandle)
{
	int strength, level;
	int levels[4] = { 2, 4, 8, 12 };
	int node;

	node = OF_getnodebyphandle(phandle);
	if (node == 0)
		return -1;

	/* XXX */
	if (bank == 0)
		return -1;

	strength = OF_getpropint(node, "drive-strength", -1);
	if (strength == -1)
		return -1;

	/* Convert drive strength to level. */
	for (level = 3; level >= 0; level--) {
		if (strength >= levels[level])
			break;
	}
	return level;
}

int
rk3288_pinctrl(uint32_t phandle, void *cookie)
{
	struct rkpinctrl_softc *sc = cookie;
	uint32_t *pins;
	int node, len, i;

	KASSERT(sc->sc_grf);
	KASSERT(sc->sc_pmu);

	node = OF_getnodebyphandle(phandle);
	if (node == 0)
		return -1;

	len = OF_getproplen(node, "rockchip,pins");
	if (len <= 0)
		return -1;

	pins = malloc(len, M_TEMP, M_WAITOK);
	if (OF_getpropintarray(node, "rockchip,pins", pins, len) != len)
		goto fail;

	for (i = 0; i < len / sizeof(uint32_t); i += 4) {
		struct regmap *rm;
		bus_size_t base, off;
		uint32_t bank, idx, mux;
		int pull, strength;
		uint32_t mask, bits;
		int s;

		bank = pins[i];
		idx = pins[i + 1];
		mux = pins[i + 2];
		pull = rk3288_pull(bank, idx, pins[i + 3]);
		strength = rk3288_strength(bank, idx, pins[i + 3]);

		if (bank > 8 || idx > 32 || mux > 7)
			continue;

		/* Bank 0 lives in the PMU. */
		if (bank < 1) {
			rm = sc->sc_pmu;
			base = RK3288_PMUGRF_GPIO0A_IOMUX;
		} else {
			rm = sc->sc_grf;
			base = RK3288_GRF_GPIO1A_IOMUX - 0x10;
		}

		s = splhigh();

		/* IOMUX control */
		off = bank * 0x10 + (idx / 8) * 0x04;

		/* GPIO3D, GPIO4A and GPIO4B are special. */
		if ((bank == 3 && idx >= 24) || (bank == 4 && idx < 16)) {
			mask = (0x7 << ((idx % 4) * 4));
			bits = (mux << ((idx % 4) * 4));
		} else {
			mask = (0x3 << ((idx % 8) * 2));
			bits = (mux << ((idx % 8) * 2));
		}
		if (bank > 3 || (bank == 3 && idx >= 28))
			off += 0x04;
		if (bank > 4 || (bank == 4 && idx >= 4))
			off += 0x04;
		if (bank > 4 || (bank == 4 && idx >= 12))
			off += 0x04;
		regmap_write_4(rm, base + off, mask << 16 | bits);

		/* GPIO pad pull down and pull up control */
		if (pull >= 0) {
			off = 0x140 + bank * 0x10 + (idx / 8) * 0x04;
			mask = (0x3 << ((idx % 8) * 2));
			bits = (pull << ((idx % 8) * 2));
			regmap_write_4(rm, base + off, mask << 16 | bits);
		}

		/* GPIO drive strength control */
		if (strength >= 0) {
			off = 0x1c0 + bank * 0x10 + (idx / 8) * 0x04;
			mask = (0x3 << ((idx % 8) * 2));
			bits = (strength << ((idx % 8) * 2));
			regmap_write_4(rm, base + off, mask << 16 | bits);
		}

		splx(s);
	}

	free(pins, M_TEMP, len);
	return 0;

fail:
	free(pins, M_TEMP, len);
	return -1;
}

/*
 * Rockchip RK3308
 */

int
rk3308_pull(uint32_t bank, uint32_t idx, uint32_t phandle)
{
	int node;

	node = OF_getnodebyphandle(phandle);
	if (node == 0)
		return -1;

	if (OF_getproplen(node, "bias-disable") == 0)
		return 0;
	if (OF_getproplen(node, "bias-pull-up") == 0)
		return 1;
	if (OF_getproplen(node, "bias-pull-down") == 0)
		return 2;

	return -1;
}

int
rk3308_strength(uint32_t bank, uint32_t idx, uint32_t phandle)
{
	int strength, level;
	int levels[4] = { 2, 4, 8, 12 };
	int node;

	node = OF_getnodebyphandle(phandle);
	if (node == 0)
		return -1;

	strength = OF_getpropint(node, "drive-strength", -1);
	if (strength == -1)
		return -1;

	/* Convert drive strength to level. */
	for (level = 3; level >= 0; level--) {
		if (strength >= levels[level])
			break;
	}
	return level;
}

int
rk3308_pinctrl(uint32_t phandle, void *cookie)
{
	struct rkpinctrl_softc *sc = cookie;
	uint32_t *pins;
	int node, len, i;

	KASSERT(sc->sc_grf);

	node = OF_getnodebyphandle(phandle);
	if (node == 0)
		return -1;

	len = OF_getproplen(node, "rockchip,pins");
	if (len <= 0)
		return -1;

	pins = malloc(len, M_TEMP, M_WAITOK);
	if (OF_getpropintarray(node, "rockchip,pins", pins, len) != len)
		goto fail;

	for (i = 0; i < len / sizeof(uint32_t); i += 4) {
		struct regmap *rm = sc->sc_grf;
		bus_size_t base, off;
		uint32_t bank, idx, mux;
		int pull, strength;
		uint32_t mask, bits;
		int s;

		bank = pins[i];
		idx = pins[i + 1];
		mux = pins[i + 2];
		pull = rk3308_pull(bank, idx, pins[i + 3]);
		strength = rk3308_strength(bank, idx, pins[i + 3]);
 
		if (bank > 4 || idx > 32 || mux > 7)
			continue;

		base = RK3308_GRF_GPIO0A_IOMUX;

		s = splhigh();

		/* IOMUX control */
		off = bank * 0x20 + (idx / 8) * 0x08;

		/* GPIO1B, GPIO1C and GPIO3B are special. */
		if ((bank == 1) && (idx == 14)) {
			mask = (0xf << 12);
			bits = (mux << 12);
		} else if ((bank == 1) && (idx == 15)) {
			off += 4;
			mask = 0x3;
			bits = mux;
		} else if ((bank == 1) && (idx >= 16 && idx <= 17)) {
			mask = (0x3 << ((idx - 16) * 2));
			bits = (mux << ((idx - 16) * 2));
		} else if ((bank == 1) && (idx >= 18 && idx <= 20)) {
			mask = (0xf << (((idx - 18) * 4) + 4));
			bits = (mux << (((idx - 18) * 4) + 4));
		} else if ((bank == 1) && (idx >= 21 && idx <= 23)) {
			off += 4;
			mask = (0xf << ((idx - 21) * 4));
			bits = (mux << ((idx - 21) * 4));
		} else if ((bank == 3) && (idx >= 12 && idx <= 13)) {
			mask = (0xf << (((idx - 12) * 4) + 8));
			bits = (mux << (((idx - 12) * 4) + 8));
		} else {
			mask = (0x3 << ((idx % 8) * 2));
			bits = (mux << ((idx % 8) * 2));
		}
		regmap_write_4(rm, base + off, mask << 16 | bits);

		/* GPIO pad pull down and pull up control */
		if (pull >= 0) {
			off = 0xa0 + bank * 0x10 + (idx / 8) * 0x04;
			mask = (0x3 << ((idx % 8) * 2));
			bits = (pull << ((idx % 8) * 2));
			regmap_write_4(rm, base + off, mask << 16 | bits);
		}

		/* GPIO drive strength control */
		if (strength >= 0) {
			off = 0x100 + bank * 0x10 + (idx / 8) * 0x04;
			mask = (0x3 << ((idx % 8) * 2));
			bits = (strength << ((idx % 8) * 2));
			regmap_write_4(rm, base + off, mask << 16 | bits);
		}

		splx(s);
	}

	free(pins, M_TEMP, len);
	return 0;

fail:
	free(pins, M_TEMP, len);
	return -1;
}

/*
 * Rockchip RK3328
 */

int
rk3328_pull(uint32_t bank, uint32_t idx, uint32_t phandle)
{
	int node;

	node = OF_getnodebyphandle(phandle);
	if (node == 0)
		return -1;

	if (OF_getproplen(node, "bias-disable") == 0)
		return 0;
	if (OF_getproplen(node, "bias-pull-up") == 0)
		return 1;
	if (OF_getproplen(node, "bias-pull-down") == 0)
		return 2;

	return -1;
}

int
rk3328_strength(uint32_t bank, uint32_t idx, uint32_t phandle)
{
	int strength, level;
	int levels[4] = { 2, 4, 8, 12 };
	int node;

	node = OF_getnodebyphandle(phandle);
	if (node == 0)
		return -1;

	strength = OF_getpropint(node, "drive-strength", -1);
	if (strength == -1)
		return -1;

	/* Convert drive strength to level. */
	for (level = 3; level >= 0; level--) {
		if (strength >= levels[level])
			break;
	}
	return level;
}

int
rk3328_pinctrl(uint32_t phandle, void *cookie)
{
	struct rkpinctrl_softc *sc = cookie;
	uint32_t *pins;
	int node, len, i;

	KASSERT(sc->sc_grf);

	node = OF_getnodebyphandle(phandle);
	if (node == 0)
		return -1;

	len = OF_getproplen(node, "rockchip,pins");
	if (len <= 0)
		return -1;

	pins = malloc(len, M_TEMP, M_WAITOK);
	if (OF_getpropintarray(node, "rockchip,pins", pins, len) != len)
		goto fail;

	for (i = 0; i < len / sizeof(uint32_t); i += 4) {
		struct regmap *rm = sc->sc_grf;
		bus_size_t base, off;
		uint32_t bank, idx, mux;
		int pull, strength;
		uint32_t mask, bits;
		int s;

		bank = pins[i];
		idx = pins[i + 1];
		mux = pins[i + 2];
		pull = rk3288_pull(bank, idx, pins[i + 3]);
		strength = rk3288_strength(bank, idx, pins[i + 3]);

		if (bank > 3 || idx > 32 || mux > 3)
			continue;

		base = RK3328_GRF_GPIO0A_IOMUX;

		s = splhigh();

		/* IOMUX control */
		off = bank * 0x10 + (idx / 8) * 0x04;

		/* GPIO2B, GPIO2C, GPIO3A and GPIO3B are special. */
		if (bank == 2 && idx == 15) {
			mask = 0x7;
			bits = mux;
		} else if (bank == 2 && idx >= 16 && idx <= 20) {
			mask = (0x7 << ((idx - 16) * 3));
			bits = (mux << ((idx - 16) * 3));
		} else if (bank == 2 && idx >= 21 && idx <= 23) {
			mask = (0x7 << ((idx - 21) * 3));
			bits = (mux << ((idx - 21) * 3));
		} else if (bank == 3 && idx <= 4) {
			mask = (0x7 << (idx * 3));
			bits = (mux << (idx * 3));
		} else if (bank == 3 && idx >= 5 && idx <= 7) {
			mask = (0x7 << ((idx - 5) * 3));
			bits = (mux << ((idx - 5) * 3));
		} else if (bank == 3 && idx >= 8 && idx <= 12) {
			mask = (0x7 << ((idx - 8) * 3));
			bits = (mux << ((idx - 8) * 3));
		} else if (bank == 3 && idx >= 13 && idx <= 15) {
			mask = (0x7 << ((idx - 13) * 3));
			bits = (mux << ((idx - 13) * 3));
		} else {
			mask = (0x3 << ((idx % 8) * 2));
			bits = (mux << ((idx % 8) * 2));
		}
		if (bank > 2 || (bank == 2 && idx >= 15))
			off += 0x04;
		if (bank > 2 || (bank == 2 && idx >= 21))
			off += 0x04;
		if (bank > 3 || (bank == 3 && idx >= 5))
			off += 0x04;
		if (bank > 3 || (bank == 3 && idx >= 13))
			off += 0x04;
		regmap_write_4(rm, base + off, mask << 16 | bits);

		/* GPIO pad pull down and pull up control */
		if (pull >= 0) {
			off = 0x100 + bank * 0x10 + (idx / 8) * 0x04;
			mask = (0x3 << ((idx % 8) * 2));
			bits = (pull << ((idx % 8) * 2));
			regmap_write_4(rm, base + off, mask << 16 | bits);
		}

		/* GPIO drive strength control */
		if (strength >= 0) {
			off = 0x200 + bank * 0x10 + (idx / 8) * 0x04;
			mask = (0x3 << ((idx % 8) * 2));
			bits = (strength << ((idx % 8) * 2));
			regmap_write_4(rm, base + off, mask << 16 | bits);
		}

		splx(s);
	}

	free(pins, M_TEMP, len);
	return 0;

fail:
	free(pins, M_TEMP, len);
	return -1;
}

/* 
 * Rockchip RK3399 
 */

int
rk3399_pull(uint32_t bank, uint32_t idx, uint32_t phandle)
{
	int pull_up, pull_down;
	int node;

	node = OF_getnodebyphandle(phandle);
	if (node == 0)
		return -1;

	if (bank == 2 && idx >= 16) {
		pull_up = 3;
		pull_down = 1;
	} else {
		pull_up = 1;
		pull_down = 2;
	}

	if (OF_getproplen(node, "bias-disable") == 0)
		return 0;
	if (OF_getproplen(node, "bias-pull-up") == 0)
		return pull_up;
	if (OF_getproplen(node, "bias-pull-down") == 0)
		return pull_down;

	return -1;
}

/* Magic because the drive strength configurations vary wildly. */

const int rk3399_strength_levels[][8] = {
	{ 2, 4, 8, 12 },			/* default */
	{ 3, 6, 9, 12 },			/* 1.8V or 3.0V */
	{ 5, 10, 15, 20 },			/* 1.8V only */
	{ 4, 6, 8, 10, 12, 14, 16, 18 },	/* 1.8V or 3.0V auto */
	{ 4, 7, 10, 13, 16, 19, 22, 26 },	/* 3.3V */
};

const int rk3399_strength_types[][4] = {
	{ 2, 2, 0, 0 },
	{ 1, 1, 1, 1 },
	{ 1, 1, 2, 2 },
	{ 4, 4, 4, 1 },
	{ 1, 3, 1, 1 },
};

const int rk3399_strength_regs[][4] = {
	{ 0x0080, 0x0088, 0x0090, 0x0098 },
	{ 0x00a0, 0x00a8, 0x00b0, 0x00b8 },
	{ 0x0100, 0x0104, 0x0108, 0x010c },
	{ 0x0110, 0x0118, 0x0120, 0x0128 },
	{ 0x012c, 0x0130, 0x0138, 0x013c },
};

int
rk3399_strength(uint32_t bank, uint32_t idx, uint32_t phandle)
{
	int strength, type, level;
	const int *levels;
	int node;

	node = OF_getnodebyphandle(phandle);
	if (node == 0)
		return -1;

	strength = OF_getpropint(node, "drive-strength", -1);
	if (strength == -1)
		return -1;

	/* Convert drive strength to level. */
	type = rk3399_strength_types[bank][idx / 8];
	levels = rk3399_strength_levels[type];
	for (level = 7; level >= 0; level--) {
		if (strength >= levels[level] && levels[level] > 0)
			break;
	}
	return level;
}

int
rk3399_pinctrl(uint32_t phandle, void *cookie)
{
	struct rkpinctrl_softc *sc = cookie;
	uint32_t *pins;
	int node, len, i;

	KASSERT(sc->sc_grf);
	KASSERT(sc->sc_pmu);

	node = OF_getnodebyphandle(phandle);
	if (node == 0)
		return -1;

	len = OF_getproplen(node, "rockchip,pins");
	if (len <= 0)
		return -1;

	pins = malloc(len, M_TEMP, M_WAITOK);
	if (OF_getpropintarray(node, "rockchip,pins", pins, len) != len)
		goto fail;

	for (i = 0; i < len / sizeof(uint32_t); i += 4) {
		struct regmap *rm;
		bus_size_t base, off;
		uint32_t bank, idx, mux;
		int pull, strength, type, shift;
		uint32_t mask, bits;
		int s;

		bank = pins[i];
		idx = pins[i + 1];
		mux = pins[i + 2];
		pull = rk3399_pull(bank, idx, pins[i + 3]);
		strength = rk3399_strength(bank, idx, pins[i + 3]);

		if (bank > 5 || idx > 32 || mux > 3)
			continue;

		/* Bank 0 and 1 live in the PMU. */
		if (bank < 2) {
			rm = sc->sc_pmu;
			base = RK3399_PMUGRF_GPIO0A_IOMUX;
		} else {
			rm = sc->sc_grf;
			base = RK3399_GRF_GPIO2A_IOMUX - 0x20;
		}

		s = splhigh();

		/* IOMUX control */
		off = bank * 0x10 + (idx / 8) * 0x04;
		mask = (0x3 << ((idx % 8) * 2));
		bits = (mux << ((idx % 8) * 2));
		regmap_write_4(rm, base + off, mask << 16 | bits);

		/* GPIO pad pull down and pull up control */
		if (pull >= 0) {
			off = 0x40 + bank * 0x10 + (idx / 8) * 0x04;
			mask = (0x3 << ((idx % 8) * 2));
			bits = (pull << ((idx % 8) * 2));
			regmap_write_4(rm, base + off, mask << 16 | bits);
		}

		/* GPIO drive strength control */
		if (strength >= 0) {
			off = rk3399_strength_regs[bank][idx / 8];
			type = rk3399_strength_types[bank][idx / 8];
			shift = (type > 2) ? 3 : 2;
			mask = (((1 << shift) - 1) << ((idx % 8) * shift));
			bits = (strength << ((idx % 8) * shift));
			if (mask & 0x0000ffff) {
				regmap_write_4(rm, base + off,
				    mask << 16 | (bits & 0x0000ffff));
			}
			if (mask & 0xffff0000) {
				regmap_write_4(rm, base + off + 0x04,
				    (mask & 0xffff0000) | bits >> 16);
			}
		}

		splx(s);
	}

	free(pins, M_TEMP, len);
	return 0;

fail:
	free(pins, M_TEMP, len);
	return -1;
}
