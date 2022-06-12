/*	$OpenBSD: stfclock.c,v 1.2 2022/06/12 10:51:55 kettenis Exp $	*/
/*
 * Copyright (c) 2022 Mark Kettenis <kettenis@openbsd.org>
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
#include <dev/ofw/fdt.h>

/* Clock IDs */
#define JH7100_CLK_CPUNDBUS_ROOT	0
#define JH7100_CLK_GMACUSB_ROOT		3
#define JH7100_CLK_PERH0_ROOT		4
#define JH7100_CLK_PERH1_ROOT		5
#define JH7100_CLK_CPUNBUS_ROOT_DIV	12
#define JH7100_CLK_PERH0_SRC		14
#define JH7100_CLK_PERH1_SRC		15
#define JH7100_CLK_PLL2_REF		19
#define JH7100_CLK_AHB_BUS		22
#define JH7100_CLK_SDIO0_AHB		114
#define JH7100_CLK_SDIO0_CCLKINT	115
#define JH7100_CLK_SDIO0_CCLKINT_INV	116
#define JH7100_CLK_SDIO1_AHB		117
#define JH7100_CLK_SDIO1_CCLKINT	118
#define JH7100_CLK_SDIO1_CCLKINT_INV	119
#define JH7100_CLK_GMAC_AHB		120
#define JH7100_CLK_GMAC_ROOT_DIV	121
#define JH7100_CLK_GMAC_GTX		123
#define JH7100_CLK_UART0_CORE		147
#define JH7100_CLK_UART3_CORE		162
#define JH7100_CLK_TEMP_APB		183
#define JH7100_CLK_TEMP_SENSE		184
#define JH7100_CLK_PLL0_OUT		186
#define JH7100_CLK_PLL1_OUT		187
#define JH7100_CLK_PLL2_OUT		188

#define JH7100_CLK_OSC_SYS		255
#define JH7100_CLK_OSC_AUD		254

/* Registers */
#define CLKMUX_MASK		0x03000000
#define CLKMUX_SHIFT		24
#define CLKDIV_MASK		0x00ffffff
#define CLKDIV_SHIFT		0

#define HREAD4(sc, reg)							\
	(bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg)))
#define HWRITE4(sc, reg, val)						\
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))
#define HSET4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) | (bits))
#define HCLR4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) & ~(bits))

struct stfclock_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	int			sc_node;

	struct clock_device	sc_cd;
};

int	stfclock_match(struct device *, void *, void *);
void	stfclock_attach(struct device *, struct device *, void *);

const struct cfattach stfclock_ca = {
	sizeof (struct stfclock_softc), stfclock_match, stfclock_attach
};

struct cfdriver stfclock_cd = {
	NULL, "stfclock", DV_DULL
};

uint32_t stfclock_get_frequency(void *, uint32_t *);
int	stfclock_set_frequency(void *, uint32_t *, uint32_t);
void	stfclock_enable(void *, uint32_t *, int);

int
stfclock_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "starfive,jh7100-clkgen");
}

void
stfclock_attach(struct device *parent, struct device *self, void *aux)
{
	struct stfclock_softc *sc = (struct stfclock_softc *)self;
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

	sc->sc_node = faa->fa_node;

	printf("\n");

	sc->sc_cd.cd_node = faa->fa_node;
	sc->sc_cd.cd_cookie = sc;
	sc->sc_cd.cd_get_frequency = stfclock_get_frequency;
	sc->sc_cd.cd_set_frequency = stfclock_set_frequency;
	sc->sc_cd.cd_enable = stfclock_enable;
	clock_register(&sc->sc_cd);
}

uint32_t
stfclock_get_frequency(void *cookie, uint32_t *cells)
{
	struct stfclock_softc *sc = cookie;
	uint32_t idx = cells[0];
	uint32_t parent, freq;
	uint32_t reg, div, mux;

	switch (idx) {
	case JH7100_CLK_OSC_SYS:
		return clock_get_frequency(sc->sc_node, "osc_sys");
	case JH7100_CLK_OSC_AUD:
		return clock_get_frequency(sc->sc_node, "osc_aud");

	case JH7100_CLK_PLL0_OUT:
		parent = JH7100_CLK_OSC_SYS;
		return 40 * stfclock_get_frequency(sc, &parent);
	case JH7100_CLK_PLL1_OUT:
		parent = JH7100_CLK_OSC_SYS;
		return 64 * stfclock_get_frequency(sc, &parent);
	case JH7100_CLK_PLL2_OUT:
		parent = JH7100_CLK_PLL2_REF;
		return 55 * stfclock_get_frequency(sc, &parent);
	}

	reg = HREAD4(sc, idx * 4);
	mux = (reg & CLKMUX_MASK) >> CLKMUX_SHIFT;
	div = (reg & CLKDIV_MASK) >> CLKDIV_SHIFT;

	switch (idx) {
	case JH7100_CLK_CPUNDBUS_ROOT:
		switch (mux) {
		default:
			parent = JH7100_CLK_OSC_SYS;
			break;
		case 1:
			parent = JH7100_CLK_PLL0_OUT;
			break;
		case 2:
			parent = JH7100_CLK_PLL1_OUT;
			break;
		case 3:
			parent = JH7100_CLK_PLL2_OUT;
			break;
		}
		return stfclock_get_frequency(sc, &parent);
	case JH7100_CLK_GMACUSB_ROOT:
		switch (mux) {
		default:
			parent = JH7100_CLK_OSC_SYS;
			break;
		case 1:
			parent = JH7100_CLK_PLL0_OUT;
			break;
		case 2:
			parent = JH7100_CLK_PLL2_OUT;
			break;
		}
		return stfclock_get_frequency(sc, &parent);	
	case JH7100_CLK_PERH0_ROOT:
		mux = (reg >> 24) & 1;
		parent = mux ? JH7100_CLK_PLL0_OUT : JH7100_CLK_OSC_SYS;
		return stfclock_get_frequency(sc, &parent);
	case JH7100_CLK_PERH1_ROOT:
		mux = (reg >> 24) & 1;
		parent = mux ? JH7100_CLK_PLL2_OUT : JH7100_CLK_OSC_SYS;
		return stfclock_get_frequency(sc, &parent);
	case JH7100_CLK_PLL2_REF:
		parent = mux ? JH7100_CLK_OSC_AUD : JH7100_CLK_OSC_SYS;
		return stfclock_get_frequency(sc, &parent);
	}

	switch (idx) {
	case JH7100_CLK_PERH0_SRC:
		parent = JH7100_CLK_PERH0_ROOT;
		break;
	case JH7100_CLK_PERH1_SRC:
		parent = JH7100_CLK_PERH1_ROOT;
		break;
	case JH7100_CLK_CPUNBUS_ROOT_DIV:
		parent = JH7100_CLK_CPUNDBUS_ROOT;
		break;
	case JH7100_CLK_AHB_BUS:
		parent = JH7100_CLK_CPUNBUS_ROOT_DIV;
		break;
	case JH7100_CLK_SDIO0_CCLKINT:
	case JH7100_CLK_UART3_CORE:
		parent = JH7100_CLK_PERH0_SRC;
		break;
	case JH7100_CLK_SDIO1_CCLKINT:
	case JH7100_CLK_UART0_CORE:
		parent = JH7100_CLK_PERH1_SRC;
		break;
	case JH7100_CLK_SDIO0_AHB:
	case JH7100_CLK_SDIO1_AHB:
	case JH7100_CLK_GMAC_AHB:
		parent = JH7100_CLK_AHB_BUS;
		div = 1;
		break;
	case JH7100_CLK_SDIO0_CCLKINT_INV:
		parent = JH7100_CLK_SDIO0_CCLKINT;
		div = 1;
		break;
	case JH7100_CLK_SDIO1_CCLKINT_INV:
		parent = JH7100_CLK_SDIO1_CCLKINT;
		div = 1;
		break;
	case JH7100_CLK_GMAC_ROOT_DIV:
		parent = JH7100_CLK_GMACUSB_ROOT;
		break;
	case JH7100_CLK_GMAC_GTX:
		parent = JH7100_CLK_GMAC_ROOT_DIV;
		break;
	default:
		printf("%s: 0x%08x\n", __func__, idx);
		return 0;
	}

	freq = stfclock_get_frequency(sc, &parent);
	return freq / div;
}

int
stfclock_set_frequency(void *cookie, uint32_t *cells, uint32_t freq)
{
	uint32_t idx = cells[0];

	printf("%s: 0x%08x\n", __func__, idx);
	return -1;
}
      
void
stfclock_enable(void *cookie, uint32_t *cells, int on)
{
	struct stfclock_softc *sc = cookie;
	uint32_t idx = cells[0];

	switch (idx) {
	case JH7100_CLK_SDIO0_CCLKINT:
	case JH7100_CLK_SDIO0_CCLKINT_INV:
	case JH7100_CLK_SDIO1_CCLKINT:
	case JH7100_CLK_SDIO1_CCLKINT_INV:
	case JH7100_CLK_SDIO0_AHB:
	case JH7100_CLK_SDIO1_AHB:
	case JH7100_CLK_GMAC_AHB:
	case JH7100_CLK_GMAC_GTX:
	case JH7100_CLK_UART0_CORE:
	case JH7100_CLK_UART3_CORE:
	case JH7100_CLK_TEMP_APB:
	case JH7100_CLK_TEMP_SENSE:
		if (on)
			HSET4(sc, idx * 4, 1U << 31);
		else
			HCLR4(sc, idx * 4, 1U << 31);
		return;
	}

	printf("%s: 0x%08x\n", __func__, idx);
}
