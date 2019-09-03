/*	$OpenBSD: amlclock.c,v 1.6 2019/09/03 22:31:52 kettenis Exp $	*/
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

#include <machine/intr.h>
#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_clock.h>
#include <dev/ofw/ofw_misc.h>
#include <dev/ofw/fdt.h>

#define G12A_SYS_PLL		0
#define G12A_FCLK_DIV2		2
#define G12A_FCLK_DIV3		3
#define G12A_FCLK_DIV4		4
#define G12A_FCLK_DIV5		5
#define G12A_FCLK_DIV7		6
#define G12A_SD_EMMC_A		33
#define G12A_SD_EMMC_B		34
#define G12A_SD_EMMC_C		35
#define G12A_SD_EMMC_A_CLK0	60
#define G12A_SD_EMMC_B_CLK0	61
#define G12A_SD_EMMC_C_CLK0	62
#define G12A_USB		47
#define G12A_FCLK_DIV2P5	99
#define G12A_CPU_CLK		187
#define G12A_PCIE_PLL		201
#define G12A_SYS1_PLL		214
#define G12A_CPUB_CLK		224

#define HHI_PCIE_PLL_CNTL0	0x26
#define HHI_PCIE_PLL_CNTL1	0x27
#define HHI_PCIE_PLL_CNTL2	0x28
#define HHI_PCIE_PLL_CNTL3	0x29
#define HHI_PCIE_PLL_CNTL4	0x2a
#define HHI_PCIE_PLL_CNTL5	0x2b
#define HHI_GCLK_MPEG0		0x50
#define HHI_GCLK_MPEG1		0x51
#define HHI_SYS_CPU_CLK_CNTL0	0x67
#define HHI_SYS_CPUB_CLK_CNTL	0x82
#define HHI_NAND_CLK_CNTL	0x97
#define HHI_SD_EMMC_CLK_CNTL	0x99
#define HHI_SYS_PLL_CNTL0	0xbd
#define HHI_SYS1_PLL_CNTL0	0xe0

#define HREAD4(sc, reg)							\
	(regmap_read_4((sc)->sc_rm, (reg) << 2))
#define HWRITE4(sc, reg, val)						\
	regmap_write_4((sc)->sc_rm, (reg) << 2, (val))
#define HSET4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) | (bits))
#define HCLR4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) & ~(bits))

struct amlclock_gate {
	uint8_t reg;
	uint8_t bit;
};

struct amlclock_gate aml_g12a_gates[] = {
	[G12A_SD_EMMC_A] = { HHI_GCLK_MPEG0, 24 },
	[G12A_SD_EMMC_B] = { HHI_GCLK_MPEG0, 25 },
	[G12A_SD_EMMC_C] = { HHI_GCLK_MPEG0, 26 },
	[G12A_USB] = { HHI_GCLK_MPEG1, 26 },

	[G12A_SD_EMMC_A_CLK0] = { HHI_SD_EMMC_CLK_CNTL, 7 },
	[G12A_SD_EMMC_B_CLK0] = { HHI_SD_EMMC_CLK_CNTL, 23 },
	[G12A_SD_EMMC_C_CLK0] = { HHI_NAND_CLK_CNTL, 7 },
};

struct amlclock_softc {
	struct device		sc_dev;
	struct regmap		*sc_rm;
	int			sc_node;

	struct amlclock_gate	*sc_gates;
	int			sc_ngates;

	struct clock_device	sc_cd;
	uint32_t		sc_xtal;
};

int amlclock_match(struct device *, void *, void *);
void amlclock_attach(struct device *, struct device *, void *);

struct cfattach	amlclock_ca = {
	sizeof (struct amlclock_softc), amlclock_match, amlclock_attach
};

struct cfdriver amlclock_cd = {
	NULL, "amlclock", DV_DULL
};

uint32_t amlclock_get_frequency(void *, uint32_t *);
int	amlclock_set_frequency(void *, uint32_t *, uint32_t);
void	amlclock_enable(void *, uint32_t *, int);

int
amlclock_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return (OF_is_compatible(faa->fa_node, "amlogic,g12a-clkc") ||
	    OF_is_compatible(faa->fa_node, "amlogic,g12b-clkc"));
}

void
amlclock_attach(struct device *parent, struct device *self, void *aux)
{
	struct amlclock_softc *sc = (struct amlclock_softc *)self;
	struct fdt_attach_args *faa = aux;

	sc->sc_rm = regmap_bynode(OF_parent(faa->fa_node));
	if (sc->sc_rm == NULL) {
		printf(": no registers\n");
		return;
	}

	sc->sc_node = faa->fa_node;
	printf("\n");

	sc->sc_gates = aml_g12a_gates;
	sc->sc_ngates = nitems(aml_g12a_gates);

	sc->sc_xtal = clock_get_frequency(sc->sc_node, "xtal");

	sc->sc_cd.cd_node = faa->fa_node;
	sc->sc_cd.cd_cookie = sc;
	sc->sc_cd.cd_get_frequency = amlclock_get_frequency;
	sc->sc_cd.cd_set_frequency = amlclock_set_frequency;
	sc->sc_cd.cd_enable = amlclock_enable;
	clock_register(&sc->sc_cd);
}

uint32_t
amlclock_get_cpu_frequency(struct amlclock_softc *sc, bus_size_t offset)
{
	uint32_t reg, mux, div;
	uint32_t idx;

	reg = HREAD4(sc, offset);
	mux = (reg >> 11) & 1;
	if (mux) {
		if (offset == HHI_SYS_CPU_CLK_CNTL0)
			idx = G12A_SYS1_PLL;
		else
			idx = G12A_SYS_PLL;
		return amlclock_get_frequency(sc, &idx);
	}
	mux = (reg >> 10) & 1;
	if (mux) {
		div = ((reg >> 18) & 1) ? ((reg & 0x3f) >> 20) : 1;
		mux = (reg >> 16) & 3;
	} else {
		div = ((reg >> 2) & 1) ? ((reg & 0x3f) >> 4) : 1;
		mux = (reg >> 0) & 3;
	}
	switch (mux) {
	case 0:
		return sc->sc_xtal / div;
	case 1:
		idx = G12A_FCLK_DIV2;
		break;
	case 2:
		idx = G12A_FCLK_DIV3;
		break;
	case 3:
		return 0;
	}
	return amlclock_get_frequency(sc, &idx) / div;
}

uint32_t
amlclock_get_frequency(void *cookie, uint32_t *cells)
{
	struct amlclock_softc *sc = cookie;
	uint32_t idx = cells[0];
	uint32_t reg, mux, div;
	uint32_t m, n;

	switch (idx) {
	case G12A_SYS_PLL:
		reg = HREAD4(sc, HHI_SYS_PLL_CNTL0);
		div = 1 << ((reg >> 16) & 0x7);
		m = (reg >> 0) & 0xff;
		n = (reg >> 10) & 0x1f;
		return (((uint64_t)sc->sc_xtal * m) / n) / div;
	case G12A_SYS1_PLL:
		reg = HREAD4(sc, HHI_SYS1_PLL_CNTL0);
		div = 1 << ((reg >> 16) & 0x7);
		m = (reg >> 0) & 0xff;
		n = (reg >> 10) & 0x1f;
		return (((uint64_t)sc->sc_xtal * m) / n) / div;
	case G12A_FCLK_DIV2:
		return 1000000000;
	case G12A_FCLK_DIV3:
		return 666666666;
	case G12A_FCLK_DIV4:
		return 500000000;
	case G12A_FCLK_DIV5:
		return 400000000;
	case G12A_FCLK_DIV7:
		return 285714285;
	case G12A_FCLK_DIV2P5:
		return 800000000;

	case G12A_SD_EMMC_A_CLK0:
		reg = HREAD4(sc, HHI_SD_EMMC_CLK_CNTL);
		mux = (reg >> 9) & 0x7;
		div = ((reg >> 0) & 0x7f) + 1;
		switch (mux) {
		case 0:
			return sc->sc_xtal / div;
		case 1:
			idx = G12A_FCLK_DIV2;
			break;
		case 2:
			idx = G12A_FCLK_DIV3;
			break;
		case 3:
			idx = G12A_FCLK_DIV5;
			break;
		case 4:
			idx = G12A_FCLK_DIV7;
			break;
		default:
			goto fail;
		}
		return amlclock_get_frequency(sc, &idx) / div;
	case G12A_SD_EMMC_B_CLK0:
		reg = HREAD4(sc, HHI_SD_EMMC_CLK_CNTL);
		mux = (reg >> 25) & 0x7;
		div = ((reg >> 16) & 0x7f) + 1;
		switch (mux) {
		case 0:
			return sc->sc_xtal / div;
		case 1:
			idx = G12A_FCLK_DIV2;
			break;
		case 2:
			idx = G12A_FCLK_DIV3;
			break;
		case 3:
			idx = G12A_FCLK_DIV5;
			break;
		case 4:
			idx = G12A_FCLK_DIV7;
			break;
		default:
			goto fail;
		}
		return amlclock_get_frequency(sc, &idx) / div;
	case G12A_SD_EMMC_C_CLK0:
		reg = HREAD4(sc, HHI_NAND_CLK_CNTL);
		mux = (reg >> 9) & 0x7;
		div = ((reg >> 0) & 0x7f) + 1;
		switch (mux) {
		case 0:
			return sc->sc_xtal / div;
		case 1:
			idx = G12A_FCLK_DIV2;
			break;
		case 2:
			idx = G12A_FCLK_DIV3;
			break;
		case 3:
			idx = G12A_FCLK_DIV5;
			break;
		case 4:
			idx = G12A_FCLK_DIV7;
			break;
		default:
			goto fail;
		}
		return amlclock_get_frequency(sc, &idx) / div;
	case G12A_CPU_CLK:
		return amlclock_get_cpu_frequency(sc, HHI_SYS_CPU_CLK_CNTL0);
	case G12A_CPUB_CLK:
		return amlclock_get_cpu_frequency(sc, HHI_SYS_CPUB_CLK_CNTL);
	}

fail:
	printf("%s: 0x%08x\n", __func__, idx);
	return 0;
}

int
amlclock_set_frequency(void *cookie, uint32_t *cells, uint32_t freq)
{
	struct amlclock_softc *sc = cookie;
	uint32_t idx = cells[0];

	switch (idx) {
	case G12A_PCIE_PLL:
		/* Fixed at 100 MHz. */
		if (freq != 100000000)
			return -1;
		HWRITE4(sc, HHI_PCIE_PLL_CNTL0, 0x20090496);
		HWRITE4(sc, HHI_PCIE_PLL_CNTL0, 0x30090496);
		HWRITE4(sc, HHI_PCIE_PLL_CNTL1, 0x00000000);
		HWRITE4(sc, HHI_PCIE_PLL_CNTL2, 0x00001100);
		HWRITE4(sc, HHI_PCIE_PLL_CNTL3, 0x10058e00);
		HWRITE4(sc, HHI_PCIE_PLL_CNTL4, 0x000100c0);
		HWRITE4(sc, HHI_PCIE_PLL_CNTL5, 0x68000048);
		HWRITE4(sc, HHI_PCIE_PLL_CNTL5, 0x68000068);
		delay(20);
		HWRITE4(sc, HHI_PCIE_PLL_CNTL4, 0x008100c0);
		delay(10);
		HWRITE4(sc, HHI_PCIE_PLL_CNTL0, 0x34090496);
		HWRITE4(sc, HHI_PCIE_PLL_CNTL0, 0x14090496);
		delay(10);
		HWRITE4(sc, HHI_PCIE_PLL_CNTL2, 0x00001000);
		return 0;
	};

	printf("%s: 0x%08x\n", __func__, idx);
	return -1;
}

void
amlclock_enable(void *cookie, uint32_t *cells, int on)
{
	struct amlclock_softc *sc = cookie;
	uint32_t idx = cells[0];

	if (idx < sc->sc_ngates && sc->sc_gates[idx].reg != 0) {
		if (on)
			HSET4(sc, sc->sc_gates[idx].reg,
			    (1U << sc->sc_gates[idx].bit));
		else
			HCLR4(sc, sc->sc_gates[idx].reg,
			    (1U << sc->sc_gates[idx].bit));
		return;
	}

	switch (idx) {
	case G12A_FCLK_DIV2:
	case G12A_PCIE_PLL:
		/* Already enabled. */
		return;
	}

	printf("%s: 0x%08x\n", __func__, idx);
}
