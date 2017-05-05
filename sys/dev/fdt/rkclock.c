/*	$OpenBSD: rkclock.c,v 1.2 2017/05/05 13:23:52 kettenis Exp $	*/
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

/* Registers */
#define RK3399_CRU_CLKSEL_CON(i)	(0x0100 + (i) * 4)
#define RK3399_CRU_CLKGATE_CON(i)	(0x0300 + (i) * 4)

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

uint32_t rk3399_get_frequency(void *, uint32_t *);
int	rk3399_set_frequency(void *, uint32_t *, uint32_t);
void	rk3399_enable(void *, uint32_t *, int);
void	rk3399_reset(void *, uint32_t *, int);

uint32_t rk3399_pmu_get_frequency(void *, uint32_t *);
int	rk3399_pmu_set_frequency(void *, uint32_t *, uint32_t);
void	rk3399_pmu_enable(void *, uint32_t *, int);
void	rk3399_pmu_reset(void *, uint32_t *, int);

struct rkclock_compat {
	const char *compat;
	void	(*enable)(void *, uint32_t *, int);
	uint32_t (*get_frequency)(void *, uint32_t *);
	int	(*set_frequency)(void *, uint32_t *, uint32_t);
	void	(*reset)(void *, uint32_t *, int);
};

struct rkclock_compat rkclock_compat[] = {
	{
		"rockchip,rk3399-cru",
		rk3399_enable,rk3399_get_frequency,
		rk3399_set_frequency, rk3399_reset
	},
	{
		"rockchip,rk3399-pmucru",
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
			sc->sc_cd.cd_enable = rkclock_compat[i].enable;
			sc->sc_cd.cd_get_frequency =
			    rkclock_compat[i].get_frequency;
			sc->sc_cd.cd_set_frequency =
			    rkclock_compat[i].set_frequency;
			sc->sc_rd.rd_reset = rkclock_compat[i].reset;
			break;
		}
	}

	sc->sc_cd.cd_node = faa->fa_node;
	sc->sc_cd.cd_cookie = sc;
	clock_register(&sc->sc_cd);

	sc->sc_rd.rd_node = faa->fa_node;
	sc->sc_rd.rd_cookie = sc;
	reset_register(&sc->sc_rd);
}

/* 
 * Rockchip RK3399 
 */

uint32_t
rk3399_get_frequency(void *cookie, uint32_t *cells)
{
	struct rkclock_softc *sc = cookie;
	uint32_t idx = cells[0];
	uint32_t reg, mux, div_con;

	switch (idx) {
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
	default:
		break;
	}

	printf("%s: 0x%08x\n", __func__, idx);
	return 0;
}

int
rk3399_set_frequency(void *cookie, uint32_t *cells, uint32_t freq)
{
	uint32_t idx = cells[0];

	printf("%s: 0x%08x\n", __func__, idx);
	return -1;
}

void
rk3399_enable(void *cookie, uint32_t *cells, int on)
{
	uint32_t idx = cells[0];

	switch (idx) {
	case RK3399_CLK_EMMC:
		/* Enabled by frimware! */
		break;
	case RK3399_CLK_UART0:
	case RK3399_CLK_UART1:
	case RK3399_CLK_UART2:
	case RK3399_CLK_UART3:
		/* Enabled by firmware? */
		break;
	case RK3399_HCLK_HOST0:
	case RK3399_HCLK_HOST0_ARB:
	case RK3399_HCLK_HOST1:
	case RK3399_HCLK_HOST1_ARB:
		/* Enabled by firmware! */
		break;
	case RK3399_ACLK_EMMC:
		/* Enabled by firmware! */
		break;
	default:
		printf("%s: 0x%08x\n", __func__, idx);
		break;
	}
}

void
rk3399_reset(void *cookie, uint32_t *cells, int on)
{
	uint32_t idx = cells[0];

	printf("%s: 0x%08x\n", __func__, idx);
}

uint32_t
rk3399_pmu_get_frequency(void *cookie, uint32_t *cells)
{
	uint32_t idx = cells[0];

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

	printf("%s: 0x%08x\n", __func__, idx);
}

void
rk3399_pmu_reset(void *cookie, uint32_t *cells, int on)
{
	uint32_t idx = cells[0];

	printf("%s: 0x%08x\n", __func__, idx);
}
