/*	$OpenBSD: rkpinctrl.c,v 1.2 2017/05/06 18:25:43 kettenis Exp $	*/
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

#include <machine/intr.h>
#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_misc.h>
#include <dev/ofw/ofw_pinctrl.h>
#include <dev/ofw/fdt.h>

#include <arm64/dev/simplebusvar.h>

/* Registers */
#define RK3399_GRF_GPIO2A_IOMUX		0xe000
#define RK3399_PMUGRF_GPIO0A_IOMUX	0x0000

struct rkpinctrl_softc {
	struct simplebus_softc	sc_sbus;

	struct regmap		*sc_grf;
	struct regmap		*sc_pmu;
};

int	rkpinctrl_match(struct device *, void *, void *);
void	rkpinctrl_attach(struct device *, struct device *, void *);

struct cfattach	rkpinctrl_ca = {
	sizeof (struct rkpinctrl_softc), rkpinctrl_match, rkpinctrl_attach
};

struct cfdriver rkpinctrl_cd = {
	NULL, "rkpinctrl", DV_DULL
};

int	rk3399_pinctrl(uint32_t, void *);

int
rkpinctrl_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "rockchip,rk3399-pinctrl");
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

	if (sc->sc_grf == NULL || sc->sc_pmu == NULL) {
		printf(": no registers\n");
		return;
	}

	pinctrl_register(faa->fa_node, rk3399_pinctrl, sc);

	/* Attach GPIO banks. */
	simplebus_attach(parent, &sc->sc_sbus.sc_dev, faa);
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

int
rk3399_strength(uint32_t phandle)
{
	int node;

	node = OF_getnodebyphandle(phandle);
	if (node == 0)
		return -1;

	/* XXX decode levels. */
	return OF_getpropint(node, "drive-strength", -1);
}


int
rk3399_pinctrl(uint32_t phandle, void *cookie)
{
	struct rkpinctrl_softc *sc = cookie;
	uint32_t *pins;
	int node, len, i;

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

		bank = pins[i];
		idx = pins[i + 1];
		mux = pins[i + 2];
		pull = rk3399_pull(bank, idx, pins[i + 3]);
		strength = rk3399_strength(pins[i + 3]);

		if (bank > 5 || idx > 32 || mux > 3)
			continue;

		/* XXX leave alone for now */
		if (strength >= 0)
			continue;

		/* Bank 0 and 1 live in the PMU. */
		if (bank < 2) {
			rm = sc->sc_pmu;
			base = RK3399_PMUGRF_GPIO0A_IOMUX;
		} else {
			rm = sc->sc_grf;
			base = RK3399_GRF_GPIO2A_IOMUX;
			bank -= 2;
		}

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
	}

	free(pins, M_TEMP, len);
	return 0;

fail:
	free(pins, M_TEMP, len);
	return -1;
}
