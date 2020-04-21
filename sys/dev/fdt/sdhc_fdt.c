/*	$OpenBSD: sdhc_fdt.c,v 1.7 2020/04/21 07:58:57 kettenis Exp $	*/
/*
 * Copyright (c) 2017 Mark Kettenis
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
#include <sys/malloc.h>
#include <sys/systm.h>

#include <machine/bus.h>
#include <machine/fdt.h>
#include <machine/intr.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_clock.h>
#include <dev/ofw/ofw_misc.h>
#include <dev/ofw/ofw_pinctrl.h>
#include <dev/ofw/fdt.h>

#include <dev/sdmmc/sdhcreg.h>
#include <dev/sdmmc/sdhcvar.h>
#include <dev/sdmmc/sdmmcvar.h>

/* RK3399 */
#define GRF_EMMCCORE_CON0_BASECLOCK		0xf000
#define  GRF_EMMCCORE_CON0_BASECLOCK_CLR		(0xff << 24)
#define  GRF_EMMCCORE_CON0_BASECLOCK_VAL(x)		(((x) & 0xff) << 8)
#define GRF_EMMCCORE_CON11			0xf02c
#define  GRF_EMMCCORE_CON11_CLOCKMULT_CLR		(0xff << 16)
#define  GRF_EMMCCORE_CON11_CLOCKMULT_VAL(x)		(((x) & 0xff) << 0)

struct sdhc_fdt_softc {
	struct sdhc_softc 	sc;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	bus_size_t		sc_size;
	void			*sc_ih;

	struct sdhc_host 	*sc_host;
	struct clock_device	sc_cd;
};

int	sdhc_fdt_match(struct device *, void *, void *);
void	sdhc_fdt_attach(struct device *, struct device *, void *);

struct cfattach sdhc_fdt_ca = {
	sizeof(struct sdhc_fdt_softc), sdhc_fdt_match, sdhc_fdt_attach
};

int	sdhc_fdt_signal_voltage(struct sdhc_softc *, int);
uint32_t sdhc_fdt_get_frequency(void *, uint32_t *);

int
sdhc_fdt_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return (OF_is_compatible(faa->fa_node, "arasan,sdhci-5.1") ||
	    OF_is_compatible(faa->fa_node, "brcm,bcm2711-emmc2") ||
	    OF_is_compatible(faa->fa_node, "brcm,bcm2835-sdhci"));
}

void
sdhc_fdt_attach(struct device *parent, struct device *self, void *aux)
{
	struct sdhc_fdt_softc *sc = (struct sdhc_fdt_softc *)self;
	struct fdt_attach_args *faa = aux;
	struct regmap *rm = NULL;
	uint32_t phandle, freq, cap = 0;

	if (faa->fa_nreg < 1) {
		printf(": no registers\n");
		return;
	}

	sc->sc_iot = faa->fa_iot;
	sc->sc_size = faa->fa_reg[0].size;

	if (bus_space_map(sc->sc_iot, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sc->sc_ioh)) {
		printf(": can't map registers\n");
		return;
	}

	pinctrl_byname(faa->fa_node, "default");

	clock_set_assigned(faa->fa_node);
	clock_enable_all(faa->fa_node);
	reset_deassert_all(faa->fa_node);

	sc->sc_ih = fdt_intr_establish(faa->fa_node, IPL_BIO,
	    sdhc_intr, sc, sc->sc.sc_dev.dv_xname);
	if (sc->sc_ih == NULL) {
		printf(": can't establish interrupt\n");
		goto unmap;
	}

	printf("\n");

	sc->sc.sc_host = &sc->sc_host;
	sc->sc.sc_dmat = faa->fa_dmat;

	/*
	 * Arasan controller always uses 1.8V and doesn't like an
	 * explicit switch.
	 */
	if (OF_is_compatible(faa->fa_node, "arasan,sdhci-5.1"))
		sc->sc.sc_signal_voltage = sdhc_fdt_signal_voltage;

	/*
	 * Rockchip RK3399 PHY doesn't like being powered down at low
	 * clock speeds and needs to be powered up explicitly.
	 */
	if (OF_is_compatible(faa->fa_node, "rockchip,rk3399-sdhci-5.1")) {
		/*
		 * The eMMC core's clock multiplier is of no use, so we just
		 * clear it.  Also make sure to set the base clock frequency.
		 */
		freq = clock_get_frequency(faa->fa_node, "clk_xin");
		freq /= 1000 * 1000; /* in MHz */
		phandle = OF_getpropint(faa->fa_node,
		    "arasan,soc-ctl-syscon", 0);
		if (phandle)
			rm = regmap_byphandle(phandle);
		if (rm) {
			regmap_write_4(rm, GRF_EMMCCORE_CON11,
			    GRF_EMMCCORE_CON11_CLOCKMULT_CLR |
			    GRF_EMMCCORE_CON11_CLOCKMULT_VAL(0));
			regmap_write_4(rm, GRF_EMMCCORE_CON0_BASECLOCK,
			    GRF_EMMCCORE_CON0_BASECLOCK_CLR |
			    GRF_EMMCCORE_CON0_BASECLOCK_VAL(freq));
		}
		/* Provide base clock frequency for the PHY driver. */
		sc->sc_cd.cd_node = faa->fa_node;
		sc->sc_cd.cd_cookie = sc;
		sc->sc_cd.cd_get_frequency = sdhc_fdt_get_frequency;
		clock_register(&sc->sc_cd);
		/*
		 * Enable the PHY.  The PHY should be powered on/off in
		 * the bus_clock function, but it's good enough to just
		 * enable it here right away and to keep it powered on.
		 */
		phy_enable(faa->fa_node, "phy_arasan");
		sc->sc.sc_flags |= SDHC_F_NOPWR0;

		/* XXX Doesn't work on Rockchip RK3399. */
		sc->sc.sc_flags |= SDHC_F_NODDR50;
	}

	if (OF_is_compatible(faa->fa_node, "brcm,bcm2711-emmc2"))
		sc->sc.sc_flags |= SDHC_F_NOPWR0;

	if (OF_is_compatible(faa->fa_node, "brcm,bcm2835-sdhci")) {
		cap = SDHC_VOLTAGE_SUPP_3_3V | SDHC_HIGH_SPEED_SUPP;
		cap |= SDHC_MAX_BLK_LEN_1024 << SDHC_MAX_BLK_LEN_SHIFT;

		freq = clock_get_frequency(faa->fa_node, NULL);
		sc->sc.sc_clkbase = freq / 1000;

		sc->sc.sc_flags |= SDHC_F_32BIT_ACCESS;
	}

	sdhc_host_found(&sc->sc, sc->sc_iot, sc->sc_ioh, sc->sc_size, 1, cap);
	return;

unmap:
	bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_size);
}

int
sdhc_fdt_signal_voltage(struct sdhc_softc *sc, int signal_voltage)
{
	switch (signal_voltage) {
	case SDMMC_SIGNAL_VOLTAGE_180:
		return 0;
	default:
		return EINVAL;
	}
}

uint32_t
sdhc_fdt_get_frequency(void *cookie, uint32_t *cells)
{
	struct sdhc_fdt_softc *sc = cookie;
	return clock_get_frequency(sc->sc_cd.cd_node, "clk_xin");
}
