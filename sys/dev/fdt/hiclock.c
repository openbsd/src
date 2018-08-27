/*	$OpenBSD: hiclock.c,v 1.1 2018/08/27 20:05:06 kettenis Exp $	*/
/*
 * Copyright (c) 2018 Mark Kettenis <kettenis@openbsd.org>
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

#include <machine/intr.h>
#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_clock.h>
#include <dev/ofw/ofw_misc.h>
#include <dev/ofw/fdt.h>

#define HREAD4(sc, reg)							\
	(bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg)))
#define HWRITE4(sc, reg, val)						\
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))

/*
 * This driver is based on preliminary device tree bindings and will
 * almost certainly need changes once the official bindings land in
 * mainline Linux.  Support for these preliminary bindings will be
 * dropped as soon as official bindings are available.
 */

struct hiclock_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;

	struct clock_device	sc_cd;
};

int hiclock_match(struct device *, void *, void *);
void hiclock_attach(struct device *, struct device *, void *);

struct cfattach	hiclock_ca = {
	sizeof (struct hiclock_softc), hiclock_match, hiclock_attach
};

struct cfdriver hiclock_cd = {
	NULL, "hiclock", DV_DULL
};

uint32_t kirin970_crgctrl_get_frequency(void *, uint32_t *);
void	kirin970_crgctrl_enable(void *, uint32_t *, int);
uint32_t hiclock_get_frequency(void *, uint32_t *);
void	hiclock_enable(void *, uint32_t *, int);

int
hiclock_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	if (OF_is_compatible(faa->fa_node, "hisilicon,kirin970-crgctrl") ||
	    OF_is_compatible(faa->fa_node, "hisilicon,kirin970-pctrl") ||
	    OF_is_compatible(faa->fa_node, "hisilicon,kirin970-pmuctrl") ||
	    OF_is_compatible(faa->fa_node, "hisilicon,kirin970-pmctrl") ||
	    OF_is_compatible(faa->fa_node, "hisilicon,kirin970-sctrl") ||
	    OF_is_compatible(faa->fa_node, "hisilicon,kirin970-iomcu") ||
	    OF_is_compatible(faa->fa_node, "hisilicon,media1-crg") ||
	    OF_is_compatible(faa->fa_node, "hisilicon,media2-crg"))
		return 10;	/* Must beat syscon(4). */

	return 0;
}

void
hiclock_attach(struct device *parent, struct device *self, void *aux)
{
	struct hiclock_softc *sc = (struct hiclock_softc *)self;
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

	if (OF_is_compatible(faa->fa_node, "syscon")) {
		regmap_register(faa->fa_node, sc->sc_iot, sc->sc_ioh,
		    faa->fa_reg[0].size);
	}

	printf("\n");

	sc->sc_cd.cd_node = faa->fa_node;
	sc->sc_cd.cd_cookie = sc;
	if (OF_is_compatible(faa->fa_node, "hisilicon,kirin970-crgctrl")) {
	    sc->sc_cd.cd_get_frequency = kirin970_crgctrl_get_frequency;
	    sc->sc_cd.cd_enable = kirin970_crgctrl_enable;
	} else {
	    sc->sc_cd.cd_get_frequency = hiclock_get_frequency;
	    sc->sc_cd.cd_enable = hiclock_enable;
	}
	clock_register(&sc->sc_cd);
}

#define KIRIN970_CLKIN_SYS		0
#define KIRIN970_CLK_PPLL0		3
#define KIRIN970_CLK_PPLL2		5
#define KIRIN970_CLK_PPLL3		6

#define KIRIN970_CLK_SD_SYS		22
#define KIRIN970_CLK_SDIO_SYS		23
#define KIRIN970_CLK_GATE_ABB_USB	29
#define KIRIN970_CLK_MUX_SD_SYS		68
#define KIRIN970_CLK_MUX_SD_PLL 	69
#define KIRIN970_CLK_MUX_SDIO_SYS	70
#define KIRIN970_CLK_MUX_SDIO_PLL 	71
#define KIRIN970_CLK_DIV_SD		93
#define KIRIN970_CLK_DIV_SDIO		94
#define KIRIN970_HCLK_GATE_USB3OTG	147
#define KIRIN970_HCLK_GATE_USB3DVFS	148
#define KIRIN970_HCLK_GATE_SDIO		149
#define KIRIN970_CLK_GATE_SD		159
#define KIRIN970_HCLK_GATE_SD		160
#define KIRIN970_CLK_GATE_SDIO		161
#define KIRIN970_CLK_GATE_USB3OTG_REF	189

uint32_t
kirin970_crgctrl_get_frequency(void *cookie, uint32_t *cells)
{
	struct hiclock_softc *sc = cookie;
	uint32_t idx = cells[0];
	uint32_t reg, freq, div;
	int mux;

	switch (idx) {
	case KIRIN970_CLKIN_SYS:
		return 19200000;
	case KIRIN970_CLK_PPLL0:
		return 1660000000;
	case KIRIN970_CLK_PPLL2:
		return 1920000000;
	case KIRIN970_CLK_PPLL3:
		return 1200000000;
	case KIRIN970_CLK_SD_SYS:
	case KIRIN970_CLK_SDIO_SYS:
		idx = KIRIN970_CLKIN_SYS;
		freq = kirin970_crgctrl_get_frequency(cookie, &idx);
		return freq / 6;
	case KIRIN970_CLK_MUX_SD_SYS:
		reg = HREAD4(sc, 0x0b8);
		mux = (reg >> 6) & 0x1;
		idx = mux ? KIRIN970_CLK_DIV_SD : KIRIN970_CLK_SD_SYS;
		return kirin970_crgctrl_get_frequency(cookie, &idx);
	case KIRIN970_CLK_MUX_SD_PLL:
		reg = HREAD4(sc, 0x0b8);
		mux = (reg >> 4) & 0x3;
		switch (mux) {
		case 0:
			idx = KIRIN970_CLK_PPLL0;
			break;
		case 1:
			idx = KIRIN970_CLK_PPLL3;
			break;
		case 2:
		case 3:
			idx = KIRIN970_CLK_PPLL2;
			break;
		}
		return kirin970_crgctrl_get_frequency(cookie, &idx);
	case KIRIN970_CLK_DIV_SD:
		reg = HREAD4(sc, 0x0b8);
		div = (reg >> 0) & 0xf;
		idx = KIRIN970_CLK_MUX_SD_PLL;
		freq = kirin970_crgctrl_get_frequency(cookie, &idx);
		return freq / (div + 1);
	case KIRIN970_CLK_GATE_SD:
		idx = KIRIN970_CLK_MUX_SD_SYS;
		return kirin970_crgctrl_get_frequency(cookie, &idx);
	case KIRIN970_CLK_GATE_SDIO:
		idx = KIRIN970_CLK_MUX_SDIO_SYS;
		return kirin970_crgctrl_get_frequency(cookie, &idx);
	}

	printf("%s: 0x%08x\n", __func__, idx);
	return 0;
}

void
kirin970_crgctrl_enable(void *cookie, uint32_t *cells, int on)
{
	uint32_t idx = cells[0];

	switch (idx) {
	case KIRIN970_CLK_GATE_ABB_USB:
	case KIRIN970_HCLK_GATE_USB3OTG:
	case KIRIN970_HCLK_GATE_USB3DVFS:
	case KIRIN970_CLK_GATE_SD:
	case KIRIN970_HCLK_GATE_SD:
	case KIRIN970_CLK_GATE_USB3OTG_REF:
		/* Enabled by default. */
		return;
	}

	printf("%s: 0x%08x\n", __func__, idx);
}

uint32_t
hiclock_get_frequency(void *cookie, uint32_t *cells)
{
	uint32_t idx = cells[0];

	printf("%s: 0x%08x\n", __func__, idx);
	return 0;
}

void
hiclock_enable(void *cookie, uint32_t *cells, int on)
{
	uint32_t idx = cells[0];

	printf("%s: 0x%08x\n", __func__, idx);
}
