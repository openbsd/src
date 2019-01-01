/*	$OpenBSD: rkclock.c,v 1.41 2019/01/01 17:12:58 kettenis Exp $	*/
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
#include <sys/sysctl.h>
#include <sys/device.h>

#include <machine/intr.h>
#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_clock.h>
#include <dev/ofw/fdt.h>

/* RK3288 registers */
#define RK3288_CRU_APLL_CON(i)		(0x0000 + (i) * 4)
#define RK3288_CRU_CPLL_CON(i)		(0x0020 + (i) * 4)
#define RK3288_CRU_GPLL_CON(i)		(0x0030 + (i) * 4)
#define  RK3288_CRU_PLL_CLKR_MASK		(0x3f << 8)
#define  RK3288_CRU_PLL_CLKR_SHIFT		8
#define  RK3288_CRU_PLL_CLKOD_MASK		(0xf << 0)
#define  RK3288_CRU_PLL_CLKOD_SHIFT		0
#define  RK3288_CRU_PLL_CLKF_MASK		(0x1fff << 0)
#define  RK3288_CRU_PLL_CLKF_SHIFT		0
#define  RK3288_CRU_PLL_RESET			(1 << 5)
#define RK3288_CRU_MODE_CON		0x0050
#define  RK3288_CRU_MODE_PLL_WORK_MODE_MASK	0x3
#define  RK3288_CRU_MODE_PLL_WORK_MODE_SLOW	0x0
#define  RK3288_CRU_MODE_PLL_WORK_MODE_NORMAL	0x1
#define RK3288_CRU_CLKSEL_CON(i)	(0x0060 + (i) * 4)
#define RK3288_CRU_SOFTRST_CON(i)	(0x01b8 + (i) * 4)

/* RK3328 registers */
#define RK3328_CRU_APLL_CON(i)		(0x0000 + (i) * 4)
#define RK3328_CRU_DPLL_CON(i)		(0x0020 + (i) * 4)
#define RK3328_CRU_CPLL_CON(i)		(0x0040 + (i) * 4)
#define RK3328_CRU_GPLL_CON(i)		(0x0060 + (i) * 4)
#define RK3328_CRU_NPLL_CON(i)		(0x00a0 + (i) * 4)
#define  RK3328_CRU_PLL_POSTDIV1_MASK		(0x7 << 12)
#define  RK3328_CRU_PLL_POSTDIV1_SHIFT		12
#define  RK3328_CRU_PLL_FBDIV_MASK		(0xfff << 0)
#define  RK3328_CRU_PLL_FBDIV_SHIFT		0
#define  RK3328_CRU_PLL_DSMPD			(1 << 12)
#define  RK3328_CRU_PLL_PLL_LOCK		(1 << 10)
#define  RK3328_CRU_PLL_POSTDIV2_MASK		(0x7 << 6)
#define  RK3328_CRU_PLL_POSTDIV2_SHIFT		6
#define  RK3328_CRU_PLL_REFDIV_MASK		(0x3f << 0)
#define  RK3328_CRU_PLL_REFDIV_SHIFT		0
#define  RK3328_CRU_PLL_FRACDIV_MASK		(0xffffff << 0)
#define  RK3328_CRU_PLL_FRACDIV_SHIFT		0
#define RK3328_CRU_CRU_MODE		0x0080
#define  RK3328_CRU_CRU_MODE_MASK		0x1
#define  RK3328_CRU_CRU_MODE_SLOW		0x0
#define  RK3328_CRU_CRU_MODE_NORMAL		0x1
#define RK3328_CRU_CLKSEL_CON(i)	(0x0100 + (i) * 4)
#define  RK3328_CRU_CORE_CLK_PLL_SEL_MASK	(0x3 << 6)
#define  RK3328_CRU_CORE_CLK_PLL_SEL_SHIFT	6
#define  RK3328_CRU_CLK_CORE_DIV_CON_MASK	(0x1f << 0)
#define  RK3328_CRU_CLK_CORE_DIV_CON_SHIFT	0
#define  RK3328_CRU_ACLK_CORE_DIV_CON_MASK	(0x7 << 4)
#define  RK3328_CRU_ACLK_CORE_DIV_CON_SHIFT	4
#define  RK3328_CRU_CLK_CORE_DBG_DIV_CON_MASK	(0xf << 0)
#define  RK3328_CRU_CLK_CORE_DBG_DIV_CON_SHIFT	0
#define  RK3328_CRU_VOP_DCLK_SRC_SEL_MASK	(0x1 << 1)
#define  RK3328_CRU_VOP_DCLK_SRC_SEL_SHIFT	1
#define RK3328_CRU_CLKGATE_CON(i)	(0x0200 + (i) * 4)
#define RK3328_CRU_SOFTRST_CON(i)	(0x0300 + (i) * 4)

/* RK3399 registers */
#define RK3399_CRU_LPLL_CON(i)		(0x0000 + (i) * 4)
#define RK3399_CRU_BPLL_CON(i)		(0x0020 + (i) * 4)
#define RK3399_CRU_DPLL_CON(i)		(0x0020 + (i) * 4)
#define RK3399_CRU_CPLL_CON(i)		(0x0060 + (i) * 4)
#define RK3399_CRU_GPLL_CON(i)		(0x0080 + (i) * 4)
#define RK3399_CRU_NPLL_CON(i)		(0x00a0 + (i) * 4)
#define RK3399_CRU_VPLL_CON(i)		(0x00c0 + (i) * 4)
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
#define  RK3399_CRU_CORE_PLL_SEL_APLL		(0x0 << 6)
#define  RK3399_CRU_CORE_PLL_SEL_BPLL		(0x1 << 6)
#define  RK3399_CRU_CORE_PLL_SEL_DPLL		(0x2 << 6)
#define  RK3399_CRU_CORE_PLL_SEL_GPLL		(0x3 << 6)
#define  RK3399_CRU_CORE_PLL_SEL_SHIFT		6
#define  RK3399_CRU_CLK_CORE_DIV_CON_MASK	(0x1f << 0)
#define  RK3399_CRU_CLK_CORE_DIV_CON_SHIFT	0
#define  RK3399_CRU_PCLK_DBG_DIV_CON_MASK	(0x1f << 8)
#define  RK3399_CRU_PCLK_DBG_DIV_CON_SHIFT	8
#define  RK3399_CRU_ATCLK_CORE_DIV_CON_MASK	(0x1f << 0)
#define  RK3399_CRU_ATCLK_CORE_DIV_CON_SHIFT	0
#define  RK3399_CRU_CLK_SD_PLL_SEL_MASK		(0x7 << 8)
#define  RK3399_CRU_CLK_SD_PLL_SEL_SHIFT	8
#define  RK3399_CRU_CLK_SD_DIV_CON_MASK		(0x7f << 0)
#define  RK3399_CRU_CLK_SD_DIV_CON_SHIFT	0
#define RK3399_CRU_CLKGATE_CON(i)	(0x0300 + (i) * 4)
#define RK3399_CRU_SOFTRST_CON(i)	(0x0400 + (i) * 4)
#define RK3399_CRU_SDMMC_CON(i)		(0x0580 + (i) * 4)

#define RK3399_PMUCRU_PPLL_CON(i)	(0x0000 + (i) * 4)
#define RK3399_PMUCRU_CLKSEL_CON(i)	(0x0080 + (i) * 4)

#include "rkclock_clocks.h"

struct rkclock {
	uint16_t idx;
	uint16_t reg;
	uint16_t sel_mask;
	uint16_t div_mask;
	uint16_t parents[8];
	uint32_t flags;
};

#define SEL(l, f)	(((1 << (l - f + 1)) - 1) << f)
#define DIV(l, f)	SEL(l, f)

#define FIXED_PARENT	(1 << 0)
#define SET_PARENT	(1 << 1)

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

	uint32_t		sc_phandle;
	struct rkclock		*sc_clocks;

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

void	rk3288_init(struct rkclock_softc *);
uint32_t rk3288_get_frequency(void *, uint32_t *);
int	rk3288_set_frequency(void *, uint32_t *, uint32_t);
void	rk3288_enable(void *, uint32_t *, int);
void	rk3288_reset(void *, uint32_t *, int);

void	rk3328_init(struct rkclock_softc *);
uint32_t rk3328_get_frequency(void *, uint32_t *);
int	rk3328_set_frequency(void *, uint32_t *, uint32_t);
int	rk3328_set_parent(void *, uint32_t *, uint32_t *);
void	rk3328_enable(void *, uint32_t *, int);
void	rk3328_reset(void *, uint32_t *, int);

void	rk3399_init(struct rkclock_softc *);
uint32_t rk3399_get_frequency(void *, uint32_t *);
int	rk3399_set_frequency(void *, uint32_t *, uint32_t);
void	rk3399_enable(void *, uint32_t *, int);
void	rk3399_reset(void *, uint32_t *, int);

void	rk3399_pmu_init(struct rkclock_softc *);
uint32_t rk3399_pmu_get_frequency(void *, uint32_t *);
int	rk3399_pmu_set_frequency(void *, uint32_t *, uint32_t);
void	rk3399_pmu_enable(void *, uint32_t *, int);
void	rk3399_pmu_reset(void *, uint32_t *, int);

struct rkclock_compat {
	const char *compat;
	int	assign;
	void	(*init)(struct rkclock_softc *);
	void	(*enable)(void *, uint32_t *, int);
	uint32_t (*get_frequency)(void *, uint32_t *);
	int	(*set_frequency)(void *, uint32_t *, uint32_t);
	int	(*set_parent)(void *, uint32_t *, uint32_t *);
	void	(*reset)(void *, uint32_t *, int);
};

struct rkclock_compat rkclock_compat[] = {
	{
		"rockchip,rk3288-cru", 0, rk3288_init,
		rk3288_enable, rk3288_get_frequency,
		rk3288_set_frequency, NULL,
		rk3288_reset
	},
	{
		"rockchip,rk3328-cru", 1, rk3328_init,
		rk3328_enable, rk3328_get_frequency,
		rk3328_set_frequency, rk3328_set_parent,
		rk3328_reset
	},
	{
		"rockchip,rk3399-cru", 1, rk3399_init,
		rk3399_enable, rk3399_get_frequency,
		rk3399_set_frequency, NULL,
		rk3399_reset
	},
	{
		"rockchip,rk3399-pmucru", 1, rk3399_pmu_init,
		rk3399_pmu_enable, rk3399_pmu_get_frequency,
		rk3399_pmu_set_frequency, NULL,
		rk3399_pmu_reset
	}
};
	
int
rkclock_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;
	int i;

	for (i = 0; i < nitems(rkclock_compat); i++) {
		if (OF_is_compatible(faa->fa_node, rkclock_compat[i].compat))
			return 10;
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

	sc->sc_phandle = OF_getpropint(faa->fa_node, "phandle", 0);

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
	sc->sc_cd.cd_set_parent = rkclock_compat[i].set_parent;
	clock_register(&sc->sc_cd);

	sc->sc_rd.rd_node = faa->fa_node;
	sc->sc_rd.rd_cookie = sc;
	sc->sc_rd.rd_reset = rkclock_compat[i].reset;
	reset_register(&sc->sc_rd);

	if (rkclock_compat[i].assign)
		clock_set_assigned(faa->fa_node);
}

struct rkclock *
rkclock_lookup(struct rkclock_softc *sc, uint32_t idx)
{
	struct rkclock *clk;

	for (clk = sc->sc_clocks; clk->idx; clk++) {
		if (clk->idx == idx)
			return clk;
	}

	return NULL;
}

uint32_t
rkclock_div_con(struct rkclock_softc *sc, struct rkclock *clk,
    uint32_t mux, uint32_t freq)
{
	uint32_t parent_freq, div, div_con, max_div_con;
	uint32_t idx = clk->parents[mux];

	/* Derive maximum value from mask. */
	max_div_con = clk->div_mask >> (ffs(clk->div_mask) - 1);
	
	parent_freq = sc->sc_cd.cd_get_frequency(sc, &idx);
	div = (parent_freq + freq - 1) / freq;
	div_con = (div > 0 ? div - 1 : 0);
	return (div_con < max_div_con) ? div_con : max_div_con;
}

uint32_t
rkclock_freq(struct rkclock_softc *sc, struct rkclock *clk,
    uint32_t mux, uint32_t freq)
{
	uint32_t parent_freq, div_con;
	uint32_t idx = clk->parents[mux];

	parent_freq = sc->sc_cd.cd_get_frequency(sc, &idx);
	div_con = rkclock_div_con(sc, clk, mux, freq);
	return parent_freq / (div_con + 1);
}

uint32_t
rkclock_get_frequency(struct rkclock_softc *sc, uint32_t idx)
{
	struct rkclock *clk;
	uint32_t reg, mux, div_con;
	int shift;

	clk = rkclock_lookup(sc, idx);
	if (clk == NULL) {
		printf("%s: 0x%08x\n", __func__, idx);
		return 0;
	}

	reg = HREAD4(sc, clk->reg);
	shift = ffs(clk->sel_mask) - 1;
	if (shift == -1)
		mux = 0;
	else
		mux = (reg & clk->sel_mask) >> shift;
	shift = ffs(clk->div_mask) - 1;
	if (shift == -1)
		div_con = 0;
	else
		div_con = (reg & clk->div_mask) >> shift;

	if (clk->parents[mux] == 0) {
		printf("%s: parent 0x%08x\n", __func__, idx);
		return 0;
	}
	idx = clk->parents[mux];
	return sc->sc_cd.cd_get_frequency(sc, &idx) / (div_con + 1);
}

int
rkclock_set_frequency(struct rkclock_softc *sc, uint32_t idx, uint32_t freq)
{
	struct rkclock *clk;
	uint32_t reg, mux, div_con;
	uint32_t best_freq, best_mux, f;
	int sel_shift, div_shift, i;

	clk = rkclock_lookup(sc, idx);
	if (clk == NULL || clk->div_mask == 0) {
		printf("%s: 0x%08x\n", __func__, idx);
		return -1;
	}

	reg = HREAD4(sc, clk->reg);
	sel_shift = ffs(clk->sel_mask) - 1;
	if (sel_shift == -1)
		mux = sel_shift = 0;
	else
		mux = (reg & clk->sel_mask) >> sel_shift;

	if (clk->parents[mux] == 0) {
		printf("%s: parent 0x%08x\n", __func__, idx);
		return 0;
	}

	if (clk->flags & SET_PARENT) {
		idx = clk->parents[mux];
		sc->sc_cd.cd_set_frequency(sc, &idx, freq);
	}

	/*
	 * Start out with the current parent.  This prevents
	 * unecessary switching to a different parent.
	 */
	best_freq = rkclock_freq(sc, clk, mux, freq);
	best_mux = mux;

	/*
	 * Find the parent that allows configuration of a frequency
	 * closest to the target frequency.
	 */
	if ((clk->flags & FIXED_PARENT) == 0) {
		for (i = 0; i < nitems(clk->parents); i++) {
			if (clk->parents[i] == 0)
				continue;
			f = rkclock_freq(sc, clk, i, freq);
			if ((f > best_freq && f <= freq) ||
			    (f < best_freq && f >= freq)) {
				best_freq = f;
				best_mux = i;
			}
		}
	}

	div_con = rkclock_div_con(sc, clk, best_mux, freq);
	div_shift = ffs(clk->div_mask) - 1;
	HWRITE4(sc, clk->reg,
	    clk->sel_mask << 16 | best_mux << sel_shift |
	    clk->div_mask << 16 | div_con << div_shift);
	return 0;
}

int
rkclock_set_parent(struct rkclock_softc *sc, uint32_t idx, uint32_t parent)
{
	struct rkclock *clk;
	uint32_t mux;
	int shift;

	clk = rkclock_lookup(sc, idx);
	if (clk == NULL || clk->sel_mask == 0) {
		printf("%s: 0x%08x\n", __func__, idx);
		return -1;
	}

	for (mux = 0; mux < nitems(clk->parents); mux++) {
		if (clk->parents[mux] == parent)
			break;
	}
	if (mux == nitems(clk->parents) || parent == 0) {
		printf("%s: 0x%08x parent 0x%08x\n", __func__, idx, parent);
		return -1;
	}

	shift = ffs(clk->sel_mask) - 1;
	HWRITE4(sc, clk->reg, clk->sel_mask << 16 | mux << shift);
	return 0;
}

/*
 * Rockchip RK3288
 */

void
rk3288_init(struct rkclock_softc *sc)
{
	int node;

	/*
	 * Since the hardware comes up with a really conservative CPU
	 * clock frequency, and U-Boot doesn't set it to a more
	 * reasonable default, try to do so here.  These defaults were
	 * chosen assuming that the CPU voltage is at least 1.1 V.
	 * Only do this on the Tinker-RK3288 for now where this is
	 * likely to be true given the default voltages for the
	 * regulators on that board.
	 */
	node = OF_finddevice("/");
	if (OF_is_compatible(node, "rockchip,rk3288-tinker")) {
		uint32_t idx;
		
		/* Run at 1.2 GHz. */
		idx = RK3288_ARMCLK;
		rk3288_set_frequency(sc, &idx, 1200000000);
	}
}

uint32_t
rk3288_get_pll(struct rkclock_softc *sc, bus_size_t base)
{
	uint32_t clkod, clkr, clkf;
	uint32_t reg;

	reg = HREAD4(sc, base);
	clkod = (reg & RK3288_CRU_PLL_CLKOD_MASK) >>
	    RK3288_CRU_PLL_CLKOD_SHIFT;
	clkr = (reg & RK3288_CRU_PLL_CLKR_MASK) >>
	    RK3288_CRU_PLL_CLKR_SHIFT;
	reg = HREAD4(sc, base + 4);
	clkf = (reg & RK3288_CRU_PLL_CLKF_MASK) >>
	    RK3288_CRU_PLL_CLKF_SHIFT;
	return 24000000ULL * (clkf + 1) / (clkr + 1) / (clkod + 1);
}

int
rk3288_set_pll(struct rkclock_softc *sc, bus_size_t base, uint32_t freq)
{
	int shift = 4 * (base / RK3288_CRU_CPLL_CON(0));
	uint32_t no, nr, nf;

	/*
	 * It is not clear whether all combinations of the clock
	 * dividers result in a stable clock.  Therefore this function
	 * only supports a limited set of PLL clock rates.  For now
	 * this set covers all the CPU frequencies supported by the
	 * Linux kernel.
	 */
	switch (freq) {
	case 1800000000:
	case 1704000000:
	case 1608000000:
	case 1512000000:
	case 1488000000:
	case 1416000000:
	case 1200000000:
		nr = no = 1;
		break;
	case 1008000000:
	case 816000000:
	case 696000000:
	case 600000000:
		nr = 1; no = 2;
		break;
	case 408000000:
	case 312000000:
		nr = 1; no = 4;
		break;
	case 216000000:
	case 126000000:
		nr = 1; no = 8;
		break;
	default:
		printf("%s: %u Hz\n", __func__, freq);
		return -1;
	}

	/* Calculate feedback divider. */
	nf = freq * nr * no / 24000000;

	/*
	 * Select slow mode to guarantee a stable clock while we're
	 * adjusting the PLL.
	 */
	HWRITE4(sc, RK3288_CRU_MODE_CON,
	    (RK3288_CRU_MODE_PLL_WORK_MODE_MASK << 16 |
	     RK3288_CRU_MODE_PLL_WORK_MODE_SLOW) << shift);

	/* Assert reset. */
	HWRITE4(sc, base + 0x000c,
	    RK3288_CRU_PLL_RESET << 16 | RK3288_CRU_PLL_RESET);

	/* Set PLL rate. */
	HWRITE4(sc, base + 0x0000,
	    RK3288_CRU_PLL_CLKR_MASK << 16 |
	    (nr - 1) << RK3288_CRU_PLL_CLKR_SHIFT |
	    RK3288_CRU_PLL_CLKOD_MASK << 16 |
	    (no - 1) << RK3288_CRU_PLL_CLKOD_SHIFT);
	HWRITE4(sc, base + 0x0004,
	    RK3288_CRU_PLL_CLKF_MASK << 16 |
	    (nf - 1) << RK3288_CRU_PLL_CLKF_SHIFT);

	/* Deassert reset and wait. */
	HWRITE4(sc, base + 0x000c,
	    RK3288_CRU_PLL_RESET << 16);
	delay((nr * 500 / 24) + 1);

	/* Switch back to normal mode. */
	HWRITE4(sc, RK3288_CRU_MODE_CON,
	    (RK3288_CRU_MODE_PLL_WORK_MODE_MASK << 16 |
	     RK3288_CRU_MODE_PLL_WORK_MODE_NORMAL) << shift);

	return 0;
}

uint32_t
rk3288_get_frequency(void *cookie, uint32_t *cells)
{
	struct rkclock_softc *sc = cookie;
	uint32_t idx = cells[0];
	uint32_t reg, mux, div_con, aclk_div_con;

	switch (idx) {
	case RK3288_PLL_APLL:
		return rk3288_get_pll(sc, RK3288_CRU_APLL_CON(0));
	case RK3288_PLL_CPLL:
		return rk3288_get_pll(sc, RK3288_CRU_CPLL_CON(0));
	case RK3288_PLL_GPLL:
		return rk3288_get_pll(sc, RK3288_CRU_GPLL_CON(0));
	case RK3288_ARMCLK:
		reg = HREAD4(sc, RK3288_CRU_CLKSEL_CON(0));
		mux = (reg >> 15) & 0x1;
		div_con = (reg >> 8) & 0x1f;
		idx = (mux == 0) ? RK3288_PLL_APLL : RK3288_PLL_GPLL;
		return rk3288_get_frequency(sc, &idx) / (div_con + 1);
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
	case RK3288_PCLK_I2C0:
	case RK3288_PCLK_I2C2:
		reg = HREAD4(sc, RK3288_CRU_CLKSEL_CON(1));
		mux = (reg >> 15) & 0x1;
		/* pd_bus_pclk_div_con */
		div_con = (reg >> 12) & 0x7;
		if (mux == 1)
			idx = RK3288_PLL_GPLL;
		else
			idx = RK3288_PLL_CPLL;
		return rk3288_get_frequency(sc, &idx) / (div_con + 1);
	case RK3288_PCLK_I2C1:
	case RK3288_PCLK_I2C3:
	case RK3288_PCLK_I2C4:
	case RK3288_PCLK_I2C5:
		reg = HREAD4(sc, RK3288_CRU_CLKSEL_CON(10));
		mux = (reg >> 15) & 0x1;
		/* peri_pclk_div_con */
		div_con = (reg >> 12) & 0x3;
		/* peri_aclk_div_con */
		aclk_div_con = reg & 0xf;
		if (mux == 1)
			idx = RK3288_PLL_GPLL;
		else
			idx = RK3288_PLL_CPLL;
		return (rk3288_get_frequency(sc, &idx) / (aclk_div_con + 1)) >>
		    div_con;
	default:
		break;
	}

	printf("%s: 0x%08x\n", __func__, idx);
	return 0;
}

int
rk3288_set_frequency(void *cookie, uint32_t *cells, uint32_t freq)
{
	struct rkclock_softc *sc = cookie;
	uint32_t idx = cells[0];
	int error;

	switch (idx) {
	case RK3288_PLL_APLL:
		return rk3288_set_pll(sc, RK3288_CRU_APLL_CON(0), freq);
	case RK3288_ARMCLK:
		idx = RK3288_PLL_APLL;
		error = rk3288_set_frequency(sc, &idx, freq);
		if (error == 0) {
			HWRITE4(sc, RK3288_CRU_CLKSEL_CON(0),
			    ((1 << 15) | (0x1f << 8)) << 16);
		}
		return error;
	}

	printf("%s: 0x%08x\n", __func__, idx);
	return -1;
}

void
rk3288_enable(void *cookie, uint32_t *cells, int on)
{
	uint32_t idx = cells[0];

	switch (idx) {
	case RK3288_CLK_SDMMC:
	case RK3288_CLK_TSADC:
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
	case RK3288_PCLK_I2C0:
	case RK3288_PCLK_I2C1:
	case RK3288_PCLK_I2C2:
	case RK3288_PCLK_I2C3:
	case RK3288_PCLK_I2C4:
	case RK3288_PCLK_I2C5:
	case RK3288_PCLK_TSADC:
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
	struct rkclock_softc *sc = cookie;
	uint32_t idx = cells[0];
	uint32_t mask = (1 << (idx % 16));

	HWRITE4(sc, RK3288_CRU_SOFTRST_CON(idx / 16),
	    mask << 16 | (on ? mask : 0));
}

/*
 * Rockchip RK3328
 */

struct rkclock rk3328_clocks[] = {
	{
		RK3328_CLK_RTC32K, RK3328_CRU_CLKSEL_CON(38),
		SEL(15, 14), DIV(13, 0),
		{ RK3328_PLL_CPLL, RK3328_PLL_GPLL, RK3328_XIN24M }
	},
	{
		RK3328_CLK_SDMMC, RK3328_CRU_CLKSEL_CON(30),
		SEL(9, 8), DIV(7, 0),
		{ RK3328_PLL_CPLL, RK3328_PLL_GPLL, RK3328_XIN24M,
		  RK3328_USB480M }
	},
	{
		RK3328_CLK_SDIO, RK3328_CRU_CLKSEL_CON(31),
		SEL(9, 8), DIV(7, 0),
		{ RK3328_PLL_CPLL, RK3328_PLL_GPLL, RK3328_XIN24M,
		  RK3328_USB480M }
	},
	{
		RK3328_CLK_EMMC, RK3328_CRU_CLKSEL_CON(32),
		SEL(9, 8), DIV(7, 0),
		{ RK3328_PLL_CPLL, RK3328_PLL_GPLL, RK3328_XIN24M,
		  RK3328_USB480M }
	},
	{
		RK3328_CLK_TSADC, RK3328_CRU_CLKSEL_CON(22),
		0, DIV(9, 0),
		{ RK3328_CLK_24M }
	},
	{
		RK3328_CLK_UART0, RK3328_CRU_CLKSEL_CON(14),
		SEL(9, 8), 0,
		{ 0, 0, RK3328_XIN24M, RK3328_XIN24M }
	},
	{
		RK3328_CLK_UART1, RK3328_CRU_CLKSEL_CON(16),
		SEL(9, 8), 0,
		{ 0, 0, RK3328_XIN24M, RK3328_XIN24M }
	},
	{
		RK3328_CLK_UART2, RK3328_CRU_CLKSEL_CON(18),
		SEL(9, 8), 0,
		{ 0, 0, RK3328_XIN24M, RK3328_XIN24M }
	},
	{
		RK3328_CLK_WIFI, RK3328_CRU_CLKSEL_CON(52),
		SEL(7, 6), DIV(5, 0),
		{ RK3328_PLL_CPLL, RK3328_PLL_GPLL, RK3328_USB480M }
	},
	{
		RK3328_CLK_I2C0, RK3328_CRU_CLKSEL_CON(34),
		SEL(7, 7), DIV(6, 0),
		{ RK3328_PLL_CPLL, RK3328_PLL_GPLL }
	},
	{
		RK3328_CLK_I2C1, RK3328_CRU_CLKSEL_CON(34),
		SEL(15, 15), DIV(14, 8),
		{ RK3328_PLL_CPLL, RK3328_PLL_GPLL }
	},
	{
		RK3328_CLK_I2C2, RK3328_CRU_CLKSEL_CON(35),
		SEL(7, 7), DIV(6, 0),
		{ RK3328_PLL_CPLL, RK3328_PLL_GPLL }
	},
	{
		RK3328_CLK_I2C3, RK3328_CRU_CLKSEL_CON(35),
		SEL(15, 15), DIV(14, 8),
		{ RK3328_PLL_CPLL, RK3328_PLL_GPLL }
	},
	{
		RK3328_CLK_PDM, RK3328_CRU_CLKSEL_CON(20),
		SEL(15, 14), DIV(12, 8),
		{ RK3328_PLL_CPLL, RK3328_PLL_GPLL, RK3328_PLL_APLL },
		FIXED_PARENT | SET_PARENT
	},
	{
		RK3328_CLK_VDEC_CABAC, RK3328_CRU_CLKSEL_CON(48),
		SEL(15, 14), DIV(12, 8),
		{ RK3328_PLL_CPLL, RK3328_PLL_GPLL, RK3328_HDMIPHY,
		  RK3328_USB480M }
	},
	{
		RK3328_CLK_VDEC_CORE, RK3328_CRU_CLKSEL_CON(49),
		SEL(7, 6), DIV(4, 0),
		{ RK3328_PLL_CPLL, RK3328_PLL_GPLL, RK3328_HDMIPHY,
		  RK3328_USB480M }
	},
	{
		RK3328_CLK_VENC_DSP, RK3328_CRU_CLKSEL_CON(52),
		SEL(15, 14), DIV(12, 8),
		{ RK3328_PLL_CPLL, RK3328_PLL_GPLL, RK3328_HDMIPHY,
		  RK3328_USB480M }
	},
	{
		RK3328_CLK_VENC_CORE, RK3328_CRU_CLKSEL_CON(51),
		SEL(15, 14), DIV(12, 8),
		{ RK3328_PLL_CPLL, RK3328_PLL_GPLL, RK3328_HDMIPHY,
		  RK3328_USB480M }
	},
	{
		RK3328_CLK_TSP, RK3328_CRU_CLKSEL_CON(21),
		SEL(15, 15), DIV(12, 8),
		{ RK3328_PLL_CPLL, RK3328_PLL_GPLL }
	},
	{
		RK3328_DCLK_LCDC, RK3328_CRU_CLKSEL_CON(40),
		SEL(1, 1), 0,
		{ RK3328_HDMIPHY, RK3328_DCLK_LCDC_SRC }
	},
	{
		RK3328_ACLK_VOP_PRE, RK3328_CRU_CLKSEL_CON(39),
		SEL(7, 6), DIV(4, 0),
		{ RK3328_PLL_CPLL, RK3328_PLL_GPLL, RK3328_HDMIPHY,
		  RK3328_USB480M }
	},
	{
		RK3328_ACLK_RGA_PRE, RK3328_CRU_CLKSEL_CON(36),
		SEL(15, 14), DIV(12, 8),
		{ RK3328_PLL_CPLL, RK3328_PLL_GPLL, RK3328_HDMIPHY,
		  RK3328_USB480M }
	},
	{
		RK3328_ACLK_BUS_PRE, RK3328_CRU_CLKSEL_CON(0),
		SEL(14, 13), DIV(12, 8),
		{ RK3328_PLL_CPLL, RK3328_PLL_GPLL, RK3328_HDMIPHY }
	},
	{
		RK3328_ACLK_PERI_PRE, RK3328_CRU_CLKSEL_CON(28),
		SEL(7, 6), DIV(4, 0),
		{ RK3328_PLL_CPLL, RK3328_PLL_GPLL, RK3328_HDMIPHY }
	},
	{
		RK3328_ACLK_RKVDEC_PRE, RK3328_CRU_CLKSEL_CON(48),
		SEL(7, 6), DIV(4, 0),
		{ RK3328_PLL_CPLL, RK3328_PLL_GPLL, RK3328_HDMIPHY,
		  RK3328_USB480M }
	},
	{
		RK3328_ACLK_RKVENC, RK3328_CRU_CLKSEL_CON(51),
		SEL(7, 6), DIV(4, 0),
		{ RK3328_PLL_CPLL, RK3328_PLL_GPLL, RK3328_HDMIPHY,
		  RK3328_USB480M }
	},
	{
		RK3328_ACLK_VPU_PRE, RK3328_CRU_CLKSEL_CON(50),
		SEL(7, 6), DIV(4, 0),
		{ RK3328_PLL_CPLL, RK3328_PLL_GPLL, RK3328_HDMIPHY,
		  RK3328_USB480M }
	},
	{
		RK3328_ACLK_VIO_PRE, RK3328_CRU_CLKSEL_CON(37),
		SEL(7, 6), DIV(4, 0),
		{ RK3328_PLL_CPLL, RK3328_PLL_GPLL, RK3328_HDMIPHY,
		  RK3328_USB480M }
	},
	{
		RK3328_PCLK_BUS_PRE, RK3328_CRU_CLKSEL_CON(1),
		0, DIV(14, 12),
		{ RK3328_ACLK_BUS_PRE }
	},
	{
		RK3328_HCLK_BUS_PRE, RK3328_CRU_CLKSEL_CON(1),
		0, DIV(9, 8),
		{ RK3328_ACLK_BUS_PRE }
	},
	{
		RK3328_PCLK_PERI, RK3328_CRU_CLKSEL_CON(29),
		0, DIV(6, 4),
		{ RK3328_ACLK_PERI_PRE }
	},
	{
		RK3328_HCLK_PERI, RK3328_CRU_CLKSEL_CON(29),
		0, DIV(1, 0),
		{ RK3328_ACLK_PERI_PRE }
	},
	{
		RK3328_CLK_24M, RK3328_CRU_CLKSEL_CON(2),
		0, DIV(12, 8),
		{ RK3328_XIN24M }
	},
	{
		/* Sentinel */
	}
};

void
rk3328_init(struct rkclock_softc *sc)
{
	int i;

	/* The code below assumes all clocks are enabled.  Check this!. */
	for (i = 0; i <= 28; i++) {
		if (HREAD4(sc, RK3328_CRU_CLKGATE_CON(i)) != 0x00000000) {
			printf("CRU_CLKGATE_CON%d: 0x%08x\n", i,
			    HREAD4(sc, RK3328_CRU_CLKGATE_CON(i)));
		}
	}

	sc->sc_clocks = rk3328_clocks;
}

uint32_t
rk3328_armclk_parent(uint32_t mux)
{
	switch (mux) {
	case 0:
		return RK3328_PLL_APLL;
	case 1:
		return RK3328_PLL_GPLL;
	case 2:
		return RK3328_PLL_DPLL;
	case 3:
		return RK3328_PLL_NPLL;
	}

	return 0;
}

uint32_t
rk3328_get_armclk(struct rkclock_softc *sc)
{
	uint32_t reg, mux, div_con;
	uint32_t idx;

	reg = HREAD4(sc, RK3328_CRU_CLKSEL_CON(0));
	mux = (reg & RK3328_CRU_CORE_CLK_PLL_SEL_MASK) >>
	    RK3328_CRU_CORE_CLK_PLL_SEL_SHIFT;
	div_con = (reg & RK3328_CRU_CLK_CORE_DIV_CON_MASK) >>
	    RK3328_CRU_CLK_CORE_DIV_CON_SHIFT;
	idx = rk3328_armclk_parent(mux);

	return rk3328_get_frequency(sc, &idx) / (div_con + 1);
}

int
rk3328_set_armclk(struct rkclock_softc *sc, uint32_t freq)
{
	uint32_t reg, mux;
	uint32_t old_freq, div;
	uint32_t idx;

	old_freq = rk3328_get_armclk(sc);
	if (freq == old_freq)
		return 0;

	reg = HREAD4(sc, RK3328_CRU_CLKSEL_CON(0));
	mux = (reg & RK3328_CRU_CORE_CLK_PLL_SEL_MASK) >>
	    RK3328_CRU_CORE_CLK_PLL_SEL_SHIFT;

	/* Keep the pclk_dbg clock at or below 300 MHz. */
	div = 1;
	while (freq / (div + 1) > 300000000)
		div++;
	/* and make sure we use an odd divider. */
	if ((div % 2) == 0)
		div++;

	/* When ramping up, set clock dividers first. */
	if (freq > old_freq) {
		HWRITE4(sc, RK3328_CRU_CLKSEL_CON(0),
		    RK3328_CRU_CLK_CORE_DIV_CON_MASK << 16 |
		    0 << RK3328_CRU_CLK_CORE_DIV_CON_SHIFT);
		HWRITE4(sc, RK3328_CRU_CLKSEL_CON(1),
		    RK3328_CRU_ACLK_CORE_DIV_CON_MASK << 16 |
		    1 << RK3328_CRU_ACLK_CORE_DIV_CON_SHIFT |
		    RK3328_CRU_CLK_CORE_DBG_DIV_CON_MASK << 16 |
		    div << RK3328_CRU_CLK_CORE_DBG_DIV_CON_SHIFT);
	}

	/* We always use NPLL and force the switch below if needed. */
	idx = RK3328_PLL_NPLL;
	rk3328_set_frequency(sc, &idx, freq);

	/* When ramping down, set clock dividers last. */
	if (freq < old_freq || mux != 3) {
		HWRITE4(sc, RK3328_CRU_CLKSEL_CON(0),
		    RK3328_CRU_CORE_CLK_PLL_SEL_MASK << 16 |
		    3 << RK3328_CRU_CORE_CLK_PLL_SEL_SHIFT |
		    RK3328_CRU_CLK_CORE_DIV_CON_MASK << 16 |
		    0 << RK3328_CRU_CLK_CORE_DIV_CON_SHIFT);
		HWRITE4(sc, RK3328_CRU_CLKSEL_CON(1),
		    RK3328_CRU_ACLK_CORE_DIV_CON_MASK << 16 |
		    1 << RK3328_CRU_ACLK_CORE_DIV_CON_SHIFT |
		    RK3328_CRU_CLK_CORE_DBG_DIV_CON_MASK << 16 |
		    div << RK3328_CRU_CLK_CORE_DBG_DIV_CON_SHIFT);
	}

	return 0;
}

uint32_t
rk3328_get_pll(struct rkclock_softc *sc, bus_size_t base)
{
	uint32_t fbdiv, postdiv1, postdiv2, refdiv;
	uint32_t dsmpd, fracdiv;
	uint64_t frac = 0;
	uint32_t reg;

	reg = HREAD4(sc, base + 0x0000);
	postdiv1 = (reg & RK3328_CRU_PLL_POSTDIV1_MASK) >>
	    RK3328_CRU_PLL_POSTDIV1_SHIFT;
	fbdiv = (reg & RK3328_CRU_PLL_FBDIV_MASK) >>
	    RK3328_CRU_PLL_FBDIV_SHIFT;
	reg = HREAD4(sc, base + 0x0004);
	dsmpd = (reg & RK3328_CRU_PLL_DSMPD);
	postdiv2 = (reg & RK3328_CRU_PLL_POSTDIV2_MASK) >>
	    RK3328_CRU_PLL_POSTDIV2_SHIFT;
	refdiv = (reg & RK3328_CRU_PLL_REFDIV_MASK) >>
	    RK3328_CRU_PLL_REFDIV_SHIFT;
	reg = HREAD4(sc, base + 0x0008);
	fracdiv = (reg & RK3328_CRU_PLL_FRACDIV_MASK) >>
	    RK3328_CRU_PLL_FRACDIV_SHIFT;

	if (dsmpd == 0)
		frac = (24000000ULL * fracdiv / refdiv) >> 24;
	return ((24000000ULL * fbdiv / refdiv) + frac) / postdiv1 / postdiv2;
}

int
rk3328_set_pll(struct rkclock_softc *sc, bus_size_t base, uint32_t freq)
{
	uint32_t fbdiv, postdiv1, postdiv2, refdiv;
	int mode_shift = -1;

	switch (base) {
	case RK3328_CRU_APLL_CON(0):
		mode_shift = 0;
		break;
	case RK3328_CRU_DPLL_CON(0):
		mode_shift = 4;
		break;
	case RK3328_CRU_CPLL_CON(0):
		mode_shift = 8;
		break;
	case RK3328_CRU_GPLL_CON(0):
		mode_shift = 12;
		break;
	case RK3328_CRU_NPLL_CON(0):
		mode_shift = 1;
		break;
	}
	KASSERT(mode_shift != -1);

	/*
	 * It is not clear whether all combinations of the clock
	 * dividers result in a stable clock.  Therefore this function
	 * only supports a limited set of PLL clock rates.  For now
	 * this set covers all the CPU frequencies supported by the
	 * Linux kernel.
	 */
	switch (freq) {
	case 1800000000U:
	case 1704000000U:
	case 1608000000U:
	case 1512000000U:
	case 1488000000U:
	case 1416000000U:
	case 1392000000U:
	case 1296000000U:
	case 1200000000U:
	case 1104000000U:
		postdiv1 = postdiv2 = refdiv = 1;
		break;
	case 1008000000U:
	case 912000000U:
	case 816000000U:
	case 696000000U:
		postdiv1 = 2; postdiv2 = refdiv = 1;
		break;
	case 600000000U:
		postdiv1 = 3; postdiv2 = refdiv = 1;
		break;
	case 408000000U:
	case 312000000U:
		postdiv1 = postdiv2 = 2; refdiv = 1;
		break;
	case 216000000U:
		postdiv1 = 4; postdiv2 = 2; refdiv = 1;
		break;
	case 96000000U:
		postdiv1 = postdiv2 = 4; refdiv = 1;
		break;
	default:
		printf("%s: %u Hz\n", __func__, freq);
		return -1;
	}

	/* Calculate feedback divider. */
	fbdiv = freq * postdiv1 * postdiv2 * refdiv / 24000000;

	/*
	 * Select slow mode to guarantee a stable clock while we're
	 * adjusting the PLL.
	 */
	HWRITE4(sc, RK3328_CRU_CRU_MODE,
	   (RK3328_CRU_CRU_MODE_MASK << 16 |
	   RK3328_CRU_CRU_MODE_SLOW) << mode_shift);

	/* Set PLL rate. */
	HWRITE4(sc, base + 0x0000,
	    RK3328_CRU_PLL_POSTDIV1_MASK << 16 |
	    postdiv1 << RK3328_CRU_PLL_POSTDIV1_SHIFT |
	    RK3328_CRU_PLL_FBDIV_MASK << 16 |
	    fbdiv << RK3328_CRU_PLL_FBDIV_SHIFT);
	HWRITE4(sc, base + 0x0004,
	    RK3328_CRU_PLL_DSMPD << 16 | RK3328_CRU_PLL_DSMPD |
	    RK3328_CRU_PLL_POSTDIV2_MASK << 16 |
	    postdiv2 << RK3328_CRU_PLL_POSTDIV2_SHIFT |
	    RK3328_CRU_PLL_REFDIV_MASK << 16 |
	    refdiv << RK3328_CRU_PLL_REFDIV_SHIFT);

	/* Wait for PLL to stabilize. */
	while ((HREAD4(sc, base + 0x0004) & RK3328_CRU_PLL_PLL_LOCK) == 0)
		delay(10);

	/* Switch back to normal mode. */
	HWRITE4(sc, RK3328_CRU_CRU_MODE,
	   (RK3328_CRU_CRU_MODE_MASK << 16 |
	   RK3328_CRU_CRU_MODE_NORMAL) << mode_shift);

	return 0;
}

int
rk3328_set_frac_pll(struct rkclock_softc *sc, bus_size_t base, uint32_t freq)
{
	uint32_t fbdiv, postdiv1, postdiv2, refdiv, fracdiv;
	int mode_shift = -1;
	uint32_t reg;

	switch (base) {
	case RK3328_CRU_APLL_CON(0):
		mode_shift = 0;
		break;
	case RK3328_CRU_DPLL_CON(0):
		mode_shift = 4;
		break;
	case RK3328_CRU_CPLL_CON(0):
		mode_shift = 8;
		break;
	case RK3328_CRU_GPLL_CON(0):
		mode_shift = 12;
		break;
	case RK3328_CRU_NPLL_CON(0):
		mode_shift = 1;
		break;
	}
	KASSERT(mode_shift != -1);

	/*
	 * It is not clear whether all combinations of the clock
	 * dividers result in a stable clock.  Therefore this function
	 * only supports a limited set of PLL clock rates.  This set
	 * set covers all the fractional PLL frequencies supported by
	 * the Linux kernel.
	 */
	switch (freq) {
	case 1016064000U:
		postdiv1 = postdiv2 = 1; refdiv = 3; fracdiv = 134217;
		break;
	case 983040000U:
		postdiv1 = postdiv2 = 1; refdiv = 24; fracdiv = 671088;
		break;
	case 491520000U:
		postdiv1 = 2; postdiv2 = 1; refdiv = 24; fracdiv = 671088;
		break;
	case 61440000U:
		postdiv1 = 7; postdiv2 = 2; refdiv = 6; fracdiv = 671088;
		break;
	case 56448000U:
		postdiv1 = postdiv2 = 4; refdiv = 12; fracdiv = 9797894;
		break;
	case 40960000U:
		postdiv1 = 4; postdiv2 = 5; refdiv = 12; fracdiv = 10066239;
		break;
	default:
		printf("%s: %u Hz\n", __func__, freq);
		return -1;
	}

	/* Calculate feedback divider. */
	fbdiv = (uint64_t)freq * postdiv1 * postdiv2 * refdiv / 24000000;

	/*
	 * Select slow mode to guarantee a stable clock while we're
	 * adjusting the PLL.
	 */
	HWRITE4(sc, RK3328_CRU_CRU_MODE,
	   (RK3328_CRU_CRU_MODE_MASK << 16 |
	   RK3328_CRU_CRU_MODE_SLOW) << mode_shift);

	/* Set PLL rate. */
	HWRITE4(sc, base + 0x0000,
	    RK3328_CRU_PLL_POSTDIV1_MASK << 16 |
	    postdiv1 << RK3328_CRU_PLL_POSTDIV1_SHIFT |
	    RK3328_CRU_PLL_FBDIV_MASK << 16 |
	    fbdiv << RK3328_CRU_PLL_FBDIV_SHIFT);
	HWRITE4(sc, base + 0x0004,
	    RK3328_CRU_PLL_DSMPD << 16 |
	    RK3328_CRU_PLL_POSTDIV2_MASK << 16 |
	    postdiv2 << RK3328_CRU_PLL_POSTDIV2_SHIFT |
	    RK3328_CRU_PLL_REFDIV_MASK << 16 |
	    refdiv << RK3328_CRU_PLL_REFDIV_SHIFT);
	reg = HREAD4(sc, base + 0x0008);
	reg &= ~RK3328_CRU_PLL_FRACDIV_MASK;
	reg |= fracdiv << RK3328_CRU_PLL_FRACDIV_SHIFT;
	HWRITE4(sc, base + 0x0008, reg);

	/* Wait for PLL to stabilize. */
	while ((HREAD4(sc, base + 0x0004) & RK3328_CRU_PLL_PLL_LOCK) == 0)
		delay(10);

	/* Switch back to normal mode. */
	HWRITE4(sc, RK3328_CRU_CRU_MODE,
	   (RK3328_CRU_CRU_MODE_MASK << 16 |
	   RK3328_CRU_CRU_MODE_NORMAL) << mode_shift);

	return 0;
}

uint32_t
rk3328_get_frequency(void *cookie, uint32_t *cells)
{
	struct rkclock_softc *sc = cookie;
	uint32_t idx = cells[0];

	switch (idx) {
	case RK3328_PLL_APLL:
		return rk3328_get_pll(sc, RK3328_CRU_APLL_CON(0));
		break;
	case RK3328_PLL_DPLL:
		return rk3328_get_pll(sc, RK3328_CRU_DPLL_CON(0));
		break;
	case RK3328_PLL_CPLL:
		return rk3328_get_pll(sc, RK3328_CRU_CPLL_CON(0));
		break;
	case RK3328_PLL_GPLL:
		return rk3328_get_pll(sc, RK3328_CRU_GPLL_CON(0));
		break;
	case RK3328_PLL_NPLL:
		return rk3328_get_pll(sc, RK3328_CRU_NPLL_CON(0));
		break;
	case RK3328_ARMCLK:
		return rk3328_get_armclk(sc);
	case RK3328_XIN24M:
		return 24000000;
	/*
	 * XXX The HDMIPHY and USB480M clocks are external.  Returning
	 * zero here will cause them to be ignored for reparenting
	 * purposes.
	 */
	case RK3328_HDMIPHY:
		return 0;
	case RK3328_USB480M:
		return 0;
	default:
		break;
	}

	return rkclock_get_frequency(sc, idx);
}

int
rk3328_set_frequency(void *cookie, uint32_t *cells, uint32_t freq)
{
	struct rkclock_softc *sc = cookie;
	uint32_t idx = cells[0];
	uint32_t reg, mux;

	switch (idx) {
	case RK3328_PLL_APLL:
		return rk3328_set_frac_pll(sc, RK3328_CRU_APLL_CON(0), freq);
	case RK3328_PLL_DPLL:
		return rk3328_set_pll(sc, RK3328_CRU_DPLL_CON(0), freq);
	case RK3328_PLL_CPLL:
		return rk3328_set_pll(sc, RK3328_CRU_CPLL_CON(0), freq);
	case RK3328_PLL_GPLL:
		return rk3328_set_frac_pll(sc, RK3328_CRU_GPLL_CON(0), freq);
	case RK3328_PLL_NPLL:
		return rk3328_set_pll(sc, RK3328_CRU_NPLL_CON(0), freq);
	case RK3328_ARMCLK:
		return rk3328_set_armclk(sc, freq);
	case RK3328_CLK_UART0:
	case RK3328_CLK_UART1:
	case RK3328_CLK_UART2:
		if (freq == rk3328_get_frequency(sc, &idx))
			return 0;
		break;
	case RK3328_DCLK_LCDC:
		reg = HREAD4(sc, RK3328_CRU_CLKSEL_CON(40));
		mux = (reg & RK3328_CRU_VOP_DCLK_SRC_SEL_MASK) >>
		    RK3328_CRU_VOP_DCLK_SRC_SEL_SHIFT;
		idx = (mux == 0) ? RK3328_HDMIPHY : RK3328_DCLK_LCDC_SRC;
		return rk3328_set_frequency(sc, &idx, freq);
	default:
		break;
	}

	return rkclock_set_frequency(sc, idx, freq);
}

int
rk3328_set_parent(void *cookie, uint32_t *cells, uint32_t *pcells)
{
	struct rkclock_softc *sc = cookie;
	uint32_t idx = cells[0];
	uint32_t parent;

	if (pcells[0] == sc->sc_phandle)
		parent = pcells[1];
	else {
		char name[32];
		int node;

		node = OF_getnodebyphandle(pcells[0]);
		if (node == 0)
			return -1;
		name[0] = 0;
		OF_getprop(node, "clock-output-names", name, sizeof(name));
		name[sizeof(name) - 1] = 0;
		if (strcmp(name, "xin24m") != 0)
			return -1;

		parent = RK3328_XIN24M;
	}

	return rkclock_set_parent(sc, idx, parent);
}

void
rk3328_enable(void *cookie, uint32_t *cells, int on)
{
	uint32_t idx = cells[0];

	/*
	 * All clocks are enabled by default, so there is nothing for
	 * us to do until we start disabling clocks.
	 */
	if (!on)
		printf("%s: 0x%08x\n", __func__, idx);
}

void
rk3328_reset(void *cookie, uint32_t *cells, int on)
{
	struct rkclock_softc *sc = cookie;
	uint32_t idx = cells[0];
	uint32_t mask = (1 << (idx % 16));

	HWRITE4(sc, RK3328_CRU_SOFTRST_CON(idx / 16),
	    mask << 16 | (on ? mask : 0));
}

/* 
 * Rockchip RK3399 
 */

struct rkclock rk3399_clocks[] = {
	{
		RK3399_CLK_I2C1, RK3399_CRU_CLKSEL_CON(61),
		SEL(7, 7), DIV(6, 0),
		{ RK3399_PLL_CPLL, RK3399_PLL_GPLL }
	},
	{
		RK3399_CLK_I2C2, RK3399_CRU_CLKSEL_CON(62),
		SEL(7, 7), DIV(6, 0),
		{ RK3399_PLL_CPLL, RK3399_PLL_GPLL }
	},
	{
		RK3399_CLK_I2C3, RK3399_CRU_CLKSEL_CON(63),
		SEL(7, 7), DIV(6, 0),
		{ RK3399_PLL_CPLL, RK3399_PLL_GPLL }
	},
	{
		RK3399_CLK_I2C5, RK3399_CRU_CLKSEL_CON(61),
		SEL(15, 15), DIV(14, 8),
		{ RK3399_PLL_CPLL, RK3399_PLL_GPLL }
	},
	{
		RK3399_CLK_I2C6, RK3399_CRU_CLKSEL_CON(62),
		SEL(15, 15), DIV(14, 8),
		{ RK3399_PLL_CPLL, RK3399_PLL_GPLL }
	},
	{
		RK3399_CLK_I2C7, RK3399_CRU_CLKSEL_CON(63),
		SEL(15, 15), DIV(14, 8),
		{ RK3399_PLL_CPLL, RK3399_PLL_GPLL }
	},
	{
		RK3399_CLK_SDMMC, RK3399_CRU_CLKSEL_CON(16),
		SEL(10, 8), DIV(6, 0),
		{ RK3399_PLL_CPLL, RK3399_PLL_GPLL, RK3399_PLL_NPLL,
		  /* RK3399_PLL_PPLL */ 0, /* RK3399_USB_480M */ 0,
		  RK3399_XIN24M }
	},
	{
		RK3399_CLK_SDIO, RK3399_CRU_CLKSEL_CON(15),
		SEL(10, 8), DIV(6, 0),
		{ RK3399_PLL_CPLL, RK3399_PLL_GPLL, RK3399_PLL_NPLL,
		  /* RK3399_PLL_PPLL */ 0, /* RK3399_USB_480M */ 0,
		  RK3399_XIN24M }
	},
	{
		RK3399_CLK_TSADC, RK3399_CRU_CLKSEL_CON(27),
		SEL(15, 15), DIV(9, 0),
		{ RK3399_XIN24M, RK3399_CLK_32K }
	},
	{
		RK3399_CLK_UART0, RK3399_CRU_CLKSEL_CON(33),
		SEL(9, 8), 0,
		{ 0, 0, RK3399_XIN24M }
	},
	{
		RK3399_CLK_UART1, RK3399_CRU_CLKSEL_CON(34),
		SEL(9, 8), 0,
		{ 0, 0, RK3399_XIN24M }
	},
	{
		RK3399_CLK_UART2, RK3399_CRU_CLKSEL_CON(35),
		SEL(9, 8), 0,
		{ 0, 0, RK3399_XIN24M }
	},
	{
		RK3399_CLK_UART3, RK3399_CRU_CLKSEL_CON(36),
		SEL(9, 8), 0,
		{ 0, 0, RK3399_XIN24M }
	},
	{
		RK3399_ACLK_PERIPH, RK3399_CRU_CLKSEL_CON(14),
		SEL(7, 7), DIV(4, 0),
		{ RK3399_PLL_CPLL, RK3399_PLL_GPLL }
	},
	{
		RK3399_ACLK_PERILP0, RK3399_CRU_CLKSEL_CON(23),
		SEL(7, 7), DIV(4, 0),
		{ RK3399_PLL_CPLL, RK3399_PLL_GPLL }
	},
	{
		RK3399_ACLK_VIO, RK3399_CRU_CLKSEL_CON(42),
		SEL(7, 6), DIV(4, 0),
		{ RK3399_PLL_CPLL, RK3399_PLL_GPLL, /* RK3399_PLL_PPLL */ }
	},
	{
		RK3399_ACLK_CCI, RK3399_CRU_CLKSEL_CON(5),
		SEL(7, 6), DIV(4, 0),
		{ RK3399_PLL_CPLL, RK3399_PLL_GPLL, RK3399_PLL_NPLL,
		  RK3399_PLL_VPLL }
	},
	{
		RK3399_ACLK_HDCP, RK3399_CRU_CLKSEL_CON(42),
		SEL(15, 14), DIV(12, 8),
		{ RK3399_PLL_CPLL, RK3399_PLL_GPLL, /* RK3399_PLL_PPLL */ }
	},
	{
		RK3399_ACLK_GIC_PRE, RK3399_CRU_CLKSEL_CON(56),
		SEL(15, 15), DIV(12, 8),
		{ RK3399_PLL_CPLL, RK3399_PLL_GPLL }
	},
	{
		RK3399_PCLK_PERIPH, RK3399_CRU_CLKSEL_CON(14),
		0, DIV(14, 12),
		{ RK3399_ACLK_PERIPH }
	},
	{
		RK3399_PCLK_PERILP0, RK3399_CRU_CLKSEL_CON(23),
		0, DIV(14, 12),
		{ RK3399_ACLK_PERILP0 }
	},
	{
		RK3399_PCLK_PERILP1, RK3399_CRU_CLKSEL_CON(25),
		0, DIV(10, 8),
		{ RK3399_HCLK_PERILP1 }
	},
	{
		RK3399_PCLK_DDR, RK3399_CRU_CLKSEL_CON(6),
		SEL(15, 15), DIV(12, 8),
		{ RK3399_PLL_CPLL, RK3399_PLL_GPLL }
	},
	{
		RK3399_HCLK_PERIPH, RK3399_CRU_CLKSEL_CON(14),
		0, DIV(9, 8),
		{ RK3399_ACLK_PERIPH }
	},
	{
		RK3399_HCLK_PERILP0, RK3399_CRU_CLKSEL_CON(23),
		0, DIV(9, 8),
		{ RK3399_ACLK_PERILP0 }
	},
	{
		RK3399_HCLK_PERILP1, RK3399_CRU_CLKSEL_CON(25),
		SEL(7, 7), DIV(4, 0),
		{ RK3399_PLL_CPLL, RK3399_PLL_GPLL }
	},
	{
		RK3399_HCLK_SDMMC, RK3399_CRU_CLKSEL_CON(13),
		SEL(15, 15), DIV(12, 8),
		{ RK3399_PLL_CPLL, RK3399_PLL_GPLL }
	},
	{
		/* Sentinel */
	}
};

/* Some of our parent clocks live in the PMUCRU. */
struct rkclock_softc *rk3399_pmucru_sc;

void
rk3399_init(struct rkclock_softc *sc)
{
	int i;

	/* PMUCRU instance should attach before us. */
	KASSERT(rk3399_pmucru_sc != NULL);

	/*
	 * The U-Boot shipped on the Theobroma Systems RK3399-Q7
	 * module is buggy and sets the parent of the clock for the
	 * "big" cluster to LPLL.  Undo that mistake here such that
	 * the clocks of both clusters are independent.
	 */
	HWRITE4(sc, RK3399_CRU_CLKSEL_CON(2),
	    RK3399_CRU_CORE_PLL_SEL_MASK << 16 |
	    RK3399_CRU_CORE_PLL_SEL_BPLL);

	/* The code below assumes all clocks are enabled.  Check this!. */
	for (i = 0; i <= 34; i++) {
		if (HREAD4(sc, RK3399_CRU_CLKGATE_CON(i)) != 0x00000000) {
			printf("CRU_CLKGATE_CON%d: 0x%08x\n", i,
			    HREAD4(sc, RK3399_CRU_CLKGATE_CON(i)));
		}
	}

	sc->sc_clocks = rk3399_clocks;
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
	case 676000000U:
		postdiv1 = 2; postdiv2 = 1; refdiv = 3;
		break;
	case 1000000000U:
	case 800000000U:
	case 600000000U:
		postdiv1 = 3; postdiv2 = refdiv = 1;
		break;
	case 594000000U:
		postdiv1 = 4; postdiv2 = refdiv = 1;
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
		printf("%s: %d Hz\n", __func__, freq);
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
		HWRITE4(sc, clksel,
		    RK3399_CRU_CLK_CORE_DIV_CON_MASK << 16 |
		    0 << RK3399_CRU_CLK_CORE_DIV_CON_SHIFT |
		    RK3399_CRU_ACLKM_CORE_DIV_CON_MASK << 16 |
		    1 << RK3399_CRU_ACLKM_CORE_DIV_CON_SHIFT);
		HWRITE4(sc, clksel + 0x0004,
		    RK3399_CRU_PCLK_DBG_DIV_CON_MASK << 16 |
		    div << RK3399_CRU_PCLK_DBG_DIV_CON_SHIFT |
		    RK3399_CRU_ATCLK_CORE_DIV_CON_MASK << 16 |
		    div << RK3399_CRU_ATCLK_CORE_DIV_CON_SHIFT);
	}

	rk3399_set_frequency(sc, &idx, freq);

	/* When ramping down, set clock dividers last. */
	if (freq < old_freq) {
		HWRITE4(sc, clksel,
		    RK3399_CRU_CLK_CORE_DIV_CON_MASK << 16 |
		    0 << RK3399_CRU_CLK_CORE_DIV_CON_SHIFT |
		    RK3399_CRU_ACLKM_CORE_DIV_CON_MASK << 16 |
		    1 << RK3399_CRU_ACLKM_CORE_DIV_CON_SHIFT);
		HWRITE4(sc, clksel + 0x0004,
		    RK3399_CRU_PCLK_DBG_DIV_CON_MASK << 16 |
		    div << RK3399_CRU_PCLK_DBG_DIV_CON_SHIFT |
		    RK3399_CRU_ATCLK_CORE_DIV_CON_MASK << 16 |
		    div << RK3399_CRU_ATCLK_CORE_DIV_CON_SHIFT);
	}

	return 0;
}

uint32_t
rk3399_get_frequency(void *cookie, uint32_t *cells)
{
	struct rkclock_softc *sc = cookie;
	uint32_t idx = cells[0];

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
	case RK3399_PLL_VPLL:
		return rk3399_get_pll(sc, RK3399_CRU_VPLL_CON(0));
	case RK3399_ARMCLKL:
		return rk3399_get_armclk(sc, RK3399_CRU_CLKSEL_CON(0));
	case RK3399_ARMCLKB:
		return rk3399_get_armclk(sc, RK3399_CRU_CLKSEL_CON(2));
	case RK3399_XIN24M:
		return 24000000;
	case RK3399_CLK_32K:
		return 32768;
	default:
		break;
	}

	return rkclock_get_frequency(sc, idx);
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
	case RK3399_PLL_CPLL:
		return rk3399_set_pll(sc, RK3399_CRU_CPLL_CON(0), freq);
	case RK3399_PLL_GPLL:
		return rk3399_set_pll(sc, RK3399_CRU_GPLL_CON(0), freq);
	case RK3399_PLL_NPLL:
		return rk3399_set_pll(sc, RK3399_CRU_NPLL_CON(0), freq);
	case RK3399_ARMCLKL:
		return rk3399_set_armclk(sc, RK3399_CRU_CLKSEL_CON(0), freq);
	case RK3399_ARMCLKB:
		return rk3399_set_armclk(sc, RK3399_CRU_CLKSEL_CON(2), freq);
	default:
		break;
	}

	return rkclock_set_frequency(sc, idx, freq);
}

void
rk3399_enable(void *cookie, uint32_t *cells, int on)
{
	uint32_t idx = cells[0];

	/*
	 * All clocks are enabled by default, so there is nothing for
	 * us to do until we start disabling clocks.
	 */
	if (!on)
		printf("%s: 0x%08x\n", __func__, idx);
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

/* PMUCRU */

struct rkclock rk3399_pmu_clocks[] = {
	{
		RK3399_CLK_I2C0, RK3399_PMUCRU_CLKSEL_CON(2),
		0, DIV(6, 0),
		{ RK3399_PLL_PPLL }
	},
	{
		RK3399_CLK_I2C4, RK3399_PMUCRU_CLKSEL_CON(3),
		0, DIV(6, 0),
		{ RK3399_PLL_PPLL }
	},
	{
		RK3399_CLK_I2C8, RK3399_PMUCRU_CLKSEL_CON(2),
		0, DIV(14, 8),
		{ RK3399_PLL_PPLL }
	},
	{
		/* Sentinel */
	}
};
	
void
rk3399_pmu_init(struct rkclock_softc *sc)
{
	sc->sc_clocks = rk3399_pmu_clocks;
	rk3399_pmucru_sc = sc;
}

uint32_t
rk3399_pmu_get_frequency(void *cookie, uint32_t *cells)
{
	struct rkclock_softc *sc = cookie;
	uint32_t idx = cells[0];

	switch (idx) {
	case RK3399_PLL_PPLL:
		return rk3399_get_pll(sc, RK3399_PMUCRU_PPLL_CON(0));
	default:
		break;
	}

	return rkclock_get_frequency(sc, idx);
}

int
rk3399_pmu_set_frequency(void *cookie, uint32_t *cells, uint32_t freq)
{
	struct rkclock_softc *sc = cookie;
	uint32_t idx = cells[0];

	switch (idx) {
	case RK3399_PLL_PPLL:
		return rk3399_set_pll(sc, RK3399_PMUCRU_PPLL_CON(0), freq);
		break;
	default:
		break;
	}

	return rkclock_set_frequency(sc, idx, freq);
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
