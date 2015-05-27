/* $OpenBSD: exclock.c,v 1.2 2015/05/27 00:06:14 jsg Exp $ */
/*
 * Copyright (c) 2012-2013 Patrick Wildt <patrick@blueri.se>
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

#include "fdt.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>
#include <sys/device.h>
#include <sys/evcount.h>
#include <sys/socket.h>
#include <sys/timeout.h>
#include <machine/intr.h>
#include <machine/bus.h>
#if NFDT > 0
#include <machine/fdt.h>
#endif
#include <armv7/armv7/armv7var.h>

/* registers */
#define CLOCK_APLL_CON0				0x0100
#define CLOCK_APLL_CON1				0x0104
#define CLOCK_BPLL_CON0				0x0110
#define CLOCK_BPLL_CON1				0x0114
#define CLOCK_EPLL_CON0				0x0130
#define CLOCK_EPLL_CON1				0x0134
#define CLOCK_EPLL_CON2				0x0138
#define CLOCK_VPLL_CON0				0x0140
#define CLOCK_VPLL_CON1				0x0144
#define CLOCK_VPLL_CON2				0x0148
#define CLOCK_CLK_DIV_CPU0			0x0500
#define CLOCK_CLK_DIV_CPU1			0x0504
#define CLOCK_CLK_DIV_TOP0			0x0510
#define CLOCK_CLK_DIV_TOP1			0x0514
#define CLOCK_PLL_DIV2_SEL			0x0A24
#define CLOCK_MPLL_CON0				0x4100
#define CLOCK_MPLL_CON1				0x4104

/* bits and bytes */
#define MPLL_FOUT_SEL_SHIFT			0x4
#define MPLL_FOUT_SEL_MASK			0x1
#define BPLL_FOUT_SEL_SHIFT			0x0
#define BPLL_FOUT_SEL_MASK			0x1

#define HCLK_FREQ				24000

#define HREAD4(sc, reg)							\
	(bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg)))
#define HWRITE4(sc, reg, val)						\
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))
#define HSET4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) | (bits))
#define HCLR4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) & ~(bits))

struct exclock_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
};

enum clocks {
	/* OSC */
	OSC,		/* 24 MHz OSC */

	/* PLLs */
	APLL,		/* ARM core clock */
	MPLL,		/* System bus clock for memory controller */
	BPLL,		/* Graphic 3D processor clock and 1066 MHz clock for memory controller if necessary */
	CPLL,		/* Multi Format Video Hardware Codec clock */
	GPLL,		/* Graphic 3D processor clock or other clocks for DVFS flexibility */
	EPLL,		/* Audio interface clocks and clocks for other external device interfaces */
	VPLL,		/* dithered PLL, helps to reduce the EMI of display and camera */
};

struct exclock_softc *exclock_sc;

int exclock_match(struct device *parent, void *v, void *aux);
void exclock_attach(struct device *parent, struct device *self, void *args);
int exclock_cpuspeed(int *);
unsigned int exclock_decode_pll_clk(enum clocks, unsigned int, unsigned int);
unsigned int exclock_get_pll_clk(enum clocks);
unsigned int exclock_get_armclk(void);
unsigned int exclock_get_i2cclk(void);

struct cfattach	exclock_ca = {
	sizeof (struct exclock_softc), NULL, exclock_attach
};
struct cfattach	exclock_fdt_ca = {
	sizeof (struct exclock_softc), exclock_match, exclock_attach
};

struct cfdriver exclock_cd = {
	NULL, "exclock", DV_DULL
};

int
exclock_match(struct device *parent, void *v, void *aux)
{
#if NFDT > 0
	struct armv7_attach_args *aa = aux;

	if (fdt_node_compatible("samsung,exynos5250-clock", aa->aa_node))
		return 1;
#endif

	return 0;
}

void
exclock_attach(struct device *parent, struct device *self, void *args)
{
	struct armv7_attach_args *aa = args;
	struct exclock_softc *sc = (struct exclock_softc *) self;
	struct armv7mem mem;

	exclock_sc = sc;
	sc->sc_iot = aa->aa_iot;
#if NFDT > 0
	if (aa->aa_node) {
		struct fdt_memory fdtmem;
		if (fdt_get_memory_address(aa->aa_node, 0, &fdtmem))
			panic("%s: could not extract memory data from FDT",
			    __func__);
		mem.addr = fdtmem.addr;
		mem.size = fdtmem.size;
	} else
#endif
	{

		mem.addr = aa->aa_dev->mem[0].addr;
		mem.size = aa->aa_dev->mem[0].size;
	}
	if (bus_space_map(sc->sc_iot, mem.addr, mem.size, 0, &sc->sc_ioh))
		panic("%s: bus_space_map failed!", __func__);

	printf(": Exynos 5 CPU freq: %d MHz",
	    exclock_get_armclk() / 1000);

	printf("\n");

	cpu_cpuspeed = exclock_cpuspeed;
}

int
exclock_cpuspeed(int *freq)
{
	*freq = exclock_get_armclk() / 1000;
	return (0);
}

unsigned int
exclock_decode_pll_clk(enum clocks pll, unsigned int r, unsigned int k)
{
	uint32_t m, p, s = 0, mask, fout, freq;
	/*
	 * APLL_CON: MIDV [25:16]
	 * MPLL_CON: MIDV [25:16]
	 * EPLL_CON: MIDV [24:16]
	 * VPLL_CON: MIDV [24:16]
	 * BPLL_CON: MIDV [25:16]: Exynos5
	 */

	switch (pll)
	{
	case APLL:
	case MPLL:
	case BPLL:
		mask = 0x3ff;
		break;
	default:
		mask = 0x1ff;
	}

	m = (r >> 16) & mask;

	/* PDIV [13:8] */
	p = (r >> 8) & 0x3f;
	/* SDIV [2:0] */
	s = r & 0x7;

	freq = HCLK_FREQ;

	if (pll == EPLL) {
		k = k & 0xffff;
		/* FOUT = (MDIV + K / 65536) * FIN / (PDIV * 2^SDIV) */
		fout = (m + k / 65536) * (freq / (p * (1 << s)));
	} else if (pll == VPLL) {
		k = k & 0xfff;
		/* FOUT = (MDIV + K / 1024) * FIN / (PDIV * 2^SDIV) */
		fout = (m + k / 1024) * (freq / (p * (1 << s)));
	} else {
		/* FOUT = MDIV * FIN / (PDIV * 2^(SDIV - 1)) */
		fout = m * (freq / (p * (1 << s)));
	}

	return fout;
}

unsigned int
exclock_get_pll_clk(enum clocks pll)
{
	struct exclock_softc *sc = exclock_sc;
	uint32_t freq;

	switch (pll) {
	case APLL:
		freq = exclock_decode_pll_clk(pll,
		    HREAD4(sc, CLOCK_APLL_CON0),
		    0);
		break;
	case MPLL:
		freq = exclock_decode_pll_clk(pll,
		    HREAD4(sc, CLOCK_MPLL_CON0),
		    0);
		break;
	case BPLL:
		freq = exclock_decode_pll_clk(pll,
		    HREAD4(sc, CLOCK_BPLL_CON0),
		    0);
		break;
	case EPLL:
		freq = exclock_decode_pll_clk(pll,
		    HREAD4(sc, CLOCK_EPLL_CON0),
		    HREAD4(sc, CLOCK_EPLL_CON1));
		break;
	case VPLL:
		freq = exclock_decode_pll_clk(pll,
		    HREAD4(sc, CLOCK_VPLL_CON0),
		    HREAD4(sc, CLOCK_VPLL_CON1));
		break;
	default:
		return 0;
	}

	/*
	 * According to the user manual, in EVT1 MPLL and BPLL always gives
	 * 1.6GHz clock, so divide by 2 to get 800MHz MPLL clock.
	 */
	if (pll == MPLL || pll == BPLL) {
		uint32_t freq_sel;
		uint32_t pll_div2_sel = HREAD4(sc, CLOCK_PLL_DIV2_SEL);

		switch (pll) {
		case MPLL:
			freq_sel = (pll_div2_sel >> MPLL_FOUT_SEL_SHIFT)
					& MPLL_FOUT_SEL_MASK;
			break;
		case BPLL:
			freq_sel = (pll_div2_sel >> BPLL_FOUT_SEL_SHIFT)
					& BPLL_FOUT_SEL_MASK;
			break;
		default:
			freq_sel = -1;
			break;
		}

		if (freq_sel == 0)
			freq /= 2;
	}

	return freq;
}

unsigned int
exclock_get_armclk()
{
	struct exclock_softc *sc = exclock_sc;
	uint32_t div, armclk, arm_ratio, arm2_ratio;

	div = HREAD4(sc, CLOCK_CLK_DIV_CPU0);

	/* ARM_RATIO: [2:0], ARM2_RATIO: [30:28] */
	arm_ratio = (div >> 0) & 0x7;
	arm2_ratio = (div >> 28) & 0x7;

	armclk = exclock_get_pll_clk(APLL) / (arm_ratio + 1);
	armclk /= (arm2_ratio + 1);

	return armclk;
}

unsigned int
exclock_get_i2cclk()
{
	struct exclock_softc *sc = exclock_sc;
	uint32_t aclk_66, aclk_66_pre, div, ratio;

	div = HREAD4(sc, CLOCK_CLK_DIV_TOP1);
	ratio = (div >> 24) & 0x7;
	aclk_66_pre = exclock_get_pll_clk(MPLL) / (ratio + 1);
	div = HREAD4(sc, CLOCK_CLK_DIV_TOP0);
	ratio = (div >> 0) & 0x7;
	aclk_66 = aclk_66_pre / (ratio + 1);

	return aclk_66;
}
