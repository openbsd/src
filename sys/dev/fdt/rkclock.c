/*	$OpenBSD: rkclock.c,v 1.15 2017/08/27 09:51:14 kettenis Exp $	*/
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
#include <sys/sysctl.h>
#include <sys/device.h>

#include <machine/intr.h>
#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_clock.h>
#include <dev/ofw/fdt.h>

/* RK3288 registers */
#define RK3288_CRU_CPLL_CON(i)		(0x0020 + (i) * 4)
#define RK3288_CRU_GPLL_CON(i)		(0x0030 + (i) * 4)
#define RK3288_CRU_CLKSEL_CON(i)	(0x0060 + (i) * 4)

/* RK3399 registers */
#define RK3399_CRU_LPLL_CON(i)		(0x0000 + (i) * 4)
#define RK3399_CRU_BPLL_CON(i)		(0x0020 + (i) * 4)
#define RK3399_CRU_DPLL_CON(i)		(0x0020 + (i) * 4)
#define RK3399_CRU_CPLL_CON(i)		(0x0060 + (i) * 4)
#define RK3399_CRU_GPLL_CON(i)		(0x0080 + (i) * 4)
#define RK3399_CRU_NPLL_CON(i)		(0x00a0 + (i) * 4)
#define  RK3399_CRU_PLL_FBDIV_MASK		(0xfff << 0)
#define  RK3399_CRU_PLL_FBDIV_SHIFT		0
#define  RK3399_CRU_PLL_POSTDIV2_MASK		(0x7 << 12)
#define  RK3399_CRU_PLL_POSTDIV2_SHIFT		12
#define  RK3399_CRU_PLL_POSTDIV1_MASK		(0x7 << 8)
#define  RK3399_CRU_PLL_POSTDIV1_SHIFT		8
#define  RK3399_CRU_PLL_REFDIV_MASK		(0x3f << 0)
#define  RK3399_CRU_PLL_REFDIV_SHIFT		0
#define  RK3399_CRU_PLL_PLL_WORK_MODE_MASK	(0x3 << 8)
#define  RK3399_CRU_PLL_PLL_WORK_MODE_SLOW	(0x0 << 8)
#define  RK3399_CRU_PLL_PLL_WORK_MODE_NORMAL	(0x1 << 8)
#define  RK3399_CRU_PLL_PLL_WORK_MODE_DEEP_SLOW	(0x2 << 8)
#define  RK3399_CRU_PLL_PLL_LOCK		(1U << 31)
#define RK3399_CRU_CLKSEL_CON(i)	(0x0100 + (i) * 4)
#define  RK3399_CRU_ACLKM_CORE_DIV_CON_MASK	(0x1f << 8)
#define  RK3399_CRU_ACLKM_CORE_DIV_CON_SHIFT	8
#define  RK3399_CRU_CORE_PLL_SEL_MASK		(0x3 << 6)
#define  RK3399_CRU_CORE_PLL_SEL_SHIFT		6
#define  RK3399_CRU_CLK_CORE_DIV_CON_MASK	(0x1f << 0)
#define  RK3399_CRU_CLK_CORE_DIV_CON_SHIFT	0
#define  RK3399_CRU_PCLK_DBG_DIV_CON_MASK	(0x1f << 8)
#define  RK3399_CRU_PCLK_DBG_DIV_CON_SHIFT	8
#define  RK3399_CRU_ATCLK_CORE_DIV_CON_MASK	(0x1f << 0)
#define  RK3399_CRU_ATCLK_CORE_DIV_CON_SHIFT	0
#define RK3399_CRU_CLKGATE_CON(i)	(0x0300 + (i) * 4)
#define RK3399_CRU_SOFTRST_CON(i)	(0x0400 + (i) * 4)
#define RK3399_CRU_SDMMC_CON(i)		(0x0580 + (i) * 4)

#define RK3399_PMUCRU_PPLL_CON(i)	(0x0000 + (i) * 4)
#define RK3399_PMUCRU_CLKSEL_CON(i)	(0x0080 + (i) * 4)

#include "rkclock_clocks.h"

#define HREAD4(sc, reg)							\
	(bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg)))
#define HWRITE4(sc, reg, val)						\
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))
#define HSET4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) | (bits))
#define HCLR4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) & ~(bits))

struct rkclock_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;

	struct clock_device	sc_cd;
	struct reset_device	sc_rd;
};

int rkclock_match(struct device *, void *, void *);
void rkclock_attach(struct device *, struct device *, void *);

struct cfattach	rkclock_ca = {
	sizeof (struct rkclock_softc), rkclock_match, rkclock_attach
};

struct cfdriver rkclock_cd = {
	NULL, "rkclock", DV_DULL
};

struct rkclock_softc *rkclock_cpuspeed_sc;

uint32_t rk3288_get_frequency(void *, uint32_t *);
int	rk3288_set_frequency(void *, uint32_t *, uint32_t);
void	rk3288_enable(void *, uint32_t *, int);
void	rk3288_reset(void *, uint32_t *, int);

void	rk3399_init(struct rkclock_softc *);
uint32_t rk3399_get_frequency(void *, uint32_t *);
int	rk3399_set_frequency(void *, uint32_t *, uint32_t);
void	rk3399_enable(void *, uint32_t *, int);
void	rk3399_reset(void *, uint32_t *, int);
int	rk3399_cpuspeed(int *);

void	rk3399_pmu_init(struct rkclock_softc *);
uint32_t rk3399_pmu_get_frequency(void *, uint32_t *);
int	rk3399_pmu_set_frequency(void *, uint32_t *, uint32_t);
void	rk3399_pmu_enable(void *, uint32_t *, int);
void	rk3399_pmu_reset(void *, uint32_t *, int);

struct rkclock_compat {
	const char *compat;
	void	(*init)(struct rkclock_softc *);
	void	(*enable)(void *, uint32_t *, int);
	uint32_t (*get_frequency)(void *, uint32_t *);
	int	(*set_frequency)(void *, uint32_t *, uint32_t);
	void	(*reset)(void *, uint32_t *, int);
};

struct rkclock_compat rkclock_compat[] = {
	{
		"rockchip,rk3288-cru", NULL,
		rk3288_enable, rk3288_get_frequency,
		rk3288_set_frequency, rk3288_reset
	},
	{
		"rockchip,rk3399-cru", rk3399_init,
		rk3399_enable, rk3399_get_frequency,
		rk3399_set_frequency, rk3399_reset,
	},
	{
		"rockchip,rk3399-pmucru", rk3399_pmu_init,
		rk3399_pmu_enable, rk3399_pmu_get_frequency,
		rk3399_pmu_set_frequency, rk3399_pmu_reset
	}
};
	
int
rkclock_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;
	int i;

	for (i = 0; i < nitems(rkclock_compat); i++) {
		if (OF_is_compatible(faa->fa_node, rkclock_compat[i].compat))
			return 1;
	}

	return 0;
}

void
rkclock_attach(struct device *parent, struct device *self, void *aux)
{
	struct rkclock_softc *sc = (struct rkclock_softc *)self;
	struct fdt_attach_args *faa = aux;
	int i;

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

	printf("\n");

	for (i = 0; i < nitems(rkclock_compat); i++) {
		if (OF_is_compatible(faa->fa_node, rkclock_compat[i].compat)) {
			break;
		}
	}
	KASSERT(i < nitems(rkclock_compat));

	if (rkclock_compat[i].init)
		rkclock_compat[i].init(sc);

	sc->sc_cd.cd_node = faa->fa_node;
	sc->sc_cd.cd_cookie = sc;
	sc->sc_cd.cd_enable = rkclock_compat[i].enable;
	sc->sc_cd.cd_get_frequency = rkclock_compat[i].get_frequency;
	sc->sc_cd.cd_set_frequency = rkclock_compat[i].set_frequency;
	clock_register(&sc->sc_cd);

	sc->sc_rd.rd_node = faa->fa_node;
	sc->sc_rd.rd_cookie = sc;
	sc->sc_rd.rd_reset = rkclock_compat[i].reset;
	reset_register(&sc->sc_rd);
}

/*
 * Rockchip RK3288
 */

uint32_t
rk3288_get_pll(struct rkclock_softc *sc, bus_size_t base)
{
	uint32_t clkod, clkr, clkf;
	uint32_t reg;

	reg = HREAD4(sc, base);
	clkod = (reg >> 0) & 0xf;
	clkr = (reg >> 8) & 0x3f;
	reg = HREAD4(sc, base + 4);
	clkf = (reg >> 0) & 0x1fff;
	return 24000000ULL * (clkf + 1) / (clkr + 1) / (clkod + 1);
}

uint32_t
rk3288_get_frequency(void *cookie, uint32_t *cells)
{
	struct rkclock_softc *sc = cookie;
	uint32_t idx = cells[0];
	uint32_t reg, mux, div_con;

	switch (idx) {
	case RK3288_PLL_CPLL:
		return rk3288_get_pll(sc, RK3288_CRU_CPLL_CON(0));
	case RK3288_PLL_GPLL:
		return rk3288_get_pll(sc, RK3288_CRU_GPLL_CON(0));
	case RK3288_CLK_SDMMC:
		reg = HREAD4(sc, RK3288_CRU_CLKSEL_CON(11));
		mux = (reg >> 6) & 0x3;
		div_con = reg & 0x3f;
		switch (mux) {
		case 0:
			idx = RK3288_PLL_CPLL;
			break;
		case 1:
			idx = RK3288_PLL_GPLL;
			break;
		case 2:
			return 24000000 / (div_con + 1);
		default:
			return 0;
		}
		return rk3288_get_frequency(sc, &idx) / (div_con + 1);
		break;
	case RK3288_CLK_UART0:
		reg = HREAD4(sc, RK3288_CRU_CLKSEL_CON(13));
		mux = (reg >> 8) & 0x3;
		div_con = reg & 0x7f;
		if (mux == 2)
			return 24000000 / (div_con + 1);
		break;
	case RK3288_CLK_UART1:
		reg = HREAD4(sc, RK3288_CRU_CLKSEL_CON(14));
		mux = (reg >> 8) & 0x3;
		div_con = reg & 0x7f;
		if (mux == 2)
			return 24000000 / (div_con + 1);
		break;
	case RK3288_CLK_UART2:
		reg = HREAD4(sc, RK3288_CRU_CLKSEL_CON(15));
		mux = (reg >> 8) & 0x3;
		div_con = reg & 0x7f;
		if (mux == 2)
			return 24000000 / (div_con + 1);
		break;
	case RK3288_CLK_UART3:
		reg = HREAD4(sc, RK3288_CRU_CLKSEL_CON(16));
		mux = (reg >> 8) & 0x3;
		div_con = reg & 0x7f;
		if (mux == 2)
			return 24000000 / (div_con + 1);
		break;
	case RK3288_CLK_UART4:
		reg = HREAD4(sc, RK3288_CRU_CLKSEL_CON(3));
		mux = (reg >> 8) & 0x3;
		div_con = reg & 0x7f;
		if (mux == 2)
			return 24000000 / (div_con + 1);
		break;
	default:
		break;
	}

	printf("%s: 0x%08x\n", __func__, idx);
	return 0;
}

int
rk3288_set_frequency(void *cookie, uint32_t *cells, uint32_t freq)
{
	uint32_t idx = cells[0];

	printf("%s: 0x%08x\n", __func__, idx);
	return -1;
}

void
rk3288_enable(void *cookie, uint32_t *cells, int on)
{
	uint32_t idx = cells[0];

	switch (idx) {
	case RK3288_CLK_SDMMC:
	case RK3288_CLK_UART0:
	case RK3288_CLK_UART1:
	case RK3288_CLK_UART2:
	case RK3288_CLK_UART3:
	case RK3288_CLK_UART4:
	case RK3288_CLK_MAC_RX:
	case RK3288_CLK_MAC_TX:
	case RK3288_CLK_SDMMC_DRV:
	case RK3288_CLK_SDMMC_SAMPLE:
	case RK3288_CLK_MAC:
	case RK3288_ACLK_GMAC:
	case RK3288_PCLK_GMAC:
	case RK3288_HCLK_HOST0:
	case RK3288_HCLK_SDMMC:
		/* Enabled by default. */
		break;
	default:
		printf("%s: 0x%08x\n", __func__, idx);
		break;
	}
}

void
rk3288_reset(void *cookie, uint32_t *cells, int on)
{
	uint32_t idx = cells[0];

	printf("%s: 0x%08x\n", __func__, idx);
}

/* 
 * Rockchip RK3399 
 */

/* Some of our parent clocks live in the PMUCRU. */
struct rkclock_softc *rk3399_pmucru_sc;

void
rk3399_init(struct rkclock_softc *sc)
{
	int node;

	rkclock_cpuspeed_sc = sc;
	cpu_cpuspeed = rk3399_cpuspeed;

	/* PMUCRU instance should attach before us. */
	KASSERT(rk3399_pmucru_sc != NULL);

	/*
	 * Since the hardware comes up with a really conservative CPU
	 * clock frequency, and U-Boot doesn't set it to a more
	 * reasonable default, try to do so here.  These defaults were
	 * chosen assuming that the voltage for both clusters is at
	 * least 1.0 V.  Only do this on the Firefly-RK3399 for now
	 * where this is likely to be true given the default voltages
	 * for the regulators on that board.
	 */
	node = OF_finddevice("/");
	if (OF_is_compatible(node, "firefly,firefly-rk3399")) {
		uint32_t idx;
		
		/* Run the "LITTLE" cluster at 1.2 GHz. */
		idx = RK3399_ARMCLKL;
		rk3399_set_frequency(sc, &idx, 1200000000);

#ifdef MULTIPROCESSOR
		/* Switch PLL of the "big" cluster into normal mode. */
		HWRITE4(sc, RK3399_CRU_BPLL_CON(3),
		    RK3399_CRU_PLL_PLL_WORK_MODE_MASK << 16 |
		    RK3399_CRU_PLL_PLL_WORK_MODE_NORMAL);
#endif
	}
}

uint32_t
rk3399_get_pll(struct rkclock_softc *sc, bus_size_t base)
{
	uint32_t fbdiv, postdiv1, postdiv2, refdiv;
	uint32_t pll_work_mode;
	uint32_t reg;

	reg = HREAD4(sc, base + 0x000c);
	pll_work_mode = reg & RK3399_CRU_PLL_PLL_WORK_MODE_MASK;
	if (pll_work_mode == RK3399_CRU_PLL_PLL_WORK_MODE_SLOW)
		return 24000000;
	if (pll_work_mode == RK3399_CRU_PLL_PLL_WORK_MODE_DEEP_SLOW)
		return 32768;

	reg = HREAD4(sc, base + 0x0000);
	fbdiv = (reg & RK3399_CRU_PLL_FBDIV_MASK) >>
	    RK3399_CRU_PLL_FBDIV_SHIFT;
	reg = HREAD4(sc, base + 0x0004);
	postdiv2 = (reg & RK3399_CRU_PLL_POSTDIV2_MASK) >>
	    RK3399_CRU_PLL_POSTDIV2_SHIFT;
	postdiv1 = (reg & RK3399_CRU_PLL_POSTDIV1_MASK) >>
	    RK3399_CRU_PLL_POSTDIV1_SHIFT;
	refdiv = (reg & RK3399_CRU_PLL_REFDIV_MASK) >>
	    RK3399_CRU_PLL_REFDIV_SHIFT;
	return 24000000ULL * fbdiv / refdiv / postdiv1 / postdiv2;
}

int
rk3399_set_pll(struct rkclock_softc *sc, bus_size_t base, uint32_t freq)
{
	uint32_t fbdiv, postdiv1, postdiv2, refdiv;

	/*
	 * It is not clear whether all combinations of the clock
	 * dividers result in a stable clock.  Therefore this function
	 * only supports a limited set of PLL clock rates.  For now
	 * this set covers all the CPU frequencies supported by the
	 * Linux kernel.
	 */
	switch (freq) {
	case 2208000000U:
	case 2184000000U:
	case 2088000000U:
	case 2040000000U:
	case 2016000000U:
	case 1992000000U:
	case 1896000000U:
	case 1800000000U:
	case 1704000000U:
	case 1608000000U:
	case 1512000000U:
	case 1488000000U:
	case 1416000000U:
	case 1200000000U:
		postdiv1 = postdiv2 = refdiv = 1;
		break;
	case 1008000000U:
	case 816000000U:
	case 696000000U:
		postdiv1 = 2; postdiv2 = refdiv = 1;
		break;
	case 600000000U:
		postdiv1 = 3; postdiv2 = refdiv = 1;
		break;
	case 408000000U:
		postdiv1 = postdiv2 = 2; refdiv = 1;
		break;
	case 216000000U:
		postdiv1 = 4; postdiv2 = 2; refdiv = 1;
		break;
	case 96000000U:
		postdiv1 = postdiv2 = 4; refdiv = 1;
		break;
	default:
		printf("%s: %d MHz\n", __func__, freq);
		return -1;
	}

	/* Calculate feedback divider. */
	fbdiv = freq * postdiv1 * postdiv2 * refdiv / 24000000;

	/*
	 * Select slow mode to guarantee a stable clock while we're
	 * adjusting the PLL.
	 */
	HWRITE4(sc, base + 0x000c,
	    RK3399_CRU_PLL_PLL_WORK_MODE_MASK << 16 |
	    RK3399_CRU_PLL_PLL_WORK_MODE_SLOW);

	/* Set PLL rate. */
	HWRITE4(sc, base + 0x0000,
	    RK3399_CRU_PLL_FBDIV_MASK << 16 |
	    fbdiv << RK3399_CRU_PLL_FBDIV_SHIFT);
	HWRITE4(sc, base + 0x0004,
	    RK3399_CRU_PLL_POSTDIV2_MASK << 16 |
	    postdiv2 << RK3399_CRU_PLL_POSTDIV2_SHIFT |
	    RK3399_CRU_PLL_POSTDIV1_MASK << 16 |
	    postdiv1 << RK3399_CRU_PLL_POSTDIV1_SHIFT |
	    RK3399_CRU_PLL_REFDIV_MASK << 16 |
	    refdiv << RK3399_CRU_PLL_REFDIV_SHIFT);

	/* Wait for PLL to stabilize. */
	while ((HREAD4(sc, base + 0x0008) & RK3399_CRU_PLL_PLL_LOCK) == 0)
		delay(10);

	/* Switch back to normal mode. */
	HWRITE4(sc, base + 0x000c,
	    RK3399_CRU_PLL_PLL_WORK_MODE_MASK << 16 |
	    RK3399_CRU_PLL_PLL_WORK_MODE_NORMAL);

	return 0;
}

uint32_t
rk3399_armclk_parent(uint32_t mux)
{
	switch (mux) {
	case 0:
		return RK3399_PLL_ALPLL;
	case 1:
		return RK3399_PLL_ABPLL;
	case 2:
		return RK3399_PLL_DPLL;
	case 3:
		return RK3399_PLL_GPLL;
	}

	return 0;
}

uint32_t
rk3399_get_armclk(struct rkclock_softc *sc, bus_size_t clksel)
{
	uint32_t reg, mux, div_con;
	uint32_t idx;

	reg = HREAD4(sc, clksel);
	mux = (reg & RK3399_CRU_CORE_PLL_SEL_MASK) >>
	    RK3399_CRU_CORE_PLL_SEL_SHIFT;
	div_con = (reg & RK3399_CRU_CLK_CORE_DIV_CON_MASK) >>
	    RK3399_CRU_CLK_CORE_DIV_CON_SHIFT;
	idx = rk3399_armclk_parent(mux);
	
	return rk3399_get_frequency(sc, &idx) / (div_con + 1);
}

int
rk3399_set_armclk(struct rkclock_softc *sc, bus_size_t clksel, uint32_t freq)
{
	uint32_t reg, mux;
	uint32_t old_freq, div;
	uint32_t idx;

	old_freq = rk3399_get_armclk(sc, clksel);
	if (freq == old_freq)
		return 0;

	reg = HREAD4(sc, clksel);
	mux = (reg & RK3399_CRU_CORE_PLL_SEL_MASK) >>
	    RK3399_CRU_CORE_PLL_SEL_SHIFT;
	idx = rk3399_armclk_parent(mux);

	/* Keep the atclk_core and pclk_dbg clocks at or below 200 MHz. */
	div = 1;
	while (freq / (div + 1) > 200000000)
		div++;

	/* When ramping up, set clock dividers first. */
	if (freq > old_freq) {
		HWRITE4(sc, RK3399_CRU_CLKSEL_CON(0),
		    RK3399_CRU_CLK_CORE_DIV_CON_MASK << 16 |
		    0 << RK3399_CRU_CLK_CORE_DIV_CON_SHIFT |
		    RK3399_CRU_ACLKM_CORE_DIV_CON_MASK << 16 |
		    1 << RK3399_CRU_ACLKM_CORE_DIV_CON_SHIFT);
		HWRITE4(sc, RK3399_CRU_CLKSEL_CON(1),
		    RK3399_CRU_PCLK_DBG_DIV_CON_MASK << 16 |
		    div << RK3399_CRU_PCLK_DBG_DIV_CON_SHIFT |
		    RK3399_CRU_ATCLK_CORE_DIV_CON_MASK << 16 |
		    div << RK3399_CRU_ATCLK_CORE_DIV_CON_SHIFT);
	}

	rk3399_set_frequency(sc, &idx, freq);

	/* When ramping dowm, set clock dividers last. */
	if (freq < old_freq) {
		HWRITE4(sc, RK3399_CRU_CLKSEL_CON(0),
		    RK3399_CRU_CLK_CORE_DIV_CON_MASK << 16 |
		    0 << RK3399_CRU_CLK_CORE_DIV_CON_SHIFT |
		    RK3399_CRU_ACLKM_CORE_DIV_CON_MASK << 16 |
		    1 << RK3399_CRU_ACLKM_CORE_DIV_CON_SHIFT);
		HWRITE4(sc, RK3399_CRU_CLKSEL_CON(1),
		    RK3399_CRU_PCLK_DBG_DIV_CON_MASK << 16 |
		    div << RK3399_CRU_PCLK_DBG_DIV_CON_SHIFT |
		    RK3399_CRU_ATCLK_CORE_DIV_CON_MASK << 16 |
		    div << RK3399_CRU_ATCLK_CORE_DIV_CON_SHIFT);
	}
	
	return 0;
}

uint32_t
rk3399_get_i2c(struct rkclock_softc *sc, size_t base, int shift)
{
	uint32_t reg, mux, div_con;
	uint32_t idx, freq;

	reg = HREAD4(sc, base);
	mux = (reg >> (7 + shift)) & 0x1;
	div_con = (reg >> shift) & 0x7f;
	if (mux == 1) {
		idx = RK3399_PLL_PPLL;
		freq = rk3399_pmu_get_frequency(rk3399_pmucru_sc, &idx);
	} else {
		idx = RK3399_PLL_CPLL;
		freq = rk3399_get_frequency(sc, &idx);
	}

	return freq / (div_con + 1);
}

uint32_t
rk3399_get_frequency(void *cookie, uint32_t *cells)
{
	struct rkclock_softc *sc = cookie;
	uint32_t idx = cells[0];
	uint32_t reg, mux, div_con;

	switch (idx) {
	case RK3399_PLL_ALPLL:
		return rk3399_get_pll(sc, RK3399_CRU_LPLL_CON(0));
	case RK3399_PLL_ABPLL:
		return rk3399_get_pll(sc, RK3399_CRU_BPLL_CON(0));
	case RK3399_PLL_DPLL:
		return rk3399_get_pll(sc, RK3399_CRU_DPLL_CON(0));
	case RK3399_PLL_CPLL:
		return rk3399_get_pll(sc, RK3399_CRU_CPLL_CON(0));
	case RK3399_PLL_GPLL:
		return rk3399_get_pll(sc, RK3399_CRU_GPLL_CON(0));
	case RK3399_PLL_NPLL:
		return rk3399_get_pll(sc, RK3399_CRU_NPLL_CON(0));
	case RK3399_ARMCLKL:
		return rk3399_get_armclk(sc, RK3399_CRU_CLKSEL_CON(0));
	case RK3399_ARMCLKB:
		return rk3399_get_armclk(sc, RK3399_CRU_CLKSEL_CON(2));
	case RK3399_CLK_I2C1:
		return rk3399_get_i2c(sc, RK3399_CRU_CLKSEL_CON(61), 0);
	case RK3399_CLK_I2C2:
		return rk3399_get_i2c(sc, RK3399_CRU_CLKSEL_CON(62), 0);
	case RK3399_CLK_I2C3:
		return rk3399_get_i2c(sc, RK3399_CRU_CLKSEL_CON(63), 0);
	case RK3399_CLK_I2C5:
		return rk3399_get_i2c(sc, RK3399_CRU_CLKSEL_CON(61), 8);
	case RK3399_CLK_I2C6:
		return rk3399_get_i2c(sc, RK3399_CRU_CLKSEL_CON(61), 8);
	case RK3399_CLK_I2C7:
		return rk3399_get_i2c(sc, RK3399_CRU_CLKSEL_CON(61), 8);
	case RK3399_CLK_SDMMC:
		reg = HREAD4(sc, RK3399_CRU_CLKSEL_CON(16));
		mux = (reg >> 8) & 0x7;
		div_con = reg & 0x7f;
		switch (mux) {
		case 0:
			idx = RK3399_PLL_CPLL;
			break;
		case 1:
			idx = RK3399_PLL_GPLL;
			break;
		case 2:
			idx = RK3399_PLL_NPLL;
			break;
#ifdef notyet
		case 3:
			idx = RK3399_PLL_PPLL;
			break;
		case 4:
			idx = RK3399_USB_480M;
			break;
#endif
		case 5:
			return 24000000 / (div_con + 1);
		default:
			return 0;
		}
		return rk3399_get_frequency(sc, &idx) / (div_con + 1);
		break;
	case RK3399_CLK_UART0:
		reg = HREAD4(sc, RK3399_CRU_CLKSEL_CON(33));
		mux = (reg >> 8) & 0x3;
		div_con = reg & 0x7f;
		if (mux == 2)
			return 24000000 / (div_con + 1);
		break;
	case RK3399_CLK_UART1:
		reg = HREAD4(sc, RK3399_CRU_CLKSEL_CON(34));
		mux = (reg >> 8) & 0x3;
		div_con = reg & 0x7f;
		if (mux == 2)
			return 24000000 / (div_con + 1);
		break;
	case RK3399_CLK_UART2:
		reg = HREAD4(sc, RK3399_CRU_CLKSEL_CON(35));
		mux = (reg >> 8) & 0x3;
		div_con = reg & 0x7f;
		if (mux == 2)
			return 24000000 / (div_con + 1);
		break;
	case RK3399_CLK_UART3:
		reg = HREAD4(sc, RK3399_CRU_CLKSEL_CON(36));
		mux = (reg >> 8) & 0x3;
		div_con = reg & 0x7f;
		if (mux == 2)
			return 24000000 / (div_con + 1);
		break;
	case RK3399_HCLK_SDMMC:
		reg = HREAD4(sc, RK3399_CRU_CLKSEL_CON(13));
		mux = (reg >> 15) & 0x1;
		div_con = (reg >> 8) & 0x1f;
		idx = mux ? RK3399_PLL_CPLL : RK3399_PLL_GPLL;
		return rk3399_get_frequency(sc, &idx) / (div_con + 1);
	default:
		break;
	}

	printf("%s: 0x%08x\n", __func__, idx);
	return 0;
}

int
rk3399_set_frequency(void *cookie, uint32_t *cells, uint32_t freq)
{
	struct rkclock_softc *sc = cookie;
	uint32_t idx = cells[0];

	switch (idx) {
	case RK3399_PLL_ALPLL:
		return rk3399_set_pll(sc, RK3399_CRU_LPLL_CON(0), freq);
	case RK3399_PLL_ABPLL:
		return rk3399_set_pll(sc, RK3399_CRU_BPLL_CON(0), freq);
	case RK3399_ARMCLKL:
		return rk3399_set_armclk(sc, RK3399_CRU_CLKSEL_CON(0), freq);
	case RK3399_ARMCLKB:
		return rk3399_set_armclk(sc, RK3399_CRU_CLKSEL_CON(2), freq);
	}

	printf("%s: 0x%08x\n", __func__, idx);
	return -1;
}

void
rk3399_enable(void *cookie, uint32_t *cells, int on)
{
	uint32_t idx = cells[0];

	switch (idx) {
	case RK3399_CLK_I2C1:
	case RK3399_CLK_I2C2:
	case RK3399_CLK_I2C3:
	case RK3399_CLK_I2C5:
	case RK3399_CLK_I2C6:
	case RK3399_CLK_I2C7:
	case RK3399_CLK_SDMMC:
	case RK3399_CLK_EMMC:
	case RK3399_CLK_TSADC:
	case RK3399_CLK_UART0:
	case RK3399_CLK_UART1:
	case RK3399_CLK_UART2:
	case RK3399_CLK_UART3:
	case RK3399_CLK_MAC_RX:
	case RK3399_CLK_MAC_TX:
	case RK3399_CLK_MAC:
	case RK3399_CLK_USB3OTG0_REF:
	case RK3399_CLK_USB3OTG1_REF:
	case RK3399_CLK_USB3OTG0_SUSPEND:
	case RK3399_CLK_USB3OTG1_SUSPEND:
	case RK3399_CLK_SDMMC_DRV:
	case RK3399_CLK_SDMMC_SAMPLE:
	case RK3399_ACLK_EMMC:
	case RK3399_ACLK_GMAC:
	case RK3399_ACLK_USB3OTG0:
	case RK3399_ACLK_USB3OTG1:
	case RK3399_ACLK_USB3_GRF:
	case RK3399_PCLK_I2C1:
	case RK3399_PCLK_I2C2:
	case RK3399_PCLK_I2C3:
	case RK3399_PCLK_I2C5:
	case RK3399_PCLK_I2C6:
	case RK3399_PCLK_I2C7:
	case RK3399_PCLK_TSADC:
	case RK3399_PCLK_GMAC:
	case RK3399_HCLK_HOST0:
	case RK3399_HCLK_HOST0_ARB:
	case RK3399_HCLK_HOST1:
	case RK3399_HCLK_HOST1_ARB:
	case RK3399_HCLK_SDMMC:
		/* Enabled by default. */
		break;
	default:
		printf("%s: 0x%08x\n", __func__, idx);
		break;
	}
}

void
rk3399_reset(void *cookie, uint32_t *cells, int on)
{
	struct rkclock_softc *sc = cookie;
	uint32_t idx = cells[0];
	uint32_t mask = (1 << (idx % 16));

	HWRITE4(sc, RK3399_CRU_SOFTRST_CON(idx / 16),
	    mask << 16 | (on ? mask : 0));
}

int
rk3399_cpuspeed(int *freq)
{
	uint32_t idx = RK3399_ARMCLKL;

	*freq = rk3399_get_frequency(rkclock_cpuspeed_sc, &idx) / 1000000;
	return 0;
}

/* PMUCRU */

void
rk3399_pmu_init(struct rkclock_softc *sc)
{
	rk3399_pmucru_sc = sc;
}

uint32_t
rk3399_pmu_get_i2c(struct rkclock_softc *sc, size_t base, int shift)
{
	uint32_t reg, div_con;
	uint32_t idx;

	reg = HREAD4(sc, base);
	div_con = (reg >> shift) & 0x7f;
	idx = RK3399_PLL_PPLL;
	return rk3399_get_frequency(sc, &idx) / (div_con + 1);
}

uint32_t
rk3399_pmu_get_frequency(void *cookie, uint32_t *cells)
{
	struct rkclock_softc *sc = cookie;
	uint32_t idx = cells[0];

	switch (idx) {
	case RK3399_PLL_PPLL:
		return rk3399_get_pll(sc, RK3399_PMUCRU_PPLL_CON(0));
	case RK3399_CLK_I2C0:
		return rk3399_pmu_get_i2c(sc, RK3399_PMUCRU_CLKSEL_CON(2), 0);
	case RK3399_CLK_I2C4:
		return rk3399_pmu_get_i2c(sc, RK3399_PMUCRU_CLKSEL_CON(3), 0);
	case RK3399_CLK_I2C8:
		return rk3399_pmu_get_i2c(sc, RK3399_PMUCRU_CLKSEL_CON(2), 8);
	default:
		break;
	}

	printf("%s: 0x%08x\n", __func__, idx);
	return 0;
}

int
rk3399_pmu_set_frequency(void *cookie, uint32_t *cells, uint32_t freq)
{
	uint32_t idx = cells[0];

	printf("%s: 0x%08x\n", __func__, idx);
	return -1;
}

void
rk3399_pmu_enable(void *cookie, uint32_t *cells, int on)
{
	uint32_t idx = cells[0];

	switch (idx) {
	case RK3399_CLK_I2C0:
	case RK3399_CLK_I2C4:
	case RK3399_CLK_I2C8:
	case RK3399_PCLK_I2C0:
	case RK3399_PCLK_I2C4:
	case RK3399_PCLK_I2C8:
		/* Enabled by default. */
		break;
	default:
		printf("%s: 0x%08x\n", __func__, idx);
		break;
	}
}

void
rk3399_pmu_reset(void *cookie, uint32_t *cells, int on)
{
	uint32_t idx = cells[0];

	printf("%s: 0x%08x\n", __func__, idx);
}
